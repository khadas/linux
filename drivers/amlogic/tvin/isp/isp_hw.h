/*
 * ISP driver
 *
 * Author: Kele Bai <kele.bai@amlogic.com>
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 */
#ifndef __TVIN_ISP_HW_H
#define __TVIN_ISP_HW_H

#include <linux/amlogic/iomap.h>

/* #if 0 */
/* #define WR(x,val)
 *   printk("0x%x <- 0x%x.\n",x,val) */
/* #define WR_BITS(x,val,start,length)
 * printk("0x%x[%u:%u] <- 0x%x.\n",x,start,start+length-1,val) */
/* #define RD(x)
 * printk("read 0x%x.\n",x) */
/* #define RD_BITS(x,start,length)
 *  printk("read 0x%x[%u:%u].\n",x,start,start+length-1) */
/* #else */
/* #define WR(x,val)
 * WRITE_VCBUS_REG(x,val) */
/* #define WR_BITS(x,val,start,length)
 * WRITE_VCBUS_REG_BITS(x,val,start,length) */
/* #define RD(x)
 * READ_VCBUS_REG(x) */
/* #define RD_BITS(x,start,length)
 * READ_VCBUS_REG_BITS(x,start,length) */
/* #endif */


static inline void WR(uint32_t reg,
		const uint32_t value)
{
	aml_write_vcbus(reg, value);
}

static inline uint32_t RD(uint32_t reg)
{
	return aml_read_vcbus(reg);
}

