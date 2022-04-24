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
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/devinfo.h>
#include <linux/pinctrl/consumer.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/atomic.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/reboot.h>
#include <linux/notifier.h>
#include <linux/random.h>
#include <linux/pm_wakeup.h>
#include <linux/pm_wakeirq.h>
#include <linux/pm.h>
#include <linux/poll.h>
#include <linux/amlogic/pm.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/scpi_protocol.h>
#include <linux/amlogic/pm.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static struct early_suspend aocec_suspend_handler;
#endif
#include "hdmi_tx_cec_20.h"
#include "hdmi_ao_cec.h"
#include "hdmi_aocec_api.h"
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_HDMI
#include "../vin/tvin/hdmirx/hdmi_rx_drv_ext.h"
#endif

DECLARE_WAIT_QUEUE_HEAD(cec_msg_wait_queue);

int cec_msg_dbg_en;
struct cec_msg_last *last_cec_msg;
struct ao_cec_dev *cec_dev;
bool ceca_err_flag;
bool ee_cec;
struct hrtimer start_bit_check;
unsigned char msg_log_buf[128] = { 0 };

static struct dbgflg stdbgflg;
static int phy_addr_test;
static struct tasklet_struct ceca_tasklet;
static enum cec_tx_ret cec_tx_result;

static unsigned char rx_msg[MAX_MSG];
static unsigned char rx_len;
static unsigned int  new_msg;
static bool pin_status;
static void cec_store_msg_to_buff(unsigned char len, unsigned char *msg);
static void cec_new_msg_push(void);
static void cec_stored_msg_push(void);

