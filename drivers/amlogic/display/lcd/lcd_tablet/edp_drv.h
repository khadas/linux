/*
 * drivers/amlogic/display/vout/lcd/edp_drv.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#ifndef EDP_DRV_H
#define EDP_DRV_H
#include <linux/amlogic/vout/lcdoutc.h>

/* ********************************************************
 * displayport host (tx) control
 * ******************************************************** */
/* Tx core ID */
#define VAL_EDP_TX_CORE_ID                             0x0a

/* Link Configuration Field */
#define VAL_EDP_TX_LINK_BW_SET_162                     0x06
#define VAL_EDP_TX_LINK_BW_SET_270                     0x0a
#define VAL_EDP_TX_LINK_BW_SET_540                     0x14

/* Lane setting */
/* vswing: 0=0.4V, 1=0.6V, 2=0.8V, 3=1.2V */
#define VAL_EDP_TX_PHY_VSWING_0                        0x03
#define VAL_EDP_TX_PHY_VSWING_1                        0x06
#define VAL_EDP_TX_PHY_VSWING_2                        0x09
#define VAL_EDP_TX_PHY_VSWING_3                        0x0F
/* preemphasis: 0=0db, 1=3.5db, 2=6db, 3=9.5db */
#define VAL_EDP_TX_PHY_PREEMPHASIS_0                   0x00
#define VAL_EDP_TX_PHY_PREEMPHASIS_1                   0x05
#define VAL_EDP_TX_PHY_PREEMPHASIS_2                   0x0a
#define VAL_EDP_TX_PHY_PREEMPHASIS_3                   0x0f

/* AUX Channel Interface */
#define VAL_EDP_TX_AUX_CMD_WRITE                       (0x8 << 8)
#define VAL_EDP_TX_AUX_CMD_READ                        (0x9 << 8)
#define VAL_EDP_TX_AUX_CMD_I2C_WRITE                   (0x0 << 8)
#define VAL_EDP_TX_AUX_CMD_I2C_WRITE_MOT               (0x4 << 8)
#define VAL_EDP_TX_AUX_CMD_I2C_READ                    (0x1 << 8)
#define VAL_EDP_TX_AUX_CMD_I2C_READ_MOT                (0x5 << 8)
#define VAL_EDP_TX_AUX_CMD_I2C_WRITE_STATUS            (0x2 << 8)

#define VAL_EDP_TX_AUX_REPLY_CODE_ACK                  (0x0 << 0)
#define VAL_EDP_TX_AUX_REPLY_CODE_NACK                 (0x1 << 0)
#define VAL_EDP_TX_AUX_REPLY_CODE_DEFER                (0x2 << 0)
#define VAL_EDP_TX_AUX_REPLY_CODE_I2C_ACK              (0x0 << 0)
#define VAL_EDP_TX_AUX_REPLY_CODE_I2C_NACK             (0x4 << 0)
#define VAL_EDP_TX_AUX_REPLY_CODE_I2C_DEFER            (0x8 << 0)

/* AUX channel management logic */
#define VAL_EDP_TX_AUX_STATE_TIMEOUT                   (1 << 3)
#define VAL_EDP_TX_AUX_STATE_RECEIVED                  (1 << 2)
#define VAL_EDP_TX_AUX_STATE_REQUEST_IN_PROGRESS       (1 << 1)
#define VAL_EDP_TX_AUX_STATE_HPD_STATE                 (1 << 0)

/* AUX channel controller status */
#define VAL_EDP_TX_AUX_STATUS_REPLY_ERROR             (1 << 3)
#define VAL_EDP_TX_AUX_STATUS_REQUEST_IN_PROGRESS     (1 << 2)
#define VAL_EDP_TX_AUX_STATUS_REPLY_IN_PROGRESS       (1 << 1)
#define VAL_EDP_TX_AUX_STATUS_REPLY_RECEIVED          (1 << 0)

#define VAL_EDP_TX_LANE_STATUS_OK_4                   0x17777
#define VAL_EDP_TX_LANE_STATUS_OK_2                   0x10077
#define VAL_EDP_TX_LANE_STATUS_OK_1                   0x10007
#define VAL_EDP_TX_LANE_STATUS_OK_NONE                0xfffff

/* AUX operation */
/* us */
#define VAL_EDP_TX_AUX_WAIT_TIME                      400

#define VAL_EDP_TX_AUX_MAX_DEFER_COUNT                7
#define VAL_EDP_TX_AUX_MAX_TIMEOUT_COUNT              5

/* ********************************************************
 * displayport sink (rx) control
 * DPCD
 * ******************************************************** */
#define VAL_EDP_DPCD_REPLY_RECEIVED                  (1 << 0)

