/*
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include "../../../stream_input/amports/amports_priv.h"
#include "vdec.h"
#include "vdec_input.h"
#include <asm/cacheflush.h>
#include <linux/crc32.h>

#define VFRAME_BLOCK_SIZE (512 * SZ_1K)/*512 for 1080p default init.*/
#define VFRAME_BLOCK_SIZE_4K (2 * SZ_1M) /*2M for 4K default.*/
#define VFRAME_BLOCK_SIZE_MAX (4 * SZ_1M)

#define VFRAME_BLOCK_PAGEALIGN 4
#define VFRAME_BLOCK_MIN_LEVEL (2 * SZ_1M)
#define VFRAME_BLOCK_MAX_LEVEL (8 * SZ_1M)
#define VFRAME_BLOCK_MAX_TOTAL_SIZE (16 * SZ_1M)

/*
2s for OMX
*/
#define MAX_FRAME_DURATION_S 2


#define VFRAME_BLOCK_HOLE (SZ_64K)

#define MIN_FRAME_PADDING_SIZE ((u32)(L1_CACHE_BYTES))

#define EXTRA_PADDING_SIZE  (16 * SZ_1K) /*HEVC_PADDING_SIZE*/

#define MEM_NAME "VFRAME_INPUT"

//static int vdec_input_get_duration_u64(struct vdec_input_s *input);
static struct vframe_block_list_s *
	vdec_input_alloc_new_block(struct vdec_input_s *input,
	ulong phy_addr,
	int size,
	chunk_free free,
	void* priv);

static int aml_copy_from_user(void *to, const void *from, ulong n)
{
	int ret =0;

	if (likely(access_ok(from, n)))
		ret = copy_from_user(to, from, n);
	else
		memcpy(to, from, n);

	return ret;
}

static int copy_from_user_to_phyaddr(void *virts, const char __user *buf,
		u32 size, ulong phys, u32 pading, bool is_mapped)
{
	u32 i, span = SZ_1M;
	u32 count = size / PAGE_ALIGN(span);
	u32 remain = size % PAGE_ALIGN(span);
	ulong addr = phys;
	u8 *p = virts;

	if (is_mapped) {
		if (aml_copy_from_user(p, buf, size))
			return -EFAULT;

		if (pading)
			memset(p + size, 0, pading);

		codec_mm_dma_flush(p, size + pading, DMA_TO_DEVICE);

		return 0;
	}

	for (i = 0; i < count; i++) {
		addr = phys + i * span;
		p = codec_mm_vmap(addr, span);
		if (!p)
			return -1;

		if (aml_copy_from_user(p, buf + i * span, span)) {
			codec_mm_unmap_phyaddr(p);
			return -EFAULT;
		}

		codec_mm_dma_flush(p, span, DMA_TO_DEVICE);
		codec_mm_unmap_phyaddr(p);
	}

	if (!remain)
		return 0;

	span = size - remain;
	addr = phys + span;
	p = codec_mm_vmap(addr, remain + pading);
	if (!p)
		return -1;

	if (aml_copy_from_user(p, buf + span, remain)) {
		codec_mm_unmap_phyaddr(p);
		return -EFAULT;
	}

	if (pading)
		memset(p + remain, 0, pading);

	codec_mm_dma_flush(p, remain + pading, DMA_TO_DEVICE);
	codec_mm_unmap_phyaddr(p);

	return 0;
}

static int vframe_chunk_fill(struct vdec_input_s *input,
			struct vframe_chunk_s *chunk, const char *buf,
			size_t count, struct vframe_block_list_s *block)
{
	u8 *p = (u8 *)block->start_virt + block->wp;
	if (block->type == VDEC_TYPE_FRAME_BLOCK) {
		copy_from_user_to_phyaddr(p, buf, count,
			block->start + block->wp,
			chunk->pading_size,
			block->is_mapped);
	} else if (block->type == VDEC_TYPE_FRAME_CIRCULAR) {
		size_t len = min((size_t)(block->size - block->wp), count);
		u32 wp;

		copy_from_user_to_phyaddr(p, buf, len,
				block->start + block->wp, 0,
				block->is_mapped);
		p += len;

		if (count > len) {
			copy_from_user_to_phyaddr(p, buf + len,
				count - len,
				block->start, 0,
				block->is_mapped);

			p += count - len;
		}

		wp = block->wp + count;
		if (wp >= block->size)
			wp -= block->size;

		len = min(block->size - wp, chunk->pading_size);

		if (!block->is_mapped) {
			p = codec_mm_vmap(block->start + wp, len);
			memset(p, 0, len);
			codec_mm_dma_flush(p, len, DMA_TO_DEVICE);
			codec_mm_unmap_phyaddr(p);
		} else {
			memset(p, 0, len);
			codec_mm_dma_flush(p, len, DMA_TO_DEVICE);
		}

		if (chunk->pading_size > len) {
			p = (u8 *)block->start_virt;

			if (!block->is_mapped) {
				p = codec_mm_vmap(block->start,
					chunk->pading_size - len);
				memset(p, 0, chunk->pading_size - len);
				codec_mm_dma_flush(p,
					chunk->pading_size - len,
					DMA_TO_DEVICE);
				codec_mm_unmap_phyaddr(p);
			} else {
				memset(p, 0, chunk->pading_size - len);
				codec_mm_dma_flush(p,
					chunk->pading_size - len,
					DMA_TO_DEVICE);
			}
		}
	}

	return 0;
}

