// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Linux Headers */
#include <linux/types.h>

/* Amlogic Headers */
#include <linux/amlogic/media/ge2d/ge2d.h>
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
#include <linux/amlogic/media/canvas/canvas.h>
#endif

/* Local Headers */
#include "ge2d_log.h"
#include "ge2d_io.h"
#include "ge2d_reg.h"

#define GE2D_DST1_INDEX 0
#define GE2D_SRC1_INDEX 1
#define GE2D_SRC2_INDEX 2

static unsigned int excluded_regs[] = {GE2D_SCALE_COEF_IDX, GE2D_SCALE_COEF};

static int gaul_filter_used;
static const  unsigned int filt_coef_gau1[] = { /* gau1+phase */
	0x20402000,
	0x203f2001,
	0x203e2002,
	0x203d2003,
	0x203c2004,
	0x203b2005,
	0x203a2006,
	0x20392007,
	0x20382008,
	0x20372009,
	0x2036200a,
	0x2035200b,
	0x2034200c,
	0x2033200d,
	0x2032200e,
	0x2031200f,
	0x20302010,
	0x202f2011,
	0x202e2012,
	0x202d2013,
	0x202c2014,
	0x202b2015,
	0x202a2016,
	0x20292017,
	0x20282018,
	0x20272019,
	0x2026201a,
	0x2025201b,
	0x2024201c,
	0x2023201d,
	0x2022201e,
	0x2021201f,
	0x20202020
};

/* average, no phase, horizontal filter and vertical filter for top field */
static const  unsigned int filt_coef_gau0[] = {
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020,
	0x20202020
};

/* average, no phase, only for vertical filter of bot filed*/
static const  unsigned int filt_coef_gau0_bot[] = {
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00,
	0x2a2b2a00
};

static const  unsigned int filt_coef0[] = { /* bicubic */
	0x00800000,
	0x007f0100,
	0xff7f0200,
	0xfe7f0300,
	0xfd7e0500,
	0xfc7e0600,
	0xfb7d0800,
	0xfb7c0900,
	0xfa7b0b00,
	0xfa7a0dff,
	0xf9790fff,
	0xf97711ff,
	0xf87613ff,
	0xf87416fe,
	0xf87218fe,
	0xf8701afe,
	0xf76f1dfd,
	0xf76d1ffd,
	0xf76b21fd,
	0xf76824fd,
	0xf76627fc,
	0xf76429fc,
	0xf7612cfc,
	0xf75f2ffb,
	0xf75d31fb,
	0xf75a34fb,
	0xf75837fa,
	0xf7553afa,
	0xf8523cfa,
	0xf8503ff9,
	0xf84d42f9,
	0xf84a45f9,
	0xf84848f8
};

static const    unsigned int filt_coef1[] = { /* 2 point bilinear */
	0x00800000,
	0x007e0200,
	0x007c0400,
	0x007a0600,
	0x00780800,
	0x00760a00,
	0x00740c00,
	0x00720e00,
	0x00701000,
	0x006e1200,
	0x006c1400,
	0x006a1600,
	0x00681800,
	0x00661a00,
	0x00641c00,
	0x00621e00,
	0x00602000,
	0x005e2200,
	0x005c2400,
	0x005a2600,
	0x00582800,
	0x00562a00,
	0x00542c00,
	0x00522e00,
	0x00503000,
	0x004e3200,
	0x004c3400,
	0x004a3600,
	0x00483800,
	0x00463a00,
	0x00443c00,
	0x00423e00,
	0x00404000
};

static const    unsigned int filt_coef2[] = { /* 3 point triangle */
	0x40400000,
	0x3f400100,
	0x3d410200,
	0x3c410300,
	0x3a420400,
	0x39420500,
	0x37430600,
	0x36430700,
	0x35430800,
	0x33450800,
	0x32450900,
	0x31450a00,
	0x30450b00,
	0x2e460c00,
	0x2d460d00,
	0x2c470d00,
	0x2b470e00,
	0x29480f00,
	0x28481000,
	0x27481100,
	0x26491100,
	0x25491200,
	0x24491300,
	0x234a1300,
	0x224a1400,
	0x214a1500,
	0x204a1600,
	0x1f4b1600,
	0x1e4b1700,
	0x1d4b1800,
	0x1c4c1800,
	0x1b4c1900,
	0x1a4c1a00
};

static const unsigned int filt_coef3[] = { /* 3 point triangle */
	0x20402000,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00
};

void ge2d_canv_config(u32 index, ulong *addr, u32 *stride, u32 *stride_mode,
		      u32 mask)
{
	int i;
	u32 m = mask;

	ge2d_log_dbg("%s:index=%d,addr=%lx,stride=%d\n",
		     __func__, index, addr[0], stride[0]);
	if (ge2d_meson_dev.canvas_status == 1) {
		if (index <= 2) {
			ge2d_reg_write(m | (GE2D_DST1_BADDR_CTRL + index * 2),
				       ((addr[0] + 7) >> 3));
			ge2d_reg_write(m | (GE2D_DST1_STRIDE_CTRL + index * 2),
				       ((stride[0] + 7) >> 3));
		}
	} else if (ge2d_meson_dev.canvas_status == 2) {
		if (index <= 2) {
			if (!ge2d_meson_dev.blk_stride_mode)
				memset(stride_mode, 0, sizeof(u32) * MAX_PLANE);

			switch (index) {
			case GE2D_SRC1_INDEX:
				for (i = 0; i < 3; i++) {
					if (!addr[i] || !stride[i])
						break;
					ge2d_reg_write
					(m |
					 (GE2D_C1_SRC1_BADDR_CTRL_Y + i * 2),
					 ((addr[i] + 7) >> 3));
					ge2d_reg_write
					(m |
					 (GE2D_C1_SRC1_STRIDE_CTRL_Y + i * 2),
					 ((stride[i] + 7) >> 3) |
					 (stride_mode[i] << 17));
				}
				break;
			case GE2D_SRC2_INDEX:
				ge2d_reg_write(m | GE2D_C1_SRC2_BADDR_CTRL,
					       ((addr[0] + 7) >> 3));
				ge2d_reg_write(m | GE2D_C1_SRC2_STRIDE_CTRL,
					       ((stride[0] + 7) >> 3) |
					       (stride_mode[0] << 17));
				break;
			case GE2D_DST1_INDEX:
				for (i = 0; i < 2; i++) {
					if (!addr[i] || !stride[i])
						break;
					ge2d_reg_write
					(m | (GE2D_C1_DST1_BADDR_CTRL + i * 2),
					 ((addr[i] + 7) >> 3));
					ge2d_reg_write
					(m |
					 (GE2D_C1_DST1_STRIDE_CTRL + i * 2),
					 ((stride[i] + 7) >> 3) |
					 (stride_mode[i] << 17));
				}
				break;
			}
		}
	}
}

#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
static void get_canvas_info(u32 canvas_index, ulong *addr, u32 *stride,
			    u32 *stride_mode)
{
	struct canvas_s canvas;
	int i, tmp_index = 0;

	if (!addr || !stride || !stride_mode) {
		ge2d_log_err("get canvas info error\n");
		return;
	}

	for (i = 0; i < 3 ; i++) {
		tmp_index = (canvas_index >> (i * 8)) & 0xff;
		if (tmp_index) {
			canvas_read(tmp_index, &canvas);
			addr[i] = canvas.addr;
			stride[i] = canvas.width;
			stride_mode[i] = canvas.blkmode;
		}
	}
}
#endif

void ge2d_lut_init(struct ge2d_config_s *cfg)
{
	u32 data, i, table_count;
	u32 *table_data;

	data  = 0;
	data |= 0;      /* start LUT position 0 */
	data |= 0 << 8; /* write LUT */
	ge2d_reg_write(GE2D_SRC1_LUT_ADDR, data);

	if (!cfg->clut8_table.data) {
		ge2d_log_err("%s clut8_table data is NULL\n", __func__);
		return;
	}

	table_count = cfg->clut8_table.count;
	table_data = cfg->clut8_table.data;
	for (i = 0; i < table_count; i++)
		ge2d_reg_write(GE2D_SRC1_LUT_DAT, table_data[i]);
}

