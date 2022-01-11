// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>

/* Local include */
#include "hdmi_rx_repeater.h"
#include "hdmi_rx_pktinfo.h"
#include "hdmi_rx_drv.h"
#include "hdmi_rx_hw.h"
#include "hdmi_rx_wrapper.h"

/*this parameter moved from wrapper.c */
/*
 * bool hdr_enable = true;
 */
static struct rxpkt_st rxpktsts;
struct packet_info_s rx_pkt;
u32 rx_vsif_type;
u32 rx_emp_type;
u32 rx_spd_type;
u32 gpkt_fifo_pri;
/*struct mutex pktbuff_lock;*/

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
	{PKT_TYPE_EMP,				PFIFO_EMP_EN},

	/*end of the table*/
	{K_FLAG_TAB_END,			K_FLAG_TAB_END},
};

/*
 * u32 timerbuff[50];
 * u32 timerbuff_idx = 0;
 */

struct st_pkt_test_buff *pkt_testbuff;

void rx_pkt_status(void)
{
	/*u32 i;*/

	//rx_pr("packet_fifo_cfg=0x%x\n", packet_fifo_cfg);
	/*rx_pr("pdec_ists_en=0x%x\n\n", pdec_ists_en);*/
	rx_pr("fifo_Int_cnt=%d\n", rxpktsts.fifo_int_cnt);
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
	rx_pr("pkt_cnt_emp=%d\n", rxpktsts.pkt_cnt_emp);
	rx_pr("pkt_cnt_vsi_ex=%d\n", rxpktsts.pkt_cnt_vsi_ex);
	rx_pr("pkt_cnt_drm_ex=%d\n", rxpktsts.pkt_cnt_drm_ex);
	rx_pr("pkt_cnt_gmd_ex=%d\n", rxpktsts.pkt_cnt_gmd_ex);
	rx_pr("pkt_cnt_aif_ex=%d\n", rxpktsts.pkt_cnt_aif_ex);
	rx_pr("pkt_cnt_avi_ex=%d\n", rxpktsts.pkt_cnt_avi_ex);
	rx_pr("pkt_cnt_acr_ex=%d\n", rxpktsts.pkt_cnt_acr_ex);
	rx_pr("pkt_cnt_gcp_ex=%d\n", rxpktsts.pkt_cnt_gcp_ex);
	rx_pr("pkt_cnt_amp_ex=%d\n", rxpktsts.pkt_cnt_amp_ex);
	rx_pr("pkt_cnt_nvbi_ex=%d\n", rxpktsts.pkt_cnt_nvbi_ex);
	rx_pr("pkt_cnt_emp_ex=%d\n", rxpktsts.pkt_cnt_emp_ex);
	rx_pr("pkt_chk_flg=%d\n", rxpktsts.pkt_chk_flg);

}

void rx_pkt_debug(void)
{
	u32 data32;

	rx_pr("\npktinfo size=%lu\n", sizeof(union pktinfo));
	rx_pr("infoframe_st size=%lu\n", sizeof(union infoframe_u));
	rx_pr("fifo_rawdata_st size=%lu\n", sizeof(struct fifo_rawdata_st));
	rx_pr("vsi_infoframe_st size=%lu\n", sizeof(struct vsi_infoframe_st));
	rx_pr("avi_infoframe_st size=%lu\n", sizeof(struct avi_infoframe_st));
	rx_pr("spd_infoframe_st size=%lu\n", sizeof(struct spd_infoframe_st));
	rx_pr("aud_infoframe_st size=%lu\n", sizeof(struct aud_infoframe_st));
	rx_pr("ms_infoframe_st size=%lu\n", sizeof(struct ms_infoframe_st));
	rx_pr("vbi_infoframe_st size=%lu\n", sizeof(struct vbi_infoframe_st));
	rx_pr("drm_infoframe_st size=%lu\n", sizeof(struct drm_infoframe_st));

	rx_pr("acr_pkt_st size=%lu\n",
	      sizeof(struct acr_pkt_st));
	rx_pr("aud_sample_pkt_st size=%lu\n",
	      sizeof(struct aud_sample_pkt_st));
	rx_pr("gcp_pkt_st size=%lu\n", sizeof(struct gcp_pkt_st));
	rx_pr("acp_pkt_st size=%lu\n", sizeof(struct acp_pkt_st));
	rx_pr("isrc_pkt_st size=%lu\n", sizeof(struct isrc_pkt_st));
	rx_pr("onebit_aud_pkt_st size=%lu\n",
	      sizeof(struct obasmp_pkt_st));
	rx_pr("dst_aud_pkt_st size=%lu\n",
	      sizeof(struct dstaud_pkt_st));
	rx_pr("hbr_aud_pkt_st size=%lu\n",
	      sizeof(struct hbraud_pkt_st));
	rx_pr("gamut_Meta_pkt_st size=%lu\n",
	      sizeof(struct gamutmeta_pkt_st));
	rx_pr("aud_3d_smppkt_st size=%lu\n",
	      sizeof(struct a3dsmp_pkt_st));
	rx_pr("oneb3d_smppkt_st size=%lu\n",
	      sizeof(struct ob3d_smppkt_st));
	rx_pr("aud_mtdata_pkt_st size=%lu\n",
	      sizeof(struct audmtdata_pkt_st));
	rx_pr("mulstr_audsamp_pkt_st size=%lu\n",
	      sizeof(struct msaudsmp_pkt_st));
	rx_pr("onebmtstr_smaud_pkt_st size=%lu\n",
	      sizeof(struct obmaudsmp_pkt_st));
	rx_pr("emp size=%lu\n",
	      sizeof(struct emp_pkt_st));
	memset(&rxpktsts, 0, sizeof(struct rxpkt_st));

	data32 = hdmirx_rd_dwc(DWC_PDEC_CTRL);
	data32 |= (rx_pkt_type_mapping(PKT_TYPE_INFOFRAME_VSI));
	data32 |= (rx_pkt_type_mapping(PKT_TYPE_INFOFRAME_AVI));
	data32 |= (rx_pkt_type_mapping(PKT_TYPE_INFOFRAME_SPD));
	data32 |= (rx_pkt_type_mapping(PKT_TYPE_INFOFRAME_AUD));
	data32 |= (rx_pkt_type_mapping(PKT_TYPE_INFOFRAME_VSI));
	data32 |= (rx_pkt_type_mapping(PKT_TYPE_INFOFRAME_MPEGSRC));
	if (rx.chip_id != CHIP_ID_TXHD &&
		rx.chip_id != CHIP_ID_T5D) {
		data32 |= (rx_pkt_type_mapping(PKT_TYPE_INFOFRAME_NVBI));
		data32 |= (rx_pkt_type_mapping(PKT_TYPE_INFOFRAME_DRM));
		data32 |= (rx_pkt_type_mapping(PKT_TYPE_AUD_META));
	}
	data32 |= (rx_pkt_type_mapping(PKT_TYPE_ACR));
	data32 |= (rx_pkt_type_mapping(PKT_TYPE_GCP));
	data32 |= (rx_pkt_type_mapping(PKT_TYPE_ACP));
	data32 |= (rx_pkt_type_mapping(PKT_TYPE_ISRC1));
	data32 |= (rx_pkt_type_mapping(PKT_TYPE_ISRC2));
	data32 |= (rx_pkt_type_mapping(PKT_TYPE_GAMUT_META));
	if (rx.chip_id >= CHIP_ID_TL1)
		data32 |= (rx_pkt_type_mapping(PKT_TYPE_EMP));

	hdmirx_wr_dwc(DWC_PDEC_CTRL, data32);
	rx_pr("enable fifo\n");
}

void rx_get_pd_fifo_param(enum pkt_type_e pkt_type,
			  struct pd_infoframe_s *pkt_info)
{
	switch (pkt_type) {
	case PKT_TYPE_INFOFRAME_VSI:
		if (rx_pkt_get_fifo_pri())
			/* srcbuff = &rx.vs_info; */
			memcpy(pkt_info, &rx_pkt.vs_info,
			       sizeof(struct pd_infoframe_s));
		else
			rx_pkt_get_vsi_ex(&pkt_info);
		break;
	case PKT_TYPE_INFOFRAME_AVI:
		if (rx_pkt_get_fifo_pri())
			/* srcbuff = &rx.avi_info; */
			memcpy(pkt_info, &rx_pkt.avi_info,
			       sizeof(struct pd_infoframe_s));
		else
			rx_pkt_get_avi_ex(&pkt_info);
		break;
	case PKT_TYPE_INFOFRAME_SPD:
		/* srcbuff = &rx.spd_info; */
		memcpy(pkt_info, &rx_pkt.spd_info,
		       sizeof(struct pd_infoframe_s));
		break;
	case PKT_TYPE_INFOFRAME_AUD:
		if (rx_pkt_get_fifo_pri())
			/* srcbuff = &rx.aud_pktinfo; */
			memcpy(pkt_info, &rx_pkt.aud_pktinfo,
			       sizeof(struct pd_infoframe_s));
		else
			rx_pkt_get_audif_ex(&pkt_info);
		break;
	case PKT_TYPE_INFOFRAME_MPEGSRC:
		/* srcbuff = &rx.mpegs_info; */
		memcpy(pkt_info, &rx_pkt.mpegs_info,
		       sizeof(struct pd_infoframe_s));
		break;
	case PKT_TYPE_INFOFRAME_NVBI:
		if (rx_pkt_get_fifo_pri())
			/* srcbuff = &rx.ntscvbi_info; */
			memcpy(pkt_info, &rx_pkt.ntscvbi_info,
			       sizeof(struct pd_infoframe_s));
		else
			rx_pkt_get_ntscvbi_ex(&pkt_info);
		break;
	case PKT_TYPE_INFOFRAME_DRM:
		if (rx_pkt_get_fifo_pri())
			/* srcbuff = &rx.drm_info; */
			memcpy(pkt_info, &rx_pkt.drm_info,
			       sizeof(struct pd_infoframe_s));
		else
			rx_pkt_get_drm_ex(&pkt_info);
		break;
	case PKT_TYPE_ACR:
		if (rx_pkt_get_fifo_pri())
			/* srcbuff = &rx.acr_info; */
			memcpy(pkt_info, &rx_pkt.acr_info,
			       sizeof(struct pd_infoframe_s));
		else
			rx_pkt_get_acr_ex(&pkt_info);
		break;
	case PKT_TYPE_GCP:
		if (rx_pkt_get_fifo_pri())
			/* srcbuff = &rx.gcp_info; */
			memcpy(pkt_info, &rx_pkt.gcp_info,
			       sizeof(struct pd_infoframe_s));
		else
			rx_pkt_get_gcp_ex(&pkt_info);
		break;
	case PKT_TYPE_ACP:
		/* srcbuff = &rx.acp_info; */
		memcpy(pkt_info, &rx_pkt.acp_info,
		       sizeof(struct pd_infoframe_s));
		break;
	case PKT_TYPE_ISRC1:
		/* srcbuff = &rx.isrc1_info; */
		memcpy(pkt_info, &rx_pkt.isrc1_info,
		       sizeof(struct pd_infoframe_s));
		break;
	case PKT_TYPE_ISRC2:
		/* srcbuff = &rx.isrc2_info; */
		memcpy(pkt_info, &rx_pkt.isrc2_info,
		       sizeof(struct pd_infoframe_s));
		break;
	case PKT_TYPE_GAMUT_META:
		if (rx_pkt_get_fifo_pri())
			/* srcbuff = &rx.gameta_info; */
			memcpy(pkt_info, &rx_pkt.gameta_info,
			       sizeof(struct pd_infoframe_s));
		else
			rx_pkt_get_gmd_ex(&pkt_info);
		break;
	case PKT_TYPE_AUD_META:
		if (rx_pkt_get_fifo_pri())
			/* srcbuff = &rx.amp_info; */
			memcpy(pkt_info, &rx_pkt.amp_info,
			       sizeof(struct pd_infoframe_s));
		else
			rx_pkt_get_amp_ex(&pkt_info);
		break;
	default:
		//if (!pd_fifo_buf)  TODO
			//memcpy(pkt_info, pd_fifo_buf, sizeof(struct pd_infoframe_s));
		//else
			//pr_err("err:pd_fifo_buf is empty\n");
		break;
		}
}

