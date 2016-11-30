/*#include "register.h"*/
/*#include "c_arc_pointer_reg.h"*/
/*#include "a9_func.h"*/
/*#include "clk_util.h"*/
/*#include "c_stimulus.h"*/
/*#include "a9_l2_func.h"*/

#include "demod_func.h"
#include <linux/dvb/aml_demod.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include "acf_filter_coefficient.h"
#include <linux/mutex.h>

#define M6D

/* static void __iomem * demod_meson_reg_map[4]; */

#define pr_dbg(fmt, args ...) \
	do { \
		if (debug_demod) \
			pr_info("FE: " fmt, ## args); \
	} while (0)
#define pr_error(fmt, args ...) printk("FE: " fmt, ## args)

MODULE_PARM_DESC(debug_demod, "\n\t\t Enable frontend debug information");
static int debug_demod;
module_param(debug_demod, int, 0644);

MODULE_PARM_DESC(demod_timeout, "\n\t\t timeout debug information");
static int demod_timeout = 120;
module_param(demod_timeout, int, 0644);

MODULE_PARM_DESC(demod_sync_count, "\n\t\t timeout debug information");
static int demod_sync_count = 60;
module_param(demod_sync_count, int, 0644);

MODULE_PARM_DESC(demod_sync_delay_time, "\n\t\t timeout debug information");
static int demod_sync_delay_time = 8;
module_param(demod_sync_delay_time, int, 0644);



MODULE_PARM_DESC(demod_mobile_power, "\n\t\t demod_mobile_power debug information");
static int demod_mobile_power = 100;
module_param(demod_mobile_power, int, 0644);

MODULE_PARM_DESC(demod_enable_performance, "\n\t\t demod_enable_performance information");
static int demod_enable_performance = 1;
module_param(demod_enable_performance, int, 0644);


static struct mutex mp;
static struct mutex dtvpll_init_lock;
static int dtvpll_init;
static int dtmb_spectrum = 2;


/* 8vsb */
static struct atsc_cfg list_8vsb[22] = {
	{0x0733, 0x00, 0},
	{0x0734, 0xff, 0},
	{0x0716, 0x02, 0},	/* F06[7] invert spectrum  0x02 0x06 */
	{0x05e7, 0x00, 0},
	{0x05e8, 0x00, 0},
	{0x0f06, 0x80, 0},
	{0x0f09, 0x04, 0},
	{0x070c, 0x18, 0},
	{0x070d, 0x9d, 0},
	{0x070e, 0x89, 0},
	{0x070f, 0x6a, 0},
	{0x0710, 0x75, 0},
	{0x0711, 0x6f, 0},
	{0x072a, 0x02, 0},
	{0x072c, 0x02, 0},
	{0x090d, 0x03, 0},
	{0x090e, 0x02, 0},
	{0x090f, 0x00, 0},
	{0x0900, 0x01, 0},
	{0x0900, 0x00, 0},
	{0x0f00, 0x01, 0},
	{0x0000, 0x00, 1}
};

/* 64qam */
static struct atsc_cfg list_qam64[111] = {
	{0x0900, 0x01, 0},
	{0x0f04, 0x08, 0},
	{0x0f06, 0x80, 0},
	{0x0f07, 0x00, 0},
	{0x0f00, 0xe0, 0},
	{0x0f00, 0xec, 0},
	{0x0001, 0x05, 0},
	{0x0002, 0x61, 0},	/* /0x61 invert spectrum */
	{0x0003, 0x3e, 0},
	{0x0004, 0xed, 0},	/* 0x9d */
	{0x0005, 0x10, 0},
	{0x0006, 0xc0, 0},
	{0x0007, 0x5c, 0},
	{0x0008, 0x0f, 0},
	{0x0009, 0x4f, 0},
	{0x000a, 0xfc, 0},
	{0x000b, 0x0c, 0},
	{0x000c, 0x6c, 0},
	{0x000d, 0x3a, 0},
	{0x000e, 0x10, 0},
	{0x000f, 0x02, 0},
	{0x0011, 0x00, 0},
	{0x0012, 0xf5, 0},
	{0x0013, 0x74, 0},
	{0x0014, 0xb9, 0},
	{0x0015, 0x1f, 0},
	{0x0016, 0x80, 0},
	{0x0017, 0x1f, 0},
	{0x0018, 0x0f, 0},
	{0x001e, 0x00, 0},
	{0x001f, 0x00, 0},
	{0x0023, 0x03, 0},
	{0x0025, 0x20, 0},
	{0x0026, 0xff, 0},
	{0x0027, 0xff, 0},
	{0x0028, 0xf8, 0},
	{0x0200, 0x20, 0},
	{0x0201, 0x62, 0},
	{0x0202, 0x23, 0},
	{0x0204, 0x19, 0},
	{0x0205, 0x74, 0},
	{0x0206, 0xab, 0},
	{0x0207, 0xff, 0},
	{0x0208, 0xc0, 0},
	{0x0209, 0xff, 0},
	{0x0211, 0xc0, 0},
	{0x0212, 0xb0, 0},
	{0x0213, 0x05, 0},
	{0x0215, 0x08, 0},
	{0x0222, 0xe0, 0},
	{0x0223, 0xf0, 0},
	{0x0226, 0x40, 0},
	{0x0229, 0x23, 0},
	{0x022a, 0x02, 0},
	{0x022c, 0x01, 0},
	{0x022e, 0x01, 0},
	{0x022f, 0x25, 0},
	{0x0230, 0x40, 0},
	{0x0231, 0x01, 0},
	{0x0734, 0xff, 0},
	{0x073a, 0xff, 0},
	{0x073b, 0x04, 0},
	{0x073c, 0x08, 0},
	{0x073d, 0x08, 0},
	{0x073e, 0x01, 0},
	{0x073f, 0xf8, 0},
	{0x0740, 0xf1, 0},
	{0x0741, 0xf3, 0},
	{0x0742, 0xff, 0},
	{0x0743, 0x0f, 0},
	{0x0744, 0x1a, 0},
	{0x0745, 0x16, 0},
	{0x0746, 0x00, 0},
	{0x0747, 0xe3, 0},
	{0x0748, 0xce, 0},
	{0x0749, 0xd4, 0},
	{0x074a, 0x00, 0},
	{0x074b, 0x4b, 0},
	{0x074c, 0x00, 0},
	{0x074d, 0xa2, 0},
	{0x074e, 0x00, 0},
	{0x074f, 0xe6, 0},
	{0x0750, 0x00, 0},
	{0x0751, 0x00, 0},
	{0x0752, 0x01, 0},
	{0x0753, 0x03, 0},
	{0x0400, 0x00, 0},
	{0x0408, 0x04, 0},
	{0x040e, 0xe0, 0},
	{0x0500, 0x02, 0},
	{0x05e7, 0x00, 0},
	{0x05e8, 0x00, 0},
	{0x0f09, 0x18, 0},
	{0x070c, 0x20, 0},
	{0x070d, 0x41, 0},	/* 0x49 */
	{0x070e, 0x04, 0},	/* 0x37 */
	{0x070f, 0x00, 0},
	{0x0710, 0x00, 0},
	{0x0711, 0x00, 0},
	{0x0716, 0xf0, 0},
	{0x090f, 0x00, 0},
	{0x0900, 0x01, 1},
	{0x0900, 0x00, 0},
	{0x0001, 0xf5, 0},
	{0x0001, 0xf5, 1},
	{0x0001, 0xf5, 1},
	{0x0001, 0xf5, 1},
	{0x0001, 0xf5, 1},
	{0x0001, 0x05, 0},
	{0x0001, 0x05, 1},
	{0x0000, 0x00, 1}
};

/* 256qam */
static struct atsc_cfg list_qam256[113] = {
	{0x0900, 0x01, 0},
	{0x0f04, 0x08, 0},
	{0x0f06, 0x80, 0},
	{0x0f00, 0xe0, 0},
	{0x0f00, 0xec, 0},
	{0x0001, 0x05, 0},
	{0x0002, 0x01, 0},	/* 0x09 */
	{0x0003, 0x2c, 0},
	{0x0004, 0x91, 0},
	{0x0005, 0x10, 0},
	{0x0006, 0xc0, 0},
	{0x0007, 0x5c, 0},
	{0x0008, 0x0f, 0},
	{0x0009, 0x4f, 0},
	{0x000a, 0xfc, 0},
	{0x000b, 0x0c, 0},
	{0x000c, 0x6c, 0},
	{0x000d, 0x3a, 0},
	{0x000e, 0x10, 0},
	{0x000f, 0x02, 0},
	{0x0011, 0x80, 0},
	{0x0012, 0xf5, 0},	/* a5 */
	{0x0013, 0x74, 0},
	{0x0014, 0xb9, 0},
	{0x0015, 0x1f, 0},
	{0x0016, 0x80, 0},
	{0x0017, 0x1f, 0},
	{0x0018, 0x0f, 0},
	{0x001e, 0x00, 0},
	{0x001f, 0x00, 0},
	{0x0023, 0x03, 0},
	{0x0025, 0x20, 0},
	{0x0026, 0xff, 0},
	{0x0027, 0xff, 0},
	{0x0028, 0xf8, 0},
	{0x0200, 0x20, 0},
	{0x0201, 0x62, 0},
	{0x0202, 0x23, 0},
	{0x0204, 0x19, 0},
	{0x0205, 0x76, 0},
	{0x0206, 0xd2, 0},
	{0x0207, 0xff, 0},
	{0x0208, 0xc0, 0},
	{0x0209, 0xff, 0},
	{0x0211, 0xc0, 0},
	{0x0212, 0xb0, 0},
	{0x0213, 0x05, 0},
	{0x0215, 0x08, 0},
	{0x0222, 0xf0, 0},
	{0x0223, 0xff, 0},
	{0x0226, 0x40, 0},
	{0x0229, 0x23, 0},
	{0x022a, 0x02, 0},
	{0x022c, 0x01, 0},
	{0x022e, 0x01, 0},
	{0x022f, 0x05, 0},
	{0x0230, 0x40, 0},
	{0x0231, 0x01, 0},
	{0x0400, 0x02, 0},
	{0x0401, 0x30, 0},
	{0x0402, 0x13, 0},
	{0x0406, 0x06, 0},
	{0x0408, 0x04, 0},
	{0x040e, 0xe0, 0},
	{0x0411, 0x02, 0},
	{0x073a, 0x02, 0},
	{0x073b, 0x09, 0},
	{0x073c, 0x0c, 0},
	{0x073d, 0x08, 0},
	{0x073e, 0xfd, 0},
	{0x073f, 0xf2, 0},
	{0x0740, 0xed, 0},
	{0x0741, 0xf4, 0},
	{0x0742, 0x03, 0},
	{0x0743, 0x15, 0},
	{0x0744, 0x1d, 0},
	{0x0745, 0x15, 0},
	{0x0746, 0xfc, 0},
	{0x0747, 0xde, 0},
	{0x0748, 0xcc, 0},
	{0x0749, 0xd6, 0},
	{0x074a, 0x04, 0},
	{0x074b, 0x4f, 0},
	{0x074c, 0x00, 0},
	{0x074d, 0xa2, 0},
	{0x074e, 0x00, 0},
	{0x074f, 0xe3, 0},
	{0x0750, 0x00, 0},
	{0x0751, 0xfc, 0},
	{0x0752, 0x00, 0},
	{0x0753, 0x03, 0},
	{0x0500, 0x02, 0},
	{0x05e7, 0x00, 0},
	{0x05e8, 0x00, 0},
	{0x0f09, 0x18, 0},
	{0x070c, 0x20, 0},
	{0x070d, 0x49, 0},
	{0x070e, 0x37, 0},
	{0x070f, 0x00, 0},
	{0x0710, 0x00, 0},
	{0x0711, 0x00, 0},
	{0x0716, 0xf0, 0},
	{0x090f, 0x00, 0},
	{0x0900, 0x01, 1},
	{0x0900, 0x00, 0},
	{0x0001, 0xf5, 0},
	{0x0001, 0xf5, 1},
	{0x0001, 0xf5, 1},
	{0x0001, 0xf5, 1},
	{0x0001, 0xf5, 1},
	{0x0001, 0x05, 0},
	{0x0001, 0x05, 1},
	{0x0000, 0x00, 1}
};

void dtvpll_lock_init(void)
{
	mutex_init(&dtvpll_init_lock);
}

void dtvpll_init_flag(int on)
{
	mutex_lock(&dtvpll_init_lock);
	dtvpll_init = on;
	mutex_unlock(&dtvpll_init_lock);
	pr_err("%s %d\n", __func__, on);
}

int get_dtvpll_init_flag(void)
{
	int val;
	mutex_lock(&dtvpll_init_lock);
	val = dtvpll_init;
	mutex_unlock(&dtvpll_init_lock);
	if (!val)
		pr_err("%s: %d\n", __func__, val);
	return val;
}

void adc_dpll_setup(int clk_a, int clk_b, int clk_sys)
{
	int unit, found, ena, enb, div2;
	int pll_m, pll_n, pll_od_a, pll_od_b, pll_xd_a, pll_xd_b;
	long freq_osc, freq_dco, freq_b, freq_a, freq_sys;
	long freq_b_act, freq_a_act, freq_sys_act, err_tmp, best_err;
	union adc_pll_cntl adc_pll_cntl;
	union adc_pll_cntl2 adc_pll_cntl2;
	union adc_pll_cntl3 adc_pll_cntl3;
	union adc_pll_cntl4 adc_pll_cntl4;
	union demod_dig_clk dig_clk_cfg;

	adc_pll_cntl.d32 = 0;
	adc_pll_cntl2.d32 = 0;
	adc_pll_cntl3.d32 = 0;
	adc_pll_cntl4.d32 = 0;

	pr_dbg("target clk_a %d  clk_b %d\n", clk_a, clk_b);

	unit = 10000;		/* 10000 as 1 MHz, 0.1 kHz resolution. */
	freq_osc = 24 * unit;

	if (clk_a < 1000)
		freq_a = clk_a * unit;
	else
		freq_a = clk_a * (unit / 1000);

	if (clk_b < 1000)
		freq_b = clk_b * unit;
	else
		freq_b = clk_b * (unit / 1000);

	ena = clk_a > 0 ? 1 : 0;
	enb = clk_b > 0 ? 1 : 0;

	if (ena || enb)
		adc_pll_cntl3.b.enable = 1;
	adc_pll_cntl3.b.reset = 1;

	found = 0;
	best_err = 100 * unit;
	pll_od_a = 1;
	pll_od_b = 1;
	pll_n = 1;
	for (pll_m = 1; pll_m < 512; pll_m++) {
		/* for (pll_n=1; pll_n<=5; pll_n++) { */
		if (is_meson_txl_cpu()) {
			freq_dco = freq_osc * pll_m / pll_n / 2;/*txl add div2*/
			if (freq_dco < 700 * unit || freq_dco > 1000 * unit)
				continue;
		} else {
			freq_dco = freq_osc * pll_m / pll_n;
			if (freq_dco < 750 * unit || freq_dco > 1550 * unit)
				continue;
		}
		pll_xd_a = freq_dco / (1 << pll_od_a) / freq_a;
		pll_xd_b = freq_dco / (1 << pll_od_b) / freq_b;

		freq_a_act = freq_dco / (1 << pll_od_a) / pll_xd_a;
		freq_b_act = freq_dco / (1 << pll_od_b) / pll_xd_b;

		err_tmp = (freq_a_act - freq_a) * ena + (freq_b_act - freq_b) *
		    enb;

		if (err_tmp >= best_err)
			continue;

		adc_pll_cntl.b.pll_m = pll_m;
		adc_pll_cntl.b.pll_n = pll_n;
		adc_pll_cntl.b.pll_od0 = pll_od_b;
		adc_pll_cntl.b.pll_od1 = pll_od_a;
		adc_pll_cntl.b.pll_xd0 = pll_xd_b;
		adc_pll_cntl.b.pll_xd1 = pll_xd_a;
		if (is_meson_txl_cpu()) {
			adc_pll_cntl4.b.pll_od3 = 0;
			adc_pll_cntl.b.pll_od2 = 0;
		} else {
			adc_pll_cntl2.b.div2_ctrl =
				freq_dco > 1000 * unit ? 1 : 0;
		}
		found = 1;
		best_err = err_tmp;
		/* } */
	}

	pll_m = adc_pll_cntl.b.pll_m;
	pll_n = adc_pll_cntl.b.pll_n;
	pll_od_b = adc_pll_cntl.b.pll_od0;
	pll_od_a = adc_pll_cntl.b.pll_od1;
	pll_xd_b = adc_pll_cntl.b.pll_xd0;
	pll_xd_a = adc_pll_cntl.b.pll_xd1;

	if (is_meson_txl_cpu())
		div2 = 1;
	else
		div2 = adc_pll_cntl2.b.div2_ctrl;
	/*
	 * p_adc_pll_cntl  =  adc_pll_cntl.d32;
	 * p_adc_pll_cntl2 = adc_pll_cntl2.d32;
	 * p_adc_pll_cntl3 = adc_pll_cntl3.d32;
	 * p_adc_pll_cntl4 = adc_pll_cntl4.d32;
	 */
	adc_pll_cntl3.b.reset = 0;
	/* *p_adc_pll_cntl3 = adc_pll_cntl3.d32; */
	if (!found) {
		pr_dbg(" ERROR can't setup %7ld kHz %7ld kHz\n",
		       freq_b / (unit / 1000), freq_a / (unit / 1000));
	} else {
		if (is_meson_txl_cpu())
			freq_dco = freq_osc * pll_m / pll_n / 2;
		else
			freq_dco = freq_osc * pll_m / pll_n;
		pr_dbg(" ADC PLL  M %3d   N %3d\n", pll_m, pll_n);
		pr_dbg(" ADC PLL DCO %ld kHz\n", freq_dco / (unit / 1000));

		pr_dbg(" ADC PLL XD %3d  OD %3d\n", pll_xd_b, pll_od_b);
		pr_dbg(" ADC PLL XD %3d  OD %3d\n", pll_xd_a, pll_od_a);

		freq_a_act = freq_dco / (1 << pll_od_a) / pll_xd_a;
		freq_b_act = freq_dco / (1 << pll_od_b) / pll_xd_b;

		pr_dbg(" B %7ld kHz %7ld kHz\n",
		       freq_b / (unit / 1000), freq_b_act / (unit / 1000));
		pr_dbg(" A %7ld kHz %7ld kHz\n",
		       freq_a / (unit / 1000), freq_a_act / (unit / 1000));

		if (clk_sys > 0) {
			dig_clk_cfg.b.demod_clk_en = 1;
			dig_clk_cfg.b.demod_clk_sel = 3;
			if (clk_sys < 1000)
				freq_sys = clk_sys * unit;
			else
				freq_sys = clk_sys * (unit / 1000);

			dig_clk_cfg.b.demod_clk_div = freq_dco / (1 + div2) /
			    freq_sys - 1;
			freq_sys_act = freq_dco / (1 + div2) /
			    (dig_clk_cfg.b.demod_clk_div + 1);
			pr_dbg(" SYS %7ld kHz div %d+1  %7ld kHz\n",
			       freq_sys / (unit / 1000),
			       dig_clk_cfg.b.demod_clk_div,
			       freq_sys_act / (unit / 1000));
		} else {
			dig_clk_cfg.b.demod_clk_en = 0;
		}

		/* *p_demod_dig_clk = dig_clk_cfg.d32; */
	}
	if (is_meson_txl_cpu()) {
		demod_set_demod_reg(TXLTV_ADC_RESET_VALUE, ADC_REG3);
		demod_set_demod_reg(adc_pll_cntl.d32, ADC_REG1);
		demod_set_demod_reg(dig_clk_cfg.d32, ADC_REG6);
		demod_set_demod_reg(TXLTV_ADC_REG3_VALUE, ADC_REG3);
		/* debug */
		pr_dbg("[adc][%x]%x\n", ADC_REG1,
				demod_read_demod_reg(ADC_REG1));
		pr_dbg("[adc][%x]%x\n", ADC_REG2,
				demod_read_demod_reg(ADC_REG2));
		pr_dbg("[adc][%x]%x\n", ADC_REG3,
				demod_read_demod_reg(ADC_REG3));
		pr_dbg("[adc][%x]%x\n", ADC_REG4,
				demod_read_demod_reg(ADC_REG4));
		pr_dbg("[adc][%x]%x\n", ADC_REG5,
				demod_read_demod_reg(ADC_REG5));
		pr_dbg("[adc][%x]%x\n", ADC_REG6,
				demod_read_demod_reg(ADC_REG6));
		pr_dbg("[adc][%x]%x\n", ADC_REG7,
				demod_read_demod_reg(ADC_REG7));
		pr_dbg("[adc][%x]%x\n", ADC_REG8,
				demod_read_demod_reg(ADC_REG8));
		pr_dbg("[adc][%x]%x\n", ADC_REG9,
				demod_read_demod_reg(ADC_REG9));
		pr_dbg("[adc][%x]%x\n", ADC_REGB,
				demod_read_demod_reg(ADC_REGB));
		pr_dbg("[adc][%x]%x\n", ADC_REGC,
				demod_read_demod_reg(ADC_REGC));
		pr_dbg("[adc][%x]%x\n", ADC_REGD,
				demod_read_demod_reg(ADC_REGD));
		pr_dbg("[adc][%x]%x\n", ADC_REGE,
				demod_read_demod_reg(ADC_REGE));
		pr_dbg("[demod][%x]%x\n", DEMOD_REG1,
				demod_read_demod_reg(DEMOD_REG1));
		pr_dbg("[demod][%x]%x\n", DEMOD_REG2,
				demod_read_demod_reg(DEMOD_REG2));
		pr_dbg("[demod][%x]%x\n", DEMOD_REG3,
				demod_read_demod_reg(DEMOD_REG3));
	} else {
		demod_set_demod_reg(ADC_RESET_VALUE, ADC_REG3);	/* adc reset */
		demod_set_demod_reg(adc_pll_cntl.d32, ADC_REG1);
		demod_set_demod_reg(dig_clk_cfg.d32, ADC_REG6);
		demod_set_demod_reg(ADC_REG3_VALUE, ADC_REG3);
		/* debug */
		pr_dbg("[adc][%x]%x\n", ADC_REG1,
				demod_read_demod_reg(ADC_REG1));
		pr_dbg("[adc][%x]%x\n", ADC_REG2,
				demod_read_demod_reg(ADC_REG2));
		pr_dbg("[adc][%x]%x\n", ADC_REG3,
				demod_read_demod_reg(ADC_REG3));
		pr_dbg("[adc][%x]%x\n", ADC_REG4,
				demod_read_demod_reg(ADC_REG4));
		pr_dbg("[adc][%x]%x\n", ADC_REG6,
				demod_read_demod_reg(ADC_REG6));
		pr_dbg("[demod][%x]%x\n", DEMOD_REG1,
				demod_read_demod_reg(DEMOD_REG1));
		pr_dbg("[demod][%x]%x\n", DEMOD_REG2,
				demod_read_demod_reg(DEMOD_REG2));
		pr_dbg("[demod][%x]%x\n", DEMOD_REG3,
				demod_read_demod_reg(DEMOD_REG3));
	}
	dtvpll_init_flag(1);
}

void demod_set_adc_core_clk(int adc_clk, int sys_clk, int dvb_mode)
{
	adc_dpll_setup(25, adc_clk, sys_clk);
}

void demod_set_cbus_reg(unsigned data, unsigned addr)
{
	void __iomem *vaddr;
	pr_dbg("[cbus][write]%x\n", (IO_CBUS_PHY_BASE + (addr << 2)));
	vaddr = ioremap((IO_CBUS_PHY_BASE + (addr << 2)), 0x4);
	writel(data, vaddr);
	iounmap(vaddr);
}

unsigned demod_read_cbus_reg(unsigned addr)
{
/* return __raw_readl(CBUS_REG_ADDR(addr)); */
	unsigned tmp;
	void __iomem *vaddr;
	vaddr = ioremap((IO_CBUS_PHY_BASE + (addr << 2)), 0x4);
	tmp = readl(vaddr);
	iounmap(vaddr);
/* tmp = aml_read_cbus(addr); */
	pr_dbg("[cbus][read]%x,data is %x\n",
	(IO_CBUS_PHY_BASE + (addr << 2)), tmp);
	return tmp;
}

void demod_set_ao_reg(unsigned data, unsigned addr)
{
	void __iomem *vaddr;

/* pr_dbg("[ao][write]%x,data is %x\n",(IO_AOBUS_BASE+addr),data); */
	vaddr = ioremap((IO_AOBUS_BASE + addr), 0x4);
	writel(data, vaddr);
	iounmap(vaddr);
}

unsigned demod_read_ao_reg(unsigned addr)
{
	unsigned tmp;
	void __iomem *vaddr;

/* pr_dbg("[ao][read]%x\n",(IO_AOBUS_BASE+addr)); */
	vaddr = ioremap((IO_AOBUS_BASE + addr), 0x4);
	tmp = readl(vaddr);
/* pr_dbg("[ao][read]%x,data is %x\n",(IO_AOBUS_BASE+addr),tmp); */
	iounmap(vaddr);
	return tmp;
}

void demod_set_demod_reg(unsigned data, unsigned addr)
{
	void __iomem *vaddr;
	mutex_lock(&mp);
/* printk("[demod][write]%x,data is %x\n",(addr),data); */
	vaddr = ioremap((addr), 0x4);
	writel(data, vaddr);
	iounmap(vaddr);
	mutex_unlock(&mp);
}

unsigned demod_read_demod_reg(unsigned addr)
{
	unsigned tmp;
	void __iomem *vaddr;
	mutex_lock(&mp);
	vaddr = ioremap((addr), 0x4);
	tmp = readl(vaddr);
	iounmap(vaddr);
	mutex_unlock(&mp);
/* printk("[demod][read]%x,data is %x\n",(addr),tmp); */
	return tmp;
}

void demod_power_switch(int pwr_cntl)
{
	int reg_data;
#if 1
	if (pwr_cntl == PWR_ON) {
		pr_dbg("[PWR]: Power on demod_comp %x,%x\n",
		       AO_RTI_GEN_PWR_SLEEP0, AO_RTI_GEN_PWR_ISO0);
		/* Powerup demod_comb */
		reg_data = demod_read_ao_reg(AO_RTI_GEN_PWR_SLEEP0);
		demod_set_ao_reg((reg_data & (~(0x1 << 10))),
				 AO_RTI_GEN_PWR_SLEEP0);
		/* [10] power on */
		pr_dbg("[PWR]: Power on demod_comp %x,%x\n",
		       HHI_DEMOD_MEM_PD_REG, RESET0_LEVEL);
		/* Power up memory */
		demod_set_demod_reg((demod_read_demod_reg(HHI_DEMOD_MEM_PD_REG)
				     & (~0x2fff)), HHI_DEMOD_MEM_PD_REG);
		/* reset */
		demod_set_demod_reg((demod_read_demod_reg(RESET0_LEVEL) &
				     (~(0x1 << 8))), RESET0_LEVEL);
	/*	msleep(20);*/

		/* remove isolation */
			demod_set_ao_reg(
				(demod_read_ao_reg(AO_RTI_GEN_PWR_ISO0) &
				  (~(0x3 << 14))), AO_RTI_GEN_PWR_ISO0);
		/* pull up reset */
		demod_set_demod_reg((demod_read_demod_reg(RESET0_LEVEL) |
				     (0x1 << 8)), RESET0_LEVEL);
/* *P_RESET0_LEVEL |= (0x1<<8); */
	} else {
		pr_dbg("[PWR]: Power off demod_comp\n");
		/* add isolation */

			demod_set_ao_reg(
				(demod_read_ao_reg(AO_RTI_GEN_PWR_ISO0) |
				  (0x3 << 14)), AO_RTI_GEN_PWR_ISO0);

		/* power down memory */
		demod_set_demod_reg((demod_read_demod_reg(HHI_DEMOD_MEM_PD_REG)
			 | 0x2fff), HHI_DEMOD_MEM_PD_REG);
		/* power down demod_comb */
		reg_data = demod_read_ao_reg(AO_RTI_GEN_PWR_SLEEP0);
		demod_set_ao_reg((reg_data | (0x1 << 10)),
				 AO_RTI_GEN_PWR_SLEEP0);
		/* [10] power on */
	}
#endif
}

static void clocks_set_sys_defaults(unsigned char dvb_mode)
{
	union demod_cfg0 cfg0;
	union demod_cfg2 cfg2;

	demod_power_switch(PWR_ON);

	if (is_meson_gxtvbb_cpu()) {
		pr_dbg("GX_TV config\n");
		demod_set_demod_reg(ADC_RESET_VALUE, ADC_REG3);
		demod_set_demod_reg(ADC_REG1_VALUE, ADC_REG1);
		demod_set_demod_reg(ADC_REG2_VALUE, ADC_REG2);
		demod_set_demod_reg(ADC_REG4_VALUE, ADC_REG4);
		demod_set_demod_reg(ADC_REG3_VALUE, ADC_REG3);
		/* dadc */
		demod_set_demod_reg(ADC_REG7_VALUE, ADC_REG7);
		demod_set_demod_reg(ADC_REG8_VALUE, ADC_REG8);
		demod_set_demod_reg(ADC_REG9_VALUE, ADC_REG9);
		demod_set_demod_reg(ADC_REGA_VALUE, ADC_REGA);
	} else if (is_meson_txl_cpu()) {
		pr_dbg("TXL_TV config\n");
		demod_set_demod_reg(TXLTV_ADC_REG3_VALUE, ADC_REG3);
		demod_set_demod_reg(TXLTV_ADC_REG1_VALUE, ADC_REG1);
		demod_set_demod_reg(TXLTV_ADC_REGB_VALUE, ADC_REGB);
		demod_set_demod_reg(TXLTV_ADC_REG2_VALUE, ADC_REG2);
		demod_set_demod_reg(TXLTV_ADC_REG3_VALUE, ADC_REG3);
		demod_set_demod_reg(TXLTV_ADC_REG4_VALUE, ADC_REG4);
		demod_set_demod_reg(TXLTV_ADC_REGC_VALUE, ADC_REGC);
		demod_set_demod_reg(TXLTV_ADC_REGD_VALUE, ADC_REGD);
		demod_set_demod_reg(TXLTV_ADC_RESET_VALUE, ADC_REG3);
		demod_set_demod_reg(TXLTV_ADC_REG3_VALUE, ADC_REG3);

		/* dadc */
		demod_set_demod_reg(TXLTV_ADC_REG7_VALUE, ADC_REG7);
		demod_set_demod_reg(TXLTV_ADC_REG8_VALUE, ADC_REG8);
		demod_set_demod_reg(TXLTV_ADC_REG9_VALUE, ADC_REG9);
		demod_set_demod_reg(TXLTV_ADC_REGE_VALUE, ADC_REGE);
	}

	demod_set_demod_reg(DEMOD_REG1_VALUE, DEMOD_REG1);
	demod_set_demod_reg(DEMOD_REG2_VALUE, DEMOD_REG2);
	demod_set_demod_reg(DEMOD_REG3_VALUE, DEMOD_REG3);
	cfg0.b.mode = 7;
	cfg0.b.adc_format = 1;
	if (dvb_mode == Gxtv_Dvbc) {	/* // 0 -DVBC, 1-DVBT, ISDBT, 2-ATSC */
		cfg0.b.ts_sel = 2;
	} else if ((dvb_mode == Gxtv_Dvbt_Isdbt) || (dvb_mode == Gxtv_Dtmb)) {
		cfg0.b.ts_sel = 1;
		cfg0.b.adc_regout = 1;
	} else if (dvb_mode == Gxtv_Atsc) {
		cfg0.b.ts_sel = 4;
	}
	demod_set_demod_reg(cfg0.d32, DEMOD_REG1);
	cfg2.b.biasgen_en = 1;
	cfg2.b.en_adc = 1;
	demod_set_demod_reg(cfg2.d32, DEMOD_REG3);
	pr_dbg("0xc8020c00 is %x,dvb_mode is %d\n",
	       demod_read_demod_reg(DEMOD_REG1), dvb_mode);
}

void dtmb_write_reg(int reg_addr, int reg_data)
{
	if (!get_dtvpll_init_flag())
		return;
	demod_set_demod_reg(reg_data, reg_addr);
/* apb_write_reg(reg_addr,reg_data); */
}

int dtmb_read_reg(int reg_addr)
{
	if (!get_dtvpll_init_flag())
		return 0;
	return demod_read_demod_reg(reg_addr);	/* apb_read_reg(reg_addr); */
}

void atsc_write_reg(int reg_addr, int reg_data)
{
	if (!get_dtvpll_init_flag())
		return;
	apb_write_reg(ATSC_BASE, (reg_addr & 0xffff) << 8 | (reg_data & 0xff));
}

unsigned long atsc_read_reg(int reg_addr)
{
	unsigned long tmp;

	if (!get_dtvpll_init_flag())
		return 0;
	apb_write_reg(ATSC_BASE + 4, (reg_addr & 0xffff) << 8);
	tmp = apb_read_reg(ATSC_BASE);

	return tmp & 0xff;
}

unsigned long atsc_read_iqr_reg(void)
{
	unsigned long tmp;

	tmp = apb_read_reg(ATSC_BASE + 8);
	pr_dbg("[atsc irq] is %lx\n", tmp);
	return tmp & 0xffffffff;
}

int atsc_qam_set(fe_modulation_t mode)
{
	int i, j;

	if (mode == VSB_8) {	/* 5-8vsb, 2-64qam, 4-256qam */
		for (i = 0; list_8vsb[i].adr != 0; i++) {
			if (list_8vsb[i].rw)
				atsc_read_reg(list_8vsb[i].adr);
			/* msleep(20); */
			else
				atsc_write_reg(list_8vsb[i].adr,
					       list_8vsb[i].dat);
			/* msleep(20); */
		}
		j = 15589;
		pr_dbg("8-vsb mode\n");
	} else if (mode == QAM_64) {
		for (i = 0; list_qam64[i].adr != 0; i++) {
			if (list_qam64[i].rw) {
				atsc_read_reg(list_qam64[i].adr);
				msleep(20);
			} else {
				atsc_write_reg(list_qam64[i].adr,
					       list_qam64[i].dat);
				msleep(20);
			}
		}
		j = 16588;	/* 33177; */
		pr_dbg("64qam mode\n");
	} else if (mode == QAM_256) {
		for (i = 0; list_qam256[i].adr != 0; i++) {
			if (list_qam256[i].rw) {
				atsc_read_reg(list_qam256[i].adr);
				msleep(20);
			} else {
				atsc_write_reg(list_qam256[i].adr,
					       list_qam256[i].dat);
				msleep(20);
			}
		}
		j = 15649;	/* 31298; */
		pr_dbg("256qam mode\n");
	} else {
		for (i = 0; list_qam256[i].adr != 0; i++) {
			if (list_qam256[i].rw) {
				atsc_read_reg(list_qam256[i].adr);
				msleep(20);
			} else {
				atsc_write_reg(list_qam256[i].adr,
					       list_qam256[i].dat);
				msleep(20);
			}
		}
		j = 15649;	/* 31298; */
		pr_dbg("256qam mode\n");
	}
	return j;
}

void atsc_initial(struct aml_demod_sta *demod_sta)
{
	int fc, fs, cr, ck, j;
	fe_modulation_t mode;

	mode = demod_sta->ch_mode;

	j = atsc_qam_set(mode);	/* set mode */

	fs = demod_sta->adc_freq;	/* KHZ 25200 */
	fc = demod_sta->ch_if;	/* KHZ 6350 */

	cr = (fc * (1 << 17) / fs) * (1 << 6);
	ck = fs * j / 10 - (1 << 25);
	/* ck_rate = (f_samp / f_vsb /2 -1)*(1<<25);
	double f_vsb = 10.76238;// double f_64q = 5.056941;
	// double f_256q = 5.360537; */

	atsc_write_reg(0x070e, cr & 0xff);
	atsc_write_reg(0x070d, (cr >> 8) & 0xff);
	atsc_write_reg(0x070c, (cr >> 16) & 0xff);

	if (demod_sta->ch_mode == VSB_8) {
		atsc_write_reg(0x0711, ck & 0xff);
		atsc_write_reg(0x0710, (ck >> 8) & 0xff);
		atsc_write_reg(0x070f, (ck >> 16) & 0xff);
	}
	pr_dbg("0x70e is %x, 0x70d is %x, 0x70c is %x\n", cr & 0xff,
	       (cr >> 8) & 0xff, (cr >> 16) & 0xff);
	pr_dbg("fs is %d(SR),fc is %d(IF),cr is %x,ck is %x\n", fs, fc, cr, ck);
}

int atsc_set_ch(struct aml_demod_sta *demod_sta,
		struct aml_demod_i2c *demod_i2c,
		struct aml_demod_atsc *demod_atsc)
{
	int ret = 0;
	u8 demod_mode;
	u8 bw, sr, ifreq, agc_mode;
	u32 ch_freq;

	bw = demod_atsc->bw;
	sr = demod_atsc->sr;
	ifreq = demod_atsc->ifreq;
	agc_mode = demod_atsc->agc_mode;
	ch_freq = demod_atsc->ch_freq;
	demod_mode = demod_atsc->dat0;
	demod_sta->ch_mode = demod_atsc->mode;	/* TODO */
	demod_sta->agc_mode = agc_mode;
	demod_sta->ch_freq = ch_freq;
	demod_sta->dvb_mode = demod_mode;
	demod_sta->ch_bw = (8 - bw) * 1000;
	atsc_initial(demod_sta);
	pr_dbg("ATSC mode\n");
	return ret;
}

#if 0
static dtmb_cfg_t list_dtmb_v1[99] = {
	{0x00000000, 0x01, 0},
	{0x00001000, 0x02, 0},
	{0x00000000, 0x03, 0},
	{0x00000000, 0x04, 0},
	{0x00000000, 0x05, 0},
	{0x00000000, 0x06, 0},
	{0x007fffff, 0x07, 0},
	{0x0000000f, 0x08, 0},
	{0x00003000, 0x09, 0},
	{0x00000001, 0x0a, 0},
	{0x0c403006, 0x0b, 0},
	{0x44444400, 0x0c, 0},
	{0x1412c320, 0x0d, 0},
	{0x00000152, 0x10, 0},
	{0x47080137, 0x11, 0},
	{0x02200a16, 0x12, 0},
	{0x42190190, 0x13, 0},
	{0x7f807f80, 0x14, 0},
	{0x0000199a, 0x15, 0},
	{0x000a1466, 0x18, 0},
	{0x00274217, 0x1a, 0},
	{0x00131036, 0x1b, 1},
	{0x00000396, 0x1c, 0},
	{0x0037f3cc, 0x1d, 0},
	{0x00000029, 0x1e, 0},
	{0x0004f031, 0x1f, 0},
	{0x00f3cbd4, 0x20, 0},
	{0x0000007e, 0x21, 0},
	{0x23270b6a, 0x22, 0},
	{0x5f700c1b, 0x23, 0},
	{0x00133c2b, 0x24, 0},
	{0x2d3e0f12, 0x25, 0},
	{0x06363038, 0x26, 0},
	{0x060e0a3e, 0x27, 0},
	{0x0015161f, 0x28, 0},
	{0x0809031b, 0x29, 0},
	{0x181c0307, 0x2a, 0},
	{0x051f1a1b, 0x2b, 0},
	{0x00451dce, 0x2c, 0},
	{0x242fde12, 0x2d, 0},
	{0x0034e8fa, 0x2e, 0},
	{0x00000007, 0x30, 0},
	{0x16000d0c, 0x31, 0},
	{0x0000011f, 0x32, 0},
	{0x01000200, 0x33, 0},
	{0x10bbf376, 0x34, 0},
	{0x00000044, 0x35, 0},
	{0x00000000, 0x36, 0},
	{0x00000000, 0x37, 0},
	{0x00000000, 0x38, 0},
	{0x00000000, 0x39, 0},
	{0x00000031, 0x3a, 0},
	{0x4d6b0a58, 0x3b, 0},
	{0x00000c04, 0x3c, 0},
	{0x0d3b0a50, 0x3d, 0},
	{0x03140480, 0x3e, 0},
	{0x05e60452, 0x3f, 0},
	{0x05780400, 0x40, 0},
	{0x0063c025, 0x41, 0},
	{0x05050202, 0x42, 0},
	{0x5e4a0a14, 0x43, 0},
	{0x00003b42, 0x44, 0},
	{0xa53080ff, 0x45, 0},
	{0x00000000, 0x46, 0},
	{0x00133202, 0x47, 0},
	{0x01f00000, 0x48, 0},
	{0x00000000, 0x49, 0},
	{0x00000000, 0x4a, 0},
	{0x00000000, 0x4b, 0},
	{0x00000000, 0x4c, 0},
	{0x20405dc8, 0x4d, 0},
	{0x00000000, 0x4e, 0},
	{0x1f0205df, 0x4f, 0},
	{0x00001120, 0x50, 0},
	{0x4f190803, 0x51, 0},
	{0x00000000, 0x52, 0},
	{0x00000040, 0x53, 0},
	{0x00100050, 0x54, 0},
	{0x00cd1000, 0x55, 0},
	{0x00010fab, 0x56, 0},
	{0x03f0fc3f, 0x58, 0},
	{0x02005014, 0x59, 0},
	{0x01405014, 0x5a, 0},
	{0x00014284, 0x5b, 0},
	{0x00000320, 0x5c, 0},
	{0x14130e05, 0x5d, 0},
	{0x4321c963, 0x5f, 0},
	{0x624668f8, 0x60, 0},
	{0xccc08888, 0x61, 0},
	{0x13212111, 0x62, 0},
	{0x21100000, 0x63, 0},
	{0x624668f8, 0x64, 0},
	{0xccc08888, 0x65, 0},
	{0x13212111, 0x66, 0},
	{0x21100000, 0x67, 0},
	{0x624668f8, 0x68, 0},
	{0xccc08888, 0x69, 0},
	{0x0, 0x0, 0}
};
#endif

void dtmb_all_reset(void)
{
	int temp_data = 0;
	if (is_meson_txl_cpu()) {
		dtmb_write_reg(DTMB_FRONT_AFIFO_ADC, 0x1f);
		/*modified bu xiaotong*/
		dtmb_write_reg(DTMB_CHE_TPS_CONFIG, 0xc00000);
		dtmb_write_reg(DTMB_CHE_EQ_CONFIG, 0x1a027719);
		dtmb_write_reg(DTMB_FRONT_AGC_CONFIG1, 0x101a7);
		dtmb_write_reg(DTMB_FRONT_47_CONFIG, 0x131a31);
		/*detect 64qam 420 595 problems*/
		dtmb_write_reg(DTMB_FRONT_19_CONFIG, 0x300);
		dtmb_write_reg(DTMB_FRONT_4d_CONFIG, 0x12ffbe0);
		/*fix fsm b bug*/
		dtmb_write_reg(DTMB_FRONT_DEBUG_CFG, 0x5680000);
		/*fix agc problem,skip warm_up status*/
		dtmb_write_reg(DTMB_FRONT_46_CONFIG, 0x1a000f0f);
		dtmb_write_reg(DTMB_FRONT_ST_FREQ, 0xf2400000);
	} else {
		dtmb_write_reg(DTMB_FRONT_AGC_CONFIG1, 0x10127);
		dtmb_write_reg(DTMB_CHE_IBDFE_CONFIG6, 0x943228cc);
		dtmb_write_reg(DTMB_CHE_IBDFE_CONFIG7, 0xc09aa8cd);
		dtmb_write_reg(DTMB_CHE_FD_TD_COEFF, 0x0);
		dtmb_write_reg(DTMB_CHE_EQ_CONFIG, 0x9dc59);
		/*0x2 is auto,0x406 is  invert spectrum*/
		if (dtmb_spectrum == 0)
			dtmb_write_reg(DTMB_TOP_CTRL_TPS, 0x406);
		else if (dtmb_spectrum == 1)
			dtmb_write_reg(DTMB_TOP_CTRL_TPS, 0x402);
		else
			dtmb_write_reg(DTMB_TOP_CTRL_TPS, 0x2);

		pr_dbg("dtmb_spectrum is %d\n", dtmb_spectrum);
		dtmb_write_reg(DTMB_TOP_CTRL_FEC, 0x41444400);
		dtmb_write_reg(DTMB_TOP_CTRL_INTLV_TIME, 0x180300);
		dtmb_write_reg(DTMB_FRONT_DDC_BYPASS, 0x662ca0);
		dtmb_write_reg(DTMB_FRONT_AFIFO_ADC, 0x29);
		dtmb_write_reg(DTMB_FRONT_DC_HOLD, 0xa1066);
		/*cci para*/
		dtmb_write_reg(DTMB_CHE_M_CCI_THR_CONFIG3, 0x80201f6);
		dtmb_write_reg(DTMB_CHE_M_CCI_THR_CONFIG2, 0x3f20080);
		dtmb_write_reg(DTMB_CHE_TPS_CONFIG, 0xc00000);
		dtmb_write_reg(DTMB_TOP_CTRL_AGC, 0x3);
		dtmb_write_reg(DTMB_TOP_CTRL_TS_SFO_CFO, 0x20403006);
		dtmb_write_reg(DTMB_FRONT_AGC_CONFIG2, 0x7200a16);
		dtmb_write_reg(DTMB_FRONT_DEBUG_CFG, 0x1e00000);
		dtmb_write_reg(DTMB_TOP_CTRL_ENABLE, 0x7fffff);
		/*close ts3 timing loop*/
		dtmb_write_reg(DTMB_TOP_CTRL_DAGC_CCI, 0x305);
		/*dektec card issue,close f case snr drop*/
		dtmb_write_reg(DTMB_CHE_MC_SC_TIMING_POWTHR, 0xc06100a);
		if (demod_enable_performance) {
			dtmb_write_reg(DTMB_CHE_IBDFE_CONFIG1, 0x4040002);
			temp_data = dtmb_read_reg(DTMB_CHE_FD_TD_COEFF);
		    temp_data = (temp_data & ~0x3fff)|(0x241f & 0x3fff);
		    temp_data = temp_data | (1<<21);
			/*Set freeze_mode and reset coeff*/
		    dtmb_write_reg(DTMB_CHE_FD_TD_COEFF, temp_data);
		    temp_data = temp_data & ~(1<<21);
			/*Set freeze_mode and reset coeff*/
		    dtmb_write_reg(DTMB_CHE_FD_TD_COEFF, temp_data);
		}
	}
}

void dtmb_initial(struct aml_demod_sta *demod_sta)
{
/* dtmb_write_reg(0x049, memstart);		//only for init */
	dtmb_spectrum = 1;
	dtmb_spectrum = demod_sta->spectrum;
	dtmb_register_reset();
	dtmb_all_reset();
#if 0
	int i;
	for (i = 0; list_dtmb_v1[i].adr != 0; i++) {
		if (list_dtmb_v1[i].rw)
			apb_read_reg(DTMB_BASE + ((list_dtmb_v1[i].adr) << 2));
		/* msleep(20); */
		else
			apb_write_reg(DTMB_BASE + ((list_dtmb_v1[i].adr) << 2),
				      list_dtmb_v1[i].dat);
		/* msleep(20); */
	}
#endif
}

int check_dtmb_fec_lock(void)
{
	int fec_lock, snr, status;
	fec_lock = (dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR) >> 14) & 0x1;
	snr = dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR) & 0x3fff;
	if (fec_lock && (snr > 4))
		status = 1;
	else
		status = 0;
	return status;
}

