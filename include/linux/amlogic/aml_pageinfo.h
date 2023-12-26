/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_PAGEINFO_H_
#define __AML_PAGEINFO_H_

#include <linux/mtd/mtd.h>

enum BL2_LAYOUT_VERS {
	LAYOUT_VER0,
	LAYOUT_VER1,
	LAYOUT_VER2,
	LAYOUT_VER3,
	LAYOUT_VER_MAX,
};

enum PAGE_INFO_V {
	PAGE_INFO_V1 = 1,
	PAGE_INFO_V2,
	PAGE_INFO_V3
};

int get_page_info_version(void);
int get_page_info_size(void);
int page_info_pre_init(u8 *boot_info, int version);
bool page_info_is_page(int page);
bool page_info_version_is_v1(void);
unsigned char *page_info_post_init(struct mtd_info *mtd, u8 cmd, u32 fip_size,
								u32 fip_copies);
void spinand_get_tpl_info(u32 *fip_size, u32 *fip_copies);
#endif
