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

#include <linux/arm-smccc.h>

#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_ddc.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>
#include "common.h"

static void hdcptx1_load_key(void)
{
	struct arm_smccc_res res;

	// hdcptx14_load_key
	arm_smccc_smc(HDCPTX_IOOPR, HDCP14_LOADKEY, 0, 0, 0, 0, 0, 0, &res);
}

bool get_hdcp1_lstore(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(HDCPTX_IOOPR, HDCP14_KEY_READY, 0, 0, 0, 0, 0, 0, &res);

	return (unsigned int)((res.a0) & 0xffffffff);
}

bool get_hdcp2_lstore(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(HDCPTX_IOOPR, HDCP22_KEY_READY, 0, 0, 0, 0, 0, 0, &res);

	return (unsigned int)((res.a0) & 0xffffffff);
}

bool get_hdcp1_result(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(HDCPTX_IOOPR, HDCP14_RESULT, 0, 0, 0, 0, 0, 0, &res);

	return (unsigned int)((res.a0) & 0xffffffff);
}

bool get_hdcp2_result(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(HDCPTX_IOOPR, HDCP22_RESULT, 0, 0, 0, 0, 0, 0, &res);

	return (unsigned int)((res.a0) & 0xffffffff);
}

bool get_hdcp2_topo(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(HDCPTX_IOOPR, HDCP22_GET_TOPO, 0, 0, 0, 0, 0, 0, &res);

	return (unsigned int)((res.a0) & 0xffffffff);
}

void set_hdcp2_topo(u32 topo_type)
{
	struct arm_smccc_res res;

	pr_info("%s: %d", __func__, topo_type);
	arm_smccc_smc(HDCPTX_IOOPR, HDCP22_SET_TOPO, topo_type, 0, 0, 0, 0, 0, &res);
}

void hdcptx_init_reg(void)
{
	hdmitx21_set_bit(HDCP_CTRL_IVCTX, BIT(2), false);
	hdmitx21_set_bit(HDCP_CTRL_IVCTX, BIT(2), true);
	hdmitx21_wr_reg(CP2TX_TP1_IVCTX, 0x92);
	hdmitx21_wr_reg(TPI_MISC_IVCTX, 1);
	hdmitx21_set_bit(CP2TX_CTRL_2_IVCTX, BIT_CP2TX_CTRL_2_RI_SEQNUMM_AUTO, false);
	hdmitx21_wr_reg(RI_128_COMP_IVCTX, 0);
	hdcptx2_smng_auto(false);
}

u8 hdcptx1_ds_cap_status_get(void)
{
	return hdmitx21_rd_reg(TPI_COPP_DATA1_IVCTX);
}

u16 hdcptx1_get_prime_ri(void)
{
	return hdmitx21_rd_reg(RI_1_IVCTX) + (hdmitx21_rd_reg(RI_2_IVCTX) << 8);
}

void hdcptx1_ds_bksv_read(u8 *p_bksv, u8 b)
{
	hdmitx21_seq_rd_reg(TPI_WR_BKSV_1_IVCTX, p_bksv, b);
}

u8 hdcptx1_ksv_v_get(void)
{
	return hdmitx21_rd_reg(TPI_KSV_V_IVCTX);
}

void hdcptx1_protection_enable(bool en)
{
	if (en) {
		hdmitx21_set_bit(TPI_COPP_DATA2_IVCTX, BIT_TPI_COPP_DATA2_CANCEL_PROT_EN, false);
		hdmitx21_set_bit(TPI_COPP_DATA2_IVCTX, (BIT_TPI_COPP_DATA2_COPP_PROTLEVEL |
			BIT_TPI_COPP_DATA2_DOUBLE_RI_CHECK |
			BIT_TPI_COPP_DATA2_INTERM_RI_CHECK_EN |
			BIT_TPI_COPP_DATA2_KSV_FORWARD), true);
	} else {
		hdmitx21_set_bit(TPI_COPP_DATA2_IVCTX, (BIT_TPI_COPP_DATA2_COPP_PROTLEVEL |
			BIT_TPI_COPP_DATA2_DOUBLE_RI_CHECK |
			BIT_TPI_COPP_DATA2_INTERM_RI_CHECK_EN |
			BIT_TPI_COPP_DATA2_KSV_FORWARD), false);
			hdmitx21_set_bit(TPI_COPP_DATA2_IVCTX,
				BIT_TPI_COPP_DATA2_CANCEL_PROT_EN, true);
	}
}

