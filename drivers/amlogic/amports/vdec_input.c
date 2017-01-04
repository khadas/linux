#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/amlogic/codec_mm/codec_mm.h>

#include "amports_priv.h"
#include "vdec.h"
#include "vdec_input.h"

#define VFRAME_BLOCK_SIZE (4*SZ_1M)
#define VFRAME_BLOCK_PAGESIZE (PAGE_ALIGN(VFRAME_BLOCK_SIZE)/PAGE_SIZE)
#define VFRAME_BLOCK_PAGEALIGN 4
#define VFRAME_BLOCK_MAX_LEVEL (8*SZ_1M)
#define VFRAME_BLOCK_HOLE (SZ_64K)

#define FRAME_PADDING_SIZE 1024U
#define MEM_NAME "VFRAME_INPUT"

static int vframe_chunk_fill(struct vdec_input_s *input,
			struct vframe_chunk_s *chunk, const char *buf,
			size_t count, struct vframe_block_list_s *block)
{
	u8 *p = (u8 *)block->start_virt + block->wp;

	if (block->type == VDEC_TYPE_FRAME_BLOCK) {
		if (copy_from_user(p, buf, count))
			return -EFAULT;

		p += count;

		memset(p, 0, FRAME_PADDING_SIZE);

		dma_sync_single_for_device(amports_get_dma_device(),
			block->start + block->wp,
			count + FRAME_PADDING_SIZE, DMA_TO_DEVICE);

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

		len = min(block->size - wp, FRAME_PADDING_SIZE);

		memset(p, 0, len);

		dma_sync_single_for_device(amports_get_dma_device(),
			block->start + wp,
			len, DMA_TO_DEVICE);

		if (FRAME_PADDING_SIZE > len) {
			p = (u8 *)block->start_virt;

			memset(p, 0, count - len);

			dma_sync_single_for_device(amports_get_dma_device(),
					block->start,
					count - len, DMA_TO_DEVICE);
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
	block->wp += chunk->size + FRAME_PADDING_SIZE;
	if (block->wp >= block->size)
		block->wp -= block->size;
	block->chunk_count++;
	chunk->block = block;
	block->input->wr_block = block;
	chunk->sequence = block->input->sequence;
	block->input->sequence++;
}

static void vframe_block_free_storage(struct vframe_block_list_s *block)
{
	if (block->addr) {
		dma_unmap_single(
			amports_get_dma_device(),
			block->start,
			VFRAME_BLOCK_PAGESIZE,
			DMA_TO_DEVICE);

		codec_mm_free_for_dma(MEM_NAME,	block->addr);

		block->addr = 0;
		block->start_virt = NULL;
		block->start = 0;
	}

	block->size = 0;
}

static int vframe_block_init_alloc_storage(struct vdec_input_s *input,
			struct vframe_block_list_s *block)
{
	block->magic = 0x4b434c42;
	block->input = input;
	block->type = input->type;

	/*
	 * todo: for different type use different size
	 */
	block->addr = codec_mm_alloc_for_dma(
		MEM_NAME,
		VFRAME_BLOCK_PAGESIZE,
		VFRAME_BLOCK_PAGEALIGN,
		CODEC_MM_FLAGS_DMA_CPU | CODEC_MM_FLAGS_FOR_VDECODER);

	if (!block->addr) {
		pr_err("Input block allocation failed\n");
		return -ENOMEM;
	}

	block->start_virt = (void *)codec_mm_phys_to_virt(block->addr);
	block->start = dma_map_single(
		amports_get_dma_device(),
		block->start_virt,
		VFRAME_BLOCK_PAGESIZE,
		DMA_TO_DEVICE);
	block->size = VFRAME_BLOCK_PAGESIZE * PAGE_SIZE;

	return 0;
}

void vdec_input_init(struct vdec_input_s *input, struct vdec_s *vdec)
{
	INIT_LIST_HEAD(&input->vframe_block_list);
	INIT_LIST_HEAD(&input->vframe_chunk_list);
	spin_lock_init(&input->lock);

	input->vdec = vdec;
}

int vdec_input_set_buffer(struct vdec_input_s *input, u32 start, u32 size)
{
	if (input_frame_based(input))
		return -EINVAL;

	input->start = start;
	input->size = size;
	input->swap_page = alloc_page(GFP_KERNEL);

	if (input->swap_page == NULL)
		return -ENOMEM;

	return 0;
}

void vdec_input_set_type(struct vdec_input_s *input, int type, int target)
{
	input->type = type;
	input->target = target;
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

	list_add_tail(&block->list, &input->vframe_block_list);
	input->size += block->size;

	vdec_input_unlock(input, flags);
}

static void vdec_input_remove_block(struct vdec_input_s *input,
				struct vframe_block_list_s *block)
{
	unsigned long flags;

	flags = vdec_input_lock(input);

	list_del(&block->list);
	input->size -= block->size;

	vdec_input_unlock(input, flags);

	vframe_block_free_storage(block);

	kfree(block);

	pr_info("block %p removed\n", block);
}

int vdec_input_level(struct vdec_input_s *input)
{
	return input->total_wr_count - input->total_rd_count;
}

int vdec_input_add_frame(struct vdec_input_s *input, const char *buf,
			size_t count)
{
	unsigned long flags;
	struct vframe_chunk_s *chunk;
	struct vdec_s *vdec = input->vdec;
	struct vframe_block_list_s *block = input->wr_block;

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

	/* prepare block to write */
	if ((!block) || (block &&
		(vframe_block_space(block) < (count + FRAME_PADDING_SIZE)))) {
		/* switch to another block for the added chunk */
		struct vframe_block_list_s *new_block;

#if 0
		pr_info("Adding new block, total level = %d, total_wr_count=%d, total_rd_count=%d\n",
			vdec_input_level(input),
			(int)input->total_wr_count,
			(int)input->total_rd_count);
#endif

		if ((!list_empty(&input->vframe_block_list)) &&
			(block->type == VDEC_TYPE_FRAME_CIRCULAR))
			return -EAGAIN;
		else {
			/* todo: add input limit check for
			 * VDEC_TYPE_FRAME_BLOCK
			 */
			if (vdec_input_level(input) >
				VFRAME_BLOCK_MAX_LEVEL) {
				pr_info("vdec_input %p reaching max size\n",
					input);
				return -EAGAIN;
			}
		}

		/* check next block of current wr_block, it should be an empty
		 * block to use
		 */
		if ((block) && (!list_is_last(
			&block->list, &input->vframe_block_list))) {
			block = list_next_entry(block, list);

			if (vframe_block_space(block) != VFRAME_BLOCK_SIZE)
				/* should never happen */
				pr_err("next writing block not empty.\n");
		} else {
			/* add a new block into input list */
			new_block = kzalloc(sizeof(struct vframe_block_list_s),
					GFP_KERNEL);
			if (new_block == NULL) {
				pr_err("vframe_block structure allocation failed\n");
				return -EAGAIN;
			}

			if (vframe_block_init_alloc_storage(input,
				new_block) != 0) {
				kfree(new_block);
				pr_err("vframe_block storage allocation failed\n");
				return -EAGAIN;
			}

			INIT_LIST_HEAD(&new_block->list);

			vdec_input_add_block(input, new_block);

			/* pr_info("added new block %p\n", new_block); */

			block = new_block;
		}
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
	chunk->pts_valid = vdec->pts_valid;
	vdec->pts_valid = false;
	chunk->offset = block->wp;
	chunk->size = count;

	INIT_LIST_HEAD(&chunk->list);

	if (vframe_chunk_fill(input, chunk, buf, count, block)) {
		pr_err("vframe_chunk_fill failed\n");
		kfree(chunk);
		return -EFAULT;
	}

	flags = vdec_input_lock(input);

	vframe_block_add_chunk(block, chunk);

	list_add_tail(&chunk->list, &input->vframe_chunk_list);

	vdec_input_unlock(input, flags);

	input->total_wr_count += count;

#if 0
	if (add_count == 2)
		input->total_wr_count += 38;
#endif

	return count;
}

struct vframe_chunk_s *vdec_input_next_chunk(struct vdec_input_s *input)
{
	if (list_empty(&input->vframe_chunk_list))
		return NULL;

	return list_first_entry(&input->vframe_chunk_list,
				struct vframe_chunk_s, list);
}

struct vframe_chunk_s *vdec_input_next_input_chunk(
			struct vdec_input_s *input)
{
	struct vframe_chunk_s *chunk = NULL;
	struct list_head *p;

	list_for_each(p, &input->vframe_chunk_list) {
		struct vframe_chunk_s *c = list_entry(
			p, struct vframe_chunk_s, list);
		if ((c->flag & VFRAME_CHUNK_FLAG_CONSUMED) == 0) {
			chunk = c;
			break;
		}
	}

	return chunk;
}

void vdec_input_release_chunk(struct vdec_input_s *input,
			struct vframe_chunk_s *chunk)
{
	unsigned long flags;
	struct vframe_block_list_s *block = chunk->block;

	flags = vdec_input_lock(input);

	list_del(&chunk->list);

	block->rp += chunk->size;
	if (block->rp >= block->size)
		block->rp -= block->size;
	block->chunk_count--;
	input->total_rd_count += chunk->size;

	if (block->chunk_count == 0) {
		/* reuse the block */
		block->wp = 0;
		block->rp = 0;

		if ((input->wr_block == block) &&
			(!list_is_last(&block->list,
				&input->vframe_block_list)))
			input->wr_block = list_next_entry(block, list);

		list_move_tail(&block->list, &input->vframe_block_list);
	}

	vdec_input_unlock(input, flags);

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

	/* release input blocks */
	list_for_each_safe(p, tmp, &input->vframe_block_list) {
		struct vframe_block_list_s *block = list_entry(
			p, struct vframe_block_list_s, list);
		vdec_input_remove_block(input, block);
	}

	/* release swap page */
	if (input->swap_page) {
		__free_page(input->swap_page);
		input->swap_page = NULL;
		input->swap_valid = false;
	}
}




