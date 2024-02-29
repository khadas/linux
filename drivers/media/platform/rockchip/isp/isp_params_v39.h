/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#ifndef _RKISP_PARAM_V39_H
#define _RKISP_PARAM_V39_H

#include "common.h"
#include "isp_params.h"

#define ISP39_3DLUT_BUF_NUM			2
#define ISP39_3DLUT_BUF_SIZE			(9 * 9 * 9 * 4)

#define ISP39_LSC_LUT_BUF_NUM			2
#define ISP39_LSC_LUT_TBL_SIZE			(9 * 17 * 4)
#define ISP39_LSC_LUT_BUF_SIZE			(ISP39_LSC_LUT_TBL_SIZE * 4)

#define ISP39_RAWHISTBIG_ROW_NUM		15
#define ISP39_RAWHISTBIG_COLUMN_NUM		15
#define ISP39_RAWHISTBIG_WEIGHT_REG_SIZE	\
	(ISP39_RAWHISTBIG_ROW_NUM * ISP39_RAWHISTBIG_COLUMN_NUM)

struct rkisp_isp_params_vdev;
struct rkisp_isp_params_ops_v39 {
	void (*dpcc_config)(struct rkisp_isp_params_vdev *params_vdev,
			    const struct isp2x_dpcc_cfg *arg, u32 id);
	void (*dpcc_enable)(struct rkisp_isp_params_vdev *params_vdev,
			    bool en, u32 id);
	void (*bls_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp32_bls_cfg *arg, u32 id);
	void (*bls_enable)(struct rkisp_isp_params_vdev *params_vdev,
			   bool en, u32 id);
	void (*sdg_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp2x_sdg_cfg *arg, u32 id);
	void (*sdg_enable)(struct rkisp_isp_params_vdev *params_vdev,
			   bool en, u32 id);
	void (*lsc_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp3x_lsc_cfg *arg, u32 id);
	void (*lsc_enable)(struct rkisp_isp_params_vdev *params_vdev,
			   bool en, u32 id);
	void (*awbgain_config)(struct rkisp_isp_params_vdev *params_vdev,
			       const struct isp32_awb_gain_cfg *arg, u32 id);
	void (*awbgain_enable)(struct rkisp_isp_params_vdev *params_vdev,
			       bool en, u32 id);
	void (*debayer_config)(struct rkisp_isp_params_vdev *params_vdev,
			       const struct isp39_debayer_cfg *arg, u32 id);
	void (*debayer_enable)(struct rkisp_isp_params_vdev *params_vdev,
			       bool en, u32 id);
	void (*ccm_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp39_ccm_cfg *arg,  u32 id);
	void (*ccm_enable)(struct rkisp_isp_params_vdev *params_vdev,
			   bool en, u32 id);
	void (*goc_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp3x_gammaout_cfg *arg,  u32 id);
	void (*goc_enable)(struct rkisp_isp_params_vdev *params_vdev,
			   bool en, u32 id);
	void (*cproc_config)(struct rkisp_isp_params_vdev *params_vdev,
			     const struct isp2x_cproc_cfg *arg, u32 id);
	void (*cproc_enable)(struct rkisp_isp_params_vdev *params_vdev,
			     bool en, u32 id);
	void (*rawaf_config)(struct rkisp_isp_params_vdev *params_vdev,
			     const struct isp39_rawaf_meas_cfg *arg, u32 id);
	void (*rawaf_enable)(struct rkisp_isp_params_vdev *params_vdev,
			     bool en, u32 id);
	void (*rawae0_config)(struct rkisp_isp_params_vdev *params_vdev,
			      const struct isp2x_rawaebig_meas_cfg *arg, u32 id);
	void (*rawae0_enable)(struct rkisp_isp_params_vdev *params_vdev,
			      bool en, u32 id);
	void (*rawae3_config)(struct rkisp_isp_params_vdev *params_vdev,
			      const struct isp2x_rawaebig_meas_cfg *arg, u32 id);
	void (*rawae3_enable)(struct rkisp_isp_params_vdev *params_vdev,
			      bool en, u32 id);
	void (*rawawb_config)(struct rkisp_isp_params_vdev *params_vdev,
			      const struct isp39_rawawb_meas_cfg *arg, u32 id);
	void (*rawawb_enable)(struct rkisp_isp_params_vdev *params_vdev,
			      bool en, u32 id);
	void (*rawhst0_config)(struct rkisp_isp_params_vdev *params_vdev,
			       const struct isp2x_rawhistbig_cfg *arg, u32 id);
	void (*rawhst0_enable)(struct rkisp_isp_params_vdev *params_vdev,
			       bool en, u32 id);
	void (*rawhst3_config)(struct rkisp_isp_params_vdev *params_vdev,
			       const struct isp2x_rawhistbig_cfg *arg, u32 id);
	void (*rawhst3_enable)(struct rkisp_isp_params_vdev *params_vdev,
			       bool en, u32 id);
	void (*hdrdrc_config)(struct rkisp_isp_params_vdev *params_vdev,
			      const struct isp39_drc_cfg *arg,
			      enum rkisp_params_type type, u32 id);
	void (*hdrdrc_enable)(struct rkisp_isp_params_vdev *params_vdev,
			      bool en, u32 id);
	void (*hdrmge_config)(struct rkisp_isp_params_vdev *params_vdev,
			      const struct isp32_hdrmge_cfg *arg,
			      enum rkisp_params_type type, u32 id);
	void (*hdrmge_enable)(struct rkisp_isp_params_vdev *params_vdev,
			      bool en, u32 id);
	void (*gic_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp21_gic_cfg *arg, u32 id);
	void (*gic_enable)(struct rkisp_isp_params_vdev *params_vdev,
			   bool en, u32 id);
	void (*dhaz_config)(struct rkisp_isp_params_vdev *params_vdev,
			    const struct isp39_dhaz_cfg *arg, u32 id);
	void (*dhaz_enable)(struct rkisp_isp_params_vdev *params_vdev,
			    bool en, u32 id);
	void (*isp3dlut_config)(struct rkisp_isp_params_vdev *params_vdev,
				const struct isp2x_3dlut_cfg *arg, u32 id);
	void (*isp3dlut_enable)(struct rkisp_isp_params_vdev *params_vdev,
				bool en, u32 id);
	void (*ldch_config)(struct rkisp_isp_params_vdev *params_vdev,
			    const struct isp39_ldch_cfg *arg, u32 id);
	void (*ldch_enable)(struct rkisp_isp_params_vdev *params_vdev,
			    bool en, u32 id);
	void (*ynr_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp39_ynr_cfg *arg, u32 id);
	void (*ynr_enable)(struct rkisp_isp_params_vdev *params_vdev,
			   bool en, u32 id);
	void (*cnr_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp39_cnr_cfg *arg, u32 id);
	void (*cnr_enable)(struct rkisp_isp_params_vdev *params_vdev,
			   bool en, u32 id);
	void (*sharp_config)(struct rkisp_isp_params_vdev *params_vdev,
			     const struct isp39_sharp_cfg *arg, u32 id);
	void (*sharp_enable)(struct rkisp_isp_params_vdev *params_vdev,
			     bool en, u32 id);
	void (*bay3d_config)(struct rkisp_isp_params_vdev *params_vdev,
			     const struct isp39_bay3d_cfg *arg, u32 id);
	void (*bay3d_enable)(struct rkisp_isp_params_vdev *params_vdev,
			     bool en, u32 id);
	void (*gain_config)(struct rkisp_isp_params_vdev *params_vdev,
			    const struct isp3x_gain_cfg *arg, u32 id);
	void (*gain_enable)(struct rkisp_isp_params_vdev *params_vdev,
			    bool en, u32 id);
	void (*cac_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp32_cac_cfg *arg, u32 id);
	void (*cac_enable)(struct rkisp_isp_params_vdev *params_vdev,
			   bool en, u32 id);
	void (*csm_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp21_csm_cfg *arg, u32 id);
	void (*cgc_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp21_cgc_cfg *arg, u32 id);
	void (*ie_enable)(struct rkisp_isp_params_vdev *params_vdev,
			  bool en, u32 id);
	void (*yuvme_config)(struct rkisp_isp_params_vdev *params_vdev,
			     const struct isp39_yuvme_cfg *arg, u32 id);
	void (*yuvme_enable)(struct rkisp_isp_params_vdev *params_vdev,
			     bool en, u32 id);
	void (*ldcv_config)(struct rkisp_isp_params_vdev *params_vdev,
			    const struct isp39_ldcv_cfg *arg, u32 id);
	void (*ldcv_enable)(struct rkisp_isp_params_vdev *params_vdev,
			    bool en, u32 id);
	void (*rgbir_config)(struct rkisp_isp_params_vdev *params_vdev,
			     const struct isp39_rgbir_cfg *arg, u32 id);
	void (*rgbir_enable)(struct rkisp_isp_params_vdev *params_vdev,
			     bool en, u32 id);
};

