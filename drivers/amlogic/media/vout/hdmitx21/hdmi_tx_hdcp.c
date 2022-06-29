// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/input.h>
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/extcon-provider.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>
#include "hw/hdmi_tx_reg.h"
#include "hdmi_tx.h"
#include <../../vin/tvin/hdmirx/hdmi_rx_repeater.h>

static int hdmi21_authenticated;
static struct hdcp_t *p_hdcp;
/* hdcp_topo info transfrred to upstream, 1.4 & 2.2 */
struct hdcp_topo_s hdcp_topo;

static int hdcp_verbose;

module_param(hdcp_verbose, int, 0644);
MODULE_PARM_DESC(hdcp_verbose, "for hdcp debug");

MODULE_PARM_DESC(hdmi21_authenticated, "\n hdmi21_authenticated\n");
module_param(hdmi21_authenticated, int, 0444);

/* notify delay in ms, for debug */
static int notify_delay_ms = 1;

static unsigned int delay_ms = 10;
unsigned long hdcp_reauth_dbg = 1;
unsigned long streamtype_dbg;
unsigned long en_fake_rcv_id;
/* static u8 fake_rcv_id[5] = {0x0}; */

static bool hdcp_schedule_work(struct hdcp_work *work, u32 delay_ms, u32 period_ms);
static bool hdcp_stop_work(struct hdcp_work *work);
static void hdcptx_update_failures(struct hdcp_t *p_hdcp, enum hdcp_fail_types_t types);
static bool hdcp1x_ds_ksv_fifo_ready(struct hdcp_t *p_hdcp, u8 int_reg[]);
static void hdcp2x_auth_stop(struct hdcp_t *p_hdcp);
static void hdcptx_send_csm_msg(struct hdcp_t *p_hdcp);
static void hdcptx_reset(struct hdcp_t *p_hdcp);
static void bksv_get_ds_list(struct hdcp_t *p_hdcp);
static void get_ds_rcv_id(struct hdcp_t *p_hdcp);
static void hdcp_update_csm(struct hdcp_t *p_hdcp);

void pr_hdcp_info(const char *fmt, ...)
{
	va_list args;
	int len;
	char temp[128] = {0};

	if (!hdcp_verbose)
		return;

	va_start(args, fmt);
	len = vsnprintf(temp, sizeof(temp), fmt, args);
	va_end(args);

	if (len)
		pr_info("%s", temp);
}

/* for hdcp repeater, downstream auth should be controlled
 * (started) by upstream side, and should not started
 * until upstream request, otherwise will cause most
 * of the cts items fail. for example:
 * when start hdcp1.4 3B-01b, hdmitx detects hpd reset-->
 * hdmitx read EDID and notify hdmirx to hpd reset-->
 * hdmitx set mode and enable hdcp1.4 auth-->
 * hdmirx side detect hdcp1.4 auth from TE source-->
 * hdmirx notify hdmitx side to re-auth-->
 * the first hdcp1.4 auth break and then restart-->
 * TE doesn't respond to this re-auth, timeout and fail.
 */
bool hdcp_need_control_by_upstream(struct hdmitx_dev *hdev)
{
	if (!hdev->repeater_tx)
		return false;

	if (!get_rx_active_sts())
		return false;
	return true;
}

void hdcp_mode_set(unsigned int mode)
{
	pr_hdcp_info("%s[%d] %d\n", __func__, __LINE__, mode);

	if (mode == 0) {
		hdcptx_reset(p_hdcp);
		p_hdcp->hdcptx_enabled = 0;
		return;
	}

	if (mode != 1 && mode != 2)
		return;

	p_hdcp->hdcptx_enabled = 1;
	if (mode == 1)
		p_hdcp->req_hdcp_ver = HDCP_VER_HDCP1X;
	if (mode == 2)
		p_hdcp->req_hdcp_ver = HDCP_VER_HDCP2X;
	hdcp_schedule_work(&p_hdcp->timer_hdcp_start, 1, 0);
}

static bool hdcp1x_ds_ksv_fifo_ready(struct hdcp_t *p_hdcp, u8 int_reg[])
{
	u8 bcaps_status;

	hdcptx1_bcaps_get(&bcaps_status);
	if ((int_reg[0] & 0x08) || (bcaps_status & 0x20))
		return true;
	else
		return false;
}

static void update_hdcp_state(struct hdcp_t *p_hdcp, enum hdcp_stat_t state)
{
	if (p_hdcp->hdcp_state != state || state == HDCP_STAT_NONE) {
		p_hdcp->hdcp_state = state;
		pr_hdcp_info("%s[%d] %d\n", __func__, __LINE__, state);
	}
}

static void hdcp2x_reauth_start(struct hdcp_t *p_hdcp)
{
	if (!p_hdcp->reauth_ignored)
		hdcptx2_reauth_send();
}

static void hdcp_topology_update(struct hdcp_t *p_hdcp)
{
	struct hdcp_topo_t *topo = &p_hdcp->hdcp_topology;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
		if (p_hdcp->ds_repeater) {
			u8 topoval;

			topoval = hdcptx2_topology_get();
			/* include the count/depth of downstream repeater itself.
			 * the rcvid list of downstream repeater will be added to notify
			 * list in assemble_ds_ksv_lists()
			 */
			if (hdcp_need_control_by_upstream(hdev)) {
				topo->rp_depth = hdcptx2_rpt_depth_get() + 1;
				topo->dev_count = hdcptx2_rpt_dev_cnt_get() + 1;
			} else {
				topo->rp_depth = hdcptx2_rpt_depth_get();
				topo->dev_count = hdcptx2_rpt_dev_cnt_get();
			}
			if (topoval & 0x08 || topo->dev_count > HDCP2X_MAX_DEV)
				topo->max_devs_exceed = true;
			else
				topo->max_devs_exceed = false;
			if (topoval & 0x04 || topo->rp_depth > HDCP2X_MAX_DEPTH)
				topo->max_cas_exceed = true;
			else
				topo->max_cas_exceed = false;
			topo->ds_hdcp2x_dev = (topoval & 0x02) ? true : false;
			topo->ds_hdcp1x_dev = (topoval & 0x01) ? true : false;

			if (topo->max_devs_exceed)
				topo->dev_count = HDCP2X_MAX_DEV;
			if (p_hdcp->hdcp_topology.max_cas_exceed)
				topo->rp_depth = HDCP2X_MAX_DEPTH;
		} else {
			topo->rp_depth = 1;
			topo->dev_count = 1;
			topo->max_devs_exceed = false;
			topo->max_cas_exceed = false;
			topo->ds_hdcp2x_dev = false;
			topo->ds_hdcp1x_dev = false;
		}
	} else if (p_hdcp->hdcp_type == HDCP_VER_HDCP1X) {
		if (p_hdcp->ds_repeater) {
			u8 bstatus[2];
			u8 max_devices = hdev->repeater_tx ?
				HDCP1X_MAX_TX_DEV_RPT : HDCP1X_MAX_TX_DEV_SRC;

			hdcptx1_bstatus_get(bstatus);
			/* include the count/depth of downstream repeater itself.
			 * the bksv of downstream repeater will be added to notify
			 * list in assemble_ds_ksv_lists()
			 */
			if (hdcp_need_control_by_upstream(hdev)) {
				topo->rp_depth = (bstatus[1] & 0x07) + 1;
				topo->dev_count = (bstatus[0] & 0x7F) + 1;
			} else {
				topo->rp_depth = bstatus[1] & 0x07;
				topo->dev_count = bstatus[0] & 0x7F;
			}
			if (bstatus[0] & 0x80 || topo->dev_count > max_devices)
				topo->max_devs_exceed = true;
			else
				topo->max_devs_exceed = false;
			if (bstatus[1] & 0x08 || topo->rp_depth > HDCP1X_MAX_DEPTH)
				topo->max_cas_exceed = true;
			else
				topo->max_cas_exceed = false;
			topo->ds_hdcp2x_dev = false;
			topo->ds_hdcp1x_dev = true;

			if (topo->max_devs_exceed)
				topo->dev_count = max_devices;
			if (topo->max_cas_exceed)
				topo->rp_depth = HDCP1X_MAX_DEPTH;
		} else {
			/* include the depth of repeater itself */
			topo->rp_depth = 1;
			topo->dev_count = 1;
			topo->max_devs_exceed = false;
			topo->max_cas_exceed = false;
			topo->ds_hdcp2x_dev = false;
			topo->ds_hdcp1x_dev = true;
			/* hdcptx1_ds_bksv_read(p_hdcp->p_ksv_lists, KSV_SIZE); */
		}
	}
}

