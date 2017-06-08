/*
 * Amlogic G9TV
 * HDMI RX
 * Copyright (C) 2014 Amlogic, Inc.
 *
 * hdmi data island packet de-code
 *
 * GNU General Public License for more details.
 *
 */
#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/amlogic/tvin/tvin.h>

/* Local include */
#include "hdmirx_repeater.h"
#include "hdmi_rx_pktinfo.h"
#include "hdmirx_drv.h"
#include "hdmi_rx_reg.h"

/*this parameter moved from wrapper.c */
bool hdr_enable = true;
MODULE_PARM_DESC(hdr_enable, "\n hdr_enable\n");
module_param(hdr_enable, bool, 0664);

static struct rxpkt_st rxpktsts;

static struct pkt_typeregmap_st pktmaping[] = {
	/*infoframe pkt*/
	{PKT_TYPE_INFOFRAME_VSI,	PFIFO_VS_EN},
	{PKT_TYPE_INFOFRAME_AVI,	PFIFO_AVI_EN},
	{PKT_TYPE_INFOFRAME_SPD,	PFIFO_SPD_EN},
	{PKT_TYPE_INFOFRAME_AUD,	PFIFO_AUD_EN},
	{PKT_TYPE_INFOFRAME_MPEGSRC,	PFIFO_MPEGS_EN},
	{PKT_TYPE_INFOFRAME_NVBI,	PFIFO_NTSCVBI_EN},
	{PKT_TYPE_INFOFRAME_DRM,	PFIFO_DRM_EN},
	/*other pkt*/
	{PKT_TYPE_ACR,				PFIFO_ACR_EN},
	{PKT_TYPE_GCP,				PFIFO_GCP_EN},
	{PKT_TYPE_ACP,				PFIFO_ACP_EN},
	{PKT_TYPE_ISRC1,			PFIFO_ISRC1_EN},
	{PKT_TYPE_ISRC2,			PFIFO_ISRC2_EN},
	{PKT_TYPE_GAMUT_META,		PFIFO_GMT_EN},
	{PKT_TYPE_AUD_META,			PFIFO_AMP_EN},
	/*end of the table*/
	{K_FLAG_TAB_END,			K_FLAG_TAB_END},
};


/*
uint32_t timerbuff[50];
uint32_t timerbuff_idx = 0;
*/

/*struct mutex pktbuff_lock;*/

void rx_pkt_status(void)
{
	/*uint32_t i;*/

	rx_pr("packet_fifo_cfg=0x%x\n", packet_fifo_cfg);
	rx_pr("pdec_ists_en=0x%x\n\n", pdec_ists_en);

	rx_pr("fifo_Int_cnt=%d\n", rxpktsts.fifo_Int_cnt);
	rx_pr("fifo_pkt_num=%d\n", rxpktsts.fifo_pkt_num);

	rx_pr("pkt_cnt_avi=%d\n", rxpktsts.pkt_cnt_avi);
	rx_pr("pkt_cnt_vsi=%d\n", rxpktsts.pkt_cnt_vsi);
	rx_pr("pkt_cnt_drm=%d\n", rxpktsts.pkt_cnt_drm);
	rx_pr("pkt_cnt_spd=%d\n", rxpktsts.pkt_cnt_spd);
	rx_pr("pkt_cnt_audif=%d\n", rxpktsts.pkt_cnt_audif);
	rx_pr("pkt_cnt_mpeg=%d\n", rxpktsts.pkt_cnt_mpeg);
	rx_pr("pkt_cnt_nvbi=%d\n", rxpktsts.pkt_cnt_nvbi);

	rx_pr("pkt_cnt_acr=%d\n", rxpktsts.pkt_cnt_acr);
	rx_pr("pkt_cnt_gcp=%d\n", rxpktsts.pkt_cnt_gcp);
	rx_pr("pkt_cnt_acp=%d\n", rxpktsts.pkt_cnt_acp);
	rx_pr("pkt_cnt_isrc1=%d\n", rxpktsts.pkt_cnt_isrc1);
	rx_pr("pkt_cnt_isrc2=%d\n", rxpktsts.pkt_cnt_isrc2);
	rx_pr("pkt_cnt_gameta=%d\n", rxpktsts.pkt_cnt_gameta);
	rx_pr("pkt_cnt_amp=%d\n", rxpktsts.pkt_cnt_amp);

	rx_pr("FIFO_STS=0x%x(b31-b16/b15-b0)\n",
			hdmirx_rd_dwc(DWC_PDEC_FIFO_STS));
	rx_pr("FIFO_STS1=0x%x(b15-b0)\n",
			hdmirx_rd_dwc(DWC_PDEC_FIFO_STS1));
	/*
	for (i = 0; i < 50; i++)
		rx_pr("t%d:%d\n", i, timerbuff[i]);
	*/
}

