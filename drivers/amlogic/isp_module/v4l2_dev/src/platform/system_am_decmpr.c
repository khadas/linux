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
#include "system_am_decmpr.h"
#include "acamera_sensor_api.h"

static struct am_cmpr *g_cmpr;
#define AM_CMPR_NAME "amlogic, isp-cmpr"
#define CMPR_BASE_ADDR 0xfe3b4000
#define CMPR_CNTL_ADDR 0xfe3b3000

extern temper_addr isp_temper_paddr[FIRMWARE_CONTEXT_NUMBER];

static inline void cntl_wr(
    int addr, uint32_t val)
{
    void __iomem *base = g_cmpr->cntl_addr;

    if (base != NULL) {
        base = base + (addr - CMPR_CNTL_ADDR);
        writel(val, base);
    } else
        pr_err("isp-cmpr write register failed.\n");

}

static inline void cntl_rd(
    int addr, uint32_t *val)
{
    void __iomem *base = g_cmpr->cntl_addr;

    if (base != NULL && val) {
        base = base + (addr  - CMPR_CNTL_ADDR);
        *val = readl(base);
    } else
        pr_err("isp-cmpr read register failed.\n");

}

static inline void update_wr_bits(
    unsigned int reg,
    unsigned int mask,
    unsigned int val)
{
    unsigned int tmp, orig;
    void __iomem *base = g_cmpr->base_addr;

    if (base !=  NULL) {
        orig = readl(base + (reg - CMPR_BASE_ADDR));
        tmp = orig & ~mask;
        tmp |= val & mask;
        writel(tmp, base + (reg - CMPR_BASE_ADDR));
    }
}

static inline void Wr_reg_bits(
    unsigned int adr, unsigned int val,
    unsigned int start, unsigned int len)
{
    update_wr_bits(adr,
        ((1 << len) - 1) << start, val << start);
}

inline void cmpr_wr(
    int addr, uint32_t val)
{
    void __iomem *base = g_cmpr->base_addr;

    if (base != NULL) {
        base = base + (addr - CMPR_BASE_ADDR);
        writel(val, base);
    } else
        pr_err("isp-cmpr write register failed.\n");
}

inline void cmpr_rd(
    int addr, uint32_t *val)
{
    void __iomem *base = g_cmpr->base_addr;

    if (base != NULL && val) {
        base = base + (addr  - CMPR_BASE_ADDR);
        *val = readl(base);
    } else
        pr_err("isp-cmpr read register failed.\n");
}

int am_cmpr_parse_dt(struct device_node *node)
{
    int rtn = -1;
#if 0
    int irq = -1;
#endif
    struct resource rs;
    struct am_cmpr *t_cmpr = NULL;

    if (node == NULL) {
        pr_err("%s: Error input param\n", __func__);
        return -1;
    }

    rtn = of_device_is_compatible(node, AM_CMPR_NAME);
    if (rtn == 0) {
        pr_err("%s: Error match compatible\n", __func__);
        return -1;
    }

    t_cmpr = kzalloc(sizeof(*t_cmpr), GFP_KERNEL);
    if (t_cmpr == NULL) {
        pr_err("%s: Failed to alloc isp-cmpr\n", __func__);
        return -1;
    }

    t_cmpr->of_node = node;

    rtn = of_address_to_resource(node, 0, &rs);
    if (rtn != 0) {
        pr_err("%s:Error get isp-cmpr reg resource\n", __func__);
        goto reg_error;
    }

    pr_info("%s: rs info: name: %s\n", __func__, rs.name);
    t_cmpr->reg = rs;
    t_cmpr->base_addr = ioremap_nocache(t_cmpr->reg.start, resource_size(&t_cmpr->reg));

    rtn = of_address_to_resource(node, 1, &rs);
    if (rtn != 0) {
        pr_err("%s:Error get cntl-cmpr reg resource\n", __func__);
        goto reg_error;
    }

    pr_info("%s: rs info: name: %s\n", __func__, rs.name);
    t_cmpr->reg = rs;
    t_cmpr->cntl_addr = ioremap_nocache(t_cmpr->reg.start, resource_size(&t_cmpr->reg));

#if 0
    irq = irq_of_parse_and_map(node, 0);
    if (irq <= 0) {
        pr_err("%s:Error get flicker irq\n", __func__);
        goto irq_error;
    }
    t_flicker->irq = irq;
    pr_info("%s:rs info: irq: %d\n", __func__, t_flicker->irq);
#endif

    t_cmpr->p_dev = of_find_device_by_node(node);

    g_cmpr = t_cmpr;

    return 0;

#if 0
irq_error:
    iounmap(t_cmpr->base_addr);
    t_cmpr->base_addr = NULL;
#endif

reg_error:
    if (t_cmpr != NULL)
        kfree(t_cmpr);

    return -1;
}

void mipi_tnr_de_soft_reset(void){
    uint32_t data = 0;
    cntl_rd(MIPI_TOP_TNR_DE_CTRL0, &data);
    cntl_wr(MIPI_TOP_TNR_DE_CTRL0,  ( data ) | (3 << 30));
    cntl_wr(MIPI_TOP_TNR_DE_CTRL0,  ( data ) & (~(3 << 30)) );

    cntl_rd(MIPI_TOP_CSI2_CTRL0, &data);
    cntl_wr(MIPI_TOP_CSI2_CTRL0,  ( data ) | (1 << 11));

    cntl_rd(MIPI_TOP_CSI2_CTRL0, &data);
    cntl_wr(MIPI_TOP_CSI2_CTRL0,  ( data ) & (~(1 << 11)));
}

void mipi_tnr_de_enable(void){
    uint32_t data = 0;
    cntl_rd(MIPI_TOP_TNR_DE_CTRL0, &data);
    cntl_wr(MIPI_TOP_TNR_DE_CTRL0,  data & (~(1<<0)));
}

void mipi_tnr_de_bypass(void){
    uint32_t data = 0;
    cntl_rd(MIPI_TOP_TNR_DE_CTRL0, &data);
    cntl_wr(MIPI_TOP_TNR_DE_CTRL0,  data | 1<<0);
}