void ge2d_set_src1_data(struct ge2d_src1_data_s *cfg, unsigned int mask)
{
	struct ge2d_config_s *ge2d_config_s;
	u32 m = mask;

	ge2d_reg_set_bits(m | GE2D_GEN_CTRL1, cfg->urgent_en,  10, 1);

	ge2d_reg_set_bits(m | GE2D_GEN_CTRL1, cfg->ddr_burst_size_y,  20, 2);
	ge2d_reg_set_bits(m | GE2D_GEN_CTRL1, cfg->ddr_burst_size_cb, 18, 2);
	ge2d_reg_set_bits(m | GE2D_GEN_CTRL1, cfg->ddr_burst_size_cr, 16, 2);

	if (ge2d_meson_dev.canvas_status) {
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
		/* if virtual canvas is used, get info from it */
		if (cfg->canaddr)
			get_canvas_info(cfg->canaddr, cfg->phy_addr,
					cfg->stride, cfg->stride_mode);
#endif
		ge2d_canv_config(GE2D_SRC1_INDEX,
				 cfg->phy_addr,
				 cfg->stride,
				 cfg->stride_mode,
				 m);
	} else {
		ge2d_reg_write(m | GE2D_SRC1_CANVAS,
			       ((cfg->canaddr & 0xff) << 24) |
			       (((cfg->canaddr >> 8) & 0xff) << 16) |
			       (((cfg->canaddr >> 16) & 0xff) << 8));
	}

	ge2d_reg_set_bits(m | GE2D_GEN_CTRL0,
			  ((cfg->x_yc_ratio << 1) | cfg->y_yc_ratio),
			   10, 2);
	ge2d_reg_set_bits(m | GE2D_GEN_CTRL0, cfg->sep_en, 0, 1);
	ge2d_reg_set_bits(m | GE2D_GEN_CTRL2, cfg->endian, 7, 1);
	ge2d_reg_set_bits(m | GE2D_GEN_CTRL2, cfg->color_map, 3, 4);
	ge2d_reg_set_bits(m | GE2D_GEN_CTRL2, cfg->format, 0, 2);
	if (ge2d_meson_dev.deep_color == 1)
		ge2d_reg_set_bits(m | GE2D_GEN_CTRL2, cfg->deep_color, 2, 1);
	if (ge2d_meson_dev.canvas_status == 1) {
		ge2d_reg_set_bits(m | GE2D_GEN_CTRL2,
				  cfg->mult_rounding, 18, 1);
		ge2d_reg_set_bits(m | GE2D_GEN_CTRL2,
				  cfg->alpha_conv_mode0, 31, 1);
		ge2d_reg_set_bits(m | GE2D_GEN_CTRL2,
				  cfg->alpha_conv_mode1, 10, 1);
		ge2d_reg_set_bits(m | GE2D_GEN_CTRL2,
				  cfg->color_conv_mode0, 30, 1);
		ge2d_reg_set_bits(m | GE2D_GEN_CTRL1,
				  cfg->color_conv_mode1, 26, 1);
	}
	ge2d_reg_set_bits(m | GE2D_GEN_CTRL0, cfg->mode_8b_sel, 5, 2);
	ge2d_reg_set_bits(m | GE2D_GEN_CTRL0, cfg->lut_en, 3, 1);
	ge2d_config_s = container_of(cfg, struct ge2d_config_s, src1_data);

	if (cfg->lut_en)
		ge2d_lut_init(ge2d_config_s);

	ge2d_reg_write(m | GE2D_SRC1_DEF_COLOR, cfg->def_color);
	if (cfg->x_yc_ratio)
		/* horizontal formatter enable */
		ge2d_reg_set_bits(m | GE2D_SRC1_FMT_CTRL, 1, 18, 1);
	else
		/* horizontal formatter disable */
		ge2d_reg_set_bits(m | GE2D_SRC1_FMT_CTRL, 0, 18, 1);
	if (cfg->y_yc_ratio)
		/* vertical formatter enable */
		ge2d_reg_set_bits(m | GE2D_SRC1_FMT_CTRL, 1, 16, 1);
	else
		/* vertical formatter disable */
		ge2d_reg_set_bits(m | GE2D_SRC1_FMT_CTRL, 0, 16, 1);
}

void ge2d_set_src1_scale_coef(unsigned int v_filt_type,
			      unsigned int h_filt_type,
			      unsigned int mask)
{
	int i;
	u32 m = mask;

	/* write vert filter coefs */
	ge2d_reg_write(m | GE2D_SCALE_COEF_IDX, 0x0000);
	if (v_filt_type == FILTER_TYPE_GAU0 ||
	    v_filt_type == FILTER_TYPE_GAU0_BOT ||
	    v_filt_type == FILTER_TYPE_GAU1 ||
	    h_filt_type == FILTER_TYPE_GAU0 ||
	    h_filt_type == FILTER_TYPE_GAU0_BOT ||
	    h_filt_type == FILTER_TYPE_GAU1)
		gaul_filter_used = 1;
	else
		gaul_filter_used = 0;
	for (i = 0; i < 33; i++) {
		if (v_filt_type == FILTER_TYPE_BICUBIC) {
			ge2d_reg_write(m | GE2D_SCALE_COEF, filt_coef0[i]);
		} else if (v_filt_type == FILTER_TYPE_BILINEAR) {
			ge2d_reg_write(m | GE2D_SCALE_COEF, filt_coef1[i]);
		} else if (v_filt_type == FILTER_TYPE_TRIANGLE) {
			ge2d_reg_write(m | GE2D_SCALE_COEF, filt_coef2[i]);
		} else if (v_filt_type == FILTER_TYPE_GAU0) {
			ge2d_reg_write(m | GE2D_SCALE_COEF, filt_coef_gau0[i]);
		} else if (v_filt_type == FILTER_TYPE_GAU0_BOT) {
			ge2d_reg_write(m | GE2D_SCALE_COEF, filt_coef_gau0_bot[i]);
		} else if (v_filt_type == FILTER_TYPE_GAU1) {
			ge2d_reg_write(m | GE2D_SCALE_COEF, filt_coef_gau1[i]);
		} else {
			/* TODO */
			ge2d_reg_write(m | GE2D_SCALE_COEF, filt_coef3[i]);
		}
	}

	/* write horz filter coefs */
	ge2d_reg_write(GE2D_SCALE_COEF_IDX, 0x0100);
	for (i = 0; i < 33; i++) {
		if (h_filt_type == FILTER_TYPE_BICUBIC) {
			ge2d_reg_write(m | GE2D_SCALE_COEF, filt_coef0[i]);
		} else if (h_filt_type == FILTER_TYPE_BILINEAR) {
			ge2d_reg_write(m | GE2D_SCALE_COEF, filt_coef1[i]);
		} else if (h_filt_type == FILTER_TYPE_TRIANGLE) {
			ge2d_reg_write(m | GE2D_SCALE_COEF, filt_coef2[i]);
		} else if (h_filt_type == FILTER_TYPE_GAU0) {
			ge2d_reg_write(m | GE2D_SCALE_COEF, filt_coef_gau0[i]);
		} else if (h_filt_type == FILTER_TYPE_GAU0_BOT) {
			ge2d_reg_write(m | GE2D_SCALE_COEF,
				       filt_coef_gau0_bot[i]);
		} else if (h_filt_type == FILTER_TYPE_GAU1) {
			ge2d_reg_write(m | GE2D_SCALE_COEF, filt_coef_gau1[i]);
		} else {
			/* TODO */
			ge2d_reg_write(m | GE2D_SCALE_COEF, filt_coef3[i]);
		}
	}
}

void ge2d_set_src1_gen(struct ge2d_src1_gen_s *cfg, unsigned int mask)
{
	ge2d_reg_write(mask | GE2D_SRC1_CLIPX_START_END,
		       (cfg->clipx_start_ex << 31) |
		       (cfg->clipx_start << 16) |
		       (cfg->clipx_end_ex << 15) |
		       (cfg->clipx_end << 0)
		       );

	ge2d_reg_write(mask | GE2D_SRC1_CLIPY_START_END,
		       (cfg->clipy_start_ex << 31) |
		       (cfg->clipy_start << 16) |
		       (cfg->clipy_end_ex << 15) |
		       (cfg->clipy_end << 0)
		       );

	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL0, cfg->pic_struct, 1, 2);
	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL0, (cfg->fill_mode & 0x1), 4, 1);

	ge2d_reg_set_bits(mask | GE2D_SRC_OUTSIDE_ALPHA,
			  ((cfg->fill_mode & 0x2) << 7) |
			  cfg->outside_alpha, 0, 9);

	ge2d_reg_set_bits(mask | GE2D_SRC1_FMT_CTRL, cfg->chfmt_rpt_pix, 19, 1);
	ge2d_reg_set_bits(mask | GE2D_SRC1_FMT_CTRL, cfg->cvfmt_rpt_pix, 17, 1);
}