void rx_pkt_debug(void)
{
	rx_pr("\npktinfo size=%d\n", sizeof(union pktinfo));
	rx_pr("infoframe_st size=%d\n", sizeof(union infoframe_u));
	rx_pr("fifo_rawdata_st size=%d\n", sizeof(struct fifo_rawdata_st));
	rx_pr("vsi_infoframe_st size=%d\n", sizeof(struct vsi_infoframe_st));
	rx_pr("avi_infoframe_st size=%d\n", sizeof(struct avi_infoframe_st));
	rx_pr("spd_infoframe_st size=%d\n", sizeof(struct spd_infoframe_st));
	rx_pr("aud_infoframe_st size=%d\n", sizeof(struct aud_infoframe_st));
	rx_pr("ms_infoframe_st size=%d\n", sizeof(struct ms_infoframe_st));
	rx_pr("vbi_infoframe_st size=%d\n", sizeof(struct vbi_infoframe_st));
	rx_pr("drm_infoframe_st size=%d\n", sizeof(struct drm_infoframe_st));

	rx_pr("acr_ptk_st size=%d\n",
				sizeof(struct acr_ptk_st));
	rx_pr("aud_sample_pkt_st size=%d\n",
				sizeof(struct aud_sample_pkt_st));
	rx_pr("gcp_pkt_st size=%d\n", sizeof(struct gcp_pkt_st));
	rx_pr("acp_pkt_st size=%d\n", sizeof(struct acp_pkt_st));
	rx_pr("isrc_pkt_st size=%d\n", sizeof(struct isrc_pkt_st));
	rx_pr("onebit_aud_pkt_st size=%d\n",
				sizeof(struct obasmp_pkt_st));
	rx_pr("dst_aud_pkt_st size=%d\n",
				sizeof(struct dstaud_pkt_st));
	rx_pr("hbr_aud_pkt_st size=%d\n",
				sizeof(struct hbraud_pkt_st));
	rx_pr("gamut_Meta_pkt_st size=%d\n",
				sizeof(struct gamutmeta_pkt_st));
	rx_pr("aud_3d_smppkt_st size=%d\n",
				sizeof(struct a3dsmp_pkt_st));
	rx_pr("oneb3d_smppkt_st size=%d\n",
				sizeof(struct ob3d_smppkt_st));
	rx_pr("aud_mtdata_pkt_st size=%d\n",
				sizeof(struct audmtdata_pkt_st));
	rx_pr("mulstr_audsamp_pkt_st size=%d\n",
				sizeof(struct msaudsmp_pkt_st));
	rx_pr("onebmtstr_smaud_pkt_st size=%d\n",
				sizeof(struct obmaudsmp_pkt_st));

	memset(&rxpktsts, 0, sizeof(struct rxpkt_st));

	rx_pkt_enable(rx_pkt_typemapping(PKT_TYPE_INFOFRAME_VSI));
	rx_pkt_enable(rx_pkt_typemapping(PKT_TYPE_INFOFRAME_AVI));
	rx_pkt_enable(rx_pkt_typemapping(PKT_TYPE_INFOFRAME_SPD));
	rx_pkt_enable(rx_pkt_typemapping(PKT_TYPE_INFOFRAME_AUD));
	rx_pkt_enable(rx_pkt_typemapping(PKT_TYPE_INFOFRAME_MPEGSRC));
	rx_pkt_enable(rx_pkt_typemapping(PKT_TYPE_INFOFRAME_NVBI));
	rx_pkt_enable(rx_pkt_typemapping(PKT_TYPE_INFOFRAME_DRM));

	rx_pkt_enable(rx_pkt_typemapping(PKT_TYPE_ACR));
	rx_pkt_enable(rx_pkt_typemapping(PKT_TYPE_GCP));
	/*
	rx_pkt_enable(rx_pkt_typemapping(PKT_TYPE_ACP));
	rx_pkt_enable(rx_pkt_typemapping(PKT_TYPE_ISRC1));
	rx_pkt_enable(rx_pkt_typemapping(PKT_TYPE_ISRC2));
	rx_pkt_enable(rx_pkt_typemapping(PKT_TYPE_GAMUT_META));
	rx_pkt_enable(rx_pkt_typemapping(PKT_TYPE_AUD_META));
	*/
	rx_pr("pdec_ists_en=0x%x\n\n", pdec_ists_en);
}

static void rx_pktdump_raw(void *pdata)
{
	uint8_t i;
	union infoframe_u *pktdata = pdata;
	rx_pr(">---raw data detail---------------->\n");
	rx_pr("HB0:0x%x\n", pktdata->raw_infoframe.pkttype);
	rx_pr("HB1:0x%x\n", pktdata->raw_infoframe.version);
	rx_pr("HB2:0x%x\n", pktdata->raw_infoframe.length);
	rx_pr("RSD:0x%x\n", pktdata->raw_infoframe.rsd);

	for (i = 0; i < 28; i++)
		rx_pr("PB%d:0x%x\n", i, pktdata->raw_infoframe.PB[i]);
	rx_pr(">------------------>end\n");
}

