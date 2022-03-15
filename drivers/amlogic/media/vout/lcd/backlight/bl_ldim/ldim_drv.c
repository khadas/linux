// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/version.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/of_reserved_mem.h>
#include <linux/cma.h>
#include <linux/dma-contiguous.h>
#include <linux/dma-mapping.h>
#include <linux/sched/clock.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include <linux/amlogic/media/vout/lcd/ldim_fw.h>
#include "../../lcd_common.h"
#include "ldim_drv.h"
#include "ldim_reg.h"

#define AML_LDIM_DEV_NAME            "aml_ldim"

unsigned char ldim_debug_print;

struct ldim_dev_s {
	struct ldim_drv_data_s *data;

	struct vpu_dev_s *vpu_dev;
	struct cdev   cdev;
	struct device *dev;
	dev_t aml_ldim_devno;
	struct class *aml_ldim_clsp;
	struct cdev *aml_ldim_cdevp;
};

static struct ldim_dev_s ldim_dev;
static struct aml_ldim_driver_s ldim_driver;

static spinlock_t ldim_isr_lock;
static spinlock_t rdma_ldim_isr_lock;
static spinlock_t ldim_pwm_vs_isr_lock;

static struct workqueue_struct *ldim_queue;
static struct work_struct ldim_on_vs_work;
static struct work_struct ldim_off_vs_work;
static struct work_struct ldim_err_handle_work;

static int ldim_on_init(void);
static int ldim_power_on(void);
static int ldim_power_off(void);
static int ldim_set_level(unsigned int level);
static void ldim_test_ctrl(int flag);
static void ldim_ld_sel_ctrl(int flag);
static void ldim_pwm_vs_update(void);
static void ldim_config_print(void);

static struct aml_ldim_info_s ldim_info;
static struct aml_ldim_pq_s ldim_pq;
static struct aml_ldim_pq_s ldim_pq_pre;

static struct ldim_config_s ldim_config = {
	.hsize = 3840,
	.vsize = 2160,
	.seg_row = 1,
	.seg_col = 1,
	.bl_mode = 1,
	.func_en = 0,
	.remap_en = 1,
	.hvcnt_bypass = 0,
	.dev_index = 0xff,
};

static struct ldim_rmem_s ldim_rmem = {
	.flag = 0,

	.wr_mem_vaddr1 = NULL,
	.wr_mem_paddr1 = 0,
	.wr_mem_vaddr2 = NULL,
	.wr_mem_paddr2 = 0,
	.rd_mem_vaddr1 = NULL,
	.rd_mem_paddr1 = 0,
	.wr_mem_vaddr2 = NULL,
	.wr_mem_paddr2 = 0,
	.wr_mem_size = 0,
	.rd_mem_size = 0,

	/* for new ldc */
	.rsv_mem_paddr = 0,
	.rsv_mem_size = 0x100000,
	.global_hist_vaddr = NULL,
	.global_hist_paddr = 0,
	.global_hist_mem_size = 0,
	.global_hist_highmem_flag = 0,
	.seg_hist_vaddr = NULL,
	.seg_hist_paddr = 0,
	.seg_hist_mem_size = 0,
	.seg_hist_highmem_flag = 0,
	.duty_vaddr = NULL,
	.duty_paddr = 0,
	.duty_mem_size = 0,
	.duty_highmem_flag = 0,
};

static struct ldim_stts_s ldim_stts = {
	.max_rgb = NULL,
	.hist_matrix = NULL,
	.global_hist = NULL,
	.seg_hist = NULL,
};

static struct ldim_comp_s ldim_comp = {
	.ldc_comp_en = 1,
	.ldc_bl_buf_diff = 0,
	.ldc_glb_gain = 0x380,
	.ldc_dth_en = 1,
	.ldc_dth_bw = 1,
};

static struct aml_ldim_driver_s ldim_driver = {
	.valid_flag = 0, /* default invalid, active when bl_ctrl_method=ldim */
	.static_pic_flag = 0,
	.vsync_change_flag = 0,
	.duty_update_flag = 0,
	.switch_ld_cnt = 0,
	.pq_updating = 0,
	.in_vsync_flag = 0,

	.init_on_flag = 0,
	.func_en = 0,
	.remap_en = 0,
	.demo_en = 0,
	.black_frm_en = 0,
	.ld_sel = 1,
	.func_bypass = 0,
	.dev_smr_bypass = 0,
	.brightness_bypass = 0,
	.test_bl_en = 0,
	.test_remap_en = 0,
	.avg_update_en = 0,
	.matrix_update_en = 0,
	.alg_en = 0,
	.top_en = 0,
	.hist_en = 0,
	.load_db_en = 1,
	.db_print_flag = 0,
	.level_update = 0,

	.state = LDIM_STATE_LD_EN,
	.data_min = LD_DATA_MIN,
	.data_max = LD_DATA_MAX,
	.brightness_level = 0,
	.litgain = LD_DATA_MAX,
	.dbg_vs_cnt = 0,
	.irq_cnt = 0,
	.pwm_vs_irq_cnt = 0,
	.arithmetic_time = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	.xfer_time = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},

	.data = NULL,
	.conf = &ldim_config,
	.dev_drv = NULL,
	.rmem = &ldim_rmem,

	.stts = &ldim_stts,
	.fw = NULL,
	.comp = &ldim_comp,
	.fw_cus_pre = NULL,
	.fw_cus_post = NULL,

	.test_matrix = NULL,
	.local_bl_matrix = NULL,
	.bl_matrix_cur = NULL,
	.bl_matrix_pre = NULL,

	.init = ldim_on_init,
	.power_on = ldim_power_on,
	.power_off = ldim_power_off,
	.set_level = ldim_set_level,
	.test_ctrl = ldim_test_ctrl,
	.ld_sel_ctrl = ldim_ld_sel_ctrl,
	.pwm_vs_update = ldim_pwm_vs_update,
	.config_print = ldim_config_print,
};

struct aml_ldim_driver_s *aml_ldim_get_driver(void)
{
	return &ldim_driver;
}

void ldim_delay(int ms)
{
	if (ms > 0 && ms < 20)
		usleep_range(ms * 1000, ms * 1000 + 1);
	else if (ms > 20)
		msleep(ms);
}

void ldim_remap_ctrl(unsigned char status)
{
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);
	unsigned int temp;

	temp = ldim_driver.matrix_update_en;
	if (status) {
		ldim_driver.matrix_update_en = 0;
		if (bdrv->data->chip_type == LCD_CHIP_T5W ||
			bdrv->data->chip_type == LCD_CHIP_T3 ||
			bdrv->data->chip_type == LCD_CHIP_T7) {
			ldim_hw_remap_en_t7(&ldim_driver, 1);
		} else {
			ldim_hw_remap_en(&ldim_driver, 1);
		}
		msleep(20);
		if (bdrv->data->chip_type == LCD_CHIP_TM2)
			ldim_hw_vpu_dma_mif_en(LDIM_VPU_DMA_RD, 1);
		else if (bdrv->data->chip_type == LCD_CHIP_TM2B)
			ldim_hw_vpu_dma_mif_en_tm2b(LDIM_VPU_DMA_RD, 1);
		ldim_driver.matrix_update_en = temp;
		ldim_driver.remap_en = 1;
	} else {
		ldim_driver.remap_en = 0;
		ldim_driver.matrix_update_en = 0;
		if (bdrv->data->chip_type == LCD_CHIP_T5W ||
			bdrv->data->chip_type == LCD_CHIP_T3 ||
			bdrv->data->chip_type == LCD_CHIP_T7) {
			ldim_hw_remap_en_t7(&ldim_driver, 0);
		} else {
			ldim_hw_remap_en(&ldim_driver, 0);
		}
		msleep(20);
		if (bdrv->data->chip_type == LCD_CHIP_TM2)
			ldim_hw_vpu_dma_mif_en(LDIM_VPU_DMA_RD, 0);
		else if (bdrv->data->chip_type == LCD_CHIP_TM2B)
			ldim_hw_vpu_dma_mif_en_tm2b(LDIM_VPU_DMA_RD, 0);
		ldim_driver.matrix_update_en = temp;
	}
	LDIMPR("%s: %d\n", __func__, status);
}

static void ldim_func_ctrl(struct aml_ldim_driver_s *ldim_drv, int flag)
{
	if (!ldim_drv)
		return;

	if (flag) {
		if (ldim_driver.ld_sel == 0) {
			if (ldim_debug_print)
				LDIMPR("%s: exit for ld_sel=0\n", __func__);
			return;
		}

		/* enable other flag */
		ldim_driver.top_en = 1;
		ldim_driver.hist_en = 1;
		ldim_driver.alg_en = 1;
		/* enable update */
		ldim_driver.avg_update_en = 1;
		/*ldim_driver.matrix_update_en = 1;*/
		ldim_driver.func_en = 1;

		ldim_remap_ctrl(ldim_config.remap_en);

		ldim_driver.state |= LDIM_STATE_FUNC_EN;
	} else {
		ldim_driver.state &= ~LDIM_STATE_FUNC_EN;

		/* disable remap */
		ldim_remap_ctrl(0);

		ldim_driver.func_en = 0;
		/* disable update */
		ldim_driver.avg_update_en = 0;
		/*ldim_driver.matrix_update_en = 0;*/
		/* disable other flag */
		ldim_driver.top_en = 0;
		ldim_driver.hist_en = 0;
		ldim_driver.alg_en = 0;

		/* refresh system brightness */
		ldim_driver.level_update = 1;
	}
	LDIMPR("%s: %d\n", __func__, flag);
}

