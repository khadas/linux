// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/of_clk.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/ioport.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <linux/sched/clock.h>
#include <linux/amlogic/media/registers/register_map.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/frc/frc_reg.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
// #include <linux/amlogic/media/frc/frc_common.h>

#include "frc_drv.h"
#include "frc_hw.h"
#include "frc_regs_table.h"
#include "frc_proc.h"

void __iomem *frc_clk_base;
void __iomem *vpu_base;

struct stvlock_frc_param gst_frc_param;

int FRC_PARAM_NUM = 8;
module_param(FRC_PARAM_NUM, int, 0664);
MODULE_PARM_DESC(FRC_PARAM_NUM, "FRC_PARAM_NUM");

const struct vf_rate_table vf_rate_table[11] = {
	{1600,  FRC_VD_FPS_60},
	{1601,	FRC_VD_FPS_60},
	{1920,  FRC_VD_FPS_50},
	{2000,  FRC_VD_FPS_48},
	{3200,	FRC_VD_FPS_30},
	{3840,	FRC_VD_FPS_25},
	{3999,	FRC_VD_FPS_24},
	{4000,	FRC_VD_FPS_24},
	{4001,  FRC_VD_FPS_24},
	{4004,  FRC_VD_FPS_24},
	{0000,	FRC_VD_FPS_00},
};

u32 vpu_reg_read(u32 addr)
{
	unsigned int value = 0;

	value = aml_read_vcbus_s(addr);
	return value;
}

void vpu_reg_write(u32 addr, u32 value)
{
	aml_write_vcbus_s(addr, value);
}

void vpu_reg_write_bits(unsigned int reg, unsigned int value,
					   unsigned int start, unsigned int len)
{
	aml_vcbus_update_bits_s(reg, value, start, len);
}

//.clk0: ( fclk_div3), //666 div: 0 1/1, 1:1/2, 2:1/4
//.clk1: ( fclk_div4), //500 div: 0 1/1, 1:1/2, 2:1/4
//.clk2: ( fclk_div5), //400 div: 0 1/1, 1:1/2, 2:1/4
//.clk3: ( fclk_div7), //285 div: 0 1/1, 1:1/2, 2:1/4
//.clk4: ( mp1_clk_out),
//.clk5: ( vid_pll0_clk),
//.clk6: ( hifi_pll_clk),
//.clk7: ( sys1_pll_clk),
void set_frc_div_clk(int sel, int div)
{
	unsigned int val = 0;
	unsigned int reg;

	reg = CLKCTRL_FRC_CLK_CNTL;

	val = readl(frc_clk_base + (reg << 2));

	if (val == 0 || (val & (1 << 31))) {
		val &= ~((0x7 << 9) |
				(1 << 8) |
				(0x7f << 0));
		val |= (sel << 9) |
			   (1 << 8) |
			   (div << 0);
		val &= ~(1 << 31);
		val |= 1 << 30;
		writel(val, (frc_clk_base + (reg << 2)));
	} else {
		val &= ~((0x7 << 25) |
				(1 << 24) |
				(0x7f << 16));
		val |= (sel << 25) |
			   (1 << 24) |
			   (div << 16);
		val |= 1 << 31;
		val |= 1 << 30;
		writel(val, (frc_clk_base + (reg << 2)));
	}
}

void set_me_div_clk(int sel, int div)
{
	unsigned int val = 0;
	unsigned int reg;

	reg = CLKCTRL_ME_CLK_CNTL;

	val = readl(frc_clk_base + (reg << 2));

	if (val == 0 || (val & (1 << 31))) {
		val &= ~((0x7 << 9) |
				(1 << 8) |
				(0x7f << 0));
		val |= (sel << 9) |
			   (1 << 8) |
			   (div << 0);
		val &= ~(1 << 31);
		val |= 1 << 30;
		writel(val, (frc_clk_base + (reg << 2)));
	} else {
		val &= ~((0x7 << 25) |
				(1 << 24) |
				(0x7f << 16));
		val |= (sel << 25) |
			   (1 << 24) |
			   (div << 16);
		val |= 1 << 31;
		val |= 1 << 30;
		writel(val, (frc_clk_base + (reg << 2)));
	}
}

void set_frc_clk_disable(void)
{
	unsigned int val = 0;
	unsigned int reg, reg1;

	reg = CLKCTRL_FRC_CLK_CNTL;
	reg1 = CLKCTRL_ME_CLK_CNTL;
	val = readl(frc_clk_base + (reg << 2));
	val &= ~(1 << 30);
	writel(val, (frc_clk_base + (reg << 2)));
	writel(val, (frc_clk_base + (reg1 << 2)));
}

void frc_clk_init(struct frc_dev_s *frc_devp)
{
	unsigned int height, width;

	height = frc_devp->out_sts.vout_height;
	width = frc_devp->out_sts.vout_width;

	if (1) /*(frc_devp->clk_frc && frc_devp->clk_me)*/ {
		clk_set_rate(frc_devp->clk_frc, 667000000);
		clk_prepare_enable(frc_devp->clk_frc);
		frc_devp->clk_frc_frq = clk_get_rate(frc_devp->clk_frc);
		pr_frc(0, "clk_frc frq : %d Mhz\n", frc_devp->clk_frc_frq / 1000000);
		frc_devp->clk_state = FRC_CLOCK_NOR;
		clk_set_rate(frc_devp->clk_me, 333333333);
		clk_prepare_enable(frc_devp->clk_me);
		frc_devp->clk_me_frq = clk_get_rate(frc_devp->clk_me);
		pr_frc(0, "clk_me frq : %d Mhz\n", frc_devp->clk_me_frq / 1000000);
	} else {
		if (height == HEIGHT_4K && width == WIDTH_4K) {
			set_frc_div_clk(0, 0);
			set_me_div_clk(2, 0);
		} else if (height == HEIGHT_2K && width == WIDTH_2K) {
			set_frc_div_clk(0, 0);
			set_me_div_clk(2, 0);
		}
	}
}

void frc_osdbit_setfalsecolor(u32 falsecolor)
{
	// WRITE_FRC_BITS(FRC_LOGO_DEBUG, falsecolor, 19, 1);  //  falsecolor enable
	frc_config_reg_value((falsecolor << 19), 0x80000, &regdata_iplogo_en_0503);
	WRITE_FRC_REG_BY_CPU(FRC_IPLOGO_EN, regdata_iplogo_en_0503);

}

void frc_init_config(struct frc_dev_s *devp)
{
	/*1: before postblend, 0: after postblend*/
	if (devp->frc_hw_pos == FRC_POS_AFTER_POSTBLEND) {
		vpu_reg_write_bits(VPU_FRC_TOP_CTRL, 0, 4, 1);
		frc_config_reg_value(0x02, 0x02, &regdata_blkscale_012c);
		WRITE_FRC_REG_BY_CPU(FRC_REG_BLK_SCALE, regdata_blkscale_012c);
		frc_config_reg_value(0x800000, 0xF00000, &regdata_iplogo_en_0503);
		WRITE_FRC_REG_BY_CPU(FRC_IPLOGO_EN, regdata_iplogo_en_0503);
	} else {
		vpu_reg_write_bits(VPU_FRC_TOP_CTRL, 1, 4, 1);
		frc_config_reg_value(0x0, 0x02, &regdata_blkscale_012c);
		WRITE_FRC_REG_BY_CPU(FRC_REG_BLK_SCALE, regdata_blkscale_012c);
		frc_config_reg_value(0x0, 0xF00000, &regdata_iplogo_en_0503);
		WRITE_FRC_REG_BY_CPU(FRC_IPLOGO_EN, regdata_iplogo_en_0503);
		frc_config_reg_value(0x0, 0x80000, &regdata_logodbg_0142);
		WRITE_FRC_REG_BY_CPU(FRC_LOGO_DEBUG, regdata_logodbg_0142);
	}
}

void frc_reset(u32 onoff)
{
	if (onoff) {
		WRITE_FRC_REG_BY_CPU(FRC_AUTO_RST_SEL, 0x3c);
		WRITE_FRC_REG_BY_CPU(FRC_SYNC_SW_RESETS, 0x3c);
	} else {
		WRITE_FRC_REG_BY_CPU(FRC_SYNC_SW_RESETS, 0x0);
		WRITE_FRC_REG_BY_CPU(FRC_AUTO_RST_SEL, 0x0);
	}
}

void frc_mc_reset(u32 onoff)
{
	if (onoff) {
		WRITE_FRC_REG_BY_CPU(FRC_MC_SW_RESETS, 0xffff);
		WRITE_FRC_REG_BY_CPU(FRC_MEVP_SW_RESETS, 0xffffff);
	} else {
		WRITE_FRC_REG_BY_CPU(FRC_MEVP_SW_RESETS, 0x0);
		WRITE_FRC_REG_BY_CPU(FRC_MC_SW_RESETS, 0x0);
	}
	pr_frc(2, "%s %d\n", __func__, onoff);
}

void set_frc_enable(u32 en)
{
	// struct frc_dev_s *devp = get_frc_devp();
	// WRITE_FRC_BITS(FRC_TOP_CTRL, 0, 8, 1);
	frc_config_reg_value(en, 0x1, &regdata_topctl_3f01);
	WRITE_FRC_REG_BY_CPU(FRC_TOP_CTRL, regdata_topctl_3f01);
	if (en == 1) {
		frc_mc_reset(1);
		frc_mc_reset(0);
		WRITE_FRC_REG_BY_CPU(FRC_TOP_SW_RESET, 0xFFFF);
		WRITE_FRC_REG_BY_CPU(FRC_TOP_SW_RESET, 0x0);
	} else {
		gst_frc_param.s2l_en = 0;
		gst_frc_param.frc_mcfixlines = 0;
		if (vlock_sync_frc_vporch(gst_frc_param) < 0)
			pr_frc(1, "frc_off_set maxlnct fail !!!\n");
		else
			pr_frc(1, "frc_off_set maxlnct success!!!\n");
	}
}

void set_frc_bypass(u32 en)
{
	vpu_reg_write_bits(VPU_FRC_TOP_CTRL, en, 0, 1);
}

void frc_crc_enable(struct frc_dev_s *frc_devp)
{
	struct frc_crc_data_s *crc_data;
	unsigned int en;

	crc_data = &frc_devp->frc_crc_data;
	en = crc_data->me_wr_crc.crc_en;
	WRITE_FRC_BITS(INP_ME_WRMIF_CTRL, en, 31, 1);

	en = crc_data->me_rd_crc.crc_en;
	WRITE_FRC_BITS(INP_ME_RDMIF_CTRL, en, 31, 1);

	en = crc_data->mc_wr_crc.crc_en;
	WRITE_FRC_BITS(INP_MC_WRMIF_CTRL, en, 31, 1);
}

void frc_set_buf_num(u32 frc_fb_num)
{
	WRITE_FRC_REG_BY_CPU(FRC_FRAME_BUFFER_NUM, frc_fb_num << 8 | frc_fb_num);
}

void frc_check_hw_stats(struct frc_dev_s *frc_devp, u8 checkflag)
{
	u32 val = 0;
	struct  frc_hw_stats_s  *tmpstats;

	tmpstats = &frc_devp->hw_stats;
	if (checkflag == 0) {
		tmpstats = &frc_devp->hw_stats;
		val = READ_FRC_REG(FRC_RO_DBG0_STAT);
		tmpstats->reg4dh.rawdat = val;
		val = READ_FRC_REG(FRC_RO_DBG1_STAT);
		tmpstats->reg4eh.rawdat = val;
		val = READ_FRC_REG(FRC_RO_DBG2_STAT);
		tmpstats->reg4fh.rawdat = val;
	} else if (checkflag == 1) {
		pr_frc(1, "[0x%x] ref_vs_dly_num = %d, ref_de_dly_num = %d\n",
			tmpstats->reg4dh.rawdat,
			tmpstats->reg4dh.bits.ref_vs_dly_num,
			tmpstats->reg4dh.bits.ref_de_dly_num);
		pr_frc(1, "[0x%x] mevp_dly_num = %d, mcout_dly_num = %d\n",
			tmpstats->reg4eh.rawdat,
			tmpstats->reg4eh.bits.mevp_dly_num,
			tmpstats->reg4eh.bits.mcout_dly_num);
		pr_frc(1, "[0x%x] memc_corr_st = %d, corr_dly_num = %d\n",
			tmpstats->reg4fh.rawdat,
			tmpstats->reg4fh.bits.memc_corr_st,
			tmpstats->reg4fh.bits.memc_corr_dly_num);
		pr_frc(1, "out_dly_err=%d, out_de_dly_num = %d\n",
			tmpstats->reg4fh.bits.out_dly_err,
			tmpstats->reg4fh.bits.out_de_dly_num);
	}
}

void frc_me_crc_read(struct frc_dev_s *frc_devp)
{
	struct frc_crc_data_s *crc_data;
	u32 val;

	crc_data = &frc_devp->frc_crc_data;
	if (crc_data->frc_crc_read) {
		val = READ_FRC_REG(INP_ME_WRMIF);
		crc_data->me_wr_crc.crc_done_flag = val & 0x1;
		if (crc_data->me_wr_crc.crc_en)
			crc_data->me_wr_crc.crc_data_cmp[0] = READ_FRC_REG(INP_ME_WRMIF_CRC1);
		else
			crc_data->me_wr_crc.crc_data_cmp[0] = 0;

		val = READ_FRC_REG(INP_ME_RDMIF);
		crc_data->me_rd_crc.crc_done_flag = val & 0x1;

		if (crc_data->me_rd_crc.crc_en) {
			crc_data->me_rd_crc.crc_data_cmp[0] = READ_FRC_REG(INP_ME_RDMIF_CRC1);
			crc_data->me_rd_crc.crc_data_cmp[1] = READ_FRC_REG(INP_ME_RDMIF_CRC2);
		} else {
			crc_data->me_rd_crc.crc_data_cmp[0] = 0;
			crc_data->me_rd_crc.crc_data_cmp[1] = 0;
		}
		if (crc_data->frc_crc_pr) {
			if (crc_data->me_wr_crc.crc_en && crc_data->me_rd_crc.crc_en)
				pr_frc(0,
					"invs_cnt = %d, mewr_done_flag = %d, mewr_cmp1 = 0x%x, merd_done_flag = %d, merd_cmp1 = 0x%x, merd_cmp2 = 0x%x\n",
					frc_devp->in_sts.vs_cnt,
					crc_data->me_wr_crc.crc_done_flag,
					crc_data->me_wr_crc.crc_data_cmp[0],
					crc_data->me_rd_crc.crc_done_flag,
					crc_data->me_rd_crc.crc_data_cmp[0],
					crc_data->me_rd_crc.crc_data_cmp[1]);
			else if (crc_data->me_wr_crc.crc_en)
				pr_frc(0,
					"invs_cnt = %d, mewr_done_flag = %d, mewr_cmp1 = 0x%x\n",
					frc_devp->in_sts.vs_cnt,
					crc_data->me_wr_crc.crc_done_flag,
					crc_data->me_wr_crc.crc_data_cmp[0]);
			else if (crc_data->me_rd_crc.crc_en)
				pr_frc(0,
					"invs_cnt = %d, merd_done_flag = %d, merd_cmp1 = 0x%x, merd_cmp2 = 0x%x\n",
					frc_devp->in_sts.vs_cnt,
					crc_data->me_rd_crc.crc_done_flag,
					crc_data->me_rd_crc.crc_data_cmp[0],
					crc_data->me_rd_crc.crc_data_cmp[1]);
		}
	}
}

