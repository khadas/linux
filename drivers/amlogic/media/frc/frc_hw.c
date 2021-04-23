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

#include "frc_drv.h"
#include "frc_reg.h"
#include "frc_hw.h"

void __iomem *frc_clk_base;
void __iomem *vpu_base;

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
		frc_devp->clk_frc_Frq = clk_get_rate(frc_devp->clk_frc);
		pr_frc(0, "clk_frc frq : %d Mhz\n", frc_devp->clk_frc_Frq / 1000000);

		clk_set_rate(frc_devp->clk_me, 400000000);
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

void frc_init_config(struct frc_dev_s *devp)
{
	/*1: before postblend, 0: after postblend*/
	if (devp->frc_hw_pos == FRC_POS_AFTER_POSTBLEND)
		vpu_reg_write_bits(VPU_FRC_TOP_CTRL, 0, 4, 1);
	else
		vpu_reg_write_bits(VPU_FRC_TOP_CTRL, 1, 4, 1);
}

void set_frc_enable(u32 en)
{
	WRITE_FRC_BITS(FRC_TOP_CTRL, 0, 8, 1);
	WRITE_FRC_BITS(FRC_TOP_CTRL, en, 0, 1);
}

void set_frc_bypass(u32 en)
{
	vpu_reg_write_bits(VPU_FRC_TOP_CTRL, en, 0, 1);
}

