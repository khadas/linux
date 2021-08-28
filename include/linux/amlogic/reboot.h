/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/*******************************************************************/
#define	MESON_COLD_REBOOT					0
#define	MESON_NORMAL_BOOT					1
#define	MESON_FACTORY_RESET_REBOOT			2
#define	MESON_UPDATE_REBOOT					3
#define	MESON_FASTBOOT_REBOOT				4
#define	MESON_UBOOT_SUSPEND					5
#define	MESON_HIBERNATE						6
#define	MESON_BOOTLOADER_REBOOT				7
#define	MESON_RPMBP_REBOOT					9
#define MESON_QUIESCENT_REBOOT					10
#define	MESON_RESCUEPARTY_REBOOT			11
#define	MESON_KERNEL_PANIC					12
#define MESON_RECOVERY_QUIESCENT_REBOOT				14
#define MESON_TEST_REBOOT					15

struct reboot_reason_str {
	char *name;
};

u32 get_reboot_reason(void);