static void rx_pktdump_vsi(void *pdata)
{
	struct vsi_infoframe_st *pktdata = pdata;
	uint32_t val;
	uint32_t i;

	rx_pr(">---vsi infoframe detail -------->\n");
	rx_pr("type: 0x%x\n", pktdata->pkttype);
	rx_pr("ver: %d\n", pktdata->ver_st.version);
	rx_pr("chgbit: %d\n", pktdata->ver_st.chgbit);
	rx_pr("length: %d\n", pktdata->length);

	val = (pktdata->ieee_h << 16) |
			(pktdata->ieee_m << 8) |
			pktdata->ieee_l;
	rx_pr("ieee: 0x%x\n", val);
	rx_pr("3d vdfmt: 0x%x\n", pktdata->sbpkt.vsi_3d.vdfmt);

	if (pktdata->length == K_DOLBY_VS_V0_PKT_LENGTH) {
		/*dobly version v0 pkt*/
		rx_pr("BP6 : 0x%x\n",
			pktdata->sbpkt.vsi_3d.pb6.pb6_data);
		for (i = 0; i < 21; i++)
			rx_pr("BP%d : 0x%x\n", i+7,
				pktdata->sbpkt.vsi_3d.resd[i]);
	} else {
		if (pktdata->sbpkt.vsi_3d.vdfmt == 0) {
			rx_pr("no additional vd fmt\n");
		} else if (pktdata->sbpkt.vsi_3d.vdfmt == 1) {
			/*extended resolution format*/
			rx_pr("hdmi vic: 0x%x\n",
				pktdata->sbpkt.vsi_3d.pb5.hdmi_vic);
		} else if (pktdata->sbpkt.vsi_3d.vdfmt == 2) {
			/*3D format*/
			rx_pr("3d struct: 0x%x\n",
				pktdata->sbpkt.vsi_3d.pb5.pb5_st.threeD_st);
			rx_pr("3d ext_data : 0x%x\n",
				pktdata->sbpkt.vsi_3d.pb6.pb6_st.threeD_ex);
		} else {
			rx_pr("unknown vd fmt\n");
		}
	}
	rx_pr(">------------------>end\n");
}

static void rx_pktdump_avi(void *pdata)
{
	struct avi_infoframe_st *pktdata = pdata;

	rx_pr(">---avi infoframe detail -------->\n");
	rx_pr("type: 0x%x\n", pktdata->pkttype);
	rx_pr("ver: %d\n", pktdata->version);
	rx_pr("length: %d\n", pktdata->length);

	if (pktdata->version == 1) {
		/*ver 1*/
		rx_pr("scaninfo S: 0x%x\n", pktdata->cont.v1.scaninfo);
		rx_pr("barinfo B : 0x%x\n", pktdata->cont.v1.barinfo);
		rx_pr("activeinfo A: 0x%x\n", pktdata->cont.v1.activeinfo);
		rx_pr("colorimetry Y: 0x%x\n",
					pktdata->cont.v1.colorindicator);

		rx_pr("fmt_ration R: 0x%x\n", pktdata->cont.v1.fmt_ration);
		rx_pr("pic_ration M: 0x%x\n", pktdata->cont.v1.pic_ration);
		rx_pr("colorimetry C: 0x%x\n", pktdata->cont.v1.colorimetry);

		rx_pr("colorimetry SC: 0x%x\n", pktdata->cont.v1.pic_scaling);
	} else {
		/*ver 2/3*/
		rx_pr("scaninfo S: 0x%x\n", pktdata->cont.v2v3.scaninfo);
		rx_pr("barinfo B: 0x%x\n", pktdata->cont.v2v3.barinfo);
		rx_pr("activeinfo A: 0x%x\n", pktdata->cont.v2v3.activeinfo);
		rx_pr("colorimetry Y: 0x%x\n",
					pktdata->cont.v2v3.colorindicator);

		rx_pr("fmt_ration R: 0x%x\n", pktdata->cont.v2v3.fmt_ration);
		rx_pr("pic_ration M: 0x%x\n", pktdata->cont.v2v3.pic_ration);
		rx_pr("colorimetry C: 0x%x\n", pktdata->cont.v2v3.colorimetry);

		rx_pr("pic_scaling SC: 0x%x\n", pktdata->cont.v2v3.pic_scaling);
		rx_pr("qt_range Q: 0x%x\n", pktdata->cont.v2v3.qt_range);
		rx_pr("ext_color EC : 0x%x\n", pktdata->cont.v2v3.ext_color);
		rx_pr("it_content ITC: 0x%x\n", pktdata->cont.v2v3.it_content);

		rx_pr("vic: 0x%x\n", pktdata->cont.v2v3.vic);

		rx_pr("pix_repeat PR: 0x%x\n",
				pktdata->cont.v2v3.pix_repeat);
		rx_pr("content_type CN: 0x%x\n",
				pktdata->cont.v2v3.content_type);
		rx_pr("ycc_range YQ: 0x%x\n",
				pktdata->cont.v2v3.ycc_range);
	}
	rx_pr("line_end_topbar: 0x%x\n",
				pktdata->line_num_end_topbar);
	rx_pr("line_start_btmbar: 0x%x\n",
				pktdata->line_num_start_btmbar);
	rx_pr("pix_num_left_bar: 0x%x\n",
				pktdata->pix_num_left_bar);
	rx_pr("pix_num_right_bar: 0x%x\n",
				pktdata->pix_num_right_bar);
	rx_pr(">------------------>end\n");
}

static void rx_pktdump_spd(void *pdata)
{

}

