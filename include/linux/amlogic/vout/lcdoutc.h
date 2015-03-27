/*
 * AMLOGIC lcd controller driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 *
 * Author:  Tim Yao <timyao@amlogic.com>
 *
 * Modify:  Evoke Zhang <evoke.zhang@amlogic.com>
 *
 */

#ifndef LCDOUTC_H
#define LCDOUTC_H
#include <linux/types.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>

/* **********************************
 * debug print define
 * ********************************** */
/* #define LCD_DEBUG_INFO */

extern unsigned int lcd_print_flag;
extern void lcd_print(const char *fmt, ...);

/* **********************************
 * clk parameter bit define
 * pll_ctrl, div_ctrl, clk_ctrl
 * ********************************** */
/* ******** pll_ctrl ******** */
/* [17:16] */
#define PLL_CTRL_OD                 16
/* [13:9] */
#define PLL_CTRL_N                  9
/* [8:0] */
#define PLL_CTRL_M                  0

/* ******** div_ctrl ******** */
/* [26:24] */
#define DIV_CTRL_EDP_DIV1           24
/* [23:20] */
#define DIV_CTRL_EDP_DIV0           20
/* [14:12] */
#define DIV_CTRL_DIV_POST           12
#define DIV_CTRL_LVDS_CLK_EN        11
/*#define DIV_CTRL_PHY_CLK_DIV2       10*/
/* [9:8] */
#define DIV_CTRL_POST_SEL           8
/* [6:4] */
#define DIV_CTRL_DIV_PRE            4

/* ******** clk_ctrl ******** */
#define CLK_CTRL_AUTO               31
/* #define CLK_CTRL_VCLK_SEL         30 */
/* #define CLK_CTRL_DIV_SEL          29 */
/* #define CLK_CTRL_PLL_SEL          28 */
/* [27:16] */
#define CLK_CTRL_FRAC               16
/* [14:12] */
#define CLK_CTRL_LEVEL              12
/* [11:8] */
#define CLK_CTRL_SS                 8
/* [7:0] */
#define CLK_CTRL_XD                 0

/* **********************************
 * other parameter bit define
 * pol_ctrl, gamma_ctrl
 * ********************************** */
/* pol_ctrl */
#define POL_CTRL_CLK                6
#define POL_CTRL_DE                 2
#define POL_CTRL_VS                 1
#define POL_CTRL_HS                 0

/* gamma_ctrl */
#define GAMMA_CTRL_REV              4
#define GAMMA_CTRL_EN               0

/* **********************************
 * global control define
 * ********************************** */
enum LCD_Chip_e {
	LCD_CHIP_M6 = 0,
	LCD_CHIP_M8,
	LCD_CHIP_M8B,
	LCD_CHIP_M8M2,
	LCD_CHIP_MAX,
};

enum Lcd_Type_e {
	LCD_DIGITAL_MIPI = 0,
	LCD_DIGITAL_LVDS,
	LCD_DIGITAL_EDP,
	LCD_DIGITAL_TTL,
	LCD_DIGITAL_MINILVDS,
	LCD_TYPE_MAX,
};
extern const char *lcd_type_table[];

struct Lcd_Basic_s {
	char *model_name;
	u16 h_active;    /* Horizontal display area */
	u16 v_active;    /* Vertical display area */
	u16 h_period;    /* Horizontal total period time */
	u16 v_period;    /* Vertical total period time */
	u32 screen_ratio_width;  /* screen aspect ratio width */
	u32 screen_ratio_height; /* screen aspect ratio height */
	u32 h_active_area;       /* screen physical width in "mm" unit */
	u32 v_active_area;       /* screen physical height in "mm" unit */

	enum Lcd_Type_e lcd_type;
	u16 lcd_bits;            /* 6 or 8 bits */
	/* option=0, means the panel only support one lcd_bits option */
	u16 lcd_bits_option;
};

struct Lcd_Timing_s {
	u32 pll_ctrl; /* video PLL settings */
	u32 div_ctrl; /* video pll div settings */
	/* [31]clk_auto, [11:8]ss_ctrl, [7:0]xd */
	u32 clk_ctrl; /* video clock settings */
	u32 lcd_clk;  /* lcd pixel clock */
	u16 sync_duration_num;
	u16 sync_duration_den;

	u16 pol_ctrl;
	/* u16 inv_cnt_addr; */
	/* u16 tcon_misc_sel_addr; */

	u16 video_on_pixel;
	u16 video_on_line;

	u16 hsync_width;
	u16 hsync_bp;
	u16 vsync_width;
	u16 vsync_bp;
	u32 vsync_h_phase; /* [31]sign, [15:0]value */
	u16 hvsync_valid;
	u16 de_valid;
	u32 h_offset;
	u32 v_offset;

