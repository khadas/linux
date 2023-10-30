// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vpp/vpp_drv.h>
#include <linux/amlogic/media/vpp/vpp_common_def.h>
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

//gamma & wb
struct vpq_rgb_ogo_s rgb_ogo;
struct vpq_rgb_ogo_s rgb_ogo_save;
struct vpp_gamma_table_s vpp_gma_table;

//dnlp
static unsigned char dnlp_table_index;
bool dnlp_enable;
struct vpp_dnlp_curve_param_s *pdnlp_data;

//local contrast
static unsigned char lcd_table_index;
bool lc_enable;
struct vpp_lc_curve_s lc_curve;
struct vpp_lc_param_s lc_param;

//black ext & blue stretch
static unsigned char blkext_table_index;
bool blkext_en;
int *pble_data;

static unsigned char bluestr_table_index;
bool blue_str_en;
int *pbls_data;

//color base; color customize
#define CM_CURVE_MAX_DATA_LENGTH     (32)
#define TYPE_SATBYY_CURVE_NUM        (1)
#define TYPE_SATBYHS_CURVE_NUM       (3)
#define TYPE_SATBYHY_CURVE_NUM       (5)
#define TYPE_LUMABYHUE_CURVE_NUM     (1)
#define TYPE_HUEBYHUE_CURVE_NUM      (1)
#define TYPE_HUEBYHS_CURVE_NUM       (5)
#define TYPE_HUEBYHY_CURVE_NUM       (5)

static unsigned char color_base_table_index;
static int satbyhs_table[32 * TYPE_SATBYHS_CURVE_NUM];
static int huebyhs_table[32 * TYPE_HUEBYHS_CURVE_NUM];
static int satbyhy_table[32 * TYPE_SATBYHY_CURVE_NUM];
static int huebyhy_table[32 * TYPE_HUEBYHY_CURVE_NUM];
static int color_custom_curve[VPQ_CMS_MAX][CM_CURVE_MAX_DATA_LENGTH];
static struct vpq_cms_s cms_param;
struct pq_cm_base_parm_s *pcm_base_data;

//aipq
unsigned char aipq_table_index;
struct vpp_aipq_table_s aipq_table;

//aisr
static unsigned char aisr_table_index;
struct vpp_aisr_param_s aisr_parm;
struct vpp_aisr_nn_param_s aisr_nn_parm;

//chrom coring
static unsigned char ccoring_table_index;

//hdr tmo
static unsigned char tmo_table_index;

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

	pdnlp_data = NULL;
	pble_data = NULL;
	pbls_data = NULL;
	pcm_base_data =  NULL;

	memset(color_custom_curve, 0, sizeof(color_custom_curve));
	memset(satbyhs_table, 0, sizeof(satbyhs_table));
	memset(huebyhs_table, 0, sizeof(huebyhs_table));
	memset(satbyhy_table, 0, sizeof(satbyhy_table));
	memset(huebyhy_table, 0, sizeof(huebyhy_table));
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

	for (i = 0; i < VPQ_ITEM_MAX; i++) {
		pic_mode_item.data[i] = 0xff;
		pic_mode_item.save[i] = 0xff;
	}

	memset(&rgb_ogo, 0, sizeof(struct vpq_rgb_ogo_s));
	memset(&rgb_ogo_save, 0, sizeof(struct vpq_rgb_ogo_s));
	memset(&lc_curve, 0, sizeof(struct vpp_lc_curve_s));
	memset(&lc_param, 0, sizeof(struct vpp_lc_param_s));
	memset(&aipq_table, 0, sizeof(struct vpp_aipq_table_s));
	memset(&aisr_parm, 0, sizeof(struct vpp_aisr_param_s));
	memset(&aisr_nn_parm, 0, sizeof(struct vpp_aisr_nn_param_s));

	dnlp_table_index = 0xff;
	lcd_table_index = 0xff;
	tmo_table_index = 0xff;
	aipq_table_index = 0xff;
	aisr_table_index = 0xff;
	ccoring_table_index = 0xff;
	blkext_table_index = 0xff;
	bluestr_table_index = 0xff;
	color_base_table_index = 0xff;

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

	memcpy(&pq_table_param, (struct PQ_TABLE_PARAM *)pdata->ptr, sizeof(struct PQ_TABLE_PARAM));
	VPQ_PR_DRV("%s pdata->len:0x%x, struct size:0x%x\n",
		__func__, pdata->len, sizeof(struct PQ_TABLE_PARAM));

	return 0;
}

/*
 * set pq module state cfg of pq table bin to vpp when boot up
 */
