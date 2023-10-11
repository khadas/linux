// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <crypto/hash.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <linux/delay.h>

#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>
#include "hw/common.h"
#include "hdmi_tx.h"

#define CEA_DATA_BLOCK_COLLECTION_ADDR_1STP 0x04
#define VIDEO_TAG 0x40
#define AUDIO_TAG 0x20
#define VENDOR_TAG 0x60
#define SPEAKER_TAG 0x80
#define EXTENSION_IFDB_TAG	0x20

#define HDMI_EDID_BLOCK_TYPE_RESERVED	 0
#define HDMI_EDID_BLOCK_TYPE_AUDIO		1
#define HDMI_EDID_BLOCK_TYPE_VIDEO		2
#define HDMI_EDID_BLOCK_TYPE_VENDER	        3
#define HDMI_EDID_BLOCK_TYPE_SPEAKER	        4
#define HDMI_EDID_BLOCK_TYPE_VESA		5
#define HDMI_EDID_BLOCK_TYPE_RESERVED2	        6
#define HDMI_EDID_BLOCK_TYPE_EXTENDED_TAG       7

#define EXTENSION_VENDOR_SPECIFIC 0x1
#define EXTENSION_COLORMETRY_TAG 0x5
/* DRM stands for "Dynamic Range and Mastering " */
#define EXTENSION_DRM_STATIC_TAG 0x6
#define EXTENSION_DRM_DYNAMIC_TAG 0x7
   #define TYPE_1_HDR_METADATA_TYPE 0x0001
   #define TS_103_433_SPEC_TYPE 0x0002
   #define ITU_T_H265_SPEC_TYPE 0x0003
   #define TYPE_4_HDR_METADATA_TYPE 0x0004
/* Video Format Preference Data block */
#define EXTENSION_VFPDB_TAG	0xd
#define EXTENSION_Y420_VDB_TAG	0xe
#define EXTENSION_Y420_CMDB_TAG	0xf
#define EXTENSION_DOLBY_VSADB	0x11
#define EXTENSION_SCDB_EXT_TAG	0x79
#define EXTENSION_SBTM_EXT_TAG	0x7a /* 122 */

#define EDID_DETAILED_TIMING_DES_BLOCK0_POS 0x36
#define EDID_DETAILED_TIMING_DES_BLOCK1_POS 0x48
#define EDID_DETAILED_TIMING_DES_BLOCK2_POS 0x5A
#define EDID_DETAILED_TIMING_DES_BLOCK3_POS 0x6C

/* EDID Descriptor Tag */
#define TAG_PRODUCT_SERIAL_NUMBER 0xFF
#define TAG_ALPHA_DATA_STRING 0xFE
#define TAG_RANGE_LIMITS 0xFD
#define TAG_DISPLAY_PRODUCT_NAME_STRING 0xFC /* MONITOR NAME */
#define TAG_COLOR_POINT_DATA 0xFB
#define TAG_STANDARD_TIMINGS 0xFA
#define TAG_DISPLAY_COLOR_MANAGEMENT 0xF9
#define TAG_CVT_TIMING_CODES 0xF8
#define TAG_ESTABLISHED_TIMING_III 0xF7
#define TAG_DUMMY_DES 0x10

#define GET_BITS_FILED(val, start, len) \
	(((val) >> (start)) & ((1 << (len)) - 1))

static u8 __nosavedata edid_checkvalue[4] = {0};
static u32 hdmitx_edid_check_valid_blocks(u8 *buf);
static void edid_dtd_parsing(struct rx_cap *prxcap, u8 *data);
static void hdmitx_edid_set_default_aud(struct hdmitx_dev *hdev);
/* Base Block, Vendor/Product Information, byte[8]~[18] */
struct edid_venddat_t {
	u8 data[10];
};

static struct edid_venddat_t vendor_id[] = {
{ {0x41, 0x0C, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x03, 0x14} },
/* { {0x05, 0xAC, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00, 0x20, 0x19} }, */
/* Add new vendor data here */
};

static int xtochar(int num, unsigned char *checksum)
{
	if (((edid_checkvalue[num]  >> 4) & 0xf) <= 9)
		checksum[0] = ((edid_checkvalue[num]  >> 4) & 0xf) + '0';
	else
		checksum[0] = ((edid_checkvalue[num]  >> 4) & 0xf) - 10 + 'a';

	if ((edid_checkvalue[num] & 0xf) <= 9)
		checksum[1] = (edid_checkvalue[num] & 0xf) + '0';
	else
		checksum[1] = (edid_checkvalue[num] & 0xf) - 10 + 'a';

	return 0;
}

static void edid_save_checkvalue(u8 *buf, u32 block_cnt,
				 struct rx_cap *rxcap)
{
	u32 i, length, max;

	if (!buf)
		return;

	length = sizeof(edid_checkvalue);
	memset(edid_checkvalue, 0x00, length);

	max = (block_cnt > length) ? length : block_cnt;

	for (i = 0; i < max; i++)
		edid_checkvalue[i] = *(buf + (i + 1) * 128 - 1);

	rxcap->chksum[0] = '0';
	rxcap->chksum[1] = 'x';

	for (i = 0; i < 4; i++)
		xtochar(i, &rxcap->chksum[2 * i + 2]);
}

static void edid_parsingidmanufacturername(struct rx_cap *prxcap,
					   u8 *data)
{
	int i;
	u8 uppercase[26] = { 0 };
	u8 brand[3];

	/* Fill array uppercase with 'A' to 'Z' */
	for (i = 0; i < 26; i++)
		uppercase[i] = 'A' + i;

	brand[0] = data[0] >> 2;
	brand[1] = ((data[0] & 0x3) << 3) + (data[1] >> 5);
	brand[2] = data[1] & 0x1f;

	if (brand[0] > 26 || brand[0] == 0 ||
	    brand[1] > 26 || brand[1] == 0 ||
	    brand[2] > 26 || brand[2] == 0)
		return;
	for (i = 0; i < 3; i++)
		prxcap->IDManufacturerName[i] = uppercase[brand[i] - 1];
}

static void edid_parsingidproductcode(struct rx_cap *prxcap,
				      u8 *data)
{
	if (!data)
		return;
	prxcap->IDProductCode[0] = data[1];
	prxcap->IDProductCode[1] = data[0];
}

static void edid_parsingidserialnumber(struct rx_cap *prxcap,
				       u8 *data)
{
	int i;

	if (data)
		for (i = 0; i < 4; i++)
			prxcap->IDSerialNumber[i] = data[3 - i];
}

/* store the idx of vesa_timing[32], which is 0
 * note: only save vesa mode, for compliance with uboot.
 * uboot not parse standard timing, or CVT block.
 * as disp_cap will check all mode in rx_cap->VIC[],
 * and all mode in vesa_timing[], if CEA mode is
 * stored in vesa_timing[], it will cause kernel
 * support more CEA mode than uboot.
 */
static void store_vesa_idx(struct rx_cap *prxcap, enum hdmi_vic vesa_timing)
{
	int i;

	for (i = 0; i < VESA_MAX_TIMING; i++) {
		if (!prxcap->vesa_timing[i]) {
			prxcap->vesa_timing[i] = vesa_timing;
			break;
		}

		if (prxcap->vesa_timing[i] == vesa_timing)
			break;
	}
}

static void store_cea_idx(struct rx_cap *prxcap, enum hdmi_vic vic)
{
	int i;
	int already = 0;

	for (i = 0; (i < VIC_MAX_NUM) && (i < prxcap->VIC_count); i++) {
		if (vic == prxcap->VIC[i]) {
			already = 1;
			break;
		}
	}
	if (!already) {
		prxcap->VIC[prxcap->VIC_count] = vic;
		prxcap->VIC_count++;
	}
}

static void store_y420_idx(struct rx_cap *prxcap, enum hdmi_vic vic)
{
	int i;
	int already = 0;

	/* Y420 is claimed in Y420VDB, y420_vic[] will list in dc_cap */
	for (i = 0; i < Y420_VIC_MAX_NUM; i++) {
		if (vic == prxcap->y420_vic[i]) {
			already = 1;
			break;
		}
	}
	if (!already) {
		for (i = 0; i < Y420_VIC_MAX_NUM; i++) {
			if (prxcap->y420_vic[i] == 0) {
				prxcap->y420_vic[i] = vic;
				break;
			}
		}
	}
}

static void edid_establishedtimings(struct rx_cap *prxcap, u8 *data)
{
	if (data[0] & (1 << 5))
		store_vesa_idx(prxcap, HDMIV_640x480p60hz);
	if (data[0] & (1 << 0))
		store_vesa_idx(prxcap, HDMIV_800x600p60hz);
	if (data[1] & (1 << 3))
		store_vesa_idx(prxcap, HDMIV_1024x768p60hz);
}

static void edid_standardtimingiii(struct rx_cap *prxcap, u8 *data)
{
	if (data[0] & (1 << 0))
		store_vesa_idx(prxcap, HDMIV_1152x864p75hz);
	if (data[1] & (1 << 6))
		store_vesa_idx(prxcap, HDMIV_1280x768p60hz);
	if (data[1] & (1 << 2))
		store_vesa_idx(prxcap, HDMIV_1280x960p60hz);
	if (data[1] & (1 << 1))
		store_vesa_idx(prxcap, HDMIV_1280x1024p60hz);
	if (data[2] & (1 << 7))
		store_vesa_idx(prxcap, HDMIV_1360x768p60hz);
	if (data[2] & (1 << 1))
		store_vesa_idx(prxcap, HDMIV_1400x1050p60hz);
	if (data[3] & (1 << 5))
		store_vesa_idx(prxcap, HDMIV_1680x1050p60hz);
	if (data[3] & (1 << 2))
		store_vesa_idx(prxcap, HDMIV_1600x1200p60hz);
	if (data[4] & (1 << 0))
		store_vesa_idx(prxcap, HDMIV_1920x1200p60hz);
}

static void calc_timing(u8 *data, struct vesa_standard_timing *t)
{
	const struct hdmi_timing *standard_timing = NULL;

	if (data[0] < 2 && data[1] < 2)
		return;
	t->hactive = (data[0] + 31) * 8;
	switch ((data[1] >> 6) & 0x3) {
	case 0:
		t->vactive = t->hactive * 5 / 8;
		break;
	case 1:
		t->vactive = t->hactive * 3 / 4;
		break;
	case 2:
		t->vactive = t->hactive * 4 / 5;
		break;
	case 3:
	default:
		t->vactive = t->hactive * 9 / 16;
		break;
	}
	t->vsync = (data[1] & 0x3f) + 60;
	standard_timing = hdmitx21_match_standrd_timing(t);
	if (standard_timing) {
		struct hdmitx_dev *hdev = get_hdmitx21_device();
		struct rx_cap *prxcap = &hdev->rxcap;

		/* prefer 16x9 mode */
		if (standard_timing->vic == HDMI_6_720x480i60_4x3 ||
			standard_timing->vic == HDMI_2_720x480p60_4x3 ||
			standard_timing->vic == HDMI_21_720x576i50_4x3 ||
			standard_timing->vic == HDMI_17_720x576p50_4x3)
			t->vesa_timing = standard_timing->vic + 1;
		else
			t->vesa_timing = standard_timing->vic;
		if (t->vesa_timing < HDMITX_VESA_OFFSET) {
			/* for compliance with uboot, don't
			 * save CEA mode in standard_timing block.
			 * uboot don't parse standard_timing block
			 */
			/* store_cea_idx(prxcap, t->vesa_timing); */
		} else {
			store_vesa_idx(prxcap, t->vesa_timing);
		}
	}
}

static void edid_standardtiming(struct rx_cap *prxcap, u8 *data,
				int max_num)
{
	int i;
	struct vesa_standard_timing timing;

	for (i = 0; i < max_num; i++) {
		memset(&timing, 0, sizeof(struct vesa_standard_timing));
		calc_timing(&data[i * 2], &timing);
	}
}

static void edid_receiverproductnameparse(struct rx_cap *prxcap,
					  u8 *data)
{
	int i = 0;
	/* some Display Product name end with 0x20, not 0x0a
	 */
	while ((data[i] != 0x0a) && (data[i] != 0x20) && (i < 13)) {
		prxcap->ReceiverProductName[i] = data[i];
		i++;
	}
	prxcap->ReceiverProductName[i] = '\0';
}
/* ----------------------------------------------------------- */
static void edid_parseceatiming(struct rx_cap *prxcap,
	unsigned char *buff)
{
	int i;
	unsigned char *dtd_base = buff;

	for (i = 0; i < 4; i++) {
		edid_dtd_parsing(prxcap, dtd_base);
		dtd_base += 0x12;
	}
}

static void set_vsdb_phy_addr(struct hdmitx_dev *hdev,
			      struct vsdb_phyaddr *vsdb,
			      u8 *edid_offset)
{
	int phy_addr;

	vsdb->a = (edid_offset[0] >> 4) & 0xf;
	vsdb->b = (edid_offset[0] >> 0) & 0xf;
	vsdb->c = (edid_offset[1] >> 4) & 0xf;
	vsdb->d = (edid_offset[1] >> 0) & 0xf;
	vsdb->valid = 1;

	phy_addr = ((vsdb->a & 0xf) << 12) |
		   ((vsdb->b & 0xf) <<  8) |
		   ((vsdb->c & 0xf) <<  4) |
		   ((vsdb->d & 0xf) << 0);
	hdev->physical_addr = phy_addr;
	if (hdev->tv_usage == 0)
		hdmitx21_event_notify(HDMITX_PHY_ADDR_VALID, &phy_addr);
}

static void set_vsdb_dc_cap(struct rx_cap *prxcap)
{
	prxcap->dc_y444 = !!(prxcap->ColorDeepSupport & (1 << 3));
	prxcap->dc_30bit = !!(prxcap->ColorDeepSupport & (1 << 4));
	prxcap->dc_36bit = !!(prxcap->ColorDeepSupport & (1 << 5));
	prxcap->dc_48bit = !!(prxcap->ColorDeepSupport & (1 << 6));
}

static void set_vsdb_dc_420_cap(struct rx_cap *prxcap,
				u8 *edid_offset)
{
	prxcap->dc_30bit_420 = !!(edid_offset[6] & (1 << 0));
	prxcap->dc_36bit_420 = !!(edid_offset[6] & (1 << 1));
	prxcap->dc_48bit_420 = !!(edid_offset[6] & (1 << 2));
}

