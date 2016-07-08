/*
 * drivers/amlogic/amlnf/dev/amlnf_ctrl.c
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

#include "../include/phynand.h"

DEFINE_MUTEX(spi_nand_mutex);

struct bch_desc bch_list_m8[MAX_ECC_MODE_NUM] = {
	[0] = ECC_INFORMATION("NAND_RAW_MODE",
		NAND_ECC_SOFT_MODE,
		0,
		0,
		0,
		0),
	[1] = ECC_INFORMATION("NAND_BCH8_MODE",
		NAND_ECC_BCH8_MODE,
		NAND_ECC_UNIT_SIZE,
		NAND_BCH8_ECC_SIZE,
		2,
		1),
	[2] = ECC_INFORMATION("NAND_BCH8_1K_MODE" ,
		NAND_ECC_BCH8_1K_MODE,
		NAND_ECC_UNIT_1KSIZE,
		NAND_BCH8_1K_ECC_SIZE,
		2,
		2),
	[3] = ECC_INFORMATION("NAND_BCH24_1K_MODE" ,
		NAND_ECC_BCH24_1K_MODE,
		NAND_ECC_UNIT_1KSIZE,
		NAND_BCH24_1K_ECC_SIZE,
		2,
		3),
	[4] = ECC_INFORMATION("NAND_BCH30_1K_MODE" ,
		NAND_ECC_BCH30_1K_MODE,
		NAND_ECC_UNIT_1KSIZE,
		NAND_BCH30_1K_ECC_SIZE,
		2,
		4),
	[5] = ECC_INFORMATION("NAND_BCH40_1K_MODE" ,
		NAND_ECC_BCH40_1K_MODE,
		NAND_ECC_UNIT_1KSIZE,
		NAND_BCH40_1K_ECC_SIZE,
		2,
		5),
	[6] = ECC_INFORMATION("NAND_BCH50_1K_MODE" ,
		NAND_ECC_BCH50_1K_MODE,
		NAND_ECC_UNIT_1KSIZE,
		NAND_BCH50_1K_ECC_SIZE,
		2,
		6),
	[7] = ECC_INFORMATION("NAND_BCH60_1K_MODE" ,
		NAND_ECC_BCH60_1K_MODE,
		NAND_ECC_UNIT_1KSIZE,
		NAND_BCH60_1K_ECC_SIZE,
		2,
		7),
	[8] = ECC_INFORMATION("NAND_SHORT_MODE" ,
		NAND_ECC_SHORT_MODE,
		NAND_ECC_UNIT_SHORT,
		NAND_BCH60_1K_ECC_SIZE,
		2,
		8),
};

struct bch_desc bch_list[MAX_ECC_MODE_NUM] = {
	[0] = ECC_INFORMATION("NAND_RAW_MODE",
		NAND_ECC_SOFT_MODE,
		0,
		0,
		0,
		0),
	[1] = ECC_INFORMATION("NAND_BCH8_MODE",
		NAND_ECC_BCH8_MODE,
		NAND_ECC_UNIT_SIZE,
		NAND_BCH8_ECC_SIZE,
		2,
		1),
	[2] = ECC_INFORMATION("NAND_BCH8_1K_MODE" ,
		NAND_ECC_BCH8_1K_MODE,
		NAND_ECC_UNIT_1KSIZE,
		NAND_BCH8_1K_ECC_SIZE,
		2,
		2),
	[3] = ECC_INFORMATION("NAND_BCH16_1K_MODE" ,
		NAND_ECC_BCH16_1K_MODE,
		NAND_ECC_UNIT_1KSIZE,
		NAND_BCH16_1K_ECC_SIZE,
		2,
		3),
	[4] = ECC_INFORMATION("NAND_BCH24_1K_MODE" ,
		NAND_ECC_BCH24_1K_MODE,
		NAND_ECC_UNIT_1KSIZE,
		NAND_BCH24_1K_ECC_SIZE,
		2,
		4),
	[5] = ECC_INFORMATION("NAND_BCH30_1K_MODE" ,
		NAND_ECC_BCH30_1K_MODE,
		NAND_ECC_UNIT_1KSIZE,
		NAND_BCH30_1K_ECC_SIZE,
		2,
		5),
	[6] = ECC_INFORMATION("NAND_BCH40_1K_MODE" ,
		NAND_ECC_BCH40_1K_MODE,
		NAND_ECC_UNIT_1KSIZE,
		NAND_BCH40_1K_ECC_SIZE,
		2,
		6),
	[7] = ECC_INFORMATION("NAND_BCH60_1K_MODE" ,
		NAND_ECC_BCH60_1K_MODE,
		NAND_ECC_UNIT_1KSIZE,
		NAND_BCH60_1K_ECC_SIZE,
		2,
		7),
	[8] = ECC_INFORMATION("NAND_SHORT_MODE" ,
		NAND_ECC_SHORT_MODE,
		NAND_ECC_UNIT_SHORT,
		NAND_BCH60_1K_ECC_SIZE,
		2,
		8),
};

static dma_addr_t nfdata_dma_addr;
static dma_addr_t nfinfo_dma_addr;
spinlock_t amlnf_lock;
wait_queue_head_t amlnf_wq;

struct list_head nphy_dev_list;
struct list_head nf_dev_list;

void *aml_nand_malloc(uint size)
{
	return kmalloc(size, GFP_KERNEL);
}

void aml_nand_free(const void *ptr)
{
	kfree(ptr);
}

void amlnf_dma_free(const void *ptr, unsigned size, unsigned char flag)
{
	if (flag == 0) /* data */
		dma_free_coherent(NULL, size, (void *)ptr, nfdata_dma_addr);
	if (flag == 1) /* usr */
		dma_free_coherent(NULL, size, (void *)ptr, nfinfo_dma_addr);
}