int check_dtmb_mobile_det(void)
{
	int mobile_det = 0;
	mobile_det = (dtmb_read_reg(DTMB_TOP_CTRL_SYS_OFDM_CNT) >> 8) & 0x7ffff;
	return mobile_det;

}


int dtmb_information(void)
{
	int tps, snr, fec_lock, fec_bch_add, fec_ldpc_unc_acc, fec_ldpc_it_avg,
	    tmp, che_snr;
	struct aml_fe_dev *dev;

	dev = NULL;
	tps = dtmb_read_reg(DTMB_TOP_CTRL_CHE_WORKCNT);
	tmp = dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR);
	if (is_meson_txl_cpu())
		che_snr = tmp & 0x3fff;
	else
		che_snr = tmp & 0xfff;
	snr = che_snr;
	snr = convert_snr(snr);
	/*	if (che_snr >= 8192) */
	/*		che_snr = che_snr - 16384;*/
	/*	snr = che_snr / 32;*/
	/* snr = 10*log10(snr)-6; */
	fec_lock = (dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR) >> 14) & 0x1;
	fec_bch_add = dtmb_read_reg(DTMB_TOP_FEC_BCH_ACC);
	fec_ldpc_unc_acc = dtmb_read_reg(DTMB_TOP_FEC_LDPC_UNC_ACC);
	fec_ldpc_it_avg = dtmb_read_reg(DTMB_TOP_FEC_LDPC_IT_AVG);
	pr_dbg("¡¾FSM ¡¿: %x %x %x %x\n",
	       dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0),
	       dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE1),
	       dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE2),
	       dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE3));
	pr_dbg
	    ("¡¾AGC ¡¿: agc_power %d,agc_if_gain %d,agc_rf_gain %d,",
	     (-(((dtmb_read_reg(DTMB_TOP_FRONT_AGC) >> 22) & 0x3ff) / 16)),
	     ((dtmb_read_reg(DTMB_TOP_FRONT_AGC)) & 0x3ff),
	     ((dtmb_read_reg(DTMB_TOP_FRONT_AGC) >> 11) & 0x7ff));
	pr_dbg
	      ("dagc_power %3d,dagc_gain %3d mobi_det_power %d\n",
	      ((dtmb_read_reg(DTMB_TOP_FRONT_DAGC) >> 0) & 0xff),
	     ((dtmb_read_reg(DTMB_TOP_FRONT_DAGC) >> 8) & 0xfff),
	     (dtmb_read_reg(DTMB_TOP_CTRL_SYS_OFDM_CNT) >> 8) & 0x7ffff);
	pr_dbg
	    ("¡¾TPS ¡¿ SC or MC %2d,f_r %2d qam_nr %2d ",
	     (dtmb_read_reg(DTMB_TOP_CHE_OBS_STATE1) >> 1) & 0x1,
	     (tps >> 22) & 0x1, (tps >> 21) & 0x1);
	pr_dbg
		("intlv %2d,cr %2d constl %2d\n",
		(tps >> 20) & 0x1,
	     (tps >> 18) & 0x3, (tps >> 16) & 0x3);

	pr_dbg
	    ("[dtmb] snr is %d,fec_lock is %d,fec_bch_add is %d,",
	     snr, fec_lock, fec_bch_add);
	pr_dbg
	    ("fec_ldpc_unc_acc is %d ,fec_ldpc_it_avg is %d\n",
	     fec_ldpc_unc_acc,
	     fec_ldpc_it_avg / 256);
	pr_dbg
	    ("------------------------------------------------------------\n");

	tuner_get_ch_power(dev);

	return 0;
}

