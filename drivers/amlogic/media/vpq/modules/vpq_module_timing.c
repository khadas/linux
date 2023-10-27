// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "vpq_module_timing.h"
#include "../vpq_printk.h"
#include "../vpq_vfm.h"

static char src_info_str[VPQ_MODULE_SIG_INFO_MAX][10];

//sequence must be case as "enum pq_source_timing_e"
char *sig_info_string[PQ_SRC_INDEX_MAX][VPQ_MODULE_SIG_INFO_MAX] = {
	{"VGA",      "--",       "--",    "--",}, //PQ_SRC_INDEX_VGA

	{"ATV",      "NTSC",     "--",    "--",}, //PQ_SRC_INDEX_ATV_NTSC
	{"ATV",      "PAL",      "--",    "--",},
	{"ATV",      "PAL_M",    "--",    "--",},
	{"ATV",      "SECAN",    "--",    "--",},
	{"ATV",      "NTSC443",  "--",    "--",},
	{"ATV",      "PAL60",    "--",    "--",},
	{"ATV",      "NTSC50",   "--",    "--",},
	{"ATV",      "PAL_N",    "--",    "--",},

	{"AV",       "NTSC",     "--",    "--",}, //PQ_SRC_INDEX_AV_NTSC
	{"AV",       "PAL",      "--",    "--",},
	{"AV",       "PAL_M",    "--",    "--",},
	{"AV",       "SECAN",    "--",    "--",},
	{"AV",       "NTSC443",  "--",    "--",},
	{"AV",       "PAL60",    "--",    "--",},
	{"AV",       "NTSC50",   "--",    "--",},
	{"AV",       "PAL_N",    "--",    "--",},

	{"SV",       "NTSC",     "--",    "--",}, //PQ_SRC_INDEX_SV_NTSC
	{"SV",       "PAL",      "--",    "--",},
	{"SV",       "PAL_M",    "--",    "--",},
	{"SV",       "SECAN",    "--",    "--",},

	{"YCBCR",    "480",      "I",     "SDR",}, //PQ_SRC_INDEX_YCBCR_480I
	{"YCBCR",    "576",      "I",     "SDR",},
	{"YCBCR",    "480",      "P",     "SDR",},
	{"YCBCR",    "576",      "P",     "SDR",},
	{"YCBCR",    "720",      "P",     "SDR",},
	{"YCBCR",    "1080",     "I",     "SDR",},
	{"YCBCR",    "1080",     "P",     "SDR",},

	{"HDMI",     "480",      "I",     "SDR",}, //PQ_SRC_INDEX_HDMI_480I
	{"HDMI",     "576",      "I",     "SDR",},
	{"HDMI",     "480",      "P",     "SDR",},
	{"HDMI",     "576",      "P",     "SDR",},
	{"HDMI",     "720",      "P",     "SDR",},
	{"HDMI",     "1080",     "I",     "SDR",},
	{"HDMI",     "1080",     "P",     "SDR",},
	{"HDMI",     "4K2K",     "I",     "SDR",},
	{"HDMI",     "4K2K",     "P",     "SDR",},
	{"HDMI",     "480",      "I",     "HDR10",}, //PQ_SRC_INDEX_HDR10_HDMI_480I
	{"HDMI",     "576",      "I",     "HDR10",},
	{"HDMI",     "480",      "P",     "HDR10",},
	{"HDMI",     "576",      "P",     "HDR10",},
	{"HDMI",     "720",      "P",     "HDR10",},
	{"HDMI",     "1080",     "I",     "HDR10",},
	{"HDMI",     "1080",     "P",     "HDR10",},
	{"HDMI",     "4K2K",     "I",     "HDR10",},
	{"HDMI",     "4K2K",     "P",     "HDR10",},
	{"HDMI",     "480",      "I",     "HLG",}, //PQ_SRC_INDEX_HLG_HDMI_480I
	{"HDMI",     "576",      "I",     "HLG",},
	{"HDMI",     "480",      "P",     "HLG",},
	{"HDMI",     "576",      "P",     "HLG",},
	{"HDMI",     "720",      "P",     "HLG",},
	{"HDMI",     "1080",     "I",     "HLG",},
	{"HDMI",     "1080",     "P",     "HLG",},
	{"HDMI",     "4K2K",     "I",     "HLG",},
	{"HDMI",     "4K2K",     "P",     "HLG",},
	{"HDMI",     "480",      "I",     "DV",}, //PQ_SRC_INDEX_DV_HDMI_480I
	{"HDMI",     "576",      "I",     "DV",},
	{"HDMI",     "480",      "P",     "DV",},
	{"HDMI",     "576",      "P",     "DV",},
	{"HDMI",     "720",      "P",     "DV",},
	{"HDMI",     "1080",     "I",     "DV",},
	{"HDMI",     "1080",     "P",     "DV",},
	{"HDMI",     "4K2K",     "I",     "DV",},
	{"HDMI",     "4K2K",     "P",     "DV",},
	{"HDMI",     "480",      "I",     "HDR10P",}, //PQ_SRC_INDEX_HDR10p_HDMI_480I
	{"HDMI",     "576",      "I",     "HDR10P",},
	{"HDMI",     "480",      "P",     "HDR10P",},
	{"HDMI",     "576",      "P",     "HDR10P",},
	{"HDMI",     "720",      "P",     "HDR10P",},
	{"HDMI",     "1080",     "I",     "HDR10P",},
	{"HDMI",     "1080",     "P",     "HDR10P",},
	{"HDMI",     "4K2K",     "I",     "HDR10P",},
	{"HDMI",     "4K2K",     "P",     "HDR10P",},

	{"DTV",      "480",      "I",     "SDR",}, //PQ_SRC_INDEX_DTV_480I
	{"DTV",      "576",      "I",     "SDR",},
	{"DTV",      "480",      "P",     "SDR",},
	{"DTV",      "576",      "P",     "SDR",},
	{"DTV",      "720",      "P",     "SDR",},
	{"DTV",      "1080",     "I",     "SDR",},
	{"DTV",      "1080",     "P",     "SDR",},
	{"DTV",      "4K2K",     "I",     "SDR",},
	{"DTV",      "4K2K",     "P",     "SDR",},
	{"DTV",      "480",      "I",     "HDR10",}, //PQ_SRC_INDEX_HDR10_DTV_480I
	{"DTV",      "576",      "I",     "HDR10",},
	{"DTV",      "480",      "P",     "HDR10",},
	{"DTV",      "576",      "P",     "HDR10",},
	{"DTV",      "720",      "P",     "HDR10",},
	{"DTV",      "1080",     "I",     "HDR10",},
	{"DTV",      "1080",     "P",     "HDR10",},
	{"DTV",      "4K2K",     "I",     "HDR10",},
	{"DTV",      "4K2K",     "P",     "HDR10",},
	{"DTV",      "480",      "I",     "HLG",}, //PQ_SRC_INDEX_HLG_DTV_480I
	{"DTV",      "576",      "I",     "HLG",},
	{"DTV",      "480",      "P",     "HLG",},
	{"DTV",      "576",      "P",     "HLG",},
	{"DTV",      "720",      "P",     "HLG",},
	{"DTV",      "1080",     "I",     "HLG",},
	{"DTV",      "1080",     "P",     "HLG",},
	{"DTV",      "4K2K",     "I",     "HLG",},
	{"DTV",      "4K2K",     "P",     "HLG",},
	{"DTV",      "480",      "I",     "DV",}, //PQ_SRC_INDEX_DV_DTV_480I
	{"DTV",      "576",      "I",     "DV",},
	{"DTV",      "480",      "P",     "DV",},
	{"DTV",      "576",      "P",     "DV",},
	{"DTV",      "720",      "P",     "DV",},
	{"DTV",      "1080",     "I",     "DV",},
	{"DTV",      "1080",     "P",     "DV",},
	{"DTV",      "4K2K",     "I",     "DV",},
	{"DTV",      "4K2K",     "P",     "DV",},
	{"DTV",      "480",      "I",     "HDR10P",}, //PQ_SRC_INDEX_HDR10p_DTV_480I
	{"DTV",      "576",      "I",     "HDR10P",},
	{"DTV",      "480",      "P",     "HDR10P",},
	{"DTV",      "576",      "P",     "HDR10P",},
	{"DTV",      "720",      "P",     "HDR10P",},
	{"DTV",      "1080",     "I",     "HDR10P",},
	{"DTV",      "1080",     "P",     "HDR10P",},
	{"DTV",      "4K2K",     "I",     "HDR10P",},
	{"DTV",      "4K2K",     "P",     "HDR10P",},

	{"MPEG",     "480",      "I",     "SDR",}, //PQ_SRC_INDEX_MPEG_480I
	{"MPEG",     "576",      "I",     "SDR",},
	{"MPEG",     "480",      "P",     "SDR",},
	{"MPEG",     "576",      "P",     "SDR",},
	{"MPEG",     "720",      "P",     "SDR",},
	{"MPEG",     "1080",     "I",     "SDR",},
	{"MPEG",     "1080",     "P",     "SDR",},
	{"MPEG",     "4K2K",     "I",     "SDR",},
	{"MPEG",     "4K2K",     "P",     "SDR",},
	{"MPEG",     "480",      "I",     "HDR10",}, //PQ_SRC_INDEX_HDR10_MPEG_480I
	{"MPEG",     "576",      "I",     "HDR10",},
	{"MPEG",     "480",      "P",     "HDR10",},
	{"MPEG",     "576",      "P",     "HDR10",},
	{"MPEG",     "720",      "P",     "HDR10",},
	{"MPEG",     "1080",     "I",     "HDR10",},
	{"MPEG",     "1080",     "P",     "HDR10",},
	{"MPEG",     "4K2K",     "I",     "HDR10",},
	{"MPEG",     "4K2K",     "P",     "HDR10",},
	{"MPEG",     "480",      "I",     "HLG",}, //PQ_SRC_INDEX_HLG_MPEG_480I
	{"MPEG",     "576",      "I",     "HLG",},
	{"MPEG",     "480",      "P",     "HLG",},
	{"MPEG",     "576",      "P",     "HLG",},
	{"MPEG",     "720",      "P",     "HLG",},
	{"MPEG",     "1080",     "I",     "HLG",},
	{"MPEG",     "1080",     "P",     "HLG",},
	{"MPEG",     "4K2K",     "I",     "HLG",},
	{"MPEG",     "4K2K",     "P",     "HLG",},
	{"MPEG",     "480",      "I",     "DV",}, //PQ_SRC_INDEX_DV_MPEG_480I
	{"MPEG",     "576",      "I",     "DV",},
	{"MPEG",     "480",      "P",     "DV",},
	{"MPEG",     "576",      "P",     "DV",},
	{"MPEG",     "720",      "P",     "DV",},
	{"MPEG",     "1080",     "I",     "DV",},
	{"MPEG",     "1080",     "P",     "DV",},
	{"MPEG",     "4K2K",     "I",     "DV",},
	{"MPEG",     "4K2K",     "P",     "DV",},
	{"MPEG",     "480",      "I",     "HDR10P",}, //PQ_SRC_INDEX_HDR10p_MPEG_480I
	{"MPEG",     "576",      "I",     "HDR10P",},
	{"MPEG",     "480",      "P",     "HDR10P",},
	{"MPEG",     "576",      "P",     "HDR10P",},
	{"MPEG",     "720",      "P",     "HDR10P",},
	{"MPEG",     "1080",     "I",     "HDR10P",},
	{"MPEG",     "1080",     "P",     "HDR10P",},
	{"MPEG",     "4K2K",     "I",     "HDR10P",},
	{"MPEG",     "4K2K",     "P",     "HDR10P",},
};

