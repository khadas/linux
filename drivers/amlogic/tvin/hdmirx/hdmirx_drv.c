/*
 * TVHDMI char device driver for M6TV chip of AMLOGIC INC.
 *
 * Copyright (C) 2012 AMLOGIC, INC. All Rights Reserved.
 * Author: Rain Zhang <rain.zhang@amlogic.com>
 * Author: Xiaofei Zhu <xiaofei.zhu@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
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
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/poll.h>
#include <linux/io.h>

/* Amlogic headers */
#include <linux/amlogic/amports/vframe_provider.h>
#include <linux/amlogic/amports/vframe_receiver.h>
#include <linux/amlogic/tvin/tvin.h>
/* #include <linux/amlogic/amports/canvas.h> */
/* #include <mach/am_regs.h> */
#include <linux/amlogic/amports/vframe.h>
#include <linux/of_gpio.h>

/* Local include */

#include "hdmirx_drv.h"
#include "hdmi_rx_reg.h"
#include "hdmi_rx_eq.h"

#define TVHDMI_NAME				"hdmirx"
#define TVHDMI_DRIVER_NAME		"hdmirx"
#define TVHDMI_MODULE_NAME		"hdmirx"
#define TVHDMI_DEVICE_NAME		"hdmirx"
#define TVHDMI_CLASS_NAME		"hdmirx"
#define INIT_FLAG_NOT_LOAD			0x80

#define HDMI_DE_REPEAT_DONE_FLAG	0xF0

/* 50ms timer for hdmirx main loop (HDMI_STATE_CHECK_FREQ is 20) */
#define TIMER_STATE_CHECK		(1*HZ/HDMI_STATE_CHECK_FREQ)


static unsigned char init_flag;
static dev_t	hdmirx_devno;
static struct class	*hdmirx_clsp;
/* static int open_flage; */
struct hdmirx_dev_s *devp_hdmirx_suspend;
struct device *hdmirx_dev;
struct delayed_work     eq_dwork;
struct workqueue_struct *eq_wq;
struct delayed_work		esm_dwork;
struct workqueue_struct	*esm_wq;
DECLARE_WAIT_QUEUE_HEAD(query_wait);
unsigned int pwr_sts;

int resume_flag = 0;
MODULE_PARM_DESC(resume_flag, "\n resume_flag\n");
module_param(resume_flag, int, 0664);

static int force_colorspace;
MODULE_PARM_DESC(force_colorspace, "\n force_colorspace\n");
module_param(force_colorspace, int, 0664);

static int hdmi_yuv444_enable = 1;
module_param(hdmi_yuv444_enable, int, 0664);
MODULE_PARM_DESC(hdmi_yuv444_enable, "hdmi_yuv444_enable");

static int repeat_function;
MODULE_PARM_DESC(repeat_function, "\n repeat_function\n");
module_param(repeat_function, int, 0664);

bool downstream_repeat_support = 1;
MODULE_PARM_DESC(downstream_repeat_support, "\n downstream_repeat_support\n");
module_param(downstream_repeat_support, bool, 0664);

static int force_color_range;
MODULE_PARM_DESC(force_color_range, "\n force_color_range\n");
module_param(force_color_range, int, 0664);

int pc_mode_en;
MODULE_PARM_DESC(pc_mode_en, "\n pc_mode_en\n");
module_param(pc_mode_en, int, 0664);

bool mute_kill_en;
MODULE_PARM_DESC(mute_kill_en, "\n mute_kill_en\n");
module_param(mute_kill_en, bool, 0664);

static bool en_4096_2_3840 = true;
MODULE_PARM_DESC(en_4096_2_3840, "\n en_4096_2_3840\n");
module_param(en_4096_2_3840, bool, 0664);

static int en_4k_2_2k;
MODULE_PARM_DESC(en_4k_2_2k, "\n en_4k_2_2k\n");
module_param(en_4k_2_2k, int, 0664);

int suspend_pddq = 1;

unsigned int hdmirx_addr_port;
unsigned int hdmirx_data_port;
unsigned int hdmirx_ctrl_port;
/* int en_4k_2_2k; */
struct reg_map {
	unsigned int phy_addr;
	unsigned int size;
	void __iomem *p;
	int flag;
};

static struct reg_map reg_maps[] = {
	{ /* CBUS */
		.phy_addr = 0xc0800000,
		.size = 0xa00000,
	},
	{ /* HIU */
		.phy_addr = 0xC883C000,
		.size = 0x2000,
	},
	{ /* HDMIRX CAPB3 */
		.phy_addr = 0xd0076000,
		.size = 0x2000,
	},
	{ /* HDMIRX SEC AHB */
		.phy_addr = 0xc883e000,
		.size = 0x2000,
	},
	{ /* HDMIRX SEC AHB */
		.phy_addr = 0xda83e000,
		.size = 0x2000,
	},
	{ /* HDMIRX SEC APB4 */
		.phy_addr = 0xc8834400,
		.size = 0x2000,
	},
	{
		.phy_addr = 0xda846000,
		.size = 0x57ba000,
	},
};

static int in_reg_maps_idx(unsigned int addr)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(reg_maps); i++) {
		if ((addr >= reg_maps[i].phy_addr) &&
			(addr < (reg_maps[i].phy_addr + reg_maps[i].size))) {
			return i;
		}
	}

	return -1;
}

