#ifndef _TVAFE_GENERAL_H
#define _TVAFE_GENERAL_H

#include <linux/amlogic/tvin/tvin.h>
#include "tvafe_cvd.h"
#include <linux/amlogic/cpu_version.h>

/************************************************* */
/* *** macro definitions *********************** */
/* ************************************ */

/* reg need in tvafe_general.c */
#define HHI_VDAC_CNTL0				0x10bd
#define HHI_VDAC_CNTL1				0x10be
#define HHI_VIID_CLK_DIV			0x104a
#define HHI_VIID_CLK_CNTL			0x104b
#define HHI_VIID_DIVIDER_CNTL			0x104c
#define HHI_VID_CLK_CNTL2			0x1065
#define HHI_VID_DIVIDER_CNTL			0x1066

#define RESET1_REGISTER				0x1102

#define VENC_VDAC_DACSEL0			0x1b78
#define P_VENC_VDAC_DACSEL0		VCBUS_REG_ADDR(VENC_VDAC_DACSEL0)
#define VENC_VDAC_DACSEL1			0x1b79
#define P_VENC_VDAC_DACSEL1		VCBUS_REG_ADDR(VENC_VDAC_DACSEL1)
#define VENC_VDAC_DACSEL2			0x1b7a
#define P_VENC_VDAC_DACSEL2		VCBUS_REG_ADDR(VENC_VDAC_DACSEL2)
#define VENC_VDAC_DACSEL3			0x1b7b
#define P_VENC_VDAC_DACSEL3		VCBUS_REG_ADDR(VENC_VDAC_DACSEL3)
#define VENC_VDAC_DACSEL4			0x1b7c
#define P_VENC_VDAC_DACSEL4		VCBUS_REG_ADDR(VENC_VDAC_DACSEL4)
#define VENC_VDAC_DACSEL5			0x1b7d
#define P_VENC_VDAC_DACSEL5		VCBUS_REG_ADDR(VENC_VDAC_DACSEL5)
#define VENC_VDAC_SETTING			0x1b7e
#define P_VENC_VDAC_SETTING		VCBUS_REG_ADDR(VENC_VDAC_SETTING)
#define VENC_VDAC_TST_VAL			0x1b7f
#define P_VENC_VDAC_TST_VAL		VCBUS_REG_ADDR(VENC_VDAC_TST_VAL)
#define VENC_VDAC_DAC0_GAINCTRL			0x1bf0
#define P_VENC_VDAC_DAC0_GAINCTRL	VCBUS_REG_ADDR(VENC_VDAC_DAC0_GAINCTRL)
#define VENC_VDAC_DAC0_OFFSET			0x1bf1
#define P_VENC_VDAC_DAC0_OFFSET		VCBUS_REG_ADDR(VENC_VDAC_DAC0_OFFSET)
#define VENC_VDAC_DAC1_GAINCTRL			0x1bf2
#define P_VENC_VDAC_DAC1_GAINCTRL	VCBUS_REG_ADDR(VENC_VDAC_DAC1_GAINCTRL)
#define VENC_VDAC_DAC1_OFFSET			0x1bf3
#define P_VENC_VDAC_DAC1_OFFSET		VCBUS_REG_ADDR(VENC_VDAC_DAC1_OFFSET)
#define VENC_VDAC_DAC2_GAINCTRL			0x1bf4
#define P_VENC_VDAC_DAC2_GAINCTRL	VCBUS_REG_ADDR(VENC_VDAC_DAC2_GAINCTRL)
#define VENC_VDAC_DAC2_OFFSET			0x1bf5
#define P_VENC_VDAC_DAC2_OFFSET		VCBUS_REG_ADDR(VENC_VDAC_DAC2_OFFSET)
#define VENC_VDAC_DAC3_GAINCTRL			0x1bf6
#define P_VENC_VDAC_DAC3_GAINCTRL	VCBUS_REG_ADDR(VENC_VDAC_DAC3_GAINCTRL)
#define VENC_VDAC_DAC3_OFFSET			0x1bf7
#define P_VENC_VDAC_DAC3_OFFSET		VCBUS_REG_ADDR(VENC_VDAC_DAC3_OFFSET)
#define VENC_VDAC_DAC4_GAINCTRL			0x1bf8
#define P_VENC_VDAC_DAC4_GAINCTRL	VCBUS_REG_ADDR(VENC_VDAC_DAC4_GAINCTRL)
#define VENC_VDAC_DAC4_OFFSET			0x1bf9
#define P_VENC_VDAC_DAC4_OFFSET		VCBUS_REG_ADDR(VENC_VDAC_DAC4_OFFSET)
#define VENC_VDAC_DAC5_GAINCTRL			0x1bfa
#define P_VENC_VDAC_DAC5_GAINCTRL	VCBUS_REG_ADDR(VENC_VDAC_DAC5_GAINCTRL)
#define VENC_VDAC_DAC5_OFFSET			0x1bfb
#define P_VENC_VDAC_DAC5_OFFSET		VCBUS_REG_ADDR(VENC_VDAC_DAC5_OFFSET)
#define VENC_VDAC_FIFO_CTRL			0x1bfc
#define P_VENC_VDAC_FIFO_CTRL		VCBUS_REG_ADDR(VENC_VDAC_FIFO_CTRL)

