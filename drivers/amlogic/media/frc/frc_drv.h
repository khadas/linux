/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/frc/frc_drv.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef FRC_DRV_H
#define FRC_DRV_H

#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/amlogic/media/frc/frc_common.h>
#include "frc_interface.h"

// frc_20210720_divided_reg_table
// frc_20210804_add_powerdown_in_shutdown
// frc_20210806 frc support 4k1k@120hz
// frc_20210811_add_reinit_fw_alg_when_resume
// frc_20210812 frc add demo function
// frc_20210818 frc add read version function
// frc_20210824 frc video delay interface
// frc_20210907 frc set delay frame"

#define FRC_FW_VER			"2021-0918 frc chg me_dly_vofst"
#define FRC_KERDRV_VER                  1058
#define FRC_DEVNO	1
#define FRC_NAME	"frc"
#define FRC_CLASS_NAME	"frc"

/*
extern int frc_dbg_en;
#define pr_frc(level, fmt, arg...)			\
	do {						\
		if ((frc_dbg_en >= (level) && frc_dbg_en < 3) || frc_dbg_en == level)	\
			pr_info("frc: " fmt, ## arg);	\
	} while (0)
*/
//------------------------------------------------------- buf define start
#define FRC_COMPRESS_RATE		60                 /*100: means no compress,60,80,50,55*/
#define FRC_COMPRESS_RATE_60_SIZE       (212 * 1024 * 1024)    // Need 209.2MB ( 0xD60000) 4MB Align
#define FRC_COMPRESS_RATE_80_SIZE       (276 * 1024 * 1024)    // Need 274.7MB  4MB Align
#define FRC_COMPRESS_RATE_50_SIZE       (180 * 1024 * 1024)    // Need 176.4MB  4MB Align
#define FRC_COMPRESS_RATE_55_SIZE       (196 * 1024 * 1024)    // Need 192.7MB  4MB Align

#define FRC_TOTAL_BUF_NUM		16
#define FRC_MEMV_BUF_NUM		6
#define FRC_MEMV2_BUF_NUM		7
#define FRC_MEVP_BUF_NUM		2

#define FRC_SLICER_NUM			4

/*down scaler config*/
#define FRC_ME_SD_RATE_HD		2
#define FRC_ME_SD_RATE_4K		4
#define FRC_LOGO_SD_RATE		1
#define FRC_HME_SD_RATE			4

#define FRC_HVSIZE_ALIGN_SIZE		16

/*bit number config*/
#define FRC_MC_BITS_NUM			10
#define FRC_ME_BITS_NUM			8

/*buff define and config*/
#define LOSSY_MC_INFO_LINE_SIZE		128	/*bytes*/
#define LOSSY_ME_INFO_LINE_SIZE		128	/*bytes*/
//------------------------------------------------------- buf define end

enum chip_id {
	ID_NULL = 0,
	ID_T3,
};

struct dts_match_data {
	enum chip_id chip;
};

struct frc_data_s {
	const struct dts_match_data *match_data;
};

struct st_frc_buf {
	/*cma memory define*/
	u32 cma_mem_size;
	struct page *cma_mem_paddr_pages;
	phys_addr_t cma_mem_paddr_start;
	u32 cma_mem_alloced;
	u32 secured;

	/*frame size*/
	u32 in_hsize;
	u32 in_vsize;

	/*align size*/
	u32 in_align_hsize;
	u32 in_align_vsize;
	u32 me_hsize;
	u32 me_vsize;
	u32 hme_hsize;
	u32 hme_vsize;
	u32 me_blk_hsize;
	u32 me_blk_vsize;
	u32 hme_blk_hsize;
	u32 hme_blk_vsize;
	u32 logo_hsize;
	u32 logo_vsize;

	/*info buffer*/
	u32 lossy_mc_y_info_buf_size;
	u32 lossy_mc_c_info_buf_size;
	u32 lossy_mc_v_info_buf_size;
	u32 lossy_me_x_info_buf_size;

	u32 lossy_mc_y_info_buf_paddr;
	u32 lossy_mc_c_info_buf_paddr;
	u32 lossy_mc_v_info_buf_paddr;
	u32 lossy_me_x_info_buf_paddr;

