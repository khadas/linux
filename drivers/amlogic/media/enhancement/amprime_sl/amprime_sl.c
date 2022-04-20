// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/registers/regs/viu_regs.h>
#include <linux/cma.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/dma-contiguous.h>
#include <linux/amlogic/iomap.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/amprime_sl/prime_sl.h>

#include "amprime_sl.h"

/*======================================*/
#define AMPRIME_SL_NAME               "amprime_sl"
#define AMPRIME_SL_CLASS_NAME         "amprime_sl"

#define pr_sl(lvl, fmt, args...)\
	do {\
		if (prime_sl_debug & (lvl))\
			pr_info("Prime_SL: " fmt, ## args);\
	} while (0)

#define signal_color_primaries ((vf->signal_type >> 16) & 0xff)
#define signal_transfer_characteristic ((vf->signal_type >> 8) & 0xff)

#define NAL_UNIT_SEI 39
#define NAL_UNIT_SEI_SUFFIX 40
#define SEI_ITU_T_T35 4
#define PRIME_SL_T35_PROV_CODE 0x003A

static const struct hdr_prime_sl_func_s *p_funcs;
struct prime_sl_device_data_s prime_sl_meson_dev;

struct amprime_sl_dev_s {
	dev_t devt;
	struct cdev cdev;
	dev_t devno;
	struct device *dev;
	struct class *clsp;
};

static struct amprime_sl_dev_s amprime_sl_dev;
static struct prime_t prime_sl_setting;

static u32 prime_sl_enable = ENABLE;
module_param(prime_sl_enable, uint, 0664);
MODULE_PARM_DESC(prime_sl_enable, "\n prime_sl_enable\n");

static u32 prime_sl_running;
module_param(prime_sl_running, uint, 0444);
MODULE_PARM_DESC(prime_sl_running, "\n prime_sl_running\n");

static u32 prime_sl_probe;
module_param(prime_sl_probe, uint, 0664);
MODULE_PARM_DESC(prime_sl_probe, "\n prime_sl_probe\n");

static u32 prime_sl_display_brightness = 150;
module_param(prime_sl_display_brightness, uint, 0664);
MODULE_PARM_DESC(prime_sl_display_brightness, "\n prime_sl_display_brightness\n");

static u32 prime_sl_output_mode = PRIME_SL_DISPLAY_OETF_PQ;
module_param(prime_sl_output_mode, uint, 0664);
MODULE_PARM_DESC(prime_sl_output_mode, "\n prime_sl_output_mode\n");

static u32 prime_sl_debug;
module_param(prime_sl_debug, uint, 0664);
MODULE_PARM_DESC(prime_sl_debug, "\n prime_sl_debug\n");

static u32 prime_sl_mode;
module_param(prime_sl_mode, uint, 0664);
MODULE_PARM_DESC(prime_sl_mode, "\n prime_sl_mode\n");

static const int shadow_gain[201] = {
	255, 255, 255, 253, 227, 208, 194, 182,
	172, 164, 157, 151, 145, 140, 136, 131,
	128, 124, 121, 118, 115, 112, 110, 107,
	105, 103, 101, 99,  97,  95,  94,  92,
	91,  89,  88,  86,  85,  84,  82,  81,
	80,  79,  78,  77,  76,  75,  74,  73,
	72,  71,  70,  69,  68,  67,  67,  66,
	65,  64,  64,  63,  62,  61,  61,  60,
	59,  59,  58,  58,  57,  56,  56,  55,
	55,  54,  54,  53,  53,  52,  52,  51,
	51,  50,  50,  49,  49,  48,  48,  47,
	47,  46,  46,  46,  45,  45,  44,  44,
	44,  43,  43,  42,  42,  42,  41,  41,
	41,  40,  40,  40,  39,  39,  39,  38,
	38,  38,  37,  37,  37,  36,  36,  36,
	36,  35,  35,  35,  34,  34,  34,  34,
	33,  33,  33,  32,  32,  32,  32,  31,
	31,  31,  31,  30,  30,  30,  30,  29,
	29,  29,  29,  29,  28,  28,  28,  28,
	27,	 27,  27,  27,  27,  26,  26,  26,
	26,	 25,  25,  25,  25,  25,  24,  24,
	24,	 24,  24,  23,  23,  23,  23,  23,
	23,	 22,  22,  22,  22,  22,  21,  21,
	21,	 21,  21,  21,  20,  20,  20,  20,
	20,	 20,  19,  19,  19,  19,  19,  19,
	18,
};

static void dbg_setting(struct prime_sl_t *prime_sl);
static void dbg_config(struct prime_cfg_t *cfg);
static void dbg_metadata(struct sl_hdr_metadata *pmetadata);

bool is_meson_g12(void)
{
	if (prime_sl_meson_dev.cpu_id == _CPU_MAJOR_ID_G12)
		return true;
	else
		return false;
}

bool is_meson_tl1(void)
{
	if (prime_sl_meson_dev.cpu_id == _CPU_MAJOR_ID_TL1)
		return true;
	else
		return false;
}

bool is_meson_tm2(void)
{
	if (prime_sl_meson_dev.cpu_id == _CPU_MAJOR_ID_TM2)
		return true;
	else
		return false;
}

bool is_meson_sc2(void)
{
	if (prime_sl_meson_dev.cpu_id == _CPU_MAJOR_ID_SC2)
		return true;
	else
		return false;
}

bool is_prime_sl_enable(void)
{
	return prime_sl_enable &&
		((prime_sl_probe && p_funcs) || prime_sl_mode);
}
EXPORT_SYMBOL(is_prime_sl_enable);

bool is_prime_sl_on(void)
{
	return prime_sl_enable &&
		prime_sl_running &&
		((prime_sl_probe && p_funcs) || prime_sl_mode);
}
EXPORT_SYMBOL(is_prime_sl_on);

#define SEI_VERSION 1

/* just debug now */
#ifdef RAW_META
static int prime_sl_parser_sei_and_meta(char *p, unsigned int size)
{
	unsigned int i;
	struct sl_hdr_metadata *metadata = &prime_sl_setting.prime_metadata;

	if (size != sizeof(struct sl_hdr_metadata)) {
		pr_info("error metadata size\n");
		return 0;
	}

	metadata->part_id = parse_para(p);
	metadata->major_spec_version_id = parse_para(p);
	metadata->minor_spec_version_id = parse_para(p);
	metadata->payload_mode = parse_para(p);
	metadata->hdr_pic_colour_space = parse_para(p);
	metadata->hdr_display_colour_space = parse_para(p);
	metadata->hdr_display_max_luminance = parse_para(p);
	metadata->hdr_display_min_luminance = parse_para(p);
	metadata->sdr_pic_colour_space = parse_para(p);
	metadata->sdr_display_colour_space = parse_para(p);
	metadata->sdr_display_max_luminance = parse_para(p);
	metadata->sdr_display_min_luminance = parse_para(p);
	for (i = 0; i < 4; i++)
		metadata->matrix_coefficient[i] = parse_para(p);
	for (i = 0; i < 2; i++)
		metadata->chroma_to_luma_injection[i] = parse_para(p);
	for (i = 0; i < 3; i++)
		metadata->kcoefficient[i] = parse_para(p);
	if (!metadata->payload_mode) {
		metadata->u.variables.tm_input_signal_blacklevel_offset = parse_para(p);
		metadata->u.variables.tm_input_signal_whitelevel_offset = parse_para(p);
		metadata->u.variables.shadow_gain = parse_para(p);
		metadata->u.variables.highlight_gain = parse_para(p);
		metadata->u.variables.mid_tone_width_adj_factor = parse_para(p);
		metadata->u.variables.tm_output_finetuning_num_val = parse_para(p);
		for (i = 0; i < metadata->u.variables.tm_output_finetuning_num_val; i++) {
			metadata->u.variables.tm_output_finetuning_x[i] = parse_para(p);
			metadata->u.variables.tm_output_finetuning_y[i] = parse_para(p);
		}
		metadata->u.variables.saturation_gain_num_val = parse_para(p);
		for (i = 0; i < metadata->u.variables.saturation_gain_num_val; i++) {
			metadata->u.variables.saturation_gain_x[i] = parse_para(p);
			metadata->u.variables.saturation_gain_y[i] = parse_para(p);
		}
	} else {
		metadata->u.tables.luminance_mapping_num_val = parse_para(p);
		for (i = 0; i < metadata->u.tables.luminance_mapping_num_val; i++) {
			metadata->u.tables.luminance_mapping_x[i] = parse_para(p);
			metadata->u.tables.luminance_mapping_y[i] = parse_para(p);
		}
		metadata->u.tables.colour_correction_num_val = parse_para(p);
		for (i = 0; i < metadata->u.tables.colour_correction_num_val; i++) {
			metadata->u.tables.colour_correction_x[i] = parse_para(p);
			metadata->u.tables.colour_correction_y[i] = parse_para(p);
		}
	}
	return 0;
}
#endif

struct sei_parser_s {
	u8 *buffer_ptr;
	u32 buffer_size;
	u32 readbytes;
	u32 readbits;
	u32 zero_num;
	u32 cur_bytes;
	u32 next_bytes;
	bool err_flag;
};

static struct sei_parser_s prime_sl_parser;

static int loadbyte(struct sei_parser_s *p, u8 *byte)
{
	*byte = 0;
	if (p->readbytes >= p->buffer_size) {
		p->err_flag = true;
		return -1;
	}

	*byte = p->buffer_ptr[p->readbytes];
	p->readbytes++;
#ifdef TMP_DISABLE
	if (*byte == 0x03 || *byte == 0x00) {
		if (p->zero_num == 2) {
			if (*byte == 0x03) {
				if (p->readbytes >= p->buffer_size) {
					p->err_flag = true;
					return -1;
				}
				*byte = p->buffer_ptr[p->readbytes];
				p->readbytes++;
			}
			p->zero_num = 0;
			if (*byte == 0)
				p->zero_num++;
		} else {
			if (*byte == 0x00)
				p->zero_num++;
			else
				p->zero_num = 0;
		}
	} else {
		p->zero_num = 0;
	}
#endif
	return 0;
}

static int loadbuffer(struct sei_parser_s *p, u32 *bytes)
{
	u8 byte;

	*bytes = 0;

	if (loadbyte(p, &byte))
		return -1;
	*bytes = byte;
	*bytes <<= 8;

	if (loadbyte(p, &byte))
		return -2;
	*bytes |= byte;
	*bytes <<= 8;

	if (loadbyte(p, &byte))
		return -3;
	*bytes |= byte;
	*bytes <<= 8;

	if (loadbyte(p, &byte))
		return -4;
	*bytes |= byte;
	pr_sl(0x40, "%s bytes:%x, readbytes:%d\n",
		__func__, *bytes, p->readbytes);
	return 0;
}

static int readbits(struct sei_parser_s *p, int bits)
{
	int value = 0;

	pr_sl(0x40, "%s e bits:%d, cur:%x, next:%x, rdbytes:%d, rdbits:%d\n",
		__func__, bits, p->cur_bytes, p->next_bytes,
		p->readbytes, p->readbits);

	p->readbits += bits;

	if (p->readbits <= 32) {
		value = p->cur_bytes >> (32 - bits);
		p->cur_bytes <<= bits;
	} else {
		p->readbits -= 32;
		value = (p->cur_bytes >> (32 - bits)) |
			(p->next_bytes >> (32 - p->readbits));
		p->cur_bytes = p->next_bytes;
		loadbuffer(p, &p->next_bytes);
		p->cur_bytes <<= p->readbits;
	}

	pr_sl(0x40, "%s x value:%x, cur:%x, next:%x, rdbytes:%d, rdbits:%d\n",
		__func__, value, p->cur_bytes, p->next_bytes,
		p->readbytes, p->readbits);
	return value;
}

#define readU(p, bits) readbits(p, bits)

#ifdef TMP_DISABLE
static u32 read_ue(struct sei_parser_s *p)
{
	u32 value, bit = 0, length = 0;

	bit = readbits(p, 1);

	if (!bit) {
		while (!bit) {
			bit = readbits(p, 1);
			length++;
		}
		value = readbits(p, length);
		value += (1 << length) - 1;
		return value;
	}
	return 0;
}
#endif

static void init_sei_parser(struct sei_parser_s *p, u8 *raw_meta, u32 meta_size)
{
	p->buffer_ptr = raw_meta;
	p->buffer_size = meta_size;
	p->readbits = 0;
	p->readbytes = 0;
	p->zero_num = 0;
	p->cur_bytes = 0;
	p->next_bytes = 0;
	p->err_flag = false;
	loadbuffer(p, &p->cur_bytes);
	loadbuffer(p, &p->next_bytes);
}

static bool is_parser_success(struct sei_parser_s *p)
{
	return p->err_flag ? false : true;
}

#define VER_1_4_0

static void fill_metadata_recovery(struct sl_hdr_metadata *metadata,
		int target_display_luminance,
		int colour_primaries,
		int transfer_characteristics)
{
	u8 lhdr_idx = target_display_luminance / 50;
	int s_gain = (target_display_luminance == 100) ? 115 : shadow_gain[lhdr_idx];

	if (transfer_characteristics == 12 ||
	    transfer_characteristics == 16) {
		metadata->part_id = 2;
		metadata->chroma_to_luma_injection[1] = 0;
		metadata->u.variables.saturation_gain_num_val = 0;
	} else {
		metadata->part_id = 1;
		metadata->chroma_to_luma_injection[1] =
			(colour_primaries == 1) ? 0 : 1638;
		metadata->u.variables.saturation_gain_num_val = 1;
		metadata->u.variables.saturation_gain_x[0] = 0;
		metadata->u.variables.saturation_gain_y[0] =
			(colour_primaries == 1) ? 115 : 118;
	}

	metadata->payload_mode = 0;

	if (colour_primaries == 1) {
		metadata->hdr_pic_colour_space = 0;
		metadata->hdr_display_colour_space = 0;
		metadata->matrix_coefficient[0] = 915;
		metadata->matrix_coefficient[1] = 464;
		metadata->matrix_coefficient[2] = 392;
		metadata->matrix_coefficient[3] = 987;
	} else {
		metadata->hdr_pic_colour_space = 1;
		metadata->hdr_display_colour_space = 2;
		metadata->matrix_coefficient[0] = 889;
		metadata->matrix_coefficient[1] = 470;
		metadata->matrix_coefficient[2] = 366;
		metadata->matrix_coefficient[3] = 994;
	}

	metadata->hdr_display_max_luminance =
		(target_display_luminance == 100) ? 1000 : target_display_luminance;
	metadata->hdr_display_min_luminance = 0;
	metadata->sdr_pic_colour_space = metadata->hdr_pic_colour_space;
	metadata->sdr_display_colour_space = metadata->hdr_display_colour_space;
	metadata->sdr_display_max_luminance = 100;
	metadata->sdr_display_min_luminance = 0;
	metadata->kcoefficient[0] = 0;
	metadata->kcoefficient[1] = 0;
	metadata->kcoefficient[2] = 0;
	metadata->chroma_to_luma_injection[0] = 0;
	metadata->u.variables.tm_input_signal_blacklevel_offset = 0;
	metadata->u.variables.tm_input_signal_whitelevel_offset = 0;
	metadata->u.variables.shadow_gain = s_gain;
	metadata->u.variables.highlight_gain = 255;
	metadata->u.variables.mid_tone_width_adj_factor = 64;
	metadata->u.variables.tm_output_finetuning_num_val = 0;
}

/* return 0: not find valid meta; 1: find meta and parser success; < 0: parser fail */
static int parser_metadata_from_sei(struct sei_parser_s *p, bool hevc)
{
	int i, m;
	struct sl_hdr_metadata *metadata = &prime_sl_setting.prime_metadata;

	memset(metadata, 0, sizeof(struct sl_hdr_metadata));

	/* itu_t_t35_country_code */
	m = readU(p, 8);
	if (m != 0xB5) {
		pr_sl(2, "Invalid itu_t_t35_country_code:%x\n", m);
		return is_parser_success(p) ? 0 : -1;
	}
	pr_sl(0x20, "itu_t_t35_country_code:%x\n", m);

	/* terminal_provider_code */
	m = readU(p, 16);
	if (m != 0x3A) {
		pr_sl(2, "Invalid terminal_provider_code:%x\n", m);
		return is_parser_success(p) ? 0 : -2;
	}
	pr_sl(0x20, "terminal_provider_code:%x\n", m);

	/* terminal_provider_oriented_code_message_idc */
	m = readU(p, 8);
	if (m != 0 && m != 1) {
		pr_sl(1, "Invalid terminal_provider_oriented_code_message_idc:%d\n", m);
		return -3;
	}
	pr_sl(0x20, "terminal_provider_oriented_code_message_idc:%d\n", m);

	/* sl_hdr_mode_value_minus1 */
	metadata->part_id = readU(p, 4) + 1;
	if (metadata->part_id != 1 && metadata->part_id != 2) {
		pr_sl(1, "Invalid sl_hdr_mode_value_minus1(part_id):%d\n",
			metadata->part_id);
		return -4;
	}
	pr_sl(0x20, "sl_hdr_mode_value_minus1(part_id):%d\n", metadata->part_id);

	/* sl_hdr_spec_major_version_idc */
	metadata->major_spec_version_id = readU(p, 4);
	if (metadata->major_spec_version_id > SEI_VERSION) {
		pr_sl(1, "Unsupported sl_hdr_spec_major_version_idc:%d\n",
			metadata->major_spec_version_id);
		return -5;
	}
	pr_sl(0x20, "sl_hdr_spec_major_version_idc:%d\n",
		metadata->major_spec_version_id);

	/* sl_hdr_spec_minor_version_idc */
	metadata->minor_spec_version_id = readU(p, 7);
	pr_sl(0x20, "metadata->minor_spec_version_id:%d\n",
		metadata->minor_spec_version_id);

	/* sl_hdr_cancel_flag */
	m = readU(p, 1);
	if (m) {
		pr_sl(1, "Unsupported sl_hdr_cancel_flag:%d\n", m);
		return -6;
	}
	pr_sl(0x20, "sl_hdr_cancel_flag:%d\n", m);

#ifdef VER_1_4_0
	/* sl_hdr_persistence_flag */
	m = readU(p, 1);
	pr_sl(0x20, "sl_hdr_persistence_flag:%d\n", m);
	/* coded_picture_info_present_flag */
	m = readU(p, 1);
	pr_sl(0x20, "coded_picture_info_present_flag:%d\n", m);
	/* target_picture_info_present_flag */
	m = readU(p, 1);
	pr_sl(0x20, "target_picture_info_present_flag:%d\n", m);
	/* src_mdcv_info_present_flag */
	m = readU(p, 1);
	pr_sl(0x20, "src_mdcv_info_present_flag:%d\n", m);
	/* sl_hdr_extension_present_flag */
	m = readU(p, 1);
	pr_sl(0x20, "sl_hdr_extension_present_flag:%d\n", m);
#else
	if (hevc) {
		/* sl_hdr_persistence_flag */
		m = readU(p, 1);
		if (m) {
			pr_sl(1, "Unsupported sl_hdr_persistence_flag:%d\n", m);
			return -7;
		}
	} else {
		/* sl_hdr_repetition_period */
		m = readU(p, 17);
		if (m) {
			pr_sl(1, "Unsupported sl_hdr_repetition_period:%d\n", m);
			return -7;
		}
	}

	/* TODO: check sei spec */
	/* original_picture_info_present_flag */
	m = readU(p, 1);
	if (m) {
		/* original_picture_primaries */
		readU(p, 8);
		/* original_picture_max_luminance */
		readU(p, 16);
		/* original_picture_min_luminance */
		readU(p, 16);
	}

	/* target_picture_info_present_flag */
	m = readU(p, 1);
	if (!m) {
		pr_sl(1, "Unsupported target_picture_info_present_flag:%d\n", m);
		return -8;
	}

	/* src_mdcv_info_present_flag */
	m = readU(p, 1);
	if (!m) {
		pr_sl(1, "Unsupported src_mdcv_info_present_flag:%d\n", m);
		return -9;
	}

	/* sl_hdr_extension_present_flag */
	readU(p, 1);
#endif

	/* sl_hdr_payload_mode */
	metadata->payload_mode = readU(p, 3);
	if (metadata->payload_mode != 0 && metadata->payload_mode != 1) {
		pr_sl(1, "Invalid sl_hdr_payload_mode:%d\n",
			metadata->payload_mode);
		return -10;
	}
	pr_sl(0x20, "sl_hdr_payload_mode:%d\n", metadata->payload_mode);

	/* target_picture_primaries */
	m = readU(p, 8);
	if (m != 1 && m != 9) {
		pr_sl(1, "Invalid target_picture_primaries:%d\n", m);
		return -11;
	}
	pr_sl(0x20, "target_picture_primaries:%d\n", m);
	metadata->sdr_pic_colour_space = m == 1 ? 0 : 1;

	/* target_picture_max_luminance */
	metadata->sdr_display_max_luminance = readU(p, 16);
	if (metadata->sdr_display_max_luminance != 100) {
		pr_sl(1, "Invalid target_picture_max_luminance:%d\n",
			metadata->sdr_display_max_luminance);
		return -12;
	}
	pr_sl(0x20, "target_picture_max_luminance:%d\n",
		metadata->sdr_display_max_luminance);

	/* target_picture_min_luminance */
	metadata->sdr_display_min_luminance = readU(p, 16);
	if (metadata->sdr_display_min_luminance) {
		pr_sl(1, "Invalid target_picture_max_luminance:%d\n",
			metadata->sdr_display_min_luminance);
		return -13;
	}
	pr_sl(0x20, "target_picture_max_luminance:%d\n",
		metadata->sdr_display_min_luminance);

	/* src_mdcv_primaries_x[0] */
	m = readU(p, 16);
	pr_sl(0x20, "src_mdcv_primaries_x[0]:%d\n", m);
	if (m == 15000) {
		/* 0.300 * 50000 */
		metadata->hdr_display_colour_space = 0;
	} else if (m == 8500) {
		/* 0.170 * 50000 */
		metadata->hdr_display_colour_space = 1;
	} else if (m == 13250) {
		/* 0.265 * 50000 */
		metadata->hdr_display_colour_space = 2;
	} else {
		pr_sl(1, "Invalid src_mdcv_primaries_x[0]:%d\n", m);
		return -14;
	}

	metadata->hdr_pic_colour_space = (metadata->hdr_display_colour_space ||
		 metadata->sdr_pic_colour_space) ? 1 : 0;
	if (metadata->sdr_pic_colour_space == metadata->hdr_pic_colour_space)
		metadata->sdr_display_colour_space = metadata->hdr_display_colour_space;
	else
		metadata->sdr_display_colour_space = 0;

	/* src_mdcv_primaries_y[0] */
	m = readU(p, 16);
	pr_sl(0x20, "src_mdcv_primaries_y[0]:%d\n", m);
	/* src_mdcv_primaries_x[1] */
	m = readU(p, 16);
	pr_sl(0x20, "src_mdcv_primaries_x[1]:%d\n", m);
	/* src_mdcv_primaries_y[1] */
	m = readU(p, 16);
	pr_sl(0x20, "src_mdcv_primaries_y[1]:%d\n", m);
	/* src_mdcv_primaries_x[2] */
	m = readU(p, 16);
	pr_sl(0x20, "src_mdcv_primaries_x[2]:%d\n", m);
	/* src_mdcv_primaries_y[2] */
	m = readU(p, 16);
	pr_sl(0x20, "src_mdcv_primaries_y[2]:%d\n", m);
	/* src_mdcv_ref_white_x */
	m = readU(p, 16);
	pr_sl(0x20, "src_mdcv_ref_white_x:%d\n", m);
	/* src_mdcv_ref_white_y */
	m = readU(p, 16);
	pr_sl(0x20, "src_mdcv_ref_white_y:%d\n", m);

	/* src_mdcv_max_mastering_luminance */
	m = readU(p, 16);
	pr_sl(0x20, "src_mdcv_max_mastering_luminance:%d\n", m);
	metadata->hdr_display_max_luminance = 50 * ((m + 25) / 50);
	if (metadata->hdr_display_max_luminance > 10000)
		metadata->hdr_display_max_luminance = 10000;

	/* src_mdcv_min_mastering_luminance */
	m = readU(p, 16);
	pr_sl(0x20, "src_mdcv_min_mastering_luminance:%d\n", m);
	metadata->hdr_display_min_luminance = (m + 5000) / 10000;

	/* matrix_coefficient_value[i] */
	for (i = 0; i < 4; i++) {
		metadata->matrix_coefficient[i] = readU(p, 16);
		if (metadata->matrix_coefficient[i] > 1023) {
			pr_sl(1, "Invalid matrix_coefficient[%d]:%d\n",
				i, metadata->matrix_coefficient[i]);
			return -15;
		}
		pr_sl(0x20, "matrix_coefficient[%d]:%d\n",
			i, metadata->matrix_coefficient[i]);
	}

	/* chroma_to_luma_injection[i] */
	for (i = 0; i < 2; i++) {
		metadata->chroma_to_luma_injection[i] = readU(p, 16);
		if (metadata->chroma_to_luma_injection[i] > 8191) {
			pr_sl(1, "Invalid chroma_to_luma_injection[%d]:%d\n",
				i, metadata->chroma_to_luma_injection[i]);
			return -16;
		}
		pr_sl(0x20, "chroma_to_luma_injection[%d]:%d\n",
			i, metadata->chroma_to_luma_injection[i]);
	}

	/* k_coefficient_value[i] */
	for (i = 0; i < 3; i++) {
		metadata->kcoefficient[i] = readU(p, 8);
		pr_sl(0x20, "kcoefficient[%d]:%d\n",
			i, metadata->kcoefficient[i]);
	}

	if (metadata->kcoefficient[0] > 63 || metadata->kcoefficient[1] > 127) {
		pr_sl(1, "Invalid kCoefficient[i]:%d %d %d\n",
			metadata->kcoefficient[0],
			metadata->kcoefficient[1],
			metadata->kcoefficient[2]);
		return -17;
	}

	if (!metadata->payload_mode) {
		/* tone_mapping_input_signal_black_level_offset */
		metadata->u.variables.tm_input_signal_blacklevel_offset = readU(p, 8);
		pr_sl(0x20, "tone_mapping_input_signal_black_level_offset:%d\n",
			metadata->u.variables.tm_input_signal_blacklevel_offset);

		/* tone_mapping_input_signal_white_level_offset */
		metadata->u.variables.tm_input_signal_whitelevel_offset = readU(p, 8);
		pr_sl(0x20, "tone_mapping_input_signal_white_level_offset:%d\n",
			metadata->u.variables.tm_input_signal_whitelevel_offset);

		/* shadow_gain_control */
		metadata->u.variables.shadow_gain = readU(p, 8);
		pr_sl(0x20, "shadow_gain_control:%d\n",
			metadata->u.variables.shadow_gain);

		/* highlight_gain_control */
		metadata->u.variables.highlight_gain = readU(p, 8);
		pr_sl(0x20, "highlight_gain_control:%d\n",
			metadata->u.variables.highlight_gain);

		/* mid_tone_width_adjustment_factor */
		metadata->u.variables.mid_tone_width_adj_factor = readU(p, 8);
		pr_sl(0x20, "mid_tone_width_adjustment_factor:%d\n",
			metadata->u.variables.mid_tone_width_adj_factor);

		/* tone_mapping_output_fine_tuning_num_val */
		metadata->u.variables.tm_output_finetuning_num_val = readU(p, 4);
		if (metadata->u.variables.tm_output_finetuning_num_val > 10) {
			pr_sl(1, "Invalid tone_mapping_output_fine_tuning_num_val:%d\n",
				metadata->u.variables.tm_output_finetuning_num_val);
			return -18;
		}
		pr_sl(0x20, "tone_mapping_output_fine_tuning_num_val:%d\n",
			metadata->u.variables.tm_output_finetuning_num_val);

		/* saturation_gain_num_val */
		metadata->u.variables.saturation_gain_num_val = readU(p, 4);
		if (metadata->u.variables.saturation_gain_num_val > 6) {
			pr_sl(1, "Invalid saturation_gain_num_val:%d\n",
				metadata->u.variables.saturation_gain_num_val);
			return -19;
		}
		pr_sl(0x20, "saturation_gain_num_val:%d\n",
			metadata->u.variables.saturation_gain_num_val);

		for (i = 0; i < metadata->u.variables.tm_output_finetuning_num_val; i++) {
			/* tone_mapping_output_fine_tuning_x[i] */
			metadata->u.variables.tm_output_finetuning_x[i] = readU(p, 8);
			/* tone_mapping_output_fine_tuning_y[i] */
			metadata->u.variables.tm_output_finetuning_y[i] = readU(p, 8);
			pr_sl(0x20, "tone_mapping_output_fine_tuning x[%d]:%d, y[%d]:%d\n",
				i, metadata->u.variables.tm_output_finetuning_x[i],
				i, metadata->u.variables.tm_output_finetuning_y[i]);
		}
		for (i = 0; i < metadata->u.variables.saturation_gain_num_val; i++) {
			/* saturation_gain_x[i] */
			metadata->u.variables.saturation_gain_x[i] = readU(p, 8);
			/* saturation_gain_y[i] */
			metadata->u.variables.saturation_gain_y[i] = readU(p, 8);
			pr_sl(0x20, "saturation_gain x[%d]:%d, y[%d]:%d\n",
				i, metadata->u.variables.saturation_gain_x[i],
				i, metadata->u.variables.saturation_gain_y[i]);
		}
	} else {
		/* lm_uniform_sampling_flag */
		m = readU(p, 1);
		pr_sl(0x20, "lm_uniform_sampling_flag:%d\n", m);

		/* luminance_mapping_num_val */
		metadata->u.tables.luminance_mapping_num_val = readU(p, 7);
		if (metadata->u.tables.luminance_mapping_num_val != 65) {
			pr_sl(1, "Unsupported luminance_mapping_num_val:%d\n",
				metadata->u.tables.luminance_mapping_num_val);
			return -20;
		}
		pr_sl(0x20, "luminance_mapping_num_val:%d\n",
			metadata->u.tables.luminance_mapping_num_val);
		for (i = 0; i < metadata->u.tables.luminance_mapping_num_val; i++) {
			if (!m) {
				/* luminance_mapping_x[i] */
				metadata->u.tables.luminance_mapping_x[i] = readU(p, 16);
				if (metadata->u.tables.luminance_mapping_x[i] > 8192) {
					pr_sl(1, "Invalid luminance_mapping_x[%d]:%d\n",
						i, metadata->u.tables.luminance_mapping_x[i]);
					return -21;
				}
			} else {
				metadata->u.tables.luminance_mapping_x[i] = i << 7;
			}
			/* luminance_mapping_y[i] */
			metadata->u.tables.luminance_mapping_y[i] = readU(p, 16);
			if (metadata->u.tables.luminance_mapping_y[i] > 8191) {
				pr_sl(1, "Invalid luminance_mapping_y[%d]:%d\n",
					i, metadata->u.tables.luminance_mapping_x[i]);
				return -22;
			}
			pr_sl(0x20, "luminance_mapping x[%d]:%d, y[%d]:%d\n",
				i, metadata->u.tables.luminance_mapping_x[i],
				i, metadata->u.tables.luminance_mapping_y[i]);
		}
		/* cc_uniform_sampling_flag */
		m = readU(p, 1);
		pr_sl(0x20, "cc_uniform_sampling_flag:%d\n", m);

		/* colour_correction_num_val */
		metadata->u.tables.colour_correction_num_val = readU(p, 7);
		if (metadata->u.tables.colour_correction_num_val != 65) {
			pr_sl(1, "Unsupported colour_correction_num_val\n");
			return -23;
		}
		pr_sl(0x20, "colour_correction_num_val:%d\n",
			metadata->u.tables.colour_correction_num_val);

		for (i = 0; i < metadata->u.tables.colour_correction_num_val; i++) {
			if (!m) {
				/* colour_correction_x[i] */
				metadata->u.tables.colour_correction_x[i] = readU(p, 16);
				if (metadata->u.tables.colour_correction_x[i] > 2048) {
					pr_sl(1, "Invalid colour_correction_x[%d]:%d\n",
						i, metadata->u.tables.colour_correction_x[i]);
					return -24;
				}
			} else {
				metadata->u.tables.colour_correction_x[i] = i << 5;
			}
			/* colour_correction_y[i] */
			metadata->u.tables.colour_correction_y[i] = readU(p, 16);
			if (metadata->u.tables.colour_correction_y[i] > 2047) {
				pr_sl(1, "Invalid colour_correction_y[%d]:%d\n",
					i, metadata->u.tables.colour_correction_x[i]);
				return -25;
			}
			pr_sl(0x20, "colour_correction x[%d]:%d, y[%d]:%d\n",
				i, metadata->u.tables.colour_correction_x[i],
				i, metadata->u.tables.colour_correction_y[i]);
		}
	}

	if (metadata->sdr_pic_colour_space < metadata->hdr_pic_colour_space) {
		if (metadata->part_id == 1) {
			metadata->hdr_pic_colour_space = 0;
			metadata->hdr_display_colour_space = 0;
		}

		if (metadata->part_id == 2)
			metadata->sdr_pic_colour_space = 1;
	}

	if (!is_parser_success(p)) {
		pr_sl(1, "parser is not completed: %px, size:%d, used:%d\n",
			p->buffer_ptr, p->buffer_size, p->readbytes);
		return -26;
	}
	return 1;
}

/* return 0: not find valid sei; 1: find sei and parser success; < 0: parser fail */
static int parser_prime_sei(struct sei_parser_s *parser,
		char *sei_buf, u32 size)
{
	char *p = sei_buf;
	char *p_sei;
	u16 header;
	u16 nal_unit_type;
	u16 payload_type, payload_size;
	int ret = 0;

	if (size < 2)
		return ret;
	header = *p++;
	header <<= 8;
	header += *p++;
	nal_unit_type = header >> 9;
	if (nal_unit_type != NAL_UNIT_SEI &&
	    nal_unit_type != NAL_UNIT_SEI_SUFFIX)
		return 0;

	pr_sl(0x40, "%s enter: nal_unit_type:%x, sei_buf:%px, p:%px, size:%d\n",
		__func__, nal_unit_type, sei_buf, p, size);

	while (p + 4 <= sei_buf + size) {
		payload_type = *p++;
		payload_size = *p++;
		if (payload_size == 0xff)
			payload_size += *p++;
		if (p + payload_size <= sei_buf + size) {
			switch (payload_type) {
			case SEI_ITU_T_T35:
				p_sei = p;
				pr_sl(0x20, "%s get: sei:%px, size:%d; p:%px, p_size:%d\n",
					__func__, sei_buf, (u32)(size - (p_sei - sei_buf)),
					p, payload_size);
				init_sei_parser(parser, p_sei, size - (p_sei - sei_buf));
				ret = parser_metadata_from_sei(parser, true);
				break;
			default:
				break;
			}
		}
		p += payload_size;
	}
	pr_sl(0x40, "%s exit: sei_buf:%px, p:%px, size:%d\n",
		__func__, sei_buf, p, size);
	return ret;
}

static int prime_sl_parser_metadata(struct vframe_s *vf)
{
	struct provider_aux_req_s req;
	struct prime_cfg_t *cfg = &prime_sl_setting.cfg;
	struct sei_parser_s *parser = &prime_sl_parser;
	unsigned int size = 0, type = 0;
	int ret = -1, i, j, len;
	char *p, *meta_buf;
	bool prime_sl_video = false;

	if (!vf || vf->source_type != VFRAME_SOURCE_TYPE_OTHERS)
		return ret;

	cfg->width = (vf->type & VIDTYPE_COMPRESS) ?
		vf->compWidth : vf->width;
	cfg->height = (vf->type & VIDTYPE_COMPRESS) ?
		vf->compHeight : vf->height;
	cfg->bit_depth = vf->bitdepth;
	cfg->yuv_range = (vf->signal_type >> 25) & 0x1;
	cfg->display_brightness = prime_sl_display_brightness;
	if (prime_sl_output_mode < PRIME_SL_DISPLAY_BYPASS)
		cfg->display_oetf = prime_sl_output_mode;
	else
		cfg->display_oetf = 1;

	req.vf = vf;
	req.bot_flag = 0;
	req.aux_buf = NULL;
	req.aux_size = 0;
	req.dv_enhance_exist = 0;
	req.low_latency = 0;
	if (get_vframe_src_fmt(vf) ==
	    VFRAME_SIGNAL_FMT_HDR10PRIME) {
		size = 0;
		req.aux_buf = (char *)get_sei_from_src_fmt(vf, &size);
		req.aux_size = size;
		prime_sl_video = true;
	} else {
		vf_notify_provider_by_name("vdec.h265.00",
			   VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
			   (void *)&req);
		if (!req.aux_buf)
			vf_notify_provider_by_name("decoder",
				   VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
				   (void *)&req);
	}

	if (req.aux_buf && req.aux_size &&
	    (prime_sl_debug & 0x10)) {
		meta_buf = req.aux_buf;
		pr_sl(0x10, "src_fmt = %d metadata(%px, %d):\n",
			get_vframe_src_fmt(vf), req.aux_buf, req.aux_size);
		for (i = 0; i < req.aux_size + 8; i += 8) {
			len = req.aux_size - i;
			if (len < 8) {
				pr_sl(0x10, "\t i = %02d", i);
				for (j = 0; j < len; j++)
					pr_sl(0x10, "%02x ", meta_buf[i + j]);
				pr_sl(0x10, "\n");
			} else {
				pr_sl(0x10,
					"\t i = %02d: %02x %02x %02x %02x %02x %02x %02x %02x\n",
					i,
					meta_buf[i],
					meta_buf[i + 1],
					meta_buf[i + 2],
					meta_buf[i + 3],
					meta_buf[i + 4],
					meta_buf[i + 5],
					meta_buf[i + 6],
					meta_buf[i + 7]);
			}
		}
	}

	if (req.aux_buf && req.aux_size) {
		u32 count = 0;
		u32 offest = 0;
		bool done = false, success = false;
		int iret;

		p = req.aux_buf;
		while (p < req.aux_buf + req.aux_size - 8) {
			size = *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			type = *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			offest += 8;
			if (offest + size > req.aux_size) {
				pr_err("%s exception: t:%x, s:%d, p:%px, c:%d, offt:%d, aux:%px, s:%d\n",
					__func__,
					type, size, p, count, offest,
					req.aux_buf, req.aux_size);
				break;
			}
			if (type == 0x02000000) {
				pr_sl(0x20, "%s find sei: aux:%px, size:%d p:%px, sei size:%d\n",
					__func__, req.aux_buf, req.aux_size, p, size);
				iret = parser_prime_sei(parser, p, size);
				if (iret) {
					success = (iret > 0) ? true : false;
					done = true;
					pr_sl(1, "parser meta data done. %s\n",
						success ? "success" : "fail");
					break;
				}
			}
			pr_sl(0x40, "%s parser one: type:%x p:%px, sei size:%d\n",
				__func__, type, p, size);
			count++;
			offest += size;
			p += size;
		}
		/* assume that it's a prime_sl video or parser fail */
		if (!success && (prime_sl_video || done)) {
			fill_metadata_recovery(&prime_sl_setting.prime_metadata,
				prime_sl_display_brightness,
				signal_color_primaries,
				signal_transfer_characteristic);
			ret = 0;
			pr_sl(1, "use recovery mode for prime meta\n");
		} else if (success) {
			ret = 0;
		}
	}
	return ret;
}

void prime_sl_process(struct vframe_s *vf)
{
	static struct vframe_s *last_vf;
	bool new_vf = false;

	if (!prime_sl_probe && !prime_sl_mode)
		return;

	/* skip mode */
	if (prime_sl_mode == 3)
		return;

	if (last_vf != vf && vf)
		new_vf = true;
	last_vf = vf;

	if (prime_sl_enable && prime_sl_mode && new_vf) {
		if (!prime_sl_parser_metadata(vf))
			pr_sl(2, "parser meta success in debug mode\n");
		else
			pr_sl(1, "parser meta fail in debug mode\n");
		if (prime_sl_mode == 2 && p_funcs) {
			p_funcs->prime_metadata_parser_process(&prime_sl_setting);
			if (prime_sl_debug & 0x10) {
				dbg_metadata(&prime_sl_setting.prime_metadata);
				dbg_config(&prime_sl_setting.cfg);
				dbg_setting(&prime_sl_setting.prime_sl);
			}
		}
		return;
	}

	if (prime_sl_enable && p_funcs && new_vf &&
		prime_sl_output_mode < PRIME_SL_DISPLAY_BYPASS &&
	    !prime_sl_parser_metadata(vf)) {
		p_funcs->prime_metadata_parser_process(&prime_sl_setting);
		if (!prime_sl_running)
			dv_mem_power_on(VPU_PRIME_DOLBY_RAM);
		prime_sl_set_reg(&prime_sl_setting.prime_sl);
		prime_sl_running = 1;
	} else if (prime_sl_running &&
			(!get_video_enabled() || !vf ||
			 prime_sl_output_mode == PRIME_SL_DISPLAY_BYPASS)) {
		/* TODO: need close? */
		prime_sl_close();
		dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
		prime_sl_running = 0;
	}
}
EXPORT_SYMBOL(prime_sl_process);

static void dbg_setting(struct prime_sl_t *prime_sl)
{
	unsigned int i;

	pr_info("%s\n", __func__);
	pr_info("\t legacy_mode_en\t%d\n", prime_sl->legacy_mode_en);
	pr_info("\t clip_en\t%d\n", prime_sl->clip_en);
	pr_info("\t reg_gclk_ctrl\t%d\n", prime_sl->reg_gclk_ctrl);
	pr_info("\t gclk_ctrl\t%d\n", prime_sl->gclk_ctrl);
	pr_info("\t primesl_en\t%d\n",
		prime_sl->primesl_en);
	pr_info("\t inv_chroma_ratio\t%d\n",
		prime_sl->inv_chroma_ratio);
	pr_info("\t inv_y_ratio\t%d\n", prime_sl->inv_y_ratio);
	pr_info("\t l_headroom\t%d\n", prime_sl->l_headroom);
	pr_info("\t footroom\t%d\n", prime_sl->footroom);
	pr_info("\t c_headroom\t%d\n", prime_sl->c_headroom);
	pr_info("\t mub\t%d\n",
		prime_sl->mub);
	pr_info("\t mua\t%d\n", prime_sl->mua);

	for (i = 0; i < 7; i++)
		pr_info("\t oct[%d]\t%d\n", i, prime_sl->oct[i]);
	for (i = 0; i < 3; i++)
		pr_info("\t d_lut_threshold[%d]\t%d\n", i,
		prime_sl->d_lut_threshold[i]);
	for (i = 0; i < 4; i++)
		pr_info("\t d_lut_step[%d]\t%d\n", i, prime_sl->d_lut_step[i]);
	for (i = 0; i < 9; i++)
		pr_info("\t rgb2yuv[%d]\t%d\n", i, prime_sl->rgb2yuv[i]);
	for (i = 0; i < 65; i++)
		pr_info("\t lut_c[%d]\t%d\n", i, prime_sl->lut_c[i]);
	for (i = 0; i < 65; i++)
		pr_info("\t lut_p[%d]\t%d\n", i, prime_sl->lut_p[i]);
	for (i = 0; i < 65; i++)
		pr_info("\t lut_d[%d]\t%d\n", i, prime_sl->lut_d[i]);
}

static void dbg_config(struct prime_cfg_t *cfg)
{
	pr_info("%s\n", __func__);

	pr_info("\t width\t%d\n", cfg->width);
	pr_info("\t height\t%d\n", cfg->height);
	/* pr_info("\t bit_depth\t%d\n", cfg->bit_depth); */
	pr_info("\t display_oetf\t%d\n", cfg->display_oetf);
	pr_info("\t display_brightness\t%d\n",
		cfg->display_brightness);
	pr_info("\t yuv_range\t%d\n", cfg->yuv_range);
}

static void dbg_metadata(struct sl_hdr_metadata *pmetadata)
{
	pr_info("%s\n", __func__);

	pr_info("\t partID\t%d\n", pmetadata->part_id);
	pr_info("\t majorSpecVersionID\t%d\n",
		pmetadata->major_spec_version_id);
	pr_info("\t minorSpecVersionID\t%d\n",
		pmetadata->minor_spec_version_id);
	pr_info("\t payloadMode\t%d\n",
		pmetadata->payload_mode);
	pr_info("\t hdrPicColourSpace\t%d\n",
		pmetadata->hdr_pic_colour_space);
	pr_info("\t hdrDisplayColourSpace\t%d\n",
		pmetadata->hdr_display_colour_space);
	pr_info("\t hdrDisplayMaxLuminance\t%d\n",
		pmetadata->hdr_display_max_luminance);

	pr_info("\t hdrDisplayMinLuminance\t%d\n",
		pmetadata->hdr_display_min_luminance);
	pr_info("\t sdrPicColourSpace\t%d\n",
		pmetadata->sdr_pic_colour_space);
	pr_info("\t sdrDisplayColourSpace\t%d\n",
		pmetadata->sdr_display_colour_space);
	pr_info("\t sdrDisplayMaxLuminance\t%d\n",
		pmetadata->sdr_display_max_luminance);
	pr_info("\t sdrDisplayMinLuminance\t%d\n",
		pmetadata->sdr_display_min_luminance);
}

static ssize_t amprime_sl_debug_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	if (!buf)
		return count;

	if (!strcmp(buf, "metadata")) {
		dbg_metadata(&prime_sl_setting.prime_metadata);
		dbg_config(&prime_sl_setting.cfg);
	} else if (!strcmp(buf, "setting")) {
		dbg_setting(&prime_sl_setting.prime_sl);
	} else {
		pr_info("unsupport commands\n");
	}
	return count;
}

