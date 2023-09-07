// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "DP_tx.h"
#include "../../lcd_reg.h"
#include "../../lcd_common.h"

static void dptx_edid_print(struct dptx_EDID_s *edp_edid)
{
	int i;

	pr_info("Manufacturer ID: %s\n"
		"Product ID:      0x%04x\n"
		"Product SN:      0x%08x\n"
		"Week:            %d\n"
		"Year:            %d\n"
		"EDID Version:    %04x\n",
		edp_edid->manufacturer_id,
		edp_edid->product_id,
		edp_edid->product_sn,
		edp_edid->week,
		edp_edid->year,
		edp_edid->version);

	for (i = 0; i < 4; i++) {
		switch (edp_edid->block_identity[i]) {
		case BLOCK_ID_SN:
			pr_info("--------- block %d ---------\n"
				"Monitor SN:           %s\n", i, edp_edid->serial_num);
			break;
		case BLOCK_ID_ASCII_STR:
			pr_info("--------- block %d ---------\n"
				"Monitor Ascii String: %s\n", i, edp_edid->asc_string);
			break;
		case BLOCK_ID_RANGE_TIMING:
			pr_info("--------- block %d ---------\n"
				"Monitor Range Timing:\n"
				"    V freq: %d - %d Hz\n"
				"    H freq: %d - %d kHz\n"
				"    Max PixelClk: %d MHz\n", i,
				edp_edid->range_limit.min_vfreq, edp_edid->range_limit.max_v_freq,
				edp_edid->range_limit.min_hfreq, edp_edid->range_limit.max_hfreq,
				edp_edid->range_limit.max_pclk / 1000000);
			break;
		case BLOCK_ID_PRODUCK_NAME:
			pr_info("--------- block %d ---------\n"
				"Monitor Name:      %s\n", i, edp_edid->name);
			break;
		case BLOCK_ID_DETAIL_TIMING:
			pr_info("--------- block %d ---------\n"
				"Detail Timing:\n"
				"    Pixel Clock:   %ld.%ld MHz\n"
				"    H Active:      %d\n"
				"    H Blank:       %d\n"
				"    V Active:      %d\n"
				"    V Blank:       %d\n"
				"    H FP:          %d\n"
				"    H PW:          %d\n"
				"    V FP:          %d\n"
				"    V PW:          %d\n"
				"    H Size:        %dmm\n"
				"    V Size:        %dmm\n"
				"    H Border:      %d\n"
				"    V Border:      %d\n"
				"    Hsync Pol:     %d\n"
				"    Vsync Pol:     %d\n", i,
				edp_edid->detail_timing[i].pclk / 1000000,
				(edp_edid->detail_timing[i].pclk % 1000000) / 1000,
				edp_edid->detail_timing[i].h_a,
				edp_edid->detail_timing[i].h_b,
				edp_edid->detail_timing[i].v_a,
				edp_edid->detail_timing[i].v_b,
				edp_edid->detail_timing[i].h_fp,
				edp_edid->detail_timing[i].h_pw,
				edp_edid->detail_timing[i].v_fp,
				edp_edid->detail_timing[i].v_pw,
				edp_edid->detail_timing[i].h_size,
				edp_edid->detail_timing[i].v_size,
				edp_edid->detail_timing[i].h_border,
				edp_edid->detail_timing[i].v_border,
				(edp_edid->detail_timing[i].timing_ctrl >> 1) & 0x1,
				(edp_edid->detail_timing[i].timing_ctrl >> 2) & 0x1);
			break;
		default:
			pr_info("--------- block %d (empty) ---------\n", i);
			break;
		}
	}
}

static int dptx_edid_valid_check(unsigned char *edid_buf)
{
	unsigned char header[8] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};
	unsigned int checksum = 0;
	int i;

	if (memcmp(edid_buf, header, 8)) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDERR("%s: invalid EDID header\n", __func__);
		return -1;
	}

	for (i = 0; i < 128; i++)
		checksum += edid_buf[i];
	if ((checksum & 0xff)) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDERR("%s: EDID checksum Wrong\n", __func__);
		return -1;
	}

	return 0;
}