void frc_mc_crc_read(struct frc_dev_s *frc_devp)
{
	struct frc_crc_data_s *crc_data;
	u32 val;

	crc_data = &frc_devp->frc_crc_data;
	if (crc_data->frc_crc_read) {
		val = READ_FRC_REG(INP_MC_WRMIF);
		crc_data->mc_wr_crc.crc_done_flag = val & 0x1;
		if (crc_data->mc_wr_crc.crc_en) {
			crc_data->mc_wr_crc.crc_data_cmp[0] = READ_FRC_REG(INP_MC_WRMIF_CRC1);
			crc_data->mc_wr_crc.crc_data_cmp[1] = READ_FRC_REG(INP_MC_WRMIF_CRC2);
		} else {
			crc_data->mc_wr_crc.crc_data_cmp[0] = 0;
			crc_data->mc_wr_crc.crc_data_cmp[1] = 0;
		}
		if (crc_data->frc_crc_pr && crc_data->mc_wr_crc.crc_en)
			pr_frc(0,
			"outvs_cnt = %d, mcwr_done_flag = %d, mcwr_cmp1 = 0x%x, mcwr_cmp2 = 0x%x\n",
				frc_devp->out_sts.vs_cnt,
				crc_data->mc_wr_crc.crc_done_flag,
				crc_data->mc_wr_crc.crc_data_cmp[0],
				crc_data->mc_wr_crc.crc_data_cmp[1]);
	}
}

void inp_undone_read(struct frc_dev_s *frc_devp)
{
	u32 inp_ud_flag, readval, timeout;

	if (!frc_devp)
		return;
	if (!frc_devp->probe_ok || !frc_devp->power_on_flag)
		return;
	if (frc_devp->frc_sts.re_config)
		return;
	if (frc_devp->frc_sts.state != FRC_STATE_ENABLE ||
		frc_devp->frc_sts.new_state != FRC_STATE_ENABLE)
		return;
	inp_ud_flag = READ_FRC_REG(FRC_INP_UE_DBG) & 0x3f;
	if (inp_ud_flag != 0) {
		WRITE_FRC_REG_BY_CPU(FRC_INP_UE_CLR, 0x3f);
		WRITE_FRC_REG_BY_CPU(FRC_INP_UE_CLR, 0x0);
		frc_devp->ud_dbg.inp_undone_err = inp_ud_flag;
		frc_devp->frc_sts.inp_undone_cnt++;
		if (frc_devp->ud_dbg.inp_ud_dbg_en != 0) {
			if (frc_devp->frc_sts.inp_undone_cnt % 0x30 == 0) {
				PR_ERR("inp_ud_err=0x%x,err_cnt=%d,vs_cnt=%d\n",
					inp_ud_flag,
					frc_devp->frc_sts.inp_undone_cnt,
					frc_devp->in_sts.vs_cnt);
			}
		}
		timeout = 0;
		do {
			readval = READ_FRC_REG(FRC_INP_UE_CLR) & 0x3f;
			if (readval == 0)
				break;
		} while (timeout++ < 100);
		if (frc_devp->ud_dbg.res1_time_en == 1) {
			if (frc_devp->frc_sts.inp_undone_cnt ==
					0x30 * 2) {
				frc_devp->frc_sts.re_config = true;
				PR_ERR("frc will reopen\n");
			}
		} else if (frc_devp->ud_dbg.res1_time_en == 2) {
			if (frc_devp->frc_sts.inp_undone_cnt ==
					0x30 * 3) {
				frc_devp->frc_sts.re_config = true;
				PR_ERR("frc will reopen\n");
			}
		} else if (frc_devp->ud_dbg.res1_time_en == 0) {
			frc_devp->frc_sts.re_config = false;
		}
	} else {
		frc_devp->ud_dbg.inp_undone_err = 0;
		frc_devp->frc_sts.inp_undone_cnt = 0;
	}
}

void me_undone_read(struct frc_dev_s *frc_devp)
{
	u32 me_ud_flag;

	if (!frc_devp)
		return;
	if (!frc_devp->probe_ok || !frc_devp->power_on_flag)
		return;
	if (frc_devp->ud_dbg.meud_dbg_en) {
		me_ud_flag = READ_FRC_REG(FRC_MEVP_RO_STAT0) & BIT_0;
		if (me_ud_flag != 0) {
			frc_devp->frc_sts.me_undone_cnt++;
			frc_devp->ud_dbg.me_undone_err = 1;
			WRITE_FRC_BITS(FRC_MEVP_CTRL0, 1, 31, 1);
			WRITE_FRC_BITS(FRC_MEVP_CTRL0, 0, 31, 1);
			if (frc_devp->frc_sts.me_undone_cnt >= MAX_ME_UNDONE_CNT)
				pr_frc(0, "outvs_cnt = %d, me_ud_err cnt= %d\n",
					frc_devp->out_sts.vs_cnt,
					frc_devp->frc_sts.me_undone_cnt);
		} else {
			frc_devp->ud_dbg.me_undone_err = 0;
			frc_devp->frc_sts.me_undone_cnt = 0;
		}
	}
}

void mc_undone_read(struct frc_dev_s *frc_devp)
{
	u32 val, mc_ud_flag;

	if (!frc_devp)
		return;
	if (!frc_devp->probe_ok || !frc_devp->power_on_flag)
		return;
	if (frc_devp->ud_dbg.mcud_dbg_en) {
		val = READ_FRC_REG(FRC_MC_DBG_MC_WRAP);
		mc_ud_flag = (val >> 24) & 0x1;
		if (mc_ud_flag != 0) {
			frc_devp->frc_sts.mc_undone_cnt++;
			frc_devp->ud_dbg.mc_undone_err = 1;
			WRITE_FRC_BITS(FRC_MC_HW_CTRL0, 1, 21, 1);
			WRITE_FRC_BITS(FRC_MC_HW_CTRL0, 0, 21, 1);
			if (frc_devp->frc_sts.me_undone_cnt >= MAX_MC_UNDONE_CNT)
				pr_frc(0, "outvs_cnt = %d, mc_ud_err cnt= %d\n",
					frc_devp->out_sts.vs_cnt,
					frc_devp->frc_sts.mc_undone_cnt);

		} else {
			frc_devp->ud_dbg.mc_undone_err = 0;
			frc_devp->frc_sts.mc_undone_cnt = 0;
		}
	}
}

void vp_undone_read(struct frc_dev_s *frc_devp)
{
	u32 vp_ud_flag;

	if (!frc_devp)
		return;
	if (!frc_devp->probe_ok || !frc_devp->power_on_flag)
		return;
	if (frc_devp->ud_dbg.vpud_dbg_en) {
		vp_ud_flag = READ_FRC_REG(FRC_VP_TOP_STAT) & 0x03;
		if (vp_ud_flag != 0) {
			frc_devp->frc_sts.vp_undone_cnt++;
			frc_devp->ud_dbg.vp_undone_err = vp_ud_flag;
			WRITE_FRC_BITS(FRC_VP_TOP_CLR_STAT, 3, 0, 2);
			WRITE_FRC_BITS(FRC_VP_TOP_CLR_STAT, 0, 0, 2);
			if (frc_devp->frc_sts.vp_undone_cnt >= MAX_VP_UNDONE_CNT)
				pr_frc(0, "outvs_cnt=%d, vperr=%x, errcnt=%d\n",
					frc_devp->out_sts.vs_cnt,
					frc_devp->ud_dbg.vp_undone_err,
					frc_devp->frc_sts.vp_undone_cnt);
		} else {
			frc_devp->frc_sts.vp_undone_cnt = 0;
			frc_devp->ud_dbg.vp_undone_err = 0;
		}
	}
}

const char * const mtx_str[] = {
	"FRC_INPUT_CSC",
	"FRC_OUTPUT_CSC"
};

const char * const csc_str[] = {
	"CSC_OFF",
	"RGB_YUV709L",
	"RGB_YUV709F",
	"YUV709L_RGB",
	"YUV709F_RGB"
};

void frc_mtx_cfg(enum frc_mtx_e mtx_sel, enum frc_mtx_csc_e mtx_csc)
{
	unsigned int pre_offset01, pre_offset2;
	unsigned int pst_offset01, pst_offset2;
	unsigned int coef00_01, coef02_10, coef11_12, coef20_21, coef22;
	unsigned int en = 1;

	switch (mtx_csc) {
	case CSC_OFF:
		en = 0;
		break;
	case RGB_YUV709L:
		coef00_01 = 0x00bb0275;
		coef02_10 = 0x003f1f99;
		coef11_12 = 0x1ea601c2;
		coef20_21 = 0x01c21e67;
		coef22 = 0x00001fd7;
		pre_offset01 = 0x0;
		pre_offset2 = 0x0;
		pst_offset01 = 0x00400200;
		pst_offset2 = 0x00000200;
		break;
	case RGB_YUV709F:
		coef00_01 = 0xda02dc;
		coef02_10 = 0x4a1f8b;
		coef11_12 = 0x1e750200;
		coef20_21 = 0x2001e2f;
		coef22 = 0x1fd0;
		pre_offset01 = 0x0;
		pre_offset2 = 0x0;
		pst_offset01 = 0x200;
		pst_offset2 = 0x200;
		break;
	case YUV709L_RGB:
		coef00_01 = 0x04A80000;
		coef02_10 = 0x072C04A8;
		coef11_12 = 0x1F261DDD;
		coef20_21 = 0x04A80876;
		coef22 = 0x0;
		pre_offset01 = 0x1fc01e00;
		pre_offset2 = 0x000001e00;
		pst_offset01 = 0x0;
		pst_offset2 = 0x0;
		break;
	case YUV709F_RGB:
		coef00_01 = 0x04000000;
		coef02_10 = 0x064D0400;
		coef11_12 = 0x1F411E21;
		coef20_21 = 0x0400076D;
		coef22 = 0x0;
		pre_offset01 = 0x000001e00;
		pre_offset2 = 0x000001e00;
		pst_offset01 = 0x0;
		pst_offset2 = 0x0;
		break;
	default:
		break;
	}

	if (!en) {
		if (mtx_sel == FRC_INPUT_CSC)
			WRITE_FRC_BITS(FRC_INP_CSC_CTRL, en, 3, 1);
		else if (mtx_sel == FRC_OUTPUT_CSC)
			WRITE_FRC_BITS(FRC_MC_CSC_CTRL, en, 3, 1);
		return;
	}

	switch (mtx_sel) {
	case FRC_INPUT_CSC:
		WRITE_FRC_REG_BY_CPU(FRC_INP_CSC_OFFSET_INP01, pre_offset01);
		WRITE_FRC_REG_BY_CPU(FRC_INP_CSC_OFFSET_INP2, pre_offset2);
		WRITE_FRC_REG_BY_CPU(FRC_INP_CSC_COEF_00_01, coef00_01);
		WRITE_FRC_REG_BY_CPU(FRC_INP_CSC_COEF_02_10, coef02_10);
		WRITE_FRC_REG_BY_CPU(FRC_INP_CSC_COEF_11_12, coef11_12);
		WRITE_FRC_REG_BY_CPU(FRC_INP_CSC_COEF_20_21, coef20_21);
		WRITE_FRC_REG_BY_CPU(FRC_INP_CSC_COEF_22, coef22);
		WRITE_FRC_REG_BY_CPU(FRC_INP_CSC_OFFSET_OUP01, pst_offset01);
		WRITE_FRC_REG_BY_CPU(FRC_INP_CSC_OFFSET_OUP2, pst_offset2);
		WRITE_FRC_BITS(FRC_INP_CSC_CTRL, en, 3, 1);
		break;
	case FRC_OUTPUT_CSC:
		WRITE_FRC_REG_BY_CPU(FRC_MC_CSC_OFFSET_INP01, pre_offset01);
		WRITE_FRC_REG_BY_CPU(FRC_MC_CSC_OFFSET_INP2, pre_offset2);
		WRITE_FRC_REG_BY_CPU(FRC_MC_CSC_COEF_00_01, coef00_01);
		WRITE_FRC_REG_BY_CPU(FRC_MC_CSC_COEF_02_10, coef02_10);
		WRITE_FRC_REG_BY_CPU(FRC_MC_CSC_COEF_11_12, coef11_12);
		WRITE_FRC_REG_BY_CPU(FRC_MC_CSC_COEF_20_21, coef20_21);
		WRITE_FRC_REG_BY_CPU(FRC_MC_CSC_COEF_22, coef22);
		WRITE_FRC_REG_BY_CPU(FRC_MC_CSC_OFFSET_OUP01, pst_offset01);
		WRITE_FRC_REG_BY_CPU(FRC_MC_CSC_OFFSET_OUP2, pst_offset2);
		WRITE_FRC_BITS(FRC_MC_CSC_CTRL, en, 3, 1);
		break;
	default:
		break;
	}
	pr_frc(1, "%s, mtx sel: %s,  mtx csc : %s\n",
		__func__, mtx_str[mtx_sel - 1], csc_str[mtx_csc]);
}

void frc_mtx_set(struct frc_dev_s *frc_devp)
{
	if (frc_devp->frc_hw_pos == FRC_POS_AFTER_POSTBLEND) {
		frc_mtx_cfg(FRC_INPUT_CSC, RGB_YUV709F);
		frc_mtx_cfg(FRC_OUTPUT_CSC, YUV709F_RGB);
		return;
	}

	if (frc_devp->frc_test_ptn) {
		frc_mtx_cfg(FRC_INPUT_CSC, RGB_YUV709L);
		frc_mtx_cfg(FRC_OUTPUT_CSC, CSC_OFF);
	} else {
		frc_mtx_cfg(FRC_INPUT_CSC, CSC_OFF);
		frc_mtx_cfg(FRC_OUTPUT_CSC, CSC_OFF);
	}
}

static void set_vd1_out_size(struct frc_dev_s *frc_devp)
{
	unsigned int hsize = 0;
	unsigned int vsize = 0;

	if (frc_devp->frc_hw_pos == FRC_POS_BEFORE_POSTBLEND) {
		if (frc_devp->force_size.force_en) {
			hsize = frc_devp->force_size.force_hsize - 1;
			vsize = frc_devp->force_size.force_vsize - 1;
			vpu_reg_write_bits(VD1_BLEND_SRC_CTRL, 1, 8, 4);
			vpu_reg_write_bits(VPP_POSTBLEND_VD1_H_START_END, hsize, 0, 13);
			vpu_reg_write_bits(VPP_POSTBLEND_VD1_V_START_END, vsize, 0, 13);
		}
	}
	pr_frc(1, "hsize = %d, vsize = %d\n", hsize, vsize);
}

void frc_pattern_on(u32 en)
{
	// WRITE_FRC_BITS(FRC_REG_INP_MODULE_EN, en, 6, 1);
	frc_config_reg_value(en << 6, 0x40, &regdata_inpmoden_04f9);
	WRITE_FRC_REG_BY_CPU(FRC_REG_INP_MODULE_EN, regdata_inpmoden_04f9);

	if (en == 1) {
		frc_mtx_cfg(FRC_INPUT_CSC, RGB_YUV709L);
		frc_mtx_cfg(FRC_OUTPUT_CSC, CSC_OFF);

	} else {
		frc_mtx_cfg(FRC_INPUT_CSC, CSC_OFF);
		frc_mtx_cfg(FRC_OUTPUT_CSC, CSC_OFF);
	}
}