#define VAL_EDP_DPCD_TEST_RESPONSE_ACK               (1 << 0)
#define VAL_EDP_DPCD_TEST_RESPONSE_NACK              (1 << 1)
#define VAL_EDP_DPCD_TEST_RESPONSE_EDID_CHKSUM_WR    (1 << 2)

/* ********************************************************
 * Training status
 * ******************************************************** */
/* DPCD device service IRQ */
#define BIT_EDP_DPCD_SERVICE_IRQ_HDCP                 2
#define BIT_EDP_DPCD_SERVICE_IRQ_TEST                 1
#define BIT_EDP_DPCD_SERVICE_IRQ_REMOTE               0

#define VAL_EDP_TRAINING_PATTERN_OFF                  0x00
#define VAL_EDP_TRAINING_PATTERN_1                    0x01
#define VAL_EDP_TRAINING_PATTERN_2                    0x02
#define VAL_EDP_TRAINING_PATTERN_3                    0x03

/* Lane status register constants */
#define VAL_EDP_LANE_0_STATUS_CLK_REC_DONE            0x01
#define VAL_EDP_LANE_0_STATUS_CHAN_EQ_DONE            0x02
#define VAL_EDP_LANE_0_STATUS_SYM_LOCK_DONE           0x04
#define VAL_EDP_LANE_1_STATUS_CLK_REC_DONE            0x10
#define VAL_EDP_LANE_1_STATUS_CHAN_EQ_DONE            0x20
#define VAL_EDP_LANE_1_STATUS_SYM_LOCK_DONE           0x40
#define VAL_EDP_LANE_2_STATUS_CLK_REC_DONE            \
					VAL_EDP_LANE_0_STATUS_CLK_REC_DONE
#define VAL_EDP_LANE_2_STATUS_CHAN_EQ_DONE            \
					VAL_EDP_LANE_0_STATUS_CHAN_EQ_DONE
#define VAL_EDP_LANE_2_STATUS_SYM_LOCK_DONE           \
					VAL_EDP_LANE_0_STATUS_SYM_LOCK_DONE
#define VAL_EDP_LANE_3_STATUS_CLK_REC_DONE            \
					VAL_EDP_LANE_1_STATUS_CLK_REC_DONE
#define VAL_EDP_LANE_3_STATUS_CHAN_EQ_DONE            \
					VAL_EDP_LANE_1_STATUS_CHAN_EQ_DONE
#define VAL_EDP_LANE_3_STATUS_SYM_LOCK_DONE           \
					VAL_EDP_LANE_1_STATUS_SYM_LOCK_DONE
#define VAL_EDP_LANE_ALIGNMENT_DONE                   1

/* Lane status register constants */
#define BIT_EDP_LANE_0_STATUS_CLK_REC_DONE            0
#define BIT_EDP_LANE_0_STATUS_CHAN_EQ_DONE            1
#define BIT_EDP_LANE_0_STATUS_SYM_LOCK_DONE           2
#define BIT_EDP_LANE_1_STATUS_CLK_REC_DONE            4
#define BIT_EDP_LANE_1_STATUS_CHAN_EQ_DONE            5
#define BIT_EDP_LANE_1_STATUS_SYM_LOCK_DONE           6
#define BIT_EDP_LANE_2_STATUS_CLK_REC_DONE            \
					BIT_EDP_LANE_0_STATUS_CLK_REC_DONE
#define BIT_EDP_LANE_2_STATUS_CHAN_EQ_DONE            \
					BIT_EDP_LANE_0_STATUS_CHAN_EQ_DONE
#define BIT_EDP_LANE_2_STATUS_SYM_LOCK_DONE           \
					BIT_EDP_LANE_0_STATUS_SYM_LOCK_DONE
#define BIT_EDP_LANE_3_STATUS_CLK_REC_DONE            \
					BIT_EDP_LANE_1_STATUS_CLK_REC_DONE
#define BIT_EDP_LANE_3_STATUS_CHAN_EQ_DONE            \
					BIT_EDP_LANE_1_STATUS_CHAN_EQ_DONE
#define BIT_EDP_LANE_3_STATUS_SYM_LOCK_DONE           \
					BIT_EDP_LANE_1_STATUS_SYM_LOCK_DONE
#define BIT_EDP_LANE_ALIGNMENT_DONE                   0

/* Link training constants */
#define VAL_EDP_TX_TRAINING_RETRY_COUNT               5
#define VAL_EDP_MAX_TRAINING_ATTEMPTS                 5
/* ms */
#define VAL_EDP_CLOCK_REC_TIMEOUT                     1
/* ms */
#define VAL_EDP_CHAN_EQ_TIMEOUT                       4

#define VAL_EDP_MAX_DEFER_COUNT                       7
#define VAL_EDP_MAX_TIMEOUT_COUNT                     5
/* us */
#define VAL_EDP_MAX_DELAY_CYCLES                      10

