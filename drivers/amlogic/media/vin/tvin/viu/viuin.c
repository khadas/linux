// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/module.h>

#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>

#include "../tvin_global.h"
#include "../tvin_frontend.h"
#include "../tvin_format_table.h"

#define DEVICE_NAME "viuin"
#define MODULE_NAME "viuin"

#define AMVIUIN_DEC_START       1
#define AMVIUIN_DEC_STOP        0

/* register */
#define VIU_MISC_CTRL1 0x1a07
#define VPU_VIU_VENC_MUX_CTRL 0x271a
#define ENCI_INFO_READ 0x271c
#define ENCP_INFO_READ 0x271d
#define ENCT_INFO_READ 0x271e
#define ENCL_INFO_READ 0x271f
#define VPU_VIU2VDIN_HDN_CTRL 0x2780

/* ENCL/ENCT polarity, positive:begin > end, negative: begin < end */
#define ENCL_VIDEO_VSO_BLINE	0x1cb9
#define ENCL_VIDEO_VSO_ELINE	0x1cba

/* ENCP polarity, positive:begin > end, negative: begin < end */
#define ENCP_VIDEO_VSO_BLINE	0x1bab
#define ENCP_VIDEO_VSO_ELINE	0x1bac

/* ENCI polarity, always negative, no need control dynamically */

/*g12a new add*/
#define VPU_VIU_ASYNC_MASK 0x2781
#define VDIN_MISC_CTRL 0x2782
#define VPU_VIU_VDIN_IF_MUX_CTRL 0x2783
#define VPU_VIU2VDIN1_HDN_CTRL 0x2784
#define VPU_VENCX_CLK_CTRL 0x2785
#define VPP_WRBAK_CTRL 0x1df9
#define VPU_422TO444_CTRL0 0x274b
#define VPU_422TO444_CTRL1 0x274c
#define VPU_422TO444_CTRL2 0x274d
#define WR_BACK_MISC_CTRL 0x1a0d

static unsigned int vsync_enter_line_curr;
module_param(vsync_enter_line_curr, uint, 0664);
MODULE_PARM_DESC(vsync_enter_line_curr,
		 "\n encoder process line num when enter isr.\n");

static unsigned int vsync_enter_line_max;
module_param(vsync_enter_line_max, uint, 0664);
MODULE_PARM_DESC(vsync_enter_line_max,
		 "\n max encoder process line num when enter isr.\n");

static unsigned int vsync_enter_line_max_threshold = 10000;
module_param(vsync_enter_line_max_threshold, uint, 0664);
MODULE_PARM_DESC(vsync_enter_line_max_threshold,
		 "\n max encoder process line num over threshold drop the frame.\n");

static unsigned int vsync_enter_line_min_threshold = 10000;
module_param(vsync_enter_line_min_threshold, uint, 0664);
MODULE_PARM_DESC(vsync_enter_line_min_threshold,
		 "\n max encoder process line num less threshold drop the frame.\n");
static unsigned int vsync_enter_line_threshold_overflow_count;
module_param(vsync_enter_line_threshold_overflow_count, uint, 0664);
MODULE_PARM_DESC(vsync_enter_line_threshold_overflow_count,
		 "\ncnt overflow encoder process line no over threshold drop the frame\n");

static unsigned short v_cut_offset;
module_param(v_cut_offset, ushort, 0664);
MODULE_PARM_DESC(v_cut_offset, "the cut window vertical offset for viuin");

static unsigned short open_cnt;
module_param(open_cnt, ushort, 0664);
MODULE_PARM_DESC(open_cnt, "open_cnt for vdin0/1");

struct viuin_s {
	unsigned int flag;
	struct vframe_prop_s *prop;
	/*add for tvin frontend*/
	struct tvin_frontend_s frontend;
	struct vdin_parm_s parm;
	unsigned int enc_info_addr;
};

static inline u32 rd_viu(u32 reg)
{
	return (u32)aml_read_vcbus(reg);
}

static inline void wr_viu(u32 reg,
			  const u32 val)
{
	aml_write_vcbus(reg, val);
}

static inline void wr_bits_viu(u32 reg,
			       const u32 value,
			       const u32 start,
			       const u32 len)
{
	aml_write_vcbus(reg, ((aml_read_vcbus(reg) &
			      ~(((1L << (len)) - 1) << (start))) |
			      (((value) & ((1L << (len)) - 1)) << (start))));
}

static inline u32 rd_bits_viu(u32 reg,
			      const u32 start,
			      const u32 len)
{
	u32 val;

	val = ((aml_read_vcbus(reg) >> (start)) & ((1L << (len)) - 1));

	return val;
}