static void _edid_parsingvendspec(struct dv_info *dv,
				  struct hdr10_plus_info *hdr10_plus,
				  struct cuva_info *cuva,
				 u8 *buf)
{
	u8 *dat = buf;
	u8 *cuva_dat = buf;
	u8 pos = 0;
	u32 ieeeoui = 0;
	u8 length = 0;

	length = dat[pos] & 0x1f;
	pos++;

	if (dat[pos] != 1) {
		pr_info("hdmitx: edid: parsing fail %s[%d]\n", __func__,
			__LINE__);
		return;
	}

	pos++;
	ieeeoui = dat[pos++];
	ieeeoui += dat[pos++] << 8;
	ieeeoui += dat[pos++] << 16;
	pr_info("Edid_ParsingVendSpec:ieeeoui=0x%x,len=%u\n", ieeeoui, length);

/*HDR10+ use vsvdb*/
	if (ieeeoui == HDR10_PLUS_IEEE_OUI) {
		memset(hdr10_plus, 0, sizeof(struct hdr10_plus_info));
		hdr10_plus->length = length;
		hdr10_plus->ieeeoui = ieeeoui;
		hdr10_plus->application_version = dat[pos] & 0x3;
		pos++;
		return;
	}
	if (ieeeoui == CUVA_IEEEOUI) {
		memcpy(cuva->rawdata, cuva_dat, 15); /* 15, fixed length */
		cuva->length = cuva_dat[0] & 0x1f;
		cuva->ieeeoui = cuva_dat[2] |
				(cuva_dat[3] << 8) |
				(cuva_dat[4] << 16);
		cuva->system_start_code = cuva_dat[5];
		cuva->version_code = cuva_dat[6] >> 4;
		cuva->display_max_lum = cuva_dat[7] |
					(cuva_dat[8] << 8) |
					(cuva_dat[9] << 16) |
					(cuva_dat[10] << 24);
		cuva->display_min_lum = cuva_dat[11] | (cuva_dat[12] << 8);
		cuva->rx_mode_sup = (cuva_dat[13] >> 6) & 0x1;
		cuva->monitor_mode_sup = (cuva_dat[13] >> 7) & 0x1;
		return;
	}

	if (ieeeoui == DV_IEEE_OUI) {
		/* it is a Dovi block*/
		memset(dv, 0, sizeof(struct dv_info));
		dv->block_flag = CORRECT;
		dv->length = length;
		memcpy(dv->rawdata, dat, dv->length + 1);
		dv->ieeeoui = ieeeoui;
		dv->ver = (dat[pos] >> 5) & 0x7;
		if (dv->ver > 2) {
			dv->block_flag = ERROR_VER;
			return;
		}
		/* Refer to DV 2.9 Page 27 */
		if (dv->ver == 0) {
			if (dv->length == 0x19) {
				dv->sup_yuv422_12bit = dat[pos] & 0x1;
				dv->sup_2160p60hz = (dat[pos] >> 1) & 0x1;
				dv->sup_global_dimming = (dat[pos] >> 2) & 0x1;
				pos++;
				dv->Rx =
					(dat[pos + 1] << 4) | (dat[pos] >> 4);
				dv->Ry =
					(dat[pos + 2] << 4) | (dat[pos] & 0xf);
				pos += 3;
				dv->Gx =
					(dat[pos + 1] << 4) | (dat[pos] >> 4);
				dv->Gy =
					(dat[pos + 2] << 4) | (dat[pos] & 0xf);
				pos += 3;
				dv->Bx =
					(dat[pos + 1] << 4) | (dat[pos] >> 4);
				dv->By =
					(dat[pos + 2] << 4) | (dat[pos] & 0xf);
				pos += 3;
				dv->Wx =
					(dat[pos + 1] << 4) | (dat[pos] >> 4);
				dv->Wy =
					(dat[pos + 2] << 4) | (dat[pos] & 0xf);
				pos += 3;
				dv->tminPQ =
					(dat[pos + 1] << 4) | (dat[pos] >> 4);
				dv->tmaxPQ =
					(dat[pos + 2] << 4) | (dat[pos] & 0xf);
				pos += 3;
				dv->dm_major_ver = dat[pos] >> 4;
				dv->dm_minor_ver = dat[pos] & 0xf;
				pos++;
				pr_info("v0 VSVDB: len=%d, sup_2160p60hz=%d\n",
					dv->length, dv->sup_2160p60hz);
			} else {
				dv->block_flag = ERROR_LENGTH;
			}
		}

		if (dv->ver == 1) {
			if (dv->length == 0x0B) {/* Refer to DV 2.9 Page 33 */
				dv->dm_version = (dat[pos] >> 2) & 0x7;
				dv->sup_yuv422_12bit = dat[pos] & 0x1;
				dv->sup_2160p60hz = (dat[pos] >> 1) & 0x1;
				pos++;
				dv->sup_global_dimming = dat[pos] & 0x1;
				dv->tmax_lum = dat[pos] >> 1;
				pos++;
				dv->colorimetry = dat[pos] & 0x1;
				dv->tmin_lum = dat[pos] >> 1;
				pos++;
				dv->low_latency = dat[pos] & 0x3;
				dv->Bx = 0x20 | ((dat[pos] >> 5) & 0x7);
				dv->By = 0x08 | ((dat[pos] >> 2) & 0x7);
				pos++;
				dv->Gx = 0x00 | (dat[pos] >> 1);
				dv->Ry = 0x40 | ((dat[pos] & 0x1) |
					((dat[pos + 1] & 0x1) << 1) |
					((dat[pos + 2] & 0x3) << 2));
				pos++;
				dv->Gy = 0x80 | (dat[pos] >> 1);
				pos++;
				dv->Rx = 0xA0 | (dat[pos] >> 3);
				pos++;
				pr_info("v1 VSVDB: len=%d, sup_2160p60hz=%d, low_latency=%d\n",
					dv->length, dv->sup_2160p60hz, dv->low_latency);
			} else if (dv->length == 0x0E) {
				dv->dm_version = (dat[pos] >> 2) & 0x7;
				dv->sup_yuv422_12bit = dat[pos] & 0x1;
				dv->sup_2160p60hz = (dat[pos] >> 1) & 0x1;
				pos++;
				dv->sup_global_dimming = dat[pos] & 0x1;
				dv->tmax_lum = dat[pos] >> 1;
				pos++;
				dv->colorimetry = dat[pos] & 0x1;
				dv->tmin_lum = dat[pos] >> 1;
				pos += 2; /* byte8 is reserved as 0 */
				dv->Rx = dat[pos++];
				dv->Ry = dat[pos++];
				dv->Gx = dat[pos++];
				dv->Gy = dat[pos++];
				dv->Bx = dat[pos++];
				dv->By = dat[pos++];
				pr_info("v1 VSVDB: len=%d, sup_2160p60hz=%d\n",
					dv->length, dv->sup_2160p60hz);
			} else {
				dv->block_flag = ERROR_LENGTH;
			}
		}
		if (dv->ver == 2) {
			/* v2 VSVDB length could be greater than 0xB
			 * and should not be treated as unrecognized
			 * block. Instead, we should parse it as a regular
			 * v2 VSVDB using just the remaining 11 bytes here
			 */
			if (dv->length >= 0x0B) {
				dv->sup_2160p60hz = 0x1;/*default*/
				dv->dm_version = (dat[pos] >> 2) & 0x7;
				dv->sup_yuv422_12bit = dat[pos] & 0x1;
				dv->sup_backlight_control = (dat[pos] >> 1) & 0x1;
				pos++;
				dv->sup_global_dimming = (dat[pos] >> 2) & 0x1;
				dv->backlt_min_luma = dat[pos] & 0x3;
				dv->tminPQ = dat[pos] >> 3;
				pos++;
				dv->Interface = dat[pos] & 0x3;
				dv->parity = (dat[pos] >> 0x2) & 0x1;
				/* if parity = 0, then not support > 60hz nor 8k */
				dv->sup_1080p120hz = dv->parity;
				dv->tmaxPQ = dat[pos] >> 3;
				pos++;
				dv->sup_10b_12b_444 = ((dat[pos] & 0x1) << 1) |
					(dat[pos + 1] & 0x1);
				dv->Gx = 0x00 | (dat[pos] >> 1);
				pos++;
				dv->Gy = 0x80 | (dat[pos] >> 1);
				pos++;
				dv->Rx = 0xA0 | (dat[pos] >> 3);
				dv->Bx = 0x20 | (dat[pos] & 0x7);
				pos++;
				dv->Ry = 0x40  | (dat[pos] >> 3);
				dv->By = 0x08  | (dat[pos] & 0x7);
				pos++;
				pr_info("v2 VSVDB: len=%d, sup_2160p60hz=%d, Interface=%d\n",
					dv->length, dv->sup_2160p60hz, dv->Interface);
			} else {
				dv->block_flag = ERROR_LENGTH;
			}
		}
		if (pos > (dv->length + 1))
			pr_info("hdmitx: edid: maybe invalid dv%d data\n", dv->ver);
		return;
	}
	/* future: other new VSVDB add here: */
}

/*hdr_priority = 1, hdr_cap mask dv_info
 *hdr_priority = 2, hdr_cap mask dv_info and hdr_info
 */
static void edid_parsingvendspec(struct hdmitx_dev *hdev,
				 struct rx_cap *prxcap,
				 u8 *buf)
{
	struct dv_info *dv = &prxcap->dv_info;
	struct dv_info *dv2 = &prxcap->dv_info2;
	struct hdr10_plus_info *hdr10_plus = &prxcap->hdr_info.hdr10plus_info;
	struct hdr10_plus_info *hdr10_plus2 = &prxcap->hdr_info2.hdr10plus_info;
	struct cuva_info *cuva = &prxcap->hdr_info.cuva_info;
	struct cuva_info *cuva2 = &prxcap->hdr_info2.cuva_info;

	u8 pos = 0;
	u32 ieeeoui = 0;

	pos++;

	if (buf[pos] != 1) {
		pr_info("hdmitx: edid: parsing fail %s[%d]\n", __func__,
			__LINE__);
		return;
	}

	pos++;
	ieeeoui = buf[pos++];
	ieeeoui += buf[pos++] << 8;
	ieeeoui += buf[pos++] << 16;

	if ((hdev->hdr_priority == 1 && ieeeoui == DV_IEEE_OUI) ||
		hdev->hdr_priority == 2) {
		_edid_parsingvendspec(dv2, hdr10_plus2, cuva2, buf);
		return;
	}
	_edid_parsingvendspec(dv, hdr10_plus, cuva, buf);
	_edid_parsingvendspec(dv2, hdr10_plus2, cuva2, buf);
}

/* ----------------------------------------------------------- */
static int edid_parsingy420vdb(struct rx_cap *prxcap, u8 *buf)
{
	u8 tag = 0, ext_tag = 0, data_end = 0;
	u32 pos = 0;

	tag = (buf[pos] >> 5) & 0x7;
	data_end = (buf[pos] & 0x1f) + 1;
	pos++;
	ext_tag = buf[pos];

	if (tag != 0x7 || ext_tag != 0xe)
		goto INVALID_Y420VDB;

	pos++;
	while (pos < data_end) {
		if (_is_y420_vic(buf[pos])) {
			store_cea_idx(prxcap, buf[pos]);
			store_y420_idx(prxcap, buf[pos]);
		}
		pos++;
	}

	return 0;

INVALID_Y420VDB:
	pr_info("[%s] it's not a valid y420vdb!\n", __func__);
	return -1;
}

static int _edid_parsedrmsb(struct hdr_info *info, u8 *buf)
{
	u8 tag = 0, ext_tag = 0, data_end = 0;
	u32 pos = 0;

	tag = (buf[pos] >> 5) & 0x7;
	data_end = (buf[pos] & 0x1f);
	memset(info->rawdata, 0, 7);
	memcpy(info->rawdata, buf, data_end + 1);
	pos++;
	ext_tag = buf[pos];
	if (tag != HDMI_EDID_BLOCK_TYPE_EXTENDED_TAG ||
	    ext_tag != EXTENSION_DRM_STATIC_TAG)
		goto INVALID_DRM_STATIC;
	pos++;
	info->hdr_support = buf[pos];
	pos++;
	info->static_metadata_type1 = buf[pos];
	pos++;
	if (data_end == 3)
		return 0;
	if (data_end == 4) {
		info->lumi_max = buf[pos];
		return 0;
	}
	if (data_end == 5) {
		info->lumi_max = buf[pos];
		info->lumi_avg = buf[pos + 1];
		return 0;
	}
	if (data_end == 6) {
		info->lumi_max = buf[pos];
		info->lumi_avg = buf[pos + 1];
		info->lumi_min = buf[pos + 2];
		return 0;
	}
	return 0;
INVALID_DRM_STATIC:
	pr_info("[%s] it's not a valid DRM STATIC BLOCK\n", __func__);
	return -1;
}

static int edid_parsedrmsb(struct rx_cap *prxcap, u8 *buf)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct hdr_info *hdr = &prxcap->hdr_info;
	struct hdr_info *hdr2 = &prxcap->hdr_info2;

	if (hdev->hdr_priority == 2) {
		_edid_parsedrmsb(hdr2, buf);
		return 0;
	}
	_edid_parsedrmsb(hdr, buf);
	_edid_parsedrmsb(hdr2, buf);
	return 1;
}

static int _edid_parsedrmdb(struct hdr_info *info, u8 *buf)
{
	u8 tag = 0, ext_tag = 0, data_end = 0;
	u32 pos = 0;
	u32 type;
	u32 type_length;
	u32 i;
	u32 num;

	tag = (buf[pos] >> 5) & 0x7;
	data_end = (buf[pos] & 0x1f);
	pos++;
	ext_tag = buf[pos];
	if (tag != HDMI_EDID_BLOCK_TYPE_EXTENDED_TAG ||
	    ext_tag != EXTENSION_DRM_DYNAMIC_TAG)
		goto INVALID_DRM_DYNAMIC;
	pos++;
	data_end--;/*extended tag code byte doesn't need*/

	while (data_end) {
		type_length = buf[pos];
		pos++;
		type = (buf[pos + 1] << 8) | buf[pos];
		pos += 2;
		switch (type) {
		case TS_103_433_SPEC_TYPE:
			num = 1;
			break;
		case ITU_T_H265_SPEC_TYPE:
			num = 2;
			break;
		case TYPE_4_HDR_METADATA_TYPE:
			num = 3;
			break;
		case TYPE_1_HDR_METADATA_TYPE:
		default:
			num = 0;
			break;
		}
		info->dynamic_info[num].of_len = type_length;
		info->dynamic_info[num].type = type;
		info->dynamic_info[num].support_flags = buf[pos];
		pos++;
		for (i = 0; i < type_length - 3; i++) {
			info->dynamic_info[num].optional_fields[i] =
			buf[pos];
			pos++;
		}
		data_end = data_end - (type_length + 1);
	}

	return 0;
INVALID_DRM_DYNAMIC:
	pr_info("[%s] it's not a valid DRM DYNAMIC BLOCK\n", __func__);
	return -1;
}

static int edid_parsedrmdb(struct rx_cap *prxcap, u8 *buf)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct hdr_info *hdr = &prxcap->hdr_info;
	struct hdr_info *hdr2 = &prxcap->hdr_info2;

	if (hdev->hdr_priority == 2) {
		_edid_parsedrmdb(hdr2, buf);
		return 0;
	}
	_edid_parsedrmdb(hdr, buf);
	_edid_parsedrmdb(hdr2, buf);
	return 1;
}

static int edid_parsingvfpdb(struct rx_cap *prxcap, u8 *buf)
{
	u32 len = buf[0] & 0x1f;
	enum hdmi_vic svr = HDMI_0_UNKNOWN;

	if (buf[1] != EXTENSION_VFPDB_TAG)
		return 0;
	if (len < 2)
		return 0;

	svr = buf[2];
	if ((svr >= 1 && svr <= 127) ||
	    (svr >= 193 && svr <= 253)) {
		prxcap->flag_vfpdb = 1;
		prxcap->preferred_mode = svr;
		pr_info("preferred mode 0 srv %d\n", prxcap->preferred_mode);
		return 1;
	}
	if (svr >= 129 && svr <= 144) {
		prxcap->flag_vfpdb = 1;
		prxcap->preferred_mode = prxcap->dtd[svr - 129].vic;
		pr_info("preferred mode 0 dtd %d\n", prxcap->preferred_mode);
		return 1;
	}
	return 0;
}

/* ----------------------------------------------------------- */
static int edid_parsingy420cmdb(struct hdmitx_dev *hdev,
				u8 *buf)
{
	u8 tag = 0, ext_tag = 0, length = 0, data_end = 0;
	u32 pos = 0, i = 0;
	struct hdmitx_info *info = &hdev->hdmi_info;

	tag = (buf[pos] >> 5) & 0x7;
	length = buf[pos] & 0x1f;
	data_end = length + 1;
	pos++;
	ext_tag = buf[pos];

	if (tag != 0x7 || ext_tag != 0xf)
		goto INVALID_Y420CMDB;

	if (length == 1) {
		info->y420_all_vic = 1;
		return 0;
	}

	info->bitmap_length = 0;
	info->bitmap_valid = 0;
	memset(info->y420cmdb_bitmap, 0x00, Y420CMDB_MAX);

	pos++;
	if (pos < data_end) {
		info->bitmap_length = data_end - pos;
		info->bitmap_valid = 1;
	}
	while (pos < data_end) {
		if (i < Y420CMDB_MAX)
			info->y420cmdb_bitmap[i] = buf[pos];
		pos++;
		i++;
	}

	return 0;

INVALID_Y420CMDB:
	pr_info("[%s] it's not a valid y420cmdb!\n", __func__);
	return -1;
}

static void edid_parsingdolbyvsadb(struct hdmitx_dev *hdev,
				unsigned char *buf)
{
	unsigned char length = 0;
	unsigned char pos = 0;
	unsigned int ieeeoui;
	struct dolby_vsadb_cap *cap = &hdev->rxcap.dolby_vsadb_cap;

	memset(cap->rawdata, 0, sizeof(cap->rawdata));
	memcpy(cap->rawdata, buf, 7); /* fixed 7 bytes */

	pos = 0;
	length = buf[pos] & 0x1f;
	if (length != 0x06)
		pr_info("%s[%d]: the length is %d, should be 6 bytes\n",
			__func__, __LINE__, length);

	cap->length = length;
	pos += 2;
	ieeeoui = buf[pos] + (buf[pos + 1] << 8) + (buf[pos + 2] << 16);
	if (ieeeoui != DOVI_IEEEOUI)
		pr_info("%s[%d]: the ieeeoui is 0x%x, should be 0x%x\n",
			__func__, __LINE__, ieeeoui, DOVI_IEEEOUI);
	cap->ieeeoui = ieeeoui;

	pos += 3;
	cap->dolby_vsadb_ver = buf[pos] & 0x7;
	if (cap->dolby_vsadb_ver)
		pr_info("%s[%d]: the version is 0x%x, should be 0x0\n",
			__func__, __LINE__, cap->dolby_vsadb_ver);

	cap->spk_center = (buf[pos] >> 4) & 1;
	cap->spk_surround = (buf[pos] >> 5) & 1;
	cap->spk_height = (buf[pos] >> 6) & 1;
	cap->headphone_only = (buf[pos] >> 7) & 1;

	pos++;
	cap->mat_48k_pcm_only = (buf[pos] >> 0) & 1;
}

