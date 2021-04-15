// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __USB_V2_HEADER_
#define __USB_V2_HEADER_

#include <linux/usb/phy.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>
#include <linux/clk.h>

#define PHY_REGISTER_SIZE	0x20
#define USB_PHY_MAX_NUMBER  0x8
#define ID_GPIO_IRQ_FLAGS \
	(IRQF_SHARED | IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING)

/* Register definitions */

int aml_new_usb_v2_register_notifier(struct notifier_block *nb);
int aml_new_usb_v2_unregister_notifier(struct notifier_block *nb);
int aml_new_otg_register_notifier(struct notifier_block *nb);
int aml_new_otg_unregister_notifier(struct notifier_block *nb);

struct u2p_aml_regs_v2 {
	void __iomem	*u2p_r_v2[4];
};

union u2p_r0_v2 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned host_device:1;
		unsigned power_ok:1;
		unsigned hast_mode:1;
		unsigned POR:1;
		unsigned IDPULLUP0:1;
		unsigned DRVVBUS0:1;
		unsigned reserved:26;
	} b;
};

union u2p_r1_v2 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned phy_rdy:1;
		unsigned IDDIG0:1;
		unsigned OTGSESSVLD0:1;
		unsigned VBUSVALID0:1;
		unsigned reserved:28;
	} b;
};

union u2p_r2_v2 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned iddig_sync:1;
		unsigned iddig_reg:1;
		unsigned iddig_cfg:2;
		unsigned iddig_en0:1;
		unsigned iddig_en1:1;
		unsigned iddig_curr:1;
		unsigned usb_iddig_irq:1;
		unsigned iddig_th:8;
		unsigned iddig_cnt:8;
		unsigned reserved:8;
	} b;
};

union u2p_r3_v2 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned vbusdig_sync:1;
		unsigned vbusdig_reg:1;
		unsigned vbusdig_cfg:2;
		unsigned vbusdig_en0:1;
		unsigned vbusdig_en1:1;
		unsigned vbusdig_curr:1;
		unsigned usb_vbusdig_irq:1;
		unsigned vbusdig_th:8;
		unsigned vbusdig_cnt:8;
		unsigned reserved:8;
	} b;
};

struct usb_aml_regs_v2 {
	void __iomem	*usb_r_v2[8];
};

union usb_r0_v2 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned reserved:17;
		unsigned p30_lane0_tx2rx_loopback:1;
		unsigned p30_lane0_ext_pclk_reg:1;
		unsigned p30_pcs_rx_los_mask_val:10;
		unsigned u2d_ss_scaledown_mode:2;
		unsigned u2d_act:1;
	} b;
};

union usb_r1_v2 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned u3h_bigendian_gs:1;
		unsigned u3h_pme_en:1;
		unsigned u3h_hub_port_overcurrent:5;
		unsigned u3h_hub_port_perm_attach:5;
		unsigned u3h_host_u2_port_disable:3;
		unsigned u3h_host_u3_port_disable:2;
		unsigned u3h_host_port_power_control_present:1;
		unsigned u3h_host_msi_enable:1;
		unsigned u3h_fladj_30mhz_reg:6;
		unsigned p30_pcs_tx_swing_full:7;
	} b;
};

union usb_r2_v2 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned reserved:20;
		unsigned p30_pcs_tx_deemph_3p5db:6;
		unsigned p30_pcs_tx_deemph_6db:6;
	} b;
};

union usb_r3_v2 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned p30_ssc_en:1;
		unsigned p30_ssc_range:3;
		unsigned p30_ssc_ref_clk_sel:9;
		unsigned p30_ref_ssp_en:1;
		unsigned reserved:6;
		unsigned p31_pcs_tx_deemph_3p5db:6;
		unsigned p31_pcs_tx_deemph_6db:6;
	} b;
};

union usb_r4_v2 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned p21_PORTRESET0:1;
		unsigned p21_SLEEPM0:1;
		unsigned mem_pd:2;
		unsigned p21_only:1;
		unsigned u3h_hub_port_overcurrent:5;
		unsigned u3h_hub_port_perm_attach:5;
		unsigned u3h_host_u2_port_disable:3;
		unsigned u3h_host_u3_port_disable:2;
	} b;
};

