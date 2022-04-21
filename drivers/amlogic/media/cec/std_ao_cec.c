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
#include <media/cec.h>
#include <media/cec-notifier.h>
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_HDMI
#include "../vin/tvin/hdmirx/hdmi_rx_drv_ext.h"
#endif

#define STD_CEC_NAME "std_cec"

int cec_msg_dbg_en;
static unsigned int input_event_en;
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

static unsigned int cec_ver_cnt;
static unsigned int cec_ver_cnt_max = 1;
static unsigned int wait_notify_ms = 100;

struct st_ao_cec {
#ifdef CONFIG_CEC_NOTIFIER
	struct cec_notifier *tx_notify;
#endif
	struct cec_adapter *adap;
	struct cec_msg rx_msg;
	u8 tx_result;
	struct delayed_work work_cec_tx;
	bool adapt_log_addr_valid;
	bool need_rx_uevent;
	/* transmit/receive done notify_work in common queue */
	bool common_queue;
	bool dbg_ret;
};

static struct st_ao_cec std_ao_cec;

static void cec_store_msg_to_buff(unsigned char len, unsigned char *msg);
static void cec_new_msg_push(void);

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
	unsigned int wait_ms = 0;

	intr_cec = cecb_irq_stat();

	/* clear irq */
	if (intr_cec != 0)
		cecb_clear_irq(intr_cec);
	else
		dprintk(L_2, "err cec intsts:0\n");

	dprintk(L_2, "cecb intsts:0x%x\n", intr_cec);
	if (cec_dev->plat_data->ee_to_ao)
		shift = 16;
	if (cec_ver_cnt == cec_ver_cnt_max) {
		wait_ms = wait_notify_ms;
		CEC_INFO("%s wait: %dms\n", __func__, wait_notify_ms);
	}

	/* TX DONE irq, increase tx buffer pointer */
	if (intr_cec == CEC_IRQ_TX_DONE) {
		cec_tx_result = CEC_FAIL_NONE;
		std_ao_cec.tx_result = CEC_TX_STATUS_OK;
		dprintk(L_2, "irqflg:TX_DONE\n");
		if (std_ao_cec.common_queue)
			queue_delayed_work(cec_dev->cec_rx_event_wq,
					   &std_ao_cec.work_cec_tx,
					   msecs_to_jiffies(wait_ms));
		else
			queue_delayed_work(cec_dev->cec_tx_event_wq,
					   &std_ao_cec.work_cec_tx,
					   msecs_to_jiffies(wait_ms));
		complete(&cec_dev->tx_ok);
	}

	/* TX error irq flags */
	if ((intr_cec & CEC_IRQ_TX_NACK)     ||
	    (intr_cec & CEC_IRQ_TX_ARB_LOST) ||
	    (intr_cec & CEC_IRQ_TX_ERR_INITIATOR)) {
		if (intr_cec & CEC_IRQ_TX_NACK) {
			cec_tx_result = CEC_FAIL_NACK;
			dprintk(L_2, "irqflg:TX_NACK\n");
			std_ao_cec.tx_result = CEC_TX_STATUS_NACK;
		} else if (intr_cec & CEC_IRQ_TX_ARB_LOST) {
			cec_tx_result = CEC_FAIL_BUSY;
			/* clear start */
			hdmirx_cec_write(DWC_CEC_TX_CNT, 0);
			hdmirx_set_bits_dwc(DWC_CEC_CTRL, 0, 0, 3);
			dprintk(L_2, "irqflg:ARB_LOST\n");
			std_ao_cec.tx_result = CEC_TX_STATUS_ARB_LOST;
		} else if (intr_cec & CEC_IRQ_TX_ERR_INITIATOR) {
			dprintk(L_2, "irqflg:INITIATOR\n");
			cec_tx_result = CEC_FAIL_OTHER;
			std_ao_cec.tx_result = CEC_TX_STATUS_ERROR;
			if (std_ao_cec.dbg_ret)
				std_ao_cec.tx_result =
					CEC_TX_STATUS_MAX_RETRIES | CEC_TX_STATUS_ERROR;
		} else {
			dprintk(L_2, "irqflg:Other\n");
			cec_tx_result = CEC_FAIL_OTHER;
			std_ao_cec.tx_result = CEC_TX_STATUS_ERROR;
		}
		if (std_ao_cec.common_queue)
			queue_delayed_work(cec_dev->cec_rx_event_wq,
					   &std_ao_cec.work_cec_tx,
					   msecs_to_jiffies(wait_ms));
		else
			queue_delayed_work(cec_dev->cec_tx_event_wq,
					   &std_ao_cec.work_cec_tx,
					   msecs_to_jiffies(wait_ms));
		complete(&cec_dev->tx_ok);
	}
	/* The CEC driver should always process the transmit
	 * interrupts first before handling the receive
	 * interrupt. The framework expects to see the
	 * cec_transmit_done call before the cec_received_msg
	 * call, otherwise it can get confused if the received
	 * message was in reply to the transmitted message.
	 */
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
	std_ao_cec.rx_msg.len = *len;
	if (std_ao_cec.rx_msg.len <= CEC_MAX_MSG_SIZE)
		memcpy(std_ao_cec.rx_msg.msg, msg, std_ao_cec.rx_msg.len);

	return ret;
}