int dtmb_check_cci(void)
{
	int cci_det = 0;
	cci_det =
	((dtmb_read_reg(DTMB_TOP_SYNC_CCI_NF2_POSITION) >> 22)
		& 0x3);
	if (cci_det > 0) {
			pr_dbg("find cci\n");
			dtmb_write_reg(DTMB_CHE_CCIDET_CONFIG, 0x20210290);
			dtmb_write_reg(DTMB_CHE_M_CCI_THR_CONFIG3, 0x20081f6);
			dtmb_write_reg(DTMB_CHE_M_CCI_THR_CONFIG2, 0x3f08020);
	}
	return cci_det;
}

int dtmb_bch_check(void)
{
	int fec_bch_add, i;
	fec_bch_add = dtmb_read_reg(DTMB_TOP_FEC_BCH_ACC);
	pr_dbg("[debug]fec lock,fec_bch_add is %d\n", fec_bch_add);
	msleep(100);
	if (((dtmb_read_reg(DTMB_TOP_FEC_BCH_ACC))-fec_bch_add) >= 50) {
		pr_dbg("[debug]fec lock,but bch add ,need reset,wait not to reset\n");
		dtmb_reset();
		for (i = 0; i < 30; i++) {
			msleep(100);
			if (check_dtmb_fec_lock() == 1) {
				pr_dbg("[debug]fec lock,but bch add ,need reset,now is lock\n");
				return 0;
			}
		}
	}
	return 0;
}