static inline u32 vframe_block_space(struct vframe_block_list_s *block)
{
	if (block->type == VDEC_TYPE_FRAME_BLOCK) {
		return block->size - block->wp;
	} else {
		return (block->rp >= block->wp) ?
			   (block->rp - block->wp) :
			   (block->rp - block->wp + block->size);
	}
}

static void vframe_block_add_chunk(struct vframe_block_list_s *block,
				struct vframe_chunk_s *chunk)
{
	block->wp += chunk->size + chunk->pading_size;
	if (block->wp >= block->size)
		block->wp -= block->size;
	block->data_size += chunk->size;
	block->chunk_count++;
	chunk->block = block;
	block->input->wr_block = block;
	chunk->sequence = block->input->sequence;
	block->input->sequence++;
}

static bool is_coherent_buff = 1;

static void vframe_block_free_block(struct vframe_block_list_s *block)
{
	if (is_coherent_buff) {
		if (block->mem_handle) {
			codec_mm_dma_free_coherent(block->mem_handle);
		}
	} else {
		if (block->addr) {
			codec_mm_free_for_dma(MEM_NAME,	block->addr);
		}
	}
	/*
	*pr_err("free block %d, size=%d\n", block->id, block->size);
	*/
	kfree(block);
}

static int vframe_block_init_alloc_storage(struct vdec_input_s *input,
			struct vframe_block_list_s *block,
			ulong phy_addr,
			int size,
			chunk_free free,
			void *priv)
{
	int alloc_size = input->default_block_size;
	block->magic = 0x4b434c42;
	block->input = input;
	block->type = input->type;

	/*
	 * todo: for different type use different size
	 */
	if (phy_addr) {
		block->is_out_buf = 1;
		block->start_virt = NULL;
		block->start = phy_addr;
		block->size = size;
		block->free = free;
		block->priv = priv;
	} else {
		alloc_size = PAGE_ALIGN(alloc_size);
		if (is_coherent_buff) {
			block->start_virt = codec_mm_dma_alloc_coherent(&block->mem_handle, &block->addr, alloc_size, MEM_NAME);
		} else {
			block->addr = codec_mm_alloc_for_dma_ex(
				MEM_NAME,
				alloc_size/PAGE_SIZE,
				VFRAME_BLOCK_PAGEALIGN,
				CODEC_MM_FLAGS_DMA_CPU | CODEC_MM_FLAGS_FOR_VDECODER,
				input->id,
				block->id);
		}

		if (!block->addr) {
			pr_err("Input block allocation failed\n");
			return -ENOMEM;
		}

		if (!is_coherent_buff)
			block->start_virt = (void *)codec_mm_phys_to_virt(block->addr);
		if (block->start_virt)
			block->is_mapped = true;
		block->start = block->addr;
		block->size = alloc_size;
		block->is_out_buf = 0;
		block->free = NULL;
	}

	return 0;
}

void vdec_input_init(struct vdec_input_s *input, struct vdec_s *vdec)
{
	INIT_LIST_HEAD(&input->vframe_block_list);
	INIT_LIST_HEAD(&input->vframe_block_free_list);
	INIT_LIST_HEAD(&input->vframe_chunk_list);
	spin_lock_init(&input->lock);
	input->id = vdec->id;
	input->block_nums = 0;
	input->vdec = vdec;
	input->block_id_seq = 0;
	input->size = 0;
	input->default_block_size = VFRAME_BLOCK_SIZE;
	snprintf(input->vdec_input_name, sizeof(input->vdec_input_name),
		 "vdec-input-%d", vdec->id);
}
int vdec_input_prepare_bufs(struct vdec_input_s *input,
	int frame_width, int frame_height)
{
	struct vframe_block_list_s *block;
	int i;
	unsigned long flags;

	if (vdec_secure(input->vdec))
		return 0;
	if (input->size > 0)
		return 0;
	if (frame_width * frame_height >= 1920 * 1088) {
		/*have add data before. ignore prepare buffers.*/
		input->default_block_size = VFRAME_BLOCK_SIZE_4K;
	}
	/*prepared 3 buffers for smooth start.*/
	for (i = 0; i < 3; i++) {
		block = vdec_input_alloc_new_block(input, 0, 0, NULL, NULL);
		if (!block)
			break;
		flags = vdec_input_lock(input);
		list_move_tail(&block->list,
				&input->vframe_block_free_list);
		input->wr_block = NULL;
		vdec_input_unlock(input, flags);
	}
	return 0;
}