static void frc_input_init(struct frc_dev_s *frc_devp,
	struct frc_top_type_s *frc_top)
{
	pr_frc(2, "%s\n", __func__);
	if (frc_devp->frc_test_ptn) {
		if (frc_devp->frc_hw_pos == FRC_POS_BEFORE_POSTBLEND &&
			frc_devp->force_size.force_en) {
			/*used for set testpattern size, only for debug*/
			frc_top->hsize = frc_devp->force_size.force_hsize;
			frc_top->vsize = frc_devp->force_size.force_vsize;
			frc_top->out_hsize = frc_devp->force_size.force_hsize;
			frc_top->out_vsize = frc_devp->force_size.force_vsize;
		} else {
			frc_top->hsize = frc_devp->out_sts.vout_width;
			frc_top->vsize = frc_devp->out_sts.vout_height;
			frc_top->out_hsize = frc_devp->out_sts.vout_width;
			frc_top->out_vsize = frc_devp->out_sts.vout_height;
		}
		frc_top->frc_ratio_mode = frc_devp->in_out_ratio;
		frc_top->film_mode = frc_devp->film_mode;
		/*hw film detect*/
		frc_top->film_hwfw_sel = frc_devp->film_mode_det;
		frc_top->frc_prot_mode = frc_devp->prot_mode;
		frc_top->frc_fb_num = FRC_TOTAL_BUF_NUM;

		set_vd1_out_size(frc_devp);
	} else {
		if (frc_devp->frc_hw_pos == FRC_POS_AFTER_POSTBLEND) {
			frc_top->hsize = frc_devp->out_sts.vout_width;
			frc_top->vsize = frc_devp->out_sts.vout_height;
		} else {
			frc_top->hsize = frc_devp->in_sts.in_hsize;
			frc_top->vsize = frc_devp->in_sts.in_vsize;
		}
		frc_top->out_hsize = frc_devp->out_sts.vout_width;
		frc_top->out_vsize = frc_devp->out_sts.vout_height;
		frc_top->frc_ratio_mode = frc_devp->in_out_ratio;
		frc_top->film_mode = frc_devp->film_mode;
		/*sw film detect*/
		frc_top->film_hwfw_sel = frc_devp->film_mode_det;
		frc_top->frc_prot_mode = frc_devp->prot_mode;
		frc_top->frc_fb_num = FRC_TOTAL_BUF_NUM;
	}
	pr_frc(2, "top in: hsize:%d, vsize:%d\n", frc_top->hsize, frc_top->vsize);
	pr_frc(2, "top out: hsize:%d, vsize:%d\n", frc_top->out_hsize, frc_top->out_vsize);
	if (frc_top->hsize == 0 || frc_top->vsize == 0)
		pr_frc(0, "err: size in hsize:%d, vsize:%d !!!!\n", frc_top->hsize, frc_top->vsize);
}

void frc_top_init(struct frc_dev_s *frc_devp)
{
	////Config frc ctrl timming
	u32 me_hold_line;//me_hold_line
	u32 mc_hold_line;//mc_hold_line
	u32 inp_hold_line;
	u32 reg_post_dly_vofst;//fixed
	u32 reg_mc_dly_vofst0 ;//fixed
	u32 reg_mc_out_line;
	u32 reg_me_dly_vofst;
	u32 mevp_frm_dly;//Read RO ro_mevp_dly_num
	u32 mc_frm_dly;//Read RO ro_mc2out_dly_num
	u32 memc_frm_dly;//total delay
	u32 reg_mc_dly_vofst1;
	u32 log = 2;
	u32 tmpvalue;
	u32 tmpvalue2;
	u32 frc_v_porch;
	u32 frc_vporch_cal;
	u32 frc_porch_delta;
	u32 adj_mc_dly;
	enum chip_id chip;

	struct frc_data_s *frc_data;
	struct frc_fw_data_s *fw_data;
	struct frc_top_type_s *frc_top;

	frc_data = (struct frc_data_s *)frc_devp->data;
	fw_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	frc_top = &fw_data->frc_top_type;

	me_hold_line = fw_data->holdline_parm.me_hold_line;
	mc_hold_line = fw_data->holdline_parm.mc_hold_line;
	inp_hold_line = fw_data->holdline_parm.inp_hold_line;
	reg_post_dly_vofst = fw_data->holdline_parm.reg_post_dly_vofst;
	reg_mc_dly_vofst0 = fw_data->holdline_parm.reg_mc_dly_vofst0;
	chip = frc_data->match_data->chip;

	tmpvalue = READ_FRC_REG(FRC_REG_TOP_RESERVE0);
	if ((tmpvalue & 0xFF) == 0)
		WRITE_FRC_BITS(FRC_REG_TOP_RESERVE0, 0x14, 0, 8);
	frc_input_init(frc_devp, frc_top);
	config_phs_lut(frc_top->frc_ratio_mode, frc_top->film_mode);
	config_phs_regs(frc_top->frc_ratio_mode, frc_top->film_mode);
	pr_frc(log, "%s\n", __func__);
	//Config frc input size
	tmpvalue = frc_top->hsize;
	tmpvalue |= (frc_top->vsize) << 16;
	WRITE_FRC_REG_BY_CPU(FRC_FRAME_SIZE, tmpvalue);
	/*!!!!!!!!!  vpu register*/
	frc_top->vfb = vpu_reg_read(ENCL_VIDEO_VAVON_BLINE);
	pr_frc(log, "ENCL_VIDEO_VAVON_BLINE:%d\n", frc_top->vfb);
	//(frc_top->vfb / 4) * 3; 3/4 point of front vblank, default
	reg_mc_out_line = frc_init_out_line();
	adj_mc_dly = frc_devp->out_line;    // from user debug

	// reg_me_dly_vofst = reg_mc_out_line;
	reg_me_dly_vofst = reg_mc_dly_vofst0;  // change for keep me first run
	if (frc_top->hsize <= 1920 && (frc_top->hsize * frc_top->vsize <= 1920 * 1080)) {
		frc_top->is_me1mc4 = 0;/*me:mc 1:2*/
		WRITE_FRC_REG_BY_CPU(FRC_INPUT_SIZE_ALIGN, 0x0);  //8*8 align
	} else {
		frc_top->is_me1mc4 = 1;/*me:mc 1:4*/
		WRITE_FRC_REG_BY_CPU(FRC_INPUT_SIZE_ALIGN, 0x3);   //16*16 align
	}
	// little window
	if (frc_top->hsize * frc_top->vsize <
		frc_top->out_hsize * frc_top->out_vsize &&
		frc_top->out_hsize * frc_top->out_vsize >= 3840 * 1080) {
		frc_top->is_me1mc4 = 1;/*me:mc 1:4*/
		WRITE_FRC_REG_BY_CPU(FRC_INPUT_SIZE_ALIGN, 0x3);   //16*16 align
		pr_frc(log, "me:mc 1:4\n");
	}
	/*The resolution of the input is not standard for test
	 * input size adjust within 8 lines and output size > 1920 * 1080
	 * default FRC_PARAM_NUM: 8 adjustable
	 */
	if (!frc_devp->in_sts.inp_size_adj_en &&
		frc_top->out_hsize - FRC_PARAM_NUM < frc_top->hsize &&
		frc_top->hsize <= frc_top->out_hsize &&
		frc_top->out_vsize - FRC_PARAM_NUM < frc_top->vsize &&
		frc_top->vsize < frc_top->out_vsize &&
		frc_top->out_hsize * frc_top->out_vsize >= 1920 * 1080) {
		frc_top->is_me1mc4 = 1;/*me:mc 1:4*/
		WRITE_FRC_REG_BY_CPU(FRC_INPUT_SIZE_ALIGN, 0x0);  //8*8 align
		pr_frc(log, " %s this input case is not standard\n", __func__);
	}

	if (frc_top->out_hsize == 1920 && frc_top->out_vsize == 1080) {
		mevp_frm_dly = 130;
		mc_frm_dly   = 11 ;//inp performace issue, need frc_clk >  enc0_clk
	} else if (frc_top->out_hsize == 3840 && frc_top->out_vsize == 2160) {
		mevp_frm_dly = 222; // reg readback  under 333MHz
		mc_frm_dly = 28;   // reg readback (14)  under 333MHz
		// mevp_frm_dly = 260;   // under 400MHz
		// mc_frm_dly = 28;      // under 400MHz
	} else if (frc_top->out_hsize == 3840 && frc_top->out_vsize == 1080) {
		reg_mc_out_line = (frc_top->vfb / 2) * 1;
		reg_me_dly_vofst = reg_mc_out_line;
		mevp_frm_dly = 222; // reg readback  under 333MHz
		mc_frm_dly = 20;   // reg readback (34)  under 333MHz
		pr_frc(log, "4k1k_mc_frm_dly:%d\n", mc_frm_dly);
	} else {
		mevp_frm_dly = 140;
		mc_frm_dly = 10;
	}

	//memc_frm_dly
	memc_frm_dly      = reg_me_dly_vofst + me_hold_line + mevp_frm_dly +
				mc_frm_dly  + mc_hold_line + adj_mc_dly;
	reg_mc_dly_vofst1 = memc_frm_dly - mc_frm_dly   - mc_hold_line ;
	frc_vporch_cal    = memc_frm_dly - reg_mc_out_line;
	WRITE_FRC_REG_BY_CPU(FRC_REG_TOP_CTRL27, frc_vporch_cal);

	if (chip == ID_T3 && is_meson_rev_a()) {
		if ((frc_top->out_hsize > 1920 && frc_top->out_vsize > 1080) ||
			(frc_top->out_hsize == 3840 && frc_top->out_vsize == 1080 &&
			frc_devp->out_sts.out_framerate > 80)) {
			/*
			 * MEMC 4K ENCL setting, vlock will change the ENCL_VIDEO_MAX_LNCNT,
			 * so need dynamic change this register
			 */
			//u32 frc_v_porch = vpu_reg_read(ENCL_FRC_CTRL);/*0x1cdd*/
			u32 max_lncnt   = vpu_reg_read(ENCL_VIDEO_MAX_LNCNT);/*0x1cbb*/
			u32 max_pxcnt   = vpu_reg_read(ENCL_VIDEO_MAX_PXCNT);/*0x1cb0*/

			pr_frc(log, "max_lncnt=%d max_pxcnt=%d\n", max_lncnt, max_pxcnt);

			//frc_v_porch     = max_lncnt > 1800 ? max_lncnt - 1800 : frc_vporch_cal;
			frc_v_porch = ((max_lncnt - frc_vporch_cal) <= 1950) ?
					frc_vporch_cal : (max_lncnt - 1950);

			gst_frc_param.frc_v_porch = frc_v_porch;
			gst_frc_param.max_lncnt = max_lncnt;
			gst_frc_param.max_pxcnt = max_pxcnt;
			gst_frc_param.frc_mcfixlines =
				mc_frm_dly + mc_hold_line - reg_mc_out_line;
			if (mc_frm_dly + mc_hold_line < reg_mc_out_line)
				gst_frc_param.frc_mcfixlines = 0;
			gst_frc_param.s2l_en = 1;
			if (vlock_sync_frc_vporch(gst_frc_param) < 0)
				pr_frc(0, "frc_on_set maxlnct fail !!!\n");
			else
				pr_frc(0, "frc_on_set maxlnct success!!!\n");

			//vpu_reg_write(ENCL_SYNC_TO_LINE_EN,
			// (1 << 13) | (max_lncnt - frc_v_porch));
			//vpu_reg_write(ENCL_SYNC_LINE_LENGTH, max_lncnt - frc_v_porch - 1);
			//vpu_reg_write(ENCL_SYNC_PIXEL_EN, (1 << 15) | (max_pxcnt - 1));

			pr_frc(log, "ENCL_SYNC_TO_LINE_EN=0x%x\n",
				vpu_reg_read(ENCL_SYNC_TO_LINE_EN));
			pr_frc(log, "ENCL_SYNC_PIXEL_EN=0x%x\n",
				vpu_reg_read(ENCL_SYNC_PIXEL_EN));
			pr_frc(log, "ENCL_SYNC_LINE_LENGTH=0x%x\n",
				vpu_reg_read(ENCL_SYNC_LINE_LENGTH));
		} else {
			frc_v_porch = frc_vporch_cal;
			gst_frc_param.frc_mcfixlines =
				mc_frm_dly + mc_hold_line - reg_mc_out_line;
			if (mc_frm_dly + mc_hold_line < reg_mc_out_line)
				gst_frc_param.frc_mcfixlines = 0;
			gst_frc_param.s2l_en = 0;
			if (vlock_sync_frc_vporch(gst_frc_param) < 0)
				pr_frc(0, "frc_infrom vlock fail !!!\n");
			else
				pr_frc(0, "frc_infrom vlock success!!!\n");
		}
		frc_porch_delta = frc_v_porch - frc_vporch_cal;
		pr_frc(log, "frc_v_porch=%d frc_porch_delta=%d\n",
			frc_v_porch, frc_porch_delta);
		vpu_reg_write_bits(ENCL_FRC_CTRL, frc_v_porch, 0, 16);
		tmpvalue = (reg_post_dly_vofst + frc_porch_delta);
		tmpvalue |= (reg_me_dly_vofst  + frc_porch_delta) << 16;
		WRITE_FRC_REG_BY_CPU(FRC_REG_TOP_CTRL14, tmpvalue);
		tmpvalue2 = (reg_mc_dly_vofst0 + frc_porch_delta);
		tmpvalue2 |= (reg_mc_dly_vofst1 + frc_porch_delta) << 16;
		WRITE_FRC_REG_BY_CPU(FRC_REG_TOP_CTRL15, tmpvalue2);
	} else {
		/*T3 revB*/
		frc_v_porch = frc_vporch_cal;
		pr_frc(2, "%s revB no care frc_vporch\n", __func__);
		gst_frc_param.frc_mcfixlines =
			mc_frm_dly + mc_hold_line - reg_mc_out_line;
		if (mc_frm_dly + mc_hold_line < reg_mc_out_line)
			gst_frc_param.frc_mcfixlines = 0;
		gst_frc_param.s2l_en = 2; /* rev B chip*/
		if (vlock_sync_frc_vporch(gst_frc_param) < 0)
			pr_frc(0, "frc_infrom vlock fail !!!\n");
		else
			pr_frc(0, "frc_infrom vlock success!!!\n");
		vpu_reg_write_bits(ENCL_FRC_CTRL, memc_frm_dly - reg_mc_out_line, 0, 16);
		// WRITE_FRC_BITS(FRC_REG_TOP_CTRL14, reg_post_dly_vofst, 0, 16);
		// WRITE_FRC_BITS(FRC_REG_TOP_CTRL14, reg_me_dly_vofst, 16, 16);
		// WRITE_FRC_BITS(FRC_REG_TOP_CTRL15, reg_mc_dly_vofst0, 0, 16);
		// WRITE_FRC_BITS(FRC_REG_TOP_CTRL15, reg_mc_dly_vofst1, 16,  16);
		WRITE_FRC_REG_BY_CPU(FRC_REG_TOP_CTRL14,
			reg_post_dly_vofst | (reg_me_dly_vofst << 16));
		WRITE_FRC_REG_BY_CPU(FRC_REG_TOP_CTRL15,
			reg_mc_dly_vofst0 | (reg_mc_dly_vofst1 << 16));
	}

	pr_frc(log, "reg_mc_out_line   = %d\n", reg_mc_out_line);
	pr_frc(log, "me_hold_line      = %d\n", me_hold_line);
	pr_frc(log, "mc_hold_line      = %d\n", mc_hold_line);
	pr_frc(log, "mevp_frm_dly      = %d\n", mevp_frm_dly);
	pr_frc(log, "adj_mc_dly        = %d\n", adj_mc_dly);
	pr_frc(log, "mc_frm_dly        = %d\n", mc_frm_dly);
	pr_frc(log, "memc_frm_dly      = %d\n", memc_frm_dly);
	pr_frc(log, "reg_mc_dly_vofst1 = %d\n", reg_mc_dly_vofst1);
	pr_frc(log, "frc_ratio_mode = %d\n", frc_top->frc_ratio_mode);
	pr_frc(log, "frc_fb_num = %d\n", frc_top->frc_fb_num);

	regdata_outholdctl_0003 = READ_FRC_REG(0x0003);
	regdata_inpholdctl_0002 = READ_FRC_REG(0x0002);
	frc_config_reg_value(me_hold_line, 0xff, &regdata_outholdctl_0003);
	frc_config_reg_value(mc_hold_line << 8, 0xff00, &regdata_outholdctl_0003);
	WRITE_FRC_REG_BY_CPU(FRC_OUT_HOLD_CTRL, regdata_outholdctl_0003);
	frc_config_reg_value(inp_hold_line, 0x1fff, &regdata_inpholdctl_0002);
	WRITE_FRC_REG_BY_CPU(FRC_INP_HOLD_CTRL, regdata_inpholdctl_0002);

	// sys_fw_param_frc_init(frc_top->hsize, frc_top->vsize, frc_top->is_me1mc4);
	// init_bb_xyxy(frc_top->hsize, frc_top->vsize, frc_top->is_me1mc4);

	/*protect mode, enable: memc delay 2 frame*/
	/*disable: memc delay n frame, n depend on cadence, for debug*/
	if (frc_top->frc_prot_mode) {
		regdata_top_ctl_0009 = READ_FRC_REG(0x0009);
		regdata_top_ctl_0011 = READ_FRC_REG(0x0011);
		frc_config_reg_value(0x01000000, 0x0F000000, &regdata_top_ctl_0009);
		WRITE_FRC_REG_BY_CPU(FRC_REG_TOP_CTRL9, regdata_top_ctl_0009); //dly_num =1
		frc_config_reg_value(0x100, 0x100, &regdata_top_ctl_0011);
		WRITE_FRC_REG_BY_CPU(FRC_REG_TOP_CTRL17, regdata_top_ctl_0011); //buf prot open
		WRITE_FRC_REG_BY_CPU(FRC_MODE_OPT, 0x6); // set bit1/bit2
	} else {
		WRITE_FRC_REG_BY_CPU(FRC_MODE_OPT, 0x0); // clear bit1/bit2
	}

}