int dtmb_constell_check(void)
{
	int constell;
	constell = dtmb_read_reg(DTMB_TOP_CTRL_CHE_WORKCNT)>>16 & 0x3;
	if (constell == 0)/*4qam*/
		dtmb_write_reg(DTMB_FRONT_47_CONFIG, 0x133221);
	else if (constell == 1)/*16qam*/
		dtmb_write_reg(DTMB_FRONT_47_CONFIG, 0x132821);
	else if (constell == 2)/*32qam*/
		dtmb_write_reg(DTMB_FRONT_47_CONFIG, 0x131e21);
	else if (constell == 3)/*64qam*/
		dtmb_write_reg(DTMB_FRONT_47_CONFIG, 0x131a31);

	return 0;
}


int dtmb_check_fsm(void)
{
	int tmp, fsm_status, i, has_singal;
	tmp = dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0);
	fsm_status =  tmp&0xffffffff;
	has_singal = 0;
	pr_dbg("[rsj1] fsm_status is %x\n", fsm_status);
	for (i = 0 ; i < 8 ; i++) {
		if (((fsm_status >> (i*4)) & 0xf) > 3) {
			/*has signal*/
		/*	pr_dbg("has signal\n");*/
			has_singal = 1;
		}
	}
	return has_singal;

}

int patch_ts3(int delay1_us, int delay2_us)
{
	if (((dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0)&0xf) == 0x7)&1) {
		dtmb_write_reg(DTMB_TOP_CTRL_FSM, 0x300f);
		dtmb_write_reg(DTMB_TOP_CTRL_FSM, 0x310f);
		msleep(delay1_us);
		dtmb_write_reg(DTMB_TOP_CTRL_ENABLE, 0xffdfff);
		dtmb_write_reg(DTMB_TOP_CTRL_ENABLE, 0xffffff);
		dtmb_write_reg(DTMB_TOP_CTRL_FSM, 0x3110);
		dtmb_write_reg(DTMB_TOP_CTRL_FSM, 0x3010);
		dtmb_write_reg(DTMB_TOP_CTRL_FSM, 0x3000);
		return 1;
	} else
		return 0;
}


int read_cfo_all(void)
{
	int icfo_all, fcfo_all;
	icfo_all = dtmb_read_reg(DTMB_TOP_CTRL_ICFO_ALL) & 0xfffff;
	fcfo_all = dtmb_read_reg(DTMB_TOP_CTRL_FCFO_ALL) & 0x3fff;
	if (icfo_all > (1 << 19))
		icfo_all = icfo_all - (1 << 20);
	if (fcfo_all > (1 << 13))
		fcfo_all = fcfo_all - (1 << 14);

	return (int)(icfo_all*4+fcfo_all);

}


int dtmb_v3_soft_sync(int cfo_init)
{

/*	int cfo_all;*/
/*	int cfo_setting;*/

	if (cfo_init == 0) {
		cfo_init = patch_ts3(11, 0);
		#if 0
		if (cfo_init == 1) {
			cfo_all = read_cfo_all();
			cfo_setting = dtmb_read_reg(DTMB_FRONT_DDC_BYPASS);
			dtmb_write_reg(DTMB_FRONT_DDC_BYPASS,
			cfo_setting+cfo_all);
			dtmb_write_reg(DTMB_TOP_CTRL_LOOP, 0x3);
			dtmb_reset();
		}
		#endif
	}
	return cfo_init;

}

int dtmb_check_status_gxtv(struct dvb_frontend *fe)
{
	int local_state;
	int time_cnt;/* cci_det, src_config;*/
	int cfo_init, count;

	dtmb_information();
	time_cnt = 0;
	local_state = 0;
	cfo_init = 0;
	if (check_dtmb_fec_lock() != 1) {
		dtmb_register_reset();
		dtmb_all_reset();
		count = 15;
		while ((count) &&
		((dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0)&0xf) < 0x6)) {
			msleep(20);
			count--;
		}

		count = demod_sync_count;
		while ((count) && (cfo_init == 0)) {

			cfo_init = dtmb_v3_soft_sync(cfo_init);

			msleep(demod_sync_delay_time);
			count--;
		}
		if ((cfo_init == 0) &&
			((dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0)&0xf) <= 7)) {
			pr_dbg("over 400ms,status is %x, need reset\n",
				(dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0)&0xf));
			return 0;
		}
		while ((time_cnt < 10) && (check_dtmb_fec_lock() != 1)) {
			msleep(demod_timeout);
			time_cnt++;
			local_state = AMLOGIC_DTMB_STEP3;
			dtmb_information();
			dtmb_check_cci();
			if (time_cnt > 8)
				pr_dbg
					("* local_state = %d\n", local_state);
		}
		if (time_cnt >= 10 && (check_dtmb_fec_lock() != 1)) {
			local_state = AMLOGIC_DTMB_STEP4;
			time_cnt = 0;
			pr_dbg
				("*all reset,timeout is %d\n", demod_timeout);
		}
	} else {
		dtmb_check_cci();
		dtmb_bch_check();
	#if 0
		cci_det = dtmb_check_cci();
		if ((check_dtmb_mobile_det() <= demod_mobile_power)
				&& (cci_det == 0)) {
			/* open */
			src_config = (dtmb_read_reg(DTMB_FRONT_SRC_CONFIG1));
			dtmb_write_reg(DTMB_FRONT_SRC_CONFIG1,
				src_config & (~(0x1 << 28)));
		} else {
			/* close */
			src_config = (dtmb_read_reg(DTMB_FRONT_SRC_CONFIG1));
			dtmb_write_reg(DTMB_FRONT_SRC_CONFIG1,
				src_config | (0x1 << 28));
		}
	#endif
	}
	if (check_dtmb_fec_lock() == 1)
		dtmb_write_reg(DTMB_TOP_CTRL_LOOP, 0xf);
	return 0;
}


