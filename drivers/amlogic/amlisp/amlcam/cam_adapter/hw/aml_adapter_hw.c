/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2020 Amlogic or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#define pr_fmt(fmt)  "aml-adap:%s:%d: " fmt, __func__, __LINE__

#include <linux/io.h>
#include <linux/delay.h>
#include <asm/types.h>

#include "../aml_adapter.h"
#include "aml_adapter_hw.h"
#include "aml_misc.h"

static int ceil_upper(int val, int mod)
{
	int ret = -1;

	if ((val == 0) || (mod == 0)) {
		pr_err("Error input a invalid value\n");
		return ret;
	}

	ret = val % mod ? ((val / mod) + 1) : (val / mod);

	return ret;
}

static void __iomem *module_get_base(void *a_dev, int module)
{
	void __iomem *m_base = NULL;
	struct adapter_dev_t *adap_dev = a_dev;

	switch (module) {
	case FRONTEND_MD:
		m_base = adap_dev->adap + FRONTEND_BASE;
	break;
	case FRONTEND1_MD:
		m_base = adap_dev->adap + FRONTEND1_BASE;
	break;
	case FRONTEND2_MD:
		m_base = adap_dev->adap + FRONTEND2_BASE;
	break;
	case FRONTEND3_MD:
		m_base = adap_dev->adap + FRONTEND3_BASE;
	break;
	case READER_MD:
		m_base = adap_dev->adap + RD_BASE;
	break;
	case PIXEL_MD:
		m_base = adap_dev->adap + PIXEL_BASE;
	break;
	case ALIGN_MD:
		m_base = adap_dev->adap + ALIGN_BASE;
	break;
	case MISC_MD:
		m_base = adap_dev->adap + MISC_BASE;
	break;
	case TEMP_MD:
		m_base = adap_dev->adap;
	break;
	case PROC_MD:
		m_base = adap_dev->adap + PROC_BASE;
	break;
	default:
		pr_err("Error module: %d\n", module);
	break;
	}

	if ((module == FRONTEND_MD) && (adap_dev->index == 1))
		m_base += 0x400;

	return m_base;
}

static int module_reg_write(void *a_dev, int module, u32 addr, u32 val)
{
	int rtn = -1;
	void __iomem *m_base = NULL;

	m_base = module_get_base(a_dev, module);
	if (!m_base) {
		pr_err("Failed to get %d module base\n", module);
		return rtn;
	}

	writel(val, m_base + addr);

	return 0;
}

static int module_reg_read(void *a_dev, int module, u32 addr, u32 *val)
{
	int rtn = -1;
	void __iomem *m_base = NULL;

	m_base = module_get_base(a_dev, module);
	if (!m_base) {
		pr_err("Failed to get %d module base\n", module);
		return rtn;
	}

	*val = readl(m_base + addr);

	return 0;
}

static int module_update_bits(void *a_dev, int module,
				u32 addr, u32 val, u32 start, u32 len)
{
	int rtn = -1;
	u32 mask = 0;
	u32 orig = 0;
	u32 temp = 0;

	if (start + len > 32) {
		pr_err("Error input start and len\n");
		return 0;
	} else if (start == 0 && len == 32) {
		rtn = module_reg_write(a_dev, module, addr, val);
		return rtn;
	}

	rtn = module_reg_read(a_dev, module, addr, &orig);
	if (rtn) {
		pr_err("Error to read: addr 0x%x\n", addr);
		return rtn;
	}

	mask = ((1 << len) - 1) << start;
	val = (val << start);

	temp = orig & (~mask);
	temp |= val & mask;

	if (temp != orig)
		rtn = module_reg_write(a_dev, module, addr, temp);
	else
		rtn = 0;

	if (rtn)
		pr_err("Error update bits: module %d, addr 0x%08x, temp 0x%08x\n",
					module, addr, temp);

	return rtn;
}

static int adap_get_pixel_depth(void *a_param)
{
	int depth = -1;
	struct adapter_dev_param *param = a_param;

	switch (param->format) {
	case ADAP_RAW6:
		depth = 6;
	break;
	case ADAP_RAW7:
		depth = 7;
	break;
	case ADAP_RAW8:
		depth = 8;
	break;
	case ADAP_RAW10:
		depth = 10;
	break;
	case ADAP_RAW12:
		depth = 12;
	break;
	case ADAP_RAW14:
		depth = 14;
	break;
	case ADAP_YUV422_8BIT:
		depth = 16;
	break;
	default:
		pr_err("Error to support format\n");
	break;
	}

	return depth;
}

/* adapter frontend sub-module cfg */
void adap_frontend_embdec_cfg(void *a_dev)
{
	int module = FRONTEND_MD;

	module_update_bits(a_dev, module, CSI2_EMB_DEC_CTRL0, 1, 1, 1); //reset
	module_update_bits(a_dev, module, CSI2_EMB_DEC_CTRL0, 0, 1, 1);

	module_update_bits(a_dev, module, CSI2_EMB_DEC_CTRL0, 3, 30, 2); //last byte

	module_update_bits(a_dev, module, CSI2_EMB_DEC_CTRL0, 0x12, 8, 6); //data type

	module_update_bits(a_dev, module, CSI2_EMB_DEC_CTRL0, 0x1, 4, 4);

	module_update_bits(a_dev, module, CSI2_EMB_DEC_CTRL0, 0, 2, 1); // mode 0

	module_reg_write(a_dev, module, CSI2_EMB_DEC_CTRL2, 0x12345678); // dec_ref
	module_reg_write(a_dev, module, CSI2_EMB_DEC_CTRL1, 0x00000000); // dec_bitmsk&

	module_update_bits(a_dev, module, CSI2_EMB_DEC_CTRL3, 0, 0, 12); // byte0
	module_update_bits(a_dev, module, CSI2_EMB_DEC_CTRL3, 1, 16, 12); // byte1
	module_update_bits(a_dev, module, CSI2_EMB_DEC_CTRL4, 2, 0, 12); // byte2
	module_update_bits(a_dev, module, CSI2_EMB_DEC_CTRL4, 3, 16, 12); // byte3
}

static void adap_frontend_embdec_enable(void *a_dev, int enable)
{
	int module = FRONTEND_MD;

	if (enable)
		module_update_bits(a_dev, module, CSI2_EMB_DEC_CTRL0, 1, 0, 1);
	else
		module_update_bits(a_dev, module, CSI2_EMB_DEC_CTRL0, 0, 0, 1);
}

static void adap_frontend_emb_cfg_buf(void *a_dev, u32 addr)
{
	int module = FRONTEND_MD;

	module_reg_write(a_dev, module, CSI2_DDR_START_OTHER, addr);
}

static void adap_frontend_emb_cfg_size(void *a_dev, u32 size)
{
	int module = FRONTEND_MD;

	module_reg_write(a_dev, module, CSI2_DDR_MAX_BYTES_OTHER, size);
}

static void adap_frontend_emb_start(void *a_dev)
{
	int module = FRONTEND_MD;

	module_update_bits(a_dev, module, CSI2_GEN_CTRL0, 1, 19, 1);
}

static void adap_frontend_emb_stop(void *a_dev)
{
	int module = FRONTEND_MD;

	module_update_bits(a_dev, module, CSI2_GEN_CTRL0, 0, 19, 1);
}

const struct emb_ops_t emb_hw_ops = {
	.emb_cfg_buf = adap_frontend_emb_cfg_buf,
	.emb_cfg_size = adap_frontend_emb_cfg_size,
	.emb_start = adap_frontend_emb_start,
	.emb_stop = adap_frontend_emb_stop,
};