/*buffer number can dynamic kmalloc,  film_hwfw_sel ????*/
void frc_inp_init(void)
{
	WRITE_FRC_REG_BY_CPU(FRC_FRAME_BUFFER_NUM, 16 << 8 | 16);
	WRITE_FRC_REG_BY_CPU(FRC_MC_PRB_CTRL1, 0x00640164); // bit8 set 1 close reg_mc_probe_en

	// WRITE_FRC_BITS(FRC_REG_INP_MODULE_EN, 1, 5, 1);//close reg_inp_logo_en
	// WRITE_FRC_BITS(FRC_REG_INP_MODULE_EN, 1, 8, 1);//open
	// WRITE_FRC_BITS(FRC_REG_INP_MODULE_EN, 1, 7, 1);//open  bbd en

	WRITE_FRC_REG_BY_CPU(FRC_REG_INP_MODULE_EN, 0x21be);//aligned padding value
	regdata_inpmoden_04f9 = READ_FRC_REG(FRC_REG_INP_MODULE_EN);

	WRITE_FRC_REG_BY_CPU(FRC_REG_TOP_CTRL25, 0x4080200); //aligned padding value

	// WRITE_FRC_BITS(FRC_MEVP_CTRL0, 0, 0, 1);//close hme
	WRITE_FRC_REG_BY_CPU(FRC_MEVP_CTRL0, 0xe);  //close hme

	/*for fw signal latch, should always enable*/
	//WRITE_FRC_BITS(FRC_REG_MODE_OPT, 1, 1, 1);
	WRITE_FRC_REG_BY_CPU(FRC_REG_MODE_OPT, 0x02);
	// WRITE_FRC_BITS(FRC_ME_E2E_CHK_EN - HME_ME_OFFSET, 0, 24,1 );//todo init hme
}

void config_phs_lut(enum frc_ratio_mode_type frc_ratio_mode,
	enum en_film_mode film_mode)
{
	#define lut_ary_size	18
	u32 input_n;
	u32 tmpregdata;
	u32 output_m;
	u64 phs_lut_table[lut_ary_size];
	int i;

	for (i = 0; i < lut_ary_size; i++)
		phs_lut_table[i] =  0xffffffffffffffff;

