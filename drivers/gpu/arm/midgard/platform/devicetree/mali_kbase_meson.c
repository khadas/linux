/**
 ** Meson hardware specific initialization
 **/
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include <mali_kbase.h>
#include "mali_kbase_meson.h"

int meson_gpu_reset(struct kbase_device *kbdev)
{
	struct meson_context *platform = kbdev->platform_context;
    void __iomem *reg_base_reset = platform->reg_base_reset;
    u32 value;

    //JOHNT
    // Level reset mail
 
    // Level reset mail
    //writel(~(0x1<<14), reg_base_reset + P_RESET2_MASK * 4);
    //writel(~(0x1<<14), reg_base_reset + P_RESET2_LEVEL * 4);

    //writel(0xffffffff, reg_base_reset + P_RESET2_LEVEL * 4);
    //writel(0xffffffff, reg_base_reset + P_RESET0_LEVEL * 4);

    MESON_PRINT("%s, %d\n", __func__, __LINE__);
    MESON_PRINT("reg_base=%p, reset0=%p\n", reg_base_reset, reg_base_reset + RESET0_MASK * 4);
    udelay(100);
#if 0
    value = readl(reg_base_reset + RESET0_REGISTER * 4);
    MESON_PRINT("line(%d), RESET0_REGISTER=%x\n", __LINE__, value);
#endif

    udelay(100);
    MESON_PRINT("%s, %d\n", __func__, __LINE__);
    value = readl(reg_base_reset + RESET0_MASK * 4);
    value = value & (~(0x1<<20));
    writel(value, reg_base_reset + RESET0_MASK * 4);

    udelay(100);
#if 0
    value = readl(reg_base_reset + RESET0_REGISTER * 4);
    MESON_PRINT("line(%d), RESET0_REGISTER=%x\n", __LINE__, value);
    udelay(100);
#endif

    value = readl(reg_base_reset + RESET0_LEVEL * 4);
    value = value & (~(0x1<<20));
    //MESON_PRINT("line(%d), value=%x\n", __LINE__, value);
    writel(value, reg_base_reset + RESET0_LEVEL * 4);
    udelay(100);

#if 0
    value = readl(reg_base_reset + RESET0_REGISTER * 4);
    MESON_PRINT("line(%d), RESET0_REGISTER=%x\n", __LINE__, value);
    udelay(100);
#endif

///////////////
#if 0
    value = readl(reg_base_reset + RESET2_REGISTER * 4);
    MESON_PRINT("line(%d), RESET2_REGISTER=%x\n", __LINE__, value);
#endif

    udelay(100);
    value = readl(reg_base_reset + RESET2_MASK * 4);
    value = value & (~(0x1<<14));
    //MESON_PRINT("line(%d), value=%x\n", __LINE__, value);
    writel(value, reg_base_reset + RESET2_MASK * 4);

#if 0
    value = readl(reg_base_reset + RESET2_REGISTER * 4);
    MESON_PRINT("line(%d), RESET2_REGISTER=%x\n", __LINE__, value);
#endif

    value = readl(reg_base_reset + RESET2_LEVEL * 4);
    value = value & (~(0x1<<14));
    //MESON_PRINT("line(%d), value=%x\n", __LINE__, value);
    writel(value, reg_base_reset + RESET2_LEVEL * 4);
    udelay(100);

#if 0
    value = readl(reg_base_reset + RESET2_REGISTER * 4);
    MESON_PRINT("line(%d), RESET2_REGISTER=%x\n", __LINE__, value);
#endif

    udelay(100);
    value = readl(reg_base_reset + RESET0_LEVEL * 4);
    value = value | ((0x1<<20));
    //MESON_PRINT("line(%d), value=%x\n", __LINE__, value);
    writel(value, reg_base_reset + RESET0_LEVEL * 4);
#if 0
    value = readl(reg_base_reset + RESET2_REGISTER * 4);
    MESON_PRINT("line(%d), RESET2_REGISTER=%x\n", __LINE__, value);
#endif

    udelay(100);
    value = readl(reg_base_reset + RESET2_LEVEL * 4);
    value = value | ((0x1<<14));
    //MESON_PRINT("line(%d), value=%x\n", __LINE__, value);
    writel(value, reg_base_reset + RESET2_LEVEL * 4);
#if 0
    value = readl(reg_base_reset + RESET2_REGISTER * 4);
    MESON_PRINT("line(%d), RESET2_REGISTER=%x\n", __LINE__, value);
#endif

    udelay(10); // OR POLL for reset done

    return 0;
}

