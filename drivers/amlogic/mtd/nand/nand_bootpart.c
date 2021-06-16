// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/aml_mtd_nand.h>
#include <linux/amlogic/gki_module.h>

extern struct mtd_info *aml_mtd_info[NAND_MAX_DEVICE];
static char *cmdline;

struct boot_area_entry general_boot_part_entry[MAX_BOOT_AREA_ENTRIES] = {
	{BAE_BB1ST, BOOT_AREA_BB1ST, 0, BOOT_FIRST_BLOB_SIZE},
	{BAE_BL2E, BOOT_AREA_BL2E, 0, 0x40000},
	{BAE_BL2X, BOOT_AREA_BL2X, 0, 0x40000},
	{BAE_DDRFIP, BOOT_AREA_DDRFIP, 0, 0x40000},
	{BAE_DEVFIP, BOOT_AREA_DEVFIP, 0, 0x380000},
};

struct boot_layout general_boot_layout = {
	.boot_entry = general_boot_part_entry
};

struct storage_startup_parameter g_ssp;

static void storage_boot_layout_debug_info(struct boot_layout *boot_layout)
{
	struct boot_area_entry *boot_entry = boot_layout->boot_entry;
	int i;

	pr_info("boot area list:\n");
	for (i = 0; i < MAX_BOOT_AREA_ENTRIES && boot_entry[i].size; i++) {
		pr_info("%10s    ", boot_entry[i].name);
		pr_info("%10llx    ", boot_entry[i].offset);
		pr_info("%10llx\n", boot_entry[i].size);
	}
}

#define NAND_RSV_BLOCK_NUM 48
#define NSP_PAGE0_DISABLE 1
static int storage_get_and_parse_ssp(void)
{
	struct storage_startup_parameter *ssp;
	union storage_independent_parameter *sip;
	struct mtd_info *mtd;

	mtd = aml_mtd_info[0];
	ssp = &g_ssp;
	memset(ssp, 0, sizeof(struct storage_startup_parameter));
	sip = &ssp->sip;

	ssp->boot_backups = 8;
	sip->nsp.page_size =  mtd->writesize;
	sip->nsp.block_size =  mtd->erasesize;
	sip->nsp.pages_per_block = mtd->erasesize /
		mtd->writesize;
	sip->nsp.layout_reserve_size =
		NAND_RSV_BLOCK_NUM * sip->nsp.block_size;

	pr_info("boot_device:%d\n", ssp->boot_device);
	pr_info("boot_seq:%d\n", ssp->boot_seq);
	pr_info("boot_backups:%d\n", ssp->boot_backups);

	return 0;
}

#define STORAGE_ROUND_UP_IF_UNALIGN(x, y)		\
	({						\
		typeof(y) y_ = y;			\
		(((x) + (y_) - 1) & (~(y_ - 1)));	\
	})
#define NAND_RSV_OFFSET	1024
#define ALIGN_SIZE	(4096)
int storage_boot_layout_general_setting(struct boot_layout *boot_layout)
{
	struct storage_startup_parameter *ssp = &g_ssp;
	struct boot_area_entry *boot_entry = NULL;
	u64 align_size, reserved_size = 0;
	u8 i, cal_copy = ssp->boot_backups;

	boot_entry = boot_layout->boot_entry;
	reserved_size = ssp->sip.nsp.layout_reserve_size;
	align_size =
		((NAND_RSV_OFFSET / cal_copy) * (u64)(ssp->sip.nsp.page_size));
	boot_entry[0].size =
		STORAGE_ROUND_UP_IF_UNALIGN(boot_entry[0].size, align_size);
	ssp->boot_entry[0].size = boot_entry[0].size;
	pr_info("ssp->boot_entry[0] offset:0x%x, size:0x%x\n",
		ssp->boot_entry[0].offset, ssp->boot_entry[0].size);
	pr_info("cal_copy:0x%x\n", cal_copy);
	pr_info("align_size:0x%llx\n", align_size);
	pr_info("reserved_size:0x%llx\n", reserved_size);
	align_size = ssp->sip.nsp.block_size;
	for (i = 1; i < MAX_BOOT_AREA_ENTRIES && boot_entry[i - 1].size; i++) {
		boot_entry[i].size =
		STORAGE_ROUND_UP_IF_UNALIGN(boot_entry[i].size, align_size);
		boot_entry[i].offset = boot_entry[i - 1].offset +
			boot_entry[i - 1].size * cal_copy + reserved_size;
		reserved_size = 0;
		ssp->boot_entry[i].size = boot_entry[i].size;
		ssp->boot_entry[i].offset = boot_entry[i].offset;
	}

	return 0;
}

int aml_nand_parse_env(char *cmd)
{
	struct boot_area_entry *boot_entry = NULL;
	char *p = cmd;
	u8 i;

	p = p ? strchr(p, ':') : NULL;
	if (!p) {
		pr_info("WARN, no bl2e/x, so use reserved\n");
		return 0;
	}

	p++;
	pr_info("%s\n", p);
	boot_entry = general_boot_layout.boot_entry;
	for (i = BOOT_AREA_BL2E; i <= BOOT_AREA_DEVFIP && p; i++) {
		boot_entry[i].size = memparse(p, NULL);
		p = strchr(p, ',') + 1;
		pr_info("%s_size(0x%llx)\n", boot_entry[i].name, boot_entry[i].size);
	}

	return 0;
}

int aml_nand_param_check_and_layout_init(void)
{
	int ret = -1;

	ret = storage_get_and_parse_ssp();
	if (ret < 0)
		return -1;

	aml_nand_parse_env(cmdline);
	storage_boot_layout_general_setting(&general_boot_layout);
	storage_boot_layout_debug_info(&general_boot_layout);

	return ret;
}

static int mtdbootpart_setup(char *s)
{
	cmdline = s;
	return 0;
}

__setup("mtdbootparts=", mtdbootpart_setup);