static void hdcptx_encryption_update(struct hdcp_t *p_hdcp, bool en)
{
	if (p_hdcp->ds_auth) {
		p_hdcp->encryption_enabled = en;

		if (p_hdcp->hdcp_type == HDCP_VER_HDCP1X)
			hdcptx1_encryption_update(en);
		else if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
			hdcptx1_encryption_update(en);
			hdcptx2_encryption_update(en);
		}
	}
}

static void hdcp_check_ds_csm_status(struct hdcp_t *p_hdcp)
{
	pr_hdcp_info("%s[%d] ds_repeater %d  hdcp_type %d  csm_valid %d  content_type %d\n",
		__func__, __LINE__, p_hdcp->ds_repeater, p_hdcp->hdcp_type,
		p_hdcp->csm_valid, p_hdcp->content_type);
	if (p_hdcp->ds_repeater) {
		if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
			if (p_hdcp->csm_valid)
				hdcptx_send_csm_msg(p_hdcp);
		} else {
			update_hdcp_state(p_hdcp, HDCP_STAT_SUCCESS);
		}
	} else {
		if (p_hdcp->hdcp_type == HDCP_VER_HDCP1X) {
			update_hdcp_state(p_hdcp, HDCP_STAT_SUCCESS);
		} else {
			if (p_hdcp->csm_valid) {
				if (p_hdcp->content_type == HDCP_CONTENT_TYPE_1)
					hdcptx_update_failures(p_hdcp, HDCP_FAIL_CONTENT_TYPE);
				else if (p_hdcp->content_type == HDCP_CONTENT_TYPE_0)
					update_hdcp_state(p_hdcp, HDCP_STAT_SUCCESS);
				else
					hdcptx_update_failures(p_hdcp, HDCP_FAIL_CONTENT_TYPE);
			} else {
				update_hdcp_state(p_hdcp, HDCP_STAT_SUCCESS);
			}
		}
	}
}

static void hdcp_authenticated_handle(struct hdcp_t *p_hdcp)
{
	pr_hdcp_info("hdcptx: part 1 done\n");
	if (p_hdcp->hdcp_type == HDCP_VER_HDCP1X) {
		hdcp_stop_work(&p_hdcp->timer_hdcp_rcv_auth);
		p_hdcp->ds_auth = true;
		pr_hdcp_info("hdcptx: 1x AuthDone\n");
		p_hdcp->fail_type = HDCP_FAIL_NONE;
		hdcptx_encryption_update(p_hdcp, true);
		hdcp_topology_update(p_hdcp);
		bksv_get_ds_list(p_hdcp);
		hdcp_check_ds_csm_status(p_hdcp);
	} else if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
		hdcp_stop_work(&p_hdcp->timer_hdcp_rcv_auth);
		p_hdcp->ds_auth = true;
		pr_hdcp_info("hdcptx: 2x AuthDone\n");
		p_hdcp->fail_type = HDCP_FAIL_NONE;
		hdcptx_encryption_update(p_hdcp, true);
		p_hdcp->ds_repeater = false;
		hdcp_topology_update(p_hdcp);
		get_ds_rcv_id(p_hdcp);
		/* ds is hdcp2.2 TV */
		set_hdcp2_topo(1);
		hdcp_check_ds_csm_status(p_hdcp);
	}
}

static bool hdcptx_query_ds_repeater(struct hdcp_t *p_hdcp)
{
	if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X)
		return hdcptx2_ds_rptr_capability();
	else
		return hdcptx1_ds_rptr_capability();
}

static bool is_bksv_valid(struct hdcp_t *p_hdcp)
{
	u8 bksv[KSV_SIZE];
	u8 i, count = 0;

	hdcptx1_ds_bksv_read(bksv, KSV_SIZE);

	for (i = 0; i < KSV_SIZE; i++) {
		while (bksv[i] != 0) {
			if (bksv[i] & 1)
				count++;
			bksv[i] >>= 1;
		}
	}
	return count == 20;
}

static void hdcp1x_check_bksv_done(struct hdcp_t *p_hdcp)
{
	u8 copp_data1;

	copp_data1 = hdcptx1_ds_cap_status_get();
	pr_hdcp_info("%s[%d] copp_data1 0x%02x\n", __func__, __LINE__,
		copp_data1);

	if (copp_data1 & 0x02) {
		hdcp_stop_work(&p_hdcp->timer_bksv_poll_done);
		hdcptx1_protection_enable(true);
		hdcptx1_intermed_ri_check_enable(1);
	}
}

static void ksv_reset_fifo(struct hdcp_t *p_hdcp)
{
	p_hdcp->p_ksv_next = p_hdcp->p_ksv_lists;
}

static bool is_topology_correct(struct hdcp_t *p_hdcp)
{
	u8 max_depth = HDCP1X_MAX_DEPTH;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	u8 max_device_count = hdev->repeater_tx ? HDCP1X_MAX_TX_DEV_RPT : HDCP1X_MAX_TX_DEV_SRC;

	if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
		max_device_count = HDCP2X_MAX_DEV;
		max_depth = HDCP2X_MAX_DEPTH;
	}
	if (p_hdcp->hdcp_topology.max_devs_exceed || p_hdcp->hdcp_topology.max_cas_exceed)
		return false;

	return true;
}

static void get_ds_rcvid_lists(struct hdcp_t *p_hdcp, u8 count)
{
	hdcptx2_ds_rpt_rcvid_list_read(p_hdcp->p_ksv_next, count, KSV_SIZE);
	p_hdcp->p_ksv_next += count * KSV_SIZE;
}

