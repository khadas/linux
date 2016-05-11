#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/amlogic/cpu_version.h>
#include "common.h"
#include "mach_reg.h"
#include "mach_reg_gxtvbb.h"
#include "hw_clk.h"


/*
 * HDMITX Clock configuration
 */

static inline int check_div(unsigned int div)
{
	if (div == -1)
		return -1;
	switch (div) {
	case 1:
		div = 0;
		break;
	case 2:
		div = 1;
		break;
	case 4:
		div = 2;
		break;
	case 6:
		div = 3;
		break;
	case 12:
		div = 4;
		break;
	default:
		break;
	}
	return div;
}

#define WAIT_FOR_PLL_LOCKED(reg)                        \
	do {                                                \
		unsigned int cnt = 10;                          \
		unsigned int time_out = 0;                      \
		while (cnt--) {                                 \
			time_out = 0;                               \
			while ((!(hd_read_reg(reg) & (1 << 31)))\
				& (time_out < 10000))               \
				time_out++;                            \
			}                                               \
		if (cnt < 9)                                     \
			pr_info("pll[0x%x] reset %d times\n", reg, 9 - cnt);\
	} while (0)

static void set_hdmitx_sys_clk(void)
{
	hd_set_reg_bits(P_HHI_HDMI_CLK_CNTL, 0, 9, 3);
	hd_set_reg_bits(P_HHI_HDMI_CLK_CNTL, 0, 0, 7);
	hd_set_reg_bits(P_HHI_HDMI_CLK_CNTL, 1, 8, 1);
}

static void set_gxb_hpll_clk_out(unsigned clk)
{
	switch (clk) {
	case 5940000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x5800027b);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 0x4c00, 0, 16);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x135c5091);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x801da72c);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x71486980);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x00000e55);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x0, 28, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL));
		break;
	case 3712500:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x5800024d);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 0x4580, 0, 16);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0d5c5091);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x801da72c);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x71486980);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x00000e55);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x4, 28, 3);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL));
		break;
	case 3450000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x58000247);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 0x4300, 0, 16);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0d5c5091);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x801da72c);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x71486980);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x00000e55);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x4, 28, 3);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL));
		break;
	case 2970000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x5800023d);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0d5c5091);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x801da72c);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x71486980);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x00000e55);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x4, 28, 3);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL));
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 0x4e00, 0, 16);
		break;
	case 4320000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x5800025a);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0d5c5091);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x801da72c);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x71486980);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x00000e55);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x0, 28, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL));
		break;
	default:
		pr_info("error hpll clk: %d\n", clk);
		break;
	}
}

static void set_gxtvbb_hpll_clk_out(unsigned clk)
{
	switch (clk) {
	case 5940000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x5800027b);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 0x4300, 0, 16);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x12dc5081);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x801da72c);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x71486980);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x00002e55);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x0, 28, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL));
		break;
	case 3712500:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x5800024d);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 0x4160, 0, 16);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0d5c5091);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x801da72c);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x71486980);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x00002e55);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x4, 28, 3);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL));
		break;
	case 3450000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x58000247);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 0x4300, 0, 16);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0d5c5091);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x801da72c);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x71486980);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x00002e55);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x4, 28, 3);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL));
		break;
	case 2970000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x5800023d);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 0x4380, 0, 16);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0d5c5091);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x801da72c);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x71486980);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x00002e55);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x4, 28, 3);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL));
		break;
	case 4320000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x5800025a);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0d5c5091);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x801da72c);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x71486980);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x00002e55);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x0, 28, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL));
		break;
	default:
		pr_info("error hpll clk: %d\n", clk);
		break;
	}
}

