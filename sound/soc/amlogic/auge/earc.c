// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

//#define DEBUG

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <linux/workqueue.h>
#include <linux/amlogic/iomap.h>
#include <linux/clk-provider.h>
#include <linux/amlogic/cpu_version.h>

#include <linux/amlogic/media/sound/hdmi_earc.h>
#include "resample.h"
#include "ddr_mngr.h"
#include "earc_hw.h"
#include "audio_utils.h"
#include "earc.h"
#include "frhdmirx_hw.h"
#include "sharebuffer.h"
#include "spdif_hw.h"
#include "../common/audio_uevent.h"
#include "audio_controller.h"
#include "hdmirx_arc_iomap.h"
#if (defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI ||\
		defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI_MODULE)
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#endif

#define DRV_NAME "EARC"

#define EARCRX_DEFAULT_LATENCY 100

static DEFINE_MUTEX(earc_mutex);

u8 default_rx_cds[] = {
	0x01, 0x01, 0x24, 0x38,/* Version, BLOCK_ID, PAYLOAD_LENGTH, Data Block Header Byte */
	0x09, 0x7f, 0x05, 0x0f, 0x04, 0x05,/* L-PCM */
	0x67, 0x7e, 0x03,/* MAT */
	0x57, 0x06, 0x03,/* EAC3 */
	0x15, 0x07, 0x50,/* AC3 */
	0x3f, 0x06, 0xc0,/* DTS */
	0x5f, 0x7e, 0x03,/* DTSHD */
	0x5f, 0x7e, 0x01,/* DTSHD */
	0x83, 0x4f, 0x00, 0x00,/* Speaker Allocation Data Block */
	0xe6, 0x11, 0x46, 0xd0, 0x00, 0x70, 0x00/* Vendor-Specific Data Block */
};

/*
 * IEC958 controller(mixer) functions
 *
 *      Channel status get/put control
 *      User bit value get/put control
 *      Valid bit value get control
 *      DPLL lock status get control
 *      User bit sync mode selection control
 */
static int IEC958_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;

	return 0;
}

#define SND_IEC958(xname, xhandler_get, xhandler_put) \
{                                          \
	.iface = SNDRV_CTL_ELEM_IFACE_PCM,   \
	.name = xname,           \
	.info = IEC958_info,      \
	.get = xhandler_get,     \
	.put = xhandler_put,     \
}

enum work_event {
	EVENT_NONE,
	EVENT_RX_ANA_AUTO_CAL = 0x1 << 0,
	EVENT_TX_ANA_AUTO_CAL = 0x1 << 1,
};

struct earc_chipinfo {
	unsigned int earc_spdifout_lane_mask;
	bool rx_dmac_sync_int;
	bool rterm_on;
	bool no_ana_auto_cal;
	bool chnum_mult_mode;
	bool unstable_tick_sel;
	bool rx_enable;
	bool tx_enable;
	int arc_version;
};

struct earc {
	struct aml_audio_controller *actrl;
	struct device *dev;

	struct clk *clk_rx_cmdc;
	struct clk *clk_rx_dmac;
	struct clk *clk_rx_cmdc_srcpll;
	struct clk *clk_rx_dmac_srcpll;
	struct clk *clk_tx_gate;
	struct clk *clk_tx_cmdc;
	struct clk *clk_tx_dmac;
	struct clk *clk_tx_cmdc_srcpll;
	struct clk *clk_tx_dmac_srcpll;

	/* for 44100hz samplerate */
	struct clk *clk_src_cd;

	struct regmap *tx_cmdc_map;
	struct regmap *tx_dmac_map;
	struct regmap *tx_top_map;
	struct regmap *rx_cmdc_map;
	struct regmap *rx_dmac_map;
	struct regmap *rx_top_map;

	struct toddr *tddr;
	struct frddr *fddr;

	int irq_earc_rx;
	int irq_earc_tx;

	/* audio codec type for tx */
	enum audio_coding_types tx_audio_coding_type;
	/* audio codec type for tx by ui kcontrol setting */
	enum audio_coding_types ui_tx_audio_coding_type;

	/* freq for tx dmac clk */
	int tx_dmac_freq;

	/* whether dmac clock is on */
	bool rx_dmac_clk_on;
	spinlock_t rx_lock;  /* rx dmac clk lock */
	spinlock_t tx_lock;  /* tx dmac clk lock */
	bool tx_dmac_clk_on;

	/* do analog auto calibration when bootup */
	struct work_struct work;
	enum work_event event;
	bool rx_bootup_auto_cal;
	bool tx_bootup_auto_cal;

	struct earc_chipinfo *chipinfo;

	/* mute in channel status */
	bool rx_cs_mute;
	bool tx_cs_mute;

	/* Channel Allocation */
	unsigned int tx_cs_lpcm_ca;

	/* channel map */
	struct snd_pcm_chmap *rx_chmap;
	struct snd_pcm_chmap *tx_chmap;

	struct snd_pcm_substream *substreams[2];

	struct work_struct rx_dmac_int_work;
	struct delayed_work tx_resume_work;
	u8 rx_cds_data[CDS_MAX_BYTES];
	u8 tx_cds_data[CDS_MAX_BYTES];
	enum sharebuffer_srcs samesource_sel;
	struct samesource_info ss_info;
	bool earctx_on;
	/*
	 * The device(eARC Rx) type, our chip is eARC Tx
	 * ATNDTYP_DISCNCT: For nothing device connected
	 * ATNDTYP_ARC: The device(eARC Rx) is ARC device
	 * ATNDTYP_EARC: The device(eARC Rx) is eARC device
	 */
	enum attend_type earctx_connected_device_type;
	unsigned int rx_status0;
	unsigned int rx_status1;
	bool tx_earc_mode;
	bool tx_reset_hpd;
	int tx_heartbeat_state;
	int tx_stream_state;
	u8 tx_latency;
	struct delayed_work send_uevent;
	bool tx_mute;
	int same_src_on;
	/* ui earc/arc switch */
	int tx_ui_flag;
	/* earctx arc port id */
	int earctx_port;
	/* get from hdmirx */
	int earctx_5v;
	/* Standardization value by normal setting */
	unsigned int standard_tx_dmac;
	unsigned int standard_tx_freq;
	int suspend_clk_off;
	bool resumed;
	/* ui earc/arc rx switch */
	int rx_ui_flag;
	/* get from hdmitx */
	int earcrx_5v;
	struct work_struct tx_hold_bus_work;
	struct delayed_work gain_disable;
	int tx_arc_status;
	int rx_cs_ready;
	/* stream ready to read the channel status and audio type */
	bool stream_stable;
	struct delayed_work rx_stable_work;
};

static struct earc *s_earc;

bool is_earc_spdif(void)
{
	return !!s_earc;
}

bool get_earcrx_chnum_mult_mode(void)
{
	if (s_earc && s_earc->chipinfo)
		return s_earc->chipinfo->chnum_mult_mode;
	return false;
}

#define EARC_BUFFER_BYTES (512 * 1024 * 2)
#define EARC_RATES      (SNDRV_PCM_RATE_8000_192000)
#define EARC_FORMATS    (SNDRV_PCM_FMTBIT_S16_LE |\
			SNDRV_PCM_FMTBIT_S24_LE |\
			SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_pcm_hardware earc_hardware = {
	.info =
		SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_BLOCK_TRANSFER |
		SNDRV_PCM_INFO_PAUSE,

	.formats = EARC_FORMATS,

	.period_bytes_min = 64,
	.period_bytes_max = 128 * 1024,
	.periods_min = 2,
	.periods_max = 1024,
	.buffer_bytes_max = EARC_BUFFER_BYTES,

	.rate_min = 8000,
	.rate_max = 192000,
	.channels_min = 1,
	.channels_max = 32,
};

static void earc_clock_enable(void)
{
	struct earc *p_earc = s_earc;
	int ret = 0;

	if (!p_earc)
		return;

	if (p_earc->suspend_clk_off) {
		if (p_earc->chipinfo->tx_enable) {
			if (!IS_ERR(p_earc->clk_tx_cmdc) && !IS_ERR(p_earc->clk_tx_cmdc_srcpll)) {
				ret = clk_set_parent(p_earc->clk_tx_cmdc,
						p_earc->clk_tx_cmdc_srcpll);
				if (ret)
					dev_err(p_earc->dev, "Can't resume set clk_tx_cmdc parent clock\n");
			}
			if (!IS_ERR(p_earc->clk_tx_dmac) && !IS_ERR(p_earc->clk_tx_dmac_srcpll)) {
				unsigned long flags;

				ret = clk_set_parent(p_earc->clk_tx_dmac,
						p_earc->clk_tx_dmac_srcpll);
				if (ret)
					dev_err(p_earc->dev, "Can't resume set clk_tx_dmac parent clock\n");
				ret = clk_prepare_enable(p_earc->clk_tx_dmac);
				if (ret)
					dev_err(p_earc->dev, "Can't resume enable earc clk_tx_dmac\n");

				spin_lock_irqsave(&p_earc->tx_lock, flags);
				p_earc->tx_dmac_clk_on = true;
				spin_unlock_irqrestore(&p_earc->tx_lock, flags);

				ret = clk_prepare_enable(p_earc->clk_tx_cmdc);
				if (ret)
					dev_err(p_earc->dev, "Can't resume enable earc clk_tx_cmdc\n");
			}
		}

		if (p_earc->chipinfo->rx_enable) {
			if (!IS_ERR(p_earc->clk_rx_cmdc) && !IS_ERR(p_earc->clk_rx_cmdc_srcpll)) {
				ret = clk_set_parent(p_earc->clk_rx_cmdc,
						p_earc->clk_rx_cmdc_srcpll);
				if (ret)
					dev_err(p_earc->dev, "Can't resume set clk_rx_cmdc parent clock\n");
			}
			if (!IS_ERR(p_earc->clk_rx_dmac) && !IS_ERR(p_earc->clk_rx_dmac_srcpll)) {
				ret = clk_set_parent(p_earc->clk_rx_dmac,
						p_earc->clk_rx_dmac_srcpll);
				if (ret)
					dev_err(p_earc->dev, "Can't resume set clk_rx_dmac parent clock\n");
				ret = clk_prepare_enable(p_earc->clk_rx_cmdc);
				if (ret)
					dev_err(p_earc->dev, "Can't resume enable earc clk_rx_cmdc\n");
				ret = clk_prepare_enable(p_earc->clk_rx_dmac);
				if (ret)
					dev_err(p_earc->dev, "Can't resume enable earc clk_rx_dmac\n");
				ret = clk_prepare_enable(p_earc->clk_rx_dmac_srcpll);
				if (ret)
					dev_err(p_earc->dev, "Can't resume enable earc clk_rx_dmac_srcpll\n");
			}
		}
	}
}

static void tx_hold_bus_work_func(struct work_struct *p_work)
{
	struct earc *p_earc = container_of(p_work, struct earc,
					   tx_hold_bus_work);
	unsigned long flags;

	spin_lock_irqsave(&s_earc->tx_lock, flags);
	if (p_earc->tx_dmac_clk_on)
		earctx_set_cs_mute(p_earc->tx_dmac_map, 1);
	spin_unlock_irqrestore(&s_earc->tx_lock, flags);
	msleep(105);

	/* at lease 100ms by hdmi2.1 spec 9.5.2.5 */
	spin_lock_irqsave(&s_earc->tx_lock, flags);
	if (p_earc->tx_dmac_clk_on)
		earctx_dmac_hold_bus_and_mute(p_earc->tx_dmac_map, true);
	spin_unlock_irqrestore(&s_earc->tx_lock, flags);

	msleep(260);

	spin_lock_irqsave(&s_earc->tx_lock, flags);
	if (p_earc->tx_dmac_clk_on) {
		earctx_dmac_hold_bus_and_mute(p_earc->tx_dmac_map, false);
		earctx_set_cs_mute(p_earc->tx_dmac_map, 0);
	}
	spin_unlock_irqrestore(&s_earc->tx_lock, flags);
	pr_info("hold bus and mute finish\n");
}

bool aml_get_earctx_enable(void)
{
	unsigned long flags;
	bool ret = false;

	if (!s_earc)
		return false;
	spin_lock_irqsave(&s_earc->tx_lock, flags);
	if (s_earc->tx_dmac_clk_on)
		ret = get_earctx_enable(s_earc->tx_cmdc_map, s_earc->tx_dmac_map);
	spin_unlock_irqrestore(&s_earc->tx_lock, flags);
	return ret;
}

enum attend_type aml_get_earctx_connected_device_type(void)
{
	if (!s_earc)
		return -ENOTCONN;

	return s_earc->earctx_connected_device_type;
}

bool aml_get_earctx_reset_hpd(void)
{
	if (!s_earc)
		return 0;

	return s_earc->tx_reset_hpd;
}

static void earctx_init(int earc_port, bool st)
{
	struct earc *p_earc = s_earc;

	mutex_lock(&earc_mutex);
	if (!p_earc->resumed)
		earc_clock_enable();
	st = st && p_earc->tx_ui_flag;
	if (!st) {
		schedule_delayed_work(&p_earc->send_uevent, 0);
		/* set disconnect when cable plugout */
		p_earc->earctx_connected_device_type = ATNDTYP_DISCNCT;
		/* release earctx same source when cable plug out */
		aml_check_and_release_sharebuffer(NULL, EARCTX_DMAC);
		/* disable arc */
		earctx_cmdc_arc_connect(p_earc->tx_cmdc_map, st);
	} else {
		/* set ARC type as default when cable plugin */
		p_earc->earctx_connected_device_type = ATNDTYP_ARC;
	}

	/* tx cmdc anlog init */
	earctx_cmdc_init(p_earc->tx_top_map, st, p_earc->chipinfo->rterm_on);

	earctx_cmdc_earc_mode(p_earc->tx_cmdc_map, p_earc->tx_earc_mode);
	earctx_cmdc_hpd_detect(p_earc->tx_top_map,
			       p_earc->tx_cmdc_map,
			       earc_port, st);
	mutex_unlock(&earc_mutex);
	if (!st)
		schedule_delayed_work(&p_earc->send_uevent, 0);
	if (!p_earc->tx_bootup_auto_cal) {
		p_earc->tx_bootup_auto_cal = true;
		p_earc->event |= EVENT_TX_ANA_AUTO_CAL;
		schedule_work(&p_earc->work);
	}
	if (st && !p_earc->tx_earc_mode)
		schedule_delayed_work(&p_earc->send_uevent, msecs_to_jiffies(700));
}