static int vdec_input_dump_block_locked(
	struct vframe_block_list_s *block,
	char *buf, int size)
{
	char *pbuf = buf;
	char sbuf[512];
	int tsize = 0;
	int s;

	if (!pbuf) {
		pbuf = sbuf;
		size = 512;
	}
	#define BUFPRINT(args...) \
	do {\
		s = snprintf(pbuf, size - tsize, args);\
		tsize += s;\
		pbuf += s; \
	} while (0)

	BUFPRINT("\tblock:[%d:%p]-addr=%p,vstart=%p,type=%d\n",
		block->id,
		block,
		(void *)block->addr,
		(void *)block->start_virt,
		block->type);
	BUFPRINT("\t-blocksize=%d,data=%d,wp=%d,rp=%d,chunk_count=%d\n",
		block->size,
		block->data_size,
		block->wp,
		block->rp,
		block->chunk_count);
	/*
	BUFPRINT("\tlist=%p,next=%p,prev=%p\n",
		&block->list,
		block->list.next,
		block->list.prev);
	*/
	#undef BUFPRINT
	if (!buf)
		pr_info("%s", sbuf);
	return tsize;
}

int vdec_input_dump_blocks(struct vdec_input_s *input,
	char *bufs, int size)
{
	struct list_head *p, *tmp;
	unsigned long flags;
	char *lbuf = bufs;
	char sbuf[256];
	int s = 0;

	if (size <= 0)
		return 0;
	if (!bufs)
		lbuf = sbuf;
	s += snprintf(lbuf + s, size - s,
		"blocks:vdec-%d id:%d,bufsize=%d,dsize=%d,frames:%d,dur:%dms\n",
		input->id,
		input->block_nums,
		input->size,
		input->data_size,
		input->have_frame_num,
		vdec_input_get_duration_u64(input)/1000);
	if (bufs)
		lbuf += s;
	else {
		pr_info("%s", sbuf);
		lbuf = NULL;
	}

	flags = vdec_input_lock(input);
	/* dump input blocks */
	list_for_each_safe(p, tmp, &input->vframe_block_list) {
		struct vframe_block_list_s *block = list_entry(
			p, struct vframe_block_list_s, list);
		if (bufs != NULL) {
			lbuf = bufs + s;
			if (size - s < 128)
				break;
		}
		s += vdec_input_dump_block_locked(block, lbuf, size - s);
	}
	list_for_each_safe(p, tmp, &input->vframe_block_free_list) {
		struct vframe_block_list_s *block = list_entry(
			p, struct vframe_block_list_s, list);
		if (bufs != NULL) {
			lbuf = bufs + s;
			if (size - s < 128)
				break;
		}
		s += vdec_input_dump_block_locked(block, lbuf, size - s);
	}
	vdec_input_unlock(input, flags);

	return s;
}

static int vdec_input_dump_chunk_locked(
	int id,
	struct vframe_chunk_s *chunk,
	char *buf, int size)
{
	char *pbuf = buf;
	char sbuf[512];
	int tsize = 0;
	int s;

	if (!pbuf) {
		pbuf = sbuf;
		size = 512;
	}
	#define BUFPRINT(args...) \
	do {\
		s = snprintf(pbuf, size - tsize, args);\
		tsize += s;\
		pbuf += s; \
	} while (0)

	BUFPRINT(
		"\t[%d][%lld:%p]-off=%d,size:%d,p:%d,\tpts64=%lld,addr=%p\n",
		id,
		chunk->sequence,
		chunk->block,
		chunk->offset,
		chunk->size,
		chunk->pading_size,
		chunk->pts64,
		(void *)(chunk->block->addr + chunk->offset));
	/*
	BUFPRINT("\tlist=%p,next=%p,prev=%p\n",
		&chunk->list,
		chunk->list.next,
		chunk->list.prev);
	*/
	#undef BUFPRINT
	if (!buf)
		pr_info("%s", sbuf);
	return tsize;
}

