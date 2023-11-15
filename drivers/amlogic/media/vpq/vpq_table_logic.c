// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vpp/vpp_drv.h>
#include <linux/amlogic/media/vpp/vpp_common_def.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include "vpq_table_logic.h"
#include "vpq_printk.h"
#include "vpq_vfm.h"
#include "modules/vpq_module_cm.h"
#include "modules/vpq_module_timing.h"
#include "modules/vpq_module_gamma.h"
#include "modules/vpq_module_dump.h"
#include "../vpp/vpp_common.h"
#include "../vpp/vpp_pq_mgr.h"

//picture mode
struct vpq_pict_mode_item_data_s pic_mode_item;

//currnt pq table index
static unsigned char pre_pq_table_index[PQ_INDEX_MAX];

//gamma & wb
struct vpq_rgb_ogo_s rgb_ogo;
struct vpp_gamma_table_s vpp_gma_table;

//dnlp
bool dnlp_enable;
struct vpp_dnlp_curve_param_s *pdnlp_data;

//local contrast
bool lc_enable;
struct vpp_lc_curve_s lc_curve;
struct vpp_lc_param_s lc_param;

//black ext & blue stretch
bool blkext_en;
int *pble_data;
bool blue_str_en;
int *pbls_data;

//color manager & color customize
int satbyy_base_curve[SATBYY_CURVE_LENGTH * SATBYY_CURVE_NUM];
int lumabyhue_base_curve[LUMABYHUE_CURVE_LENGTH * LUMABYHUE_CURVE_NUM];
int satbyhs_base_curve[SATBYHS_CURVE_LENGTH * SATBYHS_CURVE_NUM];
int huebyhue_base_curve[HUEBYHUE_CURVE_LENGTH * HUEBYHUE_CURVE_NUM];
int huebyhy_base_curve[HUEBYHY_CURVE_LENGTH * HUEBYHY_CURVE_NUM];
int huebyhs_base_curve[HUEBYHS_CURVE_LENGTH * HUEBYHS_CURVE_NUM];
int satbyhy_base_curve[SATBYHY_CURVE_LENGTH * SATBYHY_CURVE_NUM];

int satbyhs_curve[SATBYHS_CURVE_LENGTH * SATBYHS_CURVE_NUM];
int huebyhue_curve[HUEBYHUE_CURVE_LENGTH * HUEBYHUE_CURVE_NUM];
int lumabyhue_curve[LUMABYHUE_CURVE_LENGTH * LUMABYHUE_CURVE_NUM];

static struct vpq_cms_s cms_param;
static struct pq_cm_base_parm_s *pcm_base_data;

//tmo
struct vpp_tmo_param_s tmo_param;

//aipq
struct vpp_aipq_table_s aipq_table;

//aisr
struct vpp_aisr_param_s aisr_parm;
struct vpp_aisr_nn_param_s aisr_nn_parm;

//chroma coring
bool ccoring_en;
unsigned int *pccoring_data;

#define RELOAD_PQ_FOR_SAME_TIMING    (1) //ex: unplug->plug cable

static void _buffer_init(void)
{
	unsigned int vpp_gma_buf = sizeof(unsigned int) * VPQ_GAMMA_TABLE_LEN;

	vpp_gma_table.r_data = kmalloc(vpp_gma_buf, GFP_KERNEL);
	vpp_gma_table.g_data = kmalloc(vpp_gma_buf, GFP_KERNEL);
	vpp_gma_table.b_data = kmalloc(vpp_gma_buf, GFP_KERNEL);

	dnlp_enable = false;
	lc_enable = false;
	blkext_en = false;
	blue_str_en = false;
	ccoring_en = false;

	pdnlp_data = NULL;
	pble_data = NULL;
	pbls_data = NULL;
	pcm_base_data =  NULL;
	pccoring_data = NULL;

	memset(satbyy_base_curve, 0, sizeof(satbyy_base_curve));
	memset(lumabyhue_base_curve, 0, sizeof(lumabyhue_base_curve));
	memset(satbyhs_base_curve, 0, sizeof(satbyhs_base_curve));
	memset(huebyhue_base_curve, 0, sizeof(huebyhue_base_curve));
	memset(huebyhy_base_curve, 0, sizeof(huebyhy_base_curve));
	memset(huebyhs_base_curve, 0, sizeof(huebyhs_base_curve));
	memset(satbyhy_base_curve, 0, sizeof(satbyhy_base_curve));
}

static void _buffer_free(void)
{
	kfree(vpp_gma_table.r_data);
	kfree(vpp_gma_table.g_data);
	kfree(vpp_gma_table.b_data);
}

void vpq_table_init(void)
{
	VPQ_PR_DRV("%s start\n", __func__);

	int i = 0;

	for (i = 0; i < VPQ_ITEM_MAX; i++)
		pic_mode_item.data[i] = 0xff;

	for (i = 0; i < PQ_INDEX_MAX; i++)
		pre_pq_table_index[i] = 0xff;

	memset(&rgb_ogo, 0, sizeof(struct vpq_rgb_ogo_s));
	memset(&lc_curve, 0, sizeof(struct vpp_lc_curve_s));
	memset(&lc_param, 0, sizeof(struct vpp_lc_param_s));
	memset(&tmo_param, 0, sizeof(struct vpp_tmo_param_s));
	memset(&aipq_table, 0, sizeof(struct vpp_aipq_table_s));
	memset(&aisr_parm, 0, sizeof(struct vpp_aisr_param_s));
	memset(&aisr_nn_parm, 0, sizeof(struct vpp_aisr_nn_param_s));

	_buffer_init();
}

void vpq_table_deinit(void)
{
	_buffer_free();
}

/*
 * api to reset vpq default pqtable
 */
int vpq_init_default_pqtable(struct vpq_pqtable_bin_param_s *pdata)
{
	VPQ_PR_DRV("%s start\n", __func__);

	if (!pdata) {
		VPQ_PR_ERR("%s pdata is null\n", __func__);
		return -1;
	}

	if (memcpy(&pq_table_param, (struct PQ_TABLE_PARAM *)pdata->ptr,
		sizeof(struct PQ_TABLE_PARAM)) == NULL) {
		VPQ_PR_ERR("%s memcpy faile\n", __func__);
		return -1;
	}
	VPQ_PR_DRV("%s pdata->len:0x%x, struct size:0x%x\n",
		__func__, pdata->len, sizeof(struct PQ_TABLE_PARAM));

	return 0;
}

/*
 * set pq module state cfg of pq table bin to vpp when boot up
 */