void ge2d_set_src2_dst_data(struct ge2d_src2_dst_data_s *cfg, unsigned int mask)
{
	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL1, cfg->urgent_en,  9, 1);
	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL1, cfg->ddr_burst_size, 22, 2);

	if (ge2d_meson_dev.canvas_status) {
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
		/* if virtual canvas is used, get info from it */
		if (cfg->src2_canaddr)
			get_canvas_info(cfg->src2_canaddr, cfg->src2_phyaddr,
					cfg->src2_stride,
					cfg->src2_stride_mode);
		if (cfg->dst_canaddr)
			get_canvas_info(cfg->dst_canaddr, cfg->dst_phyaddr,
					cfg->dst_stride, cfg->dst_stride_mode);
#endif

		ge2d_canv_config(GE2D_SRC2_INDEX,
				 cfg->src2_phyaddr,
				 cfg->src2_stride,
				 cfg->src2_stride_mode,
				 mask);
		ge2d_canv_config(GE2D_DST1_INDEX,
				 cfg->dst_phyaddr,
				 cfg->dst_stride,
				 cfg->dst_stride_mode,
				 mask);
	} else {
		/* only for m6 and later chips. */
		ge2d_reg_write(mask | GE2D_SRC2_DST_CANVAS,
				(cfg->src2_canaddr << 8) |
				((cfg->dst_canaddr & 0xff) << 0) |
				((cfg->dst_canaddr & 0xff00) << 8)
				);
	}

	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL2, cfg->src2_endian, 15, 1);
	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL2, cfg->src2_color_map, 11, 4);
	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL2, cfg->src2_format, 8, 2);
	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL2, cfg->dst_endian, 23, 1);
	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL5, cfg->dst_swap64bit, 31, 1);
	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL2, cfg->dst_color_map, 19, 4);
	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL2, cfg->dst_format, 16, 2);
	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL0, cfg->src2_mode_8b_sel, 15, 2);
	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL0, cfg->dst_mode_8b_sel, 24, 2);

	if (ge2d_meson_dev.dst_repeat)
		ge2d_reg_set_bits(mask | GE2D_GEN_CTRL0, cfg->dst_rpt, 17, 6);

	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL3,
			  cfg->dst2_pixel_byte_width, 16, 2);
	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL3, cfg->dst2_color_map, 19, 4);
	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL3, cfg->dst2_discard_mode, 10, 4);
	/* ge2d_reg_set_bits (GE2D_GEN_CTRL3, 1, 0, 1); */
	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL3, cfg->dst2_enable, 8, 1);
	ge2d_reg_write(mask | GE2D_SRC2_DEF_COLOR, cfg->src2_def_color);
}

void ge2d_set_src2_dst_gen(struct ge2d_src2_dst_gen_s *cfg,
			   struct ge2d_cmd_s *cmd, unsigned int mask)
{
	unsigned int widtho, heighto, widthi, heighti;
	unsigned int is_rotate = cmd->dst_xy_swap ? 1 : 0;
	unsigned int is_blend = cmd->cmd_op == IS_BLEND ? 1 : 0;
	unsigned int x_rpt = 0, y_rpt = 0;
	unsigned int x_start = 0, x_end = 0, y_start = 0, y_end = 0;

	/* in blend case, if src2_repeat is support, use output W H to set */
	if (is_blend && ge2d_meson_dev.src2_repeat) {
		widtho = is_rotate ?
			 (cmd->dst_y_end - cmd->dst_y_start + 1) :
			 (cmd->dst_x_end - cmd->dst_x_start + 1);
		heighto = is_rotate ?
			  (cmd->dst_x_end - cmd->dst_x_start + 1) :
			  (cmd->dst_y_end - cmd->dst_y_start + 1);
		widthi = cmd->src2_x_end - cmd->src2_x_start + 1;
		heighti = cmd->src2_y_end - cmd->src2_y_start + 1;

		ge2d_log_dbg("src2_clipx start:%d end:%d\n",
			     cfg->src2_clipx_start, cfg->src2_clipx_end);
		ge2d_log_dbg("src2_clipy start:%d end:%d\n",
			     cfg->src2_clipy_start, cfg->src2_clipy_end);

		ge2d_log_dbg("dst_clipx start:%d end:%d\n",
			     cfg->dst_clipx_start, cfg->dst_clipx_end);
		ge2d_log_dbg("dst_clipy start:%d end:%d\n",
			     cfg->dst_clipy_start, cfg->dst_clipy_end);

		if (widthi == 0 || heighti == 0) {
			ge2d_log_err("wrong clip parameters, widthi=%d,heighti=%d\n",
				     widthi, heighti);
			return;
		}

		switch (widtho / widthi) {
		case 0:
			ge2d_log_err("src2_clipx scale down is not supported\n");
			x_rpt = 0;
			break;
		case 1:
			x_rpt = 0;
			break;
		case 2:
			x_rpt = 1;
			break;
		case 4:
			x_rpt = 2;
			break;
		case 8:
			x_rpt = 3;
			break;
		default:
			x_rpt = 3;
			ge2d_log_err("src2_clipx scale up just support 2^n\n");
			break;
		}

		switch (heighto / heighti) {
		case 0:
			ge2d_log_err("src2_clipy scale down is not supported\n");
			y_rpt = 0;
			break;
		case 1:
			y_rpt = 0;
			break;
		case 2:
			y_rpt = 1;
			break;
		case 4:
			y_rpt = 2;
			break;
		case 8:
			y_rpt = 3;
			break;
		default:
			y_rpt = 3;
			ge2d_log_err("src2_clipy scale up just support 2^n\n");
			break;
		}
		ge2d_log_dbg("src2_clipx_repeat(%d)\n", x_rpt);
		ge2d_log_dbg("src2_clipy_repeat(%d)\n", y_rpt);

		x_start = cfg->src2_clipx_start * (1 << x_rpt);
		x_end   = x_start + widthi * (1 << x_rpt) - 1;
		y_start = cfg->src2_clipy_start * (1 << y_rpt);
		y_end   = y_start + heighti * (1 << y_rpt) - 1;

		ge2d_log_dbg("src2_clipx setting, start:%d end:%d\n",
			     x_start, x_end);
		ge2d_log_dbg("src2_clipy setting, start:%d end:%d\n",
			     y_start, y_end);
		ge2d_reg_write(mask | GE2D_SRC2_CLIPX_START_END,
			       (x_start << 16) |
			       (x_end << 0)
			       );

		ge2d_reg_write(mask | GE2D_SRC2_CLIPY_START_END,
			       (y_start << 16) |
			       (y_end << 0)
			       );
	} else {
		ge2d_reg_write(mask | GE2D_SRC2_CLIPX_START_END,
			       (cfg->src2_clipx_start << 16) |
			       (cfg->src2_clipx_end << 0)
			       );

		ge2d_reg_write(mask | GE2D_SRC2_CLIPY_START_END,
			       (cfg->src2_clipy_start << 16) |
			       (cfg->src2_clipy_end << 0)
			       );
	}

	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL0, cfg->src2_pic_struct, 12, 2);
	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL0, (cfg->src2_fill_mode & 0x1), 14, 1);

	ge2d_reg_set_bits(mask | GE2D_SRC_OUTSIDE_ALPHA,
			  ((cfg->src2_fill_mode & 0x2) << 7) |
			  cfg->src2_outside_alpha, 16, 9);

	ge2d_reg_write(mask | GE2D_DST_CLIPX_START_END,
		       (cfg->dst_clipx_start << 16) |
		       (cfg->dst_clipx_end << 0)
		       );

	ge2d_reg_write(mask | GE2D_DST_CLIPY_START_END,
		       (cfg->dst_clipy_start << 16) |
		       (cfg->dst_clipy_end << 0)
		       );

	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL0, cfg->dst_clip_mode,  23, 1);
	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL1, cfg->dst_pic_struct, 14, 2);
}

