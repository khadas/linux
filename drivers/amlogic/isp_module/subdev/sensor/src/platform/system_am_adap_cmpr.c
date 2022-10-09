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
#define pr_fmt(fmt) "AM_ADAP_CMPR: " fmt

#include "system_am_adap.h"
#include "system_log.h"

extern resource_size_t dol_buf[DOL_BUF_SIZE];

//========decmpr_adapt========================================
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
        adap_wr_bit(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_CTRL),CMPR_IO,0xf,4,4);   //reg_use_inter_fmt=0xf, use internal setting
        adap_write(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_FRAME_HOLD),CMPR_IO,0x0);         //reg_frame_hold_nums=0: no RDMA
        adap_wr_bit(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_MISC),CMPR_IO,0x0,4,2);  //reg_not_wait_input4busy=0x0, reg_busy_status_use4abort=0;
        if(i==1) { //dec
	        adap_wr_bit(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_MISC),CMPR_IO,3,0,2);  //for dec: reg_inter_preslc_bitaccum=1/reg_inter_preslc_fifolevel=1
            adap_wr_bit(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_MISC),CMPR_IO,1,4,1);  //for dec: reg_not_wait_input4busy=1
        }
        dwTemp = ((prm->reg_lossless_en&0x1)   <<31) |  //1bits
             ((prm->reg_src_bit_depth&0x1f)<<24) |  //5bits
             (1                            <<21) |  //3bits, raw_mode=1
             (0                            <<20) |  //1bits, reg_mono_comp_mode=0
             ((prm->reg_xphase_ofst&0x3)   <<18) |  //2bits
             ((prm->reg_yphase_ofst&0x3)   <<16) |  //2bits
             ((prm->reg_ratio_bppx16&0x1ff)<< 4) |  //9bits
             (2                            << 0) ;  //3bits, reg_comp_chn_size=2
        adap_write(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_BASIS),CMPR_IO,dwTemp);
        dwTemp = prm->reg_pic_xsize | (prm->reg_pic_ysize<<16);
        adap_write(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_PIC_SIZE),CMPR_IO,dwTemp);
        dwTemp = (0 << 31) |  //reg_use_sw_preslc_bitaccum=0
                 (0 << 28) |  //reg_slice_num_mode=0: one slice
                 (prm->reg_pic_ysize);
        adap_write(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_SLICE_SIZE),CMPR_IO,dwTemp);
        adap_wr_bit(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_PRESL_FIFO_LEVEL),CMPR_IO,0,31,1); //reg_use_sw_preslc_fifolevel=0

        dwTemp = (prm->reg_wdr_mode_en<<31) | (prm->reg_wdr_delay_line);
        adap_write(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_WDR_LINE_DELAY),CMPR_IO,dwTemp); //WDR mode

        adap_wr_bit(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_ERROR_COUNT_THD),CMPR_IO,prm->reg_error_count_thd,0,16);
        adap_wr_bit(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_FLATNESS_0),CMPR_IO,prm->reg_flatness_qp_thresh,20,5);
        adap_wr_bit(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_FLATNESS_1),CMPR_IO,prm->reg_flatness_q_err_thresh,0,12);

        dwTemp = (prm->reg_rc_group_y<<16) | (prm->reg_slcln_ratio);
        adap_write(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_RC_GROUP_2),CMPR_IO,dwTemp);
        dwTemp = prm->reg_default_value[1] | (prm->reg_default_value[0]<<16);
        adap_write(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_DEFAULT_VALUE),CMPR_IO,dwTemp);
        adap_wr_bit(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_DEFAULT_VALUE_1),CMPR_IO,prm->reg_default_value[2],0,16);

        dwTemp = (prm->reg_flatness_det_thresh<<16) | (prm->reg_flatness_det_thresh_max);
        adap_write(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_FLATNESS_TH),CMPR_IO,dwTemp);
        adap_wr_bit(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_FLATNESS_0),CMPR_IO,prm->reg_flatness_accum_thresh,0,12);

        dwTemp = (prm->reg_pred_err0<<16) | (prm->reg_pred_flat_thd);
        adap_write(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_PRED_TH),CMPR_IO,dwTemp);

        dwTemp = (prm->reg_pred_drt5flat_thd<<16) | (prm->reg_pred_drt5_thrd);
        adap_write(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_PRED_TH_1),CMPR_IO,dwTemp);

        dwTemp = (prm->reg_pixel_luma_adj_th[1]<<16) | (prm->reg_pixel_luma_adj_th[0]);
        adap_write(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_LUMA_TH),CMPR_IO,dwTemp);

        dwTemp = (prm->reg_rc_fifo_margin_dlt[3]<<24) | (prm->reg_rc_fifo_margin_dlt[2]<<16) |
                 (prm->reg_rc_fifo_margin_dlt[1]<< 8) | (prm->reg_rc_fifo_margin_dlt[0]<< 0) ;
        adap_write(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_FIFO_DLT),CMPR_IO,dwTemp);
        dwTemp = (prm->reg_rc_fifo_margin_dlt[5]<<8) | (prm->reg_rc_fifo_margin_dlt[4]);
        adap_wr_bit(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_FIFO_AVG),CMPR_IO,dwTemp,12,14);

        dwTemp = (prm->reg_rc_fifo_qpx2_margin_dlt[3]<<24) | (prm->reg_rc_fifo_qpx2_margin_dlt[2]<<16) |
                 (prm->reg_rc_fifo_qpx2_margin_dlt[1]<< 8) | (prm->reg_rc_fifo_qpx2_margin_dlt[0]<< 0) ;
        adap_write(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_RC_QP_MARGIN_5),CMPR_IO,dwTemp);

        dwTemp = (prm->reg_rc_master_qpx2_max<<8) | (prm->reg_rc_master_qpx2_min);
        adap_wr_bit(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_RC_BUDGET_5),CMPR_IO,dwTemp,16,16);

        adap_wr_bit(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_ACCUM_OFSET_0),CMPR_IO,prm->reg_accum_ofset_en,0,1);
        adap_wr_bit(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_ACCUM_OFSET_3),CMPR_IO,prm->reg_normalize_idx_ratio,0,12);

        for (k=0;k<3;k++) {
            for (n=0;n<40;n+=4) {
                dwTemp = (prm->reg_lut_budget2qp[k][n*4+3]<<24) | (prm->reg_lut_budget2qp[k][n*4+2]<<16) |
                         (prm->reg_lut_budget2qp[k][n*4+1]<< 8) | (prm->reg_lut_budget2qp[k][n*4+0]<< 0) ;
                adap_write(addrbase_c+CMPR_OFS(AWSUB0_ISP_LOSS_QP_MAP_CHN0+k*10*4+n),CMPR_IO,dwTemp);
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
   adap_write(addrbase+CMPR2MIF_OFS(MIPI_TNR_META_WSUB0_MIF_CTRL),CMPR_IO,dwTemp);
   dwTemp = (1<<18)|(1<<17)|(0<<16)|(((dfifo_size*3/4)&0xff)<<8)|((((dfifo_size*1/4)&0xff))<<0); //
   adap_write(addrbase+CMPR2MIF_OFS(ISP_TNR_META_WSUB0_MIF_QOS),CMPR_IO,dwTemp);
   dwTemp = (1<<18)|(1<<17)|(0<<16)|(((dfifo_size*3/4)&0xff)<<8)|((((dfifo_size*1/4)&0xff))<<0); //
   adap_write(addrbase+CMPR2MIF_OFS(MIPI_TNR_META_WSUB0_MIF_URGENT),CMPR_IO,dwTemp);

   dwTemp = (1<<3) |  //reg_mif_busy_noresp_check=1
            (0<<2) |  //reg_mif_wait_input4busy=0
            ((prm->reg_mif_dbl_baddr_init&0x1)<<1) | //
            ((prm->reg_mif_dbl_baddr_en&0x1));
   adap_write(addrbase+CMPR2MIF_OFS(MIPI_TNR_META_WSUB0_MIF_MISC),CMPR_IO,dwTemp);
   adap_write(addrbase+CMPR2MIF_OFS(MIPI_TNR_META_WSUB0_MIF_BADDR),CMPR_IO,prm->reg_mif_baddr0);
   adap_write(addrbase+CMPR2MIF_OFS(MIPI_TNR_META_WSUB0_MIF_BADDR1),CMPR_IO,prm->reg_mif_baddr1);
   adap_wr_bit(addrbase+CMPR2MIF_OFS(MIPI_TNR_META_WSUB0_MIF_FIFO),CMPR_IO,dfifo_size,0,8);
   if(cmpr2mif_path==4 || cmpr2mif_path==5) {  //meta
      meta_size_b128 = ((prm->reg_pic_hsize*4+127)/128)*prm->reg_pic_vsize;
      adap_wr_bit(addrbase+CMPR2MIF_OFS(MIPI_TNR_META_WSUB0_MIF_SIZE),CMPR_IO,meta_size_b128,0,24);

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
  adap_write(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_SPLIT_CTRL),CMPR_IO,dwTemp);
  dwTemp = prm->reg_pic_hsize;
  adap_write(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_SPLIT_SIZE),CMPR_IO,dwTemp);

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
  } else if (prm->reg_frmrst_mode==2) {
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
  adap_write(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_SPLIT_FRMRST_CTRL),CMPR_IO,dwTemp);
  adap_write(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_SPLIT_FRMRST_DLY0),CMPR_IO,prm->reg_frmrst_dlys4gen);
  adap_write(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_SPLIT_FRMRST_DLY1),CMPR_IO,prm->reg_frmrst_dlys4abort);

  dwTemp = ((prm->reg_raw67_ext&0x1) <<20) |
           ((prm->reg_raw_mode&0x7)  <<16) |
           ((dfifo_size&0x3ff)       <<0)  ;
  adap_write(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_WSUB0_CTRL),CMPR_IO,dwTemp);
  adap_write(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_WSUB0_IADDRS),CMPR_IO,prm->reg_iaddr_bgn[0]);
  adap_write(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_WSUB0_IADDRE),CMPR_IO,prm->reg_iaddr_bgn[0]+(frame_size_b128-1)*16);
  if(mipi2cmpr_path==2 || mipi2cmpr_path==3) {
    dwTemp = ((prm->reg_raw67_ext&0x1) <<20) |
             ((prm->reg_raw_mode&0x7)  <<16) |
             ((dfifo_size&0x3ff)       <<0)  ;
    adap_write(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_WSUB1_CTRL),CMPR_IO,dwTemp);
    adap_write(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_WSUB1_IADDRS),CMPR_IO,prm->reg_iaddr_bgn[1]); 
    adap_write(addrbase+MIPI2CMPR_OFS(MIPI_TNR_CMPR_WSUB1_IADDRE),CMPR_IO,prm->reg_iaddr_bgn[1]+(frame_size_b128-1)*16);
  }
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
    blk_num  = (x_size + SIZE_GROUP*2 -1)/SIZE_GROUP*2;
	prm->reg_error_count_thd = (50<<rs);
	prm->reg_flatness_qp_thresh = 4 + rs;
	prm->reg_flatness_q_err_thresh = 4<<rs;
    prm->reg_rc_group_y  = y_size;
    //prm->reg_pic_ysize = y_size;
    //prm->reg_pic_xsize = x_size;
    for(i=0;i<3;i++)
        prm->reg_default_value[i] = 1<<(bw-1);

    prm->reg_flatness_det_thresh = 8<<rs;
    prm->reg_flatness_det_thresh_max = 10<<rs;
    prm->reg_flatness_accum_thresh = 1<<(bw-2);
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
    prm->reg_slcln_ratio = 1<<(PIC_HGROUP_RATIO)/blk_num;  //u14, dynamic ratio to idx of extra bit budget for the cells within the line
	if((y_size+1)/2<16)
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

