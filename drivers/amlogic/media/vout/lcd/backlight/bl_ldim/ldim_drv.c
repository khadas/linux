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
#include <linux/compat.h>
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

static unsigned char current_switch_cnt;
static struct ldim_dev_s ldim_dev;
static struct aml_ldim_driver_s ldim_driver;

static spinlock_t ldim_isr_lock;
static spinlock_t rdma_ldim_isr_lock;
static spinlock_t ldim_pwm_vs_isr_lock;

static int ldim_on_init(void);
static int ldim_power_on(void);
static int ldim_power_off(void);
static int ldim_set_level(unsigned int level);
static void ldim_test_ctrl(int flag);
static void ldim_ld_sel_ctrl(int flag);
static void ldim_pwm_vs_update(void);
static void ldim_config_print(void);

static struct ldim_config_s ldim_config = {
	.hsize = 3840,
	.vsize = 2160,
	.seg_row = 1,
	.seg_col = 1,
	.bl_mode = 1,
	.dev_index = 0xff,
};

static struct ldim_rmem_s ldim_rmem = {
	.flag = 0,
	.rsv_mem_paddr = 0,
	.rsv_mem_size = 0x100000,
};

static struct aml_ldim_driver_s ldim_driver = {
	.valid_flag = 0, /* default invalid, active when bl_ctrl_method=ldim */
	.static_pic_flag = 0,
	.vsync_change_flag = 0,
	.duty_update_flag = 0,
	.switch_ld_cnt = 1,
	.in_vsync_flag = 0,

	.init_on_flag = 0,
	.func_en = 0,
	.level_idx = 0,
	.demo_mode = 0,
	.black_frm_en = 0,
	.ld_sel = 1,
	.func_bypass = 0,
	.dev_smr_bypass = 0,
	.brightness_bypass = 0,
	.test_bl_en = 0,
	.load_db_en = 1,
	.level_update = 0,
	.resolution_update = 0,

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