void enable_mipi_cmpr_tnr(int enable, int temp3) {
  int temp3_enable;
  //--- temp2
  //encoder core
  Wr_reg_bits(TWSUB0_ISP_LOSS_CTRL,enable,0,1);
  //encoder wrmif
  Wr_reg_bits(TWSUB0_ISP_LOSS_MIF_CTRL,enable,0,1);
  //meta  wrmif
  Wr_reg_bits(MIPI_TNR_META_WSUB0_MIF_CTRL,enable,0,1);
  //decoder core
  Wr_reg_bits(TRSUB0_ISP_LOSS_CTRL,enable,0,1);
  //decoder rdmif
  Wr_reg_bits(TRSUB0_ISP_LOSS_MIF_CTRL,enable,0,1);
  //meta  rdmif
  Wr_reg_bits(MIPI_TNR_META_RSUB0_MIF_CTRL,enable,0,1);
  //--- temp3
  temp3_enable = (temp3==1) ? enable : 0;
  //encoder core
  Wr_reg_bits(TWSUB1_ISP_LOSS_CTRL,temp3_enable,0,1);
  //encoder wrmif
  Wr_reg_bits(TWSUB1_ISP_LOSS_MIF_CTRL,temp3_enable,0,1);
  //meta  wrmif
  Wr_reg_bits(MIPI_TNR_META_WSUB1_MIF_CTRL,temp3_enable,0,1);
  //decoder core
  Wr_reg_bits(TRSUB1_ISP_LOSS_CTRL,temp3_enable,0,1);
  //decoder rdmif
  Wr_reg_bits(TRSUB1_ISP_LOSS_MIF_CTRL,temp3_enable,0,1);
  //meta  rdmif
  Wr_reg_bits(MIPI_TNR_META_RSUB1_MIF_CTRL,temp3_enable,0,1);
  //split
  Wr_reg_bits(MIPI_TNR_CMPR_SPLIT_CTRL,enable,0,1);
  //pack
  Wr_reg_bits(MIPI_TNR_CMPR_PACK_CTRL,enable,0,1);
}


//=============================================================================
void init_mipi_cmprcore(int addrbase,sCMPRCORE_Param *prm)
{
  int addrbase_c;
  int dwTemp,i,k,n;

  //compress ratio
  prm->reg_ratio_bppx16 = 80; //bits per pixel x16, fracbits_bpp = x/16;
  cmpr_param_fw_calc(prm);

  for(i=0;i<2;i++) { //i=0 for enc; i=1 for dec
    addrbase_c = addrbase + (0x80*4*i);
    //compress/decompress
    Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_ICH),0,25,1);
    //cmpr_wr(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_DEBUG),0x808080);
    //Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_WDR_LINE_DELAY),1,31,1);
    Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_CTRL),0xf,4,4);   //reg_use_inter_fmt=0xf, use internal setting
    cmpr_wr(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_FRAME_HOLD),0x0);         //reg_frame_hold_nums=0: no RDMA
    Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_MISC),0x0,4,2);  //reg_not_wait_input4busy=0x0, reg_busy_status_use4abort=0;
    if(i==1) { //dec
       Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_MISC),3,0,2);  //for dec: reg_inter_preslc_bitaccum=1/reg_inter_preslc_fifolevel=1
       Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_MISC),1,4,1);  //for dec: reg_not_wait_input4busy=1
    }
    dwTemp = ((prm->reg_lossless_en&0x1)   <<31) |  //1bits
             ((prm->reg_src_bit_depth&0x1f)<<24) |  //5bits
             (1                            <<21) |  //3bits, raw_mode=1
             (0                            <<20) |  //1bits, reg_mono_comp_mode=0
             ((prm->reg_xphase_ofst&0x3)   <<18) |  //2bits
             ((prm->reg_yphase_ofst&0x3)   <<16) |  //2bits
             ((prm->reg_ratio_bppx16&0x1ff)<< 4) |  //9bits
             (2                            << 0) ;  //3bits, reg_comp_chn_size=2
    cmpr_wr(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_BASIS),dwTemp);
    dwTemp = prm->reg_pic_xsize | (prm->reg_pic_ysize<<16);
    cmpr_wr(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_PIC_SIZE),dwTemp);
    dwTemp = (0 << 31) |  //reg_use_sw_preslc_bitaccum=0
             (0 << 28) |  //reg_slice_num_mode=0: one slice
             (prm->reg_pic_ysize);
    cmpr_wr(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_SLICE_SIZE),dwTemp);
    Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_PRESL_FIFO_LEVEL),0,31,1); //reg_use_sw_preslc_fifolevel=0

    dwTemp = (prm->reg_wdr_mode_en<<31) | (prm->reg_wdr_delay_line);
    cmpr_wr(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_WDR_LINE_DELAY),dwTemp); //WDR mode

    Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_ERROR_COUNT_THD),prm->reg_error_count_thd,0,16);
    Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_FLATNESS_0),prm->reg_flatness_qp_thresh,20,5);
    Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_FLATNESS_1),prm->reg_flatness_q_err_thresh,0,12);

    dwTemp = (prm->reg_rc_group_y<<16) | (prm->reg_slcln_ratio);
    cmpr_wr(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_RC_GROUP_2),dwTemp);
    dwTemp = prm->reg_default_value[1] | (prm->reg_default_value[0]<<16);
    cmpr_wr(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_DEFAULT_VALUE),dwTemp);
    Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_DEFAULT_VALUE_1),prm->reg_default_value[2],0,16);

    dwTemp = (prm->reg_flatness_det_thresh<<16) | (prm->reg_flatness_det_thresh_max);
    cmpr_wr(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_FLATNESS_TH),dwTemp);
    Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_FLATNESS_0),prm->reg_flatness_accum_thresh,0,12);

    dwTemp = (prm->reg_pred_err0<<16) | (prm->reg_pred_flat_thd);
    cmpr_wr(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_PRED_TH),dwTemp);

    dwTemp = (prm->reg_pred_drt5flat_thd<<16) | (prm->reg_pred_drt5_thrd);
    cmpr_wr(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_PRED_TH_1),dwTemp);

    dwTemp = (prm->reg_pixel_luma_adj_th[1]<<16) | (prm->reg_pixel_luma_adj_th[0]);
    cmpr_wr(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_LUMA_TH),dwTemp);

    prm->reg_rc_fifo_margin_dlt[0] = 3;
    prm->reg_rc_fifo_margin_dlt[1] = 5;
    prm->reg_rc_fifo_margin_dlt[2] = 7;
    prm->reg_rc_fifo_margin_dlt[3] = 10;
    dwTemp = (prm->reg_rc_fifo_margin_dlt[3]<<24) | (prm->reg_rc_fifo_margin_dlt[2]<<16) |
             (prm->reg_rc_fifo_margin_dlt[1]<< 8) | (prm->reg_rc_fifo_margin_dlt[0]<< 0) ;
    cmpr_wr(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_FIFO_DLT),dwTemp);

    prm->reg_rc_fifo_margin_dlt[4] = 12;
    prm->reg_rc_fifo_margin_dlt[5] = 8;
    dwTemp =(prm->reg_rc_fifo_margin_dlt[5]<<8) | (prm->reg_rc_fifo_margin_dlt[4]);
    Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_FIFO_AVG),dwTemp,12,14);

    prm->reg_rc_fifo_qpx2_margin_dlt[0] = 3;
    prm->reg_rc_fifo_qpx2_margin_dlt[1] = 5;
    prm->reg_rc_fifo_qpx2_margin_dlt[2] = 10;
    prm->reg_rc_fifo_qpx2_margin_dlt[3] = 15;
    dwTemp = (prm->reg_rc_fifo_qpx2_margin_dlt[3]<<24) | (prm->reg_rc_fifo_qpx2_margin_dlt[2]<<16) |
             (prm->reg_rc_fifo_qpx2_margin_dlt[1]<< 8) | (prm->reg_rc_fifo_qpx2_margin_dlt[0]<< 0) ;
    cmpr_wr(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_RC_QP_MARGIN_5),dwTemp);

    prm->reg_rc_master_qpx2_min = 20;
    dwTemp = (prm->reg_rc_master_qpx2_max<<8) | (prm->reg_rc_master_qpx2_min);
    Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_RC_BUDGET_5),dwTemp,16,16);

    dwTemp = 128;
    Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_RC_BUDGET_4),dwTemp,16,8);

    dwTemp = 1200; // reg_rc_fifo_qpx2_margin_thd_3
    Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_RC_QP_MARGIN_4),dwTemp,16,12);
    dwTemp = 960; //reg_rc_fifo_qpx2_margin_thd_2
    Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_RC_QP_MARGIN_4),dwTemp,0,12);

    dwTemp = 720; //reg_rc_fifo_qpx2_margin_thd_1
    Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_RC_QP_MARGIN_3),dwTemp,16,12);
    dwTemp = 480; // reg_rc_fifo_qpx2_margin_thd_0
    Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_RC_QP_MARGIN_3),dwTemp,0,12);

    dwTemp = 1200; //reg_rc_fifo_margin_thd_5
    Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_FIFO_THD_0),dwTemp,12,12);
    dwTemp = 1080; // reg_rc_fifo_margin_thd_4
    Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_FIFO_THD_0),dwTemp,0,12);

    dwTemp = 960; //reg_rc_fifo_margin_thd_3
    Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_FIFO_THD_1),dwTemp,12,12);
    dwTemp = 840; // reg_rc_fifo_margin_thd_2
    Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_FIFO_THD_1),dwTemp,0,12);

    dwTemp = 720; //reg_rc_fifo_margin_thd_1
    Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_FIFO_THD_2),dwTemp,12,12);
    dwTemp = 480; // reg_rc_fifo_margin_thd_0
    Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_FIFO_THD_2),dwTemp,0,12);

    Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_ACCUM_OFSET_0),prm->reg_accum_ofset_en,0,1);
    Wr_reg_bits(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_ACCUM_OFSET_3),prm->reg_normalize_idx_ratio,0,12);

    for(k=0;k<3;k++) {
      for(n=0;n<40;n+=4) {
         dwTemp = (prm->reg_lut_budget2qp[k][n+3]<<24) | (prm->reg_lut_budget2qp[k][n+2]<<16) |
                  (prm->reg_lut_budget2qp[k][n+1]<< 8) | (prm->reg_lut_budget2qp[k][n+0]<< 0) ;
         cmpr_wr(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_QP_MAP_CHN0+k*10*4+n),dwTemp);
      }
    }
  }
}