void rx_init_reg_map(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(reg_maps); i++) {
		reg_maps[i].p = ioremap(reg_maps[i].phy_addr, reg_maps[i].size);
		if (!reg_maps[i].p) {
			rx_pr("hdmirx: failed Mapped PHY: 0x%x\n",
				reg_maps[i].phy_addr);
		} else {
			reg_maps[i].flag = 1;
			rx_pr("hdmirx: Mapped PHY: 0x%x\n",
				reg_maps[i].phy_addr);
		}
	}
}

static int check_regmap_flag(unsigned int addr)
{
	return 1;

}

bool hdmirx_repeat_support(void)
{
	return repeat_function && downstream_repeat_support;
}

unsigned int rd_reg(unsigned int addr)
{
	int idx = in_reg_maps_idx(addr);
	unsigned int val = 0;

	if ((idx != -1) && check_regmap_flag(addr))
		val = readl(reg_maps[idx].p + (addr - reg_maps[idx].phy_addr));
	else
		rx_pr("rd reg %x error\n");
	return val;
}

void wr_reg(unsigned int addr, unsigned int val)
{
	int idx = in_reg_maps_idx(addr);

	if ((idx != -1) && check_regmap_flag(addr))
		writel(val, reg_maps[idx].p + (addr - reg_maps[idx].phy_addr));
	else
		rx_pr("wr reg %x err\n", addr);
}


static unsigned first_bit_set(uint32_t data)
{
	unsigned n = 32;

	if (data != 0) {
		for (n = 0; (data & 1) == 0; n++)
			data >>= 1;
	}
	return n;
}

uint32_t get(uint32_t data, uint32_t mask)
{
	return (data & mask) >> first_bit_set(mask);
}

uint32_t set(uint32_t data, uint32_t mask, uint32_t value)
{
	return ((value << first_bit_set(mask)) & mask) | (data & ~mask);
}


void hdmirx_timer_handler(unsigned long arg)
{
	struct hdmirx_dev_s *devp = (struct hdmirx_dev_s *)arg;
	rx_5v_det();
	rx_check_repeat();
	if (rx.open_fg)
		hdmirx_hw_monitor();
	devp->timer.expires = jiffies + TIMER_STATE_CHECK;
	add_timer(&devp->timer);
}

int hdmirx_dec_support(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	if ((port >= TVIN_PORT_HDMI0) && (port <= TVIN_PORT_HDMI7)) {
		rx_pr("hdmirx support\n");
		return 0;
	} else
		return -1;
}

int hdmirx_dec_open(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	struct hdmirx_dev_s *devp;

	devp = container_of(fe, struct hdmirx_dev_s, frontend);
	devp_hdmirx_suspend = container_of(fe, struct hdmirx_dev_s, frontend);
	devp->param.port = port;

	/* should enable the adc ref signal for audio pll */
	vdac_enable(1, 0x10);

	hdmirx_hw_init(port);
	/* timer */
	#if 0
	init_timer(&devp->timer);
	devp->timer.data = (ulong)devp;
	devp->timer.function = hdmirx_timer_handler;
	devp->timer.expires = jiffies + TIMER_STATE_CHECK;
	add_timer(&devp->timer);
	#endif
	rx.open_fg = 1;
	rx_pr("%s port:%x ok nosignal:%d\n", __func__, port, rx.no_signal);
	return 0;
}

void hdmirx_dec_start(struct tvin_frontend_s *fe, enum tvin_sig_fmt_e fmt)
{
	struct hdmirx_dev_s *devp;
	struct tvin_parm_s *parm;

	devp = container_of(fe, struct hdmirx_dev_s, frontend);
	devp_hdmirx_suspend = container_of(fe, struct hdmirx_dev_s, frontend);
	parm = &devp->param;
	parm->info.fmt = fmt;
	parm->info.status = TVIN_SIG_STATUS_STABLE;
	rx_pr("%s fmt:%d ok\n", __func__, fmt);
}

void hdmirx_dec_stop(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	struct hdmirx_dev_s *devp;
	struct tvin_parm_s *parm;

	devp = container_of(fe, struct hdmirx_dev_s, frontend);
	parm = &devp->param;
	/* parm->info.fmt = TVIN_SIG_FMT_NULL; */
	/* parm->info.status = TVIN_SIG_STATUS_NULL; */
	rx_pr("%s ok\n", __func__);
}

void hdmirx_dec_close(struct tvin_frontend_s *fe)
{
	struct hdmirx_dev_s *devp;
	struct tvin_parm_s *parm;

	/* should disable the adc ref signal for audio pll */
	vdac_enable(0, 0x10);

	/* open_flage = 0; */
	rx.open_fg = 0;
	devp = container_of(fe, struct hdmirx_dev_s, frontend);
	parm = &devp->param;
	/*del_timer_sync(&devp->timer);*/
	hdmirx_hw_uninit();
	parm->info.fmt = TVIN_SIG_FMT_NULL;
	parm->info.status = TVIN_SIG_STATUS_NULL;
	rx_pr("%s ok\n", __func__);
}

