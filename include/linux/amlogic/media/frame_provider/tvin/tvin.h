/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __TVIN_H
#define __TVIN_H

#include <linux/types.h>
#include <linux/amlogic/media/amvecm/cm.h>
#include <linux/dvb/frontend.h>
#include <uapi/linux/amlogic/tvin.h>

enum {
	MEMP_VDIN_WITHOUT_3D = 0,
	MEMP_VDIN_WITH_3D,
	MEMP_DCDR_WITHOUT_3D,
	MEMP_DCDR_WITH_3D,
	MEMP_ATV_WITHOUT_3D,
	MEMP_ATV_WITH_3D,
};

enum adc_sel {
	ADC_ATV_DEMOD = 1,
	ADC_TVAFE = 2,
	ADC_DTV_DEMOD = 4,
	ADC_DTV_DEMODPLL = 8,
	ADC_MAX,
};

enum filter_sel {
	FILTER_ATV_DEMOD = 1,
	FILTER_TVAFE = 2,
	FILTER_DTV_DEMOD = 4,
	FILTER_DTV_DEMODT2 = 8,
	FILTER_MAX,
};

/* *********************************************************************** */

/* * TVIN general definition/enum/struct *********************************** */

/* ************************************************************************ */

enum viu_mux_port {
	VIU_MUX_SEL_ENCI = 1,
	VIU_MUX_SEL_ENCP = 2,
	VIU_MUX_SEL_ENCL = 4,
	VIU_MUX_SEL_WB0 = 8,
	VIU_MUX_SEL_WB1 = 16,
	VIU_MUX_SEL_WB2 = 32,/*t7*/
	VIU_MUX_SEL_WB3 = 64,/*t7*/
};

enum wb_chan_sel {
	WB_CHAN_DISABLE = 0,
	WB_CHAN_SEL_POST_BLD_VD1 = 1,
	WB_CHAN_SEL_POST_BLD_VD2 = 2,
	WB_CHAN_SEL_POST_OSD1 = 3,
	WB_CHAN_SEL_POST_OSD2 = 4,
	WB_CHAN_SEL_POST_DOUT = 5,
	WB_CHAN_SEL_VPP_DOUT = 6,
	WB_CHAN_SEL_PRE_BLD_VD1 = 7,/*t7*/
	WB_CHAN_SEL_POST_VD3 = 8,/*t7*/
};

const char *tvin_port_str(enum tvin_port_e port);

const char *tvin_sig_status_str(enum tvin_sig_status_e status);

/* tvin parameters */
#define TVIN_PARM_FLAG_CAP      0x00000001

/* tvin_parm_t.flag[ 0]: 1/enable or 0/disable frame capture function */

#define TVIN_PARM_FLAG_CAL      0x00000002

/* tvin_parm_t.flag[ 1]: 1/enable or 0/disable adc calibration */

/*used for processing 3d in ppmgr set this flag*/
/*to drop one field and send real height in vframe*/
#define TVIN_PARM_FLAG_2D_TO_3D 0x00000004

/* tvin_parm_t.flag[ 2]: 1/enable or 0/disable 2D->3D mode */

enum tvin_color_fmt_range_e {
	TVIN_FMT_RANGE_NULL = 0,	/* depend on video fromat */
	TVIN_RGB_FULL,		/* 1 */
	TVIN_RGB_LIMIT,		/* 2 */
	TVIN_YUV_FULL,		/* 3 */
	TVIN_YUV_LIMIT,		/* 4 */
	TVIN_COLOR_FMT_RANGE_MAX,
};

const char *tvin_trans_fmt_str(enum tvin_trans_fmt trans_fmt);

const char *tvin_trans_color_range_str(enum
						tvin_color_fmt_range_e
						color_range);

const char *tvin_trans_force_range_str(enum tvin_force_color_range_e
				       force_range);
const char *tvin_color_fmt_str(enum tvin_color_fmt_e color_fmt);

/* hs=he=vs=ve=0 is to disable Cut Window */
struct tvin_cutwin_s {
	unsigned short hs;
	unsigned short he;
	unsigned short vs;
	unsigned short ve;
};

/* for pin selection */
enum tvafe_adc_pin_e {
	TVAFE_ADC_PIN_NULL = 0,
	/*(MESON_CPU_TYPE > MESON_CPU_TYPE_MESONG9TV) */
	TVAFE_CVBS_IN0 = 1,  /* avin ch0 */
	TVAFE_CVBS_IN1 = 2,  /* avin ch1 */
	TVAFE_CVBS_IN2 = 3,  /* avin ch2 */
	TVAFE_CVBS_IN3 = 4,  /*as atvdemod to tvafe */
	TVAFE_ADC_PIN_MAX,
};

enum tvafe_src_sig_e {
	/* TODO Only M8 first */

