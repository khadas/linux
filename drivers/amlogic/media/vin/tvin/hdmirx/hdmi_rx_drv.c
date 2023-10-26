// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Standard Linux headers */
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/timer.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/poll.h>
#include <linux/io.h>
#include <linux/compat.h>
#include <linux/suspend.h>
/* #include <linux/earlysuspend.h> */
#include <linux/delay.h>
#include <linux/of_device.h>
#include <linux/dma-contiguous.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>
#include <linux/notifier.h>
#include <linux/sched/clock.h>

/* Amlogic headers */
/*#include <linux/amlogic/amports/vframe_provider.h>*/
/*#include <linux/amlogic/amports/vframe_receiver.h>*/
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include <linux/amlogic/media/vout/vdac_dev.h>
#include <linux/amlogic/media/vrr/vrr.h>
/*#include <linux/amlogic/amports/vframe.h>*/
#include <linux/amlogic/media/vout/hdmi_tx_ext.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/of_gpio.h>
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
#endif

/* Local include */
#include "hdmi_rx_repeater.h"
#include "hdmi_rx_drv.h"
#include "hdmi_rx_wrapper.h"
#include "hdmi_rx_hw.h"
#include "hdmi_rx_pktinfo.h"
#include "hdmi_rx_edid.h"
#include "hdmi_rx_eq.h"
#include "hdmi_rx_repeater.h"
/*------------------------extern function------------------------------*/
static int aml_hdcp22_pm_notify(struct notifier_block *nb,
				unsigned long event, void *dummy);
/*------------------------extern function end------------------------------*/

/*------------------------macro define------------------------------*/
#define TVHDMI_NAME			"hdmirx"
#define TVHDMI_DRIVER_NAME		"hdmirx"
#define TVHDMI_MODULE_NAME		"hdmirx"
#define TVHDMI_DEVICE_NAME		"hdmirx"
#define TVHDMI_CLASS_NAME		"hdmirx"
#define INIT_FLAG_NOT_LOAD		0x80

#define HDMI_DE_REPEAT_DONE_FLAG	0xF0
#define FORCE_YUV			1
#define FORCE_RGB			2
#define DEF_LOG_BUF_SIZE		0 /* (1024*128) */
#define PRINT_TEMP_BUF_SIZE		64 /* 128 */

/*------------------------macro define end------------------------------*/

/*------------------------variable define------------------------------*/
static unsigned char init_flag;
static dev_t	hdmirx_devno;
static struct class	*hdmirx_clsp;
/* static int open_flage; */
struct device *hdmirx_dev;
struct delayed_work     eq_dwork;
struct workqueue_struct *eq_wq;
struct delayed_work	esm_dwork;
struct workqueue_struct	*esm_wq;
struct delayed_work	repeater_dwork;
struct workqueue_struct	*repeater_wq;
struct work_struct     aml_phy_dwork;
struct workqueue_struct *aml_phy_wq;
struct work_struct     clkmsr_dwork;
struct workqueue_struct *clkmsr_wq;
struct work_struct     earc_hpd_dwork;
struct workqueue_struct *earc_hpd_wq;

unsigned int hdmirx_addr_port;
unsigned int hdmirx_data_port;
unsigned int hdmirx_ctrl_port;
/* attr */
static unsigned char *hdmirx_log_buf;
static unsigned int  hdmirx_log_wr_pos;
static unsigned int  hdmirx_log_rd_pos;
static unsigned int  hdmirx_log_buf_size;
unsigned int pwr_sts;
struct tvin_latency_s latency_info;
struct tasklet_struct rx_tasklet;
u32 *pd_fifo_buf;
static DEFINE_SPINLOCK(rx_pr_lock);
DECLARE_WAIT_QUEUE_HEAD(query_wait);

int hdmi_yuv444_enable = 1;
module_param(hdmi_yuv444_enable, int, 0664);
MODULE_PARM_DESC(hdmi_yuv444_enable, "hdmi_yuv444_enable");

bool downstream_repeat_support;
MODULE_PARM_DESC(downstream_repeat_support, "\n downstream_repeat_support\n");
module_param(downstream_repeat_support, bool, 0664);

int pc_mode_en;
MODULE_PARM_DESC(pc_mode_en, "\n pc_mode_en\n");
module_param(pc_mode_en, int, 0664);

int rx_audio_block_len = 13;
u8 rx_audio_block[MAX_AUDIO_BLK_LEN] = {
	0x2C, 0x09, 0x7F, 0x05,	0x15, 0x07, 0x50, 0x57,
	0x07, 0x01, 0x67, 0x7F, 0x01
};

u32 en_4096_2_3840;
int en_4k_2_2k;
u32 ops_port;
int en_4k_timing = 1;
int cec_dev_en;
bool dev_is_apple_tv_v2;
bool hdmi_cec_en;
static bool tv_auto_power_on;
int vdin_drop_frame_cnt = 1;
/* suspend_pddq_sel:
 * 0: keep phy on when suspend(don't need phy init when
 *   resume), it doesn't work now because phy VDD_IO_3.3V
 *   will power off when suspend, and tmds clk will be low;
 * 1&2: when CEC off there's no SDA low issue for MTK box,
 *   these workaround are not needed
 * 1: disable phy when suspend, set rxsense 1 and 0 when resume to
 *   release DDC from hdcp2.2 for MTK box, as LG 49UB8800-CE does
 * 2: disable phy when suspend, set rxsense 1 and 0 when suspend
 *   to release DDC from hdcp2.2 for MTK xiaomi box
 * other value: keep previous logic
 */
int suspend_pddq_sel = 1;
/* as cvt required, set hpd low if cec off when boot */
static int hpd_low_cec_off = 1;
int disable_port_num;
int disable_port_en;
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
static bool early_suspend_flag;
#endif

struct reg_map rx_reg_maps[MAP_ADDR_MODULE_NUM];

//RPT_RX's behavior when RPT_TX is disconnected:
//1.  HPD pulled down.
//2.  works on receiver mode
int rpt_only_mode;

/* disable hdr function in dts */
int disable_hdr;

/* audio block compose method for hdmitx/rx:
 * 1: for soundbar:
 * EDID = downstream TV's video + soundbar's audio capability.
 * 2: for RPT, use EDID of downstream directly.
 * both no audio blk mix.
 * 0: keep EDID of hdmirx itself
 */
int aud_compose_type = 1;
/*
 * vrr field VRRmin/max dynamic update enable
 */
int vrr_range_dynamic_update_en;

static struct notifier_block aml_hdcp22_pm_notifier = {
	.notifier_call = aml_hdcp22_pm_notify,
};

static struct meson_hdmirx_data rx_t3x_data = {
	.chip_id = CHIP_ID_T3X,
	.phy_ver = PHY_VER_T3X,
};

static struct meson_hdmirx_data rx_t5m_data = {
	.chip_id = CHIP_ID_T5M,
	.phy_ver = PHY_VER_T5M,
};

static struct meson_hdmirx_data rx_t5w_data = {
	.chip_id = CHIP_ID_T5W,
	.phy_ver = PHY_VER_T5W,
};

static struct meson_hdmirx_data rx_t3_data = {
	.chip_id = CHIP_ID_T3,
	.phy_ver = PHY_VER_T3,
};

static struct meson_hdmirx_data rx_t7_data = {
	.chip_id = CHIP_ID_T7,
	.phy_ver = PHY_VER_T7,
};

static struct meson_hdmirx_data rx_t5d_data = {
	.chip_id = CHIP_ID_T5D,
	.phy_ver = PHY_VER_T5,
};

static struct meson_hdmirx_data rx_t5_data = {
	.chip_id = CHIP_ID_T5,
	.phy_ver = PHY_VER_T5,
};

static struct meson_hdmirx_data rx_tm2_b_data = {
	.chip_id = CHIP_ID_TM2,
	.phy_ver = PHY_VER_TM2,
};

static struct meson_hdmirx_data rx_tm2_data = {
	.chip_id = CHIP_ID_TM2,
	.phy_ver = PHY_VER_TL1,
};

static struct meson_hdmirx_data rx_tl1_data = {
	.chip_id = CHIP_ID_TL1,
	.phy_ver = PHY_VER_TL1,
};

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static struct meson_hdmirx_data rx_txhd_data = {
	.chip_id = CHIP_ID_TXHD,
	.phy_ver = PHY_VER_ORG,
};

static struct meson_hdmirx_data rx_txlx_data = {
	.chip_id = CHIP_ID_TXLX,
	.phy_ver = PHY_VER_ORG,
};

static struct meson_hdmirx_data rx_txl_data = {
	.chip_id = CHIP_ID_TXL,
	.phy_ver = PHY_VER_ORG,
};

static struct meson_hdmirx_data rx_gxtvbb_data = {
	.chip_id = CHIP_ID_GXTVBB,
	.phy_ver = PHY_VER_ORG,
};
#endif

static const struct of_device_id hdmirx_dt_match[] = {
	{
		.compatible		= "amlogic, hdmirx_t3x",
		.data			= &rx_t3x_data
	},
	{
		.compatible		= "amlogic, hdmirx_t5m",
		.data			= &rx_t5m_data
	},
	{
		.compatible		= "amlogic, hdmirx_t5w",
		.data			= &rx_t5w_data
	},
	{
		.compatible		= "amlogic, hdmirx_t3",
		.data			= &rx_t3_data
	},
	{
		.compatible     = "amlogic, hdmirx_t7",
		.data           = &rx_t7_data
	},
	{
		.compatible     = "amlogic, hdmirx_t5d",
		.data           = &rx_t5d_data
	},
	{
		.compatible     = "amlogic, hdmirx_t5",
		.data           = &rx_t5_data
	},
	{
		.compatible     = "amlogic, hdmirx_tm2_b",
		.data           = &rx_tm2_b_data
	},
	{
		.compatible     = "amlogic, hdmirx_tm2",
		.data           = &rx_tm2_data
	},
	{
		.compatible     = "amlogic, hdmirx_tl1",
		.data           = &rx_tl1_data
	},
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible     = "amlogic, hdmirx_txhd",
		.data           = &rx_txhd_data
	},
	{
		.compatible     = "amlogic, hdmirx_txlx",
		.data           = &rx_txlx_data
	},
	{
		.compatible     = "amlogic, hdmirx-txl",
		.data           = &rx_txl_data
	},
	{
		.compatible     = "amlogic, hdmirx_gxtvbb",
		.data           = &rx_gxtvbb_data
	},
#endif
	{},
};

/*
 * rx_init_reg_map - physical addr map
 *
 * map physical address of I/O memory resources
 * into the core virtual address space
 */
int rx_init_reg_map(struct platform_device *pdev)
{
	int i;
	struct resource *res = 0;
	int size = 0;
	int ret = 0;

	for (i = 0; i < MAP_ADDR_MODULE_NUM; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			rx_pr("missing memory resource\n");
			ret = -ENOMEM;
			break;
		}
		size = resource_size(res);
		rx_reg_maps[i].phy_addr = res->start;
		rx_reg_maps[i].p = devm_ioremap_nocache(&pdev->dev,
						     res->start, size);
		rx_reg_maps[i].size = size;
		rx_pr("phy_addr = 0x%x, size = 0x%x, maped:%p\n",
		      rx_reg_maps[i].phy_addr, size, rx_reg_maps[i].p);
	}
	return ret;
}

/*
 * first_bit_set - get position of the first set bit
 */
static unsigned int first_bit_set(u32 data)
{
	unsigned int n = 32;

	if (data != 0) {
		for (n = 0; (data & 1) == 0; n++)
			data >>= 1;
	}
	return n;
}

/*
 * get - get masked bits of data
 */
unsigned int rx_get_bits(unsigned int data, unsigned int mask)
{
	unsigned int fst_bs_rtn;
	unsigned int rtn_val;

	fst_bs_rtn = first_bit_set(mask);
	if (fst_bs_rtn < 32)
		rtn_val = (data & mask) >> fst_bs_rtn;
	else
		rtn_val = 0;
	return rtn_val;
}

unsigned int rx_set_bits(unsigned int data,
			 unsigned int mask,
			 unsigned int value)
{
	unsigned int fst_bs_rtn;
	unsigned int rtn_val;

	fst_bs_rtn = first_bit_set(mask);
	if (fst_bs_rtn < 32)
		rtn_val = ((value << fst_bs_rtn) & mask) | (data & ~mask);
	else
		rtn_val = 0;
	return rtn_val;
}

bool hdmirx_repeat_support(void)
{
	return downstream_repeat_support;
}

/*
 * hdmirx_dec_support - check if given port is supported
 * @fe: frontend device of tvin interface
 * @port: port of specified frontend
 *
 * return 0 if specified port of frontend is supported,
 * otherwise return a negative value.
 */
int hdmirx_dec_support(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	if (port >= TVIN_PORT_HDMI0 && port <= TVIN_PORT_HDMI7) {
		rx_pr("hdmirx support\n");
		return 0;
	} else {
		return -1;
	}
}

/*
 * hdmirx_dec_open - open frontend
 * @fe: frontend device of tvin interface
 * @port: port index of specified frontend
 */
int hdmirx_dec_open(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	struct hdmirx_dev_s *devp;

	devp = container_of(fe, struct hdmirx_dev_s, frontend);
	devp->param.port = port;

	/* should enable the adc ref signal for audio pll */
	/* vdac_enable(1, VDAC_MODULE_AUDIO_OUT); */
	hdmirx_open_port(port);
	rx.open_fg = 1;
	rx_pr("%s port:%x ok nosignal:%d\n", __func__, port, rx.no_signal);
	return 0;
}

/*
 * hdmirx_dec_start - after signal stable, vdin
 * start process video in detected format
 * @fe: frontend device of tvin interface
 * @fmt: format in which vdin process
 */
void hdmirx_dec_start(struct tvin_frontend_s *fe, enum tvin_sig_fmt_e fmt)
{
	struct hdmirx_dev_s *devp;
	struct tvin_parm_s *parm;

	devp = container_of(fe, struct hdmirx_dev_s, frontend);
	parm = &devp->param;
	parm->info.fmt = fmt;
	parm->info.status = TVIN_SIG_STATUS_STABLE;
	rx_pr("%s fmt:%d ok\n", __func__, fmt);
}

/*
 * hdmirx_dec_stop - after signal unstable, vdin
 * stop decoder
 * @fe: frontend device of tvin interface
 * @port: port index of specified frontend
 */
void hdmirx_dec_stop(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	struct hdmirx_dev_s *devp;
	struct tvin_parm_s *parm;

	devp = container_of(fe, struct hdmirx_dev_s, frontend);
	parm = &devp->param;
	if (vpp_mute_enable) {
		if (get_video_mute() && rx.vpp_mute)
			set_video_mute(HDMI_RX_MUTE_SET, false);
		rx.vpp_mute = false;
	}
	/* parm->info.fmt = TVIN_SIG_FMT_NULL; */
	/* parm->info.status = TVIN_SIG_STATUS_NULL; */
	rx_pr("%s ok\n", __func__);
}

/*
 * hdmirx_dec_open - close frontend
 * @fe: frontend device of tvin interface
 */
void hdmirx_dec_close(struct tvin_frontend_s *fe)
{
	struct hdmirx_dev_s *devp;
	struct tvin_parm_s *parm;

	/*
	 * txl:should disable the adc ref signal for audio pll
	 * txlx:don't disable the adc ref signal for audio pll(not
	 *	reset the vdac) to avoid noise issue
	 */
	/* For txl,also need to keep bandgap always on:SWPL-1224 */
	/* if (rx.chip_id == CHIP_ID_TXL) */
		/* vdac_enable(0, VDAC_MODULE_AUDIO_OUT); */
	/* open_flage = 0; */
	rx.open_fg = 0;
	devp = container_of(fe, struct hdmirx_dev_s, frontend);
	parm = &devp->param;
	rx.vs_info_details.hdmi_allm = 0;
	rx.cur.cn_type = 0;
	rx.cur.it_content = 0;
	rx.vsif_fmm_flag = false;
	latency_info.allm_mode = 0;
	latency_info.it_content = 0;
	latency_info.cn_type = 0;
	hdmitx_update_latency_info(&latency_info);
	/*del_timer_sync(&devp->timer);*/
	hdmirx_close_port();
	parm->info.fmt = TVIN_SIG_FMT_NULL;
	parm->info.status = TVIN_SIG_STATUS_NULL;
	/* clear vpp mute, such as after unplug */
	if (vpp_mute_enable) {
		if (get_video_mute() && rx.vpp_mute)
			set_video_mute(HDMI_RX_MUTE_SET, false);
		rx.vpp_mute = false;
	}
	rx_pr("%s ok\n", __func__);
}

/*
 * hdmirx_dec_isr - interrupt handler
 * @fe: frontend device of tvin interface
 */
int hdmirx_dec_isr(struct tvin_frontend_s *fe, unsigned int hcnt64)
{
	struct hdmirx_dev_s *devp;
	struct tvin_parm_s *parm;
	u32 avmute_flag;

	devp = container_of(fe, struct hdmirx_dev_s, frontend);
	parm = &devp->param;

	if (!rx.var.force_pattern) {
		/*prevent spurious pops or noise when pw down*/
		if (rx.state == FSM_SIG_READY) {
			avmute_flag = rx_get_avmute_sts();
			if (avmute_flag == 1) {
				rx.avmute_skip += 1;
				hdmirx_set_video_mute(1);
				//skip_frame(2);
				/* return TVIN_BUF_SKIP; */
			} else {
				hdmirx_set_video_mute(0);
				rx.avmute_skip = 0;
			}
		}
	}
	/* if there is any error or overflow, do some reset, then return -1;*/
	if (parm->info.status != TVIN_SIG_STATUS_STABLE ||
	    parm->info.fmt == TVIN_SIG_FMT_NULL)
		return -1;
	else if (rx.skip > 0)
		return TVIN_BUF_SKIP;
	return 0;
}

