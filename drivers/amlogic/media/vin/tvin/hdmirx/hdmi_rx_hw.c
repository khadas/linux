/*
 * drivers/amlogic/media/vin/tvin/hdmirx/hdmi_rx_hw.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include <linux/amlogic/media/vout/vdac_dev.h>
#include <linux/arm-smccc.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>

/* Local include */
#include "hdmi_rx_eq.h"
#include "hdmi_rx_repeater.h"
#include "hdmi_rx_drv.h"
#include "hdmi_rx_hw.h"
#include "hdmi_rx_edid.h"
#include "hdmi_rx_wrapper.h"

/*------------------------marco define------------------------------*/
#define SCRAMBLE_SEL 1
#define HYST_HDMI_TO_DVI 5
/* must = 0, other agilent source fail */
#define HYST_DVI_TO_HDMI 1
#define GCP_GLOBAVMUTE_EN 1 /* ag506 must clear this bit */
#define EDID_CLK_DIV 9 /* sys clk/(9+1) = 20M */
#define HDCP_KEY_WR_TRIES		(5)

/*------------------------variable define------------------------------*/
static DEFINE_SPINLOCK(reg_rw_lock);
/* should enable fast switching, since some devices in non-current port */
/* will suspend because of RxSense = 0, such as xiaomi-mtk box */
static bool phy_fast_switching;
static bool phy_fsm_enhancement = true;
/*unsigned int last_clk_rate;*/
static uint32_t modet_clk = 24000;
int hdcp_enc_mode;
/* top_irq_en bit[16:13] hdcp_sts */
int top_intr_maskn_value = 0x20000001;

bool hdcp_enable = 1;
int acr_mode;
int auto_aclk_mute = 2;
int aud_avmute_en = 1;
int aud_mute_sel = 2;
int force_clk_rate;
int md_ists_en = VIDEO_MODE;
int pdec_ists_en;/* = AVI_CKS_CHG | DVIDET | DRM_CKS_CHG | DRM_RCV_EN;*/
uint32_t packet_fifo_cfg;
int pd_fifo_start_cnt = 0x80;
/* Controls equalizer reference voltage. */
int hdcp22_on;
MODULE_PARM_DESC(hdcp22_on, "\n hdcp22_on\n");
module_param(hdcp22_on, int, 0664);

/* test for HBR CTS, audio module can set it to force 8ch */
int hbr_force_8ch;
/*
 * hdcp14_key_mode:hdcp1.4 key handle method select
 * NORMAL_MODE:systemcontrol path
 * SECURE_MODE:secure OS path
 */
int hdcp14_key_mode = NORMAL_MODE;
int aud_ch_map;
int ignore_sscp_charerr = 1;
int ignore_sscp_tmds = 1;
int find_best_eq;
int eq_try_cnt = 20;
int pll_rst_max = 5;
/* cdr lock threshold */
int cdr_lock_level;
u32 term_cal_val;
bool term_cal_en;
int clock_lock_th = 2;
int scdc_force_en = 1;
/* for hdcp_hpd debug, disable by default */
bool hdcp_hpd_ctrl_en;
int eq_dbg_lvl;
u32 phy_trim_val;
/* bit 4: tdr enable bit
 * bit [3:0]: tdr level control
 */
int phy_term_lel;
bool phy_tdr_en;

/*------------------------variable define end------------------------------*/

static int check_regmap_flag(unsigned int addr)
{
	return 1;
}

/*
 * hdmirx_rd_dwc - Read data from HDMI RX CTRL
 * @addr: register address
 *
 * return data read value
 */
unsigned int hdmirx_rd_dwc(unsigned int addr)
{
	ulong flags;
	int data;
	unsigned long dev_offset = 0x10;

	if (rx.chip_id >= CHIP_ID_TL1) {
		spin_lock_irqsave(&reg_rw_lock, flags);
		data = rd_reg(MAP_ADDR_MODULE_TOP,
			addr + reg_maps[MAP_ADDR_MODULE_TOP].phy_addr);
		spin_unlock_irqrestore(&reg_rw_lock, flags);
	} else {
		spin_lock_irqsave(&reg_rw_lock, flags);
		wr_reg(MAP_ADDR_MODULE_TOP,
			hdmirx_addr_port | dev_offset, addr);
		data = rd_reg(MAP_ADDR_MODULE_TOP,
			hdmirx_data_port | dev_offset);
		spin_unlock_irqrestore(&reg_rw_lock, flags);
	}
	return data;
}

/*
 * hdmirx_rd_bits_dwc - read specfied bits of HDMI RX CTRL reg
 * @addr: register address
 * @mask: bits mask
 *
 * return masked bits of register value
 */
unsigned int hdmirx_rd_bits_dwc(unsigned int addr, unsigned int mask)
{
	return rx_get_bits(hdmirx_rd_dwc(addr), mask);
}

/*
 * hdmirx_wr_dwc - Write data to HDMI RX CTRL
 * @addr: register address
 * @data: new register value
 */
void hdmirx_wr_dwc(unsigned int addr, unsigned int data)
{
	ulong flags;
	unsigned int dev_offset = 0x10;

	if (rx.chip_id >= CHIP_ID_TL1) {
		spin_lock_irqsave(&reg_rw_lock, flags);
		wr_reg(MAP_ADDR_MODULE_TOP,
			addr + reg_maps[MAP_ADDR_MODULE_TOP].phy_addr, data);
		spin_unlock_irqrestore(&reg_rw_lock, flags);
	} else {
		spin_lock_irqsave(&reg_rw_lock, flags);
		wr_reg(MAP_ADDR_MODULE_TOP,
			hdmirx_addr_port | dev_offset, addr);
		wr_reg(MAP_ADDR_MODULE_TOP,
			hdmirx_data_port | dev_offset, data);
		spin_unlock_irqrestore(&reg_rw_lock, flags);
	}
}

/*
 * hdmirx_wr_bits_dwc - write specfied bits of HDMI RX CTRL reg
 * @addr: register address
 * @mask: bits mask
 * @value: new register value
 */
void hdmirx_wr_bits_dwc(unsigned int addr,
	unsigned int mask, unsigned int value)
{
	hdmirx_wr_dwc(addr, rx_set_bits(hdmirx_rd_dwc(addr), mask, value));
}

/*
 * hdmirx_rd_phy - Read data from HDMI RX phy
 * @addr: register address
 *
 * return data read value
 */
unsigned int hdmirx_rd_phy(unsigned int reg_address)
{
	int cnt = 0;

	/* hdmirx_wr_dwc(DWC_I2CM_PHYG3_SLAVE, 0x39); */
	hdmirx_wr_dwc(DWC_I2CM_PHYG3_ADDRESS, reg_address);
	hdmirx_wr_dwc(DWC_I2CM_PHYG3_OPERATION, 0x02);
	do {
		if ((cnt % 10) == 0) {
			/* wait i2cmpdone */
			if (hdmirx_rd_dwc(DWC_HDMI_ISTS)&(1<<28)) {
				hdmirx_wr_dwc(DWC_HDMI_ICLR, 1<<28);
				break;
			}
		}
		cnt++;
		if (cnt > 50000) {
			rx_pr("[HDMIRX err]: %s(%x,%x) timeout\n",
				__func__, 0x39, reg_address);
			break;
		}
	} while (1);

	return (unsigned int)(hdmirx_rd_dwc(DWC_I2CM_PHYG3_DATAI));
}

/*
 * hdmirx_rd_bits_phy - read specfied bits of HDMI RX phy reg
 * @addr: register address
 * @mask: bits mask
 *
 * return masked bits of register value
 */
unsigned int hdmirx_rd_bits_phy(unsigned int addr, unsigned int mask)
{
	return rx_get_bits(hdmirx_rd_phy(addr), mask);
}

/*
 * hdmirx_wr_phy - Write data to HDMI RX phy
 * @addr: register address
 * @data: new register value
 *
 * return 0 on write succeed, return -1 otherwise.
 */
unsigned int hdmirx_wr_phy(unsigned int reg_address, unsigned int data)
{
	int error = 0;
	int cnt = 0;

	/* hdmirx_wr_dwc(DWC_I2CM_PHYG3_SLAVE, 0x39); */
	hdmirx_wr_dwc(DWC_I2CM_PHYG3_ADDRESS, reg_address);
	hdmirx_wr_dwc(DWC_I2CM_PHYG3_DATAO, data);
	hdmirx_wr_dwc(DWC_I2CM_PHYG3_OPERATION, 0x01);

	do {
		/* wait i2cmpdone */
		if ((cnt % 10) == 0) {
			if (hdmirx_rd_dwc(DWC_HDMI_ISTS)&(1<<28)) {
				hdmirx_wr_dwc(DWC_HDMI_ICLR, 1<<28);
				break;
			}
		}
		cnt++;
		if (cnt > 50000) {
			error = -1;
			rx_pr("[err-%s]:(%x,%x)timeout\n",
					__func__, reg_address, data);
			break;
		}
	} while (1);
	return error;
}

/*
 * hdmirx_wr_bits_phy - write specfied bits of HDMI RX phy reg
 * @addr: register address
 * @mask: bits mask
 * @value: new register value
 *
 * return 0 on write succeed, return -1 otherwise.
 */
int hdmirx_wr_bits_phy(uint16_t addr, uint32_t mask, uint32_t value)
{
	return hdmirx_wr_phy(addr, rx_set_bits(hdmirx_rd_phy(addr),
		mask, value));
}

/*
 * hdmirx_rd_top - read hdmirx top reg
 * @addr: register address
 *
 * return data read value
 */
unsigned int hdmirx_rd_top(unsigned int addr)
{
	ulong flags;
	int data;
	unsigned int dev_offset = 0;

	if (rx.chip_id >= CHIP_ID_TL1) {
		spin_lock_irqsave(&reg_rw_lock, flags);
		dev_offset = TOP_DWC_BASE_OFFSET +
			reg_maps[MAP_ADDR_MODULE_TOP].phy_addr;
		if ((addr >= TOP_EDID_ADDR_S) &&
			(addr <= (TOP_EDID_PORT3_ADDR_E))) {
			data = rd_reg_b(MAP_ADDR_MODULE_TOP,
				dev_offset + addr);
		} else {
			data = rd_reg(MAP_ADDR_MODULE_TOP,
				dev_offset + (addr<<2));
		}
		spin_unlock_irqrestore(&reg_rw_lock, flags);
	} else {
		spin_lock_irqsave(&reg_rw_lock, flags);
		wr_reg(MAP_ADDR_MODULE_TOP,
			hdmirx_addr_port | dev_offset, addr);
		data = rd_reg(MAP_ADDR_MODULE_TOP,
			hdmirx_data_port | dev_offset);
		spin_unlock_irqrestore(&reg_rw_lock, flags);
	}
	return data;
}

/*
 * hdmirx_rd_bits_top - read specified bits of hdmirx top reg
 * @addr: register address
 * @mask: bits mask
 *
 * return masked bits of register value
 */
uint32_t hdmirx_rd_bits_top(uint16_t addr, uint32_t mask)
{
	return rx_get_bits(hdmirx_rd_top(addr), mask);
}

/*
 * hdmirx_wr_top - Write data to hdmirx top reg
 * @addr: register address
 * @data: new register value
 */
void hdmirx_wr_top(unsigned int addr, unsigned int data)
{
	ulong flags;
	unsigned long dev_offset = 0;

	if (rx.chip_id >= CHIP_ID_TL1) {
		spin_lock_irqsave(&reg_rw_lock, flags);
		dev_offset = TOP_DWC_BASE_OFFSET +
			reg_maps[MAP_ADDR_MODULE_TOP].phy_addr;
		if ((addr >= TOP_EDID_ADDR_S) &&
			(addr <= (TOP_EDID_PORT3_ADDR_E))) {
			wr_reg_b(MAP_ADDR_MODULE_TOP,
				dev_offset + addr, (unsigned char)data);
		} else {
			wr_reg(MAP_ADDR_MODULE_TOP,
				dev_offset + (addr<<2), data);
		}
		spin_unlock_irqrestore(&reg_rw_lock, flags);
	} else {
		spin_lock_irqsave(&reg_rw_lock, flags);
		wr_reg(MAP_ADDR_MODULE_TOP,
			hdmirx_addr_port | dev_offset, addr);
		wr_reg(MAP_ADDR_MODULE_TOP,
			hdmirx_data_port | dev_offset, data);
		spin_unlock_irqrestore(&reg_rw_lock, flags);
	}
}

/*
 * hdmirx_wr_bits_top - write specified bits of hdmirx top reg
 * @addr: register address
 * @mask: bits mask
 * @value: new register value
 */
void hdmirx_wr_bits_top(unsigned int addr,
	unsigned int mask, unsigned int value)
{
	hdmirx_wr_top(addr, rx_set_bits(hdmirx_rd_top(addr), mask, value));
}

/*
 * rd_reg_hhi
 * @offset: offset address of hhi physical addr
 *
 * returns unsigned int bytes read from the addr
 */
unsigned int rd_reg_hhi(unsigned int offset)
{
	unsigned int ret;
	unsigned long flags;
	unsigned int addr;

	spin_lock_irqsave(&reg_rw_lock, flags);
	addr = offset +
		reg_maps[MAP_ADDR_MODULE_HIU].phy_addr;
	ret = rd_reg(MAP_ADDR_MODULE_HIU, addr);
	spin_unlock_irqrestore(&reg_rw_lock, flags);
	return ret;
}

/*
 * rd_reg_hhi_bits - read specfied bits of HHI reg
 * @addr: register address
 * @mask: bits mask
 *
 * return masked bits of register value
 */
unsigned int rd_reg_hhi_bits(unsigned int offset, unsigned int mask)
{
	return rx_get_bits(rd_reg_hhi(offset), mask);
}

/*
 * wr_reg_hhi
 * @offset: offset address of hhi physical addr
 * @val: value being written
 */
void wr_reg_hhi(unsigned int offset, unsigned int val)
{
	unsigned long flags;
	unsigned int addr;

	spin_lock_irqsave(&reg_rw_lock, flags);
	addr = offset +
		reg_maps[MAP_ADDR_MODULE_HIU].phy_addr;
	wr_reg(MAP_ADDR_MODULE_HIU, addr, val);
	spin_unlock_irqrestore(&reg_rw_lock, flags);
}

/*
 * wr_reg_hhi_bits
 * @offset: offset address of hhi physical addr
 * @mask: modify bits mask
 * @val: value being written
 */
void wr_reg_hhi_bits(unsigned int offset, unsigned int mask, unsigned int val)
{
	wr_reg_hhi(offset, rx_set_bits(rd_reg_hhi(offset), mask, val));
}

/*
 * rd_reg - regisger read
 * @module: module index of the reg_map table
 * @reg_addr: offset address of specified phy addr
 *
 * returns unsigned int bytes read from the addr
 */
unsigned int rd_reg(enum map_addr_module_e module,
	unsigned int reg_addr)
{
	unsigned int val = 0;

	if ((module < MAP_ADDR_MODULE_NUM)
		&& check_regmap_flag(reg_addr))
		val = readl(reg_maps[module].p +
		(reg_addr - reg_maps[module].phy_addr));
	else
		rx_pr("rd reg %x error,md %d\n", reg_addr, module);
	return val;
}

/*
 * wr_reg - regisger write
 * @module: module index of the reg_map table
 * @reg_addr: offset address of specified phy addr
 * @val: value being written
 */
void wr_reg(enum map_addr_module_e module,
		unsigned int reg_addr, unsigned int val)
{
	if ((module < MAP_ADDR_MODULE_NUM)
		&& check_regmap_flag(reg_addr))
		writel(val, reg_maps[module].p +
		(reg_addr - reg_maps[module].phy_addr));
	else
		rx_pr("wr reg %x err\n", reg_addr);
}

/*
 * rd_reg_b - regisger read byte mode
 * @module: module index of the reg_map table
 * @reg_addr: offset address of specified phy addr
 *
 * returns unsigned char bytes read from the addr
 */
unsigned char rd_reg_b(enum map_addr_module_e module,
	unsigned int reg_addr)
{
	unsigned char val = 0;

	if ((module < MAP_ADDR_MODULE_NUM)
		&& check_regmap_flag(reg_addr))
		val = readb(reg_maps[module].p +
		(reg_addr - reg_maps[module].phy_addr));
	else
		rx_pr("rd reg %x error,md %d\n", reg_addr, module);
	return val;
}

/*
 * wr_reg_b - regisger write byte mode
 * @module: module index of the reg_map table
 * @reg_addr: offset address of specified phy addr
 * @val: value being written
 */
void wr_reg_b(enum map_addr_module_e module,
		unsigned int reg_addr, unsigned char val)
{
	if ((module < MAP_ADDR_MODULE_NUM)
		&& check_regmap_flag(reg_addr))
		writeb(val, reg_maps[module].p +
		(reg_addr - reg_maps[module].phy_addr));
	else
		rx_pr("wr reg %x err\n", reg_addr);
}


/*
 * rx_hdcp22_wr_only
 */
void rx_hdcp22_wr_only(unsigned int addr, unsigned int data)
{
	unsigned long flags;

	spin_lock_irqsave(&reg_rw_lock, flags);
	wr_reg(MAP_ADDR_MODULE_HDMIRX_CAPB3,
	reg_maps[MAP_ADDR_MODULE_HDMIRX_CAPB3].phy_addr | addr,
	data);
	spin_unlock_irqrestore(&reg_rw_lock, flags);
}

unsigned int rx_hdcp22_rd(unsigned int addr)
{
	unsigned int data;
	unsigned long flags;

	spin_lock_irqsave(&reg_rw_lock, flags);
	data = rd_reg(MAP_ADDR_MODULE_HDMIRX_CAPB3,
	reg_maps[MAP_ADDR_MODULE_HDMIRX_CAPB3].phy_addr | addr);
	spin_unlock_irqrestore(&reg_rw_lock, flags);
	return data;
}

void rx_hdcp22_rd_check(unsigned int addr,
	unsigned int exp_data, unsigned int mask)
{
	unsigned int rd_data;

	rd_data = rx_hdcp22_rd(addr);
	if ((rd_data | mask) != (exp_data | mask))
		rx_pr("addr=0x%02x rd_data=0x%08x\n", addr, rd_data);
}

void rx_hdcp22_wr(unsigned int addr, unsigned int data)
{
	rx_hdcp22_wr_only(addr, data);
	rx_hdcp22_rd_check(addr, data, 0);
}

/*
 * rx_hdcp22_rd_reg - hdcp2.2 reg write
 * @addr: register address
 * @value: new register value
 */
void rx_hdcp22_wr_reg(unsigned int addr, unsigned int data)
{
	rx_sec_reg_write((unsigned int *)(unsigned long)
	(reg_maps[MAP_ADDR_MODULE_HDMIRX_CAPB3].phy_addr + addr),
	data);
}

/*
 * rx_hdcp22_rd_reg - hdcp2.2 reg read
 * @addr: register address
 */
unsigned int rx_hdcp22_rd_reg(unsigned int addr)
{
	return (uint32_t)rx_sec_reg_read((unsigned int *)(unsigned long)
	(reg_maps[MAP_ADDR_MODULE_HDMIRX_CAPB3].phy_addr + addr));
}

/*
 * rx_hdcp22_rd_reg_bits - hdcp2.2 reg masked bits read
 * @addr: register address
 * @mask: bits mask
 */
unsigned int rx_hdcp22_rd_reg_bits(unsigned int addr, unsigned int mask)
{
	return rx_get_bits(rx_hdcp22_rd_reg(addr), mask);
}

/*
 * rx_hdcp22_wr_reg_bits - hdcp2.2 reg masked bits write
 * @addr: register address
 * @mask: bits mask
 * @value: new register value
 */
void rx_hdcp22_wr_reg_bits(unsigned int addr,
	unsigned int mask, unsigned int value)
{
	rx_hdcp22_wr_reg(addr, rx_set_bits(rx_hdcp22_rd_reg(addr),
			mask, value));
}

/*
 * hdcp22_wr_top - hdcp2.2 top reg write
 * @addr: register address
 * @data: new register value
 */
void rx_hdcp22_wr_top(unsigned int addr, unsigned int data)
{
	sec_top_write((unsigned int *)(unsigned long)addr, data);
}

/*
 * hdcp22_rd_top - hdcp2.2 top reg read
 * @addr: register address
 */
unsigned int rx_hdcp22_rd_top(uint32_t addr)
{
	return (unsigned int)sec_top_read((unsigned int *)(unsigned long)addr);
}

/*
 * sec_top_write - secure top write
 */
void sec_top_write(unsigned int *addr, unsigned int value)
{
	struct arm_smccc_res res;

	arm_smccc_smc(HDMIRX_WR_SEC_TOP, (unsigned long)(uintptr_t)addr,
					value, 0, 0, 0, 0, 0, &res);
}

/*
 * sec_top_read - secure top read
 */
unsigned int sec_top_read(unsigned int *addr)
{
	struct arm_smccc_res res;

	arm_smccc_smc(HDMIRX_RD_SEC_TOP, (unsigned long)(uintptr_t)addr,
					0, 0, 0, 0, 0, 0, &res);

	return (unsigned int)((res.a0)&0xffffffff);
}

/*
 * rx_sec_reg_write - secure region write
 */
void rx_sec_reg_write(unsigned int *addr, unsigned int value)
{
	struct arm_smccc_res res;

	arm_smccc_smc(HDCP22_RX_ESM_WRITE, (unsigned long)(uintptr_t)addr,
				value, 0, 0, 0, 0, 0, &res);
}

/*
 * rx_sec_reg_read - secure region read
 */
unsigned int rx_sec_reg_read(unsigned int *addr)
{
	struct arm_smccc_res res;

	arm_smccc_smc(HDCP22_RX_ESM_READ, (unsigned long)(uintptr_t)addr,
					0, 0, 0, 0, 0, 0, &res);

	return (unsigned int)((res.a0)&0xffffffff);
}

/*
 * rx_sec_set_duk
 */