//----------------------------------------------------------------------
//cmpr2mif_path= 0: adpat enc  wmif
//cmpr2mif_path= 1: adpat dec  rmif
//cmpr2mif_path= 2: tnr   enc  wmif
//cmpr2mif_path= 3: tnr   dec  rmif
//cmpr2mif_path= 4: tnr   meta wmif
//cmpr2mif_path= 5: tnr   meta rmif
void init_mipi_cmpr2mif(int cmpr2mif_path, int addrbase,sCMPR2MIF_Param *prm) {
  int dwTemp;
  int dfifo_size,meta_size_b128;
  int check_resp_id;


  if(cmpr2mif_path==0 || cmpr2mif_path==1 || cmpr2mif_path==2 || cmpr2mif_path==3)
     dfifo_size = 128;
  else
     dfifo_size = 64;
  if(cmpr2mif_path==0 || cmpr2mif_path==1)
     check_resp_id = 0;  //dont need to check respond_id
  else
     check_resp_id = 1;

  dwTemp = (0 << 24)        |                  //reg_sync_sel=0;
           ((prm->reg_canvas_id&0xff)<<16) |
           (1<<12)          |                  //reg_cmd_intr_len=1
           (1<<8)           |                  //reg_cmd_req_size=1 (32)
           ((prm->reg_burst_len&0x7)<<4)   |   //
           ((check_resp_id&0x1)<<2)        |   //reg_check_resp_id=1   //todo
           ((prm->reg_dol_mode&0x1)<<1);
   cmpr_wr(addrbase+CMPR2MIF_OFS(MIPI_TNR_META_WSUB0_MIF_CTRL),dwTemp);
   dwTemp = (1<<18)|(1<<17)|(0<<16)|(((dfifo_size*3/4)&0xff)<<8)|((((dfifo_size*1/4)&0xff))<<0); //
   cmpr_wr(addrbase+CMPR2MIF_OFS(ISP_TNR_META_WSUB0_MIF_QOS),dwTemp);
   dwTemp = (1<<18)|(1<<17)|(0<<16)|(((dfifo_size*3/4)&0xff)<<8)|((((dfifo_size*1/4)&0xff))<<0); //
   cmpr_wr(addrbase+CMPR2MIF_OFS(MIPI_TNR_META_WSUB0_MIF_URGENT),dwTemp);

   dwTemp = (1<<3) |  //reg_mif_busy_noresp_check=1
            (0<<2) |  //reg_mif_wait_input4busy=0
            ((prm->reg_mif_dbl_baddr_init&0x1)<<1) | //
            ((prm->reg_mif_dbl_baddr_en&0x1));
   cmpr_wr(addrbase+CMPR2MIF_OFS(MIPI_TNR_META_WSUB0_MIF_MISC),dwTemp);
   cmpr_wr(addrbase+CMPR2MIF_OFS(MIPI_TNR_META_WSUB0_MIF_BADDR),prm->reg_mif_baddr0);
   cmpr_wr(addrbase+CMPR2MIF_OFS(MIPI_TNR_META_WSUB0_MIF_BADDR1),prm->reg_mif_baddr1);
   Wr_reg_bits(addrbase+CMPR2MIF_OFS(MIPI_TNR_META_WSUB0_MIF_FIFO),dfifo_size,0,8);
   if(cmpr2mif_path==4 || cmpr2mif_path==5) {  //meta
      meta_size_b128 = ((prm->reg_pic_hsize*4+127)/128)*prm->reg_pic_vsize;
      Wr_reg_bits(addrbase+CMPR2MIF_OFS(MIPI_TNR_META_WSUB0_MIF_SIZE),meta_size_b128,0,24);

   }
}