static void rx_pktdump_aud(void *pdata)
{
	struct aud_infoframe_st *pktdata = pdata;

	rx_pr(">---audio infoframe detail ---->\n");

	rx_pr("type: 0x%x\n", pktdata->pkttype);
	rx_pr("ver: %d\n", pktdata->version);
	rx_pr("length: %d\n", pktdata->length);

	rx_pr("ch_count(CC):0x%x\n", pktdata->ch_count);
	rx_pr("coding_type(CT):0x%x\n", pktdata->coding_type);
	rx_pr("sample_size(SS):0x%x\n", pktdata->sample_size);
	rx_pr("sample_frq(SF):0x%x\n", pktdata->sample_frq);

	rx_pr("fromat(PB3):0x%x\n", pktdata->fromat);
	rx_pr("mul_ch(CA):0x%x\n", pktdata->mul_ch);
	rx_pr("lfep(BL):0x%x\n", pktdata->lfep);
	rx_pr("level_shift_value(LSV):0x%x\n", pktdata->level_shift_value);
	rx_pr("down_mix(DM_INH):0x%x\n", pktdata->down_mix);
	rx_pr(">------------------>end\n");
}

static void rx_pktdump_mpeg(void *pdata)
{

}

static void rx_pktdump_ntscvbi(void *pdata)
{
	rx_pr(">---ntscvbi infoframe detail ---->\n");

}

static void rx_pktdump_drm(void *pdata)
{
	struct drm_infoframe_st *pktdata = pdata;

	rx_pr(">---drm infoframe detail -------->\n");
	rx_pr("type: 0x%x\n", pktdata->pkttype);
	rx_pr("ver: %d\n", pktdata->version);
	rx_pr("length: %d\n", pktdata->length);

	rx_pr("b1 eotf: 0x%x\n", pktdata->eotf);
	rx_pr("b2 meta des id: 0x%x\n", pktdata->meta_des_id);

	rx_pr("dis_pri_x0: %d\n", pktdata->des_u.tp1.dis_pri_x0);
	rx_pr("dis_pri_y0: %d\n", pktdata->des_u.tp1.dis_pri_y0);
	rx_pr("dis_pri_x1: %d\n", pktdata->des_u.tp1.dis_pri_x1);
	rx_pr("dis_pri_y1: %d\n", pktdata->des_u.tp1.dis_pri_y1);
	rx_pr("dis_pri_x2: %d\n", pktdata->des_u.tp1.dis_pri_x2);
	rx_pr("dis_pri_y2: %d\n", pktdata->des_u.tp1.dis_pri_y2);
	rx_pr("white_points_x: %d\n",
		pktdata->des_u.tp1.white_points_x);
	rx_pr("white_points_y: %d\n",
		pktdata->des_u.tp1.white_points_y);
	rx_pr("max_dislum: %d\n", pktdata->des_u.tp1.max_dislum);
	rx_pr("min_dislum: %d\n", pktdata->des_u.tp1.min_dislum);
	rx_pr("max_light_lvl: %d\n", pktdata->des_u.tp1.max_light_lvl);
	rx_pr("max_fa_light_lvl: %d\n", pktdata->des_u.tp1.max_fa_light_lvl);
	rx_pr(">------------------>end\n");
}

static void rx_pktdump_acr(void *pdata)
{
	struct acr_ptk_st *pktdata = pdata;
	uint32_t CTS;
	uint32_t N;

	rx_pr(">---audio clock regeneration detail ---->\n");

	rx_pr("type: 0x%0x\n", pktdata->pkttype);

	CTS = (pktdata->sbpkt1.SB1_CTS_H << 16) |
			(pktdata->sbpkt1.SB2_CTS_M << 8) |
			pktdata->sbpkt1.SB3_CTS_L;

	N = (pktdata->sbpkt1.SB4_N_H << 16) |
			(pktdata->sbpkt1.SB5_N_M << 8) |
			pktdata->sbpkt1.SB6_N_L;
	rx_pr("sbpkt1 CTS: %d\n", CTS);
	rx_pr("sbpkt1 N : %d\n", N);


	CTS = (pktdata->sbpkt2.SB1_CTS_H << 16) |
			(pktdata->sbpkt2.SB2_CTS_M << 8) |
			pktdata->sbpkt2.SB3_CTS_L;

	N = (pktdata->sbpkt2.SB4_N_H << 16) |
			(pktdata->sbpkt2.SB5_N_M << 8) |
			pktdata->sbpkt2.SB6_N_L;
	rx_pr("sbpkt2 CTS: %d\n", CTS);
	rx_pr("sbpkt2 N : %d\n", N);


	CTS = (pktdata->sbpkt3.SB1_CTS_H << 16) |
			(pktdata->sbpkt3.SB2_CTS_M << 8) |
			pktdata->sbpkt3.SB3_CTS_L;

	N = (pktdata->sbpkt3.SB4_N_H << 16) |
			(pktdata->sbpkt3.SB5_N_M << 8) |
			pktdata->sbpkt3.SB6_N_L;
	rx_pr("sbpkt3 CTS: %d\n", CTS);
	rx_pr("sbpkt3 N : %d\n", N);


	CTS = (pktdata->sbpkt4.SB1_CTS_H << 16) |
			(pktdata->sbpkt4.SB2_CTS_M << 8) |
			pktdata->sbpkt4.SB3_CTS_L;

	N = (pktdata->sbpkt4.SB4_N_H << 16) |
			(pktdata->sbpkt4.SB5_N_M << 8) |
			pktdata->sbpkt4.SB6_N_L;
	rx_pr("sbpkt4 CTS: %d\n", CTS);
	rx_pr("sbpkt4 N : %d\n", N);
	rx_pr(">------------------>end\n");
}

