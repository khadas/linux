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
			    const struct isp2x_dpcc_cfg *arg);
	void (*dpcc_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*bls_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp32_bls_cfg *arg);
	void (*bls_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*sdg_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp2x_sdg_cfg *arg);
	void (*sdg_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*lsc_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp3x_lsc_cfg *arg);
	void (*lsc_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*awbgain_config)(struct rkisp_isp_params_vdev *params_vdev,
			       const struct isp32_awb_gain_cfg *arg);
	void (*awbgain_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*debayer_config)(struct rkisp_isp_params_vdev *params_vdev,
			       const struct isp39_debayer_cfg *arg);
	void (*debayer_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*ccm_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp39_ccm_cfg *arg);
	void (*ccm_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*goc_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp3x_gammaout_cfg *arg);
	void (*goc_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*cproc_config)(struct rkisp_isp_params_vdev *params_vdev,
			     const struct isp2x_cproc_cfg *arg);
	void (*cproc_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*rawaf_config)(struct rkisp_isp_params_vdev *params_vdev,
			     const struct isp39_rawaf_meas_cfg *arg);
	void (*rawaf_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*rawae0_config)(struct rkisp_isp_params_vdev *params_vdev,
			      const struct isp2x_rawaebig_meas_cfg *arg);
	void (*rawae0_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*rawae3_config)(struct rkisp_isp_params_vdev *params_vdev,
			      const struct isp2x_rawaebig_meas_cfg *arg);
	void (*rawae3_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*rawawb_config)(struct rkisp_isp_params_vdev *params_vdev,
			      const struct isp39_rawawb_meas_cfg *arg);
	void (*rawawb_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*rawhst0_config)(struct rkisp_isp_params_vdev *params_vdev,
			       const struct isp2x_rawhistbig_cfg *arg);
	void (*rawhst0_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*rawhst3_config)(struct rkisp_isp_params_vdev *params_vdev,
			       const struct isp2x_rawhistbig_cfg *arg);
	void (*rawhst3_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*hdrdrc_config)(struct rkisp_isp_params_vdev *params_vdev,
			      const struct isp39_drc_cfg *arg,
			      enum rkisp_params_type type);
	void (*hdrdrc_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*hdrmge_config)(struct rkisp_isp_params_vdev *params_vdev,
			      const struct isp32_hdrmge_cfg *arg,
			      enum rkisp_params_type type);
	void (*hdrmge_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*gic_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp21_gic_cfg *arg);
	void (*gic_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*dhaz_config)(struct rkisp_isp_params_vdev *params_vdev,
			    const struct isp39_dhaz_cfg *arg);
	void (*dhaz_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*isp3dlut_config)(struct rkisp_isp_params_vdev *params_vdev,
				const struct isp2x_3dlut_cfg *arg);
	void (*isp3dlut_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*ldch_config)(struct rkisp_isp_params_vdev *params_vdev,
			    const struct isp39_ldch_cfg *arg);
	void (*ldch_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*ynr_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp39_ynr_cfg *arg);
	void (*ynr_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*cnr_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp39_cnr_cfg *arg);
	void (*cnr_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*sharp_config)(struct rkisp_isp_params_vdev *params_vdev,
			     const struct isp39_sharp_cfg *arg);
	void (*sharp_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*bay3d_config)(struct rkisp_isp_params_vdev *params_vdev,
			     const struct isp39_bay3d_cfg *arg);
	void (*bay3d_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*gain_config)(struct rkisp_isp_params_vdev *params_vdev,
			    const struct isp3x_gain_cfg *arg);
	void (*gain_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*cac_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp32_cac_cfg *arg);
	void (*cac_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*csm_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp21_csm_cfg *arg);
	void (*cgc_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp21_cgc_cfg *arg);
	void (*ie_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*yuvme_config)(struct rkisp_isp_params_vdev *params_vdev,
			     const struct isp39_yuvme_cfg *arg);
	void (*yuvme_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*ldcv_config)(struct rkisp_isp_params_vdev *params_vdev,
			    const struct isp39_ldcv_cfg *arg);
	void (*ldcv_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
	void (*rgbir_config)(struct rkisp_isp_params_vdev *params_vdev,
			     const struct isp39_rgbir_cfg *arg);
	void (*rgbir_enable)(struct rkisp_isp_params_vdev *params_vdev, bool en);
};

struct rkisp_isp_params_val_v39 {
	struct tasklet_struct lsc_tasklet;

	struct rkisp_dummy_buffer buf_3dlut[ISP39_3DLUT_BUF_NUM];
	u32 buf_3dlut_idx;

	struct rkisp_dummy_buffer buf_ldch[ISP39_MESH_BUF_NUM];
	u32 buf_ldch_idx;
	u32 ldch_out_hsize;

	struct rkisp_dummy_buffer buf_ldcv[ISP39_MESH_BUF_NUM];
	u32 buf_ldcv_idx;
	u32 ldcv_out_vsize;

	struct rkisp_dummy_buffer buf_cac[ISP39_MESH_BUF_NUM];
	u32 buf_cac_idx;

	struct rkisp_dummy_buffer buf_info[RKISP_INFO2DDR_BUF_MAX];
	u32 buf_info_owner;
	u32 buf_info_cnt;
	int buf_info_idx;

	struct rkisp_dummy_buffer buf_gain;
	struct rkisp_dummy_buffer buf_3dnr_iir;
	struct rkisp_dummy_buffer buf_3dnr_cur;

	struct rkisp_dummy_buffer buf_frm;

	struct isp32_hdrmge_cfg last_hdrmge;
	struct isp32_hdrmge_cfg cur_hdrmge;
	struct isp39_drc_cfg last_hdrdrc;
	struct isp39_drc_cfg cur_hdrdrc;

	u32 dhaz_blk_num;

	bool dhaz_en;
	bool drc_en;
	bool lsc_en;
	bool mge_en;
	bool lut3d_en;
	bool bay3d_en;
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
