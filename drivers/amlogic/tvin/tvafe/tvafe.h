/*******************************************************************
 *  Copyright C 2010 by Amlogic, Inc. All Rights Reserved.
 *  File name: tvafe.h
 *  Description: IO function, structure,
 enum, used in TVIN AFE sub-module processing
 *******************************************************************/

#ifndef _TVAFE_H
#define _TVAFE_H

/* Standard Linux Headers */
#include <linux/cdev.h>
#include <linux/timer.h>
#include <linux/mutex.h>

#include <linux/amlogic/tvin/tvin.h>
#include "../tvin_global.h"
#include "../tvin_frontend.h"
#include "tvafe_general.h"   /* For Kernel used only */
#include "tvafe_adc.h"       /* For Kernel used only */
#include "tvafe_cvd.h"       /* For Kernel used only */

/* ************************************************* */
/* *** macro definitions ********************************************* */
/* *********************************************************** */
#define TVAFE_VER "Ref.2015/06/01b"

/* used to set the flag of tvafe_dev_s */
#define TVAFE_FLAG_DEV_OPENED       0x00000010
#define TVAFE_FLAG_DEV_STARTED      0x00000020
#define TVAFE_FLAG_DEV_SNOW_FLAG    0x00000040
#define TVAFE_POWERDOWN_IN_IDLE     0x00000080

/************************************************************ */
/* *** enum definitions ********************************************* */
/************************************************************ */

/************************************************************* */
/* *** structure definitions ********************************************* */
/************************************************************* */

/* tvafe module structure */
struct tvafe_info_s {
	struct tvin_parm_s          parm;
#if 0
	/* adc calibration data */
	struct tvafe_cal_s          cal;

	/* WSS data */
	struct tvafe_comp_wss_s     comp_wss;    /* WSS data; */

	struct tvafe_adc_s          adc;
#endif
	struct tvafe_cvd2_s         cvd2;
	/*WSS INFO for av/atv*/
	enum tvin_aspect_ratio_e aspect_ratio;
	enum tvin_aspect_ratio_e aspect_ratio_last;
	unsigned int		aspect_ratio_cnt;
};

/* tvafe device structure */
struct tvafe_dev_s {
	int                         index;

	dev_t                       devt;
	struct cdev                 cdev;
	struct device               *dev;


	struct mutex                afe_mutex;
	struct timer_list           timer;

	struct tvin_frontend_s      frontend;
	unsigned int                flags;
	/* bit4: TVAFE_FLAG_DEV_OPENED */
	/* bit5: TVAFE_FLAG_DEV_STARTED */
	struct tvafe_pin_mux_s      *pinmux;
	/* pin mux setting from board config */
	/* cvd2 memory */
	struct tvafe_cvd2_mem_s     mem;

	struct tvafe_info_s         tvafe;
	unsigned int			cma_config_en;
	/*cma_config_flag:1:share with codec_mm;0:cma alone*/
	unsigned int			cma_config_flag;
#ifdef CONFIG_CMA
	struct platform_device	*this_pdev;
	struct page			*venc_pages;
	unsigned int			cma_mem_size;/* BYTE */
	unsigned int			cma_mem_alloc;
#endif

};

typedef int (*hook_func_t)(void);
extern void aml_fe_hook_cvd(hook_func_t atv_mode,
		hook_func_t cvd_hv_lock, hook_func_t get_fmt);
extern int tvafe_reg_read(unsigned int reg, unsigned int *val);
extern int tvafe_reg_write(unsigned int reg, unsigned int val);
extern int tvafe_hiu_reg_read(unsigned int reg, unsigned int *val);
extern int tvafe_hiu_reg_write(unsigned int reg, unsigned int val);

#endif  /* _TVAFE_H */