static int adap_ddr_mode_cfg(void *a_dev)
{
	struct adapter_dev_t *adap_dev = a_dev;

	module_update_bits(a_dev, ALIGN_MD, MIPI_ADAPT_FE_MUX_CTL9, 24, 2, 19);

	module_update_bits(a_dev, ALIGN_MD, MIPI_ADAPT_ALIG_CNTL11, 2, 16, 3);
	module_update_bits(a_dev, ALIGN_MD, MIPI_ADAPT_FE_MUX_CTL0, 8, 24, 4);

	module_update_bits(a_dev, READER_MD, MIPI_ADAPT_DDR_RD0_CNTL0, 1, 2, 2);
	module_update_bits(a_dev, READER_MD, MIPI_ADAPT_DDR_RD0_CNTL0, 0, 26, 0);
	module_update_bits(a_dev, READER_MD, MIPI_ADAPT_DDR_RD0_CNTL1, 0, 29, 1);

	module_update_bits(a_dev, READER_MD, MIPI_ADAPT_DDR_RD0_CNTL0, 1, 25, 1);
	module_update_bits(a_dev, READER_MD, MIPI_ADAPT_DDR_RD0_CNTL0, 0, 25, 1);

	module_update_bits(a_dev, READER_MD, MIPI_ADAPT_DDR_RD1_CNTL0, 1, 2, 2);
	module_update_bits(a_dev, READER_MD, MIPI_ADAPT_DDR_RD1_CNTL0, 0, 26, 0);
	module_update_bits(a_dev, READER_MD, MIPI_ADAPT_DDR_RD1_CNTL1, 0, 29, 1);

	module_update_bits(a_dev, READER_MD, MIPI_ADAPT_DDR_RD1_CNTL0, 1, 25, 1);
	module_update_bits(a_dev, READER_MD, MIPI_ADAPT_DDR_RD1_CNTL0, 0, 25, 1);

	module_update_bits(a_dev, ALIGN_MD, MIPI_ADAPT_FE_MUX_CTL1, 2, 30, 2);

	//lbuf no sync
	module_update_bits(a_dev, ALIGN_MD, MIPI_ADAPT_FE_MUX_CTL1, 0xFF, 20, 8);

	//pixel rst
	module_update_bits(a_dev, PIXEL_MD, MIPI_ADAPT_PIXEL0_CNTL3, 0, 0, 3);
	module_update_bits(a_dev, PIXEL_MD, MIPI_ADAPT_PIXEL1_CNTL3, 0, 0, 3);

	//align rst
	module_update_bits(a_dev, ALIGN_MD, MIPI_ADAPT_ALIG_CNTL7, 0, 13, 2);

	//vfifo rst
	module_update_bits(a_dev, ALIGN_MD, MIPI_ADAPT_FE_MUX_CTL10, 0xF, 12, 4);

	//vfifo full hold
	module_update_bits(a_dev, ALIGN_MD, MIPI_ADAPT_ALIG_CNTL10, 0xFF, 24, 8);

	pr_debug("ADAP%u: ddr new cfg\n", adap_dev->index);

	return 0;
}