void rx_pkt_dump(uint8_t typeID)
{
	struct rx_s *prx = &rx;

	rx_pr("the following is dump cmd ---cur:0x%x\n", typeID);
	rx_pr("dumpvsi-> 0x81:Vendor-Specific infoframe\n");
	rx_pr("dumpavi-> 0x82:Auxiliary video infoframe\n");
	rx_pr("dumpspd-> 0x83:Source Product Description infoframe\n");
	rx_pr("dumpaud-> 0x84:Audio infoframe\n");
	rx_pr("dumpmpeg->0x85:MPEG infoframe\n");
	rx_pr("dumpnvbi-> 0x86:NTSC VBI infoframe\n");
	rx_pr("dumpdrm-> 0x87:DRM infoframe\n");
	rx_pr("dumpacr-> 0x01:audio clk regeneration\n");
	/*mutex_lock(&pktbuff_lock);*/

	switch (typeID) {
	/*infoframe pkt*/
	case PKT_TYPE_INFOFRAME_VSI:
		rx_pktdump_raw(&prx->vs_info);
		rx_pktdump_vsi(&prx->vs_info);
		break;
	case PKT_TYPE_INFOFRAME_AVI:
		rx_pktdump_raw(&prx->avi_info);
		rx_pktdump_avi(&prx->avi_info);
		break;
	case PKT_TYPE_INFOFRAME_SPD:
		rx_pktdump_raw(&prx->spd_info);
		rx_pktdump_spd(&prx->spd_info);
		break;
	case PKT_TYPE_INFOFRAME_AUD:
		rx_pktdump_raw(&prx->aud_pktinfo);
		rx_pktdump_aud(&prx->aud_pktinfo);
		break;
	case PKT_TYPE_INFOFRAME_MPEGSRC:
		rx_pktdump_raw(&prx->mpegs_info);
		rx_pktdump_mpeg(&prx->mpegs_info);
		break;
	case PKT_TYPE_INFOFRAME_NVBI:
		rx_pktdump_raw(&prx->ntscvbi_info);
		rx_pktdump_ntscvbi(&prx->ntscvbi_info);
		break;
	case PKT_TYPE_INFOFRAME_DRM:
		rx_pktdump_raw(&prx->drm_info);
		rx_pktdump_drm(&prx->drm_info);
		break;
	/*other pkt*/
	case PKT_TYPE_ACR:
		rx_pktdump_raw(&prx->acr_info);
		rx_pktdump_acr(&prx->acr_info);
		break;
	case PKT_TYPE_GCP:
		rx_pktdump_raw(&prx->gcp_info);
		break;
	case PKT_TYPE_ACP:
		rx_pktdump_raw(&prx->acp_info);
		break;
	case PKT_TYPE_ISRC1:
		rx_pktdump_raw(&prx->isrc1_info);
		break;
	case PKT_TYPE_ISRC2:
		rx_pktdump_raw(&prx->isrc2_info);
		break;
	case PKT_TYPE_GAMUT_META:
		rx_pktdump_raw(&prx->gameta_info);
		break;
	case PKT_TYPE_AUD_META:
		rx_pktdump_raw(&prx->amp_info);
		break;

	default:
		rx_pr("warning: not support\n");
		/*rx_pktdump_raw(&prx->dbg_info);*/
		break;
	}

	/*mutex_unlock(&pktbuff_lock);*/
}

uint32_t rx_pkt_typemapping(uint32_t pkt_type)
{
	struct pkt_typeregmap_st *ptab = pktmaping;
	uint32_t i = 0;
	uint32_t rt = 0;

	while (ptab[i].pkt_type != K_FLAG_TAB_END) {
		if (ptab[i].pkt_type == pkt_type)
			rt = ptab[i].reg_bit;

		if (i++ > 20)
			break;
	}
	return rt;
}

void rx_pkt_enable(uint32_t type_regbit)
{
	uint32_t data32;

	data32 = hdmirx_rd_dwc(DWC_PDEC_CTRL);
	data32 |= type_regbit;
	hdmirx_wr_dwc(DWC_PDEC_CTRL, data32);

	/*add for enable pd fifo start int for debug*/
	if ((pdec_ists_en & PD_FIFO_START_PASS) == 0) {
		pdec_ists_en |= PD_FIFO_START_PASS;
		hdmirx_irq_open();
	}
}

void rx_pkt_disable(uint32_t type_regbit)
{
	uint32_t data32;

	data32 = hdmirx_rd_dwc(DWC_PDEC_CTRL);
	data32 &= ~type_regbit;
	hdmirx_wr_dwc(DWC_PDEC_CTRL, data32);
}


void rx_pkt_initial(void)
{
	/*mutex_init(&pktbuff_lock);*/
	memset(&rxpktsts, 0, sizeof(struct rxpkt_st));
}


