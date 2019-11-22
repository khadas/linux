/*
 * drivers/amlogic/media/vin/tvin/tvafe/tvafe.h
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

#ifndef _TVAFE_H
#define _TVAFE_H

/* Standard Linux Headers */
#include <linux/cdev.h>
#include <linux/timer.h>
#include <linux/mutex.h>

#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include "../tvin_global.h"
#include "../tvin_frontend.h"
#include "tvafe_general.h"   /* For Kernel used only */
#include "tvafe_cvd.h"       /* For Kernel used only */

/* ************************************************* */
/* *** macro definitions ********************************************* */
/* *********************************************************** */
#define TVAFE_VER "Ref.2019/08/08"

/* used to set the flag of tvafe_dev_s */
#define TVAFE_FLAG_DEV_OPENED 0x00000010
#define TVAFE_FLAG_DEV_STARTED 0x00000020
#define TVAFE_FLAG_DEV_SNOW_FLAG 0x00000040
#define TVAFE_POWERDOWN_IN_IDLE 0x00000080

/*used to flag port opend for avdetect config*/
#define TVAFE_PORT_AV1 0x1
#define TVAFE_PORT_AV2 0x2

/************************************************************ */
/* *** enum definitions ********************************************* */
/************************************************************ */

/************************************************************* */
/* *** structure definitions ********************************************* */
/************************************************************* */
/* tvafe module structure */
struct tvafe_info_s {
	struct tvin_parm_s parm;
	struct tvafe_cvd2_s cvd2;
	/*WSS INFO for av/atv*/
	enum tvin_aspect_ratio_e aspect_ratio;
	unsigned int aspect_ratio_cnt;
};

#define TVAFE_AUTO_CDTO    (1 << 0)
#define TVAFE_AUTO_HS      (1 << 1)
#define TVAFE_AUTO_VS      (1 << 2)
#define TVAFE_AUTO_DE      (1 << 3)
#define TVAFE_AUTO_3DCOMB  (1 << 4)
#define TVAFE_AUTO_PGA     (1 << 5)

struct tvafe_user_param_s {
	unsigned int cutwindow_val_h[5];
	unsigned int cutwindow_val_v[5];
	unsigned int cutwindow_val_vs_ve;
	unsigned int auto_adj_en;
	unsigned int vline_chk_cnt;
	unsigned int nostd_vs_th;
	unsigned int nostd_no_vs_th;
	unsigned int nostd_vs_cntl;
	unsigned int nostd_vloop_tc;
	unsigned int force_vs_th_flag;
	unsigned int nostd_stable_cnt;
	unsigned int nostd_dmd_clp_step;
	unsigned int skip_vf_num;
	unsigned int avout_en;
};

/* tvafe device structure */
struct tvafe_dev_s {
	int	index;
	dev_t devt;
	struct cdev cdev;
	struct device *dev;

	struct mutex afe_mutex;
	struct timer_list timer;

	struct tvin_frontend_s frontend;
	unsigned int flags;
	/* bit4: TVAFE_FLAG_DEV_OPENED */
	/* bit5: TVAFE_FLAG_DEV_STARTED */
	struct tvafe_pin_mux_s *pinmux;
	/* pin mux setting from board config */
	/* cvd2 memory */
	struct tvafe_cvd2_mem_s mem;

	struct tvafe_info_s tvafe;

	struct tvafe_reg_table_s **pq_conf;

	unsigned int cma_config_en;
	/*cma_config_flag:1:share with codec_mm;0:cma alone*/
	unsigned int cma_config_flag;
#ifdef CONFIG_CMA
	struct platform_device *this_pdev;
	struct page *venc_pages;
	unsigned int cma_mem_size;/* BYTE */
	unsigned int cma_mem_alloc;
#endif
	unsigned int frame_skip_enable;
	unsigned int sizeof_tvafe_dev_s;
};

bool tvafe_get_snow_cfg(void);
void tvafe_set_snow_cfg(bool cfg);

struct tvafe_user_param_s *tvafe_get_user_param(void);
struct tvafe_dev_s *tvafe_get_dev(void);

typedef int (*hook_func_t)(void);
typedef int (*hook_func1_t)(bool);
extern void aml_fe_hook_cvd(hook_func_t atv_mode,
		hook_func_t cvd_hv_lock, hook_func_t get_fmt,
		hook_func1_t set_mode);
extern int tvafe_reg_read(unsigned int reg, unsigned int *val);
extern int tvafe_reg_write(unsigned int reg, unsigned int val);
extern int tvafe_vbi_reg_read(unsigned int reg, unsigned int *val);
extern int tvafe_vbi_reg_write(unsigned int reg, unsigned int val);
extern int tvafe_hiu_reg_read(unsigned int reg, unsigned int *val);
extern int tvafe_hiu_reg_write(unsigned int reg, unsigned int val);
extern int tvafe_device_create_file(struct device *dev);
extern void tvafe_remove_device_files(struct device *dev);
int tvafe_pq_config_probe(struct meson_tvafe_data *tvafe_data);
void cvd_set_shift_cnt(enum tvafe_cvd2_shift_cnt_e src, unsigned int val);
unsigned int cvd_get_shift_cnt(enum tvafe_cvd2_shift_cnt_e src);

extern bool disableapi;
extern bool force_stable;

extern unsigned int force_nostd;

#define TVAFE_DBG_NORMAL     (1 << 0)
#define TVAFE_DBG_ISR        (1 << 4)
#define TVAFE_DBG_SMR        (1 << 8)
#define TVAFE_DBG_SMR2       (1 << 9)
#define TVAFE_DBG_NOSTD      (1 << 12)
#define TVAFE_DBG_NOSTD2     (1 << 13)
extern unsigned int tvafe_dbg_print;

#endif  /* _TVAFE_H */