static void earcrx_init(bool st)
{
	struct earc *p_earc = s_earc;

	st = st && p_earc->rx_ui_flag;

	if (!p_earc->rx_bootup_auto_cal) {
		p_earc->rx_bootup_auto_cal = true;
		p_earc->event |= EVENT_RX_ANA_AUTO_CAL;
		schedule_work(&p_earc->work);
	}

	/* rx cmdc init */
	earcrx_cmdc_init(p_earc->rx_top_map,
			 st,
			 p_earc->chipinfo->rx_dmac_sync_int,
			 p_earc->chipinfo->rterm_on);

	if (st)
		earcrx_cmdc_int_mask(p_earc->rx_top_map);

	earcrx_cmdc_arc_connect(p_earc->rx_cmdc_map, st);

	earcrx_cmdc_hpd_detect(p_earc->rx_cmdc_map, st);
}

static irqreturn_t earc_ddr_isr(int irq, void *data)
{
	struct snd_pcm_substream *substream =
		(struct snd_pcm_substream *)data;

	if (!snd_pcm_running(substream))
		return IRQ_HANDLED;

	snd_pcm_period_elapsed(substream);

	return IRQ_HANDLED;
}

static void earcrx_update_attend_event(struct earc *p_earc,
				       bool is_earc, bool state)
{
	if (state) {
		unsigned long flags;

		if (is_earc) {
			spin_lock_irqsave(&p_earc->rx_lock, flags);
			if (p_earc->rx_dmac_clk_on)
				earcrx_set_dmac_sync_ctrl(p_earc->rx_dmac_map, true, true);
			spin_unlock_irqrestore(&p_earc->rx_lock, flags);
			audio_send_uevent(p_earc->dev, EARCRX_ATNDTYP_EVENT, ATNDTYP_EARC);
		} else {
			spin_lock_irqsave(&p_earc->rx_lock, flags);
			if (p_earc->rx_dmac_clk_on)
				earcrx_set_dmac_sync_ctrl(p_earc->rx_dmac_map, false, true);
			spin_unlock_irqrestore(&p_earc->rx_lock, flags);
			audio_send_uevent(p_earc->dev, EARCRX_ATNDTYP_EVENT, ATNDTYP_ARC);
		}
	} else {
		audio_send_uevent(p_earc->dev, EARCRX_ATNDTYP_EVENT, ATNDTYP_DISCNCT);
	}
}

static void earcrx_pll_reset(struct earc *p_earc)
{
	int i = 0, count = 0;

	earcrx_dmac_sync_int_enable(p_earc->rx_top_map, 0);
	earcrx_pll_refresh(p_earc->rx_top_map,
			   RST_BY_SELF,
			   true);
	/* wait pll valid */
	usleep_range(350, 400);
	/* bit 31 is 1, and bit 30 is 0, then need reset pll */
	while (earxrx_get_pll_valid(p_earc->rx_top_map) &&
	       !earxrx_get_pll_valid_auto(p_earc->rx_top_map)) {
		earcrx_pll_refresh(p_earc->rx_top_map,
				   RST_BY_SELF,
				   true);
		dev_info(p_earc->dev, "refresh earcrx pll, i %d\n", i++);
		if (i >= 9) {
			dev_info(p_earc->dev,
				 "refresh earcrx pll failed\n");
			break;
		}
		usleep_range(350, 400);
	}

	/* there are 5 times continues bit 31 and bit 30 is 1 */
	i = 0;
	while (true) {
		if (earxrx_get_pll_valid(p_earc->rx_top_map) &&
		    earxrx_get_pll_valid_auto(p_earc->rx_top_map)) {
			i++;
		} else {
			i = 0;
			count++;
		}
		if (i >= 5 || count >= 2)
			break;
		usleep_range(45, 50);
	}

	i = 0;
	while (earcrx_dmac_get_irqs(p_earc->rx_top_map) & 0x10000000) {
		earcrx_dmac_sync_clr_irqs(p_earc->rx_top_map);
		i++;
		if (i > 5)
			break;
		usleep_range(45, 50);
	}

	earcrx_dmac_sync_int_enable(p_earc->rx_top_map, 1);
	dev_info(p_earc->dev, "earcrx pll %d, auto %d, pending is 0x%x\n",
		earxrx_get_pll_valid(p_earc->rx_top_map),
		earxrx_get_pll_valid_auto(p_earc->rx_top_map),
		earcrx_dmac_get_irqs(p_earc->rx_top_map));
}

static void valid_auto_work_func(struct work_struct *p_work)
{
	struct earc *p_earc = container_of(p_work, struct earc, rx_dmac_int_work);

	earcrx_pll_reset(p_earc);
}

static irqreturn_t rx_handler(int irq, void *data)
{
	struct earc *p_earc = (struct earc *)data;
	unsigned int mask = earcrx_dmac_get_mask(p_earc->rx_top_map);

	p_earc->rx_status0 = earcrx_cdmc_get_irqs(p_earc->rx_top_map);
	if (p_earc->rx_status0)
		earcrx_cdmc_clr_irqs(p_earc->rx_top_map, p_earc->rx_status0);

	p_earc->rx_status1 = earcrx_dmac_get_irqs(p_earc->rx_top_map);
	p_earc->rx_status1 = p_earc->rx_status1 & mask;
	if (p_earc->rx_status1)
		earcrx_dmac_clr_irqs(p_earc->rx_top_map, p_earc->rx_status1);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t earc_rx_isr(int irq, void *data)
{
	struct earc *p_earc = (struct earc *)data;
	unsigned long flags;

	if (p_earc->rx_status0 & INT_EARCRX_CMDC_TIMEOUT) {
		earcrx_update_attend_event(p_earc,
					   false, false);

		dev_info(p_earc->dev, "EARCRX_CMDC_TIMEOUT\n");
	}

	if (p_earc->rx_status0 & INT_EARCRX_CMDC_IDLE2) {
		earcrx_update_attend_event(p_earc,
					   false, true);

		dev_info(p_earc->dev, "EARCRX_CMDC_IDLE2\n");
	}
	if (p_earc->rx_status0 & INT_EARCRX_CMDC_IDLE1) {
		earcrx_update_attend_event(p_earc,
					   false, false);

		dev_info(p_earc->dev, "EARCRX_CMDC_IDLE1\n");
	}
	if (p_earc->rx_status0 & INT_EARCRX_CMDC_DISC2)
		dev_info(p_earc->dev, "EARCRX_CMDC_DISC2\n");
	if (p_earc->rx_status0 & INT_EARCRX_CMDC_DISC1)
		dev_info(p_earc->dev, "EARCRX_CMDC_DISC1\n");
	if (p_earc->rx_status0 & INT_EARCRX_CMDC_EARC) {
		u8 latency = EARCRX_DEFAULT_LATENCY;

		earcrx_cmdc_set_latency(p_earc->rx_cmdc_map, &latency);
		earcrx_cmdc_set_cds(p_earc->rx_cmdc_map, p_earc->rx_cds_data);
		earcrx_update_attend_event(p_earc,
					   true, true);

		dev_info(p_earc->dev, "EARCRX_CMDC_EARC\n");
	}

	if (p_earc->rx_status0 & INT_EARCRX_CMDC_LOSTHB)
		dev_info(p_earc->dev, "EARCRX_CMDC_LOSTHB\n");

	if (p_earc->rx_status0 & INT_EARCRX_CMDC_STATUS_CH)
		dev_info(p_earc->dev, "EARCRX_CMDC_STATUS_CH\n");

	spin_lock_irqsave(&p_earc->rx_lock, flags);
	if (p_earc->rx_dmac_clk_on) {
		if (p_earc->rx_status1 & INT_EARCRX_ANA_RST_C_EARCRX_DIV2_HOLD_SET)
			dev_info(p_earc->dev, "EARCRX_ANA_RST_C_EARCRX_DIV2_HOLD_SET\n");

		if (p_earc->rx_status1 & INT_EARCRX_ANA_RST_C_NEW_FORMAT_SET) {
			dev_info(p_earc->dev, "EARCRX_ANA_RST_C_NEW_FORMAT_SET\n");

			earcrx_pll_refresh(p_earc->rx_top_map,
					   RST_BY_SELF, true);
		}

		if (p_earc->rx_status1 & INT_EARCRX_ERR_CORRECT_C_BCHERR_INT_SET)
			dev_info(p_earc->dev, "EARCRX_ERR_CORRECT_BCHERR\n");
		if (p_earc->rx_status1 & INT_ARCRX_BIPHASE_DECODE_R_PARITY_ERR) {
			dev_info(p_earc->dev, "ARCRX_R_PARITY_ERR reset\n");
			earcrx_pll_refresh(p_earc->rx_top_map, RST_BY_SELF, true);
		}

		if (p_earc->rx_status0 & INT_EARCRX_CMDC_HB_STATUS)
			dev_dbg(p_earc->dev, "EARCRX_CMDC_HB_STATUS\n");
		if (p_earc->rx_status1 & INT_ARCRX_BIPHASE_DECODE_C_CHST_MUTE_CLR) {
			mmio_update_bits(p_earc->rx_dmac_map, EARCRX_ERR_CORRECT_CTRL0,
				0x3,
				0x1); /* EARCRX_ERR_CORRECT_CTRL0 force mode enable */
			dev_info(p_earc->dev, "INT_ARCRX_BIPHASE_DECODE_C_CHST_MUTE_CLR\n");
		}

		if (p_earc->rx_status1 & INT_ARCRX_BIPHASE_DECODE_C_FIND_PAPB)
			dev_dbg(p_earc->dev, "ARCRX_C_FIND_PAPB\n");
		if (p_earc->rx_status1 & INT_ARCRX_BIPHASE_DECODE_C_VALID_CHANGE)
			dev_dbg(p_earc->dev, "ARCRX_C_VALID_CHANGE\n");
		if (p_earc->rx_status1 & INT_ARCRX_BIPHASE_DECODE_C_FIND_NONPCM2PCM)
			dev_dbg(p_earc->dev, "ARCRX_C_FIND_NONPCM2PCM\n");
		if (p_earc->rx_status1 & INT_ARCRX_BIPHASE_DECODE_C_PCPD_CHANGE)
			dev_dbg(p_earc->dev, "ARCRX_C_PCPD_CHANGE\n");
		if (p_earc->rx_status1 & INT_ARCRX_BIPHASE_DECODE_C_CH_STATUS_CHANGE) {
			int mute = earcrx_get_cs_mute(p_earc->rx_dmac_map);

			if (p_earc->rx_cs_mute != mute) {
				p_earc->rx_cs_mute = mute;
				if (!mute) {
					/* EARCRX_ERR_CORRECT_CTRL0 force mode disable */
					mmio_update_bits(p_earc->rx_dmac_map,
						EARCRX_ERR_CORRECT_CTRL0,
						0x3,
						0x0);
				}
				dev_info(p_earc->dev, "ARCRX_C_CH_STATUS_CHANGE\n");
			}
		}
		if (p_earc->rx_status1 & INT_ARCRX_BIPHASE_DECODE_I_SAMPLE_MODE_CHANGE)
			dev_dbg(p_earc->dev, "ARCRX_I_SAMPLE_MODE_CHANGE\n");
		if (p_earc->chipinfo->rx_dmac_sync_int &&
		    p_earc->rx_status1 & INT_EARCRX_DMAC_VALID_AUTO_NEG_INT_SET) {
			earcrx_dmac_sync_int_enable(p_earc->rx_top_map, 0);
			schedule_work(&p_earc->rx_dmac_int_work);
			dev_info(p_earc->dev, "%s EARCRX_DMAC_VALID_AUTO_NEG_INT_SET\n", __func__);
		} else if (p_earc->chipinfo->unstable_tick_sel &&
			   p_earc->rx_status1 & INT_EARCRX_DMAC_VALID_AUTO_NEG_INT_SET) {
			earcrx_dmac_sync_clr_irqs(p_earc->rx_top_map);
			dev_info(p_earc->dev, "%s unstable tick happen\n", __func__);
			earcrx_pll_refresh(p_earc->rx_top_map, RST_BY_SELF, true);
		}
	}
	spin_unlock_irqrestore(&p_earc->rx_lock, flags);

	return IRQ_HANDLED;
}

static void earctx_update_attend_event(struct earc *p_earc,
				       bool is_earc, bool state)
{
	unsigned long flags;

	if (state) {
		if (is_earc) {
			if (p_earc->tx_arc_status == ATNDTYP_EARC)
				return;
			p_earc->earctx_connected_device_type = ATNDTYP_EARC;
			dev_info(p_earc->dev, "send EARCTX_ARC_STATE=ATNDTYP_EARC\n");
			spin_lock_irqsave(&p_earc->tx_lock, flags);
			if (p_earc->tx_dmac_clk_on) {
				earctx_dmac_force_mode(p_earc->tx_dmac_map, false);
				earctx_compressed_enable(p_earc->tx_dmac_map,
					ATNDTYP_EARC, p_earc->tx_audio_coding_type, true);
			}
			spin_unlock_irqrestore(&p_earc->tx_lock, flags);
			audio_send_uevent(p_earc->dev, EARCTX_ATNDTYP_EVENT, ATNDTYP_EARC);
			p_earc->tx_arc_status = ATNDTYP_EARC;
		} else {
			/* if the connected device is earc, lost HB still is earc device */
			if (p_earc->earctx_connected_device_type != ATNDTYP_EARC)
				p_earc->earctx_connected_device_type = ATNDTYP_ARC;
			if (earctx_cmdc_get_attended_type(p_earc->tx_cmdc_map) == ATNDTYP_ARC) {
				dev_info(p_earc->dev, "send EARCTX_ARC_STATE=ATNDTYP_ARC\n");
				spin_lock_irqsave(&p_earc->tx_lock, flags);
				if (p_earc->tx_dmac_clk_on) {
					earctx_dmac_force_mode(p_earc->tx_dmac_map, false);
					earctx_compressed_enable(p_earc->tx_dmac_map,
						ATNDTYP_ARC, p_earc->tx_audio_coding_type, true);
				}
				spin_unlock_irqrestore(&p_earc->tx_lock, flags);
				audio_send_uevent(p_earc->dev, EARCTX_ATNDTYP_EVENT, ATNDTYP_ARC);
				p_earc->tx_arc_status = ATNDTYP_ARC;
			}
		}
	} else {
		/* send uevent when first is disconnect status and it is't reset hpd by earc */
		if (p_earc->tx_arc_status != ATNDTYP_DISCNCT && !p_earc->tx_reset_hpd) {
			dev_info(p_earc->dev, "send EARCTX_ARC_STATE=ATNDTYP_DISCNCT\n");
			audio_send_uevent(p_earc->dev, EARCTX_ATNDTYP_EVENT, ATNDTYP_DISCNCT);
			p_earc->tx_arc_status = ATNDTYP_DISCNCT;
		}
	}
}

static irqreturn_t earc_tx_isr(int irq, void *data)
{
	struct earc *p_earc = (struct earc *)data;
	unsigned int status0 = earctx_cdmc_get_irqs(p_earc->tx_top_map);
	struct snd_pcm_substream *substream =
		p_earc->substreams[SNDRV_PCM_STREAM_PLAYBACK];
	unsigned long flags;

	if (status0)
		earctx_cdmc_clr_irqs(p_earc->tx_top_map, status0);

	if (substream) {
		snd_pcm_stream_lock_irqsave(substream, flags);
		if (snd_pcm_running(substream) &&
		    (earctx_cmdc_get_attended_type(p_earc->tx_cmdc_map)
			== ATNDTYP_DISCNCT)) {
			snd_pcm_stop(substream, SNDRV_PCM_STATE_XRUN);
			dev_info(p_earc->dev, "snd_pcm_stop, eARC disconnect\n");
		}
		snd_pcm_stream_unlock_irqrestore(substream, flags);
	}

	if (status0 & INT_EARCTX_CMDC_EARC) {
		schedule_delayed_work(&p_earc->send_uevent, msecs_to_jiffies(200));
		p_earc->tx_reset_hpd = false;
		dev_info(p_earc->dev, "EARCTX_CMDC_EARC\n");
	}

	if (status0 & INT_EARCTX_CMDC_DISC1)
		dev_info(p_earc->dev, "EARCTX_CMDC_DISC1\n");
	if (status0 & INT_EARCTX_CMDC_DISC2)
		dev_info(p_earc->dev, "INT_EARCTX_CMDC_DISC2\n");
	if (status0 & INT_EARCTX_CMDC_STATUS_CH) {
		int state;

		mutex_lock(&earc_mutex);
		if (!p_earc->resumed)
			earc_clock_enable();
		state = earctx_cmdc_get_tx_stat_bits(p_earc->tx_cmdc_map);

		dev_info(p_earc->dev, "EARCTX_CMDC_STATUS_CH tx state: 0x%x, last state: 0x%x\n",
			state, p_earc->tx_heartbeat_state);
		/*
		 * bit 1: tx_stat_chng_conf
		 * bit 2: tx_cap_chng_conf
		 */
		if ((p_earc->tx_heartbeat_state & (0x1 << 2) && !(state & (0x1 << 2))) ||
		    (p_earc->tx_heartbeat_state & (0x1 << 1) && !(state & (0x1 << 1)))) {
			earctx_cmdc_get_latency(p_earc->tx_cmdc_map,
				&p_earc->tx_latency);
			earctx_cmdc_get_cds(p_earc->tx_cmdc_map,
				p_earc->tx_cds_data);
		}
		/* bit 5 is rx_cap_chng */
		if (p_earc->tx_heartbeat_state & (0x1 << 5))
			earctx_update_attend_event(p_earc, true, true);
		p_earc->tx_heartbeat_state = state;
		mutex_unlock(&earc_mutex);
	}
	if (status0 & INT_EARCTX_CMDC_HB_STATUS)
		dev_dbg(p_earc->dev, "EARCTX_CMDC_HB_STATUS\n");
	if (status0 & INT_EARCTX_CMDC_LOSTHB)
		dev_info(p_earc->dev, "EARCTX_CMDC_LOSTHB\n");
	if (status0 & INT_EARCTX_CMDC_TIMEOUT) {
		dev_info(p_earc->dev, "EARCTX_CMDC_TIMEOUT\n");
		mutex_lock(&earc_mutex);
		if (!p_earc->resumed)
			earc_clock_enable();
		earctx_cmdc_arc_connect(p_earc->tx_cmdc_map, true);
		mutex_unlock(&earc_mutex);
		p_earc->tx_reset_hpd = false;
	}
	if (status0 & INT_EARCTX_CMDC_RECV_NACK)
		dev_dbg(p_earc->dev, "EARCTX_CMDC_RECV_NACK\n");
	if (status0 & INT_EARCTX_CMDC_RECV_NORSP)
		dev_dbg(p_earc->dev, "EARCTX_CMDC_RECV_NORSP\n");
	if (status0 & INT_EARCTX_CMDC_RECV_UNEXP)
		dev_info(p_earc->dev, "EARCTX_CMDC_RECV_UNEXP\n");
	if (status0 & INT_EARCTX_CMDC_IDLE1) {
		dev_info(p_earc->dev, "EARCTX_CMDC_IDLE1\n");
		mutex_lock(&earc_mutex);
		if (!p_earc->resumed)
			earc_clock_enable();
		earctx_update_attend_event(p_earc, false, false);
		mutex_unlock(&earc_mutex);
	}
	if (status0 & INT_EARCTX_CMDC_IDLE2) {
		mutex_lock(&earc_mutex);
		if (!p_earc->resumed)
			earc_clock_enable();
		earctx_update_attend_event(p_earc, false, true);
		mutex_unlock(&earc_mutex);
		dev_info(p_earc->dev, "EARCTX_CMDC_IDLE2\n");
	}

	if (p_earc->tx_dmac_clk_on) {
		unsigned int status1 = earctx_dmac_get_irqs(p_earc->tx_top_map);

		if (status1)
			earctx_dmac_clr_irqs(p_earc->tx_top_map, status1);

		if (status1 & INT_EARCTX_FEM_C_HOLD_CLR)
			dev_info(p_earc->dev, "EARCTX_FEM_C_HOLD_CLR\n");
		if (status1 & INT_EARCTX_FEM_C_HOLD_START)
			dev_info(p_earc->dev, "EARCTX_FEM_C_HOLD_START\n");
		if (status1 & INT_EARCTX_ERRCORR_C_FIFO_THD_LESS_PASS)
			dev_dbg(p_earc->dev, "EARCTX_ECCFIFO_OVERFLOW\n");
		if (status1 & INT_EARCTX_ERRCORR_C_FIFO_OVERFLOW)
			dev_dbg(p_earc->dev, "EARCTX_ECC_FIFO_OVERFLOW\n");
		if (status1 & INT_EARCTX_ERRCORR_C_FIFO_EMPTY)
			dev_dbg(p_earc->dev, "EARCTX_ECC_FIFO_EMPTY\n");
	}

	return IRQ_HANDLED;
}

static int earc_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->cpu_dai->dev;
	struct earc *p_earc;
	int ret = 0;

	p_earc = (struct earc *)dev_get_drvdata(dev);

	snd_soc_set_runtime_hwparams(substream, &earc_hardware);
	snd_pcm_lib_preallocate_pages(substream, SNDRV_DMA_TYPE_DEV,
		dev, EARC_BUFFER_BYTES / 2, EARC_BUFFER_BYTES);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* select hdmirx arc source from earctx spdif */
		arc_earc_source_select(EARCTX_SPDIF_TO_HDMIRX);
		if (p_earc->same_src_on) {
			aml_check_and_release_sharebuffer(NULL, EARCTX_DMAC);
			earctx_enable(p_earc->tx_top_map,
				      p_earc->tx_cmdc_map,
				      p_earc->tx_dmac_map,
				      p_earc->tx_audio_coding_type,
				      false,
				      p_earc->chipinfo->rterm_on);
		}

		p_earc->fddr = aml_audio_register_frddr(dev,
			earc_ddr_isr, substream, false);
		if (!p_earc->fddr) {
			ret = -ENXIO;
			dev_err(dev, "failed to claim frddr\n");
			goto err_ddr;
		}
		p_earc->earctx_on = true;
		/* it is't same source case, so use the format by kcontrol setting */
		p_earc->tx_audio_coding_type = p_earc->ui_tx_audio_coding_type;
		p_earc->tx_stream_state = SNDRV_PCM_STATE_OPEN;
	} else {
		p_earc->tddr = aml_audio_register_toddr(dev,
			earc_ddr_isr, substream);
		if (!p_earc->tddr) {
			ret = -ENXIO;
			dev_err(dev, "failed to claim toddr\n");
			goto err_ddr;
		}
	}

	runtime->private_data = p_earc;

	return 0;
