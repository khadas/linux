/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/dv_emp_vsem.c
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

#include <asm/cacheflush.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/ctype.h>
#include <linux/extcon.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/reboot.h>
#include <linux/extcon.h>
#include <linux/i2c.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include "./hw/hdmi_tx_reg.h"

#include "dv_emp_vsem.h"

#undef pr_fmt
#define pr_fmt(fmt) "emp: " fmt

static unsigned int crc32_lut_table[256] = {
0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

void set_byte(unsigned char *meta,
		 unsigned char value,
		 unsigned char mask)
{
	unsigned int count;
	unsigned int test_val;
	unsigned int set_val;
	unsigned char test_mask;

	test_mask = mask;
	count = 0;
	while ((test_mask > 0) && (count < 8)) {
		test_val = 1 << count;
		if ((test_val & mask) == test_val) {
			set_val = (value & 1);
			value >>= 1;
			if (set_val > 0)
				*meta |= 1 << count;
			else
				*meta &= ~(1 << count);
			test_mask  &= ~(1 << count);
		}
		count++;
	}
}

u32 get_vsem_pkt_num(u32 metadata_len, u32 *last_packet_len)
{
	u32 num_packets = 1;

	if (metadata_len == 0)
		return 0;

	*last_packet_len = metadata_len;
	if (*last_packet_len > VSEM_FIRST_PKT_EDR_DATA_SIZE) {
		*last_packet_len -= VSEM_FIRST_PKT_EDR_DATA_SIZE;
		while (*last_packet_len > VSEM_PKT_BODY_SIZE) {
			*last_packet_len -= VSEM_PKT_BODY_SIZE;
			num_packets++;
		}
		if (*last_packet_len > 0)
			num_packets++;
	}

	return num_packets;
}

unsigned int get_crc32(u32 crc, const void *buf, u32 size)
{
	const u8 *p = (u8 *)buf;

	crc = ~crc;
	while (size) {
		crc = (crc << 8) ^ crc32_lut_table[((crc >> 24) ^ *p) & 0xff];
		p++;
		size--;
	}

	return crc;
}

static int pack_vsemds(struct emp_edr_config *pconfig,
					   unsigned char *p_metadata,
					   int metadata_len,
					   unsigned char data_version)
{
	struct vsem_pkt *cur_pkt;
	int count, acrc_loc, frt_loc;
	unsigned int p;
	unsigned int num_pkts = 1;
	unsigned int last_packet_len = 0;
	unsigned int cur_packet_len;
	unsigned int crc;
	unsigned int edr_data_len;
	unsigned char *pcrc, *pcrcfrt, acrc[4];
	unsigned char PB0;
	unsigned char HB1;

	edr_data_len = metadata_len + VSEM_CRC_LEN;
	pcrcfrt = NULL;
	num_pkts = get_vsem_pkt_num(edr_data_len, &last_packet_len);
	if (num_pkts > MAX_VSEM_NUM)
		pr_info("vsem metada_len is too big\n");

	pconfig->num_packets = num_pkts;
	if (pconfig->vsem_packets == NULL) {
		pr_info("vsem_packets don't alloc\n");
		return -1;
	}

	memset(pconfig->vsem_packets, 0, sizeof(struct vsem_pkt) * num_pkts);

	crc = get_crc32(0, p_metadata, metadata_len);

	/* first pkt */
	p = 0;
	cur_pkt = &pconfig->vsem_packets[p];
	pcrc = cur_pkt->pb;
	cur_pkt->pkt_type = 0x7F;
	HB1 = 0;
	set_byte(&HB1, 1, FIELD_FIRST);
	set_byte(&HB1, 0, FIELD_LAST);
	cur_pkt->hb1 = HB1;
	cur_pkt->seq_index = p;
	edr_data_len += 6; /* PB0~12 */
	PB0 = 0;
	set_byte(&PB0, 1, FIELD_NEW);
	set_byte(&PB0, 1, FIELD_END);
	set_byte(&PB0, 2, FIELD_DS_TYPE);
	set_byte(&PB0, 1, FIELD_AFR);
	set_byte(&PB0, 1, FIELD_VFR);
	set_byte(&PB0, 1, FIELD_SYNC);
	cur_pkt->pb[0] = PB0;
	cur_pkt->pb[1] = 0x00;
	cur_pkt->pb[2] = 0x00;
	cur_pkt->pb[3] = 0x00;
	cur_pkt->pb[4] = 0x00;
	cur_pkt->pb[5] = edr_data_len >> 8;
	cur_pkt->pb[6] = edr_data_len & 0xff;
	cur_pkt->pb[7] = 0x46;
	cur_pkt->pb[8] = 0xD0;
	cur_pkt->pb[9] = 0x00;
	cur_pkt->pb[10] = data_version;
	cur_pkt->pb[11] = 0x00;
	cur_pkt->pb[12] = 0x00;
	cur_packet_len = VSEM_FIRST_PKT_EDR_DATA_SIZE;
	if (cur_packet_len > metadata_len)
		cur_packet_len = metadata_len;
	memcpy(&cur_pkt->pb[13], p_metadata, cur_packet_len);
	pcrc = &cur_pkt->pb[13];
	p_metadata += cur_packet_len;
	metadata_len -= cur_packet_len;
	num_pkts--;
	p++;
	/*other pkts */
	while (num_pkts > 0) {
		if (p == 1)
			frt_loc = 13 + cur_packet_len - 1;
		else
			frt_loc = cur_packet_len - 1;
		pcrcfrt =
			&pconfig->vsem_packets[p - 1].pb[frt_loc];
		cur_pkt = &pconfig->vsem_packets[p];
		cur_pkt->pkt_type = 0x7F;
		HB1 = 0x00;
		set_byte(&HB1, 0, FIELD_FIRST);
		if (num_pkts == 1) {
			set_byte(&HB1, 1, FIELD_LAST);
			cur_packet_len = last_packet_len;
		} else {
			set_byte(&HB1, 0, FIELD_LAST);
			cur_packet_len = VSEM_PKT_BODY_SIZE;
		}
		cur_pkt->hb1 = HB1;
		cur_pkt->seq_index = (unsigned char)p;
		if (metadata_len > 0) {
			if (metadata_len >= cur_packet_len) {
				memcpy(cur_pkt->pb,
				       p_metadata, cur_packet_len);
				metadata_len -= cur_packet_len;
				p_metadata += cur_packet_len;
			} else {
				memcpy(cur_pkt->pb,
				       p_metadata, metadata_len);
				p_metadata += metadata_len;
				metadata_len = 0;
			}
		}
		pcrc = &cur_pkt->pb[0];
		num_pkts--;
		p++;
	}
	/*crc*/
	acrc[0] = (crc & 0xff000000) >> 24;
	acrc[1] = (crc & 0xff0000) >> 16;
	acrc[2] = (crc & 0xff00) >>  8;
	acrc[3] =  crc & 0xff;
	if (last_packet_len >= VSEM_CRC_LEN) {
		pcrc += last_packet_len - VSEM_CRC_LEN;
		for (count = 0; count < 4; count++) {
			pcrc[count] = acrc[count];
		}
	} else {
		if (pcrcfrt != NULL) {
			/*last pkt*/
			pcrc = &cur_pkt->pb[0];
			for (count = last_packet_len - 1;
			     count >= 0;
			     count--) {
				/*LEN - 1 - (last_packet_len - 1 - count)*/
				acrc_loc = VSEM_CRC_LEN -
					last_packet_len + count;
				pcrc[count] = acrc[acrc_loc];
			}
			/*the pkt before last pkt*/
			pcrcfrt -= VSEM_CRC_LEN  - last_packet_len;
			pcrcfrt++;
			for (count = 0;
			     count < VSEM_CRC_LEN - last_packet_len;
			     count++) {
			     pcrcfrt[count] = acrc[count];
			}
		}
	}
	return 0;
}

static struct emp_hdmi_cfg emp_cfg_val;
static void hdmitx_set_emp_pkt(struct emp_edr_config *pconfig)
{
	struct hdmitx_dev *hdev;

	hdev = emp_cfg_val.hdev;
	hdev->hwop.cntlconfig(hdev, CONF_EMP_PHY_ADDR, pconfig->phys_ptr);
	hdev->hwop.cntlconfig(hdev, CONF_EMP_NUMBER, pconfig->num_packets);
}

static void hdmitx_disable_emp_pkt(void)
{
	struct hdmitx_dev *hdev;

	hdev = emp_cfg_val.hdev;
	hdev->hwop.cntlconfig(hdev, CONF_EMP_NUMBER, 0);
}

static void hdmitx_emp_infoframe(struct hdmitx_dev *hdev,
					enum eotf_type type,
					enum eotf_type type_save,
				    bool signal_sdr)
{
	if ((type == EOTF_T_DOLBYVISION) &&
		(type != type_save)) {
		pr_info("%s: T_Dolby Vision enter\n", __func__);
		/*first disable drm package*/
		hdev->hwop.setpacket(HDMI_PACKET_DRM,
			NULL, NULL);
		hdev->hwop.cntlconfig(hdev, CONF_AVI_BT2020,
			CLR_AVI_BT2020);/*BT709*/
		hdev->hwop.cntlconfig(hdev,
			CONF_AVI_RGBYCC_INDIC,
			COLORSPACE_RGB444);
		hdev->hwop.cntlconfig(hdev, CONF_AVI_Q01,
			RGB_RANGE_FUL);
	}
	if  ((type == EOTF_T_LL_MODE) &&
		 (type != type_save)) {
		pr_info("%s: Dolby Vision LL enter\n", __func__);
		/*first disable drm package*/
		hdev->hwop.setpacket(HDMI_PACKET_DRM,
			NULL, NULL);
		if (hdev->rxcap.colorimetry_data & 0xe0)
			/*if RX support BT2020, then output BT2020*/
			hdev->hwop.cntlconfig(hdev, CONF_AVI_BT2020,
				SET_AVI_BT2020);/*BT2020*/
		else
			hdev->hwop.cntlconfig(hdev, CONF_AVI_BT2020,
				CLR_AVI_BT2020);/*BT709*/
		hdev->hwop.cntlconfig(hdev,
			CONF_AVI_RGBYCC_INDIC,
			COLORSPACE_YUV422);
		hdev->hwop.cntlconfig(hdev, CONF_AVI_YQ01,
			YCC_RANGE_LIM);
	}
	if ((type != EOTF_T_DOLBYVISION) &&
		(type != EOTF_T_LL_MODE) &&
		(signal_sdr == true)) {
		pr_info("%s: Dolby Vision exit\n", __func__);
		update_current_para(hdev);
		hdev->hwop.cntlconfig(hdev,
			CONF_AVI_RGBYCC_INDIC, hdev->para->cs);
		hdev->hwop.cntlconfig(hdev,
			CONF_AVI_Q01, RGB_RANGE_DEFAULT);
		hdev->hwop.cntlconfig(hdev,
			CONF_AVI_YQ01, YCC_RANGE_LIM);
		hdev->hwop.cntlconfig(hdev, CONF_AVI_BT2020,
		CLR_AVI_BT2020);/*BT709*/
	}
}

int send_emp(enum eotf_type type,
		enum mode_type tunnel_mode,
		struct dv_vsif_para *vsif_data,
		unsigned char *p_vsem,
		int vsem_len,
		bool signal_sdr)
{
	struct emp_edr_config config;
	int rv;
	struct hdmitx_dev *hdev = emp_cfg_val.hdev;
	unsigned char data_version = 0;
	static enum eotf_type type_save = EOTF_T_NULL;
	static bool vsem_flag = false;

	if (!hdev) {
		pr_info("EMP hdmitx: no device\n");
		return -1;
	}

	if ((hdev->chip_type) < MESON_CPU_ID_GXL) {
		pr_info("EMP hdmitx: not support DolbyVision\n");
		return -1;
	}
	if ((hdev->ready == 0) ||
		(hdev->rxcap.dv_info.ieeeoui != DV_IEEE_OUI)) {
		return -1;
	}

	if (hdr_status_pos != 2)
		pr_info("EMP hdmitx_set_vsif_pkt: type = %d\n", type);
	hdr_status_pos = 2;
	hdev->hdmi_current_eotf_type = type;
	hdmitx_emp_infoframe(hdev, type, type_save, signal_sdr);
	type_save = type;
	if ((type != EOTF_T_DOLBYVISION) &&
	    (type != EOTF_T_LL_MODE)) {
		if (vsem_flag == true) { /*exit from Dolby VS-EMDS*/
			hdmitx_disable_emp_pkt();
		} else {  /*exit from Dolby VSIF*/
			hdmitx_set_vsif_pkt(type, tunnel_mode, vsif_data, signal_sdr);
		}
		hdev->dv_src_feature = 0;
		vsem_flag = false;
		pr_info("EMP hdmitx_set_vsif_pkt: stop\n");
		return 1;
	}
	hdev->dv_src_feature = 1;
	if (p_vsem) { /*Dolby VS-EMDS*/
		vsem_flag = true;
		if (type == EOTF_T_DOLBYVISION)
			data_version = 0;
		else
			data_version = 1;

		dma_sync_single_for_cpu(
			hdev->hdtx_dev,
			emp_cfg_val.pkts_phy_addr,
			emp_cfg_val.size,
			DMA_TO_DEVICE);
			config.num_packets = 0;
			config.phys_ptr = emp_cfg_val.pkts_phy_addr;
			config.vsem_packets = emp_cfg_val.p_pkts;
		rv = pack_vsemds(&config, p_vsem, vsem_len, data_version);
		if (rv != 0) {
			pr_info("pack_vsem fail!\n");
			return -1;
		}
		/*Disable VSIF send*/
		hdev->hwop.setpacket(HDMI_PACKET_VEND, NULL, NULL);
		/*Enable VS-EMDS send*/
		dma_sync_single_for_device(
			hdev->hdtx_dev,
			emp_cfg_val.pkts_phy_addr,
			emp_cfg_val.size,
			DMA_TO_DEVICE);
		hdmitx_set_emp_pkt(&config);
	} else {
		vsem_flag = false;
		/*Disable VS-EMDS send*/
		hdmitx_disable_emp_pkt();
		/*Enable VSIF send*/
		hdmitx_set_vsif_pkt(type, tunnel_mode, vsif_data, signal_sdr);
	}
	return 1;
}
EXPORT_SYMBOL(send_emp);

void vsem_init_cfg(struct hdmitx_dev *hdev)
{
	size_t alloc_size, alloc_len;
	void *virt_ptr;
	dma_addr_t paddr;

	alloc_len = sizeof(struct vsem_pkt) * MAX_VSEM_NUM;
	alloc_size = (alloc_len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	virt_ptr = kmalloc(alloc_size, GFP_KERNEL | GFP_DMA);
	if (virt_ptr == NULL)
		return;

	of_dma_configure(hdev->hdtx_dev, NULL);
	paddr = dma_map_single(hdev->hdtx_dev,
		virt_ptr, alloc_size, DMA_TO_DEVICE);
	if (dma_mapping_error(hdev->hdtx_dev, paddr)) {
		pr_info("dma_map_single() failed\n");
		kfree(virt_ptr);
		return;
	}

	emp_cfg_val.hdev = hdev;
	emp_cfg_val.p_pkts = (struct vsem_pkt *)virt_ptr;
	emp_cfg_val.pkts_phy_addr = paddr;
	emp_cfg_val.size = alloc_size;
	emp_cfg_val.send_vsemds = send_emp;
}