int amlphy_prepare(unsigned int flag)
{
	spin_lock_init(&amlnf_lock);
	init_waitqueue_head(&amlnf_wq);

	return 0;
}

/*
  For fastboot, yyh.
 */
int aml_sys_info_reinit(struct amlnand_chip *aml_chip)
{
	int ret = 0;

#ifdef CONFIG_AML_NAND_KEY
	ret = aml_key_reinit(aml_chip);
	if (ret < 0)
		aml_nand_msg("nand key init failed");
#endif

#ifdef CONFIG_SECURE_NAND
	ret = aml_secure_init(aml_chip);
	if (ret < 0)
		aml_nand_msg("nand secure reinit failed");
#endif

/* fixme, yyh, add dtbs */
#if (AML_CFG_DTB_RSV_EN)
	ret = amlnf_dtb_reinit(aml_chip);
	if (ret < 0)
		aml_nand_msg("amlnf dtb reinit failed");
#endif
	if (boot_device_flag == 1) {
		ret = aml_ubootenv_reinit(aml_chip);
		if (ret < 0)
			aml_nand_msg("nand uboot env reinit failed");
	}
	aml_nand_msg("%s() done!\n", __func__);
	return ret;
}

int phydev_suspend(struct amlnand_phydev *phydev)
{
	struct amlnand_chip *aml_chip = (struct amlnand_chip *)phydev->priv;
#ifdef AML_NAND_RB_IRQ
	unsigned long flags;
#endif

	if (!strncmp((char *)phydev->name,
			NAND_BOOT_NAME,
			strlen((const char *)NAND_BOOT_NAME)))
		return 0;
	aml_nand_dbg("phydev_suspend: entered!");
#ifdef AML_NAND_RB_IRQ
	spin_lock_irqsave(&amlnf_lock, flags);
#else
	spin_lock(&amlnf_lock);
#endif
	/* set_chip_state(phydev, CHIP_PM_SUSPENDED); */
	set_chip_state(aml_chip, CHIP_PM_SUSPENDED);
#ifdef AML_NAND_RB_IRQ
	spin_unlock_irqrestore(&amlnf_lock, flags);
#else
	spin_unlock(&amlnf_lock);
#endif
	return 0;

}

void phydev_resume(struct amlnand_phydev *phydev)
{
	struct amlnand_chip *aml_chip;
	aml_chip = (struct amlnand_chip *)phydev->priv;

	amlchip_resume(phydev);
	/* reinit sysinfo...  only resume once!*/
	if (!strncmp((char *)phydev->name,
			NAND_CODE_NAME,
			strlen((const char *)NAND_CODE_NAME)))
		aml_sys_info_reinit(aml_chip);
	return;
}