static struct class_attribute amprime_sl_class_attrs[] = {
	__ATTR(debug, 0644, NULL, amprime_sl_debug_store),
	__ATTR_NULL
};

int register_prime_functions(const struct hdr_prime_sl_func_s *func)
{
	int ret = -1;

	if (!p_funcs && func) {
		pr_info("*** %s. version %s ***\n",
			__func__, func->version_info);
		ret = 0;
		p_funcs = func;
		p_funcs->prime_api_init();
	}
	return ret;
}
EXPORT_SYMBOL(register_prime_functions);

int unregister_prime_functions(void)
{
	int ret = -1;

	if (p_funcs) {
		pr_info("*** %s ***\n", __func__);
		p_funcs->prime_api_exit();
		p_funcs = NULL;
		ret = 0;
	}
	return ret;
}
EXPORT_SYMBOL(unregister_prime_functions);

static struct prime_sl_device_data_s prime_sl_g12 = {
	.cpu_id = _CPU_MAJOR_ID_G12,
};

static struct prime_sl_device_data_s prime_sl_tl1 = {
	.cpu_id = _CPU_MAJOR_ID_TL1,
};

static struct prime_sl_device_data_s prime_sl_tm2 = {
	.cpu_id = _CPU_MAJOR_ID_TM2,
};

