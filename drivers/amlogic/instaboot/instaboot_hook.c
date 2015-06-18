#include <linux/amlogic/instaboot/instaboot.h>
#include <linux/kernel.h>
#include <linux/mm_types.h>

static aml_istbt_int_void_fun_t aml_istbt_dev_ready_hook;
int aml_istbt_dev_ready(void)
{
	if (aml_istbt_dev_ready_hook)
		return aml_istbt_dev_ready_hook();
	else {
		pr_err("%s NULL\n", __func__);
		return -1;
	}
}

static aml_istbt_void_fmode_fun_t aml_istbt_swsusp_close_hook;
void swsusp_close(fmode_t mode)
{
	if (aml_istbt_swsusp_close_hook)
		aml_istbt_swsusp_close_hook(mode);
	else
		pr_err("%s NULL\n", __func__);
}

static aml_istbt_int_uintp_func_t aml_istbt_swsusp_read_hook;
int swsusp_read(unsigned int *flags_p)
{
	if (aml_istbt_swsusp_read_hook)
		return aml_istbt_swsusp_read_hook(flags_p);
	else {
		pr_err("%s NULL\n", __func__);
		return -1;
	}
}

static aml_istbt_int_uint_func_t aml_istbt_swsusp_write_hook;
int swsusp_write(unsigned int flags)
{
	if (aml_istbt_swsusp_write_hook)
		return aml_istbt_swsusp_write_hook(flags);
	else {
		pr_err("%s NULL\n", __func__);
		return -1;
	}
}

static aml_istbt_int_void_fun_t aml_istbt_swsusp_check_hook;
int swsusp_check(void)
{
	if (aml_istbt_swsusp_check_hook)
		return aml_istbt_swsusp_check_hook();
	else {
		pr_err("%s NULL\n", __func__);
		return -1;
	}
}

static aml_istbt_int_void_fun_t aml_istbt_swsusp_unmark_hook;
int swsusp_unmark(void)
{
	if (aml_istbt_swsusp_unmark_hook)
		return aml_istbt_swsusp_unmark_hook();
	else {
		pr_err("%s NULL\n", __func__);
		return -1;
	}
}

static aml_istbt_void_voidp_int_func_t aml_istbt_free_image_page_hook;
void istbt_free_image_page(void *addr, int clear_nosave_free)
{
	if (aml_istbt_free_image_page_hook)
		aml_istbt_free_image_page_hook(addr, clear_nosave_free);
	else
		pr_err("%s NULL\n", __func__);
	return;
}

static aml_istbt_pagep_gfp_func_t aml_istbt_alloc_image_page_hook;
struct page *istbt_alloc_image_page(gfp_t gfp_mask)
{
	if (aml_istbt_alloc_image_page_hook)
		return aml_istbt_alloc_image_page_hook(gfp_mask);
	else {
		pr_err("%s NULL\n", __func__);
		return NULL;
	}
}

static aml_istbt_int_ulong_func_t aml_istbt_pfn_touchable_hook;
int istbt_pfn_touchable(unsigned long pfn)
{
	if (aml_istbt_pfn_touchable_hook)
		return aml_istbt_pfn_touchable_hook(pfn);
	else {
		pr_err("%s NULL\n", __func__);
		return -1;
	}
}

static aml_istbt_void_ulong_func_t aml_istbt_pfn_destory_hook;
void istbt_pfn_destory(unsigned long pfn)
{
	if (aml_istbt_pfn_destory_hook)
		aml_istbt_pfn_destory_hook(pfn);
	else
		pr_err("%s NULL\n", __func__);
	return;
}

static aml_istbt_void_longp_longp_func_t aml_istbt_copy_page_hook;
void istbt_copy_page(long *dst, long *src)
{
	if (aml_istbt_copy_page_hook)
		aml_istbt_copy_page_hook(dst, src);
	else
		pr_err("%s NULL\n", __func__);
	return;
}

static aml_istbt_int_void_fun_t aml_istbt_init_mem_hook;
int istbt_init_mem(void)
{
	if (aml_istbt_init_mem_hook)
		return aml_istbt_init_mem_hook();
	else {
		pr_err("%s NULL\n", __func__);
		return -1;
	}
}

int aml_istbt_reg_fun(int fun_type, void *fun_p)
{
	switch (fun_type) {
	case AML_ISTBT_DEV_READY:
		aml_istbt_dev_ready_hook = fun_p;
		break;
	case AML_ISTBT_SWSUSP_CLOSE:
		aml_istbt_swsusp_close_hook = fun_p;
		break;
	case AML_ISTBT_SWSUSP_READ:
		aml_istbt_swsusp_read_hook = fun_p;
		break;
	case AML_ISTBT_SWSUSP_WRITE:
		aml_istbt_swsusp_write_hook = fun_p;
		break;
	case AML_ISTBT_SWSUSP_CHECK:
		aml_istbt_swsusp_check_hook = fun_p;
		break;
	case AML_ISTBT_SWSUSP_UNMARK:
		aml_istbt_swsusp_unmark_hook = fun_p;
		break;
	case AML_ISTBT_ALLOC_IMAGE_PAGE:
		aml_istbt_alloc_image_page_hook = fun_p;
		break;
	case AML_ISTBT_FREE_IMAGE_PAGE:
		aml_istbt_free_image_page_hook = fun_p;
		break;
	case AML_ISTBT_INIT_MEM:
		aml_istbt_init_mem_hook = fun_p;
		break;
	case AML_ISTBT_PFN_TOUCHABLE:
		aml_istbt_pfn_touchable_hook = fun_p;
		break;
	case AML_ISTBT_PFN_DESTORY:
		aml_istbt_pfn_destory_hook = fun_p;
		break;
	case AML_ISTBT_COPY_PAGE:
		aml_istbt_copy_page_hook = fun_p;
		break;
	default:
		break;
	}
	return 0;
}
EXPORT_SYMBOL(aml_istbt_reg_fun);

int aml_istbt_unreg_fun(int fun_type)
{
	switch (fun_type) {
	case AML_ISTBT_DEV_READY:
		aml_istbt_dev_ready_hook = NULL;
		break;
	case AML_ISTBT_SWSUSP_CLOSE:
		aml_istbt_swsusp_close_hook = NULL;
		break;
	case AML_ISTBT_SWSUSP_READ:
		aml_istbt_swsusp_read_hook = NULL;
		break;
	case AML_ISTBT_SWSUSP_WRITE:
		aml_istbt_swsusp_write_hook = NULL;
		break;
	case AML_ISTBT_SWSUSP_CHECK:
		aml_istbt_swsusp_check_hook = NULL;
		break;
	case AML_ISTBT_SWSUSP_UNMARK:
		aml_istbt_swsusp_unmark_hook = NULL;
		break;
	case AML_ISTBT_ALLOC_IMAGE_PAGE:
		aml_istbt_alloc_image_page_hook = NULL;
		break;
	case AML_ISTBT_FREE_IMAGE_PAGE:
		aml_istbt_free_image_page_hook = NULL;
		break;
	case AML_ISTBT_INIT_MEM:
		aml_istbt_init_mem_hook = NULL;
		break;
	case AML_ISTBT_PFN_TOUCHABLE:
		aml_istbt_pfn_touchable_hook = NULL;
		break;
	case AML_ISTBT_PFN_DESTORY:
		aml_istbt_pfn_destory_hook = NULL;
		break;
	case AML_ISTBT_COPY_PAGE:
		aml_istbt_copy_page_hook = NULL;
		break;
	default:
		break;
	}
	return 0;
}
EXPORT_SYMBOL(aml_istbt_unreg_fun);