static void set_gxl_hpll_clk_out(unsigned clk)
{
	switch (clk) {
	case 5940000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x400002f7);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x800cb200);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x860f30c4);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x0c8e0000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x001fa729);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x01a31500);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x1, 28, 1);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x0, 28, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL));
		break;
	case 3712500:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x4000029a);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x800cb2c0);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x860f30c4);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x0c8e0000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x001fa729);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x01a31500);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x1, 28, 1);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x0, 28, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL));
		break;
	case 3450000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x4000028f);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x800cb300);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x860f30c4);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x0c8e0000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x001fa729);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x01a31500);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x1, 28, 1);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x0, 28, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL));
		break;
	case 2970000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x4000027b);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x800cb300);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x860f30c4);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x0c8e0000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x001fa729);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x01a31500);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x1, 28, 1);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x0, 28, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL));
		break;
	case 4320000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x400002b4);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x800cb000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x860f30c4);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x0c8e0000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x001fa729);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x01a31500);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x1, 28, 1);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x0, 28, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL));
		break;
	default:
		pr_info("error hpll clk: %d\n", clk);
		break;
	}
}

static void set_hpll_clk_out(unsigned clk)
{
	pr_info("config HPLL = %d\n", clk);

	switch (get_cpu_type()) {
	case MESON_CPU_MAJOR_ID_GXBB:
		set_gxb_hpll_clk_out(clk);
		break;
	case MESON_CPU_MAJOR_ID_GXTVBB:
		set_gxtvbb_hpll_clk_out(clk);
		break;
	case MESON_CPU_MAJOR_ID_GXL:
		set_gxl_hpll_clk_out(clk);
		break;
	}

	pr_info("config HPLL done\n");
}

static void set_hpll_od1(unsigned div)
{
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXL) {
		switch (div) {
		case 1:
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL3, 0, 21, 2);
			break;
		case 2:
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL3, 1, 21, 2);
			break;
		case 4:
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL3, 2, 21, 2);
			break;
		default:
			pr_info("Err %s[%d]\n", __func__, __LINE__);
			break;
		}
		return;
	}

	/* other than GXL */
	switch (div) {
	case 1:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 0, 16, 2);
		break;
	case 2:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 1, 16, 2);
		break;
	case 4:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 2, 16, 2);
		break;
	case 8:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 3, 16, 2);
		break;
	default:
		break;
	}
}

static void set_hpll_od2(unsigned div)
{
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXL) {
		switch (div) {
		case 1:
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL3, 0, 23, 2);
			break;
		case 2:
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL3, 1, 23, 2);
			break;
		case 4:
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL3, 2, 23, 2);
			break;
		default:
			pr_info("Err %s[%d]\n", __func__, __LINE__);
			break;
		}
		return;
	}

	/* other than GXL */
	switch (div) {
	case 1:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 0, 22, 2);
		break;
	case 2:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 1, 22, 2);
		break;
	case 4:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 2, 22, 2);
		break;
	case 8:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 3, 22, 2);
		break;
	default:
		break;
	}
}

static void set_hpll_od3(unsigned div)
{
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXL) {
		switch (div) {
		case 1:
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL3, 0, 19, 2);
			break;
		case 2:
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL3, 1, 19, 2);
			break;
		case 4:
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL3, 2, 19, 2);
			break;
		default:
			pr_info("Err %s[%d]\n", __func__, __LINE__);
			break;
		}
		return;
	}

	/* other than GXL */
	switch (div) {
	case 1:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 0, 18, 2);
		break;
	case 2:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 1, 18, 2);
		break;
	case 4:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 2, 18, 2);
		break;
	case 8:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 3, 18, 2);
		break;
	default:
		break;
	}
}

/* --------------------------------------------------
//              clocks_set_vid_clk_div
// --------------------------------------------------
// wire            clk_final_en    = control[19];
// wire            clk_div1        = control[18];
// wire    [1:0]   clk_sel         = control[17:16];
// wire            set_preset      = control[15];
// wire    [14:0]  shift_preset    = control[14:0];
 */
