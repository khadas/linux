/*
 * drivers/amlogic/cec/hdmi_ao_cec.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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
#include <asm/irq.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/timer.h>

#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/amlogic/tvin/tvin.h>
#include <linux/wakelock_android.h>

#include <linux/amlogic/hdmi_tx/hdmi_tx_cec_20.h>
#include <linux/amlogic/hdmi_tx/hdmi_tx_module.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_irq.h>
#include "hdmi_ao_cec.h"
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/amlogic/pm.h>
#include <linux/of_address.h>


#define CEC_FRAME_DELAY		msecs_to_jiffies(400)
#define CEC_DEV_NAME		"cec"

#define DEV_TYPE_TX		4
#define DEV_TYPE_RX		0



/* global struct for tx and rx */
struct ao_cec_dev {
	unsigned long dev_type;
	unsigned int port_num;
	unsigned int arc_port;
	unsigned long irq_cec;
	void __iomem *exit_reg;
	void __iomem *cec_reg;
	void __iomem *hdmi_rxreg;
	struct hdmitx_dev *tx_dev;
	struct workqueue_struct *cec_thread;
	struct device *dbg_dev;
	struct delayed_work cec_work;
	struct completion rx_ok;
	struct completion tx_ok;
	spinlock_t cec_reg_lock;
	struct mutex cec_mutex;
#ifdef CONFIG_PM
	int cec_suspend;
#endif
	struct vendor_info_data v_data;
	struct cec_global_info_t cec_info;
};

static struct ao_cec_dev *cec_dev;

static unsigned char rx_msg[MAX_MSG];
static unsigned char rx_len;
bool cec_msg_dbg_en = 0;