/*
 * hdmi_dec_callmaster
 * @port: port index of specified frontend
 * @fe: frontend device of tvin interface
 *
 * return power_5V status of specified port
 */
static int hdmi_dec_callmaster(enum tvin_port_e port,
			       struct tvin_frontend_s *fe)
{
	int status = rx_get_hdmi5v_sts();

	switch (port) {
	case TVIN_PORT_HDMI0:
		status = (status >> 0) & 0x1;
		break;
	case TVIN_PORT_HDMI1:
		status = (status >> 1) & 0x1;
		break;
	case TVIN_PORT_HDMI2:
		status = (status >> 2) & 0x1;
		break;
	case TVIN_PORT_HDMI3:
	    status = (status >> 3) & 0x1;
		break;
	default:
		status = 1;
		break;
	}
	return status;
}

static struct tvin_decoder_ops_s hdmirx_dec_ops = {
	.support    = hdmirx_dec_support,
	.open       = hdmirx_dec_open,
	.start      = hdmirx_dec_start,
	.stop       = hdmirx_dec_stop,
	.close      = hdmirx_dec_close,
	.decode_isr = hdmirx_dec_isr,
	.callmaster_det = hdmi_dec_callmaster,
};

/*
 * hdmirx_is_nosig
 * @fe: frontend device of tvin interface
 *
 * return true if no signal is detected,
 * otherwise return false.
 */
bool hdmirx_is_nosig(struct tvin_frontend_s *fe)
{
	bool ret = 0;

	ret = rx_is_nosig();
	return ret;
}

/***************************************************
 *func: hdmirx_fmt_chg
 *	@fe: frontend device of tvin interface
 *
 *	return true if video format changed, otherwise
 *	return false.
 ***************************************************/
bool hdmirx_fmt_chg(struct tvin_frontend_s *fe)
{
	bool ret = false;
	enum tvin_sig_fmt_e fmt = TVIN_SIG_FMT_NULL;
	struct hdmirx_dev_s *devp;
	struct tvin_parm_s *parm;

	devp = container_of(fe, struct hdmirx_dev_s, frontend);
	parm = &devp->param;
	if (rx_is_sig_ready() == false) {
		ret = true;
	} else {
		fmt = hdmirx_hw_get_fmt();
		if (fmt != parm->info.fmt) {
			rx_pr("hdmirx fmt: %d --> %d\n",
			      parm->info.fmt, fmt);
			parm->info.fmt = fmt;
			ret = true;
		} else {
			ret = false;
		}
	}
	return ret;
}

/*
 * hdmirx_fmt_chg - get current video format
 * @fe: frontend device of tvin interface
 */
enum tvin_sig_fmt_e hdmirx_get_fmt(struct tvin_frontend_s *fe)
{
	enum tvin_sig_fmt_e fmt = TVIN_SIG_FMT_NULL;

	fmt = hdmirx_hw_get_fmt();
	return fmt;
}

bool hdmirx_hw_check_frame_skip(void)
{
	if (rx.state != FSM_SIG_READY || rx.skip > 0)
		return true;

	return false;
}

/*
 * hdmirx_get_dvi_info - get dvi state
 */
void hdmirx_get_dvi_info(struct tvin_sig_property_s *prop)
{
	prop->dvi_info = rx.pre.sw_dvi;
}

/*
 * hdmirx_get_hdcp_sts - get hdcp status 0:none hdcp  1:hdcp14 or 22
 */
void hdmirx_get_hdcp_sts(struct tvin_sig_property_s *prop)
{
	if (rx.hdcp.hdcp_version != HDCP_VER_NONE)
		prop->hdcp_sts = 1;
	else
		prop->hdcp_sts = 0;

}

void hdmirx_get_hw_vic(struct tvin_sig_property_s *prop)
{
	prop->hw_vic = rx.pre.hw_vic;
}

/*
 * hdmirx_get_fps_info - get video frame rate info
 */
void hdmirx_get_fps_info(struct tvin_sig_property_s *prop)
{
	u32 rate = rx.cur.frame_rate;

	rate = rate / 100 + (((rate % 100) / 10 >= 5) ? 1 : 0);
	prop->fps = rate;
}

enum tvin_aspect_ratio_e get_format_ratio(void)
{
	enum tvin_aspect_ratio_e ratio = TVIN_ASPECT_NULL;
	enum hdmi_vic_e vic = HDMI_UNKNOWN;

	vic = rx.pre.sw_vic;
	if (force_vic)
		vic = force_vic;

	switch (vic) {
	case HDMI_800_600:
	case HDMI_1024_768:
	case HDMI_1280_960:
	case HDMI_1280_1024:
	case HDMI_1600_1200:
	case HDMI_1792_1344:
	case HDMI_1856_1392:
	case HDMI_1920_1440:
	case HDMI_1400_1050:
	case HDMI_1152_864:
		ratio = TVIN_ASPECT_4x3_FULL;
		break;
	default:
		ratio = TVIN_ASPECT_16x9_FULL;
		break;
	}

	return ratio;
}

void hdmirx_get_active_aspect_ratio(struct tvin_sig_property_s *prop)
{
	prop->aspect_ratio = TVIN_ASPECT_NULL;
	if (rx.cur.active_valid) {
		if (rx.cur.active_ratio == 9) {
			prop->aspect_ratio = TVIN_ASPECT_4x3_FULL;
		} else if (rx.cur.active_ratio == 10) {
			prop->aspect_ratio = TVIN_ASPECT_16x9_FULL;
		} else if (rx.cur.active_ratio == 11) {
			prop->aspect_ratio = TVIN_ASPECT_14x9_FULL;
		} else {
			if (rx.cur.picture_ratio == 1)
				prop->aspect_ratio = TVIN_ASPECT_4x3_FULL;
			else if (rx.cur.picture_ratio == 2)
				prop->aspect_ratio = TVIN_ASPECT_16x9_FULL;
			else
				prop->aspect_ratio = get_format_ratio();
		}
		/*
		 * prop->bar_end_top = rx.cur.bar_end_top;
		 * prop->bar_start_bottom = rx.cur.bar_start_bottom;
		 * prop->bar_end_left = rx.cur.bar_end_left;
		 * prop->bar_start_right = rx.cur.bar_start_right;
		 */
	} else {
		if (rx.cur.picture_ratio == 1)
			prop->aspect_ratio = TVIN_ASPECT_4x3_FULL;
		else if (rx.cur.picture_ratio == 2)
			prop->aspect_ratio = TVIN_ASPECT_16x9_FULL;
		else
			prop->aspect_ratio = get_format_ratio();
	}
}

/*
 * hdmirx_set_timing_info
 * adjust timing info for video process
 */
void hdmirx_set_timing_info(struct tvin_sig_property_s *prop)
{
	enum tvin_sig_fmt_e sig_fmt;

	sig_fmt = hdmirx_hw_get_fmt();
	/* in some PC case, 4096X2160 show in 3840X2160 monitor will */
	/* result in blurred, so adjust hactive to 3840 to show dot by dot */
	if (en_4k_2_2k) {
		if (sig_fmt == TVIN_SIG_FMT_HDMI_4096_2160_00HZ ||
		    sig_fmt == TVIN_SIG_FMT_HDMI_3840_2160_00HZ) {
			prop->scaling4w = 1920;
			prop->scaling4h = 1080;
		}
	} else if (en_4096_2_3840) {
		if (sig_fmt == TVIN_SIG_FMT_HDMI_4096_2160_00HZ &&
		    prop->color_format == TVIN_RGB444) {
			prop->hs = 128;
			prop->he = 128;
		}
	}
	/* bug fix for tl1:under 4k2k50/60hz 10/12bit mode, */
	/* hdmi out clk will overstep the max sample rate of vdin */
	/* so need discard the last line data to avoid display err */
	/* 420 : hdmiout clk = pixel clk * 2 */
	/* 422 : hdmiout clk = pixel clk * colordepth / 8 */
	/* 444 : hdmiout clk = pixel clk */
	if (rx.chip_id < CHIP_ID_TL1) {
		/* tl1 need verify this bug */
		if (rx.pre.colordepth > E_COLORDEPTH_8 &&
		    prop->fps > 49 &&
		    (sig_fmt == TVIN_SIG_FMT_HDMI_4096_2160_00HZ ||
		     sig_fmt == TVIN_SIG_FMT_HDMI_3840_2160_00HZ))
			prop->ve = 1;
	} else if (rx.chip_id >= CHIP_ID_TM2) {
		/* workaround for 16bit4K flash line issue. */
		if (rx.pre.colordepth == E_COLORDEPTH_16 &&
		    (sig_fmt == TVIN_SIG_FMT_HDMI_4096_2160_00HZ ||
		     sig_fmt == TVIN_SIG_FMT_HDMI_3840_2160_00HZ))
			prop->ve = 1;
	}
	if (rx.var.dbg_ve)
		prop->ve = rx.var.dbg_ve;
}

/*
 * hdmirx_get_connect_info - get 5v info
 */
int hdmirx_get_connect_info(void)
{
	return pwr_sts;
}
EXPORT_SYMBOL(hdmirx_get_connect_info);

/*
 * hdmirx_set_cec_cfg - cec set sts
 * 0: cec off
 * 1: cec on, auto power_on is off
 * 2: cec on, auto power_on is on
 */
int hdmirx_set_cec_cfg(u32 cfg)
{
	switch (cfg) {
	case 1:
		hdmi_cec_en = 1;
		if (rx.boot_flag)
			rx_force_hpd_rxsense_cfg(1);
		break;
	case 2:
		hdmi_cec_en = 1;
		tv_auto_power_on = 1;
		if (rx.boot_flag)
			rx_force_hpd_rxsense_cfg(1);
		break;
	case 0:
	default:
		hdmi_cec_en = 0;
		/* fix source can't get edid if cec off */
		if (rx.boot_flag) {
			if (hpd_low_cec_off == 0)
				rx_force_hpd_rxsense_cfg(1);
		}
		break;
	}
	rx.boot_flag = false;
	rx_pr("cec cfg = %d\n", cfg);
	return 0;
}
EXPORT_SYMBOL(hdmirx_set_cec_cfg);

/*
 * hdmirx_get_base_fps - get base framerate by vic
 * refer to cta-861-h
 */
unsigned int hdmirx_get_base_fps(unsigned int hw_vic)
{
	unsigned int fps = 0;

	if ((hw_vic >= 1 && hw_vic <= 16)  ||
		hw_vic == 35  || hw_vic == 36  ||
		hw_vic == 69  || hw_vic == 76  ||
		hw_vic == 83  || hw_vic == 90  ||
		hw_vic == 97  || hw_vic == 102 ||
		hw_vic == 107 || hw_vic == 126 ||
		hw_vic == 199 || hw_vic == 207 ||
		hw_vic == 215)
		fps = 60;
	else if ((hw_vic >= 17 && hw_vic <= 31) ||
		(hw_vic >= 37 && hw_vic <= 39) ||
		hw_vic == 68  || hw_vic == 75  ||
		hw_vic == 82  || hw_vic == 89  ||
		hw_vic == 96  || hw_vic == 101 ||
		hw_vic == 106 || hw_vic == 125 ||
		hw_vic == 198 || hw_vic == 206 ||
		hw_vic == 214)
		fps = 50;
	else if ((hw_vic >= 40 && hw_vic <= 45) ||
		hw_vic == 64  || hw_vic == 70  ||
		hw_vic == 77  || hw_vic == 84  ||
		hw_vic == 91  || hw_vic == 117 ||
		hw_vic == 119 || hw_vic == 127 ||
		hw_vic == 200 || hw_vic == 208 ||
		hw_vic == 216 || hw_vic == 218)
		fps = 100;
	else if ((hw_vic >= 46 && hw_vic <= 51) ||
		hw_vic == 63  || hw_vic == 71  ||
		hw_vic == 78  || hw_vic == 85  ||
		hw_vic == 92  || hw_vic == 118 ||
		hw_vic == 120 || hw_vic == 193 ||
		hw_vic == 201 || hw_vic == 209 ||
		hw_vic == 217 || hw_vic == 219)
		fps = 120;
	else if (hw_vic >= 52 && hw_vic <= 55)
		fps = 200;
	else if (hw_vic >= 56 && hw_vic <= 59)
		fps = 240;

	return fps;
}
EXPORT_SYMBOL(hdmirx_get_base_fps);

/* see CEA-861-F table-12 and chapter 5.1:
 * By default, RGB pixel data values should be assumed to have
 * the limited range when receiving a CE video format, and the
 * full range when receiving an IT format.
 * CE Video Format: Any Video Format listed in Table 1
 * except the 640x480p Video Format.
 * IT Video Format: Any Video Format that is not a CE Video Format.
 */
static bool is_it_vid_fmt(void)
{
	bool ret = false;

	if (rx.pre.sw_vic == HDMI_640x480p60 ||
	    (rx.pre.sw_vic >= HDMI_VESA_OFFSET &&
	    rx.pre.sw_vic < HDMI_UNSUPPORT))
		ret = true;
	else
		ret = false;

	if (log_level & ERR_LOG)
		rx_pr("sw_vic: %d, it video format: %d\n", rx.pre.sw_vic, ret);
	return ret;
}

/* see CEA-861F page43
 * A Sink should adjust its scan based on the value of S.
 * A Sink would overscan if it received S=1, and underscan
 * if it received S=2. If it receives S=0, then it should
 * overscan for a CE Video Format and underscan for an IT
 * Video Format.
 */
static enum scan_mode_e hdmirx_get_scan_mode(void)
{
	if (rx.state != FSM_SIG_READY)
		return E_UNKNOWN_SCAN;

	if (rx.pre.scan_info == E_OVERSCAN)
		return E_OVERSCAN;
	else if (rx.pre.scan_info == E_UNDERSCAN)
		return E_UNDERSCAN;

	if (is_it_vid_fmt() || rx.pre.it_content) {
		return E_UNDERSCAN;
	} else if ((rx.pre.hw_vic == HDMI_UNKNOWN) &&
		 (rx.pre.sw_vic > HDMI_UNKNOWN) &&
		 (rx.pre.sw_vic < HDMI_UNSUPPORT)) {
		/* TX may send VESA timing 1080p, VIC = 0 */
		if (log_level & VIDEO_LOG)
			rx_pr("under scan for vesa mode\n");
		return E_UNDERSCAN;
	}

	return E_OVERSCAN;
}

static ssize_t scan_mode_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	return sprintf(buf, "%x\n", hdmirx_get_scan_mode());
}

/*
 * hdmirx_get_color_fmt - get video color format
 */
void hdmirx_get_color_fmt(struct tvin_sig_property_s *prop)
{
	int format = rx.cur.colorspace;

	if (rx.pre.sw_dvi == 1)
		format = E_COLOR_RGB;
	switch (format) {
	case E_COLOR_YUV422:
	case E_COLOR_YUV420:
		prop->color_format = TVIN_YUV422;
		prop->dest_cfmt = TVIN_YUV422;
		break;
	case E_COLOR_YUV444:
		prop->color_format = TVIN_YUV444;
		if (!hdmi_yuv444_enable)
			prop->dest_cfmt = TVIN_YUV422;
		else
			prop->dest_cfmt = TVIN_YUV444;
		break;
	case E_COLOR_RGB:
	default:
		prop->color_format = TVIN_RGB444;
		prop->dest_cfmt = TVIN_YUV444;
		break;
	}
	if (rx.pre.interlaced == 1)
		prop->dest_cfmt = TVIN_YUV422;

	switch (prop->color_format) {
	case TVIN_YUV444:
	case TVIN_YUV422:
		/* Table 8-5 of hdmi1.4b spec */
		if (yuv_quant_range == E_YCC_RANGE_LIMIT)
			prop->color_fmt_range = TVIN_YUV_LIMIT;
		else if (yuv_quant_range == E_YCC_RANGE_FULL)
			prop->color_fmt_range = TVIN_YUV_FULL;
		else
			prop->color_fmt_range = TVIN_FMT_RANGE_NULL;
		break;
	case TVIN_RGB444:
		if (rgb_quant_range == E_RGB_RANGE_FULL ||
		    rx.pre.sw_dvi == 1)
			prop->color_fmt_range = TVIN_RGB_FULL;
		else if (rgb_quant_range == E_RGB_RANGE_LIMIT)
			prop->color_fmt_range = TVIN_RGB_LIMIT;
		else if (is_it_vid_fmt())
			prop->color_fmt_range = TVIN_RGB_FULL;
		else
			prop->color_fmt_range = TVIN_RGB_LIMIT;
		break;
	default:
		prop->color_fmt_range = TVIN_FMT_RANGE_NULL;
		break;
	}
}

/*
 * hdmirx_get_colordepth - get video color depth
 */
void hdmirx_get_colordepth(struct tvin_sig_property_s *prop)
{
	prop->colordepth = rx.pre.colordepth;
}

int hdmirx_hw_get_3d_structure(void)
{
	u8 ret = 0;

	if (rx.vs_info_details.vd_fmt == VSI_FORMAT_3D_FORMAT)
		ret = 1;
	return ret;
}

/*
 * hdmirx_get_vsi_info - get vsi info
 * this func is used to get 3D format and dobly
 * vision state of current video
 */