void rx_pkt_getaudinfo(struct aud_info_s *audio_info)
{
	/*get AudioInfo */
	audio_info->coding_type =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, CODING_TYPE);
	audio_info->channel_count =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, CHANNEL_COUNT);
	audio_info->sample_frequency =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, SAMPLE_FREQ);
	audio_info->sample_size =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, SAMPLE_SIZE);
	audio_info->coding_extension =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, AIF_DATA_BYTE_3);
	audio_info->channel_allocation =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, CH_SPEAK_ALLOC);
	audio_info->down_mix_inhibit =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB1, DWNMIX_INHIBIT);
	audio_info->level_shift_value =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB1, LEVEL_SHIFT_VAL);
	audio_info->aud_packet_received =
			hdmirx_rd_bits_dwc(DWC_PDEC_AUD_STS, AUDS_RCV);

	audio_info->cts = hdmirx_rd_dwc(DWC_PDEC_ACR_CTS);
	audio_info->n = hdmirx_rd_dwc(DWC_PDEC_ACR_N);
	if (audio_info->cts != 0) {
		audio_info->arc = (hdmirx_get_tmds_clock()/audio_info->cts)*
			audio_info->n/128;
	} else
		audio_info->arc = 0;

}

static void rx_pkt_getvsinfo(struct vendor_specific_info_s *vs)
{
	#ifdef HDMI20_ENABLE
	struct vsi_infoframe_t vsi_info;
	memset(&vsi_info, 0, sizeof(struct vsi_infoframe_t));
	vs->identifier = hdmirx_rd_bits_dwc(DWC_PDEC_VSI_ST0, IEEE_REG_ID);
	*(unsigned int *)((unsigned char *)&vsi_info +
		VSI_3D_FORMAT_INDEX) = hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD0);
	vs->vd_fmt = vsi_info.vid_format;
	vsi_info.length = hdmirx_rd_bits_dwc(DWC_PDEC_VSI_ST1, VSI_LENGTH);
	if (log_level & VSI_LOG)
		rx_pr("vsi_info.vid_format:%d,vsi_info.length:%d\n",
		vsi_info.vid_format, vsi_info.length);
	if (vsi_info.length == DOLBY_VERSION_START_LENGTH) {
		/*dolby version start VSI*/
		vs->dolby_vision = TRUE;
		/*length = 0x18,PB6-PB23 = 0x00*/
		if (!(hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD0) & 0xFFFF0000) &&
			!hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD1) &&
			!hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD2) &&
			!hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD3) &&
			!hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD4) &&
			!(hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD5) & 0xFFFFFF)) {
			if (log_level & VSI_LOG)
				if (vs->dolby_vision_sts != DOLBY_VERSION_START)
					rx_pr("dolby vision start\n");
			vs->dolby_vision_sts = DOLBY_VERSION_START;
		}
		/*PB4 PB5 = 0x00 exit dolby version*/
		/*if (((hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD0) & 0xFF) == 0) &&
		((hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD0) & 0xFF00) == 0)) {
			if (log_level & VSI_LOG)
				rx_pr("dolby vision stop\n");
			vs->dolby_vision_sts = DOLBY_VERSION_STOP;
		} else {
			if (log_level & VSI_LOG)
				rx_pr("dolby vision start\n");
			vs->dolby_vision_sts = DOLBY_VERSION_START;
		}*/
	} else if (((vsi_info.length == 0x04) || (vsi_info.length == 0x05)) &&
		(vsi_info.vid_format != VSI_FORMAT_3D_FORMAT)) {
		/*dolby version exit VSI*/
		vs->dolby_vision = TRUE;
		if (log_level & VSI_LOG)
			rx_pr("dolby vision stop\n");
		vs->dolby_vision_sts = DOLBY_VERSION_STOP;
	} else {
	/*3d VSI*/
		vs->dolby_vision = FALSE;
		if (vsi_info.vid_format == VSI_FORMAT_3D_FORMAT) {
			vs->_3d_structure = vsi_info.detail.data_3d.struct_3d;
			vs->_3d_ext_data = vsi_info.struct_3d_ext;
		} else {
			vs->_3d_structure = 0;
			vs->_3d_ext_data = 0;
		}
		if (log_level & VSI_LOG)
			rx_pr("struct_3d:%d, struct_3d_ext:%d\n",
			vsi_info.detail.data_3d.struct_3d,
			 vsi_info.struct_3d_ext);
	}
	if (log_level & VSI_LOG)
		rx_pr("dolby vision:%d\n", vs->dolby_vision);
	#else
	vs->identifier = hdmirx_rd_bits_dwc(DWC_PDEC_VSI_ST0, IEEE_REG_ID);
	vs->vd_fmt = hdmirx_rd_bits_dwc(DWC_PDEC_VSI_ST1, HDMI_VIDEO_FORMAT);
	vs->_3d_structure = hdmirx_rd_bits_dwc(DWC_PDEC_VSI_ST1, H3D_STRUCTURE);
	vs->_3d_ext_data = hdmirx_rd_bits_dwc(DWC_PDEC_VSI_ST1, H3D_EXT_DATA);
	#endif
}