static void ceca_tx_irq_handle(void)
{
	unsigned int tx_status = aocec_rd_reg(CEC_TX_MSG_STATUS);
	unsigned int wait_ms = 0;

	if (cec_ver_cnt == cec_ver_cnt_max) {
		wait_ms = wait_notify_ms;
		CEC_INFO("%s wait: %dms\n", __func__, wait_notify_ms);
	}

	cec_tx_result = -1;
	switch (tx_status) {
	case TX_DONE:
		aocec_wr_reg(CEC_TX_MSG_CMD, TX_NO_OP);
		cec_tx_result = CEC_FAIL_NONE;
		std_ao_cec.tx_result = CEC_TX_STATUS_OK;
		if (std_ao_cec.common_queue)
			queue_delayed_work(cec_dev->cec_rx_event_wq,
					   &std_ao_cec.work_cec_tx,
					   msecs_to_jiffies(wait_ms));
		else
			queue_delayed_work(cec_dev->cec_tx_event_wq,
					   &std_ao_cec.work_cec_tx,
					   msecs_to_jiffies(wait_ms));
		break;

	case TX_BUSY:
		CEC_ERR("TX_BUSY\n");
		cec_tx_result = CEC_FAIL_BUSY;
		std_ao_cec.tx_result = CEC_TX_STATUS_ARB_LOST;
		if (std_ao_cec.common_queue)
			queue_delayed_work(cec_dev->cec_rx_event_wq,
					   &std_ao_cec.work_cec_tx,
					   msecs_to_jiffies(wait_ms));
		else
			queue_delayed_work(cec_dev->cec_tx_event_wq,
					   &std_ao_cec.work_cec_tx,
					   msecs_to_jiffies(wait_ms));
		break;

	case TX_ERROR:
		CEC_INFO("TX ERROR!\n");
		aocec_wr_reg(CEC_TX_MSG_CMD, TX_ABORT);
		ceca_hw_reset();
		if (cec_dev->cec_num <= ENABLE_ONE_CEC)
			cec_restore_logical_addr(CEC_A,
						 cec_dev->cec_info.addr_enable);
		cec_tx_result = CEC_FAIL_NACK;
		std_ao_cec.tx_result = CEC_TX_STATUS_NACK;
		if (std_ao_cec.common_queue)
			queue_delayed_work(cec_dev->cec_rx_event_wq,
					   &std_ao_cec.work_cec_tx,
					   msecs_to_jiffies(wait_ms));
		else
			queue_delayed_work(cec_dev->cec_tx_event_wq,
					   &std_ao_cec.work_cec_tx,
					   msecs_to_jiffies(wait_ms));
		break;

	case TX_IDLE:
		CEC_ERR("TX_IDLE\n");
		cec_tx_result = CEC_FAIL_OTHER;
		std_ao_cec.tx_result = CEC_TX_STATUS_ERROR;
		if (std_ao_cec.common_queue)
			queue_delayed_work(cec_dev->cec_rx_event_wq,
					   &std_ao_cec.work_cec_tx,
					   msecs_to_jiffies(wait_ms));
		else
			queue_delayed_work(cec_dev->cec_tx_event_wq,
					   &std_ao_cec.work_cec_tx,
					   msecs_to_jiffies(wait_ms));
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
	if (cec_dev->sw_chk_bus) {
		if (check_confilct()) {
			CEC_ERR("bus confilct too long\n");
			mutex_unlock(&cec_dev->cec_tx_mutex);
			return CEC_FAIL_BUSY;
		}
	}
	/* for std cec, driver never retry itself */
	retry = 0;
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
		new_msg = 1;
		/* will notify by stored_msg_push */
		/* if (cec_dev->framework_on && cec_need_store_msg_to_buff()) */
			/* return; */
		/* uevent to notify cec msg received */
		queue_delayed_work(cec_dev->cec_rx_event_wq,
				   &cec_dev->work_cec_rx, 0);
		complete(&cec_dev->rx_ok);
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
			if (cec_ll_tx(&p, 1, CEC_SIGNAL_FREE_TIME_NEW_INITIATOR) == CEC_FAIL_NONE)
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
	cec_config2_phyaddr(cec_dev->phy_addr, 1);
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
	ret = cec_ll_tx(buf, cnt, CEC_SIGNAL_FREE_TIME_NEW_INITIATOR);
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
		cecb_trigle_tx(buf, cnt, 5);

	return count;
}

/* extracted wakeup info for TV */
static ssize_t wake_up_show(struct class *cla,
			    struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", *((unsigned int *)&cec_dev->wakup_data));
}

/* wakeup reason, 0x8 means wakeup by CEC msg */
static ssize_t wake_up_reason_show(struct class *cla,
			    struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", cec_dev->wakeup_reason);
}

/* primary one touch play message of wakeup */
static ssize_t wake_up_otp_show(struct class *cla,
			    struct class_attribute *attr, char *buf)
{
	unsigned char i = 0;
	int pos = 0;

	for (i = 0; i < cec_dev->cec_wk_otp_msg[0]; i++)
		pos += snprintf(buf + pos, PAGE_SIZE,
			"%02x\n", cec_dev->cec_wk_otp_msg[i + 1]);
	return pos;
}

/* primary active source msg of wakeup */
static ssize_t wake_up_as_show(struct class *cla,
			    struct class_attribute *attr, char *buf)
{
	unsigned char i = 0;
	int pos = 0;

	for (i = 0; i < cec_dev->cec_wk_as_msg[0]; i++)
		pos += snprintf(buf + pos, PAGE_SIZE,
			"%02x\n", cec_dev->cec_wk_as_msg[i + 1]);
	return pos;
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
	} else if (token && strncmp(token, "input_en", 8) == 0) {
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;
		input_event_en = addr;
	} else if (token && strncmp(token, "chk_sig_free", 12) == 0) {
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 10, &addr) < 0)
			return count;
		cec_dev->chk_sig_free_time = !!addr;
		CEC_ERR("check signal free time enable: %d\n", cec_dev->chk_sig_free_time);
	} else if (token && strncmp(token, "sw_chk_bus", 10) == 0) {
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 10, &addr) < 0)
			return count;
		cec_dev->sw_chk_bus = !!addr;
		CEC_ERR("sw_chk_bus enable: %d\n", cec_dev->sw_chk_bus);
	} else if (token && strncmp(token, "cec_ver_cnt_max", 15) == 0) {
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 10, &addr) < 0)
			return count;
		cec_ver_cnt_max = addr;
		CEC_ERR("cec_ver_cnt_max: %d\n", cec_ver_cnt_max);
	} else if (token && strncmp(token, "wait_notify_ms", 14) == 0) {
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 10, &addr) < 0)
			return count;
		wait_notify_ms = addr;
		CEC_ERR("wait_notify_ms: %d\n", wait_notify_ms);
	} else if (token && strncmp(token, "need_rx_uevent", 14) == 0) {
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 10, &addr) < 0)
			return count;
		std_ao_cec.need_rx_uevent = !!addr;
		CEC_ERR("need_rx_uevent: %d\n", std_ao_cec.need_rx_uevent);
	}  else if (token && strncmp(token, "common_queue", 12) == 0) {
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 10, &addr) < 0)
			return count;
		std_ao_cec.common_queue = !!addr;
		CEC_ERR("common_queue: %d\n", std_ao_cec.common_queue);
	} else if (token && strncmp(token, "dbg_ret", 7) == 0) {
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 10, &addr) < 0)
			return count;
		std_ao_cec.dbg_ret = !!addr;
		CEC_ERR("dbg_ret: %d\n", std_ao_cec.dbg_ret);
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
static CLASS_ATTR_RO(wake_up_reason);
static CLASS_ATTR_RO(wake_up_otp);
static CLASS_ATTR_RO(wake_up_as);
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
	&class_attr_wake_up_reason.attr,
	&class_attr_wake_up_otp.attr,
	&class_attr_wake_up_as.attr,
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
	/* not notify cec core before log addr valid
	 * even when receive broadcast msg
	 */
	if (!std_ao_cec.adapt_log_addr_valid)
		return;

	/* actually no need for std linux cec, for debug */
	if (std_ao_cec.need_rx_uevent) {
		/* notify framework to read cec msg */
		cec_set_uevent(CEC_RX_MSG, 1);
		/* clear notify */
		cec_set_uevent(CEC_RX_MSG, 0);
	}
	std_ao_cec.rx_msg.len = rx_len;
	if (std_ao_cec.rx_msg.len <= CEC_MAX_MSG_SIZE)
		memcpy(std_ao_cec.rx_msg.msg, rx_msg, std_ao_cec.rx_msg.len);
	cec_received_msg(std_ao_cec.adap, &std_ao_cec.rx_msg);
}