static void ldim_stts_initial(unsigned int pic_h, unsigned int pic_v,
		       unsigned int hist_vnum, unsigned int hist_hnum)
{
	LDIMPR("%s: %d %d %d %d\n",
	       __func__, pic_h, pic_v, hist_vnum, hist_hnum);

	ldim_driver.fw->seg_col = hist_hnum;
	ldim_driver.fw->seg_row = hist_vnum;

	if (!ldim_dev.data) {
		LDIMERR("%s: invalid data\n", __func__);
		return;
	}
	if (ldim_dev.vpu_dev)
		vpu_dev_mem_power_on(ldim_dev.vpu_dev);
	if (ldim_dev.data->stts_init)
		ldim_dev.data->stts_init(pic_h, pic_v, hist_vnum, hist_hnum);
}

static void ldim_initial(unsigned int pic_h, unsigned int pic_v,
		  unsigned int hist_vnum, unsigned int hist_hnum,
		  unsigned int blk_mode, unsigned int ldim_bl_en,
		  unsigned int hvcnt_bypass)
{
	if (!ldim_driver.fw->ctrl->nprm) {
		LDIMERR("%s: nprm is null\n", __func__);
		return;
	}
	LDIMPR("%s: %d %d %d %d %d %d %d\n",
	       __func__, pic_h, pic_v, hist_vnum, hist_hnum,
	       blk_mode, ldim_bl_en, hvcnt_bypass);

	ldim_driver.matrix_update_en = ldim_bl_en;
	ld_func_cfg_ldreg(ldim_driver.fw->ctrl->nprm);
	/* config params begin */
	/* configuration of the panel parameters */
	ldim_driver.fw->ctrl->nprm->reg_ld_pic_row_max = pic_v;
	ldim_driver.fw->ctrl->nprm->reg_ld_pic_col_max = pic_h;
	/* Maximum to BLKVMAX  , Maximum to BLKHMAX */
	ldim_driver.fw->ctrl->nprm->reg_ld_blk_vnum = hist_vnum;
	ldim_driver.fw->ctrl->nprm->reg_ld_blk_hnum = hist_hnum;
	ldim_driver.fw->ctrl->nprm->reg_ld_blk_mode = blk_mode;
	/*config params end */
	ld_func_fw_cfg_once(ldim_driver.fw->ctrl->nprm);

	if (!ldim_dev.data) {
		LDIMERR("%s: invalid data\n", __func__);
		return;
	}
	if (ldim_dev.data->remap_init) {
		ldim_dev.data->remap_init(ldim_driver.fw->ctrl->nprm,
					  ldim_bl_en, hvcnt_bypass);
	}
}

static void ldim_drv_init(struct aml_ldim_driver_s *ldim_drv)
{
	if (!ldim_drv)
		return;

	if (ldim_drv->fw && ldim_drv->dev_drv) {
		ldim_func_profile_update(ldim_drv->fw,
					 ldim_drv->dev_drv->bl_profile);
	}

	ldim_stts_initial(ldim_drv->conf->hsize, ldim_drv->conf->vsize,
			  ldim_drv->conf->seg_row, ldim_drv->conf->seg_col);
	ldim_initial(ldim_config.hsize, ldim_config.vsize,
		     ldim_config.seg_row, ldim_config.seg_col,
		     ldim_config.bl_mode,  ldim_config.func_en,
		      ldim_config.hvcnt_bypass);
}

static int ldim_on_init(void)
{
	int ret = 0;

	if (ldim_debug_print)
		LDIMPR("%s\n", __func__);

	if (ldim_driver.data->drv_init)
		ldim_driver.data->drv_init(&ldim_driver);

	ldim_driver.init_on_flag = 1;
	ldim_driver.level_update = 1;
	ldim_driver.state |= LDIM_STATE_POWER_ON;

	return ret;
}

static int ldim_power_on(void)
{
	int ret = 0;

	LDIMPR("%s\n", __func__);

	if (ldim_driver.data->drv_init)
		ldim_driver.data->drv_init(&ldim_driver);

	ldim_driver.init_on_flag = 1;

	if (ldim_driver.dev_drv && ldim_driver.dev_drv->power_on)
		ldim_driver.dev_drv->power_on(&ldim_driver);

	ldim_driver.level_update = 1;
	ldim_driver.state |= LDIM_STATE_POWER_ON;

	return ret;
}

static int ldim_power_off(void)
{
	int ret = 0;

	LDIMPR("%s\n", __func__);

	ldim_driver.state &= ~LDIM_STATE_POWER_ON;
	ldim_driver.init_on_flag = 0;
	if (ldim_driver.dev_drv && ldim_driver.dev_drv->power_off)
		ldim_driver.dev_drv->power_off(&ldim_driver);

	return ret;
}

static int ldim_set_level(unsigned int level)
{
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);
	unsigned int level_max, level_min;

	ldim_driver.brightness_level = level;

	if (ldim_driver.brightness_bypass)
		return 0;

	level_max = bdrv->bconf.level_max;
	level_min = bdrv->bconf.level_min;

	level = ((level - level_min) * (ldim_driver.data_max - ldim_driver.data_min)) /
		(level_max - level_min) + ldim_driver.data_min;
	level &= 0xfff;
	ldim_driver.litgain = (unsigned long)level;
	ldim_driver.level_update = 1;

	return 0;
}

static void ldim_test_ctrl(int flag)
{
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);

	if (bdrv->data->chip_type >= LCD_CHIP_T7)
		return;

	if (flag) /* when enable lcd bist pattern, bypass ldim function */
		ldim_driver.func_bypass = 1;
	else
		ldim_driver.func_bypass = 0;
	LDIMPR("%s: ldim_func_bypass = %d\n",
	       __func__, ldim_driver.func_bypass);
}

static void ldim_ld_sel_ctrl(int flag)
{
	LDIMPR("%s: ld_sel: %d\n", __func__, flag);
	if (flag) {
		ldim_driver.ld_sel = 1;
		ldim_driver.state |= LDIM_STATE_LD_EN;
	} else {
		ldim_driver.ld_sel = 0;
		ldim_driver.state &= ~LDIM_STATE_LD_EN;
	}
}

static void ldim_pwm_vs_update(void)
{
	if (ldim_driver.dev_drv && ldim_driver.dev_drv->pwm_vs_update)
		ldim_driver.dev_drv->pwm_vs_update(&ldim_driver);
}

static void ldim_config_print(void)
{
	if (ldim_driver.dev_drv && ldim_driver.dev_drv->config_print)
		ldim_driver.dev_drv->config_print(&ldim_driver);
}

/* ******************************************************
 * local dimming function
 * ******************************************************
 */
static void ldim_vs_arithmetic(struct aml_ldim_driver_s *ldim_drv)
{
	struct ld_reg_s *nprm = ldim_driver.fw->ctrl->nprm;
	unsigned int size;

	if (ldim_driver.top_en == 0)
		return;

	ldim_hw_stts_read_zone(ldim_driver.conf->seg_row,
			       ldim_driver.conf->seg_col);

	if (ldim_driver.alg_en == 0)
		return;

	if (!ldim_driver.fw->fw_alg_frm) {
		if (ldim_drv->dbg_vs_cnt == 0)
			LDIMERR("%s: ldim_fw ko is not installed\n", __func__);
		return;
	}

	ldim_driver.fw->fw_alg_frm(ldim_driver.fw, ldim_driver.stts);
	size = ldim_driver.conf->seg_row * ldim_driver.conf->seg_col;
	memcpy(ldim_driver.local_bl_matrix, nprm->bl_matrix,
	       size * (sizeof(unsigned int)));
}

static void ldim_vs_arithmetic_tm2(struct aml_ldim_driver_s *ldim_drv)
{
	struct ld_reg_s *nprm = ldim_driver.fw->ctrl->nprm;
	unsigned int *local_ldim_buf;
	unsigned int size, i;

	if (!ldim_driver.stts->hist_matrix)
		return;
	if (!ldim_driver.stts->max_rgb)
		return;

	if (ldim_driver.hist_en == 0)
		return;

	size = ldim_driver.conf->seg_row * ldim_driver.conf->seg_col;
	local_ldim_buf = kcalloc(size * 20, sizeof(unsigned int), GFP_KERNEL);
	if (!local_ldim_buf)
		return;

	/*stts_read*/
	memcpy(local_ldim_buf, ldim_rmem.wr_mem_vaddr1,
	       size * 20 * sizeof(unsigned int));
	for (i = 0; i < size; i++) {
		memcpy(&ldim_driver.stts->hist_matrix[i * 16],
		       &local_ldim_buf[i * 20],
		       16 * sizeof(unsigned int));
		memcpy(&ldim_driver.stts->max_rgb[i * 3],
		       &local_ldim_buf[(i * 20) + 16],
		       3 * sizeof(unsigned int));
	}

	if (ldim_driver.alg_en == 0) {
		kfree(local_ldim_buf);
		return;
	}

	if (!ldim_driver.fw->fw_alg_frm) {
		if (ldim_driver.dbg_vs_cnt == 0)
			LDIMERR("%s: ldim_fw ko is not installed\n", __func__);
		return;
	}

	ldim_driver.fw->fw_alg_frm(ldim_driver.fw, ldim_driver.stts);
	memcpy(ldim_driver.local_bl_matrix, nprm->bl_matrix,
	       size * (sizeof(unsigned int)));

	kfree(local_ldim_buf);
}

static void ldim_remap_update(void)
{
	if (!ldim_dev.data) {
		if (ldim_driver.dbg_vs_cnt == 0)
			LDIMERR("%s: invalid data\n", __func__);
		return;
	}
	if (!ldim_dev.data->remap_update)
		return;

	ldim_dev.data->remap_update(ldim_driver.fw->ctrl->nprm,
				    ldim_driver.avg_update_en,
				    ldim_driver.matrix_update_en);
}

static void ldim_time_sort_save(unsigned long long *table,
				unsigned long long data)
{
	int i, j;

	for (i = 9; i >= 0; i--) {
		if (data > table[i]) {
			for (j = 0; j < i; j++)
				table[j] = table[j + 1];
			table[i] = data;
			break;
		}
	}
}