void ge2d_set_dp_gen(struct ge2d_config_s *config, u32 mask)
{
	struct ge2d_dp_gen_s *cfg = &config->dp_gen;

	unsigned int antiflick_color_filter_n1[] = {0,   8,    16, 32};
	unsigned int antiflick_color_filter_n2[] = {128, 112,  96, 64};
	unsigned int antiflick_color_filter_n3[] = {0,   8,    16, 32};
	unsigned int antiflick_color_filter_th[] = {8, 16, 64};
	unsigned int antiflick_alpha_filter_n1[] = {0,    8,  16, 32};
	unsigned int antiflick_alpha_filter_n2[] = {128, 112,  96, 64};
	unsigned int antiflick_alpha_filter_n3[] = {0,    8,  16, 32};
	unsigned int antiflick_alpha_filter_th[] = {8, 16, 64};
	unsigned int matrix_total = 1, i = 0;
	unsigned int matrix_base_addr;
	unsigned int matrix_pre_offset;
	unsigned int matrix_coef00_01;
	unsigned int matrix_coef02_10;
	unsigned int matrix_coef11_12;
	unsigned int matrix_coef20_21;
	unsigned int matrix_coef22_ctrl;
	unsigned int matrix_offset;
	unsigned char matrix_using = 0;
	unsigned char matrix_en = 0;

	if (cfg->conv_matrix_en) {
		cfg->antiflick_ycbcr_rgb_sel = 0; /* 0: yuv2rgb 1:rgb2rgb */
		cfg->antiflick_cbcr_en = 1;
		cfg->antiflick_r_coef = 0;
		cfg->antiflick_g_coef = 0;
		cfg->antiflick_b_coef = 0;
	} else {
		cfg->antiflick_ycbcr_rgb_sel = 1; /* 0: yuv2rgb 1:rgb2rgb */
		cfg->antiflick_cbcr_en = 1;
		cfg->antiflick_r_coef = 0x42;	/* 0.257 */
		cfg->antiflick_g_coef = 0x81;	/* 0.504 */
		cfg->antiflick_b_coef = 0x19;	/* 0.098 */
	}
	memcpy(cfg->antiflick_color_filter_n1, antiflick_color_filter_n1,
	       4 * sizeof(unsigned int));
	memcpy(cfg->antiflick_color_filter_n2, antiflick_color_filter_n2,
	       4 * sizeof(unsigned int));
	memcpy(cfg->antiflick_color_filter_n3, antiflick_color_filter_n3,
	       4 * sizeof(unsigned int));
	memcpy(cfg->antiflick_color_filter_th, antiflick_color_filter_th,
	       3 * sizeof(unsigned int));
	memcpy(cfg->antiflick_alpha_filter_n1, antiflick_alpha_filter_n1,
	       4 * sizeof(unsigned int));
	memcpy(cfg->antiflick_alpha_filter_n2, antiflick_alpha_filter_n2,
	       4 * sizeof(unsigned int));
	memcpy(cfg->antiflick_alpha_filter_n3, antiflick_alpha_filter_n3,
	       4 * sizeof(unsigned int));
	memcpy(cfg->antiflick_alpha_filter_th, antiflick_alpha_filter_th,
	       3 * sizeof(unsigned int));
	cfg->src1_vsc_bank_length = 4;
	cfg->src1_hsc_bank_length = 4;
	ge2d_reg_set_bits(mask | GE2D_SC_MISC_CTRL,
			  ((cfg->src1_hsc_rpt_ctrl << 9) |
			  (cfg->src1_vsc_rpt_ctrl << 8) |
			  (cfg->src1_vsc_phase0_always_en << 7) |
			  (cfg->src1_vsc_bank_length << 4) |
			  (cfg->src1_hsc_phase0_always_en << 3) |
			  (cfg->src1_hsc_bank_length << 0)),  0, 10);

	ge2d_reg_set_bits(mask | GE2D_SC_MISC_CTRL,
			  ((cfg->src1_vsc_nearest_en << 1) |
			  (cfg->src1_hsc_nearest_en << 0)), 29, 2);
	if (cfg->antiflick_en == 1) {
		/* Wr(GE2D_ANTIFLICK_CTRL0, 0x81000100); */
		ge2d_reg_write(mask | GE2D_ANTIFLICK_CTRL0, 0x80000000);
		ge2d_reg_write
			(mask | GE2D_ANTIFLICK_CTRL1,
			 (cfg->antiflick_ycbcr_rgb_sel << 25) |
			 (cfg->antiflick_cbcr_en << 24) |
			 ((cfg->antiflick_r_coef & 0xff) << 16) |
			 ((cfg->antiflick_g_coef & 0xff) << 8) |
			 ((cfg->antiflick_b_coef & 0xff) << 0)
			 );

		ge2d_reg_write
			(mask | GE2D_ANTIFLICK_COLOR_FILT0,
			 ((cfg->antiflick_color_filter_th[0] & 0xff) << 24) |
			 ((cfg->antiflick_color_filter_n3[0] & 0xff) << 16) |
			 ((cfg->antiflick_color_filter_n2[0] & 0xff) << 8) |
			 ((cfg->antiflick_color_filter_n1[0] & 0xff) << 0)
			 );

		ge2d_reg_write
			(mask | GE2D_ANTIFLICK_COLOR_FILT1,
			 ((cfg->antiflick_color_filter_th[1] & 0xff) << 24) |
			 ((cfg->antiflick_color_filter_n3[1] & 0xff) << 16) |
			 ((cfg->antiflick_color_filter_n2[1] & 0xff) << 8) |
			 ((cfg->antiflick_color_filter_n1[1] & 0xff) << 0)
			 );

		ge2d_reg_write
			(mask | GE2D_ANTIFLICK_COLOR_FILT2,
			 ((cfg->antiflick_color_filter_th[2] & 0xff) << 24) |
			 ((cfg->antiflick_color_filter_n3[2] & 0xff) << 16) |
			 ((cfg->antiflick_color_filter_n2[2] & 0xff) << 8) |
			 ((cfg->antiflick_color_filter_n1[2] & 0xff) << 0)
			 );

		ge2d_reg_write
			(mask | GE2D_ANTIFLICK_COLOR_FILT3,
			 ((cfg->antiflick_color_filter_n3[3] & 0xff) << 16) |
			 ((cfg->antiflick_color_filter_n2[3] & 0xff) << 8) |
			 ((cfg->antiflick_color_filter_n1[3] & 0xff) << 0)
			 );

		ge2d_reg_write
			(mask | GE2D_ANTIFLICK_ALPHA_FILT0,
			 ((cfg->antiflick_alpha_filter_th[0] & 0xff) << 24) |
			 ((cfg->antiflick_alpha_filter_n3[0] & 0xff) << 16) |
			 ((cfg->antiflick_alpha_filter_n2[0] & 0xff) << 8) |
			 ((cfg->antiflick_alpha_filter_n1[0] & 0xff) << 0)
			 );

		ge2d_reg_write
			(mask | GE2D_ANTIFLICK_ALPHA_FILT1,
			 ((cfg->antiflick_alpha_filter_th[1] & 0xff) << 24) |
			 ((cfg->antiflick_alpha_filter_n3[1] & 0xff) << 16) |
			 ((cfg->antiflick_alpha_filter_n2[1] & 0xff) << 8) |
			 ((cfg->antiflick_alpha_filter_n1[1] & 0xff) << 0)
			 );

		ge2d_reg_write
			(mask | GE2D_ANTIFLICK_ALPHA_FILT2,
			 ((cfg->antiflick_alpha_filter_th[2] & 0xff) << 24) |
			 ((cfg->antiflick_alpha_filter_n3[2] & 0xff) << 16) |
			 ((cfg->antiflick_alpha_filter_n2[2] & 0xff) << 8) |
			 ((cfg->antiflick_alpha_filter_n1[2] & 0xff) << 0)
			 );

		ge2d_reg_write
			(mask | GE2D_ANTIFLICK_ALPHA_FILT3,
			 ((cfg->antiflick_alpha_filter_n3[3] & 0xff) << 16) |
			 ((cfg->antiflick_alpha_filter_n2[3] & 0xff) << 8) |
			 ((cfg->antiflick_alpha_filter_n1[3] & 0xff) << 0)
			 );
	} else {
		ge2d_reg_set_bits(mask | GE2D_ANTIFLICK_CTRL0, 0, 31, 1);
	}

	if (ge2d_meson_dev.adv_matrix)
		matrix_total = 3;

	for (i = 0; i < matrix_total; i++) {
		switch (i) {
		case 0:
			matrix_using = cfg->use_matrix_default;
			matrix_en    = cfg->conv_matrix_en;
			matrix_base_addr = GE2D_MATRIX_PRE_OFFSET;
			break;
		case 1:
			matrix_using = cfg->use_matrix_default_src2;
			matrix_en    = cfg->conv_matrix_en_src2;
			matrix_base_addr = GE2D_MATRIX2_PRE_OFFSET;
			break;
		case 2:
			matrix_using = cfg->use_matrix_default_dst;
			matrix_en    = cfg->conv_matrix_en_dst;
			matrix_base_addr = GE2D_MATRIX3_PRE_OFFSET;
			break;
		}
		matrix_pre_offset  = matrix_base_addr++;
		matrix_coef00_01   = matrix_base_addr++;
		matrix_coef02_10   = matrix_base_addr++;
		matrix_coef11_12   = matrix_base_addr++;
		matrix_coef20_21   = matrix_base_addr++;
		matrix_coef22_ctrl = matrix_base_addr++;
		matrix_offset      = matrix_base_addr;

		if (matrix_using & MATRIX_CUSTOM) {
			struct ge2d_matrix_s *matrix = &config->matrix_custom;

			ge2d_log_dbg("using matrix_custom\n");
			cfg->matrix_coef[0] = matrix->coef0;
			cfg->matrix_coef[1] = matrix->coef1;
			cfg->matrix_coef[2] = matrix->coef2;
			cfg->matrix_coef[3] = matrix->coef3;
			cfg->matrix_coef[4] = matrix->coef4;
			cfg->matrix_coef[5] = matrix->coef5;
			cfg->matrix_coef[6] = matrix->coef6;
			cfg->matrix_coef[7] = matrix->coef7;
			cfg->matrix_coef[8] = matrix->coef8;
			cfg->matrix_offset[0] = matrix->offset0;
			cfg->matrix_offset[1] = matrix->offset1;
			cfg->matrix_offset[2] = matrix->offset2;
			cfg->matrix_sat_in_en = matrix->sat_in_en;

			ge2d_reg_write(mask | matrix_pre_offset,
				       (matrix->pre_offset0 << 20) |
				       (matrix->pre_offset1 << 10) |
				       matrix->pre_offset2);
		} else {
			if (matrix_using & MATRIX_YCC_TO_RGB) {
				/* ycbcr(16-235) to rgb(0-255) */
				cfg->matrix_coef[0] = 0x4a8;
				cfg->matrix_coef[1] = 0;
				cfg->matrix_coef[2] = 0x662;
				cfg->matrix_coef[3] = 0x4a8;
				cfg->matrix_coef[4] = 0x1e6f;
				cfg->matrix_coef[5] = 0x1cbf;
				cfg->matrix_coef[6] = 0x4a8;
				cfg->matrix_coef[7] = 0x811;
				cfg->matrix_coef[8] = 0x0;
				cfg->matrix_offset[0] = 0;
				cfg->matrix_offset[1] = 0;
				cfg->matrix_offset[2] = 0;
				cfg->matrix_sat_in_en = 1;
				cfg->matrix_minus_16_ctrl = 0x4;
				cfg->matrix_sign_ctrl = 0x3;
			} else if (matrix_using & MATRIX_RGB_TO_YCC) {
				if (matrix_using & MATRIX_BT_709) {
					/* VDIN_MATRIX_RGB_YUV709 */
					/* 0     0.183  0.614  0.062     16 */
					/* 0    -0.101 -0.338  0.439    128 */
					/* 0     0.439 -0.399 -0.04     128 */
					cfg->matrix_coef[0] = 0xbb;
					cfg->matrix_coef[1] = 0x275;
					cfg->matrix_coef[2] = 0x3f;
					cfg->matrix_coef[3] = 0x1f99;
					cfg->matrix_coef[4] = 0x1ea6;
					cfg->matrix_coef[5] = 0x1c2;
					cfg->matrix_coef[6] = 0x1c2;
					cfg->matrix_coef[7] = 0x1e67;
					cfg->matrix_coef[8] = 0x1fd7;
				} else {
					/* rgb(0-255) to ycbcr(16-235) */
					/* 0.257     0.504   0.098 */
					/* -0.148    -0.291  0.439 */
					/* 0.439     -0.368 -0.071 */
					cfg->matrix_coef[0] = 0x107;
					cfg->matrix_coef[1] = 0x204;
					cfg->matrix_coef[2] = 0x64;
					cfg->matrix_coef[3] = 0x1f68;
					cfg->matrix_coef[4] = 0x1ed6;
					cfg->matrix_coef[5] = 0x1c2;
					cfg->matrix_coef[6] = 0x1c2;
					cfg->matrix_coef[7] = 0x1e87;
					cfg->matrix_coef[8] = 0x1fb7;
				}
				cfg->matrix_offset[0] = 16;
				cfg->matrix_offset[1] = 128;
				cfg->matrix_offset[2] = 128;
				cfg->matrix_sat_in_en = 0;
				cfg->matrix_minus_16_ctrl = 0;
				cfg->matrix_sign_ctrl = 0;
			} else if (matrix_using &
				   MATRIX_FULL_RANGE_YCC_TO_RGB) {
				/* ycbcr (0-255) to rgb(0-255) */
				/* 1,     0,      1.402 */
				/* 1, -0.34414,   -0.71414 */
				/* 1, 1.772       0 */
				cfg->matrix_coef[0] = 0x400;
				cfg->matrix_coef[1] = 0;
				cfg->matrix_coef[2] = 0x59c;
				cfg->matrix_coef[3] = 0x400;
				cfg->matrix_coef[4] = 0x1ea0;
				cfg->matrix_coef[5] = 0x1d25;
				cfg->matrix_coef[6] = 0x400;
				cfg->matrix_coef[7] = 0x717;
				cfg->matrix_coef[8] = 0;
				cfg->matrix_offset[0] = 0;
				cfg->matrix_offset[1] = 0;
				cfg->matrix_offset[2] = 0;
				cfg->matrix_sat_in_en = 0;
				cfg->matrix_minus_16_ctrl = 0;
				cfg->matrix_sign_ctrl = 0x3;
			} else if (matrix_using &
				   MATRIX_RGB_TO_FULL_RANGE_YCC) {
				cfg->matrix_coef[0] = 0x132;
				cfg->matrix_coef[1] = 0x259;
				cfg->matrix_coef[2] = 0x75;
				cfg->matrix_coef[3] = 0x1f53;
				cfg->matrix_coef[4] = 0x1ead;
				cfg->matrix_coef[5] = 0x200;
				cfg->matrix_coef[6] = 0x200;
				cfg->matrix_coef[7] = 0x1e53;
				cfg->matrix_coef[8] = 0x1fad;
				cfg->matrix_offset[0] = 0;
				cfg->matrix_offset[1] = 128;
				cfg->matrix_offset[2] = 128;
				cfg->matrix_sat_in_en = 0;
				cfg->matrix_minus_16_ctrl = 0;
				cfg->matrix_sign_ctrl = 0;
			} else if (matrix_using & MATRIX_SIGNED) {
				cfg->matrix_coef[0] = 0x400;
				cfg->matrix_coef[1] = 0;
				cfg->matrix_coef[2] = 0;
				cfg->matrix_coef[3] = 0;
				cfg->matrix_coef[4] = 0x400;
				cfg->matrix_coef[5] = 0;
				cfg->matrix_coef[6] = 0;
				cfg->matrix_coef[7] = 0;
				cfg->matrix_coef[8] = 0x400;
				cfg->matrix_offset[0] = 384; /* -128 */
				cfg->matrix_offset[1] = 384; /* -128 */
				cfg->matrix_offset[2] = 384; /* -128 */
				cfg->matrix_sat_in_en = 0;
				cfg->matrix_minus_16_ctrl = 0;
				cfg->matrix_sign_ctrl = 0;
			}

			if (cfg->matrix_minus_16_ctrl)
				ge2d_reg_set_bits(mask | matrix_pre_offset,
						  0x1f0, 20, 9);
			else
				ge2d_reg_set_bits(mask | matrix_pre_offset,
						  0, 20, 9);

			if (cfg->matrix_sign_ctrl & 3)
				ge2d_reg_set_bits(mask | matrix_pre_offset,
						  ((0x180 << 10) | 0x180),
						  0, 20);
			else
				ge2d_reg_set_bits(mask | matrix_pre_offset,
						  0, 0, 20);
		}

		ge2d_reg_write(mask | matrix_coef00_01,
			       (cfg->matrix_coef[0] << 16) |
			       (cfg->matrix_coef[1] << 0)
			       );

		ge2d_reg_write(mask | matrix_coef02_10,
			       (cfg->matrix_coef[2] << 16) |
			       (cfg->matrix_coef[3] << 0)
			       );

		ge2d_reg_write(mask | matrix_coef11_12,
			       (cfg->matrix_coef[4] << 16) |
			       (cfg->matrix_coef[5] << 0)
			       );

		ge2d_reg_write(mask | matrix_coef20_21,
			       (cfg->matrix_coef[6] << 16) |
			       (cfg->matrix_coef[7] << 0)
			       );

		ge2d_reg_write(mask | matrix_coef22_ctrl,
			       (cfg->matrix_coef[8] << 16) |
			       (cfg->matrix_sat_in_en << 7) |
			       (matrix_en << 0)
			       );

		ge2d_reg_write(mask | matrix_offset,
			       (cfg->matrix_offset[0] << 20) |
			       (cfg->matrix_offset[1] << 10) |
			       (cfg->matrix_offset[2] << 0)
			       );
	}

	if (ge2d_meson_dev.dst_sign_mode && cfg->conv_matrix_en_dst)
		ge2d_reg_set_bits(GE2D_MATRIX3_COEF22_CTRL,
				  cfg->dst_signed_mode, 6, 1);

	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL1, cfg->src1_gb_alpha, 0, 8);
	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL2,
			  cfg->src1_gb_alpha_en, 29, 1);
