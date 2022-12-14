// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/atomic.h>
#include <linux/of.h>
#include <linux/compat.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/reboot.h>
#include <linux/notifier.h>
#include <linux/random.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_wakeup.h>
#include <linux/pm_wakeirq.h>
#include <linux/pm.h>
#include <linux/poll.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include <linux/amlogic/pm.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/iomap.h>

#include "adc.h"

#define ADC_DEV_NAME		"adc"

static struct tvin_adc_dev *adc_devp;
static bool probe_finish;

static int adc_wr_afe(unsigned int reg, unsigned int val)
{
	if (!probe_finish || !adc_devp)
		return -1;

	writel(val, (adc_devp->vir_addr[MAP_ADDR_AFE] + (reg << 2)));
	return 0;
}

static unsigned int adc_rd_afe(unsigned int reg)
{
	if (!probe_finish || !adc_devp)
		return -1;

	return readl(adc_devp->vir_addr[MAP_ADDR_AFE] + (reg << 2));
}

static int adc_wr_hiu(unsigned int reg, unsigned int val)
{
	if (!probe_finish || !adc_devp)
		return -1;

	writel(val, (adc_devp->vir_addr[MAP_ADDR_HIU] + (reg << 2)));
	return 0;
}

static unsigned int adc_rd_hiu(unsigned int reg)
{
	unsigned int tmp;

	if (!probe_finish || !adc_devp)
		return -1;

	tmp = readl(adc_devp->vir_addr[MAP_ADDR_HIU] + (reg << 2));

	return tmp;
}

static unsigned int adc_rd_hiu_bits(u32 reg, const u32 start, const u32 len)
{
	u32 val;

	val = ((adc_rd_hiu(reg) >> (start)) & ((1L << (len)) - 1));

	return val;
}

