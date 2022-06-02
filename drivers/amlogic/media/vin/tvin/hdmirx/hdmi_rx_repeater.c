// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#ifdef CONFIG_AMLOGIC_HDMITX
#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
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
#include <linux/of_gpio.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
/* Local include */
#include "hdmi_rx_repeater.h"
#include "hdmi_rx_drv.h"
#include "hdmi_rx_hw.h"
#include "hdmi_rx_wrapper.h"
#include "hdmi_rx_edid.h"
/*edid original data from device*/
static unsigned char receive_edid[MAX_RECEIVE_EDID];
int receive_edid_len = MAX_RECEIVE_EDID;
MODULE_PARM_DESC(receive_edid, "\n receive_edid\n");
module_param_array(receive_edid, byte, &receive_edid_len, 0664);
int tx_hpd_event;
int edid_len;
MODULE_PARM_DESC(edid_len, "\n edid_len\n");
module_param(edid_len, int, 0664);
bool new_edid;
/*original bksv from device*/
//unsigned char receive_hdcp[MAX_KSV_LIST_SIZE];
//int hdcp_array_len = MAX_KSV_LIST_SIZE;
//MODULE_PARM_DESC(receive_hdcp, "\n receive_hdcp\n");
//module_param_array(receive_hdcp, byte, &hdcp_array_len, 0664);
int hdcp_len;
int hdcp_repeat_depth;
bool new_hdcp;
bool start_auth_14;
MODULE_PARM_DESC(start_auth_14, "\n start_auth_14\n");
module_param(start_auth_14, bool, 0664);

bool repeat_plug;
MODULE_PARM_DESC(repeat_plug, "\n repeat_plug\n");
module_param(repeat_plug, bool, 0664);

int up_phy_addr;/*d c b a 4bit*/
MODULE_PARM_DESC(up_phy_addr, "\n up_phy_addr\n");
module_param(up_phy_addr, int, 0664);
int hdcp22_firm_switch_timeout;

u8 ksvlist[10] = {
	0xc1, 0x29, 0x9b, 0xb6, 0x66,
	0x77, 0x34, 0x43, 0x54, 0x6b,
	//0x88, 0xcb, 0xbc, 0xab, 0x95
};

//unsigned char *rx_get_dw_hdcp_addr(void)
//{
	//return receive_hdcp;
///}

void rx_start_repeater_auth(void)
{
	rx.hdcp.state = REPEATER_STATE_START;
	start_auth_14 = 1;
	rx.hdcp.delay = 0;
	hdcp_len = 0;
	hdcp_repeat_depth = 0;
	rx.hdcp.dev_exceed = 0;
	rx.hdcp.cascade_exceed = 0;
	rx.hdcp.depth = 0;
	rx.hdcp.count = 0;
	//memset(&receive_hdcp, 0, sizeof(receive_hdcp));
}

void rx_update_rpt_sts(struct hdcp_topo_s *topo)
{
	/* todo: only update topo for corresponding hdcp version */
	//if (rx.hdcp.rpt_reauth_event == HDCP_VER_22)
		rpt_update_hdcp2x(topo);
	//else
		rpt_update_hdcp1x(topo);
	rx_pr("update ksv, rpt_reauth_event: %d\n", rx.hdcp.rpt_reauth_event);
}

static struct hdcp_topo_s pre_topo;

