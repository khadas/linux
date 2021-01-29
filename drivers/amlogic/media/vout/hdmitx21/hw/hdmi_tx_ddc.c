// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_ddc.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>
#include "common.h"

static DEFINE_MUTEX(ddc_mutex);
static u32 ddc_write_1byte(u8 slave, u8 offset_addr, u8 data)
{
	u32 st = 0;

	mutex_lock(&ddc_mutex);
	mutex_unlock(&ddc_mutex);
	return st;
}

static u32 ddc_read_8byte(u8 slave, u8 offset_addr, u8 *data)
{
	u32 st = 0;

	mutex_lock(&ddc_mutex);
	mutex_unlock(&ddc_mutex);
	return st;
}

static u32 ddc_read_1byte(u8 slave, u8 offset_addr, u8 *data)
{
	u32 st = 0;

	mutex_lock(&ddc_mutex);
	mutex_unlock(&ddc_mutex);
	return st;
}

static u32 hdcp_rd_bksv(u8 *data)
{
	return ddc_read_8byte(HDCP_SLAVE, HDCP14_BKSV, data);
}

void scdc21_rd_sink(u8 adr, u8 *val)
{
	hdmitx21_ddc_hw_op(DDC_MUX_DDC);
	ddc_read_1byte(SCDC_SLAVE, adr, val);
}

void scdc21_wr_sink(u8 adr, u8 val)
{
	hdmitx21_ddc_hw_op(DDC_MUX_DDC);
	ddc_write_1byte(SCDC_SLAVE, adr, val);
}

u32 hdcp21_rd_hdcp14_ver(void)
{
	int ret = 0;
	u8 bksv[8] = {0};

	hdmitx21_ddc_hw_op(DDC_MUX_DDC);
	ret = hdcp_rd_bksv(&bksv[0]);
	if (ret)
		return 1;
	ret = hdcp_rd_bksv(&bksv[0]);
	if (ret)
		return 1;

	return 0;
}

u32 hdcp21_rd_hdcp22_ver(void)
{
	u32 ret;
	u8 ver;

	hdmitx21_ddc_hw_op(DDC_MUX_DDC);
	ret = ddc_read_1byte(HDCP_SLAVE, HDCP2_VERSION, &ver);
	if (ret)
		return ver == 0x04;
	ret = ddc_read_1byte(HDCP_SLAVE, HDCP2_VERSION, &ver);
	if (ret)
		return ver == 0x04;

	return 0;
}

/* only for I2C reactive using */
void edid21_read_head_8bytes(void)
{
	u8 head[8] = {0};

	hdmitx21_ddc_hw_op(DDC_MUX_DDC);
	ddc_read_8byte(EDID_SLAVE, 0x00, head);
}

void hdmitx21_read_edid(u8 *_rx_edid)
{
	u32 i;
	u32 byte_num = 0;
	u8 edid_extension = 1;
	u8 *rx_edid = _rx_edid;

	// Program SLAVE/SEGMENT/ADDR
	hdmitx21_wr_reg(LM_DDC_IVCTX, 0x80); //sel edid
	hdmitx21_wr_reg(DDC_CMD_IVCTX, 0x09); //clear fifo
	hdmitx21_wr_reg(DDC_ADDR_IVCTX, 0xa0); //edid slave addr

	// Read complete EDID data sequentially
	while (byte_num < (128 * (1 + edid_extension))) {
		if ((byte_num % 256) == 0)
			hdmitx21_wr_reg(DDC_SEGM_IVCTX, byte_num >> 8); //segment
		hdmitx21_wr_reg(DDC_OFFSET_IVCTX, byte_num & 0xff); //offset
		hdmitx21_wr_reg(DDC_DIN_CNT1_IVCTX, 1 << 3); //data length lo
		hdmitx21_wr_reg(DDC_DIN_CNT2_IVCTX, 0x00); //data length hi
		hdmitx21_wr_reg(DDC_CMD_IVCTX, 0x04); //CMD
		// Wait until I2C done
		hdmitx21_poll_reg(DDC_STATUS_IVCTX, 1 << 4, ~(1 << 4), HZ / 100); //i2c process
		hdmitx21_poll_reg(DDC_STATUS_IVCTX, 0 << 4, ~(1 << 4), HZ / 100); //i2c done
		// Read back 8 bytes
		for (i = 0; i < 8; i++) {
			if (byte_num == 126) {
				edid_extension  = hdmitx21_rd_reg(DDC_DATA_AON_IVCTX);
				rx_edid[byte_num] = edid_extension;
				if (edid_extension > 3)
					edid_extension = 3;
			} else {
				rx_edid[byte_num] = hdmitx21_rd_reg(DDC_DATA_AON_IVCTX);
			}
			byte_num++;
		}
	}
} /* hdmi20_tx_read_edid */