static int viuin_support(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	if (port >= TVIN_PORT_VIU1 &&
	    port <= TVIN_PORT_VIU2_ENCP)
		return 0;
	else
		return -1;
}

void viuin_check_venc_line(struct viuin_s *devp_local)
{
	unsigned int vencv_line_cur, cnt;

	cnt = 0;
	do {
		vencv_line_cur =
			(rd_viu(devp_local->enc_info_addr) >> 16) & 0x1fff;
		udelay(9);
		cnt++;
		if (cnt > 100000)
			break;
	} while (vencv_line_cur != 1);

	if (vencv_line_cur != 1)
		pr_info("**************%s,vencv_line_cur:%d,cnt:%d***********\n",
			__func__, vencv_line_cur, cnt);
}

/*g12a/g12b and before: use viu_loop encl/encp*/
/*tl1: use viu_loop vpp */
static int viuin_open(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	struct viuin_s *devp = container_of(fe, struct viuin_s, frontend);
	unsigned int viu_mux = 0, viu_sel = 0;

	if (!memcpy(&devp->parm, fe->private_data,
		    sizeof(struct vdin_parm_s))) {
		pr_info("[viuin..]%s memcpy error.\n", __func__);
		return -1;
	}
	/*open the venc to vdin path*/
	pr_info("viu1_sel_venc: %d\n", rd_bits_viu(VPU_VIU_VENC_MUX_CTRL, 0, 2));
	pr_info("viu2_sel_venc: %d\n", rd_bits_viu(VPU_VIU_VENC_MUX_CTRL, 2, 2));
	switch (rd_bits_viu(VPU_VIU_VENC_MUX_CTRL, 0, 2)) {
	case 0: /* ENCL */
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
			viu_mux = 0x4;
		else
			viu_mux = 0x8;
		/* wr_bits(VPU_VIU_VENC_MUX_CTRL,0x88,4,8); */
		devp->enc_info_addr = ENCL_INFO_READ;
		break;
	case 1: /* ENCI */
		viu_mux = 0x1;/* wr_bits(VPU_VIU_VENC_MUX_CTRL,0x11,4,8); */
		devp->enc_info_addr = ENCI_INFO_READ;
		break;
	case 2: /* ENCP */
		viu_mux = 0x2;/* wr_bits(VPU_VIU_VENC_MUX_CTRL,0x22,4,8); */
		devp->enc_info_addr = ENCP_INFO_READ;
		break;
	case 3: /* ENCT */
		viu_mux = 0x4;/* wr_bits(VPU_VIU_VENC_MUX_CTRL,0x44,4,8); */
		devp->enc_info_addr = ENCT_INFO_READ;
		break;
	default:
		break;
	}
	/*no need check here, will timeout sometimes*/
	/*viuin_check_venc_line(devp);*/
	if (port == TVIN_PORT_VIU1_VIDEO) {/*old chip*/
		/* enable hsync for vdin loop */
		wr_bits_viu(VIU_MISC_CTRL1, 1, 28, 1);
		viu_mux = 0x4;
	}
	if (is_meson_gxbb_cpu() || is_meson_gxm_cpu() || is_meson_gxl_cpu()) {
		if (devp->parm.v_active == 2160 && devp->parm.frame_rate > 30)
			/* 1/2 down scaling */
			wr_viu(VPU_VIU2VDIN_HDN_CTRL, 0x40f00);
	} else {
		wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, devp->parm.h_active, 0, 14);
	}

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		if (port >= TVIN_PORT_VIU1_WB0_VD1 &&
		    port <= TVIN_PORT_VIU1_WB0_POST_BLEND) {
			if (port == TVIN_PORT_VIU1_WB0_VDIN_BIST)
				viu_mux = VIU_MUX_SEL_WB1;
			else
				viu_mux = VIU_MUX_SEL_WB0;
		} else if ((port >= TVIN_PORT_VIU1_WB1_VD1) &&
			 (port <= TVIN_PORT_VIU1_WB1_POST_BLEND))
			viu_mux = VIU_MUX_SEL_WB1;
		else if (port == TVIN_PORT_VIU2_ENCL)
			viu_mux = VIU_MUX_SEL_ENCL;
		else if (port == TVIN_PORT_VIU2_ENCI)
			viu_mux = VIU_MUX_SEL_ENCI;
		else if (port == TVIN_PORT_VIU2_ENCP)
			viu_mux = VIU_MUX_SEL_ENCP;

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			viu_sel = 1;
		} else {
			/*old chip*/
			switch (port & ~0xff) {
			case TVIN_PORT_VIU1:
				viu_sel = 1;
				break;
			case TVIN_PORT_VIU2:
				viu_sel = 2;
				break;
			default:
				break;
			}
		}

		if (viu_sel == 1) {
			/* for vdi6 , vdin source 7*/
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, 0, 0, 5);
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, viu_mux, 0, 5);
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, 0, 8, 5);
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, viu_mux, 8, 5);

			/* for vdi8 , vdin source 9*/
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, 0, 16, 5);
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, viu_mux, 16, 5);
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, 0, 24, 5);
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, viu_mux, 24, 5);
		} else if (viu_sel == 2) {
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, 0, 16, 5);
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, viu_mux, 16, 5);
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, 0, 24, 5);
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, viu_mux, 24, 5);
		} else {
			wr_viu(VPU_VIU_VDIN_IF_MUX_CTRL, 0);
		}
		/*wrbak_chan0_sel wb_chan_sel */
		if (port == TVIN_PORT_VIU1_WB0_VD1) {
			wr_bits_viu(VPP_WRBAK_CTRL, 1, 0, 4);
		} else if (port == TVIN_PORT_VIU1_WB0_VD2) {
			wr_bits_viu(VPP_WRBAK_CTRL, 2, 0, 4);
		} else if (port == TVIN_PORT_VIU1_WB0_OSD1) {
			wr_bits_viu(VPP_WRBAK_CTRL, 3, 0, 4);
		} else if (port == TVIN_PORT_VIU1_WB0_OSD2) {
			wr_bits_viu(VPP_WRBAK_CTRL, 4, 0, 4);
		} else if (port == TVIN_PORT_VIU1_WB0_POST_BLEND) {
			wr_bits_viu(VPP_WRBAK_CTRL, 5, 0, 4);
		} else if (port == TVIN_PORT_VIU1_WB0_VPP) {
			wr_bits_viu(VPP_WRBAK_CTRL, 6, 0, 4);

			/* increase h banking in case vdin afifo overflow
			 * pre chip has 8bits
			 * tm2_revb increased 4bits, all 12bit
			 */
			wr_bits_viu(VPP_WRBAK_CTRL, 0xff, 16, 8);
		} else {
			wr_bits_viu(VPP_WRBAK_CTRL, 0, 4, 3);
		}
		/*wrbak_chan0_sel wb_chan_sel*/
		if (port == TVIN_PORT_VIU1_WB1_VD1)
			wr_bits_viu(VPP_WRBAK_CTRL, 1, 4, 4);
		else if (port == TVIN_PORT_VIU1_WB1_VD2)
			wr_bits_viu(VPP_WRBAK_CTRL, 2, 4, 4);
		else if (port == TVIN_PORT_VIU1_WB1_OSD1)
			wr_bits_viu(VPP_WRBAK_CTRL, 3, 4, 4);
		else if (port == TVIN_PORT_VIU1_WB1_OSD2)
			wr_bits_viu(VPP_WRBAK_CTRL, 4, 4, 4);
		else if (port == TVIN_PORT_VIU1_WB1_POST_BLEND)
			wr_bits_viu(VPP_WRBAK_CTRL, 5, 4, 4);
		else if (port == TVIN_PORT_VIU1_WB1_VPP)
			wr_bits_viu(VPP_WRBAK_CTRL, 6, 4, 4);
		else
			wr_bits_viu(VPP_WRBAK_CTRL, 0, 4, 4);

		/* wrback hsync en */
		if (port >= TVIN_PORT_VIU1_WB0_VD1 &&
		    port <= TVIN_PORT_VIU1_WB0_POST_BLEND) {
			wr_bits_viu(WR_BACK_MISC_CTRL, 1, 0, 1);/*vd0 hsync*/
			wr_bits_viu(WR_BACK_MISC_CTRL, 0, 1, 1);/*vd1 hsync*/
		} else if ((port >= TVIN_PORT_VIU1_WB1_VD1) &&
			(port <= TVIN_PORT_VIU1_WB1_POST_BLEND)) {
			wr_bits_viu(WR_BACK_MISC_CTRL, 0, 0, 1);/*vd0 hsync*/
			wr_bits_viu(WR_BACK_MISC_CTRL, 1, 1, 1);/*vd1 hsync*/
		} else {
			wr_bits_viu(WR_BACK_MISC_CTRL, 0, 0, 2);
		}
	} else {
		wr_bits_viu(VPU_VIU_VENC_MUX_CTRL, viu_mux, 4, 4);
		wr_bits_viu(VPU_VIU_VENC_MUX_CTRL, viu_mux, 8, 4);
	}
	devp->flag = 0;
	open_cnt++;
	return 0;
}

