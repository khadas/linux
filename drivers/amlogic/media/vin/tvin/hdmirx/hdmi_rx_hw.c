// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/arm-smccc.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>
#include <linux/amlogic/clk_measure.h>

/* Local include */
#include "hdmi_rx_eq.h"
#include "hdmi_rx_repeater.h"
#include "hdmi_rx_drv.h"
#include "hdmi_rx_hw.h"
#include "hdmi_rx_edid.h"
#include "hdmi_rx_wrapper.h"
#include "hdmi_rx_pktinfo.h"
#include "hdmi_rx_hw_t5m.h"
#include "hdmi_rx_hw_t3x.h"
#include "hdmi_rx_hw_t5.h"
#include "hdmi_rx_hw_t7.h"
#include "hdmi_rx_hw_tl1.h"
#include "hdmi_rx_hw_tm2.h"

/*------------------------marco define------------------------------*/
#define SCRAMBLE_SEL 1
#define HYST_HDMI_TO_DVI 5
/* must = 0, other agilent source fail */
#define HYST_DVI_TO_HDMI 0
#define GCP_GLOB_AVMUTE_EN 1 /* ag506 must clear this bit */
#define EDID_CLK_DIV 9 /* sys clk/(9+1) = 20M */
#define HDCP_KEY_WR_TRIES		(5)

/*------------------------variable define------------------------------*/
static DEFINE_SPINLOCK(reg_rw_lock);
/* should enable fast switching, since some devices in non-current port */
/* will suspend because of RxSense = 0, such as xiaomi-mtk box */
static bool phy_fast_switching;
static bool phy_fsm_enhancement = true;
/*unsigned int last_clk_rate;*/
static u32 modet_clk = 24000;
int hdcp_enc_mode;
/* top_irq_en bit[16:13] hdcp_sts */
/* bit27 DE rise */
int top_intr_maskn_value;
u32 afifo_overflow_cnt;
u32 afifo_underflow_cnt;
int rx_afifo_dbg_en;
bool hdcp_enable = 1;
int acr_mode;
int auto_aclk_mute = 2;
int aud_avmute_en = 1;
int aud_mute_sel = 2;
int force_clk_rate = 1;
u32 rx_ecc_err_thres = 100;
u32 rx_ecc_err_frames = 5;
int md_ists_en = VIDEO_MODE;
int pdec_ists_en;/* = AVI_CKS_CHG | DVIDET | DRM_CKS_CHG | DRM_RCV_EN;*/
u32 packet_fifo_cfg;
int pd_fifo_start_cnt = 0x8;
/* Controls equalizer reference voltage. */
int hdcp22_on;
MODULE_PARM_DESC(hdcp22_on, "\n hdcp22_on\n");
module_param(hdcp22_on, int, 0664);

/* 0: previous hdcp_rx22 ,1: new hdcp_rx22 */
int rx22_ver;
MODULE_PARM_DESC(rx22_ver, "\n rx22_ver\n");
module_param(rx22_ver, int, 0664);

MODULE_PARM_DESC(force_clk_rate, "\n force_clk_rate\n");
module_param(force_clk_rate, int, 0664);

/* test for HBR CTS, audio module can set it to force 8ch */
int hbr_force_8ch;
/*
 * hdcp14_key_mode:hdcp1.4 key handle method select
 * NORMAL_MODE:systemcontrol path
 * SECURE_MODE:secure OS path
 */
int hdcp14_key_mode = NORMAL_MODE;
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
u32 hdcp_hpd_ctrl_en;
int eq_dbg_lvl;
u32 phy_trim_val;
/* bit'4: tdr enable
 * bit [3:0]: tdr level control
 */
int phy_term_lel;
bool phy_tdr_en;
int hdcp_tee_path;
int kill_esm_fail;