/* Link training state constants */
#define STA_EDP_TRAINING_CLOCK_REC                    0x01
#define STA_EDP_TRAINING_CHANNEL_EQ                   0x02
#define STA_EDP_TRAINING_ADJUST_SPD                   0x04
#define STA_EDP_TRAINING_ADJUST_LANES                 0x08
#define STA_EDP_TRAINING_UPDATE_STATUS                0x10

/* Embedded DisplayPort constants */
#define VAL_EDP_NOT_SUPPORTED                         0x707

/* ********************************************************
 * displayport operation return stauts
 *
 * 0x00xx: EDP total operaion
 * 0x11xx: AUX operation
 * 0x22xx: EDP configuration verify
 * 0xAAxx: EDP training operation
 * 0xBBxx: EDP link policy maker control
 *
 * note: all success or correct status are 0x0000
 *       all failed status are 0xxxFF
 * ******************************************************** */
#define RET_EDP_TX_OPERATION_SUCCESS            0x0000
#define RET_EDP_TX_OPERATION_FAILED             0x00FF

#define RET_EDP_TX_AUX_OPERATION_SUCCESS        0x0000
#define RET_EDP_TX_AUX_OPERATION_ERROR          0x1111
#define RET_EDP_TX_AUX_OPERATION_TIMEOUT        0x1122
#define RET_EDP_TX_AUX_INVALID_PARAMETER        0x1144
#define RET_EDP_TX_AUX_OPERATION_FAILED         0x11FF

#define RET_EDP_CONFIG_VALID                    0x0000
#define RET_EDP_CONFIG_INVALID_LINK_RATE        0x2211
#define RET_EDP_CONFIG_INVALID_LANE_COUNT       0x2222
#define RET_EDP_CONFIG_HPD_DEASSERTED           0x2244

#define RET_EDP_TRAINING_CR_FAILED              0xAA11
#define RET_EDP_TRAINING_CHAN_EQ_FAILED         0xAA22
#define RET_EDP_TRAINING_INVALID_CONFIG         0xAA44

#define RET_EDP_LPM_STATUS_TRAINING_SUCCESS     0x0000
#define RET_EDP_LPM_STATUS_LINK_VALID           0x0000
#define RET_EDP_LPM_STATUS_CHANGED              0xBB10
#define RET_EDP_LPM_STATUS_RETRAIN              0xBB11
#define RET_EDP_LPM_STATUS_LINK_RATE_ADJUST     0xBB12
#define RET_EDP_LPM_STATUS_NOT_CONNECTED        0xBB13
#define RET_EDP_LPM_STATUS_TX_NOT_CONFIGURED    0xBB21
#define RET_EDP_LPM_STATUS_RX_IDLE              0xBB31
#define RET_EDP_LPM_STATUS_RX_ACTIVE            0xBB32

/* ********************************************************
 * edp control table
 * ******************************************************** */
/* 8/10 coding */
#define LINK_RATE_TO_CAPACITY(x)    (x * 8 / 10)
static const unsigned edp_link_capacity_table[] = {/* Mbps */
	LINK_RATE_TO_CAPACITY(1620),
	LINK_RATE_TO_CAPACITY(2700),
	LINK_RATE_TO_CAPACITY(5400),
};

#define VAL_EDP_TX_INVALID_VALUE    0xFF
static const unsigned char edp_link_rate_table[] = {
	VAL_EDP_TX_LINK_BW_SET_162,
	VAL_EDP_TX_LINK_BW_SET_270,
	VAL_EDP_TX_LINK_BW_SET_540,
	VAL_EDP_TX_INVALID_VALUE,
};

static const unsigned char edp_lane_count_table[] = {
	1, 2, 4, VAL_EDP_TX_INVALID_VALUE,
};

static const unsigned char edp_vswing_table[] = {
	VAL_EDP_TX_PHY_VSWING_0,
	VAL_EDP_TX_PHY_VSWING_1,
	VAL_EDP_TX_PHY_VSWING_2,
	VAL_EDP_TX_PHY_VSWING_3,
	VAL_EDP_TX_INVALID_VALUE,
};
static const unsigned char edp_preemphasis_table[] = {
	VAL_EDP_TX_PHY_PREEMPHASIS_0,
	VAL_EDP_TX_PHY_PREEMPHASIS_1,
	VAL_EDP_TX_PHY_PREEMPHASIS_2,
	VAL_EDP_TX_PHY_PREEMPHASIS_3,
	VAL_EDP_TX_INVALID_VALUE,
};

enum EDP_HPD_state_e {
	EDP_HPD_STATE_DISCONNECTED = 0,
	EDP_HPD_STATE_CONNECTED,
	EDP_HPD_STATE_INTERRUPT,
};

