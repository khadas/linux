// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/reboot.h>

#include <asm/system_misc.h>

#include <linux/amlogic/iomap.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/reboot.h>
//#include <asm/compiler.h>
#include <linux/kdebug.h>
#include <linux/arm-smccc.h>

#include <linux/syscore_ops.h>
#include <linux/cpu.h>

#include <linux/gpio.h>
#include <linux/of_gpio.h>

static void __iomem *reboot_reason_vaddr;
static u32 psci_function_id_restart;
static u32 psci_function_id_poweroff;
static char *kernel_panic;

static int sd_power_en_gpio;
static int vccio_en_gpio;

static struct reboot_reason_str reboot_reason_name[] = {
	[MESON_COLD_REBOOT] = { .name = "cold reboot" },
	[MESON_NORMAL_BOOT] = { .name = "normal boot" },
	[MESON_FACTORY_RESET_REBOOT] = { .name = "factory reset reboot" },
	[MESON_UPDATE_REBOOT] = { .name = "update reboot" },
	[MESON_FASTBOOT_REBOOT] = { .name = "fastboot reboot" },
	[MESON_UBOOT_SUSPEND] = { .name = "uboot suspend" },
	[MESON_HIBERNATE] = { .name = "hibernate" },
	[MESON_BOOTLOADER_REBOOT] = { .name = "bootloader reboot" },
	[MESON_RPMBP_REBOOT] = { .name = "rpmbp reboot" },
	[MESON_QUIESCENT_REBOOT] = { .name = "quiescent reboot" },
	[MESON_RESCUEPARTY_REBOOT] = { .name = "rescueparty reboot" },
	[MESON_KERNEL_PANIC] = { .name = "kernel panic" },
	[MESON_RECOVERY_QUIESCENT_REBOOT] = {
		.name = "recovery quiescent reboot" },
	[MESON_TEST_REBOOT] = { .name = "test reboot" },
};

u32 get_reboot_reason(void)
{
	u32 value;
	u32 reboot_reason;

	value = readl(reboot_reason_vaddr);
	reboot_reason = (value >> 12) & 0xf;

	return reboot_reason;
}
EXPORT_SYMBOL_GPL(get_reboot_reason);

static u32 parse_reason(const char *cmd)
{
	u32 reboot_reason = MESON_NORMAL_BOOT;

	if (cmd) {
		if (strcmp(cmd, "recovery") == 0 ||
		    strcmp(cmd, "factory_reset") == 0)
			reboot_reason = MESON_FACTORY_RESET_REBOOT;
		else if (strcmp(cmd, "cold_boot") == 0)
			reboot_reason = MESON_COLD_REBOOT;
		else if (strcmp(cmd, "update") == 0)
			reboot_reason = MESON_UPDATE_REBOOT;
		else if (strcmp(cmd, "fastboot") == 0)
			reboot_reason = MESON_FASTBOOT_REBOOT;
		else if (strcmp(cmd, "bootloader") == 0)
			reboot_reason = MESON_BOOTLOADER_REBOOT;
		else if (strcmp(cmd, "rpmbp") == 0)
			reboot_reason = MESON_RPMBP_REBOOT;
		else if (strcmp(cmd, "rescueparty") == 0)
			reboot_reason = MESON_RESCUEPARTY_REBOOT;
		else if (strcmp(cmd, "uboot_suspend") == 0)
			reboot_reason = MESON_UBOOT_SUSPEND;
		else if (strcmp(cmd, "quiescent") == 0 ||
			strcmp(cmd, "userrequested,recovery,quiescent") == 0 ||
			strcmp(cmd, "reboot-ab-update,quiescent") == 0 ||
			strcmp(cmd, "unattended,ota_update,quiescent") == 0 ||
			strcmp(cmd, ",quiescent") == 0)
			reboot_reason = MESON_QUIESCENT_REBOOT;
		else if (strcmp(cmd, "recovery,quiescent") == 0 ||
			 strcmp(cmd, "factory_reset,quiescent") == 0 ||
			 strcmp(cmd, "quiescent,recovery") == 0 ||
			 strcmp(cmd, "quiescent,factory_reset") == 0)
			reboot_reason = MESON_RECOVERY_QUIESCENT_REBOOT;
		else if (strcmp(cmd, "reboot_test") == 0)
			reboot_reason = MESON_TEST_REBOOT;
	} else {
		if (kernel_panic) {
			if (strcmp(kernel_panic, "kernel_panic") == 0)
				reboot_reason = MESON_KERNEL_PANIC;
		}
	}

	pr_info("reboot reason %d\n", reboot_reason);

	return reboot_reason;
}