/* --------For CEC framework interface-------- */

/* This callback enables or disables the CEC hardware.
 * Enabling the CEC hardware means powering it up in
 * a state where no logical addresses are claimed.
 * This op assumes that the physical address
 * (adap->phys_addr) is valid when enable is true
 * and will not change while the CEC adapter remains
 * enabled. The initial state of the CEC adapter
 * after calling cec_allocate_adapter() is disabled.
 */
static int ao_cec_adap_enable(struct cec_adapter *adap, bool enable)
{
	unsigned int tmp;

	CEC_INFO("%s enable: %d\n", __func__, enable);

	if (!enable) {
		/* disable CEC HW */
		return 0;
	}
	/* store phy addr in reg, witch will be used under suspend;
	 * calc it as it can't be transferred from cec core
	 */
	tmp = cec_get_cur_phy_addr();
	if (cec_dev->dev_type != CEC_TV_ADDR && tmp != 0 &&
	    tmp != 0xffff) {
		cec_dev->phy_addr = tmp;
	} else {
		CEC_INFO("%s dev_type %lu, tmp_phy_addr: 0x%x\n",
			 __func__, cec_dev->dev_type, tmp);
		cec_dev->phy_addr = 0;
	}
	cec_config2_phyaddr(cec_dev->phy_addr, 1);
	cec_hw_init();
	return 0;
}

