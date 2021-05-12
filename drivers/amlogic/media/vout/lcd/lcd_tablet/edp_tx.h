/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef EDP_TX_H
#define EDP_TX_H

#define DPTX_EDID_READ_RETRY_MAX        5

#define DPTX_AUX_CMD_WRITE		0x8
#define DPTX_AUX_CMD_READ		0x9
#define DPTX_AUX_CMD_I2C_WRITE		0x0
#define DPTX_AUX_CMD_I2C_WRITE_MOT	0x4
#define DPTX_AUX_CMD_I2C_READ		0x1
#define DPTX_AUX_CMD_I2C_READ_MOT	0x5
#define DPTX_AUX_CMD_I2C_WRITE_STATUS	0x2

#define DPTX_AUX_REPLY_CODE_ACK		0
#define DPTX_AUX_REPLY_CODE_NACK	BIT(0)
#define DPTX_AUX_REPLY_CODE_DEFER	BIT(1)
#define DPTX_AUX_REPLY_CODE_I2C_NACK	BIT(2)
#define DPTX_AUX_REPLY_CODE_I2C_DEFER	BIT(3)

struct dptx_aux_req_s {
	unsigned int cmd_code;
	unsigned int cmd_state; /* 0:write, 1:read */
	unsigned int address;
	unsigned int byte_cnt;
	unsigned char *data;
};

struct dptx_edid_timing_s {
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

struct dptx_edid_range_limit_s {
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

struct dptx_edid_s {
	unsigned char manufacturer_id[4];     //[8:9]2byte
	unsigned short product_id;     //[10:11]2byte
	unsigned int product_sn;       //[12:15]4byte
	unsigned char week;     //[16]1byte
	unsigned int year;     //[17]1byte
	unsigned short version;  //[18:19]2byte
	unsigned int established_timing; //[35:37]3byte
	unsigned int standard_timing1;   //[38:45]4byte
	unsigned int standard_timing2;   //[46:53]4byte
	struct dptx_edid_timing_s preferred_timing;
	unsigned int string_flag; //[2]serial_num, [1]asc_string, [0]name
	unsigned char name[14]; //include "\0"
	unsigned char serial_num[14];
	unsigned char asc_string[14];
	struct dptx_edid_range_limit_s range_limit;
	unsigned char ext_flag;  //[126]1byte
	//[127]1byte, 256-(sum(byte0:126)%256) =? 0x100-(sum(byte0:126) & 0xff)
	unsigned char checksum;
};

#endif
