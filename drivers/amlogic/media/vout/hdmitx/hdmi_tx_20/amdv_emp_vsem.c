// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
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
#include "./../../../enhancement/amdolby_vision/md_config.h"

#include "amdv_emp_vsem.h"

#undef pr_fmt
#define pr_fmt(fmt) "emp: " fmt

static void set_byte(unsigned char *meta,
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

static int hdmitx_pack_vsemds(struct emp_edr_config *pconfig,
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
	if (!pconfig->vsem_packets) {
		pr_info("vsem_packets don't alloc\n");
		return -1;
	}

	memset(pconfig->vsem_packets, 0, sizeof(struct vsem_pkt) * num_pkts);

	crc = get_crc32(p_metadata, metadata_len);

	/* first pkt */
	p = 0;
	cur_pkt = &pconfig->vsem_packets[p];
	pcrc = cur_pkt->pb;
	cur_pkt->pkt_type = 0x7F;
	HB1 = 0;
	set_byte(&HB1, 1, FIELD_FIRST);
	if (num_pkts > 1)
		set_byte(&HB1, 0, FIELD_LAST);
	else
		set_byte(&HB1, 1, FIELD_LAST);
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
		for (count = 0; count < 4; count++)
			pcrc[count] = acrc[count];
	} else {
		if (pcrcfrt) {
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
			for (count = 0; count < VSEM_CRC_LEN - last_packet_len;
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

void hdmitx_disable_emp_pkt(void)
{
	struct hdmitx_dev *hdev;

	hdev = emp_cfg_val.hdev;
	hdev->hwop.cntlconfig(hdev, CONF_EMP_NUMBER, 0);
}

void hdmitx_emp_infoframe(struct hdmitx_dev *hdev,
					enum eotf_type type,
					enum eotf_type type_save,
				    bool signal_sdr)
{
	if (type == EOTF_T_DOLBYVISION &&
		type != type_save) {
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
	if  (type == EOTF_T_LL_MODE &&
		 type != type_save) {
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
	if (type != EOTF_T_DOLBYVISION &&
		type != EOTF_T_LL_MODE &&
		signal_sdr) {
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
	bool vsem_flag = false;

	if (!hdev) {
		pr_info("EMP hdmitx: no device\n");
		return -1;
	}

	if (hdev->data->chip_type < MESON_CPU_ID_GXL) {
		pr_info("EMP hdmitx: not support DolbyVision\n");
		return -1;
	}
	if (hdev->ready == 0 ||
		hdev->rxcap.dv_info.ieeeoui != DV_IEEE_OUI) {
		return -1;
	}

	if (hdev->hdmi_current_eotf_type != type ||
		hdev->hdmi_current_tunnel_mode != tunnel_mode ||
		hdev->hdmi_current_signal_sdr != signal_sdr) {
		hdev->hdmi_current_eotf_type = type;
		hdev->hdmi_current_tunnel_mode = tunnel_mode;
		hdev->hdmi_current_signal_sdr = signal_sdr;
		pr_info("%s: type=%d, tunnel_mode=%d, signal_sdr=%d\n",
			__func__, type, tunnel_mode, signal_sdr);
	}
	hdmitx_emp_infoframe(hdev, type, type_save, signal_sdr);
	type_save = type;
	if (type != EOTF_T_DOLBYVISION && type != EOTF_T_LL_MODE) {
		/*exit from Dolby VSIF*/
		hdmitx_set_vsif_pkt(type, tunnel_mode, vsif_data, signal_sdr);
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

		dma_sync_single_for_cpu(hdev->hdtx_dev,
			emp_cfg_val.pkts_phy_addr,
			emp_cfg_val.size,
			DMA_TO_DEVICE);
			config.num_packets = 0;
			config.phys_ptr = emp_cfg_val.pkts_phy_addr;
			config.vsem_packets = emp_cfg_val.p_pkts;
		rv = hdmitx_pack_vsemds(&config, p_vsem, vsem_len, data_version);
		if (rv != 0) {
			pr_info("pack_vsem fail!\n");
			return -1;
		}
		/*Disable VSIF send*/
		hdev->hwop.setpacket(HDMI_PACKET_VEND, NULL, NULL);
		/*Enable VS-EMDS send*/
		dma_sync_single_for_device(hdev->hdtx_dev,
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

void dump_emp_packet(void)
{
	unsigned long phys_ptr;
	unsigned int num_packets;
	unsigned int count, body_cnt, pos;
	struct vsem_pkt *pkt_literal;
	unsigned char *tmp_buf = NULL;

	tmp_buf = kmalloc(256, GFP_KERNEL);
	if (!tmp_buf)
		return;

	phys_ptr = hdmitx_rd_reg(HDMITX_TOP_EMP_MEMADDR_START);
	if (emp_cfg_val.pkts_phy_addr != phys_ptr) {
		pr_info("%s: The EMP phys Addr Error!0x%lx VS 0x%lx\n",
			__func__, (unsigned long)emp_cfg_val.pkts_phy_addr,
			phys_ptr);
		kfree(tmp_buf);
		return;
	}
	num_packets = hdmitx_rd_reg(HDMITX_TOP_EMP_CNTL0) >> 16;
	for (count = 0; count < num_packets; count++) {
		pos = 0;
		memset(tmp_buf, 0, 256);
		pkt_literal = emp_cfg_val.p_pkts;
		pkt_literal += count;
		pos += sprintf(tmp_buf, "EMP%d: 0x%02x 0x%02x 0x%02x ",
			count, pkt_literal->pkt_type,
			pkt_literal->hb1, pkt_literal->seq_index);
		for (body_cnt = 0;
		     body_cnt < VSEM_PKT_BODY_SIZE;
		     body_cnt++)
			pos += sprintf(tmp_buf + pos,
				       "0x%02x ", pkt_literal->pb[body_cnt]);
		sprintf(tmp_buf + pos, "0x%02x", pkt_literal->padding);
		pr_info("%s\n", tmp_buf);
	}
	kfree(tmp_buf);
}
EXPORT_SYMBOL(dump_emp_packet);

void vsem_init_cfg(struct hdmitx_dev *hdev)
{
	size_t alloc_size, alloc_len;
	void *virt_ptr;
	dma_addr_t paddr;

	if (!hdev)
		pr_info("emp hdev = null\n");
	else
		pr_info("emp hdev have value\n");
	alloc_len = sizeof(struct vsem_pkt) * MAX_VSEM_NUM;
	alloc_size = (alloc_len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	//virt_ptr = kmalloc(alloc_size, GFP_KERNEL | GFP_DMA);
	virt_ptr = kmalloc(alloc_size, GFP_KERNEL);
	if (!virt_ptr)
		return;
	paddr = virt_to_phys((unsigned char *)
		((((unsigned long)virt_ptr) + 0x1f) & (~0x1f)));
	//of_dma_configure(hdev->hdtx_dev, NULL, 1);
	//paddr = dma_map_single(hdev->hdtx_dev,
	//	virt_ptr, alloc_size, DMA_TO_DEVICE);
	//if (dma_mapping_error(hdev->hdtx_dev, paddr)) {
	//	pr_info("dma_map_single() failed\n");
	//	kfree(virt_ptr);
	//	return;
	//}

	emp_cfg_val.hdev = hdev;
	emp_cfg_val.p_pkts = (struct vsem_pkt *)virt_ptr;
	emp_cfg_val.pkts_phy_addr = paddr;
	emp_cfg_val.size = alloc_size;
	emp_cfg_val.send_vsemds = send_emp;
	if (!emp_cfg_val.hdev)
		pr_info("emp_cfg_val hdev = null\n");
	else
		pr_info("emp_cfg_val hdev have value\n");
}

