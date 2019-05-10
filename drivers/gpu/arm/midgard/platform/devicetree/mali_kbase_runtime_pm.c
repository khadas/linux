/*
 *
 * (C) COPYRIGHT 2015, 2017 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#include <mali_kbase.h>
#include <mali_kbase_defs.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <backend/gpu/mali_kbase_device_internal.h>
#include "mali_kbase_config_platform.h"

void *reg_base_hiubus = NULL;
u32  override_value_aml = 0;
static int first = 1;

#define RESET0_MASK    0x10
#define RESET1_MASK    0x11
#define RESET2_MASK    0x12

#define RESET0_LEVEL    0x20
#define RESET1_LEVEL    0x21
#define RESET2_LEVEL    0x22
#define Rd(r)                           readl((reg_base_hiubus) + ((r)<<2))
#define Wr(r, v)                        writel((v), ((reg_base_hiubus) + ((r)<<2)))
#define Mali_WrReg(regnum, value)   kbase_reg_write(kbdev, (regnum), (value), NULL)
#define Mali_RdReg(regnum)          kbase_reg_read(kbdev, (regnum), NULL)
#define stimulus_print   printk
#define stimulus_display printk
#define Mali_pwr_off(x)  Mali_pwr_off_with_kdev(kbdev, (x))

extern u64 kbase_pm_get_ready_cores(struct kbase_device *kbdev, enum kbase_pm_core_type type);

//[0]:CG [1]:SC0 [2]:SC2
static void  Mali_pwr_on_with_kdev ( struct kbase_device *kbdev, uint32_t  mask)
{
    uint32_t    part1_done;
    uint32_t    shader_present;
    uint32_t    tiler_present;
    uint32_t    l2_present;

    part1_done = 0;
    Mali_WrReg(0x0000024, 0xffffffff); // clear interrupts

    shader_present = Mali_RdReg(0x100);
    tiler_present  = Mali_RdReg(0x110);
    l2_present     = Mali_RdReg(0x120);
    printk("shader_present=%d, tiler_present=%d, l2_present=%d\n",
            shader_present, tiler_present, l2_present);

    if (  mask == 0 ) {
        Mali_WrReg(0x00000180, 0xffffffff);   // Power on all cores (shader low)
        Mali_WrReg(0x00000184, 0xffffffff);   // Power on all cores (shader high)
        Mali_WrReg(0x00000190, 0xffffffff);   // Power on all cores (tiler low)
        Mali_WrReg(0x00000194, 0xffffffff);   // Power on all cores (tiler high)
        Mali_WrReg(0x000001a0, 0xffffffff);   // Power on all cores (l2 low)
        Mali_WrReg(0x000001a4, 0xffffffff);   // Power on all cores (l2 high)
    } else {
        Mali_WrReg(0x00000180, mask);    // Power on all cores (shader low)
        Mali_WrReg(0x00000184, 0);       // Power on all cores (shader high)
        Mali_WrReg(0x00000190, mask);    // Power on all cores (tiler low)
        Mali_WrReg(0x00000194, 0);       // Power on all cores (tiler high)
        Mali_WrReg(0x000001a0, mask);    // Power on all cores (l2 low)
        Mali_WrReg(0x000001a4, 0);       // Power on all cores (l2 high)
    }

    part1_done = Mali_RdReg(0x0000020);
    while((part1_done ==0)) { part1_done = Mali_RdReg(0x00000020); }
    stimulus_display("Mali_pwr_on:gpu_irq : %x\n", part1_done);
    Mali_WrReg(0x0000024, 0xffffffff); // clear interrupts
}

//[0]:CG [1]:SC0 [2]:SC2
#if 0
static void  Mali_pwr_off_with_kdev( struct kbase_device *kbdev, uint32_t  mask)
{
    uint32_t    part1_done;
    part1_done = 0;
    Mali_WrReg(0x0000024, 0xffffffff); // clear interrupts

    if ( mask == 0 ) {
        Mali_WrReg(0x000001C0, 0xffffffff);    // Power off all cores (tiler low)
        Mali_WrReg(0x000001C4, 0xffffffff);    // Power off all cores (tiler high)
        Mali_WrReg(0x000001D0, 0xffffffff);    // Power off all cores (l2 low)
        Mali_WrReg(0x000001D4, 0xffffffff);    // Power off all cores (l2 high)
        Mali_WrReg(0x000001E0, 0xffffffff);    // Power off all cores (shader low)
        Mali_WrReg(0x000001E4, 0xffffffff);    // Power off all cores (shader high)
    } else {
        Mali_WrReg(0x000001C0, mask);    // Power off all cores  (tiler low)
        Mali_WrReg(0x000001C4, 0x0);     // Power off all cores  (tiler high)
        Mali_WrReg(0x000001D0, mask);    // Power off all cores  (l2 low)
        Mali_WrReg(0x000001D4, 0x0);     // Power off all cores  (l2 high)
        Mali_WrReg(0x000001E0, mask);    // Power off all cores  (shader low)
        Mali_WrReg(0x000001E4, 0x0);     // Power off all cores  (shader high)
    }

    part1_done = Mali_RdReg(0x0000020);
    while((part1_done ==0)) { part1_done = Mali_RdReg(0x00000020); }
    stimulus_display("Mali_pwr_off:gpu_irq : %x\n", part1_done);
    Mali_WrReg(0x0000024, 0xffffffff); // clear interrupts
}
#endif

static int pm_callback_power_on(struct kbase_device *kbdev)
{
	int ret = 1; /* Assume GPU has been powered off */
	int error;
	struct platform_device *pdev = to_platform_device(kbdev->dev);
	struct resource *reg_res;
	u64 core_ready;
	u64 l2_ready;
	u64 tiler_ready;
	u32 value;

    //printk("20151013, %s, %d\n", __FILE__, __LINE__);
    if (first == 0) goto ret;

    reg_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
    if (!reg_res) {
        dev_err(kbdev->dev, "Invalid register resource\n");
        ret = -ENOENT;
    }
    //printk("%s, %d\n", __FILE__, __LINE__);
    if (NULL == reg_base_hiubus) 
        reg_base_hiubus = ioremap(reg_res->start, resource_size(reg_res));

    //printk("%s, %d\n", __FILE__, __LINE__);
    if (NULL == reg_base_hiubus) {
        dev_err(kbdev->dev, "Invalid register resource\n");
        ret = -ENOENT;
    }

    //printk("%s, %d\n", __FILE__, __LINE__);

