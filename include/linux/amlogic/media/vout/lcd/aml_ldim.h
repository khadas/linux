/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef _INC_AML_LDIM_H_
#define _INC_AML_LDIM_H_
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/media/vout/lcd/aml_lcd.h>
#include <linux/amlogic/media/vout/lcd/ldim_fw.h>
#include <linux/spi/spi.h>

/*#define LDIM_DEBUG_INFO*/
#define LDIMPR(fmt, args...)     pr_info("ldim: " fmt "", ## args)
#define LDIMERR(fmt, args...)    pr_err("ldim: error: " fmt "", ## args)

#define LD_DATA_DEPTH   12
#define LD_DATA_MIN     10
#define LD_DATA_MAX     0xfff

/* **********************************
 * IOCTL define
 * **********************************
 */
#define _VE_LDIM  'C'
#define AML_LDIM_IOC_NR_GET_INFO	0x51
#define AML_LDIM_IOC_NR_SET_INFO	0x52
#define AML_LDIM_IOC_NR_GET_INFO_NEW	0x53
#define AML_LDIM_IOC_NR_SET_INFO_NEW	0x54

#define AML_LDIM_IOC_CMD_GET_INFO \
	_IOR(_VE_LDIM, AML_LDIM_IOC_NR_GET_INFO, struct aml_ldim_info_s)
#define AML_LDIM_IOC_CMD_SET_INFO \
	_IOW(_VE_LDIM, AML_LDIM_IOC_NR_SET_INFO, struct aml_ldim_info_s)
#define AML_LDIM_IOC_CMD_GET_INFO_NEW \
	_IOR(_VE_LDIM, AML_LDIM_IOC_NR_GET_INFO_NEW, struct aml_ldim_pq_s)
#define AML_LDIM_IOC_CMD_SET_INFO_NEW \
	_IOW(_VE_LDIM, AML_LDIM_IOC_NR_SET_INFO_NEW, struct aml_ldim_pq_s)

enum ldim_dev_type_e {
	LDIM_DEV_TYPE_NORMAL = 0,
	LDIM_DEV_TYPE_SPI,
	LDIM_DEV_TYPE_I2C,
	LDIM_DEV_TYPE_MAX,
};

struct aml_ldim_driver_s;

struct aml_ldim_info_s {
	unsigned int func_en;
	unsigned int remapping_en;
	unsigned int alpha;
	unsigned int lpf_method;
	unsigned int lpf_gain;
	unsigned int lpf_res;
	unsigned int side_blk_diff_th;
	unsigned int bbd_th;
	unsigned int boost_gain;
	unsigned int rgb_base;
	unsigned int ld_remap_bypass;
	unsigned int ld_tf_step_th;
	unsigned int tf_blk_fresh_bl;
	unsigned int tf_fresh_bl;
	unsigned int fw_ld_thtf_l;
	unsigned int fw_rgb_diff_th;
	unsigned int fw_ld_thist;
	unsigned int bl_remap_curve[17];
	unsigned int reg_ld_remap_lut[16][32];
	unsigned int fw_ld_whist[16];
};

struct aml_ldim_pq_s {
	unsigned int func_en;
	unsigned int remapping_en;

	/* switch fw, use for custom fw. 0=aml_hw_fw, 1=aml_sw_fw */
	unsigned int fw_sel;

	/* fw parameters */
	unsigned int ldc_hist_mode;
	unsigned int ldc_hist_blend_mode;
	unsigned int ldc_hist_blend_alpha;
	unsigned int ldc_hist_adap_blend_max_gain;
	unsigned int ldc_hist_adap_blend_diff_th1;
	unsigned int ldc_hist_adap_blend_diff_th2;
	unsigned int ldc_hist_adap_blend_th0;
	unsigned int ldc_hist_adap_blend_thn;
	unsigned int ldc_hist_adap_blend_gain_0;
	unsigned int ldc_hist_adap_blend_gain_1;
	unsigned int ldc_init_bl_min;
	unsigned int ldc_init_bl_max;

	unsigned int ldc_sf_mode;
	unsigned int ldc_sf_gain_up;
	unsigned int ldc_sf_gain_dn;
	unsigned int ldc_sf_tsf_3x3;
	unsigned int ldc_sf_tsf_5x5;

	unsigned int ldc_bs_bl_mode;
	//unsigned int ldc_glb_apl; //read only
	unsigned int ldc_bs_glb_apl_gain;
	unsigned int ldc_bs_dark_scene_bl_th;
	unsigned int ldc_bs_gain;
	unsigned int ldc_bs_limit_gain;
	unsigned int ldc_bs_loc_apl_gain;
	unsigned int ldc_bs_loc_max_min_gain;
	unsigned int ldc_bs_loc_dark_scene_bl_th;