//---------------------------------------------------
//mipi2cmpr_path = 0:  adapt split
//mipi2cmpr_path = 1:  adapt pack
//mipi2cmpr_path = 2:  tnr   split
//mipi2cmpr_path = 3:  tnr   pack
void init_mipi_mipi2cmpr(int mipi2cmpr_path, int addrbase, sMIPI2CMPR_Param *prm) {
  int dwTemp;
  int frame_size_b128;
  int dfifo_size;

  //
  if(mipi2cmpr_path==0 || mipi2cmpr_path==1) {
    if(prm->reg_raw_mode==0 || prm->reg_raw_mode==1 || prm->reg_raw_mode==2) //raw6/7/8
       frame_size_b128 = (prm->line_stride/16)*(prm->reg_pic_vsize-1) + ((prm->reg_pic_hsize+15)/16); //((prm->reg_pic_hsize+15)/16)*prm->reg_pic_vsize;
    else if(prm->reg_raw_mode==3)   //raw10
       frame_size_b128 = (prm->line_stride/16)*(prm->reg_pic_vsize-1) + ((((prm->reg_pic_hsize+3)/4)*5+15)/16); //((((prm->reg_pic_hsize+3)/4)*5+15)/16)*prm->reg_pic_vsize;
    else if(prm->reg_raw_mode==4)   //raw12
       frame_size_b128 = (prm->line_stride/16)*(prm->reg_pic_vsize-1) + ((((prm->reg_pic_hsize+1)/2)*3+15)/16);//((((prm->reg_pic_hsize+1)/2)*3+15)/16)*prm->reg_pic_vsize;
    else  //raw14
       frame_size_b128 = (prm->line_stride/16)*(prm->reg_pic_vsize-1) + ((prm->reg_pic_hsize*2+15)/16);//((prm->reg_pic_hsize*2+15)/16)*prm->reg_pic_vsize;
    dfifo_size = 32;
  } else {
    if(prm->reg_raw_mode==0) //12bit
      frame_size_b128 =(prm->line_stride/16)*(prm->reg_pic_vsize-1)+ ((prm->reg_pic_hsize*16+127)/128); //((prm->reg_pic_hsize*16+127)/128)*prm->reg_pic_vsize; 
    else  //16bit
      frame_size_b128 =(prm->line_stride/16)*(prm->reg_pic_vsize-1)+ ((prm->reg_pic_hsize*20+127)/128); //((prm->reg_pic_hsize*20+127)/128)*prm->reg_pic_vsize; 
    dfifo_size = 128;
  }
  dwTemp = (0<<16) |                                    //reg_xxx_sync_ctrl=0
           ((prm->reg_pack_skip_flag_init&0x1) <<6)  |  //for pack
           ((prm->reg_pack_skip_auto_ctrl&0x1) <<5)  |  //for pack
           ((prm->reg_pack_keep_axicmd_order&0x1)<<4)|  //for pack
           (1<<3 ) |                                    //reg_xxx_ignore_frag_cmd=1
           ((prm->reg_tnr_temp3_en&0x1)<<2)          |  //reg_xxx_tnr_temp3_en, only for tnr
           (0<<1)  ;                                    //reg_split_big_endian=0
  cmpr_wr(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_SPLIT_CTRL),dwTemp);
  dwTemp = prm->reg_pic_hsize;
  cmpr_wr(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_SPLIT_SIZE),dwTemp);

  if(prm->reg_frmrst_mode==0) {
      dwTemp = (0<<24)    |                             //reg_pack_busy_care_rdmif=0, for pack only
               (1<<20)    |                             //reg_xxx_abort_after_cmd_1st=1  fixed
               (1<<19)    |                             //reg_xxx_internal_abort_en=1    fixed
               (0<<18)    |                             //reg_xxx_abort_hold_input=0
               (1<<17)    |                             //reg_xxx_frmrst_hold_input=1
               ((prm->reg_rdma_trigger_mode&0x1)<<16) | //reg_xxx_rdma_trigger_mode
               (0<<13)    |                             //reg_xxx_frmrst_hold4abort_mode=0
               (0<<12)    |                             //reg_xxx_frmrst_hold4gen_mode = 0
               (0<<11)    |                             //reg_xxx_dis_abort=0            fixed
               (1<<10)    |                             //reg_xxx_dis_frmrst_by_abort=1
               (0<< 9)    |                             //reg_xxx_wait_idle_for_abort=0
               (1<< 8)    |                             //reg_xxx_wait_idle_for_frmrst=1
               ((prm->reg_frmrst_mode&0x3)<<4);         //reg_xxx_frmrst_mode
  } else if(prm->reg_frmrst_mode==2) {
      dwTemp = (0<<24)    |                             //reg_pack_busy_care_rdmif=0, for pack only
               (1<<20)    |                             //reg_xxx_abort_after_cmd_1st=1  fixed
               (1<<19)    |                             //reg_xxx_internal_abort_en=1    fixed
               (1<<18)    |                             //reg_xxx_abort_hold_input=1
               (1<<17)    |                             //reg_xxx_frmrst_hold_input=1
               ((prm->reg_rdma_trigger_mode&0x1)<<16) | //reg_xxx_rdma_trigger_mode
               (1<<13)    |                             //reg_xxx_frmrst_hold4abort_mode=1   //big delay
               (1<<12)    |                             //reg_xxx_frmrst_hold4gen_mode = 1   //big delay
               (0<<11)    |                             //reg_xxx_dis_abort=0            fixed
               (0<<10)    |                             //reg_xxx_dis_frmrst_by_abort=0
               (1<< 9)    |                             //reg_xxx_wait_idle_for_abort=1
               (1<< 8)    |                             //reg_xxx_wait_idle_for_frmrst=1
               ((prm->reg_frmrst_mode&0x3)<<4);         //reg_xxx_frmrst_mode
  } else if(prm->reg_frmrst_mode==3) {
      dwTemp = (0<<24)    |                             //reg_pack_busy_care_rdmif=0, for pack only
               (1<<20)    |                             //reg_xxx_abort_after_cmd_1st=1  fixed
               (1<<19)    |                             //reg_xxx_internal_abort_en=1    fixed
               (1<<18)    |                             //reg_xxx_abort_hold_input=1
               (1<<17)    |                             //reg_xxx_frmrst_hold_input=1
               ((prm->reg_rdma_trigger_mode&0x1)<<16) | //reg_xxx_rdma_trigger_mode
               (0<<13)    |                             //reg_xxx_frmrst_hold4abort_mode=0
               (0<<12)    |                             //reg_xxx_frmrst_hold4gen_mode = 0
               (0<<11)    |                             //reg_xxx_dis_abort=0            fixed
               (1<<10)    |                             //reg_xxx_dis_frmrst_by_abort=1
               (1<< 9)    |                             //reg_xxx_wait_idle_for_abort=1
               (1<< 8)    |                             //reg_xxx_wait_idle_for_frmrst=1
               ((prm->reg_frmrst_mode&0x3)<<4);         //reg_xxx_frmrst_mode
   }
  cmpr_wr(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_SPLIT_FRMRST_CTRL),dwTemp);
  cmpr_wr(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_SPLIT_FRMRST_DLY0),prm->reg_frmrst_dlys4gen);
  cmpr_wr(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_SPLIT_FRMRST_DLY1),prm->reg_frmrst_dlys4abort);

  dwTemp = ((prm->reg_raw67_ext&0x1) <<20) |
           ((prm->reg_raw_mode&0x7)  <<16) |
           ((dfifo_size&0x3ff)       <<0)  ;
  cmpr_wr(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_WSUB0_CTRL),dwTemp);
  cmpr_wr(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_WSUB0_IADDRS),prm->reg_iaddr_bgn[0]);
  cmpr_wr(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_WSUB0_IADDRE),prm->reg_iaddr_bgn[0]+(frame_size_b128-1)*16);
  if(mipi2cmpr_path==2 || mipi2cmpr_path==3) {
    dwTemp = ((prm->reg_raw67_ext&0x1) <<20) |
             ((prm->reg_raw_mode&0x7)  <<16) |
             ((dfifo_size&0x3ff)       <<0)  ;
    cmpr_wr(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_WSUB1_CTRL),dwTemp);
    cmpr_wr(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_WSUB1_IADDRS),prm->reg_iaddr_bgn[1]);
    cmpr_wr(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_WSUB1_IADDRE),prm->reg_iaddr_bgn[1]+(frame_size_b128-1)*16);
  }
}

