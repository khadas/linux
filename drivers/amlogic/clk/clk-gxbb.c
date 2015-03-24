

#include <dt-bindings/clock/gxbb.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "clk.h"


/* fixed rate clocks generated outside the soc */
static struct amlogic_fixed_rate_clock gxbb_fixed_rate_ext_clks[] __initdata = {
/*obtain the clock speed of external fixed clock sources from device tree*/
	FRATE(0, "xtal", NULL, CLK_IS_ROOT, 24000000),
	FRATE(0, "32Khz", NULL, CLK_IS_ROOT, 32000),
	FRATE(CLK_81, "clk81", NULL, CLK_IS_ROOT, 166666666),
};

static struct of_device_id ext_clk_match[] __initdata = {
	{ .compatible = "amlogic,clock-xtal", .data = (void *)0, },
	{},
};

/* register gxbb clocks */
static void __init gxbb_clk_init(struct device_node *np)
{

	reg_base_cbus = of_iomap(np, 0);
	reg_base_aobus = of_iomap(np, 1);
	if ((!reg_base_cbus) || (!reg_base_aobus))
		panic("%s: failed to map registers\n", __func__);

	pr_debug("gxbb clk HIU base is 0x%p\n", reg_base_cbus);
	pr_debug("gxbb clk ao base is 0x%p\n", reg_base_aobus);

	amlogic_clk_init(np, reg_base_cbus, reg_base_aobus,
			CLK_NR_CLKS, NULL, 0, NULL, 0);
	amlogic_clk_of_register_fixed_ext(gxbb_fixed_rate_ext_clks,
		  ARRAY_SIZE(gxbb_fixed_rate_ext_clks), ext_clk_match);


	{
		/* Dump clocks */
		char *clks[] = {
				"xtal",
				"32Khz",
				"clk81",
		};
		int i;
		int count = ARRAY_SIZE(clks);

		for (i = 0; i < count; i++) {
			char *clk_name = clks[i];
			pr_info("clkrate [ %s \t] : %lu\n", clk_name,
				_get_rate(clk_name));
		}

	}
	pr_info("gxbb clock initialization complete\n");
}
CLK_OF_DECLARE(gxbb, "amlogic, gxbb-clock", gxbb_clk_init);
