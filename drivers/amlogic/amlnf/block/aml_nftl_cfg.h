/*
 * drivers/amlogic/amlnf/block/aml_nftl_cfg.h
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

#include "aml_nftl_type.h"
#include "../include/amlnf_cfg.h"

/* #define  PRINT printk */

#define NFTL_DONT_CACHE_DATA		0
#define SUPPORT_GC_READ_RECLAIM		0
#define SUPPORT_WEAR_LEVELING		0
#define NFTL_ERASE			0

#ifdef NAND_ADJUST_PART_TABLE
#define PART_RESERVED_BLOCK_RATIO	10
#else
#define PART_RESERVED_BLOCK_RATIO	8
#endif

#define MIN_FREE_BLOCK_NUM		6
#define GC_THRESHOLD_FREE_BLOCK_NUM	4

#define MIN_FREE_BLOCK			5

#define GC_THRESHOLD_RATIO_NUMERATOR	2
#define GC_THRESHOLD_RATIO_DENOMINATOR	3

#define MAX_CACHE_WRITE_NUM		4
#define NFTL_CACHE_FLUSH_SYNC		1


extern uint aml_nftl_get_part_cap(void *_part);
extern void aml_nftl_set_part_test(void *_part, uint test);
extern void *aml_nftl_get_part_priv(void *_part);
extern void aml_nftl_add_part_total_write(void *_part);
extern void aml_nftl_add_part_total_read(void *_part);
extern ushort aml_nftl_get_part_write_cache_nums(void *_part);