unsigned int rx_sec_set_duk(bool repeater)
{
	struct arm_smccc_res res;

	if (repeater)
		arm_smccc_smc(HDCP22_RP_SET_DUK_KEY, 0, 0, 0, 0, 0, 0, 0, &res);
	else
		arm_smccc_smc(HDCP22_RX_SET_DUK_KEY, 0, 0, 0, 0, 0, 0, 0, &res);

	return (unsigned int)((res.a0)&0xffffffff);
}

/*
 * rx_set_hdcp14_secure_key
 */
unsigned int rx_set_hdcp14_secure_key(void)
{
	struct arm_smccc_res res;

	/* 0x8200002d is the SMC cmd defined in BL31,this CMD
	 * will call set hdcp1.4 key function
	 */
	arm_smccc_smc(HDCP14_RX_SETKEY, 0, 0, 0, 0, 0, 0, 0, &res);

	return (unsigned int)((res.a0)&0xffffffff);
}

/*
 * hdmirx_phy_pddq - phy pddq config
 * @enable: enable phy pddq up
 */
void hdmirx_phy_pddq(unsigned int enable)
{
	uint32_t term_value =
		hdmirx_rd_top(TOP_HPD_PWR5V) & 0x7;

	if (rx.chip_id >= CHIP_ID_TL1) {
		wr_reg_hhi_bits(HHI_HDMIRX_PHY_MISC_CNTL2, _BIT(1), enable);
		/* set rxsense */
		if (enable)
			wr_reg_hhi_bits(HHI_HDMIRX_PHY_MISC_CNTL0,
				MSK(3, 0), 0);
		else
			wr_reg_hhi_bits(HHI_HDMIRX_PHY_MISC_CNTL0,
				MSK(3, 0), term_value);
	}
	else
		hdmirx_wr_bits_dwc(DWC_SNPS_PHYG3_CTRL,
			MSK(1, 1), enable);
}

/*
 * hdmirx_wr_ctl_port
 */
void hdmirx_wr_ctl_port(unsigned int offset, unsigned int data)
{
	unsigned long flags;
	if (rx.chip_id < CHIP_ID_TL1) {
		spin_lock_irqsave(&reg_rw_lock, flags);
		wr_reg(MAP_ADDR_MODULE_TOP, hdmirx_ctrl_port+offset, data);
		spin_unlock_irqrestore(&reg_rw_lock, flags);
	}
}

/*
 * hdmirx_top_sw_reset
 */
void hdmirx_top_sw_reset(void)
{
	ulong flags;
	unsigned long dev_offset = 0;
	spin_lock_irqsave(&reg_rw_lock, flags);
	if (rx.chip_id >= CHIP_ID_TL1) {
		dev_offset = TOP_DWC_BASE_OFFSET +
			reg_maps[MAP_ADDR_MODULE_TOP].phy_addr;
		wr_reg(MAP_ADDR_MODULE_TOP,
			dev_offset + TOP_SW_RESET, 1);
		udelay(1);
		wr_reg(MAP_ADDR_MODULE_TOP,
			dev_offset + TOP_SW_RESET, 0);
	} else {
		wr_reg(MAP_ADDR_MODULE_TOP,
			hdmirx_addr_port | dev_offset, TOP_SW_RESET);
		wr_reg(MAP_ADDR_MODULE_TOP,
			hdmirx_data_port | dev_offset, 1);
		udelay(1);
		wr_reg(MAP_ADDR_MODULE_TOP,
			hdmirx_addr_port | dev_offset, TOP_SW_RESET);
		wr_reg(MAP_ADDR_MODULE_TOP,
			hdmirx_data_port | dev_offset, 0);
	}
	spin_unlock_irqrestore(&reg_rw_lock, flags);
}

/*
 * rx_irq_en - hdmirx controller irq config
 * @enable - irq set or clear
 */
void rx_irq_en(bool enable)
{
	unsigned int data32 = 0;

	if (enable) {
		if (rx.chip_id >= CHIP_ID_TL1) {
			data32 |= 1 << 31; /* DRC_CKS_CHG */
			data32 |= 1 << 30; /* DRC_RCV */
			data32 |= 0 << 29; /* AUD_TYPE_CHG */
			data32 |= 0 << 28; /* DVI_DET */
			data32 |= 1 << 27; /* VSI_CKS_CHG */
			data32 |= 0 << 26; /* GMD_CKS_CHG */
			data32 |= 0 << 25; /* AIF_CKS_CHG */
			data32 |= 1 << 24; /* AVI_CKS_CHG */
			data32 |= 0 << 23; /* ACR_N_CHG */
			data32 |= 0 << 22; /* ACR_CTS_CHG */
			data32 |= 1 << 21; /* GCP_AV_MUTE_CHG */
			data32 |= 0 << 20; /* GMD_RCV */
			data32 |= 0 << 19; /* AIF_RCV */
			data32 |= 0 << 18; /* AVI_RCV */
			data32 |= 0 << 17; /* ACR_RCV */
			data32 |= 0 << 16; /* GCP_RCV */
			data32 |= 1 << 15; /* VSI_RCV */
			data32 |= 0 << 14; /* AMP_RCV */
			data32 |= 0 << 13; /* AMP_CHG */
			data32 |= 0 << 9; /* EMP_RCV*/
			data32 |= 0 << 8; /* PD_FIFO_NEW_ENTRY */
			data32 |= 0 << 4; /* PD_FIFO_OVERFL */
			data32 |= 0 << 3; /* PD_FIFO_UNDERFL */
			data32 |= 0 << 2; /* PD_FIFO_TH_START_PASS */
			data32 |= 0 << 1; /* PD_FIFO_TH_MAX_PASS */
			data32 |= 0 << 0; /* PD_FIFO_TH_MIN_PASS */
			data32 |= pdec_ists_en;
		} else if (rx.chip_id == CHIP_ID_TXLX) {
			data32 |= 1 << 31; /* DRC_CKS_CHG */
			data32 |= 1 << 30; /* DRC_RCV */
			data32 |= 0 << 29; /* AUD_TYPE_CHG */
			data32 |= 0 << 28; /* DVI_DET */
			data32 |= 1 << 27; /* VSI_CKS_CHG */
			data32 |= 0 << 26; /* GMD_CKS_CHG */
			data32 |= 0 << 25; /* AIF_CKS_CHG */
			data32 |= 1 << 24; /* AVI_CKS_CHG */
			data32 |= 0 << 23; /* ACR_N_CHG */
			data32 |= 0 << 22; /* ACR_CTS_CHG */
			data32 |= 1 << 21; /* GCP_AV_MUTE_CHG */
			data32 |= 0 << 20; /* GMD_RCV */
			data32 |= 0 << 19; /* AIF_RCV */
			data32 |= 0 << 18; /* AVI_RCV */
			data32 |= 0 << 17; /* ACR_RCV */
			data32 |= 0 << 16; /* GCP_RCV */
			data32 |= 1 << 15; /* VSI_RCV */
			data32 |= 0 << 14; /* AMP_RCV */
			data32 |= 0 << 13; /* AMP_CHG */
			data32 |= 0 << 8; /* PD_FIFO_NEW_ENTRY */
			data32 |= 0 << 4; /* PD_FIFO_OVERFL */
			data32 |= 0 << 3; /* PD_FIFO_UNDERFL */
			data32 |= 0 << 2; /* PD_FIFO_TH_START_PASS */
			data32 |= 0 << 1; /* PD_FIFO_TH_MAX_PASS */
			data32 |= 0 << 0; /* PD_FIFO_TH_MIN_PASS */
			data32 |= pdec_ists_en;
		} else if (rx.chip_id == CHIP_ID_TXHD) {
			/* data32 |= 1 << 31;  DRC_CKS_CHG */
			/* data32 |= 1 << 30; DRC_RCV */
			data32 |= 0 << 29; /* AUD_TYPE_CHG */
			data32 |= 0 << 28; /* DVI_DET */
			data32 |= 1 << 27; /* VSI_CKS_CHG */
			data32 |= 0 << 26; /* GMD_CKS_CHG */
			data32 |= 0 << 25; /* AIF_CKS_CHG */
			data32 |= 1 << 24; /* AVI_CKS_CHG */
			data32 |= 0 << 23; /* ACR_N_CHG */
			data32 |= 0 << 22; /* ACR_CTS_CHG */
			data32 |= 1 << 21; /* GCP_AV_MUTE_CHG */
			data32 |= 0 << 20; /* GMD_RCV */
			data32 |= 0 << 19; /* AIF_RCV */
			data32 |= 0 << 18; /* AVI_RCV */
			data32 |= 0 << 17; /* ACR_RCV */
			data32 |= 0 << 16; /* GCP_RCV */
			data32 |= 1 << 15; /* VSI_RCV */
			/* data32 |= 0 << 14;  AMP_RCV */
			/* data32 |= 0 << 13;  AMP_CHG */
			data32 |= 0 << 8; /* PD_FIFO_NEW_ENTRY */
			data32 |= 0 << 4; /* PD_FIFO_OVERFL */
			data32 |= 0 << 3; /* PD_FIFO_UNDERFL */
			data32 |= 0 << 2; /* PD_FIFO_TH_START_PASS */
			data32 |= 0 << 1; /* PD_FIFO_TH_MAX_PASS */
			data32 |= 0 << 0; /* PD_FIFO_TH_MIN_PASS */
			data32 |= pdec_ists_en;
		} else { /* TXL and previous Chip */
			data32 = 0;
			data32 |= 0 << 29; /* AUD_TYPE_CHG */
			data32 |= 0 << 28; /* DVI_DET */
			data32 |= 1 << 27; /* VSI_CKS_CHG */
			data32 |= 0 << 26; /* GMD_CKS_CHG */
			data32 |= 0 << 25; /* AIF_CKS_CHG */
			data32 |= 1 << 24; /* AVI_CKS_CHG */
			data32 |= 0 << 23; /* ACR_N_CHG */
			data32 |= 0 << 22; /* ACR_CTS_CHG */
			data32 |= 1 << 21; /* GCP_AV_MUTE_CHG */
			data32 |= 0 << 20; /* GMD_RCV */
			data32 |= 0 << 19; /* AIF_RCV */
			data32 |= 0 << 18; /* AVI_RCV */
			data32 |= 0 << 17; /* ACR_RCV */
			data32 |= 0 << 16; /* GCP_RCV */
			data32 |= 0 << 15; /* VSI_RCV */
			data32 |= 0 << 14; /* AMP_RCV */
			data32 |= 0 << 13; /* AMP_CHG */
			/* diff */
			data32 |= 1 << 10; /* DRC_CKS_CHG */
			data32 |= 1 << 9; /* DRC_RCV */
			/* diff */
			data32 |= 0 << 8; /* PD_FIFO_NEW_ENTRY */
			data32 |= 0 << 4; /* PD_FIFO_OVERFL */
			data32 |= 0 << 3; /* PD_FIFO_UNDERFL */
			data32 |= 0 << 2; /* PD_FIFO_TH_START_PASS */
			data32 |= 0 << 1; /* PD_FIFO_TH_MAX_PASS */
			data32 |= 0 << 0; /* PD_FIFO_TH_MIN_PASS */
			data32 |= pdec_ists_en;
		}
		hdmirx_wr_dwc(DWC_PDEC_IEN_SET, data32);
		hdmirx_wr_dwc(DWC_AUD_FIFO_IEN_SET, OVERFL|UNDERFL);
	} else {
		/* clear enable */
		hdmirx_wr_dwc(DWC_PDEC_IEN_CLR, ~0);
		hdmirx_wr_dwc(DWC_AUD_CEC_IEN_CLR, ~0);
		hdmirx_wr_dwc(DWC_AUD_FIFO_IEN_CLR, ~0);
		hdmirx_wr_dwc(DWC_MD_IEN_CLR, ~0);
		/* clear status */
		hdmirx_wr_dwc(DWC_PDEC_ICLR, ~0);
		hdmirx_wr_dwc(DWC_AUD_CEC_ICLR, ~0);
		hdmirx_wr_dwc(DWC_AUD_FIFO_ICLR, ~0);
		hdmirx_wr_dwc(DWC_MD_ICLR, ~0);
	}
}

/*
 * hdmirx_irq_hdcp_enable - hdcp irq enalbe
 */
void hdmirx_irq_hdcp_enable(bool enable)
{
	if (enable) {
		/* hdcp2.2 */
		if (hdcp22_on)
			hdmirx_wr_dwc(DWC_HDMI2_IEN_SET, 0x1f);
		/* hdcp1.4 */
		hdmirx_wr_dwc(DWC_HDMI_IEN_SET, AKSV_RCV);
	} else {
		/* hdcp2.2 */
		if (hdcp22_on) {
			/* clear enable */
			hdmirx_wr_dwc(DWC_HDMI2_IEN_CLR, ~0);
			/* clear status */
			hdmirx_wr_dwc(DWC_HDMI2_ICLR, ~0);
		}
		/* hdcp1.4 */
		/* clear enable */
		hdmirx_wr_dwc(DWC_HDMI_IEN_CLR, ~0);
		/* clear status */
		hdmirx_wr_dwc(DWC_HDMI_ICLR, ~0);
	}
}

void hdmirx_top_irq_en(bool flag)
{
	if (flag)
		hdmirx_wr_top(TOP_INTR_MASKN,
			top_intr_maskn_value);
	else
		hdmirx_wr_top(TOP_INTR_MASKN, 0);
}

/*
 * rx_get_audinfo - get aduio info
 */
void rx_get_audinfo(struct aud_info_s *audio_info)
{
	audio_info->coding_type =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, CODING_TYPE);
	audio_info->channel_count =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, CHANNEL_COUNT);

	audio_info->sample_frequency =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, SAMPLE_FREQ);
	audio_info->sample_size =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, SAMPLE_SIZE);
	audio_info->coding_extension =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, AIF_DATA_BYTE_3);
	audio_info->auds_ch_alloc =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, CH_SPEAK_ALLOC);
	audio_info->auds_layout =
		hdmirx_rd_bits_dwc(DWC_PDEC_STS, PD_AUD_LAYOUT);
	audio_info->aud_hbr_rcv =
		hdmirx_rd_dwc(DWC_PDEC_AUD_STS) & AUDS_HBR_RCV;
	audio_info->aud_packet_received =
			hdmirx_rd_dwc(DWC_PDEC_AUD_STS) &
			(AUDS_RCV | AUDS_HBR_RCV);
	audio_info->cts = hdmirx_rd_dwc(DWC_PDEC_ACR_CTS);

	audio_info->n = hdmirx_rd_dwc(DWC_PDEC_ACR_N);
	if (audio_info->cts != 0) {
		audio_info->arc =
			(rx_measure_clock(MEASURE_CLK_TMDS)/audio_info->cts)*
			audio_info->n/128;
	} else
		audio_info->arc = 0;

	audio_info->aud_clk = rx_measure_clock(MEASURE_CLK_AUD_PLL);
}

/*
 * rx_get_audio_status - interface for audio module
 */
void rx_get_audio_status(struct rx_audio_stat_s *aud_sts)
{
	if ((rx.state == FSM_SIG_READY) &&
		(rx.pre.sw_vic != HDMI_UNKNOWN) &&
		(rx.pre.sw_vic != HDMI_UNSUPPORT) &&
		(rx.avmute_skip == 0)) {
		aud_sts->aud_rcv_packet = rx.aud_info.aud_packet_received;
		aud_sts->aud_stb_flag = true;
		aud_sts->aud_alloc = rx.aud_info.auds_ch_alloc;
		aud_sts->aud_sr = rx.aud_info.real_sr;
		aud_sts->aud_channel_cnt = rx.aud_info.channel_count;
		aud_sts->aud_type = rx.aud_info.coding_type;
		aud_sts->afifo_thres_pass =
			((hdmirx_rd_dwc(DWC_AUD_FIFO_STS) &
			THS_PASS_STS) == 0) ? false : true;
	} else {
		memset(aud_sts, 0,
			sizeof(struct rx_audio_stat_s));
	}
}
EXPORT_SYMBOL(rx_get_audio_status);

/*
 * rx_get_audio_status - interface for audio module
 */

int rx_set_audio_param(uint32_t param)
{
	hbr_force_8ch = param & 1;
	return 1;
}
EXPORT_SYMBOL(rx_set_audio_param);

bool is_tl1_former(void)
{
	if (is_meson_tl1_cpu() &&
		is_meson_rev_a())
		return 1;
	return 0;
}
/*
 * rx_get_hdmi5v_sts - get current pwr5v status on all ports
 */
unsigned int rx_get_hdmi5v_sts(void)
{
	return (hdmirx_rd_top(TOP_HPD_PWR5V) >> 20) & 0xf;
}

/*
 * rx_get_hpd_sts - get current hpd status on all ports
 */
unsigned int rx_get_hpd_sts(void)
{
	return hdmirx_rd_top(TOP_HPD_PWR5V) & 0xf;
}

/*
 * rx_get_scdc_clkrate_sts - get tmds clk ratio
 */
unsigned int rx_get_scdc_clkrate_sts(void)
{
	uint32_t clk_rate = 0;

	if (rx.chip_id == CHIP_ID_TXHD)
		return 0;
	else {
		if (force_clk_rate & 0x10)
			clk_rate = force_clk_rate & 1;
		else
			clk_rate = (hdmirx_rd_dwc(DWC_SCDC_REGS0) >> 17) & 1;
	}
	return clk_rate;
}

/*
 * rx_get_pll_lock_sts - tmds pll lock indication
 * return true if tmds pll locked, false otherwise.
 */
unsigned int rx_get_pll_lock_sts(void)
{
	return hdmirx_rd_dwc(DWC_HDMI_PLL_LCK_STS) & 1;
}

/*
 * rx_get_aud_pll_lock_sts - audio pll lock indication
 * no use
 */
bool rx_get_aud_pll_lock_sts(void)
{
	/* if ((hdmirx_rd_dwc(DWC_AUD_PLL_CTRL) & (1 << 31)) == 0) */
	if ((rd_reg_hhi(HHI_AUD_PLL_CNTL_I) & (1 << 31)) == 0)
		return false;
	else
		return true;
}

/*
 * is_clk_stable - phy clock stable detection
 */
bool is_clk_stable(void)
{
	int clk = false;

	if (rx.chip_id >= CHIP_ID_TL1) {
		/* sqofclk */
		clk = hdmirx_rd_top(TOP_MISC_STAT0) & 0x1;
	} else {
		/* phy clk */
		clk = hdmirx_rd_phy(PHY_MAINFSM_STATUS1) & 0x100;
	}

	if (clk)
		return true;
	else
		return false;
}

void rx_afifo_store_all_subpkt(bool all_pkt)
{
	static bool flag = true;

	if (all_pkt) {
		if (log_level & AUDIO_LOG)
			rx_pr("afifo store all subpkts: %d\n", flag);
		/* when afifo overflow, try afifo store
		 * configuration alternatively
		 */
		if (flag)
			hdmirx_wr_bits_dwc(DWC_AUD_FIFO_CTRL,
				AFIF_SUBPACKETS, 0);
		else
			hdmirx_wr_bits_dwc(DWC_AUD_FIFO_CTRL,
				AFIF_SUBPACKETS, 1);
		flag = !flag;
	} else
		hdmirx_wr_bits_dwc(DWC_AUD_FIFO_CTRL,
			AFIF_SUBPACKETS, 1);
}

/*
 * hdmirx_audio_fifo_rst - reset afifo
 */
unsigned int hdmirx_audio_fifo_rst(void)
{
	int error = 0;

	hdmirx_wr_bits_dwc(DWC_AUD_FIFO_CTRL, AFIF_INIT, 1);
	udelay(20);
	hdmirx_wr_bits_dwc(DWC_AUD_FIFO_CTRL, AFIF_INIT, 0);
	hdmirx_wr_dwc(DWC_DMI_SW_RST, 0x10);
	if (log_level & AUDIO_LOG)
		rx_pr("%s\n", __func__);
	return error;
}

/*
 * hdmirx_control_clk_range
 */
int hdmirx_control_clk_range(unsigned long min, unsigned long max)
{
	int error = 0;
	unsigned int evaltime = 0;
	unsigned long ref_clk;

	ref_clk = modet_clk;
	evaltime = (ref_clk * 4095) / 158000;
	min = (min * evaltime) / ref_clk;
	max = (max * evaltime) / ref_clk;
	hdmirx_wr_bits_dwc(DWC_HDMI_CKM_F, MINFREQ, min);
	hdmirx_wr_bits_dwc(DWC_HDMI_CKM_F, CKM_MAXFREQ, max);
	return error;
}

/*
 * set_scdc_cfg
 */
void set_scdc_cfg(int hpdlow, int pwrprovided)
{
	if (rx.chip_id == CHIP_ID_TXHD)
		return;

	hdmirx_wr_dwc(DWC_SCDC_CONFIG,
		(hpdlow << 1) | (pwrprovided << 0));
}

/*
 * packet_init - packet receiving config
 */
int packet_init(void)
{
	int error = 0;
	int data32 = 0;

	data32 |= 1 << 12; /* emp_err_filter, tl1*/
	data32 |= 1 << 9; /* amp_err_filter */
	data32 |= 1 << 8; /* isrc_err_filter */
	data32 |= 1 << 7; /* gmd_err_filter */
	data32 |= 1 << 6; /* aif_err_filter */
	data32 |= 1 << 5; /* avi_err_filter */
	data32 |= 1 << 4; /* vsi_err_filter */
	data32 |= 1 << 3; /* gcp_err_filter */
	data32 |= 1 << 2; /* acrp_err_filter */
	data32 |= 1 << 1; /* ph_err_filter */
	data32 |= 0 << 0; /* checksum_err_filter */
	hdmirx_wr_dwc(DWC_PDEC_ERR_FILTER, data32);

	data32 = hdmirx_rd_dwc(DWC_PDEC_CTRL);
	data32 |= 1 << 31;	/* PFIFO_STORE_FILTER_EN */
	data32 |= 1 << 30;  /* Enable packet FIFO store EMP pkt*/
	data32 |= 1 << 4;	/* PD_FIFO_WE */
	data32 |= 0 << 1;	/* emp pkt rev int,0:last 1:every */
	data32 |= 1 << 0;	/* PDEC_BCH_EN */
	data32 &= (~GCP_GLOBAVMUTE);
	data32 |= GCP_GLOBAVMUTE_EN << 15;
	data32 |= packet_fifo_cfg;
	hdmirx_wr_dwc(DWC_PDEC_CTRL, data32);

	data32 = 0;
	data32 |= pd_fifo_start_cnt << 20;	/* PD_start */
	data32 |= 640 << 10;	/* PD_max */
	data32 |= 8 << 0;		/* PD_min */
	hdmirx_wr_dwc(DWC_PDEC_FIFO_CFG, data32);

	return error;
}