#if (defined(CONFIG_AMLOGIC_HDMITX) || defined(CONFIG_AMLOGIC_HDMITX21))
static int hdmitx_notify_callback(struct notifier_block *block,
				  unsigned long cmd, void *para)
{
	int ret = 0;

	switch (cmd) {
	case HDMITX_PLUG:
	case HDMITX_UNPLUG:
		CEC_INFO("[%s] event: %ld\n", __func__, cmd);
		queue_delayed_work(cec_dev->hdmi_plug_wq,
				   &cec_dev->work_hdmi_plug, 0);
		break;
	default:
		CEC_ERR("[%s] unsupported notify:%ld\n", __func__, cmd);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static struct notifier_block hdmitx_notifier_nb = {
	.notifier_call	= hdmitx_notify_callback,
};
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_HDMI
static int hdmirx_notify_callback(unsigned int pwr5v_sts)
{
	queue_delayed_work(cec_dev->hdmi_plug_wq, &cec_dev->work_hdmi_plug, 0);

	return 0;
}
#endif

/* --------FOR EE CEC(AOCECB)-------- */
static int cecb_pick_msg(unsigned char *msg, unsigned char *out_len)
{
	int i, size;
	int len;

	len = hdmirx_cec_read(DWC_CEC_RX_CNT);
	size = sprintf(msg_log_buf, "cec b rx len %d:", len);
	for (i = 0; i < len; i++) {
		msg[i] = hdmirx_cec_read(DWC_CEC_RX_DATA0 + i * 4);
		size += sprintf(msg_log_buf + size, " %02x", msg[i]);
	}
	size += sprintf(msg_log_buf + size, "\n");
	msg_log_buf[size] = '\0';
	/* clr CEC lock bit */
	hdmirx_cec_write(DWC_CEC_LOCK, 0);
	CEC_INFO("%s", msg_log_buf);

	#ifdef CEC_FREEZE_WAKE_UP
	if (is_pm_s2idle_mode())
		*out_len = len;
	else
	#endif
	{
		if (cec_message_op(msg, len))
			*out_len = len;
		else
			*out_len = 0;
	}
	pin_status = 1;
	return 0;
}

void cecb_irq_handle(void)
{
	u32 intr_cec;
	u32 lock;
	int shift = 0;
	struct delayed_work *dwork;

	intr_cec = cecb_irq_stat();

	/* clear irq */
	if (intr_cec != 0)
		cecb_clear_irq(intr_cec);
	else
		dprintk(L_2, "err cec intsts:0\n");

	dprintk(L_2, "cecb intsts:0x%x\n", intr_cec);
	if (cec_dev->plat_data->ee_to_ao)
		shift = 16;
	/* TX DONE irq, increase tx buffer pointer */
	if (intr_cec == CEC_IRQ_TX_DONE) {
		cec_tx_result = CEC_FAIL_NONE;
		dprintk(L_2, "irqflg:TX_DONE\n");
		complete(&cec_dev->tx_ok);
	}
	lock = hdmirx_cec_read(DWC_CEC_LOCK);
	/* EOM irq, message is coming */
	if ((intr_cec & CEC_IRQ_RX_EOM) || lock) {
		cecb_pick_msg(rx_msg, &rx_len);
		dprintk(L_2, "irqflg:RX_EOM\n");
		cec_store_msg_to_buff(rx_len, rx_msg);
		cec_new_msg_push();
		dwork = &cec_dev->cec_work;
		mod_delayed_work(cec_dev->cec_thread, dwork, 0);
	}

	/* TX error irq flags */
	if ((intr_cec & CEC_IRQ_TX_NACK)     ||
	    (intr_cec & CEC_IRQ_TX_ARB_LOST) ||
	    (intr_cec & CEC_IRQ_TX_ERR_INITIATOR)) {
		if (intr_cec & CEC_IRQ_TX_NACK) {
			cec_tx_result = CEC_FAIL_NACK;
			dprintk(L_2, "irqflg:TX_NACK\n");
		} else if (intr_cec & CEC_IRQ_TX_ARB_LOST) {
			cec_tx_result = CEC_FAIL_BUSY;
			/* clear start */
			hdmirx_cec_write(DWC_CEC_TX_CNT, 0);
			hdmirx_set_bits_dwc(DWC_CEC_CTRL, 0, 0, 3);
			dprintk(L_2, "irqflg:ARB_LOST\n");
		} else if (intr_cec & CEC_IRQ_TX_ERR_INITIATOR) {
			dprintk(L_2, "irqflg:INITIATOR\n");
			cec_tx_result = CEC_FAIL_OTHER;
		} else {
			dprintk(L_2, "irqflg:Other\n");
			cec_tx_result = CEC_FAIL_OTHER;
		}
		complete(&cec_dev->tx_ok);
	}

	/* RX error irq flag */
	if (intr_cec & CEC_IRQ_RX_ERR_FOLLOWER) {
		dprintk(L_2, "warning:FOLLOWER\n");
		hdmirx_cec_write(DWC_CEC_LOCK, 0);
		/* TODO: need reset cec hw logic? */
	}

	/* wakeup op code will triger this int*/
	if (intr_cec & CEC_IRQ_RX_WAKEUP) {
		dprintk(L_2, "warning:RX_WAKEUP\n");
		hdmirx_cec_write(DWC_CEC_WKUPCTRL, WAKEUP_EN_MASK);
		/* TODO: wake up system if needed */
	}
}

static irqreturn_t cecb_isr(int irq, void *dev_instance)
{
	cecb_irq_handle();
	return IRQ_HANDLED;
}

/* --------end of CECB-------- */

/* --------for AO CEC (CECA)-------- */
static int ceca_rx_irq_handle(unsigned char *msg, unsigned char *len)
{
	int i;
	int ret = -1;
	int pos;
	int rx_stat;

	rx_stat = aocec_rd_reg(CEC_RX_MSG_STATUS);
	if (rx_stat != RX_DONE || (aocec_rd_reg(CEC_RX_NUM_MSG) != 1)) {
		CEC_INFO("rx status:%x\n", rx_stat);
		write_ao(AO_CEC_INTR_CLR, (1 << 2));
		aocec_wr_reg(CEC_RX_MSG_CMD,  RX_ACK_CURRENT);
		aocec_wr_reg(CEC_RX_MSG_CMD,  RX_NO_OP);
		ceca_rx_buf_clear();
		return ret;
	}

	*len = aocec_rd_reg(CEC_RX_MSG_LENGTH) + 1;

	for (i = 0; i < (*len) && i < MAX_MSG; i++)
		msg[i] = aocec_rd_reg(CEC_RX_MSG_0_HEADER + i);

	ret = rx_stat;

	/* ignore ping message */
	if (cec_msg_dbg_en && *len > 1) {
		pos = 0;
		pos += sprintf(msg_log_buf + pos,
			"cec a rx len %d:", *len);
		for (i = 0; i < (*len); i++)
			pos += sprintf(msg_log_buf + pos, "%02x ", msg[i]);
		pos += sprintf(msg_log_buf + pos, "\n");
		msg_log_buf[pos] = '\0';
		CEC_INFO("%s", msg_log_buf);
	}
	last_cec_msg->len = 0;	/* invalid back up msg when rx */
	write_ao(AO_CEC_INTR_CLR, (1 << 2));
	aocec_wr_reg(CEC_RX_MSG_CMD, RX_ACK_CURRENT);
	aocec_wr_reg(CEC_RX_MSG_CMD, RX_NO_OP);
	ceca_rx_buf_clear();
	pin_status = 1;

	/* when use two cec ip, cec a only send msg, discard all rx msg */
	if (cec_dev->cec_num > ENABLE_ONE_CEC) {
		/*CEC_INFO("discard msg\n");*/
		return -1;
	}

	return ret;
}

static void ceca_tx_irq_handle(void)
{
	unsigned int tx_status = aocec_rd_reg(CEC_TX_MSG_STATUS);

	cec_tx_result = -1;
	switch (tx_status) {
	case TX_DONE:
		aocec_wr_reg(CEC_TX_MSG_CMD, TX_NO_OP);
		cec_tx_result = CEC_FAIL_NONE;
		break;

	case TX_BUSY:
		CEC_ERR("TX_BUSY\n");
		cec_tx_result = CEC_FAIL_BUSY;
		break;

	case TX_ERROR:
		CEC_INFO("TX ERROR!\n");
		aocec_wr_reg(CEC_TX_MSG_CMD, TX_ABORT);
		ceca_hw_reset();
		if (cec_dev->cec_num <= ENABLE_ONE_CEC)
			cec_restore_logical_addr(CEC_A,
						 cec_dev->cec_info.addr_enable);
		cec_tx_result = CEC_FAIL_NACK;
		break;

	case TX_IDLE:
		CEC_ERR("TX_IDLE\n");
		cec_tx_result = CEC_FAIL_OTHER;
		break;
	default:
		break;
	}
	write_ao(AO_CEC_INTR_CLR, (1 << 1));
	complete(&cec_dev->tx_ok);
}

static void ceca_tasklet_pro(unsigned long arg)
{
	unsigned int intr_stat = 0;
	struct delayed_work *dwork;

	dwork = &cec_dev->cec_work;
	intr_stat = ao_ceca_intr_stat();
	/*dprintk(L_2, "ceca_tsklet 0x%x\n", intr_stat);*/
	if (intr_stat & AO_CEC_TX_INT) {
		/* cec a tx intr */
		ceca_tx_irq_handle();
		return;
	} else if (intr_stat & AO_CEC_RX_INT) {
		/* cec a rx intr */
		if ((-1) == ceca_rx_irq_handle(rx_msg, &rx_len))
			return;

		cec_store_msg_to_buff(rx_len, rx_msg);
		cec_new_msg_push();
		mod_delayed_work(cec_dev->cec_thread, dwork, 0);
	}
}

static irqreturn_t ceca_isr(int irq, void *dev_instance)
{
	tasklet_schedule(&ceca_tasklet);
	return IRQ_HANDLED;
}

/* --------end of AOCEC (CECA)-------- */
static bool check_physical_addr_valid(int timeout)
{
	while (timeout > 0) {
		if (cec_dev->dev_type == CEC_TV_ADDR)
			break;
		if (phy_addr_test)
			break;
		/* physical address for box */
		if (get_hpd_state() &&
		    get_hdmitx_phy_addr() &&
		    get_hdmitx_phy_addr()->valid == 0) {
			msleep(40);
			timeout--;
		} else {
			break;
		}
	}
	if (timeout <= 0)
		return false;
	return true;
}

/* Return value: < 0: fail, > 0: success */
int cec_ll_tx(const unsigned char *msg, unsigned char len, unsigned char signal_free_time)
{
	int ret = -1;
	int t;
	int retry = 2;
	unsigned int cec_sel;

	mutex_lock(&cec_dev->cec_tx_mutex);
	/* only use cec a send msg */
	if (cec_dev->cec_num > ENABLE_ONE_CEC)
		cec_sel = CEC_A;
	else
		cec_sel = ee_cec;

	t = msecs_to_jiffies((cec_sel == CEC_B) ? 2000 : 5000);

	if (len == 0) {
		CEC_INFO("err len 0\n");
		mutex_unlock(&cec_dev->cec_tx_mutex);
		return CEC_FAIL_NONE;
	}
	/*
	 * AO CEC controller will ack poll message itself if logical
	 *	address already set. Must clear it before poll again
	 * self polling mode
	 */
	if (is_poll_message(msg[0]))
		cec_clear_all_logical_addr(cec_sel);

	/*
	 * for CEC CTS 9.3. Android will try 3 poll message if got NACK
	 * but AOCEC will retry 4 tx for each poll message. Framework
	 * repeat this poll message so quick makes 12 sequential poll
	 * waveform seen on CEC bus. And did not pass CTS
	 * specification of 9.3
	 */
	if (cec_sel == CEC_A && need_nack_repeat_msg(msg, len, t)) {
		if (!memcmp(msg, last_cec_msg->msg, len)) {
			CEC_INFO("NACK repeat message:%x\n", len);
			mutex_unlock(&cec_dev->cec_tx_mutex);
			return CEC_FAIL_NACK;
		}
	}

	/* make sure we got valid physical address */
	if (len >= 2 && msg[1] == CEC_OC_REPORT_PHYSICAL_ADDRESS)
		check_physical_addr_valid(3);

try_again:
	reinit_completion(&cec_dev->tx_ok);
	/*
	 * CEC controller won't ack message if it is going to send
	 * state. If we detect cec line is low during waiting signal
	 * free time, that means a send is already started by other
	 * device, we should wait it finished.
	 */
	/* remove SW check of bus, let controller to
	 * get bus arbitration directly, otherwise it
	 * may cause message transfer delay/lost
	 */
	if (cec_dev->sw_chk_bus) {
		if (check_confilct()) {
			CEC_ERR("bus confilct too long\n");
			mutex_unlock(&cec_dev->cec_tx_mutex);
			return CEC_FAIL_BUSY;
		}
	}
	if (cec_sel == CEC_B)
		ret = cecb_trigle_tx(msg, len, signal_free_time);
	else
		ret = ceca_trigle_tx(msg, len);
	if (ret < 0) {
		/* we should increase send idx if busy */
		CEC_INFO("tx busy\n");
		if (retry > 0) {
			retry--;
			msleep(100 + (prandom_u32() & 0x07) * 10);
			dprintk(L_2, "retry0 %d\n", retry);
			goto try_again;
		}
		mutex_unlock(&cec_dev->cec_tx_mutex);
		return CEC_FAIL_BUSY;
	}
	cec_tx_result = -1;
	ret = wait_for_completion_timeout(&cec_dev->tx_ok, t);
	if (ret <= 0) {
		/* timeout or interrupt */
		if (ret == 0) {
			CEC_ERR("tx timeout\n");
			cec_hw_reset(cec_sel);
		}
		ret = CEC_FAIL_OTHER;
	} else {
		ret = cec_tx_result;
	}
	if (ret != CEC_FAIL_NONE && ret != CEC_FAIL_NACK) {
		if (retry > 0) {
			retry--;
			msleep(100 + (prandom_u32() & 0x07) * 10);
			dprintk(L_2, "retry1 %d\n", retry);
			goto try_again;
		}
	}

	if (cec_sel == CEC_A) {
		last_cec_msg->last_result = ret;
		if (ret == CEC_FAIL_NACK) {
			memcpy(last_cec_msg->msg, msg, len);
			last_cec_msg->len = len;
			last_cec_msg->last_jiffies = jiffies;
		}
	}

	dprintk(L_2, "%s ret:%d, %s\n", __func__, ret, cec_tx_ret_str(ret));
	mutex_unlock(&cec_dev->cec_tx_mutex);
	return ret;
}

static bool cec_need_store_msg_to_buff(void)
{
	/* if framework has not started msg reading
	 * or there's still msg left in store buff,
	 * need to continue to store the received msg,
	 * otherwise msg will be flushed and lost.
	 */
	if (!cec_dev->framework_on || cec_dev->msg_num > 0)
		return true;
	else
		return false;
}

static void cec_store_msg_to_buff(unsigned char len, unsigned char *msg)
{
	unsigned int i;
	unsigned int msg_idx;

	if (!cec_dev)
		return;

	if (cec_need_store_msg_to_buff() &&
		len < MAX_MSG &&
	    cec_dev->msg_idx < CEC_MSG_BUFF_MAX) {
		msg_idx = cec_dev->msg_idx;
		cec_dev->msgbuff[msg_idx].len = len;
		for (i = 0; i < MAX_MSG; i++)
			cec_dev->msgbuff[msg_idx].msg[i] = msg[i];
		cec_dev->msg_idx++;
		cec_dev->msg_num++;
		CEC_INFO("save len %d, num %d\n", len, cec_dev->msg_idx);
	}
}

/*
 * cec new message wait queue - wake up poll process
 */
static void cec_new_msg_push(void)
{
	if (cec_config(0, 0) & CEC_FUNC_CFG_CEC_ON) {
		complete(&cec_dev->rx_ok);
		new_msg = 1;
		/* will notify by stored_msg_push */
		if (cec_dev->framework_on && cec_need_store_msg_to_buff())
			return;
		wake_up(&cec_msg_wait_queue);
		/* uevent to notify cec msg received */
		queue_delayed_work(cec_dev->cec_rx_event_wq,
				   &cec_dev->work_cec_rx, 0);
	}
}

static void cec_stored_msg_push(void)
{
	if (cec_config(0, 0) & CEC_FUNC_CFG_CEC_ON) {
		wake_up(&cec_msg_wait_queue);
		queue_delayed_work(cec_dev->cec_rx_event_wq,
				   &cec_dev->work_cec_rx, CEC_NOTIFY_DELAY);
	}
}

static int cec_rx_buf_check(void)
{
	unsigned int rx_num_msg = 0;

	if (ee_cec == CEC_B) {
		cecb_check_irq_enable();
		cecb_irq_handle();
	} else {
		rx_num_msg = aocec_rd_reg(CEC_RX_NUM_MSG);
		if (rx_num_msg)
			CEC_INFO("rx msg num:0x%02x\n", rx_num_msg);
	}
	return rx_num_msg;
}

static int __maybe_unused cec_late_check_rx_buffer(void)
{
	int ret;
	/*struct delayed_work *dwork = &cec_dev->cec_work;*/

	ret = cec_rx_buf_check();
	if (!ret)
		return 0;
	/*
	 * start another check if rx buffer is full
	 */
	if ((-1) == ceca_rx_irq_handle(rx_msg, &rx_len)) {
		CEC_INFO("buffer got unrecorgnized msg\n");
		ceca_rx_buf_clear();
		return 0;
	}
	return 1;
}

static void cec_rx_process(void)
{
	int len = rx_len;
	int initiator, follower;
	int opcode;
	unsigned char msg[MAX_MSG] = {};
	int dest_phy_addr;

	if (len < 2 || !new_msg)		/* ignore ping message */
		return;

	memcpy(msg, rx_msg, len);
	initiator = ((msg[0] >> 4) & 0xf);
	follower  = msg[0] & 0xf;
	if (follower != 0xf && follower != cec_dev->cec_info.log_addr) {
		CEC_ERR("wrong rx message of bad follower:%x", follower);
		rx_len = 0;
		new_msg = 0;
		return;
	}
	opcode = msg[1];
	CEC_INFO("%s op 0x%x\n", __func__, opcode);
	switch (opcode) {
	case CEC_OC_ACTIVE_SOURCE:
		dest_phy_addr = msg[2] << 8 | msg[3];
		if (dest_phy_addr == 0xffff)
			break;
		dest_phy_addr |= (initiator << 16);
		if (cec_dev->cec_suspend == CEC_PW_STANDBY) {
			/*write_ao(AO_RTI_STATUS_REG1, dest_phy_addr);*/
			/*CEC_INFO("found wake up source:%x",*/
			/*	 dest_phy_addr);*/
		}
		break;

	case CEC_OC_ROUTING_CHANGE:
		dest_phy_addr = msg[4] << 8 | msg[5];
		if (dest_phy_addr == cec_dev->phy_addr &&
		    cec_dev->cec_suspend == CEC_PW_STANDBY) {
			CEC_INFO("wake up by ROUTING_CHANGE\n");
			cec_key_report(0);
		}
		break;

	case CEC_OC_GET_CEC_VERSION:
		cec_give_version(initiator);
		break;

	case CEC_OC_GIVE_DECK_STATUS:
		cec_give_deck_status(initiator);
		break;

	case CEC_OC_GIVE_PHYSICAL_ADDRESS:
		cec_report_physical_address_smp();
		break;

	case CEC_OC_GIVE_DEVICE_VENDOR_ID:
		cec_device_vendor_id();
		break;

	case CEC_OC_GIVE_OSD_NAME:
		cec_set_osd_name(initiator);
		break;

	case CEC_OC_STANDBY:
		cec_inactive_source(initiator);
		cec_menu_status_smp(initiator, DEVICE_MENU_INACTIVE);
		break;

	case CEC_OC_SET_STREAM_PATH:
		cec_set_stream_path(msg);
		/* wake up if in early suspend */
		dest_phy_addr = (unsigned int)(msg[2] << 8 | msg[3]);
		CEC_ERR("phyaddr:0x%x, 0x%x\n",
			cec_dev->phy_addr, dest_phy_addr);
		if (dest_phy_addr != 0xffff &&
		    dest_phy_addr == cec_dev->phy_addr &&
		    cec_dev->cec_suspend == CEC_PW_STANDBY)
			cec_key_report(0);
		break;

	case CEC_OC_REQUEST_ACTIVE_SOURCE:
		if (cec_dev->cec_suspend == CEC_PW_POWER_ON)
			cec_active_source_smp();
		break;

	case CEC_OC_GIVE_DEVICE_POWER_STATUS:
		if (cec_dev->cec_suspend == CEC_PW_POWER_ON)
			cec_report_power_status(initiator, CEC_PW_POWER_ON);
		else
			cec_report_power_status(initiator, CEC_PW_STANDBY);
		break;

	case CEC_OC_USER_CONTROL_PRESSED:
		/* wake up by key function */
		if (cec_dev->cec_suspend == CEC_PW_STANDBY) {
			if (msg[2] == 0x40 || msg[2] == 0x6d)
				cec_key_report(0);
		}
		break;

	case CEC_OC_MENU_REQUEST:
		if (cec_dev->cec_suspend != CEC_PW_POWER_ON)
			cec_menu_status_smp(initiator, DEVICE_MENU_INACTIVE);
		else
			cec_menu_status_smp(initiator, DEVICE_MENU_ACTIVE);
		break;

	case CEC_OC_IMAGE_VIEW_ON:
	case CEC_OC_TEXT_VIEW_ON:
		/* request active source needed */
		dest_phy_addr = 0xffff;
		dest_phy_addr =	(dest_phy_addr << 0) | (initiator << 16);
		if (cec_dev->cec_suspend == CEC_PW_STANDBY) {
			CEC_INFO("weak up by otp\n");
			cec_key_report(0);
		}
		break;

	default:
		CEC_ERR("cec unsupported cmd:0x%x, halflg:0x%x\n",
			opcode, cec_dev->hal_flag);
		break;
	}
	new_msg = 0;
}

static bool __maybe_unused cec_service_suspended(void)
{
	/* service is not enabled */
	/*if (!(cec_dev->hal_flag & (1 << HDMI_OPTION_SERVICE_FLAG)))*/
	/*	return false;*/

	if (!(cec_dev->hal_flag & (1 << HDMI_OPTION_SYSTEM_CEC_CONTROL)))
		return true;

	return false;
}

static void cec_save_pre_setting(void)
{
	unsigned int config_data;

	/* AO_DEBUG_REG1
	 * 0-15 : phy addr
	 * 16-20: logical address
	 * 21-23: device type
	 */
	config_data = cec_dev->cec_info.log_addr;
	cec_config2_logaddr(config_data, 1);
	config_data = cec_dev->dev_type;
	cec_config2_devtype(config_data, 1);

	CEC_ERR("%s: logaddr:0x%x, devtype:0x%x\n", __func__,
		cec_dev->cec_info.log_addr,
		(unsigned int)cec_dev->dev_type);
	cec_config2_phyaddr(cec_dev->phy_addr, 1);
}

#ifdef CEC_FREEZE_WAKE_UP
static void cec_restore_pre_setting(void)
{
	unsigned int logaddr;
	unsigned int devtype;
	unsigned int cec_cfg;
	/*unsigned int data32;*/
	/*char *token;*/

	cec_cfg = cec_config(0, 0);
	/*get device type*/
	/* AO_DEBUG_REG1+
	 * 0-15 : phy addr+
	 * 16-20: logical address+
	 * 21-23: device type+
	 */
	/*data32 = cec_config2_logaddr(0, 0);*/
	 logaddr = cec_config2_logaddr(0, 0);/*(data32 >> 16) & 0xf;*/
	 devtype = cec_config2_devtype(0, 0);/*(data32 >> 20) & 0xf;*/

	/*get logical address*/
	if (cec_dev->cec_num > ENABLE_ONE_CEC)
		cec_logicaddr_add(CEC_B, logaddr);
	else
		cec_logicaddr_add(ee_cec, logaddr);
	cec_dev->cec_info.addr_enable |= (1 << logaddr);
	/* add by hal, to init some data structure */
	cec_dev->dev_type = devtype;
	cec_dev->cec_info.log_addr = logaddr;
	cec_dev->phy_addr = cec_config2_phyaddr(0, 0);/*data32 & 0xffff;*/

	CEC_ERR("%s: logaddr:0x%x, devtype:%d\n", __func__,
		cec_dev->cec_info.log_addr,
		(unsigned int)cec_dev->dev_type);

	/*suspend freeze mode, driver handle cec msg*/
	cec_dev->hal_flag &= ~(1 << HDMI_OPTION_SERVICE_FLAG);

	/*token = kmalloc(2048, GFP_KERNEL);*/
	/*if (token) {*/
	/*	dump_cecrx_reg(token);*/
	/*	CEC_ERR("%s\n", token);*/
	/*	kfree(token);*/
	/*}*/
}
#endif

static void cec_task(struct work_struct *work)
{
	struct delayed_work *dwork = &cec_dev->cec_work;
	unsigned int cec_cfg;

	cec_cfg = cec_config(0, 0);
	if ((cec_cfg & CEC_FUNC_CFG_CEC_ON) &&
	    cec_dev->cec_suspend == CEC_PW_STANDBY) {
		/*cec module on*/
		#ifdef CEC_FREEZE_WAKE_UP
		if (/*(cec_dev && cec_service_suspended()) ||*/
		    is_pm_s2idle_mode())
		#else
		if (cec_dev && cec_service_suspended())
		#endif
			cec_rx_process();

		/*for check rx buffer for old chip version, cec rx irq process*/
		/*in internal hdmi rx, for avoid msg lose*/
		if (cec_dev->plat_data->chip_id <= CEC_CHIP_TXLX &&
		    cec_cfg == CEC_FUNC_CFG_ALL) {
			if (cec_late_check_rx_buffer()) {
				/*msg in*/
				mod_delayed_work(cec_dev->cec_thread, dwork, 0);
				return;
			}
		}
	}

	if (ceca_err_flag && cec_dev->probe_finish)
		cec_hw_reset(CEC_A);

	/*triger next process*/
	queue_delayed_work(cec_dev->cec_thread, dwork, CEC_FRAME_DELAY);
}

/******************** cec class interface *************************/
static ssize_t device_type_show(struct class *cla,
				struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%ld\n", cec_dev->dev_type);
}

static ssize_t device_type_store(struct class *cla,
				 struct class_attribute *attr,
				 const char *buf, size_t count)
{
	unsigned int type;

	if (kstrtouint(buf, 10, &type) != 0)
		return -EINVAL;

	cec_dev->dev_type = type;
	/*CEC_ERR("set dev_type to %d\n", type);*/
	return count;
}

static ssize_t menu_language_show(struct class *cla,
				  struct class_attribute *attr, char *buf)
{
	char a, b, c;

	a = ((cec_dev->cec_info.menu_lang >> 16) & 0xff);
	b = ((cec_dev->cec_info.menu_lang >>  8) & 0xff);
	c = ((cec_dev->cec_info.menu_lang >>  0) & 0xff);
	return sprintf(buf, "%c%c%c\n", a, b, c);
}

static ssize_t menu_language_store(struct class *cla,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{
	char a, b, c;

	if (sscanf(buf, "%c%c%c", &a, &b, &c) != 3)
		return -EINVAL;

	cec_dev->cec_info.menu_lang = (a << 16) | (b << 8) | c;
	/*CEC_ERR("set menu_language to %s\n", buf);*/
	return count;
}

static ssize_t vendor_id_show(struct class *cla,
			      struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", cec_dev->cec_info.vendor_id);
}

static ssize_t vendor_id_store(struct class *cla, struct class_attribute *attr,
			       const char *buf, size_t count)
{
	unsigned int id;

	if (kstrtouint(buf, 16, &id) != 0)
		return -EINVAL;
	cec_dev->cec_info.vendor_id = id;
	return count;
}

static ssize_t port_num_show(struct class *cla,
			     struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", cec_dev->port_num);
}

static ssize_t dump_reg_show(struct class *cla,
			     struct class_attribute *attr, char *b)
{
	return dump_cecrx_reg(b);
}

static ssize_t arc_port_show(struct class *cla,
			     struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", cec_dev->arc_port);
}

static ssize_t osd_name_show(struct class *cla,
			     struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", cec_dev->cec_info.osd_name);
}

static ssize_t port_seq_store(struct class *cla,
			      struct class_attribute *attr,
			      const char *buf, size_t count)
{
	unsigned int seq;

	if (kstrtouint(buf, 16, &seq) != 0)
		return -EINVAL;

	CEC_ERR("port_seq:%x\n", seq);
	cec_dev->port_seq = seq;
	return count;
}

static ssize_t port_seq_show(struct class *cla,
			     struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", cec_dev->port_seq);
}

static ssize_t port_status_show(struct class *cla,
				struct class_attribute *attr, char *buf)
{
	unsigned int tmp;
	unsigned int tx_hpd;

	tx_hpd = get_hpd_state();
	if (cec_dev->dev_type != CEC_TV_ADDR) {
		tmp = tx_hpd;
		return sprintf(buf, "%x\n", tmp);
	}
	tmp = hdmirx_rd_top(TOP_HPD_PWR5V);
	CEC_INFO("TOP_HPD_PWR5V:%x\n", tmp);
	tmp >>= 20;
	tmp &= 0xf;
	tmp |= (tx_hpd << 16);
	return sprintf(buf, "%x\n", tmp);
}

static ssize_t pin_status_show(struct class *cla,
			       struct class_attribute *attr, char *buf)
{
	unsigned int tx_hpd;
	char p;

	tx_hpd = get_hpd_state();
	if (cec_dev->dev_type != CEC_TV_ADDR) {
		if (!tx_hpd) {
			pin_status = 0;
			return sprintf(buf, "%s\n", "disconnected");
		}
		if (pin_status == 0) {
			p = (cec_dev->cec_info.log_addr << 4) | CEC_TV_ADDR;
			if (cec_ll_tx(&p, 1, SIGNAL_FREE_TIME_NEW_INITIATOR) == CEC_FAIL_NONE)
				return sprintf(buf, "%s\n", "ok");
			else
				return sprintf(buf, "%s\n", "fail");
		} else {
			return sprintf(buf, "%s\n", "ok");
		}
	} else {
		return sprintf(buf, "%s\n", pin_status ? "ok" : "fail");
	}
}

static ssize_t physical_addr_show(struct class *cla,
				  struct class_attribute *attr, char *buf)
{
	unsigned int tmp = cec_dev->phy_addr;

	return sprintf(buf, "%04x\n", tmp);
}

static ssize_t physical_addr_store(struct class *cla,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{
	int addr;

	if (kstrtouint(buf, 16, &addr) != 0)
		return -EINVAL;

	if (addr > 0xffff || addr < 0) {
		/*CEC_ERR("invalid input:%s\n", buf);*/
		phy_addr_test = 0;
		return -EINVAL;
	}
	cec_dev->phy_addr = addr;
	phy_addr_test = 1;
	return count;
}

static ssize_t dbg_en_show(struct class *cla,
			   struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", cec_msg_dbg_en);
}

static ssize_t dbg_en_store(struct class *cla, struct class_attribute *attr,
			    const char *buf, size_t count)
{
	int en;

	if (kstrtouint(buf, 16, &en) != 0)
		return -EINVAL;

	cec_msg_dbg_en = en;
	en = cec_config(0, 0);
	en |= 0x80000000;
	cec_config(en, 1);
	return count;
}

static ssize_t cmd_store(struct class *cla, struct class_attribute *attr,
			 const char *bu, size_t count)
{
	char buf[20] = {};
	int tmpbuf[20] = {};
	int i;
	int cnt;
	int ret;

	cnt = sscanf(bu, "%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",
		     &tmpbuf[0], &tmpbuf[1], &tmpbuf[2], &tmpbuf[3],
		     &tmpbuf[4], &tmpbuf[5], &tmpbuf[6], &tmpbuf[7],
		     &tmpbuf[8], &tmpbuf[9], &tmpbuf[10], &tmpbuf[11],
		     &tmpbuf[12], &tmpbuf[13], &tmpbuf[14], &tmpbuf[15]);
	if (cnt < 0)
		return -EINVAL;
	if (cnt > 16)
		cnt = 16;

	for (i = 0; i < cnt; i++)
		buf[i] = (char)tmpbuf[i];

	/*CEC_ERR("cnt=%d\n", cnt);*/
	ret = cec_ll_tx(buf, cnt, SIGNAL_FREE_TIME_NEW_INITIATOR);
	dprintk(L_2, "%s ret:%d, %s\n", __func__, ret, cec_tx_ret_str(ret));
	return count;
}

static ssize_t cmda_store(struct class *cla, struct class_attribute *attr,
			  const char *bu, size_t count)
{
	char buf[20] = {};
	int tmpbuf[20] = {};
	int i;
	int cnt;

	cnt = sscanf(bu, "%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",
		     &tmpbuf[0], &tmpbuf[1], &tmpbuf[2], &tmpbuf[3],
		     &tmpbuf[4], &tmpbuf[5], &tmpbuf[6], &tmpbuf[7],
		     &tmpbuf[8], &tmpbuf[9], &tmpbuf[10], &tmpbuf[11],
		     &tmpbuf[12], &tmpbuf[13], &tmpbuf[14], &tmpbuf[15]);
	if (cnt < 0)
		return -EINVAL;
	if (cnt > 16)
		cnt = 16;

	for (i = 0; i < cnt; i++)
		buf[i] = (char)tmpbuf[i];

	if (cec_dev->cec_num > ENABLE_ONE_CEC)
		ceca_trigle_tx(buf, cnt);

	return count;
}

static ssize_t cmdb_store(struct class *cla, struct class_attribute *attr,
			  const char *bu, size_t count)
{
	char buf[20] = {};
	int tmpbuf[20] = {};
	int i;
	int cnt;

	cnt = sscanf(bu, "%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",
		     &tmpbuf[0], &tmpbuf[1], &tmpbuf[2], &tmpbuf[3],
		     &tmpbuf[4], &tmpbuf[5], &tmpbuf[6], &tmpbuf[7],
		     &tmpbuf[8], &tmpbuf[9], &tmpbuf[10], &tmpbuf[11],
		     &tmpbuf[12], &tmpbuf[13], &tmpbuf[14], &tmpbuf[15]);
	if (cnt < 0)
		return -EINVAL;
	if (cnt > 16)
		cnt = 16;

	for (i = 0; i < cnt; i++)
		buf[i] = (char)tmpbuf[i];

	if (cec_dev->cec_num > ENABLE_ONE_CEC)
		cecb_trigle_tx(buf, cnt, SIGNAL_FREE_TIME_NEW_INITIATOR);

	return count;
}

static ssize_t wake_up_show(struct class *cla,
			    struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", *((unsigned int *)&cec_dev->wakup_data));
}

static ssize_t fun_cfg_store(struct class *cla, struct class_attribute *attr,
			     const char *bu, size_t count)
{
	int cnt, val;

	cnt = kstrtouint(bu, 16, &val);
	if (cnt < 0 || val > 0xff)
		return -EINVAL;
	cec_config(val, 1);
	if (val == 0)
		cec_clear_all_logical_addr(ee_cec);/*cec_keep_reset();*/
	else
		cec_pre_init();
	return count;
}

static ssize_t fun_cfg_show(struct class *cla,
			    struct class_attribute *attr, char *buf)
{
	unsigned int reg = cec_config(0, 0);

	return sprintf(buf, "0x%x\n", reg & 0xff);
}

static ssize_t cec_version_show(struct class *cla,
				struct class_attribute *attr, char *buf)
{
	/*CEC_INFO("driver date:%s\n", CEC_DRIVER_VERSION);*/
	return sprintf(buf, "%d\n", cec_dev->cec_info.cec_version);
}

static ssize_t log_addr_store(struct class *cla, struct class_attribute *attr,
			      const char *bu, size_t count)
{
	int cnt, val;

	cnt = kstrtoint(bu, 16, &val);
	if (cnt < 0 || val > 0xf)
		return -EINVAL;
	cec_logicaddr_set(val);
	/* add by hal, to init some data structure */
	cec_dev->cec_info.log_addr = val;
	cec_dev->cec_info.power_status = CEC_PW_POWER_ON;

	return count;
}

static ssize_t log_addr_show(struct class *cla,
			     struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", cec_dev->cec_info.log_addr);
}

static ssize_t dbg_store(struct class *cla, struct class_attribute *attr,
			 const char *bu, size_t count)
{
	const char *delim = " ";
	char *token;
	char *cur = (char *)bu;
	struct dbgflg *dbg = &stdbgflg;
	unsigned int addr, val;

	token = strsep(&cur, delim);
	if (token && strncmp(token, "bypass", 6) == 0) {
		/*get the second param*/
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &val) < 0)
			return count;

		dbg->hal_cmd_bypass = val ? 1 : 0;
		/*CEC_ERR("cmdbypass:%d\n", val);*/
	} else if (token && strncmp(token, "dbgen", 5) == 0) {
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &val) < 0)
			return count;

		cec_msg_dbg_en = val;
		CEC_ERR("msg_dbg_en:%d\n", val);
		val = cec_config(0, 0);
		val |= 0x80000000;
		cec_config(val, 1);
	} else if (token && strncmp(token, "rceca", 5) == 0) {
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;

		CEC_ERR("rd ceca reg:0x%x val:0x%x\n", addr,
			aocec_rd_reg(addr));
	} else if (token && strncmp(token, "wceca", 5) == 0) {
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;

		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &val) < 0)
			return count;

		CEC_ERR("wa ceca reg:0x%x val:0x%x\n", addr, val);
		aocec_wr_reg(addr, val);
	} else if (token && strncmp(token, "rcecb", 5) == 0) {
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;

		CEC_ERR("rd cecb reg:0x%x val:0x%x\n", addr,
			hdmirx_cec_read(addr));
	} else if (token && strncmp(token, "wcecb", 5) == 0) {
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;

		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &val) < 0)
			return count;

		CEC_ERR("wb cecb reg:0x%x val:0x%x\n", addr, val);
		hdmirx_cec_write(addr, val);
	} else if (token && strncmp(token, "dump", 4) == 0) {
		token = kmalloc(2048, GFP_KERNEL);
		if (token) {
			dump_cecrx_reg(token);
			CEC_ERR("%s\n", token);
			kfree(token);
		}
	} else if (token && strncmp(token, "status", 6) == 0) {
		cec_status();
	} else if (token && strncmp(token, "rao", 3) == 0) {
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;

		val = read_ao(addr);
		CEC_ERR("rao addr:0x%x, val:0x%x",  addr, val);
	} else if (token && strncmp(token, "wao", 3) == 0) {
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;

		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &val) < 0)
			return count;

		write_ao(addr, val);
		CEC_ERR("wao addr:0x%x, val:0x%x", addr, val);
	} else if (token && strncmp(token, "preinit", 7) == 0) {
		cec_config(CEC_FUNC_CFG_ALL, 1);
		cec_pre_init();
		cec_irq_enable(true);
	} else if (token && strncmp(token, "setaddr", 7) == 0) {
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;
		cec_logicaddr_set(addr);
	} else if (token && strncmp(token, "clraddr", 7) == 0) {
		cec_dev->cec_info.addr_enable = 0;
		cec_clear_all_logical_addr(ee_cec);
	} else if (token && strncmp(token, "clralladdr", 10) == 0) {
		cec_dev->cec_info.addr_enable = 0;
		cec_clear_all_logical_addr(CEC_A);
		cec_clear_all_logical_addr(CEC_B);
	} else if (token && strncmp(token, "addaddr", 7) == 0) {
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;
		cec_dev->cec_info.addr_enable |= (1 << (addr & 0xf));
		if (cec_dev->cec_num > ENABLE_ONE_CEC)
			cec_logicaddr_add(CEC_B, addr);
		else
			cec_logicaddr_add(ee_cec, addr);
		cec_dev->cec_info.log_addr = addr;
	} else if (token && strncmp(token, "rmaddr", 6) == 0) {
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;
		cec_dev->cec_info.addr_enable &= ~(1 << (addr & 0xf));
		if (cec_dev->cec_num > ENABLE_ONE_CEC)
			cec_logicaddr_remove(CEC_B, addr);
		else
			cec_logicaddr_remove(ee_cec, addr);
	} else if (token && strncmp(token, "addaaddr", 8) == 0) {
		if (cec_dev->plat_data->ceca_ver == CECA_NONE)
			return count;
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;
		cec_dev->cec_info.addr_enable |= (1 << (addr & 0xf));
		cec_logicaddr_add(CEC_A, addr);
	} else if (token && strncmp(token, "addbaddr", 8) == 0) {
		if (cec_dev->plat_data->cecb_ver == CECB_NONE)
			return count;
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;
		cec_dev->cec_info.addr_enable |= (1 << (addr & 0xf));
		cec_logicaddr_add(CEC_B, addr);
	} else if (token && strncmp(token, "sharepin", 8) == 0) {
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &val) < 0)
			return count;
		cec_ip_share_io(true, val);
		pr_info("share_io %d (0:a to b, 1:b to a)\n", val);
	} else if (token && strncmp(token, "devtype", 7) == 0) {
		token = strsep(&cur, delim);
		/*string to int*/
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;
		cec_ap_set_dev_type(addr);
	} else if (token && strncmp(token, "setfreeze", 9) == 0) {
		cec_save_pre_setting();
		CEC_ERR("Set enter freeze mode\n");
	}  else if (token && strncmp(token, "chk_sig_free", 12) == 0) {
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;
		cec_dev->chk_sig_free_time = !!addr;
		CEC_ERR("check signal free time enable: %d\n", cec_dev->chk_sig_free_time);
	} else if (token && strncmp(token, "sw_chk_bus", 10) == 0) {
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;
		cec_dev->sw_chk_bus = !!addr;
		CEC_ERR("sw_chk_bus enable: %d\n", cec_dev->sw_chk_bus);
	} else {
		if (token)
			CEC_ERR("no cmd:%s, supported list:\n", token);
		pr_info("bypass 0/1 -bypass android framework cmd\n");
		pr_info("dbgen 0/1/2 -enable debug log\n");
		pr_info("rceca addr -read cec a register\n");
		pr_info("wceca addr val -write cec a register\n");
		pr_info("rcecb addr -read cec b register\n");
		pr_info("wcecb addr val -write cec b register\n");
		pr_info("rao addr -read ao register\n");
		pr_info("wao addr val -write ao register\n");
		pr_info("preinit - pre init cec module\n");
		pr_info("setaddr val -set logical addr\n");
		pr_info("clraddr val -clear logical addr x\n");
		pr_info("clralladdr -clear all logical addr\n");
		pr_info("addaddr val -add a logical addr\n");
		pr_info("rmaddr val -del a logical addr\n");
		pr_info("addaaddr val -add cec a logical addr\n");
		pr_info("addbaddr val -add cec b logical addr\n");
		pr_info("dump -dump cec register\n");
		pr_info("status -pr cec driver info\n");
		pr_info("sharepin 0/1 -select share pinmux\n");
	}

	return count;
}