/* emp buffer */
char emp_buf[1024];
char pre_emp_buf[1024];
int i2c_err_cnt;
u32 ddc_dbg_en;
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

	if (!rx_get_dig_clk_en_sts())
		return 0;
	if (rx.chip_id >= CHIP_ID_TL1) {
		spin_lock_irqsave(&reg_rw_lock, flags);
		data = rd_reg(MAP_ADDR_MODULE_TOP,
			      addr + rx_reg_maps[MAP_ADDR_MODULE_TOP].phy_addr);
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
 * hdmirx_rd_bits_dwc - read specified bits of HDMI RX CTRL reg
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

	if (!rx_get_dig_clk_en_sts())
		return;
	if (rx.chip_id >= CHIP_ID_TL1) {
		spin_lock_irqsave(&reg_rw_lock, flags);
		wr_reg(MAP_ADDR_MODULE_TOP,
		       addr + rx_reg_maps[MAP_ADDR_MODULE_TOP].phy_addr, data);
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
 * hdmirx_wr_bits_dwc - write specified bits of HDMI RX CTRL reg
 * @addr: register address
 * @mask: bits mask
 * @value: new register value
 */
void hdmirx_wr_bits_dwc(unsigned int addr,
			unsigned int mask,
			unsigned int value)
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
			if (hdmirx_rd_dwc(DWC_HDMI_ISTS) & (1 << 28)) {
				hdmirx_wr_dwc(DWC_HDMI_ICLR, 1 << 28);
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
 * hdmirx_rd_bits_phy - read specified bits of HDMI RX phy reg
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
			if (hdmirx_rd_dwc(DWC_HDMI_ISTS) & (1 << 28)) {
				hdmirx_wr_dwc(DWC_HDMI_ICLR, 1 << 28);
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
 * hdmirx_wr_bits_phy - write specified bits of HDMI RX phy reg
 * @addr: register address
 * @mask: bits mask
 * @value: new register value
 *
 * return 0 on write succeed, return -1 otherwise.
 */
int hdmirx_wr_bits_phy(u16 addr, u32 mask, u32 value)
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
		if (rx.chip_id >= CHIP_ID_T7)
			dev_offset = rx_reg_maps[MAP_ADDR_MODULE_TOP].phy_addr;
		else
			dev_offset = TOP_DWC_BASE_OFFSET +
				rx_reg_maps[MAP_ADDR_MODULE_TOP].phy_addr;
		if (addr >= TOP_EDID_ADDR_S &&
		    addr <= (TOP_EDID_PORT3_ADDR_E)) {
			data = rd_reg_b(MAP_ADDR_MODULE_TOP,
					dev_offset + addr);
		} else {
			data = rd_reg(MAP_ADDR_MODULE_TOP,
				      dev_offset + (addr << 2));
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
u32 hdmirx_rd_bits_top(u16 addr, u32 mask)
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
		if (rx.chip_id >= CHIP_ID_T7)
			dev_offset = rx_reg_maps[MAP_ADDR_MODULE_TOP].phy_addr;
		else
			dev_offset = TOP_DWC_BASE_OFFSET +
				rx_reg_maps[MAP_ADDR_MODULE_TOP].phy_addr;
		if (addr >= TOP_EDID_ADDR_S &&
		    addr <= (TOP_EDID_PORT3_ADDR_E)) {
			wr_reg_b(MAP_ADDR_MODULE_TOP,
				 dev_offset + addr, (unsigned char)data);
		} else {
			wr_reg(MAP_ADDR_MODULE_TOP,
			       dev_offset + (addr << 2), data);
		}
		spin_unlock_irqrestore(&reg_rw_lock, flags);
	} else {
		spin_lock_irqsave(&reg_rw_lock, flags);
		wr_reg(MAP_ADDR_MODULE_TOP, hdmirx_addr_port | dev_offset, addr);
		wr_reg(MAP_ADDR_MODULE_TOP, hdmirx_data_port | dev_offset, data);
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
			unsigned int mask,
			unsigned int value)
{
	hdmirx_wr_top(addr, rx_set_bits(hdmirx_rd_top(addr), mask, value));
}

/*
 * hdmirx_rd_amlphy - read hdmirx amlphy reg
 * @addr: register address
 *
 * return data read value
 */
unsigned int hdmirx_rd_amlphy(unsigned int addr)
{
	ulong flags;
	int data;
	unsigned int dev_offset = 0;
	u32 base_ofst = 0;

	if (rx.chip_id >= CHIP_ID_T7)
		base_ofst = TOP_AMLPHY_BASE_OFFSET_T7;
	else
		base_ofst = TOP_AMLPHY_BASE_OFFSET_T5;
	spin_lock_irqsave(&reg_rw_lock, flags);
	dev_offset = base_ofst +
		rx_reg_maps[MAP_ADDR_MODULE_TOP].phy_addr;
	data = rd_reg(MAP_ADDR_MODULE_TOP, dev_offset + addr);
	spin_unlock_irqrestore(&reg_rw_lock, flags);
	return data;
}

/*
 * hdmirx_rd_bits_amlphy - read specified bits of hdmirx amlphy reg
 * @addr: register address
 * @mask: bits mask
 *
 * return masked bits of register value
 */
u32 hdmirx_rd_bits_amlphy(u16 addr, u32 mask)
{
	return rx_get_bits(hdmirx_rd_amlphy(addr), mask);
}

/*
 * hdmirx_wr_amlphy - Write data to hdmirx amlphy reg
 * @addr: register address
 * @data: new register value
 */
void hdmirx_wr_amlphy(unsigned int addr, unsigned int data)
{
	ulong flags;
	unsigned long dev_offset = 0;
	u32 base_ofst = 0;

	if (rx.chip_id >= CHIP_ID_T7)
		base_ofst = TOP_AMLPHY_BASE_OFFSET_T7;
	else
		base_ofst = TOP_AMLPHY_BASE_OFFSET_T5;
	spin_lock_irqsave(&reg_rw_lock, flags);
	dev_offset = base_ofst +
		rx_reg_maps[MAP_ADDR_MODULE_TOP].phy_addr;
		wr_reg(MAP_ADDR_MODULE_TOP, dev_offset + addr, data);
	spin_unlock_irqrestore(&reg_rw_lock, flags);
}

/*
 * hdmirx_wr_bits_amlphy - write specified bits of hdmirx amlphy reg
 * @addr: register address
 * @mask: bits mask
 * @value: new register value
 */
void hdmirx_wr_bits_amlphy(unsigned int addr,
			   unsigned int mask,
			   unsigned int value)
{
	hdmirx_wr_amlphy(addr, rx_set_bits(hdmirx_rd_amlphy(addr), mask, value));
}

/* for T7 */
u8 hdmirx_rd_cor(u32 addr)
{
	ulong flags;
	u8 data;
	u32 dev_offset = 0;
	bool need_wr_twice = false;

	/* addr bit[8:15] is 0x1d or 0x1e need write twice */
	need_wr_twice = ((((addr >> 8) & 0xff) == 0x1d) ||
		(((addr >> 8) & 0xff) == 0x1e));
	dev_offset = TOP_COR_BASE_OFFSET_T7 +
		rx_reg_maps[MAP_ADDR_MODULE_TOP].phy_addr;
	spin_lock_irqsave(&reg_rw_lock, flags);
	data = rd_reg_b(MAP_ADDR_MODULE_TOP,
		      addr + dev_offset);
	if (need_wr_twice)
		data = rd_reg_b(MAP_ADDR_MODULE_TOP,
		      addr + dev_offset);
	spin_unlock_irqrestore(&reg_rw_lock, flags);

	return data;
}

u8 hdmirx_rd_bits_cor(u32 addr, u32 mask)
{
	return rx_get_bits(hdmirx_rd_cor(addr), mask);
}

void hdmirx_wr_cor(u32 addr, u8 data)
{
	ulong flags;
	u32 dev_offset = 0;
	bool need_wr_twice = false;

	/* addr bit[8:15] is 0x1d or 0x1e need write twice */
	need_wr_twice = ((((addr >> 8) & 0xff) == 0x1d) ||
		(((addr >> 8) & 0xff) == 0x1e));
	dev_offset = TOP_COR_BASE_OFFSET_T7 +
		rx_reg_maps[MAP_ADDR_MODULE_TOP].phy_addr;
	spin_lock_irqsave(&reg_rw_lock, flags);
	wr_reg_b(MAP_ADDR_MODULE_TOP,
	       addr + dev_offset, data);
	if (need_wr_twice)
		wr_reg_b(MAP_ADDR_MODULE_TOP,
			addr + dev_offset, data);
	spin_unlock_irqrestore(&reg_rw_lock, flags);
}

void hdmirx_wr_bits_cor(u32 addr, u32 mask, u8 value)
{
	hdmirx_wr_cor(addr, rx_set_bits(hdmirx_rd_cor(addr), mask, value));
}

unsigned int rd_reg_clk_ctl(unsigned int offset)
{
	unsigned int ret;
	unsigned long flags;
	unsigned int addr;

	spin_lock_irqsave(&reg_rw_lock, flags);
	addr = offset + rx_reg_maps[MAP_ADDR_MODULE_CLK_CTRL].phy_addr;
	ret = rd_reg(MAP_ADDR_MODULE_CLK_CTRL, addr);
	spin_unlock_irqrestore(&reg_rw_lock, flags);
	return ret;
}

u32 hdmirx_rd_bits_clk_ctl(u32 addr, u32 mask)
{
	return rx_get_bits(rd_reg_clk_ctl(addr), mask);
}

void wr_reg_clk_ctl(unsigned int offset, unsigned int val)
{
	unsigned long flags;
	unsigned int addr;

	spin_lock_irqsave(&reg_rw_lock, flags);
	addr = offset + rx_reg_maps[MAP_ADDR_MODULE_CLK_CTRL].phy_addr;
	wr_reg(MAP_ADDR_MODULE_CLK_CTRL, addr, val);
	spin_unlock_irqrestore(&reg_rw_lock, flags);
}

void hdmirx_wr_bits_clk_ctl(unsigned int addr, unsigned int mask, unsigned int value)
{
	wr_reg_clk_ctl(addr, rx_set_bits(rd_reg_clk_ctl(addr), mask, value));
}

/* For analog modules register rd */
unsigned int rd_reg_ana_ctl(unsigned int offset)
{
	unsigned int ret;
	unsigned long flags;
	unsigned int addr;

	spin_lock_irqsave(&reg_rw_lock, flags);
	addr = offset + rx_reg_maps[MAP_ADDR_MODULE_ANA_CTRL].phy_addr;
	ret = rd_reg(MAP_ADDR_MODULE_ANA_CTRL, addr);
	spin_unlock_irqrestore(&reg_rw_lock, flags);
	return ret;
}

/* For analog modules register wr */
void wr_reg_ana_ctl(unsigned int offset, unsigned int val)
{
	unsigned long flags;
	unsigned int addr;

	spin_lock_irqsave(&reg_rw_lock, flags);
	addr = offset + rx_reg_maps[MAP_ADDR_MODULE_ANA_CTRL].phy_addr;
	wr_reg(MAP_ADDR_MODULE_ANA_CTRL, addr, val);
	spin_unlock_irqrestore(&reg_rw_lock, flags);
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
	addr = offset + rx_reg_maps[MAP_ADDR_MODULE_HIU].phy_addr;
	ret = rd_reg(MAP_ADDR_MODULE_HIU, addr);
	spin_unlock_irqrestore(&reg_rw_lock, flags);
	return ret;
}

/*
 * rd_reg_hhi_bits - read specified bits of HHI reg
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
	addr = offset + rx_reg_maps[MAP_ADDR_MODULE_HIU].phy_addr;
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
 * rd_reg - register read
 * @module: module index of the reg_map table
 * @reg_addr: offset address of specified phy addr
 *
 * returns unsigned int bytes read from the addr
 */
unsigned int rd_reg(enum map_addr_module_e module,
		    unsigned int reg_addr)
{
	unsigned int val = 0;

	if (module < MAP_ADDR_MODULE_NUM && check_regmap_flag(reg_addr))
		val = readl(rx_reg_maps[module].p +
			    (reg_addr - rx_reg_maps[module].phy_addr));
	else
		rx_pr("rd reg %x error,md %d\n", reg_addr, module);
	return val;
}

/*
 * wr_reg - register write
 * @module: module index of the reg_map table
 * @reg_addr: offset address of specified phy addr
 * @val: value being written
 */
void wr_reg(enum map_addr_module_e module,
	    unsigned int reg_addr, unsigned int val)
{
	if (module < MAP_ADDR_MODULE_NUM && check_regmap_flag(reg_addr))
		writel(val, rx_reg_maps[module].p +
		       (reg_addr - rx_reg_maps[module].phy_addr));
	else
		rx_pr("wr reg %x err\n", reg_addr);
}

/*
 * rd_reg_b - register read byte mode
 * @module: module index of the reg_map table
 * @reg_addr: offset address of specified phy addr
 *
 * returns unsigned char bytes read from the addr
 */
unsigned char rd_reg_b(enum map_addr_module_e module,
		       unsigned int reg_addr)
{
	unsigned char val = 0;

	if (module < MAP_ADDR_MODULE_NUM && check_regmap_flag(reg_addr))
		val = readb(rx_reg_maps[module].p +
			    (reg_addr - rx_reg_maps[module].phy_addr));
	else
		rx_pr("rd reg %x error,md %d\n", reg_addr, module);
	return val;
}

/*
 * wr_reg_b - register write byte mode
 * @module: module index of the reg_map table
 * @reg_addr: offset address of specified phy addr
 * @val: value being written
 */
void wr_reg_b(enum map_addr_module_e module,
	      unsigned int reg_addr, unsigned char val)
{
	if (module < MAP_ADDR_MODULE_NUM && check_regmap_flag(reg_addr))
		writeb(val, rx_reg_maps[module].p +
		       (reg_addr - rx_reg_maps[module].phy_addr));
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
	       rx_reg_maps[MAP_ADDR_MODULE_HDMIRX_CAPB3].phy_addr | addr,
	data);
	spin_unlock_irqrestore(&reg_rw_lock, flags);
}

unsigned int rx_hdcp22_rd(unsigned int addr)
{
	unsigned int data;
	unsigned long flags;

	spin_lock_irqsave(&reg_rw_lock, flags);
	data = rd_reg(MAP_ADDR_MODULE_HDMIRX_CAPB3,
		      rx_reg_maps[MAP_ADDR_MODULE_HDMIRX_CAPB3].phy_addr | addr);
	spin_unlock_irqrestore(&reg_rw_lock, flags);
	return data;
}

void rx_hdcp22_rd_check(unsigned int addr,
			unsigned int exp_data,
			unsigned int mask)
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
		(rx_reg_maps[MAP_ADDR_MODULE_HDMIRX_CAPB3].phy_addr + addr),
		data);
}

/*
 * rx_hdcp22_rd_reg - hdcp2.2 reg read
 * @addr: register address
 */
unsigned int rx_hdcp22_rd_reg(unsigned int addr)
{
	return (u32)rx_sec_reg_read((unsigned int *)(unsigned long)
		(rx_reg_maps[MAP_ADDR_MODULE_HDMIRX_CAPB3].phy_addr + addr));
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
			   unsigned int mask,
			   unsigned int value)
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
unsigned int rx_hdcp22_rd_top(u32 addr)
{
	if (rx.chip_id >= CHIP_ID_T7)
		return 0;
	return (unsigned int)sec_top_read((unsigned int *)(unsigned long)addr);
}

/*
 * sec_top_write - secure top write
 */
void sec_top_write(unsigned int *addr, unsigned int value)
{
	struct arm_smccc_res res;

	if (rx.chip_id >= CHIP_ID_T7)
		arm_smccc_smc(HDMIRX_WR_SEC_TOP_NEW, (unsigned long)(uintptr_t)addr,
		      value, 0, 0, 0, 0, 0, &res);
	else
		arm_smccc_smc(HDMIRX_WR_SEC_TOP, (unsigned long)(uintptr_t)addr,
		      value, 0, 0, 0, 0, 0, &res);
}

/*
 * sec_top_read - secure top read
 */
unsigned int sec_top_read(unsigned int *addr)
{
	struct arm_smccc_res res;

	if (rx.chip_id >= CHIP_ID_T7)
		return 0;
	else
		arm_smccc_smc(HDMIRX_RD_SEC_TOP, (unsigned long)(uintptr_t)addr,
			      0, 0, 0, 0, 0, 0, &res);

	return (unsigned int)((res.a0) & 0xffffffff);
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

	return (unsigned int)((res.a0) & 0xffffffff);
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

	return (unsigned int)((res.a0) & 0xffffffff);
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

	return (unsigned int)((res.a0) & 0xffffffff);
}

/*
 * rx_smc_cmd_handler: communicate with bl31
 */
u32 rx_smc_cmd_handler(u32 index, u32 value)
{
	struct arm_smccc_res res;

	arm_smccc_smc(HDMI_RX_SMC_CMD, index,
				value, 0, 0, 0, 0, 0, &res);
	return (unsigned int)((res.a0) & 0xffffffff);
}

void hdmirx_phy_pddq_tl1_tm2(unsigned int enable, unsigned int term_val)
{
	wr_reg_hhi_bits(HHI_HDMIRX_PHY_MISC_CNTL2,
			_BIT(1), !enable);
	/* set rxsense */
	if (enable)
		wr_reg_hhi_bits(HHI_HDMIRX_PHY_MISC_CNTL0,
				MSK(3, 0), 0);
	else
		wr_reg_hhi_bits(HHI_HDMIRX_PHY_MISC_CNTL0,
				MSK(3, 0), term_val);
}

void hdmirx_phy_pddq_t5(unsigned int enable, unsigned int term_val)
{
	hdmirx_wr_bits_amlphy(T5_HHI_RX_PHY_MISC_CNTL2,
			      _BIT(1), !enable);
	/* set rxsense */
	if (enable)
		hdmirx_wr_bits_amlphy(T5_HHI_RX_PHY_MISC_CNTL0,
				      MSK(3, 0), 0);
	else
		hdmirx_wr_bits_amlphy(T5_HHI_RX_PHY_MISC_CNTL0,
				      MSK(3, 0), term_val);
}

void hdmirx_phy_pddq_snps(unsigned int enable)
{
	hdmirx_wr_bits_dwc(DWC_SNPS_PHYG3_CTRL, MSK(1, 1), enable);
}

/*
 * hdmirx_phy_pddq - phy pddq config
 * @enable: enable phy pddq up
 */
void hdmirx_phy_pddq(unsigned int enable)
{
	u32 term_value = hdmirx_rd_top(TOP_HPD_PWR5V) & 0x7;

	if (rx.chip_id >= CHIP_ID_TL1 &&
	    rx.chip_id <= CHIP_ID_TM2) {
		hdmirx_phy_pddq_tl1_tm2(enable, term_value);
	} else if (rx.chip_id >= CHIP_ID_T5) {
		hdmirx_phy_pddq_t5(enable, term_value);
	} else {
		hdmirx_wr_bits_dwc(DWC_SNPS_PHYG3_CTRL, MSK(1, 1), enable);
	}
}

/*
 * hdmirx_wr_ctl_port
 */
void hdmirx_wr_ctl_port(unsigned int offset, unsigned int data)
{
	unsigned long flags;

	if (rx.chip_id < CHIP_ID_TL1) {
		spin_lock_irqsave(&reg_rw_lock, flags);
		wr_reg(MAP_ADDR_MODULE_TOP, hdmirx_ctrl_port + offset, data);
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
	if (rx.chip_id >= CHIP_ID_TL1 &&
	    rx.chip_id <= CHIP_ID_T5D) {
		dev_offset = TOP_DWC_BASE_OFFSET +
			rx_reg_maps[MAP_ADDR_MODULE_TOP].phy_addr;
		wr_reg(MAP_ADDR_MODULE_TOP, dev_offset + TOP_SW_RESET, 1);
		udelay(1);
		wr_reg(MAP_ADDR_MODULE_TOP, dev_offset + TOP_SW_RESET, 0);
	} else if (rx.chip_id >= CHIP_ID_T7) {
		//dev_offset = rx_reg_maps[MAP_ADDR_MODULE_TOP].phy_addr;
		//wr_reg(MAP_ADDR_MODULE_TOP, dev_offset + TOP_SW_RESET, 1);
		//udelay(1);
		//wr_reg(MAP_ADDR_MODULE_TOP, dev_offset + TOP_SW_RESET, 0);
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
void rx_set_irq_t5(bool enable)
{
	unsigned int data32 = 0;
	if (enable) {
		if (rx.chip_id >= CHIP_ID_TL1) {
			data32 |= 1 << 31; /* DRC_CKS_CHG */
			data32 |= 1 << 30; /* DRC_RCV */
			data32 |= 0 << 29; /* AUD_TYPE_CHG */
			data32 |= 0 << 28; /* DVI_DET */
			data32 |= 0 << 27; /* VSI_CKS_CHG */
			data32 |= 0 << 26; /* GMD_CKS_CHG */
			data32 |= 0 << 25; /* AIF_CKS_CHG */
			data32 |= 1 << 24; /* AVI_CKS_CHG */
			data32 |= 0 << 23; /* ACR_N_CHG */
			data32 |= 0 << 22; /* ACR_CTS_CHG */
			data32 |= 0 << 21; /* GCP_AV_MUTE_CHG */
			data32 |= 0 << 20; /* GMD_RCV */
			data32 |= 0 << 19; /* AIF_RCV */
			data32 |= 1 << 18; /* AVI_RCV */
			data32 |= 0 << 17; /* ACR_RCV */
			data32 |= 0 << 16; /* GCP_RCV */
		#ifdef VSIF_PKT_READ_FROM_PD_FIFO
			data32 |= 0 << 15; /* VSI_RCV */
		#else
			data32 |= 0 << 15; /* VSI_RCV */
		#endif
			data32 |= 0 << 14; /* AMP_RCV */
			data32 |= 0 << 13; /* AMP_CHG */
			data32 |= 1 << 9; /* EMP_RCV*/
			data32 |= 0 << 8; /* PD_FIFO_NEW_ENTRY */
		#ifdef VSIF_PKT_READ_FROM_PD_FIFO
			data32 |= 1 << 4; /* PD_FIFO_OVERFL */
			data32 |= 1 << 3; /* PD_FIFO_UNDERFL */
		#else
			data32 |= 0 << 4; /* PD_FIFO_OVERFL */
			data32 |= 0 << 3; /* PD_FIFO_UNDERFL */
		#endif
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
			data32 |= 0 << 21; /* GCP_AV_MUTE_CHG */
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
			data32 |= 0 << 21; /* GCP_AV_MUTE_CHG */
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
			data32 |= 0 << 21; /* GCP_AV_MUTE_CHG */
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
		/* clear status */
		hdmirx_wr_dwc(DWC_PDEC_ICLR, ~0);
		hdmirx_wr_dwc(DWC_AUD_CEC_ICLR, ~0);
		hdmirx_wr_dwc(DWC_AUD_FIFO_ICLR, ~0);
		hdmirx_wr_dwc(DWC_MD_ICLR, ~0);
		hdmirx_wr_dwc(DWC_PDEC_IEN_SET, data32);
		hdmirx_wr_dwc(DWC_AUD_FIFO_IEN_SET, OVERFL | UNDERFL);
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
 * rx_irq_en - hdmirx controller irq config
 * @enable - irq set or clear
 */
void rx_irq_en(bool enable)
{
	if (rx.chip_id >= CHIP_ID_T7)
		rx_set_irq_t7(enable);
	else
		rx_set_irq_t5(enable);
}

/*
 * hdmirx_irq_hdcp_enable - hdcp irq enable
 */
void hdmirx_irq_hdcp_enable(bool enable)
{
	if (rx.chip_id >= CHIP_ID_T7) {
		if (enable) {
			/* encrypted sts changed en */
			//hdmirx_wr_cor(RX_HDCP1X_INTR0_MASK_HDCP1X_IVCRX, 1);
			/* AKE init received en */
			hdmirx_wr_cor(CP2PAX_INTR1_MASK_HDCP2X_IVCRX, 0x6);
		} else {
			/* clear enable */
			hdmirx_wr_cor(RX_HDCP1X_INTR0_MASK_HDCP1X_IVCRX, 0);
			/* clear status */
			hdmirx_wr_cor(RX_HDCP1X_INTR0_HDCP1X_IVCRX, 0xff);
			/* clear enable */
			hdmirx_wr_cor(CP2PAX_INTR1_MASK_HDCP2X_IVCRX, 0);
			/* clear status */
			hdmirx_wr_cor(CP2PAX_INTR1_HDCP2X_IVCRX, 0xff);
		}
	} else {
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
}

void hdmirx_top_irq_en(int en, int lvl)
{
	u32 data32;

	if (rx.chip_id >= CHIP_ID_T7) {
		data32  = 0;
		data32 |= (0    << 30); // [   30] aud_chg;
		data32 |= (0    << 29); // [   29] hdmirx_sqof_clk_fall;
		data32 |= (0    << 28); // [   28] hdmirx_sqof_clk_rise;
		data32 |= (((lvl == 2) ? 1 : 0) << 27); // [   27] de_rise_del_irq;
		data32 |= (0    << 26); // [   26] last_emp_done;
		data32 |= (((lvl == 2) ? 1 : 0) << 25); // [   25] emp_field_done;
		data32 |= (0    << 23); // [   23] meter_stable_chg_cable;
		data32 |= (0    << 19); // [   19] edid_addr2_intr
		data32 |= (0    << 18); // [   18] edid_addr1_intr
		data32 |= (0    << 17); // [   17] edid_addr0_intr
		data32 |= (0    << 16); // [   16] hdcp_enc_state_fall
		data32 |= (0    << 15); // [   15] hdcp_enc_state_rise
		data32 |= (0    << 14); // [   14] hdcp_auth_start_fall
		data32 |= (0    << 13); // [   13] hdcp_auth_start_rise
		data32 |= (0    << 12); // [   12] meter_stable_chg_hdmi
		data32 |= (0    << 11); // [   11] vid_colour_depth_chg
		data32 |= (0    << 10); // [   10] vid_fmt_chg
		data32 |= (0x0  << 6);  // [ 8: 6] hdmirx_5v_fall
		data32 |= (0x0  << 3);  // [ 5: 3] hdmirx_5v_rise
		// [    2] sherman_phy_intr: phy digital interrupt
		data32 |= (0    << 2);
		// [    1] pwd_sherman_intr: controller pwd interrupt
		data32 |= (1    << 1);
		// [    0] aon_sherman_intr: controller aon interrupt
		data32 |= (0    << 0);
		top_intr_maskn_value = data32;
	} else {
		data32 = 0;
		//hdmirx_sqofclk_fall
		data32 |= (((lvl == 2) ? 1 : 0) << 29);
		//de_rise_irq: DE rise edge.
		data32 |= (((lvl == 2) ? 1 : 0) << 27);
		//RX Controller IP interrupt.
		data32 |= (1 << 0);
		top_intr_maskn_value = data32;
	}
	if (en) {
		/* for TXLX, cec phy address error issues */
		if (rx.chip_id <= CHIP_ID_TL1)
			top_intr_maskn_value |= 0x1e0000;

		hdmirx_wr_top(TOP_INTR_MASKN, top_intr_maskn_value);
	} else {
		hdmirx_wr_top(TOP_INTR_MASKN, 0);
	}
}

/*
 * rx_get_aud_info - get aduio info
 */
void rx_get_aud_info(struct aud_info_s *audio_info)
{
	struct packet_info_s *prx = &rx_pkt;
	struct aud_infoframe_st *pkt =
		(struct aud_infoframe_st *)&prx->aud_pktinfo;

	u32 tmp = 0;

	/* refer to hdmi spec. CT = 0 */
	audio_info->coding_type = 0;
	/* refer to hdmi spec. SS = 0 */
	audio_info->sample_size = 0;
	/* refer to hdmi spec. SF = 0*/
	audio_info->sample_frequency = 0;
	if (rx.chip_id >= CHIP_ID_T7) {
		pkt->pkttype = PKT_TYPE_INFOFRAME_AUD;
		pkt->version = hdmirx_rd_cor(AUDRX_VERS_DP2_IVCRX);
		pkt->length = hdmirx_rd_cor(AUDRX_LENGTH_DP2_IVCRX);
		pkt->checksum = hdmirx_rd_cor(AUDRX_CHSUM_DP2_IVCRX);
		pkt->rsd = 0;
		/*get AudioInfo */
		pkt->coding_type = hdmirx_rd_bits_cor(AUDRX_DBYTE1_DP2_IVCRX, MSK(4, 4));
		pkt->ch_count = hdmirx_rd_bits_cor(AUDRX_DBYTE1_DP2_IVCRX, MSK(3, 0));
		pkt->sample_frq = hdmirx_rd_bits_cor(AUDRX_DBYTE2_DP2_IVCRX, MSK(3, 2));
		pkt->sample_size = hdmirx_rd_bits_cor(AUDRX_DBYTE2_DP2_IVCRX, MSK(2, 0));
		pkt->fromat = hdmirx_rd_cor(AUDRX_DBYTE3_DP2_IVCRX);
		pkt->ca = hdmirx_rd_cor(AUDRX_DBYTE4_DP2_IVCRX);
		pkt->down_mix = hdmirx_rd_bits_cor(AUDRX_DBYTE5_DP2_IVCRX, MSK(7, 6));
		pkt->level_shift_value =
			hdmirx_rd_bits_cor(AUDRX_DBYTE5_DP2_IVCRX, MSK(4, 3));
		pkt->lfep = hdmirx_rd_bits_cor(AUDRX_DBYTE5_DP2_IVCRX, MSK(2, 0));
		audio_info->channel_count = pkt->ch_count;
		audio_info->auds_ch_alloc = pkt->ca;
		audio_info->aud_hbr_rcv =
			(hdmirx_rd_cor(RX_AUDP_STAT_DP2_IVCRX) >> 6) & 1;
		audio_info->auds_layout = hdmirx_rd_bits_cor(RX_AUDP_STAT_DP2_IVCRX, MSK(2, 3));

		//if (rx.chip_id >= CHIP_ID_T3X) {
		tmp = (hdmirx_rd_cor(RX_ACR_DBYTE4_DP2_IVCRX) & 0x0f) << 16;
		tmp += hdmirx_rd_cor(RX_ACR_DBYTE5_DP2_IVCRX) << 8;
		tmp += hdmirx_rd_cor(RX_ACR_DBYTE6_DP2_IVCRX);
		audio_info->n = tmp;
		tmp = (hdmirx_rd_cor(RX_ACR_DBYTE1_DP2_IVCRX) & 0x0f) << 16;
		tmp += hdmirx_rd_cor(RX_ACR_DBYTE2_DP2_IVCRX) << 8;
		tmp += hdmirx_rd_cor(RX_ACR_DBYTE3_DP2_IVCRX);
			audio_info->cts = tmp;
		//} else {//todo
			//audio_info->n = hdmirx_rd_top(TOP_ACR_N_STAT);
			//audio_info->cts = hdmirx_rd_top(TOP_ACR_CTS_STAT);
		//}

		//if (rx.chip_id >= CHIP_ID_T3) {
		if (pkt->length == 10) {
			//aif length is 10
			if (audio_info->aud_hbr_rcv)
				audio_info->aud_packet_received = 8;
			else
				audio_info->aud_packet_received = 1;
		} else {
			audio_info->aud_packet_received = 0;
		}
		audio_info->ch_sts[0] = hdmirx_rd_cor(RX_CHST1_AUD_IVCRX);
		audio_info->ch_sts[1] = hdmirx_rd_cor(RX_CHST2_AUD_IVCRX);
		audio_info->ch_sts[2] = hdmirx_rd_cor(RX_CHST3a_AUD_IVCRX);
		audio_info->ch_sts[3] = hdmirx_rd_cor(RX_CHST4_AUD_IVCRX);
		audio_info->ch_sts[4] = hdmirx_rd_cor(RX_CHST5_AUD_IVCRX);
		audio_info->ch_sts[5] = hdmirx_rd_cor(RX_CHST6_AUD_IVCRX);
		audio_info->ch_sts[6] = hdmirx_rd_cor(RX_CHST7_AUD_IVCRX);
	} else {
		audio_info->channel_count =
			hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, CHANNEL_COUNT);
		audio_info->coding_extension =
			hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, AIF_DATA_BYTE_3);
		audio_info->auds_ch_alloc =
			hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, CH_SPEAK_ALLOC);
		audio_info->auds_layout =
			hdmirx_rd_bits_dwc(DWC_PDEC_STS, PD_AUD_LAYOUT);
		audio_info->aud_hbr_rcv =
			hdmirx_rd_dwc(DWC_PDEC_AUD_STS) & AUDS_HBR_RCV;
		audio_info->aud_packet_received =
				hdmirx_rd_dwc(DWC_PDEC_AUD_STS);
		audio_info->aud_mute_en =
			(hdmirx_rd_bits_dwc(DWC_PDEC_STS, PD_GCP_MUTE_EN) == 0)
			? false : true;
		audio_info->cts = hdmirx_rd_dwc(DWC_PDEC_ACR_CTS);
		audio_info->n = hdmirx_rd_dwc(DWC_PDEC_ACR_N);
		audio_info->afifo_cfg = rx_get_afifo_cfg();
	}
	if (audio_info->cts != 0) {
		audio_info->arc =
			(rx.clk.tmds_clk / audio_info->cts) *
			audio_info->n / 128;
	} else {
		audio_info->arc = 0;
	}
	audio_info->aud_clk = rx.clk.aud_pll;
}

/*
 * rx_get_audio_status - interface for audio module
 */
void rx_get_audio_status(struct rx_audio_stat_s *aud_sts)
{
	enum tvin_sig_fmt_e fmt = hdmirx_hw_get_fmt();

	if (rx.state == FSM_SIG_READY &&
	    fmt != TVIN_SIG_FMT_NULL &&
	    rx.avmute_skip == 0) {
		if (rx.chip_id < CHIP_ID_T7) {
			aud_sts->aud_alloc = rx.aud_info.auds_ch_alloc;
			aud_sts->aud_sr = rx.aud_info.real_sr;
			aud_sts->aud_channel_cnt = rx.aud_info.channel_count;
			aud_sts->aud_type = rx.aud_info.coding_type;
			aud_sts->afifo_thres_pass =
				((hdmirx_rd_dwc(DWC_AUD_FIFO_STS) &
				 THS_PASS_STS) == 0) ? false : true;
			aud_sts->aud_rcv_packet = rx.aud_info.aud_packet_received;
			aud_sts->aud_stb_flag =
				aud_sts->afifo_thres_pass &&
				!rx.aud_info.aud_mute_en;
		} else {
			if ((rx.afifo_sts & 3) == 0)
				aud_sts->aud_stb_flag = true;
			aud_sts->aud_alloc = rx.aud_info.auds_ch_alloc;
			aud_sts->aud_rcv_packet = rx.aud_info.aud_packet_received;
			aud_sts->aud_channel_cnt = rx.aud_info.channel_count;
			aud_sts->aud_type = rx.aud_info.coding_type;
			aud_sts->aud_sr = rx.aud_info.real_sr;
			memcpy(aud_sts->ch_sts, &rx.aud_info.ch_sts, 7);
		}
	} else {
		memset(aud_sts, 0, sizeof(struct rx_audio_stat_s));
	}
}
EXPORT_SYMBOL(rx_get_audio_status);

/*
 * rx_get_audio_status - interface for audio module
 */

int rx_set_audio_param(u32 param)
{
	if (rx.chip_id < CHIP_ID_T7)
		hbr_force_8ch = param & 1;
	else
		rx_set_aud_output_t7(param);
	return 1;
}
EXPORT_SYMBOL(rx_set_audio_param);

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
	u32 clk_rate = 0;

	if (rx.chip_id == CHIP_ID_TXHD ||
	    rx.chip_id == CHIP_ID_T5D)
		clk_rate = 0;
	else if (rx.chip_id >= CHIP_ID_T7)
		clk_rate = (hdmirx_rd_cor(SCDCS_TMDS_CONFIG_SCDC_IVCRX) >> 1) & 1;
	else
		clk_rate = (hdmirx_rd_dwc(DWC_SCDC_REGS0) >> 17) & 1;

	if (force_clk_rate & 0x10) {
		clk_rate = force_clk_rate & 1;
		if (clk_rate)
			hdmirx_wr_cor(HDMI2_MODE_CTRL_AON_IVCRX, 0x33);
		else
			hdmirx_wr_cor(HDMI2_MODE_CTRL_AON_IVCRX, 0x11);
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
	u32 clk = 0;

	if (rx.phy_ver == PHY_VER_TM2) {
		if (rx.clk.cable_clk <= MAX_TMDS_CLK &&
		    rx.clk.cable_clk >= MIN_TMDS_CLK)
			clk = rx.clk.cable_clk;
	} else if (rx.chip_id >= CHIP_ID_TL1) {
		/* sqof_clk */
		clk = hdmirx_rd_top(TOP_MISC_STAT0) & 0x1;
	} else {
		/* phy clk */
		clk = hdmirx_rd_phy(PHY_MAINFSM_STATUS1) & 0x100;
	}

	if (clk && rx.clk.cable_clk > TMDS_CLK_MIN * KHz) {
		if (rx.state >= FSM_EQ_START &&
			(abs(rx.clk.cable_clk - rx.clk.cable_clk_pre) > 5 * MHz))
			return false;
		return true;
	} else {
		return false;
	}
}

void rx_afifo_store_valid(bool en)
{
	if (rx.chip_id >= CHIP_ID_T7)
		return;

	if (!en) {
		hdmirx_wr_bits_dwc(DWC_AUD_FIFO_CTRL,
						   AFIF_SUBPACKETS, 0);
		rx.aud_info.afifo_cfg = false;
		if (log_level & AUDIO_LOG)
			rx_pr("afifo store all\n");
	} else {
		hdmirx_wr_bits_dwc(DWC_AUD_FIFO_CTRL,
						   AFIF_SUBPACKETS, 1);
		rx.aud_info.afifo_cfg = true;
		if (log_level & AUDIO_LOG)
			rx_pr("afifo store valid\n");
	}
}

bool rx_get_afifo_cfg(void)
{
	if (rx.chip_id >= CHIP_ID_T7)
		return true;

	if (hdmirx_rd_bits_dwc(DWC_AUD_FIFO_CTRL, AFIF_SUBPACKETS))
		return true;
	else
		return false;
}

/*
 * hdmirx_audio_fifo_rst - reset afifo
 */
void  hdmirx_audio_fifo_rst(void)
{

	if (rx.chip_id >= CHIP_ID_T7) {
		hdmirx_wr_bits_cor(RX_PWD_SRST_PWD_IVCRX, _BIT(1), 1);
		udelay(1);
		hdmirx_wr_bits_cor(RX_PWD_SRST_PWD_IVCRX, _BIT(1), 0);
	} else {
		hdmirx_wr_dwc(DWC_DMI_SW_RST, 0x10);
		hdmirx_wr_bits_dwc(DWC_AUD_FIFO_CTRL, AFIF_INIT, 1);
		udelay(1);
		hdmirx_wr_bits_dwc(DWC_AUD_FIFO_CTRL, AFIF_INIT, 0);
	}
	if (log_level & AUDIO_LOG)
		rx_pr("%s\n", __func__);
}

void hdmirx_audio_disabled(void)
{
	if (rx.chip_id >= CHIP_ID_T7)
		hdmirx_wr_bits_cor(RX_PWD_SRST_PWD_IVCRX, _BIT(1), 1);
	else
		hdmirx_wr_bits_dwc(DWC_AUD_FIFO_CTRL, AFIF_INIT, 1);

	if (log_level & AUDIO_LOG)
		rx_pr("%s\n", __func__);
}

/*
 * hdmirx_control_clk_range
 */
int hdmirx_control_clk_range(unsigned long min, unsigned long max)
{
	int error = 0;
	unsigned int eval_time = 0;
	unsigned long ref_clk;

	ref_clk = modet_clk;
	eval_time = (ref_clk * 4095) / 158000;
	min = (min * eval_time) / ref_clk;
	max = (max * eval_time) / ref_clk;
	hdmirx_wr_bits_dwc(DWC_HDMI_CKM_F, MINFREQ, min);
	hdmirx_wr_bits_dwc(DWC_HDMI_CKM_F, CKM_MAXFREQ, max);
	return error;
}

/*
 * set_scdc_cfg
 */
void set_scdc_cfg(int hpdlow, int pwr_provided)
{
	switch (rx.chip_id) {
	case CHIP_ID_TXHD:
	case CHIP_ID_T5D:
		break;
	case CHIP_ID_GXTVBB:
	case CHIP_ID_TXL:
	case CHIP_ID_TXLX:
	case CHIP_ID_TL1:
	case CHIP_ID_TM2:
	case CHIP_ID_T5:
		hdmirx_wr_dwc(DWC_SCDC_CONFIG,
			(hpdlow << 1) | (pwr_provided << 0));
		break;
	case CHIP_ID_T7:
	case CHIP_ID_T3:
	case CHIP_ID_T5W:
		/*
		 * clear scdc with RX_HPD_C_CTRL_AON_IVCRX(0xf5)
		 * hdmirx_wr_bits_cor(RX_C0_SRST2_AON_IVCRX, _BIT(5), 1);
		 * udelay(1);
		 * hdmirx_wr_bits_cor(RX_C0_SRST2_AON_IVCRX, _BIT(5), 0);
		 */
	default:
		//hdmirx_wr_cor(RX_HPD_C_CTRL_AON_IVCRX, pwr_provided);
		break;
	}
}

int packet_init_t5(void)
{
	int error = 0;
	int data32 = 0;

	data32 |= 1 << 12; /* emp_err_filter, tl1*/
	data32 |= 1 << 11;
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
	data32 |= 0 << 30;  /* Enable packet FIFO store EMP pkt*/
	#ifdef VSIF_PKT_READ_FROM_PD_FIFO
	data32 |= 1 << 22;  //vsi
	#endif
	data32 |= 1 << 4;	/* PD_FIFO_WE */
	data32 |= 0 << 1;	/* emp pkt rev int,0:last 1:every */
	data32 |= 1 << 0;	/* PDEC_BCH_EN */
	data32 &= (~GCP_GLOB_AVMUTE);
	data32 |= GCP_GLOB_AVMUTE_EN << 15;
	data32 |= packet_fifo_cfg;
	hdmirx_wr_dwc(DWC_PDEC_CTRL, data32);

	data32 = 0;
	data32 |= pd_fifo_start_cnt << 20;	/* PD_start */
	data32 |= 640 << 10;	/* PD_max */
	data32 |= 8 << 0;		/* PD_min */
	hdmirx_wr_dwc(DWC_PDEC_FIFO_CFG, data32);

	return error;
}

int packet_init_t7(void)
{
	u8 data8 = 0;

#ifndef MULTI_VSIF_EXPORT_TO_EMP
	/* vsif id check en */
	hdmirx_wr_cor(VSI_CTRL2_DP3_IVCRX, 1);
	/* vsif pkt id cfg, default is 000c03 */
	hdmirx_wr_cor(VSIF_ID1_DP3_IVCRX, 0xd8);
	hdmirx_wr_cor(VSIF_ID2_DP3_IVCRX, 0x5d);
	hdmirx_wr_cor(VSIF_ID3_DP3_IVCRX, 0xc4);
	//hdmirx_wr_cor(VSIF_ID4_DP3_IVCRX, 0);

	/* hf-vsif id check en */
	hdmirx_wr_bits_cor(HF_VSIF_CTRL_DP3_IVCRX, _BIT(3), 1);
	/* hf-vsif set to get dv, default is 0xc45dd8 */
	hdmirx_wr_cor(HF_VSIF_ID1_DP3_IVCRX, 0x46);
	hdmirx_wr_cor(HF_VSIF_ID2_DP3_IVCRX, 0xd0);
	hdmirx_wr_cor(HF_VSIF_ID3_DP3_IVCRX, 0x00);

	data8 = 0;
	data8 |= 1 << 7; /* enable clr vsif pkt */
	data8 |= 1 << 5; /* enable comparison first 3 bytes IEEE */
	data8 |= 4 << 2; /* clr register if 4 frames no pkt */
	hdmirx_wr_cor(VSI_CTRL1_DP3_IVCRX, data8);
	hdmirx_wr_cor(VSI_CTRL3_DP3_IVCRX, 1);
	/* aif to store hdr10+ */
	hdmirx_wr_cor(VSI_ID1_DP3_IVCRX, 0x8b);
	hdmirx_wr_cor(VSI_ID2_DP3_IVCRX, 0x84);
	hdmirx_wr_cor(VSI_ID3_DP3_IVCRX, 0x90);

	/* use unrec to store hf-vsif */
	hdmirx_wr_cor(RX_UNREC_CTRL_DP2_IVCRX, 1);
	hdmirx_wr_cor(RX_UNREC_DEC_DP2_IVCRX, PKT_TYPE_INFOFRAME_VSI);
#endif
	/* get data 0x11c0-11de */

	data8 = 0;
	data8 |= 0 << 7; /* use AIF to VSI */
	data8 |= 1 << 6; /* irq is set for any VSIF */
	data8 |= 0 << 5; /* irq is set for any ACP */
	data8 |= 1 << 4; /* irq is set for any UN-REC */
	data8 |= 0 << 3; /* irq is set for any MPEG */
	data8 |= 1 << 2; /* irq is set for any AUD */
	data8 |= 1 << 1; /* irq is set for any SPD */
	data8 |= 0 << 0; /* irq is set for any AVI */
	hdmirx_wr_cor(RX_INT_IF_CTRL_DP2_IVCRX, data8);

	data8 = 0;
	data8 |= 0 << 7; /* rsvd */
	data8 |= 0 << 6; /* rsvd */
	data8 |= 0 << 5; /* rsvd */
	data8 |= 0 << 4; /* rsvd */
	data8 |= 0 << 3; /* irq is set for any ACR */
	data8 |= 0 << 2; /* irq is set for any GCP */
	data8 |= 0 << 1; /* irq is set for any ISC2 */
	data8 |= 0 << 0; /* irq is set for any ISC1 */
	hdmirx_wr_cor(RX_INT_IF_CTRL2_DP2_IVCRX, data8);

	/* auto clr pkt if cable-unplugged/sync lost */
	data8 = 0;
	data8 |= 1 << 7; /*  */
	data8 |= 1 << 6; /*  */
	data8 |= 1 << 5; /*  */
	data8 |= 1 << 4; /*  */
	data8 |= 1 << 3; /*  */
	data8 |= 1 << 2; /*  */
	data8 |= 1 << 1; /*  */
	data8 |= 1 << 0; /*  */
	hdmirx_wr_cor(RX_AUTO_CLR_PKT1_DP2_IVCRX, data8);

	/* auto clr pkt if cable-unplugged/sync lost */
	data8 = 0;
	data8 |= 1 << 7; /*  */
	data8 |= 1 << 6; /*  */
	data8 |= 1 << 5; /*  */
	data8 |= 1 << 4; /*  */
	data8 |= 1 << 3; /*  */
	data8 |= 1 << 2; /*  */
	data8 |= 1 << 1; /*  */
	data8 |= 1 << 0; /*  */
	hdmirx_wr_cor(RX_AUTO_CLR_PKT2_DP2_IVCRX, data8);

	/* auto clr pkt if did not get update */
	data8 = 0;
	data8 |= 1 << 1; /* meta data */
	data8 |= 1 << 0; /* GCP */
	hdmirx_wr_cor(IF_CTRL2_DP3_IVCRX, data8);

	return 0;
}

/*
 * packet_init - packet receiving config
 */
int packet_init(void)
{
	if (rx.chip_id >= CHIP_ID_T7)
		packet_init_t7();
	else
		packet_init_t5();
	return 0;
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
		hdmirx_wr_dwc(DWC_PDEC_IEN_SET, _BIT(2) | i);
}

/*
 * hdmirx_packet_fifo_rst - reset packet fifo
 */
unsigned int hdmirx_packet_fifo_rst(void)
{
	int error = 0;

	hdmirx_wr_bits_dwc(DWC_PDEC_CTRL,
			   PD_FIFO_FILL_INFO_CLR | PD_FIFO_CLR, ~0);
	hdmirx_wr_bits_dwc(DWC_PDEC_CTRL,
			   PD_FIFO_FILL_INFO_CLR | PD_FIFO_CLR,  0);
	return error;
}

void rx_set_suspend_edid_clk(bool en)
{
	if (en) {
		hdmirx_wr_bits_top(TOP_EDID_GEN_CNTL,
				   MSK(7, 0), 1);
	} else {
		hdmirx_wr_bits_top(TOP_EDID_GEN_CNTL,
				   MSK(7, 0), EDID_CLK_DIV);
	}
}

void rx_i2c_div_init(void)
{
	int data32 = 0;

	data32 |= (0xf	<< 13); /* bit[16:13] */
	data32 |= 0	<< 11;
	data32 |= 0	<< 10;
	data32 |= 0	<< 9;
	data32 |= 0 << 8;
	data32 |= EDID_CLK_DIV << 0;
	hdmirx_wr_top(TOP_EDID_GEN_CNTL,  data32);
}

void rx_i2c_hdcp_cfg(void)
{
	int data32 = 0;

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
}

void rx_i2c_edid_cfg_with_port(u8 port_id, bool en)
{
	int data32 = 0;

	data32 = 0;
	/* SDA filter internal clk div */
	data32 |= 1 << 29;
	/* SDA sampling clk div */
	data32 |= 1 << 16;
	/* SCL filter internal clk div */
	data32 |= 1 << 13;
	/* SCL sampling clk div */
	data32 |= 1 << 0;
	if (!en) {
		data32 = 0xffffffff;
		if (port_id == 0)
			hdmirx_wr_top(TOP_INFILTER_I2C0, data32);
		else if (port_id == 1)
			hdmirx_wr_top(TOP_INFILTER_I2C1, data32);
		else if (port_id == 2)
			hdmirx_wr_top(TOP_INFILTER_I2C2, data32);
		else if (port_id == 3)
			hdmirx_wr_top(TOP_INFILTER_I2C3, data32);
	} else {
		hdmirx_wr_top(TOP_INFILTER_I2C0, data32);
		hdmirx_wr_top(TOP_INFILTER_I2C1, data32);
		hdmirx_wr_top(TOP_INFILTER_I2C2, data32);
		hdmirx_wr_top(TOP_INFILTER_I2C3, data32);
	}
}

/*
 * TOP_init - hdmirx top initialization
 */
static int TOP_init(void)
{
	int err = 0;
	int data32 = 0;

	if (rx.chip_id >= CHIP_ID_T7) {
		rx_hdcp22_wr_top(TOP_SECURE_MODE,  1);
		/* Filter 100ns glitch */
		hdmirx_wr_top(TOP_AUD_PLL_LOCK_FILTER,  32);
		data32  = 0;
		data32 |= (1 << 1);// [1:0]  sel
		hdmirx_wr_top(TOP_PHYIF_CNTL0, data32);
	}
	rx_i2c_div_init();
	rx_i2c_hdcp_cfg();
	rx_i2c_edid_cfg_with_port(0xf, true);
	data32 = 0;
	if (rx.chip_id >= CHIP_ID_T7) {
		/*420to444_en*/
		data32 |= 1	<< 21;
		/*422to444_en*/
		data32 |= 1	<< 20;
	}
	/* conversion mode of 422 to 444 */
	data32 |= 0	<< 19;
	/* pixel_repeat_ovr 0=auto  1 only for T7!!! */
	if (rx.chip_id == CHIP_ID_T7)
		data32 |= 1 << 7;
	/* !!!!dolby vision 422 to 444 ctl bit */
	data32 |= 0	<< 0;
	hdmirx_wr_top(TOP_VID_CNTL,	data32);

	if (rx.chip_id != CHIP_ID_TXHD &&
		rx.chip_id != CHIP_ID_T5D) {
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

		/* Configure TMDS align T7 unused */
		data32	= 0;
		hdmirx_wr_top(TOP_TMDS_ALIGN_CNTL0,	data32);
		data32	= 0;
		hdmirx_wr_top(TOP_TMDS_ALIGN_CNTL1,	data32);

		/* Enable channel output */
		data32 = hdmirx_rd_top(TOP_CHAN_SWITCH_0);
		hdmirx_wr_top(TOP_CHAN_SWITCH_0, data32 | (1 << 0));

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
	return err;
}

/*
 * DWC_init - DWC controller initialization
 */
static int DWC_init(void)
{
	int err = 0;
	unsigned long   data32;
	unsigned int eval_time = 0;

	eval_time = (modet_clk * 4095) / 158000;
	/* enable all */
	hdmirx_wr_dwc(DWC_HDMI_OVR_CTRL, ~0);
	/* recover to default value.*/
	/* remain code for some time.*/
	/* if no side effect then remove it */
	/*hdmirx_wr_bits_dwc(DWC_HDMI_SYNC_CTRL,*/
	/*	VS_POL_ADJ_MODE, VS_POL_ADJ_AUTO);*/
	/*hdmirx_wr_bits_dwc(DWC_HDMI_SYNC_CTRL,*/
	/*	HS_POL_ADJ_MODE, HS_POL_ADJ_AUTO);*/

	hdmirx_wr_bits_dwc(DWC_HDMI_CKM_EVLTM, EVAL_TIME, eval_time);
	hdmirx_control_clk_range(TMDS_CLK_MIN, TMDS_CLK_MAX);

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
	/* bit[1:0] default setting should be 2 */
	data32 |= 2     << 0;   /* [1:0]    h_start_pos */
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
	if (hdcp14_key_mode == SECURE_MODE || hdcp14_on) {
		rx_set_hdcp14_secure_key();
		rx_pr("hdcp1.4 secure mode\n");
	} else {
		rx_hdcp14_set_normal_key(&rx.hdcp);
		rx_pr("hdcp1.4 normal mode\n");
	}
	if (rx.chip_id != CHIP_ID_TXHD &&
		rx.chip_id != CHIP_ID_T5D) {
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL,
				   REPEATER, hdcp->repeat ? 1 : 0);
		/* nothing attached downstream */
		hdmirx_wr_dwc(DWC_HDCP_RPT_BSTATUS, 0);
	}
	hdmirx_wr_bits_dwc(DWC_HDCP_CTRL, ENCRIPTION_ENABLE, 1);
}

/* rst cdr to clr tmds_valid */
bool rx_clr_tmds_valid(void)
{
	bool ret = false;

	if (rx.state >= FSM_SIG_STABLE) {
		rx.state = FSM_WAIT_CLK_STABLE;
		if (vpp_mute_enable) {
			rx_mute_vpp();
			set_video_mute(HDMI_RX_MUTE_SET, true);
			rx_pr("vpp mute\n");
		}
		hdmirx_output_en(false);
		hdmirx_top_irq_en(0, 0);
		if (log_level & VIDEO_LOG)
			rx_pr("%s!\n", __func__);
	}
	if (rx.state < FSM_SIG_READY)
		return ret;
	if (rx.phy_ver == PHY_VER_TL1) {
		wr_reg_hhi_bits(HHI_HDMIRX_PHY_MISC_CNTL0, MSK(3, 7), 0);
		if (log_level & VIDEO_LOG)
			rx_pr("%s!\n", __func__);
		ret = true;
	} else if (rx.phy_ver == PHY_VER_TM2) {
		if (rx.aml_phy.force_sqo) {
			wr_reg_hhi_bits(HHI_HDMIRX_PHY_DCHD_CNTL0, _BIT(25), 0);
			udelay(5);
			wr_reg_hhi_bits(HHI_HDMIRX_PHY_DCHD_CNTL0, _BIT(25), 1);
			if (log_level & VIDEO_LOG)
				rx_pr("low amplitude %s!\n", __func__);
			ret = true;
		}
	} else if (rx.phy_ver >= PHY_VER_T5) {
		hdmirx_wr_bits_amlphy(T5_HHI_RX_PHY_DCHD_CNTL0, T5_CDR_RST, 0);
		ret = true;
		if (log_level & VIDEO_LOG)
			rx_pr("%s!\n", __func__);
	}
	return ret;
}

void rx_set_term_value_pre(unsigned char port, bool value)
{
	u32 data32;

	if (rx.chip_id >= CHIP_ID_TL1) {
		data32 = rd_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL0);
		if (port < E_PORT3) {
			if (value) {
				data32 |= (1 << port);
			} else {
				/* rst cdr to clr tmds_valid */
				data32 &= ~(MSK(3, 7));
				data32 &= ~(1 << port);
			}
			wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL0, data32);
		} else if (port == ALL_PORTS) {
			if (value) {
				data32 |= 0x7;
			} else {
				/* rst cdr to clr tmds_valid */
				data32 &= 0xfffffc78;
				data32 |= (MSK(3, 7));
				/* data32 &= 0xfffffff8; */
			}
			wr_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL0, data32);
		}
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
		}
	}
}

void rx_set_term_value_t5(unsigned char port, bool value)
{
	u32 data32;

	data32 = hdmirx_rd_amlphy(T5_HHI_RX_PHY_MISC_CNTL0);
	if (port < E_PORT3) {
		if (value) {
			data32 |= (1 << port);
		} else {
			/* rst cdr to clr tmds_valid */
			data32 &= ~(MSK(3, 7));
			data32 &= ~(1 << port);
		}
		hdmirx_wr_amlphy(T5_HHI_RX_PHY_MISC_CNTL0, data32);
	} else if (port == ALL_PORTS) {
		if (value) {
			data32 |= 0x7;
		} else {
			/* rst cdr to clr tmds_valid */
			data32 &= 0xfffffc78;
			data32 |= (MSK(3, 7));
			/* data32 &= 0xfffffff8; */
		}
		hdmirx_wr_amlphy(T5_HHI_RX_PHY_MISC_CNTL0, data32);
	}
}

void rx_set_term_value(unsigned char port, bool value)
{
	if (rx.chip_id >= CHIP_ID_T5)
		rx_set_term_value_t5(port, value);
	else
		rx_set_term_value_pre(port, value);
}

int rx_set_port_hpd(u8 port_id, bool val)
{
	if (port_id < E_PORT_NUM) {
		if (val) {
			if (rx.chip_id >= CHIP_ID_T7)
				hdmirx_wr_bits_cor(RX_C0_SRST2_AON_IVCRX, _BIT(5), 0);
			hdmirx_wr_bits_top(TOP_HPD_PWR5V, _BIT(port_id), 1);
			rx_i2c_edid_cfg_with_port(0xf, true);
			rx_set_term_value(port_id, 1);
		} else {
			if (rx.chip_id >= CHIP_ID_T7 && rx.port == port_id)
				hdmirx_wr_bits_cor(RX_C0_SRST2_AON_IVCRX, _BIT(5), 1);
			rx_i2c_edid_cfg_with_port(port_id, false);
			hdmirx_wr_bits_top(TOP_HPD_PWR5V, _BIT(port_id), 0);
			rx_set_term_value(port_id, 0);
		}
	} else if (port_id == ALL_PORTS) {
		if (val) {
			if (rx.chip_id >= CHIP_ID_T7)
				hdmirx_wr_bits_cor(RX_C0_SRST2_AON_IVCRX, _BIT(5), 0);
			rx_i2c_edid_cfg_with_port(0xf, true);
			hdmirx_wr_bits_top(TOP_HPD_PWR5V, MSK(4, 0), 0xF);
			rx_set_term_value(port_id, 1);
		} else {
			hdmirx_wr_bits_top(TOP_HPD_PWR5V, MSK(4, 0), 0x0);
			rx_set_term_value(port_id, 0);
		}
	} else {
		return -1;
	}
	if (log_level & LOG_EN)
		rx_pr("%s, port:%d, val:%d\n", __func__, port_id, val);
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
void rx_force_hpd_cfg(u8 hpd_level)
{
	unsigned int hpd_value;

	if (hpd_level) {
		if (disable_port_en)
			hpd_value = (~(1 << disable_port_num)) & 0xF;
		else
			hpd_value = 0xF;

		rx_set_port_hpd(ALL_PORTS, hpd_value);
	} else {
		rx_set_port_hpd(ALL_PORTS, 0);
	}
}

/*
 * rx_force_rxsense_cfg_pre - force config rxsense level on all ports
 * for the chips before t5
 * @level: rxsense level
 */
void rx_force_rxsense_cfg_pre(u8 level)
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
		} else {
			hdmirx_wr_bits_phy(PHY_MAIN_FSM_OVERRIDE1,
					   PHY_TERM_OV_VALUE, 0x0);
		}
	}
}