int vdec_input_dump_chunks(int id, struct vdec_input_s *input,
	char *bufs, int size)
{

	struct list_head *p, *tmp;
	unsigned long flags;
	char *lbuf = bufs;
	char sbuf[256];
	int s = 0;
	int i = 0;

	if (size <= 0)
		return 0;

	if (!bufs)
		lbuf = sbuf;
	s = snprintf(lbuf + s, size - s,
		"[%d]blocks:vdec-%d id:%d,bufsize=%d,dsize=%d,frames:%d,maxframe:%d\n",
		id,
		input->id,
		input->block_nums,
		input->size,
		input->data_size,
		input->have_frame_num,
		input->frame_max_size);
	if (bufs)
		lbuf += s;
	if (!bufs) {
		pr_info("%s", sbuf);
		lbuf = NULL;
	}
	flags = vdec_input_lock(input);
	/*dump chunks list infos.*/
	list_for_each_safe(p, tmp, &input->vframe_chunk_list) {
		struct vframe_chunk_s *chunk = list_entry(
				p, struct vframe_chunk_s, list);
		if (bufs != NULL)
			lbuf = bufs + s;
		s += vdec_input_dump_chunk_locked(id, chunk, lbuf, size - s);
		i++;
		if (i >= 10)
			break;
	}
	vdec_input_unlock(input, flags);

	return s;
}



int vdec_input_set_buffer(struct vdec_input_s *input, u32 start, u32 size)
{
	if (input_frame_based(input))
		return -EINVAL;

	input->start = start;
	input->size = size;
	input->swap_rp = start;

	if (vdec_secure(input->vdec))
		input->swap_page_phys = codec_mm_alloc_for_dma("SWAP",
			1, 0, CODEC_MM_FLAGS_TVP);
	else {
		input->swap_page = codec_mm_dma_alloc_coherent(&input->mem_handle,
				(ulong *)&input->swap_page_phys,
				PAGE_SIZE, MEM_NAME);
		if (input->swap_page == NULL)
			return -ENOMEM;
	}

	if (input->swap_page_phys == 0)
		return -ENOMEM;

	return 0;
}
EXPORT_SYMBOL(vdec_input_set_buffer);

void vdec_input_set_type(struct vdec_input_s *input, int type, int target)
{
	input->type = type;
	input->target = target;
	if (type == VDEC_TYPE_FRAME_CIRCULAR) {
		/*alway used max block.*/
		input->default_block_size = VFRAME_BLOCK_SIZE_MAX;
	}
}
EXPORT_SYMBOL(vdec_input_set_type);

int vdec_input_get_status(struct vdec_input_s *input,
			struct vdec_input_status_s *status)
{
	unsigned long flags;

	if (input->vdec == NULL)
		return -EINVAL;

	flags = vdec_input_lock(input);

	if (list_empty(&input->vframe_block_list)) {
		status->size = VFRAME_BLOCK_SIZE;
		status->data_len = 0;
		status->free_len = VFRAME_BLOCK_SIZE;
		status->read_pointer = 0;
	} else {
		int r = VFRAME_BLOCK_MAX_LEVEL - vdec_input_level(input)
			- VFRAME_BLOCK_HOLE;
		status->size = input->size;
		status->data_len = vdec_input_level(input);
		status->free_len = (r > 0) ? r : 0;
		status->read_pointer = input->total_rd_count;
	}

	vdec_input_unlock(input, flags);

	return 0;
}
EXPORT_SYMBOL(vdec_input_get_status);

static void vdec_input_add_block(struct vdec_input_s *input,
				struct vframe_block_list_s *block)
{
	unsigned long flags;

	flags = vdec_input_lock(input);
	block->wp = 0;
	block->id = input->block_id_seq++;
	list_add_tail(&block->list, &input->vframe_block_list);
	input->size += block->size;
	input->block_nums++;
	input->wr_block = block;
	vdec_input_unlock(input, flags);
}

static inline void vdec_input_del_block_locked(struct vdec_input_s *input,
				struct vframe_block_list_s *block)
{
	list_del(&block->list);
	input->size -= block->size;
	input->block_nums--;
}

int vdec_input_level(struct vdec_input_s *input)
{
	return input->total_wr_count - input->total_rd_count;
}
EXPORT_SYMBOL(vdec_input_level);

