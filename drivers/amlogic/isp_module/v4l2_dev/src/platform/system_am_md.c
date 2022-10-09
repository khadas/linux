/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2011-2018 ARM or its affiliates
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

#include <linux/irqreturn.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/ioport.h>
#include <linux/of_platform.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
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
#include <linux/kfifo.h>
#include <linux/completion.h>
#include <linux/jiffies.h>
#include <linux/version.h>
#include <linux/slab.h>
//#include <uapi/linux/sched/types.h>
#include <linux/kfifo.h>
#include <linux/completion.h>
#include <linux/jiffies.h>
#include <linux/version.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/freezer.h>
#include <linux/delay.h>

#include "acamera_types.h"
#include "acamera_logger.h"
#include "system_am_md.h"
#include "acamera_command_api.h"

#define AM_MD_IRQ         131
#define AM_MD_MEMORY      0x70000000
#define AM_MD_NAME        "amlogic, isp-md"

static struct am_md *g_md;

static uint32_t md_sad_sum[5] = {0};
static uint32_t md_1bm_sum[5] = {0};
//static int md_cnt = 0;
static int md_cls_thrd[2] = {10, 15};
extern resource_size_t paddr_md;

T_MD_PRM am_md_reg;
T_MD2_PRM am_md2_reg;


/*****************************************************************/
static uint32_t write_reg(unsigned long addr, uint32_t val)
{
    void __iomem *io_addr;
    io_addr = ioremap_nocache(addr, 8);
    if (io_addr == NULL) {
        LOG(LOG_ERR, "%s: Failed to ioremap addr\n", __func__);
        return -1;
    }
    __raw_writel(val, io_addr);
    iounmap(io_addr);
    return 0;
}

static inline void md_read_reg(int addr, uint32_t* val)
{
    void __iomem *base = g_md->base_addr;

    if (base != NULL) {
        base = base + (addr - ISP_MD2_BASE);
        *val = readl(base);
    } else
        pr_err("isp-md read register failed.\n");
    return;
}

static inline void md_write_reg(int addr, uint32_t val)
{
    void __iomem *base = g_md->base_addr;

    if (base != NULL) {
        base = base + (addr - ISP_MD2_BASE);
        writel(val, base);
    } else
        pr_err("isp-md write register failed.\n");

    return;
}

int param_md_input(T_MD_PRM *reg, int hsize_i, int vsize_i, int in_sel)
{
    reg->reg_md_input_sel = in_sel;
    reg->reg_md_input_xsize    = hsize_i;
    reg->reg_md_input_ysize    = vsize_i;
    reg->reg_md_inpc_winxxyy[0] = 0;           //u12, input combined window for processing. aligned to 2
                                               //u4
    reg->reg_md_inpc_winxxyy[1] = hsize_i;         //u12, input combined window for processing.
                                               //end
                                               //REG_NAME: MD_WINXY_1
                                               //u4
    reg->reg_md_inpc_winxxyy[2] = 0;           //u12, input combined window for processing.
                                               //u4
    reg->reg_md_inpc_winxxyy[3] = vsize_i;         //u12, input combined window for processing.

    if (reg->reg_md_input_sel>3) {
        reg->reg_md_is_on_raw      = 0;
        reg->reg_md_input_ls       = 0;
        reg->reg_md_eotf_en        = 0; //do oe function

                                                       //REG_NAME: MD_RGGB_COEF
        reg->reg_md_bld_coefs[0] = 16;               //u8,  blender coef of the components to Y, normalize to 128 as "1"
        reg->reg_md_bld_coefs[1] = 16;               //u8,  blender coef of the components to Y, normalize to 128 as "1"
        reg->reg_md_bld_coefs[2] = 16;               //u8,  blender coef of the components to Y, normalize to 128 as "1"
        reg->reg_md_bld_coefs[3] = 16;               //u8,  blender coef of the components to Y, normalize to 128 as "1"
                                                       //end
    }

    return 0;
}

int param_md_scale(T_MD_PRM *reg, int hsize_i, int vsize_i, int hsize_o, int vsize_o, int32_t base_axi_addr)
{
    reg->reg_ds_hstep =  (reg->reg_md_is_on_raw==1)?  hsize_i/2/hsize_o : hsize_i/hsize_o;
    reg->reg_ds_vstep =  (reg->reg_md_is_on_raw==1)?  vsize_i/2/vsize_o : vsize_i/vsize_o;

    reg->reg_ds_ocol  = (reg->reg_md_is_on_raw==1)? (hsize_i/2/reg->reg_ds_hstep):(hsize_i/reg->reg_ds_hstep);
    reg->reg_ds_orow  = (reg->reg_md_is_on_raw==1)? (vsize_i/2/reg->reg_ds_vstep):(vsize_i/reg->reg_ds_vstep);

    reg->reg_wr_base_addr  = base_axi_addr;     //u32,pre-pre base addr
                                               //end

                                               //REG_NAME: MD_RD_P_BADDR
    reg->reg_rd_base_addr  = base_axi_addr;     //u32,pre-pre base addr
                                               //end
    reg->reg_wr_total_size = reg->reg_ds_ocol * reg->reg_ds_orow;
    reg->reg_rd_total_size = reg->reg_ds_ocol * reg->reg_ds_orow;

    return 0;
}

int param_md_detect(int md_sad_thrd, int md_1bm_thrd)
{
    am_md_reg.reg_ismot_sad_thrd = md_sad_thrd;
    am_md_reg.reg_ismot_1bm_thrd = md_1bm_thrd;
    write_reg(MD_ISMOT_SAD_THRD, am_md_reg.reg_ismot_sad_thrd);// = 65536;
    write_reg(MD_ISMOT_1BM_THRD, am_md_reg.reg_ismot_1bm_thrd);// = 4096;
    return 0;
}

int param_md_set(T_MD_PRM *reg)
{
    int k,sel;
    sel = 0;
    write_reg(MD_TOP_CTRL, reg->reg_md_top_ctrl);//          = 0x3c0204;
    write_reg(MD_WR_CTRL0, (reg->reg_wr_axi_wr_en<<31)|
                           (reg->reg_wr_axi_req_en<<30)|
                           (reg->reg_wr_axi_bypass<<29)|
                           (reg->reg_wr_total_size & 0xffffff));//        =57600;

    write_reg(MD_WR_CTRL1, reg->reg_wr_base_addr);// = 0x80000000;

    write_reg(MD_RD_CTRL0, (reg->reg_rd_axi_rd_en<<31)|
                           (reg->reg_rd_axi_req_en<<30)|
                           (reg->reg_rd_total_size & 0xffffff));//        =57600;

    write_reg(MD_RD_CTRL1, reg->reg_rd_base_addr);//  =   0x80000000;

    write_reg(MD_INPUT_SIZE,
       ((reg->reg_md_input_xsize & 0x1fff) <<16) |
        (reg->reg_md_input_ysize & 0x1fff));

    write_reg(MD_WINXY_0,
      ((reg->reg_md_inpc_winxxyy[0] & 0x1fff)<<16) |
      ((reg->reg_md_inpc_winxxyy[1] & 0x1fff)));

    write_reg(MD_WINXY_1,
      ((reg->reg_md_inpc_winxxyy[2] & 0x1fff)<<16) |
      ((reg->reg_md_inpc_winxxyy[3] & 0x1fff)));

    write_reg(MD_RGGB_OFSET_0 , reg->reg_md_rggb_ofset[0]);
    write_reg(MD_RGGB_OFSET_1 , reg->reg_md_rggb_ofset[1]);
    write_reg(MD_RGGB_OFSET_2 , reg->reg_md_rggb_ofset[2]);
    write_reg(MD_RGGB_OFSET_3 , reg->reg_md_rggb_ofset[3]);

    write_reg(MD_RGGB_GAIN_0,
      ((reg->reg_md_rggb_gain[0]  & 0xfff)<<16) |
      ((reg->reg_md_rggb_gain[1]  & 0xfff)));

    write_reg(MD_RGGB_GAIN_1,
      ((reg->reg_md_rggb_gain[2]  & 0xfff)<<16) |
      ((reg->reg_md_rggb_gain[3]  & 0xfff)));

    write_reg(MD_RGGB_COEF,
      ((reg->reg_md_bld_coefs[0] & 0xff)<<24)|
      ((reg->reg_md_bld_coefs[1] & 0xff)<<16)|
      ((reg->reg_md_bld_coefs[2] & 0xff)<<8)|
      ((reg->reg_md_bld_coefs[3] & 0xff)));

    for(k=0; k<20; k++) {
    write_reg(MD_COMB_EOTF_0 + k*4,
       ((reg->reg_md_comb_eotf[3*k]    & 0x3ff) << 22)|
       ((reg->reg_md_comb_eotf[3*k+1]  & 0x3ff) << 12)|
       ((reg->reg_md_comb_eotf[3*k+2]  & 0x3ff) <<  2));

    }
    write_reg(MD_COMB_EOTF_20, reg->reg_md_comb_eotf[60] & 0x3ff);

    write_reg(MD_EOTF,
        (reg->reg_md_bld_use_max << 1) |
         reg->reg_md_eotf_en);

    write_reg(MD_DS_STEP,
        (reg->reg_ds_hstep << 16) |
         reg->reg_ds_vstep);

    write_reg(MD_DS_OSIZE,
        (reg->reg_ds_ocol << 16) |
         reg->reg_ds_orow);

    write_reg(MD_DS_CTRL,
        (reg->reg_ds_inp8b <<10)|
         reg->reg_ds_norm);

    write_reg(MD_CORE_CTRL0,
        ((reg->reg_md_sad_mode & 0x7)<<25) |
        ((reg->reg_md_edge_mode & 0x1) << 24) |
        ((reg->reg_md_coring & 0xff) <<16) |
        ((reg->reg_md_edge_ratio & 0x3f) << 8));

    write_reg(MD_WIN_XXYY0 ,
      ((reg->reg_md_win_xxyy[0] & 0x3ff)<<16) |
      ((reg->reg_md_win_xxyy[1] & 0x3ff)));

    write_reg(MD_WIN_XXYY1,
      ((reg->reg_md_win_xxyy[2] & 0x3ff)<<16) |
      ((reg->reg_md_win_xxyy[3] & 0x3ff)));


    write_reg(MD_ISMOT_SAD_THRD, reg->reg_ismot_sad_thrd);// = 65536;
    write_reg(MD_ISMOT_1BM_THRD, reg->reg_ismot_1bm_thrd);// = 4096;
    write_reg(MD_WR_P_BADDR, reg->reg_wr_base_addr_p);// = 0x80008000;
    write_reg(MD_RD_P_BADDR, reg->reg_rd_base_addr_p);// = 0x80008000;


    write_reg(MD_INPUT_CTRL0,
        ((reg->reg_md_enable           & 0x1) <<31)|
        ((reg->reg_md_raw_xphase_ofst  & 0x1) <<30)|
        ((reg->reg_md_raw_yphase_ofst  & 0x1) <<29)|
        ((reg->reg_md_is_on_raw        & 0x1) <<28)|
        ((reg->reg_md_input_sel        & 0xf) <<24)|
        ((reg->reg_md_input_ls         & 0xf) <<20));

    return 0;
}

