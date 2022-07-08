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

#define TPI_DDC_CMD_ENHANCED_DDC_READ  0x04
#define TPI_DDC_CMD_SEQUENTIAL_READ    0x02
#define LEN_TPI_DDC_FIFO_SIZE          16

static DEFINE_MUTEX(ddc_mutex);
static u32 ddc_write_1byte(u8 slave, u8 offset_addr, u8 data)
{
	u32 st = 0;

	mutex_lock(&ddc_mutex);
	mutex_unlock(&ddc_mutex);
	return st;
}

u32 ddc_read_8byte(u8 slave, u8 offset_addr, u8 *data)
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
	return 0;
//	return ddc_read_8byte(DDC_HDCP_ADDR, HDCP14_BKSV, data);
}

void scdc21_rd_sink(u8 adr, u8 *val)
{
	hdmitx21_ddc_hw_op(DDC_MUX_DDC);
	ddc_read_1byte(DDC_SCDC_ADDR, adr, val);
}

void scdc21_wr_sink(u8 adr, u8 val)
{
	hdmitx21_ddc_hw_op(DDC_MUX_DDC);
	ddc_write_1byte(DDC_SCDC_ADDR, adr, val);
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
	ret = ddc_read_1byte(DDC_HDCP_ADDR, DDC_HDCP_REG_VERSION, &ver);
	if (ret)
		return ver == 0x04;
	ret = ddc_read_1byte(DDC_HDCP_ADDR, DDC_HDCP_REG_VERSION, &ver);
	if (ret)
		return ver == 0x04;

	return 0;
}

/* only for I2C reactive using */
void edid21_read_head_8bytes(void)
{
//	u8 head[8] = {0};

	hdmitx21_ddc_hw_op(DDC_MUX_DDC);
//	ddc_read_8byte(EDID_SLAVE, 0x00, head);
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
	hdmitx21_wr_reg(DDC_ADDR_IVCTX, DDC_EDID_ADDR & BIT_DDC_ADDR_REG); //edid slave addr

	// Read complete EDID data sequentially
	while (byte_num < (128 * (1 + edid_extension))) {
		if ((byte_num % 256) == 0)
			hdmitx21_wr_reg(DDC_SEGM_IVCTX, byte_num >> 8); //segment
		hdmitx21_wr_reg(DDC_OFFSET_IVCTX, byte_num & 0xff); //offset
		hdmitx21_wr_reg(DDC_DIN_CNT1_IVCTX, 1 << 3); //data length lo
		hdmitx21_wr_reg(DDC_DIN_CNT2_IVCTX, 0x00); //data length hi
		hdmitx21_wr_reg(DDC_CMD_IVCTX, 0x04); //CMD
		// Wait until I2C done
		hdmitx21_poll_reg(DDC_STATUS_IVCTX, 1 << 4, ~(1 << 4), HZ / 50); //i2c process
		hdmitx21_poll_reg(DDC_STATUS_IVCTX, 0 << 4, ~(1 << 4), HZ / 50); //i2c done
		// Read back 8 bytes
		for (i = 0; i < 8; i++) {
			if (byte_num == 126) {
				edid_extension  = hdmitx21_rd_reg(DDC_DATA_AON_IVCTX);
				rx_edid[byte_num] = edid_extension;
			} else {
				rx_edid[byte_num] = hdmitx21_rd_reg(DDC_DATA_AON_IVCTX);
			}
			byte_num++;
		}
		if (byte_num > 127 && byte_num < 256)
			if (rx_edid[128 + 4] == 0xe2 && rx_edid[128 + 5] == 0x78)
				edid_extension = rx_edid[128 + 6];
		if (edid_extension > 7) {
			pr_info(HW "edid extension block number:");
			pr_info(HW " %d, reset to MAX 7\n",
				edid_extension);
			edid_extension = 7; /* Max extended block */
		}
	}
} /* hdmi20_tx_read_edid */

void hdmi_ddc_error_reset(void)
{
	hdmitx21_set_reg_bits(TPI_DDC_MASTER_EN_IVCTX, 1, 7, 1);
	hdmitx21_wr_reg(DDC_CMD_IVCTX, 0x0f);
	hdmitx21_wr_reg(DDC_CMD_IVCTX, 0x0a);
	hdmitx21_set_reg_bits(TPI_DDC_MASTER_EN_IVCTX, 0, 7, 1);
}

u8 hdmi_ddc_busy_check(void)
{
	return hdmitx21_rd_reg(DDC_STATUS_IVCTX) & BIT_DDC_STATUS_INPROG;
}