static struct vframe_block_list_s *
	vdec_input_alloc_new_block(struct vdec_input_s *input,
	ulong phy_addr,
	int size,
	chunk_free free,
	void* priv)
{
	struct vframe_block_list_s *block;
	block = kzalloc(sizeof(struct vframe_block_list_s),
			GFP_KERNEL);
	if (block == NULL) {
		input->no_mem_err_cnt++;
		pr_err("vframe_block structure allocation failed\n");
		return NULL;
	}

	if (vframe_block_init_alloc_storage(input,
		block, phy_addr, size, free, priv) != 0) {
		kfree(block);
		pr_err("vframe_block storage allocation failed\n");
		return NULL;
	}

	INIT_LIST_HEAD(&block->list);

	vdec_input_add_block(input, block);

	/*
	 *pr_info("vdec-%d:new block id=%d, total_blocks:%d, size=%d\n",
	 *	input->id,
	 *	block->id,
	 *	input->block_nums,
	 *	block->size);
	 */
	if (0 && input->size > VFRAME_BLOCK_MAX_LEVEL * 2) {
		/*
		used
		*/
		pr_info(
		"input[%d] reach max: size:%d, blocks:%d",
			input->id,
			input->size,
			input->block_nums);
		pr_info("level:%d, wr:%lld,rd:%lld\n",
			vdec_input_level(input),
			input->total_wr_count,
			input->total_rd_count);
		vdec_input_dump_blocks(input, NULL, 0);
	}
	return block;
}
int vdec_input_get_duration_u64(struct vdec_input_s *input)
{
	int duration = (input->last_inpts_u64 - input->last_comsumed_pts_u64);
	if (input->last_in_nopts_cnt > 0 &&
		input->last_comsumed_pts_u64 > 0 &&
		input->last_duration > 0) {
		duration += (input->last_in_nopts_cnt -
			input->last_comsumed_no_pts_cnt) *
			input->last_duration;
	}
	if (duration > 1000 * 1000000)/*> 1000S,I think jumped.*/
		duration = 0;
	if (duration <= 0 && input->last_duration > 0) {
		/*..*/
		duration = input->last_duration * input->have_frame_num;
	}
	if (duration < 0)
		duration = 0;
	return duration;
}
EXPORT_SYMBOL(vdec_input_get_duration_u64);

/*
	ret >= 13: have enough buffer, blocked add more buffers
*/
static int vdec_input_have_blocks_enough(struct vdec_input_s *input)
{
	int ret = 0;
	if (vdec_input_level(input) > VFRAME_BLOCK_MIN_LEVEL)
		ret += 1;
	if (vdec_input_level(input) >= VFRAME_BLOCK_MAX_LEVEL)
		ret += 2;
	if (vdec_input_get_duration_u64(input) > MAX_FRAME_DURATION_S)
		ret += 4;
	if (input->have_frame_num > 30)
		ret += 8;
	else
		ret -= 8;/*not enough frames.*/
	if (input->size >= VFRAME_BLOCK_MAX_TOTAL_SIZE)
		ret += 100;/*always bloced add more buffers.*/

	return ret;
}
static int	vdec_input_get_free_block(
	struct vdec_input_s *input,
	int size,/*frame size + pading*/
	struct vframe_block_list_s **block_ret)
{
	struct vframe_block_list_s *to_freeblock = NULL;
	struct vframe_block_list_s *block = NULL;
	unsigned long flags;
	flags = vdec_input_lock(input);
	/*get from free list.*/
	if (!list_empty(&input->vframe_block_free_list)) {
		block = list_entry(input->vframe_block_free_list.next,
		struct vframe_block_list_s, list);
		if (block->size < (size)) {
			vdec_input_del_block_locked(input, block);
			to_freeblock = block;
			block = NULL;
		} else {
			list_move_tail(&block->list,
				&input->vframe_block_list);
			input->wr_block = block;/*swith to new block*/
		}
	}
	vdec_input_unlock(input, flags);
	if (to_freeblock) {
		/*free the small block.*/
		vframe_block_free_block(to_freeblock);
	}
	if (block) {
		*block_ret = block;
		return 0;
	}

	if (vdec_input_have_blocks_enough(input) > 13) {
		/*buf fulled */
		return -EAGAIN;
	}
	if (input->no_mem_err_cnt > 3) {
		/*alloced failed more times.
		*/
		return -EAGAIN;
	}
	if (input->default_block_size <=
		size * 2) {
		int def_size = input->default_block_size;
		do {
			def_size *= 2;
		} while ((def_size <= 2 * size) &&
			(def_size <= VFRAME_BLOCK_SIZE_MAX));
		if (def_size < size)
			def_size = ALIGN(size + 64, (1 << 17));
		/*128k aligned,same as codec_mm*/
		input->default_block_size = def_size;
	}
	block = vdec_input_alloc_new_block(input, 0, 0, NULL, NULL);
	if (!block) {
		input->no_mem_err_cnt++;
		return -EAGAIN;
	}
	input->no_mem_err_cnt = 0;
	*block_ret = block;
	return 0;
}