#ifdef CONFIG_GE2D_SRC2
	if (ge2d_meson_dev.src2_alp == 1) {
		ge2d_reg_set_bits(mask | GE2D_GEN_CTRL5,
				  cfg->src2_gb_alpha, 0, 8);
		ge2d_reg_set_bits(mask | GE2D_GEN_CTRL5,
				  cfg->src2_gb_alpha_en, 8, 1);
	}
#endif
	ge2d_reg_write(mask | GE2D_ALU_CONST_COLOR, cfg->alu_const_color);
	ge2d_reg_write(mask | GE2D_SRC1_KEY, cfg->src1_key);
	ge2d_reg_write(mask | GE2D_SRC1_KEY_MASK, cfg->src1_key_mask);

	ge2d_reg_write(mask | GE2D_SRC2_KEY, cfg->src2_key);
	ge2d_reg_write(mask | GE2D_SRC2_KEY_MASK, cfg->src2_key_mask);

	ge2d_reg_write(mask | GE2D_DST_BITMASK, cfg->bitmask);

	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL0,
			  ((cfg->bytemask_only << 5) |
			  (cfg->bitmask_en << 4) |
			  (cfg->src2_key_en << 3) |
			  (cfg->src2_key_mode << 2) |
			  (cfg->src1_key_en << 1) |
			  (cfg->src1_key_mode << 0)), 26, 6);
}