static int adap_frontend_init(void *a_dev)
{
	int rtn = 0;
	u32 cfg_all_to_mem, pingpong_en;
	u32 cfg_line_sup_vs_en,cfg_line_sup_vs_sel,cfg_line_sup_sel;
	u32 cfg_isp2ddr_enable, cfg_isp2comb_enable, vc_mode;
	u32 mem_addr0_a, mem_addr0_b,mem_addr1_a, mem_addr1_b;
	u32 reg_lbuf0_vs_sel;
	u32 reg_vfifo_vs_out_pre;
	u32 long_offset,short_offset;
	int module = FRONTEND_MD;
	struct adapter_dev_t *adap_dev = a_dev;
	struct adapter_dev_param *param = &adap_dev->param;

	switch (adap_dev->index) {
	case 0:
		module = FRONTEND_MD;
		break;
	case 1:
		module = FRONTEND1_MD;
		break;
	case 2:
		module = FRONTEND2_MD;
		break;
	case 3:
		module = FRONTEND3_MD;
		break;
	default:
		break;
	}

	switch (param->mode) {
	case MODE_MIPI_RAW_SDR_DDR:
		cfg_all_to_mem = 1;
		pingpong_en = 0;
		cfg_isp2ddr_enable = 0;
		cfg_isp2comb_enable = 0;
		vc_mode = 0x11110000;
		mem_addr0_a = param->ddr_buf[0].addr[0];
		mem_addr0_b = param->ddr_buf[0].addr[0];
		mem_addr1_a = param->ddr_buf[0].addr[0];
		mem_addr1_b = param->ddr_buf[0].addr[0];
		reg_lbuf0_vs_sel = 3;
		reg_vfifo_vs_out_pre = 7;
		cfg_line_sup_vs_en	= 1;
		cfg_line_sup_vs_sel = 1;
		cfg_line_sup_sel	= 1;
		break ;
	case MODE_MIPI_RAW_SDR_DIRCT:
		cfg_all_to_mem = 0;
		pingpong_en = 0;
		cfg_isp2ddr_enable = 0;
		cfg_isp2comb_enable = 0;
		vc_mode = 0x11110000;
		mem_addr0_a = 0;
		mem_addr0_b = 0;
		mem_addr1_a = 0;
		mem_addr1_b = 0;
		reg_lbuf0_vs_sel = 0;
		reg_vfifo_vs_out_pre = 0;
		cfg_line_sup_vs_en	= 0;
		cfg_line_sup_vs_sel = 0;
		cfg_line_sup_sel	= 0;
		break;
	case MODE_MIPI_RAW_HDR_DDR_DDR:
		cfg_all_to_mem = 0;
		pingpong_en = 0;
		cfg_isp2ddr_enable = 1;
		cfg_isp2comb_enable = 0;
		vc_mode = 0x111100f1;
		mem_addr0_a = param->ddr_buf[0].addr[0];
		mem_addr0_b = param->ddr_buf[0].addr[0];
		mem_addr1_a = param->ddr_buf[0].addr[0];
		mem_addr1_b = param->ddr_buf[0].addr[0];
		reg_lbuf0_vs_sel = 0;
		reg_vfifo_vs_out_pre = 0;
		cfg_line_sup_vs_en = 1;
		cfg_line_sup_vs_sel = 1;
		cfg_line_sup_sel = 1;
		break;
	case MODE_MIPI_RAW_HDR_DDR_DIRCT:
		cfg_all_to_mem = 0;
		pingpong_en = 0;
		cfg_isp2ddr_enable = 0;
		cfg_isp2comb_enable = 0;
		vc_mode = 0x111100f1;
		mem_addr0_a = param->ddr_buf[0].addr[0];
		mem_addr0_b = param->ddr_buf[0].addr[0];
		mem_addr1_a = param->ddr_buf[0].addr[0];
		mem_addr1_b = param->ddr_buf[0].addr[0];
		reg_lbuf0_vs_sel = 0;
		reg_vfifo_vs_out_pre = 0;
		cfg_line_sup_vs_en = 1;
		cfg_line_sup_vs_sel = 1;
		cfg_line_sup_sel = 1;
	break;
	case MODE_MIPI_YUV_SDR_DDR:
		cfg_all_to_mem = 1;
		pingpong_en = 0;
		cfg_isp2ddr_enable = 0;
		cfg_isp2comb_enable = 0;
		vc_mode = 0x11110000;
		mem_addr0_a = 0;
		mem_addr0_b = 0;
		mem_addr1_a = 0;
		mem_addr1_b = 0;
		reg_lbuf0_vs_sel = 3;
		reg_vfifo_vs_out_pre = 7;
		cfg_line_sup_vs_en	= 0;
		cfg_line_sup_vs_sel = 0;
		cfg_line_sup_sel	= 0;
	break;
	case MODE_MIPI_RGB_SDR_DDR:
		cfg_all_to_mem = 1;
		pingpong_en = 0;
		cfg_isp2ddr_enable = 0;
		cfg_isp2comb_enable = 0;
		vc_mode = 0x11110000;
		mem_addr0_a = 0;
		mem_addr0_b = 0;
		mem_addr1_a = 0;
		mem_addr1_b = 0;
		reg_lbuf0_vs_sel = 3;
		reg_vfifo_vs_out_pre = 7;
		cfg_line_sup_vs_en	= 0;
		cfg_line_sup_vs_sel = 0;
		cfg_line_sup_sel	= 0;
		break;
	case MODE_MIPI_YUV_SDR_DIRCT:
		cfg_all_to_mem = 0;
		pingpong_en = 0;
		cfg_isp2ddr_enable = 0;
		cfg_isp2comb_enable = 1;
		vc_mode = 0x11110000;
		mem_addr0_a = 0;
		mem_addr0_b = 0;
		mem_addr1_a = 0;
		mem_addr1_b = 0;
		reg_lbuf0_vs_sel = 0;
		reg_vfifo_vs_out_pre = 0;
		cfg_line_sup_vs_en	= 0;
		cfg_line_sup_vs_sel = 0;
		cfg_line_sup_sel	= 0;
		break;
	case MODE_MIPI_RGB_SDR_DIRCT:
		cfg_all_to_mem = 0;
		pingpong_en = 0;
		cfg_isp2ddr_enable = 0;
		cfg_isp2comb_enable = 1;
		vc_mode = 0x11110000;
		mem_addr0_a = 0;
		mem_addr0_b = 0;
		mem_addr1_a = 0;
		mem_addr1_b = 0;
		reg_lbuf0_vs_sel = 0;
		reg_vfifo_vs_out_pre = 0;
		cfg_line_sup_vs_en	= 0;
		cfg_line_sup_vs_sel = 0;
		cfg_line_sup_sel = 0;
		break;
	default:
		cfg_all_to_mem = 0;
		pingpong_en = 0;
		cfg_isp2ddr_enable = 0;
		cfg_isp2comb_enable = 0;
		vc_mode = 0x11110000;
		mem_addr0_a = 0;
		mem_addr0_b = 0;
		mem_addr1_a = 0;
		mem_addr1_b = 0;
		reg_lbuf0_vs_sel = 0;
		reg_vfifo_vs_out_pre = 0;
		cfg_line_sup_vs_en	= 0;
		cfg_line_sup_vs_sel = 0;
		cfg_line_sup_sel = 0;
		break;
	}

	module_reg_write(a_dev, module, CSI2_CLK_RESET, 0x0);
	module_reg_write(a_dev, module, CSI2_CLK_RESET, 0x6);
	module_reg_write(a_dev, module, CSI2_X_START_END_ISP,
					(param->fe_param.fe_isp_x_end << 16 |
					param->fe_param.fe_isp_x_start << 0));

	module_reg_write(a_dev, module, CSI2_Y_START_END_ISP,
					(param->fe_param.fe_isp_y_end << 16 |
					param->fe_param.fe_isp_y_start << 0));

	module_reg_write(a_dev, module, CSI2_X_START_END_MEM,
					(param->fe_param.fe_mem_x_end << 16 |
					param->fe_param.fe_mem_x_start << 0));

	module_reg_write(a_dev, module, CSI2_Y_START_END_MEM,
					(param->fe_param.fe_mem_y_end << 16 |
					param->fe_param.fe_mem_y_start << 0));

	module_reg_write(a_dev, module, CSI2_DDR_START_PIX, mem_addr0_a);
	module_reg_write(a_dev, module, CSI2_DDR_START_PIX_ALT, mem_addr0_b);
	module_reg_write(a_dev, module, CSI2_DDR_START_PIX_B, mem_addr1_a);
	module_reg_write(a_dev, module, CSI2_DDR_START_PIX_B_ALT, mem_addr1_b);

	module_reg_write(a_dev, module, CSI2_DDR_START_OTHER,
					mem_addr0_a);

	module_reg_write(a_dev, module, CSI2_DDR_START_OTHER_ALT,
					mem_addr0_a);

	module_reg_write(a_dev, module, CSI2_DDR_STRIDE_PIX,
					param->fe_param.fe_mem_line_stride << 4);

	module_reg_write(a_dev, module, CSI2_DDR_STRIDE_PIX_B,
					param->fe_param.fe_mem_line_stride << 4);

	module_reg_write(a_dev, module, CSI2_INTERRUPT_CTRL_STAT,
					param->fe_param.fe_int_mask);

	module_reg_write(a_dev, module, CSI2_LINE_SUP_CNTL0,
					cfg_line_sup_vs_sel << 15 |
					cfg_line_sup_vs_en << 16 |
					cfg_line_sup_sel << 17 |
					param->fe_param.fe_mem_line_minbyte << 0);

	module_reg_write(a_dev, module, CSI2_VC_MODE, vc_mode);
	module_reg_write(a_dev, module, CSI2_GEN_CTRL0,
					cfg_all_to_mem         << 4 |
					pingpong_en            << 5 |
					0x01                    << 12 |
					0x03                    << 16 |
					cfg_isp2ddr_enable     << 25 |
					cfg_isp2comb_enable    << 26);

	if (param->mode == MODE_MIPI_RAW_HDR_DDR_DIRCT) {
		if (param->dol_type == ADAP_DOL_LINEINFO) {
			module_reg_write(a_dev, module, CSI2_VC_MODE, 0x11110052);
			//if ((fe_io == FRONTEND1_IO) && frontend1_flag )
			//	module_reg_write(a_dev, module, CSI2_VC_MODE, 0x11110046);   //ft1 vc_mode
			if (param->format == ADAP_RAW10 ) {
				module_reg_write(a_dev, module, CSI2_VC_MODE2_MATCH_MASK_A_L, 0x6e6e6e6e);
				module_reg_write(a_dev, module, CSI2_VC_MODE2_MATCH_MASK_A_H, 0xffffff00);
				module_reg_write(a_dev, module, CSI2_VC_MODE2_MATCH_A_L, 0x90909090);
				module_reg_write(a_dev, module, CSI2_VC_MODE2_MATCH_A_H, 0x55);
				module_reg_write(a_dev, module, CSI2_VC_MODE2_MATCH_MASK_B_L, 0x6e6e6e6e);
				module_reg_write(a_dev, module, CSI2_VC_MODE2_MATCH_MASK_B_H, 0xffffff00);
				module_reg_write(a_dev, module, CSI2_VC_MODE2_MATCH_B_L, 0x90909090);
				module_reg_write(a_dev, module, CSI2_VC_MODE2_MATCH_B_H, 0xaa);
			} else if (param->format == ADAP_RAW12) {
				module_reg_write(a_dev, module, CSI2_VC_MODE2_MATCH_MASK_A_L, 0xff000101);
				module_reg_write(a_dev, module, CSI2_VC_MODE2_MATCH_MASK_A_H, 0xffffffff);
				module_reg_write(a_dev, module, CSI2_VC_MODE2_MATCH_A_L, 0x112424);
				module_reg_write(a_dev, module, CSI2_VC_MODE2_MATCH_A_H, 0x0);
				module_reg_write(a_dev, module, CSI2_VC_MODE2_MATCH_MASK_B_L, 0xff000101);
				module_reg_write(a_dev, module, CSI2_VC_MODE2_MATCH_MASK_B_H, 0xffffffff);
				module_reg_write(a_dev, module, CSI2_VC_MODE2_MATCH_B_L, 0x222424);
				module_reg_write(a_dev, module, CSI2_VC_MODE2_MATCH_B_H, 0x0);
			}
			long_offset = param->offset.long_offset;
			short_offset = param->offset.short_offset;
			module_update_bits(a_dev, module, CSI2_X_START_END_MEM, 0xc, 0, 16);
			module_update_bits(a_dev, module, CSI2_X_START_END_MEM,
						0xc + param->width - 1, 16, 16);
			module_update_bits(a_dev, module, CSI2_Y_START_END_MEM,
						long_offset, 0, 16);
			module_update_bits(a_dev, module, CSI2_Y_START_END_MEM,
						long_offset + param->height - 1, 16, 16);
			//set short exposure offset
			module_update_bits(a_dev, module, CSI2_X_START_END_ISP, 0xc, 0, 16);
			module_update_bits(a_dev, module, CSI2_X_START_END_ISP,
						0xc + param->width - 1, 16, 16);
			module_update_bits(a_dev, module, CSI2_Y_START_END_ISP,
						short_offset, 0, 16);
			module_update_bits(a_dev, module, CSI2_Y_START_END_ISP,
						short_offset + param->height - 1, 16, 16);
		}else if (param->dol_type == ADAP_DOL_VC) {
			module_reg_write(a_dev, module, CSI2_VC_MODE, 0x11220040);
			//if ((fe_io == FRONTEND1_IO) && frontend1_flag )
			//	module_reg_write(a_dev, module, CSI2_VC_MODE, 0x22110000);
		}
	}

	//module_reg_write(a_dev, ALIGN_MD, MIPI_ADAPT_FE_MUX_CTL0,
	//				0 << 24 |
	//				reg_vfifo_vs_out_pre << 12 |
	//				reg_lbuf0_vs_sel << 8);

	return rtn;
}