static void get_ds_rcv_id(struct hdcp_t *p_hdcp)
{
	hdcptx2_ds_rcv_id_read(p_hdcp->p_ksv_next);
	p_hdcp->p_ksv_next += KSV_SIZE;
}

static void bksv_get_ds_list(struct hdcp_t *p_hdcp)
{
	hdcptx1_ds_bksv_read(p_hdcp->p_ksv_next, KSV_SIZE);
	p_hdcp->p_ksv_next += KSV_SIZE;
}

static void assemble_ds_ksv_lists(struct hdcp_t *p_hdcp)
{
	if (p_hdcp->hdcp_type == HDCP_VER_HDCP1X) {
		u8 ds_bstatus[2];
		u8 ds_count;
		u8 ds_depth;

		hdcptx1_bstatus_get(ds_bstatus);
		ds_count = ds_bstatus[0] & 0x7F;
		ds_depth = ds_bstatus[1] & 0x07;
		hdcptx1_get_ds_ksvlists(&p_hdcp->p_ksv_next, ds_count);
		/* include the bksv of downstream repeater, add to the
		 * ksv list which will report to upstream
		 */
		bksv_get_ds_list(p_hdcp);
	} else if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
		u8 dev_cnt = hdcptx2_rpt_dev_cnt_get();

		get_ds_rcvid_lists(p_hdcp, dev_cnt);
		get_ds_rcv_id(p_hdcp);
	}
}

static bool hdcp_process_repeater_fail(struct hdcp_t *p_hdcp)
{
	if (p_hdcp->hdcp_type == HDCP_VER_HDCP1X) {
		u8 ds_bstatus[2];

		bksv_get_ds_list(p_hdcp);

		hdcptx1_bstatus_get(ds_bstatus);
		if ((ds_bstatus[0] & 0x80) || (ds_bstatus[1] & 0x08)) {
			hdcptx_update_failures(p_hdcp, HDCP_FAIL_TOPOLOGY);
			return false;
		}
		hdcptx_update_failures(p_hdcp, HDCP_FAIL_V);
	} else if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
		u8 hdcp_topology;

		hdcp_topology = hdcptx2_topology_get();
		if ((hdcp_topology & 0x04) || (hdcp_topology & 0x08)) {
			hdcptx_update_failures(p_hdcp, HDCP_FAIL_TOPOLOGY);
			return false;
		}
		hdcptx_update_failures(p_hdcp, HDCP_FAIL_READY_TO);
	}
	return true;
}

static void hdcp_rpt_ready_process(struct hdcp_t *p_hdcp, bool ksv_read_status)
{
	hdcp_stop_work(&p_hdcp->timer_hdcp_rpt_auth);
	hdcp_stop_work(&p_hdcp->timer_hdcp_rcv_auth);
	if (!(p_hdcp->hdcp_state == HDCP_STAT_FAILED ||
	      p_hdcp->hdcp_state == HDCP_STAT_AUTH)) {
		if (!(p_hdcp->hdcp_state == HDCP_STAT_SUCCESS ||
		      p_hdcp->hdcp_state == HDCP_STAT_CSM)) {
			hdcptx_update_failures(p_hdcp, HDCP_FAIL_UNKNOWN);
			return;
		}
	}
	if (!ksv_read_status) {
		hdcp_topology_update(p_hdcp);
		ksv_reset_fifo(p_hdcp);
		if (!hdcp_process_repeater_fail(p_hdcp))
			return;
	} else {
		hdcp_topology_update(p_hdcp);
		if (is_topology_correct(p_hdcp)) {
			assemble_ds_ksv_lists(p_hdcp);
			if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
				if (p_hdcp->csm_msg_sent) {
					update_hdcp_state(p_hdcp, HDCP_STAT_SUCCESS);
				} else {
					update_hdcp_state(p_hdcp, HDCP_STAT_CSM);
					if (p_hdcp->csm_valid)
						hdcptx_send_csm_msg(p_hdcp);
				}
			}
		} else {
			assemble_ds_ksv_lists(p_hdcp);
			hdcptx_update_failures(p_hdcp, HDCP_FAIL_TOPOLOGY);
			return;
		}
	}
	p_hdcp->rpt_ready = true;
	pr_hdcp_info("%s[%d] rpt_ready %d\n", __func__, __LINE__,
		p_hdcp->rpt_ready);
}

static void hdcp_notify_rpt_info(struct work_struct *work)
{
	unsigned int ksv_bytes = 0;
	unsigned int i = 0;
	unsigned int j = 0;
	struct hdcp_t *p_hdcp = container_of((struct delayed_work *)work,
		struct hdcp_t, ksv_notify_wk);

	memset(&hdcp_topo, 0, sizeof(hdcp_topo));
	hdcp_topo.hdcp_ver = p_hdcp->hdcp_type;
	pr_hdcp_info("hdcp_type: %d\n", p_hdcp->hdcp_type);
	hdcp_topo.depth = p_hdcp->hdcp_topology.rp_depth;
	hdcp_topo.dev_cnt = p_hdcp->hdcp_topology.dev_count;
	if (en_fake_rcv_id) {
		/* memcpy(p_hdcp->p_ksv_next, fake_rcv_id, ARRAY_SIZE(fake_rcv_id)); */
		/* p_hdcp->p_ksv_next += ARRAY_SIZE(fake_rcv_id); */
		/* hdcp_topo.dev_cnt += 1; */
	}
	hdcp_topo.max_cascade_exceeded = p_hdcp->hdcp_topology.max_cas_exceed;
	hdcp_topo.max_devs_exceeded = p_hdcp->hdcp_topology.max_devs_exceed;
	hdcp_topo.hdcp1_dev_ds = p_hdcp->hdcp_topology.ds_hdcp1x_dev;
	hdcp_topo.hdcp2_dev_ds = p_hdcp->hdcp_topology.ds_hdcp2x_dev;
	ksv_bytes = hdcp_topo.dev_cnt * 5; //p_hdcp->p_ksv_next - p_hdcp->p_ksv_lists;
	ksv_bytes = ksv_bytes > ARRAY_SIZE(hdcp_topo.ksv_list) ?
		ARRAY_SIZE(hdcp_topo.ksv_list) : ksv_bytes;
	memcpy(hdcp_topo.ksv_list, p_hdcp->p_ksv_lists, ksv_bytes);
	pr_hdcp_info("%s depth:[%d] cnt:[%d] ksv_bytes:[%d] max_cas: [%d], max_dev: [%d]\n",
		__func__, hdcp_topo.depth, hdcp_topo.dev_cnt, ksv_bytes,
		hdcp_topo.max_cascade_exceeded, hdcp_topo.max_devs_exceeded);
	for (i = 0; i < ksv_bytes / 5; i++) {
		pr_hdcp_info("ksv list[%d]: ", i);
		for (j = 0; j < 5; j++)
			pr_hdcp_info("%02x\n", hdcp_topo.ksv_list[5 * i + j]);
		pr_hdcp_info("\n");
	}
	hdmitx21_event_notify(HDMITX_KSVLIST, &hdcp_topo);
}

