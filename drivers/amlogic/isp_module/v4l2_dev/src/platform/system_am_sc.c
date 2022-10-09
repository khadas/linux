/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2018 Amlogic or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/
#define pr_fmt(fmt) "AM_SC: " fmt

#include "system_am_sc.h"
#include <linux/irqreturn.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/ioport.h>
#include <linux/of_platform.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/dma-contiguous.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of_fdt.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/delay.h>

#define AM_SC_NAME "amlogic, isp-sc"

//#define ENABLE_DI_YUV_DNR

#ifdef ENABLE_DI_YUV_DNR
#undef ENABLE_SC_BOTTOM_HALF_TASKLET
#endif

#ifdef ENABLE_DI_YUV_DNR
// workqueue structure
struct sc_workqueue_t {
    struct work_struct work_obj;
    unsigned int ctx_id;
};
static struct sc_workqueue_t sc_workqueue;
#endif

struct am_sc_context am_ctx[CAM_CTX_NUM];
struct am_sc *g_sc;

static void f2v_get_vertical_phase(
    u32 zoom_ratio,
    f2v_vphase_type_t type,
    u8 bank_length,
    f2v_vphase_t *vphase)
{
    int offset_in, offset_out;

    /* luma */
    offset_in = f2v_420_in_pos_luma[type] << PHASE_BITS;
    offset_out = (f2v_420_out_pos[type] * zoom_ratio)
        >> (ZOOM_BITS - PHASE_BITS);

    vphase->rcv_num = bank_length;
    if (bank_length == 4 || bank_length == 3)
        vphase->rpt_num = 1;
    else
        vphase->rpt_num = 0;

    if (offset_in > offset_out) {
        vphase->rpt_num = vphase->rpt_num + 1;
        vphase->phase =
            ((4 << PHASE_BITS) + offset_out - offset_in) >> 2;
    } else {
        while ((offset_in + (4 << PHASE_BITS)) <= offset_out) {
            if (vphase->rpt_num == 1)
                vphase->rpt_num = 0;
            else
                vphase->rcv_num++;
            offset_in += 4 << PHASE_BITS;
        }
        vphase->phase = (offset_out - offset_in) >> 2;
    }
}

static inline void update_wr_reg_bits(
    unsigned int reg,
    unsigned int mask,
    unsigned int val)
{
    unsigned int tmp, orig;
    void __iomem *base = g_sc->base_addr;

    if (base !=  NULL) {
        orig = readl(base + reg);
        tmp = orig & ~mask;
        tmp |= val & mask;
        writel(tmp, base + reg);
    }
}

static inline void sc_wr_reg_bits(
    unsigned int adr, unsigned int val,
    unsigned int start, unsigned int len)
{
    update_wr_reg_bits(adr,
        ((1 << len) - 1) << start, val << start);
}

static inline void sc_reg_wr(
    int addr, uint32_t val)
{
    void __iomem *base = g_sc->base_addr;

    if (base != NULL) {
        base = base + addr;
        writel(val, base);
    } else
        pr_err("isp-sc write register failed.\n");

}

static inline void sc_reg_rd(
    int addr, uint32_t *val)
{
    void __iomem *base = g_sc->base_addr;

    if (base != NULL && val) {
        base = base + addr;
        *val = readl(base);
    } else
        pr_err("isp-sc read register failed.\n");

}

static inline uint32_t sc_get_reg(int addr)
{
    void __iomem *base = g_sc->base_addr;
    uint32_t val = 0;

    if (base != NULL) {
        base = base + addr;
        val = readl(base);
    } else
        pr_err("isp-sc read register failed.\n");

    return val;
}

static void isp_sc_setting(
    u32 src_w, u32 src_h,
    u32 dst_w, u32 dst_h)
{
    f2v_vphase_t vphase;
    s32 i;
    s32 hsc_en, vsc_en;
    s32 prehsc_en,prevsc_en;
    s32 vsc_double_line_mode;
    u32 p_src_w, p_src_h;
    u32 vert_phase_step, horz_phase_step;
    u8 top_rcv_num, bot_rcv_num;
    u8 top_rpt_num, bot_rpt_num;
    u16 top_vphase, bot_vphase;
    u8 is_frame;
    s32  vert_bank_length = 4;
    s32 *filt_coef0 = (s32 *)&isp_filt_coef0[0];
    //s32 *filt_coef1 = (s32 *)&isp_filt_coef1[0];
    s32 *filt_coef2 = (s32 *)&isp_filt_coef2[0];
    f2v_vphase_type_t top_conv_type = F2V_P2P;
    f2v_vphase_type_t bot_conv_type = F2V_P2P;

    prehsc_en = 0;
    prevsc_en = 0;
    vsc_double_line_mode = 0;

    if (src_h != dst_h)
        vsc_en = 1;
    else
        vsc_en = 0;
    if (src_w != dst_w)
        hsc_en = 1;
    else
        hsc_en = 0;

    p_src_w = prehsc_en ? ((src_w + 1) >> 1) : src_w;
    p_src_h = prevsc_en ? ((src_h + 1) >> 1) : src_h;

    sc_reg_wr(ISP_SC_HOLD_LINE, 0x10);

    if (p_src_w > 2048) {
        //force vert bank length = 2
        vert_bank_length = 2;
        vsc_double_line_mode = 1;
    }

    if (g_sc->stop_flag) {
        //write vert filter coefs
        sc_reg_wr(ISP_SC_COEF_IDX, 0x0000);
        for (i = 0; i < 33; i++) {
            if (vert_bank_length == 2)
                sc_reg_wr(ISP_SC_COEF, filt_coef2[i]); //bilinear
            else
                sc_reg_wr(ISP_SC_COEF, filt_coef0[i]); //bicubic
        }

        //write horz filter coefs
        sc_reg_wr(ISP_SC_COEF_IDX, 0x0100);
        for (i = 0; i < 33; i++) {
            sc_reg_wr(ISP_SC_COEF, filt_coef0[i]); //bicubic
        }
    }

    if (p_src_h > 2048)
        vert_phase_step =
            ((p_src_h << 18) / dst_h) << 2;
    else
        vert_phase_step = (p_src_h << 20) / dst_h;

    if (p_src_w > 2048)
        horz_phase_step =
            ((p_src_w << 18) / dst_w) << 2;
    else
        horz_phase_step = (p_src_w << 20) / dst_w;

    is_frame = (top_conv_type == F2V_IT2P)
        || (top_conv_type == F2V_IB2P)
        || (top_conv_type == F2V_P2P);

    if (is_frame) {
        f2v_get_vertical_phase(
            vert_phase_step, top_conv_type,
            vert_bank_length, &vphase);
        top_rcv_num = vphase.rcv_num;
        top_rpt_num = vphase.rpt_num;
        top_vphase  = vphase.phase;
        bot_rcv_num = 0;
        bot_rpt_num = 0;
        bot_vphase  = 0;
    } else {
        f2v_get_vertical_phase(
            vert_phase_step, top_conv_type,
            vert_bank_length, &vphase);
        top_rcv_num = vphase.rcv_num;
        top_rpt_num = vphase.rpt_num;
        top_vphase = vphase.phase;

        f2v_get_vertical_phase(
            vert_phase_step, bot_conv_type,
            vert_bank_length, &vphase);
        bot_rcv_num = vphase.rcv_num;
        bot_rpt_num = vphase.rpt_num;
        bot_vphase = vphase.phase;
    }

    vert_phase_step = (vert_phase_step << 4);
    horz_phase_step = (horz_phase_step << 4);

    sc_reg_wr(ISP_SC_LINE_IN_LENGTH, src_w);
    sc_reg_wr(ISP_SC_PIC_IN_HEIGHT, src_h);
    sc_reg_wr(ISP_VSC_REGION12_STARTP, 0);
    sc_reg_wr(ISP_VSC_REGION34_STARTP,
        ((dst_h << 16) | dst_h));
    sc_reg_wr(ISP_VSC_REGION4_ENDP, dst_h - 1);

    sc_reg_wr(ISP_VSC_START_PHASE_STEP, vert_phase_step);
    if (g_sc->stop_flag) {
        sc_reg_wr(ISP_VSC_REGION0_PHASE_SLOPE, 0);
        sc_reg_wr(ISP_VSC_REGION1_PHASE_SLOPE, 0);
        sc_reg_wr(ISP_VSC_REGION3_PHASE_SLOPE, 0);
        sc_reg_wr(ISP_VSC_REGION4_PHASE_SLOPE, 0);
    }

    sc_reg_wr(ISP_VSC_PHASE_CTRL,
        (vsc_double_line_mode << 17) |
        ((!is_frame) << 16) |
        (0 << 15) |
        (bot_rpt_num << 13) |
        (bot_rcv_num << 8) |
        (0 << 7) |
        (top_rpt_num << 5) |
        (top_rcv_num << 0));
    sc_reg_wr(ISP_VSC_INI_PHASE,
        (bot_vphase << 16) | top_vphase);
    sc_reg_wr(ISP_HSC_REGION12_STARTP, 0);
    sc_reg_wr(ISP_HSC_REGION34_STARTP,
        (dst_w << 16) | dst_w);
    sc_reg_wr(ISP_HSC_REGION4_ENDP, dst_w - 1);

    sc_reg_wr(ISP_HSC_START_PHASE_STEP, horz_phase_step);
    if (g_sc->stop_flag) {
        sc_reg_wr(ISP_HSC_REGION0_PHASE_SLOPE, 0);
        sc_reg_wr(ISP_HSC_REGION1_PHASE_SLOPE, 0);
        sc_reg_wr(ISP_HSC_REGION3_PHASE_SLOPE, 0);
        sc_reg_wr(ISP_HSC_REGION4_PHASE_SLOPE, 0);

        sc_reg_wr(ISP_HSC_PHASE_CTRL, (1 << 21) | (4 << 16) | 0);
    }

    sc_reg_wr(ISP_SC_MISC,
        (prevsc_en << 21) |
        (prehsc_en << 20) | // prehsc_en
        (prevsc_en << 19) | // prevsc_en
        (vsc_en << 18) | // vsc_en
        (hsc_en << 17) | // hsc_en
        (1 << 16) | // sc_top_en
        (1 << 15) | // vd1 sc out enable
        (0 << 12) | // horz nonlinear 4region enable
        (4 << 8) | // horz scaler bank length
        (0 << 5) | // vert scaler phase field mode enable
        (0 << 4) | // vert nonlinear 4region enable
        (vert_bank_length << 0));  // vert scaler bank length
}