u8 hdmi_ddc_status_check(void)
{
	bool check_failed = false;
	u8 ddc_status;

	ddc_status = hdmitx21_rd_reg(DDC_STATUS_IVCTX);
	if (ddc_status & (BIT_DDC_STATUS_BUSLOW | BIT_DDC_STATUS_NACK)) {
		hdmitx21_wr_reg(DDC_STATUS_IVCTX, 0x00);
		check_failed = true;
	}

	return check_failed;
}

static u8 ddc_tx_hdcp2x_check(void)
{
	u8 val;

	val = hdmitx21_rd_reg(HDCP2X_CTL_0_IVCTX) & BIT_HDCP2X_CTL_0_EN;
	if (val)
		hdmitx21_set_bit(SCDC_CTL_IVCTX, BIT_SCDC_CTL_REG_DDC_STALL_REQ, true);
	return val;
}

static void ddc_tx_ddc_error_reset(void)
{
	hdmitx21_set_bit(TPI_DDC_MASTER_EN_IVCTX, BIT_TPI_DDC_MASTER_EN_HW_EN, true);
	hdmitx21_wr_reg(DDC_CMD_IVCTX, DDC_CMD_ABORT_TRANSACTION);
	hdmitx21_wr_reg(DDC_CMD_IVCTX, DDC_CMD_CLK_RESET);
	hdmitx21_set_bit(TPI_DDC_MASTER_EN_IVCTX, BIT_TPI_DDC_MASTER_EN_HW_EN, false);
}

static u8 ddc_tx_busy_check(void)
{
	return hdmitx21_rd_reg(DDC_STATUS_IVCTX) & BIT_DDC_STATUS_INPROG;
}

static bool ddc_wait_free(void)
{
	u8 val;
	s8 tmo1 = 5; /* unit: ms */
	s8 tmo2 = 2;

	while (tmo2--) {
		while (tmo1--) {
			val = ddc_tx_busy_check();
			if (!val)
				return true;
			usleep_range(2000, 2500);
		}
		pr_info("hdmitx: ddc bus busy\n");
		ddc_tx_ddc_error_reset();
		usleep_range(2000, 2500);
	}
	return false;
}

static void ddc_tx_en(u8 seg_index, u8 slave_addr, u8 reg_addr)
{
	hdmitx21_set_bit(TPI_DDC_MASTER_EN_IVCTX, BIT_TPI_DDC_MASTER_EN_HW_EN, true);
	hdmitx21_wr_reg(DDC_ADDR_IVCTX, slave_addr & BIT_DDC_ADDR_REG);
	hdmitx21_wr_reg(DDC_SEGM_IVCTX, seg_index);
	hdmitx21_wr_reg(DDC_OFFSET_IVCTX, reg_addr);
}

static void ddc_tx_read(u8 seg_index, u16 length)
{
	u8 read_cmd;

	read_cmd = seg_index ? TPI_DDC_CMD_ENHANCED_DDC_READ : TPI_DDC_CMD_SEQUENTIAL_READ;
	hdmitx21_wr_reg(DDC_DIN_CNT2_IVCTX, (u8)(length >> 8));
	hdmitx21_wr_reg(DDC_DIN_CNT1_IVCTX, (u8)length);
	hdmitx21_wr_reg(DDC_CMD_IVCTX, DDC_CMD_CLR_FIFO);
	hdmitx21_wr_reg(DDC_CMD_IVCTX, read_cmd);
}

static u8 ddc_tx_fifo_size_read(void)
{
	return hdmitx21_rd_reg(DDC_DOUT_CNT_IVCTX) & BIT_DDC_DOUT_CNT_DATA_OUT_CNT;
}

static bool ddc_tx_err_check(void)
{
	bool check_failed = false;
	u8 ddc_status;

	ddc_status = hdmitx21_rd_reg(DDC_STATUS_IVCTX);
	if (ddc_status & (BIT_DDC_STATUS_BUSLOW | BIT_DDC_STATUS_NACK)) {
		hdmitx21_wr_reg(DDC_STATUS_IVCTX, 0x00);
		check_failed = true;
	}

	return check_failed;
}

static void ddc_tx_scdc_clr(u8 val)
{
	if (val)
		hdmitx21_set_bit(SCDC_CTL_IVCTX, BIT_SCDC_CTL_REG_DDC_STALL_REQ, false);
}

static void ddc_tx_error_check(enum ddc_err_t ds_ddc_error)
{
	if (ds_ddc_error) {
		hdmitx21_wr_reg(DDC_CMD_IVCTX, DDC_CMD_ABORT_TRANSACTION);
		hdmitx21_wr_reg(DDC_CMD_IVCTX, DDC_CMD_CLK_RESET);
	}
}