	u16 de_hs_addr;
	u16 de_he_addr;
	u16 de_vs_addr;
	u16 de_ve_addr;

	u16 hs_hs_addr;
	u16 hs_he_addr;
	u16 hs_vs_addr;
	u16 hs_ve_addr;

	u16 vs_hs_addr;
	u16 vs_he_addr;
	u16 vs_vs_addr;
	u16 vs_ve_addr;

	u16 vso_hstart;
	u16 vso_vstart;
	u16 vso_user;
};

/* Fine Effect Tune */
struct Lcd_Effect_s {
	u32 rgb_base_addr;
	u32 rgb_coeff_addr;
	unsigned char dith_user;
	u32 dith_cntl_addr;

	u32 vadj_brightness;
	u32 vadj_contrast;
	u32 vadj_saturation;

	unsigned char gamma_ctrl;
	u16 gamma_r_coeff;
	u16 gamma_g_coeff;
	u16 gamma_b_coeff;
	u16 GammaTableR[256];
	u16 GammaTableG[256];
	u16 GammaTableB[256];
	void (*set_gamma_table)(unsigned int gamma_en);
	void (*gamma_test)(unsigned int num);
};

/* mipi-dsi config */
/* byte[1] */
#define DSI_CMD_INDEX             1

#define DSI_INIT_ON_MAX           100
#define DSI_INIT_OFF_MAX          30

#define BIT_OP_MODE_INIT          0
#define BIT_OP_MODE_DISP          4
#define BIT_TRANS_CTRL_CLK        0
/* [5:4] */
#define BIT_TRANS_CTRL_SWITCH     4
struct DSI_Config_s {
	unsigned char lane_num;
	unsigned int bit_rate_max; /* MHz */
	unsigned int bit_rate_min; /* MHz*/
	unsigned int bit_rate; /* Hz */
	unsigned int factor_denominator;
	unsigned int factor_numerator;

	unsigned int venc_data_width;
	unsigned int dpi_data_format;
	unsigned int venc_fmt;
	/* mipi-dsi operation mode: video, command. [4]display , [0]init */
	unsigned int operation_mode;
	/* [0]LP mode auto stop clk lane, [5:4]phy switch between LP and HS */
	unsigned int transfer_ctrl;
	/* burst, non-burst(sync pulse, sync event) */
	unsigned char video_mode_type;

	unsigned char *dsi_init_on;
	unsigned char *dsi_init_off;
	unsigned char lcd_extern_init;
};

struct EDP_Config_s {
	unsigned char max_lane_count;
	unsigned char link_user;
	unsigned char lane_count;
	unsigned char link_rate;
	unsigned char link_adaptive;
	unsigned char vswing;
	unsigned char preemphasis;
	unsigned int bit_rate;
	unsigned int sync_clock_mode;
	unsigned char edid_timing_used;
};

struct LVDS_Config_s {
	unsigned int lvds_vswing;
	unsigned int lvds_repack_user;
	unsigned int lvds_repack;
	unsigned int pn_swap;
};

struct TTL_Config_s {
	unsigned char rb_swap;
	unsigned char bit_swap;
};

struct MLVDS_Tcon_Config_s {
	int channel_num;
	int hv_sel;
	int tcon_1st_hs_addr;
	int tcon_1st_he_addr;
	int tcon_1st_vs_addr;
	int tcon_1st_ve_addr;
	int tcon_2nd_hs_addr;
	int tcon_2nd_he_addr;
	int tcon_2nd_vs_addr;
	int tcon_2nd_ve_addr;
};

struct MLVDS_Config_s {
	int mlvds_insert_start;
	int total_line_clk;
	int test_dual_gate;
	int test_pair_num;
	int phase_select;
	int TL080_phase;
	int scan_function;
};

struct Lcd_Control_Config_s {
	struct DSI_Config_s *mipi_config;
	struct EDP_Config_s *edp_config;
	struct LVDS_Config_s *lvds_config;
	struct TTL_Config_s *ttl_config;
	struct MLVDS_Config_s *mlvds_config;
	struct MLVDS_Tcon_Config_s *mlvds_tcon_config;
};

enum Bool_state_e {
	OFF = 0,
	ON = 1,
};

/* Power Control */
#define LCD_PWR_CTRL_STEP_MAX         15
struct Lcd_CPU_GPIO_s {
	char name[15];
	struct gpio_desc *desc;
};

struct Lcd_Power_Config_s {
	unsigned char type;
	int gpio; /* for cpu gpio, it is lcd_cpu_gpio struct index */
	char gpio_name[15];
	unsigned short value;
	unsigned short delay;
};

