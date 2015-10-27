/*
 * drivers/amlogic/amlnf/block/aml_nftl_block.h
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

#ifndef __AML_NFTL_BLOCK_H
#define __AML_NFTL_BLOCK_H


#include "aml_nftl_cfg.h"
#include <linux/device.h>
#include <linux/platform_device.h>
#include "../ntd/aml_ntd.h"
#include "../include/amlnf_dev.h"

#define CFG_M2M_TRANSFER_TBL		(1)

#pragma pack(1)

#define AML_NFTL1_MAGIC		"nftl-code"
#define AML_NFTL2_MAGIC		"nftl-data"

#define AML_NFTL_MAJOR		250
#define TIMESTAMP_LENGTH		15
#define MAX_TIMESTAMP_NUM		((1<<(TIMESTAMP_LENGTH-1))-1)
#define AML_NFTL_BOUNCE_SIZE	0x40000

#define NFTL_MAX_SCHEDULE_TIMEOUT	1000
#define NFTL_FLUSH_DATA_TIME		1
#define NFTL_CACHE_FORCE_WRITE_LEN	16

#define RET_YES		1
#define RET_NO		0

#define  PRINT aml_nftl_dbg

#define nftl_notifier_to_dev(l)	container_of(l, struct aml_nftl_dev, nb)

struct aml_nftl_part_t;
struct _ftl_status;

struct aml_nftl_blk;


struct aml_nftl_dev {
	uint64_t size;
	struct device dev;
	struct ntd_info *ntd;
	struct aml_nftl_part_t *aml_nftl_part;
	struct mutex *aml_nftl_lock;
	struct task_struct *nftl_thread;
	struct timespec ts_write_start;
	struct notifier_block nb;
	struct class debug;
	struct _nftl_cfg nftl_cfg;
	int sync_flag;
	int init_flag;
	int reboot_flag;
	int	thread_stop_flag;
	/* for drain request! */
	struct aml_nftl_blk *nftl_blk;
	struct list_head list;
	/* operations */
	uint (*read_data)(struct aml_nftl_dev *nftl_dev,
		unsigned long block,
		unsigned nblk,
		unsigned char *buf);
	uint (*write_data)(struct aml_nftl_dev *nftl_dev,
		unsigned long block,
		unsigned nblk,
		unsigned char *buf);
	uint (*discard_data)(struct aml_nftl_dev *nftl_dev,
		unsigned long block,
		unsigned nblk);
	uint (*flush_write_cache)(struct aml_nftl_dev *nftl_dev);
	uint (*flush_discard_cache)(struct aml_nftl_dev *nftl_dev);
	uint (*invalid_read_cache)(struct aml_nftl_dev *nftl_dev);
	uint (*write_pair_page)(struct aml_nftl_dev *nftl_dev);
	int (*get_current_part_no)(struct aml_nftl_dev *nftl_dev);
	uint (*check_mapping)(struct aml_nftl_dev *nftl_dev,
		uint64_t offset,
		uint64_t size);
	uint (*discard_partition)(struct aml_nftl_dev *nftl_dev,
		uint64_t offset,
		uint64_t size);
	uint (*rebuild_tbls)(struct aml_nftl_dev *nftl_dev);
	uint (*compose_tbls)(struct aml_nftl_dev *nftl_dev);
};

struct aml_nftl_blk {
	struct ntd_blktrans_dev nbd;
	char name[24];
	uint64_t offset;
	uint64_t size;
	struct aml_nftl_dev *nftl_dev;
	struct request *req;
	struct request_queue *queue;
	struct scatterlist *bounce_sg;
	unsigned int bounce_sg_len;
	struct timespec ts_write_start;
	spinlock_t thread_lock;
	struct mutex *lock;

	uint (*read_data)(struct aml_nftl_blk *nftl_blk,
		unsigned long block,
		unsigned nblk,
		unsigned char *buf);
	uint (*write_data)(struct aml_nftl_blk *nftl_blk,
		unsigned long block,
		unsigned nblk,
		unsigned char *buf);
	uint (*discard_data)(struct aml_nftl_blk *nftl_blk,
		unsigned long block,
		unsigned nblk);
	uint (*flush_write_cache)(struct aml_nftl_blk *nftl_blk);
};

#pragma pack()
static inline struct aml_nftl_dev *dev_to_nftl_dev(struct device *dev)
{
	return dev ? dev_get_drvdata(dev) : NULL;
}

extern int test_flag;