#define CEC_ERR(format, args...)				\
	{if (cec_dev->dbg_dev)					\
		dev_err(cec_dev->dbg_dev, format, ##args);	\
	}

#define CEC_INFO(format, args...)				\
	{if (cec_msg_dbg_en && cec_dev->dbg_dev)		\
		dev_info(cec_dev->dbg_dev, format, ##args);	\
	}

static unsigned char msg_log_buf[128] = { 0 };
static int cec_ll_trigle_tx(void);
static int cec_tx_fail_reason;

struct cec_tx_msg_t {
	unsigned char buf[16];
	unsigned int  retry;
	unsigned int  busy;
	unsigned int  len;
};

#define CEX_TX_MSG_BUF_NUM	16
#define CEC_TX_MSG_BUF_MASK	(CEX_TX_MSG_BUF_NUM - 1)

struct cec_tx_msg {
	struct cec_tx_msg_t msg[CEX_TX_MSG_BUF_NUM];
	unsigned int send_idx;
	unsigned int queue_idx;
};

static struct cec_tx_msg cec_tx_msgs = {};

#define waiting_aocec_free() \
	do {\
		unsigned long cnt = 0;\
		while (readl(cec_dev->cec_reg + AO_CEC_RW_REG) & (1<<23)) {\
			if (3500 == cnt++) { \
				pr_info("waiting aocec free time out.\n");\
				break;\
			} \
		} \
	} while (0)

void cec_set_reg_bits(unsigned int addr, unsigned int value,
	unsigned int offset, unsigned int len)
{
	unsigned int data32 = 0;

	data32 = readl(cec_dev->cec_reg + addr);
	data32 &= ~(((1 << len) - 1) << offset);
	data32 |= (value & ((1 << len) - 1)) << offset;
	writel(data32, cec_dev->cec_reg + addr);
}

unsigned int aocec_rd_reg(unsigned long addr)
{
	unsigned int data32;
	unsigned long flags;
	waiting_aocec_free();
	spin_lock_irqsave(&cec_dev->cec_reg_lock, flags);
	data32 = 0;
	data32 |= 0 << 16; /* [16]	 cec_reg_wr */
	data32 |= 0 << 8; /* [15:8]   cec_reg_wrdata */
	data32 |= addr << 0; /* [7:0]	cec_reg_addr */
	writel(data32, cec_dev->cec_reg + AO_CEC_RW_REG);

	waiting_aocec_free();
	data32 = ((readl(cec_dev->cec_reg + AO_CEC_RW_REG)) >> 24) & 0xff;
	spin_unlock_irqrestore(&cec_dev->cec_reg_lock, flags);
	return data32;
} /* aocec_rd_reg */

void aocec_wr_reg(unsigned long addr, unsigned long data)
{
	unsigned long data32;
	unsigned long flags;
	waiting_aocec_free();
	spin_lock_irqsave(&cec_dev->cec_reg_lock, flags);
	data32 = 0;
	data32 |= 1 << 16; /* [16]	 cec_reg_wr */
	data32 |= data << 8; /* [15:8]   cec_reg_wrdata */
	data32 |= addr << 0; /* [7:0]	cec_reg_addr */
	writel(data32, cec_dev->cec_reg + AO_CEC_RW_REG);
	spin_unlock_irqrestore(&cec_dev->cec_reg_lock, flags);
} /* aocec_wr_only_reg */

static void cec_enable_irq(void)
{
	cec_set_reg_bits(AO_CEC_INTR_MASKN, 0x6, 0, 3);
	CEC_INFO("enable:int mask:0x%x\n",
		 readl(cec_dev->cec_reg + AO_CEC_INTR_MASKN));
}

static void cec_hw_buf_clear(void)
{
	aocec_wr_reg(CEC_RX_MSG_CMD, RX_DISABLE);
	aocec_wr_reg(CEC_TX_MSG_CMD, TX_ABORT);
	aocec_wr_reg(CEC_RX_CLEAR_BUF, 1);
	aocec_wr_reg(CEC_TX_CLEAR_BUF, 1);
	udelay(100);
	aocec_wr_reg(CEC_RX_CLEAR_BUF, 0);
	aocec_wr_reg(CEC_TX_CLEAR_BUF, 0);
	udelay(100);
	aocec_wr_reg(CEC_RX_MSG_CMD, RX_NO_OP);
	aocec_wr_reg(CEC_TX_MSG_CMD, TX_NO_OP);
}

void cec_logicaddr_set(int logicaddr)
{
	aocec_wr_reg(CEC_LOGICAL_ADDR0, 0);
	cec_hw_buf_clear();
	aocec_wr_reg(CEC_LOGICAL_ADDR0, (logicaddr & 0xf));
	udelay(100);
	aocec_wr_reg(CEC_LOGICAL_ADDR0, (0x1 << 4) | (logicaddr & 0xf));
	if (cec_msg_dbg_en)
		CEC_INFO("set logical addr:0x%x\n",
			aocec_rd_reg(CEC_LOGICAL_ADDR0));
}

static void cec_hw_reset(void)
{
	writel(0x1, cec_dev->cec_reg + AO_CEC_GEN_CNTL);
	/* Enable gated clock (Normal mode). */
	cec_set_reg_bits(AO_CEC_GEN_CNTL, 1, 1, 1);
	/* Release SW reset */
	udelay(100);
	cec_set_reg_bits(AO_CEC_GEN_CNTL, 0, 0, 1);

	/* Enable all AO_CEC interrupt sources */
	cec_set_reg_bits(AO_CEC_INTR_MASKN, 0x6, 0, 3);

	cec_logicaddr_set(cec_dev->cec_info.log_addr);

	/* Cec arbitration 3/5/7 bit time set. */
	cec_arbit_bit_time_set(3, 0x118, 0);
	cec_arbit_bit_time_set(5, 0x000, 0);
	cec_arbit_bit_time_set(7, 0x2aa, 0);

	memset(&cec_tx_msgs, 0, sizeof(struct cec_tx_msg));
	CEC_INFO("hw reset :logical addr:0x%x\n",
		aocec_rd_reg(CEC_LOGICAL_ADDR0));
}

void cec_rx_buf_clear(void)
{
	aocec_wr_reg(CEC_RX_CLEAR_BUF, 0x1);
	aocec_wr_reg(CEC_RX_CLEAR_BUF, 0x0);
	CEC_INFO("rx buf clean\n");
}

int cec_rx_buf_check(void)
{
	unsigned int rx_num_msg = aocec_rd_reg(CEC_RX_NUM_MSG);
	unsigned tx_status = aocec_rd_reg(CEC_TX_MSG_STATUS);
	if (rx_num_msg)
		CEC_INFO("rx msg num:0x%02x\n", rx_num_msg);

	if (tx_status == TX_BUSY) {
		cec_tx_msgs.msg[cec_tx_msgs.send_idx].busy++;
		if (cec_tx_msgs.msg[cec_tx_msgs.send_idx].busy >= 7) {
			CEC_ERR("tx busy too long, reset hw\n");
			cec_hw_reset();
			cec_tx_msgs.msg[cec_tx_msgs.send_idx].busy = 0;
		}
	}
	if (tx_status == TX_IDLE) {
		if (cec_tx_msgs.send_idx != cec_tx_msgs.queue_idx) {
			/* triggle tx if idle */
			cec_ll_trigle_tx();
		}
	}

	return rx_num_msg;
}

int cec_ll_rx(unsigned char *msg, unsigned char *len)
{
	int i;
	int ret = -1;
	int pos;
	int rx_stat;
	struct hdmitx_dev *hdev;

	rx_stat = aocec_rd_reg(CEC_RX_MSG_STATUS);
	if ((RX_DONE != rx_stat) || (1 != aocec_rd_reg(CEC_RX_NUM_MSG))) {
		CEC_INFO("rx status:%x\n", rx_stat);
		writel((1 << 2), cec_dev->cec_reg + AO_CEC_INTR_CLR);
		aocec_wr_reg(CEC_RX_MSG_CMD,  RX_ACK_CURRENT);
		aocec_wr_reg(CEC_RX_MSG_CMD,  RX_NO_OP);
		return ret;
	}

	*len = aocec_rd_reg(CEC_RX_MSG_LENGTH) + 1;

	for (i = 0; i < (*len) && i < MAX_MSG; i++)
		msg[i] = aocec_rd_reg(CEC_RX_MSG_0_HEADER + i);

	ret = rx_stat;
	hdev = get_hdmitx_device();

	/* ignore ping message */
	if (cec_msg_dbg_en  == 1 && *len > 1) {
		pos = 0;
		pos += sprintf(msg_log_buf + pos,
			"CEC: rx msg len: %d   dat: ", *len);
		for (i = 0; i < (*len); i++)
			pos += sprintf(msg_log_buf + pos, "%02x ", msg[i]);
		pos += sprintf(msg_log_buf + pos, "\n");
		msg_log_buf[pos] = '\0';
		CEC_INFO("%s", msg_log_buf);
	}
	writel((1 << 2), cec_dev->cec_reg + AO_CEC_INTR_CLR);
	aocec_wr_reg(CEC_RX_MSG_CMD, RX_ACK_NEXT);
	aocec_wr_reg(CEC_RX_MSG_CMD, RX_NO_OP);
	return ret;
}

static int cec_queue_tx_msg(const unsigned char *msg, unsigned char len)
{
	int s_idx, q_idx;

	s_idx = cec_tx_msgs.send_idx;
	q_idx = cec_tx_msgs.queue_idx;
	if (((q_idx + 1) & CEC_TX_MSG_BUF_MASK) == s_idx) {
		CEC_ERR("tx buffer full, abort msg\n");
		cec_hw_reset();
		return -1;
	}
	if (len && msg) {
		memcpy(cec_tx_msgs.msg[q_idx].buf, msg, len);
		cec_tx_msgs.msg[q_idx].len = len;
		cec_tx_msgs.queue_idx = (q_idx + 1) & CEC_TX_MSG_BUF_MASK;
	}
	return 0;
}

/************************ cec arbitration cts code **************************/
/* using the cec pin as fiq gpi to assist the bus arbitration */

/* return value: 1: successful	  0: error */
static int cec_ll_trigle_tx(void)
{
	int i;
	unsigned int n;
	int pos;
	int reg = aocec_rd_reg(CEC_TX_MSG_STATUS);
	unsigned int s_idx;
	int len;
	char *msg;

	if (reg == TX_IDLE || reg == TX_DONE) {
		s_idx = cec_tx_msgs.send_idx;
		msg = cec_tx_msgs.msg[s_idx].buf;
		len = cec_tx_msgs.msg[s_idx].len;
		for (i = 0; i < len; i++)
			aocec_wr_reg(CEC_TX_MSG_0_HEADER + i, msg[i]);

		aocec_wr_reg(CEC_TX_MSG_LENGTH, len-1);
		aocec_wr_reg(CEC_TX_MSG_CMD, TX_REQ_CURRENT);

		if (cec_msg_dbg_en  == 1) {
			pos = 0;
			pos += sprintf(msg_log_buf + pos,
				       "CEC: tx msg len: %d   dat: ", len);
			for (n = 0; n < len; n++) {
				pos += sprintf(msg_log_buf + pos,
					       "%02x ", msg[n]);
			}

			pos += sprintf(msg_log_buf + pos, "\n");

			msg_log_buf[pos] = '\0';
			pr_info("%s", msg_log_buf);
		}
		return 0;
	}
	return -1;
}

void tx_irq_handle(void)
{
	unsigned tx_status = aocec_rd_reg(CEC_TX_MSG_STATUS);
	unsigned int s_idx;
	switch (tx_status) {
	case TX_DONE:
		aocec_wr_reg(CEC_TX_MSG_CMD, TX_NO_OP);
		s_idx = cec_tx_msgs.send_idx;
		cec_tx_msgs.msg[s_idx].busy = 0;
		/*
		 * we should not increase send idx if there is nothing to send
		 * but got tx done irq. This can happen when resume from uboot
		 */
		if (cec_tx_msgs.send_idx != cec_tx_msgs.queue_idx) {
			cec_tx_msgs.send_idx = (cec_tx_msgs.send_idx + 1) &
						CEC_TX_MSG_BUF_MASK;
		}
		if (cec_tx_msgs.send_idx != cec_tx_msgs.queue_idx)
			cec_ll_trigle_tx();
		cec_tx_fail_reason = CEC_FAIL_NONE;
		complete(&cec_dev->tx_ok);
		cec_tx_msgs.msg[s_idx].retry = 0;
		break;

	case TX_BUSY:
		s_idx = cec_tx_msgs.send_idx;
		cec_tx_msgs.msg[s_idx].busy++;
		CEC_INFO("TX_BUSY\n");
		break;

	case TX_ERROR:
		if (cec_msg_dbg_en  == 1)
			CEC_ERR("TX ERROR!!!\n");
		if (RX_ERROR == aocec_rd_reg(CEC_RX_MSG_STATUS)) {
			cec_hw_reset();
		} else {
			aocec_wr_reg(CEC_TX_MSG_CMD, TX_NO_OP);
			s_idx = cec_tx_msgs.send_idx;
			if (cec_tx_msgs.msg[s_idx].retry < 1) {
				cec_tx_msgs.msg[s_idx].retry++;
				cec_ll_trigle_tx();
			} else {
				CEC_INFO("TX retry over, abort\n");
				cec_tx_msgs.msg[s_idx].retry = 0;
				cec_tx_msgs.send_idx =
					(cec_tx_msgs.send_idx + 1) &
					CEC_TX_MSG_BUF_MASK;
				s_idx = cec_tx_msgs.send_idx;
				if (s_idx != cec_tx_msgs.queue_idx)
					cec_ll_trigle_tx();
				cec_tx_fail_reason = CEC_FAIL_NACK;
				complete(&cec_dev->tx_ok);
			}
		}
		break;

	case TX_IDLE:
		if (cec_tx_msgs.send_idx != cec_tx_msgs.queue_idx) {
			/* triggle tx if idle */
			cec_ll_trigle_tx();
		}
		break;
	default:
		break;
	}
	writel((1 << 1), cec_dev->cec_reg + AO_CEC_INTR_CLR);
}

/* Return value: < 0: fail, > 0: success */
int cec_ll_tx(const unsigned char *msg, unsigned char len)
{
	int ret = 0;
	int timeout = msecs_to_jiffies(1000);
	struct hdmitx_dev *hdev;

	mutex_lock(&cec_dev->cec_mutex);
	/*
	 * do not send messanges if tv is not support CEC
	 */
	hdev = get_hdmitx_device();

	cec_tx_fail_reason = CEC_FAIL_NONE;
	cec_queue_tx_msg(msg, len);
	ret = cec_ll_trigle_tx();
	if (ret < 0) {
		cec_tx_fail_reason = CEC_FAIL_BUSY;
		mutex_unlock(&cec_dev->cec_mutex);
		return ret;
	}
	ret = wait_for_completion_timeout(&cec_dev->tx_ok, timeout);
	if (ret <= 0) {
		/* timeout or interrupt */
		cec_tx_fail_reason = CEC_FAIL_OTHER;
	} else {
		if (cec_tx_fail_reason != CEC_FAIL_NONE) {
			/* not timeout but got fail info */
			ret = -1;
		} else {
			/* send success */
			ret = len;
		}
	}
	mutex_unlock(&cec_dev->cec_mutex);

	return ret;
}

int get_cec_tx_fail(void)
{
	return cec_tx_fail_reason;
}

/* -------------------------------------------------------------------------- */
/* AO CEC0 config */
/* -------------------------------------------------------------------------- */
void ao_cec_init(void)
{
	unsigned long data32;
	unsigned int reg;
	/* Assert SW reset AO_CEC */
	reg = readl(cec_dev->cec_reg + AO_CRT_CLK_CNTL1);
	/* 24MHz/ (731 + 1) = 32786.885Hz */
	reg &= ~(0x7ff << 16);
	reg |= (731 << 16);	/* divider from 24MHz */
	reg |= (0x1 << 26);
	reg &= ~(0x800 << 16);	/* select divider */
	writel(reg, cec_dev->cec_reg + AO_CRT_CLK_CNTL1);
	data32  = 0;
	data32 |= 0 << 1;   /* [2:1]	cntl_clk: */
				/* 0=Disable clk (Power-off mode); */
				/* 1=Enable gated clock (Normal mode); */
				/* 2=Enable free-run clk (Debug mode). */
	data32 |= 1 << 0;   /* [0]	  sw_reset: 1=Reset */
	writel(data32, cec_dev->cec_reg + AO_CEC_GEN_CNTL);
	/* Enable gated clock (Normal mode). */
	cec_set_reg_bits(AO_CEC_GEN_CNTL, 1, 1, 1);
	/* Release SW reset */
	cec_set_reg_bits(AO_CEC_GEN_CNTL, 0, 0, 1);

	/* Enable all AO_CEC interrupt sources */
	cec_enable_irq();

	memset(&cec_tx_msgs, 0, sizeof(struct cec_tx_msg));
}

void cec_arbit_bit_time_set(unsigned bit_set, unsigned time_set, unsigned flag)
{   /* 11bit:bit[10:0] */
	if (flag) {
		CEC_INFO("bit_set:0x%x;time_set:0x%x\n",
			bit_set, time_set);
	}

	switch (bit_set) {
	case 3:
		/* 3 bit */
		if (flag) {
			CEC_INFO("read 3 bit:0x%x%x\n",
				aocec_rd_reg(AO_CEC_TXTIME_4BIT_BIT10_8),
				aocec_rd_reg(AO_CEC_TXTIME_4BIT_BIT7_0));
		}
		aocec_wr_reg(AO_CEC_TXTIME_4BIT_BIT7_0, time_set & 0xff);
		aocec_wr_reg(AO_CEC_TXTIME_4BIT_BIT10_8, (time_set >> 8) & 0x7);
		if (flag) {
			CEC_INFO("write 3 bit:0x%x%x\n",
				aocec_rd_reg(AO_CEC_TXTIME_4BIT_BIT10_8),
				aocec_rd_reg(AO_CEC_TXTIME_4BIT_BIT7_0));
		}
		break;
		/* 5 bit */
	case 5:
		if (flag) {
			CEC_INFO("read 5 bit:0x%x%x\n",
				aocec_rd_reg(AO_CEC_TXTIME_2BIT_BIT10_8),
				aocec_rd_reg(AO_CEC_TXTIME_2BIT_BIT7_0));
		}
		aocec_wr_reg(AO_CEC_TXTIME_2BIT_BIT7_0, time_set & 0xff);
		aocec_wr_reg(AO_CEC_TXTIME_2BIT_BIT10_8, (time_set >> 8) & 0x7);
		if (flag) {
			CEC_INFO("write 5 bit:0x%x%x\n",
				aocec_rd_reg(AO_CEC_TXTIME_2BIT_BIT10_8),
				aocec_rd_reg(AO_CEC_TXTIME_2BIT_BIT7_0));
		}
		break;
		/* 7 bit */
	case 7:
		if (flag) {
			CEC_INFO("read 7 bit:0x%x%x\n",
				aocec_rd_reg(AO_CEC_TXTIME_17MS_BIT10_8),
				aocec_rd_reg(AO_CEC_TXTIME_17MS_BIT7_0));
		}
		aocec_wr_reg(AO_CEC_TXTIME_17MS_BIT7_0, time_set & 0xff);
		aocec_wr_reg(AO_CEC_TXTIME_17MS_BIT10_8, (time_set >> 8) & 0x7);
		if (flag) {
			CEC_INFO("write 7 bit:0x%x%x\n",
				aocec_rd_reg(AO_CEC_TXTIME_17MS_BIT10_8),
				aocec_rd_reg(AO_CEC_TXTIME_17MS_BIT7_0));
		}
		break;
	default:
		break;
	}
}

static unsigned int ao_cec_intr_stat(void)
{
	return readl(cec_dev->cec_reg + AO_CEC_INTR_STAT);
}

unsigned int cec_intr_stat(void)
{
	return ao_cec_intr_stat();
}

/*
 *wr_flag: 1 write; value valid
 *		 0 read;  value invalid
 */
unsigned int cec_config(unsigned int value, bool wr_flag)
{
	if (wr_flag)
		cec_set_reg_bits(AO_DEBUG_REG0, value, 0, 8);

	return readl(cec_dev->cec_reg + AO_DEBUG_REG0);
}

/*
 *wr_flag:1 write; value valid
 *		0 read;  value invalid
 */
unsigned int cec_phyaddr_config(unsigned int value, bool wr_flag)
{
	if (wr_flag)
		cec_set_reg_bits(AO_DEBUG_REG1, value, 0, 16);

	return readl(cec_dev->cec_reg + AO_DEBUG_REG1);
}

/*
 *wr_flag:1 write; value valid
 *		0 read;  value invalid
 */
unsigned int cec_logicaddr_config(unsigned int value, bool wr_flag)
{
	if (wr_flag)
		cec_set_reg_bits(AO_DEBUG_REG3, value, 0, 8);

	return readl(cec_dev->cec_reg + AO_DEBUG_REG3);
}

void cec_keep_reset(void)
{
	writel(0x1, cec_dev->cec_reg + AO_CEC_GEN_CNTL);
}
/*
 * cec hw module init befor allocate logical address
 */
static void cec_pre_init(void)
{
	ao_cec_init();

	cec_arbit_bit_time_set(3, 0x118, 0);
	cec_arbit_bit_time_set(5, 0x000, 0);
	cec_arbit_bit_time_set(7, 0x2aa, 0);
}

static int cec_late_check_rx_buffer(void)
{
	int ret;
	struct delayed_work *dwork = &cec_dev->cec_work;

	ret = cec_rx_buf_check();
	if (!ret)
		return 0;
	/*
	 * start another check if rx buffer is full
	 */
	if ((-1) == cec_ll_rx(rx_msg, &rx_len)) {
		CEC_INFO("buffer got unrecorgnized msg\n");
		cec_rx_buf_clear();
		return 0;
	} else {
		mod_delayed_work(cec_dev->cec_thread, dwork, 0);
		return 1;
	}
}

static void cec_task(struct work_struct *work)
{
	struct delayed_work *dwork;

	dwork = &cec_dev->cec_work;
	if (!cec_late_check_rx_buffer())
		queue_delayed_work(cec_dev->cec_thread, dwork, CEC_FRAME_DELAY);
}

static irqreturn_t cec_isr_handler(int irq, void *dev_instance)
{
	unsigned int intr_stat = 0;
	struct delayed_work *dwork;

	dwork = &cec_dev->cec_work;
	intr_stat = cec_intr_stat();
	if (intr_stat & (1<<1)) {   /* aocec tx intr */
		tx_irq_handle();
		return IRQ_HANDLED;
	}
	if ((-1) == cec_ll_rx(rx_msg, &rx_len))
		return IRQ_HANDLED;

	complete(&cec_dev->rx_ok);
	/* check rx buffer is full */
	queue_delayed_work(cec_dev->cec_thread, dwork, CEC_FRAME_DELAY);
	return IRQ_HANDLED;
}

/******************** cec class interface *************************/
static ssize_t device_type_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%ld\n", cec_dev->dev_type);
}

static ssize_t device_type_store(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned long type;

	if (sscanf(buf, "%ld", &type) != 1)
		return -EINVAL;

	cec_dev->dev_type = type;
	CEC_ERR("set dev_type to %ld\n", type);
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
	struct class_attribute *attr, const char *buf, size_t count)
{
	char a, b, c;

	if (sscanf(buf, "%c%c%c", &a, &b, &c) != 3)
		return -EINVAL;

	cec_dev->cec_info.menu_lang = (a << 16) | (b << 8) | c;
	CEC_ERR("set menu_language to %s\n", buf);
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
	int id;

	if (sscanf(buf, "%x", &id) != 1)
		return -EINVAL;
	cec_dev->cec_info.vendor_id = id;
	return count;
}

static ssize_t port_num_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", cec_dev->port_num);
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

static ssize_t port_status_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	unsigned int tmp;

	if (cec_dev->dev_type == DEV_TYPE_TX) {
		tmp = cec_dev->tx_dev->hpd_state;
		return sprintf(buf, "%x\n", tmp);
#ifdef CONFIG_TVIN_HDMI
	} else {
		tmp = hdmirx_rd_top(TOP_HPD_PWR5V);
		tmp >>= 20;
		tmp &= 0xf;
		return sprintf(buf, "%x\n", tmp);
#endif
	}
}