static void dptx_edid_parse(unsigned char *edid_buf, struct dptx_EDID_s *edp_edid)
{
	struct dptx_range_limit_s *range;
	struct dptx_detail_timing_s *timing;
	unsigned int temp;
	int i, j;

	range = &edp_edid->range_limit;
	timing = edp_edid->detail_timing;

	temp = ((edid_buf[8] << 8) | edid_buf[9]);
	for (i = 0; i < 3; i++)
		edp_edid->manufacturer_id[i] = (((temp >> ((2 - i) * 5)) & 0x1f) - 1) + 'A';

	edp_edid->manufacturer_id[3] = '\0';
	temp = ((edid_buf[11] << 8) | edid_buf[10]);
	edp_edid->product_id = temp;
	temp = ((edid_buf[12] << 24) | (edid_buf[13] << 16) | (edid_buf[14] << 8) | edid_buf[15]);
	edp_edid->product_sn = temp;
	edp_edid->week = edid_buf[16];
	edp_edid->year = 1990 + edid_buf[17];
	temp = ((edid_buf[18] << 8) | edid_buf[19]);
	edp_edid->version = temp;

	// edp_edid->string_flag = 0;
	for (i = 0; i < 4; i++) {
		j = 54 + i * 18;

		if (edid_buf[j] || edid_buf[j + 1]) {//detail timing
			edp_edid->block_identity[i] = BLOCK_ID_DETAIL_TIMING;
			temp = ((edid_buf[j + 1] << 8) | (edid_buf[j])) * 10000;
			timing[i].pclk = temp;
			temp = ((((edid_buf[j + 4] >> 4) & 0xf) << 8) | edid_buf[j + 2]);
			timing[i].h_a = temp;
			temp = ((((edid_buf[j + 4] >> 0) & 0xf) << 8) | edid_buf[j + 3]);
			timing[i].h_b = temp;
			temp = ((((edid_buf[j + 7] >> 4) & 0xf) << 8) | edid_buf[j + 5]);
			timing[i].v_a = temp;
			temp = ((((edid_buf[j + 7] >> 0) & 0xf) << 8) | edid_buf[j + 6]);
			timing[i].v_b = temp;
			temp = ((((edid_buf[j + 11] >> 6) & 0x3) << 8) | edid_buf[j + 8]);
			timing[i].h_fp = temp;
			temp = ((((edid_buf[j + 11] >> 4) & 0x3) << 8) | edid_buf[j + 9]);
			timing[i].h_pw = temp;
			temp = ((((edid_buf[j + 11] >> 2) & 0x3) << 4) |
				((edid_buf[j + 10] >> 4) & 0xf));
			timing[i].v_fp = temp;
			temp = ((((edid_buf[j + 11] >> 0) & 0x3) << 4) |
				((edid_buf[j + 10] >> 0) & 0xf));
			timing[i].v_pw = temp;
			temp = ((((edid_buf[j + 14] >> 4) & 0xf) << 8) | edid_buf[j + 12]);
			timing[i].h_size = temp;
			temp = ((((edid_buf[j + 14] >> 0) & 0xf) << 8) | edid_buf[j + 13]);
			timing[i].v_size = temp;
			timing[i].h_border = edid_buf[j + 15];
			timing[i].v_border = edid_buf[j + 16];
			timing[i].timing_ctrl = edid_buf[j + 17];
		}

		if (!(edid_buf[j] || edid_buf[j + 1] || edid_buf[j + 2] || edid_buf[j + 4])) {
			switch (edid_buf[j + 3]) {
			case BLOCK_ID_PRODUCK_NAME: //monitor name
				edp_edid->block_identity[i] = BLOCK_ID_PRODUCK_NAME;
				memcpy(edp_edid->name, &edid_buf[j + 5], 13);
				edp_edid->name[13] = '\0';
				break;
			case BLOCK_ID_RANGE_TIMING: //monitor range limits
				edp_edid->block_identity[i] = BLOCK_ID_RANGE_TIMING;
				range->min_vfreq = edid_buf[j + 5];
				range->max_v_freq = edid_buf[j + 6];
				range->min_hfreq = edid_buf[j + 7];
				range->max_hfreq = edid_buf[j + 8];
				range->max_pclk = edid_buf[j + 9] * 10 * 1000000; // Hz
				range->GTF_ctrl = (edid_buf[j + 11] << 8) | edid_buf[j + 10];
				range->GTF_start_hfreq = edid_buf[j + 12] * 2000;
				range->GTF_C = edid_buf[j + 13] / 2;
				range->GTF_M = (edid_buf[j + 15] << 8) | edid_buf[j + 14];
				range->GTF_K = edid_buf[j + 16];
				range->GTF_J = edid_buf[j + 17] / 2;
				break;
			case BLOCK_ID_ASCII_STR: //ascii string
				edp_edid->block_identity[i] = BLOCK_ID_ASCII_STR;
				memcpy(edp_edid->asc_string, &edid_buf[j + 5], 13);
				edp_edid->asc_string[13] = '\0';
				break;
			case BLOCK_ID_SN: //monitor serial num
				edp_edid->block_identity[i] = BLOCK_ID_SN;
				memcpy(edp_edid->serial_num, &edid_buf[j + 5], 13);
				edp_edid->serial_num[13] = '\0';
				break;
			default:
				break;
			}
		}
	}

	edp_edid->ext_flag = edid_buf[126];
	edp_edid->checksum = edid_buf[127];
}

