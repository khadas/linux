/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __ADDR_DTMB_TOP_H__
#define __ADDR_DTMB_TOP_H__

#include "addr_dtmb_top_bit.h"
#include "addr_dtmb_sync.h"
#include "addr_dtmb_sync_bit.h"
#include "addr_dtmb_che.h"
#include "addr_dtmb_che_bit.h"
#include "addr_dtmb_front.h"
#include "addr_dtmb_front_bit.h"

#define DTMB_DEMOD_BASE		DEMOD_REG_ADDR_OFFSET(0x0)

#define  DTMB_TOP_ADDR(x) (DTMB_DEMOD_BASE + (x << 2))

#define  DTMB_TOP_CTRL_SW_RST               (0x1)
#define  DTMB_TOP_TESTBUS                   (0x2)
#define  DTMB_TOP_TB                        (0x3)
#define  DTMB_TOP_TB_V                      (0x4)
#define  DTMB_TOP_TB_ADDR_BEGIN             (0x5)
#define  DTMB_TOP_TB_ADDR_END               (0x6)
#define  DTMB_TOP_CTRL_ENABLE               (0x7)
#define  DTMB_TOP_CTRL_LOOP                 (0x8)
#define  DTMB_TOP_CTRL_FSM                  (0x9)
#define  DTMB_TOP_CTRL_AGC                  (0xa)
#define  DTMB_TOP_CTRL_TS_SFO_CFO           (0xb)
#define  DTMB_TOP_CTRL_FEC                  (0xc)
#define  DTMB_TOP_CTRL_INTLV_TIME           (0xd)
#define  DTMB_TOP_CTRL_DAGC_CCI             (0xe)
#define  DTMB_TOP_CTRL_TPS                  (0xf)
#define  DTMB_TOP_TPS_BIT                   (0x10)
#define  DTMB_TOP_CCI_FLG                   (0xc7)
#define  DTMB_TOP_TESTBUS_OUT               (0xc8)
#define  DTMB_TOP_TBUS_DC_ADDR              (0xc9)
#define  DTMB_TOP_FRONT_IQIB_CHECK          (0xca)
#define  DTMB_TOP_SYNC_TS                   (0xcb)
#define  DTMB_TOP_SYNC_PNPHASE              (0xcd)
#define  DTMB_TOP_CTRL_DDC_ICFO             (0xd2)
#define  DTMB_TOP_CTRL_DDC_FCFO             (0xd3)
#define  DTMB_TOP_CTRL_FSM_STATE0           (0xd4)
#define  DTMB_TOP_CTRL_FSM_STATE1           (0xd5)
#define  DTMB_TOP_CTRL_FSM_STATE2           (0xd6)
#define  DTMB_TOP_CTRL_FSM_STATE3           (0xd7)
#define  DTMB_TOP_CTRL_TS2                  (0xd8)
#define  DTMB_TOP_FRONT_AGC                 (0xd9)
#define  DTMB_TOP_FRONT_DAGC                (0xda)
#define  DTMB_TOP_FEC_TIME_STS              (0xdb)
#define  DTMB_TOP_FEC_LDPC_STS              (0xdc)
#define  DTMB_TOP_FEC_LDPC_IT_AVG           (0xdd)
#define  DTMB_TOP_FEC_LDPC_UNC_ACC          (0xde)
#define  DTMB_TOP_FEC_BCH_ACC               (0xdf)
#define  DTMB_TOP_CTRL_ICFO_ALL             (0xe0)
#define  DTMB_TOP_CTRL_FCFO_ALL             (0xe1)
#define  DTMB_TOP_CTRL_SFO_ALL              (0xe2)
#define  DTMB_TOP_FEC_LOCK_SNR              (0xe3)
#define  DTMB_TOP_CHE_SEG_FACTOR            (0xe4)
#define  DTMB_TOP_CTRL_CHE_WORKCNT          (0xe5)
#define  DTMB_TOP_CHE_OBS_STATE1            (0xe6)
#define  DTMB_TOP_CHE_OBS_STATE2            (0xe7)
#define  DTMB_TOP_CHE_OBS_STATE3            (0xe8)
#define  DTMB_TOP_CHE_OBS_STATE4            (0xe9)
#define  DTMB_TOP_CHE_OBS_STATE5            (0xea)
#define  DTMB_TOP_SYNC_CCI_NF1              (0xee)
#define  DTMB_TOP_SYNC_CCI_NF2              (0xef)
#define  DTMB_TOP_SYNC_CCI_NF2_POSITION     (0xf0)
#define  DTMB_TOP_CTRL_SYS_OFDM_CNT         (0xf1)
#define  DTMB_TOP_CTRL_TPS_Q_FINAL          (0xf2)
#define  DTMB_TOP_FRONT_DC                  (0xf3)
#define  DTMB_TOP_CHE_DEBUG                 (0xf6)
#define  DTMB_TOP_CTRL_TOTPS_READY_CNT      (0xff)

#endif