static void isp_mtx_setting(u8 ch_mode, s32 mode)
{
    s32 mat_conv_en = 0;
    s32 i, pre_offset[3] ={0, 0, 0}, post_offset[3]= {0, 0, 0};
    s32 mat_coef[15];
    bool invert = false;

    if ((ch_mode == 0) && (mode == 1)
        && (g_sc->isp_frame.reg_rgb_mode == 1))
        invert = true;

    if (mode == 1) {
        mat_conv_en = 1;
        for (i = 0; i < 3; i++) {
            pre_offset[i] = rgb2yuvpre[i];
            post_offset[i] = (invert == true) ?
                rgb2yuvpos_invert[i] :
                rgb2yuvpos[i];
        }
        for (i = 0; i < 15; i++)
            mat_coef[i] = (invert == true) ?
                rgb2ycbcr_invert[i] :
                rgb2ycbcr[i];
    } else if (mode == 2) {
        mat_conv_en = 1;
        for (i = 0; i < 3; i++) {
            pre_offset[i] = yuv2rgbpre[i];
            post_offset[i] = yuv2rgbpos[i];
        }
        for (i = 0; i < 15; i++)
            mat_coef[i] = ycbcr2rgb[i];
    }
    sc_reg_wr(ISP_MATRIX_COEF00_01,
        (mat_coef[0 * 3 + 0] << 16) |
        (mat_coef[0 * 3 + 1] & 0x1FFF));
    sc_reg_wr(ISP_MATRIX_COEF02_10,
        (mat_coef[0 * 3 + 2] << 16) |
        (mat_coef[1 * 3 + 0] & 0x1FFF));
    sc_reg_wr(ISP_MATRIX_COEF11_12,
        (mat_coef[1 * 3 + 1] << 16) |
        (mat_coef[1 * 3 + 2] & 0x1FFF));
    sc_reg_wr(ISP_MATRIX_COEF20_21,
        (mat_coef[2 * 3 + 0] << 16) |
        (mat_coef[2 * 3 + 1] & 0x1FFF));
    sc_reg_wr(ISP_MATRIX_COEF22,
        mat_coef[2 * 3 + 2]);
    sc_reg_wr(ISP_MATRIX_OFFSET0_1,
        (post_offset[0] << 16) |
        (post_offset[1] & 0xFFF));
    sc_reg_wr(ISP_MATRIX_OFFSET2,
        post_offset[2]);
    sc_reg_wr(ISP_MATRIX_PRE_OFFSET0_1,
        (pre_offset[0] << 16) |
        (pre_offset[1] & 0xFFF));
    sc_reg_wr(ISP_MATRIX_PRE_OFFSET2,
        pre_offset[2]);
    sc_reg_wr(ISP_MATRIX_EN_CTRL,
        mat_conv_en);
}

static void isp_mif_setting(u8 swap_uv, ISP_MIF_t *wr_mif)
{
    sc_wr_reg_bits(ISP_SCWR_MIF_CTRL0,
        ((1 << 0) |
        (0 << 1) |
        (0 << 2) |
        (0 << 3) |
        (wr_mif->reg_rev_x << 4) |
        (wr_mif->reg_rev_y << 5) |
        (wr_mif->reg_little_endian << 6) |
        (0 << 7) |
        (1 << 8) |
        (swap_uv << 9) |
        (0 <<10) |
        (wr_mif->reg_bit10_mode << 11) |
        (wr_mif->reg_enable_3ch << 12) |
        (wr_mif->reg_only_1ch << 13)),
        0, 14);
    sc_wr_reg_bits(ISP_SCWR_MIF_CTRL0,
        wr_mif->reg_pingpong_en, 17, 1);
    sc_reg_wr(ISP_SCWR_MIF_CTRL1,
        wr_mif->reg_words_lim |
        (wr_mif->reg_burst_lim << 4) |
        (wr_mif->reg_rgb_mode << 8) |
        (wr_mif->reg_hconv_mode << 10) |
        (wr_mif->reg_vconv_mode << 12) |
        (0 << 16) |
        (0 << 20));
    sc_reg_wr(ISP_SCWR_MIF_CTRL2, 0);
    sc_reg_wr(ISP_SCWR_MIF_CTRL3,
        wr_mif->reg_hsizem1 |
        (wr_mif->reg_vsizem1 << 16));
    sc_reg_wr(ISP_SCWR_MIF_CTRL4,
        wr_mif->reg_start_x |
        (wr_mif->reg_start_y << 16));
    sc_reg_wr(ISP_SCWR_MIF_CTRL5,
        wr_mif->reg_end_x |
        (wr_mif->reg_end_y << 16));
    if (g_sc->stop_flag) {
        sc_reg_wr(ISP_SCWR_MIF_CTRL6,
            (wr_mif->reg_canvas_strb_luma & 0xffff) |
            (wr_mif->reg_canvas_strb_chroma << 16));
        sc_reg_wr(ISP_SCWR_MIF_CTRL7,
            wr_mif->reg_canvas_strb_r);
        sc_reg_wr(ISP_SCWR_MIF_CTRL11,
            wr_mif->reg_canvas_baddr_luma_other);
        sc_reg_wr(ISP_SCWR_MIF_CTRL12,
            wr_mif->reg_canvas_baddr_chroma_other);
        sc_reg_wr(ISP_SCWR_MIF_CTRL13,
            wr_mif->reg_canvas_baddr_r_other);
        sc_reg_wr(ISP_SCWR_MIF_CTRL8,
            wr_mif->reg_canvas_baddr_luma);
        sc_reg_wr(ISP_SCWR_MIF_CTRL9,
            wr_mif->reg_canvas_baddr_chroma);
        sc_reg_wr(ISP_SCWR_MIF_CTRL10,
            wr_mif->reg_canvas_baddr_r);
    }
}

