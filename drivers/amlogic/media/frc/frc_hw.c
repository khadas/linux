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

const struct vf_rate_table vf_rate_table[9] = {
	{1600,  FRC_VD_FPS_60},
	{1601,	FRC_VD_FPS_60},
	{1920,  FRC_VD_FPS_50},
	{2000,  FRC_VD_FPS_48},
	{3200,	FRC_VD_FPS_30},
	{3840,	FRC_VD_FPS_25},
	{4000,	FRC_VD_FPS_24},
	{4004,	FRC_VD_FPS_24},
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
	WRITE_FRC_BITS(FRC_LOGO_DEBUG, falsecolor, 19, 1);  //  falsecolor enable
}

void frc_init_config(struct frc_dev_s *devp)
{
	/*1: before postblend, 0: after postblend*/
	if (devp->frc_hw_pos == FRC_POS_AFTER_POSTBLEND) {
		vpu_reg_write_bits(VPU_FRC_TOP_CTRL, 0, 4, 1);
		WRITE_FRC_BITS(FRC_REG_BLK_SCALE, 1, 1, 1); // OSD Bit Enable
		WRITE_FRC_BITS(FRC_IPLOGO_EN, 0x8, 20, 4);
	} else {
		vpu_reg_write_bits(VPU_FRC_TOP_CTRL, 1, 4, 1);
		WRITE_FRC_BITS(FRC_REG_BLK_SCALE, 0, 1, 1);   // OSD Bit Disable
		WRITE_FRC_BITS(FRC_IPLOGO_EN, 0, 20, 4);
		WRITE_FRC_BITS(FRC_LOGO_DEBUG, 0, 19, 1);
	}
}

void frc_reset(u32 onoff)
{
	if (onoff) {
		WRITE_FRC_REG(FRC_AUTO_RST_SEL, 0x3c);
		WRITE_FRC_REG(FRC_SYNC_SW_RESETS, 0x3c);
	} else {
		WRITE_FRC_REG(FRC_SYNC_SW_RESETS, 0x0);
		WRITE_FRC_REG(FRC_AUTO_RST_SEL, 0x0);
	}
}

void frc_mc_reset(u32 onoff)
{
	if (onoff) {
		WRITE_FRC_REG(FRC_MC_SW_RESETS, 0xffff);
		WRITE_FRC_REG(FRC_MEVP_SW_RESETS, 0xffffff);
	} else {
		WRITE_FRC_REG(FRC_MEVP_SW_RESETS, 0x0);
		WRITE_FRC_REG(FRC_MC_SW_RESETS, 0x0);
	}
	pr_frc(1, "%s %d\n", __func__, onoff);
}