int vpq_set_pq_module_cfg(struct vpq_pqmodule_cfg_s *pdata)
{
	VPQ_PR_DRV("%s start\n", __func__);

	int ret = -1;
	struct vpp_pq_state_s pq_state;

	memset(&pq_state, 0, sizeof(struct vpp_pq_state_s));

	//set vpq local pq_module_cfg
	memcpy(&pq_module_cfg, pdata, sizeof(struct TABLE_PQ_MODULE_CFG));

	//pick up pq module cfg to vpp module
	pq_state.pq_en = pq_module_cfg.pq_en;
	pq_state.pq_cfg.vadj1_en = pq_module_cfg.vadj1_en;
	pq_state.pq_cfg.vd1_ctrst_en = pq_module_cfg.vd1_ctrst_en;
	pq_state.pq_cfg.vadj2_en = pq_module_cfg.vadj2_en;
	pq_state.pq_cfg.post_ctrst_en = pq_module_cfg.post_ctrst_en;
	pq_state.pq_cfg.pregamma_en = pq_module_cfg.pregamma_en;
	pq_state.pq_cfg.gamma_en = pq_module_cfg.gamma_en;
	pq_state.pq_cfg.wb_en = pq_module_cfg.wb_en;
	pq_state.pq_cfg.dnlp_en = pq_module_cfg.dnlp_en;
	pq_state.pq_cfg.lc_en = pq_module_cfg.lc_en;
	pq_state.pq_cfg.black_ext_en = pq_module_cfg.black_ext_en;
	pq_state.pq_cfg.blue_stretch_en = pq_module_cfg.blue_stretch_en;
	pq_state.pq_cfg.chroma_cor_en = pq_module_cfg.chroma_cor_en;
	pq_state.pq_cfg.sharpness0_en = pq_module_cfg.sharpness0_en;
	pq_state.pq_cfg.sharpness1_en = pq_module_cfg.sharpness1_en;
	pq_state.pq_cfg.cm_en = pq_module_cfg.cm_en;
	pq_state.pq_cfg.lut3d_en = pq_module_cfg.lut3d_en;
	pq_state.pq_cfg.dejaggy_sr0_en = pq_module_cfg.dejaggy_sr0_en;
	pq_state.pq_cfg.dejaggy_sr1_en = pq_module_cfg.dejaggy_sr1_en;
	pq_state.pq_cfg.dering_sr0_en = pq_module_cfg.dering_sr0_en;
	pq_state.pq_cfg.dering_sr1_en = pq_module_cfg.dering_sr1_en;

	ret = vpp_pq_mgr_set_status(&pq_state);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d\n", __func__, ret);

	return ret;
}

//get current pq module state from vpp
void vpq_get_pq_module_status(struct vpq_pq_state_s *pdata)
{
	struct vpp_pq_state_s pq_state;

	memset(&pq_state, 0, sizeof(struct vpp_pq_state_s));

	vpp_pq_mgr_get_status(&pq_state);

	pdata->pq_en = pq_state.pq_en;
	pdata->pq_cfg.vadj1_en = pq_state.pq_cfg.vadj1_en;
	pdata->pq_cfg.vd1_ctrst_en = pq_state.pq_cfg.vd1_ctrst_en;
	pdata->pq_cfg.vadj2_en = pq_state.pq_cfg.vadj2_en;
	pdata->pq_cfg.post_ctrst_en = pq_state.pq_cfg.post_ctrst_en;
	pdata->pq_cfg.pregamma_en = pq_state.pq_cfg.pregamma_en;
	pdata->pq_cfg.gamma_en = pq_state.pq_cfg.gamma_en;
	pdata->pq_cfg.wb_en = pq_state.pq_cfg.wb_en;
	pdata->pq_cfg.dnlp_en = pq_state.pq_cfg.dnlp_en;
	pdata->pq_cfg.lc_en = pq_state.pq_cfg.lc_en;
	pdata->pq_cfg.black_ext_en = pq_state.pq_cfg.black_ext_en;
	pdata->pq_cfg.blue_stretch_en = pq_state.pq_cfg.blue_stretch_en;
	pdata->pq_cfg.chroma_cor_en = pq_state.pq_cfg.chroma_cor_en;
	pdata->pq_cfg.sharpness0_en = pq_state.pq_cfg.sharpness0_en;
	pdata->pq_cfg.sharpness1_en = pq_state.pq_cfg.sharpness1_en;
	pdata->pq_cfg.cm_en = pq_state.pq_cfg.cm_en;
	pdata->pq_cfg.lut3d_en = pq_state.pq_cfg.lut3d_en;
	pdata->pq_cfg.dejaggy_sr0_en = pq_state.pq_cfg.dejaggy_sr0_en;
	pdata->pq_cfg.dejaggy_sr1_en = pq_state.pq_cfg.dejaggy_sr1_en;
	pdata->pq_cfg.dering_sr0_en = pq_state.pq_cfg.dering_sr0_en;
	pdata->pq_cfg.dering_sr1_en = pq_state.pq_cfg.dering_sr1_en;
}

//set pq module status on/off
int vpq_set_pq_module_status(enum vpq_module_e module, int status)
{
	int ret = -1;

	ret = vpp_pq_mgr_set_module_status((enum vpp_module_e)module, (bool)status);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d module:%d status:%d\n", __func__, ret, module, status);

	return 0;
}

static enum vpq_resolution_e vpq_get_resolution(void)
{
	int i = 0;
	int height = 0;
	int width = 0;
	int reso_h_thread[6] = {480, 576, 720, 1080, 2160, 4320};
	enum vpq_resolution_e reso_type = VPQ_4K_3840_2160;

	vpq_frm_get_height_width(&height, &width);

	for (i = 0; i < sizeof(reso_h_thread) / sizeof(int); i++) {
		if (height <= reso_h_thread[i]) {
			VPQ_PR_INFO(PR_TABLE, "%s i:%d\n", __func__, i);
			reso_type = (enum vpq_resolution_e)i;
			break;
		}
	}

	return reso_type;
}

int vpq_set_brightness(int value)
{
	int ret = -1;

	if (value == pic_mode_item.data[VPQ_ITEM_BRIGHTNESS]) {
		VPQ_PR_INFO(PR_TABLE, "%s no need load pq to vpp\n", __func__);
		return 0;
	}

	ret = vpp_pq_mgr_set_brightness(value);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d value:%d\n", __func__, ret, value);

	pic_mode_item.data[VPQ_ITEM_BRIGHTNESS] = value;

	return ret;
}

int vpq_set_contrast(int value)
{
	int ret = -1;

	if (value == pic_mode_item.data[VPQ_ITEM_CONTRAST]) {
		VPQ_PR_INFO(PR_TABLE, "%s no need load pq to vpp\n", __func__);
		return 0;
	}

	ret = vpp_pq_mgr_set_contrast(value);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d value:%d\n", __func__, ret, value);

	pic_mode_item.data[VPQ_ITEM_CONTRAST] = value;

	return ret;
}

