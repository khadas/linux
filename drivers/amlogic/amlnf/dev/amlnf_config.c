/*
 * drivers/amlogic/amlnf/dev/amlnf_config.c
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

#include "../include/amlnf_dev.h"

/***********************************************************************
 * Nand Config
 **********************************************************************/

struct amlnf_partition *amlnand_config = NULL;

int amlnand_get_partition_table(void)
{
	int ret = 0;
	u32	config_size;

	if (part_table == NULL) {
		aml_nand_msg("part_table from ACS is NULL, do not init nand");
		return -NAND_FAILED;
	}

	config_size = MAX_NAND_PART_NUM * sizeof(struct amlnf_partition);
	amlnand_config = aml_nand_malloc(config_size);
	if (!amlnand_config) {
		aml_nand_dbg("amlnand_config: malloc failed!");
		ret = -NAND_MALLOC_FAILURE;
	}

	/* show_partition_table(); */
	memcpy(amlnand_config, part_table, config_size);

	return ret;
}