void set_frc_enable(u32 en)
{
	// struct frc_dev_s *devp = get_frc_devp();
	WRITE_FRC_BITS(FRC_TOP_CTRL, 0, 8, 1);
	WRITE_FRC_BITS(FRC_TOP_CTRL, en, 0, 1);
	if (en == 1) {
		frc_mc_reset(1);
		frc_mc_reset(0);
		WRITE_FRC_BITS(FRC_TOP_SW_RESET, 0xFFFF, 0, 16);
		WRITE_FRC_BITS(FRC_TOP_SW_RESET, 0x0, 0, 16);
	} else {
		gst_frc_param.s2l_en = 0;
		gst_frc_param.frc_mcfixlines = 0;
		if (vlock_sync_frc_vporch(gst_frc_param) < 0)
			pr_frc(0, "frc_off_set maxlnct fail !!!\n");
		else
			pr_frc(0, "frc_off_set maxlnct success!!!\n");
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
	WRITE_FRC_BITS(FRC_FRAME_BUFFER_NUM, frc_fb_num, 0, 5);//set   frc_fb_num
	WRITE_FRC_BITS(FRC_FRAME_BUFFER_NUM, frc_fb_num, 8, 5);//set   frc_fb_num
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
		WRITE_FRC_BITS(FRC_INP_UE_CLR, 0x3f, 0, 6);
		WRITE_FRC_BITS(FRC_INP_UE_CLR, 0x0, 0, 6);
		frc_devp->ud_dbg.inp_undone_err = inp_ud_flag;
		frc_devp->frc_sts.inp_undone_cnt++;
		if (frc_devp->ud_dbg.inpud_dbg_en != 0) {
			if (frc_devp->frc_sts.inp_undone_cnt % 0x30 == 0) {
				PR_ERR("inp_ud_err=0x%x,err_cnt=%d,vs_cnt=%d\n",
					inp_ud_flag,
					frc_devp->frc_sts.inp_undone_cnt,
					frc_devp->in_sts.vs_cnt);
			}
		}
		timeout = 0;
		do {
			readval = READ_FRC_BITS(FRC_INP_UE_CLR, 0, 6);
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
		WRITE_FRC_REG(FRC_INP_CSC_OFFSET_INP01, pre_offset01);
		WRITE_FRC_REG(FRC_INP_CSC_OFFSET_INP2, pre_offset2);
		WRITE_FRC_REG(FRC_INP_CSC_COEF_00_01, coef00_01);
		WRITE_FRC_REG(FRC_INP_CSC_COEF_02_10, coef02_10);
		WRITE_FRC_REG(FRC_INP_CSC_COEF_11_12, coef11_12);
		WRITE_FRC_REG(FRC_INP_CSC_COEF_20_21, coef20_21);
		WRITE_FRC_REG(FRC_INP_CSC_COEF_22, coef22);
		WRITE_FRC_REG(FRC_INP_CSC_OFFSET_OUP01, pst_offset01);
		WRITE_FRC_REG(FRC_INP_CSC_OFFSET_OUP2, pst_offset2);
		WRITE_FRC_BITS(FRC_INP_CSC_CTRL, en, 3, 1);
		break;
	case FRC_OUTPUT_CSC:
		WRITE_FRC_REG(FRC_MC_CSC_OFFSET_INP01, pre_offset01);
		WRITE_FRC_REG(FRC_MC_CSC_OFFSET_INP2, pre_offset2);
		WRITE_FRC_REG(FRC_MC_CSC_COEF_00_01, coef00_01);
		WRITE_FRC_REG(FRC_MC_CSC_COEF_02_10, coef02_10);
		WRITE_FRC_REG(FRC_MC_CSC_COEF_11_12, coef11_12);
		WRITE_FRC_REG(FRC_MC_CSC_COEF_20_21, coef20_21);
		WRITE_FRC_REG(FRC_MC_CSC_COEF_22, coef22);
		WRITE_FRC_REG(FRC_MC_CSC_OFFSET_OUP01, pst_offset01);
		WRITE_FRC_REG(FRC_MC_CSC_OFFSET_OUP2, pst_offset2);
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
	WRITE_FRC_BITS(FRC_REG_INP_MODULE_EN, en, 6, 1);
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
	pr_frc(1, "%s\n", __func__);
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
	/*no loss*/
	frc_top->mc_loss_en = true;
	frc_top->me_loss_en = true;
	pr_frc(1, "top in: hsize:%d, vsize:%d\n", frc_top->hsize, frc_top->vsize);
	pr_frc(1, "top out: hsize:%d, vsize:%d\n", frc_top->out_hsize, frc_top->out_vsize);
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

	frc_input_init(frc_devp, frc_top);
	config_phs_lut(frc_top->frc_ratio_mode, frc_top->film_mode);
	config_phs_regs(frc_top->frc_ratio_mode, frc_top->film_mode);
	pr_frc(log, "%s\n", __func__);
	//Config frc input size
	WRITE_FRC_BITS(FRC_FRAME_SIZE ,(frc_top->vsize <<16) | frc_top->hsize  , 0  ,32 );

	/*!!!!!!!!! tread de, vpu register*/
	frc_top->vfb = vpu_reg_read(ENCL_VIDEO_VAVON_BLINE);
	pr_frc(log, "ENCL_VIDEO_VAVON_BLINE:%d\n", frc_top->vfb);
	//(frc_top->vfb / 4) * 3; 3/4 point of front vblank, default
	reg_mc_out_line = frc_init_out_line();
	adj_mc_dly = frc_devp->out_line;    // from user debug

	// reg_me_dly_vofst = reg_mc_out_line;
	reg_me_dly_vofst = reg_mc_dly_vofst0;  // change for keep me frist run
	if (frc_top->hsize <= 1920 && (frc_top->hsize * frc_top->vsize <= 1920 * 1080)) {
		frc_top->is_me1mc4 = 0;/*me:mc 1:2*/
		WRITE_FRC_BITS(FRC_INPUT_SIZE_ALIGN, 0, 0, 1); //8*8 align
		WRITE_FRC_BITS(FRC_INPUT_SIZE_ALIGN, 0, 1, 1); //8*8 align
	} else {
		frc_top->is_me1mc4 = 1;/*me:mc 1:4*/
		WRITE_FRC_BITS(FRC_INPUT_SIZE_ALIGN, 1, 0, 1); //16*16 align
		WRITE_FRC_BITS(FRC_INPUT_SIZE_ALIGN, 1, 1, 1); //16*16 align
	}
	// little window
	if (frc_top->hsize * frc_top->vsize <
		frc_top->out_hsize * frc_top->out_vsize &&
		frc_top->out_hsize * frc_top->out_vsize >= 3840 * 1080) {
		frc_top->is_me1mc4 = 1;/*me:mc 1:4*/
		WRITE_FRC_BITS(FRC_INPUT_SIZE_ALIGN, 1, 0, 1); //16*16 align
		WRITE_FRC_BITS(FRC_INPUT_SIZE_ALIGN, 1, 1, 1); //16*16 align
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
		WRITE_FRC_BITS(FRC_INPUT_SIZE_ALIGN, 0, 0, 1); //8*8 align
		WRITE_FRC_BITS(FRC_INPUT_SIZE_ALIGN, 0, 1, 1); //8*8 align
		pr_frc(log, " %s The resolution of the input is not standard\n", __func__);
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
	WRITE_FRC_REG(FRC_REG_TOP_CTRL27, frc_vporch_cal);

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
		WRITE_FRC_BITS(FRC_REG_TOP_CTRL14,
			(reg_post_dly_vofst + frc_porch_delta), 0, 16);
		WRITE_FRC_BITS(FRC_REG_TOP_CTRL14,
			(reg_me_dly_vofst  + frc_porch_delta), 16, 16);
		WRITE_FRC_BITS(FRC_REG_TOP_CTRL15,
			(reg_mc_dly_vofst0 + frc_porch_delta), 0, 16);
		WRITE_FRC_BITS(FRC_REG_TOP_CTRL15,
			(reg_mc_dly_vofst1 + frc_porch_delta), 16, 16);
	} else {
		/*T3 revB*/
		frc_v_porch = frc_vporch_cal;
		pr_frc(2, "%s T3 revB chip validation\n", __func__);
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
		WRITE_FRC_BITS(FRC_REG_TOP_CTRL14, reg_post_dly_vofst, 0, 16);
		WRITE_FRC_BITS(FRC_REG_TOP_CTRL14, reg_me_dly_vofst, 16, 16);
		WRITE_FRC_BITS(FRC_REG_TOP_CTRL15, reg_mc_dly_vofst0, 0, 16);
		WRITE_FRC_BITS(FRC_REG_TOP_CTRL15, reg_mc_dly_vofst1, 16,  16);
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

	WRITE_FRC_BITS(FRC_OUT_HOLD_CTRL, me_hold_line, 0, 8);
	WRITE_FRC_BITS(FRC_OUT_HOLD_CTRL, mc_hold_line, 8, 8);

	WRITE_FRC_BITS(FRC_INP_HOLD_CTRL, inp_hold_line, 0, 13);//default 6

	//iplogo en

	////Config inp
	// frc_inp_init(frc_top->frc_fb_num, frc_top->film_hwfw_sel);

	////Config phs_lut    frc_ratio_mode: frequence ratio 30in/60 out 1 : 2
	// config_phs_lut(frc_top->frc_ratio_mode, frc_top->film_mode);
	// config_phs_regs(frc_top->frc_ratio_mode, frc_top->film_mode);

	//initial reg get from jitao must config before this function
	//config_me_top_hw_reg/sys_fw_param_frc_init/init_bb_xyxy
	// frc_internal_initial();
	////config_me_top_hw_reg
	// config_me_top_hw_reg();//have initial config before this function
	////Config bb
	sys_fw_param_frc_init(frc_top->hsize, frc_top->vsize, frc_top->is_me1mc4);
	init_bb_xyxy(frc_top->hsize, frc_top->vsize, frc_top->is_me1mc4);

	//loss ena or disable, default is disable
	// cfg_me_loss(frc_top->me_loss_en);
	// cfg_mc_loss(frc_top->mc_loss_en);

	/*protect mode, enable: memc delay 2 frame*/
	/*disable: memc delay n frame, n depend on candence, for debug*/
	if (frc_top->frc_prot_mode) {
		WRITE_FRC_BITS(FRC_REG_TOP_CTRL9, 1, 24, 4);//dly_num =1
		WRITE_FRC_BITS(FRC_REG_TOP_CTRL17, 1, 8, 1);//buf prot open
	}
}

/*buffer number can dynamic kmalloc,  film_hwfw_sel ????*/
void frc_inp_init(u32 frc_fb_num, u32 film_hwfw_sel)
{
	WRITE_FRC_BITS(FRC_FRAME_BUFFER_NUM             ,frc_fb_num    ,0 ,5 );//set   frc_fb_num
	WRITE_FRC_BITS(FRC_FRAME_BUFFER_NUM             ,frc_fb_num    ,8 ,5 );//set   frc_fb_num
	WRITE_FRC_BITS(FRC_MC_PRB_CTRL1                 ,1             ,8 ,1 );//close reg_mc_probe_en
	WRITE_FRC_BITS(FRC_REG_INP_MODULE_EN, 1, 5, 1);//close reg_inp_logo_en
	WRITE_FRC_BITS(FRC_REG_INP_MODULE_EN, 1, 8, 1);//open
	WRITE_FRC_BITS(FRC_REG_INP_MODULE_EN, 1, 7, 1);//open  bbd en
	WRITE_FRC_BITS(FRC_REG_TOP_CTRL25               ,0x4080200     ,0 ,31);//aligned padding value
	//WRITE_FRC_BITS(FRC_REG_FILM_PHS_1               ,film_hwfw_sel ,16,1 );
	WRITE_FRC_BITS(FRC_MEVP_CTRL0, 0, 0, 1);//close hme

	/*for fw signal latch, should always enable*/
	WRITE_FRC_BITS(FRC_REG_MODE_OPT, 1, 1, 1);
	WRITE_FRC_BITS(FRC_ME_E2E_CHK_EN - HME_ME_OFFSET,0         ,24,1 );//todo init hme
}

void config_phs_lut(enum frc_ratio_mode_type frc_ratio_mode,
	enum en_film_mode film_mode)
{
	#define lut_ary_size	18
	u32 input_n;
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
		//    //candence=22224(10),table_cnt=  12,match_data=0xaa8
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
		//    //candence=33(11),table_cnt=   3,match_data=0x4
		//    //    pha_start,plogo_diff,clogo_diff,pfrm_diff,cfrm_diff,nfrm_diff,mc_phase,me_phase,film_phase,frc_phase
		//    phs_lut_table[0]={ 1'd1,   4'd3,      4'd2,      4'd5,     4'd4,    4'd1,     8'd0,   8'd42,     8'd1,       8'd0};
		//    phs_lut_table[1]={ 1'd0,   4'd3,      4'd2,      4'd6,     4'd4,    4'd2,     8'd42,   8'd42,     8'd2,       8'd0};
		//    phs_lut_table[2]={ 1'd0,   4'd3,      4'd2,      4'd7,     4'd4,    4'd3,     8'd85,   8'd85,     8'd0,       8'd0};
		//}
		//else if(film_mode == EN_FILM334) {
		//    //candence=334(12),table_cnt=  10,match_data=0x248
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
		//    //candence=55(13),table_cnt=   5,match_data=0x10
		//    //               pha_start,plogo_diff,clogo_diff,pfrm_diff,cfrm_diff,nfrm_diff,mc_phase,me_phase,film_phase,frc_phase
		//    phs_lut_table[0]={ 1'd1,      4'd3,      4'd2,      4'd7,     4'd6,    4'd1,     8'd0,    8'd25,     8'd1,     8'd0};
		//    phs_lut_table[1]={ 1'd0,      4'd3,      4'd2,      4'd8,     4'd6,    4'd2,     8'd25,   8'd25,     8'd2,     8'd0};
		//    phs_lut_table[2]={ 1'd0,      4'd3,      4'd2,      4'd9,     4'd6,    4'd3,     8'd51,   8'd51,     8'd3,     8'd0};
		//    phs_lut_table[3]={ 1'd0,      4'd3,      4'd2,      4'd10,    4'd6,    4'd4,     8'd76,   8'd76,     8'd4,     8'd0};
		//    phs_lut_table[4]={ 1'd0,      4'd3,      4'd2,      4'd11,    4'd6,    4'd5,     8'd102,  8'd102,    8'd0,     8'd0};
		//
		//}
		//else if(film_mode == EN_FILM64) {
		//    //candence=64(14),table_cnt=  10,match_data=0x220
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
		//    //candence=66(15),table_cnt=   6,match_data=0x20
		//    //    pha_start,plogo_diff,clogo_diff,pfrm_diff,cfrm_diff,nfrm_diff,mc_phase,me_phase,film_phase,frc_phase
		//    phs_lut_table[0]={ 1'd1,   4'd3,      4'd2,      4'd8,     4'd7,    4'd1,     8'd0,   8'd21,     8'd1,       8'd0};
		//    phs_lut_table[1]={ 1'd0,   4'd3,      4'd2,      4'd9,     4'd7,    4'd2,     8'd21,   8'd21,     8'd2,       8'd0};
		//    phs_lut_table[2]={ 1'd0,   4'd3,      4'd2,      4'd10,     4'd7,    4'd3,     8'd42,   8'd42,     8'd3,       8'd0};
		//    phs_lut_table[3]={ 1'd0,   4'd3,      4'd2,      4'd11,     4'd7,    4'd4,     8'd64,   8'd64,     8'd4,       8'd0};
		//    phs_lut_table[4]={ 1'd0,   4'd3,      4'd2,      4'd12,     4'd7,    4'd5,     8'd85,   8'd85,     8'd5,       8'd0};
		//    phs_lut_table[5]={ 1'd0,   4'd3,      4'd2,      4'd13,     4'd7,    4'd6,     8'd106,   8'd106,     8'd0,       8'd0};
		//}
		//else if(film_mode == EN_FILM87) {
		//    //candence=87(16),table_cnt=  15,match_data=0x4080
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
		//    //candence=212(17),table_cnt=   5,match_data=0x1a
		//    //pha_start,plogo_diff,clogo_diff,pfrm_diff,cfrm_diff,nfrm_diff,mc_phase,me_phase,film_phase,frc_phase
		//    phs_lut_table[0]={ 1'd1,   4'd3,      4'd2,      4'd5,     4'd3,    4'd1,     8'd0,   8'd76,      8'd1,       8'd0};
		//    phs_lut_table[1]={ 1'd0,   4'd4,      4'd3,      4'd6,     4'd3,    4'd2,     8'd76,   8'd76,     8'd2,       8'd0};
		//    phs_lut_table[2]={ 1'd1,   4'd3,      4'd2,      4'd4,     4'd3,    4'd2,     8'd25,   8'd25,     8'd3,       8'd0};
		//    phs_lut_table[3]={ 1'd0,   4'd4,      4'd3,      4'd5,     4'd4,    4'd3,     8'd102,   8'd102,   8'd4,       8'd0};
		//    phs_lut_table[4]={ 1'd1,   4'd3,      4'd2,      4'd5,     4'd4,    4'd2,     8'd51,   8'd51,     8'd0,       8'd0};
		//}
		//else {
		//      stimulus_print ("==== USE ERROR CANDENCE ====");
		//}

		input_n          = 1;
		output_m         = 1;
	}

	WRITE_FRC_BITS(FRC_REG_TOP_CTRL17,1,3,1);//lut_cfg_en

	WRITE_FRC_REG(FRC_TOP_LUT_ADDR, 0);
	for (i = 0; i < 256; i++) {
		if (i < lut_ary_size) {
			WRITE_FRC_REG(FRC_TOP_LUT_DATA, phs_lut_table[i]       & 0xffffffff);
			WRITE_FRC_REG(FRC_TOP_LUT_DATA, phs_lut_table[i] >> 32 & 0xffffffff);
		} else {
			WRITE_FRC_REG(FRC_TOP_LUT_DATA, 0xffffffff);
			WRITE_FRC_REG(FRC_TOP_LUT_DATA, 0xffffffff);
		}
	}

	WRITE_FRC_BITS(FRC_REG_TOP_CTRL17,1,4,1);//lut_cfg_en

	WRITE_FRC_BITS(FRC_REG_PHS_TABLE,input_n ,24,8);
	WRITE_FRC_BITS(FRC_REG_PHS_TABLE,output_m,16,8);
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
		}
		else {
			  pr_frc(0, "==== USE ERROR CANDENCE ====\n");
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
	WRITE_FRC_BITS(FRC_REG_TOP_CTRL18,(inp_frm_vld_lut >> 0 ) & 0xffffffff , 0  ,32 );//input [63:0] reg_inp_frm_vld_lut;
	WRITE_FRC_BITS(FRC_REG_TOP_CTRL19,(inp_frm_vld_lut >> 32) & 0xffffffff , 0  ,32 );//input [63:0] reg_inp_frm_vld_lut;

	WRITE_FRC_BITS(FRC_REG_PD_PAT_NUM ,reg_frc_pd_pat_num ,0 ,32);
	WRITE_FRC_BITS(FRC_REG_TOP_CTRL9  ,reg_out_frm_dly_num,24,4);

	WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_0,(reg_ip_film_end >> 0 ) & 0x1,27  ,1);
	WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_0,(reg_ip_film_end >> 1 ) & 0x1,19  ,1);
	WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_0,(reg_ip_film_end >> 2 ) & 0x1,11  ,1);
	WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_0,(reg_ip_film_end >> 3 ) & 0x1,3   ,1);
	WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_1,(reg_ip_film_end >> 4 ) & 0x1,27  ,1);
	WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_1,(reg_ip_film_end >> 5 ) & 0x1,19  ,1);
	WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_1,(reg_ip_film_end >> 6 ) & 0x1,11  ,1);
	WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_1,(reg_ip_film_end >> 7 ) & 0x1,3   ,1);
	WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_2,(reg_ip_film_end >> 8 ) & 0x1,27  ,1);
	WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_2,(reg_ip_film_end >> 9 ) & 0x1,19  ,1);
	WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_2,(reg_ip_film_end >> 10) & 0x1,11  ,1);
	WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_2,(reg_ip_film_end >> 11) & 0x1,3   ,1);
	WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_3,(reg_ip_film_end >> 12) & 0x1,27  ,1);
	WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_3,(reg_ip_film_end >> 13) & 0x1,19  ,1);
	WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_3,(reg_ip_film_end >> 14) & 0x1,11  ,1);
	WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_3,(reg_ip_film_end >> 15) & 0x1,3   ,1);

	if(film_mode == EN_FILM87 && frc_ratio_mode == FRC_RATIO_1_1){
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_0,reg_ip_incr[0]  ,28 ,4);
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_0,reg_ip_incr[1]  ,20 ,4);
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_0,reg_ip_incr[2]  ,12 ,4);
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_0,reg_ip_incr[3]  ,4  ,4);

		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_1,reg_ip_incr[4]  ,28 ,4);
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_1,reg_ip_incr[5]  ,20 ,4);
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_1,reg_ip_incr[6]  ,12 ,4);
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_1,reg_ip_incr[7]  ,4  ,4);

		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_2,reg_ip_incr[8 ],28  ,4);
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_2,reg_ip_incr[9 ],20  ,4);
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_2,reg_ip_incr[10],12  ,4);
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_2,reg_ip_incr[11],4   ,4);

		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_3,reg_ip_incr[12],28  ,4);
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_3,reg_ip_incr[13],20  ,4);
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_3,reg_ip_incr[14],12  ,4);
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_3,reg_ip_incr[15],4   ,4);
	}
	else {

		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_0,0              ,28 ,4);
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_0,0              ,20 ,4);
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_0,0              ,12 ,4);
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_0,0              ,4  ,4);

		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_1,0              ,28 ,4);
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_1,0              ,20 ,4);
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_1,0              ,12 ,4);
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_1,0              ,4  ,4);

		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_2,0             ,28  ,4);
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_2,0             ,20  ,4);
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_2,0             ,12  ,4);
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_2,0             ,4   ,4);

		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_3,0             ,28  ,4);
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_3,0             ,20  ,4);
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_3,0             ,12  ,4);
		WRITE_FRC_BITS(FRC_REG_LOAD_ORG_FRAME_3,0             ,4   ,4);
	}
}
EXPORT_SYMBOL(config_phs_regs);

