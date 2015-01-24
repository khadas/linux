

#include <dt-bindings/clock/meson8.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "clk.h"
#include "clk-pll.h"

#define CLK_GATE_BASE 0x100B
#define CLK_GATE_AO_BASE 0xc8100000
#define OFFSET(x) (((x) - CLK_GATE_BASE)*4) /*(x -0x100B)*4 */

#define AO_RTI_GEN_CTNL_REG0 0xc8100040
#define OFFSET_AO(x) (((x) - CLK_GATE_AO_BASE)*4)
void __iomem *reg_base_cbus, *reg_base_aobus;

/* list of PLLs */
enum meson8m2_plls {
	fixed_pll, sys_pll, vid_pll, g_pll, hdmi_pll,
	nr_plls			/* number of PLLs */
};

/* list of all parent clocks */
PNAME(clk81_p1) = {"xtal", "32Khz"};
PNAME(clk81_p2) = {"clk81_m_1", "", "fclk_div7",
	    "mpll_clk_out1", "mpll_clk_out2",
	    "fclk_div4", "fclk_div3", "fclk_div5"};
PNAME(clk81_p3) = {"clk81_m_1", "clk81_g"};
PNAME(cpu_p1) = {"ext_osc", "sys_pll"};
PNAME(cpu_p2) = {"cpu_m_1", "sys_pll2", "sys_pll_div3", "sys_pll_divm"};
PNAME(cpu_p3) = {"xtal", "cpu_m_2"};
PNAME(mux_hdmi_sys_p) = {"xtal", "fclk_div4", "fclk_div3", "fclk_div5"};

/* fixed rate clocks generated outside the soc */
static struct amlogic_fixed_rate_clock meson8_fixed_rate_ext_clks[] __initdata = {
/*obtain the clock speed of external fixed clock sources from device tree*/
	FRATE(0, "xtal", NULL, CLK_IS_ROOT, 24000000),
	FRATE(0, "32Khz", NULL, CLK_IS_ROOT, 32000),
};

/* fixed factor clocks */
static struct amlogic_fixed_factor_clock meson8_fixed_factor_clks[] __initdata = {
	FFACTOR(0, "fclk_div3", "fixed_pll", 1, 3, CLK_GET_RATE_NOCACHE),
	FFACTOR(0, "fclk_div4", "fixed_pll", 1, 4, CLK_GET_RATE_NOCACHE),
	FFACTOR(0, "fclk_div5", "fixed_pll", 1, 5, CLK_GET_RATE_NOCACHE),
	FFACTOR(0, "fclk_div7", "fixed_pll", 1, 7, CLK_GET_RATE_NOCACHE),
	FFACTOR(0, "sys_pll_div2", "sys_pll", 1, 2, CLK_GET_RATE_NOCACHE),
	FFACTOR(0, "sys_pll_div3", "sys_pll", 1, 3, CLK_GET_RATE_NOCACHE),
};

/* mux clocks */
static struct amlogic_mux_clock meson8_mux_clks[] __initdata = {
/*clk81*/
	MUX_A(0, "clk81_m_1", clk81_p1, OFFSET(HHI_MPEG_CLK_CNTL),
		9, 1, "ext_osc", 0),
	MUX(0, "clk81_m_2", clk81_p2, OFFSET(HHI_MPEG_CLK_CNTL), 12, 3, 0),
	MUX(0, "clk81_m_3", clk81_p3, OFFSET(HHI_MPEG_CLK_CNTL), 8, 1, 0),
/*cpu clk*/
	MUX(0, "cpu_m_1", cpu_p1, OFFSET(HHI_SYS_CPU_CLK_CNTL), 0, 1, 0),
	MUX(0, "cpu_m_2", cpu_p2, OFFSET(HHI_SYS_CPU_CLK_CNTL), 2, 2, 0),
	MUX(0, "cpu_m_3", cpu_p3, OFFSET(HHI_SYS_CPU_CLK_CNTL), 7, 1, 0),
};

/* divider clocks */
static struct amlogic_div_clock meson8_div_clks[] __initdata = {
/*clk81*/
	DIV(0, "clk81_d", "clk81_m_2", OFFSET(HHI_MPEG_CLK_CNTL), 0, 6, 0),
	DIV(0, "sys_pll_divm", "sys_pll_div2",
		OFFSET(HHI_SYS_CPU_CLK_CNTL), 20, 10, 0),
};