//JOHNT
    // Level reset mail

    // Level reset mail
    //Wr(P_RESET2_MASK, ~(0x1<<14));
    //Wr(P_RESET2_LEVEL, ~(0x1<<14));

    //Wr(P_RESET2_LEVEL, 0xffffffff);
    //Wr(P_RESET0_LEVEL, 0xffffffff);

    value = Rd(RESET0_MASK);
    value = value & (~(0x1<<20));
    //printk("line(%d), value=%x\n", __LINE__, value);
    Wr(RESET0_MASK, value);

    value = Rd(RESET0_LEVEL);
    value = value & (~(0x1<<20));
    //printk("line(%d), value=%x\n", __LINE__, value);
    Wr(RESET0_LEVEL, value);
///////////////
    value = Rd(RESET2_MASK);
    value = value & (~(0x1<<14));
    //printk("line(%d), value=%x\n", __LINE__, value);
    Wr(RESET2_MASK, value);

    value = Rd(RESET2_LEVEL);
    value = value & (~(0x1<<14));
    //printk("line(%d), value=%x\n", __LINE__, value);
    Wr(RESET2_LEVEL, value);

    value = Rd(RESET0_LEVEL);
    value = value | ((0x1<<20));
    //printk("line(%d), value=%x\n", __LINE__, value);
    Wr(RESET0_LEVEL, value);

    value = Rd(RESET2_LEVEL);
    value = value | ((0x1<<14));
    //printk("line(%d), value=%x\n", __LINE__, value);
    Wr(RESET2_LEVEL, value);

    udelay(10); // OR POLL for reset done

    kbase_reg_write(kbdev, GPU_CONTROL_REG(PWR_KEY), 0x2968A819, NULL);
    kbase_reg_write(kbdev, GPU_CONTROL_REG(PWR_OVERRIDE1), 0xfff | (0x20<<16), NULL);

    Mali_pwr_on_with_kdev(kbdev, 0x1);
    //printk("set PWR_ORRIDE, reg=%p, reg_start=%llx, reg_size=%zx, reg_mapped=%p\n",
    //        kbdev->reg, kbdev->reg_start, kbdev->reg_size, reg_base_hiubus);
	dev_dbg(kbdev->dev, "pm_callback_power_on %p\n",
			(void *)kbdev->dev->pm_domain);

    first = 0;
    //printk("%s, %d\n", __FILE__, __LINE__);