void config_me_top_hw_reg(void)
{
	u32    reg_data;
	u32    mvx_div_mode;
	u32    mvy_div_mode;
	u32    hmvx_div_mode;
	u32    hmvy_div_mode;
	u32    me_max_mvx  ;
	u32    me_max_mvy  ;
	u32    hme_max_mvx ;
	u32    hme_max_mvy ;

	reg_data = READ_FRC_REG(FRC_REG_BLK_SIZE_XY);
	mvx_div_mode = (reg_data>>14) & 0x3;
	mvy_div_mode = (reg_data>>12) & 0x3;

	me_max_mvx   = (MAX_ME_MVX << mvx_div_mode  < (1<<(ME_MVX_BIT-1))-1) ? (MAX_ME_MVX << mvx_div_mode) :  ((1<<(ME_MVX_BIT-1))-1);
	me_max_mvy   = (MAX_ME_MVY << mvy_div_mode  < (1<<(ME_MVY_BIT-1))-1) ? (MAX_ME_MVY << mvy_div_mode) :  ((1<<(ME_MVY_BIT-1))-1);

	WRITE_FRC_BITS(FRC_ME_CMV_MAX_MV, me_max_mvx , 16, 12);
	WRITE_FRC_BITS(FRC_ME_CMV_MAX_MV, me_max_mvy , 0 , 11);

	reg_data = READ_FRC_REG(FRC_REG_BLK_SIZE_XY);
	hmvx_div_mode = (reg_data >> 2) & 0x3;
	hmvy_div_mode = (reg_data >> 0) & 0x3;

	hme_max_mvx  = (MAX_HME_MVX << hmvx_div_mode  < (1<<(HME_MVX_BIT-1))-1) ? (MAX_HME_MVX << hmvx_div_mode) :  ((1<<(HME_MVX_BIT-1))-1);
	hme_max_mvy  = (MAX_HME_MVY << hmvy_div_mode  < (1<<(HME_MVY_BIT-1))-1) ? (MAX_HME_MVY << hmvy_div_mode) :  ((1<<(HME_MVY_BIT-1))-1);

	WRITE_FRC_BITS(FRC_HME_CMV_MAX_MV, hme_max_mvx , 16, 12);
	WRITE_FRC_BITS(FRC_HME_CMV_MAX_MV, hme_max_mvy , 0 , 11);

	WRITE_FRC_BITS(FRC_ME_CMV_CTRL, 1, 31, 1);
	WRITE_FRC_BITS(FRC_ME_CMV_CTRL, 0, 31, 1);
}