/*
 * hdmi rx packet module debug interface, if system not
 * enable pkt fifo module, you need first fun "debugfifo" cmd
 * to enable hw de-code pkt. and then rum "status", you will
 * see the pkt_xxx_cnt value is increasing. means have received
 * pkt info.
 * @param  input [are group of string]
 *  cmd style: pktinfo [param1] [param2]
 *  cmd ex:pktinfo dump 0x82
 *  ->means dump avi infofram pkt content
 * @return [no]
 */
void rx_debug_pktinfo(char input[][20])
{
	u32 sts = 0;
	u32 enable = 0;
	u32 res = 0;

	if (strncmp(input[1], "debugfifo", 9) == 0) {
		/*open all pkt interrupt source for debug*/
		rx_pkt_debug();
		enable |= (PD_FIFO_START_PASS | PD_FIFO_OVERFL);
		pdec_ists_en |= enable;
		rx_pr("pdec_ists_en=0x%x\n", pdec_ists_en);
		rx_irq_en(1);
	} else if (strncmp(input[1], "debugext", 8) == 0) {
		if (rx.chip_id == CHIP_ID_TXLX ||
		    rx.chip_id == CHIP_ID_TXHD)
			enable |= _BIT(30);/* DRC_RCV*/
		else
			enable |= _BIT(9);/* DRC_RCV*/
		if (rx.chip_id >= CHIP_ID_TL1)
			enable |= _BIT(9);/* EMP_RCV*/
		enable |= _BIT(20);/* GMD_RCV */
		enable |= _BIT(19);/* AIF_RCV */
		enable |= _BIT(18);/* AVI_RCV */
		enable |= _BIT(17);/* ACR_RCV */
		enable |= _BIT(16);/* GCP_RCV */
		enable |= _BIT(15);/* VSI_RCV CHG*/
		enable |= _BIT(14);/* AMP_RCV*/
		pdec_ists_en |= enable;
		rx_pr("pdec_ists_en=0x%x\n", pdec_ists_en);
		rx_irq_en(1);
	} else if (strncmp(input[1], "status", 6) == 0) {
		rx_pkt_status();
	} else if (strncmp(input[1], "dump", 7) == 0) {
		/*check input type*/
		if (kstrtou32(input[2], 16, &res) < 0)
			rx_pr("error input:fmt is 0xValue\n");
		rx_pkt_dump(res);
	} else if (strncmp(input[1], "irqdisable", 10) == 0) {
		if (strncmp(input[2], "fifo", 4) == 0) {
			sts = (PD_FIFO_START_PASS | PD_FIFO_OVERFL);
		} else if (strncmp(input[2], "drm", 3) == 0) {
			if (rx.chip_id == CHIP_ID_TXLX ||
			    rx.chip_id == CHIP_ID_TXHD)
				sts = _BIT(30);
			else
				sts = _BIT(9);
		} else if (strncmp(input[2], "gmd", 3) == 0) {
			sts = _BIT(20);
		} else if (strncmp(input[2], "aif", 3) == 0) {
			sts = _BIT(19);
		} else if (strncmp(input[2], "avi", 3) == 0) {
			sts = _BIT(18);
		} else if (strncmp(input[2], "acr", 3) == 0) {
			sts = _BIT(17);
		} else if (strncmp(input[2], "gcp", 3) == 0) {
			sts = GCP_RCV;
		} else if (strncmp(input[2], "vsi", 3) == 0) {
			sts = VSI_RCV;
		} else if (strncmp(input[2], "amp", 3) == 0) {
			sts = _BIT(14);
		} else if (strncmp(input[2], "emp", 3) == 0) {
			if (rx.chip_id >= CHIP_ID_TL1)
				sts = _BIT(9);
			else
				rx_pr("no emp function\n");
		}
		pdec_ists_en &= ~sts;
		rx_pr("pdec_ists_en=0x%x\n", pdec_ists_en);
		/*disable irq*/
		hdmirx_wr_dwc(DWC_PDEC_IEN_CLR, sts);
	} else if (strncmp(input[1], "irqenable", 9) == 0) {
		sts = hdmirx_rd_dwc(DWC_PDEC_IEN);
		if (strncmp(input[2], "fifo", 4) == 0) {
			enable |= (PD_FIFO_START_PASS | PD_FIFO_OVERFL);
		} else if (strncmp(input[2], "drm", 3) == 0) {
			if (rx.chip_id == CHIP_ID_TXLX ||
			    rx.chip_id == CHIP_ID_TXHD)
				enable |= _BIT(30);
			else
				enable |= _BIT(9);
		} else if (strncmp(input[2], "gmd", 3) == 0) {
			enable |= _BIT(20);
		} else if (strncmp(input[2], "aif", 3) == 0) {
			enable |= _BIT(19);
		} else if (strncmp(input[2], "avi", 3) == 0) {
			enable |= _BIT(18);
		} else if (strncmp(input[2], "acr", 3) == 0) {
			enable |= _BIT(17);
		} else if (strncmp(input[2], "gcp", 3) == 0) {
			enable |= GCP_RCV;
		} else if (strncmp(input[2], "vsi", 3) == 0) {
			enable |= VSI_RCV;
		} else if (strncmp(input[2], "amp", 3) == 0) {
			enable |= _BIT(14);
		} else if (strncmp(input[2], "emp", 3) == 0) {
			if (rx.chip_id >= CHIP_ID_TL1)
				enable |= _BIT(9);
			else
				rx_pr("no emp function\n");
		}
		pdec_ists_en = enable | sts;
		rx_pr("pdec_ists_en=0x%x\n", pdec_ists_en);
		/*open irq*/
		hdmirx_wr_dwc(DWC_PDEC_IEN_SET, pdec_ists_en);
		/*hdmirx_irq_open()*/
	} else if (strncmp(input[1], "fifopkten", 9) == 0) {
		/*check input*/
		if (kstrtou32(input[2], 16, &res) < 0)
			return;
		rx_pr("pkt ctl disable:0x%x", res);
		/*check pkt enable ctl bit*/
		sts = rx_pkt_type_mapping((u32)res);
		if (sts == 0)
			return;

		packet_fifo_cfg |= sts;
		/* not work immediately ?? meybe int is not open*/
		enable = hdmirx_rd_dwc(DWC_PDEC_CTRL);
		enable |= sts;
		hdmirx_wr_dwc(DWC_PDEC_CTRL, enable);
	} else if (strncmp(input[1], "fifopktdis", 10) == 0) {
		/*check input*/
		if (kstrtou32(input[2], 16, &res) < 0)
			return;
		rx_pr("pkt ctl disable:0x%x", res);
		/*check pkt enable ctl bit*/
		sts = rx_pkt_type_mapping((u32)res);
		if (sts == 0)
			return;

		packet_fifo_cfg &= (~sts);
		/* not work immediately ?? meybe int is not open*/
		enable = hdmirx_rd_dwc(DWC_PDEC_CTRL);
		enable &= (~sts);
		hdmirx_wr_dwc(DWC_PDEC_CTRL, enable);
	} else if (strncmp(input[1], "contentchk", 10) == 0) {
		/*check input*/
		if (kstrtou32(input[2], 16, &res) < 0) {
			rx_pr("error input:fmt is 0xXX\n");
			return;
		}
		rx_pkt_content_chk_en(res);
	} else if (strncmp(input[1], "pdfifopri", 9) == 0) {
		/*check input*/
		if (kstrtou32(input[2], 16, &res) < 0) {
			rx_pr("error input:fmt is 0xXX\n");
			return;
		}
		rx_pkt_set_fifo_pri(res);
	}
}

static void rx_pktdump_raw(void *pdata)
{
	u8 i;
	union infoframe_u *pktdata = pdata;

	rx_pr(">---raw data detail------>\n");
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
	u32 i;

	rx_pr(">---vsi infoframe detail -------->\n");
	rx_pr("type: 0x%x\n", pktdata->pkttype);
	rx_pr("ver: %d\n", pktdata->ver_st.version);
	rx_pr("chgbit: %d\n", pktdata->ver_st.chgbit);
	rx_pr("length: %d\n", pktdata->length);
	rx_pr("ieee: 0x%x\n", pktdata->ieee);
	for (i = 0; i < 6; i++)
		rx_pr("payload %d : 0x%x\n", i,
		      pktdata->sbpkt.payload.data[i]);
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
		rx_pr("scaninfo S: 0x%x\n", pktdata->cont.v4.scaninfo);
		rx_pr("barinfo B: 0x%x\n", pktdata->cont.v4.barinfo);
		rx_pr("activeinfo A: 0x%x\n", pktdata->cont.v4.activeinfo);
		rx_pr("colorimetry Y: 0x%x\n",
		      pktdata->cont.v4.colorindicator);
		rx_pr("fmt_ration R: 0x%x\n", pktdata->cont.v4.fmt_ration);
		rx_pr("pic_ration M: 0x%x\n", pktdata->cont.v4.pic_ration);
		rx_pr("colorimetry C: 0x%x\n", pktdata->cont.v4.colorimetry);
		rx_pr("pic_scaling SC: 0x%x\n", pktdata->cont.v4.pic_scaling);
		rx_pr("qt_range Q: 0x%x\n", pktdata->cont.v4.qt_range);
		rx_pr("ext_color EC : 0x%x\n", pktdata->cont.v4.ext_color);
		rx_pr("it_content ITC: 0x%x\n", pktdata->cont.v4.it_content);
		rx_pr("vic: 0x%x\n", pktdata->cont.v4.vic);
		rx_pr("pix_repeat PR: 0x%x\n",
		      pktdata->cont.v4.pix_repeat);
		rx_pr("content_type CN: 0x%x\n",
		      pktdata->cont.v4.content_type);
		rx_pr("ycc_range YQ: 0x%x\n",
		      pktdata->cont.v4.ycc_range);
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
	struct spd_infoframe_st *pktdata = pdata;

	rx_pr(">---spd infoframe detail ---->\n");
	rx_pr("type: 0x%x\n", pktdata->pkttype);
	rx_pr("ver: %d\n", pktdata->version);
	rx_pr("length: %d\n", pktdata->length);
	rx_pr(">------------------>end\n");
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
	rx_pr("mul_ch(CA):0x%x\n", pktdata->ca);
	rx_pr("lfep(BL):0x%x\n", pktdata->lfep);
	rx_pr("level_shift_value(LSV):0x%x\n", pktdata->level_shift_value);
	rx_pr("down_mix(DM_INH):0x%x\n", pktdata->down_mix);
	rx_pr(">------------------>end\n");
}

static void rx_pktdump_drm(void *pdata)
{
	struct drm_infoframe_st *pktdata = pdata;
	union infoframe_u pktraw;

	memset(&pktraw, 0, sizeof(union infoframe_u));

	rx_pr(">---drm infoframe detail -------->\n");
	rx_pr("type: 0x%x\n", pktdata->pkttype);
	rx_pr("ver: %x\n", pktdata->version);
	rx_pr("length: %x\n", pktdata->length);

	rx_pr("b1 eotf: 0x%x\n", pktdata->des_u.tp1.eotf);
	rx_pr("b2 meta des id: 0x%x\n", pktdata->des_u.tp1.meta_des_id);

	rx_pr("dis_pri_x0: %x\n", pktdata->des_u.tp1.dis_pri_x0);
	rx_pr("dis_pri_y0: %x\n", pktdata->des_u.tp1.dis_pri_y0);
	rx_pr("dis_pri_x1: %x\n", pktdata->des_u.tp1.dis_pri_x1);
	rx_pr("dis_pri_y1: %x\n", pktdata->des_u.tp1.dis_pri_y1);
	rx_pr("dis_pri_x2: %x\n", pktdata->des_u.tp1.dis_pri_x2);
	rx_pr("dis_pri_y2: %x\n", pktdata->des_u.tp1.dis_pri_y2);
	rx_pr("white_points_x: %x\n", pktdata->des_u.tp1.white_points_x);
	rx_pr("white_points_y: %x\n", pktdata->des_u.tp1.white_points_y);
	rx_pr("max_dislum: %x\n", pktdata->des_u.tp1.max_dislum);
	rx_pr("min_dislum: %x\n", pktdata->des_u.tp1.min_dislum);
	rx_pr("max_light_lvl: %x\n", pktdata->des_u.tp1.max_light_lvl);
	rx_pr("max_fa_light_lvl: %x\n", pktdata->des_u.tp1.max_fa_light_lvl);
	rx_pr(">------------------>end\n");
}

static void rx_pktdump_acr(void *pdata)
{
	struct acr_pkt_st *pktdata = pdata;
	u32 CTS;
	u32 N;

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
	rx_pr(">------------------>end\n");
}