static void adc_wr_hiu_bits(u32 reg, const u32 value,
			    const u32 start, const u32 len)
{
	adc_wr_hiu(reg, ((adc_rd_hiu(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

/* add for dtv demod */
void adc_set_ddemod_default(enum fe_delivery_system delsys)
{
	struct tvin_adc_dev *devp = adc_devp;
	struct adc_reg_addr *adc_addr = &devp->plat_data->adc_addr;

	if (!probe_finish)
		return;

	if (is_meson_txl_cpu() || is_meson_txlx_cpu() ||
	    is_meson_gxlx_cpu()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		adc_wr_hiu(HHI_DADC_CNTL, 0x00102038);
		adc_wr_hiu(HHI_DADC_CNTL2, 0x00000406);
		adc_wr_hiu(HHI_DADC_CNTL3, 0x00082183);

		/*W_HIU_REG(HHI_VDAC_CNTL0, 0x00000200);*/
		/*W_HIU_BIT(HHI_VDAC_CNTL0, 1, 9, 1);*/
		/* remove vdac reg write, make sure it write in vdac driver,
		 * because multi module use it
		 */
#endif
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		switch (devp->plat_data->chip_id) {
		case ADC_CHIP_T5D:
		case ADC_CHIP_T3:
		case ADC_CHIP_T5W:
			switch (delsys) {
			case SYS_DVBT:
			case SYS_DVBT2:
				adc_wr_afe(AFE_VAFE_CTRL0, 0x00050710);
				adc_wr_afe(AFE_VAFE_CTRL1, 0x3000);
				adc_wr_afe(AFE_VAFE_CTRL2, 0x1fe09e31);
				adc_wr_hiu(adc_addr->dadc_cntl, 0x0030303c);

				if (devp->plat_data->chip_id >= ADC_CHIP_T3)
					adc_wr_hiu(adc_addr->dadc_cntl_2, 0x00003448);
				else
					adc_wr_hiu(adc_addr->dadc_cntl_2, 0x00003440);

				/* bit[1:0] adc en/dis, 00:dis, 11:en */
				adc_wr_hiu(adc_addr->dadc_cntl_3, 0x08300b93);
				adc_wr_hiu(adc_addr->dadc_cntl_4, 0xc00);
				break;

			case SYS_DVBS:
			case SYS_DVBS2:
				adc_wr_hiu(adc_addr->s2_dadc_cntl, 0x4100900f);
				adc_wr_hiu(adc_addr->dadc_cntl_4, 0x0);

				if (devp->plat_data->chip_id >= ADC_CHIP_T3) {
					adc_wr_hiu(adc_addr->dadc_cntl_2, 0x00000008);
					adc_wr_hiu(adc_addr->s2_dadc_cntl, 0x41209007);
				}
				/* only for debug to clkmsr(id:149) */
				/* adc_wr_hiu(adc_addr->dadc_cntl_3, 0x08300b8f); */
				break;

			default:
				adc_wr_afe(AFE_VAFE_CTRL0, 0x000d0710);
				adc_wr_afe(AFE_VAFE_CTRL1, 0x3000);
				adc_wr_afe(AFE_VAFE_CTRL2, 0x1fe09e31);
				adc_wr_hiu(adc_addr->dadc_cntl, 0x00303044);
				if (devp->plat_data->chip_id >= ADC_CHIP_T3)
					adc_wr_hiu(adc_addr->dadc_cntl_2, 0x00003488);
				else
					adc_wr_hiu(adc_addr->dadc_cntl_2, 0x00003480);
				adc_wr_hiu(adc_addr->dadc_cntl_3, 0x08300b83);
				adc_wr_hiu(adc_addr->dadc_cntl_4, 0x0);
				break;
			}

			adc_wr_hiu(adc_addr->s2_dadc_cntl_2, 0x00000582);
			break;

		case ADC_CHIP_S4:
		case ADC_CHIP_S4D:
			/* enable bandgap */
			adc_wr_hiu_bits(0xb0, 1, 11, 1);
			adc_wr_hiu(0xd1, 0x562);
			adc_wr_hiu(0xd0, 0x41209007);
			break;

		default:
			adc_wr_afe(AFE_VAFE_CTRL0, 0x000d0710);
			adc_wr_afe(AFE_VAFE_CTRL1, 0x3000);
			adc_wr_afe(AFE_VAFE_CTRL2, 0x1fe09e31);
			adc_wr_hiu(adc_addr->dadc_cntl, 0x00303044);
			adc_wr_hiu(adc_addr->dadc_cntl_2, 0x00003480);
			adc_wr_hiu(adc_addr->dadc_cntl_3, 0x08300b83);
			break;
		}
	}
}
EXPORT_SYMBOL(adc_set_ddemod_default);

int adc_set_filter_ctrl(bool on, enum filter_sel module_sel, void *data)
{
	int ret = 0;
	struct tvin_adc_dev *devp = adc_devp;

	if (!probe_finish || !devp)
		return -ENXIO;

	mutex_lock(&devp->filter_mutex);

	if (!on) {
		devp->filter_flg &= ~module_sel;
		mutex_unlock(&devp->filter_mutex);
		if (devp->print_en)
			pr_info("%s:reon:%d, module:0x%x, filter_flg:0x%x\n",
				__func__, on, module_sel, devp->filter_flg);
		return 0;
	}

	switch (module_sel) {
	case FILTER_ATV_DEMOD:
		if (devp->filter_flg & (FILTER_TVAFE | FILTER_DTV_DEMOD)) {
			ret = -1;

			pr_info("%s: DEMOD fail!!!, filter_flg: %d\n",
					__func__, devp->filter_flg);
			break;
		}

		if (devp->filter_flg & FILTER_ATV_DEMOD) {
			if (devp->print_en)
				pr_info("%s: DEMOD ATV had done!: %d\n",
						__func__, devp->filter_flg);
			break;
		}

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			adc_wr_afe(AFE_VAFE_CTRL0, 0x000d0710);
			adc_wr_afe(AFE_VAFE_CTRL1, 0x3000);
			adc_wr_afe(AFE_VAFE_CTRL2, 0x1fe09e31);
		} else if (get_cpu_type() == MESON_CPU_MAJOR_ID_TXL ||
			get_cpu_type() == MESON_CPU_MAJOR_ID_TXLX) {
			adc_wr_afe(AFE_VAFE_CTRL0, 0x00090b00);
			adc_wr_afe(AFE_VAFE_CTRL1, 0x00000110);
			adc_wr_afe(AFE_VAFE_CTRL2, 0x0010ef93);
		}

		devp->filter_flg |= FILTER_ATV_DEMOD;
		break;

	case FILTER_TVAFE:
		if (devp->filter_flg & (FILTER_ATV_DEMOD | FILTER_DTV_DEMOD)) {
			ret = -2;

			pr_info("%s: AFE fail!!!, filter_flg: %d\n",
					__func__, devp->filter_flg);
			break;
		}

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			adc_wr_afe(AFE_VAFE_CTRL0, 0x00490710);
			adc_wr_afe(AFE_VAFE_CTRL1, 0x0000110e);
			adc_wr_afe(AFE_VAFE_CTRL2, 0x1fe09f83);
		} else if (get_cpu_type() == MESON_CPU_MAJOR_ID_TXL ||
			get_cpu_type() == MESON_CPU_MAJOR_ID_TXLX) {
			adc_wr_afe(AFE_VAFE_CTRL0, 0x00090b00);
			adc_wr_afe(AFE_VAFE_CTRL1, 0x00000110);
			adc_wr_afe(AFE_VAFE_CTRL2, 0x0010ef93);
		}

		devp->filter_flg |= FILTER_TVAFE;
		break;

	case FILTER_DTV_DEMOD:
		if (devp->filter_flg & (FILTER_ATV_DEMOD | FILTER_TVAFE)) {
			ret = -3;

			pr_info("%s: DDEMOD fail!!!, filter_flg: %d\n",
					__func__, devp->filter_flg);
			break;
		}

		devp->filter_flg |= FILTER_DTV_DEMOD;
		break;

	case FILTER_DTV_DEMODT2:
		if (devp->filter_flg & (FILTER_ATV_DEMOD | FILTER_TVAFE)) {
			ret = -4;

			pr_info("%s:DMODPLL fail!!!, filter_flg: %d\n",
					__func__, devp->filter_flg);
			break;
		}

		devp->filter_flg |= FILTER_DTV_DEMODT2;
		break;

	default:
		if (devp->print_en)
			pr_err("%s: module: 0x%x wrong module index!!!",
					__func__, module_sel);
		break;
	}
	if (devp->print_en)
		pr_info("%s: on:%d, module:0x%x, filter_flg:0x%x\n",
			__func__, on, module_sel, devp->filter_flg);
	mutex_unlock(&devp->filter_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(adc_set_filter_ctrl);

static void adc_set_dtvdemod_pll_by_clk(struct tvin_adc_dev *devp,
					struct dfe_adcpll_para *p_dtv_para)
{
	unsigned int reg_offset = 0;
	enum adc_chip_ver chip = devp->plat_data->chip_id;
	struct adc_reg_addr *adc_addr = &devp->plat_data->adc_addr;
	struct adc_pll_reg_addr *pll_addr = &devp->plat_data->pll_addr;

	if (chip == ADC_CHIP_S4 || chip == ADC_CHIP_S4D)
		reg_offset = 0x10;

	switch (p_dtv_para->adc_clk) {
	case ADC_CLK_24M:
		if (chip == ADC_CHIP_S4 || chip == ADC_CHIP_S4D) {
			adc_wr_hiu(pll_addr->adc_pll_cntl_0 + reg_offset, 0x0105047d);
			//adc_wr_hiu(HHI_ADC_PLL_CNTL0_TL1 + reg_offset, 0x312004e0);
			adc_wr_hiu(pll_addr->adc_pll_cntl_1 + reg_offset, 0x03c00000);

			/* enable bandgap */
			adc_wr_hiu_bits(adc_addr->vdac_cntl_0, 1, 11, 1);
			adc_wr_hiu(pll_addr->adc_pll_cntl_2 + reg_offset, 0x01800000);
			adc_wr_hiu(pll_addr->adc_pll_cntl_3 + reg_offset, 0x08691d00);
			adc_wr_hiu(pll_addr->adc_pll_cntl_4 + reg_offset, 0x33771291);
			adc_wr_hiu(pll_addr->adc_pll_cntl_5 + reg_offset, 0x39270000);

			adc_wr_hiu(pll_addr->adc_pll_cntl_6 + reg_offset, 0x50540000);
			adc_wr_hiu(pll_addr->adc_pll_cntl_0 + reg_offset, 0x1105047d);
		} else {
			adc_wr_hiu(pll_addr->adc_pll_cntl_0 + reg_offset, 0x012004e0);
			adc_wr_hiu(pll_addr->adc_pll_cntl_0 + reg_offset, 0x312004e0);
			adc_wr_hiu(pll_addr->adc_pll_cntl_1 + reg_offset, 0x05400000);

			/* enable bandgap */
			if (chip >= ADC_CHIP_T5)
				adc_wr_hiu_bits(adc_addr->vdac_cntl_0, 1, 11, 1);

			if (chip >= ADC_CHIP_T5D)
				adc_wr_hiu(pll_addr->adc_pll_cntl_2 + reg_offset, 0xe1800000);
			else
				adc_wr_hiu(pll_addr->adc_pll_cntl_2, 0xe0800000);

			adc_wr_hiu(pll_addr->adc_pll_cntl_3 + reg_offset, 0x48681c00);
			adc_wr_hiu(pll_addr->adc_pll_cntl_4 + reg_offset, 0x88770290);

			//if (chip >= ADC_CHIP_T5D)
			//adc_wr_hiu_bits(pll_addr->adc_pll_cntl_5 + reg_offset, 1, 0, 16);
			//else
			if (chip == ADC_CHIP_T5W)
				adc_wr_hiu(pll_addr->adc_pll_cntl_5, 0x3927a00a);
			else
				adc_wr_hiu(pll_addr->adc_pll_cntl_5, 0x3927a000);

			adc_wr_hiu(pll_addr->adc_pll_cntl_6 + reg_offset, 0x56540000);
			adc_wr_hiu(pll_addr->adc_pll_cntl_0 + reg_offset, 0x111104e0);
		}
		break;

	case ADC_CLK_25M:
		adc_wr_hiu(pll_addr->adc_pll_cntl_0 + reg_offset, 0x001104c8);
		adc_wr_hiu(pll_addr->adc_pll_cntl_0 + reg_offset, 0x301104c8);
		adc_wr_hiu(pll_addr->adc_pll_cntl_1 + reg_offset, 0x03000000);
		adc_wr_hiu(pll_addr->adc_pll_cntl_2 + reg_offset, 0xe1800000);
		adc_wr_hiu(pll_addr->adc_pll_cntl_3 + reg_offset, 0x48681c00);
		adc_wr_hiu(pll_addr->adc_pll_cntl_4 + reg_offset, 0x88770290);
		adc_wr_hiu(pll_addr->adc_pll_cntl_6 + reg_offset, 0x56540000);
		adc_wr_hiu(pll_addr->adc_pll_cntl_0 + reg_offset, 0x101104c8);
		break;

	default:
		break;
	}
}

static void adc_set_dtvdemod_pll_by_delsys(struct tvin_adc_dev *devp,
						struct dfe_adcpll_para *p_dtv_para)
{
	struct adc_pll_reg_addr *pll_addr = &devp->plat_data->pll_addr;
	enum adc_chip_ver chip = devp->plat_data->chip_id;
	unsigned int reg_offset = 0;

	if (chip == ADC_CHIP_S4 || chip == ADC_CHIP_S4D)
		reg_offset = 0x10;

	switch (p_dtv_para->delsys) {
	case SYS_DVBT:
	case SYS_DVBT2:
		adc_wr_hiu(pll_addr->adc_pll_cntl_0, 0x200504b4);
		adc_wr_hiu(pll_addr->adc_pll_cntl_0, 0x300504b4);
		adc_wr_hiu(pll_addr->adc_pll_cntl_1, 0x01400000);
		adc_wr_hiu(pll_addr->adc_pll_cntl_2, 0x0);
		adc_wr_hiu(pll_addr->adc_pll_cntl_3, 0x48681f00);
		adc_wr_hiu(pll_addr->adc_pll_cntl_4, 0x88770290);
		adc_wr_hiu(pll_addr->adc_pll_cntl_5, 0x3927a000);
		adc_wr_hiu(pll_addr->adc_pll_cntl_6, 0x56540000);
		adc_wr_hiu(pll_addr->adc_pll_cntl_0, 0x100504b4);
		break;

	case SYS_DVBS:
	case SYS_DVBS2:
		if (chip == ADC_CHIP_S4D) {
			if (p_dtv_para->pga_gain == 2)
				adc_wr_hiu(0xd0, 0x49209007);
			else if (p_dtv_para->pga_gain == 1)
				adc_wr_hiu(0xd0, 0x45209007);
			else
				adc_wr_hiu(0xd0, 0x41209007);

			adc_wr_hiu(pll_addr->adc_pll_cntl_0 + reg_offset, 0x20070487);
			adc_wr_hiu(pll_addr->adc_pll_cntl_0 + reg_offset, 0x30070487);
			adc_wr_hiu(pll_addr->adc_pll_cntl_1 + reg_offset, 0x01000000);
			adc_wr_hiu(pll_addr->adc_pll_cntl_2 + reg_offset, 0x0);
			adc_wr_hiu(pll_addr->adc_pll_cntl_3 + reg_offset, 0x48681f00);
			adc_wr_hiu(pll_addr->adc_pll_cntl_4 + reg_offset, 0x88770290);
			adc_wr_hiu(pll_addr->adc_pll_cntl_5 + reg_offset, 0x3927a000);
			adc_wr_hiu(pll_addr->adc_pll_cntl_6 + reg_offset, 0x56540000);
			adc_wr_hiu(pll_addr->adc_pll_cntl_0 + reg_offset, 0x10070487);
		} else if (chip == ADC_CHIP_T3 || chip == ADC_CHIP_T5W) {
			adc_wr_hiu(pll_addr->adc_pll_cntl_0, 0x20070487);
			adc_wr_hiu(pll_addr->adc_pll_cntl_0, 0x30070487);
			adc_wr_hiu(pll_addr->adc_pll_cntl_1, 0x01000000);
			adc_wr_hiu(pll_addr->adc_pll_cntl_2, 0x0);
			adc_wr_hiu(pll_addr->adc_pll_cntl_3, 0x48681f00);
			adc_wr_hiu(pll_addr->adc_pll_cntl_4, 0x88770290);
			adc_wr_hiu(pll_addr->adc_pll_cntl_5, 0x3927a000);
			adc_wr_hiu(pll_addr->adc_pll_cntl_6, 0x56540000);
			adc_wr_hiu(pll_addr->adc_pll_cntl_0, 0x10070487);
		} else {
			adc_wr_hiu(pll_addr->adc_pll_cntl_0, 0x20050487);
			adc_wr_hiu(pll_addr->adc_pll_cntl_0, 0x30050487);
			adc_wr_hiu(pll_addr->adc_pll_cntl_1, 0x00800000);
			adc_wr_hiu(pll_addr->adc_pll_cntl_2, 0x0);
			adc_wr_hiu(pll_addr->adc_pll_cntl_3, 0x48681f00);
			adc_wr_hiu(pll_addr->adc_pll_cntl_4, 0x88770290);
			adc_wr_hiu(pll_addr->adc_pll_cntl_5, 0x3927a000);
			adc_wr_hiu(pll_addr->adc_pll_cntl_6, 0x56540000);
			adc_wr_hiu(pll_addr->adc_pll_cntl_0, 0x10050487);
		}
		break;

	default:
		adc_set_dtvdemod_pll_by_clk(devp, p_dtv_para);
		break;
	}
}

int adc_set_pll_cntl(bool on, enum adc_sel module_sel, void *p_para)
{
	unsigned int adc_pll_lock_cnt = 0, adc_pll_sts = 0;
	int ret = 0;/* 0: success; -x: failed */
	struct dfe_adcpll_para *p_dtv_para = p_para;/* only for dtv demod */
	struct tvin_adc_dev *devp = adc_devp;
	struct adc_reg_addr *adc_addr;
	struct adc_pll_reg_addr *pll_addr;
	unsigned int reg_offset = 0;

	if (!probe_finish || !devp) {
		ret = -6;
		return ret;
	}

	adc_addr = &devp->plat_data->adc_addr;
	pll_addr = &devp->plat_data->pll_addr;

	if (!on) {
		mutex_lock(&devp->pll_mutex);
		devp->pll_flg &= ~module_sel;
		mutex_unlock(&devp->pll_mutex);
		if (devp->print_en)
			pr_info("%s:reinit flag on:%d,module:0x%x,flag:0x%x\n",
				__func__, on, module_sel, devp->pll_flg);
		return ret;
	}

	switch (module_sel) {
	case ADC_ATV_DEMOD:
		if (devp->pll_flg & (ADC_TVAFE | ADC_DTV_DEMOD)) {
			ret = -1;

			if (devp->print_en)
				pr_info("%s:DEMOD fail!:%d\n",
					__func__, devp->pll_flg);
			break;
		}

		if (devp->pll_flg & ADC_ATV_DEMOD) {
			if (devp->print_en)
				pr_info("%s:DEMOD ATV had done!:%d\n",
					__func__, devp->pll_flg);
			break;
		}

		mutex_lock(&devp->pll_mutex);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			do {
				if (get_cpu_type() >= MESON_CPU_MAJOR_ID_T5) {
					adc_wr_hiu(pll_addr->adc_pll_cntl_0, 0x210504a0);
					adc_wr_hiu(pll_addr->adc_pll_cntl_0, 0x310504a0);
					adc_wr_hiu(pll_addr->adc_pll_cntl_1, 0x03c00000);
					adc_wr_hiu(pll_addr->adc_pll_cntl_2, 0x00000000);
					adc_wr_hiu(pll_addr->adc_pll_cntl_3, 0x48681f00);
					adc_wr_hiu(pll_addr->adc_pll_cntl_4, 0x88770290);
					adc_wr_hiu(pll_addr->adc_pll_cntl_5, 0x39272000);
					adc_wr_hiu(pll_addr->adc_pll_cntl_6, 0x56540000);
					adc_wr_hiu_bits(adc_addr->vdac_cntl_0, 1, 11, 1);
					adc_wr_hiu(pll_addr->adc_pll_cntl_0, 0x110504a0);
					if (devp->plat_data->chip_id >= ADC_CHIP_T5D) {
						adc_wr_hiu(adc_addr->s2_dadc_cntl_2, 0x00000582);
						adc_wr_hiu(adc_addr->dadc_cntl_4, 0x0);
					}
				} else {
					adc_wr_hiu(pll_addr->adc_pll_cntl_0, 0x01200490);
					adc_wr_hiu(pll_addr->adc_pll_cntl_0, 0x31200490);
					adc_wr_hiu(pll_addr->adc_pll_cntl_1, 0x06c00000);
					adc_wr_hiu(pll_addr->adc_pll_cntl_2, 0xe1800000);
					adc_wr_hiu(pll_addr->adc_pll_cntl_3, 0x48681c00);
					adc_wr_hiu(pll_addr->adc_pll_cntl_4, 0x88770290);
					adc_wr_hiu(pll_addr->adc_pll_cntl_5, 0x39272000);
					adc_wr_hiu(pll_addr->adc_pll_cntl_6, 0x56540000);
					adc_wr_hiu(pll_addr->adc_pll_cntl_0, 0x11010490);
				}
				/** DADC CNTL for LIF signal input **/
				adc_wr_hiu(adc_addr->dadc_cntl, 0x00303044);

				if (devp->plat_data->chip_id >= ADC_CHIP_T3)
					adc_wr_hiu(adc_addr->dadc_cntl_2, 0x00003488);
				else
					adc_wr_hiu(adc_addr->dadc_cntl_2, 0x00003480);
				adc_wr_hiu(adc_addr->dadc_cntl_3, 0x08300b83);
				usleep_range(100, 101);
				adc_pll_lock_cnt++;
				if (devp->plat_data->chip_id == ADC_CHIP_T3)
					adc_pll_sts = adc_rd_hiu_bits(pll_addr->adc_pll_cntl_7, 31, 1);
				else
					adc_pll_sts = adc_rd_hiu_bits(pll_addr->adc_pll_cntl_0, 31, 1);
			} while (!adc_pll_sts && (adc_pll_lock_cnt < 10));
		} else {
			do {
				if (is_meson_txl_cpu() ||
				    is_meson_txlx_cpu()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
					adc_wr_hiu(HHI_ADC_PLL_CNTL3, 0x4a6a2110);
					adc_wr_hiu(HHI_ADC_PLL_CNTL, 0x30f14250);
					adc_wr_hiu(HHI_ADC_PLL_CNTL1, 0x22000442);
					/* 0x5ba00380 from pll;0x5ba00384 clk */
					/* from crystal */
					adc_wr_hiu(HHI_ADC_PLL_CNTL2, 0x5ba00384);
					adc_wr_hiu(HHI_ADC_PLL_CNTL3, 0x4a6a2110);
					adc_wr_hiu(HHI_ADC_PLL_CNTL4, 0x02913004);
					adc_wr_hiu(HHI_ADC_PLL_CNTL5, 0x00034a00);
					adc_wr_hiu(HHI_ADC_PLL_CNTL6, 0x00005000);
					adc_wr_hiu(HHI_ADC_PLL_CNTL3, 0xca6a2110);
					adc_wr_hiu(HHI_ADC_PLL_CNTL3, 0x4a6a2110);
					/** DADC CNTL for LIF signal input **/
					adc_wr_hiu(HHI_DADC_CNTL, 0x00102038);
					adc_wr_hiu(HHI_DADC_CNTL2, 0x00000406);
					adc_wr_hiu(HHI_DADC_CNTL3, 0x00082183);
#endif
				} else {
					adc_wr_hiu(HHI_ADC_PLL_CNTL3, 0xca2a2110);
					adc_wr_hiu(HHI_ADC_PLL_CNTL4, 0x2933800);
					adc_wr_hiu(HHI_ADC_PLL_CNTL, 0xe0644220);
					adc_wr_hiu(HHI_ADC_PLL_CNTL2, 0x34e0bf84);
					adc_wr_hiu(HHI_ADC_PLL_CNTL3, 0x4a2a2110);
					/** DADC CNTL for LIF signal input **/
					adc_wr_hiu(HHI_DADC_CNTL, 0x1411036);
					adc_wr_hiu(HHI_DADC_CNTL2, 0x0);
					adc_wr_hiu(HHI_DADC_CNTL3, 0x430036);
					adc_wr_hiu(HHI_DADC_CNTL4, 0x80600240);
					/* TVFE reset */
					adc_wr_hiu_bits(RESET1_REGISTER, 1, 7, 1);
				}
			usleep_range(100, 101);
			adc_pll_lock_cnt++;
			} while (!adc_rd_hiu_bits(HHI_ADC_PLL_CNTL, 31, 1) &&
				(adc_pll_lock_cnt < 10));
		}
		devp->pll_flg |= ADC_ATV_DEMOD;
		mutex_unlock(&devp->pll_mutex);
		if (adc_pll_lock_cnt == 10)
			pr_info("%s: ATV_DEMOD adc pll lock fail!!!\n",
					__func__);
		break;
	case ADC_TVAFE:
		if (devp->pll_flg & (ADC_ATV_DEMOD | ADC_DTV_DEMOD)) {
			ret = -2;

			if (devp->print_en)
				pr_info("%s:AFE fail!!!:%d\n", __func__, devp->pll_flg);
			break;
		}
		mutex_lock(&devp->pll_mutex);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			do {
				adc_wr_hiu(pll_addr->adc_pll_cntl_0, 0x01200490);
				adc_wr_hiu(pll_addr->adc_pll_cntl_0, 0x31200490);
				adc_wr_hiu(pll_addr->adc_pll_cntl_1, 0x06c00000);
				if (devp->plat_data->chip_id >= ADC_CHIP_T5)
					adc_wr_hiu(pll_addr->adc_pll_cntl_2,
					       0xe1800000);
				else
					adc_wr_hiu(pll_addr->adc_pll_cntl_2,
					       0xe0800000);
				if (devp->plat_data->chip_id >= ADC_CHIP_T5D) {
					adc_wr_hiu(adc_addr->s2_dadc_cntl_2, 0x00000582);
					adc_wr_hiu(adc_addr->dadc_cntl_4, 0x0);
				}
				adc_wr_hiu(pll_addr->adc_pll_cntl_3, 0x48681c00);
				adc_wr_hiu(pll_addr->adc_pll_cntl_4, 0x88770290);
				adc_wr_hiu(pll_addr->adc_pll_cntl_5, 0x39272000);
				adc_wr_hiu(pll_addr->adc_pll_cntl_6, 0x56540000);
				if (get_cpu_type() >= MESON_CPU_MAJOR_ID_T5)
					adc_wr_hiu_bits(adc_addr->vdac_cntl_0,
							1, 11, 1);
				adc_wr_hiu(pll_addr->adc_pll_cntl_0, 0x11010490);

				usleep_range(100, 101);
				adc_pll_lock_cnt++;
				if (devp->plat_data->chip_id == ADC_CHIP_T3)
					adc_pll_sts = adc_rd_hiu_bits(pll_addr->adc_pll_cntl_7, 31, 1);
				else
					adc_pll_sts = adc_rd_hiu_bits(pll_addr->adc_pll_cntl_0, 31, 1);
			} while (!adc_pll_sts && (adc_pll_lock_cnt < 10));
			adc_wr_hiu(adc_addr->dadc_cntl, 0x00303044);

			if (devp->plat_data->chip_id >= ADC_CHIP_T3)
				adc_wr_hiu(adc_addr->dadc_cntl_2, 0x00003408);
			else
				adc_wr_hiu(adc_addr->dadc_cntl_2, 0x00003400);

			adc_wr_hiu(adc_addr->dadc_cntl_3, 0x08300b83);
		} else {
			do {
				if (is_meson_txl_cpu() ||
				    is_meson_txlx_cpu()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
					adc_wr_hiu(HHI_ADC_PLL_CNTL3, 0x4a6a2110);
					adc_wr_hiu(HHI_ADC_PLL_CNTL, 0x30f14250);
					adc_wr_hiu(HHI_ADC_PLL_CNTL1, 0x22000442);
					/* 0x5ba00380 from pll;0x5ba00384 clk */
					/* from crystal */
					adc_wr_hiu(HHI_ADC_PLL_CNTL2, 0x5ba00384);
					adc_wr_hiu(HHI_ADC_PLL_CNTL3, 0x4a6a2110);
					adc_wr_hiu(HHI_ADC_PLL_CNTL4, 0x02913004);
					adc_wr_hiu(HHI_ADC_PLL_CNTL5, 0x00034a00);
					adc_wr_hiu(HHI_ADC_PLL_CNTL6, 0x00005000);
					adc_wr_hiu(HHI_ADC_PLL_CNTL3, 0xca6a2110);
					adc_wr_hiu(HHI_ADC_PLL_CNTL3, 0x4a6a2110);
					adc_wr_hiu(HHI_CADC_CNTL, 0x02000A08);
					adc_wr_hiu(HHI_CADC_CNTL2, 0x04007B05);
#endif
				} else {
					adc_wr_hiu(HHI_ADC_PLL_CNTL3, 0xca2a2110);
					adc_wr_hiu(HHI_ADC_PLL_CNTL4, 0x2933800);
					adc_wr_hiu(HHI_ADC_PLL_CNTL, 0xe0644220);
					adc_wr_hiu(HHI_ADC_PLL_CNTL2, 0x34e0bf84);
					adc_wr_hiu(HHI_ADC_PLL_CNTL3, 0x4a2a2110);
					/* TVFE reset */
					adc_wr_hiu_bits(RESET1_REGISTER,
							1, 7, 1);
				}
				usleep_range(100, 101);
				adc_pll_lock_cnt++;
			} while (!adc_rd_hiu_bits(HHI_ADC_PLL_CNTL, 31, 1) &&
				(adc_pll_lock_cnt < 10));
		}
		devp->pll_flg |= ADC_TVAFE;
		mutex_unlock(&devp->pll_mutex);
		if (adc_pll_lock_cnt == 10)
			pr_info("%s: TVAFE adc pll lock fail!!!\n",
					__func__);
		break;

	case ADC_DTV_DEMOD:
		if (devp->pll_flg & (ADC_ATV_DEMOD | ADC_TVAFE)) {
			ret = -3;

			if (devp->print_en)
				pr_info("%s:DDEMOD fail!:%d\n",
					__func__, devp->pll_flg);
			break;
		}

		mutex_lock(&devp->pll_mutex);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			do {
				switch (devp->plat_data->chip_id) {
				case ADC_CHIP_T5D:
				case ADC_CHIP_S4:
				case ADC_CHIP_T3:
				case ADC_CHIP_S4D:
				case ADC_CHIP_T5W:
					adc_set_dtvdemod_pll_by_delsys(devp, p_dtv_para);
					break;

				default:
					adc_set_dtvdemod_pll_by_clk(devp, p_dtv_para);
					break;
				}

				usleep_range(100, 101);
				adc_pll_lock_cnt++;
				if (devp->plat_data->chip_id == ADC_CHIP_T3) {
					adc_pll_sts = adc_rd_hiu_bits(pll_addr->adc_pll_cntl_7, 31, 1);
				} else {
					if (devp->plat_data->chip_id == ADC_CHIP_S4 ||
						devp->plat_data->chip_id == ADC_CHIP_S4D)
						reg_offset = 0x10;
					else
						reg_offset = 0;

					adc_pll_sts = adc_rd_hiu_bits(pll_addr->adc_pll_cntl_0 + reg_offset, 31, 1);
				}

			} while (!adc_pll_sts && (adc_pll_lock_cnt < 10));

			if (devp->plat_data->chip_id < ADC_CHIP_T3)
				adc_wr_hiu(0xf3, 0x00800000);
		} else if (is_meson_txl_cpu() || is_meson_txlx_cpu()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
			do {
				adc_wr_hiu(HHI_ADC_PLL_CNTL3, 0x4a6a2110);
				adc_wr_hiu(HHI_ADC_PLL_CNTL,  0x5d414260);
				adc_wr_hiu(HHI_ADC_PLL_CNTL1, 0x22000442);

				if (is_meson_txl_cpu())
					adc_wr_hiu(HHI_ADC_PLL_CNTL2, 0x5ba00384);
				else
					adc_wr_hiu(HHI_ADC_PLL_CNTL2, 0x5ba00385);

				adc_wr_hiu(HHI_ADC_PLL_CNTL3, 0x4a6a2110);
				adc_wr_hiu(HHI_ADC_PLL_CNTL4, 0x02913004);
				adc_wr_hiu(HHI_ADC_PLL_CNTL5, 0x00034a00);
				adc_wr_hiu(HHI_ADC_PLL_CNTL6, 0x00005000);
				/* reset */
				adc_wr_hiu(HHI_ADC_PLL_CNTL3, 0xca6a2110);
				adc_wr_hiu(HHI_ADC_PLL_CNTL3, 0x4a6a2110);

				usleep_range(100, 101);
				adc_pll_lock_cnt++;

			} while (!adc_rd_hiu_bits(HHI_ADC_PLL_CNTL, 31, 1) &&
				 (adc_pll_lock_cnt < 10));
#endif
		} else if (is_meson_gxlx_cpu()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
			adc_wr_hiu(HHI_ADC_PLL_CNTL1, 0x22000442);
			adc_wr_hiu(HHI_ADC_PLL_CNTL5, 0x00034a00);
			adc_wr_hiu(HHI_ADC_PLL_CNTL6, 0x00005000);
			adc_pll_lock_cnt = 1;
#endif
		}

		devp->pll_flg |= ADC_DTV_DEMOD;
		mutex_unlock(&devp->pll_mutex);
		if (adc_pll_lock_cnt >= 10)
			pr_info("%s: DTV_DEMOD adc pll lock fail!!!\n",
					__func__);
		break;
	case ADC_DTV_DEMODPLL:
		if (devp->pll_flg & (ADC_ATV_DEMOD | ADC_TVAFE)) {
			ret = -4;

			if (devp->print_en)
				pr_info("%s:DMODPLL fail!!!:%d\n", __func__,
					devp->pll_flg);
			break;
		}

		if (!p_dtv_para) {
			ret = -5;

			if (devp->print_en)
				pr_info("%s: DTV para is NULL\n", __func__);
			break;
		}
		mutex_lock(&devp->pll_mutex);

		if (is_meson_txl_cpu() || is_meson_txlx_cpu()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
			do {
				/* reset */
				adc_wr_hiu(HHI_ADC_PLL_CNTL3, 0xca6a2110);
				adc_wr_hiu(HHI_ADC_PLL_CNTL, p_dtv_para->adcpllctl);
				if (p_dtv_para->atsc) {
					adc_wr_hiu(HHI_DEMOD_CLK_CNTL, 0x507);
				} else {
					/* bugzilla 139044 */
					if (is_meson_txl_cpu())
						adc_wr_hiu(HHI_DEMOD_CLK_CNTL,
						       0x301);
					else
						adc_wr_hiu(HHI_DEMOD_CLK_CNTL,
						       0x502);
				}

				adc_wr_hiu(HHI_ADC_PLL_CNTL3, 0x4a6a2110);

				usleep_range(100, 101);
				adc_pll_lock_cnt++;

			} while (!adc_rd_hiu_bits(HHI_ADC_PLL_CNTL, 31, 1) &&
				 (adc_pll_lock_cnt < 10));
#endif
		} else if (is_meson_gxlx_cpu()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
			adc_wr_hiu(HHI_DEMOD_CLK_CNTL, 0x1000502);
			//adc_pll_lock_cnt = 1;
#endif
		}  else {
			/*is_meson_gxtvbb_cpu()*/
			adc_wr_hiu(HHI_ADC_PLL_CNTL3, 0x8a2a2110);/*reset*/
			adc_wr_hiu(HHI_ADC_PLL_CNTL,  p_dtv_para->adcpllctl);
			adc_wr_hiu(HHI_DEMOD_CLK_CNTL, p_dtv_para->demodctl);
			adc_wr_hiu(HHI_ADC_PLL_CNTL3, 0x0a2a2110);
			//adc_pll_lock_cnt = 1;
		}

		devp->pll_flg |= ADC_DTV_DEMOD;
		mutex_unlock(&devp->pll_mutex);
		//if (adc_pll_lock_cnt == 10 && devp->print_en)
			//pr_info("%s: adc pll lock fail!!!\n", __func__);
		break;

	default:
		if (devp->print_en)
			pr_err("%s:module: 0x%x wrong module index !! ",
			       __func__, module_sel);
		break;
	}
	if (devp->print_en)
		pr_info("%s:init flag on:%d,module:0x%x,flag:0x%x cnt:0x%x\n",
			__func__, on, module_sel, devp->pll_flg, adc_pll_lock_cnt);
	return ret;
}
EXPORT_SYMBOL(adc_set_pll_cntl);

static void adc_pll_down(void)
{
	if (!probe_finish || !adc_devp)
		return;
	struct adc_pll_reg_addr *pll_addr = &adc_devp->plat_data->pll_addr;
	struct adc_reg_addr *adc_addr = &adc_devp->plat_data->adc_addr;

	pr_info("adc: %s\n", __func__);

	if (!adc_devp->pll_flg && is_meson_tl1_cpu() &&
	    adc_rd_hiu_bits(pll_addr->adc_pll_cntl_0, 28, 1)) {
		adc_wr_hiu_bits(pll_addr->adc_pll_cntl_0, 0, 28, 1);
	}

	if (!adc_devp->pll_flg && get_cpu_type() >= MESON_CPU_MAJOR_ID_T5) {
		adc_wr_hiu_bits(adc_addr->vdac_cntl_1, 0, 7, 1);
		adc_wr_hiu_bits(adc_addr->dadc_cntl_3, 0, 0, 2);
		adc_wr_hiu_bits(pll_addr->adc_pll_cntl_0, 0, 28, 1);
		if (adc_devp->plat_data->is_tv_chip)
			adc_wr_hiu_bits(adc_addr->s2_dadc_cntl_2, 0, 8, 1);
	}

	if (adc_devp->plat_data->is_tv_chip) {
		adc_wr_afe(AFE_VAFE_CTRL0, 0x0);
		adc_wr_afe(AFE_VAFE_CTRL1, 0x0);
		adc_wr_afe(AFE_VAFE_CTRL2, 0x0);
	}
}

int adc_get_pll_flag(void)
{
	unsigned int ret = 0;

	if (!probe_finish || !adc_devp)
		return -1;

	if (!mutex_trylock(&adc_devp->pll_mutex))
		return 0;

	ret = adc_devp->pll_flg;
	mutex_unlock(&adc_devp->pll_mutex);

	return ret;
}
EXPORT_SYMBOL(adc_get_pll_flag);

void adc_set_pll_reset(void)
{
	if (!probe_finish || !adc_devp)
		return;

	adc_devp->pll_flg = 0;
}
EXPORT_SYMBOL(adc_set_pll_reset);

static void adc_parse_para(char *buf_orig, char **parm)
{
	char *ps, *token;
	char delim1[3] = " ";
	char delim2[2] = "\n";
	unsigned int n = 0;

	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;

		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}

static void adc_dump_regs(void)
{
	struct adc_reg_addr *adc_addr = NULL;
	struct adc_pll_reg_addr *pll_addr = NULL;
	int chip_id = 0;
	unsigned int reg_offset = 0;

	if (IS_ERR_OR_NULL(adc_devp) || IS_ERR_OR_NULL(adc_devp->plat_data)) {
		pr_info("adc_devp is NULL, it's not initialized yet\n");

		return;
	}

	adc_addr = &adc_devp->plat_data->adc_addr;
	pll_addr = &adc_devp->plat_data->pll_addr;
	chip_id = adc_devp->plat_data->chip_id;

	pr_info("chip_id=0x%x is_tv=0x%x\n",
		chip_id, adc_devp->plat_data->is_tv_chip);

	pr_info("pll_flag=0x%x filter_flg=0x%x print_en=0x%x\n",
		adc_devp->pll_flg, adc_devp->filter_flg,
		adc_devp->print_en);

	if (chip_id == ADC_CHIP_S4 || chip_id == ADC_CHIP_S4D) {
		reg_offset = 0x10;
	} else {
		reg_offset = 0;
		pr_info("-------- vafe --------\n");
		pr_info("AFE_VAFE_CTRL0(0x%x):0x%x\n",
			AFE_VAFE_CTRL0, adc_rd_afe(AFE_VAFE_CTRL0));
		pr_info("AFE_VAFE_CTRL1(0x%x):0x%x\n",
			AFE_VAFE_CTRL1, adc_rd_afe(AFE_VAFE_CTRL1));
		pr_info("AFE_VAFE_CTRL2(0x%x):0x%x\n",
			AFE_VAFE_CTRL2, adc_rd_afe(AFE_VAFE_CTRL2));
	}

	pr_info("-------- hiu --------\n");
	pr_info("HHI_DADC_CNTL(0x%x):0x%x\n", adc_addr->dadc_cntl,
		adc_rd_hiu(adc_addr->dadc_cntl));
	pr_info("HHI_DADC_CNTL2(0x%x):0x%x\n", adc_addr->dadc_cntl_2,
		adc_rd_hiu(adc_addr->dadc_cntl_2));
	pr_info("HHI_DADC_CNTL3(0x%x):0x%x\n", adc_addr->dadc_cntl_3,
		adc_rd_hiu(adc_addr->dadc_cntl_3));
	pr_info("HHI_DADC_CNTL4(0x%x):0x%x\n", adc_addr->dadc_cntl_4,
		adc_rd_hiu(adc_addr->dadc_cntl_4));
	pr_info("HHI_S2_DADC_CNTL(0x%x):0x%x\n", adc_addr->s2_dadc_cntl,
		adc_rd_hiu(adc_addr->s2_dadc_cntl));
	pr_info("HHI_S2_DADC_CNTL2(0x%x):0x%x\n", adc_addr->s2_dadc_cntl_2,
		adc_rd_hiu(adc_addr->s2_dadc_cntl_2));
	pr_info("HHI_ADC_PLL_CNTL0(0x%x):0x%x\n",
		pll_addr->adc_pll_cntl_0 + reg_offset,
		adc_rd_hiu(pll_addr->adc_pll_cntl_0 + reg_offset));
	pr_info("HHI_ADC_PLL_CNTL1(0x%x):0x%x\n",
		pll_addr->adc_pll_cntl_1 + reg_offset,
		adc_rd_hiu(pll_addr->adc_pll_cntl_1 + reg_offset));
	pr_info("HHI_ADC_PLL_CNTL2(0x%x):0x%x\n",
		pll_addr->adc_pll_cntl_2 + reg_offset,
		adc_rd_hiu(pll_addr->adc_pll_cntl_2 + reg_offset));
	pr_info("HHI_ADC_PLL_CNTL3(0x%x):0x%x\n",
		pll_addr->adc_pll_cntl_3 + reg_offset,
		adc_rd_hiu(pll_addr->adc_pll_cntl_3 + reg_offset));
	pr_info("HHI_ADC_PLL_CNTL4(0x%x):0x%x\n",
		pll_addr->adc_pll_cntl_4 + reg_offset,
		adc_rd_hiu(pll_addr->adc_pll_cntl_4 + reg_offset));
	pr_info("HHI_ADC_PLL_CNTL5(0x%x):0x%x\n",
		pll_addr->adc_pll_cntl_5 + reg_offset,
		adc_rd_hiu(pll_addr->adc_pll_cntl_5 + reg_offset));
	pr_info("HHI_ADC_PLL_CNTL6(0x%x):0x%x\n",
		pll_addr->adc_pll_cntl_6 + reg_offset,
		adc_rd_hiu(pll_addr->adc_pll_cntl_6 + reg_offset));
	pr_info("HHI_ADC_PLL_CNTL7(0x%x):0x%x\n",
		pll_addr->adc_pll_cntl_7 + reg_offset,
		adc_rd_hiu(pll_addr->adc_pll_cntl_7 + reg_offset));
	pr_info("HHI_VDAC_CNTL0(0x%x):0x%x\n", adc_addr->vdac_cntl_0,
		adc_rd_hiu(adc_addr->vdac_cntl_0));

	pr_info("HHI_VDAC_CNTL1_T5(0x%x):0x%x\n", HHI_VDAC_CNTL1_T5,
		adc_rd_hiu(HHI_VDAC_CNTL1_T5));
}

static ssize_t adc_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	char *buf_orig, *parm[47] = {NULL};
	unsigned int val;
	struct tvin_adc_dev *devp = adc_devp;
	unsigned int reg_addr;
	unsigned int reg_val;

	if (!probe_finish || !devp)
		return -1;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	adc_parse_para(buf_orig, (char **)&parm);

	if (parm[0] && !strcmp(parm[0], "state")) {
		adc_dump_regs();
	} else if (parm[0] && !strcmp(parm[0], "down")) {
		adc_pll_down();
	} else if (parm[0] && !strcmp(parm[0], "dump_reg")) {
		adc_dump_regs();
	} else if (parm[0] && !strcmp(parm[0], "debug")) {
		if (!parm[1])
			goto tvin_adc_store_err;

		if (kstrtouint(parm[1], 10, &val) >= 0)
			devp->print_en = val;
	} else if (parm[0] && parm[1] && parm[2]) {
		if (kstrtouint(parm[1], 16, &reg_addr) < 0) {
			pr_info("reg addr error\n");
			goto tvin_adc_store_err;
		}

		if (kstrtouint(parm[2], 16, &reg_val) < 0) {
			pr_info("reg value error\n");
			goto tvin_adc_store_err;
		}

		if (!strcmp(parm[0], "vafe"))
			adc_wr_afe(reg_addr, reg_val);
		else if (!strcmp(parm[0], "hiu"))
			adc_wr_hiu(reg_addr, reg_val);
		else
			pr_info("reg not match\n");
	}

tvin_adc_store_err:
	kfree(buf_orig);
	return count;
}

static ssize_t adc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (!probe_finish || !adc_devp)
		return -1;

	pr_info("debug : %d\n", adc_devp->print_en);
	pr_info("echo state > /sys/class/adc/adc/adc\n");
	pr_info("echo dump_reg > /sys/class/adc/adc/adc\n");
	pr_info("echo down > /sys/class/adc/adc/adc\n");
	pr_info("echo vafe reg_addr reg_val >  /sys/class/adc/adc/adc\n");
	pr_info("echo hiu  reg_addr reg_val >  /sys/class/adc/adc/adc\n");
	pr_info("version : %s\n", ADC_VER);
	return 0;
}

