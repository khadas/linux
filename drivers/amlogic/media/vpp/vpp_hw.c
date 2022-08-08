// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Standard Linux headers */
#include "vpp_reg_io.h"
#include "vpp_hw.h"
#include "vpp_common.h"
#include "vpp_drv.h"

/* vpp pipeline for all features:
 * SR0-PPS-SR1(SHARPNESS1 DNLP LC)-CHR CORING-BLE-CM-BLUE STR-VADJ1-
 * (FRC-POST BLEND) - PST2 MTX-VDJ2-3DLUT-PREGM-WB-OUTCLIP-GAMMA
 */

void hist_cfg(struct cfg_data_s *cfg_data)
{
	enum chip_id_e chip_id;
	struct vpp_dev_s devp = get_vpp_dev();

	chip_id = devp->pm_data->chip_id;

	if (chip_id == CHIP_T5D)
		WRITE_VPP_REG_BITS(VI_HIST_CTRL, 0x2, 11, 3);
	else
		WRITE_VPP_REG_BITS(VI_HIST_CTRL, 0x1, 11, 3);
	WRITE_VPP_REG_BITS(VI_HIST_CTRL, 0x1, 0, 1);
	WRITE_VPP_REG(VI_HIST_GCLK_CTRL, 0xffffffff);
	WRITE_VPP_REG_BITS(VI_HIST_CTRL, 2, VI_HIST_POW_BIT, VI_HIST_POW_WID);
}

/*sr0:sharpness/cti/lti/dejaggy/lpf etc*/
int sr0_cfg(struct cfg_data_s *cfg_data)
{
}

/*sr0 dnlp curve update*/
int sr0_dnlp_update(struct cfg_data_s *cfg_data)
{
	enum reg_cfg_mode reg_mod;
	enum cfg_mode cfg_mod;
	int i;
	uint *tbl;

	reg_mod = cfg_data->reg_mod;
	tbl = (uint)cfg_data->data_p;

	case (reg_mod) {
	case CFG_DIR:
		for (i = 0; i < 32; i++)
			WRITE_VPP_REG(SRSHARP0_DNLP_00, tbl[i]);
		break;
	case CFG_RDMA:
		for (i = 0; i < 32; i++)
			VSYNC_WR_MPEG_REG(SRSHARP0_DNLP_00, tbl[i]);
		break;
	default:
		break;
	}

	pr_vpp(PR_DEBUG, "reg_mod = %d, cfg_mod = %d\n", reg_mod, cfg_mod);
}

/*sr1: dnlp curve update*/
int sr1_dnlp_update(struct cfg_data_s *cfg_data)
{
	enum reg_cfg_mode reg_mod;
	enum cfg_mode cfg_mod;
	int i;
	uint *tbl;

	reg_mod = cfg_data->reg_mod;
	tbl = (uint)cfg_data->data_p;

	case (reg_mod) {
	case CFG_DIR:
		for (i = 0; i < 32; i++)
			WRITE_VPP_REG(SRSHARP1_DNLP_00, tbl[i]);
		break;
	case CFG_RDMA:
		for (i = 0; i < 32; i++)
			VSYNC_WR_MPEG_REG(SRSHARP1_DNLP_00, tbl[i]);
		break;
	default:
		break;
	}

	pr_vpp(PR_DEBUG, "reg_mod = %d, cfg_mod = %d\n", reg_mod, cfg_mod);
}

/*sr1: lc curve update*/
int sr1_lc_cfg(struct cfg_data_s *cfg_data)
{
}

/*chroma coring*/
int cc_cfg(struct cfg_data_s *cfg_data)
{
}

/*black extension*/
int ble_cfg(struct cfg_data_s *cfg_data)
{
}

/*color management, init bypass curve*/
void cm_cfg(void)
{
	enum bit_dp_e bitdepth;
	struct vpp_dev_s devp = get_vpp_dev();
	int i, j;

	bitdepth = devp->vpp_cfg_data.bit_depth;

	WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, SAT_BYYB_NODE0);
	WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x0);
	WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, SAT_BYYB_NODE1);
	WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x0);
	WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, SAT_BYYB_NODE2);
	WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x0);
	WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, SAT_SRC_NODE);
	WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x8000400);
	WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, CM_ENH_SFT_MODE);
	WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x0);
	WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, FITLER_CFG);
	WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x0);
	WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, CM_GLOBAL_GAIN);
	WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x2000000);
	WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, CM_ENH_CTL);
	WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x1d);
	WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, ROI_X_SCOPE);
	WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x0);
	WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, ROI_Y_SCOPE);
	WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x0);
	WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, IFO_MODE);
	WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x0);

	switch (bitdepth) {
	case BDP_12:
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, XVYCC_YSCP_REG);
		WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0xfff0000);
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, XVYCC_USCP_REG);
		WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0xfff0000);
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, XVYCC_VSCP_REG);
		WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0xfff0000);
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, LUMA_ADJ0_REG);
		WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x100400);
		break;
	case BDP_10:
	default:
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, XVYCC_YSCP_REG);
		WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x3ff0000);
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, XVYCC_USCP_REG);
		WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x3ff0000);
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, XVYCC_VSCP_REG);
		WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x3ff0000);
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, LUMA_ADJ0_REG);
		WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x40400);
		break;
	}

	for (i = 0; i < 32; i++) {
		for (j = 0; j < 5; j++) {
			reg = CM2_ENH_COEF0_H00 + i * 8 + j;
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, reg);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x0);
		}
	}
}

