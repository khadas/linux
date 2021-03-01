/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __EARC_HW_H__
#define __EARC_HW_H__

#include "regs.h"
#include "iomap.h"
#include "../common/iomapres.h"
#include "../common/iec_info.h"

/* For earc spdifout mask lane register offset V1:
 * EARCTX_SPDIFOUT_CTRL0, offset: 4 - 11
 */
#define EARC_SPDIFOUT_LANE_MASK_V1 1
/* For earc spdifout mask lane register offset V2:
 * EARCTX_SPDIFOUT_CTRL2, offset: 0 - 15
 */
#define EARC_SPDIFOUT_LANE_MASK_V2 2

#define INT_EARCRX_CMDC_IDLE2               (0x1 << 15)
#define INT_EARCRX_CMDC_IDLE1               (0x1 << 14)
#define INT_EARCRX_CMDC_DISC2               (0x1 << 13)
#define INT_EARCRX_CMDC_DISC1               (0x1 << 12)
#define INT_EARCRX_CMDC_EARC                (0x1 << 11)
#define INT_EARCRX_CMDC_HB_STATUS           (0x1 << 10)
#define INT_EARCRX_CMDC_LOSTHB              (0x1 << 9)
#define INT_EARCRX_CMDC_TIMEOUT             (0x1 << 8)
#define INT_EARCRX_CMDC_STATUS_CH           (0x1 << 7)
#define INT_EARCRX_CMDC_REC_INVALID_ID      (0x1 << 6)
#define INT_EARCRX_CMDC_REC_INVALID_OFFSET  (0x1 << 5)
#define INT_EARCRX_CMDC_REC_UNEXP           (0x1 << 4)
#define INT_EARCRX_CMDC_REC_ECC_ERR         (0x1 << 3)
#define INT_EARCRX_CMDC_REC_PARITY_ERR      (0x1 << 2)
#define INT_EARCRX_CMDC_RECV_PACKET         (0x1 << 1)
#define INT_EARCRX_CMDC_REC_TIME_OUT        (0x1 << 0)

#define INT_EARCRX_DMAC_VALID_AUTO_NEG_INT_SET             (0x1 << 28)
#define INT_EARCRX_ANA_RST_C_NEW_FORMAT_SET                (0x1 << 17)
#define INT_EARCRX_ANA_RST_C_EARCRX_DIV2_HOLD_SET          (0x1 << 16)
#define INT_EARCRX_ERR_CORRECT_C_BCHERR_INT_SET            (0x1 << 15)
#define INT_EARCRX_ERR_CORRECT_R_AFIFO_OVERFLOW_SET        (0x1 << 14)
#define INT_EARCRX_ERR_CORRECT_R_FIFO_OVERFLOW_SET         (0x1 << 13)
#define INT_EARCRX_USER_BIT_CHECK_R_FIFO_OVERFLOW          (0x1 << 12)
#define INT_EARCRX_USER_BIT_CHECK_C_FIFO_THD_PASS          (0x1 << 11)
#define INT_EARCRX_USER_BIT_CHECK_C_U_PK_LOST_INT_SET      (0x1 << 10)
#define INT_ARCRX_USER_BIT_CHECK_C_IU_PK_END               (0x1 << 9)
#define INT_ARCRX_BIPHASE_DECODE_C_CHST_MUTE_CLR           (0x1 << 8)
#define INT_ARCRX_BIPHASE_DECODE_C_FIND_PAPB               (0x1 << 7)
#define INT_ARCRX_BIPHASE_DECODE_C_VALID_CHANGE            (0x1 << 6)
#define INT_ARCRX_BIPHASE_DECODE_C_FIND_NONPCM2PCM         (0x1 << 5)
#define INT_ARCRX_BIPHASE_DECODE_C_PCPD_CHANGE             (0x1 << 4)
#define INT_ARCRX_BIPHASE_DECODE_C_CH_STATUS_CHANGE        (0x1 << 3)
#define INT_ARCRX_BIPHASE_DECODE_I_SAMPLE_MODE_CHANGE      (0x1 << 2)
#define INT_ARCRX_BIPHASE_DECODE_R_PARITY_ERR              (0x1 << 1)
#define INT_ARCRX_DMAC_SYNC_AFIFO_OVERFLOW                 (0x1 << 0)

