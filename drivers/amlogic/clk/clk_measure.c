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
int    m8m2_clk_measure(char  index)
{
	const char *clk_table[] = {
	" CTS_MIPI_CSI_CFG_CLK(63)",
	" VID2_PLL_CLK(62)",
	" GPIO_CLK(61)",
	" USB_32K_ALT(60)",
	" CTS_HCODEC_CLK(59)",
	" Reserved(58)",
	" Reserved(57)",
	" Reserved(56)",
	" Reserved(55)",
	" Reserved(54)",
	" Reserved(53)",
	" Reserved(52)",
	" Reserved(51)",
	" Reserved(50)",
	" CTS_PWM_E_CLK(49)",
	" CTS_PWM_F_CLK(48)",
	" DDR_DPLL_PT_CLK(47)",
	" CTS_PCM2_SCLK(46)",
	" CTS_PWM_A_CLK(45)",
	" CTS_PWM_B_CLK(44)",
	" CTS_PWM_C_CLK(43)",
	" CTS_PWM_D_CLK(42)",
	" CTS_ETH_RX_TX(41)",
	" CTS_PCM_MCLK(40)",
	" CTS_PCM_SCLK(39)",
	" CTS_VDIN_MEAS_CLK(38)",
	" Reserved(37)",
	" CTS_HDMI_TX_PIXEL_CLK(36)",
	" CTS_MALI_CLK (35)",
	" CTS_SDHC_SDCLK(34)",
	" CTS_SDHC_RXCLK(33)",
	" CTS_VDAC_CLK(32)",
	" CTS_AUDAC_CLKPI(31)",
	" MPLL_CLK_TEST_OUT(30)",
	" Reserved(29)",
	" CTS_SAR_ADC_CLK(28)",
	" Reserved(27)",
	" SC_CLK_INT(26)",
	" Reserved(25)",
	" LVDS_FIFO_CLK(24)",
	" HDMI_CH0_TMDSCLK(23)",
	" CLK_RMII_FROM_PAD (22)",
	" I2S_CLK_IN_SRC0(21)",
	" RTC_OSC_CLK_OUT(20)",
	" CTS_HDMI_SYS_CLK(19)",
	" A9_CLK_DIV16(18)",
	" Reserved(17)",
	" CTS_FEC_CLK_2(16)",
	" CTS_FEC_CLK_1 (15)",
	" CTS_FEC_CLK_0 (14)",
	" CTS_AMCLK(13)",
	" Reserved(12)",
	" CTS_ETH_RMII(11)",
	" Reserved(10)",
	" CTS_ENCL_CLK(9)",
	" CTS_ENCP_CLK(8)",
	" CLK81 (7)",
	" VID_PLL_CLK(6)",
	" Reserved (5)",
	" Reserved (4)",
	" A9_RING_OSC_CLK(3)",
	" AM_RING_OSC_CLK_OUT_EE2(2)",
	" AM_RING_OSC_CLK_OUT_EE1(1)",
	" AM_RING_OSC_CLK_OUT_EE0(0)",
	};
	int  i;
	int len = sizeof(clk_table)/sizeof(char *) - 1;
	if (index  == 0xff) {
		for (i = 0; i < len; i++)
			pr_info("[%10d]%s\n", clk_util_clk_msr(i),
					clk_table[len-i]);
		return 0;
	}
	pr_info("[%10d]%s\n", clk_util_clk_msr(index),
		   clk_table[len-index]);
	return 0;
}

void meson_clk_measure(unsigned int clk_mux)
{
	m8m2_clk_measure(clk_mux);
}
EXPORT_SYMBOL(meson_clk_measure);
static void __init clock_msr_init(struct device_node *np)
{
	msr_clk_reg0 = of_iomap(np, 0);
	msr_clk_reg2 = of_iomap(np, 1);
}
CLK_OF_DECLARE(meson_clk_msr, "amlogic,m8m2_measure", clock_msr_init);