static int edid_y420cmdb_fill_all_vic(struct hdmitx_dev *hdmitx_device)
{
	struct rx_cap *rxcap = &hdmitx_device->rxcap;
	struct hdmitx_info *info = &hdmitx_device->hdmi_info;
	u32 count = rxcap->VIC_count;
	u32 a, b;

	if (info->y420_all_vic != 1)
		return 1;

	a = count / 8;
	a = (a >= Y420CMDB_MAX) ? Y420CMDB_MAX : a;
	b = count % 8;

	if (a > 0)
		memset(&info->y420cmdb_bitmap[0], 0xff, a);

	if (b != 0 && a < Y420CMDB_MAX)
		info->y420cmdb_bitmap[a] = (1 << b) - 1;

	info->bitmap_length = (b == 0) ? a : (a + 1);
	info->bitmap_valid = (info->bitmap_length != 0) ? 1 : 0;

	return 0;
}

static int edid_y420cmdb_postprocess(struct hdmitx_dev *hdev)
{
	u32 i = 0, j = 0, valid = 0;
	struct rx_cap *prxcap = &hdev->rxcap;
	struct hdmitx_info *info = &hdev->hdmi_info;
	u8 *p = NULL;
	enum hdmi_vic vic;

	if (info->y420_all_vic == 1)
		edid_y420cmdb_fill_all_vic(hdev);

	if (info->bitmap_valid == 0)
		goto PROCESS_END;

	for (i = 0; i < info->bitmap_length; i++) {
		p = &info->y420cmdb_bitmap[i];
		for (j = 0; j < 8; j++) {
			valid = ((*p >> j) & 0x1);
			vic = prxcap->VIC[i * 8 + j];
			if (valid != 0 && _is_y420_vic(vic)) {
				store_cea_idx(prxcap, vic);
				store_y420_idx(prxcap, vic);
			}
		}
	}

PROCESS_END:
	return 0;
}

static void edid_y420cmdb_reset(struct hdmitx_info *info)
{
	info->bitmap_valid = 0;
	info->bitmap_length = 0;
	info->y420_all_vic = 0;
	memset(info->y420cmdb_bitmap, 0x00, Y420CMDB_MAX);
}

/* ----------------------------------------------------------- */
static void hdmitx_3d_update(struct rx_cap *prxcap)
{
	int j = 0;

	for (j = 0; j < 16; j++) {
		if ((prxcap->threeD_MASK_15_0 >> j) & 0x1) {
			/* frame packing */
			if (prxcap->threeD_Structure_ALL_15_0
				& (1 << 0))
				prxcap->support_3d_format[prxcap->VIC[j]]
				.frame_packing = 1;
			/* top and bottom */
			if (prxcap->threeD_Structure_ALL_15_0
				& (1 << 6))
				prxcap->support_3d_format[prxcap->VIC[j]]
				.top_and_bottom = 1;
			/* top and bottom */
			if (prxcap->threeD_Structure_ALL_15_0
				& (1 << 8))
				prxcap->support_3d_format[prxcap->VIC[j]]
				.side_by_side = 1;
		}
	}
}

/* parse Sink 3D information */
static int hdmitx_edid_3d_parse(struct rx_cap *prxcap, u8 *dat,
				u32 size)
{
	int j = 0;
	u32 base = 0;
	u32 pos = base + 1;
	u32 index = 0;

	if (dat[base] & (1 << 7))
		pos += 2;
	if (dat[base] & (1 << 6))
		pos += 2;
	if (dat[base] & (1 << 5)) {
		prxcap->threeD_present = dat[pos] >> 7;
		prxcap->threeD_Multi_present = (dat[pos] >> 5) & 0x3;
		pos += 1;
		prxcap->hdmi_vic_LEN = dat[pos] >> 5;
		prxcap->HDMI_3D_LEN = dat[pos] & 0x1f;
		pos += prxcap->hdmi_vic_LEN + 1;

		if (prxcap->threeD_Multi_present == 0x01) {
			prxcap->threeD_Structure_ALL_15_0 =
				(dat[pos] << 8) + dat[pos + 1];
			prxcap->threeD_MASK_15_0 = 0;
			pos += 2;
		}
		if (prxcap->threeD_Multi_present == 0x02) {
			prxcap->threeD_Structure_ALL_15_0 =
				(dat[pos] << 8) + dat[pos + 1];
			pos += 2;
			prxcap->threeD_MASK_15_0 =
			(dat[pos] << 8) + dat[pos + 1];
			pos += 2;
		}
	}
	while (pos < size) {
		if ((dat[pos] & 0xf) < 0x8) {
			/* frame packing */
			if ((dat[pos] & 0xf) == T3D_FRAME_PACKING)
				prxcap->support_3d_format[prxcap->VIC[((dat[pos]
					& 0xf0) >> 4)]].frame_packing = 1;
			/* top and bottom */
			if ((dat[pos] & 0xf) == T3D_TAB)
				prxcap->support_3d_format[prxcap->VIC[((dat[pos]
					& 0xf0) >> 4)]].top_and_bottom = 1;
			pos += 1;
		} else {
			/* SidebySide */
			if ((dat[pos] & 0xf) == T3D_SBS_HALF &&
			    (dat[pos + 1] >> 4) < 0xb) {
				index = (dat[pos] & 0xf0) >> 4;
				prxcap->support_3d_format[prxcap->VIC[index]]
				.side_by_side = 1;
			}
			pos += 2;
		}
	}
	if (prxcap->threeD_MASK_15_0 == 0) {
		for (j = 0; (j < 16) && (j < prxcap->VIC_count); j++) {
			prxcap->support_3d_format[prxcap->VIC[j]]
			.frame_packing = 1;
			prxcap->support_3d_format[prxcap->VIC[j]]
			.top_and_bottom = 1;
			prxcap->support_3d_format[prxcap->VIC[j]]
			.side_by_side = 1;
		}
	} else {
		hdmitx_3d_update(prxcap);
	}
	return 1;
}

/* parse Sink 4k2k information */
static void hdmitx_edid_4k2k_parse(struct rx_cap *prxcap, u8 *dat,
				   u32 size)
{
	if (size > 4 || size == 0) {
		pr_info(EDID
			"4k2k in edid out of range, SIZE = %d\n",
			size);
		return;
	}
	while (size--) {
		if (*dat == 1)
			store_cea_idx(prxcap, HDMI_95_3840x2160p30_16x9);
		else if (*dat == 2)
			store_cea_idx(prxcap, HDMI_94_3840x2160p25_16x9);
		else if (*dat == 3)
			store_cea_idx(prxcap, HDMI_93_3840x2160p24_16x9);
		else if (*dat == 4)
			store_cea_idx(prxcap, HDMI_98_4096x2160p24_256x135);
		else
			;
		dat++;
	}
}

static void get_latency(struct rx_cap *prxcap, u8 *val)
{
	if (val[0] == 0)
		prxcap->vLatency = LATENCY_INVALID_UNKNOWN;
	else if (val[0] == 0xFF)
		prxcap->vLatency = LATENCY_NOT_SUPPORT;
	else
		prxcap->vLatency = (val[0] - 1) * 2;

	if (val[1] == 0)
		prxcap->aLatency = LATENCY_INVALID_UNKNOWN;
	else if (val[1] == 0xFF)
		prxcap->aLatency = LATENCY_NOT_SUPPORT;
	else
		prxcap->aLatency = (val[1] - 1) * 2;
}

static void get_ilatency(struct rx_cap *prxcap, u8 *val)
{
	if (val[0] == 0)
		prxcap->i_vLatency = LATENCY_INVALID_UNKNOWN;
	else if (val[0] == 0xFF)
		prxcap->i_vLatency = LATENCY_NOT_SUPPORT;
	else
		prxcap->i_vLatency = val[0] * 2 - 1;

	if (val[1] == 0)
		prxcap->i_aLatency = LATENCY_INVALID_UNKNOWN;
	else if (val[1] == 0xFF)
		prxcap->i_aLatency = LATENCY_NOT_SUPPORT;
	else
		prxcap->i_aLatency = val[1] * 2 - 1;
}

static void hdmitx21_edid_parse_hdmi14(struct rx_cap *prxcap,
				     u8 offset,
				     u8 *blockbuf,
				     u8 count)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct hdmitx_info *info = &hdev->hdmi_info;
	int idx = 0, tmp = 0;

	prxcap->ieeeoui = HDMI_IEEE_OUI;
	set_vsdb_phy_addr(hdev, &info->vsdb_phy_addr, &blockbuf[offset + 3]);
	rx_edid_physical_addr(info->vsdb_phy_addr.a,
			      info->vsdb_phy_addr.b,
			      info->vsdb_phy_addr.c,
			      info->vsdb_phy_addr.d);

	prxcap->ColorDeepSupport =
	(count > 5) ? blockbuf[offset + 5] : 0;
	set_vsdb_dc_cap(prxcap);
	prxcap->Max_TMDS_Clock1 =
	(count > 6) ? blockbuf[offset + 6] : DEFAULT_MAX_TMDS_CLK;

	if (count > 7) {
		tmp = blockbuf[offset + 7];
		idx = offset + 8;
		if (tmp & (1 << 6)) {
			u8 val[2];

			val[0] = blockbuf[idx];
			val[1] = blockbuf[idx + 1];
			get_latency(prxcap, val);
			idx += 2;
			val[0] = blockbuf[idx];
			val[1] = blockbuf[idx + 1];
			get_ilatency(prxcap, val);
			idx += 2;
		} else if (tmp & (1 << 7)) {
			u8 val[2];

			val[0] = blockbuf[idx];
			val[1] = blockbuf[idx + 1];
			get_latency(prxcap, val);
			idx += 2;
		}
		prxcap->cnc0 = (tmp >> 0) & 1;
		prxcap->cnc1 = (tmp >> 1) & 1;
		prxcap->cnc2 = (tmp >> 2) & 1;
		prxcap->cnc3 = (tmp >> 3) & 1;
		if (tmp & (1 << 5)) {
			idx += 1;
			/* valid 4k */
			if (blockbuf[idx] & 0xe0) {
				hdmitx_edid_4k2k_parse(prxcap,
						       &blockbuf[idx + 1],
				blockbuf[idx] >> 5);
			}
			/* valid 3D */
			if (blockbuf[idx - 1] & 0xe0) {
				hdmitx_edid_3d_parse(prxcap,
						     &blockbuf[offset + 7],
				count - 7);
			}
		}
	}
}

static void hdmitx21_edid_parse_ifdb(struct rx_cap *prxcap, u8 *blockbuf)
{
	u8 payload_len;
	u8 len;
	u8 sum_len = 0;

	if (!prxcap || !blockbuf)
		return;

	payload_len = blockbuf[0] & 0x1f;
	/* no additional bytes after extended tag code */
	if (payload_len <= 1)
		return;

	/* begin with an InfoFrame Processing Descriptor */
	if ((blockbuf[2] & 0x1f) != 0)
		pr_info(EDID "ERR: IFDB not begin with InfoFrame Processing Descriptor\n");
	sum_len = 1; /* Extended Tag Code len */

	len = (blockbuf[2] >> 5) & 0x7;
	sum_len += (len + 2);
	if (payload_len < sum_len) {
		pr_info(EDID "ERR: IFDB length abnormal: %d exceed playload len %d\n",
			sum_len, payload_len);
		return;
	}

	prxcap->additional_vsif_num = blockbuf[3];
	if (payload_len == sum_len)
		return;

	while (sum_len < payload_len) {
		if ((blockbuf[sum_len + 1] & 0x1f) == 1) {
			/* Short Vendor-Specific InfoFrame Descriptor */
			/* pr_info(EDID "InfoFrame Type Code: 0x1, len: %d, IEEE: %x\n", */
				/* len, blockbuf[sum_len + 4] << 16 | */
				/* blockbuf[sum_len + 3] << 8 | blockbuf[sum_len + 2]); */
			/* number of additional bytes following the 3-byte OUI */
			len = (blockbuf[sum_len + 1] >> 5) & 0x7;
			sum_len += (len + 1 + 3);
		} else {
			/* Short InfoFrame Descriptor */
			/* pr_info(EDID "InfoFrame Type Code: %x, len: %d\n", */
				/* blockbuf[sum_len + 1] & 0x1f, len); */
			len = (blockbuf[sum_len + 1] >> 5) & 0x7;
			sum_len += (len + 1);
		}
	}
}

static void hdmitx21_edid_parse_hfscdb(struct rx_cap *prxcap,
	u8 offset, u8 *blockbuf, u8 count)
{
	prxcap->hf_ieeeoui = HDMI_FORUM_IEEE_OUI;
	prxcap->Max_TMDS_Clock2 = blockbuf[offset + 4];
	prxcap->scdc_present = !!(blockbuf[offset + 5] & (1 << 7));
	prxcap->scdc_rr_capable = !!(blockbuf[offset + 5] & (1 << 6));
	prxcap->lte_340mcsc_scramble = !!(blockbuf[offset + 5] & (1 << 3));
	prxcap->max_frl_rate = (blockbuf[offset + 6] & 0xf0) >> 4;
	set_vsdb_dc_420_cap(prxcap, &blockbuf[offset]);

	if (count < 8)
		return;
	prxcap->allm = !!(blockbuf[offset + 7] & (1 << 1));
	prxcap->fva = !!(blockbuf[offset + 7] & (1 << 2));
	prxcap->neg_mvrr = !!(blockbuf[offset + 7] & (1 << 3));
	prxcap->cinemavrr = !!(blockbuf[offset + 7] & (1 << 4));
	prxcap->mdelta = !!(blockbuf[offset + 7] & (1 << 5));
	prxcap->qms = !!(blockbuf[offset + 7] & (1 << 6));
	prxcap->fapa_end_extended = !!(blockbuf[offset + 7] & (1 << 7));

	if (count < 10)
		return;
	prxcap->vrr_max = (((blockbuf[offset + 8] & 0xc0) >> 6) << 8) +
				blockbuf[offset + 9];
	prxcap->vrr_min = (blockbuf[offset + 8] & 0x3f);
	prxcap->fapa_start_loc = !!(blockbuf[offset + 7] & (1 << 0));

	if (count < 11)
		return;
	prxcap->dsc_10bpc = !!(blockbuf[offset + 10] & (1 << 0));
	prxcap->dsc_12bpc = !!(blockbuf[offset + 10] & (1 << 1));
	prxcap->dsc_16bpc = !!(blockbuf[offset + 10] & (1 << 2));
	prxcap->dsc_all_bpp = !!(blockbuf[offset + 10] & (1 << 3));
	prxcap->qms_tfr_min = !!(blockbuf[offset + 10] & (1 << 4));
	prxcap->qms_tfr_max = !!(blockbuf[offset + 10] & (1 << 5));
	prxcap->dsc_native_420 = !!(blockbuf[offset + 10] & (1 << 6));
	prxcap->dsc_1p2 = !!(blockbuf[offset + 10] & (1 << 7));
	/* dsc_1p2 shall be cleared (=0) for devices that
	 * do not support FRL (i.e. Max_FRL_Rate=0).
	 */
	if (prxcap->max_frl_rate == 0)
		prxcap->dsc_1p2 = 0;
	if (prxcap->dsc_1p2) {
		if (count < 13) {
			pr_info(EDID "error: dsc_1p2 support, but dsc not complete\n");
			prxcap->dsc_1p2 = 0;
			return;
		}
		/* 3: up to 4 slices and up to (340 MHz/K SliceAdjust)
		 * pixel clock per slice
		 * 4: up to 8 slices and up to (340 MHz/K SliceAdjust)
		 * pixel clock per slice
		 */
		prxcap->dsc_max_slices = blockbuf[offset + 11] & 0xf;
		/* This is value shall be the same or lower than
		 * the physical maximum rate specified by the
		 * Max_FRL_Rate field.
		 */
		prxcap->dsc_max_frl_rate = (blockbuf[offset + 11] >> 4) & 0xf;
		/* The number of bytes is computed as:
		 * 1024 x (1+DSC_ TotalChunkKBytes)
		 */
		prxcap->dsc_total_chunk_bytes = blockbuf[offset + 12] & 0x3f;
	}
}