int vpq_set_saturation(int value)
{
	int ret = -1;

	if (value == pic_mode_item.data[VPQ_ITEM_SATURATION]) {
		VPQ_PR_INFO(PR_TABLE, "%s no need load pq to vpp\n", __func__);
		return 0;
	}

	ret = vpp_pq_mgr_set_saturation(value);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d value:%d\n", __func__, ret, value);

	pic_mode_item.data[VPQ_ITEM_SATURATION] = value;

	return ret;
}

int vpq_set_hue(int value)
{
	int ret = -1;

	if (value == pic_mode_item.data[VPQ_ITEM_HUE]) {
		VPQ_PR_INFO(PR_TABLE, "%s no need load pq to vpp\n", __func__);
		return 0;
	}

	ret = vpp_pq_mgr_set_hue(value);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d value:%d\n", __func__, ret, value);

	pic_mode_item.data[VPQ_ITEM_HUE] = value;

	return ret;
}

int vpq_set_sharpness(int value)
{
	int ret = -1;

	if (value == pic_mode_item.data[VPQ_ITEM_SHARPNESS]) {
		VPQ_PR_INFO(PR_TABLE, "%s no need load pq to vpp\n", __func__);
		return 0;
	}

	ret = vpp_pq_mgr_set_sharpness(value);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d value:%d\n", __func__, ret, value);

	pic_mode_item.data[VPQ_ITEM_SHARPNESS] = value;

	return ret;
}

int vpq_set_brightness_post(int value)
{
	int ret = -1;

	ret = vpp_pq_mgr_set_brightness_post(value);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d value:%d\n", __func__, ret, value);

	return ret;
}

int vpq_set_contrast_post(int value)
{
	int ret = -1;

	ret = vpp_pq_mgr_set_contrast_post(value);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d value:%d\n", __func__, ret, value);

	return ret;
}

int vpq_set_saturation_post(int value)
{
	int ret = -1;

	ret = vpp_pq_mgr_set_saturation_post(value);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d value:%d\n", __func__, ret, value);

	return ret;
}

int vpq_set_hue_post(int value)
{
	int ret = -1;

	ret = vpp_pq_mgr_set_hue_post(value);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d value:%d\n", __func__, ret, value);

	return ret;
}

int vpq_set_gamma_table(struct vpq_gamma_table_s *pdata)
{
	int ret = -1;
	int i = 0;

	if (!vpp_gma_table.r_data || !vpp_gma_table.g_data || !vpp_gma_table.b_data) {
		VPQ_PR_INFO(PR_TABLE, "%s malloc fail\n", __func__);
		return -1;
	}

	for (i = 0; i < VPQ_GAMMA_TABLE_LEN; i++) {
		vpp_gma_table.r_data[i] = pdata->r_data[i];
		vpp_gma_table.g_data[i] = pdata->g_data[i];
		vpp_gma_table.b_data[i] = pdata->b_data[i];
	}

	ret = vpp_pq_mgr_set_gamma_table(&vpp_gma_table);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d\n", __func__, ret);

	return ret;
}

int vpq_set_pre_gamma_table(struct vpq_pre_gamma_table_s *pdata)
{
	int ret = -1;
	int i = 0;
	struct vpp_gamma_table_s pre_gam_data;

	memset(&pre_gam_data, 0, sizeof(struct vpp_gamma_table_s));

	for (i = 0; i < VPQ_PRE_GAMMA_TABLE_LEN; i++) {
		pre_gam_data.r_data[i] = pdata->r_data[i];
		pre_gam_data.g_data[i] = pdata->g_data[i];
		pre_gam_data.b_data[i] = pdata->b_data[i];

		VPQ_PR_INFO(PR_TABLE, "%s pre_gam_data.r/g/b[%d]:0x%x 0x%x 0x%x\n",
			__func__, i,
			pre_gam_data.r_data[i], pre_gam_data.g_data[i], pre_gam_data.b_data[i]);
	}

	ret = vpp_pq_mgr_set_pre_gamma_table(&pre_gam_data);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d\n", __func__, ret);

	return ret;
}

int vpq_set_rgb_ogo(struct vpq_rgb_ogo_s *pdata)
{
	int ret = -1;

	if (memcmp(pdata, &rgb_ogo, sizeof(struct vpq_rgb_ogo_s)) == 0) {
		VPQ_PR_INFO(PR_TABLE, "%s same data, no need load pq to vpp\n", __func__);
		return 0;
	}

	ret = vpp_pq_mgr_set_whitebalance((struct vpp_white_balance_s *)pdata);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d\n", __func__, ret);

	memcpy(&rgb_ogo, pdata, sizeof(struct vpq_rgb_ogo_s));

	return ret;
}