static void rx_pktdump_emp(void *pdata)
{
	struct emp_pkt_st *pktdata = pdata;

	rx_pr("pkttype=0x%x\n", pktdata->pkttype);
	rx_pr("first=0x%x\n", pktdata->first);
	rx_pr("last=0x%x\n", pktdata->last);
	rx_pr("sequence_idx=0x%x\n", pktdata->sequence_idx);
	rx_pr("cnt.new=0x%x\n", pktdata->cnt.new);
	rx_pr("cnt.end=0x%x\n", pktdata->cnt.end);
	rx_pr("cnt.ds_type=0x%x\n", pktdata->cnt.ds_type);
	rx_pr("cnt.afr=0x%x\n", pktdata->cnt.afr);
	rx_pr("cnt.vfr=0x%x\n", pktdata->cnt.vfr);
	rx_pr("cnt.sync=0x%x\n", pktdata->cnt.sync);
	rx_pr("cnt.or_id=0x%x\n", pktdata->cnt.organization_id);
	rx_pr("cnt.tag=0x%x\n", pktdata->cnt.data_set_tag_lo);
	rx_pr("cnt.length=0x%x\n", pktdata->cnt.data_set_length_lo);
}

void rx_pkt_dump(enum pkt_type_e typeid)
{
	struct packet_info_s *prx = &rx_pkt;
	union infoframe_u pktdata;

	rx_pr("dump cmd:0x%x\n", typeid);

	/*mutex_lock(&pktbuff_lock);*/
	memset(&pktdata, 0, sizeof(pktdata));
	switch (typeid) {
	/*infoframe pkt*/
	case PKT_TYPE_INFOFRAME_VSI:
		rx_pktdump_raw(&prx->vs_info);
		rx_pktdump_vsi(&prx->vs_info);
		rx_pr("-------->ex register set >>\n");
		rx_pkt_get_vsi_ex(&pktdata);
		rx_pktdump_vsi(&pktdata);
		break;
	case PKT_TYPE_INFOFRAME_AVI:
		rx_pktdump_raw(&prx->avi_info);
		rx_pktdump_avi(&prx->avi_info);
		rx_pr("-------->ex register set >>\n");
		rx_pkt_get_avi_ex(&pktdata);
		rx_pktdump_avi(&pktdata);
		break;
	case PKT_TYPE_INFOFRAME_SPD:
		rx_pktdump_raw(&prx->spd_info);
		rx_pktdump_spd(&prx->spd_info);
		break;
	case PKT_TYPE_INFOFRAME_AUD:
		rx_pktdump_raw(&prx->aud_pktinfo);
		rx_pktdump_aud(&prx->aud_pktinfo);
		rx_pr("-------->ex register set >>\n");
		rx_pkt_get_audif_ex(&pktdata);
		rx_pktdump_raw(&pktdata);
		break;
	case PKT_TYPE_INFOFRAME_MPEGSRC:
		rx_pktdump_raw(&prx->mpegs_info);
		/*rx_pktdump_mpeg(&prx->mpegs_info);*/
		break;
	case PKT_TYPE_INFOFRAME_NVBI:
		rx_pktdump_raw(&prx->ntscvbi_info);
		/*rx_pktdump_ntscvbi(&prx->ntscvbi_info);*/
		rx_pr("-------->ex register set >>\n");
		rx_pkt_get_ntscvbi_ex(&pktdata);
		rx_pktdump_raw(&pktdata);
		break;
	case PKT_TYPE_INFOFRAME_DRM:
		rx_pktdump_raw(&prx->drm_info);
		rx_pktdump_drm(&prx->drm_info);
		rx_pr("-------->ex register set >>\n");
		rx_pkt_get_drm_ex(&pktdata);
		rx_pktdump_drm(&pktdata);
		break;
	/*other pkt*/
	case PKT_TYPE_ACR:
		rx_pktdump_raw(&prx->acr_info);
		rx_pktdump_acr(&prx->acr_info);
		rx_pr("-------->ex register set >>\n");
		rx_pkt_get_acr_ex(&pktdata);
		rx_pktdump_acr(&pktdata);
		break;
	case PKT_TYPE_GCP:
		rx_pktdump_raw(&prx->gcp_info);
		rx_pr("-------->ex register set >>\n");
		rx_pkt_get_gcp_ex(&pktdata);
		rx_pktdump_raw(&pktdata);
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
		rx_pr("-------->ex register set >>\n");
		rx_pkt_get_gmd_ex(&pktdata);
		rx_pktdump_raw(&pktdata);
		break;
	case PKT_TYPE_AUD_META:
		rx_pktdump_raw(&prx->amp_info);
		rx_pr("-------->ex register set >>\n");
		rx_pkt_get_amp_ex(&pktdata);
		rx_pktdump_raw(&pktdata);
		break;
	case PKT_TYPE_EMP:
		rx_pktdump_emp(&prx->emp_info);
		rx_pktdump_raw(&prx->emp_info);
		break;
	default:
		rx_pr("warning: not support\n");
		rx_pr("vsi->0x81:Vendor-Specific infoframe\n");
		rx_pr("avi->0x82:Auxiliary video infoframe\n");
		rx_pr("spd->0x83:Source Product Description infoframe\n");
		rx_pr("aud->0x84:Audio infoframe\n");
		rx_pr("mpeg->0x85:MPEG infoframe\n");
		rx_pr("nvbi->0x86:NTSC VBI infoframe\n");
		rx_pr("drm->0x87:DRM infoframe\n");
		rx_pr("acr->0x01:audio clk regeneration\n");
		rx_pr("gcp->0x03\n");
		rx_pr("acp->0x04\n");
		rx_pr("isrc1->0x05\n");
		rx_pr("isrc2->0x06\n");
		rx_pr("gmd->0x0a\n");
		rx_pr("amp->0x0d\n");
		rx_pr("emp->0x7f:EMP\n");
		break;
	}

	/*mutex_unlock(&pktbuff_lock);*/
}

u32 rx_pkt_type_mapping(enum pkt_type_e pkt_type)
{
	struct pkt_typeregmap_st *ptab = pktmaping;
	u32 i = 0;
	u32 rt = 0;

	while (ptab[i].pkt_type != K_FLAG_TAB_END) {
		if (ptab[i].pkt_type == pkt_type)
			rt = ptab[i].reg_bit;
		i++;
	}
	return rt;
}

void rx_pkt_initial(void)
{
	memset(&rxpktsts, 0, sizeof(struct rxpkt_st));

	memset(&rx_pkt.vs_info, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.avi_info, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.spd_info, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.aud_pktinfo, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.mpegs_info, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.ntscvbi_info, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.drm_info, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.emp_info, 0, sizeof(struct pd_infoframe_s));

	memset(&rx_pkt.acr_info, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.gcp_info, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.acp_info, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.isrc1_info, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.isrc2_info, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.gameta_info, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.amp_info, 0, sizeof(struct pd_infoframe_s));
}

/*please ignore checksum byte*/
void rx_pkt_get_audif_ex(void *pktinfo)
{
	struct aud_infoframe_st *pkt = pktinfo;

	if (!pktinfo) {
		rx_pr("pkinfo null\n");
		return;
	}

	/*memset(pkt, 0, sizeof(struct aud_infoframe_st));*/

	pkt->pkttype = PKT_TYPE_INFOFRAME_AUD;
	pkt->version =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, MSK(8, 0));
	pkt->length =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, MSK(5, 8));
	pkt->checksum =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, MSK(8, 16));
	pkt->rsd = 0;

	/*get AudioInfo */
	pkt->coding_type =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, CODING_TYPE);
	pkt->ch_count =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, CHANNEL_COUNT);
	pkt->sample_frq =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, SAMPLE_FREQ);
	pkt->sample_size =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, SAMPLE_SIZE);
	pkt->fromat =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, AIF_DATA_BYTE_3);
	pkt->ca =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, CH_SPEAK_ALLOC);
	pkt->down_mix =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB1, DWNMIX_INHIBIT);
	pkt->level_shift_value =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB1, LEVEL_SHIFT_VAL);
	pkt->lfep =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB1, MSK(2, 8));
}

void rx_pkt_get_acr_ex(void *pktinfo)
{
	struct acr_pkt_st *pkt = pktinfo;
	u32 N, CTS;

	if (!pktinfo) {
		rx_pr("pkinfo null\n");
		return;
	}

	/*memset(pkt, 0, sizeof(struct acr_pkt_st));*/

	pkt->pkttype = PKT_TYPE_ACR;
	pkt->zero0 = 0x0;
	pkt->zero1 = 0x0;

	CTS = hdmirx_rd_dwc(DWC_PDEC_ACR_CTS);
	N = hdmirx_rd_dwc(DWC_PDEC_ACR_N);

	pkt->sbpkt1.SB1_CTS_H = ((CTS >> 16) & 0xf);
	pkt->sbpkt1.SB2_CTS_M = ((CTS >> 8) & 0xff);
	pkt->sbpkt1.SB3_CTS_L = (CTS & 0xff);
	pkt->sbpkt1.SB4_N_H = ((N >> 16) & 0xf);
	pkt->sbpkt1.SB5_N_M = ((N >> 8) & 0xff);
	pkt->sbpkt1.SB6_N_L = (N & 0xff);

	pkt->sbpkt2.SB1_CTS_H = ((CTS >> 16) & 0xf);
	pkt->sbpkt2.SB2_CTS_M = ((CTS >> 8) & 0xff);
	pkt->sbpkt2.SB3_CTS_L = (CTS & 0xff);
	pkt->sbpkt2.SB4_N_H = ((N >> 16) & 0xf);
	pkt->sbpkt2.SB5_N_M = ((N >> 8) & 0xff);
	pkt->sbpkt2.SB6_N_L = (N & 0xff);

	pkt->sbpkt3.SB1_CTS_H = ((CTS >> 16) & 0xf);
	pkt->sbpkt3.SB2_CTS_M = ((CTS >> 8) & 0xff);
	pkt->sbpkt3.SB3_CTS_L = (CTS & 0xff);
	pkt->sbpkt3.SB4_N_H = ((N >> 16) & 0xf);
	pkt->sbpkt3.SB5_N_M = ((N >> 8) & 0xff);
	pkt->sbpkt3.SB6_N_L = (N & 0xff);

	pkt->sbpkt4.SB1_CTS_H = ((CTS >> 16) & 0xf);
	pkt->sbpkt4.SB2_CTS_M = ((CTS >> 8) & 0xff);
	pkt->sbpkt4.SB3_CTS_L = (CTS & 0xff);
	pkt->sbpkt4.SB4_N_H = ((N >> 16) & 0xf);
	pkt->sbpkt4.SB5_N_M = ((N >> 8) & 0xff);
	pkt->sbpkt4.SB6_N_L = (N & 0xff);
}

/*please ignore checksum byte*/
void rx_pkt_get_avi_ex(void *pktinfo)
{
	struct avi_infoframe_st *pkt = pktinfo;

	if (!pktinfo) {
		rx_pr("pkinfo null\n");
		return;
	}

	/*memset(pkt, 0, sizeof(struct avi_infoframe_st));*/

	pkt->pkttype = PKT_TYPE_INFOFRAME_AVI;
	pkt->version =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_HB, MSK(8, 0));
	pkt->length =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_HB, MSK(5, 8));

	pkt->checksum =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_HB, MSK(8, 16));
	/* AVI parameters */
	pkt->cont.v4.vic =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, VID_IDENT_CODE);
	pkt->cont.v4.pix_repeat =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_HB, PIX_REP_FACTOR);
	pkt->cont.v4.colorindicator =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, VIDEO_FORMAT);
	pkt->cont.v4.it_content =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, IT_CONTENT);
	pkt->cont.v4.pic_scaling =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, RGB_QUANT_RANGE);
	pkt->cont.v4.content_type =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_HB, MSK(2, 28));
	pkt->cont.v4.qt_range =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_HB, YUV_QUANT_RANGE);
	pkt->cont.v4.activeinfo =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, ACT_INFO_PRESENT);
	pkt->cont.v4.barinfo =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, BAR_INFO_VALID);
	pkt->cont.v4.scaninfo =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, SCAN_INFO);
	pkt->cont.v4.colorimetry =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, COLORIMETRY);
	pkt->cont.v4.pic_ration =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, PIC_ASPECT_RATIO);
	pkt->cont.v4.fmt_ration =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, ACT_ASPECT_RATIO);
	pkt->cont.v4.it_content =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, IT_CONTENT);
	pkt->cont.v4.ext_color =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, EXT_COLORIMETRY);
	pkt->cont.v4.pic_scaling =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, NON_UNIF_SCALE);

	pkt->line_num_end_topbar =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_TBB, LIN_END_TOP_BAR);
	pkt->line_num_start_btmbar =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_TBB, LIN_ST_BOT_BAR);
	pkt->pix_num_left_bar =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_LRB, PIX_END_LEF_BAR);
	pkt->pix_num_right_bar =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_LRB, PIX_ST_RIG_BAR);
}