static unsigned short get_2_bytes(u8 *addr)
{
	if (!addr)
		return 0;
	return addr[0] | (addr[1] << 8);
}

static void hdmitx21_edid_parse_sbtmdb(struct rx_cap *prxcap,
	u8 offset, u8 *blockbuf, u8 len)
{
	struct sbtm_info *info;

	if (!prxcap || !blockbuf || !len)
		return;
	if (len < 2)
		return;
	blockbuf = blockbuf + offset;
	/* length should be 2, 3, 5, 7, ... or 29 */
	if (!(len == 2 || (len <= 29 && ((len % 2) == 1))))
		pr_info("%s[%d] len is %d\n", __func__, __LINE__, len);
	info = &prxcap->hdr_info.sbtm_info;

	blockbuf++;
	len--;
	if (blockbuf[0])
		info->sbtm_support = 1;
	if (!info->sbtm_support)
		return;
	info->max_sbtm_ver = GET_BITS_FILED(blockbuf[0], 0, 4);
	info->grdm_support = GET_BITS_FILED(blockbuf[0], 5, 2);
	info->drdm_ind = GET_BITS_FILED(blockbuf[0], 7, 1);
	if (info->drdm_ind == 0)
		return;
	info->hgig_cat_drdm_sel = GET_BITS_FILED(blockbuf[1], 0, 3);
	info->use_hgig_drdm = GET_BITS_FILED(blockbuf[1], 4, 1);
	info->maxrgb = GET_BITS_FILED(blockbuf[1], 5, 1);
	info->gamut = GET_BITS_FILED(blockbuf[1], 6, 2);
	blockbuf += 2;
	len -= 2;
	if (info->drdm_ind && !info->gamut && len >= 16) {
		info->red_x = get_2_bytes(&blockbuf[0]);
		info->red_y = get_2_bytes(&blockbuf[2]);
		info->green_x = get_2_bytes(&blockbuf[4]);
		info->green_y = get_2_bytes(&blockbuf[6]);
		info->blue_x = get_2_bytes(&blockbuf[8]);
		info->blue_y = get_2_bytes(&blockbuf[10]);
		info->white_x = get_2_bytes(&blockbuf[12]);
		info->white_y = get_2_bytes(&blockbuf[14]);
		len -= 16;
		blockbuf += 16;
	}
	if (info->drdm_ind && !info->use_hgig_drdm && len >= 2) {
		info->min_bright_10 = blockbuf[0];
		info->peak_bright_100 = blockbuf[1];
		len -= 2;
		blockbuf += 2;
		if (len >= 2) {
			info->p0_exp = GET_BITS_FILED(blockbuf[0], 0, 2);
			info->p0_mant = GET_BITS_FILED(blockbuf[0], 2, 6);
			info->peak_bright_p0 = blockbuf[1];
			len -= 2;
			blockbuf += 2;
		}
		if (len >= 2) {
			info->p1_exp = GET_BITS_FILED(blockbuf[0], 0, 2);
			info->p1_mant = GET_BITS_FILED(blockbuf[0], 2, 6);
			info->peak_bright_p1 = blockbuf[1];
			len -= 2;
			blockbuf += 2;
		}
		if (len >= 2) {
			info->p2_exp = GET_BITS_FILED(blockbuf[0], 0, 2);
			info->p2_mant = GET_BITS_FILED(blockbuf[0], 2, 6);
			info->peak_bright_p2 = blockbuf[1];
			len -= 2;
			blockbuf += 2;
		}
		if (len >= 2) {
			info->p3_exp = GET_BITS_FILED(blockbuf[0], 0, 2);
			info->p3_mant = GET_BITS_FILED(blockbuf[0], 2, 6);
			info->peak_bright_p3 = blockbuf[1];
			/* end parsing */
		}
	}
}

/* refer to CEA-861-G 7.5.1 video data block */
static void _store_vics(struct rx_cap *prxcap, u8 vic_dat)
{
	u8 vic_bit6_0 = vic_dat & (~0x80);
	u8 vic_bit7 = !!(vic_dat & 0x80);

	if (!prxcap)
		return;

	if (vic_bit6_0 >= 1 && vic_bit6_0 <= 64) {
		store_cea_idx(prxcap, vic_bit6_0);
		if (vic_bit7) {
			if (prxcap->native_vic && !prxcap->native_vic2)
				prxcap->native_vic2 = vic_bit6_0;
			if (!prxcap->native_vic)
				prxcap->native_vic = vic_bit6_0;
		}
	} else {
		store_cea_idx(prxcap, vic_dat);
	}
}

static int hdmitx_edid_cta_block_parse(struct hdmitx_dev *hdev,
				   u8 *blockbuf)
{
	u8 offset, end;
	u8 count;
	u8 tag;
	int i, tmp, idx;
	u8 *vfpdb_offset = NULL;
	struct rx_cap *prxcap = &hdev->rxcap;
	u32 aud_flag = 0;

	/* CEA-861 implementations are required to use Tag = 0x02
	 * for the CEA Extension Tag and Sources should ignore
	 * Tags that are not understood. but for Samsung LA32D400E1
	 * its extension tag is 0x0 while other bytes normal,
	 * so continue parse as other sources do
	 */
	if (blockbuf[0] == 0x0) {
		pr_info(EDID "unknown Extension Tag detected, continue\n");
	} else if (blockbuf[0] != 0x02) {
		pr_info("skip the block of tag: 0x%02x%02x", blockbuf[0], blockbuf[1]);
		return -1; /* not a CEA BLOCK. */
	}
	end = blockbuf[2]; /* CEA description. */
	prxcap->native_Mode = blockbuf[1] >= 2 ? blockbuf[3] : 0;
	prxcap->number_of_dtd += blockbuf[1] >= 2 ? (blockbuf[3] & 0xf) : 0;

	/* do not reset anything during parsing as there could be
	 * more than one block. Below variable should be reset once
	 * before parsing and are already being reset before parse
	 *call
	 */
	/* prxcap->native_vic = 0;*/
	/* prxcap->native_vic2 = 0;*/
	/* prxcap->AUD_count = 0;*/

	edid_y420cmdb_reset(&hdev->hdmi_info);
	if (end > 127)
		return 0;
	for (offset = 4 ; offset < end ; ) {
		tag = blockbuf[offset] >> 5;
		count = blockbuf[offset] & 0x1f;
		switch (tag) {
		case HDMI_EDID_BLOCK_TYPE_AUDIO:
			aud_flag = 1;
			tmp = count / 3;
			idx = prxcap->AUD_count;
			prxcap->AUD_count += tmp;
			offset++;
			for (i = 0; i < tmp; i++) {
				prxcap->RxAudioCap[idx + i].audio_format_code =
					(blockbuf[offset + i * 3] >> 3) & 0xf;
				prxcap->RxAudioCap[idx + i].channel_num_max =
					blockbuf[offset + i * 3] & 0x7;
				prxcap->RxAudioCap[idx + i].freq_cc =
					blockbuf[offset + i * 3 + 1] & 0x7f;
				prxcap->RxAudioCap[idx + i].cc3 =
					blockbuf[offset + i * 3 + 2];
			}
			offset += count;
			break;

		case HDMI_EDID_BLOCK_TYPE_VIDEO:
			offset++;
			for (i = 0; i < count ; i++) {
				pr_info("i=%d VIC=%d\n", i, blockbuf[offset + i]);
				_store_vics(prxcap, blockbuf[offset + i]);
			}
			offset += count;
			break;

		case HDMI_EDID_BLOCK_TYPE_VENDER:
			offset++;
			if (blockbuf[offset] == 0x03 &&
			    blockbuf[offset + 1] == 0x0c &&
			    blockbuf[offset + 2] == 0x00) {
				hdmitx21_edid_parse_hdmi14(prxcap, offset,
							 blockbuf, count);
			} else if ((blockbuf[offset] == 0xd8) &&
				(blockbuf[offset + 1] == 0x5d) &&
				(blockbuf[offset + 2] == 0xc4)) {
				hdmitx21_edid_parse_hfscdb(prxcap, offset,
							 blockbuf, count);
			}
			offset += count; /* ignore the remains. */
			break;

		case HDMI_EDID_BLOCK_TYPE_SPEAKER:
			offset++;
			prxcap->RxSpeakerAllocation = blockbuf[offset];
			offset += count;
			break;

		case HDMI_EDID_BLOCK_TYPE_VESA:
			offset++;
			offset += count;
			break;

		case HDMI_EDID_BLOCK_TYPE_EXTENDED_TAG:
			{
				u8 ext_tag = 0;

				ext_tag = blockbuf[offset + 1];
				switch (ext_tag) {
				case EXTENSION_VENDOR_SPECIFIC:
					edid_parsingvendspec(hdev, prxcap,
							     &blockbuf[offset]);
					break;
				case EXTENSION_COLORMETRY_TAG:
					prxcap->colorimetry_data =
						blockbuf[offset + 2];
					prxcap->colorimetry_data2 =
						blockbuf[offset + 2];
					break;
				case EXTENSION_DRM_STATIC_TAG:
					edid_parsedrmsb(prxcap,
							&blockbuf[offset]);
					rx_set_hdr_lumi(&blockbuf[offset],
							(blockbuf[offset] &
							 0x1f) + 1);
					break;
				case EXTENSION_DRM_DYNAMIC_TAG:
					edid_parsedrmdb(prxcap,
							&blockbuf[offset]);
					break;
				case EXTENSION_VFPDB_TAG:
/* Just record VFPDB offset address, call Edid_ParsingVFPDB() after DTD
 * parsing, in case that
 * SVR >=129 and SVR <=144, Interpret as the Kth DTD in the EDID,
 * where K = SVR - 128 (for K=1 to 16)
 */
					vfpdb_offset = &blockbuf[offset];
					break;
				case EXTENSION_Y420_VDB_TAG:
					edid_parsingy420vdb(prxcap,
							    &blockbuf[offset]);
					break;
				case EXTENSION_Y420_CMDB_TAG:
					edid_parsingy420cmdb(hdev,
							     &blockbuf[offset]);
					break;
				case EXTENSION_DOLBY_VSADB:
					edid_parsingdolbyvsadb(hdev,
							     &blockbuf[offset]);
					break;
				case EXTENSION_SCDB_EXT_TAG:
					hdmitx21_edid_parse_hfscdb(prxcap, offset + 1,
							 blockbuf, count);
					break;
				case EXTENSION_SBTM_EXT_TAG:
					hdmitx21_edid_parse_sbtmdb(prxcap, offset + 1,
							 blockbuf, count);
					break;
				case EXTENSION_IFDB_TAG:
					prxcap->ifdb_present = true;
					hdmitx21_edid_parse_ifdb(prxcap, &blockbuf[offset]);
					break;
				default:
					break;
				}
			}
			offset += count + 1;
			break;

		case HDMI_EDID_BLOCK_TYPE_RESERVED:
			offset++;
			offset += count;
			break;

		case HDMI_EDID_BLOCK_TYPE_RESERVED2:
			offset++;
			offset += count;
			break;

		default:
			break;
		}
	}

	if (aud_flag == 0)
		hdmitx_edid_set_default_aud(hdev);

	edid_y420cmdb_postprocess(hdev);
	hdev->vic_count = prxcap->VIC_count;

	/* dtds in extended blocks */
	i = 0;
	offset = blockbuf[2] + i * 18;
	for ( ; (offset + 18) < 0x7f; i++) {
		edid_dtd_parsing(prxcap, &blockbuf[offset]);
		offset += 18;
	}

	if (vfpdb_offset)
		edid_parsingvfpdb(prxcap, vfpdb_offset);

	return 0;
}

static void hdmitx_edid_set_default_aud(struct hdmitx_dev *hdev)
{
	struct rx_cap *prxcap = &hdev->rxcap;

	/* if AUD_count not equal to 0, no need default value */
	if (prxcap->AUD_count)
		return;

	prxcap->AUD_count = 1;
	prxcap->RxAudioCap[0].audio_format_code = 1; /* PCM */
	prxcap->RxAudioCap[0].channel_num_max = 1; /* 2ch */
	prxcap->RxAudioCap[0].freq_cc = 7; /* 32/44.1/48 kHz */
	prxcap->RxAudioCap[0].cc3 = 1; /* 16bit */
}

/* for below cases:
 * for exception process: no CEA vic in parse result
 * DVI case(only one block), HDMI/HDCP CTS
 */
static void hdmitx_edid_set_default_vic(struct hdmitx_dev *hdmitx_device)
{
	struct rx_cap *prxcap = &hdmitx_device->rxcap;

	prxcap->VIC_count = 0x2;
	prxcap->VIC[0] = HDMI_3_720x480p60_16x9;
	prxcap->VIC[1] = HDMI_4_1280x720p60_16x9;
	prxcap->native_vic = HDMI_3_720x480p60_16x9;
	hdmitx_device->vic_count = prxcap->VIC_count;
	pr_info(EDID "set default vic\n");
}

#define PRINT_HASH(hash)

static int edid_hash_calc(u8 *hash, const char *data,
			  u32 len)
{
	return 1;
}

static int hdmitx_edid_search_IEEEOUI(char *buf)
{
	int i;

	for (i = 0; i < 0x180 - 2; i++) {
		if (buf[i] == 0x03 && buf[i + 1] == 0x0c &&
		    buf[i + 2] == 0x00)
			return 1;
	}
	return 0;
}

/* check EDID strictly */
static int edid_check_valid(u8 *buf)
{
	u32 chksum = 0;
	u32 i = 0;

	/* check block 0 first 8 bytes */
	if (buf[0] != 0 && buf[7] != 0)
		return 0;
	for (i = 1; i < 7; i++) {
		if (buf[i] != 0xff)
			return 0;
	}

	/* check block 0 checksum */
	for (chksum = 0, i = 0; i < 0x80; i++)
		chksum += buf[i];

	if ((chksum & 0xff) != 0)
		return 0;

	/* check Extension flag at block 0 */
	/* for DVI: there may be >= 0 cta block
	 * so it's normal to have only basic block
	 */
	if (buf[0x7e] == 0)
		return 1;

	/* check block 1 extension tag */
	if (!(buf[0x80] == 0x2 || buf[0x80] == 0xf0))
		return 0;

	/* check block 1 checksum */
	for (chksum = 0, i = 0x80; i < 0x100; i++)
		chksum += buf[i];

	if ((chksum & 0xff) != 0)
		return 0;

	return 1;
}

/* check the checksum for each sub block */
static int _check_edid_blk_chksum(u8 *block)
{
	u32 chksum = 0;
	u32 i = 0;

	for (chksum = 0, i = 0; i < 0x80; i++)
		chksum += block[i];
	if ((chksum & 0xff) != 0)
		return 0;
	else
		return 1;
}

/* check the first edid block */
static int _check_base_structure(u8 *buf)
{
	u32 i = 0;

	/* check block 0 first 8 bytes */
	if (buf[0] != 0 && buf[7] != 0)
		return 0;

	for (i = 1; i < 7; i++) {
		if (buf[i] != 0xff)
			return 0;
	}

	if (_check_edid_blk_chksum(buf) == 0)
		return 0;

	return 1;
}

/*
 * check the EDID
 * base structure: header, checksum
 * extension: the first non-zero byte, checksum
 */

int check21_dvi_hdmi_edid_valid(u8 *buf)
{
	int i;
	int blk_cnt = buf[0x7e] + 1;

	/* limit blk_cnt to EDID_MAX_BLOCK  */
	if (blk_cnt > EDID_MAX_BLOCK)
		blk_cnt = EDID_MAX_BLOCK;

	/* check block 0 */
	if (_check_base_structure(&buf[0]) == 0)
		return 0;

	if (blk_cnt == 1)
		return 1;

	/* check extension block 1 and more */
	for (i = 1; i < blk_cnt; i++) {
		if (buf[i * 0x80] == 0)
			return 0;
		if (_check_edid_blk_chksum(&buf[i * 0x80]) == 0)
			return 0;
	}

	return 1;
}

