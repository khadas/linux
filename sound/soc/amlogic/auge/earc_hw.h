/*
 * sound/soc/amlogic/auge/earc_hw.h
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#ifndef __EARC_HW_H__
#define __EARC_HW_H__

#include "regs.h"
#include "iomap.h"
#include <linux/amlogic/media/sound/iomapres.h>
#include <linux/amlogic/media/sound/spdif_info.h>

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

#define CDS_MAX_BYTES 256

void earcrx_pll_refresh(struct regmap *top_map);
void earcrx_cmdc_init(struct regmap *top_map);
void earcrx_cmdc_arc_connect(struct regmap *cmdc_map, bool init);
void earcrx_cmdc_hpd_detect(struct regmap *cmdc_map, bool st);
void earcrx_dmac_init(struct regmap *top_map, struct regmap *dmac_map);
void earcrx_arc_init(struct regmap *dmac_map);
enum cmdc_st earcrx_cmdc_get_state(struct regmap *cmdc_map);
enum attend_type earcrx_cmdc_get_attended_type(struct regmap *cmdc_map);
void earcrx_cdmc_clr_irqs(struct regmap *top_map, int clr);
int earcrx_cdmc_get_irqs(struct regmap *top_map);
void earcrx_dmac_clr_irqs(struct regmap *top_map, int clr);
int earcrx_dmac_get_irqs(struct regmap *top_map);
void earcrx_enable(struct regmap *cmdc_map,
		   struct regmap *dmac_map, bool enable);
void earctx_cmdc_int_mask(struct regmap *top_map);

void earctx_cmdc_init(struct regmap *top_map);
void earctx_cmdc_arc_connect(struct regmap *cmdc_map, bool init);
void earctx_cmdc_hpd_detect(struct regmap *top_map,
			    struct regmap *cmdc_map,
			    int earc_port, bool st);
void earctx_dmac_init(struct regmap *top_map, struct regmap *dmac_map);
void earctx_dmac_set_format(struct regmap *dmac_map,
			    int frddr_idx, int msb, int frddr_type);
void earctx_set_channel_status_info(struct regmap *dmac_map,
				    struct iec958_chsts *chsts);
enum cmdc_st earctx_cmdc_get_state(struct regmap *cmdc_map);
enum attend_type earctx_cmdc_get_attended_type(struct regmap *cmdc_map);
void earctx_cdmc_clr_irqs(struct regmap *top_map, int clr);
int earctx_cdmc_get_irqs(struct regmap *top_map);
void earctx_dmac_clr_irqs(struct regmap *top_map, int clr);
int earctx_dmac_get_irqs(struct regmap *top_map);
void earctx_enable(struct regmap *top_map,
		   struct regmap *cmdc_map,
		   struct regmap *dmac_map,
		   bool enable);

void earcrx_cmdc_get_latency(struct regmap *cmdc_map, u8 *latency);
void earcrx_cmdc_set_latency(struct regmap *cmdc_map, u8 *latency);
void earcrx_cmdc_get_cds(struct regmap *cmdc_map, u8 *cds);
void earcrx_cmdc_set_cds(struct regmap *cmdc_map, u8 *cds);

void earctx_cmdc_get_latency(struct regmap *cmdc_map, u8 *latency);
void earctx_cmdc_set_latency(struct regmap *cmdc_map, u8 *latency);
void earctx_cmdc_get_cds(struct regmap *cmdc_map, u8 *cds);

#endif