/* gate clocks */
static struct amlogic_gate_clock meson8_gate_clks[] __initdata = {
/*clk81*/
	GATE(0, "clk81_g", "clk81_d", OFFSET(HHI_MPEG_CLK_CNTL), 7, 0, 0, 0),
};

/* Modules and sub-modules within the meson chip can be disabled by shutting
 * off the clock. There ars bits associated with either the MPEG_DOMAIN and
 * OTHER_DOMAIN gated clock enabled. If a bit is set high, the clock is enabled.
 */
static struct amlogic_gate_clock meson8_periph_gate_clks[] __initdata = {
/*uart*/
	GATE(CLK_UART_AO, "uart_ao", "clk81_m_3",
		OFFSET_AO(AO_RTI_GEN_CTNL_REG0), 3, 0, 0, 1),
};

static  struct amlogic_pll_rate_table meson8m2_syspll_tbl[] = {
	PLL_2500_RATE(24, 56, 1, 2, 6, 0, 0), /* fvco 1344, / 4, /14 */
	PLL_2500_RATE(48, 64, 1, 2, 3, 0, 0), /* fvco 1536, / 4, / 8 */
	PLL_2500_RATE(72, 72, 1, 2, 2, 1, 1), /* fvco 1728, / 4, / 6 */
	PLL_2500_RATE(96, 64, 1, 2, 1, 0, 0), /* fvco 1536, / 4, / 4 */
	PLL_2500_RATE(120, 80, 1, 2, 1, 1, 1), /* fvco 1920, / 4, / 4 */
	PLL_2500_RATE(144, 96, 1, 2, 1, 0, 0), /* fvco 2304, / 4, / 4 */
	PLL_2500_RATE(168, 56, 1, 1, 1, 0, 0), /* fvco 1344, / 2, / 4 */
	PLL_2500_RATE(192, 64, 1, 1, 1, 0, 0), /* fvco 1536, / 2, / 4 */
	PLL_2500_RATE(216, 72, 1, 1, 1, 1, 1), /* fvco 1728, / 2, / 4 */
	PLL_2500_RATE(240, 80, 1, 1, 1, 1, 1), /* fvco 1920, / 2, / 4 */
	PLL_2500_RATE(264, 88, 1, 1, 1, 0, 0), /* fvco 2112, / 2, / 4 */
	PLL_2500_RATE(288, 96, 1, 1, 1, 0, 0), /* fvco 2304, / 2, / 4 */
	PLL_2500_RATE(312, 52, 1, 2, 0, 0, 0), /* fvco 1248, / 4, / 1 */
	PLL_2500_RATE(336, 56, 1, 2, 0, 0, 0), /* fvco 1344, / 4, / 1 */
	PLL_2500_RATE(360, 60, 1, 2, 0, 0, 0), /* fvco 1440, / 4, / 1 */
	PLL_2500_RATE(384, 64, 1, 2, 0, 0, 0), /* fvco 1536, / 4, / 1 */
	PLL_2500_RATE(408, 68, 1, 2, 0, 1, 0), /* fvco 1632, / 4, / 1 */
	PLL_2500_RATE(432, 72, 1, 2, 0, 1, 1), /* fvco 1728, / 4, / 1 */
	PLL_2500_RATE(456, 76, 1, 2, 0, 1, 1), /* fvco 1824, / 4, / 1 */
	PLL_2500_RATE(480, 80, 1, 2, 0, 1, 1), /* fvco 1920, / 4, / 1 */
	PLL_2500_RATE(504, 84, 1, 2, 0, 1, 1), /* fvco 2016, / 4, / 1 */
	PLL_2500_RATE(528, 88, 1, 2, 0, 0, 0), /* fvco 2112, / 4, / 1 */
	PLL_2500_RATE(552, 92, 1, 2, 0, 0, 0), /* fvco 2208, / 4, / 1 */
	PLL_2500_RATE(576, 96, 1, 2, 0, 0, 0), /* fvco 2304, / 4, / 1 */
	PLL_2500_RATE(600, 50, 1, 1, 0, 0, 0), /* fvco 1200, / 2, / 1 */
	PLL_2500_RATE(624, 52, 1, 1, 0, 0, 0), /* fvco 1248, / 2, / 1 */
	PLL_2500_RATE(648, 54, 1, 1, 0, 0, 0), /* fvco 1296, / 2, / 1 */
	PLL_2500_RATE(672, 56, 1, 1, 0, 0, 0), /* fvco 1344, / 2, / 1 */
	PLL_2500_RATE(696, 58, 1, 1, 0, 0, 0), /* fvco 1392, / 2, / 1 */
	PLL_2500_RATE(720, 60, 1, 1, 0, 0, 0), /* fvco 1440, / 2, / 1 */
	PLL_2500_RATE(744, 62, 1, 1, 0, 0, 0), /* fvco 1488, / 2, / 1 */
	PLL_2500_RATE(768, 64, 1, 1, 0, 0, 0), /* fvco 1536, / 2, / 1 */
	PLL_2500_RATE(792, 66, 1, 1, 0, 0, 0), /* fvco 1584, / 2, / 1 */
	PLL_2500_RATE(816, 68, 1, 1, 0, 1, 0), /* fvco 1632, / 2, / 1 */
	PLL_2500_RATE(840, 70, 1, 1, 0, 1, 0), /* fvco 1680, / 2, / 1 */
	PLL_2500_RATE(864, 72, 1, 1, 0, 1, 1), /* fvco 1728, / 2, / 1 */
	PLL_2500_RATE(888, 74, 1, 1, 0, 1, 1), /* fvco 1776, / 2, / 1 */
	PLL_2500_RATE(912, 76, 1, 1, 0, 1, 1), /* fvco 1824, / 2, / 1 */
	PLL_2500_RATE(936, 78, 1, 1, 0, 1, 1), /* fvco 1872, / 2, / 1 */
	PLL_2500_RATE(960, 80, 1, 1, 0, 1, 1), /* fvco 1920, / 2, / 1 */
	PLL_2500_RATE(984, 82, 1, 1, 0, 1, 1), /* fvco 1968, / 2, / 1 */
	PLL_2500_RATE(1008, 84, 1, 1, 0, 1, 1), /* fvco 2016, / 2, / 1 */
	PLL_2500_RATE(1032, 86, 1, 1, 0, 1, 1), /* fvco 2064, / 2, / 1 */
	PLL_2500_RATE(1056, 88, 1, 1, 0, 0, 0), /* fvco 2112, / 2, / 1 */
	PLL_2500_RATE(1080, 90, 1, 1, 0, 0, 0), /* fvco 2160, / 2, / 1 */
	PLL_2500_RATE(1104, 92, 1, 1, 0, 0, 0), /* fvco 2208, / 2, / 1 */
	PLL_2500_RATE(1128, 94, 1, 1, 0, 0, 0), /* fvco 2256, / 2, / 1 */
	PLL_2500_RATE(1152, 96, 1, 1, 0, 0, 0), /* fvco 2304, / 2, / 1 */
	PLL_2500_RATE(1176, 98, 1, 1, 0, 0, 0), /* fvco 2352, / 2, / 1 */
	PLL_2500_RATE(1200, 50, 1, 0, 0, 0, 0), /* fvco 1200, / 1, / 1 */
	PLL_2500_RATE(1224, 51, 1, 0, 0, 0, 0), /* fvco 1224, / 1, / 1 */
	PLL_2500_RATE(1248, 52, 1, 0, 0, 0, 0), /* fvco 1248, / 1, / 1 */
	PLL_2500_RATE(1272, 53, 1, 0, 0, 0, 0), /* fvco 1272, / 1, / 1 */
	PLL_2500_RATE(1296, 54, 1, 0, 0, 0, 0), /* fvco 1296, / 1, / 1 */
	PLL_2500_RATE(1320, 55, 1, 0, 0, 0, 0), /* fvco 1320, / 1, / 1 */
	PLL_2500_RATE(1344, 56, 1, 0, 0, 0, 0), /* fvco 1344, / 1, / 1 */
	PLL_2500_RATE(1368, 57, 1, 0, 0, 0, 0), /* fvco 1368, / 1, / 1 */
	PLL_2500_RATE(1392, 58, 1, 0, 0, 0, 0), /* fvco 1392, / 1, / 1 */
	PLL_2500_RATE(1416, 59, 1, 0, 0, 0, 0), /* fvco 1416, / 1, / 1 */
	PLL_2500_RATE(1440, 60, 1, 0, 0, 0, 0), /* fvco 1440, / 1, / 1 */
	PLL_2500_RATE(1464, 61, 1, 0, 0, 0, 0), /* fvco 1464, / 1, / 1 */
	PLL_2500_RATE(1488, 62, 1, 0, 0, 0, 0), /* fvco 1488, / 1, / 1 */
	PLL_2500_RATE(1512, 63, 1, 0, 0, 0, 0), /* fvco 1512, / 1, / 1 */
	PLL_2500_RATE(1536, 64, 1, 0, 0, 0, 0), /* fvco 1536, / 1, / 1 */
	PLL_2500_RATE(1560, 65, 1, 0, 0, 0, 0), /* fvco 1560, / 1, / 1 */
	PLL_2500_RATE(1584, 66, 1, 0, 0, 0, 0), /* fvco 1584, / 1, / 1 */
	PLL_2500_RATE(1608, 67, 1, 0, 0, 1, 0), /* fvco 1608, / 1, / 1 */
	PLL_2500_RATE(1632, 68, 1, 0, 0, 1, 0), /* fvco 1632, / 1, / 1 */
	PLL_2500_RATE(1656, 68, 1, 0, 0, 1, 0), /* fvco 1656, / 1, / 1 */
	PLL_2500_RATE(1680, 68, 1, 0, 0, 1, 0), /* fvco 1680, / 1, / 1 */
	PLL_2500_RATE(1704, 68, 1, 0, 0, 1, 0), /* fvco 1704, / 1, / 1 */
	PLL_2500_RATE(1728, 69, 1, 0, 0, 1, 0), /* fvco 1728, / 1, / 1 */
	PLL_2500_RATE(1752, 69, 1, 0, 0, 1, 0), /* fvco 1752, / 1, / 1 */
	PLL_2500_RATE(1776, 69, 1, 0, 0, 1, 0), /* fvco 1776, / 1, / 1 */
	PLL_2500_RATE(1800, 69, 1, 0, 0, 1, 0), /* fvco 1800, / 1, / 1 */
	PLL_2500_RATE(1824, 70, 1, 0, 0, 1, 0), /* fvco 1824, / 1, / 1 */
	PLL_2500_RATE(1848, 70, 1, 0, 0, 1, 0), /* fvco 1848, / 1, / 1 */
	PLL_2500_RATE(1872, 70, 1, 0, 0, 1, 0), /* fvco 1872, / 1, / 1 */
	PLL_2500_RATE(1896, 70, 1, 0, 0, 1, 0), /* fvco 1896, / 1, / 1 */
	PLL_2500_RATE(1920, 71, 1, 0, 0, 1, 0), /* fvco 1920, / 1, / 1 */
	PLL_2500_RATE(1944, 71, 1, 0, 0, 1, 0), /* fvco 1944, / 1, / 1 */
	PLL_2500_RATE(1968, 71, 1, 0, 0, 1, 0), /* fvco 1968, / 1, / 1 */
	PLL_2500_RATE(1992, 71, 1, 0, 0, 1, 0), /* fvco 1992, / 1, / 1 */
	PLL_2500_RATE(2016, 72, 1, 0, 0, 1, 0), /* fvco 2016, / 1, / 1 */
	PLL_2500_RATE(2040, 72, 1, 0, 0, 1, 0), /* fvco 2040, / 1, / 1 */
	PLL_2500_RATE(2064, 72, 1, 0, 0, 1, 0), /* fvco 2064, / 1, / 1 */
	PLL_2500_RATE(2088, 72, 1, 0, 0, 1, 0), /* fvco 2088, / 1, / 1 */
	PLL_2500_RATE(2112, 73, 1, 0, 0, 0, 0), /* fvco 2112, / 1, / 1 */
};