int dtmb_check_status_txl(struct dvb_frontend *fe)
{
	int time_cnt;
	time_cnt = 0;
	dtmb_information();
	if (check_dtmb_fec_lock() != 1) {
		while ((time_cnt < 10) && (check_dtmb_fec_lock() != 1)) {
			msleep(demod_timeout);
			time_cnt++;
			dtmb_information();
			if (((dtmb_read_reg(DTMB_TOP_CTRL_CHE_WORKCNT)
				>> 21) & 0x1) == 0x1) {
				pr_dbg("4qam-nr,need set spectrum\n");
				if (dtmb_spectrum == 1) {
					dtmb_write_reg
					(DTMB_TOP_CTRL_TPS, 0x1010406);
				} else if (dtmb_spectrum == 0) {
					dtmb_write_reg
					(DTMB_TOP_CTRL_TPS, 0x1010402);
				} else {
					dtmb_write_reg
					(DTMB_TOP_CTRL_TPS, 0x1010002);
				}
			}
			if (time_cnt > 8)
				pr_dbg
					("* time_cnt = %d\n", time_cnt);
		}
		if (time_cnt >= 10 && (check_dtmb_fec_lock() != 1)) {
			time_cnt = 0;
			dtmb_register_reset();
			dtmb_all_reset();
			if	(dtmb_spectrum == 0)
				dtmb_spectrum = 1;
			else
				dtmb_spectrum = 0;
			pr_dbg
				("*all reset,timeout is %d\n", demod_timeout);
		}
	} else {
		dtmb_bch_check();
		dtmb_constell_check();
	}
	return 0;
}


void dtmb_reset(void)
{
	union DTMB_TOP_CTRL_SW_RST_BITS sw_rst;

	sw_rst.b.ctrl_sw_rst = 1;
	sw_rst.b.ctrl_sw_rst_noreg = 1;
	dtmb_write_reg(DTMB_TOP_CTRL_SW_RST, sw_rst.d32);
	sw_rst.b.ctrl_sw_rst = 0;
	sw_rst.b.ctrl_sw_rst_noreg = 0;
	dtmb_write_reg(DTMB_TOP_CTRL_SW_RST, sw_rst.d32);
}

void dtmb_register_reset(void)
{
	union DTMB_TOP_CTRL_SW_RST_BITS sw_rst;

	sw_rst.b.ctrl_sw_rst = 1;
	dtmb_write_reg(DTMB_TOP_CTRL_SW_RST, sw_rst.d32);
	sw_rst.b.ctrl_sw_rst = 0;
	dtmb_write_reg(DTMB_TOP_CTRL_SW_RST, sw_rst.d32);
}

int dtmb_set_ch(struct aml_demod_sta *demod_sta,
		struct aml_demod_i2c *demod_i2c,
		struct aml_demod_dtmb *demod_dtmb)
{
	int ret = 0;
	u8 demod_mode;
	u8 bw, sr, ifreq, agc_mode;
	u32 ch_freq;

	bw = demod_dtmb->bw;
	sr = demod_dtmb->sr;
	ifreq = demod_dtmb->ifreq;
	agc_mode = demod_dtmb->agc_mode;
	ch_freq = demod_dtmb->ch_freq;
	demod_mode = demod_dtmb->dat0;
	demod_sta->ch_mode = demod_dtmb->mode;	/* TODO */
	demod_sta->agc_mode = agc_mode;
	demod_sta->ch_freq = ch_freq;
	demod_sta->dvb_mode = demod_mode;
	demod_sta->ch_bw = (8 - bw) * 1000;
	dtmb_initial(demod_sta);
	pr_dbg("DTMB mode\n");
	return ret;
}

int dvbt_set_ch(struct aml_demod_sta *demod_sta,
		struct aml_demod_i2c *demod_i2c,
		struct aml_demod_dvbt *demod_dvbt)
{
	int ret = 0;
	u8_t demod_mode = 1;
	u8_t bw, sr, ifreq, agc_mode;
	u32_t ch_freq;

	bw = demod_dvbt->bw;
	sr = demod_dvbt->sr;
	ifreq = demod_dvbt->ifreq;
	agc_mode = demod_dvbt->agc_mode;
	ch_freq = demod_dvbt->ch_freq;
	demod_mode = demod_dvbt->dat0;
	if (ch_freq < 1000 || ch_freq > 900000000) {
		/* pr_dbg("Error: Invalid Channel Freq option %d\n",
		ch_freq); */
		ch_freq = 474000;
		ret = -1;
	}

	if (demod_mode < 0 || demod_mode > 4) {
		/* pr_dbg("Error: Invalid demod mode option %d\n",
		demod_mode); */
		demod_mode = 1;
		ret = -1;
	}

	/* demod_sta->dvb_mode  = 1; */
	demod_sta->ch_mode = 0;	/* TODO */
	demod_sta->agc_mode = agc_mode;
	demod_sta->ch_freq = ch_freq;
	demod_sta->dvb_mode = demod_mode;
	/*   if (demod_i2c->tuner == 1)
	 *     demod_sta->ch_if = 36130;
	 * else if (demod_i2c->tuner == 2)
	 *     demod_sta->ch_if = 4570;
	 * else if (demod_i2c->tuner == 3)
	 *     demod_sta->ch_if = 4000;// It is nouse.(alan)
	 * else if (demod_i2c->tuner == 7)
	 *     demod_sta->ch_if = 5000;//silab 5000kHz IF*/

	demod_sta->ch_bw = (8 - bw) * 1000;
	demod_sta->symb_rate = 0;	/* TODO */

/* bw=0; */
	demod_mode = 1;
	/* for si2176 IF:5M   sr 28.57 */
	sr = 4;
	ifreq = 4;
	if (bw == BANDWIDTH_AUTO)
		demod_mode = 2;
	ofdm_initial(bw,
			/* 00:8M 01:7M 10:6M 11:5M */
		     sr,
		     /* 00:45M 01:20.8333M 10:20.7M 11:28.57  100:24m */
		     ifreq,
		     /* 000:36.13M 001:-5.5M 010:4.57M 011:4M 100:5M */
		     demod_mode - 1,
		     /* 00:DVBT,01:ISDBT */
		     1
		     /* 0: Unsigned, 1:TC */
	    );
	pr_dbg("DVBT/ISDBT mode\n");

	return ret;
}

int demod_set_sys(struct aml_demod_sta *demod_sta,
		  struct aml_demod_i2c *demod_i2c,
		  struct aml_demod_sys *demod_sys)
{
/* int adc_clk; */
/* demod_sta->tmp=Adc_mode; */
	unsigned char dvb_mode;
	int clk_adc, clk_dem;
	int gpioDV_2;
	int gpiW_2;

	dvb_mode = demod_sta->dvb_mode;
	clk_adc = demod_sys->adc_clk;
	clk_dem = demod_sys->demod_clk;
	pr_dbg
	    ("demod_set_sys,clk_adc is %d,clk_demod is %d\n",
	     clk_adc, clk_dem);
	mutex_init(&mp);
	clocks_set_sys_defaults(dvb_mode);
	/* open dtv adc pinmux */
	if (is_meson_txl_cpu()) {
		gpioDV_2 = demod_read_demod_reg(0xc8834400 + (0x2e << 2));
		pr_dbg("[R840]set adc pinmux,gpioDV_2 %x\n", gpioDV_2);
		gpioDV_2 = gpioDV_2 | (0x1 << 22);
		gpioDV_2 = gpioDV_2 & ~(0x3 << 19);
		gpioDV_2 = gpioDV_2 & ~(0x1 << 23);
		gpioDV_2 = gpioDV_2 & ~(0x1 << 31);
		demod_set_demod_reg(gpioDV_2, 0xc8834400 + (0x2e << 2));
		pr_dbg("[R840]set adc pinmux,gpioDV_2 %x\n", gpioDV_2);
	} else {
		gpiW_2 = demod_read_demod_reg(0xc88344c4);
		gpiW_2 = gpiW_2 | (0x1 << 25);
		gpiW_2 = gpiW_2 & ~(0xd << 24);
		demod_set_demod_reg(gpiW_2, 0xc88344c4);
		pr_dbg("[R840]set adc pinmux,gpiW_2 %x\n", gpiW_2);
	}
	/* set adc clk */
	demod_set_adc_core_clk(clk_adc, clk_dem, dvb_mode);
	/* init for dtmb */
	if (dvb_mode == Gxtv_Dtmb) {
		/* open arbit */
	/*	demod_set_demod_reg(0x8, DEMOD_REG4);*/
	}
	demod_sta->adc_freq = clk_adc;
	demod_sta->clk_freq = clk_dem;
	return 0;
}

void demod_set_reg(struct aml_demod_reg *demod_reg)
{
	switch (demod_reg->mode) {
	case 0:
		demod_reg->addr = demod_reg->addr + QAM_BASE;
		break;
	case 1:
	case 2:
		demod_reg->addr = DTMB_TOP_ADDR(demod_reg->addr);
		break;
	case 3:
		/* demod_reg->addr=ATSC_BASE; */
		break;
	case 4:
		demod_reg->addr = demod_reg->addr * 4 + DEMOD_CFG_BASE;
		break;
	case 5:
		demod_reg->addr = demod_reg->addr + DEMOD_BASE;
		break;
	case 6:
		/* demod_reg->addr=demod_reg->addr*4+DEMOD_CFG_BASE; */
		break;
	case 11:
		demod_reg->addr = demod_reg->addr;
		break;
	case 10:
		/* demod_reg->addr=(u32_t)phys_to_virt(demod_reg->addr); */
		break;
	}

	if (demod_reg->mode == 3)
		atsc_write_reg(demod_reg->addr, demod_reg->val);
	else if (demod_reg->mode == 11)
		demod_set_cbus_reg(demod_reg->val, demod_reg->addr);
	else if (demod_reg->mode == 10)
		apb_write_reg_collect(demod_reg->addr, demod_reg->val);
	/* demod_reg->val_high = apb_read_reg_high(demod_reg->addr); */
	else
		demod_set_demod_reg(demod_reg->val, demod_reg->addr);
}

void demod_get_reg(struct aml_demod_reg *demod_reg)
{
	if (demod_reg->mode == 0) {
		demod_reg->addr = demod_reg->addr + QAM_BASE;
	} else if ((demod_reg->mode == 1) || (demod_reg->mode == 2)) {
		demod_reg->addr = DTMB_TOP_ADDR(demod_reg->addr);
	} else if (demod_reg->mode == 3) {
		/* demod_reg->addr=demod_reg->addr+ATSC_BASE; */
	} else if (demod_reg->mode == 4) {
		demod_reg->addr = demod_reg->addr * 4 + DEMOD_CFG_BASE;
	} else if (demod_reg->mode == 5) {
		demod_reg->addr = demod_reg->addr + DEMOD_BASE;
	} else if (demod_reg->mode == 6) {
		/* demod_reg->addr=demod_reg->addr*4+DEMOD_CFG_BASE; */
	} else if (demod_reg->mode == 11) {
		demod_reg->addr = demod_reg->addr;
	} else if (demod_reg->mode == 10) {
		/* printk("demod_reg->addr is %x\n",demod_reg->addr); */
		/* test=(unsigned long)phys_to_virt(test); */
/* demod_reg->addr=(unsigned long)phys_to_virt(demod_reg->addr); */
/* printk("demod_reg->addr is %lx %x\n",test,demod_reg->addr); */
	}

	if (demod_reg->mode == 3) {
		demod_reg->val = atsc_read_reg(demod_reg->addr);
		/* apb_write_reg(ATSC_BASE+4, (demod_reg->addr&0xffff)<<8); */
		/* demod_reg->val = apb_read_reg(ATSC_BASE)&0xff; */
	} else if (demod_reg->mode == 6) {
		demod_reg->val = atsc_read_iqr_reg();
		/* apb_write_reg(ATSC_BASE+4, (demod_reg->addr&0xffff)<<8); */
		/* demod_reg->val = apb_read_reg(ATSC_BASE)&0xff; */
	} else if (demod_reg->mode == 11) {
		demod_reg->val = demod_read_cbus_reg(demod_reg->addr);
	} else if (demod_reg->mode == 10) {
		demod_reg->val = apb_read_reg_collect(demod_reg->addr);
	/*	demod_reg->val_high = apb_read_reg_high(demod_reg->addr);*/
	} else {
		demod_reg->val = demod_read_demod_reg(demod_reg->addr);
	}
}

void apb_write_reg_collect(unsigned int addr, unsigned int data)
{
	writel(data, ((void __iomem *)(phys_to_virt(addr))));
/* *(volatile unsigned int*)addr = data; */
}

unsigned long apb_read_reg_collect(unsigned long addr)
{
	unsigned long tmp;
/*	void __iomem *vaddr;
	vaddr = ioremap(((unsigned long)phys_to_virt(addr)), 0x4);
	tmp = readl(vaddr);
	iounmap(vaddr);*/
	tmp = readl((void __iomem *)(phys_to_virt(addr)));
/*tmp = *(volatile unsigned long *)((unsigned long)phys_to_virt(addr));*/
/* printk("[all][read]%lx,data is %lx\n",addr,tmp); */
	return tmp & 0xffffffff;
}



void apb_write_reg(unsigned int addr, unsigned int data)
{
	demod_set_demod_reg(data, addr);
}

unsigned long apb_read_reg_high(unsigned long addr)
{
	unsigned long tmp;
	tmp = 0;
	return (tmp >> 32) & 0xffffffff;
}

unsigned long apb_read_reg(unsigned long addr)
{
	return demod_read_demod_reg(addr);
}

void apb_write_regb(unsigned long addr, int index, unsigned long data)
{
	/*to achieve write func*/
}

void enable_qam_int(int idx)
{
	unsigned long mask;

	mask = apb_read_reg(QAM_BASE + 0xd0);
	mask |= (1 << idx);
	apb_write_reg(QAM_BASE + 0xd0, mask);
}

void disable_qam_int(int idx)
{
	unsigned long mask;

	mask = apb_read_reg(QAM_BASE + 0xd0);
	mask &= ~(1 << idx);
	apb_write_reg(QAM_BASE + 0xd0, mask);
}

char *qam_int_name[] = { "      ADC",
	"   Symbol",
	"       RS",
	" In_Sync0",
	" In_Sync1",
	" In_Sync2",
	" In_Sync3",
	" In_Sync4",
	"Out_Sync0",
	"Out_Sync1",
	"Out_Sync2",
	"Out_Sync3",
	"Out_Sync4",
	"In_SyncCo",
	"OutSyncCo",
	"  In_Dagc",
	" Out_Dagc",
	"  Eq_Mode",
	"RS_Uncorr"
};