static int dptx_read_edid(struct aml_lcd_drv_s *pdrv, unsigned char *edid_buf)
{
	int i, ret;

	if (!edid_buf) {
		LCDERR("[%d]: %s: edid buf is null\n", pdrv->index, __func__);
		return -1;
	}

	for (i = 0; i < 128; i += 16) {
		ret = dptx_aux_i2c_read(pdrv, 0x50, i, 16, &edid_buf[i]);
		if (ret)
			break;
	}

	return ret;
}

// read & parse EDID
int dptx_EDID_probe(struct aml_lcd_drv_s *pdrv, struct dptx_EDID_s *edp_edid)
{
	unsigned char edid_buf[128];
	char *str;
	int i, n, retry_cnt = 0, ret;

	if (!pdrv || !edp_edid)
		return -1;

dptx_EDID_probe_retry:
	memset(edid_buf, 0, 128 * sizeof(unsigned char));

	ret = dptx_read_edid(pdrv, edid_buf);

	if (ret) {
		if (retry_cnt++ > DPTX_EDID_READ_RETRY_MAX) {
			LCDERR("[%d]: %s: read fail @%d\n", pdrv->index, __func__, retry_cnt);
			goto dptx_EDID_probe_err;
		}
		goto dptx_EDID_probe_retry;
	}

	ret = dptx_edid_valid_check(edid_buf);
	if (ret) {
		if (retry_cnt++ > DPTX_EDID_READ_RETRY_MAX) {
			LCDERR("[%d]: %s: checksum error @%d\n", pdrv->index, __func__, retry_cnt);
			goto dptx_EDID_probe_err;
		}
		goto dptx_EDID_probe_retry;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		str = kcalloc(400, sizeof(char), GFP_KERNEL);
		if (str) {
			LCDPR("[%d]: EDID Raw data:", pdrv->index);
			n = 0;
			for (i = 0; i < 128; i++) {
				if (i % 16 == 0)
					n += sprintf(str + n, "\n");
				n += sprintf(str + n, " %02x", edid_buf[i]);
			}
			pr_info("%s\n", str);
			kfree(str);
		}
	}

	if (retry_cnt > EDP_EDID_RETRY_MAX)
		return -1;

	dptx_edid_parse(edid_buf, edp_edid);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		dptx_edid_print(edp_edid);

	LCDPR("[%d]: %s ok\n", pdrv->index, __func__);
	return 0;

dptx_EDID_probe_err:
	LCDERR("[%d]: %s failed\n", pdrv->index, __func__);
	return -1;
}