err_ddr:
	snd_pcm_lib_preallocate_free(substream);
	return ret;
}

static int earc_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct earc *p_earc = runtime->private_data;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		p_earc->earctx_on = false;
		aml_audio_unregister_frddr(p_earc->dev, substream);
	} else {
		aml_audio_unregister_toddr(p_earc->dev, substream);
	}

	runtime->private_data = NULL;
	snd_pcm_lib_preallocate_free(substream);

	return 0;
}

static int earc_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *hw_params)
{
	return snd_pcm_lib_malloc_pages(substream,
			params_buffer_bytes(hw_params));
}

static int earc_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_lib_free_pages(substream);

	return 0;
}

static int earc_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct earc *p_earc = runtime->private_data;
	unsigned int start_addr, end_addr, int_addr;
	unsigned int period, threshold;

	start_addr = runtime->dma_addr;
	end_addr = start_addr + runtime->dma_bytes - FIFO_BURST;
	period	 = frames_to_bytes(runtime, runtime->period_size);
	int_addr = period / FIFO_BURST;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct frddr *fr = p_earc->fddr;

		/*
		 * Contrast minimum of period and fifo depth,
		 * and set the value as half.
		 */
		threshold = min(period, fr->chipinfo->fifo_depth);
		threshold /= 2;
		/* Use all the fifo */
		aml_frddr_set_fifos(fr, fr->chipinfo->fifo_depth, threshold);

		aml_frddr_set_buf(fr, start_addr, end_addr);
		aml_frddr_set_intrpt(fr, int_addr);
	} else {
		struct toddr *to = p_earc->tddr;

		/*
		 * Contrast minimum of period and fifo depth,
		 * and set the value as half.
		 */
		threshold = min(period, to->chipinfo->fifo_depth);
		threshold /= 2;
		aml_toddr_set_fifos(to, threshold);

		aml_toddr_set_buf(to, start_addr, end_addr);
		aml_toddr_set_intrpt(to, int_addr);
	}

	return 0;
}

static snd_pcm_uframes_t earc_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct earc *p_earc = runtime->private_data;
	unsigned int addr, start_addr;
	snd_pcm_uframes_t frames;

	start_addr = runtime->dma_addr;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		addr = aml_frddr_get_position(p_earc->fddr);
	else
		addr = aml_toddr_get_position(p_earc->tddr);

	frames = bytes_to_frames(runtime, addr - start_addr);
	if (frames > runtime->buffer_size)
		frames = 0;

	return frames;
}

static int earc_mmap(struct snd_pcm_substream *substream,
			struct vm_area_struct *vma)
{
	return snd_pcm_lib_default_mmap(substream, vma);
}

static struct snd_pcm_ops earc_ops = {
	.open      = earc_open,
	.close     = earc_close,
	.ioctl     = snd_pcm_lib_ioctl,
	.hw_params = earc_hw_params,
	.hw_free   = earc_hw_free,
	.prepare   = earc_prepare,
	.pointer   = earc_pointer,
	.mmap      = earc_mmap,
};

static int earc_dai_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int bit_depth = snd_pcm_format_width(runtime->format);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct frddr *fr = p_earc->fddr;
		enum frddr_dest dst = EARCTX_DMAC;
		unsigned int fifo_id;
		enum attend_type type =
			earctx_cmdc_get_attended_type(p_earc->tx_cmdc_map);
		struct iec_cnsmr_cs cs_info;

		if (type == ATNDTYP_DISCNCT) {
			dev_err(p_earc->dev,
				"Neither eARC_TX or ARC_TX is attended!\n");
			return -ENOTCONN;
		}

		dev_info(p_earc->dev,
			"%s connected, Expected frddr dst:%s\n",
			(type == ATNDTYP_ARC) ? "ARC" : "eARC",
			frddr_src_get_str(dst));
		fifo_id = aml_frddr_get_fifo_id(fr);

		aml_frddr_set_format(fr,
				     runtime->channels,
				     runtime->rate,
				     bit_depth - 1,
				     spdifout_get_frddr_type(bit_depth));
		aml_frddr_select_dst(fr, dst);

		earctx_dmac_init(p_earc->tx_top_map,
				 p_earc->tx_dmac_map,
				 p_earc->chipinfo->earc_spdifout_lane_mask,
				 3,
				 0x10,
				 p_earc->tx_mute);
		earctx_dmac_set_format(p_earc->tx_dmac_map,
				       fr->fifo_id,
				       bit_depth - 1,
				       spdifout_get_frddr_type(bit_depth));

		iec_get_cnsmr_cs_info(&cs_info,
				      p_earc->tx_audio_coding_type,
				      runtime->channels,
				      runtime->rate);
		earctx_set_cs_info(p_earc->tx_dmac_map,
				   p_earc->tx_audio_coding_type,
				   &cs_info,
				   &p_earc->tx_cs_lpcm_ca);

		earctx_set_cs_mute(p_earc->tx_dmac_map, p_earc->tx_cs_mute);

		p_earc->tx_stream_state = SNDRV_PCM_STATE_PREPARED;
	} else {
		struct toddr *to = p_earc->tddr;
		unsigned int msb = 0, lsb = 0, toddr_type = 0;
		unsigned int src = EARCRX_DMAC;
		struct toddr_fmt fmt;
		enum attend_type type =
			earcrx_cmdc_get_attended_type(p_earc->rx_cmdc_map);

		if (type == ATNDTYP_DISCNCT) {
			dev_err(p_earc->dev,
				"Neither eARC_RX or ARC_RX is attended!\n");
			return -ENOTCONN;
		}

		dev_info(p_earc->dev,
			"%s connected, Expected toddr src:%s\n",
			(type == ATNDTYP_ARC) ? "ARC" : "eARC",
			toddr_src_get_str(src));

		if (bit_depth == 32)
			toddr_type = 3;
		else if (bit_depth == 24)
			toddr_type = 4;
		else
			toddr_type = 0;

		msb = 28 - 1;
		lsb = (bit_depth == 16) ? 28 - bit_depth : 4;
		if (get_resample_version() >= T5_RESAMPLE &&
		    get_resample_source(RESAMPLE_A) == EARCRX_DMAC) {
			msb = 32 - 1;
			lsb = 32 - bit_depth;
		}

		fmt.type      = toddr_type;
		fmt.msb       = msb;
		fmt.lsb       = lsb;
		fmt.endian    = 0;
		fmt.bit_depth = bit_depth;
		fmt.ch_num    = runtime->channels;
		fmt.rate      = runtime->rate;

		aml_toddr_select_src(to, src);
		aml_toddr_set_format(to, &fmt);

		earcrx_dmac_init(p_earc->rx_top_map,
				 p_earc->rx_dmac_map,
				 p_earc->chipinfo->unstable_tick_sel,
				 p_earc->chipinfo->chnum_mult_mode);
		earcrx_arc_init(p_earc->rx_dmac_map);
	}

	return 0;
}