#define VAL_AUX_CMD_STATE_WRITE       0
#define VAL_AUX_CMD_STATE_READ        1
struct TRDP_AUXTrans_Req_s {
	unsigned int cmd_code;
	unsigned int cmd_state;
	unsigned int address;
	unsigned int byte_count;
	unsigned char *wr_data;
};

struct TRDP_DPCDData_s {
	unsigned char dpcd_rev;
	unsigned char max_lane_count;
	unsigned char max_link_rate;
	unsigned char downstream_port_caps[10];
	unsigned char downstream_port_types[10];
	unsigned char rev_string;
	unsigned char link_rate_string;
	char *port_type_string;
	unsigned num_rcv_ports;
	unsigned num_downstream_ports;
	unsigned char format_conversion_support;
	unsigned char oui_support;
	unsigned char ansi_8B10_support;
	unsigned char enhanced_framing_support;
	unsigned char downspread_support;
	unsigned char require_aux_handshake;
	unsigned char rx0_has_edid;
	unsigned char rx0_use_prev;
	unsigned char rx0_buffer_size;
};

/* Main Stream Attribute */
struct EDP_MSA_s {
	unsigned short h_active;
	unsigned short v_active;
	unsigned short h_period;
	unsigned short v_period;
	unsigned int clk;
	unsigned short hsync_pol;
	unsigned short hsync_width;
	unsigned short hsync_bp;
	unsigned short vsync_pol;
	unsigned short vsync_width;
	unsigned short vsync_bp;

	unsigned short ppc; /* pixels per clock cycle */
	unsigned short cformat; /* color format(0=RGB, 1=4:2:2, 2=Y only) */
	unsigned short bpc; /* bits per color */
	unsigned int sync_clock_mode;
};

struct EDP_Link_Config_s {
	unsigned char max_lane_count;
	unsigned char max_link_rate;
	unsigned char enhanced_framing_en;
	unsigned char lane_count;
	unsigned char link_rate;
	unsigned char vswing;
	unsigned char preemphasis;
	unsigned char ss_enable;
	unsigned char link_update;
	unsigned char training_settings;
	unsigned char main_stream_enable;
	unsigned char use_dpcd_caps;
	unsigned char link_rate_adjust_en;
	unsigned char link_adaptive;
	unsigned int bit_rate; /* Mbps */
};

struct EDID_Timing_s {
	unsigned int pclk;
	unsigned short h_active;
	unsigned short h_blank;
	unsigned short v_active;
	unsigned short v_blank;
	unsigned short h_fp;
	unsigned short h_pw;
	unsigned short v_fp;
	unsigned short v_pw;
	unsigned int h_size;
	unsigned int v_size;
	unsigned short h_border;
	unsigned short v_border;
	unsigned int timing_ctrl;
};

struct EDID_Range_Limit_s {
	unsigned int min_vfreq;
	unsigned int max_v_freq;
	unsigned int min_hfreq;
	unsigned int max_hfreq;
	unsigned int max_pclk;
	unsigned int GTF_ctrl;
	unsigned int GTF_start_hfreq;
	unsigned int GTF_C;
	unsigned int GTF_M;
	unsigned int GTF_K;
	unsigned int GTF_J;
};

struct EDP_EDID_Data_Type_s {
	unsigned char mid[4];     /* [8:9]2byte */
	unsigned short pid;     /* [10:11]2byte */
	unsigned int psn;       /* [12:15]4byte */
	unsigned char week;     /* [16]1byte */
	unsigned int year;     /* [17]1byte */
	unsigned short version;  /* [18:19]2byte */
	unsigned int established_timing; /* [35:37]3byte */
	unsigned int standard_timing1;   /* [38:45]4byte */
	unsigned int standard_timing2;   /* [46:53]4byte */
	struct EDID_Timing_s preferred_timing;
	unsigned int string_flag; /* [2]serial_num, [1]asc_string, [0]name */
	unsigned char name[14]; /* include "\0" */
	unsigned char serial_num[14];
	unsigned char asc_string[14];
	struct EDID_Range_Limit_s range_limit;
	unsigned char ext_flag;  /* [126]1byte */
	unsigned char checksum;  /* [127]1byte,
		256-(sum(byte0:126)%256) =? 0x100-(sum(byte0:126) & 0xff) */
};

extern unsigned char get_edp_config_index(const unsigned char *config_table,
		unsigned char config_value);
extern void edp_phy_config_update(unsigned char vswing, unsigned char preemp);
extern unsigned int edp_link_off(void);
extern unsigned int edp_host_on(struct Lcd_Config_s *pConf);
extern void edp_host_off(void);
extern void edp_edid_pre_enable(void);
extern void edp_edid_pre_disable(void);
extern int edp_edid_timing_probe(struct Lcd_Config_s *pConf);
extern void edp_probe(struct Lcd_Config_s *pConf);
extern void edp_remove(struct Lcd_Config_s *pConf);

#endif
