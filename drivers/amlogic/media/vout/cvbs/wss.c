// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/vout/cvbs/wss.c
 *
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
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include "wss.h"
#include "cvbs_out_reg.h"
#include "cvbs_mode.h"

static const char * const wss_480i_cmd[] = {"ar", "cgms", "psp",
		"prerec", "CC", "mvsn", "off"};
static const char * const wss_576i_cmd[] = {"ar", "mode", "coding", "helper",
		"ttxsubt", "opensubt", "surrsnd", "cgms", "full", "CC",
		"mvsn", "off"};
static unsigned int cgms_ntsc_crc[] = {0x0, 0x5, 0xa, 0xf};
static unsigned int cvbsout_wss_flag;

static void wss_set_output(unsigned int cmd, unsigned int mode,
			   unsigned int line, unsigned int data,
			   unsigned int start, unsigned int length)
{
	unsigned int value;

	switch (cmd) {
	case WSS_576I_CMD_CC:
	case WSS_480I_CMD_CC:
		cvbsout_wss_flag |= WSS_CC_EN_BIT;
		cvbs_out_reg_write(ENCI_VBI_CCDT_EVN, data);
		/*cvbs_out_reg_write(ENCI_VBI_CCDT_ODD, data);*/
		/*480cvbs default envline 21,oddline 21 */
		/* 576cvbs default envline 21,oddline 22 */
		cvbs_out_reg_setb(ENCI_VBI_SETTING, 0x1, 0, 2);
		break;
	case WSS_480I_CMD_CGMS_A:
		cvbsout_wss_flag |= WSS_480I_CGMS_A_EN_BIT;
		data = (data > 3) ? 0 : data;
		value = ((data << start) | (cgms_ntsc_crc[data] << 14));

		cvbs_out_reg_write(ENCI_VBI_CGMSDT_L, (value & 0xffff));
		cvbs_out_reg_write(ENCI_VBI_CGMSDT_H, ((value >> 16) & 0xff));
		/* bit[7:0] even, bit[15:8] odd */
		cvbs_out_reg_write(ENCI_VBI_CGMS_LN,
				   ((line - 4) | (line - 3) << 8));
		cvbs_out_reg_setb(ENCI_VBI_SETTING, 0x3, 4, 2);
		/*480i, enable even field for line 20*/
		/*enable odd field for line 283 */
		break;
	case WSS_576I_CMD_MVSN:
	case WSS_480I_CMD_MVSN:
		if (!data) {
			cvbsout_wss_flag &= (~WSS_MVSN_EN_BIT);
			cvbs_out_reg_setb(ENCI_VIDEO_MODE_ADV, 0, 15, 1);
			cvbs_out_reg_write(ENCI_MACV_N0, 0x0);
		} else {
			cvbsout_wss_flag |= WSS_MVSN_EN_BIT;
			cvbs_out_reg_setb(ENCI_VIDEO_MODE_ADV, 1, 15, 1);
			cvbs_out_reg_write(ENCI_MACV_N0, 0x3e);
		}
		break;
	case WSS_576I_CMD_CGMS_A:
		cvbsout_wss_flag |= WSS_576I_CGMS_A_EN_BIT;
		cvbs_out_reg_setb(ENCI_VBI_WSSDT, data, start, length);
		value = cvbs_out_reg_read(ENCI_VBI_WSSDT);
		if ((value & 0xf) == 0x0)/* correct the bit3: odd_parity_bit */
			cvbs_out_reg_setb(ENCI_VBI_WSSDT, 1, 3, 1);
		cvbs_out_reg_write(ENCI_VBI_WSS_LN, line - 1);
		if (mode == 480)
			cvbs_out_reg_setb(ENCI_VBI_SETTING, 0x3, 2, 2);
		/*480i, enable even field for line 20*/
		/*enable odd field for line 283 */
		else if (mode == 576)
			cvbs_out_reg_setb(ENCI_VBI_SETTING, 0x1, 2, 2);
		/* 576i, should enable odd field for line 23 */
		break;
	default:
		cvbsout_wss_flag |= WSS_OTHER_BIT;
		cvbs_out_reg_setb(ENCI_VBI_WSSDT, data, start, length);
		value = cvbs_out_reg_read(ENCI_VBI_WSSDT);
		if ((value & 0xf) == 0x0)/* correct the bit3: odd_parity_bit */
			cvbs_out_reg_setb(ENCI_VBI_WSSDT, 1, 3, 1);
		cvbs_out_reg_write(ENCI_VBI_WSS_LN, line - 1);
		if (mode == 480)
			cvbs_out_reg_setb(ENCI_VBI_SETTING, 0x3, 2, 2);
		/*480i, enable even field for line 20*/
		/*enable odd field for line 283 */
		else if (mode == 576)
			cvbs_out_reg_setb(ENCI_VBI_SETTING, 0x1, 2, 2);
		/* 576i, should enable odd field for line 23 */
		break;
	}
	pr_info("[%s] line = %d data = 0x%x start_bit = %d length = %d cmd:%#x wss_flag:%#x\n",
		__func__, line, data, start, length, cmd, cvbsout_wss_flag);
}