//according to ui "color manager" level, load cm2 base curve from bin, send to vpp
int vpq_set_color_base(int value)
{
	int ret = -1;
	int i = 0;
	int table_index = 255;

	VPQ_PR_INFO(PR_TABLE, "%s value:%d\n", __func__, value);
	if (value >= CM_LV_MAX)
		return -1;

	//pick up color base table from "cm_base_table"
	table_index = vpq_module_timing_table_index(PQ_INDEX_CM2);
	VPQ_PR_INFO(PR_TABLE, "%s table_index:%d\n", __func__, table_index);
	if (table_index == 255) //means no reg table need set
		return -1;

	if (table_index == pre_pq_table_index[PQ_INDEX_CM2] &&
		value == pic_mode_item.data[VPQ_ITEM_COLOR_BASE]) {
		VPQ_PR_INFO(PR_TABLE, "%s same table, no need load pq to vpp\n", __func__);
		return 0;
	}

	pcm_base_data = &pq_table_param.cm_base_table[table_index][value];

	//LumaByHue
	for (i = 0; i < LUMABYHUE_CURVE_LENGTH; i++)
		lumabyhue_base_curve[i] = pcm_base_data->lumabyhue_0[i];

	ret = vpp_pq_mgr_set_cm_curve(EN_CM_LUMA, lumabyhue_base_curve);

	//SatByHS
	for (i = 0; i < SATBYHS_CURVE_LENGTH; i++) {
		satbyhs_base_curve[i + SATBYHS_CURVE_LENGTH * 0] = pcm_base_data->satbyhs_0[i];
		satbyhs_base_curve[i + SATBYHS_CURVE_LENGTH * 1] = pcm_base_data->satbyhs_1[i];
		satbyhs_base_curve[i + SATBYHS_CURVE_LENGTH * 2] = pcm_base_data->satbyhs_2[i];
	}
	ret |= vpp_pq_mgr_set_cm_curve(EN_CM_SAT, satbyhs_base_curve);

	//HueByHue
	for (i = 0; i < HUEBYHUE_CURVE_LENGTH; i++)
		huebyhue_base_curve[i] = pcm_base_data->huebyhue_0[i];

	ret |= vpp_pq_mgr_set_cm_curve(EN_CM_HUE, huebyhue_base_curve);

	//HueByHS
	for (i = 0; i < HUEBYHS_CURVE_LENGTH; i++) {
		huebyhs_base_curve[i + HUEBYHS_CURVE_LENGTH * 0] = pcm_base_data->huebyhs_0[i];
		huebyhs_base_curve[i + HUEBYHS_CURVE_LENGTH * 1] = pcm_base_data->huebyhs_1[i];
		huebyhs_base_curve[i + HUEBYHS_CURVE_LENGTH * 2] = pcm_base_data->huebyhs_2[i];
		huebyhs_base_curve[i + HUEBYHS_CURVE_LENGTH * 3] = pcm_base_data->huebyhs_3[i];
		huebyhs_base_curve[i + HUEBYHS_CURVE_LENGTH * 4] = pcm_base_data->huebyhs_4[i];
	}
	ret |= vpp_pq_mgr_set_cm_curve(EN_CM_HUE_BY_HS, huebyhs_base_curve);

	//SatByY
	for (i = 0; i < SATBYY_CURVE_LENGTH; i++)
		satbyy_base_curve[i] = pcm_base_data->satbyy_0[i];

	ret |= vpp_pq_mgr_set_cm_curve(EN_CM_SAT_BY_L, satbyy_base_curve);

	//SatByHY
	for (i = 0; i < SATBYHY_CURVE_LENGTH; i++) {
		satbyhy_base_curve[i + SATBYHY_CURVE_LENGTH * 0] = pcm_base_data->satbyhy_0[i];
		satbyhy_base_curve[i + SATBYHY_CURVE_LENGTH * 1] = pcm_base_data->satbyhy_1[i];
		satbyhy_base_curve[i + SATBYHY_CURVE_LENGTH * 2] = pcm_base_data->satbyhy_2[i];
		satbyhy_base_curve[i + SATBYHY_CURVE_LENGTH * 3] = pcm_base_data->satbyhy_3[i];
		satbyhy_base_curve[i + SATBYHY_CURVE_LENGTH * 4] = pcm_base_data->satbyhy_4[i];
	}
	ret |= vpp_pq_mgr_set_cm_curve(EN_CM_SAT_BY_HL, satbyhy_base_curve);

	//HueByHY
	for (i = 0; i < HUEBYHY_CURVE_LENGTH; i++) {
		huebyhy_base_curve[i + HUEBYHY_CURVE_LENGTH * 0] = pcm_base_data->huebyhy_0[i];
		huebyhy_base_curve[i + HUEBYHY_CURVE_LENGTH * 1] = pcm_base_data->huebyhy_1[i];
		huebyhy_base_curve[i + HUEBYHY_CURVE_LENGTH * 2] = pcm_base_data->huebyhy_2[i];
		huebyhy_base_curve[i + HUEBYHY_CURVE_LENGTH * 3] = pcm_base_data->huebyhy_3[i];
		huebyhy_base_curve[i + HUEBYHY_CURVE_LENGTH * 4] = pcm_base_data->huebyhy_4[i];
	}
	ret |= vpp_pq_mgr_set_cm_curve(EN_CM_HUE_BY_HL, huebyhy_base_curve);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d\n", __func__, ret);

	pic_mode_item.data[VPQ_ITEM_COLOR_BASE] = value;
	pre_pq_table_index[PQ_INDEX_CM2] = table_index;

	return ret;
}

//according to ui "color customize" values, calculate new curve, add base curver, send to vpp
int vpq_set_color_customize(struct vpq_cms_s *pdata)
{
	int ret = -1;
	int i = 0;
	int color_custom_curve[CM_CUSTOM_CURVE_LENGTH];
	struct color_adj_param_s adj_param;

	memset(&adj_param, 0, sizeof(struct color_adj_param_s));

	if (!pcm_base_data) {
		VPQ_PR_INFO(PR_TABLE, "%s base table hasn't been load\n", __func__);
		return -EFAULT;
	}

	VPQ_PR_INFO(PR_TABLE, "%s pdata:%d %d %d %d %d\n",
		__func__,
		pdata->color_type, pdata->color_9, pdata->color_14,
		pdata->cms_type, pdata->value);

	if (memcmp(pdata, &cms_param, sizeof(struct vpq_cms_s)) == 0) {
		VPQ_PR_INFO(PR_TABLE, "%s same data, no need load pq to vpp\n", __func__);
		return 0;
	}

	adj_param.color_type = (enum color_adj_type_e)pdata->color_type;
	adj_param.value = pdata->value;

	if (pdata->color_9 == VPQ_COLOR_RED)
		adj_param.mode_9 = EN_CAM9_RED;
	else if (pdata->color_9 == VPQ_COLOR_GREEN)
		adj_param.mode_9 = EN_CAM9_GREEN;
	else if (pdata->color_9 == VPQ_COLOR_BLUE)
		adj_param.mode_9 = EN_CAM9_BLUE;
	else if (pdata->color_9 == VPQ_COLOR_CYAN)
		adj_param.mode_9 = EN_CAM9_CYAN;
	else if (pdata->color_9 == VPQ_COLOR_MAGENTA)
		adj_param.mode_9 = EN_CAM9_PURPLE;
	else if (pdata->color_9 == VPQ_COLOR_YELLOW)
		adj_param.mode_9 = EN_CAM9_YELLOW;
	else if (pdata->color_9 == VPQ_COLOR_SKIN)
		adj_param.mode_9 = EN_CAM9_SKIN;
	else if (pdata->color_9 == VPQ_COLOR_YELLOW_GREEN)
		adj_param.mode_9 = EN_CAM9_YELLOW_GREEN;
	else if (pdata->color_9 == VPQ_COLOR_BLUE_GREEN)
		adj_param.mode_9 = EN_CAM9_BLUE_GREEN;

	if (pdata->cms_type == VPQ_CMS_SAT) {
		adj_param.curve_type = EN_COLOR_ADJ_CURVE_SAT;
		color_adj_by_mode(adj_param, color_custom_curve);

		for (i = 0; i < SATBYHS_CURVE_LENGTH; i++) {
			satbyhs_curve[i + SATBYHS_CURVE_LENGTH * 0] =
				pcm_base_data->satbyhs_0[i] + color_custom_curve[i];
			satbyhs_curve[i + SATBYHS_CURVE_LENGTH * 1] =
				pcm_base_data->satbyhs_1[i] + color_custom_curve[i];
			satbyhs_curve[i + SATBYHS_CURVE_LENGTH * 2] =
				pcm_base_data->satbyhs_2[i] + color_custom_curve[i];
		}

		ret = vpp_pq_mgr_set_cm_curve(EN_CM_SAT, satbyhs_curve);
	} else if (pdata->cms_type == VPQ_CMS_HUE) {
		adj_param.curve_type = EN_COLOR_ADJ_CURVE_HUE;
		color_adj_by_mode(adj_param, color_custom_curve);

		for (i = 0; i < HUEBYHUE_CURVE_LENGTH; i++)
			huebyhue_curve[i] =
				pcm_base_data->huebyhue_0[i] + color_custom_curve[i];

		ret = vpp_pq_mgr_set_cm_curve(EN_CM_HUE, huebyhue_curve);
	} else if (pdata->cms_type == VPQ_CMS_LUMA) {
		adj_param.curve_type = EN_COLOR_ADJ_CURVE_LUMA;
		color_adj_by_mode(adj_param, color_custom_curve);

		for (i = 0; i < LUMABYHUE_CURVE_LENGTH; i++)
			lumabyhue_curve[i] =
				pcm_base_data->lumabyhue_0[i] + color_custom_curve[i];

		ret = vpp_pq_mgr_set_cm_curve(EN_CM_LUMA, lumabyhue_curve);
	}

	VPQ_PR_INFO(PR_TABLE, "%s ret:%d\n", __func__, ret);

	memcpy(&cms_param, pdata, sizeof(struct vpq_cms_s));

	return ret;
}