/* if logical_addr == CEC_LOG_ADDR_INVALID then
 * all programmed logical addresses are to be erased.
 * Otherwise the given logical address should be
 * programmed. If the maximum number of available
 * logical addresses is exceeded, then it should
 * return -ENXIO. Once a logical address is programmed
 * the CEC hardware can receive directed messages
 * to that address. Note that adap_log_addr must
 * return 0 if logical_addr is CEC_LOG_ADDR_INVALID.
 */

static int ao_cec_set_log_addr(struct cec_adapter *adap, u8 logical_addr)
{
	unsigned char cur_log_addr_num = 0;
	unsigned char i = 0;

	CEC_INFO("%s enable: %x\n", __func__, logical_addr);
	for (i = 0; i < 16; i++) {
		if (((cec_dev->cec_info.addr_enable >> i) & 0x1) == 0x1)
			cur_log_addr_num++;
	}
	if (logical_addr == CEC_LOG_ADDR_INVALID) {
		cec_ap_clear_logical_addr();
		std_ao_cec.adapt_log_addr_valid = false;
		return 0;
	} else if (cur_log_addr_num > MAX_LOG_ADDR_CNT) {
		/* log_addr_num > maximum number of available */
		CEC_ERR("exceed the maximum supportted log addr\n");
		return -ENXIO;
	}
	if (logical_addr == 0) {
		/* clear phy addr if it's TV ? */
		/* cec_dev->phy_addr = 0; */
		/* cec_config2_phyaddr(0, 1); */
		cec_dev->dev_type = CEC_TV_ADDR;
	} else if (logical_addr == CEC_PLAYBACK_DEVICE_1_ADDR ||
		logical_addr == CEC_PLAYBACK_DEVICE_2_ADDR ||
		logical_addr == CEC_PLAYBACK_DEVICE_3_ADDR) {
		cec_dev->dev_type = CEC_PLAYBACK_DEVICE_1_ADDR;
	} else {
		cec_dev->dev_type = CEC_PLAYBACK_DEVICE_1_ADDR;
		CEC_ERR("cec_adapter logic_addr abnormal\n");
	}
	cec_ap_add_logical_addr(logical_addr);
	cec_config2_devtype(cec_dev->dev_type, 1);
	std_ao_cec.adapt_log_addr_valid = true;
	return 0;
}