static void isp_mif_addr(ISP_MIF_t *wr_mif)
{
    sc_reg_wr(ISP_SCWR_MIF_CTRL6,
        (wr_mif->reg_canvas_strb_luma & 0xffff) |
        (wr_mif->reg_canvas_strb_chroma << 16));
    sc_reg_wr(ISP_SCWR_MIF_CTRL7,
        wr_mif->reg_canvas_strb_r);
    sc_reg_wr(ISP_SCWR_MIF_CTRL11,
        wr_mif->reg_canvas_baddr_luma_other);
    sc_reg_wr(ISP_SCWR_MIF_CTRL12,
        wr_mif->reg_canvas_baddr_chroma_other);
    sc_reg_wr(ISP_SCWR_MIF_CTRL13,
        wr_mif->reg_canvas_baddr_r_other);
    sc_reg_wr(ISP_SCWR_MIF_CTRL8,
        wr_mif->reg_canvas_baddr_luma);
    sc_reg_wr(ISP_SCWR_MIF_CTRL9,
        wr_mif->reg_canvas_baddr_chroma);
    sc_reg_wr(ISP_SCWR_MIF_CTRL10,
        wr_mif->reg_canvas_baddr_r);
}

static void enable_isp_scale_new (
    int    initial_en,
    int    ir_source,
    int    dbg_mode,
    int    clip_mode,
    int    clip_x_st,
    int    clip_x_ed,
    int    clip_y_st,
    int    clip_y_ed,
    int    src_w,      //in_w
    int    src_h,      //in_h
    int    wr_w,      //out_w
    int    wr_h,       //out_h
    int    mux_sel,
    int    sc_en,
    int    mtx_mode,     //1:rgb->yuv,2:yuv->rgb,0:bypass
    int    swap_uv,
    int    ch_mode,
    /*uint32_t    baddr,*/
    ISP_MIF_t *wr_mif
){
    //int tmp;
    uint32_t reg_data;
    u32 val = 0;

    int clip_w = clip_mode ? clip_x_ed - clip_x_st + 1 : src_w;
    int clip_h = clip_mode ? clip_y_ed - clip_y_st + 1 : src_h;
    int  sco_w = sc_en ? wr_w : clip_w;
    int  sco_h = sc_en ? wr_h : clip_h;

    wr_mif->reg_hsizem1  = sco_w-1;
    wr_mif->reg_vsizem1  = sco_h-1;
    wr_mif->reg_start_x = 0;
    wr_mif->reg_start_y = 0;
    wr_mif->reg_end_x  = sco_w-1;
    wr_mif->reg_end_y  = sco_h-1;
    if ( g_sc->stop_flag || g_sc->refresh ) isp_mif_setting(swap_uv, wr_mif);

    sc_reg_wr(ISP_SCWR_GCLK_CTRL, (src_w<<16));
    if (clip_mode) {
        sc_reg_wr(ISP_SCWR_CLIP_CTRL1,(1<<31) | (clip_x_ed<<16) | clip_x_st);
        sc_reg_wr(ISP_SCWR_CLIP_CTRL2, (clip_y_ed<<16) | clip_y_st);
    } else
        sc_reg_wr(ISP_SCWR_CLIP_CTRL1, 0x7FFF0000);

    if ( g_sc->stop_flag || g_sc->refresh ) {
        if (dbg_mode) {
            sc_reg_wr(ISP_SCWR_TOP_GEN, (1<<9) | dbg_mode);
            sc_reg_wr(ISP_SCWR_MIF_CTRL14, 0x70010101);
        } else
            sc_reg_wr(ISP_SCWR_TOP_GEN, 0xD1);
    }

    if (sc_en)
        isp_sc_setting(clip_w,clip_h,wr_w,wr_h);
    else
        sc_reg_wr(ISP_SC_MISC, 0x18404);

    if ( g_sc->stop_flag ) {
        sc_reg_rd(ISP_SCWR_TOP_CTRL, &reg_data);
        sc_reg_wr(ISP_SCWR_TOP_CTRL,(reg_data & 0xff0bffff) |
                             ((mux_sel & 0x7)<<20) |
                             (ir_source << 18));

        sc_reg_wr(ISP_SCWR_SYNC_DELAY, 0x0000100); //bit26~27 == 0, write done, bit26~27 == 2, vsync
    }

    isp_mtx_setting(ch_mode, mtx_mode);

    sc_reg_wr(ISP_SCWR_SC_CTRL1, ((clip_w & 0x1fff) << 16) | (clip_h & 0x1fff));

    if (initial_en && g_sc->stop_flag) {
        sc_reg_rd(ISP_SCWR_TOP_DBG0, &val);
        if (val & (1 << 6))
            g_sc->last_end_frame = 0;
        else
            g_sc->last_end_frame = 1;

        sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 1, 19, 1);
    }

    if ( mux_sel != 4  && wr_mif->reg_rgb_mode != 0 )
        sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 5, 13, 3);
    else
        sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 0, 13, 3);

    if (g_sc->refresh == false) pr_info(" finished isp scale setting \n");
}

