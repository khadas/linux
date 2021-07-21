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
#include <linux/amlogic/media/vout/lcd/ldim_alg.h>
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

#define AML_LDIM_IOC_CMD_GET_INFO \
	_IOR(_VE_LDIM, AML_LDIM_IOC_NR_GET_INFO, struct aml_ldim_info_s)
#define AML_LDIM_IOC_CMD_SET_INFO \
	_IOW(_VE_LDIM, AML_LDIM_IOC_NR_SET_INFO, struct aml_ldim_info_s)

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
	unsigned int bl_remap_curve[16];
	unsigned int reg_ld_remap_lut[16][32];
	unsigned int fw_ld_whist[16];
};

struct ldim_config_s {
	unsigned short hsize;
	unsigned short vsize;
	unsigned char hist_row;
	unsigned char hist_col;
	unsigned char bl_mode;
	unsigned char func_en;
	unsigned char remap_en;
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

struct ldim_seg_hist_s {
	unsigned int weight_avg;
	unsigned int weight_avg_95;
	unsigned int max_index;
	unsigned int min_index;
};

#define LDIM_INIT_ON_MAX     300
#define LDIM_INIT_OFF_MAX    20
struct ldim_dev_driver_s {
	unsigned char index;
	char name[20];
	char pinmux_name[20];
	unsigned char type;
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
		       unsigned short *buf, unsigned char len);
	int (*dev_smr_dummy)(struct aml_ldim_driver_s *ldim_drv);
	int (*pwm_vs_update)(struct aml_ldim_driver_s *ldim_drv);
	void (*config_print)(struct aml_ldim_driver_s *ldim_drv);
};

struct ldim_drv_data_s {
	unsigned short h_region_max;
	unsigned short v_region_max;
	unsigned short total_region_max;
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
	void (*min_gain_lut_update)(void);
};

/*******global API******/
struct aml_ldim_driver_s {
	unsigned char valid_flag;
	unsigned char static_pic_flag;
	unsigned char vsync_change_flag;

	unsigned char init_on_flag;
	unsigned char func_en;
	unsigned char remap_en;
	unsigned char demo_en;
	unsigned char black_frm_en;
	unsigned char func_bypass;  /* for lcd bist pattern */
	unsigned char dev_smr_bypass;
	unsigned char brightness_bypass;
	unsigned char test_en;
	unsigned char avg_update_en;
	unsigned char matrix_update_en;
	unsigned char alg_en;
	unsigned char top_en;
	unsigned char hist_en;
	unsigned char load_db_en;
	unsigned char db_print_flag;
	unsigned char level_update;

	unsigned int data_min;
	unsigned int data_max;
	unsigned int brightness_level;
	unsigned int litgain;
	unsigned int irq_cnt;

	struct ldim_drv_data_s *data;
	struct ldim_config_s *conf;
	struct ldim_dev_driver_s *dev_drv;
	struct ldim_rmem_s *rmem;
	unsigned int *global_hist;
	struct ldim_seg_hist_s *seg_hist;
	unsigned int *duty;
	unsigned int *hist_matrix;
	unsigned int *max_rgb;
	unsigned short *test_matrix;
	unsigned short *local_bl_matrix;
	unsigned short *bl_matrix_cur;
	unsigned short *bl_matrix_pre;
	struct ldim_fw_para_s *fw_para;

	/* driver api */
	int (*init)(void);
	int (*power_on)(void);
	int (*power_off)(void);
	int (*set_level)(unsigned int level);
	void (*test_ctrl)(int flag);
	void (*pwm_vs_update)(void);
	void (*config_print)(void);
};

struct aml_ldim_driver_s *aml_ldim_get_driver(void);

int aml_ldim_get_config_dts(struct device_node *child);
int aml_ldim_get_config_unifykey(unsigned char *buf);
int aml_ldim_probe(struct platform_device *pdev);
int aml_ldim_remove(void);

#endif

