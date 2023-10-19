// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/video_sink/video.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#endif
#include "set_hdr2_v0.h"
#include "arch/vpp_hdr_regs.h"
#include "arch/vpp_s5_hdr_regs.h"
#include "arch/vpp_regs.h"
#include "arch/vpp_dolbyvision_regs.h"
#include "reg_helper.h"
#include "hdr/gamut_convert.h"
#include "arch/vpp_hdr_regs.h"
#include "color/ai_color.h"

static uint cpu_write_lut = 1;
module_param(cpu_write_lut, uint, 0664);
MODULE_PARM_DESC(cpu_write_lut, "\n cpu_write_lut\n");

static uint dma_sel = 3;
module_param(dma_sel, uint, 0664);
MODULE_PARM_DESC(dma_sel, "\n dma_sel\n");

static uint dma_sel1 = 3;
module_param(dma_sel1, uint, 0664);
MODULE_PARM_DESC(dma_sel1, "\n dma_sel1\n");

/*in/out matrix*/
void s5_set_hdr_matrix(enum hdr_module_sel module_sel,
		    enum hdr_matrix_sel mtx_sel,
		    struct hdr_proc_mtx_param_s *hdr_mtx_param,
		    struct hdr10pgen_param_s *p_hdr10pgen_param,
		    struct hdr_proc_lut_param_s *hdr_lut_param,
		    enum vpp_index_e vpp_index)
{
	unsigned int MATRIXI_COEF00_01 = 0;
	unsigned int MATRIXI_COEF02_10 = 0;
	unsigned int MATRIXI_COEF11_12 = 0;
	unsigned int MATRIXI_COEF20_21 = 0;
	unsigned int MATRIXI_COEF22 = 0;
	unsigned int MATRIXI_COEF30_31 = 0;
	unsigned int MATRIXI_COEF32_40 = 0;
	unsigned int MATRIXI_COEF41_42 = 0;
	unsigned int MATRIXI_OFFSET0_1 = 0;
	unsigned int MATRIXI_OFFSET2 = 0;
	unsigned int MATRIXI_PRE_OFFSET0_1 = 0;
	unsigned int MATRIXI_PRE_OFFSET2 = 0;
	unsigned int MATRIXI_CLIP = 0;
	unsigned int MATRIXI_EN_CTRL = 0;

	unsigned int MATRIXO_COEF00_01 = 0;
	unsigned int MATRIXO_COEF02_10 = 0;
	unsigned int MATRIXO_COEF11_12 = 0;
	unsigned int MATRIXO_COEF20_21 = 0;
	unsigned int MATRIXO_COEF22 = 0;
	unsigned int MATRIXO_COEF30_31 = 0;
	unsigned int MATRIXO_COEF32_40 = 0;
	unsigned int MATRIXO_COEF41_42 = 0;
	unsigned int MATRIXO_OFFSET0_1 = 0;
	unsigned int MATRIXO_OFFSET2 = 0;
	unsigned int MATRIXO_PRE_OFFSET0_1 = 0;
	unsigned int MATRIXO_PRE_OFFSET2 = 0;
	unsigned int MATRIXO_CLIP = 0;
	unsigned int MATRIXO_EN_CTRL = 0;

	unsigned int CGAIN_OFFT = 0;
	unsigned int CGAIN_COEF0 = 0;
	unsigned int CGAIN_COEF1 = 0;
	unsigned int ADPS_CTRL = 0;
	unsigned int ADPS_ALPHA0 = 0;
	unsigned int ADPS_ALPHA1 = 0;
	unsigned int ADPS_BETA0 = 0;
	unsigned int ADPS_BETA1 = 0;
	unsigned int ADPS_BETA2 = 0;
	unsigned int ADPS_COEF0 = 0;
	unsigned int ADPS_COEF1 = 0;
	unsigned int GMUT_CTRL = 0;
	unsigned int GMUT_COEF0 = 0;
	unsigned int GMUT_COEF1 = 0;
	unsigned int GMUT_COEF2 = 0;
	unsigned int GMUT_COEF3 = 0;
	unsigned int GMUT_COEF4 = 0;

	unsigned int hdr_ctrl = 0;
	unsigned int hdr_clk_gate = 0;
	unsigned int cur_hdr_ctrl = 0;

	int adpscl_mode = 0;

	int c_gain_lim_coef[3];
	int gmut_coef[3][3];
	int gmut_shift;
	int adpscl_bypass[3];
	int adpscl_alpha[3] = {0, 0, 0};
	int adpscl_shift[3];
	int adpscl_ys_coef[3] = {
		269,
		694,
		61
	};
	int adpscl_beta[3];
	int adpscl_beta_s[3];

	int scale_shift = 0;

	int i = 0;
	int in_mtx[MTX_NUM_PARAM] = {
		1024, 0, 0,
		0, 1024, 0,
		0, 0, 1024,
		0, 0, 0,
		0, 0, 0,
		0
	};

	int out_mtx[MTX_NUM_PARAM] = {
		1024, 0, 0,
		0, 1024, 0,
		0, 0, 1024,
		0, 0, 0,
		0, 0, 0,
		0
	};
	int vpp_sel = 0;/*0xfe;*/