void mipi_adapt_de_enable (void){
    uint32_t data = 0;
	adap_read(MIPI_TOP_ADAPT_DE_CTRL0, CMPR_CNTL_IO,&data);
	adap_write(MIPI_TOP_TNR_DE_CTRL0, CMPR_CNTL_IO,data & (~(1<<7 | 1<<3)));
}

void mipi_adapt_de_bypass (void){
    uint32_t data = 0;
	adap_read(MIPI_TOP_ADAPT_DE_CTRL0,CMPR_CNTL_IO, &data);
    adap_write(MIPI_TOP_ADAPT_DE_CTRL0,CMPR_CNTL_IO, data | 1<<7 | 1<<3);
}

//----
void set_mipi_cmpr_rdma_mode (int is_adpat,int enable) {
  int dwTemp;
  dwTemp = enable ? 0xffffffff : 0;
  if (is_adpat==1) {  //adapt
     adap_write(MIPI_ADAPT_CMPR_SPLIT_FRMHOLD,CMPR_IO,dwTemp);
     adap_write(MIPI_ADAPT_CMPR_PACK_FRMHOLD ,CMPR_IO,dwTemp);
     adap_write(AWSUB0_ISP_LOSS_MIF_FRM_HOLD ,CMPR_IO,dwTemp);
     adap_write(ARSUB0_ISP_LOSS_MIF_FRM_HOLD ,CMPR_IO,dwTemp);
     adap_wr_bit(MIPI_ADAPT_CMPR_SPLIT_FRMRST_CTRL,CMPR_IO,enable&0x1,15,1);
     adap_wr_bit(MIPI_ADAPT_CMPR_PACK_FRMRST_CTRL ,CMPR_IO,enable&0x1,15,1);
  } else {   //tnr
     adap_write(MIPI_TNR_CMPR_SPLIT_FRMHOLD,CMPR_IO,  dwTemp);
     adap_write(MIPI_TNR_CMPR_PACK_FRMHOLD ,CMPR_IO,  dwTemp);
     adap_write(TWSUB0_ISP_LOSS_MIF_FRM_HOLD ,CMPR_IO,dwTemp);
     adap_write(TRSUB0_ISP_LOSS_MIF_FRM_HOLD ,CMPR_IO,dwTemp);
     adap_write(TWSUB0_ISP_LOSS_MIF_FRM_HOLD ,CMPR_IO,dwTemp);
     adap_write(TRSUB0_ISP_LOSS_MIF_FRM_HOLD ,CMPR_IO,dwTemp);
     adap_wr_bit(MIPI_TNR_CMPR_SPLIT_FRMRST_CTRL,CMPR_IO,enable&0x1,15,1);
     adap_wr_bit(MIPI_TNR_CMPR_PACK_FRMRST_CTRL ,CMPR_IO,enable&0x1,15,1);
  }
}