	unsigned int ldc_tf_en;
	//unsigned int ldc_tf_sc_flag; //read only
	unsigned int ldc_tf_low_alpha;
	unsigned int ldc_tf_high_alpha;
	unsigned int ldc_tf_low_alpha_sc;
	unsigned int ldc_tf_high_alpha_sc;

	unsigned int ldc_dimming_curve_en;
	unsigned int ldc_sc_hist_diff_th;
	unsigned int ldc_sc_apl_diff_th;
	unsigned int bl_remap_curve[17];

	/* comp parameters */
	unsigned int ldc_bl_buf_diff;
	unsigned int ldc_glb_gain;
	unsigned int ldc_dth_en;
	unsigned int ldc_dth_bw;
	unsigned int ldc_gain_lut[16][64];
	unsigned int ldc_min_gain_lut[64];
	unsigned int ldc_dither_lut[32][16];
};

struct ldim_comp_s {
	unsigned int ldc_comp_en;
	unsigned int ldc_bl_buf_diff;
	unsigned int ldc_glb_gain;
	unsigned int ldc_dth_en;
	unsigned int ldc_dth_bw;
};

struct ldim_config_s {
	unsigned short hsize;
	unsigned short vsize;
	unsigned char seg_row;
	unsigned char seg_col;
	unsigned char bl_mode;
	unsigned char func_en;
	unsigned char remap_en;
	unsigned char demo_en;
	unsigned char hvcnt_bypass;
	unsigned char dev_index;
	struct aml_ldim_info_s *ldim_info;
};

struct ldim_profile_s {
	unsigned int mode;
	unsigned int ld_lut_hdg[32];
	unsigned int ld_lut_vdg[32];
	unsigned int ld_lut_vhk[32];
	unsigned int profile_k;
	unsigned int profile_bits;
	char file_path[256];
};

struct ldim_rmem_s {
	unsigned char flag;
	unsigned int hist_fid;
	unsigned int duty_fid;

	void *wr_mem_vaddr1;
	phys_addr_t wr_mem_paddr1;
	void *wr_mem_vaddr2;
	phys_addr_t wr_mem_paddr2;
	void *rd_mem_vaddr1;
	phys_addr_t rd_mem_paddr1;
	void *rd_mem_vaddr2;
	phys_addr_t rd_mem_paddr2;
	unsigned int wr_mem_size;
	unsigned int rd_mem_size;

	/* for new ldc */
	void *rsv_mem_vaddr;
	phys_addr_t rsv_mem_paddr;
	unsigned int rsv_mem_size;

	phys_addr_t profile_mem_paddr;
	unsigned int profile_mem_size;

	unsigned char *global_hist_vaddr;
	phys_addr_t global_hist_paddr;
	unsigned int global_hist_mem_size;
	unsigned int global_hist_highmem_flag;

	unsigned char *seg_hist_vaddr;
	phys_addr_t seg_hist_paddr;
	unsigned int seg_hist_mem_size;
	unsigned int seg_hist_highmem_flag;

	unsigned char *duty_vaddr;
	phys_addr_t duty_paddr;
	unsigned int duty_mem_size;
	unsigned int duty_highmem_flag;
};

#define LDIM_DEV_NAME_MAX    30
#define LDIM_INIT_ON_MAX     1000
#define LDIM_INIT_OFF_MAX    24
struct ldim_dev_driver_s {
	unsigned char index;
	char name[LDIM_DEV_NAME_MAX];
	char pinmux_name[LDIM_DEV_NAME_MAX];
	unsigned char key_valid;
	unsigned char type;
	unsigned int dma_support;
	int cs_hold_delay;
	int cs_clk_delay;
	int en_gpio;
	int en_gpio_on;
	int en_gpio_off;
	int lamp_err_gpio;
	unsigned char fault_check;
	unsigned char write_check;
	unsigned char pinmux_flag;
	unsigned char chip_cnt;

	unsigned int bl_row;
	unsigned int bl_col;
	unsigned int dim_min;
	unsigned int dim_max;

	unsigned int zone_num;
	unsigned short *bl_mapping;
	struct ldim_profile_s *bl_profile;

	unsigned char init_loaded;
	unsigned char cmd_size;
	unsigned char *init_on;
	unsigned char *init_off;
	unsigned int init_on_cnt;
	unsigned int init_off_cnt;

	struct bl_pwm_config_s ldim_pwm_config;
	struct bl_pwm_config_s analog_pwm_config;

	struct pinctrl *pin;
	struct device *dev;
	struct class *class;
	struct spi_device *spi_dev;
	struct spi_board_info *spi_info;

	void (*dim_range_update)(struct ldim_dev_driver_s *dev_drv);
	int (*pinmux_ctrl)(struct ldim_dev_driver_s *dev_drv, int status);
	int (*reg_write)(struct ldim_dev_driver_s *dev_drv, unsigned char chip_id,
			 unsigned char reg, unsigned char *buf, unsigned int len);
	int (*reg_read)(struct ldim_dev_driver_s *dev_drv, unsigned char chip_id,
			unsigned char reg, unsigned char *buf, unsigned int len);