void init_bb_xyxy(u32 hsize, u32 vsize, u32 is_me1mc4)
{
	u32     reg_data;
	u32     me_width;
	u32     me_height;
	u32     hme_width;
	u32     hme_height;
	u32     me_blk_xsize;
	u32     me_blk_ysize;
	u32     hme_blk_xsize;
	u32     hme_blk_ysize;
	u32     me_dsx;
	u32     me_dsy;
	u32     hme_dsx;
	u32     hme_dsy;
	u32     me_blksize_dsx;
	u32     me_blksize_dsy;
	u32     hme_blksize_dsx;
	u32     hme_blksize_dsy;
	u32     hsize_align, vsize_align;

	reg_data = READ_FRC_REG(FRC_REG_ME_HME_SCALE);

	me_dsx  = (reg_data>>6) & 0x3;
	me_dsy  = (reg_data>>4) & 0x3;
	hme_dsx = (reg_data>>2) & 0x3;
	hme_dsy = (reg_data)    & 0x3;

	reg_data = READ_FRC_REG(FRC_REG_ME_HME_SCALE);
	me_dsx  = (reg_data>>6) & 0x3;
	me_dsy  = (reg_data>>4) & 0x3;
	hme_dsx = (reg_data>>2) & 0x3;
	hme_dsy = (reg_data>>0) & 0x3;

	reg_data = READ_FRC_REG(FRC_REG_BLK_SIZE_XY);
	me_blksize_dsx  = (reg_data>>19) & 0x7;
	me_blksize_dsy  = (reg_data>>16) & 0x7;
	hme_blksize_dsx = (reg_data>>7)  & 0x7;
	hme_blksize_dsy = (reg_data>>4)  & 0x7;

	//TODO match with reg_file.txt
	//me
	WRITE_FRC_BITS(FRC_ME_PRE_LBUF_OFST,  43, 16, 15);
	WRITE_FRC_BITS(FRC_ME_PRE_LBUF_OFST, -48, 0 , 15);
	WRITE_FRC_BITS(FRC_ME_CUR_LBUF_OFST,  43, 16, 15);
	WRITE_FRC_BITS(FRC_ME_CUR_LBUF_OFST, -48, 0 , 15);
	WRITE_FRC_BITS(FRC_ME_NEX_LBUF_OFST,  43, 16, 15);
	WRITE_FRC_BITS(FRC_ME_NEX_LBUF_OFST, -48, 0 , 15);

	WRITE_FRC_BITS(FRC_ME_LBUF_NUM, 92,  0, 8);
	WRITE_FRC_BITS(FRC_ME_LBUF_NUM, 92,  8, 8);
	WRITE_FRC_BITS(FRC_ME_LBUF_NUM, 92, 16, 8);

	//hme
	WRITE_FRC_BITS(FRC_ME_PRE_LBUF_OFST - HME_ME_OFFSET,43 ,16,16);
	WRITE_FRC_BITS(FRC_ME_PRE_LBUF_OFST - HME_ME_OFFSET,-48,0 ,16);
	WRITE_FRC_BITS(FRC_ME_CUR_LBUF_OFST - HME_ME_OFFSET,43 ,16,16);
	WRITE_FRC_BITS(FRC_ME_CUR_LBUF_OFST - HME_ME_OFFSET,-48,0 ,16);
	WRITE_FRC_BITS(FRC_ME_NEX_LBUF_OFST - HME_ME_OFFSET,43 ,16,16);
	WRITE_FRC_BITS(FRC_ME_NEX_LBUF_OFST - HME_ME_OFFSET,-48,0 ,16);

	//me  me_blksize_dsx me_blksize_dsy-> FRC_REG_BLK_SIZE_XY
	if(is_me1mc4 == 0) {
		hsize_align = (hsize + 7) / 8 * 8;
		vsize_align = (vsize + 7) / 8 * 8;
	} else {
		hsize_align = (hsize + 15) / 16 * 16;
		vsize_align = (vsize + 15) / 16 * 16;
	}
	me_width      = (hsize_align+(1<<me_dsx)-1)/(1<<me_dsx);/*960*/
	me_height     = (vsize_align+(1<<me_dsy)-1)/(1<<me_dsy);/*540*/
	hme_width     = (me_width+(1<<hme_dsx)-1)/(1<<hme_dsx);/*240*/
	hme_height    = (me_height+(1<<hme_dsy)-1)/(1<<hme_dsy);/*135*/
	me_blk_xsize  = (me_width+(1<<me_blksize_dsx)-1)/(1<<me_blksize_dsx);/*240*/
	me_blk_ysize  = (me_height+(1<<me_blksize_dsy)-1)/(1<<me_blksize_dsy);/*135*/
	hme_blk_xsize = (hme_width+(1<<hme_blksize_dsx)-1)/(1<<hme_blksize_dsx);/*60*/
	hme_blk_ysize = (hme_height+(1<<hme_blksize_dsy)-1)/(1<<hme_blksize_dsy);/*34*/
	pr_frc(1, "me_width = %d\n", me_width);
	pr_frc(1, "me_height = %d\n", me_height);
	pr_frc(1, "hme_width = %d\n", hme_width);
	pr_frc(1, "hme_height = %d\n", hme_height);
	pr_frc(1, "me_blk_xsize = %d\n", me_blk_xsize);
	pr_frc(1, "me_blk_ysize = %d\n", me_blk_ysize);
	pr_frc(1, "hme_blk_xsize = %d\n", hme_blk_xsize);
	pr_frc(1, "hme_blk_ysize = %d\n", hme_blk_ysize);

	WRITE_FRC_BITS(FRC_ME_BB_PIX_ED, me_width-1,     16, 12);/*reg_me_bb_xyxy[2] 60*/
	WRITE_FRC_BITS(FRC_ME_BB_PIX_ED, me_height-1,     0, 12);/*reg_me_bb_xyxy[3] 40*/
	WRITE_FRC_BITS(FRC_ME_BB_BLK_ED, me_blk_xsize-1, 16, 10);/*reg_me_bb_blk_xyxy[2] 240*/
	WRITE_FRC_BITS(FRC_ME_BB_BLK_ED, me_blk_ysize-1,  0, 10);/*reg_me_bb_blk_xyxy[3] 135*/
	WRITE_FRC_BITS(FRC_ME_STAT_12R_HST, 0, 0,10);/*reg_me_stat_region_hend 0*/
	WRITE_FRC_BITS(FRC_ME_STAT_12R_H01, me_blk_xsize*1/4, 16, 10);/*reg_me_stat_region_hend 60*/
	WRITE_FRC_BITS(FRC_ME_STAT_12R_H01, me_blk_xsize*2/4, 0, 10);/*reg_me_stat_region_hend 120*/
	WRITE_FRC_BITS(FRC_ME_STAT_12R_H23, me_blk_xsize*3/4, 16, 10);/*reg_me_stat_region_hend 180*/
	WRITE_FRC_BITS(FRC_ME_STAT_12R_H23, me_blk_xsize*4/4 - 1, 0, 10);/*reg_me_stat_region_hend 240*/

	WRITE_FRC_BITS(FRC_ME_STAT_12R_V0    , 0, 16, 10);
	WRITE_FRC_BITS(FRC_ME_STAT_12R_V0    , me_blk_ysize*1/3, 0, 10);/*reg_me_stat_region_vend[0]: 45*/
	WRITE_FRC_BITS(FRC_ME_STAT_12R_V1    , me_blk_ysize*2/3, 16, 10);/*reg_me_stat_region_vend[1]: 90*/
	WRITE_FRC_BITS(FRC_ME_STAT_12R_V1    , me_blk_ysize*3/3 - 1, 0, 10);/*reg_me_stat_region_vend[2]: 135*/

	//hme
	WRITE_FRC_BITS(FRC_ME_BB_PIX_ED- HME_ME_OFFSET, hme_width -1, 16, 12);/*hme w 240*/
	WRITE_FRC_BITS(FRC_ME_BB_PIX_ED- HME_ME_OFFSET, hme_height-1, 0, 12);/*hme h 135*/
	WRITE_FRC_BITS(FRC_ME_BB_BLK_ED- HME_ME_OFFSET,hme_blk_xsize-1, 16, 10);/*hme w block 60*/
	WRITE_FRC_BITS(FRC_ME_BB_BLK_ED- HME_ME_OFFSET,hme_blk_ysize-1, 0, 10);/*hme w block 34*/
	WRITE_FRC_BITS(FRC_ME_STAT_12R_HST - HME_ME_OFFSET, 0,16,10);
	WRITE_FRC_BITS(FRC_ME_STAT_12R_H01 - HME_ME_OFFSET, hme_blk_xsize*1/4, 16, 10);
	WRITE_FRC_BITS(FRC_ME_STAT_12R_H01 - HME_ME_OFFSET, hme_blk_xsize*2/4, 0, 10);
	WRITE_FRC_BITS(FRC_ME_STAT_12R_H23 - HME_ME_OFFSET, hme_blk_xsize*3/4, 16, 10);
	WRITE_FRC_BITS(FRC_ME_STAT_12R_H23 - HME_ME_OFFSET, hme_blk_xsize*4/4 - 1, 0, 10);
	WRITE_FRC_BITS(FRC_ME_STAT_12R_V0  - HME_ME_OFFSET, hme_blk_ysize*1/3, 0, 10);
	WRITE_FRC_BITS(FRC_ME_STAT_12R_V1  - HME_ME_OFFSET, hme_blk_ysize*2/3, 16, 10);
	WRITE_FRC_BITS(FRC_ME_STAT_12R_V1  - HME_ME_OFFSET, hme_blk_ysize*3/3 - 1, 0, 10);

	//vp
	WRITE_FRC_BITS(FRC_VP_BB_1, 0, 0, 16);
	WRITE_FRC_BITS(FRC_VP_BB_1, 0, 16, 16);
	WRITE_FRC_BITS(FRC_VP_BB_2, me_blk_xsize-1, 0, 16);/*reg_vp_bb_xyxy[2] 240*/
	WRITE_FRC_BITS(FRC_VP_BB_2, me_blk_ysize-1, 16, 16);/*reg_vp_bb_xyxy[3] 135*/

	WRITE_FRC_BITS(FRC_VP_ME_BB_1 , 0, 0, 16);
	WRITE_FRC_BITS(FRC_VP_ME_BB_1 , 0, 16, 16);
	WRITE_FRC_BITS(FRC_VP_ME_BB_2 , me_blk_xsize-1,0 ,16);/*reg_vp_me_bb_blk_xyxy[2] 240*/
	WRITE_FRC_BITS(FRC_VP_ME_BB_2 , me_blk_ysize-1,16,16);/*reg_vp_me_bb_blk_xyxy[3] 135*/

	WRITE_FRC_BITS(FRC_VP_REGION_WINDOW_1, 0, 0, 8);
	WRITE_FRC_BITS(FRC_VP_REGION_WINDOW_1, me_blk_xsize*1/4, 8, 12);/*reg_vp_stat_region_hend[0] 60*/
	WRITE_FRC_BITS(FRC_VP_REGION_WINDOW_1, me_blk_xsize*2/4, 20, 12);/*reg_vp_stat_region_hend[1] 120*/
	WRITE_FRC_BITS(FRC_VP_REGION_WINDOW_2, me_blk_xsize*3/4, 12, 12);/*reg_vp_stat_region_hend[2] 180*/
	WRITE_FRC_BITS(FRC_VP_REGION_WINDOW_2, me_blk_xsize*4/4 - 1, 0, 12);/*reg_vp_stat_region_hend[3] 240*/

	WRITE_FRC_BITS(FRC_VP_REGION_WINDOW_2, 0, 24, 8);
	WRITE_FRC_BITS(FRC_VP_REGION_WINDOW_3, me_blk_ysize * 1 / 3, 0, 8);/*reg_vp_stat_region_vend[0] 45*/
	WRITE_FRC_BITS(FRC_VP_REGION_WINDOW_3, me_blk_ysize * 2 / 3, 8, 12);/*reg_vp_stat_region_vend[1] 90*/
	WRITE_FRC_BITS(FRC_VP_REGION_WINDOW_3, me_blk_ysize * 3 / 3 - 1, 20, 12);/*reg_vp_stat_region_vend[2] 135*/

	//mc:
	WRITE_FRC_BITS(FRC_MC_BB_HANDLE_ME_BLK_BB_XYXY_RIT_AND_BOT, me_blk_xsize-1, 16, 16);
	WRITE_FRC_BITS(FRC_MC_BB_HANDLE_ME_BLK_BB_XYXY_RIT_AND_BOT, me_blk_ysize-1,  0, 16);

	//melogo:
	WRITE_FRC_BITS(FRC_MELOGO_BB_BLK_ST, 0, 16, 10);
	WRITE_FRC_BITS(FRC_MELOGO_BB_BLK_ST, 0,  0, 10);
	WRITE_FRC_BITS(FRC_MELOGO_BB_BLK_ED, me_blk_xsize-1, 16, 10);/*reg_melogo_bb_blk_xyxy[2] 240*/
	WRITE_FRC_BITS(FRC_MELOGO_BB_BLK_ED, me_blk_ysize-1,  0, 10);/*reg_melogo_bb_blk_xyxy[3] 135*/
	WRITE_FRC_BITS( FRC_MELOGO_REGION_HWINDOW_0, 0, 0, 10);
	WRITE_FRC_BITS( FRC_MELOGO_REGION_HWINDOW_1, me_blk_xsize*1/4,0 ,10);/*reg_melogo_stat_region_hend[0] 60*/
	WRITE_FRC_BITS( FRC_MELOGO_REGION_HWINDOW_1, me_blk_xsize*2/4,16,10);/*reg_melogo_stat_region_hend[1] 120*/
	WRITE_FRC_BITS( FRC_MELOGO_REGION_HWINDOW_2, me_blk_xsize*3/4,0 ,10);/*reg_melogo_stat_region_hend[2] 180*/
	WRITE_FRC_BITS( FRC_MELOGO_REGION_HWINDOW_2, me_blk_xsize*4/4 - 1,16,10);/*reg_melogo_stat_region_hend[3] 240*/

	WRITE_FRC_BITS( FRC_MELOGO_REGION_VWINDOW_0, 0, 0, 10);
	WRITE_FRC_BITS( FRC_MELOGO_REGION_VWINDOW_0, me_blk_ysize*1/3,16,10);/*reg_melogo_stat_region_vend[0] 45*/
	WRITE_FRC_BITS( FRC_MELOGO_REGION_VWINDOW_1, me_blk_ysize*2/3,0 ,10);/*reg_melogo_stat_region_vend[1] 90*/
	WRITE_FRC_BITS( FRC_MELOGO_REGION_VWINDOW_1, me_blk_ysize*3/3 - 1,16,10);/*reg_melogo_stat_region_vend[2] 135*/

	//IPLOGO
	WRITE_FRC_BITS( FRC_IPLOGO_BB_PIX_ST, 0, 16, 12);
	WRITE_FRC_BITS( FRC_IPLOGO_BB_PIX_ST, 0, 0, 12);
	WRITE_FRC_BITS( FRC_IPLOGO_BB_PIX_ED, me_width - 1, 16, 12);/*reg_iplogo_bb_xyxy[2] 960*/
	WRITE_FRC_BITS( FRC_IPLOGO_BB_PIX_ED, me_height - 1, 0, 12);/*reg_iplogo_bb_xyxy[3] 540*/

	WRITE_FRC_BITS(FRC_IPLOGO_REGION_HWINDOW_0, 0, 0, 10);
	WRITE_FRC_BITS(FRC_IPLOGO_REGION_HWINDOW_1, me_blk_xsize*1/4,0 ,10);/*reg_iplogo_stat_region_hend[0] 240*/
	WRITE_FRC_BITS(FRC_IPLOGO_REGION_HWINDOW_1, me_blk_xsize*2/4,16,10);/*reg_iplogo_stat_region_hend[1] 480*/
	WRITE_FRC_BITS(FRC_IPLOGO_REGION_HWINDOW_2, me_blk_xsize*3/4,0 ,10);/*reg_iplogo_stat_region_hend[2] 720*/
	WRITE_FRC_BITS(FRC_IPLOGO_REGION_HWINDOW_2, me_blk_xsize*4/4 - 1,16,10);/*reg_iplogo_stat_region_hend[3] 960*/

	WRITE_FRC_BITS(FRC_IPLOGO_REGION_VWINDOW_0, 0, 0, 10);
	WRITE_FRC_BITS(FRC_IPLOGO_REGION_VWINDOW_0, me_blk_ysize * 1 / 3, 16, 10);/*reg_iplogo_stat_region_vend[0] 180*/
	WRITE_FRC_BITS(FRC_IPLOGO_REGION_VWINDOW_1, me_blk_ysize * 2 / 3, 0, 10);/*reg_iplogo_stat_region_vend[1] 360*/
	WRITE_FRC_BITS(FRC_IPLOGO_REGION_VWINDOW_1, me_blk_ysize * 3 / 3 - 1, 16, 10);/*reg_iplogo_stat_region_vend[2] 540*/

	WRITE_FRC_BITS(FRC_LOGO_BB_RIT_BOT, hsize, 16, 16);
	WRITE_FRC_BITS(FRC_LOGO_BB_RIT_BOT, vsize, 0, 16);
}