static void wss_close_output(unsigned int mode)
{
	//close mvsn
	if (cvbsout_wss_flag & WSS_MVSN_EN_BIT) {
		cvbsout_wss_flag &= (~WSS_MVSN_EN_BIT);
		cvbs_out_reg_setb(ENCI_VIDEO_MODE_ADV, 0, 15, 1);
		cvbs_out_reg_write(ENCI_MACV_N0, 0x0);
	}
	//close cgms-a and other
	if (mode == 480) {
		cvbsout_wss_flag &= (~WSS_480I_CGMS_A_EN_BIT);
		cvbs_out_reg_setb(ENCI_VBI_SETTING, 0x0, 2, 4);
	} else {
		cvbsout_wss_flag &= (~WSS_576I_CGMS_A_EN_BIT);
		cvbsout_wss_flag &= (~WSS_OTHER_BIT);
		cvbs_out_reg_setb(ENCI_VBI_SETTING, 0x0, 2, 2);
	}

	if (cvbsout_wss_flag & WSS_CC_EN_BIT) {
		cvbsout_wss_flag &= (~WSS_CC_EN_BIT);
		cvbs_out_reg_setb(ENCI_VBI_SETTING, 0x0, 0, 2);
	}
	pr_info("[%s]close mode=%d cvbsout_wss_flag:%#x %#x:%#x %#x:%#X %#x:%#X\n",
		__func__, mode, cvbsout_wss_flag,
		ENCI_VBI_SETTING, cvbs_out_reg_read(ENCI_VBI_SETTING),
		ENCI_VIDEO_MODE_ADV, cvbs_out_reg_read(ENCI_VIDEO_MODE_ADV),
		ENCI_MACV_N0, cvbs_out_reg_read(ENCI_MACV_N0));
}

/* for 576i, according to <ETSI EN 300294 V1.4.1> */

static struct wss_info_t wss_info[] = {
	/* cmd, line, start, length, mask, description */
	{
		WSS_576I_CMD_AR,
		WSS_576I_LINE,
		WSS_576I_AR_START,
		WSS_576I_AR_LENGTH,
		WSS_576I_AR_MASK,
		"wss aspect ratio option:\n"
		"0: full format 4:3\n"
		"1: box 16:9 Top\n"
		"2: box 14:9 Top\n"
		"3: full format 4:3(shoot and protect 14:9 Centre)\n"
		"4: box 14:9 Centre\n"
		"5: box > 16:9 Centre\n"
		"6: box 16:9 Centre\n"
		"7: full format 16:9(anamorphic)\n"
	},

	{
		WSS_576I_CMD_MODE,
		WSS_576I_LINE,
		WSS_576I_MODE_START,
		WSS_576I_MODE_LENGTH,
		WSS_576I_MODE_MASK,
		"wss mode option:\n"
		"0: Camera mode\n"
		"1: film mode\n"
	},