int ge2d_cmd_fifo_full(void)
{
	return ge2d_reg_read(GE2D_STATUS0) & (1 << 1);
}

void ge2d_set_cmd(struct ge2d_cmd_s *cfg, u32 mask)
{
	unsigned int widthi, heighti, tmp_widthi, tmp_heighti, widtho, heighto;
	unsigned int multo;
	unsigned int x_extra_bit_start = 0, x_extra_bit_end = 0;
	unsigned int y_extra_bit_start = 0, y_extra_bit_end = 0;
	unsigned int x_chr_phase = 0, y_chr_phase = 0;
	unsigned int x_yc_ratio, y_yc_ratio;
	unsigned int src2_x_interp_ctrl = 0; /* interpolation mode  */
	unsigned int src2_x_repeat = 0, src2_y_repeat = 0;
	int sc_prehsc_en, sc_prevsc_en;
	int rate_w = 10, rate_h = 10;
	/* expand src region with one line. */
	unsigned int src1_y_end = cfg->src1_y_end + 1;
	struct ge2d_queue_item_s *queue_item = NULL;
	unsigned int mem_secure = 0;
	unsigned int cmd_queue_reg = 0;

	while ((ge2d_reg_read(mask | GE2D_STATUS0) & (1 << 1)))
		;

	x_yc_ratio = ge2d_reg_get_bits(mask | GE2D_GEN_CTRL0, 11, 1);
	y_yc_ratio = ge2d_reg_get_bits(mask | GE2D_GEN_CTRL0, 10, 1);

	/* src:yuv , dst: rgb */
	if ((cfg->src1_fmt & GE2D_FORMAT_YUV) &&
	    ((cfg->dst_fmt & GE2D_FORMAT_YUV) == 0)) {
		if (x_yc_ratio) {
			if ((cfg->src1_x_rev + cfg->dst_x_rev) == 1) {
				x_extra_bit_start = 3;
				x_extra_bit_end   = 2;
				x_chr_phase = 0x4c;
			} else {
				x_extra_bit_start = 2;
				x_extra_bit_end   = 3;
				x_chr_phase = 0xc4;
			}
		}
		if (y_yc_ratio) {
			if ((cfg->src1_y_rev + cfg->dst_y_rev) == 1) {
				y_extra_bit_start = 3;
				y_extra_bit_end   = 2;
				y_chr_phase = 0x4c;
			} else {
				y_extra_bit_start = 2;
				y_extra_bit_end   = 3;
				y_chr_phase = 0xc4;
			}
		}
	} else {
		if (x_yc_ratio) {
			if ((cfg->src1_x_rev + cfg->dst_x_rev) == 1) {
				x_extra_bit_start = 3;
				x_extra_bit_end   = 2;
				x_chr_phase = 0x08;
			} else {
				x_extra_bit_start = 2;
				x_extra_bit_end   = 3;
				x_chr_phase = 0x08;
			}
		}

		if (y_yc_ratio) {
			if ((cfg->src1_y_rev + cfg->dst_y_rev) == 1) {
				y_extra_bit_start = 3;
				y_extra_bit_end   = 2;
				y_chr_phase = 0x4c;
			} else {
				y_extra_bit_start = 2;
				y_extra_bit_end   = 3;
				y_chr_phase = 0x4c;
			}
		}
	}
	ge2d_reg_write(mask | GE2D_SRC1_X_START_END,
		       (x_extra_bit_start << 30) |  /* x start extra */
		       ((cfg->src1_x_start & 0x3fff) << 16) |
		       (x_extra_bit_end << 14) |    /* x end extra */
		       ((cfg->src1_x_end & 0x3fff) << 0)
		       );

	ge2d_reg_write(mask | GE2D_SRC1_Y_START_END,
		       (y_extra_bit_start << 30) |  /* y start extra */
		       ((cfg->src1_y_start & 0x3fff) << 16) |
		       (y_extra_bit_end << 14) |    /* y end extra */
		       ((src1_y_end & 0x3fff) << 0)
		       );

	ge2d_reg_set_bits(mask | GE2D_SRC1_FMT_CTRL, x_chr_phase, 8, 8);
	ge2d_reg_set_bits(mask | GE2D_SRC1_FMT_CTRL, y_chr_phase, 0, 8);

	if (((cfg->src1_x_end - cfg->src1_x_start) ==
	     (cfg->dst_x_end - cfg->dst_x_start)) &&
	    ((cfg->src1_y_end - cfg->src1_y_start) ==
	     (cfg->dst_y_end - cfg->dst_y_start))) {
		/* set chroma formatter repeat mode */
		ge2d_reg_set_bits(mask | GE2D_SRC1_FMT_CTRL, 1, 19, 1);
		ge2d_reg_set_bits(mask | GE2D_SRC1_FMT_CTRL, 1, 17, 1);
	}

	/* src1 scaler setting */
	widthi  = cfg->src1_x_end - cfg->src1_x_start + 1;
	heighti = cfg->src1_y_end - cfg->src1_y_start + 1;

	widtho  = cfg->dst_xy_swap ? (cfg->dst_y_end - cfg->dst_y_start + 1) :
		  (cfg->dst_x_end - cfg->dst_x_start + 1);
	heighto = cfg->dst_xy_swap ? (cfg->dst_x_end - cfg->dst_x_start + 1) :
		  (cfg->dst_y_end - cfg->dst_y_start + 1);

	sc_prehsc_en = (widthi > widtho * 2) ? 1 : 0;
	sc_prevsc_en = (heighti > heighto * 2) ? 1 : 0;

	tmp_widthi = sc_prehsc_en ? ((widthi + 1) >> 1) : widthi;
	tmp_heighti = sc_prevsc_en ? ((heighti + 1) >> 1) : heighti;

	if (cfg->hsc_phase_step == 0)
		cfg->hsc_phase_step = ((tmp_widthi << 18) / widtho) <<
					6; /* width no more than 8192 */

	if (cfg->vsc_phase_step == 0)
		cfg->vsc_phase_step = ((tmp_heighti << 18) / heighto) <<
					6;/* height no more than 8192 */

	if (cfg->sc_hsc_en && cfg->hsc_div_en) {
		unsigned int hsc_div_length;

		if (ge2d_meson_dev.chip_type < MESON_CPU_MAJOR_ID_G12B)
			hsc_div_length = (120 << 24) / cfg->hsc_phase_step;
		else
			hsc_div_length = (124 << 24) / cfg->hsc_phase_step;

		/* in blend case, for chip after C2
		 * src2 repeat function needs hsc_div_length 8 alignment
		 */
		if (cfg->cmd_op == IS_BLEND && ge2d_meson_dev.src2_repeat)
			hsc_div_length = (hsc_div_length + 7) & ~7;
		cfg->hsc_div_length = hsc_div_length;
		multo = cfg->hsc_phase_step * cfg->hsc_div_length;
		cfg->hsc_adv_num   = multo >> 24;
		cfg->hsc_adv_phase = multo & 0xffffff;
	}

	if (!gaul_filter_used) {
		if (widthi == 0 || heighti == 0) {
			ge2d_log_err("wrong parameters, widthi=%d,heighti=%d\n",
				     widthi, heighti);
			return;
		}

		rate_w = (widtho * 10) / widthi;
		rate_h = (heighto * 10) / heighti;
		if (rate_h == 10) {
			/* not scaler case */
			cfg->vsc_ini_phase = 0;
			ge2d_reg_set_bits(mask | GE2D_SC_MISC_CTRL,
					  ((0 << 1) | (0 << 0)), 8, 2);
		} else if (rate_h < 10) {
			/* scaler down case */
			cfg->sc_vsc_en = 1;
			cfg->vsc_rpt_l0_num = 1;
			if (rate_h != 0)
				cfg->vsc_ini_phase =
					0x5000000 / rate_h - 0x800000;
			else
				cfg->vsc_ini_phase = 0x5000000;
		} else {
			/* scaler up case */
			cfg->sc_vsc_en = 1;
			cfg->vsc_rpt_l0_num = 2;
			cfg->vsc_ini_phase =
				0x800000 + 0x5000000 / rate_h;
		}

		if (rate_w == 10) {
			/* not scaler case */
			cfg->hsc_ini_phase = 0;
			ge2d_reg_set_bits(mask | GE2D_SC_MISC_CTRL,
					  ((0 << 1) | (0 << 0)), 8, 2);
		} else if (rate_w < 10) {
			/* scaler down case */
			cfg->sc_hsc_en = 1;
			cfg->hsc_rpt_p0_num = 1;
			if (rate_w != 0)
				cfg->hsc_ini_phase =
					0x5000000 / rate_w - 0x800000;
			else
				cfg->hsc_ini_phase = 0x5000000;
		} else {
			/* scaler up case */
			cfg->sc_hsc_en = 1;
			cfg->hsc_rpt_p0_num = 2;
			cfg->hsc_ini_phase =
				0x800000 + 0x5000000 / rate_w;
		}
		/* expand src1/src2 color with 1 */
		ge2d_reg_set_bits(mask | GE2D_GEN_CTRL2, 1, 27, 1);
		ge2d_reg_set_bits(mask | GE2D_GEN_CTRL2, 1, 25, 1);
	}
	ge2d_log_dbg("rate_w=%d,rate_h=%d\n", rate_w, rate_h);
	ge2d_reg_set_bits(mask | GE2D_SC_MISC_CTRL,
			  ((cfg->hsc_div_en << 17) |
			  (cfg->hsc_div_length << 4) |
			  (sc_prehsc_en << 3) |
			  (sc_prevsc_en << 2) |
			  (cfg->sc_vsc_en << 1) |
			  (cfg->sc_hsc_en << 0)), 11, 18);

	ge2d_reg_write(mask | GE2D_HSC_START_PHASE_STEP, cfg->hsc_phase_step);

	ge2d_reg_write(mask | GE2D_HSC_PHASE_SLOPE, cfg->hsc_phase_slope);

#ifdef CONFIG_GE2D_ADV_NUM
	ge2d_reg_write(mask | GE2D_HSC_ADV_CTRL,
		       (cfg->hsc_adv_num << 24) |
		       (cfg->hsc_adv_phase << 0)
		       );
	if (cfg->hsc_adv_num > 255)
		cfg->hsc_adv_num = cfg->hsc_adv_num >> 8;
	else
		cfg->hsc_adv_num = 0;
	ge2d_reg_write(mask | GE2D_HSC_INI_CTRL,
		       (cfg->hsc_rpt_p0_num << 29) |
		       (cfg->hsc_adv_num << 24) |
		       ((cfg->hsc_ini_phase & 0xffffff) << 0)
		       );
#else
	ge2d_reg_write(mask | GE2D_HSC_ADV_CTRL,
		       (cfg->hsc_adv_num << 24) |
		       (cfg->hsc_adv_phase << 0)
		       );
	ge2d_reg_write(mask | GE2D_HSC_INI_CTRL,
		       (cfg->hsc_rpt_p0_num << 29) |
		       ((cfg->hsc_ini_phase & 0xffffff) << 0)
		       );
#endif

	ge2d_reg_write(mask | GE2D_VSC_START_PHASE_STEP, cfg->vsc_phase_step);

	ge2d_reg_write(mask | GE2D_VSC_PHASE_SLOPE, cfg->vsc_phase_slope);

	ge2d_reg_write(mask | GE2D_VSC_INI_CTRL,
		       (cfg->vsc_rpt_l0_num << 29) |
		       (cfg->vsc_ini_phase << 0)
		       );

	/* src2 scaler setting, just support 2^n repeat (n = 0, 1, 2 or 3) */
	if (cfg->cmd_op == IS_BLEND && ge2d_meson_dev.src2_repeat) {
		widthi  = cfg->src2_x_end - cfg->src2_x_start + 1;
		heighti = cfg->src2_y_end - cfg->src2_y_start + 1;

		if (widthi == 0 || heighti == 0) {
			ge2d_log_err("wrong parameters, widthi=%d,heighti=%d\n",
				     widthi, heighti);
			return;
		}

		switch (widtho / widthi) {
		case 0:
			ge2d_log_err("src2_x scale down is not supported\n");
			src2_x_interp_ctrl = 0;
			src2_x_repeat = 0;
			break;
		case 1:
			src2_x_interp_ctrl = 0;
			src2_x_repeat = 0;
			break;
		case 2:
			src2_x_interp_ctrl = 1;
			src2_x_repeat = 1;
			break;
		case 4:
			src2_x_interp_ctrl = 1;
			src2_x_repeat = 2;
			break;
		case 8:
			src2_x_interp_ctrl = 1;
			src2_x_repeat = 3;
			break;
		default:
			src2_x_interp_ctrl = 1;
			src2_x_repeat = 3;
			ge2d_log_err("src2_x scale up just support 2^n\n");
			break;
		}

		switch (heighto / heighti) {
		case 0:
			ge2d_log_err("src2_y scale down is not supported\n");
			src2_y_repeat = 0;
			break;
		case 1:
			src2_y_repeat = 0;
			break;
		case 2:
			src2_y_repeat = 1;
			break;
		case 4:
			src2_y_repeat = 2;
			break;
		case 8:
			src2_y_repeat = 3;
			break;
		default:
			src2_y_repeat = 3;
			ge2d_log_err("src2_y scale up just support 2^n\n");
			break;
		}
		ge2d_log_dbg("src2_x_interp_ctrl(%d), src2_x_repeat(%d)\n",
			     src2_x_interp_ctrl, src2_x_repeat);
		ge2d_log_dbg("src2_y_repeat(%d)\n", src2_y_repeat);
	}

	/* in blend case, if src2_repeat is support, use output W H to set */
	if (cfg->cmd_op == IS_BLEND && ge2d_meson_dev.src2_repeat) {
		unsigned int x_rpt = src2_x_repeat;
		unsigned int y_rpt = src2_x_repeat;
		unsigned int x_start = 0, x_end = 0, y_start = 0, y_end = 0;

		x_start = cfg->src2_x_start * (1 << x_rpt);
		x_end   = x_start + widthi * (1 << x_rpt) - 1;
		y_start = cfg->src2_y_start * (1 << y_rpt);
		y_end   = y_start + heighti * (1 << y_rpt) - 1;

		ge2d_log_dbg("src2_x setting, start:%d end:%d\n",
			     x_start, x_end);
		ge2d_log_dbg("src2_y setting, start:%d end:%d\n",
			     y_start, y_end);
		ge2d_reg_write(mask | GE2D_SRC2_X_START_END,
			       (x_start << 16) |
			       (x_end << 0)
			       );

		ge2d_reg_write(mask | GE2D_SRC2_Y_START_END,
			       (y_start << 16) |
			       (y_end << 0)
			       );
	} else {
		ge2d_reg_write(mask | GE2D_SRC2_X_START_END,
			       (cfg->src2_x_start << 16) |
			       (cfg->src2_x_end << 0)
			       );

		ge2d_reg_write(mask | GE2D_SRC2_Y_START_END,
			       (cfg->src2_y_start << 16) |
			       (cfg->src2_y_end << 0)
			       );
	}
	ge2d_reg_write(mask | GE2D_DST_X_START_END,
		       (cfg->dst_x_start << 16) |
		       (cfg->dst_x_end << 0)
		       );

	ge2d_reg_write(mask | GE2D_DST_Y_START_END,
		       (cfg->dst_y_start << 16) |
		       (cfg->dst_y_end << 0)
		       );
#ifdef CONFIG_GE2D_SRC2
	if (ge2d_meson_dev.src2_alp == 1)
		ge2d_reg_write(mask | GE2D_ALU_OP_CTRL,
			       (cfg->src2_cmult_ad << 27) |
			       (cfg->src1_cmult_asel << 25) |
			       (cfg->src2_cmult_asel << 23) |
			       (cfg->color_blend_mode << 20) |
			       (cfg->color_src_blend_factor << 16) |
			       (((cfg->color_blend_mode == 5) ?
			       cfg->color_logic_op :
			       cfg->color_dst_blend_factor) << 12) |
			       (cfg->alpha_blend_mode << 8) |
			       (cfg->alpha_src_blend_factor << 4) |
			       (((cfg->alpha_blend_mode == 5) ?
			       cfg->alpha_logic_op :
			       cfg->alpha_dst_blend_factor) << 0)
			       );
	else
#endif
		ge2d_reg_write(mask | GE2D_ALU_OP_CTRL,
			       (cfg->src1_cmult_asel << 25) |
			       (cfg->src2_cmult_asel << 24) |
			       (cfg->color_blend_mode << 20) |
			       (cfg->color_src_blend_factor << 16) |
			       (((cfg->color_blend_mode == 5) ?
			       cfg->color_logic_op :
			       cfg->color_dst_blend_factor) << 12) |
			       (cfg->alpha_blend_mode << 8) |
			       (cfg->alpha_src_blend_factor << 4) |
			       (((cfg->alpha_blend_mode == 5) ?
			       cfg->alpha_logic_op :
			       cfg->alpha_dst_blend_factor) << 0)
			       );

	/* if true, disable bug fix about the dp_out_done/
	 * scale_out_done(test1823) hang issue when
	 * scaling down ratio is high.
	 */
	if (ge2d_meson_dev.hang_flag == 1)
		ge2d_reg_set_bits(mask | GE2D_GEN_CTRL4, cfg->hang_flag, 0, 1);

	if (is_queue(mask))
		cmd_queue_reg = 1;
	ge2d_reg_set_bits(mask | GE2D_GEN_CTRL4, cmd_queue_reg, 29, 1);
	/* memory security setting */
	queue_item = container_of(cfg, struct ge2d_queue_item_s, cmd);
	if (ge2d_meson_dev.chip_type >= MESON_CPU_MAJOR_ID_SC2)
		mem_secure = queue_item->config.mem_sec;
	ge2d_log_dbg("secure mode:%d\n", mem_secure);

	ge2d_reg_write(mask | GE2D_CMD_CTRL,
		       (mem_secure << 28) |
		       (src2_x_interp_ctrl << 14) |
		       (src2_x_repeat << 12) |
		       (src2_y_repeat << 10) |
		       (cfg->src2_fill_color_en << 9) |
		       (cfg->src1_fill_color_en << 8) |
		       (cfg->dst_xy_swap << 7) |
		       (cfg->dst_x_rev << 6) |
		       (cfg->dst_y_rev << 5) |
		       (cfg->src2_x_rev << 4) |
		       (cfg->src2_y_rev << 3) |
		       (cfg->src1_x_rev << 2) |
		       (cfg->src1_y_rev << 1) |
		       1  << 0 /* start cmd */
		       );
	cfg->release_flag |= START_FLAG;
}