static void hdcp_req_reauth_whandler(struct work_struct *work)
{
	struct hdcp_t *p_hdcp = container_of((struct delayed_work *)work,
			struct hdcp_t, req_reauth_wk);
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	/* firstly need to disable current hdcp auth */
	if (hdcp_reauth_dbg == 1) {
		hdcp_mode_set(0);
		mdelay(delay_ms);
	}
	if (p_hdcp->req_reauth_ver == 0) {
		/* auto mode, need to use ds hdcp version */
		hdmitx21_enable_hdcp(hdev);
	} else if (p_hdcp->req_reauth_ver == 1) {
		/* force hdcp1.x mode */
		if (get_hdcp1_lstore()) {
			hdev->hdcp_mode = 1;
			hdcp_mode_set(1);
		} else {
			hdev->hdcp_mode = 0;
		}
	} else if (p_hdcp->req_reauth_ver == 2) {
		/* force hdcp2.x mode */
		if (get_hdcp2_lstore() && is_rx_hdcp2ver()) {
			hdev->hdcp_mode = 2;
			hdcp_mode_set(2);
		} else {
			hdev->hdcp_mode = 0;
		}
	}
}

void hdmitx21_rst_stream_type(struct hdcp_t *hdcp)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (!hdcp)
		return;
	hdcp->csm_message.streamid_type = hdev->def_stream_type;
}

/* note: it maybe used in timer */
u8 hdmitx_reauth_request(u8 hdcp_version)
{
	u8 upstream_type = 0;
	u8 cur_content_type = 0;
	bool ds_repeater = false;

	pr_info("%s[%d] hdcp_ver 0x%x\n", __func__, __LINE__, hdcp_version);

	/* bit4: smng type notify, bit3~0: hdcp ver, 0:auto, 1:hdcp14, 2:hdcp22 */
	if ((hdcp_version & 0x10) == 0x0) {
		/* first re-auth */
		hdcp_version &= 0xF;
		if (hdcp_version != 0 &&
			hdcp_version != 1 &&
			hdcp_version != 2)
			return 0;

		p_hdcp->req_reauth_ver = hdcp_version;
		/* recovery default streamid_type, by default stream type = 0 */
		hdmitx21_rst_stream_type(p_hdcp);
		schedule_delayed_work(&p_hdcp->req_reauth_wk, 0);
	} else {
		/* streamtype_dbg:
		 * 0: if ds is hdcp22 repeater, then propagate stream type with re-auth
		 * 1: force to not re-auth, for debug
		 * 2 or others: don't check ds, if stream type changed, force propagate
		 * stream type with re-auth, even ds is hdcp receiver or hdcp14 repeater
		 */
		if (streamtype_dbg == 1) {
			return 0;
		} else if (streamtype_dbg == 0) {
			ds_repeater = hdcptx_query_ds_repeater(p_hdcp);
			if (!ds_repeater || p_hdcp->hdcp_type != HDCP_VER_HDCP2X) {
				pr_hdcp_info("no need propagate stream type, ds_repeater: %d, ds_hdcp_type: %d\n",
					ds_repeater, p_hdcp->hdcp_type);
				return 0;
			}
		}
		/* if downstream connect repeater, then propagate stream id to downstream */
		upstream_type = hdcp_version & 0xF;
		cur_content_type = p_hdcp->csm_message.streamid_type & 0x00FF;
		/* if current stream type not consistent with upstream,
		 * re-auth with new stream type notified by upstream,
		 * hdcp re-auth version is req_reauth_ver
		 */
		if (upstream_type != cur_content_type) {
			p_hdcp->csm_message.streamid_type =
				(p_hdcp->csm_message.streamid_type & 0xFF00) |
				upstream_type;
			pr_hdcp_info("stream type change from %d to %d\n",
				cur_content_type, upstream_type);
			if (p_hdcp->cont_smng_method == 0) {
				schedule_delayed_work(&p_hdcp->req_reauth_wk, 0);
			} else if (p_hdcp->cont_smng_method == 1) {
				p_hdcp->csm_updated = true;
				hdcp_update_csm(p_hdcp);
				hdcptx2_rpt_smng_xfer_start();
			}
		}
	}
	return 1;
}
EXPORT_SYMBOL(hdmitx_reauth_request);

