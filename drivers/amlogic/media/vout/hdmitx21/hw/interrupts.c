// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
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
#include <linux/clk.h>

#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_ddc.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>
#include <linux/amlogic/media/vout/hdmi_tx_ext.h>
#include "common.h"

static void intr2_sw_handler(struct intr_t *);
static void top_hpd_intr_stub_handler(struct intr_t *);

static pf_callback earc_hdmitx_hpdst;

pf_callback *hdmitx21_earc_hpdst(void)
{
	return &earc_hdmitx_hpdst;
}

union intr_u hdmi_all_intrs = {
	.entity = {
		.top_intr = {
			.intr_mask_reg = HDMITX_TOP_INTR_MASKN,
			.intr_st_reg = HDMITX_TOP_INTR_STAT,
			.intr_clr_reg = HDMITX_TOP_INTR_STAT_CLR,
			.intr_top_bit = BIT(1) | BIT(2),
			.mask_data = BIT(1) | BIT(2),
			.callback = top_hpd_intr_stub_handler,
		},
		.tpi_intr = { /* for hdcp1 */
			.intr_mask_reg = TPI_INTR_EN_IVCTX,
			.intr_st_reg = TPI_INTR_ST0_IVCTX,
			.intr_clr_reg = TPI_INTR_ST0_IVCTX,
			.intr_top_bit = BIT(0),
			.mask_data = 0xff,
			.callback = hdcp1x_intr_handler,
		},
		.intr2 = {
			.intr_mask_reg = INTR2_MASK_SW_TPI_IVCTX,
			.intr_st_reg = INTR2_SW_TPI_IVCTX,
			.intr_clr_reg = INTR2_SW_TPI_IVCTX,
			.intr_top_bit = BIT(0),
			.mask_data = BIT(0),
			.callback = intr2_sw_handler,
		},
		.cp2tx_intr0 = {
			.intr_mask_reg = CP2TX_INTR0_MASK_IVCTX,
			.intr_st_reg = CP2TX_INTR0_IVCTX,
			.intr_clr_reg = CP2TX_INTR0_IVCTX,
			.intr_top_bit = BIT(0),
			.mask_data = 0xFF,
			.callback = hdcp2x_intr_handler,
		},
		.cp2tx_intr1 = {
			.intr_mask_reg = CP2TX_INTR1_MASK_IVCTX,
			.intr_st_reg = CP2TX_INTR1_IVCTX,
			.intr_clr_reg = CP2TX_INTR1_IVCTX,
			.intr_top_bit = BIT(0),
			.mask_data = 0xFF,
			.callback = hdcp2x_intr_handler,
		},
		.cp2tx_intr2 = {
			.intr_mask_reg = CP2TX_INTR2_MASK_IVCTX,
			.intr_st_reg = CP2TX_INTR2_IVCTX,
			.intr_clr_reg = CP2TX_INTR2_IVCTX,
			.intr_top_bit = BIT(0),
			.mask_data = 0xFF,
			.callback = hdcp2x_intr_handler,
		},
		.cp2tx_intr3 = {
			.intr_mask_reg = CP2TX_INTR3_MASK_IVCTX,
			.intr_st_reg = CP2TX_INTR3_IVCTX,
			.intr_clr_reg = CP2TX_INTR3_IVCTX,
			.intr_top_bit = BIT(0),
			.mask_data = BIT(2) | BIT(3),
			.callback = hdcp2x_intr_handler,
		},
	},
};

void intr_status_save_clr_cp2txs(u8 regs[])
{
	regs[0] = hdmi_all_intrs.entity.cp2tx_intr0.st_data;
	regs[1] = hdmi_all_intrs.entity.cp2tx_intr1.st_data;
	regs[2] = hdmi_all_intrs.entity.cp2tx_intr2.st_data;
	regs[3] = hdmi_all_intrs.entity.cp2tx_intr3.st_data;
	hdmi_all_intrs.entity.cp2tx_intr0.st_data = 0;
	hdmi_all_intrs.entity.cp2tx_intr1.st_data = 0;
	hdmi_all_intrs.entity.cp2tx_intr2.st_data = 0;
	hdmi_all_intrs.entity.cp2tx_intr3.st_data = 0;
}

static void top_hpd_intr_stub_handler(struct intr_t *intr)
{
}

static void intr2_sw_handler(struct intr_t *intr)
{
	static u32 vsync_cnt;

	vsync_cnt++;
	if (vsync_cnt % 64 == 0) {
		/* blank here */
		/* pr_info("%s[%d] vsync_cnt %d\n", __func__, __LINE__, vsync_cnt); */
	}
}