#define INT_EARCTX_CMDC_HPD_H               (0x1 << 17)
#define INT_EARCTX_CMDC_HPD_L               (0x1 << 16)
#define INT_EARCTX_CMDC_IDLE2               (0x1 << 15)
#define INT_EARCTX_CMDC_IDLE1               (0x1 << 14)
#define INT_EARCTX_CMDC_DISC2               (0x1 << 13)
#define INT_EARCTX_CMDC_DISC1               (0x1 << 12)
#define INT_EARCTX_CMDC_EARC                (0x1 << 11)
#define INT_EARCTX_CMDC_HB_STATUS           (0x1 << 10)
#define INT_EARCTX_CMDC_LOSTHB              (0x1 << 9)
#define INT_EARCTX_CMDC_TIMEOUT             (0x1 << 8)
#define INT_EARCTX_CMDC_STATUS_CH           (0x1 << 7)
#define INT_EARCTX_CMDC_RECV_FINISHED       (0x1 << 6)
#define INT_EARCTX_CMDC_RECV_NACK           (0x1 << 5)
#define INT_EARCTX_CMDC_RECV_NORSP          (0x1 << 4)
#define INT_EARCTX_CMDC_RECV_UNEXP          (0x1 << 3)
#define INT_EARCTX_CMDC_RECV_DATA           (0x1 << 2)
#define INT_EARCTX_CMDC_RECV_ACK            (0x1 << 1)
#define INT_EARCTX_CMDC_RECV_ECC_ERR        (0x1 << 0)

#define INT_EARCTX_FEM_C_HOLD_CLR                (0x1 << 4)
#define INT_EARCTX_FEM_C_HOLD_START              (0x1 << 3)
#define INT_EARCTX_ERRCORR_C_FIFO_THD_LESS_PASS  (0x1 << 2)
#define INT_EARCTX_ERRCORR_C_FIFO_OVERFLOW       (0x1 << 1)
#define INT_EARCTX_ERRCORR_C_FIFO_EMPTY          (0x1 << 0)

/* Status Bits */
#define STAT_CHNG       (0x4)
#define CAP_CHNG        (0x3)
#define EARC_HPD        (0x0)
#define EARC_VALID      (0x7)
#define STAT_CHNG_CONF  (0x4)
#define CAP_CHNG_CONF   (0x3)
#define HDMI_HPD        (0x0)

/* Capability Data Structure, max bytes */
#define CDS_MAX_BYTES 256

/* each reg's length for channel status */
#define REG_CS_LEN    32

/* category code */
#define IEC_CS_CATEGORY_OFFSET             8
#define IEC_CS_CATEGORY_MASK               0xff

/* Sampling frequency */
#define IEC_CS_SFREQ_OFFSET                24
#define IEC_CS_SFREQ_MASK                  0xf

/* Word Length */
#define IEC_CS_WLEN_OFFSET                 32
#define IEC_CS_WLEN_MASK                   0xf

/* Channel status in IEC for eARC */

/* Multi-channel */
#define IEC_CS_EARC_MULTICH_OFFSET         4
#define IEC_CS_EARC_MULTICH_MASK           0x3

/*
 * multi-channels layout, 2ch/8ch/16ch/32ch
 * compressed audio layout, layout a/layout b
 * one bit audio layout, 6ch/12ch
 */
#define IEC_CS_AUDIO_LAYOUT_OFFSET         44
#define IEC_CS_AUDIO_LAYOUT_MASK           0xf

/* mute */
#define IEC_CS_MUTE_OFFSET                 146
#define IEC_CS_MUTE_MASK                   0x1

/* CS AIF for eARC */
/* data byte 4 for multi-channel Uncompressed Audio */
#define IEC_CS_AIF_DB4_OFFSET            136
#define IEC_CS_AIF_DB4_MASK              0xff

/* Earc channel status info */

/* whether encryption, Bit 3 in Channel Status */
enum earc_encryption {
	EARC_UNENCRYPTED = 0x0,
	EARC_ENCRYPTED = 0x1,
};

/* iec audio format */
enum earc_audio_fmt {
	EARC_FMT_LPCM = 0x0,
	/* Non-linear PCM */
	EARC_FMT_NONLPCM = 0x01,
	/* One Bit Audio */
	EARC_FMT_OBA = 0x03,
};

