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

#define _VE_LDIM  'C'

/* VPP.ldim IOCTL command list */
#define LDIM_IOC_PARA  _IOW(_VE_LDIM, 0x50, struct ldim_param_s)

enum ldim_dev_type_e {
	LDIM_DEV_TYPE_NORMAL = 0,
	LDIM_DEV_TYPE_SPI,
	LDIM_DEV_TYPE_I2C,
	LDIM_DEV_TYPE_MAX,
};

struct ldim_config_s {
	unsigned short hsize;
	unsigned short vsize;
	unsigned char hist_row;
	unsigned char hist_col;
	unsigned char bl_mode;
	unsigned char bl_en;
	unsigned char hvcnt_bypass;
	unsigned char dev_index;
};

struct ldim_param_s {
	/* beam model */
	int rgb_base;
	int boost_gain;
	int lpf_res;
	int fw_ld_th_sf; /* spatial filter threshold */
	/* beam curve */
	int ld_vgain;
	int ld_hgain;
	int ld_litgain;
	int ld_lut_vdg_lext;
	int ld_lut_hdg_lext;
	int ld_lut_vhk_lext;
	int ld_lut_hdg[32];
	int ld_lut_vdg[32];
	int ld_lut_vhk[32];
	/* beam shape minor adjustment */
	int ld_lut_vhk_pos[32];
	int ld_lut_vhk_neg[32];
	int ld_lut_hhk[32];
	int ld_lut_vho_pos[32];
	int ld_lut_vho_neg[32];
	/* remapping */
	int lit_idx_th;
	int comp_gain;
};

#define LDIM_INIT_ON_MAX     300
#define LDIM_INIT_OFF_MAX    20
struct ldim_dev_config_s {
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
	unsigned char device_count;
	unsigned char pinmux_flag;

	unsigned int blk_row;
	unsigned int blk_col;
	unsigned int dim_min;
	unsigned int dim_max;

	unsigned char init_loaded;
	unsigned char cmd_size;
	unsigned char *init_on;
	unsigned char *init_off;
	unsigned int init_on_cnt;
	unsigned int init_off_cnt;

	struct bl_pwm_config_s ldim_pwm_config;
	struct bl_pwm_config_s analog_pwm_config;

	unsigned short bl_zone_num;
	unsigned short bl_mapping[LD_BLKREGNUM];

	void (*dim_range_update)(void);
	int (*dev_reg_write)(unsigned int dev_id, unsigned char *buf,
			     unsigned int len);
	int (*dev_reg_read)(unsigned int dev_id, unsigned char *buf,
			    unsigned int len);

	struct pinctrl *pin;
	struct device *dev;
	struct class *dev_class;
	struct spi_device *spi_dev;
	struct spi_board_info *spi_info;
};

struct ldim_rmem_s {
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
	unsigned char func_bypass;  /* for lcd bist pattern */
	unsigned char brightness_bypass;
	unsigned char test_en;
	unsigned char avg_update_en;
	unsigned char matrix_update_en;
	unsigned char alg_en;
	unsigned char top_en;
	unsigned char hist_en;
	unsigned char load_db_en;
	unsigned char db_print_flag;

	unsigned int data_min;
	unsigned int data_max;
	unsigned int brightness_level;
	unsigned int litgain;
	unsigned int irq_cnt;

	struct ldim_config_s *conf;
	struct ldim_dev_config_s *ldev_conf;
	struct ldim_rmem_s *rmem;
	unsigned int *hist_matrix;
	unsigned int *max_rgb;
	unsigned short *test_matrix;
	unsigned short *local_ldim_matrix;
	unsigned short *ldim_matrix_buf;
	struct ldim_fw_para_s *fw_para;
	int (*init)(void);
	int (*power_on)(void);
	int (*power_off)(void);
	int (*set_level)(unsigned int level);
	int (*pinmux_ctrl)(int status);
	int (*pwm_vs_update)(void);
	int (*device_power_on)(void);
	int (*device_power_off)(void);
	int (*device_bri_update)(unsigned short *buf, unsigned char len);
	int (*device_bri_check)(void);
	void (*config_print)(void);
	void (*test_ctrl)(int flag);
};

struct aml_ldim_driver_s *aml_ldim_get_driver(void);

int aml_ldim_get_config_dts(struct device_node *child);
int aml_ldim_get_config_unifykey(unsigned char *buf);
int aml_ldim_probe(struct platform_device *pdev);
int aml_ldim_remove(void);

#endif