void rx_pkt_get_spd_ex(void *pktinfo)
{
	struct spd_infoframe_st *pkt = pktinfo;
	int i = 0;

	if (!pktinfo) {
		rx_pr("pkinfo null\n");
		return;
	}

	pkt->pkttype = PKT_TYPE_INFOFRAME_SPD;
	pkt->pkttype = hdmirx_rd_cor(SPDRX_TYPE_DP2_IVCRX);
	if (pkt->pkttype != PKT_TYPE_INFOFRAME_SPD)
		rx_pr("wrong SPD\n");
	pkt->version = hdmirx_rd_cor(SPDRX_VERS_DP2_IVCRX);
	pkt->length = hdmirx_rd_cor(SPDRX_LENGTH_DP2_IVCRX);
	pkt->checksum = hdmirx_rd_cor(SPDRX_CHSUM_DP2_IVCRX);
	for (i = 0; i < 27; i++)
		pkt->des_u.data[i] = hdmirx_rd_cor(SPDRX_DBYTE1_DP2_IVCRX + i);
}

void rx_pkt_get_vsi_ex(void *pktinfo)
{
	struct vsi_infoframe_st *pkt = pktinfo;
	u32 st0;
	u32 st1;
	u8 tmp, i;
	u32 tmp32;

	if (!pktinfo) {
		rx_pr("pkinfo null\n");
		return;
	}
	if (rx.chip_id >= CHIP_ID_T7) {
		if (log_level & IRQ_LOG)
			rx_pr("type = %x\n", rx_vsif_type);
		/* must use if_else to decide priority */
		if (rx_vsif_type & VSIF_TYPE_DV15) {
			pkt->pkttype = PKT_TYPE_INFOFRAME_VSI;
			pkt->length = hdmirx_rd_cor(HF_VSIRX_LENGTH_DP3_IVCRX) & 0x1f;
			pkt->ieee = IEEE_DV15;
			tmp = hdmirx_rd_cor(HF_VSIRX_VERS_DP3_IVCRX);
			pkt->ver_st.version = tmp & 0x7f;
			pkt->ver_st.chgbit = tmp >> 7 & 1;
			pkt->checksum = hdmirx_rd_cor(HF_VSIRX_DBYTE0_DP3_IVCRX);
			tmp = hdmirx_rd_cor(HF_VSIRX_DBYTE4_DP3_IVCRX);
			pkt->sbpkt.vsi_dobv15.ll = tmp & 1;
			if (log_level & VSI_LOG)
				rx_pr("LL=%d\n", pkt->sbpkt.vsi_dobv15.ll);
			pkt->sbpkt.vsi_dobv15.dv_vs10_sig_type = tmp >> 1 & 0x0f;
			pkt->sbpkt.vsi_dobv15.source_dm_ver = tmp >> 5 & 7;
			tmp = hdmirx_rd_cor(HF_VSIRX_DBYTE5_DP3_IVCRX);
			pkt->sbpkt.vsi_dobv15.tmax_pq_hi = tmp & 0x0f;
			pkt->sbpkt.vsi_dobv15.aux_md = tmp >> 6 & 1;
			pkt->sbpkt.vsi_dobv15.bklt_md = tmp >> 7 & 1;
			tmp = hdmirx_rd_cor(HF_VSIRX_DBYTE6_DP3_IVCRX);
			pkt->sbpkt.vsi_dobv15.tmax_pq_lo = tmp;
			tmp = hdmirx_rd_cor(HF_VSIRX_DBYTE7_DP3_IVCRX);
			pkt->sbpkt.vsi_dobv15.aux_run_mode = tmp;
			tmp = hdmirx_rd_cor(HF_VSIRX_DBYTE8_DP3_IVCRX);
			pkt->sbpkt.vsi_dobv15.aux_run_ver = tmp;
			tmp = hdmirx_rd_cor(HF_VSIRX_DBYTE9_DP3_IVCRX);
			pkt->sbpkt.vsi_dobv15.aux_debug = tmp;
			tmp = hdmirx_rd_cor(HF_VSIRX_DBYTE10_DP3_IVCRX);
			pkt->sbpkt.vsi_dobv15.content_type = tmp;
			for (i = 0; i < 17; i++) {
				tmp = hdmirx_rd_cor(HF_VSIRX_DBYTE11_DP3_IVCRX + i);
				pkt->sbpkt.vsi_dobv15.data[i] = tmp;
			}
			if (rx_vsif_type & VSIF_TYPE_HDMI21) {
				tmp = hdmirx_rd_cor(RX_UNREC_BYTE9_DP2_IVCRX);
				if ((tmp >> 1) & 1)
					pkt->ieee = IEEE_DV_PLUS_ALLM;
			}
			tmp32 = hdmirx_rd_cor(HF_VSIRX_DBYTE4_DP3_IVCRX);
			tmp32 |= hdmirx_rd_cor(HF_VSIRX_DBYTE5_DP3_IVCRX) << 8;
			tmp32 |= hdmirx_rd_cor(HF_VSIRX_DBYTE6_DP3_IVCRX) << 16;
			tmp32 |= hdmirx_rd_cor(HF_VSIRX_DBYTE7_DP3_IVCRX) << 24;
			pkt->sbpkt.payload.data[0] = tmp32;

			tmp32 = hdmirx_rd_cor(HF_VSIRX_DBYTE8_DP3_IVCRX);
			tmp32 |= hdmirx_rd_cor(HF_VSIRX_DBYTE9_DP3_IVCRX) << 8;
			tmp32 |= hdmirx_rd_cor(HF_VSIRX_DBYTE10_DP3_IVCRX) << 16;
			tmp32 |= hdmirx_rd_cor(HF_VSIRX_DBYTE11_DP3_IVCRX) << 24;
			pkt->sbpkt.payload.data[1] = tmp32;
		} else if (rx_vsif_type & VSIF_TYPE_HDR10P) {
			pkt->pkttype = PKT_TYPE_INFOFRAME_VSI;
			pkt->length = hdmirx_rd_cor(AUDRX_TYPE_DP2_IVCRX) & 0x1f;
			pkt->ieee = IEEE_HDR10PLUS;
			for (i = 0; i < 24; i++) {
				tmp = hdmirx_rd_cor(AUDRX_DBYTE4_DP2_IVCRX + i);
				pkt->sbpkt.vsi_st.data[i] = tmp;
			}
		} else if (rx_vsif_type & VSIF_TYPE_HDMI14) {
			pkt->pkttype = PKT_TYPE_INFOFRAME_VSI;
			pkt->length = hdmirx_rd_cor(VSIRX_LENGTH_DP3_IVCRX) & 0x1f;
			pkt->ver_st.version = 1;//hdmirx_rd_cor(VSIRX_VERS_DP3_IVCRX);
			pkt->ieee = IEEE_VSI14;
			for (i = 0; i < 24; i++) {
				tmp = hdmirx_rd_cor(VSIRX_DBYTE4_DP3_IVCRX + i);
				pkt->sbpkt.vsi_st.data[i] = tmp;
			}
		} else if (rx_vsif_type & VSIF_TYPE_HDMI21) {
			pkt->pkttype = PKT_TYPE_INFOFRAME_VSI;
			pkt->ieee = IEEE_VSI21;
			pkt->sbpkt.vsi_st_21.ver = 1;
			tmp = hdmirx_rd_cor(RX_UNREC_BYTE9_DP2_IVCRX);
			pkt->sbpkt.vsi_st_21.valid_3d = tmp & 1;
			pkt->sbpkt.vsi_st_21.allm_mode = (tmp >> 1) & 1;
			pkt->sbpkt.vsi_st_21.ccbpc = (tmp >> 4) & 0x0f;
			for (i = 0; i < 21; i++) {
				tmp = hdmirx_rd_cor(HF_VSIRX_DBYTE10_DP3_IVCRX + i);
				pkt->sbpkt.vsi_st_21.data[i] = tmp;
			}
		}
	} else if (rx.chip_id != CHIP_ID_TXHD) {
		st0 = hdmirx_rd_dwc(DWC_PDEC_VSI_ST0);
		st1 = hdmirx_rd_dwc(DWC_PDEC_VSI_ST1);
		pkt->pkttype = PKT_TYPE_INFOFRAME_VSI;
		pkt->length = st1 & 0x1f;
		pkt->checksum = (st0 >> 24) & 0xff;
		pkt->ieee = st0 & 0xffffff;
		pkt->ver_st.version = 1;
		pkt->ver_st.chgbit = 0;
		pkt->sbpkt.payload.data[0] =
			hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD0);
		pkt->sbpkt.payload.data[1] =
			hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD1);
		pkt->sbpkt.payload.data[2] =
			hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD2);
		pkt->sbpkt.payload.data[3] =
			hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD3);
		pkt->sbpkt.payload.data[4] =
			hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD4);
		pkt->sbpkt.payload.data[5] =
			hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD5);
	}
}

/*return 32 byte data , data struct see register spec*/
void rx_pkt_get_amp_ex(void *pktinfo)
{
	struct pd_infoframe_s *pkt = pktinfo;
	u32 HB;

	if (!pktinfo) {
		rx_pr("pkinfo null\n");
		return;
	}

	/*memset(pkt, 0, sizeof(struct pd_infoframe_s));*/
	if (rx.chip_id != CHIP_ID_TXHD) {
		HB = hdmirx_rd_dwc(DWC_PDEC_AMP_HB);
		pkt->HB = (HB << 8) | PKT_TYPE_AUD_META;
		pkt->PB0 = hdmirx_rd_dwc(DWC_PDEC_AMP_PB0);
		pkt->PB1 = hdmirx_rd_dwc(DWC_PDEC_AMP_PB1);
		pkt->PB2 = hdmirx_rd_dwc(DWC_PDEC_AMP_PB2);
		pkt->PB3 = hdmirx_rd_dwc(DWC_PDEC_AMP_PB3);
		pkt->PB4 = hdmirx_rd_dwc(DWC_PDEC_AMP_PB4);
		pkt->PB5 = hdmirx_rd_dwc(DWC_PDEC_AMP_PB5);
		pkt->PB6 = hdmirx_rd_dwc(DWC_PDEC_AMP_PB6);
	}
}

void rx_pkt_get_gmd_ex(void *pktinfo)
{
	struct gamutmeta_pkt_st *pkt = pktinfo;
	u32 HB, PB;

	if (!pktinfo) {
		rx_pr("pkinfo null\n");
		return;
	}

	/*memset(pkt, 0, sizeof(struct gamutmeta_pkt_st));*/

	pkt->pkttype = PKT_TYPE_GAMUT_META;

	HB = hdmirx_rd_dwc(DWC_PDEC_GMD_HB);
	PB = hdmirx_rd_dwc(DWC_PDEC_GMD_PB);

	/*3:0*/
	pkt->affect_seq_num = (HB & 0xf);
	/*6:4*/
	pkt->gbd_profile = ((HB >> 4) & 0x7);
	/*7*/
	pkt->next_feild = ((HB >> 7) & 0x1);
	/*11:8*/
	pkt->cur_seq_num = ((HB >> 8) & 0xf);
	/*13:12*/
	pkt->pkt_seq = ((HB >> 12) & 0x3);
	/*15*/
	pkt->no_cmt_gbd = ((HB >> 15) & 0x1);

	pkt->sbpkt.p1_profile.gbd_length_l = (PB & 0xff);
	pkt->sbpkt.p1_profile.gbd_length_h = ((PB >> 8) & 0xff);
	pkt->sbpkt.p1_profile.checksum = ((PB >> 16) & 0xff);
}