	if(frc_ratio_mode == FRC_RATIO_1_2) {
				  //      phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff mc_phase me_phase film_phase frc_phase
		//phs_lut_table[0] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h00   ,8'h40   ,8'h00     ,8'h00};
		//phs_lut_table[1] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h40   ,8'h40   ,8'h00     ,8'h01};
		phs_lut_table[0] = 0x13232100400000;
		phs_lut_table[1] = 0x03232140400001;
		input_n          = 1;
		output_m         = 2;
	}
	else if(frc_ratio_mode == FRC_RATIO_2_3) {
				 //      phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff mc_phase me_phase film_phase frc_phase
		//phs_lut_table[0] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h00   ,8'h55   ,8'h00     ,8'h00};
		//phs_lut_table[1] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h55   ,8'h55   ,8'h00     ,8'h01};
		//phs_lut_table[2] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h2a   ,8'h2a   ,8'h00     ,8'h02};
		phs_lut_table[0] = 0x13232100550000;
		phs_lut_table[1] = 0x03232155550001;
		phs_lut_table[2] = 0x1323212a2a0002;
		input_n          = 2;
		output_m         = 3;
	}
	else if(frc_ratio_mode == FRC_RATIO_2_5) {
				  //      phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff mc_phase me_phase film_phase frc_phase
		//phs_lut_table[0] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h00   ,8'h33   ,8'h00     ,8'h00};
		//phs_lut_table[1] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h33   ,8'h33   ,8'h00     ,8'h01};
		//phs_lut_table[2] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h66   ,8'h66   ,8'h00     ,8'h02};
		//phs_lut_table[3] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h19   ,8'h19   ,8'h00     ,8'h03};
		//phs_lut_table[4] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h4c   ,8'h4c   ,8'h00     ,8'h04};
		phs_lut_table[0] = 0x13232100330000;
		phs_lut_table[1] = 0x03232133330001;
		phs_lut_table[2] = 0x03232166660002;
		phs_lut_table[3] = 0x13232119190003;
		phs_lut_table[4] = 0x0323214c4c0004;
		input_n          = 2;
		output_m         = 5;
	}
	else if(frc_ratio_mode == FRC_RATIO_5_6) {
				 //      phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff mc_phase me_phase film_phase frc_phase
		//phs_lut_table[0] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h00   ,8'h6a   ,8'h00     ,8'h00};
		//phs_lut_table[1] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h6a   ,8'h6a   ,8'h00     ,8'h01};
		//phs_lut_table[2] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h55   ,8'h55   ,8'h00     ,8'h02};
		//phs_lut_table[3] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h40   ,8'h40   ,8'h00     ,8'h03};
		//phs_lut_table[4] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h2a   ,8'h2a   ,8'h00     ,8'h04};
		//phs_lut_table[5] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h15   ,8'h15   ,8'h00     ,8'h05};
		phs_lut_table[0] = 0x132321006a0000;
		phs_lut_table[1] = 0x0323216a6a0001;
		phs_lut_table[2] = 0x13232155550002;
		phs_lut_table[3] = 0x13232140400003;
		phs_lut_table[4] = 0x1323212a2a0004;
		phs_lut_table[5] = 0x13232115150005;
		input_n          = 5;
		output_m         = 6;
	}
	else if(frc_ratio_mode == FRC_RATIO_5_12) {
				 //      phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff mc_phase me_phase film_phase frc_phase
		//phs_lut_table[0] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h00   ,8'h35   ,8'h00     ,8'h00};
		//phs_lut_table[1] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h35   ,8'h35   ,8'h00     ,8'h01};
		//phs_lut_table[2] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h6a   ,8'h6a   ,8'h00     ,8'h02};
		//phs_lut_table[3] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h20   ,8'h20   ,8'h00     ,8'h03};
		//phs_lut_table[4] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h55   ,8'h55   ,8'h00     ,8'h04};
		//phs_lut_table[5] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h0a   ,8'h0a   ,8'h00     ,8'h05};
		//phs_lut_table[6] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h40   ,8'h40   ,8'h00     ,8'h06};
		//phs_lut_table[7] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h75   ,8'h75   ,8'h00     ,8'h07};
		//phs_lut_table[8] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h2a   ,8'h2a   ,8'h00     ,8'h08};
		//phs_lut_table[9] = {1'h0     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h60   ,8'h60   ,8'h00     ,8'h09};
		//phs_lut_table[10]= {1'h1     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h15   ,8'h15   ,8'h00     ,8'h0a};
		//phs_lut_table[11]= {1'h0     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h4a   ,8'h4a   ,8'h00     ,8'h0b};
		phs_lut_table[0] = 0x13232100350000;
		phs_lut_table[1] = 0x03232135350001;
		phs_lut_table[2] = 0x0323216a6a0002;
		phs_lut_table[3] = 0x13232120200003;
		phs_lut_table[4] = 0x03232155550004;
		phs_lut_table[5] = 0x1323210a0a0005;
		phs_lut_table[6] = 0x03232140400006;
		phs_lut_table[7] = 0x03232175750007;
		phs_lut_table[8] = 0x1323212a2a0008;
		phs_lut_table[9] = 0x03232160600009;
		phs_lut_table[10]= 0x1323211515000a;
		phs_lut_table[11]= 0x0323214a4a000b;
		input_n          = 5;
		output_m         = 12;
	}
	else if(frc_ratio_mode == FRC_RATIO_1_1) {
		if(film_mode == EN_VIDEO) {
					  //      phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff mc_phase me_phase film_phase frc_phase
			//phs_lut_table[0] = {1'h1     ,4'h3      ,4'h2      ,4'h3     ,4'h2     ,4'h1     ,8'h00   ,8'h00  ,8'h00     ,8'h00};
			phs_lut_table[0] = 0x13232100000000;
		}
		//else if(film_mode == EN_FILM22) {
		//             //      phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff mc_phase me_phase film_phase frc_phase
		//    phs_lut_table[0] = {1'h1     ,4'h3      ,4'h2      ,4'h4     ,4'h3     ,4'h1     ,8'd0   ,8'd64   ,8'h01     ,8'h00};
		//    phs_lut_table[1] = {1'h0     ,4'h3      ,4'h2      ,4'h5     ,4'h3     ,4'h2     ,8'd64  ,8'd64   ,8'h00     ,8'h00};
		//}
		else if(film_mode == EN_FILM32) {
					 //      phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff mc_phase me_phase film_phase frc_phase
			//phs_lut_table[0] = {1'h1     ,4'h3      ,4'h2      ,4'h5     ,4'h4     ,4'h1     ,8'h00   ,8'h33   ,8'h01     ,8'h00};
			//phs_lut_table[1] = {1'h0     ,4'h3      ,4'h2      ,4'h6     ,4'h4     ,4'h2     ,8'h33   ,8'h33   ,8'h02     ,8'h00};
			//phs_lut_table[2] = {1'h0     ,4'h4      ,4'h3      ,4'h7     ,4'h4     ,4'h3     ,8'h66   ,8'h66   ,8'h03     ,8'h00};
			//phs_lut_table[3] = {1'h1     ,4'h3      ,4'h2      ,4'h5     ,4'h4     ,4'h2     ,8'h19   ,8'h19   ,8'h04     ,8'h00};
			//phs_lut_table[4] = {1'h0     ,4'h3      ,4'h2      ,4'h6     ,4'h4     ,4'h3     ,8'h4c   ,8'h4c   ,8'h00     ,8'h00};
			phs_lut_table[0] = 0x13254100330100;
			phs_lut_table[1] = 0x03264233330200;
			phs_lut_table[2] = 0x04374366660300;
			phs_lut_table[3] = 0x13254219190400;
			phs_lut_table[4] = 0x0326434c4c0000;
		}
		//else if(film_mode == EN_FILM3223) {
		//             //      phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff mc_phase me_phase film_phase frc_phase
		//    phs_lut_table[0] = { 1'd1,   4'd3,      4'd2,      4'd6,     4'd4,      4'd1,     8'd0,   8'd51,     8'd1,    8'h00};
		//    phs_lut_table[1] = { 1'd0,   4'd3,      4'd2,      4'd7,     4'd4,      4'd2,     8'd51,  8'd51,     8'd2,    8'h00};
		//    phs_lut_table[2] = { 1'd0,   4'd4,      4'd3,      4'd8,     4'd4,      4'd3,     8'd102, 8'd102,    8'd3,    8'h00};
		//    phs_lut_table[3] = { 1'd1,   4'd3,      4'd2,      4'd5,     4'd4,      4'd2,     8'd25,  8'd25,     8'd4,    8'h00};
		//    phs_lut_table[4] = { 1'd0,   4'd4,      4'd3,      4'd6,     4'd4,      4'd3,     8'd76,  8'd76,     8'd5,    8'h00};
		//    phs_lut_table[5] = { 1'd1,   4'd3,      4'd2,      4'd5,     4'd4,      4'd2,     8'd0,   8'd51,     8'd6,    8'h00};
		//    phs_lut_table[6] = { 1'd0,   4'd3,      4'd2,      4'd6,     4'd4,      4'd3,     8'd51,  8'd51,     8'd7,    8'h00};
		//    phs_lut_table[7] = { 1'd0,   4'd4,      4'd3,      4'd7,     4'd5,      4'd4,     8'd102, 8'd102,    8'd8,    8'h00};
		//    phs_lut_table[8] = { 1'd1,   4'd3,      4'd2,      4'd6,     4'd5,      4'd2,     8'd25,  8'd25,     8'd9,    8'h00};
		//    phs_lut_table[9] = { 1'd0,   4'd3,      4'd2,      4'd7,     4'd5,      4'd3,     8'd76,  8'd76,     8'd0,    8'h00};
		//}
		//else if(film_mode == EN_FILM2224) {
		//             //      phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff mc_phase me_phase film_phase frc_phase
		//    phs_lut_table[0] = { 1'd1,   4'd3,      4'd2,      4'd6,     4'd5,     4'd1,    8'd0,    8'd51,     8'd1,    8'h00};
		//    phs_lut_table[1] = { 1'd0,   4'd3,      4'd2,      4'd7,     4'd5,     4'd2,    8'd51,   8'd51,     8'd2,    8'h00};
		//    phs_lut_table[2] = { 1'd0,   4'd4,      4'd3,      4'd8,     4'd5,     4'd3,    8'd102,  8'd102,    8'd3,    8'h00};
		//    phs_lut_table[3] = { 1'd1,   4'd3,      4'd2,      4'd6,     4'd4,     4'd2,    8'd25,   8'd25,     8'd4,    8'h00};
		//    phs_lut_table[4] = { 1'd0,   4'd4,      4'd3,      4'd7,     4'd4,     4'd3,    8'd76,   8'd76,     8'd5,    8'h00};
		//    phs_lut_table[5] = { 1'd1,   4'd3,      4'd2,      4'd5,     4'd4,     4'd2,    8'd0,    8'd51,     8'd6,    8'h00};
		//    phs_lut_table[6] = { 1'd0,   4'd4,      4'd3,      4'd6,     4'd4,     4'd3,    8'd51,   8'd51,     8'd7,    8'h00};
		//    phs_lut_table[7] = { 1'd0,   4'd4,      4'd3,      4'd7,     4'd5,     4'd4,    8'd102,  8'd102,    8'd8,    8'h00};
		//    phs_lut_table[8] = { 1'd1,   4'd4,      4'd3,      4'd6,     4'd5,     4'd3,    8'd25,   8'd25,     8'd9,    8'h00};
		//    phs_lut_table[9] = { 1'd0,   4'd4,      4'd3,      4'd7,     4'd5,     4'd4,    8'd76,   8'd76,     8'd0,    8'h00};
		//}
		//else if(film_mode == EN_FILM32322) {
		//             //      phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff mc_phase me_phase film_phase frc_phase
		//    phs_lut_table[0] =  { 1'd1,   4'd3,      4'd2,      4'd5,     4'd4,    4'd1,     8'd0,   8'd53,     8'd1,      8'h00};
		//    phs_lut_table[1] =  { 1'd0,   4'd3,      4'd2,      4'd6,     4'd4,    4'd2,     8'd53,   8'd53,     8'd2,     8'h00};
		//    phs_lut_table[2] =  { 1'd0,   4'd4,      4'd3,      4'd7,     4'd4,    4'd3,     8'd106,   8'd106,     8'd3,   8'h00};
		//    phs_lut_table[3] =  { 1'd1,   4'd3,      4'd2,      4'd5,     4'd4,    4'd2,     8'd32,   8'd32,     8'd4,     8'h00};
		//    phs_lut_table[4] =  { 1'd0,   4'd4,      4'd3,      4'd6,     4'd4,    4'd3,     8'd85,   8'd85,     8'd5,     8'h00};
		//    phs_lut_table[5] =  { 1'd1,   4'd3,      4'd2,      4'd5,     4'd4,    4'd2,     8'd10,   8'd10,     8'd6,     8'h00};
		//    phs_lut_table[6] =  { 1'd0,   4'd3,      4'd2,      4'd6,     4'd4,    4'd3,     8'd64,   8'd64,     8'd7,     8'h00};
		//    phs_lut_table[7] =  { 1'd0,   4'd4,      4'd3,      4'd7,     4'd5,    4'd4,     8'd117,   8'd117,     8'd8,   8'h00};
		//    phs_lut_table[8] =  { 1'd1,   4'd3,      4'd2,      4'd6,     4'd5,    4'd2,     8'd42,   8'd42,     8'd9,     8'h00};
		//    phs_lut_table[9] =  { 1'd0,   4'd4,      4'd3,      4'd7,     4'd5,    4'd3,     8'd96,   8'd96,     8'd10,    8'h00};
		//    phs_lut_table[10] = { 1'd1,   4'd3,      4'd2,      4'd6,     4'd4,    4'd2,     8'd21,   8'd21,     8'd11,    8'h00};
		//    phs_lut_table[11] = { 1'd0,   4'd3,      4'd2,      4'd7,     4'd4,    4'd3,     8'd74,   8'd74,     8'd0,     8'h00};
		//}
		//else if(film_mode == EN_FILM44) {
		//             //      phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff mc_phase me_phase film_phase frc_phase
		//    phs_lut_table[0] =  { 1'd1,   4'd3,      4'd2,      4'd6,     4'd5,    4'd1,     8'd0,   8'd32,     8'd1,    8'h00};
		//    phs_lut_table[1] =  { 1'd0,   4'd3,      4'd2,      4'd7,     4'd5,    4'd2,     8'd32,   8'd32,     8'd2,   8'h00};
		//    phs_lut_table[2] =  { 1'd0,   4'd3,      4'd2,      4'd8,     4'd5,    4'd3,     8'd64,   8'd64,     8'd3,   8'h00};
		//    phs_lut_table[3] =  { 1'd0,   4'd3,      4'd2,      4'd9,     4'd5,    4'd4,     8'd96,   8'd96,     8'd0,   8'h00};
		//}
		//else if(film_mode == EN_FILM21111) {
		//             //      phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff mc_phase me_phase film_phase frc_phase
		//    phs_lut_table[0] =  { 1'd1,   4'd3,      4'd2,      4'd4,     4'd3,    4'd1,     8'd0,   8'd106,     8'd1,     8'h00};
		//    phs_lut_table[1] =  { 1'd0,   4'd4,      4'd3,      4'd5,     4'd3,    4'd2,     8'd106,   8'd106,     8'd2,   8'h00};
		//    phs_lut_table[2] =  { 1'd1,   4'd4,      4'd3,      4'd4,     4'd3,    4'd2,     8'd85,   8'd85,     8'd3,     8'h00};
		//    phs_lut_table[3] =  { 1'd1,   4'd4,      4'd3,      4'd4,     4'd3,    4'd2,     8'd64,   8'd64,     8'd4,     8'h00};
		//    phs_lut_table[4] =  { 1'd1,   4'd4,      4'd3,      4'd4,     4'd3,    4'd2,     8'd42,   8'd42,     8'd5,     8'h00};
		//    phs_lut_table[5] =  { 1'd1,   4'd3,      4'd2,      4'd4,     4'd3,    4'd2,     8'd21,   8'd21,     8'd0,     8'h00};
		//}
		//else if(film_mode == EN_FILM23322) {
		//             //      phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff mc_phase me_phase film_phase frc_phase
		//    phs_lut_table[0] =  { 1'd1,   4'd3,      4'd2,      4'd6,     4'd4,    4'd1,     8'd0,   8'd53,     8'd1,       8'h00};
		//    phs_lut_table[1] =  { 1'd0,   4'd3,      4'd2,      4'd7,     4'd4,    4'd2,     8'd53,   8'd53,     8'd2,      8'h00};
		//    phs_lut_table[2] =  { 1'd0,   4'd4,      4'd3,      4'd8,     4'd4,    4'd3,     8'd106,   8'd106,     8'd3,    8'h00};
		//    phs_lut_table[3] =  { 1'd1,   4'd3,      4'd2,      4'd5,     4'd4,    4'd2,     8'd32,   8'd32,     8'd4,      8'h00};
		//    phs_lut_table[4] =  { 1'd0,   4'd4,      4'd3,      4'd6,     4'd4,    4'd3,     8'd85,   8'd85,     8'd5,      8'h00};
		//    phs_lut_table[5] =  { 1'd1,   4'd3,      4'd2,      4'd5,     4'd4,    4'd2,     8'd10,   8'd10,     8'd6,      8'h00};
		//    phs_lut_table[6] =  { 1'd0,   4'd4,      4'd3,      4'd6,     4'd4,    4'd3,     8'd64,   8'd64,     8'd7,      8'h00};
		//    phs_lut_table[7] =  { 1'd0,   4'd4,      4'd3,      4'd7,     4'd5,    4'd4,     8'd117,   8'd117,     8'd8,    8'h00};
		//    phs_lut_table[8] =  { 1'd1,   4'd3,      4'd2,      4'd6,     4'd5,    4'd3,     8'd42,   8'd42,     8'd9,      8'h00};
		//    phs_lut_table[9] =  { 1'd0,   4'd4,      4'd3,      4'd7,     4'd5,    4'd4,     8'd96,   8'd96,     8'd10,     8'h00};
		//    phs_lut_table[10] = { 1'd1,   4'd3,      4'd2,      4'd6,     4'd5,    4'd2,     8'd21,   8'd21,     8'd11,     8'h00};
		//    phs_lut_table[11] = { 1'd0,   4'd3,      4'd2,      4'd7,     4'd5,    4'd3,     8'd74,   8'd74,     8'd0,      8'h00};
		//}
		//else if(film_mode == EN_FILM2111) {
		//             //      phs_start plogo_diff clogo_diff pfrm_diff cfrm_diff nfrm_diff mc_phase me_phase film_phase frc_phase
		//    phs_lut_table[0] =  { 1'd1,   4'd3,      4'd2,      4'd4,     4'd3,    4'd1,     8'd0,   8'd102,     8'd1,      8'h00};
		//    phs_lut_table[1] =  { 1'd0,   4'd4,      4'd3,      4'd5,     4'd3,    4'd2,     8'd102,   8'd102,     8'd2,    8'h00};
		//    phs_lut_table[2] =  { 1'd1,   4'd4,      4'd3,      4'd4,     4'd3,    4'd2,     8'd76,   8'd76,     8'd3,      8'h00};
		//    phs_lut_table[3] =  { 1'd1,   4'd4,      4'd3,      4'd4,     4'd3,    4'd2,     8'd51,   8'd51,     8'd4,      8'h00};
		//    phs_lut_table[4] =  { 1'd1,   4'd3,      4'd2,      4'd4,     4'd3,    4'd2,     8'd25,   8'd25,     8'd0,      8'h00};
		//}

		//else if(film_mode == EN_FILM22224) {
		//    //cadence=22224(10),table_cnt=  12,match_data=0xaa8
		//    //    pha_start,plogo_diff,clogo_diff,pfrm_diff,cfrm_diff,nfrm_diff,mc_phase,me_phase,film_phase,frc_phase
		//    phs_lut_table[0]={ 1'd1,   4'd3,      4'd2,      4'd6,     4'd5,    4'd1,     8'd0,   8'd53,     8'd1,       8'd0};
		//    phs_lut_table[1]={ 1'd0,   4'd3,      4'd2,      4'd7,     4'd5,    4'd2,     8'd53,   8'd53,     8'd2,       8'd0};
		//    phs_lut_table[2]={ 1'd0,   4'd4,      4'd3,      4'd8,     4'd5,    4'd3,     8'd106,   8'd106,     8'd3,       8'd0};
		//    phs_lut_table[3]={ 1'd1,   4'd3,      4'd2,      4'd6,     4'd4,    4'd2,     8'd32,   8'd32,     8'd4,       8'd0};
		//    phs_lut_table[4]={ 1'd0,   4'd4,      4'd3,      4'd7,     4'd4,    4'd3,     8'd85,   8'd85,     8'd5,       8'd0};
		//    phs_lut_table[5]={ 1'd1,   4'd3,      4'd2,      4'd5,     4'd4,    4'd2,     8'd10,   8'd10,     8'd6,       8'd0};
		//    phs_lut_table[6]={ 1'd0,   4'd4,      4'd3,      4'd6,     4'd4,    4'd3,     8'd64,   8'd64,     8'd7,       8'd0};
		//    phs_lut_table[7]={ 1'd0,   4'd4,      4'd3,      4'd7,     4'd5,    4'd4,     8'd117,   8'd117,     8'd8,       8'd0};
		//    phs_lut_table[8]={ 1'd1,   4'd4,      4'd3,      4'd6,     4'd5,    4'd3,     8'd42,   8'd42,     8'd9,       8'd0};
		//    phs_lut_table[9]={ 1'd0,   4'd4,      4'd3,      4'd7,     4'd5,    4'd4,     8'd96,   8'd96,     8'd10,       8'd0};
		//    phs_lut_table[10]={ 1'd1,   4'd3,      4'd2,      4'd6,     4'd5,    4'd3,     8'd21,   8'd21,     8'd11,       8'd0};
		//    phs_lut_table[11]={ 1'd0,   4'd3,      4'd2,      4'd7,     4'd5,    4'd4,     8'd74,   8'd74,     8'd0,       8'd0};
		//}
		//else if(film_mode == EN_FILM33) {
		//    //cadence=33(11),table_cnt=   3,match_data=0x4
		//    //    pha_start,plogo_diff,clogo_diff,pfrm_diff,cfrm_diff,nfrm_diff,mc_phase,me_phase,film_phase,frc_phase
		//    phs_lut_table[0]={ 1'd1,   4'd3,      4'd2,      4'd5,     4'd4,    4'd1,     8'd0,   8'd42,     8'd1,       8'd0};
		//    phs_lut_table[1]={ 1'd0,   4'd3,      4'd2,      4'd6,     4'd4,    4'd2,     8'd42,   8'd42,     8'd2,       8'd0};
		//    phs_lut_table[2]={ 1'd0,   4'd3,      4'd2,      4'd7,     4'd4,    4'd3,     8'd85,   8'd85,     8'd0,       8'd0};
		//}
		//else if(film_mode == EN_FILM334) {
		//    //cadence=334(12),table_cnt=  10,match_data=0x248
		//    //    pha_start,plogo_diff,clogo_diff,pfrm_diff,cfrm_diff,nfrm_diff,mc_phase,me_phase,film_phase,frc_phase
		//    phs_lut_table[0]={ 1'd1,   4'd3,      4'd2,      4'd6,     4'd5,    4'd1,     8'd0,   8'd38,     8'd1,       8'd0};
		//    phs_lut_table[1]={ 1'd0,   4'd3,      4'd2,      4'd7,     4'd5,    4'd2,     8'd38,   8'd38,     8'd2,       8'd0};
		//    phs_lut_table[2]={ 1'd0,   4'd3,      4'd2,      4'd8,     4'd5,    4'd3,     8'd76,   8'd76,     8'd3,       8'd0};
		//    phs_lut_table[3]={ 1'd0,   4'd4,      4'd3,      4'd9,     4'd5,    4'd4,     8'd115,   8'd115,     8'd4,       8'd0};
		//    phs_lut_table[4]={ 1'd1,   4'd3,      4'd2,      4'd6,     4'd5,    4'd2,     8'd25,   8'd25,     8'd5,       8'd0};
		//    phs_lut_table[5]={ 1'd0,   4'd3,      4'd2,      4'd7,     4'd5,    4'd3,     8'd64,   8'd64,     8'd6,       8'd0};
		//    phs_lut_table[6]={ 1'd0,   4'd4,      4'd3,      4'd8,     4'd5,    4'd4,     8'd102,   8'd102,     8'd7,       8'd0};
		//    phs_lut_table[7]={ 1'd1,   4'd3,      4'd2,      4'd6,     4'd5,    4'd2,     8'd12,   8'd12,     8'd8,       8'd0};
		//    phs_lut_table[8]={ 1'd0,   4'd3,      4'd2,      4'd7,     4'd5,    4'd3,     8'd51,   8'd51,     8'd9,       8'd0};
		//    phs_lut_table[9]={ 1'd0,   4'd3,      4'd2,      4'd8,     4'd5,    4'd4,     8'd89,   8'd89,     8'd0,       8'd0};
		//
		//}
		//else if(film_mode == EN_FILM55) {
		//    //cadence=55(13),table_cnt=   5,match_data=0x10
		//    //               pha_start,plogo_diff,clogo_diff,pfrm_diff,cfrm_diff,nfrm_diff,mc_phase,me_phase,film_phase,frc_phase
		//    phs_lut_table[0]={ 1'd1,      4'd3,      4'd2,      4'd7,     4'd6,    4'd1,     8'd0,    8'd25,     8'd1,     8'd0};
		//    phs_lut_table[1]={ 1'd0,      4'd3,      4'd2,      4'd8,     4'd6,    4'd2,     8'd25,   8'd25,     8'd2,     8'd0};
		//    phs_lut_table[2]={ 1'd0,      4'd3,      4'd2,      4'd9,     4'd6,    4'd3,     8'd51,   8'd51,     8'd3,     8'd0};
		//    phs_lut_table[3]={ 1'd0,      4'd3,      4'd2,      4'd10,    4'd6,    4'd4,     8'd76,   8'd76,     8'd4,     8'd0};
		//    phs_lut_table[4]={ 1'd0,      4'd3,      4'd2,      4'd11,    4'd6,    4'd5,     8'd102,  8'd102,    8'd0,     8'd0};
		//
		//}
		//else if(film_mode == EN_FILM64) {
		//    //cadence=64(14),table_cnt=  10,match_data=0x220
		//    //               pha_start,plogo_diff,clogo_diff,pfrm_diff,cfrm_diff,nfrm_diff,mc_phase,me_phase,film_phase,frc_phase
		//    phs_lut_table[0]={ 1'd1,      4'd3,      4'd2,      4'd8,     4'd7,    4'd1,     8'd0,    8'd25,     8'd1,       8'd0};
		//    phs_lut_table[1]={ 1'd0,      4'd3,      4'd2,      4'd9,     4'd7,    4'd2,     8'd25,   8'd25,     8'd2,       8'd0};
		//    phs_lut_table[2]={ 1'd0,      4'd3,      4'd2,      4'd10,    4'd7,    4'd3,     8'd51,   8'd51,     8'd3,       8'd0};
		//    phs_lut_table[3]={ 1'd0,      4'd3,      4'd2,      4'd11,    4'd7,    4'd4,     8'd76,   8'd76,     8'd4,       8'd0};
		//    phs_lut_table[4]={ 1'd0,      4'd4,      4'd3,      4'd12,    4'd7,    4'd5,     8'd102,  8'd102,    8'd5,       8'd0};
		//    phs_lut_table[5]={ 1'd1,      4'd3,      4'd2,      4'd8,     4'd6,    4'd2,     8'd0,    8'd25,     8'd6,       8'd0};
		//    phs_lut_table[6]={ 1'd0,      4'd3,      4'd2,      4'd9,     4'd6,    4'd3,     8'd25,   8'd25,     8'd7,       8'd0};
		//    phs_lut_table[7]={ 1'd0,      4'd3,      4'd2,      4'd10,    4'd6,    4'd4,     8'd51,   8'd51,     8'd8,       8'd0};
		//    phs_lut_table[8]={ 1'd0,      4'd3,      4'd2,      4'd11,    4'd6,    4'd5,     8'd76,   8'd76,     8'd9,       8'd0};
		//    phs_lut_table[9]={ 1'd0,      4'd3,      4'd2,      4'd12,    4'd7,    4'd6,     8'd102,  8'd102,    8'd0,       8'd0};
		//}
		//else if(film_mode == EN_FILM66) {
		//    //cadence=66(15),table_cnt=   6,match_data=0x20
		//    //    pha_start,plogo_diff,clogo_diff,pfrm_diff,cfrm_diff,nfrm_diff,mc_phase,me_phase,film_phase,frc_phase
		//    phs_lut_table[0]={ 1'd1,   4'd3,      4'd2,      4'd8,     4'd7,    4'd1,     8'd0,   8'd21,     8'd1,       8'd0};
		//    phs_lut_table[1]={ 1'd0,   4'd3,      4'd2,      4'd9,     4'd7,    4'd2,     8'd21,   8'd21,     8'd2,       8'd0};
		//    phs_lut_table[2]={ 1'd0,   4'd3,      4'd2,      4'd10,     4'd7,    4'd3,     8'd42,   8'd42,     8'd3,       8'd0};
		//    phs_lut_table[3]={ 1'd0,   4'd3,      4'd2,      4'd11,     4'd7,    4'd4,     8'd64,   8'd64,     8'd4,       8'd0};
		//    phs_lut_table[4]={ 1'd0,   4'd3,      4'd2,      4'd12,     4'd7,    4'd5,     8'd85,   8'd85,     8'd5,       8'd0};
		//    phs_lut_table[5]={ 1'd0,   4'd3,      4'd2,      4'd13,     4'd7,    4'd6,     8'd106,   8'd106,     8'd0,       8'd0};
		//}
		//else if(film_mode == EN_FILM87) {
		//    //cadence=87(16),table_cnt=  15,match_data=0x4080
		//    //              pha_start,plogo_diff,clogo_diff,pfrm_diff,cfrm_diff,nfrm_diff,mc_phase,me_phase,film_phase,frc_phase
		//    //phs_lut_table[0]= { 1'd1,   4'd3,      4'd2,      4'd4,      4'd10,    4'd0,     8'd0,    8'd17,    8'd1,     8'd0};
		//    //phs_lut_table[1]= { 1'd0,   4'd3,      4'd2,      4'd4,      4'd1,     4'd0,     8'd17,   8'd17,    8'd2,     8'd0};
		//    //phs_lut_table[2]= { 1'd0,   4'd3,      4'd2,      4'd4,      4'd11,    4'd0,     8'd34,   8'd34,    8'd3,     8'd0};
		//    //phs_lut_table[3]= { 1'd0,   4'd3,      4'd2,      4'd4,      4'd5,     4'd0,     8'd51,   8'd51,    8'd4,     8'd0};
		//    //phs_lut_table[4]= { 1'd0,   4'd3,      4'd2,      4'd4,      4'd14,    4'd0,     8'd68,   8'd68,    8'd5,     8'd0};
		//    //phs_lut_table[5]= { 1'd0,   4'd3,      4'd2,      4'd13,     4'd2,     4'd9,     8'd85,   8'd85,    8'd6,     8'd0};
		//    //phs_lut_table[6]= { 1'd0,   4'd3,      4'd2,      4'd7,      4'd6,     4'd3,     8'd102,  8'd102,   8'd7,     8'd0};
		//    //phs_lut_table[7]= { 1'd0,   4'd4,      4'd3,      4'd1,      4'd11,    4'd12,    8'd119,  8'd119,   8'd8,     8'd0};
		//    //phs_lut_table[8]= { 1'd1,   4'd3,      4'd2,      4'd1,      4'd2,     4'd14,    8'd8,    8'd8,     8'd9,     8'd0};
		//    //phs_lut_table[9]= { 1'd0,   4'd3,      4'd2,      4'd7,      4'd8,     4'd5,     8'd25,   8'd25,    8'd10,    8'd0};
		//    //phs_lut_table[10]={ 1'd0,   4'd3,      4'd2,      4'd13,     4'd14,    4'd11,    8'd42,   8'd42,    8'd11,    8'd0};
		//    //phs_lut_table[11]={ 1'd0,   4'd3,      4'd2,      4'd4,      4'd5,     4'd2,     8'd59,   8'd59,    8'd12,    8'd0};
		//    //phs_lut_table[12]={ 1'd0,   4'd3,      4'd2,      4'd9,      4'd10,    4'd7,     8'd76,   8'd76,    8'd13,    8'd0};
		//    //phs_lut_table[13]={ 1'd0,   4'd3,      4'd2,      4'd13,     4'd14,    4'd11,    8'd93,   8'd93,    8'd14,    8'd0};
		//    //phs_lut_table[14]={ 1'd0,   4'd3,      4'd12,     4'd12,     4'd4,     4'd10,    8'd110,  8'd110,   8'd0,     8'd0};
		//
		//    phs_lut_table[0]= { 1'd1,   4'd3,      4'd2,      4'd8,     4'd5,     4'd2,     8'd0,    8'd17,    8'd1,     8'd0};
		//    phs_lut_table[1]= { 1'd0,   4'd3,      4'd2,      4'd10,    4'd7,     4'd4,     8'd17,   8'd17,    8'd2,     8'd0};
		//    phs_lut_table[2]= { 1'd0,   4'd3,      4'd2,      4'd12,    4'd9,     4'd6,     8'd34,   8'd34,    8'd3,     8'd0};
		//    phs_lut_table[3]= { 1'd0,   4'd3,      4'd2,      4'd14,    4'd11,    4'd8,     8'd51,   8'd51,    8'd4,     8'd0};
		//    phs_lut_table[4]= { 1'd0,   4'd3,      4'd2,      4'd1,     4'd14,    4'd11,    8'd68,   8'd68,    8'd5,     8'd0};
		//    phs_lut_table[5]= { 1'd0,   4'd3,      4'd2,      4'd5,     4'd2,     4'd15,    8'd85,   8'd85,    8'd6,     8'd0};
		//    phs_lut_table[6]= { 1'd0,   4'd3,      4'd2,      4'd8,     4'd5,     4'd2,     8'd102,  8'd102,   8'd7,     8'd0};
		//    phs_lut_table[7]= { 1'd0,   4'd4,      4'd3,      4'd10,    4'd7,     4'd4,     8'd119,  8'd119,   8'd8,     8'd0};
		//    phs_lut_table[8]= { 1'd1,   4'd3,      4'd2,      4'd9,     4'd7,     4'd4,     8'd8,    8'd8,     8'd9,     8'd0};
		//    phs_lut_table[9]= { 1'd0,   4'd3,      4'd2,      4'd11,    4'd9,     4'd6,     8'd25,   8'd25,    8'd10,    8'd0};
		//    phs_lut_table[10]={ 1'd0,   4'd3,      4'd2,      4'd13,    4'd11,    4'd8,     8'd42,   8'd42,    8'd11,    8'd0};
		//    phs_lut_table[11]={ 1'd0,   4'd3,      4'd2,      4'd15,    4'd13,    4'd10,    8'd59,   8'd59,    8'd12,    8'd0};
		//    phs_lut_table[12]={ 1'd0,   4'd3,      4'd2,      4'd3,     4'd1,     4'd14,    8'd76,   8'd76,    8'd13,    8'd0};
		//    phs_lut_table[13]={ 1'd0,   4'd3,      4'd2,      4'd5,     4'd3,     4'd2,     8'd93,   8'd93,    8'd14,    8'd0};
		//    phs_lut_table[14]={ 1'd0,   4'd3,      4'd12,     4'd8,     4'd6,     4'd3,     8'd110,  8'd110,   8'd0,     8'd0};
		//}
		//else if(film_mode == EN_FILM212) {
		//    //cadence=212(17),table_cnt=   5,match_data=0x1a
		//    //pha_start,plogo_diff,clogo_diff,pfrm_diff,cfrm_diff,nfrm_diff,mc_phase,me_phase,film_phase,frc_phase
		//    phs_lut_table[0]={ 1'd1,   4'd3,      4'd2,      4'd5,     4'd3,    4'd1,     8'd0,   8'd76,      8'd1,       8'd0};
		//    phs_lut_table[1]={ 1'd0,   4'd4,      4'd3,      4'd6,     4'd3,    4'd2,     8'd76,   8'd76,     8'd2,       8'd0};
		//    phs_lut_table[2]={ 1'd1,   4'd3,      4'd2,      4'd4,     4'd3,    4'd2,     8'd25,   8'd25,     8'd3,       8'd0};
		//    phs_lut_table[3]={ 1'd0,   4'd4,      4'd3,      4'd5,     4'd4,    4'd3,     8'd102,   8'd102,   8'd4,       8'd0};
		//    phs_lut_table[4]={ 1'd1,   4'd3,      4'd2,      4'd5,     4'd4,    4'd2,     8'd51,   8'd51,     8'd0,       8'd0};
		//}
		//else {
		//      stimulus_print ("==== USE ERROR CADENCE ====");
		//}

		input_n          = 1;
		output_m         = 1;
	}
	frc_config_reg_value(0x08, 0x18, &regdata_top_ctl_0011); //lut_cfg_en
	WRITE_FRC_REG_BY_CPU(FRC_REG_TOP_CTRL17, regdata_top_ctl_0011);
	WRITE_FRC_REG_BY_CPU(FRC_TOP_LUT_ADDR, 0);
	// for (i = 0; i < 256; i++) {
	for (i = 0; i < lut_ary_size + 2; i++) {

		if (i < lut_ary_size) {
			WRITE_FRC_REG_BY_CPU(FRC_TOP_LUT_DATA, phs_lut_table[i]       & 0xffffffff);
			WRITE_FRC_REG_BY_CPU(FRC_TOP_LUT_DATA, phs_lut_table[i] >> 32 & 0xffffffff);
		} else {
			WRITE_FRC_REG_BY_CPU(FRC_TOP_LUT_DATA, 0xffffffff);
			WRITE_FRC_REG_BY_CPU(FRC_TOP_LUT_DATA, 0xffffffff);
		}
	}
	frc_config_reg_value(0x10, 0x18, &regdata_top_ctl_0011); //lut_cfg_en
	WRITE_FRC_REG_BY_CPU(FRC_REG_TOP_CTRL17, regdata_top_ctl_0011);