static void hdcp1x_process_intr(struct hdcp_t *p_hdcp, u8 int_reg[])
{
	u8 hdcp1xcoppst, hdcp1xauthintst;
	u16 prime_ri;

	hdcp1xauthintst = int_reg[0]; // 0x63d
	hdcp1xcoppst = hdcptx1_copp_status_get(); // 0x629
	prime_ri = hdcptx1_get_prime_ri(); // 0x222 0x223
	pr_hdcp_info("%s[%d] hdcp1xauthintst 0x%x hdcp1xcoppst 0x%x Ri 0x%x\n",
		__func__, __LINE__, hdcp1xauthintst, hdcp1xcoppst, prime_ri);
	if ((hdcp1xauthintst & BIT_TPI_INTR_ST0_BKSV_ERR) ||
		(hdcp1xauthintst & BIT_TPI_INTR_ST0_BKSV_BCAPS_ERR))
		hdcptx_update_failures(p_hdcp, HDCP_FAIL_BKSV_RXID);
	if (hdcp1xauthintst & BIT_TPI_INTR_ST0_BKSV_BCAPS_DONE) {
		p_hdcp->update_topology = false;
		p_hdcp->update_topo_state = false;
		hdcp1x_check_bksv_done(p_hdcp);
	}
	if (hdcp1xauthintst & BIT_TPI_INTR_ST0_REAUTH_RI_MISMATCH) {
		p_hdcp->ds_repeater = false;
		switch (hdcp1xcoppst & (0x80 | 0x40)) {
		case 0x00:
			break;
		case 0x40:
			/* first part done */
			if (!hdcptx_query_ds_repeater(p_hdcp)) {
				p_hdcp->ds_repeater = false;
				/* R0 == R0' */
				hdcp_authenticated_handle(p_hdcp);
				/* ds is receiver case */
				schedule_delayed_work(&p_hdcp->ksv_notify_wk, 0);
			} else {
				p_hdcp->ds_repeater = true;
				p_hdcp->ds_auth = true;
				p_hdcp->update_topology = true;
				hdcp_stop_work(&p_hdcp->timer_hdcp_rcv_auth);
				hdcp_schedule_work(&p_hdcp->timer_hdcp_rpt_auth,
					HDCP_DS_KSVLIST_RETRY_TIMER, 0);
			}
			break;
		case(0x80 | 0x40):
			/* second part done */
			hdcptx_encryption_update(p_hdcp, true);
			/* g_prot secure -- downstream is repeater */
			p_hdcp->ds_repeater = true;
			p_hdcp->hdcp14_second_part_pass = true;
			pr_hdcp_info("%s[%d] ds_repeater %d  rpt_ready %d\n",
				__func__, __LINE__,
				p_hdcp->ds_repeater, p_hdcp->rpt_ready);
			if (p_hdcp->rpt_ready) {
				p_hdcp->update_topo_state = false;
				update_hdcp_state(p_hdcp, HDCP_STAT_SUCCESS);
				/* ds is repeater case
				 * if set upstream READY for ksv list even V' invalid,
				 * it will fail hdcp1.4 repeater CTS 3C-II-05.
				 * only can notify upstream to set READY after
				 * downstream side auth pass with downstream repeater
				 */
				schedule_delayed_work(&p_hdcp->ksv_notify_wk, 0);
			} else {
				p_hdcp->update_topo_state = true;
			}
			hdcp_stop_work(&p_hdcp->timer_hdcp_rpt_auth);
			break;
		default:
			break;
		}
		switch (hdcptx1_ksv_v_get() & 0xc0) {
		case 0xc0:
			pr_hdcp_info("hdcptx1: the rx auth is pass and rx repeter auth is begin\n");
			break;
		case 0x80:
			pr_hdcp_info("hdcptx1: the rx all auth is pass\n");
			break;
		case 0x40:
			pr_hdcp_info("hdcptx1: the rx auth is error\n");
			break;
		case 0x00:
		default:
			pr_hdcp_info("hdcptx1: the rx auth start again or ddc_gpu_tpi_granted\n");
			break;
		}
	}
	if (hdcp1xauthintst & BIT_TPI_INTR_ST0_REESTABLISH) {
		switch (hdcp1xcoppst & 0x30) {
		case 0x00:
		case 0x30:
			pr_hdcp_info("hdcptx1: link normal or rsvd\n");
			break;
		case 0x10:
		case 0x20:
			pr_hdcp_info("hdcptx1: link lost or reneg\n");
			hdcptx_update_failures(p_hdcp, (hdcp1xcoppst & 0x08));
			break;
		default:
			break;
		}
	}

	if (hdcp1x_ds_ksv_fifo_ready(p_hdcp, int_reg)) {
		if (p_hdcp->update_topology) {
			hdcp_stop_work(&p_hdcp->timer_hdcp_rpt_auth);
			hdcp_rpt_ready_process(p_hdcp, true);
			p_hdcp->update_topology = false;
			p_hdcp->update_topo_state = true;
			pr_hdcp_info("%s[%d] update_topo_state %d\n", __func__,
				__LINE__, p_hdcp->update_topo_state);
			if (p_hdcp->update_topo_state) {
				p_hdcp->update_topo_state = false;
				update_hdcp_state(p_hdcp, HDCP_STAT_SUCCESS);
			}
			/* ds is repeater case, 1.only can notify upstream to set READY
			 * after second part succeed, otherwise 3C-II-05 will fail;
			 * 2.but for 1.4 repeater 3B-01b:Regular procedure With Repeater
			 * - DEVICE_COUNT=0, second part comes first before ksv fifo
			 * ready, here need to notify upstream side to update topo info
			 * 3.hdcp1.4 repeatr 3C-II-06~09, still need to notify topo
			 * info to upstream side even if topo info exceed the maximum
			 */
			if (p_hdcp->hdcp14_second_part_pass ||
				p_hdcp->hdcp_topology.max_cas_exceed ||
				p_hdcp->hdcp_topology.max_devs_exceed)
				schedule_delayed_work(&p_hdcp->ksv_notify_wk, 0);
		}
	}
}

void hdcp1x_intr_handler(struct intr_t *intr)
{
	u8 hdcp14_intreg[1];

	pr_hdcp_info("%s[%d]\n", __func__, __LINE__);
	hdcp14_intreg[0] = intr->st_data;
	/* clear intr state asap instead of clear after process intr */
	intr->st_data = 0;
	hdcp1x_process_intr(p_hdcp, hdcp14_intreg);
}

const char *msg1_info[] = {
	"ro_rpt_rcvid_changed",
	"ro_rpt_smng_changed",
	"ro_ake_sent_rcvd",
	"ro_ske_sent_rcvd",
	"ro_rpt_rcvid_xfer_done",
	"ro_rpt_smng_xfer_done",
	"ro_cert_sent_rcvd",
	"ro_gp3",
};

const char *msg2_info[] = {
	"ro_km_sent_rcvd",
	"ro_ekhkm_sent_rcvd",
	"ro_h_sent_rcvd",
	"ro_pair_sent_rcvd",
	"ro_lc_sent_rcvd",
	"ro_l_sent_rcvd",
	"ro_vack_sent_rcvd",
	"ro_mack_sent_rcvd",
};

static void hdcp2x_process_intr(u8 int_reg[])
{
	int i;
	u8 cp2tx_intr0_st = int_reg[0]; // 0x803 auth status
	u8 cp2tx_intr1_st = int_reg[1];  // 0x804 msg status
	u8 cp2tx_intr2_st = int_reg[2];  // 0x805
	u8 cp2tx_intr3_st = int_reg[3];  // 0x806
	u8 cp2tx_state = hdcp2x_get_state_st(); // 0x80d

	pr_hdcp_info("%s[%d] 0x%x 0x%x 0x%x 0x%x 0x%x\n", __func__, __LINE__,
		int_reg[0], int_reg[1], int_reg[2], int_reg[3], cp2tx_state);

	if (cp2tx_intr0_st & BIT_CP2TX_INTR0_AUTH_DONE) {
		/* intr0 bit7 and bit0 needs to be 1, then start smng xfer */
		/* otherwise the 1B-09 will fail */
		/* if csm already update by upstream, don't transfer once more */
		if ((cp2tx_intr0_st & BIT_CP2TX_INTR0_POLL_INTERVAL) &&
			p_hdcp->ds_repeater &&
			!p_hdcp->csm_updated)
			hdcptx2_rpt_smng_xfer_start();
		if (hdcptx2_auth_status()) {
			if (!hdcptx_query_ds_repeater(p_hdcp)) {
				hdcp_authenticated_handle(p_hdcp);
				/* ds is receiver case */
				schedule_delayed_work(&p_hdcp->ksv_notify_wk,
					msecs_to_jiffies(notify_delay_ms));
			} else {
				hdcp_stop_work(&p_hdcp->timer_hdcp_rcv_auth);
				hdcp_stop_work(&p_hdcp->timer_hdcp_rpt_auth);
				pr_hdcp_info("hdcptx2: auth done\n");
			}
		}
	}
	if (cp2tx_intr0_st & BIT_CP2TX_INTR0_AUTH_FAIL) {
		hdcp2x_auth_stop(p_hdcp);
		hdcptx_update_failures(p_hdcp, p_hdcp->ds_repeater);
	}
	if (cp2tx_intr0_st & BIT_CP2TX_INTR0_RPT_READY_CHANGE) {
		p_hdcp->ds_repeater = true;
		pr_hdcp_info("hdcptx2: repeater ready\n");
	}
	if (cp2tx_intr0_st & BIT_CP2TX_INTR0_REAUTH_REQ) {
		pr_hdcp_info("hdcptx2: reauth req from ds device\n");
		hdcp2x_reauth_start(p_hdcp);
	}

	for (i = 0; i < 8; i++) {
		if (cp2tx_intr1_st & (1 << i))
			pr_hdcp_info("%s", msg1_info[i]);
	}
	for (i = 0; i < 8; i++) {
		if (cp2tx_intr2_st & (1 << i))
			pr_hdcp_info("%s", msg2_info[i]);
	}

	if (cp2tx_intr1_st & BIT_CP2TX_INTR1_RPT_RCVID_CHANGED) {
		pr_hdcp_info("hdcptx2: rcv_id changed");
		hdcp_stop_work(&p_hdcp->timer_hdcp_rpt_auth);
		hdcp_rpt_ready_process(p_hdcp, true);
		/* ds is repeater case */
		schedule_delayed_work(&p_hdcp->ksv_notify_wk, msecs_to_jiffies(notify_delay_ms));
		/* if no hdcp1.x or hdcp2.0 legacy devices downstream, then topo 1 */
		if (p_hdcp->hdcp_topology.ds_hdcp1x_dev == 0 &&
		    p_hdcp->hdcp_topology.ds_hdcp2x_dev == 0)
			set_hdcp2_topo(1);
		else
			set_hdcp2_topo(0);
	}
	if (cp2tx_intr1_st & BIT(2)) //BIT__HDCP2X_INTR0__AKE_SEND
		p_hdcp->reauth_ignored = true;
	if (cp2tx_intr1_st & BIT(5)) //BIT__HDCP2X_INTR0__RPTR_SMNG_TRANS_DONE
		update_hdcp_state(p_hdcp, HDCP_STAT_SUCCESS);
	if (cp2tx_intr1_st & BIT(6)) //BIT__HDCP2X_INTR0__CERT_RECV
		p_hdcp->reauth_ignored = false;

	if (cp2tx_intr1_st & BIT(3)) { //BIT__HDCP2X_INTR0__SKE_SEND
		p_hdcp->reauth_ignored = false;
		if (hdcptx_query_ds_repeater(p_hdcp)) {
			/*
			 * down stream is a hdcp2.2 repeater
			 */
			p_hdcp->ds_repeater = true;
			p_hdcp->ds_auth = true;
			hdcptx_encryption_update(p_hdcp, true);
			hdcp_stop_work(&p_hdcp->timer_hdcp_rcv_auth);
			hdcp_schedule_work(&p_hdcp->timer_hdcp_rpt_auth,
				HDCP_RCVIDLIST_CHECK_TIMER, 0);
			hdcp_check_ds_csm_status(p_hdcp);
		} else {
			p_hdcp->ds_repeater = false;
		}
	}
	if (cp2tx_intr3_st & BIT(4))
		;
}