int vpq_set_pq_module_cfg(void)
{
	VPQ_PR_DRV("%s start\n", __func__);

	int ret = -1;
	struct vpp_pq_state_s pq_state;

	memset(&pq_state, 0, sizeof(struct vpp_pq_state_s));

	//send pq module cfg to vpp
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

	VPQ_PR_INFO(PR_TABLE, "%s ret:%d", __func__, ret);
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
int vpq_set_pq_module_status(enum vpq_module_e module, bool status)
{
	int ret = -1;

	ret = vpp_pq_mgr_set_module_status((enum vpp_module_e)module, status);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d module:%d status:%d", __func__, ret, module, status);

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
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d value:%d", __func__, ret, value);

	pic_mode_item.data[VPQ_ITEM_BRIGHTNESS] = value;
	pic_mode_item.save[VPQ_ITEM_BRIGHTNESS] = value;

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
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d value:%d", __func__, ret, value);

	pic_mode_item.data[VPQ_ITEM_CONTRAST] = value;
	pic_mode_item.save[VPQ_ITEM_CONTRAST] = value;

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
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d value:%d", __func__, ret, value);

	pic_mode_item.data[VPQ_ITEM_SATURATION] = value;
	pic_mode_item.save[VPQ_ITEM_SATURATION] = value;

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
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d value:%d", __func__, ret, value);

	pic_mode_item.data[VPQ_ITEM_HUE] = value;
	pic_mode_item.save[VPQ_ITEM_HUE] = value;

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
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d value:%d", __func__, ret, value);

	pic_mode_item.data[VPQ_ITEM_SHARPNESS] = value;
	pic_mode_item.save[VPQ_ITEM_SHARPNESS] = value;

	return ret;
}

int vpq_set_brightness_post(int value)
{
	int ret = -1;

	ret = vpp_pq_mgr_set_brightness_post(value);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d value:%d", __func__, ret, value);

	return ret;
}

int vpq_set_contrast_post(int value)
{
	int ret = -1;

	ret = vpp_pq_mgr_set_contrast_post(value);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d value:%d", __func__, ret, value);

	return ret;
}

int vpq_set_saturation_post(int value)
{
	int ret = -1;

	ret = vpp_pq_mgr_set_saturation_post(value);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d value:%d", __func__, ret, value);

	return ret;
}

int vpq_set_hue_post(int value)
{
	int ret = -1;

	ret = vpp_pq_mgr_set_hue_post(value);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d value:%d", __func__, ret, value);

	return ret;
}

int vpq_set_gamma_index(int value)
{
	int ret = -1;
	int i = 0;
	struct vpp_gamma_table_s vpp_gma_table;

	if (value == pic_mode_item.data[VPQ_ITEM_GAMMA_INDEX]) {
		VPQ_PR_INFO(PR_TABLE, "%s no need load pq to vpp\n", __func__);
		return 0;
	}

	vpp_gma_table.r_data = (unsigned int *)gamma_table.gamma_data[value].R;
	vpp_gma_table.g_data = (unsigned int *)gamma_table.gamma_data[value].G;
	vpp_gma_table.b_data = (unsigned int *)gamma_table.gamma_data[value].B;

	for (i = 0; i < VPQ_GAMMA_TABLE_LEN; i++)
		VPQ_PR_INFO(PR_TABLE, "%s vpp_gma_table.r/g/b[%d]:%d %d %d",
			__func__, i,
			vpp_gma_table.r_data[i], vpp_gma_table.g_data[i], vpp_gma_table.b_data[i]);

	ret = vpp_pq_mgr_set_gamma_table(&vpp_gma_table);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d value:%d", __func__, ret, value);

	pic_mode_item.data[VPQ_ITEM_GAMMA_INDEX] = value;
	pic_mode_item.save[VPQ_ITEM_GAMMA_INDEX] = value;

	return ret;
}

int vpq_set_gamma_table(struct vpq_gamma_table_s *pdata)
{
	int ret = -1;
	int i = 0;
	struct vpp_gamma_table_s vpp_gma_table;

	vpp_gma_table.r_data = (unsigned int *)pdata->r_data;
	vpp_gma_table.g_data = (unsigned int *)pdata->g_data;
	vpp_gma_table.b_data = (unsigned int *)pdata->b_data;
	if (!vpp_gma_table.r_data || !vpp_gma_table.g_data || !vpp_gma_table.b_data) {
		VPQ_PR_INFO(PR_TABLE, "%s malloc fail", __func__);
		return -1;
	}

	for (i = 0; i < VPQ_GAMMA_TABLE_LEN; i++)
		VPQ_PR_INFO(PR_TABLE, "%s vpp_gma_table.r/g/b[%d]:%d %d %d",
			__func__, i,
			vpp_gma_table.r_data[i], vpp_gma_table.g_data[i], vpp_gma_table.b_data[i]);

	ret = vpp_pq_mgr_set_gamma_table(&vpp_gma_table);

	kfree(vpp_gma_table.r_data);
	kfree(vpp_gma_table.g_data);
	kfree(vpp_gma_table.b_data);

	VPQ_PR_INFO(PR_TABLE, "%s ret:%d", __func__, ret);
	return ret;
}

int vpq_set_blend_gamma(struct vpq_blend_gamma_s *pdata)
{
	int ret = -1;
	int i = 0;
	struct vpq_gamma_tabel_s *gamma_r = NULL, *gamma_g = NULL, *gamma_b = NULL;
	struct vpq_tcon_gamma_table_s *wb_gamma_r = NULL, *wb_gamma_g = NULL, *wb_gamma_b = NULL;
	struct vpq_tcon_gamma_table_s blend_gamma_r, blend_gamma_g, blend_gamma_b;

	memset(&blend_gamma_r, 0, sizeof(struct vpq_tcon_gamma_table_s));
	memset(&blend_gamma_g, 0, sizeof(struct vpq_tcon_gamma_table_s));
	memset(&blend_gamma_b, 0, sizeof(struct vpq_tcon_gamma_table_s));

	VPQ_PR_INFO(PR_TABLE, "%s gamma_curve:%d ctemp_mode:%d",
		__func__, pdata->gamma_curve, pdata->ctemp_mode);
	VPQ_PR_INFO(PR_TABLE, "%s gamma:%d ctemp:%d",
		__func__,
		pic_mode_item.data[VPQ_ITEM_GAMMA_INDEX],
		pic_mode_item.data[VPQ_ITEM_CTEMP_INDEX]);

	if (pdata->gamma_curve == pic_mode_item.data[VPQ_ITEM_GAMMA_INDEX] &&
		pdata->ctemp_mode == pic_mode_item.data[VPQ_ITEM_CTEMP_INDEX]) {
		VPQ_PR_INFO(PR_TABLE, "%s no need load pq to vpp\n", __func__);
		return 0;
	}

	//gamma curve
	gamma_r = (struct vpq_gamma_tabel_s *)gamma_table.gamma_data[pdata->gamma_curve].R;
	gamma_g = (struct vpq_gamma_tabel_s *)gamma_table.gamma_data[pdata->gamma_curve].G;
	gamma_b = (struct vpq_gamma_tabel_s *)gamma_table.gamma_data[pdata->gamma_curve].B;
	if (!gamma_r || !gamma_g || !gamma_b) {
		VPQ_PR_INFO(PR_TABLE, "%s gamma_r/g/b is NULL", __func__);
		return -1;
	}

	//colortemp gamma curve
	wb_gamma_r = (struct vpq_tcon_gamma_table_s *)ctemp_table.ctemp_data[pdata->ctemp_mode].R;
	wb_gamma_g = (struct vpq_tcon_gamma_table_s *)ctemp_table.ctemp_data[pdata->ctemp_mode].G;
	wb_gamma_b = (struct vpq_tcon_gamma_table_s *)ctemp_table.ctemp_data[pdata->ctemp_mode].B;
	if (!wb_gamma_r || !wb_gamma_g || !wb_gamma_b) {
		VPQ_PR_INFO(PR_TABLE, "%s wb_gamma_r/g/b is NULL", __func__);
		return -1;
	}

	//blend gamma
	vpq_module_gamma_blend(wb_gamma_r, gamma_r, &blend_gamma_r);
	vpq_module_gamma_blend(wb_gamma_g, gamma_g, &blend_gamma_g);
	vpq_module_gamma_blend(wb_gamma_b, gamma_b, &blend_gamma_b);

	//for (i = 0; i < VPQ_GAMMA_TABLE_LEN; i++)
	//	VPQ_PR_INFO(PR_TABLE, "%s [%d] gma_r/g/b:%d %d %d
	//		wb_r/g/b:%d %d %d blend_r/g/b:%d %d %d",
	//		__func__, i,
	//		gamma_r->data[i], gamma_g->data[i], gamma_b->data[i],
	//		wb_gamma_r->data[i], wb_gamma_g->data[i], wb_gamma_b->data[i],
	//		blend_gamma_r.data[i], blend_gamma_g.data[i], blend_gamma_b.data[i]);

	for (i = 0; i < VPQ_GAMMA_TABLE_LEN; i++) {
		vpp_gma_table.r_data[i] = (unsigned int)blend_gamma_r.data[i];
		vpp_gma_table.g_data[i] = (unsigned int)blend_gamma_g.data[i];
		vpp_gma_table.b_data[i] = (unsigned int)blend_gamma_b.data[i];
	}

	ret = vpp_pq_mgr_set_gamma_table(&vpp_gma_table);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d", __func__, ret);

	pic_mode_item.data[VPQ_ITEM_GAMMA_INDEX] = pdata->gamma_curve;
	pic_mode_item.save[VPQ_ITEM_GAMMA_INDEX] = pdata->gamma_curve;
	pic_mode_item.data[VPQ_ITEM_CTEMP_INDEX] = pdata->ctemp_mode;
	pic_mode_item.save[VPQ_ITEM_CTEMP_INDEX] = pdata->ctemp_mode;

	return ret;
}

int vpq_set_pre_gamma_table(struct vpq_pre_gamma_table_s *pdata)
{
	int ret = -1;
	int i = 0;
	struct vpp_gamma_table_s pre_gam_data;

	memset(&pre_gam_data, 0, sizeof(struct vpp_gamma_table_s));

	for (i = 0; i < VPQ_PRE_GAMMA_TABLE_LEN; i++) {
		pre_gam_data.r_data[i] = (unsigned int)pdata->r_data[i];
		pre_gam_data.g_data[i] = (unsigned int)pdata->g_data[i];
		pre_gam_data.b_data[i] = (unsigned int)pdata->b_data[i];

		VPQ_PR_INFO(PR_TABLE, "%s pre_gam_data.r/g/b[%d]:0x%x 0x%x 0x%x",
			__func__, i,
			pre_gam_data.r_data[i], pre_gam_data.g_data[i], pre_gam_data.b_data[i]);
	}

	ret = vpp_pq_mgr_set_pre_gamma_table(&pre_gam_data);

	VPQ_PR_INFO(PR_TABLE, "%s ret:%d", __func__, ret);
	return ret;
}

int vpq_set_rgb_ogo(struct vpq_rgb_ogo_s *pdata)
{
	int ret = -1;
	struct vpp_white_balance_s wb_data;

	memset(&wb_data, 0, sizeof(struct vpp_white_balance_s));

	if (pdata->en == rgb_ogo.en &&
		pdata->r_pre_offset == rgb_ogo.r_pre_offset &&
		pdata->g_pre_offset == rgb_ogo.g_pre_offset &&
		pdata->b_pre_offset == rgb_ogo.b_pre_offset &&
		pdata->r_gain == rgb_ogo.r_gain &&
		pdata->g_gain == rgb_ogo.g_gain &&
		pdata->b_gain == rgb_ogo.b_gain &&
		pdata->r_post_offset == rgb_ogo.r_post_offset &&
		pdata->g_post_offset == rgb_ogo.g_post_offset &&
		pdata->b_post_offset == rgb_ogo.b_post_offset) {
		VPQ_PR_INFO(PR_TABLE, "%s same data, no need load pq to vpp\n", __func__);
		return 0;
	}

	wb_data.r_pre_offset = pdata->r_pre_offset;
	wb_data.g_pre_offset = pdata->g_pre_offset;
	wb_data.b_pre_offset = pdata->b_pre_offset;
	wb_data.r_gain = pdata->r_gain;
	wb_data.g_gain = pdata->g_gain;
	wb_data.b_gain = pdata->b_gain;
	wb_data.r_post_offset = pdata->r_post_offset;
	wb_data.g_post_offset = pdata->g_post_offset;
	wb_data.b_post_offset = pdata->b_post_offset;

	VPQ_PR_INFO(PR_TABLE, "%s wb_data:%d %d %d %d %d %d %d %d %d",
		__func__,
		wb_data.r_pre_offset, wb_data.g_pre_offset,
		wb_data.g_pre_offset, wb_data.r_gain,
		wb_data.g_gain, wb_data.b_gain,
		wb_data.r_post_offset, wb_data.g_post_offset,
		wb_data.b_post_offset);

	ret = vpp_pq_mgr_set_whitebalance(&wb_data);

	memcpy(&rgb_ogo, pdata, sizeof(struct vpq_rgb_ogo_s));
	memcpy(&rgb_ogo_save, pdata, sizeof(struct vpq_rgb_ogo_s));

	VPQ_PR_INFO(PR_TABLE, "%s ret:%d", __func__, ret);
	return ret;
}

int vpq_set_color_customize(struct vpq_cms_s *pdata)
{
	int ret = -1;
	int i = 0;
	struct color_adj_param_s adj_param;
	int satbyhs_curve[32 * TYPE_SATBYHS_CURVE_NUM];
	int huebyhue_curve[32 * TYPE_HUEBYHUE_CURVE_NUM];
	int lumabyhue_curve[32 * TYPE_LUMABYHUE_CURVE_NUM];

	memset(&adj_param, 0, sizeof(struct color_adj_param_s));

	if (!pcm_base_data) {
		VPQ_PR_INFO(PR_TABLE, "%s base table hasn't been load\n", __func__);
		return -EFAULT;
	}

	VPQ_PR_INFO(PR_TABLE, "%s pdata:%d %d %d %d %d",
		__func__,
		pdata->color_type, pdata->color_9, pdata->color_14,
		pdata->cms_type, pdata->value);

	if (pdata->color_type == cms_param.color_type &&
		pdata->color_9 == cms_param.color_9 &&
		pdata->cms_type == cms_param.cms_type &&
		pdata->value == cms_param.value) {
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
		color_adj_by_mode(adj_param, color_custom_curve[VPQ_CMS_SAT]);

		for (i = 0; i < 32; i++) {
			satbyhs_curve[i + 32 * 0] =
				pcm_base_data->satbyhs_0[i] + color_custom_curve[VPQ_CMS_SAT][i];
			satbyhs_curve[i + 32 * 1] =
				pcm_base_data->satbyhs_1[i] + color_custom_curve[VPQ_CMS_SAT][i];
			satbyhs_curve[i + 32 * 2] =
				pcm_base_data->satbyhs_2[i] + color_custom_curve[VPQ_CMS_SAT][i];

			//VPQ_PR_INFO(PR_TABLE, "%s curve_temp_sat[%d]:%d %d %d",
			//	__func__, i,
			//	satbyhs_curve[i + 32 * 0], satbyhs_curve[i + 32 * 1],
			//	satbyhs_curve[i + 32 * 2]);
		}

		ret = vpp_pq_mgr_set_cm_curve(EN_CM_SAT, satbyhs_curve);
	} else if (pdata->cms_type == VPQ_CMS_HUE) {
		adj_param.curve_type = EN_COLOR_ADJ_CURVE_HUE;
		color_adj_by_mode(adj_param, color_custom_curve[VPQ_CMS_HUE]);

		for (i = 0; i < 32; i++) {
			huebyhue_curve[i] =
				pcm_base_data->huebyhue_0[i] + color_custom_curve[VPQ_CMS_HUE][i];

			//VPQ_PR_INFO(PR_TABLE, "%s huebyhue_curve[%d]:%d",
			//	__func__, i, huebyhue_curve[i]);
		}

		ret = vpp_pq_mgr_set_cm_curve(EN_CM_HUE, huebyhue_curve);
	} else if (pdata->cms_type == VPQ_CMS_LUMA) {
		adj_param.curve_type = EN_COLOR_ADJ_CURVE_LUMA;
		color_adj_by_mode(adj_param, color_custom_curve[VPQ_CMS_LUMA]);

		for (i = 0; i < 32; i++) {
			lumabyhue_curve[i] =
				pcm_base_data->lumabyhue_0[i] + color_custom_curve[VPQ_CMS_LUMA][i];

			//VPQ_PR_INFO(PR_TABLE, "%s lumabyhue_curve[%d]:%d",
			//	__func__, i, lumabyhue_curve[i]);
		}

		ret = vpp_pq_mgr_set_cm_curve(EN_CM_LUMA, lumabyhue_curve);
	}

	VPQ_PR_INFO(PR_TABLE, "%s ret:%d", __func__, ret);

	memcpy(&cms_param, pdata, sizeof(struct vpq_cms_s));

	return ret;
}

int vpq_set_dnlp_mode(int value)
{
	int ret = -1;
	int table_index = 255;

	VPQ_PR_INFO(PR_TABLE, "%s value:%d", __func__, value);
	if (value >= DNLP_LV_MAX)
		return -1;

	//pick up dnlp table from "pq_index_table0"
	table_index = vpq_module_timing_table_index(PQ_INDEX_DNLP);
	VPQ_PR_INFO(PR_TABLE, "%s table_index:%d", __func__, table_index);
	if (table_index == 255) //means no reg table need set
		return -1;

	if (table_index == dnlp_table_index &&
		value == pic_mode_item.data[VPQ_ITEM_DYNAMIC_CONTRAST]) {
		VPQ_PR_INFO(PR_TABLE, "%s same table, no need load pq to vpp\n", __func__);
		return 0;
	}

	//"dnlp_enable" refer "pq_dnlp_param_e" PQ_DNLP_ENABLE
	if (pq_table_param.dnlp_table[table_index][value].param[PQ_DNLP_ENABLE])
		dnlp_enable = true;
	else
		dnlp_enable = false;
	ret = vpp_pq_mgr_set_module_status(EN_MODULE_BLE, dnlp_enable);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d dnlp_enable:%d", __func__, ret, dnlp_enable);

	pdnlp_data =
		(struct vpp_dnlp_curve_param_s *)&pq_table_param.dnlp_table[table_index][value];
	if (!pdnlp_data)
		return -EFAULT;

	ret = vpp_pq_mgr_set_dnlp_param(pdnlp_data);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d", __func__, ret);

	pic_mode_item.data[VPQ_ITEM_DYNAMIC_CONTRAST] = value;
	pic_mode_item.save[VPQ_ITEM_DYNAMIC_CONTRAST] = value;
	dnlp_table_index = table_index;

	return ret;
}

int vpq_set_csc_type(int value)
{
	int ret = -1;

	ret = vpp_pq_mgr_set_csc_type(value);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d value:%d", __func__, ret, value);

	return ret;
}

int vpq_get_csc_type(void)
{
	int ret = -1;

	ret = vpp_pq_mgr_get_csc_type();
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d", __func__, ret);

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

	VPQ_PR_INFO(PR_TABLE, "%s value:%d", __func__, value);
	if (value >= LC_LV_MAX)
		return -1;

	//pick up lc curve and reg table from "pq_index_table0"
	table_index = vpq_module_timing_table_index(PQ_INDEX_LC);
	VPQ_PR_INFO(PR_TABLE, "%s table_index:%d", __func__, table_index);
	if (table_index == 255) //means no reg table need set
		return -1;

	if (table_index == lcd_table_index &&
		value == pic_mode_item.data[VPQ_ITEM_LOCAL_CONTRAST]) {
		VPQ_PR_INFO(PR_TABLE, "%s same table, no need load pq to vpp\n", __func__);
		return 0;
	}

	//pq_table_param.lc_node_table.param seems not used by vpp
	memcpy(&lc_curve, &pq_table_param.lc_node_table[table_index][value],
			sizeof(struct vpp_lc_curve_s));

	ret = vpp_pq_mgr_set_lc_curve(&lc_curve);
	VPQ_PR_INFO(PR_TABLE, "%s set_lc_curve ret:%d", __func__, ret);

	//"lc_enable" refer "pq_lc_reg_parm_s" data[0]
	if (pq_table_param.lc_reg_table[table_index][value].lc_enable)
		lc_enable = true;
	else
		lc_enable = false;
	ret = vpp_pq_mgr_set_module_status(EN_MODULE_LC, lc_enable);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d lc_enable:%d", __func__, ret, lc_enable);

	memcpy(&lc_param,
		&pq_table_param.lc_reg_table[table_index][value].lc_curve_nodes_vlpf,
		sizeof(struct vpp_lc_param_s));

	ret |= vpp_pq_mgr_set_lc_param(&lc_param);
	VPQ_PR_INFO(PR_TABLE, "%s set_lc_param ret:%d", __func__, ret);

	pic_mode_item.data[VPQ_ITEM_LOCAL_CONTRAST] = value;
	pic_mode_item.save[VPQ_ITEM_LOCAL_CONTRAST] = value;
	lcd_table_index = table_index;

	return ret;
}

int vpq_set_hdr_tmo_mode(int value)
{
	int ret = -1;
	int table_index = 255;
	struct vpp_tmo_param_s tmo_param;

	memset(&tmo_param, 0, sizeof(struct vpp_tmo_param_s));

	VPQ_PR_INFO(PR_TABLE, "%s value:%d", __func__, value);
	if (value >= HDR_TMO_LV_MAX)
		return -1;

	table_index = vpq_module_timing_table_index(PQ_INDEX_HDR_TMO);
	VPQ_PR_INFO(PR_TABLE, "%s table_index:%d", __func__, table_index);
	if (table_index == 255) //means no reg table need set
		return -1;

	if (table_index == tmo_table_index &&
		value == pic_mode_item.data[VPQ_ITEM_TMO]) {
		VPQ_PR_INFO(PR_TABLE, "%s same table, no need load pq to vpp\n", __func__);
		return 0;
	}

	memcpy(&tmo_param, &pq_table_param.hdr_tmo_table[table_index][value],
			sizeof(struct vpp_tmo_param_s));

	//VPQ_PR_INFO(PR_TABLE, "%s %d %d %d %d %d %d %d %d %d %d",__func__,
	//			tmo_param.tmo_en, tmo_param.reg_highlight,
	//			tmo_param.reg_hist_th, tmo_param.reg_light_th,
	//			tmo_param.reg_highlight_th1, tmo_param.reg_highlight_th2,
	//			tmo_param.reg_display_e, tmo_param.reg_middle_a,
	//			tmo_param.reg_middle_a_adj, tmo_param.reg_middle_b);
	//VPQ_PR_INFO(PR_TABLE, "%s %d %d %d %d %d %d %d %d %d %d", __func__,
	//			tmo_param.reg_middle_s, tmo_param.reg_max_th1,
	//			tmo_param.reg_middle_th, tmo_param.reg_thold1,
	//			tmo_param.reg_thold2, tmo_param.reg_thold3,
	//			tmo_param.reg_thold4, tmo_param.reg_max_th2,
	//			tmo_param.reg_pnum_th, tmo_param.reg_hl0);
	//VPQ_PR_INFO(PR_TABLE, "%s %d %d %d %d %d %d %d %d %d %d", __func__,
	//			tmo_param.reg_hl1, tmo_param.reg_hl2,
	//			tmo_param.reg_hl3, tmo_param.reg_display_adj,
	//			tmo_param.reg_avg_th, tmo_param.reg_avg_adj,
	//			tmo_param.reg_low_adj, tmo_param.reg_high_en,
	//			tmo_param.reg_high_adj1, tmo_param.reg_high_adj2);
	//VPQ_PR_INFO(PR_TABLE, "%s %d %d %d", __func__,
	//			tmo_param.reg_high_maxdiff, tmo_param.reg_high_mindiff,
	//			tmo_param.alpha);

	ret = vpp_pq_mgr_set_hdr_tmo_param(&tmo_param);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d", __func__, ret);

	pic_mode_item.data[VPQ_ITEM_TMO] = value;
	pic_mode_item.save[VPQ_ITEM_TMO] = value;
	tmo_table_index = table_index;

	return ret;
}

int vpq_set_hdr_tmo_data(struct vpq_hdr_lut_s *pdata)
{
	int ret = -1;

	if (!pdata)
		return ret;
	ret = vpp_pq_mgr_set_hdr_tmo_curve((struct vpp_hdr_lut_s *)pdata);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d", __func__, ret);

	return ret;
}

int vpq_set_aipq_mode(int value)
{
	int ret = -1;
	int i = 0;
	int table_index = 255;

	VPQ_PR_INFO(PR_TABLE, "%s value:%d", __func__, value);

	table_index = vpq_module_timing_table_index(PQ_INDEX_AIPQ);
	VPQ_PR_INFO(PR_TABLE, "%s table_index:%d", __func__, table_index);
	if (table_index == 255) //means no reg table need set
		return -1;

	if (table_index == aipq_table_index) {
		VPQ_PR_INFO(PR_TABLE, "%s same data, no need load pq to vpp\n", __func__);
		return 0;
	}

	aipq_table.height = pq_table_param.aipq_table[table_index].ai_size.height;
	aipq_table.width = pq_table_param.aipq_table[table_index].ai_size.width;
	aipq_table.table_ptr = (void *)pq_table_param.aipq_table[table_index].ai_pq_table;
	if (!aipq_table.table_ptr) {
		VPQ_PR_INFO(PR_TABLE, "%s malloc buf failed", __func__);
		return -EFAULT;
	}

	for (i = 0; i < aipq_table.height * aipq_table.width; i++)
		VPQ_PR_INFO(PR_TABLE, "%s table_ptr[%d]:0x%x",
			__func__, i, ((unsigned int *)aipq_table.table_ptr)[i]);

	ret = vpp_pq_mgr_set_aipq_data(&aipq_table);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d", __func__, ret);

	pic_mode_item.data[VPQ_ITEM_AIPQ] = value;
	pic_mode_item.save[VPQ_ITEM_AIPQ] = value;
	aipq_table_index = table_index;

	return ret;
}

int vpq_set_aisr_mode(int value)
{
	int ret = -1;
	int table_index = 255;

	VPQ_PR_INFO(PR_TABLE, "%s value:%d", __func__, value);

	table_index = vpq_module_timing_table_index(PQ_INDEX_AISR);
	VPQ_PR_INFO(PR_TABLE, "%s table_index:%d", __func__, table_index);
	if (table_index == 255) //means no reg table need set
		return -1;

	if (value == pic_mode_item.data[VPQ_ITEM_AISR] &&
		table_index == aisr_table_index) {
		VPQ_PR_INFO(PR_TABLE, "%s same data, no need load pq to vpp\n", __func__);
		return 0;
	}

	memcpy(&aisr_parm,
		&pq_table_param.ai_sr_table[table_index][AISR_TYPE_X4][value].aisr_pram,
		sizeof(struct vpp_aisr_param_s));
	memcpy(&aisr_nn_parm,
		&pq_table_param.ai_sr_table[table_index][AISR_TYPE_X4][value].aisr_nn_parm,
		sizeof(struct vpp_aisr_nn_param_s));

	ret = vpp_pq_mgr_set_aisr_param(&aisr_parm, &aisr_nn_parm);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d", __func__, ret);

	pic_mode_item.data[VPQ_ITEM_AISR] = value;
	pic_mode_item.save[VPQ_ITEM_AISR] = value;
	aisr_table_index = table_index;

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

	VPQ_PR_INFO(PR_TABLE, "%s value:%d", __func__, value);
	if (value >= BLKEXT_LV_MAX)
		return -1;

	//pick up blkext reg table from "pq_index_table0"
	table_index = vpq_module_timing_table_index(PQ_INDEX_BLACKEXT);
	VPQ_PR_INFO(PR_TABLE, "%s table_index:%d", __func__, table_index);
	if (table_index == 255) //means no reg table need set
		return -1;

	if (table_index == blkext_table_index &&
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
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d blkext_en:%d", __func__, ret, blkext_en);

	//"blackext_start"~"blackext_slope2" refer "pq_blkext_parm_s" data[1]~data[4]
	pble_data = (int *)&pq_table_param.blkext_table[table_index][value].blackext_start;
	VPQ_PR_INFO(PR_TABLE, "%s pble_data: %d %d %d %d\n",
		__func__, pble_data[0], pble_data[1], pble_data[2], pble_data[3]);

	vpp_pq_mgr_set_blkext_params(pble_data, length);

	pic_mode_item.data[VPQ_ITEM_BLACK_STRETCH] = value;
	pic_mode_item.save[VPQ_ITEM_BLACK_STRETCH] = value;
	blkext_table_index = table_index;

	return 0;
}

int vpq_set_blue_stretch(int value)
{
	int ret = -1;
	int table_index = 255;
	int i = 0;
	int length = 18; //"blue_str_table" data[1]~data[18]

	VPQ_PR_INFO(PR_TABLE, "%s value:%d", __func__, value);
	if (value >= BLUESTR_LV_MAX)
		return -1;

	//pick up blue_stretch reg table from "pq_index_table0"
	table_index = vpq_module_timing_table_index(PQ_INDEX_BLUE_STRETCH);
	VPQ_PR_INFO(PR_TABLE, "%s table_index:%d", __func__, table_index);
	if (table_index == 255) //means no reg table need set
		return -1;

	if (table_index == bluestr_table_index &&
		value == pic_mode_item.data[VPQ_ITEM_BLUE_STRETCH]) {
		VPQ_PR_INFO(PR_TABLE, "%s same table, no need load pq to vpp\n", __func__);
		return 0;
	}

	//"bluestretch_en_sel"~"bluestretch_error_cbn_inv" refer "blue_str_table" data[1]~data[18]
	pbls_data = (int *)&pq_table_param.blue_str_table[table_index][value].bluestretch_en_sel;

	for (i = 0; i < 18; i++)
		VPQ_PR_INFO(PR_TABLE, "%s pbls_data[%d]:%d\n", __func__, i, pbls_data[i]);

	vpp_pq_mgr_set_blue_stretch_params(pbls_data, length);

	//"bluestretch_enable" refer "blue_str_table" data[0]
	if (pq_table_param.blue_str_table[table_index][value].bluestretch_enable)
		blue_str_en = true;
	else
		blue_str_en = false;
	ret = vpp_pq_mgr_set_module_status(EN_MODULE_BLS, blue_str_en);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d blue_str_en:%d", __func__, ret, blue_str_en);

	bluestr_table_index = table_index;
	pic_mode_item.data[VPQ_ITEM_BLUE_STRETCH] = value;
	pic_mode_item.save[VPQ_ITEM_BLUE_STRETCH] = value;

	return 0;
}

int vpq_set_chroma_coring(int value)
{
	int ret = -1;
	int table_index = 255;
	bool ccoring_en = false;
	unsigned int *pdata = NULL;
	int length = 3; //"ccoring_table" data[1]~data[3]

	VPQ_PR_INFO(PR_TABLE, "%s value:%d", __func__, value);
	if (value >= CCORING_LV_MAX)
		return -1;

	//pick up chroma_coring reg table from "pq_index_table0"
	table_index = vpq_module_timing_table_index(PQ_INDEX_CHROMA_CORING);
	VPQ_PR_INFO(PR_TABLE, "%s table_index:%d", __func__, table_index);
	if (table_index == 255) //means no reg table need set
		return -1;

	if (table_index == ccoring_table_index &&
		value == pic_mode_item.data[VPQ_ITEM_CHROMA_CORING]) {
		VPQ_PR_INFO(PR_TABLE, "%s same table, no need load pq to vpp\n", __func__);
		return 0;
	}

	//"chroma_coring_enable" refer "ccoring_table" data[0]
	if (pq_table_param.ccoring_table[table_index][value].chroma_coring_enable)
		ccoring_en = true;
	ret = vpp_pq_mgr_set_module_status(EN_MODULE_CCORING, ccoring_en);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d ccoring_en:%d", __func__, ret, ccoring_en);

	//"chroma_coring_y_threshold"~"chroma_coring_slope" refer "ccoring_table" data[1]~data[3]
	pdata =
	 (unsigned int *)&pq_table_param.ccoring_table[table_index][value].chroma_coring_slope;

	//VPQ_PR_INFO(PR_TABLE, "%s pdata: %d %d %d\n", __func__,
	//			pdata[0], pdata[1], pdata[2]);

	vpp_pq_mgr_set_ccoring_params(pdata, length);

	//"center_bar_enable" ~ "left_top_scrn_width" no need we control

	pic_mode_item.data[VPQ_ITEM_CHROMA_CORING] = value;
	pic_mode_item.save[VPQ_ITEM_CHROMA_CORING] = value;
	ccoring_table_index = table_index;

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

int vpq_set_color_base(int value)
{
	int ret = -1;
	int i = 0;
	int table_index = 255;

	VPQ_PR_INFO(PR_TABLE, "%s value:%d", __func__, value);
	if (value >= CM_LV_MAX)
		return -1;

	//pick up color base table from "cm_base_table"
	table_index = vpq_module_timing_table_index(PQ_INDEX_CM2);
	VPQ_PR_INFO(PR_TABLE, "%s table_index:%d", __func__, table_index);
	if (table_index == 255) //means no reg table need set
		return -1;

	if (table_index == color_base_table_index &&
		value == pic_mode_item.data[VPQ_ITEM_COLOR_BASE]) {
		VPQ_PR_INFO(PR_TABLE, "%s same table, no need load pq to vpp\n", __func__);
		return 0;
	}

	pcm_base_data = &pq_table_param.cm_base_table[table_index][value];

	//LumaByHue
	ret = vpp_pq_mgr_set_cm_curve(EN_CM_LUMA, pcm_base_data->lumabyhue_0);

	//SatByHS
	for (i = 0; i < 32; i++) {
		satbyhs_table[i + 32 * 0] = pcm_base_data->satbyhs_0[i];
		satbyhs_table[i + 32 * 1] = pcm_base_data->satbyhs_1[i];
		satbyhs_table[i + 32 * 2] = pcm_base_data->satbyhs_2[i];
	}
	ret |= vpp_pq_mgr_set_cm_curve(EN_CM_SAT, satbyhs_table);

	//HueByHue
	ret |= vpp_pq_mgr_set_cm_curve(EN_CM_HUE, pcm_base_data->huebyhue_0);

	//HueByHS
	for (i = 0; i < 32; i++) {
		huebyhs_table[i + 32 * 0] = pcm_base_data->huebyhs_0[i];
		huebyhs_table[i + 32 * 1] = pcm_base_data->huebyhs_1[i];
		huebyhs_table[i + 32 * 2] = pcm_base_data->huebyhs_2[i];
		huebyhs_table[i + 32 * 3] = pcm_base_data->huebyhs_3[i];
		huebyhs_table[i + 32 * 4] = pcm_base_data->huebyhs_4[i];
	}
	ret |= vpp_pq_mgr_set_cm_curve(EN_CM_HUE_BY_HS, huebyhs_table);

	//SatByY
	ret |= vpp_pq_mgr_set_cm_curve(EN_CM_SAT_BY_L, pcm_base_data->satbyy_0);

	//SatByHY
	for (i = 0; i < 32; i++) {
		satbyhy_table[i + 32 * 0] = pcm_base_data->satbyhy_0[i];
		satbyhy_table[i + 32 * 1] = pcm_base_data->satbyhy_1[i];
		satbyhy_table[i + 32 * 2] = pcm_base_data->satbyhy_2[i];
		satbyhy_table[i + 32 * 3] = pcm_base_data->satbyhy_3[i];
		satbyhy_table[i + 32 * 4] = pcm_base_data->satbyhy_4[i];
	}
	ret |= vpp_pq_mgr_set_cm_curve(EN_CM_SAT_BY_HL, satbyhy_table);

	//HueByHY
	for (i = 0; i < 32; i++) {
		huebyhy_table[i + 32 * 0] = pcm_base_data->huebyhy_0[i];
		huebyhy_table[i + 32 * 1] = pcm_base_data->huebyhy_1[i];
		huebyhy_table[i + 32 * 2] = pcm_base_data->huebyhy_2[i];
		huebyhy_table[i + 32 * 3] = pcm_base_data->huebyhy_3[i];
		huebyhy_table[i + 32 * 4] = pcm_base_data->huebyhy_4[i];
	}
	ret |= vpp_pq_mgr_set_cm_curve(EN_CM_HUE_BY_HL, huebyhy_table);

	VPQ_PR_INFO(PR_TABLE, "%s ret:%d", __func__, ret);

	pic_mode_item.data[VPQ_ITEM_COLOR_BASE] = value;
	pic_mode_item.save[VPQ_ITEM_COLOR_BASE] = value;
	color_base_table_index = table_index;

	return ret;
}

int vpq_set_cabc(void)
{
	int ret = -1;
	struct vpp_cabc_param_s cabc_param;

	memset(&cabc_param, 0, sizeof(struct vpp_cabc_param_s));

	ret = vpp_pq_mgr_set_cabc_param(&cabc_param);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d", __func__, ret);

	return ret;
}

int vpq_set_add(void)
{
	int ret = -1;
	struct vpp_aad_param_s add_param;

	memset(&add_param, 0, sizeof(struct vpp_aad_param_s));

	ret = vpp_pq_mgr_set_aad_param(&add_param);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d", __func__, ret);

	return ret;
}

int vpq_set_overscan_data(unsigned int length, struct vpq_overscan_data_s *pdata)
{
	int ret = -1;

	if (!pdata)
		return -1;

	ret = vpp_pq_mgr_set_overscan_table(length, (struct vpp_overscan_table_s *)pdata);
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d", __func__, ret);

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
	VPQ_PR_INFO(PR_TABLE, "%s ret:%d value:%d", __func__, ret, value);

	return ret;
}

void vpq_set_pq_effect_all(void)
{
	VPQ_PR_DRV("%s start\n", __func__);

	enum vpq_resolution_e reso;
	struct vpq_blend_gamma_s blend_gma;

	reso = vpq_get_resolution();

	//load ui case
	vpq_set_brightness(pic_mode_item.save[VPQ_ITEM_BRIGHTNESS]);
	vpq_set_contrast(pic_mode_item.save[VPQ_ITEM_CONTRAST]);
	vpq_set_saturation(pic_mode_item.save[VPQ_ITEM_SATURATION]);
	vpq_set_hue(pic_mode_item.save[VPQ_ITEM_HUE]);
	vpq_set_sharpness(pic_mode_item.save[VPQ_ITEM_SHARPNESS]);
	vpq_set_dnlp_mode(pic_mode_item.save[VPQ_ITEM_DYNAMIC_CONTRAST]);
	vpq_set_lc_mode(pic_mode_item.save[VPQ_ITEM_LOCAL_CONTRAST]);
	vpq_set_black_stretch(pic_mode_item.save[VPQ_ITEM_BLACK_STRETCH]);
	vpq_set_blue_stretch(pic_mode_item.save[VPQ_ITEM_BLUE_STRETCH]);
	vpq_set_chroma_coring(pic_mode_item.save[VPQ_ITEM_CHROMA_CORING]);
	vpq_set_nr(pic_mode_item.save[VPQ_ITEM_NR]);
	vpq_set_deblock(pic_mode_item.save[VPQ_ITEM_DEBLOCK]);
	vpq_set_demosquito(pic_mode_item.save[VPQ_ITEM_DEMOSQUITO]);

	//todo AML_HAL_AMDOLBY_SetPQMode
	//todo AML_HAL_AMDOLBY_SetDarkDetail
	//todo Cpq_SetMemcMode
	//todo SetDynamicBacklight

	blend_gma.gamma_curve = pic_mode_item.save[VPQ_ITEM_GAMMA_INDEX];
	blend_gma.ctemp_mode = pic_mode_item.save[VPQ_ITEM_CTEMP_INDEX];
	vpq_set_blend_gamma(&blend_gma);

	vpq_set_rgb_ogo(&rgb_ogo_save);

	//load pqtable case
	//vpq_set_gamma_index(pic_mode_item.save[VPQ_ITEM_GAMMA_INDEX]);
	vpq_set_mcdi_mode(pic_mode_item.save[VPQ_ITEM_MCDI]);
	//todo SetDisplayMode
	vpq_set_hdr_tmo_mode(pic_mode_item.save[VPQ_ITEM_TMO]);
	vpq_set_smoothplus_mode(pic_mode_item.save[VPQ_ITEM_SMOOTHPLUS]);
	//todo enableAipq
	//todo Cpq_SetAiSrEnable
	//todo AML_HAL_LD_SetLevelIdx
	vpq_set_color_base(pic_mode_item.save[VPQ_ITEM_COLOR_BASE]);

	vpq_set_aipq_mode(pic_mode_item.save[VPQ_ITEM_AIPQ]);
	vpq_set_aisr_mode(pic_mode_item.save[VPQ_ITEM_AISR]);

	VPQ_PR_DRV("%s end\n", __func__);
}

int vpq_set_frame_status(enum vpq_frame_status_e status)
{
	if (RELOAD_PQ_FOR_SAME_TIMING && status == VPQ_VFRAME_STOP) {
		VPQ_PR_DRV("%s reset local memory\n", __func__);
		int i = 0;

		for (i = 0; i < VPQ_ITEM_MAX; i++)
			pic_mode_item.data[i] = 0xff;

		memset(&rgb_ogo, 0, sizeof(struct vpq_rgb_ogo_s));

		dnlp_table_index = 0xff;
		lcd_table_index = 0xff;
		tmo_table_index = 0xff;
		aipq_table_index = 0xff;
		aisr_table_index = 0xff;
		ccoring_table_index = 0xff;
		blkext_table_index = 0xff;
		bluestr_table_index = 0xff;
		color_base_table_index = 0xff;
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
	enum vpq_vfm_source_type_e src_type;
	enum tvin_port_e src_port;
	enum vpq_vfm_source_mode_e src_mode;
	enum vpq_vfm_hdr_type_e hdr_type;
	unsigned int height;
	unsigned int width;

	src_type = vpq_vfm_get_source_type();
	VPQ_PR_INFO(PR_TABLE, "%s src_type:%d\n", __func__, src_type);
	switch (src_type) {
	default:
	case VPQ_SOURCE_TYPE_OTHERS:
		pdata->src_type = VPQ_SRC_TYPE_MPEG;
		break;
	case VPQ_SOURCE_TYPE_TUNER:
		pdata->src_type = VPQ_SRC_TYPE_ATV;
		break;
	case VPQ_SOURCE_TYPE_CVBS:
		pdata->src_type = VPQ_SRC_TYPE_CVBS;
		break;
	case VPQ_SOURCE_TYPE_HDMI:
		pdata->src_type = VPQ_SRC_TYPE_HDMI;
		break;
	}

	src_port = vpq_vfm_get_source_port();
	VPQ_PR_INFO(PR_TABLE, "%s src_port:0x%x\n", __func__, src_port);
	switch (src_port) {
	case VPQ_PORT_HDMI0:
		pdata->hdmi_port = VPQ_HDMI_PORT_0;
		break;
	case VPQ_PORT_HDMI1:
		pdata->hdmi_port = VPQ_HDMI_PORT_1;
		break;
	case VPQ_PORT_HDMI2:
		pdata->hdmi_port = VPQ_HDMI_PORT_2;
		break;
	case VPQ_PORT_HDMI3:
		pdata->hdmi_port = VPQ_HDMI_PORT_3;
		break;
	default:
		pdata->hdmi_port = VPQ_HDMI_PORT_NULL;
		break;
	}

	src_mode = vpq_vfm_get_source_mode();
	VPQ_PR_INFO(PR_TABLE, "%s src_mode:%d\n", __func__, src_mode);
	switch (src_mode) {
	default:
	case VPQ_SOURCE_MODE_OTHERS:
		pdata->sig_mode = VPQ_SIG_MODE_OTHERS;
		break;
	case VPQ_SOURCE_MODE_PAL:
		pdata->sig_mode = VPQ_SIG_MODE_PAL;
		break;
	case VPQ_SOURCE_MODE_NTSC:
		pdata->sig_mode = VPQ_SIG_MODE_NTSC;
		break;
	case VPQ_SOURCE_MODE_SECAM:
		pdata->sig_mode = VPQ_SIG_MODE_SECAM;
		break;
	}

	hdr_type = vpq_vfm_get_hdr_type();
	VPQ_PR_INFO(PR_TABLE, "%s hdr_type:%d\n", __func__, hdr_type);
	switch (hdr_type) {
	default:
	case VPQ_HDR_TYPE_NONE:
		pdata->hdr_type = VPQ_HDRTYPE_NONE;
		break;
	case VPQ_HDR_TYPE_SDR:
		pdata->hdr_type = VPQ_HDRTYPE_SDR;
		break;
	case VPQ_HDR_TYPE_HDR10:
		pdata->hdr_type = VPQ_HDRTYPE_HDR10;
		break;
	case VPQ_HDR_TYPE_HLG:
		pdata->hdr_type = VPQ_HDRTYPE_HLG;
		break;
	case VPQ_HDR_TYPE_HDR10PLUS:
		pdata->hdr_type = VPQ_HDRTYPE_HDR10PLUS;
		break;
	case VPQ_HDR_TYPE_DOBVI:
		pdata->hdr_type = VPQ_HDRTYPE_DOBVI;
		break;
	case VPQ_HDR_TYPE_MVC:
		pdata->hdr_type = VPQ_HDRTYPE_MVC;
		break;
	case VPQ_HDR_TYPE_CUVA_HDR:
		pdata->hdr_type = VPQ_HDRTYPE_CUVA_HDR;
		break;
	case VPQ_HDR_TYPE_CUVA_HLG:
		pdata->hdr_type = VPQ_HDRTYPE_CUVA_HLG;
		break;
	}

	vpq_frm_get_height_width(&height, &width);
	VPQ_PR_INFO(PR_TABLE, "%s height:%d width:%d\n", __func__, height, width);
	pdata->height = height;
	pdata->width = width;
}

int vpq_dump_pq_table(enum vpq_dump_type_e value)
{
	return vpq_module_dump_pq_table(value);
}