void hdmirx_get_vsi_info(struct tvin_sig_property_s *prop)
{
	static u8 last_vsi_state;

	if (last_vsi_state != rx.vs_info_details.vsi_state) {
		if (log_level & PACKET_LOG) {
			rx_pr("!!!vsi state = %d\n",
			      rx.vs_info_details.vsi_state);
			rx_pr("1:4K3D;2:vsi21;3:HDR10+;4:DV10;5:DV15\n");
		}
		prop->trans_fmt = TVIN_TFMT_2D;
		prop->dolby_vision = DV_NULL;
		prop->hdr10p_info.hdr10p_on = false;
		prop->cuva_info.cuva_on = false;
		prop->filmmaker.fmm_vsif_flag = false;
		last_vsi_state = rx.vs_info_details.vsi_state;
	}
	if (rx.pre.colorspace != E_COLOR_YUV420)
		prop->dolby_vision = rx.vs_info_details.dolby_vision_flag;

	if (rx.chip_id >= CHIP_ID_T7) {
		if (log_level & PACKET_LOG && rx.new_emp_pkt)
			rx_pr("vsi_state:0x%x\n", rx.vs_info_details.vsi_state);

		if (rx.vs_info_details.vsi_state & E_VSI_DV10 ||
			rx.vs_info_details.vsi_state & E_VSI_DV15)
			rx.vs_info_details.vsi_state = E_VSI_DV15;
		else if (rx.vs_info_details.vsi_state & E_VSI_HDR10PLUS)
			rx.vs_info_details.vsi_state = E_VSI_HDR10PLUS;
		else if (rx.vs_info_details.vsi_state & E_VSI_CUVAHDR)
			rx.vs_info_details.vsi_state = E_VSI_CUVAHDR;
		else if (rx.vs_info_details.vsi_state & E_VSI_FILMMAKER)
			rx.vs_info_details.vsi_state = E_VSI_FILMMAKER;
		else if (rx.vs_info_details.vsi_state & E_VSI_VSI21)
			rx.vs_info_details.vsi_state = E_VSI_VSI21;
		else
			rx.vs_info_details.vsi_state = E_VSI_4K3D;

		switch (rx.vs_info_details.vsi_state) {
		case E_VSI_DV10:
		case E_VSI_DV15:
			prop->low_latency = rx.vs_info_details.low_latency;
			if (rx.vs_info_details.dolby_vision_flag == DV_VSIF) {
				memcpy(&prop->dv_vsif_raw,
				       &rx_pkt.multi_vs_info[DV15], 3);
				memcpy((char *)(&prop->dv_vsif_raw) + 3,
				       &rx_pkt.multi_vs_info[DV15].PB0,
				       sizeof(struct tvin_dv_vsif_raw_s) - 4);
			}
			break;
		case E_VSI_HDR10PLUS:
			prop->hdr10p_info.hdr10p_on = rx.vs_info_details.hdr10plus;
			memcpy(&prop->hdr10p_info.hdr10p_data, &rx_pkt.multi_vs_info[HDR10PLUS],
				sizeof(struct tvin_hdr10p_data_s));
			break;
		case E_VSI_CUVAHDR:
			prop->cuva_info.cuva_on = true;
			if (rx.vs_info_details.cuva_hdr) {
				memset(&prop->cuva_info.cuva_data, 0,
					sizeof(struct tvin_cuva_data_s));
				memcpy(&prop->cuva_info.cuva_data,
				       &rx_pkt.multi_vs_info[CUVAHDR],
				       sizeof(struct tvin_cuva_data_s));
			}
			break;
		case E_VSI_FILMMAKER:
			if (rx.vs_info_details.filmmaker && !rx.vs_info_details.hdmi_allm) {
				rx.vsif_fmm_flag = true;
				memset(&prop->filmmaker.fmm_data, 0,
					sizeof(struct tvin_fmm_data_s));
				memcpy((u8 *)&prop->filmmaker.fmm_data,
					(u8 *)&rx_pkt.multi_vs_info[FILMMAKER], 3);
				memcpy((u8 *)&prop->filmmaker.fmm_data + 3,
					(u8 *)&rx_pkt.multi_vs_info[FILMMAKER] + 4,
					sizeof(struct tvin_fmm_data_s) - 3);
			}
			break;
		case E_VSI_4K3D:
			if (hdmirx_hw_get_3d_structure() == 1) {
				if (rx.vs_info_details._3d_structure == 0x1) {
					/* field alternative */
					prop->trans_fmt = TVIN_TFMT_3D_FA;
				} else if (rx.vs_info_details._3d_structure == 0x2) {
					/* line alternative */
					prop->trans_fmt = TVIN_TFMT_3D_LA;
				} else if (rx.vs_info_details._3d_structure == 0x3) {
					/* side-by-side full */
					prop->trans_fmt = TVIN_TFMT_3D_LRF;
				} else if (rx.vs_info_details._3d_structure == 0x4) {
					/* L + depth */
					prop->trans_fmt = TVIN_TFMT_3D_LD;
				} else if (rx.vs_info_details._3d_structure == 0x5) {
					/* L + depth + graphics + graphics-depth */
					prop->trans_fmt = TVIN_TFMT_3D_LDGD;
				} else if (rx.vs_info_details._3d_structure == 0x6) {
					/* top-and-bot */
					prop->trans_fmt = TVIN_TFMT_3D_TB;
				} else if (rx.vs_info_details._3d_structure == 0x8) {
					/* Side-by-Side half */
					switch (rx.vs_info_details._3d_ext_data) {
					case 0x5:
						/*Odd/Left picture, Even/Right picture*/
						prop->trans_fmt = TVIN_TFMT_3D_LRH_OLER;
						break;
					case 0x6:
						/*Even/Left picture, Odd/Right picture*/
						prop->trans_fmt = TVIN_TFMT_3D_LRH_ELOR;
						break;
					case 0x7:
						/*Even/Left picture, Even/Right picture*/
						prop->trans_fmt = TVIN_TFMT_3D_LRH_ELER;
						break;
					case 0x4:
						/*Odd/Left picture, Odd/Right picture*/
					default:
						prop->trans_fmt = TVIN_TFMT_3D_LRH_OLOR;
						break;
					}
				}
			}
			break;
		case E_VSI_VSI21:
			if (hdmirx_hw_get_3d_structure() == 1) {
				if (rx.vs_info_details._3d_structure == 0x1) {
					/* field alternative */
					prop->trans_fmt = TVIN_TFMT_3D_FA;
				} else if (rx.vs_info_details._3d_structure == 0x2) {
				/* line alternative */
					prop->trans_fmt = TVIN_TFMT_3D_LA;
				} else if (rx.vs_info_details._3d_structure == 0x3) {
					/* side-by-side full */
					prop->trans_fmt = TVIN_TFMT_3D_LRF;
				} else if (rx.vs_info_details._3d_structure == 0x4) {
					/* L + depth */
					prop->trans_fmt = TVIN_TFMT_3D_LD;
				} else if (rx.vs_info_details._3d_structure == 0x5) {
					/* L + depth + graphics + graphics-depth */
					prop->trans_fmt = TVIN_TFMT_3D_LDGD;
				} else if (rx.vs_info_details._3d_structure == 0x6) {
					/* top-and-bot */
					prop->trans_fmt = TVIN_TFMT_3D_TB;
				} else if (rx.vs_info_details._3d_structure == 0x8) {
					/* Side-by-Side half */
					switch (rx.vs_info_details._3d_ext_data) {
					case 0x5:
						/*Odd/Left picture, Even/Right picture*/
						prop->trans_fmt = TVIN_TFMT_3D_LRH_OLER;
						break;
					case 0x6:
						/*Even/Left picture, Odd/Right picture*/
						prop->trans_fmt = TVIN_TFMT_3D_LRH_ELOR;
						break;
					case 0x7:
						/*Even/Left picture, Even/Right picture*/
						prop->trans_fmt = TVIN_TFMT_3D_LRH_ELER;
						break;
					case 0x4:
						/*Odd/Left picture, Odd/Right picture*/
					default:
						prop->trans_fmt = TVIN_TFMT_3D_LRH_OLOR;
						break;
					}
				}
				if (rx.threed_info.meta_data_flag) {
					prop->threed_info.meta_data_flag = true;
					prop->threed_info.meta_data_type =
						rx.threed_info.meta_data_type;
					prop->threed_info.meta_data_length =
						rx.threed_info.meta_data_length;
					memcpy(prop->threed_info.meta_data,
						rx.threed_info.meta_data,
						sizeof(prop->threed_info.meta_data));
				}
			}
			break;
		default:
			break;
		}

	} else {
		switch (rx.vs_info_details.vsi_state) {
		case E_VSI_HDR10PLUS:
			prop->hdr10p_info.hdr10p_on = rx.vs_info_details.hdr10plus;
			memcpy(&prop->hdr10p_info.hdr10p_data,
			       &rx_pkt.vs_info, sizeof(struct tvin_hdr10p_data_s));
			break;
		case E_VSI_DV10:
		case E_VSI_DV15:
			prop->low_latency = rx.vs_info_details.low_latency;
			if (rx.vs_info_details.dolby_vision_flag == DV_VSIF) {
				memcpy(&prop->dv_vsif_raw,
				       &rx_pkt.vs_info, 3);
				memcpy((char *)(&prop->dv_vsif_raw) + 3,
				       &rx_pkt.vs_info.PB0,
				       sizeof(struct tvin_dv_vsif_raw_s) - 4);
			}
			break;
		case E_VSI_FILMMAKER:
			if (rx.vs_info_details.filmmaker) {
				rx.vsif_fmm_flag = true;
				memset(&prop->filmmaker.fmm_data, 0,
					sizeof(struct tvin_fmm_data_s));
				memcpy((u8 *)&prop->filmmaker.fmm_data,
					(u8 *)&rx_pkt.vs_info, 3);
				memcpy((u8 *)&prop->filmmaker.fmm_data + 3,
					(u8 *)&rx_pkt.vs_info + 4,
					sizeof(struct tvin_fmm_data_s) - 3);
			}
		break;
		case E_VSI_4K3D:
		if (hdmirx_hw_get_3d_structure() == 1) {
			if (rx.vs_info_details._3d_structure == 0x1) {
				/* field alternative */
				prop->trans_fmt = TVIN_TFMT_3D_FA;
			} else if (rx.vs_info_details._3d_structure == 0x2) {
				/* line alternative */
				prop->trans_fmt = TVIN_TFMT_3D_LA;
			} else if (rx.vs_info_details._3d_structure == 0x3) {
				/* side-by-side full */
				prop->trans_fmt = TVIN_TFMT_3D_LRF;
			} else if (rx.vs_info_details._3d_structure == 0x4) {
				/* L + depth */
				prop->trans_fmt = TVIN_TFMT_3D_LD;
			} else if (rx.vs_info_details._3d_structure == 0x5) {
				/* L + depth + graphics + graphics-depth */
				prop->trans_fmt = TVIN_TFMT_3D_LDGD;
			} else if (rx.vs_info_details._3d_structure == 0x6) {
				/* top-and-bot */
				prop->trans_fmt = TVIN_TFMT_3D_TB;
			} else if (rx.vs_info_details._3d_structure == 0x8) {
				/* Side-by-Side half */
				switch (rx.vs_info_details._3d_ext_data) {
				case 0x5:
					/*Odd/Left picture, Even/Right picture*/
					prop->trans_fmt = TVIN_TFMT_3D_LRH_OLER;
					break;
				case 0x6:
					/*Even/Left picture, Odd/Right picture*/
					prop->trans_fmt = TVIN_TFMT_3D_LRH_ELOR;
					break;
				case 0x7:
					/*Even/Left picture, Even/Right picture*/
					prop->trans_fmt = TVIN_TFMT_3D_LRH_ELER;
					break;
				case 0x4:
					/*Odd/Left picture, Odd/Right picture*/
				default:
					prop->trans_fmt = TVIN_TFMT_3D_LRH_OLOR;
					break;
				}
			}
		}
		break;
		case E_VSI_VSI21:
			if (hdmirx_hw_get_3d_structure() == 1) {
				if (rx.vs_info_details._3d_structure == 0x1) {
					/* field alternative */
					prop->trans_fmt = TVIN_TFMT_3D_FA;
				} else if (rx.vs_info_details._3d_structure == 0x2) {
				/* line alternative */
					prop->trans_fmt = TVIN_TFMT_3D_LA;
				} else if (rx.vs_info_details._3d_structure == 0x3) {
					/* side-by-side full */
					prop->trans_fmt = TVIN_TFMT_3D_LRF;
				} else if (rx.vs_info_details._3d_structure == 0x4) {
					/* L + depth */
					prop->trans_fmt = TVIN_TFMT_3D_LD;
				} else if (rx.vs_info_details._3d_structure == 0x5) {
					/* L + depth + graphics + graphics-depth */
					prop->trans_fmt = TVIN_TFMT_3D_LDGD;
				} else if (rx.vs_info_details._3d_structure == 0x6) {
					/* top-and-bot */
					prop->trans_fmt = TVIN_TFMT_3D_TB;
				} else if (rx.vs_info_details._3d_structure == 0x8) {
					/* Side-by-Side half */
					switch (rx.vs_info_details._3d_ext_data) {
					case 0x5:
						/*Odd/Left picture, Even/Right picture*/
						prop->trans_fmt = TVIN_TFMT_3D_LRH_OLER;
						break;
					case 0x6:
						/*Even/Left picture, Odd/Right picture*/
						prop->trans_fmt = TVIN_TFMT_3D_LRH_ELOR;
						break;
					case 0x7:
						/*Even/Left picture, Even/Right picture*/
						prop->trans_fmt = TVIN_TFMT_3D_LRH_ELER;
						break;
					case 0x4:
						/*Odd/Left picture, Odd/Right picture*/
					default:
						prop->trans_fmt = TVIN_TFMT_3D_LRH_OLOR;
						break;
					}
				}
				if (rx.threed_info.meta_data_flag) {
					prop->threed_info.meta_data_flag = true;
					prop->threed_info.meta_data_type =
						rx.threed_info.meta_data_type;
					prop->threed_info.meta_data_length =
						rx.threed_info.meta_data_length;
					memcpy(prop->threed_info.meta_data,
						rx.threed_info.meta_data,
						sizeof(prop->threed_info.meta_data));
				}
			}
			break;
		default:
			break;
		}
	}
}

void hdmirx_get_spd_info(struct tvin_sig_property_s *prop)
{
	if (!rx_pkt_chk_updated_spd()) {
		memset(&prop->spd_data, 0, sizeof(struct tvin_spd_data_s));
	} else {
		rx_pkt_clr_updated_spd();
		memcpy(&prop->spd_data, &rx_pkt.spd_info, sizeof(struct tvin_spd_data_s));
	}
}

/*
 * hdmirx_get_repetition_info - get repetition info
 */
void hdmirx_get_repetition_info(struct tvin_sig_property_s *prop)
{
	prop->decimation_ratio = 0;
}

/*
 * hdmirx_get_allm_mode - get allm mode
 */
void hdmirx_get_latency_info(struct tvin_sig_property_s *prop)
{
	prop->latency.allm_mode =
		rx.vs_info_details.hdmi_allm || rx.vs_info_details.dv_allm;
	prop->latency.it_content = rx.cur.it_content;
	prop->latency.cn_type = rx.cur.cn_type;
#ifdef CONFIG_AMLOGIC_HDMITX
	if (rx.open_fg  &&
		(latency_info.allm_mode != rx.vs_info_details.hdmi_allm ||
		latency_info.it_content != rx.cur.it_content ||
		latency_info.cn_type != rx.cur.cn_type)) {
		latency_info.allm_mode  = rx.vs_info_details.hdmi_allm;
		latency_info.it_content = rx.cur.it_content;
		latency_info.cn_type  = rx.cur.cn_type;
		hdmitx_update_latency_info(&latency_info);
	}
#endif
}

static u32 emp_irq_cnt;
void hdmirx_get_emp_dv_info(struct tvin_sig_property_s *prop)
{
	u8 i;

	//emp buffer not only stores DV_EMP packet, but also other packets.
	//only DV_EMP is needed here
	if (rx.vs_info_details.dolby_vision_flag != DV_EMP)
		return;

	if (rx.emp_dv_info.flag) {
		prop->emp_data.size = rx.emp_dv_info.dv_pkt_cnt;
		for (i = 0; i < rx.emp_dv_info.dv_pkt_cnt; i++) {
			memcpy(prop->emp_data.empbuf + i * 31,
				rx.emp_dv_info.dv_addr + i * 32, 3);
			//28=31-3 start of PB0
			memcpy(prop->emp_data.empbuf + i * 31 + 3,
				rx.emp_dv_info.dv_addr + i * 32 + 4, 28);
		}
	}

#ifndef HDMIRX_SEND_INFO_TO_VDIN
	if (emp_irq_cnt == rx.emp_buff.irq_cnt)
		rx.vs_info_details.emp_pkt_cnt = 0;
	emp_irq_cnt = rx.emp_buff.irq_cnt;
#endif
}

void hdmirx_get_vtem_info(struct tvin_sig_property_s *prop)
{
	memset(&prop->vtem_data, 0, sizeof(struct tvin_vtem_data_s));
	if (rx.vtem_info.vrr_en)
		memcpy(&prop->vtem_data,
			   &rx.vtem_info, sizeof(struct vtem_info_s));
}

void hdmirx_get_sbtm_info(struct tvin_sig_property_s *prop)
{
	memset(&prop->sbtm_data, 0, sizeof(struct tvin_sbtm_data_s));
	if (rx.sbtm_info.flag)
		memcpy(&prop->sbtm_data,
			   &rx.sbtm_info, sizeof(struct sbtm_info_s));
}