void rx_pkt_get_gcp_ex(void *pktinfo)
{
	struct gcp_pkt_st *pkt = pktinfo;
	u32 gcpavmute;

	if (!pktinfo) {
		rx_pr("pkinfo null\n");
		return;
	}

	/*memset(pkt, 0, sizeof(struct gcp_pkt_st));*/

	gcpavmute = hdmirx_rd_dwc(DWC_PDEC_GCP_AVMUTE);

	pkt->pkttype = PKT_TYPE_GCP;
	pkt->sbpkt.clr_avmute = (gcpavmute & 0x01);
	pkt->sbpkt.set_avmute = ((gcpavmute >> 1) & 0x01);
	pkt->sbpkt.colordepth = ((gcpavmute >> 4) & 0xf);
	pkt->sbpkt.pixelpkgphase = ((gcpavmute >> 8) & 0xf);
	pkt->sbpkt.def_phase = ((gcpavmute >> 12) & 0x01);
}

void rx_pkt_get_drm_ex(void *pktinfo)
{
	struct drm_infoframe_st *drmpkt = pktinfo;

	if (!pktinfo) {
		rx_pr("pkinfo null\n");
		return;
	}

	drmpkt->pkttype = PKT_TYPE_INFOFRAME_DRM;
	if (rx.chip_id != CHIP_ID_TXHD) {
		drmpkt->length = (hdmirx_rd_dwc(DWC_PDEC_DRM_HB) >> 8);
		drmpkt->version = hdmirx_rd_dwc(DWC_PDEC_DRM_HB);

		drmpkt->des_u.payload[0] =
			hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD0);
		drmpkt->des_u.payload[1] =
			hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD1);
		drmpkt->des_u.payload[2] =
			hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD2);
		drmpkt->des_u.payload[3] =
			hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD3);
		drmpkt->des_u.payload[4] =
			hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD4);
		drmpkt->des_u.payload[5] =
			hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD5);
		drmpkt->des_u.payload[6] =
			hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD6);
	}
}

/*return 32 byte data , data struct see register spec*/
void rx_pkt_get_ntscvbi_ex(void *pktinfo)
{
	struct pd_infoframe_s *pkt = pktinfo;

	if (!pktinfo) {
		rx_pr("pkinfo null\n");
		return;
	}

	if (rx.chip_id != CHIP_ID_TXHD &&
		rx.chip_id != CHIP_ID_T5D) {
		/*byte 0 , 1*/
		pkt->HB = hdmirx_rd_dwc(DWC_PDEC_NTSCVBI_HB);
		pkt->PB0 = hdmirx_rd_dwc(DWC_PDEC_NTSCVBI_PB0);
		pkt->PB1 = hdmirx_rd_dwc(DWC_PDEC_NTSCVBI_PB1);
		pkt->PB2 = hdmirx_rd_dwc(DWC_PDEC_NTSCVBI_PB2);
		pkt->PB3 = hdmirx_rd_dwc(DWC_PDEC_NTSCVBI_PB3);
		pkt->PB4 = hdmirx_rd_dwc(DWC_PDEC_NTSCVBI_PB4);
		pkt->PB5 = hdmirx_rd_dwc(DWC_PDEC_NTSCVBI_PB5);
		pkt->PB6 = hdmirx_rd_dwc(DWC_PDEC_NTSCVBI_PB6);
	}
}

u32 rx_pkt_chk_attach_vsi(void)
{
	if (rxpktsts.pkt_attach_vsi)
		return 1;
	else
		return 0;
}

void rx_pkt_clr_attach_vsi(void)
{
	rxpktsts.pkt_attach_vsi = 0;
}

u32 rx_pkt_chk_updated_spd(void)
{
	if (rxpktsts.pkt_spd_updated)
		return 1;
	else
		return 0;
}

void rx_pkt_clr_updated_spd(void)
{
	rxpktsts.pkt_spd_updated = 0;
}

u32 rx_pkt_chk_attach_drm(void)
{
	if (rxpktsts.pkt_attach_drm)
		return 1;
	else
		return 0;
}

void rx_pkt_clr_attach_drm(void)
{
	rxpktsts.pkt_attach_drm = 0;
}

u32 rx_pkt_chk_busy_vsi(void)
{
	if (rxpktsts.pkt_op_flag & PKT_OP_VSI)
		return 1;
	else
		return 0;
}

u32 rx_pkt_chk_busy_drm(void)
{
	if (rxpktsts.pkt_op_flag & PKT_OP_DRM)
		return 1;
	else
		return 0;
}

/*  version2.86 ieee-0x00d046, length 0x1B
 *	pb4 bit[0]: Low_latency
 *	pb4 bit[1]: Dolby_vision_signal
 *	pb5 bit[7]: Backlt_Ctrl_MD_Present
 *	pb5 bit[3:0] | pb6 bit[7:0]: Eff_tmax_PQ
 *
 *	version2.6 ieee-0x000c03,
 *	start: length 0x18
 *	stop: 0x05,0x04
 */
void rx_get_vsi_info(void)
{
	struct vsi_infoframe_st *pkt;
	unsigned int tmp;
	static u32 num;

	rx.vs_info_details.vsi_state = E_VSI_NULL;
	rx.vs_info_details._3d_structure = 0;
	rx.vs_info_details._3d_ext_data = 0;
	rx.vs_info_details.low_latency = false;
	rx.vs_info_details.backlt_md_bit = false;
	rx.vs_info_details.allm_mode = false;
	rx.vs_info_details.dolby_vision_flag = DV_NULL;
	rx.vs_info_details.hdr10plus = false;
	rx.vs_info_details.emp_pkt_cnt = rx.empbuff.emppktcnt;
	rx.empbuff.emppktcnt = 0;
	pkt = (struct vsi_infoframe_st *)&rx_pkt.vs_info;

	if (log_level & PACKET_LOG)
		rx_pr("ieee-%x\n", pkt->ieee);
	switch (pkt->ieee) {
	case IEEE_DV15:
		/* dolbyvision 1.5 */
		rx.vs_info_details.vsi_state = E_VSI_DV15;
		if (pkt->length != E_PKT_LENGTH_27) {
			if (log_level & PACKET_LOG)
				rx_pr("vsi dv15 length err\n");
			//dv vsif
			rx.vs_info_details.dolby_vision_flag = DV_VSIF;
			break;
		}
		tmp = pkt->sbpkt.payload.data[0] & _BIT(1);
		if (tmp)
			rx.vs_info_details.dolby_vision_flag = DV_VSIF;
		tmp = pkt->sbpkt.payload.data[0] & _BIT(0);
		rx.vs_info_details.low_latency = tmp ? true : false;
		tmp = pkt->sbpkt.payload.data[0] >> 15 & 0x01;
		rx.vs_info_details.backlt_md_bit = tmp ? true : false;
		if (tmp) {
			tmp = (pkt->sbpkt.payload.data[0] >> 16 & 0xff) |
			      (pkt->sbpkt.payload.data[0] & 0xf00);
			rx.vs_info_details.eff_tmax_pq = tmp;
		}
		tmp = (pkt->sbpkt.payload.data[1] >> 16 & 0xf);
		if (tmp == 2)
			rx.vs_info_details.allm_mode = true;
		break;
	//====================for dv5.0 ====================
	case IEEE_DV_PLUS_ALLM:
		//same as DV15. but allm is true because vsif21 allm is on
		//new requirement from dv5.0
		/* dolbyvision 1.5 */
		rx.vs_info_details.vsi_state = E_VSI_DV15;
		if (pkt->length != E_PKT_LENGTH_27) {
			if (log_level & PACKET_LOG)
				rx_pr("vsi dv15 length err\n");
			//dv vsif
			rx.vs_info_details.dolby_vision_flag = DV_VSIF;
			break;
		}
		tmp = pkt->sbpkt.payload.data[0] & _BIT(1);
		if (tmp)
			rx.vs_info_details.dolby_vision_flag = DV_VSIF;
		tmp = pkt->sbpkt.payload.data[0] & _BIT(0);
		rx.vs_info_details.low_latency = tmp ? true : false;
		tmp = pkt->sbpkt.payload.data[0] >> 15 & 0x01;
		rx.vs_info_details.backlt_md_bit = tmp ? true : false;
		if (tmp) {
			tmp = (pkt->sbpkt.payload.data[0] >> 16 & 0xff) |
			      (pkt->sbpkt.payload.data[0] & 0xf00);
			rx.vs_info_details.eff_tmax_pq = tmp;
		}
		//tmp = (pkt->sbpkt.payload.data[1] >> 16 & 0xf);
		//if (tmp == 2)
		rx.vs_info_details.allm_mode = true;
		pkt->ieee = IEEE_DV15;
		break;
	//====================for dv5.0 end=================
	case IEEE_VSI21:
		/* hdmi2.1 */
		rx.vs_info_details.vsi_state = E_VSI_VSI21;
		tmp = pkt->sbpkt.payload.data[0] & _BIT(9);
		rx.vs_info_details.allm_mode = tmp ? true : false;
		break;
	case IEEE_HDR10PLUS:
		/* HDR10+ */
		rx.vs_info_details.vsi_state = E_VSI_HDR10PLUS;
		if (pkt->length != E_PKT_LENGTH_27 ||
		    pkt->pkttype != 0x01 ||
		    pkt->ver_st.version != 0x01 ||
		    ((pkt->sbpkt.payload.data[1] >> 6) & 0x03) != 0x01)
			if (log_level & PACKET_LOG)
				rx_pr("vsi hdr10+ length err\n");
		/* consider hdr10+ is true when IEEE matched */
		rx.vs_info_details.hdr10plus = true;
		break;
	case IEEE_VSI14:
		/* dolbyvision1.0 */
		rx.vs_info_details.vsi_state = E_VSI_4K3D;
		if (pkt->length == E_PKT_LENGTH_24) {
			rx.vs_info_details.dolby_vision_flag = DV_VSIF;
			if ((pkt->sbpkt.payload.data[0] & 0xffff) == 0)
				pkt->sbpkt.payload.data[0] = 0xffff;
			rx.vs_info_details.vsi_state = E_VSI_DV10;
		} else if ((pkt->length == E_PKT_LENGTH_5) &&
			(pkt->sbpkt.payload.data[0] & 0xffff)) {
			rx.vs_info_details.dolby_vision_flag = DV_NULL;
		} else if ((pkt->length == E_PKT_LENGTH_4) &&
			   ((pkt->sbpkt.payload.data[0] & 0xff) == 0)) {
			rx.vs_info_details.dolby_vision_flag = DV_NULL;
		} else {
			if (pkt->sbpkt.vsi_3dext.vdfmt == VSI_FORMAT_3D_FORMAT) {
				rx.vs_info_details._3d_structure =
					pkt->sbpkt.vsi_3dext.threed_st;
				rx.vs_info_details._3d_ext_data =
					pkt->sbpkt.vsi_3dext.threed_ex;
			if (log_level & VSI_LOG)
				rx_pr("struct_3d:%d, struct_3d_ext:%d\n",
				      pkt->sbpkt.vsi_3dext.threed_st,
				      pkt->sbpkt.vsi_3dext.threed_ex);
			}
			rx.vs_info_details.dolby_vision_flag = DV_NULL;
		}
		break;
	default:
		break;
	}
	if (rx.vs_info_details.emp_pkt_cnt &&
	    rx.empbuff.emp_tagid == IEEE_DV15) {
		//pkt->ieee = rx.empbuff.emp_tagid;
		rx.vs_info_details.dolby_vision_flag = DV_EMP;
		rx.vs_info_details.vsi_state = E_VSI_DV15;
		pkt->sbpkt.vsi_dobv15.dv_vs10_sig_type = 1;
		pkt->sbpkt.vsi_dobv15.ll =
			(rx.empbuff.data_ver & 1) ? 1 : 0;
		pkt->sbpkt.vsi_dobv15.bklt_md = 0;
		pkt->sbpkt.vsi_dobv15.aux_md = 0;
		pkt->sbpkt.vsi_dobv15.content_type =
			rx.empbuff.emp_content_type;
		if (pkt->sbpkt.vsi_dobv15.content_type == 2)
			rx.vs_info_details.allm_mode = true;
		if (log_level & PACKET_LOG)
			rx_pr("dv_emp-allm-%d\n", rx.vs_info_details.allm_mode);
	} else {
		if (num == rxpktsts.pkt_cnt_vsi)
			pkt->ieee = 0;
		num = rxpktsts.pkt_cnt_vsi;
	}
	rx.empbuff.emp_tagid = 0;
	/* pkt->ieee = 0; */
	/* memset(&rx_pkt.vs_info, 0, sizeof(struct pd_infoframe_s)); */
}