/* normal 1 audio clk src both for 44.1k and 48k */
static void earctx_set_dmac_freq_normal_1(struct earc *p_earc, unsigned int freq,  bool tune)
{
	unsigned int mpll_freq = freq *
		mpll2dmac_clk_ratio_by_type(p_earc->tx_audio_coding_type);

	/* make sure mpll_freq doesn't exceed MPLL max freq */
	while (mpll_freq > AML_MPLL_FREQ_MAX)
		mpll_freq = mpll_freq >> 1;

	dev_info(p_earc->dev, "%s, set src freq:%d\n", __func__, mpll_freq);
	/* same with tdm mpll */
	clk_set_rate(p_earc->clk_tx_dmac_srcpll, mpll_freq);
}

/* 2 audio clk src each for 44.1k and 48k */
static void earctx_set_dmac_freq_normal_2(struct earc *p_earc, unsigned int freq,  bool tune)
{
	int ret = 0;
	int ratio = 0;

	if (p_earc->standard_tx_freq % 8000 == 0) {
		ret = clk_set_parent(p_earc->clk_tx_dmac, p_earc->clk_tx_dmac_srcpll);
		if (ret)
			dev_warn(p_earc->dev, "can't set clk_tx_dmac parent srcpll clock\n");
		ratio = MPLL_HBR_FIXED_FREQ / p_earc->standard_tx_dmac;
		clk_set_rate(p_earc->clk_tx_dmac_srcpll, freq * ratio);
	} else if (p_earc->standard_tx_freq % 11025 == 0) {
		ret = clk_set_parent(p_earc->clk_tx_dmac, p_earc->clk_src_cd);
		if (ret)
			dev_warn(p_earc->dev, "can't set clk_tx_dmac parent cd clock\n");
		ratio = MPLL_CD_FIXED_FREQ / p_earc->standard_tx_dmac;
		clk_set_rate(p_earc->clk_src_cd, freq * ratio);
	} else {
		dev_warn(p_earc->dev, "unsupport clock rate %d\n",
			p_earc->standard_tx_freq);
	}
	dev_info(p_earc->dev, "%s, set src freq:%d\n", __func__, freq * ratio);
}

static void earctx_set_dmac_freq_normal(struct earc *p_earc, unsigned int freq,  bool tune)
{
	if (freq == 0) {
		dev_err(p_earc->dev, "%s(), clk 0 err\n", __func__);
		return;
	}

	if (IS_ERR(p_earc->clk_src_cd))
		earctx_set_dmac_freq_normal_1(p_earc, freq, tune);
	else
		earctx_set_dmac_freq_normal_2(p_earc, freq, tune);

	p_earc->tx_dmac_freq = freq;
	if (!tune)
		clk_set_rate(p_earc->clk_tx_dmac, freq);

	if (!p_earc->tx_dmac_clk_on) {
		int ret;
		unsigned long flags;

		ret = clk_prepare_enable(p_earc->clk_tx_dmac);

		spin_lock_irqsave(&s_earc->tx_lock, flags);
		if (ret)
			dev_err(p_earc->dev, "Can't enable earc clk_tx_dmac: %d\n", ret);
		else
			p_earc->tx_dmac_clk_on = true;
		spin_unlock_irqrestore(&s_earc->tx_lock, flags);
	}

	dev_info(p_earc->dev,
		"%s, tune 0x%x p_earc->standard_tx_freq %d set freq:%d, get freq:%lu, get src freq:%lu, standard_tx_dmac %d\n",
		__func__, tune,
		p_earc->standard_tx_freq,
		freq,
		clk_get_rate(p_earc->clk_tx_dmac),
		clk_get_rate(p_earc->clk_tx_dmac_srcpll),
		p_earc->standard_tx_dmac);
}

static void earctx_set_dmac_freq(struct earc *p_earc, unsigned int freq,  bool tune)
{
	earctx_set_dmac_freq_normal(p_earc, freq, tune);
}

static void earctx_update_clk(struct earc *p_earc,
			      unsigned int channels,
			      unsigned int rate)
{
	unsigned int multi = audio_multi_clk(p_earc->tx_audio_coding_type);
	unsigned int freq = rate * 128 * EARC_DMAC_MUTIPLIER * multi;

	dev_info(p_earc->dev, "rate %d, set %dX normal dmac clk, p_earc->tx_dmac_freq:%d\n",
		rate, multi, p_earc->tx_dmac_freq);

	p_earc->standard_tx_dmac = freq;
	p_earc->standard_tx_freq = rate;
	earctx_set_dmac_freq(p_earc, freq, false);
}

int spdif_codec_to_earc_codec[][2] = {
	{AUD_CODEC_TYPE_STEREO_PCM, AUDIO_CODING_TYPE_STEREO_LPCM},
	{AUD_CODEC_TYPE_DTS_RAW_MODE, AUDIO_CODING_TYPE_DTS},
	{AUD_CODEC_TYPE_AC3, AUDIO_CODING_TYPE_AC3},
	{AUD_CODEC_TYPE_DTS, AUDIO_CODING_TYPE_DTS},
	{AUD_CODEC_TYPE_EAC3, AUDIO_CODING_TYPE_EAC3},
	{AUD_CODEC_TYPE_DTS_HD, AUDIO_CODING_TYPE_DTS_HD},
	{AUD_CODEC_TYPE_MULTI_LPCM, AUDIO_CODING_TYPE_MULTICH_2CH_LPCM},
	{AUD_CODEC_TYPE_TRUEHD, AUDIO_CODING_TYPE_MLP},
	{AUD_CODEC_TYPE_DTS_HD_MA, AUDIO_CODING_TYPE_DTS_HD_MA},
	{AUD_CODEC_TYPE_HSR_STEREO_PCM, AUDIO_CODING_TYPE_STEREO_LPCM},
	{AUD_CODEC_TYPE_AC3_LAYOUT_B, AUDIO_CODING_TYPE_AC3_LAYOUT_B},
	{AUD_CODEC_TYPE_OBA, AUDIO_CODING_TYPE_HBR_LPCM}

};

int aml_earctx_set_audio_coding_type(enum audio_coding_types new_coding_type)
{
	struct frddr *fr;
	enum attend_type type;
	struct iec_cnsmr_cs cs_info;
	int channels, rate;
	unsigned long flags;

	if (!s_earc || IS_ERR(s_earc->tx_cmdc_map))
		return 0;

	s_earc->tx_audio_coding_type = new_coding_type;

	fr = s_earc->fddr;
	/* Update dmac clk ? */
	if (s_earc->earctx_on) {
		channels = fr->channels;
		rate = fr->rate;
	} else {
		channels = s_earc->ss_info.channels;
		rate = s_earc->ss_info.rate;
	}
	if (channels > 0 && rate > 0)
		earctx_update_clk(s_earc, channels, rate);
	else
		return 0;

	spin_lock_irqsave(&s_earc->tx_lock, flags);
	if (!s_earc->tx_dmac_clk_on)
		goto exit;

	dev_info(s_earc->dev, "tx audio coding type: 0x%02x\n", new_coding_type);

	type = earctx_cmdc_get_attended_type(s_earc->tx_cmdc_map);

	/* Update ECC enable/disable */
	earctx_compressed_enable(s_earc->tx_dmac_map,
				 type, new_coding_type, true);

	/* Update Channel Status in runtime */
	iec_get_cnsmr_cs_info(&cs_info,
			      s_earc->tx_audio_coding_type,
			      channels,
			      rate);
	earctx_set_cs_info(s_earc->tx_dmac_map,
			   s_earc->tx_audio_coding_type,
			   &cs_info,
			   &s_earc->tx_cs_lpcm_ca);

exit:
	spin_unlock_irqrestore(&s_earc->tx_lock, flags);
	return 0;
}

int sharebuffer_earctx_prepare(struct snd_pcm_substream *substream,
	struct frddr *fr, enum aud_codec_types type, int lane_i2s)
{
	enum attend_type earc_type;
	struct snd_pcm_runtime *runtime = substream->runtime;
	int bit_depth = snd_pcm_format_width(runtime->format);
	int i;
	unsigned int chmask = 0, swap_masks = 0;

	if (!s_earc)
		return -ENOTCONN;

	earc_type = earctx_cmdc_get_attended_type(s_earc->tx_cmdc_map);
	if (earc_type == ATNDTYP_DISCNCT) {
		dev_dbg(s_earc->dev, "%s: Neither eARC_TX or ARC_TX is attended!\n", __func__);
		return -ENOTCONN;
	}

	if (IS_ERR(s_earc->clk_tx_dmac) || IS_ERR(s_earc->clk_tx_dmac_srcpll))
		return -ENOTCONN;

	/* same source channels always 2 */
	s_earc->ss_info.channels =  2;
	s_earc->ss_info.rate = runtime->rate;
	aml_earctx_set_audio_coding_type(spdif_codec_to_earc_codec[type][1]);
	for (i = 0; i < runtime->channels; i++)
		chmask |= (1 << i);
	swap_masks = (2 * lane_i2s) | (2 * lane_i2s + 1) << 4;
	earctx_dmac_init(s_earc->tx_top_map,
			 s_earc->tx_dmac_map,
			 s_earc->chipinfo->earc_spdifout_lane_mask,
			 chmask,
			 swap_masks,
			 s_earc->tx_mute);
	earctx_dmac_set_format(s_earc->tx_dmac_map,
			       fr->fifo_id,
			       bit_depth - 1,
			       spdifout_get_frddr_type(bit_depth));
	earctx_set_cs_mute(s_earc->tx_dmac_map, s_earc->tx_cs_mute);
	s_earc->same_src_on = 1;

	s_earc->tx_stream_state = SNDRV_PCM_STATE_PREPARED;

	return 0;
}

void aml_earctx_enable(bool enable)
{
	unsigned long flags;

	if (!s_earc)
		return;
	spin_lock_irqsave(&s_earc->tx_lock, flags);
	if (s_earc->tx_dmac_clk_on) {
		if (enable)
			schedule_work(&s_earc->tx_hold_bus_work);
		earctx_enable(s_earc->tx_top_map,
			s_earc->tx_cmdc_map,
			s_earc->tx_dmac_map,
			s_earc->tx_audio_coding_type,
			enable,
			s_earc->chipinfo->rterm_on);
		earctx_dmac_mute(s_earc->tx_dmac_map, s_earc->tx_mute);
		if (enable)
			s_earc->tx_stream_state = SNDRV_PCM_STATE_RUNNING;
		else
			s_earc->tx_stream_state = SNDRV_PCM_STATE_DISCONNECTED;
	}
	spin_unlock_irqrestore(&s_earc->tx_lock, flags);
}

static int earc_dai_trigger(struct snd_pcm_substream *substream, int cmd,
			       struct snd_soc_dai *cpu_dai)
{
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dev_info(substream->pcm->card->dev, "eARC/ARC TX enable\n");

			aml_frddr_enable(p_earc->fddr, true);
			earctx_enable(p_earc->tx_top_map,
				      p_earc->tx_cmdc_map,
				      p_earc->tx_dmac_map,
				      p_earc->tx_audio_coding_type,
				      true,
				      p_earc->chipinfo->rterm_on);
			earctx_dmac_mute(p_earc->tx_dmac_map, p_earc->tx_mute);
			schedule_work(&s_earc->tx_hold_bus_work);
			p_earc->tx_stream_state = SNDRV_PCM_STATE_RUNNING;
		} else {
			dev_info(substream->pcm->card->dev, "eARC/ARC RX enable\n");

			aml_toddr_enable(p_earc->tddr, true);
			earcrx_enable(p_earc->rx_cmdc_map,
				      p_earc->rx_dmac_map,
				      true);
			p_earc->rx_cs_ready = true;
			schedule_delayed_work(&p_earc->rx_stable_work, msecs_to_jiffies(100));
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dev_info(substream->pcm->card->dev, "eARC/ARC TX disable\n");
			if (!p_earc->tx_dmac_clk_on) {
				dev_info(substream->pcm->card->dev, "clk is disable\n");
			} else {
				earctx_enable(p_earc->tx_top_map,
					      p_earc->tx_cmdc_map,
					      p_earc->tx_dmac_map,
					      p_earc->tx_audio_coding_type,
					      false,
					      p_earc->chipinfo->rterm_on);
			}
			aml_frddr_enable(p_earc->fddr, false);
			p_earc->tx_stream_state = SNDRV_PCM_STATE_DISCONNECTED;
		} else {
			p_earc->rx_cs_ready = false;
			dev_info(substream->pcm->card->dev, "eARC/ARC RX disable\n");
			cancel_delayed_work(&p_earc->rx_stable_work);
			p_earc->stream_stable = false;
			earcrx_enable(p_earc->rx_cmdc_map,
				      p_earc->rx_dmac_map,
				      false);
			aml_toddr_enable(p_earc->tddr, false);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int earc_dai_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *cpu_dai)
{
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int channels = params_channels(params);
	unsigned int rate = params_rate(params);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		earctx_update_clk(p_earc, channels, rate);

	snd_soc_dai_set_sysclk(cpu_dai, substream->stream, rate, SND_SOC_CLOCK_OUT);

	return 0;
}

static int earc_dai_set_sysclk(struct snd_soc_dai *cpu_dai,
				int clk_id, unsigned int freq, int dir)
{
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);

	if (clk_id == 1) {
		/* RX */
		clk_set_rate(p_earc->clk_rx_dmac, 500000000);

		dev_info(p_earc->dev,
			"earc rx cmdc clk:%lu rx dmac clk:%lu\n",
			clk_get_rate(p_earc->clk_rx_cmdc),
			clk_get_rate(p_earc->clk_rx_dmac));
	}

	return 0;
}