void hdmirx_get_cuva_emds_info(struct tvin_sig_property_s *prop)
{
	if (rx.emp_cuva_info.cuva_emds_size > sizeof(prop->cuva_emds_data))
		rx_pr("cuva emds size exceeds 96 bytes\n");
	memset(&prop->cuva_emds_data, 0, sizeof(prop->cuva_emds_data));
	if (rx.emp_cuva_info.flag)
		memcpy(&prop->cuva_emds_data, rx.emp_cuva_info.emds_addr,
			sizeof(prop->cuva_emds_data));
}

void hdmirx_get_fmm_info(struct tvin_sig_property_s *prop)
{
	/*
	 * 1) AVI InfoFrame it_content:Byte3(bit7)-1 & content_type:Byte5(bit4-5):2
	 * 2) Vendor Specific InfoFrame 0x1ABBFB & content_type:0x01 & content_subtype:0x0
	 */
	if ((rx.cur.it_content && rx.cur.cn_type == 0x2)) {
		prop->filmmaker.fmm_flag = true;
		prop->filmmaker.it_content = rx.cur.it_content;
		prop->filmmaker.cn_type = rx.cur.cn_type;
	} else if (rx.vsif_fmm_flag) {
		prop->filmmaker.fmm_vsif_flag = true;
	} else {
		prop->filmmaker.fmm_flag = false;
		prop->filmmaker.fmm_vsif_flag = false;
	}
}

void rx_set_sig_info(void)
{
	struct tvin_frontend_s *fe = tvin_get_frontend(TVIN_PORT_HDMI0,
		VDIN_FRONTEND_IDX);

	if (fe->sm_ops && fe->sm_ops->vdin_set_property)
		fe->sm_ops->vdin_set_property(fe);
	else
		rx_pr("hdmi notify err\n");
}

void rx_update_sig_info(void)
{
	rx_check_pkt_flag();
	rx_get_vsi_info();
	rx_get_em_info();
	//rx_get_aif_info();
	rx_set_sig_info();
}

/*
 * hdmirx_get_hdr_info - get hdr info
 */
void hdmirx_get_hdr_info(struct tvin_sig_property_s *prop)
{
	struct drm_infoframe_st *drm_pkt;

	/*check drm packet is attach every VS*/
	u32 drm_attach = rx_pkt_chk_attach_drm();

	drm_pkt = (struct drm_infoframe_st *)&rx_pkt.drm_info;

	if (drm_attach) {
		rx.hdr_info.hdr_state = HDR_STATE_SET;
		rx_pkt_clr_attach_drm();
	}

	/* hdr data processing */
	switch (rx.hdr_info.hdr_state) {
	case HDR_STATE_NULL:
		/* filter for state, 10*10ms */
		if (rx.hdr_info.hdr_check_cnt++ > 10) {
			prop->hdr_info.hdr_state = HDR_STATE_NULL;
			rx.hdr_info.hdr_check_cnt = 0;
		}
		break;
	case HDR_STATE_GET:
		rx.hdr_info.hdr_check_cnt = 0;
		break;
	case HDR_STATE_SET:
		rx.hdr_info.hdr_check_cnt = 0;
		if (prop->hdr_info.hdr_state != HDR_STATE_GET) {
			if (rx_pkt_chk_busy_drm())
				break;

			prop->hdr_info.hdr_data.length = drm_pkt->length;
			prop->hdr_info.hdr_data.eotf = drm_pkt->des_u.tp1.eotf;
			prop->hdr_info.hdr_data.metadata_id =
				drm_pkt->des_u.tp1.meta_des_id;
			prop->hdr_info.hdr_data.primaries[0].x =
				drm_pkt->des_u.tp1.dis_pri_x0;
			prop->hdr_info.hdr_data.primaries[0].y =
				drm_pkt->des_u.tp1.dis_pri_y0;
			prop->hdr_info.hdr_data.primaries[1].x =
				drm_pkt->des_u.tp1.dis_pri_x1;
			prop->hdr_info.hdr_data.primaries[1].y =
				drm_pkt->des_u.tp1.dis_pri_y1;
			prop->hdr_info.hdr_data.primaries[2].x =
				drm_pkt->des_u.tp1.dis_pri_x2;
			prop->hdr_info.hdr_data.primaries[2].y =
				drm_pkt->des_u.tp1.dis_pri_y2;
			prop->hdr_info.hdr_data.white_points.x =
				drm_pkt->des_u.tp1.white_points_x;
			prop->hdr_info.hdr_data.white_points.y =
				drm_pkt->des_u.tp1.white_points_y;
			prop->hdr_info.hdr_data.master_lum.x =
				drm_pkt->des_u.tp1.max_dislum;
			prop->hdr_info.hdr_data.master_lum.y =
				drm_pkt->des_u.tp1.min_dislum;
			prop->hdr_info.hdr_data.mcll =
				drm_pkt->des_u.tp1.max_light_lvl;
			prop->hdr_info.hdr_data.mfall =
				drm_pkt->des_u.tp1.max_fa_light_lvl;
			prop->hdr_info.hdr_data.rawdata[0] = 0x87;
			prop->hdr_info.hdr_data.rawdata[1] = 0x1;
			prop->hdr_info.hdr_data.rawdata[2] = drm_pkt->length;
			memcpy(&prop->hdr_info.hdr_data.rawdata[3],
				   &drm_pkt->des_u.payload, 28);
			/* vdin can read current hdr data */
			prop->hdr_info.hdr_state = HDR_STATE_GET;
		}
		/* Rx can get new hdr data now, the hdr data
		 * is already taken by vdin, no need to
		 * wait until vdin process this hdr data
		 */
		rx.hdr_info.hdr_state = HDR_STATE_NULL;
		break;
	default:
		break;
	}
}

/*
 * get hdmirx avi packet ext_colorimetry info
 */
 #define E_EXTENDED_VALID 3
 #define E_ADDITIONAL_VALID 7
void hdmirx_get_avi_ext_colorimetry(struct tvin_sig_property_s *prop)
{
	prop->avi_ec = rx.cur.ext_colorimetry;
	if (rx.cur.colorimetry != E_EXTENDED_VALID) {
		prop->avi_ec = rx.cur.colorimetry;
	} else {
		if (rx.cur.ext_colorimetry != E_ADDITIONAL_VALID)
			prop->avi_ec = E_EXTENDED_VALID + rx.cur.ext_colorimetry;
		else
			prop->avi_ec = E_ADDITIONAL_VALID;//todo
	}
}

/***************************************************
 *func: hdmirx_get_sig_property - get signal property
 **************************************************/
void hdmirx_get_sig_property(struct tvin_frontend_s *fe,
			     struct tvin_sig_property_s *prop)
{
	hdmirx_get_dvi_info(prop);
	hdmirx_get_colordepth(prop);
	hdmirx_get_fps_info(prop);
	hdmirx_get_color_fmt(prop);
	hdmirx_get_repetition_info(prop);
	hdmirx_set_timing_info(prop);
	hdmirx_get_hdr_info(prop);
	hdmirx_get_vsi_info(prop);
	hdmirx_get_spd_info(prop);
	hdmirx_get_latency_info(prop);
	hdmirx_get_emp_dv_info(prop);
	hdmirx_get_vtem_info(prop);
	hdmirx_get_sbtm_info(prop);
	hdmirx_get_cuva_emds_info(prop);
	hdmirx_get_fmm_info(prop);
	hdmirx_get_active_aspect_ratio(prop);
	hdmirx_get_hdcp_sts(prop);
	hdmirx_get_hw_vic(prop);
	hdmirx_get_avi_ext_colorimetry(prop);
	prop->skip_vf_num = vdin_drop_frame_cnt;
}

/*
 * hdmirx_check_frame_skip - check frame skip
 *
 * return true if video frame skip is needed,
 * return false otherwise.
 */
bool hdmirx_check_frame_skip(struct tvin_frontend_s *fe)
{
	return hdmirx_hw_check_frame_skip();
}

bool hdmirx_dv_config(bool en, struct tvin_frontend_s *fe)
{
	set_dv_ll_mode(en);

	return true;
}

bool hdmirx_clr_vsync(struct tvin_frontend_s *fe)
{
	return rx_clr_tmds_valid();
}

static struct tvin_state_machine_ops_s hdmirx_sm_ops = {
	.nosig            = hdmirx_is_nosig,
	.fmt_changed      = hdmirx_fmt_chg,
	.get_fmt          = hdmirx_get_fmt,
	.fmt_config       = NULL,
	.adc_cal          = NULL,
	.pll_lock         = NULL,
	.get_sig_property  = hdmirx_get_sig_property,
	.vga_set_param    = NULL,
	.vga_get_param    = NULL,
	.check_frame_skip = hdmirx_check_frame_skip,
	.hdmi_dv_config   = hdmirx_dv_config,
	.hdmi_clr_vsync	= hdmirx_clr_vsync,
};

/*
 * hdmirx_open - file operation interface
 */
static int hdmirx_open(struct inode *inode, struct file *file)
{
	struct hdmirx_dev_s *devp;

	devp = container_of(inode->i_cdev, struct hdmirx_dev_s, cdev);
	file->private_data = devp;
	return 0;
}

/*
 * hdmirx_release - file operation interface
 */
static int hdmirx_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

/*
 * hdmirx_ioctl - file operation interface
 */
static long hdmirx_ioctl(struct file *file, unsigned int cmd,
			 unsigned long arg)
{
	long ret = 0;
	/* unsigned int delay_cnt = 0; */
	struct hdmirx_dev_s *devp = NULL;
	void __user *argp = (void __user *)arg;
	u32 param = 0, temp_val, temp;
	//unsigned int size = sizeof(struct pd_infoframe_s);
	struct pd_infoframe_s pkt_info;
	struct spd_infoframe_st *spdpkt;
	unsigned int pin_status;
	void *src_buff;
	u8 sad_data[30];
	u8 len = 0;
	u8 i = 0;
	u8 port_idx = 0;
	u8 hdmi_idx = 0;

	if (_IOC_TYPE(cmd) != HDMI_IOC_MAGIC) {
		pr_err("%s invalid command: %u\n", __func__, cmd);
		return -EINVAL;
	}
	src_buff = &pkt_info;
	devp = file->private_data;
	switch (cmd) {
	case HDMI_IOC_HDCP_GET_KSV:{
		struct _hdcp_ksv ksv;

		if (!argp)
			return -EINVAL;
		mutex_lock(&devp->rx_lock);
		ksv.bksv0 = rx.hdcp.bksv[0];
		ksv.bksv1 = rx.hdcp.bksv[1];
		if (copy_to_user(argp, &ksv, sizeof(struct _hdcp_ksv))) {
			ret = -EFAULT;
			mutex_unlock(&devp->rx_lock);
			break;
		}
		mutex_unlock(&devp->rx_lock);
		break;
	}
	case HDMI_IOC_HDCP_ON:
		hdcp_enable = 1;
		rx_irq_en(false);
		rx_set_cur_hpd(0, 4);
		/*fsm_restart();*/
		break;
	case HDMI_IOC_HDCP_OFF:
		hdcp_enable = 0;
		rx_irq_en(false);
		rx_set_cur_hpd(0, 4);
		hdmirx_hw_config();
		/*fsm_restart();*/
		break;
	case HDMI_IOC_EDID_UPDATE:
		/* ref board ui can only be set in current hdmi port */
		if (edid_delivery_mothed == EDID_DELIVERY_ALL_PORT) {
			rx_irq_en(false);
			rx_set_cur_hpd(0, 4);
			edid_update_flag = 1;
		}
		fsm_restart();
		rx_pr("*update edid*\n");
		break;
	case HDMI_IOC_EDID_UPDATE_WITH_PORT:
		if (!argp)
			return -EINVAL;
		mutex_lock(&devp->rx_lock);
		if (copy_from_user(&hdmi_idx, argp, sizeof(unsigned char))) {
			ret = -EFAULT;
			mutex_unlock(&devp->rx_lock);
			break;
		}
		port_idx = hdmi_idx - 1;
		if (port_idx >= 3) {
			rx_pr("port_idx illeage\n");
			ret = -EFAULT;
			mutex_unlock(&devp->rx_lock);
			break;
		}

		rx_set_port_hpd(port_idx, 0);
		rx_pr("port%d_hpd_low\n", port_idx);

		if (!rx.open_fg) {
			port_hpd_rst_flag |= (1 << port_idx);
			hdmi_rx_top_edid_update();
		} else {
			if (port_idx != rx.port) {
				port_hpd_rst_flag |= (1 << port_idx);
				hdmi_rx_top_edid_update();
			} else {
				rx_irq_en(false);
				//rx_set_cur_hpd(0, 4);
				fsm_restart();
			}
		}
		mutex_unlock(&devp->rx_lock);
		break;
	case HDMI_IOC_PC_MODE_ON:
		pc_mode_en = 1;
		hotplug_wait_query();
		rx_pr("pc mode on\n");
		break;
	case HDMI_IOC_PC_MODE_OFF:
		pc_mode_en = 0;
		hotplug_wait_query();
		rx_pr("pc mode off\n");
		break;
	case HDMI_IOC_HDCP22_AUTO:
		break;
	case HDMI_IOC_HDCP22_FORCE14:
		break;
	case HDMI_IOC_PD_FIFO_PKTTYPE_EN:
		/*rx_pr("IOC_PD_FIFO_PKTTYPE_EN\n");*/
		/*get input param*/
		if (copy_from_user(&param, argp, sizeof(u32))) {
			pr_err("get pd fifo param err\n");
			ret = -EFAULT;
			break;
		}
		rx_pr("en cmd:0x%x\n", param);
		rx_pkt_buffclear(param);
		temp = rx_pkt_type_mapping(param);
		packet_fifo_cfg |= temp;
		/*enable pkt pd fifo*/
		temp_val = hdmirx_rd_dwc(DWC_PDEC_CTRL);
		temp_val |= temp;
		hdmirx_wr_dwc(DWC_PDEC_CTRL, temp_val);
		/*enable int*/
		pdec_ists_en |= PD_FIFO_START_PASS | PD_FIFO_OVERFL;
		/*open pd fifo interrupt source if signal stable*/
		temp_val = hdmirx_rd_dwc(DWC_PDEC_IEN);
		if ((temp_val & PD_FIFO_START_PASS) == 0 &&
		    rx.state >= FSM_SIG_UNSTABLE) {
			temp_val |= pdec_ists_en;
			hdmirx_wr_dwc(DWC_PDEC_IEN_SET, temp_val);
			rx_pr("open pd fifo int:0x%x\n", pdec_ists_en);
		}
		break;
	case HDMI_IOC_PD_FIFO_PKTTYPE_DIS:
		/*rx_pr("IOC_PD_FIFO_PKTTYPE_DIS\n");*/
		/*get input param*/
		if (copy_from_user(&param, argp, sizeof(u32))) {
			pr_err("get pd fifo param err\n");
			ret = -EFAULT;
			break;
		}
		rx_pr("dis cmd:0x%x\n", param);
		temp = rx_pkt_type_mapping(param);
		packet_fifo_cfg &= ~temp;
		/*disable pkt pd fifo*/
		temp_val = hdmirx_rd_dwc(DWC_PDEC_CTRL);
		temp_val &= ~temp;
		hdmirx_wr_dwc(DWC_PDEC_CTRL, temp_val);
		break;

	case HDMI_IOC_GET_PD_FIFO_PARAM:
		/*mutex_lock(&pktbuff_lock);*/
		/*rx_pr("IOC_GET_PD_FIFO_PARAM\n");*/
		/*get input param*/
		//if (copy_from_user(&param, argp, sizeof(u32))) {
			//pr_err("get pd fifo param err\n");
			//ret = -EFAULT;
			/*mutex_unlock(&pktbuff_lock);*/
			//break;
	//	}
		//memset(&pkt_info, 0, sizeof(pkt_info));
		//src_buff = &pkt_info;
		//size = sizeof(struct pd_infoframe_s);
		//rx_get_pd_fifo_param(param, &pkt_info);

		/*return pkt info*/
		//if (size > 0 && !argp) {
			//if (copy_to_user(argp, src_buff, size)) {
				//pr_err("get pd fifo param err\n");
				//ret = -EFAULT;
			//}
		//}
		/*mutex_unlock(&pktbuff_lock);*/
		break;
	case HDMI_IOC_HDCP14_KEY_MODE:
		if (copy_from_user(&hdcp14_key_mode, argp,
				   sizeof(enum hdcp14_key_mode_e))) {
			ret = -EFAULT;
			pr_info("HDMI_IOC_HDCP14_KEY_MODE err\n\n");
			break;
		}
		rx_pr("hdcp1.4 key mode-%d\n", hdcp14_key_mode);
		break;
	case HDMI_IOC_HDCP22_NOT_SUPPORT:
		/* if sysctl can not find the aic tools,
		 * it will inform driver that 2.2 not support via ioctl
		 */
		hdcp22_on = 0;
		if (rx.open_fg)
			rx_send_hpd_pulse();
		else
			hdmirx_wr_dwc(DWC_HDCP22_CONTROL, 2);
		rx_pr("hdcp2.2 not support\n");
		break;
	case HDMI_IOC_GET_AUD_SAD:
		if (!argp)
			return -EINVAL;
		mutex_lock(&devp->rx_lock);
		rx_edid_get_aud_sad(sad_data);
		if (copy_to_user(argp, &sad_data, 30)) {
			pr_err("sad err\n");
			ret = -EFAULT;
			mutex_unlock(&devp->rx_lock);
			break;
		}
		mutex_unlock(&devp->rx_lock);
		break;
	case HDMI_IOC_SET_AUD_SAD:
		if (!argp)
			return -EINVAL;
		mutex_lock(&devp->rx_lock);
		if (copy_from_user(&sad_data, argp, 30)) {
			pr_err("get sad data err\n");
			ret = -EFAULT;
			mutex_unlock(&devp->rx_lock);
			break;
		}
		mutex_unlock(&devp->rx_lock);
		while (sad_data[i * 3] != '\0' && i < 10) {
			len += 3;
			i++;
		}
		if (!rx_edid_set_aud_sad(sad_data, len))
			rx_pr("sad data length err\n");
		break;
	case HDMI_IOC_GET_SPD_SRC_INFO:
		spdpkt = (struct spd_infoframe_st *)&rx_pkt.spd_info;
		if (!argp)
			return -EINVAL;
		mutex_lock(&devp->rx_lock);
		if (copy_to_user(argp, spdpkt, sizeof(struct spd_infoframe_st))) {
			pr_err("spd src info err\n");
			ret = -EFAULT;
			mutex_unlock(&devp->rx_lock);
			break;
		}
		mutex_unlock(&devp->rx_lock);
		break;
	case HDMI_5V_PIN_STATUS:
		pin_status = rx_get_hdmi5v_sts();
		if (!argp)
			return -EINVAL;
		mutex_lock(&devp->rx_lock);
		if (copy_to_user(argp, &pin_status, sizeof(unsigned int))) {
			pr_err("send pin status err\n");
			ret = -EFAULT;
			mutex_unlock(&devp->rx_lock);
			break;
		}
		mutex_unlock(&devp->rx_lock);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
/*
 * hdmirx_compat_ioctl - file operation interface
 */
static long hdmirx_compat_ioctl(struct file *file,
				unsigned int cmd,
				unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = hdmirx_ioctl(file, cmd, arg);
	return ret;
}
#endif

/*
 * hotplug_wait_query - wake up poll process
 */
void hotplug_wait_query(void)
{
	wake_up(&query_wait);
}

/*
 * hdmirx_sts_read - read interface for tvserver
 */
static ssize_t hdmirx_sts_read(struct file *file,
			       char __user *buf,
			       size_t count,
			       loff_t *pos)
{
	int ret = 0;
	int rx_sts;

	/*
	 * sysctl set the pc_mode on/off via UI,tvserver need to know
	 * the status,but it cant get the status from sysctl because of the
	 * limit of the communication from sysctl to tvserver,so it needs
	 * hdmi driver to feedback the pc_mode status to tvserver
	 */

	rx_sts = pwr_sts | (pc_mode_en << 4);

	if (copy_to_user(buf, &rx_sts, sizeof(unsigned int)))
		return -EFAULT;

	return ret;
}

/*
 * hdmirx_sts_poll - poll interface for tvserver
 */
static unsigned int hdmirx_sts_poll(struct file *filp,
				    poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(filp, &query_wait, wait);
	mask |= POLLIN | POLLRDNORM;

	return mask;
}

static const struct file_operations hdmirx_fops = {
	.owner		= THIS_MODULE,
	.open		= hdmirx_open,
	.release	= hdmirx_release,
	.read       = hdmirx_sts_read,
	.poll       = hdmirx_sts_poll,
	.unlocked_ioctl	= hdmirx_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = hdmirx_compat_ioctl,
#endif
};

int rx_pr_buf(char *buf, int len)
{
	unsigned long flags;
	int pos;
	int hdmirx_log_rd_pos_;

	if (hdmirx_log_buf_size == 0)
		return 0;
	spin_lock_irqsave(&rx_pr_lock, flags);
	hdmirx_log_rd_pos_ = hdmirx_log_rd_pos;
	if (hdmirx_log_wr_pos >= hdmirx_log_rd_pos)
		hdmirx_log_rd_pos_ += hdmirx_log_buf_size;
	for (pos = 0;
		pos < len && hdmirx_log_wr_pos < (hdmirx_log_rd_pos_ - 1);
		pos++, hdmirx_log_wr_pos++) {
		if (hdmirx_log_wr_pos >= hdmirx_log_buf_size)
			hdmirx_log_buf[hdmirx_log_wr_pos - hdmirx_log_buf_size] =
				buf[pos];
		else
			hdmirx_log_buf[hdmirx_log_wr_pos] = buf[pos];
	}
	if (hdmirx_log_wr_pos >= hdmirx_log_buf_size)
		hdmirx_log_wr_pos -= hdmirx_log_buf_size;
	spin_unlock_irqrestore(&rx_pr_lock, flags);
	return pos;
}

int rx_pr(const char *fmt, ...)
{
	va_list args;
	int avail = PRINT_TEMP_BUF_SIZE;
	char buf[PRINT_TEMP_BUF_SIZE];
	int pos = 0;
	int len = 0;
	static bool last_break = 1;

	if (last_break == 1 &&
	    strlen(fmt) > 1) {
		strcpy(buf, "[RX]-");
		for (len = 0; len < strlen(fmt); len++)
			if (fmt[len] == '\n')
				pos++;
			else
				break;

		strncpy(buf + 5, fmt + pos, (sizeof(buf) - 5));
	} else {
		strcpy(buf, fmt);
	}
	if (fmt[strlen(fmt) - 1] == '\n')
		last_break = 1;
	else
		last_break = 0;
	if (log_level & LOG_EN) {
		va_start(args, fmt);
		vprintk(buf, args);
		va_end(args);
		return 0;
	}
	if (hdmirx_log_buf_size == 0)
		return 0;

	/* len += snprintf(buf+len, avail-len, "%d:",log_seq++); */
	len += snprintf(buf + len, avail - len, "[%u] ", (unsigned int)jiffies);
	va_start(args, fmt);
	len += vsnprintf(buf + len, avail - len, fmt, args);
	va_end(args);
	if ((avail - len) <= 0)
		buf[PRINT_TEMP_BUF_SIZE - 1] = '\0';

	pos = rx_pr_buf(buf, len);
	return pos;
}

static int log_init(int bufsize)
{
	if (bufsize == 0) {
		if (hdmirx_log_buf) {
			/* kfree(hdmirx_log_buf); */
			hdmirx_log_buf = NULL;
			hdmirx_log_buf_size = 0;
			hdmirx_log_rd_pos = 0;
			hdmirx_log_wr_pos = 0;
		}
	}
	if (bufsize >= 1024 && !hdmirx_log_buf) {
		hdmirx_log_buf_size = 0;
		hdmirx_log_rd_pos = 0;
		hdmirx_log_wr_pos = 0;
		hdmirx_log_buf = kmalloc(bufsize, GFP_KERNEL);
		if (hdmirx_log_buf)
			hdmirx_log_buf_size = bufsize;
	}
	return 0;
}

static ssize_t log_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	unsigned long flags;
	ssize_t read_size = 0;

	if (hdmirx_log_buf_size == 0)
		return 0;
	spin_lock_irqsave(&rx_pr_lock, flags);
	if (hdmirx_log_rd_pos < hdmirx_log_wr_pos)
		read_size = hdmirx_log_wr_pos - hdmirx_log_rd_pos;
	else if (hdmirx_log_rd_pos > hdmirx_log_wr_pos)
		read_size = hdmirx_log_buf_size - hdmirx_log_rd_pos;

	if (read_size > PAGE_SIZE)
		read_size = PAGE_SIZE;
	if (read_size > 0)
		memcpy(buf, hdmirx_log_buf + hdmirx_log_rd_pos, read_size);
	hdmirx_log_rd_pos += read_size;
	if (hdmirx_log_rd_pos >= hdmirx_log_buf_size)
		hdmirx_log_rd_pos = 0;
	spin_unlock_irqrestore(&rx_pr_lock, flags);
	return read_size;
}

static ssize_t log_store(struct device *dev,
			 struct device_attribute *attr,
			 const char *buf, size_t count)
{
	long tmp;
	unsigned long flags;

	if (strncmp(buf, "bufsize", 7) == 0) {
		if (kstrtoul(buf + 7, 10, &tmp) < 0)
			return -EINVAL;
		spin_lock_irqsave(&rx_pr_lock, flags);
		log_init(tmp);
		spin_unlock_irqrestore(&rx_pr_lock, flags);
		rx_pr("hdmirx_store:set bufsize tmp %ld %d\n",
		      tmp, hdmirx_log_buf_size);
	} else {
		rx_pr(0, "%s", buf);
	}
	return 16;
}

static ssize_t debug_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	return 0;
}