static struct amlogic_pll_clock meson8m2_plls[] __initdata = {
	PLL(pll_2550, CLK_FIXED_PLL, "fixed_pll", "xtal",
		OFFSET(HHI_MPLL_CNTL), NULL),
	PLL(pll_2500, CLK_SYS_PLL, "sys_pll", "xtal",
		OFFSET(HHI_SYS_PLL_CNTL), meson8m2_syspll_tbl),
	PLL(pll_1500, CLK_G_PLL, "g_pll", "xtal",
		OFFSET(HHI_GP_PLL_CNTL), NULL),
};
/*	[ddr_pll] = PLL(pll_1500, CLK_DDR_PLL, "ddr_pll", "xtal", DDRPLL_LOCK,
		DDRPLL_CON0, NULL),*/
static struct amlogic_clk_branch meson8m2_clk_branches[] __initdata = {
	COMPOSITE(HDMI_SYS_CLK, "hdmi_sys_clk", mux_hdmi_sys_p, 0,
		OFFSET(HHI_HDMI_CLK_CNTL), 9, 2, 0,
		OFFSET(HHI_HDMI_CLK_CNTL), 0, 6, 0,
		OFFSET(HHI_HDMI_CLK_CNTL), 8, 0),
};
static struct of_device_id ext_clk_match[] __initdata = {
	{ .compatible = "amlogic,clock-xtal", .data = (void *)0, },
	{},
};