static int earc_dai_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);
	int ret;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		unsigned long flags;

		if (IS_ERR(p_earc->clk_rx_dmac) || IS_ERR(p_earc->clk_rx_dmac_srcpll))
			return -ENOTCONN;
		/* rx dmac clk */
		ret = clk_prepare_enable(p_earc->clk_rx_dmac);
		if (ret) {
			dev_err(p_earc->dev, "Can't enable earc clk_rx_dmac: %d\n", ret);
			goto err;
		}
		spin_lock_irqsave(&p_earc->rx_lock, flags);
		p_earc->rx_dmac_clk_on = true;
		spin_unlock_irqrestore(&p_earc->rx_lock, flags);

		ret = clk_prepare_enable(p_earc->clk_rx_dmac_srcpll);
		if (ret) {
			dev_err(p_earc->dev, "Can't enable earc clk_rx_dmac_srcpll: %d\n", ret);
			goto err;
		}

		earcrx_pll_refresh(p_earc->rx_top_map, RST_BY_SELF, true);
	}

	p_earc->substreams[substream->stream] = substream;

	return 0;

err:
	dev_err(p_earc->dev, "failed enable clock\n");

	return -EINVAL;
}

static void earc_dai_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);
	char *clk_name = (char *)__clk_get_name(p_earc->clk_tx_dmac_srcpll);

	if ((aml_return_chip_id() == CLK_NOTIFY_CHIP_ID) &&
		(p_earc->standard_tx_freq % 11025 == 0)) {
		if (!strcmp(__clk_get_name(clk_get_parent(p_earc->clk_tx_dmac)),
				__clk_get_name(p_earc->clk_tx_dmac_srcpll))) {
			if (!strcmp(clk_name, "hifi_pll")) {
				clk_set_rate(p_earc->clk_tx_dmac_srcpll, MPLL_HBR_FIXED_FREQ);
				dev_info(p_earc->dev, "restore to MPLL_HBR_FIXED_FREQ\n");
			}
		}
	}

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (!IS_ERR(p_earc->clk_rx_dmac)) {
			unsigned long flags;

			spin_lock_irqsave(&p_earc->rx_lock, flags);
			p_earc->rx_dmac_clk_on = false;
			spin_unlock_irqrestore(&p_earc->rx_lock, flags);
			clk_disable_unprepare(p_earc->clk_rx_dmac);
		}
		if (!IS_ERR(p_earc->clk_rx_dmac_srcpll))
			clk_disable_unprepare(p_earc->clk_rx_dmac_srcpll);
	}

	p_earc->substreams[substream->stream] = NULL;
}

static struct snd_soc_dai_ops earc_dai_ops = {
	.prepare    = earc_dai_prepare,
	.trigger    = earc_dai_trigger,
	.hw_params  = earc_dai_hw_params,
	.set_sysclk = earc_dai_set_sysclk,
	.startup	= earc_dai_startup,
	.shutdown	= earc_dai_shutdown,
};

static struct snd_soc_pcm_stream pcm_stream = {
	.channels_min = 1,
	.channels_max = 32,
	.rates        = EARC_RATES,
	.formats      = EARC_FORMATS,
};

static struct snd_soc_dai_driver earc_dai[] = {
	{
		.name     = "EARC/ARC",
		.id       = 0,
		.ops    = &earc_dai_ops,
	},
};

static const char *const attended_type[] = {
	"DISCONNECT",
	"ARC",
	"eARC"
};

static int ss_prepare(struct snd_pcm_substream *substream,
			void *pfrddr,
			int samesource_sel,
			int lane_i2s,
			enum aud_codec_types type,
			int share_lvl,
			int separated)
{
	if (s_earc->earctx_on) {
		dev_dbg(s_earc->dev, "%s earctx priority\n", __func__);
		return 0;
	}
	dev_dbg(s_earc->dev, "%s() %d, lvl %d\n", __func__, __LINE__, share_lvl);

	sharebuffer_prepare(substream,
		pfrddr,
		samesource_sel,
		lane_i2s,
		type,
		share_lvl,
		separated);

	return 0;
}

static int ss_trigger(int cmd, int samesource_sel, bool reenable)
{
	if (s_earc->earctx_on) {
		dev_dbg(s_earc->dev, "%s earctx priority\n", __func__);
		return 0;
	}
	dev_dbg(s_earc->dev, "%s() ss %d\n", __func__, samesource_sel);
	sharebuffer_trigger(cmd, samesource_sel, reenable);

	return 0;
}

static int ss_free(struct snd_pcm_substream *substream,
	void *pfrddr, int samesource_sel, int share_lvl)
{
	if (s_earc->earctx_on) {
		dev_dbg(s_earc->dev, "%s earctx priority\n", __func__);
		return 0;
	}
	dev_info(s_earc->dev, "%s() samesrc %d, lvl %d\n",
		__func__, samesource_sel, share_lvl);
	s_earc->same_src_on = 0;
	if (aml_check_sharebuffer_valid(pfrddr, samesource_sel)) {
		unsigned long flags;

		/* first mute arc when release same source */
		spin_lock_irqsave(&s_earc->tx_lock, flags);
		if (s_earc->tx_dmac_clk_on)
			earctx_dmac_mute(s_earc->tx_dmac_map, true);
		spin_unlock_irqrestore(&s_earc->tx_lock, flags);
		sharebuffer_free(substream, pfrddr, samesource_sel, share_lvl);
	}
	return 0;
}

struct samesrc_ops earctx_ss_ops = {
	.prepare = ss_prepare,
	.trigger = ss_trigger,
	.hw_free = ss_free,
};

const struct soc_enum attended_type_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(attended_type),
			attended_type);

static int earcrx_get_attend_type(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum attend_type type;

	if (!p_earc || IS_ERR(p_earc->rx_cmdc_map))
		return 0;
	type = earcrx_cmdc_get_attended_type(p_earc->rx_cmdc_map);
	ucontrol->value.integer.value[0] = type;

	return 0;
}

static int earcrx_set_attend_type(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum cmdc_st state;

	if (!p_earc || IS_ERR(p_earc->rx_cmdc_map))
		return 0;
	state = earcrx_cmdc_get_state(p_earc->rx_cmdc_map);

	if (state != CMDC_ST_IDLE2)
		return 0;

	/* only support set cmdc from idle to ARC */

	return 0;
}

static int earcrx_arc_get_enable(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum attend_type type;

	if (!p_earc || IS_ERR(p_earc->rx_cmdc_map))
		return 0;
	type = earcrx_cmdc_get_attended_type(p_earc->rx_cmdc_map);
	ucontrol->value.integer.value[0] = (bool)(type == ATNDTYP_ARC);

	return 0;
}

static int earcrx_arc_set_enable(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);

	if (!p_earc || IS_ERR(p_earc->rx_cmdc_map))
		return 0;

	earcrx_cmdc_arc_connect(p_earc->rx_cmdc_map,
		(bool)ucontrol->value.integer.value[0]);

	return 0;
}

static int earctx_get_attend_type(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum attend_type type;

	if (!p_earc || IS_ERR(p_earc->tx_cmdc_map))
		return 0;

	type = earctx_cmdc_get_attended_type(p_earc->tx_cmdc_map);

	ucontrol->value.integer.value[0] = type;

	return 0;
}

static int earctx_set_attend_type(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum cmdc_st state;

	if (!p_earc || IS_ERR(p_earc->tx_cmdc_map))
		return 0;

	state = earctx_cmdc_get_state(p_earc->tx_cmdc_map);

	if (state != CMDC_ST_IDLE2)
		return 0;

	/* only support set cmdc from idle to ARC */

	return 0;
}

static int earcrx_get_latency(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum cmdc_st state;
	u8  val = 0;

	if (!p_earc || IS_ERR(p_earc->rx_cmdc_map))
		return 0;

	state = earcrx_cmdc_get_state(p_earc->rx_cmdc_map);
	if (state != CMDC_ST_EARC)
		return 0;

	earcrx_cmdc_get_latency(p_earc->rx_cmdc_map, &val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int earcrx_set_latency(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	u8 latency = ucontrol->value.integer.value[0];
	enum cmdc_st state;

	if (!p_earc || IS_ERR(p_earc->rx_cmdc_map))
		return 0;

	state = earcrx_cmdc_get_state(p_earc->rx_cmdc_map);
	if (state != CMDC_ST_EARC)
		return 0;

	earcrx_cmdc_set_latency(p_earc->rx_cmdc_map, &latency);

	return 0;
}

static int earcrx_get_cds(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	struct soc_bytes_ext *bytes_ext =
		(struct soc_bytes_ext *)kcontrol->private_value;
	u8 *value = (u8 *)ucontrol->value.bytes.data;
	int i;

	if (!p_earc || IS_ERR(p_earc->rx_top_map))
		return 0;

	for (i = 0; i < bytes_ext->max; i++)
		*value++ = p_earc->rx_cds_data[i];

	return 0;
}

static int earcrx_set_cds(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum cmdc_st state;

	if (!p_earc || IS_ERR(p_earc->rx_cmdc_map))
		return 0;

	memcpy(p_earc->rx_cds_data, ucontrol->value.bytes.data, CDS_MAX_BYTES);

	state = earcrx_cmdc_get_state(p_earc->rx_cmdc_map);
	if (state == CMDC_ST_EARC)
		earcrx_cmdc_set_cds(p_earc->rx_cmdc_map, p_earc->rx_cds_data);

	return 0;
}

int earcrx_get_mute(struct snd_kcontrol *kcontrol,
		    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	unsigned long flags;
	int mute = 0;

	if (!p_earc || IS_ERR(p_earc->rx_dmac_map))
		return 0;

	spin_lock_irqsave(&p_earc->rx_lock, flags);
	if (p_earc->rx_dmac_clk_on)
		mute = earcrx_get_cs_mute(p_earc->rx_dmac_map);
	spin_unlock_irqrestore(&p_earc->rx_lock, flags);

	ucontrol->value.integer.value[0] = mute;

	return 0;
}

int earcrx_get_audio_coding_type(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum audio_coding_types coding_type;
	enum attend_type type;
	unsigned long flags;

	if (!p_earc || IS_ERR(p_earc->rx_cmdc_map))
		return 0;

	if (!p_earc->rx_dmac_clk_on)
		return 0;

	type = earcrx_cmdc_get_attended_type(p_earc->rx_cmdc_map);
	spin_lock_irqsave(&p_earc->rx_lock, flags);
	if (p_earc->stream_stable)
		coding_type = earcrx_get_cs_fmt(p_earc->rx_dmac_map, type);
	else
		coding_type = AUDIO_CODING_TYPE_UNDEFINED;
	spin_unlock_irqrestore(&p_earc->rx_lock, flags);

	ucontrol->value.integer.value[0] = coding_type;

	return 0;
}

int earcrx_get_freq(struct snd_kcontrol *kcontrol,
		    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum audio_coding_types coding_type = 0;
	enum attend_type type;
	int freq = 0;
	unsigned long flags;

	if (!p_earc || IS_ERR(p_earc->rx_cmdc_map))
		return 0;

	if (!p_earc->rx_cs_ready)
		return 0;

	type = earcrx_cmdc_get_attended_type(p_earc->rx_cmdc_map);
	spin_lock_irqsave(&p_earc->rx_lock, flags);
	if (p_earc->rx_dmac_clk_on) {
		coding_type = earcrx_get_cs_fmt(p_earc->rx_dmac_map, type);
		freq = earcrx_get_cs_freq(p_earc->rx_dmac_map, coding_type);
	}
	spin_unlock_irqrestore(&p_earc->rx_lock, flags);

	ucontrol->value.integer.value[0] = freq;

	return 0;
}

static int earcrx_get_freq_by_pll(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	int val = 0x7;
	unsigned long flags;

	if (!p_earc || IS_ERR(p_earc->rx_cmdc_map))
		return 0;

	spin_lock_irqsave(&p_earc->rx_lock, flags);
	if (p_earc->rx_dmac_clk_on)
		val = earcrx_get_sample_rate(p_earc->rx_dmac_map);
	spin_unlock_irqrestore(&p_earc->rx_lock, flags);

	if (val == 0x7)
		val = 0;
	else
		val += 1;

	ucontrol->value.integer.value[0] = val;

	return 0;
}

int earcrx_get_word_length(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	unsigned long flags;
	int wlen = 0;

	if (!p_earc || IS_ERR(p_earc->rx_dmac_map))
		return 0;

	spin_lock_irqsave(&p_earc->rx_lock, flags);
	if (p_earc->rx_dmac_clk_on)
		wlen = earcrx_get_cs_word_length(p_earc->rx_dmac_map);
	spin_unlock_irqrestore(&p_earc->rx_lock, flags);

	ucontrol->value.integer.value[0] = wlen;

	return 0;
}

static int earctx_get_latency(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum cmdc_st state;
	u8 val = 0;

	if (!p_earc || IS_ERR(p_earc->tx_cmdc_map))
		return 0;

	state = earctx_cmdc_get_state(p_earc->tx_cmdc_map);
	if (state != CMDC_ST_EARC)
		return 0;

	earctx_cmdc_get_latency(p_earc->tx_cmdc_map, &val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int earctx_set_latency(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	u8 latency = ucontrol->value.integer.value[0];
	enum cmdc_st state;

	if (!p_earc || IS_ERR(p_earc->tx_cmdc_map))
		return 0;

	state = earctx_cmdc_get_state(p_earc->tx_cmdc_map);
	if (state != CMDC_ST_EARC)
		return 0;

	earctx_cmdc_set_latency(p_earc->tx_cmdc_map, &latency);

	return 0;
}

static int earctx_get_cds(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	struct soc_bytes_ext *bytes_ext =
		(struct soc_bytes_ext *)kcontrol->private_value;
	u8 *value = (u8 *)ucontrol->value.bytes.data;
	enum cmdc_st state;
	u8 data[256];
	int i;

	if (!p_earc || IS_ERR(p_earc->tx_cmdc_map))
		return 0;

	state = earctx_cmdc_get_state(p_earc->tx_cmdc_map);
	if (state != CMDC_ST_EARC)
		return 0;

	earctx_cmdc_get_cds(p_earc->tx_cmdc_map, data);

	for (i = 0; i < bytes_ext->max; i++)
		*value++ = data[i];

	return 0;
}

int earctx_get_audio_coding_type(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);

	if (!p_earc || IS_ERR(p_earc->tx_top_map))
		return 0;

	ucontrol->value.integer.value[0] = p_earc->tx_audio_coding_type;

	return 0;
}

int earctx_set_audio_coding_type(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum audio_coding_types new_coding_type = ucontrol->value.integer.value[0];

	if (!p_earc || IS_ERR(p_earc->tx_cmdc_map) ||
	    new_coding_type == p_earc->ui_tx_audio_coding_type)
		return 0;

	p_earc->ui_tx_audio_coding_type = new_coding_type;
	return aml_earctx_set_audio_coding_type(new_coding_type);
}

int earctx_get_mute(struct snd_kcontrol *kcontrol,
		    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);

	if (!p_earc || IS_ERR(p_earc->tx_top_map))
		return 0;

	ucontrol->value.integer.value[0] = p_earc->tx_cs_mute;

	return 0;
}

/*
 * eARC TX asserts the Channel Status Mute bit
 * to eARC RX before format change
 */
int earctx_set_mute(struct snd_kcontrol *kcontrol,
		    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	bool mute = ucontrol->value.integer.value[0];
	unsigned long flags;

	if (!p_earc || IS_ERR(p_earc->tx_dmac_map))
		return 0;

	p_earc->tx_cs_mute = mute;

	spin_lock_irqsave(&p_earc->tx_lock, flags);
	if (p_earc->tx_dmac_clk_on)
		earctx_set_cs_mute(p_earc->tx_dmac_map, mute);
	spin_unlock_irqrestore(&p_earc->tx_lock, flags);

	return 0;
}

static void earctx_set_earc_mode(struct earc *p_earc, bool earc_mode)
{
	unsigned long flags;

	if (!p_earc->earctx_5v) {
		dev_info(p_earc->dev, "cable is disconnect, no need set\n");
		return;
	}

	/* switch to force mode for consume data when switch earc mode */
	spin_lock_irqsave(&p_earc->tx_lock, flags);
	if (p_earc->tx_dmac_clk_on)
		earctx_dmac_force_mode(p_earc->tx_dmac_map, true);
	spin_unlock_irqrestore(&p_earc->tx_lock, flags);

#if (defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI ||\
	defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI_MODULE)
	if (!p_earc->tx_earc_mode) {
		rx_earc_hpd_cntl(); /* reset hpd */
		earctx_enable_d2a(p_earc->tx_top_map, false);
	}
#endif
	/* set arc initiated and arc_enable */
	earctx_cmdc_arc_connect(p_earc->tx_cmdc_map, !earc_mode);
	/* set earc mode */
	earctx_cmdc_earc_mode(p_earc->tx_cmdc_map, earc_mode);
#if (defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI ||\
	defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI_MODULE)
	if (p_earc->tx_earc_mode) {
		p_earc->tx_reset_hpd = true;
		earctx_enable_d2a(p_earc->tx_top_map, true);
		rx_earc_hpd_cntl(); /* reset hpd */
	}
#endif
}