int vpq_set_dnlp_mode(int value)
{
	int ret = -1;
	int table_index = 255;

	VPQ_PR_INFO(PR_TABLE, "%s value:%d\n", __func__, value);
	if (value >= DNLP_LV_MAX)
		return -1;

	//pick up dnlp table from "pq_index_table0"
	table_index = vpq_module_timing_table_index(PQ_INDEX_DNLP);
	VPQ_PR_INFO(PR_TABLE, "%s table_index:%d\n", __func__, table_index);
	if (table_index == 255) //means no reg table need set
		return -1;

	if (table_index == pre_pq_table_index[PQ_INDEX_DNLP] &&
		value == pic_mode_item.data[VPQ_ITEM_DYNAMIC_CONTRAST]) {
		VPQ_PR_INFO(PR_TABLE, "%s same table, no need load pq to vpp\n", __func__);
		return 0;
	}

	//"dnlp_enable" refer "pq_dnlp_param_e" PQ_DNLP_ENABLE
	if (pq_table_param.dnlp_table[table_index][value].dnlp_enable[0])
		dnlp_enable = true;
	else
		dnlp_enable = false;
	ret = vpp_pq_mgr_set_module_status(EN_MODULE_DNLP, dnlp_enable);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d\n", __func__, ret);

	pdnlp_data =
		(struct vpp_dnlp_curve_param_s *)&pq_table_param.dnlp_table[table_index][value];
	if (!pdnlp_data)
		return -EFAULT;

	ret |= vpp_pq_mgr_set_dnlp_param(pdnlp_data);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d\n", __func__, ret);

	pic_mode_item.data[VPQ_ITEM_DYNAMIC_CONTRAST] = value;
	pre_pq_table_index[PQ_INDEX_DNLP] = table_index;

	return ret;
}

int vpq_set_csc_type(int value)
{
	int ret = -1;

	ret = vpp_pq_mgr_set_csc_type(value);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d value:%d\n", __func__, ret, value);

	return ret;
}

int vpq_get_csc_type(void)
{
	int ret = -1;

	ret = vpp_pq_mgr_get_csc_type();
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d\n", __func__, ret);

	return ret;
}

int vpq_get_hist_avg(struct vpq_histgm_ave_s *pdata)
{
	if (!pdata)
		return -1;

	vpp_pq_mgr_get_histogram_ave((struct vpp_histgm_ave_s *)pdata);

	return 0;
}

int vpq_get_histogram(struct vpq_histgm_param_s *pdata)
{
	if (!pdata)
		return -1;

	vpp_pq_mgr_get_histogram((struct vpp_histgm_param_s *)pdata);

	return 0;
}

int vpq_set_lc_mode(int value)
{
	int ret = -1;
	int table_index = 255;

	VPQ_PR_INFO(PR_TABLE, "%s value:%d\n", __func__, value);
	if (value >= LC_LV_MAX)
		return -1;

	//pick up lc curve and reg table from "pq_index_table0"
	table_index = vpq_module_timing_table_index(PQ_INDEX_LC);
	VPQ_PR_INFO(PR_TABLE, "%s table_index:%d\n", __func__, table_index);
	if (table_index == 255) //means no reg table need set
		return -1;

	if (table_index == pre_pq_table_index[PQ_INDEX_LC] &&
		value == pic_mode_item.data[VPQ_ITEM_LOCAL_CONTRAST]) {
		VPQ_PR_INFO(PR_TABLE, "%s same table, no need load pq to vpp\n", __func__);
		return 0;
	}

	//pq_table_param.lc_node_table.param seems not used by vpp
	memcpy(&lc_curve, &pq_table_param.lc_node_table[table_index][value],
			sizeof(struct vpp_lc_curve_s));

	ret = vpp_pq_mgr_set_lc_curve(&lc_curve);
	VPQ_PR_INFO(PR_TABLE, "%s set_lc_curve ret:%d\n", __func__, ret);

	//"lc_enable" refer "pq_lc_reg_parm_s" data[0]
	if (pq_table_param.lc_reg_table[table_index][value].lc_enable)
		lc_enable = true;
	else
		lc_enable = false;
	ret = vpp_pq_mgr_set_module_status(EN_MODULE_LC, lc_enable);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d lc_enable:%d\n", __func__, ret, lc_enable);

	memcpy(&lc_param,
		&pq_table_param.lc_reg_table[table_index][value].lc_curve_nodes_vlpf,
		sizeof(struct vpp_lc_param_s));

	ret |= vpp_pq_mgr_set_lc_param(&lc_param);
	VPQ_PR_INFO(PR_TABLE, "%s set_lc_param ret:%d\n", __func__, ret);

	pic_mode_item.data[VPQ_ITEM_LOCAL_CONTRAST] = value;
	pre_pq_table_index[PQ_INDEX_LC] = table_index;

	return ret;
}

