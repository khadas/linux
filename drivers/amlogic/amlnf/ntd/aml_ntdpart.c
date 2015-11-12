/*
 * drivers/amlogic/amlnf/ntd/aml_ntdpart.c
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


#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/kmod.h>
#include <linux/err.h>

#include "aml_ntd.h"


/* Our partition linked list */
static LIST_HEAD(ntd_partitions);
static DEFINE_MUTEX(ntd_partitions_mutex);

/*****************************************************************************
*Name         :
*Description  :   ntd methods which simply translate the effective address
*			and pass
*             :   through to the _real_ device.
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int read_page_with_oob(struct ntd_info *ntd,
	uint page,
	u_char *oob_buf,
	u_char *buf)
{
	int ret;
	/* unsigned int temp; */
	uint64_t byte_addr;
	struct amlnand_phydev *nand_dev = (struct amlnand_phydev *)ntd->priv;
	/* printk("1111read_page_with_oob : %x %d\n", page, p); */
	/* int *u = p; */
	/* int i; */
	/* for(i=0; i<4; i++){ */
	/* printk("*p[%d]=%d\n",i,*(u+i)); */
	/* } */
	byte_addr = page;
	byte_addr <<= ntd->pagesize_shift;

	nand_dev->ops.mode = NAND_HW_ECC;
	nand_dev->ops.addr = byte_addr;
	nand_dev->ops.len = ntd->pagesize;
	nand_dev->ops.retlen = 0;
	nand_dev->ops.ooblen = ntd->oobsize;
	nand_dev->ops.datbuf = buf;
	nand_dev->ops.oobbuf = oob_buf;

	ret = nand_dev->read(nand_dev);
	/* for( i=0; i<4; i++){ */
	/* printk("*p[%d]=%d\n",i,*(u+i)); */
	/* } */
	/* printk("2222read_page_with_oob : %x %d\n", page, p); */

	return ret;
}