static void ldim_on_vs_brightness(void);
static void ldim_off_vs_brightness(void);

atomic_t ldim_inirq_flag = ATOMIC_INIT(0);

int is_in_ldim_vsync_isr(void)
{
	if (atomic_read(&ldim_inirq_flag) > 0)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(is_in_ldim_vsync_isr);

static irqreturn_t ldim_vsync_isr(int irq, void *dev_id)
{
	unsigned long long local_time[3];
	unsigned long flags;

	if (ldim_driver.init_on_flag == 0 && ldim_driver.func_en == 0)
		return IRQ_HANDLED;
	atomic_set(&ldim_inirq_flag, 1);

	spin_lock_irqsave(&ldim_isr_lock, flags);

	ldim_driver.in_vsync_flag = 1;

	if (ldim_driver.dbg_vs_cnt++ >= 300) /* for debug print */
		ldim_driver.dbg_vs_cnt = 0;

	ldim_remap_update();

	if (ldim_driver.func_en && ldim_driver.ld_sel) {
		local_time[0] = sched_clock();
		if (ldim_dev.data->vs_arithmetic)
			ldim_dev.data->vs_arithmetic(&ldim_driver);
		local_time[1] = sched_clock();
		local_time[2] = local_time[1] - local_time[0];
		ldim_time_sort_save(ldim_driver.arithmetic_time, local_time[2]);
	}

	ldim_driver.irq_cnt++;
	if (ldim_driver.irq_cnt > 0xfffffff)
		ldim_driver.irq_cnt = 0;

	ldim_driver.in_vsync_flag = 0;

	spin_unlock_irqrestore(&ldim_isr_lock, flags);
	atomic_set(&ldim_inirq_flag, 0);

	return IRQ_HANDLED;
}

static irqreturn_t ldim_pwm_vs_isr(int irq, void *dev_id)
{
	unsigned long flags;

	if (ldim_driver.init_on_flag == 0)
		return IRQ_HANDLED;

	if (ldim_driver.pwm_vs_irq_cnt == ldim_driver.irq_cnt)
		return IRQ_HANDLED;

	ldim_driver.pwm_vs_irq_cnt = ldim_driver.irq_cnt;

	spin_lock_irqsave(&ldim_pwm_vs_isr_lock, flags);

	if (ldim_debug_print == 7)
		LDIMPR("%s: pwm_vs_irq_cnt = %d\n", __func__, ldim_driver.pwm_vs_irq_cnt);

	if (ldim_driver.func_en && ldim_driver.ld_sel) {
#ifdef LDIM_SPI_DUTY_VSYNC_DIRECT
		ldim_on_vs_brightness();
#else
		/*schedule_work(&ldim_on_vs_work);*/
		queue_work(ldim_queue, &ldim_on_vs_work);
#endif

	} else {
#ifdef LDIM_SPI_DUTY_VSYNC_DIRECT
		ldim_off_vs_brightness();
#else
		/*schedule_work(&ldim_off_vs_work);*/
		queue_work(ldim_queue, &ldim_off_vs_work);
#endif
	}

	spin_unlock_irqrestore(&ldim_pwm_vs_isr_lock, flags);

	return IRQ_HANDLED;
}

static void ldim_dev_err_handler(void)
{
	struct ldim_dev_driver_s *dev_drv = ldim_driver.dev_drv;
	int ret;

	if (ldim_driver.dev_smr_bypass)
		return;

	if (!dev_drv || !dev_drv->dev_err_handler)
		return;

	ret = dev_drv->dev_err_handler(&ldim_driver);
	if (ret) {
		/*force update for next vsync*/
		ldim_driver.level_update = 1;
	}
}

static void ldim_dev_smr(int update_flag, unsigned int size)
{
	struct ldim_dev_driver_s *dev_drv = ldim_driver.dev_drv;

	if (ldim_driver.dev_smr_bypass)
		return;

	if (!dev_drv)
		return;
	if (!dev_drv->dev_smr) {
		if (ldim_driver.dbg_vs_cnt == 0)
			LDIMERR("%s: dev_smr is null\n", __func__);
		return;
	}

	if (update_flag) {
		dev_drv->dev_smr(&ldim_driver, ldim_driver.bl_matrix_cur, size);
		memcpy(ldim_driver.bl_matrix_pre, ldim_driver.bl_matrix_cur,
				(size * sizeof(unsigned int)));
	} else {
		if (dev_drv->dev_smr_dummy)
			dev_drv->dev_smr_dummy(&ldim_driver);
	}

#ifdef LDIM_SPI_DUTY_VSYNC_DIRECT
	/*schedule_work(&ldim_err_handle_work);*/
	queue_work(ldim_queue, &ldim_err_handle_work);
#else
	ldim_dev_err_handler();
#endif
}

static void ldim_on_vs_brightness(void)
{
	unsigned long long local_time[3];
	unsigned int size, i;
	int update_flag = 0;

	if (ldim_driver.init_on_flag == 0)
		return;

	if (ldim_driver.func_bypass)
		return;

	local_time[0] = sched_clock();

	size = ldim_driver.conf->seg_row * ldim_driver.conf->seg_col;

	if (ldim_driver.test_bl_en) {
		memcpy(ldim_driver.bl_matrix_cur, ldim_driver.test_matrix,
		       (size * sizeof(unsigned int)));
	} else {
		if (ldim_driver.black_frm_en &&
		    ldim_driver.fw->ctrl->black_frm) {
			memset(ldim_driver.bl_matrix_cur, 0,
			       (size * sizeof(unsigned int)));
		} else {
			for (i = 0; i < size; i++) {
				ldim_driver.bl_matrix_cur[i] =
					(((ldim_driver.local_bl_matrix[i] * ldim_driver.litgain)
					+ (LD_DATA_MAX / 2)) >> LD_DATA_DEPTH);
			}
		}
	}

	if (ldim_driver.duty_update_flag & 0x01)
		update_flag = memcmp(ldim_driver.bl_matrix_cur, ldim_driver.bl_matrix_pre,
			     (size * sizeof(unsigned int)));
	else
		update_flag = 1;

	ldim_dev_smr(update_flag, size);

	local_time[1] = sched_clock();
	local_time[2] = local_time[1] - local_time[0];
	ldim_time_sort_save(ldim_driver.xfer_time, local_time[2]);
}

static void ldim_off_vs_brightness(void)
{
	unsigned long long local_time[3];
	unsigned int size, i;
	int update_flag = 0;

	if (ldim_driver.init_on_flag == 0)
		return;

	local_time[0] = sched_clock();

	size = ldim_driver.conf->seg_row * ldim_driver.conf->seg_col;
	if (ldim_driver.test_bl_en) {
		memcpy(ldim_driver.bl_matrix_cur, ldim_driver.test_matrix,
			(size * sizeof(unsigned int)));
	} else {
		for (i = 0; i < size; i++)
			ldim_driver.bl_matrix_cur[i] = ldim_driver.litgain;
	}

	if (ldim_driver.level_update) {
		ldim_driver.level_update = 0;
		update_flag = 1;
	} else {
		if (ldim_driver.duty_update_flag & 0x02)
			update_flag = memcmp(ldim_driver.bl_matrix_cur,
				ldim_driver.bl_matrix_pre,
				(size * sizeof(unsigned int)));
		else
			update_flag = 1;
	}
	if (ldim_debug_print == 4 && update_flag) {
		if (ldim_driver.test_bl_en)
			LDIMPR("%s: test_matrix update\n", __func__);
		else
			LDIMPR("%s: level update: 0x%x\n", __func__, ldim_driver.litgain);
	}
	ldim_dev_smr(update_flag, size);

	local_time[1] = sched_clock();
	local_time[2] = local_time[1] - local_time[0];
	ldim_time_sort_save(ldim_driver.xfer_time, local_time[2]);
}

static void ldim_on_update_brightness(struct work_struct *work)
{
	ldim_on_vs_brightness();
}

static void ldim_off_update_brightness(struct work_struct *work)
{
	ldim_off_vs_brightness();
}

static void ldim_err_handler(struct work_struct *work)
{
	ldim_dev_err_handler();
}

/* ******************************************************
 * local dimming dummy function for virtual ldim dev
 * ******************************************************
 */
static int ldim_dev_add_virtual_driver(struct aml_ldim_driver_s *ldim_drv)
{
	LDIMPR("%s\n", __func__);
	ldim_drv->init();

	return 0;
}

/* ******************************************************/

static int ldim_open(struct inode *inode, struct file *file)
{
	struct ldim_dev_s *devp;

	LDIMPR("%s\n", __func__);
	/* Get the per-device structure that contains this cdev */
	devp = container_of(inode->i_cdev, struct ldim_dev_s, cdev);
	file->private_data = devp;
	return 0;
}

static int ldim_release(struct inode *inode, struct file *file)
{
	LDIMPR("%s\n", __func__);
	file->private_data = NULL;
	return 0;
}

static void aml_ldim_info_update(void)
{
	struct fw_ctrl_s *fctrl;
	int i = 0, j = 0;

	if (!ldim_driver.fw || !ldim_driver.fw->ctrl) {
		LDIMERR("%s: fw ctrl is null\n", __func__);
		return;
	}

	fctrl = ldim_driver.fw->ctrl;
	fctrl->tf_alpha = ldim_info.alpha;
	fctrl->lpf_method = ldim_info.lpf_method;
	fctrl->lpf_gain = ldim_info.lpf_gain;
	fctrl->lpf_res = ldim_info.lpf_res;
	fctrl->side_blk_diff_th = ldim_info.side_blk_diff_th;
	fctrl->bbd_th = ldim_info.bbd_th;
	fctrl->boost_gain = ldim_info.boost_gain;
	fctrl->rgb_base = ldim_info.rgb_base;
	fctrl->ld_remap_bypass = ldim_info.ld_remap_bypass;
	fctrl->ld_tf_step_th = ldim_info.ld_tf_step_th;
	fctrl->tf_blk_fresh_bl = ldim_info.tf_blk_fresh_bl;
	fctrl->tf_fresh_bl = ldim_info.tf_fresh_bl;
	fctrl->fw_ld_thtf_l = ldim_info.fw_ld_thtf_l;
	fctrl->fw_rgb_diff_th = ldim_info.fw_rgb_diff_th;
	fctrl->fw_ld_thist = ldim_info.fw_ld_thist;

	for (i = 0; i < 17; i++)
		fctrl->bl_remap_curve[i] = ldim_info.bl_remap_curve[i];

	for (i = 0; i < 16; i++)
		fctrl->fw_ld_whist[i] = ldim_info.fw_ld_whist[i];

	for (i = 0; i < 16; i++) {
		for (j = 0; j < 32; j++)
			ld_remap_lut[i][j] = ldim_info.reg_ld_remap_lut[i][j];
	}

	ldim_config.remap_en = ldim_info.remapping_en;
	ldim_config.func_en = ldim_info.func_en;
}

static void ldim_remap_lut_print(char *buf, int len)
{
	int i, j;

	LDIMPR("ld_remap_lut:\n");
	for (i = 0; i < 16; i++) {
		LDIMPR("  %d:\n", i);
		len = 0;
		for (j = 0; j < 32; j++) {
			if (j == 16)
				len += sprintf(buf + len, "\n");

			len += sprintf(buf + len, " %d",
				       ldim_info.reg_ld_remap_lut[i][j]);
		}
		LDIMPR("%s\n", buf);
	}
}

static int aml_ldim_pq_update(void)
{
	struct fw_ctrl_s *fctrl;
	struct ldim_comp_s *comp;
	int i = 0, j = 0;
	int ret = 0;

	if (!ldim_driver.fw || !ldim_driver.fw->ctrl) {
		LDIMERR("%s: fw ctrl is null\n", __func__);
		return -EFAULT;
	}

	ldim_driver.fw->fw_sel = ldim_pq.fw_sel;

	fctrl = ldim_driver.fw->ctrl;

	fctrl->prm_ldc->ldc_hist_mode = ldim_pq.ldc_hist_mode;
	fctrl->prm_ldc->ldc_hist_blend_mode = ldim_pq.ldc_hist_blend_mode;
	fctrl->prm_ldc->ldc_hist_blend_alpha = ldim_pq.ldc_hist_blend_alpha;
	fctrl->prm_ldc->ldc_hist_adap_blend_max_gain =
			ldim_pq.ldc_hist_adap_blend_max_gain;
	fctrl->prm_ldc->ldc_hist_adap_blend_diff_th1 =
			ldim_pq.ldc_hist_adap_blend_diff_th1;
	fctrl->prm_ldc->ldc_hist_adap_blend_diff_th2 =
			ldim_pq.ldc_hist_adap_blend_diff_th2;
	fctrl->prm_ldc->ldc_hist_adap_blend_th0 =
			ldim_pq.ldc_hist_adap_blend_th0;
	fctrl->prm_ldc->ldc_hist_adap_blend_thn =
			ldim_pq.ldc_hist_adap_blend_thn;
	fctrl->prm_ldc->ldc_hist_adap_blend_gain_0 =
			ldim_pq.ldc_hist_adap_blend_gain_0;
	fctrl->prm_ldc->ldc_hist_adap_blend_gain_1 =
			ldim_pq.ldc_hist_adap_blend_gain_1;
	fctrl->prm_ldc->ldc_init_bl_min = ldim_pq.ldc_init_bl_min;
	fctrl->prm_ldc->ldc_init_bl_max = ldim_pq.ldc_init_bl_max;

	fctrl->prm_ldc->ldc_sf_mode = ldim_pq.ldc_sf_mode;
	fctrl->prm_ldc->ldc_sf_gain_up = ldim_pq.ldc_sf_gain_up;
	fctrl->prm_ldc->ldc_sf_gain_dn = ldim_pq.ldc_sf_gain_dn;
	fctrl->prm_ldc->ldc_sf_tsf_3x3 = ldim_pq.ldc_sf_tsf_3x3;
	fctrl->prm_ldc->ldc_sf_tsf_5x5 = ldim_pq.ldc_sf_tsf_5x5;

	fctrl->prm_ldc->ldc_bs_bl_mode = ldim_pq.ldc_bs_bl_mode;
	//fctrl->prm_ldc->ldc_glb_apl = ldim_pq.ldc_glb_apl;
	fctrl->prm_ldc->ldc_bs_glb_apl_gain = ldim_pq.ldc_bs_glb_apl_gain;
	fctrl->prm_ldc->ldc_bs_dark_scene_bl_th =
			ldim_pq.ldc_bs_dark_scene_bl_th;
	fctrl->prm_ldc->ldc_bs_gain = ldim_pq.ldc_bs_gain;
	fctrl->prm_ldc->ldc_bs_limit_gain = ldim_pq.ldc_bs_limit_gain;
	fctrl->prm_ldc->ldc_bs_loc_apl_gain = ldim_pq.ldc_bs_loc_apl_gain;
	fctrl->prm_ldc->ldc_bs_loc_max_min_gain =
			ldim_pq.ldc_bs_loc_max_min_gain;
	fctrl->prm_ldc->ldc_bs_loc_dark_scene_bl_th =
			ldim_pq.ldc_bs_loc_dark_scene_bl_th;

	fctrl->prm_ldc->ldc_tf_en = ldim_pq.ldc_tf_en;
	//fctrl->prm_ldc->ldc_tf_sc_flag = ldim_pq.ldc_tf_sc_flag;
	fctrl->prm_ldc->ldc_tf_low_alpha = ldim_pq.ldc_tf_low_alpha;
	fctrl->prm_ldc->ldc_tf_high_alpha = ldim_pq.ldc_tf_high_alpha;
	fctrl->prm_ldc->ldc_tf_low_alpha_sc = ldim_pq.ldc_tf_low_alpha_sc;
	fctrl->prm_ldc->ldc_tf_high_alpha_sc = ldim_pq.ldc_tf_high_alpha_sc;

	fctrl->prm_ldc->ldc_dimming_curve_en = ldim_pq.ldc_dimming_curve_en;
	fctrl->prm_ldc->ldc_sc_hist_diff_th = ldim_pq.ldc_sc_hist_diff_th;
	fctrl->prm_ldc->ldc_sc_apl_diff_th = ldim_pq.ldc_sc_apl_diff_th;
	for (i = 0; i < 17; i++)
		fctrl->bl_remap_curve[i] = ldim_pq.bl_remap_curve[i];

	comp = ldim_driver.comp;
	comp->ldc_bl_buf_diff = ldim_pq.ldc_bl_buf_diff;
	comp->ldc_glb_gain = ldim_pq.ldc_glb_gain;
	comp->ldc_dth_en = ldim_pq.ldc_dth_en;
	comp->ldc_dth_bw = ldim_pq.ldc_dth_bw;

	for (i = 0; i < 16; i++) {
		for (j = 0; j < 64; j++)
			ldc_gain_lut_array[i][j] = ldim_pq.ldc_gain_lut[i][j];
	}
	for (i = 0; i < 64; i++)
		ldc_min_gain_lut[i] = ldim_pq.ldc_min_gain_lut[i];
	for (i = 0; i < 32; i++) {
		for (j = 0; j < 16; j++)
			ldc_dither_lut[i][j] = ldim_pq.ldc_dither_lut[i][j];
	}

	ldim_config.remap_en = ldim_pq.remapping_en;
	ldim_config.func_en = ldim_pq.func_en;
	LDIMPR("%s, func_en = %d, remap_en = %d\n",
		__func__, ldim_config.func_en, ldim_config.remap_en);

	if (ldim_driver.init_on_flag == 0 || ldim_driver.pq_updating) {
		LDIMERR("%s, init_on_flag = %d, pq_updating = %d\n",
		__func__, ldim_driver.init_on_flag, ldim_driver.pq_updating);
		return -EFAULT;
	}
	if (ldim_driver.init_on_flag)
		ldim_driver.pq_updating = 1;

	return ret;
}

static long ldim_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int i = 0, len = 0;
	int ret = 0;
	char *curve_buf;
	char *lut_buf;
	void __user *argp;
	int mcd_nr;
	unsigned char *buf;
	struct aml_ldim_bin_s ldim_buff;

	mcd_nr = _IOC_NR(cmd);
	LDIMPR("%s: cmd_dir = 0x%x, cmd_nr = 0x%x\n",
	       __func__, _IOC_DIR(cmd), mcd_nr);

	argp = (void __user *)arg;
	switch (mcd_nr) {
	case AML_LDIM_IOC_NR_GET_INFO:
		if (copy_to_user(argp, &ldim_info,
				 sizeof(struct aml_ldim_info_s))) {
			ret = -EFAULT;
		}
		break;
	case AML_LDIM_IOC_NR_SET_INFO:
		memset(&ldim_info, 0, sizeof(struct aml_ldim_info_s));
		if (copy_from_user(&ldim_info, argp,
				   sizeof(struct aml_ldim_info_s))) {
			ret = -EFAULT;
		} else {
			aml_ldim_info_update();
			if (ldim_debug_print) {
				LDIMPR("set ldim info:\n"
				       "func_en:		%d\n"
				       "remapping_en:		%d\n"
				       "alpha:			%d\n"
				       "LPF_method:		%d\n"
				       "lpf_gain:		%d\n"
				       "lpf_res:		%d\n"
				       "side_blk_diff_th:	%d\n"
				       "bbd_th:			%d\n"
				       "boost_gain:		%d\n"
				       "rgb_base:		%d\n"
				       "Ld_remap_bypass:	%d\n"
				       "LD_TF_STEP_TH:		%d\n"
				       "TF_BLK_FRESH_BL:	%d\n"
				       "TF_FRESH_BL:		%d\n"
				       "fw_LD_ThTF_l:		%d\n"
				       "fw_rgb_diff_th:		%d\n"
				       "fw_ld_thist:		%d\n",
				       ldim_info.func_en,
				       ldim_info.remapping_en,
				       ldim_info.alpha,
				       ldim_info.lpf_method,
				       ldim_info.lpf_gain,
				       ldim_info.lpf_res,
				       ldim_info.side_blk_diff_th,
				       ldim_info.bbd_th,
				       ldim_info.boost_gain,
				       ldim_info.rgb_base,
				       ldim_info.ld_remap_bypass,
				       ldim_info.ld_tf_step_th,
				       ldim_info.tf_blk_fresh_bl,
				       ldim_info.tf_fresh_bl,
				       ldim_info.fw_ld_thtf_l,
				       ldim_info.fw_rgb_diff_th,
				       ldim_info.fw_ld_thist);

				/* ldim_bl_remap_curve_print */
				len = 17 * 8 + 20;
				curve_buf = kcalloc(len, sizeof(char), GFP_KERNEL);
				if (!curve_buf)
					return 0;
				LDIMPR("bl_remap_curve:\n");
				len = 0;
				for (i = 0; i < 17; i++)
					len +=
					sprintf(curve_buf + len, "\t%d",
						ldim_info.bl_remap_curve[i]);
				LDIMPR("%s\n", curve_buf);

				LDIMPR("fw_ld_whist:\n");
				len = 0;
				for (i = 0; i < 16; i++) {
					len += sprintf(curve_buf + len, "\t%d",
						ldim_info.fw_ld_whist[i]);
				}
				LDIMPR("%s\n", curve_buf);

				/*ldim_bl_remap_lut_print*/
				len = 32 * 8 + 20;
				lut_buf = kcalloc(len, sizeof(char), GFP_KERNEL);
				if (!lut_buf) {
					kfree(curve_buf);
					return 0;
				}
				ldim_remap_lut_print(lut_buf, len);
				kfree(curve_buf);
				kfree(lut_buf);
			}
		}
		break;
	case AML_LDIM_IOC_NR_GET_INFO_NEW:
		if (copy_to_user(argp, &ldim_pq,
				 sizeof(struct aml_ldim_pq_s))) {
			ret = -EFAULT;
		}
		break;
	case AML_LDIM_IOC_NR_SET_INFO_NEW:
		memset(&ldim_pq, 0, sizeof(struct aml_ldim_pq_s));
		if (copy_from_user(&ldim_pq, argp,
				   sizeof(struct aml_ldim_pq_s))) {
			ret = -EFAULT;
		}
		if (memcmp(&ldim_pq, &ldim_pq_pre, sizeof(struct aml_ldim_pq_s))) {
			memset(&ldim_pq_pre, 0, sizeof(struct aml_ldim_pq_s));
			memcpy(&ldim_pq_pre, &ldim_pq, sizeof(struct aml_ldim_pq_s));
			LDIMPR("%s pq data is different, do pq update\n", __func__);
			ret = aml_ldim_pq_update();
		} else {
			LDIMPR("%s pq data same as before!! do nothing!\n", __func__);
		}
		break;
	case AML_LDIM_IOC_NR_GET_BL_MAPPING_PATH:
		LDIMPR("get bl_mapping_path is(%s)\n", ldim_driver.dev_drv->bl_mapping_path);
		if (copy_to_user(argp, ldim_driver.dev_drv->bl_mapping_path,
				 0xff)) {
			ret = -EFAULT;
		}
		break;
	case AML_LDIM_IOC_NR_SET_BL_MAPPING:
		if (copy_from_user(&ldim_buff,
				   (void __user *)arg,
				   sizeof(struct aml_ldim_bin_s))) {
			LDIMERR("cp aml_ldim_bin_s fail\n");
			return -EFAULT;
		}
		argp = (void __user *)ldim_buff.ptr;
		LDIMPR("index =  0x%x, len=  0x%x\n", ldim_buff.index, ldim_buff.len);
		if (ldim_buff.len != (2 * ldim_driver.dev_drv->zone_num)) {
			LDIMERR("bl_mapping_size = %d NOT match with zone_num!!!\n", ldim_buff.len);
			return -EFAULT;
		}
		buf = vmalloc(ldim_buff.len);
		if (!buf) {
			LDIMERR("%s vmalloc buf for receive bl mapping failed\n", __func__);
			vfree(buf);
			return -EFAULT;
		}
		if (copy_from_user((void *)buf, argp,
				   ldim_buff.len)) {
			LDIMERR("cp aml_ldim_bin_s to buf fail\n");
			vfree(buf);
			return -EFAULT;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
			for (i = 0; i < ldim_buff.len; i++)
				LDIMPR("buf[%d] =  0x%x\n", i, buf[i]);
		}
		memset(ldim_driver.dev_drv->bl_mapping, 0, ldim_buff.len);
		memcpy(ldim_driver.dev_drv->bl_mapping, buf, ldim_buff.len);
		LDIMPR("%s : write %d bytes zone_mapping done\n", __func__, ldim_buff.len);
		vfree(buf);
		break;
	case AML_LDIM_IOC_NR_GET_BL_PROFILE_PATH:
		LDIMPR("get profile_path is(%s)\n",
			ldim_driver.dev_drv->bl_profile->file_path);
		if (copy_to_user(argp, ldim_driver.dev_drv->bl_profile->file_path,
				 0xff)) {
			ret = -EFAULT;
		}
		break;
	case AML_LDIM_IOC_NR_SET_BL_PROFILE:
		if (copy_from_user(&ldim_buff,
				   (void __user *)arg,
				   sizeof(struct aml_ldim_bin_s))) {
			LDIMERR("cp aml_ldim_bin_s fail\n");
			return -EFAULT;
		}
		argp = (void __user *)ldim_buff.ptr;
		LDIMPR("index =  0x%x, len=  0x%x\n", ldim_buff.index, ldim_buff.len);
		if (ldim_buff.len == 0 || ldim_buff.len > 0xC0000) {
			LDIMERR("profile bin size = %d is invalid!!\n",
				ldim_buff.len);
			return -EFAULT;
		}
		buf = vmalloc(ldim_buff.len);
		if (!buf) {
			LDIMERR("%s vmalloc buf for receive profile failed\n", __func__);
			vfree(buf);
			return -EFAULT;
		}
		if (copy_from_user((void *)buf, argp,
				   ldim_buff.len)) {
			LDIMERR("cp aml_ldim_bin_s to buf fail\n");
			vfree(buf);
			return -EFAULT;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
			for (i = 0; i < 100; i++)  //for check profile begins
				LDIMPR("profile[%d] =  0x%x\n", i, buf[i]);
		}
		ldc_mem_write_profile(buf, ldim_driver.rmem->profile_mem_paddr, ldim_buff.len);
		LDIMPR("%s : write %d bytes profile done\n", __func__, ldim_buff.len);
		vfree(buf);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long ldim_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = ldim_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations ldim_fops = {
	.owner          = THIS_MODULE,
	.open           = ldim_open,
	.release        = ldim_release,
	.unlocked_ioctl = ldim_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ldim_compat_ioctl,
#endif
};

int aml_ldim_get_config_dts(struct device_node *child)
{
	struct vinfo_s *vinfo = get_current_vinfo();
	unsigned int para[5];
	int ret;

	if (!child) {
		LDIMERR("child device_node is null\n");
		return -1;
	}

	/* default setting */
	ldim_config.hsize = vinfo->width;
	ldim_config.vsize = vinfo->height;

	/* get row & col from dts */
	ret = of_property_read_u32_array(child, "bl_ldim_zone_row_col", para, 2);
	if (ret) {
		ret = of_property_read_u32_array(child, "bl_ldim_region_row_col", para, 2);
		if (ret) {
			LDIMERR("failed to get bl_ldim_zone_row_col\n");
			goto aml_ldim_get_config_dts_next;
		}
	}
	if ((para[0] * para[1]) > LD_BLKREGNUM) {
		LDIMERR("zone row*col(%d*%d) is out of support\n", para[0], para[1]);
	} else {
		ldim_config.seg_row = para[0];
		ldim_config.seg_col = para[1];
	}
	LDIMPR("get bl_zone row = %d, col = %d\n",
	       ldim_config.seg_row, ldim_config.seg_col);

aml_ldim_get_config_dts_next:
	/* get bl_mode from dts */
	ret = of_property_read_u32(child, "bl_ldim_mode", &para[0]);
	if (ret)
		LDIMERR("failed to get bl_ldim_mode\n");
	else
		ldim_config.bl_mode = (unsigned char)para[0];
	LDIMPR("get bl_mode = %d\n", ldim_config.bl_mode);

	/* get ldim_dev_index from dts */
	ret = of_property_read_u32(child, "ldim_dev_index", &para[0]);
	if (ret)
		LDIMERR("failed to get ldim_dev_index\n");
	else
		ldim_config.dev_index = (unsigned char)para[0];
	if (ldim_config.dev_index < 0xff)
		LDIMPR("get ldim_dev_index = %d\n", ldim_config.dev_index);

	return 0;
}

int aml_ldim_get_config_unifykey(unsigned char *buf)
{
	unsigned char *p;
	struct vinfo_s *vinfo = get_current_vinfo();

	/* default setting */
	ldim_config.hsize = vinfo->width;
	ldim_config.vsize = vinfo->height;

	p = buf;

	/* ldim: 24byte */
	/* get bl_ldim_region_row_col 4byte*/
	ldim_config.seg_row = *(p + LCD_UKEY_BL_LDIM_ROW);
	ldim_config.seg_col = *(p + LCD_UKEY_BL_LDIM_COL);
	LDIMPR("get bl_zone row = %d, col = %d\n",
	       ldim_config.seg_row, ldim_config.seg_col);

	/* get bl_ldim_mode 1byte*/
	ldim_config.bl_mode = *(p + LCD_UKEY_BL_LDIM_MODE);
	LDIMPR("get bl_mode = %d\n", ldim_config.bl_mode);

	/* get ldim_dev_index 1byte*/
	ldim_config.dev_index = *(p + LCD_UKEY_BL_LDIM_DEV_INDEX);
	if (ldim_config.dev_index < 0xff)
		LDIMPR("get dev_index = %d\n", ldim_config.dev_index);

	return 0;
}

static int ldim_rmem_tm2(void)
{
	/* init reserved memory */
	ldim_rmem.wr_mem_vaddr1 = kmalloc(ldim_rmem.wr_mem_size, GFP_KERNEL);
	if (!ldim_rmem.wr_mem_vaddr1)
		goto ldim_rmem_err0;
	ldim_rmem.wr_mem_paddr1 = virt_to_phys(ldim_rmem.wr_mem_vaddr1);

	ldim_rmem.wr_mem_vaddr2 = kmalloc(ldim_rmem.wr_mem_size, GFP_KERNEL);
	if (!ldim_rmem.wr_mem_vaddr2)
		goto ldim_rmem_err1;
	ldim_rmem.wr_mem_paddr2 = virt_to_phys(ldim_rmem.wr_mem_vaddr2);

	ldim_rmem.rd_mem_vaddr1 = kmalloc(ldim_rmem.rd_mem_size, GFP_KERNEL);
	if (!ldim_rmem.rd_mem_vaddr1)
		goto ldim_rmem_err2;
	ldim_rmem.rd_mem_paddr1 = virt_to_phys(ldim_rmem.rd_mem_vaddr1);

	ldim_rmem.rd_mem_vaddr2 = kmalloc(ldim_rmem.rd_mem_size, GFP_KERNEL);
	if (!ldim_rmem.rd_mem_vaddr2)
		goto ldim_rmem_err3;
	ldim_rmem.rd_mem_paddr2 = virt_to_phys(ldim_rmem.rd_mem_vaddr2);

	memset(ldim_rmem.wr_mem_vaddr1, 0, ldim_rmem.wr_mem_size);
	memset(ldim_rmem.wr_mem_vaddr2, 0, ldim_rmem.wr_mem_size);
	memset(ldim_rmem.rd_mem_vaddr1, 0, ldim_rmem.rd_mem_size);
	memset(ldim_rmem.rd_mem_vaddr2, 0, ldim_rmem.rd_mem_size);

	return 0;

ldim_rmem_err3:
	kfree(ldim_rmem.rd_mem_vaddr1);
ldim_rmem_err2:
	kfree(ldim_rmem.wr_mem_vaddr2);
ldim_rmem_err1:
	kfree(ldim_rmem.wr_mem_vaddr1);
ldim_rmem_err0:
	return -1;
}

static int aml_ldim_malloc(struct platform_device *pdev, struct ldim_drv_data_s *data,
			   unsigned int row, unsigned int col)
{
	struct ldim_stts_s *stts = ldim_driver.stts;
	struct ldim_fw_s *fw = ldim_driver.fw;
	unsigned int zone_num = row * col;
	int i, ret = 0;

	if (!fw) {
		LDIMERR("%s: fw is null\n", __func__);
		return -1;
	}

	if (ldim_dev.data->alloc_rmem) {
		ret = ldim_dev.data->alloc_rmem();
		if (ret)
			goto ldim_malloc_err0;
	}

	if (!stts)
		goto ldim_malloc_err0;
	stts->hist_matrix = kcalloc((zone_num * 16), sizeof(unsigned int),
				    GFP_KERNEL);
	if (!stts->hist_matrix)
		goto ldim_malloc_err0;

	stts->max_rgb = kcalloc((zone_num * 3), sizeof(unsigned int),
				GFP_KERNEL);
	if (!stts->max_rgb)
		goto ldim_malloc_err1;

	ldim_driver.local_bl_matrix = kcalloc(zone_num, sizeof(unsigned int),
					      GFP_KERNEL);
	if (!ldim_driver.local_bl_matrix)
		goto ldim_malloc_err2;

	ldim_driver.bl_matrix_cur = kcalloc(zone_num, sizeof(unsigned int),
					    GFP_KERNEL);
	if (!ldim_driver.bl_matrix_cur)
		goto ldim_malloc_err3;

	ldim_driver.bl_matrix_pre = kcalloc(zone_num, sizeof(unsigned int),
					    GFP_KERNEL);
	if (!ldim_driver.bl_matrix_pre)
		goto ldim_malloc_err4;

	ldim_driver.test_matrix = kcalloc(zone_num, sizeof(unsigned int),
					  GFP_KERNEL);
	if (!ldim_driver.test_matrix)
		goto ldim_malloc_err5;
	for (i = 0; i < zone_num; i++)
		ldim_driver.test_matrix[i] = 2048;

	return 0;

ldim_malloc_err5:
	kfree(ldim_driver.bl_matrix_pre);
	ldim_driver.bl_matrix_pre = NULL;
ldim_malloc_err4:
	kfree(ldim_driver.bl_matrix_cur);
	ldim_driver.bl_matrix_cur = NULL;
ldim_malloc_err3:
	kfree(ldim_driver.local_bl_matrix);
	ldim_driver.local_bl_matrix = NULL;
ldim_malloc_err2:
	kfree(stts->max_rgb);
	stts->max_rgb = NULL;
ldim_malloc_err1:
	kfree(stts->hist_matrix);
	stts->hist_matrix = NULL;
ldim_malloc_err0:
	LDIMERR("%s failed\n", __func__);
	return -1;
}

static unsigned char *aml_ldim_rmem_map(unsigned long paddr, unsigned int mem_size,
					unsigned int *flag)
{
	unsigned char *vaddr = NULL;
	unsigned int highmem_flag = 0;

	highmem_flag = PageHighMem(phys_to_page(paddr));
	*flag = highmem_flag;
	//LDIMPR("%s: highmem_flag: %d\n", __func__, highmem_flag);
	if (highmem_flag == 0) {
		vaddr = phys_to_virt(paddr);
		if (!vaddr)
			return NULL;
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
			LDIMPR("%s: phys_to_virt: paddr=0x%lx, vaddr=0x%px, size: 0x%x\n",
		       __func__, paddr, vaddr, mem_size);
	} else {
		vaddr = lcd_vmap(paddr, mem_size);
		if (!vaddr)
			return NULL;
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
			LDIMPR("%s: vmap: paddr=0x%lx, vaddr=0x%px, size: 0x%x\n",
		       __func__, paddr, vaddr, mem_size);
	}

	return vaddr;
}

static void aml_ldim_rmem_unmap(unsigned char *vaddr, unsigned int flag)
{
	if (flag)
		lcd_unmap_phyaddr(vaddr);
}

static int aml_ldim_malloc_t7(struct platform_device *pdev, struct ldim_drv_data_s *data,
			      unsigned int row, unsigned int col)
{
	struct ldim_stts_s *stts = ldim_driver.stts;
	struct ldim_fw_s *fw = ldim_driver.fw;
	unsigned int zone_num = row * col;
	unsigned int mem_size;
	int i, ret = 0;

	/* init reserved memory */
	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret) {
		LDIMERR("init reserved memory failed\n");
		return -1;
	}
	mem_size = ldim_rmem.rsv_mem_size;
	ldim_rmem.rsv_mem_vaddr = dma_alloc_coherent(&pdev->dev, mem_size,
					&ldim_rmem.rsv_mem_paddr, GFP_KERNEL);
	if (!ldim_rmem.rsv_mem_vaddr) {
		ldim_rmem.flag = 0;
		LDIMERR("ldc resv mem failed\n");
		return -1;
	}
	ldim_rmem.rsv_mem_size = mem_size;
	ldim_rmem.flag = 1;
	LDIMPR("ldc resv mem paddr: 0x%lx, size: 0x%x\n",
	       (unsigned long)ldim_rmem.rsv_mem_paddr, ldim_rmem.rsv_mem_size);

	ldim_rmem.profile_mem_paddr = ldim_rmem.rsv_mem_paddr + LDC_PROFILE_OFFSET;
	ldim_rmem.profile_mem_size = LDC_GLOBAL_HIST_OFFSET - LDC_PROFILE_OFFSET;

	ldim_rmem.global_hist_paddr = ldim_rmem.rsv_mem_paddr + LDC_GLOBAL_HIST_OFFSET;
	ldim_rmem.global_hist_mem_size = LDC_SEG_HIST0_OFFSET - LDC_GLOBAL_HIST_OFFSET;
	ldim_rmem.global_hist_vaddr = aml_ldim_rmem_map(ldim_rmem.global_hist_paddr,
							ldim_rmem.global_hist_mem_size,
							&ldim_rmem.global_hist_highmem_flag);
	if (!ldim_rmem.global_hist_vaddr)
		goto ldim_malloc_t7_err0;

	ldim_rmem.seg_hist_paddr = ldim_rmem.rsv_mem_paddr + LDC_SEG_HIST0_OFFSET;
	ldim_rmem.seg_hist_mem_size = LDC_SEG_DUTY0_OFFSET - LDC_SEG_HIST0_OFFSET;
	ldim_rmem.seg_hist_vaddr = aml_ldim_rmem_map(ldim_rmem.seg_hist_paddr,
						     ldim_rmem.seg_hist_mem_size,
						     &ldim_rmem.seg_hist_highmem_flag);
	if (!ldim_rmem.seg_hist_vaddr) {
		aml_ldim_rmem_unmap(ldim_rmem.global_hist_vaddr,
				    ldim_rmem.global_hist_highmem_flag);
		goto ldim_malloc_t7_err0;
	}

	ldim_rmem.duty_paddr = ldim_rmem.rsv_mem_paddr + LDC_SEG_DUTY0_OFFSET;
	ldim_rmem.duty_mem_size = LDC_MEM_END - LDC_SEG_DUTY0_OFFSET;
	ldim_rmem.duty_vaddr = aml_ldim_rmem_map(ldim_rmem.duty_paddr,
						 ldim_rmem.duty_mem_size,
						 &ldim_rmem.duty_highmem_flag);
	if (!ldim_rmem.duty_vaddr) {
		aml_ldim_rmem_unmap(ldim_rmem.seg_hist_vaddr,
				    ldim_rmem.seg_hist_highmem_flag);
		goto ldim_malloc_t7_err0;
	}

	if (!stts)
		goto ldim_malloc_t7_err0;

	stts->global_hist = kcalloc(64, sizeof(unsigned int), GFP_KERNEL);
	if (!stts->global_hist) {
		aml_ldim_rmem_unmap(ldim_rmem.duty_vaddr,
				    ldim_rmem.duty_highmem_flag);
		goto ldim_malloc_t7_err0;
	}

	stts->seg_hist = kcalloc(zone_num, sizeof(*stts->seg_hist), GFP_KERNEL);
	if (!stts->seg_hist)
		goto ldim_malloc_t7_err1;

	if (!fw)
		goto ldim_malloc_t7_err2;
	fw->bl_matrix = kcalloc(zone_num, sizeof(unsigned int), GFP_KERNEL);
	if (!fw->bl_matrix)
		goto ldim_malloc_t7_err2;

	ldim_driver.local_bl_matrix = kcalloc(zone_num, sizeof(unsigned int),
					      GFP_KERNEL);
	if (!ldim_driver.local_bl_matrix)
		goto ldim_malloc_t7_err3;

	ldim_driver.bl_matrix_cur = kcalloc(zone_num, sizeof(unsigned int),
					    GFP_KERNEL);
	if (!ldim_driver.bl_matrix_cur)
		goto ldim_malloc_t7_err4;

	ldim_driver.bl_matrix_pre = kcalloc(zone_num, sizeof(unsigned int),
					    GFP_KERNEL);
	if (!ldim_driver.bl_matrix_pre)
		goto ldim_malloc_t7_err5;

	ldim_driver.test_matrix = kcalloc(zone_num, sizeof(unsigned int),
					  GFP_KERNEL);
	if (!ldim_driver.test_matrix)
		goto ldim_malloc_t7_err6;
	for (i = 0; i < zone_num; i++)
		ldim_driver.test_matrix[i] = 2048;

	return 0;

ldim_malloc_t7_err6:
	kfree(ldim_driver.bl_matrix_pre);
ldim_malloc_t7_err5:
	kfree(ldim_driver.bl_matrix_cur);
ldim_malloc_t7_err4:
	kfree(ldim_driver.local_bl_matrix);
ldim_malloc_t7_err3:
	kfree(fw->bl_matrix);
ldim_malloc_t7_err2:
	kfree(stts->seg_hist);
ldim_malloc_t7_err1:
	kfree(stts->global_hist);
ldim_malloc_t7_err0:
	LDIMERR("%s failed\n", __func__);
	return -1;
}

static void ldim_remap_update_t3(struct ld_reg_s *nprm,
		unsigned int avg_update_en, unsigned int matrix_update_en)
{
	if (ldim_driver.init_on_flag == 0 && ldim_driver.func_en) {
		if (ldim_driver.data && ldim_driver.data->func_ctrl)
			ldim_driver.data->func_ctrl(&ldim_driver, 0);
		return;
	}

	if (ldim_driver.ld_sel == 0 && ldim_driver.func_en == 0) {
		if (ldim_debug_print == 5)
			LDIMPR("%s: exit for ld_sel=0\n", __func__);
		return;
	} else if (ldim_driver.ld_sel == 0 && ldim_driver.func_en) {
		if (ldim_driver.data && ldim_driver.data->func_ctrl)
			ldim_driver.data->func_ctrl(&ldim_driver, 0);
		return;
	}

	if (ldim_config.func_en != ldim_driver.func_en) {
		if (ldim_debug_print == 5)
			LDIMPR("%s  func_en = %d : %d\n", __func__,
			ldim_driver.func_en, ldim_config.func_en);
		if (ldim_driver.data && ldim_driver.data->func_ctrl)
			ldim_driver.data->func_ctrl(&ldim_driver,
			ldim_config.func_en);
	} else if (ldim_config.remap_en != ldim_driver.remap_en) {
		if (ldim_driver.func_en) {
			ldim_hw_remap_en_t7(&ldim_driver, ldim_config.remap_en);
			ldim_driver.remap_en = ldim_config.remap_en;
		}
	}

	if (ldim_driver.pq_updating && ldim_driver.init_on_flag) {
		ldc_min_gain_lut_set();
		ldc_dither_lut_set();
		ldim_driver.data->remap_lut_update();
		ldim_config_update_t7(&ldim_driver);
		ldim_hw_remap_en_t7(&ldim_driver, ldim_config.remap_en);
		ldim_driver.pq_updating = 0;
		if (ldim_debug_print == 5)
			LDIMPR("%s  pq_updating = %d\n", __func__,
			ldim_driver.pq_updating);
	}
}

static struct ldim_drv_data_s ldim_data_tl1 = {
	.h_zone_max = 31,
	.v_zone_max = 16,
	.total_zone_max = 128,
	.wr_mem_size = 0,
	.rd_mem_size = 0,

	.remap_update = NULL,
	.alloc_rmem = NULL,
	.stts_init = ldim_hw_stts_initial_tl1,
	.remap_init = NULL,
	.vs_arithmetic = ldim_vs_arithmetic,

	.memory_init = aml_ldim_malloc,
	.drv_init = ldim_drv_init,
	.func_ctrl = ldim_func_ctrl,
	.remap_lut_update = NULL,
};

static struct ldim_drv_data_s ldim_data_tm2 = {
	.h_zone_max = 48,
	.v_zone_max = 32,
	.total_zone_max = 1536,
	.wr_mem_size = 0x3000,
	.rd_mem_size = 0x1000,

	.remap_update = ldim_hw_remap_update_tm2,
	.alloc_rmem = ldim_rmem_tm2,
	.stts_init = ldim_hw_stts_initial_tm2,
	.remap_init = ldim_hw_remap_init_tm2,
	.vs_arithmetic = ldim_vs_arithmetic_tm2,

	.memory_init = aml_ldim_malloc,
	.drv_init = ldim_drv_init,
	.func_ctrl = ldim_func_ctrl,
	.remap_lut_update = NULL,
};

static struct ldim_drv_data_s ldim_data_tm2b = {
	.h_zone_max = 48,
	.v_zone_max = 32,
	.total_zone_max = 1536,
	.wr_mem_size = 0x3000,
	.rd_mem_size = 0x1000,

	.remap_update = ldim_hw_remap_update_tm2b,
	.alloc_rmem = ldim_rmem_tm2,
	.stts_init = ldim_hw_stts_initial_tm2,
	.remap_init = ldim_hw_remap_init_tm2,
	.vs_arithmetic = ldim_vs_arithmetic_tm2,

	.memory_init = aml_ldim_malloc,
	.drv_init = ldim_drv_init,
	.func_ctrl = ldim_func_ctrl,
	.remap_lut_update = NULL,
};

static struct ldim_drv_data_s ldim_data_t7 = {
	.h_zone_max = 48,
	.v_zone_max = 32,
	.total_zone_max = 1536,
	.wr_mem_size = 0x400000,
	.rd_mem_size = 0,

	.remap_update = ldim_remap_update_t3,
	.alloc_rmem = NULL,
	.stts_init = NULL,
	.remap_init = NULL,
	.vs_arithmetic = ldim_vs_arithmetic_t7,

	.memory_init = aml_ldim_malloc_t7,
	.drv_init = ldim_drv_init_t7,
	.func_ctrl = ldim_func_ctrl_t7,
	.remap_lut_update = ldc_gain_lut_set_t7,
};

static struct ldim_drv_data_s ldim_data_t3 = {
	.h_zone_max = 48,
	.v_zone_max = 32,
	.total_zone_max = 1536,
	.wr_mem_size = 0x400000,
	.rd_mem_size = 0,

	.remap_update = ldim_remap_update_t3,
	.alloc_rmem = NULL,
	.stts_init = NULL,
	.remap_init = NULL,
	.vs_arithmetic = ldim_vs_arithmetic_t7,

	.memory_init = aml_ldim_malloc_t7,
	.drv_init = ldim_drv_init_t3,
	.func_ctrl = ldim_func_ctrl_t7,
	.remap_lut_update = ldc_gain_lut_set_t3,
};

static int ldim_region_num_check(struct ldim_drv_data_s *data)
{
	unsigned short temp;

	if (!data) {
		LDIMERR("%s: data is NULL\n", __func__);
		return -1;
	}

	if (ldim_config.seg_row > data->v_zone_max) {
		LDIMERR("%s: blk row (%d) is out of support (%d)\n",
			__func__, ldim_config.seg_row, data->v_zone_max);
		return -1;
	}
	if (ldim_config.seg_col > data->h_zone_max) {
		LDIMERR("%s: blk col (%d) is out of support (%d)\n",
			__func__, ldim_config.seg_col, data->h_zone_max);
		return -1;
	}
	temp = ldim_config.seg_row * ldim_config.seg_col;
	if (temp > data->total_zone_max) {
		LDIMERR("%s: blk total region (%d) is out of support (%d)\n",
			__func__, temp, data->total_zone_max);
		return -1;
	}

	return 0;
}

int aml_ldim_probe(struct platform_device *pdev)
{
	int ret = 0;
	unsigned int ldim_vsync_irq = 0;
	unsigned int ldim_pwm_vs_irq = 0;
	struct ldim_dev_s *devp = &ldim_dev;
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);
	struct ldim_fw_s *fw = aml_ldim_get_fw();
	struct ldim_fw_custom_s *fw_cus_pre = aml_ldim_get_fw_cus_pre();
	struct ldim_fw_custom_s *fw_cus_post = aml_ldim_get_fw_cus_post();

	memset(devp, 0, (sizeof(struct ldim_dev_s)));

#ifdef LDIM_DEBUG_INFO
	ldim_debug_print = 1;
#endif

	switch (bdrv->data->chip_type) {
	case LCD_CHIP_TL1:
		devp->data = &ldim_data_tl1;
		ldim_driver.data = &ldim_data_tl1;
		break;
	case LCD_CHIP_TM2:
		devp->data = &ldim_data_tm2;
		ldim_driver.data = &ldim_data_tm2;
		devp->vpu_dev = vpu_dev_register(VPU_DMA, "bl_ldim");
		break;
	case LCD_CHIP_TM2B:
		devp->data = &ldim_data_tm2b;
		ldim_driver.data = &ldim_data_tm2b;
		devp->vpu_dev = vpu_dev_register(VPU_DMA, "bl_ldim");
		break;
	case LCD_CHIP_T7:
		devp->data = &ldim_data_t7;
		ldim_driver.data = &ldim_data_t7;
		break;
	case LCD_CHIP_T3:
	case LCD_CHIP_T5W:
		devp->data = &ldim_data_t3;
		ldim_driver.data = &ldim_data_t3;
		break;
	default:
		devp->data = NULL;
		LDIMERR("%s: don't support ldim for current chip\n", __func__);
		return -1;
	}
	ret = ldim_region_num_check(devp->data);
	if (ret)
		return -1;

	ldim_driver.in_vsync_flag = 0;
	ldim_driver.level_update = 0;
	ldim_driver.duty_update_flag = 0;
	ldim_driver.switch_ld_cnt = 0;
	ldim_driver.pq_updating = 0;
	ldim_driver.ld_sel = 1;
	ldim_driver.state |= LDIM_STATE_LD_EN;

	if (!fw) {
		LDIMERR("%s: fw is null\n", __func__);
		return -1;
	}
	ldim_driver.fw = fw;
	ldim_driver.fw->seg_row = ldim_config.seg_row;
	ldim_driver.fw->seg_col = ldim_config.seg_col;
	ldim_driver.fw->valid = 1;

	if (!fw_cus_pre) {
		LDIMERR("%s: fw_cus_pre is null\n", __func__);
	} else {
		LDIMPR("%s: fw_cus_pre is exist\n", __func__);
		ldim_driver.fw_cus_pre = fw_cus_pre;
		ldim_driver.fw_cus_pre->seg_row = ldim_config.seg_row;
		ldim_driver.fw_cus_pre->seg_col = ldim_config.seg_col;
		ldim_driver.fw_cus_pre->valid = 1;
	}

	if (!fw_cus_post) {
		LDIMERR("%s: fw_cus_post is null\n", __func__);
	} else {
		LDIMPR("%s: fw_cus_post is exist\n", __func__);
		ldim_driver.fw_cus_post = fw_cus_post;
		ldim_driver.fw_cus_post->seg_row = ldim_config.seg_row;
		ldim_driver.fw_cus_post->seg_col = ldim_config.seg_col;
		ldim_driver.fw_cus_post->valid = 1;
	}

	if (devp->data->memory_init) {
		ret = devp->data->memory_init(pdev, devp->data,
			ldim_config.seg_row, ldim_config.seg_col);
		if (ret) {
			LDIMERR("%s failed\n", __func__);
			goto err;
		}
		ldim_driver.rmem = &ldim_rmem;
	}

	ret = alloc_chrdev_region(&devp->aml_ldim_devno, 0, 1, AML_LDIM_DEVICE_NAME);
	if (ret < 0) {
		LDIMERR("failed to alloc major number\n");
		ret = -ENODEV;
		goto err;
	}

	devp->aml_ldim_clsp = class_create(THIS_MODULE, "aml_ldim");
	if (IS_ERR(devp->aml_ldim_clsp)) {
		ret = PTR_ERR(devp->aml_ldim_clsp);
		return ret;
	}
	ret = aml_ldim_debug_probe(devp->aml_ldim_clsp);
	if (ret)
		goto err1;

	devp->aml_ldim_cdevp = kmalloc(sizeof(*devp->aml_ldim_cdevp), GFP_KERNEL);
	if (!devp->aml_ldim_cdevp) {
		ret = -ENOMEM;
		goto err2;
	}

	/* connect the file operations with cdev */
	cdev_init(devp->aml_ldim_cdevp, &ldim_fops);
	devp->aml_ldim_cdevp->owner = THIS_MODULE;

	/* connect the major/minor number to the cdev */
	ret = cdev_add(devp->aml_ldim_cdevp, devp->aml_ldim_devno, 1);
	if (ret) {
		LDIMERR("failed to add device\n");
		goto err3;
	}

	devp->dev = device_create(devp->aml_ldim_clsp, NULL,
		devp->aml_ldim_devno, NULL, AML_LDIM_CLASS_NAME);
	if (IS_ERR(devp->dev)) {
		ret = PTR_ERR(devp->dev);
		goto err4;
	}

	ldim_queue = create_workqueue("ldim workqueue");
	if (!ldim_queue) {
		LDIMERR("ldim_queue create failed\n");
		ret = -1;
		goto err4;
	}
	INIT_WORK(&ldim_on_vs_work, ldim_on_update_brightness);
	INIT_WORK(&ldim_off_vs_work, ldim_off_update_brightness);
	INIT_WORK(&ldim_err_handle_work, ldim_err_handler);

	spin_lock_init(&ldim_isr_lock);
	spin_lock_init(&rdma_ldim_isr_lock);
	spin_lock_init(&ldim_pwm_vs_isr_lock);

	bdrv->res_ldim_vsync_irq = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "vsync");
	if (!bdrv->res_ldim_vsync_irq) {
		ret = -ENODEV;
		LDIMERR("ldim_vsync_irq resource error\n");
		goto err4;
	}
	ldim_vsync_irq = bdrv->res_ldim_vsync_irq->start;
	if (request_irq(ldim_vsync_irq, ldim_vsync_isr, IRQF_SHARED,
		"ldim_vsync", (void *)"ldim_vsync")) {
		LDIMERR("can't request ldim_vsync_irq(%d)\n", ldim_vsync_irq);
	}

	bdrv->res_ldim_pwm_vs_irq = platform_get_resource_byname(pdev,
		IORESOURCE_IRQ, "ldim_pwm_vs");
	if (!bdrv->res_ldim_pwm_vs_irq) {
		ret = -ENODEV;
		LDIMERR("dim_pwm_vs_irq resource error\n");
		goto err4;
	}
	ldim_pwm_vs_irq = bdrv->res_ldim_pwm_vs_irq->start;
	if (request_irq(ldim_pwm_vs_irq, ldim_pwm_vs_isr, IRQF_TRIGGER_FALLING,
		"ldim_pwm_vs", (void *)"ldim_pwm_vs")) {
		LDIMERR("can't request ldim_pwm_vs_irq(%d)\n", ldim_pwm_vs_irq);
	}

	ldim_driver.valid_flag = 1;

	if (bdrv->bconf.method != BL_CTRL_LOCAL_DIMMING)
		ldim_dev_add_virtual_driver(&ldim_driver);

	LDIMPR("%s ok\n", __func__);
	return 0;

