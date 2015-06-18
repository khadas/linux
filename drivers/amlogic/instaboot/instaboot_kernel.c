#include <linux/amlogic/instaboot/instaboot.h>
#include <linux/export.h>
#include <linux/atomic.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <asm/memory.h>
#include <linux/utsname.h>
#include "instaboot_kernel.h"

static void *nftl_info_trans_buffer[3]  __nosavedata;
EXPORT_SYMBOL(nftl_info_trans_buffer);

#ifdef CONFIG_HIBERNATION

atomic_t snapshot_device_available = ATOMIC_INIT(1);

int aml_snapshot_read_next(struct snapshot_handle *handle)
{
	return snapshot_read_next(handle);
}
EXPORT_SYMBOL(aml_snapshot_read_next);

int aml_snapshot_write_next(struct snapshot_handle *handle)
{
	return snapshot_write_next(handle);
}
EXPORT_SYMBOL(aml_snapshot_write_next);

void aml_snapshot_write_finalize(struct snapshot_handle *handle)
{
	snapshot_write_finalize(handle);
}
EXPORT_SYMBOL(aml_snapshot_write_finalize);

unsigned long aml_snapshot_get_image_size(void)
{
	return snapshot_get_image_size();
}
EXPORT_SYMBOL(aml_snapshot_get_image_size);

int aml_snapshot_image_loaded(struct snapshot_handle *handle)
{
	return snapshot_image_loaded(handle);
}
EXPORT_SYMBOL(aml_snapshot_image_loaded);

dev_t aml_get_swsusp_resume_device(void)
{
	pr_debug("swsusp_resume_device: %u\n",
			(unsigned int)swsusp_resume_device);
	return swsusp_resume_device;
}
EXPORT_SYMBOL(aml_get_swsusp_resume_device);

end_swap_bio_read_p_t aml_get_end_swap_bio_read(void)
{
	end_swap_bio_read_p_t fun_p;
	fun_p = end_swap_bio_read;
	return end_swap_bio_read;
}
EXPORT_SYMBOL(aml_get_end_swap_bio_read);

unsigned int aml_nr_free_highpages(void)
{
	return nr_free_highpages();
}
EXPORT_SYMBOL(aml_nr_free_highpages);

void aml_bio_set_pages_dirty(struct bio *bio)
{
	bio_set_pages_dirty(bio);
}
EXPORT_SYMBOL(aml_bio_set_pages_dirty);

void aml_get_utsname(struct new_utsname *un)
{
	if (un)
		memcpy(un, init_utsname(), sizeof(struct new_utsname));
}
EXPORT_SYMBOL(aml_get_utsname);

int ib_show_progress_bar(u32 percent)
{
	return osd_show_progress_bar(percent);
}
EXPORT_SYMBOL(ib_show_progress_bar);

/*
   in kernel booting process, acquire some memory for device probe,
   which will not be crush when recovery the instaboot image.
*/
struct reserve_mem {
	unsigned long long startaddr;
	unsigned long long size;
	unsigned int flag;		/* 0: high memory  1:low memory */
	char name[16];			/* limit: device name must < 14; */
};

#define MAX_RESERVE_BLOCK  32

struct reserve_mgr {
	int count;
	unsigned long long start_memory_addr;
	unsigned long long total_memory;
	unsigned long long current_addr_from_low;
	unsigned long long current_addr_from_high;
	struct reserve_mem reserve[MAX_RESERVE_BLOCK];
};

struct mem_block {
	struct list_head hook;
	unsigned long start;
	unsigned long end;
};

static LIST_HEAD(mem_list);

void *aml_boot_alloc_mem(size_t size)
{
	phys_addr_t buf = 0;
	unsigned int page_num, blk_pfn_n;
	struct mem_block *mem_blk;
	unsigned long buf_pfn = 0;

	page_num = (size + (1 << PAGE_SHIFT) - 1) >> PAGE_SHIFT;

	list_for_each_entry(mem_blk, &mem_list, hook) {
		blk_pfn_n = mem_blk->end - mem_blk->start;
		if (page_num < blk_pfn_n) {
			buf_pfn = mem_blk->start;
			mem_blk->start += page_num;
			break;
		}
	}
	if (buf_pfn)
		buf = (phys_addr_t)pfn_to_kaddr(buf_pfn);

	pr_info("alloc buf: 0x%lx, size: %lu\n",
			(unsigned long)buf, (unsigned long)size);
	return (void *)buf;
}
EXPORT_SYMBOL(aml_boot_alloc_mem);

static int regist_free_memblk(unsigned long start, unsigned long end)
{
	struct mem_block *mem_blk;
	unsigned long s_pfn, e_pfn;

	s_pfn = (start + (1 << PAGE_SHIFT) - 1) >> PAGE_SHIFT;
	e_pfn = (end + (1 << PAGE_SHIFT) - 1) >> PAGE_SHIFT;
	list_for_each_entry(mem_blk, &mem_list, hook) {
		if (!mem_blk)
			break;
		if (s_pfn > mem_blk->end || e_pfn < mem_blk->start)
			continue;
		if (s_pfn <= mem_blk->start)
			mem_blk->start = s_pfn;
		if (e_pfn >= mem_blk->end)
			mem_blk->end = e_pfn;
		return 0;
	}

	mem_blk = kzalloc(sizeof(struct mem_block), GFP_KERNEL);
	if (!mem_blk)
		return -ENOMEM;

	mem_blk->start = s_pfn;
	mem_blk->end = e_pfn;

	list_add(&mem_blk->hook, &mem_list);
	/*pr_info("reg memblk: 0x%lx ~ 0x%lx\n", start, end);*/

	return 0;
}

static int __init aml_boot_mem_init(void)
{
	struct reserve_mgr *rsv_mgr;
	struct reserve_mem *prm;
	unsigned long start = 0, end = 0;
	int i;
	unsigned long long base_addr;

	rsv_mgr = NULL;
	if (!rsv_mgr)
		return 0;

	for (i = 0; i < rsv_mgr->count; i++) {
		prm = &rsv_mgr->reserve[i];
		if ((!(prm->flag & 2))) {
			start = (unsigned long)(prm->startaddr + base_addr);
			end = (unsigned long)(prm->startaddr +
					prm->size - 1 + base_addr);
			regist_free_memblk(start, end);
		}
	}

	return 0;
}
core_initcall(aml_boot_mem_init);

#else
void *aml_boot_alloc_mem(size_t size)
{
	return NULL;
}
EXPORT_SYMBOL(aml_boot_alloc_mem);
#endif /* CONFIG_HIBERNATION */