static int set_tnr_md_level( uint32_t ctx_id, int val )
{
    int result;
    uint32_t ret_val;

    result = acamera_command( ctx_id, TSCENE_MODES, TNR_MD_LELVE_ID, val, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set TNR_MD_LELVE_ID to %d, ret_value: %d.", val, result );
        return result;
    }

    return 0;
}

static int set_tnr_md_mtn( uint32_t ctx_id, int val )
{
    int result;
    uint32_t ret_val;

    result = acamera_command( ctx_id, TSCENE_MODES, TNR_MD_MTN_ID, val, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set TNR_MD_LELVE_ID to %d, ret_value: %d.", val, result );
        return result;
    }

    return 0;
}

static int get_tnr_md_mode( uint32_t ctx_id )
{
    int result;
    uint32_t ret_val;

    result = acamera_command( ctx_id, TSCENE_MODES, TNR_MD_MODE_ID, 0, COMMAND_GET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set TNR_MD_MODE_ID, ret_value: %d.", result );
        return result;
    }

    return ret_val;
}

static int get_tnr_md_thrd1( uint32_t ctx_id )
{
    int result;
    uint32_t ret_val;

    result = acamera_command( ctx_id, TSCENE_MODES, TNR_MD_THRD1_ID, 0, COMMAND_GET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set TNR_MD_THRD1_ID, ret_value: %d.", result );
        return result;
    }

    return ret_val;
}

static int get_tnr_md_thrd2( uint32_t ctx_id )
{
    int result;
    uint32_t ret_val;

    result = acamera_command( ctx_id, TSCENE_MODES, TNR_MD_THRD2_ID, 0, COMMAND_GET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set TNR_MD_THRD2_ID, ret_value: %d.", result );
        return result;
    }

    return ret_val;
}

static int get_tnr_md_log( uint32_t ctx_id )
{
    int result;
    uint32_t ret_val;

    result = acamera_command( ctx_id, TSCENE_MODES, TNR_MD_LOG_ID, 0, COMMAND_GET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set TNR_MD_THRD2_ID, ret_value: %d.", result );
        return result;
    }

    return ret_val;
}

int motion_classify(int* md_sad, // ro_md_sad_sum, sum of the motion sad for all the pixels within the window;
                    int* md_1bm, // ro_md_1bm_sum, sum of the 1bit motion for all the pixels within the window;
                    int md_cls_mode,   // motion classification mode, 0: only use 1bm, 1: use 1bm and sad, default = 0,
                    int* md_cls,  // motion thresholds for motion level classification, default (for mode0) thrd[0]=1/10 pic size, thrd[0]=4/10 pic size,
                    int *offset)
{
    int k;
    int md_sad_flt = 0;
    int md_1bm_flt = 0;
    int mtn_value = 0;
    int mtn_level = 0;//0: still, 1: small motion; 2: large/sure motion, default value could be changed for real cases
    int flt[] = {1,2,2,2,1};

    for (k = 0; k < 4; k++) {//k
        md_sad_flt += flt[k] * md_sad[k];
        md_1bm_flt += flt[k] * md_1bm[k];
    }
    md_sad_flt >>= 3;//normalized sad filtered
    md_1bm_flt >>= 3;//normalized 1bm filtered

    if (md_1bm_flt == 0)
        return 0;

    if (md_cls_mode == 0) {
        mtn_value = md_1bm_flt;
    } else {
        mtn_value = md_1bm_flt==0 ? 0 : (md_sad_flt/md_1bm_flt);
    }
    if (mtn_value < md_cls[0]) {
        mtn_level = 0;
    } else if (mtn_value < md_cls[1]) {
        mtn_level = 1;
    } else {
        mtn_level = 2;
    }
    if (get_tnr_md_log(g_md->ctx_id) == 1 || get_tnr_md_log(g_md->ctx_id) == 3)
        pr_err("mtn value: %d, md_level:%d, mode:%d, thrd1:%d, thrd2:%d, sad_thrd:%d, 1bm_thrd:%d\n",
                mtn_value, mtn_level, md_cls_mode, md_cls[0], md_cls[1], am_md_reg.reg_ismot_sad_thrd, am_md_reg.reg_ismot_1bm_thrd);
    *offset = mtn_value;

    return mtn_level;
}

#if 0
static uint32_t md_read_reg(int addr)
{
    void __iomem *base = g_md->base_addr;
    uint32_t ret = 0;

    if (base != NULL) {
        base = base + addr;
        ret = readl(base);
    } else
        pr_err("isp-md read register failed.\n");

    return ret;
}

static irqreturn_t motion_detection_isr(int irq, void *para)
{
    md_1bm_sum[0] = md_1bm_sum[1];
    md_1bm_sum[1] = md_1bm_sum[2];
    md_1bm_sum[2] = md_1bm_sum[3];
    md_1bm_sum[3] = md_1bm_sum[4];
    md_1bm_sum[4] = md_read_reg(RO_MD_1BM_SUM - ISP_MD_REG_START);

    md_sad_sum[0] = md_sad_sum[1];
    md_sad_sum[1] = md_sad_sum[2];
    md_sad_sum[2] = md_sad_sum[3];
    md_sad_sum[3] = md_sad_sum[4];
    md_sad_sum[4] = md_read_reg(RO_MD_SAD_SUM - ISP_MD_REG_START);
    g_md->f_motion = 1;
    wake_up_interruptible( &g_md->tnr_wq);
    //pr_err("motion event\n");
    return IRQ_HANDLED;
}
#endif

irqreturn_t motion_detection2_isr(int irq, void *para)
{
    pr_err("motion event---\n");
    return IRQ_HANDLED;
}