void mod_ctl(enum vpp_module_e mod,
	enum reg_cfg_mode cfg_mod,
	enum mod_ctl_e en)
{
	u32 tmp, tmp2;

	switch (mod) {
	case MOD_SR0:
		break;
	case MOD_SR0_DNLP:
		if (cfg_mod == CFG_DIR)
			WRITE_VPP_REG_BITS(SRSHARP0_DNLP_EN, en, 0, 1);
		else if (cfg_mod == CFG_RDMA)
			VSYNC_WR_MPEG_REG_BITS(SRSHARP0_DNLP_EN, en, 0, 1)
		break;
	case MOD_SR1:
		break;
	case MOD_SR1_DNLP:
		if (cfg_mod == CFG_DIR)
			WRITE_VPP_REG_BITS(SRSHARP1_DNLP_EN, en, 0, 1);
		else if (cfg_mod == CFG_RDMA)
			VSYNC_WR_MPEG_REG_BITS(SRSHARP1_DNLP_EN, en, 0, 1)
		break;
	case MOD_LC:
		if (cfg_mod == CFG_DIR) {
			WRITE_VPP_REG_BITS(LC_STTS_HIST_REGION_IDX,
				en, 31, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_LC_TOP_CTRL, en, 4, 1);
			WRITE_VPP_REG_BITS(LC_CURVE_CTRL, en, 0, 1);
			WRITE_VPP_REG_BITS(LC_CURVE_RAM_CTRL, en, 0, 1);
		} else if (cfg_mod == CFG_RDMA) {
			VSYNC_WR_MPEG_REG_BITS(LC_STTS_HIST_REGION_IDX,
				en, 31, 1);
			VSYNC_WR_MPEG_REG_BITS(SRSHARP1_LC_TOP_CTRL, en, 4, 1);
			VSYNC_WR_MPEG_REG_BITS(LC_CURVE_CTRL, en, 0, 1);
			VSYNC_WR_MPEG_REG_BITS(LC_CURVE_RAM_CTRL, en, 0, 1);
		}
		break;
	case MOD_CM:
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, 0x208);
		tmp = READ_VPP_REG(VPP_CHROMA_DATA_PORT);
		if (en)
			tmp |= en;
		else
			tmp &= ~en;
		if (cfg_mod == CFG_DIR) {
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, 0x208);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, tmp);
		} else if (cfg_mod == CFG_RDMA) {
			VSYNC_WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, 0x208);
			VSYNC_WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, tmp);
		}
		break;
	case MOD_BLE:
		break;
	case MOD_BLS:
		break;
	case MOD_CCORING:
		break;
	case MOD_VADJ1:
		break;
	case MOD_WB:
		break;
	case MOD_VADJ2:
		break;
	case MOD_LUT3D:
		tmp = READ_VPP_REG(VPP_LUT3D_CTRL);
		tmp = (tmp & 0xFFFFFF8E) |
			(en & 0x1) |
			(0x7 << 4);
		if (cfg_mod == CFG_DIR) {
			WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 0);
			WRITE_VPP_REG(VPP_LUT3D_CTRL, tmp);
		} else if (cfg_mod == CFG_RDMA) {
			VSYNC_WR_MPEG_REG(VPP_LUT3D_CBUS2RAM_CTRL, 0);
			VSYNC_WR_MPEG_REG(VPP_LUT3D_CTRL, tmp);
		}
		break;
	case MOD_PREGAMMA:
		break;
	case MOD_GAMMA:
		break;
	default:
		break;
	}
}

/*blue stretch*/
int blue_str_cfg(struct cfg_data_s *cfg_data)
{
}

/*vdj1: brightness/contrast/saturation/hue*/
int vd_adj_cfg(struct cfg_data_s *cfg_data)
{
}

/*3dlut*/
int lut3d_cfg(void)
{
}

/*3dlut*/
int lut3d_set(void)
{
}

/*pre gamma*/
int pregm_set(struct cfg_data_s *cfg_data)
{
}

/*white balance*/
int wb_set(struct cfg_data_s *cfg_data)
{
}

int outclip_set(struct cfg_data_s *cfg_data)
{
}

/*gamma*/
int gm_set(struct cfg_data_s *cfg_data)
{
}

/*matrix*/
int mtx_cfg(struct cfg_data_s *cfg_data)
{
}

static const struct hw_ops_s hw_ops = {
	.hist_cfg = hist_cfg,
	.sr0_cfg = sr0_cfg,
	.sr0_dnlp_cfg = sr0_dnlp_cfg,
	.sr0_dnlp_curve = sr0_dnlp_curve,
	.sr1_cfg = sr1_cfg,
	.sr1_dnlp_cfg = sr1_dnlp_cfg,
	.sr1_dnlp_curve = sr1_dnlp_curve,
	.sr1_lc_cfg = sr1_lc_cfg,
	.cc_cfg = cc_cfg,
	.ble_cfg = ble_cfg,
	.cm_cfg = cm_cfg,
	.blue_str_cfg = blue_str_cfg,
	.vd_adj_cfg = vd_adj_cfg,
	.lut3d_cfg = lut3d_cfg,
	.pregm_set = pregm_set,
	.wb_set = wb_set,
	.outclip_set = outclip_set,
	.gm_set = gm_set,
	.mtx_cfg = mtx_cfg
};

void hw_ops_attach(const struct hw_ops_s *ops)
{
	ops = &hw_ops;
	pr_drv("vpp hw ops init");
}
EXPORT_SYMBOL(hw_ops_attach);