static void ddc_tx_fifo_read(u8 *p_buf, u16 fifo_size)
{
	hdmitx21_seq_rd_reg(DDC_DATA_AON_IVCTX, p_buf, fifo_size);
}

static void ddc_tx_disable(void)
{
	hdmitx21_set_bit(TPI_DDC_MASTER_EN_IVCTX, BIT_TPI_DDC_MASTER_EN_HW_EN, false);
}

static enum ddc_err_t _hdmitx_ddcm_read_(u8 seg_index,
	u8 slave_addr, u8 reg_addr, u8 *p_buf, u16 length)
{
	enum ddc_err_t ds_ddc_error = DDC_ERR_NONE;
	u16 fifo_size;
	u16 timeout_ms;
	u8 val;

	val = ddc_tx_hdcp2x_check();

	do {
		if (length == 0 || !p_buf)
			break;

		if (!ddc_wait_free())
			return DDC_ERR_BUSY;

		ddc_tx_en(seg_index, slave_addr, reg_addr);
		ddc_tx_read(seg_index, length);

		timeout_ms = (u16)(length * 12 / 10);
		usleep_range(2000, 3000);
		do {
			fifo_size = ddc_tx_fifo_size_read();

			if (fifo_size) {
				if (fifo_size > length) {
					ds_ddc_error = DDC_ERR_HW;
					break;
				} else if (fifo_size > LEN_TPI_DDC_FIFO_SIZE) {
					ds_ddc_error = DDC_ERR_LIM_EXCEED;
					break;
				}
				/* read fifo_size bytes */
				ddc_tx_fifo_read(p_buf, fifo_size);

				length -= fifo_size;
				p_buf += fifo_size;
			} else {
				usleep_range(1000, 1500);
				timeout_ms--;
				if (ddc_tx_err_check()) {
					ds_ddc_error = DDC_ERR_NACK;
					break;
				}
			}
		} while (length && timeout_ms);

		if (ds_ddc_error)
			break;
	} while (false);

	ddc_tx_scdc_clr(val);
	ddc_tx_error_check(ds_ddc_error);

	/* disable the DDC master */
	ddc_tx_disable();
	return ds_ddc_error;
}

void ddc_toggle_sw_tpi(void)
{
	hdmitx21_set_bit(LM_DDC_IVCTX, BIT_LM_DDC_SWTPIEN_B7, true);
	hdmitx21_set_bit(LM_DDC_IVCTX, BIT_LM_DDC_SWTPIEN_B7, false);
}

bool hdmitx_ddcm_read(u8 seg_index, u8 slave_addr, u8 reg_addr, u8 *p_buf, u16 len)
{
	enum ddc_err_t ddc_err;

	ddc_err = _hdmitx_ddcm_read_(seg_index, slave_addr, reg_addr, p_buf, len);
	return (ddc_err == DDC_ERR_NONE) ? false : true;
}

enum ddc_err_t hdmitx_ddc_read_1byte(u8 slave_addr, u8 reg_addr, u8 *p_buf)
{
	hdmitx21_wr_reg(LM_DDC_IVCTX, 0x80);
	hdmitx21_wr_reg(DDC_CMD_IVCTX, 0x09);
	hdmitx21_wr_reg(DDC_ADDR_IVCTX, slave_addr & BIT_DDC_ADDR_REG);

	hdmitx21_wr_reg(DDC_OFFSET_IVCTX, reg_addr);
	hdmitx21_wr_reg(DDC_DIN_CNT1_IVCTX, 1);
	hdmitx21_wr_reg(DDC_DIN_CNT2_IVCTX, 0x00);
	hdmitx21_wr_reg(DDC_CMD_IVCTX, 0x02);
	// Wait until I2C done
	hdmitx21_poll_reg(DDC_STATUS_IVCTX, 1 << 4, ~(1 << 4), HZ / 100);
	hdmitx21_poll_reg(DDC_STATUS_IVCTX, 0 << 4, ~(1 << 4), HZ / 100);
	p_buf[0]  = hdmitx21_rd_reg(DDC_DATA_AON_IVCTX);

	return DDC_ERR_NONE;
} /* hdmi20_tx_read_edid */

bool is_rx_hdcp2ver(void)
{
	u8 cap_val = 0;

	hdmitx_ddc_read_1byte(DDC_HDCP_DEVICE_ADDR, REG_DDC_HDCP_VERSION, &cap_val);

	return cap_val == 0x04;
}