	/*#if (MESON_CPU_TYPE == MESON_CPU_TYPE_MESONG9TV) */
	CVBS_IN0 = 0,
	CVBS_IN1,
	CVBS_IN2,
	CVBS_IN3,
	TVAFE_SRC_SIG_MAX_NUM,
};

struct tvafe_pin_mux_s {
	enum tvafe_adc_pin_e pin[TVAFE_SRC_SIG_MAX_NUM];
};

bool IS_TVAFE_SRC(enum tvin_port_e port);
bool IS_TVAFE_ATV_SRC(enum tvin_port_e port);
bool IS_TVAFE_AVIN_SRC(enum tvin_port_e port);
bool IS_HDMI_SRC(enum tvin_port_e port);

/*
 *function defined applied for other driver
 */

struct dfe_adcpll_para {
	unsigned int adcpllctl;
	unsigned int demodctl;
	unsigned int mode; //[1]: atsc mode, [2]: for dvbc_blind_scan mode;
	enum fe_delivery_system delsys;
	unsigned int adc_clk;
	/* adc pga gain:
	 * x1(0): 0x41209007(default).
	 * x1.5(1): 0x45209007.
	 * x2.5(2): 0x49209007.
	 */
	unsigned int pga_gain;
};

struct rx_audio_stat_s {
	/* 1: AUDS, 2: OBA, 4:DST, 8: HBR, 16: OBM, 32: MAS */
	int aud_rcv_packet;
	/*audio stable status*/
	bool aud_stb_flag;
	/*audio sample rate*/
	int aud_sr;
	/**audio channel count*/
	/*0: refer to stream header,*/
	/*1: 2ch, 2: 3ch, 3: 4ch, 4: 5ch,*/
	/*5: 6ch, 6: 7ch, 7: 8ch*/
	int aud_channel_cnt;
	/**audio coding type*/
	/*0: refer to stream header, 1: IEC60958 PCM,*/
	/*2: AC-3, 3: MPEG1 (Layers 1 and 2),*/
	/*4: MP3 (MPEG1 Layer 3), 5: MPEG2 (multichannel),*/
	/*6: AAC, 7: DTS, 8: ATRAC, 9: One Bit Audio,*/
	/*10: Dolby Digital Plus, 11: DTS-HD,*/
	/*12: MAT (MLP), 13: DST, 14: WMA Pro*/
	int aud_type;
	/* indicate if audio fifo start threshold is crossed */
	bool afifo_thres_pass;
	/*
	 * 0 [ch1 ch2]
	 * 1,2,3 [ch1 ch2 ch3 ch4]
	 * 4,8 [ch1 ch2 ch5 ch6]
	 * 5,6,7,9,10,11 [ch1 ch2 ch3 ch4 ch5 ch6]
	 * 12,16,24,28 [ch1 ch2 ch5 ch6 ch7 ch8]
	 * 20 [ch1 ch2 ch7 ch8]
	 * 21,22,23[ch1 ch2 ch3 ch4 ch7 ch8]
	 * all others [all of 8ch]
	 */
	int aud_alloc;
	u8 ch_sts[7];
};

int get_vdin_delay_num(void);
#ifdef CONFIG_AMLOGIC_MEDIA_ADC
void adc_set_pll_reset(void);
int adc_get_pll_flag(void);
/*ADC_EN_ATV_DEMOD	0x1*/
/*ADC_EN_TVAFE		0x2*/
/*ADC_EN_DTV_DEMOD	0x4*/
/*ADC_EN_DTV_DEMODPLL	0x8*/
int adc_set_pll_cntl(bool on, enum adc_sel module_sel, void *p_para_);
void adc_set_ddemod_default(struct dfe_adcpll_para *adcpll_para);/* add for dtv demod */
int adc_set_filter_ctrl(bool on, enum filter_sel module_sel, void *data);
int adc_get_status(enum adc_sel module_sel);
#else
void adc_set_pll_reset(void)
{
}

int adc_get_pll_flag(void)
{
	return 0;
}

int adc_set_pll_cntl(bool on, enum adc_sel module_sel, void *p_para_)
{
	return 0;
}

void adc_set_ddemod_default(enum fe_delivery_system delsys)
{
}

int adc_set_filter_ctrl(bool on, enum filter_sel module_sel, void *data)
{
	return 0;
}

int adc_get_status(enum adc_sel module_sel)
{
	return 0;
}
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_VDIN
unsigned int get_vdin_buffer_num(void);
#else
unsigned int get_vdin_buffer_num(void)
{
}
#endif
void rx_get_audio_status(struct rx_audio_stat_s *aud_sts);
void rx_set_atmos_flag(bool en);
bool rx_get_atmos_flag(void);
u_char rx_edid_get_aud_sad(u_char *sad_data);
bool rx_edid_set_aud_sad(u_char *sad, u_char len);
int rx_set_audio_param(uint32_t param);
void rx_earc_hpd_cntl(void);
#endif
