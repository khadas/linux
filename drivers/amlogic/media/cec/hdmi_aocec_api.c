// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
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
#include <linux/platform_device.h>
#include <linux/pinctrl/devinfo.h>
#include <linux/pinctrl/consumer.h>
#include <linux/mutex.h>
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
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/scpi_protocol.h>
#include <linux/amlogic/pm.h>
#include "hdmi_aocec_api.h"
#include <media/cec.h>
#include "linux/rtc.h"
#include <linux/timekeeping.h>

static int cec_line_cnt;

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

static struct cec_uevent cec_events[] = {
	{
		.type = HDMI_PLUG_EVENT,
		.env = "hdmi_conn=",
	},
	{
		.type = CEC_RX_MSG,
		.env = "cec_rx_msg=",
	},
	{
		.type = CEC_PWR_UEVENT,
		.env = "cec_wakeup=",
	},
	{
		/* end of cec_events[] */
		.type = CEC_NONE_EVENT,
	},
};

void write_ao(unsigned int addr, unsigned int data)
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

unsigned int read_ao(unsigned int addr)
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

unsigned int read_periphs(unsigned int addr)
{
	unsigned int data = 0;

	if (cec_dev->periphs_reg)
		data = readl(cec_dev->periphs_reg + addr);
	return data;
}