void hdcp2x_intr_handler(struct intr_t *intr)
{
	u8 hdcp2_intreg[4];

	intr_status_save_clr_cp2txs(hdcp2_intreg);
	hdcp2x_process_intr(hdcp2_intreg);
}

static void hdcp1x_auth_start(struct hdcp_t *p_hdcp)
{
	pr_hdcp_info("%s[%d]\n", __func__, __LINE__);
	p_hdcp->hdcp_type = HDCP_VER_HDCP1X;
	update_hdcp_state(p_hdcp, HDCP_STAT_AUTH);
	hdcptx1_encryption_update(false);
	hdcptx1_auth_start();
	hdcp_schedule_work(&p_hdcp->timer_bksv_poll_done, HDCP_BSKV_CHECK_TIMER, 0);
}

static void hdcp2x_auth_start(struct hdcp_t *p_hdcp)
{
	u8 content_type;

	p_hdcp->hdcp_type = HDCP_VER_HDCP2X;
	if (p_hdcp->content_type == HDCP_CONTENT_TYPE_0)
		content_type = 0;
	else if (p_hdcp->content_type == HDCP_CONTENT_TYPE_1)
		content_type = 1;
	else
		content_type = 0xFF;
	pr_hdcp_info("%s[%d] content_type %d %d\n", __func__, __LINE__,
		p_hdcp->content_type, content_type);

	hdcptx2_src_auth_start(content_type);

	hdcp2x_reauth_start(p_hdcp);
	hdcp_schedule_work(&p_hdcp->timer_ddc_check_nak, 100, 200);
	update_hdcp_state(p_hdcp, HDCP_STAT_AUTH);
}

static enum hdcp_ver_t hdcp_check_ds_hdcp2ver(struct hdcp_t *p_hdcp)
{
	enum hdcp_ver_t hdcp_type = HDCP_VER_NONE;
	enum ddc_err_t ddc_err = DDC_ERR_NONE;
	u8 cap_val = 0;

	ddc_err = hdmitx_ddcm_read(0, DDC_HDCP_DEVICE_ADDR, REG_DDC_HDCP_VERSION, &cap_val, 1);

	if (ddc_err == DDC_ERR_NONE) {
		if (cap_val == 0x04)
			hdcp_type = HDCP_VER_HDCP2X;
		else
			hdcp_type = HDCP_VER_HDCP1X;
	} else {
		hdcptx_update_failures(p_hdcp, HDCP_FAIL_DDC_NACK);
	}
	return hdcp_type;
}

static void hdcp1x_auth_stop(struct hdcp_t *p_hdcp)
{
	hdcptx1_auth_stop();
	hdcptx1_protection_enable(false);
	ddc_toggle_sw_tpi();
}

static bool ddc_bus_wait_free(void)
{
	u8 tmo1 = 10;
	u8 tmo2 = 2;

	while (tmo2--) {
		while (tmo1--) {
			if (!hdmi_ddc_busy_check())
				return true;
			usleep_range(1000, 2000);
		}
		hdmi_ddc_error_reset();
		usleep_range(1000, 2000);
	}
	return false;
}

static bool ddc_check_busy(struct hdcp_t *p_hdcp)
{
	enum ddc_err_t error = DDC_ERR_NONE;
	u8 time_out_ms = 7;

	usleep_range(2000, 3000);
	do {
		if (!ddc_bus_wait_free()) {
			error = DDC_ERR_BUSY;
			time_out_ms--;
			if (!time_out_ms)
				return false;
			usleep_range(2000, 3000);
		} else {
			error = DDC_ERR_NONE;
		}
	} while (error != DDC_ERR_NONE);
	return true;
}

static void hdcp2x_auth_stop(struct hdcp_t *p_hdcp)
{
	hdcp_stop_work(&p_hdcp->timer_ddc_check_nak);
	if (ddc_check_busy(p_hdcp))
		hdcptx2_auth_stop();
}

static void hdcp_reset_hw(struct hdcp_t *p_hdcp)
{
	hdcptx_init_reg();
	hdcp1x_auth_stop(p_hdcp);
	hdcp2x_auth_stop(p_hdcp);
}

static void hdcptx_send_csm_msg(struct hdcp_t *p_hdcp)
{
	if (p_hdcp->ds_repeater && p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
		p_hdcp->csm_msg_sent = true;
		hdcptx2_csm_send(&p_hdcp->csm_message);
		p_hdcp->csm_message.seq_num_m++;
		if (p_hdcp->csm_message.seq_num_m > 0xFFFFFFUL)
			p_hdcp->csm_message.seq_num_m = 0;
		memcpy(&p_hdcp->prev_csm_message, &p_hdcp->csm_message, sizeof(struct hdcp_csm_t));
	}
}