static void edid_manufacturedateparse(struct rx_cap *prxcap,
				      u8 *data)
{
	if (!data)
		return;

	/* week:
	 *	0: not specified
	 *	0x1~0x36: valid week
	 *	0x37~0xfe: reserved
	 *	0xff: model year is specified
	 */
	if (data[0] == 0 || (data[0] >= 0x37 && data[0] <= 0xfe))
		prxcap->manufacture_week = 0;
	else
		prxcap->manufacture_week = data[0];

	/* year:
	 *	0x0~0xf: reserved
	 *	0x10~0xff: year of manufacture,
	 *		or model year(if specified by week=0xff)
	 */
	prxcap->manufacture_year =
		(data[1] <= 0xf) ? 0 : data[1];
}

static void edid_versionparse(struct rx_cap *prxcap,
			      u8 *data)
{
	if (!data)
		return;

	/*
	 *	0x1: edid version 1
	 *	0x0,0x2~0xff: reserved
	 */
	prxcap->edid_version = (data[0] == 0x1) ? 1 : 0;

	/*
	 *	0x0~0x4: revision number
	 *	0x5~0xff: reserved
	 */
	prxcap->edid_revision = (data[1] < 0x5) ? data[1] : 0;
}

static void Edid_PhysicalSizeParse(struct rx_cap *prxcap,
				   u8 *data)
{
	if (data[0] != 0 && data[1] != 0) {
		/* Here the unit is cm, transfer to mm */
		prxcap->physical_width  = data[0] * 10;
		prxcap->physical_height = data[1] * 10;
	}
}

/* if edid block 0 are all zeros, then consider RX as HDMI device */
static int edid_zero_data(u8 *buf)
{
	int sum = 0;
	int i = 0;

	for (i = 0; i < 128; i++)
		sum += buf[i];

	if (sum == 0)
		return 1;
	else
		return 0;
}

static void dump_dtd_info(struct dtd *t)
{
	if (0) {
		pr_info(EDID "%s[%d]\n", __func__, __LINE__);
		pr_info(EDID "pixel_clock: %d\n", t->pixel_clock);
		pr_info(EDID "h_active: %d\n", t->h_active);
		pr_info(EDID "v_active: %d\n", t->v_active);
		pr_info(EDID "v_blank: %d\n", t->v_blank);
		pr_info(EDID "h_sync_offset: %d\n", t->h_sync_offset);
		pr_info(EDID "h_sync: %d\n", t->h_sync);
		pr_info(EDID "v_sync_offset: %d\n", t->v_sync_offset);
		pr_info(EDID "v_sync: %d\n", t->v_sync);
	}
}

static void edid_dtd_parsing(struct rx_cap *prxcap, u8 *data)
{
	const struct hdmi_timing *dtd_timing = NULL;
	struct dtd *t = &prxcap->dtd[prxcap->dtd_idx];

	/* if data[0-2] are zeroes, no need parse, and skip*/
	if (data[0] == 0 && data[1] == 0 && data[2] == 0)
		return;
	memset(t, 0, sizeof(struct dtd));
	t->pixel_clock = data[0] + (data[1] << 8);
	t->h_active = (((data[4] >> 4) & 0xf) << 8) + data[2];
	t->h_blank = ((data[4] & 0xf) << 8) + data[3];
	t->v_active = (((data[7] >> 4) & 0xf) << 8) + data[5];
	t->v_blank = ((data[7] & 0xf) << 8) + data[6];
	t->h_sync_offset = (((data[11] >> 6) & 0x3) << 8) + data[8];
	t->h_sync = (((data[11] >> 4) & 0x3) << 8) + data[9];
	t->v_sync_offset = (((data[11] >> 2) & 0x3) << 4) +
		((data[10] >> 4) & 0xf);
	t->v_sync = (((data[11] >> 0) & 0x3) << 4) + ((data[10] >> 0) & 0xf);
	t->h_image_size = (((data[14] >> 4) & 0xf) << 8) + data[12];
	t->v_image_size = ((data[14] & 0xf) << 8) + data[13];
	t->flags = data[17];
/*
 * Special handling of 1080i60hz, 1080i50hz
 */
	if (t->pixel_clock == 7425 && t->h_active == 1920 &&
	    t->v_active == 1080) {
		t->v_active = t->v_active / 2;
		t->v_blank = t->v_blank / 2;
	}
/*
 * Special handling of 480i60hz, 576i50hz
 */
	if ((((t->flags >> 1) & 0x3) == 0) && t->h_active == 1440) {
		if (t->pixel_clock == 2700) /* 576i50hz */
			goto next;
		if ((t->pixel_clock - 2700) < 10) /* 480i60hz */
			t->pixel_clock = 2702;
next:
		t->v_active = t->v_active / 2;
		t->v_blank = t->v_blank / 2;
	}
/*
 * call hdmitx21_match_dtd_timing() to check t is matched with VIC
 */
	dtd_timing = hdmitx21_match_dtd_timing(t);
	if (dtd_timing) {
		/* diff 4x3 and 16x9 mode */
		if (dtd_timing->vic == HDMI_6_720x480i60_4x3 ||
			dtd_timing->vic == HDMI_2_720x480p60_4x3 ||
			dtd_timing->vic == HDMI_21_720x576i50_4x3 ||
			dtd_timing->vic == HDMI_17_720x576p50_4x3) {
			if (abs(t->v_image_size * 100 / t->h_image_size - 3 * 100 / 4) <= 2)
				t->vic = dtd_timing->vic;
			else
				t->vic = dtd_timing->vic + 1;
		} else {
			t->vic = dtd_timing->vic;
		}
		prxcap->preferred_mode = prxcap->dtd[0].vic; /* Select dtd0 */
		pr_debug(EDID "get dtd%d vic: %d\n",
			prxcap->dtd_idx, t->vic);
		prxcap->dtd_idx++;
		if (t->vic < HDMITX_VESA_OFFSET)
			store_cea_idx(prxcap, t->vic);
		else
			store_vesa_idx(prxcap, t->vic);
	} else {
		dump_dtd_info(t);
	}
}

static void edid_check_pcm_declare(struct rx_cap *prxcap)
{
	int idx_pcm = 0;
	int i;

	if (!prxcap->AUD_count)
		return;

	/* Try to find more than 1 PCMs, RxAudioCap[0] is always basic audio */
	for (i = 1; i < prxcap->AUD_count; i++) {
		if (prxcap->RxAudioCap[i].audio_format_code ==
			prxcap->RxAudioCap[0].audio_format_code) {
			idx_pcm = i;
			break;
		}
	}

	/* Remove basic audio */
	if (idx_pcm) {
		for (i = 0; i < prxcap->AUD_count - 1; i++)
			memcpy(&prxcap->RxAudioCap[i],
			       &prxcap->RxAudioCap[i + 1],
			       sizeof(struct rx_audiocap));
		/* Clear the last audio declaration */
		memset(&prxcap->RxAudioCap[i], 0, sizeof(struct rx_audiocap));
		prxcap->AUD_count--;
	}
}

static bool is_4k60_supported(struct rx_cap *prxcap)
{
	int i = 0;

	if (!prxcap)
		return false;

	for (i = 0; (i < prxcap->VIC_count) && (i < VIC_MAX_NUM); i++) {
		if (((prxcap->VIC[i] & 0xff) == HDMI_96_3840x2160p50_16x9) ||
		    ((prxcap->VIC[i] & 0xff) == HDMI_97_3840x2160p60_16x9)) {
			return true;
		}
	}
	return false;
}

static void edid_descriptor_pmt(struct rx_cap *prxcap,
				struct vesa_standard_timing *t,
				u8 *data)
{
	struct hdmi_format_para *para = NULL;

	t->tmds_clk = data[0] + (data[1] << 8);
	t->hactive = data[2] + (((data[4] >> 4) & 0xf) << 8);
	t->hblank = data[3] + ((data[4] & 0xf) << 8);
	t->vactive = data[5] + (((data[7] >> 4) & 0xf) << 8);
	t->vblank = data[6] + ((data[7] & 0xf) << 8);
	para = hdmitx21_get_vesa_paras(t);
	if (para && (para->timing.vic < (HDMI_107_3840x2160p60_64x27 + 1))) {
		prxcap->native_vic = para->timing.vic;
		pr_debug("hdmitx: get PMT vic: %d\n", para->timing.vic);
	}
	if (para && para->timing.vic >= HDMITX_VESA_OFFSET)
		store_vesa_idx(prxcap, para->timing.vic);
}

static void edid_descriptor_pmt2(struct rx_cap *prxcap,
				 struct vesa_standard_timing *t,
				 u8 *data)
{
	struct hdmi_format_para *para = NULL;

	t->tmds_clk = data[0] + (data[1] << 8);
	t->hactive = data[2] + (((data[4] >> 4) & 0xf) << 8);
	t->hblank = data[3] + ((data[4] & 0xf) << 8);
	t->vactive = data[5] + (((data[7] >> 4) & 0xf) << 8);
	t->vblank = data[6] + ((data[7] & 0xf) << 8);
	para = hdmitx21_get_vesa_paras(t);
	if (para && para->timing.vic >= HDMITX_VESA_OFFSET)
		store_vesa_idx(prxcap, para->timing.vic);
}

static void edid_cvt_timing_3bytes(struct rx_cap *prxcap,
				   struct vesa_standard_timing *t,
				   const u8 *data)
{
	struct hdmi_format_para *para = NULL;

	t->hactive = ((data[0] + (((data[1] >> 4) & 0xf) << 8)) + 1) * 2;
	switch ((data[1] >> 2) & 0x3) {
	case 0:
		t->vactive = t->hactive * 3 / 4;
		break;
	case 1:
		t->vactive = t->hactive * 9 / 16;
		break;
	case 2:
		t->vactive = t->hactive * 5 / 8;
		break;
	case 3:
	default:
		t->vactive = t->hactive * 3 / 5;
		break;
	}
	switch ((data[2] >> 5) & 0x3) {
	case 0:
		t->vsync = 50;
		break;
	case 1:
		t->vsync = 60;
		break;
	case 2:
		t->vsync = 75;
		break;
	case 3:
	default:
		t->vsync = 85;
		break;
	}
	para = hdmitx21_get_vesa_paras(t);
	if (para)
		t->vesa_timing = para->timing.vic;
}

static void edid_cvt_timing(struct rx_cap *prxcap, u8 *data)
{
	int i;
	struct vesa_standard_timing t;

	for (i = 0; i < 4; i++) {
		memset(&t, 0, sizeof(struct vesa_standard_timing));
		edid_cvt_timing_3bytes(prxcap, &t, &data[i * 3]);
		if (t.vesa_timing) {
			if (t.vesa_timing >= HDMITX_VESA_OFFSET)
				store_vesa_idx(prxcap, t.vesa_timing);
		}
	}
}

static void check_dv_truly_support(struct hdmitx_dev *hdev, struct dv_info *dv)
{
	struct rx_cap *prxcap = &hdev->rxcap;
	u32 max_tmds_clk = 0;

	if (dv->ieeeoui == DV_IEEE_OUI && dv->ver <= 2) {
		/* check max tmds rate to determine if 4k60 DV can truly be
		 * supported.
		 */
		if (prxcap->Max_TMDS_Clock2) {
			max_tmds_clk = prxcap->Max_TMDS_Clock2 * 5;
		} else {
			/* Default min is 74.25 / 5 */
			if (prxcap->Max_TMDS_Clock1 < 0xf)
				prxcap->Max_TMDS_Clock1 = DEFAULT_MAX_TMDS_CLK;
			max_tmds_clk = prxcap->Max_TMDS_Clock1 * 5;
		}
		if (dv->ver == 0)
			dv->sup_2160p60hz = dv->sup_2160p60hz &&
						(max_tmds_clk >= 594);

		if (dv->ver == 1 && dv->length == 0xB) {
			if (dv->low_latency == 0x00) {
				/*standard mode */
				dv->sup_2160p60hz = dv->sup_2160p60hz &&
							(max_tmds_clk >= 594);
			} else if (dv->low_latency == 0x01) {
				/* both standard and LL are supported. 4k60 LL
				 * DV support should/can be determined using
				 * video formats supported inthe E-EDID as flag
				 * sup_2160p60hz might not be set.
				 */
				if ((dv->sup_2160p60hz ||
				     is_4k60_supported(prxcap)) &&
				    max_tmds_clk >= 594)
					dv->sup_2160p60hz = 1;
				else
					dv->sup_2160p60hz = 0;
			}
		}

		if (dv->ver == 1 && dv->length == 0xE)
			dv->sup_2160p60hz = dv->sup_2160p60hz &&
						(max_tmds_clk >= 594);

		if (dv->ver == 2) {
			/* 4k60 DV support should be determined using video
			 * formats supported in the EEDID as flag sup_2160p60hz
			 * is not applicable for VSVDB V2.
			 */
			if (is_4k60_supported(prxcap) && max_tmds_clk >= 594)
				dv->sup_2160p60hz = 1;
			else
				dv->sup_2160p60hz = 0;
		}
	}
}

//todo
int hdmitx21_find_philips(struct hdmitx_dev *hdev)
{
	int j;
	int length = sizeof(vendor_id) / sizeof(struct edid_venddat_t);

	for (j = 0; j < length; j++) {
		if (memcmp(&hdev->EDID_buf[8], &vendor_id[j],
			sizeof(struct edid_venddat_t)) == 0)
			return 1;
	}
	return 0;
}

/*
 * if the EDID is invalid, then set the fallback mode
 * Resolution & RefreshRate:
 * 1920x1080p60hz 16:9
 * 1280x720p60hz 16:9 (default)
 * 720x480p 16:9
 * ColorSpace: RGB
 * ColorDepth: 8bit
 */
static void edid_set_fallback_mode(struct hdmitx_dev *hdev)
{
	struct rx_cap *prxcap;
	struct vsdb_phyaddr *phyaddr;

	if (!hdev)
		return;

	phyaddr = &hdev->hdmi_info.vsdb_phy_addr;

	/* EDID extended blk chk error, set the 720p60, rgb,8bit */
	prxcap = &hdev->rxcap;
	prxcap->ieeeoui = HDMI_IEEE_OUI;

	/* set the default cec physical address as 0xffff */
	phyaddr->a = 0xf;
	phyaddr->b = 0xf;
	phyaddr->c = 0xf;
	phyaddr->d = 0xf;
	phyaddr->valid = 0;
	hdev->physical_addr = 0xffff;

	prxcap->Max_TMDS_Clock1 = DEFAULT_MAX_TMDS_CLK; /* 165MHZ / 5 */
	prxcap->native_Mode = 0; /* only RGB */
	prxcap->dc_y444 = 0; /* only 8bit */
	prxcap->VIC_count = 0x3;
	prxcap->VIC[0] = HDMI_16_1920x1080p60_16x9;
	prxcap->VIC[1] = HDMI_4_1280x720p60_16x9;
	prxcap->VIC[2] = HDMI_3_720x480p60_16x9;
	prxcap->native_vic = HDMI_4_1280x720p60_16x9;
	hdev->vic_count = prxcap->VIC_count;
	pr_info(EDID "set default vic 720p60hz\n");

	hdmitx_edid_set_default_aud(hdev);
}

static void _edid_parse_base_structure(struct hdmitx_dev *hdmitx_device,
	u8 *EDID_buf)
{
	u8 checksum;
	u8 zero_numbers;
	u8 cta_block_count;
	int i;
	struct rx_cap *prxcap = &hdmitx_device->rxcap;

	edid_parsingidmanufacturername(prxcap, &EDID_buf[8]);
	edid_parsingidproductcode(prxcap, &EDID_buf[0x0A]);
	edid_parsingidserialnumber(prxcap, &EDID_buf[0x0C]);

	edid_establishedtimings(prxcap, &EDID_buf[0x23]);

	edid_manufacturedateparse(prxcap, &EDID_buf[16]);

	edid_versionparse(prxcap, &EDID_buf[18]);

	Edid_PhysicalSizeParse(prxcap, &EDID_buf[21]);
	prxcap->blk0_chksum = EDID_buf[0x7F];

	cta_block_count = EDID_buf[0x7E];

	if (cta_block_count == 0) {
		pr_info(EDID "EDID BlockCount=0\n");
		/* DVI case judgement: only contains one block and
		 * checksum valid
		 */
		checksum = 0;
		zero_numbers = 0;
		for (i = 0; i < 128; i++) {
			checksum += EDID_buf[i];
			if (EDID_buf[i] == 0)
				zero_numbers++;
		}
		pr_info(EDID "edid blk0 checksum:%d ext_flag:%d\n",
			checksum, EDID_buf[0x7e]);
		if ((checksum & 0xff) == 0)
			prxcap->ieeeoui = 0;
		else
			prxcap->ieeeoui = HDMI_IEEE_OUI;
		if (zero_numbers > 120)
			prxcap->ieeeoui = HDMI_IEEE_OUI;
		hdmitx_edid_set_default_vic(hdmitx_device);
	}
}