/* interrupt handler */
int hdmirx_dec_isr(struct tvin_frontend_s *fe, unsigned int hcnt64)
{
	struct hdmirx_dev_s *devp;
	struct tvin_parm_s *parm;

	devp = container_of(fe, struct hdmirx_dev_s, frontend);
	parm = &devp->param;
	/* if there is any error or overflow, do some reset, then rerurn -1;*/
	if ((parm->info.status != TVIN_SIG_STATUS_STABLE) ||
	    (parm->info.fmt == TVIN_SIG_FMT_NULL))
		return -1;
	else if (rx.change > 0)
		return TVIN_BUF_SKIP;
	return 0;
}

static int hdmi_dec_callmaster(enum tvin_port_e port,
	struct tvin_frontend_s *fe)
{
	int status = hdmirx_rd_top(TOP_HPD_PWR5V);
	switch (port) {
	case TVIN_PORT_HDMI0:
		status = status >> (20 + 0) & 0x1;
		break;
	case TVIN_PORT_HDMI1:
		status = status >> (20 + 1) & 0x1;
		break;
	case TVIN_PORT_HDMI2:
		status = status >> (20 + 2) & 0x1;
		break;
	case TVIN_PORT_HDMI3:
	    status = status >> (20 + 3) & 0x1;
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

bool hdmirx_is_nosig(struct tvin_frontend_s *fe)
{
	bool ret = 0;

	ret = hdmirx_hw_is_nosig();
	return ret;
}

bool hdmirx_fmt_chg(struct tvin_frontend_s *fe)
{
	bool ret = false;
	enum tvin_sig_fmt_e fmt = TVIN_SIG_FMT_NULL;
	struct hdmirx_dev_s *devp;
	struct tvin_parm_s *parm;

	devp = container_of(fe, struct hdmirx_dev_s, frontend);
	parm = &devp->param;
	if (hdmirx_hw_pll_lock() == false)
		ret = true;
	else {
		fmt = hdmirx_hw_get_fmt();
		if (fmt != parm->info.fmt) {
			rx_pr("hdmirx fmt: %d --> %d\n",
				parm->info.fmt, fmt);
			parm->info.fmt = fmt;
			ret = true;
		} else
		    ret = false;
	}
	return ret;
}

bool hdmirx_pll_lock(struct tvin_frontend_s *fe)
{
	bool ret = true;

	ret = hdmirx_hw_pll_lock();
	return ret;
}

enum tvin_sig_fmt_e hdmirx_get_fmt(struct tvin_frontend_s *fe)
{
	enum tvin_sig_fmt_e fmt = TVIN_SIG_FMT_NULL;

	fmt = hdmirx_hw_get_fmt();
	return fmt;
}
#define FORCE_YUV	1
#define FORCE_RGB	2

void hdmirx_get_sig_property(struct tvin_frontend_s *fe,
	struct tvin_sig_property_s *prop)
{
	unsigned char _3d_structure, _3d_ext_data;
	enum tvin_sig_fmt_e sig_fmt;
	int dvi_info = hdmirx_hw_get_dvi_info();
	unsigned int rate = rx.pre.refresh_rate * 2;

	/* use dvi info bit4~ for frame rate display */
	rate = rate/100 + (((rate%100)/10 >= 5) ? 1 : 0);

	prop->dvi_info = (rate << 4) | dvi_info;
	prop->colordepth = rx_get_colordepth();
	prop->dest_cfmt = TVIN_YUV422;
	switch (hdmirx_hw_get_color_fmt()) {
	case 1:
		prop->color_format = TVIN_YUV444;
		if (pc_mode_en)
			prop->dest_cfmt = TVIN_YUV444;
		/* if (hdmi_yuv444_enable) */
			/* prop->dest_cfmt = TVIN_YUV444; */
		break;
	case 3:
		prop->color_format = TVIN_YUV422;
		break;
	case 0:
		prop->color_format = TVIN_RGB444;
		if (((hdmi_yuv444_enable) && (it_content)) ||
			 pc_mode_en)
			prop->dest_cfmt = TVIN_YUV444;
		break;
	default:
		prop->color_format = TVIN_RGB444;
		if (pc_mode_en)
			prop->dest_cfmt = TVIN_YUV444;
		break;
	}

	if (force_colorspace == FORCE_YUV)
		prop->color_format = TVIN_YUV444;
	if (force_colorspace == FORCE_RGB)
		prop->color_format = TVIN_RGB444;

	sig_fmt = hdmirx_hw_get_fmt();

	prop->trans_fmt = TVIN_TFMT_2D;
	if (hdmirx_hw_get_3d_structure(&_3d_structure,
		&_3d_ext_data) >= 0) {
		if (_3d_structure == 0x1) {
			/* field alternative */
			prop->trans_fmt = TVIN_TFMT_3D_FA;
		} else if (_3d_structure == 0x2) {
			/* line alternative */
			prop->trans_fmt = TVIN_TFMT_3D_LA;
		} else if (_3d_structure == 0x3) {
			/* side-by-side full */
			prop->trans_fmt = TVIN_TFMT_3D_LRF;
		} else if (_3d_structure == 0x4) {
			/* L + depth */
			prop->trans_fmt = TVIN_TFMT_3D_LD;
		} else if (_3d_structure == 0x5) {
			/* L + depth + graphics + graphics-depth */
			prop->trans_fmt = TVIN_TFMT_3D_LDGD;
		} else if (_3d_structure == 0x6) {
			/* top-and-bot */
			prop->trans_fmt = TVIN_TFMT_3D_TB;
		} else if (_3d_structure == 0x8) {
			/* Side-by-Side half */
			switch (_3d_ext_data) {
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
	if (is_frame_packing())
		prop->trans_fmt = TVIN_TFMT_3D_FP;
	else if (is_alternative())
		prop->trans_fmt = TVIN_TFMT_3D_LA;

	prop->decimation_ratio = (hdmirx_hw_get_pixel_repeat() - 1) |
			HDMI_DE_REPEAT_DONE_FLAG;

	if (rx.pre.interlaced == 1)
		prop->dest_cfmt = TVIN_YUV422;

	switch (prop->color_format) {
	case TVIN_YUV444:
	case TVIN_YUV422:
		if (force_color_range) {
			if (force_color_range == 1)
				prop->color_fmt_range = TVIN_YUV_FULL;
			else if (force_color_range == 2)
				prop->color_fmt_range = TVIN_YUV_LIMIT;
			else
				prop->color_fmt_range = TVIN_FMT_RANGE_NULL;
		} else {
			if (yuv_quant_range == 1)
				prop->color_fmt_range = TVIN_YUV_LIMIT;
			else if (yuv_quant_range == 2)
				prop->color_fmt_range = TVIN_YUV_FULL;
			else
				prop->color_fmt_range = TVIN_FMT_RANGE_NULL;
		}
		break;
	case TVIN_RGB444:
		if (force_color_range) {
			if (force_color_range == 1)
				prop->color_fmt_range = TVIN_RGB_FULL;
			else if (force_color_range == 2)
				prop->color_fmt_range = TVIN_RGB_LIMIT;
			else
				prop->color_fmt_range = TVIN_FMT_RANGE_NULL;
		} else {
			if ((rgb_quant_range == 2) || (dvi_info == 1))
				prop->color_fmt_range = TVIN_RGB_FULL;
			else if (rgb_quant_range == 1)
				prop->color_fmt_range = TVIN_RGB_LIMIT;
			else
				prop->color_fmt_range = TVIN_FMT_RANGE_NULL;
		}
		break;

	default:
		prop->color_fmt_range = TVIN_FMT_RANGE_NULL;
		break;
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
			prop->hdr_info.hdr_data = rx.hdr_info.hdr_data;

			/* vdin can read current hdr data */
			prop->hdr_info.hdr_state = HDR_STATE_GET;

			/* Rx can get new hdr data */
			rx.hdr_info.hdr_state = HDR_STATE_NULL;
		}
	break;
	default:
	break;
	}
	/*in some PC case, 4096X2160 show in 3840X2160 monitor will
	result in blurred, so adjust hactive to 3840 to show dot by dot*/
	if (en_4096_2_3840) {
		if ((TVIN_SIG_FMT_HDMI_4096_2160_00HZ == sig_fmt) &&
			(prop->color_format == TVIN_RGB444)) {
			prop->hs = 128;
			prop->he = 128;
		}
	}
	if (en_4k_2_2k) {
		if (TVIN_SIG_FMT_HDMI_4096_2160_00HZ == sig_fmt) {
			prop->hs = 128;
			prop->he = 128;
			prop->vs = 0;
			prop->ve = 0;
			prop->scaling4w = 1920;
			prop->scaling4h = 1080;
		} else if (TVIN_SIG_FMT_HDMI_3840_2160_00HZ == sig_fmt) {
			prop->scaling4h = 1080;
			prop->scaling4w = 1920;
		}
	}
	if (((TVIN_SIG_FMT_HDMI_2880X480I_60HZ == sig_fmt) ||
			(TVIN_SIG_FMT_HDMI_2880X576I_50HZ == sig_fmt)) &&
			((prop->decimation_ratio & 0xF) == 0)) {
			prop->scaling4w = 1440;
		}
}

bool hdmirx_check_frame_skip(struct tvin_frontend_s *fe)
{
	return hdmirx_hw_check_frame_skip();
}

static struct tvin_state_machine_ops_s hdmirx_sm_ops = {
	.nosig            = hdmirx_is_nosig,
	.fmt_changed      = hdmirx_fmt_chg,
	.get_fmt          = hdmirx_get_fmt,
	.fmt_config       = NULL,
	.adc_cal          = NULL,
	.pll_lock         = hdmirx_pll_lock,
	.get_sig_propery  = hdmirx_get_sig_property,
	.vga_set_param    = NULL,
	.vga_get_param    = NULL,
	.check_frame_skip = hdmirx_check_frame_skip,
};

static int hdmirx_open(struct inode *inode, struct file *file)
{
	struct hdmirx_dev_s *devp;

	devp = container_of(inode->i_cdev, struct hdmirx_dev_s, cdev);
	file->private_data = devp;
	return 0;
}

static int hdmirx_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long hdmirx_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	/* unsigned int delay_cnt = 0; */
	void __user *argp = (void __user *)arg;

	if (_IOC_TYPE(cmd) != HDMI_IOC_MAGIC) {
		pr_err("%s invalid command: %u\n", __func__, cmd);
		return -ENOSYS;
	}
	switch (cmd) {
	case HDMI_IOC_HDCP_GET_KSV:{
		struct _hdcp_ksv ksv;
		if (argp == NULL)
			return -ENOSYS;
		ksv.bksv0 = rx.hdcp.bksv[0];
		ksv.bksv1 = rx.hdcp.bksv[1];
		if (copy_to_user(argp, &ksv,
			sizeof(struct _hdcp_ksv))) {
			ret = -EFAULT;
			break;
		}
		break;
	}
	case HDMI_IOC_HDCP_ON:
		hdcp_enable = 1;
		hdmirx_hw_config();
		hdmirx_set_hpd(rx.port, 0);
		rx.state = FSM_HPD_LOW;
		rx.pre_state = FSM_HPD_LOW;
		break;
	case HDMI_IOC_HDCP_OFF:
		hdcp_enable = 0;
		hdmirx_hw_config();
		hdmirx_set_hpd(rx.port, 0);
		rx.state = FSM_HPD_LOW;
		rx.pre_state = FSM_HPD_LOW;
		break;
	case HDMI_IOC_EDID_UPDATE:
		do_hpd_reset_flag = 1;
		rx.state = FSM_HPD_LOW;
		rx.pre_state = FSM_HPD_LOW;
		hdmi_rx_ctrl_edid_update();
		rx_pr("*update edid*\n");
		break;
	case HDMI_IOC_PC_MODE_ON:
		pc_mode_en = 1;
		rx_pr("pc mode on\n");
		break;
	case HDMI_IOC_PC_MODE_OFF:
		pc_mode_en = 0;
		rx_pr("pc mode off\n");
		break;
	case HDMI_IOC_HDCP22_AUTO:
		hdmirx_set_hpd(rx.port, 0);
		hdcp22_on = 1;
		force_hdcp14_en = 0;
		hdmirx_hw_config();
		hpd_to_esm = 1;
		rx.state = FSM_HPD_HIGH;
		rx.pre_state = FSM_HPD_HIGH;
		rx_pr("hdcp22 auto\n");
		break;
	case HDMI_IOC_HDCP22_FORCE14:
		hdmirx_set_hpd(rx.port, 0);
		force_hdcp14_en = 1;
		hdcp22_on = 0;
		hdmirx_wr_dwc(DWC_HDCP22_CONTROL, 0x2);
		esm_set_stable(0);
		rx.state = FSM_HPD_HIGH;
		rx.pre_state = FSM_HPD_HIGH;
		rx_pr("force hdcp1.4\n");
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long hdmirx_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	unsigned long ret;
	arg = (unsigned long)compat_ptr(arg);
	ret = hdmirx_ioctl(file, cmd, arg);
	return ret;
}
#endif

void hdmirx_wait_query(void)
{
	wake_up(&query_wait);
}

static ssize_t hdmirx_hpd_read(struct file *file,
	    char __user *buf, size_t count, loff_t *pos)
{
	int ret = 0;

	if (copy_to_user(buf, &pwr_sts, sizeof(unsigned int)))
		return -EFAULT;

	return ret;
}

static unsigned int hdmirx_hpd_poll(struct file *filp,
		poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(filp, &query_wait, wait);
	mask |= POLLIN|POLLRDNORM;

	return mask;
}

static const struct file_operations hdmirx_fops = {
	.owner		= THIS_MODULE,
	.open		= hdmirx_open,
	.release	= hdmirx_release,
	.read       = hdmirx_hpd_read,
	.poll       = hdmirx_hpd_poll,
	.unlocked_ioctl	= hdmirx_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = hdmirx_compat_ioctl,
#endif

};

/* attr */
static unsigned char *hdmirx_log_buf;
static unsigned int  hdmirx_log_wr_pos;
static unsigned int  hdmirx_log_rd_pos;
static unsigned int  hdmirx_log_buf_size;
static DEFINE_SPINLOCK(rx_pr_lock);
#define DEF_LOG_BUF_SIZE (1024*128)
#define PRINT_TEMP_BUF_SIZE 128

void hdmirx_powerdown(const char *buf, int size)
{
	char tmpbuf[128];
	int i = 0;

	while ((buf[i]) && (buf[i] != ',') && (buf[i] != ' ')) {
		tmpbuf[i] = buf[i];
		i++;
	}
	tmpbuf[i] = 0;
	if (strncmp(tmpbuf, "powerdown", 9) == 0) {
		if (rx.open_fg == 1) {
			del_timer_sync(&devp_hdmirx_suspend->timer);
			/* wr_reg(IO_APB_BUS_BASE,
				HHI_HDMIRX_CLK_CNTL, 0x0); */
		}
		rx_pr("[hdmirx]: hdmirx power down\n");
	}
}

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
			hdmirx_log_buf[hdmirx_log_wr_pos - hdmirx_log_buf_size]
				= buf[pos];
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
	if ((last_break == 1) &&
		(strlen(fmt) > 1)) {
		strcpy(buf, "[RX]-");
		for (len = 0; len < strlen(fmt); len++)
			if (fmt[len] == '\n')
				pos++;
			else
				break;

		strcpy(buf + 5, fmt + pos);
	} else
		strcpy(buf, fmt);
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
	if ((avail-len) <= 0)
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
	if ((bufsize >= 1024) && (hdmirx_log_buf == NULL)) {
		hdmirx_log_buf_size = 0;
		hdmirx_log_rd_pos = 0;
		hdmirx_log_wr_pos = 0;
		hdmirx_log_buf = kmalloc(bufsize, GFP_KERNEL);
		if (hdmirx_log_buf)
			hdmirx_log_buf_size = bufsize;
	}
	return 0;
}

static ssize_t show_log(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	unsigned long flags;
	ssize_t read_size = 0;

	if (hdmirx_log_buf_size == 0)
		return 0;
	spin_lock_irqsave(&rx_pr_lock, flags);
	if (hdmirx_log_rd_pos < hdmirx_log_wr_pos)
		read_size = hdmirx_log_wr_pos-hdmirx_log_rd_pos;
	else if (hdmirx_log_rd_pos > hdmirx_log_wr_pos)
		read_size = hdmirx_log_buf_size-hdmirx_log_rd_pos;

	if (read_size > PAGE_SIZE)
		read_size = PAGE_SIZE;
	if (read_size > 0)
		memcpy(buf, hdmirx_log_buf+hdmirx_log_rd_pos, read_size);
	hdmirx_log_rd_pos += read_size;
	if (hdmirx_log_rd_pos >= hdmirx_log_buf_size)
		hdmirx_log_rd_pos = 0;
	spin_unlock_irqrestore(&rx_pr_lock, flags);
	return read_size;
}

static ssize_t store_log(struct device *dev,
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


static ssize_t hdmirx_debug_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return 0;
}

static ssize_t hdmirx_debug_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	hdmirx_debug(buf, count);
	hdmirx_powerdown(buf, count);
	return count;
}

static ssize_t hdmirx_edid_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return hdmirx_read_edid_buf(buf, PAGE_SIZE);
}

static ssize_t hdmirx_edid_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	hdmirx_fill_edid_buf(buf, count);
	return count;
}