DEFINE_MUTEX(nand_controller_mutex);

int nand_idleflag = 0;
#define	NAND_CTRL_NONE_RB	(1<<1)
void nand_get_chip(void *chip)
{
	struct amlnand_chip *aml_chip = (struct amlnand_chip *)chip;
	struct hw_controller *controller = &(aml_chip->controller);
	int retry = 0, ret = 0;

	while (1) {
		/* lock controller*/
		mutex_lock(&nand_controller_mutex);
		nand_idleflag = 1;
		if ((controller->option & NAND_CTRL_NONE_RB) == 0) {
			ret = pinctrl_select_state(aml_chip->nand_pinctrl ,
				aml_chip->nand_rbstate);
		} else {
		#if (!AML_CFG_PINMUX_ONCE_FOR_ALL)
			ret = pinctrl_select_state(aml_chip->nand_pinctrl ,
				aml_chip->nand_norbstate);
		#endif
		}
		if (ret < 0)
			aml_nand_msg("%s:%d %s can't get pinctrl",
				__func__,
				__LINE__,
				dev_name(aml_chip->device));
		else
			break;

		if (retry++ > 10) {
			aml_nand_msg("get pin fail over 10 times retry=%d",
				retry);
			/* fixme, yyh*/
			break;
		}
	}
	return;
}

void nand_release_chip(void *chip)
{
	struct amlnand_chip *aml_chip = (struct amlnand_chip *)chip;
	struct hw_controller *controller = &(aml_chip->controller);
	int ret = 0;

	if (nand_idleflag) {
		/* enter standby state. */
		controller->enter_standby(controller);
	#if (!AML_CFG_PINMUX_ONCE_FOR_ALL)
		ret = pinctrl_select_state(aml_chip->nand_pinctrl,
			aml_chip->nand_idlestate);
	#endif
		if (ret < 0)
			aml_nand_msg("select idle state error");
		nand_idleflag = 0;
		/* release controller*/
		mutex_unlock(&nand_controller_mutex);
	}
}

/**
 * amlnand_get_device - [GENERIC] Get chip for selected access
 *
 * Get the device and lock it for exclusive access
 */
int amlnand_get_device(struct amlnand_chip *aml_chip,
	enum chip_state_t new_state)
{
#ifdef AML_NAND_RB_IRQ
	unsigned long flags;
#endif
	DECLARE_WAITQUEUE(wait, current);
retry:
#ifdef AML_NAND_RB_IRQ
	spin_lock_irqsave(&amlnf_lock, flags);
#else
	spin_lock(&amlnf_lock);
#endif
	if (get_chip_state(aml_chip) == CHIP_READY) {
		set_chip_state(aml_chip, new_state);
#ifdef AML_NAND_RB_IRQ
		spin_unlock_irqrestore(&amlnf_lock, flags);
#else
		spin_unlock(&amlnf_lock);
#endif
		/* set nand pinmux here */
		nand_get_chip(aml_chip);
		return 0;
	}
	set_current_state(TASK_UNINTERRUPTIBLE);
	add_wait_queue(&amlnf_wq, &wait);
#ifdef AML_NAND_IRQ_MODE
	spin_unlock_irqrestore(&amlnf_lock, flags);
#else
	spin_unlock(&amlnf_lock);
#endif
	schedule();
	remove_wait_queue(&amlnf_wq, &wait);

	goto retry;
}

/**
 * nand_release_device - [GENERIC] release chip
 * @aml_chip:	nand chip structure
 *
 * Deselect, release chip lock and wake up anyone waiting on the device
 */
void amlnand_release_device(struct amlnand_chip *aml_chip)
{
#ifdef AML_NAND_RB_IRQ
	unsigned long flags;
#endif
	/* Release the controller and the chip */
#ifdef AML_NAND_RB_IRQ
	spin_lock_irqsave(&amlnf_lock, flags);
#else
	spin_lock(&amlnf_lock);
#endif
	set_chip_state(aml_chip, CHIP_READY);
	wake_up(&amlnf_wq);
#ifdef AML_NAND_RB_IRQ
	spin_unlock_irqrestore(&amlnf_lock, flags);
#else
	spin_unlock(&amlnf_lock);
#endif
	/* clear nand pinmux here */
	nand_release_chip(aml_chip);
}

