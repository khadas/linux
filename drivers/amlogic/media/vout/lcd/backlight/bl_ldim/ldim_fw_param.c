// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include <linux/amlogic/media/vout/lcd/ldim_fw.h>
#include "ldim_drv.h"

static struct ldim_stts_s ldim_stts = {
	.global_hist = NULL,
	.seg_hist = NULL,
};

static struct ldim_fw_s ldim_fw = {
	/* header */
	.para_ver = FW_PARA_VER,
	.para_size = sizeof(struct ldim_fw_s),
	.alg_ver = "not installed",
	.fw_sel = 1, /* bit0:hw/sw, bit1:no/have fw_cus */
	.valid = 0,
	.flag = 0,

	.seg_col = 1,
	.seg_row = 1,
	.func_en = 1,
	.remap_en = 0,
	.res_update = 0,
	.fw_ctrl = 0x10,/*bit4:ld_sel*/
	.fw_state = 0,

	.bl_matrix_dbg = 0,
	.fw_hist_print = 0,
	.fw_print_frequent = 300,
	.fw_print_lv = 0,

	.conf = NULL,
	.rmem = NULL,
	.stts = &ldim_stts,
	.profile = NULL,

	.bl_matrix = NULL,

	.fw_alg_frm = NULL,
	.fw_alg_para_print = NULL,
	.fw_init = NULL,
	.fw_info_update = NULL,
	.fw_pq_set = NULL,
	.fw_profile_set = NULL,
	.fw_rmem_duty_get = NULL,
	.fw_rmem_duty_set = NULL,
	.fw_debug_show = NULL,
	.fw_debug_store = NULL,
};

static struct ldim_fw_custom_s ldim_fw_cus = {
	.valid = 0,
	.seg_col = 1,
	.seg_row = 1,
	.global_hist_bin_num = 64,

	.fw_print_frequent = 200,
	.fw_print_lv = 0,

	.param = NULL,
	.bl_matrix = NULL,

	.fw_alg_frm = NULL,
	.fw_alg_para_print = NULL,
};

struct ldim_fw_s *aml_ldim_get_fw(void)
{
	return &ldim_fw;
}
EXPORT_SYMBOL(aml_ldim_get_fw);

struct ldim_fw_custom_s *aml_ldim_get_fw_cus(void)
{
	return &ldim_fw_cus;
}
EXPORT_SYMBOL(aml_ldim_get_fw_cus);