static int tnr_md_level_thread( void *data )
{
    set_freezable();
    int level = 0;
    int mode = 0;
    int offset = 0;
    for ( ;; ) {
        try_to_freeze();

        if ( kthread_should_stop() )
            break;

        if ( wait_event_interruptible_timeout( g_md->tnr_wq,
            (g_md->f_motion == 1),
            msecs_to_jiffies( 100 ) ) <= 0 ) {
            //pr_info("Error: wait_event return < 0\n");
            continue;
        }

        mode = get_tnr_md_mode(g_md->ctx_id);
        md_cls_thrd[0] = get_tnr_md_thrd1(g_md->ctx_id);
        md_cls_thrd[1] = get_tnr_md_thrd2(g_md->ctx_id);
        if (md_cls_thrd[0] >=  md_cls_thrd[1]) {
            g_md->f_motion = 0;
            continue;
        }

        level = motion_classify(md_sad_sum, md_1bm_sum, mode, md_cls_thrd, &offset);
        set_tnr_md_level(g_md->ctx_id, level);
        set_tnr_md_mtn(g_md->ctx_id, offset);
        //if(get_tnr_md_log(0) == 1)
            //pr_err("Motion detection event, level = %d, mode:%d, thrd1:%d, thrd2:%d, f_motion:%d\n", level, mode, md_cls_thrd[0], md_cls_thrd[1], g_md->f_motion);
        g_md->f_motion = 0;

    }
    return 0;
}

int am_md_parse_dt(struct device_node *node)
{
    int rtn = -1;
    int irq = -1;
    struct resource rs;
    struct am_md *t_md = NULL;

    if (node == NULL) {
        pr_err("%s: Error input param\n", __func__);
        return -1;
    }

    rtn = of_device_is_compatible(node, AM_MD_NAME);
    if (rtn == 0) {
        pr_err("%s: Error match compatible\n", __func__);
        return -1;
    }

    t_md = kzalloc(sizeof(*t_md), GFP_KERNEL);
    if (t_md == NULL) {
        pr_err("%s: Failed to alloc isp-md\n", __func__);
        return -1;
    }

    t_md->of_node = node;

    rtn = of_address_to_resource(node, 0, &rs);
    if (rtn != 0) {
        pr_err("%s:Error get isp-md reg resource\n", __func__);
        goto reg_error;
    }

    pr_info("%s: rs idx info: name: %s\n", __func__, rs.name);
    if (strcmp(rs.name, "isp_md") == 0) {
        t_md->reg = rs;
        t_md->base_addr = ioremap_nocache(t_md->reg.start, resource_size(&t_md->reg));
    }

    irq = irq_of_parse_and_map(node, 0);
    if (irq <= 0) {
        pr_err("%s:Error get isp_md irq\n", __func__);
        goto irq_error;
    }

    t_md->irq = irq;
    pr_info("%s:rs info: irq: %d\n", __func__, t_md->irq);

    t_md->p_dev = of_find_device_by_node(node);

    g_md = t_md;

    return 0;
irq_error:
    iounmap(t_md->base_addr);
    t_md->base_addr = NULL;

reg_error:
    if (t_md != NULL)
        kfree(t_md);

    return -1;
}


/*************************************************************************
    > File Name: isp_md_pkg.c
  > Author: ma6174
  > Mail: ma6174@163.com
  > Created Time: Thu 09 May 2019 02:18:28 PM PDT
 ************************************************************************/