//-----
void set_mipi_mipi2cmpr_sw_frmbgn(int addrbase) {
  int dwTemp0,dwTemp1;
  int frmrst_mask = 0xffffffcf;

  cmpr_rd(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_SPLIT_FRMRST_CTRL), &dwTemp0);
  dwTemp1 = (dwTemp0 & frmrst_mask) | (1<<4) | 1;
  cmpr_wr(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_SPLIT_FRMRST_CTRL),dwTemp1);
  cmpr_wr(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_SPLIT_FRMRST_CTRL),dwTemp0);
}


//===========================================================================================
void cmpr_param_fw_calc(sCMPRCORE_Param *prm)
{
    #define SIZE_GROUP        4
    #define PIC_HGROUP_RATIO  14
    #define BIT_DEPTH_NORM    16

    int blk_num,i,k;
    int y_size = prm->reg_pic_ysize;
    int x_size = prm->reg_pic_xsize;
    int bw = prm->reg_src_bit_depth;
    int rs = bw > 8 ? bw - 8 : 0;
    int fifo_margin_dlt[6] = {2,5,8,11,14,bw};
    int fifo_qpx2_margin_dlt[4] = {2,3+rs,5+rs,8+rs};

    //default value
    blk_num  = (x_size + SIZE_GROUP*2 -1)/(SIZE_GROUP*2);
    prm->reg_error_count_thd = (50<<rs);
    prm->reg_flatness_qp_thresh = 4; //4 + rs;
    prm->reg_flatness_q_err_thresh = 4<<rs;
    prm->reg_rc_group_y  = y_size;
    //prm->reg_pic_ysize = y_size;
    //prm->reg_pic_xsize = x_size;
    for(i=0;i<3;i++)
        prm->reg_default_value[i] = 1<<(bw-1);

    prm->reg_flatness_det_thresh = 8<<rs;
    prm->reg_flatness_det_thresh_max = 10<<rs;
    prm->reg_flatness_accum_thresh = 1<<10; //1<<(bw-2);
    prm->reg_pred_err0 = 1<<(bw-1);
    prm->reg_pred_flat_thd = 4<<rs;
    prm->reg_pred_drt5flat_thd = 1<<(bw-2);  //u16, dynamic ratio to min_err  for no valid edge det base on err_flat
    prm->reg_pred_drt5_thrd = 20<<rs;        //u16: static threshold to detect no valid edge base on min_err,    default= 20

    for(k=0;k<2;k++)
       prm->reg_pixel_luma_adj_th[k] = (1<<(bw-3-k));  //u16
    for(k=0;k<6;k++)
        prm->reg_rc_fifo_margin_dlt[k] = fifo_margin_dlt[k]; //u5, delta of fifo level to guard the rc loop by adding delta to p_budget2x, defautl=[]
    for(k=0;k<4;k++)
        prm->reg_rc_fifo_qpx2_margin_dlt[k] = fifo_qpx2_margin_dlt[k];


    prm->reg_rc_master_qpx2_max = 2*prm->reg_src_bit_depth;               //u6: maxmum qpx2 during the rc_loop
    prm->reg_rc_master_qpx2_min = 2*(prm->reg_ratio_bppx16/16 +2)+3;      //u6: minmum qpx2 during the rc_loop

    cmpr_param_fw_lut_init(prm);

    //must
    prm->reg_slcln_ratio = (1<<(PIC_HGROUP_RATIO))/blk_num;  //u14, dynamic ratio to idx of extra bit budget for the cells within the line
    if ((y_size+1)/2<16)
    {
        prm->reg_accum_ofset_en = 0;
        prm->reg_normalize_idx_ratio= 0; //u12
    }
    else
    {
        prm->reg_accum_ofset_en = 1;
        prm->reg_normalize_idx_ratio= (1<<BIT_DEPTH_NORM)/ ((y_size+1)/2);   //u12
    }
}