static ssize_t hdmirx_key_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return hdmirx_read_key_buf(buf, PAGE_SIZE);
}

static ssize_t hdmirx_key_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	hdmirx_fill_key_buf(buf, count);
	return count;
}

static ssize_t show_reg(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return hdmirx_hw_dump_reg(buf, PAGE_SIZE);
}

static ssize_t cec_get_state(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return 0;
}

static ssize_t cec_set_state(struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	return count;
}

static DEVICE_ATTR(debug, 0666, hdmirx_debug_show, hdmirx_debug_store);
static DEVICE_ATTR(edid, 0666, hdmirx_edid_show, hdmirx_edid_store);
static DEVICE_ATTR(key, 0666, hdmirx_key_show, hdmirx_key_store);
static DEVICE_ATTR(log, 0666, show_log, store_log);
static DEVICE_ATTR(reg, 0666, show_reg, store_log);
static DEVICE_ATTR(cec, 0666, cec_get_state, cec_set_state);

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
	/*return device_create(hdmirx_clsp, parent, devno, NULL, "%s",
	  TVHDMI_DEVICE_NAME); */
}

static void hdmirx_delete_device(int minor)
{
	dev_t devno = MKDEV(MAJOR(hdmirx_devno), minor);
	device_destroy(hdmirx_clsp, devno);
}