int hdmitx21_edid_parse(struct hdmitx_dev *hdmitx_device)
{
	u8 cta_block_count;
	u8 *EDID_buf;
	int i;
	int idx[4];
	struct rx_cap *prxcap = &hdmitx_device->rxcap;
	struct dv_info *dv = &hdmitx_device->rxcap.dv_info;

	if (check21_dvi_hdmi_edid_valid(hdmitx_device->EDID_buf)) {
		EDID_buf = hdmitx_device->EDID_buf;
		hdmitx_device->edid_parsing = 1;
		memcpy(hdmitx_device->EDID_buf1, hdmitx_device->EDID_buf,
		       EDID_MAX_BLOCK * 128);
	} else {
		EDID_buf = hdmitx_device->EDID_buf1;
	}
	if (check21_dvi_hdmi_edid_valid(hdmitx_device->EDID_buf1))
		hdmitx_device->edid_parsing = 1;

	hdmitx_device->edid_ptr = EDID_buf;
	pr_debug(EDID "EDID Parser:\n");
	/* Calculate the EDID hash for special use */
	memset(hdmitx_device->EDID_hash, 0,
	       ARRAY_SIZE(hdmitx_device->EDID_hash));
	edid_hash_calc(hdmitx_device->EDID_hash, hdmitx_device->EDID_buf, 256);

	if (check21_dvi_hdmi_edid_valid(EDID_buf) == 0) {
		edid_set_fallback_mode(hdmitx_device);
		pr_info("set fallback mode\n");
		return 0;
	}
	if (_check_base_structure(EDID_buf))
		_edid_parse_base_structure(hdmitx_device, EDID_buf);

	cta_block_count = EDID_buf[0x7E];
	/* HFR-EEODB */
	if (cta_block_count == 1 && EDID_buf[128 + 4] == 0xe2 &&
		EDID_buf[128 + 5] == 0x78)
		cta_block_count = EDID_buf[128 + 6];
	/* limit cta_block_count to EDID_MAX_BLOCK - 1 */
	if (cta_block_count > EDID_MAX_BLOCK - 1)
		cta_block_count = EDID_MAX_BLOCK - 1;
	for (i = 1; i <= cta_block_count; i++) {
		if (EDID_buf[i * 0x80] == 0x02)
			hdmitx_edid_cta_block_parse(hdmitx_device, &EDID_buf[i * 0x80]);
	}

	edid_check_pcm_declare(&hdmitx_device->rxcap);
	/* move parts that may contain cea timing parse behind
	 * VDB parse, so that to not affect VDB index which
	 * will be used in Y420CMDB map
	 */
	edid_standardtiming(&hdmitx_device->rxcap, &EDID_buf[0x26], 8);
	edid_parseceatiming(&hdmitx_device->rxcap, &EDID_buf[0x36]);
/*
 * Because DTDs are not able to represent some Video Formats, which can be
 * represented as SVDs and might be preferred by Sinks, the first DTD in the
 * base EDID data structure and the first SVD in the first CEA Extension can
 * differ. When the first DTD and SVD do not match and the total number of
 * DTDs defining Native Video Formats in the whole EDID is zero, the first
 * SVD shall take precedence.
 */
	if (!prxcap->flag_vfpdb &&
	    prxcap->preferred_mode != prxcap->VIC[0] &&
	    prxcap->number_of_dtd == 0) {
		pr_info(EDID "change preferred_mode from %d to %d\n",
			prxcap->preferred_mode,	prxcap->VIC[0]);
		prxcap->preferred_mode = prxcap->VIC[0];
	}

	idx[0] = EDID_DETAILED_TIMING_DES_BLOCK0_POS;
	idx[1] = EDID_DETAILED_TIMING_DES_BLOCK1_POS;
	idx[2] = EDID_DETAILED_TIMING_DES_BLOCK2_POS;
	idx[3] = EDID_DETAILED_TIMING_DES_BLOCK3_POS;
	for (i = 0; i < 4; i++) {
		if ((EDID_buf[idx[i]]) && (EDID_buf[idx[i] + 1])) {
			struct vesa_standard_timing t;

			memset(&t, 0, sizeof(struct vesa_standard_timing));
			if (i == 0)
				edid_descriptor_pmt(prxcap, &t,
						    &EDID_buf[idx[i]]);
			if (i == 1)
				edid_descriptor_pmt2(prxcap, &t,
						     &EDID_buf[idx[i]]);
			continue;
		}
		switch (EDID_buf[idx[i] + 3]) {
		case TAG_STANDARD_TIMINGS:
			edid_standardtiming(prxcap, &EDID_buf[idx[i] + 5], 6);
			break;
		case TAG_CVT_TIMING_CODES:
			edid_cvt_timing(prxcap, &EDID_buf[idx[i] + 6]);
			break;
		case TAG_ESTABLISHED_TIMING_III:
			edid_standardtimingiii(prxcap, &EDID_buf[idx[i] + 6]);
			break;
		case TAG_RANGE_LIMITS:
			break;
		case TAG_DISPLAY_PRODUCT_NAME_STRING:
			edid_receiverproductnameparse(prxcap,
						      &EDID_buf[idx[i] + 5]);
			break;
		default:
			break;
		}
	}

	if (hdmitx_edid_search_IEEEOUI(&EDID_buf[128])) {
		prxcap->ieeeoui = HDMI_IEEE_OUI;
		pr_debug(EDID "find IEEEOUT\n");
	} else {
		prxcap->ieeeoui = 0x0;
		pr_info(EDID "not find IEEEOUT\n");
	}

	/* strictly DVI device judgement */
	/* valid EDID & no audio tag & no IEEEOUI */
	if (edid_check_valid(&EDID_buf[0]) &&
		!hdmitx_edid_search_IEEEOUI(&EDID_buf[128])) {
		prxcap->ieeeoui = 0x0;
		pr_info(EDID "sink is DVI device\n");
	} else {
		prxcap->ieeeoui = HDMI_IEEE_OUI;
	}
	if (edid_zero_data(EDID_buf))
		prxcap->ieeeoui = HDMI_IEEE_OUI;

	if (!prxcap->AUD_count && !prxcap->ieeeoui)
		hdmitx_edid_set_default_aud(hdmitx_device);
	/* CEA-861F 7.5.2  If only Basic Audio is supported,
	 * no Short Audio Descriptors are necessary.
	 */
	if (!prxcap->AUD_count && hdmitx_device->hdmi_info.support_basic_audio_flag)
		hdmitx_edid_set_default_aud(hdmitx_device);

	edid_save_checkvalue(EDID_buf, cta_block_count + 1, prxcap);

	if (!hdmitx_edid_check_valid_blocks(&EDID_buf[0])) {
		prxcap->ieeeoui = HDMI_IEEE_OUI;
		pr_info(EDID "Invalid edid, consider RX as HDMI device\n");
	}

	/* EDID parsing complete - check if 4k60/50 DV can be truly supported */
	dv = &prxcap->dv_info;
	check_dv_truly_support(hdmitx_device, dv);
	dv = &prxcap->dv_info2;
	check_dv_truly_support(hdmitx_device, dv);

	hdmitx_device->vend_id_hit = hdmitx21_find_philips(hdmitx_device);
	/* For some receivers, they don't claim the screen size
	 * and re-calculate it from the h/v image size from dtd
	 * the unit of screen size is cm, but the unit of image size is mm
	 */
	if (prxcap->physical_width == 0 || prxcap->physical_height == 0) {
		struct dtd *t = &prxcap->dtd[0];

		prxcap->physical_width  = t->h_image_size;
		prxcap->physical_height = t->v_image_size;
	}

	/* if edid are all zeroes, or no VIC, set default vic */
	if (edid_zero_data(EDID_buf) || prxcap->VIC_count == 0)
		hdmitx_edid_set_default_vic(hdmitx_device);
	if (prxcap->ieeeoui != HDMI_IEEE_OUI)
		hdmitx_device->physical_addr = 0xffff;

	return 0;
}

enum hdmi_vic hdmitx21_edid_vic_tab_map_vic(const char *disp_mode)
{
	const struct hdmi_timing *timing = NULL;
	enum hdmi_vic vic = HDMI_0_UNKNOWN;

	timing = hdmitx21_gettiming_from_name(disp_mode);
	if (timing)
		vic = timing->vic;

	return vic;
}

const char *hdmitx21_edid_vic_to_string(enum hdmi_vic vic)
{
	const struct hdmi_timing *timing = NULL;
	const char *disp_str = NULL;

	timing = hdmitx21_gettiming_from_vic(vic);
	if (timing)
		disp_str = timing->name;

	return disp_str;
}

bool is_vic_support_y420(struct hdmitx_dev *hdev, enum hdmi_vic vic)
{
	unsigned int i = 0;
	struct rx_cap *prxcap = &hdev->rxcap;
	bool ret = false;
	const struct hdmi_timing *timing = hdmitx21_gettiming_from_vic(vic);

	if (!timing)
		return ret;

	/* In Spec2.1 Table 7-34, greater than 2160p30hz will support y420 */
	if ((timing->v_active >= 2160 && timing->v_freq > 30000) ||
		timing->v_active >= 4320) {
		for (i = 0; i < Y420_VIC_MAX_NUM; i++) {
			if (prxcap->y420_vic[i]) {
				if (prxcap->y420_vic[i] == vic) {
					ret = true;
					break;
				}
			} else {
				ret = false;
				break;
			}
		}
	}

	return ret;
}

static bool hdmitx_check_4x3_16x9_mode(struct hdmitx_dev *hdev,
		enum hdmi_vic vic)
{
	bool flag = 0;
	int j;
	struct rx_cap *prxcap = NULL;

	prxcap = &hdev->rxcap;
	if (vic == HDMI_2_720x480p60_4x3 ||
		vic == HDMI_6_720x480i60_4x3 ||
		vic == HDMI_17_720x576p50_4x3 ||
		vic == HDMI_21_720x576i50_4x3) {
		for (j = 0; (j < prxcap->VIC_count) && (j < VIC_MAX_NUM); j++) {
			if ((vic + 1) == (prxcap->VIC[j] & 0xff)) {
				flag = 1;
				break;
			}
		}
	} else if (vic == HDMI_3_720x480p60_16x9 ||
			vic == HDMI_7_720x480i60_16x9 ||
			vic == HDMI_18_720x576p50_16x9 ||
			vic == HDMI_22_720x576i50_16x9) {
		for (j = 0; (j < prxcap->VIC_count) && (j < VIC_MAX_NUM); j++) {
			if ((vic - 1) == (prxcap->VIC[j] & 0xff)) {
				flag = 1;
				break;
			}
		}
	}
	return flag;
}

/* 0: VESA DSC 1.2a is not supported
 * 1: up to 1 slice and up to (340 MHz/K SliceAdjust) pixel clock per slice
 * 2: up to 2 slices and up to (340 MHz/K SliceAdjust) pixel clock per slice
 * 3: up to 4 slices and up to (340 MHz/K SliceAdjust) pixel clock per slice
 * 4: up to 8 slices and up to (340 MHz/K SliceAdjust) pixel clock per slice
 * 5: up to 8 slices and up to (400 MHz/K SliceAdjust) pixel clock per slice
 * 6: up to 12 slices and up to (400 MHz/K SliceAdjust) pixel clock per slice
 * 7: up to 16 slices and up to (400 MHz/K SliceAdjust) pixel clock per slice
 * 8~15: Reserved
 */
u8 dsc_max_slices_num[] = {
	0,
	1,
	2,
	4,
	8,
	8,
	12,
	16
};

#ifdef CONFIG_AMLOGIC_DSC
/* get the needed frl rate, refer to 2.1 spec table 7-37/38,
 * actually it may also need to check bpp
 */
enum frl_rate_enum get_dsc_frl_rate(enum dsc_encode_mode dsc_mode)
{
	enum frl_rate_enum frl_rate = FRL_RATE_MAX;

	switch (dsc_mode) {
	case DSC_RGB_3840X2160_60HZ:
	case DSC_YUV444_3840X2160_60HZ:
	case DSC_YUV422_3840X2160_60HZ:
	case DSC_YUV420_3840X2160_60HZ:
	case DSC_RGB_3840X2160_50HZ:
	case DSC_YUV444_3840X2160_50HZ:
	case DSC_YUV422_3840X2160_50HZ:
	case DSC_YUV420_3840X2160_50HZ:
		frl_rate = FRL_3G3L;
		break;
	case DSC_RGB_3840X2160_120HZ:
	case DSC_YUV444_3840X2160_120HZ:
	case DSC_RGB_3840X2160_100HZ:
	case DSC_YUV444_3840X2160_100HZ:
		frl_rate = FRL_6G3L;
		break;
	case DSC_YUV422_3840X2160_120HZ:
	case DSC_YUV420_3840X2160_120HZ:
	case DSC_YUV422_3840X2160_100HZ:
	case DSC_YUV420_3840X2160_100HZ:
		frl_rate = FRL_3G3L;
		break;

	case DSC_RGB_7680X4320_60HZ:
	case DSC_YUV444_7680X4320_60HZ:
		/* 6G4L is spec recommended, but actually it can't
		 * work on board, need to work under 8G4L
		 */
		frl_rate = FRL_6G4L;
		break;
	case DSC_YUV422_7680X4320_60HZ:
	case DSC_YUV420_7680X4320_60HZ:
		/* 6G3L is spec recommended, but actually it can't
		 * work on board, need to work under 6G4L
		 */
		frl_rate = FRL_6G3L;
		break;

	case DSC_RGB_7680X4320_50HZ:
	case DSC_YUV444_7680X4320_50HZ:
		frl_rate = FRL_6G4L;
		break;
	case DSC_YUV422_7680X4320_50HZ:
	case DSC_YUV420_7680X4320_50HZ:
		frl_rate = FRL_6G3L;
		break;

	case DSC_YUV444_7680X4320_30HZ: /* bpp = 12 */
	case DSC_RGB_7680X4320_30HZ: /* bpp = 12 */
		frl_rate = FRL_6G3L;
		break;
	case DSC_YUV422_7680X4320_30HZ: /* bpp = 7.375 */
	case DSC_YUV420_7680X4320_30HZ: /* bpp = 7.375 */
		/* 3G3L is spec recommended, but actually it can't
		 * work on board, need to work under 6G3L
		 */
		frl_rate = FRL_3G3L;
		break;

	case DSC_YUV444_7680X4320_25HZ: /* bpp = 12 */
	case DSC_RGB_7680X4320_25HZ: /* bpp = 12 */
	case DSC_YUV444_7680X4320_24HZ: /* bpp = 12 */
	case DSC_RGB_7680X4320_24HZ: /* bpp = 12 */
		frl_rate = FRL_6G3L;
		break;
	case DSC_YUV422_7680X4320_25HZ: /* bpp = 7.6875 */
	case DSC_YUV420_7680X4320_25HZ: /* bpp = 7.6875 */
	case DSC_YUV422_7680X4320_24HZ: /* bpp = 7.6875 */
	case DSC_YUV420_7680X4320_24HZ: /* bpp = 7.6875 */
		frl_rate = FRL_3G3L;
		break;
	case DSC_ENCODE_MAX:
	default:
		frl_rate = FRL_RATE_MAX;
		break;
	}
	return frl_rate;
}