//--------
void cmpr_param_fw_lut_init(sCMPRCORE_Param *prm)
{
    int bit_depth;
    int k, n,i;

    int budget2qp[6][40] = {{ 8, 8,   8,  7,   7,  6,  6,  5,  5,  4,   4,  4,  3,   2,  1, 0,  0, 0,  0,  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                            { 9, 8,   8,  7,   7,  7,  6,  6,  5,  5,   5,  4,  4,   4,  3, 3,  2, 2,  1,  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                            {12,12,  12, 11,  11, 10, 10,  9,  9,  8,   8,  8,  7,   7,  6, 6,  5, 5,  4,  4,  3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                            {14,14,  13, 13,  12, 12, 11, 11, 10,  9,   9,  8,  8,   7,  7, 6,  6, 5,  5,  4,  4,3,3,2,2,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
                            {16,16,  15, 15,  14, 14, 13, 13, 12, 12,  11, 11,  10, 10,  9, 9,  8, 8,  7,  7,  6,6,5,5,4,4,3,3,2,2,1,0,0,0,0,0,0,0,0,0},
                            {20,20,  19, 19,  18, 18, 17, 17, 16, 16,  15, 14,  14, 13, 13,12, 12,11, 11, 10, 10,9,9,8,8,7,7,6,6,5,5,4,4,3,3,2,2,1,0,0}};
    bit_depth = prm->reg_src_bit_depth;
    // initalize the luts
    for (k=0;k<3;k++)
    {
        bit_depth = (prm->reg_src_bit_depth);
        i = (bit_depth > 8)?(bit_depth - 8)>>1 : 0;
        if(i>4)
            i=5;
        for (n=0; n<(2*bit_depth); n++)
            prm->reg_lut_budget2qp[k][n] = budget2qp[i][n];
        for(;n<40;n++)
            prm->reg_lut_budget2qp[k][n] = 0;
    }
}

uint32_t mipi_isp_line_stride (uint32_t hsize){
    uint32_t line_stride;
    line_stride = (hsize*32+2047)>>11; //128 byte  align
    line_stride = line_stride<<8;
    return line_stride;
}

int aml_cmpr_init(int tnr_cmpr_en, int w, int h, int lossloss_en, uint8_t bayer){
    if (tnr_cmpr_en) {
        sCMPRCORE_Param  s_cmprcore_tnr_sub0;
        sCMPR2MIF_Param  s_cmpr2mif_tnr_sub0_wmif;
        sCMPR2MIF_Param  s_cmpr2mif_tnr_sub0_rmif;
        sCMPR2MIF_Param  s_cmpr2mif_tnr_meta0_wmif;
        sCMPR2MIF_Param  s_cmpr2mif_tnr_meta0_rmif;
        sCMPRCORE_Param  s_cmprcore_tnr_sub1;
        sCMPR2MIF_Param  s_cmpr2mif_tnr_sub1_wmif;
        sCMPR2MIF_Param  s_cmpr2mif_tnr_sub1_rmif;
        sCMPR2MIF_Param  s_cmpr2mif_tnr_meta1_wmif;
        sCMPR2MIF_Param  s_cmpr2mif_tnr_meta1_rmif;
        sMIPI2CMPR_Param s_mipi2cmpr_tnr_split;
        sMIPI2CMPR_Param s_mipi2cmpr_tnr_pack;

        mipi_tnr_de_soft_reset();

        int xphase_ofst = 0;
        int yphase_ofst = 0;
        if (bayer == BAYER_GRBG) {
            xphase_ofst = 0;
            yphase_ofst = 0;
        } else if (bayer == BAYER_RGGB) {
            xphase_ofst = 1;
            yphase_ofst = 0;

        } else if (bayer == BAYER_BGGR) {
            xphase_ofst = 0;
            yphase_ofst = 1;
        }

        int cmpr_tnr_temp3_en, cmpr_tnr_data_16bit,cmpr_tnr_bits;
        int cmpr_tnr_lossless, cmpr_tnr_line_stride;
        int cmpr_tnr_rd_1st_ignore = ISP_DMA_READ_START_SEL ? 0 : 1;
        int cmpr_tnr_vblank_dlys   = 100000;                        //vblank
        //---
        cmpr_tnr_temp3_en   = 1;//(ISP_TNR_MODE>>1) & 0x1;
        cmpr_tnr_data_16bit = 1;//(ISP_TNR_MODE & 0x1);             //should match tnr bit
        cmpr_tnr_line_stride = w*3;//mipi_isp_line_stride(w);    //should match tnr stide
        cmpr_tnr_lossless = lossloss_en;//TNR_CMPR_LOSSLESS ? 1 : 0;
        cmpr_tnr_bits     = cmpr_tnr_data_16bit ? 16 : 12;
        enable_mipi_cmpr_tnr(0,cmpr_tnr_temp3_en);
        //--------
        //TNR enc sub0
        s_cmprcore_tnr_sub0.reg_pic_xsize = w;
        s_cmprcore_tnr_sub0.reg_pic_ysize = h;
        s_cmprcore_tnr_sub0.reg_src_bit_depth = cmpr_tnr_bits;
        s_cmprcore_tnr_sub0.reg_lossless_en = cmpr_tnr_lossless;     //lossless
        s_cmprcore_tnr_sub0.reg_xphase_ofst = xphase_ofst;
        s_cmprcore_tnr_sub0.reg_yphase_ofst = yphase_ofst;
        s_cmprcore_tnr_sub0.reg_wdr_mode_en = 0;
        s_cmprcore_tnr_sub0.reg_wdr_delay_line = 4;
        init_mipi_cmprcore(TNR_CMPR_SUB0_BASE,&s_cmprcore_tnr_sub0);
        //TNR enc sub0 wmif
        s_cmpr2mif_tnr_sub0_wmif.reg_pic_hsize = w;
        s_cmpr2mif_tnr_sub0_wmif.reg_pic_vsize = h;
        s_cmpr2mif_tnr_sub0_wmif.reg_canvas_id = 0x12;
        s_cmpr2mif_tnr_sub0_wmif.reg_burst_len = 3;  //len=8
        s_cmpr2mif_tnr_sub0_wmif.reg_dol_mode  = 0;
        s_cmpr2mif_tnr_sub0_wmif.reg_mif_dbl_baddr_init = 1;
        s_cmpr2mif_tnr_sub0_wmif.reg_mif_dbl_baddr_en   = 1;
        s_cmpr2mif_tnr_sub0_wmif.reg_mif_baddr0 = (unsigned int)isp_temper_paddr[0].isp_paddr;
        s_cmpr2mif_tnr_sub0_wmif.reg_mif_baddr1 = (unsigned int)isp_temper_paddr[0].isp_paddr + w*h*3;
        init_mipi_cmpr2mif(MIPI_TNR_ENC_WMIF_PATH,TNR_CMPR_SUB0_WMIF_BASE,&s_cmpr2mif_tnr_sub0_wmif);
        //TNR enc sub0 rmif
        s_cmpr2mif_tnr_sub0_rmif.reg_pic_hsize = w;
        s_cmpr2mif_tnr_sub0_rmif.reg_pic_vsize = h;
        s_cmpr2mif_tnr_sub0_rmif.reg_canvas_id = 0x13;
        s_cmpr2mif_tnr_sub0_rmif.reg_burst_len = 3;
        s_cmpr2mif_tnr_sub0_rmif.reg_dol_mode = 0;
        s_cmpr2mif_tnr_sub0_rmif.reg_mif_dbl_baddr_init = 1;
        s_cmpr2mif_tnr_sub0_rmif.reg_mif_dbl_baddr_en   = 1;
        s_cmpr2mif_tnr_sub0_rmif.reg_mif_baddr0 = (unsigned int)isp_temper_paddr[0].isp_paddr;
        s_cmpr2mif_tnr_sub0_rmif.reg_mif_baddr1 = (unsigned int)isp_temper_paddr[0].isp_paddr + w*h*3;
        init_mipi_cmpr2mif(MIPI_TNR_DEC_RMIF_PATH,TNR_CMPR_SUB0_RMIF_BASE,&s_cmpr2mif_tnr_sub0_rmif);
        //TNR meta sub0 wmif
        s_cmpr2mif_tnr_meta0_wmif.reg_pic_hsize = w;
        s_cmpr2mif_tnr_meta0_wmif.reg_pic_vsize = h;
        s_cmpr2mif_tnr_meta0_wmif.reg_canvas_id = 0x14;
        s_cmpr2mif_tnr_meta0_wmif.reg_burst_len = 3;
        s_cmpr2mif_tnr_meta0_wmif.reg_dol_mode  = 0;
        s_cmpr2mif_tnr_meta0_wmif.reg_mif_dbl_baddr_init = 1;
        s_cmpr2mif_tnr_meta0_wmif.reg_mif_dbl_baddr_en   = 1;
        s_cmpr2mif_tnr_meta0_wmif.reg_mif_baddr0 = (unsigned int)isp_temper_paddr[0].isp_paddr + w * h * cmpr_tnr_bits /8;
        s_cmpr2mif_tnr_meta0_wmif.reg_mif_baddr1 = (unsigned int)isp_temper_paddr[0].isp_paddr + w * h * cmpr_tnr_bits /8 + w*h*3;
        init_mipi_cmpr2mif(MIPI_TNR_META_WMIF_PATH,MIPI_TNR_WSUB0_META_BASE,&s_cmpr2mif_tnr_meta0_wmif);
        //TNR meta sub0 rmif
        s_cmpr2mif_tnr_meta0_rmif.reg_pic_hsize = w;
        s_cmpr2mif_tnr_meta0_rmif.reg_pic_vsize = h;
        s_cmpr2mif_tnr_meta0_rmif.reg_canvas_id = 0x15;
        s_cmpr2mif_tnr_meta0_rmif.reg_burst_len = 3;
        s_cmpr2mif_tnr_meta0_rmif.reg_dol_mode = 0;
        s_cmpr2mif_tnr_meta0_rmif.reg_mif_dbl_baddr_init = 1;
        s_cmpr2mif_tnr_meta0_rmif.reg_mif_dbl_baddr_en   = 1;
        s_cmpr2mif_tnr_meta0_rmif.reg_mif_baddr0 = (unsigned int)isp_temper_paddr[0].isp_paddr + w * h * cmpr_tnr_bits /8;
        s_cmpr2mif_tnr_meta0_rmif.reg_mif_baddr1 = (unsigned int)isp_temper_paddr[0].isp_paddr + w * h * cmpr_tnr_bits /8 + w*h*3;
        init_mipi_cmpr2mif(MIPI_TNR_META_RMIF_PATH,MIPI_TNR_RSUB0_META_BASE,&s_cmpr2mif_tnr_meta0_rmif);
        if (cmpr_tnr_temp3_en == 1) {
            //TNR enc sub1
            s_cmprcore_tnr_sub1.reg_pic_xsize = w;
            s_cmprcore_tnr_sub1.reg_pic_ysize = h;
            s_cmprcore_tnr_sub1.reg_src_bit_depth = cmpr_tnr_bits;
            s_cmprcore_tnr_sub1.reg_lossless_en = cmpr_tnr_lossless;     //lossless
            s_cmprcore_tnr_sub1.reg_xphase_ofst = xphase_ofst;
            s_cmprcore_tnr_sub1.reg_yphase_ofst = yphase_ofst;
            s_cmprcore_tnr_sub1.reg_wdr_mode_en = 0;
            s_cmprcore_tnr_sub1.reg_wdr_delay_line = 4;
            init_mipi_cmprcore(TNR_CMPR_SUB1_BASE,&s_cmprcore_tnr_sub1);
            //TNR enc sub1 wmif
            s_cmpr2mif_tnr_sub1_wmif.reg_pic_hsize = w;
            s_cmpr2mif_tnr_sub1_wmif.reg_pic_vsize = h;
            s_cmpr2mif_tnr_sub1_wmif.reg_canvas_id = 0x16;
            s_cmpr2mif_tnr_sub1_wmif.reg_burst_len = 3;
            s_cmpr2mif_tnr_sub1_wmif.reg_dol_mode  = 0;
            s_cmpr2mif_tnr_sub1_wmif.reg_mif_dbl_baddr_init = 1;
            s_cmpr2mif_tnr_sub1_wmif.reg_mif_dbl_baddr_en   = 1;
            s_cmpr2mif_tnr_sub1_wmif.reg_mif_baddr0 = (unsigned int)isp_temper_paddr[0].isp_paddr  + w*h*3 * 2;
            s_cmpr2mif_tnr_sub1_wmif.reg_mif_baddr1 = (unsigned int)isp_temper_paddr[0].isp_paddr  + w*h*3 * 3;
            init_mipi_cmpr2mif(MIPI_TNR_ENC_WMIF_PATH,TNR_CMPR_SUB1_WMIF_BASE, &s_cmpr2mif_tnr_sub1_wmif);
            //TNR enc sub1 rmif
            s_cmpr2mif_tnr_sub1_rmif.reg_pic_hsize = w;
            s_cmpr2mif_tnr_sub1_rmif.reg_pic_vsize = h;
            s_cmpr2mif_tnr_sub1_rmif.reg_canvas_id = 0x17;
            s_cmpr2mif_tnr_sub1_rmif.reg_burst_len = 3;
            s_cmpr2mif_tnr_sub1_rmif.reg_dol_mode = 0;
            s_cmpr2mif_tnr_sub1_rmif.reg_mif_dbl_baddr_init = 1;
            s_cmpr2mif_tnr_sub1_rmif.reg_mif_dbl_baddr_en   = 1;
            s_cmpr2mif_tnr_sub1_rmif.reg_mif_baddr0 = (unsigned int)isp_temper_paddr[0].isp_paddr  + w*h*3 * 2;
            s_cmpr2mif_tnr_sub1_rmif.reg_mif_baddr1 = (unsigned int)isp_temper_paddr[0].isp_paddr  + w*h*3 * 3;
            init_mipi_cmpr2mif(MIPI_TNR_DEC_RMIF_PATH,TNR_CMPR_SUB1_RMIF_BASE, &s_cmpr2mif_tnr_sub1_rmif);
            //TNR meta sub1 wmif
            s_cmpr2mif_tnr_meta1_wmif.reg_pic_hsize = w;
            s_cmpr2mif_tnr_meta1_wmif.reg_pic_vsize = h;
            s_cmpr2mif_tnr_meta1_wmif.reg_canvas_id = 0x18;
            s_cmpr2mif_tnr_meta1_wmif.reg_burst_len = 3;
            s_cmpr2mif_tnr_meta1_wmif.reg_dol_mode  = 0;
            s_cmpr2mif_tnr_meta1_wmif.reg_mif_dbl_baddr_init = 1;
            s_cmpr2mif_tnr_meta1_wmif.reg_mif_dbl_baddr_en   = 1;
            s_cmpr2mif_tnr_meta1_wmif.reg_mif_baddr0 = (unsigned int)isp_temper_paddr[0].isp_paddr  + w*h*3 * 2 + w*h*cmpr_tnr_bits/8;
            s_cmpr2mif_tnr_meta1_wmif.reg_mif_baddr1 = (unsigned int)isp_temper_paddr[0].isp_paddr  + w*h*3 * 3 + w*h*cmpr_tnr_bits/8;
            init_mipi_cmpr2mif(MIPI_TNR_META_WMIF_PATH,MIPI_TNR_WSUB1_META_BASE,&s_cmpr2mif_tnr_meta1_wmif);
            //TNR enc sub1 rmif
            s_cmpr2mif_tnr_meta1_rmif.reg_pic_hsize = w;
            s_cmpr2mif_tnr_meta1_rmif.reg_pic_vsize = h;
            s_cmpr2mif_tnr_meta1_rmif.reg_canvas_id = 0x19;
            s_cmpr2mif_tnr_meta1_rmif.reg_burst_len = 3;
            s_cmpr2mif_tnr_meta1_rmif.reg_dol_mode = 0;
            s_cmpr2mif_tnr_meta1_rmif.reg_mif_dbl_baddr_init = 1;
            s_cmpr2mif_tnr_meta1_rmif.reg_mif_dbl_baddr_en   = 1;
            s_cmpr2mif_tnr_meta1_rmif.reg_mif_baddr0 = (unsigned int)isp_temper_paddr[0].isp_paddr  + w*h*3 * 2 + w*h*cmpr_tnr_bits/8;
            s_cmpr2mif_tnr_meta1_rmif.reg_mif_baddr1 = (unsigned int)isp_temper_paddr[0].isp_paddr  + w*h*3 * 3 + w*h*cmpr_tnr_bits/8;
            init_mipi_cmpr2mif(MIPI_TNR_META_RMIF_PATH,MIPI_TNR_RSUB1_META_BASE, &s_cmpr2mif_tnr_meta1_rmif);
        }
       //TNR split
       s_mipi2cmpr_tnr_split.reg_pic_hsize = w;
       s_mipi2cmpr_tnr_split.reg_pic_vsize = h;
       s_mipi2cmpr_tnr_split.reg_rdma_trigger_mode   = 1;    //use frame_end for RDMA
       s_mipi2cmpr_tnr_split.reg_tnr_temp3_en = cmpr_tnr_temp3_en;
       s_mipi2cmpr_tnr_split.reg_frmrst_mode       = TNR_CMPR_WR_FRMRST_MODE;      //frmrst_mode = 0
       s_mipi2cmpr_tnr_split.reg_frmrst_dlys4gen   = cmpr_tnr_vblank_dlys;   //vblank delay
       s_mipi2cmpr_tnr_split.reg_frmrst_dlys4abort = cmpr_tnr_vblank_dlys;   //vblank delay
       s_mipi2cmpr_tnr_split.reg_raw67_ext         = 0;
       s_mipi2cmpr_tnr_split.reg_raw_mode          = cmpr_tnr_data_16bit;
       s_mipi2cmpr_tnr_split.reg_iaddr_bgn[0]      = (unsigned int)isp_temper_paddr[0].isp_paddr;
       s_mipi2cmpr_tnr_split.reg_iaddr_bgn[1]      = (unsigned int)isp_temper_paddr[0].isp_paddr + w*h*3 * 2;
       s_mipi2cmpr_tnr_split.line_stride           = cmpr_tnr_line_stride;
       init_mipi_mipi2cmpr(MIPI2CMPR_TNR_SPLIT_PATH,MIPI_TNR_CMPR_SPLIT_BASE, &s_mipi2cmpr_tnr_split);
       //TNR pack
       s_mipi2cmpr_tnr_pack.reg_pic_hsize = w;
       s_mipi2cmpr_tnr_pack.reg_pic_vsize = h;
       s_mipi2cmpr_tnr_pack.reg_rdma_trigger_mode   = 1;    //use frame_end for RDMA
       s_mipi2cmpr_tnr_pack.reg_pack_skip_flag_init = cmpr_tnr_rd_1st_ignore;
       s_mipi2cmpr_tnr_pack.reg_pack_skip_auto_ctrl = cmpr_tnr_rd_1st_ignore;
       s_mipi2cmpr_tnr_pack.reg_pack_keep_axicmd_order=1;
       s_mipi2cmpr_tnr_pack.reg_tnr_temp3_en = cmpr_tnr_temp3_en;
       s_mipi2cmpr_tnr_pack.reg_frmrst_mode       = TNR_CMPR_RD_FRMRST_MODE; //frmrst_mode = 0
       s_mipi2cmpr_tnr_pack.reg_frmrst_dlys4gen   = cmpr_tnr_vblank_dlys;   //vblank delay
       s_mipi2cmpr_tnr_pack.reg_frmrst_dlys4abort = cmpr_tnr_vblank_dlys;   //vblank delay
       s_mipi2cmpr_tnr_pack.reg_raw67_ext         = 0;
       s_mipi2cmpr_tnr_pack.reg_raw_mode          = cmpr_tnr_data_16bit;
       s_mipi2cmpr_tnr_pack.reg_iaddr_bgn[0]      = (unsigned int)isp_temper_paddr[0].isp_paddr;
       s_mipi2cmpr_tnr_pack.reg_iaddr_bgn[1]      = (unsigned int)isp_temper_paddr[0].isp_paddr + w*h*3 * 2;
       s_mipi2cmpr_tnr_pack.line_stride           = cmpr_tnr_line_stride;
       init_mipi_mipi2cmpr(MIPI2CMPR_TNR_PACK_PATH,MIPI_TNR_CMPR_PACK_BASE, &s_mipi2cmpr_tnr_pack);

       cmpr_wr(TRSUB0_ISP_LOSS_MISC, 0x1b);
       mipi_tnr_de_enable();
       enable_mipi_cmpr_tnr(1,cmpr_tnr_temp3_en);
       if (s_mipi2cmpr_tnr_pack.reg_frmrst_mode == 2)
          set_mipi_mipi2cmpr_sw_frmbgn(MIPI_TNR_CMPR_PACK_BASE);

       cmpr_wr(MIPI_TNR_CMPR_INT_EN,(1<<15)|(1<<14)|(1<<13)|(1<<11)|(1<<4)|(1<<2));

    } else {
        //enable_mipi_cmpr_tnr(0,1);
        //mipi_tnr_de_bypass();
    }

    pr_err("init decmpr success w%d h%d, enable:%d, lossless:%d\n",w,h,tnr_cmpr_en,lossloss_en);
    return 0;
}

int aml_cmpr_deinit(void)
{
    if (!g_cmpr) {
        pr_info("%d, g_cmpr is NULL.\n", __LINE__);
        return -1;
    }

    iounmap(g_cmpr->base_addr);
    g_cmpr->base_addr = NULL;
    iounmap(g_cmpr->cntl_addr);
    g_cmpr->cntl_addr = NULL;

    kfree(g_cmpr);
    g_cmpr = NULL;
    return 0;
}

//===========================================================================================