static char *adc_class_devnode(struct device *dev, umode_t *mode)
{
	if (mode)
		*mode = 0666;

	return NULL;
}

static DEVICE_ATTR_RW(adc);

static struct class adc_class = {
	.name = ADC_DEV_NAME,
	.devnode = adc_class_devnode,
};

static int adc_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t adc_read(struct file *f, char __user *buf,
			size_t size, loff_t *p)
{
	return 0;
}

static ssize_t adc_write(struct file *f, const char __user *buf,
			 size_t size, loff_t *p)
{
	return 0;
}

static int adc_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long adc_ioctl(struct file *f,
		      unsigned int cmd, unsigned long arg)
{
	/*void __user *argp = (void __user *)arg;*/
	if (!probe_finish || !adc_devp)
		return -1;

	mutex_lock(&adc_devp->ioctl_mutex);
	switch (cmd) {
	default:
		break;
	}
	mutex_unlock(&adc_devp->ioctl_mutex);
	return 0;
}

#ifdef CONFIG_COMPAT
static long adc_compat_ioctl(struct file *f,
			     unsigned int cmd, unsigned long arg)
{
	arg = (unsigned long)compat_ptr(arg);
	return adc_ioctl(f, cmd, arg);
}
#endif

static const struct file_operations adc_fops = {
	.owner          = THIS_MODULE,
	.open           = adc_open,
	.read           = adc_read,
	.write          = adc_write,
	.release        = adc_release,
	.unlocked_ioctl = adc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = adc_compat_ioctl,
#endif
};