	{
		WSS_576I_CMD_CODING,
		WSS_576I_LINE,
		WSS_576I_CODING_START,
		WSS_576I_CODING_LENGTH,
		WSS_576I_CODING_MASK,
		"wss coding option:\n"
		"0: standard coding\n"
		"1: Motion Adaptive Colour Plus\n"
	},

	{
		WSS_576I_CMD_HELPER,
		WSS_576I_LINE,
		WSS_576I_HELPER_START,
		WSS_576I_HELPER_LENGTH,
		WSS_576I_HELPER_MASK,
		"wss helper option:\n"
		"0: no helper\n"
		"1: modulated helper\n"
	},

	{
		WSS_576I_CMD_TTX_SUBT,
		WSS_576I_LINE,
		WSS_576I_TTX_SUBT_START,
		WSS_576I_TTX_SUBT_LENGTH,
		WSS_576I_TTX_SUBT_MASK,
		"wss ttx-subt option:\n"
		"0: no subtitles within Teletext\n"
		"1: subtitles within Teletext\n"
	},

	{
		WSS_576I_CMD_OPEN_SUBT,
		WSS_576I_LINE,
		WSS_576I_OPEN_SUBT_START,
		WSS_576I_OPEN_SUBT_LENGTH,
		WSS_576I_OPEN_SUBT_MASK,
		"wss open subtitle option:\n"
		"0: no open subtitles\n"
		"1: subtitles out of active image area\n"
		"2: subtitles in active image area\n"
		"3: reserved\n"
	},

	{
		WSS_576I_CMD_SURROUND_SND,
		WSS_576I_LINE,
		WSS_576I_SURROUND_SND_START,
		WSS_576I_SURROUND_SND_LENGTH,
		WSS_576I_SURROUND_SND_MASK,
		"wss surround sound option:\n"
		"0: no surround sound information\n"
		"1: surround sound mode\n"
	},

	{
		WSS_576I_CMD_CGMS_A,
		WSS_576I_LINE,
		WSS_576I_CGMS_A_START,
		WSS_576I_CGMS_A_LENGTH,
		WSS_576I_CGMS_A_MASK,
		"wss cgms option:\n"
		"0: no copy right asserted or status unknown / copying not restricted\n"
		"1: copy right asserted / copying not restricted\n"
		"2: no copy right asserted or status unknown / copying restricted\n"
		"3: copy right asserted / copying restricted\n"
	},

	{
		WSS_576I_CMD_FULL,
		WSS_576I_LINE,
		WSS_576I_FULL_START,
		WSS_576I_FULL_LENGTH,
		WSS_576I_FULL_MASK,
		"please input full wss data(14 bits)!\n"
	},

	{
		WSS_576I_CMD_CC,
		WSS_576I_CC_LINE,
		WSS_576I_CC_START,
		WSS_576I_CC_LENGTH,
		WSS_576I_CC_MASK,
		"please input a param to run this!ex: echo CC data > /sys/class/tv/wss\n"
	},

	{
		WSS_576I_CMD_MVSN,
		WSS_576I_MVSN_LINE,
		WSS_576I_MVSN_START,
		WSS_576I_MVSN_LENGTH,
		WSS_576I_MVSN_MASK,
		"wss macrovision option:\n"
		"0: to do\n"
		"1: to do\n"
	},

	{
		WSS_480I_CMD_AR,
		WSS_480I_LINE,
		WSS_480I_AR_START,
		WSS_480I_AR_LENGTH,
		WSS_480I_AR_MASK,
		"wss aspect ratio option:\n"
		"0: 4:3 normal\n"
		"1: 4:3 letter box\n"
		"2: 16:9 normal\n"
		"3: not defined\n"
	},

	{
		WSS_480I_CMD_CGMS_A,
		WSS_480I_LINE,
		WSS_480I_CGMS_A_START,
		WSS_480I_CGMS_A_LENGTH,
		WSS_480I_CGMS_A_MASK,
		"wss cgms option:\n"
		"0: copying is permitted without restriction\n"
		"1: Condition not to be used\n"
		"2: one generation of copies may be made\n"
		"3: no copying is permitted\n"
	},