void rx_force_rxsense_cfg_t5(u8 level)
{
	unsigned int term_ovr_value;
	unsigned int data32;

	/* enable terminal connect */
	data32 = hdmirx_rd_amlphy(T5_HHI_RX_PHY_MISC_CNTL0);
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
	hdmirx_wr_amlphy(T5_HHI_RX_PHY_MISC_CNTL0, data32);
}

void rx_force_rxsense_cfg(u8 level)
{
	if (rx.chip_id >= CHIP_ID_T5)
		rx_force_rxsense_cfg_t5(level);
	else
		rx_force_rxsense_cfg_pre(level);
}

/*
 * rx_force_hpd_rxsense_cfg - force config
 * hpd & rxsense level on all ports
 * @level: hpd & rxsense level
 */
void rx_force_hpd_rxsense_cfg(u8 level)
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

void rx_dig_clk_en(bool en)
{
	if (rx.chip_id >= CHIP_ID_T7) {
		hdmirx_wr_bits_clk_ctl(RX_CLK_CTRL1, CFG_CLK_EN, en);
		hdmirx_wr_bits_clk_ctl(RX_CLK_CTRL3, METER_CLK_EN, en);
	} else {
		hdcp22_clk_en(en);
		/* enable gate of cts_hdmirx_modet_clk */
		/* enable gate of cts_hdmirx_cfg_clk */
		if (rx.chip_id >= CHIP_ID_T5) {
			hdmirx_wr_bits_clk_ctl(HHI_HDMIRX_CLK_CNTL, MODET_CLK_EN, en);
			hdmirx_wr_bits_clk_ctl(HHI_HDMIRX_CLK_CNTL, CFG_CLK_EN, en);
		} else {
			wr_reg_hhi_bits(HHI_HDMIRX_CLK_CNTL, MODET_CLK_EN, en);
			wr_reg_hhi_bits(HHI_HDMIRX_CLK_CNTL, CFG_CLK_EN, en);
		}
	}
}

bool rx_get_dig_clk_en_sts(void)
{
	int ret;

	if (rx.chip_id >= CHIP_ID_T7)
		return true;
	if (rx.chip_id >= CHIP_ID_T5)
		ret = hdmirx_rd_bits_clk_ctl(HHI_HDMIRX_CLK_CNTL, CFG_CLK_EN);
	else
		ret = rd_reg_hhi_bits(HHI_HDMIRX_CLK_CNTL, CFG_CLK_EN);
	return ret;
}

void rx_esm_tmds_clk_en(bool en)
{
	if (rx.chip_id >= CHIP_ID_T7)
		return;
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
	u32 data32;

	if (rx.chip_id >= CHIP_ID_T7)
		return;
	if (en) {
		if (rx.chip_id >= CHIP_ID_T5)
			data32 = rd_reg_clk_ctl(HHI_HDCP22_CLK_CNTL);
		else
			data32 = rd_reg_hhi(HHI_HDCP22_CLK_CNTL);
		/* [26:25] select cts_oscin_clk=24 MHz */
		data32 |= 0 << 25;
		/* [   24] Enable gated clock */
		data32 |= 1 << 24;
		data32 |= 0 << 16;
		/* [10: 9] fclk_div7=2000/7=285.71 MHz */
		data32 |= 0 << 9;
		/* [    8] clk_en. Enable gated clock */
		data32 |= 1 << 8;
		/* [ 6: 0] Divide by 1. = 285.71/1 = 285.71 MHz */
		data32 |= 0 << 0;
		if (rx.chip_id >= CHIP_ID_T5)
			wr_reg_clk_ctl(HHI_HDCP22_CLK_CNTL, data32);
		else
			wr_reg_hhi(HHI_HDCP22_CLK_CNTL, data32);
		/* axi clk config*/
		if (rx.chip_id >= CHIP_ID_T5)
			data32 = rd_reg_clk_ctl(HHI_AXI_CLK_CNTL);
		else
			data32 = rd_reg_hhi(HHI_AXI_CLK_CNTL);
		/* [    8] clk_en. Enable gated clock */
		data32 |= 1 << 8;
		if (rx.chip_id >= CHIP_ID_T5)
			wr_reg_clk_ctl(HHI_AXI_CLK_CNTL, data32);
		else
			wr_reg_hhi(HHI_AXI_CLK_CNTL, data32);

		if (rx.chip_id >= CHIP_ID_TL1)
			/* TL1:esm related clk bit9-11 */
			hdmirx_wr_bits_top(TOP_CLK_CNTL, MSK(3, 9), 0x7);
		else
			/* TL1:esm related clk bit3-5 */
			hdmirx_wr_bits_top(TOP_CLK_CNTL, MSK(3, 3), 0x7);

		if (rx.chip_id >= CHIP_ID_TM2)
			/* Enable axi_clk,for tm2 */
			/* AXI arbiter is moved outside of hdmitx. */
			/* There is an AXI arbiter in the chip's EE domain */
			/* for arbitrating AXI requests from HDMI TX and RX.*/
			hdmirx_wr_bits_top(TOP_CLK_CNTL, MSK(1, 12), 0x1);
	} else {
		if (rx.chip_id >= CHIP_ID_T5) {
			wr_reg_clk_ctl(HHI_HDCP22_CLK_CNTL, 0);
			wr_reg_clk_ctl(HHI_AXI_CLK_CNTL, 0);
		} else {
			wr_reg_hhi(HHI_HDCP22_CLK_CNTL, 0);
			wr_reg_hhi(HHI_AXI_CLK_CNTL, 0);
		}
		if (rx.chip_id >= CHIP_ID_TL1)
			/* TL1:esm related clk bit9-11 */
			hdmirx_wr_bits_top(TOP_CLK_CNTL, MSK(3, 9), 0x0);
		else
			/* TXLX:esm related clk bit3-5 */
			hdmirx_wr_bits_top(TOP_CLK_CNTL, MSK(3, 3), 0x0);
	}
}

/*
 * hdmirx_hdcp22_esm_rst - software reset esm
 */
void hdmirx_hdcp22_esm_rst(void)
{
	if (rx.chip_id >= CHIP_ID_T7)
		return;

	rx_pr("before kill\n");
	rx_kill_esm();
	mdelay(5);
	if (!rx22_ver) {
		rx_pr("before rst:\n");
		/* For TL1,the sw_reset_hdcp22 bit is top reg 0x0,bit'12 */
		if (rx.chip_id >= CHIP_ID_TL1)
			hdmirx_wr_top(TOP_SW_RESET, 0x1000);
		else
			/* For txlx and previous chips,the sw_reset_hdcp22 is bit'8 */
			hdmirx_wr_top(TOP_SW_RESET, 0x100);
		rx_pr("before releas\n");
		mdelay(1);
		hdmirx_wr_top(TOP_SW_RESET, 0x0);
	} else {
		rx_pr("do not kill\n");
	}
	rx_pr("esm rst\n");
}

/*
 * hdmirx_hdcp22_init - hdcp2.2 initialization
 */
void rx_is_hdcp22_support(void)
{
	int temp;

	if (rx.chip_id >= CHIP_ID_T7)
		return;
	temp = rx_sec_set_duk(hdmirx_repeat_support());
	if (temp > 0) {
		rx_hdcp22_wr_top(TOP_SKP_CNTL_STAT, 7);
		hdcp22_on = 1;
		if (temp == 2)
			rx_pr("2.2 test key!!!\n");
	} else {
		hdcp22_on = 0;
	}
	rx_pr("hdcp22 == %d\n", hdcp22_on);
}