void ge2d_wait_done(void)
{
	while (ge2d_reg_read(GE2D_STATUS0) & 1)
		;
}

unsigned int ge2d_queue_cnt(void)
{
	return ge2d_reg_read(GE2D_AXI2DMA_STATUS);
}

bool ge2d_queue_empty(void)
{
	int ret = false;

	/* cmd queue is empty */
	if (!ge2d_queue_cnt())
		ret = true;

	return ret;
}

bool ge2d_is_busy(void)
{
	if (ge2d_reg_read(GE2D_STATUS0) & 1)
		return true;
	else
		return false;
}

void ge2d_soft_rst(void)
{
	ge2d_reg_set_bits(GE2D_GEN_CTRL1, 1, 31, 1);
	ge2d_reg_set_bits(GE2D_GEN_CTRL1, 0, 31, 1);
}

void ge2d_dma_reset(void)
{
	ge2d_reg_set_bits(GE2D_AXI2DMA_CTRL0, 1, 29, 1);
	ge2d_reg_set_bits(GE2D_AXI2DMA_CTRL0, 0, 29, 1);
}

void ge2d_set_gen(struct ge2d_gen_s *cfg)
{
	ge2d_reg_set_bits(GE2D_GEN_CTRL1, cfg->interrupt_ctrl, 24, 2);

	ge2d_reg_write(GE2D_DP_ONOFF_CTRL,
		       (cfg->dp_onoff_mode << 31) |
		       (cfg->dp_on_cnt << 16) |
		       (cfg->vfmt_onoff_en << 15) |
		       (cfg->dp_off_cnt << 0)
		       );
	if (ge2d_meson_dev.fifo == 1) {
		ge2d_reg_set_bits(GE2D_GEN_CTRL4,
				  (cfg->bytes_per_burst << 12) |
				  (cfg->fifo_size << 10) |
				  (cfg->fifo_size << 8) |
				  (cfg->fifo_size << 6) |
				  (cfg->fifo_size << 4) |
				  (cfg->burst_ctrl << 2) |
				  (cfg->burst_ctrl),
				  16, 13);
	}
}