static int adc_remove(struct platform_device *pdev)
{
	if (probe_finish && adc_devp) {
		device_destroy(&adc_class, MKDEV(adc_devp->dev_no, 0));
		unregister_chrdev(adc_devp->dev_no, ADC_DEV_NAME);
		class_unregister(&adc_class);
		mutex_destroy(&adc_devp->pll_mutex);
		mutex_destroy(&adc_devp->ioctl_mutex);
		mutex_destroy(&adc_devp->filter_mutex);
		kfree(adc_devp);
	}

	return 0;
}

static void adc_shutdown(struct platform_device *pdev)
{
	adc_pll_down();
	pr_info("adc: %s\n", __func__);
}

#ifdef CONFIG_PM
static int adc_suspend(struct platform_device *pdev, pm_message_t state)
{
	adc_pll_down();
	pr_info("adc: %s\n", __func__);
	return 0;
}

static int adc_resume(struct platform_device *pdev)
{
	return 0;
}
#endif

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static const struct adc_platform_data_s adc_data_txl = {
	.chip_id = ADC_CHIP_TXL,
};

static const struct adc_platform_data_s adc_data_txlx = {
	.chip_id = ADC_CHIP_TXLX,
};

static const struct adc_platform_data_s adc_data_gxlx = {
	.chip_id = ADC_CHIP_GXLX,
};