void hdcptx1_intermed_ri_check_enable(bool en)
{
	hdmitx21_set_bit(TPI_COPP_DATA2_IVCTX, BIT_TPI_COPP_DATA2_INTERM_RI_CHECK_EN, en);
}

void hdcptx1_encryption_update(bool en)
{
	hdmitx21_set_bit(TPI_COPP_DATA2_IVCTX, BIT_TPI_COPP_DATA2_INTR_ENCRYPTION, !en);
}

void hdcptx1_auth_start(void)
{
	hdcptx1_load_key();

	hdmitx21_set_bit(LM_DDC_IVCTX, BIT_LM_DDC_SWTPIEN_B7, true);
	hdmitx21_set_bit(TPI_COPP_DATA2_IVCTX, BIT_TPI_COPP_DATA2_CANCEL_PROT_EN, false);
	hdmitx21_set_bit(TPI_COPP_DATA2_IVCTX,
		(BIT_TPI_COPP_DATA2_TPI_HDCP_PREP_EN |
		BIT_TPI_COPP_DATA2_DOUBLE_RI_CHECK |
		BIT_TPI_COPP_DATA2_INTERM_RI_CHECK_EN |
		/* BIT_TPI_COPP_DATA2_KSV_FORWARD | */
		BIT_TPI_COPP_DATA2_COPP_PROTLEVEL), true);
	hdmitx21_set_bit(LM_DDC_IVCTX, BIT_LM_DDC_SWTPIEN_B7, false);
}

void hdcptx1_auth_stop(void)
{
	hdmitx21_set_bit(TPI_COPP_DATA2_IVCTX,
		(BIT_TPI_COPP_DATA2_TPI_HDCP_PREP_EN | BIT_TPI_COPP_DATA2_COPP_PROTLEVEL), false);
}

u8 hdcptx1_copp_status_get(void)
{
	return hdmitx21_rd_reg(TPI_COPP_DATA1_IVCTX);
}

void hdcptx1_bcaps_get(u8 *p_bcaps_status)
{
	*p_bcaps_status = hdmitx21_rd_reg(TPI_DS_BCAPS_IVCTX);
}

bool hdcptx1_ds_rptr_capability(void)
{
	if (hdmitx21_rd_reg(TPI_COPP_DATA1_IVCTX) & BIT_TPI_COPP_DATA1_HDCP_REP)
		return true;
	else
		return false;
}

void hdcptx1_bstatus_get(u8 *p_ds_bstatus)
{
	hdmitx21_seq_rd_reg(TPI_BSTATUS1_IVCTX, p_ds_bstatus, 2);
}

void hdcptx1_get_ds_ksvlists(u8 **p_ksv, u8 count)
{
	u16 bytes_to_read = 0;
	u16 fifo_byte_counter = 0;
	u8 time_out = 100; /* timeout for reading ds ksv list */
	u8 fifo_status = 0;
	int temp = 0;
	int i = 0;
	u32 fifo_byte;

	/* Clear hash done interrupt */
	hdmitx21_set_bit(INTR2_SW_TPI_IVCTX, BIT_INTR2_SW_HASH_DONE_B6, true);

	fifo_byte_counter = count * KSV_SIZE;
	while ((fifo_byte_counter != 0) && time_out--) {
		if (bytes_to_read != 0) {
			/* get ds bksv list from fifo */
			/* hdmitx21_fifo_read(TPI_KSV_FIFO_FORW_IVCTX, *p_ksv, bytes_to_read); */
			for (i = 0; i < bytes_to_read; i++) {
				fifo_byte = hdmitx21_rd_reg(TPI_KSV_FIFO_FORW_IVCTX);
				(*p_ksv)[i] = fifo_byte & 0xff;
				hdmitx21_wr_reg(TPI_KSV_FIFO_FORW_IVCTX, fifo_byte);
				/* printk("%02x\n", (*p_ksv)[i]); */
			}
			temp = temp + bytes_to_read;
			if ((temp % 64) == 0) {
				temp = 0;
				/* wait for hash done interrupt */
				while ((hdmitx21_rd_reg(INTR2_SW_TPI_IVCTX) & BIT(6)) == 0)
					;
				/* clear hash done interrupt */
				hdmitx21_set_bit(INTR2_SW_TPI_IVCTX,
					BIT_INTR2_SW_HASH_DONE_B6, true);
			}
			*p_ksv += bytes_to_read;
			fifo_byte_counter -= bytes_to_read;
		}
		fifo_status = hdmitx21_rd_reg(TPI_KSV_FIFO_STAT_IVCTX);
		bytes_to_read = fifo_status & BIT_TPI_KSV_FIFO_STAT_BYTES;
		/* printk("ksv_fifo_status: 0x%x\n", fifo_status); */
		mdelay(1);
	}
}