static void init_sc_mif_setting(int ctx_id, ISP_MIF_t *mif_frame)
{
    u32 plane_size, frame_size;
    tframe_t *buf = NULL;
    int retval;

    if (!mif_frame)
        return;

    if (g_sc->stop_flag) {
        if (kfifo_len(&am_ctx[ctx_id].sc_fifo_in) > 0) {
            retval = kfifo_out(&am_ctx[ctx_id].sc_fifo_in, &buf, sizeof(tframe_t*));
            if (retval == sizeof(tframe_t*)) {
                g_sc->multi_camera.cam_id[0] = ctx_id;
                g_sc->multi_camera.pre_frame[0] = buf;
            } else {
                pr_info("%d, fifo out failed.\n", __LINE__);
            }
        } else {
            pr_info("%d, sc fifo is empty .\n", __LINE__);
        }
    }

    if (am_ctx[ctx_id].info.out_fmt == NV12_GREY) {
        am_ctx[ctx_id].ch_mode = 1;
    } else {
        am_ctx[ctx_id].ch_mode = 0;
    }

    memset(mif_frame, 0, sizeof(ISP_MIF_t));
    plane_size = am_ctx[ctx_id].info.out_w * am_ctx[ctx_id].info.out_h;
    mif_frame->reg_little_endian = 1;
    if ((am_ctx[ctx_id].info.in_fmt == RGB24) ||
        (am_ctx[ctx_id].info.in_fmt == AYUV)) {
        if ((am_ctx[ctx_id].info.out_fmt == RGB24) ||
            (am_ctx[ctx_id].info.out_fmt == AYUV)) {
            mif_frame->reg_rgb_mode = 1;
        } else if (am_ctx[ctx_id].info.out_fmt == NV12_YUV) {
            mif_frame->reg_rgb_mode = 2;
        } else if (am_ctx[ctx_id].info.out_fmt == NV12_YVU) {
            mif_frame->reg_rgb_mode = 2;
        } else if (am_ctx[ctx_id].info.out_fmt == UYVY) {
            mif_frame->reg_rgb_mode = 0;
        } else if (am_ctx[ctx_id].info.out_fmt == NV12_GREY) {
            mif_frame->reg_rgb_mode = 2;
        } else if (am_ctx[ctx_id].info.out_fmt == RAW16) {
            mif_frame->reg_rgb_mode = 0;
        } else if (am_ctx[ctx_id].info.out_fmt == RAW_YUY2)
            mif_frame->reg_rgb_mode = 0;
    } else if ((am_ctx[ctx_id].info.in_fmt == NV12_YUV) ||
            (am_ctx[ctx_id].info.in_fmt == NV12_GREY)) {
        if (am_ctx[ctx_id].info.out_fmt == NV12_GREY) {
            mif_frame->reg_rgb_mode = 2;
        }
    }

    mif_frame->reg_bit10_mode = 0;
    mif_frame->reg_words_lim = 4;
    mif_frame->reg_burst_lim = 2;
    mif_frame->reg_hconv_mode = 2;
    mif_frame->reg_vconv_mode = 0;
    mif_frame->reg_pingpong_en = 0;
    mif_frame->reg_start_x = 0;
    mif_frame->reg_start_y = 0;
    mif_frame->reg_end_x = am_ctx[ctx_id].info.out_w -1;
    mif_frame->reg_end_y = am_ctx[ctx_id].info.out_h - 1;
    mif_frame->reg_hsizem1 = am_ctx[ctx_id].info.out_w - 1;
    mif_frame->reg_vsizem1 = am_ctx[ctx_id].info.out_h - 1;
    if (am_ctx[ctx_id].ch_mode == 1) {
        mif_frame->reg_only_1ch = 1;
        mif_frame->reg_enable_3ch = 0;
        frame_size = plane_size;
        mif_frame->reg_canvas_strb_luma =
            am_ctx[ctx_id].info.out_w;
        mif_frame->reg_canvas_strb_chroma = 0;
        mif_frame->reg_canvas_strb_r = 0;
        mif_frame->reg_hconv_mode = 2;
        mif_frame->reg_vconv_mode = 2;
        mif_frame->reg_rgb_mode = 2;
    } else if (am_ctx[ctx_id].ch_mode == 3) {
        mif_frame->reg_only_1ch = 0;
        mif_frame->reg_enable_3ch = 1;
        frame_size = plane_size * 3;
        mif_frame->reg_canvas_strb_luma =
            am_ctx[ctx_id].info.out_w;
        mif_frame->reg_canvas_strb_chroma =
            am_ctx[ctx_id].info.out_w;
        mif_frame->reg_canvas_strb_r =
            am_ctx[ctx_id].info.out_w;
        mif_frame->reg_hconv_mode = 2;
        mif_frame->reg_vconv_mode = 2;
        mif_frame->reg_rgb_mode = 2;
    } else {
        mif_frame->reg_only_1ch = 0;
        mif_frame->reg_enable_3ch = 0;
        if (mif_frame->reg_rgb_mode == 0) {
            frame_size = plane_size * 2;
            mif_frame->reg_canvas_strb_luma =
                am_ctx[ctx_id].info.out_w * 2;
            mif_frame->reg_canvas_strb_chroma = 0;
            mif_frame->reg_canvas_strb_r = 0;
        } else if (mif_frame->reg_rgb_mode == 1) {
            frame_size = plane_size * 3;
            mif_frame->reg_canvas_strb_luma =
                am_ctx[ctx_id].info.out_w * 3;
            mif_frame->reg_canvas_strb_chroma = 0;
            mif_frame->reg_canvas_strb_r = 0;
        } else if (mif_frame->reg_rgb_mode == 2) {
            frame_size = plane_size * 3 / 2;
            mif_frame->reg_canvas_strb_luma =
                am_ctx[ctx_id].info.out_w;
            mif_frame->reg_canvas_strb_chroma =
                am_ctx[ctx_id].info.out_w;
            mif_frame->reg_canvas_strb_r = 0;
        } else {
            frame_size = plane_size * 3;
            mif_frame->reg_canvas_strb_luma =
                am_ctx[ctx_id].info.out_w * 3;
            mif_frame->reg_canvas_strb_chroma = 0;
            mif_frame->reg_canvas_strb_r = 0;
        }
    }

    if (buf == NULL)
        return;

    mif_frame->reg_canvas_baddr_luma =
        buf->primary.address;
    mif_frame->reg_canvas_baddr_luma_other =
        mif_frame->reg_canvas_baddr_luma;

    if (mif_frame->reg_canvas_strb_chroma) {
        mif_frame->reg_canvas_baddr_chroma =
            buf->secondary.address;
        mif_frame->reg_canvas_baddr_chroma_other =
            mif_frame->reg_canvas_baddr_chroma;
    }

    if (mif_frame->reg_canvas_strb_r) {
        mif_frame->reg_canvas_baddr_r =
            buf->secondary.address;
        mif_frame->reg_canvas_baddr_r_other =
            mif_frame->reg_canvas_baddr_r;
    }

    if (!mif_frame->reg_pingpong_en) {
        mif_frame->reg_canvas_baddr_luma_other = 0;
        mif_frame->reg_canvas_baddr_chroma_other = 0;
        mif_frame->reg_canvas_baddr_r_other = 0;
    }

    pr_info("init_sc_mif_setting: %dx%d -> %dx%d, %x-%x-%x-%x-%x-%x, stride: %x-%x-%x\n",
        am_ctx[ctx_id].info.src_w, am_ctx[ctx_id].info.src_h,
        am_ctx[ctx_id].info.out_w, am_ctx[ctx_id].info.out_h,
        mif_frame->reg_canvas_baddr_luma,
        mif_frame->reg_canvas_baddr_chroma,
        mif_frame->reg_canvas_baddr_r,
        mif_frame->reg_canvas_baddr_luma_other,
        mif_frame->reg_canvas_baddr_chroma_other,
        mif_frame->reg_canvas_baddr_r_other,
        mif_frame->reg_canvas_strb_luma,
        mif_frame->reg_canvas_strb_chroma,
        mif_frame->reg_canvas_strb_r);
}

#ifdef ENABLE_SC_BOTTOM_HALF_TASKLET
static void sc_do_tasklet( unsigned long data )
{
    tframe_t *f_buff;
    u8 ctx_id = ((struct sc_tasklet_t *)data)->ctx_id;
    metadata_t metadata;
    memset(&metadata, 0, sizeof(metadata_t));
    //pr_err("do tasklet: %d\n", ctx_id);
    while (kfifo_out(&am_ctx[ctx_id].sc_fifo_out, &f_buff, sizeof(tframe_t*))) {
        metadata.width = am_ctx[ctx_id].info.out_w;
        metadata.height = am_ctx[ctx_id].info.out_h;
        metadata.frame_id = am_ctx[ctx_id].frame_id;
        metadata.frame_number = am_ctx[ctx_id].frame_id;
        metadata.line_size = (((3 * am_ctx[ctx_id].info.out_w) + 127) & (~127));
        am_ctx[ctx_id].callback(am_ctx[ctx_id].ctx, f_buff, &metadata );
        am_ctx[ctx_id].frame_id++;
    }
}
#else
static void sc_do_workqueue( struct work_struct *work )
{
    tframe_t *f_buff;
    u8 ctx_id = ((struct work_struct *)work)->ctx_id;
    metadata_t metadata;
    memset(&metadata, 0, sizeof(metadata_t));

    while (kfifo_out(&am_ctx[ctx_id].sc_fifo_out, &f_buff, sizeof(tframe_t*))) {
        metadata.width = am_ctx[ctx_id].info.out_w;
        metadata.height = am_ctx[ctx_id].info.out_h;
        metadata.frame_id = am_ctx[ctx_id].frame_id;
        metadata.frame_number = am_ctx[ctx_id].frame_id;
        metadata.line_size = (((3 * am_ctx[ctx_id].info.out_w) + 127) & (~127));
        am_ctx[ctx_id].callback(am_ctx[ctx_id].ctx, f_buff, &metadata );
        am_ctx[ctx_id].frame_id++;
    }
}
#endif