/* register meson8 clocks */
static void __init meson8_clk_init(struct device_node *np)
{

	reg_base_cbus = of_iomap(np, 0);
	reg_base_aobus = of_iomap(np, 1);
	if ((!reg_base_cbus) || (!reg_base_aobus))
		panic("%s: failed to map registers\n", __func__);

	pr_debug("meson8 clk reg_base_cbus is 0x%p\n", reg_base_cbus);
	pr_debug("meson8 clk reg_base_aobus is 0x%p\n", reg_base_aobus);

	amlogic_clk_init(np, reg_base_cbus, reg_base_aobus,
			CLK_NR_CLKS, NULL, 0, NULL, 0);
	amlogic_clk_of_register_fixed_ext(meson8_fixed_rate_ext_clks,
		  ARRAY_SIZE(meson8_fixed_rate_ext_clks), ext_clk_match);

	amlogic_clk_register_pll(meson8m2_plls,
	    ARRAY_SIZE(meson8m2_plls), reg_base_cbus);
	hdmi_clk_init(reg_base_cbus);
	amlogic_clk_register_fixed_factor(meson8_fixed_factor_clks,
			ARRAY_SIZE(meson8_fixed_factor_clks));

	amlogic_clk_register_mux(meson8_mux_clks,
			ARRAY_SIZE(meson8_mux_clks));
	amlogic_clk_register_div(meson8_div_clks,
			ARRAY_SIZE(meson8_div_clks));
	amlogic_clk_register_gate(meson8_gate_clks,
			ARRAY_SIZE(meson8_gate_clks));
	amlogic_clk_register_gate(meson8_periph_gate_clks,
			ARRAY_SIZE(meson8_periph_gate_clks));
	amlogic_clk_register_branches(meson8m2_clk_branches,
				  ARRAY_SIZE(meson8m2_clk_branches));

	{
		/* Dump clocks */
		char *clks[] = {
				"xtal",
				"32Khz",
				"fixed_pll",
				"sys_pll",
				"fclk_div3",
				"fclk_div4",
				"fclk_div5",
				"fclk_div7",
				"clk81_m_1",
				"clk81_m_2",
				"clk81_d",
				"clk81_g",
				"clk81_m_3",
				"uart_ao",
				"cpu_m_1",
				"cpu_m_2",
				"cpu_m_3",
				"sys_pll_div2",
				"sys_pll_div3",
				"sys_pll_divm",
				"hdmi_pll_lvds",
				"hdmi_sys_clk"
		};
		int i;
		int count = ARRAY_SIZE(clks);

		for (i = 0; i < count; i++) {
			char *clk_name = clks[i];
			pr_info("clkrate [ %s \t] : %lu\n", clk_name,
				_get_rate(clk_name));
		}

	}
	pr_info("meson8 clock initialization complete\n");
}
CLK_OF_DECLARE(meson8, "amlogic,meson8m2-clock", meson8_clk_init);