/*
 * pd_fifo_irq_ctl
 */
void pd_fifo_irq_ctl(bool en)
{
	int i = hdmirx_rd_dwc(DWC_PDEC_IEN);

	if (en == 0)
		hdmirx_wr_bits_dwc(DWC_PDEC_IEN_CLR, _BIT(2), 1);
	else
		hdmirx_wr_dwc(DWC_PDEC_IEN_SET, (_BIT(2) | i));
}

/*
 * hdmirx_packet_fifo_rst - reset packet fifo
 */
unsigned int hdmirx_packet_fifo_rst(void)
{
	int error = 0;

	hdmirx_wr_bits_dwc(DWC_PDEC_CTRL,
		PD_FIFO_FILL_INFO_CLR|PD_FIFO_CLR, ~0);
	hdmirx_wr_bits_dwc(DWC_PDEC_CTRL,
		PD_FIFO_FILL_INFO_CLR|PD_FIFO_CLR,  0);
	return error;
}

/*
 * TOP_init - hdmirx top initialization
 */
static int TOP_init(void)
{
	int err = 0;
	int data32 = 0;

	data32 |= (0xf	<< 13); /* bit[16:13] */
	data32 |= 0	<< 11;
	data32 |= 0	<< 10;
	data32 |= 0	<< 9;
	data32 |= 0 << 8;
	data32 |= EDID_CLK_DIV << 0;
	hdmirx_wr_top(TOP_EDID_GEN_CNTL,  data32);

	data32 = 0;
	/* SDA filter internal clk div */
	data32 |= 1 << 29;
	/* SDA sampling clk div */
	data32 |= 1 << 16;
	/* SCL filter internal clk div */
	data32 |= 1 << 13;
	/* SCL sampling clk div */
	data32 |= 1 << 0;
	hdmirx_wr_top(TOP_INFILTER_HDCP, data32);
	hdmirx_wr_top(TOP_INFILTER_I2C0, data32);
	hdmirx_wr_top(TOP_INFILTER_I2C1, data32);
	hdmirx_wr_top(TOP_INFILTER_I2C2, data32);
	hdmirx_wr_top(TOP_INFILTER_I2C3, data32);

	data32 = 0;
	/* conversion mode of 422 to 444 */
	data32 |= 0	<< 19;
	/* !!!!dolby vision 422 to 444 ctl bit */
	data32 |= 0	<< 0;
	hdmirx_wr_top(TOP_VID_CNTL,	data32);

	if (rx.chip_id != CHIP_ID_TXHD) {
		data32 = 0;
		data32 |= 0	<< 20;
		data32 |= 0	<< 8;
		data32 |= 0x0a	<< 0;
		hdmirx_wr_top(TOP_VID_CNTL2,  data32);
	}
	data32 = 0;
	if (rx.chip_id >= CHIP_ID_TL1) {
		/* n_cts_auto_mode: */
		/*	0-every ACR packet */
		/*	1-on N or CTS value change */
		data32 |= 1 << 4;
	}
	/* delay cycles before n/cts update pulse */
	data32 |= 7 << 0;
	if (rx.chip_id >= CHIP_ID_TL1)
		hdmirx_wr_top(TOP_TL1_ACR_CNTL2, data32);
	else
		hdmirx_wr_top(TOP_ACR_CNTL2, data32);

	if (rx.chip_id >= CHIP_ID_TL1) {
		data32 = hdmirx_rd_dwc(DWC_HDCP_CTRL);
		/* 0: Original behaviour */
		/* 1: Balance path delay between non-HDCP and HDCP */
		data32 |= 1 << 27; /* none & hdcp */
		/* 0: Original behaviour */
		/* 1: Balance path delay between HDCP14 and HDCP22. */
		data32 |= 1 << 26; /* 1.4 & 2.2 */
		hdmirx_wr_dwc(DWC_HDCP_CTRL, data32);

		/* Configure channel switch */
	    data32  = 0;
	    data32 |= (0 << 4); /* [  4]  valid_always*/
	    data32 |= (7 << 0); /* [3:0]  decoup_thresh*/
	    hdmirx_wr_top(TOP_CHAN_SWITCH_1, data32);

	    data32  = 0;
		data32 |= (2 << 28); /* [29:28]      source_2 */
		data32 |= (1 << 26); /* [27:26]      source_1 */
		data32 |= (0 << 24); /* [25:24]      source_0 */
	    hdmirx_wr_top(TOP_CHAN_SWITCH_0, data32);

		/* Configure TMDS algin */
		data32	= 0;
		hdmirx_wr_top(TOP_TMDS_ALIGN_CNTL0,	data32);
		data32	= 0;
		hdmirx_wr_top(TOP_TMDS_ALIGN_CNTL1,	data32);

		/* Enable channel output */
		data32 = hdmirx_rd_top(TOP_CHAN_SWITCH_0);
		hdmirx_wr_top(TOP_CHAN_SWITCH_0, data32 | (1<<0));

		/* configure cable clock measure */
		data32 = 0;
		data32 |= (1 << 28); /* [31:28] meas_tolerance */
		data32 |= (8192 << 0); /* [23: 0] ref_cycles */
		hdmirx_wr_top(TOP_METER_CABLE_CNTL, data32);
	}

	/* configure hdmi clock measure */
	data32 = 0;
	data32 |= (1 << 28); /* [31:28] meas_tolerance */
	data32 |= (8192 << 0); /* [23: 0] ref_cycles */
	hdmirx_wr_top(TOP_METER_HDMI_CNTL, data32);

	data32 = 0;
	/* bit4: hpd override, bit5: hpd reverse */
	data32 |= 1 << 4;
	if (rx.chip_id == CHIP_ID_GXTVBB)
		data32 |= 0 << 5;
	else
		data32 |= 1 << 5;
	/* pull down all the hpd */
	hdmirx_wr_top(TOP_HPD_PWR5V, data32);
	return err;
}

/*
 * DWC_init - DWC controller initialization
 */
static int DWC_init(void)
{
	int err = 0;
	unsigned long   data32;
	unsigned int evaltime = 0;

	evaltime = (modet_clk * 4095) / 158000;
	/* enable all */
	hdmirx_wr_dwc(DWC_HDMI_OVR_CTRL, ~0);
	/* recover to default value.*/
	/* remain code for some time.*/
	/* if no side effect then remove it */
	/*hdmirx_wr_bits_dwc(DWC_HDMI_SYNC_CTRL,*/
	/*	VS_POL_ADJ_MODE, VS_POL_ADJ_AUTO);*/
	/*hdmirx_wr_bits_dwc(DWC_HDMI_SYNC_CTRL,*/
	/*	HS_POL_ADJ_MODE, HS_POL_ADJ_AUTO);*/

	hdmirx_wr_bits_dwc(DWC_HDMI_CKM_EVLTM,
		EVAL_TIME, evaltime);
	hdmirx_control_clk_range(TMDS_CLK_MIN,
		TMDS_CLK_MAX);

	/* hdmirx_wr_bits_dwc(DWC_SNPS_PHYG3_CTRL,*/
		/*((1 << 2) - 1) << 2, port); */

	data32 = 0;
	data32 |= 0     << 20;
	data32 |= 1     << 19;
	data32 |= 5     << 16;  /* [18:16]  valid_mode */
	data32 |= 0     << 12;  /* [13:12]  ctrl_filt_sens */
	data32 |= 3     << 10;  /* [11:10]  vs_filt_sens */
	data32 |= 0     << 8;   /* [9:8]    hs_filt_sens */
	data32 |= 2     << 6;   /* [7:6]    de_measure_mode */
	data32 |= 0     << 5;   /* [5]      de_regen */
	data32 |= 3     << 3;   /* [4:3]    de_filter_sens */
	hdmirx_wr_dwc(DWC_HDMI_ERROR_PROTECT, data32);

	data32 = 0;
	data32 |= 0     << 8;   /* [10:8]   hact_pix_ith */
	data32 |= 0     << 5;   /* [5]      hact_pix_src */
	data32 |= 1     << 4;   /* [4]      htot_pix_src */
	hdmirx_wr_dwc(DWC_MD_HCTRL1, data32);

	data32 = 0;
	data32 |= 1     << 12;  /* [14:12]  hs_clk_ith */
	data32 |= 7     << 8;   /* [10:8]   htot32_clk_ith */
	data32 |= 1     << 5;   /* [5]      vs_act_time */
	data32 |= 3     << 3;   /* [4:3]    hs_act_time */
	data32 |= 0     << 0;   /* [1:0]    h_start_pos */
	hdmirx_wr_dwc(DWC_MD_HCTRL2, data32);

	data32 = 0;
	data32 |= 1	<< 4;   /* [4]      v_offs_lin_mode */
	data32 |= 1	<< 1;   /* [1]      v_edge */
	data32 |= 0	<< 0;   /* [0]      v_mode */
	hdmirx_wr_dwc(DWC_MD_VCTRL, data32);

	data32  = 0;
	data32 |= 1 << 10;  /* [11:10]  vofs_lin_ith */
	data32 |= 3 << 8;   /* [9:8]    vact_lin_ith */
	data32 |= 0 << 6;   /* [7:6]    vtot_lin_ith */
	data32 |= 7 << 3;   /* [5:3]    vs_clk_ith */
	data32 |= 2 << 0;   /* [2:0]    vtot_clk_ith */
	hdmirx_wr_dwc(DWC_MD_VTH, data32);

	data32  = 0;
	data32 |= 1 << 2;   /* [2]      fafielddet_en */
	data32 |= 0 << 0;   /* [1:0]    field_pol_mode */
	hdmirx_wr_dwc(DWC_MD_IL_POL, data32);

	data32  = 0;
	data32 |= 0	<< 1;
	data32 |= 1	<< 0;
	hdmirx_wr_dwc(DWC_HDMI_RESMPL_CTRL, data32);

	data32	= 0;
	data32 |= (hdmirx_rd_dwc(DWC_HDMI_MODE_RECOVER) & 0xf8000000);
	data32 |= (0	<< 24);
	data32 |= (0	<< 18);
	data32 |= (HYST_HDMI_TO_DVI	<< 13);
	data32 |= (HYST_DVI_TO_HDMI	<< 8);
	data32 |= (0	<< 6);
	data32 |= (0	<< 4);
	/* EESS_OESS */
	/* 0: new auto mode,check on HDMI mode or 1.1 features en */
	/* 1: force OESS */
	/* 2: force EESS */
	/* 3: auto mode,check CTL[3:0]=d9/d8 during WOO */
	data32 |= (hdcp_enc_mode	<< 2);
	data32 |= (0	<< 0);
	hdmirx_wr_dwc(DWC_HDMI_MODE_RECOVER, data32);

	data32 = hdmirx_rd_dwc(DWC_HDCP_CTRL);
	/* 0: Original behaviour */
	/* 1: Balance path delay between non-HDCP and HDCP */
	data32 |= 1 << 27; /* none & hdcp */
	/* 0: Original behaviour */
	/* 1: Balance path delay between HDCP14 and HDCP22. */
	data32 |= 1 << 26; /* 1.4 & 2.2 */
	hdmirx_wr_dwc(DWC_HDCP_CTRL, data32);

	return err;
}

void rx_hdcp14_set_normal_key(const struct hdmi_rx_hdcp *hdcp)
{
	unsigned int i = 0;
	unsigned int k = 0;
	int error = 0;

	for (i = 0; i < HDCP_KEYS_SIZE; i += 2) {
		for (k = 0; k < HDCP_KEY_WR_TRIES; k++) {
			if (hdmirx_rd_bits_dwc(DWC_HDCP_STS,
				HDCP_KEY_WR_OK_STS) != 0) {
				break;
			}
		}
		if (k < HDCP_KEY_WR_TRIES) {
			hdmirx_wr_dwc(DWC_HDCP_KEY1, hdcp->keys[i + 0]);
			hdmirx_wr_dwc(DWC_HDCP_KEY0, hdcp->keys[i + 1]);
		} else {
			error = -EAGAIN;
			break;
		}
	}
	hdmirx_wr_dwc(DWC_HDCP_BKSV1, hdcp->bksv[0]);
	hdmirx_wr_dwc(DWC_HDCP_BKSV0, hdcp->bksv[1]);
}
/*
 * hdmi_rx_ctrl_hdcp_config - config hdcp1.4 keys
 */
void rx_hdcp14_config(const struct hdmi_rx_hdcp *hdcp)
{
	unsigned int data32 = 0;

	/* I2C_SPIKE_SUPPR */
	data32 |= 1 << 16;
	/* FAST_I2C */
	data32 |= 0 << 12;
	/* ONE_DOT_ONE */
	data32 |= 0 << 9;
	/* FAST_REAUTH */
	data32 |= 0 << 8;
	/* DDC_ADDR */
	data32 |= 0x3a << 1;
	hdmirx_wr_dwc(DWC_HDCP_SETTINGS, data32);
	/* hdmirx_wr_bits_dwc(DWC_HDCP_SETTINGS, HDCP_FAST_MODE, 0); */
	/* Enable hdcp bcaps bit(bit7). In hdcp1.4 spec: Use of
	 * this bit is reserved, hdcp Receivers not capable of
	 * supporting HDMI must clear this bit to 0. For YAMAHA
	 * RX-V377 amplifier, enable this bit is needed, in case
	 * the amplifier won't do hdcp1.4 interaction occasionally.
	 */
	hdmirx_wr_bits_dwc(DWC_HDCP_SETTINGS, HDCP_BCAPS, 1);
	hdmirx_wr_bits_dwc(DWC_HDCP_CTRL, ENCRIPTION_ENABLE, 0);
	/* hdmirx_wr_bits_dwc(ctx, DWC_HDCP_CTRL, KEY_DECRYPT_ENABLE, 1); */
	hdmirx_wr_bits_dwc(DWC_HDCP_CTRL, KEY_DECRYPT_ENABLE, 0);
	hdmirx_wr_dwc(DWC_HDCP_SEED, hdcp->seed);
	if (hdcp14_key_mode == SECURE_MODE) {
		rx_set_hdcp14_secure_key();
		rx_pr("hdcp1.4 secure mode\n");
	} else {
		rx_hdcp14_set_normal_key(&rx.hdcp);
		rx_pr("hdcp1.4 normal mode\n");
	}
	if (rx.chip_id != CHIP_ID_TXHD) {
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL,
			REPEATER, hdcp->repeat ? 1 : 0);
		/* nothing attached downstream */
		hdmirx_wr_dwc(DWC_HDCP_RPT_BSTATUS, 0);
	}
	hdmirx_wr_bits_dwc(DWC_HDCP_CTRL, ENCRIPTION_ENABLE, 1);
}

#if 0
void rx_set_term_enable(bool enable)
{
	if (rx.chip_id >= CHIP_ID_TL1) {
		/* need to do : for tl1 */
	} else
		hdmirx_wr_bits_phy(PHY_MAIN_FSM_OVERRIDE1,
			PHY_TERM_OVERRIDE, enable);
}
#endif

void rx_set_term_value(unsigned char port, bool value)
{
	unsigned int data32;

	if (rx.chip_id >= CHIP_ID_TL1) {
		/* need to do : for tl1 */
		data32 = rd_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL0);
		if (port < E_PORT3) {
			if (value)
				data32 |= (1 << port);
			else
				data32 &= ~(1 << port);
			wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL0, data32);
		} else if (port == ALL_PORTS) {
			if (value)
				data32 |= 0x7;
			else
				data32 &= 0xfffffff8;
			wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL0, data32);
		} else
			rx_pr("%s port num overflow\n", __func__);
	} else {
		if (port < E_PORT_NUM) {
			if (value)
				hdmirx_wr_bits_phy(PHY_MAIN_FSM_OVERRIDE1,
					_BIT(port + 4), 1);
			else
				hdmirx_wr_bits_phy(PHY_MAIN_FSM_OVERRIDE1,
					_BIT(port + 4), 0);
		} else if (port == ALL_PORTS) {
			if (value)
				hdmirx_wr_bits_phy(PHY_MAIN_FSM_OVERRIDE1,
					PHY_TERM_OV_VALUE, 0xF);
			else
				hdmirx_wr_bits_phy(PHY_MAIN_FSM_OVERRIDE1,
					PHY_TERM_OV_VALUE, 0);
		} else
			rx_pr("%s port num overflow\n", __func__);
	}
}

int rx_set_port_hpd(uint8_t port_id, bool val)
{
	if (port_id < E_PORT_NUM) {
		if (val) {
			hdmirx_wr_bits_top(TOP_HPD_PWR5V, _BIT(port_id), 1);
			rx_set_term_value(port_id, 1);
		} else {
			hdmirx_wr_bits_top(TOP_HPD_PWR5V, _BIT(port_id), 0);
			rx_set_term_value(port_id, 0);
		}
	} else if (port_id == ALL_PORTS) {
		if (val) {
			hdmirx_wr_bits_top(TOP_HPD_PWR5V, MSK(4, 0), 0xF);
			rx_set_term_value(port_id, 1);
		} else {
			hdmirx_wr_bits_top(TOP_HPD_PWR5V, MSK(4, 0), 0x0);
			rx_set_term_value(port_id, 0);
		}
	} else
		return -1;

	if (log_level & LOG_EN)
		rx_pr("%s, port:%d, val:%d\n", __func__,
						port_id, val);
	return 0;
}

/* add param to differentiate repeater/main state machine/etc
 * 0: main loop; 2: workaround; 3: repeater flow; 4: special use
 */
void rx_set_cur_hpd(u8 val, u8 func)
{
	rx_pr("func-%d to", func);
	rx_set_port_hpd(rx.port, val);
}

/*
 * rx_force_hpd_config - force config hpd level on all ports
 * @hpd_level: hpd level
 */
void rx_force_hpd_cfg(uint8_t hpd_level)
{
	unsigned int hpd_value;

	if (hpd_level) {
		if (disable_port_en)
			hpd_value = (~(1 << disable_port_num)) & 0xF;
		else
			hpd_value = 0xF;

		hdmirx_wr_bits_top(TOP_HPD_PWR5V,
			MSK(4, 0), hpd_value);
	} else
		hdmirx_wr_bits_top(TOP_HPD_PWR5V, MSK(4, 0), 0x0);
}

/*
 * rx_force_rxsense_cfg - force config rxsense level on all ports
 * @level: rxsense level
 */
void rx_force_rxsense_cfg(uint8_t level)
{
	unsigned int term_ovr_value;
	unsigned int data32;

	if (rx.chip_id >= CHIP_ID_TL1) {
		/* enable terminal connect */
		data32 = rd_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL0);
		if (level) {
			if (disable_port_en)
				term_ovr_value =
					(~(1 << disable_port_num)) & 0x7;
			else
				term_ovr_value = 0x7;
			data32 |= term_ovr_value;
		} else {
			data32 &= 0xfffffff8;
		}
		wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL0, data32);
	} else {
		if (level) {
			if (disable_port_en)
				term_ovr_value =
					(~(1 << disable_port_num)) & 0xF;
			else
				term_ovr_value = 0xF;

			hdmirx_wr_bits_phy(PHY_MAIN_FSM_OVERRIDE1,
				PHY_TERM_OV_VALUE, term_ovr_value);
		} else
			hdmirx_wr_bits_phy(PHY_MAIN_FSM_OVERRIDE1,
				PHY_TERM_OV_VALUE, 0x0);
	}
}

/*
 * rx_force_hpd_rxsense_cfg - force config
 * hpd & rxsense level on all ports
 * @level: hpd & rxsense level
 */
void rx_force_hpd_rxsense_cfg(uint8_t level)
{
	rx_force_hpd_cfg(level);
	rx_force_rxsense_cfg(level);
	if (log_level & LOG_EN)
		rx_pr("hpd & rxsense force val:%d\n", level);
}

/*
 * control_reset - hdmirx controller reset
 */
void control_reset(void)
{
	unsigned long data32;

	/* disable functional modules */
	hdmirx_top_sw_reset();

	/* Enable functional modules */
	data32  = 0;
	data32 |= 1 << 5;   /* [5]      cec_enable */
	data32 |= 1 << 4;   /* [4]      aud_enable */
	data32 |= 1 << 3;   /* [3]      bus_enable */
	data32 |= 1 << 2;   /* [2]      hdmi_enable */
	data32 |= 1 << 1;   /* [1]      modet_enable */
	data32 |= 1 << 0;   /* [0]      cfg_enable */
	hdmirx_wr_dwc(DWC_DMI_DISABLE_IF, data32);
	mdelay(1);
	hdmirx_wr_dwc(DWC_DMI_SW_RST,	0x0000001F);
}

void rx_esm_tmdsclk_en(bool en)
{
	hdmirx_wr_bits_top(TOP_CLK_CNTL, HDCP22_TMDSCLK_EN, en);
	if (hdcp22_on && hdcp_hpd_ctrl_en)
		hdmirx_hdcp22_hpd(en);
	if (log_level & HDCP_LOG)
		rx_pr("%s:%d\n", __func__, en);
}

/*
 * hdcp22_clk_en - clock gating for hdcp2.2
 * @en: enable or disable clock
 */