static ssize_t debug_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf,
				  size_t count)
{
	hdmirx_debug(buf, count);
	return count;
}

static ssize_t edid_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	return hdmirx_read_edid_buf(buf, MAX_EDID_BUF_SIZE);
}

static ssize_t edid_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf,
				 size_t count)
{
	hdmirx_fill_edid_buf(buf, count);
	return count;
}

static ssize_t key_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	return 0;/*hdmirx_read_key_buf(buf, PAGE_SIZE);*/
}

static ssize_t key_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t count)
{
	hdmirx_fill_key_buf(buf, count);
	return count;
}

static ssize_t reg_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return hdmirx_hw_dump_reg(buf, PAGE_SIZE);
}

static ssize_t hdcp_version_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	return sprintf(buf, "%x\n", rx.hdcp.hdcp_version);
}

static ssize_t hdcp_version_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf,
				  size_t count)
{
	return count;
}

static ssize_t hdcp14_onoff_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	int pos = 0;

	if (rx.chip_id >= CHIP_ID_T7)
		;//TODO
	else
		return sprintf(buf, "%s", (hdmirx_rd_dwc(DWC_HDCP_BKSV0) ? "1" : "0"));
	return pos;
}

static ssize_t hdcp22_onoff_show(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	int pos = 0;

	pos += sprintf(buf, "%d", hdcp22_on);
	return pos;
}

static ssize_t hw_info_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	struct hdcp_hw_info_s info;

	memset(&info, 0, sizeof(info));
	info.cur_5v = rx.cur_5v_sts;
	info.open = rx.open_fg;
	info.frame_rate = rx.pre.frame_rate / 100;
	info.signal_stable = ((rx.state == FSM_SIG_READY) ? 1 : 0);
	return sprintf(buf, "%x\n", *((unsigned int *)&info));
}

static ssize_t hw_info_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf,
			     size_t count)
{
	return count;
}

//static ssize_t edid_dw_show(struct device *dev,
//			    struct device_attribute *attr,
//			    char *buf)
//{
//	return 0;
//}

//static ssize_t edid_dw_store(struct device *dev,
//			     struct device_attribute *attr,
//			     const char *buf,
///			     size_t count)
//{
//	int cnt = count;

//	rx_pr("edid store len: %d\n", cnt);
//	rx_set_receiver_edid(buf, cnt);

//	return count;
//}

static ssize_t edid_with_port_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return 0;
}

static ssize_t edid_with_port_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	hdmirx_fill_edid_with_port_buf(buf, count);
	return count;
}

/*************************************
 *  val == 0 : cec disable
 *  val == 1 : cec on
 *  val == 2 : cec on && system startup
 **************************************/
static ssize_t cec_store(struct device *dev,
			 struct device_attribute *attr,
			 const char *buf,
			 size_t count)
{
	int cnt, val;

	/* cnt = sscanf(buf, "%x", &val); */
	cnt = kstrtoint(buf, 0, &val);
	if (cnt < 0 || val > 0xff)
		return -EINVAL;
	/* val: 0xAB
	 * A: tv_auto_power_on, B: hdmi_cec_en
	 */
	if ((val & 0xF) == 0) {
		hdmi_cec_en = 0;
		/* fix source can't get edid if cec off */
		if (rx.boot_flag) {
			if (hpd_low_cec_off == 0)
				rx_force_hpd_rxsense_cfg(1);
		}
	} else if ((val & 0xF) == 1) {
		hdmi_cec_en = 1;
	} else if ((val & 0xF) == 2) {
		hdmi_cec_en = 1;
		rx_force_hpd_rxsense_cfg(1);
	}
	rx.boot_flag = false;
	tv_auto_power_on = (val >> 4) & 0xF;
	rx_pr("cec sts = %d\n", val);
	return count;
}

static ssize_t cec_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	return 0;
}

static ssize_t param_show(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	rx_get_global_variable(buf);
	return 0;
}

static ssize_t param_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf,
			   size_t count)
{
	rx_set_global_variable(buf, count);
	return count;
}

static ssize_t esm_base_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	int pos = 0;

	pos += snprintf(buf, PAGE_SIZE, "0x%x\n",
			rx_reg_maps[MAP_ADDR_MODULE_HDMIRX_CAPB3].phy_addr);
	rx_pr("hdcp_rx22 esm base:%#x\n",
	      rx_reg_maps[MAP_ADDR_MODULE_HDMIRX_CAPB3].phy_addr);

	return pos;
}

static ssize_t esm_base_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf,
			      size_t count)
{
	return count;
}

static ssize_t info_show(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
	return hdmirx_show_info(buf, PAGE_SIZE);
}

static ssize_t info_store(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf,
			  size_t count)
{
	return count;
}

static ssize_t arc_aud_type_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf,
				  size_t count)
{
	rx_parse_arc_aud_type(buf);
	return count;
}

static ssize_t arc_aud_type_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	return 0;
}

static ssize_t reset22_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf,
				  size_t count)
{
	return count;
}

static ssize_t reset22_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	return 0;
}

static ssize_t earc_cap_ds_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	return 0;
}

static ssize_t earc_cap_ds_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	unsigned char char_len = 0;
	unsigned int data = 0;
	unsigned char i = 0;
	unsigned char tmp[3] = {0};
	unsigned char earc_cap_ds[EARC_CAP_DS_MAX_LENGTH] = {0};
	int ret = 0;

	char_len = strlen(buf);
	rx_pr("character length = %d\n", char_len);
	for (i = 0; i < char_len / 2; i++) {
		tmp[2] = '\0';
		memcpy(tmp, buf + 2 * i, 2);
		ret = kstrtouint(tmp, 16, &data);
		if (ret < 0) {
			rx_pr("kstrtouint failed\n");
			return count;
		}
		earc_cap_ds[i] = data;
	}
	rx_pr("eARC cap ds len: %d\n", i);
	rx_set_earc_cap_ds(earc_cap_ds, i);

	return count;
}

static ssize_t edid_select_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	return sprintf(buf, "edid select for portD~A: 0x%x\n",
		edid_select);
}

static ssize_t edid_select_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf,
				 size_t count)
{
	int ret;
	unsigned int tmp;
	int i;
	/* PCB port number for UI HDMI1/2/3/4 */
	unsigned char pos[E_PORT_NUM] = {0};

	/* edid selection for UI HDMI4/3/2/1, eg 0x0120
	 * value 2: auto, 1: EDID2.0, 0: EDID1.4
	 */
	ret = kstrtouint(buf, 16, &tmp);
	if (ret)
		return -EINVAL;

	for (i = 0; i < E_PORT_NUM; i++) {
		switch ((port_map >> (i * 4)) & 0xF) {
		case 1:
			pos[0] = i;
			break;
		case 2:
			pos[1] = i;
			break;
		case 3:
			pos[2] = i;
			break;
		case 4:
			pos[3] = i;
			break;
		default:
			break;
		}
	}
	/* edid select for portD/C/B/A */
	edid_select = ((tmp & 0xF) << (pos[0] * 4)) |
		(((tmp >> 4) & 0xF) << (pos[1] * 4)) |
		(((tmp >> 8) & 0xF) << (pos[2] * 4)) |
		(((tmp >> 12) & 0xF) << (pos[3] * 4));
	rx_pr("edid select for UI HDMI4~1: 0x%x, for portD~A: 0x%x\n",
	      tmp, edid_select);
	return count;
}

static ssize_t vrr_func_ctrl_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	return sprintf(buf, "vrr func: %d\n", vrr_func_en);
}

static ssize_t vrr_func_ctrl_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf,
				 size_t count)
{
	int ret;
	unsigned int tmp;

	ret = kstrtouint(buf, 16, &tmp);
	if (ret)
		return -EINVAL;

	vrr_func_en = tmp;
	rx_pr("set vrr_func_en to: %d\n", vrr_func_en);
	return count;
}

static ssize_t audio_blk_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	int i;
	ssize_t len = 0;

	len += sprintf(buf, "audio_blk = {");
	for (i = 0; i < rx_audio_block_len; i++)
		len += sprintf(buf + len, "%02x ", rx_audio_block[i]);
	len += sprintf(buf + len, "}\n");
	return len;
}

static ssize_t audio_blk_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf,
			       size_t count)
{
	int cnt = count;

	if (cnt > MAX_AUDIO_BLK_LEN)
		cnt = MAX_AUDIO_BLK_LEN;
	rx_pr("audio_blk store len: %d\n", cnt);
	memcpy(rx_audio_block, buf, cnt);
	rx_audio_block_len = cnt;

	return count;
}

static ssize_t mode_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	ssize_t len = 0;

	if (abs(rx.cur.hactive - 3840) < 5 && abs(rx.cur.vactive - 2160) < 5) {
		len += snprintf(buf + len, PAGE_SIZE, "2160");
	} else if (abs(rx.cur.hactive - 1920) < 5 && abs(rx.cur.vactive - 2160) < 5 &&
			   rx.cur.colorspace == E_COLOR_YUV420) {
		len += snprintf(buf + len, PAGE_SIZE, "2160");
	} else if (abs(rx.cur.hactive - 1920) < 5 && abs(rx.cur.vactive - 1080) < 5) {
		len += snprintf(buf + len, PAGE_SIZE, "1080");
	} else if (abs(rx.cur.hactive - 1280) < 5 && abs(rx.cur.vactive - 720) < 5) {
		len += snprintf(buf + len, PAGE_SIZE, "720");
	} else if (abs(rx.cur.hactive - 720) < 5 && abs(rx.cur.vactive - 480) < 5) {
		len += snprintf(buf + len, PAGE_SIZE, "480");
	} else if (abs(rx.cur.hactive - 720) < 5 && abs(rx.cur.vactive - 576) < 5) {
		len += snprintf(buf + len, PAGE_SIZE, "576");
	} else if ((rx.cur.hactive != 0) && (rx.cur.vactive != 0)) {
		len += snprintf(buf + len, PAGE_SIZE, "%dx%d",
		rx.cur.hactive, rx.cur.vactive);
	} else {
		len += snprintf(buf + len, PAGE_SIZE, "invalid");
		return len;
	}
	len += snprintf(buf + len, PAGE_SIZE, "%s",
			(rx.cur.interlaced) ? "i" : "p");
	len += snprintf(buf + len, PAGE_SIZE, "%dhz",
			(rx.cur.frame_rate + 50) / 100);

	return len;
}