static void sc_config_next_buffer(tframe_t* f_buf, int ping)
{
    if (ping) {
        g_sc->isp_frame.reg_canvas_baddr_luma = f_buf->primary.address;
        g_sc->isp_frame.reg_canvas_baddr_chroma = f_buf->secondary.address;
        g_sc->isp_frame.reg_canvas_baddr_r = f_buf->secondary.address;
    } else {
        g_sc->isp_frame.reg_canvas_baddr_luma_other = f_buf->primary.address;
        g_sc->isp_frame.reg_canvas_baddr_chroma_other = f_buf->secondary.address;
        g_sc->isp_frame.reg_canvas_baddr_r_other = f_buf->secondary.address;
    }
    isp_mif_addr(&g_sc->isp_frame);
}

static void am_sc_swap_buf(int check_last, u32 flag_ready)
{
    u32 frame_delay = FRAME_DELAY_QUEUE - 1;
    g_sc->last_end_frame = flag_ready;

    if (g_sc->isp_frame.reg_pingpong_en) {
        if (g_sc->no_ready_th || (check_last == 0)) {
            if (g_sc->multi_camera.pre_frame[frame_delay - 1] && g_sc->multi_camera.pre_frame[frame_delay - 2]) {
                tframe_t* temp_frame = g_sc->multi_camera.pre_frame[frame_delay - 1];
                int cam_id = g_sc->multi_camera.cam_id[frame_delay - 1];
                g_sc->multi_camera.pre_frame[frame_delay - 1] = g_sc->multi_camera.pre_frame[frame_delay - 2];
                g_sc->multi_camera.pre_frame[frame_delay - 2] = temp_frame;
                g_sc->multi_camera.cam_id[frame_delay - 1] = g_sc->multi_camera.cam_id[frame_delay - 2];
                g_sc->multi_camera.cam_id[frame_delay - 2] = cam_id;
            }
        }
    }

    if (check_last) {
        for (frame_delay = 0; frame_delay < FRAME_DELAY_QUEUE; frame_delay ++) {
            if (g_sc->multi_camera.cam_id[frame_delay] == g_sc->cam_id_current) {
                if (g_sc->multi_camera.pre_frame[frame_delay])
                    kfifo_in(&am_ctx[g_sc->multi_camera.cam_id[frame_delay]].sc_fifo_in, &g_sc->multi_camera.pre_frame[frame_delay], sizeof(tframe_t*));
                g_sc->multi_camera.pre_frame[frame_delay] = NULL;
            }
        }
    }
}

static irqreturn_t isp_sc_isr(int irq, void *data)
{
    if (g_sc->start_delay_cnt < START_DELAY_THR) {
        g_sc->start_delay_cnt++;
    } else {
        /* maybe need drop one more frame */
        if (g_sc->start_delay_cnt == START_DELAY_THR) {
            pr_info("wrmif start: %x, %x, %x\n", sc_get_reg(ISP_SCWR_TOP_DBG1), sc_get_reg(ISP_SCWR_TOP_DBG2), sc_get_reg(ISP_SCWR_TOP_CTRL));
            sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 1, 1, 1);
            sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 1, 0, 1);
            g_sc->start_delay_cnt++;
        } else {
            u32 flag = 0;
            u32 flag_ready = 0;
            tframe_t *f_buf = NULL;
            unsigned long flags;
            int retval;

            sc_reg_rd(ISP_SCWR_TOP_DBG0, &flag);

            flag_ready = (flag & (1 << 6)) ? 1 : 0;
            if (flag_ready == g_sc->last_end_frame) {
                if (am_ctx[g_sc->cam_id_current].temp_buf) {
                    pr_info("%d, sc last fifo no ready %d.\n", __LINE__, g_sc->cam_id_current);
                    am_sc_swap_buf(1, flag_ready);
                    g_sc->no_ready_th ++;
                    g_sc->working = 0;
                    return IRQ_HANDLED;
                 }
            } else
                g_sc->no_ready_th = 0;

            if (g_sc->isp_frame.reg_pingpong_en == 0)
                flag = 0;

            spin_lock_irqsave( &g_sc->sc_lock, flags );
            if (kfifo_len(&am_ctx[g_sc->cam_id_next].sc_fifo_in) > 0) {
                retval = kfifo_out(&am_ctx[g_sc->cam_id_next].sc_fifo_in, &f_buf, sizeof(tframe_t*));
                if (retval != sizeof(tframe_t*))
                    pr_info("%d, fifo out failed %d.\n", __LINE__, g_sc->cam_id_next);
            } else {
                //also output stream into camera dummy fifo. it can keep correct cam seq
                if (am_ctx[g_sc->cam_id_next].temp_buf)
                    f_buf = am_ctx[g_sc->cam_id_next].temp_buf;
                else if (am_ctx[g_sc->cam_id_next_next].temp_buf)
                    f_buf = am_ctx[g_sc->cam_id_next_next].temp_buf;
                else if (am_ctx[g_sc->cam_id_current].temp_buf)
                    f_buf = am_ctx[g_sc->cam_id_current].temp_buf;
                else if (am_ctx[g_sc->cam_id_last].temp_buf)
                    f_buf = am_ctx[g_sc->cam_id_last].temp_buf;
            }
            spin_unlock_irqrestore( &g_sc->sc_lock, flags );

            if (g_sc->refresh && am_ctx[g_sc->cam_id_next].temp_buf) {
                am_sc_hw_init(g_sc->cam_id_next, 0, g_sc->refresh);
                g_sc->refresh = 0;
            }

            if (f_buf)
                sc_config_next_buffer(f_buf, (flag & (1 << 8)) ? 0 : 1);

            if ((g_sc->user > 1) || (g_sc->dcam_mode)) {
                sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 1, 28, 1);
                sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 0, 28, 1);
            }

            if (g_sc->multi_camera.pre_frame[FRAME_DELAY_QUEUE-1]) {
#ifdef ENABLE_SC_BOTTOM_HALF_TASKLET
                if (!kfifo_is_full(&am_ctx[g_sc->multi_camera.cam_id[FRAME_DELAY_QUEUE-1]].sc_fifo_out)) {
                    kfifo_in(&am_ctx[g_sc->multi_camera.cam_id[FRAME_DELAY_QUEUE-1]].sc_fifo_out, &(g_sc->multi_camera.pre_frame[FRAME_DELAY_QUEUE-1]), sizeof(tframe_t*));
                }
                g_sc->sc_tasklet.ctx_id = g_sc->multi_camera.cam_id[FRAME_DELAY_QUEUE-1];
                tasklet_schedule(&g_sc->sc_tasklet.tasklet_obj);
#else
                if (!kfifo_is_full(&am_ctx[g_sc->multi_camera.cam_id[FRAME_DELAY_QUEUE-1]].sc_fifo_out)) {
                    kfifo_in(&am_ctx[g_sc->multi_camera.cam_id[FRAME_DELAY_QUEUE-1]].sc_fifo_out, &(g_sc->multi_camera.pre_frame[FRAME_DELAY_QUEUE-1]), sizeof(tframe_t*));
                }
                sc_workqueue.ctx_id = g_sc->multi_camera.cam_id[FRAME_DELAY_QUEUE-1];
                schedule_work(&(sc_workqueue.work_obj));
#endif
            } else {
                if (am_ctx[g_sc->multi_camera.cam_id[FRAME_DELAY_QUEUE-1]].temp_buf)
                    pr_info("%d, cam %d sc fifo is empty.\n", __LINE__, g_sc->multi_camera.cam_id[FRAME_DELAY_QUEUE-1]);
            }

            for (flag = FRAME_DELAY_QUEUE - 1; flag > 0; flag --) {
                g_sc->multi_camera.pre_frame[flag] = g_sc->multi_camera.pre_frame[flag - 1];
                g_sc->multi_camera.cam_id[flag] = g_sc->multi_camera.cam_id[flag - 1];
            }
            g_sc->multi_camera.pre_frame[0] = f_buf;
            g_sc->multi_camera.cam_id[0] = g_sc->cam_id_next;
            g_sc->last_end_frame = flag_ready;
            //reserve dummy buffer
            if (f_buf == am_ctx[g_sc->cam_id_last].temp_buf ||
                f_buf == am_ctx[g_sc->cam_id_next].temp_buf ||
                f_buf == am_ctx[g_sc->cam_id_current].temp_buf ||
                f_buf == am_ctx[g_sc->cam_id_next_next].temp_buf)
                g_sc->multi_camera.pre_frame[0] = NULL;
        }
    }
    g_sc->working = 0;
    return IRQ_HANDLED;
}

