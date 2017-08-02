#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/amlogic/codec_mm/codec_mm.h>
#include <linux/delay.h>

#include "amports_priv.h"
#include "vdec.h"
#include "vdec_input.h"

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
static int vdec_input_get_duration_u64(struct vdec_input_s *input);
static struct vframe_block_list_s *
	vdec_input_alloc_new_block(struct vdec_input_s *input);

static int vframe_chunk_fill(struct vdec_input_s *input,
			struct vframe_chunk_s *chunk, const char *buf,
			size_t count, struct vframe_block_list_s *block)
{
	u8 *p = (u8 *)block->start_virt + block->wp;
	int total_size = count + chunk->pading_size;
	if (block->type == VDEC_TYPE_FRAME_BLOCK) {
		if (copy_from_user(p, buf, count))
			return -EFAULT;

		p += count;

		memset(p, 0, chunk->pading_size);

		dma_sync_single_for_device(amports_get_dma_device(),
			block->start + block->wp,
			total_size, DMA_TO_DEVICE);

	} else if (block->type == VDEC_TYPE_FRAME_CIRCULAR) {
		size_t len = min((size_t)(block->size - block->wp), count);
		u32 wp;

		if (copy_from_user(p, buf, len))
			return -EFAULT;

		dma_sync_single_for_device(amports_get_dma_device(),
			block->start + block->wp,
			len, DMA_TO_DEVICE);

		p += len;

		if (count > len) {
			p = (u8 *)block->start_virt;
			if (copy_from_user(p, buf, count - len))
				return -EFAULT;

			dma_sync_single_for_device(amports_get_dma_device(),
				block->start,
				count-len, DMA_TO_DEVICE);

			p += count - len;
		}

		wp = block->wp + count;
		if (wp >= block->size)
			wp -= block->size;

		len = min(block->size - wp, chunk->pading_size);

		memset(p, 0, len);

		dma_sync_single_for_device(amports_get_dma_device(),
			block->start + wp,
			len, DMA_TO_DEVICE);

		if (chunk->pading_size > len) {
			p = (u8 *)block->start_virt;

			memset(p, 0, count - len);

			dma_sync_single_for_device(amports_get_dma_device(),
				block->start,
				chunk->pading_size - len, DMA_TO_DEVICE);
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

static void vframe_block_free_block(struct vframe_block_list_s *block)
{
	if (block->addr) {
		codec_mm_free_for_dma(MEM_NAME,	block->addr);
	}
	/*
	pr_err("free block %d, size=%d\n", block->id, block->size);
	*/
	kfree(block);
}

static int vframe_block_init_alloc_storage(struct vdec_input_s *input,
			struct vframe_block_list_s *block)
{
	int alloc_size = input->default_block_size;
	block->magic = 0x4b434c42;
	block->input = input;
	block->type = input->type;

	/*
	 * todo: for different type use different size
	 */
	alloc_size = PAGE_ALIGN(alloc_size);
	block->addr = codec_mm_alloc_for_dma_ex(
		MEM_NAME,
		alloc_size/PAGE_SIZE,
		VFRAME_BLOCK_PAGEALIGN,
		CODEC_MM_FLAGS_DMA_CPU | CODEC_MM_FLAGS_FOR_VDECODER,
		input->id,
		block->id);

	if (!block->addr) {
		pr_err("Input block allocation failed\n");
		return -ENOMEM;
	}

	block->start_virt = (void *)codec_mm_phys_to_virt(block->addr);
	block->start = block->addr;
	block->size = alloc_size;

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
}
int vdec_input_prepare_bufs(struct vdec_input_s *input,
	int frame_width, int frame_height)
{
	struct vframe_block_list_s *block;
	int i;
	unsigned long flags;

	if (input->size > 0)
		return 0;
	if (frame_width * frame_height >= 1920 * 1088) {
		/*have add data before. ignore prepare buffers.*/
		input->default_block_size = VFRAME_BLOCK_SIZE_4K;
	}
	/*prepared 3 buffers for smooth start.*/
	for (i = 0; i < 3; i++) {
		block = vdec_input_alloc_new_block(input);
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
		"\t[%lld:%p]-off=%d,size:%d,p:%d,\tpts64=%lld,addr=%p\n",
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

int vdec_input_dump_chunks(struct vdec_input_s *input,
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
	snprintf(lbuf + s, size - s,
		"blocks:vdec-%d id:%d,bufsize=%d,dsize=%d,frames:%d,maxframe:%d\n",
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
		s += vdec_input_dump_chunk_locked(chunk, lbuf, size - s);
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
		input->swap_page = alloc_page(GFP_KERNEL);
		if (input->swap_page) {
			input->swap_page_phys =
				page_to_phys(input->swap_page);
		}
	}

	if (input->swap_page_phys == 0)
		return -ENOMEM;

	return 0;
}

void vdec_input_set_type(struct vdec_input_s *input, int type, int target)
{
	input->type = type;
	input->target = target;
	if (type == VDEC_TYPE_FRAME_CIRCULAR) {
		/*alway used max block.*/
		input->default_block_size = VFRAME_BLOCK_SIZE_MAX;
	}
}

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

static struct vframe_block_list_s *
	vdec_input_alloc_new_block(struct vdec_input_s *input)
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
		block) != 0) {
		kfree(block);
		pr_err("vframe_block storage allocation failed\n");
		return NULL;
	}

	INIT_LIST_HEAD(&block->list);

	vdec_input_add_block(input, block);

	/*
	pr_info("vdec-%d:new block id=%d, total_blocks:%d, size=%d\n",
		input->id,
		block->id,
		input->block_nums,
		block->size);
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
static int vdec_input_get_duration_u64(struct vdec_input_s *input)
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
	block = vdec_input_alloc_new_block(input);
	if (!block) {
		input->no_mem_err_cnt++;
		return -EAGAIN;
	}
	input->no_mem_err_cnt = 0;
	*block_ret = block;
	return 0;
}

int vdec_input_add_frame(struct vdec_input_s *input, const char *buf,
			size_t count)
{
	unsigned long flags;
	struct vframe_chunk_s *chunk;
	struct vdec_s *vdec = input->vdec;
	struct vframe_block_list_s *block;
	int need_pading_size = MIN_FRAME_PADDING_SIZE;

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
	chunk = kzalloc(sizeof(struct vframe_chunk_s), GFP_KERNEL);

	if (!chunk) {
		pr_err("vframe_chunk structure allocation failed\n");
		return -ENOMEM;
	}

	chunk->magic = 0x4b554843;
	if (vdec->pts_valid) {
		chunk->pts = vdec->pts;
		chunk->pts64 = vdec->pts64;
	}
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
	chunk->offset = block->wp;
	chunk->size = count;
	chunk->pading_size = need_pading_size;
	INIT_LIST_HEAD(&chunk->list);

	if (vframe_chunk_fill(input, chunk, buf, count, block)) {
		pr_err("vframe_chunk_fill failed\n");
		kfree(chunk);
		return -EFAULT;
	}

	flags = vdec_input_lock(input);

	vframe_block_add_chunk(block, chunk);

	list_add_tail(&chunk->list, &input->vframe_chunk_list);
	input->data_size += chunk->size;
	input->have_frame_num++;
	if (chunk->pts_valid) {
		input->last_inpts_u64 = chunk->pts64;
		input->last_in_nopts_cnt = 0;
	} else {
		/*nopts*/
		input->last_in_nopts_cnt++;
	}
	vdec_input_unlock(input, flags);
	if (chunk->size > input->frame_max_size)
		input->frame_max_size = chunk->size;
	input->total_wr_count += count;
#if 0
	if (add_count == 2)
		input->total_wr_count += 38;
#endif

	return count;
}

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

void vdec_input_release_chunk(struct vdec_input_s *input,
			struct vframe_chunk_s *chunk)
{
	unsigned long flags;
	struct vframe_block_list_s *block = chunk->block;
	struct vframe_block_list_s *tofreeblock = NULL;
	flags = vdec_input_lock(input);

	list_del(&chunk->list);
	input->have_frame_num--;
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
	if (block->chunk_count == 0 &&
		input->wr_block != block) {/*don't free used block*/
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

unsigned long vdec_input_lock(struct vdec_input_s *input)
{
	unsigned long flags;

	spin_lock_irqsave(&input->lock, flags);

	return flags;
}

void vdec_input_unlock(struct vdec_input_s *input, unsigned long flags)
{
	spin_unlock_irqrestore(&input->lock, flags);
}

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
	if (input->swap_page_phys) {
		if (vdec_secure(input->vdec))
			codec_mm_free_for_dma("SWAP", input->swap_page_phys);
		else
			__free_page(input->swap_page);
		input->swap_page = NULL;
		input->swap_page_phys = 0;
	}
	input->swap_valid = false;
}