static ssize_t colorspace_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	ssize_t len = 0;

	if (rx.cur.colorspace == E_COLOR_RGB)
		len += snprintf(buf + len, PAGE_SIZE, "rgb");
	else if (rx.cur.colorspace == E_COLOR_YUV422)
		len += snprintf(buf + len, PAGE_SIZE, "422");
	else if (rx.cur.colorspace == E_COLOR_YUV444)
		len += snprintf(buf + len, PAGE_SIZE, "444");
	else if (rx.cur.colorspace == E_COLOR_YUV420)
		len += snprintf(buf + len, PAGE_SIZE, "420");
	else
		len += snprintf(buf + len, PAGE_SIZE, "invalid\n");

	return len;
}

static ssize_t colordepth_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE, "%d", rx.cur.colordepth);

	return len;
}

static ssize_t frac_mode_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	ssize_t len = 0;

	if (rx.cur.frame_rate && (rx.cur.frame_rate % 100 == 0))
		len += snprintf(buf + len, PAGE_SIZE, "0");
	else
		len += snprintf(buf + len, PAGE_SIZE, "1");

	return len;
}

static ssize_t hdmi_hdr_status_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	ssize_t len = 0;
	struct drm_infoframe_st *drmpkt;

	drmpkt = (struct drm_infoframe_st *)&rx_pkt.drm_info;

	if (rx.vs_info_details.dolby_vision_flag == 1) {
		if (rx.vs_info_details.low_latency)
			len += snprintf(buf + len, PAGE_SIZE, "DolbyVision-Lowlatency");
		else
			len += snprintf(buf + len, PAGE_SIZE, "DolbyVision-Std");
	} else if (rx.vs_info_details.hdr10plus != 0) {
		len += snprintf(buf + len, PAGE_SIZE, "HDR10Plus");
	} else if (drmpkt->des_u.tp1.eotf == EOTF_SDR) {
		len += snprintf(buf + len, PAGE_SIZE, "SDR");
	} else if (drmpkt->des_u.tp1.eotf == EOTF_HDR) {
		len += snprintf(buf + len, PAGE_SIZE, "HDR");
	} else if (drmpkt->des_u.tp1.eotf == EOTF_SMPTE_ST_2048) {
		len += snprintf(buf + len, PAGE_SIZE, "HDR10-GAMMA_ST2084");
	} else if (drmpkt->des_u.tp1.eotf == EOTF_HLG) {
		len += snprintf(buf + len, PAGE_SIZE, "HDR10-GAMMA_HLG");
	} else {
		len += snprintf(buf + len, PAGE_SIZE, "SDR");
	}
	return len;
}

static ssize_t hdcp_auth_sts_show(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	int pos = 0;

	pos += sprintf(buf, "%d", rx_get_hdcp_auth_sts());
	return pos;
}

static DEVICE_ATTR_RW(debug);
static DEVICE_ATTR_RW(edid);
static DEVICE_ATTR_RW(key);
static DEVICE_ATTR_RW(log);
static DEVICE_ATTR_RO(reg);
static DEVICE_ATTR_RW(cec);
static DEVICE_ATTR_RW(param);
static DEVICE_ATTR_RW(esm_base);
static DEVICE_ATTR_RW(info);
static DEVICE_ATTR_RW(arc_aud_type);
static DEVICE_ATTR_RW(reset22);
static DEVICE_ATTR_RW(hdcp_version);
static DEVICE_ATTR_RW(hw_info);
//static DEVICE_ATTR_RW(edid_dw);
static DEVICE_ATTR_RW(earc_cap_ds);
static DEVICE_ATTR_RW(edid_select);
static DEVICE_ATTR_RW(audio_blk);
static DEVICE_ATTR_RO(scan_mode);
static DEVICE_ATTR_RW(edid_with_port);
static DEVICE_ATTR_RW(vrr_func_ctrl);
static DEVICE_ATTR_RO(hdcp14_onoff);
static DEVICE_ATTR_RO(hdcp22_onoff);
static DEVICE_ATTR_RO(mode);
static DEVICE_ATTR_RO(colorspace);
static DEVICE_ATTR_RO(colordepth);
static DEVICE_ATTR_RO(frac_mode);
static DEVICE_ATTR_RO(hdmi_hdr_status);
static DEVICE_ATTR_RO(hdcp_auth_sts);

static int hdmirx_add_cdev(struct cdev *cdevp,
			   const struct file_operations *fops,
			   int minor)
{
	int ret;
	dev_t devno = MKDEV(MAJOR(hdmirx_devno), minor);

	cdev_init(cdevp, fops);
	cdevp->owner = THIS_MODULE;
	ret = cdev_add(cdevp, devno, 1);
	return ret;
}

static struct device *hdmirx_create_device(struct device *parent, int id)
{
	dev_t devno = MKDEV(MAJOR(hdmirx_devno),  id);

	return device_create(hdmirx_clsp, parent, devno, NULL, "%s0",
			TVHDMI_DEVICE_NAME);
	/* @to do this after Middleware API modified */
	/*return device_create(hdmirx_clsp, parent, devno, NULL, "%s",*/
	  /*TV_HDMI_DEVICE_NAME); */
}

static void hdmirx_delete_device(int minor)
{
	dev_t devno = MKDEV(MAJOR(hdmirx_devno), minor);

	device_destroy(hdmirx_clsp, devno);
}

static void hdmirx_get_base_addr(struct device_node *node)
{
	int ret;

	/*get base addr from dts*/
	hdmirx_addr_port = rx_reg_maps[MAP_ADDR_MODULE_TOP].phy_addr;
	if (node) {
		if (hdmirx_addr_port == 0) {
			ret = of_property_read_u32(node,
						   "hdmirx_addr_port",
						   &hdmirx_addr_port);
			if (ret)
				pr_err("get hdmirx_addr_port fail.\n");

			ret = of_property_read_u32(node,
						   "hdmirx_data_port",
						   &hdmirx_data_port);
			if (ret)
				pr_err("get hdmirx_data_port fail.\n");
			ret = of_property_read_u32(node,
						   "hdmirx_ctrl_port",
						   &hdmirx_ctrl_port);
			if (ret)
				pr_err("get hdmirx_ctrl_port fail.\n");
		} else {
			hdmirx_data_port = hdmirx_addr_port + 4;
			hdmirx_ctrl_port = hdmirx_addr_port + 8;
		}
		/* rx_pr("port addr:%#x ,data:%#x, ctrl:%#x\n", */
			/* hdmirx_addr_port, */
			/* hdmirx_data_port, hdmirx_ctrl_port); */
	}
}

static int hdmirx_switch_pinmux(struct device *dev)
{
	struct pinctrl *pin;
	const char *pin_name;
	int ret = 0;

	/* pinmux set */
	if (dev->of_node) {
		ret = of_property_read_string_index(dev->of_node,
						    "pinctrl-names",
						    0, &pin_name);
		if (!ret)
			pin = devm_pinctrl_get_select(dev, pin_name);
			/* rx_pr("hdmirx: pinmux:%p, name:%s\n", */
			/* pin, pin_name); */
	}
	return ret;
}

static void rx_phy_suspend(void)
{
	/* set HPD low when cec off or TV auto power on disabled. */
	if (!hdmi_cec_en || !tv_auto_power_on)
		rx_set_port_hpd(ALL_PORTS, 0);
	if (suspend_pddq_sel == 0) {
		rx_pr("don't set phy pddq down\n");
	} else {
		/* there's no SDA low issue on MTK box when hpd low */
		if (hdmi_cec_en && tv_auto_power_on) {
			if (suspend_pddq_sel == 2) {
				/* set rxsense pulse */
				rx_phy_rxsense_pulse(10, 10, 0);
			}
		}
		/* phy powerdown */
		rx_phy_power_on(0);
	}
	hdmirx_top_irq_en(0, 0);
}

static void rx_phy_resume(void)
{
	/* set below rxsense pulse only if hpd = high,
	 * there's no SDA low issue on MTK box when hpd low
	 */
	if (hdmi_cec_en && tv_auto_power_on) {
		if (suspend_pddq_sel == 1) {
			/* set rxsense pulse, if delay time between
			 * rxsense pulse and phy_int shottern than
			 * 50ms, SDA may be pulled low 800ms on MTK box
			 */
			rx_phy_rxsense_pulse(10, 50, 1);
		}
	}
	if (rx.phy_ver >= PHY_VER_TM2)
		rx.aml_phy.pre_int = 1;
	hdmirx_phy_init();
	pre_port = 0xff;
	rx.boot_flag = true;
	//hdmirx_top_irq_en(1, 2);
}

void rx_emp_resource_allocate(struct device *dev)
{
	if (rx.chip_id >= CHIP_ID_TL1) {
		/* allocate buffer */
		if (!rx.emp_buff.store_a)
			rx.emp_buff.store_a =
				kmalloc(EMP_BUFFER_SIZE, GFP_KERNEL);
		else
			rx_pr("malloc emp buffer err\n");

		if (rx.emp_buff.store_a)
			rx.emp_buff.store_b =
				rx.emp_buff.store_a + (EMP_BUFFER_SIZE >> 1);
		else
			rx_pr("emp buff err-0\n");
		rx_pr("pkt_buffa=0x%p\n", rx.emp_buff.store_a);
		rx_pr("pkt_buffb=0x%p\n", rx.emp_buff.store_b);
		rx.emp_buff.dump_mode = DUMP_MODE_EMP;
		/* allocate buffer for hw access*/
		rx.emp_buff.pg_addr =
			dma_alloc_from_contiguous(dev,
						  EMP_BUFFER_SIZE >> PAGE_SHIFT, 0, 0);
		if (rx.emp_buff.pg_addr) {
			/* hw access */
			/* page to real physical address*/
			rx.emp_buff.p_addr_a =
				page_to_phys(rx.emp_buff.pg_addr);
			rx.emp_buff.p_addr_b =
				rx.emp_buff.p_addr_a + (EMP_BUFFER_SIZE >> 1);
			//page_address
			rx_pr("buff_a paddr=0x%p\n",
			      (void *)rx.emp_buff.p_addr_a);
			rx_pr("buff_b paddr=0x%p\n",
			      (void *)rx.emp_buff.p_addr_b);
		} else {
			rx_pr("emp buff err-1\n");
		}
		rx.emp_buff.emp_pkt_cnt = 0;
	}
}

int rx_hdcp22_send_uevent(int val)
{
	char env[MAX_UEVENT_LEN];
	char *envp[2];
	int ret = 0;
	static int val_pre = 2;

	if (val == val_pre)
		return ret;
	val_pre = val;
	if (log_level & HDCP_LOG)
		rx_pr("rx22 new event-%d\n", val);
	if (!hdmirx_dev) {
		rx_pr("no hdmirx_dev\n");
		return -1;
	}

	memset(env, 0, sizeof(env));
	envp[0] = env;
	envp[1] = NULL;
	snprintf(env, MAX_UEVENT_LEN, "rx22=%d", val);

	ret = kobject_uevent_env(&hdmirx_dev->kobj, KOBJ_CHANGE, envp);
	return ret;
}
void rx_tmds_resource_allocate(struct device *dev)
{
	/*u32 *src_v_addr;*/
	/*u32 *temp;*/
	/*u32 i, j;*/
	/*phys_addr_t p_addr;*/
	/*struct page *pg_addr;*/

	if (rx.chip_id >= CHIP_ID_TL1) {
		if (rx.emp_buff.dump_mode == DUMP_MODE_EMP) {
			if (rx.emp_buff.pg_addr) {
				dma_release_from_contiguous(dev,
							    rx.emp_buff.pg_addr,
							    EMP_BUFFER_SIZE >> PAGE_SHIFT);
				/*free_reserved_area();*/
				rx.emp_buff.pg_addr = 0;
				rx_pr("release emp data buffer\n");
			}
		} else {
			if (rx.emp_buff.pg_addr)
				dma_release_from_contiguous(dev,
							    rx.emp_buff.pg_addr,
							    TMDS_BUFFER_SIZE >> PAGE_SHIFT);
			rx.emp_buff.pg_addr = 0;
			rx_pr("release pre tmds data buffer\n");
		}

		/* allocate tmds data buffer */
		rx.emp_buff.pg_addr =
			dma_alloc_from_contiguous(dev, TMDS_BUFFER_SIZE >> PAGE_SHIFT, 0, 0);

		if (rx.emp_buff.pg_addr)
			rx.emp_buff.p_addr_a =
				page_to_phys(rx.emp_buff.pg_addr);
		else
			rx_pr("allocate tmds data buff fail\n");
		rx.emp_buff.dump_mode = DUMP_MODE_TMDS;
		rx_pr("buff_a paddr=0x%p\n", (void *)rx.emp_buff.p_addr_a);
	}
}

/*
 * capture emp pkt data save file emp_data.bin
 *
 */
void rx_emp_data_capture(void)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buf = NULL;
	char *path = "/data/emp_data.bin";
	static unsigned int offset;
	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR | O_CREAT, 0666);

	if (IS_ERR(filp)) {
		rx_pr("create %s error.\n", path);
		return;
	}

	pos += offset;
	/*start buffer address*/
	buf = rx.emp_buff.ready;
	/*write size*/
	offset = rx.emp_buff.emp_pkt_cnt * 32;
	vfs_write(filp, buf, offset, &pos);
	rx_pr("write from 0x%x to 0x%x to %s.\n",
	      0, 0 + offset, path);
	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);
}

/*
 * capture tmds data save file emp_data.bin
 *
 */
void rx_tmds_data_capture(void)
{
	/* data to terminal or save a file */
	struct file *filp = NULL;
	loff_t pos = 0;
	char *path = "/data/tmds_data.bin";
	unsigned int offset = 0;
	char *src_v_addr;
	mm_segment_t old_fs = get_fs();
	unsigned int recv_pagenum = 0, i, j;
	unsigned int recv_byte_cnt;
	struct page *pg_addr;
	phys_addr_t p_addr;
	char *tmp_buff;
	unsigned int *paddr;

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR | O_CREAT, 0666);

	if (IS_ERR(filp)) {
		pr_info("create %s error.\n", path);
		return;
	}

	tmp_buff = kmalloc(PAGE_SIZE + 16, GFP_KERNEL);
	if (!tmp_buff) {
		rx_pr("tmds malloc buffer err\n");
		return;
	}
	memset(tmp_buff, 0, PAGE_SIZE);
	recv_byte_cnt = rx.emp_buff.tmds_pkt_cnt * 4;
	recv_pagenum = (recv_byte_cnt >> PAGE_SHIFT) + 1;

	rx_pr("total byte:%d page:%d\n", recv_byte_cnt, recv_pagenum);
	for (i = 0; i < recv_pagenum; i++) {
		/* one page 4k,tmds data physical address, need map v addr */
		p_addr = rx.emp_buff.p_addr_a + i * PAGE_SIZE;
		pg_addr = phys_to_page(p_addr);
		src_v_addr = kmap(pg_addr);
		dma_sync_single_for_cpu(hdmirx_dev,
					p_addr,
					PAGE_SIZE,
					DMA_TO_DEVICE);
		pos = i * PAGE_SIZE;
		if (recv_byte_cnt >= PAGE_SIZE) {
			offset = PAGE_SIZE;
			memcpy(tmp_buff, src_v_addr, PAGE_SIZE);
			vfs_write(filp, tmp_buff, offset, &pos);
			recv_byte_cnt -= PAGE_SIZE;
		} else {
			offset = recv_byte_cnt;
			memcpy(tmp_buff, src_v_addr, recv_byte_cnt);
			vfs_write(filp, tmp_buff, offset, &pos);
			recv_byte_cnt = 0;
		}

		/* release current page */
		kunmap(pg_addr);
	}

	/* for test */
	for (i = 0; i < recv_pagenum; i++) {
		p_addr = rx.emp_buff.p_addr_a + i * PAGE_SIZE;
		pg_addr = phys_to_page(p_addr);
		/* p addr map to v addr*/
		paddr = kmap(pg_addr);
		for (j = 0; j < PAGE_SIZE;) {
			*paddr = 0xaabbccdd;
			paddr++;
			j += 4;
		}
		rx_pr(".");
		dma_sync_single_for_device(hdmirx_dev,
					   p_addr,
					   PAGE_SIZE,
					   DMA_TO_DEVICE);
		/* release current page */
		kunmap(pg_addr);
	}

	kfree(tmp_buff);
	rx_pr("write to %s\n", path);
	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);
}