int vdec_input_add_chunk(struct vdec_input_s *input, const char *buf,
		size_t count, u32 handle, chunk_free free, void* priv)
{
	unsigned long flags;
	struct vframe_chunk_s *chunk;
	struct vdec_s *vdec = input->vdec;
	struct vframe_block_list_s *block;

	int need_pading_size = MIN_FRAME_PADDING_SIZE;

	if (vdec_secure(vdec)) {
		block = vdec_input_alloc_new_block(input, (ulong)buf,
			PAGE_ALIGN(count + HEVC_PADDING_SIZE + 1),
			free, priv); /*Add padding large than HEVC_PADDING_SIZE */
		if (!block)
			return -ENOMEM;
		block->handle = handle;
	} else {
#if 0
		if (add_count == 0) {
			add_count++;
			memcpy(sps, buf, 30);
			return 30;
		} else if (add_count == 1) {
			add_count++;
			memcpy(pps, buf, 8);
			return 8;
		}
		add_count++;
#endif

#if 0
		pr_info("vdec_input_add_frame add %p, count=%d\n", buf, (int)count);

		if (count >= 8) {
			pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
			buf[0], buf[1], buf[2], buf[3],
			buf[4], buf[5], buf[6], buf[7]);
		}
		if (count >= 16) {
			pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
			buf[8], buf[9], buf[10], buf[11],
			buf[12], buf[13], buf[14], buf[15]);
		}
		if (count >= 24) {
			pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
			buf[16], buf[17], buf[18], buf[19],
			buf[20], buf[21], buf[22], buf[23]);
		}
		if (count >= 32) {
			pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
			buf[24], buf[25], buf[26], buf[27],
			buf[28], buf[29], buf[30], buf[31]);
	}
#endif
		if (input_stream_based(input))
			return -EINVAL;

		if (count < PAGE_SIZE) {
			need_pading_size = PAGE_ALIGN(count + need_pading_size) -
				count;
		} else {
			/*to 64 bytes aligned;*/
			if (count & 0x3f)
				need_pading_size += 64 - (count & 0x3f);
		}
		block = input->wr_block;
		if (block &&
			(vframe_block_space(block) > (count + need_pading_size))) {
			/*this block have enough buffers.
			do nothings.
			*/
		} else if (block && (block->type == VDEC_TYPE_FRAME_CIRCULAR)) {
			/*in circular module.
			only one block,.*/
			return -EAGAIN;
		} else if (block != NULL) {
			/*have block but not enough space.
			recycle the no enough blocks.*/
			flags = vdec_input_lock(input);
			if (input->wr_block == block &&
				block->chunk_count == 0) {
				block->rp = 0;
				block->wp = 0;
				/*block no data move to freelist*/
				list_move_tail(&block->list,
					&input->vframe_block_free_list);
				input->wr_block = NULL;
			}
			vdec_input_unlock(input, flags);
			block = NULL;
		}
		if (!block) {/*try new block.*/
			int ret = vdec_input_get_free_block(input,
				count + need_pading_size + EXTRA_PADDING_SIZE,
				&block);
			if (ret < 0)/*no enough block now.*/
				return ret;
		}
	}

	chunk = kzalloc(sizeof(struct vframe_chunk_s), GFP_KERNEL);

	if (!chunk) {
		pr_err("vframe_chunk structure allocation failed\n");
		return -ENOMEM;
	}

	if ((vdec->hdr10p_data_valid == true) &&
		(vdec->hdr10p_data_size != 0)) {
		char *new_buf;
		new_buf = vzalloc(vdec->hdr10p_data_size);
		if (new_buf) {
			memcpy(new_buf, vdec->hdr10p_data_buf, vdec->hdr10p_data_size);
			chunk->hdr10p_data_buf = new_buf;
			chunk->hdr10p_data_size = vdec->hdr10p_data_size;
		} else {
			pr_err("%s:hdr10p data vzalloc size(%d) failed\n",
				__func__, vdec->hdr10p_data_size);
			chunk->hdr10p_data_buf = NULL;
			chunk->hdr10p_data_size = 0;
		}
	} else {
		chunk->hdr10p_data_buf = NULL;
		chunk->hdr10p_data_size = 0;
	}
	vdec->hdr10p_data_valid = false;

	chunk->magic = 0x4b554843;
	if (vdec->pts_valid) {
		chunk->pts = vdec->pts;
		chunk->pts64 = vdec->pts64;
	}

	if (vdec->timestamp_valid)
		chunk->timestamp = vdec->timestamp;

	if (vdec->pts_valid &&
		input->last_inpts_u64 > 0 &&
		input->last_in_nopts_cnt == 0) {
		int d = (int)(chunk->pts64 - input->last_inpts_u64);
		if (d > 0 && (d < input->last_duration))
			input->last_duration = d;
		/*	alwasy: used the smallest duration;
			if 60fps->30 fps.
			maybe have warning value.
		*/
	}
	chunk->pts_valid = vdec->pts_valid;
	vdec->pts_valid = false;
	INIT_LIST_HEAD(&chunk->list);

	if (vdec_secure(vdec)) {
		chunk->offset = 0;
		chunk->size = count;
		chunk->pading_size = PAGE_ALIGN(chunk->size + need_pading_size) -
			chunk->size;
	} else {
		chunk->offset = block->wp;
		chunk->size = count;
		chunk->pading_size = need_pading_size;
		if (vframe_chunk_fill(input, chunk, buf, count, block)) {
			pr_err("vframe_chunk_fill failed\n");
			kfree(chunk);
			return -EFAULT;
		}

	}


	flags = vdec_input_lock(input);

	vframe_block_add_chunk(block, chunk);

	list_add_tail(&chunk->list, &input->vframe_chunk_list);
	input->data_size += chunk->size;
	input->have_frame_num++;

	if (input->have_frame_num == 1)
		input->vdec_up(vdec);
	ATRACE_COUNTER(input->vdec_input_name, input->have_frame_num);
	if (chunk->pts_valid) {
		input->last_inpts_u64 = chunk->pts64;
		input->last_in_nopts_cnt = 0;
	} else {
		/*nopts*/
		input->last_in_nopts_cnt++;
	}
	if (chunk->size > input->frame_max_size)
		input->frame_max_size = chunk->size;
	input->total_wr_count += count;
	vdec_input_unlock(input, flags);