static const struct adc_platform_data_s adc_data_tl1 = {
	ADC_ADDR_TL1_TO_S4,
	ADC_PLL_ADDR_TL1,
	.chip_id = ADC_CHIP_TL1,
};
#endif

static const struct adc_platform_data_s adc_data_tm2 = {
	ADC_ADDR_TL1_TO_S4,
	ADC_PLL_ADDR_TL1,
	.chip_id = ADC_CHIP_TM2,
	.is_tv_chip = true,
};

static const struct adc_platform_data_s adc_data_t5 = {
	ADC_ADDR_TL1_TO_S4,
	ADC_PLL_ADDR_TL1,
	.chip_id = ADC_CHIP_T5,
	.is_tv_chip = true,
};

static const struct adc_platform_data_s adc_data_t5d = {
	ADC_ADDR_TL1_TO_S4,
	ADC_PLL_ADDR_TL1,
	.chip_id = ADC_CHIP_T5D,
	.is_tv_chip = true,
};

static const struct adc_platform_data_s adc_data_s4 = {
	ADC_ADDR_TL1_TO_S4,
	ADC_PLL_ADDR_TL1,
	.chip_id = ADC_CHIP_S4,
	.is_tv_chip = false,
};

static const struct adc_platform_data_s adc_data_t3 = {
	ADC_ADDR_T3,
	ADC_PLL_ADDR_T3,
	.chip_id = ADC_CHIP_T3,
	.is_tv_chip = true,
};