#if 0
static int vsi_handler(struct hdmi_rx_ctrl *ctx)
{
	struct vendor_specific_info_s *vs_info = &rx.vendor_specific_info;

	rx_pkt_getvsinfo(vs_info);
	if (log_level & VSI_LOG)
		rx_pr("dolby vision:%d,dolby_vision_sts %d)\n",
		vs_info->dolby_vision, vs_info->dolby_vision_sts);
	return TRUE;
}
#endif

static int rx_pkt_getdrminfo(struct hdmi_rx_ctrl *ctx)
{
	int error = 0;

	if (rx.state != FSM_SIG_READY)
		return 0;

	if (hdr_enable == false)
		return 0;
#if 0/*Qy temp mark for compile */
	if (sm_pause)
		return 0;
#endif
	if (ctx == 0)
		return -EINVAL;

	/* waiting, before send the hdr data to post modules */
	if (rx.hdr_info.hdr_state != HDR_STATE_NULL)
		return -EBUSY;

	rx.hdr_info.hdr_state = HDR_STATE_GET;
	rx.hdr_info.hdr_data.eotf =
		(hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD0) >> 8) & 0xFF;
	rx.hdr_info.hdr_data.metadata_id =
		(hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD0) >> 16) & 0xFF;
	rx.hdr_info.hdr_data.lenght =
		(unsigned char)(hdmirx_rd_dwc(DWC_PDEC_DRM_HB) >> 8);

	rx.hdr_info.hdr_data.primaries[0].x =
		((hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD0) >> 24) & 0xFF) +
		((hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD1) << 8) & 0xFF00);
	rx.hdr_info.hdr_data.primaries[0].y =
		((hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD1) >> 8) & 0xFF) +
		((hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD1) >> 8) & 0xFF00);
	rx.hdr_info.hdr_data.primaries[1].x =
		((hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD1) >> 24) & 0xFF) +
		((hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD2) << 8) & 0xFF00);
	rx.hdr_info.hdr_data.primaries[1].y =
		((hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD2) >> 8) & 0xFF) +
		((hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD2) >> 8) & 0xFF00);
	rx.hdr_info.hdr_data.primaries[2].x =
		((hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD2) >> 24) & 0xFF) +
		((hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD3) << 8) & 0xFF00);
	rx.hdr_info.hdr_data.primaries[2].y =
		((hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD3) >> 8) & 0xFF) +
		((hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD3) >> 8) & 0xFF00);
	rx.hdr_info.hdr_data.white_points.x =
		((hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD3) >> 24) & 0xFF) +
		((hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD4) << 8) & 0xFF00);
	rx.hdr_info.hdr_data.white_points.y =
		((hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD4) >> 8) & 0xFF) +
		((hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD4) >> 8) & 0xFF00);
	rx.hdr_info.hdr_data.master_lum.x =
		((hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD4) >> 24) & 0xFF) +
		((hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD5) << 8) & 0xFF00);
	rx.hdr_info.hdr_data.master_lum.y =
		((hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD5) >> 8) & 0xFF) +
		((hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD5) >> 8) & 0xFF00);
	rx.hdr_info.hdr_data.mcll =
		((hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD5) >> 24) & 0xFF) +
		((hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD6) << 8) & 0xFF00);
	rx.hdr_info.hdr_data.mfall =
		((hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD6) >> 8) & 0xFF) +
		((hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD6) >> 8) & 0xFF00);

	rx.hdr_info.hdr_state = HDR_STATE_SET;

	return error;
}