#if 0
	if (add_count == 2)
		input->total_wr_count += 38;
#endif

	return count;
}

int vdec_input_add_frame(struct vdec_input_s *input, const char *buf,
			size_t count)
{
	int ret = 0;
	struct drm_info drm;
	struct vdec_s *vdec = input->vdec;
	unsigned long phy_buf;

	if (vdec_secure(vdec)) {
		while (count > 0) {
			if (count < sizeof(struct drm_info))
				return -EIO;
			if (copy_from_user((void*)&drm, buf + ret, sizeof(struct drm_info)))
				return -EAGAIN;
			if (!(drm.drm_flag & TYPE_DRMINFO_V2))
				return -EIO; /*must drm info v2 version*/
			phy_buf = (unsigned long) drm.drm_phy;
			vdec_input_add_chunk(input, (char *)phy_buf,
				(size_t)drm.drm_pktsize, drm.handle, NULL, NULL);
			count -= sizeof(struct drm_info);
			ret += sizeof(struct drm_info);

			/* the drm frame data might include head infos and raw */
			/* data thus the next drm unit still need a valid pts.*/
			if (count >= sizeof(struct drm_info))
				vdec->pts_valid = true;
		}
	} else {
		ret = vdec_input_add_chunk(input, buf, count, 0, NULL, NULL);
	}

	return ret;
}
EXPORT_SYMBOL(vdec_input_add_frame);

int vdec_input_add_frame_with_dma(struct vdec_input_s *input, ulong addr,
	size_t count, u32 handle, chunk_free free, void* priv)
{
	struct vdec_s *vdec = input->vdec;

	return vdec_secure(vdec) ?
		vdec_input_add_chunk(input,
			(char *)addr, count, handle, free, priv) : -1;
}
EXPORT_SYMBOL(vdec_input_add_frame_with_dma);

struct vframe_chunk_s *vdec_input_next_chunk(struct vdec_input_s *input)
{
	struct vframe_chunk_s *chunk = NULL;
	unsigned long flags;
	flags = vdec_input_lock(input);
	if (!list_empty(&input->vframe_chunk_list)) {
		chunk = list_first_entry(&input->vframe_chunk_list,
				struct vframe_chunk_s, list);
	}
	vdec_input_unlock(input, flags);
	return chunk;
}
EXPORT_SYMBOL(vdec_input_next_chunk);

struct vframe_chunk_s *vdec_input_next_input_chunk(
			struct vdec_input_s *input)
{
	struct vframe_chunk_s *chunk = NULL;
	struct list_head *p;
	unsigned long flags;
	flags = vdec_input_lock(input);

	list_for_each(p, &input->vframe_chunk_list) {
		struct vframe_chunk_s *c = list_entry(
			p, struct vframe_chunk_s, list);
		if ((c->flag & VFRAME_CHUNK_FLAG_CONSUMED) == 0) {
			chunk = c;
			break;
		}
	}
	vdec_input_unlock(input, flags);
	return chunk;
}
EXPORT_SYMBOL(vdec_input_next_input_chunk);