static void vpq_module_map_sig_info_string(void)
{
	unsigned int height = 0, width = 0;
	enum vpq_vfm_source_type_e src_type = VPQ_SOURCE_TYPE_OTHERS;
	enum vpq_vfm_hdr_type_e hdr_type = VPQ_HDR_TYPE_SDR;
	enum vpq_vfm_scan_mode_e scan_mode = VPQ_SCAN_MODE_NULL;

	src_type = vpq_vfm_get_source_type();
	vpq_frm_get_height_width(&height, &width);
	hdr_type = vpq_vfm_get_hdr_type();
	scan_mode = vpq_vfm_get_signal_scan_mode();

	VPQ_PR_INFO(PR_MODULE, "%s src_type:%d height:%d hdr_type:%d scan_mode:%d",
		__func__, src_type, height, hdr_type, scan_mode);

	if (src_type == VPQ_SOURCE_TYPE_OTHERS)
		strcpy(src_info_str[VPQ_MODULE_SIG_INFO_SOURCE], "MPEG");
	else if (src_type == VPQ_SOURCE_TYPE_HDMI)
		strcpy(src_info_str[VPQ_MODULE_SIG_INFO_SOURCE], "HDMI");

	if (height <= 480)
		strcpy(src_info_str[VPQ_MODULE_SIG_INFO_HEIGHT], "480");
	else if (height > 480 && height <= 576)
		strcpy(src_info_str[VPQ_MODULE_SIG_INFO_HEIGHT], "576");
	else if (height > 576 && height <= 720)
		strcpy(src_info_str[VPQ_MODULE_SIG_INFO_HEIGHT], "720");
	else if (height > 720 && height <= 1080)
		strcpy(src_info_str[VPQ_MODULE_SIG_INFO_HEIGHT], "1080");
	else if (height > 1080)
		strcpy(src_info_str[VPQ_MODULE_SIG_INFO_HEIGHT], "4K2K");

	if (scan_mode == VPQ_SCAN_MODE_PROGRESSIVE)
		strcpy(src_info_str[VPQ_MODULE_SIG_INFO_SCAN_MODE], "P");
	else if (scan_mode == VPQ_SCAN_MODE_INTERLACED)
		strcpy(src_info_str[VPQ_MODULE_SIG_INFO_SCAN_MODE], "I");

	if (hdr_type == VPQ_HDR_TYPE_SDR)
		strcpy(src_info_str[VPQ_MODULE_SIG_INFO_HDR_TYPE], "SDR");
	else if (hdr_type == VPQ_HDR_TYPE_HDR10)
		strcpy(src_info_str[VPQ_MODULE_SIG_INFO_HDR_TYPE], "HDR10");
	else if (hdr_type == VPQ_HDR_TYPE_HLG)
		strcpy(src_info_str[VPQ_MODULE_SIG_INFO_HDR_TYPE], "HLG");
	else if (hdr_type == VPQ_HDR_TYPE_DOBVI)
		strcpy(src_info_str[VPQ_MODULE_SIG_INFO_HDR_TYPE], "DV");
	else if (hdr_type == VPQ_HDR_TYPE_HDR10PLUS)
		strcpy(src_info_str[VPQ_MODULE_SIG_INFO_HDR_TYPE], "HDR10P");
	else
		strcpy(src_info_str[VPQ_MODULE_SIG_INFO_HDR_TYPE], "SDR");

	VPQ_PR_INFO(PR_MODULE, "%s src_info_str:%s %s %s %s",
		__func__,
		src_info_str[VPQ_MODULE_SIG_INFO_SOURCE],
		src_info_str[VPQ_MODULE_SIG_INFO_HEIGHT],
		src_info_str[VPQ_MODULE_SIG_INFO_SCAN_MODE],
		src_info_str[VPQ_MODULE_SIG_INFO_HDR_TYPE]);
}