	{
		WSS_480I_CMD_PSP,
		WSS_480I_LINE,
		WSS_480I_PSP_START,
		WSS_480I_PSP_LENGTH,
		WSS_480I_PSP_MASK,
		"wss psp option:\n"
		"0: psp off\n"
		"1: psp on, split burst off\n"
		"2: psp on, 2-line split burst on\n"
		"3: psp on, 4-line split burst on\n"
	},

	{
		WSS_480I_CMD_PRE_RECORDED,
		WSS_480I_LINE,
		WSS_480I_PRE_RECORDED_START,
		WSS_480I_PRE_RECORDED_LENGTH,
		WSS_480I_PRE_RECORDED_MASK,
		"wss analogue source bit option:\n"
		"0: not analogue pre-recorded packaged medium\n"
		"1: analogue pre-recorded packaged medium\n"
	},

	{
		WSS_480I_CMD_CC,
		WSS_480I_CC_LINE,
		WSS_480I_CC_START,
		WSS_480I_CC_LENGTH,
		WSS_480I_CC_MASK,
		"please echo CC data to /sys/class/tv/wss\n"
	},

	{
		WSS_480I_CMD_OFF,
		WSS_480I_LINE,
		WSS_480I_FULL_START,
		WSS_480I_FULL_LENGTH,
		WSS_480I_FULL_MASK,
		"please input full wss data(12 bits)!\n"
	},

	{
		WSS_480I_CMD_MVSN,
		WSS_480I_MVSN_LINE,
		WSS_480I_MVSN_START,
		WSS_480I_MVSN_LENGTH,
		WSS_480I_MVSN_MASK,
		"wss macrovision option:\n"
		"0: to do\n"
		"1: to do\n"
	},
};

static unsigned int wss_params_mapping(unsigned int cmd, unsigned int param)
{
	unsigned int value = param;

	switch (cmd) {
	case WSS_576I_CMD_AR:
		switch (value) {
		case 1:
			value = WSS_576I_BITS_AR_BOX_169_TOP;
			break;
		case 2:
			value = WSS_576I_BITS_AR_BOX_149_TOP;
			break;
		case 3:
			value = WSS_576I_BITS_AR_FULL_43_SHOOT;
			break;
		case 4:
			value = WSS_576I_BITS_AR_BOX_149_CENTRE;
			break;
		case 5:
			value = WSS_576I_BITS_AR_BOX_OVER_169_CENTRE;
			break;
		case 6:
			value = WSS_576I_BITS_AR_BOX_169_CENTRE;
			break;
		case 7:
			value = WSS_576I_BITS_AR_FULL_169;
			break;
		case 0:
		default:
			value = WSS_576I_BITS_AR_FULL_43;
			break;
		}
		break;
	default:
		break;
	}
	return value;
}

static void wss_process_cmd(unsigned int cmd, unsigned int param)
{
	unsigned int value, mode = 576;
	unsigned int i, max = sizeof(wss_info) / sizeof(struct wss_info_t);
/* pr_info("[%s] cmd = 0x%x, param = 0x%x\n", __FUNCTION__, cmd, param); */
	if (cmd == WSS_576I_CMD_OFF) {
		wss_close_output(576);
	} else if (cmd == WSS_480I_CMD_OFF) {
		wss_close_output(480);
	} else {
		if (cmd <= WSS_576I_CMD_OFF)
			mode = 576;
		else if ((cmd >= WSS_480I_CMD_AR) && (cmd <= WSS_480I_CMD_OFF))
			mode = 480;

		for (i = 0; i < max; i++) {
			if (cmd == wss_info[i].wss_cmd) {
				value = param & wss_info[i].mask;
				value = wss_params_mapping(cmd, value);
				wss_set_output(cmd, mode, wss_info[i].wss_line,
				value, wss_info[i].start, wss_info[i].length);
			}
		}
	}
}