void fw_param_bbd_init(u32 hsize ,u32 vsize)
{

	u32 reg_data;
	u32 reg_bb_top_edge;
	u32 reg_bb_lft_edge;
	u32 reg_bb_top_motion;
	u32 reg_bb_lft_motion;
	u32 reg_bb_finer;

	WRITE_FRC_BITS(FRC_BBD_TOP_BOT_EDGE_TH, ((READ_FRC_REG(FRC_BBD_TOP_BOT_EDGE_TH)>>16) & 0xfff)*hsize/1920, 16, 12);  //reg_bb_top_bot_edge_th1
	WRITE_FRC_BITS(FRC_BBD_TOP_BOT_EDGE_TH, ((READ_FRC_REG(FRC_BBD_TOP_BOT_EDGE_TH))     & 0xfff)*hsize/1920,  0, 12);  //reg_bb_top_bot_edge_th2
	WRITE_FRC_BITS(FRC_BBD_LFT_RIT_EDGE_TH, ((READ_FRC_REG(FRC_BBD_LFT_RIT_EDGE_TH)>>16) & 0xfff)*vsize/1080, 16, 12); //reg_bb_lft_rit_edge_th1
	WRITE_FRC_BITS(FRC_BBD_LFT_RIT_EDGE_TH, ((READ_FRC_REG(FRC_BBD_LFT_RIT_EDGE_TH))     & 0xfff)*vsize/1080,  0, 12); //reg_bb_lft_rit_edge_th2

	reg_data=READ_FRC_REG(FRC_BBD_ROUGH_BLK_STEP) & 0xff;
	WRITE_FRC_BITS(FRC_BBD_ROUGH_BLK_STEP, reg_data, 0, 8);  //reg_bb_rough_blk_step
	WRITE_FRC_BITS(FRC_BBD_VER_TH, ((READ_FRC_REG(FRC_BBD_VER_TH)>>16) & 0xffff)*hsize/1920, 16, 16); //reg_bb_ver_th1
	WRITE_FRC_BITS(FRC_BBD_VER_TH, ((READ_FRC_REG(FRC_BBD_VER_TH))     & 0xffff)*hsize/1920,  0, 16); //reg_bb_ver_th2
	WRITE_FRC_BITS(FRC_BBD_ROUGH_HOR_TH, ((READ_FRC_REG(FRC_BBD_ROUGH_HOR_TH)>>16) & 0xffff)*reg_data*vsize/(1080*16), 16, 16); //reg_bb_rough_hor_th1
	WRITE_FRC_BITS(FRC_BBD_ROUGH_HOR_TH, ((READ_FRC_REG(FRC_BBD_ROUGH_HOR_TH))     & 0xffff)*reg_data*vsize/(1080*16),  0, 16); //reg_bb_rough_hor_th2
	WRITE_FRC_BITS(FRC_BBD_FINER_HOR_TH, ((READ_FRC_REG(FRC_BBD_FINER_HOR_TH)>>16) & 0xffff), 16, 16); //reg_bb_finer_hor_th1
	WRITE_FRC_BITS(FRC_BBD_FINER_HOR_TH, ((READ_FRC_REG(FRC_BBD_FINER_HOR_TH))     & 0xffff),  0, 16); //reg_bb_finer_hor_th2

	reg_data = READ_FRC_REG(FRC_BBD_DETECT_EDGE_LINE_NUM);
	reg_bb_top_edge = (reg_data>>16) & 0xffff;
	reg_bb_lft_edge = (reg_data)     & 0xffff;
	WRITE_FRC_BITS(FRC_BBD_DETECT_EDGE_LINE_NUM, reg_bb_top_edge*vsize/1080, 16, 16); //reg_bb_top_bot_det_edge_line_num
	WRITE_FRC_BITS(FRC_BBD_DETECT_EDGE_LINE_NUM, reg_bb_lft_edge*hsize/1920,   0, 16); //reg_bb_lft_rit_det_edge_line_num

	reg_data = READ_FRC_REG(FRC_BBD_DETECT_MOTION_LINE_NUM);
	reg_bb_top_motion = (reg_data>>16) & 0xffff;
	reg_bb_lft_motion = reg_data       & 0xffff;
	WRITE_FRC_BITS(FRC_BBD_DETECT_MOTION_LINE_NUM, reg_bb_top_motion*vsize/1080, 16, 16); //reg_bb_top_bot_det_motion_line_num
	WRITE_FRC_BITS(FRC_BBD_DETECT_MOTION_LINE_NUM, reg_bb_lft_motion*hsize/1920,   0, 16); //reg_bb_lft_rit_det_motion_line_num

	WRITE_FRC_BITS(FRC_BBD_ROW_MOTION_TH, ((READ_FRC_REG(FRC_BBD_ROW_MOTION_TH)>>12) & 0xfff)*hsize/1920, 16, 12); //reg_bb_row_motion_th1
	WRITE_FRC_BITS(FRC_BBD_ROW_MOTION_TH, ((READ_FRC_REG(FRC_BBD_ROW_MOTION_TH))     & 0xfff)*hsize/1920,  0, 12); //reg_bb_row_motion_th2
	WRITE_FRC_BITS(FRC_BBD_COL_MOTION_TH, ((READ_FRC_REG(FRC_BBD_COL_MOTION_TH)>>12) & 0xfff)*vsize/1080, 16, 12); //reg_bb_col_motion_th1
	WRITE_FRC_BITS(FRC_BBD_COL_MOTION_TH, ((READ_FRC_REG(FRC_BBD_COL_MOTION_TH))     & 0xfff)*vsize/1080,  0, 12); //reg_bb_col_motion_th2

	//spatial
	WRITE_FRC_BITS(FRC_BBD_DETECT_EDGE_LINE_NUM,  ((reg_bb_lft_edge>>1)<<1),  0, 16); //reg_bb_lft_rit_det_edge_line_num , even

	//temporal
	//since reg_bb_motion_finer_col_num initial value is 64, so too big is not to care
	reg_data = READ_FRC_REG(FRC_BBD_MOTION_NUM);
	reg_bb_finer = reg_data & 0xff;
	if((reg_bb_lft_motion/reg_bb_finer)<4){
		 WRITE_FRC_BITS(FRC_BBD_MOTION_NUM, reg_bb_lft_motion/4, 0, 7); // reg_bb_motion_finer_col_num
		 WRITE_FRC_BITS(FRC_BBD_DETECT_MOTION_LINE_NUM, reg_bb_finer*4, 0, 16); // reg_bb_lft_rit_det_motion_line_num
	}else{
		WRITE_FRC_BITS(FRC_BBD_DETECT_MOTION_LINE_NUM, reg_bb_lft_motion/(reg_bb_finer*2)*(reg_bb_finer*2), 0, 16); //reg_bb_lft_rit_det_motion_line_num
	}
}