/* for HDMIRX/CEC notify */
#define HDMITX_PLUG				1
#define HDMITX_UNPLUG			2
#define HDMITX_PHY_ADDR_VALID	3
#define HDMITX_KSVLIST			4
int rx_hdmi_tx_notify_handler(struct notifier_block *nb,
				     unsigned long value, void *p)
{
	int ret = NOTIFY_DONE;
	int phy_addr = 0;
	struct hdcp_topo_s *topo;

	switch (value) {
	case HDMITX_PLUG:
		if (log_level & EDID_LOG)
			rx_pr("%s, HDMITX_PLUG\n", __func__);
		if (p) {
			rx_pr("update EDID from HDMITX\n");
			rx_update_tx_edid_with_audio_block(p, rx_audio_block);
		}
		rx_irq_en(false);
		rx_set_cur_hpd(0, 4);
		if (!rx.open_fg)
			port_hpd_rst_flag = 7;
		//if (hdmirx_repeat_support())
		rx.hdcp.repeat = true;
		fsm_restart();
		ret = NOTIFY_OK;
		break;
	case HDMITX_UNPLUG:
		if (log_level & EDID_LOG)
			rx_pr("%s, HDMITX_UNPLUG, recover primary EDID\n",
			      __func__);
		rx.hdcp.repeat = false;
		if (rpt_only_mode == 1)
			rx_force_hpd_rxsense_cfg(0);
		else
			rx_update_tx_edid_with_audio_block(NULL, NULL);
		hdmi_rx_top_edid_update();
		hdcp_init_t7();
		//rx_irq_en(false);
		//rx_set_cur_hpd(0, 4);
		//fsm_restart();
		ret = NOTIFY_OK;
		break;
	case HDMITX_PHY_ADDR_VALID:
		phy_addr = *((int *)p);
		if (log_level & EDID_LOG)
			rx_pr("%s, HDMITX_PHY_ADDR_VALID %x\n",
			      __func__, phy_addr & 0xffff);
		ret = NOTIFY_OK;
		break;
	case HDMITX_KSVLIST:
		if (!hdmirx_repeat_support())
			break;
		topo = (struct hdcp_topo_s *)p;
		rx_pr("ksv list!!\n");
		if (!topo) {
			rx_pr("ksv_list is empty\n");
			break;
		}
		/* before request ds re-auth, no need to care ds ksv list */
		if (rx.hdcp.rpt_reauth_event & HDCP_NEED_REQ_DS_AUTH)
			break;
		if (!memcmp(&pre_topo, topo, sizeof(pre_topo))) {
			rx_pr("same topo, exit\n");
			ret = NOTIFY_OK;
			break;
		}
		memcpy(&pre_topo, topo, sizeof(pre_topo));
		rx.hdcp.ds_hdcp_ver = topo->hdcp_ver;
		rx_pr("seq_num_V: 0x%x\n", rx.hdcp.topo_updated);
		rx_update_rpt_sts(topo);
		if (rx.hdcp.rpt_reauth_event == HDCP_VER_22) {
			rx.hdcp.topo_updated++;
			if (rx.hdcp.topo_updated > 0xFFFFFFUL)
				rx.hdcp.topo_updated = 0;
		}
		//hdmirx_wr_cor(RX_SHA_ctrl_HDCP1X_IVCRX, 1);
		mdelay(1);
		rx_pr("step3\n");
		/* todo: only update topo for corresponding hdcp version */
		//if (rx.hdcp.rpt_reauth_event == HDCP_VER_14)
			rx.hdcp.hdcp14_ready = true;
		ret = NOTIFY_OK;
		break;
	default:
		rx_pr("unsupported hdmitx notify:%ld, arg:%p\n",
		      value, p);
		ret = NOTIFY_DONE;
		break;
	}
	return ret;
}

void rx_check_repeat(void)
{
	//struct hdcp14_topo_s *topo_data = (struct hdcp14_topo_s *)receive_hdcp;
	//struct hdcp_topo_s *topo;
	u8 data8 = 0;

	if (!hdmirx_repeat_support())
		return;

	//topo->depth = 1;
	//topo->dev_cnt = 2;
	//RX detect hdcp auth from upstream, but not request tx re-auth yet
	if (rx.hdcp.rpt_reauth_event & HDCP_NEED_REQ_DS_AUTH) {
		//rx_update_rpt_sts(NULL);
		memset(&pre_topo, 0, sizeof(pre_topo));
		rx.hdcp.topo_updated = 0;
		hdmitx_reauth_request(0);
		rx.hdcp.rpt_reauth_event &= (~HDCP_NEED_REQ_DS_AUTH);
		//rx_start_repeater_auth();
		//rx.hdcp.rpt_reauth_event = 0;
	}
	if (rx.hdcp.hdcp14_ready) {
		//hdmirx_wr_cor(RX_SHA_ctrl_HDCP1X_IVCRX, 1);
		hdmirx_wr_cor(RX_BCAPS_SET_HDCP1X_IVCRX, 0xe0);
		rx_pr("step4\n");
		rx.hdcp.hdcp14_ready = false;
	}
	if (rx.hdcp.stream_manage_rcvd) {
		data8 = rx_get_stream_manage_info();
		rx_pr("step5-%d\n", data8);
		hdmitx_reauth_request(data8 | 0x10);
		rx.hdcp.stream_manage_rcvd = false;
	}
}

unsigned char *rx_get_dw_edid_addr(void)
{
	return receive_edid;
}

//int rx_set_receiver_edid(const char *data, int len)
//{
	//if (!data || !len)
	//	return false;

	//if (len > MAX_RECEIVE_EDID || len < 3) {
	//	memset(receive_edid, 0, sizeof(receive_edid));
	//	edid_len = 0;
	///} else {
	//	memcpy(receive_edid, data, len);
	//	edid_len = len;
	//}
	//new_edid = true;
	//return true;