static void wss_process_description(unsigned int cmd)
{
	unsigned int i, max = sizeof(wss_info) / sizeof(struct wss_info_t);

	for (i = 0; i < max; i++) {
		if (cmd == wss_info[i].wss_cmd)
			pr_info("%s", wss_info[i].description);
	}
}

static void wss_show_status(unsigned int mode, char *wss_cmd)
{
	unsigned int data = cvbs_out_reg_read(ENCI_VBI_WSSDT);

	if (!wss_cmd) {
		pr_info("%s wss_cmd is null\n", __func__);
		return;
	}
	pr_info("[%s] mode = %d, wss_cmd = |%s| cvbsout_wss_flag:%#x\n",
		__func__, mode, wss_cmd, cvbsout_wss_flag);
	if (mode == MODE_576CVBS) {
		if (!strncmp(wss_cmd, "cgms", strlen("cgms"))) {
			data = (data >> WSS_576I_CGMS_A_START) &
							WSS_576I_CGMS_A_MASK;
			switch (data) {
			case 0:
				pr_info("cgms 0: no copy right asserted or status unknown / copying not restricted\n");
				break;
			case 1:
				pr_info("cgms 1: copy right asserted / copying not restricted\n");
				break;
			case 2:
				pr_info("cgms 2: no copy right asserted or status unknown / copying restricted\n");
				break;
			case 3:
				pr_info("cgms 3: copy right asserted / copying restricted\n");
				break;
			}
		} else if (!strncmp(wss_cmd, "mvsn", strlen("mvsn"))) {
			unsigned int macro_register[] = {
				ENCI_VIDEO_MODE_ADV,
				ENCI_MACV_N0,
				ENCI_MACV_MAX_AMP,
				ENCI_MACV_PULSE_LO,
				ENCI_MACV_PULSE_HI,
				ENCI_MACV_BKP_MAX,
				ENCP_MACV_EN,
			};
			int i, size;

			size = sizeof(macro_register) / sizeof(int);
			pr_info("------------------------\n");
			for (i = 0; i < size; i++) {
				pr_info("vcbus [0x%x] = 0x%x\n",
					macro_register[i],
					cvbs_out_reg_read
					(macro_register[i]));
			}
		}
	}
}

static void wss_dispatch_cmd(char *p)
{
	int argn, i, cmd_max, ret;
	char *para = NULL, *argv[2] = {NULL, NULL};
	unsigned int cmd = 0xff, param = 0;
	unsigned long param_l = 0;
	enum vmode_e mode;

	mode = get_current_vmode();
	if (mode != VMODE_CVBS) {
		pr_info("mode is not VMODE_CVBS,return\n");
		return;
	}
	pr_info("[%s]: current_vmode = 0x%x, user input = %s\n",
		__func__, mode, p);
	for (argn = 0; argn < 2; argn++) {
		para = strsep(&p, " ");
		if (!para)
			break;
		argv[argn] = para;
	}
	if (!strncmp(argv[0], "status", strlen("status"))) {
		cmd = 0xee;
	} else {
		if (get_local_cvbs_mode() == MODE_480CVBS) {
			cmd_max = sizeof(wss_480i_cmd) / sizeof(char *);
			for (i = 0; i < cmd_max; i++) {
				if (!strncmp(argv[0], wss_480i_cmd[i],
					     strlen(wss_480i_cmd[i]))) {
					cmd = WSS_480I_CMD_AR + i;
					break;
				}
			}
		} else if (get_local_cvbs_mode() == MODE_576CVBS) {
			cmd_max = sizeof(wss_576i_cmd) / sizeof(char *);
			for (i = 0; i < cmd_max; i++) {
				if (!strncmp(argv[0], wss_576i_cmd[i],
					     strlen(wss_576i_cmd[i]))) {
					cmd = WSS_576I_CMD_AR + i;
					break;
				}
			}
		} else {
			return;
		}
	}
	pr_info("[%s] wss cmd = 0x%x, argn = %d\n", __func__, cmd, argn);
	if (cmd == 0xff) {
		pr_info("[%s] invalid cmd = %s\n", __func__, argv[0]);
		return;
	} else if (cmd == 0xee) {/* inquire status */
		wss_show_status(get_local_cvbs_mode(), argv[1]);
		return;
	}
	if (cmd == WSS_576I_CMD_OFF || cmd == WSS_480I_CMD_OFF) {
		wss_process_cmd(cmd, 0);
	} else if (argn == 1) {
		wss_process_description(cmd);
	} else {
		ret = kstrtoul(argv[1], 16, &param_l);
		param = (unsigned int)param_l;
		wss_process_cmd(cmd, param);
	}
}

