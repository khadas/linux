#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/pm.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 29))
#include <mach/register.h>
#include <mach/irqs.h>
#include <mach/io.h>
#include <plat/io.h>
#endif

#include <linux/io.h>

#include <asm/io.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 29))
#include <linux/amlogic/iomap.h>
#endif
#include "meson_main.h"

#if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6TVD

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 29))
#define HHI_MALI_CLK_CNTL 0x106C
#define mplt_read(r)        aml_read_cbus((r))
#define mplt_write(v, r)    aml_write_cbus((r), (v))
#define mplt_setbits(r, m)  aml_write_cbus((r), (aml_read_cbus(r) | (m)));
#define mplt_clrbits(r, m)  aml_write_cbus((r), (aml_read_cbus(r) & (~(m))));
#else
#define mplt_read(r)        aml_read_reg32((P_##r))
#define mplt_write(v, r)    aml_write_reg32((P_##r), (v))
#define mplt_setbits(r, m)  aml_write_reg32((P_##r), (aml_read_reg32(P_##r) | (m)));
#define mplt_clrbits(r, m)  aml_write_reg32((P_##r), (aml_read_reg32(P_##r) & (~(m))));
#endif
#define FCLK_MPLL2 (2 << 9)
static DEFINE_SPINLOCK(lock);
static mali_plat_info_t* pmali_plat = NULL;
static u32 mali_extr_backup = 0;
static u32 mali_extr_sample_backup = 0;

int mali_clock_init(mali_plat_info_t* mali_plat)
{
	u32 def_clk_data;
	if (mali_plat == NULL) {
		printk(" Mali platform data is NULL!!!\n");
		return -1;
	}

	pmali_plat = mali_plat;
	if (pmali_plat->have_switch) {
		def_clk_data = pmali_plat->clk[pmali_plat->def_clock];
		mplt_write(def_clk_data | (def_clk_data << 16), HHI_MALI_CLK_CNTL);
		mplt_setbits(HHI_MALI_CLK_CNTL, 1 << 24);
		mplt_setbits(HHI_MALI_CLK_CNTL, 1 << 8);
	} else {
		mali_clock_set(pmali_plat->def_clock);
	}

	mali_extr_backup = pmali_plat->clk[pmali_plat->clk_len - 1];
	mali_extr_sample_backup = pmali_plat->clk_sample[pmali_plat->clk_len - 1];
	return 0;
}

int mali_clock_critical(critical_t critical, size_t param)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);
	ret = critical(param);
	spin_unlock_irqrestore(&lock, flags);
	return ret;
}

static int critical_clock_set(size_t param)
{
	unsigned int idx = param;
	if (pmali_plat->have_switch) {
		u32 clk_value;
		mplt_setbits(HHI_MALI_CLK_CNTL, 1 << 31);
		clk_value = mplt_read(HHI_MALI_CLK_CNTL) & 0xffff0000;
		clk_value = clk_value | pmali_plat->clk[idx] | (1 << 8);
		mplt_write(clk_value, HHI_MALI_CLK_CNTL);
		mplt_clrbits(HHI_MALI_CLK_CNTL, 1 << 31);
	} else {
		mplt_clrbits(HHI_MALI_CLK_CNTL, 1 << 8);
		mplt_clrbits(HHI_MALI_CLK_CNTL, (0x7F | (0x7 << 9)));
		mplt_write(pmali_plat->clk[idx], HHI_MALI_CLK_CNTL);
		mplt_setbits(HHI_MALI_CLK_CNTL, 1 << 8);
	}
	return 0;
}

int mali_clock_set(unsigned int clock)
{
	return mali_clock_critical(critical_clock_set, (size_t)clock);
}

void disable_clock(void)
{
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);
	mplt_clrbits(HHI_MALI_CLK_CNTL, 1 << 8);
	spin_unlock_irqrestore(&lock, flags);
}

void enable_clock(void)
{
	u32 ret;
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);
	mplt_setbits(HHI_MALI_CLK_CNTL, 1 << 8);
	ret = mplt_read(HHI_MALI_CLK_CNTL) & (1 << 8);
	spin_unlock_irqrestore(&lock, flags);
}

u32 get_mali_freq(u32 idx)
{
    if (!mali_pm_statue) {
    	return pmali_plat->clk_sample[idx];
    } else {
        return 0;    
    }
}

void set_str_src(u32 data)
{
#if 0
	if (data == 11) {
		writel(0x0004d000, (u32*)P_HHI_MPLL_CNTL9);
	} else if (data > 11) {
		writel(data, (u32*)P_HHI_MPLL_CNTL9);
	}
#endif
	if (data == 0) {
		pmali_plat->clk[pmali_plat->clk_len - 1] = mali_extr_backup;
		pmali_plat->clk_sample[pmali_plat->clk_len - 1] = mali_extr_sample_backup;
	} else if (data > 10) {
		pmali_plat->clk_sample[pmali_plat->clk_len - 1] = 600;
		pmali_plat->clk[pmali_plat->clk_len - 1] = FCLK_MPLL2;
	}
}
#endif