#define OFDM_INT_STS         0
#define OFDM_INT_EN          0

void enable_ofdm_int(int ofdm_irq)
{

}

void disable_ofdm_int(int ofdm_irq)
{

}

char *ofdm_int_name[] = { "PFS_FCFO",
	"PFS_ICFO",
	" CS_FCFO",
	" PFS_SFO",
	" PFS_TPS",
	"      SP",
	"     CCI",
	"  Symbol",
	" In_Sync",
	"Out_Sync",
	"FSM Stat"
};

unsigned long read_ofdm_int(void)
{

	return 0;
}

#define PHS_LOOP_OPEN

void qam_read_all_regs(void)
{

}

void ini_icfo_pn_index(int mode)
{				/* 00:DVBT,01:ISDBT */
	if (mode == 0) {
		apb_write_reg(DVBT_BASE + 0x3f8, 0x00000031);
		apb_write_reg(DVBT_BASE + 0x3fc, 0x00030000);
		apb_write_reg(DVBT_BASE + 0x3f8, 0x00000032);
		apb_write_reg(DVBT_BASE + 0x3fc, 0x00057036);
		apb_write_reg(DVBT_BASE + 0x3f8, 0x00000033);
		apb_write_reg(DVBT_BASE + 0x3fc, 0x0009c08d);
		apb_write_reg(DVBT_BASE + 0x3f8, 0x00000034);
		apb_write_reg(DVBT_BASE + 0x3fc, 0x000c90c0);
		apb_write_reg(DVBT_BASE + 0x3f8, 0x00000035);
		apb_write_reg(DVBT_BASE + 0x3fc, 0x001170ff);
		apb_write_reg(DVBT_BASE + 0x3f8, 0x00000036);
		apb_write_reg(DVBT_BASE + 0x3fc, 0x0014d11a);
	} else if (mode == 1) {
		apb_write_reg(DVBT_BASE + 0x3f8, 0x00000031);
		apb_write_reg(DVBT_BASE + 0x3fc, 0x00085046);
		apb_write_reg(DVBT_BASE + 0x3f8, 0x00000032);
		apb_write_reg(DVBT_BASE + 0x3fc, 0x0019a0e9);
		apb_write_reg(DVBT_BASE + 0x3f8, 0x00000033);
		apb_write_reg(DVBT_BASE + 0x3fc, 0x0024b1dc);
		apb_write_reg(DVBT_BASE + 0x3f8, 0x00000034);
		apb_write_reg(DVBT_BASE + 0x3fc, 0x003b3313);
		apb_write_reg(DVBT_BASE + 0x3f8, 0x00000035);
		apb_write_reg(DVBT_BASE + 0x3fc, 0x0048d409);
		apb_write_reg(DVBT_BASE + 0x3f8, 0x00000036);
		apb_write_reg(DVBT_BASE + 0x3fc, 0x00527509);
	}
}

static int coef[] = {
	0xf900, 0xfe00, 0x0000, 0x0000, 0x0100, 0x0100, 0x0000, 0x0000,
	0xfd00, 0xf700, 0x0000, 0x0000, 0x4c00, 0x0000, 0x0000, 0x0000,
	0x2200, 0x0c00, 0x0000, 0x0000, 0xf700, 0xf700, 0x0000, 0x0000,
	0x0300, 0x0900, 0x0000, 0x0000, 0x0600, 0x0600, 0x0000, 0x0000,
	0xfc00, 0xf300, 0x0000, 0x0000, 0x2e00, 0x0000, 0x0000, 0x0000,
	0x3900, 0x1300, 0x0000, 0x0000, 0xfa00, 0xfa00, 0x0000, 0x0000,
	0x0100, 0x0200, 0x0000, 0x0000, 0xf600, 0x0000, 0x0000, 0x0000,
	0x0700, 0x0700, 0x0000, 0x0000, 0xfe00, 0xfb00, 0x0000, 0x0000,
	0x0900, 0x0000, 0x0000, 0x0000, 0x3200, 0x1100, 0x0000, 0x0000,
	0x0400, 0x0400, 0x0000, 0x0000, 0xfe00, 0xfb00, 0x0000, 0x0000,
	0x0e00, 0x0000, 0x0000, 0x0000, 0xfb00, 0xfb00, 0x0000, 0x0000,
	0x0100, 0x0200, 0x0000, 0x0000, 0xf400, 0x0000, 0x0000, 0x0000,
	0x3900, 0x1300, 0x0000, 0x0000, 0x1700, 0x1700, 0x0000, 0x0000,
	0xfc00, 0xf300, 0x0000, 0x0000, 0x0c00, 0x0000, 0x0000, 0x0000,
	0x0300, 0x0900, 0x0000, 0x0000, 0xee00, 0x0000, 0x0000, 0x0000,
	0x2200, 0x0c00, 0x0000, 0x0000, 0x2600, 0x2600, 0x0000, 0x0000,
	0xfd00, 0xf700, 0x0000, 0x0000, 0x0200, 0x0000, 0x0000, 0x0000,
	0xf900, 0xfe00, 0x0000, 0x0000, 0x0400, 0x0b00, 0x0000, 0x0000,
	0xf900, 0x0000, 0x0000, 0x0000, 0x0700, 0x0200, 0x0000, 0x0000,
	0x2100, 0x2100, 0x0000, 0x0000, 0x0200, 0x0700, 0x0000, 0x0000,
	0xf900, 0x0000, 0x0000, 0x0000, 0x0b00, 0x0400, 0x0000, 0x0000,
	0xfe00, 0xf900, 0x0000, 0x0000, 0x0200, 0x0000, 0x0000, 0x0000,
	0xf700, 0xfd00, 0x0000, 0x0000, 0x2600, 0x2600, 0x0000, 0x0000,
	0x0c00, 0x2200, 0x0000, 0x0000, 0xee00, 0x0000, 0x0000, 0x0000,
	0x0900, 0x0300, 0x0000, 0x0000, 0x0c00, 0x0000, 0x0000, 0x0000,
	0xf300, 0xfc00, 0x0000, 0x0000, 0x1700, 0x1700, 0x0000, 0x0000,
	0x1300, 0x3900, 0x0000, 0x0000, 0xf400, 0x0000, 0x0000, 0x0000,
	0x0200, 0x0100, 0x0000, 0x0000, 0xfb00, 0xfb00, 0x0000, 0x0000,
	0x0e00, 0x0000, 0x0000, 0x0000, 0xfb00, 0xfe00, 0x0000, 0x0000,
	0x0400, 0x0400, 0x0000, 0x0000, 0x1100, 0x3200, 0x0000, 0x0000,
	0x0900, 0x0000, 0x0000, 0x0000, 0xfb00, 0xfe00, 0x0000, 0x0000,
	0x0700, 0x0700, 0x0000, 0x0000, 0xf600, 0x0000, 0x0000, 0x0000,
	0x0200, 0x0100, 0x0000, 0x0000, 0xfa00, 0xfa00, 0x0000, 0x0000,
	0x1300, 0x3900, 0x0000, 0x0000, 0x2e00, 0x0000, 0x0000, 0x0000,
	0xf300, 0xfc00, 0x0000, 0x0000, 0x0600, 0x0600, 0x0000, 0x0000,
	0x0900, 0x0300, 0x0000, 0x0000, 0xf700, 0xf700, 0x0000, 0x0000,
	0x0c00, 0x2200, 0x0000, 0x0000, 0x4c00, 0x0000, 0x0000, 0x0000,
	0xf700, 0xfd00, 0x0000, 0x0000, 0x0100, 0x0100, 0x0000, 0x0000,
	0xfe00, 0xf900, 0x0000, 0x0000, 0x0b00, 0x0400, 0x0000, 0x0000,
	0xfc00, 0xfc00, 0x0000, 0x0000, 0x0200, 0x0700, 0x0000, 0x0000,
	0x4200, 0x0000, 0x0000, 0x0000, 0x0700, 0x0200, 0x0000, 0x0000,
	0xfc00, 0xfc00, 0x0000, 0x0000, 0x0400, 0x0b00, 0x0000, 0x0000
};

void tfd_filter_coff_ini(void)
{
	int i = 0;

	for (i = 0; i < 336; i++) {
		apb_write_reg(DVBT_BASE + 0x99 * 4, (i << 16) | coef[i]);
		apb_write_reg(DVBT_BASE + 0x03 * 4, (1 << 12));
	}
}