static const struct adc_platform_data_s adc_data_s4d = {
	ADC_ADDR_TL1_TO_S4,
	ADC_PLL_ADDR_TL1,
	.chip_id = ADC_CHIP_S4D,
	.is_tv_chip = false,
};

static const struct adc_platform_data_s adc_data_t5w = {
	ADC_ADDR_TL1_TO_S4,
	ADC_PLL_ADDR_TL1,
	.chip_id = ADC_CHIP_T5W,
	.is_tv_chip = true,
};

static const struct of_device_id adc_dt_match[] = {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible = "amlogic, adc-txl",
		.data = &adc_data_txl,
	},
	{
		.compatible = "amlogic, adc-txlx",
		.data = &adc_data_txlx,
	},
	{
		.compatible = "amlogic, adc-gxlx",
		.data = &adc_data_gxlx,
	},
	{
		.compatible = "amlogic, adc-tl1",
		.data = &adc_data_tl1,
	},
#endif
	{
		.compatible = "amlogic, adc-tm2",
		.data = &adc_data_tm2,
	},
	{
		.compatible = "amlogic, adc-t5",
		.data = &adc_data_t5,
	},
	{
		.compatible = "amlogic, adc-t5d",
		.data = &adc_data_t5d,
	},
	{
		.compatible = "amlogic, adc-s4",
		.data = &adc_data_s4,
	},
	{
		.compatible = "amlogic, adc-t3",
		.data = &adc_data_t3,
	},
	{
		.compatible = "amlogic, adc-s4d",
		.data = &adc_data_s4d,
	},
	{
		.compatible = "amlogic, adc-t5w",
		.data = &adc_data_t5w,
	},
	{}
};

