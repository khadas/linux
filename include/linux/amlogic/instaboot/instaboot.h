#ifndef _INSTABOOT_H__
#define _INSTABOOT_H__
#include <linux/bio.h>
#include <linux/types.h>
#include <linux/fs.h>
struct snapshot_handle;
typedef int (*aml_istbt_int_void_fun_t)(void);
typedef void (*aml_istbt_void_fmode_fun_t)(fmode_t mode);
typedef int (*aml_istbt_int_uintp_func_t)(unsigned int *p);
typedef int (*aml_istbt_int_uint_func_t)(unsigned int ui);
typedef int (*aml_istbt_int_int_func_t)(int i);
typedef void (*end_swap_bio_read_p_t)(struct bio *bio, int err);
typedef void (*aml_istbt_void_voidp_int_func_t)(void *, int);
typedef struct page *(*aml_istbt_pagep_gfp_func_t)(gfp_t);
typedef int (*aml_istbt_int_ulong_func_t)(unsigned long);
typedef void (*aml_istbt_void_ulong_func_t)(unsigned long);
typedef void (*aml_istbt_void_longp_longp_func_t)(long *, long *);


enum {
	AML_ISTBT_DEV_READY = 0,
	AML_ISTBT_SWSUSP_CLOSE,
	AML_ISTBT_SWSUSP_READ,
	AML_ISTBT_SWSUSP_WRITE,
	AML_ISTBT_SWSUSP_CHECK,
	AML_ISTBT_SWSUSP_UNMARK,
	AML_ISTBT_ALLOC_IMAGE_PAGE,
	AML_ISTBT_FREE_IMAGE_PAGE,
	AML_ISTBT_INIT_MEM,
	AML_ISTBT_PFN_TOUCHABLE,
	AML_ISTBT_PFN_DESTORY,
	AML_ISTBT_COPY_PAGE,
	AML_ISTBT_FUN_MAX,
};

/* realized in module */
extern int aml_istbt_dev_ready(void);
extern void swsusp_close(fmode_t mode);
extern int swsusp_read(unsigned int *flags_p);
extern int swsusp_write(unsigned int flags);
extern int swsusp_check(void);
extern int swsusp_unmark(void);
extern struct page *istbt_alloc_image_page(gfp_t gfp_mask);
extern void istbt_free_image_page(void *addr, int clear_nosave_free);
extern int istbt_init_mem(void);
extern int istbt_pfn_touchable(unsigned long pfn);
extern void istbt_pfn_destory(unsigned long pfn);
extern void istbt_copy_page(long *dst, long *src);

/* realized in kernel */
extern int aml_snapshot_read_next(struct snapshot_handle *handle);
extern int aml_snapshot_write_next(struct snapshot_handle *handle);
extern void aml_snapshot_write_finalize(struct snapshot_handle *handle);
extern unsigned long aml_snapshot_get_image_size(void);
extern int aml_snapshot_image_loaded(struct snapshot_handle *handle);
extern dev_t aml_get_swsusp_resume_device(void);
extern int ib_show_progress_bar(u32 percent);

extern end_swap_bio_read_p_t aml_get_end_swap_bio_read(void);
extern unsigned int aml_nr_free_highpages(void);
extern void aml_bio_set_pages_dirty(struct bio *bio);

extern void *aml_boot_alloc_rsvmem(size_t size);

extern int aml_istbt_reg_fun(int fun_type, void *fun_p);
extern int aml_istbt_unreg_fun(int fun_type);


extern int notrace swsusp_arch_suspend(void);
extern int swsusp_arch_resume(void);
extern void save_processor_state(void);
extern void restore_processor_state(void);

extern int osd_show_progress_bar(u32 percent);
extern int osd_init_progress_bar(void);

extern void wait_for_emmc_probe(void);

extern unsigned int is_instabooting;

#endif /* _INSTABOOT_H__ */