static void viuin_close(struct tvin_frontend_s *fe)
{
	struct viuin_s *devp = container_of(fe, struct viuin_s, frontend);

	/*no need check here, will timeout sometimes*/
	/*viuin_check_venc_line(devp);*/
	memset(&devp->parm, 0, sizeof(struct vdin_parm_s));
	/*close the venc to vdin path*/
	if (open_cnt)
		open_cnt--;
	if (open_cnt == 0) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
			wr_viu(VPU_VIU_VDIN_IF_MUX_CTRL, 0);
			wr_viu(VPP_WRBAK_CTRL, 0);

		} else {
			wr_bits_viu(VPU_VIU_VENC_MUX_CTRL, 0, 8, 4);
			wr_bits_viu(VPU_VIU_VENC_MUX_CTRL, 0, 4, 4);
		}
	}
	if (rd_viu(VPU_VIU2VDIN_HDN_CTRL) != 0)
		wr_viu(VPU_VIU2VDIN_HDN_CTRL, 0x0);
}

static void viuin_start(struct tvin_frontend_s *fe, enum tvin_sig_fmt_e fmt)
{
	/* do something the same as start_amvdec_viu_in */
	struct viuin_s *devp = container_of(fe, struct viuin_s, frontend);

	if (devp->flag && AMVIUIN_DEC_START) {
		pr_info("[viuin..]%s viu_in is started already.\n", __func__);
		return;
	}
	vsync_enter_line_max = 0;
	vsync_enter_line_threshold_overflow_count = 0;
	devp->flag = AMVIUIN_DEC_START;
}