static ssize_t physical_addr_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	int a, b, c, d;
	unsigned int tmp;

	if (cec_dev->dev_type ==  DEV_TYPE_TX) {
		/* physical address for mbox */
		if (cec_dev->tx_dev->hdmi_info.vsdb_phy_addr.valid == 0)
			return -EINVAL;
		a = cec_dev->tx_dev->hdmi_info.vsdb_phy_addr.a;
		b = cec_dev->tx_dev->hdmi_info.vsdb_phy_addr.b;
		c = cec_dev->tx_dev->hdmi_info.vsdb_phy_addr.c;
		d = cec_dev->tx_dev->hdmi_info.vsdb_phy_addr.d;
		tmp = ((a << 12) | (b << 8) | (c << 4) | (d << 0));
	} else {
		/* physical address for TV */
		tmp = 0;
	}

	return sprintf(buf, "%04x\n", tmp);
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

	if (sscanf(buf, "%d", &en) != 1)
		return -EINVAL;

	cec_msg_dbg_en = en ? 1 : 0;
	return count;
}

static struct class_attribute aocec_class_attr[] = {
	__ATTR_RO(port_num),
	__ATTR_RO(osd_name),
	__ATTR_RO(port_status),
	__ATTR_RO(arc_port),
	__ATTR_RO(physical_addr),
	__ATTR(vendor_id, 0664, vendor_id_show, vendor_id_store),
	__ATTR(menu_language, 0664, menu_language_show, menu_language_store),
	__ATTR(device_type, 0664, device_type_show, device_type_store),
	__ATTR(dbg_en, 0664, dbg_en_show, dbg_en_store),
	__ATTR_NULL
};