static void adap_frontend_start(void *a_dev)
{
	struct adapter_dev_t *adap_dev = a_dev;
	int module = FRONTEND_MD;

	switch (adap_dev->index) {
	case 0:
		module = FRONTEND_MD;
		break;
	case 1:
		module = FRONTEND1_MD;
		break;
	case 2:
		module = FRONTEND2_MD;
		break;
	case 3:
		module = FRONTEND3_MD;
		break;
	default:
		break;
	}

	adap_frontend_embdec_enable(a_dev, 0);

	module_update_bits(a_dev, module, CSI2_GEN_CTRL0, 0xf, 0, 4);
}

static void adap_frontend_stop(void *a_dev)
{
	struct adapter_dev_t *adap_dev = a_dev;
	int module = FRONTEND_MD;

	switch (adap_dev->index) {
	case 0:
		module = FRONTEND_MD;
		break;
	case 1:
		module = FRONTEND1_MD;
		break;
	case 2:
		module = FRONTEND2_MD;
		break;
	case 3:
		module = FRONTEND3_MD;
		break;
	default:
		break;
	}

	module_update_bits(a_dev, module, CSI2_GEN_CTRL0, 0x0, 0, 4);
}

/* adapter reader sub-module cfg */

static int adap_reader_init(void *a_dev)
{
	int rtn = 0;
	u32 val = 0;
	u32 dol_mode,pingpong_mode;
	u32 port_sel_0;
	u32 port_sel_1;
	u32 ddr_rden_0;
	u32 ddr_rden_1;
	u32 ddr_rd0_ping,ddr_rd0_pong;
	u32 ddr_rd1_ping,ddr_rd1_pong;
	u32 continue_mode = 0;
	int module = READER_MD;
	struct adapter_dev_t *adap_dev = a_dev;
	struct adapter_dev_param *param = &adap_dev->param;

	switch (param->mode) {
	case MODE_MIPI_RAW_SDR_DDR:
		dol_mode = 0;
		pingpong_mode = 0;
		continue_mode = 0;
		port_sel_0 = 1;
		port_sel_1 = 1;
		ddr_rden_0 = 1;
		ddr_rden_1 = 0;
		ddr_rd0_ping = param->ddr_buf[0].addr[0];
		ddr_rd0_pong = param->ddr_buf[0].addr[0];
		ddr_rd1_ping = param->ddr_buf[0].addr[0];
		ddr_rd1_pong = param->ddr_buf[0].addr[0];
	break;
	case MODE_MIPI_RAW_SDR_DIRCT:
		dol_mode = 0;
		pingpong_mode = 0;
		port_sel_0 = 0;
		port_sel_1 = 0;
		ddr_rden_0 = 1;
		ddr_rden_1 = 0;
		ddr_rd0_ping = 0;
		ddr_rd0_pong = 0;
		ddr_rd1_ping = 0;
		ddr_rd1_pong = 0;
	break;
	case MODE_MIPI_RAW_HDR_DDR_DDR:
		dol_mode = 0;
		pingpong_mode = 0;
		port_sel_0 = 1;
		port_sel_1 = 1;
		ddr_rden_0 = 1;
		ddr_rden_1 = 1;
		ddr_rd0_ping = 0;
		ddr_rd0_pong = 0;
		ddr_rd1_ping = 0;
		ddr_rd1_pong = 0;
	break;
	case MODE_MIPI_RAW_HDR_DDR_DIRCT:
		dol_mode = 1;
		pingpong_mode = 1;
		continue_mode = 1;
		port_sel_0 = 0;
		port_sel_1 = 1;
		ddr_rden_0 = 1;
		ddr_rden_1 = 1;
		ddr_rd0_ping = param->ddr_buf[0].addr[0];
		ddr_rd0_pong = param->ddr_buf[0].addr[0];
		ddr_rd1_ping = param->ddr_buf[0].addr[0];
		ddr_rd1_pong = param->ddr_buf[0].addr[0];
	break;
	case MODE_MIPI_YUV_SDR_DDR:
		dol_mode = 0;
		pingpong_mode = 0;
		port_sel_0 = 1;
		port_sel_1 = 0;
		ddr_rden_0 = 1;
		ddr_rden_1 = 0;
		ddr_rd0_ping = 0;
		ddr_rd0_pong = 0;
		ddr_rd1_ping = 0;
		ddr_rd1_pong = 0;
	break;
	case MODE_MIPI_RGB_SDR_DDR:
		dol_mode = 0;
		pingpong_mode = 0;
		port_sel_0 = 1;
		port_sel_1 = 0;
		ddr_rden_0 = 1;
		ddr_rden_1 = 0;
		ddr_rd0_ping = 0;
		ddr_rd0_pong = 0;
		ddr_rd1_ping = 0;
		ddr_rd1_pong = 0;
	break;
	case MODE_MIPI_YUV_SDR_DIRCT:
		dol_mode = 0;
		pingpong_mode = 0;
		port_sel_0 = 2;
		port_sel_1 = 0;
		ddr_rden_0 = 1;
		ddr_rden_1 = 0;
		ddr_rd0_ping = 0;
		ddr_rd0_pong = 0;
		ddr_rd1_ping = 0;
		ddr_rd1_pong = 0;
	break;
	case MODE_MIPI_RGB_SDR_DIRCT:
		dol_mode = 0;
		pingpong_mode = 0;
		port_sel_0 = 2;
		port_sel_1 = 0;
		ddr_rden_0 = 1;
		ddr_rden_1 = 0;
		ddr_rd0_ping = 0;
		ddr_rd0_pong = 0;
		ddr_rd1_ping = 0;
		ddr_rd1_pong = 0;
	break;
	default:
		dol_mode = 0;
		pingpong_mode = 0;
		port_sel_0 = 0;
		port_sel_1 = 0;
		ddr_rden_0 = 1;
		ddr_rden_1 = 0;
		ddr_rd0_ping = 0;
		ddr_rd0_pong = 0;
		ddr_rd1_ping = 0;
		ddr_rd1_pong = 0;
	break;
	}

	module_reg_write(a_dev, module, MIPI_ADAPT_DDR_RD0_CNTL0, 3 << 28 |
                                    1 << 26 |
                                    continue_mode << 24 |
                                    dol_mode << 23 |
                                    pingpong_mode << 22 |
                                    param->rd_param.rd_mem_line_stride << 4);
	module_reg_write(a_dev, module, MIPI_ADAPT_DDR_RD0_CNTL1, port_sel_0 << 30 |
                                    param->rd_param.rd_mem_line_number << 16 |
                                    param->rd_param.rd_mem_line_size << 0);
	module_reg_write(a_dev, module, MIPI_ADAPT_DDR_RD0_CNTL2, ddr_rd0_ping);
	module_reg_write(a_dev, module, MIPI_ADAPT_DDR_RD0_CNTL3, ddr_rd0_pong);


	module_reg_read(a_dev, module, MIPI_ADAPT_DDR_RD0_CNTL0, &val);
	module_reg_write(a_dev, module, MIPI_ADAPT_DDR_RD0_CNTL0, val | ddr_rden_0  << 0);

	module_reg_write(a_dev, module, MIPI_ADAPT_DDR_RD1_CNTL0, 3 << 28 |
                                    1 << 26 |
                                    continue_mode << 24 |
                                    dol_mode << 23 |
                                    pingpong_mode << 22 |
                                    param->rd_param.rd_mem_line_stride << 4);
	module_reg_write(a_dev, module, MIPI_ADAPT_DDR_RD1_CNTL1, port_sel_1 << 30 |
                                    param->rd_param.rd_mem_line_number << 16 |
                                    param->rd_param.rd_mem_line_size << 0);
	module_reg_write(a_dev, module, MIPI_ADAPT_DDR_RD1_CNTL2, ddr_rd1_ping);
	module_reg_write(a_dev, module, MIPI_ADAPT_DDR_RD1_CNTL3, ddr_rd1_pong);

	module_reg_read(a_dev, module, MIPI_ADAPT_DDR_RD1_CNTL0, &val);
	module_reg_write(a_dev, module, MIPI_ADAPT_DDR_RD1_CNTL0, val | ddr_rden_1  << 0 | HDR_LOOPBACK_MODE << 1);

	return rtn;
}