unsigned char *pEdid_buffer;
static int hdmirx_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct hdmirx_dev_s *hdevp;
	struct resource *res;
	struct pinctrl *pin;
	const char *pin_name;

	struct clk *xtal_clk;
	struct clk *fclk_div5_clk;
	int clk_rate;

	log_init(DEF_LOG_BUF_SIZE);
	pEdid_buffer = (unsigned char *) pdev->dev.platform_data;
	hdmirx_dev = &pdev->dev;
	/* allocate memory for the per-device structure */
	hdevp = kmalloc(sizeof(struct hdmirx_dev_s), GFP_KERNEL);
	if (!hdevp) {
		rx_pr("hdmirx:allocate memory failed\n");
		ret = -ENOMEM;
		goto fail_kmalloc_hdev;
	}
	memset(hdevp, 0, sizeof(struct hdmirx_dev_s));

	rx_init_reg_map();
	/*@to get from bsp*/
	#if 0
	if (pdev->dev.of_node) {
		ret = of_property_read_u32(pdev->dev.of_node,
				"hdmirx_id", &(hdevp->index));
		if (ret) {
			pr_err("%s:don't find  hdmirx id.\n", __func__);
			goto fail_create_device;
		}
	} else {
			pr_err("%s: don't find match hdmirx node\n", __func__);
			return -1;
	}
	#endif
	hdevp->index = 0; /* pdev->id; */
	/* create cdev and reigser with sysfs */
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
	ret = device_create_file(hdevp->dev, &dev_attr_cec);
	if (ret < 0) {
		rx_pr("hdmirx: fail to create cec attribute file\n");
		goto fail_create_cec_file;
	}
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		rx_pr("%s: can't get irq resource\n", __func__);
		ret = -ENXIO;
		/* goto fail_get_resource_irq; */
	}
	hdevp->irq = res->start;
	snprintf(hdevp->irq_name, sizeof(hdevp->irq_name),
			"hdmirx%d-irq", hdevp->index);
	rx_pr("hdevpd irq: %d, %d\n", hdevp->index,
			hdevp->irq);
	if (pdev->dev.of_node) {
		ret = of_property_read_u32(pdev->dev.of_node,
				"repeat", &repeat_function);
		if (ret) {
			pr_err("get repeat_function fail.\n");
			repeat_function = 0;
		}
	}
	rx.hdcp.switch_hdcp_auth.name = "hdmirx_hdcp_auth";
	ret = switch_dev_register(&rx.hdcp.switch_hdcp_auth);
	if (ret)
		pr_err("hdcp_auth switch init fail.\n");
	rx.hpd_sdev.name = "hdmirx_hpd";
	ret = switch_dev_register(&rx.hpd_sdev);
	if (ret)
		pr_err("hdmirx_hpd switch init fail.\n");
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

	/* pinmux set */
	if (pdev->dev.of_node) {
		ret = of_property_read_string_index(pdev->dev.of_node,
					    "pinctrl-names",
					    0, &pin_name);
		if (!ret) {
			pin = devm_pinctrl_get_select(&pdev->dev, pin_name);
			rx_pr("hdmirx: pinmux:%p, name:%s\n", pin, pin_name);
		}
	}
	if (pdev->dev.of_node) {
		ret = of_property_read_u32(pdev->dev.of_node,
				"rx_port_maps", &real_port_map);
		if (ret) {
			pr_err("get port_map fail.\n");
			real_port_map = 0x3120;
		}
	}
	if (pdev->dev.of_node) {
		ret = of_property_read_u32(pdev->dev.of_node,
				"hdmirx_addr_port", &hdmirx_addr_port);
		if (ret)
			pr_err("get hdmirx_addr_port fail.\n");
		ret = of_property_read_u32(pdev->dev.of_node,
				"hdmirx_data_port", &hdmirx_data_port);
		if (ret)
			pr_err("get hdmirx_data_port fail.\n");
		ret = of_property_read_u32(pdev->dev.of_node,
				"hdmirx_ctrl_port", &hdmirx_ctrl_port);
		if (ret)
			pr_err("get hdmirx_ctrl_port fail.\n");
		ret = of_property_read_u32(pdev->dev.of_node,
				"repeat", &repeat_function);
		if (ret) {
			pr_err("get repeat_function fail.\n");
			repeat_function = 0;
		}
	}
	dev_set_drvdata(hdevp->dev, hdevp);

	xtal_clk = clk_get(&pdev->dev, "xtal");
	if (IS_ERR(xtal_clk))
		rx_pr("get xtal err\n");
	else {
		clk_rate = clk_get_rate(xtal_clk);
		pr_info("%s: xtal_clk is %d MHZ\n", __func__,
				clk_rate/1000000);
	}
	fclk_div5_clk = clk_get(&pdev->dev, "fclk_div5");
	if (IS_ERR(fclk_div5_clk))
		rx_pr("get fclk_div5_clk err\n");
	else {
		clk_rate = clk_get_rate(fclk_div5_clk);
		pr_info("%s: fclk_div5_clk is %d MHZ\n", __func__,
				clk_rate/1000000);
	}
	hdevp->modet_clk = clk_get(&pdev->dev, "hdmirx_modet_clk");
	if (IS_ERR(hdevp->modet_clk))
		rx_pr("get modet_clk err\n");
	else {
		clk_set_parent(hdevp->modet_clk, xtal_clk);
		clk_set_rate(hdevp->modet_clk, 24000000);
		clk_rate = clk_get_rate(hdevp->modet_clk);
		pr_info("%s: modet_clk is %d MHZ\n", __func__,
				clk_rate/1000000);
	}

	hdevp->cfg_clk = clk_get(&pdev->dev, "hdmirx_cfg_clk");
	if (IS_ERR(hdevp->cfg_clk))
		rx_pr("get cfg_clk err\n");
	else {
		clk_set_parent(hdevp->cfg_clk, fclk_div5_clk);
		clk_set_rate(hdevp->cfg_clk, 133333333);
		clk_rate = clk_get_rate(hdevp->cfg_clk);
		pr_info("%s: cfg_clk is %d MHZ\n", __func__,
				clk_rate/1000000);
	}

	/*
	hdevp->acr_ref_clk = clk_get(&pdev->dev, "hdmirx_acr_ref_clk");
	if (IS_ERR(hdevp->acr_ref_clk))
		rx_pr("get acr_ref_clk err\n");
	else {
		clk_set_parent(hdevp->acr_ref_clk, fclk_div5_clk);
		clk_set_rate(hdevp->acr_ref_clk, 24000000);
		clk_rate = clk_get_rate(hdevp->acr_ref_clk);
		pr_info("%s: acr_ref_clk is %d MHZ\n", __func__,
				clk_rate/1000000);
	}
	*/
	hdevp->audmeas_clk = clk_get(&pdev->dev, "hdmirx_audmeas_clk");
	if (IS_ERR(hdevp->audmeas_clk))
		rx_pr("get audmeas_clk err\n");
	else {
		clk_set_parent(hdevp->audmeas_clk, fclk_div5_clk);
		clk_set_rate(hdevp->audmeas_clk, 200000000);
		clk_rate = clk_get_rate(hdevp->audmeas_clk);
		pr_info("%s: audmeas_clk is %d MHZ\n", __func__,
				clk_rate/1000000);
	}

	/* create for hot plug function */
	eq_wq = create_singlethread_workqueue(hdevp->frontend.name);
	INIT_DELAYED_WORK(&eq_dwork, eq_algorithm);

	esm_wq = create_singlethread_workqueue(hdevp->frontend.name);
	INIT_DELAYED_WORK(&esm_dwork, rx_hpd_to_esm_handle);
	/* queue_delayed_work(eq_wq, &eq_dwork, msecs_to_jiffies(5)); */

	ret = of_property_read_u32(pdev->dev.of_node,
				"en_4k_2_2k", &en_4k_2_2k);
	if (ret) {
			pr_err("%s:don't find  en_4k_2_2k.\n", __func__);
			en_4k_2_2k = 0;
	}

	hdmirx_hw_probe();

	init_timer(&hdevp->timer);
	hdevp->timer.data = (ulong)hdevp;
	hdevp->timer.function = hdmirx_timer_handler;
	hdevp->timer.expires = jiffies + TIMER_STATE_CHECK;
	add_timer(&hdevp->timer);
	rx.boot_flag = TRUE;

	rx_pr("hdmirx: driver probe ok\n");

	return 0;