void frc_pattern_on(u32 en)
{
	WRITE_FRC_BITS(FRC_REG_INP_MODULE_EN, en, 6, 1);
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

void me_undown_read(struct frc_dev_s *frc_devp)
{
	u32 val, me_ud_flag;

	if (frc_devp->ud_dbg.meud_dbg_en) {
		val = READ_FRC_REG(FRC_INP_UE_DBG);
		me_ud_flag = val & 0x3e;
		pr_frc(0, "invs_cnt = %d, me_ud_flag = %d\n",
			frc_devp->in_sts.vs_cnt, me_ud_flag);
		WRITE_FRC_BITS(FRC_INP_UE_CLR, 0x3e, 1, 5);
		WRITE_FRC_BITS(FRC_INP_UE_CLR, 0x0, 1, 5);
	}
}

void mc_undown_read(struct frc_dev_s *frc_devp)
{
	u32 val, mc_ud_flag;

	if (frc_devp->ud_dbg.mcud_dbg_en) {
		val = READ_FRC_REG(FRC_MC_DBG_MC_WRAP);
		mc_ud_flag = (val >> 24) & 0x1;
		pr_frc(0, "outvs_cnt = %d, mc_ud_flag = %d\n",
			frc_devp->out_sts.vs_cnt, mc_ud_flag);
		WRITE_FRC_BITS(FRC_MC_HW_CTRL0, 1, 21, 1);
		WRITE_FRC_BITS(FRC_MC_HW_CTRL0, 0, 21, 1);
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
	unsigned int hsize, vsize;

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
		/*no loss*/
		frc_top->loss_en = 0;
		frc_top->frc_prot_mode = frc_devp->prot_mode;
		frc_top->frc_fb_num = FRC_TOTAL_BUF_NUM;

		set_vd1_out_size(frc_devp);
	} else {
		if (frc_devp->frc_hw_pos == FRC_POS_AFTER_POSTBLEND) {
			frc_top->hsize = frc_devp->out_sts.vout_width;
			frc_top->vsize = frc_devp->out_sts.vout_height;
			frc_top->out_hsize = frc_devp->out_sts.vout_width;
			frc_top->out_vsize = frc_devp->out_sts.vout_height;
		} else {
			frc_top->hsize = frc_devp->in_sts.in_hsize;
			frc_top->vsize = frc_devp->in_sts.in_vsize;
			frc_top->out_hsize = frc_devp->in_sts.in_hsize;
			frc_top->out_vsize = frc_devp->in_sts.in_vsize;
		}
		frc_top->frc_ratio_mode = frc_devp->in_out_ratio;
		frc_top->film_mode = frc_devp->film_mode;
		/*sw film detect*/
		frc_top->film_hwfw_sel = frc_devp->film_mode_det;
		/*no loss*/
		frc_top->loss_en = frc_devp->loss_en;
		frc_top->frc_prot_mode = frc_devp->prot_mode;
		frc_top->frc_fb_num = FRC_TOTAL_BUF_NUM;
	}
	pr_frc(1, "top in: hsize:%d, vsize:%d\n", frc_top->hsize, frc_top->vsize);
	pr_frc(1, "top out: hsize:%d, vsize:%d\n", frc_top->out_hsize, frc_top->out_vsize);
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
	u32 mevp_frm_dly        ;//Read RO ro_mevp_dly_num
	u32 mc_frm_dly          ;//Read RO ro_mc2out_dly_num
	u32 memc_frm_dly        ;//total delay
	u32 reg_mc_dly_vofst1   ;
	u32 log = 1;

	struct frc_fw_data_s *fw_data;
	struct frc_top_type_s *frc_top;

	fw_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	frc_top = &fw_data->frc_top_type;

	me_hold_line = fw_data->holdline_parm.me_hold_line;
	mc_hold_line = fw_data->holdline_parm.mc_hold_line;
	inp_hold_line = fw_data->holdline_parm.inp_hold_line;
	reg_post_dly_vofst = fw_data->holdline_parm.reg_post_dly_vofst;
	reg_mc_dly_vofst0 = fw_data->holdline_parm.reg_mc_dly_vofst0;

	frc_input_init(frc_devp, frc_top);
	pr_frc(log, "%s\n", __func__);
	////Config frc input size
	WRITE_FRC_BITS(FRC_FRAME_SIZE ,(frc_top->vsize <<16) | frc_top->hsize  , 0  ,32 );

	/*!!!!!!!!! tread de, vpu register*/
	frc_top->vfb = vpu_reg_read(ENCL_VIDEO_VAVON_BLINE);
	//pr_frc(log, "ENCL_VIDEO_VAVON_BLINE:%d\n", frc_top->vfb);
	reg_mc_out_line = (frc_top->vfb / 4) * 3;// 3/4 point of front vblank
	reg_me_dly_vofst = reg_mc_out_line;

	if (frc_top->out_hsize == 1920 && frc_top->out_vsize == 1080) {
		mevp_frm_dly = 130;
		mc_frm_dly   = 11 ;//inp performace issue, need frc_clk >  enc0_clk
		frc_top->is_me1mc4 = 0;/*me:mc 1:2*/
	} else if (frc_top->out_hsize == 3840 && frc_top->out_vsize == 2160) {
		mevp_frm_dly = 260;
		mc_frm_dly = 28;
		frc_top->is_me1mc4 = 1;/*me:mc 1:4*/

		/* MEMC 4K ENCL setting, vlock will change the ENCL_VIDEO_MAX_LNCNT,
		 * so need dynamic change this register
		 */
		u32 frc_v_porch = vpu_reg_read(ENCL_FRC_CTRL);/*0x1cdd*/
		u32 max_lncnt = vpu_reg_read(ENCL_VIDEO_MAX_LNCNT);/*0x1cbb*/
		u32 max_pxcnt = vpu_reg_read(ENCL_VIDEO_MAX_PXCNT);/*0x1cb0*/

		pr_frc(1, "frc_v_porch=%d max_lncnt=%d max_pxcnt=%d\n",
			frc_v_porch, max_lncnt, max_pxcnt);
		vpu_reg_write(ENCL_SYNC_TO_LINE_EN, (1 << 13) | (max_lncnt - frc_v_porch));
		vpu_reg_write(ENCL_SYNC_PIXEL_EN, (1 << 15) | (max_pxcnt - 1));
		vpu_reg_write(ENCL_SYNC_LINE_LENGTH, max_lncnt - frc_v_porch - 1);
		pr_frc(1, "ENCL_SYNC_TO_LINE_EN=0x%x\n", vpu_reg_read(ENCL_SYNC_TO_LINE_EN));
		pr_frc(1, "ENCL_SYNC_PIXEL_EN=0x%x\n", vpu_reg_read(ENCL_SYNC_PIXEL_EN));
		pr_frc(1, "ENCL_SYNC_LINE_LENGTH=0x%x\n", vpu_reg_read(ENCL_SYNC_LINE_LENGTH));
	} else {
		mevp_frm_dly = 140;
		mc_frm_dly   = 10 ;//inp performace issue, need frc_clk >  enc0_clk
		frc_top->is_me1mc4 = 0;
	}

	//memc_frm_dly
	memc_frm_dly      = reg_me_dly_vofst + me_hold_line + mevp_frm_dly + mc_frm_dly  + mc_hold_line ;//ref_frm_en before max_cnt_line
	reg_mc_dly_vofst1 = memc_frm_dly - mc_frm_dly   - mc_hold_line ;

	vpu_reg_write_bits(ENCL_FRC_CTRL, memc_frm_dly - reg_mc_out_line, 0, 16);
	//WRITE_FRC_BITS(ENCL_FRC_CTRL, memc_frm_dly - reg_mc_out_line, 0, 16);
	WRITE_FRC_BITS(FRC_REG_TOP_CTRL14, reg_post_dly_vofst, 0, 16);
	WRITE_FRC_BITS(FRC_REG_TOP_CTRL14, reg_me_dly_vofst, 16, 16);
	WRITE_FRC_BITS(FRC_REG_TOP_CTRL15, reg_mc_dly_vofst0, 0, 16);
	WRITE_FRC_BITS(FRC_REG_TOP_CTRL15, reg_mc_dly_vofst1, 16,  16);

	pr_frc(log, "reg_mc_out_line   = %d\n", reg_mc_out_line);
	pr_frc(log, "me_hold_line      = %d\n", me_hold_line);
	pr_frc(log, "mc_hold_line      = %d\n", mc_hold_line);
	pr_frc(log, "mevp_frm_dly      = %d\n", mevp_frm_dly);
	pr_frc(log, "mc_frm_dly        = %d\n", mc_frm_dly);
	pr_frc(log, "memc_frm_dly      = %d\n", memc_frm_dly);
	pr_frc(log, "reg_mc_dly_vofst1 = %d\n", reg_mc_dly_vofst1);
	pr_frc(log, "frc_ratio_mode = %d\n", frc_top->frc_ratio_mode);
	pr_frc(log, "frc_fb_num = %d\n", frc_top->frc_fb_num);

	WRITE_FRC_BITS(FRC_OUT_HOLD_CTRL  ,me_hold_line, 0   ,8  );
	WRITE_FRC_BITS(FRC_OUT_HOLD_CTRL  ,mc_hold_line, 8   ,8  );

	WRITE_FRC_BITS(FRC_INP_HOLD_CTRL, inp_hold_line, 0, 13);//default 6

	//iplogo en

	////Config inp
	frc_inp_init(frc_top->frc_fb_num, frc_top->film_hwfw_sel);

	////Config phs_lut    frc_ratio_mode: frequence ratio 30in/60 out 1 : 2
	config_phs_lut(frc_top->frc_ratio_mode, frc_top->film_mode);
	config_phs_regs(frc_top->frc_ratio_mode, frc_top->film_mode);

	//initial reg get from jitao must config before this function
	//config_me_top_hw_reg/sys_fw_param_frc_init/init_bb_xyxy
	// frc_internal_initial();
	////config_me_top_hw_reg
	config_me_top_hw_reg();//have initial config before this function
	////Config bb
	sys_fw_param_frc_init(frc_top->hsize,frc_top->vsize, frc_top->is_me1mc4);
	init_bb_xyxy     (frc_top->hsize,frc_top->vsize);

	//loss ena or disable, default is disable
	cfg_me_loss(frc_top->loss_en);
	cfg_mc_loss(frc_top->loss_en);


	/*protect mode, enable: memc delay 2 frame*/
	/*disable: memc delay n frame, n depend on candence, for debug*/
	if(frc_top->frc_prot_mode) {
		WRITE_FRC_BITS(FRC_REG_TOP_CTRL9 ,1,24,4);//dly_num =1
		WRITE_FRC_BITS(FRC_REG_TOP_CTRL17,1,8 ,1);//buf prot open
	}
}

/*buffer number can dynamic kmalloc,  film_hwfw_sel ????*/
void frc_inp_init(u32 frc_fb_num, u32 film_hwfw_sel)
{
	WRITE_FRC_BITS(FRC_FRAME_BUFFER_NUM             ,frc_fb_num    ,0 ,5 );//set   frc_fb_num
	WRITE_FRC_BITS(FRC_FRAME_BUFFER_NUM             ,frc_fb_num    ,8 ,5 );//set   frc_fb_num
	WRITE_FRC_BITS(FRC_MC_PRB_CTRL1                 ,1             ,8 ,1 );//close reg_mc_probe_en
	WRITE_FRC_BITS(FRC_REG_INP_MODULE_EN            ,0             ,5 ,1 );//close reg_inp_logo_en
	//WRITE_FRC_BITS(FRC_REG_INP_MODULE_EN            ,0             ,6 ,1 );//close reg_menr_en
	WRITE_FRC_BITS(FRC_REG_INP_MODULE_EN            ,1             ,8, 1 );//open  reg_inp_logo_en
	WRITE_FRC_BITS(FRC_REG_TOP_CTRL25               ,0x4080200     ,0 ,31);//aligned padding value
	WRITE_FRC_BITS(FRC_REG_FILM_PHS_1               ,film_hwfw_sel ,16,1 );

	/*for fw signal latch, should always enable*/
	WRITE_FRC_BITS(FRC_REG_MODE_OPT, 1, 1, 1);
	WRITE_FRC_BITS(FRC_ME_E2E_CHK_EN - HME_ME_OFFSET,0         ,24,1 );//todo init hme
}

void config_phs_lut(enum frc_ratio_mode_type frc_ratio_mode,
	enum en_film_mode film_mode)
{
	#define lut_ary_size	18
	u32 input_n ;
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
	uint64_t            inp_frm_vld_lut     = 1;
	u32            reg_ip_incr[16]     = {0,1,3,1,1,1,1,1,2,3,2,1,1,1,1,2};

	//config ip_film_end
	if(frc_ratio_mode == FRC_RATIO_1_1) {
		inp_frm_vld_lut = 1;

		if(film_mode == EN_VIDEO) {
			reg_ip_film_end     = 1;
			reg_frc_pd_pat_num  = 1;
			reg_out_frm_dly_num = 3;
		}
		else if(film_mode == EN_FILM22) {
			reg_ip_film_end    = 0x2;
			reg_frc_pd_pat_num = 2;
			reg_out_frm_dly_num= 4;
		}
		else if(film_mode == EN_FILM32) {
			reg_ip_film_end    = 0x12;
			reg_frc_pd_pat_num = 5;
			reg_out_frm_dly_num= 10;
		}
		else if(film_mode == EN_FILM3223) {
			reg_ip_film_end    = 0x24a;
			reg_frc_pd_pat_num = 10;
			reg_out_frm_dly_num= 15;
		}
		else if(film_mode == EN_FILM2224) {
			reg_ip_film_end    = 0x22a;
			reg_frc_pd_pat_num = 10;
			reg_out_frm_dly_num= 15;
		}
		else if(film_mode == EN_FILM32322) {
			reg_ip_film_end    = 0x94a;
			reg_frc_pd_pat_num = 12;
			reg_out_frm_dly_num= 15;
		}
		else if(film_mode == EN_FILM44) {
			reg_ip_film_end    = 0x8;
			reg_frc_pd_pat_num = 4;
			reg_out_frm_dly_num= 8;
		}
		else if(film_mode == EN_FILM21111) {
			reg_ip_film_end    = 0x2f;
			reg_frc_pd_pat_num = 6;
			reg_out_frm_dly_num= 10;
		}
		else if(film_mode == EN_FILM23322) {
			reg_ip_film_end    = 0x92a;
			reg_frc_pd_pat_num = 12;
			reg_out_frm_dly_num= 15;
		}
		else if(film_mode == EN_FILM2111) {
			reg_ip_film_end    = 0x17;
			reg_frc_pd_pat_num = 5;
			reg_out_frm_dly_num= 10;
		}
		else if(film_mode == EN_FILM22224) {
			reg_ip_film_end    = 0x8aa;
			reg_frc_pd_pat_num = 12;
			reg_out_frm_dly_num= 15;
		}
		else if(film_mode == EN_FILM33) {
			reg_ip_film_end    = 0x4;
			reg_frc_pd_pat_num = 3;
			reg_out_frm_dly_num= 6;
		}
		else if(film_mode == EN_FILM334) {
			reg_ip_film_end    = 0x224;
			reg_frc_pd_pat_num = 10;
			reg_out_frm_dly_num= 15;
		}
		else if(film_mode == EN_FILM55) {
			reg_ip_film_end    = 0x10;
			reg_frc_pd_pat_num = 5;
			reg_out_frm_dly_num= 10;
		}
		else if(film_mode == EN_FILM64) {
			reg_ip_film_end    = 0x208;
			reg_frc_pd_pat_num = 10;
			reg_out_frm_dly_num= 15;
		}
		else if(film_mode == EN_FILM66) {
			reg_ip_film_end    = 0x20;
			reg_frc_pd_pat_num = 6;
			reg_out_frm_dly_num= 15;
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
			reg_out_frm_dly_num= 15;
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

void init_bb_xyxy(u32 hsize ,u32 vsize)
{
	 u32     reg_data;
	 u32     me_width     ;
	 u32     me_height    ;
	 u32     hme_width    ;
	 u32     hme_height   ;
	 u32     me_blk_xsize ;
	 u32     me_blk_ysize ;
	 u32     hme_blk_xsize;
	 u32     hme_blk_ysize;
	 u32     me_dsx          ;
	 u32     me_dsy          ;
	 u32     hme_dsx         ;
	 u32     hme_dsy         ;
	 u32     me_blksize_dsx  ;
	 u32     me_blksize_dsy  ;
	 u32     hme_blksize_dsx ;
	 u32     hme_blksize_dsy ;

	 reg_data=READ_FRC_REG(FRC_REG_ME_HME_SCALE);

	 me_dsx  = (reg_data>>6) & 0x3;
	 me_dsy  = (reg_data>>4) & 0x3;
	 hme_dsx = (reg_data>>2) & 0x3;
	 hme_dsy = (reg_data)    & 0x3;

	reg_data=READ_FRC_REG(FRC_REG_ME_HME_SCALE);
	me_dsx  = (reg_data>>6) & 0x3;
	me_dsy  = (reg_data>>4) & 0x3;
	hme_dsx = (reg_data>>2) & 0x3;
	hme_dsy = (reg_data>>0) & 0x3;

	reg_data=READ_FRC_REG(FRC_REG_BLK_SIZE_XY);
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

	//me  me_blksize_dsx， me_blksize_dsy-> FRC_REG_BLK_SIZE_XY
	 me_width      = (hsize+(1<<me_dsx)-1)/(1<<me_dsx);/*960*/
	 me_height     = (vsize+(1<<me_dsy)-1)/(1<<me_dsy);/*540*/
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

	 WRITE_FRC_BITS(FRC_ME_BB_PIX_ED, me_width-1,     16, 12);/*reg_me_bb_xyxy[2] ：960*/
	 WRITE_FRC_BITS(FRC_ME_BB_PIX_ED, me_height-1,     0, 12);/*reg_me_bb_xyxy[3] ：540*/
	 WRITE_FRC_BITS(FRC_ME_BB_BLK_ED, me_blk_xsize-1, 16, 10);/*reg_me_bb_blk_xyxy[2] 240*/
	 WRITE_FRC_BITS(FRC_ME_BB_BLK_ED, me_blk_ysize-1,  0, 10);/*reg_me_bb_blk_xyxy[3] 135*/

	WRITE_FRC_BITS(FRC_ME_STAT_12R_HST   , 0, 0,10);/*reg_me_stat_region_hend 0*/
	WRITE_FRC_BITS(FRC_ME_STAT_12R_H01   , me_blk_xsize*1/4 ,16,10);/*reg_me_stat_region_hend 60*/
	WRITE_FRC_BITS(FRC_ME_STAT_12R_H01   , me_blk_xsize*2/4 ,0 ,10);/*reg_me_stat_region_hend 120*/
	WRITE_FRC_BITS(FRC_ME_STAT_12R_H23   , me_blk_xsize*3/4 ,16,10);/*reg_me_stat_region_hend 180*/
	WRITE_FRC_BITS(FRC_ME_STAT_12R_H23   , me_blk_xsize*4/4 - 1, 0 ,10);/*reg_me_stat_region_hend 240*/

	WRITE_FRC_BITS(FRC_ME_STAT_12R_V0    , 0 ,16 ,10);
	WRITE_FRC_BITS(FRC_ME_STAT_12R_V0    , me_blk_ysize*1/3 ,0 ,10);/*reg_me_stat_region_vend[0]: 45*/
	WRITE_FRC_BITS(FRC_ME_STAT_12R_V1    , me_blk_ysize*2/3 ,16,10);/*reg_me_stat_region_vend[1]: 90*/
	WRITE_FRC_BITS(FRC_ME_STAT_12R_V1    , me_blk_ysize*3/3 - 1,0 ,10);/*reg_me_stat_region_vend[2]: 135*/

	//hme
	 WRITE_FRC_BITS(FRC_ME_BB_PIX_ED- HME_ME_OFFSET, hme_width -1,16,12);/*hme w 240*/
	 WRITE_FRC_BITS(FRC_ME_BB_PIX_ED- HME_ME_OFFSET, hme_height-1,0 ,12);/*hme h 135*/
	 WRITE_FRC_BITS(FRC_ME_BB_BLK_ED- HME_ME_OFFSET  ,hme_blk_xsize-1,16,10);/*hme w block 60*/
	 WRITE_FRC_BITS(FRC_ME_BB_BLK_ED- HME_ME_OFFSET  ,hme_blk_ysize-1,0 ,10);/*hme w block 34*/

	WRITE_FRC_BITS(FRC_ME_STAT_12R_HST - HME_ME_OFFSET  , 0 ,16,10);
	WRITE_FRC_BITS(FRC_ME_STAT_12R_H01 - HME_ME_OFFSET  , hme_blk_xsize*1/4 ,16,10);
	WRITE_FRC_BITS(FRC_ME_STAT_12R_H01 - HME_ME_OFFSET  , hme_blk_xsize*2/4 ,0 ,10);
	WRITE_FRC_BITS(FRC_ME_STAT_12R_H23 - HME_ME_OFFSET  , hme_blk_xsize*3/4 ,16,10);
	WRITE_FRC_BITS(FRC_ME_STAT_12R_H23 - HME_ME_OFFSET  , hme_blk_xsize*4/4 - 1, 0, 10);
	WRITE_FRC_BITS(FRC_ME_STAT_12R_V0  - HME_ME_OFFSET  , hme_blk_ysize*1/3 ,0 ,10);
	WRITE_FRC_BITS(FRC_ME_STAT_12R_V1  - HME_ME_OFFSET  , hme_blk_ysize*2/3 ,16,10);
	WRITE_FRC_BITS(FRC_ME_STAT_12R_V1  - HME_ME_OFFSET  , hme_blk_ysize*3/3 - 1, 0, 10);

	//vp
	WRITE_FRC_BITS(FRC_VP_BB_1, 0, 0, 16);
	WRITE_FRC_BITS(FRC_VP_BB_1, 0, 16, 16);
	WRITE_FRC_BITS(FRC_VP_BB_2 , me_blk_xsize-1,0 ,16);/*reg_vp_bb_xyxy[2] 240*/
	WRITE_FRC_BITS(FRC_VP_BB_2 , me_blk_ysize-1,16,16);/*reg_vp_bb_xyxy[3] 135*/

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
	fw_param_bbd_init(frm_hsize, frm_vsize);
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
//	WRITE_FRC_BITS(FRC_REG_ME_HME_SCALE, reg_hme_dsx_scale, 2, 2);
//	WRITE_FRC_BITS(FRC_REG_ME_HME_SCALE, reg_hme_dsy_scale, 0, 2);

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

	reg_bb_det_motion_top  =  (0 == reg_me_dsy_scale) ? reg_bb_det_top : reg_bb_det_top     >> reg_me_dsy_scale;
	reg_bb_det_motion_bot  = ((0 == reg_me_dsy_scale) ? reg_bb_det_bot : (reg_bb_det_bot+1) >> reg_me_dsy_scale) - 1;
	reg_bb_det_motion_lft  =  (0 == reg_me_dsx_scale) ? reg_bb_det_lft : (reg_bb_det_lft >> reg_me_dsx_scale);
	reg_bb_det_motion_rit  = ((0 == reg_me_dsx_scale) ? reg_bb_det_rit : (reg_bb_det_rit+1) >> reg_me_dsx_scale) - 1;

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
		WRITE_FRC_BITS(FRC_REG_DEMOWINDOW1_XYXY_ST, frm_hsize >> 1, 16, 16); //reg_demowindow1_xyxy_0
		WRITE_FRC_BITS(FRC_REG_DEMOWINDOW1_XYXY_ST,              0,  0, 16); //reg_demowindow1_xyxy_1
		WRITE_FRC_BITS(FRC_REG_DEMOWINDOW1_XYXY_ED,  frm_hsize - 1, 16, 16); //reg_demowindow1_xyxy_2
		WRITE_FRC_BITS(FRC_REG_DEMOWINDOW1_XYXY_ED,  frm_vsize - 1,  0, 16); //reg_demowindow1_xyxy_3
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

//`endif
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

/*
 * driver probe call
 */
void frc_internal_initial(void)
{
	int i;
	int mc_range_norm_lut[36] = {
		-8, 8, -24, 24, -16, 16, -16, 16,
		-24, 24, -8, 8, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0
	};
	int mc_range_sing_lut[36] = {
		-8, 8, -40, 8, -8, 16, -32, 8,
		-8, 24, -24, 8, -8, 32, -16, 8,
		-8, 40, -8, 8, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0
	};

	//frc_test_pattern_cfg(1);
	//WRITE_FRC_REG(FRC_AUTO_RST_SEL, 0);
	//internal test pattern speeder
	WRITE_FRC_BITS(FRC_IP_PAT_RECT_MV, 8, 16, 12);
	WRITE_FRC_BITS(FRC_IP_PAT_RECT_MV, 4, 0, 12);
	//move mode
	WRITE_FRC_BITS(FRC_IP_PAT_RECT_CYCLE, 1, 8, 1);
	/*film 32 mode*/
	WRITE_FRC_BITS(FRC_IP_PAT_RECT_CYCLE, 2, 5, 3);
	WRITE_FRC_BITS(FRC_IP_PAT_RECT_CYCLE, 6, 0, 5);

	/*default 1: bypass frc, 0: data in frc*/
	vpu_reg_write_bits(VPU_FRC_TOP_CTRL, 1, 0, 1);
	/* test pattern on */
	frc_pattern_on(0);

	/**/
	WRITE_FRC_BITS(FRC_REG_BLK_SIZE_XY, 2/*prm_frc->reg_me_mvx_div_mode*/, 14, 2);
	WRITE_FRC_BITS(FRC_REG_BLK_SIZE_XY, 2/*prm_frc->reg_me_mvy_div_mode*/, 12, 2);

	WRITE_FRC_BITS(FRC_REG_BLK_SIZE_XY, 2/*prm_frc->reg_hme_mvx_div_mode*/, 2, 2);
	WRITE_FRC_BITS(FRC_REG_BLK_SIZE_XY, 2/*prm_frc->reg_hme_mvy_div_mode*/, 0, 2);

	/*ttttttttt*/
	WRITE_FRC_BITS(FRC_REG_BLK_SIZE_XY, 2/*prm_frc->reg_me_blksize_x*/, 19, 2);
	WRITE_FRC_BITS(FRC_REG_BLK_SIZE_XY, 2/*prm_frc->reg_me_blksize_y*/, 16, 2);

	WRITE_FRC_BITS(FRC_REG_BLK_SIZE_XY, 2/*prm_frc->reg_hme_blksize_x*/, 7, 2);
	WRITE_FRC_BITS(FRC_REG_BLK_SIZE_XY, 2/*prm_frc->reg_hme_blksize_y*/, 4, 2);

	WRITE_FRC_BITS(FRC_REG_ME_BLD_COEF, 13/*prm_frc->reg_me_bld_coef[0]*/, 24, 6);
	//prm_frc->reg_me_dsx_coef[k] = me_ds_coef[k]  me_ds_coef[] = {48, 24, 12, 4};
	//prm_frc->reg_me_dsy_coef[k] = me_ds_coef[k]
	WRITE_FRC_BITS(FRC_REG_ME_DS_COEF_0, 48, 8, 8);
	WRITE_FRC_BITS(FRC_REG_ME_DS_COEF_0, 48, 0, 8);
	WRITE_FRC_BITS(FRC_REG_ME_DS_COEF_1, 24, 8, 8);
	WRITE_FRC_BITS(FRC_REG_ME_DS_COEF_1, 24, 0, 8);
	WRITE_FRC_BITS(FRC_REG_ME_DS_COEF_2, 12, 8, 8);
	WRITE_FRC_BITS(FRC_REG_ME_DS_COEF_2, 12, 0, 8);
	WRITE_FRC_BITS(FRC_REG_ME_DS_COEF_3, 4, 8, 8);
	WRITE_FRC_BITS(FRC_REG_ME_DS_COEF_3, 4, 0, 8);

	//prm_frc->reg_hme_bld_coef[0] = 13
	WRITE_FRC_BITS(FRC_REG_HME_BLD_COEF, 13, 24, 6);
	//prm_frc->reg_hme_dsx_coef[k] = hme_ds_coef[k]; hme_ds_coef[] = {24, 20, 16, 16};
	//prm_frc->reg_hme_dsy_coef[k] = hme_ds_coef[k]
	WRITE_FRC_BITS(FRC_REG_HME_DS_COEF_0, 24, 8, 8);
	WRITE_FRC_BITS(FRC_REG_HME_DS_COEF_0, 24, 0, 8);
	WRITE_FRC_BITS(FRC_REG_HME_DS_COEF_1, 20, 8, 8);
	WRITE_FRC_BITS(FRC_REG_HME_DS_COEF_1, 20, 0, 8);
	WRITE_FRC_BITS(FRC_REG_HME_DS_COEF_2, 16, 8, 8);
	WRITE_FRC_BITS(FRC_REG_HME_DS_COEF_2, 16, 0, 8);
	WRITE_FRC_BITS(FRC_REG_HME_DS_COEF_3, 16, 8, 8);
	WRITE_FRC_BITS(FRC_REG_HME_DS_COEF_3, 16, 0, 8);

	WRITE_FRC_BITS(FRC_ME_EN, 1/*prm_me->reg_me_lpf_en*/, 30, 1);
	WRITE_FRC_BITS(FRC_ME_EN, 2/*prm_me->reg_me_mvx_div_mode*/, 4, 2);
	WRITE_FRC_BITS(FRC_ME_EN, 2/*prm_me->reg_me_mvy_div_mode*/, 0, 2);

	WRITE_FRC_BITS(FRC_ME_BB_PIX_ST, 1/*prm_me->reg_me_bb_mode*/, 31, 1);

	WRITE_FRC_BITS(FRC_ME_STAT_NEW_REGION, 0/*prm_me->reg_me_region_fb_loop_sel*/, 16, 2);

	WRITE_FRC_BITS(FRC_ME_STAT_NEW_REGION, 6/*prm_me->reg_me_region_fb_xsize*/, 8, 8);
	WRITE_FRC_BITS(FRC_ME_STAT_NEW_REGION, 4/*prm_me->reg_me_region_fb_ysize*/, 0, 8);

	WRITE_FRC_BITS(FRC_ME_RPD_ADD_CANDIDATES, 1/*prm_me->reg_me_phs_rp_dil_cnt_th*/, 20, 4);

	WRITE_FRC_BITS(FRC_ME_RPD_ADD_CANDIDATES, 0/*prm_me->reg_me_rpdb_en_fs*/, 16, 1);
	WRITE_FRC_BITS(FRC_ME_RPD_ADD_CANDIDATES, 1/*prm_me->reg_me_rpd0_en_fs*/, 15, 1);

	WRITE_FRC_BITS(FRC_ME_RPD_ADD_CANDIDATES, 0/*me->reg_me_rpd_penalty_gmv_en*/, 11, 1);
	WRITE_FRC_BITS(FRC_ME_RPD_ADD_CANDIDATES, 0/*prm_me->reg_me_rpd_penalty_st_en*/, 10, 1);

	WRITE_FRC_BITS(FRC_ME_RPD_ADD_CANDIDATES, 1/*me->reg_me_fs2vp_sel[0]*/, 9, 1);
	WRITE_FRC_BITS(FRC_ME_RPD_ADD_CANDIDATES, 1/*me->reg_me_fs2vp_sel[1]*/, 8, 1);

	WRITE_FRC_BITS(FRC_ME_BVSEL_GROUP_THX, 64/*me->reg_me_mvx_var_gain1*/, 0, 8);
	WRITE_FRC_BITS(FRC_ME_BVSEL_GROUP_THXY, 64/*me->reg_me_mvxy_var_gain1*/, 0, 8);

	WRITE_FRC_BITS(FRC_ME_BB_HANDLE, 1/*me->reg_me_oob_movinside_x_ofst_th*/, 26, 4);
	WRITE_FRC_BITS(FRC_ME_BB_HANDLE, 1/*me->reg_me_oob_movinside_y_ofst_th*/, 22, 4);

	WRITE_FRC_BITS(FRC_ME_BVSEL_GROUP_EN, 4/*me->reg_me_oob_movinside_x_ofst_th2*/, 12, 4);
	WRITE_FRC_BITS(FRC_ME_BVSEL_GROUP_EN, 4/*me->reg_me_oob_movinside_y_ofst_th2*/, 8, 4);

	//prm_me->reg_me_penalty_gmv[m] = penalty_gmv[m]; int penalty_gmv[] = {60, 58, 58};
	WRITE_FRC_BITS(FRC_ME_CAD_ZGMV_EN_0 - HME_ME_OFFSET, 60/*hme->reg_me_penalty_gmv[0]*/, 16, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_ZGMV_EN_0, 60/*me->reg_me_penalty_gmv[0]*/, 16, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_ZGMV_EN_1 - HME_ME_OFFSET, 58/*hme->reg_me_penalty_gmv[1]*/, 16, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_ZGMV_EN_1, 58/*me->reg_me_penalty_gmv[1]*/, 16, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_ZGMV_EN_2 - HME_ME_OFFSET, 58/*hme->reg_me_penalty_gmv[2]*/, 16, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_ZGMV_EN_2, 58/*me->reg_me_penalty_gmv[2]*/, 16, 8);

	//prm_me->reg_me_rpd_cn_en  = 1
	WRITE_FRC_BITS(FRC_ME_RPD_EN, 1/*me->reg_me_rpd_cn_en*/, 31, 1);
	//prm_me->reg_me_rpd_nc_en  = 1
	WRITE_FRC_BITS(FRC_ME_RPD_EN, 1/*me->reg_me_rpd_nc_en*/, 30, 1);

	//prm_me->reg_me_rpd_flg_refine_cnt_th = 3
	WRITE_FRC_BITS(FRC_ME_RPD_EN, 3/*me->reg_me_rpd_flg_refine_cnt_th*/, 24, 3);
	//prm_me->reg_me_rpd_flg_refine_bdy = 2
	WRITE_FRC_BITS(FRC_ME_RPD_EN, 2/*me->reg_me_rpd_flg_refine_bdy*/, 22, 2);

	//prm_me->reg_me_fs_lwidth_col_chk_max_th = 12
	//prm_me->reg_me_fs_lwidth_row_chk_max_th = 12
	WRITE_FRC_BITS(FRC_ME_RPD_EN, 12/*me->reg_me_fs_lwidth_col_chk_max_th*/, 8, 7);
	WRITE_FRC_BITS(FRC_ME_RPD_EN, 12/*me->reg_me_fs_lwidth_row_chk_max_th*/, 0, 8);

	//prm_me->reg_me_fs_dil_cnt_th =2
	WRITE_FRC_BITS(FRC_ME_RPD_FS_POST, 2/*me->reg_me_fs_dil_cnt_th*/, 16, 4);

	//prm_me->reg_me_rpd_min_valley_th =  40
	//prm_me->reg_me_rpd_min_peak_th   =  40
	WRITE_FRC_BITS(FRC_ME_RPD_VALLEY_PEAK, 40/*me->reg_me_rpd_min_peak_th*/, 0, 8);
	WRITE_FRC_BITS(FRC_ME_RPD_VALLEY_PEAK, 40/*me->reg_me_rpd_min_valley_th*/, 16, 8);

	//prm_me->reg_me_rpd_t1_flat_th0 = 600
	WRITE_FRC_BITS(FRC_ME_RPD_T1_FLAT, 600/*me->reg_me_rpd_t1_flat_th0*/, 16, 12);
	//prm_me->reg_me_rpd_t2_lhlhl_th = 12
	WRITE_FRC_BITS(FRC_ME_RPD_T2_LHL, 12/*me->reg_me_rpd_t2_lhlhl_th*/, 16, 8);

	//prm_me->reg_me_rpd_t2_flat_th0  = 600
	//prm_me->reg_me_rpd_t2_flat_th2  = 400
	WRITE_FRC_BITS(FRC_ME_RPD_T2_FLAT, 600/*me->reg_me_rpd_t2_flat_th0*/, 20, 12);
	WRITE_FRC_BITS(FRC_ME_RPD_T2_FLAT, 400/*me->reg_me_rpd_t2_flat_th2*/, 0, 12);

	//prm_me->reg_me_rpd_t3_flat_th0  = 500
	WRITE_FRC_BITS(FRC_ME_RPD_T3_FLAT, 500/*me->reg_me_rpd_t3_flat_th0*/, 0, 12);
	//prm_me->reg_me_rpd_auto_flat_th0 = 600
	WRITE_FRC_BITS(FRC_ME_RPD_AUTO_FLAT, 600/*me->reg_me_rpd_auto_flat_th0*/, 16, 12);

	WRITE_FRC_BITS(FRC_ME_CAD_FS_EN, 1/*me->reg_me_add_fs_cn_en*/, 31, 1);
	WRITE_FRC_BITS(FRC_ME_CAD_FS_EN, 1/*me->reg_me_add_fs_nc_en*/, 30, 1);

	WRITE_FRC_BITS(FRC_ME_CAD_FS_EN, 2/*me->reg_me_fs_sad_core_th*/, 0, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_FS_EN, 1/*me->reg_me_fs_src_flag_mode*/, 8, 1);

	WRITE_FRC_BITS(FRC_ME_CAD_PRJ_EN, 1/*me->reg_me_add_prj_en*/, 18, 1);
	WRITE_FRC_BITS(FRC_ME_CAD_PRJ_GROUP_2, 8/*me->reg_mv_proj_gmv_diff_th*/, 8, 4);

	WRITE_FRC_BITS(FRC_ME_RPD_RAD_GAIN_CN, 4/*me->reg_me_rad_rpchk_cn_xgain*/, 8, 5);
	WRITE_FRC_BITS(FRC_ME_RPD_RAD_GAIN_CN, 4/*me->reg_me_rad_rpchk_cn_ygain*/, 0, 5);
	WRITE_FRC_BITS(FRC_ME_RPD_RAD_GAIN_NC, 4/*me->reg_me_rad_rpchk_nc_xgain*/, 8, 5);
	WRITE_FRC_BITS(FRC_ME_RPD_RAD_GAIN_NC, 4/*me->reg_me_rad_rpchk_nc_ygain*/, 0, 5);

	WRITE_FRC_BITS(FRC_ME_RPD_RAD_GAIN_PHS, 0/*me->reg_me_rad_rpchk_phs_en*/, 16, 1);
	WRITE_FRC_BITS(FRC_ME_RPD_RAD_GAIN_PHS, 4/*me->reg_me_rad_rpchk_phs_xgain*/, 8, 5);
	WRITE_FRC_BITS(FRC_ME_RPD_RAD_GAIN_PHS, 4/*me->reg_me_rad_rpchk_phs_ygain*/, 0, 5);

	WRITE_FRC_BITS(FRC_ME_RPD_PENALTY_GMV, 0/*me->reg_me_rpd_penalty_gmv_cn*/, 8, 8);
	WRITE_FRC_BITS(FRC_ME_RPD_PENALTY_GMV, 0/*me->reg_me_rpd_penalty_gmv_nc*/, 0, 8);

	WRITE_FRC_BITS(FRC_ME_BVSEL_GROUP_THXY, 20/*me->reg_me_mvxy_var_gain1*/, 0, 8);

	WRITE_FRC_BITS(FRC_ME_CAD_SETS_SEL_0, 1/*me->reg_me_add_sel[0][1]*/, 24, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_SETS_SEL_0, 2/*me->reg_me_add_sel[0][2]*/, 20, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_SETS_SEL_0, 0/*me->reg_me_add_sel[0][3]*/, 16, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_SETS_SEL_0, 3/*me->reg_me_add_sel[0][4]*/, 12, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_SETS_SEL_0, 4/*me->reg_me_add_sel[0][5]*/, 8, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_SETS_SEL_0, 10/*me->reg_me_add_sel[0][6]*/, 4, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_SETS_SEL_0, 7/*me->reg_me_add_sel[0][7]*/, 0, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_SETS_SEL_1, 1/*me->reg_me_add_sel[1][1]*/, 24, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_SETS_SEL_1, 2/*me->reg_me_add_sel[1][2]*/, 20, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_SETS_SEL_1, 0/*me->reg_me_add_sel[1][3]*/, 16, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_SETS_SEL_1, 3/*me->reg_me_add_sel[1][4]*/, 12, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_SETS_SEL_1, 5/*me->reg_me_add_sel[1][5]*/, 8, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_SETS_SEL_1, 10/*me->reg_me_add_sel[1][6]*/, 4, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_SETS_SEL_1, 7/*me->reg_me_add_sel[1][7]*/, 0, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_SETS_SEL_2, 1/*me->reg_me_add_sel[2][1]*/, 24, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_SETS_SEL_2, 2/*me->reg_me_add_sel[2][2]*/, 20, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_SETS_SEL_2, 0/*me->reg_me_add_sel[2][3]*/, 16, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_SETS_SEL_2, 3/*me->reg_me_add_sel[2][4]*/, 12, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_SETS_SEL_2, 5/*me->reg_me_add_sel[2][5]*/, 8, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_SETS_SEL_2, 10/*me->reg_me_add_sel[2][6]*/, 4, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_SETS_SEL_2, 7/*me->reg_me_add_sel[2][7]*/, 0, 4);

	WRITE_FRC_BITS(FRC_ME_CAD_ST_EN_0, 0/*me->reg_me_add_s_t_en[0][3]*/, 6, 1);
	WRITE_FRC_BITS(FRC_ME_CAD_ST_EN_0, 0/*me->reg_me_add_s_t_en[0][4]*/, 5, 1);
	WRITE_FRC_BITS(FRC_ME_CAD_ST_EN_0, 1/*me->reg_me_add_s_t_en[0][7]*/, 2, 1);
	WRITE_FRC_BITS(FRC_ME_CAD_ST_EN_0, 1/*me->reg_me_add_s_t_en[0][8]*/, 1, 1);
	WRITE_FRC_BITS(FRC_ME_CAD_ST_EN_1, 0/*me->reg_me_add_s_t_en[1][3]*/, 6, 1);
	WRITE_FRC_BITS(FRC_ME_CAD_ST_EN_1, 0/*me->reg_me_add_s_t_en[1][4]*/, 5, 1);
	WRITE_FRC_BITS(FRC_ME_CAD_ST_EN_1, 1/*me->reg_me_add_s_t_en[1][7]*/, 2, 1);
	WRITE_FRC_BITS(FRC_ME_CAD_ST_EN_1, 1/*me->reg_me_add_s_t_en[1][8]*/, 1, 1);
	WRITE_FRC_BITS(FRC_ME_CAD_ST_EN_2, 0/*me->reg_me_add_s_t_en[2][3]*/, 6, 1);
	WRITE_FRC_BITS(FRC_ME_CAD_ST_EN_2, 0/*me->reg_me_add_s_t_en[2][4]*/, 5, 1);
	WRITE_FRC_BITS(FRC_ME_CAD_ST_EN_2, 1/*me->reg_me_add_s_t_en[2][7]*/, 2, 1);
	WRITE_FRC_BITS(FRC_ME_CAD_ST_EN_2, 1/*me->reg_me_add_s_t_en[2][8]*/, 1, 1);

	WRITE_FRC_BITS(FRC_ME_CAD_ST1_0, 42/*me->reg_me_penalty_s_t[0][2]*/, 8, 8);
	//WRITE_FRC_BITS(FRC_ME_CAD_ST1_0, 0/*me->reg_me_penalty_s_t[0][3]*/, 0, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_ST2_0, 0/*me->reg_me_penalty_s_t[0][4]*/, 8, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_ST2_0, 44/*me->reg_me_penalty_s_t[0][5]*/, 0, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_ST3_0, 56/*me->reg_me_penalty_s_t[0][7]*/, 0, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_ST4_0, 56/*me->reg_me_penalty_s_t[0][8]*/, 8, 8);

	WRITE_FRC_BITS(FRC_ME_CAD_ST1_1, 42/*me->reg_me_penalty_s_t[1][2]*/, 8, 8);
	//WRITE_FRC_BITS(FRC_ME_CAD_ST1_1, 0/*me->reg_me_penalty_s_t[1][3]*/, 0, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_ST2_1, 0/*me->reg_me_penalty_s_t[1][4]*/, 8, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_ST2_1, 44/*me->reg_me_penalty_s_t[1][5]*/, 0, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_ST3_1, 56/*me->reg_me_penalty_s_t[1][7]*/, 0, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_ST4_1, 56/*me->reg_me_penalty_s_t[1][8]*/, 8, 8);

	WRITE_FRC_BITS(FRC_ME_CAD_ST1_2, 42/*me->reg_me_penalty_s_t[2][2]*/, 8, 8);
	//WRITE_FRC_BITS(FRC_ME_CAD_ST1_2, 0/*me->reg_me_penalty_s_t[2][3]*/, 0, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_ST2_2, 0/*me->reg_me_penalty_s_t[2][4]*/, 8, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_ST2_2, 44/*me->reg_me_penalty_s_t[2][5]*/, 0, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_ST3_2, 56/*me->reg_me_penalty_s_t[2][7]*/, 0, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_ST4_2, 56/*me->reg_me_penalty_s_t[2][8]*/, 8, 8);

	WRITE_FRC_BITS(FRC_ME_CAD_RAD0_MM_0, 3/*me->reg_me_rad_x2_max[0][0]*/, 4, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD0_MM_0, 2/*me->reg_me_rad_y2_max[0][0]*/, 0, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD0_MM_1, 3/*me->reg_me_rad_x2_max[1][0]*/, 4, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD0_MM_1, 2/*me->reg_me_rad_y2_max[1][0]*/, 0, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD0_MM_2, 3/*me->reg_me_rad_x2_max[2][0]*/, 4, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD0_MM_2, 2/*me->reg_me_rad_y2_max[2][0]*/, 0, 4);

	WRITE_FRC_BITS(FRC_ME_CAD_RAD1_MM_0, 5/*me->reg_me_rad_x1_max[0][1]*/, 12, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD1_MM_0, 4/*me->reg_me_rad_y1_max[0][1]*/, 8, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD1_MM_1, 5/*me->reg_me_rad_x1_max[1][1]*/, 12, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD1_MM_1, 4/*me->reg_me_rad_y1_max[1][1]*/, 8, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD1_MM_2, 5/*me->reg_me_rad_x1_max[2][1]*/, 12, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD1_MM_2, 4/*me->reg_me_rad_y1_max[2][1]*/, 8, 4);

	WRITE_FRC_BITS(FRC_ME_CAD_RAD1_MM_0, 11/*me->reg_me_rad_x2_max[0][1]*/, 4, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD1_MM_0, 8/*me->reg_me_rad_y2_max[0][1]*/, 0, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD1_MM_1, 11/*me->reg_me_rad_x2_max[1][1]*/, 4, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD1_MM_1, 8/*me->reg_me_rad_y2_max[1][1]*/, 0, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD1_MM_2, 11/*me->reg_me_rad_x2_max[2][1]*/, 4, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD1_MM_2, 8/*me->reg_me_rad_y2_max[2][1]*/, 0, 4);

	WRITE_FRC_BITS(FRC_ME_CAD_RAD2_MM_0, 2/*me->reg_me_rad_x2_max[0][2]*/, 4, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD2_MM_0, 2/*me->reg_me_rad_y2_max[0][2]*/, 0, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD2_MM_1, 2/*me->reg_me_rad_x2_max[1][2]*/, 4, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD2_MM_1, 2/*me->reg_me_rad_y2_max[1][2]*/, 0, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD2_MM_2, 2/*me->reg_me_rad_x2_max[2][2]*/, 4, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD2_MM_2, 2/*me->reg_me_rad_y2_max[2][2]*/, 0, 4);

	WRITE_FRC_BITS(FRC_ME_CAD_RAD3_MM_0, 2/*me->reg_me_rad_x2_max[0][3]*/, 4, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD3_MM_0, 2/*me->reg_me_rad_y2_max[0][3]*/, 0, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD3_MM_1, 2/*me->reg_me_rad_x2_max[1][3]*/, 4, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD3_MM_1, 2/*me->reg_me_rad_y2_max[1][3]*/, 0, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD3_MM_2, 2/*me->reg_me_rad_x2_max[2][3]*/, 4, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_RAD3_MM_2, 2/*me->reg_me_rad_y2_max[2][3]*/, 0, 4);

	WRITE_FRC_BITS(FRC_ME_CAD_HIE_PEN0_0, 48/*me->reg_me_penalty_h[0][3]*/, 0, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_HIE_PEN0_1, 48/*me->reg_me_penalty_h[1][3]*/, 0, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_HIE_PEN0_2, 48/*me->reg_me_penalty_h[2][3]*/, 0, 8);

	WRITE_FRC_BITS(FRC_ME_CAD_HIE_PEN1_0, 48/*me->reg_me_penalty_h[0][6]*/, 8, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_HIE_PEN1_1, 48/*me->reg_me_penalty_h[1][6]*/, 8, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_HIE_PEN1_2, 48/*me->reg_me_penalty_h[2][6]*/, 8, 8);

	WRITE_FRC_BITS(FRC_ME_CAD_HIE_PEN2_0, 48/*me->reg_me_penalty_h[0][9]*/, 16, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_HIE_PEN2_1, 48/*me->reg_me_penalty_h[1][9]*/, 16, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_HIE_PEN2_2, 48/*me->reg_me_penalty_h[2][9]*/, 16, 8);

	WRITE_FRC_BITS(FRC_ME_CAD_HIE_PEN3_0, 48/*me->reg_me_penalty_h[0][12]*/, 24, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_HIE_PEN3_1, 48/*me->reg_me_penalty_h[1][12]*/, 24, 8);
	WRITE_FRC_BITS(FRC_ME_CAD_HIE_PEN3_2, 48/*me->reg_me_penalty_h[2][12]*/, 24, 8);

	//prm_me->reg_me_cmv_s_t_pos_x[m][0] =  0
	///prm_me->reg_me_cmv_s_t_pos_x[m][1] = -2
	//prm_me->reg_me_cmv_s_t_pos_x[m][2] =  0
	///prm_me->reg_me_cmv_s_t_pos_x[m][4] =  0
	//prm_me->reg_me_cmv_s_t_pos_x[m][5] = -1
	//prm_me->reg_me_cmv_s_t_pos_x[m][6] =  1
	//prm_me->reg_me_cmv_s_t_pos_x[m][7] = -2
	//prm_me->reg_me_cmv_s_t_pos_x[m][8] =  2

	WRITE_FRC_BITS(FRC_ME_CAD_POS0_0, 0/*me->reg_me_cmv_s_t_pos_x[0][0]*/, 28, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS0_0, (u32)(-2)/*me->reg_me_cmv_s_t_pos_x[0][1]*/, 20, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS0_0, 0/*me->reg_me_cmv_s_t_pos_x[0][2]*/, 12, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_0, 0/*me->reg_me_cmv_s_t_pos_x[0][4]*/, 28, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_0, (u32)(-1)/*me->reg_me_cmv_s_t_pos_x[0][5]*/, 20, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_0, 1/*me->reg_me_cmv_s_t_pos_x[0][6]*/, 12, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_0, (u32)(-2)/*me->reg_me_cmv_s_t_pos_x[0][7]*/, 4, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS2_0, 2/*me->reg_me_cmv_s_t_pos_x[0][8]*/, 12, 4);

	WRITE_FRC_BITS(FRC_ME_CAD_POS0_1, 0/*me->reg_me_cmv_s_t_pos_x[1][0]*/, 28, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS0_1, (u32)(-2)/*me->reg_me_cmv_s_t_pos_x[1][1]*/, 20, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS0_1, 0/*me->reg_me_cmv_s_t_pos_x[1][2]*/, 12, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_1, 0/*me->reg_me_cmv_s_t_pos_x[1][4]*/, 28, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_1, (u32)(-1)/*me->reg_me_cmv_s_t_pos_x[1][5]*/, 20, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_1, 1/*me->reg_me_cmv_s_t_pos_x[1][6]*/, 12, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_1, (u32)(-2)/*me->reg_me_cmv_s_t_pos_x[1][7]*/, 4, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS2_1, 2/*me->reg_me_cmv_s_t_pos_x[1][8]*/, 12, 4);

	WRITE_FRC_BITS(FRC_ME_CAD_POS0_2, 0/*me->reg_me_cmv_s_t_pos_x[2][0]*/, 28, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS0_2, (u32)(-2)/*me->reg_me_cmv_s_t_pos_x[2][1]*/, 20, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS0_2, 0/*me->reg_me_cmv_s_t_pos_x[2][2]*/, 12, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_2, 0/*me->reg_me_cmv_s_t_pos_x[2][4]*/, 28, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_2, (u32)(-1)/*me->reg_me_cmv_s_t_pos_x[2][5]*/, 20, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_2, 1/*me->reg_me_cmv_s_t_pos_x[2][6]*/, 12, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_2, (u32)(-2)/*me->reg_me_cmv_s_t_pos_x[2][7]*/, 4, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS2_2, 2/*me->reg_me_cmv_s_t_pos_x[2][8]*/, 12, 4);

	WRITE_FRC_BITS(FRC_ME_CAD_POS0_3, 0/*me->reg_me_cmv_s_t_pos_x[3][0]*/, 28, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS0_3, (u32)(-2)/*me->reg_me_cmv_s_t_pos_x[3][1]*/, 20, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS0_3, 0/*me->reg_me_cmv_s_t_pos_x[3][2]*/, 12, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_3, 0/*me->reg_me_cmv_s_t_pos_x[3][4]*/, 28, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_3, (u32)(-1)/*me->reg_me_cmv_s_t_pos_x[3][5]*/, 20, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_3, 1/*me->reg_me_cmv_s_t_pos_x[3][6]*/, 12, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_3, (u32)(-2)/*me->reg_me_cmv_s_t_pos_x[3][7]*/, 4, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS2_3, 2/*me->reg_me_cmv_s_t_pos_x[3][8]*/, 12, 4);

	WRITE_FRC_BITS(FRC_ME_CAD_POS0_4, 0/*me->reg_me_cmv_s_t_pos_x[4][0]*/, 28, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS0_4, (u32)(-2)/*me->reg_me_cmv_s_t_pos_x[4][1]*/, 20, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS0_4, 0/*me->reg_me_cmv_s_t_pos_x[4][2]*/, 12, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_4, 0/*me->reg_me_cmv_s_t_pos_x[4][4]*/, 28, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_4, (u32)(-1)/*me->reg_me_cmv_s_t_pos_x[4][5]*/, 20, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_4, 1/*me->reg_me_cmv_s_t_pos_x[4][6]*/, 12, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_4, (u32)(-2)/*me->reg_me_cmv_s_t_pos_x[4][7]*/, 4, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS2_4, 2/*me->reg_me_cmv_s_t_pos_x[4][8]*/, 12, 4);

	WRITE_FRC_BITS(FRC_ME_CAD_POS0_5, 0/*me->reg_me_cmv_s_t_pos_x[5][0]*/, 28, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS0_5, (u32)(-2)/*me->reg_me_cmv_s_t_pos_x[5][1]*/, 20, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS0_5, 0/*me->reg_me_cmv_s_t_pos_x[5][2]*/, 12, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_5, 0/*me->reg_me_cmv_s_t_pos_x[5][4]*/, 28, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_5, (u32)(-1)/*me->reg_me_cmv_s_t_pos_x[5][5]*/, 20, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_5, 1/*me->reg_me_cmv_s_t_pos_x[5][6]*/, 12, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_5, (u32)(-2)/*me->reg_me_cmv_s_t_pos_x[5][7]*/, 4, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS2_5, 2/*me->reg_me_cmv_s_t_pos_x[5][8]*/, 12, 4);

	//prm_me->reg_me_cmv_s_t_pos_y[m][1] =  0
	//prm_me->reg_me_cmv_s_t_pos_y[m][3] =  0
	//prm_me->reg_me_cmv_s_t_pos_y[m][4] =  0
	//prm_me->reg_me_cmv_s_t_pos_y[m][6] = -1
	///prm_me->reg_me_cmv_s_t_pos_y[m][7] =  2
	//prm_me->reg_me_cmv_s_t_pos_y[m][8] =  2

	//WRITE_FRC_BITS(FRC_ME_CAD_POS0_0, 0/*me->reg_me_cmv_s_t_pos_y[0][0]*/, 24, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS0_0, 0/*me->reg_me_cmv_s_t_pos_y[0][1]*/, 16, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS0_0, 0/*me->reg_me_cmv_s_t_pos_y[0][3]*/, 0, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_0, 0/*me->reg_me_cmv_s_t_pos_y[0][4]*/, 24, 4);
	//WRITE_FRC_BITS(FRC_ME_CAD_POS1_0, /*me->reg_me_cmv_s_t_pos_y[0][5]*/, 16, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_0, (u32)(-1)/*me->reg_me_cmv_s_t_pos_y[0][6]*/, 8, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_0, 2/*me->reg_me_cmv_s_t_pos_y[0][7]*/, 0, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS2_0, 2/*me->reg_me_cmv_s_t_pos_y[0][8]*/, 8, 4);

	//WRITE_FRC_BITS(FRC_ME_CAD_POS0_1, 0/*me->reg_me_cmv_s_t_pos_y[1][0]*/, 24, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS0_1, 0/*me->reg_me_cmv_s_t_pos_y[1][1]*/, 16, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS0_1, 0/*me->reg_me_cmv_s_t_pos_y[1][3]*/, 0, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_1, 0/*me->reg_me_cmv_s_t_pos_y[1][4]*/, 24, 4);
	//WRITE_FRC_BITS(FRC_ME_CAD_POS1_1, /*me->reg_me_cmv_s_t_pos_y[1][5]*/, 16, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_1, (u32)(-1)/*me->reg_me_cmv_s_t_pos_y[1][6]*/, 8, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_1, 2/*me->reg_me_cmv_s_t_pos_y[1][7]*/, 0, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS2_1, 2/*me->reg_me_cmv_s_t_pos_y[1][8]*/, 8, 4);

	//WRITE_FRC_BITS(FRC_ME_CAD_POS0_2, 0/*me->reg_me_cmv_s_t_pos_y[2][0]*/, 24, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS0_2, 0/*me->reg_me_cmv_s_t_pos_y[2][1]*/, 16, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS0_2, 0/*me->reg_me_cmv_s_t_pos_y[2][3]*/, 0, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_2, 0/*me->reg_me_cmv_s_t_pos_y[2][4]*/, 24, 4);
	//WRITE_FRC_BITS(FRC_ME_CAD_POS1_2, /*me->reg_me_cmv_s_t_pos_y[2][5]*/, 16, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_2, (u32)(-1)/*me->reg_me_cmv_s_t_pos_y[2][6]*/, 8, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_2, 2/*me->reg_me_cmv_s_t_pos_y[2][7]*/, 0, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS2_2, 2/*me->reg_me_cmv_s_t_pos_y[2][8]*/, 8, 4);

	//WRITE_FRC_BITS(FRC_ME_CAD_POS0_3, 0/*me->reg_me_cmv_s_t_pos_y[3][0]*/, 24, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS0_3, 0/*me->reg_me_cmv_s_t_pos_y[3][1]*/, 16, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS0_3, 0/*me->reg_me_cmv_s_t_pos_y[3][3]*/, 0, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_3, 0/*me->reg_me_cmv_s_t_pos_y[3][4]*/, 24, 4);
	//WRITE_FRC_BITS(FRC_ME_CAD_POS1_3, /*me->reg_me_cmv_s_t_pos_y[3][5]*/, 16, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_3, (u32)(-1)/*me->reg_me_cmv_s_t_pos_y[3][6]*/, 8, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_3, 2/*me->reg_me_cmv_s_t_pos_y[3][7]*/, 0, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS2_3, 2/*me->reg_me_cmv_s_t_pos_y[3][8]*/, 8, 4);

	//WRITE_FRC_BITS(FRC_ME_CAD_POS0_4, 0/*me->reg_me_cmv_s_t_pos_y[4][0]*/, 24, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS0_4, 0/*me->reg_me_cmv_s_t_pos_y[4][1]*/, 16, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS0_4, 0/*me->reg_me_cmv_s_t_pos_y[4][3]*/, 0, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_4, 0/*me->reg_me_cmv_s_t_pos_y[4][4]*/, 24, 4);
	//WRITE_FRC_BITS(FRC_ME_CAD_POS1_4, /*me->reg_me_cmv_s_t_pos_y[4][5]*/, 16, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_4, (u32)(-1)/*me->reg_me_cmv_s_t_pos_y[4][6]*/, 8, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_4, 2/*me->reg_me_cmv_s_t_pos_y[4][7]*/, 0, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS2_4, 2/*me->reg_me_cmv_s_t_pos_y[4][8]*/, 8, 4);

	//WRITE_FRC_BITS(FRC_ME_CAD_POS0_5, 0/*me->reg_me_cmv_s_t_pos_y[5][0]*/, 24, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS0_5, 0/*me->reg_me_cmv_s_t_pos_y[5][1]*/, 16, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS0_5, 0/*me->reg_me_cmv_s_t_pos_y[5][3]*/, 0, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_5, 0/*me->reg_me_cmv_s_t_pos_y[5][4]*/, 24, 4);
	//WRITE_FRC_BITS(FRC_ME_CAD_POS1_5, /*me->reg_me_cmv_s_t_pos_y[5][5]*/, 16, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_5, (u32)(-1)/*me->reg_me_cmv_s_t_pos_y[5][6]*/, 8, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS1_5, 2/*me->reg_me_cmv_s_t_pos_y[5][7]*/, 0, 4);
	WRITE_FRC_BITS(FRC_ME_CAD_POS2_5, 2/*me->reg_me_cmv_s_t_pos_y[5][8]*/, 8, 4);

	//prm_me->reg_me_rad_edge_core[m]      = 2
	WRITE_FRC_BITS(FRC_ME_RAD_EDGE_0, 2/*me->reg_me_rad_edge_core[0]*/, 20, 4);
	WRITE_FRC_BITS(FRC_ME_RAD_EDGE_1, 2/*me->reg_me_rad_edge_core[1]*/, 20, 4);
	WRITE_FRC_BITS(FRC_ME_RAD_EDGE_2, 2/*me->reg_me_rad_edge_core[2]*/, 20, 4);

	//prm_me->reg_me_rad_edge_thn[m]       = 8
	//prm_me->reg_me_rad_edge_th0[m]       =  48
	WRITE_FRC_BITS(FRC_ME_RAD_EDGE_0, 48/*me->reg_me_rad_edge_th0[0]*/, 0, 10);
	WRITE_FRC_BITS(FRC_ME_RAD_EDGE_0, 8/*me->reg_me_rad_edge_thn[0]*/, 16, 4);
	WRITE_FRC_BITS(FRC_ME_RAD_EDGE_1, 48/*me->reg_me_rad_edge_th0[1]*/, 0, 10);
	WRITE_FRC_BITS(FRC_ME_RAD_EDGE_1, 8/*me->reg_me_rad_edge_thn[1]*/, 16, 4);
	WRITE_FRC_BITS(FRC_ME_RAD_EDGE_2, 48/*me->reg_me_rad_edge_th0[2]*/, 0, 10);
	WRITE_FRC_BITS(FRC_ME_RAD_EDGE_2, 8/*me->reg_me_rad_edge_thn[2]*/, 16, 4);

	//prm_me->reg_me_choosebv_mode[m]=1
	WRITE_FRC_BITS(FRC_ME_OBME_MODE_0, 1/*me->reg_me_choosebv_mode[0]*/, 14, 1);
	WRITE_FRC_BITS(FRC_ME_OBME_MODE_1, 1/*me->reg_me_choosebv_mode[1]*/, 14, 1);
	WRITE_FRC_BITS(FRC_ME_OBME_MODE_2, 1/*me->reg_me_choosebv_mode[2]*/, 14, 1);

	//prm_me->reg_obme_mask_mode_max[m] = 1
	//prm_me->reg_obme_mask_mode_min[m] = 1
	WRITE_FRC_BITS(FRC_ME_OBME_MODE_0, 1/*me->reg_obme_mask_mode_max[0]*/, 8, 3);
	WRITE_FRC_BITS(FRC_ME_OBME_MODE_0, 1/*me->reg_obme_mask_mode_min[0]*/, 4, 3);
	WRITE_FRC_BITS(FRC_ME_OBME_MODE_1, 1/*me->reg_obme_mask_mode_max[1]*/, 8, 3);
	WRITE_FRC_BITS(FRC_ME_OBME_MODE_1, 1/*me->reg_obme_mask_mode_min[1]*/, 4, 3);
	WRITE_FRC_BITS(FRC_ME_OBME_MODE_2, 1/*me->reg_obme_mask_mode_max[2]*/, 8, 3);
	WRITE_FRC_BITS(FRC_ME_OBME_MODE_2, 1/*me->reg_obme_mask_mode_min[2]*/, 4, 3);

	//prm_me->reg_obme_mask_mode[m]     = 1
	WRITE_FRC_BITS(FRC_ME_OBME_MODE_0, 1/*me->reg_obme_mask_mode[0]*/, 0, 3);
	WRITE_FRC_BITS(FRC_ME_OBME_MODE_1, 1/*me->reg_obme_mask_mode[1]*/, 0, 3);
	WRITE_FRC_BITS(FRC_ME_OBME_MODE_2, 1/*me->reg_obme_mask_mode[2]*/, 0, 3);

	//prm_me->reg_hme_mvdiff_smooth_th0[m]     = 3
	//prm_me->reg_hme_mvdiff_smooth_thn[m]     = 6
	WRITE_FRC_BITS(FRC_ME_H_MVDIFF_SMOOTH_0, 3/*me->reg_hme_mvdiff_smooth_th0[0]*/, 20, 4);
	WRITE_FRC_BITS(FRC_ME_H_MVDIFF_SMOOTH_0, 6/*me->reg_hme_mvdiff_smooth_thn[0]*/, 16, 4);
	WRITE_FRC_BITS(FRC_ME_H_MVDIFF_SMOOTH_1, 3/*me->reg_hme_mvdiff_smooth_th0[1]*/, 20, 4);
	WRITE_FRC_BITS(FRC_ME_H_MVDIFF_SMOOTH_1, 6/*me->reg_hme_mvdiff_smooth_thn[1]*/, 16, 4);
	WRITE_FRC_BITS(FRC_ME_H_MVDIFF_SMOOTH_2, 3/*me->reg_hme_mvdiff_smooth_th0[2]*/, 20, 4);
	WRITE_FRC_BITS(FRC_ME_H_MVDIFF_SMOOTH_2, 6/*me->reg_hme_mvdiff_smooth_thn[2]*/, 16, 4);

	//prm_me->reg_add_hme_mvdiff_penalty_zmv[m] =1
	//prm_me->reg_add_hme_mvdiff_penalty_rand[m] =1
	//prm_me->reg_add_hme_mvdiff_penalty_fs[m] =1
	WRITE_FRC_BITS(FRC_ME_H_MVDIFF_EN_0, 1/*me->reg_add_hme_mvdiff_penalty_fs[0]*/, 8, 1);
	WRITE_FRC_BITS(FRC_ME_H_MVDIFF_EN_0, 1/*me->reg_add_hme_mvdiff_penalty_rand[0]*/, 10, 1);
	WRITE_FRC_BITS(FRC_ME_H_MVDIFF_EN_0, 1/*me->reg_add_hme_mvdiff_penalty_zmv[0]*/, 12, 1);
	WRITE_FRC_BITS(FRC_ME_H_MVDIFF_EN_1, 1/*me->reg_add_hme_mvdiff_penalty_fs[1]*/, 8, 1);
	WRITE_FRC_BITS(FRC_ME_H_MVDIFF_EN_1, 1/*me->reg_add_hme_mvdiff_penalty_rand[1]*/, 10, 1);
	WRITE_FRC_BITS(FRC_ME_H_MVDIFF_EN_1, 1/*me->reg_add_hme_mvdiff_penalty_zmv[1]*/, 12, 1);
	WRITE_FRC_BITS(FRC_ME_H_MVDIFF_EN_2, 1/*me->reg_add_hme_mvdiff_penalty_fs[2]*/, 8, 1);
	WRITE_FRC_BITS(FRC_ME_H_MVDIFF_EN_2, 1/*me->reg_add_hme_mvdiff_penalty_rand[2]*/, 10, 1);
	WRITE_FRC_BITS(FRC_ME_H_MVDIFF_EN_2, 1/*me->reg_add_hme_mvdiff_penalty_zmv[2]*/, 12, 1);

	//prm_me->reg_hme_mvdiff_penalty_zmv_gain[m] =32
	WRITE_FRC_BITS(FRC_ME_H_MVDIFF_GAIN0_0, 32/*me->reg_hme_mvdiff_penalty_zmv_gain[0]*/, 24, 8);
	WRITE_FRC_BITS(FRC_ME_H_MVDIFF_GAIN0_1, 32/*me->reg_hme_mvdiff_penalty_zmv_gain[1]*/, 24, 8);
	WRITE_FRC_BITS(FRC_ME_H_MVDIFF_GAIN0_2, 32/*me->reg_hme_mvdiff_penalty_zmv_gain[2]*/, 24, 8);

	//prm_me->reg_hme_mvdiff_penalty_fs_rpdb_gain[m] = 0
	//prm_me->reg_hme_mvdiff_penalty_fs_rpd1_gain[m] =32
	WRITE_FRC_BITS(FRC_ME_H_MVDIFF_GAIN1_0, 32/*me->reg_hme_mvdiff_penalty_fs_rpd1_gain[0]*/, 0, 8);
	WRITE_FRC_BITS(FRC_ME_H_MVDIFF_GAIN1_0, 0/*me->reg_hme_mvdiff_penalty_fs_rpdb_gain[0]*/, 16, 8);
	WRITE_FRC_BITS(FRC_ME_H_MVDIFF_GAIN1_1, 32/*me->reg_hme_mvdiff_penalty_fs_rpd1_gain[1]*/, 0, 8);
	WRITE_FRC_BITS(FRC_ME_H_MVDIFF_GAIN1_1, 0/*me->reg_hme_mvdiff_penalty_fs_rpdb_gain[1]*/, 16, 8);
	WRITE_FRC_BITS(FRC_ME_H_MVDIFF_GAIN1_2, 32/*me->reg_hme_mvdiff_penalty_fs_rpd1_gain[2]*/, 0, 8);
	WRITE_FRC_BITS(FRC_ME_H_MVDIFF_GAIN1_2, 0/*me->reg_hme_mvdiff_penalty_fs_rpdb_gain[2]*/, 16, 8);

	//prm_me->reg_acdc_sel_sadac_thrd[m]   = 278
	WRITE_FRC_BITS(FRC_ME_SAD_ACDC_REG0_0, 278/*me->reg_acdc_sel_sadac_thrd[0]*/, 0, 12);
	WRITE_FRC_BITS(FRC_ME_SAD_ACDC_REG1_0, 278/*me->reg_acdc_sel_sadac_thrd[1]*/, 0, 12);
	WRITE_FRC_BITS(FRC_ME_SAD_ACDC_REG2_0, 278/*me->reg_acdc_sel_sadac_thrd[2]*/, 0, 12);

	//prm_hme->reg_me_bv_e2e_ref_en    =   1
	WRITE_FRC_BITS(FRC_ME_E2E_CHK_EN, 1/*me->reg_me_bv_e2e_ref_en*/, 24, 1);

	//prm_hme->reg_smob_rule0_mv_min_diff_th    =  16
	//prm_hme->reg_smob_rule0_mv_max_diff_th    =  32
	WRITE_FRC_BITS(FRC_MEPP_SMOB_R0_TH, 32/*me->reg_smob_rule0_mv_min_diff_th*/, 24, 8);
	WRITE_FRC_BITS(FRC_MEPP_SMOB_R0_TH, 16/*me->reg_smob_rule0_mv_max_diff_th*/, 16, 8);

	//prm_hme->reg_smob_rule0_mv_min_similar_th =  12
	//prm_hme->reg_smob_rule0_mv_max_similar_th =  16
	WRITE_FRC_BITS(FRC_MEPP_SMOB_R0_TH, 12/*me->reg_smob_rule0_mv_min_similar_th*/, 8, 8);
	WRITE_FRC_BITS(FRC_MEPP_SMOB_R0_TH, 16/*me->reg_smob_rule0_mv_max_similar_th*/, 0, 8);

	//prm_vp->reg_vp_mvx_div_mode = 2
	//prm_vp->reg_vp_mvy_div_mode = 2
	WRITE_FRC_BITS(FRC_VP_TOP1, 2/*vp->reg_vp_mvx_div_mode*/, 12, 2);
	WRITE_FRC_BITS(FRC_VP_TOP1, 2/*vp->reg_vp_mvy_div_mode*/, 8, 2);

	//prm_vp->reg_vp_add_blkdiv_ftxy_en = 1
	WRITE_FRC_BITS(FRC_VP_TOP1, 1/*vp->reg_vp_add_blkdiv_ftxy_en*/, 1, 1);

	//prm_vp->reg_cp2pcr_cp2pcrr.reg_dyn_diff_curv[0]   = 5
	//prm_vp->reg_cp2pcr_cp2pcrr.reg_dyn_diff_curv[1]   = 44
	WRITE_FRC_BITS(FRC_VP_CP2PCR_CP2PCRR_DIFF_TH_1, 5, 24, 8);
	WRITE_FRC_BITS(FRC_VP_CP2PCR_CP2PCRR_DIFF_TH_2, 44, 0, 8);

	//prm_vp->reg_cn2ncr_cn2ncrr.reg_dyn_diff_curv[0]   = 5
	//prm_vp->reg_cn2ncr_cn2ncrr.reg_dyn_diff_curv[1]   = 44
	WRITE_FRC_BITS(FRC_VP_CN2NCR_CN2NCRR_DIFF_TH_1, 5, 24, 8);
	WRITE_FRC_BITS(FRC_VP_CN2NCR_CN2NCRR_DIFF_TH_2, 44, 0, 8);

	//prm_vp->reg_cprev_cp2pcr.reg_dyn_diff_curv[0]   = 5
	//prm_vp->reg_cprev_cp2pcr.reg_dyn_diff_curv[1]   = 44
	WRITE_FRC_BITS(FRC_VP_CPREV_CP2PCR_DIFF_TH_1, 5, 24, 8);
	WRITE_FRC_BITS(FRC_VP_CPREV_CP2PCR_DIFF_TH_2, 44, 0, 8);

	//prm_vp->reg_cnrev_cn2ncr.reg_dyn_diff_curv[0]   = 5
	//prm_vp->reg_cnrev_cn2ncr.reg_dyn_diff_curv[1]   = 44
	WRITE_FRC_BITS(FRC_VP_CNREV_CN2NCR_DIFF_TH_1, 5, 24, 8);
	WRITE_FRC_BITS(FRC_VP_CNREV_CN2NCR_DIFF_TH_2, 44, 0, 8);

	//prm_vp->reg_cpext_cp2pcr.reg_dyn_diff_curv[0]   = 5
	//prm_vp->reg_cpext_cp2pcr.reg_dyn_diff_curv[1]   = 44
	WRITE_FRC_BITS(FRC_VP_CPEXT_CP2PCR_DIFF_TH_1, 5, 24, 8);
	WRITE_FRC_BITS(FRC_VP_CPEXT_CP2PCR_DIFF_TH_2, 44, 0, 8);

	//prm_vp->reg_cnext_cn2ncr.reg_dyn_diff_curv[0]   = 5
	//prm_vp->reg_cnext_cn2ncr.reg_dyn_diff_curv[1]   = 44
	WRITE_FRC_BITS(FRC_VP_CNEXT_CN2NCR_DIFF_TH_1, 5, 24, 8);
	WRITE_FRC_BITS(FRC_VP_CNEXT_CN2NCR_DIFF_TH_2, 44, 0, 8);

	//prm_vp->reg_cp_cn2ncr.reg_dyn_diff_curv[0]   = 5
	//prm_vp->reg_cp_cn2ncr.reg_dyn_diff_curv[1]   = 44
	WRITE_FRC_BITS(FRC_VP_CP_CN2NCR_DIFF_TH_1, 5, 24, 8);
	WRITE_FRC_BITS(FRC_VP_CP_CN2NCR_DIFF_TH_2, 44, 0, 8);

	//prm_vp->reg_cn_cp2pcr.reg_dyn_diff_curv[0]   = 5
	//prm_vp->reg_cn_cp2pcr.reg_dyn_diff_curv[1]   = 44
	WRITE_FRC_BITS(FRC_VP_CN_CP2PCR_DIFF_TH_1, 5, 24, 8);
	WRITE_FRC_BITS(FRC_VP_CN_CP2PCR_DIFF_TH_2, 44, 0, 8);

	//prm_vp->reg_cn_cp.reg_dyn_diff_curv[0]   = 5
	//prm_vp->reg_cn_cp.reg_dyn_diff_curv[1]   = 44
	WRITE_FRC_BITS(FRC_VP_CN_CP_DIFF_TH_1, 5, 24, 8);
	WRITE_FRC_BITS(FRC_VP_CN_CP_DIFF_TH_2, 44, 0, 8);

	//prm_vp->reg_cn_cn2pcr.reg_dyn_diff_curv[0]   = 5
	//prm_vp->reg_cn_cn2pcr.reg_dyn_diff_curv[1]   = 44
	WRITE_FRC_BITS(FRC_VP_CN_CN2PCR_DIFF_TH_1, 5, 24, 8);
	WRITE_FRC_BITS(FRC_VP_CN_CN2PCR_DIFF_TH_2, 44, 0, 8);

	//prm_vp->reg_cn_cn2ncrr.reg_dyn_diff_curv[0]   = 5
	///prm_vp->reg_cn_cn2ncrr.reg_dyn_diff_curv[1]   = 44
	WRITE_FRC_BITS(FRC_VP_CN_CN2NCRR_DIFF_TH_1, 5, 24, 8);
	WRITE_FRC_BITS(FRC_VP_CN_CN2NCRR_DIFF_TH_2, 44, 0, 8);

	//prm_vp->reg_cp_cp2ncr.reg_dyn_diff_curv[0]   = 5
	//prm_vp->reg_cp_cp2ncr.reg_dyn_diff_curv[1]   = 44
	WRITE_FRC_BITS(FRC_VP_CP_CP2NCR_DIFF_TH_1, 5, 24, 8);
	WRITE_FRC_BITS(FRC_VP_CP_CP2NCR_DIFF_TH_2, 44, 0, 8);

	//prm_vp->reg_cp_cp2pcrr.reg_dyn_diff_curv[0]   = 5
	//prm_vp->reg_cp_cp2pcrr.reg_dyn_diff_curv[1]   = 44
	WRITE_FRC_BITS(FRC_VP_CP_CP2PCRR_DIFF_TH_1, 5, 24, 8);
	WRITE_FRC_BITS(FRC_VP_CP_CP2PCRR_DIFF_TH_2, 44, 0, 8);

	//prm_vp->reg_cp2pcr_cp2pcrr_loc_chk_dir_th[0]    = 6
	//prm_vp->reg_cp2pcr_cp2pcrr_loc_chk_dir_th[1]    = 6
	WRITE_FRC_BITS(FRC_VP_CP2PCR_CP2PCRR_DIR_TYPE, 5, 24, 8);
	WRITE_FRC_BITS(FRC_VP_CP2PCR_CP2PCRR_DIR_TYPE, 44, 16, 8);

	//prm_vp->reg_cn2ncr_cn2ncrr_loc_chk_dir_th[0]    = 6
	//prm_vp->reg_cn2ncr_cn2ncrr_loc_chk_dir_th[1]    = 6
	WRITE_FRC_BITS(FRC_VP_CN2NCR_CN2NCRR_DIR_TYPE, 5, 24, 8);
	WRITE_FRC_BITS(FRC_VP_CN2NCR_CN2NCRR_DIR_TYPE, 44, 16, 8);
	WRITE_FRC_BITS(FRC_VP_SAD_TH, 40/*vp->reg_vp_sad_chk_b_th*/, 8, 8);
	WRITE_FRC_BITS(FRC_VP_SAD_TH, 20/*vp->reg_vp_sad_chk_s_th*/, 16, 8);

	WRITE_FRC_BITS(FRC_VP_RETIMER_ENABLE, 0/*vp->reg_vp_cross_rule_en*/, 1, 1);
	WRITE_FRC_BITS(FRC_VP_RETIMER_ENABLE, 0/*vp->reg_vp_dont_care_gmv*/, 7, 1);

	WRITE_FRC_BITS(FRC_VP_SHIFT_BLK, 20/*vp->reg_retimer_diff_gmv_invalid_th*/, 14, 8);

	WRITE_FRC_BITS(FRC_VP_SHIFT_BLK, 1/*vp->reg_retimer_sft_x_range*/, 10, 2);
	WRITE_FRC_BITS(FRC_VP_SHIFT_BLK, 1/*vp->reg_retimer_sft_y_range*/, 8, 2);

	WRITE_FRC_BITS(FRC_VP_SHIFT_BLK, 1/*vp->reg_retimer_sft_CN2NCr_en*/, 3, 1);
	WRITE_FRC_BITS(FRC_VP_SHIFT_BLK, 1/*vp->reg_retimer_sft_CP2PCr_en*/, 2, 1);

	WRITE_FRC_BITS(FRC_VP_DEHALO_TOP, 20/*vp->reg_phs_pre_adp5_mvdiff_th*/, 16, 8);
	WRITE_FRC_BITS(FRC_VP_DEHALO_TOP, 20/*vp->reg_uni_pre_adp5_mvdiff_th*/, 24, 8);

	WRITE_FRC_BITS(FRC_VP_DEHALO_TOP, 1/*vp->reg_uni_pre_med5_enable*/, 4, 2);

	WRITE_FRC_BITS(FRC_VP_DEHALO_TOP, 0/*vp->reg_phr_pst_med5_enable*/, 0, 2);
	WRITE_FRC_BITS(FRC_VP_EXTEND_ALPHA_CPR_PCR, 0/*vp->reg_extend_alpha_s1.reg_s_th*/, 24, 8);
	WRITE_FRC_BITS(FRC_VP_EXTEND_ALPHA_CPR_PCR, 32/*vp->reg_extend_alpha_s1.reg_b_th*/, 16, 8);

	WRITE_FRC_BITS(FRC_VP_EXTEND_ALPHA_CPR_PCR, 24/*vp->reg_extend_alpha_s1.reg_gain*/, 8, 8);
	WRITE_FRC_BITS(FRC_VP_EXTEND_ALPHA_CPR_PCR, 4/*vp->reg_extend_alpha_s1.reg_mv_th0*/, 1, 7);

	WRITE_FRC_BITS(FRC_VP_EXTEND_ALPHA_CPRR_PCRR, 0/*vp->reg_extend_alpha_s2.reg_s_th*/, 24, 8);
	WRITE_FRC_BITS(FRC_VP_EXTEND_ALPHA_CPRR_PCRR, 32/*vp->reg_extend_alpha_s2.reg_b_th*/, 16, 8);
	WRITE_FRC_BITS(FRC_VP_EXTEND_ALPHA_CPRR_PCRR, 24/*vp->reg_extend_alpha_s2.reg_gain*/, 8, 8);
	WRITE_FRC_BITS(FRC_VP_EXTEND_ALPHA_CPRR_PCRR, 4/*vp->reg_extend_alpha_s2.reg_mv_th0*/, 1, 7);

	WRITE_FRC_BITS(FRC_VP_EXTEND_ALPHA_2PCR_2CPR, 0/*vp->reg_extend_alpha_s3.reg_s_th*/, 24, 8);
	WRITE_FRC_BITS(FRC_VP_EXTEND_ALPHA_2PCR_2CPR, 80/*vp->reg_extend_alpha_s3.reg_b_th*/, 16, 8);
	WRITE_FRC_BITS(FRC_VP_EXTEND_ALPHA_2PCR_2CPR, 16/*vp->reg_extend_alpha_s3.reg_gain*/, 8, 8);
	WRITE_FRC_BITS(FRC_VP_EXTEND_ALPHA_2PCR_2CPR, 4/*vp->reg_extend_alpha_s3.reg_mv_th0*/, 1, 7);

	WRITE_FRC_BITS(FRC_VP_EXTEND_ALPHA_4PBR_4CNR, 0/*vp->reg_extend_alpha_s4.reg_s_th*/, 24, 8);
	//WRITE_FRC_BITS(FRC_VP_EXTEND_ALPHA_4PBR_4CNR, /*vp->reg_extend_alpha_s4.reg_b_th*/, 16, 8);
	WRITE_FRC_BITS(FRC_VP_EXTEND_ALPHA_4PBR_4CNR, 32/*vp->reg_extend_alpha_s4.reg_gain*/, 8, 8);
	WRITE_FRC_BITS(FRC_VP_EXTEND_ALPHA_4PBR_4CNR, 6/*vp->reg_extend_alpha_s4.reg_mv_th0*/, 7, 7);
	WRITE_FRC_BITS(FRC_VP_EXTEND_ALPHA_4PBR_4CNR, 1/*vp->reg_extend_alpha_s4.reg_enable*/, 0, 1);

	//prm_vp->reg_extend_alpha_s5.reg_s_th   = 0
	//prm_vp->reg_extend_alpha_s5.reg_b_th   = 60
	WRITE_FRC_BITS(FRC_VP_EXTEND_ALPHA_AVG4PBR_AVG4CNR, 0, 24, 8);
	WRITE_FRC_BITS(FRC_VP_EXTEND_ALPHA_AVG4PBR_AVG4CNR, 60, 16, 8);

	WRITE_FRC_BITS(FRC_VP_EXTEND_LIMIT, 100/*vp->reg_extend_limit_s4*/, 16, 8);

	WRITE_FRC_BITS(FRC_VP_DEHALO_DALPHA, 0/*vp->reg_dehalo_cpr2pcr_pcr2cpr_sel_mode*/, 1, 1);

	WRITE_FRC_BITS(FRC_VP_DEHALO_DYN_TH, 2/*vp->reg_phs_th.reg_dyn_diff_curv[0]*/, 16, 8);
	WRITE_FRC_BITS(FRC_VP_DEHALO_DYN_TH, 44/*vp->reg_phs_th.reg_dyn_diff_curv[1]*/, 24, 8);

	WRITE_FRC_BITS(FRC_VP_DEHALO_DYN_TH, 8/*vp->reg_phs_th.reg_dyn_b_th_gain*/, 12, 4);

	WRITE_FRC_BITS(FRC_VP_DEHALO_DIR_TYPE, 6/*vp->reg_phs_loc_chk_dir_th[0]*/, 24, 8);
	WRITE_FRC_BITS(FRC_VP_DEHALO_DIR_TYPE, 6/*vp->reg_phs_loc_chk_dir_th[1]*/, 16, 8);

	WRITE_FRC_BITS(FRC_VP_DOUBLE_FG, 30/*vp->reg_phs_limit_occl_fgbg_th*/, 8, 8);
	WRITE_FRC_BITS(FRC_VP_DOUBLE_FG, 0/*vp->reg_phs_limit_occl_track_en*/, 0, 1);

	WRITE_FRC_BITS(FRC_VP_MV_CHECK1, 0/*vp->reg_phs_limit_occl_mvchk_en*/, 0, 1);
	WRITE_FRC_BITS(FRC_VP_MV_CHECK2, 8/*vp->reg_phs_inact_thx*/, 8, 8);
	WRITE_FRC_BITS(FRC_VP_MV_CHECK2, 8/*vp->reg_phs_inact_thy*/, 0, 8);

	WRITE_FRC_BITS(FRC_VP_EDGE_CHECK, 0/*vp->reg_phs_edge_chk_fg_mv_length_th*/, 24, 8);
	WRITE_FRC_BITS(FRC_VP_EDGE_CHECK, 0/*vp->reg_phs_edge_chk_uncov_en[3]*/, 7, 1);
	WRITE_FRC_BITS(FRC_VP_EDGE_CHECK, 0/*vp->reg_phs_edge_chk_cover_en[3]*/, 3, 1);
	WRITE_FRC_BITS(FRC_VP_EDGE_CHECK, 0/*vp->reg_phs_edge_detail_th*/, 8, 4);


	//prm_vp->reg_fg_mv_length_th =16
	WRITE_FRC_BITS(FRC_VP_BGMV_CHECK, 16, 0, 8);

	//prm_vp->reg_dehalo_bgmv_chk_en  = 1
	WRITE_FRC_BITS(FRC_VP_DEHALO_RULE_EN, 1, 1, 1);

	//prm_vp->reg_cover_use_close_mv = 1
	//prm_vp->reg_uncov_use_close_mv  = 1
	WRITE_FRC_BITS(FRC_VP_GEN_OCCL_MV, 1, 1, 1);
	WRITE_FRC_BITS(FRC_VP_GEN_OCCL_MV, 1, 0, 1);

	///prm_vp->reg_dehalo_phs_pre_b_th            = 10
	//prm_vp->reg_dehalo_phs_cur_b_th            = 10
	WRITE_FRC_BITS(FRC_VP_GEN_TYPE_RPL_PHS_MV_1, 10, 16, 8);
	WRITE_FRC_BITS(FRC_VP_GEN_TYPE_RPL_PHS_MV_1, 10, 24, 8);

	///prm_vp->reg_dehalo_cover_ex_s_th           = 4
	//prm_vp->reg_dehalo_uncov_ex_s_th           = 4
	WRITE_FRC_BITS(FRC_VP_GEN_TYPE_RPL_PHS_MV_2, 4, 16, 8);
	WRITE_FRC_BITS(FRC_VP_GEN_TYPE_RPL_PHS_MV_2, 4, 8, 8);

	//prm_vp->reg_dehalo_pre_nex_bg_s_th         = 4
	WRITE_FRC_BITS(FRC_VP_GEN_TYPE_RPL_PHS_MV_2, 4, 0, 8);

	//prm_vp->reg_dehalo_fhri_hole_b_th          = 5
	//prm_vp->reg_dehalo_fhri_invs_s_th          = 2
	WRITE_FRC_BITS(FRC_VP_DEHALO_FHRI, 5, 16, 4);
	WRITE_FRC_BITS(FRC_VP_DEHALO_FHRI, 2, 4, 4);

	//prm_vp->reg_phs_r_gmv_enable                 = 0
	WRITE_FRC_BITS(FRC_VP_DEHALO_PHS_R, 0, 8, 1);

	//prm_vp->reg_type_rplc_cover_mode            = 3
	//prm_vp->reg_type_rplc_uncov_mode            = 3
	WRITE_FRC_BITS(FRC_VP_DEHALO_PHS_R, 3, 2, 2);
	WRITE_FRC_BITS(FRC_VP_DEHALO_PHS_R, 3, 0, 2);

	//prm_vp->reg_dehalo_var_cppc_slope          = 4
	//prm_vp->reg_dehalo_var_phs_slope           = 6
	WRITE_FRC_BITS(FRC_VP_DEHALO_MV_VAR, 4, 8, 4);
	WRITE_FRC_BITS(FRC_VP_DEHALO_MV_VAR, 4, 4, 4);

	//prm_vp->reg_dehalo_occl_lpf_en             = 1
	WRITE_FRC_BITS(FRC_VP_DEHALO_OCCL_LPF, 1, 0, 1);

	//prm_vp->reg_dehalo_oct2_rpl_phs_mv_chk_en  = 1
	WRITE_FRC_BITS(FRC_VP_DEHALO_OCT2_1, 1, 25, 1);

	//prm_vp->reg_dehalo_vector2_use_gmv_sel    = 1
	WRITE_FRC_BITS(FRC_VP_DEHALO_OCT2_1, 1, 24, 1);

	//prm_vp->reg_dehalo_vector2_none_occ_use_gmv= 0
	WRITE_FRC_BITS(FRC_VP_DEHALO_OCT2_1, 0, 1, 1);

	//prm_vp->reg_dehalo_oct2_bg_mv_length_th  = 8
	WRITE_FRC_BITS(FRC_VP_DEHALO_OCT2_2, 8, 16, 8);

	//prm_vp->reg_dehalo_oct2_diff_v1_v2_chk_en = 1
	WRITE_FRC_BITS(FRC_VP_DEHALO_OCT2_2, 1, 2, 1);

	//m_vp->reg_vp_rp_sobj_win_x_half= 2;
	//m_vp->reg_vp_rp_sobj_win_y_half= 2;
	WRITE_FRC_BITS(FRC_VP_DEHALO_TURN_OFF, 2, 22, 2);
	WRITE_FRC_BITS(FRC_VP_DEHALO_TURN_OFF, 2, 20, 2);

	//m_vp->reg_vp_sobj_rpd0_cnt_th= 1;
	WRITE_FRC_BITS(FRC_VP_DEHALO_TURN_OFF, 1, 16, 4);

	//m_vp->reg_vp_sobj_ball_cnt_th= 1;
	WRITE_FRC_BITS(FRC_VP_DEHALO_TURN_OFF, 1, 12, 4);

	//m_vp->reg_vp_rp_cnt_th= 1;
	WRITE_FRC_BITS(FRC_VP_DEHALO_TURN_OFF, 1, 8, 4);

	//m_vp->reg_vp_sobj_disable    = 1
	WRITE_FRC_BITS(FRC_VP_DEHALO_TURN_OFF, 1, 1, 1);

	//m_vp->reg_vp_logo_disable    = 1
	WRITE_FRC_BITS(FRC_VP_DEHALO_TURN_OFF, 1, 0, 1);

	//m_vp->reg_vp_ip_blklogo_mode   = 2
	WRITE_FRC_BITS(FRC_VP_DEHALO_TURN_OFF_2, 2, 8, 2);

	//prm_vp->reg_vplogo_mvchk_en   = 0
	WRITE_FRC_BITS(FRC_VP_LOGO_MV_CHECK_1, 0, 0, 1);

	//prm_mc->reg_mc_bb_inner_en = 1
	WRITE_FRC_BITS(FRC_MC_SETTING1, 1, 24, 1);

	//prm_mc->reg_mc_mvx_scale = 1
	//prm_mc->reg_mc_mvy_scale = 1
	WRITE_FRC_BITS(FRC_MC_SETTING1  , 1, 8, 4);
	WRITE_FRC_BITS(FRC_MC_SETTING1  , 1, 0, 4);

	//prm_mc->reg_mc_luma_norm_shift_posi[0] = 43
	///prm_mc->reg_mc_luma_norm_shift_posi[1] = 86
	//prm_mc->reg_mc_luma_norm_shift_posi[2] = 129
	//prm_mc->reg_mc_luma_norm_shift_posi[3] = 0

	WRITE_FRC_BITS(FRC_NORM_SHIFT1, 43, 24, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT1, 86, 16, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT1, 129, 8, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT1, 0, 0, 8);

	//prm_mc->reg_mc_luma_norm_shift_posi[4] = 0
	//prm_mc->reg_mc_luma_norm_shift_posi[5] = 0
	//prm_mc->reg_mc_luma_norm_shift_posi[6] = 0
	//prm_mc->reg_mc_luma_norm_shift_posi[7] = 0
	WRITE_FRC_BITS(FRC_NORM_SHIFT2, 0, 24, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT2, 0, 16, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT2, 0, 8, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT2, 0, 0, 8);

	//prm_mc->reg_mc_luma_norm_shift_posi[8] = 0
	//prm_mc->reg_mc_chrm_norm_shift_posi[0] = 43
	//prm_mc->reg_mc_luma_norm_shift_posi[1] = 86
	//prm_mc->reg_mc_luma_norm_shift_posi[2] = 129
	WRITE_FRC_BITS(FRC_NORM_SHIFT3, 0, 24, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT3, 43, 16, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT3, 86, 8, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT3, 129, 0, 8);

	//prm_mc->reg_mc_chrm_norm_shift_posi[3] = 0
	//prm_mc->reg_mc_luma_norm_shift_posi[4] = 0
	//prm_mc->reg_mc_luma_norm_shift_posi[5] = 0
	//prm_mc->reg_mc_luma_norm_shift_posi[6] = 0
	WRITE_FRC_BITS(FRC_NORM_SHIFT4, 0, 24, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT4, 0, 16, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT4, 0, 8, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT4, 0, 0, 8);

	//prm_mc->reg_mc_chrm_norm_shift_posi[7] = 0
	//prm_mc->reg_mc_luma_norm_shift_posi[8] = 0
	//prm_mc->reg_mc_luma_sing_shift_posi[0] = 26
	//prm_mc->reg_mc_luma_sing_shift_posi[1] = 52
	WRITE_FRC_BITS(FRC_NORM_SHIFT5, 0, 24, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT5, 0, 16, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT5, 26, 8, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT5, 52, 0, 8);

	//prm_mc->reg_mc_luma_sing_shift_posi[2] = 77;
	//prm_mc->reg_mc_luma_sing_shift_posi[3] = 103;
	//prm_mc->reg_mc_luma_sing_shift_posi[4] = 129;
	//prm_mc->reg_mc_luma_sing_shift_posi[5] = 0;
	//prm_mc->reg_mc_luma_sing_shift_posi[6] = 0;
	//prm_mc->reg_mc_luma_sing_shift_posi[7] = 0;
	//prm_mc->reg_mc_luma_sing_shift_posi[8] = 0;
	WRITE_FRC_BITS(FRC_NORM_SHIFT6, 77, 24, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT6, 103, 16, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT6, 129, 8, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT6, 0, 0, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT7, 0, 24, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT7, 0, 16, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT7, 0, 8, 8);

	//prm_mc->reg_mc_chrm_sing_shift_posi[0] = 26;
	//prm_mc->reg_mc_chrm_sing_shift_posi[1] = 52;
	//prm_mc->reg_mc_chrm_sing_shift_posi[2] = 77;
	//prm_mc->reg_mc_chrm_sing_shift_posi[3] = 103;
	//prm_mc->reg_mc_chrm_sing_shift_posi[4] = 129;

	WRITE_FRC_BITS(FRC_NORM_SHIFT7, 26, 0, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT8, 52, 24, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT8, 77, 16, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT8, 103, 8, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT8, 129, 0, 8);

	//prm_mc->reg_mc_chrm_sing_shift_posi[5] = 0;
	//prm_mc->reg_mc_chrm_sing_shift_posi[6] = 0;
	//prm_mc->reg_mc_chrm_sing_shift_posi[7] = 0;
	//prm_mc->reg_mc_chrm_sing_shift_posi[8] = 0;
	WRITE_FRC_BITS(FRC_NORM_SHIFT9, 0, 24, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT9, 0, 16, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT9, 0, 8, 8);
	WRITE_FRC_BITS(FRC_NORM_SHIFT9, 0, 0, 8);

	//prm_mc->reg_mc_blk_dehalo_rp_min_th_y = 60;
	WRITE_FRC_BITS(FRC_MC_BLK_DEHALO_EN_AND_RP_TH_Y, 60, 0, 12);

	//prm_mc->reg_mc_blk_dehalo_cp_level_y_slope0 = 8;
	//prm_mc->reg_mc_blk_dehalo_cp_level_y_slope1 = 8;
	//prm_mc->reg_mc_blk_dehalo_cp_level_y_slope2 = 8;
	WRITE_FRC_BITS(FRC_MC_BLK_DEHALO_CP_LEVEL_Y_SLOPE, 8, 16, 8);
	WRITE_FRC_BITS(FRC_MC_BLK_DEHALO_CP_LEVEL_Y_SLOPE, 8, 8, 8);
	WRITE_FRC_BITS(FRC_MC_BLK_DEHALO_CP_LEVEL_Y_SLOPE, 8, 0, 8);

	//prm_mc->reg_mc_blk_dehalo_cp_level_uv_slope0 = 8;
	//prm_mc->reg_mc_blk_dehalo_cp_level_uv_slope1 = 8;
	//prm_mc->reg_mc_blk_dehalo_cp_level_uv_slope2 = 8;
	WRITE_FRC_BITS(FRC_MC_BLK_DEHALO_CP_LEVEL_UV_SLOPE, 8, 16, 8);
	WRITE_FRC_BITS(FRC_MC_BLK_DEHALO_CP_LEVEL_UV_SLOPE, 8, 8, 8);
	WRITE_FRC_BITS(FRC_MC_BLK_DEHALO_CP_LEVEL_UV_SLOPE, 8, 0, 8);

	//prm_mc->reg_mc_blk_dehalo_cp_y_gain_th0 = 80;
	//prm_mc->reg_mc_blk_dehalo_cp_y_gain_th1 = 80;
	WRITE_FRC_BITS(FRC_MC_BLK_DEHALO_CP_Y_GAIN_TH, 80, 16, 12);
	WRITE_FRC_BITS(FRC_MC_BLK_DEHALO_CP_Y_GAIN_TH, 80, 0, 12);

	//prm_mc->reg_mc_blk_dehalo_cp_y_gain_slope = 4;
	WRITE_FRC_BITS(FRC_MC_BLK_DEHALO_CP_Y_GAIN_SLOPE_MAX_MIN, 4, 16, 8);

	//prm_mc->reg_mc_blk_dehalo_cp_y_gain_max = 128;
	//prm_mc->reg_mc_blk_dehalo_cp_y_gain_min = 128;
	WRITE_FRC_BITS(FRC_MC_BLK_DEHALO_CP_Y_GAIN_SLOPE_MAX_MIN, 128, 8, 8);
	WRITE_FRC_BITS(FRC_MC_BLK_DEHALO_CP_Y_GAIN_SLOPE_MAX_MIN, 128, 0, 8);

	//prm_mc->reg_mc_blk_dehalo_cp_uv_gain_th0 = 80;
	//prm_mc->reg_mc_blk_dehalo_cp_uv_gain_th1 = 80;
	WRITE_FRC_BITS(FRC_MC_BLK_DEHALO_CP_UV_GAIN_TH, 80, 16, 12);
	WRITE_FRC_BITS(FRC_MC_BLK_DEHALO_CP_UV_GAIN_TH, 80, 0, 12);

	//prm_mc->reg_mc_blk_dehalo_cp_uv_gain_slope = 2;
	WRITE_FRC_BITS(FRC_MC_BLK_DEHALO_CP_UV_GAIN_SLOP_MAX_MIN, 2, 16, 8);

	//prm_mc->reg_mc_blk_dehalo_cp_uv_gain_max = 128;
	//prm_mc->reg_mc_blk_dehalo_cp_uv_gain_min = 128;
	WRITE_FRC_BITS(FRC_MC_BLK_DEHALO_CP_UV_GAIN_SLOP_MAX_MIN, 128, 8, 8);
	WRITE_FRC_BITS(FRC_MC_BLK_DEHALO_CP_UV_GAIN_SLOP_MAX_MIN, 128, 0, 8);

	//prm_mc->reg_mc_pix_dehalo_rp_max_th_y = 185;
	WRITE_FRC_BITS(FRC_MC_PIX_DEHALO_SETTING, 185, 16, 12);

	//prm_mc->reg_mc_pix_dehalo_cp_level_y_slope0 = 6;
	//prm_mc->reg_mc_pix_dehalo_cp_level_y_slope1 = 6;
	WRITE_FRC_BITS(FRC_MC_PIX_DEHALO_CP_LEVEL_Y_SLOPE, 6, 16, 8);
	WRITE_FRC_BITS(FRC_MC_PIX_DEHALO_CP_LEVEL_Y_SLOPE, 6, 8, 8);

	//prm_mc->reg_mc_pix_dehalo_cp_uv2y_gain_th0 = 30;
	//prm_mc->reg_mc_pix_dehalo_cp_uv2y_gain_th1 = 90;
	WRITE_FRC_BITS(FRC_MC_PIX_DEHALO_CP_UV2Y_GAIN_TH, 30, 16, 12);
	WRITE_FRC_BITS(FRC_MC_PIX_DEHALO_CP_UV2Y_GAIN_TH, 90, 0, 12);

	//prm_mc->reg_mc_pix_dehalo_cp_uv2y_gain_slope = 12;
	//prm_mc->reg_mc_pix_dehalo_cp_uv2y_gain_min = 0;
	WRITE_FRC_BITS(FRC_MC_PIX_DEHALO_CP_YV2Y_GAIN, 12, 24, 8);
	WRITE_FRC_BITS(FRC_MC_PIX_DEHALO_CP_YV2Y_GAIN, 0, 0, 12);

	//prm_mc->reg_mc_pix_dehalo_rp_max_th_uv = 50;
	//prm_mc->reg_mc_pix_dehalo_rp_min_th_uv = 40;
	WRITE_FRC_BITS(FRC_MC_PIX_DEHALO_RP_TH_UV_MAX_MIN, 50, 16, 12);
	WRITE_FRC_BITS(FRC_MC_PIX_DEHALO_RP_TH_UV_MAX_MIN, 40, 0, 12);

	//prm_mc->reg_mc_pix_dehalo_diff_uv_mode = 0;
	WRITE_FRC_BITS(FRC_MC_PIX_DEHALO_CP_LEVEL_UV, 0, 30, 2);

	//prm_mc->reg_mc_pix_dehalo_cp_y2uv_gain_slope = 12;
	//prm_mc->reg_mc_pix_dehalo_cp_y2uv_gain_min = 0;
	WRITE_FRC_BITS(FRC_MC_PIX_DEHALO_CP_Y2UV_GIAN, 12, 24, 8);
	WRITE_FRC_BITS(FRC_MC_PIX_DEHALO_CP_YV2Y_GAIN, 0, 0, 12);

	//prm_mc->reg_mc_flicker0_mode = 1;
	WRITE_FRC_BITS(FRC_MC_DEFLICKER, 1, 13, 1);

	//prm_mc->reg_mc_variance_blend_en = 1;
	WRITE_FRC_BITS(FRC_MC_PT_EN, 1, 3, 1);

	//prm_mc->reg_mc_ptb_rp_bet_inout_single_en=0;
	WRITE_FRC_BITS(FRC_MC_PT_EN, 0, 0, 1);

	//prm_mc->reg_mc_dbg_med3_outer_en = 1;
	//prm_mc->reg_mc_dbg_med3_pt_en = 1;
	WRITE_FRC_BITS(FRC_MC_ME3_PT, 1, 24, 1);
	WRITE_FRC_BITS(FRC_MC_ME3_PT, 1, 23, 1);

	//prm_mc->reg_mc_pt_flag_sel_mode = 4;
	WRITE_FRC_BITS(FRC_MC_ME3_PT, 4, 16, 4);

	//prm_mc->reg_mc_ptb_outer_num_thmin = 2
	WRITE_FRC_BITS(FRC_MC_PTBWL_SETTING, 2, 20, 2);

	//prm_mc->reg_mc_var_on_en = 0
	//prm_mc->reg_mc_var_w_th = 20
	WRITE_FRC_BITS(FRC_MC_VAR_SETTING1, 0, 16, 1);
	WRITE_FRC_BITS(FRC_MC_VAR_SETTING1, 20, 0, 8);

	//prm_mc->reg_mc_var_dif_a0 = 2
	WRITE_FRC_BITS(FRC_MC_VAR_DIF_A, 2, 24, 4);

	//prm_mc->reg_mc_h8v4_sel_mv_mode = 1
	WRITE_FRC_BITS(FRC_MC_DBG_H8V4_SEL_EN, 1, 0, 1);

	//prm_melogo->reg_melogo_smv_pc_enable    = 1;
	//prm_melogo->reg_melogo_smv_cp_enable    = 1;
	WRITE_FRC_BITS(FRC_MELOGO_SMV_HPAN_XY_THD, 1, 29, 1);
	WRITE_FRC_BITS(FRC_MELOGO_SMV_HPAN_XY_THD, 1, 28, 1);

	//prm_melogo->reg_melogo_smv_sad_thd = 250;
	WRITE_FRC_BITS(FRC_MELOGO_BMV_XY_THD, 250, 24, 8);

	//prm_bb->reg_bb_ver_th1 = 4
	//prm_bb->reg_bb_ver_th2 = 299
	WRITE_FRC_BITS(FRC_BBD_VER_TH, 4, 16, 16);
	WRITE_FRC_BITS(FRC_BBD_VER_TH, 299, 0, 16);

	//prm_bb->reg_bb_rough_hor_th1 = 95;
	//prm_bb->reg_bb_rough_hor_th2 = 265;
	WRITE_FRC_BITS(FRC_BBD_ROUGH_HOR_TH, 95, 16, 16);
	WRITE_FRC_BITS(FRC_BBD_ROUGH_HOR_TH, 265, 0, 16);

	//prm_bb->reg_bb_finer_hor_th1 = 47;
	//prm_bb->reg_bb_finer_hor_th2 = 95;
	WRITE_FRC_BITS(FRC_BBD_ROUGH_HOR_TH, 47, 16, 16);
	WRITE_FRC_BITS(FRC_BBD_ROUGH_HOR_TH, 95, 0, 16);

	//prm_bb->reg_bb_adv_rough_rit_posi1 = 1008;
	//prm_bb->reg_bb_adv_rough_rit_posi2 = 1008;
	WRITE_FRC_BITS(FRC_BBD_ADV_ROUGH_RIT_POSI, 1008, 16, 16);
	WRITE_FRC_BITS(FRC_BBD_ADV_ROUGH_RIT_POSI, 1008, 0, 16);

	//prm_bb->reg_bb_top_bot_edge_th1 = 205;
	//prm_bb->reg_bb_top_bot_edge_th2 = 256;
	WRITE_FRC_BITS(FRC_BBD_TOP_BOT_EDGE_TH, 205, 16, 16);
	WRITE_FRC_BITS(FRC_BBD_TOP_BOT_EDGE_TH, 256, 0, 16);

	//prm_bb->reg_bb_lft_rit_edge_th1 = 102;
	//prm_bb->reg_bb_lft_rit_edge_th2 = 135;
	WRITE_FRC_BITS(FRC_BBD_LFT_RIT_EDGE_TH, 102, 16, 16);
	WRITE_FRC_BITS(FRC_BBD_LFT_RIT_EDGE_TH, 135, 0, 16);

	//prm_bb->reg_bb_adv_rough_rit_motion_posi1 = 448;
	//prm_bb->reg_bb_adv_rough_rit_motion_posi2 = 448;
	WRITE_FRC_BITS(FRC_BBD_ADV_ROUGH_RIT_MOTION_POSI, 448, 16, 16);
	WRITE_FRC_BITS(FRC_BBD_ADV_ROUGH_RIT_MOTION_POSI, 448, 0, 16);

	for (i = 0; i < 36; i++) {
		WRITE_FRC_BITS(FRC_MC_RANGE_NORM_LUT_0 + i, mc_range_norm_lut[i] & 0x1ff, 16, 9);
		WRITE_FRC_BITS(FRC_MC_RANGE_NORM_LUT_0 + i, mc_range_norm_lut[i] & 0x1ff, 0, 9);
	}

	for (i = 0; i < 36; i++) {
		WRITE_FRC_BITS(FRC_MC_RANGE_SING_LUT_0 + i, mc_range_sing_lut[i] & 0x1ff, 16, 9);
		WRITE_FRC_BITS(FRC_MC_RANGE_SING_LUT_0 + i, mc_range_sing_lut[i] & 0x1ff, 0, 9);
	}
	pr_frc(1, "%s - end\n", __func__);
}