static ssize_t dbg_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	/*CEC_INFO(" %s\n", __func__);*/
	return 0;
}

static ssize_t port_info_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	unsigned char i = 0;
	int pos = 0;
	struct hdmi_port_info *port;

	port = kcalloc(cec_dev->port_num, sizeof(*port), GFP_KERNEL);
	if (!port) {
		CEC_ERR("no memory for port info\n");
		return pos;
	}
	/* check delay time:20 x 40ms maximum */
	check_physical_addr_valid(20);
	init_cec_port_info(port, cec_dev);

	for (i = 0; i < cec_dev->port_num; i++) {
		/* port_id: 1/2/3 means HDMIRX1/2/3, 0 means HDMITX port */
		pos += snprintf(buf + pos, PAGE_SIZE, "port_id: %d, ",
				port[i].port_id);
		pos += snprintf(buf + pos, PAGE_SIZE, "port_type: %s, ",
				port[i].type == HDMI_INPUT ?
				"hdmirx" : "hdmitx");
		pos += snprintf(buf + pos, PAGE_SIZE, "physical_address: %x, ",
				port[i].physical_address);
		pos += snprintf(buf + pos, PAGE_SIZE, "cec_supported: %s, ",
				port[i].cec_supported ? "true" : "false");
		pos += snprintf(buf + pos, PAGE_SIZE, "arc_supported: %s\n",
				port[i].arc_supported ? "true" : "false");
	}
	kfree(port);

	return pos;
}