	if (module_sel == VD1_HDR) {
		MATRIXI_COEF00_01 = S5_VD1_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF00_01 = S5_VD1_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF02_10 = S5_VD1_HDR2_MATRIXI_COEF02_10;
		MATRIXI_COEF11_12 = S5_VD1_HDR2_MATRIXI_COEF11_12;
		MATRIXI_COEF20_21 = S5_VD1_HDR2_MATRIXI_COEF20_21;
		MATRIXI_COEF22 = S5_VD1_HDR2_MATRIXI_COEF22;
		MATRIXI_COEF30_31 = S5_VD1_HDR2_MATRIXI_COEF30_31;
		MATRIXI_COEF32_40 = S5_VD1_HDR2_MATRIXI_COEF32_40;
		MATRIXI_COEF41_42 = S5_VD1_HDR2_MATRIXI_COEF41_42;
		MATRIXI_OFFSET0_1 = S5_VD1_HDR2_MATRIXI_OFFSET0_1;
		MATRIXI_OFFSET2 = S5_VD1_HDR2_MATRIXI_OFFSET2;
		MATRIXI_PRE_OFFSET0_1 = S5_VD1_HDR2_MATRIXI_PRE_OFFSET0_1;
		MATRIXI_PRE_OFFSET2 = S5_VD1_HDR2_MATRIXI_PRE_OFFSET2;
		MATRIXI_CLIP = S5_VD1_HDR2_MATRIXI_CLIP;
		MATRIXI_EN_CTRL = S5_VD1_HDR2_MATRIXI_EN_CTRL;

		MATRIXO_COEF00_01 = S5_VD1_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF00_01 = S5_VD1_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF02_10 = S5_VD1_HDR2_MATRIXO_COEF02_10;
		MATRIXO_COEF11_12 = S5_VD1_HDR2_MATRIXO_COEF11_12;
		MATRIXO_COEF20_21 = S5_VD1_HDR2_MATRIXO_COEF20_21;
		MATRIXO_COEF22 = S5_VD1_HDR2_MATRIXO_COEF22;
		MATRIXO_COEF30_31 = S5_VD1_HDR2_MATRIXO_COEF30_31;
		MATRIXO_COEF32_40 = S5_VD1_HDR2_MATRIXO_COEF32_40;
		MATRIXO_COEF41_42 = S5_VD1_HDR2_MATRIXO_COEF41_42;
		MATRIXO_OFFSET0_1 = S5_VD1_HDR2_MATRIXO_OFFSET0_1;
		MATRIXO_OFFSET2 = S5_VD1_HDR2_MATRIXO_OFFSET2;
		MATRIXO_PRE_OFFSET0_1 = S5_VD1_HDR2_MATRIXO_PRE_OFFSET0_1;
		MATRIXO_PRE_OFFSET2 = S5_VD1_HDR2_MATRIXO_PRE_OFFSET2;
		MATRIXO_CLIP = S5_VD1_HDR2_MATRIXO_CLIP;
		MATRIXO_EN_CTRL = S5_VD1_HDR2_MATRIXO_EN_CTRL;

		CGAIN_OFFT = S5_VD1_HDR2_CGAIN_OFFT;
		CGAIN_COEF0 = S5_VD1_HDR2_CGAIN_COEF0;
		CGAIN_COEF1 = S5_VD1_HDR2_CGAIN_COEF1;
		ADPS_CTRL = S5_VD1_HDR2_ADPS_CTRL;
		ADPS_ALPHA0 = S5_VD1_HDR2_ADPS_ALPHA0;
		ADPS_ALPHA1 = S5_VD1_HDR2_ADPS_ALPHA1;
		ADPS_BETA0 = S5_VD1_HDR2_ADPS_BETA0;
		ADPS_BETA1 = S5_VD1_HDR2_ADPS_BETA1;
		ADPS_BETA2 = S5_VD1_HDR2_ADPS_BETA2;
		ADPS_COEF0 = S5_VD1_HDR2_ADPS_COEF0;
		ADPS_COEF1 = S5_VD1_HDR2_ADPS_COEF1;
		GMUT_CTRL = S5_VD1_HDR2_GMUT_CTRL;
		GMUT_COEF0 = S5_VD1_HDR2_GMUT_COEF0;
		GMUT_COEF1 = S5_VD1_HDR2_GMUT_COEF1;
		GMUT_COEF2 = S5_VD1_HDR2_GMUT_COEF2;
		GMUT_COEF3 = S5_VD1_HDR2_GMUT_COEF3;
		GMUT_COEF4 = S5_VD1_HDR2_GMUT_COEF4;

		hdr_ctrl = S5_VD1_HDR2_CTRL;
		hdr_clk_gate = S5_VD1_HDR2_CLK_GATE;
	} else if (module_sel == S5_VD1_SLICE1) {
		MATRIXI_COEF00_01 = S5_VD1_SLICE1_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF00_01 = S5_VD1_SLICE1_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF02_10 = S5_VD1_SLICE1_HDR2_MATRIXI_COEF02_10;
		MATRIXI_COEF11_12 = S5_VD1_SLICE1_HDR2_MATRIXI_COEF11_12;
		MATRIXI_COEF20_21 = S5_VD1_SLICE1_HDR2_MATRIXI_COEF20_21;
		MATRIXI_COEF22 = S5_VD1_SLICE1_HDR2_MATRIXI_COEF22;
		MATRIXI_COEF30_31 = S5_VD1_SLICE1_HDR2_MATRIXI_COEF30_31;
		MATRIXI_COEF32_40 = S5_VD1_SLICE1_HDR2_MATRIXI_COEF32_40;
		MATRIXI_COEF41_42 = S5_VD1_SLICE1_HDR2_MATRIXI_COEF41_42;
		MATRIXI_OFFSET0_1 = S5_VD1_SLICE1_HDR2_MATRIXI_OFFSET0_1;
		MATRIXI_OFFSET2 = S5_VD1_SLICE1_HDR2_MATRIXI_OFFSET2;
		MATRIXI_PRE_OFFSET0_1 = S5_VD1_SLICE1_HDR2_MATRIXI_PRE_OFFSET0_1;
		MATRIXI_PRE_OFFSET2 = S5_VD1_SLICE1_HDR2_MATRIXI_PRE_OFFSET2;
		MATRIXI_CLIP = S5_VD1_SLICE1_HDR2_MATRIXI_CLIP;
		MATRIXI_EN_CTRL = S5_VD1_SLICE1_HDR2_MATRIXI_EN_CTRL;

		MATRIXO_COEF00_01 = S5_VD1_SLICE1_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF00_01 = S5_VD1_SLICE1_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF02_10 = S5_VD1_SLICE1_HDR2_MATRIXO_COEF02_10;
		MATRIXO_COEF11_12 = S5_VD1_SLICE1_HDR2_MATRIXO_COEF11_12;
		MATRIXO_COEF20_21 = S5_VD1_SLICE1_HDR2_MATRIXO_COEF20_21;
		MATRIXO_COEF22 = S5_VD1_SLICE1_HDR2_MATRIXO_COEF22;
		MATRIXO_COEF30_31 = S5_VD1_SLICE1_HDR2_MATRIXO_COEF30_31;
		MATRIXO_COEF32_40 = S5_VD1_SLICE1_HDR2_MATRIXO_COEF32_40;
		MATRIXO_COEF41_42 = S5_VD1_SLICE1_HDR2_MATRIXO_COEF41_42;
		MATRIXO_OFFSET0_1 = S5_VD1_SLICE1_HDR2_MATRIXO_OFFSET0_1;
		MATRIXO_OFFSET2 = S5_VD1_SLICE1_HDR2_MATRIXO_OFFSET2;
		MATRIXO_PRE_OFFSET0_1 = S5_VD1_SLICE1_HDR2_MATRIXO_PRE_OFFSET0_1;
		MATRIXO_PRE_OFFSET2 = S5_VD1_SLICE1_HDR2_MATRIXO_PRE_OFFSET2;
		MATRIXO_CLIP = S5_VD1_SLICE1_HDR2_MATRIXO_CLIP;
		MATRIXO_EN_CTRL = S5_VD1_SLICE1_HDR2_MATRIXO_EN_CTRL;

		CGAIN_OFFT = S5_VD1_SLICE1_HDR2_CGAIN_OFFT;
		CGAIN_COEF0 = S5_VD1_SLICE1_HDR2_CGAIN_COEF0;
		CGAIN_COEF1 = S5_VD1_SLICE1_HDR2_CGAIN_COEF1;
		ADPS_CTRL = S5_VD1_SLICE1_HDR2_ADPS_CTRL;
		ADPS_ALPHA0 = S5_VD1_SLICE1_HDR2_ADPS_ALPHA0;
		ADPS_ALPHA1 = S5_VD1_SLICE1_HDR2_ADPS_ALPHA1;
		ADPS_BETA0 = S5_VD1_SLICE1_HDR2_ADPS_BETA0;
		ADPS_BETA1 = S5_VD1_SLICE1_HDR2_ADPS_BETA1;
		ADPS_BETA2 = S5_VD1_SLICE1_HDR2_ADPS_BETA2;
		ADPS_COEF0 = S5_VD1_SLICE1_HDR2_ADPS_COEF0;
		ADPS_COEF1 = S5_VD1_SLICE1_HDR2_ADPS_COEF1;
		GMUT_CTRL = S5_VD1_SLICE1_HDR2_GMUT_CTRL;
		GMUT_COEF0 = S5_VD1_SLICE1_HDR2_GMUT_COEF0;
		GMUT_COEF1 = S5_VD1_SLICE1_HDR2_GMUT_COEF1;
		GMUT_COEF2 = S5_VD1_SLICE1_HDR2_GMUT_COEF2;
		GMUT_COEF3 = S5_VD1_SLICE1_HDR2_GMUT_COEF3;
		GMUT_COEF4 = S5_VD1_SLICE1_HDR2_GMUT_COEF4;

		hdr_ctrl = S5_VD1_SLICE1_HDR2_CTRL;
		hdr_clk_gate = S5_VD1_SLICE1_HDR2_CLK_GATE;
	} else if (module_sel == S5_VD1_SLICE2) {
		MATRIXI_COEF00_01 = S5_VD1_SLICE2_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF00_01 = S5_VD1_SLICE2_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF02_10 = S5_VD1_SLICE2_HDR2_MATRIXI_COEF02_10;
		MATRIXI_COEF11_12 = S5_VD1_SLICE2_HDR2_MATRIXI_COEF11_12;
		MATRIXI_COEF20_21 = S5_VD1_SLICE2_HDR2_MATRIXI_COEF20_21;
		MATRIXI_COEF22 = S5_VD1_SLICE2_HDR2_MATRIXI_COEF22;
		MATRIXI_COEF30_31 = S5_VD1_SLICE2_HDR2_MATRIXI_COEF30_31;
		MATRIXI_COEF32_40 = S5_VD1_SLICE2_HDR2_MATRIXI_COEF32_40;
		MATRIXI_COEF41_42 = S5_VD1_SLICE2_HDR2_MATRIXI_COEF41_42;
		MATRIXI_OFFSET0_1 = S5_VD1_SLICE2_HDR2_MATRIXI_OFFSET0_1;
		MATRIXI_OFFSET2 = S5_VD1_SLICE2_HDR2_MATRIXI_OFFSET2;
		MATRIXI_PRE_OFFSET0_1 = S5_VD1_SLICE2_HDR2_MATRIXI_PRE_OFFSET0_1;
		MATRIXI_PRE_OFFSET2 = S5_VD1_SLICE2_HDR2_MATRIXI_PRE_OFFSET2;
		MATRIXI_CLIP = S5_VD1_SLICE2_HDR2_MATRIXI_CLIP;
		MATRIXI_EN_CTRL = S5_VD1_SLICE2_HDR2_MATRIXI_EN_CTRL;

		MATRIXO_COEF00_01 = S5_VD1_SLICE2_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF00_01 = S5_VD1_SLICE2_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF02_10 = S5_VD1_SLICE2_HDR2_MATRIXO_COEF02_10;
		MATRIXO_COEF11_12 = S5_VD1_SLICE2_HDR2_MATRIXO_COEF11_12;
		MATRIXO_COEF20_21 = S5_VD1_SLICE2_HDR2_MATRIXO_COEF20_21;
		MATRIXO_COEF22 = S5_VD1_SLICE2_HDR2_MATRIXO_COEF22;
		MATRIXO_COEF30_31 = S5_VD1_SLICE2_HDR2_MATRIXO_COEF30_31;
		MATRIXO_COEF32_40 = S5_VD1_SLICE2_HDR2_MATRIXO_COEF32_40;
		MATRIXO_COEF41_42 = S5_VD1_SLICE2_HDR2_MATRIXO_COEF41_42;
		MATRIXO_OFFSET0_1 = S5_VD1_SLICE2_HDR2_MATRIXO_OFFSET0_1;
		MATRIXO_OFFSET2 = S5_VD1_SLICE2_HDR2_MATRIXO_OFFSET2;
		MATRIXO_PRE_OFFSET0_1 = S5_VD1_SLICE2_HDR2_MATRIXO_PRE_OFFSET0_1;
		MATRIXO_PRE_OFFSET2 = S5_VD1_SLICE2_HDR2_MATRIXO_PRE_OFFSET2;
		MATRIXO_CLIP = S5_VD1_SLICE2_HDR2_MATRIXO_CLIP;
		MATRIXO_EN_CTRL = S5_VD1_SLICE2_HDR2_MATRIXO_EN_CTRL;

		CGAIN_OFFT = S5_VD1_SLICE2_HDR2_CGAIN_OFFT;
		CGAIN_COEF0 = S5_VD1_SLICE2_HDR2_CGAIN_COEF0;
		CGAIN_COEF1 = S5_VD1_SLICE2_HDR2_CGAIN_COEF1;
		ADPS_CTRL = S5_VD1_SLICE2_HDR2_ADPS_CTRL;
		ADPS_ALPHA0 = S5_VD1_SLICE2_HDR2_ADPS_ALPHA0;
		ADPS_ALPHA1 = S5_VD1_SLICE2_HDR2_ADPS_ALPHA1;
		ADPS_BETA0 = S5_VD1_SLICE2_HDR2_ADPS_BETA0;
		ADPS_BETA1 = S5_VD1_SLICE2_HDR2_ADPS_BETA1;
		ADPS_BETA2 = S5_VD1_SLICE2_HDR2_ADPS_BETA2;
		ADPS_COEF0 = S5_VD1_SLICE2_HDR2_ADPS_COEF0;
		ADPS_COEF1 = S5_VD1_SLICE2_HDR2_ADPS_COEF1;
		GMUT_CTRL = S5_VD1_SLICE2_HDR2_GMUT_CTRL;
		GMUT_COEF0 = S5_VD1_SLICE2_HDR2_GMUT_COEF0;
		GMUT_COEF1 = S5_VD1_SLICE2_HDR2_GMUT_COEF1;
		GMUT_COEF2 = S5_VD1_SLICE2_HDR2_GMUT_COEF2;
		GMUT_COEF3 = S5_VD1_SLICE2_HDR2_GMUT_COEF3;
		GMUT_COEF4 = S5_VD1_SLICE2_HDR2_GMUT_COEF4;

		hdr_ctrl = S5_VD1_SLICE2_HDR2_CTRL;
		hdr_clk_gate = S5_VD1_SLICE2_HDR2_CLK_GATE;
	} else if (module_sel == S5_VD1_SLICE3) {
		MATRIXI_COEF00_01 = S5_VD1_SLICE3_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF00_01 = S5_VD1_SLICE3_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF02_10 = S5_VD1_SLICE3_HDR2_MATRIXI_COEF02_10;
		MATRIXI_COEF11_12 = S5_VD1_SLICE3_HDR2_MATRIXI_COEF11_12;
		MATRIXI_COEF20_21 = S5_VD1_SLICE3_HDR2_MATRIXI_COEF20_21;
		MATRIXI_COEF22 = S5_VD1_SLICE3_HDR2_MATRIXI_COEF22;
		MATRIXI_COEF30_31 = S5_VD1_SLICE3_HDR2_MATRIXI_COEF30_31;
		MATRIXI_COEF32_40 = S5_VD1_SLICE3_HDR2_MATRIXI_COEF32_40;
		MATRIXI_COEF41_42 = S5_VD1_SLICE3_HDR2_MATRIXI_COEF41_42;
		MATRIXI_OFFSET0_1 = S5_VD1_SLICE3_HDR2_MATRIXI_OFFSET0_1;
		MATRIXI_OFFSET2 = S5_VD1_SLICE3_HDR2_MATRIXI_OFFSET2;
		MATRIXI_PRE_OFFSET0_1 = S5_VD1_SLICE3_HDR2_MATRIXI_PRE_OFFSET0_1;
		MATRIXI_PRE_OFFSET2 = S5_VD1_SLICE3_HDR2_MATRIXI_PRE_OFFSET2;
		MATRIXI_CLIP = S5_VD1_SLICE3_HDR2_MATRIXI_CLIP;
		MATRIXI_EN_CTRL = S5_VD1_SLICE3_HDR2_MATRIXI_EN_CTRL;

		MATRIXO_COEF00_01 = S5_VD1_SLICE3_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF00_01 = S5_VD1_SLICE3_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF02_10 = S5_VD1_SLICE3_HDR2_MATRIXO_COEF02_10;
		MATRIXO_COEF11_12 = S5_VD1_SLICE3_HDR2_MATRIXO_COEF11_12;
		MATRIXO_COEF20_21 = S5_VD1_SLICE3_HDR2_MATRIXO_COEF20_21;
		MATRIXO_COEF22 = S5_VD1_SLICE3_HDR2_MATRIXO_COEF22;
		MATRIXO_COEF30_31 = S5_VD1_SLICE3_HDR2_MATRIXO_COEF30_31;
		MATRIXO_COEF32_40 = S5_VD1_SLICE3_HDR2_MATRIXO_COEF32_40;
		MATRIXO_COEF41_42 = S5_VD1_SLICE3_HDR2_MATRIXO_COEF41_42;
		MATRIXO_OFFSET0_1 = S5_VD1_SLICE3_HDR2_MATRIXO_OFFSET0_1;
		MATRIXO_OFFSET2 = S5_VD1_SLICE3_HDR2_MATRIXO_OFFSET2;
		MATRIXO_PRE_OFFSET0_1 = S5_VD1_SLICE3_HDR2_MATRIXO_PRE_OFFSET0_1;
		MATRIXO_PRE_OFFSET2 = S5_VD1_SLICE3_HDR2_MATRIXO_PRE_OFFSET2;
		MATRIXO_CLIP = S5_VD1_SLICE3_HDR2_MATRIXO_CLIP;
		MATRIXO_EN_CTRL = S5_VD1_SLICE3_HDR2_MATRIXO_EN_CTRL;

		CGAIN_OFFT = S5_VD1_SLICE3_HDR2_CGAIN_OFFT;
		CGAIN_COEF0 = S5_VD1_SLICE3_HDR2_CGAIN_COEF0;
		CGAIN_COEF1 = S5_VD1_SLICE3_HDR2_CGAIN_COEF1;
		ADPS_CTRL = S5_VD1_SLICE3_HDR2_ADPS_CTRL;
		ADPS_ALPHA0 = S5_VD1_SLICE3_HDR2_ADPS_ALPHA0;
		ADPS_ALPHA1 = S5_VD1_SLICE3_HDR2_ADPS_ALPHA1;
		ADPS_BETA0 = S5_VD1_SLICE3_HDR2_ADPS_BETA0;
		ADPS_BETA1 = S5_VD1_SLICE3_HDR2_ADPS_BETA1;
		ADPS_BETA2 = S5_VD1_SLICE3_HDR2_ADPS_BETA2;
		ADPS_COEF0 = S5_VD1_SLICE3_HDR2_ADPS_COEF0;
		ADPS_COEF1 = S5_VD1_SLICE3_HDR2_ADPS_COEF1;
		GMUT_CTRL = S5_VD1_SLICE3_HDR2_GMUT_CTRL;
		GMUT_COEF0 = S5_VD1_SLICE3_HDR2_GMUT_COEF0;
		GMUT_COEF1 = S5_VD1_SLICE3_HDR2_GMUT_COEF1;
		GMUT_COEF2 = S5_VD1_SLICE3_HDR2_GMUT_COEF2;
		GMUT_COEF3 = S5_VD1_SLICE3_HDR2_GMUT_COEF3;
		GMUT_COEF4 = S5_VD1_SLICE3_HDR2_GMUT_COEF4;

		hdr_ctrl = S5_VD1_SLICE3_HDR2_CTRL;
		hdr_clk_gate = S5_VD1_SLICE3_HDR2_CLK_GATE;
	} else if (module_sel == VD2_HDR) {
		MATRIXI_COEF00_01 = S5_VD2_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF00_01 = S5_VD2_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF02_10 = S5_VD2_HDR2_MATRIXI_COEF02_10;
		MATRIXI_COEF11_12 = S5_VD2_HDR2_MATRIXI_COEF11_12;
		MATRIXI_COEF20_21 = S5_VD2_HDR2_MATRIXI_COEF20_21;
		MATRIXI_COEF22 = S5_VD2_HDR2_MATRIXI_COEF22;
		MATRIXI_COEF30_31 = S5_VD2_HDR2_MATRIXI_COEF30_31;
		MATRIXI_COEF32_40 = S5_VD2_HDR2_MATRIXI_COEF32_40;
		MATRIXI_COEF41_42 = S5_VD2_HDR2_MATRIXI_COEF41_42;
		MATRIXI_OFFSET0_1 = S5_VD2_HDR2_MATRIXI_OFFSET0_1;
		MATRIXI_OFFSET2 = S5_VD2_HDR2_MATRIXI_OFFSET2;
		MATRIXI_PRE_OFFSET0_1 = S5_VD2_HDR2_MATRIXI_PRE_OFFSET0_1;
		MATRIXI_PRE_OFFSET2 = S5_VD2_HDR2_MATRIXI_PRE_OFFSET2;
		MATRIXI_CLIP = S5_VD2_HDR2_MATRIXI_CLIP;
		MATRIXI_EN_CTRL = S5_VD2_HDR2_MATRIXI_EN_CTRL;

		MATRIXO_COEF00_01 = S5_VD2_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF00_01 = S5_VD2_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF02_10 = S5_VD2_HDR2_MATRIXO_COEF02_10;
		MATRIXO_COEF11_12 = S5_VD2_HDR2_MATRIXO_COEF11_12;
		MATRIXO_COEF20_21 = S5_VD2_HDR2_MATRIXO_COEF20_21;
		MATRIXO_COEF22 = S5_VD2_HDR2_MATRIXO_COEF22;
		MATRIXO_COEF30_31 = S5_VD2_HDR2_MATRIXO_COEF30_31;
		MATRIXO_COEF32_40 = S5_VD2_HDR2_MATRIXO_COEF32_40;
		MATRIXO_COEF41_42 = S5_VD2_HDR2_MATRIXO_COEF41_42;
		MATRIXO_OFFSET0_1 = S5_VD2_HDR2_MATRIXO_OFFSET0_1;
		MATRIXO_OFFSET2 = S5_VD2_HDR2_MATRIXO_OFFSET2;
		MATRIXO_PRE_OFFSET0_1 = S5_VD2_HDR2_MATRIXO_PRE_OFFSET0_1;
		MATRIXO_PRE_OFFSET2 = S5_VD2_HDR2_MATRIXO_PRE_OFFSET2;
		MATRIXO_CLIP = S5_VD2_HDR2_MATRIXO_CLIP;
		MATRIXO_EN_CTRL = S5_VD2_HDR2_MATRIXO_EN_CTRL;

		CGAIN_OFFT = S5_VD2_HDR2_CGAIN_OFFT;
		CGAIN_COEF0 = S5_VD2_HDR2_CGAIN_COEF0;
		CGAIN_COEF1 = S5_VD2_HDR2_CGAIN_COEF1;
		ADPS_CTRL = S5_VD2_HDR2_ADPS_CTRL;
		ADPS_ALPHA0 = S5_VD2_HDR2_ADPS_ALPHA0;
		ADPS_ALPHA1 = S5_VD2_HDR2_ADPS_ALPHA1;
		ADPS_BETA0 = S5_VD2_HDR2_ADPS_BETA0;
		ADPS_BETA1 = S5_VD2_HDR2_ADPS_BETA1;
		ADPS_BETA2 = S5_VD2_HDR2_ADPS_BETA2;
		ADPS_COEF0 = S5_VD2_HDR2_ADPS_COEF0;
		ADPS_COEF1 = S5_VD2_HDR2_ADPS_COEF1;
		GMUT_CTRL = S5_VD2_HDR2_GMUT_CTRL;
		GMUT_COEF0 = S5_VD2_HDR2_GMUT_COEF0;
		GMUT_COEF1 = S5_VD2_HDR2_GMUT_COEF1;
		GMUT_COEF2 = S5_VD2_HDR2_GMUT_COEF2;
		GMUT_COEF3 = S5_VD2_HDR2_GMUT_COEF3;
		GMUT_COEF4 = S5_VD2_HDR2_GMUT_COEF4;

		hdr_ctrl = S5_VD2_HDR2_CTRL;
		hdr_clk_gate = S5_VD2_HDR2_CLK_GATE;
	} else if (module_sel == OSD1_HDR) {
		MATRIXI_COEF00_01 = S5_OSD1_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF00_01 = S5_OSD1_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF02_10 = S5_OSD1_HDR2_MATRIXI_COEF02_10;
		MATRIXI_COEF11_12 = S5_OSD1_HDR2_MATRIXI_COEF11_12;
		MATRIXI_COEF20_21 = S5_OSD1_HDR2_MATRIXI_COEF20_21;
		MATRIXI_COEF22 = S5_OSD1_HDR2_MATRIXI_COEF22;
		MATRIXI_COEF30_31 = S5_OSD1_HDR2_MATRIXI_COEF30_31;
		MATRIXI_COEF32_40 = S5_OSD1_HDR2_MATRIXI_COEF32_40;
		MATRIXI_COEF41_42 = S5_OSD1_HDR2_MATRIXI_COEF41_42;
		MATRIXI_OFFSET0_1 = S5_OSD1_HDR2_MATRIXI_OFFSET0_1;
		MATRIXI_OFFSET2 = S5_OSD1_HDR2_MATRIXI_OFFSET2;
		MATRIXI_PRE_OFFSET0_1 = S5_OSD1_HDR2_MATRIXI_PRE_OFFSET0_1;
		MATRIXI_PRE_OFFSET2 = S5_OSD1_HDR2_MATRIXI_PRE_OFFSET2;
		MATRIXI_CLIP = S5_OSD1_HDR2_MATRIXI_CLIP;
		MATRIXI_EN_CTRL = S5_OSD1_HDR2_MATRIXI_EN_CTRL;

		MATRIXO_COEF00_01 = S5_OSD1_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF00_01 = S5_OSD1_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF02_10 = S5_OSD1_HDR2_MATRIXO_COEF02_10;
		MATRIXO_COEF11_12 = S5_OSD1_HDR2_MATRIXO_COEF11_12;
		MATRIXO_COEF20_21 = S5_OSD1_HDR2_MATRIXO_COEF20_21;
		MATRIXO_COEF22 = S5_OSD1_HDR2_MATRIXO_COEF22;
		MATRIXO_COEF30_31 = S5_OSD1_HDR2_MATRIXO_COEF30_31;
		MATRIXO_COEF32_40 = S5_OSD1_HDR2_MATRIXO_COEF32_40;
		MATRIXO_COEF41_42 = S5_OSD1_HDR2_MATRIXO_COEF41_42;
		MATRIXO_OFFSET0_1 = S5_OSD1_HDR2_MATRIXO_OFFSET0_1;
		MATRIXO_OFFSET2 = S5_OSD1_HDR2_MATRIXO_OFFSET2;
		MATRIXO_PRE_OFFSET0_1 = S5_OSD1_HDR2_MATRIXO_PRE_OFFSET0_1;
		MATRIXO_PRE_OFFSET2 = S5_OSD1_HDR2_MATRIXO_PRE_OFFSET2;
		MATRIXO_CLIP = S5_OSD1_HDR2_MATRIXO_CLIP;
		MATRIXO_EN_CTRL = S5_OSD1_HDR2_MATRIXO_EN_CTRL;

		CGAIN_OFFT = S5_OSD1_HDR2_CGAIN_OFFT;
		CGAIN_COEF0 = S5_OSD1_HDR2_CGAIN_COEF0;
		CGAIN_COEF1 = S5_OSD1_HDR2_CGAIN_COEF1;
		ADPS_CTRL = S5_OSD1_HDR2_ADPS_CTRL;
		ADPS_ALPHA0 = S5_OSD1_HDR2_ADPS_ALPHA0;
		ADPS_ALPHA1 = S5_OSD1_HDR2_ADPS_ALPHA1;
		ADPS_BETA0 = S5_OSD1_HDR2_ADPS_BETA0;
		ADPS_BETA1 = S5_OSD1_HDR2_ADPS_BETA1;
		ADPS_BETA2 = S5_OSD1_HDR2_ADPS_BETA2;
		ADPS_COEF0 = S5_OSD1_HDR2_ADPS_COEF0;
		ADPS_COEF1 = S5_OSD1_HDR2_ADPS_COEF1;
		GMUT_CTRL = S5_OSD1_HDR2_GMUT_CTRL;
		GMUT_COEF0 = S5_OSD1_HDR2_GMUT_COEF0;
		GMUT_COEF1 = S5_OSD1_HDR2_GMUT_COEF1;
		GMUT_COEF2 = S5_OSD1_HDR2_GMUT_COEF2;
		GMUT_COEF3 = S5_OSD1_HDR2_GMUT_COEF3;
		GMUT_COEF4 = S5_OSD1_HDR2_GMUT_COEF4;

		hdr_ctrl = S5_OSD1_HDR2_CTRL;
		hdr_clk_gate = S5_OSD1_HDR2_CLK_GATE;
	} else if (module_sel == OSD3_HDR) {
		MATRIXI_COEF00_01 = S5_OSD3_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF00_01 = S5_OSD3_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF02_10 = S5_OSD3_HDR2_MATRIXI_COEF02_10;
		MATRIXI_COEF11_12 = S5_OSD3_HDR2_MATRIXI_COEF11_12;
		MATRIXI_COEF20_21 = S5_OSD3_HDR2_MATRIXI_COEF20_21;
		MATRIXI_COEF22 = S5_OSD3_HDR2_MATRIXI_COEF22;
		MATRIXI_COEF30_31 = S5_OSD3_HDR2_MATRIXI_COEF30_31;
		MATRIXI_COEF32_40 = S5_OSD3_HDR2_MATRIXI_COEF32_40;
		MATRIXI_COEF41_42 = S5_OSD3_HDR2_MATRIXI_COEF41_42;
		MATRIXI_OFFSET0_1 = S5_OSD3_HDR2_MATRIXI_OFFSET0_1;
		MATRIXI_OFFSET2 = S5_OSD3_HDR2_MATRIXI_OFFSET2;
		MATRIXI_PRE_OFFSET0_1 = S5_OSD3_HDR2_MATRIXI_PRE_OFFSET0_1;
		MATRIXI_PRE_OFFSET2 = S5_OSD3_HDR2_MATRIXI_PRE_OFFSET2;
		MATRIXI_CLIP = S5_OSD3_HDR2_MATRIXI_CLIP;
		MATRIXI_EN_CTRL = S5_OSD3_HDR2_MATRIXI_EN_CTRL;

		MATRIXO_COEF00_01 = S5_OSD3_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF00_01 = S5_OSD3_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF02_10 = S5_OSD3_HDR2_MATRIXO_COEF02_10;
		MATRIXO_COEF11_12 = S5_OSD3_HDR2_MATRIXO_COEF11_12;
		MATRIXO_COEF20_21 = S5_OSD3_HDR2_MATRIXO_COEF20_21;
		MATRIXO_COEF22 = S5_OSD3_HDR2_MATRIXO_COEF22;
		MATRIXO_COEF30_31 = S5_OSD3_HDR2_MATRIXO_COEF30_31;
		MATRIXO_COEF32_40 = S5_OSD3_HDR2_MATRIXO_COEF32_40;
		MATRIXO_COEF41_42 = S5_OSD3_HDR2_MATRIXO_COEF41_42;
		MATRIXO_OFFSET0_1 = S5_OSD3_HDR2_MATRIXO_OFFSET0_1;
		MATRIXO_OFFSET2 = S5_OSD3_HDR2_MATRIXO_OFFSET2;
		MATRIXO_PRE_OFFSET0_1 = S5_OSD3_HDR2_MATRIXO_PRE_OFFSET0_1;
		MATRIXO_PRE_OFFSET2 = S5_OSD3_HDR2_MATRIXO_PRE_OFFSET2;
		MATRIXO_CLIP = S5_OSD3_HDR2_MATRIXO_CLIP;
		MATRIXO_EN_CTRL = S5_OSD3_HDR2_MATRIXO_EN_CTRL;

		CGAIN_OFFT = S5_OSD3_HDR2_CGAIN_OFFT;
		CGAIN_COEF0 = S5_OSD3_HDR2_CGAIN_COEF0;
		CGAIN_COEF1 = S5_OSD3_HDR2_CGAIN_COEF1;
		ADPS_CTRL = S5_OSD3_HDR2_ADPS_CTRL;
		ADPS_ALPHA0 = S5_OSD3_HDR2_ADPS_ALPHA0;
		ADPS_ALPHA1 = S5_OSD3_HDR2_ADPS_ALPHA1;
		ADPS_BETA0 = S5_OSD3_HDR2_ADPS_BETA0;
		ADPS_BETA1 = S5_OSD3_HDR2_ADPS_BETA1;
		ADPS_BETA2 = S5_OSD3_HDR2_ADPS_BETA2;
		ADPS_COEF0 = S5_OSD3_HDR2_ADPS_COEF0;
		ADPS_COEF1 = S5_OSD3_HDR2_ADPS_COEF1;
		GMUT_CTRL = S5_OSD3_HDR2_GMUT_CTRL;
		GMUT_COEF0 = S5_OSD3_HDR2_GMUT_COEF0;
		GMUT_COEF1 = S5_OSD3_HDR2_GMUT_COEF1;
		GMUT_COEF2 = S5_OSD3_HDR2_GMUT_COEF2;
		GMUT_COEF3 = S5_OSD3_HDR2_GMUT_COEF3;
		GMUT_COEF4 = S5_OSD3_HDR2_GMUT_COEF4;

		hdr_ctrl = S5_OSD3_HDR2_CTRL;
		hdr_clk_gate = S5_OSD3_HDR2_CLK_GATE;
	} else if (module_sel == VDIN1_HDR) {
		MATRIXI_COEF00_01 = VDIN_PP_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF00_01 = VDIN_PP_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF02_10 = VDIN_PP_HDR2_MATRIXI_COEF02_10;
		MATRIXI_COEF11_12 = VDIN_PP_HDR2_MATRIXI_COEF11_12;
		MATRIXI_COEF20_21 = VDIN_PP_HDR2_MATRIXI_COEF20_21;
		MATRIXI_COEF22 = VDIN_PP_HDR2_MATRIXI_COEF22;
		MATRIXI_COEF30_31 = VDIN_PP_HDR2_MATRIXI_COEF30_31;
		MATRIXI_COEF32_40 = VDIN_PP_HDR2_MATRIXI_COEF32_40;
		MATRIXI_COEF41_42 = VDIN_PP_HDR2_MATRIXI_COEF41_42;
		MATRIXI_OFFSET0_1 = VDIN_PP_HDR2_MATRIXI_OFFSET0_1;
		MATRIXI_OFFSET2 = VDIN_PP_HDR2_MATRIXI_OFFSET2;
		MATRIXI_PRE_OFFSET0_1 = VDIN_PP_HDR2_MATRIXI_PRE_OFFSET0_1;
		MATRIXI_PRE_OFFSET2 = VDIN_PP_HDR2_MATRIXI_PRE_OFFSET2;
		MATRIXI_CLIP = VDIN_PP_HDR2_MATRIXI_CLIP;
		MATRIXI_EN_CTRL = VDIN_PP_HDR2_MATRIXI_EN_CTRL;

		MATRIXO_COEF00_01 = VDIN_PP_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF00_01 = VDIN_PP_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF02_10 = VDIN_PP_HDR2_MATRIXO_COEF02_10;
		MATRIXO_COEF11_12 = VDIN_PP_HDR2_MATRIXO_COEF11_12;
		MATRIXO_COEF20_21 = VDIN_PP_HDR2_MATRIXO_COEF20_21;
		MATRIXO_COEF22 = VDIN_PP_HDR2_MATRIXO_COEF22;
		MATRIXO_COEF30_31 = VDIN_PP_HDR2_MATRIXO_COEF30_31;
		MATRIXO_COEF32_40 = VDIN_PP_HDR2_MATRIXO_COEF32_40;
		MATRIXO_COEF41_42 = VDIN_PP_HDR2_MATRIXO_COEF41_42;
		MATRIXO_OFFSET0_1 = VDIN_PP_HDR2_MATRIXO_OFFSET0_1;
		MATRIXO_OFFSET2 = VDIN_PP_HDR2_MATRIXO_OFFSET2;
		MATRIXO_PRE_OFFSET0_1 = VDIN_PP_HDR2_MATRIXO_PRE_OFFSET0_1;
		MATRIXO_PRE_OFFSET2 = VDIN_PP_HDR2_MATRIXO_PRE_OFFSET2;
		MATRIXO_CLIP = VDIN_PP_HDR2_MATRIXO_CLIP;
		MATRIXO_EN_CTRL = VDIN_PP_HDR2_MATRIXO_EN_CTRL;

		CGAIN_OFFT = VDIN_PP_HDR2_CGAIN_OFFT;
		CGAIN_COEF0 = VDIN_PP_HDR2_CGAIN_COEF0;
		CGAIN_COEF1 = VDIN_PP_HDR2_CGAIN_COEF1;
		ADPS_CTRL = VDIN_PP_HDR2_ADPS_CTRL;
		ADPS_ALPHA0 = VDIN_PP_HDR2_ADPS_ALPHA0;
		ADPS_ALPHA1 = VDIN_PP_HDR2_ADPS_ALPHA1;
		ADPS_BETA0 = VDIN_PP_HDR2_ADPS_BETA0;
		ADPS_BETA1 = VDIN_PP_HDR2_ADPS_BETA1;
		ADPS_BETA2 = VDIN_PP_HDR2_ADPS_BETA2;
		ADPS_COEF0 = VDIN_PP_HDR2_ADPS_COEF0;
		ADPS_COEF1 = VDIN_PP_HDR2_ADPS_COEF1;
		GMUT_CTRL = VDIN_PP_HDR2_GMUT_CTRL;
		GMUT_COEF0 = VDIN_PP_HDR2_GMUT_COEF0;
		GMUT_COEF1 = VDIN_PP_HDR2_GMUT_COEF1;
		GMUT_COEF2 = VDIN_PP_HDR2_GMUT_COEF2;
		GMUT_COEF3 = VDIN_PP_HDR2_GMUT_COEF3;
		GMUT_COEF4 = VDIN_PP_HDR2_GMUT_COEF4;

		hdr_ctrl = VDIN_PP_HDR2_CTRL;
		hdr_clk_gate = VDIN_PP_HDR2_CLK_GATE;
		vpp_sel = 0xff;
	}