static void adap_reader_start(void *a_dev)
{
	int module = READER_MD;

	module_update_bits(a_dev, module, MIPI_ADAPT_DDR_RD0_CNTL0, 1, 31, 1);

	module_update_bits(a_dev, module, MIPI_ADAPT_DDR_RD1_CNTL0, 1, 31, 1);
}

/* adapter pixel sub-module cfg */

static int adap_pixel_init(void *a_dev)
{
	int rtn = 0;
	u32 val = 0;
	u32 data_mode_0;
	u32 data_mode_1;
	u32 pixel_en_0 ;
	u32 pixel_en_1 ;
	int module = PIXEL_MD;
	struct adapter_dev_t *adap_dev = a_dev;
	struct adapter_dev_param *param = &adap_dev->param;

	switch (param->mode) {
	case MODE_MIPI_RAW_SDR_DDR:
		data_mode_0 = 0;
		data_mode_1 = 0;
		pixel_en_0 = 1;
		pixel_en_1 = 0;
	break;
	case MODE_MIPI_RAW_SDR_DIRCT:
		data_mode_0 = 1;
		data_mode_1 = 0;
		pixel_en_0 = 1;
		pixel_en_1 = 0;
	break;
	case MODE_MIPI_RAW_HDR_DDR_DDR:
		data_mode_0 = 0;
		data_mode_1 = 0;
		pixel_en_0 = 1;
		pixel_en_1 = 1;
	break;
	case MODE_MIPI_RAW_HDR_DDR_DIRCT:
		data_mode_0 = 1;
		data_mode_1 = 0;
		pixel_en_0 = 1;
		pixel_en_1 = 1;
	break;
	case MODE_MIPI_YUV_SDR_DDR:
		data_mode_0 = 2;
		data_mode_1 = 0;
		pixel_en_0 = 1;
		pixel_en_1 = 0;
	break;
	case MODE_MIPI_RGB_SDR_DDR:
		data_mode_0 = 2;
		data_mode_1 = 0;
		pixel_en_0 = 1;
		pixel_en_1 = 0;
	break;
	case MODE_MIPI_YUV_SDR_DIRCT:
		data_mode_0 = 3;
		data_mode_1 = 0;
		pixel_en_0 = 1;
		pixel_en_1 = 0;
	break;
	case MODE_MIPI_RGB_SDR_DIRCT:
		data_mode_0 = 3;
		data_mode_1 = 0;
		pixel_en_0 = 1;
		pixel_en_1 = 0;
	break;
	default:  //sdr dir
		data_mode_0 = 1;
		data_mode_1 = 0;
		pixel_en_0 = 1;
		pixel_en_1 = 0;
	break;
	}

	module_reg_write(a_dev, module, MIPI_ADAPT_PIXEL0_CNTL0,
					param->pixel_param.pixel_data_type << 20 |
					data_mode_0 << 16 );

	module_reg_write(a_dev, module, MIPI_ADAPT_PIXEL0_CNTL1,
					param->pixel_param.pixel_isp_x_start << 16 |
					param->pixel_param.pixel_isp_x_end << 0);

	module_reg_write(a_dev, module, MIPI_ADAPT_PIXEL0_CNTL2,
					param->pixel_param.pixel_pixel_number << 15 |
					param->pixel_param.pixel_line_size << 0);

	module_reg_write(a_dev, module, MIPI_ADAPT_PIXEL1_CNTL0,
					param->pixel_param.pixel_data_type << 20 |
					data_mode_1 << 16 );

	module_reg_write(a_dev, module, MIPI_ADAPT_PIXEL1_CNTL1,
					param->pixel_param.pixel_isp_x_start << 16 |
					param->pixel_param.pixel_isp_x_end << 0);

	module_reg_write(a_dev, module, MIPI_ADAPT_PIXEL1_CNTL2,
					param->pixel_param.pixel_pixel_number << 15 |
					param->pixel_param.pixel_line_size << 0);

	return rtn;
}

static void adap_pixel_start(void *a_dev)
{
	int module = PIXEL_MD;

	module_update_bits(a_dev, module, MIPI_ADAPT_PIXEL0_CNTL0, 1, 31, 1);

	module_update_bits(a_dev, module, MIPI_ADAPT_PIXEL1_CNTL0, 1, 31, 1);
}

/* adapter align sub-module cfg */