static void ao_cec_tx_work(struct work_struct *work)
{
	struct st_ao_cec *ao_cec = container_of((struct delayed_work *)work,
			struct st_ao_cec, work_cec_tx);

	cec_transmit_attempt_done(ao_cec->adap, ao_cec->tx_result);
}

/* The attempts argument is the suggested number of
 * attempts for the transmit. The signal_free_time
 * is the number of data bit periods that the adapter
 * should wait when the line is free before attempting
 * to send a message. This value depends on whether
 * this transmit is a retry, a message from a new
 * initiator or a new message for the same initiator.
 * Most hardware will handle this automatically,
 * but in some cases this information is needed.
 * The CEC_FREE_TIME_TO_USEC macro can be used to
 * convert signal_free_time to microseconds
 * (one data bit period is 2.4 ms)
 */
static int ao_cec_transmit(struct cec_adapter *adap, u8 attempts,
				 u32 signal_free_time, struct cec_msg *msg)
{
	if (!msg)
		return -1;
	dprintk(L_2, "%s signal_free_time: %x, attempts: %d\n",
		__func__, signal_free_time, attempts);
	if (is_get_cec_ver_msg(msg->msg, msg->len))
		cec_ver_cnt++;
	else
		cec_ver_cnt = 0;
	cec_ll_tx(msg->msg, msg->len, signal_free_time);

	return 0;
}

/* This optional callback can be used to show the status
 * of the CEC hardware. The status is available through
 * debugfs: cat /sys/kernel/debug/cec/cecX/status
 */
void aml_adap_status(struct cec_adapter *adap, struct seq_file *file)
{
	cec_status();
	CEC_ERR("cec_ver_cnt: %d\n", cec_ver_cnt);
	CEC_ERR("cec_ver_cnt_max: %d\n", cec_ver_cnt_max);
	CEC_ERR("wait_notify_ms: %d\n", wait_notify_ms);
	CEC_ERR("common_queue: %d\n", std_ao_cec.common_queue);
	CEC_ERR("adapt_log_addr_valid: %d\n", std_ao_cec.adapt_log_addr_valid);
}

/* To free any resources when the adapter is deleted,
 * This optional callback can be used to free any
 * resources that might have been allocated by
 * the driver. It's called from cec_delete_adapter.
 */
void aml_adap_free(struct cec_adapter *adap)
{
}

static const struct cec_adap_ops ao_cec_ops = {
	/* Low-level callbacks */
	.adap_enable = ao_cec_adap_enable,
	.adap_log_addr = ao_cec_set_log_addr,
	.adap_transmit = ao_cec_transmit,
	.adap_status = aml_adap_status,
	.adap_free = aml_adap_free,

	/* Error injection callbacks */
	/* High-level CEC message callback */

	/* If the driver wants to process a CEC message,
	 * then it can implement this callback. If it
	 * doesn't want to handle this message, then it
	 * should return -ENOMSG, otherwise the CEC
	 * framework assumes it processed this message
	 * and it will not do anything with it.
	 */
	/* int (*received)(struct cec_adapter *adap, struct cec_msg *msg); */
};

/* --------end of CEC framework interface-------- */