void  meson_gpu_pwr_on(struct kbase_device *kbdev, u32  mask)
{
    u32    part1_done = 0;
    u32     value = 0;
    u32     count = 0;

    kbdev->pm.backend.gpu_powered = true;
    MESON_PRINT("%s, %d begin\n", __func__,__LINE__);

#if 0
    value = 0x10 | (0x1<<16);
#else
    value = 0xfff | (0x20<<16);
#endif
    while (part1_done != value) {
        Mali_WrReg(GPU_CONTROL_REG(PWR_KEY), 0x2968A819);
        Mali_WrReg(GPU_CONTROL_REG(PWR_OVERRIDE1), value);
        part1_done = Mali_RdReg(GPU_CONTROL_REG(PWR_OVERRIDE1));
        MESON_PRINT("write again, count=%d, overrider1=%x\n", count, part1_done);
        udelay(20);
        count ++;
        if (0 == (count %100)) MESON_PRINT("write again, count%d\n", count);
    }
    part1_done = Mali_RdReg(GPU_CONTROL_REG(PWR_OVERRIDE1));
    MESON_PRINT("write again, count=%d, overrider1=%x\n", count, part1_done);

    Mali_WrReg(0x0000024, 0xffffffff); // clear interrupts

    MESON_PRINT("%s, %d\n", __func__,__LINE__);
    if ((mask & 0x1) != 0 ) {
        MESON_PRINT("%s, %d\n", __func__,__LINE__);
        Mali_WrReg(0x00000190, 0xffffffff);    // Power on all cores
        Mali_WrReg(0x00000194, 0xffffffff);    // Power on all cores
        Mali_WrReg(0x000001a0, 0xffffffff);    // Power on all cores
        Mali_WrReg(0x000001a4, 0xffffffff);    // Power on all cores
    }
    MESON_PRINT("power on %d\n", __LINE__);

    if ( (mask >> 1) != 0 ) {
        Mali_WrReg(0x00000180, mask >> 1);    // Power on all cores
        Mali_WrReg(0x00000184, 0x0);    // Power on all cores
        MESON_PRINT("%s, %d\n", __func__,__LINE__);
    }

    MESON_PRINT("%s, %d\n", __func__,__LINE__);
    if ( mask != 0 ) {
        MESON_PRINT("%s, %d\n", __func__,__LINE__);
        udelay(10);
        part1_done = Mali_RdReg(0x0000020);
        while(part1_done ==0) {
            part1_done = Mali_RdReg(0x00000020);
            udelay(10);
        }

        MESON_PRINT("%s, %d\n", __func__,__LINE__);
        Mali_WrReg(0x0000024, 0xffffffff); // clear interrupts
    }
    MESON_PRINT("%s, %d end\n", __func__,__LINE__);
}

void  meson_gpu_pwr_off(struct kbase_device *kbdev, u32  mask)
{
#if 1
    u32 part1_done;
    part1_done = 0;
    Mali_WrReg(0x0000024, 0xffffffff); // clear interrupts

    if ( (mask >> 1) != 0 ) {
        Mali_WrReg(0x000001C0, mask >> 1);    // Power off all cores
        Mali_WrReg(0x000001C4, 0x0);          // Power off all cores
    }

    if (  (mask & 0x1) != 0 ) {
        Mali_WrReg(0x000001D0, 0xffffffff);    // Power off all cores
        Mali_WrReg(0x000001D4, 0xffffffff);    // Power off all cores
        Mali_WrReg(0x000001E0, 0xffffffff);    // Power off all cores
        Mali_WrReg(0x000001E4, 0xffffffff);    // Power off all cores
    }

    if ( mask != 0 ) {
        part1_done = Mali_RdReg(0x0000020);
        while((part1_done ==0)) { part1_done = Mali_RdReg(0x00000020); }
        MESON_PRINT("Mali_pwr_off:gpu_irq : %x\n", part1_done);
        Mali_WrReg(0x0000024, 0xffffffff); // clear interrupts
    }
#endif
}