//int param_md_init(T_MD_PRM *reg, int xsize, int ysize)
int param_md_init(T_MD_PRM *reg)
{
    //int xsize = 1920;
    //int ysize = 1080;
    int k;
#if 0
    int bld_coefs[4] = {32, 32, 32, 32};
    // (256)x   4xstep(1+1 + 2 + 4 + 8 + 16 +32 +64 +128 + 128 + 128 + 128 + 128 + 128 + 128) = 61 taps
    int eotf_u12[61] = {  0,   1,   2,   3,   4,    5,   6,   7,   8,  // xdelt = 1, 1
                              10,  12,  14,  16,   20,  24,  28,  32,  // xdelt = 2, 4
                              40,  48,  56,  64,   80,  96, 112, 128,  // xdelt = 8, 16
                             160, 192, 224, 256,  320, 384, 448, 512,  // xdelt = 32, 64
                             640, 768, 892,1024, 1152,1280,1408,1536,  // xdelt = 128
                            1664,1792,1920,2048, 2176,2304,2432,2560,
                            2688,2816,2944,3072, 3200,3328,3456,3584,
                            3712,3840,3968,4095};                      // linear one
    int eotf_u12_sqrt[61] = {   0,  64,  91, 111, 128,   143, 157, 169, 181, // xdelt = 1, 1
                                   202, 222, 239, 256,   286, 314, 339, 362, // xdelt = 2, 4
                                   405, 443, 479, 512,   572, 627, 677, 724, // xdelt = 8, 16
                                   810, 887, 958,1024,  1145,1254,1355,1448, // xdelt = 32, 64
                                  1619,1774,1911,2048,  2172,2290,2401,2508, // xdelt = 128
                                  2611,2709,2804,2896,  2985,3072,3156,3238,
                                  3318,3396,3473,3547,  3620,3692,3762,3831,
                                  3899,3966,4031,4095};                // square root
    int inpc_winxxyy[4] = {0, xsize, 0, ysize};
    int win_xxyy[4]= {0, 1023, 0, 1023};
#endif
    //MIN((eotf_u10+2)>>2,1023):
    int eotf_u10[61] = {  0,   0,   1,   1,   1,    1,   2,   2,   2,  // xdelt = 1, 1
                               3,   3,   4,   4,    5,   6,   7,   8,  // xdelt = 2, 4
                              10,  12,  14,  16,   20,  24,  28,  32,  // xdelt = 8, 16
                              40,  48,  56,  64,   80,  96, 112, 128,  // xdelt = 32, 64
                             160, 192, 223, 256,  288, 320, 352, 384,  // xdelt = 128
                             416, 448, 480, 512,  544, 576, 608, 640,
                             672, 704, 736, 768,  800, 832, 864, 896,
                             928, 960, 992,1023};                      // linear one
    //RTL CONTROL
                                                  //REG_NAME: MD_TOP_GCLK
    reg->reg_md_gclk           = 0;               // u32: clock gate control
                                                    // end

                                                  //REG_NAME: MD_TOP_CTRL
    reg->reg_md_top_ctrl           = 0x3c0204;       // u32: md top ntrol
                                                  // end
                                               //REG_NAME: MD_WR_CTRL0
    reg->reg_wr_axi_wr_en         =1;            //u1:   axi wr enable
    reg->reg_wr_axi_req_en        =1;            //u1:   axi request enable
    reg->reg_wr_axi_bypass        =0;            //u1:   bypass axi wr
    reg->reg_wr_total_size        =57600;    //u24:  pixels hsize * vsize
                                               //end
                                               //REG_NAME: MD_WR_CTRL1
    reg->reg_wr_base_addr = 0x80000000;                       //u32:  wr axi base address
                                               //end
                                               //REG_NAME: MD_WR_CTRL2
                                               //u9
    reg->reg_wr_burst_lens= 2;                      //u3: burst_lens limiation, burst_size: 0: 1x128, 1: 2x128, 2£þ3: 4x128
    reg->reg_wr_req_th = 4;                          //u4: fifo depth req_th * 8 *128 bits in fifo.  >=1
    reg->reg_wr_urgent_ctrl = 0;                     //u16: urgent control, randome
                                               //end
                                               //REG_NAME: MD_WR_CTRL3
    reg->reg_wr_awid = 0;                            //u8: wr id  ,random
    reg->reg_wr_awprot = 0;                          //u3: awprot for security control,random
                                                     //u21
                                                     //end
                                                   //REG_NAME:MD_RO_WR_ST0
    reg->RO_wr_st0 = 0;                               //u32
                                                   //end
                                                   //REG_NAME:MD_RO_WR_ST1
    reg->RO_wr_st1 = 0;                               //u32
                                                   //end
                                                   //REG_NAME:MD_RO_WR_ST2
    reg->RO_wr_st2 = 0;                            //u32
                                                   //end

                                              //REG_NAME: MD_RD_CTRL0
    reg->reg_rd_axi_rd_en   =1;                //u1: axi rd enable
    reg->reg_rd_axi_req_en  =1;                //u1: axi request enable
                                              //u6
    reg->reg_rd_total_size  = 57600;                //u24: total size hsize*vsize, <= 1024*1024
                                              //end
                                              //REG_NAME: MD_RD_CTRL1
    reg->reg_rd_base_addr  =   0x80000000;                //u32
                                              //end
                                              //REG_NAME:MD_RD_CTRL2
                                              //u9
    reg->reg_rd_burst_lens  = 2 ;                //u3: burst_lens limiation, burst_size: 0: 1x128, 1: 2x128, 2£þ3: 4x128
    reg->reg_rd_req_th      = 4 ;                //u4: fifo depth req_th * 8 *128 bits in fifo.
    reg->reg_rd_urgent_ctrl = 0 ;                //u16: urgent control
                                              //end
                                              //REG_NAME:MD_RD_CTRL3
    reg->reg_rd_arid          = 0 ;                //u8: wr id
    reg->reg_rd_arprot         = 0;                //u3: awprot for security control
                                                   //u21
                                              //end
                                              //REG_NAME:MD_RO_RD_ST0
    reg->RO_rd_st0         =0  ;                //u32
                                              //end
                                              //REG_NAME:MD_RO_RD_ST1
    reg->RO_rd_st1         =0  ;                //u32
                                              //end


    // cmodel control
                                                  //REG_NAME: MD_INPUT_CTRL0
    reg->reg_md_enable         = 1;               //u1: lp motion detection enable, 0: disable;  1: enable
    reg->reg_md_raw_xphase_ofst= 0;               //u1: horizontal phase of the raw data, 0: start wz R-col (RGGB or GBRG); 1: start wz B-col (GRBG or BGGR)
    reg->reg_md_raw_yphase_ofst= 0;               //u1: vertical phase of the raw data,   0: start wz R-row (RGGB or GRBG); 1: start wz B-row (GRBG or BGGR)
    reg->reg_md_is_on_raw      = 1;               //u1: the md_sel is on raw data, set together with md_sel.    0: YUV/RGB;  1:RAW

    reg->reg_md_input_sel      = 1;
// 0: raw sensor input; 1: raw WDR stitch; 2:raw fed_out(nr_in); 3:raw mirror_in; 4: RGB after dms; 5: IRout; 6:RGB/YUV after gamma; 7: fe_o yuv bypass , default=1

    reg->reg_md_input_ls       = 0;               //u4, E domain data left shift bit num to align with u20 Odomain data and will clip to u10 by droping 10lsb, e.g. yuv is u12, then ls = 8;
                                                  //u20
                                                  //end

                                                  //REG_NAME: MD_INPUT_SIZE
                                                  //u3
    reg->reg_md_input_xsize    = 1920;           //u13, xsize of the input in pixels, set to image xsize
                                                  //u3
    reg->reg_md_input_ysize    = 1080;           //u13, ysize of the input in pixels, set to image ysize
                                                  //end

    //step-I O domain reg_RGGB data do the black-level sub, gain, OETF
    //for (k=0;k<4;k++) {
    //    if (reg->reg_md_is_on_raw==1)
    //        reg->reg_md_inpc_winxxyy[k] = inpc_winxxyy[k]>>1;//u12x4, input combined window for processing.
    //    else
    //        reg->reg_md_inpc_winxxyy[k] = inpc_winxxyy[k];   //u12x4, input combined window for processing.
                                               //REG_NAME: MD_WINXY_0
                                               //u4
    reg->reg_md_inpc_winxxyy[0] = 0;           //u12, input combined window for processing. aligned to 2
                                               //u4
    reg->reg_md_inpc_winxxyy[1] = 960;         //u12, input combined window for processing.
                                               //end
                                               //REG_NAME: MD_WINXY_1
                                               //u4
    reg->reg_md_inpc_winxxyy[2] = 0;           //u12, input combined window for processing.
                                               //u4
    reg->reg_md_inpc_winxxyy[3] = 540;         //u12, input combined window for processing.
                                               //end

                                               //REG_NAME: MD_RGGB_OFSET_0
                                               //u12
    reg->reg_md_rggb_ofset[0]= 0;          //s20, ofset to the components, same as the ISP mission mode ofset
                                               //end
                                               //REG_NAME: MD_RGGB_OFSET_1
                                               //u12
    reg->reg_md_rggb_ofset[1]= 0;          //s20, ofset to the components, same as the ISP mission mode ofset
                                               //end
                                               //REG_NAME: MD_RGGB_OFSET_2
                                               //u12
    reg->reg_md_rggb_ofset[2]= 0;          //s20, ofset to the components, same as the ISP mission mode ofset
                                               //end
                                               //REG_NAME: MD_RGGB_OFSET_3
    //u12
    reg->reg_md_rggb_ofset[3]= 0;          //s20, ofset to the components, same as the ISP mission mode ofset
                                               //end


    //reg->reg_md_rggb_gain[k] = 1024;         //u12x4, gain to different channel, normalize to 1024 as "1.0"
                                                   //REG_NAME: MD_RGGB_GAIN_0
                                                   //u4
    reg->reg_md_rggb_gain[0] = 1024;           //u12, gain to different channel, normalize to 1024 as "1.0"
                                                   //u4
    reg->reg_md_rggb_gain[1] = 1024;           //u12, gain to different channel, normalize to 1024 as "1.0"
                                                   //end
                                                   //REG_NAME: MD_RGGB_GAIN_1
                                                   //u4
    reg->reg_md_rggb_gain[2] = 1024;           //u12, gain to different channel, normalize to 1024 as "1.0"
                                                   //u4
    reg->reg_md_rggb_gain[3] = 1024;           //u12, gain to different channel, normalize to 1024 as "1.0"
                                                   //end
    //reg->reg_md_bld_coefs[k] = bld_coefs[k]; //u8x4,  blender coef of the components to Y, normalize to 128 as "1"

                                                   //REG_NAME: MD_RGGB_COEF
    reg->reg_md_bld_coefs[0] = 32;             //u8,  blender coef of the components to Y, normalize to 128 as "1"
    reg->reg_md_bld_coefs[1] = 32;             //u8,  blender coef of the components to Y, normalize to 128 as "1"
    reg->reg_md_bld_coefs[2] = 32;             //u8,  blender coef of the components to Y, normalize to 128 as "1"
    reg->reg_md_bld_coefs[3] = 32;             //u8,  blender coef of the components to Y, normalize to 128 as "1"
                                                   //end

    //}
    //for (k=0;k<61;k++)
    //reg->reg_md_comb_eotf[k] = MIN((eotf_u12[k]+2)>>2, 1023);            //u10x61,OETF, piece wise lut   TODO
    for(k=0; k<20; k++) {
                                                         //REG_NAME: MD_COMB_EOTF
        reg->reg_md_comb_eotf[3*k]    = eotf_u10[3*k];   //u10, piec wise lut
        reg->reg_md_comb_eotf[3*k+1]  = eotf_u10[3*k+1]; //u10, piec wise lut
        reg->reg_md_comb_eotf[3*k+2]  = eotf_u10[3*k+2]; //u10, piec wise lut
                                                         //u2
                                                         //end
    }
    //for(k=20; k<21; k++) {
                                                         //REG_NAME: MD_COMB_EOTF_20
                                                         //u22
        reg->reg_md_comb_eotf[60]    = eotf_u10[60];   //u10, piec wise lut
                                                         //end
    //}
                                                   //REG_NAME: MD_EOTF
                                                   //u30
    reg->reg_md_bld_use_max  = 0;                  //u1, use max of the components for the blender, 0: no max, 1:max (more sensitive)
    reg->reg_md_eotf_en      = 1;                     //u1,OETF, piece wise lut enable
                                                   //end
    //step-II the downscaled frame buffer size
                                                   //REG_NAME: MD_DS_STEP
                                                   //u12
    reg->reg_ds_hstep = 3;                         //u4: integer pixel ratio for horizontal scalar, o2i_ratio= 1/step, 1£þ15
                                                   //u12
    reg->reg_ds_vstep = 3;                         //u4: integer pixel ratio for vertical scalar,   o2i_ratio= 1/step??1£þ15
                                                   //end

                                                   //REG_NAME: MD_DS_OSIZE
                                                   //u4
    //reg->reg_ds_ocol  = (reg->reg_md_is_on_raw==1)? (xsize/2/reg->reg_ds_hstep+1):(xsize/reg->reg_ds_hstep+1);                      //u12: actual downscaled field buffer col number.
    //<1024x1024
    reg->reg_ds_ocol  = 320;                       //u12: actual downscaled field buffer col number.
                                                   //u4
    //reg->reg_ds_orow  = (reg->reg_md_is_on_raw==1)? (ysize/2/reg->reg_ds_vstep+1):(ysize/reg->reg_ds_vstep+1);                      //u12: actual downscaled field buffer row number.
    reg->reg_ds_orow  = 180;                      //u12: actual downscaled field buffer row number.
                                                  //end

                                                  //REG_NAME: MD_DS_CTRL
                                                   //u21
    reg->reg_ds_inp8b = 0;                         //u1, input to ds accum cell use 8bits instead of 10bits,only set to 1 when (v_step*h_step>64) 0: input 10bits, 1: input 8bits
    //reg->reg_ds_norm  = MIN(((reg->reg_ds_inp8b)? 4096:1024)/(reg->reg_ds_hstep*reg->reg_ds_vstep), 1023);                   //u10; normalization gain to the accum to get the 8bits data for ddr, norm = (inp8b? 4096:1024)/pixelnum(v_step*h_step)
    reg->reg_ds_norm  = 256;                      //u10; normalization gain to the accum to get the 8bits data for ddr, norm = (inp8b? 4096:1024)/pixelnum(v_step*h_step)
                                                  //end


    //step-III do the sad calculation and sum-up for RO
                                                      //REG_NAME: MD_CORE_CTRL0
                                                      //u4
    reg->reg_md_sad_mode   = 2;                       //u3,  window size for sad, 0: no sad, 1: 1x7,  2:3x3; 3:3x5, else 3x7
    reg->reg_md_edge_mode  = 0;                       //u1, mode for the pre and cur edge calculation, 0: min(edge_pre, edge_cur);  1: avg(edge_pre, edge_cur)
    reg->reg_md_coring     = 6;                       //u8, coring for sad for noise robustness. sad_core = max(sad-coring,0);
                                                      //u2
    reg->reg_md_edge_ratio = 4;                       //u6, ratio to edge as dynamic coring part, final_coring or 1b_thrd = (edge*ratio + coring); norm to 16 as '1'
                                                      //u8
                                                      //end
    //for (k=0;k<4; k++)
    //reg->reg_md_win_xxyy[k] = win_xxyy[k];         //u8x4: window for motion sum statistics on down-scaled image. [x_st x_ed; y_st y_ed]*4

    // set the win_xxyy, avoid the out of range
    //if (reg->reg_md_is_on_raw==1)    {
    //    reg->reg_md_win_xxyy[1] = xsize/2/(reg->reg_ds_hstep);
    //    reg->reg_md_win_xxyy[3] = ysize/2/(reg->reg_ds_vstep);}
    //else{
    //    reg->reg_md_win_xxyy[1] = xsize/(reg->reg_ds_hstep);
    //    reg->reg_md_win_xxyy[3] = ysize/(reg->reg_ds_vstep);}
                                                     //REG_NAME: MD_WIN_XXYY0
                                                     //u6
    reg->reg_md_win_xxyy[0]  = 0;                    //u10 window for motion sum statistics on down-scaled image. [x_st x_ed; y_st y_ed]*4
                                                     //u6
    reg->reg_md_win_xxyy[1]  = 1023;                 //u10 window for motion sum statistics on down-scaled image. [x_st x_ed; y_st y_ed]*4
                                                     //end
                                                     //REG_NAME: MD_WIN_XXYY1
                                                     //u6
    reg->reg_md_win_xxyy[2]  = 0;                    //u10 window for motion sum statistics on down-scaled image. [x_st x_ed; y_st y_ed]*4
                                                     //u6
    reg->reg_md_win_xxyy[3]  = 1023;                 //u10 window for motion sum statistics on down-scaled image. [x_st x_ed; y_st y_ed]*4
                                                     //end

    // the output sum of motion, put to read-only registers
                                                   //REG_NAME: RO_MD_SAD_SUM
                                                   //u4
    reg->RO_md_sad_sum = 0;                        //u28, sum of the motion sad for all the pixels within the window;
                                                   //end
                                                   //REG_NAME: RO_MD_1BM_SUM
                                                   //u12
    reg->RO_md_1bm_sum = 0;                        //u20, sum of the 1bit motion for all the pixels within the window;
                                                   //end
                                                   //REG_NAME: RO_MD_EDG_SUM
                                                   //u4
    reg->RO_md_edg_sum = 0;                        //u28, sum of the edge info for all the pixels within the window;
                                                   //end

    // to HW decision on is_motion and send out interupt to CPU
    //valid_num = (reg->reg_md_win_xxyy[1]-reg->reg_md_win_xxyy[0]+ 1)*(reg->reg_md_win_xxyy[3]-reg->reg_md_win_xxyy[2] + 1)*16; //2^^24
    //reg->reg_ismot_sad_thrd = MIN(16*((valid_num*64)>>10),  (1<<24)-1);             //u24, threshold to sum_sad for current frame is motion
                                                 //REG_NAME: MD_ISMOT_SAD_THRD
                                                 //u8
    reg->reg_ismot_sad_thrd = 65536;             //u24, threshold to sum_sad for current frame is motion
                                                 //end
                                                 //REG_NAME: MD_ISMOT_1BM_THRD
                                                 //u16
    //reg->reg_ismot_1bm_thrd = MIN(((valid_num*64)>>10), (1<<16)-1)    ;             //u16, threshold to sum_1bm for current frame consider as motion
    reg->reg_ismot_1bm_thrd = 4096;             //u16, threshold to sum_1bm for current frame consider as motion
                                                 //end

    //added for new feature by Augustine
                                               //REG_NAME: MD_WR_P_BADDR
    reg->reg_wr_base_addr_p  = 0x80008000;     //u32,pre-pre base addr
                                               //end

                                               //REG_NAME: MD_RD_P_BADDR
    reg->reg_rd_base_addr_p  = 0x80008000;     //u32,pre-pre base addr
                                               //end

    init_waitqueue_head( &g_md->tnr_wq );
    //static struct sched_param param;
    //param.sched_priority = 2;//struct sched_param param = { .sched_priority = 2 };
    g_md->kmd_thread = kthread_run( tnr_md_level_thread, NULL, "isp_md_tnr");
    //sched_setscheduler(g_md->kmd_thread, SCHED_IDLE, &param);
    g_md->f_motion = 0;
    wake_up_process(g_md->kmd_thread);

    return 0;
}
extern void cache_flush_for_device(uint32_t buf_start, uint32_t buf_size);