void rx_pkt_buffclear(enum pkt_type_e pkt_type)
{
	struct packet_info_s *prx = &rx_pkt;
	void *pktinfo;

	if (pkt_type == PKT_TYPE_INFOFRAME_VSI) {
		pktinfo = &prx->vs_info;
	} else if (pkt_type == PKT_TYPE_INFOFRAME_AVI) {
		pktinfo = &prx->avi_info;
	} else if (pkt_type == PKT_TYPE_INFOFRAME_SPD) {
		pktinfo = &prx->spd_info;
	} else if (pkt_type == PKT_TYPE_INFOFRAME_AUD) {
		pktinfo = &prx->aud_pktinfo;
	} else if (pkt_type == PKT_TYPE_INFOFRAME_MPEGSRC) {
		pktinfo = &prx->mpegs_info;
	} else if (pkt_type == PKT_TYPE_INFOFRAME_NVBI) {
		pktinfo = &prx->ntscvbi_info;
	} else if (pkt_type == PKT_TYPE_INFOFRAME_DRM) {
		pktinfo = &prx->drm_info;
	} else if (pkt_type == PKT_TYPE_EMP) {
		pktinfo = &prx->emp_info;
	} else if (pkt_type == PKT_TYPE_ACR) {
		pktinfo = &prx->acr_info;
	} else if (pkt_type == PKT_TYPE_GCP) {
		pktinfo = &prx->gcp_info;
	} else if (pkt_type == PKT_TYPE_ACP) {
		pktinfo = &prx->acp_info;
	} else if (pkt_type == PKT_TYPE_ISRC1) {
		pktinfo = &prx->isrc1_info;
	} else if (pkt_type == PKT_TYPE_ISRC2) {
		pktinfo = &prx->isrc2_info;
	} else if (pkt_type == PKT_TYPE_GAMUT_META) {
		pktinfo = &prx->gameta_info;
	} else if (pkt_type == PKT_TYPE_AUD_META) {
		pktinfo = &prx->amp_info;
	} else {
		rx_pr("err type:0x%x\n", pkt_type);
		return;
	}
	memset(pktinfo, 0, sizeof(struct pd_infoframe_s));
}

void rx_pkt_content_chk_en(u32 enable)
{
	rx_pr("content chk:%d\n", enable);
	if (enable) {
		if (!pkt_testbuff)
			pkt_testbuff = kmalloc(sizeof(*pkt_testbuff),
					       GFP_KERNEL);
		if (pkt_testbuff) {
			memset(pkt_testbuff, 0,
			       sizeof(*pkt_testbuff));
			rxpktsts.pkt_chk_flg = true;
		} else {
			rx_pr("kmalloc fail for pkt chk\n");
		}
	} else {
		if (rxpktsts.pkt_chk_flg)
			kfree(pkt_testbuff);
		rxpktsts.pkt_chk_flg = false;
		pkt_testbuff = NULL;
	}
}

/*
 * read pkt data from pkt fifo or external register set
 */
void rx_pkt_set_fifo_pri(u32 pri)
{
	gpkt_fifo_pri = pri;
	rx_pr("gpkt_fifo_pri=%d\n", pri);
}

u32 rx_pkt_get_fifo_pri(void)
{
	return gpkt_fifo_pri;
}

void rx_pkt_check_content(void)
{
	struct packet_info_s *cur_pkt = &rx_pkt;
	struct st_pkt_test_buff *pre_pkt;
	struct rxpkt_st *pktsts;
	static u32 acr_pkt_cnt;
	static u32 ex_acr_pkt_cnt;
	struct pd_infoframe_s pktdata;

	pre_pkt = pkt_testbuff;
	pktsts = &rxpktsts;

	if (!pre_pkt)
		return;

	if (rxpktsts.pkt_chk_flg) {
		/*compare vsi*/
		if (pktsts->pkt_cnt_vsi) {
			if (memcmp((char *)&pre_pkt->vs_info,
				   (char *)&cur_pkt->vs_info,
				   sizeof(struct pd_infoframe_s)) != 0) {
				rx_pr(" vsi chg\n");
				/*dump pre data*/
				rx_pktdump_raw(&pre_pkt->vs_info);
				/*dump current data*/
				rx_pktdump_raw(&cur_pkt->vs_info);
				/*save current*/
				memcpy(&pre_pkt->vs_info,
				       &cur_pkt->vs_info,
				       sizeof(struct pd_infoframe_s));
			}
		}

		/*compare avi*/
		if (pktsts->pkt_cnt_avi) {
			if (memcmp((char *)&pre_pkt->avi_info,
				   (char *)&cur_pkt->avi_info,
				   sizeof(struct pd_infoframe_s)) != 0) {
				rx_pr(" avi chg\n");
				rx_pktdump_raw(&pre_pkt->avi_info);
				rx_pktdump_raw(&cur_pkt->avi_info);
				memcpy(&pre_pkt->avi_info,
				       &cur_pkt->avi_info,
				       sizeof(struct pd_infoframe_s));
			}
		}

		/*compare spd*/
		if (pktsts->pkt_cnt_spd) {
			if (memcmp((char *)&pre_pkt->spd_info,
				   (char *)&cur_pkt->spd_info,
				   sizeof(struct pd_infoframe_s)) != 0) {
				rx_pr(" spd chg\n");
				rx_pktdump_raw(&pre_pkt->spd_info);
				rx_pktdump_raw(&cur_pkt->spd_info);
				memcpy(&pre_pkt->spd_info,
				       &cur_pkt->spd_info,
				       sizeof(struct pd_infoframe_s));
			}
		}
		/*compare aud_pktinfo*/
		if (pktsts->pkt_cnt_audif) {
			if (memcmp((char *)&pre_pkt->aud_pktinfo,
				   (char *)&cur_pkt->aud_pktinfo,
				   sizeof(struct pd_infoframe_s)) != 0) {
				rx_pr(" audif chg\n");
				rx_pktdump_raw(&pre_pkt->aud_pktinfo);
				rx_pktdump_raw(&cur_pkt->aud_pktinfo);
				memcpy(&pre_pkt->aud_pktinfo,
				       &cur_pkt->aud_pktinfo,
				       sizeof(struct pd_infoframe_s));
			}
		}
		/*compare mpegs_info*/
		if (pktsts->pkt_cnt_mpeg) {
			if (memcmp((char *)&pre_pkt->mpegs_info,
				   (char *)&cur_pkt->mpegs_info,
				   sizeof(struct pd_infoframe_s)) != 0) {
				rx_pktdump_raw(&pre_pkt->mpegs_info);
				rx_pktdump_raw(&cur_pkt->mpegs_info);
				memcpy(&pre_pkt->mpegs_info,
				       &cur_pkt->mpegs_info,
				       sizeof(struct pd_infoframe_s));
			}
		}
		/*compare ntscvbi_info*/
		if (pktsts->pkt_cnt_nvbi) {
			if (memcmp((char *)&pre_pkt->ntscvbi_info,
				   (char *)&cur_pkt->ntscvbi_info,
				   sizeof(struct pd_infoframe_s)) != 0) {
				rx_pr(" nvbi chg\n");
				rx_pktdump_raw(&pre_pkt->ntscvbi_info);
				rx_pktdump_raw(&cur_pkt->ntscvbi_info);
				memcpy(&pre_pkt->ntscvbi_info,
				       &cur_pkt->ntscvbi_info,
				       sizeof(struct pd_infoframe_s));
			}
		}
		/*compare drm_info*/
		if (pktsts->pkt_cnt_drm) {
			if (memcmp((char *)&pre_pkt->drm_info,
				   (char *)&cur_pkt->drm_info,
				   sizeof(struct pd_infoframe_s)) != 0) {
				rx_pr(" drm chg\n");
				rx_pktdump_raw(&pre_pkt->drm_info);
				rx_pktdump_raw(&cur_pkt->drm_info);
				memcpy(&pre_pkt->drm_info,
				       &cur_pkt->drm_info,
				       sizeof(struct pd_infoframe_s));
			}
		}
		/*compare acr_info*/
		if (pktsts->pkt_cnt_acr) {
			if (memcmp((char *)&pre_pkt->acr_info,
				   (char *)&cur_pkt->acr_info,
				   sizeof(struct pd_infoframe_s)) != 0) {
				if (acr_pkt_cnt++ > 100) {
					acr_pkt_cnt = 0;
					rx_pr(" acr chg\n");
					rx_pktdump_acr(&cur_pkt->acr_info);
				}
				/*save current*/
				memcpy(&pre_pkt->acr_info,
				       &cur_pkt->acr_info,
				       sizeof(struct pd_infoframe_s));
			}
		}
		/*compare gcp_info*/
		if (pktsts->pkt_cnt_gcp) {
			if (memcmp((char *)&pre_pkt->gcp_info,
				   (char *)&cur_pkt->gcp_info,
				   sizeof(struct pd_infoframe_s)) != 0) {
				rx_pr(" gcp chg\n");
				/*dump pre data*/
				rx_pktdump_raw(&pre_pkt->gcp_info);
				/*dump current data*/
				rx_pktdump_raw(&cur_pkt->gcp_info);
				/*save current*/
				memcpy(&pre_pkt->gcp_info,
				       &cur_pkt->gcp_info,
				       sizeof(struct pd_infoframe_s));
			}
		}
		/*compare acp_info*/
		if (pktsts->pkt_cnt_acp) {
			if (memcmp((char *)&pre_pkt->acp_info,
				   (char *)&cur_pkt->acp_info,
				   sizeof(struct pd_infoframe_s)) != 0) {
				rx_pr(" acp chg\n");
				/*dump pre data*/
				rx_pktdump_raw(&pre_pkt->acp_info);
				/*dump current data*/
				rx_pktdump_raw(&cur_pkt->acp_info);
				/*save current*/
				memcpy(&pre_pkt->acp_info,
				       &cur_pkt->acp_info,
				       sizeof(struct pd_infoframe_s));
			}
		}
		/*compare isrc1_info*/
		if (pktsts->pkt_cnt_isrc1) {
			if (memcmp((char *)&pre_pkt->isrc1_info,
				   (char *)&cur_pkt->isrc1_info,
				   sizeof(struct pd_infoframe_s)) != 0) {
				rx_pr(" isrc2 chg\n");
				/*dump pre data*/
				rx_pktdump_raw(&pre_pkt->isrc1_info);
				/*dump current data*/
				rx_pktdump_raw(&cur_pkt->isrc1_info);
				/*save current*/
				memcpy(&pre_pkt->isrc1_info,
				       &cur_pkt->isrc1_info,
				       sizeof(struct pd_infoframe_s));
			}
		}
		/*compare isrc2_info*/
		if (pktsts->pkt_cnt_isrc2) {
			if (memcmp((char *)&pre_pkt->isrc2_info,
				   (char *)&cur_pkt->isrc2_info,
				   sizeof(struct pd_infoframe_s)) != 0) {
				rx_pr(" isrc1 chg\n");
				/*dump pre data*/
				rx_pktdump_raw(&pre_pkt->isrc2_info);
				/*dump current data*/
				rx_pktdump_raw(&cur_pkt->isrc2_info);
				/*save current*/
				memcpy(&pre_pkt->isrc2_info,
				       &cur_pkt->isrc2_info,
				       sizeof(struct pd_infoframe_s));
			}
		}
		/*compare gameta_info*/
		if (pktsts->pkt_cnt_gameta) {
			if (memcmp((char *)&pre_pkt->gameta_info,
				   (char *)&cur_pkt->gameta_info,
				   sizeof(struct pd_infoframe_s)) != 0) {
				rx_pr(" gmd chg\n");
				/*dump pre data*/
				rx_pktdump_raw(&pre_pkt->gameta_info);
				/*dump current data*/
				rx_pktdump_raw(&cur_pkt->gameta_info);
				/*save current*/
				memcpy(&pre_pkt->gameta_info,
				       &cur_pkt->gameta_info,
				       sizeof(struct pd_infoframe_s));
			}
		}
		/*compare amp_info*/
		if (pktsts->pkt_cnt_amp) {
			if (memcmp((char *)&pre_pkt->amp_info,
				   (char *)&cur_pkt->amp_info,
				   sizeof(struct pd_infoframe_s)) != 0) {
				rx_pr(" amp chg\n");
				/*dump pre data*/
				rx_pktdump_raw(&pre_pkt->amp_info);
				/*dump current data*/
				rx_pktdump_raw(&cur_pkt->amp_info);
				/*save current*/
				memcpy(&pre_pkt->amp_info,
				       &cur_pkt->amp_info,
				       sizeof(struct pd_infoframe_s));
			}
		}

		rx_pkt_get_audif_ex(&pktdata);
		if (memcmp((char *)&pre_pkt->ex_audif,
			   (char *)&pktdata,
			   sizeof(struct pd_infoframe_s)) != 0) {
			rx_pr(" ex_audif chg\n");
			memcpy(&pre_pkt->ex_audif, &pktdata,
			       sizeof(struct pd_infoframe_s));
		}

		rx_pkt_get_acr_ex(&pktdata);
		if (memcmp((char *)&pre_pkt->ex_acr,
			   (char *)&pktdata,
			   sizeof(struct pd_infoframe_s)) != 0) {
			if (ex_acr_pkt_cnt++ > 100) {
				ex_acr_pkt_cnt = 0;
				rx_pr(" ex_acr chg\n");
			}
			memcpy(&pre_pkt->ex_acr, &pktdata,
			       sizeof(struct pd_infoframe_s));
		}

		rx_pkt_get_avi_ex(&pktdata);
		if (memcmp((char *)&pre_pkt->ex_avi,
			   (char *)&pktdata,
			   sizeof(struct pd_infoframe_s)) != 0) {
			rx_pr(" ex_avi chg\n");
			memcpy(&pre_pkt->ex_avi, &pktdata,
			       sizeof(struct pd_infoframe_s));
		}

		rx_pkt_get_vsi_ex(&pktdata);
		if (memcmp((char *)&pre_pkt->ex_vsi,
			   (char *)&pktdata,
			   sizeof(struct pd_infoframe_s)) != 0) {
			rx_pr(" ex_vsi chg\n");
			memcpy(&pre_pkt->ex_vsi, &pktdata,
			       sizeof(struct pd_infoframe_s));
		}

		rx_pkt_get_amp_ex(&pktdata);
		if (memcmp((char *)&pre_pkt->ex_amp,
			   (char *)&pktdata,
			   sizeof(struct pd_infoframe_s)) != 0) {
			rx_pr(" ex_amp chg\n");
			memcpy(&pre_pkt->ex_amp, &pktdata,
			       sizeof(struct pd_infoframe_s));
		}

		rx_pkt_get_gmd_ex(&pktdata);
		if (memcmp((char *)&pre_pkt->ex_gmd,
			   (char *)&pktdata,
			   sizeof(struct pd_infoframe_s)) != 0) {
			rx_pr(" ex_gmd chg\n");
			memcpy(&pre_pkt->ex_gmd, &pktdata,
			       sizeof(struct pd_infoframe_s));
		}

		rx_pkt_get_gcp_ex(&pktdata);
		if (memcmp((char *)&pre_pkt->ex_gcp,
			   (char *)&pktdata,
			   sizeof(struct pd_infoframe_s)) != 0) {
			rx_pr(" ex_gcp chg\n");
			memcpy(&pre_pkt->ex_gcp, &pktdata,
			       sizeof(struct pd_infoframe_s));
		}

		rx_pkt_get_drm_ex(&pktdata);
		if (memcmp((char *)&pre_pkt->ex_drm,
			   (char *)&pktdata,
			   sizeof(struct pd_infoframe_s)) != 0) {
			rx_pr(" ex_drm chg\n");
			memcpy(&pre_pkt->ex_drm, &pktdata,
			       sizeof(struct pd_infoframe_s));
		}

		rx_pkt_get_ntscvbi_ex(&pktdata);
		if (memcmp((char *)&pre_pkt->ex_nvbi,
			   (char *)&pktdata,
			   sizeof(struct pd_infoframe_s)) != 0) {
			rx_pr(" ex_nvbi chg\n");
			memcpy(&pre_pkt->ex_nvbi, &pktdata,
			       sizeof(struct pd_infoframe_s));
		}
	}
}