static unsigned int read_pad_reg(unsigned int addr)
{
	unsigned int data = 0;

	if (cec_dev->pad_reg)
		data = readl(cec_dev->pad_reg + addr);
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

unsigned int hdmirx_cec_read(unsigned int reg)
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
void hdmirx_cec_write(unsigned int reg, unsigned int value)
{
	/*
	 * TXLX has moved ee cec to ao domain
	 */
	if (reg >= DWC_CEC_CTRL && cec_dev->plat_data->ee_to_ao)
		aocecb_wr_reg((reg - DWC_CEC_CTRL) / 4, value);
	else
		hdmirx_wr_dwc(reg, value);
}

inline void hdmirx_set_bits_dwc(u32 reg, u32 bits,
				       u32 start, u32 len)
{
	unsigned int tmp;

	tmp = hdmirx_cec_read(reg);
	tmp &= ~(((1 << len) - 1) << start);
	tmp |=  (bits << start);
	hdmirx_cec_write(reg, tmp);
}

/* --------application operation-------- */
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

/* max length = 14+1 */
#define OSD_NAME_DEV	1
const u8 dev_osd_name[1][16] = {
	{1, 0x43, 0x68, 0x72, 0x6f, 0x6d, 0x65, 0x63, 0x61, 0x73, 0x74},
};

const u8 dev_vendor_id[1][3] = {
	{0, 0, 0},
};

/* special notify for hdmirx */
bool cec_message_op(unsigned char *msg, unsigned char len)
{
	int i, j;

	if (((msg[0] & 0xf0) >> 4) == cec_dev->cec_info.log_addr) {
		CEC_ERR("bad initiator with self 0x%x",
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

/* --------------------- FOR EE CEC(AOCECB) -------------------- */
static void cecb_hw_reset(void)
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

static void cecb_irq_enable(bool enable)
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

void cecb_check_irq_enable(void)
{
	unsigned int reg32;

	/* irq on chip txlx has separate from EE cec, no need check */
	if (cec_dev->plat_data->ee_to_ao)
		return;

	reg32 = hdmirx_cec_read(DWC_AUD_CEC_IEN);
	if ((reg32 & EE_CEC_IRQ_EN_MASK) != EE_CEC_IRQ_EN_MASK) {
		CEC_INFO("irq_en is wrong:%x\n", reg32);
		hdmirx_cec_write(DWC_AUD_CEC_IEN_SET, EE_CEC_IRQ_EN_MASK);
	}
}

inline void cecb_clear_irq(unsigned int flags)
{
	if (!cec_dev->plat_data->ee_to_ao)
		hdmirx_cec_write(DWC_AUD_CEC_ICLR, flags);
	else
		write_ao(AO_CECB_INTR_CLR, flags);
}

int cecb_irq_stat(void)
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

int cecb_trigger_tx(const unsigned char *msg, unsigned char len, unsigned char sig_free)
{
	int i = 0, size = 0;
	int lock;
	u32 cec_ctrl = 0;
	struct timespec64 kts;
	struct rtc_time tm;

	cecb_check_irq_enable();
	while (1) {
		/* send is in process */
		lock = hdmirx_cec_read(DWC_CEC_LOCK);
		if (lock) {
			CEC_ERR("receive msg in tx\n");
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
		CEC_INFO("%s busy cnt: %d\n", __func__, i);
	}
	ktime_get_real_ts64(&kts);
	rtc_time64_to_tm(kts.tv_sec, &tm);
	size += sprintf(msg_log_buf + size, "UTC+0 %ptRd %ptRt: [TX] len: %d, msg:", &tm, &tm, len);
	for (i = 0; i < len; i++) {
		hdmirx_cec_write(DWC_CEC_TX_DATA0 + i * 4, msg[i]);
		size += sprintf(msg_log_buf + size, " %02x", msg[i]);
	}
	msg_log_buf[size] = '\0';
	CEC_PRINT("%s\n", msg_log_buf);
	/* start send */
	hdmirx_cec_write(DWC_CEC_TX_CNT, len);

	if (cec_dev->chk_sig_free_time) {
		cec_ctrl = 0x1;
		if (sig_free == CEC_SIGNAL_FREE_TIME_RETRY)
			cec_ctrl |= (0 << 1);
		else if (sig_free == CEC_SIGNAL_FREE_TIME_NEW_INITIATOR)
			cec_ctrl |= (1 << 1);
		else if (sig_free == CEC_SIGNAL_FREE_TIME_NEXT_XFER)
			cec_ctrl |= (2 << 1);
		else
			cec_ctrl |= (1 << 1);
	} else {
		cec_ctrl = 3;
	}
	hdmirx_set_bits_dwc(DWC_CEC_CTRL, cec_ctrl, 0, 3);
	return 0;
}

static void cecb_addr_add(unsigned int l_add)
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

/*--------------------- END of EE CEC --------------------*/

/*--------------------- FOR AO CEC(CECA) --------------------*/
static void ceca_arbit_bit_time_set(unsigned int bit_set,
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
	ceca_arbit_bit_time_set(3, 0x118, 0);
	ceca_arbit_bit_time_set(5, 0x000, 0);
	ceca_arbit_bit_time_set(7, 0x2aa, 0);
	dprintk(L_1, "%s\n", __func__);
}

unsigned int ao_ceca_intr_stat(void)
{
	return read_ao(AO_CEC_INTR_STAT);
}

static void ao_ceca_irq_enable(bool enable)
{
	if (enable)
		cec_set_reg_bits(AO_CEC_INTR_MASKN, 0x6, 0, 3);
	else
		cec_set_reg_bits(AO_CEC_INTR_MASKN, 0x0, 0, 3);
	CEC_INFO("ao enable:int mask:0x%x\n",
		 read_ao(AO_CEC_INTR_MASKN));
}

/* return value: 1: successful	  0: error */
int ceca_trigger_tx(const unsigned char *msg, int len)
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

static void ceca_hw_buf_clear(void)
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

void ceca_rx_buf_clear(void)
{
	aocec_wr_reg(CEC_RX_CLEAR_BUF, 0x1);
	aocec_wr_reg(CEC_RX_CLEAR_BUF, 0x0);
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
	ceca_arbit_bit_time_set(3, 0x118, 0);
	ceca_arbit_bit_time_set(5, 0x000, 0);
	ceca_arbit_bit_time_set(7, 0x2aa, 0);
}

static void ceca_addr_add(unsigned int l_add)
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
			if ((addr & 0x10) && ((addr & 0xf) == (l_add & 0xf)))
				return;
		}

		/* find a empty place */
		for (i = CEC_LOGICAL_ADDR0; i <= CEC_LOGICAL_ADDR4; i++) {
			addr = aocec_rd_reg(i);
			if (addr & 0x10) {
				continue;
			} else {
				ceca_hw_buf_clear();
				aocec_wr_reg(i, (l_add & 0xf));
				/*udelay(100);*/
				aocec_wr_reg(i, (l_add & 0xf) | 0x10);
				break;
			}
		}
	}
}

/*--------------------- END of AO CEC --------------------*/

/* --------hw related------- */
void cec_set_clk(struct device *dev)
{
	if (cec_dev->plat_data->chip_id >= CEC_CHIP_A1) {
		cec_dev->ceca_clk = devm_clk_get(dev, "ceca_clk");
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

		cec_dev->cecb_clk = devm_clk_get(dev, "cecb_clk");
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

void cec_hw_init(void)
{
	if (cec_dev->cec_num > ENABLE_ONE_CEC) {
		ao_ceca_init();
		ao_cecb_init();
		cec_ip_share_io(cec_dev->plat_data->share_io, ee_cec);
	} else {
		if (ee_cec == CEC_B) {
			ao_cecb_init();
			/* on T7, only use CEC_A pin for CEC on board,
			 * for cec robustness, use cecb controller
			 * with CEC_A pin(share to CEC_B)
			 */
			if (cec_dev->plat_data->chip_id == CEC_CHIP_T7)
				cec_ip_share_io(true, CEC_A);
		} else {
			ao_ceca_init();
		}
	}
}

/* cec hw module init before allocate logical address */
void cec_pre_init(void)
{
	cec_hw_init();
	//need restore all logical address
	if (cec_dev->cec_num > ENABLE_ONE_CEC)
		cec_restore_logical_addr(CEC_B, cec_dev->cec_info.addr_enable);
	else
		cec_restore_logical_addr(ee_cec, cec_dev->cec_info.addr_enable);
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

/* interface only for debug */
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

			ceca_hw_buf_clear();
			aocec_wr_reg(CEC_LOGICAL_ADDR0, (l_add & 0xf));
			/*udelay(100);*/
			aocec_wr_reg(CEC_LOGICAL_ADDR0,
				(0x1 << 4) | (l_add & 0xf));
		}
	}
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

/* interface only for debug */
void cec_logicaddr_remove(unsigned int cec_sel, unsigned int l_add)
{
	unsigned int addr;
	unsigned int i;
	unsigned char temp;

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
					ceca_hw_buf_clear();
				}
			}
		}
	}

	/* clear saved logic addr */
	temp = (read_ao(AO_DEBUG_REG1) >> 16) & 0xf;
	if (temp == l_add) {
		cec_set_reg_bits(AO_DEBUG_REG1, 0, 16, 4);
	} else {
		temp = (read_ao(AO_DEBUG_REG1) >> 24) & 0xf;
		if (temp == l_add)
			cec_set_reg_bits(AO_DEBUG_REG1, 0, 24, 4);
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

/* should not clean logic addr in AO_DEBUG_REG1
 * for chips not g12a/b/sm1 but uses uboot2015,
 * it will clear logic addr before
 * enter suspend, and AO_DEBUG_REG1 is
 * used to store the logic addr used under uboot;
 * for g12a/b, in addition to AO_DEBUG_REG1
 * cec related data are transferred to uboot
 * by mailbox(SWPL-32434)
 */
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

void cec_clear_saved_logic_addr(void)
{
	/* clear saved logic addr */
	cec_set_reg_bits(AO_DEBUG_REG1, 0, 16, 4);
	cec_set_reg_bits(AO_DEBUG_REG1, 0, 24, 4);
}

void cec_irq_enable(bool enable)
{
	if (cec_dev->cec_num > ENABLE_ONE_CEC) {
		cecb_irq_enable(enable);
		ao_ceca_irq_enable(enable);
	} else {
		if (ee_cec == CEC_B)
			cecb_irq_enable(enable);
		else
			ao_ceca_irq_enable(enable);
	}
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

enum hrtimer_restart cec_line_check(struct hrtimer *timer)
{
	if (get_line() == 0)
		cec_line_cnt++;
	hrtimer_forward_now(timer, HR_DELAY(1));
	return HRTIMER_RESTART;
}

int check_conflict(void)
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

void cec_ip_share_io(u32 share, u32 cec_ip)
{
	if (share) {
		if (cec_ip == CEC_A) {
			cec_set_reg_bits(AO_CEC_GEN_CNTL, 1, 4, 1);
			cec_set_reg_bits(AO_CECB_GEN_CNTL, 0, 4, 1);
			CEC_ERR("share pin mux to b\n");
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

/* --------msg related-------- */

void cec_give_version(unsigned int dest)
{
	unsigned char index = cec_dev->cec_info.log_addr;
	unsigned char msg[3];

	if (dest != 0xf) {
		msg[0] = ((index & 0xf) << 4) | dest;
		msg[1] = CEC_OC_CEC_VERSION;
		msg[2] = cec_dev->cec_info.cec_version;
		cec_ll_tx(msg, 3, CEC_SIGNAL_FREE_TIME_NEW_INITIATOR);
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

	cec_ll_tx(msg, 5, CEC_SIGNAL_FREE_TIME_NEW_INITIATOR);
}

void cec_device_vendor_id(void)
{
	unsigned char index = cec_dev->cec_info.log_addr;
	unsigned char msg[5];
	unsigned int vendor_id;

	vendor_id = cec_dev->cec_info.vendor_id;
	msg[0] = ((index & 0xf) << 4) | CEC_BROADCAST_ADDR;
	msg[1] = CEC_OC_DEVICE_VENDOR_ID;
	msg[2] = (vendor_id >> 16) & 0xff;
	msg[3] = (vendor_id >> 8) & 0xff;
	msg[4] = (vendor_id >> 0) & 0xff;

	cec_ll_tx(msg, 5, CEC_SIGNAL_FREE_TIME_NEW_INITIATOR);
}

void cec_give_deck_status(unsigned int dest)
{
	unsigned char index = cec_dev->cec_info.log_addr;
	unsigned char msg[3];

	msg[0] = ((index & 0xf) << 4) | dest;
	msg[1] = CEC_OC_DECK_STATUS;
	msg[2] = 0x1a;
	cec_ll_tx(msg, 3, CEC_SIGNAL_FREE_TIME_NEW_INITIATOR);
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
	cec_ll_tx(msg, 3, CEC_SIGNAL_FREE_TIME_NEW_INITIATOR);
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

	cec_ll_tx(msg, 4, CEC_SIGNAL_FREE_TIME_NEW_INITIATOR);
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

		cec_ll_tx(msg, 2 + osd_len, CEC_SIGNAL_FREE_TIME_NEW_INITIATOR);
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
	cec_ll_tx(msg, 4, CEC_SIGNAL_FREE_TIME_NEW_INITIATOR);
}

void cec_request_active_source(void)
{
	unsigned char msg[2];
	unsigned char index = cec_dev->cec_info.log_addr;

	msg[0] = ((index & 0xf) << 4) | CEC_BROADCAST_ADDR;
	msg[1] = CEC_OC_REQUEST_ACTIVE_SOURCE;
	cec_ll_tx(msg, 2, CEC_SIGNAL_FREE_TIME_NEW_INITIATOR);
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
	cec_ll_tx(msg, 3, CEC_SIGNAL_FREE_TIME_NEW_INITIATOR);
}

/* --------common logic interface------- */
inline bool is_poll_message(unsigned char header)
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

inline bool is_get_cec_ver_msg(const unsigned char *msg, int len)
{
	if (!msg || len < 2)
		return false;
	if (msg[1] == CEC_OC_GET_CEC_VERSION)
		return true;
	return false;
}

bool need_nack_repeat_msg(const unsigned char *msg, int len, int t)
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
 * 1 write; value valid
 * 0 read; value invalid
 * 0-15: phy addr+
 * 16-19: logical address+
 * 20-23: device type+
 */
unsigned int cec_config2_phyaddr(unsigned int value, bool wr_flag)
{
	if (wr_flag)
		cec_set_reg_bits(AO_DEBUG_REG1, value, 0, 16);

	return read_ao(AO_DEBUG_REG1) & 0xffff;
}

/*
 *wr_flag: reg AO_DEBUG_REG1 (bit 16-19)
 * 1 write; value valid
 * 0 read; value invalid
 * 0-15: phy addr+
 * 16-19: logical address+
 * 20-23: device type+
 * 24-27: second logical address (new, for soundbar)
 */
unsigned int cec_config2_logaddr(unsigned int value, bool wr_flag)
{
	unsigned char temp;

	if (wr_flag) {
		temp = (read_ao(AO_DEBUG_REG1) >> 16) & 0xf;
		/* uboot will check first logic address firstly, if it's
		 * invalid address, it will set default logic address
		 * need replace first logic address with valid address.
		 * if the saved first address value is 0 or 0xf, it means
		 * it can be replaced.
		 */
		if (temp == 0 || temp == 0xf) {
			cec_set_reg_bits(AO_DEBUG_REG1, value, 16, 4);
		} else if (temp != value) {
			/* assume platform will only alloc correct logic addr */
			cec_set_reg_bits(AO_DEBUG_REG1, value, 24, 4);
			CEC_INFO("save second logic addr: %d\n", value);
		}
	}
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

void cec_key_report(int suspend)
{
	if (!(cec_config(0, 0) & CEC_FUNC_CFG_AUTO_POWER_ON)) {
		CEC_ERR("auto pw on is off (cfg:0x%x)\n", cec_config(0, 0));
		return;
	}
	#ifdef CEC_FREEZE_WAKE_UP
	if (is_pm_s2idle_mode()) {
		/* for kernel5.4, if NO_SUSPEND flag is set
		 * when request irq, the interrupt of this
		 * module can't wakeup system, need to force
		 * wakeup by hard event interface.
		 */
		pm_wakeup_hard_event(cec_dev->dbg_dev);
		CEC_INFO("freeze mode:pm_wakeup_event\n");
	}
	#endif
	input_event(cec_dev->cec_info.remote_cec_dev, EV_KEY, KEY_POWER, 1);
	input_sync(cec_dev->cec_info.remote_cec_dev);
	input_event(cec_dev->cec_info.remote_cec_dev, EV_KEY, KEY_POWER, 0);
	input_sync(cec_dev->cec_info.remote_cec_dev);
	CEC_INFO("event %s %d\n", __func__, suspend);
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

void init_cec_port_info(struct hdmi_port_info *port,
			       struct ao_cec_dev *cec_dev)
{
	unsigned int a, b, c = 0, d, e = 0;
	unsigned int phy_head = 0xf000, phy_app = 0x1000, phy_addr;
	struct vsdb_phyaddr *tx_phy_addr = get_hdmitx_phy_addr();

	/* physical address for TV or repeator */
	if (!tx_phy_addr || cec_dev->dev_type == CEC_TV_ADDR) {
		phy_addr = 0;
	} else if (tx_phy_addr->valid == 1) {
		/* get phy address from tx module */
		a = tx_phy_addr->a;
		b = tx_phy_addr->b;
		c = tx_phy_addr->c;
		d = tx_phy_addr->d;
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
	unsigned char i = 0;
	unsigned int tmp = 0;

	CEC_ERR("driver date:%s\n", CEC_DRIVER_VERSION);
	/*CEC_ERR("chip type:0x%x\n",*/
	/*	get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR));*/
	CEC_ERR("ee_cec:%d\n", ee_cec);
	CEC_ERR("cec_num:%d\n", cec_dev->cec_num);
	CEC_ERR("dev_type:%d\n", (unsigned int)cec_dev->dev_type);
	CEC_ERR("wk_logic_addr:0x%x\n", cec_dev->wakeup_data.wk_logic_addr);
	CEC_ERR("wk_phy_addr:0x%x\n", cec_dev->wakeup_data.wk_phy_addr);
	CEC_ERR("wk_port_id:0x%x\n", cec_dev->wakeup_data.wk_port_id);
	CEC_ERR("wakeup_reason:0x%x\n", cec_dev->wakeup_reason);
	CEC_ERR("phy_addr:0x%x\n", cec_dev->phy_addr);
	CEC_ERR("cec_version:0x%x\n", cec_dev->cec_info.cec_version);
	CEC_ERR("hal_ctl:0x%x\n", cec_dev->cec_info.hal_ctl);
	CEC_ERR("menu_lang:0x%x\n", cec_dev->cec_info.menu_lang);
	CEC_ERR("menu_status:0x%x\n", cec_dev->cec_info.menu_status);
	CEC_ERR("open_count:%d\n", cec_dev->cec_info.open_count.counter);
	CEC_ERR("vendor_id:0x%x\n", cec_dev->cec_info.vendor_id);
	CEC_ERR("port_num:0x%x\n", cec_dev->port_num);
	CEC_ERR("output:0x%x\n", cec_dev->output);
	CEC_ERR("arc_port:0x%x\n", cec_dev->arc_port);
	CEC_ERR("hal_flag:0x%x\n", cec_dev->hal_flag);
	CEC_ERR("hpd_state:0x%x\n", get_hpd_state());
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
	CEC_ERR("framework_on:0x%x\n", cec_dev->framework_on);
	CEC_ERR("store msg_num:0x%x\n", cec_dev->msg_num);
	CEC_ERR("store msg_idx:0x%x\n", cec_dev->msg_idx);
	CEC_ERR("cec_msg_dbg_en: %d\n", cec_msg_dbg_en);
	CEC_ERR("port_seq: %x\n", cec_dev->port_seq);

	port = kcalloc(cec_dev->port_num, sizeof(*port), GFP_KERNEL);
	if (port) {
		init_cec_port_info(port, cec_dev);
		for (i = 0; i < cec_dev->port_num; i++) {
			/* port_id: 1/2/3 means HDMIRX1/2/3, 0 means HDMITX port */
			CEC_ERR("port_id: %d, ", port[i].port_id);
			CEC_ERR("port_type: %s, ",
				port[i].type == HDMI_INPUT ? "hdmirx" : "hdmitx");
			CEC_ERR("physical_address: %x, ", port[i].physical_address);
			CEC_ERR("cec_supported: %s, ", port[i].cec_supported ? "true" : "false");
			CEC_ERR("arc_supported: %s\n", port[i].arc_supported ? "true" : "false");
		}
		kfree(port);
	}

#if (defined(CONFIG_AMLOGIC_HDMITX) || defined(CONFIG_AMLOGIC_HDMITX21))
	tmp |= (get_hpd_state() << 4);
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_HDMI
	tmp |= (hdmirx_get_connect_info() & 0xF);
#endif
	CEC_ERR("hdmitx/rx connect status: 0x%x\n", tmp);

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
	CEC_ERR("chk_sig_free_time: %d\n", cec_dev->chk_sig_free_time);
	CEC_ERR("sw_chk_bus: %d\n", cec_dev->sw_chk_bus);
}

int dump_cec_status(char *buf)
{
	struct hdmi_port_info *port;
	unsigned char i = 0;
	unsigned int tmp = 0;
	int pos = 0;

	pos += snprintf(buf + pos, PAGE_SIZE, "driver date:%s\n", CEC_DRIVER_VERSION);
	/*pos += snprintf(buf + pos, PAGE_SIZE, "chip type:0x%x\n",*/
	/*	get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR));*/
	pos += snprintf(buf + pos, PAGE_SIZE, "ee_cec:%d\n", ee_cec);
	pos += snprintf(buf + pos, PAGE_SIZE, "cec_num:%d\n", cec_dev->cec_num);
	pos += snprintf(buf + pos, PAGE_SIZE, "dev_type:%d\n", (unsigned int)cec_dev->dev_type);
	pos += snprintf(buf + pos, PAGE_SIZE,
		"wk_logic_addr:0x%x\n", cec_dev->wakeup_data.wk_logic_addr);
	pos += snprintf(buf + pos, PAGE_SIZE,
		"wk_phy_addr:0x%x\n", cec_dev->wakeup_data.wk_phy_addr);
	pos += snprintf(buf + pos, PAGE_SIZE, "wk_port_id:0x%x\n", cec_dev->wakeup_data.wk_port_id);
	pos += snprintf(buf + pos, PAGE_SIZE, "wakeup_reason:0x%x\n", cec_dev->wakeup_reason);
	pos += snprintf(buf + pos, PAGE_SIZE, "phy_addr:0x%x\n", cec_dev->phy_addr);
	pos += snprintf(buf + pos, PAGE_SIZE, "cec_version:0x%x\n", cec_dev->cec_info.cec_version);
	pos += snprintf(buf + pos, PAGE_SIZE, "hal_ctl:0x%x\n", cec_dev->cec_info.hal_ctl);
	pos += snprintf(buf + pos, PAGE_SIZE, "menu_lang:0x%x\n", cec_dev->cec_info.menu_lang);
	pos += snprintf(buf + pos, PAGE_SIZE, "menu_status:0x%x\n", cec_dev->cec_info.menu_status);
	pos += snprintf(buf + pos, PAGE_SIZE,
		"open_count:%d\n", cec_dev->cec_info.open_count.counter);
	pos += snprintf(buf + pos, PAGE_SIZE, "vendor_id:0x%x\n", cec_dev->cec_info.vendor_id);
	pos += snprintf(buf + pos, PAGE_SIZE, "port_num:0x%x\n", cec_dev->port_num);
	pos += snprintf(buf + pos, PAGE_SIZE, "output:0x%x\n", cec_dev->output);
	pos += snprintf(buf + pos, PAGE_SIZE, "arc_port:0x%x\n", cec_dev->arc_port);
	pos += snprintf(buf + pos, PAGE_SIZE, "hal_flag:0x%x\n", cec_dev->hal_flag);
	pos += snprintf(buf + pos, PAGE_SIZE, "hpd_state:0x%x\n", get_hpd_state());
	pos += snprintf(buf + pos, PAGE_SIZE, "cec_config:0x%x\n", cec_config(0, 0));
	pos += snprintf(buf + pos, PAGE_SIZE, "log_addr:0x%x\n", cec_dev->cec_info.log_addr);

	pos += snprintf(buf + pos, PAGE_SIZE, "id:0x%x\n", cec_dev->plat_data->chip_id);
	pos += snprintf(buf + pos, PAGE_SIZE, "ceca_ver:0x%x\n", cec_dev->plat_data->ceca_ver);
	pos += snprintf(buf + pos, PAGE_SIZE, "cecb_ver:0x%x\n", cec_dev->plat_data->cecb_ver);
	pos += snprintf(buf + pos, PAGE_SIZE, "share_io:0x%x\n", cec_dev->plat_data->share_io);
	pos += snprintf(buf + pos, PAGE_SIZE, "line_bit:0x%x\n", cec_dev->plat_data->line_bit);
	pos += snprintf(buf + pos, PAGE_SIZE, "ee_to_ao:0x%x\n", cec_dev->plat_data->ee_to_ao);
	pos += snprintf(buf + pos, PAGE_SIZE, "irq_ceca:0x%x\n", cec_dev->irq_ceca);
	pos += snprintf(buf + pos, PAGE_SIZE, "irq_cecb:0x%x\n", cec_dev->irq_cecb);
	pos += snprintf(buf + pos, PAGE_SIZE, "framework_on:0x%x\n", cec_dev->framework_on);
	pos += snprintf(buf + pos, PAGE_SIZE, "store msg_num:0x%x\n", cec_dev->msg_num);
	pos += snprintf(buf + pos, PAGE_SIZE, "store msg_idx:0x%x\n", cec_dev->msg_idx);
	pos += snprintf(buf + pos, PAGE_SIZE, "cec_msg_dbg_en: %d\n", cec_msg_dbg_en);
	pos += snprintf(buf + pos, PAGE_SIZE, "port_seq: %x\n", cec_dev->port_seq);
	pos += snprintf(buf + pos, PAGE_SIZE, "cec_log_en: %x\n", cec_dev->cec_log_en);

	port = kcalloc(cec_dev->port_num, sizeof(*port), GFP_KERNEL);
	if (port) {
		init_cec_port_info(port, cec_dev);
		for (i = 0; i < cec_dev->port_num; i++) {
			/* port_id: 1/2/3 means HDMIRX1/2/3, 0 means HDMITX port */
			pos += snprintf(buf + pos, PAGE_SIZE, "port_id: %d, ",
					port[i].port_id);
			pos += snprintf(buf + pos, PAGE_SIZE, "port_type: %s, ",
					port[i].type == HDMI_INPUT ? "hdmirx" : "hdmitx");
			pos += snprintf(buf + pos, PAGE_SIZE, "physical_address: %x, ",
					port[i].physical_address);
			pos += snprintf(buf + pos, PAGE_SIZE, "cec_supported: %s, ",
					port[i].cec_supported ? "true" : "false");
			pos += snprintf(buf + pos, PAGE_SIZE, "arc_supported: %s\n",
					port[i].arc_supported ? "true" : "false");
		}
		kfree(port);
	}

#if (defined(CONFIG_AMLOGIC_HDMITX) || defined(CONFIG_AMLOGIC_HDMITX21))
	tmp |= (get_hpd_state() << 4);
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_HDMI
	tmp |= (hdmirx_get_connect_info() & 0xF);
#endif
	snprintf(buf + pos, PAGE_SIZE, "hdmitx/rx connect status: 0x%x\n", tmp);

	if (cec_dev->cec_num > ENABLE_ONE_CEC) {
		pos += snprintf(buf + pos, PAGE_SIZE,
			"B addrL 0x%x\n", hdmirx_cec_read(DWC_CEC_ADDR_L));
		pos += snprintf(buf + pos, PAGE_SIZE,
			"B addrH 0x%x\n", hdmirx_cec_read(DWC_CEC_ADDR_H));

		pos += snprintf(buf + pos, PAGE_SIZE,
			"A addr0 0x%x\n", aocec_rd_reg(CEC_LOGICAL_ADDR0));
		pos += snprintf(buf + pos, PAGE_SIZE,
			"A addr1 0x%x\n", aocec_rd_reg(CEC_LOGICAL_ADDR1));
		/* pos += snprintf(buf + pos, PAGE_SIZE, */
			/* "addr2 0x%x\n", aocec_rd_reg(CEC_LOGICAL_ADDR2));*/
		/* pos += snprintf(buf + pos, PAGE_SIZE, */
			/* "addr3 0x%x\n", aocec_rd_reg(CEC_LOGICAL_ADDR3));*/
		/* pos += snprintf(buf + pos, PAGE_SIZE, */
			/* "addr4 0x%x\n", aocec_rd_reg(CEC_LOGICAL_ADDR4));*/
	} else {
		if (ee_cec == CEC_B) {
			pos += snprintf(buf + pos, PAGE_SIZE, "addrL 0x%x\n",
				hdmirx_cec_read(DWC_CEC_ADDR_L));
			pos += snprintf(buf + pos, PAGE_SIZE, "addrH 0x%x\n",
				hdmirx_cec_read(DWC_CEC_ADDR_H));
		} else {
			pos += snprintf(buf + pos, PAGE_SIZE, "addr0 0x%x\n",
				aocec_rd_reg(CEC_LOGICAL_ADDR0));
			pos += snprintf(buf + pos, PAGE_SIZE, "addr1 0x%x\n",
				aocec_rd_reg(CEC_LOGICAL_ADDR1));
			pos += snprintf(buf + pos, PAGE_SIZE, "addr2 0x%x\n",
				aocec_rd_reg(CEC_LOGICAL_ADDR2));
			pos += snprintf(buf + pos, PAGE_SIZE, "addr3 0x%x\n",
				aocec_rd_reg(CEC_LOGICAL_ADDR3));
			pos += snprintf(buf + pos, PAGE_SIZE, "addr4 0x%x\n",
				aocec_rd_reg(CEC_LOGICAL_ADDR4));
		}
	}
	pos += snprintf(buf + pos, PAGE_SIZE, "addr_enable:0x%x\n", cec_dev->cec_info.addr_enable);
	pos += snprintf(buf + pos, PAGE_SIZE,
		"chk_sig_free_time: %d\n", cec_dev->chk_sig_free_time);
	pos += snprintf(buf + pos, PAGE_SIZE, "sw_chk_bus: %d\n", cec_dev->sw_chk_bus);
	return pos;
}

unsigned int cec_get_cur_phy_addr(void)
{
		unsigned int a, b, c, d;
		unsigned int tmp = 0, i;
		struct vsdb_phyaddr *tx_phy_addr = get_hdmitx_phy_addr();

		if (!tx_phy_addr || cec_dev->dev_type == CEC_TV_ADDR) {
			tmp = 0;
		} else {
			for (i = 0; i < 5; i++) {
				/*hpd attach and wait read edid*/
				a = tx_phy_addr->a;
				b = tx_phy_addr->b;
				c = tx_phy_addr->c;
				d = tx_phy_addr->d;
				tmp = ((a << 12) | (b << 8) | (c << 4) | (d));

				if (tx_phy_addr->valid == 1)
					break;
				msleep(20);
			}
		}

		return tmp;
}

/*
 * save param to mailbox, to uboot
 */
void cec_save_mail_box(void)
{
	struct st_cec_mailbox_data cec_mailbox;
	CEC_INFO("%s\n", __func__);
	cec_mailbox.cec_config = cec_config(0, 0);
	cec_mailbox.phy_addr = cec_config2_phyaddr(0, 0);
	cec_mailbox.phy_addr |= cec_config2_logaddr(0, 0) << 16;
	cec_mailbox.phy_addr |= cec_config2_devtype(0, 0) << 20;
	/* for second logic addr */
	cec_mailbox.phy_addr |= ((read_ao(AO_DEBUG_REG1) >> 24) & 0xf) << 24;
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

void cec_get_wakeup_reason(void)
{
	/* cec bootup earlier than gx_pm module,
	 * need to use scpi interface instead
	 * of gx_pm interface when powerup
	 * for tm2, also need to use scpi interface
	 * for resume reason
	 */
	scpi_get_wakeup_reason(&cec_dev->wakeup_reason);
	/* cec_dev->wakeup_reason = get_resume_reason(); */
	CEC_ERR("wakeup_reason:0x%x\n", cec_dev->wakeup_reason);
}

void cec_clear_wakeup_reason(void)
{
	int ret;

	ret = scpi_clr_wakeup_reason();
	if (ret < 0)
		CEC_INFO("clr wakeup reason fail\n");
}

unsigned int cec_get_wk_port_id(unsigned int phy_addr)
{
	int i = 0;
	unsigned int port_id = 0;

	if (phy_addr == 0xFFFF || (phy_addr & 0xF000) == 0)
		return 0xFF;

	for (i = 0; i < PHY_ADDR_LEN; i++) {
		port_id = (phy_addr >> (PHY_ADDR_LEN - i - 1) * 4) & 0xF;
		if (port_id == 0) {
			port_id = (phy_addr >> (PHY_ADDR_LEN - i) * 4) & 0xF;
			break;
		}
	}
	return port_id;
}

/* for shutdown/resume
 * wakeup message from source
 * <Image View On> 0x04 len = 2
 * <Text View On> 0x0D len = 2
 * <Active Source> 0x82 len = 4
 *
 * wakeup message from TV
 * <Set Stream Path> 0x86 len = 4
 * <Routing Change> 0x80 len = 6
 * <remote key> 0x40 0x6d 0x09 len = 3
 * if <Routing Change> is received
 * then ignore other otp/as msg
 */
void cec_get_wakeup_data(void)
{
	unsigned int stick_cec1 = 0;
	unsigned int stick_cec2 = 0;
	/*temp for mailbox not ready*/
	/*data = readl(cec_dev->periphs_reg + (0xa6 << 2));*/
	/*cec_dev->wakeup_data.wk_logic_addr = data & 0xff;*/
	/*cec_dev->wakeup_data.wk_phy_addr = (data >> 8) & 0xffff;*/
	/*cec_dev->wakeup_data.wk_port_id = (data >> 24) & 0xff;*/

	memset(cec_dev->cec_wk_otp_msg, 0, sizeof(cec_dev->cec_wk_otp_msg));
	memset(cec_dev->cec_wk_as_msg, 0, sizeof(cec_dev->cec_wk_as_msg));
#ifdef CEC_MAIL_BOX
	scpi_get_cec_val(SCPI_CMD_GET_CEC1, &stick_cec1);
	scpi_get_cec_val(SCPI_CMD_GET_CEC2, &stick_cec2);
#endif
	CEC_ERR("wakeup_data: %#x %#x\n", stick_cec1, stick_cec2);

	/* CEC_FUNC_MASK and AUTO_POWER_ON_MASK
	 * already judge under uboot
	 */
	/* T7 no cec sticky register, so use 6 byte of general sticky reg
	 * only save necessary message witch may affect routing
	 * SYSCTRL_STICKY_REG6: bit 31~16
	 * SYSCTRL_STICKY_REG5: bit 31~0
	 */
	if (cec_dev->plat_data->chip_id == CEC_CHIP_T7) {
		switch ((stick_cec1 >> 16) & 0xff) {
		/* when T7 as playback or soundbar */
		case CEC_OC_ROUTING_CHANGE:
			cec_dev->cec_wk_otp_msg[0] = 6;
			cec_dev->cec_wk_otp_msg[1] = (stick_cec1 >> 24) & 0xff;
			cec_dev->cec_wk_otp_msg[2] = (stick_cec1 >> 16) & 0xff;
			cec_dev->cec_wk_otp_msg[3] = (stick_cec1 >> 8) & 0xff;
			cec_dev->cec_wk_otp_msg[4] = stick_cec1 & 0xff;
			cec_dev->cec_wk_otp_msg[5] = (stick_cec2 >> 24) & 0xff;
			cec_dev->cec_wk_otp_msg[6] = (stick_cec2 >> 16) & 0xff;
			break;
		/* when T7 as playback or soundbar */
		case CEC_OC_SET_STREAM_PATH:
			cec_dev->cec_wk_otp_msg[0] = 4;
			cec_dev->cec_wk_otp_msg[1] = (stick_cec1 >> 24) & 0xff;
			cec_dev->cec_wk_otp_msg[2] = (stick_cec1 >> 16) & 0xff;
			cec_dev->cec_wk_otp_msg[3] = (stick_cec1 >> 8) & 0xff;
			cec_dev->cec_wk_otp_msg[4] = stick_cec1 & 0xff;
			break;
		/* when T7 as TV */
		case CEC_OC_ACTIVE_SOURCE:
			cec_dev->cec_wk_as_msg[0] = 4;
			cec_dev->cec_wk_as_msg[1] = (stick_cec1 >> 24) & 0xff;
			cec_dev->cec_wk_as_msg[2] = (stick_cec1 >> 16) & 0xff;
			cec_dev->cec_wk_as_msg[3] = (stick_cec1 >> 8) & 0xff;
			cec_dev->cec_wk_as_msg[4] = stick_cec1 & 0xff;
			cec_dev->wakeup_data.wk_phy_addr = stick_cec1 & 0xffff;
			cec_dev->wakeup_data.wk_logic_addr =
			(cec_dev->cec_wk_as_msg[1] >> 4) & 0xf;
			cec_dev->wakeup_data.wk_port_id =
			cec_get_wk_port_id(cec_dev->wakeup_data.wk_phy_addr);
			break;
		default:
			break;
		}
		return;
	}
	/* for otp, recovery and compose to msg */
	switch ((stick_cec1 >> 16) & 0xff) {
	case CEC_OC_ROUTING_CHANGE:
		cec_dev->cec_wk_otp_msg[0] = 6;
		cec_dev->cec_wk_otp_msg[1] = (stick_cec1 >> 24) & 0xff;
		cec_dev->cec_wk_otp_msg[2] = (stick_cec1 >> 16) & 0xff;
		cec_dev->cec_wk_otp_msg[3] = (stick_cec1 >> 8) & 0xff;
		cec_dev->cec_wk_otp_msg[4] = stick_cec1 & 0xff;
		cec_dev->cec_wk_otp_msg[5] = (stick_cec2 >> 24) & 0xff;
		cec_dev->cec_wk_otp_msg[6] = (stick_cec2 >> 16) & 0xff;
		break;
	case CEC_OC_SET_STREAM_PATH:
		cec_dev->cec_wk_otp_msg[0] = 4;
		cec_dev->cec_wk_otp_msg[1] = (stick_cec1 >> 24) & 0xff;
		cec_dev->cec_wk_otp_msg[2] = (stick_cec1 >> 16) & 0xff;
		cec_dev->cec_wk_otp_msg[3] = (stick_cec1 >> 8) & 0xff;
		cec_dev->cec_wk_otp_msg[4] = stick_cec1 & 0xff;
		break;
	case CEC_OC_USER_CONTROL_PRESSED:
		cec_dev->cec_wk_otp_msg[0] = 3;
		cec_dev->cec_wk_otp_msg[1] = (stick_cec1 >> 24) & 0xff;
		cec_dev->cec_wk_otp_msg[2] = (stick_cec1 >> 16) & 0xff;
		cec_dev->cec_wk_otp_msg[3] = (stick_cec1 >> 8) & 0xff;
		break;
	case CEC_OC_IMAGE_VIEW_ON:
	case CEC_OC_TEXT_VIEW_ON:
		cec_dev->cec_wk_otp_msg[0] = 2;
		cec_dev->cec_wk_otp_msg[1] = (stick_cec1 >> 24) & 0xff;
		cec_dev->cec_wk_otp_msg[2] = (stick_cec1 >> 16) & 0xff;
		cec_dev->wakeup_data.wk_phy_addr = 0xffff;
		cec_dev->wakeup_data.wk_logic_addr =
			(cec_dev->cec_wk_otp_msg[1] >> 4) & 0xf;
		cec_dev->wakeup_data.wk_port_id = 0xff;
		break;
	default:
		break;
	}

	if (cec_dev->cec_wk_otp_msg[0] > 4)
		return;
	/* for active source, recovery and compose to msg */
	if (((stick_cec2 >> 16) & 0xff) == CEC_OC_ACTIVE_SOURCE) {
		cec_dev->cec_wk_as_msg[0] = 4;
		cec_dev->cec_wk_as_msg[1] = (stick_cec2 >> 24) & 0xff;
		cec_dev->cec_wk_as_msg[2] = (stick_cec2 >> 16) & 0xff;
		cec_dev->cec_wk_as_msg[3] = (stick_cec2 >> 8) & 0xff;
		cec_dev->cec_wk_as_msg[4] = stick_cec2 & 0xff;
		cec_dev->wakeup_data.wk_phy_addr = stick_cec2 & 0xffff;
		cec_dev->wakeup_data.wk_logic_addr =
			(cec_dev->cec_wk_as_msg[1] >> 4) & 0xf;
		cec_dev->wakeup_data.wk_port_id =
			cec_get_wk_port_id(cec_dev->wakeup_data.wk_phy_addr);
	}
}

int dump_cec_reg(char *buf)
{
	int i = 0, pos = 0;
	unsigned char reg;
	unsigned int reg32;

	if (ee_cec == CEC_A) {
		reg32 = read_ao(AO_CEC_CLK_CNTL_REG0);
		pos += snprintf(buf + pos, PAGE_SIZE, "AO_CEC_CLK_CNTL_REG0:0x%08x\n",
			reg32);
		reg32 = read_ao(AO_CEC_CLK_CNTL_REG1);
		pos += snprintf(buf + pos, PAGE_SIZE, "AO_CEC_CLK_CNTL_REG1:0x%08x\n",
			reg32);
		reg32 = read_ao(AO_CEC_GEN_CNTL);
		pos += snprintf(buf + pos, PAGE_SIZE, "AO_CEC_GEN_CNTL:	0x%08x\n",
			reg32);
		reg32 = read_ao(AO_CEC_RW_REG);
		pos += snprintf(buf + pos, PAGE_SIZE, "AO_CEC_RW_REG: 0x%08x\n",
			reg32);
		reg32 = read_ao(AO_CEC_INTR_MASKN);
		pos += snprintf(buf + pos, PAGE_SIZE, "AO_CEC_INTR_MASKN:0x%08x\n",
			reg32);
		reg32 = read_ao(AO_CEC_INTR_STAT);
		pos += snprintf(buf + pos, PAGE_SIZE, "AO_CEC_INTR_STAT: 0x%08x\n",
			reg32);
	}

	if (cec_dev->plat_data->chip_id == CEC_CHIP_A1) {
		pos += snprintf(buf + pos, PAGE_SIZE, "%2x:%2x\n", 0xe4, read_periphs(0xe4));
		pos += snprintf(buf + pos, PAGE_SIZE, "%2x:%2x\n", 0xe8, read_periphs(0xe8));
		pos += snprintf(buf + pos, PAGE_SIZE, "%2x:%2x\n", 0xec, read_periphs(0xec));
		pos += snprintf(buf + pos, PAGE_SIZE, "%2x:%2x\n", 0xf0, read_periphs(0xf0));
	} else if (cec_dev->plat_data->chip_id == CEC_CHIP_SC2 ||
		   cec_dev->plat_data->chip_id == CEC_CHIP_S4) {
		pos += snprintf(buf + pos, PAGE_SIZE, "%2x:%2x\n", 0x22, read_clock(0x22));
		pos += snprintf(buf + pos, PAGE_SIZE, "%2x:%2x\n", 0x23, read_clock(0x23));
		pos += snprintf(buf + pos, PAGE_SIZE, "%2x:%2x\n", 0x24, read_clock(0x24));
		pos += snprintf(buf + pos, PAGE_SIZE, "%2x:%2x\n", 0x25, read_clock(0x25));
	}

	if (ee_cec == CEC_B) {
		reg32 = read_ao(AO_CECB_CLK_CNTL_REG0);
		pos += snprintf(buf + pos, PAGE_SIZE,
			     "AO_CECB_CLK_CNTL_REG0:0x%08x\n",
			     reg32);
		reg32 = read_ao(AO_CECB_CLK_CNTL_REG1);
		pos += snprintf(buf + pos, PAGE_SIZE,
			     "AO_CECB_CLK_CNTL_REG1:0x%08x\n",
			     reg32);
		reg32 = read_ao(AO_CECB_GEN_CNTL);
		pos += snprintf(buf + pos, PAGE_SIZE, "AO_CECB_GEN_CNTL: 0x%08x\n",
			reg32);
		reg32 = read_ao(AO_CECB_RW_REG);
		pos += snprintf(buf + pos, PAGE_SIZE, "AO_CECB_RW_REG: 0x%08x\n",
			reg32);
		reg32 = read_ao(AO_CECB_INTR_MASKN);
		pos += snprintf(buf + pos, PAGE_SIZE, "AO_CECB_INTR_MASKN:0x%08x\n",
			reg32);
		reg32 = read_ao(AO_CECB_INTR_STAT);
		pos += snprintf(buf + pos, PAGE_SIZE, "AO_CECB_INTR_STAT: 0x%08x\n",
			reg32);

		pos += snprintf(buf + pos, PAGE_SIZE, "CEC MODULE REGS:\n");
		pos += snprintf(buf + pos, PAGE_SIZE, "CEC_CTRL = 0x%02x\n",
			hdmirx_cec_read(DWC_CEC_CTRL));
		if (cec_dev->plat_data->cecb_ver >= CECB_VER_2)
			pos += snprintf(buf + pos, PAGE_SIZE, "CEC_CTRL2 = 0x%02x\n",
				hdmirx_cec_read(DWC_CEC_CTRL2));
		pos += snprintf(buf + pos, PAGE_SIZE, "CEC_MASK = 0x%02x\n",
			hdmirx_cec_read(DWC_CEC_MASK));
		pos += snprintf(buf + pos, PAGE_SIZE, "CEC_ADDR_L = 0x%02x\n",
			hdmirx_cec_read(DWC_CEC_ADDR_L));
		pos += snprintf(buf + pos, PAGE_SIZE, "CEC_ADDR_H = 0x%02x\n",
			hdmirx_cec_read(DWC_CEC_ADDR_H));
		pos += snprintf(buf + pos, PAGE_SIZE, "CEC_TX_CNT = 0x%02x\n",
			hdmirx_cec_read(DWC_CEC_TX_CNT));
		pos += snprintf(buf + pos, PAGE_SIZE, "CEC_RX_CNT = 0x%02x\n",
			hdmirx_cec_read(DWC_CEC_RX_CNT));
		if (cec_dev->plat_data->cecb_ver >= CECB_VER_2)
			pos += snprintf(buf + pos, PAGE_SIZE, "CEC_STAT0 = 0x%02x\n",
				hdmirx_cec_read(DWC_CEC_STAT0));
		pos += snprintf(buf + pos, PAGE_SIZE, "CEC_LOCK = 0x%02x\n",
			hdmirx_cec_read(DWC_CEC_LOCK));
		pos += snprintf(buf + pos, PAGE_SIZE, "CEC_WKUPCTRL = 0x%02x\n",
			hdmirx_cec_read(DWC_CEC_WKUPCTRL));

		pos += snprintf(buf + pos, PAGE_SIZE, "%s", "RX buffer:");
		for (i = 0; i < 16; i++) {
			reg = hdmirx_cec_read(DWC_CEC_RX_DATA0 + i * 4) & 0xff;
			pos += snprintf(buf + pos, PAGE_SIZE, " %02x", reg);
		}
		pos += snprintf(buf + pos, PAGE_SIZE, "\n");

		pos += snprintf(buf + pos, PAGE_SIZE, "%s", "TX buffer:");
		for (i = 0; i < 16; i++) {
			reg = hdmirx_cec_read(DWC_CEC_TX_DATA0 + i * 4) & 0xff;
			pos += snprintf(buf + pos, PAGE_SIZE, " %02x", reg);
		}
		pos += snprintf(buf + pos, PAGE_SIZE, "\n");
	} else {
		for (i = 0; i < ARRAY_SIZE(cec_reg_name1); i++) {
			pos += snprintf(buf + pos, PAGE_SIZE, "%s:%2x\n",
				cec_reg_name1[i], aocec_rd_reg(i + 0x10));
		}

		for (i = 0; i < ARRAY_SIZE(cec_reg_name2); i++) {
			pos += snprintf(buf + pos, PAGE_SIZE, "%s:%2x\n",
				cec_reg_name2[i], aocec_rd_reg(i + 0x90));
		}

		if (cec_dev->plat_data->ceca_sts_reg) {
			for (i = 0; i < ARRAY_SIZE(ceca_reg_name3); i++) {
				pos += snprintf(buf + pos, PAGE_SIZE, "%s:%2x\n",
				 ceca_reg_name3[i], aocec_rd_reg(i + 0xA0));
			}
		}
	}

	return pos;
}

int cec_set_uevent(enum cec_event_type type, unsigned int val)
{
	char env[MAX_UEVENT_LEN];
	struct cec_uevent *event = cec_events;
	char *envp[2];
	int ret = -1;

	/* hdmi_plug/cec_rx_msg uevent may concurrent, need mutex */
	mutex_lock(&cec_dev->cec_uevent_mutex);
	for (event = cec_events; event->type != CEC_NONE_EVENT; event++) {
		if (type == event->type)
			break;
	}
	if (event->type == CEC_NONE_EVENT) {
		CEC_ERR("[%s] unsupported event:0x%x\n", __func__, type);
		mutex_unlock(&cec_dev->cec_uevent_mutex);
		return ret;
	}
	if (event->state == val) {
		CEC_INFO("[%s] state not chg:0x%x\n", __func__, val);
		mutex_unlock(&cec_dev->cec_uevent_mutex);
		return ret;
	}
	event->state = val;
	memset(env, 0, sizeof(env));
	envp[0] = env;
	envp[1] = NULL;
	snprintf(env, MAX_UEVENT_LEN, "%s%x", event->env, val);

	ret = kobject_uevent_env(&cec_dev->dbg_dev->kobj, KOBJ_CHANGE, envp);
	CEC_INFO("[%s] %s %d\n", __func__, env, ret);
	mutex_unlock(&cec_dev->cec_uevent_mutex);

	return ret;
}

/*for bringup no this api*/
int __attribute__((weak))cec_set_dev_info(uint8_t dev_idx)
{
	return 0;
}

int __attribute__((weak))hdmirx_get_connect_info(void)
{
	return 0;
}

/*
 * 0: cec off
 * 1: cec on, auto power_on is off
 * 2: cec on, auto power_on is on
 */
int __attribute__((weak))hdmirx_set_cec_cfg(u32 cfg)
{
	return 0;
}

inline unsigned int get_pin_status(void)
{
	unsigned int reg;

	switch (cec_dev->plat_data->chip_id) {
	case CEC_CHIP_SC2:
	case CEC_CHIP_S4:
		/* GPIOH_3 */
		reg = read_pad_reg(PADCTRL_GPIOH_I);
		reg = (reg >> 3) & 0x1;
		break;
	case CEC_CHIP_S5:
		/* GPIOH_3 */
		reg = read_pad_reg(PADCTRL_GPIOH_I_S5);
		reg = (reg >> 3) & 0x1;
		break;
	case CEC_CHIP_T7:
		/* GPIOW_12 CEC_A */
	case CEC_CHIP_T3:
		/* GPIOW_12 */
		reg = read_pad_reg(PADCTRL_GPIOW_I);
		reg = (reg >> 12) & 0x1;
		break;
	case CEC_CHIP_TM2:
		/* GPIOAO_10 */
		reg = read_ao(AO_GPIO_I);
		reg = (reg >> 10) & 0x1;
		break;
	case CEC_CHIP_T5D:
	case CEC_CHIP_T5W:
		/* case CEC_CHIP_T5: T5 doesn't have kernel5.4 branch */
		/* GPIOW_12 */
		reg = read_pad_reg(PREG_PAD_GPIO3_I);
		reg = (reg >> 12) & 0x1;
		break;
	default:
		/* means not implemented */
		reg = 0xFF;
		break;
	}

	return reg;
}