/* extern struct mutex ntd_table_mutex; */
/* EXPORT_SYMBOL(ntd_table_mutex); */
extern int check_storage_device(void);
extern int check_nand_on_board(void);
extern int is_phydev_off_adjust(void);
extern int aml_nftl_initialize(struct aml_nftl_dev *nftl_dev, int no);
extern void aml_nftl_part_release(struct aml_nftl_part_t *part);
extern uint do_prio_gc(struct aml_nftl_part_t *part);
extern uint garbage_collect(struct aml_nftl_part_t *part);
extern uint do_static_wear_leveling(struct aml_nftl_part_t *part);
extern void *aml_nftl_malloc(uint size);
extern void aml_nftl_free(const void *ptr);
/* extern int aml_nftl_dbg(const char * fmt,args...); */
extern int aml_blktrans_initialize(struct aml_nftl_blk *nftl_blk,
	struct aml_nftl_dev *nftl_dev,
	uint64_t offset,
	uint64_t size);

extern int print_discard_page_map(struct aml_nftl_part_t *part);
extern int amlnf_class_register(struct class *cls);
extern void amlnf_ktime_get_ts(struct timespec *ts);
extern int aml_nftl_start(void *priv,
	void *cfg,
	struct aml_nftl_part_t **ppart,
	uint64_t size,
	unsigned erasesize,
	unsigned writesize,
	unsigned oobavail,
	char *name,
	int no,
	char type,
	int init_flag);
extern uint gc_all(struct aml_nftl_part_t *part);
extern uint gc_one(struct aml_nftl_part_t *part);
extern void print_nftl_part(struct aml_nftl_part_t *part);
extern int part_param_init(struct aml_nftl_part_t *part,
	ushort start_block,
	uint logic_sects,
	uint backup_cap_in_sects,
	int init_flag);
extern int aml_nftl_reinit(struct aml_nftl_dev *nftl_dev);
extern uint is_no_use_device(struct aml_nftl_part_t *part, uint size);
extern uint create_part_list_first(struct aml_nftl_part_t *part, uint size);
extern uint create_part_list(struct aml_nftl_part_t *part);
/* extern int nand_test(struct aml_nftl_dev *nftl_dev,
		unsigned char flag,
		uint blocks); */
extern int part_param_exit(struct aml_nftl_part_t *part);
extern int cache_init(struct aml_nftl_part_t *part);
extern int cache_exit(struct aml_nftl_part_t *part);
extern uint get_vaild_blocks(struct aml_nftl_part_t *part,
	uint start_block,
	uint blocks);
extern uint __nand_read(struct aml_nftl_part_t *part,
	uint start_sector,
	uint len,
	unsigned char *buf);
extern uint __nand_write(struct aml_nftl_part_t *part,
	uint start_sector,
	uint len,
	unsigned char *buf,
	int sync_flag);
extern uint __nand_discard(struct aml_nftl_part_t *part,
	uint start_sector,
	uint len,
	int sync_flag);
extern uint __nand_flush_write_cache(struct aml_nftl_part_t *part);
extern uint __nand_flush_discard_cache(struct aml_nftl_part_t *part);
extern uint __nand_invalid_read_cache(struct aml_nftl_part_t *part);
extern uint __nand_write_pair_page(struct aml_nftl_part_t *part);
extern int __get_current_part_no(struct aml_nftl_part_t *part);
extern uint __check_mapping(struct aml_nftl_part_t *part,
	uint64_t offset,
	uint64_t size);
extern uint __discard_partition(struct aml_nftl_part_t *part,
	uint64_t offset,
	uint64_t size);
extern void print_free_list(struct aml_nftl_part_t *part);
extern void print_block_invalid_list(struct aml_nftl_part_t *part);
extern int nand_discard_logic_page(struct aml_nftl_part_t *part,
	uint page_no);
extern  int get_adjust_block_num(void);
extern int aml_nftl_erase_part(struct aml_nftl_part_t *part);
extern int aml_nftl_set_status(struct aml_nftl_part_t *part,
	unsigned char status);
extern void print_block_count_list(struct aml_nftl_part_t *part);
extern void print_block_count_list_sorted(struct aml_nftl_part_t *part);
extern void dump_l2p(struct aml_nftl_part_t *part);
extern int nand_read_logic_page(struct aml_nftl_part_t *part,
	u32 page_no, u8 *buf);
extern int nand_read_page(struct aml_nftl_part_t *part,
	struct _physic_op_par *p);
extern int compose_part_list_info(struct aml_nftl_part_t *part);
extern int restore_part_list_info(struct aml_nftl_part_t *part);
extern int aml_platform_driver_register(struct platform_driver *driver);
#endif