union usb_r5_v2 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned iddig_sync:1;
		unsigned iddig_reg:1;
		unsigned iddig_cfg:2;
		unsigned iddig_en0:1;
		unsigned iddig_en1:1;
		unsigned iddig_curr:1;
		unsigned usb_iddig_irq:1;
		unsigned iddig_th:8;
		unsigned iddig_cnt:8;
		unsigned reserved:8;
	} b;
};

union usb_r7_v2 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned p31_ssc_en:1;
		unsigned p31_ssc_range:3;
		unsigned p31_ssc_ref_clk_sel:9;
		unsigned p31_ref_ssp_en:1;
		unsigned reserved:2;
		unsigned p31_pcs_tx_deemph_6db:6;
		unsigned reserve:3;
		unsigned p31_pcs_tx_swing_full:7;
	} b;
};

struct amlogic_usb_v2 {
	struct usb_phy		phy;
	struct device		*dev;
	void __iomem	*regs;
	void __iomem	*reset_regs;
	void __iomem	*phy_cfg[4];
	void __iomem	*phy3_cfg;
	void __iomem	*phy3_cfg_r1;
	void __iomem	*phy3_cfg_r2;
	void __iomem	*phy3_cfg_r4;
	void __iomem	*phy3_cfg_r5;
	void __iomem	*phy31_cfg;
	void __iomem	*phy31_cfg_r1;
	void __iomem	*phy31_cfg_r2;
	void __iomem	*phy31_cfg_r4;
	void __iomem	*phy31_cfg_r5;
	void __iomem	*usb2_phy_cfg;
	u32 pll_setting[8];
	int phy_cfg_state[4];
	/* Set VBus Power though GPIO */
	int vbus_power_pin;
	int vbus_power_pin_work_mask;
	int otg;
	u32 version;
	struct delayed_work	work;
	struct delayed_work	id_gpio_work;
	struct gpio_desc *usb_gpio_desc;
	struct gpio_desc *idgpiodesc;

	int portnum;
	int suspend_flag;
	int phy_version;
	u32 phy_reset_level_bit[USB_PHY_MAX_NUMBER];
	u32 usb_reset_bit;
	u32 otg_phy_index;
	u32 reset_level;
	struct clk		*clk;
	struct clk		*usb_clk;
	struct clk		*gate0_clk;
	struct clk		*gate1_clk;
	u32 portconfig_31;
	u32 portconfig_30;
	void __iomem	*usb_phy_trim_reg;
	u32 phy_id;
	struct clk		*general_clk;
	u32 usb3_apb_reset_bit;
	u32 usb3_phy_reset_bit;
	u32 usb3_reset_shift;
};

union phy3_r1 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned phy_tx1_term_offset:5;
		unsigned phy_tx0_term_offset:5;
		unsigned phy_rx1_eq:3;
		unsigned phy_rx0_eq:3;
		unsigned phy_los_level:5;
		unsigned phy_los_bias:3;
		unsigned phy_ref_clkdiv2:1;
		unsigned phy_mpll_multiplier:7;
	} b;
};


union phy3_r2 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned pcs_tx_deemph_gen2_6db:6;
		unsigned pcs_tx_deemph_gen2_3p5db:6;
		unsigned pcs_tx_deemph_gen1:6;
		unsigned phy_tx_vboost_lvl:3;
		unsigned reserved:11;
	} b;
};


union phy3_r4 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned phy_cr_write:1;
		unsigned phy_cr_read:1;
		unsigned phy_cr_data_in:16;
		unsigned phy_cr_cap_data:1;
		unsigned phy_cr_cap_addr:1;
		unsigned reserved:12;
	} b;
};

union phy3_r5 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned phy_cr_data_out:16;
		unsigned phy_cr_ack:1;
		unsigned phy_bs_out:1;
		unsigned reserved:14;
	} b;
};

struct amlogic_usb_m31 {
	struct usb_phy		phy;
	struct device		*dev;
	struct clk		*clk;
	int suspend_flag;
	u32 portnum;
	void __iomem	*phy3_cfg;
};