void sys_fw_param_frc_init(u32 frm_hsize, u32 frm_vsize, u32 is_me1mc4)
{
	u32 reg_me_mvx_div_mode;
	u32 reg_me_mvy_div_mode;
	u32 reg_demo_window1_en;
	u32 reg_demo_window2_en;
	u32 reg_demo_window3_en;
	u32 reg_demo_window4_en;
	u32 mc_org_me_bb_2;
	u32 mc_org_me_bb_3;
	u32 mc_org_me_blk_bb_2;
	u32 mc_org_me_blk_bb_3;
	u32 reg_me_dsx_scale;
	u32 reg_me_dsy_scale;
	//u32 reg_hme_dsx_scale;
	//u32 reg_hme_dsy_scale;
	u32 reg_mc_blksize_xscale;
	u32 reg_mc_blksize_yscale;
	u32 reg_mc_mvx_scale ;
	u32 reg_mc_mvy_scale ;
	u32 reg_mc_fetch_size;
	u32 reg_mc_blk_x     ;
	u32 reg_data;
	u32 reg_data1;
	u32 reg_bb_det_top;
	u32 reg_bb_det_bot;
	u32 reg_bb_det_lft;
	u32 reg_bb_det_rit;
	u32 reg_bb_det_motion_top;
	u32 reg_bb_det_motion_bot;
	u32 reg_bb_det_motion_lft;
	u32 reg_bb_det_motion_rit;
	u32 bb_mo_ds_2;
	u32 bb_mo_ds_3;
	u32 bb_oob_v_2;
	u32 me_mc_ratio;
	u32 logo_mc_ratio;


//inpRalM.ro_frc_input_ful_idx = option->start_no; // latest input frame idx from the input sequence
//`ifdef BBD_TEST
//`ifndef CFG_FPGA_REGS_FILE
//fw_param_bbd_init(frm_hsize, frm_vsize);
//`endif

	//if(ME_MC_RATIO==0) {
	//    reg_me_dsx_scale = 0 ;  // u2: downscale mode of x direction for me input data; 0: no downscale; 1:1/2 downscale; 2:1/4 downscale
	//    reg_me_dsy_scale = 0 ;  // u2: downscale mode of y direction for me input data; 0: no downscale; 1:1/2 downscale; 2:1/4 downscale
	//}
	//else if(ME_MC_RATIO==2) {
	//    reg_me_dsx_scale = 2 ;  // u2: downscale mode of x direction for me input data; 0: no downscale; 1:1/2 downscale; 2:1/4 downscale
	//    reg_me_dsy_scale = 2 ;  // u2: downscale mode of y direction for me input data; 0: no downscale; 1:1/2 downscale; 2:1/4 downscale
	//}
	//else if(ME_MC_RATIO == 3) {
	//    reg_me_dsx_scale = 3 ;  // u2: downscale mode of x direction for me input data; 0: no downscale; 1:1/2 downscale; 2:1/4 downscale
	//    reg_me_dsy_scale = 3 ;  // u2: downscale mode of y direction for me input data; 0: no downscale; 1:1/2 downscale; 2:1/4 downscale
	//}
	//else {
	//    reg_me_dsx_scale = 1 ;  // u2: downscale mode of x direction for me input data; 0: no downscale; 1:1/2 downscale; 2:1/4 downscale
	//    reg_me_dsy_scale = 1 ;  // u2: downscale mode of y direction for me input data; 0: no downscale; 1:1/2 downscale; 2:1/4 downscale
	//}
	//WRITE_FRC_BITS(FRC_REG_ME_HME_SCALE, reg_me_dsx_scale, 6, 2);
	//WRITE_FRC_BITS(FRC_REG_ME_HME_SCALE, reg_me_dsy_scale, 4, 2);

	if(is_me1mc4==0) {
		reg_me_dsx_scale =1;
		reg_me_dsy_scale =1;
		//reg_hme_dsx_scale =1;
		//reg_hme_dsy_scale =1;
		me_mc_ratio   = 1;
		logo_mc_ratio = 1;
	}
	else {
		reg_me_dsx_scale =2;
		reg_me_dsy_scale =2;
		//reg_hme_dsx_scale =2;
		//reg_hme_dsy_scale =2;
		me_mc_ratio   = 2;
		logo_mc_ratio = 2;
	}
	WRITE_FRC_BITS(FRC_REG_ME_HME_SCALE, reg_me_dsx_scale, 6, 2);
	WRITE_FRC_BITS(FRC_REG_ME_HME_SCALE, reg_me_dsy_scale, 4, 2);
	// WRITE_FRC_BITS(FRC_REG_ME_HME_SCALE, reg_hme_dsx_scale, 2, 2);
	// WRITE_FRC_BITS(FRC_REG_ME_HME_SCALE, reg_hme_dsy_scale, 0, 2);

	WRITE_FRC_BITS(FRC_REG_BLK_SCALE  , logo_mc_ratio               , 4 , 2);//reg_logo_mc_ratio
	WRITE_FRC_BITS(FRC_REG_BLK_SCALE  , OSD_MC_RATIO                , 2 , 2);//reg_osd_mc_ratio
	WRITE_FRC_BITS(FRC_REG_BLK_SCALE  , logo_mc_ratio - OSD_MC_RATIO, 18, 2);//reg_osd_logo_ratio
	WRITE_FRC_BITS(FRC_REG_BLK_SCALE  , 1                           , 14, 2);//reg_osd_logo_ratio_th
	WRITE_FRC_BITS(FRC_REG_BLK_SIZE_XY, logo_mc_ratio - me_mc_ratio , 25, 1);//reg_me_logo_dsx_ratio
	WRITE_FRC_BITS(FRC_REG_BLK_SIZE_XY, logo_mc_ratio - me_mc_ratio , 24, 1);//reg_me_logo_dsy_ratio

	reg_me_mvx_div_mode = (READ_FRC_REG(FRC_ME_EN)>>4) & 0x3;
	reg_me_mvy_div_mode = (READ_FRC_REG(FRC_ME_EN)>>0) & 0x3;

	WRITE_FRC_BITS(FRC_ME_GMV_CTRL, (ME_MVX_BIT -2 + reg_me_mvx_div_mode) - (ME_FINER_HIST_BIT+ME_ROUGH_X_HIST_BIT),  4, 3);         //reg_gmv_mvx_sft
	WRITE_FRC_BITS(FRC_ME_GMV_CTRL, (ME_MVY_BIT -2 + reg_me_mvy_div_mode) - (ME_FINER_HIST_BIT+ME_ROUGH_Y_HIST_BIT),  0, 3);         //reg_gmv_mvy_sft
	WRITE_FRC_BITS(FRC_ME_REGION_RP_GMV_2, reg_me_mvx_div_mode + 2, 17, 3);  //reg_region_rp_gmv_mvx_sft
	WRITE_FRC_BITS(FRC_ME_REGION_RP_GMV_2, reg_me_mvy_div_mode + 2, 14, 3);  //reg_region_rp_gmv_mvy_sft

	/* //  below changed to fixed CID139496
	if(me_mc_ratio==0) {
		reg_mc_blksize_xscale  = 2 ;              //  u3: (0~4); mc block horizontal size in full pixel scale = (1<<xscal), set to (reg_me_dsx_scale.value + 2) as default
		reg_mc_blksize_yscale  = 2 ;              //  u3: (0~4); mc block vertical size in full pixel scale = (1<<yscal), set to (reg_me_dsy_scale.value + 2) as default

		reg_mc_mvx_scale =  0 ;                   //  u4, upscale of mvx from vector of ME/VP to get the vector under MC full scale; 0: no upscale; 1:2x upscale; 2:4xupscale, should be set to equal of reg_me_dsx_scale
		reg_mc_mvy_scale =  0 ;                   //  u4, upscale of mvy from vector of ME/VP to get the vector under MC full scale; 0: no upscale; 1:2x upscale; 2:4xupscale, should be set to equal of reg_me_dsy_scale
		reg_mc_fetch_size = 3 ;
		reg_mc_blk_x      = 4 ;
	}
	else if(me_mc_ratio==2) {
		reg_mc_blksize_xscale  = 4 ;              //  u3: (0~4); mc block horizontal size in full pixel scale = (1<<xscal), set to (reg_me_dsx_scale.value + 2) as default
		reg_mc_blksize_yscale  = 4 ;              //  u3: (0~4); mc block vertical size in full pixel scale = (1<<yscal), set to (reg_me_dsy_scale.value + 2) as default

		reg_mc_mvx_scale =  2 ;                   //  u4, upscale of mvx from vector of ME/VP to get the vector under MC full scale; 0: no upscale; 1:2x upscale; 2:4xupscale, should be set to equal of reg_me_dsx_scale
		reg_mc_mvy_scale =  2 ;                   //  u4, upscale of mvy from vector of ME/VP to get the vector under MC full scale; 0: no upscale; 1:2x upscale; 2:4xupscale, should be set to equal of reg_me_dsy_scale
		reg_mc_fetch_size = 9 ;
		reg_mc_blk_x      = 16 ;
	}
	else if(me_mc_ratio == 3) {
		reg_mc_blksize_xscale  = 5 ;              //  u3: (0~4); mc block horizontal size in full pixel scale = (1<<xscal), set to (reg_me_dsx_scale.value + 2) as default
		reg_mc_blksize_yscale  = 5 ;              //  u3: (0~4); mc block vertical size in full pixel scale = (1<<yscal), set to (reg_me_dsy_scale.value + 2) as default

		reg_mc_mvx_scale =  3 ;                   //  u4, upscale of mvx from vector of ME/VP to get the vector under MC full scale; 0: no upscale; 1:2x upscale; 2:4xupscale, should be set to equal of reg_me_dsx_scale
		reg_mc_mvy_scale =  3 ;                   //  u4, upscale of mvy from vector of ME/VP to get the vector under MC full scale; 0: no upscale; 1:2x upscale; 2:4xupscale, should be set to equal of reg_me_dsy_scale
		reg_mc_fetch_size = 17 ;
		reg_mc_blk_x      = 32 ;
	}
	else {
		reg_mc_blksize_xscale  = 3 ;              //  u3: (0~4); mc block horizontal size in full pixel scale = (1<<xscal), set to (reg_me_dsx_scale.value + 2) as default
		reg_mc_blksize_yscale  = 3 ;              //  u3: (0~4); mc block vertical size in full pixel scale = (1<<yscal), set to (reg_me_dsy_scale.value + 2) as default

		reg_mc_mvx_scale =  1 ;                   //  u4, upscale of mvx from vector of ME/VP to get the vector under MC full scale; 0: no upscale; 1:2x upscale; 2:4xupscale, should be set to equal of reg_me_dsx_scale
		reg_mc_mvy_scale =  1 ;                   //  u4, upscale of mvy from vector of ME/VP to get the vector under MC full scale; 0: no upscale; 1:2x upscale; 2:4xupscale, should be set to equal of reg_me_dsy_scale
		reg_mc_fetch_size = 5 ;
		reg_mc_blk_x      = 8 ;
	}
	*/
	// because me_mc_ratio only equal 1 or 2
	if (me_mc_ratio == 2) {
		reg_mc_blksize_xscale = 4;
		reg_mc_blksize_yscale = 4;
		reg_mc_mvx_scale =  2;
		reg_mc_mvy_scale =  2;
		reg_mc_fetch_size = 9;
		reg_mc_blk_x      = 16;
	} else {
		reg_mc_blksize_xscale = 3;
		reg_mc_blksize_yscale = 3;
		reg_mc_mvx_scale =  1;
		reg_mc_mvy_scale =  1;
		reg_mc_fetch_size = 5;
		reg_mc_blk_x      = 8;
	}
	WRITE_FRC_BITS(FRC_REG_BLK_SCALE, reg_mc_blksize_xscale, 9, 3);  //reg_mc_blksize_xscale
	WRITE_FRC_BITS(FRC_REG_BLK_SCALE, reg_mc_blksize_yscale, 6, 3);  //reg_mc_blksize_yscale
	WRITE_FRC_BITS(FRC_MC_SETTING1  , reg_mc_mvx_scale,      8, 4);  //reg_mc_mvx_scale
	WRITE_FRC_BITS(FRC_MC_SETTING1  , reg_mc_mvy_scale,      0, 4);  //reg_mc_mvy_scale
	WRITE_FRC_BITS(FRC_MC_SETTING2  , reg_mc_fetch_size,     8, 6);  //reg_mc_fetch_size
	WRITE_FRC_BITS(FRC_MC_SETTING2  , reg_mc_blk_x,          0, 6);  //reg_mc_blk_x

	//  MC
	WRITE_FRC_BITS(FRC_NOW_SRCH_REG , MAX_MC_Y_VRANG,       8, 8);  //reg_mc_vsrch_rng_luma
	WRITE_FRC_BITS(FRC_NOW_SRCH_REG , MAX_MC_C_VRANG,       0, 8);  //reg_mc_vsrch_rng_chrm

	reg_bb_det_top  = 0           ;//todo add input ??
	reg_bb_det_bot  = frm_vsize-1 ;//todo add input ??
	reg_bb_det_lft  = 0           ;//todo add input ??
	reg_bb_det_rit  = frm_hsize-1 ;//todo add input ??

	WRITE_FRC_BITS(FRC_BBD_DETECT_REGION_TOP2BOT, reg_bb_det_top, 16, 16);  //if reg_bb_det_top < 0
	WRITE_FRC_BITS(FRC_BBD_DETECT_REGION_TOP2BOT, reg_bb_det_bot, 0 , 16);  //if reg_bb_det_bot>frm_vsize-1
	WRITE_FRC_BITS(FRC_BBD_DETECT_REGION_LFT2RIT, reg_bb_det_lft, 16, 16);  //if reg_bb_det_lft < 0
	WRITE_FRC_BITS(FRC_BBD_DETECT_REGION_LFT2RIT, reg_bb_det_rit, 0 , 16);  //if reg_bb_det_rit>frm_hsize-1

	/*
	 *below changed to fixed CID139497, CID139498
	 * // reg_bb_det_motion_top  =  (0 == reg_me_dsy_scale) ? reg_bb_det_top :
	 *				reg_bb_det_top     >> reg_me_dsy_scale;
	 * // reg_bb_det_motion_bot  = ((0 == reg_me_dsy_scale) ? reg_bb_det_bot :
	 *				(reg_bb_det_bot+1) >> reg_me_dsy_scale) - 1;
	 * // reg_bb_det_motion_lft  =  (0 == reg_me_dsx_scale) ? reg_bb_det_lft :
	 *				(reg_bb_det_lft >> reg_me_dsx_scale);
	 * // reg_bb_det_motion_rit  = ((0 == reg_me_dsx_scale) ? reg_bb_det_rit :
	 *				(reg_bb_det_rit+1) >> reg_me_dsx_scale) - 1;
	 */
	reg_bb_det_motion_top  = reg_bb_det_top >> reg_me_dsy_scale;
	reg_bb_det_motion_bot  = ((reg_bb_det_bot + 1) >> reg_me_dsy_scale) - 1;
	reg_bb_det_motion_lft  = reg_bb_det_lft >> reg_me_dsx_scale;
	reg_bb_det_motion_rit  = ((reg_bb_det_rit + 1) >> reg_me_dsx_scale) - 1;

	WRITE_FRC_BITS(FRC_BBD_DETECT_MOTION_REGION_TOP2BOT, reg_bb_det_motion_top, 16, 16); //reg_bb_det_motion_top
	WRITE_FRC_BITS(FRC_BBD_DETECT_MOTION_REGION_TOP2BOT, reg_bb_det_motion_bot, 0,  16); //reg_bb_det_motion_bot
	WRITE_FRC_BITS(FRC_BBD_DETECT_MOTION_REGION_LFT2RIT, reg_bb_det_motion_lft, 16, 16); //reg_bb_det_motion_lft
	WRITE_FRC_BITS(FRC_BBD_DETECT_MOTION_REGION_LFT2RIT, reg_bb_det_motion_rit, 0,  16); //reg_bb_det_motion_rit

	//TODO....
	WRITE_FRC_BITS(FRC_BBD_DETECT_DETAIL_H_TOP2BOT, reg_bb_det_top, 16, 16); //reg_bb_det_detail_h_top
	WRITE_FRC_BITS(FRC_BBD_DETECT_DETAIL_H_TOP2BOT, reg_bb_det_bot,  0, 16); //reg_bb_det_detail_h_bot
	WRITE_FRC_BITS(FRC_BBD_DETECT_DETAIL_H_LFT2RIT, reg_bb_det_lft, 16, 16); //reg_bb_det_detail_h_lft
	WRITE_FRC_BITS(FRC_BBD_DETECT_DETAIL_H_LFT2RIT, reg_bb_det_rit,  0, 16); //reg_bb_det_detail_h_rit

	WRITE_FRC_BITS(FRC_BBD_DETECT_DETAIL_V_TOP2BOT, reg_bb_det_top, 16, 16); //reg_bb_det_detail_v_top
	WRITE_FRC_BITS(FRC_BBD_DETECT_DETAIL_V_TOP2BOT, reg_bb_det_bot,  0, 16); //reg_bb_det_detail_v_bot
	WRITE_FRC_BITS(FRC_BBD_DETECT_DETAIL_V_LFT2RIT, reg_bb_det_motion_lft,  16, 16); //reg_bb_det_detail_v_lft
	WRITE_FRC_BITS(FRC_BBD_DETECT_DETAIL_V_LFT2RIT, reg_bb_det_motion_rit,   0, 16); //reg_bb_det_detail_v_rit

	WRITE_FRC_BITS(FRC_REG_DEBUG_PATH_TOP_BOT_MOTION_POSI2, reg_bb_det_top, 16, 16); //reg_debug_top_motion_posi2
	WRITE_FRC_BITS(FRC_REG_DEBUG_PATH_TOP_BOT_MOTION_POSI2, reg_bb_det_bot,  0, 16); //reg_debug_bot_motion_posi2
	WRITE_FRC_BITS(FRC_REG_DEBUG_PATH_LFT_RIT_MOTION_POSI2, reg_bb_det_lft, 16, 16); //reg_debug_lft_motion_posi2
	WRITE_FRC_BITS(FRC_REG_DEBUG_PATH_LFT_RIT_MOTION_POSI2, reg_bb_det_rit,  0, 16); //reg_debug_rit_motion_posi2

	WRITE_FRC_BITS(FRC_REG_DEBUG_PATH_TOP_BOT_MOTION_POSI1, reg_bb_det_top, 16, 16); //reg_debug_top_motion_posi1
	WRITE_FRC_BITS(FRC_REG_DEBUG_PATH_TOP_BOT_MOTION_POSI1, reg_bb_det_bot,  0, 16); //reg_debug_bot_motion_posi1
	WRITE_FRC_BITS(FRC_REG_DEBUG_PATH_LFT_RIT_MOTION_POSI1, reg_bb_det_lft, 16, 16); //reg_debug_lft_motion_posi1
	WRITE_FRC_BITS(FRC_REG_DEBUG_PATH_LFT_RIT_MOTION_POSI1, reg_bb_det_rit,  0, 16); //reg_debug_rit_motion_posi1

	WRITE_FRC_BITS(FRC_REG_DEBUG_PATH_TOP_BOT_EDGE_POSI2, reg_bb_det_top, 16, 16); //reg_debug_top_edge_posi2
	WRITE_FRC_BITS(FRC_REG_DEBUG_PATH_TOP_BOT_EDGE_POSI2, reg_bb_det_bot,  0, 16); //reg_debug_bot_edge_posi2
	WRITE_FRC_BITS(FRC_REG_DEBUG_PATH_LFT_RIT_EDGE_POSI2, reg_bb_det_lft, 16, 16); //reg_debug_lft_edge_posi2
	WRITE_FRC_BITS(FRC_REG_DEBUG_PATH_LFT_RIT_EDGE_POSI2, reg_bb_det_rit,  0, 16); //reg_debug_rit_edge_posi2

	WRITE_FRC_BITS(FRC_REG_DEBUG_PATH_TOP_BOT_EDGE_POSI1, reg_bb_det_top, 16, 16); //reg_debug_top_edge_posi1
	WRITE_FRC_BITS(FRC_REG_DEBUG_PATH_TOP_BOT_EDGE_POSI1, reg_bb_det_bot,  0, 16); //reg_debug_bot_edge_posi1
	WRITE_FRC_BITS(FRC_REG_DEBUG_PATH_LFT_RIT_EDGE_POSI1, reg_bb_det_lft, 16, 16); //reg_debug_lft_edge_posi1
	WRITE_FRC_BITS(FRC_REG_DEBUG_PATH_LFT_RIT_EDGE_POSI1, reg_bb_det_rit,  0, 16); //reg_debug_rit_edge_posi1

	// black bar location  (will be changed oframe base by fw algorithm)
	WRITE_FRC_BITS(FRC_REG_BLACKBAR_XYXY_ST, reg_bb_det_lft, 16, 16); //reg_blackbar_xyxy_0
	WRITE_FRC_BITS(FRC_REG_BLACKBAR_XYXY_ST, reg_bb_det_top,  0, 16); //reg_blackbar_xyxy_1
	WRITE_FRC_BITS(FRC_REG_BLACKBAR_XYXY_ED, reg_bb_det_rit, 16, 16); //reg_blackbar_xyxy_2
	WRITE_FRC_BITS(FRC_REG_BLACKBAR_XYXY_ED, reg_bb_det_bot,  0, 16); //reg_blackbar_xyxy_3

	WRITE_FRC_BITS(FRC_REG_BLACKBAR_XYXY_ME_ST, reg_bb_det_lft, 16, 16); //reg_blackbar_xyxy_me_0
	WRITE_FRC_BITS(FRC_REG_BLACKBAR_XYXY_ME_ST, reg_bb_det_top,  0, 16); //reg_blackbar_xyxy_me_1
	WRITE_FRC_BITS(FRC_REG_BLACKBAR_XYXY_ME_ED, reg_bb_det_rit, 16, 16); //reg_blackbar_xyxy_me_2
	WRITE_FRC_BITS(FRC_REG_BLACKBAR_XYXY_ME_ED, reg_bb_det_bot,  0, 16); //reg_blackbar_xyxy_me_3

	// TODO: related to mc registers??
	reg_data = READ_FRC_REG(FRC_MC_DEMO_WINDOW);
	reg_demo_window1_en = (reg_data >> 3) & 0x1;
	reg_demo_window2_en = (reg_data >> 2) & 0x1;
	reg_demo_window3_en = (reg_data >> 1) & 0x1;
	reg_demo_window4_en = (reg_data )     & 0x1;
	if(reg_demo_window1_en) {
	// WRITE_FRC_BITS(FRC_REG_DEMOWINDOW1_XYXY_ST, frm_hsize >> 1, 16, 16);
	// WRITE_FRC_BITS(FRC_REG_DEMOWINDOW1_XYXY_ST,             0,  0, 16);
	// WRITE_FRC_BITS(FRC_REG_DEMOWINDOW1_XYXY_ED, frm_hsize - 1, 16, 16);
	// WRITE_FRC_BITS(FRC_REG_DEMOWINDOW1_XYXY_ED, frm_vsize - 1,  0, 16);
	}
	if(reg_demo_window2_en){
		WRITE_FRC_BITS(FRC_REG_DEMOWINDOW2_XYXY_ST, frm_hsize >> 1, 16, 16); //reg_demowindow2_xyxy_0
		WRITE_FRC_BITS(FRC_REG_DEMOWINDOW2_XYXY_ST,              0,  0, 16); //reg_demowindow2_xyxy_1
		WRITE_FRC_BITS(FRC_REG_DEMOWINDOW2_XYXY_ED,  frm_hsize - 1, 16, 16); //reg_demowindow2_xyxy_2
		WRITE_FRC_BITS(FRC_REG_DEMOWINDOW2_XYXY_ED,  frm_vsize - 1,  0, 16); //reg_demowindow2_xyxy_3
	}
	if(reg_demo_window3_en){
		WRITE_FRC_BITS(FRC_REG_DEMOWINDOW3_XYXY_ST, frm_hsize >> 1, 16, 16); //reg_demowindow3_xyxy_0
		WRITE_FRC_BITS(FRC_REG_DEMOWINDOW3_XYXY_ST,              0,  0, 16); //reg_demowindow3_xyxy_1
		WRITE_FRC_BITS(FRC_REG_DEMOWINDOW3_XYXY_ED,  frm_hsize - 1, 16, 16); //reg_demowindow3_xyxy_2
		WRITE_FRC_BITS(FRC_REG_DEMOWINDOW3_XYXY_ED,  frm_vsize - 1,  0, 16); //reg_demowindow3_xyxy_3
	}
	if(reg_demo_window4_en){
		WRITE_FRC_BITS(FRC_REG_DEMOWINDOW4_XYXY_ST, frm_hsize >> 1, 16, 16); //reg_demowindow4_xyxy_0
		WRITE_FRC_BITS(FRC_REG_DEMOWINDOW4_XYXY_ST,              0,  0, 16); //reg_demowindow4_xyxy_1
		WRITE_FRC_BITS(FRC_REG_DEMOWINDOW4_XYXY_ED,  frm_hsize - 1, 16, 16); //reg_demowindow4_xyxy_2
		WRITE_FRC_BITS(FRC_REG_DEMOWINDOW4_XYXY_ED,  frm_vsize - 1,  0, 16); //reg_demowindow4_xyxy_3
	}

	WRITE_FRC_BITS(FRC_REG_DS_WIN_SETTING_LFT_TOP, 0, 16, 16); //reg_bb_ds_xyxy_0
	WRITE_FRC_BITS(FRC_REG_DS_WIN_SETTING_LFT_TOP, 0,  0, 16); //reg_bb_ds_xyxy_1
	WRITE_FRC_BITS(FRC_BBD_DS_WIN_SETTING_RIT_BOT, (frm_hsize-1), 16, 16); //reg_bb_ds_xyxy_2
	WRITE_FRC_BITS(FRC_BBD_DS_WIN_SETTING_RIT_BOT, (frm_vsize-1),  0, 16); //reg_bb_ds_xyxy_3

	WRITE_FRC_BITS(FRC_MC_BB_HANDLE_ORG_ME_BB_XYXY_LEFT_TOP, 0, 16, 16); //reg_mc_org_me_bb_xyxy_0
	WRITE_FRC_BITS(FRC_MC_BB_HANDLE_ORG_ME_BB_XYXY_LEFT_TOP, 0,  0, 16); //reg_mc_org_me_bb_xyxy_1
	reg_data = READ_FRC_REG(FRC_REG_ME_HME_SCALE);
	mc_org_me_bb_2 = (reg_data >> 6) & 0x3;
	mc_org_me_bb_3 = (reg_data >> 4) & 0x3;
	WRITE_FRC_BITS(FRC_MC_BB_HANDLE_ORG_ME_BB_XYXY_RIGHT_BOT, (frm_hsize >> mc_org_me_bb_2) - 1 , 16, 16); //reg_mc_org_me_bb_xyxy_2
	WRITE_FRC_BITS(FRC_MC_BB_HANDLE_ORG_ME_BB_XYXY_RIGHT_BOT, (frm_vsize >> mc_org_me_bb_3) - 1 ,  0, 16); //reg_mc_org_me_bb_xyxy_3

	WRITE_FRC_BITS(FRC_MC_BB_HANDLE_ORG_ME_BLK_BB_XYXY_LFT_AND_TOP, 0, 16, 16); //reg_mc_org_me_blk_bb_xyxy_0
	WRITE_FRC_BITS(FRC_MC_BB_HANDLE_ORG_ME_BLK_BB_XYXY_LFT_AND_TOP, 0,  0, 16); //reg_mc_org_me_blk_bb_xyxy_1
	reg_data1 = READ_FRC_REG(FRC_REG_BLK_SIZE_XY);
	mc_org_me_blk_bb_2 = (reg_data1 >> 19) & 0x7;
	mc_org_me_blk_bb_3 = (reg_data1 >> 16) & 0x7;
	WRITE_FRC_BITS(FRC_MC_BB_HANDLE_ORG_ME_BLK_BB_XYXY_RIT_AND_BOT, ((frm_hsize >> mc_org_me_bb_2) >> mc_org_me_blk_bb_2) - 1 , 16, 16); //reg_mc_org_me_blk_bb_xyxy_2
	WRITE_FRC_BITS(FRC_MC_BB_HANDLE_ORG_ME_BLK_BB_XYXY_RIT_AND_BOT, ((frm_vsize >> mc_org_me_bb_3) >> mc_org_me_blk_bb_3) - 1 ,  0, 16); //reg_mc_org_me_blk_bb_xyxy_3

	////need write??
	//bbRalM.ro_bb_top_posi1 = bbRalM.reg_bb_det_top;	    //  u16:     bot line1 position,    dft ysize - 1
	//bbRalM.ro_bb_top_posi2 = bbRalM.reg_bb_det_top;        //  u16:     bot line2 position,    dft ysize - 1

	//bbRalM.ro_bb_bot_posi1 = bbRalM.reg_bb_det_bot;	    //  u16:     bot line1 position,    dft ysize - 1
	//bbRalM.ro_bb_bot_posi2 = bbRalM.reg_bb_det_bot;        //  u16:     bot line2 position,    dft ysize - 1

	//bbRalM.ro_bb_finer_lft_posi1 = bbRalM.reg_bb_det_lft;  //  u16:     right line1 position,  dft xsize-1
	//bbRalM.ro_bb_finer_lft_posi2 = bbRalM.reg_bb_det_lft;  //  u16:     right line2 position,  dft xsize-1

	//bbRalM.ro_bb_finer_rit_posi1 = bbRalM.reg_bb_det_rit;  //  u16:     right line1 position,  dft xsize-1
	//bbRalM.ro_bb_finer_rit_posi2 = bbRalM.reg_bb_det_rit;  //  u16:     right line2 position,  dft xsize-1

	WRITE_FRC_BITS(FRC_BBD_APL_HIST_WIN_LFT_TOP, reg_bb_det_lft, 16, 16); //reg_bb_apl_hist_xyxy_0
	WRITE_FRC_BITS(FRC_BBD_APL_HIST_WIN_LFT_TOP, reg_bb_det_top,  0, 16); //reg_bb_apl_hist_xyxy_1
	WRITE_FRC_BITS(FRC_BBD_APL_HIST_WIN_RIT_BOT, reg_bb_det_rit, 16, 16); //reg_bb_apl_hist_xyxy_2
	WRITE_FRC_BITS(FRC_BBD_APL_HIST_WIN_RIT_BOT, reg_bb_det_bot,  0, 16); //reg_bb_apl_hist_xyxy_3

	WRITE_FRC_BITS(FRC_BBD_OOB_APL_CAL_LFT_TOP_RANGE, reg_bb_det_lft, 16, 16); //reg_bb_oob_apl_xyxy_0
	WRITE_FRC_BITS(FRC_BBD_OOB_APL_CAL_LFT_TOP_RANGE, reg_bb_det_top,  0, 16); //reg_bb_oob_apl_xyxy_1
	WRITE_FRC_BITS(FRC_BBD_OOB_APL_CAL_RIT_BOT_RANGE, reg_bb_det_rit, 16, 16); //reg_bb_oob_apl_xyxy_2
	WRITE_FRC_BITS(FRC_BBD_OOB_APL_CAL_RIT_BOT_RANGE, reg_bb_det_bot,  0, 16); //reg_bb_oob_apl_xyxy_3

	WRITE_FRC_BITS(FRC_BBD_OOB_DETAIL_WIN_LFT_TOP, reg_bb_det_lft, 16, 16); //reg_bb_oob_h_detail_xyxy_0
	WRITE_FRC_BITS(FRC_BBD_OOB_DETAIL_WIN_LFT_TOP, reg_bb_det_top,  0, 16); //reg_bb_oob_h_detail_xyxy_1
	WRITE_FRC_BITS(FRC_BBD_OOB_DETAIL_WIN_RIT_BOT, reg_bb_det_rit, 16, 16); //reg_bb_oob_h_detail_xyxy_2
	WRITE_FRC_BITS(FRC_BBD_OOB_DETAIL_WIN_RIT_BOT, reg_bb_det_bot,  0, 16); //reg_bb_oob_h_detail_xyxy_3

	bb_mo_ds_2 = MIN( (frm_hsize >> mc_org_me_bb_2)-1, (reg_bb_det_rit >> mc_org_me_bb_2) );
	bb_mo_ds_3 = MIN( (frm_vsize >> mc_org_me_bb_3)-1, (reg_bb_det_bot >> mc_org_me_bb_3) );
	WRITE_FRC_BITS(FRC_BBD_MOTION_DETEC_REGION_LFT_TOP_DS, reg_bb_det_lft >> mc_org_me_bb_2, 16, 16); //reg_bb_motion_xyxy_ds_0
	WRITE_FRC_BITS(FRC_BBD_MOTION_DETEC_REGION_LFT_TOP_DS, reg_bb_det_top >> mc_org_me_bb_3,  0, 16); //reg_bb_motion_xyxy_ds_1
	WRITE_FRC_BITS(FRC_BBD_MOTION_DETEC_REGION_RIT_BOT_DS, bb_mo_ds_2, 16, 16); //reg_bb_motion_xyxy_ds_2
	WRITE_FRC_BITS(FRC_BBD_MOTION_DETEC_REGION_RIT_BOT_DS, bb_mo_ds_3,  0, 16); //reg_bb_motion_xyxy_ds_3

	bb_oob_v_2 = MIN( (frm_hsize >> mc_org_me_bb_2)-1, (reg_bb_det_rit >> mc_org_me_bb_2) );
	WRITE_FRC_BITS(FRC_BBD_OOB_V_DETAIL_WIN_LFT_TOP, reg_bb_det_lft >> mc_org_me_bb_2, 16, 16); //reg_bb_oob_v_detail_xyxy_0
	WRITE_FRC_BITS(FRC_BBD_OOB_V_DETAIL_WIN_LFT_TOP,                   reg_bb_det_top,  0, 16); //reg_bb_oob_v_detail_xyxy_1
	WRITE_FRC_BITS(FRC_BBD_OOB_V_DETAIL_WIN_RIT_BOT,                       bb_oob_v_2, 16, 16); //reg_bb_oob_v_detail_xyxy_2
	WRITE_FRC_BITS(FRC_BBD_OOB_V_DETAIL_WIN_RIT_BOT,                   reg_bb_det_bot,  0, 16); //reg_bb_oob_v_detail_xyxy_3

	WRITE_FRC_BITS(FRC_BBD_FLATNESS_DETEC_REGION_LFT_TOP, reg_bb_det_lft, 16, 16); //reg_bb_flat_xyxy_0
	WRITE_FRC_BITS(FRC_BBD_FLATNESS_DETEC_REGION_LFT_TOP, reg_bb_det_top,  0, 16); //reg_bb_flat_xyxy_1
	WRITE_FRC_BITS(FRC_BBD_FLATNESS_DETEC_REGION_RIT_BOT, reg_bb_det_rit, 16, 16); //reg_bb_flat_xyxy_2
	WRITE_FRC_BITS(FRC_BBD_FLATNESS_DETEC_REGION_RIT_BOT, reg_bb_det_bot,  0, 16); //reg_bb_flat_xyxy_3

// endif
}