bool is_new_visf_pkt_rcv(union infoframe_u *pktdata)
{
	struct vsi_infoframe_st *pkt;
	enum vsi_state_e new_pkt, old_pkt;
	bool allm_sts = false;

	pkt = (struct vsi_infoframe_st *)&rx_pkt.vs_info;
	/* old vsif pkt */
	if (pkt->ieee == IEEE_DV15)
		old_pkt = E_VSI_DV15;
	else if (pkt->ieee == IEEE_VSI21)
		old_pkt = E_VSI_VSI21;
	else if (pkt->ieee == IEEE_HDR10PLUS)
		old_pkt = E_VSI_HDR10PLUS;
	else if ((pkt->ieee == IEEE_VSI14) &&
		 (pkt->length == E_PKT_LENGTH_24))
		old_pkt = E_VSI_DV10;
	else
		old_pkt = E_VSI_4K3D;

	//====================for dv5.0=====================
	if (old_pkt == E_VSI_VSI21) {
		if (pkt->sbpkt.payload.data[0] & _BIT(9))
			allm_sts = true;
	}
	//====================for dv5.0 end=================
	pkt = (struct vsi_infoframe_st *)(pktdata);
	/* new vsif */
	if (pkt->ieee == IEEE_DV15)
		new_pkt = E_VSI_DV15;
	else if (pkt->ieee == IEEE_VSI21)
		new_pkt = E_VSI_VSI21;
	else if (pkt->ieee == IEEE_HDR10PLUS)
		new_pkt = E_VSI_HDR10PLUS;
	else if ((pkt->ieee == IEEE_VSI14) &&
		 (pkt->length == E_PKT_LENGTH_24))
		new_pkt = E_VSI_DV10;
	else
		new_pkt = E_VSI_4K3D;
	//====================for dv5.0=====================
	if (new_pkt == E_VSI_VSI21) {
		if (pkt->sbpkt.payload.data[0] & _BIT(9))
			allm_sts = true;
	}
	if ((old_pkt == E_VSI_VSI21 && new_pkt == E_VSI_DV15) ||
		(old_pkt == E_VSI_DV15 && new_pkt == E_VSI_VSI21))
		if (allm_sts)
			pkt->ieee = IEEE_DV_PLUS_ALLM;
	//====================for dv5.0 end=================
	if (new_pkt > old_pkt)
		return true;

	return false;
}