/******************** cec hal interface ***************************/
static int hdmitx_cec_open(struct inode *inode, struct file *file)
{
	cec_dev->cec_info.open_count++;
	if (cec_dev->cec_info.open_count) {
		cec_dev->cec_info.hal_ctl = 1;
		/* enable all cec features */
		cec_config(0x2f, 1);
	}
	return 0;
}

static int hdmitx_cec_release(struct inode *inode, struct file *file)
{
	cec_dev->cec_info.open_count--;
	if (!cec_dev->cec_info.open_count) {
		cec_dev->cec_info.hal_ctl = 0;
		/* disable all cec features */
		cec_config(0x0, 1);
	}
	return 0;
}

static ssize_t hdmitx_cec_read(struct file *f, char __user *buf,
			   size_t size, loff_t *p)
{
	int ret;

	rx_len = 0;
	ret = wait_for_completion_timeout(&cec_dev->rx_ok, CEC_FRAME_DELAY);
	if (ret <= 0)
		return ret;
	if (rx_len == 0)
		return 0;

	if (copy_to_user(buf, rx_msg, rx_len))
		return -EINVAL;
	return rx_len;
}

static ssize_t hdmitx_cec_write(struct file *f, const char __user *buf,
			    size_t size, loff_t *p)
{
	unsigned char tempbuf[16] = {};

	if (size > 16)
		size = 16;
	if (size <= 0)
		return 0;

	if (copy_from_user(tempbuf, buf, size))
		return -EINVAL;

	return cec_ll_tx(tempbuf, size);
}