	/*data buffer*/
	u32 lossy_mc_y_data_buf_size[FRC_TOTAL_BUF_NUM];
	u32 lossy_mc_c_data_buf_size[FRC_TOTAL_BUF_NUM];
	u32 lossy_mc_v_data_buf_size[FRC_TOTAL_BUF_NUM];/*444 mode use*/
	u32 lossy_me_data_buf_size[FRC_TOTAL_BUF_NUM];

	u32 lossy_mc_y_data_buf_paddr[FRC_TOTAL_BUF_NUM];
	u32 lossy_mc_c_data_buf_paddr[FRC_TOTAL_BUF_NUM];
	u32 lossy_mc_v_data_buf_paddr[FRC_TOTAL_BUF_NUM];
	u32 lossy_me_data_buf_paddr[FRC_TOTAL_BUF_NUM];

	/*link buffer*/
	u32 lossy_mc_y_link_buf_size[FRC_TOTAL_BUF_NUM];
	u32 lossy_mc_c_link_buf_size[FRC_TOTAL_BUF_NUM];
	u32 lossy_mc_v_link_buf_size[FRC_TOTAL_BUF_NUM];/*444 mode use*/
	u32 lossy_me_link_buf_size[FRC_TOTAL_BUF_NUM];

	u32 lossy_mc_y_link_buf_paddr[FRC_TOTAL_BUF_NUM];
	u32 lossy_mc_c_link_buf_paddr[FRC_TOTAL_BUF_NUM];
	u32 lossy_mc_v_link_buf_paddr[FRC_TOTAL_BUF_NUM];
	u32 lossy_me_link_buf_paddr[FRC_TOTAL_BUF_NUM];

	/*norm buffer*/
	u32 norm_hme_data_buf_size[FRC_TOTAL_BUF_NUM];
	u32 norm_memv_buf_size[FRC_MEMV_BUF_NUM];
	u32 norm_hmemv_buf_size[FRC_MEMV2_BUF_NUM];
	u32 norm_mevp_out_buf_size[FRC_MEVP_BUF_NUM];

	u32 norm_hme_data_buf_paddr[FRC_TOTAL_BUF_NUM];
	u32 norm_memv_buf_paddr[FRC_MEMV_BUF_NUM];
	u32 norm_hmemv_buf_paddr[FRC_MEMV2_BUF_NUM];
	u32 norm_mevp_out_buf_paddr[FRC_MEVP_BUF_NUM];

	/*logo buffer*/
	u32 norm_iplogo_buf_size[FRC_TOTAL_BUF_NUM];
	u32 norm_logo_irr_buf_size;
	u32 norm_logo_scc_buf_size;
	u32 norm_melogo_buf_size[FRC_TOTAL_BUF_NUM];

	u32 norm_iplogo_buf_paddr[FRC_TOTAL_BUF_NUM];
	u32 norm_logo_irr_buf_paddr;
	u32 norm_logo_scc_buf_paddr;
	u32 norm_melogo_buf_paddr[FRC_TOTAL_BUF_NUM];

	u32 total_size;
	u32 real_total_size;
};

struct st_frc_sts {
	u32 auto_ctrl;
	enum frc_state_e state;
	enum frc_state_e new_state;
	u32 state_transing;
	u32 frame_cnt;
	u32 vs_cnt;
	u32 re_cfg_cnt;
	u32 out_put_mode_changed;
	u32 re_config;
};

struct st_frc_in_sts {
	u32 vf_sts;		/*0:no vframe input, 1:have vframe input, other: unknown*/
	u32 vf_type;		/*vframe type*/
	u32 duration;		/*vf duration*/
	u32 in_hsize;
	u32 in_vsize;
	u32 signal_type;
	u32 source_type;	/*enum vframe_source_type_e*/
	struct vframe_s *vf;
	u32 vf_repeat_cnt;
	u32 vf_null_cnt;

	u32 vs_cnt;
	u32 vs_tsk_cnt;
	u32 vs_duration;
	u64 vs_timestamp;

	u32 have_vf_cnt;
	u32 no_vf_cnt;