void isp_md2_init(T_MD2_PRM *prm_common,int xsize,int ysize)
{
    //int k, winx, winy, num_pix, half, is_rgb2yuv;
    //int win_xxyy[4]= {0, 255, 0, 255};

    // csc matrix
//    int rgb2yuv_matrix[9] = { 77, 150, 29,  -43, -85,  128,  128, -107, -21 };
    //int yuv2rgb_matrix[9] = { 256, 0, 359,  256, -88, -183,  256, 454,   0  };
                                                //REG_NAME: ISP_MD2_COMMON0
    prm_common->reg_ctrl0                = 0x4;    //u32, latch control
                                                //end
                                                //REG_NAME: ISP_MD2_COMMON1
    prm_common->reg_ctrl1                = 0xffffffff;    //u32, latch control
                                                //end
                                                //REG_NAME: ISP_MD2_COMMON2
    prm_common->reg_ctrl2                = 0x0001800;    //u32, latch control
                                                //end
                                                //REG_NAME: ISP_MD2_COMMON3
    prm_common->reg_num_cmp             = 4;    //u4, number cmp
    prm_common->reg_bit_depth           = 20;   //u8, bit_depth
    prm_common->reg_gclk                = 0;    //u8, clk gate
    prm_common->reg_insel               = 6;    //u4, input data channel select
    prm_common->reg_bypass              = 0;    //u1, axi wr bypass
                                                //u5
    prm_common->reg_soft_rst            = 0;    //u1, soft_rst
                                                //u1
                                                //end
                                                //REG_NAME:ISP_MD2_COMMON_SIZE
                                                //u2
    prm_common->reg_lXSizeIn  = xsize;          //u14, horizontal size
                                                //u2
    prm_common->reg_lYSizeIn  = ysize;          //u14, vertical size
                                                //end
                                                //REG_NAME: ISP_MD2AXI_CTRL
    prm_common->reg_axi_bsize  = 4;             //u8, each framesize buffer_size
    prm_common->reg_wr_burst_lens = 2;          //u2, busrt lens reg_wr_burst_lens   0: 1x128, 1: 2x128, 2‾3: 4x128
    prm_common->reg_wr_req_th     = 4;          //u4,  fifo depth req_th * 8 *128 bits in fifo.
    prm_common->reg_wr_awid       = 0;          //u8,  wid
    prm_common->reg_wr_awport     = 0;          //u3, urgent control
    prm_common->reg_axi_reserved  = 0;          //u7, reserved
                                                //end
                                                //REG_NAME: ISP_MD2AXI_CTRL0
    prm_common->reg_wr_urgent_ctrl = 0;         //u32, urgent control
                                                //end
                                                //REG_NAME: ISP_MD2AXI_BUFFSIZE
    prm_common->reg_axi_mode  = 1;              //u2, 0:always axi_addr, 1: axi_addr-axi_addrp pingpong, 2~3:buffer_mode, start with axi_addr
    prm_common->reg_axi_buff_rst  = 0;          //u1, frame buffer reset
    prm_common->reg_axi_fbuf   = 0x40000;       //max buf size 256*128*8Byte        //u29, axi_buffer size
                                                //end
                                                //REG_NAME: ISP_MD2AXI_FRAMESIZE
                                                //u3, reserved
    prm_common->reg_axi_fsize  = 0x40000*2;          //u29, axi_fsize
                                                //end
                                                //REG_NAME: ISP_MD2AXI_ADDR
    cache_flush_for_device(paddr_md, 0x40000*2);
    char* buf = phys_to_virt(paddr_md);
    memset(buf, 0x0, 0x40000*2);

    prm_common->reg_axi_addr  = (unsigned int)paddr_md;//268435456;      //u32, axi_addr
                                                //end
                                                //REG_NAME: ISP_MD2AXI_ADDRP
    prm_common->reg_axi_addrp  = (unsigned int)(paddr_md + 0x40000);//268435456;      //u32, axi_addrp
                                                //end


    // motion detection2 sta, (on RGB/YUV only)
                                                    //REG_NAME: MD2_CTRL
                                                    //u25
    prm_common->reg_md2_sta_enable= 1;                  //u1, enable of the md2 statistics. 0: disable 1: enable
    prm_common->reg_md2_sta_yuvin = 0;                  //u1, yuv or rgb input selection, 0: rgb; 1:yuv, default = 0


    prm_common->reg_md2_sta_y_by_ir=0;             //u1, enable of replace y by ir components. 0: disable 1: enable
    prm_common->reg_md2_sta_in_rs = (20-10);       //s4, input data righ shift before pre-shrink, x= BL-10,  range -2~6

                                                   //end


                                                   //REG_NAME: MD2_INPUT_SIZE

    prm_common->reg_md2_input_xsize= xsize;        //u16, xsize of the input in pixels, set to image xsize
    prm_common->reg_md2_input_ysize= ysize;        //u16, ysize of the input in pixels, set to image ysize
                                                   //end

                                                    //REG_NAME: MD2_WIND_XSTED
                                                    //u2
    prm_common->reg_md2_win_xxyy_0 = 0;               //u14: window for motion sum statistics on down-scaled image. [x_st x_ed; y_st y_ed]
                                                   //u2
    prm_common->reg_md2_win_xxyy_1 = xsize;            //u14
                                                   //end

                                                    //REG_NAME: MD2_WIND_YSTED
                                                    //u2
    prm_common->reg_md2_win_xxyy_2 = 0;                //u14
                                                    //u2
    prm_common->reg_md2_win_xxyy_3 = ysize-60;              //u14
                                                    //end

                                                    //REG_NAME: MD2_PRE_STEP_NORM
                                                 //u7
    prm_common->reg_md2_preds_hstp = 4;              //u5, integer pixel ratio for pre horizontal shrink, o2i_ratio= 1/step, 1~31
    prm_common->reg_md2_preds_vstp = 2;              //u4, integer pixel ratio for pre vertical shrink,   o2i_ratio= 1/step£¬1~15
                                                 //u4
    prm_common->reg_md2_preds_norm = 682;            //u12, normalization gain to the accum to get average of the block, norm = 4096/pixelnum(v_step*h_step)
                                                      //end

                                                       //REG_NAME: MD2_PRE_OUT_SIZE
                                                       //u6
    prm_common->reg_md2_preds_ocol = 512;                   //u10, pre-shrink output image horizontal size, maximum 512, floor((win_xxyy[1]-win_xxyy[0])/hstp).
                                                        //u6
    prm_common->reg_md2_preds_orow = 512;                  //u10, pre-shrink output image vertical size, maximum 512
                                                         //end

                                                         //REG_NAME: MD2_OFFSET_INP
                                                         //u5
    prm_common->reg_md2_offset_inp_1 = 0;                      // s11,s(BL+1)=s11 pre-offset before the 3x3 matrix.
                                                       //u5
    prm_common->reg_md2_offset_inp_0 = 0;                      // s11,s(BL+1)=s11 pre-offset before the 3x3 matrix.
                                                           //end
  // for(k=0;k<4;k++)
  // {
  //                                                             //REG_NAME: MD2_MATRIX
  //                                                             //u4
  //    prm_common->reg_md2_3x3matrix[2*k+1] = rgb2yuv_matrix[2*k+1];       // s12, 3x3 matrix coefs.
  //                                                                    //u4
  //    prm_common->reg_md2_3x3matrix[2*k] = rgb2yuv_matrix[2*k];       // s12, 3x3 matrix coefs.
  //                                                                   //end


  // }

                                                                //REG_NAME: MD2_MATRIX_4
                                                                //u14
    prm_common->reg_md2_3x3mtrx_rs =0;                              // u2:  coefs precision mode. 0 s3.8; 1: s2.9; 2: s1.10 3:s0.11
                                                                //u4
   // prm_common->reg_md2_3x3matrix[8] = rgb2yuv_matrix[8];            // s12, 3x3 matrix coefs.
                                                                //end

                                                                //REG_NAME: MD2_OFFSET
                                                               //u5
   //prm_common->reg_md2_offset_inp[2] = 0;                         // s11,s(BL+1)=s11 pre-offset before the 3x3 matrix.
   //                                                           //u5
   //prm_common->reg_md2_offset_oup[2] = 0;                         // s11,s(BL+1)=s11 post-offset after the 3x3 matrix.
   //                                                           //end

   //                                                             //REG_NAME: MD2_OFFSET_OUP
   //                                                         //u5
   //prm_common->reg_md2_offset_oup[1] = 0;                      // s11,s(BL+1)=s11 post-offset after the 3x3 matrix.
   //                                                         //u5
   //prm_common->reg_md2_offset_oup[0] = 0;                      // s11,s(BL+1)=s11 post-offset after the 3x3 matrix.
                                                           //end


                                                 //REG_NAME: MD2_POST_STEP_NORM
                                                 //u8
    prm_common->reg_md2_pstds_hstp = 4;              //u4, integer pixel ratio for post horizontal shrink, o2i_ratio= 1/step, 1~15
    prm_common->reg_md2_pstds_vstp = 3;              //u4, integer pixel ratio for post vertical shrink,   o2i_ratio= 1/step£¬1~15
                                                 //u4
    prm_common->reg_md2_pstds_norm = 512;            //u12, normalization gain to the accum to get average in block, norm = 4096/pixelnum(v_step*h_step)
                                                //end



                                                        //REG_NAME: MD2_POST_OUT_SIZE
                                                       //u7
    prm_common->reg_md2_pstds_ocol = 256;                  //u9, post-shrink output image horizontal size, maximum 256
                                                        //u7
    prm_common->reg_md2_pstds_orow = 128;                 //u9, post-shrink output image vertical size, maximum 256
                                                         //end

                                                         //REG_NAME: MD2_TH
                                               //u12
    prm_common->reg_md2_ratio_mod = 0;             //u2, ration mode selection. 0: R/(R+G+B+1), 1: R/G;  2： (R+G+B)/R; 3: G/R
    prm_common->reg_md2_ratio_nrm = 2;               //u2, normalizatin of ratio for: mode=0, range=(1, 0]/(1<<x), dft=2;  mode>0, precision = u(2+x).(6-x), dft=1
    prm_common->reg_md2_satur_thr=240;             //u8, saturation threshold of u8
    prm_common->reg_md2_under_thr=10;              //u8, under threshold of u8
                                               //end
                                                 //REG_NAME: ISP_MD2_STA_INFO0
    prm_common->ro_md2_ro0   = 0;                  //u32, debug info0
                                                 //end
                                                 //REG_NAME: ISP_MD2_STA_INFO1
    prm_common->ro_md2_ro1   = 0;                  //u32, debug info1
                                                 //end
                                                 //REG_NAME: ISP_MD2_STA_INFO2
    prm_common->ro_md2_ro2   = 0;                  //u32, debug info2
                                                 //end
                                                 //REG_NAME: ISP_MD2_STA_INFO3
    prm_common->ro_md2_ro3   = 0;                  //u32, debug info3
                                                 //end
                                                 //REG_NAME: ISP_MD2_STA_INFO4
    prm_common->ro_md2_ro4   = 0;                  //u32, debug info4
                                                 //end
                                                 //REG_NAME: ISP_MD2_STA_INFO5
    prm_common->ro_md2_ro5   = 0;                  //u32, debug info5
                                                 //end
                                                 //REG_NAME: ISP_MD2_STA_INFO6
    prm_common->ro_md2_ro6   = 0;                  //u32, debug info6
                                                 //end
                                                 //REG_NAME: ISP_MD2_STA_INFO7
    prm_common->ro_md2_ro7   = 0;                  //u32, debug info7
                                                 //end
//for (k = 0; k < MD2_PSTDS_XMAX*MD2_PSTDS_YMAX; k++)
//{                                               //REG_NAME: MD2_RO_PACK0
//    prm_common->ro_md2_sta_pack0[k]= 0;             //u32 each cell, MD2_STA packed LSB data write to SRAM or DDR for FW based Motion detection
//                                                //end
//                                                //REG_NAME: MD2_RO_PACK1
//    prm_common->ro_md2_sta_pack1[k]= 0;             //u32 each cell, MD2_STA packed MSB data write to SRAM or DDR for FW based Motion detection
//                                                //end
//}
   // return 0;

    prm_common->reg_md2_preds_ocol = (prm_common->reg_md2_win_xxyy_1 - prm_common->reg_md2_win_xxyy_0)/prm_common->reg_md2_preds_hstp;

    prm_common->reg_md2_preds_orow = (prm_common->reg_md2_win_xxyy_3 - prm_common->reg_md2_win_xxyy_2)/prm_common->reg_md2_preds_vstp;

    prm_common->reg_md2_pstds_ocol = prm_common->reg_md2_preds_ocol/prm_common->reg_md2_pstds_hstp;

    prm_common->reg_md2_pstds_orow = prm_common->reg_md2_preds_orow/prm_common->reg_md2_pstds_vstp;

    prm_common->reg_md2_preds_norm = 4096/(prm_common->reg_md2_preds_hstp*prm_common->reg_md2_preds_vstp);

    prm_common->reg_md2_pstds_norm = 4096/(prm_common->reg_md2_pstds_hstp*prm_common->reg_md2_pstds_vstp);


}