static struct prime_sl_device_data_s prime_sl_sc2 = {
	.cpu_id = _CPU_MAJOR_ID_SC2,
};

static const struct of_device_id amprime_sl_match[] = {
	{
		.compatible = "amlogic, prime_sl_g12",
		.data = &prime_sl_g12,
	},
	{
		.compatible = "amlogic, prime_sl_tl1",
		.data = &prime_sl_tl1,
	},
	{
		.compatible = "amlogic, prime_sl_tm2",
		.data = &prime_sl_tm2,
	},
	{
		.compatible = "amlogic, prime_sl_sc2",
		.data = &prime_sl_sc2,
	},
	{},
};

static int amprime_sl_probe(struct platform_device *pdev)
{
	int ret = 0;
	unsigned int i = 0;
	struct amprime_sl_dev_s *devp = &amprime_sl_dev;

	pr_info("%s start & ver: %s\n", __func__, DRIVER_VER);
	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		struct prime_sl_device_data_s *prime_sl_meson;
		struct device_node *of_node = pdev->dev.of_node;

		match = of_match_node(amprime_sl_match, of_node);
		if (match) {
			prime_sl_meson =
				(struct prime_sl_device_data_s *)match->data;
			if (prime_sl_meson) {
				memcpy(&prime_sl_meson_dev, prime_sl_meson,
					sizeof(struct prime_sl_device_data_s));
			} else {
				pr_err("%s data NOT match\n", __func__);
				return -ENODEV;
			}
		} else {
			pr_err("%s NOT match\n", __func__);
			return -ENODEV;
		}
	}

	pr_info("cpu_id=%d\n", prime_sl_meson_dev.cpu_id);
	memset(devp, 0, (sizeof(struct amprime_sl_dev_s)));
	ret = alloc_chrdev_region(&devp->devno, 0, 1, AMPRIME_SL_NAME);
	if (ret < 0)
		goto fail_alloc_region;

	devp->clsp = class_create(THIS_MODULE,
		AMPRIME_SL_CLASS_NAME);
	if (IS_ERR(devp->clsp)) {
		ret = PTR_ERR(devp->clsp);
		goto fail_create_class;
	}

	for (i = 0;  amprime_sl_class_attrs[i].attr.name; i++) {
		if (class_create_file(devp->clsp,
			&amprime_sl_class_attrs[i]) < 0)
			goto fail_class_create_file;
	}