static noinline int __invoke_psci_fn_smc(u64 function_id, u64 arg0, u64 arg1,
					 u64 arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc((unsigned long)function_id,
		      (unsigned long)arg0,
		      (unsigned long)arg1,
		      (unsigned long)arg2,
		      0, 0, 0, 0, &res);
	return res.a0;
}

void meson_smc_restart(u64 function_id, u64 reboot_reason)
{
	__invoke_psci_fn_smc(function_id,
			     reboot_reason, 0, 0);
}

void meson_common_restart(char mode, const char *cmd)
{
	u32 reboot_reason = parse_reason(cmd);

	if (psci_function_id_restart)
		meson_smc_restart((u64)psci_function_id_restart,
				  (u64)reboot_reason);
}

void sd_card_power_reset(void)
{
	int ret = 0;

	if (sd_power_en_gpio < 1 || vccio_en_gpio < 1)
		return;

	gpio_free(vccio_en_gpio);
	gpio_free(sd_power_en_gpio);

	ret = gpio_request_one(vccio_en_gpio,
			GPIOF_OUT_INIT_LOW, "SD_VCCIO");

	if (ret < 0) {
		pr_err("%s, gpio_request_one failed, ret=%d!\n",
				__func__, ret);
		return;
	}

	mdelay(10);

	ret = gpio_direction_output(vccio_en_gpio, 1);
	if (ret < 0) {
		pr_err("%s, gpio_direction_output failed, ret=%d!\n",
				__func__, ret);

		return;
	}

	ret = gpio_request_one(sd_power_en_gpio,
					GPIOF_OUT_INIT_HIGH, "SD_POWER");
	if (ret < 0) {
		pr_err("%s, gpio_request_one failed, ret=%d!\n",
				__func__, ret);

		return;
	}
	mdelay(10);

	ret = gpio_direction_output(vccio_en_gpio, 0);
	if (ret < 0) {
		pr_err("%s, gpio_direction_output failed, ret=%d!\n",
				__func__, ret);
		return;
	}
	ret = gpio_direction_output(sd_power_en_gpio, 0);
	if (ret < 0) {
		pr_err("%s, gpio_direction_output failed, ret=%d!\n",
				__func__, ret);
		return;
	}
	mdelay(10);

	gpio_free(vccio_en_gpio);
	gpio_free(sd_power_en_gpio);

	pr_info("Reset SDCARD power.\n");
}

static int do_aml_restart(struct notifier_block *nb, unsigned long reboot_mode,
			  void *cmd)
{
	sd_card_power_reset();
	meson_common_restart(reboot_mode, cmd);

	return NOTIFY_DONE;
}

static void do_aml_poweroff(void)
{
	sd_card_power_reset();

	/* TODO: Add poweroff capability */
	__invoke_psci_fn_smc(0x82000042, 1, 0, 0);
	__invoke_psci_fn_smc(psci_function_id_poweroff,
			     0, 0, 0);
}

static int panic_notify(struct notifier_block *self,
			unsigned long cmd, void *ptr)
{
	kernel_panic = "kernel_panic";

	return NOTIFY_DONE;
}

static struct notifier_block panic_notifier = {
	.notifier_call	= panic_notify,
};

ssize_t reboot_reason_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	unsigned int value, len, reboot_reason;

	if (!reboot_reason_vaddr)
		return 0;
	value = readl(reboot_reason_vaddr);
	reboot_reason = ((value >> 12) & 0xf);
	len = sprintf(buf, "reboot reason = %d, %s\n", reboot_reason,
		      reboot_reason_name[reboot_reason].name);

	return len;
}

static DEVICE_ATTR_RO(reboot_reason);