void hdcp22_clk_en(bool en)
{
	if (en) {
		wr_reg_hhi(HHI_HDCP22_CLK_CNTL,
			(rd_reg_hhi(HHI_HDCP22_CLK_CNTL) & 0xffff0000) |
			 /* [10: 9] fclk_div7=2000/7=285.71 MHz */
			((0 << 9)   |
			 /* [    8] clk_en. Enable gated clock */
			 (1 << 8)   |
			 /* [ 6: 0] Divide by 1. = 285.71/1 = 285.71 MHz */
			 (0 << 0)));

			wr_reg_hhi(HHI_HDCP22_CLK_CNTL,
			(rd_reg_hhi(HHI_HDCP22_CLK_CNTL) & 0x0000ffff) |
			/* [26:25] select cts_oscin_clk=24 MHz */
			((0 << 25)  |
			 (1 << 24)  |   /* [   24] Enable gated clock */
			 (0 << 16)));
		if (rx.chip_id >= CHIP_ID_TL1)
			/* TL1:esm related clk bit9-11 */
			hdmirx_wr_bits_top(TOP_CLK_CNTL, MSK(3, 9), 0x7);
		else
			/* TL1:esm related clk bit3-5 */
			hdmirx_wr_bits_top(TOP_CLK_CNTL, MSK(3, 3), 0x7);

		if (rx.chip_id >= CHIP_ID_TM2)
			/* Enable axi_clk,for tm2 */
			/* AXI arbiter is moved outside of hdmitx. */
			/* There is an AXI arbiter in the chip’s EE domain */
			/* for arbitrating AXI requests from HDMI TX and RX.*/
			hdmirx_wr_bits_top(TOP_CLK_CNTL, MSK(1, 12), 0x1);
	} else {
		hdmirx_wr_bits_top(TOP_CLK_CNTL, MSK(3, 3), 0x0);
		wr_reg_hhi(HHI_HDCP22_CLK_CNTL, 0);
	}
}

/*
 * hdmirx_hdcp22_esm_rst - software reset esm
 */
void hdmirx_hdcp22_esm_rst(void)
{
	/* For TL1,the sw_reset_hdcp22 bit is top reg 0x0,bit'12 */
	if (rx.chip_id >= CHIP_ID_TL1)
		hdmirx_wr_top(TOP_SW_RESET, 0x1000);
	else
		/* For txlx and previous chips,the sw_reset_hdcp22 is bit'8 */
		hdmirx_wr_top(TOP_SW_RESET, 0x100);
	mdelay(1);
	hdmirx_wr_top(TOP_SW_RESET, 0x0);
	rx_pr("esm rst\n");
}

/*
 * hdmirx_hdcp22_init - hdcp2.2 initialization
 */
int rx_is_hdcp22_support(void)
{
	int ret = 0;
	int temp;

	temp = rx_sec_set_duk(hdmirx_repeat_support());
	if (temp) {
		rx_hdcp22_wr_top(TOP_SKP_CNTL_STAT, 7);
		ret = 1;
		if (temp == 2)
			rx_pr("2.2 test key!!!\n");
	} else
		ret = 0;
	rx_pr("hdcp22 == %d\n", ret);

	return ret;
}

/*
 * hdmirx_hdcp22_hpd - set hpd level for hdcp2.2
 * @value: whether to set hpd high
 */
void hdmirx_hdcp22_hpd(bool value)
{
	unsigned long data32 = hdmirx_rd_dwc(DWC_HDCP22_CONTROL);

	if (value)
		data32 |= 0x1000;
	else
		data32 &= (~0x1000);
	hdmirx_wr_dwc(DWC_HDCP22_CONTROL, data32);
}

/*
 * hdcp_22_off
 */
void hdcp_22_off(void)
{
	/* note: can't pull down hpd before enter suspend */
	/* it will stop cec wake up func if EE domain still working */
	/* rx_set_cur_hpd(0); */
	hpd_to_esm = 0;
	hdmirx_wr_dwc(DWC_HDCP22_CONTROL,
				0x0);
	if (hdcp22_kill_esm == 0)
		hdmirx_hdcp22_esm_rst();
	else
		hdcp22_kill_esm = 0;
	hdcp22_clk_en(0);
	rx_pr("hdcp22 off\n");
}

/*
 * hdcp_22_on
 */
void hdcp_22_on(void)
{
	hdcp22_kill_esm = 0;
	/* switch_set_state(&rx.hpd_sdev, 0x0); */
	extcon_set_state_sync(rx.rx_excton_rx22, EXTCON_DISP_HDMI, 0);
	hdcp22_clk_en(1);
	hdmirx_wr_dwc(DWC_HDCP22_CONTROL,
				0x1000);
	/* rx_hdcp22_wr_top(TOP_SKP_CNTL_STAT, 0x1); */
	/* hdmirx_hw_config(); */
	/* switch_set_state(&rx.hpd_sdev, 0x1); */
	extcon_set_state_sync(rx.rx_excton_rx22, EXTCON_DISP_HDMI, 1);
	hpd_to_esm = 1;
	/* dont need to delay 900ms to wait sysctl start hdcp_rx22,*/
	/*sysctl is userspace it wakes up later than driver */
	/* mdelay(900); */
	/* rx_set_cur_hpd(1); */
	rx_pr("hdcp22 on\n");
}

/*
 * clk_init - clock initialization
 * config clock for hdmirx module
 */
void clk_init(void)
{
	unsigned int data32;

	/* DWC clock enable */
	/* Turn on clk_hdmirx_pclk, also = sysclk */
	wr_reg_hhi(HHI_GCLK_MPEG0,
		rd_reg_hhi(HHI_GCLK_MPEG0) | (1 << 21));

	/* Enable APB3 fail on error */
	/* APB3 to HDMIRX-TOP err_en */
	/* default 0x3ff, | bit15 = 1 | bit12 = 1 */

	hdmirx_wr_ctl_port(0, 0x93ff);
	hdmirx_wr_ctl_port(0x10, 0x93ff);

	/* turn on clocks: md, cfg... */
	/* G9 clk tree */
	/* fclk_div5 400M ----- mux sel = 3 */
	/* fclk_div3 850M ----- mux sel = 2 */
	/* fclk_div4 637M ----- mux sel = 1 */
	/* XTAL		24M  ----- mux sel = 0 */
	/* [26:25] HDMIRX mode detection clock mux select: osc_clk */
	/* [24]    HDMIRX mode detection clock enable */
	/* [22:16] HDMIRX mode detection clock divider */
	/* [10: 9] HDMIRX config clock mux select: */
	/* [    8] HDMIRX config clock enable */
	/* [ 6: 0] HDMIRX config clock divider: */
	#if 0
	data32  = 0;
	data32 |= 0 << 25;
	data32 |= 1 << 24;
	data32 |= 0 << 16;
	data32 |= 3 << 9;
	data32 |= 1 << 8;
	data32 |= 2 << 0;
	wr_reg_hhi(HHI_HDMIRX_CLK_CNTL, data32);

	data32 = 0;
	data32 |= 2	<< 25;
	data32 |= acr_mode << 24;
	data32 |= 0	<< 16;
	data32 |= 2	<< 9;
	data32 |= 1	<< 8;
	data32 |= 2	<< 0;
	wr_reg_hhi(HHI_HDMIRX_AUD_CLK_CNTL, data32);
	#endif
	if ((rx.chip_id == CHIP_ID_TXLX) ||
		(rx.chip_id == CHIP_ID_TXHD) ||
		(rx.chip_id >= CHIP_ID_TL1)) {
		/* [15] hdmirx_aud_pll4x_en override enable */
		/* [14] hdmirx_aud_pll4x_en override value */
		/* [6:5] clk_sel for cts_hdmirx_aud_pll_clk: */
		/* 0=hdmirx_aud_pll_clk */
		/* [4] clk_en for cts_hdmirx_aud_pll_clk */
		/* [2:0] clk_div for cts_hdmirx_aud_pll_clk */
		data32  = 0;
		data32 |= (0 << 15);
		data32 |= (1 << 14);
		data32 |= (0 << 5);
		data32 |= (0 << 4);
		data32 |= (0 << 0);
		wr_reg_hhi(HHI_AUDPLL_CLK_OUT_CNTL, data32);
		data32 |= (1 << 4);
		wr_reg_hhi(HHI_AUDPLL_CLK_OUT_CNTL, data32);
	}

	data32 = hdmirx_rd_top(TOP_CLK_CNTL);
	data32 |= 0 << 31;  /* [31]     disable clkgating */
	data32 |= 1 << 17;  /* [17]     audfifo_rd_en */
	data32 |= 1 << 16;  /* [16]     pktfifo_rd_en */
	if (rx.chip_id >= CHIP_ID_TL1) {
		data32 |= 0 << 8;   /* [8]      tmds_ch2_clk_inv */
		data32 |= 0 << 7;  /* [7]     tmds_ch1_clk_inv */
		data32 |= 0 << 6;  /* [6]     tmds_ch0_clk_inv */
		data32 |= 0 << 5;   /* [5]      pll4x_cfg */
		/* force_pll4x:
		 * 1=Force pll4x_en value to be pll4x_cfg.
		 * 0=Use auto detect.
		 */
		data32 |= 0 << 4;   /* [4]      force_pll4x */
		data32 |= 0 << 3;   /* [3]      phy_clk_inv: 1-invert */
	} else {
		data32 |= 1 << 2;   /* [2]      hdmirx_cecclk_en */
		data32 |= 0 << 1;   /* [1]      bus_clk_inv */
		data32 |= 0 << 0;   /* [0]      hdmi_clk_inv */
	}
	hdmirx_wr_top(TOP_CLK_CNTL, data32);    /* DEFAULT: {32'h0} */
}

/*
 * hdmirx_20_init - hdmi2.0 config
 */
void hdmirx_20_init(void)
{
	unsigned long data32;
	unsigned long scdc_en =
		scdc_force_en ? 1 : rx.edid_ver;

	data32 = 0;
	data32 |= 1	<< 12; /* [12]     vid_data_checken */
	data32 |= 1	<< 11; /* [11]     data_island_checken */
	data32 |= 1	<< 10; /* [10]     gb_checken */
	data32 |= 1	<< 9;  /* [9]      preamb_checken */
	data32 |= 1	<< 8;  /* [8]      ctrl_checken */
	data32 |= scdc_en	<< 4;  /* [4]      scdc_enable */
	/* To support some TX that sends out SSCP even when not scrambling:
	 * 0: Original behaviour
	 * 1: During TMDS character error detection, treat SSCP character
	 *	as normal TMDS character.
	 * Note: If scramble is turned on, this bit will not take effect,
	 *	revert to original IP behaviour.
	 */
	data32 |= ignore_sscp_charerr << 3; /* [3]ignore sscp character err */
	/* To support some TX that sends out SSCP even when not scrambling:
	 * 0: Original behaviour
	 * 1: During TMDS decoding, treat SSCP character
	 * as normal TMDS character
	 * Note: If scramble is turned on, this bit will not take effect,
	 * revert to original IP behaviour.
	 */
	data32 |= ignore_sscp_tmds << 2;  /* [2]	   ignore sscp tmds */
	data32 |= SCRAMBLE_SEL	<< 0;  /* [1:0]    scramble_sel */
	hdmirx_wr_dwc(DWC_HDMI20_CONTROL,    data32);

	data32  = 0;
	data32 |= 1	<< 24; /* [25:24]  i2c_spike_suppr */
	data32 |= 0	<< 20; /* [20]     i2c_timeout_en */
	data32 |= 0	<< 0;  /* [19:0]   i2c_timeout_cnt */
	hdmirx_wr_dwc(DWC_SCDC_I2CCONFIG,    data32);

	data32  = 0;
	data32 |= 1    << 1;  /* [1]      hpd_low */
	data32 |= 0    << 0;  /* [0]      power_provided */
	hdmirx_wr_dwc(DWC_SCDC_CONFIG,   data32);

	data32  = 0;
	data32 |= 0xabcdef << 8;  /* [31:8]   manufacture_oui */
	data32 |= 1	<< 0;  /* [7:0]    sink_version */
	hdmirx_wr_dwc(DWC_SCDC_WRDATA0,	data32);

	data32  = 0;
	data32 |= 10	<< 20; /* [29:20]  chlock_max_err */
	data32 |= 24000	<< 0;  /* [15:0]   milisec_timer_limit */
	hdmirx_wr_dwc(DWC_CHLOCK_CONFIG, data32);

	/* hdcp2.2 ctl */
	if (hdcp22_on) {
		/* set hdcp_hpd high later */
		if (hdcp_hpd_ctrl_en)
			hdmirx_wr_dwc(DWC_HDCP22_CONTROL, 0);
		else
			hdmirx_wr_dwc(DWC_HDCP22_CONTROL, 0x1000);
	} else {
		hdmirx_wr_dwc(DWC_HDCP22_CONTROL, 2);
	}
}



/*
 * hdmirx_audio_init - audio initialization
 */
int hdmirx_audio_init(void)
{
	/* 0=I2S 2-channel; 1=I2S 4 x 2-channel. */
	int err = 0;
	unsigned long data32 = 0;

	data32 |= 7	<< 13;
	data32 |= 0	<< 12;
	data32 |= 1	<< 11;
	data32 |= 0	<< 10;

	data32 |= 0	<< 9;
	data32 |= 1	<< 8;
	data32 |= 1	<< 6;
	data32 |= 3	<< 4;
	data32 |= 0	<< 3;
	data32 |= acr_mode  << 2;
	data32 |= acr_mode  << 1;
	data32 |= acr_mode  << 0;
	hdmirx_wr_top(TOP_ACR_CNTL_STAT, data32);

	if (rx.chip_id >= CHIP_ID_TL1) {
		data32 = 0;
		data32 |= 0	<< 2;/*meas_mode*/
		data32 |= 1	<< 1;/*enable*/
		data32 |= 1	<< 0;/*reset*/
		if (acr_mode)
			data32 |= 2 << 16;/*aud pll*/
		else
			data32 |= 500 << 16;/*acr*/
		hdmirx_wr_top(TOP_AUDMEAS_CTRL, data32);
		hdmirx_wr_top(TOP_AUDMEAS_CYCLES_M1, 65535);
		/*start messure*/
		hdmirx_wr_top(TOP_AUDMEAS_CTRL, data32 & (~0x1));
	}

	/*
	 *recover to default value, bit[27:24]
	 *set aud_pll_lock filter
	 *data32  = 0;
	 *data32 |= 0 << 28;
	 *data32 |= 0 << 24;
	 *hdmirx_wr_dwc(DWC_AUD_PLL_CTRL, data32);
	 */

	/* AFIFO depth 1536word.*/
	/*increase start threshold to middle position */
	data32  = 0;
	data32 |= 160 << 18; /* start */
	data32 |= 200	<< 9; /* max */
	data32 |= 8	<< 0; /* min */
	hdmirx_wr_dwc(DWC_AUD_FIFO_TH, data32);

	/* recover to default value.*/
	/*remain code for some time.*/
	/*if no side effect then remove it */
	/*
	 *data32  = 0;
	 *data32 |= 1	<< 16;
	 *data32 |= 0	<< 0;
	 *hdmirx_wr_dwc(DWC_AUD_FIFO_CTRL, data32);
	 */

	data32  = 0;
	data32 |= 0	<< 8;
	data32 |= 1	<< 7;
	data32 |= aud_ch_map << 2;
	data32 |= 1	<< 0;
	hdmirx_wr_dwc(DWC_AUD_CHEXTR_CTRL, data32);

	data32 = 0;
	/* [22:21]	aport_shdw_ctrl */
	data32 |= 3	<< 21;
	/* [20:19]  auto_aclk_mute */
	data32 |= auto_aclk_mute	<< 19;
	/* [16:10]  aud_mute_speed */
	data32 |= 1	<< 10;
	/* [7]      aud_avmute_en */
	data32 |= aud_avmute_en	<< 7;
	/* [6:5]    aud_mute_sel */
	data32 |= aud_mute_sel	<< 5;
	/* [4:3]    aud_mute_mode */
	data32 |= 1	<< 3;
	/* [2:1]    aud_ttone_fs_sel */
	data32 |= 0	<< 1;
	/* [0]      testtone_en */
	data32 |= 0	<< 0;
	hdmirx_wr_dwc(DWC_AUD_MUTE_CTRL, data32);

	/* recover to default value.*/
	/*remain code for some time.*/
	/*if no side effect then remove it */
	/*
	 *data32 = 0;
	 *data32 |= 0	<< 16;
	 *data32 |= 0	<< 12;
	 *data32 |= 0	<< 4;
	 *data32 |= 0	<< 1;
	 *data32 |= 0	<< 0;
	 *hdmirx_wr_dwc(DWC_AUD_PAO_CTRL,   data32);
	 */

	/* recover to default value.*/
	/*remain code for some time.*/
	/*if no side effect then remove it */
	/*
	 *data32  = 0;
	 *data32 |= 0	<< 8;
	 *hdmirx_wr_dwc(DWC_PDEC_AIF_CTRL,  data32);
	 */

	data32  = 0;
	/* [4:2]    deltacts_irqtrig */
	data32 |= 0 << 2;
	/* [1:0]    cts_n_meas_mode */
	data32 |= 0 << 0;
	/* DEFAULT: {27'd0, 3'd0, 2'd1} */
	hdmirx_wr_dwc(DWC_PDEC_ACRM_CTRL, data32);

	/* unsupport HBR serial mode. invalid bit */
	hdmirx_wr_bits_dwc(DWC_AUD_CTRL, DWC_AUD_HBR_ENABLE, 1);

	/* SAO cfg, disable I2S output, no use */
	data32 = 0;
	data32 |= 1	<< 10;
	data32 |= 0	<< 9;
	data32 |= 0x0f	<< 5;
	data32 |= 0	<< 1;
	data32 |= 1	<< 0;
	hdmirx_wr_dwc(DWC_AUD_SAO_CTRL, data32);

	data32  = 0;
	data32 |= 1	<< 6;
	data32 |= 0xf	<< 2;
	hdmirx_wr_dwc(DWC_PDEC_ASP_CTRL, data32);

	return err;
}

/*
 * snps phy g3 initial
 */
void snps_phyg3_init(void)
{
	unsigned int data32;
	unsigned int term_value =
		hdmirx_rd_top(TOP_HPD_PWR5V);

	data32 = 0;
	data32 |= 1 << 6;
	data32 |= 1 << 4;
	data32 |= rx.port << 2;
	data32 |= 1 << 1;
	data32 |= 1 << 0;
	hdmirx_wr_dwc(DWC_SNPS_PHYG3_CTRL, data32);
	mdelay(1);

	data32	= 0;
	data32 |= 1 << 6;
	data32 |= 1 << 4;
	data32 |= rx.port << 2;
	data32 |= 1 << 1;
	data32 |= 0 << 0;
	hdmirx_wr_dwc(DWC_SNPS_PHYG3_CTRL, data32);

	data32 = 0;
	data32 |= 6 << 10;
	data32 |= 1 << 9;
	data32 |= ((24000 * 4) / 1000);
	hdmirx_wr_phy(PHY_CMU_CONFIG, data32);

	hdmirx_wr_phy(PHY_VOLTAGE_LEVEL, 0x1ea);

	data32 = 0;
	data32 |= 0	<< 15;
	data32 |= 0	<< 13;
	data32 |= 0	<< 12;
	data32 |= phy_fast_switching << 11;
	data32 |= 0	<< 10;
	data32 |= phy_fsm_enhancement << 9;
	data32 |= 0	<< 8;
	data32 |= 0	<< 7;
	data32 |= 0 << 5;
	data32 |= 0	<< 3;
	data32 |= 0 << 2;
	data32 |= 0 << 0;
	hdmirx_wr_phy(PHY_SYSTEM_CONFIG, data32);

	hdmirx_wr_phy(MPLL_PARAMETERS2,	0x1c94);
	hdmirx_wr_phy(MPLL_PARAMETERS3,	0x3713);
	/*default 0x24da , EQ optimizing for kaiboer box */
	hdmirx_wr_phy(MPLL_PARAMETERS4,	0x24dc);
	hdmirx_wr_phy(MPLL_PARAMETERS5,	0x5492);
	hdmirx_wr_phy(MPLL_PARAMETERS6,	0x4b0d);
	hdmirx_wr_phy(MPLL_PARAMETERS7,	0x4760);
	hdmirx_wr_phy(MPLL_PARAMETERS8,	0x008c);
	hdmirx_wr_phy(MPLL_PARAMETERS9,	0x0010);
	hdmirx_wr_phy(MPLL_PARAMETERS10, 0x2d20);
	hdmirx_wr_phy(MPLL_PARAMETERS11, 0x2e31);
	hdmirx_wr_phy(MPLL_PARAMETERS12, 0x4b64);
	hdmirx_wr_phy(MPLL_PARAMETERS13, 0x2493);
	hdmirx_wr_phy(MPLL_PARAMETERS14, 0x676d);
	hdmirx_wr_phy(MPLL_PARAMETERS15, 0x23e0);
	hdmirx_wr_phy(MPLL_PARAMETERS16, 0x001b);
	hdmirx_wr_phy(MPLL_PARAMETERS17, 0x2218);
	hdmirx_wr_phy(MPLL_PARAMETERS18, 0x1b25);
	hdmirx_wr_phy(MPLL_PARAMETERS19, 0x2492);
	hdmirx_wr_phy(MPLL_PARAMETERS20, 0x48ea);
	hdmirx_wr_phy(MPLL_PARAMETERS21, 0x0011);
	hdmirx_wr_phy(MPLL_PARAMETERS22, 0x04d2);
	hdmirx_wr_phy(MPLL_PARAMETERS23, 0x0414);

	/* Configuring I2C to work in fastmode */
	hdmirx_wr_dwc(DWC_I2CM_PHYG3_MODE,	 0x1);
	/* disable overload protect for Philips DVD */
	/* NOTE!!!!! don't remove below setting */
	hdmirx_wr_phy(OVL_PROT_CTRL, 0xa);

	/* clear clkrate cfg */
	hdmirx_wr_bits_phy(PHY_CDR_CTRL_CNT, CLK_RATE_BIT, 0);
	/*last_clk_rate = 0;*/
	rx.phy.clk_rate = 0;
	/* enable all ports's termination */
	data32 = 0;
	data32 |= 1 << 8;
	data32 |= ((term_value & 0xF) << 4);
	hdmirx_wr_phy(PHY_MAIN_FSM_OVERRIDE1, data32);

	data32 = 0;
	data32 |= 1 << 6;
	data32 |= 1 << 4;
	data32 |= rx.port << 2;
	data32 |= 0 << 1;
	data32 |= 0 << 0;
	hdmirx_wr_dwc(DWC_SNPS_PHYG3_CTRL, data32);

}