static void _intr_enable(struct intr_t *pint, bool en)
{
	hdmitx21_wr_reg(pint->intr_mask_reg, en ? pint->mask_data : 0);
	hdmitx21_set_bit(HDMITX_TOP_INTR_MASKN, pint->intr_top_bit, en);
}

void hdcp_enable_intrs(bool en)
{
	_intr_enable((struct intr_t *)&hdmi_all_intrs.entity.tpi_intr, en);
	_intr_enable((struct intr_t *)&hdmi_all_intrs.entity.cp2tx_intr0, en);
	_intr_enable((struct intr_t *)&hdmi_all_intrs.entity.cp2tx_intr1, en);
	_intr_enable((struct intr_t *)&hdmi_all_intrs.entity.cp2tx_intr2, en);
	_intr_enable((struct intr_t *)&hdmi_all_intrs.entity.cp2tx_intr3, en);
}

static void hdmitx_phy_bandgap_en(struct hdmitx_dev *hdev)
{
	switch (hdev->data->chip_type) {
	case MESON_CPU_ID_T7:
		hdmitx21_phy_bandgap_en_t7();
		break;
	default:
		break;
	}
}

void hdmitx_top_intr_handler(struct work_struct *work)
{
	int i;
	struct intr_t *pint = (struct intr_t *)&hdmi_all_intrs;
	u32 val;
	struct hdmitx_dev *hdev = container_of((struct delayed_work *)work,
		struct hdmitx_dev, work_internal_intr);

	if (pint->st_data) {
		u32 dat_top;

		dat_top = pint->st_data;
		pint->st_data = 0;
		/* check HPD status */
		if ((dat_top & (1 << 1)) && (dat_top & (1 << 2))) {
			if (hdmitx21_hpd_hw_op(HPD_READ_HPD_GPIO))
				dat_top &= ~(1 << 2);
			else
				dat_top &= ~(1 << 1);
		}
		if ((dat_top & 0x6) && hdev->hpd_lock) {
			pr_info("HDMI hpd locked\n");
			goto next;
		}
		/* HPD rising */
		if (dat_top & (1 << 1)) {
			hdev->hdmitx_event |= HDMI_TX_HPD_PLUGIN;
			hdev->hdmitx_event &= ~HDMI_TX_HPD_PLUGOUT;
			hdmitx_phy_bandgap_en(hdev);
			if (earc_hdmitx_hpdst)
				earc_hdmitx_hpdst(true);
			queue_delayed_work(hdev->hdmi_wq,
					   &hdev->work_hpd_plugin, HZ / 2);
		}
		/* HPD falling */
		if (dat_top & (1 << 2)) {
			queue_delayed_work(hdev->hdmi_wq,
					   &hdev->work_aud_hpd_plug, 2 * HZ);
			hdev->hdmitx_event |= HDMI_TX_HPD_PLUGOUT;
			hdev->hdmitx_event &= ~HDMI_TX_HPD_PLUGIN;
			if (earc_hdmitx_hpdst)
				earc_hdmitx_hpdst(false);
			queue_delayed_work(hdev->hdmi_wq,
				&hdev->work_hpd_plugout, 0);
		}
	}
next:
	/* already called top_intr.callback, next others */
	for (i = 1; i < sizeof(union intr_u) / sizeof(struct intr_t); i++) {
		pint++;
		if (pint->st_data) {
			val = pint->st_data;
			pint->callback(pint);
			pint->st_data = 0;
		}
	}
}

static void intr_status_save_and_clear(void)
{
	int i;
	struct intr_t *pint = (struct intr_t *)&hdmi_all_intrs;

	for (i = 0; i < sizeof(union intr_u) / sizeof(struct intr_t); i++) {
		pint->st_data = hdmitx21_rd_reg(pint->intr_st_reg);
		hdmitx21_wr_reg(pint->intr_clr_reg, pint->st_data);
		pint++;
	}
}

static irqreturn_t intr_handler(int irq, void *dev)
{
	struct hdmitx_dev *hdev = (struct hdmitx_dev *)dev;
	struct hdcp_t *amhdcp = (struct hdcp_t *)hdev->am_hdcp;

	intr_status_save_and_clear();

	queue_delayed_work(amhdcp->hdcp_wq, &hdev->work_internal_intr, 1);

	return IRQ_HANDLED;
}

void hdmitx_setupirqs(struct hdmitx_dev *phdev)
{
	int r;

	hdmitx21_wr_reg(HDMITX_TOP_INTR_STAT_CLR, 0x7);
	r = request_irq(phdev->irq_hpd, &intr_handler,
			IRQF_SHARED, "hdmitx",
			(void *)phdev);
}