/*	cdev_init(&devp->cdev, &amprime_sl_fops);
 *	devp->cdev.owner = THIS_MODULE;
 *	ret = cdev_add(&devp->cdev, devp->devno, 1);
 *	if (ret)
 *		goto fail_add_cdev;
 *
 *	devp->dev = device_create(devp->clsp, NULL, devp->devno,
 *			NULL, AMPRIME_SL_NAME);
 *	if (IS_ERR(devp->dev)) {
 *		ret = PTR_ERR(devp->dev);
 *		goto fail_create_device;
 *	}
 */
	prime_sl_probe = 1;
	pr_info("%s: probe ok\n", __func__);
	return 0;
/*
 *
 *fail_create_device:
 *	pr_info("[amprime_sl.] : amprime_sl device create error.\n");
 *	cdev_del(&devp->cdev);
 *
 *fail_add_cdev:
 *	pr_info("[amprime_sl.] : amprime_sl add device error.\n");
 */
fail_class_create_file:
	pr_info("[amprime_sl.] : amprime_sl class create file error.\n");
	for (i = 0; amprime_sl_class_attrs[i].attr.name; i++)
		class_remove_file(devp->clsp,
			&amprime_sl_class_attrs[i]);
	class_destroy(devp->clsp);
fail_create_class:
	pr_info("[amprime_sl.] : amprime_sl class create error.\n");
	unregister_chrdev_region(devp->devno, 1);