void config_loss_out(u32 fmt422)
{
#ifdef LOSS_TEST
   // #include "rand_seed_reg_acc.sv"  //?????????????
	WRITE_FRC_BITS(CLOSS_MISC, 1, 0, 1); //reg_enable
	WRITE_FRC_BITS(CLOSS_MISC, fmt422, 1, 1); //reg_inp_422;   1: 422, 0:444

	WRITE_FRC_BITS(CLOSS_MISC, (6<<8)|(7<<16), 12, 20); //reg_misc
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
	WRITE_FRC_BITS(FRC_REG_INP_MODULE_EN, 1, 7, 1); //reg_inp_bbd_en
}

void set_mc_lbuf_ext(void)
{
	WRITE_FRC_BITS(FRC_MC_HW_CTRL1, 1, 16, 1); //reg_mc_fhd_lbuf_ext
}

void cfg_me_loss(u32 me_loss_en)
{
	WRITE_FRC_BITS(FRC_REG_TOP_CTRL11, me_loss_en, 1, 1); //reg_frc_me_loss_en
}

void cfg_mc_loss(u32 mc_loss_en)
{
	WRITE_FRC_BITS(FRC_REG_TOP_CTRL11, mc_loss_en, 0, 1); //reg_frc_mc_loss_en
}