void switch_cmd_queue_irq(u32 queue_index, u32 enable)
{
	union ge2d_reg_bit_u convert;
	u32 interrupt_ctrl = 0;

	/* generate interrupt when ge2d change from busy to not busy */
	if (enable)
		interrupt_ctrl = 2;

	convert.reg_bit.is_queue = 1;
	convert.reg_bit.queue_index = queue_index;

	ge2d_reg_set_bits(convert.val | GE2D_GEN_CTRL1,
			  interrupt_ctrl, 24, 2);
}

void ge2d_backup_initial_regs(void __iomem *to_buf)
{
	int i;
	u32 offset = 0;
	u32 *dst = (u32 *)to_buf;

	for (i = GE2D_REG_START; i <= GE2D_REG_END; i++) {
		*(dst + offset * 2) = ge2d_reg_read(i);
		*(dst + offset * 2 + 1) = i;
		offset++;
	}

	/* replace excluded regs */
	for (i = 0; i < ARRAY_SIZE(excluded_regs); i++) {
		offset = excluded_regs[i] - GE2D_REG_START;
		*(dst + offset * 2) = 0;
		*(dst + offset * 2 + 1) = GE2D_VOID_REG;
	}
}

void init_cmd_queue_buf(u32 queue_index)
{
	u32 offset = ONE_CMD_BUF_SIZE * (queue_index - 1);

	memcpy(cmd_queue_vaddr + offset, backup_init_reg_vaddr, ONE_CMD_BUF_SIZE);
}

/* switch position
 * put GE2D_CMD_CTRL in the tail
 */
void adjust_cmd_queue_buf(u32 queue_index)
{
	u32 tmp[2], offset1, offset2;
	u32 *start_addr = (unsigned int *)cmd_queue_vaddr;
	u32 *addr_src, *addr_dst;

	offset1 = GE2D_REG_CNT * (queue_index - 1) +
			GE2D_CMD_CTRL - GE2D_REG_START;
	offset2 = GE2D_REG_CNT * (queue_index - 1) +
			GE2D_REG_END - GE2D_REG_START;

	addr_src = start_addr + offset1 * 2;
	addr_dst = start_addr + offset2 * 2;

	memcpy(tmp, addr_src, sizeof(u32) * 2);
	memcpy(addr_src, addr_dst, sizeof(u32) * 2);
	memcpy(addr_dst, tmp, sizeof(u32) * 2);
}

void start_cmd_queue_process(u32 queue_cnt)
{
	u32 enable_dma = 1, enable_dma_req = 1;

	ge2d_reg_write(GE2D_AXI2DMA_CTRL2, (cmd_queue_paddr) >> 2);
	ge2d_reg_write(GE2D_AXI2DMA_CTRL1, (queue_cnt << 8) | (GE2D_REG_CNT / 2));
	ge2d_reg_write(GE2D_AXI2DMA_CTRL0,
		       (enable_dma << 31) | (enable_dma_req << 28));
}

void stop_cmd_queue_process(void)
{
	u32 enable_dma = 0, enable_dma_req = 0;

	ge2d_reg_write(GE2D_AXI2DMA_CTRL0,
		       (enable_dma << 31) | (enable_dma_req << 28));
}

void dump_cmd_queue_regs(u32 queue_index)
{
	int i, offset;
	u32 *start_addr;

	offset = GE2D_REG_CNT * (queue_index - 1);
	start_addr = (unsigned int *)cmd_queue_vaddr + offset * 2;

	pr_info("=== reg setting %d ===\n", queue_index);
	for (i = 0; i < GE2D_REG_CNT; i++) {
		pr_info("reg[0x%x]: 0x%x\n", *(start_addr + i * 2 + 1),
			*(start_addr + i * 2));
	}
}