static void earc_resume(void)
{
	struct earc *p_earc = s_earc;

	if (!p_earc)
		return;

	earc_clock_enable();

	if (!p_earc->chipinfo->tx_enable)
		return;

	dev_info(p_earc->dev, "%s arc_version %d, earc mode is %d\n",
		__func__, p_earc->chipinfo->arc_version, p_earc->tx_earc_mode);
	/* open bandgap, bit [1] = 0 */
	if (p_earc->chipinfo->arc_version == TM2_ARC)
		aml_hiubus_update_bits(HHI_HDMIRX_PHY_MISC2, 0x1 << 1, 0);
	else if (p_earc->chipinfo->arc_version == T7_ARC)
		hdmirx_arc_update_reg(HDMIRX_PHY_MISC2, 0x1 << 1, 0);

#if (defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI ||\
	defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI_MODULE)
	/* earc port, need reset for earc discovery when earc mode is on */
	if (!IS_ERR(p_earc->tx_cmdc_map) && p_earc->tx_earc_mode) {
		if (p_earc->earctx_5v) {
			earctx_set_earc_mode(p_earc, p_earc->tx_earc_mode);
		} else {
			/* cable is plugout, but 5v is on, that means
			 * the hdmirx callback is mistaked, so we neeed
			 * reset hpd, and then init earctx for earc disconnery.
			 */
			if (p_earc->earctx_port >= 0 &&
			    (rx_get_hdmi5v_sts() & (0x1 << p_earc->earctx_port))) {
				p_earc->earctx_5v = true;
				rx_earc_hpd_cntl(); /* reset hpd */
				/* wait hpd is high, time is 600ms */
				schedule_delayed_work(&p_earc->tx_resume_work,
					msecs_to_jiffies(600));
			}
		}
	}
#endif

	p_earc->resumed = true;
}

static void tx_resume_work_func(struct work_struct *p_work)
{
	struct earc *p_earc = container_of(p_work, struct earc, tx_resume_work.work);

	dev_info(p_earc->dev, "%s earctx_init", __func__);
	earctx_init(p_earc->earctx_port, true);
}

int earctx_earc_mode_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	int earc_mode = ucontrol->value.integer.value[0];

	if (!p_earc || IS_ERR(p_earc->tx_cmdc_map) || p_earc->tx_earc_mode == earc_mode)
		return 0;

	p_earc->tx_earc_mode = earc_mode;
	earctx_set_earc_mode(p_earc, earc_mode);
	if (!earc_mode && earctx_cmdc_get_attended_type(p_earc->tx_cmdc_map) == ATNDTYP_ARC)
		schedule_delayed_work(&p_earc->send_uevent, msecs_to_jiffies(700));

	return 0;
}

int earctx_earc_mode_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);

	if (!p_earc)
		return 0;

	ucontrol->value.integer.value[0] = p_earc->tx_earc_mode;

	return 0;
}

static int earcrx_get_iec958(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	unsigned long flags;
	int cs = 0, i;

	if (!p_earc || IS_ERR(p_earc->rx_dmac_map))
		return 0;

	spin_lock_irqsave(&p_earc->rx_lock, flags);
	if (p_earc->rx_dmac_clk_on) {
		for (i = 0; i < 6; i++) {
			cs = earcrx_get_cs_iec958(p_earc->rx_dmac_map, i);

			ucontrol->value.iec958.status[i * 4] = (cs >> 0) & 0xff;
			ucontrol->value.iec958.status[i * 4 + 1] = (cs >> 8) & 0xff;
			ucontrol->value.iec958.status[i * 4 + 2] = (cs >> 16) & 0xff;
			ucontrol->value.iec958.status[i * 4 + 3] = (cs >> 24) & 0xff;
		}
	}
	spin_unlock_irqrestore(&p_earc->rx_lock, flags);

	dev_info(p_earc->dev,
		"x get[AES0=%#x AES1=%#x AES2=%#x AES3=%#x]\n",
		ucontrol->value.iec958.status[0],
		ucontrol->value.iec958.status[1],
		ucontrol->value.iec958.status[2],
		ucontrol->value.iec958.status[3]
	);

	return 0;
}

static int earctx_get_iec958(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	int cs;
	unsigned long flags;

	if (!p_earc || IS_ERR(p_earc->tx_dmac_map))
		return 0;

	spin_lock_irqsave(&p_earc->tx_lock, flags);
	if (p_earc->tx_dmac_clk_on) {
		cs = earctx_get_cs_iec958(p_earc->tx_dmac_map);

		ucontrol->value.iec958.status[0] = (cs >> 0) & 0xff;
		ucontrol->value.iec958.status[1] = (cs >> 8) & 0xff;
		ucontrol->value.iec958.status[2] = (cs >> 16) & 0xff;
		ucontrol->value.iec958.status[3] = (cs >> 24) & 0xff;

		dev_info(p_earc->dev,
			"x get[AES0=%#x AES1=%#x AES2=%#x AES3=%#x]\n",
			ucontrol->value.iec958.status[0],
			ucontrol->value.iec958.status[1],
			ucontrol->value.iec958.status[2],
			ucontrol->value.iec958.status[3]
		);
	}
	spin_unlock_irqrestore(&p_earc->tx_lock, flags);
	return 0;
}

static int earctx_set_iec958(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	int cs = 0;
	unsigned long flags;

	if (!p_earc || IS_ERR(p_earc->tx_dmac_map))
		return 0;

	spin_lock_irqsave(&p_earc->tx_lock, flags);
	if (p_earc->tx_dmac_clk_on) {
		cs |= ucontrol->value.iec958.status[0];
		cs |= ucontrol->value.iec958.status[1] << 8;
		cs |= ucontrol->value.iec958.status[2] << 16;
		cs |= ucontrol->value.iec958.status[3] << 24;

		earctx_set_cs_iec958(p_earc->tx_dmac_map, cs);

		dev_info(p_earc->dev,
			"x put [AES0=%#x AES1=%#x AES2=%#x AES3=%#x]\n",
			ucontrol->value.iec958.status[0],
			ucontrol->value.iec958.status[1],
			ucontrol->value.iec958.status[2],
			ucontrol->value.iec958.status[3]
			);
	}
	spin_unlock_irqrestore(&p_earc->tx_lock, flags);
	return 0;
}

static const struct snd_pcm_chmap_elem snd_pcm_chmaps[] = {
	{ .channels = 2,
	  .map = { SNDRV_CHMAP_FL, SNDRV_CHMAP_FR }
	},
	{ .channels = 3,
	  .map = { SNDRV_CHMAP_FL, SNDRV_CHMAP_FR,
		   SNDRV_CHMAP_LFE }
	},
	{ .channels = 6,
	  .map = { SNDRV_CHMAP_FL, SNDRV_CHMAP_FR,
		   SNDRV_CHMAP_RL, SNDRV_CHMAP_RR,
		   SNDRV_CHMAP_FC, SNDRV_CHMAP_LFE }
	},
	{ .channels = 8,
	  .map = { SNDRV_CHMAP_FL, SNDRV_CHMAP_FR,
		   SNDRV_CHMAP_RL, SNDRV_CHMAP_RR,
		   SNDRV_CHMAP_FC, SNDRV_CHMAP_LFE,
		   SNDRV_CHMAP_RLC, SNDRV_CHMAP_RRC }
	}
};

static bool support_chmap(enum audio_coding_types coding_type)
{
	if (coding_type == AUDIO_CODING_TYPE_STEREO_LPCM ||
	    coding_type == AUDIO_CODING_TYPE_MULTICH_2CH_LPCM ||
	    coding_type == AUDIO_CODING_TYPE_MULTICH_8CH_LPCM)
		return true;

	return false;
}

static int earcrx_get_ca(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum audio_coding_types coding_type;
	unsigned long flags;

	if (!p_earc || IS_ERR(p_earc->rx_cmdc_map)) {
		ucontrol->value.integer.value[0] = 0xff;
		return 0;
	}

	if (!p_earc->rx_dmac_clk_on) {
		ucontrol->value.integer.value[0] = 0xff;
		return 0;
	}

	spin_lock_irqsave(&p_earc->rx_lock, flags);
	if (p_earc->rx_dmac_clk_on) {
		coding_type = earcrx_get_cs_fmt(p_earc->rx_dmac_map,
					earcrx_cmdc_get_attended_type(p_earc->rx_cmdc_map));

		ucontrol->value.integer.value[0] =
			earcrx_get_cs_ca(p_earc->rx_dmac_map);
	} else {
		ucontrol->value.integer.value[0] = 0xff;
	}
	spin_unlock_irqrestore(&p_earc->rx_lock, flags);

	return 0;
}

static int earctx_get_ca(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);

	if (!p_earc || IS_ERR(p_earc->tx_top_map)) {
		ucontrol->value.integer.value[0] = 0xff;
		return 0;
	}

	if (support_chmap(p_earc->tx_audio_coding_type))
		ucontrol->value.integer.value[0] = p_earc->tx_cs_lpcm_ca;
	else
		ucontrol->value.integer.value[0] = 0xff;

	return 0;
}

static int earctx_set_ca(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	int ca = ucontrol->value.integer.value[0];
	unsigned long flags;

	if (!p_earc || IS_ERR(p_earc->tx_top_map))
		return 0;

	if (ca >= 0x32)
		return -EINVAL;

	p_earc->tx_cs_lpcm_ca = ca;

	spin_lock_irqsave(&p_earc->tx_lock, flags);
	if (p_earc->tx_dmac_clk_on) {
		if (!support_chmap(p_earc->tx_audio_coding_type))
			ca = 0;

		/* runtime, update to channel status */
		earctx_set_cs_ca(p_earc->tx_dmac_map, ca);
	}
	spin_unlock_irqrestore(&p_earc->tx_lock, flags);
	return 0;
}

static int earctx_clk_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	if (!IS_ERR(p_earc->clk_tx_dmac)) {
		ucontrol->value.enumerated.item[0] =
				clk_get_rate(p_earc->clk_tx_dmac);
		p_earc->tx_dmac_freq = ucontrol->value.enumerated.item[0];
	}
	return 0;
}

static int earctx_clk_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	int freq = p_earc->tx_dmac_freq;
	int value = ucontrol->value.enumerated.item[0];

	if (value > 2000000 || value < 0) {
		pr_err("Fine earctx dmac setting range(0~2000000), %d\n",
				value);
		return 0;
	}
	value = value - 1000000;
	freq += value;

	earctx_set_dmac_freq(p_earc, freq, true);
	return 0;
}

static int earctx_clk_ppm_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	int freq = p_earc->standard_tx_dmac;
	int value = ucontrol->value.enumerated.item[0];

	if (value > 2000 || value < 0) {
		pr_err("Fine earctx dmac ppm tuning range(0~2000), %d\n", value);
		return -1;
	}
	freq = freq + freq * (value - 1000) / 1000000;

	earctx_set_dmac_freq(p_earc, freq, true);
	return 0;
}

static int arc_get_ui_flag(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);

	ucontrol->value.integer.value[0] = p_earc->tx_ui_flag;

	return 0;
}

static int arc_set_ui_flag(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);

	if (p_earc->tx_ui_flag == ucontrol->value.integer.value[0])
		return 0;
	p_earc->tx_ui_flag = ucontrol->value.integer.value[0];

	if (p_earc->earctx_5v) {
#if (defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI ||\
	defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI_MODULE)
		rx_earc_hpd_cntl(); /* reset hpd */
#endif
		/* wait hdp is high */
		if (p_earc->tx_ui_flag)
			usleep_range(1000 * 600, 1000 * 650);
		earctx_init(p_earc->earctx_port, p_earc->tx_ui_flag);
	}

	return 0;
}

static int arcrx_get_ui_flag(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);

	ucontrol->value.integer.value[0] = p_earc->rx_ui_flag;

	return 0;
}

static int arcrx_set_ui_flag(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);

	if (p_earc->rx_ui_flag == ucontrol->value.integer.value[0])
		return 0;
	p_earc->rx_ui_flag = ucontrol->value.integer.value[0];
	if (p_earc->earcrx_5v)
		earcrx_init(p_earc->rx_ui_flag);

	return 0;
}

static int arc_spdifout_mute_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	unsigned long flags;

	spin_lock_irqsave(&p_earc->tx_lock, flags);
	if (p_earc->tx_dmac_clk_on)
		ucontrol->value.integer.value[0] = earctx_get_dmac_mute(p_earc->tx_dmac_map);
	spin_unlock_irqrestore(&p_earc->tx_lock, flags);

	return 0;
}