/* channels, Bit 4, 5 in Channel Status */
enum earc_ch {
	/* Linear PCM */
	EARC_CH_STEREO_LPCM = 0x0,
	EARC_CH_MULTICH_LPCM = 0x01,
	/* Non-linear PCM */
	EARC_CH_NONLPCM = 0x0,
	/* One Bit Audio */
	EARC_CH_OBA = 0x03,
};

enum aif_data_byte4 {
	/* FR FL */
	AIF_2CH     = 0x0,
	/* LFE,FR,FL */
	AIF_3CH     = 0x01,
	/* RS,LS,FC,LFE,FR,FL */
	AIF_6CH     = 0x0b,
	/* RRC,RLC,RS,LS,FC,LFE,FR,FL */
	AIF_8CH     = 0x13,
	AIF_16CH    = 0xFE,
	AIF_32CH    = 0xFF,
};

enum iec_audio_type {
	TYPE_PCM,
	TYPE_NONPCM,
	TYPE_OBA,
};

/* channel layout for LPCM, multi-channel LPCM, Compressed Audio */
enum iec_audio_layout {
	/* layout for LPCM */
	LO2_LPCM = 0x0,
	LO8_LPCM = 0x7,
	LO16_LPCM = 0xb,
	LO32_LPCM = 0x3,

	/* layout for compressed audio */
	LOA_COMPRESS = 0x0,
	LOB_COMPRESS = 0x7,

	/* layout for One Bit Audio */
	LO6_OBA = 0x5,
	LO12_OBA = 0x9,
};

/* cmdc discovery and disconnect state */
enum cmdc_st {
	CMDC_ST_OFF,
	CMDC_ST_IDLE1,
	CMDC_ST_IDLE2,
	CMDC_ST_DISC1,
	CMDC_ST_DISC2,
	CMDC_ST_EARC,
	CMDC_ST_ARC
};

/* attended type: disconect, ARC, eARC */
enum attend_type {
	ATNDTYP_DISCNCT,
	ATNDTYP_ARC,
	ATNDTYP_EARC
};

enum tx_hd_hdp_mux {
	GPIOW_1,
	GPIOW_9,
	GPIOW_5
};

enum device_id {
	/* Capabilities Data Structure */
	CAP_DEV_ID = 0xA0,
	/* eARC status and latency control registers */
	STAT_CTRL_DEV_ID = 0x74
};

enum reg_offset {
	/* eARC Status Register Offset */
	EARCRX_STAT_REG = 0xD0,
	EARCTX_STAT_REG = 0xD1,
	/* eARC Latency Registers Offset */
	ERX_LATENCY_REG = 0xD2,
	ERX_LATENCY_REQ_REG = 0xD3
};

enum pll_rst_src {
	RST_BY_SELF, /* earcrx_pll_self_reset */
	RST_BY_D2A,  /* earcrx_pll_digital_rstn_d2a */
	RST_BY_DMACRX, /* earcrx_dmac_rx_sqvalid */
};

void earcrx_pll_refresh(struct regmap *top_map,
			enum pll_rst_src rst_src,
			bool level);
void earcrx_cmdc_int_mask(struct regmap *top_map);
void earcrx_cmdc_init(struct regmap *top_map, bool en, bool rx_dmac_sync_int, bool rterm_on);
void earcrx_cmdc_arc_connect(struct regmap *cmdc_map, bool init);
void earcrx_cmdc_hpd_detect(struct regmap *cmdc_map, bool st);
void earcrx_dmac_sync_int_enable(struct regmap *top_map, int enable);
void earcrx_dmac_init(struct regmap *top_map,
		      struct regmap *dmac_map,
		      bool unstable_tick_sel,
		      bool chnum_mult_mode);
void earcrx_arc_init(struct regmap *dmac_map);
unsigned int earcrx_get_cs_iec958(struct regmap *dmac_map);
unsigned int earcrx_get_cs_ca(struct regmap *dmac_map);
unsigned int earcrx_get_cs_mute(struct regmap *dmac_map);
unsigned int earcrx_get_cs_fmt(struct regmap *dmac_map, enum attend_type type);
unsigned int earcrx_get_cs_freq(struct regmap *dmac_map,
				enum audio_coding_types coding_type);