static void set_hpll_od3_clk_div(int div_sel)
{
	int shift_val = 0;
	int shift_sel = 0;

	pr_info("%s[%d] div = %d\n", __func__, __LINE__, div_sel);
	/* Disable the output clock */
	hd_set_reg_bits(P_HHI_VID_PLL_CLK_DIV, 0, 18, 2);
	hd_set_reg_bits(P_HHI_VID_PLL_CLK_DIV, 0, 15, 1);

	switch (div_sel) {
	case VID_PLL_DIV_1:
		shift_val = 0xFFFF;
		shift_sel = 0;
		break;
	case VID_PLL_DIV_2:
		shift_val = 0x0aaa;
		shift_sel = 0;
		break;
	case VID_PLL_DIV_3:
		shift_val = 0x0db6;
		shift_sel = 0;
		break;
	case VID_PLL_DIV_3p5:
		shift_val = 0x36cc;
		shift_sel = 1;
		break;
	case VID_PLL_DIV_3p75:
		shift_val = 0x6666;
		shift_sel = 2;
		break;
	case VID_PLL_DIV_4:
		shift_val = 0x0ccc;
		shift_sel = 0;
		break;
	case VID_PLL_DIV_5:
		shift_val = 0x739c;
		shift_sel = 2;
		break;
	case VID_PLL_DIV_6:
		shift_val = 0x0e38;
		shift_sel = 0;
		break;
	case VID_PLL_DIV_6p25:
		shift_val = 0x0000;
		shift_sel = 3;
		break;
	case VID_PLL_DIV_7:
		shift_val = 0x3c78;
		shift_sel = 1;
		break;
	case VID_PLL_DIV_7p5:
		shift_val = 0x78f0;
		shift_sel = 2;
		break;
	case VID_PLL_DIV_12:
		shift_val = 0x0fc0;
		shift_sel = 0;
		break;
	case VID_PLL_DIV_14:
		shift_val = 0x3f80;
		shift_sel = 1;
		break;
	case VID_PLL_DIV_15:
		shift_val = 0x7f80;
		shift_sel = 2;
		break;
	case VID_PLL_DIV_2p5:
		shift_val = 0x5294;
		shift_sel = 2;
		break;
	default:
		pr_info("Error: clocks_set_vid_clk_div:  Invalid parameter\n");
		break;
	}

	if (shift_val == 0xffff)      /* if divide by 1 */
		hd_set_reg_bits(P_HHI_VID_PLL_CLK_DIV, 1, 18, 1);
	else {
		hd_set_reg_bits(P_HHI_VID_PLL_CLK_DIV, 0, 18, 1);
		hd_set_reg_bits(P_HHI_VID_PLL_CLK_DIV, 0, 16, 2);
		hd_set_reg_bits(P_HHI_VID_PLL_CLK_DIV, 0, 15, 1);
		hd_set_reg_bits(P_HHI_VID_PLL_CLK_DIV, 0, 0, 14);

		hd_set_reg_bits(P_HHI_VID_PLL_CLK_DIV, shift_sel, 16, 2);
		hd_set_reg_bits(P_HHI_VID_PLL_CLK_DIV, 1, 15, 1);
		hd_set_reg_bits(P_HHI_VID_PLL_CLK_DIV, shift_val, 0, 14);
		hd_set_reg_bits(P_HHI_VID_PLL_CLK_DIV, 0, 15, 1);
	}
	/* Enable the final output clock */
	hd_set_reg_bits(P_HHI_VID_PLL_CLK_DIV, 1, 19, 1);
}

static void set_vid_clk_div(unsigned div)
{
	hd_set_reg_bits(P_HHI_VID_CLK_CNTL, 0, 16, 3);
	hd_set_reg_bits(P_HHI_VID_CLK_DIV, div-1, 0, 8);
	hd_set_reg_bits(P_HHI_VID_CLK_CNTL, 7, 0, 3);
}

static void set_hdmi_tx_pixel_div(unsigned div)
{
	div = check_div(div);
	if (div == -1)
		return;
	hd_set_reg_bits(P_HHI_HDMI_CLK_CNTL, div, 16, 4);
	hd_set_reg_bits(P_HHI_VID_CLK_CNTL2, 1, 5, 1);
}
static void set_encp_div(unsigned div)
{
	div = check_div(div);
	if (div == -1)
		return;
	hd_set_reg_bits(P_HHI_VID_CLK_DIV, div, 24, 4);
	hd_set_reg_bits(P_HHI_VID_CLK_CNTL2, 1, 2, 1);
	hd_set_reg_bits(P_HHI_VID_CLK_CNTL, 1, 19, 1);
}