int vpq_set_hdr_tmo_mode(int value)
{
	int ret = -1;
	int table_index = 255;

	VPQ_PR_INFO(PR_TABLE, "%s value:%d\n", __func__, value);
	if (value >= HDR_TMO_LV_MAX)
		return -1;

	table_index = vpq_module_timing_table_index(PQ_INDEX_HDR_TMO);
	VPQ_PR_INFO(PR_TABLE, "%s table_index:%d\n", __func__, table_index);
	if (table_index == 255) //means no reg table need set
		return -1;

	if (table_index == pre_pq_table_index[PQ_INDEX_HDR_TMO] &&
		value == pic_mode_item.data[VPQ_ITEM_TMO]) {
		VPQ_PR_INFO(PR_TABLE, "%s same table, no need load pq to vpp\n", __func__);
		return 0;
	}

	memcpy(&tmo_param, &pq_table_param.hdr_tmo_table[table_index][value],
			sizeof(struct vpp_tmo_param_s));

	ret = vpp_pq_mgr_set_hdr_tmo_param(&tmo_param);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d\n", __func__, ret);

	pic_mode_item.data[VPQ_ITEM_TMO] = value;
	pre_pq_table_index[PQ_INDEX_HDR_TMO] = table_index;

	return ret;
}

int vpq_set_hdr_tmo_data(struct vpq_hdr_lut_s *pdata)
{
	int ret = -1;

	if (!pdata)
		return ret;
	ret = vpp_pq_mgr_set_hdr_tmo_curve((struct vpp_hdr_lut_s *)pdata);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d\n", __func__, ret);

	return ret;
}

int vpq_set_aipq_mode(int value)
{
	int ret = -1;
	int table_index = 255;

	VPQ_PR_INFO(PR_TABLE, "%s value:%d\n", __func__, value);

	table_index = vpq_module_timing_table_index(PQ_INDEX_AIPQ);
	VPQ_PR_INFO(PR_TABLE, "%s table_index:%d\n", __func__, table_index);
	if (table_index == 255) //means no reg table need set
		return -1;

	if (table_index == pre_pq_table_index[PQ_INDEX_AIPQ]) {
		VPQ_PR_INFO(PR_TABLE, "%s same data, no need load pq to vpp\n", __func__);
		return 0;
	}

	aipq_table.height = pq_table_param.aipq_table[table_index].ai_size.height;
	aipq_table.width = pq_table_param.aipq_table[table_index].ai_size.width;
	aipq_table.table_ptr = (void *)pq_table_param.aipq_table[table_index].ai_pq_table;
	if (!aipq_table.table_ptr) {
		VPQ_PR_INFO(PR_TABLE, "%s malloc buf failed\n", __func__);
		return -EFAULT;
	}

	ret = vpp_pq_mgr_set_aipq_data(&aipq_table);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d\n", __func__, ret);

	pic_mode_item.data[VPQ_ITEM_AIPQ] = value;
	pre_pq_table_index[PQ_INDEX_AIPQ] = table_index;

	return ret;
}

int vpq_set_aisr_mode(int value)
{
	int ret = -1;
	int table_index = 255;
	int aisr_type = AISR_MAX;
	enum vpq_resolution_e input_reso = VPQ_FHD_1920_1080;
	const struct vinfo_s *vinfo = NULL;

	VPQ_PR_INFO(PR_TABLE, "%s value:%d\n", __func__, value);

	table_index = vpq_module_timing_table_index(PQ_INDEX_AISR);
	VPQ_PR_INFO(PR_TABLE, "%s table_index:%d\n", __func__, table_index);
	if (table_index == 255) //means no reg table need set
		return -1;

	if (value == pic_mode_item.data[VPQ_ITEM_AISR] &&
		table_index == pre_pq_table_index[PQ_INDEX_AISR]) {
		VPQ_PR_INFO(PR_TABLE, "%s same data, no need load pq to vpp\n", __func__);
		return 0;
	}

	input_reso = vpq_get_resolution();
	VPQ_PR_INFO(PR_TABLE, "%s input_reso:%d\n", __func__, input_reso);

	vinfo = get_current_vinfo();
	if (!vinfo)
		VPQ_PR_INFO(PR_TABLE, "%s vinfo is null\n", __func__);
	VPQ_PR_INFO(PR_TABLE, "%s output reso:%d*%d\n", __func__, vinfo->width, vinfo->height);

	//temp define just output 4k has aisr
	if (vinfo->height != 2160)
		return ret;

	if (input_reso == VPQ_SD_576_480 || input_reso == VPQ_SD_720_576)
		aisr_type = AISR_SD_T_4K;
	else if (input_reso == VPQ_HD_1280_720)
		aisr_type = AISR_HD_T_4K;
	else if (input_reso == VPQ_FHD_1920_1080)
		aisr_type = AISR_FHD_T_4K;
	else
		return ret;
	VPQ_PR_INFO(PR_TABLE, "%s aisr_type:%d\n", __func__, aisr_type);

	memcpy(&aisr_parm,
		&pq_table_param.ai_sr_table[table_index][aisr_type][value].aisr_pram,
		sizeof(struct vpp_aisr_param_s));
	memcpy(&aisr_nn_parm,
		&pq_table_param.ai_sr_table[table_index][aisr_type][value].aisr_nn_parm,
		sizeof(struct vpp_aisr_nn_param_s));

	ret = vpp_pq_mgr_set_aisr_param(&aisr_parm, &aisr_nn_parm);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d\n", __func__, ret);

	pic_mode_item.data[VPQ_ITEM_AISR] = value;
	pre_pq_table_index[PQ_INDEX_AISR] = table_index;

	return ret;
}

enum vpq_color_primary_e vpq_get_color_primary(void)
{
	enum vpp_color_primary_e color_pri = EN_COLOR_PRI_NULL;

	color_pri = vpp_pq_mgr_get_color_primary();

	return (enum vpq_color_primary_e)color_pri;
}

int vpq_get_hdr_metadata(struct vpq_hdr_metadata_s *pdata)
{
	vpp_pq_mgr_get_hdr_metadata((struct vpp_hdr_metadata_s *)pdata);

	return 0;
}

int vpq_set_black_stretch(int value)
{
	int ret = -1;
	int table_index = 255;
	int length = 4; //"pq_blkext_parm_s" data[1]~data[4]

	VPQ_PR_INFO(PR_TABLE, "%s value:%d\n", __func__, value);
	if (value >= BLKEXT_LV_MAX)
		return -1;

	//pick up blkext reg table from "pq_index_table0"
	table_index = vpq_module_timing_table_index(PQ_INDEX_BLACKEXT);
	VPQ_PR_INFO(PR_TABLE, "%s table_index:%d\n", __func__, table_index);
	if (table_index == 255) //means no reg table need set
		return -1;

	if (table_index == pre_pq_table_index[PQ_INDEX_BLACKEXT] &&
		value == pic_mode_item.data[VPQ_ITEM_BLACK_STRETCH]) {
		VPQ_PR_INFO(PR_TABLE, "%s same table, no need load pq to vpp\n", __func__);
		return 0;
	}

	//"blackext_enable" refer "pq_blkext_parm_s" data[0]
	if (pq_table_param.blkext_table[table_index][value].blackext_enable)
		blkext_en = true;
	else
		blkext_en = false;
	ret = vpp_pq_mgr_set_module_status(EN_MODULE_BLE, blkext_en);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d\n", __func__, ret);

	//"blackext_start"~"blackext_slope2" refer "pq_blkext_parm_s" data[1]~data[4]
	pble_data = (int *)&pq_table_param.blkext_table[table_index][value].blackext_start;
	vpp_pq_mgr_set_blkext_params(pble_data, length);

	pic_mode_item.data[VPQ_ITEM_BLACK_STRETCH] = value;
	pre_pq_table_index[PQ_INDEX_BLACKEXT] = table_index;

	return 0;
}