#if 0
void dump_pinmux_regs(struct hw_controller *controller)
{
	aml_nand_msg("-------------------------------------");
	aml_nand_msg("pinmux-0: 0x%x",
		amlnf_read_reg32(controller->pinmux_base));
	aml_nand_msg("pinmux-4: 0x%x",
		amlnf_read_reg32((controller->pinmux_base + 16)));
	aml_nand_msg("pinmux-5: 0x%x",
		amlnf_read_reg32((controller->pinmux_base + 20)));
	aml_nand_msg("clock: 0x%x",
		amlnf_read_reg32(controller->nand_clk_reg));
	aml_nand_msg("-------------------------------------");
}
#endif

void set_nand_core_clk(struct hw_controller *controller, int clk_freq)
{
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB) {
		/* basic test code for gxb using 24Mhz, fixme. */
		#if 0 /*fixme, dbg code*/
		amlnf_write_reg32(controller->nand_clk_reg, 0x81000201);
		return ; /* fixme, dbg code, using 24Mhz clock... */
		#endif
		if (clk_freq == 200) {
			/* 1000Mhz/5 = 200Mhz */
			amlnf_write_reg32(controller->nand_clk_reg, 0x81000245);
		} else if (clk_freq == 250) {
			/* 1000Mhz/4 = 250Mhz */
			amlnf_write_reg32(controller->nand_clk_reg, 0x81000244);
		} else {
			/* 1000Mhz/5 = 200Mhz */
			amlnf_write_reg32(controller->nand_clk_reg, 0x81000245);
			aml_nand_msg("%s() %d, use default setting!",
				__func__, __LINE__);
		}
		return;
	} else
		aml_nand_msg("cpu type can not support!\n");

	return;
}

void get_sys_clk_rate(struct hw_controller *controller, int *rate)
{
	u32 cpu_type;

	cpu_type = get_cpu_type();
	if (cpu_type >= MESON_CPU_MAJOR_ID_M8) {

		set_nand_core_clk(controller, *rate);
	}
}