struct Lcd_Power_Ctrl_s {
	struct Lcd_Power_Config_s power_on_config[LCD_PWR_CTRL_STEP_MAX];
	struct Lcd_Power_Config_s power_off_config[LCD_PWR_CTRL_STEP_MAX];
	int cpu_gpio_num;
	int power_on_step;
	int power_off_step;
	unsigned char power_level; /* internal: 0=only power, 1=power+signal */
	int (*power_ctrl)(enum Bool_state_e status);
	int (*ports_ctrl)(enum Bool_state_e status);
};

struct Lcd_Misc_Ctrl_s {
	struct pinctrl *pin;
	unsigned char vpp_sel; /*0:vpp, 1:vpp2 */
	struct class *debug_class;
	unsigned char lcd_status;
	void (*module_enable)(void);
	void (*module_disable)(void);
	void (*lcd_test)(unsigned int num);
	void (*print_version)(void);
	void (*edp_edid_load)(void);
};

struct Lcd_Config_s {
	enum LCD_Chip_e chip;
	struct Lcd_Basic_s lcd_basic;
	struct Lcd_Timing_s lcd_timing;
	struct Lcd_Effect_s lcd_effect;
	struct Lcd_Control_Config_s lcd_control;
	struct Lcd_Power_Ctrl_s lcd_power_ctrl;
	struct Lcd_Misc_Ctrl_s lcd_misc_ctrl;
};

/* **********************************
 * power control define
 * ********************************** */
enum Lcd_Power_Type_e {
	LCD_POWER_TYPE_CPU = 0,
	LCD_POWER_TYPE_PMU,
	LCD_POWER_TYPE_SIGNAL,
	LCD_POWER_TYPE_INITIAL,
	LCD_POWER_TYPE_MAX,
};

enum Lcd_Power_Pmu_Gpio_e {
	LCD_POWER_PMU_GPIO0 = 0,
	LCD_POWER_PMU_GPIO1,
	LCD_POWER_PMU_GPIO2,
	LCD_POWER_PMU_GPIO3,
	LCD_POWER_PMU_GPIO4,
	LCD_POWER_PMU_GPIO_MAX,
};

#define LCD_POWER_GPIO_OUTPUT_LOW       0
#define LCD_POWER_GPIO_OUTPUT_HIGH      1
#define LCD_POWER_GPIO_INPUT            2
#define LCD_GPIO_OUTPUT_LOW             0
#define LCD_GPIO_OUTPUT_HIGH            1
#define LCD_GPIO_INPUT                  2

/* **********************************
 * global control API
 * ********************************** */
extern enum LCD_Chip_e lcd_chip_type;

enum Bool_check_e {
	FALSE = 0,
	TRUE = 1,
};

extern int detect_dsi_init_table(struct device_node *m_node, int on_off);
extern enum Bool_check_e lcd_chip_valid_check(struct Lcd_Config_s *pConf);

extern struct Lcd_Config_s *get_lcd_config(void);
extern void lcd_config_init(struct Lcd_Config_s *pConf);
extern void lcd_config_probe(struct Lcd_Config_s *pConf);
extern void lcd_config_remove(struct Lcd_Config_s *pConf);

#define DPRINT(...)                       pr_info(__VA_ARGS__)
#define lcd_gpio_request(dev, str)        gpiod_get(dev, str)
#define lcd_gpio_free(gdesc)              gpiod_put(gdesc)
#define lcd_gpio_input(gdesc)             gpiod_direction_input(gdesc)
#define lcd_gpio_output(gdesc, val)       gpiod_direction_output(gdesc, val)
#define lcd_gpio_get_value(gdesc)         gpiod_get_value(gdesc)
#define lcd_gpio_set_value(gdesc, val)    gpiod_set_value(gdesc, val)

/* **********************************
 * mipi-dsi read/write api
 * ********************************** */
/* *************************************************************
 * Function: dsi_write_cmd
 * Supported Data Type: DT_GEN_SHORT_WR_0, DT_GEN_SHORT_WR_1, DT_GEN_SHORT_WR_2,
			DT_DCS_SHORT_WR_0, DT_DCS_SHORT_WR_1,
			DT_GEN_LONG_WR, DT_DCS_LONG_WR,
			DT_SET_MAX_RET_PKT_SIZE
			DT_GEN_RD_0, DT_GEN_RD_1, DT_GEN_RD_2,
			DT_DCS_RD_0
 * Return:              command number
 * ************************************************************* */
extern int dsi_write_cmd(unsigned char *payload);

/* *************************************************************
 * Function: dsi_read_single
 * Supported Data Type: DT_GEN_RD_0, DT_GEN_RD_1, DT_GEN_RD_2,
			DT_DCS_RD_0
 * Return:              data count
			0 for not support
 * ************************************************************* */
extern int dsi_read_single(unsigned char *payload, unsigned char *rd_data,
		unsigned int rd_byte_len);
#endif