static bool edid_check_dsc_support(struct hdmitx_dev *hdev, struct hdmi_format_para *para)
{
	enum dsc_encode_mode dsc_mode = DSC_ENCODE_MAX;
	u8 dsc_slice_num = 0;
	enum frl_rate_enum dsc_frl_rate = FRL_NONE;
	u32 bytes_target = 0;
	struct rx_cap *prxcap;

	if (!hdev || !para)
		return false;

	prxcap = &hdev->rxcap;
	if (prxcap->dsc_1p2 == 0)
		return false;

	/* check dsc color depth cap */
	if (para->cd == COLORDEPTH_30B &&
		!prxcap->dsc_10bpc)
		return false;
	else if (para->cd == COLORDEPTH_36B &&
		!prxcap->dsc_12bpc)
		return false;
	/* check dsc color space cap */
	if (para->cs == HDMI_COLORSPACE_YUV420 &&
		!prxcap->dsc_native_420)
		return false;
	/* driver not support current dsc mode */
	dsc_mode = dsc_enc_confirm_mode(para->timing.h_active,
		para->timing.v_active, para->timing.v_freq, para->cs);
	if (dsc_mode == DSC_ENCODE_MAX)
		return false;
	if (hdev->dsc_policy == 0) {
		/* force not support below 12bit format temporarily */
		switch (dsc_mode) {
		/* 4k120hz */
		case DSC_RGB_3840X2160_120HZ:
		case DSC_YUV444_3840X2160_120HZ:
		/* 4k100hz */
		case DSC_RGB_3840X2160_100HZ:
		case DSC_YUV444_3840X2160_100HZ:
		/* 8k60hz */
		case DSC_RGB_7680X4320_60HZ:
		case DSC_YUV444_7680X4320_60HZ:
		/* 8k50hz */
		case DSC_RGB_7680X4320_50HZ:
		case DSC_YUV444_7680X4320_50HZ:
		/* 8k24hz */
		case DSC_RGB_7680X4320_24HZ:
		case DSC_YUV444_7680X4320_24HZ:
		/* 8k25hz */
		case DSC_RGB_7680X4320_25HZ:
		case DSC_YUV444_7680X4320_25HZ:
		/* 8k30hz */
		case DSC_RGB_7680X4320_30HZ:
		case DSC_YUV444_7680X4320_30HZ:
			if (para->cd == COLORDEPTH_36B)
				return false;
			break;
		default:
			break;
		}
	}
	dsc_slice_num = dsc_get_slice_num(dsc_mode);
	/* slice num exceed rx cap */
	if (dsc_slice_num == 0 ||
		dsc_slice_num > dsc_max_slices_num[prxcap->dsc_max_slices])
		return false;
	/* note: pixel clock per slice not checked, assume
	 * it's always within rx cap
	 */
	/* check dsc frl rate within rx cap */
	dsc_frl_rate = get_dsc_frl_rate(dsc_mode);
	if (dsc_frl_rate == FRL_RATE_MAX ||
		dsc_frl_rate > prxcap->dsc_max_frl_rate ||
		dsc_frl_rate > prxcap->max_frl_rate)
		return false;

	/* 2.1 spec table 6-56, if Bytes Target is greater than
	 * the value indicated by DSC_TotalChunkKBytes (see Sections
	 * 7.7.1 and 7.7.4.2), then the configuration is not
	 * supported with Compressed Video Transport.
	 */
	bytes_target = dsc_get_bytes_target_by_mode(dsc_mode);
	if (bytes_target > (prxcap->dsc_total_chunk_bytes + 1) * 1024)
		return false;

	return true;
}
#endif

/* For some TV's EDID, there maybe exist some information ambiguous.
 * Such as EDID declare support 2160p60hz(Y444 8bit), but no valid
 * Max_TMDS_Clock2 to indicate that it can support 5.94G signal.
 */
bool hdmitx21_edid_check_valid_mode(struct hdmitx_dev *hdev,
				  struct hdmi_format_para *para)
{
	bool valid = 0;
	struct rx_cap *prxcap = NULL;
	const struct dv_info *dv = &hdev->rxcap.dv_info;
	u32 rx_max_tmds_clk = 0;
	u32 calc_tmds_clk = 0;
	int i = 0;
	int svd_flag = 0;
	int must_frl_flag = 0;
	/* Default max color depth is 24 bit */
	enum hdmi_color_depth rx_y444_max_dc = COLORDEPTH_24B;
	enum hdmi_color_depth rx_rgb_max_dc = COLORDEPTH_24B;
	u32 rx_frl_bandwidth = 0;
	u32 tx_frl_bandwidth = 0;
	/* maximum supported bandwidth of soc */
	u32 tx_bandwidth_cap = 0;
	const struct hdmi_timing *timing;

	if (!hdev || !para)
		return 0;

	prxcap = &hdev->rxcap;

	/* exclude such as: 2160p60hz YCbCr444 10bit */
	switch (para->timing.vic) {
	case HDMI_96_3840x2160p50_16x9:
	case HDMI_97_3840x2160p60_16x9:
	case HDMI_101_4096x2160p50_256x135:
	case HDMI_102_4096x2160p60_256x135:
	case HDMI_106_3840x2160p50_64x27:
	case HDMI_107_3840x2160p60_64x27:
		if (para->cs == HDMI_COLORSPACE_RGB ||
		    para->cs == HDMI_COLORSPACE_YUV444)
			if (para->cd != COLORDEPTH_24B &&
				(prxcap->max_frl_rate == FRL_NONE ||
				hdev->tx_max_frl_rate == FRL_NONE))
				return 0;
		break;
	case HDMI_7_720x480i60_16x9:
	case HDMI_22_720x576i50_16x9:
		if (para->cs == HDMI_COLORSPACE_YUV422)
			return 0;
	default:
		break;
	}

	/* DVI case, only 8bit */
	if (prxcap->ieeeoui != HDMI_IEEE_OUI) {
		if (para->cd != COLORDEPTH_24B)
			return 0;
	}

	/* target mode is not contained at RX SVD */
	for (i = 0; (i < prxcap->VIC_count) && (i < VIC_MAX_NUM); i++) {
		if ((para->timing.vic & 0xff) == (prxcap->VIC[i] & 0xff)) {
			svd_flag = 1;
			break;
		} else if (hdmitx_check_4x3_16x9_mode(hdev, para->timing.vic & 0xff)) {
			svd_flag = 1;
			break;
		}
	}
	if (svd_flag == 0)
		return 0;

	/* Get RX Max_TMDS_Clock */
	if (prxcap->Max_TMDS_Clock2) {
		rx_max_tmds_clk = prxcap->Max_TMDS_Clock2 * 5;
	} else {
		/* Default min is 74.25 / 5 */
		if (prxcap->Max_TMDS_Clock1 < 0xf)
			prxcap->Max_TMDS_Clock1 = DEFAULT_MAX_TMDS_CLK;
		rx_max_tmds_clk = prxcap->Max_TMDS_Clock1 * 5;
	}

	calc_tmds_clk = para->tmds_clk / 1000;
	rx_frl_bandwidth = get_frl_bandwidth(prxcap->max_frl_rate);
	timing = hdmitx21_gettiming_from_vic(para->timing.vic);
	if (!timing)
		return 0;

	/* more than 4k60 must use frl mode */
	if (timing->h_active > 4096 || timing->v_active > 2160 ||
	timing->v_freq == 48000 || calc_tmds_clk > 594 ||
	timing->pixel_freq / 1000 > 600)
		must_frl_flag = 1;

	if (prxcap->max_frl_rate == FRL_NONE) {
		if (must_frl_flag)
			return 0;
	}

	if (hdev->tx_max_frl_rate == FRL_NONE) {
		if (must_frl_flag)
			return 0;
		/* Used for S1A to judge whether it exceeds the chip support */
		/* if (hdev->data.chip_type == MESON_CPU_ID_S1A) { */
		/* tx_bandwidth_cap = 225; */
		/* if (calc_tmds_clk > tx_bandwidth_cap) */
		/*	return 0; */
		/* } */
		if (calc_tmds_clk > rx_max_tmds_clk)
			return 0;
	} else {
#ifdef CONFIG_AMLOGIC_DSC
		if (hdev->dsc_policy == 1) {
			if (edid_check_dsc_support(hdev, para))
				return 1;
		} else if (hdev->dsc_policy == 2) {
			/* for debug test */
			return 1;
		}
#endif
		if (!must_frl_flag) {
			/* used TMDS, calc_tmds_clk must less than 594, not
			 * need repeat judgment(calc_tmds_clk < tx_bandwidth_cap)
			 */
			if (calc_tmds_clk > rx_max_tmds_clk)
				return 0;
		} else {
			/* try to check if able to run under FRL mode */
			/* tx_frl_bandwidth = timing->pixel_freq / 1000 * 24 * 1.122 */
			tx_frl_bandwidth = calc_frl_bandwidth(timing->pixel_freq / 1000,
				para->cs, para->cd);
			tx_bandwidth_cap = get_frl_bandwidth(hdev->tx_max_frl_rate);
			if (prxcap->dsc_1p2 == 0) {
				if (tx_frl_bandwidth > tx_bandwidth_cap)
					return 0;
				else if (tx_frl_bandwidth > rx_frl_bandwidth)
					return 0;
			} else {
				if (tx_frl_bandwidth <= tx_bandwidth_cap &&
					tx_frl_bandwidth <= rx_frl_bandwidth)
					; // non-dsc bandwidth is within cap, continue check
#ifdef CONFIG_AMLOGIC_DSC
				else if (hdev->dsc_policy == 3) //forcely filter out dsc mode output
					return 0;
				else if (!edid_check_dsc_support(hdev, para))
					return 0;
#else
				else
					return 0;
#endif
			}
			valid = 1;
		}
	}

	if (para->cs == HDMI_COLORSPACE_YUV444) {
		/* Rx may not support Y444 */
		if (!(prxcap->native_Mode & (1 << 5)))
			return 0;
		if (prxcap->dc_y444 && (prxcap->dc_30bit ||
					dv->sup_10b_12b_444 == 0x1))
			rx_y444_max_dc = COLORDEPTH_30B;
		if (prxcap->dc_y444 && (prxcap->dc_36bit ||
					dv->sup_10b_12b_444 == 0x2))
			rx_y444_max_dc = COLORDEPTH_36B;
		if (para->cd <= rx_y444_max_dc)
			valid = 1;
		else
			valid = 0;
		return valid;
	}
	if (para->cs == HDMI_COLORSPACE_YUV422) {
		/* Rx may not support Y422 */
		if (!(prxcap->native_Mode & (1 << 4)))
			return 0;
		return 1;
	}
	if (para->cs == HDMI_COLORSPACE_RGB) {
		/* Always assume RX supports RGB444 */
		if (prxcap->dc_30bit || dv->sup_10b_12b_444 == 0x1)
			rx_rgb_max_dc = COLORDEPTH_30B;
		if (prxcap->dc_36bit || dv->sup_10b_12b_444 == 0x2)
			rx_rgb_max_dc = COLORDEPTH_36B;
		if (para->cd <= rx_rgb_max_dc)
			valid = 1;
		else
			valid = 0;
		return valid;
	}
	if (para->cs == HDMI_COLORSPACE_YUV420) {
		if (!is_vic_support_y420(hdev, para->timing.vic))
			return 0;
		if (!prxcap->dc_30bit_420)
			if (para->cd == COLORDEPTH_30B)
				return 0;
		if (!prxcap->dc_36bit_420)
			if (para->cd == COLORDEPTH_36B)
				return 0;
		valid = 1;
	}

	return valid;
}

static bool check_sd(struct rx_cap *prxcap, const char *_mode,
	const char *mode, const unsigned int _vic,
	unsigned int *vic)
{
	int i;
	bool check1 = 0;
	bool check2 = 0;

	if (strcmp(_mode, mode) == 0) {
		for (i = 0 ; i < prxcap->VIC_count ; i++) {
			if (prxcap->VIC[i] == _vic)
				check1 = 1;
		}
		for (i = 0 ; i < prxcap->VIC_count ; i++) {
			if (prxcap->VIC[i] == (_vic + 1))
				check2 = 1;
		}
		if (check1 == 1 && check2 == 0) {
			*vic = _vic;
			return 1;
		}
	}

	return 0;
}

/* For some TV only support 480p 4:3,
 *then consider it also supports in disp_cap
 */
static bool is_sink_only_sd_4x3(struct hdmitx_dev *hdev,
	const char *mode, unsigned int *vic)
{
	struct rx_cap *prxcap = &hdev->rxcap;

	if (check_sd(prxcap, "480i60hz", mode, HDMI_6_720x480i60_4x3, vic))
		return 1;
	if (check_sd(prxcap, "480p60hz", mode, HDMI_2_720x480p60_4x3, vic))
		return 1;
	if (check_sd(prxcap, "576i50hz", mode, HDMI_21_720x576i50_4x3, vic))
		return 1;
	if (check_sd(prxcap, "576p50hz", mode, HDMI_17_720x576p50_4x3, vic))
		return 1;

	return 0;
}

/* force_flag: 0 means check with RX's edid */
/* 1 means no check which RX's edid */
enum hdmi_vic hdmitx21_edid_get_VIC(struct hdmitx_dev *hdev,
				  const char *disp_mode,
				  char force_flag)
{
	struct rx_cap *prxcap = &hdev->rxcap;
	int  j;
	enum hdmi_vic vic = hdmitx21_edid_vic_tab_map_vic(disp_mode);
	enum hdmi_vic vesa_vic;

	if (vic >= HDMITX_VESA_OFFSET)
		vesa_vic = vic;
	else
		vesa_vic = HDMI_0_UNKNOWN;
	if (vic != HDMI_0_UNKNOWN) {
		if (force_flag == 0) {
			for (j = 0; j < prxcap->VIC_count; j++) {
				if (prxcap->VIC[j] == vic)
					break;
			}
			if (j >= prxcap->VIC_count)
				vic = HDMI_0_UNKNOWN;
		}
		/* if TV only supports 480p/2, add 480p60hz as well */
		if (is_sink_only_sd_4x3(hdev, disp_mode, &vic))
			pr_info("hdmitx: find SD only 4x3\n");
	}
	return vic;
}

/* Clear HDMI Hardware Module EDID RAM and EDID Buffer */
void hdmitx21_edid_ram_buffer_clear(struct hdmitx_dev *hdmitx_device)
{
	u32 i = 0;

	if (hdmitx_device->forced_edid)
		return;

	/* Clear HDMI Hardware Module EDID RAM */
	hdmitx_device->hwop.cntlddc(hdmitx_device, DDC_EDID_CLEAR_RAM, 0);

	/* Clear EDID Buffer */
	for (i = 0; i < EDID_MAX_BLOCK * 128; i++)
		hdmitx_device->EDID_buf[i] = 0;
	for (i = 0; i < EDID_MAX_BLOCK * 128; i++)
		hdmitx_device->EDID_buf1[i] = 0;
}

static void hdmitx21_phy_addr_clear(struct hdmitx_dev *hdmitx_device)
{
	if (!hdmitx_device)
		return;

	hdmitx_device->hdmi_info.vsdb_phy_addr.a = 0;
	hdmitx_device->hdmi_info.vsdb_phy_addr.b = 0;
	hdmitx_device->hdmi_info.vsdb_phy_addr.c = 0;
	hdmitx_device->hdmi_info.vsdb_phy_addr.d = 0;
	hdmitx_device->hdmi_info.vsdb_phy_addr.valid = 0;
}

/* Clear the Parse result of HDMI Sink's EDID. */
void hdmitx21_edid_clear(struct hdmitx_dev *hdmitx_device)
{
	char tmp[2] = {0};
	struct rx_cap *prxcap = &hdmitx_device->rxcap;

	memset(prxcap, 0, sizeof(struct rx_cap));

	/* Note: in most cases, we think that rx is tv and the default
	 * IEEEOUI is HDMI Identifier
	 */
	prxcap->ieeeoui = HDMI_IEEE_OUI;

	hdmitx_device->vic_count = 0;
	memset(&hdmitx_device->EDID_hash[0], 0,
	       sizeof(hdmitx_device->EDID_hash));
	hdmitx_device->edid_parsing = 0;
	hdmitx_edid_set_default_aud(hdmitx_device);
	rx_set_hdr_lumi(&tmp[0], 2);
	hdmitx21_phy_addr_clear(hdmitx_device);
	//rx_set_receiver_edid(&tmp[0], 2);
	/* clear info */
	hdmitx_device->hdmi_info.support_underscan_flag = 0;
	hdmitx_device->hdmi_info.support_basic_audio_flag = 0;
	hdmitx_device->hdmi_info.support_ycbcr444_flag = 0;
	hdmitx_device->hdmi_info.support_ycbcr422_flag = 0;
}

/*
 * print one block data of edid
 */
#define TMP_EDID_BUF_SIZE	(256 + 8)
static void hdmitx_edid_blk_print(u8 *blk, u32 blk_idx)
{
	u32 i, pos;
	u8 *tmp_buf = NULL;

	tmp_buf = kmalloc(TMP_EDID_BUF_SIZE, GFP_KERNEL);
	if (!tmp_buf)
		return;

	memset(tmp_buf, 0, TMP_EDID_BUF_SIZE);
	pr_info(EDID "blk%d raw data\n", blk_idx);
	for (i = 0, pos = 0; i < 128; i++) {
		pos += sprintf(tmp_buf + pos, "%02x", blk[i]);
		if (((i + 1) & 0x1f) == 0)    /* print 32bytes a line */
			pos += sprintf(tmp_buf + pos, "\n");
	}
	pos += sprintf(tmp_buf + pos, "\n");
	pr_info(EDID "\n%s\n", tmp_buf);
	kfree(tmp_buf);
}