static void hdcp2x_validate_csm_msg(struct hdcp_t *p_hdcp)
{
	u8 content_type = (u8)(p_hdcp->csm_message.streamid_type & 0x00FF);

	if (content_type == 0x00)
		p_hdcp->content_type = HDCP_CONTENT_TYPE_0;
	else if (content_type == 0x01)
		p_hdcp->content_type = HDCP_CONTENT_TYPE_1;
	else
		p_hdcp->content_type = HDCP_CONTENT_TYPE_NONE;

	if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
		if (!p_hdcp->rpt_ready)
			return;
		if (p_hdcp->ds_repeater)
			hdcptx_send_csm_msg(p_hdcp);
	}
}

static void hdcp_update_csm(struct hdcp_t *p_hdcp)
{
	hdcp2x_validate_csm_msg(p_hdcp);
}

static void hdcptx_reset(struct hdcp_t *p_hdcp)
{
	p_hdcp->ds_repeater = false; /*!< attached is a repeater or not */
	p_hdcp->update_topology = false;
	p_hdcp->update_topo_state = false;
	p_hdcp->rpt_ready = false;
	p_hdcp->reauth_ignored = false;
	p_hdcp->csm_message.seq_num_m = 0;
	p_hdcp->hdcp_type = HDCP_VER_NONE;
	p_hdcp->hdcp_state = HDCP_STAT_NONE;
	p_hdcp->csm_valid = true;
	p_hdcp->csm_msg_sent = false;
	p_hdcp->csm_updated = false;
	p_hdcp->hdcp14_second_part_pass = false;

	hdcp_stop_work(&p_hdcp->timer_hdcp_start);
	hdcp_stop_work(&p_hdcp->timer_hdcp_rcv_auth);
	hdcp_stop_work(&p_hdcp->timer_hdcp_rpt_auth);
	hdcp_stop_work(&p_hdcp->timer_hdcp_auth_fail_retry);
	hdcp_stop_work(&p_hdcp->timer_bksv_poll_done);
	hdcp_stop_work(&p_hdcp->timer_ddc_check_nak);
	hdcp_stop_work(&p_hdcp->timer_update_csm);

	ksv_reset_fifo(p_hdcp);
	p_hdcp->ds_auth = false;
	hdcp_reset_hw(p_hdcp);

	hdcptx2_encryption_update(false);
}

static bool hdcp_schedule_work(struct hdcp_work *work, u32 delay_ms, u32 period_ms)
{
	pr_hdcp_info("hdcptx: schedule %s: delay %d ms  period %d ms\n",
		work->name, delay_ms, period_ms);

	delay_ms = (delay_ms + 3) / 4;
	period_ms = (period_ms + 3) / 4;

	work->delay_ms = 0;
	work->period_ms = period_ms;

	if (delay_ms == 0 && period_ms == 0) {
		cancel_delayed_work(&work->dwork);
		return 1;
	}

	if (delay_ms)
		return queue_delayed_work(p_hdcp->hdcp_wq, &work->dwork, delay_ms);
	else
		return queue_delayed_work(p_hdcp->hdcp_wq, &work->dwork, period_ms);
}

static bool hdcp_stop_work(struct hdcp_work *work)
{
	cancel_delayed_work(&work->dwork);
	pr_hdcp_info("hdcptx: stop %s\n", work->name);
	return 0;
}

static void hdcptx_auth_start(struct hdcp_t *p_hdcp)
{
	enum hdcp_ver_t hdcp_mode;

	hdcp_mode = p_hdcp->req_hdcp_ver;

	if (hdcp_mode != HDCP_VER_NONE) {
		hdcp_mode = p_hdcp->req_hdcp_ver;
		if (p_hdcp->hdcptx_enabled) {
			ksv_reset_fifo(p_hdcp);
			hdcp_enable_intrs(1);
			hdcp_schedule_work(&p_hdcp->timer_hdcp_rcv_auth,
				HDCP_STAGE1_RETRY_TIMER, 0);
			if (hdcp_mode == HDCP_VER_HDCP1X)
				hdcp1x_auth_start(p_hdcp);
			if (hdcp_mode == HDCP_VER_HDCP2X)
				hdcp2x_auth_start(p_hdcp);
			hdcp_schedule_work(&p_hdcp->timer_ddc_check_nak, 100, 200);
		}
	}
}

const static char *fail_string[] = {
	"none",
	"ddc_nack",
	"bksv_rxid",
	"auth_fail",
	"ready_to",
	"v",
	"topology",
	"ri",
	"reauth_req",
	"content_type",
	"auth_time_out",
	"hash",
	"unknown",
};

static void hdcptx_update_failures(struct hdcp_t *p_hdcp, enum hdcp_fail_types_t types)
{
	pr_hdcp_info("%s[%d] types: %s\n", __func__, __LINE__,
		fail_string[types]);
	hdcp_stop_work(&p_hdcp->timer_hdcp_rcv_auth);
	hdcp_stop_work(&p_hdcp->timer_hdcp_rpt_auth);
	if (p_hdcp->hdcptx_enabled) {
		if (types == HDCP_FAIL_CONTENT_TYPE) {
			p_hdcp->fail_type = types;
			update_hdcp_state(p_hdcp, HDCP_STAT_FAILED);
		} else {
			p_hdcp->reauth_ignored = false;
			p_hdcp->fail_type = types;
			update_hdcp_state(p_hdcp, HDCP_STAT_FAILED);
			ksv_reset_fifo(p_hdcp);
			hdcp_schedule_work(&p_hdcp->timer_hdcp_auth_fail_retry,
				HDCP_FAILED_RETRY_TIMER, 0);
		}
	}
}

static void hdcp_check_ds_auth_whandler(struct work_struct *w)
{
	struct hdcp_work *work = &p_hdcp->timer_hdcp_rcv_auth;

	pr_hdcp_info("%s[%d] period %d ms\n", __func__, __LINE__,
		work->period_ms);
	hdcptx_update_failures(p_hdcp, HDCP_FAIL_AUTH_TIME_OUT);
	if (work->period_ms)
		queue_delayed_work(p_hdcp->hdcp_wq, &work->dwork, work->period_ms);
}

static void hdcp_check_bksv_done_whandler(struct work_struct *w)
{
	u8 copp_data1;
	struct hdcp_work *work = &p_hdcp->timer_bksv_poll_done;

	pr_hdcp_info("%s[%d] period %d ms\n", __func__, __LINE__,
		work->period_ms);
	copp_data1 = hdcptx1_ds_cap_status_get();

	if (copp_data1 & 0x02) {
		hdcp_stop_work(work);
		if (is_bksv_valid(p_hdcp))
			hdcptx1_protection_enable(true);
		else
			hdcptx_update_failures(p_hdcp, HDCP_FAIL_BKSV_RXID);
	}
	if (work->period_ms)
		queue_delayed_work(p_hdcp->hdcp_wq, &work->dwork, work->period_ms);
}