fail_create_cec_file:
	device_remove_file(hdevp->dev, &dev_attr_cec);
fail_create_reg_file:
	device_remove_file(hdevp->dev, &dev_attr_reg);
fail_create_log_file:
	device_remove_file(hdevp->dev, &dev_attr_log);
fail_create_key_file:
	device_remove_file(hdevp->dev, &dev_attr_key);
fail_create_edid_file:
	device_remove_file(hdevp->dev, &dev_attr_edid);
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

	cancel_delayed_work(&eq_dwork);
	destroy_workqueue(eq_wq);

	cancel_delayed_work(&esm_dwork);
	destroy_workqueue(esm_wq);

	device_remove_file(hdevp->dev, &dev_attr_debug);
	device_remove_file(hdevp->dev, &dev_attr_edid);
	device_remove_file(hdevp->dev, &dev_attr_key);
	device_remove_file(hdevp->dev, &dev_attr_log);
	device_remove_file(hdevp->dev, &dev_attr_reg);
	device_remove_file(hdevp->dev, &dev_attr_cec);
	tvin_unreg_frontend(&hdevp->frontend);
	hdmirx_delete_device(hdevp->index);
	cdev_del(&hdevp->cdev);
	kfree(hdevp);
	rx_pr("hdmirx: driver removed ok.\n");
	return 0;
}