void rx_run_eq(void)
{
	if (rx.chip_id < CHIP_ID_TL1)
		rx_eq_algorithm();
	else
		hdmirx_phy_init();
}

bool rx_eq_done(void)
{
	bool ret = true;

	if (rx.chip_id < CHIP_ID_TL1) {
		if (rx_get_eq_run_state() == E_EQ_START)
			ret = false;
	}
	return ret;
}
/*
 * hdmirx_phy_init - hdmirx phy initialization
 */
void hdmirx_phy_init(void)
{
	/* uint32_t data32; */
	/* uint32_t cur_cable_clk; */
	if (rx.chip_id >= CHIP_ID_TL1)
		aml_phy_bw_switch();
	else
		snps_phyg3_init();
}

/*
 * rx_clkrate_monitor - clock ratio monitor
 * detect SCDC tmds clk ratio changes and
 * update phy setting
 */
bool rx_clkrate_monitor(void)
{
	uint32_t clk_rate, phy_band, pll_band;
	bool changed = false;
	int i;
	int error = 0;
	clk_rate = rx_get_scdc_clkrate_sts();

	if (rx.state < FSM_WAIT_CLK_STABLE)
		return changed;
	/*rx.phy.cable_clk = rx_measure_clock(MEASURE_CLK_CABLE);*/
	if (is_clk_stable()) {
		rx.phy.cable_clk = rx_measure_clock(MEASURE_CLK_CABLE);
		rx.phy.tmds_clk = rx_measure_clock(MEASURE_CLK_TMDS);
		pll_band = aml_phy_pll_band(rx.phy.cable_clk, clk_rate);
		phy_band = aml_cable_clk_band(rx.phy.cable_clk, clk_rate);
		if ((rx.phy.pll_bw != pll_band) ||
			(rx.phy.phy_bw != phy_band)) {
			rx.phy.cablesel = 0;
			rx.phy.phy_bw = phy_band;
			rx.phy.pll_bw = pll_band;
		}
	}

	if (clk_rate != rx.phy.clk_rate) {
		changed = true;
		if (rx.chip_id < CHIP_ID_TL1) {
			for (i = 0; i < 3; i++) {
				error = hdmirx_wr_bits_phy(PHY_CDR_CTRL_CNT,
					CLK_RATE_BIT, clk_rate);

				if (error == 0)
					break;
			}
		} else {
			aml_phy_bw_switch();
		}
		if (log_level & VIDEO_LOG)
			rx_pr("clk_rate:%d, last_clk_rate: %d\n",
			clk_rate, rx.phy.clk_rate);
		rx.phy.clk_rate = clk_rate;
	}
	if (changed) {
		if (rx.state >= FSM_WAIT_CLK_STABLE)
			rx.state = FSM_WAIT_CLK_STABLE;
	}
	return changed;
}


/*
 * rx_hdcp_init - hdcp1.4 init and enable
 */
void rx_hdcp_init(void)
{
	if (hdcp_enable)
		rx_hdcp14_config(&rx.hdcp);
	else
		hdmirx_wr_bits_dwc(DWC_HDCP_CTRL, ENCRIPTION_ENABLE, 0);
}

/*type 1 pull down hpd,reset hdcp2.2
 *type 2 only pull down hpd
 */
void hdmirx_load_firm_reset(int type)
{
	int ret = 0;

	rx_pr("%s\n", __func__);
	rx_pr("3firm_change:%d,repeat_plug:%d,repeat:%d\n",
				rx.firm_change, repeat_plug, rx.hdcp.repeat);
	/*wait the fsm end*/
	rx.firm_change = 1;
	msleep(20);
	/*External_Mute(1);rx_aud_pll_ctl(0);*/
	rx_set_cur_hpd(0, 4);
	/*type 2 only pull down hpd*/
	if (type == 2) {
		downstream_hpd_flag = 0;
		fsm_restart();
		return;
	}
	if (!repeat_plug)
		downstream_hpd_flag = 1;
	else
		downstream_hpd_flag = 0;
	ret = rx_sec_set_duk(hdmirx_repeat_support());
	rx_pr("ret = %d\n", ret);
	if (ret) {
		hdmirx_wr_dwc(DWC_HDCP22_CONTROL, 0x0);
		hdmirx_hdcp22_esm_rst();
		mdelay(100);
		hdmirx_wr_dwc(DWC_HDCP22_CONTROL,
					0x1000);
		rx_hdcp22_wr_top(TOP_SKP_CNTL_STAT, 0x1);
		fsm_restart();
		rx_is_hdcp22_support();
	}
	rx_pr("4firm_change:%d,repeat_plug:%d,repeat:%d\n",
			rx.firm_change, repeat_plug, rx.hdcp.repeat);
}

/* need reset bandgap when
 * aud_clk=0 & req_clk!=0
 * according to analog team's request
 */
void rx_audio_bandgap_rst(void)
{
	if (rx.chip_id >= CHIP_ID_TL1)
		return;

	wr_reg_hhi_bits(HHI_VDAC_CNTL0_TXLX, _BIT(13), 1);
	udelay(10);
	wr_reg_hhi_bits(HHI_VDAC_CNTL0_TXLX, _BIT(13), 0);
	if (log_level & AUDIO_LOG)
		rx_pr("%s\n", __func__);
}

void rx_audio_bandgap_en(void)
{
	if (rx.chip_id >= CHIP_ID_TL1)
		return;

	wr_reg_hhi_bits(HHI_VDAC_CNTL0_TXLX, _BIT(9), 1);
	wr_reg_hhi_bits(HHI_VDAC_CNTL0_TXLX, _BIT(13), 1);
	udelay(10);
	wr_reg_hhi_bits(HHI_VDAC_CNTL0_TXLX, _BIT(13), 0);

	if (log_level & AUDIO_LOG)
		rx_pr("%s\n", __func__);
}

void rx_sw_reset(int level)
{
	unsigned long data32 = 0;

	if (level == 1) {
		data32 |= 0 << 7;	/* [7]vid_enable */
		data32 |= 0 << 5;	/* [5]cec_enable */
		data32 |= 0 << 4;	/* [4]aud_enable */
		data32 |= 0 << 3;	/* [3]bus_enable */
		data32 |= 0 << 2;	/* [2]hdmi_enable */
		data32 |= 1 << 1;	/* [1]modet_enable */
		data32 |= 0 << 0;	/* [0]cfg_enable */

	} else if (level == 2) {
		data32 |= 0 << 7;	/* [7]vid_enable */
		data32 |= 0 << 5;	/* [5]cec_enable */
		data32 |= 1 << 4;	/* [4]aud_enable */
		data32 |= 0 << 3;	/* [3]bus_enable */
		data32 |= 1 << 2;	/* [2]hdmi_enable */
		data32 |= 1 << 1;	/* [1]modet_enable */
		data32 |= 0 << 0;	/* [0]cfg_enable */
	}
	hdmirx_wr_dwc(DWC_DMI_SW_RST, data32);
}

void hdmirx_hw_config(void)
{
	rx_pr("%s port:%d\n", __func__, rx.port);
	control_reset();
	/* hdmi_rx_top_edid_update(); */
	rx_hdcp_init();
	hdmirx_audio_init();
	packet_init();
	if (rx.chip_id != CHIP_ID_TXHD)
		hdmirx_20_init();
	DWC_init();
	hdmirx_irq_hdcp_enable(true);
	if (rx.chip_id >= CHIP_ID_TL1)
		aml_phy_switch_port();
	hdmirx_phy_init();
	hdmirx_top_irq_en(true);
	rx_pr("%s  %d Done!\n", __func__, rx.port);
	/* hdmi reset will cause cec not working*/
	/* cec modult need reset */
	if (rx.chip_id <= CHIP_ID_TXL)
		cec_hw_reset(1);/*1:snps cec*/
}

/*
 * hdmirx_hw_probe - hdmirx top/controller/phy init
 */
void hdmirx_hw_probe(void)
{
	hdmirx_wr_top(TOP_MEM_PD, 0);
	hdmirx_top_irq_en(false);
	hdmirx_wr_top(TOP_SW_RESET, 0);
	clk_init();
	TOP_init();
	control_reset();
	DWC_init();
	rx_emp_to_ddr_init();
	hdmi_rx_top_edid_update();
	/*hdmirx_irq_enable(FALSE);*/
	/*hdmirx_irq_hdcp22_enable(FALSE);*/
	hdcp22_clk_en(1);
	hdmirx_audio_init();
	packet_init();
	if (rx.chip_id != CHIP_ID_TXHD)
		hdmirx_20_init();
	if (rx.chip_id >= CHIP_ID_TL1)
		aml_phy_switch_port();
	hdmirx_phy_init();
	hdmirx_wr_top(TOP_PORT_SEL, 0x10);
	hdmirx_wr_top(TOP_INTR_STAT_CLR, ~0);
	hdmirx_top_irq_en(true);
	/* rx_pr("%s Done!\n", __func__); */
}

/*
 * rx_audio_pll_sw_update
 * Sent an update pulse to audio pll module.
 * Indicate the ACR info is changed.
 */
void rx_audio_pll_sw_update(void)
{
	hdmirx_wr_bits_top(TOP_ACR_CNTL_STAT, _BIT(11), 1);
}
/*
 * func: rx_acr_info_update
 * refresh aud_pll by manual N/CTS changing
 */
void rx_acr_info_sw_update(void)
{
	hdmirx_wr_dwc(DWC_AUD_CLK_CTRL, 0x10);
	udelay(100);
	hdmirx_wr_dwc(DWC_AUD_CLK_CTRL, 0x0);
}

/*
 * is_afifo_error - audio fifo unnormal detection
 * check if afifo block or not
 * bit4: indicate FIFO is overflow
 * bit3: indicate FIFO is underflow
 * bit2: start threshold pass
 * bit1: wr point above max threshold
 * bit0: wr point below mix threshold
 *
 * return true if afifo under/over flow, false otherwise.
 */
bool is_afifo_error(void)
{
	bool ret = false;

	if ((hdmirx_rd_dwc(DWC_AUD_FIFO_STS) &
		(OVERFL_STS | UNDERFL_STS)) != 0) {
		ret = true;
		if (log_level & AUDIO_LOG)
			rx_pr("afifo err\n");
	}
	return ret;
}

/*
 * is_aud_pll_error - audio clock range detection
 * noraml mode: aud_pll = aud_sample_rate * 128
 * HBR: aud_pll = aud_sample_rate * 128 * 4
 *
 * return true if audio clock is in range, false otherwise.
 */
bool is_aud_pll_error(void)
{
	bool ret = true;
	u32 clk = rx.aud_info.aud_clk;
	int32_t aud_128fs = rx.aud_info.real_sr * 128;
	int32_t aud_512fs = rx.aud_info.real_sr * 512;

	if (rx.aud_info.real_sr == 0)
		return false;
	if	((abs(clk - aud_128fs) < AUD_PLL_THRESHOLD) ||
		(abs(clk - aud_512fs) < AUD_PLL_THRESHOLD)) {
		ret = false;
	}
	if ((ret) && (log_level & AUDIO_LOG))
		rx_pr("clk:%d,128fs:%d,512fs:%d,\n", clk, aud_128fs, aud_512fs);
	return ret;
}

/*
 * rx_aud_pll_ctl - audio pll config
 */
void rx_aud_pll_ctl(bool en)
{
	int tmp = 0;
	/*unsigned int od, od2;*/

	if (rx.chip_id >= CHIP_ID_TL1) {
		if (en) {
			/* AUD_CLK=N/CTS*TMDS_CLK */
			wr_reg_hhi(HHI_AUD_PLL_CNTL, 0x40001540);
			#if 1
			/* use mpll */
			tmp = 0;
			tmp |= 2 << 2; /* 0:tmds_clk 1:ref_clk 2:mpll_clk */
			wr_reg_hhi(HHI_AUD_PLL_CNTL2, tmp);
			/* cntl3 2:0 000=1*cts 001=2*cts 010=4*cts 011=8*cts */
			wr_reg_hhi(HHI_AUD_PLL_CNTL3, rx.phy.aud_div);
			#else
			/* use tmds clk */
			tmp = 0;
			tmp |= 0 << 2; /* 0:tmds_clk 1:ref_clk 2:mpll_clk */
			wr_reg_hhi(HHI_AUD_PLL_CNTL2, tmp);
			/* cntl3 2:0 000=1*cts 001=2*cts 010=4*cts 011=8*cts */
			wr_reg_hhi(HHI_AUD_PLL_CNTL3, 0);
			#endif
			rx_pr("aud div=%d\n", rd_reg_hhi(HHI_AUD_PLL_CNTL3));
			wr_reg_hhi(HHI_AUD_PLL_CNTL, 0x60001540);
			rx_pr("audio pll lock:0x%x\n",
				rd_reg_hhi(HHI_AUD_PLL_CNTL_I));
			/*rx_audio_pll_sw_update();*/
		} else {
			/* disable pll, into reset mode */
			External_Mute(1);
			wr_reg_hhi(HHI_AUD_PLL_CNTL, 0x0);
		}
	} else {
		if (en) {
			rx_audio_bandgap_en();
			tmp = hdmirx_rd_phy(PHY_MAINFSM_STATUS1);
			wr_reg_hhi(HHI_AUD_PLL_CNTL, 0x20000000);
			/* audio pll div depends on input freq */
			wr_reg_hhi(HHI_AUD_PLL_CNTL6, (tmp >> 9 & 3) << 28);
			/* audio pll div fixed to N/CTS as below*/
			/* wr_reg_hhi(HHI_AUD_PLL_CNTL6, 0x40000000); */
			wr_reg_hhi(HHI_AUD_PLL_CNTL5, 0x0000002e);
			wr_reg_hhi(HHI_AUD_PLL_CNTL4, 0x30000000);
			wr_reg_hhi(HHI_AUD_PLL_CNTL3, 0x00000000);
			wr_reg_hhi(HHI_AUD_PLL_CNTL, 0x40000000);
			wr_reg_hhi(HHI_ADC_PLL_CNTL4, 0x805);
			rx_audio_pll_sw_update();
			/*External_Mute(0);*/
		} else{
			/* disable pll, into reset mode */
			External_Mute(1);
			wr_reg_hhi(HHI_AUD_PLL_CNTL, 0x20000000);
		}
	}
}

unsigned char rx_get_hdcp14_sts(void)
{
	return (unsigned char)((hdmirx_rd_dwc(DWC_HDCP_STS) >> 8) & 3);
}
/*
 * rx_get_video_info - get current avi info
 */
void rx_get_video_info(void)
{
	const unsigned int factor = 100;
	unsigned int divider = 0;
	uint32_t tmp = 0;

	/* DVI mode */
	rx.cur.hw_dvi = hdmirx_rd_bits_dwc(DWC_PDEC_STS, DVIDET) != 0;
	if (hdcp22_on) {
		/* hdcp encrypted state */
		tmp = hdmirx_rd_dwc(DWC_HDCP22_STATUS);
		rx.cur.hdcp_type = (tmp >> 4) & 1;
	} else {
		rx.cur.hdcp_type = 0;
	}

	if (rx.cur.hdcp_type == 0) {
		rx.cur.hdcp14_state = (hdmirx_rd_dwc(DWC_HDCP_STS) >> 8) & 3;
		rx.cur.hdcp22_state = 0xff;
	} else {
		rx.cur.hdcp14_state = 0xff;
		rx.cur.hdcp22_state = tmp & 1;
	}
	/* AVI parameters */
	rx.cur.hw_vic =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, VID_IDENT_CODE);
	rx.cur.cn_type =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_HB, CONETNT_TYPE);
	rx.cur.repeat =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_HB, PIX_REP_FACTOR);
	rx.cur.colorspace =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, VIDEO_FORMAT);

	rx.cur.it_content =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, IT_CONTENT);
	rx.cur.rgb_quant_range =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, RGB_QUANT_RANGE);
	rx.cur.yuv_quant_range =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_HB, YUV_QUANT_RANGE);
	#if 0
	rx.cur.active_valid =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, ACT_INFO_PRESENT);
	rx.cur.bar_valid =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, BAR_INFO_VALID);
	rx.cur.scan_info =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, SCAN_INFO);
	rx.cur.colorimetry =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, COLORIMETRY);
	rx.cur.picture_ratio =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, PIC_ASPECT_RATIO);
	rx.cur.active_ratio =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, ACT_ASPECT_RATIO);
	rx.cur.it_content =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, IT_CONTENT);
	rx.cur.ext_colorimetry =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, EXT_COLORIMETRY);
	rx.cur.rgb_quant_range =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, RGB_QUANT_RANGE);
	rx.cur.n_uniform_scale =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, NON_UNIF_SCALE);
	rx.cur.hw_vic =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, VID_IDENT_CODE);
	rx.cur.repeat =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_HB, PIX_REP_FACTOR);
	/** @note HW does not support AVI YQ1-0, */
	/* YCC quantization range */
	/** @note HW does not support AVI CN1-0, */
	/* IT content type */

	rx.cur.bar_end_top =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_TBB, LIN_END_TOP_BAR);
	rx.cur.bar_start_bottom =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_TBB, LIN_ST_BOT_BAR);
	rx.cur.bar_end_left =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_LRB, PIX_END_LEF_BAR);
	rx.cur.bar_start_right =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_LRB, PIX_ST_RIG_BAR);
	#endif

	/* refresh rate */
	tmp = hdmirx_rd_bits_dwc(DWC_MD_VTC, VTOT_CLK);
	/* tmp = (tmp == 0)? 0: (ctx->md_clk * 100000) / tmp; */
	/* if((params->vtotal == 0) || (params->htotal == 0)) */
	if (tmp == 0)
		rx.cur.frame_rate = 0;
	else
		rx.cur.frame_rate = (modet_clk * 100000) / tmp;
	/* else { */
		/* params->frame_rate = (hdmirx_get_pixel_clock() /*/
		/*	(params->vtotal * params->htotal / 100)); */

	/* } */
	/* deep color mode */
	tmp = hdmirx_rd_bits_dwc(DWC_HDMI_STS, DCM_CURRENT_MODE);

	switch (tmp) {
	case DCM_CURRENT_MODE_48b:
		rx.cur.colordepth = E_COLORDEPTH_16;
		/* divide by 2 */
		divider = 2.00 * factor;
		break;
	case DCM_CURRENT_MODE_36b:
		rx.cur.colordepth = E_COLORDEPTH_12;
		divider = 1.50 * factor;	/* divide by 1.5 */
		break;
	case DCM_CURRENT_MODE_30b:
		rx.cur.colordepth = E_COLORDEPTH_10;
		divider = 1.25 * factor;	/* divide by 1.25 */
		break;
	default:
		rx.cur.colordepth = E_COLORDEPTH_8;
		divider = 1.00 * factor;
		break;
	}
	/* pixel clock */
	rx.cur.pixel_clk = rx_measure_clock(MEASURE_CLK_PIXEL) / divider;
	/* image parameters */
	rx.cur.interlaced = hdmirx_rd_bits_dwc(DWC_MD_STS, ILACE) != 0;
	rx.cur.voffset = hdmirx_rd_bits_dwc(DWC_MD_VOL, VOFS_LIN);
	rx.cur.vactive = hdmirx_rd_bits_dwc(DWC_MD_VAL, VACT_LIN);
	rx.cur.vtotal = hdmirx_rd_bits_dwc(DWC_MD_VTL, VTOT_LIN);
	rx.cur.hoffset = hdmirx_rd_bits_dwc(DWC_MD_HT1, HOFS_PIX);
	rx.cur.hactive = hdmirx_rd_bits_dwc(DWC_MD_HACT_PX, HACT_PIX);
	rx.cur.htotal = hdmirx_rd_bits_dwc(DWC_MD_HT1, HTOT_PIX);
	rx.cur.pixel_clk = (rx.cur.pixel_clk * factor) / divider;
	rx.cur.hoffset = (rx.cur.hoffset * factor) / divider;
	rx.cur.hactive	= (rx.cur.hactive * factor) / divider;
	rx.cur.htotal = (rx.cur.htotal  * factor) / divider;
}

/*
 * hdmirx_set_video_mute - video mute
 * @mute: mute enable or disable
 */
void hdmirx_set_video_mute(bool mute)
{
	/* bluescreen cfg */
	if (rx.pre.colorspace == E_COLOR_RGB) {
		hdmirx_wr_bits_dwc(DWC_HDMI_VM_CFG_CH2, MSK(16, 0), 0x00);
		hdmirx_wr_bits_dwc(DWC_HDMI_VM_CFG_CH_0_1, MSK(16, 0), 0x00);
	} else if (rx.pre.colorspace == E_COLOR_YUV420) {
		hdmirx_wr_bits_dwc(DWC_HDMI_VM_CFG_CH2, MSK(16, 0), 0x1000);
		hdmirx_wr_bits_dwc(DWC_HDMI_VM_CFG_CH_0_1, MSK(16, 0), 0x8000);
	} else {
		hdmirx_wr_bits_dwc(DWC_HDMI_VM_CFG_CH2, MSK(16, 0), 0x8000);
		hdmirx_wr_bits_dwc(DWC_HDMI_VM_CFG_CH_0_1, MSK(16, 0), 0x8000);
	}
	hdmirx_wr_bits_dwc(DWC_HDMI_VM_CFG_CH2, _BIT(16), mute);
	if (log_level & VIDEO_LOG)
		rx_pr("%s-mute:%d\n", __func__, mute);
}