bool hdcptx2_ds_rptr_capability(void)
{
	if (hdmitx21_rd_reg(CP2TX_GEN_STATUS_IVCTX) & BIT_CP2TX_GEN_STATUS_RO_REPEATER)
		return true;
	else
		return false;
}

void hdcptx2_encryption_update(bool en)
{
	hdmitx21_set_bit(HDCP2X_CTL_0_IVCTX, BIT_HDCP2X_CTL_0_ENCRYPT_EN, en);
}

void hdcptx2_csm_send(struct hdcp_csm_t *csm_msg)
{
	hdmitx21_wr_reg(CP2TX_RPT_SMNG_K_IVCTX, 1);
	hdmitx21_wr_reg(CP2TX_SEQ_NUM_M_0_IVCTX, (u8)(csm_msg->seq_num_m & 0xFF));
	hdmitx21_wr_reg(CP2TX_SEQ_NUM_M_1_IVCTX, (u8)((csm_msg->seq_num_m >> 8) & 0xff));
	hdmitx21_wr_reg(CP2TX_SEQ_NUM_M_2_IVCTX, (u8)((csm_msg->seq_num_m >> 16) & 0xff));
	hdmitx21_wr_reg(CP2TX_TX_CTRL_0_IVCTX, BIT_CP2TX_TX_CTRL_0_RI_RPT_SMNG_WR_START);
	hdmitx21_wr_reg(CP2TX_TX_CTRL_0_IVCTX, 0x00);
	hdmitx21_wr_reg(CP2TX_TX_RPT_SMNG_IN_IVCTX, (u8)(csm_msg->streamid_type >> 8));
	hdmitx21_wr_reg(CP2TX_TX_CTRL_0_IVCTX, BIT_CP2TX_TX_CTRL_0_RI_RPT_SMNG_WR);
	hdmitx21_wr_reg(CP2TX_TX_CTRL_0_IVCTX, 0);
	hdmitx21_wr_reg(CP2TX_TX_RPT_SMNG_IN_IVCTX, (u8)(csm_msg->streamid_type));
	hdmitx21_wr_reg(CP2TX_TX_CTRL_0_IVCTX, BIT_CP2TX_TX_CTRL_0_RI_RPT_SMNG_WR);
	hdmitx21_wr_reg(CP2TX_TX_CTRL_0_IVCTX, 0);
}

void hdcptx2_rpt_smng_xfer_start(void)
{
	hdmitx21_wr_reg(CP2TX_TX_CTRL_0_IVCTX, BIT_CP2TX_TX_CTRL_0_RI_RPT_SMNG_XFER_START);
	hdmitx21_wr_reg(CP2TX_TX_CTRL_0_IVCTX, 0);
}

bool hdcptx2_auth_status(void)
{
	u8 hdcp2_auth_status = hdmitx21_rd_reg(CP2TX_AUTH_STAT_IVCTX);
	u8 hdcp2_auth_state_status = hdmitx21_rd_reg(CP2TX_STATE_IVCTX);

	if (hdcp2_auth_status == 0x81 && hdcp2_auth_state_status == 0x2B)
		return true;
	else
		return false;
}