static void set_enci_div(unsigned div)
{
	div = check_div(div);
	if (div == -1)
		return;
	hd_set_reg_bits(P_HHI_VID_CLK_DIV, div, 28, 4);
	hd_set_reg_bits(P_HHI_VID_CLK_CNTL2, 1, 0, 1);
	hd_set_reg_bits(P_HHI_VID_CLK_CNTL, 1, 19, 1);
}

/* mode hpll_clk_out od1 od2(PHY) od3
 * vid_pll_div vid_clk_div hdmi_tx_pixel_div encp_div enci_div
 */
static struct hw_enc_clk_val_group setting_enc_clk_val[] = {
	{{HDMI_720x480i60_16x9,
	  HDMI_720x576i50_16x9,
	  HDMI_VIC_END},
		4320000, 4, 4, 1, VID_PLL_DIV_5, 1, 2, -1, 2},
	{{HDMI_720x576p50_16x9,
	  HDMI_720x480p60_16x9,
	  HDMI_VIC_END},
		4320000, 4, 4, 1, VID_PLL_DIV_5, 1, 2, 1, -1},
	{{HDMI_1280x720p50_16x9,
	  HDMI_1280x720p60_16x9,
	  HDMI_VIC_END},
		2970000, 4, 1, 1, VID_PLL_DIV_5, 1, 2, 1, -1},
	{{HDMI_1920x1080i60_16x9,
	  HDMI_1920x1080i50_16x9,
	  HDMI_VIC_END},
		2970000, 4, 1, 1, VID_PLL_DIV_5, 1, 2, 1, -1},
	{{HDMI_1920x1080p60_16x9,
	  HDMI_1920x1080p50_16x9,
	  HDMI_VIC_END},
		2970000, 1, 2, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
	{{HDMI_1920x1080p30_16x9,
	  HDMI_1920x1080p24_16x9,
	  HDMI_1920x1080p25_16x9,
	  HDMI_VIC_END},
		2970000, 2, 2, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
	{{HDMI_3840x2160p30_16x9,
	  HDMI_3840x2160p25_16x9,
	  HDMI_3840x2160p24_16x9,
	  HDMI_4096x2160p24_256x135,
	  HDMI_4096x2160p25_256x135,
	  HDMI_4096x2160p30_256x135,
	  HDMI_VIC_END},
		2970000, 1, 1, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
	{{HDMI_3840x2160p60_16x9,
	  HDMI_3840x2160p50_16x9,
	  HDMI_4096x2160p60_256x135,
	  HDMI_4096x2160p50_256x135,
	  HDMI_VIC_END},
		5940000, 1, 1, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
	{{HDMI_4096x2160p60_256x135_Y420,
	  HDMI_4096x2160p50_256x135_Y420,
	  HDMI_3840x2160p60_16x9_Y420,
	  HDMI_3840x2160p50_16x9_Y420,
	  HDMI_VIC_END},
		2970000, 1, 1, 1, VID_PLL_DIV_5, 1, 2, 1, -1},
	{{HDMI_VIC_FAKE,
	  HDMI_VIC_END},
		3450000, 1, 2, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
};

/* mode hpll_clk_out od1 od2(PHY) od3
 * vid_pll_div vid_clk_div hdmi_tx_pixel_div encp_div enci_div
 */
static struct hw_enc_clk_val_group setting_enc_clk_val_30[] = {
	{{HDMI_1920x1080p60_16x9,
	  HDMI_1920x1080p50_16x9,
	  HDMI_VIC_END},
		3712500, 1, 2, 2, VID_PLL_DIV_6p25, 1, 1, 1, -1},
	{{HDMI_4096x2160p60_256x135_Y420,
	  HDMI_4096x2160p50_256x135_Y420,
	  HDMI_3840x2160p60_16x9_Y420,
	  HDMI_3840x2160p50_16x9_Y420,
	  HDMI_VIC_END},
		3712500, 1, 1, 1, VID_PLL_DIV_6p25, 1, 2, 1, -1},
	{{HDMI_3840x2160p24_16x9,
	  HDMI_3840x2160p25_16x9,
	  HDMI_3840x2160p30_16x9,
	  HDMI_VIC_END},
		3712500, 1, 1, 1, VID_PLL_DIV_6p25, 1, 2, 2, -1},
	{{HDMI_VIC_FAKE,
	  HDMI_VIC_END},
		3450000, 1, 2, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
};

void hdmitx_set_clk(enum hdmi_vic vic)
{
	int i = 0;
	int j = 0;
	struct hw_enc_clk_val_group *p_enc = NULL;

	p_enc = &setting_enc_clk_val[0];
	for (j = 0; j < sizeof(setting_enc_clk_val)
		/ sizeof(struct hw_enc_clk_val_group); j++) {
		for (i = 0; ((i < GROUP_MAX) && (p_enc[j].group[i]
			!= HDMI_VIC_END)); i++) {
			if (vic == p_enc[j].group[i])
				goto next;
		}
	}
	if (j == sizeof(setting_enc_clk_val)
		/ sizeof(struct hw_enc_clk_val_group)) {
		pr_info("Not find VIC = %d for hpll setting\n", vic);
		return;
	}
next:
	set_hdmitx_sys_clk();
	set_hpll_clk_out(p_enc[j].hpll_clk_out);
	set_hpll_od1(p_enc[j].od1);
	set_hpll_od2(p_enc[j].od2);
	set_hpll_od3(p_enc[j].od3);
	set_hpll_od3_clk_div(p_enc[j].vid_pll_div);
	pr_info("j = %d  vid_clk_div = %d\n", j, p_enc[j].vid_clk_div);
	set_vid_clk_div(p_enc[j].vid_clk_div);
	set_hdmi_tx_pixel_div(p_enc[j].hdmi_tx_pixel_div);
	set_encp_div(p_enc[j].encp_div);
	set_enci_div(p_enc[j].enci_div);
}

void hdmitx_set_clk_30b(enum hdmi_vic vic)
{
	int i = 0;
	int j = 0;
	struct hw_enc_clk_val_group *p_enc = NULL;

	p_enc = &setting_enc_clk_val_30[0];
	for (j = 0; j < sizeof(setting_enc_clk_val_30)
		/ sizeof(struct hw_enc_clk_val_group); j++) {
		for (i = 0; ((i < GROUP_MAX) && (p_enc[j].group[i]
			!= HDMI_VIC_END)); i++) {
			if (vic == p_enc[j].group[i])
				goto next;
		}
	}
	if (j == sizeof(setting_enc_clk_val_30) /
		sizeof(struct hw_enc_clk_val_group)) {
		pr_info("Not find VIC = %d for hpll setting\n", vic);
		return;
	}
next:
	set_hdmitx_sys_clk();
	set_hpll_clk_out(p_enc[j].hpll_clk_out);
	set_hpll_od1(p_enc[j].od1);
	set_hpll_od2(p_enc[j].od2);
	set_hpll_od3(p_enc[j].od3);
	set_hpll_od3_clk_div(p_enc[j].vid_pll_div);
	pr_info("j = %d  vid_clk_div = %d\n", j, p_enc[j].vid_clk_div);
	set_vid_clk_div(p_enc[j].vid_clk_div);
	set_hdmi_tx_pixel_div(p_enc[j].hdmi_tx_pixel_div);
	set_encp_div(p_enc[j].encp_div);
	set_enci_div(p_enc[j].enci_div);
}

int hdmitx_fine_tune_hpll(enum fine_tune_mode_e mode)
{
	static unsigned int save_div_frac;

	if (mode == DOWN_HPLL) {
		save_div_frac = hd_read_reg(P_HHI_HDMI_PLL_CNTL2);
		if (is_meson_gxl_cpu() || is_meson_gxm_cpu()) {
			if ((save_div_frac & 0xfff) == 0x300)
				hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2,
					0x280 , 0, 11);
			else if ((save_div_frac & 0xfff) == 0x200)
				hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2,
					0x100 , 0, 11);
		} else
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2,
				0xd03 , 0, 11);
	} else if (mode == UP_HPLL) {
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2,
		save_div_frac&0xfff , 0, 11);
	}

	return 0;
}