void set_dv_ll_mode(bool en)
{
	if (en) {
		hdmirx_wr_bits_top(TOP_VID_CNTL, _BIT(17), 1);
		hdmirx_wr_bits_top(TOP_VID_CNTL, _BIT(19), 1);
	} else {
		hdmirx_wr_bits_top(TOP_VID_CNTL, _BIT(17), 0);
		hdmirx_wr_bits_top(TOP_VID_CNTL, _BIT(19), 0);
	}
}

/*
 * hdmirx_config_video - video mute config
 */
void hdmirx_config_video(void)
{
	hdmirx_set_video_mute(0);
	set_dv_ll_mode(false);
}

/*
 * hdmirx_config_audio - audio channel map
 */
void hdmirx_config_audio(void)
{
	/* if audio layout bit = 1, set audio channel map
	 * according to audio speaker allocation, if layout
	 * bit = 0, use ch1 & ch2 by default.
	 */
	if (rx.aud_info.aud_hbr_rcv && hbr_force_8ch) {
		hdmirx_wr_dwc(DWC_AUD_CHEXTR_CTRL, 0xff);
		if (log_level & AUDIO_LOG)
			rx_pr("HBR rcv, force 8ch\n");
	} else if (rx.aud_info.auds_layout) {
		hdmirx_wr_bits_dwc(DWC_AUD_CHEXTR_CTRL,
			AUD_CH_MAP_CFG,
			rx.aud_info.auds_ch_alloc);
	} else {
		hdmirx_wr_bits_dwc(DWC_AUD_CHEXTR_CTRL,
			AUD_CH_MAP_CFG, 0);
	}
}

/*
 * rx_get_clock: git clock from hdmi top
 * tl1: have hdmi, cable clock
 * other: have hdmi clock
 */
int rx_get_clock(enum measure_clk_top_e clk_src)
{
	int clock = -1;
	uint32_t tmp_data = 0;
	uint32_t meas_cycles = 0;
	uint64_t tmp_data2 = 0;
	uint32_t audclk = 0;

	if (clk_src == TOP_HDMI_TMDSCLK)
		tmp_data = hdmirx_rd_top(TOP_METER_HDMI_STAT);
	else if (clk_src == TOP_HDMI_CABLECLK) {
		if (rx.chip_id >= CHIP_ID_TL1)
			tmp_data = hdmirx_rd_top(TOP_METER_CABLE_STAT);
	} else if (clk_src == TOP_HDMI_AUDIOCLK) {
		if (rx.chip_id >= CHIP_ID_TL1) {
			/*get audio clk*/
			tmp_data = hdmirx_rd_top(TOP_AUDMEAS_REF_CYCLES_STAT0);
			tmp_data2 = hdmirx_rd_top(TOP_AUDMEAS_REF_CYCLES_STAT1);
			audclk = ((tmp_data2 & 0xffff) << 32)|tmp_data;
			if (tmp_data2 & (0x1 << 17))
				audclk = (24000 * 65536) / ((audclk + 1)/1000);
			else
				rx_pr("audio clk measure fail\n");
		}
		return audclk;
	} else
		tmp_data = 0;

	/* measure stable */
	if (tmp_data & 0x80000000) {
		meas_cycles = tmp_data & 0xffffff;
		clock = (2930 * meas_cycles);/*Hz*/
		/*clock = (24000000 * meas_cycles) / 8192;*/
		/*rx_pr("hdmi_clk cycle cnt=%d,frq=%d\n",cycle_cnt,clock);*/
	}
	/*reset hdmi,cable clk meter*/
	hdmirx_wr_top(TOP_SW_RESET, 0x60);
	hdmirx_wr_top(TOP_SW_RESET, 0x0);
	return clock;
}

#if 0
/*
 * clk_util_clk_msr
 */
unsigned int clk_util_clk_msr(unsigned int clk_mux)
{
	return meson_clk_measure(clk_mux);
}

/*
 * hdmirx_get_clock - get clock interface
 * @index - clock index
 */
unsigned int hdmirx_get_clock(int index)
{
	return clk_util_clk_msr(index);
}


/*
 * hdmirx_get_tmds_clock - get tmds clock
 */
unsigned int hdmirx_get_tmds_clock(void)
{
	uint32_t clk = clk_util_clk_msr(25);

	if (clk == 0) {
		clk = hdmirx_rd_dwc(DWC_HDMI_CKM_RESULT) & 0xffff;
		clk = clk * 158000 / 4095 * 1000;
		if (log_level & VIDEO_LOG)
			rx_pr("use DWC internal tmds clk msr\n");
	}
	return clk;
}

/*
 * hdmirx_get_pixel_clock - get pixel clock
 */
unsigned int hdmirx_get_pixel_clock(void)
{
	return clk_util_clk_msr(29);
}

/*
 * hdmirx_get_audio_clock - get audio pll clock
 */
unsigned int hdmirx_get_audio_clock(void)
{
	return clk_util_clk_msr(24);
}

/*
 * hdmirx_get_mpll_div_clk - get mpll div clock
 */
unsigned int hdmirx_get_mpll_div_clk(void)
{
	return clk_util_clk_msr(27);
}

/*
 * hdmirx_get_esm_clock - get esm clock
 */
unsigned int hdmirx_get_esm_clock(void)
{
	return clk_util_clk_msr(68);
}
#endif
/*
 * function - get clk related with hdmirx
 */
unsigned int rx_measure_clock(enum measure_clk_src_e clksrc)
{
	unsigned int clock = 0;

	/*	from clock measure: txlx_clk_measure
	 *		cable [x] need read from hdmitop
	 *		tmds clock [25] Hdmirx_tmds_clk
	 *		pixel clock [29] Hdmirx_pix_clk
	 *		audio clock	[24] Hdmirx_aud_pll_clk
	 *		cts audio [98] cts_hdmirx_aud_pll_clk
	 *		mpll clock [27] Hdmirx_mpll_div_clk
	 *		esm clock [68] Cts_hdcp22_esm
	 */

	/*	from clock measure: tl1_table
	 *		cable clock [30] hdmirx_cable_clk
	 *		tmds clock [63] hdmirx_tmds_clk
	 *		pixel clock [29] hdmirx_apll_clk_out_div
	 *		audio clock	[74] hdmirx_aud_pll_clk
	 *		cts audio [60] cts_hdmirx_aud_pll_clk
	 *		mpll clock [67] hdmirx_apll_clk_audio
	 *		esm clock [68] Cts_hdcp22_esm
	 */

	if (clksrc == MEASURE_CLK_CABLE) {
		if (rx.chip_id >= CHIP_ID_TL1) {
			clock = meson_clk_measure(30);
			/*clock = rx_get_clock(TOP_HDMI_CABLECLK);*/
		}
	} else if (clksrc == MEASURE_CLK_TMDS) {
		if (rx.chip_id >= CHIP_ID_TL1)
			clock = meson_clk_measure(63);
		else {
			clock = meson_clk_measure(25);
			if (clock == 0) {
				clock =
				hdmirx_rd_dwc(DWC_HDMI_CKM_RESULT) & 0xffff;
				clock = clock * 158000 / 4095 * 1000;
			}
		}
	} else if (clksrc == MEASURE_CLK_PIXEL) {
		clock = meson_clk_measure(29);
	} else if (clksrc == MEASURE_CLK_AUD_PLL) {
		if (rx.chip_id >= CHIP_ID_TL1)
			clock = meson_clk_measure(74);/*audio vid out*/
		else
			clock = meson_clk_measure(24);
	} else if (clksrc == MEASURE_CLK_AUD_DIV) {
		if (rx.chip_id >= CHIP_ID_TL1)
			clock = meson_clk_measure(67);/*apll_clk_audio*/
		else
			clock = meson_clk_measure(98);

	} else if (clksrc == MEASURE_CLK_MPLL) {
		if (rx.chip_id >= CHIP_ID_TL1)
			clock = meson_clk_measure(29);/*apll_clk_out_div*/
		else
			clock = meson_clk_measure(27);
	} else if (clksrc == MEASURE_CLK_ESM) {
		clock = meson_clk_measure(68);
	}

	return clock;
}

static const unsigned int wr_only_register[] = {
0x0c, 0x3c, 0x60, 0x64, 0x68, 0x6c, 0x70, 0x74, 0x78, 0x7c, 0x8c, 0xa0,
0xac, 0xc8, 0xd8, 0xdc, 0x184, 0x188, 0x18c, 0x190, 0x194, 0x198, 0x19c,
0x1a0, 0x1a4, 0x1a8, 0x1ac, 0x1b0, 0x1b4, 0x1b8, 0x1bc, 0x1c0, 0x1c4,
0x1c8, 0x1cc, 0x1d0, 0x1d4, 0x1d8, 0x1dc, 0x1e0, 0x1e4, 0x1e8, 0x1ec,
0x1f0, 0x1f4, 0x1f8, 0x1fc, 0x204, 0x20c, 0x210, 0x214, 0x218, 0x21c,
0x220, 0x224, 0x228, 0x22c, 0x230, 0x234, 0x238, 0x268, 0x26c, 0x270,
0x274, 0x278, 0x290, 0x294, 0x298, 0x29c, 0x2a8, 0x2ac, 0x2b0, 0x2b4,
0x2b8, 0x2bc, 0x2d4, 0x2dc, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc,
0x314, 0x318, 0x328, 0x32c, 0x348, 0x34c, 0x350, 0x354, 0x358, 0x35c,
0x384, 0x388, 0x38c, 0x398, 0x39c, 0x3d8, 0x3dc, 0x400, 0x404, 0x408,
0x40c, 0x410, 0x414, 0x418, 0x810, 0x814, 0x818, 0x830, 0x834, 0x838,
0x83c, 0x854, 0x858, 0x85c, 0xf60, 0xf64, 0xf70, 0xf74, 0xf78, 0xf7c,
0xf88, 0xf8c, 0xf90, 0xf94, 0xfa0, 0xfa4, 0xfa8, 0xfac, 0xfb8, 0xfbc,
0xfc0, 0xfc4, 0xfd0, 0xfd4, 0xfd8, 0xfdc, 0xfe8, 0xfec, 0xff0, 0x1f04,
0x1f0c, 0x1f10, 0x1f24, 0x1f28, 0x1f2c, 0x1f30, 0x1f34, 0x1f38, 0x1f3c
};

bool is_wr_only_reg(uint32_t addr)
{
	int i;

	/*sizeof(wr_only_register)/sizeof(uint32_t)*/
	for (i = 0; i < sizeof(wr_only_register)/sizeof(uint32_t); i++) {
		if (addr == wr_only_register[i])
			return true;
	}
	return false;
}

void rx_debug_load22key(void)
{
	int ret = 0;
	int wait_kill_done_cnt = 0;

	ret = rx_sec_set_duk(hdmirx_repeat_support());
	rx_pr("22 = %d\n", ret);
	if (ret) {
		rx_pr("load 2.2 key\n");
		sm_pause = 1;
		rx_set_cur_hpd(0, 4);
		hdcp22_on = 1;
		hdcp22_kill_esm = 1;
		while (wait_kill_done_cnt++ < 10) {
			if (!hdcp22_kill_esm)
				break;
			msleep(20);
		}
		hdcp22_kill_esm = 0;
		extcon_set_state_sync(rx.rx_excton_rx22,
			EXTCON_DISP_HDMI, 0);
		hdmirx_wr_dwc(DWC_HDCP22_CONTROL, 0x0);
		hdmirx_hdcp22_esm_rst();
		mdelay(110);
		rx_is_hdcp22_support();
		hdmirx_wr_dwc(DWC_HDCP22_CONTROL,
					0x1000);
		rx_hdcp22_wr_top(TOP_SKP_CNTL_STAT, 0x1);
		hdcp22_clk_en(1);
		extcon_set_state_sync(rx.rx_excton_rx22,
			EXTCON_DISP_HDMI, 1);
		mdelay(100);
		hdmi_rx_top_edid_update();
		hdmirx_hw_config();
		hpd_to_esm = 1;
		/* mdelay(900); */
		rx_set_cur_hpd(1, 4);
		sm_pause = 0;
	}
}

void rx_debug_loadkey(void)
{
	rx_pr("load hdcp key\n");
	hdmi_rx_top_edid_update();
	hdmirx_hw_config();
	pre_port = 0xfe;
}

void print_reg(uint start_addr, uint end_addr)
{
	int i;

	if (end_addr < start_addr)
		return;

	for (i = start_addr; i <= end_addr; i += sizeof(uint)) {
		if ((i - start_addr) % (sizeof(uint)*4) == 0)
			rx_pr("[0x%-4x] ", i);
		if (!is_wr_only_reg(i))
			rx_pr("0x%-8x,", hdmirx_rd_dwc(i));
		else
			rx_pr("xxxxxx    ,");

		if ((i - start_addr) % (sizeof(uint)*4) == sizeof(uint)*3)
			rx_pr("\n");
	}

	if ((end_addr - start_addr + sizeof(uint)) % (sizeof(uint)*4) != 0)
		rx_pr("\n");
}

void dump_reg(void)
{
	int i = 0;

	rx_pr("\n***Top registers***\n");
	rx_pr("[addr ]  addr + 0x0,");
	rx_pr("addr + 0x1,  addr + 0x2,	addr + 0x3\n");
	for (i = 0; i <= 0x24;) {
		rx_pr("[0x%-3x]", i);
		rx_pr("0x%-8x", hdmirx_rd_top(i));
		rx_pr("0x%-8x,0x%-8x,0x%-8x\n",
			hdmirx_rd_top(i + 1),
			hdmirx_rd_top(i + 2),
			hdmirx_rd_top(i + 3));
		i = i + 4;
	}

	if (rx.chip_id >= CHIP_ID_TL1) {
		for (i = 0x25; i <= 0x84;) {
			rx_pr("[0x%-3x]", i);
			rx_pr("0x%-8x", hdmirx_rd_top(i));
			rx_pr("0x%-8x,0x%-8x,0x%-8x\n",
				hdmirx_rd_top(i + 1),
				hdmirx_rd_top(i + 2),
				hdmirx_rd_top(i + 3));
			i = i + 4;
		}
	}

	if (rx.chip_id < CHIP_ID_TL1) {
		rx_pr("\n***PHY registers***\n");
		rx_pr("[addr ]  addr + 0x0,");
		rx_pr("addr + 0x1,addr + 0x2,");
		rx_pr("addr + 0x3\n");
		for (i = 0; i <= 0x9a;) {
			rx_pr("[0x%-3x]", i);
			rx_pr("0x%-8x", hdmirx_rd_phy(i));
			rx_pr("0x%-8x,0x%-8x,0x%-8x\n",
				hdmirx_rd_phy(i + 1),
				hdmirx_rd_phy(i + 2),
				hdmirx_rd_phy(i + 3));
			i = i + 4;
		}

	} else if (rx.chip_id >= CHIP_ID_TL1) {
		/* dump phy register */
		rx_pr("\n***AML PHY registers***\n");
		for (i = HHI_HDMIRX_APLL_CNTL0;
				i <= HHI_HDMIRX_APLL_CNTL4;) {
			rx_pr("apll cntl 0x%x:0x%8x\n", i >> 2, rd_reg_hhi(i));
			i = i + 4;
		}

		rx_pr("MISC_CNTL0:0x%8x\n",
			rd_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL0));
		rx_pr("MISC_CNTL1:0x%8x\n",
			rd_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL1));

		for (i = HHI_HDMIRX_PHY_MISC_CNTL2;
				i <= HHI_HDMIRX_PHY_DCHD_CNTL2;) {
			rx_pr("phy 0x%x:0x%8x\n", i >> 2, rd_reg_hhi(i));
			i = i + 4;
		}
		rx_pr("MISC_STAT0:0x%8x\n",
			hdmirx_rd_top(TOP_MISC_STAT0));
		rx_pr("DCHD_STAT:0x%8x\n",
			rd_reg_hhi(HHI_HDMIRX_PHY_DCHD_STAT));
	}

	rx_pr("\n**Controller registers**\n");
	rx_pr("[addr ]  addr + 0x0,");
	rx_pr("addr + 0x4,  addr + 0x8,");
	rx_pr("addr + 0xc\n");
	print_reg(0, 0xfc);
	print_reg(0x140, 0x3ac);
	print_reg(0x3c0, 0x418);
	print_reg(0x480, 0x4bc);
	print_reg(0x600, 0x610);
	print_reg(0x800, 0x87c);
	print_reg(0x8e0, 0x8e0);
	print_reg(0x8fc, 0x8fc);
	print_reg(0xf60, 0xffc);

	/* print_reg(0x2000, 0x21fc); */
	/* print_reg(0x2700, 0x2714); */
	/* print_reg(0x2f00, 0x2f14); */
	/* print_reg(0x3000, 0x3020); */
	/* print_reg(0x3040, 0x3054); */
	/* print_reg(0x3080, 0x3118); */
	/* print_reg(0x3200, 0x32e4); */
}

void dump_edid_reg(void)
{
	int i = 0;
	int j = 0;

	rx_pr("\n***********************\n");
	rx_pr("0x1 1.4 edid\n");
	rx_pr("0x2 1.4 edid with audio blocks\n");
	rx_pr("0x3 1.4 edid with 420 capability\n");
	rx_pr("0x4 1.4 edid with 420 video data\n");
	rx_pr("0x5 2.0 edid with HDR,DV,420\n");
	rx_pr("********************************\n");
	if (rx.chip_id < CHIP_ID_TL1) {
		for (i = 0; i < 16; i++) {
			rx_pr("[%2d] ", i);
			for (j = 0; j < 16; j++) {
				rx_pr("0x%02x, ",
				      hdmirx_rd_top(TOP_EDID_OFFSET +
							(i * 16 + j)));
			}
			rx_pr("\n");
		}
	} else if (rx.chip_id == CHIP_ID_TL1) {
		for (i = 0; i < 16; i++) {
			rx_pr("[%2d] ", i);
			for (j = 0; j < 16; j++) {
				rx_pr("0x%02x, ",
				      hdmirx_rd_top(TOP_EDID_ADDR_S +
							(i * 16 + j)));
			}
			rx_pr("\n");
		}
	} else {
		for (i = 0; i < 16; i++) {
			rx_pr("[%2d] ", i);
			for (j = 0; j < 16; j++) {
				rx_pr("0x%02x, ",
				      hdmirx_rd_top(TOP_EDID_ADDR_S +
							(i * 16 + j)));
			}
			rx_pr("\n");
		}
		for (i = 0; i < 16; i++) {
			rx_pr("[%2d] ", i);
			for (j = 0; j < 16; j++) {
				rx_pr("0x%02x, ",
				      hdmirx_rd_top(TOP_EDID_PORT2_ADDR_S +
							(i * 16 + j)));
			}
			rx_pr("\n");
		}
		for (i = 0; i < 16; i++) {
			rx_pr("[%2d] ", i);
			for (j = 0; j < 16; j++) {
				rx_pr("0x%02x, ",
				      hdmirx_rd_top(TOP_EDID_PORT3_ADDR_S +
							(i * 16 + j)));
			}
			rx_pr("\n");
		}
	}
}

int rx_debug_wr_reg(const char *buf, char *tmpbuf, int i)
{
	uint32_t adr = 0;
	uint32_t value = 0;

	if (kstrtou32(tmpbuf + 3, 16, &adr) < 0)
		return -EINVAL;
	rx_pr("adr = %#x\n", adr);
	if (kstrtou32(buf + i + 1, 16, &value) < 0)
		return -EINVAL;
	rx_pr("value = %#x\n", value);
	if (tmpbuf[1] == 'h') {
		if (buf[2] == 't') {
			hdmirx_wr_top(adr, value);
			rx_pr("write %x to TOP [%x]\n",
				value, adr);
		} else if (buf[2] == 'd') {
			hdmirx_wr_dwc(adr, value);
			rx_pr("write %x to DWC [%x]\n",
				value, adr);
		} else if (buf[2] == 'p') {
			hdmirx_wr_phy(adr, value);
			rx_pr("write %x to PHY [%x]\n",
				value, adr);
		} else if (buf[2] == 'u') {
			wr_reg_hhi(adr, value);
			rx_pr("write %x to hiu [%x]\n",
				value, adr);
		} else if (buf[2] == 'h') {
			rx_hdcp22_wr_top(adr, value);
			rx_pr("write %x to hdcp [%x]\n",
				value, adr);
		} else if (buf[2] == 'c') {
			rx_hdcp22_wr_reg(adr, value);
			rx_pr("write %x to chdcp [%x]\n",
				value, adr);
		}
	}
	return 0;
}

int rx_debug_rd_reg(const char *buf, char *tmpbuf)
{
	uint32_t adr = 0;
	uint32_t value = 0;

	if (tmpbuf[1] == 'h') {
		if (kstrtou32(tmpbuf + 3, 16, &adr) < 0)
			return -EINVAL;
		if (tmpbuf[2] == 't') {
			value = hdmirx_rd_top(adr);
			rx_pr("TOP [%x]=%x\n", adr, value);
		} else if (tmpbuf[2] == 'd') {
			value = hdmirx_rd_dwc(adr);
			rx_pr("DWC [%x]=%x\n", adr, value);
		} else if (tmpbuf[2] == 'p') {
			value = hdmirx_rd_phy(adr);
			rx_pr("PHY [%x]=%x\n", adr, value);
		} else if (tmpbuf[2] == 'u') {
			value = rd_reg_hhi(adr);
			rx_pr("HIU [%x]=%x\n", adr, value);
		} else if (tmpbuf[2] == 'h') {
			value = rx_hdcp22_rd_top(adr);
			rx_pr("HDCP [%x]=%x\n", adr, value);
		} else if (tmpbuf[2] == 'c') {
			value = rx_hdcp22_rd_reg(adr);
			rx_pr("chdcp [%x]=%x\n", adr, value);
		}
	}
	return 0;
}