/*
 * kill esm may not executed in rx22
 * kill esm in driver when 2.2 off
 * refer to ESM_Kill->esm_hostlib_mb_cmd
 */

void rx_kill_esm(void)
{
	rx_hdcp22_wr_reg(0x28, 9);
}

/*
 * hdmirx_hdcp22_hpd - set hpd level for hdcp2.2
 * @value: whether to set hpd high
 */
void hdmirx_hdcp22_hpd(bool value)
{
	if (rx.chip_id >= CHIP_ID_T7)
		return;
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
	if (rx.chip_id >= CHIP_ID_T7) {
		return;
	} else {
		/* note: can't pull down hpd before enter suspend */
		/* it will stop cec wake up func if EE domain still working */
		/* rx_set_cur_hpd(0); */
		hpd_to_esm = 0;
		hdmirx_wr_dwc(DWC_HDCP22_CONTROL, 0x0);
	//if (hdcp22_kill_esm == 0)
	hdmirx_hdcp22_esm_rst();
	//else
		//hdcp22_kill_esm = 0;
	hdcp22_clk_en(0);
	}
	rx_pr("hdcp22 off\n");
}

/*
 * hdcp_22_on
 */
void hdcp_22_on(void)
{
	if (rx.chip_id >= CHIP_ID_T7) {
		//TODO..
	} else {
		hdcp22_kill_esm = 0;
		/* switch_set_state(&rx.hpd_sdev, 0x0); */
		/* extcon_set_state_sync(rx.rx_extcon_rx22, EXTCON_DISP_HDMI, 0); */
		rx_hdcp22_send_uevent(0);
		hdcp22_clk_en(1);
		hdmirx_wr_dwc(DWC_HDCP22_CONTROL, 0x1000);
		/* rx_hdcp22_wr_top(TOP_SKP_CNTL_STAT, 0x1); */
		/* hdmirx_hw_config(); */
		/* switch_set_state(&rx.hpd_sdev, 0x1); */
		/* extcon_set_state_sync(rx.rx_extcon_rx22, EXTCON_DISP_HDMI, 1); */
		rx_hdcp22_send_uevent(1);
		hpd_to_esm = 1;
		/* don't need to delay 900ms to wait sysctl start hdcp_rx22,*/
		/*sysctl is userspace it wakes up later than driver */
		/* mdelay(900); */
		/* rx_set_cur_hpd(1); */
	}
	rx_pr("hdcp22 on\n");
}

/*
 * clk_init - clock initialization
 * config clock for hdmirx module
 */
void clk_init_cor(void)
{
	unsigned int data32;

	rx_pr("\n clk_init\n");
	/* DWC clock enable */
	/* Turn on clk_hdmirx_pclk, also = sysclk */
	wr_reg_clk_ctl(CLKCTRL_SYS_CLK_EN0_REG2,
		       rd_reg_clk_ctl(CLKCTRL_SYS_CLK_EN0_REG2) | (1 << 9));

	data32	= 0;
	data32 |= (0 << 25);// [26:25] clk_sel for cts_hdmirx_2m_clk: 0=cts_oscin_clk
	data32 |= (0 << 24);// [   24] clk_en for cts_hdmirx_2m_clk
	data32 |= (11 << 16);// [22:16] clk_div for cts_hdmirx_2m_clk: 24/12=2M
	data32 |= (3 << 9);// [10: 9] clk_sel for cts_hdmirx_5m_clk: 3=fclk_div5
	data32 |= (0 << 8);// [    8] clk_en for cts_hdmirx_5m_clk
	data32 |= (79 << 0);// [ 6: 0] clk_div for cts_hdmirx_5m_clk: fclk_dvi5/80=400/80=5M
	wr_reg_clk_ctl(RX_CLK_CTRL, data32);
	data32 |= (1 << 24);// [   24] clk_en for cts_hdmirx_2m_clk
	data32 |= (1 << 8);// [    8] clk_en for cts_hdmirx_5m_clk
	wr_reg_clk_ctl(RX_CLK_CTRL, data32);

	data32  = 0;
	data32 |= (3 << 25);// [26:25] clk_sel for cts_hdmirx_hdcp2x_eclk: 3=fclk_div5
	data32 |= (0 << 24);// [   24] clk_en for cts_hdmirx_hdcp2x_eclk
	data32 |= (15 << 16);// [22:16] clk_div for cts_hdmirx_hdcp2x_eclk: fclk_dvi5/16=400/16=25M
	data32 |= (3 << 9);// [10: 9] clk_sel for cts_hdmirx_cfg_clk: 3=fclk_div5
	data32 |= (0 << 8);// [    8] clk_en for cts_hdmirx_cfg_clk
	data32 |= (7 << 0);// [ 6: 0] clk_div for cts_hdmirx_cfg_clk: fclk_dvi5/8=400/8=50M
	wr_reg_clk_ctl(RX_CLK_CTRL1, data32);
	data32 |= (1 << 24);// [   24] clk_en for cts_hdmirx_hdcp2x_eclk
	data32 |= (1 << 8);// [    8] clk_en for cts_hdmirx_cfg_clk
	wr_reg_clk_ctl(RX_CLK_CTRL1, data32);

	data32  = 0;
	data32 |= (1 << 25);// [26:25] clk_sel for cts_hdmirx_acr_ref_clk: 1=fclk_div4
	data32 |= (0 << 24);// [   24] clk_en for cts_hdmirx_acr_ref_clk
	data32 |= (0 << 16);// [22:16] clk_div for cts_hdmirx_acr_ref_clk: fclk_div4/1=500M
	data32 |= (0 << 9);// [10: 9] clk_sel for cts_hdmirx_aud_pll_clk: 0=hdmirx_aud_pll_clk
	data32 |= (0 << 8);// [    8] clk_en for cts_hdmirx_aud_pll_clk
	data32 |= (0 << 0);// [ 6: 0] clk_div for cts_hdmirx_aud_pll_clk
	wr_reg_clk_ctl(RX_CLK_CTRL2, data32);
	data32 |= (1 << 24);// [   24] clk_en for cts_hdmirx_acr_ref_clk
	data32 |= (1 << 8);// [    8] clk_en for cts_hdmirx_aud_pll_clk
	wr_reg_clk_ctl(RX_CLK_CTRL2, data32);

	data32  = 0;
	data32 |= (0 << 9);// [10: 9] clk_sel for cts_hdmirx_meter_clk: 0=cts_oscin_clk
	data32 |= (0 << 8);// [    8] clk_en for cts_hdmirx_meter_clk
	data32 |= (0 << 0);// [ 6: 0] clk_div for cts_hdmirx_meter_clk: 24M
	wr_reg_clk_ctl(RX_CLK_CTRL3, data32);
	data32 |= (1 << 8);// [    8] clk_en for cts_hdmirx_meter_clk
	wr_reg_clk_ctl(RX_CLK_CTRL3, data32);

	data32  = 0;
	data32 |= (0 << 31);// [31]	  free_clk_en
	data32 |= (0 << 15);// [15]	  hbr_spdif_en
	data32 |= (0 << 8);// [8]	  tmds_ch2_clk_inv
	data32 |= (0 << 7);// [7]	  tmds_ch1_clk_inv
	data32 |= (0 << 6);// [6]	  tmds_ch0_clk_inv
	data32 |= (0 << 5);// [5]	  pll4x_cfg
	data32 |= (0 << 4);// [4]	  force_pll4x
	data32 |= (0 << 3);// [3]	  phy_clk_inv
	hdmirx_wr_top(TOP_CLK_CNTL,	data32);
}

void clk_init_dwc(void)
{
	unsigned int data32;

	/* DWC clock enable */
	/* Turn on clk_hdmirx_pclk, also = sysclk */
	wr_reg_hhi(HHI_GCLK_MPEG0,
		   rd_reg_hhi(HHI_GCLK_MPEG0) | (1 << 21));

	hdmirx_wr_ctl_port(0, 0x93ff);
	hdmirx_wr_ctl_port(0x10, 0x93ff);

	if (rx.chip_id == CHIP_ID_TXLX ||
	    rx.chip_id == CHIP_ID_TXHD ||
	    rx.chip_id >= CHIP_ID_TL1) {
		data32  = 0;
		data32 |= (0 << 15);
		data32 |= (1 << 14);
		data32 |= (0 << 5);
		data32 |= (0 << 4);
		data32 |= (0 << 0);
		if (rx.chip_id >= CHIP_ID_T5) {
			wr_reg_clk_ctl(HHI_AUD_PLL_CLK_OUT_CNTL, data32);
			data32 |= (1 << 4);
			wr_reg_clk_ctl(HHI_AUD_PLL_CLK_OUT_CNTL, data32);
		} else {
			wr_reg_hhi(HHI_AUD_PLL_CLK_OUT_CNTL, data32);
			data32 |= (1 << 4);
			wr_reg_hhi(HHI_AUD_PLL_CLK_OUT_CNTL, data32);
		}
	}
	data32 = hdmirx_rd_top(TOP_CLK_CNTL);
	data32 |= 0 << 31;  /* [31]     disable clk_gating */
	data32 |= 1 << 17;  /* [17]     aud_fifo_rd_en */
	data32 |= 1 << 16;  /* [16]     pkt_fifo_rd_en */
	if (rx.chip_id >= CHIP_ID_TL1) {
		data32 |= 0 << 8;   /* [8]      tmds_ch2_clk_inv */
		data32 |= 0 << 7;   /* [7]      tmds_ch1_clk_inv */
		data32 |= 0 << 6;   /* [6]      tmds_ch0_clk_inv */
		data32 |= 0 << 5;   /* [5]      pll4x_cfg */
		data32 |= 0 << 4;   /* [4]      force_pll4x */
		data32 |= 0 << 3;   /* [3]      phy_clk_inv: 1-invert */
	} else {
		data32 |= 1 << 2;   /* [2]      hdmirx_cec_clk_en */
		data32 |= 0 << 1;   /* [1]      bus_clk_inv */
		data32 |= 0 << 0;   /* [0]      hdmi_clk_inv */
	}
	hdmirx_wr_top(TOP_CLK_CNTL, data32);    /* DEFAULT: {32'h0} */
}

void clk_init(void)
{
	if (rx.chip_id >= CHIP_ID_T7)
		clk_init_cor();
	else
		clk_init_dwc();
}

/*
 * hdmirx_20_init - hdmi2.0 config
 */
void hdmirx_20_init(void)
{
	unsigned long data32;
	bool scdc_en =
		scdc_force_en ? 1 : get_edid_selection(rx.port);

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
	hdmirx_wr_dwc(DWC_SCDC_I2C_CONFIG,    data32);

	data32  = 0;
	data32 |= 1    << 1;  /* [1]      hpd_low */
	data32 |= 0    << 0;  /* [0]      power_provided */
	hdmirx_wr_dwc(DWC_SCDC_CONFIG,   data32);

	data32  = 0;
	data32 |= 0 << 8;  /* [31:8]   manufacture_oui */
	data32 |= 1	<< 0;  /* [7:0]    sink_version */
	hdmirx_wr_dwc(DWC_SCDC_WRDATA0,	data32);

	data32  = 0;
	data32 |= 10	<< 20; /* [29:20]  chlock_max_err */
	data32 |= 24000	<< 0;  /* [15:0]   mili_sec_timer_limit */
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

	data32  = 0;
	data32 |= 1	<< 16;
	data32 |= 0	<< 0;
	hdmirx_wr_dwc(DWC_AUD_FIFO_CTRL, data32);

	data32  = 0;
	data32 |= 0	<< 8;
	data32 |= 0	<< 7;
	data32 |= 0 << 2;
	data32 |= 1	<< 0;
	hdmirx_wr_dwc(DWC_AUD_CHEXTR_CTRL, data32);

	data32 = 0;
	/* [22:21]	a port_sh_dw_ctrl */
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
	/* [2:1]    aud_t_tone_fs_sel */
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
	if (rx.chip_id < CHIP_ID_TM2)
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
	usleep_range(1000, 1010);

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

	/* clear clk_rate cfg */
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

	if (rx_get_eq_run_state() == E_EQ_START)
		ret = false;
	return ret;
}

void aml_phy_offset_cal(void)
{
	if (rx.phy_ver >= PHY_VER_T7)
		aml_phy_offset_cal_t7();
	else if (rx.phy_ver == PHY_VER_T5)
		aml_phy_offset_cal_t5();
}

/*
 * rx_clkrate_monitor - clock ratio monitor
 * detect SCDC tmds clk ratio changes and
 * update phy setting
 */
bool rx_clk_rate_monitor(void)
{
	u32 clk_rate, phy_band, pll_band;
	bool changed = false;
	int i;
	int error = 0;

	clk_rate = rx_get_scdc_clkrate_sts();
	/* should rm squelch judgement for low-amplitude issue */
	/* otherwise,sw can not detect the low-amplitude signal */
	/* if (rx.state < FSM_WAIT_CLK_STABLE) */
		/*return changed;*/
	/*if (is_clk_stable()) { */
	pll_band = aml_phy_pll_band(rx.clk.cable_clk, clk_rate);
	phy_band = aml_cable_clk_band(rx.clk.cable_clk, clk_rate);
	if (rx.phy.pll_bw != pll_band ||
	    rx.phy.phy_bw != phy_band) {
		rx.phy.cablesel = 0;
		rx.phy.phy_bw = phy_band;
		rx.phy.pll_bw = pll_band;
	}
	/* } */

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
			hdmirx_phy_init();
		}
		if (log_level & VIDEO_LOG)
			rx_pr("clk_rate:%d, last_clk_rate: %d\n",
			      clk_rate, rx.phy.clk_rate);
		rx.phy.clk_rate = clk_rate;
	}
	if (changed) {
		rx.cableclk_stb_flg = false;
		//if (rx.state >= FSM_WAIT_CLK_STABLE)
			//rx.state = FSM_WAIT_CLK_STABLE;
		i2c_err_cnt = 0;
	}
	return changed;
}

static void hdmirx_cor_reset(void)
{
	ulong flags;
	unsigned long dev_offset = 0;

	if (rx.chip_id < CHIP_ID_T7)
		return;
	spin_lock_irqsave(&reg_rw_lock, flags);
	dev_offset = rx_reg_maps[MAP_ADDR_MODULE_TOP].phy_addr;
	wr_reg(MAP_ADDR_MODULE_TOP, dev_offset + TOP_SW_RESET, 1);
	udelay(1);
	wr_reg(MAP_ADDR_MODULE_TOP, dev_offset + TOP_SW_RESET, 0);
	spin_unlock_irqrestore(&reg_rw_lock, flags);
}

void rx_afifo_monitor(void)
{
	if (rx.state != FSM_SIG_READY)
		return;

	if (rx.chip_id < CHIP_ID_T7) {
		if (rx.aud_info.auds_layout || rx.aud_info.aud_hbr_rcv) {
			if (rx.aud_info.afifo_cfg) {
				dump_audio_status();
				rx_afifo_store_valid(false);
				hdmirx_audio_fifo_rst();
			}
		} else {
			if (!rx.aud_info.afifo_cfg) {
				dump_audio_status();
				rx_afifo_store_valid(true);
				hdmirx_audio_fifo_rst();
			}
		}
		return;
	}
	if (rx_afifo_dbg_en) {
		afifo_overflow_cnt = 0;
		afifo_underflow_cnt = 0;
		return;
	}

	rx.afifo_sts = hdmirx_rd_cor(RX_INTR4_PWD_IVCRX) & 3;
	hdmirx_wr_cor(RX_INTR4_PWD_IVCRX, 3);
	if (rx.afifo_sts & 2) {
		afifo_overflow_cnt++;
		hdmirx_audio_fifo_rst();
		if (log_level & AUDIO_LOG)
			rx_pr("overflow\n");
	} else if (rx.afifo_sts & 1) {
		afifo_underflow_cnt++;
		hdmirx_audio_fifo_rst();
		if (log_level & AUDIO_LOG)
			rx_pr("underflow\n");
	} else {
		if (afifo_overflow_cnt)
			afifo_overflow_cnt--;
		if (afifo_underflow_cnt)
			afifo_underflow_cnt--;
	}
	if (afifo_overflow_cnt > 600) {
		afifo_overflow_cnt = 0;
		hdmirx_output_en(false);
		hdmirx_cor_reset();
		//hdmirx_hbr2spdif(0);
		//rx_set_cur_hpd(0, 5);
		//rx.state = FSM_5V_LOST;
		rx_pr("!!force reset\n");
	}
	//if (afifo_underflow_cnt) {
		//afifo_underflow_cnt = 0;
		//rx_aud_fifo_rst();
		//rx_pr("!!pll rst\n");
	//}
}

void rx_hdcp_monitor(void)
{
	static u8 sts1, sts2, sts3;
	u8 tmp;

	if (rx.chip_id < CHIP_ID_T7)
		return;
	if (rx.hdcp.hdcp_version == HDCP_VER_NONE)
		return;
	if (rx.state < FSM_SIG_STABLE)
		return;

	rx_get_ecc_info();
	if (rx.ecc_err && rx.ecc_pkt_cnt == rx.ecc_err) {
		if (log_level & VIDEO_LOG)
			rx_pr("ecc:%d-%d\n", rx.ecc_err,
				  rx.ecc_pkt_cnt);
		skip_frame(1);
		rx.ecc_err_frames_cnt++;
	} else {
		rx.ecc_err_frames_cnt = 0;
	}
	if (rx.ecc_err_frames_cnt >= rx_ecc_err_frames) {
		if (rx.hdcp.hdcp_version == HDCP_VER_22)
			rx_hdcp_22_sent_reauth();
		else if (rx.hdcp.hdcp_version == HDCP_VER_14)
			rx_hdcp_14_sent_reauth();
		rx_pr("reauth-err:%d\n", rx.ecc_err);
		rx.state = FSM_SIG_WAIT_STABLE;
		rx.ecc_err = 0;
		rx.ecc_err_frames_cnt = 0;
	}
	//hdcp14 status
	tmp = hdmirx_rd_cor(RX_HDCP_STAT_HDCP1X_IVCRX);
	if (tmp == 2 || tmp == 0x0a) {
		rx_pr("hdcp1sts %x->%x\n", sts1, tmp);
		sts1 = tmp;
	}
	tmp = hdmirx_rd_cor(CP2PAX_AUTH_STAT_HDCP2X_IVCRX);
	if (tmp != sts2 && (log_level & HDCP_LOG)) {
		rx_pr("hdcp2sts %x->%x\n", sts2, tmp);
		sts2 = tmp;
	}
	tmp = hdmirx_rd_cor(CP2PAX_STATE_HDCP2X_IVCRX);
	if (tmp != sts3 && (log_level & HDCP_LOG)) {
		rx_pr("hdcp2sts3 %x->%x\n", sts3, tmp);
		sts3 = tmp;
	}
}

bool rx_special_func_en(void)
{
	bool ret = false;

	if (rx.chip_id <= CHIP_ID_T7)
		return ret;

#ifdef CVT_DEF_FIXED_HPD_PORT
	if (rx.port == E_PORT0 && ((CVT_DEF_FIXED_HPD_PORT & (1 << E_PORT0)) != 0))
		ret = true;

	if (rx.boot_flag && rx.port == E_PORT0) {
		if (hdmirx_rd_cor(SCDCS_TMDS_CONFIG_SCDC_IVCRX) & 2)
			ret = true;
		//no hdcp
		rx_pr("pc port first boot\n");
	}
#endif

	return ret;
}

