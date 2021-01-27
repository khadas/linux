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
/*#include <linux/pinctrl/consumer.h>*/
#include <linux/pm_wakeup.h>
#include <linux/pm_wakeirq.h>
#include <linux/pm.h>
#include <linux/poll.h>
/*#include <linux/amlogic/media/frame_provider/tvin/tvin.h>*/
/*#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_cec_20.h>*/
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include <linux/amlogic/pm.h>
#include <linux/amlogic/cpu_version.h>
/*#include <linux/amlogic/jtag.h>*/
#include <linux/amlogic/scpi_protocol.h>
#include <linux/amlogic/pm.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static struct early_suspend aocec_suspend_handler;
#endif
#include "hdmi_tx_cec_20.h"
#include "hdmi_ao_cec.h"

/* cec driver function config */
#define CEC_FREEZE_WAKE_UP
#define CEC_MAIL_BOX

DECLARE_WAIT_QUEUE_HEAD(cec_msg_wait_queue);

static struct cec_msg_last *last_cec_msg;
static struct dbgflg stdbgflg;
static int phy_addr_test;
static struct tasklet_struct ceca_tasklet;

static struct ao_cec_dev *cec_dev;
static enum cec_tx_ret cec_tx_result;
static int cec_line_cnt;
static struct hrtimer start_bit_check;
static unsigned char rx_msg[MAX_MSG];
static unsigned char rx_len;
static unsigned int  new_msg;

static bool ceca_err_flag;

/*static bool wake_ok = 1;*/
static bool ee_cec;
static bool pin_status;
static int cec_msg_dbg_en;
static struct st_cec_mailbox_data cec_mailbox;