int rx_get_aud_pll_err_sts(void)
{
	int ret = E_AUDPLL_OK;
	int32_t req_clk = rx_measure_clock(MEASURE_CLK_MPLL);
	u32 aud_clk = rx.aud_info.aud_clk;
	uint32_t phy_pll_rate = (hdmirx_rd_phy(PHY_MAINFSM_STATUS1)>>9)&0x3;
	uint32_t aud_pll_cntl = (rd_reg_hhi(HHI_AUD_PLL_CNTL6)>>28)&0x3;

	if (rx.chip_id >= CHIP_ID_TL1) {
		/* need to do something ...*/
	} else {
		if (req_clk > PHY_REQUEST_CLK_MAX ||
				req_clk < PHY_REQUEST_CLK_MIN) {
			ret = E_REQUESTCLK_ERR;
			if (log_level & AUDIO_LOG)
				rx_pr("request clk err:%d\n", req_clk);
		} else if (phy_pll_rate != aud_pll_cntl) {
			ret = E_PLLRATE_CHG;
			if (log_level & AUDIO_LOG)
				rx_pr("pll rate chg,phy=%d,pll=%d\n",
					phy_pll_rate, aud_pll_cntl);
		} else if (aud_clk == 0) {
			ret = E_AUDCLK_ERR;
			if (log_level & AUDIO_LOG)
				rx_pr("aud_clk=0\n");
		}
	}
	return ret;
}

uint32_t aml_cable_clk_band(uint32_t cableclk,
	uint32_t clkrate)
{
	uint32_t bw;
	uint32_t cab_clk = cableclk;

	if (rx.chip_id < CHIP_ID_TL1)
		return phy_frq_band_2;

	/* rx_pr("cable clk=%d, clkrate=%d\n", cableclk, clkrate); */
	/* 1:40 */
	if (clkrate)
		cab_clk = cableclk << 2;

	/* 1:10 */
	if (cab_clk < (45*MHz))
		bw = phy_frq_band_0;
	else if (cab_clk < (77*MHz))
		bw = phy_frq_band_1;
	else if (cab_clk < (155*MHz))
		bw = phy_frq_band_2;
	else if (cab_clk < (340*MHz))
		bw = phy_frq_band_3;
	else if (cab_clk < (525*MHz))
		bw = phy_frq_band_4;
	else if (cab_clk < (600*MHz))
		bw = phy_frq_band_5;
	else {
		bw = phy_frq_band_2;
		rx_pr("bw err,clk=%d\n", cableclk/MHz);
	}
	return bw;
}

uint32_t aml_phy_pll_band(uint32_t cableclk,
	uint32_t clkrate)
{
	uint32_t bw;
	uint32_t cab_clk = cableclk;

	if (clkrate)
		cab_clk = cableclk << 2;

	/* 1:10 */
	if (cab_clk < (35*MHz))
		bw = pll_frq_band_0;
	else if (cab_clk < (77*MHz))
		bw = pll_frq_band_1;
	else if (cab_clk < (155*MHz))
		bw = pll_frq_band_2;
	else if (cab_clk < (300*MHz))
		bw = pll_frq_band_3;
	else if (cab_clk < (600*MHz))
		bw = pll_frq_band_4;
	else
		bw = pll_frq_band_2;

	return bw;
}

static const uint32_t phy_misci[][4] = {
		/* 0xd7		0xd8		0xe0		0xe1 */
	{	/* 24~45M */
		0x3003707f, 0x00000080, 0x02218000, 0x00000010,
	},
	{	/* 45~74.5M */
		0x3003707f, 0x00000080, 0x02218000, 0x00000010,
	},
	{	/* 77~155M */
		0x3003707f, 0x00000080, 0x02218000, 0x00000010,
	},
	{	/* 155~340M */
		0x3003707f, 0x00000080, 0x02218000, 0x00000010,
	},
	{	/* 340~525M */
		0x3003707f, 0x007f0080, 0x02218000, 0x00000010,
	},
	{	/* 525~600M */
		0x3003707f, 0x007f8080, 0x02218000, 0x00000010,
	},
};

static const uint32_t phy_dcha[][3] = {
		/*	0xe2		0xe3		0xe4 */
	{	/* 24~45M */
		0x00000280, 0x4400c202, 0x030088a2,
	},
	{	/* 45~74.5M */
		0x00000280, 0x4400c202, 0x030088a2,
	},
	{	/* 77~155M */
		0x000002a2, 0x6800c202, 0x01009126,
	},
	{	/* 155~340M */
		0x00000280, 0x0800c202, 0x0100cc31,
	},
	{	/* 340~525M */
		0x00000280, 0x0700003c, 0x1d00cc31,
	},
	{	/* 525~600M */
		0x00000280, 0x07000000, 0x1d00cc31,
	},
};

static const uint32_t phy_dcha_reva[][3] = {
		/*	0xe2		0xe3		0xe4 */
	{	/* 24~45M */
		0x00000280, 0x2400c202, 0x030088a2,
	},
	{	/* 45~74.5M */
		0x00000280, 0x2400c202, 0x030088a2,
	},
	{	/* 77~155M */
		0x000002a2, 0x4800c202, 0x01009126,
	},
	{	/* 155~340M */
		0x000002a2, 0x0800c202, 0x0100cc31,
	},
	{	/* 340~525M */
		0x000002a2, 0x0700003c, 0x1d00cc31,
	},
	{	/* 525~600M */
		0x000002a2, 0x0700003c, 0x1d00cc31,
	},
};


/* long cable */
static const uint32_t phy_dchd_1[][3] = {
		/*	0xe5		0xe6		0xe7 */
	{	/* 24~45M */
		0x003e714a, 0x1e051630, 0x00018000,
	},
	{	/* 45~74.5M */
		0x003e714a, 0x1e051630, 0x00018000,
	},
	{	/* 77~155M */
		0x003c714a, 0x1e062620, 0x00018000,
	},
	{	/* 155~340M */
		0x003c714a, 0x1e050064, 0x0001a000,
	},
	{	/* 340~525M */
		0x003c714a, 0x1e050064, 0x0001a000,
	},
	{	/* 525~600M */
		0x003e714a, 0x1e050560, 0x0001a000,
	},
};

/* short cable */
static const uint32_t phy_dchd_2[][3] = {
		/*	0xe5		0xe6		0xe7 */
	{	/* 24~45M */
		0x003e714a, 0x1e022220, 0x00018000,
	},
	{	/* 45~74.5M */
		0x003e714a, 0x1e022220, 0x00018000,
	},
	{	/* 77~155M */
		0x003c714a, 0x1e022220, 0x00018000,
	},
	{	/* 155~340M */
		0x003c714a, 0x1e022220, 0x0001a000,
	},
	{	/* 340~525M */
		0x003c714a, 0x1e040460, 0x0001a000,
	},
	{	/* 525~600M */
		0x003e714a, 0x1e040460, 0x0001a000,
	},
};

static const u32 eq_debug[] = {
	/* value     eq gain */
	0xffffffff,
	0xffff1030,/* min */
	0x7fff1030,
	0x7f7f1030,
	0x3f7f1030,
	0x3f3f1030,
	0x1f3f1030,
	0x1f1f1030,
	0x0f0f1030,/* half of all range max */
	0x0f071030,
	0x07071030,
	0x07031030,
	0x03031030,
	0x03011030,
	0x01011030,
	0x01001030,
	0x00001030,/* max */
	0xffff0000,/* min */
	0x7fff0000,
	0x7f7f0000,
	0x3f7f0000,
	0x3f3f0000,
	0x1f3f0000,
	0x1f1f0000,
	0x0f0f0000,/* half of all range max */
	0x0f070000,
	0x07070000,
	0x07030000,
	0x03030000,
	0x03010000,
	0x01010000,
	0x01000000,
	0x00000000,/* max */
};

void aml_phy_switch_port(void)
{
	   uint32_t data32;
	   /* reset and select data port */
	   data32 = 0x00000010;
	   data32 |= ((1 << rx.port) << 6);
	   wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL3, data32);
	   /* release reset */
	   data32 |= (1 << 11);
	   wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL3, data32);
	   udelay(5);

	   data32 = 0;
	   data32 |= rx.port << 2;
	   hdmirx_wr_dwc(DWC_SNPS_PHYG3_CTRL, data32);
}

void aml_phy_init_1(void)
{
	uint32_t idx = rx.phy.phy_bw;
	uint32_t data32;

	data32 = phy_misci[idx][1];
	wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL1, data32);

	wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL2,
		phy_misci[idx][2]);

	/* no need reselect port and reset clk comm again */
	/* reset and select data port */
	/*data32 = phy_misci[idx][3];*/
	/*data32 |= ((1 << rx.port) << 6);*/
	/*wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL3, data32);*/

	/* no need reset clock channel */
	/*data32 |= (1 << 11);*/
	/*wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL3, data32);*/

	if (is_tl1_former()) {
		wr_reg_hhi(HHI_HDMIRX_PHY_DCHA_CNTL0,
			phy_dcha_reva[idx][0]);

		wr_reg_hhi(HHI_HDMIRX_PHY_DCHA_CNTL1,
			phy_dcha_reva[idx][1]);

		wr_reg_hhi(HHI_HDMIRX_PHY_DCHA_CNTL2,
			phy_dcha_reva[idx][2]);
	} else {
		wr_reg_hhi(HHI_HDMIRX_PHY_DCHA_CNTL0,
			phy_dcha[idx][0]);

		wr_reg_hhi(HHI_HDMIRX_PHY_DCHA_CNTL1,
			phy_dcha[idx][1]);

		wr_reg_hhi(HHI_HDMIRX_PHY_DCHA_CNTL2,
			phy_dcha[idx][2]);
	}
	if (cdr_lock_level == 0)
		wr_reg_hhi(HHI_HDMIRX_PHY_DCHD_CNTL0,
			phy_dchd_1[idx][0]);
	else if (cdr_lock_level == 1)
		wr_reg_hhi(HHI_HDMIRX_PHY_DCHD_CNTL0,
			0x006f0041);
	else if (cdr_lock_level == 2)
		wr_reg_hhi(HHI_HDMIRX_PHY_DCHD_CNTL0,
			0x002f714a);
	wr_reg_hhi(HHI_HDMIRX_PHY_DCHD_CNTL2,
		phy_dchd_1[idx][2]);

	/* change enable sequency */
	data32 = rd_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL0);
	data32 |= (0xf << 7);
	wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL0, data32);
	udelay(5);
	/* data channel release reset */
	/* change enable sequency */
	udelay(50);
		if ((rx.phy.cablesel % 2) == 0)
		data32 = phy_dchd_1[idx][1];
	else if ((rx.phy.cablesel % 2) == 1)
		data32 = phy_dchd_2[idx][1];

	wr_reg_hhi(HHI_HDMIRX_PHY_DCHD_CNTL1, data32);/*398*/
	udelay(5);
	data32 |= 0x00400000;
	wr_reg_hhi(HHI_HDMIRX_PHY_DCHD_CNTL1, data32);/*398*/
}

bool is_ft_trim_done(void)
{
	int ret = phy_trim_val & 0x1;

	rx_pr("ft trim=%d\n", ret);
	return ret;
}

void aml_phy_get_trim_val(void)
{
	u32 data32;

	if (rx.chip_id < CHIP_ID_TL1)
		return;
	phy_tdr_en = (phy_term_lel >> 4) & 0x1;
	phy_term_lel = phy_term_lel & 0xf;
	phy_trim_val = rd_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL1);
	data32 = (phy_trim_val >> 12) & 0x3ff;
	data32 = (~((~data32) << phy_term_lel) | (1 << phy_term_lel));
	phy_trim_val = ((phy_trim_val & (~(0x3ff << 12)))
		| (data32 << 12));
}
void aml_phy_init(void)
{
	uint32_t idx = rx.phy.phy_bw;
	u32 data32, c1, c2, r1, r2;
	uint32_t term_value =
		hdmirx_rd_top(TOP_HPD_PWR5V) & 0x7;

	c1 = eq_debug[eq_dbg_lvl] & 0xff;
	c2 = (eq_debug[eq_dbg_lvl] >> 8) & 0xff;
	r1 = (eq_debug[eq_dbg_lvl] >> 16) & 0xff;
	r2 = (eq_debug[eq_dbg_lvl] >> 24) & 0xff;
	data32 = phy_misci[idx][0];
	data32 = (data32 & (~0x7));
	data32 |= term_value;
	data32 &= ~(disable_port_num & 0x07);
	/* terminal en */
	wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL0, data32);
	udelay(2);
	/* data channel and common block reset */
	/*update from "data channel and common block */
	/* reset"to"only common block reset" */
	data32 |= 0xf << 7;
	/* data32 |= 0x1 << 10; */
	udelay(5);
	wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL0, data32);
	udelay(2);

	data32 = phy_misci[idx][1];
	if ((idx < phy_frq_band_5) && phy_tdr_en) {
		if (term_cal_en) {
			data32 = (((data32 & (~(0x3ff << 12))) |
				(term_cal_val << 12)) | (1 << 22));
		} else {
			data32 = phy_trim_val;
		}
	}
	wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL1, data32);
	wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL2, phy_misci[idx][2]);

	/* reset and select data port */
	data32 = phy_misci[idx][3];
	data32 |= ((1 << rx.port) << 6);
	wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL3, data32);

	/* release reset */
	data32 |= (1 << 11);
	wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL3, data32);

	udelay(5);
	if (is_tl1_former()) {
		wr_reg_hhi(HHI_HDMIRX_PHY_DCHA_CNTL0,
			phy_dcha_reva[idx][0]);

		wr_reg_hhi(HHI_HDMIRX_PHY_DCHA_CNTL1,
			phy_dcha_reva[idx][1]);

		wr_reg_hhi(HHI_HDMIRX_PHY_DCHA_CNTL2,
			phy_dcha_reva[idx][2]);
	} else {
		if (eq_dbg_lvl == 0) {
			wr_reg_hhi(HHI_HDMIRX_PHY_DCHA_CNTL0,
				   phy_dcha[idx][0]);

			wr_reg_hhi(HHI_HDMIRX_PHY_DCHA_CNTL1,
				   phy_dcha[idx][1]);

			wr_reg_hhi(HHI_HDMIRX_PHY_DCHA_CNTL2,
				   phy_dcha[idx][2]);
		} else {
			wr_reg_hhi(HHI_HDMIRX_PHY_DCHA_CNTL0,
				   (phy_dcha[idx][0] | (c1 << 10)
				| (c2 << 18) | (r1 << 26)));

			wr_reg_hhi(HHI_HDMIRX_PHY_DCHA_CNTL1,
				   (phy_dcha[idx][1] | (r1 >> 6))
				| (r2 << 2));

			wr_reg_hhi(HHI_HDMIRX_PHY_DCHA_CNTL2,
				   (phy_dcha[idx][2] & (~(1 << 24))));
		}
	}

	if (cdr_lock_level == 0)
		wr_reg_hhi(HHI_HDMIRX_PHY_DCHD_CNTL0,
			phy_dchd_1[idx][0]);
	else if (cdr_lock_level == 1)
		wr_reg_hhi(HHI_HDMIRX_PHY_DCHD_CNTL0,
			0x006f0041);
	else if (cdr_lock_level == 2)
		wr_reg_hhi(HHI_HDMIRX_PHY_DCHD_CNTL0,
			0x002f714a);
	wr_reg_hhi(HHI_HDMIRX_PHY_DCHD_CNTL2,
		phy_dchd_1[idx][2]);

	wr_reg_hhi(HHI_HDMIRX_PHY_DCHD_CNTL2, phy_dchd_1[idx][2]);
	if ((rx.phy.cablesel % 2) == 0)
		data32 = phy_dchd_1[idx][1];
	else if ((rx.phy.cablesel % 2) == 1)
		data32 = phy_dchd_2[idx][1];

	/* wr_reg_hhi(HHI_HDMIRX_PHY_DCHD_CNTL1, data32); */
	udelay(5);
	/*data32 |= 0x00400000;*/
	wr_reg_hhi(HHI_HDMIRX_PHY_DCHD_CNTL1, data32);/*398*/
}

void aml_eq_setting(void)
{
	uint32_t data32 = 0;
	uint32_t idx = rx.phy.phy_bw;

	/* data channel release reset */
	data32 = rd_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL0);
	data32 |= (0x7 << 7);
	wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL0, data32);
	udelay(50);
	if (find_best_eq) {
		data32 = phy_dchd_1[idx][1] & (~(MSK(16, 4)));
		data32 |= find_best_eq << 4;
	} else if ((rx.phy.cablesel % 2) == 0)
		data32 = phy_dchd_1[idx][1];
	else if ((rx.phy.cablesel % 2) == 1)
		data32 = phy_dchd_2[idx][1];

	wr_reg_hhi(HHI_HDMIRX_PHY_DCHD_CNTL1, data32);
	udelay(5);
	data32 |= 0x00400000;
	wr_reg_hhi(HHI_HDMIRX_PHY_DCHD_CNTL1, data32);
}

void rx_get_best_eq_setting(void)
{
	uint32_t ch0, ch1, ch2;
	static uint32_t err_sum;
	static uint32_t time_cnt;
	static uint32_t array_cnt;

	if ((rx.chip_id < CHIP_ID_TL1) ||
		(!find_best_eq))
		return;
	if ((find_best_eq >= 0x7777) ||
		(array_cnt >= 255)) {
		rx_pr("eq traversal completed.\n");
		rx_pr("best eq value:%d\n", array_cnt);
		if (array_cnt) {
			do  {
				rx_pr("%x:\n", rx.phy.eq_data[array_cnt]);
			} while (array_cnt--);
		} else
			rx_pr("%x:\n", rx.phy.eq_data[array_cnt]);
		find_best_eq = 0;
		array_cnt = 0;
		return;
	}
	if (time_cnt == 0) {
		aml_phy_init();
		udelay(1);
		wr_reg_hhi_bits(HHI_HDMIRX_PHY_DCHD_CNTL1,
			MSK(16, 4),
			find_best_eq);
		udelay(2);
		wr_reg_hhi_bits(HHI_HDMIRX_PHY_DCHD_CNTL1, _BIT(22), 1);
		rx_pr("set eq:%x\n", find_best_eq);
		err_sum = 0;
		do {
			find_best_eq++;
		} while (((find_best_eq & 0xf) > 7) ||
				(((find_best_eq >> 4) & 0xf) > 7) ||
				(((find_best_eq >> 8) & 0xf) > 7) ||
				(((find_best_eq >> 12) & 0xf) > 7));
	}
	time_cnt++;
	if (time_cnt > 2) {
		if (!is_tmds_valid())
			return;
	}
	if (time_cnt > 4) {
		rx_get_error_cnt(&ch0, &ch1, &ch2);
		err_sum += (ch0 + ch1 + ch2);
	}
	if (time_cnt > eq_try_cnt) {
		time_cnt = 0;
		if (err_sum < rx.phy.err_sum) {
			rx.phy.err_sum = err_sum;
			rx_pr("err_sum = %d\n", err_sum);
			array_cnt = 0;
			rx.phy.eq_data[array_cnt] = find_best_eq;
		} else if ((err_sum == rx.phy.err_sum) ||
			(err_sum == 0)) {
			rx.phy.err_sum = err_sum;
			array_cnt++;
			rx_pr("array = %x\n", array_cnt);
			rx.phy.eq_data[array_cnt] = find_best_eq;
		}
		//if (ch0 || ch1 || ch2)
			//rx_pr("err cnt:%d,%d,%d\n", ch0, ch1, ch2); ;
	}
}

bool is_tmds_clk_stable(void)
{
	bool ret = true;
	uint32_t cableclk;
	uint32_t pixel_clk;

	if (rx.phy.clk_rate)
		cableclk = rx.phy.cable_clk * 4;
	else
		cableclk = rx.phy.cable_clk;

	pixel_clk = meson_clk_measure(29);
	if (abs(cableclk - pixel_clk) > clock_lock_th * MHz) {
		if (log_level & VIDEO_LOG)
			rx_pr("cableclk=%d,tmdsclk=%d,pixelclk=%d\n",
				cableclk/MHz, rx.phy.tmds_clk/MHz,
				meson_clk_measure(29)/MHz);
		ret = false;
	} else
		ret = true;

	return ret;
}

/*
 * for tl1 phy function
 */
struct apll_param apll_tab[] = {
	/*od for tmds: 2/4/8/16/32*/
	/*od2 for audio: 1/2/4/8/16*/
	/* bw M, N, od, od_div, od2, od2_div, aud_div */
	/* {pll_frq_band_0, 160, 1, 0x5, 32,0x2, 8, 2}, */
	{pll_frq_band_0, 160, 1, 0x5, 32, 0x1, 8, 3},/* 16 x 27 */
	/* {pll_frq_band_1, 80, 1, 0x4,	16, 0x2, 8, 1}, */
	{pll_frq_band_1, 80, 1, 0x4, 16, 0x0, 8, 3},/* 8 x 74 */
	/* {pll_frq_band_2, 40, 1, 0x3, 8,	0x2, 8, 0}, */
	{pll_frq_band_2, 40, 1, 0x3, 8,	 0x0, 8, 2}, /* 4 x 148 */
	/* {pll_frq_band_3, 40, 2, 0x2, 4,	0x1, 4, 0}, */
	{pll_frq_band_3, 40, 2, 0x2, 4,	 0x0, 4, 1},/* 2 x 297 */
	/* {pll_frq_band_4, 40, 1, 0x1, 2,	0x0, 2, 0}, */
	{pll_frq_band_4, 40, 1, 0x1, 2,	 0x0, 2, 0},/* 594 */
	{pll_frq_null, 40, 1, 0x3, 8,	0x2, 8, 0},
};

