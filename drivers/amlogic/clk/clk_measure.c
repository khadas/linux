#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <asm/system_info.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/clk-provider.h>

void __iomem *msr_clk_reg0;
void __iomem *msr_clk_reg2;
static unsigned int clk_util_clk_msr(unsigned int clk_mux)
{
	unsigned int  msr;
	unsigned int regval = 0;
	unsigned int val;
	writel_relaxed(0, msr_clk_reg0);
    /* Set the measurement gate to 64uS */
	val = readl_relaxed(msr_clk_reg0);
	val = (val & (~0xffff)) | (64-1);
	writel_relaxed(val, msr_clk_reg0);
    /* Disable continuous measurement */
    /* disable interrupts */
	val = readl_relaxed(msr_clk_reg0);
	val = val & (~((1<<18)|(1<<17)));
	writel_relaxed(val, msr_clk_reg0);
	val = readl_relaxed(msr_clk_reg0);
	val = (val & (~(0x1f<<20))) | (clk_mux<<20)|(1<<19)|(1<<16);
	writel_relaxed(val, msr_clk_reg0);
    /* Wait for the measurement to be done */
	do {
		regval = readl_relaxed(msr_clk_reg0);
	} while (regval & (1 << 31));
    /* disable measuring */
	val = readl_relaxed(msr_clk_reg0);
	val = val & (~(1<<16));
	msr = (readl_relaxed(msr_clk_reg2)+31)&0x000FFFFF;
    /* Return value in MHz*measured_val */
	return (msr>>6)*1000000;

}
unsigned int meson_clk_measure(unsigned int clk_mux)
{
	return clk_util_clk_msr(clk_mux);
}
EXPORT_SYMBOL(meson_clk_measure);
static void __init clock_msr_init(struct device_node *np)
{
	msr_clk_reg0 = of_iomap(np, 0);
	msr_clk_reg2 = of_iomap(np, 1);
}
CLK_OF_DECLARE(meson_clk_msr, "amlogic,m8m2_measure", clock_msr_init);