/*****************************************************************
 **
 **	aml_wss sysfs interface
 **
 ******************************************************************/
ssize_t aml_CVBS_attr_wss_show(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	unsigned int line = 0;
	unsigned int data = 0;
	ssize_t len = 0;
	unsigned int enable = ((cvbs_out_reg_read(ENCI_VBI_SETTING) & 0x3c)
						== 0) ? 0 : 1;
	if (get_local_cvbs_mode() == MODE_576CVBS) {
		line = cvbs_out_reg_read(ENCI_VBI_WSS_LN) + 1;
		data = cvbs_out_reg_read(ENCI_VBI_WSSDT);
	} else if (get_local_cvbs_mode() == MODE_480CVBS) {
		line = cvbs_out_reg_read(ENCI_VBI_CGMS_LN);
		line >>= 8;
		line = line + 3;

		data = cvbs_out_reg_read(ENCI_VBI_CGMSDT_L);
		data >>= wss_info[11].start;
		data &= 0xff;
	}

	len += sprintf(buf + len, "cvbsout_wss_flag:%#x\n", cvbsout_wss_flag);
	len += sprintf(buf + len, "CC info\n");
	len += sprintf(buf + len, "%#x:%#x %#x:%#x\n",
			ENCI_VBI_CCDT_EVN, cvbs_out_reg_read(ENCI_VBI_CCDT_EVN),
			ENCI_VBI_SETTING, cvbs_out_reg_read(ENCI_VBI_SETTING));
	len += sprintf(buf + len, "480i cgms-a info\n");
	len += sprintf(buf + len, "wss line:%d data %#x enable:%#x\n", line, data, enable);
	len += sprintf(buf + len, "%#x:%#x %#x:%#x %#x:%#x %#x:%#x\n",
			ENCI_VBI_CGMSDT_L, cvbs_out_reg_read(ENCI_VBI_CGMSDT_L),
			ENCI_VBI_CGMSDT_H, cvbs_out_reg_read(ENCI_VBI_CGMSDT_H),
			ENCI_VBI_CGMS_LN, cvbs_out_reg_read(ENCI_VBI_CGMS_LN),
			ENCI_VBI_SETTING, cvbs_out_reg_read(ENCI_VBI_SETTING));
	len += sprintf(buf + len, "MVSN info\n");
	len += sprintf(buf + len, "%#x:%#x %#x:%#x\n",
			ENCI_VIDEO_MODE_ADV, cvbs_out_reg_read(ENCI_VIDEO_MODE_ADV),
			ENCI_MACV_N0, cvbs_out_reg_read(ENCI_MACV_N0));
	len += sprintf(buf + len, "576i cgms-a or other info\n");
	len += sprintf(buf + len, "wss line:%d data %#x enable:%#x\n", line, data, enable);
	len += sprintf(buf + len, "%#x:%#x %#x:%#x %#x:%#x\n",
			ENCI_VBI_WSSDT, cvbs_out_reg_read(ENCI_VBI_WSSDT),
			ENCI_VBI_WSS_LN, cvbs_out_reg_read(ENCI_VBI_WSS_LN),
			ENCI_VBI_SETTING, cvbs_out_reg_read(ENCI_VBI_SETTING));
	return len;
}

ssize_t aml_CVBS_attr_wss_store(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	char *p = NULL;

	p = kstrdup(buf, GFP_KERNEL);
	wss_dispatch_cmd(p);
	kfree(p);
	return count;
}