void hdcptx2_auth_stop(void)
{
	u8 ddc_status;
	u8 count = 0;

	hdmitx21_wr_reg(HDCP2X_POLL_CS_IVCTX, 0x71);

	hdmitx21_set_bit(HDCP2X_CTL_1_IVCTX, BIT_HDCP2X_CTL_1_HPD_SW, false);
	hdmitx21_set_bit(HDCP2X_CTL_1_IVCTX, BIT_HDCP2X_CTL_1_HPD_OVR, true);
	hdmitx21_set_bit(HDCP2X_CTL_1_IVCTX, BIT_HDCP2X_CTL_1_REAUTH_SW, true);
	hdmitx21_set_bit(HDCP2X_CTL_1_IVCTX, BIT_HDCP2X_CTL_1_REAUTH_SW, false);

	do {
		ddc_status = hdmitx21_rd_reg(HDCP2X_DDC_STS_IVCTX);
		if ((ddc_status & BIT_REGTX0_HDCP2X_DDC_STS_CTL_CS_B3_B0) == false)
			break;
		count++;
		if ((ddc_status & BIT_REGTX0_HDCP2X_DDC_STS_CTL_CS_B3_B0) == 0x0E) {
			hdmitx21_set_bit(HDCP2X_CTL_0_IVCTX, BIT_HDCP2X_CTL_0_EN, false);
			hdmitx21_set_bit(TPI_DDC_MASTER_EN_IVCTX,
				BIT_TPI_DDC_MASTER_EN_HW_EN, true);
			hdmitx21_wr_reg(DDC_CMD_IVCTX, BIT_DDC_CMD_DDC_CMD);
			usleep_range(2000, 3000);
			hdmitx21_set_bit(TPI_DDC_MASTER_EN_IVCTX,
				BIT_TPI_DDC_MASTER_EN_HW_EN, false);
			hdmitx21_set_bit(HDCP2X_CTL_0_IVCTX, BIT_HDCP2X_CTL_0_EN, true);
		}
		usleep_range(1000, 2000);
	} while (count < 5);
	hdmitx21_set_bit(HDCP2X_CTL_0_IVCTX, BIT_HDCP2X_CTL_0_EN, false);
}

void hdcptx2_reauth_send(void)
{
	hdmitx21_wr_reg(HDCP2X_POLL_CS_IVCTX, 0x70);  /* Enable Polling */
	hdmitx21_set_bit(HDCP2X_CTL_1_IVCTX, BIT_HDCP2X_CTL_1_HPD_SW, true);
	hdmitx21_set_bit(HDCP2X_CTL_1_IVCTX, BIT_HDCP2X_CTL_1_REAUTH_SW, true);
	hdmitx21_set_bit(HDCP2X_CTL_1_IVCTX, BIT_HDCP2X_CTL_1_REAUTH_SW, false);

	hdmitx21_wr_reg(CP2TX_CTRL_1_IVCTX, 0x21);
	hdmitx21_wr_reg(CP2TX_CTRL_1_IVCTX, 0x20);
	/* hdmitx21_set_bit(AON_CYP_CTL_IVCTX, BIT(3), false); */
}

u8 hdcptx2_topology_get(void)
{
	return hdmitx21_rd_reg(CP2TX_RPT_DETAIL_IVCTX);
}

u8 hdcptx2_rpt_dev_cnt_get(void)
{
	return hdmitx21_rd_reg(CP2TX_RPT_DEVCNT_IVCTX);
}

u8 hdcptx2_rpt_depth_get(void)
{
	return hdmitx21_rd_reg(CP2TX_RPT_DEPTH_IVCTX);
}