static int adap_align_init(void *a_dev)
{
	int rtn = 0;
	int module = ALIGN_MD;
	struct adapter_dev_t *adap_dev = a_dev;
	struct adapter_dev_param *param = &adap_dev->param;
	u32 lane0_en;
	u32 lane0_sel;
	u32 lane1_en;
	u32 lane1_sel;
	u32 pix_datamode_0;
	u32 pix_datamode_1;
	u32 vdata0_sel;
	u32 vdata1_sel;
	u32 vdata2_sel;
	u32 vdata3_sel;
	u32 yuvrgb_mode;

	switch (param->mode) {
	case MODE_MIPI_RAW_SDR_DDR:
		lane0_en = 1;
		lane0_sel = 0;
		lane1_en = 0;
		lane1_sel = 0;
		pix_datamode_0 = 0;
		pix_datamode_1 = 0;
		vdata0_sel = 0;
		vdata1_sel = 0;
		vdata2_sel = 0;
		vdata3_sel = 0;
		yuvrgb_mode = 0;
	break;
	case MODE_MIPI_RAW_SDR_DIRCT:
		lane0_en = 1;
		lane0_sel = 0;
		lane1_en = 0;
		lane1_sel = 0;
		pix_datamode_0 = 1;
		pix_datamode_1 = 0;
		vdata0_sel = 0;
		vdata1_sel = 0;
		vdata2_sel = 0;
		vdata3_sel = 0;
		yuvrgb_mode = 0;
	break;
	case MODE_MIPI_RAW_HDR_DDR_DDR:
		lane0_en = 1;
		lane0_sel = 0;
		lane1_en = 1;
		lane1_sel = 1;
		pix_datamode_0 = 0;
		pix_datamode_1 = 0;
		vdata0_sel = 0;
		vdata1_sel = 1;
		vdata2_sel = 0;
		vdata3_sel = 0;
		yuvrgb_mode = 0;
	break;
	case MODE_MIPI_RAW_HDR_DDR_DIRCT:
		lane0_en = 1;
		lane0_sel = 0;
		lane1_en = 1;
		lane1_sel = 1;
		pix_datamode_0 = 1;
		pix_datamode_1 = 0;
		vdata0_sel = 1;
		vdata1_sel = 0;
		vdata2_sel = 1;
		vdata3_sel = 1;
		yuvrgb_mode = 0;
	break;
	case MODE_MIPI_YUV_SDR_DDR:
		lane0_en = 1;
		lane0_sel = 0;
		lane1_en = 0;
		lane1_sel = 0;
		pix_datamode_0= 0;
		pix_datamode_1= 0;
		vdata0_sel = 0;
		vdata1_sel = 0;
		vdata2_sel = 0;
		vdata3_sel = 0;
		yuvrgb_mode = 1;
	break;
	case MODE_MIPI_RGB_SDR_DDR:
		lane0_en = 1;
		lane0_sel = 0;
		lane1_en = 0;
		lane1_sel = 0;
		pix_datamode_0 = 0;
		pix_datamode_1 = 0;
		vdata0_sel = 0;
		vdata1_sel = 0;
		vdata2_sel = 0;
		vdata3_sel = 0;
		yuvrgb_mode = 1;
	break;
	case MODE_MIPI_YUV_SDR_DIRCT:
		lane0_en = 1;
		lane0_sel = 0;
		lane1_en = 0;
		lane1_sel = 0;
		pix_datamode_0 = 1;
		pix_datamode_1 = 0;
		vdata0_sel = 0;
		vdata1_sel = 0;
		vdata2_sel = 0;
		vdata3_sel = 0;
		yuvrgb_mode = 1;
	break;
	case MODE_MIPI_RGB_SDR_DIRCT:
		lane0_en = 1;
		lane0_sel = 0;
		lane1_en = 0;
		lane1_sel = 0;
		pix_datamode_0 = 1;
		pix_datamode_1 = 0;
		vdata0_sel = 0;
		vdata1_sel = 0;
		vdata2_sel = 0;
		vdata3_sel = 0;
		yuvrgb_mode = 1;
	break;
	default:
		lane0_en = 1;
		lane0_sel = 0;
		lane1_en = 0;
		lane1_sel = 0;
		pix_datamode_0 = 1;
		pix_datamode_1 = 0;
		vdata0_sel = 0;
		vdata1_sel = 0;
		vdata2_sel = 0;
		vdata3_sel = 0;
		yuvrgb_mode = 0;
	break;
	}

	module_reg_write(a_dev, module, MIPI_ADAPT_ALIG_CNTL0,
					(param->alig_param.alig_hsize + 64) << 0 |
					(param->alig_param.alig_vsize + 64) << 16 );

	module_reg_write(a_dev, module, MIPI_ADAPT_ALIG_CNTL1,
					param->alig_param.alig_hsize << 16 |
					0 << 0 );

	module_reg_write(a_dev, module, MIPI_ADAPT_ALIG_CNTL2,
					param->alig_param.alig_vsize << 16 |
					0 << 0 );

	//module_reg_write(a_dev, module, MIPI_ADAPT_ALIG_CNTL3,
	//				param->alig_param.alig_hsize << 0 |
	//				0 << 16 );

	module_reg_write(a_dev, module, MIPI_ADAPT_ALIG_CNTL6,
					lane0_en << 0 |	//lane enable
					lane0_sel << 1 |  //lane select
					lane1_en << 2 |  //lane enable
					lane1_sel << 3 |  //lane select
					pix_datamode_0 << 4 |  //1 dir 0 ddr
					pix_datamode_1 << 5 |  //
					vdata0_sel << 8 |  //vdata0 select
					vdata1_sel << 9 |  //vdata1 select
					vdata2_sel << 10 |  //vdata2 select
					vdata3_sel << 11 |  //vdata3 select
					0xf << 12 |
					yuvrgb_mode << 31);

	module_reg_write(a_dev, module, MIPI_ADAPT_ALIG_CNTL8, ((1 <<12 ) | (1 <<5)));
	module_update_bits(a_dev, module, MIPI_ADAPT_ALIG_CNTL10, 0xff, 24, 8);
	module_update_bits(a_dev, module, MIPI_ADAPT_FE_MUX_CTL1, 0xff, 20, 8);

	return rtn;
}

static void adap_align_start(void *a_dev)
{
	int module = ALIGN_MD;
	int alig_width = 0;
	int alig_height = 0;
	struct adapter_dev_t *adap_dev = a_dev;
	struct adapter_dev_param *param = &adap_dev->param;

	alig_width = param->width + 64; //hblank > 32 cycles
	alig_height = param->height + 64; //vblank > 48 lines

	module_update_bits(a_dev, module, MIPI_ADAPT_ALIG_CNTL0, alig_width, 0, 16);
	module_update_bits(a_dev, module, MIPI_ADAPT_ALIG_CNTL0, alig_height, 16, 16);
	module_update_bits(a_dev, module, MIPI_ADAPT_ALIG_CNTL1, param->width, 16, 13);
	module_update_bits(a_dev, module, MIPI_ADAPT_ALIG_CNTL2, param->height, 16, 13);

	module_update_bits(a_dev, module, MIPI_ADAPT_ALIG_CNTL8, 1, 31, 1);
}

/* adapter all sub-modules reset cfg */

static void adap_module_reset(void *a_dev)
{
	struct adapter_global_info *g_info = aml_adap_global_get_info();
	struct adapter_dev_t *adap_dev = a_dev;
	int module = FRONTEND_MD;

	switch (adap_dev->index) {
	case 0:
		module = FRONTEND_MD;
		break;
	case 1:
		module = FRONTEND1_MD;
		break;
	case 2:
		module = FRONTEND2_MD;
		break;
	case 3:
		module = FRONTEND3_MD;
		break;
	default:
		break;
	}

	module_reg_write(a_dev, module, CSI2_GEN_CTRL0, 0x00000000);

	if (g_info->user)
		return;

	module_update_bits(a_dev, READER_MD, MIPI_ADAPT_DDR_RD0_CNTL0, 0, 0, 1);

	udelay(1000);
#ifdef T7C_CHIP
	module_update_bits(a_dev, PROC_MD, MIPI_TOP_CSI2_CTRL0, 1, 6, 1);
	module_update_bits(a_dev, PROC_MD, MIPI_TOP_CSI2_CTRL0, 0, 6, 1);
#else
	module_update_bits(a_dev, PROC_MD, MIPI_PROC_TOP_CTRL0, 1, 2, 1);
	module_update_bits(a_dev, PROC_MD, MIPI_PROC_TOP_CTRL0, 0, 2, 1);
#endif
}