static void viuin_stop(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	struct viuin_s *devp = container_of(fe, struct viuin_s, frontend);

	if (devp->flag && AMVIUIN_DEC_START)
		devp->flag |= AMVIUIN_DEC_STOP;
	else
		pr_info("[viuin..]%s viu in dec isn't start.\n", __func__);
}

static int viuin_isr(struct tvin_frontend_s *fe, unsigned int hcnt64)
{
	int curr_port;

	struct viuin_s *devp = container_of(fe, struct viuin_s, frontend);

	curr_port = rd_bits_viu(VPU_VIU_VENC_MUX_CTRL, 0, 2);

	vsync_enter_line_curr = (rd_viu(devp->enc_info_addr) >> 16) & 0x1fff;
	if (vsync_enter_line_curr > vsync_enter_line_max)
		vsync_enter_line_max = vsync_enter_line_curr;
	if (vsync_enter_line_max_threshold > vsync_enter_line_min_threshold &&
	    curr_port == 0) {
		if (vsync_enter_line_curr > vsync_enter_line_max_threshold ||
		    vsync_enter_line_curr < vsync_enter_line_min_threshold) {
			vsync_enter_line_threshold_overflow_count++;
			return TVIN_BUF_SKIP;
		}
	}
	return 0;
}

static struct tvin_decoder_ops_s viu_dec_ops = {
	.support            = viuin_support,
	.open               = viuin_open,
	.start              = viuin_start,
	.stop               = viuin_stop,
	.close              = viuin_close,
	.decode_isr         = viuin_isr,
};

static void viuin_sig_property(struct tvin_frontend_s *fe,
			       struct tvin_sig_property_s *prop)
{
	static const struct vinfo_s *vinfo;
	struct viuin_s *devp = container_of(fe, struct viuin_s, frontend);
	unsigned int line_begin, line_end;