/*****************************************************************************
*Name         :
*Description  :   ntd methods which simply translate the effective address
*             :   and pass through to the _real_ device.
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int read_only_oob(struct ntd_info *ntd, u_int32_t page, u_char *oob_buf)
{
	int ret;
	/* unsigned int temp; */
	uint64_t byte_addr;
	struct amlnand_phydev *nand_dev = (struct amlnand_phydev *)ntd->priv;

	byte_addr = page;
	byte_addr <<= ntd->pagesize_shift;

	nand_dev->ops.mode = NAND_HW_ECC;
	nand_dev->ops.addr = byte_addr;
	nand_dev->ops.len = ntd->pagesize;
	nand_dev->ops.retlen = 0;
	nand_dev->ops.ooblen = ntd->oobsize;
	nand_dev->ops.datbuf = NULL;
	nand_dev->ops.oobbuf = oob_buf;

	ret = nand_dev->read(nand_dev);

	return ret;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int write_page_with_oob(struct ntd_info *ntd,
	uint page,
	u_char *oob_buf,
	u_char *buf)
{
	int ret;
	/* unsigned int temp; */
	uint64_t byte_addr;
	struct amlnand_phydev *nand_dev = (struct amlnand_phydev *)ntd->priv;

	byte_addr = page;
	byte_addr <<= ntd->pagesize_shift;

	nand_dev->ops.mode = NAND_HW_ECC;
	nand_dev->ops.addr = byte_addr;
	nand_dev->ops.len = ntd->pagesize;
	nand_dev->ops.retlen = 0;
	nand_dev->ops.ooblen = ntd->oobsize;
	nand_dev->ops.datbuf = buf;
	nand_dev->ops.oobbuf = oob_buf;

	ret = nand_dev->write(nand_dev);

	return ret;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int part_erase(struct ntd_info *ntd, uint block)
{
	uint64_t byte_addr;
	struct amlnand_phydev *nand_dev = (struct amlnand_phydev *)ntd->priv;

	byte_addr = block;
	byte_addr <<= ntd->blocksize_shift;

	nand_dev->ops.mode = NAND_HW_ECC;
	nand_dev->ops.addr = byte_addr;
	nand_dev->ops.len = ntd->blocksize;
	nand_dev->ops.retlen = 0;
	nand_dev->ops.ooblen = 0;
	nand_dev->ops.datbuf = NULL;
	nand_dev->ops.oobbuf = NULL;

	return nand_dev->erase(nand_dev);
}


void ntd_erase_callback(struct ntd_info *ntd, uint from)
{

}
EXPORT_SYMBOL_GPL(ntd_erase_callback);


/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int part_block_isbad(struct ntd_info *ntd, uint block)
{
	int ret;
	uint64_t byte_addr;
	struct amlnand_phydev *nand_dev = (struct amlnand_phydev *)ntd->priv;

	byte_addr = block;
	byte_addr <<= ntd->blocksize_shift;

	nand_dev->ops.mode = NAND_HW_ECC;
	nand_dev->ops.addr = byte_addr;
	nand_dev->ops.len = ntd->blocksize;
	nand_dev->ops.retlen = 0;
	nand_dev->ops.ooblen = 0;
	nand_dev->ops.datbuf = NULL;
	nand_dev->ops.oobbuf = NULL;

	ret = nand_dev->block_isbad(nand_dev);
	return ret;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int part_block_markbad(struct ntd_info *ntd, uint block)
{
	int ret;
	uint64_t byte_addr;
	struct amlnand_phydev *nand_dev = (struct amlnand_phydev *)ntd->priv;

	byte_addr = block;
	byte_addr <<= ntd->blocksize_shift;

	nand_dev->ops.mode = NAND_HW_ECC;
	nand_dev->ops.addr = byte_addr;
	nand_dev->ops.len = ntd->blocksize;
	nand_dev->ops.retlen = 0;
	nand_dev->ops.ooblen = 0;
	nand_dev->ops.datbuf = NULL;
	nand_dev->ops.oobbuf = NULL;

	ret = nand_dev->block_markbad(nand_dev);

	return ret;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int part_suspend(struct ntd_info *ntd)
{
	struct amlnand_phydev *phy;
	int ret = 0;
	phy = ntd->priv;


	if (phy == NULL) {
		aml_nand_msg("%s : get phy dev failed", __func__);
		return 0;
	}
	ret = phy->suspend(phy);
	if (ret)
		aml_nand_msg("part_suspend %s  failed!!!", phy->name);

	aml_nand_msg("part_suspend %s", phy->name);

	return ret;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static void part_resume(struct ntd_info *ntd)
{
	struct amlnand_phydev *phy;
	/* int ret = 0; */
	phy = ntd->priv;
	if (phy == NULL) {
		aml_nand_msg("%s : get phy dev failed", __func__);
		return;
	}
	 phy->resume(phy);

	aml_nand_msg("part_resume %s", phy->name);

	return;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int part_flush(struct ntd_info *ntd)
{
	aml_nand_msg("part_flush");
	return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static inline void free_partition(struct ntd_info *p)
{
	kfree(p->name);
	kfree(p);
}

/*****************************************************************************
*Name         :
*Description  :This function unregisters and destroy all slave ntd objects
*             :which are attached to the given master ntd object.
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int del_ntd_partitions(struct amlnand_phydev *master)
{
	struct ntd_info *slave, *next;
	int ret, err = 0;

	mutex_lock(&ntd_partitions_mutex);

	list_for_each_entry_safe(slave, next, &ntd_partitions, list)
		if ((slave->name != NULL) && (slave->priv == (void *)master)) {
			ret = del_ntd_device(slave);
			if (ret < 0) {
				err = ret;
				continue;
			}
			list_del(&slave->list);
			free_partition(slave);
		}

	mutex_unlock(&ntd_partitions_mutex);

	return err;
}
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
void print_ntd(struct ntd_info *ntd)
{
	aml_nand_msg("ntd->name %s", ntd->name);
	aml_nand_msg("ntd->offset %llx", ntd->offset);
	aml_nand_msg("ntd->flags %lx", ntd->flags);
	aml_nand_msg("ntd->size %llx", ntd->size);
	aml_nand_msg("ntd->blocksize %ld", ntd->blocksize);
	aml_nand_msg("ntd->pagesize %ld", ntd->pagesize);
	aml_nand_msg("ntd->oobsize %ld", ntd->oobsize);
	aml_nand_msg("ntd->blocksize_shift %ld", ntd->blocksize_shift);
	aml_nand_msg("ntd->pagesize_shift %ld", ntd->pagesize_shift);
	aml_nand_msg("ntd->blocksize_mask %ld", ntd->blocksize_mask);
	aml_nand_msg("ntd->pagesize_mask %ld", ntd->pagesize_mask);
	aml_nand_msg("ntd->index %ld", ntd->index);

}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static struct ntd_info *allocate_partition(struct amlnand_phydev *master)
{
	struct ntd_info *slave;
	char *name;

	uint block = 0;

	uint block_num = 0;
	/* allocate the partition structure */
	slave = kzalloc(sizeof(*slave), GFP_KERNEL);
	/* name = kstrdup(part->name, GFP_KERNEL); */
	name = kstrdup(master->name, GFP_KERNEL);
	if (!name || !slave) {
		aml_nand_msg("\"%s\":mem alloc err while creating partitions",
			master->name);
		kfree(name);
		kfree(slave);
		return ERR_PTR(-ENOMEM);
	}

	/* set up the NTD object for this partition */
	slave->flags = master->flags;
	slave->blocksize = master->erasesize;
	slave->pagesize = master->writesize;
	slave->oobsize = master->oobavail;
	slave->name = name;
	slave->blocksize_shift = ffs(slave->blocksize) - 1;
	slave->pagesize_shift = ffs(slave->pagesize) - 1;

	slave->parts = (struct ntd_partition *)master->partitions;
	slave->nr_partitions = master->nr_partitions;

	/* slave->owner = master->owner; */
	/* slave->dev.parent = master->dev.parent; */
	/* printk("-------------------------------slave->dev.parentr  %d\n",
		slave->dev.parent); */

	slave->read_page_with_oob = read_page_with_oob;
	slave->write_page_with_oob = write_page_with_oob;
	slave->read_only_oob = read_only_oob;

	slave->block_isbad = part_block_isbad;
	slave->block_markbad = part_block_markbad;

	slave->suspend = part_suspend;
	slave->resume = part_resume;
	slave->flush = part_flush;
	slave->get_device = NULL;
	slave->put_device = NULL;

	slave->erase = part_erase;
	slave->priv = master;

	slave->offset = master->offset;
	slave->size = master->size;

	if ((slave->flags & NTD_WRITEABLE) &&
		ntd_mod_by_eb(slave->offset, slave)) {
		/* Doesn't start on a boundary of major erase size */
		/* FIXME: Let it be writable if it is on a boundary of
		 * _minor_ erase size though */
		slave->flags &= ~NTD_WRITEABLE;
		aml_nand_msg("ntd:part \"%s\" doesn't start on blk boundary",
			slave->name);
		aml_nand_msg("force read-only");
	}

	if ((slave->flags & NTD_WRITEABLE)
		&& ntd_mod_by_eb(slave->size, slave)) {
		slave->flags &= ~NTD_WRITEABLE;
		aml_nand_msg("ntd:part \"%s\" doesn't end on block",
			slave->name);
		aml_nand_msg("force read-only");
	}

	/* print_ntd(slave); */

	block_num = (uint)(slave->size >> slave->blocksize_shift);
	do {
		if (slave->block_isbad(slave, block))
			slave->badblocks++;
		block++;
		block_num--;
	} while (block_num != 0);

	/* out_register: */
	return slave;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int add_ntd_partitions(struct amlnand_phydev *master)
{
	struct ntd_info *slave;
	/* uint64_t cur_offset = 0; */
	/* int i; */

	/*
	if(master->size > 0xc0000000)
		master->size = 0xc0000000;
	*/

	slave = allocate_partition(master);
	if (IS_ERR(slave))
		return PTR_ERR(slave);

	mutex_lock(&ntd_partitions_mutex);
	slave->thread_stop_flag = 0;
	list_add(&slave->list, &ntd_partitions);
	mutex_unlock(&ntd_partitions_mutex);

	add_ntd_device(slave);

	/* add_ntd_device(slave); */

	/* for (i = 0; i < nbparts; i++) { */
	/* slave = allocate_partition(master, parts + i, i,cur_offset); */
	/* if (IS_ERR(slave)){ */
	/* return PTR_ERR(slave); */
	/* } */
	/*  */
	/* mutex_lock(&ntd_partitions_mutex); */
	/* list_add(&slave->list, &ntd_partitions); */
	/* mutex_unlock(&ntd_partitions_mutex); */
	/* add_ntd_device(slave); */
	/* cur_offset += parts->size; */
	/* } */

	return 0;
}