void frc_force_secure(u32 onoff)
{
	if (onoff)
		WRITE_FRC_BITS(FRC_MC_LOSS_SLICE_SEC, 1, 0, 1);//reg_mc_loss_slice_sec
	else
		WRITE_FRC_BITS(FRC_MC_LOSS_SLICE_SEC, 0, 0, 1);//reg_mc_loss_slice_sec
}

void recfg_memc_mif_base_addr(u32 base_ofst)
{
	u32 reg_addr;
	u32 reg_data;

	for(reg_addr = FRC_REG_MC_YINFO_BADDR; reg_addr < FRC_REG_LOGO_SCC_BUF_ADDR; reg_addr += 4) {
		reg_data = READ_FRC_REG(reg_addr);
		reg_data += base_ofst;
		WRITE_FRC_REG(reg_addr, reg_data);
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
	{"FRC_REG_BLK_SIZE_XY->reg_hme_mvx_div_mode", FRC_REG_BLK_SIZE_XY, 2, 2},
	{"FRC_REG_BLK_SIZE_XY->reg_hme_mvy_div_mode", FRC_REG_BLK_SIZE_XY, 0, 2},

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

/*
 * driver probe call
 */
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
		WRITE_FRC_REG(regs_table[i].addr, regs_table[i].value);
	pr_frc(0, "regs_table[%d] init done\n", REG_NUM);

	frc_inp_init(frc_top->frc_fb_num, frc_top->film_hwfw_sel);
	config_phs_lut(frc_top->frc_ratio_mode, frc_top->film_mode);
	config_phs_regs(frc_top->frc_ratio_mode, frc_top->film_mode);
	config_me_top_hw_reg();
	cfg_me_loss(frc_top->me_loss_en);
	cfg_mc_loss(frc_top->mc_loss_en);
	/*protect mode, enable: memc delay 2 frame*/
	/*disable: memc delay n frame, n depend on candence, for debug*/
	pr_frc(0, "%s\n", __func__);
	return;
}

u8 frc_frame_forcebuf_enable(u8 enable)
{
	u8  ro_frc_input_fid = 0;//u4

	if (enable == 1) {	//frc off->on
		ro_frc_input_fid = READ_FRC_REG(FRC_REG_PAT_POINTER) >> 4 & 0xF;
		//force phase 0
		UPDATE_FRC_REG_BITS(FRC_REG_TOP_CTRL7, 0x9000000, 0x9000000);
		UPDATE_FRC_REG_BITS(FRC_REG_TOP_CTRL8, 0, 0xFFFF);
	} else {
		UPDATE_FRC_REG_BITS(FRC_REG_TOP_CTRL7, 0x0000000, 0x9000000);
	}
	return ro_frc_input_fid;
}

void frc_frame_forcebuf_count(u8 forceidx)
{
	UPDATE_FRC_REG_BITS(FRC_REG_TOP_CTRL7,
	(forceidx | forceidx << 4 | forceidx << 8), 0xFFF);//0-11bit
}

u16 frc_check_vf_rate(u16 duration, struct frc_dev_s *frc_devp)
{
	int i = 0;
	u16 framerate = 0;

	/*duration: 1600(60fps) 1920(50fps) 3200(30fps) 3203(29.97)*/
	/*3840(25fps) 4000(24fps) 4004(23.976fps)*/
	while (i++ < 8) {
		if (vf_rate_table[i].duration == duration) {
			framerate = vf_rate_table[i].framerate;
			break;
		}
	}
	if (framerate != frc_devp->in_sts.frc_vf_rate) {
		pr_frc(3, "input vf rate changed [%d->%d].\n",
			frc_devp->in_sts.frc_vf_rate, framerate);
		frc_devp->in_sts.frc_vf_rate = framerate;
	}
	return frc_devp->in_sts.frc_vf_rate;
}