static int aml_aocec_probe(struct platform_device *pdev)
{
	int ret = 0;
	const struct of_device_id *of_id;
#ifdef CONFIG_CEC_NOTIFIER
#ifdef CONFIG_DRM_MESON_HDMI
	struct device *hdmi_tx_dev;
#endif
#endif
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
	unsigned char a, b, c, d;
	u16 phy_addr = 0;
	struct vsdb_phyaddr *tx_phy_addr = get_hdmitx_phy_addr();
	unsigned int is_ee_cec;

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
	r = of_property_read_u32(node, "ee_cec", &is_ee_cec);
	if (r) {
		CEC_ERR("not find 'ee_cec'\n");
		ee_cec = CEC_A;
	} else {
		ee_cec = !!is_ee_cec;
	}
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
	r = of_property_read_u32(node, "port_seq", &cec_dev->port_seq);
	if (r) {
		cec_dev->port_seq = 0xf321;
		CEC_ERR("not find 'port_seq', use default: 0x%x\n",
			cec_dev->port_seq);
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
						WQ_HIGHPRI |
						WQ_CPU_INTENSIVE, 0);
	if (!cec_dev->hdmi_plug_wq) {
		CEC_INFO("create hdmi_plug_wq failed\n");
		ret = -EFAULT;
		goto tag_hdmi_plug_wq_err;
	}
	INIT_DELAYED_WORK(&cec_dev->work_hdmi_plug, cec_hdmi_plug_handler);
	/* for cec rx msg uevent report */
	cec_dev->cec_rx_event_wq = alloc_workqueue("cec_rx_event",
						   WQ_HIGHPRI |
						   WQ_CPU_INTENSIVE, 0);
	if (!cec_dev->cec_rx_event_wq) {
		CEC_INFO("create cec_rx_event_wq failed\n");
		ret = -EFAULT;
		goto tag_cec_rx_event_wq_err;
	}
	INIT_DELAYED_WORK(&cec_dev->work_cec_rx, cec_rx_uevent_handler);
	/* notify cec framework tx done, note that need
	 * to use dedicated high priority workqueue,
	 * as cec-compliance test will check the roundtrip
	 * time of msg, so need to return tx result asap.
	 */
	cec_dev->cec_tx_event_wq = alloc_workqueue("cec_tx_event",
						   WQ_HIGHPRI |
						   WQ_CPU_INTENSIVE, 0);
	if (!cec_dev->cec_tx_event_wq) {
		CEC_INFO("create cec_tx_event_wq failed\n");
		ret = -EFAULT;
		goto tag_cec_tx_event_wq_err;
	}
	INIT_DELAYED_WORK(&std_ao_cec.work_cec_tx, ao_cec_tx_work);
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
	/* not check signal free time by default
	 * otherwise it eazily fail at
	 * utils/cec-compliance/cec-test-adapter.cpp(1224):
	 * There were 87 messages in the receive queue for 68 transmits
	 * intsts:0x11 and irqflg:INITIATOR
	 */
	cec_dev->chk_sig_free_time = false;
	cec_dev->sw_chk_bus = false;
	std_ao_cec.adapt_log_addr_valid = false;
	/* std linux cec not need uevent, for back */
	std_ao_cec.need_rx_uevent = false;
	/* tx/rx done notify queued in sequence if
	 * set to true, for debug;
	 */
	std_ao_cec.common_queue = false;
	/* Must be 1 <= available_las <= CEC_MAX_LOG_ADDRS. */
	/* std_ao_cec is priv data */
	std_ao_cec.adap = cec_allocate_adapter(&ao_cec_ops, &std_ao_cec,
						STD_CEC_NAME,
						CEC_CAP_DEFAULTS |
						/* CEC_CAP_CONNECTOR_INFO | */
						CEC_CAP_PHYS_ADDR,
						MAX_LOG_ADDR_CNT);
	if (IS_ERR(std_ao_cec.adap)) {
		ret = -EFAULT;
		pr_info("%s cec_allocate_adapter fail\n", __func__);
		goto tag_cec_allocate_adapter_fail;
	}
	std_ao_cec.adap->owner = THIS_MODULE;

	/* register the /dev/cecX device node and the remote control device */
	ret = cec_register_adapter(std_ao_cec.adap, &pdev->dev);
	if (ret < 0) {
		/* if cec_register_adapter() fails,
		 * then call cec_delete_adapter() to clean up
		 */
		cec_delete_adapter(std_ao_cec.adap);
		ret = -EFAULT;
		pr_info("%s cec_register_adapter fail\n", __func__);
		goto tag_cec_allocate_adapter_fail;
	}
	/* find or create a new cec_notifier for the given device.
	 * if notifier register fail, continue without notifier
	 */
#ifdef CONFIG_CEC_NOTIFIER
#ifdef CONFIG_DRM_MESON_HDMI
	hdmi_tx_dev = cec_notifier_parse_hdmi_phandle(&pdev->dev);
	if (IS_ERR(hdmi_tx_dev)) {
		pr_info("%s cec_notifier_parse_hdmi_phandle fail\n", __func__);
		/* goto register_notifier_fail; */
	} else {
		std_ao_cec.tx_notify =
			cec_notifier_cec_adap_register(hdmi_tx_dev,
						       "hdmitx",
						       std_ao_cec.adap);
		if (!std_ao_cec.tx_notify) {
			pr_info("cec_notifier_cec_adap_register fail\n");
			/* goto register_notifier_fail; */
		}
	}
#endif
#endif
	if (get_hpd_state() &&
	    tx_phy_addr &&
	    tx_phy_addr->valid == 1) {
		a = tx_phy_addr->a;
		b = tx_phy_addr->b;
		c = tx_phy_addr->c;
		d = tx_phy_addr->d;
		phy_addr = ((a << 12) | (b << 8) | (c << 4) | (d));
		cec_s_phys_addr(std_ao_cec.adap,
				phy_addr,
				false);
	}
	CEC_ERR("%s success end\n", __func__);
	cec_dev->probe_finish = true;
	return 0;
#ifdef CONFIG_CEC_NOTIFIER
#ifdef CONFIG_DRM_MESON_HDMI
/* register_notifier_fail: */
	/* cec_unregister_adapter(std_ao_cec.adap); */
#endif
#endif
tag_cec_allocate_adapter_fail:
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_HDMI
	unregister_cec_callback();
#endif
#if (defined(CONFIG_AMLOGIC_HDMITX) || defined(CONFIG_AMLOGIC_HDMITX21))
	hdmitx_event_notifier_unregist(&hdmitx_notifier_nb);
#endif
	destroy_workqueue(cec_dev->cec_tx_event_wq);
tag_cec_tx_event_wq_err:
	destroy_workqueue(cec_dev->cec_rx_event_wq);
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
	class_unregister(&aocec_class);
tag_cec_alloc_input_err:
tag_cec_class_reg:
	devm_kfree(&pdev->dev, cec_dev);
tag_cec_devm_err:
	pr_info("%s fail\n", __func__);
	return ret;
}