	.fw = NULL,
	.cus_fw = NULL,

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

static int ldim_on_init(void)
{
	if (ldim_debug_print)
		LDIMPR("%s\n", __func__);

	ldim_driver.init_on_flag = 1;
	ldim_driver.level_update = 1;
	ldim_driver.state |= LDIM_STATE_POWER_ON;

	return 0;
}

static int ldim_power_on(void)
{
	if (ldim_driver.init_on_flag) {
		LDIMPR("%s: already power on, exit\n", __func__);
		return -1;
	}

	LDIMPR("%s\n", __func__);

	ldim_driver.init_on_flag = 1;

	if (ldim_driver.dev_drv && ldim_driver.dev_drv->power_on)
		ldim_driver.dev_drv->power_on(&ldim_driver);

	ldim_driver.level_update = 1;
	ldim_driver.state |= LDIM_STATE_POWER_ON;

	return 0;
}

static int ldim_power_off(void)
{
	if (ldim_driver.init_on_flag == 0) {
		LDIMPR("%s: already power off, exit\n", __func__);
		return -1;
	}
	LDIMPR("%s\n", __func__);

	ldim_driver.state &= ~LDIM_STATE_POWER_ON;
	ldim_driver.init_on_flag = 0;
	if (ldim_driver.dev_drv && ldim_driver.dev_drv->power_off)
		ldim_driver.dev_drv->power_off(&ldim_driver);

	return 0;
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

static void ldim_fw_vsync_update(void)
{
	struct ldim_fw_s *fw = ldim_driver.fw;

	if (!fw)
		return;
	if (!fw->conf)
		return;

	if (fw->conf->func_en != ldim_driver.func_en ||
		fw->conf->hsize != ldim_driver.conf->hsize ||
		fw->conf->vsize != ldim_driver.conf->vsize ||
		fw->conf->level_index != ldim_driver.level_idx ||
		fw->res_update != ldim_driver.resolution_update) {
		if (ldim_driver.func_en == 0 && fw->conf->func_en == 1)
			current_switch_cnt = ldim_driver.switch_ld_cnt;

		fw->conf->func_en = ldim_driver.func_en;
		fw->conf->hsize = ldim_driver.conf->hsize;
		fw->conf->vsize = ldim_driver.conf->vsize;
		fw->conf->level_index = ldim_driver.level_idx;
		fw->res_update = ldim_driver.resolution_update;

		if (fw->fw_info_update)
			fw->fw_info_update(ldim_driver.fw);
	}
}

void ldim_vs_arithmetic(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int size;
	struct ldim_fw_s *fw = ldim_drv->fw;
	struct ldim_fw_custom_s *cus_fw = ldim_drv->cus_fw;

	if (!fw)
		return;
	if (!fw->stts)
		return;
	if (!fw->bl_matrix)
		return;

	size = ldim_drv->conf->seg_row * ldim_drv->conf->seg_col;

	if (fw->fw_alg_frm)
		fw->fw_alg_frm(fw);
	/*fw_sel: 0:hw, 1:aml sw, 2: aml sw + cus sw*/
	if (fw->fw_sel == 0) {
		if (fw->fw_rmem_duty_get)
			fw->fw_rmem_duty_get(fw);
		memcpy(ldim_drv->local_bl_matrix, fw->bl_matrix,
		       size * (sizeof(unsigned int)));
	} else {
		if (fw->fw_sel == 1) {
			memcpy(ldim_drv->local_bl_matrix, fw->bl_matrix,
		       size * (sizeof(unsigned int)));
		} else {
			memcpy(cus_fw->bl_matrix, fw->bl_matrix, size * (sizeof(unsigned int)));
			if (cus_fw->fw_alg_frm)
				cus_fw->fw_alg_frm(cus_fw, fw->stts);
			if (fw->fw_rmem_duty_set)
				fw->fw_rmem_duty_set(cus_fw->bl_matrix);
			memcpy(ldim_drv->local_bl_matrix, cus_fw->bl_matrix,
			size * (sizeof(unsigned int)));
		}
	}
}

/* ******************************************************
 * local dimming function
 * ******************************************************
 */
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

	if (ldim_driver.valid_flag == 0)
		return IRQ_HANDLED;

	if (ldim_driver.init_on_flag == 0)
		return IRQ_HANDLED;

	atomic_set(&ldim_inirq_flag, 1);

	spin_lock_irqsave(&ldim_isr_lock, flags);

	ldim_driver.in_vsync_flag = 1;

	if (ldim_driver.dbg_vs_cnt++ >= 300) /* for debug print */
		ldim_driver.dbg_vs_cnt = 0;

	ldim_fw_vsync_update();

	local_time[0] = sched_clock();
	if (ldim_dev.data->vs_arithmetic)
		ldim_dev.data->vs_arithmetic(&ldim_driver);
	local_time[1] = sched_clock();
	local_time[2] = local_time[1] - local_time[0];
	ldim_time_sort_save(ldim_driver.arithmetic_time, local_time[2]);

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

	if (ldim_driver.valid_flag == 0)
		return IRQ_HANDLED;

	if (ldim_driver.init_on_flag == 0)
		return IRQ_HANDLED;

	ldim_driver.pwm_vs_irq_cnt = ldim_driver.irq_cnt;

	spin_lock_irqsave(&ldim_pwm_vs_isr_lock, flags);

	if (ldim_debug_print == 7)
		LDIMPR("%s: pwm_vs_irq_cnt = %d\n", __func__, ldim_driver.pwm_vs_irq_cnt);

	if ((ldim_driver.func_en && ldim_driver.ld_sel) ||
		current_switch_cnt) {
		ldim_on_vs_brightness();
		current_switch_cnt--;

	} else {
		ldim_off_vs_brightness();
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

	ldim_dev_err_handler();
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
		if (ldim_driver.black_frm_en) {
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
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);

	if (bdrv->bconf.method != BL_CTRL_LOCAL_DIMMING) {
		LDIMERR("%s: bconf.method is not ldim!!\n", __func__);
		return -1;
	}

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

char aml_ldim_get_bbd_state(void)
{
	char ret = ldim_driver.fw->fw_state & 0x01;
	return ret;
}
EXPORT_SYMBOL(aml_ldim_get_bbd_state);


static long ldim_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int i = 0;
	int ret = 0;
	void __user *argp;
	int mcd_nr;
	unsigned char *buf;
	struct aml_ldim_bin_s ldim_buff;
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);
	unsigned int temp = 0;

	mcd_nr = _IOC_NR(cmd);
	LDIMPR("%s: cmd_dir = 0x%x, cmd_nr = 0x%x\n",
	       __func__, _IOC_DIR(cmd), mcd_nr);

	if (bdrv->bconf.method != BL_CTRL_LOCAL_DIMMING) {
		LDIMERR("%s: bconf.method is not ldim!!\n", __func__);
		return -1;
	}

	if (ldim_driver.dev_drv->init_loaded == 0) {
		LDIMERR("%s: dev_drv->init_loaded == 0!!\n", __func__);
		return -1;
	}

	argp = (void __user *)arg;
	switch (mcd_nr) {
	case AML_LDIM_IOC_NR_SET_PQ_INIT:
		if (copy_from_user(&ldim_buff,
				   (void __user *)arg,
				   sizeof(struct aml_ldim_bin_s))) {
			LDIMERR("cp pq_init.bin fail\n");
			return -EFAULT;
		}
		argp = (void __user *)ldim_buff.ptr;
		LDIMPR("index =  0x%x, len=  0x%x\n", ldim_buff.index, ldim_buff.len);
		//ldim_buff.len must > sizeof(fw_pqdata_s)
		if (ldim_buff.len == 0 || ldim_buff.len > 0x8000) {
			LDIMERR("ldim buf len = %d is invalid!!\n", ldim_buff.len);
			return -EFAULT;
		}
		buf = vmalloc(ldim_buff.len);
		if (!buf) {
			LDIMERR("%s vmalloc buf for receive pq_init.bin failed\n", __func__);
			vfree(buf);
			return -EFAULT;
		}
		if (copy_from_user((void *)buf, argp, ldim_buff.len)) {
			LDIMERR("cp pq_init.bin to buf fail\n");
			vfree(buf);
			return -EFAULT;
		}

		if (ldim_driver.fw->fw_pq_set)
			ldim_driver.fw->fw_pq_set(buf, ldim_buff.len);
		vfree(buf);
		break;
	case AML_LDIM_IOC_NR_GET_LEVEL_IDX:
		if (copy_to_user(argp, &ldim_driver.level_idx, sizeof(unsigned char)))
			ret = -EFAULT;
		break;
	case AML_LDIM_IOC_NR_SET_LEVEL_IDX:
		if (copy_from_user(&ldim_driver.level_idx, argp,
				   sizeof(unsigned char))) {
			ret = -EFAULT;
		}
		if (ldim_driver.level_idx)
			ldim_driver.func_en = 1;
		else
			ldim_driver.func_en = 0;
		LDIMPR("%s ldim_driver.level_idx=%d\n", __func__, ldim_driver.level_idx);
		break;
	case AML_LDIM_IOC_NR_GET_FUNC_EN:
		if (copy_to_user(argp, &ldim_driver.func_en, sizeof(unsigned char)))
			ret = -EFAULT;
		break;
	case AML_LDIM_IOC_NR_SET_FUNC_EN:
		if (copy_from_user(&ldim_driver.func_en, argp,
				   sizeof(unsigned char))) {
			ret = -EFAULT;
		}
		LDIMPR("%s ldim_driver.func_en=%d!\n", __func__, ldim_driver.func_en);
		break;
	case AML_LDIM_IOC_NR_GET_REMAP_EN:
		if (copy_to_user(argp, &ldim_driver.fw->conf->remap_en, sizeof(unsigned char)))
			ret = -EFAULT;
		break;
	case AML_LDIM_IOC_NR_SET_REMAP_EN:
		if (copy_from_user(&ldim_driver.fw->conf->remap_en, argp,
				   sizeof(unsigned char))) {
			ret = -EFAULT;
		}
		LDIMPR("%s remap_en=%d!\n", __func__, ldim_driver.fw->conf->remap_en);
		break;
	case AML_LDIM_IOC_NR_GET_DEMOMODE:
		if (copy_to_user(argp, &ldim_driver.demo_mode, sizeof(unsigned char)))
			ret = -EFAULT;
		break;
	case AML_LDIM_IOC_NR_SET_DEMOMODE:
		if (copy_from_user(&ldim_driver.demo_mode, argp,
				   sizeof(unsigned char))) {
			ret = -EFAULT;
		}
		LDIMPR("%s ldim_driver.demo_mode=%d!\n", __func__, ldim_driver.demo_mode);
		break;
	case AML_LDIM_IOC_NR_GET_ZONENUM:
		temp = (ldim_config.seg_col << 8) | ldim_config.seg_row;
		if (copy_to_user(argp, &temp, sizeof(unsigned int)))
			ret = -EFAULT;
		break;
	case AML_LDIM_IOC_NR_GET_BL_MATRIX:
		temp = ldim_config.seg_col * ldim_config.seg_row;
		if (copy_to_user(argp, ldim_driver.bl_matrix_cur, temp * sizeof(unsigned int)))
			ret = -EFAULT;
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
			LDIMERR("cp bl_mapping.bin fail\n");
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
			ldim_driver.fw->profile->file_path);
		if (copy_to_user(argp, ldim_driver.fw->profile->file_path,
				 0xff)) {
			ret = -EFAULT;
		}
		break;
	case AML_LDIM_IOC_NR_SET_BL_PROFILE:
		if (copy_from_user(&ldim_buff,
				   (void __user *)arg,
				   sizeof(struct aml_ldim_bin_s))) {
			LDIMERR("cp profile.bin fail\n");
			return -EFAULT;
		}
		argp = (void __user *)ldim_buff.ptr;
		LDIMPR("index =  0x%x, len=  0x%x\n", ldim_buff.index, ldim_buff.len);
		if (ldim_buff.len == 0 || ldim_buff.len > 0x24c080) {
			LDIMERR("profile bin size = %d is invalid!!\n", ldim_buff.len);
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
		if (ldim_driver.fw->fw_profile_set)
			ldim_driver.fw->fw_profile_set(buf, ldim_buff.len);
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
	ldim_config.seg_row = para[0];
	ldim_config.seg_col = para[1];
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

void aml_ldim_rmem_info(void)
{
	pr_info("reserved mem info:\n"
		"mem_paddr = 0x%lx\n"
		"mem_vaddr = 0x%px\n"
		"mem_size  = 0x%x\n"
		"mem_flag  = %d\n\n",
		(unsigned long)ldim_rmem.rsv_mem_paddr,
		ldim_rmem.rsv_mem_vaddr,
		ldim_rmem.rsv_mem_size, ldim_rmem.flag);
}

static int aml_ldim_malloc(struct platform_device *pdev, struct ldim_drv_data_s *data,
			      unsigned int row, unsigned int col)
{
	unsigned int zone_num = row * col;
	unsigned int mem_size;
	int i, ret = 0;

	/* init reserved memory */
	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret)
		ldim_rmem.flag = 2; //system dma pool
	else
		ldim_rmem.flag = 1; //ldim own cma pool
	mem_size = ldim_rmem.rsv_mem_size;
	ldim_rmem.rsv_mem_vaddr = dma_alloc_coherent(&pdev->dev, mem_size,
				&ldim_rmem.rsv_mem_paddr, GFP_KERNEL);
	if (!ldim_rmem.rsv_mem_vaddr) {
		ldim_rmem.flag = 0;
		LDIMERR("ldc resv mem failed\n");
		return -1;
	}
	LDIMPR("ldc rsv_mem paddr: 0x%lx, vaddr: 0x%px, size: 0x%x, flag: %d\n",
	       (unsigned long)ldim_rmem.rsv_mem_paddr,
	       ldim_rmem.rsv_mem_vaddr,
	       ldim_rmem.rsv_mem_size, ldim_rmem.flag);

	ldim_driver.local_bl_matrix = kcalloc(zone_num, sizeof(unsigned int),
					      GFP_KERNEL);
	if (!ldim_driver.local_bl_matrix)
		goto ldim_malloc_t7_err0;

	ldim_driver.bl_matrix_cur = kcalloc(zone_num, sizeof(unsigned int),
					    GFP_KERNEL);
	if (!ldim_driver.bl_matrix_cur)
		goto ldim_malloc_t7_err1;

	ldim_driver.bl_matrix_pre = kcalloc(zone_num, sizeof(unsigned int),
					    GFP_KERNEL);
	if (!ldim_driver.bl_matrix_pre)
		goto ldim_malloc_t7_err2;

	ldim_driver.test_matrix = kcalloc(zone_num, sizeof(unsigned int),
					  GFP_KERNEL);
	if (!ldim_driver.test_matrix)
		goto ldim_malloc_t7_err3;
	for (i = 0; i < zone_num; i++)
		ldim_driver.test_matrix[i] = 2048;

	return 0;

ldim_malloc_t7_err3:
	kfree(ldim_driver.bl_matrix_pre);
ldim_malloc_t7_err2:
	kfree(ldim_driver.bl_matrix_cur);
ldim_malloc_t7_err1:
	kfree(ldim_driver.local_bl_matrix);
ldim_malloc_t7_err0:
	LDIMERR("%s failed\n", __func__);
	return -1;
}

static struct ldim_drv_data_s ldim_data_t7 = {
	.ldc_chip_type = 0,
	.spi_sync = 0,
	.rsv_mem_size = 0x100000,
	.h_zone_max = 48,
	.v_zone_max = 32,
	.total_zone_max = 1536,

	.vs_arithmetic = ldim_vs_arithmetic,
	.memory_init = aml_ldim_malloc,
	.drv_init = NULL,
	.func_ctrl = NULL,
};

static struct ldim_drv_data_s ldim_data_t3 = {
	.ldc_chip_type = 1,
	.spi_sync = 0,
	.rsv_mem_size = 0x100000,
	.h_zone_max = 48,
	.v_zone_max = 32,
	.total_zone_max = 1536,

	.vs_arithmetic = ldim_vs_arithmetic,
	.memory_init = aml_ldim_malloc,
	.drv_init = NULL,
	.func_ctrl = NULL,
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
	int ldim_vsync_irq = 0;
	int ldim_pwm_vs_irq = 0;
	struct ldim_dev_s *devp = &ldim_dev;
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);
	struct ldim_fw_s *fw = aml_ldim_get_fw();
	struct ldim_fw_custom_s *fw_cus = aml_ldim_get_fw_cus();

	if (!bdrv)
		return -1;

	memset(devp, 0, (sizeof(struct ldim_dev_s)));

#ifdef LDIM_DEBUG_INFO
	ldim_debug_print = 1;
#endif

	switch (bdrv->data->chip_type) {
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

	ldim_rmem.rsv_mem_size = ldim_driver.data->rsv_mem_size;
	ldim_driver.resolution_update = 0;
	ldim_driver.in_vsync_flag = 0;
	ldim_driver.level_update = 0;
	ldim_driver.duty_update_flag = 0;
	ldim_driver.ld_sel = 1;
	ldim_driver.state |= LDIM_STATE_LD_EN;
	current_switch_cnt = 0;

	if (devp->data->memory_init) {
		ret = devp->data->memory_init(pdev, devp->data,
			ldim_config.seg_row, ldim_config.seg_col);
		if (ret) {
			LDIMERR("%s failed\n", __func__);
			goto err;
		}
	}

	if (!fw) {
		LDIMERR("%s: fw is null\n", __func__);
		return -1;
	}
	ldim_driver.fw = fw;
	ldim_driver.fw->chip_type = devp->data->ldc_chip_type;
	ldim_driver.fw->seg_row = ldim_config.seg_row;
	ldim_driver.fw->seg_col = ldim_config.seg_col;
	ldim_driver.fw->rmem = &ldim_rmem;
	ldim_driver.fw->valid = 1;

	if (!fw_cus) {
		LDIMPR("%s: fw_cus is null\n", __func__);
	} else {
		LDIMPR("%s: fw_cus is exist\n", __func__);
		ldim_driver.cus_fw = fw_cus;
		ldim_driver.cus_fw->seg_row = ldim_config.seg_row;
		ldim_driver.cus_fw->seg_col = ldim_config.seg_col;
		ldim_driver.cus_fw->valid = 1;
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

	spin_lock_init(&ldim_isr_lock);
	spin_lock_init(&rdma_ldim_isr_lock);
	spin_lock_init(&ldim_pwm_vs_isr_lock);

	ldim_vsync_irq = platform_get_irq_byname(pdev, "vsync");
	if (ldim_vsync_irq == -ENXIO) {
		ret = -ENODEV;
		LDIMERR("ldim_vsync_irq resource error\n");
		goto err4;
	}
	LDIMPR("ldim_vsync_irq: %d\n", ldim_vsync_irq);
	if (request_irq(ldim_vsync_irq, ldim_vsync_isr, IRQF_SHARED,
		"ldim_vsync", (void *)"ldim_vsync")) {
		LDIMERR("can't request ldim_vsync_irq(%d)\n", ldim_vsync_irq);
	}

	ldim_pwm_vs_irq = platform_get_irq_byname(pdev, "ldim_pwm_vs");
	if (ldim_pwm_vs_irq == -ENXIO) {
		ret = -ENODEV;
		LDIMERR("ldim_pwm_vs_irq resource error\n");
		goto err4;
	}
	LDIMPR("ldim_pwm_vs_irq: %d\n", ldim_pwm_vs_irq);
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