static int kbase_platform_meson_init(struct kbase_device *kbdev)
{
#if 0
	int err;
#endif
	struct device_node *gpu_dn = kbdev->dev->of_node;
#ifdef CONFIG_MALI_MIDGARD_DVFS
	unsigned long flags;
#endif /* CONFIG_MALI_MIDGARD_DVFS */
	struct meson_context *platform;
    u32 part1_done = 0;

	platform = kmalloc(sizeof(struct meson_context), GFP_KERNEL);

	if (!platform)
		return -ENOMEM;

	memset(platform, 0, sizeof(struct meson_context));

	kbdev->platform_context = (void *) platform;

    platform->reg_base_reset = of_iomap(gpu_dn, 1);
	_dev_info(kbdev->dev, "reset io source  0x%p\n",platform->reg_base_reset);

	platform->reg_base_aobus = of_iomap(gpu_dn, 2);
	_dev_info(kbdev->dev, "ao io source  0x%p\n", platform->reg_base_aobus);

	platform->reg_base_hiubus = of_iomap(gpu_dn, 3);
	_dev_info(kbdev->dev, "hiu io source  0x%p\n", platform->reg_base_hiubus);

    platform->clk_mali = devm_clk_get(kbdev->dev, "gpu_mux");
	if (IS_ERR(platform->clk_mali)) {
		dev_err(kbdev->dev, "failed to get clock pointer\n");
	} else {
        clk_prepare_enable(platform->clk_mali);
        clk_set_rate(platform->clk_mali, 285000000);
    }
    MESON_PRINT("%s, %d begin\n", __func__, __LINE__);
    meson_gpu_reset(kbdev);
    meson_gpu_pwr_on(kbdev, 0xe);

    part1_done = Mali_RdReg(GPU_CONTROL_REG(PWR_OVERRIDE1));
    MESON_PRINT("line%d, overrider1=%x\n", __LINE__, part1_done);
    meson_gpu_pwr_off(kbdev, 0xe);
#if 1
    part1_done = Mali_RdReg(GPU_CONTROL_REG(PWR_OVERRIDE1));
    MESON_PRINT("line%d, overrider1=%x\n", __LINE__, part1_done);

    meson_gpu_reset(kbdev);
    meson_gpu_pwr_on(kbdev, 0xe);
    part1_done = Mali_RdReg(GPU_CONTROL_REG(PWR_OVERRIDE1));
    MESON_PRINT("line%d, overrider1=%x\n", __LINE__, part1_done);
#endif
#if 0
	platform->cmu_pmu_status = 0;
	platform->dvfs_wq = NULL;
	platform->polling_speed = 100;
	gpu_debug_level = DVFS_WARNING;
#endif

	mutex_init(&platform->gpu_clock_lock);
	mutex_init(&platform->gpu_dvfs_handler_lock);
	spin_lock_init(&platform->gpu_dvfs_spinlock);
#if 0
	err = gpu_control_module_init(kbdev);
	if (err)
		goto clock_init_fail;

	/* dvfs gobernor init*/
	gpu_dvfs_governor_init(kbdev, G3D_DVFS_GOVERNOR_DEFAULT);
#endif
#ifdef CONFIG_MALI_MIDGARD_DVFS
	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	platform->wakeup_lock = 0;
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
#endif /* CONFIG_MALI_MIDGARD_DVFS */
#if 0
	/* dvfs handler init*/
	gpu_dvfs_handler_init(kbdev);

	err = gpu_notifier_init(kbdev);
	if (err)
		goto notifier_init_fail;

	err = gpu_create_sysfs_file(kbdev->dev);
	if (err)
		goto sysfs_init_fail;
#endif

    MESON_PRINT("%s, %d end\n", __func__, __LINE__);
	return 0;
#if 0
clock_init_fail:
notifier_init_fail:
sysfs_init_fail:
	kfree(platform);

	return err;
#endif
}

/**
 ** Meson hardware specific termination
 **/
static void kbase_platform_meson_term(struct kbase_device *kbdev)
{
	struct meson_context *platform;
	platform = (struct meson_context *) kbdev->platform_context;
#if 0
	gpu_notifier_term();

#ifdef CONFIG_MALI_MIDGARD_DVFS
	gpu_dvfs_handler_deinit(kbdev);
#endif /* CONFIG_MALI_MIDGARD_DVFS */

	gpu_control_module_term(kbdev);
#endif

	kfree(kbdev->platform_context);
	kbdev->platform_context = 0;

#if 0
	gpu_remove_sysfs_file(kbdev->dev);
#endif
}

struct kbase_platform_funcs_conf platform_funcs = {
	.platform_init_func = &kbase_platform_meson_init,
	.platform_term_func = &kbase_platform_meson_term,
};