int vpq_set_blue_stretch(int value)
{
	int ret = -1;
	int table_index = 255;
	int length = 18; //"blue_str_table" data[1]~data[18]

	VPQ_PR_INFO(PR_TABLE, "%s value:%d\n", __func__, value);
	if (value >= BLUESTR_LV_MAX)
		return -1;

	//pick up blue_stretch reg table from "pq_index_table0"
	table_index = vpq_module_timing_table_index(PQ_INDEX_BLUE_STRETCH);
	VPQ_PR_INFO(PR_TABLE, "%s table_index:%d\n", __func__, table_index);
	if (table_index == 255) //means no reg table need set
		return -1;

	if (table_index == pre_pq_table_index[PQ_INDEX_BLUE_STRETCH] &&
		value == pic_mode_item.data[VPQ_ITEM_BLUE_STRETCH]) {
		VPQ_PR_INFO(PR_TABLE, "%s same table, no need load pq to vpp\n", __func__);
		return 0;
	}

	//"bluestretch_en_sel"~"bluestretch_error_cbn_inv" refer "blue_str_table" data[1]~data[18]
	pbls_data = (int *)&pq_table_param.blue_str_table[table_index][value].bluestretch_en_sel;

	vpp_pq_mgr_set_blue_stretch_params(pbls_data, length);

	//"bluestretch_enable" refer "blue_str_table" data[0]
	if (pq_table_param.blue_str_table[table_index][value].bluestretch_enable)
		blue_str_en = true;
	else
		blue_str_en = false;
	ret = vpp_pq_mgr_set_module_status(EN_MODULE_BLS, blue_str_en);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d\n", __func__, ret);

	pic_mode_item.data[VPQ_ITEM_BLUE_STRETCH] = value;
	pre_pq_table_index[PQ_INDEX_BLUE_STRETCH] = table_index;

	return 0;
}

int vpq_set_chroma_coring(int value)
{
	int ret = -1;
	int table_index = 255;
	int length = 3; //"ccoring_table" data[1]~data[3]

	VPQ_PR_INFO(PR_TABLE, "%s value:%d\n", __func__, value);
	if (value >= CCORING_LV_MAX)
		return -1;

	//pick up chroma_coring reg table from "pq_index_table0"
	table_index = vpq_module_timing_table_index(PQ_INDEX_CHROMA_CORING);
	VPQ_PR_INFO(PR_TABLE, "%s table_index:%d\n", __func__, table_index);
	if (table_index == 255) //means no reg table need set
		return -1;

	if (table_index == pre_pq_table_index[PQ_INDEX_CHROMA_CORING] &&
		value == pic_mode_item.data[VPQ_ITEM_CHROMA_CORING]) {
		VPQ_PR_INFO(PR_TABLE, "%s same table, no need load pq to vpp\n", __func__);
		return 0;
	}

	//"chroma_coring_enable" refer "ccoring_table" data[0]
	if (pq_table_param.ccoring_table[table_index][value].chroma_coring_enable)
		ccoring_en = true;
	else
		ccoring_en = false;
	ret = vpp_pq_mgr_set_module_status(EN_MODULE_CCORING, ccoring_en);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d\n", __func__, ret);

	//"chroma_coring_y_threshold"~"chroma_coring_slope" refer "ccoring_table" data[1]~data[3]
	pccoring_data =
	 (unsigned int *)&pq_table_param.ccoring_table[table_index][value].chroma_coring_slope;

	vpp_pq_mgr_set_ccoring_params(pccoring_data, length);

	//"center_bar_enable" ~ "left_top_scrn_width" no need we control

	pic_mode_item.data[VPQ_ITEM_CHROMA_CORING] = value;
	pre_pq_table_index[PQ_INDEX_CHROMA_CORING] = table_index;

	return 0;
}

int vpq_set_xvycc_mode(int value)
{
	//todo
	return -1;
}

int vpq_set_nr(int value)
{
	//todo
	pic_mode_item.data[VPQ_ITEM_NR] = value;
	return -1;
}

int vpq_set_deblock(int value)
{
	//todo
	pic_mode_item.data[VPQ_ITEM_DEBLOCK] = value;
	return -1;
}

int vpq_set_demosquito(int value)
{
	//todo
	pic_mode_item.data[VPQ_ITEM_DEMOSQUITO] = value;
	return -1;
}

int vpq_set_mcdi_mode(int value)
{
	//todo
	pic_mode_item.data[VPQ_ITEM_MCDI] = value;
	return -1;
}

int vpq_set_smoothplus_mode(int value)
{
	//todo
	pic_mode_item.data[VPQ_ITEM_SMOOTHPLUS] = value;
	return -1;
}

int vpq_set_cabc(void)
{
	int ret = -1;
	struct vpp_cabc_param_s cabc_param;

	memset(&cabc_param, 0, sizeof(struct vpp_cabc_param_s));

	ret = vpp_pq_mgr_set_cabc_param(&cabc_param);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d\n", __func__, ret);

	return ret;
}

int vpq_set_add(void)
{
	int ret = -1;
	struct vpp_aad_param_s add_param;

	memset(&add_param, 0, sizeof(struct vpp_aad_param_s));

	ret = vpp_pq_mgr_set_aad_param(&add_param);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d\n", __func__, ret);

	return ret;
}

int vpq_set_overscan_data(unsigned int length, struct vpq_overscan_data_s *pdata)
{
	int ret = -1;

	if (!pdata)
		return -1;

	ret = vpp_pq_mgr_set_overscan_table(length, (struct vpp_overscan_table_s *)pdata);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d\n", __func__, ret);

	return ret;
}

int vpq_set_pll_value(void)
{
	//todo
	return -1;
}

int vpq_set_pc_mode(int value)
{
	int ret = -1;

	ret = vpp_pq_mgr_set_pc_mode(value);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d value:%d\n", __func__, ret, value);

	return ret;
}