int rx_pkt_fifodecode(struct packet_info_s *prx,
		      union infoframe_u *pktdata,
		      struct rxpkt_st *pktsts)
{
	switch (pktdata->raw_infoframe.pkttype) {
	case PKT_TYPE_INFOFRAME_VSI:
		if (rxpktsts.dv_pkt_num > 0) {
			if (!is_new_visf_pkt_rcv(pktdata))
				break;
		} else {
			pktsts->pkt_attach_vsi++;
			pktsts->pkt_cnt_vsi++;
		}
		rxpktsts.dv_pkt_num++;
		pktsts->pkt_op_flag |= PKT_OP_VSI;
		memcpy(&prx->vs_info, pktdata,
		       sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_VSI;
		break;
	case PKT_TYPE_INFOFRAME_AVI:
		pktsts->pkt_cnt_avi++;
		pktsts->pkt_op_flag |= PKT_OP_AVI;
		/*memset(&prx->avi_info, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->avi_info, pktdata, sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_AVI;
		break;
	case PKT_TYPE_INFOFRAME_SPD:
		pktsts->pkt_cnt_spd++;
		pktsts->pkt_op_flag |= PKT_OP_SPD;
		/*memset(&prx->spd_info, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->spd_info, pktdata,
		       sizeof(struct spd_infoframe_st));
		pktsts->pkt_op_flag &= ~PKT_OP_SPD;
		break;
	case PKT_TYPE_INFOFRAME_AUD:
		pktsts->pkt_cnt_audif++;
		pktsts->pkt_op_flag |= PKT_OP_AIF;
		/*memset(&prx->aud_pktinfo, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->aud_pktinfo,
		       pktdata, sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_AIF;
		break;
	case PKT_TYPE_INFOFRAME_MPEGSRC:
		pktsts->pkt_cnt_mpeg++;
		pktsts->pkt_op_flag |= PKT_OP_MPEGS;
		/*memset(&prx->mpegs_info, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->mpegs_info, pktdata,
		       sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_MPEGS;
		break;
	case PKT_TYPE_INFOFRAME_NVBI:
		pktsts->pkt_cnt_nvbi++;
		pktsts->pkt_op_flag |= PKT_OP_NVBI;
		/* memset(&prx->ntscvbi_info, 0, */
			/* sizeof(struct pd_infoframe_s)); */
		memcpy(&prx->ntscvbi_info, pktdata,
		       sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_NVBI;
		break;
	case PKT_TYPE_INFOFRAME_DRM:
		pktsts->pkt_attach_drm++;
		pktsts->pkt_cnt_drm++;
		pktsts->pkt_op_flag |= PKT_OP_DRM;
		/*memset(&prx->drm_info, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->drm_info, pktdata, sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_DRM;
		break;
	case PKT_TYPE_ACR:
		pktsts->pkt_cnt_acr++;
		pktsts->pkt_op_flag |= PKT_OP_ACR;
		/*memset(&prx->acr_info, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->acr_info,
		       pktdata, sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_ACR;
		break;
	case PKT_TYPE_GCP:
		pktsts->pkt_cnt_gcp++;
		pktsts->pkt_op_flag |= PKT_OP_GCP;
		/*memset(&prx->gcp_info, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->gcp_info,
		       pktdata, sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_GCP;
		break;
	case PKT_TYPE_ACP:
		pktsts->pkt_cnt_acp++;
		pktsts->pkt_op_flag |= PKT_OP_ACP;
		/*memset(&prx->acp_info, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->acp_info,
		       pktdata, sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_ACP;
		break;
	case PKT_TYPE_ISRC1:
		pktsts->pkt_cnt_isrc1++;
		pktsts->pkt_op_flag |= PKT_OP_ISRC1;
		/*memset(&prx->isrc1_info, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->isrc1_info,
		       pktdata, sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_ISRC1;
		break;
	case PKT_TYPE_ISRC2:
		pktsts->pkt_cnt_isrc2++;
		pktsts->pkt_op_flag |= PKT_OP_ISRC2;
		/*memset(&prx->isrc2_info, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->isrc2_info,
		       pktdata, sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_ISRC2;
		break;
	case PKT_TYPE_GAMUT_META:
		pktsts->pkt_cnt_gameta++;
		pktsts->pkt_op_flag |= PKT_OP_GMD;
		/*memset(&prx->gameta_info, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->gameta_info,
		       pktdata, sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_GMD;
		break;
	case PKT_TYPE_AUD_META:
		pktsts->pkt_cnt_amp++;
		pktsts->pkt_op_flag |= PKT_OP_AMP;
		/*memset(&prx->amp_info, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->amp_info,
		       pktdata, sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_AMP;
		break;
	case PKT_TYPE_EMP:
		pktsts->pkt_cnt_emp++;
		pktsts->pkt_op_flag |= PKT_OP_EMP;
		memcpy(&prx->emp_info,
		       pktdata, sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_EMP;
		break;

	default:
		break;
	}
	return 0;
}

int rx_pkt_handler(enum pkt_decode_type pkt_int_src)
{
	//u32 i = 0;
	u32 j = 0;
	u32 pkt_num = 0;
	bool find_emp_header = false;
	//u32 pkt_cnt = 0;
	u8 try_cnt = 3;
	union infoframe_u *pktdata;
	struct packet_info_s *prx = &rx_pkt;
	/*u32 t1, t2;*/
	rxpktsts.dv_pkt_num = 0;
	rxpktsts.fifo_pkt_num = 0;
	if (pkt_int_src == PKT_BUFF_SET_FIFO) {
		/*from pkt fifo*/
		if (!pd_fifo_buf)
			return 0;
		memset(pd_fifo_buf, 0, PFIFO_SIZE);
		memset(&prx->vs_info, 0, sizeof(struct pd_infoframe_s));
		/*t1 = sched_clock();*/
		/*for recode interrupt cnt*/
		/* rxpktsts.fifo_int_cnt++; */
		pkt_num = hdmirx_rd_dwc(DWC_PDEC_FIFO_STS1);
		//if (log_level & 0x8000)
			//rx_pr("pkt=%d\n", pkt_num);
		while (pkt_num >= K_ONEPKT_BUFF_SIZE) {
			rxpktsts.fifo_pkt_num++;
			/*read one pkt from fifo*/
			for (j = 0; j < K_ONEPKT_BUFF_SIZE; j++) {
				/*8 word per packet size*/
				pd_fifo_buf[j] =
					hdmirx_rd_dwc(DWC_PDEC_FIFO_DATA);
			}
			if (log_level & PACKET_LOG)
				rx_pr("PD[%d]=%x\n",
				      rxpktsts.fifo_pkt_num,
				      pd_fifo_buf[0]);
			pktdata = (union infoframe_u *)pd_fifo_buf;
			rx_pkt_fifodecode(prx, pktdata, &rxpktsts);
			rx.irq_flag &= ~IRQ_PACKET_FLAG;
			if (try_cnt != 0)
				try_cnt--;
			if (try_cnt == 0)
				return 0;

			pkt_num = hdmirx_rd_dwc(DWC_PDEC_FIFO_STS1);
		}
	} else if (pkt_int_src == PKT_BUFF_SET_VSI) {
		rxpktsts.pkt_attach_vsi++;
		rxpktsts.pkt_op_flag |= PKT_OP_VSI;
		rx_pkt_get_vsi_ex(&prx->vs_info);
		rxpktsts.pkt_op_flag &= ~PKT_OP_VSI;
		rxpktsts.pkt_cnt_vsi_ex++;
		rxpktsts.pkt_cnt_vsi++;
		if (rxpktsts.pkt_cnt_vsi == 0xffffffff)
			rxpktsts.pkt_cnt_vsi = 0;
	} else if (pkt_int_src == PKT_BUFF_SET_DRM) {
		rxpktsts.pkt_attach_drm++;
		rxpktsts.pkt_op_flag |= PKT_OP_DRM;
		rx_pkt_get_drm_ex(&prx->drm_info);
		rxpktsts.pkt_op_flag &= ~PKT_OP_DRM;
		rxpktsts.pkt_cnt_drm_ex++;
	} else if (pkt_int_src == PKT_BUFF_SET_GMD) {
		rxpktsts.pkt_op_flag |= PKT_OP_GMD;
		rx_pkt_get_gmd_ex(&prx->gameta_info);
		rxpktsts.pkt_op_flag &= ~PKT_OP_GMD;
		rxpktsts.pkt_cnt_gmd_ex++;
	} else if (pkt_int_src == PKT_BUFF_SET_AIF) {
		rxpktsts.pkt_op_flag |= PKT_OP_AIF;
		rx_pkt_get_audif_ex(&prx->aud_pktinfo);
		rxpktsts.pkt_op_flag &= ~PKT_OP_AIF;
		rxpktsts.pkt_cnt_aif_ex++;
	} else if (pkt_int_src == PKT_BUFF_SET_AVI) {
		rxpktsts.pkt_op_flag |= PKT_OP_AVI;
		rx_pkt_get_avi_ex(&prx->avi_info);
		rxpktsts.pkt_op_flag &= ~PKT_OP_AVI;
		rxpktsts.pkt_cnt_avi_ex++;
	} else if (pkt_int_src == PKT_BUFF_SET_ACR) {
		rxpktsts.pkt_op_flag |= PKT_OP_ACR;
		rx_pkt_get_acr_ex(&prx->acr_info);
		rxpktsts.pkt_op_flag &= ~PKT_OP_ACR;
		rxpktsts.pkt_cnt_acr_ex++;
	} else if (pkt_int_src == PKT_BUFF_SET_GCP) {
		rxpktsts.pkt_op_flag |= PKT_OP_GCP;
		rx_pkt_get_gcp_ex(&prx->gcp_info);
		rxpktsts.pkt_op_flag &= ~PKT_OP_GCP;
		rxpktsts.pkt_cnt_gcp_ex++;
	} else if (pkt_int_src == PKT_BUFF_SET_AMP) {
		rxpktsts.pkt_op_flag |= PKT_OP_AMP;
		rx_pkt_get_amp_ex(&prx->amp_info);
		rxpktsts.pkt_op_flag &= ~PKT_OP_AMP;
		rxpktsts.pkt_cnt_amp_ex++;
	} else if (pkt_int_src == PKT_BUFF_SET_NVBI) {
		rxpktsts.pkt_op_flag |= PKT_OP_NVBI;
		rx_pkt_get_ntscvbi_ex(&prx->ntscvbi_info);
		rxpktsts.pkt_op_flag &= ~PKT_OP_NVBI;
		rxpktsts.pkt_cnt_nvbi_ex++;
	} else if (pkt_int_src == PKT_BUFF_SET_EMP) {
		/*from pkt fifo*/
		if (!pd_fifo_buf)
			return 0;
		memset(pd_fifo_buf, 0, PFIFO_SIZE);
		memset(&prx->emp_info, 0, sizeof(struct pd_infoframe_s));
		pkt_num = rx.empbuff.emppktcnt;
		if (log_level & 0x4000)
			rx_pr("pkt=%d\n", pkt_num);
		while (pkt_num) {
			pkt_num--;
			/*read one pkt from fifo*/
			if (rx.chip_id >= CHIP_ID_T7) {
				memcpy(pd_fifo_buf, emp_buf + rxpktsts.fifo_pkt_num * 31, 3);
				memcpy((char *)pd_fifo_buf + 4,
				    emp_buf + rxpktsts.fifo_pkt_num * 31 + 3,
				    28);
			} else {
				memcpy(pd_fifo_buf, emp_buf + rxpktsts.fifo_pkt_num * 32, 3);
				memcpy((char *)pd_fifo_buf + 4,
				    emp_buf + rxpktsts.fifo_pkt_num * 32 + 3,
				    28);
			}
			rxpktsts.fifo_pkt_num++;
			if (log_level & PACKET_LOG)
				rx_pr("PD[%d]=%x\n", rxpktsts.fifo_pkt_num, pd_fifo_buf[0]);
			pktdata = (union infoframe_u *)pd_fifo_buf;
			rx_pkt_fifodecode(prx, pktdata, &rxpktsts);
			if ((pd_fifo_buf[0] & 0xff) == PKT_TYPE_EMP && !find_emp_header) {
				find_emp_header = true;
				rx.empbuff.ogi_id =
					(pd_fifo_buf[1] >> 16) & 0xff;
				j = (((pd_fifo_buf[2] >> 24) & 0xff) +
					((pd_fifo_buf[3] & 0xff) << 8) +
					(((pd_fifo_buf[3] >> 8) & 0xff) << 16));
				rx.empbuff.emp_tagid = j;
				rx.empbuff.data_ver =
					(pd_fifo_buf[3] >> 16) & 0xff;
				rx.empbuff.emp_content_type =
					(pd_fifo_buf[5] >> 24) & 0x0f;
			}
			rx.irq_flag &= ~IRQ_PACKET_FLAG;
		}
	} else if (pkt_int_src == PKT_BUFF_SET_SPD) {
		rxpktsts.pkt_op_flag |= PKT_OP_SPD;
		rx_pkt_get_spd_ex(&prx->spd_info);
		rxpktsts.pkt_op_flag &= ~PKT_OP_SPD;
		rxpktsts.pkt_cnt_spd++;
		rxpktsts.pkt_spd_updated = 1;
	}

	/*t2 = sched_clock();*/
	/*
	 * timerbuff[timerbuff_idx] = pkt_num;
	 * if (timerbuff_idx++ >= 50)
	 *	timerbuff_idx = 0;
	 */
	return 0;
}

#define VTEM_OGI_ID 1
#define VTEM_TAG_ID	1
void rx_get_vtem_info(void)
{
	u8 tmp;
	struct emp_pkt_st *pkt;

	pkt = (struct emp_pkt_st *)&rx_pkt.emp_info;

	if (rx.chip_id < CHIP_ID_T7)
		return;
	if (log_level == 0x121) {
		rx_pr("pkt->pkttype = %d", pkt->pkttype);
		rx_pr("pkt->cnt.organization_id = %x", pkt->cnt.organization_id);
		rx_pr("pkt->cnt.data_set_tag_lo = %x", pkt->cnt.data_set_tag_lo);
		rx_pr("pkt->cnt.md[0] = %d", pkt->cnt.md[0]);
		rx_pr("pkt->cnt.md[1] = %d", pkt->cnt.md[1]);
		rx_pr("pkt->cnt.md[2] = %d", pkt->cnt.md[2]);
		rx_pr("pkt->cnt.md[3] = %d", pkt->cnt.md[3]);
		log_level = 1;
	}
	if (pkt->pkttype == PKT_TYPE_EMP &&
		pkt->cnt.organization_id == VTEM_OGI_ID &&
		pkt->cnt.data_set_tag_lo == VTEM_TAG_ID) {
		tmp = pkt->cnt.md[0];
		rx.vtem_info.vrr_en = tmp & 1;
		rx.vtem_info.m_const = (tmp >> 1) & 1;
		rx.vtem_info.qms_en = (tmp >> 2) & 1;
		rx.vtem_info.fva_factor_m1 = (tmp >> 4) & 0x0f;
		tmp = pkt->cnt.md[1];
		rx.vtem_info.base_vfront = tmp;
		tmp = pkt->cnt.md[2];
		rx.vtem_info.rb = (tmp > 2) & 1;
		rx.vtem_info.base_framerate = pkt->cnt.md[3];
		rx.vtem_info.base_framerate |= (tmp & 3) << 8;
		pkt->pkttype = 0;
	} else {
		rx.vtem_info.vrr_en = 0;
		rx.vtem_info.m_const = 0;
		rx.vtem_info.fva_factor_m1 = 0;
		rx.vtem_info.base_vfront = 0;
		rx.vtem_info.rb = 0;
		rx.vtem_info.base_framerate = 0;
	}
	//if (rx.vrr_en) {
		//tmp = hdmirx_rd_cor(RX_VT_EMP_DBYTE0_DP0B_IVCRX);
		//rx.vtem_info.vrr_en = tmp & 1;
		//rx.vtem_info.m_const = (tmp >> 1) & 1;
		//rx.vtem_info.qms_en = (tmp >> 2) & 1;
		//rx.vtem_info.fva_factor_m1 = (tmp >> 4) & 0x0f;
		//tmp = hdmirx_rd_cor(RX_VT_EMP_DBYTE1_DP0B_IVCRX);
		//rx.vtem_info.base_vfront = tmp;
		//tmp = hdmirx_rd_cor(RX_VT_EMP_DBYTE2_DP0B_IVCRX);
		//rx.vtem_info.rb = (tmp > 2) & 1;
		//rx.vtem_info.base_framerate = hdmirx_rd_cor(RX_VT_EMP_DBYTE3_DP0B_IVCRX);
		//rx.vtem_info.base_framerate |= (tmp & 3) << 8;
	//} else {
		//rx.vtem_info.vrr_en = 0;
		//rx.vtem_info.m_const = 0;
		//rx.vtem_info.fva_factor_m1 = 0;
		///rx.vtem_info.base_vfront = 0;
		///rx.vtem_info.rb = 0;
		///rx.vtem_info.base_framerate = 0;
	//}
}

void rx_get_aif_info(void)
{
	struct aud_infoframe_st *pkt;

	pkt = (struct aud_infoframe_st *)&rx_pkt.aud_pktinfo;
	rx.aud_info.channel_count = pkt->ch_count;
	/*rx.aud_info.coding_type = pkt->coding_type;*/
	rx.aud_info.auds_ch_alloc = pkt->ca;
	/*if (log_level & PACKET_LOG) {*/
	/*	rx_pr("cc=%x\n", pkt->ch_count);*/
	/*	rx_pr("ct=%x\n", pkt->coding_type);*/
	/*	rx_pr("ca=%x\n", pkt->ca);*/
	/*}*/
}

void dump_pktinfo_status(void)
{
	rx_pr("vsi pkt:%d\n", rxpktsts.pkt_cnt_vsi);
	rx_pr("spd pkt:%d\n", rxpktsts.pkt_cnt_spd);
	rx_pr("emp pkt:%d\n", rxpktsts.pkt_cnt_emp);
	rx_pr("drm pkt:%d\n", rxpktsts.pkt_cnt_drm);
}

void rx_get_freesync_info(void)
{
	struct spd_infoframe_st *pkt;

	pkt = (struct spd_infoframe_st *)&rx_pkt.spd_info;

	if (log_level & PACKET_LOG)
		rx_pr("ieee-%x\n", pkt->des_u.freesync.ieee);

	if (pkt->des_u.freesync.ieee == IEEE_FREESYNC) {
		rx.free_sync_sts = pkt->des_u.freesync.supported +
			(pkt->des_u.freesync.enabled << 1) +
			(pkt->des_u.freesync.active  << 2);
	} else {
		rx.free_sync_sts = 0;
	}
}