int rx_pkt_decode(struct rx_s *prx,
				union infoframe_u *pktdata,
				struct rxpkt_st *pktsts)
{
	switch (pktdata->raw_infoframe.pkttype) {
	case PKT_TYPE_INFOFRAME_VSI:
		pktsts->pkt_cnt_vsi++;
		memset(&prx->vs_info, 0, sizeof(struct pd_infoframe_s));
		memcpy(&prx->vs_info, pktdata, sizeof(struct pd_infoframe_s));
		break;
	case PKT_TYPE_INFOFRAME_AVI:
		pktsts->pkt_cnt_avi++;
		memset(&prx->avi_info, 0, sizeof(struct pd_infoframe_s));
		memcpy(&prx->avi_info, pktdata, sizeof(struct pd_infoframe_s));
		break;
	case PKT_TYPE_INFOFRAME_SPD:
		pktsts->pkt_cnt_spd++;
		memset(&prx->spd_info, 0, sizeof(struct pd_infoframe_s));
		memcpy(&prx->spd_info, pktdata,
					sizeof(struct spd_infoframe_st));
		break;
	case PKT_TYPE_INFOFRAME_AUD:
		pktsts->pkt_cnt_audif++;
		memset(&prx->aud_pktinfo, 0, sizeof(struct pd_infoframe_s));
		memcpy(&prx->aud_pktinfo,
				pktdata, sizeof(struct pd_infoframe_s));
		break;
	case PKT_TYPE_INFOFRAME_MPEGSRC:
		pktsts->pkt_cnt_mpeg++;
		memcpy(&prx->mpegs_info, pktdata,
					sizeof(struct pd_infoframe_s));
		break;
	case PKT_TYPE_INFOFRAME_NVBI:
		pktsts->pkt_cnt_nvbi++;
		memcpy(&prx->ntscvbi_info, pktdata,
					sizeof(struct pd_infoframe_s));
		break;
	case PKT_TYPE_INFOFRAME_DRM:
		pktsts->pkt_cnt_drm++;
		memset(&prx->drm_info, 0, sizeof(struct pd_infoframe_s));
		memcpy(&prx->drm_info, pktdata, sizeof(struct pd_infoframe_s));
		break;

	case PKT_TYPE_ACR:
		pktsts->pkt_cnt_acr++;
		memset(&prx->acr_info, 0, sizeof(struct pd_infoframe_s));
		memcpy(&prx->acr_info,
			pktdata, sizeof(struct pd_infoframe_s));
		break;
	case PKT_TYPE_GCP:
		pktsts->pkt_cnt_gcp++;
		memset(&prx->gcp_info, 0, sizeof(struct pd_infoframe_s));
		memcpy(&prx->gcp_info,
			pktdata, sizeof(struct pd_infoframe_s));
		break;
	case PKT_TYPE_ACP:
		pktsts->pkt_cnt_acp++;
		memset(&prx->acp_info, 0, sizeof(struct pd_infoframe_s));
		memcpy(&prx->acp_info,
			pktdata, sizeof(struct pd_infoframe_s));
		break;
	case PKT_TYPE_ISRC1:
		pktsts->pkt_cnt_isrc1++;
		memset(&prx->isrc1_info, 0, sizeof(struct pd_infoframe_s));
		memcpy(&prx->isrc1_info,
			pktdata, sizeof(struct pd_infoframe_s));
		break;
	case PKT_TYPE_ISRC2:
		pktsts->pkt_cnt_isrc2++;
		memset(&prx->isrc2_info, 0, sizeof(struct pd_infoframe_s));
		memcpy(&prx->isrc2_info,
			pktdata, sizeof(struct pd_infoframe_s));
		break;
	case PKT_TYPE_GAMUT_META:
		pktsts->pkt_cnt_gameta++;
		memset(&prx->gameta_info, 0, sizeof(struct pd_infoframe_s));
		memcpy(&prx->gameta_info,
			pktdata, sizeof(struct pd_infoframe_s));
		break;
	case PKT_TYPE_AUD_META:
		pktsts->pkt_cnt_amp++;
		memset(&prx->amp_info, 0, sizeof(struct pd_infoframe_s));
		memcpy(&prx->amp_info,
			pktdata, sizeof(struct pd_infoframe_s));
		break;
	default:
		break;
	}

	return 0;
}

int rx_pkt_handler(uint32_t pkt_int_src)
{
	uint32_t i = 0;
	uint32_t j = 0;
	uint32_t pkt_num = 0;
	uint32_t pkt_cnt = 0;
	union infoframe_u *pktdata;
	struct rx_s *prx = &rx;
	/*uint32_t t1, t2;*/

	if (pkt_int_src == PKT_BUFF_SET_FIFO) {
		/*from pkt fifo*/
		if (!pd_fifo_buf)
			return 0;

		/*t1 = sched_clock();*/
		/*for recode interrupt cnt*/
		rxpktsts.fifo_Int_cnt++;

		/*ps:read 10 packet from fifo cost time < less than 200 us */
		if (hdmirx_rd_dwc(DWC_PDEC_STS) & PD_TH_START) {
readpkt:
			/*how many packet number need read from fifo*/
			/*If software tries to read more packets from the
			FIFO than what is stored already, an underflow occurs.*/
			pkt_num = hdmirx_rd_dwc(DWC_PDEC_FIFO_STS1) >> 3;
			rxpktsts.fifo_pkt_num = pkt_num;

			for (pkt_cnt = 0; pkt_cnt < pkt_num; pkt_cnt++) {
				/*read one pkt from fifo*/
				for (j = 0; j < K_ONEPKT_BUFF_SIZE; j++) {
					/*8 word per packet size*/
					pd_fifo_buf[i+j] =
					hdmirx_rd_dwc(DWC_PDEC_FIFO_DATA);
				}

				if (log_level & PACKET_LOG)
					rx_pr("PD[%d]=%x\n", i, pd_fifo_buf[i]);

				pktdata = (union infoframe_u *)&pd_fifo_buf[i];
				/*mutex_lock(&pktbuff_lock);*/
				/*pkt decode*/
					rx_pkt_decode(prx, pktdata, &rxpktsts);
				/*mutex_unlock(&pktbuff_lock);*/
				i += 8;
				if (i > (PFIFO_SIZE - 8))
					i = 0;
			}
			/*while read from buff, other more packets attach*/
			pkt_num = hdmirx_rd_dwc(DWC_PDEC_FIFO_STS1) >> 3;
			if (pkt_num > K_PKT_REREAD_SIZE)
				goto readpkt;
		}
	} else if (pkt_int_src == PKT_BUFF_SET_VSI) {
		rx_pkt_getvsinfo(&prx->vendor_specific_info);
	} else if (pkt_int_src == PKT_BUFF_SET_DRM) {
		rx_pkt_getdrminfo(&prx->ctrl);
	}

	/*t2 = sched_clock();*/
	/*
	timerbuff[timerbuff_idx] = pkt_num;
	if (timerbuff_idx++ >= 50)
		timerbuff_idx = 0;
	*/
	return 0;
}

