/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef AM_S5_HDR2_V0
#define AM_S5_HDR2_V0
//#include <linux/types.h>
//#include <linux/amlogic/media/vout/vinfo.h>
//#include <linux/amlogic/media/amvecm/hdr2_ext.h>
//#include "hdr/am_hdr10_plus_ootf.h"
//#include "amcsc.h"
#include "set_hdr2_v0.h"

void s5_set_hdr_matrix(enum hdr_module_sel module_sel,
		    enum hdr_matrix_sel mtx_sel,
		    struct hdr_proc_mtx_param_s *hdr_mtx_param,
		    struct hdr10pgen_param_s *p_hdr10pgen_param,
		    struct hdr_proc_lut_param_s *hdr_lut_param,
		    enum vpp_index_e vpp_index);
void s5_set_eotf_lut(enum hdr_module_sel module_sel,
		  struct hdr_proc_lut_param_s *hdr_lut_param,
		  enum vpp_index_e vpp_index);
void s5_set_ootf_lut(enum hdr_module_sel module_sel,
		  struct hdr_proc_lut_param_s *hdr_lut_param,
		  enum vpp_index_e vpp_index);
void s5_set_oetf_lut(enum hdr_module_sel module_sel,
		  struct hdr_proc_lut_param_s *hdr_lut_param,
		  enum vpp_index_e vpp_index);
void s5_set_c_gain(enum hdr_module_sel module_sel,
		struct hdr_proc_lut_param_s *hdr_lut_param,
		enum vpp_index_e vpp_index);
void s5_hdr_hist_config(enum hdr_module_sel module_sel,
		     struct hdr_proc_lut_param_s *hdr_lut_param,
		     enum vpp_index_e vpp_index);
void set_vpu_lut_dma_mif(struct VPU_LUT_DMA_t      *vpu_lut_dma);
void vpu_lut_dma(enum hdr_module_sel module_sel,
	struct hdr_proc_lut_param_s *hdr_lut_param, enum LUT_DMA_ID_e dma_id);
void s5_hdr_reg_dump(unsigned int offset);
void hdr_lut_buffer_malloc(struct platform_device *pdev);
void hdr_lut_buffer_free(struct platform_device *pdev);
void disable_ai_color(void);
void read_dma_buf(void);
void write_dma_buf(u32 table_offset, u32 tbl_id, u32 value);
#endif