ret:
	error = pm_runtime_get_sync(kbdev->dev);
	if (error == 1) {
		/*
		 * Let core know that the chip has not been
		 * powered off, so we can save on re-initialization.
		 */
		ret = 0;
	}
	udelay(100);
#if 1

    core_ready = kbase_pm_get_ready_cores(kbdev, KBASE_PM_CORE_SHADER);
    l2_ready = kbase_pm_get_ready_cores(kbdev, KBASE_PM_CORE_L2);
    tiler_ready = kbase_pm_get_ready_cores(kbdev, KBASE_PM_CORE_TILER);
    //printk("core_ready=%llx, l2_ready=%llx, tiler_ready=%llx\n", core_ready, l2_ready, tiler_ready);
#endif
	dev_dbg(kbdev->dev, "pm_runtime_get_sync returned %d\n", error);

	return ret;
}

static void pm_callback_power_off(struct kbase_device *kbdev)
{
	dev_dbg(kbdev->dev, "pm_callback_power_off\n");
    //printk("%s, %d\n", __FILE__, __LINE__);
#if 0
    iounmap(reg_base_hiubus);
    reg_base_hiubus = NULL;
#endif
	pm_runtime_mark_last_busy(kbdev->dev);
	pm_runtime_put_autosuspend(kbdev->dev);
}

#ifdef KBASE_PM_RUNTIME
static int kbase_device_runtime_init(struct kbase_device *kbdev)
{
	int ret = 0;

	dev_dbg(kbdev->dev, "kbase_device_runtime_init\n");
	
	pm_runtime_set_autosuspend_delay(kbdev->dev, AUTO_SUSPEND_DELAY);
	pm_runtime_use_autosuspend(kbdev->dev);
	pm_runtime_set_active(kbdev->dev);
	
	dev_dbg(kbdev->dev, "kbase_device_runtime_init\n");
	pm_runtime_enable(kbdev->dev);

	if (!pm_runtime_enabled(kbdev->dev)) {
		dev_warn(kbdev->dev, "pm_runtime not enabled");
		ret = -ENOSYS;
	}

	return ret;
}

static void kbase_device_runtime_disable(struct kbase_device *kbdev)
{
	dev_dbg(kbdev->dev, "kbase_device_runtime_disable\n");
	pm_runtime_disable(kbdev->dev);
}
#endif
static int pm_callback_runtime_on(struct kbase_device *kbdev)
{
	dev_dbg(kbdev->dev, "pm_callback_runtime_on\n");

	return 0;
}

static void pm_callback_runtime_off(struct kbase_device *kbdev)
{
	dev_dbg(kbdev->dev, "pm_callback_runtime_off\n");
}

static void pm_callback_resume(struct kbase_device *kbdev)
{
	int ret = pm_callback_runtime_on(kbdev);

	WARN_ON(ret);
}

static void pm_callback_suspend(struct kbase_device *kbdev)
{
	pm_callback_runtime_off(kbdev);
}

struct kbase_pm_callback_conf pm_callbacks = {
	.power_on_callback = pm_callback_power_on,
	.power_off_callback = pm_callback_power_off,
	.power_suspend_callback = pm_callback_suspend,
	.power_resume_callback = pm_callback_resume,
#ifdef KBASE_PM_RUNTIME
	.power_runtime_init_callback = kbase_device_runtime_init,
	.power_runtime_term_callback = kbase_device_runtime_disable,
	.power_runtime_on_callback = pm_callback_runtime_on,
	.power_runtime_off_callback = pm_callback_runtime_off,
#else				/* KBASE_PM_RUNTIME */
	.power_runtime_init_callback = NULL,
	.power_runtime_term_callback = NULL,
	.power_runtime_on_callback = NULL,
	.power_runtime_off_callback = NULL,
#endif				/* KBASE_PM_RUNTIME */
};