static long hdmitx_cec_ioctl(struct file *f,
			     unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	unsigned long tmp;
	struct hdmi_port_info *port;
	int a, b, c, d;

	switch (cmd) {
	case CEC_IOC_GET_PHYSICAL_ADDR:
		if (cec_dev->dev_type ==  DEV_TYPE_TX) {
			/* physical address for mbox */
			if (cec_dev->tx_dev->hdmi_info.vsdb_phy_addr.valid == 0)
				return -EINVAL;
			a = cec_dev->tx_dev->hdmi_info.vsdb_phy_addr.a;
			b = cec_dev->tx_dev->hdmi_info.vsdb_phy_addr.b;
			c = cec_dev->tx_dev->hdmi_info.vsdb_phy_addr.c;
			d = cec_dev->tx_dev->hdmi_info.vsdb_phy_addr.d;
			tmp = ((a << 12) | (b << 8) | (c << 4) | (d << 0));
		} else {
			/* physical address for TV */
			tmp = 0;
		}
		cec_phyaddr_config(tmp, 1);
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd)))
			return -EINVAL;
		break;

	case CEC_IOC_GET_VERSION:
		tmp = CEC_VERSION_14A;
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd)))
			return -EINVAL;
		break;

	case CEC_IOC_GET_VENDOR_ID:
		tmp = cec_dev->v_data.vendor_id;
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd)))
			return -EINVAL;
		break;

	case CEC_IOC_GET_PORT_NUM:
		tmp = cec_dev->port_num;
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd)))
			return -EINVAL;
		break;

	case CEC_IOC_GET_PORT_INFO:
		port = kzalloc(sizeof(*port) * cec_dev->port_num, GFP_KERNEL);
		if (!port) {
			CEC_ERR("no memory\n");
			return -EINVAL;
		}
		if (cec_dev->dev_type == DEV_TYPE_TX) {
			/* for tx only 1 port */
			a = cec_dev->tx_dev->hdmi_info.vsdb_phy_addr.a;
			b = cec_dev->tx_dev->hdmi_info.vsdb_phy_addr.b;
			c = cec_dev->tx_dev->hdmi_info.vsdb_phy_addr.c;
			d = cec_dev->tx_dev->hdmi_info.vsdb_phy_addr.d;
			tmp = ((a << 12) | (b << 8) | (c << 4) | (d << 0));
			if (cec_dev->tx_dev->hdmi_info.vsdb_phy_addr.valid == 0)
				tmp = 0xffff;
			port->type = HDMI_OUTPUT;
			port->port_id = 1;
			port->cec_supported = 1;
			/* not support arc */
			port->arc_supported = 0;
			port->physical_address = tmp & 0xffff;
			if (copy_to_user(argp, port, sizeof(*port))) {
				kfree(port);
				return -EINVAL;
			}
		} else {
			b = cec_dev->port_num;
			for (a = 0; a < b; a++) {
				port[a].type = HDMI_INPUT;
				port[a].port_id = a + 1;
				port[a].cec_supported = 1;
				/* no support arc */
				port[a].arc_supported = 0;
				port[a].physical_address = (a + 1) * 0x1000;
			}
			if (copy_to_user(argp, port, sizeof(*port) * b)) {
				kfree(port);
				return -EINVAL;
			}
		}
		kfree(port);
		break;

	case CEC_IOC_GET_SEND_FAIL_REASON:
		tmp = get_cec_tx_fail();
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd)))
			return -EINVAL;
		break;

	case CEC_IOC_SET_OPTION_WAKEUP:
		/* TODO: */
		break;

	case CEC_IOC_SET_OPTION_ENALBE_CEC:
		/* TODO: */
		break;

	case CEC_IOC_SET_OPTION_SYS_CTRL:
		/* TODO: */
		break;

	case CEC_IOC_SET_OPTION_SET_LANG:
		cec_dev->cec_info.menu_lang = arg;
		break;

	case CEC_IOC_GET_CONNECT_STATUS:
		if (cec_dev->dev_type == DEV_TYPE_TX)
			tmp = cec_dev->tx_dev->hpd_state;
	#ifdef CONFIG_TVIN_HDMI
		else {
			if (copy_from_user(&a, argp, _IOC_SIZE(cmd)))
				return -EINVAL;
			tmp = (hdmirx_rd_top(TOP_HPD_PWR5V) >> 20);
			if (tmp & (1 << (a - 1)))
				tmp = 1;
			else
				tmp = 0;
		}
	#endif
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd)))
			return -EINVAL;
		break;

	case CEC_IOC_ADD_LOGICAL_ADDR:
		tmp = arg & 0xf;
		cec_logicaddr_set(tmp);
		/* add by hal, to init some data structure */
		cec_dev->cec_info.log_addr = tmp;
		cec_dev->cec_info.power_status = POWER_ON;
		cec_logicaddr_config(tmp, 1);

		cec_dev->cec_info.cec_version = CEC_VERSION_14A;
		cec_dev->cec_info.vendor_id = cec_dev->v_data.vendor_id;
		strcpy(cec_dev->cec_info.osd_name,
		       cec_dev->v_data.cec_osd_string);

		if (cec_dev->dev_type == DEV_TYPE_TX)
			cec_dev->cec_info.menu_status = DEVICE_MENU_ACTIVE;
		break;

	case CEC_IOC_CLR_LOGICAL_ADDR:
		/* TODO: clear global info */
		break;

	case CEC_IOC_SET_DEV_TYPE:
		if (arg != DEV_TYPE_TX && arg != DEV_TYPE_RX)
			return -EINVAL;
		cec_dev->dev_type = arg;
		break;

	default:
		break;
	}
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