bool rx_sw_scramble_en(void)
{
	bool ret = false;

	if (rx.port == ops_port) {
		ret = true;
		rx_pr("ops port in\n");
	}

	return ret;
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

/* need reset bandgap when
 * aud_clk=0 & req_clk!=0
 * according to analog team's request
 */
void rx_audio_bandgap_rst(void)
{
	if (rx.chip_id >= CHIP_ID_TL1) {
		wr_reg_hhi_bits(HHI_VDAC_CNTL0, _BIT(10), 1);
		udelay(10);
		wr_reg_hhi_bits(HHI_VDAC_CNTL0, _BIT(10), 0);
	} else {
		wr_reg_hhi_bits(HHI_VDAC_CNTL0_TXLX, _BIT(13), 1);
		udelay(10);
		wr_reg_hhi_bits(HHI_VDAC_CNTL0_TXLX, _BIT(13), 0);
	}
	if (log_level & AUDIO_LOG)
		rx_pr("%s\n", __func__);
}

void rx_audio_bandgap_en(void)
{
	if (rx.chip_id >= CHIP_ID_TL1) {
		/* for tl1/tm2 0:bg on   1: bg off */
		wr_reg_hhi_bits(HHI_VDAC_CNTL1, _BIT(7), 0);

		wr_reg_hhi_bits(HHI_VDAC_CNTL0, _BIT(10), 1);
		udelay(10);
		wr_reg_hhi_bits(HHI_VDAC_CNTL0, _BIT(10), 0);
	} else {
		/* for txlx/txl... 1:bg on   0: bg off */
		wr_reg_hhi_bits(HHI_VDAC_CNTL0_TXLX, _BIT(9), 1);

		wr_reg_hhi_bits(HHI_VDAC_CNTL0_TXLX, _BIT(13), 1);
		udelay(10);
		wr_reg_hhi_bits(HHI_VDAC_CNTL0_TXLX, _BIT(13), 0);
	}
	if (log_level & AUDIO_LOG)
		rx_pr("%s\n", __func__);
}

void rx_sw_reset(int level)
{
	unsigned long data32 = 0;

	if (rx.chip_id >= CHIP_ID_T7) {
		rx_sw_reset_t7(level);
	} else {
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
		hdmirx_audio_fifo_rst();
		hdmirx_packet_fifo_rst();
	}
}

void rx_esm_reset(int level)
{
	if (rx.chip_id >= CHIP_ID_T7 || !hdcp22_on)
		return;
	if (level == 0) {//for state machine
		esm_set_stable(false);
		if (esm_recovery_mode
			== ESM_REC_MODE_RESET)
			esm_set_reset(true);
		/* for some hdcp2.2 devices which
		 * don't retry 2.2 interaction
		 * continuously and don't response
		 * to re-auth, such as chroma 2403,
		 * esm needs to be on work even
		 * before tmds is valid so that to
		 * not miss 2.2 interaction
		 */
		/* else */
			/* rx_esm_tmds_clk_en(false); */
	} else if (level == 1) {//for open port
		esm_set_stable(false);
		esm_set_reset(true);
		if (esm_recovery_mode == ESM_REC_MODE_TMDS)
			rx_esm_tmds_clk_en(false);
	} else if (level == 2) {//for fsm_restart&signal_status_init
		if (esm_recovery_mode == ESM_REC_MODE_TMDS)
			rx_esm_tmds_clk_en(false);
		esm_set_stable(false);
	} else if (level == 3) {//for dwc_reset
		if (rx.hdcp.hdcp_version != HDCP_VER_14) {
			if (esm_recovery_mode == ESM_REC_MODE_TMDS)
				rx_esm_tmds_clk_en(true);
			esm_set_stable(true);
			if (rx.hdcp.hdcp_version == HDCP_VER_22)
				hdmirx_hdcp22_reauth();
		}
	}
}

void cor_init(void)
{
	u8 data8;
	u32 data32;

	//--------AON REG------
	data8  = 0;
	data8 |= (0 << 4);// [4]   reg_sw_rst_auto
	data8 |= (1 << 0);// [0]   reg_sw_rst
	hdmirx_wr_cor(RX_AON_SRST, data8);//register address: 0x0005

	data8  = 0;
	data8 |= (0 << 5);// [5]  reg_scramble_on_ovr
	data8 |= (1 << 4);// [4]  reg_hdmi2_on_ovr
	data8 |= (0 << 2);// [3:2]rsvd
	data8 |= (0 << 1);// [1]  reg_scramble_on_val
	data8 |= (1 << 0);// [0]  reg_hdmi2_on_val
	hdmirx_wr_cor(RX_HDMI2_MODE_CTRL, data8);//register address: 0x0040

	data8  = 0;
	data8 |= (0 << 3);// [3] reg_soft_intr_en
	data8 |= (0 << 1);// [1] reg_intr_polarity (default is 1)
	hdmirx_wr_cor(RX_INT_CTRL, data8);//register address: 0x0079

	//-------PWD REG-------
	data8  = 0;
	data8 |= (0 << 7);// [7]  reg_mhl3ce_sel_rx
	data8 |= (0 << 6);// [6]  rsvd
	data8 |= (0 << 5);// [5]  reg_vdi_rx_dig_bypass
	data8 |= (0 << 4);// [4]  reg_tmds_mode_inv
	data8 |= (0 << 3);// [3]  reg_bypass_rx2tx_dsc_video
	data8 |= (0 << 2);// [2]  rsvd
	data8 |= (0 << 1);// [1]  rsvd
	data8 |= (0 << 0);// [0]  reg_core_iso_en  TMDS core isolation enable
	hdmirx_wr_cor(RX_PWD_CTRL, data8);//register address: 0x1001

	data8  = 0;
	data8 |= (0 << 3);// [4:3] reg_dsc_bypass_align
	data8 |= (0 << 2);// [2]   reg_hv_sync_cntrl
	data8 |= (1 << 1);// [1]   reg_sync_pol
	data8 |= (1 << 0);// [0]   reg_pd_all   0:power down; 1: normal operation
	hdmirx_wr_cor(RX_SYS_CTRL1, data8);//register address: 0x1007

	data8  = 0;
	data8 |= (1 << 4);// [5:4]	reg_di_ch2_sel
	data8 |= (0 << 2);// [3:2]	reg_di_ch1_sel
	data8 |= (2 << 0);// [1:0]	reg_di_ch0_sel
	hdmirx_wr_cor(RX_SYS_TMDS_CH_MAP, data8);//register address: 0x100E

	data8 = 0;
	data8 |= (0 << 7);//rsvd
	data8 |= (1 << 6);//reg_phy_di_dff_en : enable for dff latching data coming from TMDS phy
	data8 |= (0 << 5);//reg_di_ch2_invt
	data8 |= (1 << 4);//reg_di_ch1_invt
	data8 |= (0 << 3);//reg_di_ch0_invt
	data8 |= (0 << 2);//reg_di_ch2_bsi
	data8 |= (1 << 1);//reg_di_ch1_bsi
	data8 |= (0 << 0);//reg_di_ch0_bsi
	hdmirx_wr_cor(RX_SYS_TMDS_D_IR, data8);//register address: 0x100F

	/* deep color clock source */
	data8 = 0;
	data8 |= (0 << 6);//[7:6] reg_pp_status
	data8 |= (1 << 5);//[5]   reg_offset_coen
	data8 |= (0 << 4);//[4]   reg_dc_ctl_ow  //!!!!
	data8 |= (0 << 0);//[3:0] reg_dc_ctl  deep-color clock from the TMDS RX core
	hdmirx_wr_cor(RX_TMDS_CCTRL2, data8);//register address: 0x1013

	data8  = 0;
	data8 |= (1 << 7);//reg_tst_x_clk 1:Crystal oscillator clock muxed to test output pin
	data8 |= (0 << 6);//reg_tst_ckdt 1:CKDT muxed to test output pin
	data8 |= (0 << 5);//reg_invert_tclk
	hdmirx_wr_cor(RX_TEST_STAT, data8);//register address: 0x103b (0x80)

	data8  = 0;
	data8 |= (0 << 3);//[5:3] divides the vpc out clock
	//[2:0] divides the vpc core clock:
	//0: divide by 1; 1: divide by 2; 3: divide by 4; 7: divide by 8
	data8 |= (1 << 0);
	hdmirx_wr_cor(RX_PWD0_CLK_DIV_0, data8) ;//register address: 0x10c1

	data8 = 0;
	data8 |= (0 << 7);// [  7] cbcr_order
	data8 |= (0 << 6);// [  6] yc_demux_polarity
	data8 |= (0 << 5);// [  5] yc_demux_enable
	hdmirx_wr_cor(RX_VP_INPUT_FORMAT_LO, data8);

	data8  = 0;
	data8 |= (0 << 3);// [  3] mux_cb_or_cr
	data8 |= (0 << 2);// [  2] mux_420_enable
	data8 |= (0 << 0);// [1:0] input_pixel_rate
	hdmirx_wr_cor(RX_VP_INPUT_FORMAT_HI, data8);

	//===hdcp 1.4 needed
	hdmirx_wr_cor(RX_SW_HDMI_MODE_PWD_IVCRX, 0x04);//register address: 0x1022

	//[0] reg_ext_mclk_en; 1 select external mclk; 0: select internal dacr mclk
	hdmirx_wr_cor(EXT_MCLK_SEL_PWD_IVCRX, 0x01);//register address: 0x10c6

	//DEPACK
	data8  = 0;
	data8 |= (1 << 4);//[4] reg_wait4_two_avi_pkts  default is 1, but need two packet
	data8 |= (0 << 0);//[0] reg_all_if_clr_en
	hdmirx_wr_cor(VSI_CTRL4_DP3_IVCRX, data8);   //register_address: 0x120f

	// ------PHY CLK/RST-----
	hdmirx_wr_cor(PWD0_CLK_EN_1_PHYCK_IVCRX, 0xff);//register address: 0x20a3
	hdmirx_wr_cor(PWD0_CLK_EN_2_PHYCK_IVCRX, 0x3f);//register address: 0x20a4
	hdmirx_wr_cor(PWD0_CLK_EN_3_PHYCK_IVCRX, 0x01);//register address: 0x20a5

	hdmirx_wr_cor(RX_AON_SRST_AON_IVCRX, 0x00);//reset
	hdmirx_wr_cor(RX_PWD_INT_CTRL, 0x00);//[1] reg_intr_polarity, default = 1
	//-------------------
	//  vp core config
	//-------------------
	//invecase Rx 422 output directly
	//mux 422 12bit*2 in to 8bit*3 out
	data8  = 0;
	data8 |= (0	   << 3);// de_polarity
	data8 |= (0	   << 2);// rsvd
	data8 |= (0	   << 1);// hsync_polarity
	data8 |= (0	   << 0);// vsync_polarity
	hdmirx_wr_cor(VP_OUTPUT_SYNC_CFG_VID_IVCRX, data8);//register address: 0x1842

	data32 = 0;
	//data32 |= (((rx_color_format==HDMI_COLOR_FORMAT_422)?3:2)   << 9);
	data32 |= (2 << 9);
	// [11: 9] select_cr: 0=ch1(Y); 1=ch0(Cb); 2=ch2(Cr); 3={ch2 8-b,ch0 4-b}(422).
	//data32 |= (((rx_color_format==HDMI_COLOR_FORMAT_422)?3:1)   << 6);
	data32 |= (1 << 6);
	// [ 8: 6] select_cb: 0=ch1(Y); 1=ch0(Cb); 2=ch2(Cr); 3={ch2 8-b,ch0 4-b}(422).
	//data32 |= (((rx_color_format==HDMI_COLOR_FORMAT_422)?3:0)   << 3);
	data32 |= (0 << 3);
	// [ 5: 3] select_y : 0=ch1(Y); 1=ch0(Cb); 2=ch2(Cr); 3={ch1 8-b,ch0 4-b}(422).
	data32 |= (0 << 2);// [    2] reverse_cr
	data32 |= (0 << 1);// [    1] reverse_cb
	data32 |= (0 << 0);// [    0] reverse_y
	hdmirx_wr_cor(VP_INPUT_MAPPING_VID_IVCRX, data32 & 0xff);
	hdmirx_wr_cor(VP_INPUT_MAPPING_VID_IVCRX + 1, (data32 >> 8) & 0xff);

	//------------------
	// audio I2S config
	//------------------
	data8 = 0;
	data8 |= (5 << 4);//reg_vres_xclk_diff
	data8 |= (0 << 0);//reg_vid_xlckpclk_en
	hdmirx_wr_cor(VID_XPCLK_EN_AUD_IVCRX, data8);//register address: 0x1468 (0x50)

	data8 = 0xc; //[5:0] reg_post_val_sw
	hdmirx_wr_cor(RX_POST_SVAL_AUD_IVCRX, data8);//register address: 0x1411 (0x0c)

	data8 = 0;
	data8 |= (1 << 6);//[7:6] reg_fm_val_sw 0:128*fs; 1: 256*fs; 2:384*fs; 3: 512*fs
	data8 |= (3 << 0);//[5:0] reg_fs_val_sw
	hdmirx_wr_cor(RX_FREQ_SVAL_AUD_IVCRX, data8);//register address: 0x1402 (0x43)

	hdmirx_wr_cor(RX_UPLL_SVAL_AUD_IVCRX, 0x00);//UPLL_SVAL

	hdmirx_wr_cor(RX_AVG_WINDOW_AUD_IVCRX, 0xff);//AVG_WINDOW

	data8 |= (0 << 7); //[7] cts_dropped_auto_en
	data8 |= (0 << 6); //[6] post_hw_sw_sel
	data8 |= (0 << 5); //[5] upll_hw_sw_sel
	data8 |= (0 << 4); //[4] cts_hw_sw_sel: 0=hw; 1=sw.
	data8 |= (0 << 3); //[3] n_hw_sw_sel: 0=hw; 1=sw.
	data8 |= (0 << 2); //[2] cts_reused_auto_en
	data8 |= (0 << 1); //[1] fs_hw_sw_sel: 0=hw; 1=sw.
	data8 |= (0 << 0); //[0] acr_init_wp
	hdmirx_wr_cor(RX_ACR_CTRL1_AUD_IVCRX, data8);//register address: 0x1400 (0x7a)

	data8 = 0;
	data8 |= (0 << 6);//[7:6] r_hdmi_aud_sample_f_extn
	data8 |= (0 << 4);//[4]   reg_fs_filter_en
	data8 |= (0 << 0);//[3:0] rhdmi_aud_sample_f
	hdmirx_wr_cor(RX_TCLK_FS_AUD_IVCRX, data8);	//register address: 0x1417 (0x0)

	data8 = 0;
	data8 |= (1 << 3);//[6:3] reg_cts_thresh
	data8 |= (1 << 2);//[2] reg_mclk_loopback
	data8 |= (0 << 1);//[1] reg_log_win_ena
	data8 |= (0 << 0);//[0] reg_post_div2_ena
	hdmirx_wr_cor(RX_ACR_CTRL3_AUD_IVCRX, data8);//register address: 0x1418 (0xc)

	data8 = 0;
	data8 |= (1 << 7);//[7]  reg_sd3_en
	data8 |= (1 << 6);//[6]  reg_sd2_en
	data8 |= (1 << 5);//[5]  reg_sd1_en
	data8 |= (1 << 4);//[4]  reg_sd0_en
	data8 |= (0 << 3);//[3]  reg_mclk_en
	data8 |= (1 << 2);//[2]  reg_mute_flag
	data8 |= (0 << 1);//[1]  reg_vucp
	data8 |= (0 << 0);//[0]  reg_pcm
	hdmirx_wr_cor(RX_I2S_CTRL2_AUD_IVCRX, data8);

	data8 = 0;
	data8 |= (3 << 6);//[7:6] reg_sd3_map
	data8 |= (2 << 4);//[5:4] reg_sd2_map
	data8 |= (1 << 2);//[3:2] reg_sd1_map
	//[1:0] reg_sd0_map : 0 from FIFO#1; 1 from FIFO#2; 2:from FIFO#3; 3: from FIFO#3
	data8 |= (0 << 0);
	hdmirx_wr_cor(RX_I2S_MAP_AUD_IVCRX, data8);//register address: 0x1428 (0xe4)

	hdmirx_wr_cor(RX_I2S_CTRL1_AUD_IVCRX, 0x40);//I2S_CTRL1
	hdmirx_wr_cor(RX_SW_OW_AUD_IVCRX, 0x00);//SW_OW

	hdmirx_wr_cor(RX_AUDO_MUTE_AUD_IVCRX, 0x00);//AUDO_MODE
	hdmirx_wr_cor(RX_OW_15_8_AUD_IVCRX, 0x00);//OW_15_8
	hdmirx_wr_cor(AAC_MCLK_SEL_AUD_IVCRX, 0x80);//MCLK_SEL [5:4]=>00:128*Fs;01:256*Fs

	data8 = 0;
	data8 |= (0 << 7);//[7] enable overwrite length ralated cbit
	data8 |= (0 << 6);//[6] enable sine wave
	data8 |= (1 << 5);//[5] enable hardware mute
	data8 |= (1 << 4);//[4] pass spdif error
	data8 |= (1 << 3);//[3] pass aud error
	data8 |= (1 << 2);//[2] reg_i2s_mode
	data8 |= (1 << 1);//[1] reg_spdif_mode
	data8 |= (1 << 0);//[0] reg_spidf_en
	hdmirx_wr_cor(RX_AUDRX_CTRL_AUD_IVCRX, data8);//AUDRX_CTRL

	data8 = 0;
	data8 |= (1 << 1);//[1] dont_clr_sys_intr
	hdmirx_wr_cor(AEC4_CTRL_AUD_IVCRX, data8); //AEC4 CTRL

	data8 = 0;
	data8 |= (1 << 7);//ctl acr en
	data8 |= (1 << 6);//aac exp sel
	data8 |= (0 << 5);//[5] reg_aac_out_off_en
	data8 |= (0 << 2);//[2] reserved
	data8 |= (1 << 1);//[1] aac hw auto unmute enable
	data8 |= (1 << 0);//[0] aac hw auto mute enable
	hdmirx_wr_cor(AEC0_CTRL_AUD_IVCRX, data8); //AEC0 CTRL

	data8 = 0;
	data8 |= (0 << 7);//[7] H resolution change
	data8 |= (0 << 6);//[6] polarity change
	data8 |= (0 << 5);//[5] change of interlaced
	data8 |= (0 << 4);//[4] change of the FS
	data8 |= (0 << 3);//[3] CTS reused
	data8 |= (0 << 2);//[2] audio fifo overrun
	data8 |= (0 << 1);//[1] audio fifo underrun
	data8 |= (0 << 0);//[0] hdmi mode
	hdmirx_wr_cor(RX_AEC_EN2_AUD_IVCRX, data8);//RX_AEC_EN2

	data8 = 0;
	//if(rx_hbr_sel_i2s_spdif == 1){        //hbr_i2s
	//data8 |= (0	      << 4);}
	//else if(rx_hbr_sel_i2s_spdif == 2){   //hbr_spdif
	//data8 |= (0	      << 4);}
	//else{				       //not hbr
	data8 |= (1 << 4);
	//}
	hdmirx_wr_cor(RX_3D_SW_OW2_AUD_IVCRX, data8);//duplicate

	hdmirx_wr_cor(RX_PWD_SRST_PWD_IVCRX, 0x1a);//SRST = 1
	/* BIT0 AUTO RST AUD FIFO when fifo err */
	hdmirx_wr_cor(RX_PWD_SRST_PWD_IVCRX, 0x01);//SRST = 0

	/* TDM cfg */
	hdmirx_wr_cor(RX_TDM_CTRL1_AUD_IVCRX, 0x00);
	hdmirx_wr_cor(RX_TDM_CTRL2_AUD_IVCRX, 0x10);

	//clr gcp wr; disable hw avmute
	hdmirx_wr_cor(DEC_AV_MUTE_DP2_IVCRX, 0x20);

	// hdcp 2x ECC detection enable  mode 3
	hdmirx_wr_cor(HDCP2X_RX_ECC_CTRL, 3);
	hdmirx_wr_cor(HDCP2X_RX_ECC_CONS_ERR_THR, 50);
	hdmirx_wr_cor(HDCP2X_RX_ECC_FRM_ERR_THR_0, 0x1);
	hdmirx_wr_cor(HDCP2X_RX_ECC_FRM_ERR_THR_1, 0x0);
	//hdmirx_wr_cor(HDCP2X_RX_ECC_GVN_FRM_ERR_THR_2, 0xff);
	//hdmirx_wr_cor(HDCP2X_RX_GVN_FRM, 30);

	//DPLL
	hdmirx_wr_cor(DPLL_CFG6_DPLL_IVCRX, 0x10);
	hdmirx_wr_cor(DPLL_HDMI2_DPLL_IVCRX, 0);

	data8 =  0;
	data8 |= (1 << 4);//acp clr
	data8 |= (1 << 3);//unrec clr
	data8 |= (1 << 2);//mpeg clr
	data8 |= (1 << 1);//spd clr
	data8 |= (1 << 0);//avi clr
	hdmirx_wr_cor(IF_CTRL1_DP3_IVCRX, data8);

	hdmirx_wr_cor(HDMI2_MODE_CTRL_AON_IVCRX, 0x11);
}

void hdmirx_hbr2spdif(u8 val)
{
	/* if (rx.chip_id < CHIP_ID_T7) */
	return;

	if (hdmirx_rd_cor(RX_AUDP_STAT_DP2_IVCRX) & _BIT(6))
		hdmirx_wr_bits_top(TOP_CLK_CNTL, _BIT(15), val);
	else
		hdmirx_wr_bits_top(TOP_CLK_CNTL, _BIT(15), 0);
}

void hdmirx_output_en(bool en)
{
	if (rx.chip_id < CHIP_ID_T7)
		return;
	if (en)
		hdmirx_wr_top(TOP_OVID_OVERRIDE0, 0);
	else
		hdmirx_wr_top(TOP_OVID_OVERRIDE0, 0x80000000);
}

void hdmirx_hw_config(void)
{
	rx_pr("%s port:%d\n", __func__, rx.port);
	hdmirx_top_sw_reset();
	hdmirx_output_en(false);
	if (rx.chip_id >= CHIP_ID_T7) {
		cor_init();
	} else {
		control_reset();
		rx_hdcp_init();
		hdmirx_audio_init();
		//packet_init();
		if (rx.chip_id != CHIP_ID_TXHD)
			hdmirx_20_init();
		DWC_init();
	}
	hdmirx_irq_hdcp_enable(false);
	if (rx.chip_id >= CHIP_ID_T7)
		hdcp_init_t7();
	packet_init();
	if (rx.chip_id >= CHIP_ID_TL1)
		aml_phy_switch_port();
	hdmirx_phy_init();
	//if (rx.chip_id < CHIP_ID_T7)
		//hdmirx_top_irq_en(1, 2);
	rx_pr("%s  %d Done!\n", __func__, rx.port);
	/* hdmi reset will cause cec not working*/
	/* cec modult need reset */
	if (rx.chip_id <= CHIP_ID_TXL)
		cec_hw_reset(1);
	pre_port = 0xff;
}

/*
 * hdmirx_hw_probe - hdmirx top/controller/phy init
 */
void hdmirx_hw_probe(void)
{
	if (rx.chip_id < CHIP_ID_T7)
		hdmirx_wr_top(TOP_MEM_PD, 0);
	hdmirx_top_irq_en(0, 0);
	hdmirx_wr_top(TOP_SW_RESET, 0);
	clk_init();
	TOP_init();
	hdmirx_top_sw_reset();
	if (rx.chip_id >= CHIP_ID_T7) {
		cor_init();
	} else {
		control_reset();
		DWC_init();
		hdcp22_clk_en(1);
		hdmirx_audio_init();
		//packet_init();
		if (rx.chip_id != CHIP_ID_TXHD)
			hdmirx_20_init();
		}
	rx_emp_to_ddr_init();
	hdmi_rx_top_edid_update();
	if (rx.chip_id >= CHIP_ID_TL1)
		aml_phy_switch_port();

	/* for t5,offset_cal also did some phy & pll init operation*/
	/* dont need to do phy init again */
	if (rx.phy_ver >= PHY_VER_T5)
		aml_phy_offset_cal();
	else
		hdmirx_phy_init();
	if (rx.chip_id >= CHIP_ID_T7)
		hdcp_init_t7();
	hdmirx_wr_top(TOP_PORT_SEL, 0x10);
	hdmirx_wr_top(TOP_INTR_STAT_CLR, ~0);
	//if (rx.chip_id < CHIP_ID_T7)
		//hdmirx_top_irq_en(1, 2);
	rx_pr("%s Done!\n", __func__);
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
	if (rx.chip_id >= CHIP_ID_T7)
		return;

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
bool is_aud_fifo_error(void)
{
	bool ret = false;

	if (rx.chip_id >= CHIP_ID_T7)
		return ret;

	if ((hdmirx_rd_dwc(DWC_AUD_FIFO_STS) &
		(OVERFL_STS | UNDERFL_STS)) &&
		rx.aud_info.aud_packet_received) {
		ret = true;
		if (log_level & DBG_LOG)
			rx_pr("afifo err\n");
	}
	return ret;
}

/*
 * is_aud_pll_error - audio clock range detection
 * normal mode: aud_pll = aud_sample_rate * 128
 * HBR: aud_pll = aud_sample_rate * 128 * 4
 *
 * return true if audio clock is in range, false otherwise.
 */
bool is_aud_pll_error(void)
{
	bool ret = true;
	u32 clk = rx.aud_info.aud_clk;
	u32 aud_128fs = rx.aud_info.real_sr * 128;
	u32 aud_512fs = rx.aud_info.real_sr * 512;

	if (rx.chip_id >= CHIP_ID_T7)
		return false;
	if (rx.aud_info.real_sr == 0)
		return false;
	if (abs(clk - aud_128fs) < AUD_PLL_THRESHOLD ||
	    abs(clk - aud_512fs) < AUD_PLL_THRESHOLD)
		ret = false;
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
		if (rx.chip_id == CHIP_ID_T7) {
			if (en) {
				tmp = rd_reg_clk_ctl(RX_CLK_CTRL2);
				tmp |= (1 << 8);// [    8] clk_en for cts_hdmirx_aud_pll_clk
				wr_reg_clk_ctl(RX_CLK_CTRL2, tmp);
				/* AUD_CLK=N/CTS*TMDS_CLK */
				hdmirx_wr_amlphy(HHI_AUD_PLL_CNTL_T7, 0x40001540);
				/* use mpll */
				tmp = 0;
				tmp |= 2 << 2; /* 0:tmds_clk 1:ref_clk 2:mpll_clk */
				hdmirx_wr_amlphy(HHI_AUD_PLL_CNTL2_T7, tmp);
				/* cntl3 2:0 000=1*cts 001=2*cts 010=4*cts 011=8*cts */
				hdmirx_wr_amlphy(HHI_AUD_PLL_CNTL3_T7, rx.phy.aud_div);
				if (log_level & AUDIO_LOG)
					rx_pr("aud div=%d\n", rd_reg_hhi(HHI_AUD_PLL_CNTL3));
				hdmirx_wr_amlphy(HHI_AUD_PLL_CNTL_T7, 0x60001540);
				if (log_level & AUDIO_LOG)
					rx_pr("audio pll lock:0x%x\n",
						  hdmirx_rd_amlphy(HHI_AUD_PLL_CNTL_I_T7));
				rx_audio_pll_sw_update();
			} else {
				/* disable pll, into reset mode */
				hdmirx_wr_amlphy(HHI_AUD_PLL_CNTL_T7, 0x0);
				tmp = rd_reg_clk_ctl(RX_CLK_CTRL2);
				tmp &= ~(1 << 8);// [    8] clk_en for cts_hdmirx_aud_pll_clk
				wr_reg_clk_ctl(RX_CLK_CTRL2, tmp);
			}
		} else if (rx.chip_id == CHIP_ID_T3) {
			if (en) {
				tmp = rd_reg_clk_ctl(RX_CLK_CTRL2);
				tmp |= (1 << 8);// [    8] clk_en for cts_hdmirx_aud_pll_clk
				wr_reg_clk_ctl(RX_CLK_CTRL2, tmp);
				/* AUD_CLK=N/CTS*TMDS_CLK */
				wr_reg_ana_ctl(ANACTL_AUD_PLL_CNTL, 0x40001540);
				/* use mpll */
				tmp = 0;
				tmp |= 2 << 2; /* 0:tmds_clk 1:ref_clk 2:mpll_clk */
				wr_reg_ana_ctl(ANACTL_AUD_PLL_CNTL2, tmp);
				/* cntl3 2:0 000=1*cts 001=2*cts 010=4*cts 011=8*cts */
				wr_reg_ana_ctl(ANACTL_AUD_PLL_CNTL3, rx.phy.aud_div);
				if (log_level & AUDIO_LOG)
					rx_pr("aud div=%d\n", rd_reg_ana_ctl(ANACTL_AUD_PLL_CNTL3));
				wr_reg_ana_ctl(ANACTL_AUD_PLL_CNTL, 0x60001540);
				if (log_level & AUDIO_LOG)
					/* t3 audio pll lock bit: top reg acr_cntl_stat bit'31 */
					rx_pr("audio pll lock:0x%x\n",
						  (hdmirx_rd_top(TOP_ACR_CNTL_STAT) >> 31));
				rx_audio_pll_sw_update();
			} else {
				/* disable pll, into reset mode */
				wr_reg_ana_ctl(ANACTL_AUD_PLL_CNTL, 0x0);
				tmp = rd_reg_clk_ctl(RX_CLK_CTRL2);
				tmp &= ~(1 << 8);// [    8] clk_en for cts_hdmirx_aud_pll_clk
				wr_reg_clk_ctl(RX_CLK_CTRL2, tmp);
			}
		} else if (rx.chip_id == CHIP_ID_T5W) {
			if (en) {
				tmp = rd_reg_clk_ctl(RX_CLK_CTRL2_T5W);
				/* [    8] clk_en for cts_hdmirx_aud_pll_clk */
				tmp |= (1 << 8);
				wr_reg_clk_ctl(RX_CLK_CTRL2_T5W, tmp);
				/* AUD_CLK=N/CTS*TMDS_CLK */
				wr_reg_hhi(HHI_AUD_PLL_CNTL, 0x40001540);
				/* use mpll */
				tmp = 0;
				/* 0:tmds_clk 1:ref_clk 2:mpll_clk */
				tmp |= 2 << 2;
				wr_reg_hhi(HHI_AUD_PLL_CNTL2, tmp);
				/* cntl3 2:0 0=1*cts 1=2*cts */
				/* 010=4*cts 011=8*cts */
				wr_reg_hhi(HHI_AUD_PLL_CNTL3, rx.phy.aud_div);
				if (log_level & AUDIO_LOG)
					rx_pr("aud div=%d\n",
					rd_reg_hhi(ANACTL_AUD_PLL_CNTL3));
				wr_reg_hhi(HHI_AUD_PLL_CNTL, 0x60001540);
				if (log_level & AUDIO_LOG) {
					/* pll lock bit:*/
					/*top reg acr_cntl_stat bit'31 */
					tmp = hdmirx_rd_top(TOP_ACR_CNTL_STAT);
					rx_pr("apll lock:0x%x\n", (tmp >> 31));
				}
				rx_audio_pll_sw_update();
			} else {
				/* disable pll, into reset mode */
				wr_reg_ana_ctl(ANACTL_AUD_PLL_CNTL, 0x0);
				tmp = rd_reg_clk_ctl(RX_CLK_CTRL2);
				/* [    8] clk_en for cts_hdmirx_aud_pll_clk */
				tmp &= ~(1 << 8);
				wr_reg_clk_ctl(RX_CLK_CTRL2, tmp);
			}
		} else {
			if (en) {
				/* AUD_CLK=N/CTS*TMDS_CLK */
				wr_reg_hhi(HHI_AUD_PLL_CNTL, 0x40001540);
				/* use mpll */
				tmp = 0;
				tmp |= 2 << 2; /* 0:tmds_clk 1:ref_clk 2:mpll_clk */
				wr_reg_hhi(HHI_AUD_PLL_CNTL2, tmp);
				/* cntl3 2:0 000=1*cts 001=2*cts 010=4*cts 011=8*cts */
				wr_reg_hhi(HHI_AUD_PLL_CNTL3, rx.phy.aud_div);

				if (log_level & AUDIO_LOG)
					rx_pr("aud div=%d\n", rd_reg_hhi(HHI_AUD_PLL_CNTL3));
				wr_reg_hhi(HHI_AUD_PLL_CNTL, 0x60001540);
				if (log_level & AUDIO_LOG)
					rx_pr("audio pll lock:0x%x\n",
					      rd_reg_hhi(HHI_AUD_PLL_CNTL_I));
				/*rx_audio_pll_sw_update();*/
			} else {
				/* disable pll, into reset mode */
				wr_reg_hhi(HHI_AUD_PLL_CNTL, 0x0);
			}
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
		} else {
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
bool rx_get_dvi_mode(void)
{
	u32 ret;

	if (rx.chip_id >= CHIP_ID_T7)
		ret = hdmirx_rd_cor(RX_AUDP_STAT_DP2_IVCRX) & 1;
	else
		ret = !hdmirx_rd_bits_dwc(DWC_PDEC_STS, DVIDET);
	if (ret)
		return false;
	else
		return true;
}

u8 rx_get_hdcp_type(void)
{
	u32 tmp;
	u8 data_dec, data_auth;

	if (rx.chip_id >= CHIP_ID_T7) {
		//
		//get from irq_handler
		//
		data_auth = hdmirx_rd_cor(CP2PAX_AUTH_STAT_HDCP2X_IVCRX);
		data_dec = hdmirx_rd_cor(RX_HDCP_STATUS_PWD_IVCRX);
		rx.cur.hdcp14_state = (hdmirx_rd_cor(RX_HDCP_STAT_HDCP1X_IVCRX) >> 4) & 3;
		rx.cur.hdcp22_state = ((data_dec & 1) << 1) | (data_auth & 1);
		//if (rx.cur.hdcp22_state & 3 && rx.cur.hdcp14_state != 3)
			//rx.hdcp.hdcp_version = HDCP_VER_22;
		//else if (rx.cur.hdcp14_state == 3 && rx.cur.hdcp22_state != 3)
			//rx.hdcp.hdcp_version = HDCP_VER_14;
		//else
			//rx.hdcp.hdcp_version = HDCP_VER_NONE;
	} else {
		if (hdcp22_on) {
			tmp = hdmirx_rd_dwc(DWC_HDCP22_STATUS);
			rx.cur.hdcp_type = (tmp >> 4) & 1;
			rx.cur.hdcp22_state = tmp & 1;
		}
		if (!rx.cur.hdcp_type)
			rx.cur.hdcp14_state = (hdmirx_rd_dwc(DWC_HDCP_STS) >> 8) & 3;
	}
	return 1;
}

/*
 * rx_get_hdcp_auth_sts
 */
int rx_get_hdcp_auth_sts(void)
{
	bool ret = 0;
	int hdcp22_status;

	if (rx.state < FSM_SIG_READY)
		return ret;

	if (rx.chip_id <= CHIP_ID_T5D)
		hdcp22_status = (rx.cur.hdcp22_state == 3) ? 1 : 0;
	else
		hdcp22_status = rx.cur.hdcp22_state & 1;

	if ((rx.hdcp.hdcp_version == HDCP_VER_14 && rx.cur.hdcp14_state == 3) ||
		(rx.hdcp.hdcp_version == HDCP_VER_22 && hdcp22_status))
		ret = 1;
	return ret;
}

void rx_get_avi_params(void)
{
	u8 data8, data8_lo, data8_up;

	if (rx.chip_id >= CHIP_ID_T7) {
		/*byte 1*/
		data8 = hdmirx_rd_cor(AVIRX_DBYTE1_DP2_IVCRX);
		/*byte1:bit[7:5]*/
		rx.cur.colorspace = (data8 >> 5) & 0x07;
		/*byte 1 bit'4*/
		rx.cur.active_valid = (data8 >> 4) & 0x01;
		/*byte1:bit[3:2]*/
		rx.cur.bar_valid = (data8 >> 2) & 0x03;

		/*byte1:bit[1:0]*/
		rx.cur.scan_info = data8 & 0x3;

		/*byte 2*/
		data8 = hdmirx_rd_cor(AVIRX_DBYTE2_DP2_IVCRX);
		/*byte1:bit[7:6]*/
		rx.cur.colorimetry = (data8 >> 6) & 0x3;
		/*byte1:bit[5:4]*/
		rx.cur.picture_ratio = (data8 >> 4) & 0x3;
		/*byte1:bit[3:0]*/
		rx.cur.active_ratio = data8 & 0xf;

		/*byte 3*/
		data8 = hdmirx_rd_cor(AVIRX_DBYTE3_DP2_IVCRX);
		/* byte3 bit'7 */
		rx.cur.it_content = (data8 >> 7) & 0x1;
		/* byte3 bit[6:4] */
		rx.cur.ext_colorimetry = (data8 >> 4) & 0x7;
		/* byte3 bit[3:2]*/
		rx.cur.rgb_quant_range = (data8 >> 2) & 0x3;
		/* byte3 bit[1:0]*/
		rx.cur.n_uniform_scale = data8 & 0x3;

		/*byte 4*/
		rx.cur.hw_vic = hdmirx_rd_cor(AVIRX_DBYTE4_DP2_IVCRX);

		/*byte 5*/
		data8 = hdmirx_rd_cor(AVIRX_DBYTE5_DP2_IVCRX);
		/*byte5:bit[7:6]*/
		rx.cur.yuv_quant_range = (data8 >> 6) & 0x3;
		/*byte5:bit[5:4]*/
		rx.cur.cn_type = (data8 >> 4) & 0x3;
		/*byte5:bit[3:0]*/
		rx.cur.repeat = data8 & 0xf;

		/* byte [9:6]*/
		if (rx.cur.bar_valid == 3) {
			data8_lo = hdmirx_rd_cor(AVIRX_DBYTE6_DP2_IVCRX);
			data8_up = hdmirx_rd_cor(AVIRX_DBYTE7_DP2_IVCRX);
			rx.cur.bar_end_top = (data8_lo | (data8_up << 8));
			data8_lo = hdmirx_rd_cor(AVIRX_DBYTE8_DP2_IVCRX);
			data8_up = hdmirx_rd_cor(AVIRX_DBYTE9_DP2_IVCRX);
			rx.cur.bar_start_bottom = (data8_lo | (data8_up << 8));
			data8_lo = hdmirx_rd_cor(AVIRX_DBYTE10_DP2_IVCRX);
			data8_up = hdmirx_rd_cor(AVIRX_DBYTE11_DP2_IVCRX);
			rx.cur.bar_end_left = (data8_lo | (data8_up << 8));
			data8_lo = hdmirx_rd_cor(AVIRX_DBYTE12_DP2_IVCRX);
			data8_up = hdmirx_rd_cor(AVIRX_DBYTE13_DP2_IVCRX);
			rx.cur.bar_start_right = (data8_lo | (data8_up << 8));
		}
	} else {
		rx.cur.hw_vic = hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, VID_IDENT_CODE);
		rx.cur.cn_type = hdmirx_rd_bits_dwc(DWC_PDEC_AVI_HB, CONTENT_TYPE);
		rx.cur.repeat = hdmirx_rd_bits_dwc(DWC_PDEC_AVI_HB, PIX_REP_FACTOR);
		rx.cur.colorspace = hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, VIDEO_FORMAT);
		rx.cur.it_content = hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, IT_CONTENT);
		rx.cur.rgb_quant_range = hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, RGB_QUANT_RANGE);
		rx.cur.yuv_quant_range = hdmirx_rd_bits_dwc(DWC_PDEC_AVI_HB, YUV_QUANT_RANGE);
		rx.cur.scan_info = hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, SCAN_INFO);
		rx.cur.n_uniform_scale = hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, NON_UNIF_SCALE);
		rx.cur.ext_colorimetry = hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, EXT_COLORIMETRY);
		rx.cur.active_ratio = hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, ACT_ASPECT_RATIO);
		rx.cur.active_valid = hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, ACT_INFO_PRESENT);
		rx.cur.bar_valid = hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, BAR_INFO_VALID);
		if (rx.cur.bar_valid == 3) {
			rx.cur.bar_end_top =
				hdmirx_rd_bits_dwc(DWC_PDEC_AVI_TBB, LIN_END_TOP_BAR);
			rx.cur.bar_start_bottom =
				hdmirx_rd_bits_dwc(DWC_PDEC_AVI_TBB, LIN_ST_BOT_BAR);
			rx.cur.bar_end_left =
				hdmirx_rd_bits_dwc(DWC_PDEC_AVI_LRB, PIX_END_LEF_BAR);
			rx.cur.bar_start_right =
				hdmirx_rd_bits_dwc(DWC_PDEC_AVI_LRB, PIX_ST_RIG_BAR);
		}
		rx.cur.colorimetry =
			hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, COLORIMETRY);
		rx.cur.picture_ratio =
			hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, PIC_ASPECT_RATIO);
	}
}