#define HHI_VAFE_CLKXTALIN_CNTL			0x108f
#define P_HHI_VAFE_CLKXTALIN_CNTL	CBUS_REG_ADDR(HHI_VAFE_CLKXTALIN_CNTL)
#define HHI_VAFE_CLKOSCIN_CNTL			0x1090
#define P_HHI_VAFE_CLKOSCIN_CNTL	CBUS_REG_ADDR(HHI_VAFE_CLKOSCIN_CNTL)
#define HHI_VAFE_CLKIN_CNTL			0x1091
#define P_HHI_VAFE_CLKIN_CNTL		CBUS_REG_ADDR(HHI_VAFE_CLKIN_CNTL)
#define HHI_TVFE_AUTOMODE_CLK_CNTL		0x1092
#define P_HHI_TVFE_AUTOMODE_CLK_CNTL CBUS_REG_ADDR(HHI_TVFE_AUTOMODE_CLK_CNTL)
#define HHI_VAFE_CLKPI_CNTL			0x1093
#define P_HHI_VAFE_CLKPI_CNTL		CBUS_REG_ADDR(HHI_VAFE_CLKPI_CNTL)

#define HHI_VPU_CLK_CNTL			0x106f

#define HHI_CADC_CNTL				0x1020
#define HHI_CADC_CNTL2				0x1021
#define HHI_CADC_CNTL3				0x1022
#define HHI_CADC_CNTL4				0x1023
#define HHI_CADC_CNTL5				0x1024
#define HHI_CADC_CNTL6				0x1025

#define HHI_DADC_CNTL				0x1027
#define HHI_DADC_CNTL2				0x1028
#define HHI_DADC_RDBK0_I			0x1029
#define HHI_DADC_CNTL3				0x102a
#define HHI_DADC_CNTL4				0x102b

#define HHI_ADC_PLL_CNTL			0x10aa
#define P_HHI_ADC_PLL_CNTL			CBUS_REG_ADDR(HHI_ADC_PLL_CNTL)
#define HHI_ADC_PLL_CNTL2			0x10ab
#define P_HHI_ADC_PLL_CNTL2			CBUS_REG_ADDR(HHI_ADC_PLL_CNTL2)
#define HHI_ADC_PLL_CNTL3			0x10ac
#define P_HHI_ADC_PLL_CNTL3			CBUS_REG_ADDR(HHI_ADC_PLL_CNTL3)
#define HHI_ADC_PLL_CNTL4			0x10ad
#define P_HHI_ADC_PLL_CNTL4			CBUS_REG_ADDR(HHI_ADC_PLL_CNTL4)
#define HHI_ADC_PLL_CNTL5			0x109e
#define P_HHI_ADC_PLL_CNTL5			CBUS_REG_ADDR(HHI_ADC_PLL_CNTL5)
#define HHI_ADC_PLL_CNTL6			0x109f
#define P_HHI_ADC_PLL_CNTL6			CBUS_REG_ADDR(HHI_ADC_PLL_CNTL6)
#define HHI_ADC_PLL_CNTL1			0x10af
#define P_HHI_ADC_PLL_CNTL1			CBUS_REG_ADDR(HHI_ADC_PLL_CNTL1)
#define HHI_GCLK_OTHER              0x1054

/* adc pll ctl, atv demod & tvafe use the same adc module
 * module index: atv demod:0x01; tvafe:0x2
*/
#define ADC_EN_ATV_DEMOD	0x1
#define ADC_EN_TVAFE		0x2