/* cat after receive "hdmi_conn=" uevent
 * 0xXY, bit[4] stands for HDMITX port,
 * bit[3~0] stands for HDMIRX PCB port D~A,
 * bit value 1: plug, 0: unplug
 */
static ssize_t conn_status_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	unsigned int tmp = 0;

#if (defined(CONFIG_AMLOGIC_HDMITX) || defined(CONFIG_AMLOGIC_HDMITX21))
	tmp |= (get_hpd_state() << 4);
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_HDMI
	tmp |= (hdmirx_get_connect_info() & 0xF);
#endif
	return sprintf(buf, "0x%x\n", tmp);
}

/******************** cec hal interface ***************************/
static int hdmitx_cec_open(struct inode *inode, struct file *file)
{
	if (atomic_add_return(1, &cec_dev->cec_info.open_count)) {
		cec_dev->cec_info.hal_ctl = 1;
		/* set default logical addr flag for uboot */
		cec_config2_logaddr(0xf, 1);
	}
	return 0;
}

static int hdmitx_cec_release(struct inode *inode, struct file *file)
{
	if (!atomic_sub_return(1, &cec_dev->cec_info.open_count))
		cec_dev->cec_info.hal_ctl = 0;
	return 0;
}

static ssize_t hdmitx_cec_read(struct file *f, char __user *buf,
			       size_t size, loff_t *p)
{
	int ret;
	unsigned int idx;
	unsigned int len = 0;
	static unsigned int store_msg_rd_idx;

	if (!cec_dev)
		return 0;

	cec_dev->framework_on = 1;
	if (cec_dev->msg_num) {
		cec_dev->msg_num--;
		idx = store_msg_rd_idx;
		len = cec_dev->msgbuff[idx].len;
		store_msg_rd_idx++;
		if (copy_to_user(buf, &cec_dev->msgbuff[idx].msg[0], len))
			return -EINVAL;
		CEC_INFO("read msg from buff len=%d\n", len);
		/* notify uplayer to read all stored msg */
		if (cec_dev->msg_num > 0)
			cec_stored_msg_push();
		return len;
	}
	store_msg_rd_idx = 0;

	/*CEC_ERR("read msg start\n");*/
	ret = wait_for_completion_timeout(&cec_dev->rx_ok, CEC_FRAME_DELAY);
	if (ret <= 0) {
		/*CEC_ERR("read msg ret=0\n");*/
		return ret;
	}

	if (rx_len == 0) {
		/*CEC_ERR("read msg rx_len=0\n");*/
		return 0;
	}

	/* CEC off, cec IP will receive boradcast msg
	 * needn't transfer these msgs to cec framework
	 * discard the msg
	 */
	if (!(cec_config(0, 0) & CEC_FUNC_CFG_CEC_ON)) {
		new_msg = 0;
		return 0;
	}

	new_msg = 0;
	/*CEC_ERR("read msg end\n");*/
	if (copy_to_user(buf, rx_msg, rx_len))
		return -EINVAL;
	/*CEC_INFO("read msg len=%d\n", rx_len);*/
	return rx_len;
}

static ssize_t hdmitx_cec_write(struct file *f, const char __user *buf,
				size_t size, loff_t *p)
{
	unsigned char tempbuf[16] = {};
	int ret = CEC_FAIL_OTHER;
	unsigned int cec_cfg;

	if (stdbgflg.hal_cmd_bypass)
		return -EINVAL;

	if (size > 16)
		size = 16;
	if (size <= 0)
		return -EINVAL;

	if (copy_from_user(tempbuf, buf, size))
		return -EINVAL;

	/*osd name configed in android prop file*/
	if (tempbuf[1] == CEC_OC_SET_OSD_NAME) {
		memset(cec_dev->cec_info.osd_name, 0, 16);
		memcpy(cec_dev->cec_info.osd_name, &tempbuf[2], (size - 2));
		cec_dev->cec_info.osd_name[15] = (size - 2);
	}

	cec_cfg = cec_config(0, 0);
	if (cec_cfg & CEC_FUNC_CFG_CEC_ON) {
		/*cec module on*/
		ret = cec_ll_tx(tempbuf, size, SIGNAL_FREE_TIME_NEW_INITIATOR);
	}
	return ret;
}