void aml_phy_pll_setting(void)
{
	uint32_t M, N;
	uint32_t od, od_div;
	uint32_t od2, od2_div;
	uint32_t bw = rx.phy.pll_bw;
	uint32_t vco_clk;
	uint32_t data, data2;
	uint32_t cableclk = rx.phy.cable_clk / KHz;
	int pll_rst_cnt = 0;

	od_div = apll_tab[bw].od_div;
	od = apll_tab[bw].od;
	M = apll_tab[bw].M;
	N = apll_tab[bw].N;
	od2_div = apll_tab[bw].od2_div;
	od2 = apll_tab[bw].od2;

	/*set audio pll divider*/
	rx.phy.aud_div = apll_tab[bw].aud_div;
	vco_clk = (cableclk * M) / N; /*KHz*/
	if ((vco_clk < (2970 * KHz)) || (vco_clk > (6000 * KHz))) {
		if (log_level & VIDEO_LOG)
			rx_pr("err: M=%d,N=%d,vco_clk=%d\n", M, N, vco_clk);
	}

	if (is_tl1_former())
		od2 += 1;
	do {
		/*cntl0 M <7:0> N<14:10>*/
		data = 0x00090400 & 0xffff8300;
		data |= M;
		data |= (N << 10);
		wr_reg_hhi(HHI_HDMIRX_APLL_CNTL0, data | 0x20000000);
		udelay(5);
		wr_reg_hhi(HHI_HDMIRX_APLL_CNTL0, data | 0x30000000);
		udelay(5);
		wr_reg_hhi(HHI_HDMIRX_APLL_CNTL1, 0x00000000);
		udelay(5);
		wr_reg_hhi(HHI_HDMIRX_APLL_CNTL2, 0x00001118);
		udelay(5);
		data2 = 0x10058f30 | od2;
		wr_reg_hhi(HHI_HDMIRX_APLL_CNTL3, data2);
		if (is_tl1_former())
			data2 = 0x000100c0;
		else
			data2 = 0x080130c0;
		data2 |= (od << 24);
		wr_reg_hhi(HHI_HDMIRX_APLL_CNTL4, data2);
		udelay(5);
		/*apll_vctrl_mon_en*/
		wr_reg_hhi(HHI_HDMIRX_APLL_CNTL4, data2 | 0x00800000);
		udelay(5);
		wr_reg_hhi(HHI_HDMIRX_APLL_CNTL0, data | 0x34000000);
		udelay(5);
		wr_reg_hhi(HHI_HDMIRX_APLL_CNTL0, data | 0x14000000);
		udelay(5);

		/* bit'5: force lock bit'2: improve phy ldo voltage */
		wr_reg_hhi(HHI_HDMIRX_APLL_CNTL2, 0x0000303c);

		udelay(100);
		if (pll_rst_cnt++ > pll_rst_max) {
			rx_pr("pll rst error\n");
			break;
		}
		if (log_level & VIDEO_LOG)
			rx_pr("pll init-cableclk=%d,pixelclk=%d,sq=%d\n",
			rx.phy.cable_clk/MHz,
				meson_clk_measure(29)/MHz,
				hdmirx_rd_top(TOP_MISC_STAT0) & 0x1);
	} while ((!is_tmds_clk_stable()) && is_clk_stable());

	/* data channel reset */
	data = rd_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL0);
	data &= (~(0x7 << 7));
	wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL0, data);
}

void aml_phy_pw_onoff(uint32_t onoff)
{
	uint32_t data = rd_reg_hhi(HHI_HDMIRX_APLL_CNTL0);

	if (onoff) {
		/* apll power down */
		data &= ~(1 << 26);
		data &= ~(1 << 28);
		data |= (1 << 29);
		wr_reg_hhi(HHI_HDMIRX_APLL_CNTL0, data);

		/*phy */
		data = rd_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL0);
		data &= ~(1 << 7);
		data &= ~(1 << 8);
		data &= ~(1 << 9);
		wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL0, data);
	} else {
		aml_phy_init();
		/*tl1_apll_setting(apll_frq_band_2);*/
	}
}

/*
 * aml phy initial
 */
void aml_phy_bw_switch(void)
{
	aml_phy_init();
	udelay(10);
	aml_phy_pll_setting();
	udelay(10);
	aml_eq_setting();
	/* aml_phy_init_1(); */
}

unsigned int aml_phy_pll_lock(void)
{
	if (rd_reg_hhi(HHI_HDMIRX_APLL_CNTL0) & 0x80000000)
		return true;
	else
		return false;
}

bool is_tmds_valid(void)
{
	if (force_vic)
		return true;

	if (rx.chip_id >= CHIP_ID_TL1)
		return (aml_phy_tmds_valid() == 1) ? true : false;
	else
		return (rx_get_pll_lock_sts() == 1) ? true : false;
}

unsigned int aml_phy_tmds_valid(void)
{
	unsigned int tmds_valid;
	unsigned int tmdsclk_valid;
	unsigned int sqofclk;
	/* unsigned int pll_lock; */
	unsigned int tmds_align;

	tmds_valid = hdmirx_rd_dwc(DWC_HDMI_PLL_LCK_STS) & 0x01;
	sqofclk = hdmirx_rd_top(TOP_MISC_STAT0) & 0x1;
	/*pll_lock = rd_reg_hhi(HHI_HDMIRX_APLL_CNTL0) & 0x80000000;*/
	tmdsclk_valid = is_tmds_clk_stable();
	tmds_align = hdmirx_rd_top(TOP_TMDS_ALIGN_STAT) & 0x3f000000;
	if (tmds_valid && sqofclk && tmdsclk_valid &&
		(tmds_align == 0x3f000000))
		return true;
	else {
		if (log_level & VIDEO_LOG) {
			rx_pr("tmds:%x,sqo:%x,tmdsclk_valid:%x,align:%x\n",
				tmds_valid, sqofclk, tmdsclk_valid, tmds_align);
			rx_pr("cable clk0:%d\n",
				rx_measure_clock(MEASURE_CLK_CABLE));
			rx_pr("cable clk1:%d\n",
				rx_get_clock(TOP_HDMI_CABLECLK));
		}
		return false;
	}
}

void rx_phy_rxsense_pulse(unsigned int t1, unsigned int t2, bool en)
{
	/* set rxsense pulse */
	hdmirx_phy_pddq(!en);
	mdelay(t1);
	hdmirx_phy_pddq(en);
	mdelay(t2);
}

void aml_phy_power_off(void)
{
	/* pll power down */
	wr_reg_hhi_bits(HHI_HDMIRX_APLL_CNTL0, _BIT(28), 0);
	wr_reg_hhi_bits(HHI_HDMIRX_APLL_CNTL0, _BIT(29), 1);

	/* phy power down */
	wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL0, 0x32037800);
	wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL1, 0x1000000);
	wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL2, 0x62208002);
	wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL3, 0x7);
	wr_reg_hhi(HHI_HDMIRX_PHY_DCHA_CNTL0, 0x1e);
	wr_reg_hhi(HHI_HDMIRX_PHY_DCHA_CNTL1, 0x10000800);
	wr_reg_hhi(HHI_HDMIRX_PHY_DCHD_CNTL0, 0x200000);
	wr_reg_hhi(HHI_HDMIRX_PHY_DCHD_CNTL1, 0x0);

	if (log_level & VIDEO_LOG)
		rx_pr("%s\n", __func__);
}

void rx_phy_power_on(unsigned int onoff)
{
	if (onoff)
		hdmirx_phy_pddq(0);
	else
		hdmirx_phy_pddq(1);
	if (rx.chip_id >= CHIP_ID_TL1) {
		/*the enable of these regs are in phy init*/
		if (onoff == 0) {
			aml_phy_power_off();
		}
	}
}

void rx_emp_to_ddr_init(void)
{
	unsigned int data;

	if (rx.chip_id < CHIP_ID_TL1)
		return;

	if (rx.empbuff.pg_addr) {
		rx_pr("rx_emp_to_ddr_init\n");
		/*disable field done and last pkt interrupt*/
		top_intr_maskn_value &= ~(1 << 25);
		top_intr_maskn_value &= ~(1 << 26);
		hdmirx_wr_top(TOP_INTR_MASKN, top_intr_maskn_value);

		if (rx.empbuff.p_addr_a) {
			/* emp int enable */
			/* config ddr buffer */
			hdmirx_wr_top(TOP_EMP_DDR_START_A,
				rx.empbuff.p_addr_a);
			hdmirx_wr_top(TOP_EMP_DDR_START_B,
				rx.empbuff.p_addr_b);
		}
		#if 0
		/* instead by clk tree */
		data  = 0;
		/*[10: 9] HDMIRX AXI clock mux select: fclk_div3=667MHz*/
		data |= (2 << 9);
		/*[    8] HDMIRX AXI clock enable*/
		data |= (1 << 8);
		/*[ 6: 0] HDMIRX AXI clock divider: 667/1=667MHz*/
		data |= (0 << 0);
		wr_reg_hhi(HHI_HDMIRX_AXI_CLK_CNTL, data);
		#endif
		/* enable store EMP pkt type */
		data = 0;
		data |= 0x1 << 15;/* ddr_store_emp */
		hdmirx_wr_top(TOP_EMP_DDR_FILTER, data);
		/* max pkt count */
		hdmirx_wr_top(TOP_EMP_CNTMAX, EMP_BUFF_MAX_PKT_CNT);

		data = 0;
		data |= 0xf << 16;/*[23:16] hs_beat_rate=0xf */
		data |= 0x0 << 14;/*[14] buffer_info_mode=0 */
		data |= 0x1 << 13;/*[13] reset_on_de=1 */
		data |= 0x1 << 12;/*[12] burst_end_on_last_emp=1 */
		data |= 0x0 << 2;/*[11:2] de_rise_delay=0 */
		data |= 0x0 << 0;/*[1:0] Endian = 0 */
		hdmirx_wr_top(TOP_EMP_CNTL_0, data);

		data = 0;
		data |= 0 << 1;/*ddr_mode[1] 0: emp 1: tmds*/
		hdmirx_wr_top(TOP_EMP_CNTL_1, data);

		data = 0;
		data |= 1;	/*ddr_en[0] 1:enable*/
		hdmirx_wr_top(TOP_EMP_CNTL_1, data);

		/* emp int enable TOP_INTR_MASKN*/
		/* emp field end done at DE rist bit[25]*/
		/* emp last EMP pkt recv done bit[26]*/
		/* disable emp irq */
		/* top_intr_maskn_value |= _BIT(25);
		 * hdmirx_wr_top(TOP_INTR_MASKN, top_intr_maskn_value);
		 */
	}

	rx.empbuff.ready = NULL;
	rx.empbuff.irqcnt = 0;
	rx.empbuff.emppktcnt = 0;
	rx.empbuff.tmdspktcnt = 720*480;
}

void rx_emp_field_done_irq(void)
{
	phys_addr_t p_addr;
	unsigned int recv_pkt_cnt, recv_byte_cnt, recv_pagenum;
	unsigned int emp_pkt_cnt = 0;
	unsigned char *src_addr = 0;
	unsigned char *dts_addr;
	unsigned int i, j, k;
	unsigned int datacnt = 0;
	struct page *cur_start_pg_addr;

	/*emp data start physical address*/
	p_addr = hdmirx_rd_top(TOP_EMP_DDR_PTR_S_BUF);

	/*buffer number*/
	recv_pkt_cnt = hdmirx_rd_top(TOP_EMP_RCV_CNT_BUF);
	recv_byte_cnt = recv_pkt_cnt * 32;
	recv_pagenum = (recv_byte_cnt >> PAGE_SHIFT) + 1;

	if (rx.empbuff.irqcnt & 0x1)
		dts_addr = rx.empbuff.store_b;
	else
		dts_addr = rx.empbuff.store_a;

	if (recv_pkt_cnt >= EMP_BUFF_MAX_PKT_CNT) {
		recv_pkt_cnt = EMP_BUFF_MAX_PKT_CNT - 1;
		rx_pr("pkt cnt err:%d\n", recv_pkt_cnt);
	}

	for (i = 0; i < recv_pagenum;) {
		/*one page 4k*/
		cur_start_pg_addr = phys_to_page(p_addr + i*PAGE_SIZE);
		src_addr = kmap_atomic(cur_start_pg_addr);
		dma_sync_single_for_cpu(hdmirx_dev, (p_addr + i*PAGE_SIZE),
			PAGE_SIZE, DMA_TO_DEVICE);
		if (recv_byte_cnt >= PAGE_SIZE) {
			for (j = 0; j < PAGE_SIZE;) {
				if (src_addr[j] == 0x7f) {
					emp_pkt_cnt++;
					/*32 bytes per emp pkt*/
					for (k = 0; k < 32; k++) {
						dts_addr[datacnt] =
						src_addr[PAGE_SIZE * i + j + k];
						datacnt++;
					}
				}
				j += 32;
			}
			recv_byte_cnt -= PAGE_SIZE;
		} else {
			for (j = 0; j < recv_byte_cnt;) {
				if (src_addr[j] == 0x7f) {
					emp_pkt_cnt++;
					/*32 bytes per emp pkt*/
					for (k = 0; k < 32; k++) {
						dts_addr[datacnt] =
						src_addr[PAGE_SIZE * i + j + k];
						datacnt++;
					}
				}
				j += 32;
			}
		}
		/*release*/
		/*__kunmap_atomic(src_addr);*/
		kunmap_atomic(src_addr);
		i++;
	}

	/*ready address*/
	rx.empbuff.ready = dts_addr;
	/*ready pkt cnt*/
	rx.empbuff.emppktcnt = emp_pkt_cnt;
	/*emp field dont irq counter*/
	rx.empbuff.irqcnt++;
}

void rx_emp_status(void)
{
	rx_pr("p_addr_a=0x%p\n", (void *)rx.empbuff.p_addr_a);
	rx_pr("p_addr_b=0x%p\n", (void *)rx.empbuff.p_addr_b);
	rx_pr("store_a=0x%p\n", rx.empbuff.store_a);
	rx_pr("store_b=0x%p\n", rx.empbuff.store_b);
	rx_pr("irq cnt =0x%x\n", (unsigned int)rx.empbuff.irqcnt);
	rx_pr("ready=0x%p\n", rx.empbuff.ready);
	rx_pr("dump_mode =0x%x\n", rx.empbuff.dump_mode);
	rx_pr("recv tmp pkt cnt=0x%x\n", rx.empbuff.emppktcnt);
	rx_pr("recv tmds pkt cnt=0x%x\n", rx.empbuff.tmdspktcnt);
}

void rx_tmds_to_ddr_init(void)
{
	unsigned int data, data2;
	unsigned int i = 0;

	if (rx.chip_id < CHIP_ID_TL1)
		return;

	if (rx.empbuff.pg_addr) {
		rx_pr("rx_tmds_to_ddr_init\n");
		/*disable field done and last pkt interrupt*/
		top_intr_maskn_value &= ~(1 << 25);
		top_intr_maskn_value &= ~(1 << 26);
		hdmirx_wr_top(TOP_INTR_MASKN, top_intr_maskn_value);

		/* disable emp rev */
		data = hdmirx_rd_top(TOP_EMP_CNTL_1);
		data &= ~0x1;
		hdmirx_wr_top(TOP_EMP_CNTL_1, data);
		/* wait until emp finish */
		data2 = hdmirx_rd_top(TOP_EMP_STAT_0) & 0x7fffffff;
		data = hdmirx_rd_top(TOP_EMP_STAT_1);
		while (data2 || data) {
			mdelay(1);
			data2 = hdmirx_rd_top(TOP_EMP_STAT_0) & 0x7fffffff;
			data = hdmirx_rd_top(TOP_EMP_STAT_1);
			if (i++ > 100) {
				rx_pr("warning: wait emp timeout\n");
				break;
			}
		}
		if (rx.empbuff.p_addr_a) {
			/* config ddr buffer */
			hdmirx_wr_top(TOP_EMP_DDR_START_A,
				rx.empbuff.p_addr_a);
			hdmirx_wr_top(TOP_EMP_DDR_START_B,
				rx.empbuff.p_addr_a);
			rx_pr("cfg hw addr=0x%p\n",
			      (void *)rx.empbuff.p_addr_a);
		}

		/* max pkt count to avoid buffer overflow */
		/* one pixel 4bytes */
		data = ((rx.empbuff.tmdspktcnt/8) * 8) - 1;
		hdmirx_wr_top(TOP_EMP_CNTMAX, data);
		rx_pr("pkt max cnt limit=0x%x\n", data);

		data = 0;
		data |= 0x0 << 16;/*[23:16] hs_beat_rate=0xf */
		data |= 0x1 << 14;/*[14] buffer_info_mode=0 */
		data |= 0x1 << 13;/*[13] reset_on_de=1 */
		data |= 0x0 << 12;/*[12] burst_end_on_last_emp=1 */
		data |= 0x0 << 2;/*[11:2] de_rise_delay=0 */
		data |= 0x0 << 0;/*[1:0] Endian = 0 */
		hdmirx_wr_top(TOP_EMP_CNTL_0, data);

		/* working mode: tmds data to ddr enable */
		data = hdmirx_rd_top(TOP_EMP_CNTL_1);
		data |= 0x1 << 1;/*ddr_mode[1] 0: emp 1: tmds*/
		hdmirx_wr_top(TOP_EMP_CNTL_1, data);

		/* emp int enable TOP_INTR_MASKN*/
		/* emp field end done at DE rist bit[25]*/
		/* emp last EMP pkt recv done bit[26]*/
		top_intr_maskn_value |= _BIT(26);
		hdmirx_wr_top(TOP_INTR_MASKN, top_intr_maskn_value);

		/*start record*/
		data |= 0x1;	/*ddr_en[0] 1:enable*/
		hdmirx_wr_top(TOP_EMP_CNTL_1, data);
	}
}

void rx_emp_lastpkt_done_irq(void)
{
	unsigned int data;

	/* disable record */
	data = hdmirx_rd_top(TOP_EMP_CNTL_1);
	data &= ~0x1;	/*ddr_en[0] 1:enable*/
	hdmirx_wr_top(TOP_EMP_CNTL_1, data);

	/*need capture data*/

	rx_pr(">> lastpkt_done_irq >\n");
}

void rx_emp_capture_stop(void)
{
	unsigned int i = 0, data, data2;

	/*disable field done and last pkt interrupt*/
	top_intr_maskn_value &= ~(1 << 25);
	top_intr_maskn_value &= ~(1 << 26);
	hdmirx_wr_top(TOP_INTR_MASKN, top_intr_maskn_value);

	/* disable emp rev */
	data = hdmirx_rd_top(TOP_EMP_CNTL_1);
	data &= ~0x1;
	hdmirx_wr_top(TOP_EMP_CNTL_1, data);
	/* wait until emp finish */
	data2 = hdmirx_rd_top(TOP_EMP_STAT_0) & 0x7fffffff;
	data = hdmirx_rd_top(TOP_EMP_STAT_1);
	while (data2 || data) {
		mdelay(1);
		data2 = hdmirx_rd_top(TOP_EMP_STAT_0) & 0x7fffffff;
		data = hdmirx_rd_top(TOP_EMP_STAT_1);
		if (i++ > 100) {
			rx_pr("warning: wait emp timeout\n");
			break;
		}
	}
	rx_pr("emp capture stop\n");
}


/*
 * get hdmi data error counter
 * for tl1
 * return:
 * ch0 , ch1 , ch2 error counter value
 */
void rx_get_error_cnt(uint32_t *ch0, uint32_t *ch1,
							uint32_t *ch2)
{
	uint32_t val;

	val = hdmirx_rd_top(TOP_CHAN01_ERRCNT);
	*ch0 = val & 0xffff;
	*ch1 = (val >> 16) & 0xffff;
	val = hdmirx_rd_top(TOP_CHAN2_ERRCNT);
	*ch2 = val & 0xffff;
}

/*
 * get hdmi audio N CTS
 * for tl1
 * return:
 * audio ACR N
 * audio ACR CTS
 */
void rx_get_audio_N_CTS(uint32_t *N, uint32_t *CTS)
{
	*N = hdmirx_rd_top(TOP_ACR_N_STAT);
	*CTS = hdmirx_rd_top(TOP_ACR_CTS_STAT);
}

/* termination calibration */
void rx_phy_rt_cal(void)
{
	int i = 0, j = 0;
	u32 x_val[100][2];
	u32 temp;
	int val_cnt = 1;

	rx_pr("360=0x%x\n", rd_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL1));
	for (; i < 100; i++) {
		wr_reg_hhi_bits(HHI_HDMIRX_PHY_MISC_CNTL0, MISCI_COMMON_RST, 0);
		wr_reg_hhi_bits(HHI_HDMIRX_PHY_MISC_CNTL0, MISCI_COMMON_RST, 1);
		udelay(1);
		temp = (rd_reg_hhi(HHI_HDMIRX_PHY_MISC_STAT) >> 1) & 0x3ff;
		rx_pr("temp=%x\n", temp);
		if (i == 0) {
			x_val[0][0] = temp;
			x_val[0][1] = 1;
		}

		for (; j < i; j++) {
			rx_pr("j=%d\n", j);
			if (temp == x_val[j][0]) {
				x_val[j][1]	+= 1;
				rx_pr("++,val=%x\n", x_val[j][0]);
				goto todo;
			}
		}
todo:
		if (j == (val_cnt + 1)) {
			x_val[j][0] = temp;
			x_val[j][1] = 1;
			val_cnt++;
			rx_pr("new\n");
		}
		rx_pr("x_val=0x%x,cnt=%d,val_cnt=%d\n",
		      x_val[j][0], x_val[j][1], val_cnt);

		if (x_val[j][1] == 10) {
			term_cal_val = (~((x_val[j][0]) << 1)) & 0x3ff;
			for (; j < val_cnt; j++)
				rx_pr("val=%x,cnt=%d\n",
				      x_val[j][0], x_val[j][1]);
			return;
		}
		j = 0;
	}
}