void rx_get_colordepth(void)
{
	u8 tmp;

	if (rx.chip_id >= CHIP_ID_T7)
		tmp = hdmirx_rd_cor(COR_VININ_STS);
	else
		tmp = hdmirx_rd_bits_dwc(DWC_HDMI_STS, DCM_CURRENT_MODE);
	switch (tmp) {
	case DCM_CURRENT_MODE_48b:
		rx.cur.colordepth = E_COLORDEPTH_16;
		break;
	case DCM_CURRENT_MODE_36b:
		rx.cur.colordepth = E_COLORDEPTH_12;
		break;
	case DCM_CURRENT_MODE_30b:
		rx.cur.colordepth = E_COLORDEPTH_10;
		break;
	default:
		rx.cur.colordepth = E_COLORDEPTH_8;
		break;
	}
}

void rx_get_framerate(void)
{
	u32 tmp;

	if (rx.chip_id >= CHIP_ID_T7) {
		tmp = (hdmirx_rd_cor(COR_FRAME_RATE_HI) << 16) |
			(hdmirx_rd_cor(COR_FRAME_RATE_MI) << 8) |
			(hdmirx_rd_cor(COR_FRAME_RATE_LO));
		if (tmp == 0)
			rx.cur.frame_rate = 0;
		else
			rx.cur.frame_rate = rx.clk.p_clk / (tmp / 100);
	} else {
		tmp = hdmirx_rd_bits_dwc(DWC_MD_VTC, VTOT_CLK);
		if (tmp == 0)
			rx.cur.frame_rate = 0;
		else
			rx.cur.frame_rate = (modet_clk * 100000) / tmp;
	}
}

void rx_get_interlaced(void)
{
	u8 tmp;

	if (rx.chip_id >= CHIP_ID_T7)
		tmp = hdmirx_rd_cor(COR_FDET_STS) & _BIT(2);
	else
		tmp = hdmirx_rd_bits_dwc(DWC_MD_STS, ILACE);

	if (tmp)
		rx.cur.interlaced = true;
	else
		rx.cur.interlaced = false;
}

void rx_get_de_sts(void)
{
	u32 tmp;

	if (!rx.cur.colordepth)
		rx.cur.colordepth = 8;

	if (rx.chip_id >= CHIP_ID_T7) {
		tmp = hdmirx_rd_cor(COR_PIXEL_CNT_LO) |
			(hdmirx_rd_cor(COR_PIXEL_CNT_HI) << 8);
		rx.cur.hactive = tmp;
		rx.cur.vactive = hdmirx_rd_cor(COR_LINE_CNT_LO) |
			(hdmirx_rd_cor(COR_LINE_CNT_HI) << 8);
		rx.cur.htotal = (hdmirx_rd_cor(COR_HSYNC_LOW_COUNT_LO) |
			(hdmirx_rd_cor(COR_HSYNC_LOW_COUNT_HI) << 8)) +
			(hdmirx_rd_cor(COR_HSYNC_HIGH_COUNT_LO) |
			(hdmirx_rd_cor(COR_HSYNC_HIGH_COUNT_HI) << 8));
		rx.cur.vtotal = (hdmirx_rd_cor(COR_VSYNC_LOW_COUNT_LO) |
			(hdmirx_rd_cor(COR_VSYNC_LOW_COUNT_HI) << 8)) +
			(hdmirx_rd_cor(COR_VSYNC_HIGH_COUNT_LO) |
			(hdmirx_rd_cor(COR_VSYNC_HIGH_COUNT_HI) << 8));
		if (rx.cur.repeat) {
			rx.cur.hactive	= rx.cur.hactive / (rx.cur.repeat + 1);
			rx.cur.htotal = rx.cur.htotal / (rx.cur.repeat + 1);
		}
	} else {
		rx.cur.vactive = hdmirx_rd_bits_dwc(DWC_MD_VAL, VACT_LIN);
		rx.cur.vtotal = hdmirx_rd_bits_dwc(DWC_MD_VTL, VTOT_LIN);
		rx.cur.hactive = hdmirx_rd_bits_dwc(DWC_MD_HACT_PX, HACT_PIX);
		rx.cur.htotal = hdmirx_rd_bits_dwc(DWC_MD_HT1, HTOT_PIX);
		rx.cur.hactive	= rx.cur.hactive * 8 / rx.cur.colordepth;
		rx.cur.htotal = rx.cur.htotal * 8 / rx.cur.colordepth;
		if (rx.cur.repeat) {
			rx.cur.hactive	= rx.cur.hactive / (rx.cur.repeat + 1);
			rx.cur.htotal = rx.cur.htotal / (rx.cur.repeat + 1);
		}
	}
}

void rx_get_ecc_info(void)
{
	if (rx.chip_id < CHIP_ID_T7)
		return;

	rx.ecc_err = rx_get_ecc_err();
	rx.ecc_pkt_cnt = rx_get_ecc_pkt_cnt();
}

/*
 * rx_get_video_info - get current avi info
 */
void rx_get_video_info(void)
{
	/* DVI mode */
	rx.cur.hw_dvi = rx_get_dvi_mode();
	/* HDCP sts*/
	rx_get_hdcp_type();
	/* AVI parameters */
	rx_get_avi_params();
	/* frame rate */
	rx_get_framerate();
	/* deep color mode */
	rx_get_colordepth();
	/* pixel clock */
	/*rx.cur.pixel_clk = rx.clk.pixel_clk / rx.cur.colordepth * 8;*/
	/* image parameters */
	rx_get_de_sts();
	/* interlace */
	rx_get_interlaced();
}

void hdmirx_set_vp_mapping(enum colorspace_e cs)
{
	u32 data32 = 0;

	if (rx.chip_id < CHIP_ID_T7)
		return;

	switch (cs) {
	case E_COLOR_YUV422:
		data32 |= 3 << 9;
		data32 |= 3 << 6;
		data32 |= 3 << 3;
		hdmirx_wr_cor(VP_INPUT_MAPPING_VID_IVCRX, data32 & 0xff);
		hdmirx_wr_cor(VP_INPUT_MAPPING_VID_IVCRX + 1, (data32 >> 8) & 0xff);
		data32 = hdmirx_rd_top(TOP_VID_CNTL);
		data32 &= (~(0x7 << 24));
		data32 |= 1 << 24;
		hdmirx_wr_top(TOP_VID_CNTL, data32);
		break;
	case E_COLOR_YUV420:
	case E_COLOR_RGB:
		data32 |= 2 << 9;
		data32 |= 1 << 6;
		data32 |= 0 << 3;
		hdmirx_wr_cor(VP_INPUT_MAPPING_VID_IVCRX, data32 & 0xff);
		hdmirx_wr_cor(VP_INPUT_MAPPING_VID_IVCRX + 1, (data32 >> 8) & 0xff);
		data32 = hdmirx_rd_top(TOP_VID_CNTL);
		data32 &= (~(0x7 << 24));
		data32 |= 0 << 24;
		hdmirx_wr_top(TOP_VID_CNTL, data32);
		break;
	case E_COLOR_YUV444:
	default:
		data32 |= 2 << 9;
		data32 |= 1 << 6;
		data32 |= 0 << 3;
		hdmirx_wr_cor(VP_INPUT_MAPPING_VID_IVCRX, data32 & 0xff);
		hdmirx_wr_cor(VP_INPUT_MAPPING_VID_IVCRX + 1, (data32 >> 8) & 0xff);
		data32 = hdmirx_rd_top(TOP_VID_CNTL);
		data32 &= (~(0x7 << 24));
		data32 |= 2 << 24;
		hdmirx_wr_top(TOP_VID_CNTL, data32);
	break;
	}
}

/*
 * hdmirx_set_video_mute - video mute
 * @mute: mute enable or disable
 */
void hdmirx_set_video_mute(bool mute)
{
	static bool pre_mute_flag;

	/* bluescreen cfg */
	if (rx.chip_id >= CHIP_ID_T7 && rx.chip_id < CHIP_ID_T5M) {
		if (mute && (rx_pkt_chk_attach_drm() ||
			rx.vs_info_details.dolby_vision_flag != DV_NULL))
			return;
		if (mute != pre_mute_flag) {
			vdin_set_black_pattern(mute);
			pre_mute_flag = mute;
		}
	} else {
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
	}
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
	u32 temp = 0;
	u8 data8;
	u8 pixel_rpt_cnt;
	int reg_clk_vp_core_div, reg_clk_vp_out_div;
	if (dbg_cs & 0x10)
		temp = dbg_cs & 0x0f;
	else
		temp = rx.pre.colorspace;

	hdmirx_set_video_mute(0);
	set_dv_ll_mode(false);
	hdmirx_output_en(true);
	rx_irq_en(true);
	hdmirx_top_irq_en(1, 2);

	if (rx.chip_id < CHIP_ID_T7)
		return;

	hdmirx_set_vp_mapping(temp);

	if (rx.chip_id == CHIP_ID_T7) {
		/* repetition config */
		switch (rx.cur.repeat) {
		case 1:
			reg_clk_vp_core_div = 3;
			reg_clk_vp_out_div = 1;
			pixel_rpt_cnt = 1;
		break;
		case 3:
			reg_clk_vp_core_div = 7;
			reg_clk_vp_out_div = 3;
			pixel_rpt_cnt = 2;
		break;
		case 7: //todo
		default:
			reg_clk_vp_core_div = 1;
			reg_clk_vp_out_div = 0;
			pixel_rpt_cnt = 0;
		break;
		}
		data8 = hdmirx_rd_cor(RX_PWD0_CLK_DIV_0);
		data8 &= (~0x3f);
		//[5:3] divides the vpc out clock
		data8 |= (reg_clk_vp_out_div << 3);//[5:3] divides the vpc out clock
		//[2:0] divides the vpc core clock:
		//0: divide by 1; 1: divide by 2; 3: divide by 4; 7: divide by 8
		data8 |= (reg_clk_vp_core_div << 0);
		hdmirx_wr_cor(RX_PWD0_CLK_DIV_0, data8) ;//register address: 0x10c1

		data8 = hdmirx_rd_cor(RX_VP_INPUT_FORMAT_HI);
		data8 &= (~0x7);
		data8 |= ((pixel_rpt_cnt & 0x3) << 0);
		hdmirx_wr_cor(RX_VP_INPUT_FORMAT_HI, data8);
	} else if (rx.chip_id >= CHIP_ID_T3) {
		if (rx.pre.sw_vic >= HDMI_VESA_OFFSET ||
			rx.pre.sw_vic == HDMI_640x480p60 ||
			rx.pre.sw_dvi)
			hdmirx_wr_bits_top(TOP_VID_CNTL, _BIT(7), 1);
		else//use auto de-repeat
			hdmirx_wr_bits_top(TOP_VID_CNTL, _BIT(7), 0);
	}
	rx_sw_reset_t7(2);
}

/*
 * hdmirx_config_audio - audio channel map
 */