#ifdef CONFIG_AMLOGIC_MEDIA_VRR
static int rx_vrr_notify_handler(struct notifier_block *nb,
				     unsigned long value, void *p)
{
	int ret = 0;
	struct vrr_notifier_data_s vdata;

	switch (value) {
	case VRR_EVENT_UPDATE:
		memcpy(&vdata, p, sizeof(struct vrr_notifier_data_s));
		rx.vrr_min = vdata.dev_vfreq_min;
		rx.vrr_max = vdata.dev_vfreq_max;
		rx_pr("%s: vrr_min=%d, vrr_max=%d\n", __func__, rx.vrr_min, rx.vrr_max);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
static void hdmirx_early_suspend(struct early_suspend *h)
{
	if (early_suspend_flag)
		return;

	early_suspend_flag = true;
	rx_irq_en(false);
	rx_phy_suspend();
	rx_pr("%s- ok\n", __func__);
}

static void hdmirx_late_resume(struct early_suspend *h)
{
	if (!early_suspend_flag)
		return;

	early_suspend_flag = false;
	rx_phy_resume();
	rx_pr("%s- ok\n", __func__);
};

static struct early_suspend hdmirx_early_suspend_handler = {
	/* .level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 10, */
	.suspend = hdmirx_early_suspend,
	.resume = hdmirx_late_resume,
	/* .param = &hdmirx_device, */
};
#endif

static int hdmirx_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct hdmirx_dev_s *hdevp;
	struct resource *res;
	struct clk *xtal_clk;
	struct clk *fclk_div3_clk;
	struct clk *fclk_div4_clk;
	struct clk *fclk_div5_clk;
	struct clk *fclk_div7_clk;
	int clk_rate;
	const struct of_device_id *of_id;
	int disable_port;
	struct input_dev *input_dev;

	log_init(DEF_LOG_BUF_SIZE);
	hdmirx_dev = &pdev->dev;
	/*get compatible matched device, to get chip related data*/
	of_id = of_match_device(hdmirx_dt_match, &pdev->dev);
	if (!of_id) {
		rx_pr("unable to get matched device\n");
		return -1;
	}
	/* allocate memory for the per-device structure */
	hdevp = kmalloc(sizeof(*hdevp), GFP_KERNEL);
	if (!hdevp) {
		rx_pr("hdmirx:allocate memory failed\n");
		ret = -ENOMEM;
		goto fail_kmalloc_hdev;
	}
	memset(hdevp, 0, sizeof(struct hdmirx_dev_s));
	hdevp->data = of_id->data;
	rx.hdmirx_dev = hdevp;

	if (hdevp->data) {
		rx.chip_id = hdevp->data->chip_id;
		rx.phy_ver = hdevp->data->phy_ver;
		rx_pr("chip id:%d\n", rx.chip_id);
		rx_pr("phy ver:%d\n", rx.hdmirx_dev->data->phy_ver);
	} else {
		/*txlx chip for default*/
		rx.chip_id = CHIP_ID_TXLX;
		rx_pr("err: hdevp->data null\n");
	}

	ret = rx_init_reg_map(pdev);
	if (ret < 0) {
		rx_pr("failed to ioremap\n");
		/*goto fail_ioremap;*/
	}
	hdmirx_get_base_addr(pdev->dev.of_node);
	hdevp->index = 0; /* pdev->id; */
	/* create cdev and register with sysfs */
	ret = hdmirx_add_cdev(&hdevp->cdev, &hdmirx_fops, hdevp->index);
	if (ret) {
		rx_pr("%s: failed to add cdev\n", __func__);
		goto fail_add_cdev;
	}
	/* create /dev nodes */
	hdevp->dev = hdmirx_create_device(&pdev->dev, hdevp->index);
	if (IS_ERR(hdevp->dev)) {
		rx_pr("hdmirx: failed to create device node\n");
		ret = PTR_ERR(hdevp->dev);
		goto fail_create_device;
	}
	/*create sysfs attribute files*/
	ret = device_create_file(hdevp->dev, &dev_attr_debug);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create debug attribute file\n");
		goto fail_create_debug_file;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_edid);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create edid attribute file\n");
		goto fail_create_edid_file;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_edid_with_port);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create edid_with_port attribute file\n");
		goto fail_create_edid_with_port_file;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_key);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create key attribute file\n");
		goto fail_create_key_file;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_log);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create log attribute file\n");
		goto fail_create_log_file;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_reg);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create reg attribute file\n");
		goto fail_create_reg_file;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_param);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create param attribute file\n");
		goto fail_create_param_file;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_info);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create info attribute file\n");
		goto fail_create_info_file;
	}
	if (rx.chip_id < CHIP_ID_T7) {
		ret = device_create_file(hdevp->dev, &dev_attr_esm_base);
		if (ret < 0) {
			rx_pr("hdmirx: fail to create esm_base attribute file\n");
			goto fail_create_esm_base_file;
		}
	}
	ret = device_create_file(hdevp->dev, &dev_attr_cec);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create cec attribute file\n");
		goto fail_create_cec_file;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_arc_aud_type);
		if (ret < 0) {
			rx_pr("hdmirx: fail to create arc_aud_type file\n");
			goto fail_create_arc_aud_type_file;
		}
	ret = device_create_file(hdevp->dev, &dev_attr_reset22);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create reset22 file\n");
		goto fail_create_reset22;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_hdcp_version);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create hdcp version file\n");
		goto fail_create_hdcp_version;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_hw_info);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create cur 5v file\n");
		goto fail_create_hw_info;
	}
	//ret = device_create_file(hdevp->dev, &dev_attr_edid_dw);
	//if (ret < 0) {
		//rx_pr("hdmirx: fail to create edid_dw file\n");
		//goto fail_create_edid_dw;
	//}
	ret = device_create_file(hdevp->dev, &dev_attr_earc_cap_ds);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create earc_cap_ds file\n");
		goto fail_create_earc_cap_ds;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_edid_select);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create edid_select file\n");
		goto fail_create_edid_select;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_vrr_func_ctrl);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create vrr_func_ctrl file\n");
		goto fail_create_vrr_func_ctrl;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_audio_blk);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create audio_blk file\n");
		goto fail_create_audio_blk;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_scan_mode);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create scan_mode file\n");
		goto fail_create_scan_mode;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_hdcp14_onoff);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create hdcp14_onoff file\n");
		goto fail_create_hdcp14_onoff;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_hdcp22_onoff);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create hdcp22_onoff file\n");
		goto fail_create_hdcp22_onoff;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_mode);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create mode file\n");
		goto fail_create_mode;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_colorspace);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create colorspace file\n");
		goto fail_create_colorspace;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_colordepth);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create colordepth file\n");
		goto fail_create_colordepth;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_frac_mode);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create frac_mode file\n");
		goto fail_create_frac_mode;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_hdmi_hdr_status);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create hdmi_hdr_status file\n");
		goto fail_create_hdmi_hdr_status;
	}
	ret = device_create_file(hdevp->dev, &dev_attr_hdcp_auth_sts);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create hdcp_auth_sts file\n");
		goto fail_create_hdcp_auth_sts;
	}
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		rx_pr("%s: can't get irq resource\n", __func__);
		ret = -ENXIO;
		goto fail_get_resource_irq;
	}
	hdevp->irq = res->start;
	snprintf(hdevp->irq_name, sizeof(hdevp->irq_name),
		 "hdmirx%d-irq", hdevp->index);
	rx_pr("hdevp irq: %d, %d\n", hdevp->index,
	      hdevp->irq);
	if (request_irq(hdevp->irq,
			&irq_handler,
			IRQF_SHARED,
			hdevp->irq_name,
			(void *)&rx))
		rx_pr(__func__, "RX IRQ request");
	/* frontend */
	tvin_frontend_init(&hdevp->frontend,
			   &hdmirx_dec_ops,
			   &hdmirx_sm_ops,
			   hdevp->index);
	sprintf(hdevp->frontend.name, "%s", TVHDMI_NAME);
	if (tvin_reg_frontend(&hdevp->frontend) < 0)
		rx_pr("hdmirx: driver probe error!!!\n");

	dev_set_drvdata(hdevp->dev, hdevp);
	platform_set_drvdata(pdev, hdevp);

	xtal_clk = clk_get(&pdev->dev, "xtal");
	if (IS_ERR(xtal_clk))
		rx_pr("get xtal err\n");
	else
		clk_rate = clk_get_rate(xtal_clk);
	fclk_div3_clk = clk_get(&pdev->dev, "fclk_div3");
	if (IS_ERR(fclk_div3_clk))
		rx_pr("get fclk_div3_clk err\n");
	else
		clk_rate = clk_get_rate(fclk_div3_clk);
	fclk_div5_clk = clk_get(&pdev->dev, "fclk_div5");
	if (IS_ERR(fclk_div5_clk))
		rx_pr("get fclk_div5_clk err\n");
	else
		clk_rate = clk_get_rate(fclk_div5_clk);
	fclk_div7_clk = clk_get(&pdev->dev, "fclk_div7");
	if (IS_ERR(fclk_div7_clk))
		rx_pr("get fclk_div7_clk err\n");
	else
		clk_rate = clk_get_rate(fclk_div7_clk);
	hdevp->modet_clk = clk_get(&pdev->dev, "hdmirx_modet_clk");
	if (IS_ERR(hdevp->modet_clk)) {
		rx_pr("get modet_clk err\n");
	} else {
		/* clk_set_parent(hdevp->modet_clk, xtal_clk); */
		ret = clk_set_rate(hdevp->modet_clk, 24000000);
		if (ret)
			rx_pr("clk_set_rate:modet err-%d\n", __LINE__);
		clk_prepare_enable(hdevp->modet_clk);
		clk_rate = clk_get_rate(hdevp->modet_clk);
	}
	hdevp->cfg_clk = clk_get(&pdev->dev, "hdmirx_cfg_clk");
	if (IS_ERR(hdevp->cfg_clk)) {
		rx_pr("get cfg_clk err\n");
	} else {
		if (rx.chip_id >= CHIP_ID_T7)
			ret = clk_set_rate(hdevp->cfg_clk, 50000000);
		else
			ret = clk_set_rate(hdevp->cfg_clk, 133333333);
		if (ret)
			rx_pr("clk_set_rate:cfg err-%d\n", __LINE__);
		clk_prepare_enable(hdevp->cfg_clk);
		clk_rate = clk_get_rate(hdevp->cfg_clk);
	}
	if (rx.chip_id < CHIP_ID_T7) {
		hdevp->esm_clk = clk_get(&pdev->dev, "hdcp_rx22_esm");
		if (IS_ERR(hdevp->esm_clk)) {
			rx_pr("get esm_clk err\n");
		} else {
			/* clk_set_parent(hdevp->esm_clk, fclk_div7_clk); */
			ret = clk_set_rate(hdevp->esm_clk, 285714285);
			if (ret)
				rx_pr("clk_set_rate:esm err-%d\n", __LINE__);
			clk_prepare_enable(hdevp->esm_clk);
			clk_rate = clk_get_rate(hdevp->esm_clk);
		}
		hdevp->skp_clk = clk_get(&pdev->dev, "hdcp_rx22_skp");
		if (IS_ERR(hdevp->skp_clk)) {
			rx_pr("get skp_clk err\n");
		} else {
			/* clk_set_parent(hdevp->skp_clk, xtal_clk); */
			ret = clk_set_rate(hdevp->skp_clk, 24000000);
			if (ret)
				rx_pr("clk_set_rate:skp err-%d\n", __LINE__);
			clk_prepare_enable(hdevp->skp_clk);
			clk_rate = clk_get_rate(hdevp->skp_clk);
		}
	}
	if (rx.chip_id >= CHIP_ID_TL1) {
		/*for audio clk measure*/
		hdevp->meter_clk = clk_get(&pdev->dev, "cts_hdmirx_meter_clk");
		if (IS_ERR(hdevp->meter_clk)) {
			rx_pr("get cts hdmirx meter clk err\n");
		} else {
			/* clk_set_parent(hdevp->meter_clk, xtal_clk); */
			ret = clk_set_rate(hdevp->meter_clk, 24000000);
			if (ret)
				rx_pr("clk_set_rate:meter err-%d\n", __LINE__);
			clk_prepare_enable(hdevp->meter_clk);
			clk_rate = clk_get_rate(hdevp->meter_clk);
		}
		/*for emp data to ddr*/
		hdevp->axi_clk = clk_get(&pdev->dev, "cts_hdmi_axi_clk");
		if (IS_ERR(hdevp->axi_clk)) {
			rx_pr("get cts axi clk err\n");
		} else {
			clk_set_rate(hdevp->axi_clk, 667000000);
			if (ret)
				rx_pr("clk_set_rate:meter err-%d\n", __LINE__);
			clk_prepare_enable(hdevp->axi_clk);
			clk_rate = clk_get_rate(hdevp->axi_clk);
		}
		/* */
		ret = of_property_read_u32(pdev->dev.of_node,
					   "term_lvl",
					   &phy_term_lel);
		if (ret) {
			rx_pr("term_lvl not found.\n");
			phy_term_lel = 0;
		}
	} else {
		hdevp->audmeas_clk = clk_get(&pdev->dev, "hdmirx_audmeas_clk");
		if (IS_ERR(hdevp->audmeas_clk)) {
			rx_pr("get audmeas_clk err\n");
		} else {
			/* clk_set_parent(hdevp->audmeas_clk, fclk_div5_clk); */
			ret = clk_set_rate(hdevp->audmeas_clk, 200000000);
			if (ret)
				rx_pr("clk_set_rate:audmeas err-%d\n", __LINE__);
			clk_rate = clk_get_rate(hdevp->audmeas_clk);
		}
	}

	if (rx.chip_id >= CHIP_ID_T7) {
		hdevp->cts_hdmirx_5m_clk = clk_get(&pdev->dev, "cts_hdmirx_5m_clk");
		if (IS_ERR(hdevp->cts_hdmirx_5m_clk)) {
			rx_pr("get cts_hdmirx_5m_clk err\n");
		} else {
			/* clk_set_parent(hdevp->meter_clk, xtal_clk); */
			ret = clk_set_rate(hdevp->cts_hdmirx_5m_clk, 5000000);
			if (ret)
				rx_pr("clk_set_rate:meter err-%d\n", __LINE__);
			clk_prepare_enable(hdevp->cts_hdmirx_5m_clk);
			clk_rate = clk_get_rate(hdevp->cts_hdmirx_5m_clk);
		}
		hdevp->cts_hdmirx_2m_clk = clk_get(&pdev->dev, "cts_hdmirx_2m_clk");
		if (IS_ERR(hdevp->cts_hdmirx_2m_clk)) {
			rx_pr("get cts_hdmirx_2m_clk err\n");
		} else {
			/* clk_set_parent(hdevp->meter_clk, xtal_clk); */
			ret = clk_set_rate(hdevp->cts_hdmirx_2m_clk, 2000000);
			if (ret)
				rx_pr("clk_set_rate:meter err-%d\n", __LINE__);
			clk_prepare_enable(hdevp->cts_hdmirx_2m_clk);
			clk_rate = clk_get_rate(hdevp->cts_hdmirx_2m_clk);
		}
		/*for emp data to ddr*/
		hdevp->cts_hdmirx_hdcp2x_eclk = clk_get(&pdev->dev, "cts_hdmirx_hdcp2x_eclk");
		if (IS_ERR(hdevp->cts_hdmirx_hdcp2x_eclk)) {
			rx_pr("get cts_hdmirx_hdcp2x_eclk err\n");
		} else {
			clk_set_rate(hdevp->cts_hdmirx_hdcp2x_eclk, 25000000);
			clk_prepare_enable(hdevp->cts_hdmirx_hdcp2x_eclk);
			clk_rate = clk_get_rate(hdevp->cts_hdmirx_hdcp2x_eclk);
		}
		fclk_div4_clk = clk_get(&pdev->dev, "fclk_div4");
		if (IS_ERR(fclk_div4_clk))
			rx_pr("get fclk_div4_clk err\n");
		else
			clk_rate = clk_get_rate(fclk_div4_clk);
	}
	pd_fifo_buf = kmalloc_array(1, PFIFO_SIZE * sizeof(u32),
				    GFP_KERNEL);
	if (!pd_fifo_buf) {
		rx_pr("hdmirx:allocate pd fifo failed\n");
		ret = -ENOMEM;
		goto fail_kmalloc_pd_fifo;
	}

	tasklet_init(&rx_tasklet, rx_tasklet_handler, (unsigned long)&rx);
	/* create for hot plug function */
	eq_wq = create_singlethread_workqueue(hdevp->frontend.name);
	INIT_DELAYED_WORK(&eq_dwork, eq_dwork_handler);

	if (rx.chip_id < CHIP_ID_T7) {
		esm_wq = create_singlethread_workqueue(hdevp->frontend.name);
		INIT_DELAYED_WORK(&esm_dwork, rx_hpd_to_esm_handle);
		/* queue_delayed_work(eq_wq, &eq_dwork, msecs_to_jiffies(5)); */
	}
	/* create for aml phy init */
	aml_phy_wq = create_workqueue(hdevp->frontend.name);
	INIT_WORK(&aml_phy_dwork, aml_phy_init_handler);

	/* create for clk msr */
	clkmsr_wq = create_workqueue(hdevp->frontend.name);
	INIT_WORK(&clkmsr_dwork, rx_clkmsr_handler);

	/* create for earc hpd ctl */
	earc_hpd_wq = create_workqueue(hdevp->frontend.name);
	INIT_WORK(&earc_hpd_dwork, rx_earc_hpd_handler);

	/*repeater_wq = create_singlethread_workqueue(hdevp->frontend.name);*/
	/*INIT_DELAYED_WORK(&repeater_dwork, repeater_dwork_handle);*/

	ret = of_property_read_u32(pdev->dev.of_node,
				   "ops_port", &ops_port);
	if (ret) {
		rx_pr("ops_port not found.\n");
		ops_port = 0xf0;
	}
	ret = of_property_read_u32(pdev->dev.of_node,
				   "en_4k_2_2k", &en_4k_2_2k);
	if (ret) {
		rx_pr("en_4k_2_2k not found.\n");
		en_4k_2_2k = 0;
	}
	ret = of_property_read_u32(pdev->dev.of_node,
				   "en_4k_timing", &en_4k_timing);
	if (ret)
		en_4k_timing = 1;
	ret = of_property_read_u32(pdev->dev.of_node,
				   "vrr_range_dynamic_update_en", &vrr_range_dynamic_update_en);
	if (ret) {
		vrr_range_dynamic_update_en = 0;
		rx_pr("vrr_range_dynamic_update_en not found.\n");
	}
	ret = of_property_read_u32(pdev->dev.of_node,
				   "hpd_low_cec_off", &hpd_low_cec_off);
	if (ret)
		hpd_low_cec_off = 1;
	ret = of_property_read_u32(pdev->dev.of_node,
				   "disable_port", &disable_port);
	if (ret) {
		/* don't disable port if dts not indicate */
		disable_port_en = 0;
	} else {
		/* bit4: enable feature, bit3~0: port_num */
		disable_port_en = (disable_port >> 4) & 0x1;
		disable_port_num = disable_port & 0xF;
	}
	ret = of_property_read_u32(pdev->dev.of_node,
				   "arc_port", &rx.arc_port);
	if (ret) {
		/* default arc port is port B */
		rx.arc_port = 0x1;
		rx_pr("not find arc_port, portB by default\n");
	}
	ret = of_property_read_u32(pdev->dev.of_node,
				   "hdcp_tee_path",
						&hdcp_tee_path);
	if (ret) {
		hdcp_tee_path = 0;
		rx_pr("not find hdcp_tee_path, hdcp normal path\n");
	}
	/*if (hdcp_tee_path)*/
		/*hdcp22_on = 1;*/
	/*else*/
	rx_is_hdcp22_support();
	ret = of_property_read_u32(pdev->dev.of_node,
				   "aud_compose_type",
				   &aud_compose_type);
	if (ret) {
		aud_compose_type = 1;
		rx_pr("not find aud_compose_type, soundbar by default\n");
	}
	ret = of_property_read_u32(pdev->dev.of_node,
				   "rpt_only_mode",
				   &rpt_only_mode);
	if (ret) {
		rpt_only_mode = 0;
		rx_pr("not find rpt_only_mode, soundbar by default\n");
	}
	ret = of_property_read_u32(pdev->dev.of_node,
				   "disable_hdr",
				   &disable_hdr);
	if (ret) {
		disable_hdr = 0;
		rx_pr("not find disable_hdr, hdr enable by default\n");
	}
	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret != 0)
		rx_pr("warning: no rev cmd mem\n");
	rx_emp_resource_allocate(&pdev->dev);
	rx.port = rx.arc_port;
	aml_phy_get_trim_val();
	edid_auto_mode_init();
	hdmirx_hw_probe();
	if (rx.chip_id >= CHIP_ID_TL1 && phy_tdr_en)
		term_cal_en = (!is_ft_trim_done());
	hdmirx_init_params();
	hdmirx_switch_pinmux(&pdev->dev);
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	register_early_suspend(&hdmirx_early_suspend_handler);
#endif
	mutex_init(&hdevp->rx_lock);
	register_pm_notifier(&aml_hdcp22_pm_notifier);
	/* init_timer(&hdevp->timer); */
	/* hdevp->timer.data = (ulong)hdevp; */
	hdevp->timer.function = hdmirx_timer_handler;
	hdevp->timer.expires = jiffies + TIMER_STATE_CHECK;
	add_timer(&hdevp->timer);
	rx.boot_flag = true;