static void hdcp_check_update_whandler(struct work_struct *w)
{
	struct hdcp_work *work = &p_hdcp->timer_hdcp_start;

	pr_hdcp_info("%s[%d] period %d ms\n", __func__, __LINE__,
		work->period_ms);
	hdcptx_reset(p_hdcp);
	if (hdcp_reauth_dbg == 2) {
		mdelay(delay_ms);
	} else if (hdcp_reauth_dbg == 3) {
		mdelay(delay_ms);
		hdcptx_reset(p_hdcp);
	}
	if (p_hdcp->hdcptx_enabled) {
		p_hdcp->hdcp_cap_ds = hdcp_check_ds_hdcp2ver(p_hdcp);
		if (p_hdcp->hdcp_cap_ds != HDCP_VER_NONE) {
			update_hdcp_state(p_hdcp, HDCP_STAT_AUTH);
			hdcptx_auth_start(p_hdcp);
		}
	} else {
		update_hdcp_state(p_hdcp, HDCP_STAT_NONE);
		p_hdcp->hdcp_cap_ds = HDCP_VER_NONE;
		p_hdcp->fail_type = HDCP_FAIL_NONE;
	}
	if (work->period_ms)
		queue_delayed_work(p_hdcp->hdcp_wq, &work->dwork, work->period_ms);
}

static void hdcp_ddc_check_nack_whandler(struct work_struct *w)
{
	static int cnt;
	struct hdcp_work *work = &p_hdcp->timer_ddc_check_nak;

	if (cnt % 128 == 0) {
		cnt++;
		pr_hdcp_info("%s[%d] period %d ms\n", __func__, __LINE__,
			work->period_ms);
	}
	if (hdmi_ddc_status_check()) {
		p_hdcp->hdcp_cap_ds = HDCP_VER_NONE;
		hdcp2x_auth_stop(p_hdcp);
		hdcptx_update_failures(p_hdcp, HDCP_FAIL_DDC_NACK);
	}
	if (work->period_ms)
		queue_delayed_work(p_hdcp->hdcp_wq, &work->dwork, work->period_ms);
}

static void hdcp_check_auth_failretry_whandler(struct work_struct *w)
{
	struct hdcp_work *work = &p_hdcp->timer_hdcp_auth_fail_retry;

	pr_hdcp_info("%s[%d] period %d ms\n", __func__, __LINE__,
		work->period_ms);
	hdcptx_reset(p_hdcp);
	hdcptx_auth_start(p_hdcp);
	if (work->period_ms)
		queue_delayed_work(p_hdcp->hdcp_wq, &work->dwork, work->period_ms);
}

static void hdcp_update_csm_whandler(struct work_struct *w)
{
	struct hdcp_work *work = &p_hdcp->timer_update_csm;

	pr_hdcp_info("%s[%d] period %d ms\n", __func__, __LINE__,
		work->period_ms);
	hdcp_update_csm(p_hdcp);

	if (work->period_ms)
		queue_delayed_work(p_hdcp->hdcp_wq, &work->dwork, work->period_ms);
}

static void init_hdcp_works(struct hdcp_work *work,
	void (*workfunc)(struct work_struct *), const char *name)
{
	INIT_DELAYED_WORK(&work->dwork, workfunc);
	work->name = name;
}

static struct hdcp_t hdcp_hdcp;

static int hdmitx21_get_hdcp_auth_rlt(struct hdmitx_dev *hdev)
{
	if (hdev->hdcp_mode == 1)
		return (int)get_hdcp1_result();
	if (hdev->hdcp_mode == 2)
		return (int)get_hdcp2_result();
	else
		return 0;
}

static int  hdmitx21_hdcp_stat_monitor(void *data)
{
	static int auth_stat;
	struct hdmitx_dev *hdev = (struct hdmitx_dev *)data;

	while (hdev->hpd_event != 0xff) {
		hdmi21_authenticated = hdmitx21_get_hdcp_auth_rlt(hdev);
		if (auth_stat != hdmi21_authenticated) {
			hdmitx21_hdcp_status(hdmi21_authenticated);
			auth_stat = hdmi21_authenticated;
			pr_info("hdcptx: %d  auth: %d\n", hdev->hdcp_mode,
				auth_stat);
			if (hdev->drm_hdcp_cb.hdcp_notify)
				hdev->drm_hdcp_cb.hdcp_notify(hdev->drm_hdcp_cb.data,
					hdev->hdcp_mode, auth_stat);
		}
		msleep_interruptible(100);
	}
	return 0;
}

int hdmitx21_hdcp_init(void)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pr_hdcp_info("%s\n", __func__);
	hdev->am_hdcp = &hdcp_hdcp;
	p_hdcp = &hdcp_hdcp;
	p_hdcp->hdcp_state = HDCP_STAT_NONE;
	p_hdcp->hdcp_type = HDCP_VER_NONE;
	p_hdcp->hdcp_cap_ds = HDCP_VER_NONE;
	p_hdcp->ds_auth = false;
	p_hdcp->ds_repeater = false;
	p_hdcp->fail_type = HDCP_FAIL_NONE;
	p_hdcp->req_hdcp_ver = HDCP_VER_NONE;
	p_hdcp->reauth_ignored = false;
	p_hdcp->encryption_enabled = false;
	p_hdcp->content_type = HDCP_CONTENT_TYPE_0;
	hdmitx21_rst_stream_type(p_hdcp);
	p_hdcp->p_ksv_lists =
		kmalloc((HDCP1X_MAX_TX_DEV_SRC + 1) * sizeof(struct hdcp_ksv_t), GFP_KERNEL);
	p_hdcp->hdcp_wq = alloc_workqueue(DEVICE_NAME, WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
	init_hdcp_works(&p_hdcp->timer_hdcp_rcv_auth, hdcp_check_ds_auth_whandler, "hdcp_rcv_auth");
	init_hdcp_works(&p_hdcp->timer_hdcp_rpt_auth, hdcp_check_ds_auth_whandler, "hdcp_rpt_auth");
	init_hdcp_works(&p_hdcp->timer_bksv_poll_done,
		hdcp_check_bksv_done_whandler, "bksv_poll_done");
	init_hdcp_works(&p_hdcp->timer_hdcp_start, hdcp_check_update_whandler, "hdcp_start");
	init_hdcp_works(&p_hdcp->timer_ddc_check_nak,
		hdcp_ddc_check_nack_whandler, "ddc_check_nak");
	init_hdcp_works(&p_hdcp->timer_hdcp_auth_fail_retry,
		hdcp_check_auth_failretry_whandler, "hdcp_auth_fail_retry");
	init_hdcp_works(&p_hdcp->timer_update_csm, hdcp_update_csm_whandler, "update_csm");
	INIT_DELAYED_WORK(&p_hdcp->ksv_notify_wk, hdcp_notify_rpt_info);
	INIT_DELAYED_WORK(&p_hdcp->req_reauth_wk, hdcp_req_reauth_whandler);
	hdev->task_hdcp = kthread_run(hdmitx21_hdcp_stat_monitor, (void *)hdev,
				      "kthread_hdcp");

	return 0;
}

void __exit hdmitx21_hdcp_exit(void)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	hdcptx_reset(p_hdcp);
	kthread_stop(hdev->task_hdcp);
}