void hdmirx_config_audio(void)
{
	if (rx.chip_id >= CHIP_ID_T7) {
		/* set MCLK for I2S/SPDIF */
		hdmirx_wr_cor(AAC_MCLK_SEL_AUD_IVCRX, 0x80);
		hdmirx_hbr2spdif(1);
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
	u32 tmp_data = 0;
	u32 meas_cycles = 0;
	u64 tmp_data2 = 0;
	u64 aud_clk = 0;

	if (clk_src == TOP_HDMI_TMDSCLK) {
		tmp_data = hdmirx_rd_top(TOP_METER_HDMI_STAT);
		if (tmp_data & 0x80000000) {
			meas_cycles = tmp_data & 0xffffff;
			clock = (2930 * meas_cycles);
		}
	} else if (clk_src == TOP_HDMI_CABLECLK) {
		if (rx.chip_id >= CHIP_ID_TL1)
			tmp_data = hdmirx_rd_top(TOP_METER_CABLE_STAT);
		if (tmp_data & 0x80000000) {
			meas_cycles = tmp_data & 0xffffff;
			clock = (2930 * meas_cycles);
		}
	} else if (clk_src == TOP_HDMI_AUDIOCLK) {
		if (rx.chip_id >= CHIP_ID_TL1) {
			/*get audio clk*/
			tmp_data = hdmirx_rd_top(TOP_AUDMEAS_REF_CYCLES_STAT0);
			tmp_data2 = hdmirx_rd_top(TOP_AUDMEAS_REF_CYCLES_STAT1);
			aud_clk = ((tmp_data2 & 0xffff) << 32) | tmp_data;
			if (tmp_data2 & (0x1 << 17))
				aud_clk = div_u64((24000 * 65536), div_u64((aud_clk + 1), 1000));
			else
				rx_pr("audio clk measure fail\n");
		}
		clock = aud_clk;
	} else {
		tmp_data = 0;
	}

	/*reset hdmi,cable clk meter*/
	hdmirx_wr_top(TOP_SW_RESET, 0x60);
	hdmirx_wr_top(TOP_SW_RESET, 0x0);
	return clock;
}

void rx_clkmsr_monitor(void)
{
	schedule_work(&clkmsr_dwork);
}

void rx_clkmsr_handler(struct work_struct *work)
{
	switch (rx.chip_id) {
	case CHIP_ID_T5W:
		rx.clk.cable_clk = meson_clk_measure_with_precision(30, 32);
		rx.clk.tmds_clk = meson_clk_measure_with_precision(63, 32);
		rx.clk.aud_pll = meson_clk_measure_with_precision(74, 32);
		/* renamed to clk81_hdmirx_pclk,id=7 */
		rx.clk.p_clk = meson_clk_measure_with_precision(7, 32);
		break;
	case CHIP_ID_T5D:
		rx.clk.cable_clk = meson_clk_measure_with_precision(30, 32);
		rx.clk.tmds_clk = meson_clk_measure_with_precision(63, 32);
		rx.clk.aud_pll = meson_clk_measure_with_precision(74, 32);
		rx.clk.pixel_clk = meson_clk_measure_with_precision(29, 32);
		break;
	case CHIP_ID_T7:
	case CHIP_ID_T3:
		/* to decrease cpu loading of clk_msr work queue */
		/* 64: clk_msr resample time 32us,previous setting is 640us */
		rx.clk.cable_clk = meson_clk_measure_with_precision(44, 32);
		rx.clk.tmds_clk = meson_clk_measure_with_precision(43, 32);
		rx.clk.aud_pll = meson_clk_measure_with_precision(104, 32);
		rx.clk.p_clk = meson_clk_measure_with_precision(0, 32);
		break;
	case CHIP_ID_T5:
	case CHIP_ID_TM2:
	case CHIP_ID_TL1:
		rx.clk.cable_clk = meson_clk_measure_with_precision(30, 32);
		rx.clk.tmds_clk = meson_clk_measure_with_precision(63, 32);
		rx.clk.aud_pll = meson_clk_measure_with_precision(104, 32);
		rx.clk.pixel_clk = meson_clk_measure_with_precision(29, 32);
		break;
	default:
		rx.clk.aud_pll = meson_clk_measure_with_precision(24, 32);
		rx.clk.pixel_clk = meson_clk_measure_with_precision(29, 32);
		rx.clk.tmds_clk = meson_clk_measure_with_precision(25, 32);
		if (rx.clk.tmds_clk == 0) {
			rx.clk.tmds_clk =
				hdmirx_rd_dwc(DWC_HDMI_CKM_RESULT) & 0xffff;
			rx.clk.tmds_clk =
				rx.clk.tmds_clk * 158000 / 4095 * 1000;
		}
		if (rx.state == FSM_SIG_READY)
			/* phy request clk */
			rx.clk.mpll_clk =
				meson_clk_measure_with_precision(27, 32);
		break;
	}
}

/*
 * function - get clk related with hdmirx
 */
unsigned int rx_measure_clock(enum measure_clk_src_e clk_src)
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

	/*	from clock measure: t7_table
	 *		cable clock [44] hdmirx_cable_clk
	 *		tmds clock [43] hdmirx_tmds_clk
	 *		pixel clock [29] hdmirx_apll_clk_out_div
	 *		audio clock	[74] hdmirx_aud_pll_clk
	 *		cts audio [60] cts_hdmirx_aud_pll_clk
	 *		mpll clock [67] hdmirx_apll_clk_audio
	 *		esm clock [68] Cts_hdcp22_esm
	 */
	if (clk_src == MEASURE_CLK_CABLE) {
		if (rx.chip_id >= CHIP_ID_TL1 &&
		    rx.chip_id <= CHIP_ID_T5D) {
			clock = meson_clk_measure(30);
			/*clock = rx_get_clock(TOP_HDMI_CABLE_CLK);*/
		} else if (rx.chip_id >= CHIP_ID_T7) {
			clock = meson_clk_measure(44);
			/*clock = rx_get_clock(TOP_HDMI_CABLE_CLK);*/
		}
	} else if (clk_src == MEASURE_CLK_TMDS) {
		if (rx.chip_id >= CHIP_ID_TL1 &&
		    rx.chip_id <= CHIP_ID_T5D) {
			clock = meson_clk_measure(63);
		} else if (rx.chip_id >= CHIP_ID_T7) {
			clock = meson_clk_measure(43);
		} else {
			clock = meson_clk_measure(25);
			if (clock == 0) {
				clock = hdmirx_rd_dwc(DWC_HDMI_CKM_RESULT) & 0xffff;
				clock = clock * 158000 / 4095 * 1000;
			}
		}
	} else if (clk_src == MEASURE_CLK_PIXEL) {
		if (rx.chip_id >= CHIP_ID_T7)
			clock = 1;
		else
			clock = meson_clk_measure(29);
	} else if (clk_src == MEASURE_CLK_AUD_PLL) {
		if (rx.chip_id >= CHIP_ID_TL1)
			clock = meson_clk_measure(104);/*audio vid out*/
		else
			clock = meson_clk_measure(24);
	} else if (clk_src == MEASURE_CLK_AUD_DIV) {
		if (rx.chip_id >= CHIP_ID_TL1)
			clock = meson_clk_measure(67);/*apll_clk_audio*/
		else
			clock = meson_clk_measure(98);

	} else if (clk_src == MEASURE_CLK_MPLL) {
		if (rx.chip_id >= CHIP_ID_TL1)
			clock = 2;//meson_clk_measure(29);/*apll_clk_out_div*/
		else
			clock = meson_clk_measure(27);
	} else if (clk_src == MEASURE_CLK_PCLK) {
		clock = meson_clk_measure(0);
	}
	return clock;
}

void rx_earc_hpd_handler(struct work_struct *work)
{
	cancel_delayed_work(&eq_dwork);
	rx_set_port_hpd(rx.arc_port, 0);
	usleep_range(600000, 650000);
	rx_set_port_hpd(rx.arc_port, 1);
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

bool is_wr_only_reg(u32 addr)
{
	int i;

	if (rx.chip_id >= CHIP_ID_T7)
		return false;

	for (i = 0; i < sizeof(wr_only_register) / sizeof(u32); i++) {
		if (addr == wr_only_register[i])
			return true;
	}
	return false;
}

void rx_debug_load22key(void)
{
	int ret = 0;
	int wait_kill_done_cnt = 0;

	if (rx.chip_id >= CHIP_ID_T7) {
		rx.fsm_ext_state = FSM_HPD_LOW;
	} else {
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
			/* extcon_set_state_sync(rx.rx_extcon_rx22, EXTCON_DISP_HDMI, 0); */
			rx_hdcp22_send_uevent(0);
			hdmirx_wr_dwc(DWC_HDCP22_CONTROL, 0x0);
			/* if key_a is already exist on platform,*/
			/*need to set valid bit to 0 before burning key_b,*/
			/*otherwise,key_b will not be activated*/
			rx_hdcp22_wr_top(TOP_SKP_CNTL_STAT, 0x1);
			hdmirx_hdcp22_esm_rst();
			mdelay(110);
			rx_is_hdcp22_support();
			hdmirx_wr_dwc(DWC_HDCP22_CONTROL, 0x1000);
			/* rx_hdcp22_wr_top(TOP_SKP_CNTL_STAT, 0x1); */
			hdcp22_clk_en(1);
			/* extcon_set_state_sync(rx.rx_extcon_rx22, EXTCON_DISP_HDMI, 1); */
			rx_hdcp22_send_uevent(1);
			mdelay(100);
			hdmi_rx_top_edid_update();
			hdmirx_hw_config();
			hpd_to_esm = 1;
			/* mdelay(900); */
			rx_set_cur_hpd(1, 4);
			sm_pause = 0;
		}
	}
	rx_pr("%s\n", __func__);
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
		if ((i - start_addr) % (sizeof(uint) * 4) == 0)
			pr_cont("[0x%-4x] ", i);
		if (!is_wr_only_reg(i)) {
			if (rx.chip_id >= CHIP_ID_T7) {
				pr_cont("0x%x,   ", hdmirx_rd_cor(i));
				pr_cont("0x%x,   ", hdmirx_rd_cor(i + 1));
				pr_cont("0x%x,   ", hdmirx_rd_cor(i + 2));
				pr_cont("0x%x,   ", hdmirx_rd_cor(i + 3));
			} else {
				pr_cont("0x%x,   ", hdmirx_rd_dwc(i));
			}
		} else {
			pr_cont("xxxx,   ");
		}

		if ((i - start_addr) % (sizeof(uint) * 4) == sizeof(uint) * 3)
			rx_pr(" ");
	}

	if ((end_addr - start_addr + sizeof(uint)) % (sizeof(uint) * 4) != 0)
		rx_pr(" ");
}

void dump_reg(void)
{
	int i = 0;

	rx_pr("\n***Top registers***\n");
	rx_pr("[addr ]  addr + 0x0,");
	rx_pr("addr + 0x1,  addr + 0x2,	addr + 0x3\n");
	for (i = 0; i <= 0x84;) {
		pr_cont("[0x%-3x]", i);
		pr_cont("0x%-8x,0x%-8x,0x%-8x,0x%-8x\n",
				hdmirx_rd_top(i),
				hdmirx_rd_top(i + 1),
				hdmirx_rd_top(i + 2),
				hdmirx_rd_top(i + 3));
		i = i + 4;
	}
	if (rx.chip_id >= CHIP_ID_TL1) {
		for (i = 0x25; i <= 0x84;) {
			pr_cont("[0x%-3x]", i);
			pr_cont("0x%-8x,0x%-8x,0x%-8x,0x%-8x\n",
				   hdmirx_rd_top(i),
				   hdmirx_rd_top(i + 1),
				   hdmirx_rd_top(i + 2),
				   hdmirx_rd_top(i + 3));
			i = i + 4;
		}
	}
	if (rx.chip_id < CHIP_ID_TL1) {
		rx_pr("\n***PHY registers***\n");
		pr_cont("[addr ]  addr + 0x0,");
		pr_cont("addr + 0x1,addr + 0x2,");
		rx_pr("addr + 0x3\n");
		for (i = 0; i <= 0x9a;) {
			pr_cont("[0x%-3x]", i);
			pr_cont("0x%-8x,0x%-8x,0x%-8x,0x%-8x\n",
				   hdmirx_rd_phy(i),
			       hdmirx_rd_phy(i + 1),
			       hdmirx_rd_phy(i + 2),
			       hdmirx_rd_phy(i + 3));
			i = i + 4;
		}
	} else if (rx.chip_id >= CHIP_ID_TL1) {
		/* dump phy register */
		dump_reg_phy();
	}

	if (rx.chip_id < CHIP_ID_T7) {
		rx_pr("\n**Controller registers**\n");
		pr_cont("[addr ]  addr + 0x0,");
		pr_cont("addr + 0x4,  addr + 0x8,");
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
	} else {
		print_reg(0x0, 0xfe);
		print_reg(0x300, 0x3ff);
		print_reg(0x1001, 0x1f78);
	}
}

void dump_reg_phy(void)
{
	if (rx.phy_ver == PHY_VER_T5)
		dump_reg_phy_t5();
	else if (rx.phy_ver >= PHY_VER_T7)
		dump_reg_phy_t7();
	else
		dump_reg_phy_tl1_tm2();
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
			pr_cont("[%2d] ", i);
			for (j = 0; j < 16; j++) {
				pr_cont("0x%02x, ",
				      hdmirx_rd_top(TOP_EDID_OFFSET +
						    (i * 16 + j)));
			}
			rx_pr(" ");
		}
	} else if (rx.chip_id == CHIP_ID_TL1) {
		for (i = 0; i < 16; i++) {
			pr_cont("[%2d] ", i);
			for (j = 0; j < 16; j++) {
				pr_cont("0x%02x, ",
				      hdmirx_rd_top(TOP_EDID_ADDR_S +
						    (i * 16 + j)));
			}
			rx_pr(" ");
		}
	} else {
		for (i = 0; i < 16; i++) {
			pr_cont("[%2d] ", i);
			for (j = 0; j < 16; j++) {
				pr_cont("0x%02x, ",
				      hdmirx_rd_top(TOP_EDID_ADDR_S +
						    (i * 16 + j)));
			}
			rx_pr(" ");
		}
		for (i = 0; i < 16; i++) {
			pr_cont("[%2d] ", i);
			for (j = 0; j < 16; j++) {
				pr_cont("0x%02x, ",
				      hdmirx_rd_top(TOP_EDID_PORT2_ADDR_S +
						    (i * 16 + j)));
			}
			rx_pr(" ");
		}
		for (i = 0; i < 16; i++) {
			pr_cont("[%2d] ", i);
			for (j = 0; j < 16; j++) {
				pr_cont("0x%02x, ",
				      hdmirx_rd_top(TOP_EDID_PORT3_ADDR_S +
						    (i * 16 + j)));
			}
			rx_pr(" ");
		}
	}
}

int rx_debug_wr_reg(const char *buf, char *tmpbuf, int i)
{
	u32 adr = 0;
	u32 value = 0;

	if (kstrtou32(tmpbuf + 3, 16, &adr) < 0)
		return -EINVAL;
	rx_pr("adr = %#x\n", adr);
	if (kstrtou32(buf + i + 1, 16, &value) < 0)
		return -EINVAL;
	rx_pr("value = %#x\n", value);
	if (tmpbuf[1] == 'h') {
		if (buf[2] == 't') {
			hdmirx_wr_top(adr, value);
			rx_pr("write %x to TOP [%x]\n", value, adr);
		} else if (buf[2] == 'd') {
			if (rx.chip_id >= CHIP_ID_T7)
				hdmirx_wr_cor(adr, value);
			else
				hdmirx_wr_dwc(adr, value);
			rx_pr("write %x to DWC [%x]\n", value, adr);
		} else if (buf[2] == 'p') {
			hdmirx_wr_phy(adr, value);
			rx_pr("write %x to PHY [%x]\n", value, adr);
		} else if (buf[2] == 'u') {
			wr_reg_hhi(adr, value);
			rx_pr("write %x to hiu [%x]\n", value, adr);
		} else if (buf[2] == 's') {
			rx_hdcp22_wr_top(adr, value);
			rx_pr("write %x to sec-top [%x]\n", value, adr);
		} else if (buf[2] == 'h') {
			rx_hdcp22_wr_reg(adr, value);
			rx_pr("write %x to esm [%x]\n", value, adr);
		} else if (buf[2] == 'a') {
			hdmirx_wr_amlphy(adr, value);
			rx_pr("write %x to amlphy [%x]\n", value, adr);
		}
	}
	return 0;
}

int rx_debug_rd_reg(const char *buf, char *tmpbuf)
{
	u32 adr = 0;
	u32 value = 0;

	if (tmpbuf[1] == 'h') {
		if (kstrtou32(tmpbuf + 3, 16, &adr) < 0)
			return -EINVAL;
		if (tmpbuf[2] == 't') {
			value = hdmirx_rd_top(adr);
			rx_pr("TOP [%x]=%x\n", adr, value);
		} else if (tmpbuf[2] == 'd') {
			if (rx.chip_id >= CHIP_ID_T7)
				value = hdmirx_rd_cor(adr);
			else
				value = hdmirx_rd_dwc(adr);
			rx_pr("DWC [%x]=%x\n", adr, value);
		} else if (tmpbuf[2] == 'p') {
			value = hdmirx_rd_phy(adr);
			rx_pr("PHY [%x]=%x\n", adr, value);
		} else if (tmpbuf[2] == 'u') {
			value = rd_reg_hhi(adr);
			rx_pr("HIU [%x]=%x\n", adr, value);
		} else if (tmpbuf[2] == 's') {
			value = rx_hdcp22_rd_top(adr);
			rx_pr("SEC-TOP [%x]=%x\n", adr, value);
		} else if (tmpbuf[2] == 'h') {
			value = rx_hdcp22_rd_reg(adr);
			rx_pr("esm [%x]=%x\n", adr, value);
		} else if (tmpbuf[2] == 'a') {
			value = hdmirx_rd_amlphy(adr);
			rx_pr("amlphy [%x]=%x\n", adr, value);
		}
	}
	return 0;
}

int rx_get_aud_pll_err_sts(void)
{
	int ret = E_AUDPLL_OK;
	u32 req_clk = 0;
	u32 aud_clk = 0;//rx.aud_info.aud_clk;
	u32 phy_pll_rate = 0;//(hdmirx_rd_phy(PHY_MAINFSM_STATUS1) >> 9) & 0x3;
	u32 aud_pll_cntl = 0;//(rd_reg_hhi(HHI_AUD_PLL_CNTL6) >> 28) & 0x3;

	if (rx.chip_id >= CHIP_ID_TL1)
		return ret;

	req_clk = rx.clk.mpll_clk;
	aud_clk = rx.aud_info.aud_clk;
	phy_pll_rate = (hdmirx_rd_phy(PHY_MAINFSM_STATUS1) >> 9) & 0x3;
	aud_pll_cntl = (rd_reg_hhi(HHI_AUD_PLL_CNTL6) >> 28) & 0x3;

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

	return ret;
}

u32 aml_cable_clk_band(u32 cable_clk, u32 clk_rate)
{
	u32 bw;
	u32 cab_clk = cable_clk;

	if (rx.chip_id < CHIP_ID_TL1)
		return PHY_BW_2;

	/* rx_pr("cable clk=%d, clk_rate=%d\n", cable_clk, clk_rate); */
	/* 1:40 */
	if (clk_rate)
		cab_clk = cable_clk << 2;

	/* 1:10 */
	if (cab_clk < (45 * MHz))
		bw = PHY_BW_0;
	else if (cab_clk < (77 * MHz))
		bw = PHY_BW_1;
	else if (cab_clk < (155 * MHz))
		bw = PHY_BW_2;
	else if (cab_clk < (340 * MHz))
		bw = PHY_BW_3;
	else if (cab_clk < (525 * MHz))
		bw = PHY_BW_4;
	else if (cab_clk < (600 * MHz))
		bw = PHY_BW_5;
	else
		bw = PHY_BW_2;
	return bw;
}

u32 aml_phy_pll_band(u32 cable_clk, u32 clk_rate)
{
	u32 bw;
	u32 cab_clk = cable_clk;

	if (clk_rate)
		cab_clk = cable_clk << 2;

	/* 1:10 */
	if (cab_clk < (35 * MHz))
		bw = PLL_BW_0;
	else if (cab_clk < (77 * MHz))
		bw = PLL_BW_1;
	else if (cab_clk < (155 * MHz))
		bw = PLL_BW_2;
	else if (cab_clk < (340 * MHz))
		bw = PLL_BW_3;
	else if (cab_clk < (600 * MHz))
		bw = PLL_BW_4;
	else
		bw = PLL_BW_2;

	return bw;
}

void aml_phy_switch_port(void)
{
	if (rx.chip_id == CHIP_ID_TL1)
		aml_phy_switch_port_tl1();
	else if (rx.chip_id == CHIP_ID_TM2)
		aml_phy_switch_port_tm2();
	else if (rx.chip_id >= CHIP_ID_T5 &&
		rx.chip_id <= CHIP_ID_T5D)
		aml_phy_switch_port_t5();
	else if (rx.chip_id >= CHIP_ID_T7)
		aml_phy_switch_port_t7();
}

bool is_ft_trim_done(void)
{
	int ret = phy_trim_val & 0x1;

	rx_pr("ft trim=%d\n", ret);
	return ret;
}

/*T5 todo:*/
void aml_phy_get_trim_val_tl1_tm2(void)
{
	phy_trim_val = rd_reg_hhi(HHI_HDMIRX_PHY_MISC_CNTL1);
	dts_debug_flag = (phy_term_lel >> 4) & 0x1;
	rlevel = phy_term_lel & 0xf;
	if (rlevel > 11)
		rlevel = 10;
	phy_tdr_en = dts_debug_flag;
}

void aml_phy_get_trim_val(void)
{
	if (rx.chip_id >= CHIP_ID_TL1 &&
		rx.chip_id <= CHIP_ID_TM2)
		aml_phy_get_trim_val_tl1_tm2();
	else if (rx.chip_id >= CHIP_ID_T5 &&
		rx.chip_id <= CHIP_ID_T5D)
		aml_phy_get_trim_val_t5();
	else if (rx.chip_id >= CHIP_ID_T7)
		aml_phy_get_trim_val_t7();
}

void rx_get_best_eq_setting(void)
{
	u32 ch0, ch1, ch2;
	static u32 err_sum;
	static u32 time_cnt;
	static u32 array_cnt;

	if (rx.chip_id < CHIP_ID_TL1 || !find_best_eq)
		return;
	if (find_best_eq >= 0x7777 || array_cnt >= 255) {
		rx_pr("eq traversal completed.\n");
		rx_pr("best eq value:%d\n", array_cnt);
		if (array_cnt) {
			do  {
				rx_pr("%x:\n", rx.phy.eq_data[array_cnt]);
			} while (array_cnt--);
		} else {
			rx_pr("%x:\n", rx.phy.eq_data[array_cnt]);
		}
		find_best_eq = 0;
		array_cnt = 0;
		return;
	}
	if (time_cnt == 0) {
		hdmirx_phy_init();
		udelay(1);
		wr_reg_hhi_bits(HHI_HDMIRX_PHY_DCHD_CNTL1, MSK(16, 4), find_best_eq);
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
	}
}

bool is_tmds_clk_stable(void)
{
	bool ret = true;
	u32 cable_clk;

	if (rx.phy.clk_rate)
		cable_clk = rx.clk.cable_clk * 4;
	else
		cable_clk = rx.clk.cable_clk;

	if (abs(cable_clk - rx.clk.tmds_clk) > clock_lock_th * MHz) {
		if (log_level & VIDEO_LOG)
			rx_pr("cable_clk=%d,tmdsclk=%d,\n",
			      cable_clk / MHz, rx.clk.tmds_clk / MHz);
		ret = false;
	} else {
		ret = true;
	}
	return ret;
}