static void disable_non_bootcpu_shutdown(void)
{
	int error = 0, cpu, primary = 0;

	if (!cpu_online(primary))
		primary = cpumask_first(cpu_online_mask);

	cpu_hotplug_enable();

	for_each_online_cpu(cpu) {
		if (cpu == primary)
			continue;

		error = cpu_down(cpu);
		if (error) {
			pr_err("Error taking CPU%d down: %d\n", cpu, error);
			break;
		}
	}

	if (!error)
		WARN_ON(num_online_cpus() > 1);
	else
		pr_err("Non-boot CPUs are not disabled\n");

	cpu_hotplug_disable();
}

static struct syscore_ops disable_non_bootcpu_syscore_ops = {
	.shutdown		= disable_non_bootcpu_shutdown,
};

static int __init reboot_pm_init_ops(void)
{
	register_syscore_ops(&disable_non_bootcpu_syscore_ops);
	return 0;
}

static struct notifier_block aml_restart_nb = {
	.notifier_call = do_aml_restart,
	.priority = 130,
};

static int aml_restart_probe(struct platform_device *pdev)
{
	u32 id;
	int ret;

	if (!of_property_read_u32(pdev->dev.of_node, "sys_reset", &id)) {
		psci_function_id_restart = id;
		register_restart_handler(&aml_restart_nb);
	}

	if (!of_property_read_u32(pdev->dev.of_node, "sys_poweroff", &id)) {
		psci_function_id_poweroff = id;
		pm_power_off = do_aml_poweroff;
	}

	sd_power_en_gpio = 0;
	vccio_en_gpio = 0;
	sd_power_en_gpio = of_get_named_gpio(pdev->dev.of_node,
			"sd_power_en_gpio", 0);

	vccio_en_gpio = of_get_named_gpio(pdev->dev.of_node,
			"vccio_en_gpio", 0);

	reboot_reason_vaddr = devm_platform_ioremap_resource(pdev, 0);
	if (!IS_ERR(reboot_reason_vaddr)) {
		ret = device_create_file(&pdev->dev, &dev_attr_reboot_reason);
		if (ret != 0)
			pr_err("%s, device create file failed, ret = %d!\n",
			       __func__, ret);
	}

	if (of_property_read_bool(pdev->dev.of_node,
					"dis_nb_cpus_in_shutdown")) {
		pr_info("Enable disable_nonboot_cpus in syscore shutdown.\n");
		reboot_pm_init_ops();
	}

	ret = register_die_notifier(&panic_notifier);
	if (ret != 0) {
		pr_err("%s,register die notifier failed,ret =%d!\n",
		       __func__, ret);
		return ret;
	}

	/* Register a call for panic conditions. */
	ret = atomic_notifier_chain_register(&panic_notifier_list,
					     &panic_notifier);
	if (ret != 0) {
		pr_err("%s,register panic notifier failed,ret =%d!\n",
		       __func__, ret);
		return ret;
	}
	return 0;
}

static int aml_restart_remove(struct platform_device *pdev)
{
	u32 id;

	atomic_notifier_chain_unregister(&panic_notifier_list,
						&panic_notifier);
	unregister_die_notifier(&panic_notifier);

	if (of_property_read_bool(pdev->dev.of_node,
					"dis_nb_cpus_in_shutdown"))
		unregister_syscore_ops(&disable_non_bootcpu_syscore_ops);

	if (!IS_ERR(reboot_reason_vaddr))
		device_remove_file(&pdev->dev, &dev_attr_reboot_reason);

	if (!of_property_read_u32(pdev->dev.of_node, "sys_reset", &id))
		unregister_restart_handler(&aml_restart_nb);

	return 0;
}

static const struct of_device_id of_aml_restart_match[] = {
	{ .compatible = "aml, reboot", },
	{},
};
MODULE_DEVICE_TABLE(of, of_aml_restart_match);

static struct platform_driver aml_restart_driver = {
	.probe = aml_restart_probe,
	.remove = aml_restart_remove,
	.driver = {
		.name = "aml-restart",
		.of_match_table = of_match_ptr(of_aml_restart_match),
	},
};

module_platform_driver(aml_restart_driver);

MODULE_LICENSE("GPL");