void set_md2_size(int xin, int yin, int xout, int yout) {
    am_md2_reg.reg_lXSizeIn  = xin;           //u14, horizontal size
                                                //u2
    am_md2_reg.reg_lYSizeIn  = yin;//u14, vertical size


    am_md2_reg.reg_md2_input_xsize= xin;             //u16, xsize of the input in pixels, set to image xsize
    am_md2_reg.reg_md2_input_ysize= yin;              //u16, ysize of the input in pixels, set to image ysize
                                                   //end

                                                    //REG_NAME: MD2_WIND_XSTED
                                                    //u2
    am_md2_reg.reg_md2_win_xxyy_0 = 0;               //u14: window for motion sum statistics on down-scaled image. [x_st x_ed; y_st y_ed]
                                                   //u2
    am_md2_reg.reg_md2_win_xxyy_1 = xout;            //u14
                                                   //end

                                                    //REG_NAME: MD2_WIND_YSTED
                                                    //u2
    am_md2_reg.reg_md2_win_xxyy_2 = 0;                //u14
                                                    //u2
    am_md2_reg.reg_md2_win_xxyy_3 = yout;              //u14
}

void isp_md2_cfg(T_MD2_PRM *prm_common)
{
//    int k, winx, winy, num_pix, half, is_rgb2yuv;
//    int win_xxyy[4]= {0, 255, 0, 255};

    // csc matrix
//    int rgb2yuv_matrix[9] = { 77, 150, 29, -43, -85, 128, 128, -107, -21 };
//    int yuv2rgb_matrix[9] = { 256, 0, 359, 256, -88, -183, 256, 454, 0 };
    md_write_reg( ISP_MD2_COMMON0,
    prm_common->reg_ctrl0);                     //u32, latch control
                                                //end
    md_write_reg( ISP_MD2_COMMON1,
    prm_common->reg_ctrl1);                     //u32, latch control
                                                //end
    md_write_reg( ISP_MD2_COMMON2,
    prm_common->reg_ctrl2);                     //u32, latch control
                                                //end
    md_write_reg( ISP_MD2_COMMON3,
   (prm_common->reg_num_cmp  <<28  )|           //u4, number cmp
   (prm_common->reg_bit_depth<<20  )|           //u8, bit_depth
   (prm_common->reg_gclk     <<12  )|           //u8, clk gate
   (prm_common->reg_insel    <<8   )|           //u4, input data channel select
   (prm_common->reg_bypass    <<7  )|           //u1, axi wr bypass
   //(                               )|         //u5
   (prm_common->reg_soft_rst    <<1   ) );      //u1, soft_rst
                                                //u1
                                                //end
    md_write_reg(ISP_MD2_COMMON_SIZE,
                                                //u2
    (prm_common->reg_lXSizeIn<<16)|             //u14, horizontal size
                                                //u2
    prm_common->reg_lYSizeIn  );                //u14, vertical size
                                                //end
    md_write_reg( ISP_MD2AXI_CTRL,
    (prm_common->reg_axi_bsize    <<24 )|       //u8, each framesize buffer_size
    (prm_common->reg_wr_burst_lens<<22 )|       //u2, busrt lens reg_wr_burst_lens   0: 1x128, 1: 2x128, 2‾3: 4x128
    (prm_common->reg_wr_req_th    <<18 )|       //u4,  fifo depth req_th * 8 *128 bits in fifo.
    (prm_common->reg_wr_awid      <<10 )|       //u8,  wid
    (prm_common->reg_wr_awport   <<7));         //u3, urgent control
    //prm_common->reg_axi_reserved  = 0;        //u7, reserved
                                                //end
   //write_reg( ISP_MD2AXI_CTRL0,
   //prm_common->reg_wr_urgent_ctrl = 0;        //u32, urgent control
                                                //end
    md_write_reg( ISP_MD2AXI_BUFFSIZE,
    (prm_common->reg_axi_mode     << 30  )|     //u2, 0:always axi_addr, 1: axi_addr-axi_addrp pingpong, 2~3:buffer_mode, start with axi_addr
    (prm_common->reg_axi_buff_rst << 29  )|     //u1, frame buffer reset
    prm_common->reg_axi_fbuf);                  //u29, axi_buffer size
                                                //end
    md_write_reg( ISP_MD2AXI_FRAMESIZE,
                                                //u3, reserved
    prm_common->reg_axi_fsize );                //u29, axi_fsize
                                                //end
    md_write_reg( ISP_MD2AXI_ADDR,
    prm_common->reg_axi_addr );                 //u32, axi_addr
                                                //end
    md_write_reg( ISP_MD2AXI_ADDRP,
    prm_common->reg_axi_addrp );                //u32, axi_addrp
                                                //end

    // motion detection2 sta, (on RGB/YUV only)
    md_write_reg( MD2_CTRL,                     //u25
    (0xa << 8)                              |
    (prm_common->reg_md2_sta_enable  <<  6 )|   //u1, enable of the md2 statistics. 0: disable 1: enable
    (prm_common->reg_md2_sta_yuvin   <<  5 )|   //u1, yuv or rgb input selection, 0: rgb; 1:yuv, default = 0
    (prm_common->reg_md2_sta_y_by_ir <<  4 )|   //u1, enable of replace y by ir components. 0: disable 1: enable
    prm_common->reg_md2_sta_in_rs);             //s4, input data righ shift before pre-shrink, x= BL-10,  range -2~6
                                                //end

    md_write_reg( MD2_INPUT_SIZE,
    (prm_common->reg_md2_input_xsize << 16)|    //u16, xsize of the input in pixels, set to image xsize
    prm_common->reg_md2_input_ysize );          //u16, ysize of the input in pixels, set to image ysize
                                                //end

    md_write_reg( MD2_WIND_XSTED,                         //u2
    (prm_common->reg_md2_win_xxyy_0 )|          //u14: window for motion sum statistics on down-scaled image. [x_st x_ed; y_st y_ed]
    prm_common->reg_md2_win_xxyy_1  );          //u14
                                                //end

    md_write_reg( MD2_WIND_YSTED,
                                                //u2
    (prm_common->reg_md2_win_xxyy_2  )|         //u14
    prm_common->reg_md2_win_xxyy_3  );          //u14
                                                //end

   md_write_reg( MD2_PRE_STEP_NORM,
                                                //u7
   (prm_common->reg_md2_preds_hstp << 20  )|    //u5, integer pixel ratio for pre horizontal shrink, o2i_ratio= 1/step, 1~31
   (prm_common->reg_md2_preds_vstp << 16  )|    //u4, integer pixel ratio for pre vertical shrink,   o2i_ratio= 1/step£¬1~15
                                                //u4
   prm_common->reg_md2_preds_norm);             //u12, normalization gain to the accum to get average of the block, norm = 4096/pixelnum(v_step*h_step)
 //                                             //end

   md_write_reg( MD2_PRE_OUT_SIZE,
                                                //u6
   (prm_common->reg_md2_preds_ocol <<16)  |     //u10, pre-shrink output image horizontal size, maximum 512, floor((win_xxyy[1]-win_xxyy[0])/hstp).
                                                //u6
   prm_common->reg_md2_preds_orow);             //u10, pre-shrink output image vertical size, maximum 512
 //                                             //end

 //  write_reg( MD2_OFFSET_INP,
 //                                             //u5
 //  prm_common->reg_md2_offset_inp[1] = 0;     // s11,s(BL+1)=s11 pre-offset before the 3x3 matrix.
 //                                             //u5
 //  prm_common->reg_md2_offset_inp[0] = 0;     // s11,s(BL+1)=s11 pre-offset before the 3x3 matrix.
 //                                             //end
// for(k=0;k<4;k++)
// {
// write_reg( MD2_MATRIX,
//                                              //u4
//    prm_common->reg_md2_3x3matrix[2*k+1] = rgb2yuv_matrix[2*k+1];   // s12, 3x3 matrix coefs.
//                                                                    //u4
//    prm_common->reg_md2_3x3matrix[2*k] = rgb2yuv_matrix[2*k];       // s12, 3x3 matrix coefs.
//                                                                    //end
//
//
// }

//  write_reg( MD2_MATRIX_4,
//                                                              //u14
//  prm_common->reg_md2_3x3mtrx_rs =0;                              // u2:  coefs precision mode. 0 s3.8; 1: s2.9; 2: s1.10 3:s0.11
//                                                              //u4
//  prm_common->reg_md2_3x3matrix[8] = rgb2yuv_matrix[8];            // s12, 3x3 matrix coefs.
//                                                              //end
//
   md_write_reg( MD2_OFFSET,0x80);
   md_write_reg( MD2_OFFSET_OUP, (0x80<<16));
//                                                             //u5
//  prm_common->reg_md2_offset_inp[2] = 0;                         // s11,s(BL+1)=s11 pre-offset before the 3x3 matrix.
//                                                             //u5
//  prm_common->reg_md2_offset_oup[2] = 0;                         // s11,s(BL+1)=s11 post-offset after the 3x3 matrix.
//                                                             //end
//
//  write_reg( MD2_OFFSET_OUP,
//                                                           //u5
//  prm_common->reg_md2_offset_oup[1] = 0;                      // s11,s(BL+1)=s11 post-offset after the 3x3 matrix.
//                                                           //u5
//  prm_common->reg_md2_offset_oup[0] = 0;                      // s11,s(BL+1)=s11 post-offset after the 3x3 matrix.
//                                                         //end
//
//
      md_write_reg( MD2_POST_STEP_NORM,
                                                   //u8
     (prm_common->reg_md2_pstds_hstp << 20)|              //u4, integer pixel ratio for post horizontal shrink, o2i_ratio= 1/step, 1~15
     (prm_common->reg_md2_pstds_vstp << 16)|              //u4, integer pixel ratio for post vertical shrink,   o2i_ratio= 1/step£¬1~15
                                                   //u4
      prm_common->reg_md2_pstds_norm);            //u12, normalization gain to the accum to get average in block, norm = 4096/pixelnum(v_step*h_step)
//                                              //end
//
//
//
      md_write_reg( MD2_POST_OUT_SIZE,
                                                     //u7
  (prm_common->reg_md2_pstds_ocol<<16)|// = 256;                  //u9, post-shrink output image horizontal size, maximum 256
                                                      //u7
  prm_common->reg_md2_pstds_orow);// = 128;                 //u9, post-shrink output image vertical size, maximum 256
//                                                       //end
//
//  write_reg( MD2_TH,
//                                             //u12
//  prm_common->reg_md2_ratio_mod = 0;             //u2, ration mode selection. 0: R/(R+G+B+1), 1: R/G;  2： (R+G+B)/R; 3: G/R
//  prm_common->reg_md2_ratio_nrm = 2;               //u2, normalizatin of ratio for: mode=0, range=(1, 0]/(1<<x), dft=2;  mode>0, precision = u(2+x).(6-x), dft=1
//  prm_common->reg_md2_satur_thr=240;             //u8, saturation threshold of u8
//  prm_common->reg_md2_under_thr=10;              //u8, under threshold of u8
//                                             //end
//  write_reg( ISP_MD2_STA_INFO0,
//  prm_common->ro_md2_ro0   = 0;                  //u32, debug info0
//                                               //end
//  write_reg( ISP_MD2_STA_INFO1,
//  prm_common->ro_md2_ro1   = 0;                  //u32, debug info1
//                                               //end
//  write_reg( ISP_MD2_STA_INFO2,
//  prm_common->ro_md2_ro2   = 0;                  //u32, debug info2
//                                               //end
//  write_reg( ISP_MD2_STA_INFO3,
//  prm_common->ro_md2_ro3   = 0;                  //u32, debug info3
//                                               //end
//  write_reg( ISP_MD2_STA_INFO4,
//  prm_common->ro_md2_ro4   = 0;                  //u32, debug info4
//                                               //end
//  write_reg( ISP_MD2_STA_INFO5,
//  prm_common->ro_md2_ro5   = 0;                  //u32, debug info5
//                                               //end
//  write_reg( ISP_MD2_STA_INFO6,
//  prm_common->ro_md2_ro6   = 0;                  //u32, debug info6
//                                               //end
//  write_reg( ISP_MD2_STA_INFO7,
//  prm_common->ro_md2_ro7   = 0;                  //u32, debug info7
                                                 //end
//for (k = 0; k < MD2_PSTDS_XMAX*MD2_PSTDS_YMAX; k++)
//{    write_reg( MD2_RO_PACK0,
//    prm_common->ro_md2_sta_pack0[k]= 0;             //u32 each cell, MD2_STA packed LSB data write to SRAM or DDR for FW based Motion detection
//                                                //end
//    write_reg( MD2_RO_PACK1,
//    prm_common->ro_md2_sta_pack1[k]= 0;             //u32 each cell, MD2_STA packed MSB data write to SRAM or DDR for FW based Motion detection
//                                                //end
//}
  //  return 0;

}