	if (!hdr_mtx_param ||
	    (!hdr_lut_param && mtx_sel == HDR_GAMUT_MTX))
		return;

	/* need change clock gate as freerun when mtx on directly, not rdma op */
	/* Now only operate osd1/vd1/vd2 hdr core */
	if ((get_cpu_type() <= MESON_CPU_MAJOR_ID_S5) &&
		(get_cpu_type() != MESON_CPU_MAJOR_ID_T3) &&
		(get_cpu_type() != MESON_CPU_MAJOR_ID_T5W)) {
		if (hdr_clk_gate != 0) {
			cur_hdr_ctrl =
				VSYNC_READ_VPP_REG_VPP_SEL(hdr_ctrl, vpp_sel);
			if (hdr_mtx_param->mtx_on && !(cur_hdr_ctrl & (1 << 13))) {
				WRITE_VPP_REG_BITS_S5(hdr_clk_gate, 0xaaa, 0, 12);
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(hdr_clk_gate,
					0xaaa, 0, 12, vpp_sel);
			}
		}
	}

	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(hdr_ctrl,
		 hdr_mtx_param->mtx_on, 13, 1, vpp_sel);

	/* recover the clock gate as auto gate by rdma op when mtx off */
	/* Now only operate osd1/vd1/vd2 hdr core */
	if ((get_cpu_type() <= MESON_CPU_MAJOR_ID_S5) &&
		(get_cpu_type() != MESON_CPU_MAJOR_ID_T3) &&
		(get_cpu_type() != MESON_CPU_MAJOR_ID_T5W)) {
		if (hdr_clk_gate != 0 && !hdr_mtx_param->mtx_on)
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(hdr_clk_gate,
				0, 0, 12, vpp_sel);
	}