	tmpregdata = input_n << 24 | output_m << 16;
	frc_config_reg_value(tmpregdata, 0xffff0000, &regdata_phs_tab_0116);
	WRITE_FRC_REG_BY_CPU(FRC_REG_PHS_TABLE, regdata_phs_tab_0116);

}

/*get new frame mode*/
void config_phs_regs(enum frc_ratio_mode_type frc_ratio_mode,
	enum en_film_mode film_mode)
{
	u32            reg_out_frm_dly_num = 3;
	u32            reg_frc_pd_pat_num  = 1;
	u32            reg_ip_film_end     = 1;
	u64            inp_frm_vld_lut     = 1;
	u32            reg_ip_incr[16]     = {0,1,3,1,1,1,1,1,2,3,2,1,1,1,1,2};
	u32            record_value = 0;
	//config ip_film_end
	if(frc_ratio_mode == FRC_RATIO_1_1) {
		inp_frm_vld_lut = 1;

		if(film_mode == EN_VIDEO) {
			reg_ip_film_end     = 1;
			reg_frc_pd_pat_num  = 1;
			reg_out_frm_dly_num = 3;   // 10
		}
		else if(film_mode == EN_FILM22) {
			reg_ip_film_end    = 0x2;
			reg_frc_pd_pat_num = 2;
			reg_out_frm_dly_num = 7;
		}
		else if(film_mode == EN_FILM32) {
			reg_ip_film_end    = 0x12;
			reg_frc_pd_pat_num = 5;
			reg_out_frm_dly_num = 10;
		}
		else if(film_mode == EN_FILM3223) {
			reg_ip_film_end    = 0x24a;
			reg_frc_pd_pat_num = 10;
			reg_out_frm_dly_num = 15;
		}
		else if(film_mode == EN_FILM2224) {
			reg_ip_film_end    = 0x22a;
			reg_frc_pd_pat_num = 10;
			reg_out_frm_dly_num = 15;
		}
		else if(film_mode == EN_FILM32322) {
			reg_ip_film_end    = 0x94a;
			reg_frc_pd_pat_num = 12;
			reg_out_frm_dly_num = 15;
		}
		else if(film_mode == EN_FILM44) {
			reg_ip_film_end    = 0x8;
			reg_frc_pd_pat_num = 4;
			reg_out_frm_dly_num = 14;
		}
		else if(film_mode == EN_FILM21111) {
			reg_ip_film_end    = 0x2f;
			reg_frc_pd_pat_num = 6;
			reg_out_frm_dly_num = 10;
		}
		else if(film_mode == EN_FILM23322) {
			reg_ip_film_end    = 0x92a;
			reg_frc_pd_pat_num = 12;
			reg_out_frm_dly_num = 15;
		}
		else if(film_mode == EN_FILM2111) {
			reg_ip_film_end    = 0x17;
			reg_frc_pd_pat_num = 5;
			reg_out_frm_dly_num = 10;
		}
		else if(film_mode == EN_FILM22224) {
			reg_ip_film_end    = 0x8aa;
			reg_frc_pd_pat_num = 12;
			reg_out_frm_dly_num = 15;
		}
		else if(film_mode == EN_FILM33) {
			reg_ip_film_end    = 0x4;
			reg_frc_pd_pat_num = 3;
			reg_out_frm_dly_num = 11;
		}
		else if(film_mode == EN_FILM334) {
			reg_ip_film_end    = 0x224;
			reg_frc_pd_pat_num = 10;
			reg_out_frm_dly_num = 15;
		}
		else if(film_mode == EN_FILM55) {
			reg_ip_film_end    = 0x10;
			reg_frc_pd_pat_num = 5;
			reg_out_frm_dly_num = 15;
		}
		else if(film_mode == EN_FILM64) {
			reg_ip_film_end    = 0x208;
			reg_frc_pd_pat_num = 10;
			reg_out_frm_dly_num = 15;
		}
		else if(film_mode == EN_FILM66) {
			reg_ip_film_end    = 0x20;
			reg_frc_pd_pat_num = 6;
			reg_out_frm_dly_num = 15;
		}
		else if(film_mode == EN_FILM87) {
			 //memc need pre-load 3 different frames, so at least input 8+7+8=23 frames,
			 //but memc lbuf number max is 16<23, so skip some repeated frames
			reg_ip_film_end     = 0x4040;
			reg_frc_pd_pat_num  = 15;
			reg_out_frm_dly_num = 15;
		}
		else if(film_mode == EN_FILM212) {
			reg_ip_film_end    = 0x15;
			reg_frc_pd_pat_num = 5;
			reg_out_frm_dly_num = 15;
		} else if (film_mode == EN_FILM_MAX) {
			reg_ip_film_end = 0xffff;  // fix deadcode
		}
		else {
			pr_frc(0, "==== USE ERROR CADENCE ====\n");
		}
	}
	else {
		reg_ip_film_end    = 0x1;
		reg_frc_pd_pat_num = 1;

		if(frc_ratio_mode == FRC_RATIO_1_2) {
			 inp_frm_vld_lut = 0x2;
		}
		else if(frc_ratio_mode == FRC_RATIO_2_3) {
			 inp_frm_vld_lut = 0x6;
		}
		else if(frc_ratio_mode == FRC_RATIO_2_5) {
			 inp_frm_vld_lut = 0x14;

		}
		else if(frc_ratio_mode == FRC_RATIO_5_6) {
			 inp_frm_vld_lut = 0x3e;
		}
		else if(frc_ratio_mode == FRC_RATIO_5_12) {
			 inp_frm_vld_lut = 0xa94;
		}

	}
	// input [63:0] reg_inp_frm_vld_lut;
	WRITE_FRC_REG_BY_CPU(FRC_REG_TOP_CTRL18, (inp_frm_vld_lut >> 0) & 0xffffffff);
	WRITE_FRC_REG_BY_CPU(FRC_REG_TOP_CTRL19, (inp_frm_vld_lut >> 32) & 0xffffffff);
	WRITE_FRC_REG_BY_CPU(FRC_REG_PD_PAT_NUM, reg_frc_pd_pat_num);