static inline void WR_BITS(uint32_t reg,
		const uint32_t value,
		const uint32_t start,
		const uint32_t len)
{
	WR(reg, ((RD(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

static inline uint32_t RD_BITS(uint32_t reg,
				    const uint32_t start,
				    const uint32_t len)
{
	uint32_t val;

	val = ((RD(reg) >> (start)) & ((1L << (len)) - 1));

	return val;
}

#define	PERIPHS_PIN_MUX_5	0x2031
#define	PERIPHS_PIN_MUX_7	0x2033
#define	PERIPHS_PIN_MUX_9	0x2035

#define GAMMA_R				0x00000000
#define GAMMA_G				0x00000200
#define GAMMA_B				0x00000400

struct awb_rgb_stat_s {
	unsigned int rgb_count;
	unsigned int rgb_sum[3];/* r g b */
};

struct awb_yuv_stat_s {
	unsigned int count;
	unsigned int sum;
};

struct isp_awb_stat_s {
	struct awb_rgb_stat_s rgb; /* R G B */
	/* y < low    0, -u; 1, +u; 2, -v; 3, +v */
	struct awb_yuv_stat_s yuv_low[4];
	struct awb_yuv_stat_s yuv_mid[4];
	struct awb_yuv_stat_s yuv_high[4]; /* y > high */
};

struct isp_ae_stat_s {
	unsigned int bayer_over_info[3];/* R G B */
	unsigned int luma_win[16];
	unsigned char curstep;
	float curgain;
	float maxgain;
	float mingain;
	unsigned char maxstep;
};

struct isp_af_stat_s {
	unsigned int bayer_over_info[3];/* R G B */
	unsigned int luma_win[16];
};

struct isp_awb_gain_s {
	unsigned int r_val;
	unsigned int g_val;
	unsigned int b_val;
};

struct isp_blnr_stat_s {
	unsigned int ac[4];/* G0 R1 B2 G3 */
	unsigned int dc[4];/* G0 R1 B2 G3 */
	unsigned int af_ac[16];
};

extern void isp_wr(unsigned int addr, unsigned int value);
extern unsigned int isp_rd(unsigned int addr);
extern void isp_top_init(struct xml_top_s *top,
		unsigned int w, unsigned int h);
extern void isp_set_test_pattern(struct xml_tp_s *tp);
extern void isp_set_clamp_gain(struct xml_cg_s *cg);
extern void isp_set_lens_shading(struct xml_ls_s *ls);
void isp_set_gamma_correction(struct xml_gc_s *gc);
extern void isp_set_defect_pixel_correction(struct xml_dp_s *dp);
extern void isp_set_demosaicing(struct xml_dm_s *dm);
extern void isp_set_matrix(struct xml_csc_s *csc, unsigned int height);
extern void isp_set_sharpness(struct xml_sharp_s *sharp);
extern void isp_set_nr(struct xml_nr_s *nr);
extern void isp_set_awb_stat(struct xml_awb_s *awb,
		unsigned int w, unsigned int h);
extern void isp_set_ae_stat(struct xml_ae_s *ae,
		unsigned int w, unsigned int h);
extern void isp_set_af_stat(struct xml_af_s *af,
		unsigned int w, unsigned int h);
extern void isp_set_af_scan_stat(unsigned int x0,
		unsigned int y0, unsigned int x1, unsigned int y1);
extern void isp_set_blenr_stat(unsigned int x0,
		unsigned int y0, unsigned int x1, unsigned int y1);
extern void isp_set_dbg(struct xml_dbg_s *dbg);
extern void isp_set_lnsd_mode(unsigned int mode);
extern void isp_set_def_config(struct xml_default_regs_s *regs,
		enum tvin_port_e fe_port, enum tvin_color_fmt_e bfmt,
		unsigned int w, unsigned int h);
extern void isp_load_def_setting(unsigned int hsize,
		unsigned int vsize, unsigned char bayer_fmt);
extern void isp_test_pattern(unsigned int hsize, unsigned int vsize,
		unsigned int htotal, unsigned int vtotal,
		unsigned char bayer_fmt);
extern void isp_set_manual_wb(struct xml_wb_manual_s *wb);
extern void isp_get_awb_stat(struct isp_awb_stat_s *awb_stat);
extern void isp_get_ae_stat(struct isp_ae_stat_s *ae_stat);
extern void isp_get_af_stat(struct isp_af_stat_s *af_stat);
extern void isp_get_af_scan_stat(struct isp_blnr_stat_s *blnr_stat);
extern void isp_get_blnr_stat(struct isp_blnr_stat_s *blnr_stat);
extern void isp_set_ae_win(unsigned int left, unsigned int right,
		unsigned int top, unsigned int bottom);
extern void isp_set_awb_win(unsigned int left, unsigned int right,
		unsigned int top, unsigned int bottom);
extern void isp_set_ae_thrlpf(unsigned char thr_r, unsigned char thr_g,
		unsigned char thr_b, unsigned char lpf);
extern void isp_set_awb_yuv_thr(unsigned char yh, unsigned char yl,
		unsigned char u, unsigned char v);
extern void isp_set_awb_rgb_thr(unsigned char gb, unsigned char gr,
		unsigned br);
extern void flash_on(bool mode_pol_inv, bool led1_pol_inv,
		bool pin_mux_inv, struct wave_s *wave_param);
extern void torch_level(bool mode_pol_inv, bool led1_pol_inv,
		bool pin_mux_inv, bool torch_pol_inv,
		struct wave_s *wave_param, unsigned int level);
extern void wave_power_manage(bool enable);
extern void isp_hw_reset(void);
extern void isp_bypass_all(void);
extern void isp_bypass_for_rgb(void);
extern void isp_hw_enable(bool enable);
extern void isp_awb_set_gain(unsigned int r, unsigned int g,
		unsigned int b);
extern void isp_awb_get_gain(struct isp_awb_gain_s *awb_gain);
extern void set_isp_gamma_table(unsigned short *gamma, unsigned int type);
extern void get_isp_gamma_table(unsigned short *gamma, unsigned int type);
extern void disable_gc_lns_pk(bool flag);
extern void isp_ls_curve(unsigned int psize_v2h, unsigned int hactive,
	unsigned int vactive, unsigned int ocenter_c2l,
	unsigned int ocenter_c2t, unsigned int gain_0db,
	unsigned int curvature_gr, unsigned int curvature_r,
	unsigned int curvature_b, unsigned int curvature_gb, bool force_enable);

#endif