void nand_boot_info_prepare(struct amlnand_phydev *phydev,
	unsigned char *page0_buf)
{
	struct amlnand_chip *aml_chip = (struct amlnand_chip *)phydev->priv;
	struct nand_flash *flash = &(aml_chip->flash);
	struct phydev_ops *devops = &(phydev->ops);
	struct hw_controller *controller = &(aml_chip->controller);
	struct en_slc_info *slc_info = NULL;
	int i;
	unsigned int en_slc, configure_data, pages_per_blk;
	int chip_num = 1, nand_read_info, new_nand_type;
	unsigned int boot_num = 1, each_boot_pages;
	unsigned int valid_pages = BOOT_COPY_NUM * BOOT_PAGES_PER_COPY;

	struct nand_page0_cfg_t *info_cfg = NULL;
	struct nand_page0_info_t *info = NULL;

	slc_info = &(controller->slc_info);
	i = 0;
	info_cfg = (struct nand_page0_cfg_t *)page0_buf;
	/*
	info = (struct nand_page0_info_t *)((page0_buf + 384) -
				sizeof(struct nand_page0_info_t));
	*/
	info = (struct nand_page0_info_t *)&info_cfg->nand_page0_info;
	pages_per_blk = flash->blocksize / flash->pagesize;
	new_nand_type = aml_chip->flash.new_type;
	/* en_slc = (( flash->new_type < 10)&&( flash->new_type))? 1:0; */
	configure_data = NFC_CMD_N2M(controller->ran_mode,
			controller->bch_mode, 0, (controller->ecc_unit >> 3),
			controller->ecc_steps);

	if ((flash->new_type < 10) && (flash->new_type))
		en_slc = 1;
	else if (flash->new_type == SANDISK_19NM)
		en_slc = 2;
	else
		en_slc = 0;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8) {

		memset(page0_buf, 0x0, flash->pagesize);
		info_cfg->ext =
			(configure_data|(1<<23) | (1<<22) | (2<<20) | (1<<19));
		/* need finish here for romboot retry */
		info_cfg->id = 0;
		info_cfg->max = 0;
		memset((u8 *)(&info_cfg->list[0]),
			0,
			NAND_PAGELIST_CNT);
		if (en_slc) {
			info_cfg->ext |= (1<<26);
			if (en_slc == 1) {
				memcpy((u8 *)(&info_cfg->list[0]),
					(u8 *)(&slc_info->pagelist[1]),
					NAND_PAGELIST_CNT);
			} else if (en_slc == 2) {
				info_cfg->ext |= (1<<24);
				for (i = 1; i < NAND_PAGELIST_CNT; i++)
					info_cfg->list[i-1] = i<<1;
			}
		}
		chip_num = controller->chip_num;
		/*
		aml_nand_msg("chip_num %d controller->chip_num %d", \
		chip_num, controller->chip_num);
		*/
		/* chip_num occupy the lowest 2 bit */
		nand_read_info = chip_num;

		/*
		make it
		1)calu the number of boot saved and pages each boot needs.
		2)the number is 2*n but less than 4.
		*/
		aml_nand_msg("valid_pages = %d en_slc = %d devops->len = %llx",
			valid_pages,
			en_slc, devops->len);
		valid_pages = (en_slc)?(valid_pages>>1):valid_pages;
		for (i = 1;
			i < ((valid_pages*flash->pagesize)/devops->len + 1);
			i++) {
			if (((valid_pages*flash->pagesize)/(2*i)
					>= devops->len) && (boot_num < 4))
				boot_num <<= 1;
			else
				break;
		}
		each_boot_pages = valid_pages/boot_num;
		/*
		each_boot_pages =
			(en_slc)?(each_boot_pages<<1):each_boot_pages;
		*/

		info->ce_mask = aml_chip->ce_bit_mask;
		info->nand_read_info = nand_read_info;
		info->pages_in_block = pages_per_blk;
		info->new_nand_type = new_nand_type;
		info->boot_num = boot_num;
		info->each_boot_pages = each_boot_pages;

		aml_nand_msg("new_type = 0x%x\n", info->new_nand_type);
		aml_nand_msg("page_per_blk = 0x%x\n", info->pages_in_block);
		aml_nand_msg("boot_num = %d each_boot_pages = %d", boot_num,
			each_boot_pages);

	} else {
		memset(page0_buf, 0xbb, flash->pagesize);
		memcpy(page0_buf, (u8 *)(&configure_data), sizeof(int));
		memcpy(page0_buf + sizeof(int),
			(u8 *)(&pages_per_blk),
			sizeof(int));
		new_nand_type = aml_chip->flash.new_type;
		memcpy(page0_buf + 2 * sizeof(int),
			(u8 *)(&new_nand_type),
			sizeof(int));

		chip_num = controller->chip_num;
		nand_read_info = chip_num;/* chip_num occupy the lowest 2 bit */
		memcpy(page0_buf + 3 * sizeof(int),
			(u8 *)(&nand_read_info),
			sizeof(int));
	}
}

void uboot_set_ran_mode(struct amlnand_phydev *phydev)
{
	struct amlnand_chip *aml_chip = (struct amlnand_chip *)phydev->priv;
	struct hw_controller *controller = &(aml_chip->controller);

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
		controller->ran_mode = 1;
	else
		controller->ran_mode = 0;
}