#define LOG_ADC_CAL
/* #define LOG_VGA_EDID */
/* ******************************************************** */
/* *** enum definitions ****************************** */
/* ************************************************** */
enum tvafe_adc_ch_e {
	TVAFE_ADC_CH_NULL = 0,
/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
	TVAFE_ADC_CH_0 = 1,
	TVAFE_ADC_CH_1 = 2,
	TVAFE_ADC_CH_2 = 3,
	TVAFE_ADC_CH_3 = 4,
#if 0
	/* m6tv */
	TVAFE_ADC_CH_PGA = 1,
	TVAFE_ADC_CH_A = 2,
	TVAFE_ADC_CH_B = 3,
	TVAFE_ADC_CH_C = 4,
#endif
};
#if 0
/* ******************************************* */
/* *** structure definitions ******************** */
/* ********************************************** */
struct tvafe_cal_operand_s {
	unsigned int a;
	unsigned int b;
	unsigned int c;
	unsigned int step;
	unsigned int bpg_h;
	unsigned int bpg_v;
	unsigned int clk_ctl;
	unsigned int vafe_ctl;
#ifdef CONFIG_ADC_CAL_SIGNALED
	unsigned int pin_a_mux:2;
	unsigned int pin_b_mux:3;
	unsigned int pin_c_mux:3;
	unsigned int sog_mux:3;
#endif
	unsigned int sync_mux:1;
	unsigned int clk_ext:1;
	unsigned int bpg_m:2;
	unsigned int lpf_a:1;
	unsigned int lpf_b:1;
	unsigned int lpf_c:1;
	unsigned int clamp_inv:1;
	unsigned int clamp_ext:1;
	unsigned int adj:1;
	unsigned int cnt:3;
	unsigned int dir:1;
	unsigned int dir0:1;
	unsigned int dir1:1;
	unsigned int dir2:1;
	unsigned int adc0;
	unsigned int adc1;
	unsigned int adc2;
	unsigned int data0;
	unsigned int data1;
	unsigned int data2;
	unsigned int cal_fmt_cnt;
	unsigned int cal_fmt_max;
};

struct tvafe_cal_s {
	/* adc calibration data */
	struct tvafe_adc_cal_s      cal_val;
	struct tvafe_adc_comp_cal_s  fmt_cal_val;
	struct tvafe_cal_operand_s  cal_operand;
};
#endif
/* **************************************************** */
/* ******** function claims ******** */
/* ********************************************* */
extern int  tvafe_set_source_muxing(enum tvin_port_e port,
			struct tvafe_pin_mux_s *pinmux);
extern void tvafe_set_regmap(struct am_regs_s *p);
#if 0
extern void tvafe_vga_set_edid(struct tvafe_vga_edid_s *edid);
extern void tvafe_vga_get_edid(struct tvafe_vga_edid_s *edid);
extern void tvafe_set_cal_value(struct tvafe_cal_s *cal);
extern int tvafe_get_cal_value(struct tvafe_cal_s *cal);
extern void tvafe_set_cal_value2(struct tvafe_adc_cal_s *cal);
extern int tvafe_get_cal_value2(struct tvafe_adc_cal_s *cal);
extern bool tvafe_adc_cal(struct tvin_parm_s *parm,
			struct tvafe_cal_s *cal);
extern void tvafe_adc_clamp_adjust(struct tvin_parm_s *parm,
			struct tvafe_cal_s *cal);
extern void tvafe_get_wss_data(struct tvafe_comp_wss_s *wss);
extern void tvafe_set_vga_fmt(struct tvin_parm_s *parm,
		struct tvafe_cal_s *cal, struct tvafe_pin_mux_s *pinmux);
extern void tvafe_set_comp_fmt(struct tvin_parm_s *parm,
		struct tvafe_cal_s *cal, struct tvafe_pin_mux_s *pinmux);
#endif
extern void tvafe_init_reg(struct tvafe_cvd2_s *cvd2,
		struct tvafe_cvd2_mem_s *mem, enum tvin_port_e port,
		struct tvafe_pin_mux_s *pinmux);
extern void tvafe_set_apb_bus_err_ctrl(void);
extern void tvafe_enable_module(bool enable);
extern void tvafe_enable_avout(enum tvin_port_e port, bool enable);

/* vdac ctrl,adc/dac ref signal,cvbs out signal
 * module index: atv demod:0x01; dtv demod:0x02; tvafe:0x4; dac:0x8
*/
void vdac_enable(bool on, unsigned int module_sel);
extern void adc_set_pll_reset(void);
extern int tvafe_adc_get_pll_flag(void);

extern struct mutex pll_mutex;
extern bool tvafe_dbg_enable;

#endif  /* _TVAFE_GENERAL_H */