#ifdef CONFIG_PM
static int hdmirx_suspend(struct platform_device *pdev, pm_message_t state)
{
	int i = 0;

	rx_pr("[hdmirx]: hdmirx_suspend\n");
	if (rx.open_fg == 1) {
		if (resume_flag == 0)
			del_timer_sync(&devp_hdmirx_suspend->timer);
		for (i = 0; i < 5000; i++)
			;
	}
	if (suspend_pddq)
		hdmirx_phy_pddq(1);
	if (hdcp22_on)
		hdcp22_suspend();
	/*clk_off();*/
	rx_pr("[hdmirx]: suspend success\n");
	return 0;
}

static int hdmirx_resume(struct platform_device *pdev)
{
	int i;
	/*hdmirx_hw_probe();*/
	if (suspend_pddq)
		hdmirx_phy_pddq(0);
	for (i = 0; i < 5000; i++)
		;
	if ((resume_flag == 0) && (rx.open_fg == 1))
		add_timer(&devp_hdmirx_suspend->timer);
	if (hdcp22_on)
		hdcp22_resume();
	rx_pr("hdmirx: resume module---end,rx.open_fg:%d\n", rx.open_fg);
	pre_port = 0xff;
	return 0;

}
#endif

#ifdef CONFIG_HIBERNATION
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