void aml_phy_init_handler(struct work_struct *work)
{
	//cancel_work(&aml_phy_dwork);
	if (rx.phy_ver == PHY_VER_TL1)
		aml_phy_init_tl1();
	else if (rx.phy_ver == PHY_VER_TM2)
		aml_phy_init_tm2();
	else if (rx.phy_ver == PHY_VER_T5)
		aml_phy_init_t5();
	else if (rx.phy_ver >= PHY_VER_T7 && rx.phy_ver <= PHY_VER_T5W)
		aml_phy_init_t7();
	else if (rx.phy_ver == PHY_VER_T5M)
		aml_phy_init_t5m();
	else if (rx.phy_ver == PHY_VER_T3X)
		aml_phy_init_t3x();
	eq_sts = E_EQ_FINISH;
}

void aml_phy_init(void)
{
	schedule_work(&aml_phy_dwork);
	eq_sts = E_EQ_START;
}

/*
 * hdmirx_phy_init - hdmirx phy initialization
 */
void hdmirx_phy_init(void)
{
	if (rx.chip_id >= CHIP_ID_TL1)
		aml_phy_init();
	else
		snps_phyg3_init();
}

void rx_phy_short_bist(void)
{
	if (rx.phy_ver == PHY_VER_TL1)
		aml_phy_short_bist_tl1();
	else if (rx.phy_ver == PHY_VER_TM2)
		aml_phy_short_bist_tm2();
	else if (rx.phy_ver == PHY_VER_T5)
		aml_phy_short_bist_t5();
	else if (rx.phy_ver >= PHY_VER_T7 && rx.phy_ver <= PHY_VER_T5W)
		aml_phy_short_bist_t7();
	else if (rx.phy_ver == PHY_VER_T5M)
		aml_phy_short_bist_t5m();
	else if (rx.phy_ver == PHY_VER_T3X)
		aml_phy_short_bist_t3x();
}

unsigned int aml_phy_pll_lock_tm2(void)
{
	if (rd_reg_hhi(TM2_HHI_HDMIRX_APLL_CNTL0) & 0x80000000)
		return true;
	else
		return false;
}

unsigned int aml_phy_pll_lock_t5(void)
{
	if (hdmirx_rd_amlphy(T5_HHI_RX_APLL_CNTL0) & 0x80000000)
		return true;
	else
		return false;
}

unsigned int aml_phy_pll_lock(void)
{
	if (rx.chip_id >= CHIP_ID_T5)
		return aml_phy_pll_lock_t5();
	else
		return aml_phy_pll_lock_tm2();
}

bool is_tmds_valid(void)
{
	if (force_vic)
		return true;

	if (!rx.cableclk_stb_flg)
		return false;

	if (rx.chip_id >= CHIP_ID_TL1)
		return (aml_phy_tmds_valid() == 1) ? true : false;
	else
		return (rx_get_pll_lock_sts() == 1) ? true : false;
}

unsigned int aml_phy_tmds_valid(void)
{
	if (rx.phy_ver == PHY_VER_TL1)
		return aml_get_tmds_valid_tl1();
	else if (rx.phy_ver == PHY_VER_TM2)
		return aml_get_tmds_valid_tm2();
	else if (rx.phy_ver == PHY_VER_T5)
		return aml_get_tmds_valid_t5();
	else if (rx.phy_ver >= PHY_VER_T7 && rx.phy_ver <= PHY_VER_T5W)
		return aml_get_tmds_valid_t7();
	else if (rx.phy_ver == PHY_VER_T5M)
		return aml_get_tmds_valid_t5m();
	else if (rx.phy_ver == PHY_VER_T3X)
		return aml_get_tmds_valid_t3x();
	else
		return false;
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
	/* phy power down */
	if (rx.phy_ver == PHY_VER_TL1) {
		/* pll power down */
		aml_phy_power_off_tl1();
	} else if (rx.phy_ver == PHY_VER_TM2) {
		/* pll power down */
		aml_phy_power_off_tm2();
	} else if (rx.phy_ver == PHY_VER_T5) {
		/* pll power down */
		aml_phy_power_off_t5();
	} else if (rx.phy_ver >= PHY_VER_T7  &&
		rx.phy_ver <= PHY_VER_T5W) {
		/* pll power down */
		aml_phy_power_off_t7();
	} else if (rx.phy_ver == PHY_VER_T5M) {
		/* pll power down */
		aml_phy_power_off_t5m();
	}
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
		if (onoff == 0)
			aml_phy_power_off();
	}
}

void aml_phy_iq_skew_monitor(void)
{
	if (rx.phy_ver == PHY_VER_T5)
		aml_phy_iq_skew_monitor_t5();
	else if (rx.phy_ver >= PHY_VER_T7 && rx.phy_ver <= PHY_VER_T5W)
		aml_phy_iq_skew_monitor_t7();
	else if (rx.phy_ver == PHY_VER_T5M)
		aml_phy_iq_skew_monitor_t5m();
	else if (rx.phy_ver == PHY_VER_T3X)
		aml_phy_iq_skew_monitor_t3x();
}

void aml_eq_eye_monitor(void)
{
	if (rx.phy_ver == PHY_VER_T5)
		aml_eq_eye_monitor_t5();
	else if (rx.phy_ver >= PHY_VER_T7 && rx.phy_ver <= PHY_VER_T5W)
		aml_eq_eye_monitor_t7();
	else if (rx.phy_ver == PHY_VER_T5M)
		aml_eq_eye_monitor_t5m();
	else if (rx.phy_ver == PHY_VER_T3X)
		aml_eq_eye_monitor_t3x();
}

void rx_emp_to_ddr_init(void)
{
	u32 data32;

	if (rx.chip_id < CHIP_ID_TL1)
		return;

	if (rx.emp_buff.pg_addr) {
		rx_pr("%s\n", __func__);
		/*disable field done and last pkt interrupt*/
		data32 = hdmirx_rd_top(TOP_INTR_MASKN);
		data32 &= ~(1 << 25);
		data32 &= ~(1 << 26);
		hdmirx_wr_top(TOP_INTR_MASKN, data32);

		if (rx.emp_buff.p_addr_a) {
			/* emp int enable */
			/* config ddr buffer */
			hdmirx_wr_top(TOP_EMP_DDR_START_A, rx.emp_buff.p_addr_a >> 2);
			hdmirx_wr_top(TOP_EMP_DDR_START_B, rx.emp_buff.p_addr_b >> 2);
		}
		/* enable store EMP pkt type */
		data32 = 0;
		if (disable_hdr)
			data32 |= 0 << 22; /* ddr_store_drm */
		else
			data32 |= 1 << 22;/* ddr_store_drm */
		/* ddr_store_aif */
		if (rx.chip_id == CHIP_ID_T7)
			data32 |= 1 << 19;
		else
			data32 |= 0 << 19;
		data32 |= 0 << 18;/* ddr_store_spd */
#ifdef MULTI_VSIF_EXPORT_TO_EMP
		data32 |= 1 << 16;/* ddr_store_vsi */
#else
		data32 |= 0 << 16;/* ddr_store_vsi */
#endif
		data32 |= 1 << 15;/* ddr_store_emp */
		data32 |= 0 << 12;/* ddr_store_amp */
		data32 |= 0 << 8;/* ddr_store_hbr */
		data32 |= 0 << 1;/* ddr_store_auds */
		hdmirx_wr_top(TOP_EMP_DDR_FILTER, data32);
		/* max pkt count */
		hdmirx_wr_top(TOP_EMP_CNTMAX, EMP_BUFF_MAX_PKT_CNT);

		data32 = 0;
		data32 |= 0xf << 16;/*[23:16] hs_beat_rate=0xf */
		data32 |= 0x0 << 14;/*[14] buffer_info_mode=0 */
		data32 |= 0x1 << 13;/*[13] reset_on_de=1 */
		data32 |= 0x1 << 12;/*[12] burst_end_on_last_emp=1 */
		data32 |= 0x3ff << 2;/*[11:2] de_rise_delay=0 */
		data32 |= 0x0 << 0;/*[1:0] Endian = 0 */
		hdmirx_wr_top(TOP_EMP_CNTL_0, data32);

		data32 = 0;
		data32 |= 0 << 1;/*ddr_mode[1] 0: emp 1: tmds*/
		hdmirx_wr_top(TOP_EMP_CNTL_1, data32);

		data32 = 0;
		data32 |= 0 << 1; /*ddr_mode[1] 0: emp 1: tmds*/
		data32 |= 1 << 0; /*ddr_en[0] 1:enable*/
		hdmirx_wr_top(TOP_EMP_CNTL_1, data32);

		/* emp int enable TOP_INTR_MASKN*/
		/* emp field end done at DE rist bit[25]*/
		/* emp last EMP pkt recv done bit[26]*/
		/* disable emp irq */
		//top_intr_maskn_value |= _BIT(25);
		//hdmirx_wr_top(TOP_INTR_MASKN, top_intr_maskn_value);
	}

	rx.emp_buff.ready = NULL;
	rx.emp_buff.irq_cnt = 0;
	rx.emp_buff.emp_pkt_cnt = 0;
	rx.emp_buff.tmds_pkt_cnt = 720 * 480;
}

void rx_emp_field_done_irq(void)
{
	phys_addr_t p_addr;
	unsigned int recv_pkt_cnt, recv_byte_cnt, recv_pagenum;
	unsigned int emp_pkt_cnt = 0;
	unsigned char *src_addr = 0;
	unsigned char *dst_addr;
	unsigned int i, j, k;
	unsigned int data_cnt = 0;
	struct page *cur_start_pg_addr;
	struct emp_buff *emp_buf_p = NULL;

	/*emp data start physical address*/
	p_addr = (u64)hdmirx_rd_top(TOP_EMP_DDR_PTR_S_BUF) << 2;

	/*buffer number*/
	recv_pkt_cnt = hdmirx_rd_top(TOP_EMP_RCV_CNT_BUF);
	emp_buf_p = &rx.emp_buff;

	recv_byte_cnt = recv_pkt_cnt * 32;
	if (recv_byte_cnt > (EMP_BUFFER_SIZE >> 1))
		recv_byte_cnt = EMP_BUFFER_SIZE >> 1;
	//if (log_level & PACKET_LOG)
		//rx_pr("recv_byte_cnt=0x%x\n", recv_byte_cnt);
	recv_pagenum = (recv_byte_cnt >> PAGE_SHIFT) + 1;

	if (rx.emp_buff.irq_cnt & 0x1)
		dst_addr = rx.emp_buff.store_b;
	else
		dst_addr = rx.emp_buff.store_a;

	if (recv_pkt_cnt >= EMP_BUFF_MAX_PKT_CNT) {
		recv_pkt_cnt = EMP_BUFF_MAX_PKT_CNT - 1;
		rx_pr("pkt cnt err:%d\n", recv_pkt_cnt);
	}
	if (!rx.emp_pkt_rev)
		rx.emp_pkt_rev = true;
	for (i = 0; i < recv_pagenum;) {
		/*one page 4k*/
		cur_start_pg_addr = phys_to_page(p_addr + i * PAGE_SIZE);
		if (p_addr == emp_buf_p->p_addr_a)
			src_addr = kmap_atomic(cur_start_pg_addr);
		else
			src_addr = kmap_atomic(cur_start_pg_addr) + (emp_buf_p->p_addr_b -
				emp_buf_p->p_addr_a) % PAGE_SIZE;
		if (!src_addr)
			return;
		dma_sync_single_for_cpu(hdmirx_dev, (p_addr + i * PAGE_SIZE),
					PAGE_SIZE, DMA_TO_DEVICE);
		for (j = 0; j < recv_byte_cnt;) {
			//if (src_addr[j] == 0x7f) {
			emp_pkt_cnt++;
			/*32 bytes per emp pkt*/
			for (k = 0; k < 32; k++) {
				dst_addr[data_cnt] = src_addr[j + k];
				data_cnt++;
			}
			//}
			j += 32;
		}
		/*release*/
		/*__kunmap_atomic(src_addr);*/
		kunmap_atomic(src_addr);
		i++;
	}

	if (emp_pkt_cnt * 32 > 1024) {
		if (log_level & 0x400)
			rx_pr("emp buffer overflow!!\n");
	} else {
		/*ready address*/
		rx.emp_buff.ready = dst_addr;
		/*ready pkt cnt*/
		rx.emp_buff.emp_pkt_cnt = emp_pkt_cnt;
		for (i = 0; i < rx.emp_buff.emp_pkt_cnt; i++)
			memcpy((char *)(emp_buf + 31 * i),
				   (char *)(dst_addr + 32 * i), 31);
		/*emp field dont irq counter*/
		rx.emp_buff.irq_cnt++;
		//rx.emp_buff.ogi_id = emp_buf[6];
		//rx.emp_buff.emp_tagid = emp_buf[10] +
			//(emp_buf[11] << 8) +
			//(emp_buf[12] << 16);
		//rx.emp_buff.data_ver = emp_buf[13];
		//rx.emp_buff.emp_content_type = emp_buf[19];
	}
}

void rx_emp_status(void)
{
	rx_pr("p_addr_a=0x%p\n", (void *)rx.emp_buff.p_addr_a);
	rx_pr("p_addr_b=0x%p\n", (void *)rx.emp_buff.p_addr_b);
	rx_pr("store_a=0x%p\n", rx.emp_buff.store_a);
	rx_pr("store_b=0x%p\n", rx.emp_buff.store_b);
	rx_pr("irq cnt =0x%x\n", (unsigned int)rx.emp_buff.irq_cnt);
	rx_pr("ready=0x%p\n", rx.emp_buff.ready);
	rx_pr("dump_mode =0x%x\n", rx.emp_buff.dump_mode);
	rx_pr("recv emp pkt cnt=0x%x\n", rx.emp_buff.emp_pkt_cnt);
	rx_pr("recv tmds pkt cnt=0x%x\n", rx.emp_buff.tmds_pkt_cnt);
}

void rx_tmds_to_ddr_init(void)
{
	unsigned int data, data2;
	unsigned int i = 0;

	if (rx.chip_id < CHIP_ID_T7)
		return;

	if (rx.emp_buff.pg_addr) {
		rx_pr("%s\n", __func__);
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
		if (rx.emp_buff.p_addr_a) {
			/* config ddr buffer */
			hdmirx_wr_top(TOP_EMP_DDR_START_A, rx.emp_buff.p_addr_a >> 2);
			hdmirx_wr_top(TOP_EMP_DDR_START_B, rx.emp_buff.p_addr_a >> 2);
			rx_pr("cfg hw addr=0x%p\n", (void *)rx.emp_buff.p_addr_a);
		}

		/* max pkt count to avoid buffer overflow */
		/* one pixel 4bytes */
		data = ((rx.emp_buff.tmds_pkt_cnt / 8) * 8) - 1;
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
void rx_get_error_cnt(u32 *ch0, u32 *ch1, u32 *ch2)
{
	u32 val;

	if (rx.chip_id == CHIP_ID_T7) {
		/* t7 top 0x41/0x42 can not shadow IP's periodical error counter */
		/* use cor register to get err cnt,t3 fix it */
		hdmirx_wr_bits_cor(DPLL_CTRL0_DPLL_IVCRX, MSK(3, 0), 0x0);
		*ch0 = hdmirx_rd_cor(SCDCS_CED0_L_SCDC_IVCRX) |
			((hdmirx_rd_cor(SCDCS_CED0_H_SCDC_IVCRX & 0x7f) << 8));
		*ch1 = hdmirx_rd_cor(SCDCS_CED1_L_SCDC_IVCRX) |
			((hdmirx_rd_cor(SCDCS_CED1_H_SCDC_IVCRX & 0x7f) << 8));
		*ch2 = hdmirx_rd_cor(SCDCS_CED2_L_SCDC_IVCRX) |
			((hdmirx_rd_cor(SCDCS_CED2_H_SCDC_IVCRX & 0x7f) << 8));
		udelay(1);
		hdmirx_wr_bits_cor(DPLL_CTRL0_DPLL_IVCRX, MSK(3, 0), 0x7);
	} else {
		val = hdmirx_rd_top(TOP_CHAN01_ERRCNT);
		*ch0 = val & 0xffff;
		*ch1 = (val >> 16) & 0xffff;
		val = hdmirx_rd_top(TOP_CHAN2_ERRCNT);
		*ch2 = val & 0xffff;
	}
}

/*
 * get hdmi audio N CTS
 * for tl1
 * return:
 * audio ACR N
 * audio ACR CTS
 */
void rx_get_audio_N_CTS(u32 *N, u32 *CTS)
{
	*N = hdmirx_rd_top(TOP_ACR_N_STAT);
	*CTS = hdmirx_rd_top(TOP_ACR_CTS_STAT);
}

u8 rx_get_avmute_sts(void)
{
	u8 ret = 0;

	if (rx.chip_id >= CHIP_ID_T7) {
		if (hdmirx_rd_cor(RX_GCP_DBYTE0_DP3_IVCRX) & 1)
			ret = 1;
	} else {
		if (hdmirx_rd_dwc(DWC_PDEC_GCP_AVMUTE) & 0x02)
			ret = 1;
	}
	return ret;
}
/* termination calibration */
void rx_phy_rt_cal(void)
{
	int i = 0, j = 0;
	u32 x_val[100][2];
	u32 temp;
	int val_cnt = 1;

	for (; i < 100; i++) {
		wr_reg_hhi_bits(HHI_HDMIRX_PHY_MISC_CNTL0, MISCI_COMMON_RST, 0);
		wr_reg_hhi_bits(HHI_HDMIRX_PHY_MISC_CNTL0, MISCI_COMMON_RST, 1);
		udelay(1);
		temp = (rd_reg_hhi(HHI_HDMIRX_PHY_MISC_STAT) >> 1) & 0x3ff;
		if (i == 0) {
			x_val[0][0] = temp;
			x_val[0][1] = 1;
		}

		for (; j < i; j++) {
			if (temp == x_val[j][0]) {
				x_val[j][1]	+= 1;
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

		if (x_val[j][1] == 10) {
			term_cal_val = (~((x_val[j][0]) << 1)) & 0x3ff;
			rx_pr("tdr cal val=0x%x", term_cal_val);
			return;
		}
		j = 0;
	}
}

/*
 * for Nvidia PC long detection time issue
 */
void rx_i2c_err_monitor(void)
{
	int data32 = 0;

	if (!(rx.ddc_filter_en || is_ddc_filter_en()))
		return;

	i2c_err_cnt++;
	data32 = hdmirx_rd_top(TOP_EDID_GEN_CNTL);
	if ((i2c_err_cnt % 3) != 1)
		data32 = ((data32 & (~0xff)) | 0x9);
	else
		data32 = ((data32 & (~0xff)) | 0x4f);
	hdmirx_wr_top(TOP_EDID_GEN_CNTL,  data32);
	if (log_level & EDID_LOG)
		rx_pr("data32: 0x%x,\n", data32);
}

bool is_ddc_filter_en(void)
{
	bool ret = false;
	int data32 = 0;

	data32 = hdmirx_rd_top(TOP_EDID_GEN_CNTL);
	if ((data32 & 0xff) > 0x10)
		ret = true;

	return ret;
}

bool rx_need_ddc_monitor(void)
{
	bool ret = true;

	if (ddc_dbg_en)
		ret = false;

	if (rx.chip_id > CHIP_ID_T5W || (is_meson_t7_cpu() && is_meson_rev_c()))
		ret = false;

	return ret;
}

/*
 * FUNC: rx_ddc_active_monitor
 * ddc active monitor
 */
void rx_ddc_active_monitor(void)
{
	u32 temp = 0;

	if (!rx_need_ddc_monitor())
		return;

	if (rx.state != FSM_WAIT_CLK_STABLE)
		return;

	if (!((1 << rx.port) & EDID_DETECT_PORT))
		return;

	//if (rx.ddc_filter_en)
		//return;

	switch (rx.port) {
	case E_PORT0:
		temp = hdmirx_rd_top(TOP_EDID_GEN_STAT);
		break;
	case E_PORT1:
		temp = hdmirx_rd_top(TOP_EDID_GEN_STAT_B);
		break;
	case E_PORT2:
		temp = hdmirx_rd_top(TOP_EDID_GEN_STAT_C);
		break;
	case E_PORT3:
		temp = hdmirx_rd_top(TOP_EDID_GEN_STAT_D);
		break;
	default:
		break;
	}

	temp = temp & 0xff;
	/*0x0a, 0x15 for hengyi ops-pc. refer to 88378
	 *0x14 for special spliter. refer to 72949
	 *0x13 for 8268 refer to 73940
	 *0x2 need to be removed for white pc
	 *fix edid filter setting
	 */
	if (temp < 0x3f &&
		temp != 0x1 &&
		temp != 0x3 &&
		temp != 0x8 &&
		temp != 0xa &&
		temp != 0xc &&
		temp != 0x13 &&
		temp != 0x14 &&
		temp != 0x15 &&
		temp) {
		rx.ddc_filter_en = true;
		if (log_level & EDID_LOG)
			rx_pr("port: %d, edid_status: 0x%x,\n", rx.port, temp);
	} else {
		if (log_level & EDID_LOG)
			rx_pr("port: %d, edid_status: 0x%x,\n", rx.port, temp);
		rx.ddc_filter_en = false;
	}
}

u32 rx_get_ecc_pkt_cnt(void)
{
	u32 pkt_cnt;

	pkt_cnt = hdmirx_rd_cor(RX_PKT_CNT_DP2_IVCRX) |
			(hdmirx_rd_cor(RX_PKT_CNT2_DP2_IVCRX) << 8);

	return pkt_cnt;
}

u32 rx_get_ecc_err(void)
{
	u32 err_cnt;

	hdmirx_wr_cor(RX_ECC_CTRL_DP2_IVCRX, 3);
	err_cnt = hdmirx_rd_cor(RX_HDCP_ERR_DP2_IVCRX) |
			(hdmirx_rd_cor(RX_HDCP_ERR2_DP2_IVCRX) << 8);

	return err_cnt;
}

void rx_hdcp_22_sent_reauth(void)
{
	hdmirx_wr_bits_cor(CP2PAX_CTRL_0_HDCP2X_IVCRX, _BIT(7), 0);
	hdmirx_wr_bits_cor(CP2PAX_CTRL_0_HDCP2X_IVCRX, _BIT(7), 0);
	//hdmirx_wr_cor(RX_ECC_CTRL_DP2_IVCRX, 3);
	hdmirx_wr_bits_cor(CP2PAX_CTRL_0_HDCP2X_IVCRX, _BIT(7), 1);
}

void rx_hdcp_14_sent_reauth(void)
{
	hdmirx_wr_cor(RX_HDCP_DEBUG_HDCP1X_IVCRX, 0x80);
}

void rx_check_ecc_error(void)
{
	u32 ecc_pkt_cnt;

	rx.ecc_err = rx_get_ecc_err();
	ecc_pkt_cnt = rx_get_ecc_pkt_cnt();
	if (log_level & ECC_LOG)
		rx_pr("ecc:%d-%d\n",
			  rx.ecc_err,
			  ecc_pkt_cnt);
	if (rx.ecc_err && ecc_pkt_cnt) {
		rx.ecc_err_frames_cnt++;
		if (rx.ecc_err_frames_cnt % 20 == 0)
			rx_pr("ecc:%d\n", rx.ecc_err);
		if (rx.ecc_err == ecc_pkt_cnt)
			skip_frame(2);
	} else {
		rx.ecc_err_frames_cnt = 0;
	}
}

int is_rx_hdcp14key_loaded_t7(void)
{
	return rx_smc_cmd_handler(HDCP14_RX_QUERY, 0);
}

int is_rx_hdcp22key_loaded_t7(void)
{
	return rx_smc_cmd_handler(HDCP22_RX_QUERY, 0);
}

int is_rx_hdcp14key_crc_pass(void)
{
	return rx_smc_cmd_handler(HDCP14_CRC_STS, 0);
}

int is_rx_hdcp22key_crc0_pass(void)
{
	return rx_smc_cmd_handler(HDCP22_CRC0_STS, 0);
}

int is_rx_hdcp22key_crc1_pass(void)
{
	return rx_smc_cmd_handler(HDCP22_CRC1_STS, 0);
}