	switch (devp->parm.port) {
	case TVIN_PORT_VIU1_VIDEO:
	case TVIN_PORT_VIU1_WB0_POST_BLEND:
		prop->color_format = TVIN_YUV444;
		break;
	case TVIN_PORT_VIU1:
	case TVIN_PORT_VIU2:
	case TVIN_PORT_VIU1_WB0_VPP:
	case TVIN_PORT_VIU1_WB1_VPP:
		vinfo = get_current_vinfo();
		prop->color_format = (enum tvin_color_fmt_e)vinfo->viu_color_fmt;
		break;

	/* ENCL/ENCI/ENCP is only for viu2 loopback currently
	 * though hw also support viu1 loopback through ENC
	 */
	case TVIN_PORT_VIU2_ENCL:
		vinfo = get_current_vinfo2();
		line_begin = rd_viu(ENCL_VIDEO_VSO_BLINE);
		line_end = rd_viu(ENCL_VIDEO_VSO_ELINE);

		if (line_begin < line_end)
			prop->polarity_vs = 1; /* negative */
		else if (line_begin > line_end)
			prop->polarity_vs = 0; /* positive */
		else
			pr_err("error: TVIN_PORT_VIU2_ENCL line begin = end\n");

		prop->color_format = (enum tvin_color_fmt_e)vinfo->viu_color_fmt;
		break;
	case TVIN_PORT_VIU2_ENCI:
		vinfo = get_current_vinfo2();

		/* always negative */
		prop->polarity_vs = 1; /* negative */
		prop->color_format = (enum tvin_color_fmt_e)vinfo->viu_color_fmt;
		break;
	case TVIN_PORT_VIU2_ENCP:
		vinfo = get_current_vinfo2();
		line_begin = rd_viu(ENCP_VIDEO_VSO_BLINE);
		line_end = rd_viu(ENCP_VIDEO_VSO_ELINE);

		if (line_begin < line_end)
			prop->polarity_vs = 1; /* negative */
		else if (line_begin > line_end)
			prop->polarity_vs = 0; /* positive */
		else
			pr_err("error: TVIN_PORT_VIU2_ENCP line begin = end\n");

		prop->color_format = (enum tvin_color_fmt_e)vinfo->viu_color_fmt;
		break;
	default:
		prop->color_format = devp->parm.cfmt;
		break;
	}

	prop->dest_cfmt = devp->parm.dfmt;

	prop->scaling4w = devp->parm.dest_hactive;
	prop->scaling4h = devp->parm.dest_vactive;

	prop->vs = v_cut_offset;
	prop->ve = 0;
	prop->hs = 0;
	prop->he = 0;
	prop->decimation_ratio = 0;
}

static bool viu_check_frame_skip(struct tvin_frontend_s *fe)
{
	struct viuin_s *devp = container_of(fe, struct viuin_s, frontend);

	if (devp->parm.skip_count > 0) {
		devp->parm.skip_count--;
		return true;
	}

	return false;
}

static struct tvin_state_machine_ops_s viu_sm_ops = {
	.get_sig_property = viuin_sig_property,
	.check_frame_skip = viu_check_frame_skip,
};

static int viuin_probe(struct platform_device *pdev)
{
	struct viuin_s *viuin_devp;

	viuin_devp = kmalloc(sizeof(*viuin_devp), GFP_KERNEL);
	if (!viuin_devp)
		return -ENOMEM;
	memset(viuin_devp, 0, sizeof(struct viuin_s));
	sprintf(viuin_devp->frontend.name, "%s", DEVICE_NAME);
	if (!tvin_frontend_init(&viuin_devp->frontend,
				&viu_dec_ops, &viu_sm_ops, 0)) {
		if (tvin_reg_frontend(&viuin_devp->frontend))
			pr_info("[viuin..]%s register viu frontend error.\n",
				__func__);
	}
	platform_set_drvdata(pdev, viuin_devp);
	pr_info("[viuin..]%s probe ok.\n", __func__);
	return 0;
}

static int viuin_remove(struct platform_device *pdev)
{
	struct viuin_s *devp = platform_get_drvdata(pdev);

	if (devp) {
		tvin_unreg_frontend(&devp->frontend);
		kfree(devp);
	}
	return 0;
}

static struct platform_driver viuin_driver = {
	.probe	= viuin_probe,
	.remove	= viuin_remove,
	.driver	= {
		.name	= DEVICE_NAME,
	}
};

static struct platform_device *viuin_device;

int __init viuin_init_module(void)
{
	pr_info("[viuin..]%s viuin module init\n", __func__);
	viuin_device = platform_device_alloc(DEVICE_NAME, 0);
	if (!viuin_device) {
		pr_err("[viuin..]%s failed to alloc viuin_device.\n",
		       __func__);
		return -ENOMEM;
	}

	if (platform_device_add(viuin_device)) {
		platform_device_put(viuin_device);
		pr_err("[viuin..]%sfailed to add viuin_device.\n", __func__);
		return -ENODEV;
	}
	if (platform_driver_register(&viuin_driver)) {
		pr_err("[viuin..]%sfailed to register viuin driver.\n",
		       __func__);
		platform_device_del(viuin_device);
		platform_device_put(viuin_device);
		return -ENODEV;
	}

	pr_info("[viuin..]%s done\n", __func__);
	return 0;
}

void __exit viuin_exit_module(void)
{
	pr_info("[viuin..]%s viuin module remove.\n", __func__);
	platform_driver_unregister(&viuin_driver);
	platform_device_unregister(viuin_device);
}

//MODULE_DESCRIPTION("AMLOGIC viu input driver");
//MODULE_LICENSE("GPL");
//MODULE_VERSION("3.0.0");