void am_md_init(int ctx_id, int width, int height)
{
    //int ret = 0;

    isp_md2_init(&am_md2_reg, width, height);
    isp_md2_cfg(&am_md2_reg);
    pr_err("init md2 success.-----------------\n");
#if 0
    ret = request_irq(g_md->irq, motion_detection2_isr, IRQF_SHARED | IRQF_TRIGGER_RISING,
        "isp-md-irq", (void *)g_md);
#endif
    return;
}

void am_md_deinit(void)
{
    if (g_md == NULL) {
        pr_err("Error g_md is NULL\n");
        return;
    }

    if (g_md->kmd_thread) {
        kthread_stop( g_md->kmd_thread);
        g_md->kmd_thread = NULL;
    }

    if(g_md->irq)
        free_irq(g_md->irq, (void *)g_md);

}

void am_md_destroy(void)
{
    if (g_md == NULL) {
        pr_err("Error g_md is NULL\n");
        return;
    }

    if (g_md->base_addr != NULL)
    {
        iounmap(g_md->base_addr);
        g_md->base_addr = NULL;
    }

    if (g_md)
    {
        kfree(g_md);
        g_md = NULL;
    }
}

void aml_md_get_data(uint32_t* dst){
    if (!paddr_md) {
        pr_info("%d, paddr_md is NULL.\n", __LINE__);
        return;
    }

    uint32_t data = 0;
    md_read_reg(ISP_MD2_STA_INFO4, &data);
    data = data & 0x1;

    char* buf;
    //pr_err("md get data ok.pp: %d\n", data);
    if (data == 0)
        buf = phys_to_virt(paddr_md + 0x40000);
    else
        buf = phys_to_virt(paddr_md);
    memcpy(dst, buf, am_md2_reg.reg_axi_fbuf);
}