	/*device api*/
	int (*power_on)(struct aml_ldim_driver_s *ldim_drv);
	int (*power_off)(struct aml_ldim_driver_s *ldim_drv);
	int (*dev_smr)(struct aml_ldim_driver_s *ldim_drv,
		       unsigned int *buf, unsigned int len);
	int (*dev_smr_dummy)(struct aml_ldim_driver_s *ldim_drv);
	int (*dev_err_handler)(struct aml_ldim_driver_s *ldim_drv);
	int (*pwm_vs_update)(struct aml_ldim_driver_s *ldim_drv);
	void (*config_print)(struct aml_ldim_driver_s *ldim_drv);
	int (*config_update)(struct aml_ldim_driver_s *ldim_drv);
};

struct ldim_drv_data_s {
	unsigned short h_zone_max;
	unsigned short v_zone_max;
	unsigned short total_zone_max;
	unsigned int wr_mem_size;
	unsigned int rd_mem_size;

	int (*alloc_rmem)(void);
	void (*remap_update)(struct ld_reg_s *nprm,
			     unsigned int avg_update_en,
			     unsigned int matrix_update_en);
	void (*stts_init)(unsigned int pic_h, unsigned int pic_v,
			  unsigned int blk_vnum, unsigned int blk_hnum);
	void (*remap_init)(struct ld_reg_s *nprm,
			   unsigned int bl_en, unsigned int hvcnt_bypass);
	void (*vs_arithmetic)(struct aml_ldim_driver_s *ldim_drv);

	int (*memory_init)(struct platform_device *pdev,
			   struct ldim_drv_data_s *data,
			   unsigned int row, unsigned int col);
	void (*drv_init)(struct aml_ldim_driver_s *ldim_drv);
	void (*func_ctrl)(struct aml_ldim_driver_s *ldim_drv, int flag);
	void (*remap_lut_update)(void);
};

/*******global API******/
#define LDIM_STATE_POWER_ON             BIT(0)
#define LDIM_STATE_FUNC_EN              BIT(1)
#define LDIM_STATE_REMAP_EN             BIT(2)
#define LDIM_STATE_REMAP_FORCE_UPDATE   BIT(3)
#define LDIM_STATE_LD_EN                BIT(4)

struct aml_ldim_driver_s {
	unsigned char valid_flag;
	unsigned char static_pic_flag;
	unsigned char vsync_change_flag;
	unsigned char duty_update_flag;

	unsigned char init_on_flag;
	unsigned char func_en;
	unsigned char remap_en;
	unsigned char demo_en;
	unsigned char black_frm_en;
	unsigned char ld_sel;  /* for gd bypass */
	unsigned char func_bypass;  /* for lcd bist pattern */
	unsigned char dev_smr_bypass;
	unsigned char brightness_bypass;
	unsigned char test_bl_en;
	unsigned char test_remap_en;
	unsigned char avg_update_en;
	unsigned char matrix_update_en;
	unsigned char alg_en;
	unsigned char top_en;
	unsigned char hist_en;
	unsigned char load_db_en;
	unsigned char db_print_flag;
	unsigned char level_update;

	unsigned int state;
	unsigned int data_min;
	unsigned int data_max;
	unsigned int brightness_level;
	unsigned int litgain;
	unsigned int dbg_vs_cnt;
	unsigned int irq_cnt;
	unsigned long long arithmetic_time[10];
	unsigned long long xfer_time[10];

	struct ldim_drv_data_s *data;
	struct ldim_config_s *conf;
	struct ldim_dev_driver_s *dev_drv;
	struct ldim_rmem_s *rmem;

	struct ldim_stts_s *stts;
	struct ldim_fw_s *fw;
	struct ldim_comp_s *comp;
	struct ldim_fw_custom_s *fw_cus_pre;
	struct ldim_fw_custom_s *fw_cus_post;

	unsigned int *test_matrix;
	unsigned int *local_bl_matrix;
	unsigned int *bl_matrix_cur;
	unsigned int *bl_matrix_pre;

	/* driver api */
	int (*init)(void);
	int (*power_on)(void);
	int (*power_off)(void);
	int (*set_level)(unsigned int level);
	void (*test_ctrl)(int flag);
	void (*ld_sel_ctrl)(int flag);
	void (*pwm_vs_update)(void);
	void (*config_print)(void);
};

struct aml_ldim_driver_s *aml_ldim_get_driver(void);

int aml_ldim_get_config_dts(struct device_node *child);
int aml_ldim_get_config_unifykey(unsigned char *buf);
int aml_ldim_probe(struct platform_device *pdev);
int aml_ldim_remove(void);

#endif