static void gain_disable_work_func(struct work_struct *p_work)
{
	struct earc *p_earc = container_of(p_work, struct earc, gain_disable.work);
	unsigned long flags;

	dev_info(p_earc->dev, "%s\n", __func__);
	spin_lock_irqsave(&p_earc->tx_lock, flags);
	if (p_earc->tx_dmac_clk_on)
		aml_earc_auto_gain_enable(p_earc->tx_dmac_map, 0xff);
	spin_unlock_irqrestore(&p_earc->tx_lock, flags);
}

static int arc_spdifout_mute_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	unsigned long flags;

	dev_info(p_earc->dev, "%s mute %ld\n", __func__, ucontrol->value.integer.value[0]);
	/* mute by set gain 0 as mute maybe have pop sound for some AVR */
	spin_lock_irqsave(&p_earc->tx_lock, flags);
	if (p_earc->tx_dmac_clk_on) {
		aml_earc_auto_gain_enable(p_earc->tx_dmac_map,
			!ucontrol->value.integer.value[0]);
	}
	spin_unlock_irqrestore(&p_earc->tx_lock, flags);
	schedule_delayed_work(&p_earc->gain_disable, msecs_to_jiffies(2000));

	return 0;
}

static int arc_spdifout_reg_mute_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);

	ucontrol->value.integer.value[0] = p_earc->tx_mute;

	return 0;
}

static int arc_spdifout_reg_mute_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	int mute = ucontrol->value.integer.value[0];
	unsigned long flags;

	if (!p_earc)
		return 0;
	p_earc->tx_mute = mute;
	spin_lock_irqsave(&p_earc->tx_lock, flags);
	/* set unmute when stream is running */
	if (s_earc->tx_dmac_clk_on && s_earc->tx_stream_state == SNDRV_PCM_STATE_RUNNING)
		earctx_dmac_mute(p_earc->tx_dmac_map, mute);
	spin_unlock_irqrestore(&p_earc->tx_lock, flags);

	return 0;
}

static const struct snd_kcontrol_new earc_tx_controls[] = {
	SOC_ENUM_EXT("eARC_TX attended type",
		     attended_type_enum,
		     earctx_get_attend_type,
		     earctx_set_attend_type),

	SND_SOC_BYTES_EXT("eARC_TX Latency",
			  1,
			  earctx_get_latency,
			  earctx_set_latency),

	SND_SOC_BYTES_EXT("eARC_TX CDS",
			  CDS_MAX_BYTES,
			  earctx_get_cds,
			  NULL),

	SOC_ENUM_EXT("eARC_TX Audio Coding Type",
		     audio_coding_type_enum,
		     earctx_get_audio_coding_type,
		     earctx_set_audio_coding_type),

	SND_SOC_BYTES_EXT("eARC_TX Channel Allocation",
			  1,
			  earctx_get_ca,
			  earctx_set_ca),

	SOC_SINGLE_BOOL_EXT("eARC_TX CS Mute",
			    0,
			    earctx_get_mute,
			    earctx_set_mute),

	SOC_SINGLE_BOOL_EXT("eARC_TX eARC Mode",
			    0,
			    earctx_earc_mode_get,
			    earctx_earc_mode_put),

	SOC_SINGLE_EXT("eARC_TX CLK Fine Setting",
		       0, 0, 2000000, 0,
		       earctx_clk_get, earctx_clk_put),

	SOC_SINGLE_EXT("eARC_TX CLK Fine PPM Tuning",
		       0, 0, 2000, 0,
		       earctx_clk_get, earctx_clk_ppm_put),

	SOC_SINGLE_BOOL_EXT("ARC eARC TX enable",
			    0,
			    arc_get_ui_flag,
			    arc_set_ui_flag),

	SOC_SINGLE_BOOL_EXT("ARC eARC Spdifout Mute",
			    0,
			    arc_spdifout_mute_get,
			    arc_spdifout_mute_put),

	SOC_SINGLE_BOOL_EXT("ARC eARC Spdifout Reg Mute",
			    0,
			    arc_spdifout_reg_mute_get,
			    arc_spdifout_reg_mute_put),

	SND_IEC958(SNDRV_CTL_NAME_IEC958("", PLAYBACK, DEFAULT),
		   earctx_get_iec958,
		   earctx_set_iec958)
};

static const struct snd_kcontrol_new earc_rx_controls[] = {
	SOC_SINGLE_BOOL_EXT("eARC RX ARC Switch",
			    0,
			    earcrx_arc_get_enable,
			    earcrx_arc_set_enable),

	SOC_ENUM_EXT("eARC_RX attended type",
		     attended_type_enum,
		     earcrx_get_attend_type,
		     earcrx_set_attend_type),

	SND_SOC_BYTES_EXT("eARC_RX Latency",
			  1,
			  earcrx_get_latency,
			  earcrx_set_latency),

	SND_SOC_BYTES_EXT("eARC_RX CDS",
			  CDS_MAX_BYTES,
			  earcrx_get_cds,
			  earcrx_set_cds),

	SOC_ENUM_EXT("eARC_RX Audio Coding Type",
		     audio_coding_type_enum,
		     earcrx_get_audio_coding_type,
		     NULL),

	SND_SOC_BYTES_EXT("eARC_RX Channel Allocation",
			  1,
			  earcrx_get_ca,
			  NULL),

	SOC_SINGLE_BOOL_EXT("eARC_RX CS Mute",
			    0,
			    earcrx_get_mute,
			    NULL),

	SOC_SINGLE_EXT("eARC_RX Audio Sample Frequency",
		       0, 0, 384000, 0,
		       earcrx_get_freq,
		       NULL),

	SOC_ENUM_EXT("eARC_RX Audio Sample Frequency by pll",
				spdifin_sample_rate_enum,
				earcrx_get_freq_by_pll,
				NULL),

	SOC_SINGLE_EXT("eARC_RX Audio Word Length",
		       0, 0, 32, 0,
		       earcrx_get_word_length,
		       NULL),

	SOC_SINGLE_BOOL_EXT("ARC eARC RX enable",
			    0,
			    arcrx_get_ui_flag,
			    arcrx_set_ui_flag),

	/* Status channel controller */
	SND_IEC958(SNDRV_CTL_NAME_IEC958("", CAPTURE, DEFAULT),
		   earcrx_get_iec958,
		   NULL)
};

static int aml_component_earc_probe(struct snd_soc_component *component)
{
	int ret = 0;
	struct earc *p_earc = snd_soc_component_get_drvdata(component);

	if (p_earc->chipinfo->tx_enable) {
		ret = snd_soc_add_component_controls(component,
				earc_tx_controls,
				ARRAY_SIZE(earc_tx_controls));
		if (ret < 0)
			pr_err("%s, failed add snd earc tx controls\n",
				__func__);
	}

	if (p_earc->chipinfo->rx_enable) {
		ret = snd_soc_add_component_controls(component,
				earc_rx_controls,
				ARRAY_SIZE(earc_rx_controls));
		if (ret < 0)
			pr_err("%s, failed add snd earc rx controls\n",
				__func__);
	}

	return 0;
}

static const struct snd_soc_component_driver earc_component = {
	.ops   = &earc_ops,
	.name  = DRV_NAME,
	.probe = aml_component_earc_probe,
};

struct earc_chipinfo sm1_earc_chipinfo = {
	.earc_spdifout_lane_mask = EARC_SPDIFOUT_LANE_MASK_V1,
	.rx_dmac_sync_int = false,
	.rx_enable = true,
	.tx_enable = false,
};

struct earc_chipinfo tm2_earc_chipinfo = {
	.earc_spdifout_lane_mask = EARC_SPDIFOUT_LANE_MASK_V1,
	.rx_dmac_sync_int = false,
	.rx_enable = true,
	.tx_enable = true,
	.arc_version = TM2_ARC,
};

struct earc_chipinfo tm2_revb_earc_chipinfo = {
	.earc_spdifout_lane_mask = EARC_SPDIFOUT_LANE_MASK_V2,
	.rx_dmac_sync_int = true,
	.rx_enable = true,
	.tx_enable = true,
	.arc_version = TM2_ARC,
};

struct earc_chipinfo sc2_earc_chipinfo = {
	.earc_spdifout_lane_mask = EARC_SPDIFOUT_LANE_MASK_V2,
	.rx_dmac_sync_int = true,
	.rx_enable = true,
	.tx_enable = false,
};

struct earc_chipinfo t7_earc_chipinfo = {
	.earc_spdifout_lane_mask = EARC_SPDIFOUT_LANE_MASK_V2,
	.rx_dmac_sync_int = false,
	.rterm_on = true,
	.no_ana_auto_cal = true,
	.chnum_mult_mode = true,
	.unstable_tick_sel = true,
	.rx_enable = true,
	.tx_enable = true,
	.arc_version = T7_ARC,
};

struct earc_chipinfo t3_earc_chipinfo = {
	.earc_spdifout_lane_mask = EARC_SPDIFOUT_LANE_MASK_V2,
	.rx_dmac_sync_int = false,
	.rterm_on = true,
	.no_ana_auto_cal = true,
	.chnum_mult_mode = true,
	.unstable_tick_sel = true,
	.rx_enable = false,
	.tx_enable = true,
	.arc_version = T7_ARC,
};

struct earc_chipinfo t5m_earc_chipinfo = {
	.earc_spdifout_lane_mask = EARC_SPDIFOUT_LANE_MASK_V2,
	.rx_dmac_sync_int = false,
	.rterm_on = true,
	.no_ana_auto_cal = true,
	.chnum_mult_mode = true,
	.unstable_tick_sel = true,
	.rx_enable = false,
	.tx_enable = true,
	.arc_version = T5M_ARC,
};

struct earc_chipinfo s5_earc_chipinfo = {
	.earc_spdifout_lane_mask = EARC_SPDIFOUT_LANE_MASK_V2,
	.rx_dmac_sync_int = false,
	.rterm_on = true,
	.no_ana_auto_cal = true,
	.chnum_mult_mode = true,
	.unstable_tick_sel = true,
	.rx_enable = true,
	.tx_enable = false,
};

static const struct of_device_id earc_device_id[] = {
	{
		.compatible = "amlogic, sm1-snd-earc",
		.data = &sm1_earc_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-snd-earc",
		.data = &tm2_earc_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-revb-snd-earc",
		.data = &tm2_revb_earc_chipinfo,
	},
	{
		.compatible = "amlogic, sc2-snd-earc",
		.data = &sc2_earc_chipinfo,
	},
	{
		.compatible = "amlogic, t7-snd-earc",
		.data = &t7_earc_chipinfo,
	},
	{
		.compatible = "amlogic, t3-snd-earc",
		.data = &t3_earc_chipinfo,
	},
	{
		.compatible = "amlogic, t5m-snd-earc",
		.data = &t5m_earc_chipinfo,
	},
	{
		.compatible = "amlogic, s5-snd-earc",
		.data = &s5_earc_chipinfo,
	},
	{}
};

MODULE_DEVICE_TABLE(of, earc_device_id);

void earc_hdmitx_hpdst(bool st)
{
	struct earc *p_earc = s_earc;

	if (!p_earc)
		return;

	dev_info(p_earc->dev, "HDMITX cable is %s\n",
		 st ? "plugin" : "plugout");

	if (!p_earc->resumed)
		earc_clock_enable();

	p_earc->earcrx_5v = st;
	earcrx_init(st);
}

static int earcrx_cmdc_setup(struct earc *p_earc)
{
	int ret = 0;

	/* set cmdc clk */
	audiobus_update_bits(EE_AUDIO_CLK_GATE_EN1, 0x1 << 6, 0x1 << 6);
	if (!IS_ERR(p_earc->clk_rx_cmdc)) {
		clk_set_rate(p_earc->clk_rx_cmdc, 10000000);

		ret = clk_prepare_enable(p_earc->clk_rx_cmdc);
		if (ret) {
			dev_err(p_earc->dev, "Can't enable earc clk_rx_cmdc\n");
			return ret;
		}
	}

	ret = devm_request_threaded_irq(p_earc->dev,
					p_earc->irq_earc_rx,
					rx_handler,
					earc_rx_isr,
					IRQF_TRIGGER_HIGH |
					IRQF_ONESHOT,
					"earc_rx",
					p_earc);
	if (ret) {
		dev_err(p_earc->dev, "failed to claim earc_rx %u\n",
			p_earc->irq_earc_rx);
		return ret;
	}

	/* Default: arc arc_initiated */
	earcrx_cmdc_arc_connect(p_earc->rx_cmdc_map, true);

	return ret;
}

static void send_uevent_work_func(struct work_struct *p_work)
{
	struct earc *p_earc = container_of(p_work, struct earc, send_uevent.work);
	enum attend_type type = earctx_cmdc_get_attended_type(p_earc->tx_cmdc_map);

	mutex_lock(&earc_mutex);
	if (!p_earc->resumed)
		earc_clock_enable();
	if (type == ATNDTYP_ARC) {
		earctx_enable_d2a(p_earc->tx_top_map, true);
		earctx_update_attend_event(p_earc, false, true);
	} else if (type == ATNDTYP_DISCNCT) {
		earctx_update_attend_event(p_earc, false, false);
	} else {
		earctx_update_attend_event(p_earc, true, true);
	}
	mutex_unlock(&earc_mutex);
}

void earc_hdmirx_hpdst(int earc_port, bool st)
{
	struct earc *p_earc = s_earc;

	if (!p_earc)
		return;

	dev_info(p_earc->dev, "HDMIRX cable port:%d is %s\n",
		 earc_port,
		 st ? "plugin" : "plugout");
	p_earc->earctx_port = earc_port; /* get earc port id from hdmirx */
	p_earc->earctx_5v = st;
	earctx_init(earc_port, st);
}

static int earctx_cmdc_setup(struct earc *p_earc)
{
	int ret = 0;

	/* set cmdc clk */
	audiobus_update_bits(EE_AUDIO_CLK_GATE_EN1, 0x1 << 5, 0x1 << 5);
	if (!IS_ERR(p_earc->clk_tx_cmdc)) {
		clk_set_rate(p_earc->clk_tx_cmdc, 10000000);

		ret = clk_prepare_enable(p_earc->clk_tx_cmdc);
		if (ret) {
			dev_err(p_earc->dev, "Can't enable earc clk_tx_cmdc\n");
			return ret;
		}
	}

	ret = devm_request_threaded_irq(p_earc->dev,
					p_earc->irq_earc_tx,
					NULL,
					earc_tx_isr,
					IRQF_TRIGGER_HIGH |
					IRQF_ONESHOT,
					"earc_tx",
					p_earc);
	if (ret) {
		dev_err(p_earc->dev, "failed to claim earc_tx %u\n",
			p_earc->irq_earc_tx);
		return ret;
	}

	/* Default: no time out to connect RX */
	earctx_cmdc_set_timeout(p_earc->tx_cmdc_map, 1);
	/* Default: arc arc_initiated */
	earctx_cmdc_int_mask(p_earc->tx_top_map);

	return ret;
}