void enable_mipi_cmpr_adapt(int enable) {
  //encoder core
  adap_wr_bit(AWSUB0_ISP_LOSS_CTRL,CMPR_IO,enable,0,1);
  //encoder wrmif
  adap_wr_bit(AWSUB0_ISP_LOSS_MIF_CTRL,CMPR_IO,enable,0,1);
  //decoder core
  adap_wr_bit(ARSUB0_ISP_LOSS_CTRL,CMPR_IO,enable,0,1);
  //decoder rdmif
  adap_wr_bit(ARSUB0_ISP_LOSS_MIF_CTRL,CMPR_IO,enable,0,1);
  //split
  adap_wr_bit(MIPI_ADAPT_CMPR_SPLIT_CTRL,CMPR_IO,enable,0,1);
  adap_wr_bit(MIPI_ADAPT_CMPR_SPLIT_CTRL,CMPR_IO,1,16,1);
  //pack
  adap_wr_bit(MIPI_ADAPT_CMPR_PACK_CTRL,CMPR_IO,enable,0,1);
}
#define DATA_BUF0_ADDR 0x1000000

int aml_adap_decmpr_init(int adapt_cmpr_en, int w, int h, int fmt) {
    if(adapt_cmpr_en==1) {
		   sCMPRCORE_Param	s_cmprcore_adapt;
		   sCMPR2MIF_Param	s_cmpr2mif_adpat_wmif;
		   sCMPR2MIF_Param	s_cmpr2mif_adpat_rmif;
		   sMIPI2CMPR_Param s_mipi2cmpr_adpat_split;
		   sMIPI2CMPR_Param s_mipi2cmpr_adpat_pack;
		   int cmpr_adapt_raw_type, cmpr_adapt_raw_mode, cmpr_adapt_raw_bits;
		   int cmpr_adapt_lossless, cmpr_adapt_dol_mode, cmpr_adapt_line_stride;
		   int cmpr_adapt_vblank_dlys = 200000;
		   //---
		   cmpr_adapt_raw_type	  = fmt;
		   cmpr_adapt_line_stride = w*10/8;
		   cmpr_adapt_dol_mode = ADAPT_DOL_MODE ? 1 : 0;
		   cmpr_adapt_lossless = ADAPT_CMPR_LOSSLESS ? 1:0;
		   cmpr_adapt_raw_mode = cmpr_adapt_raw_type - 0x28;
		   cmpr_adapt_raw_bits = cmpr_adapt_raw_mode==0 ? 6  :
								 cmpr_adapt_raw_mode==1 ? 7  :
								 cmpr_adapt_raw_mode==2 ? 8  :
								 cmpr_adapt_raw_mode==3 ? 10 :
								 cmpr_adapt_raw_mode==4 ? 12 : 14;
		   enable_mipi_cmpr_adapt(0);
		   //Adapt CMPR_CORE
		   s_cmprcore_adapt.reg_pic_xsize = w;
		   s_cmprcore_adapt.reg_pic_ysize = h;
		   s_cmprcore_adapt.reg_src_bit_depth = (cmpr_adapt_raw_bits<8) ? 8 : cmpr_adapt_raw_bits;
		   s_cmprcore_adapt.reg_lossless_en = cmpr_adapt_lossless;	   //lossless
		   s_cmprcore_adapt.reg_xphase_ofst = 0;
		   s_cmprcore_adapt.reg_yphase_ofst = 0;
		   s_cmprcore_adapt.reg_wdr_mode_en = cmpr_adapt_dol_mode;
		   s_cmprcore_adapt.reg_wdr_delay_line = 1;
		   init_mipi_cmprcore(ADAPT_CMPR_SUB0_BASE,&s_cmprcore_adapt);
		   //Adapt wmif
		   s_cmpr2mif_adpat_wmif.reg_pic_hsize = w;
		   s_cmpr2mif_adpat_wmif.reg_pic_vsize = h;
		   s_cmpr2mif_adpat_wmif.reg_canvas_id = 0x10;
		   s_cmpr2mif_adpat_wmif.reg_burst_len = 4;
		   s_cmpr2mif_adpat_wmif.reg_dol_mode = cmpr_adapt_dol_mode;
		   s_cmpr2mif_adpat_wmif.reg_mif_dbl_baddr_init = 1;
		   s_cmpr2mif_adpat_wmif.reg_mif_dbl_baddr_en	= 1;
		   s_cmpr2mif_adpat_wmif.reg_mif_baddr0 = dol_buf[0];
		   s_cmpr2mif_adpat_wmif.reg_mif_baddr1 = dol_buf[0];
		   init_mipi_cmpr2mif(MIPI_ADAPT_ENC_WMIF_PATH,ADAPT_CMPR_SUB0_WMIF_BASE,&s_cmpr2mif_adpat_wmif);
		   //Adapt rmif
		   s_cmpr2mif_adpat_rmif.reg_pic_hsize = w;
		   s_cmpr2mif_adpat_rmif.reg_pic_vsize = h;
		   s_cmpr2mif_adpat_rmif.reg_canvas_id = 0x11;
		   s_cmpr2mif_adpat_rmif.reg_burst_len = 4;
		   s_cmpr2mif_adpat_rmif.reg_dol_mode = cmpr_adapt_dol_mode;
		   s_cmpr2mif_adpat_rmif.reg_mif_dbl_baddr_init = 1;
		   s_cmpr2mif_adpat_rmif.reg_mif_dbl_baddr_en	= 1;
		   s_cmpr2mif_adpat_rmif.reg_mif_baddr0 = dol_buf[0];
		   s_cmpr2mif_adpat_rmif.reg_mif_baddr1 = dol_buf[0];
		   init_mipi_cmpr2mif(MIPI_ADAPT_DEC_RMIF_PATH,ADAPT_CMPR_SUB0_RMIF_BASE,&s_cmpr2mif_adpat_rmif);
		   //Adapt split
		   s_mipi2cmpr_adpat_split.reg_pic_hsize = w;
		   s_mipi2cmpr_adpat_split.reg_pic_vsize = h;
		   s_mipi2cmpr_adpat_split.reg_rdma_trigger_mode   = 1;    //use frame_end for RDMA
		   //s_mipi2cmpr_adpat_split.reg_pack_skip_flag_init = 0;
		   //s_mipi2cmpr_adpat_split.reg_pack_skip_auto_ctrl = 0;
		   //s_mipi2cmpr_adpat_split.reg_pack_keep_axicmd_order=0;
		   //s_mipi2cmpr_adpat_split.reg_tnr_temp3_en = 0;
		   s_mipi2cmpr_adpat_split.reg_frmrst_mode		 = ADAPT_CMPR_WR_FRMRST_MODE;	   //frmrst_mode = 0
		   s_mipi2cmpr_adpat_split.reg_frmrst_dlys4gen	 = cmpr_adapt_vblank_dlys;	 //vblank delay
		   s_mipi2cmpr_adpat_split.reg_frmrst_dlys4abort = cmpr_adapt_vblank_dlys;	 //vblank delay
		   s_mipi2cmpr_adpat_split.reg_raw67_ext		 =(cmpr_adapt_raw_mode<2) ? 1 : 0;
		   s_mipi2cmpr_adpat_split.reg_raw_mode 		 = cmpr_adapt_raw_mode; 	 //raw14: 3bit->raw6/7/8/10/12/14
		   s_mipi2cmpr_adpat_split.reg_iaddr_bgn[0] 	 = dol_buf[0];
		   s_mipi2cmpr_adpat_split.line_stride			 = cmpr_adapt_line_stride;
		   init_mipi_mipi2cmpr(MIPI2CMPR_ADAPT_SPLIT_PATH,MIPI_ADAPT_CMPR_SPLIT_BASE,&s_mipi2cmpr_adpat_split);
		   //Adapt pack
		   s_mipi2cmpr_adpat_pack.reg_pic_hsize = w;
		   s_mipi2cmpr_adpat_pack.reg_pic_vsize = h;
		   s_mipi2cmpr_adpat_pack.reg_rdma_trigger_mode   = 1;	  //use frame_end for RDMA
		   s_mipi2cmpr_adpat_pack.reg_pack_skip_flag_init = 0;
		   s_mipi2cmpr_adpat_pack.reg_pack_skip_auto_ctrl = 0;
		   s_mipi2cmpr_adpat_pack.reg_pack_keep_axicmd_order=1;
		   //s_mipi2cmpr_adpat_pack.reg_tnr_temp3_en = 0;
		   s_mipi2cmpr_adpat_pack.reg_frmrst_mode		= ADAPT_CMPR_RD_FRMRST_MODE;	  //frmrst_mode = 0
		   s_mipi2cmpr_adpat_pack.reg_frmrst_dlys4gen	= cmpr_adapt_vblank_dlys;	//vblank delay
		   s_mipi2cmpr_adpat_pack.reg_frmrst_dlys4abort = cmpr_adapt_vblank_dlys;	//vblank delay
		   s_mipi2cmpr_adpat_pack.reg_raw67_ext 		=(cmpr_adapt_raw_mode<2) ? 1 : 0;;
		   s_mipi2cmpr_adpat_pack.reg_raw_mode			= cmpr_adapt_raw_mode;		//raw14: 3bit->raw6/7/8/10/12/14
		   s_mipi2cmpr_adpat_pack.reg_iaddr_bgn[0]		= dol_buf[0];
		   s_mipi2cmpr_adpat_pack.line_stride			= cmpr_adapt_line_stride;
		   init_mipi_mipi2cmpr(MIPI2CMPR_ADAPT_PACK_PATH,MIPI_ADAPT_CMPR_PACK_BASE,&s_mipi2cmpr_adpat_pack);
		   //
		   mipi_adapt_de_enable();
		   enable_mipi_cmpr_adapt(1);
		   //start adapter axi 1to2
		   adap_write(MIPI_TOP_ADAPT_DE_CTRL0,CMPR_CNTL_IO,1);
		   adap_write(MIPI_TOP_ADAPT_DE_CTRL1,CMPR_CNTL_IO,1);
		   adap_write(MIPI_TOP_ADAPT_DE_CTRL2,CMPR_CNTL_IO,dol_buf[0]);
		   adap_write(MIPI_TOP_ADAPT_DE_CTRL3,CMPR_CNTL_IO,dol_buf[0]+w*10/8*h);
		   pr_err("init adapt_decmpr--------------------- success w%d h%d\n",w,h);
	} else {
		enable_mipi_cmpr_adapt(0);
		mipi_adapt_de_bypass();
	}
	adap_write(MIPI_ADAPT_CMPR_INT_EN,CMPR_IO,(1<<15)|(1<<14)|(1<<13)|(1<<11)|(1<<4)|(1<<2));
	return 0;
}

int bypass_init(void) {
	uint32_t data = 0;
	adap_read(MIPI_TOP_ADAPT_DE_CTRL0 , CMPR_CNTL_IO, &data);
	adap_write(MIPI_TOP_ADAPT_DE_CTRL0, CMPR_CNTL_IO, data | 1<<7 | 1<<3);  //adapt_de_bypass

	adap_read(MIPI_TOP_TNR_DE_CTRL0 , CMPR_CNTL_IO, &data);
	adap_write(MIPI_TOP_TNR_DE_CTRL0, CMPR_CNTL_IO, data | 1 << 0);       //tnr_de_bypass
	return 0;
}