unsigned int earcrx_get_cs_word_length(struct regmap *dmac_map);
enum cmdc_st earcrx_cmdc_get_state(struct regmap *cmdc_map);
enum attend_type earcrx_cmdc_get_attended_type(struct regmap *cmdc_map);
void earcrx_cdmc_clr_irqs(struct regmap *top_map, int clr);
int earcrx_cdmc_get_irqs(struct regmap *top_map);
void earcrx_dmac_sync_clr_irqs(struct regmap *top_map);
void earcrx_dmac_clr_irqs(struct regmap *top_map, int clr);
int earcrx_dmac_get_irqs(struct regmap *top_map);
int earcrx_dmac_get_mask(struct regmap *top_map);
bool earcrx_pll_dmac_valid(struct regmap *top_map);
void earcrx_reset(struct regmap *dmac_map);
void earcrx_enable(struct regmap *cmdc_map,
		   struct regmap *dmac_map, bool enable);
void earctx_cmdc_int_mask(struct regmap *top_map);

void earctx_cmdc_init(struct regmap *top_map, bool en, bool rterm_on);
void earctx_cmdc_set_timeout(struct regmap *cmdc_map, int no_timeout);
void earctx_cmdc_arc_connect(struct regmap *cmdc_map, bool init);
void earctx_cmdc_hpd_detect(struct regmap *top_map,
			    struct regmap *cmdc_map,
			    int earc_port, bool st);
void earctx_dmac_init(struct regmap *top_map,
		      struct regmap *dmac_map,
		      int earc_spdifout_lane_mask);
void earctx_dmac_set_format(struct regmap *dmac_map,
			    int frddr_idx, int msb, int frddr_type);
int earctx_get_cs_iec958(struct regmap *dmac_map);
void earctx_set_cs_iec958(struct regmap *dmac_map, int cs);
void earctx_set_cs_mute(struct regmap *dmac_map, bool mute);
void earctx_set_cs_ca(struct regmap *dmac_map, unsigned int ca);
void earctx_set_cs_info(struct regmap *dmac_map,
			enum audio_coding_types coding_type,
			struct iec_cnsmr_cs *cs_info,
			unsigned int *lpcm_ca);
enum cmdc_st earctx_cmdc_get_state(struct regmap *cmdc_map);
enum attend_type earctx_cmdc_get_attended_type(struct regmap *cmdc_map);
void earctx_cdmc_clr_irqs(struct regmap *top_map, int clr);
int earctx_cdmc_get_irqs(struct regmap *top_map);
void earctx_dmac_clr_irqs(struct regmap *top_map, int clr);
int earctx_dmac_get_irqs(struct regmap *top_map);
void earctx_compressed_enable(struct regmap *dmac_map,
			      enum attend_type type,
			      enum audio_coding_types coding_type,
			      bool enable);
void earctx_enable(struct regmap *top_map,
		   struct regmap *cmdc_map,
		   struct regmap *dmac_map,
		   enum audio_coding_types coding_type,
		   bool enable,
		   bool rterm_on);

void earcrx_cmdc_get_latency(struct regmap *cmdc_map, u8 *latency);
void earcrx_cmdc_set_latency(struct regmap *cmdc_map, u8 *latency);
void earcrx_cmdc_get_cds(struct regmap *cmdc_map, u8 *cds);
void earcrx_cmdc_set_cds(struct regmap *cmdc_map, u8 *cds);

int earctx_cmdc_get_stat_bits(struct regmap *cmdc_map);
void earctx_cmdc_clr_stat_bits(struct regmap *cmdc_map, int stat_bits);
void earctx_cmdc_set_hdmi_hpd_bit(struct regmap *cmdc_map, bool is_high);
void earctx_cmdc_set_hb_valid_bit(struct regmap *cmdc_map, bool hb_valid);
void earctx_cmdc_get_latency(struct regmap *cmdc_map, u8 *latency);
void earctx_cmdc_set_latency(struct regmap *cmdc_map, u8 *latency);
void earctx_cmdc_get_cds(struct regmap *cmdc_map, u8 *cds);

void earcrx_ana_auto_cal(struct regmap *top_map);
void earctx_ana_auto_cal(struct regmap *top_map);
bool earxrx_get_pll_valid(struct regmap *top_map);
bool earxrx_get_pll_valid_auto(struct regmap *top_map);
u8 earcrx_cmdc_get_rx_stat_bits(struct regmap *cmdc_map);
#endif