static int vpq_module_compare_sig_info_string(int value)
{
	VPQ_PR_INFO(PR_MODULE, "%s value %d", __func__, value);

	if (strcmp(src_info_str[VPQ_MODULE_SIG_INFO_SOURCE],
			sig_info_string[value][VPQ_MODULE_SIG_INFO_SOURCE]) == 0 &&
		strcmp(src_info_str[VPQ_MODULE_SIG_INFO_HEIGHT],
			sig_info_string[value][VPQ_MODULE_SIG_INFO_HEIGHT]) == 0 &&
		strcmp(src_info_str[VPQ_MODULE_SIG_INFO_SCAN_MODE],
			sig_info_string[value][VPQ_MODULE_SIG_INFO_SCAN_MODE]) == 0 &&
		strcmp(src_info_str[VPQ_MODULE_SIG_INFO_HDR_TYPE],
			sig_info_string[value][VPQ_MODULE_SIG_INFO_HDR_TYPE]) == 0)
		return 1;
	else
		return 0;
}

int vpq_module_timing_table_index(enum pq_index_table_index_e module_index)
{
	int i = 0;
	int timing_index = 0;
	enum vpq_vfm_source_type_e src_type = VPQ_SOURCE_TYPE_OTHERS;
	enum vpq_vfm_source_mode_e src_mode = VPQ_SOURCE_MODE_OTHERS;

	src_type = vpq_vfm_get_source_type();

	VPQ_PR_INFO(PR_MODULE, "%s module_index:%d", __func__, module_index);

	//by src_timing index to pick up table number
	if (src_type == VPQ_SOURCE_TYPE_TUNER) {
		src_mode = vpq_vfm_get_source_mode();

		if (src_mode == VPQ_SOURCE_MODE_NTSC)
			timing_index = PQ_SRC_INDEX_ATV_NTSC;
		else if (src_mode == VPQ_SOURCE_MODE_PAL)
			timing_index = PQ_SRC_INDEX_ATV_PAL;
		else
			timing_index = PQ_SRC_INDEX_ATV_NTSC;

		/*todo: how about PAL_xx and NTSC_yy format*/
	} else if (src_type == VPQ_SOURCE_TYPE_CVBS) {
		src_mode = vpq_vfm_get_source_mode();

		if (src_mode == VPQ_SOURCE_MODE_NTSC)
			timing_index = PQ_SRC_INDEX_AV_NTSC;
		else if (src_mode == VPQ_SOURCE_MODE_PAL)
			timing_index = PQ_SRC_INDEX_AV_PAL;
		else if (src_mode == VPQ_SOURCE_MODE_SECAM)
			timing_index = PQ_SRC_INDEX_AV_SECAN;
		else
			timing_index = PQ_SRC_INDEX_AV_NTSC;

		/*todo: how about PAL_xx and NTSC_yy format*/
	} else if (src_type == VPQ_SOURCE_TYPE_OTHERS) {
		vpq_module_map_sig_info_string();

		for (i = PQ_SRC_INDEX_MPEG_480I; i < PQ_SRC_INDEX_HDR10p_MPEG_4K2KP + 1; i++) {
			if (vpq_module_compare_sig_info_string(i)) {
				VPQ_PR_INFO(PR_MODULE, "%s pq_source_timing_e:%d", __func__, i);
				timing_index = i;
				break;
			}

			if (i == PQ_SRC_INDEX_HDR10p_MPEG_4K2KP) {
				VPQ_PR_INFO(PR_MODULE, "%s sig info not match", __func__);
				timing_index = PQ_SRC_INDEX_MPEG_480I;
			}
		}
	} else if (src_type == VPQ_SOURCE_TYPE_HDMI) {
		vpq_module_map_sig_info_string();

		for (i = PQ_SRC_INDEX_HDMI_480I; i < PQ_SRC_INDEX_HDR10p_HDMI_4K2KP + 1; i++) {
			if (vpq_module_compare_sig_info_string(i)) {
				VPQ_PR_INFO(PR_MODULE, "%s pq_source_timing_e:%d", __func__, i);
				timing_index = i;
				break;
			}

			if (i == PQ_SRC_INDEX_HDR10p_HDMI_4K2KP) {
				VPQ_PR_INFO(PR_MODULE, "%s sig info not match", __func__);
				timing_index = PQ_SRC_INDEX_HDMI_480I;
			}
		}
	}

	/*todo: how about DTV*/

	return pq_table_param.pq_index_table0[timing_index][module_index];
}