	frc_config_reg_value(reg_out_frm_dly_num << 24,
						0xF000000, &regdata_top_ctl_0009);
	WRITE_FRC_REG_BY_CPU(FRC_REG_TOP_CTRL9, regdata_top_ctl_0009);
	regdata_loadorgframe[0] = READ_FRC_REG(FRC_REG_LOAD_ORG_FRAME_0);
	regdata_loadorgframe[1] = READ_FRC_REG(FRC_REG_LOAD_ORG_FRAME_1);
	regdata_loadorgframe[2] = READ_FRC_REG(FRC_REG_LOAD_ORG_FRAME_2);
	regdata_loadorgframe[3] = READ_FRC_REG(FRC_REG_LOAD_ORG_FRAME_3);
	record_value  = 0x0;
	if ((reg_ip_film_end >> 0) & 0x1)
		record_value |= 0x08000000;
	if ((reg_ip_film_end >> 1) & 0x1)
		record_value |= 0x00080000;
	if ((reg_ip_film_end >> 2) & 0x1)
		record_value |= 0x00000800;
	if ((reg_ip_film_end >> 3) & 0x1)
		record_value |= 0x00000008;
	frc_config_reg_value(record_value, 0x08080808, &regdata_loadorgframe[0]);
	record_value  = 0x0;
	if ((reg_ip_film_end >> 4) & 0x1)
		record_value |= 0x08000000;
	if ((reg_ip_film_end >> 5) & 0x1)
		record_value |= 0x00080000;
	if ((reg_ip_film_end >> 6) & 0x1)
		record_value |= 0x00000800;
	if ((reg_ip_film_end >> 7) & 0x1)
		record_value |= 0x00000008;
	frc_config_reg_value(record_value, 0x08080808,	&regdata_loadorgframe[1]);
	record_value  = 0x0;
	if ((reg_ip_film_end >> 8) & 0x1)
		record_value |= 0x08000000;
	if ((reg_ip_film_end >> 9) & 0x1)
		record_value |= 0x00080000;
	if ((reg_ip_film_end >> 10) & 0x1)
		record_value |= 0x00000800;
	if ((reg_ip_film_end >> 11) & 0x1)
		record_value |= 0x00000008;
	frc_config_reg_value(record_value, 0x08080808,	&regdata_loadorgframe[2]);
	record_value  = 0x0;
	if ((reg_ip_film_end >> 12) & 0x1)
		record_value |= 0x08000000;
	if ((reg_ip_film_end >> 13) & 0x1)
		record_value |= 0x00080000;
	if ((reg_ip_film_end >> 14) & 0x1)
		record_value |= 0x00000800;
	if ((reg_ip_film_end >> 15) & 0x1)
		record_value |= 0x00000008;
	frc_config_reg_value(record_value, 0x08080808,	&regdata_loadorgframe[3]);

	if (film_mode == EN_FILM87 && frc_ratio_mode == FRC_RATIO_1_1) {
		frc_config_reg_value(reg_ip_incr[0] << 28, 0xF0000000, &regdata_loadorgframe[0]);
		frc_config_reg_value(reg_ip_incr[1] << 20, 0x00F00000, &regdata_loadorgframe[0]);
		frc_config_reg_value(reg_ip_incr[2] << 12, 0x0000F000, &regdata_loadorgframe[0]);
		frc_config_reg_value(reg_ip_incr[3] << 4,  0x000000F0, &regdata_loadorgframe[0]);

		frc_config_reg_value(reg_ip_incr[4] << 28, 0xF0000000, &regdata_loadorgframe[1]);
		frc_config_reg_value(reg_ip_incr[5] << 20, 0x00F00000, &regdata_loadorgframe[1]);
		frc_config_reg_value(reg_ip_incr[6] << 12, 0x0000F000, &regdata_loadorgframe[1]);
		frc_config_reg_value(reg_ip_incr[7] << 4,  0x000000F0, &regdata_loadorgframe[1]);

		frc_config_reg_value(reg_ip_incr[8] << 28,  0xF0000000, &regdata_loadorgframe[2]);
		frc_config_reg_value(reg_ip_incr[9] << 20,  0x00F00000, &regdata_loadorgframe[2]);
		frc_config_reg_value(reg_ip_incr[10] << 12, 0x0000F000, &regdata_loadorgframe[2]);
		frc_config_reg_value(reg_ip_incr[11] << 4,  0x000000F0, &regdata_loadorgframe[2]);

		frc_config_reg_value(reg_ip_incr[12] << 28, 0xF0000000, &regdata_loadorgframe[3]);
		frc_config_reg_value(reg_ip_incr[13] << 20, 0x00F00000, &regdata_loadorgframe[3]);
		frc_config_reg_value(reg_ip_incr[14] << 12, 0x0000F000, &regdata_loadorgframe[3]);
		frc_config_reg_value(reg_ip_incr[15] << 4,  0x000000F0, &regdata_loadorgframe[3]);
	} else {
		frc_config_reg_value(0, 0xF0000000, &regdata_loadorgframe[0]);
		frc_config_reg_value(0, 0x00F00000, &regdata_loadorgframe[0]);
		frc_config_reg_value(0, 0x0000F000, &regdata_loadorgframe[0]);
		frc_config_reg_value(0, 0x000000F0, &regdata_loadorgframe[0]);

		frc_config_reg_value(0, 0xF0000000, &regdata_loadorgframe[1]);
		frc_config_reg_value(0, 0x00F00000, &regdata_loadorgframe[1]);
		frc_config_reg_value(0, 0x0000F000, &regdata_loadorgframe[1]);
		frc_config_reg_value(0, 0x000000F0, &regdata_loadorgframe[1]);

		frc_config_reg_value(0, 0xF0000000, &regdata_loadorgframe[2]);
		frc_config_reg_value(0, 0x00F00000, &regdata_loadorgframe[2]);
		frc_config_reg_value(0, 0x0000F000, &regdata_loadorgframe[2]);
		frc_config_reg_value(0, 0x000000F0, &regdata_loadorgframe[2]);

		frc_config_reg_value(0, 0xF0000000, &regdata_loadorgframe[3]);
		frc_config_reg_value(0, 0x00F00000, &regdata_loadorgframe[3]);
		frc_config_reg_value(0, 0x0000F000, &regdata_loadorgframe[3]);
		frc_config_reg_value(0, 0x000000F0, &regdata_loadorgframe[3]);
	}
	WRITE_FRC_REG_BY_CPU(FRC_REG_LOAD_ORG_FRAME_0, regdata_loadorgframe[0]);
	WRITE_FRC_REG_BY_CPU(FRC_REG_LOAD_ORG_FRAME_1, regdata_loadorgframe[1]);
	WRITE_FRC_REG_BY_CPU(FRC_REG_LOAD_ORG_FRAME_2, regdata_loadorgframe[2]);
	WRITE_FRC_REG_BY_CPU(FRC_REG_LOAD_ORG_FRAME_3, regdata_loadorgframe[3]);
}
EXPORT_SYMBOL(config_phs_regs);

void config_me_top_hw_reg(void)
{
	u32    reg_data;
	u32    mvx_div_mode;
	u32    mvy_div_mode;
	u32    me_max_mvx  ;
	u32    me_max_mvy  ;
	//u32    hmvx_div_mode;
	//u32    hmvy_div_mode;
	//u32    hme_max_mvx ;
	//u32    hme_max_mvy ;

	reg_data = READ_FRC_REG(FRC_REG_BLK_SIZE_XY);
	// reg_data = regdata_blksizexy_012b;
	mvx_div_mode = (reg_data>>14) & 0x3;
	mvy_div_mode = (reg_data>>12) & 0x3;

	me_max_mvx   = (MAX_ME_MVX << mvx_div_mode  < (1<<(ME_MVX_BIT-1))-1) ? (MAX_ME_MVX << mvx_div_mode) :  ((1<<(ME_MVX_BIT-1))-1);
	me_max_mvy   = (MAX_ME_MVY << mvy_div_mode  < (1<<(ME_MVY_BIT-1))-1) ? (MAX_ME_MVY << mvy_div_mode) :  ((1<<(ME_MVY_BIT-1))-1);

	WRITE_FRC_REG_BY_CPU(FRC_ME_CMV_MAX_MV, (me_max_mvx << 16) | me_max_mvy);

	// hme setting
	// reg_data = READ_FRC_REG(FRC_REG_BLK_SIZE_XY);
	// hmvx_div_mode = (reg_data >> 2) & 0x3;
	// hmvy_div_mode = (reg_data >> 0) & 0x3;
	// hme_max_mvx  = (MAX_HME_MVX << hmvx_div_mode  < (1<<(HME_MVX_BIT-1))-1) ?
	// (MAX_HME_MVX << hmvx_div_mode) :  ((1<<(HME_MVX_BIT-1))-1);
	// hme_max_mvy  = (MAX_HME_MVY << hmvy_div_mode  < (1<<(HME_MVY_BIT-1))-1) ?
	// (MAX_HME_MVY << hmvy_div_mode) :  ((1<<(HME_MVY_BIT-1))-1);
	// WRITE_FRC_REG_BY_CPU(FRC_HME_CMV_MAX_MV, (hme_max_mvx << 16) | hme_max_mvy);

	WRITE_FRC_REG_BY_CPU(FRC_ME_CMV_CTRL, 0x800000ff);
	WRITE_FRC_REG_BY_CPU(FRC_ME_CMV_CTRL, 0x000000ff);

}

void config_loss_out(u32 fmt422)
{
#ifdef LOSS_TEST
   // #include "rand_seed_reg_acc.sv"  //?????????????
	WRITE_FRC_BITS(CLOSS_MISC, 1, 0, 1); //reg_enable
	WRITE_FRC_BITS(CLOSS_MISC, fmt422, 1, 1); //reg_inp_422;   1: 422, 0:444

	WRITE_FRC_BITS(CLOSS_MISC, (6 << 8) | (7 << 16), 12, 20); //reg_misc
	printf("config_loss_out\n");
#endif
}

void enable_nr(void)
{
	u32  reg_data;
	u32  reg_nr_misc_data;

	WRITE_FRC_BITS(FRC_REG_INP_MODULE_EN, 1, 9, 1); //reg_mc_nr_en
	reg_data = READ_FRC_REG(FRC_NR_MISC);
	reg_nr_misc_data = reg_data & 0x00000001;
	WRITE_FRC_BITS(FRC_NR_MISC, reg_nr_misc_data, 0, 32); //reg_nr_misc
}

void enable_bbd(void)
{
	// WRITE_FRC_BITS(FRC_REG_INP_MODULE_EN, 1, 7, 1); //reg_inp_bbd_en
	frc_config_reg_value(0x80, 0x80, &regdata_inpmoden_04f9);
	WRITE_FRC_REG_BY_CPU(FRC_REG_INP_MODULE_EN, regdata_inpmoden_04f9);
}

void set_mc_lbuf_ext(void)
{
	WRITE_FRC_BITS(FRC_MC_HW_CTRL1, 1, 16, 1); //reg_mc_fhd_lbuf_ext
}

void frc_cfg_memc_loss(u32 memc_loss_en)
{
	u32 memcloss = 0x0404000c;

	if (memc_loss_en > 3)
		memc_loss_en = 3;
	memcloss |= memc_loss_en;
	WRITE_FRC_REG_BY_CPU(FRC_REG_TOP_CTRL11, memcloss);
}

void frc_force_secure(u32 onoff)
{
	if (onoff)
		WRITE_FRC_REG_BY_CPU(FRC_MC_LOSS_SLICE_SEC, 1);//reg_mc_loss_slice_sec
	else
		WRITE_FRC_REG_BY_CPU(FRC_MC_LOSS_SLICE_SEC, 0);//reg_mc_loss_slice_sec
}

void recfg_memc_mif_base_addr(u32 base_ofst)
{
	u32 reg_addr;
	u32 reg_data;

	for(reg_addr = FRC_REG_MC_YINFO_BADDR; reg_addr < FRC_REG_LOGO_SCC_BUF_ADDR; reg_addr += 4) {
		reg_data = READ_FRC_REG(reg_addr);
		reg_data += base_ofst;
		WRITE_FRC_REG_BY_CPU(reg_addr, reg_data);
	}
}

static const struct dbg_dump_tab frc_reg_tab[] = {
	{"FRC_TOP_CTRL->reg_frc_en_in", FRC_TOP_CTRL, 0, 1},
	{"FRC_REG_INP_MODULE_EN", FRC_REG_INP_MODULE_EN, 0, 0},

	{"FRC_REG_TOP_CTRL14->post_dly_vofst", FRC_REG_TOP_CTRL14, 0, 16},
	{"FRC_REG_TOP_CTRL14->reg_me_dly_vofst", FRC_REG_TOP_CTRL14, 16, 16},
	{"FRC_REG_TOP_CTRL15->reg_mc_dly_vofst0", FRC_REG_TOP_CTRL15, 0, 16},
	{"FRC_REG_TOP_CTRL15->reg_mc_dly_vofst1", FRC_REG_TOP_CTRL15, 16, 16},

	{"FRC_OUT_HOLD_CTRL->me_hold_line", FRC_OUT_HOLD_CTRL, 0, 8},
	{"FRC_OUT_HOLD_CTRL->mc_hold_line", FRC_OUT_HOLD_CTRL, 8, 8},
	{"FRC_INP_HOLD_CTRL->inp_hold_line", FRC_INP_HOLD_CTRL, 0, 13},

	{"FRC_FRAME_BUFFER_NUM->logo_fb_num", FRC_FRAME_BUFFER_NUM, 0, 5},
	{"FRC_FRAME_BUFFER_NUM->frc_fb_num", FRC_FRAME_BUFFER_NUM, 8, 5},

	{"FRC_REG_BLK_SIZE_XY->reg_me_mvx_div_mode", FRC_REG_BLK_SIZE_XY, 14, 2},
	{"FRC_REG_BLK_SIZE_XY->reg_me_mvy_div_mode", FRC_REG_BLK_SIZE_XY, 12, 2},
	// {"FRC_REG_BLK_SIZE_XY->reg_hme_mvx_div_mode", FRC_REG_BLK_SIZE_XY, 2, 2},
	// {"FRC_REG_BLK_SIZE_XY->reg_hme_mvy_div_mode", FRC_REG_BLK_SIZE_XY, 0, 2},

	{"FRC_REG_ME_DS_COEF_0", FRC_REG_ME_DS_COEF_0,	0, 0},
	{"FRC_REG_ME_DS_COEF_1", FRC_REG_ME_DS_COEF_1,	0, 0},
	{"FRC_REG_ME_DS_COEF_2", FRC_REG_ME_DS_COEF_2,	0, 0},
	{"FRC_REG_ME_DS_COEF_3", FRC_REG_ME_DS_COEF_3,	0, 0},

	{"NULL", 0xffffffff, 0, 0},
};