void hdcptx2_ds_rpt_rcvid_list_read(u8 *p_rpt_rcv_id, u8 dev_count, u8 bytes_to_read)
{
	u8 i, j;

	hdmitx21_set_bit(CP2TX_TX_CTRL_0_IVCTX, BIT_CP2TX_TX_CTRL_0_RI_RPT_RCVID_RD_START, true);
	hdmitx21_set_bit(CP2TX_TX_CTRL_0_IVCTX, BIT_CP2TX_TX_CTRL_0_RI_RPT_RCVID_RD_START, false);

	for (j = 0; j < dev_count; j++) {
		for (i = 0; i < bytes_to_read; i++) {
			*p_rpt_rcv_id++ = hdmitx21_rd_reg(CP2TX_RPT_RCVID_OUT_IVCTX);
			hdmitx21_set_bit(CP2TX_TX_CTRL_0_IVCTX,
				BIT_CP2TX_TX_CTRL_0_RI_RPT_RCVID_RD, false);
			hdmitx21_set_bit(CP2TX_TX_CTRL_0_IVCTX,
				BIT_CP2TX_TX_CTRL_0_RI_RPT_RCVID_RD, true);
		}
	}
}

void hdcptx2_ds_rcv_id_read(u8 *p_rcv_id)
{
	p_rcv_id[4] = hdmitx21_rd_reg(CP2TX_RX_ID_CORE_0_IVCTX);
	p_rcv_id[3] = hdmitx21_rd_reg(CP2TX_RX_ID_CORE_1_IVCTX);
	p_rcv_id[2] = hdmitx21_rd_reg(CP2TX_RX_ID_CORE_2_IVCTX);
	p_rcv_id[1] = hdmitx21_rd_reg(CP2TX_RX_ID_CORE_3_IVCTX);
	p_rcv_id[0] = hdmitx21_rd_reg(CP2TX_RX_ID_CORE_4_IVCTX);
}

void hdcptx2_src_auth_start(u8 content_type)
{
	u32 reset_val = 0;

	if (content_type != 0 && content_type != 1)
		content_type = 0;

	/* reset hdcp2x logic and HW state machine */
	/* mostly, ddc bus is already free after previous stop
	 * operation, now double check
	 */
	if (!ddc_bus_wait_free())
		pr_info("%s: reset during ddc busy!!\n", __func__);
	reset_val = hdmitx21_rd_reg(HDCP2X_TX_SRST_IVCTX);
	//hdmitx21_set_bit(HDCP2X_TX_SRST_IVCTX, BIT(5), true);
	hdmitx21_wr_reg(HDCP2X_TX_SRST_IVCTX, reset_val | 0x20);
	//hdmitx21_set_bit(HDCP2X_TX_SRST_IVCTX, BIT(5), false);
	hdmitx21_wr_reg(HDCP2X_TX_SRST_IVCTX, reset_val &  (~0x20));

	hdmitx21_set_bit(AON_CYP_CTL_IVCTX, BIT(3), true);
	hdmitx21_set_bit(AON_CYP_CTL_IVCTX, BIT(3), false);

	hdmitx21_set_bit(HDCP2X_CTL_1_IVCTX, BIT_HDCP2X_CTL_1_HPD_SW, true);
	hdmitx21_set_bit(HDCP2X_CTL_1_IVCTX, BIT_HDCP2X_CTL_1_HPD_OVR, true);
	hdmitx21_wr_reg(CP2TX_GP_IN2_IVCTX,
		BIT_CP2TX_TIMEOUT_DISABLE_B2 | BIT_CP2TX_IGNORE_RXSTATUS_B0);
	hdmitx21_set_bit(HDCP2X_CTL_0_IVCTX,
		BIT_HDCP2X_CTL_0_EN | BIT_HDCP2X_CTL_0_ENCRYPT_EN, true);
}

void hdcptx2_smng_auto(bool en)
{
	hdmitx21_set_bit(CP2TX_CTRL_0_IVCTX, BIT_CP2TX_CTRL_0_RI_SMNG_AUTO, en);
}

u8 hdcp2x_get_state_st(void)
{
	return hdmitx21_rd_reg(CP2TX_STATE_IVCTX);
}

void hdcptx1_query_aksv(struct hdcp_ksv_t *p_val)
{
	hdmitx21_seq_rd_reg(AKSV_1_IVCTX, p_val->b, KSV_SIZE);
}

void hdcptx_en_aes_dualpipe(bool en)
{
	hdmitx21_set_reg_bits(CP2TX_AESCTL_IVCTX, en ? 3 : 0, 2, 2);
}