void ofdm_initial(int bandwidth,
		/* 00:8M 01:7M 10:6M 11:5M */
		int samplerate,
		/* 00:45M 01:20.8333M 10:20.7M 11:28.57 100: 24.00 */
		int IF,
		/* 000:36.13M 001:-5.5M 010:4.57M 011:4M 100:5M */
		int mode,
		/* 00:DVBT,01:ISDBT */
		int tc_mode
		/* 0: Unsigned, 1:TC */
		)
{
#if 0
	int tmp;
	int ch_if;
	int adc_freq;
	pr_dbg
	    ("[ofdm_initial]bandwidth is %d,samplerate is %d",
	     bandwidth, samplerate);
	pr_dbg
	    ("IF is %d, mode is %d,tc_mode is %d\n",
	    IF, mode, tc_mode);
	switch (IF) {
	case 0:
		ch_if = 36130;
		break;
	case 1:
		ch_if = -5500;
		break;
	case 2:
		ch_if = 4570;
		break;
	case 3:
		ch_if = 4000;
		break;
	case 4:
		ch_if = 5000;
		break;
	default:
		ch_if = 4000;
		break;
	}
	switch (samplerate) {
	case 0:
		adc_freq = 45000;
		break;
	case 1:
		adc_freq = 20833;
		break;
	case 2:
		adc_freq = 20700;
		break;
	case 3:
		adc_freq = 28571;
		break;
	case 4:
		adc_freq = 24000;
		break;
	default:
		adc_freq = 28571;
		break;
	}

	apb_write_reg(DVBT_BASE + (0x02 << 2), 0x00800000);
	/* SW reset bit[23] ; write anything to zero */
	apb_write_reg(DVBT_BASE + (0x00 << 2), 0x00000000);

	apb_write_reg(DVBT_BASE + (0xe << 2), 0xffff);
	/* enable interrupt */

	if (mode == 0) {	/* DVBT */
		switch (samplerate) {
		case 0:
			apb_write_reg(DVBT_BASE + (0x08 << 2), 0x00005a00);
			break;	/* 45MHz */
		case 1:
			apb_write_reg(DVBT_BASE + (0x08 << 2), 0x000029aa);
			break;	/* 20.833 */
		case 2:
			apb_write_reg(DVBT_BASE + (0x08 << 2), 0x00002966);
			break;	/* 20.7   SAMPLERATE*512 */
		case 3:
			apb_write_reg(DVBT_BASE + (0x08 << 2), 0x00003924);
			break;	/* 28.571 */
		case 4:
			apb_write_reg(DVBT_BASE + (0x08 << 2), 0x00003000);
			break;	/* 24 */
		default:
			apb_write_reg(DVBT_BASE + (0x08 << 2), 0x00003924);
			break;	/* 28.571 */
		}
	} else {		/* ISDBT */
		switch (samplerate) {
		case 0:
			apb_write_reg(DVBT_BASE + (0x08 << 2), 0x0000580d);
			break;	/* 45MHz */
		case 1:
			apb_write_reg(DVBT_BASE + (0x08 << 2), 0x0000290d);
			break;	/* 20.833 = 56/7 * 20.8333 / (512/63)*512 */
		case 2:
			apb_write_reg(DVBT_BASE + (0x08 << 2), 0x000028da);
			break;	/* 20.7 */
		case 3:
			apb_write_reg(DVBT_BASE + (0x08 << 2), 0x0000383F);
			break;	/* 28.571  3863 */
		case 4:
			apb_write_reg(DVBT_BASE + (0x08 << 2), 0x00002F40);
			break;	/* 24 */
		default:
			apb_write_reg(DVBT_BASE + (0x08 << 2), 0x00003863);
			break;	/* 28.571 */
		}
	}
/* memstart=0x93900000; */
	pr_dbg("memstart is %x\n", memstart);
	apb_write_reg(DVBT_BASE + (0x10 << 2), memstart);
	/* 0x8f300000 */

	apb_write_reg(DVBT_BASE + (0x14 << 2), 0xe81c4ff6);
	/* AGC_TARGET 0xf0121385 */

	switch (samplerate) {
	case 0:
		apb_write_reg(DVBT_BASE + (0x15 << 2), 0x018c2df2);
		break;
	case 1:
		apb_write_reg(DVBT_BASE + (0x15 << 2), 0x0185bdf2);
		break;
	case 2:
		apb_write_reg(DVBT_BASE + (0x15 << 2), 0x0185bdf2);
		break;
	case 3:
		apb_write_reg(DVBT_BASE + (0x15 << 2), 0x0187bdf2);
		break;
	case 4:
		apb_write_reg(DVBT_BASE + (0x15 << 2), 0x0187bdf2);
		break;
	default:
		apb_write_reg(DVBT_BASE + (0x15 << 2), 0x0187bdf2);
		break;
	}
	if (tc_mode == 1)
		apb_write_regb(DVBT_BASE + (0x15 << 2), 11, 0);
	/* For TC mode. Notice, For ADC input is Unsigned,
	For Capture Data, It is TC. */
	apb_write_regb(DVBT_BASE + (0x15 << 2), 26, 1);
	/* [19:0] = [I , Q], I is high, Q is low. This bit is swap I/Q. */

	apb_write_reg(DVBT_BASE + (0x16 << 2), 0x00047f80);
	/* AGC_IFGAIN_CTRL */
	apb_write_reg(DVBT_BASE + (0x17 << 2), 0x00027f80);
	/* AGC_RFGAIN_CTRL */
	apb_write_reg(DVBT_BASE + (0x18 << 2), 0x00000190);
	/* AGC_IFGAIN_ACCUM */
	apb_write_reg(DVBT_BASE + (0x19 << 2), 0x00000190);
	/* AGC_RFGAIN_ACCUM */
	if (ch_if < 0)
		ch_if += adc_freq;
	if (ch_if > adc_freq)
		ch_if -= adc_freq;

	tmp = ch_if * (1 << 15) / adc_freq;
	apb_write_reg(DVBT_BASE + (0x20 << 2), tmp);

	apb_write_reg(DVBT_BASE + (0x21 << 2), 0x001ff000);
	/* DDC CS_FCFO_ADJ_CTRL */
	apb_write_reg(DVBT_BASE + (0x22 << 2), 0x00000000);
	/* DDC ICFO_ADJ_CTRL */
	apb_write_reg(DVBT_BASE + (0x23 << 2), 0x00004000);
	/* DDC TRACK_FCFO_ADJ_CTRL */

	apb_write_reg(DVBT_BASE + (0x27 << 2), (1 << 23)
	| (3 << 19) | (3 << 15) |  (1000 << 4) | 9);
	/* {8'd0,1'd1,4'd3,4'd3,11'd50,4'd9});//FSM_1 */
	apb_write_reg(DVBT_BASE + (0x28 << 2), (100 << 13) | 1000);
	/* {8'd0,11'd40,13'd50});//FSM_2 */
	apb_write_reg(DVBT_BASE + (0x29 << 2), (31 << 20) | (1 << 16) |
	(24 << 9) | (3 << 6) | 20);
	/* {5'd0,7'd127,1'd0,3'd0,7'd24,3'd5,6'd20}); */

	if (mode == 0) {	/* DVBT */
		if (bandwidth == 0) {	/* 8M */
			switch (samplerate) {
			case 0:
				ini_acf_iireq_src_45m_8m();
				apb_write_reg(DVBT_BASE + (0x44 << 2),
					      0x004ebf2e);
				break;	/* 45M */
			case 1:
				ini_acf_iireq_src_207m_8m();
				apb_write_reg(DVBT_BASE + (0x44 << 2),
					      0x00247551);
				break;	/* 20.833M */
			case 2:
				ini_acf_iireq_src_207m_8m();
				apb_write_reg(DVBT_BASE + (0x44 << 2),
					      0x00243999);
				break;	/* 20.7M */
			case 3:
				ini_acf_iireq_src_2857m_8m();
				apb_write_reg(DVBT_BASE + (0x44 << 2),
					      0x0031ffcd);
				break;	/* 28.57M */
			case 4:
				ini_acf_iireq_src_24m_8m();
				apb_write_reg(DVBT_BASE + (0x44 << 2),
					      0x002A0000);
				break;	/* 24M */
			default:
				ini_acf_iireq_src_2857m_8m();
				apb_write_reg(DVBT_BASE + (0x44 << 2),
					      0x0031ffcd);
				break;	/* 28.57M */
			}
		} else if (bandwidth == 1) {	/* 7M */
			switch (samplerate) {
			case 0:
				ini_acf_iireq_src_45m_7m();
				apb_write_reg(DVBT_BASE + (0x44 << 2),
					      0x0059ff10);
				break;	/* 45M */
			case 1:
				ini_acf_iireq_src_207m_7m();
				apb_write_reg(DVBT_BASE + (0x44 << 2),
					      0x0029aaa6);
				break;	/* 20.833M */
			case 2:
				ini_acf_iireq_src_207m_7m();
				apb_write_reg(DVBT_BASE + (0x44 << 2),
					      0x00296665);
				break;	/* 20.7M */
			case 3:
				ini_acf_iireq_src_2857m_7m();
				apb_write_reg(DVBT_BASE + (0x44 << 2),
					      0x00392491);
				break;	/* 28.57M */
			case 4:
				ini_acf_iireq_src_24m_7m();
				apb_write_reg(DVBT_BASE + (0x44 << 2),
					      0x00300000);
				break;	/* 24M */
			default:
				ini_acf_iireq_src_2857m_7m();
				apb_write_reg(DVBT_BASE + (0x44 << 2),
					      0x00392491);
				break;	/* 28.57M */
			}
		} else if (bandwidth == 2) {	/* 6M */
			switch (samplerate) {
			case 0:
				ini_acf_iireq_src_45m_6m();
				apb_write_reg(DVBT_BASE + (0x44 << 2),
					      0x00690000);
				break;	/* 45M */
			case 1:
				ini_acf_iireq_src_207m_6m();
				apb_write_reg(DVBT_BASE + (0x44 << 2),
					      0x00309c3e);
				break;	/* 20.833M */
			case 2:
				ini_acf_iireq_src_207m_6m();
				apb_write_reg(DVBT_BASE + (0x44 << 2),
					      0x002eaaaa);
				break;	/* 20.7M */
			case 3:
				ini_acf_iireq_src_2857m_6m();
				apb_write_reg(DVBT_BASE + (0x44 << 2),
					      0x0042AA69);
				break;	/* 28.57M */
			case 4:
				ini_acf_iireq_src_24m_6m();
				apb_write_reg(DVBT_BASE + (0x44 << 2),
					      0x00380000);
				break;	/* 24M */
			default:
				ini_acf_iireq_src_2857m_6m();
				apb_write_reg(DVBT_BASE + (0x44 << 2),
					      0x0042AA69);
				break;	/* 28.57M */
			}
		} else {	/* 5M */
			switch (samplerate) {
			case 0:
				ini_acf_iireq_src_45m_5m();
				apb_write_reg(DVBT_BASE + (0x44 << 2),
					      0x007dfbe0);
				break;	/* 45M */
			case 1:
				ini_acf_iireq_src_207m_5m();
				apb_write_reg(DVBT_BASE + (0x44 << 2),
					      0x003a554f);
				break;	/* 20.833M */
			case 2:
				ini_acf_iireq_src_207m_5m();
				apb_write_reg(DVBT_BASE + (0x44 << 2),
					      0x0039f5c0);
				break;	/* 20.7M */
			case 3:
				ini_acf_iireq_src_2857m_5m();
				apb_write_reg(DVBT_BASE + (0x44 << 2),
					      0x004FFFFE);
				break;	/* 28.57M */
			case 4:
				ini_acf_iireq_src_24m_5m();
				apb_write_reg(DVBT_BASE + (0x44 << 2),
					      0x00433333);
				break;	/* 24M */
			default:
				ini_acf_iireq_src_2857m_5m();
				apb_write_reg(DVBT_BASE + (0x44 << 2),
					      0x004FFFFE);
				break;	/* 28.57M */
			}
		}
	} else {		/* ISDBT */
		switch (samplerate) {
		case 0:
			ini_acf_iireq_src_45m_6m();
			apb_write_reg(DVBT_BASE + (0x44 << 2), 0x00589800);
			break;	/* 45M
			SampleRate/(symbolRate)*2^20,
			symbolRate = 512/63 for isdbt */
		case 1:
			ini_acf_iireq_src_207m_6m();
			apb_write_reg(DVBT_BASE + (0x44 << 2), 0x002903d4);
			break;	/* 20.833M */
		case 2:
			ini_acf_iireq_src_207m_6m();
			apb_write_reg(DVBT_BASE + (0x44 << 2), 0x00280ccc);
			break;	/* 20.7M */
		case 3:
			ini_acf_iireq_src_2857m_6m();
			apb_write_reg(DVBT_BASE + (0x44 << 2), 0x00383fc8);
			break;	/* 28.57M */
		case 4:
			ini_acf_iireq_src_24m_6m();
			apb_write_reg(DVBT_BASE + (0x44 << 2), 0x002F4000);
			break;	/* 24M */
		default:
			ini_acf_iireq_src_2857m_6m();
			apb_write_reg(DVBT_BASE + (0x44 << 2), 0x00383fc8);
			break;	/* 28.57M */
		}
	}

	if (mode == 0)		/* DVBT */
		apb_write_reg(DVBT_BASE + (0x02 << 2),
			      (bandwidth << 20) | 0x10002);
	else			/* ISDBT */
		apb_write_reg(DVBT_BASE + (0x02 << 2), (1 << 20) | 0x1001a);
	/* {0x000,2'h1,20'h1_001a});    // For ISDBT , bandwith should be 1, */

	apb_write_reg(DVBT_BASE + (0x45 << 2), 0x00000000);
	/* SRC SFO_ADJ_CTRL */
	apb_write_reg(DVBT_BASE + (0x46 << 2), 0x02004000);
	/* SRC SFO_ADJ_CTRL */
	apb_write_reg(DVBT_BASE + (0x48 << 2), 0x000c0287);
	/* DAGC_CTRL1 */
	apb_write_reg(DVBT_BASE + (0x49 << 2), 0x00000005);
	/* DAGC_CTRL2 */
	apb_write_reg(DVBT_BASE + (0x4c << 2), 0x00000bbf);
	/* CCI_RP */
	apb_write_reg(DVBT_BASE + (0x4d << 2), 0x00000376);
	/* CCI_RPSQ */
	apb_write_reg(DVBT_BASE + (0x4e << 2), 0x0f0f1d09);
	/* CCI_CTRL */
	apb_write_reg(DVBT_BASE + (0x4f << 2), 0x00000000);
	/* CCI DET_INDX1 */
	apb_write_reg(DVBT_BASE + (0x50 << 2), 0x00000000);
	/* CCI DET_INDX2 */
	apb_write_reg(DVBT_BASE + (0x51 << 2), 0x00000000);
	/* CCI_NOTCH1_A1 */
	apb_write_reg(DVBT_BASE + (0x52 << 2), 0x00000000);
	/* CCI_NOTCH1_A2 */
	apb_write_reg(DVBT_BASE + (0x53 << 2), 0x00000000);
	/* CCI_NOTCH1_B1 */
	apb_write_reg(DVBT_BASE + (0x54 << 2), 0x00000000);
	/* CCI_NOTCH2_A1 */
	apb_write_reg(DVBT_BASE + (0x55 << 2), 0x00000000);
	/* CCI_NOTCH2_A2 */
	apb_write_reg(DVBT_BASE + (0x56 << 2), 0x00000000);
	/* CCI_NOTCH2_B1 */
	apb_write_reg(DVBT_BASE + (0x58 << 2), 0x00000885);
	/* MODE_DETECT_CTRL // 582 */
	if (mode == 0)		/* DVBT */
		apb_write_reg(DVBT_BASE + (0x5c << 2), 0x00001011);	/*  */
	else
		apb_write_reg(DVBT_BASE + (0x5c << 2), 0x00000753);
	/* ICFO_EST_CTRL ISDBT ICFO thres = 2 */

	apb_write_reg(DVBT_BASE + (0x5f << 2), 0x0ffffe10);
	/* TPS_FCFO_CTRL */
	apb_write_reg(DVBT_BASE + (0x61 << 2), 0x0000006c);
	/* FWDT ctrl */
	apb_write_reg(DVBT_BASE + (0x68 << 2), 0x128c3929);
	apb_write_reg(DVBT_BASE + (0x69 << 2), 0x91017f2d);
	/* 0x1a8 */
	apb_write_reg(DVBT_BASE + (0x6b << 2), 0x00442211);
	/* 0x1a8 */
	apb_write_reg(DVBT_BASE + (0x6c << 2), 0x01fc400a);
	/* 0x */
	apb_write_reg(DVBT_BASE + (0x6d << 2), 0x0030303f);
	/* 0x */
	apb_write_reg(DVBT_BASE + (0x73 << 2), 0xffffffff);
	/* CCI0_PILOT_UPDATE_CTRL */
	apb_write_reg(DVBT_BASE + (0x74 << 2), 0xffffffff);
	/* CCI0_DATA_UPDATE_CTRL */
	apb_write_reg(DVBT_BASE + (0x75 << 2), 0xffffffff);
	/* CCI1_PILOT_UPDATE_CTRL */
	apb_write_reg(DVBT_BASE + (0x76 << 2), 0xffffffff);
	/* CCI1_DATA_UPDATE_CTRL */

	tmp = mode == 0 ? 0x000001a2 : 0x00000da2;
	apb_write_reg(DVBT_BASE + (0x78 << 2), tmp);	/* FEC_CTR */

	apb_write_reg(DVBT_BASE + (0x7d << 2), 0x0000009d);
	apb_write_reg(DVBT_BASE + (0x7e << 2), 0x00004000);
	apb_write_reg(DVBT_BASE + (0x7f << 2), 0x00008000);

	apb_write_reg(DVBT_BASE + ((0x8b + 0) << 2), 0x20002000);
	apb_write_reg(DVBT_BASE + ((0x8b + 1) << 2), 0x20002000);
	apb_write_reg(DVBT_BASE + ((0x8b + 2) << 2), 0x20002000);
	apb_write_reg(DVBT_BASE + ((0x8b + 3) << 2), 0x20002000);
	apb_write_reg(DVBT_BASE + ((0x8b + 4) << 2), 0x20002000);
	apb_write_reg(DVBT_BASE + ((0x8b + 5) << 2), 0x20002000);
	apb_write_reg(DVBT_BASE + ((0x8b + 6) << 2), 0x20002000);
	apb_write_reg(DVBT_BASE + ((0x8b + 7) << 2), 0x20002000);

	apb_write_reg(DVBT_BASE + (0x93 << 2), 0x31);
	apb_write_reg(DVBT_BASE + (0x94 << 2), 0x00);
	apb_write_reg(DVBT_BASE + (0x95 << 2), 0x7f1);
	apb_write_reg(DVBT_BASE + (0x96 << 2), 0x20);

	apb_write_reg(DVBT_BASE + (0x98 << 2), 0x03f9115a);
	apb_write_reg(DVBT_BASE + (0x9b << 2), 0x000005df);

	apb_write_reg(DVBT_BASE + (0x9c << 2), 0x00100000);
	/* TestBus write valid, 0 is system clk valid */
	apb_write_reg(DVBT_BASE + (0x9d << 2), 0x01000000);
	/* DDR Start address */
	apb_write_reg(DVBT_BASE + (0x9e << 2), 0x02000000);
	/* DDR End   address */

	apb_write_regb(DVBT_BASE + (0x9b << 2), 7, 0);
	/* Enable Testbus dump to DDR */
	apb_write_regb(DVBT_BASE + (0x9b << 2), 8, 0);
	/* Run Testbus dump to DDR */

	apb_write_reg(DVBT_BASE + (0xd6 << 2), 0x00000003);
	/* apb_write_reg(DVBT_BASE+(0xd7<<2), 0x00000008); */
	apb_write_reg(DVBT_BASE + (0xd8 << 2), 0x00000120);
	apb_write_reg(DVBT_BASE + (0xd9 << 2), 0x01010101);

	ini_icfo_pn_index(mode);
	tfd_filter_coff_ini();

	calculate_cordic_para();
	msleep(20);
	/* delay_us(1); */

	apb_write_reg(DVBT_BASE + (0x02 << 2),
		      apb_read_reg(DVBT_BASE + (0x02 << 2)) | (1 << 0));
	apb_write_reg(DVBT_BASE + (0x02 << 2),
		      apb_read_reg(DVBT_BASE + (0x02 << 2)) | (1 << 24));
#endif
/* dvbt_check_status(); */
}