union phy_m31_r0 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned PHY_SSCG_ON:1;
		unsigned PHY_CTLE_OFF:1;
		unsigned REFPAD_EXT_100M_EN:1;
		unsigned CLKPADEN:1;
		unsigned U3_SSRX_SEL:1;
		unsigned U3_SSTX_SEL:1;
		unsigned PCIE_XTLSEL:1;
		unsigned USB_XTLSEL:3;
		unsigned U3_HOST_PHY:1;
		unsigned USB_CLKSEL:1;
		unsigned PCIE_CLKSEL:2;
		unsigned OSCOUTEN:1;
		unsigned PLL_EN:1;
		unsigned LPM_ALIVE:1;
		unsigned PHY_SEL:2;
		unsigned p_datawidth:1;
		unsigned u_datawidth:1;
		unsigned p_rx_termination:1;
		unsigned FSLSSERIALMODE:1;
		unsigned TX_SE0:1;
		unsigned TX_DAT:1;
		unsigned TX_ENABLE_N:1;
		unsigned TXBITSTUFFENABLEH:1;
		unsigned TXBITSTUFFENABLE:1;
		unsigned UTMI_VCONTROLLOADM:1;
		unsigned RESERVED:3;
	} b;
};

union phy_m31_r1 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned U31_INTERNALLOOPBACK:1;
		unsigned BIST_TXDEMPH_SEL:2;
		unsigned LS_EN:1;
		unsigned HS_BIST_MODE:1;
		unsigned LFPSRX_EN:1;
		unsigned VCONTROL:6;
		unsigned debug_sel:7;
		unsigned RESERVED:13;
	} b;
};

union phy_m31_r2 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned CC_RP_0D9_EN:1;
		unsigned CC_RP_1D5_EN:2;
		unsigned CC_RP_3D0_EN:1;
		unsigned CC_HOST_EN:1;
		unsigned CC_EN:1;
		unsigned bist_bypass_sel:1;
		unsigned RESERVED:25;
	} b;
};

union phy_m31_r3 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned phy_ref_clk_ctrl:2;
		unsigned clean_st:1;
		unsigned bist_bypass_cfg:29;
	} b;
};

union phy_m31_r4 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned bypass_sel:16;
		unsigned bypass_cfg:16;
	} b;
};

union phy_m31_r5 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned REFCLK_OFF_REQ_qre:1;
		unsigned PCLK_OFF_P1_REQ_pre:1;
		unsigned SQ_OFF_P1_REQ_pre:1;
		unsigned TX_OFF_P1_REQ_pre:1;
		unsigned PMCLK_EN_pre:1;
		unsigned REFCLK_OFF_ST:1;
		unsigned PCLK_OFF_P1_ST:1;
		unsigned SQ_OFF_P1_ST:1;
		unsigned TX_OFF_P1_ST:1;
		unsigned CAL_FB_VAL_L0_pre:1;
		unsigned pm_st:4;
		unsigned BIST_OK:1;
		unsigned BIST_OK_U2:1;
		unsigned pcie_rx_elecidle:1;
		unsigned linestate:2;
		unsigned usb_rx_elecidle:1;
		unsigned RESERVED:12;
	} b;
};

union phy_m31_r6 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned CAL_PARAM_OUT_sync_low:32;
	} b;
};

union phy_m31_r7 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned CAL_PARAM_OUT_sync_middle:32;
	} b;
};

union phy_m31_r8 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned CAL_PARAM_OUT_sync_high:14;
		unsigned RESERVED:18;
	} b;
};

union phy_m31_r9 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned CAL_FB_IN_L0_pre_low:32;
	} b;
};

union phy_m31_r10 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned CAL_FB_IN_L0_pre_middle:32;
	} b;
};

union phy_m31_r11 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned CAL_FB_IN_L0_pre_high:14;
		unsigned RESERVED:18;
	} b;
};

union phy_m31_r12 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned bypass_cal_fb_low:32;
	} b;
};

union phy_m31_r13 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned bypass_cal_fb_middle:32;
	} b;
};

union phy_m31_r14 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned bypass_cal_fb_high:14;
		unsigned RESERVED:18;
	} b;
};

union phy_m31_r15 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned m31phy_debug_out:32;
	} b;
};

int aml_new_otg_get_mode(void);
int aml_new_usb_get_mode(void);
int amlogic_crg_device_usb2_init(u32 phy_id);
#ifdef CONFIG_AMLOGIC_USB3PHY
void aml_new_otg_init(void);
#endif

#endif