static long hdmitx_cec_ioctl(struct file *f,
			     unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	unsigned int tmp;
	struct hdmi_port_info *port;
	unsigned int a, i = 0;
	struct st_rx_msg wk_msg;
	/*unsigned int tx_hpd;*/

	mutex_lock(&cec_dev->cec_ioctl_mutex);
	switch (cmd) {
	case CEC_IOC_GET_PHYSICAL_ADDR:
		/*check_physical_addr_valid(20);*/
		/* physical address for TV or repeator */
		tmp = cec_get_cur_phy_addr();
		if (cec_dev->dev_type != CEC_TV_ADDR && tmp != 0 &&
		    tmp != 0xffff)
			cec_dev->phy_addr = tmp;

		if (!phy_addr_test) {
			cec_config2_phyaddr(cec_dev->phy_addr, 1);
			/*CEC_INFO("type %d, save phy_addr:0x%x\n",*/
			/*	 (unsigned int)cec_dev->dev_type,*/
			/*	 cec_dev->phy_addr);*/
		} else {
			tmp = cec_dev->phy_addr;
		}

		if (copy_to_user(argp, &cec_dev->phy_addr, _IOC_SIZE(cmd))) {
			mutex_unlock(&cec_dev->cec_ioctl_mutex);
			return -EINVAL;
		}
		break;

	case CEC_IOC_GET_VERSION:
		tmp = cec_dev->cec_info.cec_version;
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd))) {
			mutex_unlock(&cec_dev->cec_ioctl_mutex);
			return -EINVAL;
		}
		break;

	case CEC_IOC_GET_VENDOR_ID:
		tmp = cec_dev->cec_info.vendor_id;
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd))) {
			mutex_unlock(&cec_dev->cec_ioctl_mutex);
			return -EINVAL;
		}
		break;

	case CEC_IOC_GET_PORT_NUM:
		tmp = cec_dev->port_num;
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd))) {
			mutex_unlock(&cec_dev->cec_ioctl_mutex);
			return -EINVAL;
		}
		break;

	case CEC_IOC_GET_PORT_INFO:
		port = kcalloc(cec_dev->port_num, sizeof(*port), GFP_KERNEL);
		if (!port) {
			CEC_ERR("no memory\n");
			mutex_unlock(&cec_dev->cec_ioctl_mutex);
			return -EINVAL;
		}
		check_physical_addr_valid(20);	/*delay time:20 x 100ms*/
		init_cec_port_info(port, cec_dev);
		if (copy_to_user(argp, port, sizeof(*port) *
				cec_dev->port_num))
			CEC_ERR("err get port info\n");

		kfree(port);
		break;

	case CEC_IOC_SET_OPTION_WAKEUP:
		tmp = cec_config(0, 0);
		if (arg)
			tmp |= CEC_FUNC_CFG_AUTO_POWER_ON;
		else
			tmp &= ~(CEC_FUNC_CFG_AUTO_POWER_ON);

		if (tmp & CEC_IOC_SET_OPTION_ENALBE_CEC) {
			if (tmp & CEC_FUNC_CFG_AUTO_POWER_ON)
				hdmirx_set_cec_cfg(2);
			else
				hdmirx_set_cec_cfg(1);
		} else {
			hdmirx_set_cec_cfg(0);
		}

		cec_config(tmp, 1);
		break;

	case CEC_IOC_SET_AUTO_DEVICE_OFF:
		tmp = cec_config(0, 0);
		if (arg)
			tmp |= CEC_FUNC_CFG_AUTO_STANDBY;
		else
			tmp &= ~(CEC_FUNC_CFG_AUTO_STANDBY);
		cec_config(tmp, 1);
		break;

	case CEC_IOC_SET_OPTION_ENALBE_CEC:
		a = cec_config(0, 0);
		if (arg)
			a |= CEC_FUNC_CFG_CEC_ON;
		else
			a &= ~(CEC_FUNC_CFG_CEC_ON);

		if (a & CEC_IOC_SET_OPTION_ENALBE_CEC) {
			if (a & CEC_FUNC_CFG_AUTO_POWER_ON)
				hdmirx_set_cec_cfg(2);
			else
				hdmirx_set_cec_cfg(1);
		} else {
			hdmirx_set_cec_cfg(0);
		}

		cec_config(a, 1);

		tmp = (1 << HDMI_OPTION_ENABLE_CEC);
		if (arg) {
			cec_dev->hal_flag |= tmp;
			cec_pre_init();
		} else {
			cec_dev->hal_flag &= ~(tmp);
			/*CEC_INFO("disable CEC\n");*/
			/*cec_keep_reset();*/
			cec_clear_all_logical_addr(ee_cec);
		}
		break;

	case CEC_IOC_SET_OPTION_SYS_CTRL:
		tmp = (1 << HDMI_OPTION_SYSTEM_CEC_CONTROL);
		/* now framework ready to handle msg, if there's
		 * any msg received or stored, notify framework
		 * to read, including CEC wakeup msg
		 */
		if (arg) {
			cec_dev->hal_flag |= tmp;
			if (new_msg || cec_dev->msg_num > 0)
				cec_stored_msg_push();
			CEC_ERR("CEC framework ctrl enabled\n");
		} else {
			cec_dev->hal_flag &= ~(tmp);
			CEC_ERR("CEC framework ctrl disabled\n");
		}

		cec_dev->hal_flag |= (1 << HDMI_OPTION_SERVICE_FLAG);
		break;

	case CEC_IOC_SET_OPTION_SET_LANG:
		cec_dev->cec_info.menu_lang = arg;
		break;

	case CEC_IOC_GET_CONNECT_STATUS:
		if (copy_from_user(&a, argp, _IOC_SIZE(cmd))) {
			mutex_unlock(&cec_dev->cec_ioctl_mutex);
			return -EINVAL;
		}

		/* mixed for rx & tx */
		/* a is current port idx, 0: tx device */
		if (a != 0) {
			tmp = hdmirx_get_connect_info() & 0xF;
			for (i = 0; i < CEC_PHY_PORT_NUM; i++) {
				if (((cec_dev->port_seq >> i * 4) & 0xF) == a)
					break;
			}
			dprintk(L_3, "phy port:%d, ui port:%d\n", i, a);

			if ((tmp & (1 << i)) && a != 0xF)
				tmp = 1;
			else
				tmp = 0;
		} else {
			tmp = get_hpd_state();
		}
		/*CEC_ERR("port id:%d, sts:%d\n", a, tmp);*/
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd))) {
			mutex_unlock(&cec_dev->cec_ioctl_mutex);
			return -EINVAL;
		}
		break;

	case CEC_IOC_ADD_LOGICAL_ADDR:
		cec_ap_add_logical_addr(arg & 0xf);
		break;

	case CEC_IOC_CLR_LOGICAL_ADDR:
		cec_ap_clear_logical_addr();
		break;

	case CEC_IOC_SET_DEV_TYPE:
		cec_ap_set_dev_type(arg);
		cec_config2_devtype(cec_dev->dev_type, 1);
		break;

	case CEC_IOC_SET_ARC_ENABLE:
		/*CEC_INFO("Ioc set arc pin\n");*/
		cec_enable_arc_pin(arg);
		break;

	case CEC_IOC_GET_BOOT_ADDR:
		tmp = (cec_dev->wakup_data.wk_logic_addr << 16) |
				cec_dev->wakup_data.wk_phy_addr;
		CEC_ERR("Boot addr:%#x\n", (unsigned int)tmp);
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd))) {
			mutex_unlock(&cec_dev->cec_ioctl_mutex);
			return -EINVAL;
		}
		break;

	case CEC_IOC_GET_BOOT_REASON:
		tmp = cec_dev->wakeup_reason;
		/*CEC_ERR("Boot reason:%#x\n", (unsigned int)tmp);*/
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd))) {
			mutex_unlock(&cec_dev->cec_ioctl_mutex);
			return -EINVAL;
		}
		break;

	case CEC_IOC_SET_FREEZE_MODE:
		/* system enter power down freeze mode
		 * need save current device type and logical addr
		 */
		cec_save_pre_setting();
		/*CEC_ERR("need enter freeze mode\n");*/
		break;

	case CEC_IOC_GET_BOOT_PORT:
		tmp = cec_dev->wakup_data.wk_port_id;
		/*CEC_ERR("Boot port:%#x\n", (unsigned int)tmp);*/
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd))) {
			mutex_unlock(&cec_dev->cec_ioctl_mutex);
			return -EINVAL;
		}
		break;
	case CEC_IOC_SET_DEBUG_EN:
		cec_msg_dbg_en = arg & 0xff;
		break;
	case CEC_IOC_GET_WK_OTP_MSG:
		memset(&wk_msg, 0x0, sizeof(wk_msg));
		wk_msg.len = cec_dev->cec_wk_otp_msg[0];
		memcpy(wk_msg.msg, &cec_dev->cec_wk_otp_msg[1],
		       sizeof(wk_msg.len));
		if (copy_to_user(argp, &wk_msg, _IOC_SIZE(cmd))) {
			mutex_unlock(&cec_dev->cec_ioctl_mutex);
			return -EINVAL;
		}
		break;
	case CEC_IOC_GET_WK_AS_MSG:
		memset(&wk_msg, 0x0, sizeof(wk_msg));
		wk_msg.len = cec_dev->cec_wk_as_msg[0];
		memcpy(wk_msg.msg, &cec_dev->cec_wk_as_msg[1],
		       sizeof(wk_msg.len));
		if (copy_to_user(argp, &wk_msg, _IOC_SIZE(cmd))) {
			mutex_unlock(&cec_dev->cec_ioctl_mutex);
			return -EINVAL;
		}
		break;
	case CEC_IOC_KEY_EVENT:
		input_event(cec_dev->cec_info.remote_cec_dev, EV_KEY,
				arg & 0xFFF, (arg >> 12) & 0x1);
		input_sync(cec_dev->cec_info.remote_cec_dev);
		CEC_INFO("input keyevent %lu\n", arg & 0xFFF);
		break;
	default:
		CEC_ERR("error ioctrl: 0x%x\n", cmd);
		break;
	}
	mutex_unlock(&cec_dev->cec_ioctl_mutex);
	return 0;
}

#ifdef CONFIG_COMPAT
static long hdmitx_cec_compat_ioctl(struct file *f,
				    unsigned int cmd, unsigned long arg)
{
	arg = (unsigned long)compat_ptr(arg);
	return hdmitx_cec_ioctl(f, cmd, arg);
}
#endif

/*
 * For android framework check new message
 */
static unsigned int cec_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(filp, &cec_msg_wait_queue, wait);
	if (new_msg || cec_dev->msg_num > 0)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

/* for improve rw permission */
static char *aml_cec_class_devnode(struct device *dev, umode_t *mode)
{
	if (mode) {
		*mode = 0666;
		CEC_INFO("mode is %x\n", *mode);
	}
	return NULL;
}

static CLASS_ATTR_WO(cmd);
static CLASS_ATTR_WO(cmda);
static CLASS_ATTR_WO(cmdb);
static CLASS_ATTR_RO(port_num);
static CLASS_ATTR_RO(osd_name);
static CLASS_ATTR_RO(dump_reg);
static CLASS_ATTR_RO(port_status);
static CLASS_ATTR_RO(pin_status);
static CLASS_ATTR_RO(cec_version);
static CLASS_ATTR_RO(arc_port);
static CLASS_ATTR_RO(wake_up);
static CLASS_ATTR_RW(port_seq);
static CLASS_ATTR_RW(physical_addr);
static CLASS_ATTR_RW(vendor_id);
static CLASS_ATTR_RW(menu_language);
static CLASS_ATTR_RW(device_type);
static CLASS_ATTR_RW(dbg_en);
static CLASS_ATTR_RW(log_addr);
static CLASS_ATTR_RW(fun_cfg);
static CLASS_ATTR_RW(dbg);
static CLASS_ATTR_RO(conn_status);
static CLASS_ATTR_RO(port_info);

static struct attribute *aocec_class_attrs[] = {
	&class_attr_cmd.attr,
	&class_attr_cmda.attr,
	&class_attr_cmdb.attr,
	&class_attr_port_num.attr,
	&class_attr_osd_name.attr,
	&class_attr_dump_reg.attr,
	&class_attr_port_status.attr,
	&class_attr_pin_status.attr,
	&class_attr_cec_version.attr,
	&class_attr_arc_port.attr,
	&class_attr_wake_up.attr,
	&class_attr_port_seq.attr,
	&class_attr_physical_addr.attr,
	&class_attr_vendor_id.attr,
	&class_attr_menu_language.attr,
	&class_attr_device_type.attr,
	&class_attr_dbg_en.attr,
	&class_attr_log_addr.attr,
	&class_attr_fun_cfg.attr,
	&class_attr_dbg.attr,
	&class_attr_conn_status.attr,
	&class_attr_port_info.attr,
	NULL,
};

ATTRIBUTE_GROUPS(aocec_class);
static struct class aocec_class = {
	.name = CEC_DEV_NAME,
	.owner = THIS_MODULE,
	.class_groups = aocec_class_groups,
	.devnode = aml_cec_class_devnode,
};

static const struct file_operations hdmitx_cec_fops = {
	.owner          = THIS_MODULE,
	.open           = hdmitx_cec_open,
	.read           = hdmitx_cec_read,
	.write          = hdmitx_cec_write,
	.release        = hdmitx_cec_release,
	.unlocked_ioctl = hdmitx_cec_ioctl,
	.poll		= cec_poll,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = hdmitx_cec_compat_ioctl,
#endif
};

/************************ cec high level code *****************************/
#ifdef CONFIG_HAS_EARLYSUSPEND
static void aocec_early_suspend(struct early_suspend *h)
{
	cec_dev->cec_suspend = CEC_PW_STANDBY;
	cec_save_mail_box();
	/* reset wakeup reason for considering light sleep situation*/
	cec_dev->wakeup_reason = 0;
	CEC_INFO("%s\n", __func__);
}

static void aocec_late_resume(struct early_suspend *h)
{
	if (!cec_dev)
		return;

	/* cec_dev->wakeup_st = 0; */
	/* cec_dev->msg_idx = 0; */

	cec_dev->cec_suspend = CEC_PW_POWER_ON;
	CEC_ERR("%s, suspend sts:%d\n", __func__, cec_dev->cec_suspend);
}
#endif

#ifdef CONFIG_OF
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static const struct cec_platform_data_s cec_gxl_data = {
	.chip_id = CEC_CHIP_GXL,
	.line_reg = 0,
	.line_bit = 8,
	.ee_to_ao = 0,
	.ceca_sts_reg = 0,
	.ceca_ver = CECA_VER_0,
	.cecb_ver = CECB_VER_0,
	.share_io = false,
	.reg_tab_group = cec_reg_group_old,
};

static const struct cec_platform_data_s cec_txlx_data = {
	.chip_id = CEC_CHIP_TXLX,
	.line_reg = 0,
	.line_bit = 7,
	.ee_to_ao = 1,
	.ceca_sts_reg = 0,
	.ceca_ver = CECA_VER_0,
	.cecb_ver = CECB_VER_1,
	.share_io = false,
	.reg_tab_group = cec_reg_group_old,
};

static const struct cec_platform_data_s cec_txl_data = {
	.chip_id = CEC_CHIP_TXL,
	.line_reg = 0,/*line_reg=0:AO_GPIO_I*/
	.line_bit = 7,
	.ee_to_ao = 0,
	.ceca_sts_reg = 0,
	.ceca_ver = CECA_VER_0,
	.cecb_ver = CECB_VER_0,
	.share_io = false,
	.reg_tab_group = cec_reg_group_old,
};

static const struct cec_platform_data_s cec_tl1_data = {
	.chip_id = CEC_CHIP_TL1,
	.line_reg = 0,/*line_reg=0:AO_GPIO_I*/
	.line_bit = 10,
	.ee_to_ao = 1,
	.ceca_sts_reg = 1,
	.ceca_ver = CECA_VER_0,
	.cecb_ver = CECB_VER_2,
	.share_io = false,
	.reg_tab_group = cec_reg_group_old,
};

static const struct cec_platform_data_s cec_a1_data = {
	.chip_id = CEC_CHIP_A1,
	.line_reg = 0xff,/*don't check*/
	.line_bit = 0,
	.ee_to_ao = 1,
	.ceca_sts_reg = 1,
	.ceca_ver = CECA_VER_1,
	.cecb_ver = CECB_VER_2,
	.share_io = true,
	.reg_tab_group = cec_reg_group_a1,
};
#endif