void calculate_cordic_para(void)
{
	apb_write_reg(DVBT_BASE + 0x0c, 0x00000040);
}

char *ofdm_fsm_name[] = { "    IDLE",
	"     AGC",
	"     CCI",
	"     ACQ",
	"    SYNC",
	"TRACKING",
	"  TIMING",
	" SP_SYNC",
	" TPS_DEC",
	"FEC_LOCK",
	"FEC_LOST"
};

void check_fsm_state(void)
{
	unsigned long tmp;

	tmp = apb_read_reg(DVBT_BASE + 0xa8);
	/* printk(">>>>>>>>>>>>>>>>>>>>>>>>> OFDM FSM From %d
	to %d\n", tmp>>4&0xf, tmp&0xf); */

	if ((tmp & 0xf) == 3) {
		apb_write_regb(DVBT_BASE + (0x9b << 2), 8, 1);
		/* Stop dump testbus; */
		apb_write_regb(DVBT_BASE + (0x0f << 2), 0, 1);
		tmp = apb_read_reg(DVBT_BASE + (0x9f << 2));
		/* printk(">>>>>>>>>>>>>>>>>>>>>>>>> STOP DUMP DATA To DDR :
		End Addr %d,Is it overflow?%d\n", tmp>>1, tmp&0x1); */
	}
}

void ofdm_read_all_regs(void)
{
	int i;
	unsigned long tmp;

	for (i = 0; i < 0xff; i++)
		tmp = apb_read_reg(DVBT_BASE + 0x00 + i * 4);
	/* printk("OFDM Reg (0x%x) is 0x%x\n", i, tmp); */

}

static int dvbt_get_status(struct aml_demod_sta *demod_sta,
			   struct aml_demod_i2c *demod_i2c)
{
	return apb_read_reg(DVBT_BASE + 0x0) >> 12 & 1;
}

static int dvbt_get_ber(struct aml_demod_sta *demod_sta,
			struct aml_demod_i2c *demod_i2c)
{
/* pr_dbg("[RSJ]per is %u\n",apb_read_reg(DVBT_BASE+(0xbf<<2))); */
	return apb_read_reg(DVBT_BASE + (0xbf << 2));
}

static int dvbt_get_snr(struct aml_demod_sta *demod_sta,
			struct aml_demod_i2c *demod_i2c)
{
/* pr_dbg("2snr is %u\n",((apb_read_reg(DVBT_BASE+(0x0a<<2)))>>20)&0x3ff); */
	return ((apb_read_reg(DVBT_BASE + (0x0a << 2))) >> 20) & 0x3ff;
	/*dBm: bit0~bit2=decimal */
}

static int dvbt_get_strength(struct aml_demod_sta *demod_sta,
			     struct aml_demod_i2c *demod_i2c)
{
/* int dbm = dvbt_get_ch_power(demod_sta, demod_i2c); */
/* return dbm; */
	return 0;
}

static int dvbt_get_ucblocks(struct aml_demod_sta *demod_sta,
			     struct aml_demod_i2c *demod_i2c)
{
	return 0;
/* return dvbt_get_per(); */
}

struct demod_status_ops *dvbt_get_status_ops(void)
{
	static struct demod_status_ops ops = {
		.get_status = dvbt_get_status,
		.get_ber = dvbt_get_ber,
		.get_snr = dvbt_get_snr,
		.get_strength = dvbt_get_strength,
		.get_ucblocks = dvbt_get_ucblocks,
	};

	return &ops;
}

int app_apb_read_reg(int addr)
{
	addr = DTMB_TOP_ADDR(addr);
	return (int)demod_read_demod_reg(addr);
}

int app_apb_write_reg(int addr, int data)
{
	addr = DTMB_TOP_ADDR(addr);
	demod_set_demod_reg(data, addr);
	return 0;
}

void monitor_isdbt(void)
{
	int SNR;
	int SNR_SP = 500;
	int SNR_TPS = 0;
	int SNR_CP = 0;
	int timeStamp = 0;
	int SFO_residual = 0;
	int SFO_esti = 0;
	int FCFO_esti = 0;
	int FCFO_residual = 0;
	int AGC_Gain = 0;
	int RF_AGC = 0;
	int Signal_power = 0;
	int FECFlag = 0;
	int EQ_seg_ratio = 0;
	int tps_0 = 0;
	int tps_1 = 0;
	int tps_2 = 0;

	int time_stamp;
	int SFO;
	int FCFO;
	int timing_adj;
	int RS_CorrectNum;

	int cnt;
	int tmpAGCGain;

	tmpAGCGain = 0;
	cnt = 0;

/* app_apb_write_reg(0x8, app_apb_read_reg(0x8) & ~(1 << 17));
// TPS symbol index update : active high */
	time_stamp = app_apb_read_reg(0x07) & 0xffff;
	SNR = app_apb_read_reg(0x0a);
	FECFlag = (app_apb_read_reg(0x00) >> 11) & 0x3;
	SFO = app_apb_read_reg(0x47) & 0xfff;
	SFO_esti = app_apb_read_reg(0x60) & 0xfff;
	FCFO_esti = (app_apb_read_reg(0x60) >> 11) & 0xfff;
	FCFO = (app_apb_read_reg(0x26)) & 0xffffff;
	RF_AGC = app_apb_read_reg(0x0c) & 0x1fff;
	timing_adj = app_apb_read_reg(0x6f) & 0x1fff;
	RS_CorrectNum = app_apb_read_reg(0xc1) & 0xfffff;
	Signal_power = (app_apb_read_reg(0x1b)) & 0x1ff;
	EQ_seg_ratio = app_apb_read_reg(0x6e) & 0x3ffff;
	tps_0 = app_apb_read_reg(0x64);
	tps_1 = app_apb_read_reg(0x65);
	tps_2 = app_apb_read_reg(0x66) & 0xf;

	timeStamp = (time_stamp >> 8) * 68 + (time_stamp & 0x7f);
	SFO_residual = (SFO > 0x7ff) ? (SFO - 0x1000) : SFO;
	FCFO_residual = (FCFO > 0x7fffff) ? (FCFO - 0x1000000) : FCFO;
	/* RF_AGC          = (RF_AGC>0x3ff)? (RF_AGC - 0x800): RF_AGC; */
	FCFO_esti = (FCFO_esti > 0x7ff) ? (FCFO_esti - 0x1000) : FCFO_esti;
	SNR_CP = (SNR) & 0x3ff;
	SNR_TPS = (SNR >> 10) & 0x3ff;
	SNR_SP = (SNR >> 20) & 0x3ff;
	SNR_SP = (SNR_SP > 0x1ff) ? SNR_SP - 0x400 : SNR_SP;
	SNR_TPS = (SNR_TPS > 0x1ff) ? SNR_TPS - 0x400 : SNR_TPS;
	SNR_CP = (SNR_CP > 0x1ff) ? SNR_CP - 0x400 : SNR_CP;
	AGC_Gain = tmpAGCGain >> 4;
	tmpAGCGain = (AGC_Gain > 0x3ff) ? AGC_Gain - 0x800 : AGC_Gain;
	timing_adj = (timing_adj > 0xfff) ? timing_adj - 0x2000 : timing_adj;
	EQ_seg_ratio =
	    (EQ_seg_ratio > 0x1ffff) ? EQ_seg_ratio - 0x40000 : EQ_seg_ratio;

	pr_dbg
	    ("T %4x SP %3d TPS %3d CP %3d EQS %8x RSC %4d",
	     app_apb_read_reg(0xbf)
	     , SNR_SP, SNR_TPS, SNR_CP
/* ,EQ_seg_ratio */
	     , app_apb_read_reg(0x62)
	     , RS_CorrectNum);
	pr_dbg
	    ("SFO %4d FCFO %4d Vit %4x Timing %3d SigP %3x",
	    SFO_residual, FCFO_residual, RF_AGC, timing_adj,
	     Signal_power);
	pr_dbg
	    ("FEC %x RSErr %8x ReSyn %x tps %03x%08x",
	    FECFlag, app_apb_read_reg(0x0b)
	     , (app_apb_read_reg(0xc0) >> 20) & 0xff,
	     app_apb_read_reg(0x05) & 0xfff, app_apb_read_reg(0x04)
	    );
	pr_dbg("\n");
}

int find_2(int data, int *table, int len)
{
	int end;
	int index;
	int start;
	int cnt = 0;

	start = 0;
	end = len;
	/* printf("data is %d\n",data); */
	while ((len > 1) && (cnt < 10)) {
		cnt++;
		index = (len / 2);
		if (data > table[start + index]) {
			start = start + index;
			len = len - index - 1;
		}
		if (data < table[start + index]) {
			len = index + 1;
		} else if (data == table[start + index]) {
			start = start + index;
			break;
		}
	}
	return start;
}

int read_atsc_all_reg(void)
{
	return 0;
#if 0
	int i, j, k;
	j = 4;
	unsigned long data;
	pr_dbg("system agc is:");	/* system agc */
	for (i = 0xc00; i <= 0xc0c; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0xc80; i <= 0xc87; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	pr_dbg("\n vsb control is:");	/*vsb control */
	j = 4;
	for (i = 0x900; i <= 0x905; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x908; i <= 0x912; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x917; i <= 0x91b; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x980; i <= 0x992; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	pr_dbg("\n vsb demod is:");	/*vsb demod */
	j = 4;
	for (i = 0x700; i <= 0x711; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x716; i <= 0x720; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x722; i <= 0x724; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x726; i <= 0x72c; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x730; i <= 0x732; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x735; i <= 0x751; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x780; i <= 0x795; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x752; i <= 0x755; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	pr_dbg("\n vsb equalizer is:");	/*vsb equalizer */
	j = 4;
	for (i = 0x501; i <= 0x5ff; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	pr_dbg("\n vsb fec is:");	/*vsb fec */
	j = 4;
	for (i = 0x601; i <= 0x601; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x682; i <= 0x685; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	pr_dbg("\n qam demod is:");	/*qam demod */
	j = 4;
	for (i = 0x1; i <= 0x1a; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x25; i <= 0x28; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x101; i <= 0x10b; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x206; i <= 0x207; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	pr_dbg("\n qam equalize is:");	/*qam equalize */
	j = 4;
	for (i = 0x200; i <= 0x23d; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	j = 4;
	for (i = 0x260; i <= 0x275; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	pr_dbg("\n qam fec is:");	/*qam fec */
	j = 4;
	for (i = 0x400; i <= 0x418; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	pr_dbg("\n system mpeg formatter is:");	/*system mpeg formatter */
	j = 4;
	for (i = 0xf00; i <= 0xf09; i++) {
		data = atsc_read_reg(i);
		if (j == 4) {
			pr_dbg("\n[addr:0x%x]", i);
			j = 0;
		}
		pr_dbg("%02x   ", data);
		j++;
	}
	pr_dbg("\n\n");
	return 0;
#endif
}

int check_atsc_fsm_status(void)
{
	int SNR;
	int atsc_snr = 0;
	int SNR_dB;
	int SNR_table[56] = { 0, 7, 9, 11, 14,
		17,
		22,
		27, 34, 43, 54,
		68, 86, 108, 136, 171,
		215,
		271, 341,
		429, 540,
		566, 592, 620, 649, 680,
		712,
		746, 781,
		818, 856,
		896, 939, 983, 1029, 1078,
		1182,
		1237,
		1237, 1296, 1357,
		1708, 2150, 2707, 3408, 4291,
		5402,
		6800,
		8561, 10778, 13568,
		16312, 17081, 18081, 19081, 65536
	};
	int SNR_dB_table[56] = { 360, 350, 340, 330, 320, 310, 300,
		290,
		280,
		270, 260,
		250, 240, 230, 220, 210, 200, 190,
		180,
		170,
		160,
		158, 156, 154, 152, 150, 148, 146,
		144,
		142,
		140,
		138, 136, 134, 132, 130, 128, 126,
		124,
		122,
		120,
		110, 100, 90, 80, 70, 60, 50,
		40,
		30,
		20,
		12, 10, 4, 2, 0
	};

	int tmp[3];
	int cr;
	int ck;
	int SM;
	int tni;
	int ber;
	int per;

	int cnt;

	cnt = 0;
	ber = 0;
	per = 0;

/* g_demod_mode    = 2; */
	tni = atsc_read_reg((0x08) >> 16);
/* g_demod_mode    = 4; */
	tmp[0] = atsc_read_reg(0x0511);
	tmp[1] = atsc_read_reg(0x0512);
	SNR = (tmp[0] << 8) + tmp[1];
	SNR_dB = SNR_dB_table[find_2(SNR, SNR_table, 56)];

	tmp[0] = atsc_read_reg(0x0780);
	tmp[1] = atsc_read_reg(0x0781);
	tmp[2] = atsc_read_reg(0x0782);
	cr = tmp[0] + (tmp[1] << 8) + (tmp[2] << 16);
	tmp[0] = atsc_read_reg(0x0786);
	tmp[1] = atsc_read_reg(0x0787);
	tmp[2] = atsc_read_reg(0x0788);
	ck = (tmp[0] << 16) + (tmp[1] << 8) + tmp[2];
	ck = (ck > 8388608) ? ck - 16777216 : ck;
	SM = atsc_read_reg(0x0980);
/* ber per */
	atsc_write_reg(0x0601, atsc_read_reg(0x0601) & (~(1 << 3)));
	atsc_write_reg(0x0601, atsc_read_reg(0x0601) | (1 << 3));
	ber = atsc_read_reg(0x0683) + (atsc_read_reg(0x0682) << 8);
	per = atsc_read_reg(0x0685) + (atsc_read_reg(0x0684) << 8);

/* read_atsc_all_reg(); */

	pr_dbg
	    ("INT %x SNR %x SNRdB %d.%d FSM %x cr %d ck %d",
	     tni, SNR, (SNR_dB / 10)
	     , (SNR_dB - (SNR_dB / 10) * 10)
	     , SM, cr, ck);
	pr_dbg
		("ber is %d, per is %d\n",
		ber, per);

	atsc_snr = (SNR_dB / 10);
	return atsc_snr;

	/*   unsigned long sm,snr1,snr2,snr;
	 * static int fec_lock_cnt = 0;
	 *
	 * delay_us(10000);
	 * sm = atsc_read_reg(0x0980);
	 * snr1 = atsc_read_reg(0x0511)&0xff;
	 * snr2 = atsc_read_reg(0x0512)&0xff;
	 * snr  = (snr1 << 8) + snr2;
	 *
	 * printk(">>>>>>>>>>>>>>>>>>>>>>>>>
	 OFDM FSM %x    SNR %x\n", sm&0xff, snr);
	 *
	 * if (sm == 0x79) stimulus_finish_pass();*/
}