void vpq_set_pq_effect(void)
{
	VPQ_PR_DRV("%s start\n", __func__);

	vpq_set_sharpness(pic_mode_item.data[VPQ_ITEM_SHARPNESS]);
	vpq_set_dnlp_mode(pic_mode_item.data[VPQ_ITEM_DYNAMIC_CONTRAST]);
	vpq_set_lc_mode(pic_mode_item.data[VPQ_ITEM_LOCAL_CONTRAST]);
	vpq_set_black_stretch(pic_mode_item.data[VPQ_ITEM_BLACK_STRETCH]);
	vpq_set_blue_stretch(pic_mode_item.data[VPQ_ITEM_BLUE_STRETCH]);
	vpq_set_chroma_coring(pic_mode_item.data[VPQ_ITEM_CHROMA_CORING]);
	vpq_set_nr(pic_mode_item.data[VPQ_ITEM_NR]);
	vpq_set_deblock(pic_mode_item.data[VPQ_ITEM_DEBLOCK]);
	vpq_set_demosquito(pic_mode_item.data[VPQ_ITEM_DEMOSQUITO]);

	vpq_set_mcdi_mode(pic_mode_item.data[VPQ_ITEM_MCDI]);
	//todo SetDisplayMode
	vpq_set_hdr_tmo_mode(pic_mode_item.data[VPQ_ITEM_TMO]);
	vpq_set_smoothplus_mode(pic_mode_item.data[VPQ_ITEM_SMOOTHPLUS]);
	//todo AML_HAL_LD_SetLevelIdx
	vpq_set_color_base(pic_mode_item.data[VPQ_ITEM_COLOR_BASE]);

	vpq_set_aipq_mode(pic_mode_item.data[VPQ_ITEM_AIPQ]);
	vpq_set_aisr_mode(pic_mode_item.data[VPQ_ITEM_AISR]);

	VPQ_PR_DRV("%s end\n", __func__);
}

int vpq_set_frame_status(enum vpq_frame_status_e status)
{
	if (RELOAD_PQ_FOR_SAME_TIMING && status == VPQ_VFRAME_STOP) {
		VPQ_PR_DRV("%s reset local memory\n", __func__);
		int i = 0;

		for (i = 0; i < VPQ_ITEM_MAX; i++)
			pic_mode_item.data[i] = 0xff;

		for (i = 0; i < PQ_INDEX_MAX; i++)
			pre_pq_table_index[i] = 0xff;

		memset(&rgb_ogo, 0, sizeof(struct vpq_rgb_ogo_s));
	}

	return 1;
}

int vpq_get_event_info(void)
{
	enum vpq_vfm_event_info_e event_info = vpq_vfm_get_event_info();

	return (int)event_info;
}

void vpq_get_signal_info(struct vpq_signal_info_s *pdata)
{
	vpq_vfm_source_type_e src_type;
	vpq_vfm_port_e src_port;
	vpq_vfm_sig_fmt_e sig_fmt;
	vpq_vfm_trans_fmt_e trans_fmt;
	vpq_vfm_source_mode_e src_mode;
	enum vpq_vfm_hdr_type_e hdr_type;
	enum vpq_vfm_scan_mode_e scan_mode;
	unsigned int height;
	unsigned int width;
	unsigned int fps;
	bool is_dlby;

	src_type = vpq_vfm_get_source_type();
	VPQ_PR_INFO(PR_TABLE, "%s src_type:%d\n", __func__, src_type);
	switch (src_type) {
	default:
	case VFRAME_SOURCE_TYPE_OTHERS:
		pdata->src_type = VPQ_SRC_TYPE_MPEG;
		break;
	case VFRAME_SOURCE_TYPE_TUNER:
		pdata->src_type = VPQ_SRC_TYPE_ATV;
		break;
	case VFRAME_SOURCE_TYPE_CVBS:
		pdata->src_type = VPQ_SRC_TYPE_CVBS;
		break;
	case VFRAME_SOURCE_TYPE_HDMI:
		pdata->src_type = VPQ_SRC_TYPE_HDMI;
		break;
	}

	src_port = vpq_vfm_get_source_port();
	VPQ_PR_INFO(PR_TABLE, "%s src_port:0x%x\n", __func__, src_port);
	switch (src_port) {
	case TVIN_PORT_HDMI0:
		pdata->hdmi_port = VPQ_HDMI_PORT_0;
		break;
	case TVIN_PORT_HDMI1:
		pdata->hdmi_port = VPQ_HDMI_PORT_1;
		break;
	case TVIN_PORT_HDMI2:
		pdata->hdmi_port = VPQ_HDMI_PORT_2;
		break;
	case TVIN_PORT_HDMI3:
		pdata->hdmi_port = VPQ_HDMI_PORT_3;
		break;
	default:
		pdata->hdmi_port = VPQ_HDMI_PORT_NULL;
		break;
	}

	src_mode = vpq_vfm_get_source_mode();
	VPQ_PR_INFO(PR_TABLE, "%s src_mode:%d\n", __func__, src_mode);
	pdata->sig_mode = (enum vpq_sig_mode_e)src_mode;

	hdr_type = vpq_vfm_get_hdr_type();
	is_dlby = vpq_vfm_get_is_dlby();
	VPQ_PR_INFO(PR_TABLE, "%s hdr_type:%d is_dlby:%d\n", __func__, hdr_type, is_dlby);
	if (is_dlby)
		pdata->hdr_type = VPQ_HDR_TYPE_DOBVI;
	else
		pdata->hdr_type = (enum vpq_hdr_type_e)hdr_type;

	scan_mode = vpq_vfm_get_signal_scan_mode();
	VPQ_PR_INFO(PR_TABLE, "%s scan_mode:%d\n", __func__, scan_mode);
	pdata->scan_mode = scan_mode;

	sig_fmt = vpq_vfm_get_signal_format();
	VPQ_PR_INFO(PR_TABLE, "%s sig_fmt:%d(0x%x)\n", __func__, sig_fmt, sig_fmt);
	pdata->sig_fmt = (unsigned int)sig_fmt;

	trans_fmt = vpq_vfm_get_trans_format();
	VPQ_PR_INFO(PR_TABLE, "%s trans_fmt:%d\n", __func__, trans_fmt);
	pdata->trans_fmt = (unsigned int)trans_fmt;

	vpq_frm_get_height_width(&height, &width);
	VPQ_PR_INFO(PR_TABLE, "%s height:%d width:%d\n", __func__, height, width);
	pdata->height = height;
	pdata->width = width;

	vpq_frm_get_fps(&fps);
	VPQ_PR_INFO(PR_TABLE, "%s fps:%d\n", __func__, fps);
	pdata->fps = fps;
}

int vpq_dump_pq_table(enum vpq_dump_type_e value)
{
	return vpq_module_dump_pq_table(value);
}