#ifdef CONFIG_AMLOGIC_HDMITX
	if (rx.chip_id == CHIP_ID_TM2 ||
	    rx.chip_id == CHIP_ID_T7) {
		rx.tx_notify.notifier_call = rx_hdmi_tx_notify_handler;
		hdmitx_event_notifier_regist(&rx.tx_notify);
	}
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_VRR
	if (rx.chip_id >= CHIP_ID_T3) {
		rx.vrr_notify.notifier_call = rx_vrr_notify_handler;
		aml_vrr_atomic_notifier_register(&rx.vrr_notify);
	}
#endif
	input_dev = input_allocate_device();
	if (!input_dev) {
		rx_pr("input_allocate_device failed\n");
	} else {
		/* supported input event type and key */
		set_bit(KEY_POWER, input_dev->keybit);
		set_bit(EV_KEY, input_dev->evbit);
		input_dev->name = "input_hdmirx";
		input_dev->id.bustype = BUS_ISA;
		input_dev->id.vendor = 0x0001;
		input_dev->id.product = 0x0001;
		input_dev->id.version = 0x0001;

		ret = input_register_device(input_dev);
		if (ret < 0) {
			rx_pr("input_register_device failed: %d\n", ret);
			input_free_device(input_dev);
		} else {
			hdevp->hdmirx_input_dev = input_dev;
		}
	}
	rx_pr("hdmirx: driver probe ok\n");
	return 0;
fail_kmalloc_pd_fifo:
	return ret;
fail_get_resource_irq:
	return ret;

fail_create_hdcp_auth_sts:
	device_remove_file(hdevp->dev, &dev_attr_hdcp_auth_sts);
fail_create_hdmi_hdr_status:
	device_remove_file(hdevp->dev, &dev_attr_hdmi_hdr_status);
fail_create_frac_mode:
	device_remove_file(hdevp->dev, &dev_attr_frac_mode);
fail_create_colordepth:
	device_remove_file(hdevp->dev, &dev_attr_colordepth);
fail_create_colorspace:
	device_remove_file(hdevp->dev, &dev_attr_colorspace);
fail_create_mode:
	device_remove_file(hdevp->dev, &dev_attr_mode);
fail_create_hdcp22_onoff:
	device_remove_file(hdevp->dev, &dev_attr_hdcp22_onoff);
fail_create_hdcp14_onoff:
	device_remove_file(hdevp->dev, &dev_attr_hdcp14_onoff);
fail_create_scan_mode:
	device_remove_file(hdevp->dev, &dev_attr_scan_mode);
fail_create_audio_blk:
	device_remove_file(hdevp->dev, &dev_attr_audio_blk);
fail_create_edid_select:
	device_remove_file(hdevp->dev, &dev_attr_edid_select);
fail_create_vrr_func_ctrl:
	device_remove_file(hdevp->dev, &dev_attr_vrr_func_ctrl);
fail_create_earc_cap_ds:
	device_remove_file(hdevp->dev, &dev_attr_earc_cap_ds);
//fail_create_edid_dw:
//	device_remove_file(hdevp->dev, &dev_attr_edid_dw);
fail_create_hw_info:
	device_remove_file(hdevp->dev, &dev_attr_hw_info);
fail_create_hdcp_version:
	device_remove_file(hdevp->dev, &dev_attr_hdcp_version);
fail_create_reset22:
	device_remove_file(hdevp->dev, &dev_attr_reset22);
fail_create_arc_aud_type_file:
	device_remove_file(hdevp->dev, &dev_attr_arc_aud_type);
fail_create_cec_file:
	device_remove_file(hdevp->dev, &dev_attr_cec);
fail_create_esm_base_file:
	device_remove_file(hdevp->dev, &dev_attr_esm_base);
fail_create_info_file:
	device_remove_file(hdevp->dev, &dev_attr_info);
fail_create_param_file:
	device_remove_file(hdevp->dev, &dev_attr_param);
fail_create_reg_file:
	device_remove_file(hdevp->dev, &dev_attr_reg);
fail_create_log_file:
	device_remove_file(hdevp->dev, &dev_attr_log);
fail_create_key_file:
	device_remove_file(hdevp->dev, &dev_attr_key);
fail_create_edid_file:
	device_remove_file(hdevp->dev, &dev_attr_edid);
fail_create_edid_with_port_file:
	device_remove_file(hdevp->dev, &dev_attr_edid_with_port);
fail_create_debug_file:
	device_remove_file(hdevp->dev, &dev_attr_debug);

/* fail_get_resource_irq: */
	/* hdmirx_delete_device(hdevp->index); */
fail_create_device:
	cdev_del(&hdevp->cdev);
fail_add_cdev:
/* fail_get_id: */
	kfree(hdevp);
fail_kmalloc_hdev:
	return ret;
}

static int hdmirx_remove(struct platform_device *pdev)
{
	struct hdmirx_dev_s *hdevp;

	hdevp = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&eq_dwork);
	destroy_workqueue(eq_wq);

	cancel_delayed_work_sync(&esm_dwork);
	destroy_workqueue(esm_wq);

	cancel_work_sync(&aml_phy_dwork);
	destroy_workqueue(aml_phy_wq);
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	unregister_early_suspend(&hdmirx_early_suspend_handler);
#endif
	mutex_destroy(&hdevp->rx_lock);
	device_remove_file(hdevp->dev, &dev_attr_hdcp_auth_sts);
	device_remove_file(hdevp->dev, &dev_attr_hdmi_hdr_status);
	device_remove_file(hdevp->dev, &dev_attr_frac_mode);
	device_remove_file(hdevp->dev, &dev_attr_colordepth);
	device_remove_file(hdevp->dev, &dev_attr_colorspace);
	device_remove_file(hdevp->dev, &dev_attr_mode);
	device_remove_file(hdevp->dev, &dev_attr_hdcp22_onoff);
	device_remove_file(hdevp->dev, &dev_attr_hdcp14_onoff);
	device_remove_file(hdevp->dev, &dev_attr_scan_mode);
	device_remove_file(hdevp->dev, &dev_attr_audio_blk);
	device_remove_file(hdevp->dev, &dev_attr_edid_select);
	device_remove_file(hdevp->dev, &dev_attr_debug);
	device_remove_file(hdevp->dev, &dev_attr_edid);
	device_remove_file(hdevp->dev, &dev_attr_edid_with_port);
	device_remove_file(hdevp->dev, &dev_attr_key);
	device_remove_file(hdevp->dev, &dev_attr_log);
	device_remove_file(hdevp->dev, &dev_attr_reg);
	device_remove_file(hdevp->dev, &dev_attr_cec);
	device_remove_file(hdevp->dev, &dev_attr_esm_base);
	device_remove_file(hdevp->dev, &dev_attr_info);
	device_remove_file(hdevp->dev, &dev_attr_arc_aud_type);
	device_remove_file(hdevp->dev, &dev_attr_earc_cap_ds);
	//device_remove_file(hdevp->dev, &dev_attr_edid_dw);
	device_remove_file(hdevp->dev, &dev_attr_hw_info);
	device_remove_file(hdevp->dev, &dev_attr_hdcp_version);
	device_remove_file(hdevp->dev, &dev_attr_reset22);
	tvin_unreg_frontend(&hdevp->frontend);
	hdmirx_delete_device(hdevp->index);
	tasklet_kill(&rx_tasklet);
	kfree(pd_fifo_buf);
	cdev_del(&hdevp->cdev);
	kfree(hdevp);
	rx_pr("hdmirx: driver removed ok.\n");
	return 0;
}

#ifdef CONFIG_AMLOGIC_HDMITX
int get_tx_boot_hdr_priority(char *str)
{
	unsigned int val = 0;

	if ((strncmp("1", str, 1) == 0) || (strncmp("2", str, 1) == 0)) {
		val = str[0] - '0';
		tx_hdr_priority = val;
		pr_info("tx_hdr_priority: %d\n", val);
	}
	return 0;
}
__setup("hdr_priority=", get_tx_boot_hdr_priority);
#endif

static int aml_hdcp22_pm_notify(struct notifier_block *nb,
				unsigned long event, void *dummy)
{
	int delay = 0;

	if (rx.chip_id >= CHIP_ID_T7)
		return NOTIFY_OK;
	if (event == PM_SUSPEND_PREPARE && hdcp22_on) {
		rx_pr("PM_SUSPEND_PREPARE\n");
		hdcp22_kill_esm = 1;
		/*wait time out ESM_KILL_WAIT_TIMES*20 ms*/
		while (delay++ < ESM_KILL_WAIT_TIMES) {
			if (!hdcp22_kill_esm)
				break;
			msleep(20);
		}
		if (!hdcp22_kill_esm)
			rx_pr("hdcp22 kill ok!\n");
		else
			rx_pr("hdcp22 kill timeout!\n");
		hdcp_22_off();
	} else if ((event == PM_POST_SUSPEND) && hdcp22_on) {
		rx_pr("PM_POST_SUSPEND\n");
		hdcp_22_on();
	}
	return NOTIFY_OK;
}

#ifdef CONFIG_PM
static int hdmirx_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct hdmirx_dev_s *hdevp;

	hdevp = platform_get_drvdata(pdev);
	del_timer_sync(&hdevp->timer);
	hdmirx_top_irq_en(0, 0);
	hdmirx_output_en(false);
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	/* if early suspend not called, need to pw down phy here */
	if (!early_suspend_flag)
#endif
		rx_phy_suspend();
	/*
	 * clk source changed under suspend mode,
	 * div must change together.
	 */
	rx_set_suspend_edid_clk(true);
	rx_dig_clk_en(0);
	rx_pr("hdmirx: suspend success\n");
	return 0;
}

static int hdmirx_resume(struct platform_device *pdev)
{
	struct hdmirx_dev_s *hdevp;

	hdevp = platform_get_drvdata(pdev);
	add_timer(&hdevp->timer);
	rx_dig_clk_en(1);
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	/* if early suspend not called, need to pw up phy here */
	if (!early_suspend_flag)
#endif
		rx_phy_resume();
	rx_set_suspend_edid_clk(false);
	rx_pr("hdmirx: resume\n");
	/* for wakeup by pwr5v pin, only available on T7 for now */
	if (get_resume_method() == HDMI_RX_WAKEUP &&
	    hdevp->hdmirx_input_dev) {
		input_event(hdevp->hdmirx_input_dev,
			EV_KEY, KEY_POWER, 1);
		input_sync(hdevp->hdmirx_input_dev);
		input_event(hdevp->hdmirx_input_dev,
			EV_KEY, KEY_POWER, 0);
		input_sync(hdevp->hdmirx_input_dev);
	}
	return 0;
}
#endif

static void hdmirx_shutdown(struct platform_device *pdev)
{
	struct hdmirx_dev_s *hdevp;

	rx_pr("%s\n", __func__);
	hdevp = platform_get_drvdata(pdev);
	del_timer_sync(&hdevp->timer);
	/* set HPD low when cec off or TV auto power on disabled.*/
	if (!hdmi_cec_en || !tv_auto_power_on)
		rx_set_port_hpd(ALL_PORTS, 0);
	/* phy powerdown */
	rx_phy_power_on(0);
	if (hdcp22_on)
		hdcp_22_off();
	hdmirx_top_irq_en(0, 0);
	hdmirx_output_en(false);
	rx_dig_clk_en(0);
	rx_pr("%s- success\n", __func__);
}

#ifdef CONFIG_PM
static int hdmirx_restore(struct device *dev)
{
	/* queue_delayed_work(eq_wq, &eq_dwork, msecs_to_jiffies(5)); */
	return 0;
}

static int hdmirx_pm_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	return hdmirx_suspend(pdev, PMSG_SUSPEND);
}

static int hdmirx_pm_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	return hdmirx_resume(pdev);
}

const struct dev_pm_ops hdmirx_pm = {
	.restore	= hdmirx_restore,
	.suspend	= hdmirx_pm_suspend,
	.resume		= hdmirx_pm_resume,
};

#endif

static struct platform_driver hdmirx_driver = {
	.probe      = hdmirx_probe,
	.remove     = hdmirx_remove,
#ifdef CONFIG_PM
	.suspend    = hdmirx_suspend,
	.resume     = hdmirx_resume,
#endif
	.shutdown	= hdmirx_shutdown,
	.driver     = {
	.name   = TVHDMI_DRIVER_NAME,
	.owner	= THIS_MODULE,
	.of_match_table = hdmirx_dt_match,
#ifdef CONFIG_PM
	.pm     = &hdmirx_pm,
#endif
	}
};

int __init hdmirx_init(void)
{
	int ret = 0;
	/* struct platform_device *pdev; */

	if (init_flag & INIT_FLAG_NOT_LOAD)
		return 0;

	ret = alloc_chrdev_region(&hdmirx_devno, 0, 1, TVHDMI_NAME);
	if (ret < 0) {
		rx_pr("hdmirx: failed to allocate major number\n");
		goto fail_alloc_cdev_region;
	}

	hdmirx_clsp = class_create(THIS_MODULE, TVHDMI_NAME);
	if (IS_ERR(hdmirx_clsp)) {
		rx_pr("hdmirx: can't get hdmirx_clsp\n");
		ret = PTR_ERR(hdmirx_clsp);
		goto fail_class_create;
	}

	ret = platform_driver_register(&hdmirx_driver);
	if (ret != 0) {
		rx_pr("hdmirx module failed, error %d\n", ret);
		ret = -ENODEV;
		goto fail_pdrv_register;
	}
	rx_pr("%s\n", __func__);

	return 0;

fail_pdrv_register:
	class_destroy(hdmirx_clsp);
fail_class_create:
	unregister_chrdev_region(hdmirx_devno, 1);
fail_alloc_cdev_region:
	return ret;
}

void __exit hdmirx_exit(void)
{
	class_destroy(hdmirx_clsp);
	unregister_chrdev_region(hdmirx_devno, 1);
	platform_driver_unregister(&hdmirx_driver);
	rx_pr("%s\n", __func__);
}

//MODULE_DESCRIPTION("AMLOGIC HDMIRX driver");
//MODULE_LICENSE("GPL");