static int adap_hw_init(void *a_dev)
{
	int rtn = 0;
	struct adapter_dev_t *adap_dev = a_dev;
	struct adapter_dev_param *param = &adap_dev->param;
	struct adapter_global_info *g_info = aml_adap_global_get_info();

	param->fe_param.fe_sel = adap_dev->index;
	param->fe_param.fe_work_mode = param->mode;
	param->fe_param.fe_mem_x_start = 0;
	param->fe_param.fe_mem_x_end = param->width - 1;
	param->fe_param.fe_mem_y_start = 0;
	param->fe_param.fe_mem_y_end = param->height - 1;
	param->fe_param.fe_isp_x_start = 0;
	param->fe_param.fe_isp_x_end = param->width - 1;
	param->fe_param.fe_isp_y_start = 0;
	param->fe_param.fe_isp_y_end = param->height - 1;
	param->fe_param.fe_mem_other_addr = 0;

	param->fe_param.fe_mem_line_stride =
		ceil_upper((adap_get_pixel_depth(param) * param->width), (8 * 16));

	param->fe_param.fe_mem_line_minbyte =
		(adap_get_pixel_depth(param) * param->width + 7) >> 3;

	param->fe_param.fe_int_mask = 0;

	//cofigure rd
	param->rd_param.rd_work_mode = param->mode;
	param->rd_param.rd_mem_line_stride =
		(param->width * adap_get_pixel_depth(param) + 127) >> 7;

	param->rd_param.rd_mem_line_size =
		(param->width * adap_get_pixel_depth(param) + 127) >> 7;

	param->rd_param.rd_mem_line_number = param->height;

	//cofigure pixel
	param->pixel_param.pixel_work_mode = param->mode;
	param->pixel_param.pixel_data_type = param->format;
	param->pixel_param.pixel_isp_x_start = 0;
	param->pixel_param.pixel_isp_x_end = param->width - 1 ;

	param->pixel_param.pixel_line_size =
		(param->width * adap_get_pixel_depth(param) + 127) >> 7;

	param->pixel_param.pixel_pixel_number = param->width;

	//cofigure alig
	param->alig_param.alig_work_mode = param->mode;
	param->alig_param.alig_hsize = param->width;
	param->alig_param.alig_vsize = param->height;

	rtn = adap_frontend_init(a_dev);
	if (rtn)
		return rtn;

	if (g_info->user)
		return rtn;

	rtn = adap_reader_init(a_dev);
	if (rtn)
		return rtn;

	rtn = adap_pixel_init(a_dev);
	if (rtn)
		return rtn;

	rtn = adap_align_init(a_dev);
	if (rtn)
		return rtn;

	pr_info("ADAP%u: hw init\n", adap_dev->index);

#ifdef T7C_CHIP
	module_update_bits(a_dev, ALIGN_MD, MIPI_ADAPT_FE_MUX_CTL0, adap_dev->index, 24, 4);
	module_update_bits(a_dev, ALIGN_MD, MIPI_ADAPT_FE_MUX_CTL0, adap_dev->index, 0, 2);

	module_update_bits(a_dev, ALIGN_MD, MIPI_ADAPT_FE_MUX_CTL9, adap_dev->index * 6, 2, 19);
	module_update_bits(a_dev, ALIGN_MD, MIPI_ADAPT_FE_MUX_CTL9, adap_dev->index, 0, 2);
#endif

	return rtn;
}

static int adap_fe_set_fmt(struct aml_video *video, struct aml_format *fmt)
{
	return 0;
}

static int adap_fe_cfg_buf(struct aml_video *video, struct aml_buffer *buff)
{
	int module = FRONTEND_MD;
	u32 addr = buff->addr[AML_PLANE_A];
	struct adapter_dev_t *adap_dev = video->priv;

	switch (adap_dev->index) {
	case 0:
		module = FRONTEND_MD;
		break;
	case 1:
		module = FRONTEND1_MD;
		break;
	case 2:
		module = FRONTEND2_MD;
		break;
	case 3:
		module = FRONTEND3_MD;
		break;
	default:
		break;
	}

	module_reg_write(video->priv, module, CSI2_DDR_START_PIX, addr);
	module_reg_write(video->priv, module, CSI2_DDR_START_PIX_ALT, addr);

	return 0;
}

static int adap_wdr_cfg_buf(void *a_dev)
{
	struct adapter_dev_t *adap_dev = a_dev;
	struct adapter_dev_param *param = &adap_dev->param;
	int module = FRONTEND_MD;

	switch (adap_dev->index) {
	case 0:
		module = FRONTEND_MD;
		break;
	case 1:
		module = FRONTEND1_MD;
		break;
	case 2:
		module = FRONTEND2_MD;
		break;
	case 3:
		module = FRONTEND3_MD;
		break;
	default:
		break;
	}

	module_reg_write(a_dev, module, CSI2_DDR_START_PIX, param->ddr_buf[0].addr[0]);
	module_reg_write(a_dev, module, CSI2_DDR_START_PIX_ALT, param->ddr_buf[0].addr[0]);
	module_reg_write(a_dev, module, CSI2_DDR_START_PIX_B, param->ddr_buf[0].addr[0]);
	module_reg_write(a_dev, module, CSI2_DDR_START_PIX_B_ALT, param->ddr_buf[0].addr[0]);
	module_reg_write(a_dev, module, CSI2_DDR_START_OTHER, param->ddr_buf[0].addr[0]);
	module_reg_write(a_dev, module, CSI2_DDR_START_OTHER_ALT, param->ddr_buf[0].addr[0]);

	if (HDR_LOOPBACK_MODE)
		module_reg_write(a_dev, module, CSI2_DDR_LOOP_LINES_PIX, 119);

	module = READER_MD;

	module_reg_write(a_dev, module, MIPI_ADAPT_DDR_RD0_CNTL2, param->ddr_buf[0].addr[0]);
	module_reg_write(a_dev, module, MIPI_ADAPT_DDR_RD0_CNTL3, param->ddr_buf[0].addr[0]);
	module_reg_write(a_dev, module, MIPI_ADAPT_DDR_RD1_CNTL2, param->ddr_buf[0].addr[0]);
	module_reg_write(a_dev, module, MIPI_ADAPT_DDR_RD1_CNTL3, param->ddr_buf[0].addr[0]);

	if (HDR_LOOPBACK_MODE) {
		module_reg_write(a_dev, module, MIPI_ADAPT_DDR_RD1_CNTL5, param->ddr_buf[0].addr[0] + (param->rd_param.rd_mem_line_stride << 4) * 120);
		module_reg_write(a_dev, module, MIPI_ADAPT_DDR_RD1_CNTL6, param->ddr_buf[0].addr[0] + (param->rd_param.rd_mem_line_stride << 4) * 120);
	}

	return 0;
}