	u32 game_mode;
	u32 secure_mode;
};

struct st_frc_out_sts {
	u32 out_framerate;
	u32 vout_height;
	u32 vout_width;

	u32 vs_cnt;
	u32 vs_tsk_cnt;
	u32 vs_duration;
	u64 vs_timestamp;
};

struct tool_debug_s {
	u32 reg_read;
	u32 reg_read_val;
};

struct dbg_dump_tab {
	u8 *name;
	u32 addr;
	u32 start;
	u32 len;
};

enum frc_mtx_e {
	FRC_INPUT_CSC = 1,
	FRC_OUTPUT_CSC,
};

enum frc_mtx_csc_e {
	CSC_OFF = 0,
	RGB_YUV709L,
	RGB_YUV709F,
	YUV709L_RGB,
	YUV709F_RGB,
};

struct crc_parm_s {
	u32 crc_en;
	u32 crc_done_flag;
	u32 crc_data_cmp[2];/*3cmp*/
};

struct frc_crc_data_s {
	u32 frc_crc_read;
	u32 frc_crc_pr;
	struct crc_parm_s me_wr_crc;
	struct crc_parm_s me_rd_crc;
	struct crc_parm_s mc_wr_crc;
};

struct frc_ud_s {
	u32 meud_dbg_en;
	u32 mcud_dbg_en;
};

struct frc_force_size_s {
	u32 force_en;
	u32 force_hsize;
	u32 force_vsize;
};

struct frc_dev_s {
	dev_t devt;
	struct cdev cdev;
	dev_t devno;
	struct device *dev;
	/*power domain for dsp*/
	struct device *pd_dev;
	struct class *clsp;
	struct platform_device	*pdev;

	unsigned int frc_en;		/*0:frc disabled in dts; 1:frc enable in dts*/
	enum eFRC_POS frc_hw_pos;	/*0:before postblend; 1:after postblend*/
	unsigned int frc_test_ptn;
	unsigned int frc_fw_pause;
	u32 probe_ok;
	u32 power_on_flag;
	struct frc_data_s *data;
	void *fw_data;

	int in_irq;
	char in_irq_name[20];
	int out_irq;
	char out_irq_name[20];
	int rdma_irq;
	char rdma_irq_name[20];

	void __iomem *reg;
	void __iomem *clk_reg;
	void __iomem *vpu_reg;
	struct clk *clk_frc;
	u32 clk_frc_Frq;
	struct clk *clk_me;
	u32 clk_me_frq;

	/* vframe check */
	u32 vs_duration;	/*vpu int duration*/
	u64 vs_timestamp;	/*vpu int time stamp*/
	u32 in_out_ratio;

	u32 dbg_force_en;
	u32 dbg_in_out_ratio;
	u32 dbg_input_hsize;
	u32 dbg_input_vsize;
	u32 dbg_reg_monitor_i;
	u32 dbg_in_reg[MONITOR_REG_MAX];
	u32 dbg_reg_monitor_o;
	u32 dbg_out_reg[MONITOR_REG_MAX];
	u32 dbg_buf_len;
	u32 dbg_vf_monitor;

	//u32 loss_en;
	u32 loss_ratio;

	u32 prot_mode;/*0:memc prefetch acorrding mode frame 1:memc prefetch 1 frame*/
	u32 film_mode;
	u32 film_mode_det;/*0: hw detect, 1: sw detect*/

	struct tasklet_struct input_tasklet;
	struct tasklet_struct output_tasklet;

	//struct workqueue_struct *frc_wq;
	//struct work_struct frc_work;

	struct st_frc_sts frc_sts;
	struct st_frc_in_sts in_sts;
	struct st_frc_out_sts out_sts;

	struct st_frc_buf buf;

	struct tool_debug_s tool_dbg;
	struct frc_crc_data_s frc_crc_data;
	struct frc_ud_s ud_dbg;
	struct frc_force_size_s force_size;
};

struct frc_dev_s *get_frc_devp(void);
void get_vout_info(struct frc_dev_s *frc_devp);
struct frc_fw_data_s *get_fw_data(void);
#endif