	if (mtx_sel == HDR_IN_MTX) {
		for (i = 0; i < MTX_NUM_PARAM; i++)
			in_mtx[i] = hdr_mtx_param->mtx_in[i];

//#ifdef HDR2_PRINT
		pr_csc(64, "hdr: in_mtx %d %d = %x,%x %x %x %x %x,%x\n",
			hdr_mtx_param->mtx_on,
			hdr_mtx_param->mtx_only,
			(hdr_mtx_param->mtxi_pre_offset[0] << 16) |
			(hdr_mtx_param->mtxi_pre_offset[1] & 0xFFF),
			(in_mtx[0 * 3 + 0] << 16) |
			(in_mtx[0 * 3 + 1] & 0x1FFF),
			(in_mtx[0 * 3 + 2] << 16) |
			(in_mtx[1 * 3 + 0] & 0x1FFF),
			(in_mtx[1 * 3 + 1] << 16) |
			(in_mtx[1 * 3 + 2] & 0x1FFF),
			(in_mtx[2 * 3 + 0] << 16) |
			(in_mtx[2 * 3 + 1] & 0x1FFF),
			in_mtx[2 * 3 + 2],
			(hdr_mtx_param->mtxi_pos_offset[0] << 16) |
			(hdr_mtx_param->mtxi_pos_offset[1] & 0xFFF));
//#endif
		if (hdr_mtx_param->mtx_only == MTX_ONLY &&
		    !hdr_mtx_param->mtx_on)
			VSYNC_WRITE_VPP_REG_VPP_SEL(MATRIXI_EN_CTRL, 1, vpp_sel);
		else
			VSYNC_WRITE_VPP_REG_VPP_SEL(MATRIXI_EN_CTRL,
				hdr_mtx_param->mtx_on, vpp_sel);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(hdr_ctrl,
			hdr_mtx_param->mtx_on, 4, 1, vpp_sel);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(hdr_ctrl,
			hdr_mtx_param->mtx_only,
			16, 1, vpp_sel);
		/*bit14-15 is ai color for s5*/
		/*VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(hdr_ctrl, 1, 14, 1, vpp_sel);*/

		VSYNC_WRITE_VPP_REG_VPP_SEL(MATRIXI_COEF00_01,
				    (in_mtx[0 * 3 + 0] << 16) |
				    (in_mtx[0 * 3 + 1] & 0x1FFF), vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(MATRIXI_COEF02_10,
				    (in_mtx[0 * 3 + 2] << 16) |
				    (in_mtx[1 * 3 + 0] & 0x1FFF), vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(MATRIXI_COEF11_12,
				    (in_mtx[1 * 3 + 1] << 16) |
				    (in_mtx[1 * 3 + 2] & 0x1FFF), vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(MATRIXI_COEF20_21,
				    (in_mtx[2 * 3 + 0] << 16) |
				    (in_mtx[2 * 3 + 1] & 0x1FFF), vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(MATRIXI_COEF22,
				    in_mtx[2 * 3 + 2], vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(MATRIXI_OFFSET0_1,
				    (hdr_mtx_param->mtxi_pos_offset[0] << 16) |
				    (hdr_mtx_param->mtxi_pos_offset[1] & 0xFFF), vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(MATRIXI_OFFSET2,
				    hdr_mtx_param->mtxi_pos_offset[2], vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(MATRIXI_PRE_OFFSET0_1,
				    (hdr_mtx_param->mtxi_pre_offset[0] << 16) |
				    (hdr_mtx_param->mtxi_pre_offset[1] & 0xFFF), vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(MATRIXI_PRE_OFFSET2,
				    hdr_mtx_param->mtxi_pre_offset[2], vpp_sel);
	} else if (mtx_sel == HDR_GAMUT_MTX) {
		u32 ogain_lut_148 = hdr_lut_param->ogain_lut[148];

		for (i = 0; i < 9; i++)
			gmut_coef[i / 3][i % 3] =
				hdr_mtx_param->mtx_gamut[i];
		/* TODO: use hdr_mtx_param->gmut_shift directly */
		gmut_shift = calc_gmut_shift(hdr_mtx_param);

		for (i = 0; i < 3; i++)
			c_gain_lim_coef[i] =
				hdr_mtx_param->mtx_cgain[i] << 2;
		/* 0: adaptive scaler mode(Ys); 1: max linear(RGB max) */
		/* 2: none linear Ys -- Do NOT use it */
		if (hdr_mtx_param->p_sel & HLG_HDR ||
		    hdr_mtx_param->p_sel & HLG_SDR ||
		    hdr_mtx_param->p_sel & HLG_IPT) {
			if (cuva_static_hlg_en &&
				(hdr_mtx_param->p_sel & HLG_SDR))
				adpscl_mode = 1;
			else
				adpscl_mode = 0;
		} else {
			adpscl_mode = 1;
		}

		for (i = 0; i < 3; i++) {
			if (hdr_mtx_param->mtx_only == MTX_ONLY)
				adpscl_bypass[i] = 1;
			else
				adpscl_bypass[i] = 0;

			if (hdr_mtx_param->p_sel & HLG_HDR ||
			    hdr_mtx_param->p_sel & HLG_IPT)
				adpscl_alpha[i] = 1000 *
				(1 << hdr_lut_param->adp_scal_y_shift) / 10000;
			else if ((hdr_mtx_param->p_sel & HDR_SDR) ||
				 (hdr_mtx_param->p_sel & HDR10P_SDR))
				adpscl_alpha[i] =
					(1 << hdr_lut_param->adp_scal_y_shift);
			else
				adpscl_alpha[i] =
					(1 << hdr_lut_param->adp_scal_y_shift);

			if (adpscl_mode == 1)
				adpscl_ys_coef[i] =
					1 << hdr_lut_param->adp_scal_x_shift;
			else
				adpscl_ys_coef[i] =
					hdr_lut_param->ys_coef[i] >>
					(10 - hdr_lut_param->adp_scal_x_shift);

			adpscl_beta_s[i] = 0;
			adpscl_beta[i] = 0;
		}

		/*shift0 is for x coordinate*/
		/*shift1 is for scale multiple*/
		if (hdr_mtx_param->p_sel & HDR_SDR) {
			if (hdr_mtx_param->gmt_bit_mode) {
				adpscl_shift[0] =
					hdr_lut_param->adp_scal_x_shift;
				adpscl_shift[1] = OO_NOR -
				_log2((1 << OO_NOR) / 64);
			} else {
				/*because input 1/2, shift0/shift1 need change*/
				adpscl_shift[0] =
					hdr_lut_param->adp_scal_x_shift - 1;
				adpscl_shift[1] = OO_NOR -
				_log2((1 << OO_NOR) / ogain_lut_148)
				- 1;
			}
		} else if (hdr_mtx_param->p_sel & CUVA_SDR) {
			if (hdr_mtx_param->gmt_bit_mode) {
				adpscl_shift[0] =
					hdr_lut_param->adp_scal_x_shift;
				adpscl_shift[1] = OO_NOR -
				_log2((1 << OO_NOR) / 32);
			} else {
				/*because input 1/2, shift0/shift1 need change*/
				adpscl_shift[0] =
					hdr_lut_param->adp_scal_x_shift - 1;
				adpscl_shift[1] = OO_NOR -
				_log2((1 << OO_NOR) / 32)
				- 1;
			}
		} else if (hdr_mtx_param->p_sel & CUVAHLG_SDR) {
			adpscl_shift[0] = hdr_lut_param->adp_scal_x_shift;
			adpscl_shift[1] = OO_NOR -
				_log2((1 << OO_NOR) / 32);
		} else if (hdr_mtx_param->p_sel & HLG_SDR) {
			if (hdr_mtx_param->gmt_bit_mode || cuva_static_hlg_en) {
				adpscl_shift[0] =
					hdr_lut_param->adp_scal_x_shift;
				adpscl_shift[1] = OO_NOR -
				_log2((1 << OO_NOR) / ogain_lut_148);
			} else {
				/*because input 1/2, shift0/shift1 need change*/
				adpscl_shift[0] =
					hdr_lut_param->adp_scal_x_shift - 1;
				adpscl_shift[1] = OO_NOR -
				_log2((1 << OO_NOR) / ogain_lut_148)
				- 1;
			}
		} else if (hdr_mtx_param->p_sel & HDR10P_SDR) {
			if (p_hdr10pgen_param)
				scale_shift = _log2((1 << OO_NOR) /
				p_hdr10pgen_param->gain[148]);
			else
				scale_shift =
				_log2((1 << OO_NOR) / ogain_lut_148);
			if (hdr_mtx_param->gmt_bit_mode) {
				adpscl_shift[0] =
					hdr_lut_param->adp_scal_x_shift;
				adpscl_shift[1] = OO_NOR - scale_shift;
			} else {
				/*because input 1/2, shift0/shift1 need change*/
				adpscl_shift[0] =
					hdr_lut_param->adp_scal_x_shift - 1;
				adpscl_shift[1] = OO_NOR - scale_shift - 1;
			}
			if (p_hdr10pgen_param) {
				adpscl_shift[0] -= p_hdr10pgen_param->shift;
				adpscl_shift[1] -= p_hdr10pgen_param->shift;
			}
		} else if (hdr_mtx_param->p_sel & HDR_HLG) {
			adpscl_shift[0] = hdr_lut_param->adp_scal_x_shift;
			adpscl_shift[1] = OO_NOR -
			_log2((1 << OO_NOR) / ogain_lut_148);
		} else if (hdr_mtx_param->p_sel & SDR_GMT_CONVERT) {
			if (hdr_mtx_param->gmt_bit_mode) {
				scale_shift =
				_log2((1 << OO_NOR) / ogain_lut_148);
				/*because input 1/2, shift0/shift1 need change*/
				adpscl_shift[0] = hdr_lut_param->adp_scal_x_shift;
				adpscl_shift[1] = OO_NOR - scale_shift;
			} else {
				scale_shift =
				_log2((1 << OO_NOR) / ogain_lut_148);
				/*because input 1/2, shift0/shift1 need change*/
				adpscl_shift[0] = hdr_lut_param->adp_scal_x_shift - 1;
				adpscl_shift[1] = OO_NOR - scale_shift - 1;
			}
		} else if (hdr_mtx_param->p_sel &
			(CUVA_HDR | CUVAHLG_HDR | CUVAHLG_HLG)) {
			adpscl_shift[0] = hdr_lut_param->adp_scal_x_shift;
			adpscl_shift[1] = hdr_lut_param->adp_scal_x_shift;
		} else if (hdr_mtx_param->p_sel == IPT_SDR) {
			adpscl_shift[0] = hdr_lut_param->adp_scal_x_shift  - 2;
			adpscl_shift[1] = OO_NOR -
			_log2((1 << OO_NOR) / ogain_lut_148) - 2;
		} else if (hdr_mtx_param->p_sel == HDR_HDR) {
			adpscl_shift[0] = hdr_lut_param->adp_scal_x_shift;
			adpscl_shift[1] = OO_NOR - _log2((1 << OO_NOR) / 64);
		} else {
			adpscl_shift[0] = hdr_lut_param->adp_scal_x_shift;
			adpscl_shift[1] = OO_NOR;
		}

		/*shift2 is not used, set default*/
		adpscl_shift[2] = hdr_lut_param->adp_scal_y_shift;

#ifdef HDR2_PRINT
		pr_info("hdr: gamut_mtx %d mode %d shift %d = %x %x %x %x %x\n",
			hdr_mtx_param->mtx_on,
			hdr_mtx_param->mtx_gamut_mode,
			gmut_shift,
			(gmut_coef[0][1] & 0xffff) << 16 |
			(gmut_coef[0][0] & 0xffff),
			(gmut_coef[1][0] & 0xffff) << 16 |
			(gmut_coef[0][2] & 0xffff),
			(gmut_coef[1][2] & 0xffff) << 16 |
			(gmut_coef[1][1] & 0xffff),
			(gmut_coef[2][1] & 0xffff) << 16 |
			(gmut_coef[2][0] & 0xffff),
			gmut_coef[2][2] & 0xffff);
		pr_info("hdr: adpscl bypass %d, x_shift %d, y_shift %d\n",
			adpscl_bypass[0], adpscl_shift[0], adpscl_shift[1]);
#endif

		/*g12a/g12b for cuva process, workaround oo before gamut*/
		if ((is_meson_g12a_cpu() || is_meson_g12b_cpu()) &&
		    (hdr_mtx_param->p_sel &
		     (CUVA_HDR | CUVA_SDR | CUVAHLG_HDR |
		      CUVAHLG_SDR | CUVAHLG_HLG)))
			hdr_mtx_param->mtx_gamut_mode = 2;

		if ((is_meson_g12a_cpu() || is_meson_g12b_cpu()) &&
		    cuva_static_hlg_en &&
		    (hdr_mtx_param->p_sel & HLG_SDR))
			hdr_mtx_param->mtx_gamut_mode = 2;

		/*gamut mode: 1->gamut before ootf*/
					/*2->gamut after ootf*/
					/*other->disable gamut*/
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(hdr_ctrl,
					 hdr_mtx_param->mtx_gamut_mode, 6, 2, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(GMUT_CTRL, gmut_shift, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(GMUT_COEF0,
				    (gmut_coef[0][1] & 0xffff) << 16 |
				    (gmut_coef[0][0] & 0xffff), vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(GMUT_COEF1,
				    (gmut_coef[1][0] & 0xffff) << 16 |
				    (gmut_coef[0][2] & 0xffff), vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(GMUT_COEF2,
				    (gmut_coef[1][2] & 0xffff) << 16 |
				    (gmut_coef[1][1] & 0xffff), vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(GMUT_COEF3,
				    (gmut_coef[2][1] & 0xffff) << 16 |
				    (gmut_coef[2][0] & 0xffff), vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(GMUT_COEF4,
				    gmut_coef[2][2] & 0xffff, vpp_sel);

		VSYNC_WRITE_VPP_REG_VPP_SEL(CGAIN_COEF0,
				    c_gain_lim_coef[1] << 16 |
				    c_gain_lim_coef[0], vpp_sel);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(CGAIN_COEF1,
					 c_gain_lim_coef[2],	0, 12, vpp_sel);

		VSYNC_WRITE_VPP_REG_VPP_SEL(ADPS_CTRL,
				    adpscl_bypass[2] << 6 |
				    adpscl_bypass[1] << 5 |
				    adpscl_bypass[0] << 4 |
				    adpscl_mode, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(ADPS_ALPHA0,
				    adpscl_alpha[1] << 16 | adpscl_alpha[0], vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(ADPS_ALPHA1,
				    adpscl_shift[0] << 24 |
				    adpscl_shift[1] << 20 |
				    adpscl_shift[2] << 16 |
				    adpscl_alpha[2], vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(ADPS_BETA0,
				    adpscl_beta_s[0] << 20 | adpscl_beta[0], vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(ADPS_BETA1,
				    adpscl_beta_s[1] << 20 | adpscl_beta[1], vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(ADPS_BETA2,
				    adpscl_beta_s[2] << 20 | adpscl_beta[2], vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(ADPS_COEF0,
				    adpscl_ys_coef[1] << 16 |
				    adpscl_ys_coef[0], vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(ADPS_COEF1,
				    adpscl_ys_coef[2], vpp_sel);
	} else if (mtx_sel == HDR_OUT_MTX) {
		for (i = 0; i < MTX_NUM_PARAM; i++)
			out_mtx[i] = hdr_mtx_param->mtx_out[i];
//#ifdef HDR2_PRINT
		pr_csc(64, "hdr: out_mtx %d %d = %x,%x %x %x %x %x,%x\n",
			hdr_mtx_param->mtx_on,
			hdr_mtx_param->mtx_only,
			(hdr_mtx_param->mtxo_pre_offset[0] << 16) |
			(hdr_mtx_param->mtxo_pre_offset[1] & 0xFFF),
			(out_mtx[0 * 3 + 0] << 16) |
			(out_mtx[0 * 3 + 1] & 0x1FFF),
			(out_mtx[0 * 3 + 2] << 16) |
			(out_mtx[1 * 3 + 0] & 0x1FFF),
			(out_mtx[1 * 3 + 1] << 16) |
			(out_mtx[1 * 3 + 2] & 0x1FFF),
			(out_mtx[2 * 3 + 0] << 16) |
			(out_mtx[2 * 3 + 1] & 0x1FFF),
			out_mtx[2 * 3 + 2],
			(hdr_mtx_param->mtxo_pos_offset[0] << 16) |
			(hdr_mtx_param->mtxo_pos_offset[1] & 0xFFF));
//#endif
		VSYNC_WRITE_VPP_REG_VPP_SEL(CGAIN_OFFT,
			(hdr_mtx_param->mtx_cgain_offset[2] << 16) |
			hdr_mtx_param->mtx_cgain_offset[1], vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(MATRIXO_EN_CTRL,
			hdr_mtx_param->mtx_on, vpp_sel);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(hdr_ctrl, 0, 17, 1, vpp_sel);
		/*bit14-15 is ai color for s5*/
		/*VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(hdr_ctrl, 1, 15, 1, vpp_sel);*/

		VSYNC_WRITE_VPP_REG_VPP_SEL(MATRIXO_COEF00_01,
			(out_mtx[0 * 3 + 0] << 16) |
			(out_mtx[0 * 3 + 1] & 0x1FFF), vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(MATRIXO_COEF02_10,
			(out_mtx[0 * 3 + 2] << 16) |
			(out_mtx[1 * 3 + 0] & 0x1FFF), vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(MATRIXO_COEF11_12,
			(out_mtx[1 * 3 + 1] << 16) |
			(out_mtx[1 * 3 + 2] & 0x1FFF), vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(MATRIXO_COEF20_21,
			(out_mtx[2 * 3 + 0] << 16) |
			(out_mtx[2 * 3 + 1] & 0x1FFF), vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(MATRIXO_COEF22,
			out_mtx[2 * 3 + 2], vpp_sel);

		VSYNC_WRITE_VPP_REG_VPP_SEL(MATRIXO_OFFSET0_1,
			(hdr_mtx_param->mtxo_pos_offset[0] << 16) |
			(hdr_mtx_param->mtxo_pos_offset[1] & 0xFFF), vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(MATRIXO_OFFSET2,
			hdr_mtx_param->mtxo_pos_offset[2], vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(MATRIXO_PRE_OFFSET0_1,
			(hdr_mtx_param->mtxo_pre_offset[0] << 16) |
			(hdr_mtx_param->mtxo_pre_offset[1] & 0xFFF), vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(MATRIXO_PRE_OFFSET2,
			hdr_mtx_param->mtxo_pre_offset[2], vpp_sel);
	}
}

void s5_set_eotf_lut(enum hdr_module_sel module_sel,
		  struct hdr_proc_lut_param_s *hdr_lut_param,
		  enum vpp_index_e vpp_index)
{
	static unsigned int lut[HDR2_EOTF_LUT_SIZE];
	unsigned int eotf_lut_addr_port = 0;
	unsigned int eotf_lut_data_port = 0;
	unsigned int hdr_ctrl = 0;
	unsigned int i = 0;
	int vpp_sel = 0;/*0xfe;*/

	if (module_sel == VD1_HDR) {
		eotf_lut_addr_port = S5_VD1_EOTF_LUT_ADDR_PORT;
		eotf_lut_data_port = S5_VD1_EOTF_LUT_DATA_PORT;
		hdr_ctrl = S5_VD1_HDR2_CTRL;
	} else if (module_sel == S5_VD1_SLICE1) {
		eotf_lut_addr_port = S5_VD1_SLICE1_EOTF_LUT_ADDR_PORT;
		eotf_lut_data_port = S5_VD1_SLICE1_EOTF_LUT_DATA_PORT;
		hdr_ctrl = S5_VD1_HDR2_CTRL;
	} else if (module_sel == S5_VD1_SLICE2) {
		eotf_lut_addr_port = S5_VD1_SLICE2_EOTF_LUT_ADDR_PORT;
		eotf_lut_data_port = S5_VD1_SLICE2_EOTF_LUT_DATA_PORT;
		hdr_ctrl = S5_VD1_HDR2_CTRL;
	} else if (module_sel == S5_VD1_SLICE3) {
		eotf_lut_addr_port = S5_VD1_SLICE3_EOTF_LUT_ADDR_PORT;
		eotf_lut_data_port = S5_VD1_SLICE3_EOTF_LUT_DATA_PORT;
		hdr_ctrl = S5_VD1_HDR2_CTRL;
	} else if (module_sel == VD2_HDR) {
		eotf_lut_addr_port = S5_VD2_EOTF_LUT_ADDR_PORT;
		eotf_lut_data_port = S5_VD2_EOTF_LUT_DATA_PORT;
		hdr_ctrl = S5_VD2_HDR2_CTRL;
	} else if (module_sel == OSD1_HDR) {
		eotf_lut_addr_port = S5_OSD1_EOTF_LUT_ADDR_PORT;
		eotf_lut_data_port = S5_OSD1_EOTF_LUT_DATA_PORT;
		hdr_ctrl = S5_OSD1_HDR2_CTRL;
	} else if (module_sel == OSD3_HDR) {
		eotf_lut_addr_port = S5_OSD3_EOTF_LUT_ADDR_PORT;
		eotf_lut_data_port = S5_OSD3_EOTF_LUT_DATA_PORT;
		hdr_ctrl = S5_OSD3_HDR2_CTRL;
	} else if (module_sel == VDIN1_HDR) {
		eotf_lut_addr_port = VDIN_PP_EOTF_LUT_ADDR_PORT;
		eotf_lut_data_port = VDIN_PP_EOTF_LUT_DATA_PORT;
		hdr_ctrl = VDIN_PP_HDR2_CTRL;
		vpp_sel = 0xff;
	}

	for (i = 0; i < HDR2_EOTF_LUT_SIZE; i++)
		lut[i] = hdr_lut_param->eotf_lut[i];

	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(hdr_ctrl, hdr_lut_param->lut_on, 3, 1, vpp_sel);

	if (!hdr_lut_param->lut_on)
		return;

	/* use dma transport*/
	if (cpu_write_lut) {
		VSYNC_WRITE_VPP_REG_VPP_SEL(eotf_lut_addr_port, 0x0, vpp_sel);
		for (i = 0; i < HDR2_EOTF_LUT_SIZE; i++)
			VSYNC_WRITE_VPP_REG_VPP_SEL(eotf_lut_data_port, lut[i], vpp_sel);
	}
}

void s5_set_ootf_lut(enum hdr_module_sel module_sel,
		  struct hdr_proc_lut_param_s *hdr_lut_param,
		  enum vpp_index_e vpp_index)
{
	static unsigned int lut[HDR2_OOTF_LUT_SIZE];
	unsigned int ootf_lut_addr_port = 0;
	unsigned int ootf_lut_data_port = 0;
	unsigned int hdr_ctrl = 0;
	unsigned int i = 0;
	int vpp_sel = 0;/*0xfe;*/

	if (module_sel == VD1_HDR) {
		ootf_lut_addr_port = S5_VD1_OGAIN_LUT_ADDR_PORT;
		ootf_lut_data_port = S5_VD1_OGAIN_LUT_DATA_PORT;
		hdr_ctrl = S5_VD1_HDR2_CTRL;
	} else if (module_sel == S5_VD1_SLICE1) {
		ootf_lut_addr_port = S5_VD1_SLICE1_OGAIN_LUT_ADDR_PORT;
		ootf_lut_data_port = S5_VD1_SLICE1_OGAIN_LUT_DATA_PORT;
		hdr_ctrl = S5_VD1_SLICE1_HDR2_CTRL;
	} else if (module_sel == S5_VD1_SLICE2) {
		ootf_lut_addr_port = S5_VD1_SLICE2_OGAIN_LUT_ADDR_PORT;
		ootf_lut_data_port = S5_VD1_SLICE2_OGAIN_LUT_DATA_PORT;
		hdr_ctrl = S5_VD1_SLICE2_HDR2_CTRL;
	} else if (module_sel == S5_VD1_SLICE3) {
		ootf_lut_addr_port = S5_VD1_SLICE3_OGAIN_LUT_ADDR_PORT;
		ootf_lut_data_port = S5_VD1_SLICE3_OGAIN_LUT_DATA_PORT;
		hdr_ctrl = S5_VD1_SLICE3_HDR2_CTRL;
	} else if (module_sel == VD2_HDR) {
		ootf_lut_addr_port = S5_VD2_OGAIN_LUT_ADDR_PORT;
		ootf_lut_data_port = S5_VD2_OGAIN_LUT_DATA_PORT;
		hdr_ctrl = S5_VD2_HDR2_CTRL;
	} else if (module_sel == OSD1_HDR) {
		ootf_lut_addr_port = S5_OSD1_OGAIN_LUT_ADDR_PORT;
		ootf_lut_data_port = S5_OSD1_OGAIN_LUT_DATA_PORT;
		hdr_ctrl = S5_OSD1_HDR2_CTRL;
	} else if (module_sel == OSD3_HDR) {
		ootf_lut_addr_port = S5_OSD3_OGAIN_LUT_ADDR_PORT;
		ootf_lut_data_port = S5_OSD3_OGAIN_LUT_DATA_PORT;
		hdr_ctrl = S5_OSD3_HDR2_CTRL;
	} else if (module_sel == VDIN1_HDR) {
		ootf_lut_addr_port = VDIN_PP_OGAIN_LUT_ADDR_PORT;
		ootf_lut_data_port = VDIN_PP_OGAIN_LUT_DATA_PORT;
		hdr_ctrl = VDIN_PP_HDR2_CTRL;
		vpp_sel = 0xff;
	}

	for (i = 0; i < HDR2_OOTF_LUT_SIZE; i++)
		lut[i] = hdr_lut_param->ogain_lut[i];

	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(hdr_ctrl, hdr_lut_param->lut_on, 1, 1, vpp_sel);

	if (!hdr_lut_param->lut_on)
		return;

	/* use dma transport*/
	if (cpu_write_lut) {
		VSYNC_WRITE_VPP_REG_VPP_SEL(ootf_lut_addr_port, 0x0, vpp_sel);

		for (i = 0; i < HDR2_OOTF_LUT_SIZE / 2; i++)
			VSYNC_WRITE_VPP_REG_VPP_SEL(ootf_lut_data_port,
			(lut[i * 2 + 1] << 16) + lut[i * 2], vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(ootf_lut_data_port, lut[148], vpp_sel);
	}
}

void s5_set_oetf_lut(enum hdr_module_sel module_sel,
		  struct hdr_proc_lut_param_s *hdr_lut_param,
		  enum vpp_index_e vpp_index)
{
	static unsigned int lut[HDR2_OETF_LUT_SIZE];
	unsigned int oetf_lut_addr_port = 0;
	unsigned int oetf_lut_data_port = 0;
	unsigned int hdr_ctrl = 0;
	unsigned int i = 0;
	int vpp_sel = 0;/*0xfe;*/

	if (module_sel == VD1_HDR) {
		oetf_lut_addr_port = S5_VD1_OETF_LUT_ADDR_PORT;
		oetf_lut_data_port = S5_VD1_OETF_LUT_DATA_PORT;
		hdr_ctrl = S5_VD1_HDR2_CTRL;
	} else if (module_sel == S5_VD1_SLICE1) {
		oetf_lut_addr_port = S5_VD1_SLICE1_OETF_LUT_ADDR_PORT;
		oetf_lut_data_port = S5_VD1_SLICE1_OETF_LUT_DATA_PORT;
		hdr_ctrl = S5_VD1_SLICE1_HDR2_CTRL;
	} else if (module_sel == S5_VD1_SLICE2) {
		oetf_lut_addr_port = S5_VD1_SLICE2_OETF_LUT_ADDR_PORT;
		oetf_lut_data_port = S5_VD1_SLICE2_OETF_LUT_DATA_PORT;
		hdr_ctrl = S5_VD1_SLICE2_HDR2_CTRL;
	} else if (module_sel == S5_VD1_SLICE3) {
		oetf_lut_addr_port = S5_VD1_SLICE3_OETF_LUT_ADDR_PORT;
		oetf_lut_data_port = S5_VD1_SLICE3_OETF_LUT_DATA_PORT;
		hdr_ctrl = S5_VD1_SLICE3_HDR2_CTRL;
	} else if (module_sel == VD2_HDR) {
		oetf_lut_addr_port = S5_VD2_OETF_LUT_ADDR_PORT;
		oetf_lut_data_port = S5_VD2_OETF_LUT_DATA_PORT;
		hdr_ctrl = S5_VD2_HDR2_CTRL;
	} else if (module_sel == OSD1_HDR) {
		oetf_lut_addr_port = S5_OSD1_OETF_LUT_ADDR_PORT;
		oetf_lut_data_port = S5_OSD1_OETF_LUT_DATA_PORT;
		hdr_ctrl = S5_OSD1_HDR2_CTRL;
	} else if (module_sel == OSD3_HDR) {
		oetf_lut_addr_port = S5_OSD3_OETF_LUT_ADDR_PORT;
		oetf_lut_data_port = S5_OSD3_OETF_LUT_DATA_PORT;
		hdr_ctrl = S5_OSD3_HDR2_CTRL;
	} else if (module_sel == VDIN1_HDR) {
		oetf_lut_addr_port = VDIN_PP_OETF_LUT_ADDR_PORT;
		oetf_lut_data_port = VDIN_PP_OETF_LUT_DATA_PORT;
		hdr_ctrl = VDIN_PP_HDR2_CTRL;
		vpp_sel = 0xff;
	}

	for (i = 0; i < HDR2_OETF_LUT_SIZE; i++)
		lut[i] = hdr_lut_param->oetf_lut[i];

	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(hdr_ctrl, hdr_lut_param->lut_on, 2, 1, vpp_sel);

	if (!hdr_lut_param->lut_on)
		return;

	/* use dma transport*/
	if (cpu_write_lut) {
		VSYNC_WRITE_VPP_REG_VPP_SEL(oetf_lut_addr_port, 0x0, vpp_sel);

		for (i = 0; i < HDR2_OETF_LUT_SIZE / 2; i++) {
			if (hdr_lut_param->bitdepth == 10)
				VSYNC_WRITE_VPP_REG_VPP_SEL(oetf_lut_data_port,
					((lut[i * 2 + 1] >> 2) << 16) +
					(lut[i * 2] >> 2), vpp_sel);
			else
				VSYNC_WRITE_VPP_REG_VPP_SEL(oetf_lut_data_port,
					(lut[i * 2 + 1] << 16) +
					lut[i * 2], vpp_sel);
		}
		if (hdr_lut_param->bitdepth == 10)
			VSYNC_WRITE_VPP_REG_VPP_SEL(oetf_lut_data_port, lut[148] >> 2, vpp_sel);
		else
			VSYNC_WRITE_VPP_REG_VPP_SEL(oetf_lut_data_port, lut[148], vpp_sel);
	}
}

void s5_set_c_gain(enum hdr_module_sel module_sel,
		struct hdr_proc_lut_param_s *hdr_lut_param,
		enum vpp_index_e vpp_index)
{
	unsigned int lut[HDR2_CGAIN_LUT_SIZE];
	unsigned int cgain_lut_addr_port = 0;
	unsigned int cgain_lut_data_port = 0;
	unsigned int hdr_ctrl = 0;
	unsigned int cgain_coef1 = 0;
	unsigned int i = 0;
	int vpp_sel = 0;/*0xfe;*/

	if (module_sel == VD1_HDR) {
		cgain_lut_addr_port = S5_VD1_CGAIN_LUT_ADDR_PORT;
		cgain_lut_data_port = S5_VD1_CGAIN_LUT_DATA_PORT;
		hdr_ctrl = S5_VD1_HDR2_CTRL;
		cgain_coef1 = S5_VD1_HDR2_CGAIN_COEF1;
	} else if (module_sel == S5_VD1_SLICE1) {
		cgain_lut_addr_port = S5_VD1_SLICE1_CGAIN_LUT_ADDR_PORT;
		cgain_lut_data_port = S5_VD1_SLICE1_CGAIN_LUT_DATA_PORT;
		hdr_ctrl = S5_VD1_SLICE1_HDR2_CTRL;
		cgain_coef1 = S5_VD1_SLICE1_HDR2_CGAIN_COEF1;
	} else if (module_sel == S5_VD1_SLICE2) {
		cgain_lut_addr_port = S5_VD1_SLICE2_CGAIN_LUT_ADDR_PORT;
		cgain_lut_data_port = S5_VD1_SLICE2_CGAIN_LUT_DATA_PORT;
		hdr_ctrl = S5_VD1_SLICE2_HDR2_CTRL;
		cgain_coef1 = S5_VD1_SLICE2_HDR2_CGAIN_COEF1;
	} else if (module_sel == S5_VD1_SLICE3) {
		cgain_lut_addr_port = S5_VD1_SLICE3_CGAIN_LUT_ADDR_PORT;
		cgain_lut_data_port = S5_VD1_SLICE3_CGAIN_LUT_DATA_PORT;
		hdr_ctrl = S5_VD1_SLICE3_HDR2_CTRL;
		cgain_coef1 = S5_VD1_SLICE3_HDR2_CGAIN_COEF1;
	} else if (module_sel == VD2_HDR) {
		cgain_lut_addr_port = S5_VD2_CGAIN_LUT_ADDR_PORT;
		cgain_lut_data_port = S5_VD2_CGAIN_LUT_DATA_PORT;
		hdr_ctrl = S5_VD2_HDR2_CTRL;
		cgain_coef1 = S5_VD2_HDR2_CGAIN_COEF1;
	} else if (module_sel == OSD1_HDR) {
		cgain_lut_addr_port = S5_OSD1_CGAIN_LUT_ADDR_PORT;
		cgain_lut_data_port = S5_OSD1_CGAIN_LUT_DATA_PORT;
		hdr_ctrl = S5_OSD1_HDR2_CTRL;
		cgain_coef1 = S5_OSD1_HDR2_CGAIN_COEF1;
	} else if (module_sel == OSD3_HDR) {
		cgain_lut_addr_port = S5_OSD3_CGAIN_LUT_ADDR_PORT;
		cgain_lut_data_port = S5_OSD3_CGAIN_LUT_DATA_PORT;
		hdr_ctrl = S5_OSD3_HDR2_CTRL;
		cgain_coef1 = S5_OSD3_HDR2_CGAIN_COEF1;
	} else if (module_sel == VDIN1_HDR) {
		cgain_lut_addr_port = VDIN_PP_CGAIN_LUT_ADDR_PORT;
		cgain_lut_data_port = VDIN_PP_CGAIN_LUT_DATA_PORT;
		hdr_ctrl = VDIN_PP_HDR2_CTRL;
		cgain_coef1 = VDIN_PP_HDR2_CGAIN_COEF1;
		vpp_sel = 0xff;
	}

	for (i = 0; i < HDR2_CGAIN_LUT_SIZE; i++)
		lut[i] = hdr_lut_param->cgain_lut[i];

	/*cgain mode: 0->y domin*/
	/*cgain mode: 1->rgb domin, use r/g/b max*/
	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(hdr_ctrl,
				 0, 12, 1, vpp_sel);
	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(hdr_ctrl,
				 hdr_lut_param->cgain_en, 0, 1, vpp_sel);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_SM1)) {
		if (hdr_lut_param->bitdepth == 10)
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(cgain_coef1,
				0x400, 16, 13, vpp_sel);
		else if (hdr_lut_param->bitdepth == 12)
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(cgain_coef1,
				0x1000, 16, 13, vpp_sel);
	}

	if (!hdr_lut_param->cgain_en)
		return;

	/* use dma transport*/
	if (cpu_write_lut) {
		VSYNC_WRITE_VPP_REG_VPP_SEL(cgain_lut_addr_port, 0x0, vpp_sel);

		for (i = 0; i < HDR2_CGAIN_LUT_SIZE / 2; i++)
			VSYNC_WRITE_VPP_REG_VPP_SEL(cgain_lut_data_port,
				(lut[i * 2 + 1] << 16) + lut[i * 2], vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(cgain_lut_data_port, lut[64], vpp_sel);
	}
}

static u32 s5_hdr_hist[NUM_HDR_HIST][128];
static u32 hdr_max_rgb;
static u8 percentile_percent[9] = {
	1, 5, 10, 25, 50, 75, 90, 95, 99
};

u32 s5_percentile[9];

u32 s5_hist_maxrgb_luminance[128] = {
	0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 1, 1, 1, 1,
	1, 2, 2, 2, 3, 3, 4, 4,
	5, 5, 6, 7, 7, 8, 9, 10, 11,
	12, 14, 15, 17, 18, 20, 22, 24,
	26, 29, 31, 34, 37, 41, 44, 48,
	52, 57, 62, 67, 72, 78, 85, 92,
	99, 107, 116, 125, 135, 146, 158, 170,
	183, 198, 213, 229, 247, 266, 287, 308,
	332, 357, 384, 413, 445, 478, 514, 553, 594,
	639, 686, 737, 792, 851, 915, 983, 1056, 1134,
	1219, 1309, 1406, 1511, 1623, 1744, 1873, 2012,
	2162, 2323, 2496, 2683, 2883, 3098, 3330, 3580,
	3849, 4138, 4450, 4786, 5148, 5539, 5959, 6413,
	6903, 7431, 8001, 8616, 9281, 10000
};

/*AI color disable*/
void disable_ai_color(void)
{
	WRITE_VPP_REG_BITS_S5(S5_VD1_HDR2_CTRL, 0, 14, 2);
	WRITE_VPP_REG_BITS_S5(S5_VD1_SLICE1_HDR2_CTRL, 0, 14, 2);
	WRITE_VPP_REG_BITS_S5(S5_VD1_SLICE2_HDR2_CTRL, 0, 14, 2);
	WRITE_VPP_REG_BITS_S5(S5_VD1_SLICE3_HDR2_CTRL, 0, 14, 2);
	WRITE_VPP_REG_BITS_S5(S5_VD2_HDR2_CTRL, 0, 14, 2);
	WRITE_VPP_REG_BITS_S5(S5_OSD1_HDR2_CTRL, 0, 14, 2);
	WRITE_VPP_REG_BITS_S5(S5_OSD3_HDR2_CTRL, 0, 14, 2);
	ai_clr_config(0);
}

void s5_set_hist(enum hdr_module_sel module_sel, int enable,
	      enum hdr_hist_sel hist_sel,
	unsigned int hist_width, unsigned int hist_height)
{
	unsigned int hist_ctrl_port = 0;

	if (module_sel == VD1_HDR)
		hist_ctrl_port = S5_VD1_HDR2_HIST_CTRL;
	else
		return;

	if (enable) {
		WRITE_VPP_REG_S5(hist_ctrl_port + 1, hist_width - 1);
		WRITE_VPP_REG_S5(hist_ctrl_port + 2, hist_height - 1);
		WRITE_VPP_REG_S5(hist_ctrl_port,
			(1 << 4) | (hist_sel << 0));
	} else if (READ_VPP_REG_BITS(hist_ctrl_port, 4, 1)) {
		WRITE_VPP_REG_BITS_S5(hist_ctrl_port, 0, 4, 1);
		hdr_max_rgb = 0;
	}
}

void s5_get_hist(enum vd_path_e vd_path, enum hdr_hist_sel hist_sel)
{
	unsigned int hist_ctrl_port = 0;
	unsigned int hist_height, hist_width, i;
	u32 num_pixel, total_pixel;
	int j;
	int k = 0;
	enum hdr_module_sel module_sel = VD1_HDR;
	unsigned int hdr2_hist_rd;

	hist_width = 0;
	hist_height = 0;

	if (vd_path == VD1_PATH)
		module_sel = VD1_HDR;
	else if (vd_path == VD2_PATH)
		module_sel = VD2_HDR;

	if (module_sel == VD1_HDR) {
		hist_ctrl_port = S5_VD1_HDR2_HIST_CTRL;
		hdr2_hist_rd = S5_VD1_HDR2_HIST_CTRL + 3;
	} else if (module_sel == VD2_HDR) {
		hist_ctrl_port = S5_VD2_HDR2_HIST_CTRL;
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2))
			hdr2_hist_rd = VD2_HDR2_HIST_RD_2;
		else
			hdr2_hist_rd = S5_VD2_HDR2_HIST_CTRL + 3;
	}

	if (module_sel == VD1_HDR) {
		hist_width = READ_VPP_REG_BITS(VPP_PREBLEND_H_SIZE, 0, 13);
		hist_height = READ_VPP_REG_BITS(VPP_PREBLEND_H_SIZE, 16, 13);
	} else if (module_sel == VD2_HDR) {
		hist_width = READ_VPP_REG_BITS(VPP_VD2_HDR_IN_SIZE, 0, 13);
		hist_height = READ_VPP_REG_BITS(VPP_VD2_HDR_IN_SIZE, 16, 13);
	}

	if (!hist_width || !hist_height)
		return;

	if ((hist_height != READ_VPP_REG(hist_ctrl_port + 2) + 1) ||
	    (hist_width != READ_VPP_REG(hist_ctrl_port + 1) + 1) ||
	    /*(READ_VPP_REG_BITS(hist_ctrl_port, 4, 1) == 0) ||*/
	    (READ_VPP_REG_BITS(hist_ctrl_port, 0, 3) != hist_sel)) {
		s5_set_hist(module_sel, 1, hist_sel, hist_width, hist_height);
		return;
	}

	for (i = 0; i < NUM_HDR_HIST - 1; i++)
		memcpy(s5_hdr_hist[i], s5_hdr_hist[i + 1], 128 * sizeof(u32));
	memset(s5_percentile, 0, 9 * sizeof(u32));
	total_pixel = 0;
	for (i = 0; i < 128; i++) {
		WRITE_VPP_REG_BITS_S5(hist_ctrl_port, i, 16, 8);
		num_pixel = READ_VPP_REG(hdr2_hist_rd);
		total_pixel += num_pixel;
		s5_hdr_hist[NUM_HDR_HIST - 1][i] = num_pixel;
	}
	num_pixel = 0;

	if (total_pixel) {
		for (i = 0; i < 128; i++) {
			num_pixel += s5_hdr_hist[NUM_HDR_HIST - 1][i];
			for (j = 8; j >= k; j--) {
				if (num_pixel * 100 / total_pixel >=
					percentile_percent[j]) {
					s5_percentile[j] =
					s5_hist_maxrgb_luminance[i];
					k = j + 1;
					if (k > 8)
						k = 8;
					break;
				}
			}
			if (s5_hdr_hist[NUM_HDR_HIST - 1][i])
				hdr_max_rgb =
					(i + 1) * 10000 / 128;
			if (s5_percentile[8] != 0)
				break;
		}
		if (s5_percentile[0] == 0)
			s5_percentile[0] = 1;
		for (i = 1; i < 9; i++) {
			if (s5_percentile[i] == 0)
				s5_percentile[i] = s5_percentile[i - 1] + 1;
		}
		s5_percentile[1] = s5_percentile[8];
	}

#ifdef HDR2_PRINT
	if (total_pixel && percentile_index) {
		for (i = 0; i < 16; i++) {
			pr_info("hist[%d..]=%d %d %d %d %d %d %d %d\n",
				i * 8,
				s5_hdr_hist[NUM_HDR_HIST - 1][i * 8],
				s5_hdr_hist[NUM_HDR_HIST - 1][i * 8 + 1],
				s5_hdr_hist[NUM_HDR_HIST - 1][i * 8 + 2],
				s5_hdr_hist[NUM_HDR_HIST - 1][i * 8 + 3],
				s5_hdr_hist[NUM_HDR_HIST - 1][i * 8 + 4],
				s5_hdr_hist[NUM_HDR_HIST - 1][i * 8 + 5],
				s5_hdr_hist[NUM_HDR_HIST - 1][i * 8 + 6],
				s5_hdr_hist[NUM_HDR_HIST - 1][i * 8 + 7]);
			pr_info("max=%d s5_percentile=%d %d %d %d %d %d %d %d %d\n",
				hdr_max_rgb,
				s5_percentile[0], s5_percentile[1], s5_percentile[2],
				s5_percentile[3], s5_percentile[4], s5_percentile[5],
				s5_percentile[6], s5_percentile[7], s5_percentile[8]);
		}
	}
#endif
}

void s5_hdr_hist_config(enum hdr_module_sel module_sel,
		     struct hdr_proc_lut_param_s *hdr_lut_param,
		     enum vpp_index_e vpp_index)
{
	unsigned int hist_ctrl;
	unsigned int hist_hs_he;
	unsigned int hist_vs_ve;
	int vpp_sel = 0;/*0xfe;*/

	if (module_sel == VD1_HDR) {
		hist_ctrl = S5_VD1_HDR2_HIST_CTRL;
		hist_hs_he = S5_VD1_HDR2_HIST_H_START_END;
		hist_vs_ve = S5_VD1_HDR2_HIST_V_START_END;
	} else if (module_sel == S5_VD1_SLICE1) {
		hist_ctrl = S5_VD1_SLICE1_HDR2_HIST_CTRL;
		hist_hs_he = S5_VD1_SLICE1_HDR2_HIST_H_START_END;
		hist_vs_ve = S5_VD1_SLICE1_HDR2_HIST_V_START_END;
	} else if (module_sel == S5_VD1_SLICE2) {
		hist_ctrl = S5_VD1_SLICE2_HDR2_HIST_CTRL;
		hist_hs_he = S5_VD1_SLICE2_HDR2_HIST_H_START_END;
		hist_vs_ve = S5_VD1_SLICE2_HDR2_HIST_V_START_END;
	} else if (module_sel == S5_VD1_SLICE3) {
		hist_ctrl = S5_VD1_SLICE3_HDR2_HIST_CTRL;
		hist_hs_he = S5_VD1_SLICE3_HDR2_HIST_H_START_END;
		hist_vs_ve = S5_VD1_SLICE3_HDR2_HIST_V_START_END;
	} else if (module_sel == VD2_HDR) {
		hist_ctrl = S5_VD2_HDR2_HIST_CTRL;
		hist_hs_he = S5_VD2_HDR2_HIST_H_START_END;
		hist_vs_ve = S5_VD2_HDR2_HIST_V_START_END;
	} else if (module_sel == OSD1_HDR) {
		hist_ctrl = S5_OSD1_HDR2_HIST_CTRL;
		hist_hs_he = S5_OSD1_HDR2_HIST_H_START_END;
		hist_vs_ve = S5_OSD1_HDR2_HIST_V_START_END;
	} else if (module_sel == OSD3_HDR) {
		hist_ctrl = S5_OSD3_HDR2_HIST_CTRL;
		hist_hs_he = S5_OSD3_HDR2_HIST_H_START_END;
		hist_vs_ve = S5_OSD3_HDR2_HIST_V_START_END;
	} else if (module_sel == VDIN1_HDR) {
		hist_ctrl = VDIN_PP_HDR2_HIST_CTRL;
		hist_hs_he = VDIN_PP_HDR2_HIST_H_START_END;
		hist_vs_ve = VDIN_PP_HDR2_HIST_V_START_END;
		vpp_sel = 0xff;
	} else {
		return;
	}

	if (get_cpu_type() < MESON_CPU_MAJOR_ID_TM2)
		return;

	if (hdr_lut_param->hist_en) {
		VSYNC_WRITE_VPP_REG_VPP_SEL(hist_ctrl, 0, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(hist_hs_he, 0xeff, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(hist_vs_ve, 0x86f, vpp_sel);
	} else {
		VSYNC_WRITE_VPP_REG_VPP_SEL(hist_ctrl, 0x5510, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(hist_hs_he, 0x10000, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(hist_vs_ve, 0x0, vpp_sel);
	}
}

void s5_clip_func_after_ootf(int mtx_gamut_mode,
			enum hdr_module_sel module_sel,
			enum vpp_index_e vpp_index)
{
	int clip_en = 0;
	int clip_max = 0;
	unsigned int adps_ctrl;
	int vpp_sel = 0;/*0xfe;*/

	/* if Dynamic TMO+ enable : clip_en = 1 clip_max = 524288
	 * (hdr_process_select is HDR_SDR or HDR10P_SDR);
	 * else if mtx_gamut_mode = 1 : clip_en = 0/1 clip_max = 524288;
	 * else if  mtx_gamut_mode == 2 : clip_en = 1 clip_max = 393216;
	 * else : clip_en = 0 clip_max = 524288;
	 */
	if (module_sel == VD1_HDR)
		adps_ctrl = S5_VD1_HDR2_ADPS_CTRL;
	else if (module_sel == S5_VD1_SLICE1)
		adps_ctrl = S5_VD1_SLICE1_HDR2_ADPS_CTRL;
	else if (module_sel == S5_VD1_SLICE2)
		adps_ctrl = S5_VD1_SLICE2_HDR2_ADPS_CTRL;
	else if (module_sel == S5_VD1_SLICE3)
		adps_ctrl = S5_VD1_SLICE3_HDR2_ADPS_CTRL;
	else if (module_sel == VD2_HDR)
		adps_ctrl = S5_VD2_HDR2_ADPS_CTRL;
	else if (module_sel == OSD1_HDR)
		adps_ctrl = S5_OSD1_HDR2_ADPS_CTRL;
	else
		return;

	if (mtx_gamut_mode == 1) {
		clip_max = 524288 >> 14;
	} else if (mtx_gamut_mode == 2) {
		clip_en = 1;
		clip_max = 393216 >> 14;
	} else {
		clip_en = 0;
		clip_max = 524288 >> 14;
	}

	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(adps_ctrl,
		clip_en, 7, 1, vpp_sel);
	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(adps_ctrl,
		clip_max, 8, 6, vpp_sel);
}

void s5_hdr_gclk_ctrl_switch(enum hdr_module_sel module_sel,
	enum hdr_process_sel hdr_process_select,
	int vpp_sel)
{
	bool enable = true;

	/* only support T3 */
	if (get_cpu_type() != MESON_CPU_MAJOR_ID_T3 &&
		get_cpu_type() != MESON_CPU_MAJOR_ID_T5W)
		return;

	if (hdr_process_select & HDR_BYPASS)
		enable = false;
	else
		enable = true;

	if (enable) {
		/* async: 0, rdma delay a vsync */
		if (module_sel == VD1_HDR)
			vpu_module_clk_enable(vpp_sel, VD1_HDR_CORE, 0);
		else if (module_sel == VD2_HDR)
			vpu_module_clk_enable(vpp_sel, VD2_HDR_CORE, 0);
		else if (module_sel == OSD1_HDR)
			vpu_module_clk_enable(vpp_sel, OSD1_HDR_CORE, 0);
		else if (module_sel == OSD2_HDR)
			vpu_module_clk_enable(vpp_sel, OSD2_HDR_CORE, 0);
		else if (module_sel == OSD3_HDR)
			vpu_module_clk_enable(vpp_sel, OSD3_HDR_CORE, 0);
	} else {
		/* async: 0, rdma delay a vsync */
		if (module_sel == VD1_HDR)
			vpu_module_clk_disable(vpp_sel, VD1_HDR_CORE, 0);
		else if (module_sel == VD2_HDR)
			vpu_module_clk_disable(vpp_sel, VD2_HDR_CORE, 0);
		else if (module_sel == OSD1_HDR)
			vpu_module_clk_disable(vpp_sel, OSD1_HDR_CORE, 0);
		else if (module_sel == OSD2_HDR)
			vpu_module_clk_disable(vpp_sel, OSD2_HDR_CORE, 0);
		else if (module_sel == OSD3_HDR)
			vpu_module_clk_disable(vpp_sel, OSD3_HDR_CORE, 0);
	}
}

void set_vpu_lut_dma_mif(struct VPU_LUT_DMA_t      *vpu_lut_dma)
{
	u32  mif_num = 0;
	u32  lut_reg_sel8_15 = 0;
	u32  VPU_DMA_RDMIF_BADR0;
	u32  VPU_DMA_RDMIF_BADR1;
	u32  VPU_DMA_RDMIF_BADR2;
	u32  VPU_DMA_RDMIF_BADR3;
	u32  VPU_DMA_RDMIF_CTRL;

	if (vpu_lut_dma->dma_id == LDIM_DMA_ID) {
		mif_num = 0;
		VPU_DMA_RDMIF_BADR0 = VPU_DMA_RDMIF0_BADR0;
		VPU_DMA_RDMIF_BADR1 = VPU_DMA_RDMIF0_BADR1;
		VPU_DMA_RDMIF_BADR2 = VPU_DMA_RDMIF0_BADR2;
		VPU_DMA_RDMIF_BADR3 = VPU_DMA_RDMIF0_BADR3;
		VPU_DMA_RDMIF_CTRL  = VPU_DMA_RDMIF0_CTRL;
	} else if (vpu_lut_dma->dma_id == DI_FG_DMA_ID) {
		mif_num = 1;
		VPU_DMA_RDMIF_BADR0 = VPU_DMA_RDMIF1_BADR0;
		VPU_DMA_RDMIF_BADR1 = VPU_DMA_RDMIF1_BADR1;
		VPU_DMA_RDMIF_BADR2 = VPU_DMA_RDMIF1_BADR2;
		VPU_DMA_RDMIF_BADR3 = VPU_DMA_RDMIF1_BADR3;
		VPU_DMA_RDMIF_CTRL  = VPU_DMA_RDMIF1_CTRL;
	} else if (vpu_lut_dma->dma_id == VD1S0_FG_DMA_ID) {
		mif_num = 2;
		VPU_DMA_RDMIF_BADR0 = VPU_DMA_RDMIF2_BADR0;
		VPU_DMA_RDMIF_BADR1 = VPU_DMA_RDMIF2_BADR1;
		VPU_DMA_RDMIF_BADR2 = VPU_DMA_RDMIF2_BADR2;
		VPU_DMA_RDMIF_BADR3 = VPU_DMA_RDMIF2_BADR3;
		VPU_DMA_RDMIF_CTRL  = VPU_DMA_RDMIF2_CTRL;
	} else if (vpu_lut_dma->dma_id == VD1S1_FG_DMA_ID) {
		mif_num = 3;
		VPU_DMA_RDMIF_BADR0 = VPU_DMA_RDMIF3_BADR0;
		VPU_DMA_RDMIF_BADR1 = VPU_DMA_RDMIF3_BADR1;
		VPU_DMA_RDMIF_BADR2 = VPU_DMA_RDMIF3_BADR2;
		VPU_DMA_RDMIF_BADR3 = VPU_DMA_RDMIF3_BADR3;
		VPU_DMA_RDMIF_CTRL  = VPU_DMA_RDMIF3_CTRL;
	} else if (vpu_lut_dma->dma_id == VD1S2_FG_DMA_ID) {
		mif_num = 4;
		VPU_DMA_RDMIF_BADR0 = VPU_DMA_RDMIF4_BADR0;
		VPU_DMA_RDMIF_BADR1 = VPU_DMA_RDMIF4_BADR1;
		VPU_DMA_RDMIF_BADR2 = VPU_DMA_RDMIF4_BADR2;
		VPU_DMA_RDMIF_BADR3 = VPU_DMA_RDMIF4_BADR3;
		VPU_DMA_RDMIF_CTRL  = VPU_DMA_RDMIF4_CTRL;
	} else if (vpu_lut_dma->dma_id == VD1S3_FG_DMA_ID) {
		mif_num = 5;
		VPU_DMA_RDMIF_BADR0 = VPU_DMA_RDMIF5_BADR0;
		VPU_DMA_RDMIF_BADR1 = VPU_DMA_RDMIF5_BADR1;
		VPU_DMA_RDMIF_BADR2 = VPU_DMA_RDMIF5_BADR2;
		VPU_DMA_RDMIF_BADR3 = VPU_DMA_RDMIF5_BADR3;
		VPU_DMA_RDMIF_CTRL  = VPU_DMA_RDMIF5_CTRL;
	} else if (vpu_lut_dma->dma_id == VD2_FG_DMA_ID) {
		mif_num = 6;
		VPU_DMA_RDMIF_BADR0 = VPU_DMA_RDMIF6_BADR0;
		VPU_DMA_RDMIF_BADR1 = VPU_DMA_RDMIF6_BADR1;
		VPU_DMA_RDMIF_BADR2 = VPU_DMA_RDMIF6_BADR2;
		VPU_DMA_RDMIF_BADR3 = VPU_DMA_RDMIF6_BADR3;
		VPU_DMA_RDMIF_CTRL  = VPU_DMA_RDMIF6_CTRL;
	} else if (vpu_lut_dma->dma_id == HDR2_DMA_ID) {
		mif_num = 7;
		VPU_DMA_RDMIF_BADR0 = VPU_DMA_RDMIF7_BADR0;
		VPU_DMA_RDMIF_BADR1 = VPU_DMA_RDMIF7_BADR1;
		VPU_DMA_RDMIF_BADR2 = VPU_DMA_RDMIF7_BADR2;
		VPU_DMA_RDMIF_BADR3 = VPU_DMA_RDMIF7_BADR3;
		VPU_DMA_RDMIF_CTRL  = VPU_DMA_RDMIF7_CTRL;
	} else if (vpu_lut_dma->dma_id == VPP_LUT3D_DMA_ID) {
		mif_num = 8;
		lut_reg_sel8_15 = 1;
		VPU_DMA_RDMIF_BADR0 = VPU_DMA_RDMIF0_BADR0;
		VPU_DMA_RDMIF_BADR1 = VPU_DMA_RDMIF0_BADR1;
		VPU_DMA_RDMIF_BADR2 = VPU_DMA_RDMIF0_BADR2;
		VPU_DMA_RDMIF_BADR3 = VPU_DMA_RDMIF0_BADR3;
		VPU_DMA_RDMIF_CTRL  = VPU_DMA_RDMIF0_CTRL;
	} else {
		mif_num = 7;
		VPU_DMA_RDMIF_BADR0 = VPU_DMA_RDMIF7_BADR0;
		VPU_DMA_RDMIF_BADR1 = VPU_DMA_RDMIF7_BADR1;
		VPU_DMA_RDMIF_BADR2 = VPU_DMA_RDMIF7_BADR2;
		VPU_DMA_RDMIF_BADR3 = VPU_DMA_RDMIF7_BADR3;
		VPU_DMA_RDMIF_CTRL  = VPU_DMA_RDMIF7_CTRL;
	}

	WRITE_VPP_REG_BITS_S5(VPU_DMA_RDMIF_SEL, lut_reg_sel8_15, 0, 1);

	WRITE_VPP_REG_S5(VPU_DMA_RDMIF_BADR0, vpu_lut_dma->mif_baddr[mif_num][0]);
	WRITE_VPP_REG_S5(VPU_DMA_RDMIF_BADR1, vpu_lut_dma->mif_baddr[mif_num][1]);
	WRITE_VPP_REG_S5(VPU_DMA_RDMIF_BADR2, vpu_lut_dma->mif_baddr[mif_num][2]);
	WRITE_VPP_REG_S5(VPU_DMA_RDMIF_BADR3, vpu_lut_dma->mif_baddr[mif_num][3]);

	/*reg_rd0_stride*/
	WRITE_VPP_REG_BITS_S5(VPU_DMA_RDMIF_CTRL,
		vpu_lut_dma->chan_rd_bytes_num[mif_num], 0, 13);

	/*Bit 13 little_endian*/
	WRITE_VPP_REG_BITS_S5(VPU_DMA_RDMIF_CTRL,
		vpu_lut_dma->chan_little_endian[mif_num], 13, 1);
	/*Bit 14 swap_64bit*/
	WRITE_VPP_REG_BITS_S5(VPU_DMA_RDMIF_CTRL,
		vpu_lut_dma->chan_swap_64bit[mif_num], 14, 1);

	/*Bit 23:16*/
	/*reg_rd0_enable_int,*/
	/*channel0 select interrupt source*/
	WRITE_VPP_REG_BITS_S5(VPU_DMA_RDMIF_CTRL,
		vpu_lut_dma->chan_sel_src_num[mif_num],  16, 8);
	pr_csc(64, "set vpu_lut_dma_mif done\n");
}

void init_vpu_lut_dma(struct VPU_LUT_DMA_t    *vpu_lut_dma)
{
	memset((void *)vpu_lut_dma, 0, sizeof(struct VPU_LUT_DMA_t));

	//default: must identical to coef data in DDR BACKDOOR.
	vpu_lut_dma->reg_hdr_dma_sel_vd1s0 = 1;
	vpu_lut_dma->reg_hdr_dma_sel_vd1s1 = 2;
	vpu_lut_dma->reg_hdr_dma_sel_vd1s2 = 3;
	vpu_lut_dma->reg_hdr_dma_sel_vd1s3 = 4;
	vpu_lut_dma->reg_hdr_dma_sel_vd2 = 5;
	vpu_lut_dma->reg_hdr_dma_sel_osd1 = 6;
	vpu_lut_dma->reg_hdr_dma_sel_osd2 = 7;
	vpu_lut_dma->reg_hdr_dma_sel_osd3 = 8;
}

void set_vpu_lut_dma(struct VPU_LUT_DMA_t *vpu_lut_dma)
{
	unsigned int val;

	val = READ_VPP_REG_S5(VIU_DMA_CTRL0);
	val |= vpu_lut_dma->reg_vd1s0_hdr_dma_mode << 0 |
		vpu_lut_dma->reg_vd1s1_hdr_dma_mode << 1 |
		vpu_lut_dma->reg_vd1s2_hdr_dma_mode << 2 |
		vpu_lut_dma->reg_vd1s3_hdr_dma_mode << 3 |
		vpu_lut_dma->reg_vd2_hdr_dma_mode << 4 |
		vpu_lut_dma->reg_osd1_hdr_dma_mode << 6 |
		vpu_lut_dma->reg_osd2_hdr_dma_mode << 7 |
		vpu_lut_dma->reg_osd3_hdr_dma_mode << 8;
	WRITE_VPP_REG_S5(VIU_DMA_CTRL0, val);
	val = READ_VPP_REG_S5(VIU_DMA_CTRL1);
	val |= vpu_lut_dma->reg_hdr_dma_sel_vd1s0 << 0  |
		vpu_lut_dma->reg_hdr_dma_sel_vd1s1 << 4 |
		vpu_lut_dma->reg_hdr_dma_sel_vd1s2 << 8 |
		vpu_lut_dma->reg_hdr_dma_sel_vd1s3 << 12 |
		vpu_lut_dma->reg_hdr_dma_sel_vd2 << 16 |
		vpu_lut_dma->reg_hdr_dma_sel_osd1 << 20 |
		vpu_lut_dma->reg_hdr_dma_sel_osd2 << 24 |
		vpu_lut_dma->reg_hdr_dma_sel_osd3 << 28;
	WRITE_VPP_REG_S5(VIU_DMA_CTRL1, val);

	set_vpu_lut_dma_mif(vpu_lut_dma);

	//axird3_req_en,
	//dut.u_chip_top.
	//u_chip_core.
	//u_vpu_hdmi_top.
	//u_vpu_intf_async_top.
	//u_vpu_intf_axird_async_3.
	//req_en
	//WRITE_VPP_REG_BITS_S5(VPU_INTF_CTRL, 1, 22, 1);
}

struct eotf_lut_s {
	s64 eotf_lut_0 : 20;
	s64 eotf_lut_1 : 20;
	s64 eotf_lut_2 : 20;
	s64 eotf_lut__rev : 4;
	s64 eotf_lut_3 : 16;
	s64 eotf_lut_4 : 20;
	s64 eotf_lut_5 : 20;
	s64 reserve : 8;
};

struct oetf_lut_s {
	s64 oetf_lut_0 : 12;
	s64 oetf_lut_1 : 12;
	s64 oetf_lut_2 : 12;
	s64 oetf_lut_3 : 12;
	s64 oetf_lut_4 : 12;
	s64 oetf_lut_res : 4;
	s64 oetf_lut_5 : 8;
	s64 oetf_lut_6 : 12;
	s64 oetf_lut_7 : 12;
	s64 oetf_lut_8 : 12;
	s64 oetf_lut_9 : 12;
	s64 reserve : 8;
};

struct ootf_lut_s {
	s16 ootf_lut_0;
	s16 ootf_lut_1;
	s16 ootf_lut_2;
	s16 ootf_lut_3;
	s16 ootf_lut_4;
	s16 ootf_lut_5;
	s16 ootf_lut_6;
	s16 ootf_lut_7;
};

struct cgain_lut_s {
	s64 cgain_lut_0 : 12;
	s64 cgain_lut_1 : 12;
	s64 cgain_lut_2 : 12;
	s64 cgain_lut_3 : 12;
	s64 cgain_lut_4 : 12;
	s64 cgain_lut_res : 4;
	s64 cgain_lut_5 : 8;
	s64 cgain_lut_6 : 12;
	s64 cgain_lut_7 : 12;
	s64 cgain_lut_8 : 12;
	s64 cgain_lut_9 : 12;
	s64 reserve : 8;
};

/*size of each lut struct is 128bit, count = 65*/
struct dma_lut_address {
	struct eotf_lut_s eotf_lut[24];
	struct oetf_lut_s oetf_lut[15];
	struct ootf_lut_s ootf_lut[19];
	struct cgain_lut_s cgain_lut[7];
};

enum dma_lut_off_enum {
	OFFSET_VD1S0 = 0,
	OFFSET_VD1S1 = 1,
	OFFSET_VD1S2 = 2,
	OFFSET_VD1S3 = 3,
	OFFSET_VD2 = 4,
	OFFSET_OSD1 = 5,
	OFFSET_OSD2 = 6,
	OFFSET_OSD3 = 7
};

static const char *dma_lut_off_str[8] = {
	"VD1S0",
	"VD1S1",
	"VD1S2",
	"VD1S3",
	"VD2",
	"OSD1",
	"OSD2",
	"OSD3"
};

/*vd1s0+vd1s1+vd1s2+vd1s3+vd2+osd1+osd2+osd3*/
/*each dma 128bit*/
/*2 dma header + 65 body + 1 dma tail*/
static struct dma_lut_address lut_addr;
static dma_addr_t dma_paddr;
static void *dma_vaddr;
#define DMA_LUT_BODY_COUNT 65
#define DMA_HDR_COUNT 8
#define DMA_COUNT_SINGLE_HDR (2 + DMA_LUT_BODY_COUNT + 1)
#define DMA_COUNT_TOTAL_HDR (DMA_COUNT_SINGLE_HDR * DMA_HDR_COUNT)
#define DMA_SIZE_SINGLE_HDR (DMA_COUNT_SINGLE_HDR * 128 / 8)
#define DMA_SIZE_TOTAL_HDR (DMA_SIZE_SINGLE_HDR * DMA_HDR_COUNT)

void fill_dma_buffer(enum hdr_module_sel module_sel,
	struct hdr_proc_lut_param_s *hdr_lut_param, struct dma_lut_address *dma_addr)
{
	int i = 0;
	int j = 0;

	/* fill eotf lut for dma*/
	for (i = 0; i < 24; i++) {
		/*dma data is 128bit,eotf data is 20bit,every 6 eotf data forms dma data*/
		if (i < 23) {
			dma_addr->eotf_lut[i].eotf_lut_0 = hdr_lut_param->eotf_lut[j];
			dma_addr->eotf_lut[i].eotf_lut_1 = hdr_lut_param->eotf_lut[j + 1];
			dma_addr->eotf_lut[i].eotf_lut_2 = hdr_lut_param->eotf_lut[j + 2];
			dma_addr->eotf_lut[i].eotf_lut__rev =
				hdr_lut_param->eotf_lut[j + 3] & 0xf;
			dma_addr->eotf_lut[i].eotf_lut_3 =
				(hdr_lut_param->eotf_lut[j + 3] & 0xffff0) >> 4;
			dma_addr->eotf_lut[i].eotf_lut_4 = hdr_lut_param->eotf_lut[j + 4];
			dma_addr->eotf_lut[i].eotf_lut_5 = hdr_lut_param->eotf_lut[j + 5];
			dma_addr->eotf_lut[i].reserve = 0;
			j += 6;
			/*pr_csc(64, "%s:m=%d,[%3d]:[%5x,%5x,%5x,%5x,%5x,%5x,%5x,%5x\n",*/
				   /*__func__, module_sel, j,*/
				   /*dma_addr->eotf_lut[i].eotf_lut_0,*/
				   /*dma_addr->eotf_lut[i].eotf_lut_1,*/
				   /*dma_addr->eotf_lut[i].eotf_lut_2,*/
				   /*dma_addr->eotf_lut[i].eotf_lut__rev,*/
				   /*dma_addr->eotf_lut[i].eotf_lut_3,*/
				   /*dma_addr->eotf_lut[i].eotf_lut_4,*/
				   /*dma_addr->eotf_lut[i].eotf_lut_5,*/
				   /*dma_addr->eotf_lut[i].reserve);*/
		} else {/*23*6=138, handle the last 5 eotf data*/
			dma_addr->eotf_lut[i].eotf_lut_0 = hdr_lut_param->eotf_lut[j];/*138*/
			dma_addr->eotf_lut[i].eotf_lut_1 = hdr_lut_param->eotf_lut[j + 1];/*139*/
			dma_addr->eotf_lut[i].eotf_lut_2 = hdr_lut_param->eotf_lut[j + 2];/*140*/
			dma_addr->eotf_lut[i].eotf_lut__rev =
				hdr_lut_param->eotf_lut[j + 3] & 0xf;
			dma_addr->eotf_lut[i].eotf_lut_3 =
				(hdr_lut_param->eotf_lut[j + 3] & 0xffff0) >> 4;/*141*/
			dma_addr->eotf_lut[i].eotf_lut_4 = hdr_lut_param->eotf_lut[j + 4];/*142*/
			dma_addr->eotf_lut[i].eotf_lut_5 = 0;
			dma_addr->eotf_lut[i].reserve = 0;
			j += 5;
			/*pr_csc(64, "%s:m=%d,[%3d]:[%5x,%5x,%5x,%5x,%5x,%5x,%5x,%5x]\n",*/
				   /*__func__, module_sel, j,*/
				   /*hdr_lut_param->eotf_lut[j], hdr_lut_param->eotf_lut[j + 1],*/
				   /*hdr_lut_param->eotf_lut[j + 2],*/
				   /*hdr_lut_param->eotf_lut[j + 3],*/
				   /*hdr_lut_param->eotf_lut[j + 4]);*/
		}
	}

	/* fill oetf lut for dma*/
	j = 0;
	for (i = 0; i < 15; i++) {
		/*dma data is 128bit,oetf data is 12bit,every 10 oetf data forms dma data*/
		if (i < 14) {
			dma_addr->oetf_lut[i].oetf_lut_0 = hdr_lut_param->oetf_lut[j];
			dma_addr->oetf_lut[i].oetf_lut_1 = hdr_lut_param->oetf_lut[j + 1];
			dma_addr->oetf_lut[i].oetf_lut_2 = hdr_lut_param->oetf_lut[j + 2];
			dma_addr->oetf_lut[i].oetf_lut_3 = hdr_lut_param->oetf_lut[j + 3];
			dma_addr->oetf_lut[i].oetf_lut_4 = hdr_lut_param->oetf_lut[j + 4];
			dma_addr->oetf_lut[i].oetf_lut_res =
				hdr_lut_param->oetf_lut[j + 5] & 0xf;
			dma_addr->oetf_lut[i].oetf_lut_5 =
				(hdr_lut_param->oetf_lut[j + 5] & 0xff0) >> 4;
			dma_addr->oetf_lut[i].oetf_lut_6 = hdr_lut_param->oetf_lut[j + 6];
			dma_addr->oetf_lut[i].oetf_lut_7 = hdr_lut_param->oetf_lut[j + 7];
			dma_addr->oetf_lut[i].oetf_lut_8 = hdr_lut_param->oetf_lut[j + 8];
			dma_addr->oetf_lut[i].oetf_lut_9 = hdr_lut_param->oetf_lut[j + 9];
			dma_addr->oetf_lut[i].reserve = 0x00;
			j += 10;
		} else {/*14*10=140, handle the last 9 oetf data*/
			dma_addr->oetf_lut[i].oetf_lut_0 = hdr_lut_param->oetf_lut[j];//140
			dma_addr->oetf_lut[i].oetf_lut_1 = hdr_lut_param->oetf_lut[j + 1];//141
			dma_addr->oetf_lut[i].oetf_lut_2 = hdr_lut_param->oetf_lut[j + 2];//142
			dma_addr->oetf_lut[i].oetf_lut_3 = hdr_lut_param->oetf_lut[j + 3];//143
			dma_addr->oetf_lut[i].oetf_lut_4 = hdr_lut_param->oetf_lut[j + 4];//144
			dma_addr->oetf_lut[i].oetf_lut_res =
				hdr_lut_param->oetf_lut[j + 5] & 0xf;
			dma_addr->oetf_lut[i].oetf_lut_5 =
				(hdr_lut_param->oetf_lut[j + 5] & 0xff0) >> 4;//145
			dma_addr->oetf_lut[i].oetf_lut_6 = hdr_lut_param->oetf_lut[j + 6];//146
			dma_addr->oetf_lut[i].oetf_lut_7 = hdr_lut_param->oetf_lut[j + 7];//147
			dma_addr->oetf_lut[i].oetf_lut_8 = hdr_lut_param->oetf_lut[j + 8];//148

			dma_addr->oetf_lut[i].oetf_lut_9 = 0;
			dma_addr->oetf_lut[i].reserve = 0;
			j += 10;
		}
	}

	/* fill ootf lut for dma*/
	j = 0;
	for (i = 0; i < 19; i++) {
		/*dma data is 128bit,ootf data is 16bit,every 8 oetf data forms dma data.*/
		if (i < 18) {
			dma_addr->ootf_lut[i].ootf_lut_0 = hdr_lut_param->ogain_lut[j];
			dma_addr->ootf_lut[i].ootf_lut_1 = hdr_lut_param->ogain_lut[j + 1];
			dma_addr->ootf_lut[i].ootf_lut_2 = hdr_lut_param->ogain_lut[j + 2];
			dma_addr->ootf_lut[i].ootf_lut_3 = hdr_lut_param->ogain_lut[j + 3];
			dma_addr->ootf_lut[i].ootf_lut_4 = hdr_lut_param->ogain_lut[j + 4];
			dma_addr->ootf_lut[i].ootf_lut_5 = hdr_lut_param->ogain_lut[j + 5];
			dma_addr->ootf_lut[i].ootf_lut_6 = hdr_lut_param->ogain_lut[j + 6];
			dma_addr->ootf_lut[i].ootf_lut_7 = hdr_lut_param->ogain_lut[j + 7];
			j += 8;
		} else {/*18*8=144, handle the last 5 ootf data*/
			dma_addr->ootf_lut[i].ootf_lut_0 = hdr_lut_param->ogain_lut[j];//144
			dma_addr->ootf_lut[i].ootf_lut_1 = hdr_lut_param->ogain_lut[j + 1];//145
			dma_addr->ootf_lut[i].ootf_lut_2 = hdr_lut_param->ogain_lut[j + 2];//146
			dma_addr->ootf_lut[i].ootf_lut_3 = hdr_lut_param->ogain_lut[j + 3];//147
			dma_addr->ootf_lut[i].ootf_lut_4 = hdr_lut_param->ogain_lut[j + 4];//148
			dma_addr->ootf_lut[i].ootf_lut_5 = 0;
			dma_addr->ootf_lut[i].ootf_lut_6 = 0;
			dma_addr->ootf_lut[i].ootf_lut_7 = 0;
			j += 8;
		}
	}

	/* fill cgain lut for dma */
	j = 0;
	for (i = 0; i < 7; i++) {
		/*dma data is 128bit,ootf data is 12bit,every 10 cgain data forms dma data*/
		if (i < 6) {
			dma_addr->cgain_lut[i].cgain_lut_0 = hdr_lut_param->cgain_lut[j];
			dma_addr->cgain_lut[i].cgain_lut_1 = hdr_lut_param->cgain_lut[j + 1];
			dma_addr->cgain_lut[i].cgain_lut_2 = hdr_lut_param->cgain_lut[j + 2];
			dma_addr->cgain_lut[i].cgain_lut_3 = hdr_lut_param->cgain_lut[j + 3];
			dma_addr->cgain_lut[i].cgain_lut_4 = hdr_lut_param->cgain_lut[j + 4];
			dma_addr->cgain_lut[i].cgain_lut_res =
				hdr_lut_param->cgain_lut[j + 5] & 0xf;
			dma_addr->cgain_lut[i].cgain_lut_5 =
				(hdr_lut_param->cgain_lut[j + 5] & 0xff0) >> 4;
			dma_addr->cgain_lut[i].cgain_lut_6 = hdr_lut_param->cgain_lut[j + 6];
			dma_addr->cgain_lut[i].cgain_lut_7 = hdr_lut_param->cgain_lut[j + 7];
			dma_addr->cgain_lut[i].cgain_lut_8 = hdr_lut_param->cgain_lut[j + 8];
			dma_addr->cgain_lut[i].cgain_lut_9 = hdr_lut_param->cgain_lut[j + 9];
			dma_addr->cgain_lut[i].reserve = 0x00;
			j += 10;
		} else {/*10*6=60, handle the last 5 ootf data*/
			dma_addr->cgain_lut[i].cgain_lut_0 = hdr_lut_param->cgain_lut[j];//60
			dma_addr->cgain_lut[i].cgain_lut_1 = hdr_lut_param->cgain_lut[j + 1];//61
			dma_addr->cgain_lut[i].cgain_lut_2 = hdr_lut_param->cgain_lut[j + 2];//62
			dma_addr->cgain_lut[i].cgain_lut_3 = hdr_lut_param->cgain_lut[j + 3];//63
			dma_addr->cgain_lut[i].cgain_lut_4 = hdr_lut_param->cgain_lut[j + 4];//64
			dma_addr->cgain_lut[i].cgain_lut_res = 0;
			dma_addr->cgain_lut[i].cgain_lut_5 = 0;
			dma_addr->cgain_lut[i].cgain_lut_6 = 0;
			dma_addr->cgain_lut[i].cgain_lut_7 = 0;
			dma_addr->cgain_lut[i].cgain_lut_8 = 0;
			dma_addr->cgain_lut[i].cgain_lut_9 = 0;
			dma_addr->cgain_lut[i].reserve = 0x00;
			j += 10;
		}
	}
}

//struct hdr_proc_lut_param_s s5_hdr_lut_param;
void fill_dma_header_and_body(void *p_dma_vaddr,
	struct dma_lut_address *lut_addr, enum hdr_module_sel module_sel)
{
	u32 *b;
	ulong body_size;
	ulong total_size;
	u8 reg_dma_sel_vd1_slice0 = 1;
	u8 reg_dma_sel_vd1_slice1 = 2;
	u8 reg_dma_sel_vd1_slice2 = 3;
	u8 reg_dma_sel_vd1_slice3 = 4;
	u8 reg_dma_sel_vd2 = 5;
	u8 reg_dma_sel_osd1 = 6;
	u8 reg_dma_sel_osd2 = 7;
	u8 reg_dma_sel_osd3 = 8;
	u8 sel_val = 0;
	int i = 0;

	total_size = sizeof(struct dma_lut_address) + 16 + 16; /*128bit+128bit header*/
	body_size = sizeof(struct dma_lut_address);
	memset(p_dma_vaddr, 0x0, total_size);

	if (module_sel == OSD1_HDR)
		sel_val = reg_dma_sel_osd1;
	else if (module_sel == OSD2_HDR)
		sel_val = reg_dma_sel_osd2;
	else if (module_sel == OSD3_HDR)
		sel_val = reg_dma_sel_osd3;
	else if (module_sel == VD1_HDR)
		sel_val = reg_dma_sel_vd1_slice0;
	else if (module_sel == S5_VD1_SLICE1)
		sel_val = reg_dma_sel_vd1_slice1;
	else if (module_sel == S5_VD1_SLICE2)
		sel_val = reg_dma_sel_vd1_slice2;
	else if (module_sel == S5_VD1_SLICE3)
		sel_val = reg_dma_sel_vd1_slice3;
	else if (module_sel == VD2_HDR)
		sel_val = reg_dma_sel_vd2;
	else
		sel_val = reg_dma_sel_osd1;

	b = (u32 *)(p_dma_vaddr);
	if (dma_sel == 0) {
		b[0] = 0x60000000;
		b[1] = 0x00000000;
		b[2] = 0x00000000;
		b[3] = 0x00000000;
	} else if (dma_sel == 1) {
		b[0] = 0x00000000;
		b[1] = 0x60000000;
		b[2] = 0x00000000;
		b[3] = 0x00000000;
	} else if (dma_sel == 2) {
		b[0] = 0x00000000;
		b[1] = 0x00000000;
		b[2] = 0x60000000;
		b[3] = 0x00000000;
	} else if (dma_sel == 3) {
		b[0] = 0x00000000;
		b[1] = 0x00000000;
		b[2] = 0x00000000;
		//b[3] = 0x60000000;
		b[3] = sel_val << 28;
	}

	pr_csc(64, "copy header: module=%d,[0x%x, 0x%x, 0x%x, 0x%x], addr=%px\n",
		   module_sel, b[3], b[2], b[1], b[0], &b[0]);

	if (dma_sel1 == 0) {
		b[4] = 0xf2000000;
		b[5] = 0x00000000;
		b[6] = 0x00000000;
		b[7] = 0x00000000;
	} else if (dma_sel1 == 1) {
		b[4] = 0x00000000;
		b[5] = 0xf2000000;
		b[6] = 0x00000000;
		b[7] = 0x00000000;
	} else if (dma_sel1 == 2) {
		b[4] = 0x00000000;
		b[5] = 0x00000000;
		b[6] = 0xf2000000;
		b[7] = 0x00000000;
	} else if (dma_sel1 == 3) {
		b[4] = 0x00000000;
		b[5] = 0x00000000;
		b[6] = 0x00000000;
		b[7] = 0xf2000000;
	} else if (dma_sel1 == 5) {
		b[4] = 0x00000000;
		b[5] = 0xf2f2f2f2;
		b[6] = 0xf2f2f2f2;
		b[7] = 0xf2f2f2f2;
	} else if (dma_sel1 == 6) {
		b[4] = 0xf2f2f2f2;
		b[5] = 0x00000000;
		b[6] = 0xf2f2f2f2;
		b[7] = 0xf2f2f2f2;
	} else if (dma_sel1 == 7) {
		b[4] = 0xf2f2f2f2;
		b[5] = 0xf2f2f2f2;
		b[6] = 0x00000000;
		b[7] = 0xf2f2f2f2;
	} else if (dma_sel1 == 8) {
		b[4] = 0xf2f2f2f2;
		b[5] = 0xf2f2f2f2;
		b[6] = 0xf2f2f2f2;
		b[7] = 0x00000000;
	}

	pr_csc(64, "copy header: module=%d,[0x%x,0x%x,0x%x,0x%x],addr=%px\n",
		   module_sel, b[7], b[6], b[5], b[4], &b[4]);

	memcpy(&b[8], lut_addr, sizeof(struct dma_lut_address));

	pr_csc(64, "copy body  : size=%ld\n", body_size);

	if (debug_csc & 128) {
		for (i = 0; i < body_size / 4; i = i + 4)
			pr_info("%3d: %8x, %8x, %8x, %8x\n", i,
			b[8 + i + 3], b[8 + i + 2], b[8 + i + 1], b[8 + i + 0]);
	}
}

void fill_dma_tail(void *p_dma_vaddr, enum hdr_module_sel module_sel)
{
	u32 *b;
	ulong offset;

	offset = (DMA_LUT_BODY_COUNT + 2) * 4;/*2 header*/
	b = (u32 *)(p_dma_vaddr);

	if (dma_sel1 == 0) {
		b[offset + 0] = 0xf0000000;
		b[offset + 1] = 0x00000000;
		b[offset + 2] = 0x00000000;
		b[offset + 3] = 0x00000000;
	} else if (dma_sel1 == 1) {
		b[offset + 0] = 0x00000000;
		b[offset + 1] = 0xf0000000;
		b[offset + 2] = 0x00000000;
		b[offset + 3] = 0x00000000;
	} else if (dma_sel1 == 2) {
		b[offset + 0] = 0x00000000;
		b[offset + 1] = 0x00000000;
		b[offset + 2] = 0xf0000000;
		b[offset + 3] = 0x00000000;
	} else if (dma_sel1 == 3) {
		b[offset + 0] = 0x00000000;
		b[offset + 1] = 0x00000000;
		b[offset + 2] = 0x00000000;
		b[offset + 3] = 0xf0000000;
	} else if (dma_sel1 == 4) {
		b[offset + 0] = 0xf0000000;
		b[offset + 1] = 0xf0000000;
		b[offset + 2] = 0xf0000000;
		b[offset + 3] = 0xf0000000;
	} else if (dma_sel1 == 5) {
		b[offset + 0] = 0x00000000;
		b[offset + 1] = 0xf0000000;
		b[offset + 2] = 0xf0000000;
		b[offset + 3] = 0xf0000000;
	} else if (dma_sel1 == 6) {
		b[offset + 0] = 0xf0000000;
		b[offset + 1] = 0x00000000;
		b[offset + 2] = 0xf0000000;
		b[offset + 3] = 0xf0000000;
	} else if (dma_sel1 == 7) {
		b[offset + 0] = 0xf0000000;
		b[offset + 1] = 0xf0000000;
		b[offset + 2] = 0x00000000;
		b[offset + 3] = 0xf0000000;
	} else if (dma_sel1 == 8) {
		b[offset + 0] = 0xf0000000;
		b[offset + 1] = 0xf0000000;
		b[offset + 2] = 0xf0000000;
		b[offset + 3] = 0x00000000;
	}
	pr_csc(64, "copy tail  : module=%d,[0x%x,0x%x,0x%x,0x%x],addr=%px\n",
		   module_sel, b[offset + 3], b[offset + 2],
		   b[offset + 1], b[offset + 0], &b[offset]);
}

//static struct platform_device vecm_pdev;
struct device vecm_dev;
ulong alloc_size;

void hdr_lut_buffer_malloc(struct platform_device *pdev)
{
	vecm_dev = pdev->dev;
	alloc_size = DMA_SIZE_TOTAL_HDR;
	dma_vaddr = dma_alloc_coherent(&vecm_dev,
		alloc_size, &dma_paddr, GFP_KERNEL);
	pr_info("hdr dma_vaddr: %px\n", (u32 *)(dma_vaddr));
}

void hdr_lut_buffer_free(struct platform_device *pdev)
{
	vecm_dev = pdev->dev;
	dma_free_coherent(&vecm_dev, alloc_size, dma_vaddr, dma_paddr);
}

void vpu_lut_dma(enum hdr_module_sel module_sel,
	struct hdr_proc_lut_param_s *hdr_lut_param, enum LUT_DMA_ID_e dma_id)
{
	//===============================================
	// VPU LUT DMA:
	//===============================================
	u32 dma_id_int;
	struct VPU_LUT_DMA_t g_vpu_lut_dma;
	u32 dma_addr_offset;

	if (cpu_write_lut)
		return;
	if (!dma_vaddr)
		return;

	fill_dma_buffer(module_sel, hdr_lut_param, &lut_addr);
	init_vpu_lut_dma(&g_vpu_lut_dma);

	g_vpu_lut_dma.dma_id = dma_id;
	dma_id_int = (u32)(g_vpu_lut_dma.dma_id);
	if (module_sel == OSD1_HDR) {
		g_vpu_lut_dma.reg_osd1_hdr_dma_mode = 1;
		dma_addr_offset = DMA_SIZE_SINGLE_HDR * OFFSET_OSD1;
	} else if (module_sel == OSD2_HDR) {
		g_vpu_lut_dma.reg_osd2_hdr_dma_mode = 1;
		dma_addr_offset = DMA_SIZE_SINGLE_HDR * OFFSET_OSD2;
	} else if (module_sel == OSD3_HDR) {
		g_vpu_lut_dma.reg_osd3_hdr_dma_mode = 1;
		dma_addr_offset = DMA_SIZE_SINGLE_HDR * OFFSET_OSD3;
	} else if (module_sel == VD1_HDR) {
		g_vpu_lut_dma.reg_vd1s0_hdr_dma_mode = 1;
		dma_addr_offset = DMA_SIZE_SINGLE_HDR * OFFSET_VD1S0;
	} else if (module_sel == S5_VD1_SLICE1) {
		g_vpu_lut_dma.reg_vd1s1_hdr_dma_mode = 1;
		dma_addr_offset = DMA_SIZE_SINGLE_HDR * OFFSET_VD1S1;
	} else if (module_sel == S5_VD1_SLICE2) {
		g_vpu_lut_dma.reg_vd1s2_hdr_dma_mode = 1;
		dma_addr_offset = DMA_SIZE_SINGLE_HDR * OFFSET_VD1S2;
	} else if (module_sel == S5_VD1_SLICE3) {
		g_vpu_lut_dma.reg_vd1s3_hdr_dma_mode = 1;
		dma_addr_offset = DMA_SIZE_SINGLE_HDR * OFFSET_VD1S3;
	} else if (module_sel == VD2_HDR) {
		g_vpu_lut_dma.reg_vd2_hdr_dma_mode = 1;
		dma_addr_offset = DMA_SIZE_SINGLE_HDR * OFFSET_VD2;
	} else {
		g_vpu_lut_dma.reg_osd1_hdr_dma_mode = 1;
		dma_addr_offset = DMA_SIZE_SINGLE_HDR * OFFSET_OSD1;
	}
	/*0: config wr_mif 1: config rd_mif, only rd mif for s5*/
	g_vpu_lut_dma.rd_wr_sel = 1;
	fill_dma_header_and_body(dma_vaddr + dma_addr_offset, &lut_addr, module_sel);
	fill_dma_tail(dma_vaddr + dma_addr_offset, module_sel);
	g_vpu_lut_dma.mif_baddr[dma_id_int][0] = ((u32)(dma_paddr) >> 4);
	g_vpu_lut_dma.mif_baddr[dma_id_int][1] = ((u32)(dma_paddr) >> 4);
	g_vpu_lut_dma.mif_baddr[dma_id_int][2] = ((u32)(dma_paddr) >> 4);
	g_vpu_lut_dma.mif_baddr[dma_id_int][3] = ((u32)(dma_paddr) >> 4);

	g_vpu_lut_dma.chan_rd_bytes_num[dma_id_int] = DMA_COUNT_TOTAL_HDR;
	/*interrupt source,select viu_vsync_int_i[0]*/
	g_vpu_lut_dma.chan_sel_src_num[dma_id_int] = 1;
	set_vpu_lut_dma(&g_vpu_lut_dma);
}

void s5_hdr_reg_dump(unsigned int offset)
{
	unsigned int val;
	unsigned int i;
	unsigned int reg;
	unsigned int hdr_ctrl = S5_OSD1_HDR2_CTRL;
	unsigned int eotf_lut_addr_port = S5_VD1_EOTF_LUT_ADDR_PORT;
	unsigned int eotf_lut_data_port = S5_VD1_EOTF_LUT_DATA_PORT;
	unsigned int ootf_lut_addr_port = S5_VD1_OGAIN_LUT_ADDR_PORT;
	unsigned int ootf_lut_data_port = S5_VD1_OGAIN_LUT_DATA_PORT;
	unsigned int oetf_lut_addr_port = S5_VD1_OETF_LUT_ADDR_PORT;
	unsigned int oetf_lut_data_port = S5_VD1_OETF_LUT_DATA_PORT;
	unsigned int cgain_lut_addr_port = S5_VD1_CGAIN_LUT_ADDR_PORT;
	unsigned int cgain_lut_data_port = S5_VD1_CGAIN_LUT_DATA_PORT;

	if (offset == VD1_HDR) {
		hdr_ctrl = S5_VD1_HDR2_CTRL;
		eotf_lut_addr_port = S5_VD1_EOTF_LUT_ADDR_PORT;
		eotf_lut_data_port = S5_VD1_EOTF_LUT_DATA_PORT;
		ootf_lut_addr_port = S5_VD1_OGAIN_LUT_ADDR_PORT;
		ootf_lut_data_port = S5_VD1_OGAIN_LUT_DATA_PORT;
		oetf_lut_addr_port = S5_VD1_OETF_LUT_ADDR_PORT;
		oetf_lut_data_port = S5_VD1_OETF_LUT_DATA_PORT;
		cgain_lut_addr_port = S5_VD1_CGAIN_LUT_ADDR_PORT;
		cgain_lut_data_port = S5_VD1_CGAIN_LUT_DATA_PORT;
	} else if (offset == S5_VD1_SLICE1) {
		hdr_ctrl = S5_VD1_SLICE1_HDR2_CTRL;
		eotf_lut_addr_port = S5_VD1_SLICE1_EOTF_LUT_ADDR_PORT;
		eotf_lut_data_port = S5_VD1_SLICE1_EOTF_LUT_DATA_PORT;
		ootf_lut_addr_port = S5_VD1_SLICE1_OGAIN_LUT_ADDR_PORT;
		ootf_lut_data_port = S5_VD1_SLICE1_OGAIN_LUT_DATA_PORT;
		oetf_lut_addr_port = S5_VD1_SLICE1_OETF_LUT_ADDR_PORT;
		oetf_lut_data_port = S5_VD1_SLICE1_OETF_LUT_DATA_PORT;
		cgain_lut_addr_port = S5_VD1_SLICE1_CGAIN_LUT_ADDR_PORT;
		cgain_lut_data_port = S5_VD1_SLICE1_CGAIN_LUT_DATA_PORT;
	} else if (offset == S5_VD1_SLICE2) {
		hdr_ctrl = S5_VD1_SLICE2_HDR2_CTRL;
		eotf_lut_addr_port = S5_VD1_SLICE2_EOTF_LUT_ADDR_PORT;
		eotf_lut_data_port = S5_VD1_SLICE2_EOTF_LUT_DATA_PORT;
		ootf_lut_addr_port = S5_VD1_SLICE2_OGAIN_LUT_ADDR_PORT;
		ootf_lut_data_port = S5_VD1_SLICE2_OGAIN_LUT_DATA_PORT;
		oetf_lut_addr_port = S5_VD1_SLICE2_OETF_LUT_ADDR_PORT;
		oetf_lut_data_port = S5_VD1_SLICE2_OETF_LUT_DATA_PORT;
		cgain_lut_addr_port = S5_VD1_SLICE2_CGAIN_LUT_ADDR_PORT;
		cgain_lut_data_port = S5_VD1_SLICE2_CGAIN_LUT_DATA_PORT;
	} else if (offset == S5_VD1_SLICE3) {
		hdr_ctrl = S5_VD1_SLICE3_HDR2_CTRL;
		eotf_lut_addr_port = S5_VD1_SLICE3_EOTF_LUT_ADDR_PORT;
		eotf_lut_data_port = S5_VD1_SLICE3_EOTF_LUT_DATA_PORT;
		ootf_lut_addr_port = S5_VD1_SLICE3_OGAIN_LUT_ADDR_PORT;
		ootf_lut_data_port = S5_VD1_SLICE3_OGAIN_LUT_DATA_PORT;
		oetf_lut_addr_port = S5_VD1_SLICE3_OETF_LUT_ADDR_PORT;
		oetf_lut_data_port = S5_VD1_SLICE3_OETF_LUT_DATA_PORT;
		cgain_lut_addr_port = S5_VD1_SLICE3_CGAIN_LUT_ADDR_PORT;
		cgain_lut_data_port = S5_VD1_SLICE3_CGAIN_LUT_DATA_PORT;
	} else if (offset == VD2_HDR) {
		hdr_ctrl = S5_VD2_HDR2_CTRL;
		eotf_lut_addr_port = S5_VD2_EOTF_LUT_ADDR_PORT;
		eotf_lut_data_port = S5_VD2_EOTF_LUT_DATA_PORT;
		ootf_lut_addr_port = S5_VD2_OGAIN_LUT_ADDR_PORT;
		ootf_lut_data_port = S5_VD2_OGAIN_LUT_DATA_PORT;
		oetf_lut_addr_port = S5_VD2_OETF_LUT_ADDR_PORT;
		oetf_lut_data_port = S5_VD2_OETF_LUT_DATA_PORT;
		cgain_lut_addr_port = S5_VD2_CGAIN_LUT_ADDR_PORT;
		cgain_lut_data_port = S5_VD2_CGAIN_LUT_DATA_PORT;
	} else if (offset == OSD3_HDR) {
		hdr_ctrl = S5_OSD3_HDR2_CTRL;
		eotf_lut_addr_port = S5_OSD3_EOTF_LUT_ADDR_PORT;
		eotf_lut_data_port = S5_OSD3_EOTF_LUT_DATA_PORT;
		ootf_lut_addr_port = S5_OSD3_OGAIN_LUT_ADDR_PORT;
		ootf_lut_data_port = S5_OSD3_OGAIN_LUT_DATA_PORT;
		oetf_lut_addr_port = S5_OSD3_OETF_LUT_ADDR_PORT;
		oetf_lut_data_port = S5_OSD3_OETF_LUT_DATA_PORT;
		cgain_lut_addr_port = S5_OSD3_CGAIN_LUT_ADDR_PORT;
		cgain_lut_data_port = S5_OSD3_CGAIN_LUT_DATA_PORT;
	} else if (offset == VDIN1_HDR) {
		hdr_ctrl = VDIN_PP_HDR2_CTRL;
		eotf_lut_addr_port = VDIN_PP_EOTF_LUT_ADDR_PORT;
		eotf_lut_data_port = VDIN_PP_EOTF_LUT_DATA_PORT;
		ootf_lut_addr_port = VDIN_PP_OGAIN_LUT_ADDR_PORT;
		ootf_lut_data_port = VDIN_PP_OGAIN_LUT_DATA_PORT;
		oetf_lut_addr_port = VDIN_PP_OETF_LUT_ADDR_PORT;
		oetf_lut_data_port = VDIN_PP_OETF_LUT_DATA_PORT;
		cgain_lut_addr_port = VDIN_PP_CGAIN_LUT_ADDR_PORT;
		cgain_lut_data_port = VDIN_PP_CGAIN_LUT_DATA_PORT;
	} else {
		hdr_ctrl = S5_OSD1_HDR2_CTRL;
		eotf_lut_addr_port = S5_OSD1_EOTF_LUT_ADDR_PORT;
		eotf_lut_data_port = S5_OSD1_EOTF_LUT_DATA_PORT;
		ootf_lut_addr_port = S5_OSD1_OGAIN_LUT_ADDR_PORT;
		ootf_lut_data_port = S5_OSD1_OGAIN_LUT_DATA_PORT;
		oetf_lut_addr_port = S5_OSD1_OETF_LUT_ADDR_PORT;
		oetf_lut_data_port = S5_OSD1_OETF_LUT_DATA_PORT;
		cgain_lut_addr_port = S5_OSD1_CGAIN_LUT_ADDR_PORT;
		cgain_lut_data_port = S5_OSD1_CGAIN_LUT_DATA_PORT;
	}

	pr_info("-------s5 hdr config reg offset = %d hdr_ctrl = 0x%x-------\n",
		offset, hdr_ctrl);
	for (i = 0; i <= 0x40; i++) {
		reg = hdr_ctrl + i;
		/*skip lut*/
		if ((i >= 0x1e && i <= 0x23) ||
			i == 0x26 ||
			i == 0x27)
			continue;

		val = READ_VPP_REG(reg);
		pr_info("[0x%4x] = 0x%08x\n", reg, val);
	}

	pr_info("-------s5 hdr eotf reg = %04x-------\n", eotf_lut_addr_port);
	WRITE_VPP_REG_S5(eotf_lut_addr_port, 0x0);
	for (i = 0; i < HDR2_EOTF_LUT_SIZE; i++) {
		WRITE_VPP_REG_S5(eotf_lut_addr_port, i);
		val = READ_VPP_REG(eotf_lut_data_port);
		pr_info("[%04d] = 0x%08x\n", i, val);
	}

	pr_info("-------s5 hdr ootf reg = %04x-------\n", ootf_lut_addr_port);
	for (i = 0; i <= HDR2_OOTF_LUT_SIZE / 2; i++) {
		WRITE_VPP_REG_S5(ootf_lut_addr_port, i);
		val = READ_VPP_REG(ootf_lut_data_port);
		pr_info("[%04d] = 0x%08x\n", i, val);
	}

	pr_info("-------s5 hdr oetf reg = %04x-------\n", oetf_lut_addr_port);
	for (i = 0; i <= HDR2_OETF_LUT_SIZE / 2; i++) {
		WRITE_VPP_REG_S5(oetf_lut_addr_port, i);
		val = READ_VPP_REG(oetf_lut_data_port);
		pr_info("[%04d] = 0x%08x\n", i, val);
	}

	pr_info("-------s5 hdr cgain reg = %04x-------\n", cgain_lut_addr_port);
	for (i = 0; i <= HDR2_CGAIN_LUT_SIZE / 2; i++) {
		WRITE_VPP_REG_S5(cgain_lut_addr_port, i);
		val = READ_VPP_REG(cgain_lut_data_port);
		pr_info("[%04d] = 0x%08x\n", i, val);
	}
	pr_info("\nDMA_CTRL0[1a28] 0x%x\n", READ_VPP_REG_S5(VIU_DMA_CTRL0));
	pr_info("DMA_CTRL1[1a29] 0x%x\n", READ_VPP_REG_S5(VIU_DMA_CTRL1));
	pr_info("DMA_RDMIF7_CTRL [2757] 0x%x\n", READ_VPP_REG_S5(VPU_DMA_RDMIF7_CTRL));
	pr_info("DMA_RDMIF7_BADR0[2774] 0x%x\n", READ_VPP_REG_S5(VPU_DMA_RDMIF7_BADR0));
	pr_info("DMA_RDMIF7_BADR1[2775] 0x%x\n", READ_VPP_REG_S5(VPU_DMA_RDMIF7_BADR1));
	pr_info("DMA_RDMIF7_BADR2[2776] 0x%x\n", READ_VPP_REG_S5(VPU_DMA_RDMIF7_BADR2));
	pr_info("DMA_RDMIF7_BADR3[2777] 0x%x\n", READ_VPP_REG_S5(VPU_DMA_RDMIF7_BADR3));
}

void read_dma_buf(void)
{
	u32 *data32;
	u32 i;
	u32 j;
	u32 eotf_off;
	u32 oetf_off;
	u32 ootf_off;
	u32 cgain_off;
	u32 tail_off;
	u32 total_size = DMA_SIZE_SINGLE_HDR;

	eotf_off = 2 * 4;
	oetf_off = (2 + 24) * 4;
	ootf_off = (2 + 24 + 15) * 4;
	cgain_off = (2 + 24 + 15 + 19) * 4;
	tail_off = (2 + 24 + 15 + 19 + 7) * 4;
	data32 = (u32 *)(dma_vaddr);
	pr_info("\nshow %s %s %s %s %s %s %s %s dma buf\n",
			dma_lut_off_str[0], dma_lut_off_str[1],
			dma_lut_off_str[2], dma_lut_off_str[3],
			dma_lut_off_str[4], dma_lut_off_str[5],
			dma_lut_off_str[6], dma_lut_off_str[7]);

	for (j = 0; j < 8; j++) {
		data32 =  (u32 *)(dma_vaddr + DMA_SIZE_SINGLE_HDR * j);

		pr_info("\n----------dump %s dma_buf begin-----------%px\n",
				dma_lut_off_str[j], &data32[0]);
		for (i = 0; i < total_size / 4; i = i + 4)
			pr_info("%3d: %8x, %8x, %8x, %8x\n", i,
			data32[i + 3], data32[i + 2], data32[i + 1], data32[i + 0]);

		pr_info("\n==>header: %8x %8x %8x %8x\n",
			data32[3], data32[2], data32[1], data32[0]);
		pr_info("           %8x %8x %8x %8x\n",
			data32[7], data32[6], data32[5], data32[4]);

		pr_info("\n==>eotf:\n");
		for (i = 0; i < 24; i++)
			pr_info("%5x %5x %5x %5x %5x %5x\n",
					data32[eotf_off + 4 * i] & 0xfffff,
					(data32[eotf_off + 4 * i + 1] & 0xff) << 12 |
					(data32[eotf_off + 4 * i] & 0xfff00000) >> 20,
					(data32[eotf_off + 4 * i + 1] & 0x0fffff00) >> 8,
					(data32[eotf_off + 4 * i + 1] & 0xf0000000) >> 28 |
					(data32[eotf_off + 4 * i + 2] & 0x0000ffff) << 4,
					(data32[eotf_off + 4 * i + 3] & 0xf) << 16 |
					(data32[eotf_off + 4 * i + 2] & 0xffff0000) >> 16,
					(data32[eotf_off + 4 * i + 3] & 0xfffff0) >> 4);

		pr_info("\n==>oetf:\n");
		for (i = 0; i < 15; i++)
			pr_info("%3x %3x %3x %3x %3x %3x %3x %3x %3x %3x\n",
					data32[oetf_off + 4 * i] & 0xfff,
					(data32[oetf_off + 4 * i] & 0xfff000) >> 12,
					(data32[oetf_off + 4 * i + 1] & 0xf) << 8 |
					(data32[oetf_off + 4 * i] & 0xff000000) >> 24,
					(data32[oetf_off + 4 * i + 1] & 0xfff0) >> 4,
					(data32[oetf_off + 4 * i + 1] & 0xfff0000) >> 16,
					(data32[oetf_off + 4 * i + 1] & 0xf0000000) >> 28 |
					(data32[oetf_off + 4 * i + 2] & 0xff) << 4,
					(data32[oetf_off + 4 * i + 2] & 0xfff00) >> 8,
					(data32[oetf_off + 4 * i + 2] & 0xfff00000) >> 20,
					(data32[oetf_off + 4 * i + 3] & 0xfff),
					(data32[oetf_off + 4 * i + 3] & 0xfff000) >> 12);

		pr_info("\n==>ootf:\n");
		for (i = 0; i < 19; i++)
			pr_info("%4x %4x %4x %4x %4x %4x %4x %4x\n",
					data32[ootf_off + 4 * i] & 0xffff,
					(data32[ootf_off + 4 * i] & 0xffff0000) >> 16,
					data32[ootf_off + 4 * i + 1] & 0xffff,
					(data32[ootf_off + 4 * i + 1] & 0xffff0000) >> 16,
					data32[ootf_off + 4 * i + 2] & 0xffff,
					(data32[ootf_off + 4 * i + 2] & 0xffff0000) >> 16,
					data32[ootf_off + 4 * i + 3] & 0xffff,
					(data32[ootf_off + 4 * i + 3] & 0xffff0000) >> 16);

		pr_info("\n==>cgain:\n");
		for (i = 0; i < 7; i++)
			pr_info("%3x %3x %3x %3x %3x %3x %3x %3x %3x %3x\n",
					data32[cgain_off + 4 * i] & 0xfff,
					(data32[cgain_off + 4 * i] & 0xfff000) >> 12,
					(data32[cgain_off + 4 * i + 1] & 0xf) << 8 |
					(data32[cgain_off + 4 * i] & 0xff000000) >> 24,
					(data32[cgain_off + 4 * i + 1] & 0xfff0) >> 4,
					(data32[cgain_off + 4 * i + 1] & 0xfff0000) >> 16,
					(data32[cgain_off + 4 * i + 1] & 0xf0000000) >> 28 |
					(data32[cgain_off + 4 * i + 2] & 0xff) << 4,
					(data32[cgain_off + 4 * i + 2] & 0xfff00) >> 8,
					(data32[cgain_off + 4 * i + 2] & 0xfff00000) >> 20,
					(data32[cgain_off + 4 * i + 3] & 0xfff),
					(data32[cgain_off + 4 * i + 3] & 0xfff000) >> 12);

		pr_info("\n==>tail: %8x %8x %8x %8x\n",
				data32[tail_off + 3], data32[tail_off + 2],
				data32[tail_off + 1], data32[tail_off]);

		pr_info("---------dump %s dma_buf end-----------\n", dma_lut_off_str[j]);
	}
	pr_info("\nDMA_CTRL0[1a28] 0x%x\n", READ_VPP_REG_S5(VIU_DMA_CTRL0));
	pr_info("DMA_CTRL1[1a29] 0x%x\n", READ_VPP_REG_S5(VIU_DMA_CTRL1));
	pr_info("DMA_RDMIF7_CTRL [2757] 0x%x\n", READ_VPP_REG_S5(VPU_DMA_RDMIF7_CTRL));
	pr_info("DMA_RDMIF7_BADR0[2774] 0x%x\n", READ_VPP_REG_S5(VPU_DMA_RDMIF7_BADR0));
	pr_info("DMA_RDMIF7_BADR1[2775] 0x%x\n", READ_VPP_REG_S5(VPU_DMA_RDMIF7_BADR1));
	pr_info("DMA_RDMIF7_BADR2[2776] 0x%x\n", READ_VPP_REG_S5(VPU_DMA_RDMIF7_BADR2));
	pr_info("DMA_RDMIF7_BADR3[2777] 0x%x\n", READ_VPP_REG_S5(VPU_DMA_RDMIF7_BADR3));
}

void write_dma_buf(u32 table_offset, u32 tbl_id, u32 value)
{
	u32 *data32;
	u32 data_offset = 0;

	if (table_offset < DMA_HDR_COUNT) {
		/*each hdr has 68*4 count*/
		data_offset = table_offset * DMA_COUNT_SINGLE_HDR * 4;
	}

	if (!dma_vaddr || tbl_id > DMA_COUNT_SINGLE_HDR * 4) {
		pr_info("No dma table %px to write or id %d overflow\n",
			dma_vaddr, tbl_id);
		return;
	}
	data32 = (u32 *)dma_vaddr;
	pr_info("dma_vaddr:%px, %px, modify table[%d]=0x%x -> 0x%x\n",
			dma_vaddr, &data32[data_offset + tbl_id],
			data_offset + tbl_id, data32[data_offset + tbl_id], value);

	data32[data_offset + tbl_id] = value;
}