static int adap_rd_set_fmt(struct aml_video *video, struct aml_format *fmt)
{
	int temp = 0;
	int pixel_bit = 0;
	int multiple = 0;
	int hsize_org = 0;
	int hsize_blk = 0;
	int hsize_isp = 0;
	int hsize_mipi = 0;
	int module = 0;
	struct adapter_dev_t *adap_dev = video->priv;
	struct adapter_dev_param *param = &adap_dev->param;

	if (adap_dev->index != 0)
		return 0;

	switch (fmt->code) {
	case ADAP_RAW6:
		pixel_bit = 6;
		multiple = 128;
	break;
	case ADAP_RAW8:
		pixel_bit = 8;
		multiple = 128;
	break;
	case ADAP_RAW10:
		pixel_bit = 10;
		multiple = 640;
	break;
	case ADAP_RAW12:
		pixel_bit = 12;
		multiple = 384;
	break;
	case ADAP_RAW14:
		pixel_bit = 14;
		multiple = 128;
	break;
	}

	hsize_org = fmt->width;
	if ((hsize_org / 2) % 6 == 0)
		hsize_blk = (hsize_org / 2) / 6;
	else
		hsize_blk = (hsize_org / 2) / 6 + 1;

	hsize_isp = (hsize_org / 2) + hsize_blk + 20;

	if ((hsize_isp * pixel_bit) % multiple == 0)
		hsize_mipi = hsize_isp;
	else {
		temp = hsize_isp * pixel_bit / multiple + 1;
		hsize_mipi = temp * multiple /pixel_bit;
	}

	param->hsize_mipi = hsize_mipi;
	param->pixel_bit = pixel_bit;

	module = PIXEL_MD;
	module_update_bits(video->priv, module, MIPI_ADAPT_PIXEL0_CNTL2, hsize_mipi, 15, 13);

	module = ALIGN_MD;
	module_update_bits(video->priv, module, MIPI_ADAPT_ALIG_CNTL1, hsize_mipi, 16, 16);

	module = READER_MD;
	temp = hsize_mipi * pixel_bit / 128;
	module_update_bits(video->priv, module, MIPI_ADAPT_DDR_RD0_CNTL1, temp, 0, 10);

	module = PIXEL_MD;
	temp = hsize_mipi * pixel_bit / 128;
	module_update_bits(video->priv, module, MIPI_ADAPT_PIXEL0_CNTL2, temp, 0, 10);

	adap_ddr_mode_cfg(video->priv);

	pr_info("ADAP%u: hsize_mipi %d, pixel_bit %d, line_size %d\n",
				adap_dev->index, hsize_mipi, pixel_bit, temp);

	return 0;
}

static int adap_rd_cfg_buf(struct aml_video *video, struct aml_buffer *buff)
{
	int module = READER_MD;
	u32 addr = buff->addr[AML_PLANE_B];

	module_reg_write(video->priv, module, MIPI_ADAPT_DDR_RD0_CNTL2, addr);
	module_reg_write(video->priv, module, MIPI_ADAPT_DDR_RD0_CNTL3, addr);

	module_update_bits(video->priv, module, MIPI_ADAPT_DDR_RD0_CNTL0, 1, 25, 1);
	module_update_bits(video->priv, module, MIPI_ADAPT_DDR_RD0_CNTL0, 0, 25, 1);

	return 0;
}

static void adap_fe_enable(struct aml_video *video)
{
	adap_frontend_start(video->priv);
}

static void adap_fe_disable(struct aml_video *video)
{
	adap_frontend_stop(video->priv);
}

static void adap_rd_enable(struct aml_video *video)
{
	adap_align_start(video->priv);
	adap_pixel_start(video->priv);
	adap_reader_start(video->priv);
}

static void adap_rd_disable(struct aml_video *video)
{
	return;
}

static void adap_hw_offline(void *a_dev)
{
	struct adapter_dev_t *adap_dev = a_dev;

	adap_reader_init(a_dev);
	adap_pixel_init(a_dev);
	adap_align_init(a_dev);

	adap_ddr_mode_cfg(a_dev);
}

static int adap_hw_irq_handler(void *a_dev)
{
	return 0;
}

static int adap_hw_irq_status(void *a_dev)
{
	u32 val = 0;
	struct adapter_dev_t *adap_dev = a_dev;

	module_reg_read(a_dev, adap_dev->index, CSI2_INTERRUPT_CTRL_STAT, &val);
	module_reg_write(a_dev, adap_dev->index, CSI2_INTERRUPT_CTRL_STAT, val);

	return val;
}

static void adap_hw_irq_disable(void *a_dev)
{
	struct adapter_dev_t *adap_dev = a_dev;
	struct adapter_dev_param *param = &adap_dev->param;

	if ((param->mode == MODE_MIPI_RAW_SDR_DIRCT) ||
		(param->mode == MODE_MIPI_RAW_HDR_DDR_DIRCT) ||
		(param->mode == MODE_MIPI_YUV_SDR_DIRCT))
		return;

	module_update_bits(a_dev, adap_dev->index, CSI2_INTERRUPT_CTRL_STAT, 0, 2, 1); //fe wr done

	//module_update_bits(a_dev, ISP_TOP_OFFSET, MIPI_TOP_ISP_PENDING_MASK0, 0, ALIGN_FRAME_END, 1); //align done
}

static void adap_hw_irq_enable(void *a_dev)
{
	struct adapter_dev_t *adap_dev = a_dev;
	struct adapter_dev_param *param = &adap_dev->param;

	if ((param->mode == MODE_MIPI_RAW_SDR_DIRCT) ||
		(param->mode == MODE_MIPI_RAW_HDR_DDR_DIRCT) ||
		(param->mode == MODE_MIPI_YUV_SDR_DIRCT))
		return;

	module_update_bits(a_dev, adap_dev->index, CSI2_INTERRUPT_CTRL_STAT, 1, 2, 1); //fe wr done

	//module_update_bits(a_dev, ISP_TOP_OFFSET, MIPI_TOP_ISP_PENDING_MASK0, 1, ALIGN_FRAME_END, 1); //align done
}

static void adap_hw_reset(void *a_dev)
{
	struct adapter_dev_t *adap_dev = a_dev;

	adap_module_reset(a_dev);

	pr_info("ADAP%u: hw reset\n", adap_dev->index);
}

static u64 adap_hw_timestamp(void *a_dev)
{
	u64 sof_count = 0;

	return sof_count;
}

static int adap_hw_start(void *a_dev)
{
	struct adapter_dev_t *adap_dev = a_dev;

	if (adap_dev->param.mode == MODE_MIPI_RAW_SDR_DIRCT ||
		adap_dev->param.mode == MODE_MIPI_RAW_HDR_DDR_DIRCT ||
		adap_dev->param.mode == MODE_MIPI_YUV_SDR_DIRCT) {
		adap_align_start(a_dev);
		adap_pixel_start(a_dev);
		adap_reader_start(a_dev);
	} else
		adap_ddr_mode_cfg(a_dev);

	adap_frontend_start(a_dev);

	pr_info("ADAP%u: hw start\n", adap_dev->index);

	return 0;
}

static void adap_hw_stop(void *a_dev)
{
	struct adapter_dev_t *adap_dev = a_dev;

	adap_frontend_stop(a_dev);

	pr_info("ADAP%u: hw stop\n", adap_dev->index);
}

const struct adapter_dev_ops adap_dev_hw_ops = {
	.hw_init = adap_hw_init,
	.hw_reset = adap_hw_reset,
	.hw_start = adap_hw_start,
	.hw_stop = adap_hw_stop,
	.hw_fe_cfg_buf = adap_fe_cfg_buf,
	.hw_fe_set_fmt = adap_fe_set_fmt,
	.hw_fe_enable = adap_fe_enable,
	.hw_fe_disable = adap_fe_disable,
	.hw_rd_set_fmt = adap_rd_set_fmt,
	.hw_rd_cfg_buf = adap_rd_cfg_buf,
	.hw_rd_enable = adap_rd_enable,
	.hw_rd_disable = adap_rd_disable,
	.hw_irq_handler = adap_hw_irq_handler,
	.hw_interrupt_status = adap_hw_irq_status,
	.hw_timestamp = adap_hw_timestamp,
	.hw_wdr_cfg_buf = adap_wdr_cfg_buf,
	.hw_irq_en = adap_hw_irq_enable,
	.hw_irq_dis = adap_hw_irq_disable,
	.hw_offline_mode = adap_hw_offline,
};