static int adc_probe(struct platform_device *pdev)
{
	struct device *cdev;
	int ret = 0;
	struct tvin_adc_dev *devp;
	const struct of_device_id *of_id;
	int r;
	int size = 0;
	struct resource *res = 0;
	unsigned int i;

	devp = devm_kzalloc(&pdev->dev, sizeof(struct tvin_adc_dev),
			    GFP_KERNEL);
	if (IS_ERR_OR_NULL(devp)) {
		ret = -ENOMEM;
		goto tag_devm_kzalloc_err;
	}
	adc_devp = devp;
	/*devp->pdev  = pdev;*/
	devp->dev  = &pdev->dev;
	devp->node = pdev->dev.of_node;

	/* cdev registe */
	r = class_register(&adc_class);
	if (r) {
		ret = -EINVAL;
		goto tag_class_reg_err;
	}

	pdev->dev.class = &adc_class;
	r = register_chrdev(0, ADC_DEV_NAME, &adc_fops);
	if (r < 0) {
		ret = -ENODEV;
		goto tag_chrdev_reg_err;
	}
	devp->dev_no = r;

	cdev = device_create(&adc_class, &pdev->dev,
			     MKDEV(devp->dev_no, 0),
			     NULL, ADC_DEV_NAME);
	if (IS_ERR_OR_NULL(cdev)) {
		ret = -ENXIO;
		goto tag_device_create_err;
	}

	/*create sysfs attribute files*/
	ret = device_create_file(cdev, &dev_attr_adc);
	if (ret < 0) {
		pr_info("%s: create attribute files fail.\n", __func__);
		goto tag_create_dbg_err;
	}

	/*devp->cdev = cdev;*/

	/* get compatible matched device, to get chip related data */
	of_id = of_match_device(adc_dt_match, &pdev->dev);
	if (of_id) {
		devp->of_id = of_id;
		devp->plat_data = (struct adc_platform_data_s *)of_id->data;
	}

	for (i = 0; i < MAP_ADDR_NUM; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			pr_err("%s: res %d is faile\n", __func__, i);
			ret = -ENOMEM;
			goto fail_get_resource_mem;
		}
		size = resource_size(res);
		devp->phy_addr[i].size = size;
		devp->phy_addr[i].phy_addr = res->start;
		devp->vir_addr[i] = devm_ioremap_nocache(&pdev->dev, res->start, size);
	}

	mutex_init(&devp->ioctl_mutex);
	mutex_init(&devp->pll_mutex);
	mutex_init(&devp->filter_mutex);
	probe_finish = true;
	pr_info("%s ver:%s\n", __func__, ADC_VER);
	return 0;