/*
 * check EDID buf contains valid block numbers
 */
static u32 hdmitx_edid_check_valid_blocks(u8 *buf)
{
	u32 valid_blk_no = 0;
	u32 i = 0, j = 0;
	u32 tmp_chksum = 0;

	for (j = 0; j < EDID_MAX_BLOCK; j++) {
		for (i = 0; i < 128; i++)
			tmp_chksum += buf[i + j * 128];
		if (tmp_chksum != 0) {
			valid_blk_no++;
			if ((tmp_chksum & 0xff) == 0)
				pr_debug(EDID "check sum valid\n");
			else
				pr_info(EDID "check sum invalid\n");
		}
		tmp_chksum = 0;
	}
	return valid_blk_no;
}

/*
 * suppose DDC read EDID two times successfully,
 * then compare EDID_buf and EDID_buf1.
 * if same, just print out EDID_buf raw data, else print out 2 buffers
 */
void hdmitx21_edid_buf_compare_print(struct hdmitx_dev *hdmitx_device)
{
	u32 i = 0;
	u32 err_no = 0;
	u8 *buf0 = hdmitx_device->EDID_buf;
	u8 *buf1 = hdmitx_device->EDID_buf1;
	u32 valid_blk_no = 0;
	u32 blk_idx = 0;

	for (i = 0; i < EDID_MAX_BLOCK * 128; i++) {
		if (buf0[i] != buf1[i])
			err_no++;
	}

	if (err_no == 0) {
		/* calculate valid edid block numbers */
		valid_blk_no = hdmitx_edid_check_valid_blocks(buf0);

		if (valid_blk_no == 0) {
			pr_info(EDID "raw data are all zeroes\n");
		} else {
			for (blk_idx = 0; blk_idx < valid_blk_no; blk_idx++)
				hdmitx_edid_blk_print(&buf0[blk_idx * 128],
						      blk_idx);
		}
	} else {
		pr_info(EDID "%d errors between two reading\n", err_no);
		valid_blk_no = hdmitx_edid_check_valid_blocks(buf0);
		for (blk_idx = 0; blk_idx < valid_blk_no; blk_idx++)
			hdmitx_edid_blk_print(&buf0[blk_idx * 128], blk_idx);

		valid_blk_no = hdmitx_edid_check_valid_blocks(buf1);
		for (blk_idx = 0; blk_idx < valid_blk_no; blk_idx++)
			hdmitx_edid_blk_print(&buf1[blk_idx * 128], blk_idx);
	}
}

int hdmitx21_edid_dump(struct hdmitx_dev *hdmitx_device, char *buffer,
		     int buffer_len)
{
	int i, pos = 0;
	struct rx_cap *prxcap = &hdmitx_device->rxcap;

	pos += snprintf(buffer + pos, buffer_len - pos,
		"Rx Manufacturer Name: %s\n", prxcap->IDManufacturerName);
	pos += snprintf(buffer + pos, buffer_len - pos,
		"Rx Product Code: %02x%02x\n",
		prxcap->IDProductCode[0],
		prxcap->IDProductCode[1]);
	pos += snprintf(buffer + pos, buffer_len - pos,
		"Rx Serial Number: %02x%02x%02x%02x\n",
		prxcap->IDSerialNumber[0],
		prxcap->IDSerialNumber[1],
		prxcap->IDSerialNumber[2],
		prxcap->IDSerialNumber[3]);
	pos += snprintf(buffer + pos, buffer_len - pos,
		"Rx Product Name: %s\n", prxcap->ReceiverProductName);

	pos += snprintf(buffer + pos, buffer_len - pos,
		"Manufacture Week: %d\n", prxcap->manufacture_week);
	pos += snprintf(buffer + pos, buffer_len - pos,
		"Manufacture Year: %d\n", prxcap->manufacture_year + 1990);

	pos += snprintf(buffer + pos, buffer_len - pos,
		"Physical size(mm): %d x %d\n",
		prxcap->physical_width, prxcap->physical_height);

	pos += snprintf(buffer + pos, buffer_len - pos,
		"EDID Version: %d.%d\n",
		prxcap->edid_version, prxcap->edid_revision);

	pos += snprintf(buffer + pos, buffer_len - pos,
		"EDID block number: 0x%x\n", hdmitx_device->EDID_buf[0x7e]);
	pos += snprintf(buffer + pos, buffer_len - pos,
		"blk0 chksum: 0x%02x\n", prxcap->blk0_chksum);

	pos += snprintf(buffer + pos, buffer_len - pos,
		"Source Physical Address[a.b.c.d]: %x.%x.%x.%x\n",
		hdmitx_device->hdmi_info.vsdb_phy_addr.a,
		hdmitx_device->hdmi_info.vsdb_phy_addr.b,
		hdmitx_device->hdmi_info.vsdb_phy_addr.c,
		hdmitx_device->hdmi_info.vsdb_phy_addr.d);

	// TODO native_vic2
	pos += snprintf(buffer + pos, buffer_len - pos,
		"native Mode %x, VIC (native %d):\n",
		prxcap->native_Mode, prxcap->native_vic);

	pos += snprintf(buffer + pos, buffer_len - pos,
		"ColorDeepSupport %x\n", prxcap->ColorDeepSupport);

	for (i = 0; i < prxcap->VIC_count ; i++) {
		pos += snprintf(buffer + pos, buffer_len - pos, "%d ",
		prxcap->VIC[i]);
	}
	pos += snprintf(buffer + pos, buffer_len - pos, "\n");
	pos += snprintf(buffer + pos, buffer_len - pos,
		"Audio {format, channel, freq, cce}\n");
	for (i = 0; i < prxcap->AUD_count; i++) {
		pos += snprintf(buffer + pos, buffer_len - pos,
			"{%d, %d, %x, %x}\n",
			prxcap->RxAudioCap[i].audio_format_code,
			prxcap->RxAudioCap[i].channel_num_max,
			prxcap->RxAudioCap[i].freq_cc,
			prxcap->RxAudioCap[i].cc3);
	}
	pos += snprintf(buffer + pos, buffer_len - pos,
		"Speaker Allocation: %x\n", prxcap->RxSpeakerAllocation);
	pos += snprintf(buffer + pos, buffer_len - pos,
		"Vendor: 0x%x ( %s device)\n",
		prxcap->ieeeoui, (prxcap->ieeeoui) ? "HDMI" : "DVI");

	pos += snprintf(buffer + pos, buffer_len - pos,
		"MaxTMDSClock1 %d MHz\n", prxcap->Max_TMDS_Clock1 * 5);

	if (prxcap->hf_ieeeoui) {
		pos +=
		snprintf(buffer + pos,
			 buffer_len - pos, "Vendor2: 0x%x\n",
			prxcap->hf_ieeeoui);
		pos += snprintf(buffer + pos, buffer_len - pos,
			"MaxTMDSClock2 %d MHz\n", prxcap->Max_TMDS_Clock2 * 5);
	}

	if (prxcap->allm)
		pos += snprintf(buffer + pos, buffer_len - pos, "ALLM: %x\n",
				prxcap->allm);

	if (prxcap->cnc3)
		pos += snprintf(buffer + pos, buffer_len - pos, "Game/CNC3: %x\n",
				prxcap->cnc3);

	pos += snprintf(buffer + pos, buffer_len - pos, "vLatency: ");
	if (prxcap->vLatency == LATENCY_INVALID_UNKNOWN)
		pos += snprintf(buffer + pos, buffer_len - pos,
				" Invalid/Unknown\n");
	else if (prxcap->vLatency == LATENCY_NOT_SUPPORT)
		pos += snprintf(buffer + pos, buffer_len - pos,
			" UnSupported\n");
	else
		pos += snprintf(buffer + pos, buffer_len - pos,
			" %d\n", prxcap->vLatency);

	pos += snprintf(buffer + pos, buffer_len - pos, "aLatency: ");
	if (prxcap->aLatency == LATENCY_INVALID_UNKNOWN)
		pos += snprintf(buffer + pos, buffer_len - pos,
				" Invalid/Unknown\n");
	else if (prxcap->aLatency == LATENCY_NOT_SUPPORT)
		pos += snprintf(buffer + pos, buffer_len - pos,
			" UnSupported\n");
	else
		pos += snprintf(buffer + pos, buffer_len - pos, " %d\n",
			prxcap->aLatency);

	pos += snprintf(buffer + pos, buffer_len - pos, "i_vLatency: ");
	if (prxcap->i_vLatency == LATENCY_INVALID_UNKNOWN)
		pos += snprintf(buffer + pos, buffer_len - pos,
				" Invalid/Unknown\n");
	else if (prxcap->i_vLatency == LATENCY_NOT_SUPPORT)
		pos += snprintf(buffer + pos, buffer_len - pos,
			" UnSupported\n");
	else
		pos += snprintf(buffer + pos, buffer_len - pos, " %d\n",
			prxcap->i_vLatency);

	pos += snprintf(buffer + pos, buffer_len - pos, "i_aLatency: ");
	if (prxcap->i_aLatency == LATENCY_INVALID_UNKNOWN)
		pos += snprintf(buffer + pos, buffer_len - pos,
				" Invalid/Unknown\n");
	else if (prxcap->i_aLatency == LATENCY_NOT_SUPPORT)
		pos += snprintf(buffer + pos, buffer_len - pos,
			" UnSupported\n");
	else
		pos += snprintf(buffer + pos, buffer_len - pos, " %d\n",
			prxcap->i_aLatency);

	if (prxcap->colorimetry_data)
		pos += snprintf(buffer + pos, buffer_len - pos,
			"ColorMetry: 0x%x\n", prxcap->colorimetry_data);
	pos += snprintf(buffer + pos, buffer_len - pos, "SCDC: %x\n",
		prxcap->scdc_present);
	pos += snprintf(buffer + pos, buffer_len - pos, "RR_Cap: %x\n",
		prxcap->scdc_rr_capable);
	pos += snprintf(buffer + pos, buffer_len - pos, "FRL_RATE: %x\n",
		prxcap->max_frl_rate);
	pos +=
	snprintf(buffer + pos, buffer_len - pos, "LTE_340M_Scramble: %x\n",
		 prxcap->lte_340mcsc_scramble);

	/* dsc capability */
	pos += snprintf(buffer + pos, buffer_len - pos, "dsc_10bpc: %d\n",
		 prxcap->dsc_10bpc);
	pos += snprintf(buffer + pos, buffer_len - pos, "dsc_12bpc: %d\n",
		 prxcap->dsc_12bpc);
	pos += snprintf(buffer + pos, buffer_len - pos, "dsc_16bpc: %d\n",
		 prxcap->dsc_16bpc);
	pos += snprintf(buffer + pos, buffer_len - pos, "dsc_all_bpp: %d\n",
		 prxcap->dsc_all_bpp);
	pos += snprintf(buffer + pos, buffer_len - pos, "dsc_native_420: %d\n",
		 prxcap->dsc_native_420);
	pos += snprintf(buffer + pos, buffer_len - pos, "dsc_1p2: %d\n",
		 prxcap->dsc_1p2);
	pos += snprintf(buffer + pos, buffer_len - pos, "dsc_max_slices: 0x%x(%d slices)\n",
		 prxcap->dsc_max_slices, dsc_max_slices_num[prxcap->dsc_max_slices]);
	pos += snprintf(buffer + pos, buffer_len - pos, "dsc_max_frl_rate: 0x%x\n",
		 prxcap->dsc_max_frl_rate);
	pos += snprintf(buffer + pos, buffer_len - pos, "dsc_total_chunk_bytes: 0x%x\n",
		 prxcap->dsc_total_chunk_bytes);

	if (prxcap->dv_info.ieeeoui == DOVI_IEEEOUI)
		pos += snprintf(buffer + pos, buffer_len - pos,
			"  DolbyVision%d", prxcap->dv_info.ver);
	if (prxcap->hdr_info2.hdr_support)
		pos += snprintf(buffer + pos, buffer_len - pos, "  HDR/%d",
			prxcap->hdr_info2.hdr_support);
	if (prxcap->hdr_info.sbtm_info.sbtm_support)
		pos += snprintf(buffer + pos, buffer_len - pos, "  SBTM");
	if (prxcap->dc_y444 || prxcap->dc_30bit || prxcap->dc_30bit_420)
		pos += snprintf(buffer + pos, buffer_len - pos, "  DeepColor");
	pos += snprintf(buffer + pos, buffer_len - pos, "\n");
	pos += snprintf(buffer + pos, buffer_len - pos, "additional_vsif_num: %d\n",
			prxcap->additional_vsif_num);
	pos += snprintf(buffer + pos, buffer_len - pos, "ifdb_present: %d\n",
			prxcap->ifdb_present);
	/* for checkvalue which maybe used by application to adjust
	 * whether edid is changed
	 */
	pos += snprintf(buffer + pos, buffer_len - pos,
		"checkvalue: 0x%02x%02x%02x%02x\n",
			edid_checkvalue[0],
			edid_checkvalue[1],
			edid_checkvalue[2],
			edid_checkvalue[3]);

	return pos;
}

bool hdmitx21_edid_only_support_sd(struct hdmitx_dev *hdev)
{
	struct rx_cap *prxcap = &hdev->rxcap;
	enum hdmi_vic vic;
	u32 i, j;
	bool only_support_sd = true;
	/* EDID of SL8800 equipment only support below formats */
	static enum hdmi_vic sd_fmt[] = {
		1, 3, 4, 17, 18
	};

	for (i = 0; i < prxcap->VIC_count; i++) {
		vic = prxcap->VIC[i];
		for (j = 0; j < ARRAY_SIZE(sd_fmt); j++) {
			if (vic == sd_fmt[j])
				break;
		}
		if (j == ARRAY_SIZE(sd_fmt)) {
			only_support_sd = false;
			break;
		}
	}

	return only_support_sd;
}

bool is_4k_sink(struct hdmitx_dev *hdev)
{
	struct rx_cap *prxcap = &hdev->rxcap;
	enum hdmi_vic vic;
	u32 i = 0;

	if (!prxcap)
		return false;

	for (i = 0; (i < prxcap->VIC_count) && (i < VIC_MAX_NUM); i++) {
		vic = prxcap->VIC[i] & 0xff;
		if (vic >= HDMI_93_3840x2160p24_16x9 &&
		    vic <= HDMI_97_3840x2160p60_16x9) {
			return true;
		}
	}
	return false;
}

u32 hdmitx21_check_edid_all_zeros(u8 *buf)
{
	u32 i = 0, j = 0;
	u32 chksum = 0;

	for (j = 0; j < EDID_MAX_BLOCK; j++) {
		chksum = 0;
		for (i = 0; i < 128; i++)
			chksum += buf[i + j * 128];
		if (chksum != 0)
			return 0;
	}
	return 1;
}

/* static bool hdmitx21_edid_header_invalid(u8 *buf) */
/* { */
/*	bool base_blk_invalid = false; */
/*	bool ext_blk_invalid = false; */
/*	bool ret = false; */
/*	int i = 0; */

/*	if (buf[0] != 0 || buf[7] != 0) { */
/*		base_blk_invalid = true; */
/*	} else { */
/*		for (i = 1; i < 7; i++) { */
/*			if (buf[i] != 0xff) { */
/*				base_blk_invalid = true; */
/*				break; */
/*			} */
/*		} */
/*	} */
	/* judge header strictly, only if both header invalid */
/*	if (buf[0x7e] > 0) { */
/*		if (buf[0x80] != 0x2 && buf[0x80] != 0xf0) */
/*			ext_blk_invalid = true; */
/*		ret = base_blk_invalid && ext_blk_invalid; */
/*	} else { */
/*		 ret = base_blk_invalid; */
/*	} */

/*	return ret; */
/* } */

bool hdmitx21_edid_notify_ng(u8 *buf)
{
	if (!buf)
		return true;

	return check21_dvi_hdmi_edid_valid(buf) == 0;

	/* notify EDID NG to systemcontrol */
	/* if (hdmitx_check_edid_all_zeros(buf)) */
		/* return true; */
	/* else if ((buf[0x7e] > 3) && */
		/* hdmitx_edid_header_invalid(buf)) */
		/* return true; */
	/* may extend NG case here */

	/* return false; */
}
