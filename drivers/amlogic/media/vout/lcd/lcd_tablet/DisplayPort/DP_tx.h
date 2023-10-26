/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef EDP_TX_H
#define EDP_TX_H

//! aux operation
struct dptx_aux_req_s {
	unsigned int cmd_code;
	unsigned int cmd_state; /* 0:write, 1:read */
	unsigned int address;
	unsigned int byte_cnt;
	unsigned char *data;
};

int dptx_aux_i2c_read(struct aml_lcd_drv_s *pdrv, unsigned int dev_addr, unsigned int reg_addr,
				unsigned int len, unsigned char *buf);
int dptx_aux_write(struct aml_lcd_drv_s *pdrv, unsigned int addr, int len, unsigned char *buf);
int dptx_aux_write_single(struct aml_lcd_drv_s *pdrv, unsigned int addr, unsigned char val);
int dptx_aux_read(struct aml_lcd_drv_s *pdrv, unsigned int addr, int len, unsigned char *buf);

//! DP_utils.c
enum DP_link_rate_e {
	DP_LINK_RATE_RBR  = 0x06,
	DP_LINK_RATE_HBR  = 0x0a,
	DP_LINK_RATE_HBR2 = 0x14,
	DP_LINK_RATE_HBR3 = 0x1e,

	DP_LINK_RATE_UBR10  = 0x01,
	DP_LINK_RATE_UBR20  = 0x02,
	DP_LINK_RATE_UBR135 = 0x04,
	DP_LINK_RATE_INVALID = 0,
};

#define VAL_DPTX_PHY_VSWING_0	0x03   // 0.4
#define VAL_DPTX_PHY_VSWING_1	0x06   // 0.6
#define VAL_DPTX_PHY_VSWING_2	0x09   // 0.8
#define VAL_DPTX_PHY_VSWING_3	0x0F   // 1.2
#define VAL_DPTX_PHY_PREEM_0	0x00   // 0 db
#define VAL_DPTX_PHY_PREEM_1	0x05   // 3.5 db
#define VAL_DPTX_PHY_PREEM_2	0x0a   // 6 db
#define VAL_DPTX_PHY_PREEM_3	0x0f   // 9.5 db

extern u16 DP_training_rd_interval[5];
extern char *edp_ver_str[5];

enum DPCD_level_e { //both vswing and preem
	VAL_DP_std_LEVEL_0 = 0,
	VAL_DP_std_LEVEL_1,
	VAL_DP_std_LEVEL_2,
	VAL_DP_std_LEVEL_3,
};

//ANACTRL phy value(0~0xf) to DP standard value(2bit)
enum DPCD_level_e dptx_vswing_phy_to_std(unsigned int phy_vswing);
enum DPCD_level_e dptx_preem_phy_to_std(unsigned int phy_preem);

//DP standard value (2bit) to ANACTRL phy value(0~0xf)
unsigned char dptx_vswing_std_to_phy(enum DPCD_level_e tx_vswing);
unsigned char dptx_preem_std_to_phy(enum DPCD_level_e tx_preem);
/* helper func to transform from DP standard value(2bit) to DPCD TRAINING_LANEx_SET
 * Bits 1:0 = VOLTAGE SWING SET
 * Bit 2 = MAX_SWING_REACHED
 * Bit 4:3 = PRE-EMPHASIS_SET
 * Bit 5 = MAX_PRE-EMPHASIS_REACHED
 */
unsigned char to_DPCD_LANESET(enum DPCD_level_e vswing, enum DPCD_level_e preem);

//! training
// Link training constants
#define DP_FULL_LINK_TRAINING_ATTEMPTS 3
#define DP_TRAINING_LINKRATE_ATTEMPTS  3
#define DP_CLOCK_REC_TIMEOUT    150
#define DP_TRAINING_EQ_ATTEMPTS 5
#define DP_TRAINING_CR_ATTEMPTS 5
enum DP_training_status_e {
	DP_TRAINING_CLOCK_REC,
	DP_TRAINING_CHANNEL_EQ,
	DP_TRAINING_SUCCESS,
	DP_TRAINING_FAILED,
	DP_TRAINING_ADJ_SPD_CR_FAIL,
	DP_TRAINING_ADJ_SPD_CR_FAIL_IN_EQ,
	DP_TRAINING_ADJ_SPD_EQ_FAIL_OVERTIME,
};

enum DP_training_mode_e {
	DP_LINK_TRAINING_AUTO,
	DP_FAST_LINK_TRAINING,
	DP_FULL_LINK_TRAINING,
};

int dptx_link_training(struct aml_lcd_drv_s *pdrv);
int dptx_full_link_training(struct aml_lcd_drv_s *pdrv);
int dptx_fast_link_training(struct aml_lcd_drv_s *pdrv);

//! EDID
#define DPTX_EDID_READ_RETRY_MAX 5
enum DP_EDID_BLOCK_IDENTITY_e {
	BLOCK_ID_SN            = 0xff,
	BLOCK_ID_ASCII_STR     = 0xfe,
	BLOCK_ID_RANGE_TIMING  = 0xfd,
	BLOCK_ID_PRODUCK_NAME  = 0xfc,
	BLOCK_ID_DETAIL_TIMING = 0x01
};

enum DP_EDID_TIMING_FLAG_e {
	TIMING_FLAG_VALID = BIT(0),
	TIMING_FLAG_DETAIL = BIT(1),
	TIMING_FLAG_PRESET = BIT(2),
	TIMING_FLAG_FR_STEP = BIT(3),
	TIMING_FLAG_SUPPORT = BIT(4),
};