fail_get_resource_mem:
tag_create_dbg_err:
	device_destroy(&adc_class, MKDEV(devp->dev_no, 0));
tag_device_create_err:
	unregister_chrdev(devp->dev_no, ADC_DEV_NAME);

tag_chrdev_reg_err:
	probe_finish = false;
	class_unregister(&adc_class);

tag_class_reg_err:
	devm_kfree(&pdev->dev, devp);
	adc_devp = NULL;
tag_devm_kzalloc_err:
	return ret;
}

static struct platform_driver adc_driver = {
	.driver = {
		.name  = "tvinadc",
		.owner = THIS_MODULE,
	#ifdef CONFIG_OF
		.of_match_table = adc_dt_match,
	#endif
	},
	.probe  = adc_probe,
	.remove = adc_remove,
	.shutdown = adc_shutdown,
#ifdef CONFIG_PM
	.suspend = adc_suspend,
	.resume  = adc_resume,
#endif
};

int __init adc_init(void)
{
	int ret;

	ret = platform_driver_register(&adc_driver);
	return ret;
}

void __exit adc_uninit(void)
{
	platform_driver_unregister(&adc_driver);
}

//MODULE_VERSION(TVDIN_ADC_VER);
//MODULE_AUTHOR("yong.qin@amlogic.com");
//MODULE_DESCRIPTION("AMLOGIC tvdin adc driver");
//MODULE_LICENSE("GPL");