static const struct cec_platform_data_s cec_g12a_data = {
	.chip_id = CEC_CHIP_G12A,
	.line_reg = 1,
	.line_bit = 3,
	.ee_to_ao = 1,
	.ceca_sts_reg = 0,
	.ceca_ver = CECA_VER_0,
	.cecb_ver = CECB_VER_1,
	.share_io = false,
	.reg_tab_group = cec_reg_group_old,
};

static const struct cec_platform_data_s cec_g12b_data = {
	.chip_id = CEC_CHIP_G12B,
	.line_reg = 1,
	.line_bit = 3,
	.ee_to_ao = 1,
	.ceca_sts_reg = 0,
	.ceca_ver = CECA_VER_0,
	.cecb_ver = CECB_VER_1,
	.share_io = false,
	.reg_tab_group = cec_reg_group_old,
};


static const struct cec_platform_data_s cec_sm1_data = {
	.chip_id = CEC_CHIP_SM1,
	.line_reg = 1,/*line_reg=1:PREG_PAD_GPIO3_I*/
	.line_bit = 3,
	.ee_to_ao = 1,
	.ceca_sts_reg = 1,
	.ceca_ver = CECA_VER_1,
	.cecb_ver = CECB_VER_2,
	.share_io = true,
	.reg_tab_group = cec_reg_group_old,
};

static const struct cec_platform_data_s cec_tm2_data = {
	.chip_id = CEC_CHIP_TM2,
	/* don't check */
	.line_reg = 0xff,
	.line_bit = 10,
	.ee_to_ao = 1,
	.ceca_sts_reg = 1,
	.ceca_ver = CECA_VER_1,
	.cecb_ver = CECB_VER_2,
	.share_io = true,
	.reg_tab_group = cec_reg_group_old,
};

static const struct cec_platform_data_s cec_sc2_data = {
	.chip_id = CEC_CHIP_SC2,
	.line_reg = 0xff,/*don't check*/
	.line_bit = 0,
	.ee_to_ao = 1,
	.ceca_sts_reg = 1,
	.ceca_ver = CECA_VER_1,
	.cecb_ver = CECB_VER_3,
	.share_io = true,
	.reg_tab_group = cec_reg_group_a1,
};

static const struct cec_platform_data_s cec_t5_data = {
	.chip_id = CEC_CHIP_T5,
	.line_reg = 0xff,/*don't check*/ /*line_reg=0:AO_GPIO_I*/
	.line_bit = 10,
	.ee_to_ao = 1,
	.ceca_sts_reg = 1,
	.ceca_ver = CECA_NONE,
	.cecb_ver = CECB_VER_2,
	.share_io = true,
	.reg_tab_group = cec_reg_group_old,
};

static const struct cec_platform_data_s cec_t5d_data = {
	.chip_id = CEC_CHIP_T5D,
	.line_reg = 0xff,/*don't check*/ /*line_reg=0:AO_GPIO_I*/
	.line_bit = 10,
	.ee_to_ao = 1,
	.ceca_sts_reg = 1,
	.ceca_ver = CECA_NONE,
	.cecb_ver = CECB_VER_2,
	.share_io = true,
	.reg_tab_group = cec_reg_group_old,
};

static const struct cec_platform_data_s cec_t7_data = {
	.chip_id = CEC_CHIP_T7,
	.line_reg = 0xff,/*don't check*/
	.line_bit = 0,
	.ee_to_ao = 1,
	.ceca_sts_reg = 1,
	.ceca_ver = CECA_VER_1,
	.cecb_ver = CECB_VER_3,
	.share_io = true,
	.reg_tab_group = cec_reg_group_a1,
};

static const struct cec_platform_data_s cec_s4_data = {
	.chip_id = CEC_CHIP_S4,
	.line_reg = 0xff,/*don't check*/
	.line_bit = 0,
	.ee_to_ao = 1,
	.ceca_sts_reg = 1,
	.ceca_ver = CECA_VER_1,
	.cecb_ver = CECB_VER_3,
	.share_io = true,
	.reg_tab_group = cec_reg_group_a1,
};

static const struct cec_platform_data_s cec_t3_data = {
	.chip_id = CEC_CHIP_T3,
	.line_reg = 0xff,/*don't check*/
	.line_bit = 0,
	.ee_to_ao = 1,
	.ceca_sts_reg = 0,
	.ceca_ver = CECA_NONE,
	.cecb_ver = CECB_VER_3,
	.share_io = false,
	.reg_tab_group = cec_reg_group_a1,
};

static const struct cec_platform_data_s cec_t5w_data = {
	.chip_id = CEC_CHIP_T5W,
	.line_reg = 0xff,/*don't check*/ /*line_reg=0:AO_GPIO_I*/
	.line_bit = 10,
	.ee_to_ao = 1,
	.ceca_sts_reg = 1,
	.ceca_ver = CECA_NONE,
	.cecb_ver = CECB_VER_2,
	.share_io = true,
	.reg_tab_group = cec_reg_group_old,
};

static const struct of_device_id aml_cec_dt_match[] = {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible = "amlogic, amlogic-aocec",
		.data = &cec_gxl_data,
	},
	{
		.compatible = "amlogic, aocec-txlx",
		.data = &cec_txlx_data,
	},
	{
		.compatible = "amlogic, aocec-txl",
		.data = &cec_txl_data,
	},
	{
		.compatible = "amlogic, aocec-tl1",
		.data = &cec_tl1_data,
	},
	{
		.compatible = "amlogic, aocec-a1",
		.data = &cec_a1_data,
	},
#endif
	{
		.compatible = "amlogic, aocec-g12a",
		.data = &cec_g12a_data,
	},
	{
		.compatible = "amlogic, aocec-g12b",
		.data = &cec_g12b_data,
	},
	{
		.compatible = "amlogic, aocec-sm1",
		.data = &cec_sm1_data,
	},
	{
		.compatible = "amlogic, aocec-tm2",
		.data = &cec_tm2_data,
	},
	{
		.compatible = "amlogic, aocec-sc2",
		.data = &cec_sc2_data,
	},
	{
		.compatible = "amlogic, aocec-t5",
		.data = &cec_t5_data,
	},
	{
		.compatible = "amlogic, aocec-t5d",
		.data = &cec_t5d_data,
	},
	{
		.compatible = "amlogic, aocec-t7",
		.data = &cec_t7_data,
	},
	{
		.compatible = "amlogic, aocec-s4",
		.data = &cec_s4_data,
	},
	{
		.compatible = "amlogic, aocec-t3",
		.data = &cec_t3_data,
	},
	{
		.compatible = "amlogic, aocec-t5w",
		.data = &cec_t5w_data,
	},
	{}
};
#endif

static void cec_node_val_init(void)
{
	/* initial main logical address */
	cec_dev->cec_info.log_addr = 0;
	/* all logical address disable */
	cec_dev->cec_info.addr_enable = 0;
	cec_dev->cec_info.open_count.counter = 0;
}

static void cec_dbg_init(void)
{
	stdbgflg.hal_cmd_bypass = 0;
}

static int __of_irq_count(struct device_node *dev)
{
	struct of_phandle_args irq;
	int nr = 0;

	while (of_irq_parse_one(dev, nr, &irq) == 0)
		nr++;

	return nr;
}

/* handler for both hdmitx & rx */
static void cec_hdmi_plug_handler(struct work_struct *work)
{
	/* struct ao_cec_dev *pcec_dev = container_of((struct delayed_work *)work, */
		/* struct ao_cec_dev, work_hdmitx_plug); */
	unsigned int tmp = 0;

#if (defined(CONFIG_AMLOGIC_HDMITX) || defined(CONFIG_AMLOGIC_HDMITX21))
	tmp |= (get_hpd_state() << 4);
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_HDMI
	tmp |= (hdmirx_get_connect_info() & 0xF);
#endif

	cec_set_uevent(HDMI_PLUG_EVENT, tmp);
}

static void cec_rx_uevent_handler(struct work_struct *work)
{
	/* struct ao_cec_dev *pcec_dev = container_of((struct delayed_work *)work, */
		/* struct ao_cec_dev, work_cec_rx); */
	/* notify framework to read cec msg */
	cec_set_uevent(CEC_RX_MSG, 1);
	/* clear notify */
	cec_set_uevent(CEC_RX_MSG, 0);
}