int am_sc_parse_dt(struct device_node *node, int port)
{
    int rtn = -1;
    int irq = -1;
    struct resource rs;
    struct am_sc *t_sc = NULL;

    if (node == NULL) {
        pr_err("%s: Error input param\n", __func__);
        return -1;
    }

    rtn = of_device_is_compatible(node, AM_SC_NAME);
    if (rtn == 0) {
        pr_err("%s: Error match compatible\n", __func__);
        return -1;
    }

    t_sc = kzalloc(sizeof(*t_sc), GFP_KERNEL);
    if (t_sc == NULL) {
        pr_err("%s: Failed to alloc isp-sc\n", __func__);
        return -1;
    }

    t_sc->of_node = node;

    rtn = of_address_to_resource(node, port, &rs);
    if (rtn != 0) {
        pr_err("%s:Error get isp-sc reg resource\n", __func__);
        goto reg_error;
    }

    pr_info("%s: rs idx info: name: %s\n", __func__, rs.name);
    if (strcmp(rs.name, "isp_sc0") == 0) {
        t_sc->reg = rs;
        t_sc->base_addr = ioremap_nocache(
            t_sc->reg.start, resource_size(&t_sc->reg));
    }

    irq = irq_of_parse_and_map(node, port);
    if (irq <= 0) {
        pr_err("%s:Error get isp-sc irq\n", __func__);
        goto irq_error;
    }

    t_sc->irq = irq;
    pr_info("%s:rs info: irq: %d, ds%d\n", __func__, t_sc->irq, port);

    t_sc->p_dev = of_find_device_by_node(node);

    memcpy((void *)&t_sc->isp_frame, (void *)&isp_frame, sizeof(ISP_MIF_t));
    t_sc->port = port;
    t_sc->stop_flag = true;
    t_sc->user = 0;
    t_sc->working = 0;
    t_sc->dcam_mode = 0;
    t_sc->no_ready_th = 0;
    t_sc->refresh = 0;
    t_sc->cam_id_last = 0;
    t_sc->cam_id_current = 0;
    t_sc->cam_id_next = 0;
    t_sc->cam_id_next_next = 0;

    g_sc = t_sc;

    spin_lock_init(&g_sc->sc_lock);

    int i = 0;
    for (i = 0; i < CAM_CTX_NUM; i ++) {
        if (!kfifo_initialized(&am_ctx[i].sc_fifo_in)) {
            rtn = kfifo_alloc(&am_ctx[i].sc_fifo_in, PAGE_SIZE, GFP_KERNEL);
            if (rtn) {
                pr_info("alloc sc fifo failed.\n");
                return rtn;
            }
        }

        if (!kfifo_initialized(&am_ctx[i].sc_fifo_out)) {
            rtn = kfifo_alloc(&am_ctx[i].sc_fifo_out, PAGE_SIZE, GFP_KERNEL);
            if (rtn) {
                pr_info("alloc sc_tasklet fifo failed.\n");
                return rtn;
            }
        }
    }

    return 0;
irq_error:
    iounmap(t_sc->base_addr);
    t_sc->base_addr = NULL;

reg_error:
    if (t_sc != NULL)
        kfree(t_sc);
    return -1;
}

void am_sc_deinit_parse_dt(void)
{
    if (g_sc == NULL) {
        pr_err("Error g_sc is NULL\n");
        return;
    }

    int i = 0;
    for (i = 0; i < CAM_CTX_NUM; i ++) {
        kfifo_free(&am_ctx[i].sc_fifo_in);
        kfifo_free(&am_ctx[i].sc_fifo_out);
    }

    if (g_sc->base_addr != NULL) {
        iounmap(g_sc->base_addr);
        g_sc->base_addr = NULL;
    }

    kfree(g_sc);
    g_sc = NULL;
}

void am_sc_api_dma_buffer(int ctx_id, tframe_t * data, unsigned int index)
{
    unsigned long flags;

    if (am_ctx[ctx_id].temp_buf == NULL) return;
    spin_lock_irqsave(&g_sc->sc_lock, flags);
    tframe_t *buf = am_ctx[ctx_id].temp_buf + index;
    memcpy(buf, data, sizeof(tframe_t));
    if (index == 0) {
        pr_info("ctx %d reserve dummy fifo.\n", ctx_id);
        spin_unlock_irqrestore(&g_sc->sc_lock, flags);
        return;
    }

    if (!kfifo_is_full(&am_ctx[ctx_id].sc_fifo_in)) {
        kfifo_in(&am_ctx[ctx_id].sc_fifo_in, &buf, sizeof(tframe_t*));
    } else {
        pr_info("sc fifo is full .\n");
    }
    //pr_err("sc dma buffer:%d, %d, %x\n", ctx_id, index, buf->primary.address);
    spin_unlock_irqrestore(&g_sc->sc_lock, flags);
}

uint32_t am_sc_get_width(int ctx_id)
{
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return -1;
    }
    return am_ctx[ctx_id].info.out_w;
}

void am_sc_set_width(int ctx_id, uint32_t src_w, uint32_t out_w)
{
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return;
    }
    if (am_ctx[ctx_id].info.src_w == 0)
        am_ctx[ctx_id].info.src_w = src_w;
    am_ctx[ctx_id].info.out_w = out_w;
}

uint32_t am_sc_get_startx(int ctx_id)
{
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return 0;
    }
    return am_ctx[ctx_id].info.startx;
}

uint32_t am_sc_get_starty(int ctx_id)
{
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return 0;
    }
    return am_ctx[ctx_id].info.starty;
}

void am_sc_set_fps(int ctx_id, uint32_t c_fps, uint32_t t_fps)
{
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return;
    }
}

uint32_t am_sc_get_fps(int ctx_id)
{
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return 0;
    }
    return am_ctx[ctx_id].info.t_fps;
}

void am_sc_set_startx(int ctx_id, uint32_t startx)
{
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return;
    }
    am_ctx[ctx_id].info.startx = startx;
}

void am_sc_set_starty(int ctx_id, uint32_t starty)
{
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return;
    }
    am_ctx[ctx_id].info.starty = starty;
}

uint32_t am_sc_get_crop_width(int ctx_id)
{
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return 0;
    }
    return am_ctx[ctx_id].info.c_width;
}

uint32_t am_sc_get_crop_height(int ctx_id)
{
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return 0;
    }
    return am_ctx[ctx_id].info.c_height;
}

void am_sc_set_crop_width(int ctx_id, uint32_t c_width)
{
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return;
    }
    am_ctx[ctx_id].info.c_width = c_width;
}

void am_sc_set_crop_height(int ctx_id, uint32_t c_height)
{
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return;
    }
    am_ctx[ctx_id].info.c_height = c_height;
}

void am_sc_set_crop_enable(int ctx_id)
{
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return;
    }
    g_sc->refresh = 1;
}

void am_sc_set_src_width(int ctx_id, uint32_t src_w)
{
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return;
    }
    am_ctx[ctx_id].info.src_w = src_w;
}

void am_sc_set_src_height(int ctx_id, uint32_t src_h)
{
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return;
    }
    am_ctx[ctx_id].info.src_h = src_h;
}

uint32_t am_sc_get_height(int ctx_id)
{
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return -1;
    }
    return am_ctx[ctx_id].info.out_h;
}

uint32_t am_sc_get_output_format(int ctx_id)
{
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return -1;
    }
    return am_ctx[ctx_id].info.out_fmt;
}