err4:
	cdev_del(&devp->cdev);
err3:
	kfree(devp->aml_ldim_cdevp);
err2:
	aml_ldim_debug_remove(devp->aml_ldim_clsp);
	class_destroy(devp->aml_ldim_clsp);
err1:
	unregister_chrdev_region(devp->aml_ldim_devno, 1);
err:
	return ret;
}

int aml_ldim_remove(void)
{
	struct ldim_dev_s *devp = &ldim_dev;
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);

	kfree(ldim_driver.bl_matrix_cur);
	kfree(ldim_driver.stts->hist_matrix);
	kfree(ldim_driver.stts->max_rgb);
	kfree(ldim_driver.test_matrix);
	kfree(ldim_driver.local_bl_matrix);

	free_irq(bdrv->res_ldim_vsync_irq->start, (void *)"ldim_vsync");

	cdev_del(devp->aml_ldim_cdevp);
	kfree(devp->aml_ldim_cdevp);
	aml_ldim_debug_remove(devp->aml_ldim_clsp);
	class_destroy(devp->aml_ldim_clsp);
	unregister_chrdev_region(devp->aml_ldim_devno, 1);

	LDIMPR("%s ok\n", __func__);
	return 0;
}

static int __init ldim_buf_device_init(struct reserved_mem *rmem,
				       struct device *dev)
{
	return 0;
}

static const struct reserved_mem_ops ldim_buf_ops = {
	.device_init = ldim_buf_device_init,
};

static int __init ldim_buf_setup(struct reserved_mem *rmem)
{
	ldim_rmem.rsv_mem_paddr = rmem->base;
	ldim_rmem.rsv_mem_size = rmem->size;
	rmem->ops = &ldim_buf_ops;
	LDIMPR("Reserved memory: created buf at 0x%lx, size %ld MiB\n",
	       (unsigned long)rmem->base, (unsigned long)rmem->size / SZ_1M);
	return 0;
}

RESERVEDMEM_OF_DECLARE(buf, "amlogic, ldc-memory", ldim_buf_setup);