/* for improve rw permission */
static char *aml_cec_class_devnode(struct device *dev, umode_t *mode)
{
	if (mode)
		*mode = 0666;
	CEC_INFO("mode is %x\n", *mode);
	return NULL;
}

static struct class aocec_class = {
	.name = CEC_DEV_NAME,
	.class_attrs = aocec_class_attr,
	.devnode = aml_cec_class_devnode,
};


static const struct file_operations hdmitx_cec_fops = {
	.owner          = THIS_MODULE,
	.open           = hdmitx_cec_open,
	.read           = hdmitx_cec_read,
	.write          = hdmitx_cec_write,
	.release        = hdmitx_cec_release,
	.unlocked_ioctl = hdmitx_cec_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = hdmitx_cec_compat_ioctl,
#endif
};

/************************ cec high level code *****************************/

static int aml_cec_probe(struct platform_device *pdev)
{
	struct device *cdev;
#ifdef CONFIG_OF
	struct device_node *node = pdev->dev.of_node;
	int irq_idx = 0, r;
	const char *pin_name = NULL, *irq_name = NULL;
	struct pinctrl *p;
	struct vendor_info_data *vend;
	struct resource *res;
	resource_size_t *base;
#endif

	cec_dev = kzalloc(sizeof(struct ao_cec_dev), GFP_KERNEL);
	if (!cec_dev) {
		CEC_ERR("alloc memory failed\n");
		return -ENOMEM;
	}
	device_rename(&pdev->dev, "aocec");
	cec_dev->dev_type = DEV_TYPE_TX;
	cec_dev->dbg_dev  = &pdev->dev;
	cec_dev->tx_dev   = get_hdmitx_device();

	/* cdev registe */
	r = class_register(&aocec_class);
	if (r) {
		CEC_ERR("regist class failed\n");
		return -EINVAL;
	}
	pdev->dev.class = &aocec_class;
	cec_dev->cec_info.dev_no = register_chrdev(0, CEC_DEV_NAME,
					  &hdmitx_cec_fops);
	if (cec_dev->cec_info.dev_no < 0) {
		CEC_ERR("alloc chrdev failed\n");
		return -EINVAL;
	}
	CEC_INFO("alloc chrdev %x\n", cec_dev->cec_info.dev_no);
	cdev = device_create(&aocec_class, &pdev->dev,
			     MKDEV(cec_dev->cec_info.dev_no, 0),
			     NULL, CEC_DEV_NAME);
	if (IS_ERR(cdev)) {
		CEC_ERR("create chrdev failed, dev:%p\n", cdev);
		unregister_chrdev(cec_dev->cec_info.dev_no,
				  CEC_DEV_NAME);
		return -EINVAL;
	}

	init_completion(&cec_dev->rx_ok);
	init_completion(&cec_dev->tx_ok);
	mutex_init(&cec_dev->cec_mutex);
	spin_lock_init(&cec_dev->cec_reg_lock);
	cec_dev->cec_thread = create_workqueue("cec_work");
	if (cec_dev->cec_thread == NULL) {
		CEC_INFO("create work queue failed\n");
		return -EFAULT;
	}
	INIT_DELAYED_WORK(&cec_dev->cec_work, cec_task);
	cec_dev->cec_info.remote_cec_dev = input_allocate_device();
	if (!cec_dev->cec_info.remote_cec_dev)
		CEC_INFO("No enough memory\n");

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

#ifdef CONFIG_OF
	/* pinmux set */
	if (of_get_property(node, "pinctrl-names", NULL)) {
		r = of_property_read_string(node,
					    "pinctrl-names",
					    &pin_name);
		if (!r)
			p = devm_pinctrl_get_select(&pdev->dev, pin_name);
	}

	/* irq set */
	irq_idx = of_irq_get(node, 0);
	cec_dev->irq_cec = irq_idx;
	if (of_get_property(node, "interrupt-names", NULL)) {
		r = of_property_read_string(node, "interrupt-names", &irq_name);
		if (!r) {
			r = request_irq(irq_idx, &cec_isr_handler, IRQF_SHARED,
					irq_name, (void *)cec_dev);
		}
	}
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) {
		base = ioremap(res->start, res->end - res->start);
		cec_dev->exit_reg = (void *)base;
	} else {
		CEC_INFO("no memory resource\n");
		cec_dev->exit_reg = NULL;
	}
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res) {
		base = ioremap(res->start, res->end - res->start);
		cec_dev->cec_reg = (void *)base;
	} else {
		CEC_ERR("no CEC reg resource\n");
		cec_dev->cec_reg = NULL;
	}
	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (res) {
		base = ioremap(res->start, res->end - res->start);
		cec_dev->hdmi_rxreg = (void *)base;
	} else {
		CEC_ERR("no hdmirx reg resource\n");
		cec_dev->hdmi_rxreg = NULL;
	}

	r = of_property_read_u32(node, "port_num", &(cec_dev->port_num));
	if (r) {
		CEC_ERR("not find 'port_num'\n");
		cec_dev->port_num = 1;
	}
	r = of_property_read_u32(node, "arc_port_mask", &(cec_dev->arc_port));
	if (r) {
		CEC_ERR("not find 'arc_port_mask'\n");
		cec_dev->arc_port = 0;
	}

	vend = &cec_dev->v_data;
	r = of_property_read_string(node, "vendor_name",
		(const char **)&(vend->vendor_name));
	if (r)
		CEC_INFO("not find vendor name\n");

	r = of_property_read_u32(node, "vendor_id", &(vend->vendor_id));
	if (r)
		CEC_INFO("not find vendor id\n");

	r = of_property_read_string(node, "product_desc",
		(const char **)&(vend->product_desc));
	if (r)
		CEC_INFO("not find product desc\n");

	r = of_property_read_string(node, "cec_osd_string",
		(const char **)&(vend->cec_osd_string));
	if (r) {
		CEC_INFO("not find cec osd string\n");
		strcpy(vend->cec_osd_string, "AML TV/BOX");
	}