static void earc_work_func(struct work_struct *work)
{
	struct earc *p_earc = container_of(work, struct earc, work);

	/* RX */
	if (!IS_ERR(p_earc->rx_top_map) &&
	    !p_earc->chipinfo->no_ana_auto_cal &&
	    p_earc->event & EVENT_RX_ANA_AUTO_CAL) {
		p_earc->event &= ~EVENT_RX_ANA_AUTO_CAL;
		earcrx_ana_auto_cal(p_earc->rx_top_map);
	}

	/* TX */
	if (!IS_ERR(p_earc->tx_top_map) &&
	    !p_earc->chipinfo->no_ana_auto_cal &&
	    p_earc->event & EVENT_TX_ANA_AUTO_CAL) {
		p_earc->event &= ~EVENT_TX_ANA_AUTO_CAL;
		earctx_ana_auto_cal(p_earc->tx_top_map);
	}
}

static void rx_stable_work_func(struct work_struct *p_work)
{
	struct earc *p_earc = container_of(to_delayed_work(p_work),
			struct earc, rx_stable_work);

	p_earc->stream_stable = true;
}

static int earc_platform_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *node_prt = NULL;
	struct platform_device *pdev_parent;
	struct device *dev = &pdev->dev;
	struct aml_audio_controller *actrl = NULL;
	struct earc *p_earc = NULL;
	int ret = 0, i;
	int ss = 0;

	p_earc = devm_kzalloc(dev, sizeof(struct earc), GFP_KERNEL);
	if (!p_earc)
		return -ENOMEM;

	ret = of_property_read_u32(dev->of_node, "samesource_sel", &ss);
	if (ret < 0)
		p_earc->samesource_sel = SHAREBUFFER_NONE;
	else
		p_earc->samesource_sel = ss;

	p_earc->dev = dev;
	dev_set_drvdata(dev, p_earc);

	p_earc->chipinfo = (struct earc_chipinfo *)
		of_device_get_match_data(dev);

	if (!p_earc->chipinfo) {
		dev_warn_once(dev, "check whether to update earc chipinfo\n");
		return -EINVAL;
	}

	/* get audio controller */
	node_prt = of_get_parent(node);
	if (!node_prt)
		return -ENXIO;

	pdev_parent = of_find_device_by_node(node_prt);
	of_node_put(node_prt);
	actrl = (struct aml_audio_controller *)
			platform_get_drvdata(pdev_parent);
	p_earc->actrl = actrl;

	p_earc->tx_cmdc_map = regmap_resource(dev, "tx_cmdc");
	if (!p_earc->tx_cmdc_map)
		dev_info(dev, "Can't get earctx_cmdc regmap!!\n");

	p_earc->tx_dmac_map = regmap_resource(dev, "tx_dmac");
	if (!p_earc->tx_dmac_map)
		dev_info(dev, "Can't get earctx_dmac regmap!!\n");

	p_earc->tx_top_map = regmap_resource(dev, "tx_top");
	if (!p_earc->tx_top_map)
		dev_info(dev, "Can't get earctx_top regmap!!\n");

	p_earc->rx_cmdc_map = regmap_resource(dev, "rx_cmdc");
	if (!p_earc->rx_cmdc_map)
		dev_info(dev, "Can't get earcrx_cdmc regmap!!\n");

	p_earc->rx_dmac_map = regmap_resource(dev, "rx_dmac");
	if (!p_earc->rx_dmac_map)
		dev_info(dev, "Can't get earcrx_dmac regmap!!\n");

	p_earc->rx_top_map = regmap_resource(dev, "rx_top");
	if (!p_earc->rx_top_map)
		dev_info(dev, "Can't get earcrx_top regmap!!\n");

	ret = of_property_read_u32(node, "suspend-clk-off",
			&p_earc->suspend_clk_off);
	if (ret < 0)
		dev_err(&pdev->dev, "Can't retrieve suspend-clk-off\n");

	p_earc->clk_src_cd = devm_clk_get(&pdev->dev, "clk_src_cd");
	if (IS_ERR(p_earc->clk_src_cd))
		dev_info(&pdev->dev, "no clk_src_cd clock for 44k case\n");
	/* RX */
	if (p_earc->chipinfo->rx_enable) {
		spin_lock_init(&p_earc->rx_lock);
		p_earc->clk_rx_cmdc = devm_clk_get(&pdev->dev, "rx_cmdc");
		if (IS_ERR(p_earc->clk_rx_cmdc))
			dev_info(&pdev->dev, "Can't get clk_rx_cmdc\n");

		p_earc->clk_rx_dmac = devm_clk_get(&pdev->dev, "rx_dmac");
		if (IS_ERR(p_earc->clk_rx_dmac))
			dev_info(&pdev->dev, "Can't get clk_rx_dmac\n");

		p_earc->clk_rx_cmdc_srcpll = devm_clk_get(&pdev->dev, "rx_cmdc_srcpll");
		if (IS_ERR(p_earc->clk_rx_cmdc_srcpll))
			dev_info(&pdev->dev, "Can't get clk_rx_cmdc_srcpll\n");

		p_earc->clk_rx_dmac_srcpll = devm_clk_get(&pdev->dev, "rx_dmac_srcpll");
		if (IS_ERR(p_earc->clk_rx_dmac_srcpll))
			dev_info(&pdev->dev, "Can't get clk_rx_dmac_srcpll\n");

		if (!IS_ERR(p_earc->clk_rx_cmdc) && !IS_ERR(p_earc->clk_rx_cmdc_srcpll)) {
			ret = clk_set_parent(p_earc->clk_rx_cmdc, p_earc->clk_rx_cmdc_srcpll);
			if (ret) {
				dev_err(dev, "Can't set clk_rx_cmdc parent clock\n");
				return ret;
			}
		}
		if (!IS_ERR(p_earc->clk_rx_dmac) && !IS_ERR(p_earc->clk_rx_dmac_srcpll)) {
			ret = clk_set_parent(p_earc->clk_rx_dmac, p_earc->clk_rx_dmac_srcpll);
			if (ret) {
				dev_err(dev, "Can't set clk_rx_dmac parent clock\n");
				return ret;
			}
		}

		p_earc->irq_earc_rx =
			platform_get_irq_byname(pdev, "earc_rx");
		if (p_earc->irq_earc_rx < 0)
			dev_err(dev, "platform get irq earc_rx failed\n");
		else
			dev_info(dev, "%s, irq_earc_rx:%d\n", __func__, p_earc->irq_earc_rx);

		earc_dai[0].capture = pcm_stream;
	}

	/* TX */
	if (p_earc->chipinfo->tx_enable) {
		spin_lock_init(&p_earc->tx_lock);
		p_earc->clk_tx_cmdc = devm_clk_get(&pdev->dev, "tx_cmdc");
		if (IS_ERR(p_earc->clk_tx_cmdc))
			dev_info(&pdev->dev, "Check whether support eARC TX\n");

		p_earc->clk_tx_dmac = devm_clk_get(&pdev->dev, "tx_dmac");
		if (IS_ERR(p_earc->clk_tx_dmac))
			dev_info(&pdev->dev, "Check whether support eARC TX\n");

		p_earc->clk_tx_cmdc_srcpll = devm_clk_get(&pdev->dev, "tx_cmdc_srcpll");
		if (IS_ERR(p_earc->clk_tx_cmdc_srcpll))
			dev_info(&pdev->dev, "Check whether support eARC TX\n");

		p_earc->clk_tx_dmac_srcpll = devm_clk_get(&pdev->dev, "tx_dmac_srcpll");
		if (IS_ERR(p_earc->clk_tx_dmac_srcpll))
			dev_info(&pdev->dev, "Check whether support eARC TX\n");

		if (!IS_ERR(p_earc->clk_tx_cmdc) && !IS_ERR(p_earc->clk_tx_cmdc_srcpll)) {
			ret = clk_set_parent(p_earc->clk_tx_cmdc, p_earc->clk_tx_cmdc_srcpll);
			if (ret) {
				dev_err(dev, "Can't set clk_tx_cmdc parent clock\n");
				return ret;
			}
		}
		if (!IS_ERR(p_earc->clk_tx_dmac) && !IS_ERR(p_earc->clk_tx_dmac_srcpll)) {
			ret = clk_set_parent(p_earc->clk_tx_dmac, p_earc->clk_tx_dmac_srcpll);
			if (ret) {
				dev_err(dev, "Can't set clk_tx_dmac parent clock\n");
				return ret;
			}
		}

		p_earc->irq_earc_tx =
			platform_get_irq_byname(pdev, "earc_tx");
		if (p_earc->irq_earc_tx < 0)
			dev_err(dev, "platform get irq earc_tx failed\n");
		else
			dev_info(dev, "%s, irq_earc_tx:%d\n", __func__, p_earc->irq_earc_tx);
		earc_dai[0].playback = pcm_stream;
		p_earc->earctx_port = -1;
	}

	/* default is mute, need HDMI ARC Switch */
	p_earc->tx_mute = 1;
	p_earc->resumed = true;

	ret = snd_soc_register_component(&pdev->dev,
				&earc_component,
				earc_dai,
				ARRAY_SIZE(earc_dai));
	if (ret) {
		dev_err(&pdev->dev,
			"snd_soc_register_component failed\n");
		return ret;
	}
	p_earc->tx_earc_mode = true;
	p_earc->tx_ui_flag = 1;
	p_earc->rx_ui_flag = 1;
	s_earc = p_earc;

	/* RX */
	if (!IS_ERR(p_earc->rx_top_map))
		earcrx_cmdc_setup(p_earc);

	/* TX */
	if (!IS_ERR(p_earc->tx_top_map)) {
		earctx_cmdc_setup(p_earc);
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_HDMI
		register_earctx_callback(earc_hdmirx_hpdst);
#endif
		earctx_ss_ops.private = p_earc;
		register_samesrc_ops(SHAREBUFFER_EARCTX, &earctx_ss_ops);
		INIT_DELAYED_WORK(&p_earc->tx_resume_work, tx_resume_work_func);
		INIT_DELAYED_WORK(&p_earc->send_uevent, send_uevent_work_func);
		INIT_WORK(&p_earc->tx_hold_bus_work, tx_hold_bus_work_func);
		INIT_DELAYED_WORK(&p_earc->gain_disable, gain_disable_work_func);
	}

	if ((!IS_ERR(p_earc->rx_top_map)) ||
	    (!IS_ERR(p_earc->tx_top_map))) {
		INIT_WORK(&p_earc->work, earc_work_func);
		INIT_WORK(&p_earc->rx_dmac_int_work, valid_auto_work_func);
	}

	if (!IS_ERR(p_earc->rx_top_map)) {
		register_earcrx_callback(earc_hdmitx_hpdst);
		for (i = 0; i < sizeof(default_rx_cds) / sizeof(u8); i++)
			p_earc->rx_cds_data[i] = default_rx_cds[i];
		if (earcrx_cmdc_get_attended_type(p_earc->rx_cmdc_map) == ATNDTYP_EARC)
			earcrx_cmdc_set_cds(p_earc->rx_cmdc_map, p_earc->rx_cds_data);
		INIT_DELAYED_WORK(&p_earc->rx_stable_work, rx_stable_work_func);
	}
	dev_err(dev, "registered eARC platform\n");

	return 0;
}

int earc_platform_remove(struct platform_device *pdev)
{
	if (!IS_ERR(s_earc->rx_top_map))
		unregister_earcrx_callback();
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_HDMI
	if (!IS_ERR(s_earc->tx_top_map))
		unregister_earctx_callback();
#endif
	s_earc = NULL;
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

static int earc_platform_resume(struct platform_device *pdev)
{
	struct earc *p_earc = dev_get_drvdata(&pdev->dev);

	if (!p_earc->resumed)
		earc_resume();

	return 0;
}

static int earc_platform_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	struct earc *p_earc = dev_get_drvdata(&pdev->dev);

	mutex_lock(&earc_mutex);
	if (p_earc->suspend_clk_off) {
		if (p_earc->chipinfo->rx_enable) {
			if (!IS_ERR(p_earc->clk_rx_cmdc)) {
				while (__clk_is_enabled(p_earc->clk_rx_cmdc))
					clk_disable_unprepare(p_earc->clk_rx_cmdc);
			}

			if (!IS_ERR(p_earc->clk_rx_dmac)) {
				while (__clk_is_enabled(p_earc->clk_rx_dmac))
					clk_disable_unprepare(p_earc->clk_rx_dmac);
			}
		}

		if (p_earc->chipinfo->tx_enable) {
			if (!IS_ERR(p_earc->clk_tx_cmdc)) {
				while (__clk_is_enabled(p_earc->clk_tx_cmdc))
					clk_disable_unprepare(p_earc->clk_tx_cmdc);
			}

			if (!IS_ERR(p_earc->clk_tx_dmac)) {
				unsigned long flags;

				spin_lock_irqsave(&p_earc->tx_lock, flags);
				p_earc->tx_dmac_clk_on = false;
				spin_unlock_irqrestore(&p_earc->tx_lock, flags);
				while (__clk_is_enabled(p_earc->clk_tx_dmac))
					clk_disable_unprepare(p_earc->clk_tx_dmac);
			}
		}
	}

	/* shutdown the power */
	if (!IS_ERR(p_earc->tx_cmdc_map))
		earctx_enable_d2a(p_earc->tx_top_map, false);
	p_earc->resumed = false;
	mutex_unlock(&earc_mutex);

	return 0;
}

struct platform_driver earc_driver = {
	.driver = {
		.name           = DRV_NAME,
		.of_match_table = earc_device_id,
	},
	.probe = earc_platform_probe,
	.suspend = earc_platform_suspend,
	.remove = earc_platform_remove,
	.resume  = earc_platform_resume,
};

int __init earc_init(void)
{
	return platform_driver_register(&earc_driver);
}

void __exit earc_exit(void)
{
	platform_driver_unregister(&earc_driver);
}

#ifndef MODULE
arch_initcall_sync(earc_init);
module_exit(earc_exit);
MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic eARC/ARC TX/RX ASoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("Platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, earc_device_id);
#endif