int aml_sys_info_init(struct amlnand_chip *aml_chip)
{
#ifdef CONFIG_AML_NAND_KEY
	struct nand_arg_info *nand_key = &aml_chip->nand_key;
#endif
#ifdef CONFIG_SECURE_NAND
	struct nand_arg_info *nand_secure = &aml_chip->nand_secure;
#endif
#if (AML_CFG_DTB_RSV_EN)
	struct nand_arg_info *amlnf_dtb = &aml_chip->amlnf_dtb;
#endif
	struct nand_arg_info *uboot_env =  &aml_chip->uboot_env;
	unsigned char *buf = NULL;
	unsigned int buf_size = 0;
	int ret = 0;

	buf_size = max_t(u32, CONFIG_SECURE_SIZE, CONFIG_KEY_MAX_SIZE);
	buf = aml_nand_malloc(buf_size);
	if (!buf)
		aml_nand_msg("aml_sys_info_init : malloc failed");

	memset(buf, 0x0, buf_size);

#ifdef CONFIG_AML_NAND_KEY
	if (nand_key->arg_valid == 0) {
		ret = aml_key_init(aml_chip);
		if (ret < 0) {
			aml_nand_msg("nand key init failed");
			goto exit_error;
		}
	}
#endif

#ifdef CONFIG_SECURE_NAND
	if (nand_secure->arg_valid == 0) {
		ret = aml_secure_init(aml_chip);
		if (ret < 0) {
			aml_nand_msg("nand secure init failed");
			goto exit_error;
		}
	}
#endif
#if (AML_CFG_DTB_RSV_EN)
	if (amlnf_dtb->arg_valid == 0) {
		ret = amlnf_dtb_init(aml_chip);
		if (ret < 0) {
			aml_nand_msg("amlnf dtb init failed");
			/* fixme, should go on! */
			/* goto exit_error; */
		}
	}
#endif
	if ((uboot_env->arg_valid == 0) && (boot_device_flag == 1)) {
		ret = aml_ubootenv_init(aml_chip);
		if (ret < 0) {
			aml_nand_msg("nand uboot env init failed");
			goto exit_error;
		}
	}

#ifdef CONFIG_AML_NAND_KEY
	if (nand_key->arg_valid == 0) {
		ret = amlnand_save_info_by_name(aml_chip,
			(unsigned char *)(&(aml_chip->nand_key)),
			buf,
			KEY_INFO_HEAD_MAGIC,
			aml_chip->keysize);
		if (ret < 0) {
			aml_nand_msg("nand save default key failed");
			goto exit_error;
		}
	}
#endif

#ifdef CONFIG_SECURE_NAND
	if (nand_secure->arg_valid == 0) {
		ret = amlnand_save_info_by_name(aml_chip,
			(unsigned char *)&(aml_chip->nand_secure),
			buf,
			SECURE_INFO_HEAD_MAGIC,
			CONFIG_SECURE_SIZE);
		if (ret < 0) {
			aml_nand_msg("nand save default secure_ptr failed");
			goto exit_error;
		}
	}
#endif
#if (AML_CFG_DTB_RSV_EN)
	/* fixme, no need to save a empty dtb. */
#endif
exit_error:
	kfree(buf);
	buf = NULL;
	return ret;
}

int aml_sys_info_error_handle(struct amlnand_chip *aml_chip)
{

#ifdef CONFIG_AML_NAND_KEY
	 if ((aml_chip->nand_key.arg_valid == 1) &&
		(aml_chip->nand_key.update_flag)) {
		aml_nand_update_key(aml_chip, NULL);
		aml_chip->nand_key.update_flag = 0;
		aml_nand_msg("NAND UPDATE CKECK  : ");
		aml_nand_msg("arg %s:arg_valid=%d,blk_addr=%d,page_addr=%d",
			"nandkey",
			aml_chip->nand_key.arg_valid,
			aml_chip->nand_key.valid_blk_addr,
			aml_chip->nand_key.valid_page_addr);
	}
#endif

#ifdef CONFIG_SECURE_NAND
	if ((aml_chip->nand_secure.arg_valid == 1)
		&& (aml_chip->nand_secure.update_flag)) {
		aml_nand_update_secure(aml_chip, NULL);
		aml_chip->nand_secure.update_flag = 0;
		aml_nand_msg("NAND UPDATE CKECK  : ");
		aml_nand_msg("arg%s:arg_valid=%d,blk_addr=%d,page_addr=%d",
			"nandsecure",
			aml_chip->nand_secure.arg_valid,
			aml_chip->nand_secure.valid_blk_addr,
			aml_chip->nand_secure.valid_page_addr);
	}
#endif
	if ((aml_chip->uboot_env.arg_valid == 1)
		&& (aml_chip->uboot_env.update_flag)) {
		aml_nand_update_ubootenv(aml_chip, NULL);
		aml_chip->uboot_env.update_flag = 0;
		aml_nand_msg("NAND UPDATE CKECK  : ");
		aml_nand_msg("arg%s:arg_valid=%d,blk_addr=%d,page_addr=%d",
			"ubootenv",
			aml_chip->uboot_env.arg_valid,
			aml_chip->uboot_env.valid_blk_addr,
			aml_chip->uboot_env.valid_page_addr);
	}
	return 0;
}