void vdec_input_release_chunk(struct vdec_input_s *input,
			struct vframe_chunk_s *chunk)
{
	struct vframe_chunk_s *p;
	u32 chunk_valid = 0;
	unsigned long flags;
	struct vframe_block_list_s *block = chunk->block;
	struct vframe_block_list_s *tofreeblock = NULL;
	flags = vdec_input_lock(input);

	list_for_each_entry(p, &input->vframe_chunk_list, list) {
		if (p == chunk) {
			chunk_valid = 1;
			break;
		}
	}
	/* 2 threads go here, the other done the deletion,so return*/
	if (chunk_valid == 0) {
		vdec_input_unlock(input, flags);
		pr_err("%s chunk is deleted,so return.\n", __func__);
		return;
	}

	list_del(&chunk->list);
	input->have_frame_num--;
	ATRACE_COUNTER(input->vdec_input_name, input->have_frame_num);
	if (chunk->pts_valid) {
		input->last_comsumed_no_pts_cnt = 0;
		input->last_comsumed_pts_u64 = chunk->pts64;
	} else
		input->last_comsumed_no_pts_cnt++;
	block->rp += chunk->size;
	if (block->rp >= block->size)
		block->rp -= block->size;
	block->data_size -= chunk->size;
	block->chunk_count--;
	input->data_size -= chunk->size;
	input->total_rd_count += chunk->size;
	if (block->is_out_buf) {
		list_move_tail(&block->list,
			&input->vframe_block_free_list);
		if (block->free) {
			vdec_input_del_block_locked(input, block);
			block->free(block->priv, block->handle);
			kfree(block);
		}
	} else if (block->chunk_count == 0 &&
		input->wr_block != block ) {/*don't free used block*/
		if (block->size < input->default_block_size) {
			vdec_input_del_block_locked(input, block);
			tofreeblock = block;
		} else {
			block->rp = 0;
			block->wp = 0;
			list_move_tail(&block->list,
				&input->vframe_block_free_list);
		}
	}

	vdec_input_unlock(input, flags);
	if (tofreeblock)
		vframe_block_free_block(tofreeblock);
	kfree(chunk);
}
EXPORT_SYMBOL(vdec_input_release_chunk);

unsigned long vdec_input_lock(struct vdec_input_s *input)
{
	unsigned long flags;

	spin_lock_irqsave(&input->lock, flags);

	return flags;
}
EXPORT_SYMBOL(vdec_input_lock);

void vdec_input_unlock(struct vdec_input_s *input, unsigned long flags)
{
	spin_unlock_irqrestore(&input->lock, flags);
}
EXPORT_SYMBOL(vdec_input_unlock);

void vdec_input_release(struct vdec_input_s *input)
{
	struct list_head *p, *tmp;

	/* release chunk data */
	list_for_each_safe(p, tmp, &input->vframe_chunk_list) {
		struct vframe_chunk_s *chunk = list_entry(
				p, struct vframe_chunk_s, list);
		vdec_input_release_chunk(input, chunk);
	}
	list_for_each_safe(p, tmp, &input->vframe_block_list) {
		/*should never here.*/
		list_move_tail(p, &input->vframe_block_free_list);
	}
	/* release input blocks */
	list_for_each_safe(p, tmp, &input->vframe_block_free_list) {
		struct vframe_block_list_s *block = list_entry(
			p, struct vframe_block_list_s, list);
		vdec_input_del_block_locked(input, block);
		vframe_block_free_block(block);
	}

	/* release swap pages */
	if (vdec_secure(input->vdec)) {
		if (input->swap_page_phys)
			codec_mm_free_for_dma("SWAP", input->swap_page_phys);
	} else {
		if (input->swap_page)
			codec_mm_dma_free_coherent(input->mem_handle);
	}
	input->swap_page = NULL;
	input->swap_page_phys = 0;
	input->swap_valid = false;
}
EXPORT_SYMBOL(vdec_input_release);

u32 vdec_input_get_freed_handle(struct vdec_s *vdec)
{
	struct vframe_block_list_s *block;
	struct vdec_input_s *input = &vdec->input;
	unsigned long flags;
	u32 handle = 0;

	if (!vdec)
		return 0;

	if (!vdec_secure(vdec))
		return 0;

	flags = vdec_input_lock(input);
	do {
		block = list_first_entry_or_null(&input->vframe_block_free_list,
		struct vframe_block_list_s, list);
		if (!block) {
			break;
		}

		handle = block->handle;
		vdec_input_del_block_locked(input, block);
		if (block->free)
			block->free(block->priv, handle);
		kfree(block);

	} while(!handle);

	vdec_input_unlock(input, flags);
	return handle;
}
EXPORT_SYMBOL(vdec_input_get_freed_handle);