static int aml_cec_probe(struct platform_device *pdev)
{
	struct device *cdev;
	int ret = 0;
	const struct of_device_id *of_id;
#ifdef CONFIG_OF
	struct device_node *node = pdev->dev.of_node;
	int r;
	const char *irq_name_a = NULL;
	const char *irq_name_b = NULL;
	struct pinctrl *pin;
	struct vendor_info *vend;
	struct resource *res;
	resource_size_t *base;
#endif

	if (!node || !node->name) {
		pr_err("%s no devtree node\n", __func__);
		return -ENOMEM;
	}

	cec_dev = devm_kzalloc(&pdev->dev, sizeof(struct ao_cec_dev),
			       GFP_KERNEL);
	if (IS_ERR(cec_dev)) {
		dev_err(&pdev->dev, "device malloc err!\n");
		ret = -ENOMEM;
		goto tag_cec_devm_err;
	}

	/*will replace by CEC_IOC_SET_DEV_TYPE*/
	cec_dev->dev_type = CEC_PLAYBACK_DEVICE_1_ADDR;
	cec_dev->dbg_dev  = &pdev->dev;
	/* cec_dev->cpu_type = get_cpu_type(); */
	cec_dev->node = pdev->dev.of_node;
	cec_dev->probe_finish = false;
	phy_addr_test = 0;
	/*CEC_ERR("%s driver ver:%s\n", __func__, CEC_DRIVER_VERSION);*/
	cec_dbg_init();

	/* cdev registe */
	r = class_register(&aocec_class);
	if (r) {
		CEC_ERR("regist class failed\n");
		ret = -EINVAL;
		goto tag_cec_class_reg;
	}
	pdev->dev.class = &aocec_class;
	r = register_chrdev(0, CEC_DEV_NAME,
			    &hdmitx_cec_fops);
	if (r < 0) {
		CEC_ERR("alloc chrdev failed\n");
		ret = -EINVAL;
		goto tag_cec_chr_reg_err;
	}
	cec_dev->cec_info.dev_no = r;
	/*CEC_INFO("alloc chrdev %x\n", cec_dev->cec_info.dev_no);*/
	cdev = device_create(&aocec_class, &pdev->dev,
			     MKDEV(cec_dev->cec_info.dev_no, 0),
			     NULL, CEC_DEV_NAME);
	if (IS_ERR(cdev)) {
		CEC_ERR("create chrdev failed, dev:%p\n", cdev);
		ret = -EINVAL;
		goto tag_cec_device_create_err;
	}

	/*get compatible matched device, to get chip related data*/
	of_id = of_match_device(aml_cec_dt_match, &pdev->dev);
	if (of_id) {
		cec_dev->plat_data = (struct cec_platform_data_s *)of_id->data;
		dprintk(L_4, "compatible:%s\n", of_id->compatible);
		dprintk(L_4, "chip_id:%d\n", cec_dev->plat_data->chip_id);
		dprintk(L_4, "line_reg:%d\n", cec_dev->plat_data->line_reg);
		dprintk(L_4, "line_bit:%d\n", cec_dev->plat_data->line_bit);
		dprintk(L_4, "ceca_ver:%d\n", cec_dev->plat_data->ceca_ver);
		dprintk(L_4, "cecb_ver:%d\n", cec_dev->plat_data->cecb_ver);
	} else {
		CEC_ERR("unable to get matched device\n");
	}

	cec_node_val_init();
	init_completion(&cec_dev->rx_ok);
	init_completion(&cec_dev->tx_ok);
	mutex_init(&cec_dev->cec_tx_mutex);
	mutex_init(&cec_dev->cec_ioctl_mutex);
	mutex_init(&cec_dev->cec_uevent_mutex);
	spin_lock_init(&cec_dev->cec_reg_lock);
	cec_dev->cec_info.remote_cec_dev = input_allocate_device();
	if (!cec_dev->cec_info.remote_cec_dev) {
		CEC_INFO("No enough memory\n");
		ret = -ENOMEM;
		goto tag_cec_alloc_input_err;
	}

	cec_dev->cec_info.remote_cec_dev->name = "cec_input";

	cec_dev->cec_info.remote_cec_dev->evbit[0] = BIT_MASK(EV_KEY);
	cec_dev->cec_info.remote_cec_dev->keybit[BIT_WORD(BTN_0)] =
		BIT_MASK(BTN_0);
	cec_dev->cec_info.remote_cec_dev->id.bustype = BUS_ISA;
	cec_dev->cec_info.remote_cec_dev->id.vendor = 0x1b8e;
	cec_dev->cec_info.remote_cec_dev->id.product = 0x0cec;
	cec_dev->cec_info.remote_cec_dev->id.version = 0x0001;

	set_bit(KEY_POWER, cec_dev->cec_info.remote_cec_dev->keybit);
	set_bit(KEY_ENTER, cec_dev->cec_info.remote_cec_dev->keybit);
	set_bit(KEY_UP, cec_dev->cec_info.remote_cec_dev->keybit);
	set_bit(KEY_DOWN, cec_dev->cec_info.remote_cec_dev->keybit);
	set_bit(KEY_LEFT, cec_dev->cec_info.remote_cec_dev->keybit);
	set_bit(KEY_RIGHT, cec_dev->cec_info.remote_cec_dev->keybit);
	set_bit(KEY_HOMEPAGE, cec_dev->cec_info.remote_cec_dev->keybit);
	set_bit(KEY_ESC, cec_dev->cec_info.remote_cec_dev->keybit);
	set_bit(KEY_TAB, cec_dev->cec_info.remote_cec_dev->keybit);
	set_bit(KEY_VOLUMEUP, cec_dev->cec_info.remote_cec_dev->keybit);
	set_bit(KEY_VOLUMEDOWN, cec_dev->cec_info.remote_cec_dev->keybit);
	set_bit(KEY_MUTE, cec_dev->cec_info.remote_cec_dev->keybit);
	set_bit(KEY_PLAYPAUSE, cec_dev->cec_info.remote_cec_dev->keybit);
	set_bit(KEY_STOPCD, cec_dev->cec_info.remote_cec_dev->keybit);
	set_bit(KEY_REWIND, cec_dev->cec_info.remote_cec_dev->keybit);
	set_bit(KEY_FASTFORWARD, cec_dev->cec_info.remote_cec_dev->keybit);
	set_bit(KEY_NEXT, cec_dev->cec_info.remote_cec_dev->keybit);
	set_bit(KEY_PREVIOUS, cec_dev->cec_info.remote_cec_dev->keybit);
	set_bit(KEY_NEXTSONG, cec_dev->cec_info.remote_cec_dev->keybit);
	set_bit(KEY_PREVIOUSSONG, cec_dev->cec_info.remote_cec_dev->keybit);
	set_bit(KEY_BACK, cec_dev->cec_info.remote_cec_dev->keybit);

	if (input_register_device(cec_dev->cec_info.remote_cec_dev)) {
		CEC_INFO("Failed to register device\n");
		input_free_device(cec_dev->cec_info.remote_cec_dev);
	}

	/* config: read from dts */
	r = of_property_read_u32(node, "cec_sel", &cec_dev->cec_num);
	if (r) {
		CEC_ERR("not find 'cec_sel'\n");
		cec_dev->cec_num = ENABLE_ONE_CEC;
	}

	/* if using EE CEC */
	if (of_property_read_bool(node, "ee_cec"))
		ee_cec = CEC_B;
	else
		ee_cec = CEC_A;
	CEC_ERR("using cec:%d\n", ee_cec);
	/* pinmux set */
	if (of_get_property(node, "pinctrl-names", NULL)) {
		pin = devm_pinctrl_get(&pdev->dev);
		/*get sleep state*/
		cec_dev->dbg_dev->pins->sleep_state =
			pinctrl_lookup_state(pin, "cec_pin_sleep");
		if (IS_ERR(cec_dev->dbg_dev->pins->sleep_state))
			pr_info("get sleep state error!\n");
		/*get active state*/
		if (ee_cec == CEC_B) {
			cec_dev->dbg_dev->pins->default_state =
				pinctrl_lookup_state(pin, "hdmitx_aocecb");
			if (IS_ERR(cec_dev->dbg_dev->pins->default_state)) {
				pr_info("get aocecb error!\n");
				cec_dev->dbg_dev->pins->default_state =
					pinctrl_lookup_state(pin, "default");
				CEC_ERR("use default cec\n");
				/*force use default*/
				ee_cec = CEC_A;
			}
		} else {
			cec_dev->dbg_dev->pins->default_state =
				pinctrl_lookup_state(pin, "default");
			if (IS_ERR(cec_dev->dbg_dev->pins->default_state))
				pr_info("get default error1!\n");
		}
		/*select pin state*/
		ret = pinctrl_pm_select_default_state(&pdev->dev);
		if (ret > 0)
			pr_info("select state error:0x%x\n", ret);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ao_exit");
	if (res) {
		base = devm_ioremap(&pdev->dev, res->start,
				    res->end - res->start);
		if (!base) {
			CEC_ERR("Unable to map ao_exit base\n");
			goto tag_cec_reg_map_err;
		}
		cec_dev->exit_reg = (void *)base;
	} else {
		dprintk(L_4, "no ao_exit regs\n");
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ao");
	if (res) {
		base = devm_ioremap(&pdev->dev, res->start,
				    res->end - res->start);
		if (!base) {
			CEC_ERR("Unable to map ao base\n");
			goto tag_cec_reg_map_err;
		}
		cec_dev->cec_reg = (void *)base;
	} else {
		CEC_ERR("no ao regs\n");
		goto tag_cec_reg_map_err;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "hdmirx");
	if (res) {
		base = devm_ioremap(&pdev->dev, res->start,
				    res->end - res->start);
		if (!base) {
			CEC_ERR("Unable to map hdmirx base\n");
			goto tag_cec_reg_map_err;
		}
		cec_dev->hdmi_rxreg = (void *)base;
	} else {
		dprintk(L_4, "no hdmirx regs\n");
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "hhi");
	if (res) {
		base = devm_ioremap(&pdev->dev, res->start,
				    res->end - res->start);
		if (!base) {
			CEC_ERR("Unable to map hhi base\n");
			goto tag_cec_reg_map_err;
		}
		cec_dev->hhi_reg = (void *)base;
	} else {
		dprintk(L_4, "no hhi regs\n");
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "periphs");
	if (res) {
		base = devm_ioremap(&pdev->dev, res->start,
				    res->end - res->start);
		if (!base) {
			CEC_ERR("Unable to map periphs base\n");
			goto tag_cec_reg_map_err;
		}
		cec_dev->periphs_reg = (void *)base;
	} else {
		dprintk(L_4, "no periphs regs\n");
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "clock");
	if (res) {
		base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
		if (!base) {
			CEC_ERR("Unable to map clock base\n");
			goto tag_cec_reg_map_err;
		}
		cec_dev->clk_reg = (void *)base;
	} else {
		cec_dev->clk_reg = NULL;
		dprintk(L_4, "no clock regs\n");
	}

	r = of_property_read_u32(node, "port_num", &cec_dev->port_num);
	if (r) {
		CEC_ERR("not find 'port_num'\n");
		cec_dev->port_num = 1;
	}
	r = of_property_read_u32(node, "arc_port_mask", &cec_dev->arc_port);
	if (r) {
		CEC_ERR("not find 'arc_port_mask'\n");
		cec_dev->arc_port = 0;
	}
	r = of_property_read_u32(node, "output", &cec_dev->output);
	if (r) {
		CEC_ERR("not find 'output'\n");
		cec_dev->output = 0;
	}
	vend = &cec_dev->v_data;
	r = of_property_read_string(node, "vendor_name",
				    (const char **)&vend->vendor_name);
	if (r)
		dprintk(L_4, "not find vendor name\n");

	r = of_property_read_u32(node, "vendor_id", &vend->vendor_id);
	if (r)
		dprintk(L_4, "not find vendor id\n");
	cec_dev->cec_info.vendor_id = vend->vendor_id;

	r = of_property_read_string(node, "product_desc",
				    (const char **)&vend->product_desc);
	if (r)
		dprintk(L_4, "not find product desc\n");

	r = of_property_read_string(node, "cec_osd_string",
				    (const char **)&vend->cec_osd_string);
	if (r) {
		dprintk(L_4, "not find cec osd string\n");
		strcpy(vend->cec_osd_string, "AML TV/BOX");
	}
	strncpy(cec_dev->cec_info.osd_name,
		cec_dev->v_data.cec_osd_string, 14);
	r = of_property_read_u32(node, "cec_version",
				 &cec_dev->cec_info.cec_version);
	if (r) {
		/* default set to 2.0 */
		dprintk(L_4, "not find cec_version\n");
		cec_dev->cec_info.cec_version = CEC_VERSION_14A;
	}

	cec_set_clk(pdev);
	/* irq set */
	cec_irq_enable(false);
	/* default enable all function*/
	cec_config(CEC_FUNC_CFG_ALL, 1);
	/* for init */
	cec_pre_init();

	/* cec hw module reset */
	if (cec_dev->cec_num > ENABLE_ONE_CEC) {
		cec_hw_reset(CEC_A);
		cec_hw_reset(CEC_B);
	} else {
		cec_hw_reset(ee_cec);
	}

	if (__of_irq_count(node) > 1) {
		/* need enable two irq */
		cec_dev->irq_cecb = of_irq_get(node, 0);/*cecb int*/
		cec_dev->irq_ceca = of_irq_get(node, 1);/*ceca int*/
	} else {
		cec_dev->irq_cecb = of_irq_get(node, 0);
		cec_dev->irq_ceca = cec_dev->irq_cecb;
	}

	dprintk(0, "%d irq src, a:%d, b:%d\n", __of_irq_count(node),
		   cec_dev->irq_ceca, cec_dev->irq_cecb);
	if (of_get_property(node, "interrupt-names", NULL)) {
		if (of_property_count_strings(node, "interrupt-names") > 1) {
			r = of_property_read_string_index(node,
							  "interrupt-names", 0,
							  &irq_name_b);
			r = of_property_read_string_index(node,
							  "interrupt-names", 1,
							  &irq_name_a);
		} else {
			r = of_property_read_string_index(node,
							  "interrupt-names", 0,
							  &irq_name_a);
			irq_name_b = irq_name_a;
		}

		if (cec_dev->cec_num > ENABLE_ONE_CEC) {
			/* request two int source */
			CEC_ERR("request_irq two irq src\n");
			r = request_irq(cec_dev->irq_ceca, &ceca_isr,
					IRQF_SHARED | IRQF_NO_SUSPEND,
					irq_name_a,
					(void *)cec_dev);
			if (r < 0)
				CEC_INFO("aocec irq request fail\n");

			r = request_irq(cec_dev->irq_cecb, &cecb_isr,
					IRQF_SHARED | IRQF_NO_SUSPEND,
					irq_name_b,
					(void *)cec_dev);
			if (r < 0)
				CEC_INFO("cecb irq request fail\n");
		} else {
			if (!r && ee_cec == CEC_A) {
				r = request_irq(cec_dev->irq_ceca, &ceca_isr,
						IRQF_SHARED | IRQF_NO_SUSPEND,
						irq_name_a,
						(void *)cec_dev);
				if (r < 0)
					CEC_INFO("aocec irq request fail\n");
			}

			if (!r && ee_cec == CEC_B) {
				r = request_irq(cec_dev->irq_cecb, &cecb_isr,
						IRQF_SHARED | IRQF_NO_SUSPEND,
						irq_name_b,
						(void *)cec_dev);
				if (r < 0)
					CEC_INFO("cecb irq request fail\n");
			}
		}
	}

	last_cec_msg = devm_kzalloc(&pdev->dev, sizeof(*last_cec_msg),
				    GFP_KERNEL);
	if (!last_cec_msg) {
		CEC_ERR("allocate last_cec_msg failed\n");
		ret = -ENOMEM;
		goto tag_cec_msg_alloc_err;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	aocec_suspend_handler.level   = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 20;
	aocec_suspend_handler.suspend = aocec_early_suspend;
	aocec_suspend_handler.resume  = aocec_late_resume;
	aocec_suspend_handler.param   = cec_dev;
	register_early_suspend(&aocec_suspend_handler);
#endif

	hrtimer_init(&start_bit_check, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	start_bit_check.function = cec_line_check;
	cec_dev->cec_thread = create_workqueue("cec_work");
	if (!cec_dev->cec_thread) {
		CEC_INFO("create work queue failed\n");
		ret = -EFAULT;
		goto tag_cec_threat_err;
	}
	/* for hdmitx/rx plug uevent report */
	cec_dev->hdmi_plug_wq = alloc_workqueue("cec_hdmi_plug",
						WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
	if (!cec_dev->hdmi_plug_wq) {
		CEC_INFO("create hdmi_plug_wq failed\n");
		ret = -EFAULT;
		goto tag_hdmi_plug_wq_err;
	}
	INIT_DELAYED_WORK(&cec_dev->work_hdmi_plug, cec_hdmi_plug_handler);
	/* for cec rx msg uevent report */
	cec_dev->cec_rx_event_wq = alloc_workqueue("cec_rx_event",
						   WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
	if (!cec_dev->cec_rx_event_wq) {
		CEC_INFO("create cec_rx_event_wq failed\n");
		ret = -EFAULT;
		goto tag_cec_rx_event_wq_err;
	}
	INIT_DELAYED_WORK(&cec_dev->work_cec_rx, cec_rx_uevent_handler);
	#ifdef CEC_FREEZE_WAKE_UP
	/*freeze wakeup init*/
	device_init_wakeup(&pdev->dev, 1);
	/*CEC_INFO("dev init wakeup\n");*/
	if (cec_dev->cec_num > ENABLE_ONE_CEC) {
		dev_pm_set_wake_irq(&pdev->dev, cec_dev->irq_ceca);
		dev_pm_set_wake_irq(&pdev->dev, cec_dev->irq_cecb);
	} else {
		if (ee_cec == CEC_A)
			dev_pm_set_wake_irq(&pdev->dev, cec_dev->irq_ceca);
		else
			dev_pm_set_wake_irq(&pdev->dev, cec_dev->irq_cecb);
	}
	#endif
	INIT_DELAYED_WORK(&cec_dev->cec_work, cec_task);
	queue_delayed_work(cec_dev->cec_thread, &cec_dev->cec_work, 0);
	tasklet_init(&ceca_tasklet, ceca_tasklet_pro,
		     (unsigned long)cec_dev);
#if (defined(CONFIG_AMLOGIC_HDMITX) || defined(CONFIG_AMLOGIC_HDMITX21))
	hdmitx_event_notifier_regist(&hdmitx_notifier_nb);
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_HDMI
	register_cec_callback(hdmirx_notify_callback);
#endif
	#ifdef CEC_MAIL_BOX
	cec_get_wakeup_reason();
	cec_dev->msg_idx = 0;
	cec_dev->msg_num = 0;
	cec_dev->framework_on = 0;
	/* shutdown->resme, need to read stick regs to get info */
	cec_get_wakeup_data();
	/* store and push otp/active source msg from uboot */
	if (cec_dev->wakeup_reason == CEC_WAKEUP) {
		if (cec_dev->cec_wk_otp_msg[0] > 0)
			cec_store_msg_to_buff(cec_dev->cec_wk_otp_msg[0],
					      &cec_dev->cec_wk_otp_msg[1]);
		if (cec_dev->cec_wk_as_msg[0] > 0)
			cec_store_msg_to_buff(cec_dev->cec_wk_as_msg[0],
					      &cec_dev->cec_wk_as_msg[1]);
	}
	#endif
	cec_irq_enable(true);
	/* still check bus by default, maybe not necessary */
	cec_dev->sw_chk_bus = true;
	cec_dev->chk_sig_free_time = false;
	CEC_ERR("%s success end\n", __func__);
	cec_dev->probe_finish = true;
	return 0;

tag_cec_rx_event_wq_err:
	destroy_workqueue(cec_dev->hdmi_plug_wq);
tag_hdmi_plug_wq_err:
	destroy_workqueue(cec_dev->cec_thread);
tag_cec_threat_err:
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&aocec_suspend_handler);
#endif
tag_cec_msg_alloc_err:
	if (cec_dev->cec_num > ENABLE_ONE_CEC) {
		free_irq(cec_dev->irq_ceca, (void *)cec_dev);
		free_irq(cec_dev->irq_cecb, (void *)cec_dev);
	} else {
		if (ee_cec == CEC_B)
			free_irq(cec_dev->irq_cecb, (void *)cec_dev);
		else
			free_irq(cec_dev->irq_ceca, (void *)cec_dev);
	}
tag_cec_reg_map_err:
	input_free_device(cec_dev->cec_info.remote_cec_dev);
tag_cec_alloc_input_err:
	device_destroy(&aocec_class, MKDEV(cec_dev->cec_info.dev_no, 0));
tag_cec_device_create_err:
	unregister_chrdev(cec_dev->cec_info.dev_no, CEC_DEV_NAME);
tag_cec_chr_reg_err:
	class_unregister(&aocec_class);
tag_cec_class_reg:
	devm_kfree(&pdev->dev, cec_dev);
tag_cec_devm_err:
	CEC_ERR("%s fail\n", __func__);
	return ret;
}

static int aml_cec_remove(struct platform_device *pdev)
{
	/*CEC_INFO("%s\n", __func__);*/
	if (cec_dev->cec_num > ENABLE_ONE_CEC) {
		free_irq(cec_dev->irq_ceca, (void *)cec_dev);
		free_irq(cec_dev->irq_cecb, (void *)cec_dev);
	} else {
		if (ee_cec == CEC_B)
			free_irq(cec_dev->irq_cecb, (void *)cec_dev);
		else
			free_irq(cec_dev->irq_ceca, (void *)cec_dev);
	}
	kfree(last_cec_msg);

	if (cec_dev->cec_thread) {
		cancel_delayed_work_sync(&cec_dev->cec_work);
		destroy_workqueue(cec_dev->cec_thread);
	}
#if (defined(CONFIG_AMLOGIC_HDMITX) || defined(CONFIG_AMLOGIC_HDMITX21))
	hdmitx_event_notifier_unregist(&hdmitx_notifier_nb);
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_HDMI
	unregister_cec_callback();
#endif
	if (cec_dev->hdmi_plug_wq) {
		cancel_delayed_work_sync(&cec_dev->work_hdmi_plug);
		destroy_workqueue(cec_dev->hdmi_plug_wq);
	}
	if (cec_dev->cec_rx_event_wq) {
		cancel_delayed_work_sync(&cec_dev->work_cec_rx);
		destroy_workqueue(cec_dev->cec_rx_event_wq);
	}
	input_unregister_device(cec_dev->cec_info.remote_cec_dev);
	unregister_chrdev(cec_dev->cec_info.dev_no, CEC_DEV_NAME);
	class_unregister(&aocec_class);
	kfree(cec_dev);
	return 0;
}

#ifdef CONFIG_PM
static int aml_cec_pm_prepare(struct device *dev)
{
	unsigned int i, j;

	if (IS_ERR_OR_NULL(cec_dev)) {
		pr_info("%s cec_dev is null\n", __func__);
		return 0;
	}
	/*initial msg buffer*/
	/* cec_dev->wakeup_st = 0; */
	cec_dev->msg_idx = 0;
	cec_dev->msg_num = 0;
	for (i = 0; i < CEC_MSG_BUFF_MAX; i++) {
		cec_dev->msgbuff[i].len = 0;
		for (j = 0; j < MAX_MSG; j++)
			cec_dev->msgbuff[i].msg[j] = 0;
	}
	//cec_dev->cec_suspend = CEC_DEEP_SUSPEND;
	/* todo: when to notify uplayer that cec enter suspend */
	cec_set_uevent(CEC_PWR_UEVENT, CEC_SUSPEND);
	CEC_ERR("%s\n", __func__);
	return 0;
}

static void aml_cec_pm_complete(struct device *dev)
{
	if (IS_ERR_OR_NULL(cec_dev))
		return;

	/* cec_dev->wakeup_st = 0; */
	/* cec_dev->msg_idx = 0; */
#ifdef CEC_MAIL_BOX
	cec_get_wakeup_reason();
	if (cec_dev->wakeup_reason == CEC_WAKEUP) {
		cec_key_report(0);
		cec_clear_wakeup_reason();
		/* todo: when to notify linux/android platform
		 * to read wakeup msg
		 */
		if (cec_dev->cec_wk_otp_msg[0] > 0)
			cec_set_uevent(CEC_PWR_UEVENT, CEC_WAKEUP_OTP);
		if (cec_dev->cec_wk_as_msg[0] > 0)
			cec_set_uevent(CEC_PWR_UEVENT, CEC_WAKEUP_AS);
	} else {
		cec_set_uevent(CEC_PWR_UEVENT, CEC_WAKEUP_NORMAL);
	}
#endif
	CEC_ERR("%s\n", __func__);
}

static int aml_cec_suspend_noirq(struct device *dev)
{
	int ret = 0;
	/*unsigned int tempaddr;*/

	cec_dev->cec_info.power_status = CEC_PW_TRANS_ON_TO_STANDBY;
	cec_dev->cec_suspend = CEC_PW_TRANS_ON_TO_STANDBY;

	#ifdef CEC_FREEZE_WAKE_UP
	if (is_pm_s2idle_mode()) {
		CEC_ERR("%s:freeze mode\n", __func__);
		cec_restore_pre_setting();
	} else {
	#endif
	{
		if (cec_dev->cec_num > ENABLE_ONE_CEC)
			cec_clear_all_logical_addr(CEC_B);
		else
			cec_clear_all_logical_addr(ee_cec);

		if (!IS_ERR(cec_dev->dbg_dev->pins->sleep_state))
			ret = pinctrl_pm_select_sleep_state(cec_dev->dbg_dev);
	}
	#ifdef CEC_FREEZE_WAKE_UP
	}
	#endif
	cec_save_mail_box();
	cec_dev->cec_info.power_status = CEC_PW_STANDBY;
	cec_dev->cec_suspend = CEC_PW_STANDBY;
	cec_dev->msg_idx = 0;
	cec_dev->msg_num = 0;
	cec_dev->framework_on = 0;
	CEC_INFO("%s\n", __func__);
	return 0;
}

static void cec_get_wk_msg(void)
{
	int i = 0;

	memset(cec_dev->cec_wk_otp_msg, 0, sizeof(cec_dev->cec_wk_otp_msg));
	scpi_get_cec_wk_msg(SCPI_CMD_GET_CEC_OTP_MSG,
		 cec_dev->cec_wk_otp_msg);
	CEC_INFO("cec_wk_otp_msg len: %x\n", cec_dev->cec_wk_otp_msg[0]);
	for (i = 0; i < cec_dev->cec_wk_otp_msg[0]; i++)
		CEC_INFO("cec_wk_otp_msg[%d] %02x\n", i,
			 cec_dev->cec_wk_otp_msg[i + 1]);
	if (cec_dev->cec_wk_otp_msg[2] == CEC_OC_IMAGE_VIEW_ON ||
	    cec_dev->cec_wk_otp_msg[2] == CEC_OC_TEXT_VIEW_ON) {
		cec_dev->wakup_data.wk_phy_addr = 0xffff;
		cec_dev->wakup_data.wk_logic_addr =
			(cec_dev->cec_wk_otp_msg[1] >> 4) & 0xf;
		cec_dev->wakup_data.wk_port_id = 0xff;
	}

	memset(cec_dev->cec_wk_as_msg, 0, sizeof(cec_dev->cec_wk_as_msg));
	scpi_get_cec_wk_msg(SCPI_CMD_GET_CEC_AS_MSG,
		 cec_dev->cec_wk_as_msg);
	CEC_INFO("cec_wk_as_msg len: %x\n", cec_dev->cec_wk_as_msg[0]);
	for (i = 0; i < cec_dev->cec_wk_as_msg[0]; i++)
		CEC_INFO("cec_wk_as_msg[%d] %02x\n", i,
			 cec_dev->cec_wk_as_msg[i + 1]);
	if (cec_dev->cec_wk_as_msg[2] == CEC_OC_ACTIVE_SOURCE) {
		cec_dev->wakup_data.wk_phy_addr =
			(cec_dev->cec_wk_as_msg[3] << 8) |
			cec_dev->cec_wk_as_msg[4];
		cec_dev->wakup_data.wk_logic_addr =
			(cec_dev->cec_wk_as_msg[1] >> 4) & 0xf;
		cec_dev->wakup_data.wk_port_id =
			cec_get_wk_port_id(cec_dev->wakup_data.wk_phy_addr);
	}
}

static int aml_cec_resume_noirq(struct device *dev)
{
	int ret = 0;

	CEC_ERR("cec resume noirq!\n");

	cec_dev->cec_info.power_status = CEC_PW_TRANS_STANDBY_TO_ON;
	cec_dev->cec_suspend = CEC_PW_TRANS_STANDBY_TO_ON;
	/*initial msg buffer*/
	/* cec_dev->msg_idx = 0; */
	/* cec_dev->msg_num = 0; */

	#ifdef CEC_FREEZE_WAKE_UP
	if (is_pm_s2idle_mode())
		CEC_ERR("is freeze mode\n");
	else
	#endif
	{
		cec_clear_all_logical_addr(ee_cec);
		cec_get_wakeup_reason();
		/* since SC2, it uses uboot2019
		 * for previous chips, uboot2015, still use stick regs
		 */
		if (cec_dev->plat_data->chip_id >= CEC_CHIP_SC2)
			cec_get_wk_msg();
		else
			cec_get_wakeup_data();
		/* disable all logical address */
		/*cec_dev->cec_info.addr_enable = 0;*/
	}
	/* store and push otp/active source msg from uboot */
	if (cec_dev->wakeup_reason == CEC_WAKEUP) {
		if (cec_dev->cec_wk_otp_msg[0] > 0)
			cec_store_msg_to_buff(cec_dev->cec_wk_otp_msg[0],
					      &cec_dev->cec_wk_otp_msg[1]);
		if (cec_dev->cec_wk_as_msg[0] > 0)
			cec_store_msg_to_buff(cec_dev->cec_wk_as_msg[0],
					      &cec_dev->cec_wk_as_msg[1]);
	}
	cec_pre_init();
	if (!IS_ERR(cec_dev->dbg_dev->pins->default_state))
		ret = pinctrl_pm_select_default_state(cec_dev->dbg_dev);
	else
		CEC_ERR("pinctrl default_state error\n");
	cec_irq_enable(true);
	cec_dev->cec_info.power_status = CEC_PW_POWER_ON;
	cec_dev->cec_suspend = CEC_PW_POWER_ON;
	/* cec_dev->wakeup_st = 1; */
	return 0;
}

static const struct dev_pm_ops aml_cec_pm = {
	.prepare  = aml_cec_pm_prepare,
	.complete = aml_cec_pm_complete,
	.suspend_noirq = aml_cec_suspend_noirq,
	.resume_noirq = aml_cec_resume_noirq,
};
#endif

static void aml_cec_shutdown(struct platform_device *pdev)
{
	/*CEC_ERR("%s\n", __func__);*/
	cec_save_mail_box();
	cec_dev->cec_info.power_status = CEC_PW_STANDBY;
	cec_dev->cec_suspend = CEC_PW_STANDBY;
}

static struct platform_driver aml_cec_driver = {
	.driver = {
		.name  = "cectx",
		.owner = THIS_MODULE,
	#ifdef CONFIG_PM
		.pm     = &aml_cec_pm,
	#endif
	#ifdef CONFIG_OF
		.of_match_table = aml_cec_dt_match,
	#endif
	},
	.shutdown = aml_cec_shutdown,
	.probe  = aml_cec_probe,
	.remove = aml_cec_remove,
};

int __init cec_init(void)
{
	int ret;

	ret = platform_driver_register(&aml_cec_driver);
	/*pr_info("cec_init ret:0x%x\n", __func__, ret);*/

	return ret;
}

void __exit cec_uninit(void)
{
	platform_driver_unregister(&aml_cec_driver);
}