void frc_dump_reg_tab(void)
{
	u32 i = 0;

	pr_frc(0, "VPU_FRC_TOP_CTRL (0x%x)->bypass val:0x%x (1:bypass)\n",
		VPU_FRC_TOP_CTRL, vpu_reg_read(VPU_FRC_TOP_CTRL) & 0x1);

	pr_frc(0, "VPU_FRC_TOP_CTRL (0x%x)->pos_sel val:0x%x (0:after blend)\n",
		VPU_FRC_TOP_CTRL, (vpu_reg_read(VPU_FRC_TOP_CTRL) >> 4) & 0x1);

	pr_frc(0, "ENCL_FRC_CTRL (0x%x)->val:0x%x\n",
		ENCL_FRC_CTRL, vpu_reg_read(ENCL_FRC_CTRL) & 0xffff);

	pr_frc(0, "ENCL_VIDEO_VAVON_BLINE:0x%x->val: 0x%x\n",
		ENCL_VIDEO_VAVON_BLINE, vpu_reg_read(ENCL_VIDEO_VAVON_BLINE) & 0xffff);

	while (frc_reg_tab[i].addr < 0x3fff) {
		if (frc_reg_tab[i].addr == 0xffffffff)
			break;

		if (frc_reg_tab[i].len != 0) {
			pr_frc(0, "%s (0x%x) val:0x%x\n",
			frc_reg_tab[i].name, frc_reg_tab[i].addr,
			READ_FRC_BITS(frc_reg_tab[i].addr,
			frc_reg_tab[i].start, frc_reg_tab[i].len));
		} else {
			pr_frc(0, "%s (0x%x) val:0x%x\n",
			frc_reg_tab[i].name, frc_reg_tab[i].addr,
			READ_FRC_REG(frc_reg_tab[i].addr));
		}
		i++;
	}
}

/*request from xianjun, dump fixed table reg*/
void frc_dump_fixed_table(void)
{
	int i = 0;
	unsigned int value = 0;

	for (i = 0; i < REG_NUM; i++) {
		value = READ_FRC_REG(regs_table[i].addr);
		pr_frc(0, "0x%04x, 0x%08x\n", regs_table[i].addr, value);
	}
}

void frc_set_val_from_reg(void)
{
/*
	regdata_phs_tab_0116   = 0x02050000;

	regdata_blksizexy_012b = 0x0012a12a;
	regdata_blkscale_012c  = 0x00086920;
	regdata_hme_scale_012d  = 0xaa;
	regdata_inpholdctl_0002 = 0x00100006;
	regdata_outholdctl_0003 = 0x82020202;
	regdata_inpmoden_04f9 = 0x0000203e;
	regdata_me_stat12rhst_110b = 0x00030800;
	regdata_me_en_1100 = 0x4d0d2222;

	regdata_bbd_t2b_0604 = 0x0000086f,
	regdata_bbd_l2r_0605 = 0x00000eff,
*/
	regdata_loadorgframe[0] = 0;
	regdata_loadorgframe[1] = 0;
	regdata_loadorgframe[2] = 0;

	regdata_inpholdctl_0002 = READ_FRC_REG(0x0002);
	regdata_outholdctl_0003 = READ_FRC_REG(0x0003);
	regdata_top_ctl_0007 = READ_FRC_REG(0x0007);
	regdata_top_ctl_0009 = READ_FRC_REG(0x0009);
	regdata_top_ctl_0011 =  READ_FRC_REG(0x0011);

	regdata_pat_pointer_0102 = READ_FRC_REG(0x0102);
	// regdata_loadorgframe[16];		// 0x0103

	regdata_phs_tab_0116 = READ_FRC_REG(0x0116);
	regdata_blksizexy_012b = READ_FRC_REG(0x012b);
	regdata_blkscale_012c = READ_FRC_REG(0x012c);
	regdata_hme_scale_012d = READ_FRC_REG(0x012d);

	regdata_logodbg_0142 = READ_FRC_REG(0x0142);
	regdata_inpmoden_04f9 = READ_FRC_REG(0x04f9);

	regdata_iplogo_en_0503 = READ_FRC_REG(0x0503);
	regdata_bbd_t2b_0604 = READ_FRC_REG(0x0604);
	regdata_bbd_l2r_0605 = READ_FRC_REG(0x0605);

	regdata_me_en_1100 = READ_FRC_REG(0x1100);
	regdata_me_bbpixed_1108 = READ_FRC_REG(0x1108);
	regdata_me_bbblked_110a = READ_FRC_REG(0x110a);
	regdata_me_stat12rhst_110b = READ_FRC_REG(0x110b);
	regdata_me_stat12rh_110c = READ_FRC_REG(0x110c);
	regdata_me_stat12rh_110d = READ_FRC_REG(0x110d);
	regdata_me_stat12rv_110e = READ_FRC_REG(0x110e);
	regdata_me_stat12rv_110f = READ_FRC_REG(0x110f);

	regdata_vpbb1_1e03 = READ_FRC_REG(0x1e03);
	regdata_vpbb2_1e04 = READ_FRC_REG(0x1e04);
	regdata_vpmebb1_1e05 = READ_FRC_REG(0x1e05);
	regdata_vpmebb2_1e06 = READ_FRC_REG(0x1e06);

	regdata_vp_win1_1e58 = READ_FRC_REG(0x1e58);
	regdata_vp_win2_1e59 = READ_FRC_REG(0x1e59);
	regdata_vp_win3_1e5a = READ_FRC_REG(0x1e5a);
	regdata_vp_win4_1e5b = READ_FRC_REG(0x1e5b);

	regdata_mcset1_3000 = READ_FRC_REG(0x3000);
	regdata_mcset2_3001 = READ_FRC_REG(0x3001);

	regdata_mcdemo_win_3200  = READ_FRC_REG(0x3200);

	regdata_topctl_3f01 = READ_FRC_REG(0x3f01);
}

/* driver probe call */
void frc_internal_initial(struct frc_dev_s *frc_devp)
{
	int i;
	//int mc_range_norm_lut[36] = {
	//	-8, 8, -24, 24, -16, 16, -16, 16,
	//	-24, 24, -8, 8, 0, 0, 0, 0,
	//	0, 0, 0, 0, 0, 0, 0, 0,
	//	0, 0, 0, 0, 0, 0, 0, 0,
	//	0, 0, 0, 0
	//};
	//int mc_range_sing_lut[36] = {
	//	-8, 8, -40, 8, -8, 16, -32, 8,
	//	-8, 24, -24, 8, -8, 32, -16, 8,
	//	-8, 40, -8, 8, 0, 0, 0, 0,
	//	0, 0, 0, 0, 0, 0, 0, 0,
	//	0, 0, 0, 0
	//};

	//int glb_pos_hi_y_th[22] = {
	//	16, 32, 48, 64, 80, 96, 160, 192,
	//	320, 384, 0, 0, 0, 0, 0, 0, 0, 0,
	//	0, 0, 0, 0
	//};

	struct frc_fw_data_s *fw_data;
	struct frc_top_type_s *frc_top;

	fw_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	frc_top = &fw_data->frc_top_type;

	for (i = 0; i < REG_NUM; i++)
		WRITE_FRC_REG_BY_CPU(regs_table[i].addr, regs_table[i].value);
	pr_frc(0, "regs_table[%d] init done\n", REG_NUM);

	frc_set_val_from_reg();

	// frc_inp_init(frc_top->frc_fb_num, frc_top->film_hwfw_sel);
	frc_inp_init();
	// config_phs_lut(frc_top->frc_ratio_mode, frc_top->film_mode);
	// config_phs_regs(frc_top->frc_ratio_mode, frc_top->film_mode);
	config_me_top_hw_reg();
	frc_top->memc_loss_en = 0x03;
	frc_cfg_memc_loss(frc_top->memc_loss_en);
	/*protect mode, enable: memc delay 2 frame*/
	/*disable: memc delay n frame, n depend on cadence, for debug*/
	pr_frc(0, "%s\n", __func__);
	return;
}

u8 frc_frame_forcebuf_enable(u8 enable)
{
	u8  ro_frc_input_fid = 0;//u4

	if (enable == 1) {	//frc off->on
		// ro_frc_input_fid = READ_FRC_REG(FRC_REG_PAT_POINTER) >> 4 & 0xF;
		ro_frc_input_fid = (regdata_pat_pointer_0102 >> 4) & 0xF;
		//force phase 0
		// UPDATE_FRC_REG_BITS(FRC_REG_TOP_CTRL7, 0x9000000, 0x9000000);
		frc_config_reg_value(0x9000000, 0x9000000, &regdata_top_ctl_0007);
		WRITE_FRC_REG_BY_CPU(FRC_REG_TOP_CTRL7, regdata_top_ctl_0007);
		WRITE_FRC_REG_BY_CPU(FRC_REG_TOP_CTRL8, 0);
	} else {
		// UPDATE_FRC_REG_BITS(FRC_REG_TOP_CTRL7, 0x0000000, 0x9000000);
		frc_config_reg_value(0x0, 0x9000000, &regdata_top_ctl_0007);
		WRITE_FRC_REG_BY_CPU(FRC_REG_TOP_CTRL7, regdata_top_ctl_0007);
	}
	return ro_frc_input_fid;
}

void frc_frame_forcebuf_count(u8 forceidx)
{
	// UPDATE_FRC_REG_BITS(FRC_REG_TOP_CTRL7,
	// (forceidx | forceidx << 4 | forceidx << 8), 0xFFF);//0-11bit
	frc_config_reg_value((forceidx | forceidx << 4 | forceidx << 8),
		0xFFF, &regdata_top_ctl_0007);
	WRITE_FRC_REG_BY_CPU(FRC_REG_TOP_CTRL7, regdata_top_ctl_0007);
}

u16 frc_check_vf_rate(u16 duration, struct frc_dev_s *frc_devp)
{
	int i, getflag;
	u16 framerate = 0;

	/*duration: 1600(60fps) 1920(50fps) 3200(30fps) 3203(29.97)*/
	/*3840(25fps) 4000(24fps) 4004(23.976fps)*/
	i = 0;
	getflag = 0;
	while (i < 8) {
		if (vf_rate_table[i].duration == duration) {
			framerate = vf_rate_table[i].framerate;
			getflag = 1;
			break;
		}
		i++;
	}
	if (getflag == 1 && framerate != frc_devp->in_sts.frc_vf_rate) {
		pr_frc(3, "input vf rate changed [%d->%d].\n",
			frc_devp->in_sts.frc_vf_rate, framerate);
		frc_devp->in_sts.frc_vf_rate = framerate;
		getflag = 0;
	}
	return frc_devp->in_sts.frc_vf_rate;
}

void frc_get_film_base_vf(struct frc_dev_s *frc_devp)
{
	struct frc_fw_data_s *pfw_data;
	u16 in_frame_rate = frc_devp->in_sts.frc_vf_rate;
	u16 outfrmrate = frc_devp->out_sts.out_framerate;
	// u32 vf_duration = frc_devp->in_sts.duration;

	pfw_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	pfw_data->frc_top_type.film_mode = EN_VIDEO;
	pfw_data->frc_top_type.vfp &= 0xFFFFFFF0;
	if (frc_devp->in_sts.frc_is_tvin)  // HDMI_src no inform alg
		return;
	if ((pfw_data->frc_top_type.vfp & BIT_7) == BIT_7)
		return;
	pfw_data->frc_top_type.vfp |= 0x4;
	pr_frc(1, "get in_rate:%d,out_rate:%d\n", in_frame_rate, outfrmrate);
	switch (in_frame_rate) {
	case FRC_VD_FPS_24:
		if (outfrmrate == 50) {
			pfw_data->frc_top_type.film_mode = EN_FILM1123;
		} else if (outfrmrate == 60 || outfrmrate == 59) {
			pfw_data->frc_top_type.film_mode = EN_FILM32;
			pfw_data->frc_top_type.vfp |= 0x07;
		} else if (outfrmrate == 120) {
			pfw_data->frc_top_type.film_mode = EN_FILM55;
		}
		break;
	case FRC_VD_FPS_25:
		if (outfrmrate == 50) {
			pfw_data->frc_top_type.film_mode = EN_FILM22;
			pfw_data->frc_top_type.vfp |= 0x04;
		} else if (outfrmrate == 60 || outfrmrate == 59) {
			pfw_data->frc_top_type.film_mode = EN_FILM32322;
		} else if (outfrmrate == 100) {
			pfw_data->frc_top_type.film_mode = EN_FILM44;
		}
		break;
	case FRC_VD_FPS_30:
		if (outfrmrate == 50) {
			pfw_data->frc_top_type.film_mode = EN_FILM212;
		} else if (outfrmrate == 60 || outfrmrate == 59) {
			pfw_data->frc_top_type.film_mode = EN_FILM22;
			pfw_data->frc_top_type.vfp |= 0x04;
		} else if (outfrmrate == 120) {
			pfw_data->frc_top_type.film_mode = EN_FILM44;
		}
		break;
	case FRC_VD_FPS_50:
		if (outfrmrate == 60 || outfrmrate == 59) {
			pfw_data->frc_top_type.film_mode = EN_FILM21111;
		} else if (outfrmrate == 100) {
			pfw_data->frc_top_type.film_mode = EN_FILM22;
			pfw_data->frc_top_type.vfp |= 0x04;
		}
		break;
	case FRC_VD_FPS_60:
		if (outfrmrate == 120) {
			pfw_data->frc_top_type.film_mode = EN_FILM22;
			pfw_data->frc_top_type.vfp |= 0x04;
		}
		break;
	default:
		pfw_data->frc_top_type.film_mode = EN_VIDEO;
		break;
	}
}

void frc_set_enter_forcefilm(struct frc_dev_s *frc_devp, u16 flag)
{
	struct frc_fw_data_s *pfw_data;

	pfw_data = (struct frc_fw_data_s *)frc_devp->fw_data;

	if (flag)
		pfw_data->frc_top_type.vfp |= BIT_4;
	else
		pfw_data->frc_top_type.vfp &= (~BIT_4);
}

void frc_set_notell_film(struct frc_dev_s *frc_devp, u16 flag)
{
	struct frc_fw_data_s *pfw_data;

	pfw_data = (struct frc_fw_data_s *)frc_devp->fw_data;

	if (flag == 1)
		pfw_data->frc_top_type.vfp |= BIT_7;
	else
		pfw_data->frc_top_type.vfp &= ~BIT_7;
	pfw_data->frc_top_type.vfp &= ~BIT_6;
}