///}

void rx_set_repeater_support(bool enable)
{
	downstream_repeat_support = enable;
	repeat_plug = enable;
	rx_pr("****************=%d\n", downstream_repeat_support);
}
EXPORT_SYMBOL(rx_set_repeater_support);

bool rx_poll_dwc(u16 addr, u32 exp_data,
		 u32 mask, u32 max_try)
{
	u32 rd_data;
	u32 cnt = 0;
	u8 done = 0;

	rd_data = hdmirx_rd_dwc(addr);
	while (((cnt < max_try) || (max_try == 0)) && (done != 1)) {
		if ((rd_data & mask) == (exp_data & mask)) {
			done = 1;
		} else {
			cnt++;
			rd_data = hdmirx_rd_dwc(addr);
		}
	}
	if (done == 0) {
		/* if(log_level & ERR_LOG) */
		rx_pr("poll dwc%x time-out!\n", addr);
		return false;
	}
	return true;
}

bool rx_set_repeat_aksv(unsigned char *data, int len, int depth,
			bool dev_exceed, bool cascade_exceed)
{
	int i;
	bool ksvlist_ready = 0;

	if (data == 0 || ((depth == 0 || len == 0) &&
			  !dev_exceed && !cascade_exceed))
		return false;
	rx_pr("set ksv list len:%d,depth:%d, exceed count:%d,cascade:%d\n",
	      len, depth, dev_exceed, cascade_exceed);
	/*set repeat depth*/
	if (depth <= MAX_REPEAT_DEPTH && !cascade_exceed) {
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS,
				   MAX_CASCADE_EXCEEDED, 0);
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS, DEPTH, depth);
		rx.hdcp.depth = depth;
	} else {
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS,
				   MAX_CASCADE_EXCEEDED, 1);
		rx.hdcp.depth = 0;
	}
	/*set repeat count*/
	if (len <= HDCP14_KSV_MAX_COUNT && !dev_exceed) {
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS,
				   MAX_DEVS_EXCEEDED, 0);
		rx.hdcp.count = len;
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS, DEVICE_COUNT,
				   rx.hdcp.count);
	} else {
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS,
				   MAX_DEVS_EXCEEDED, 1);
		rx.hdcp.count = 0;
	}
	/*set repeat status*/
	/* if (rx.hdcp.count > 0) {
	 *	rx.hdcp.repeat = true;
	 *	hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL, REPEATER, 1);
	 *} else {
	 *	rx.hdcp.repeat = false;
	 *	hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL, REPEATER, 0);
	 *}
	 */
	ksvlist_ready = ((rx.hdcp.count > 0) && (rx.hdcp.depth > 0));
	rx_pr("[RX]write ksv list count:%d\n", rx.hdcp.count);
	/*write ksv list to fifo*/
	for (i = 0; i < rx.hdcp.count; i++) {
		if (rx_poll_dwc(DWC_HDCP_RPT_CTRL, ~KSV_HOLD, KSV_HOLD,
				KSV_LIST_WR_TH)) {
			/*check fifo can be written*/
			hdmirx_wr_dwc(DWC_HDCP_RPT_KSVFIFOCTRL, i);
			hdmirx_wr_dwc(DWC_HDCP_RPT_KSVFIFO1,
				      *(data + i * MAX_KSV_SIZE + 4));
			hdmirx_wr_dwc(DWC_HDCP_RPT_KSVFIFO0,
				      *((u32 *)(data + i * MAX_KSV_SIZE)));
			if (log_level & VIDEO_LOG)
				rx_pr("[RX]write ksv list index:%d, ksv hi:%#x, low:%#x\n",
				      i, *(data + i * MAX_KSV_SIZE + 4),
				      *((u32 *)(data + i * MAX_KSV_SIZE)));
		} else {
			return false;
		}
	}
	/* Wait for ksv_hold=0*/
	rx_poll_dwc(DWC_HDCP_RPT_CTRL, ~KSV_HOLD, KSV_HOLD, KSV_LIST_WR_TH);
	/*set ksv list ready*/
	hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL, KSVLIST_READY, ksvlist_ready);
	/* Wait for HW completion of V value*/
	if (ksvlist_ready)
		rx_poll_dwc(DWC_HDCP_RPT_CTRL, FIFO_READY,
			    FIFO_READY, KSV_V_WR_TH);
	rx_pr("[RX]write Ready signal! ready:%u\n",
	      (unsigned int)ksvlist_ready);

	return true;
}

void rx_set_repeat_signal(bool repeat)
{
	//rx.hdcp.repeat = repeat;
}
#endif