#define CEC_ERR(format, args...)				\
	do {	\
		if (cec_dev->dbg_dev)				\
			pr_info("cec: " format, ##args);	\
	} while (0)

#define CEC_INFO(format, args...)				\
	do {	\
		if (cec_msg_dbg_en && cec_dev->dbg_dev)		\
			pr_info("cec: " format, ##args);	\
	} while (0)

#define dprintk(level, fmt, arg...) \
	do { \
		if (cec_msg_dbg_en >= (level) && cec_dev->dbg_dev) \
			pr_info("cec: " fmt, ## arg); \
	} while (0)

static unsigned char msg_log_buf[128] = { 0 };

static const char * const cec_reg_name1[] = {
	"CEC_TX_MSG_LENGTH",
	"CEC_TX_MSG_CMD",
	"CEC_TX_WRITE_BUF",
	"CEC_TX_CLEAR_BUF",
	"CEC_RX_MSG_CMD",
	"CEC_RX_CLEAR_BUF",
	"CEC_LOGICAL_ADDR0",
	"CEC_LOGICAL_ADDR1",
	"CEC_LOGICAL_ADDR2",
	"CEC_LOGICAL_ADDR3",
	"CEC_LOGICAL_ADDR4",
	"CEC_CLOCK_DIV_H",
	"CEC_CLOCK_DIV_L"
};

static const char * const cec_reg_name2[] = {
	"CEC_RX_MSG_LENGTH",
	"CEC_RX_MSG_STATUS",
	"CEC_RX_NUM_MSG",
	"CEC_TX_MSG_STATUS",
	"CEC_TX_NUM_MSG"
};

static const char * const ceca_reg_name3[] = {
	"STAT_0_0",
	"STAT_0_1",
	"STAT_0_2",
	"STAT_0_3",
	"STAT_1_0",
	"STAT_1_1",
	"STAT_1_2"
};

unsigned int top_reg_tab[AO_REG_DEF_END][cec_reg_group_max] = {
	/*old, A1 later, */
	{(0x1d << 2), 0xffff},/*AO_CEC_CLK_CNTL_REG0*/
	{(0x1e << 2), 0xffff},/*AO_CEC_CLK_CNTL_REG1*/
	{(0x40 << 2), 0x0},/*AO_CEC_GEN_CNTL*/
	{(0x41 << 2), 0x4},/*AO_CEC_RW_REG*/
	{(0x42 << 2), 0x8},/*AO_CEC_INTR_MASKN*/
	{(0x43 << 2), 0xc},/*AO_CEC_INTR_CLR*/
	{(0x44 << 2), 0x10},/*AO_CEC_INTR_STAT*/

	{(0xa0 << 2), 0xffff},/*AO_CECB_CLK_CNTL_REG0*/
	{(0xa1 << 2), 0xffff},/*AO_CECB_CLK_CNTL_REG1*/
	{(0xa2 << 2), 0x40},/*AO_CECB_GEN_CNTL*/
	{(0xa3 << 2), 0x44},/*AO_CECB_RW_REG*/
	{(0xa4 << 2), 0x48},/*AO_CECB_INTR_MASKN*/
	{(0xa5 << 2), 0x4c},/*AO_CECB_INTR_CLR*/
	{(0xa6 << 2), 0x50},/*AO_CECB_INTR_STAT*/

	{(0x01 << 2), 0xffff},/*AO_RTI_STATUS_REG1*/
	{(0x04 << 2), 0xffff},/*AO_RTI_PWR_CNTL_REG0*/
	{(0x1a << 2), 0xffff},/*AO_CRT_CLK_CNTL1*/
	{(0x25 << 2), 0xffff},/*AO_RTC_ALT_CLK_CNTL0*/
	{(0x26 << 2), 0xffff},/*AO_RTC_ALT_CLK_CNTL1*/

	/*AO_DEBUG_REG0 SYSCTRL_STATUS_REG0*/
	{(0x28 << 2), REG_MASK_PR | (0xa0 << 2)},
	/*AO_DEBUG_REG1 SYSCTRL_STATUS_REG1*/
	{(0x29 << 2), REG_MASK_PR | (0xa1 << 2)},
	/*AO_GPIO_I*/
	{(0x0A << 2), 0xffff},
};

static void write_ao(unsigned int addr, unsigned int data)
{
	unsigned int real_addr;
	unsigned int reg_grp = cec_dev->plat_data->reg_tab_group;

	if (reg_grp >= cec_reg_group_max) {
		pr_err("%s reg grp %d err\n", __func__, reg_grp);
		return;
	}

	real_addr = top_reg_tab[addr][reg_grp];

	if ((real_addr & REG_MASK_ADDR) == 0xffff) {
		dprintk(L_4, "%s, no exist reg:0x%x", __func__, addr);
		return;
	}
	dprintk(L_4, "%s :reg idx:0x%x, 0x%x(0x%lx) val:0x%x\n", __func__,
		   addr, (real_addr & REG_MASK_ADDR),
		   (long)(cec_dev->cec_reg + (real_addr & REG_MASK_ADDR)),
		   data);
	if (real_addr & REG_MASK_PR)
		writel(data, cec_dev->periphs_reg +
		       (real_addr & REG_MASK_ADDR));
	else
		writel(data, cec_dev->cec_reg +
		       (real_addr & REG_MASK_ADDR));
}

static unsigned int read_ao(unsigned int addr)
{
	unsigned int real_addr;
	unsigned int data;
	unsigned int reg_grp = cec_dev->plat_data->reg_tab_group;

	if (reg_grp >= cec_reg_group_max) {
		pr_err("%s reg grp %d err\n", __func__, reg_grp);
		return 0;
	}

	real_addr = top_reg_tab[addr][reg_grp];

	if (((real_addr & REG_MASK_ADDR)) == 0xffff) {
		dprintk(L_4, "w ao no exist reg:0x%x", addr);
		return 0;
	}

	dprintk(L_4, "%s :reg idx:0x%x, 0x%x(0x%lx)\n", __func__, addr,
		   (real_addr & REG_MASK_ADDR),
		   (long)(cec_dev->cec_reg + (real_addr & REG_MASK_ADDR)));

	if (real_addr & REG_MASK_PR)
		data = readl(cec_dev->periphs_reg +
			     (real_addr & REG_MASK_ADDR));
	else
		data = readl(cec_dev->cec_reg +
			     (real_addr & REG_MASK_ADDR));
	dprintk(L_4, "\t val:0x%x\n", data);
	return data;
}

static void write_hiu(unsigned int addr, unsigned int data)
{
	if (cec_dev->hhi_reg)
		writel(data, cec_dev->hhi_reg + addr);
}

static unsigned int read_hiu(unsigned long addr)
{
	unsigned int data = 0;

	if (cec_dev->hhi_reg)
		data = readl(cec_dev->hhi_reg + addr);
	return data;
}

void write_periphs(unsigned int addr, unsigned int data)
{
	if (cec_dev->periphs_reg)
		writel(data, cec_dev->periphs_reg + addr);
}

static unsigned int read_periphs(unsigned int addr)
{
	unsigned int data = 0;

	if (cec_dev->periphs_reg)
		data = readl(cec_dev->periphs_reg + addr);
	return data;
}

void write_clock(unsigned int addr, unsigned int data)
{
	if (cec_dev->clk_reg)
		writel(data, cec_dev->clk_reg + (addr << 2));
}

unsigned int read_clock(unsigned int addr)
{
	unsigned int data = 0;

	if (cec_dev->clk_reg)
		data = readl(cec_dev->clk_reg + (addr << 2));
	return data;
}

unsigned int waiting_aocec_free(unsigned int r)
{
	unsigned int cnt = 0;
	int ret = true;

	while (read_ao(r) & (1 << 23)) {
		if (cnt++ >= 3500) {
			pr_info("waiting aocec %x free time out %d\n", r, cnt);
			/*if (cec_dev->probe_finish)*/
			/*	cec_hw_reset(CEC_A);*/
			ret = false;
			break;
		}
	}

	return ret;
}

static void cec_set_reg_bits(unsigned int addr, unsigned int value,
			     unsigned int offset, unsigned int len)
{
	unsigned int data32 = 0;

	data32 = read_ao(addr);
	data32 &= ~(((1 << len) - 1) << offset);
	data32 |= (value & ((1 << len) - 1)) << offset;
	write_ao(addr, data32);
}

unsigned int aocec_rd_reg(unsigned long addr)
{
	unsigned int data32;
	unsigned long flags;

	spin_lock_irqsave(&cec_dev->cec_reg_lock, flags);
	if (!waiting_aocec_free(AO_CEC_RW_REG)) {
		spin_unlock_irqrestore(&cec_dev->cec_reg_lock, flags);
		ceca_err_flag = 1;
		return 0;
	}
	data32 = 0;
	data32 |= 0 << 16; /* [16]	 cec_reg_wr */
	data32 |= 0 << 8; /* [15:8]   cec_reg_wrdata */
	data32 |= addr << 0; /* [7:0]	cec_reg_addr */
	write_ao(AO_CEC_RW_REG, data32);
	if (!waiting_aocec_free(AO_CEC_RW_REG)) {
		spin_unlock_irqrestore(&cec_dev->cec_reg_lock, flags);
		ceca_err_flag = 1;
		return 0;
	}
	data32 = ((read_ao(AO_CEC_RW_REG)) >> 24) & 0xff;
	spin_unlock_irqrestore(&cec_dev->cec_reg_lock, flags);
	return data32;
} /* aocec_rd_reg */

void aocec_wr_reg(unsigned long addr, unsigned long data)
{
	unsigned long data32;
	unsigned long flags;

	spin_lock_irqsave(&cec_dev->cec_reg_lock, flags);
	if (!waiting_aocec_free(AO_CEC_RW_REG)) {
		spin_unlock_irqrestore(&cec_dev->cec_reg_lock, flags);
		ceca_err_flag = 1;
		return;
	}
	data32 = 0;
	data32 |= 1 << 16; /* [16]	 cec_reg_wr */
	data32 |= data << 8; /* [15:8]   cec_reg_wrdata */
	data32 |= addr << 0; /* [7:0]	cec_reg_addr */
	write_ao(AO_CEC_RW_REG, data32);
	spin_unlock_irqrestore(&cec_dev->cec_reg_lock, flags);
} /* aocec_wr_only_reg */

/*------------for AO_CECB------------------*/
static unsigned int aocecb_rd_reg(unsigned long addr)
{
	unsigned int data32;
	unsigned long flags;
	unsigned int timeout = 0;

	spin_lock_irqsave(&cec_dev->cec_reg_lock, flags);
	data32 = 0;
	data32 |= 0 << 16; /* [16]	 cec_reg_wr */
	data32 |= 0 << 8; /* [15:8]   cec_reg_wrdata */
	data32 |= addr << 0; /* [7:0]	cec_reg_addr */
	write_ao(AO_CECB_RW_REG, data32);
	/* add for check access busy */
	data32 = read_ao(AO_CECB_RW_REG);
	while (data32 & (1 << 23)) {
		if (timeout++ > 200) {
			CEC_ERR("cecb access reg 0x%x fail\n",
				(unsigned int)addr);
			break;
		}
		data32 = read_ao(AO_CECB_RW_REG);
	}
	data32 = (data32 >> 24) & 0xff;
	spin_unlock_irqrestore(&cec_dev->cec_reg_lock, flags);
	return data32;
} /* aocecb_rd_reg */

static void aocecb_wr_reg(unsigned long addr, unsigned long data)
{
	unsigned long data32;
	unsigned long flags;

	spin_lock_irqsave(&cec_dev->cec_reg_lock, flags);
	data32 = 0;
	data32 |= 1 << 16; /* [16]	 cec_reg_wr */
	data32 |= data << 8; /* [15:8]   cec_reg_wrdata */
	data32 |= addr << 0; /* [7:0]	cec_reg_addr */
	write_ao(AO_CECB_RW_REG, data32);
	spin_unlock_irqrestore(&cec_dev->cec_reg_lock, flags);
} /* aocecb_wr_only_reg */

/*----------------- low level for EE cec rx/tx support ----------------*/
static inline void hdmirx_set_bits_top(u32 reg, u32 bits,
				       u32 start, u32 len)
{
	unsigned int tmp;

	tmp = hdmirx_rd_top(reg);
	tmp &= ~(((1 << len) - 1) << start);
	tmp |=  (bits << start);
	hdmirx_wr_top(reg, tmp);
}

static unsigned int hdmirx_cec_read(unsigned int reg)
{
	/*
	 * TXLX has moved ee cec to ao domain
	 */
	if (reg >= DWC_CEC_CTRL && cec_dev->plat_data->ee_to_ao)
		return aocecb_rd_reg((reg - DWC_CEC_CTRL) / 4);
	else
		return hdmirx_rd_dwc(reg);
}

/*only for ee cec*/
static void hdmirx_cec_write(unsigned int reg, unsigned int value)
{
	/*
	 * TXLX has moved ee cec to ao domain
	 */
	if (reg >= DWC_CEC_CTRL && cec_dev->plat_data->ee_to_ao)
		aocecb_wr_reg((reg - DWC_CEC_CTRL) / 4, value);
	else
		hdmirx_wr_dwc(reg, value);
}

static inline void hdmirx_set_bits_dwc(u32 reg, u32 bits,
				       u32 start, u32 len)
{
	unsigned int tmp;

	tmp = hdmirx_cec_read(reg);
	tmp &= ~(((1 << len) - 1) << start);
	tmp |=  (bits << start);
	hdmirx_cec_write(reg, tmp);
}

const char *cec_tx_ret_str(int ret)
{
	switch (ret) {
	case CEC_FAIL_NONE:
		return "RET_NONE";
	case CEC_FAIL_NACK:
		return "RET_NACK";
	case CEC_FAIL_BUSY:
		return "RET_BUSY";
	case CEC_FAIL_OTHER:
	default:
		return "RET_OTHER";
	}
}

void cec_dbg_init(void)
{
	stdbgflg.hal_cmd_bypass = 0;
}

void cec_store_msg_to_buff(unsigned char len, unsigned char *msg)
{
	unsigned int i;
	unsigned int msg_idx;

	if (!cec_dev)
		return;

	if (cec_dev->wakeup_st && len < MAX_MSG &&
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

void cecb_hw_reset(void)
{
	/* cec disable */
	if (!cec_dev->plat_data->ee_to_ao) {
		hdmirx_set_bits_dwc(DWC_DMI_DISABLE_IF, 0, 5, 1);
	} else {
		cec_set_reg_bits(AO_CECB_GEN_CNTL, 1, 0, 1);
		cec_set_reg_bits(AO_CECB_GEN_CNTL, 0, 0, 1);
	}
	udelay(9);
	dprintk(L_4, "%s\n", __func__);
}

static void cecrx_check_irq_enable(void)
{
	unsigned int reg32;

	/* irq on chip txlx has sperate from EE cec, no need check */
	if (cec_dev->plat_data->ee_to_ao)
		return;

	reg32 = hdmirx_cec_read(DWC_AUD_CEC_IEN);
	if ((reg32 & EE_CEC_IRQ_EN_MASK) != EE_CEC_IRQ_EN_MASK) {
		CEC_INFO("irq_en is wrong:%x\n", reg32);
		hdmirx_cec_write(DWC_AUD_CEC_IEN_SET, EE_CEC_IRQ_EN_MASK);
	}
}

static int cecb_trigle_tx(const unsigned char *msg, unsigned char len)
{
	int i = 0, size = 0;
	int lock;

	cecrx_check_irq_enable();
	while (1) {
		/* send is in process */
		lock = hdmirx_cec_read(DWC_CEC_LOCK);
		if (lock) {
			CEC_ERR("recevie msg in tx\n");
			cecb_irq_handle();
			return -1;
		}
		if (hdmirx_cec_read(DWC_CEC_CTRL) & 0x01)
			i++;
		else
			break;
		if (i > 25) {
			CEC_ERR("waiting busy timeout\n");
			return -1;
		}
		msleep(20);
	}
	size += sprintf(msg_log_buf + size, "cecb: tx len: %d data:", len);
	for (i = 0; i < len; i++) {
		hdmirx_cec_write(DWC_CEC_TX_DATA0 + i * 4, msg[i]);
		size += sprintf(msg_log_buf + size, " %02x", msg[i]);
	}
	msg_log_buf[size] = '\0';
	CEC_INFO("%s\n", msg_log_buf);
	/* start send */
	hdmirx_cec_write(DWC_CEC_TX_CNT, len);
	hdmirx_set_bits_dwc(DWC_CEC_CTRL, 3, 0, 3);
	return 0;
}

int cec_has_irq(void)
{
	unsigned int intr_cec;

	if (!cec_dev->plat_data->ee_to_ao) {
		intr_cec = hdmirx_cec_read(DWC_AUD_CEC_ISTS);
		intr_cec &= EE_CEC_IRQ_EN_MASK;
	} else {
		intr_cec = read_ao(AO_CECB_INTR_STAT);
		intr_cec &= CECB_IRQ_EN_MASK;
	}
	return intr_cec;
}

static inline void cecrx_clear_irq(unsigned int flags)
{
	if (!cec_dev->plat_data->ee_to_ao)
		hdmirx_cec_write(DWC_AUD_CEC_ICLR, flags);
	else
		write_ao(AO_CECB_INTR_CLR, flags);
}

/* max length = 14+1 */
#define OSD_NAME_DEV	1
const u8 dev_osd_name[1][16] = {
	{1, 0x43, 0x68, 0x72, 0x6f, 0x6d, 0x65, 0x63, 0x61, 0x73, 0x74},
};

const u8 dev_vendor_id[1][3] = {
	{0, 0, 0},
};

/*for bringup no this api*/
int __attribute__((weak))cec_set_dev_info(uint8_t dev_idx)
{
	return 0;
}

int __attribute__((weak))hdmirx_get_connect_info(void)
{
	return 0;
}

static bool cec_message_op(unsigned char *msg, unsigned char len)
{
	int i, j;

	if (((msg[0] & 0xf0) >> 4) == cec_dev->cec_info.log_addr) {
		CEC_ERR("bad iniator with self 0x%x",
			cec_dev->cec_info.log_addr);
		return false;
	}
	switch (msg[1]) {
	case 0x47:
		/* OSD name */
		if (len > 16)
			break;
		for (j = 0; j < OSD_NAME_DEV; j++) {
			for (i = 2; i < len; i++) {
				if (msg[i] != dev_osd_name[j][i - 1])
					break;
			}
			if (i == len) {
			#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_HDMI
				cec_set_dev_info(dev_osd_name[j][0]);
			#endif
				CEC_INFO("specific dev:%d", dev_osd_name[j][0]);
			}
		}
		break;
	case 0x87:
		/* verdor ID */
		break;
	default:
		break;
	}
	return true;
}

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

	intr_cec = cec_has_irq();

	/* clear irq */
	if (intr_cec != 0)
		cecrx_clear_irq(intr_cec);
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

static void ao_cecb_init(void)
{
	unsigned long data32;
	unsigned int reg;

	cecb_hw_reset();

	if (!cec_dev->plat_data->ee_to_ao) {
		/* set cec clk 32768k */
		data32  = read_hiu(HHI_32K_CLK_CNTL);
		data32  = 0;
		/*
		 * [17:16] clk_sel: 0=oscin; 1=slow_oscin;
		 * 2=fclk_div3; 3=fclk_div5.
		 */
		data32 |= 0         << 16;
		/* [   15] clk_en */
		data32 |= 1         << 15;
		/* [13: 0] clk_div */
		data32 |= (732 - 1)   << 0;
		write_hiu(HHI_32K_CLK_CNTL, data32);
		hdmirx_wr_top(TOP_EDID_ADDR_CEC, EDID_CEC_ID_ADDR);

		/* hdmirx_cecclk_en */
		hdmirx_set_bits_top(TOP_CLK_CNTL, 1, 2, 1);
		hdmirx_set_bits_top(TOP_EDID_GEN_CNTL, EDID_AUTO_CEC_EN, 11, 1);

		/* enable all cec irq */
		/*cec_irq_enable(true);*/
		/* clear all wake up source */
		hdmirx_cec_write(DWC_CEC_WKUPCTRL, 0);
		/* cec enable */
		hdmirx_set_bits_dwc(DWC_DMI_DISABLE_IF, 1, 5, 1);
	} else {
		if (cec_dev->plat_data->chip_id <= CEC_CHIP_A1) {
			/*ao cec b set clk*/
			reg =   (0 << 31) |
				(0 << 30) |
				/* clk_div0/clk_div1 in turn */
				(1 << 28) |
				/* Div_tcnt1 */
				((732 - 1) << 12) |
				/* Div_tcnt0 */
				((733 - 1) << 0);
			write_ao(AO_CECB_CLK_CNTL_REG0, reg);
			reg =   (0 << 13) |
				((11 - 1)  << 12) |
				((8 - 1)  <<  0);
			write_ao(AO_CECB_CLK_CNTL_REG1, reg);

			reg = read_ao(AO_CECB_CLK_CNTL_REG0);
			reg |= (1 << 31);
			write_ao(AO_CECB_CLK_CNTL_REG0, reg);

			/*usleep_range(100);*/
			reg |= (1 << 30);
			write_ao(AO_CECB_CLK_CNTL_REG0, reg);

			read_ao(AO_RTI_PWR_CNTL_REG0);
			reg |=  (0x01 << 14);	/* xtal gate */
			write_ao(AO_RTI_PWR_CNTL_REG0, reg);
		}

		data32  = 0;
		data32 |= (7 << 12);	/* filter_del */
		data32 |= (1 <<  8);	/* filter_tick: 1us */
		data32 |= (1 <<  3);	/* enable system clock */
		data32 |= 0 << 1;	/* [2:1]	cntl_clk: */
			/* 0=Disable clk (Power-off mode); */
			/* 1=Enable gated clock (Normal mode); */
			/* 2=Enable free-run clk (Debug mode). */
		data32 |= 1 << 0;	/* [0]	  sw_reset: 1=Reset */
		write_ao(AO_CECB_GEN_CNTL, data32);
		/* Enable gated clock (Normal mode). */
		cec_set_reg_bits(AO_CECB_GEN_CNTL, 1, 1, 1);
		/* Release SW reset */
		cec_set_reg_bits(AO_CECB_GEN_CNTL, 0, 0, 1);

		if (cec_dev->plat_data->cecb_ver >= CECB_VER_2) {
			reg = 0;
			reg |= (0 << 6);/*curb_err_init*/
			reg |= (0 << 5);/*en_chk_sbitlow*/
			reg |= (CEC_B_ARB_TIME << 0);/*rise_del_max*/
			hdmirx_cec_write(DWC_CEC_CTRL2, reg);
		}

		/* Enable all AO_CECB interrupt sources */
		/*cec_irq_enable(true);*/
		hdmirx_cec_write(DWC_CEC_WKUPCTRL, WAKEUP_EN_MASK);
	}
}

void eecec_irq_enable(bool enable)
{
	u32 tmp;

	if (cec_dev->plat_data->chip_id < CEC_CHIP_TXLX) {
		if (enable) {
			hdmirx_cec_write(DWC_AUD_CEC_IEN_SET,
					 EE_CEC_IRQ_EN_MASK);
		} else {
			hdmirx_cec_write(DWC_AUD_CEC_ICLR,
					 (~(hdmirx_cec_read(DWC_AUD_CEC_IEN)) |
					 EE_CEC_IRQ_EN_MASK));
			hdmirx_cec_write(DWC_AUD_CEC_IEN_SET,
					 hdmirx_cec_read(DWC_AUD_CEC_IEN) &
					 ~EE_CEC_IRQ_EN_MASK);
			hdmirx_cec_write(DWC_AUD_CEC_IEN_CLR,
					 (~(hdmirx_cec_read(DWC_AUD_CEC_IEN)) |
					  EE_CEC_IRQ_EN_MASK));
		}
		CEC_INFO("%s-0:int %d mask:0x%x\n", __func__, enable,
			 hdmirx_cec_read(DWC_AUD_CEC_IEN));
	} else {
		if (enable) {
			write_ao(AO_CECB_INTR_MASKN, CECB_IRQ_EN_MASK);
		} else {
			tmp = read_ao(AO_CECB_INTR_MASKN);
			tmp &= ~CECB_IRQ_EN_MASK;
			write_ao(AO_CECB_INTR_MASKN, tmp);
		}
		CEC_INFO("%s-1:int %d mask:0x%x\n", __func__, enable,
			 read_ao(AO_CECB_INTR_MASKN));
	}
}

void cec_irq_enable(bool enable)
{
	if (cec_dev->cec_num > ENABLE_ONE_CEC) {
		eecec_irq_enable(enable);
		aocec_irq_enable(enable);
	} else {
		if (ee_cec == CEC_B)
			eecec_irq_enable(enable);
		else
			aocec_irq_enable(enable);
	}
}

int dump_cecrx_reg(char *b)
{
	int i = 0, s = 0;
	unsigned char reg;
	unsigned int reg32;

	if (ee_cec == CEC_A) {
		reg32 = read_ao(AO_CEC_CLK_CNTL_REG0);
		s += sprintf(b + s, "AO_CEC_CLK_CNTL_REG0:0x%08x\n",
			reg32);
		reg32 = read_ao(AO_CEC_CLK_CNTL_REG1);
		s += sprintf(b + s, "AO_CEC_CLK_CNTL_REG1:0x%08x\n",
			reg32);
		reg32 = read_ao(AO_CEC_GEN_CNTL);
		s += sprintf(b + s, "AO_CEC_GEN_CNTL:	0x%08x\n",
			reg32);
		reg32 = read_ao(AO_CEC_RW_REG);
		s += sprintf(b + s, "AO_CEC_RW_REG:	0x%08x\n",
			reg32);
		reg32 = read_ao(AO_CEC_INTR_MASKN);
		s += sprintf(b + s, "AO_CEC_INTR_MASKN:0x%08x\n",
			reg32);
		reg32 = read_ao(AO_CEC_INTR_STAT);
		s += sprintf(b + s, "AO_CEC_INTR_STAT: 0x%08x\n",
			reg32);
	}

	if (cec_dev->plat_data->chip_id == CEC_CHIP_A1) {
		s += sprintf(b + s, "%2x:%2x\n", 0xe4, read_periphs(0xe4));
		s += sprintf(b + s, "%2x:%2x\n", 0xe8, read_periphs(0xe8));
		s += sprintf(b + s, "%2x:%2x\n", 0xec, read_periphs(0xec));
		s += sprintf(b + s, "%2x:%2x\n", 0xf0, read_periphs(0xf0));
	} else if (cec_dev->plat_data->chip_id == CEC_CHIP_SC2 ||
		   cec_dev->plat_data->chip_id == CEC_CHIP_S4) {
		s += sprintf(b + s, "%2x:%2x\n", 0x22, read_clock(0x22));
		s += sprintf(b + s, "%2x:%2x\n", 0x23, read_clock(0x23));
		s += sprintf(b + s, "%2x:%2x\n", 0x24, read_clock(0x24));
		s += sprintf(b + s, "%2x:%2x\n", 0x25, read_clock(0x25));
	}

	if (ee_cec == CEC_B) {
		reg32 = read_ao(AO_CECB_CLK_CNTL_REG0);
		s += sprintf(b + s,
			     "AO_CECB_CLK_CNTL_REG0:0x%08x\n",
			     reg32);
		reg32 = read_ao(AO_CECB_CLK_CNTL_REG1);
		s += sprintf(b + s,
			     "AO_CECB_CLK_CNTL_REG1:0x%08x\n",
			     reg32);
		reg32 = read_ao(AO_CECB_GEN_CNTL);
		s += sprintf(b + s, "AO_CECB_GEN_CNTL:	0x%08x\n",
			reg32);
		reg32 = read_ao(AO_CECB_RW_REG);
		s += sprintf(b + s, "AO_CECB_RW_REG:	0x%08x\n",
			reg32);
		reg32 = read_ao(AO_CECB_INTR_MASKN);
		s += sprintf(b + s, "AO_CECB_INTR_MASKN:0x%08x\n",
			reg32);
		reg32 = read_ao(AO_CECB_INTR_STAT);
		s += sprintf(b + s, "AO_CECB_INTR_STAT: 0x%08x\n",
			reg32);

		s += sprintf(b + s, "CEC MODULE REGS:\n");
		s += sprintf(b + s, "CEC_CTRL	  = 0x%02x\n",
			hdmirx_cec_read(DWC_CEC_CTRL));
		if (cec_dev->plat_data->cecb_ver >= CECB_VER_2)
			s += sprintf(b + s, "CEC_CTRL2	  = 0x%02x\n",
				hdmirx_cec_read(DWC_CEC_CTRL2));
		s += sprintf(b + s, "CEC_MASK	  = 0x%02x\n",
			hdmirx_cec_read(DWC_CEC_MASK));
		s += sprintf(b + s, "CEC_ADDR_L   = 0x%02x\n",
			hdmirx_cec_read(DWC_CEC_ADDR_L));
		s += sprintf(b + s, "CEC_ADDR_H   = 0x%02x\n",
			hdmirx_cec_read(DWC_CEC_ADDR_H));
		s += sprintf(b + s, "CEC_TX_CNT   = 0x%02x\n",
			hdmirx_cec_read(DWC_CEC_TX_CNT));
		s += sprintf(b + s, "CEC_RX_CNT   = 0x%02x\n",
			hdmirx_cec_read(DWC_CEC_RX_CNT));
		if (cec_dev->plat_data->cecb_ver >= CECB_VER_2)
			s += sprintf(b + s, "CEC_STAT0	 = 0x%02x\n",
				hdmirx_cec_read(DWC_CEC_STAT0));
		s += sprintf(b + s, "CEC_LOCK	  = 0x%02x\n",
			hdmirx_cec_read(DWC_CEC_LOCK));
		s += sprintf(b + s, "CEC_WKUPCTRL = 0x%02x\n",
			hdmirx_cec_read(DWC_CEC_WKUPCTRL));

		s += sprintf(b + s, "%s", "RX buffer:");
		for (i = 0; i < 16; i++) {
			reg = hdmirx_cec_read(DWC_CEC_RX_DATA0 + i * 4) & 0xff;
			s += sprintf(b + s, " %02x", reg);
		}
		s += sprintf(b + s, "\n");

		s += sprintf(b + s, "%s", "TX buffer:");
		for (i = 0; i < 16; i++) {
			reg = hdmirx_cec_read(DWC_CEC_TX_DATA0 + i * 4) & 0xff;
			s += sprintf(b + s, " %02x", reg);
		}
		s += sprintf(b + s, "\n");
	} else {
		for (i = 0; i < ARRAY_SIZE(cec_reg_name1); i++) {
			s += sprintf(b + s, "%s:%2x\n",
				cec_reg_name1[i], aocec_rd_reg(i + 0x10));
		}

		for (i = 0; i < ARRAY_SIZE(cec_reg_name2); i++) {
			s += sprintf(b + s, "%s:%2x\n",
				cec_reg_name2[i], aocec_rd_reg(i + 0x90));
		}

		if (cec_dev->plat_data->ceca_sts_reg) {
			for (i = 0; i < ARRAY_SIZE(ceca_reg_name3); i++) {
				s += sprintf(b + s, "%s:%2x\n",
				 ceca_reg_name3[i], aocec_rd_reg(i + 0xA0));
			}
		}
	}

	return s;
}

/*--------------------- END of EE CEC --------------------*/

void aocec_irq_enable(bool enable)
{
	if (enable)
		cec_set_reg_bits(AO_CEC_INTR_MASKN, 0x6, 0, 3);
	else
		cec_set_reg_bits(AO_CEC_INTR_MASKN, 0x0, 0, 3);
	CEC_INFO("ao enable:int mask:0x%x\n",
		 read_ao(AO_CEC_INTR_MASKN));
}

static void cec_hw_buf_clear(void)
{
	aocec_wr_reg(CEC_RX_MSG_CMD, RX_DISABLE);
	aocec_wr_reg(CEC_TX_MSG_CMD, TX_ABORT);
	aocec_wr_reg(CEC_RX_CLEAR_BUF, 1);
	aocec_wr_reg(CEC_TX_CLEAR_BUF, 1);
	udelay(9);
	aocec_wr_reg(CEC_RX_CLEAR_BUF, 0);
	aocec_wr_reg(CEC_TX_CLEAR_BUF, 0);
	udelay(9);
	aocec_wr_reg(CEC_RX_MSG_CMD, RX_NO_OP);
	aocec_wr_reg(CEC_TX_MSG_CMD, TX_NO_OP);
}

void cec_logicaddr_set(int l_add)
{
	/* save logical address for suspend/wake up */
	cec_config2_logaddr(l_add, 1);

	cec_dev->cec_info.addr_enable = (1 << l_add);
	if (ee_cec == CEC_B) {
		/* set ee_cec logical addr */
		if (l_add < 8)
			hdmirx_cec_write(DWC_CEC_ADDR_L, 1 << l_add);
		else
			hdmirx_cec_write(DWC_CEC_ADDR_H, 1 << (l_add - 8));

	} else {
		if (cec_dev->plat_data->chip_id >= CEC_CHIP_A1) {
			/* set ee_cec logical addr */
			if (l_add < 8)
				aocec_wr_reg(CEC_LOGICAL_ADDR0, 1 << l_add);
			else
				aocec_wr_reg(CEC_LOGICAL_ADDR1,
					     1 << (l_add - 8));
		} else {
			/*clear all logical address*/
			aocec_wr_reg(CEC_LOGICAL_ADDR0, 0);
			aocec_wr_reg(CEC_LOGICAL_ADDR1, 0);
			aocec_wr_reg(CEC_LOGICAL_ADDR2, 0);
			aocec_wr_reg(CEC_LOGICAL_ADDR3, 0);
			aocec_wr_reg(CEC_LOGICAL_ADDR4, 0);

			cec_hw_buf_clear();
			aocec_wr_reg(CEC_LOGICAL_ADDR0, (l_add & 0xf));
			/*udelay(100);*/
			aocec_wr_reg(CEC_LOGICAL_ADDR0,
				(0x1 << 4) | (l_add & 0xf));
		}
	}
}

void ceca_addr_add(unsigned int l_add)
{
	unsigned int addr;
	unsigned int i;

	if (cec_dev->plat_data->chip_id >= CEC_CHIP_A1) {
		if (l_add < 8) {
			addr = aocec_rd_reg(CEC_LOGICAL_ADDR0);
			addr |= (1 << l_add);
			aocec_wr_reg(CEC_LOGICAL_ADDR0, addr);
		} else {
			addr = aocec_rd_reg(CEC_LOGICAL_ADDR1);
			addr |= (1 << (l_add - 8));
			aocec_wr_reg(CEC_LOGICAL_ADDR1, addr);
		}
	} else {
		/* check if the logical addr is exist ? */
		for (i = CEC_LOGICAL_ADDR0; i <= CEC_LOGICAL_ADDR4; i++) {
			addr = aocec_rd_reg(i);
			if ((addr & 0x10) && ((addr & 0xf) == (l_add & 0xf))) {
				return;
			}
		}

		/* find a empty place */
		for (i = CEC_LOGICAL_ADDR0; i <= CEC_LOGICAL_ADDR4; i++) {
			addr = aocec_rd_reg(i);
			if (addr & 0x10) {
				continue;
			} else {
				cec_hw_buf_clear();
				aocec_wr_reg(i, (l_add & 0xf));
				/*udelay(100);*/
				aocec_wr_reg(i, (l_add & 0xf) | 0x10);
				break;
			}
		}
	}
}

void cecb_addr_add(unsigned int l_add)
{
	unsigned int addr;

	if (l_add < 8) {
		addr = hdmirx_cec_read(DWC_CEC_ADDR_L);
		addr |= (1 << l_add);
		hdmirx_cec_write(DWC_CEC_ADDR_L, addr);
	} else {
		addr = hdmirx_cec_read(DWC_CEC_ADDR_H);
		addr |= (1 << (l_add - 8));
		hdmirx_cec_write(DWC_CEC_ADDR_H, addr);
	}
	CEC_INFO("cec b add addr %d\n", l_add);
}

void cec_logicaddr_add(unsigned int cec_sel, unsigned int l_add)
{
	/* save logical address for suspend/wake up */
	cec_config2_logaddr(l_add, 1);

	if (cec_sel == CEC_B)
		cecb_addr_add(l_add);
	else
		ceca_addr_add(l_add);
}

void cec_logicaddr_remove(unsigned int cec_sel, unsigned int l_add)
{
	unsigned int addr;
	unsigned int i;

	if (cec_sel == CEC_B) {
		if (l_add < 8) {
			addr = hdmirx_cec_read(DWC_CEC_ADDR_L);
			addr &= ~(1 << l_add);
			hdmirx_cec_write(DWC_CEC_ADDR_L, addr);
		} else {
			addr = hdmirx_cec_read(DWC_CEC_ADDR_H);
			addr &= ~(1 << (l_add - 8));
			hdmirx_cec_write(DWC_CEC_ADDR_H, addr);
		}
	} else {
		if (cec_dev->plat_data->chip_id >= CEC_CHIP_A1) {
			if (l_add < 8) {
				addr = aocec_rd_reg(CEC_LOGICAL_ADDR0);
				addr &= ~(1 << l_add);
				aocec_wr_reg(CEC_LOGICAL_ADDR0, addr);
			} else {
				addr = aocec_rd_reg(CEC_LOGICAL_ADDR1);
				addr &= ~(1 << (l_add - 8));
				aocec_wr_reg(CEC_LOGICAL_ADDR1, addr);
			}
		} else {
			for (i = CEC_LOGICAL_ADDR0;
				 i <= CEC_LOGICAL_ADDR4; i++) {
				addr = aocec_rd_reg(i);
				if ((addr & 0xf) == (l_add & 0xf)) {
					aocec_wr_reg(i, (addr & 0xf));
					/*udelay(100);*/
					aocec_wr_reg(i, 0);
					cec_hw_buf_clear();
				}
			}
		}
	}
}

void cec_restore_logical_addr(unsigned int cec_sel, unsigned int addr_en)
{
	unsigned int i;
	unsigned int addr_enable = addr_en;

	cec_clear_all_logical_addr(cec_sel);
	for (i = 0; i < 15; i++) {
		if (addr_enable & 0x1)
			cec_logicaddr_add(cec_sel, i);

		addr_enable = addr_enable >> 1;
	}
	dprintk(L_4, "%s cec:%d, en:%d\n", __func__, cec_sel, addr_en);
}

void ceca_hw_reset(void)
{
	write_ao(AO_CEC_GEN_CNTL, 0x1);
	/* Enable gated clock (Normal mode). */
	cec_set_reg_bits(AO_CEC_GEN_CNTL, 1, 1, 1);
	/* Release SW reset */
	udelay(9);
	cec_set_reg_bits(AO_CEC_GEN_CNTL, 0, 0, 1);

	/* Enable all AO_CEC interrupt sources */
	/*cec_irq_enable(true);*/

	/* cec_logicaddr_set(cec_dev->cec_info.log_addr); */

	/* Cec arbitration 3/5/7 bit time set. */
	cec_arbit_bit_time_set(3, 0x118, 0);
	cec_arbit_bit_time_set(5, 0x000, 0);
	cec_arbit_bit_time_set(7, 0x2aa, 0);
}

void cec_hw_reset(unsigned int cec_sel)
{
	unsigned int reg;

	if (cec_sel == CEC_B) {
		cecb_hw_reset();
		/* cec_logicaddr_set(cec_dev->cec_info.log_addr); */
		/* DWC_CEC_CTRL2 will be reset to 0*/
		if (cec_dev->plat_data->cecb_ver >= CECB_VER_2) {
			reg = 0;
			reg |= (0 << 6);/*curb_err_init*/
			reg |= (0 << 5);/*en_chk_sbitlow*/
			reg |= (CEC_B_ARB_TIME << 0);/*rise_del_max*/
			hdmirx_cec_write(DWC_CEC_CTRL2, reg);
		}
	} else {
		ceca_hw_reset();
	}
	/* cec_logicaddr_set(cec_dev->cec_info.log_addr); */
	cec_restore_logical_addr(cec_sel, cec_dev->cec_info.addr_enable);
}

void cec_rx_buf_clear(void)
{
	aocec_wr_reg(CEC_RX_CLEAR_BUF, 0x1);
	aocec_wr_reg(CEC_RX_CLEAR_BUF, 0x0);
}

static inline bool is_poll_message(unsigned char header)
{
	unsigned char initiator, follower;

	initiator = (header >> 4) & 0xf;
	follower  = (header) & 0xf;
	return initiator == follower;
}

static inline bool is_feature_abort_msg(const unsigned char *msg, int len)
{
	if (!msg || len < 2)
		return false;
	if (msg[1] == CEC_OC_FEATURE_ABORT)
		return true;
	return false;
}

static inline bool is_report_phy_addr_msg(const unsigned char *msg, int len)
{
	if (!msg || len < 4)
		return false;
	if (msg[1] == CEC_OC_REPORT_PHYSICAL_ADDRESS)
		return true;
	return false;
}

static bool need_nack_repeat_msg(const unsigned char *msg, int len, int t)
{
	if (len == last_cec_msg->len &&
	    (is_poll_message(msg[0]) || is_feature_abort_msg(msg, len) ||
	      is_report_phy_addr_msg(msg, len)) &&
	    last_cec_msg->last_result == CEC_FAIL_NACK &&
	    jiffies - last_cec_msg->last_jiffies < t) {
		return true;
	}
	return false;
}

void cec_clear_all_logical_addr(unsigned int cec_sel)
{
	CEC_INFO("clear all logical addr %d\n", cec_sel);

	if (cec_sel == CEC_B) {
		hdmirx_cec_write(DWC_CEC_ADDR_L, 0);
		hdmirx_cec_write(DWC_CEC_ADDR_H, 0);
	} else {
		aocec_wr_reg(CEC_LOGICAL_ADDR0, 0);
		aocec_wr_reg(CEC_LOGICAL_ADDR1, 0);
		aocec_wr_reg(CEC_LOGICAL_ADDR2, 0);
		aocec_wr_reg(CEC_LOGICAL_ADDR3, 0);
		aocec_wr_reg(CEC_LOGICAL_ADDR4, 0);
	}
	/*udelay(100);*/
}

void cec_enable_arc_pin(bool enable)
{
	unsigned int data;
	unsigned int chipid = cec_dev->plat_data->chip_id;

	/* box no arc out*/
	if (chipid != CEC_CHIP_TXL &&
	    chipid != CEC_CHIP_TXLX &&
	    chipid != CEC_CHIP_TL1 &&
	    chipid != CEC_CHIP_TM2)
		return;

	/*tm2 later, audio module handle this*/
	if (chipid >= CEC_CHIP_TM2)
		return;

	/* tl1 tm2*/
	if (cec_dev->plat_data->cecb_ver >= CECB_VER_2) {
		data = rd_reg_hhi(HHI_HDMIRX_ARC_CNTL);
		/* enable bit 1:1 bit 0: 0*/
		if (enable)
			data |= 0x02;
		else
			data &= 0xfffffffd;
		wr_reg_hhi(HHI_HDMIRX_ARC_CNTL, data);
		/*CEC_INFO("set arc en:%d, reg:%x\n", enable, data);*/
	} else {
		/* only tv chip select arc according arg */
		if (enable)
			hdmirx_wr_top(TOP_ARCTX_CNTL, 0x01);
		else
			hdmirx_wr_top(TOP_ARCTX_CNTL, 0x00);
		/*CEC_INFO("set arc en:%d, reg:%lx\n",*/
		/*	 enable, hdmirx_rd_top(TOP_ARCTX_CNTL));*/
	}
}
EXPORT_SYMBOL(cec_enable_arc_pin);

int cec_rx_buf_check(void)
{
	unsigned int rx_num_msg = 0;

	if (ee_cec == CEC_B) {
		cecrx_check_irq_enable();
		cecb_irq_handle();
	} else {
		rx_num_msg = aocec_rd_reg(CEC_RX_NUM_MSG);
		if (rx_num_msg)
			CEC_INFO("rx msg num:0x%02x\n", rx_num_msg);
	}
	return rx_num_msg;
}

int ceca_rx_irq_handle(unsigned char *msg, unsigned char *len)
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
		cec_rx_buf_clear();
		return ret;
	}

	*len = aocec_rd_reg(CEC_RX_MSG_LENGTH) + 1;

	for (i = 0; i < (*len) && i < MAX_MSG; i++)
		msg[i] = aocec_rd_reg(CEC_RX_MSG_0_HEADER + i);

	ret = rx_stat;
	/* for resume, cec hal not ready */
	cec_store_msg_to_buff(*len, msg);

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
	cec_rx_buf_clear();
	pin_status = 1;

	/* when use two cec ip, cec a only send msg, discard all rx msg */
	if (cec_dev->cec_num > ENABLE_ONE_CEC) {
		/*CEC_INFO("discard msg\n");*/
		return -1;
	}

	return ret;
}

/************************ cec arbitration cts code **************************/
/* using the cec pin as fiq gpi to assist the bus arbitration */

/* return value: 1: successful	  0: error */
static int ceca_trigle_tx(const unsigned char *msg, int len)
{
	int i;
	unsigned int n;
	int pos;
	int reg;
	unsigned int j = 40;
	unsigned int tx_stat;
	static int cec_timeout_cnt = 1;

	while (1) {
		tx_stat = aocec_rd_reg(CEC_TX_MSG_STATUS);
		if (tx_stat != TX_BUSY)
			break;

		if (!(j--)) {
			CEC_INFO("ceca waiting busy timeout\n");
			aocec_wr_reg(CEC_TX_MSG_CMD, TX_ABORT);
			cec_timeout_cnt++;
			if (cec_timeout_cnt > 0x08)
				cec_hw_reset(CEC_A);
			break;
		}
		msleep(20);
	}

	reg = aocec_rd_reg(CEC_TX_MSG_STATUS);
	if (reg == TX_IDLE || reg == TX_DONE) {
		for (i = 0; i < len; i++)
			aocec_wr_reg(CEC_TX_MSG_0_HEADER + i, msg[i]);

		aocec_wr_reg(CEC_TX_MSG_LENGTH, len - 1);
		aocec_wr_reg(CEC_TX_MSG_CMD, TX_REQ_CURRENT);

		if (cec_msg_dbg_en) {
			pos = 0;
			pos += sprintf(msg_log_buf + pos,
				       "ceca: tx len: %d data: ", len);
			for (n = 0; n < len; n++) {
				pos += sprintf(msg_log_buf + pos,
					       "%02x ", msg[n]);
			}

			pos += sprintf(msg_log_buf + pos, "\n");

			msg_log_buf[pos] = '\0';
			pr_info("%s", msg_log_buf);
		}
		cec_timeout_cnt = 0;
		return 0;
	}
	CEC_ERR("error msg sts:0x%x\n", reg);
	return -1;
}

void ceca_tx_irq_handle(void)
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
		if (cec_msg_dbg_en)
			CEC_ERR("TX ERROR!\n");
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

static int get_line(void)
{
	int reg, ret = -EINVAL;

	/*0xff don't check*/
	if (cec_dev->plat_data->line_reg == 0xff)
		return 1;
	else if (cec_dev->plat_data->line_reg == 1)
		reg = read_periphs(PREG_PAD_GPIO3_I);
	else
		reg = read_ao(AO_GPIO_I);
	ret = (reg & (1 << cec_dev->plat_data->line_bit));

	return ret;
}

static enum hrtimer_restart cec_line_check(struct hrtimer *timer)
{
	if (get_line() == 0)
		cec_line_cnt++;
	hrtimer_forward_now(timer, HR_DELAY(1));
	return HRTIMER_RESTART;
}

static int check_confilct(void)
{
	int i;

	for (i = 0; i < CEC_CHK_BUS_CNT; i++) {
		/*
		 * sleep 20ms and using hrtimer to check cec line every 1ms
		 */
		cec_line_cnt = 0;
		hrtimer_start(&start_bit_check, HR_DELAY(1), HRTIMER_MODE_REL);
		msleep(20);
		hrtimer_cancel(&start_bit_check);
		if (cec_line_cnt == 0)
			break;
		CEC_INFO("line busy:%d\n", cec_line_cnt);
	}
	if (i >= CEC_CHK_BUS_CNT)
		return -EBUSY;
	else
		return 0;
}

static bool check_physical_addr_valid(int timeout)
{
	while (timeout > 0) {
		if (cec_dev->dev_type == CEC_TV_ADDR)
			break;
		if (phy_addr_test)
			break;
		/* physical address for box */
		if (cec_dev->tx_dev->hpd_state &&
		    cec_dev->tx_dev->hdmi_info.vsdb_phy_addr.valid == 0) {
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
int cec_ll_tx(const unsigned char *msg, unsigned char len)
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
	if (check_confilct()) {
		CEC_ERR("bus confilct too long\n");
		mutex_unlock(&cec_dev->cec_tx_mutex);
		return CEC_FAIL_BUSY;
	}

	if (cec_sel == CEC_B)
		ret = cecb_trigle_tx(msg, len);
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

/* -------------------------------------------------------------------------- */
/* AO CEC0 config */
/* -------------------------------------------------------------------------- */
static void ao_ceca_init(void)
{
	unsigned long data32;
	unsigned int reg;

	if (cec_dev->plat_data->chip_id > CEC_CHIP_TXL &&
	    cec_dev->plat_data->chip_id < CEC_CHIP_A1) {
		if (cec_dev->plat_data->ee_to_ao) {
			/*ao cec a set clk*/
			reg = (0 << 31) |
				(0 << 30) |
				/* clk_div0/clk_div1 in turn */
				(1 << 28) |
				/* Div_tcnt1 */
				((732 - 1) << 12) |
				/* Div_tcnt0 */
				((733 - 1) << 0);
			write_ao(AO_CEC_CLK_CNTL_REG0, reg);
			reg = (0 << 13) |
				((11 - 1)  << 12) |
				((8 - 1)	<<	0);
			write_ao(AO_CEC_CLK_CNTL_REG1, reg);
			/*enable clk in*/
			reg = read_ao(AO_CEC_CLK_CNTL_REG0);
			reg |= (1 << 31);
			write_ao(AO_CEC_CLK_CNTL_REG0, reg);
			/*enable clk out*/
			/*usleep_range(100);*/
			reg |= (1 << 30);
			write_ao(AO_CEC_CLK_CNTL_REG0, reg);

			reg = read_ao(AO_RTI_PWR_CNTL_REG0);
			/* enable the crystal clock*/
			reg |=	(0x01 << 14);
			write_ao(AO_RTI_PWR_CNTL_REG0, reg);
		} else {
			reg =	(0 << 31) |
				(0 << 30) |
				(1 << 28) | /* clk_div0/clk_div1 in turn */
				((732 - 1) << 12) | /* Div_tcnt1 */
				((733 - 1) << 0); /* Div_tcnt0 */
			write_ao(AO_RTC_ALT_CLK_CNTL0, reg);
			reg =	(0 << 13) |
				((11 - 1)  << 12) |
				((8 - 1)	<<	0);
			write_ao(AO_RTC_ALT_CLK_CNTL1, reg);
			/*udelay(100);*/
			/*enable clk in*/
			reg = read_ao(AO_RTC_ALT_CLK_CNTL0);
			reg |= (1 << 31);
			write_ao(AO_RTC_ALT_CLK_CNTL0, reg);
			/*enable clk out*/
			/*udelay(100);*/
			reg |= (1 << 30);
			write_ao(AO_RTC_ALT_CLK_CNTL0, reg);

			reg = read_ao(AO_CRT_CLK_CNTL1);
			reg |= (0x800 << 16);/* select cts_rtc_oscin_clk */
			write_ao(AO_CRT_CLK_CNTL1, reg);

			reg = read_ao(AO_RTI_PWR_CNTL_REG0);
			reg &= ~(0x07 << 10);
			reg |=	(0x04 << 10);/* XTAL generate 32k */
			write_ao(AO_RTI_PWR_CNTL_REG0, reg);
		}
	}

	if (cec_dev->plat_data->ee_to_ao) {
		data32	= 0;
		data32 |= (7 << 12);	/* filter_del */
		data32 |= (1 <<  8);	/* filter_tick: 1us */
		data32 |= (1 <<  3);	/* enable system clock*/
		data32 |= 0 << 1;	/* [2:1]	cntl_clk: */
				/* 0=Disable clk (Power-off mode); */
				/* 1=Enable gated clock (Normal mode);*/
				/* 2=Enable free-run clk (Debug mode).*/
		data32 |= 1 << 0;	/* [0]	  sw_reset: 1=Reset*/
		write_ao(AO_CEC_GEN_CNTL, data32);
	} else {
		data32	= 0;
		data32 |= 0 << 1;	/* [2:1]	cntl_clk:*/
				/* 0=Disable clk (Power-off mode);*/
				/* 1=Enable gated clock (Normal mode);*/
				/* 2=Enable free-run clk (Debug mode).*/
		data32 |= 1 << 0;	/* [0]	  sw_reset: 1=Reset */
		write_ao(AO_CEC_GEN_CNTL, data32);
	}

	/* Enable gated clock (Normal mode). */
	cec_set_reg_bits(AO_CEC_GEN_CNTL, 1, 1, 1);
	/* Release SW reset */
	cec_set_reg_bits(AO_CEC_GEN_CNTL, 0, 0, 1);

	/* Enable all AO_CEC interrupt sources */
	/*cec_irq_enable(true);*/
	cec_arbit_bit_time_set(3, 0x118, 0);
	cec_arbit_bit_time_set(5, 0x000, 0);
	cec_arbit_bit_time_set(7, 0x2aa, 0);
	dprintk(L_1, "%s\n", __func__);
}

void cec_arbit_bit_time_set(unsigned int bit_set,
			    unsigned int time_set, unsigned int flag)
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
	return read_ao(AO_CEC_INTR_STAT);
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
	if (wr_flag) {
		write_ao(AO_DEBUG_REG0, value);
		cec_dev->cfg = value;
	}

	return cec_dev->cfg;
}

/*
 *wr_flag: reg AO_DEBUG_REG1 (bit 0-16)
 *	1 write; value valid
 *	0 read;  value invalid
 *		0-15 : phy addr+
 *		16-19: logical address+
 *		20-23: device type+
 */
unsigned int cec_config2_phyaddr(unsigned int value, bool wr_flag)
{
	if (wr_flag)
		cec_set_reg_bits(AO_DEBUG_REG1, value, 0, 16);

	return read_ao(AO_DEBUG_REG1) & 0xffff;
}

/*
 *wr_flag: reg AO_DEBUG_REG1 (bit 16-19)
 *	1 write; value valid
 *	0 read;  value invalid
 *		0-15 : phy addr+
 *		16-19: logical address+
 *		20-23: device type+
 */
unsigned int cec_config2_logaddr(unsigned int value, bool wr_flag)
{
	if (wr_flag)
		cec_set_reg_bits(AO_DEBUG_REG1, value, 16, 4);

	return (read_ao(AO_DEBUG_REG1) >> 16) & 0xf;
}

/*
 *wr_flag: reg AO_DEBUG_REG1 (bit 20-23)
 *	1 write; value valid
 *	0 read;  value invalid
 *		0-15 : phy addr+
 *		16-19: logical address+
 *		20-23: device type+
 */
unsigned int cec_config2_devtype(unsigned int value, bool wr_flag)
{
	if (wr_flag)
		cec_set_reg_bits(AO_DEBUG_REG1, value, 20, 4);

	return (read_ao(AO_DEBUG_REG1) >> 20) & 0xf;
}

void cec_ip_share_io(u32 share, u32 cec_ip)
{
	if (share) {
		if (cec_ip == CEC_A) {
			cec_set_reg_bits(AO_CEC_GEN_CNTL, 1, 4, 1);
			cec_set_reg_bits(AO_CECB_GEN_CNTL, 0, 4, 1);
			/*CEC_ERR("share pin mux to b\n");*/
		} else {
			cec_set_reg_bits(AO_CEC_GEN_CNTL, 0, 4, 1);
			cec_set_reg_bits(AO_CECB_GEN_CNTL, 1, 4, 1);
			/*CEC_ERR("share pin mux to a\n");*/
		}
	} else {
		cec_set_reg_bits(AO_CEC_GEN_CNTL, 0, 4, 1);
		cec_set_reg_bits(AO_CECB_GEN_CNTL, 0, 4, 1);
	}
}

/*
 * cec hw module init before allocate logical address
 */
static void cec_pre_init(void)
{
	if (cec_dev->cec_num > ENABLE_ONE_CEC) {
		ao_ceca_init();
		ao_cecb_init();
		cec_ip_share_io(cec_dev->plat_data->share_io, ee_cec);
	} else {
		if (ee_cec == CEC_B)
			ao_cecb_init();
		else
			ao_ceca_init();
	}

	//need restore all logical address
	if (cec_dev->cec_num > ENABLE_ONE_CEC)
		cec_restore_logical_addr(CEC_B, cec_dev->cec_info.addr_enable);
	else
		cec_restore_logical_addr(ee_cec, cec_dev->cec_info.addr_enable);
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
		cec_rx_buf_clear();
		return 0;
	}
	return 1;
}

void cec_key_report(int suspend)
{
	if (!(cec_config(0, 0) & CEC_FUNC_CFG_AUTO_POWER_ON)) {
		CEC_ERR("auto pw on is off (cfg:0x%x)\n", cec_config(0, 0));
		return;
	}
	#ifdef CEC_FREEZE_WAKE_UP
	if (is_pm_s2idle_mode()) {
		pm_wakeup_event(cec_dev->dbg_dev, 2000);
		CEC_INFO("freeze mode:pm_wakeup_event\n");
	}
	#endif
	input_event(cec_dev->cec_info.remote_cec_dev, EV_KEY, KEY_POWER, 1);
	input_sync(cec_dev->cec_info.remote_cec_dev);
	input_event(cec_dev->cec_info.remote_cec_dev, EV_KEY, KEY_POWER, 0);
	input_sync(cec_dev->cec_info.remote_cec_dev);
	CEC_INFO("event %s %d\n", __func__, suspend);
}

void cec_give_version(unsigned int dest)
{
	unsigned char index = cec_dev->cec_info.log_addr;
	unsigned char msg[3];

	if (dest != 0xf) {
		msg[0] = ((index & 0xf) << 4) | dest;
		msg[1] = CEC_OC_CEC_VERSION;
		msg[2] = cec_dev->cec_info.cec_version;
		cec_ll_tx(msg, 3);
	}
}

void cec_report_physical_address_smp(void)
{
	unsigned char msg[5];
	unsigned char index = cec_dev->cec_info.log_addr;
	unsigned char phy_addr_ab, phy_addr_cd;

	phy_addr_ab = (cec_dev->phy_addr >> 8) & 0xff;
	phy_addr_cd = (cec_dev->phy_addr >> 0) & 0xff;
	msg[0] = ((index & 0xf) << 4) | CEC_BROADCAST_ADDR;
	msg[1] = CEC_OC_REPORT_PHYSICAL_ADDRESS;
	msg[2] = phy_addr_ab;
	msg[3] = phy_addr_cd;
	msg[4] = cec_dev->dev_type;

	cec_ll_tx(msg, 5);
}

void cec_device_vendor_id(void)
{
	unsigned char index = cec_dev->cec_info.log_addr;
	unsigned char msg[5];
	unsigned int vendor_id;

	vendor_id = cec_dev->v_data.vendor_id;
	msg[0] = ((index & 0xf) << 4) | CEC_BROADCAST_ADDR;
	msg[1] = CEC_OC_DEVICE_VENDOR_ID;
	msg[2] = (vendor_id >> 16) & 0xff;
	msg[3] = (vendor_id >> 8) & 0xff;
	msg[4] = (vendor_id >> 0) & 0xff;

	cec_ll_tx(msg, 5);
}

void cec_give_deck_status(unsigned int dest)
{
	unsigned char index = cec_dev->cec_info.log_addr;
	unsigned char msg[3];

	msg[0] = ((index & 0xf) << 4) | dest;
	msg[1] = CEC_OC_DECK_STATUS;
	msg[2] = 0x1a;
	cec_ll_tx(msg, 3);
}

void cec_menu_status_smp(int dest, int status)
{
	unsigned char msg[3];
	unsigned char index = cec_dev->cec_info.log_addr;

	msg[0] = ((index & 0xf) << 4) | dest;
	msg[1] = CEC_OC_MENU_STATUS;
	if (status == DEVICE_MENU_ACTIVE)
		msg[2] = DEVICE_MENU_ACTIVE;
	else
		msg[2] = DEVICE_MENU_INACTIVE;
	cec_ll_tx(msg, 3);
}

void cec_inactive_source(int dest)
{
	unsigned char index = cec_dev->cec_info.log_addr;
	unsigned char msg[4];
	unsigned char phy_addr_ab, phy_addr_cd;

	phy_addr_ab = (cec_dev->phy_addr >> 8) & 0xff;
	phy_addr_cd = (cec_dev->phy_addr >> 0) & 0xff;
	msg[0] = ((index & 0xf) << 4) | dest;
	msg[1] = CEC_OC_INACTIVE_SOURCE;
	msg[2] = phy_addr_ab;
	msg[3] = phy_addr_cd;

	cec_ll_tx(msg, 4);
}

void cec_set_osd_name(int dest)
{
	unsigned char index = cec_dev->cec_info.log_addr;
	unsigned char osd_len = strlen(cec_dev->cec_info.osd_name);
	unsigned char msg[16];

	if (dest != 0xf) {
		msg[0] = ((index & 0xf) << 4) | dest;
		msg[1] = CEC_OC_SET_OSD_NAME;
		memcpy(&msg[2], cec_dev->cec_info.osd_name, osd_len);

		cec_ll_tx(msg, 2 + osd_len);
	}
}

void cec_active_source_smp(void)
{
	unsigned char msg[4];
	unsigned char index = cec_dev->cec_info.log_addr;
	unsigned char phy_addr_ab;
	unsigned char phy_addr_cd;

	phy_addr_ab = (cec_dev->phy_addr >> 8) & 0xff;
	phy_addr_cd = (cec_dev->phy_addr >> 0) & 0xff;
	msg[0] = ((index & 0xf) << 4) | CEC_BROADCAST_ADDR;
	msg[1] = CEC_OC_ACTIVE_SOURCE;
	msg[2] = phy_addr_ab;
	msg[3] = phy_addr_cd;
	cec_ll_tx(msg, 4);
}

void cec_request_active_source(void)
{
	unsigned char msg[2];
	unsigned char index = cec_dev->cec_info.log_addr;

	msg[0] = ((index & 0xf) << 4) | CEC_BROADCAST_ADDR;
	msg[1] = CEC_OC_REQUEST_ACTIVE_SOURCE;
	cec_ll_tx(msg, 2);
}

void cec_set_stream_path(unsigned char *msg)
{
	unsigned int phy_addr_active;

	phy_addr_active = (unsigned int)(msg[2] << 8 | msg[3]);
	if (phy_addr_active == cec_dev->phy_addr) {
		cec_active_source_smp();
		/*
		 * some types of TV such as panasonic need to send menu status,
		 * otherwise it will not send remote key event to control
		 * device's menu
		 */
		cec_menu_status_smp(msg[0] >> 4, DEVICE_MENU_ACTIVE);
	}
}

void cec_report_power_status(int dest, int status)
{
	unsigned char index = cec_dev->cec_info.log_addr;
	unsigned char msg[3];

	msg[0] = ((index & 0xf) << 4) | dest;
	msg[1] = CEC_OC_REPORT_POWER_STATUS;
	msg[2] = status;
	cec_ll_tx(msg, 3);
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
	cec_dev->cec_info.vendor_id = cec_dev->v_data.vendor_id;
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

static void ceca_tasklet_pro(unsigned long arg)
{
	unsigned int intr_stat = 0;
	struct delayed_work *dwork;

	dwork = &cec_dev->cec_work;
	intr_stat = ao_cec_intr_stat();
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

	tx_hpd = cec_dev->tx_dev->hpd_state;
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

	tx_hpd = cec_dev->tx_dev->hpd_state;
	if (cec_dev->dev_type != CEC_TV_ADDR) {
		if (!tx_hpd) {
			pin_status = 0;
			return sprintf(buf, "%s\n", "disconnected");
		}
		if (pin_status == 0) {
			p = (cec_dev->cec_info.log_addr << 4) | CEC_TV_ADDR;
			if (cec_ll_tx(&p, 1) == CEC_FAIL_NONE)
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
	ret = cec_ll_tx(buf, cnt);
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
		cecb_trigle_tx(buf, cnt);

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

	if (!cec_dev)
		return 0;

	if (cec_dev->msg_num) {
		cec_dev->msg_num--;
		idx = cec_dev->msg_idx;
		len = cec_dev->msgbuff[idx].len;
		cec_dev->msg_idx++;
		if (copy_to_user(buf, &cec_dev->msgbuff[idx].msg[0], len))
			return -EINVAL;
		CEC_INFO("read msg from buff len=%d\n", len);
		return len;
	}

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
	if ((size > 0 && size < 16) && tempbuf[1] == CEC_OC_SET_OSD_NAME) {
		memset(cec_dev->cec_info.osd_name, 0, 16);
		memcpy(cec_dev->cec_info.osd_name, &tempbuf[2], (size - 2));
		cec_dev->cec_info.osd_name[15] = (size - 2);
	}

	cec_cfg = cec_config(0, 0);
	if (cec_cfg & CEC_FUNC_CFG_CEC_ON) {
		/*cec module on*/
		ret = cec_ll_tx(tempbuf, size);
	}
	return ret;
}

static void init_cec_port_info(struct hdmi_port_info *port,
			       struct ao_cec_dev *cec_dev)
{
	unsigned int a, b, c = 0, d, e = 0;
	unsigned int phy_head = 0xf000, phy_app = 0x1000, phy_addr;
	struct hdmitx_dev *tx_dev;

	/* physical address for TV or repeator */
	tx_dev = cec_dev->tx_dev;
	if (!tx_dev || cec_dev->dev_type == CEC_TV_ADDR) {
		phy_addr = 0;
	} else if (tx_dev->hdmi_info.vsdb_phy_addr.valid == 1) {
		/* get phy address from tx module */
		a = tx_dev->hdmi_info.vsdb_phy_addr.a;
		b = tx_dev->hdmi_info.vsdb_phy_addr.b;
		c = tx_dev->hdmi_info.vsdb_phy_addr.c;
		d = tx_dev->hdmi_info.vsdb_phy_addr.d;
		phy_addr = ((a << 12) | (b << 8) | (c << 4) | (d));
	} else {
		phy_addr = 0;
	}

	/* found physical address append for repeator */
	for (a = 0; a < 4; a++) {
		if (phy_addr & phy_head) {
			phy_head >>= 4;
			phy_app  >>= 4;
		} else {
			break;
		}
	}

	dprintk(L_3, "%s phy_addr:%x, port num:%x\n", __func__, phy_addr,
		   cec_dev->port_num);
	dprintk(L_3, "port_seq=0x%x\n", cec_dev->port_seq);
	/* init for port info */
	for (a = 0; a < sizeof(cec_dev->port_seq) * 2; a++) {
		/* set port physical address according port sequence */
		if (cec_dev->port_seq) {
			c = (cec_dev->port_seq >> (4 * a)) & 0xf;
			if (c == 0xf) { /* not used */
				CEC_INFO("port %d is not used\n", a);
				continue;
			}
			port[e].physical_address = (c) * phy_app + phy_addr;
		} else {
			/* asending order if port_seq is not set */
			port[e].physical_address = (a + 1) * phy_app + phy_addr;
		}

		/* select input / output port*/
		if ((e + cec_dev->output) == cec_dev->port_num) {
			port[e].physical_address = phy_addr;
			port[e].port_id = 0;
			port[e].type = HDMI_OUTPUT;
		} else {
			port[e].type = HDMI_INPUT;
			port[e].port_id = c;/*a + 1; phy port - ui id*/
		}
		port[e].cec_supported = 1;
		/* set ARC feature according mask */
		if (cec_dev->arc_port & (1 << e))
			port[e].arc_supported = 1;
		else
			port[e].arc_supported = 0;
		dprintk(L_3, "portinfo id:%d arc:%d phy:%x,type:%d\n",
			   port[e].port_id, port[e].arc_supported,
			   port[e].physical_address,
			   port[e].type);
		e++;
		if (e >= cec_dev->port_num)
			break;
	}
}

void cec_status(void)
{
	struct hdmi_port_info *port;

	CEC_ERR("driver date:%s\n", CEC_DRIVER_VERSION);
	/*CEC_ERR("chip type:0x%x\n",*/
	/*	get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR));*/
	CEC_ERR("ee_cec:%d\n", ee_cec);
	CEC_ERR("cec_num:%d\n", cec_dev->cec_num);
	CEC_ERR("dev_type:%d\n", (unsigned int)cec_dev->dev_type);
	CEC_ERR("wk_logic_addr:0x%x\n", cec_dev->wakup_data.wk_logic_addr);
	CEC_ERR("wk_phy_addr:0x%x\n", cec_dev->wakup_data.wk_phy_addr);
	CEC_ERR("wk_port_id:0x%x\n", cec_dev->wakup_data.wk_port_id);
	CEC_ERR("wakeup_reason:0x%x\n", cec_dev->wakeup_reason);
	CEC_ERR("phy_addr:0x%x\n", cec_dev->phy_addr);
	CEC_ERR("cec_version:0x%x\n", cec_dev->cec_info.cec_version);
	CEC_ERR("hal_ctl:0x%x\n", cec_dev->cec_info.hal_ctl);
	CEC_ERR("menu_lang:0x%x\n", cec_dev->cec_info.menu_lang);
	CEC_ERR("menu_status:0x%x\n", cec_dev->cec_info.menu_status);
	CEC_ERR("open_count:%d\n", cec_dev->cec_info.open_count.counter);
	CEC_ERR("vendor_id:0x%x\n", cec_dev->v_data.vendor_id);
	CEC_ERR("port_num:0x%x\n", cec_dev->port_num);
	CEC_ERR("output:0x%x\n", cec_dev->output);
	CEC_ERR("arc_port:0x%x\n", cec_dev->arc_port);
	CEC_ERR("hal_flag:0x%x\n", cec_dev->hal_flag);
	CEC_ERR("hpd_state:0x%x\n", cec_dev->tx_dev->hpd_state);
	CEC_ERR("cec_config:0x%x\n", cec_config(0, 0));
	CEC_ERR("log_addr:0x%x\n", cec_dev->cec_info.log_addr);

	CEC_ERR("id:0x%x\n", cec_dev->plat_data->chip_id);
	CEC_ERR("ceca_ver:0x%x\n", cec_dev->plat_data->ceca_ver);
	CEC_ERR("cecb_ver:0x%x\n", cec_dev->plat_data->cecb_ver);
	CEC_ERR("share_io:0x%x\n", cec_dev->plat_data->share_io);
	CEC_ERR("line_bit:0x%x\n", cec_dev->plat_data->line_bit);
	CEC_ERR("ee_to_ao:0x%x\n", cec_dev->plat_data->ee_to_ao);
	CEC_ERR("irq_ceca:0x%x\n", cec_dev->irq_ceca);
	CEC_ERR("irq_cecb:0x%x\n", cec_dev->irq_cecb);

	port = kcalloc(cec_dev->port_num, sizeof(*port), GFP_KERNEL);
	if (port) {
		init_cec_port_info(port, cec_dev);
		kfree(port);
	}

	if (cec_dev->cec_num > ENABLE_ONE_CEC) {
		CEC_ERR("B addrL 0x%x\n", hdmirx_cec_read(DWC_CEC_ADDR_L));
		CEC_ERR("B addrH 0x%x\n", hdmirx_cec_read(DWC_CEC_ADDR_H));

		CEC_ERR("A addr0 0x%x\n", aocec_rd_reg(CEC_LOGICAL_ADDR0));
		CEC_ERR("A addr1 0x%x\n", aocec_rd_reg(CEC_LOGICAL_ADDR1));
		/*CEC_ERR("addr2 0x%x\n", aocec_rd_reg(CEC_LOGICAL_ADDR2));*/
		/*CEC_ERR("addr3 0x%x\n", aocec_rd_reg(CEC_LOGICAL_ADDR3));*/
		/*CEC_ERR("addr4 0x%x\n", aocec_rd_reg(CEC_LOGICAL_ADDR4));*/
	} else {
		if (ee_cec == CEC_B) {
			CEC_ERR("addrL 0x%x\n",
				hdmirx_cec_read(DWC_CEC_ADDR_L));
			CEC_ERR("addrH 0x%x\n",
				hdmirx_cec_read(DWC_CEC_ADDR_H));
		} else {
			CEC_ERR("addr0 0x%x\n",
				aocec_rd_reg(CEC_LOGICAL_ADDR0));
			CEC_ERR("addr1 0x%x\n",
				aocec_rd_reg(CEC_LOGICAL_ADDR1));
			CEC_ERR("addr2 0x%x\n",
				aocec_rd_reg(CEC_LOGICAL_ADDR2));
			CEC_ERR("addr3 0x%x\n",
				aocec_rd_reg(CEC_LOGICAL_ADDR3));
			CEC_ERR("addr4 0x%x\n",
				aocec_rd_reg(CEC_LOGICAL_ADDR4));
		}
	}
	CEC_ERR("addr_enable:0x%x\n", cec_dev->cec_info.addr_enable);
}

void cec_ap_clear_logical_addr(void)
{
	if (cec_dev->cec_num > ENABLE_ONE_CEC)
		cec_clear_all_logical_addr(CEC_B);
	else
		cec_clear_all_logical_addr(ee_cec);
	cec_dev->cec_info.addr_enable = 0;
}

void cec_ap_add_logical_addr(u32 l_addr)
{
	/*cec_logicaddr_set(tmp);*/
	/*cec_logicaddr_add(ee_cec, tmp);*/
	if (cec_dev->cec_num > ENABLE_ONE_CEC)
		cec_logicaddr_add(CEC_B, l_addr);
	else
		cec_logicaddr_add(ee_cec, l_addr);
	cec_dev->cec_info.addr_enable |= (1 << l_addr);

	/* add by hal, to init some data structure */
	cec_dev->cec_info.log_addr = l_addr;
	cec_dev->cec_info.power_status = CEC_PW_POWER_ON;
	cec_dev->cec_info.vendor_id = cec_dev->v_data.vendor_id;
	strncpy(cec_dev->cec_info.osd_name,
		cec_dev->v_data.cec_osd_string, 14);
}

void cec_ap_rm_logical_addr(u32 addr)
{
	cec_dev->cec_info.addr_enable &= ~(1 << (addr & 0xf));
	if (cec_dev->cec_num > 1)
		cec_logicaddr_remove(CEC_B, addr);
	else
		cec_logicaddr_remove(ee_cec, addr);
}

void cec_ap_set_dev_type(u32 type)
{
	cec_dev->dev_type = type & 0xf;
}

unsigned int cec_get_cur_phy_addr(void)
{
		struct hdmitx_dev *tx_dev;
		unsigned int a, b, c, d;
		unsigned int tmp = 0, i;

		tx_dev = cec_dev->tx_dev;
		if (!tx_dev || cec_dev->dev_type == CEC_TV_ADDR) {
			tmp = 0;
		} else {
			for (i = 0; i < 5; i++) {
				/*hpd attach and wait read edid*/
				a = tx_dev->hdmi_info.vsdb_phy_addr.a;
				b = tx_dev->hdmi_info.vsdb_phy_addr.b;
				c = tx_dev->hdmi_info.vsdb_phy_addr.c;
				d = tx_dev->hdmi_info.vsdb_phy_addr.d;
				tmp = ((a << 12) | (b << 8) | (c << 4) | (d));

				if (tx_dev->hdmi_info.vsdb_phy_addr.valid == 1)
					break;
				msleep(20);
			}
		}

		return tmp;
}

static long hdmitx_cec_ioctl(struct file *f,
			     unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	unsigned int tmp;
	struct hdmi_port_info *port;
	unsigned int a, i = 0;
	/*struct hdmitx_dev *tx_dev;*/
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
		tmp = cec_dev->v_data.vendor_id;
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
		if (arg)
			cec_dev->hal_flag |= tmp;
		else
			cec_dev->hal_flag &= ~(tmp);

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
			tmp = cec_dev->tx_dev->hpd_state;
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
	if (new_msg)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

/*
 * cec new message wait queue - wake up poll process
 */
void cec_new_msg_push(void)
{
	if (cec_config(0, 0) & CEC_FUNC_CFG_CEC_ON) {
		complete(&cec_dev->rx_ok);
		new_msg = 1;
		wake_up(&cec_msg_wait_queue);
	}
}

/*
 * save param to mailbox, to uboot
 */
void cec_save_mail_box(void)
{
	unsigned int tmp;

	CEC_INFO("%s\n", __func__);
	tmp = cec_get_cur_phy_addr();
	if (cec_dev->dev_type != CEC_TV_ADDR) {
		if (tmp == 0)
			cec_dev->phy_addr = 0xffff;
		else
			cec_dev->phy_addr = tmp;
	} else {
		cec_dev->phy_addr = 0;
	}

	cec_mailbox.cec_config = cec_config(0, 0);
	cec_mailbox.phy_addr = cec_config2_phyaddr(0, 0);
	cec_mailbox.phy_addr |= cec_config2_logaddr(0, 0) << 16;
	cec_mailbox.phy_addr |= cec_config2_devtype(0, 0) << 20;
	CEC_INFO("phy_addr:0x%x", cec_mailbox.phy_addr);

	cec_mailbox.vendor_id = cec_dev->cec_info.vendor_id;
	memcpy(cec_mailbox.osd_name, cec_dev->cec_info.osd_name, 16);
	/*
	 * uboot 2015: cec driver in bl301, transfer cec data to uboot by
	 * SCPI_CMD_SET_USR_DATA at client SCPI_CL_SET_CEC_DATA
	 *
	 * uboot 2019: cec driver in aocpu bl30, transfer cec data to uboot
	 * by SCPI_CMD_SET_CEC_DATA
	 */
	if (cec_dev->plat_data->chip_id >= CEC_CHIP_SC2)
		scpi_send_cec_data(SCPI_CMD_SET_CEC_DATA, (void *)&cec_mailbox,
				   sizeof(struct st_cec_mailbox_data));
	else
		scpi_send_usr_data(SCPI_CL_SET_CEC_DATA, (void *)&cec_mailbox,
				   sizeof(struct st_cec_mailbox_data));
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

	cec_dev->wakeup_st = 0;
	cec_dev->msg_idx = 0;

	cec_dev->cec_suspend = CEC_PW_POWER_ON;
	CEC_ERR("%s, suspend sts:%d\n", __func__, cec_dev->cec_suspend);
}
#endif

#ifdef CONFIG_OF
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
	.line_reg = 0,/*line_reg=0:AO_GPIO_I*/
	.line_bit = 10,
	.ee_to_ao = 1,
	.ceca_sts_reg = 1,
	.ceca_ver = CECA_VER_1,
	.cecb_ver = CECB_VER_2,
	.share_io = true,
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
	.share_io = false,
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

static const struct of_device_id aml_cec_dt_match[] = {
	{
		.compatible = "amlogic, amlogic-aocec",
		.data = &cec_gxl_data,
	},
	{
		.compatible = "amlogic, aocec-txlx",
		.data = &cec_txlx_data,
	},
	{
		.compatible = "amlogic, aocec-g12a",
		.data = &cec_g12a_data,
	},
	{
		.compatible = "amlogic, aocec-g12b",
		.data = &cec_g12b_data,
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
		.compatible = "amlogic, aocec-sm1",
		.data = &cec_sm1_data,
	},
	{
		.compatible = "amlogic, aocec-tm2",
		.data = &cec_tm2_data,
	},
	{
		.compatible = "amlogic, aocec-a1",
		.data = &cec_a1_data,
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

static void cec_set_clk(struct platform_device *pdev)
{
	if (cec_dev->plat_data->chip_id >= CEC_CHIP_A1) {
		cec_dev->ceca_clk = devm_clk_get(&pdev->dev, "ceca_clk");
		if (IS_ERR(cec_dev->ceca_clk)) {
			CEC_INFO("no ceca clk src\n");
			if (cec_dev->plat_data->ceca_ver == CECA_NONE) {
				CEC_INFO("ceca not exist\n");
			} else if (cec_dev->plat_data->chip_id == CEC_CHIP_A1) {
				write_periphs(0xe4, 0xd02db2dc);
				write_periphs(0xe8, 0xa007);
			} else if (cec_dev->plat_data->chip_id ==
				   CEC_CHIP_SC2) {
				write_clock(0x22, 0xd02db2dc);
				write_clock(0x23, 0xa007);
			} else {
				write_ao(AO_CEC_CLK_CNTL_REG0, 0xd02db2dc);
				write_ao(AO_CEC_CLK_CNTL_REG0, 0xa007);
			}
		} else {
			clk_set_rate(cec_dev->ceca_clk, 32768);
			clk_prepare_enable(cec_dev->ceca_clk);
			CEC_INFO("get clka rate:%ld\n",
				 clk_get_rate(cec_dev->ceca_clk));
		}

		cec_dev->cecb_clk = devm_clk_get(&pdev->dev, "cecb_clk");
		if (IS_ERR(cec_dev->cecb_clk)) {
			CEC_INFO("no cecb clk src\n");
			if (cec_dev->plat_data->cecb_ver == CECB_NONE) {
				CEC_INFO("cecb not exist\n");
			} else if (cec_dev->plat_data->chip_id == CEC_CHIP_A1) {
				write_periphs(0xec, 0xd02db2dc);
				write_periphs(0xf0, 0xa007);
			} else if (cec_dev->plat_data->chip_id ==
				   CEC_CHIP_SC2) {
				write_clock(0x24, 0xd02db2dc);
				write_clock(0x25, 0xa007);
			} else {
				write_ao(AO_CECB_CLK_CNTL_REG0, 0xd02db2dc);
				write_ao(AO_CECB_CLK_CNTL_REG1, 0xa007);
			}
		} else {
			clk_set_rate(cec_dev->cecb_clk, 32768);
			clk_prepare_enable(cec_dev->cecb_clk);
			CEC_INFO("get clkb rate:%ld\n",
				 clk_get_rate(cec_dev->cecb_clk));
		}
	}
}

static void cec_get_wakeup_reason(void)
{
	cec_dev->wakeup_reason = get_resume_method();
	/*scpi_get_wakeup_reason(&cec_dev->wakeup_reason);*/
	CEC_ERR("wakeup_reason:0x%x\n", cec_dev->wakeup_reason);
}

static void cec_clear_wakeup_reason(void)
{
	int ret;

	ret = scpi_clr_wakeup_reason();
	if (ret < 0)
		CEC_INFO("clr wakeup reason fail\n");
}

static void cec_get_wakeup_data(void)
{
	/*temp for mailbox not ready*/
	/*data = readl(cec_dev->periphs_reg + (0xa6 << 2));*/
	/*cec_dev->wakup_data.wk_logic_addr = data & 0xff;*/
	/*cec_dev->wakup_data.wk_phy_addr = (data >> 8) & 0xffff;*/
	/*cec_dev->wakup_data.wk_port_id = (data >> 24) & 0xff;*/

	scpi_get_cec_val(SCPI_CMD_GET_CEC1,
			 (unsigned int *)&cec_dev->wakup_data);

	CEC_ERR("wakeup_data: %#x\n",
		*((unsigned int *)&cec_dev->wakup_data));
}

static int __of_irq_count(struct device_node *dev)
{
	struct of_phandle_args irq;
	int nr = 0;

	while (of_irq_parse_one(dev, nr, &irq) == 0)
		nr++;

	return nr;
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
	struct vendor_info_data *vend;
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
	cec_dev->tx_dev   = get_hdmitx_device();
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
	#ifdef CEC_MAIL_BOX
	cec_get_wakeup_reason();
	cec_get_wakeup_data();
	#endif
	cec_irq_enable(true);
	CEC_ERR("%s success end\n", __func__);
	cec_dev->probe_finish = true;
	return 0;

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
	destroy_workqueue(cec_dev->cec_thread);
tag_cec_threat_err:
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
	cec_dev->wakeup_st = 0;
	cec_dev->msg_idx = 0;
	cec_dev->msg_num = 0;
	for (i = 0; i < CEC_MSG_BUFF_MAX; i++) {
		cec_dev->msgbuff[i].len = 0;
		for (j = 0; j < MAX_MSG; j++)
			cec_dev->msgbuff[i].msg[j] = 0;
	}
	//cec_dev->cec_suspend = CEC_DEEP_SUSPEND;
	CEC_ERR("%s\n", __func__);
	return 0;
}

static void aml_cec_pm_complete(struct device *dev)
{
	if (IS_ERR_OR_NULL(cec_dev))
		return;

	cec_dev->wakeup_st = 0;
	cec_dev->msg_idx = 0;
#ifdef CEC_MAIL_BOX
	cec_get_wakeup_reason();
	if (cec_dev->wakeup_reason == CEC_WAKEUP) {
		cec_key_report(0);
		cec_clear_wakeup_reason();
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
	return 0;
}

static int aml_cec_resume_noirq(struct device *dev)
{
	int ret = 0;

	CEC_ERR("cec resume noirq!\n");

	cec_dev->cec_info.power_status = CEC_PW_TRANS_STANDBY_TO_ON;
	cec_dev->cec_suspend = CEC_PW_TRANS_STANDBY_TO_ON;
	/*initial msg buffer*/
	cec_dev->msg_idx = 0;
	cec_dev->msg_num = 0;

	#ifdef CEC_FREEZE_WAKE_UP
	if (is_pm_s2idle_mode())
		CEC_ERR("is freeze mode\n");
	else
	#endif
	{
		cec_clear_all_logical_addr(ee_cec);
		cec_get_wakeup_reason();
		cec_get_wakeup_data();
		/* disable all logical address */
		/*cec_dev->cec_info.addr_enable = 0;*/
	}
	cec_pre_init();
	if (!IS_ERR(cec_dev->dbg_dev->pins->default_state))
		ret = pinctrl_pm_select_default_state(cec_dev->dbg_dev);
	else
		CEC_ERR("pinctrl default_state error\n");
	cec_irq_enable(true);
	cec_dev->cec_info.power_status = CEC_PW_POWER_ON;
	cec_dev->cec_suspend = CEC_PW_POWER_ON;
	cec_dev->wakeup_st = 1;
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