struct dptx_detail_timing_s {
	unsigned int pclk;
	unsigned short h_a;
	unsigned short h_b;
	unsigned short v_a;
	unsigned short v_b;
	unsigned short h_fp;
	unsigned short h_pw;
	unsigned short v_fp;
	unsigned short v_pw;
	unsigned int h_size;
	unsigned int v_size;
	unsigned short h_border;
	unsigned short v_border;
	unsigned int timing_ctrl;

	/* @var: flag:
	 * [0]: valid(unset);
	 * [1]: is detail timing
	 * [2]: is preset timing;
	 * [3]: is fr_step timing;
	 * [4]: supported;
	 */
	enum DP_EDID_TIMING_FLAG_e flag;
	unsigned char timing_res;

};

struct dptx_range_limit_s {
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

struct dptx_EDID_s {
	unsigned char manufacturer_id[4];     //[8:9]2byte
	unsigned short product_id;     //[10:11]2byte
	unsigned int product_sn;       //[12:15]4byte
	unsigned char week;     //[16]1byte
	unsigned int year;     //[17]1byte
	unsigned short version;  //[18:19]2byte
	unsigned int established_timing; //[35:37]3byte
	unsigned int standard_timing1;   //[38:45]4byte
	unsigned int standard_timing2;   //[46:53]4byte

	enum DP_EDID_BLOCK_IDENTITY_e block_identity[4];
	unsigned char name[14]; //include "\0"
	unsigned char serial_num[14];
	unsigned char asc_string[14];
	struct dptx_range_limit_s range_limit;
	struct dptx_detail_timing_s detail_timing[4];

	unsigned char ext_flag;  //[126]1byte
	//[127]1byte, 256-(sum(byte0:126)%256) =? 0x100-(sum(byte0:126) & 0xff)
	unsigned char checksum;
};

int dptx_EDID_probe(struct aml_lcd_drv_s *pdrv, struct dptx_EDID_s *edp_edid);

//! timing
#define DP_MAX_TIMING 30
#define DP_EDID_BLOCK_NUM 4
#define DP_FR_STEP_NUM 7
extern unsigned char DP_fr_step[DP_FR_STEP_NUM];
enum DP_TIMING_RES {
	DP_TIMING_SPEC        = 0,
	DP_TIMING_1080P_LOWER = 1,
	DP_TIMING_1080P       = 2,
	DP_TIMING_2K_1080p    = 3,
	DP_TIMING_2K          = 4,
	DP_TIMING_4K_2K       = 5,
	DP_TIMING_4K          = 6,
	DP_TIMING_4K_UPPER    = 7
};

unsigned char dptx_check_timing(struct aml_lcd_drv_s *pdrv, struct dptx_detail_timing_s *timing);
void dptx_timing_update(struct aml_lcd_drv_s *pdrv, struct dptx_detail_timing_s *timing);
void dptx_timing_apply(struct aml_lcd_drv_s *pdrv);
void dptx_manage_timing(struct aml_lcd_drv_s *pdrv, struct dptx_EDID_s *EDID_p);
void dptx_clear_timing(struct aml_lcd_drv_s *pdrv);
void dptx_print_timing(struct aml_lcd_drv_s *pdrv, unsigned char print_flag);
struct dptx_detail_timing_s *dptx_get_timing(struct aml_lcd_drv_s *pdrv, u8 th);
struct dptx_detail_timing_s *dptx_get_optimum_timing(struct aml_lcd_drv_s *pdrv);

/* dptx func */
int  dptx_set_phy_config(struct aml_lcd_drv_s *pdrv, unsigned char use_known);
int  dptx_set_lane_config(struct aml_lcd_drv_s *pdrv);
void dptx_set_clk_config(struct aml_lcd_drv_s *pdrv);
void dptx_set_msa(struct aml_lcd_drv_s *pdrv);
//part: 0:combo dphy; 1:eDP pipeline; 2:eDP ctrl
void dptx_reset_part(struct aml_lcd_drv_s *pdrv, unsigned char part);
void dptx_reset(struct aml_lcd_drv_s *pdrv);
int dptx_wait_phy_ready(struct aml_lcd_drv_s *pdrv);
int dptx_band_width_check(enum DP_link_rate_e link_rate, u8 lane_cnt, u32 pclk, u8 bit_per_pixel);
int dptx_link_config_update(struct aml_lcd_drv_s *pdrv);

struct DP_dev_support_s {
	u8 DPCD_rev;
	u32 link_rate;
	u8 line_cnt;
	u8 enhanced_frame;
	u8 TPS_support; //[0]:TPS3; [1]:TPS4
	u8 down_spread;
	u8 coding_support; //[0]:8b/10b; [1]:128b/132b
	u8 msa_timing_par_ignored;
	u8 train_aux_rd_interval; //0:400us, 1:4ms, 2:8ms, 3:12ms, 4:16ms
	u8 extended_receiver_cap; //DPCD[2200h~22ffh]
	u8 DACP_support; //[0]:1.HDCP; [1]:panel auth; [2]:eDP ASSR

	u8 edp_ver;
	u8 edp_DPCD_reg; //DPCD[0700h~07ffh]

	u16 h_active;
	u16 v_active;
	u16 frame_rate;
};

//! Content Protect
void dptx_set_ContentProtection(struct aml_lcd_drv_s *pdrv);

#define CAP_COMP(X, Y) ({typeof(X) x_ = (X); typeof(Y) y_ = (Y); (x_ < y_) ? x_ : y_; })
extern struct DP_dev_support_s source_support_T7;

#endif