void am_sc_set_height(int ctx_id, uint32_t src_h, uint32_t out_h)
{
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return;
    }
    if (am_ctx[ctx_id].info.src_h == 0)
        am_ctx[ctx_id].info.src_h = src_h;
    am_ctx[ctx_id].info.out_h = out_h;
}

void am_sc_set_input_format(int ctx_id, uint32_t value)
{
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return;
    }
    am_ctx[ctx_id].info.in_fmt = RGB24;
}

void am_sc_set_output_format(int ctx_id, uint32_t value)
{
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return;
    }
    am_ctx[ctx_id].info.out_fmt = value;
}

void am_sc_set_buf_num(int ctx_id, uint32_t num)
{
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return;
    }
    am_ctx[ctx_id].req_buf_num = num;
}

int am_sc_set_callback(acamera_context_ptr_t p_ctx, buffer_callback_t sc0_callback)
{
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return -1;
    }
    am_ctx[p_ctx->fsm_mgr.ctx_id].callback = sc0_callback;
    am_ctx[p_ctx->fsm_mgr.ctx_id].ctx = p_ctx;
    return 0;
}

int am_sc_system_init(int ctx_id)
{
    int ret = 0, i = 0;

    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return -1;
    }

    if (am_ctx[ctx_id].req_buf_num == 0)
        return 0;

    if (!am_ctx[ctx_id].temp_buf) {
        am_ctx[ctx_id].temp_buf = (tframe_t*)kmalloc( sizeof(tframe_t) * (am_ctx[ctx_id].req_buf_num), GFP_KERNEL | __GFP_NOFAIL);
    }

    if (g_sc->stop_flag == false) return 0;

    for (i = 0; i < FRAME_DELAY_QUEUE; i ++) {
        g_sc->multi_camera.pre_frame[i] = NULL;
        g_sc->multi_camera.cam_id[i] = 0;
    }
    g_sc->start_delay_cnt = 0;

    return ret;
}

int am_sc_hw_init(int ctx_id, int is_print, int clip_mode)
{
    uint32_t flag = 0;
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return -1;
    }
    if (am_ctx[ctx_id].req_buf_num == 0)
        return 0;

    if ((g_sc->stop_flag == false) && !clip_mode) {
        pr_info("wrmif no stop %d\n", ctx_id);
        return 0;
    }

    sc_reg_rd(ISP_SCWR_TOP_CTRL, &flag);
    if ((flag & 0x01) && !clip_mode) {
        pr_info("wrmif Bit0 no stop\n");
        return 0;
    }

    if (sc_get_reg(ISP_SCWR_TOP_DBG1) != sc_get_reg(ISP_SCWR_TOP_DBG2)) {
        sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 1, 25, 1);
        sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 0, 25, 1);
        pr_info("wrmif DBG force stop: %x - %x\n", sc_get_reg(ISP_SCWR_TOP_DBG1), sc_get_reg(ISP_SCWR_TOP_DBG2));
    }

    if (!clip_mode)
        sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 1, 25, 1);

    if (sc_get_reg(ISP_SCWR_TOP_DBG1) != sc_get_reg(ISP_SCWR_TOP_DBG2)) {
        sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 1, 25, 1);
        sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 0, 25, 1);
        pr_info("wrmif DBG force skip: %x - %x\n", sc_get_reg(ISP_SCWR_TOP_DBG1), sc_get_reg(ISP_SCWR_TOP_DBG2));
        return 0;
    }

    int mtx_mode = 0;
    int dbg_mode = 0;
    int mux_sel = 0;
    unsigned long flags;
    spin_lock_irqsave( &g_sc->sc_lock, flags );
    if ( g_sc->stop_flag || g_sc->refresh ) init_sc_mif_setting(ctx_id, &g_sc->isp_frame);
    if (am_ctx[ctx_id].info.in_fmt == RGB24) {
        if (am_ctx[ctx_id].info.out_fmt == RGB24) {
            mtx_mode = 0;
        } else if ((am_ctx[ctx_id].info.out_fmt == NV12_YUV) ||
                (am_ctx[ctx_id].info.out_fmt == NV12_YVU) ||
                (am_ctx[ctx_id].info.out_fmt == UYVY) ||
                (am_ctx[ctx_id].info.out_fmt == AYUV)) {
            mtx_mode = 1;
        } else if (am_ctx[ctx_id].info.out_fmt == NV12_GREY) {
            mtx_mode = 1;
        } else if (am_ctx[ctx_id].info.out_fmt == RAW16) {
            mtx_mode = 0;
            dbg_mode = 2;
            mux_sel = 4;
        } else if (am_ctx[ctx_id].info.out_fmt == RAW_YUY2) {
            mtx_mode = 0;
            dbg_mode = 0;
        }
    } else if (am_ctx[ctx_id].info.in_fmt == AYUV) {
        if ((am_ctx[ctx_id].info.out_fmt == NV12_YUV) ||
            (am_ctx[ctx_id].info.out_fmt == NV12_YVU) ||
            (am_ctx[ctx_id].info.out_fmt == UYVY) ||
            (am_ctx[ctx_id].info.out_fmt == AYUV)) {
            mtx_mode = 0;
        } else if (am_ctx[ctx_id].info.out_fmt == RGB24) {
            mtx_mode = 2;
        } else if (am_ctx[ctx_id].info.out_fmt == NV12_GREY) {
            mtx_mode = 0;
        }
    } else if ((am_ctx[ctx_id].info.in_fmt == NV12_YUV) ||
            (am_ctx[ctx_id].info.in_fmt == NV12_GREY)) {
        if (am_ctx[ctx_id].info.out_fmt == NV12_GREY) {
            mtx_mode = 0;
        }
    } else {
        pr_err("unSupported format");
    }

    if ( am_ctx[ctx_id].info.c_width == 0 )
        am_ctx[ctx_id].info.c_width = am_ctx[ctx_id].info.src_w;

    if ( am_ctx[ctx_id].info.c_height == 0 )
        am_ctx[ctx_id].info.c_height = am_ctx[ctx_id].info.src_h;

    if ( ( am_ctx[ctx_id].info.c_width != am_ctx[ctx_id].info.src_w ) || ( am_ctx[ctx_id].info.c_height != am_ctx[ctx_id].info.src_h ) )
        am_ctx[ctx_id].info.clip_sc_mode |= CLIP_MODE;
    else
        am_ctx[ctx_id].info.clip_sc_mode &= ~CLIP_MODE;

    if ( !clip_mode )
        am_ctx[ctx_id].info.clip_sc_mode &= ~CLIP_MODE;

    if ( g_sc->port < 3 ) {
        if ( ( am_ctx[ctx_id].info.c_width != am_ctx[ctx_id].info.out_w ) || ( am_ctx[ctx_id].info.c_height != am_ctx[ctx_id].info.out_h ) )
            am_ctx[ctx_id].info.clip_sc_mode |= SC_MODE;
    }

    if ( is_print ) pr_info("ctx_id-%d, src_w = %d, src_h = %d, out_w = %d, out_h = %d, crop_w = %d, crop_h = %d, clip = %d, in_fmt:%d, out_fmt:%d\n",
            ctx_id, am_ctx[ctx_id].info.src_w, am_ctx[ctx_id].info.src_h, am_ctx[ctx_id].info.out_w, am_ctx[ctx_id].info.out_h, am_ctx[ctx_id].info.c_width,
            am_ctx[ctx_id].info.c_height, am_ctx[ctx_id].info.clip_sc_mode, am_ctx[ctx_id].info.in_fmt, am_ctx[ctx_id].info.out_fmt);

    u8 swap_uv = 0;
    if (am_ctx[ctx_id].info.in_fmt == RGB24) {
        if (am_ctx[ctx_id].info.out_fmt == NV12_YUV) {
            swap_uv = 1;
        } else if (am_ctx[ctx_id].info.out_fmt == NV12_YVU) {
            swap_uv = 0;
        } else if (am_ctx[ctx_id].info.out_fmt == UYVY) {
            swap_uv = 1;
        }
    } else if (am_ctx[ctx_id].info.in_fmt == AYUV) {
        if ((am_ctx[ctx_id].info.out_fmt == NV12_YUV) ||
            (am_ctx[ctx_id].info.out_fmt == NV12_YVU)) {
           swap_uv = 1;
        }
    }

    enable_isp_scale_new(1, 0, dbg_mode, am_ctx[ctx_id].info.clip_sc_mode & CLIP_MODE, am_ctx[ctx_id].info.startx,
        am_ctx[ctx_id].info.startx + am_ctx[ctx_id].info.c_width - 1, am_ctx[ctx_id].info.starty,  am_ctx[ctx_id].info.starty + am_ctx[ctx_id].info.c_height - 1,
        am_ctx[ctx_id].info.src_w, am_ctx[ctx_id].info.src_h, am_ctx[ctx_id].info.out_w, am_ctx[ctx_id].info.out_h, mux_sel, am_ctx[ctx_id].info.clip_sc_mode & SC_MODE, mtx_mode,
        swap_uv, am_ctx[ctx_id].ch_mode, &g_sc->isp_frame);

    sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 0, 25, 1);
    spin_unlock_irqrestore( &g_sc->sc_lock, flags );
    return 0;
}