fail_alloc_region:
	pr_info("[amprime_sl.] : amprime_sl alloc error.\n");
	pr_info("[amprime_sl.] : amprime_sl.\n");
	return ret;
}

static int __exit amprime_sl_remove(struct platform_device *pdev)
{
	struct amprime_sl_dev_s *devp = &amprime_sl_dev;

	prime_sl_probe = 0;
	device_destroy(devp->clsp, devp->devno);
	cdev_del(&devp->cdev);
	class_destroy(devp->clsp);
	unregister_chrdev_region(devp->devno, 1);
	pr_info("[ amprime_sl.] :  amprime_sl.\n");
	return 0;
}

static struct platform_driver amprime_sl_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "amprime_sl",
		.of_match_table = amprime_sl_match,
	},
	.probe = amprime_sl_probe,
	.remove = __exit_p(amprime_sl_remove),
};

int __init amprime_sl_init(void)
{
	pr_info("prime_sl module init\n");
	if (platform_driver_register(&amprime_sl_driver)) {
		pr_info("failed to register amprime_sl module\n");
		return -ENODEV;
	}
	return 0;
}

void __exit amprime_sl_exit(void)
{
	pr_info("prime_sl module exit\n");
}

//MODULE_DESCRIPTION("Amlogic HDR Prime SL driver");
//MODULE_LICENSE("GPL");

