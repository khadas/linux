/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef CODEC_MM_SCATTER_PRIV_HEADER
#define CODEC_MM_SCATTER_PRIV_HEADER
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/codec_mm_scatter.h>

struct codec_mm_slot {
	struct codec_mm_s *mm;
	unsigned long page_header;
#define SLOT_FROM_CODEC_MM 1
#define SLOT_FROM_GET_FREE_PAGES 2
	int from_type;
	void *pagemap;
	ulong phy_addr;
	int pagemap_size;
	int page_num;
	int alloced_page_num;
	int next_bit;
	int sid;
	int isroot;
	int on_alloc_free;
	/* spin lock */
	spinlock_t lock;
	struct list_head sid_list;
	struct list_head free_list;
};

int codec_mm_dump_slot(struct codec_mm_slot *slot, void *buf, int size);

int codec_mm_scatter_mgt_init(struct device *dev);
int codec_mm_scatter_mgt_test(void);

int codec_mm_scatter_info_dump(void *buf, int size);
int codec_mm_dump_all_slots(void);
int codec_mm_dump_free_slots(void);
int codec_mm_dump_all_scatters(void);

int codec_mm_scatter_mgt_test(void);
int codec_mm_scatter_test(int mode, int p1, int p2);
int codec_mm_dump_all_hash_table(void);

int codec_mm_scatter_mask_for_keep(void *sc_mm);
int codec_mm_free_all_free_slots(void);
int codec_mm_scatter_inc_for_keeper(void *sc_mm);
int codec_mm_scatter_dec_keeper_user(void *sc_mm, int delay_ms);

int codec_mm_scatter_mgt_get_config(char *buf);
int codec_mm_scatter_mgt_set_config(const char *buf, size_t size);

int codec_mm_scatter_free_all_ignorecache(int flags);

void codec_mm_clear_alloc_infos(void);

#endif
