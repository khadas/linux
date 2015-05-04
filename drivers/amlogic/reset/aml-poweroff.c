/*
 * drivers/amlogic/reset/aml-poweroff.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
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
/*
 * Commands accepted by the arm_machine_restart() system call.
 *
 * MESON_NORMAL_BOOT			Restart system normally.
 * MESON_FACTORY_RESET_REBOOT      Restart system into recovery factory reset.
 * MESON_UPDATE_REBOOT			Restart system into recovery update.
 * MESON_CHARGING_REBOOT		Restart system into charging.
 * MESON_CRASH_REBOOT			Restart system with system crach.
 * MESON_FACTORY_TEST_REBOOT	Restart system into factory test.
 * MESON_SYSTEM_SWITCH_REBOOT	Restart system for switch other OS.
 * MESON_SAFE_REBOOT			Restart system into safe mode.
 * MESON_LOCK_REBOOT			Restart system into lock mode.
 */
#define	MESON_CHARGING_REBOOT				0x0
#define	MESON_NORMAL_BOOT				0x01010101
#define	MESON_FACTORY_RESET_REBOOT			0x02020202
#define	MESON_UPDATE_REBOOT				0x03030303
#define	MESON_CRASH_REBOOT				0x04040404
#define	MESON_FACTORY_TEST_REBOOT			0x05050505
#define	MESON_SYSTEM_SWITCH_REBOOT			0x06060606
#define	MESON_SAFE_REBOOT				0x07070707
#define	MESON_LOCK_REBOOT				0x08080808
#define	MESON_USB_BURNER_REBOOT				0x09090909
#define MESON_UBOOT_SUSPEND				0x0b0b0b0b
#define	MESON_REBOOT_CLEAR				0xdeaddead

#define AO_RTI_STATUS_REG1	((0x00 << 10) | (0x01 << 2))
#define WATCHDOG_TC		0x2640
void meson_common_restart(char mode, const char *cmd)
{
	u32 reboot_reason = MESON_NORMAL_BOOT;
	if (cmd) {
		if (strcmp(cmd, "charging_reboot") == 0)
			reboot_reason = MESON_CHARGING_REBOOT;
		else if (strcmp(cmd, "recovery") == 0 ||
			 strcmp(cmd, "factory_reset") == 0)
			reboot_reason = MESON_FACTORY_RESET_REBOOT;
		else if (strcmp(cmd, "update") == 0)
			reboot_reason = MESON_UPDATE_REBOOT;
		else if (strcmp(cmd, "report_crash") == 0)
			reboot_reason = MESON_CRASH_REBOOT;
		else if (strcmp(cmd, "factory_testl_reboot") == 0)
			reboot_reason = MESON_FACTORY_TEST_REBOOT;
		else if (strcmp(cmd, "switch_system") == 0)
			reboot_reason = MESON_SYSTEM_SWITCH_REBOOT;
		else if (strcmp(cmd, "safe_mode") == 0)
			reboot_reason = MESON_SAFE_REBOOT;
		else if (strcmp(cmd, "lock_system") == 0)
			reboot_reason = MESON_LOCK_REBOOT;
		else if (strcmp(cmd, "usb_burner_reboot") == 0)
			reboot_reason = MESON_USB_BURNER_REBOOT;
		else if (strcmp(cmd, "uboot_suspend") == 0)
			reboot_reason = MESON_UBOOT_SUSPEND;
	}
	aml_write_aobus(AO_RTI_STATUS_REG1, reboot_reason);
	if (is_meson_m8m2_cpu())
		aml_write_cbus(WATCHDOG_TC, (1<<19) | 100);
	if (is_meson_m8_cpu())
		aml_write_cbus(WATCHDOG_TC, (1<<22) | 100);
}
static void do_aml_restart(enum reboot_mode reboot_mode, const char *cmd)
{
	meson_common_restart(reboot_mode, cmd);
}

static void do_aml_poweroff(void)
{
	/* TODO: Add poweroff capability */
	meson_common_restart('h', "charging_reboot");
}

static int aml_restart_probe(struct platform_device *pdev)
{
	pm_power_off = do_aml_poweroff;
	arm_pm_restart = do_aml_restart;
	return 0;
}

static const struct of_device_id of_aml_restart_match[] = {
	{ .compatible = "aml, restart", },
	{},
};
MODULE_DEVICE_TABLE(of, of_aml_restart_match);

static struct platform_driver aml_restart_driver = {
	.probe = aml_restart_probe,
	.driver = {
		.name = "aml-restart",
		.of_match_table = of_match_ptr(of_aml_restart_match),
	},
};

static int __init aml_restart_init(void)
{
	return platform_driver_register(&aml_restart_driver);
}
device_initcall(aml_restart_init);