#endif
	/* for init */
	cec_pre_init();
	queue_delayed_work(cec_dev->cec_thread, &cec_dev->cec_work, 0);
	return 0;
}

static int aml_cec_remove(struct platform_device *pdev)
{
	CEC_INFO("cec uninit!\n");
	free_irq(cec_dev->irq_cec, (void *)cec_dev);

	if (cec_dev->cec_thread) {
		cancel_delayed_work_sync(&cec_dev->cec_work);
		destroy_workqueue(cec_dev->cec_thread);
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
	cec_dev->cec_suspend = 1;
	CEC_INFO("cec prepare suspend!\n");
	return 0;
}

static void aml_cec_pm_complete(struct device *dev)
{
	int exit = 0;

	if (cec_dev->exit_reg) {
		exit = readl(cec_dev->exit_reg);
		CEC_INFO("wake up flag:%x\n", exit);
	}
	if (((exit >> 28) & 0xf) == CEC_WAKEUP) {
		input_event(cec_dev->cec_info.remote_cec_dev,
			    EV_KEY, KEY_POWER, 1);
		input_sync(cec_dev->cec_info.remote_cec_dev);
		input_event(cec_dev->cec_info.remote_cec_dev,
			    EV_KEY, KEY_POWER, 0);
		input_sync(cec_dev->cec_info.remote_cec_dev);
		CEC_INFO("== WAKE UP BY CEC ==\n");
	}
	cec_dev->cec_suspend = 0;
}

static int aml_cec_resume_noirq(struct device *dev)
{
	CEC_INFO("cec resume noirq!\n");
	cec_dev->cec_info.power_status = TRANS_STANDBY_TO_ON;
	return 0;
}

static const struct dev_pm_ops aml_cec_pm = {
	.prepare  = aml_cec_pm_prepare,
	.complete = aml_cec_pm_complete,
	.resume_noirq = aml_cec_resume_noirq,
};
#endif

#ifdef CONFIG_OF
static const struct of_device_id aml_cec_dt_match[] = {
	{
		.compatible = "amlogic, amlogic-aocec",
	},
	{},
};
#endif

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
	.probe  = aml_cec_probe,
	.remove = aml_cec_remove,
};

static int __init cec_init(void)
{
	int ret;
	ret = platform_driver_register(&aml_cec_driver);
	return ret;
}

static void __exit cec_uninit(void)
{
	platform_driver_unregister(&aml_cec_driver);
}


module_init(cec_init);
module_exit(cec_uninit);
MODULE_DESCRIPTION("AMLOGIC HDMI TX CEC driver");
MODULE_LICENSE("GPL");