static int aml_aocec_remove(struct platform_device *pdev)
{
#ifdef CONFIG_CEC_NOTIFIER
#ifdef CONFIG_DRM_MESON_HDMI
	cec_notifier_cec_adap_unregister(std_ao_cec.tx_notify);
#endif
#endif
	/* Note: if cec_register_adapter() fails, then call cec_delete_adapter()
	 * to clean up. But if cec_register_adapter() succeeded, then only call
	 * cec_unregister_adapter() to clean up, never cec_delete_adapter(). The
	 * unregister function will delete the adapter automatically once the
	 * last user of that /dev/cecX device has closed its file handle.
	 */
	if (std_ao_cec.adap)
		cec_unregister_adapter(std_ao_cec.adap);
	CEC_INFO("%s\n", __func__);
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
	if (cec_dev->cec_tx_event_wq) {
		cancel_delayed_work_sync(&std_ao_cec.work_cec_tx);
		destroy_workqueue(cec_dev->cec_tx_event_wq);
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
		if (input_event_en != 0)
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

static void aml_aocec_shutdown(struct platform_device *pdev)
{
	/*CEC_ERR("%s\n", __func__);*/
	cec_save_mail_box();
	cec_dev->cec_info.power_status = CEC_PW_STANDBY;
	cec_dev->cec_suspend = CEC_PW_STANDBY;
}

static struct platform_driver aml_aocec_driver = {
	.driver = {
		.name  = "ao_cec_drv",
		.owner = THIS_MODULE,
	#ifdef CONFIG_PM
		.pm     = &aml_cec_pm,
	#endif
	#ifdef CONFIG_OF
		.of_match_table = aml_cec_dt_match,
	#endif
	},
	.shutdown = aml_aocec_shutdown,
	.probe  = aml_aocec_probe,
	.remove = aml_aocec_remove,
};

int __init cec_init(void)
{
	int ret;

	ret = platform_driver_register(&aml_aocec_driver);

	return ret;
}

void __exit cec_uninit(void)
{
	platform_driver_unregister(&aml_aocec_driver);
}