int am_sc_start(int ctx_id)
{
    int ret = 0;
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return -1;
    }

    g_sc->user ++;
    if (g_sc->stop_flag == false) return 0;
#ifdef ENABLE_SC_BOTTOM_HALF_TASKLET
    tasklet_init( &g_sc->sc_tasklet.tasklet_obj, sc_do_tasklet, (unsigned long)&g_sc->sc_tasklet );
#else
    INIT_WORK( &sc_workqueue.work_obj, sc_do_workqueue);
#endif
    ret = request_irq(g_sc->irq, isp_sc_isr, IRQF_SHARED | IRQF_TRIGGER_RISING,
        "isp-sc-irq", (void *)g_sc);
    pr_info("%s irq = %d, ret = %d\n", __func__,g_sc->irq, ret);

    /* switch int to sync reset for start and delay frame */
    sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 1, 3, 1);
    g_sc->stop_flag = false;

    return 0;
}

int am_sc_reset(int ctx_id)
{
    return 0;
}

int am_sc_stop(int ctx_id)
{
    int i = 0;
    unsigned long flags = 0;
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return -1;
    }

    if (g_sc->user) g_sc->user --;

    if (!g_sc->stop_flag && !g_sc->user) {
        g_sc->stop_flag = true;
        g_sc->start_delay_cnt = 0;
        g_sc->working = 0;
        sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 0, 0, 1);
        sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 0, 3, 1);
        mdelay(66);
        pr_info("wrmif stop: %x, %x\n", sc_get_reg(ISP_SCWR_TOP_DBG1), sc_get_reg(ISP_SCWR_TOP_DBG2));

        for (i = 0; i < FRAME_DELAY_QUEUE; i ++) {
            g_sc->multi_camera.pre_frame[i] = NULL;
            g_sc->multi_camera.cam_id[i] = 0;
        }

        free_irq(g_sc->irq, (void *)g_sc);
#ifdef ENABLE_SC_BOTTOM_HALF_TASKLET
        tasklet_kill( &g_sc->sc_tasklet.tasklet_obj );
#endif
    }

    spin_lock_irqsave( &g_sc->sc_lock, flags );
    if (am_ctx[ctx_id].temp_buf != NULL) {
        kfree(am_ctx[ctx_id].temp_buf);
        am_ctx[ctx_id].temp_buf = NULL;
    }

    kfifo_reset(&am_ctx[ctx_id].sc_fifo_out);
    kfifo_reset(&am_ctx[ctx_id].sc_fifo_in);
    am_ctx[ctx_id].frame_id = 0;

    am_ctx[ctx_id].info.startx = 0;
    am_ctx[ctx_id].info.starty = 0;
    am_ctx[ctx_id].info.c_width = 0;
    am_ctx[ctx_id].info.c_height = 0;
    am_ctx[ctx_id].info.src_w = 0;
    am_ctx[ctx_id].info.src_h = 0;
    am_ctx[ctx_id].info.clip_sc_mode = 0;
    spin_unlock_irqrestore( &g_sc->sc_lock, flags );

    return 0;
}

int am_sc_system_deinit(int ctx_id)
{
    if (!g_sc) {
        pr_info("%d, g_sc is NULL.\n", __LINE__);
        return -1;
    }

    iounmap(g_sc->base_addr);
    g_sc->base_addr = NULL;

    kfree(g_sc);
    g_sc = NULL;
    return 0;
}

int am_sc_hw_deinit(int ctx_id)
{
    return 0;
}

void am_sc_hw_enable(void)
{
    sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 1, 1, 1);
    sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 1, 0, 1);
}

void am_sc_hw_disable(void)
{
    sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 0, 1, 1);
    sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 0, 0, 1);
}

void am_sc_set_workstatus(uint32_t status)
{
    int i = 0, ret = 0;
    uint32_t c_width = am_ctx[0].info.c_width;
    uint32_t c_height = am_ctx[0].info.c_height;
    uint32_t src_w = am_ctx[0].info.src_w;
    uint32_t src_h = am_ctx[0].info.src_h;
    uint32_t out_w = am_ctx[0].info.out_w;
    uint32_t out_h = am_ctx[0].info.out_h;
    uint32_t out_fmt = am_ctx[0].info.out_fmt;

    if (g_sc->stop_flag) return;

    g_sc->cam_id_last = system_get_last_api_context();
    g_sc->cam_id_current = system_get_current_api_context();
    g_sc->cam_id_next = system_get_next_api_context();
    g_sc->cam_id_next_next = system_get_next_next_api_context();

    g_sc->working = status;
    if (g_sc->user > 1) {
        for (i = 0; i < CAM_CTX_NUM - 1; i ++) {
            if (c_width != am_ctx[i + 1].info.c_width) {
                ret = -1;
                break;
            } else if (c_height != am_ctx[i + 1].info.c_height) {
                ret = -1;
                break;
            } else if (src_w != am_ctx[i + 1].info.src_w) {
                ret = -1;
                break;
            } else if (src_h != am_ctx[i + 1].info.src_h) {
                ret = -1;
                break;
            } else if (out_w != am_ctx[i + 1].info.out_w) {
                ret = -1;
                break;
            } else if (out_h != am_ctx[i + 1].info.out_h) {
                ret = -1;
                break;
            } else if (out_fmt != am_ctx[i + 1].info.out_fmt) {
                ret = -1;
                break;
            }
        }
    } else {
        if ((g_sc->cam_id_current != g_sc->cam_id_next) || (g_sc->cam_id_current != g_sc->cam_id_next_next))
            g_sc->refresh = 1;
    }

    if (ret != 0)
        g_sc->refresh = 1;
}

uint32_t am_sc_get_workstatus(void)
{
    return g_sc->working;
}

void am_sc_set_camid(uint32_t status)
{
    g_sc->cam_id_last = system_get_last_api_context();
    g_sc->cam_id_current = system_get_current_api_context();
    g_sc->cam_id_next = system_get_next_api_context();
    g_sc->cam_id_next_next = system_get_next_next_api_context();
}

void am_sc_reset_hwstatus(uint32_t status)
{
    if (status == SCMIF_ERR) {
        if ( am_ctx[g_sc->cam_id_next].temp_buf) {
            sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 0, 0, 1);
            sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 0, 1, 1);
            mdelay(66);
            g_sc->refresh = 1;
            am_sc_hw_init(g_sc->cam_id_next, 0, 1);
            g_sc->refresh = 0;
            sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 1, 1, 1);
            sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 1, 0, 1);
            sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 1, 28, 1);
            sc_wr_reg_bits(ISP_SCWR_TOP_CTRL, 0, 28, 1);
            pr_err("%s\n", __func__);
        }
    }
}

void am_sc_dcam(uint32_t status)
{
    g_sc->dcam_mode = status;
}