static const struct of_device_id hdmirx_dt_match[] = {
	{
		.compatible     = "amlogic, hdmirx",
	},
	{},
};

static struct platform_driver hdmirx_driver = {
	.probe      = hdmirx_probe,
	.remove     = hdmirx_remove,
#ifdef CONFIG_PM
	.suspend    = hdmirx_suspend,
	.resume     = hdmirx_resume,
#endif
	.driver     = {
		.name   = TVHDMI_DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = hdmirx_dt_match,
#ifdef CONFIG_HIBERNATION
		.pm     = &hdmirx_pm,
#endif
	}
};

static int __init hdmirx_init(void)
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

	#if 0
	pdev = platform_device_alloc(TVHDMI_NAME, 0);
	if (IS_ERR(pdev)) {
		rx_pr("%s alloc platform device error.\n",
			__func__);
		goto fail_class_create;
	}
	if (platform_device_add(pdev)) {
		rx_pr("%s failed register platform device.\n",
			__func__);
		goto fail_class_create;
	}
	#endif
	ret = platform_driver_register(&hdmirx_driver);
	if (ret != 0) {
		rx_pr("register hdmirx module failed, error %d\n",
			ret);
		ret = -ENODEV;
		goto fail_pdrv_register;
	}
	rx_pr("hdmirx: hdmirx_init.\n");

	return 0;

fail_pdrv_register:
	class_destroy(hdmirx_clsp);
fail_class_create:
	unregister_chrdev_region(hdmirx_devno, 1);
fail_alloc_cdev_region:
	return ret;

}

static void __exit hdmirx_exit(void)
{
	class_destroy(hdmirx_clsp);
	unregister_chrdev_region(hdmirx_devno, 1);
	platform_driver_unregister(&hdmirx_driver);
	rx_pr("hdmirx: hdmirx_exit.\n");
}

module_init(hdmirx_init);
module_exit(hdmirx_exit);

MODULE_DESCRIPTION("AMLOGIC HDMIRX driver");
MODULE_LICENSE("GPL");
