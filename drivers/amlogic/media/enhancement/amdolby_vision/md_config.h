/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _MD_CONFIG_H_
#define _MD_CONFIG_H_

#include <linux/types.h>

#define PKT_NUM_MAX 5

struct first_pkt {
	unsigned char md_len_hi;
	unsigned char md_len_lo;
	unsigned char md_body[119];
};

struct following_pkt {
	unsigned char md_body[121];
};

struct metadata_pkt {
	unsigned char h_0;
	unsigned char h_1;
	unsigned char h_2;
	union {
		struct first_pkt md_fst;
		struct following_pkt md_flw;
	} md;
};

void packetize_md(unsigned char *p_md, int len, u32 *crc_val);
u32 get_crc32(const void *data, size_t data_size);
void malloc_md_pkt(void);
void free_md_pkt(void);
#endif