struct rkisp_isp_params_val_v39 {
	struct rkisp_dummy_buffer buf_3dlut[ISP_UNITE_MAX][ISP39_3DLUT_BUF_NUM];
	u32 buf_3dlut_idx[ISP_UNITE_MAX];

	struct rkisp_dummy_buffer buf_ldch[ISP_UNITE_MAX][ISP39_MESH_BUF_NUM];
	u32 buf_ldch_idx[ISP_UNITE_MAX];
	u32 ldch_out_hsize;

	struct rkisp_dummy_buffer buf_ldcv[ISP_UNITE_MAX][ISP39_MESH_BUF_NUM];
	u32 buf_ldcv_idx[ISP_UNITE_MAX];
	u32 ldcv_out_vsize;

	struct rkisp_dummy_buffer buf_cac[ISP_UNITE_MAX][ISP39_MESH_BUF_NUM];
	u32 buf_cac_idx[ISP_UNITE_MAX];

	struct rkisp_dummy_buffer buf_lsclut[ISP39_LSC_LUT_BUF_NUM];
	u32 buf_lsclut_idx;

	struct rkisp_dummy_buffer buf_info[RKISP_INFO2DDR_BUF_MAX];
	u32 buf_info_owner;
	u32 buf_info_cnt;
	int buf_info_idx;

	u32 gain_size;
	u32 bay3d_iir_size;
	u32 bay3d_cur_size;
	struct rkisp_dummy_buffer buf_gain;
	struct rkisp_dummy_buffer buf_3dnr_iir;
	struct rkisp_dummy_buffer buf_3dnr_cur;

	struct rkisp_dummy_buffer buf_frm;

	u32 dhaz_blk_num;

	bool is_bigmode;
};

#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V39)
int rkisp_init_params_vdev_v39(struct rkisp_isp_params_vdev *params_vdev);
void rkisp_uninit_params_vdev_v39(struct rkisp_isp_params_vdev *params_vdev);
#else
static inline int rkisp_init_params_vdev_v39(struct rkisp_isp_params_vdev *params_vdev)
{
	return -EINVAL;
}
static inline void rkisp_uninit_params_vdev_v39(struct rkisp_isp_params_vdev *params_vdev) {}
#endif

#endif /* _RKISP_PARAM_V39_H */
