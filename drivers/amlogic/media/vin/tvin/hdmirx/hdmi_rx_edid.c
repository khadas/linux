/*
 * drivers/amlogic/media/vin/tvin/hdmirx/hdmi_rx_edid.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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
#include "hdmi_rx_drv.h"
#include "hdmi_rx_edid.h"
#include "hdmi_rx_hw.h"

unsigned char edid_temp[EDID_SIZE];
static char edid_buf[MAX_EDID_BUF_SIZE] = {0x0};
static int edid_size;
struct edid_data_s tmp_edid_data;
int arc_port_id;
bool need_support_atmos_bit;
static unsigned char receive_hdr_lum[3];

static unsigned int earc_cap_ds_len;
static unsigned char recv_earc_cap_ds[EARC_CAP_DS_MAX_LENGTH] = {0};
bool new_earc_cap_ds;
static uint8_t com_aud[DB_LEN_MAX-1] = {0};

int edid_mode;
int port_map = 0x4231;
MODULE_PARM_DESC(port_map, "\n port_map\n");
module_param(port_map, int, 0664);

bool new_hdr_lum;
MODULE_PARM_DESC(new_hdr_lum, "\n new_hdr_lum\n");
module_param(new_hdr_lum, bool, 0664);

/*
 * 1:reset hpd after atmos edid update
 * 0:not reset hpd after atmos edid update
 */
bool atmos_edid_update_hpd_en = 1;
/* if free space is not enough in edid to do
 * data blk splice, then remove the last dtd(s)
 */
bool en_take_dtd_space = true;
/*
 * 1:reset hpd after cap ds edid update
 * 0:not reset hpd after cap ds edid update
 */
bool earc_cap_ds_update_hpd_en = true;


/* hdmi1.4 edid */
static unsigned char edid_14[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
0x05, 0xAC, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00,
0x2B, 0x1B, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78,
0x0A, 0xCF, 0x74, 0xA3, 0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x21, 0x08, 0x00, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3A,
0x80, 0x18, 0x71, 0x38, 0x2D, 0x40, 0x58, 0x2C,
0x45, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E,
0x01, 0x1D, 0x00, 0xBC, 0x52, 0xD0, 0x1E, 0x20,
0xB8, 0x28, 0x55, 0x40, 0x20, 0xC2, 0x31, 0x00,
0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x41,
0x4D, 0x4C, 0x20, 0x54, 0x56, 0x0A, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x94,
0x02, 0x03, 0x35, 0xF0, 0x53, 0x10, 0x1F, 0x14,
0x05, 0x13, 0x04, 0x20, 0x22, 0x3C, 0x3E, 0x12,
0x16, 0x03, 0x07, 0x11, 0x15, 0x02, 0x06, 0x01,
0x26, 0x09, 0x07, 0x03, 0x15, 0x07, 0x50, 0x83,
0x01, 0x00, 0x00, 0x6E, 0x03, 0x0C, 0x00, 0x10,
0x00, 0xB8, 0x3C, 0x20, 0x80, 0x80, 0x01, 0x02,
0x03, 0x04, 0xE2, 0x00, 0xFB, 0x02, 0x3A, 0x80,
0xD0, 0x72, 0x38, 0x2D, 0x40, 0x10, 0x2C, 0x45,
0x80, 0x30, 0xEB, 0x52, 0x00, 0x00, 0x1F, 0x8C,
0x0A, 0xD0, 0x8A, 0x20, 0xE0, 0x2D, 0x10, 0x10,
0x3E, 0x96, 0x00, 0x13, 0x8E, 0x21, 0x00, 0x00,
0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3B,
};

/* 1.4 edid,support dobly,MAT,DTS,ATMOS*/
static unsigned char edid_14_aud[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
0x05, 0xAC, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00,
0x2B, 0x1B, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78,
0x0A, 0xCF, 0x74, 0xA3, 0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x21, 0x08, 0x00, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3A,
0x80, 0x18, 0x71, 0x38, 0x2D, 0x40, 0x58, 0x2C,
0x45, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E,
0x01, 0x1D, 0x00, 0xBC, 0x52, 0xD0, 0x1E, 0x20,
0xB8, 0x28, 0x55, 0x40, 0x20, 0xC2, 0x31, 0x00,
0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x41,
0x4D, 0x4C, 0x20, 0x54, 0x56, 0x0A, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x94,
0x02, 0x03, 0x41, 0xF0, 0x53, 0x10, 0x1F, 0x14,
0x05, 0x13, 0x04, 0x20, 0x22, 0x3C, 0x3E, 0x12,
0x16, 0x03, 0x07, 0x11, 0x15, 0x02, 0x06, 0x01,
0x32, 0x09, 0x07, 0x03, 0x15, 0x07, 0x50, 0x57,
0x07, 0x01, 0x67, 0x7E, 0x00, 0x0F, 0x7F, 0x07,
0x3D, 0x07, 0xC0, 0x83, 0x01, 0x00, 0x00, 0x6E,
0x03, 0x0C, 0x00, 0x10, 0x00, 0xB8, 0x3C, 0x20,
0x80, 0x80, 0x01, 0x02, 0x03, 0x04, 0xE2, 0x00,
0xFB, 0x02, 0x3A, 0x80, 0xD0, 0x72, 0x38, 0x2D,
0x40, 0x10, 0x2C, 0x45, 0x80, 0x30, 0xEB, 0x52,
0x00, 0x00, 0x1F, 0x8C, 0x0A, 0xD0, 0x8A, 0x20,
0xE0, 0x2D, 0x10, 0x10, 0x3E, 0x96, 0x00, 0x13,
0x8E, 0x21, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46,
};

/* 1.4 edid,support 420 capability map block*/
static unsigned char edid_14_420c[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
0x05, 0xAC, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00,
0x2B, 0x1B, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78,
0x0A, 0xCF, 0x74, 0xA3, 0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x21, 0x08, 0x00, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3A,
0x80, 0x18, 0x71, 0x38, 0x2D, 0x40, 0x58, 0x2C,
0x45, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E,
0x01, 0x1D, 0x00, 0xBC, 0x52, 0xD0, 0x1E, 0x20,
0xB8, 0x28, 0x55, 0x40, 0x20, 0xC2, 0x31, 0x00,
0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x41,
0x4D, 0x4C, 0x20, 0x54, 0x56, 0x0A, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x94,
0x02, 0x03, 0x3E, 0xF0, 0x53, 0x10, 0x1F, 0x14,
0x05, 0x13, 0x04, 0x20, 0x22, 0x3C, 0x3E, 0x12,
0x16, 0x03, 0x07, 0x11, 0x15, 0x02, 0x06, 0x01,
0x29, 0x09, 0x07, 0x03, 0x15, 0x07, 0x50, 0x57,
0x07, 0x01, 0x83, 0x01, 0x00, 0x00, 0x6E, 0x03,
0x0C, 0x00, 0x10, 0x00, 0xB8, 0x3C, 0x20, 0x80,
0x80, 0x01, 0x02, 0x03, 0x04, 0xE2, 0x00, 0xFB,
0xE5, 0x0E, 0x60, 0x61, 0x65, 0x66, 0x02, 0x3A,
0x80, 0xD0, 0x72, 0x38, 0x2D, 0x40, 0x10, 0x2C,
0x45, 0x80, 0x30, 0xEB, 0x52, 0x00, 0x00, 0x1F,
0x8C, 0x0A, 0xD0, 0x8A, 0x20, 0xE0, 0x2D, 0x10,
0x10, 0x3E, 0x96, 0x00, 0x13, 0x8E, 0x21, 0x00,
0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x51,
};

/* 1.4 edid,support 420  video data block*/
static unsigned char edid_14_420vd[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
0x05, 0xAC, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00,
0x2B, 0x1B, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78,
0x0A, 0xCF, 0x74, 0xA3, 0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x21, 0x08, 0x00, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3A,
0x80, 0x18, 0x71, 0x38, 0x2D, 0x40, 0x58, 0x2C,
0x45, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E,
0x01, 0x1D, 0x00, 0xBC, 0x52, 0xD0, 0x1E, 0x20,
0xB8, 0x28, 0x55, 0x40, 0x20, 0xC2, 0x31, 0x00,
0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x41,
0x4D, 0x4C, 0x20, 0x54, 0x56, 0x0A, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x94,
0x02, 0x03, 0x41, 0xF0, 0x57, 0x10, 0x1F, 0x14,
0x05, 0x13, 0x04, 0x20, 0x22, 0x3C, 0x3E, 0x12,
0x16, 0x03, 0x07, 0x11, 0x15, 0x02, 0x06, 0x01,
0x60, 0x5F, 0x65, 0x66, 0x29, 0x09, 0x07, 0x03,
0x15, 0x07, 0x50, 0x57, 0x07, 0x01, 0x83, 0x01,
0x00, 0x00, 0x6E, 0x03, 0x0C, 0x00, 0x10, 0x00,
0xB8, 0x3C, 0x20, 0x80, 0x80, 0x01, 0x02, 0x03,
0x04, 0xE2, 0x00, 0xFB, 0xE4, 0x0F, 0x00, 0x00,
0x78, 0x02, 0x3A, 0x80, 0xD0, 0x72, 0x38, 0x2D,
0x40, 0x10, 0x2C, 0x45, 0x80, 0x30, 0xEB, 0x52,
0x00, 0x00, 0x1F, 0x8C, 0x0A, 0xD0, 0x8A, 0x20,
0xE0, 0x2D, 0x10, 0x10, 0x3E, 0x96, 0x00, 0x13,
0x8E, 0x21, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD4,
};

/* 2.0 edid,support HDR,DOLBYVISION */
static unsigned char edid_20[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
0x05, 0xAC, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00,
0x20, 0x19, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78,
0x0A, 0xCF, 0x74, 0xA3, 0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x21, 0x08, 0x00, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x04, 0x74,
0x00, 0x30, 0xF2, 0x70, 0x5A, 0x80, 0xB0, 0x58,
0x8A, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E,
0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40,
0x58, 0x2C, 0x45, 0x00, 0x20, 0xC2, 0x31, 0x00,
0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x41,
0x4D, 0x4C, 0x20, 0x54, 0x56, 0x0A, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xDA,
0x02, 0x03, 0x62, 0xF0, 0x5B, 0x5F, 0x10, 0x1F,
0x14, 0x05, 0x13, 0x04, 0x20, 0x22, 0x3C, 0x3E,
0x12, 0x16, 0x03, 0x07, 0x11, 0x15, 0x02, 0x06,
0x01, 0x61, 0x5D, 0x64, 0x65, 0x66, 0x62, 0x60,
0x29, 0x09, 0x07, 0x03, 0x15, 0x07, 0x50, 0x57,
0x07, 0x01, 0x83, 0x01, 0x00, 0x00, 0x6E, 0x03,
0x0C, 0x00, 0x10, 0x00, 0xB8, 0x3C, 0x2F, 0x80,
0x80, 0x01, 0x02, 0x03, 0x04, 0x67, 0xD8, 0x5D,
0xC4, 0x01, 0x78, 0x88, 0x07, 0xE3, 0x05, 0x40,
0x01, 0xE5, 0x0F, 0x00, 0x00, 0x90, 0x05, 0xE3,
0x06, 0x05, 0x01, 0xEE, 0x01, 0x46, 0xD0, 0x00,
0x26, 0x0F, 0x8B, 0x00, 0xA8, 0x53, 0x4B, 0x9D,
0x27, 0x0B, 0x02, 0x3A, 0x80, 0xD0, 0x72, 0x38,
0x2D, 0x40, 0x10, 0x2C, 0x45, 0x80, 0x30, 0xEB,
0x52, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6C,
};

/* correspond to enum hdmi_vic_e */
const char *hdmi_fmt[] = {
	"HDMI_UNKNOWN",
	"HDMI_640x480p60_4x3",
	"HDMI_720x480p60_4x3",
	"HDMI_720x480p60_16x9",
	"HDMI_1280x720p60_16x9",
	"HDMI_1920x1080i60_16x9",
	"HDMI_720x480i60_4x3",
	"HDMI_720x480i60_16x9",
	"HDMI_720x240p60_4x3",
	"HDMI_720x240p60_16x9",
	"HDMI_2880x480i60_4x3",
	"HDMI_2880x480i60_16x9",
	"HDMI_2880x240p60_4x3",
	"HDMI_2880x240p60_16x9",
	"HDMI_1440x480p60_4x3",
	"HDMI_1440x480p60_16x9",
	"HDMI_1920x1080p60_16x9",
	"HDMI_720x576p50_4x3",
	"HDMI_720x576p50_16x9",
	"HDMI_1280x720p50_16x9",
	"HDMI_1920x1080i50_16x9",
	"HDMI_720x576i50_4x3",
	"HDMI_720x576i50_16x9",
	"HDMI_720x288p_4x3",
	"HDMI_720x288p_16x9",
	"HDMI_2880x576i50_4x3",
	"HDMI_2880x576i50_16x9",
	"HDMI_2880x288p50_4x3",
	"HDMI_2880x288p50_16x9",
	"HDMI_1440x576p_4x3",
	"HDMI_1440x576p_16x9",
	"HDMI_1920x1080p50_16x9",
	"HDMI_1920x1080p24_16x9",
	"HDMI_1920x1080p25_16x9",
	"HDMI_1920x1080p30_16x9",
	"HDMI_2880x480p60_4x3",
	"HDMI_2880x480p60_16x9",
	"HDMI_2880x576p50_4x3",
	"HDMI_2880x576p50_16x9",
	"HDMI_1920x1080i_t1250_50_16x9",
	"HDMI_1920x1080i100_16x9",
	"HDMI_1280x720p100_16x9",
	"HDMI_720x576p100_4x3",
	"HDMI_720x576p100_16x9",
	"HDMI_720x576i100_4x3",
	"HDMI_720x576i100_16x9",
	"HDMI_1920x1080i120_16x9",
	"HDMI_1280x720p120_16x9",
	"HDMI_720x480p120_4x3",
	"HDMI_720x480p120_16x9",
	"HDMI_720x480i120_4x3",
	"HDMI_720x480i120_16x9",
	"HDMI_720x576p200_4x3",
	"HDMI_720x576p200_16x9",
	"HDMI_720x576i200_4x3",
	"HDMI_720x576i200_16x9",
	"HDMI_720x480p240_4x3",
	"HDMI_720x480p240_16x9",
	"HDMI_720x480i240_4x3",
	"HDMI_720x480i240_16x9",
	/* Refet to CEA 861-F */
	"HDMI_1280x720p24_16x9",
	"HDMI_1280x720p25_16x9",
	"HDMI_1280x720p30_16x9",
	"HDMI_1920x1080p120_16x9",
	"HDMI_1920x1080p100_16x9",
	"HDMI_1280x720p24_64x27",
	"HDMI_1280x720p25_64x27",
	"HDMI_1280x720p30_64x27",
	"HDMI_1280x720p50_64x27",
	"HDMI_1280x720p60_64x27",
	"HDMI_1280x720p100_64x27",
	"HDMI_1280x720p120_64x27",
	"HDMI_1920x1080p24_64x27",
	"HDMI_1920x1080p25_64x27",
	"HDMI_1920x1080p30_64x27",
	"HDMI_1920x1080p50_64x27",
	"HDMI_1920x1080p60_64x27",
	"HDMI_1920x1080p100_64x27",
	"HDMI_1920x1080p120_64x27",
	"HDMI_1680x720p24_64x27",
	"HDMI_1680x720p25_64x27",
	"HDMI_1680x720p30_64x27",
	"HDMI_1680x720p50_64x27",
	"HDMI_1680x720p60_64x27",
	"HDMI_1680x720p100_64x27",
	"HDMI_1680x720p120_64x27",
	"HDMI_2560x1080p24_64x27",
	"HDMI_2560x1080p25_64x27",
	"HDMI_2560x1080p30_64x27",
	"HDMI_2560x1080p50_64x27",
	"HDMI_2560x1080p60_64x27",
	"HDMI_2560x1080p100_64x27",
	"HDMI_2560x1080p120_64x27",
	"HDMI_3840x2160p24_16x9",
	"HDMI_3840x2160p25_16x9",
	"HDMI_3840x2160p30_16x9",
	"HDMI_3840x2160p50_16x9",
	"HDMI_3840x2160p60_16x9",
	"HDMI_4096x2160p24_256x135",
	"HDMI_4096x2160p25_256x135",
	"HDMI_4096x2160p30_256x135",
	"HDMI_4096x2160p50_256x135",
	"HDMI_4096x2160p60_256x135",
	"HDMI_3840x2160p24_64x27",
	"HDMI_3840x2160p25_64x27",
	"HDMI_3840x2160p30_64x27",
	"HDMI_3840x2160p50_64x27",
	"HDMI_3840x2160p60_64x27",
	"HDMI_RESERVED",
};

const char *_3d_structure[] = {
	/* hdmi1.4 spec, Table H-7 */
	/* value: 0x0000 */
	"Frame packing",
	"Field alternative",
	"Line alternative",
	"Side-by-Side(Full)",
	"L + depth",
	"L + depth + graphics + graphics-depth",
	"Top-and-Bottom",
	/* value 0x0111: Reserved for future use */
	"Resvrd",
	/* value 0x1000 */
	"Side-by-Side(Half) with horizontal sub-sampling",
	/* value 0x1001-0x1110:Reserved for future use */
	"Resvrd",
	"Resvrd",
	"Resvrd",
	"Resvrd",
	"Resvrd",
	"Resvrd",
	"Side-by-Side(Half) with all quincunx sub-sampling",
};

const char *_3d_detail_x[] = {
	/* hdmi1.4 spec, Table H-8 */
	/* value: 0x0000 */
	"All horizontal sub-sampling and quincunx matrix",
	"Horizontal sub-sampling",
	"Not_in_Use1",
	"Not_in_Use2",
	"Not_in_Use3",
	"Not_in_Use4",
	"All_Quincunx",
	"ODD_left_ODD_right",
	"ODD_left_EVEN_right",
	"EVEN_left_ODD_right",
	"EVEN_left_EVEN_right",
	"Resvrd1",
	"Resvrd2",
	"Resvrd3",
	"Resvrd4",
	"Resvrd5",
};

const char *aud_fmt[] = {
	"HEADER",
	"L-PCM",
	"AC3",
	"MPEG1",
	"MP3",
	"MPEG2",
	"AAC",
	"DTS",
	"ATRAC",
	"OBA",
	"DDP", /* E_AC3 */
	"DTS_HD",
	"MAT", /* TRUE_HD*/
	"DST",
	"WMAPRO",
};

unsigned char *edid_list[] = {
	edid_buf,
	edid_14,
	edid_14_aud,
	edid_14_420c,
	edid_14_420vd,
	edid_20,
};

static struct cta_blk_pair cta_blk[] = {
	{
		.tag_code = AUDIO_TAG,
		.blk_name = "Audio_DB",
	},
	{
		.tag_code = VIDEO_TAG,
		.blk_name = "Video_DB",
	},
	{
		.tag_code = VENDOR_TAG,
		.blk_name = "Vendor_Specific_DB",
	},
	{
		.tag_code = HF_VENDOR_DB_TAG,
		.blk_name = "HF_Vendor_Specific_DB",
	},
	{
		.tag_code = SPEAKER_TAG,
		.blk_name = "Speaker_Alloc_DB",
	},
	{
		.tag_code = VDTCDB_TAG,
		.blk_name = "VESA_DTC_DB",
	},
	{
		.tag_code = (USE_EXTENDED_TAG << 8) | VCDB_TAG,
		.blk_name = "Video_Cap_DB",
	},
	{
		.tag_code = (USE_EXTENDED_TAG << 8) | VSVDB_TAG,
		.blk_name = "Vendor_Specific_Video_DB",
	},
	{
		.tag_code = (USE_EXTENDED_TAG << 8) | VDDDB_TAG,
		.blk_name = "VESA_Display_Device_DB",
	},
	{
		.tag_code = (USE_EXTENDED_TAG << 8) | VVTBE_TAG,
		.blk_name = "VESA_Video_Timing_Blk_Ext",
	},
	{
		.tag_code = (USE_EXTENDED_TAG << 8) | CDB_TAG,
		.blk_name = "Colorimetry_DB",
	},
	{
		.tag_code = (USE_EXTENDED_TAG << 8) | HDR_STATIC_TAG,
		.blk_name = "HDR_Static_Metadata",
	},
	{
		.tag_code = (USE_EXTENDED_TAG << 8) | HDR_DYNAMIC_TAG,
		.blk_name = "HDR_Dynamic_Metadata",
	},
	{
		.tag_code = (USE_EXTENDED_TAG << 8) | VFPDB_TAG,
		.blk_name = "Video_Fmt_Preference_DB",
	},
	{
		.tag_code = (USE_EXTENDED_TAG << 8) | Y420VDB_TAG,
		.blk_name = "YCbCr_420_Video_DB",
	},
	{
		.tag_code = (USE_EXTENDED_TAG << 8) | Y420CMDB_TAG,
		.blk_name = "YCbCr_420_Cap_Map_DB",
	},
	{
		.tag_code = (USE_EXTENDED_TAG << 8) | VSADB_TAG,
		.blk_name = "Vendor_Specific_Audio_DB",
	},
	{
		.tag_code = (USE_EXTENDED_TAG << 8) | RCDB_TAG,
		.blk_name = "Room_Configuration_DB",
	},
	{
		.tag_code = (USE_EXTENDED_TAG << 8) | SLDB_TAG,
		.blk_name = "Speaker_Location_DB",
	},
	{
		.tag_code = (USE_EXTENDED_TAG << 8) | IFDB_TAG,
		.blk_name = "Infoframe_DB",
	},
	{
		.tag_code = TAG_MAX,
		.blk_name = "Unrecognized_DB",
	},
	{},
};

char *rx_get_cta_blk_name(uint16_t tag)
{
	int i = 0;

	for (i = 0; cta_blk[i].tag_code != TAG_MAX; i++) {
		if (tag == cta_blk[i].tag_code)
			break;
	}
	return cta_blk[i].blk_name;
}

unsigned char rx_get_edid_index(void)
{
	if ((edid_mode < EDID_LIST_NUM) &&
		(edid_mode > EDID_LIST_TOP))
		return edid_mode;
	if ((edid_mode == 0) &&
		edid_size > 4 &&
		edid_buf[0] == 'E' &&
		edid_buf[1] == 'D' &&
		edid_buf[2] == 'I' &&
		edid_buf[3] == 'D') {
		rx_pr("edid: use Top edid\n");
		return EDID_LIST_TOP;
	}
	return EDID_LIST_NULL;
}

unsigned char *rx_get_edid_buffer(unsigned char index)
{
	if (index == EDID_LIST_TOP)
		return edid_list[index] + 4;
	else
		return edid_list[index];
}

unsigned int rx_get_edid_size(unsigned char index)
{
	if (index == EDID_LIST_TOP)
		return edid_size - 4;
	else if (index == EDID_LIST_NULL)
		return edid_size;
	else
		return EDID_SIZE;
}

unsigned char *rx_get_edid(int edid_index)
{
	/*int edid_index = rx_get_edid_index();*/

	memcpy(edid_temp, rx_get_edid_buffer(edid_index),
				rx_get_edid_size(edid_index));
	/*
	 *rx_pr("index=%d,get size=%d,edid_size=%d,rept=%d\n",
	 *			edid_index,
	 *			rx_get_edid_size(edid_index),
	 *			edid_size, hdmirx_repeat_support());
	 *rx_pr("edid update port map:%#x,up_phy_addr=%#x\n",
	 *			port_map, up_phy_addr);
	 */
	return edid_temp;
}

unsigned int hdmirx_read_edid_buf(char *buf, int max_size)
{
	if (edid_size > max_size) {
		rx_pr("Error: %s,edid size %d",
				__func__,
				edid_size);
		rx_pr(" is larger than the buf size of %d\n",
			max_size);
		return 0;
	}
	memcpy(buf, edid_buf, edid_size);
	rx_pr("HDMIRX: read edid buf\n");
	return edid_size;
}

void hdmirx_fill_edid_buf(const char *buf, int size)
{
	if (size > MAX_EDID_BUF_SIZE) {
		rx_pr("Error: %s,edid size %d",
				__func__,
				size);
		rx_pr(" is larger than the max size of %d\n",
			MAX_EDID_BUF_SIZE);
		return;
	}
	memcpy(edid_buf, buf, size);

	edid_size = size;
	rx_pr("HDMIRX: fill edid buf, size %d\n",
		size);
}

void rx_edid_update_hdr_info(
							unsigned char *p_edid,
							u_int idx)
{
	u_char hdr_edid[EDID_HDR_SIZE];

	if ((p_edid == NULL) || ((receive_hdr_lum[0] == 0)
		&& (receive_hdr_lum[1] == 0) && (receive_hdr_lum[2] == 0)))
		return;
	/*check if data is updated*/
	if (!(receive_hdr_lum[0] | receive_hdr_lum[1]
			| receive_hdr_lum[2]))
		return;
	/* update hdr info */
	hdr_edid[0] = ((EDID_TAG_HDR >> 3)&0xE0) + (6 & 0x1f);
	hdr_edid[1] = EDID_TAG_HDR & 0xFF;
	memcpy(hdr_edid + 4, receive_hdr_lum,
				sizeof(receive_hdr_lum));
	rx_modify_edid(p_edid, rx_get_edid_size(idx),
						hdr_edid);
}

unsigned int rx_exchange_bits(unsigned int value)
{
	unsigned int temp;

	temp = value & 0xF;
	value = (((value >> 4) & 0xF) | (value & 0xFFF0));
	value = ((value & 0xFF0F) | (temp << 4));
	temp = value & 0xF00;
	value = (((value >> 4) & 0xF00) | (value & 0xF0FF));
	value = ((value & 0x0FFF) | (temp << 4));
	return value;
}

int rx_get_tag_code(uint8_t *edid_data)
{
	int tag_code = TAG_MAX;
	unsigned int ieee_oui;

	if (!edid_data)
		return tag_code;
	/* extern tag */
	if ((*edid_data >> 5) == USE_EXTENDED_TAG)
		tag_code = (7 << 8) | *(edid_data + 1);
	else if ((*edid_data >> 5) == VENDOR_TAG) {
		/* diff VSDB with HF-VSDB */
		ieee_oui = (edid_data[3] << 16) |
			(edid_data[2] << 8) | edid_data[1];
		if (ieee_oui == 0x000C03)
			tag_code = (*edid_data >> 5);
		else if (ieee_oui == 0xC45DD8)
			tag_code =
				(*edid_data >> 5) + HF_VSDB_OFFSET;
	} else
		tag_code = (*edid_data >> 5);

	return tag_code;
}

int rx_get_ceadata_offset(uint8_t *cur_edid, uint8_t *addition)
{
	int i;
	int type;
	unsigned char max_offset;

	if ((cur_edid == NULL) || (addition == NULL))
		return 0;

	type = rx_get_tag_code(addition);
	i = EDID_DEFAULT_START;/*block check start index*/
	max_offset = cur_edid[130] + EDID_EXT_BLK_OFF;

	while (i < max_offset) {
		if (type == rx_get_tag_code(cur_edid + i))
			return i;
		i += (1 + (*(cur_edid + i) & 0x1f));
	}
	if (log_level & VIDEO_LOG)
		rx_pr("type: %#x, start addr: %#x\n", type, i);

	return 0;
}

void rx_mix_edid_audio(uint8_t *cur_data, uint8_t *addition)
{
	struct edid_audio_block_t *ori_data;
	struct edid_audio_block_t *add_data;
	unsigned char ori_len, add_len;
	int i, j;

	if ((cur_data == 0) || (addition == 0) ||
		(*cur_data >> 5) != (*addition >> 5))
		return;

	ori_data = (struct edid_audio_block_t *)(cur_data + 1);
	add_data = (struct edid_audio_block_t *)(addition + 1);
	ori_len = (*cur_data & 0x1f)/FORMAT_SIZE;
	add_len = (*addition & 0x1f)/FORMAT_SIZE;
	if (log_level & VIDEO_LOG)
		rx_pr("mix audio format ori len:%d,add len:%d\n",
							ori_len, add_len);
	for (i = 0; i < add_len; i++) {
		if (log_level & VIDEO_LOG)
			rx_pr("mix audio format:%d\n", add_data[i].format_code);
		/*only support lpcm dts dd+*/
		if (!is_audio_support(add_data[i].format_code))
			continue;
		/*mix audio data*/
		for (j = 0; j < ori_len; j++) {
			if (ori_data[j].format_code ==
						add_data[i].format_code) {
				if (log_level & VIDEO_LOG)
					rx_pr("mix audio mix format:%d\n",
						add_data[i].format_code);
				/*choose channel is lager*/
				ori_data[j].max_channel =
					((ori_data[j].max_channel >
					add_data[i].max_channel) ?
					ori_data[j].max_channel :
					add_data[i].max_channel);
				/*mix sample freqence*/
				*(((unsigned char *)&ori_data[j]) + 1) =
				*(((unsigned char *)&ori_data[j]) + 1) |
					*(((unsigned char *)&add_data[i]) + 1);
				/*mix bit rate*/
				if (add_data[i].format_code ==
							AUDIO_FORMAT_LPCM)
					*(((unsigned char *)&ori_data[j]) + 2) =
					*(((unsigned char *)&ori_data[j]) + 2) |
					*(((unsigned char *)&add_data[i]) + 2);
				else
					ori_data[j].bit_rate.others =
					add_data[i].bit_rate.others;
				break;
			}
			if (j == (ori_len - 1)) {
				if (log_level & VIDEO_LOG)
					rx_pr("mix audio add new format: %d\n",
					add_data[i].format_code);
				if (((*cur_data & 0x1f) + FORMAT_SIZE)
							 <= 0x1f) {
					memcpy(cur_data +
						 (*cur_data & 0x1f) + 1,
					&add_data[i], FORMAT_SIZE);
					*cur_data += FORMAT_SIZE;
				}
			}
		}
	}
}

void rx_mix_edid_hdr(uint8_t *cur_data, uint8_t *addition)
{
	struct edid_hdr_block_t *cur_block;
	struct edid_hdr_block_t *add_block;

	if ((cur_data == 0) || (addition == 0) ||
		(*cur_data >> 5) != (*addition >> 5))
		return;

	cur_block = (struct edid_hdr_block_t *)(cur_data + 1);
	add_block = (struct edid_hdr_block_t *)(addition + 1);
	if (add_block->max_lumi | add_block->avg_lumi |
					add_block->min_lumi) {
		cur_block->max_lumi = add_block->max_lumi;
		cur_block->avg_lumi = add_block->avg_lumi;
		cur_block->min_lumi = add_block->min_lumi;
		if ((*cur_data & 0x1f) < (*addition & 0x1f))
			*cur_data = *addition;
	}
}

int rx_edid_free_size(uint8_t *cur_edid, int size)
{
	int block_start;

	if (cur_edid == 0)
		return -1;
	/*get description offset*/
	block_start = cur_edid[EDID_BLOCK1_OFFSET +
							EDID_DESCRIP_OFFSET];
	block_start += EDID_BLOCK1_OFFSET;
	if (log_level & VIDEO_LOG)
		rx_pr("%s block_start:%d\n", __func__, block_start);
	/*find the empty data index*/
	while ((cur_edid[block_start] > 0) &&
					(block_start < size)) {
		if (log_level & VIDEO_LOG)
			rx_pr("%s running:%d\n", __func__, block_start);
		if ((cur_edid[block_start] & 0x1f) == 0)
			break;
		block_start += DETAILED_TIMING_LEN;
	}
	if (log_level & VIDEO_LOG)
		rx_pr("%s block_start end:%d\n", __func__, block_start);
	/*compute the free size*/
	if (block_start < (size - 1))
		return (size - block_start - 1);
	else
		return -1;
}

void rx_mix_block(uint8_t *cur_data, uint8_t *addition)
{
	int tag_code;

	if ((cur_data == 0) || (addition == 0) ||
		(*cur_data >> 5) != (*addition >> 5))
		return;
	if (log_level & VIDEO_LOG)
		rx_pr("before type:%d - %d,len:%d - %d\n",
	(*cur_data >> 5), (*addition >> 5),
	(*cur_data & 0x1f), (*addition & 0x1f));

	tag_code = rx_get_tag_code(cur_data);

	switch (tag_code) {
	case EDID_TAG_AUDIO:
		rx_mix_edid_audio(cur_data, addition);
		break;

	case EDID_TAG_HDR:
		rx_mix_edid_hdr(cur_data, addition);
		break;
	}
	if (log_level & VIDEO_LOG)
		rx_pr("end type:%d - %d,len:%d - %d\n",
	(*cur_data >> 5), (*addition >> 5),
	(*cur_data & 0x1f), (*addition & 0x1f));
}

void rx_modify_edid(unsigned char *buffer,
				int len, unsigned char *addition)
{
	int start_addr = 0;/*the addr in edid need modify*/
	int start_addr_temp = 0;/*edid_temp start addr*/
	int temp_len = 0;
	unsigned char *cur_data = rx.edid_mix_buf;
	int addition_size = 0;
	int cur_size = 0;
	int i, free_size;

	if ((len <= 255) || (buffer == 0) || (addition == 0))
		return;

	/*get the mix block value*/
	if (*addition & 0x1f) {
		/*get addition block index in local edid*/
		start_addr = rx_get_ceadata_offset(buffer, addition);
		if (log_level & VIDEO_LOG)
			rx_pr("%s start addr:%d\n", __func__, start_addr);
		if (start_addr > EDID_DEFAULT_START) {
			cur_size = (*(buffer + start_addr) & 0x1f) + 1;
			addition_size = (*addition & 0x1f) + 1;
			if (sizeof(rx.edid_mix_buf) >=
						(cur_size + addition_size))
				memcpy(cur_data, buffer + start_addr, cur_size);
			else
				return;
			if (log_level & VIDEO_LOG)
				rx_pr("%s mix size:%d\n", __func__,
					 (cur_size + addition_size));
			/*add addition block property to local edid*/
			rx_mix_block(cur_data, addition);
			addition_size = (*cur_data & 0x1f) + 1;
		} else
			return;
		if (log_level & VIDEO_LOG)
			rx_pr(
			"start_addr: %#x,cur_size: %d,addition_size: %d\n",
			start_addr, cur_size, addition_size);

		/*set the block value to edid_temp*/
		start_addr_temp = rx_get_ceadata_offset(buffer, addition);
		temp_len = ((buffer[start_addr_temp] & 0x1f) + 1);
		/*check the free size is enough for merged block*/
		free_size = rx_edid_free_size(buffer, EDID_SIZE);
		if (log_level & VIDEO_LOG)
			rx_pr("%s free_size:%d\n", __func__, free_size);
		if ((free_size < (addition_size - temp_len)) ||
			(free_size <= 0))
			return;
		if (log_level & VIDEO_LOG)
			rx_pr("edid_temp start: %#x, len: %d\n",
			start_addr_temp, temp_len);
		/*move data behind current data if need*/
		if (temp_len < addition_size) {
			for (i = 0; i < EDID_SIZE - start_addr_temp -
				addition_size; i++) {
				buffer[255 - i] =
				buffer[255 - (addition_size - temp_len)
						 - i];
			}
		} else if (temp_len > addition_size) {
			for (i = 0; i < EDID_SIZE - start_addr_temp -
						temp_len; i++) {
				buffer[start_addr_temp + i + addition_size]
				 = buffer[start_addr_temp + i + temp_len];
			}
		}
		/*check detail description offset if needs modify*/
		if (start_addr_temp < buffer[EDID_BLOCK1_OFFSET +
			EDID_DESCRIP_OFFSET] + EDID_BLOCK1_OFFSET)
			buffer[EDID_BLOCK1_OFFSET + EDID_DESCRIP_OFFSET]
				+= (addition_size - temp_len);
		/*copy current edid data*/
		memcpy(buffer + start_addr_temp, cur_data,
						addition_size);
	}
}

void rx_edid_update_audio_info(unsigned char *p_edid,
						unsigned int len)
{
	if (p_edid == NULL)
		return;
	rx_modify_edid(p_edid, len, rx_get_dw_edid_addr());
}

unsigned int rx_edid_cal_phy_addr(
					u_int brepeat,
					u_int up_addr,
					u_int portmap,
					u_char *pedid,
					u_int *phy_offset,
					u_int *phy_addr)
{
	u_int root_offset = 0;
	u_int i;
	u_int flag = 0;

	if (!(pedid && phy_offset && phy_addr))
		return -1;

	for (i = 0; i <= 255; i++) {
		/*find phy_addr_offset*/
		if (pedid[i] == 0x03) {
			if ((pedid[i+1] == 0x0c) &&
			(pedid[i+2] == 0x00)) {
				if (brepeat)
					pedid[i+3] = 0x00;
				else
					pedid[i+3] = 0x10;
				*phy_offset = i+3;
				flag = 1;
				break;
			}
		}
	}

	if (brepeat) {
		/*get the root index*/
		i = 0;
		while (i < 4) {
			if (((up_addr << (i*4)) & 0xf000) != 0) {
				root_offset = i;
				break;
			}
			i++;
		}
		if (i == 4)
			root_offset = 4;

		//rx_pr("portmap:%#x,rootoffset=%d,upaddr=%#x\n",
					//portmap,
					///root_offset,
					//up_addr);

		for (i = 0; i < E_PORT_NUM; i++) {
			if (root_offset == 0)
				phy_addr[i] = 0xFFFF;
			else
				phy_addr[i] = (up_addr |
				((((portmap >> i*4) & 0xf) << 12) >>
				(root_offset - 1)*4));

			phy_addr[i] = rx_exchange_bits(phy_addr[i]);
			//rx_pr("port %d phy:%d\n", i, phy_addr[i]);
		}
	} else {
		for (i = 0; i < E_PORT_NUM; i++)
			phy_addr[i] = ((portmap >> i*4) & 0xf) << 4;
	}

	return flag;
}

bool is_ddc_idle(unsigned char port_id)
{
	unsigned int sts;
	unsigned int ddc_sts;
	unsigned int ddc_offset;

	switch (port_id) {
	case 0:
		sts = hdmirx_rd_top(TOP_EDID_GEN_STAT);
		break;
	case 1:
		sts = hdmirx_rd_top(TOP_EDID_GEN_STAT_B);
		break;
	case 2:
		sts = hdmirx_rd_top(TOP_EDID_GEN_STAT_C);
		break;
	case 3:
		sts = hdmirx_rd_top(TOP_EDID_GEN_STAT_D);
		break;
	default:
		sts = 0;
		break;
	}

	ddc_sts = (sts >> 20) & 0x1f;
	ddc_offset = sts & 0xff;

	if ((ddc_sts == 0) &&
		((ddc_offset == 0xff) ||
		(ddc_offset == 0)))
		return true;

	if (log_level & ERR_LOG)
		rx_pr("ddc busy\n");

	return false;
}

void rx_edid_fill_to_register(
						u_char *pedid,
						u_int brepeat,
						u_int *pphy_addr,
						u_char *pchecksum)
{
	u_int i;
	u_int checksum = 0;
	u_int value = 0;
	u_int tmp_addr;

	if (!(pedid && pphy_addr && pchecksum))
		return;

	/* physical address info at second block */
	for (i = 128; i <= 255; i++) {
		value = pedid[i];
		if (i < 255) {
			checksum += pedid[i];
			checksum &= 0xff;
		} else if (i == 255) {
			value = (0x100 - checksum)&0xff;
		}
	}

	/* physical address info at second block */
	if (rx.chip_id < CHIP_ID_TL1)
		tmp_addr = TOP_EDID_OFFSET;
	else
		tmp_addr = TOP_EDID_ADDR_S;
	for (i = 0; i <= 255; i++) {
		/* fill first edid buffer */
		hdmirx_wr_top(tmp_addr + i,
			pedid[i]);
		/* fill second edid buffer */
		hdmirx_wr_top(tmp_addr + 0x100 + i,
			pedid[i]);
	}
	if (rx.chip_id == CHIP_ID_TM2) {
		for (i = 0; i <= 255; i++) {
			/* fill first edid buffer */
			hdmirx_wr_top(TOP_EDID_PORT2_ADDR_S + i,
				pedid[i]);
			/* fill second edid buffer */
			hdmirx_wr_top(TOP_EDID_PORT2_ADDR_S + 0x100 + i,
				pedid[i]);
		}
		for (i = 0; i <= 255; i++) {
			/* fill first edid buffer */
			hdmirx_wr_top(TOP_EDID_PORT3_ADDR_S + i,
				pedid[i]);
			/* fill second edid buffer */
			hdmirx_wr_top(TOP_EDID_PORT3_ADDR_S + 0x100 + i,
				pedid[i]);
		}
	}
	/* caculate 4 port check sum */
	if (brepeat) {
		for (i = 0; i < E_PORT_NUM; i++) {
			pchecksum[i] = (0x100 + value - (pphy_addr[i] & 0xFF) -
			((pphy_addr[i] >> 8) & 0xFF)) & 0xff;
			/*rx_pr("port %d phy:%d\n", i, pphy_addr[i]);*/
		}
	} else {
		for (i = 0; i < E_PORT_NUM; i++) {
			pchecksum[i] = (0x100 - (checksum +
				(pphy_addr[i] - 0x10))) & 0xff;
			/*rx_pr("port %d phy:%d\n", i, pphy_addr[i]);*/
		}
	}
}

void rx_edid_update_overlay(
						u_int phy_addr_offset,
						u_int *pphy_addr,
						u_char *pchecksum)
{
	u_int tmp_addr;

	if (!(pphy_addr && pchecksum))
		return;

	/* replace the first edid ram data */
	/* physical address byte 1 */
	hdmirx_wr_top(TOP_EDID_RAM_OVR2,
		(phy_addr_offset + 1) | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR2_DATA,
		((pphy_addr[E_PORT0] >> 8) & 0xFF) |
		 (((pphy_addr[E_PORT1] >> 8) & 0xFF)<<8)
			| (((pphy_addr[E_PORT2] >> 8) & 0xFF)<<16)
			| (((pphy_addr[E_PORT3] >> 8) & 0xFF)<<24));

	if (rx.chip_id == CHIP_ID_TM2) {
		tmp_addr = TOP_EDID_ADDR_S + phy_addr_offset + 1;
		hdmirx_wr_top(tmp_addr, pphy_addr[E_PORT0] >> 8);
		hdmirx_wr_top(tmp_addr + 0x200, pphy_addr[E_PORT1] >> 8);
		hdmirx_wr_top(tmp_addr + 0x400, pphy_addr[E_PORT2] >> 8);
	}
	/* physical address byte 0 */
	hdmirx_wr_top(TOP_EDID_RAM_OVR1,
		phy_addr_offset | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR1_DATA,
		(pphy_addr[E_PORT0] & 0xFF) | ((pphy_addr[E_PORT1] & 0xFF)<<8) |
			((pphy_addr[E_PORT2] & 0xFF)<<16) |
			((pphy_addr[E_PORT3] & 0xFF) << 24));

	if (rx.chip_id == CHIP_ID_TM2) {
		tmp_addr = TOP_EDID_ADDR_S + phy_addr_offset;
		hdmirx_wr_top(tmp_addr, pphy_addr[E_PORT0] & 0xff);
		hdmirx_wr_top(tmp_addr + 0x200, pphy_addr[E_PORT1] & 0xff);
		hdmirx_wr_top(tmp_addr + 0x400, pphy_addr[E_PORT2] & 0xff);
	}
	/* checksum */
	hdmirx_wr_top(TOP_EDID_RAM_OVR0,
		0xff | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR0_DATA,
			pchecksum[E_PORT0]|(pchecksum[E_PORT1]<<8)|
			(pchecksum[E_PORT2]<<16) | (pchecksum[E_PORT3] << 24));

	if (rx.chip_id == CHIP_ID_TM2) {
		tmp_addr = TOP_EDID_ADDR_S + 0xff;
		hdmirx_wr_top(tmp_addr, pchecksum[E_PORT0]);
		hdmirx_wr_top(tmp_addr + 0x200, pchecksum[E_PORT1]);
		hdmirx_wr_top(tmp_addr + 0x400, pchecksum[E_PORT2]);
	}

	/* replace the second edid ram data */
	/* physical address byte 1 */
	hdmirx_wr_top(TOP_EDID_RAM_OVR5,
		(phy_addr_offset + 0x101) | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR5_DATA,
		((pphy_addr[E_PORT0] >> 8) & 0xFF) |
		 (((pphy_addr[E_PORT1] >> 8) & 0xFF)<<8)
			| (((pphy_addr[E_PORT2] >> 8) & 0xFF)<<16)
			| (((pphy_addr[E_PORT3] >> 8) & 0xFF)<<24));
	/* physical address byte 0 */
	hdmirx_wr_top(TOP_EDID_RAM_OVR4,
		(phy_addr_offset + 0x100) | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR4_DATA,
		(pphy_addr[E_PORT0] & 0xFF) | ((pphy_addr[E_PORT1] & 0xFF)<<8) |
			((pphy_addr[E_PORT2] & 0xFF)<<16) |
			((pphy_addr[E_PORT3] & 0xFF) << 24));
	/* checksum */
	hdmirx_wr_top(TOP_EDID_RAM_OVR3,
		(0xff + 0x100) | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR3_DATA,
			pchecksum[E_PORT0]|(pchecksum[E_PORT1]<<8)|
			(pchecksum[E_PORT2]<<16) | (pchecksum[E_PORT3] << 24));

	//for (i = 0; i < E_PORT_NUM; i++) {
		//rx_pr(">port %d,addr 0x%x,checksum 0x%x\n",
					//i, pphy_addr[i], pchecksum[i]);
	//}
}

/* @func: seek dd+ atmos bit
 * @param:get audio type info by cec message:
 *	request short sudio descriptor
 */
unsigned char rx_parse_arc_aud_type(const unsigned char *buff)
{
	unsigned char tmp[7] = {0};
	unsigned char aud_length = 0;
	unsigned char i = 0;
	unsigned int aud_data = 0;
	int ret = 0;

	aud_length = strlen(buff);
	rx_pr("length = %d\n", aud_length);

	for (i = 0; i < aud_length; i += 6) {
		tmp[6] = '\0';
		memcpy(tmp, buff + i, 6);
		/* ret = sscanf(tmp, "%x", &aud_data); */
		ret = kstrtoint(tmp, 0, &aud_data);
		if (ret != 1)
			rx_pr("kstrtoint failed\n");
		if (aud_data >> 19 == AUDIO_FORMAT_DDP)
			break;
	}
	if ((i < aud_length) &&
		((aud_data & 0x1) == 0x1)) {
		if (!need_support_atmos_bit) {
			need_support_atmos_bit = true;
			hdmi_rx_top_edid_update();
			if (rx.open_fg && (rx.port != rx.arc_port)) {
				if (atmos_edid_update_hpd_en)
					rx_send_hpd_pulse();
				rx_pr("*update edid-atmos*\n");
			} else {
				pre_port = 0xff;
				rx_pr("update atmos later, in arc port:%s\n",
					rx.port == rx.arc_port ? "Y" : "N");
			}
		}
	} else {
		if (need_support_atmos_bit) {
			need_support_atmos_bit = false;
			hdmi_rx_top_edid_update();
			if (rx.open_fg && (rx.port != rx.arc_port)) {
				if (atmos_edid_update_hpd_en)
					rx_send_hpd_pulse();
				rx_pr("*update edid-no atmos*\n");
			} else {
				pre_port = 0xff;
				rx_pr("update atmos later, in arc port:%s\n",
					rx.port == rx.arc_port ? "Y" : "N");
			}
		}
	}

	return 0;
}

/*
 * audio parse the atmos info and inform hdmirx via a flag
 */
void rx_set_atmos_flag(bool en)
{
	if (need_support_atmos_bit != en) {
		need_support_atmos_bit = en;
		hdmi_rx_top_edid_update();
		if (rx.open_fg && (rx.port != rx.arc_port)) {
			if (atmos_edid_update_hpd_en)
				rx_send_hpd_pulse();
			rx_pr("*update edid-atmos*\n");
		} else {
			pre_port = 0xff;
			rx_pr("update atmos later, in arc port:%s\n",
				rx.port == rx.arc_port ? "Y" : "N");
		}
	}
}
EXPORT_SYMBOL(rx_set_atmos_flag);

bool rx_get_atmos_flag(void)
{
	return need_support_atmos_bit;
}
EXPORT_SYMBOL(rx_get_atmos_flag);

unsigned char get_atmos_offset(unsigned char *p_edid)
{
	unsigned char max_offset = 0;
	unsigned char tag_offset = 0;
	unsigned char tag_data = 0;
	unsigned char aud_length = 0;
	unsigned char i = 0;
	unsigned char ret = 0;

	max_offset = p_edid[130] + 128;
	tag_offset = 132;
	do {
		tag_data = p_edid[tag_offset];
		if ((tag_data & 0xE0) == 0x20) {
			if (log_level & EDID_LOG)
				rx_pr("audio_");
			aud_length = tag_data & 0x1F;
			break;
		}
		tag_offset += (tag_data & 0x1F) + 1;
	} while (tag_offset < max_offset);

	for (i = 0; i < aud_length; i += 3) {
		if (p_edid[tag_offset+1+i] >> 3 == 10) {
			ret = tag_offset+1+i+2;
			break;
		}
	}
	return ret;
}

unsigned char rx_edid_update_atmos(unsigned char *p_edid)
{
	unsigned char offset = get_atmos_offset(p_edid);

	if (offset == 0)
		rx_pr("can not find atmos info\n");
	else {
		if (need_support_atmos_bit)
			p_edid[offset] = 1;
		else
			p_edid[offset] = 0;
		if (log_level & EDID_LOG)
			rx_pr("offset = %d\n", offset);
	}
	return 0;
}

static enum edid_ver_e rx_get_edid_ver(uint8_t *p_edid)
{
	if (edid_tag_extract(p_edid, VENDOR_TAG + HF_VSDB_OFFSET))
		return EDID_V20;
	else
		return EDID_V14;
}

unsigned int hdmi_rx_top_edid_update(void)
{
	int edid_index = rx_get_edid_index();
	bool brepeat = true;
	u_char *pedid_data;
	u_int sts;
	u_int phy_addr_offset;
	u_int phy_addr[E_PORT_NUM] = {0, 0, 0, 0};
	u_char checksum[E_PORT_NUM] = {0, 0, 0, 0};

	if (edid_index >= EDID_LIST_NUM)
		return 0;
	/* get edid from buffer, return buffer addr */
	pedid_data = rx_get_edid(edid_index);
	rx.edid_ver = rx_get_edid_ver(pedid_data);

	/* update hdr info to edid buffer */
	rx_edid_update_hdr_info(pedid_data, edid_index);

	if (brepeat) {
		/* repeater mode */
		rx_edid_update_audio_info(pedid_data,
				rx_get_edid_size(edid_index));
	}
	edid_splice_earc_capds(pedid_data,
		recv_earc_cap_ds, earc_cap_ds_len);
	/* if (need_support_atmos_bit) */
	rx_edid_update_atmos(pedid_data);

	/* caculate physical address and checksum */
	sts = rx_edid_cal_phy_addr(brepeat,
					up_phy_addr, port_map,
					pedid_data, &phy_addr_offset,
					phy_addr);
	if (!sts) {
		/* not find physical address info */
		rx_pr("err: not finded phy addr info\n");
	}

	/* write edid to edid register */
	rx_edid_fill_to_register(pedid_data, brepeat,
							phy_addr, checksum);
	if (sts) {
		/* update physical and checksum */
		rx_edid_update_overlay(phy_addr_offset,
			phy_addr, checksum);
	}
	return 1;
}

void rx_edid_print_vic_fmt(unsigned char i,
	unsigned char hdmi_vic)
{
	/* CTA-861-G: Table-18 */
	/* SVD = 128, 254, 255 are reserved */
	if ((hdmi_vic >= 1) && (hdmi_vic <= 64)) {
		rx_pr("\tSVD#%2d: vic(%3d), format: %s\n",
			i+1, hdmi_vic, hdmi_fmt[hdmi_vic]);
	} else if ((hdmi_vic >= 65) && (hdmi_vic <= 107)) {
		/* from first new set */
		rx_pr("\tSVD#%2d: vic(%3d), format: %s\n",
			i+1, hdmi_vic, hdmi_fmt[hdmi_vic]);
	} else if ((hdmi_vic >= 108) && (hdmi_vic <= 127)) {
		/* from first new set: 8bit VIC */
	} else if ((hdmi_vic >= 129) && (hdmi_vic <= 192)) {
		hdmi_vic &= 0x7F;
		rx_pr("\tSVD#%2d: vic(%3d), native format: %s\n",
			i+1, hdmi_vic, hdmi_fmt[hdmi_vic]);
	} else if ((hdmi_vic >= 193) && (hdmi_vic <= 253)) {
		/* from second new set: 8bit VIC */
	}
}

/* edid header of base block
 * offset 0x00 ~ 0x07
 */
static bool check_edid_header_valid(unsigned char *buff)
{
	int i;
	bool ret = true;

	if (!(buff[0] | buff[7])) {
		for (i = 1; i < 7; i++) {
			if (buff[i] != 0xFF) {
				ret = false;
				break;
			}
		}
	} else {
		ret = false;
	}
	return ret;
}

/* manufacturer name
 * offset 0x8~0x9
 */
static void get_edid_manufacturer_name(unsigned char *buff,
	unsigned char start, struct edid_info_s *edid_info)
{
	int i;
	unsigned char uppercase[26] = { 0 };
	unsigned char brand[3];

	if (!edid_info || !buff)
		return;
	/* Fill array uppercase with 'A' to 'Z' */
	for (i = 0; i < 26; i++)
		uppercase[i] = 'A' + i;
	/* three 5-byte AscII code, such as 'A' = 00001, 'C' = 00011,*/
	brand[0] = buff[start] >> 2;
	brand[1] = ((buff[start] & 0x3) << 3)+(buff[start+1] >> 5);
	brand[2] = buff[start+1] & 0x1f;
	for (i = 0; i < 3; i++)
		edid_info->manufacturer_name[i] = uppercase[brand[i] - 1];
}

/* product code
 * offset 0xA~0xB
 */
static void get_edid_product_code(unsigned char *buff,
	unsigned char start, struct edid_info_s *edid_info)
{
	if (!edid_info || !buff)
		return;
	edid_info->product_code = buff[start+1] << 8 | buff[start];
}

/* serial number
 * offset 0xC~0xF
 */
static void get_edid_serial_number(unsigned char *buff,
	unsigned char start, struct edid_info_s *edid_info)
{
	if (!edid_info || !buff)
		return;
	edid_info->serial_number = (buff[start+3] << 24) |
		(buff[start+2] << 16) |
		(buff[start+1] << 8) |
		buff[start];
}

/* manufacture date
 * offset 0x10~0x11
 */
static void get_edid_manufacture_date(unsigned char *buff,
	unsigned char start, struct edid_info_s *edid_info)
{
	if (!edid_info || !buff)
		return;
	/* week of manufacture:
	 * 0: not specified
	 * 0x1~0x35: valid week
	 * 0x36~0xfe: reserved
	 * 0xff: model year is specified
	 */
	if ((buff[start] == 0) ||
		((buff[start] >= 0x36) && (buff[start] <= 0xfe)))
		edid_info->product_week = 0;
	else
		edid_info->product_week = buff[start];
	/* year of manufacture,
	 * or model year (if specified by week=0xff)
	 */
	edid_info->product_year = buff[start+1] + 1990;
}

/* The EDID version in base block
 * offset 0x12 and 0x13
 *static void get_edid_version(unsigned char *buff,
 *	unsigned char start, struct edid_info_s *edid_info)
 *{
 *	if (!edid_info || !buff)
 *		return;
 *	edid_info->edid_version = buff[start];
 *	edid_info->edid_revision = buff[start+1];
 *}
 */

/* Basic Display Parameters and Features
 * offset 0x14~0x18
 */
static void get_edid_display_parameters(unsigned char *buff,
	unsigned char start, struct edid_info_s *edid_info)
{}

/* Color Characteristics. Color Characteristics provides information about
 * the display device's chromaticity and color temperature parameters
 * (white temperature in degrees Kelvin)
 * offset 0x19~0x22
 */
static void get_edid_color_characteristics(unsigned char *buff,
	unsigned char start, struct edid_info_s *edid_info)
{}

/* established timings are computer display timings recognized by VESA.
 * offset 0x23~0x25
 */
static void get_edid_established_timings(unsigned char *buff,
	unsigned char start, struct edid_info_s *edid_info)
{
	if (!edid_info || !buff)
		return;
	rx_pr("established timing:\n");
	/* each bit for an established timing */
	if (buff[start] & (1 << 5))
		rx_pr("\t640*480p60hz\n");
	if (buff[start] & (1 << 0))
		rx_pr("\t800*600p60hz\n");
	if (buff[start+1] & (1 << 3))
		rx_pr("\t1024*768p60hz\n");
}

/* Standard timings are those either recognized by VESA
 * through the VESA Discrete Monitor Timing or Generalized
 * Timing Formula standards. Each timing is two bytes.
 * offset 0x26~0x35
 */
static void get_edid_standard_timing(unsigned char *buff, unsigned char start,
	unsigned int length, struct edid_info_s *edid_info)
{
	unsigned char  i, img_aspect_ratio;
	int hactive_pixel, vactive_pixel, refresh_rate;
	int asp_ratio[] = {
		80*10/16,
		80*3/4,
		80*4/5,
		80*9/16,
	};/*multiple 80 first*/
	if (!edid_info || !buff)
		return;
	rx_pr("standard timing:\n");
	for (i = 0; i < length; i = i+2) {
		if ((buff[start+i] != 0x01) && (buff[start+i+1] != 0x01)) {
			hactive_pixel = (int)((buff[start+i] + 31)*8);
			/* image aspect ratio:
			 * 0 -> 16:10
			 * 1 -> 4:3
			 * 2 -> 5:4
			 * 3 -> 16:9
			 */
			img_aspect_ratio = (buff[start+i+1] >> 6) & 0x3;
			vactive_pixel =
				hactive_pixel*asp_ratio[img_aspect_ratio]/80;
			refresh_rate = (int)(buff[start+i+1] & 0x3F) + 60;
			rx_pr("\t%d*%dP%dHz\n", hactive_pixel,
				vactive_pixel, refresh_rate);
		}
	}
}

static void get_edid_monitor_name(unsigned char *p_edid,
	unsigned char start, struct edid_info_s *edid_info)
{
	unsigned char i, j;

	if (!p_edid || !edid_info)
		return;
	/* CEA861-F Table83 */
	for (i = 0; i < 4; i++) {
		/* 0xFC denotes that last 13 bytes of this
		 * descriptor block contain Monitor name
		 */
		if (p_edid[start+i*18+3] == 0xFC)
			break;
	}
	/* if name < 13 bytes, terminate name with 0x0A
	 * and fill remainder of 13 bytes with 0x20
	 */
	for (j = 0; j < 13; j++) {
		if (p_edid[start+i*18+5+j] == 0x0A)
			break;
		edid_info->monitor_name[j] = p_edid[0x36+i*18+5+j];
	}

}

static void get_edid_range_limits(unsigned char *p_edid,
	unsigned char start, struct edid_info_s *edid_info)
{
	unsigned char i;

	for (i = 0; i < 4; i++) {
		/* 0xFD denotes that last 13 bytes of this
		 * descriptor block contain monitor range limits
		 */
		if (p_edid[start+i*18+3] == 0xFD)
			break;
	}
	/*maxmium supported pixel clock*/
	edid_info->max_sup_pixel_clk = p_edid[0x36+i*18+9]*10;
}

/* edid parse
 * Descriptor data
 * 0xff monitor S/N
 * 0xfe ASCII data string
 * 0xfd monitor range limits
 * 0xfc monitor name
 * 0xfb color point
 * 0xfa standard timing ID
 */
void edid_parse_block0(uint8_t *p_edid, struct edid_info_s *edid_info)
{
	bool edid_header_valid;

	if (!p_edid || !edid_info)
		return;
	edid_header_valid = check_edid_header_valid(p_edid);
	if (!edid_header_valid) {
		rx_pr("edid block0 header invalid!\n");
		return;
	}
	/* manufacturer name offset 8~9 */
	get_edid_manufacturer_name(p_edid, 8, edid_info);
	/* product code offset 10~11 */
	get_edid_product_code(p_edid, 10, edid_info);
	/* serial number offset 12~15 */
	get_edid_serial_number(p_edid, 12, edid_info);

	/* product date offset 0x10~0x11 */
	get_edid_manufacture_date(p_edid, 0x10, edid_info);
	/* EDID version offset 0x12~0x13*/
	/* get_edid_version(p_edid, 0x12, edid_info); */
	/* Basic Display Parameters and Features offset 0x14~0x18 */
	get_edid_display_parameters(p_edid, 0x14, edid_info);
	/* Color Characteristics offset 0x19~0x22 */
	get_edid_color_characteristics(p_edid, 0x19, edid_info);
	/* established timing offset 0x23~0x25 */
	get_edid_established_timings(p_edid, 0x23, edid_info);
	/* standard timing offset 0x26~0x35*/
	get_edid_standard_timing(p_edid, 0x26, 16, edid_info);
	/* best resolution: hactive*vactive*/
	edid_info->descriptor1.hactive =
		p_edid[0x38] + (((p_edid[0x3A] >> 4) & 0xF) << 8);
	edid_info->descriptor1.vactive =
		p_edid[0x3B] + (((p_edid[0x3D] >> 4) & 0xF) << 8);
	edid_info->descriptor1.pixel_clk =
		(p_edid[0x37] << 8) + p_edid[0x36];
	/* monitor name */
	get_edid_monitor_name(p_edid, 0x36, edid_info);
	get_edid_range_limits(p_edid, 0x36, edid_info);
	edid_info->extension_flag = p_edid[0x7e];
	edid_info->block0_chk_sum = p_edid[0x7f];
}

static void get_edid_video_data(unsigned char *buff, unsigned char start,
	unsigned char len, struct cta_blk_parse_info *edid_info)
{
	unsigned char i;

	if (!buff || !edid_info)
		return;
	edid_info->video_db.svd_num = len;
	for (i = 0; i < len; i++) {
		edid_info->video_db.hdmi_vic[i] =
			buff[i+start];
	}
}

static void get_edid_audio_data(unsigned char *buff, unsigned char start,
	unsigned char len, struct cta_blk_parse_info *edid_info)
{
	enum edid_audio_format_e fmt;
	int i = start;
	struct pcm_t *pcm;

	if (!buff || !edid_info)
		return;
	do {
		fmt = (buff[i] & 0x78) >> 3;/* bit6~3 */
		edid_info->audio_db.aud_fmt_sup[fmt] = 1;
		/* CEA-861F page 82: audio data block*/
		switch (fmt) {
		case AUDIO_FORMAT_LPCM:
			pcm = &(edid_info->audio_db.sad[fmt].bit_rate.pcm);
			edid_info->audio_db.sad[fmt].max_channel =
				(buff[i] & 0x7);
			if (buff[i+1] & 0x40)
				edid_info->audio_db.sad[fmt].freq_192khz = 1;
			if (buff[i+1] & 0x20)
				edid_info->audio_db.sad[fmt].freq_176_4khz = 1;
			if (buff[i+1] & 0x10)
				edid_info->audio_db.sad[fmt].freq_96khz = 1;
			if (buff[i+1] & 0x08)
				edid_info->audio_db.sad[fmt].freq_88_2khz = 1;
			if (buff[i+1] & 0x04)
				edid_info->audio_db.sad[fmt].freq_48khz = 1;
			if (buff[i+1] & 0x02)
				edid_info->audio_db.sad[fmt].freq_44_1khz = 1;
			if (buff[i+1] & 0x01)
				edid_info->audio_db.sad[fmt].freq_32khz = 1;
			if (buff[i+2] & 0x04)
				pcm->size_24bit = 1;
			if (buff[i+2] & 0x02)
				pcm->size_20bit = 1;
			if (buff[i+2] & 0x01)
				pcm->size_16bit = 1;
			break;
		/* CEA861F table50 fmt2~8 byte 3:
		 * Maximum bit rate divided by 8 kHz
		 */
		case AUDIO_FORMAT_AC3:
		case AUDIO_FORMAT_MPEG1:
		case AUDIO_FORMAT_MP3:
		case AUDIO_FORMAT_MPEG2:
		case AUDIO_FORMAT_AAC:
		case AUDIO_FORMAT_DTS:
		case AUDIO_FORMAT_ATRAC:
			edid_info->audio_db.sad[fmt].max_channel =
				(buff[i] & 0x07);
			if (buff[i+1] & 0x40)
				edid_info->audio_db.sad[fmt].freq_192khz = 1;
			if (buff[i+1] & 0x20)
				edid_info->audio_db.sad[fmt].freq_176_4khz = 1;
			if (buff[i+1] & 0x10)
				edid_info->audio_db.sad[fmt].freq_96khz = 1;
			if (buff[i+1] & 0x08)
				edid_info->audio_db.sad[fmt].freq_88_2khz = 1;
			if (buff[i+1] & 0x04)
				edid_info->audio_db.sad[fmt].freq_48khz = 1;
			if (buff[i+1] & 0x02)
				edid_info->audio_db.sad[fmt].freq_44_1khz = 1;
			if (buff[i+1] & 0x01)
				edid_info->audio_db.sad[fmt].freq_32khz = 1;
			edid_info->audio_db.sad[fmt].bit_rate.others =
				buff[i+2];
			break;
		/* for audio format code 9~13:
		 * byte3 is dependent on Audio Format Code
		 */
		case AUDIO_FORMAT_OBA:
		case AUDIO_FORMAT_DDP:
		case AUDIO_FORMAT_DTSHD:
		case AUDIO_FORMAT_MAT:
		case AUDIO_FORMAT_DST:
			edid_info->audio_db.sad[fmt].max_channel =
				(buff[i] & 0x07);
			if (buff[i+1] & 0x40)
				edid_info->audio_db.sad[fmt].freq_192khz = 1;
			if (buff[i+1] & 0x20)
				edid_info->audio_db.sad[fmt].freq_176_4khz = 1;
			if (buff[i+1] & 0x10)
				edid_info->audio_db.sad[fmt].freq_96khz = 1;
			if (buff[i+1] & 0x08)
				edid_info->audio_db.sad[fmt].freq_88_2khz = 1;
			if (buff[i+1] & 0x04)
				edid_info->audio_db.sad[fmt].freq_48khz = 1;
			if (buff[i+1] & 0x02)
				edid_info->audio_db.sad[fmt].freq_44_1khz = 1;
			if (buff[i+1] & 0x01)
				edid_info->audio_db.sad[fmt].freq_32khz = 1;
			edid_info->audio_db.sad[fmt].bit_rate.others
				= buff[i+2];
			break;
		/* audio format code 14:
		 * last 3 bits of byte3: profile
		 */
		case AUDIO_FORMAT_WMAPRO:
			edid_info->audio_db.sad[fmt].max_channel =
				(buff[i] & 0x07);
			if (buff[i+1] & 0x40)
				edid_info->audio_db.sad[fmt].freq_192khz = 1;
			if (buff[i+1] & 0x20)
				edid_info->audio_db.sad[fmt].freq_176_4khz = 1;
			if (buff[i+1] & 0x10)
				edid_info->audio_db.sad[fmt].freq_96khz = 1;
			if (buff[i+1] & 0x08)
				edid_info->audio_db.sad[fmt].freq_88_2khz = 1;
			if (buff[i+1] & 0x04)
				edid_info->audio_db.sad[fmt].freq_48khz = 1;
			if (buff[i+1] & 0x02)
				edid_info->audio_db.sad[fmt].freq_44_1khz = 1;
			if (buff[i+1] & 0x01)
				edid_info->audio_db.sad[fmt].freq_32khz = 1;
			edid_info->audio_db.sad[fmt].bit_rate.others
				= buff[i+2];
			break;
		/* case 15: audio coding extension coding type */
		default:
			break;
		}
		i += 3; /*next audio fmt*/
	} while (i < (start + len));
}

static void get_edid_speaker_data(unsigned char *buff, unsigned char start,
	unsigned char len, struct cta_blk_parse_info *edid_info)
{
	int i;

	if (!buff || !edid_info)
		return;
	/* speaker allocation is 3 bytes long*/
	if (len != 3) {
		rx_pr("invalid length for speaker allocation data block: %d\n",
			len);
		return;
	}
	for (i = 1; i <= 0x80; i = i << 1) {
		switch (buff[start] & i) {
		case 0x80:
			edid_info->speaker_alloc.flw_frw = 1;
			break;
		case 0x40:
			edid_info->speaker_alloc.rlc_rrc = 1;
			break;
		case 0x20:
			edid_info->speaker_alloc.flc_frc = 1;
			break;
		case 0x10:
			edid_info->speaker_alloc.rc = 1;
			break;
		case 0x08:
			edid_info->speaker_alloc.rl_rr = 1;
			break;
		case 0x04:
			edid_info->speaker_alloc.fc = 1;
			break;
		case 0x02:
			edid_info->speaker_alloc.lfe = 1;
			break;
		case 0x01:
			edid_info->speaker_alloc.fl_fr = 1;
			break;
		default:
			break;
		}
	}
	for (i = 1; i <= 0x4; i = i << 1) {
		switch (buff[start+1] & i) {
		case 0x4:
			edid_info->speaker_alloc.fch = 1;
			break;
		case 0x2:
			edid_info->speaker_alloc.tc = 1;
			break;
		case 0x1:
			edid_info->speaker_alloc.flh_frh = 1;
			break;
		default:
			break;
		}
	}
}

/* if is valid vsdb, then return 1
 * if is valid hf-vsdb, then return 2
 * if is invalid db, then return 0
 */
static int get_edid_vsdb(unsigned char *buff, unsigned char start,
	unsigned char len, struct cta_blk_parse_info *edid_info)
{
	unsigned char _3d_present_offset;
	unsigned char hdmi_vic_len;
	unsigned char hdmi_vic_offset;
	unsigned char i, j;
	unsigned int ieee_oui;
	unsigned char _3d_struct_all_offset;
	unsigned char hdmi_3d_len;
	unsigned char _3d_struct;
	unsigned char _2d_vic_order_offset;
	unsigned char temp_3d_len;
	int ret = 0;

	if (!buff || !edid_info)
		return 0;
	/* basic 5 bytes; others: extension fields */
	if (len < 5) {
		rx_pr("invalid VSDB length: %d!\n", len);
		return ret;
	}
	ieee_oui = (buff[start+2] << 16) |
		(buff[start+1] << 8) |
		buff[start];
	if ((ieee_oui != 0x000C03) &&
		(ieee_oui != 0xC45DD8)) {
		rx_pr("invalid IEEE OUI\n");
		return ret;
	} else if (ieee_oui == 0xC45DD8) {
		ret = 2;
		goto hf_vsdb;
	}
	ret = 1;
	edid_info->vsdb.ieee_oui = ieee_oui;
	/* phy addr: 2 bytes*/
	edid_info->vsdb.a = (buff[start+3] >> 4) & 0xf;
	edid_info->vsdb.b = buff[start+3] & 0xf;
	edid_info->vsdb.c = (buff[start+4] >> 4) & 0xf;
	edid_info->vsdb.d = buff[start+4] & 0xf;
	/* after first 5 bytes: vsdb1.4 extension fileds */
	if (len > 5) {
		edid_info->vsdb.support_AI = (buff[start+5] >> 7) & 0x1;
		edid_info->vsdb.DC_48bit = (buff[start+5] >> 6) & 0x1;
		edid_info->vsdb.DC_36bit = (buff[start+5] >> 5) & 0x1;
		edid_info->vsdb.DC_30bit = (buff[start+5] >> 4) & 0x1;
		edid_info->vsdb.DC_y444 = (buff[start+5] >> 3) & 0x1;
		edid_info->vsdb.dvi_dual = buff[start+5] & 0x1;
	}
	if (len > 6)
		edid_info->vsdb.max_tmds_clk = buff[start+6];
	if (len > 7) {
		edid_info->vsdb.latency_fields_present =
			(buff[start+7] >> 7) & 0x1;
		edid_info->vsdb.i_latency_fields_present =
			(buff[start+7] >> 6) & 0x1;
		edid_info->vsdb.hdmi_video_present =
			(buff[start+7] >> 5) & 0x1;
		edid_info->vsdb.cnc3 = (buff[start+7] >> 3) & 0x1;
		edid_info->vsdb.cnc2 = (buff[start+7] >> 2) & 0x1;
		edid_info->vsdb.cnc1 = (buff[start+7] >> 1) & 0x1;
		edid_info->vsdb.cnc0 = buff[start+7] & 0x1;
	}
	if (edid_info->vsdb.latency_fields_present) {
		if (len < 10) {
			rx_pr("invalid vsdb len for latency: %d\n", len);
			return 0;
		}
		edid_info->vsdb.video_latency = buff[start+8];
		edid_info->vsdb.audio_latency = buff[start+9];
		_3d_present_offset = 10;
	} else {
		rx_pr("latency fields not present\n");
		_3d_present_offset = 8;
	}
	if (edid_info->vsdb.i_latency_fields_present) {
		/* I_Latency_Fields_Present shall be zero
		 * if Latency_Fields_Present is zero.
		 */
		if (edid_info->vsdb.latency_fields_present)
			rx_pr("i_latency_fields should not be set\n");
		else if (len < 12) {
			rx_pr("invalid vsdb len for i_latency: %d\n", len);
			return 0;
		}
		edid_info->vsdb.interlaced_video_latency
			= buff[start+10];
		edid_info->vsdb.interlaced_audio_latency
			= buff[start+11];
		_3d_present_offset = 12;
	}
	/* HDMI_Video_present: If set then additional video format capabilities
	 * are described by using the fields starting after the Latency
	 * area. This consists of 4 parts with the order described below:
	 * 1 byte containing the 3D_present flag and other flags
	 * 1 byte with length fields HDMI_VIC_LEN and HDMI_3D_LEN
	 * zero or more bytes for information about HDMI_VIC formats supported
	 * (length of this field is indicated by HDMI_VIC_LEN).
	 * zero or more bytes for information about 3D formats supported
	 * (length of this field is indicated by HDMI_3D_LEN)
	 */
	if (edid_info->vsdb.hdmi_video_present) {
		/* if hdmi video present,
		 * 2 additonal bytes at least will present
		 * 1 byte containing the 3D_present flag and other flags
		 * 1 byte with length fields HDMI_VIC_LEN and HDMI_3D_LEN
		 * 0 or more bytes for info about HDMI_VIC formats supported
		 * 0 or more bytes for info about 3D formats supported
		 */
		if (len < _3d_present_offset + 2) {
			rx_pr("invalid vsdb length for hdmi video: %d\n", len);
			return 0;
		}
		edid_info->vsdb._3d_present =
			(buff[start+_3d_present_offset] >> 7) & 0x1;
		edid_info->vsdb._3d_multi_present =
			(buff[start+_3d_present_offset] >> 5) & 0x3;
		edid_info->vsdb.image_size =
			(buff[start+_3d_present_offset] >> 3) & 0x3;
		edid_info->vsdb.hdmi_vic_len =
			(buff[start+_3d_present_offset+1] >> 5) & 0x7;
		edid_info->vsdb.hdmi_3d_len =
			(buff[start+_3d_present_offset+1]) & 0x1f;
		/* parse 4k2k video format, 4 4k2k format maximum*/
		hdmi_vic_offset = _3d_present_offset + 2;
		hdmi_vic_len = edid_info->vsdb.hdmi_vic_len;
		if (hdmi_vic_len > 4) {
			rx_pr("invalid hdmi vic len: %d\n",
				edid_info->vsdb.hdmi_vic_len);
			return 0;
		}

		/* HDMI_VIC_LEN may be 0 */
		if (len < hdmi_vic_offset + hdmi_vic_len) {
			rx_pr("invalid length for 4k2k: %d\n", len);
			return 0;
		}
		for (i = 0; i < hdmi_vic_len; i++) {
			if (buff[start+hdmi_vic_offset+i] == 1)
				edid_info->vsdb.hdmi_4k2k_30hz_sup = 1;
			else if (buff[start+hdmi_vic_offset+i] == 2)
				edid_info->vsdb.hdmi_4k2k_25hz_sup = 1;
			else if (buff[start+hdmi_vic_offset+i] == 3)
				edid_info->vsdb.hdmi_4k2k_24hz_sup = 1;
			else if (buff[start+hdmi_vic_offset+i] == 4)
				edid_info->vsdb.hdmi_smpte_sup = 1;
		}

		/* 3D info parse */
		_3d_struct_all_offset =
			hdmi_vic_offset + hdmi_vic_len;
		hdmi_3d_len = edid_info->vsdb.hdmi_3d_len;
		/* there may be additional 0 present after 3D info  */
		if (len < _3d_struct_all_offset + hdmi_3d_len) {
			rx_pr("invalid vsdb length for 3d: %d\n", len);
			return 0;
		}
		/* 3d_multi_present: hdmi1.4 spec page155:
		 * 0: neither structure or mask present,
		 * 1: only 3D_Structure_ALL_15бн0 is present
		 *    and assigns 3D formats to all of the
		 *    VICs listed in the first 16 entries
		 *    in the EDID
		 * 2: 3D_Structure_ALL_15бн0 and 3D_MASK_15бн0
		 *    are present and assign 3D formats to
		 *    some of the VICs listed in the first
		 *    16 entries in the EDID.
		 * 3: neither structure or mask present,
		 *    Reserved for future use.
		 */
		if (edid_info->vsdb._3d_multi_present == 1) {
			edid_info->vsdb._3d_struct_all =
				(buff[start+_3d_struct_all_offset] << 8) +
				buff[start+_3d_struct_all_offset+1];
			_2d_vic_order_offset =
				_3d_struct_all_offset + 2;
			temp_3d_len = 2;
		} else if (edid_info->vsdb._3d_multi_present == 2) {
			edid_info->vsdb._3d_struct_all =
				(buff[start+_3d_struct_all_offset] << 8) +
				buff[start+_3d_struct_all_offset+1];
			edid_info->vsdb._3d_mask_15_0 =
				(buff[start+_3d_struct_all_offset+2] << 8) +
				buff[start+_3d_struct_all_offset+3];
			_2d_vic_order_offset =
				_3d_struct_all_offset + 4;
			temp_3d_len = 4;
		} else {
			_2d_vic_order_offset =
				_3d_struct_all_offset;
			temp_3d_len = 0;
		}
		i = _2d_vic_order_offset;
		for (j = 0; (temp_3d_len < hdmi_3d_len)
			&& (j < 16); j++) {
			edid_info->vsdb._2d_vic[j]._2d_vic_order =
				(buff[start+i] >> 4) & 0xF;
			edid_info->vsdb._2d_vic[j]._3d_struct =
				buff[start+i] & 0xF;
			_3d_struct =
				edid_info->vsdb._2d_vic[j]._3d_struct;
			/* hdmi1.4 spec page156
			 * if 3D_Structure_X is 0000~0111,
			 * 3D_Detail_X shall not be present,
			 * otherwise shall be present
			 */
			if ((_3d_struct	>= 0x8) &&
				(_3d_struct <= 0xF)) {
				edid_info->vsdb._2d_vic[j]._3d_detail =
					(buff[start+i+1] >> 4) & 0xF;
				i += 2;
				temp_3d_len += 2;
				if (temp_3d_len > hdmi_3d_len) {
					rx_pr("invalid len for 3d_detail: %d\n",
						len);
					ret = 0;
					break;
				}
			} else {
				i++;
				temp_3d_len++;
			}
		}
		edid_info->vsdb._2d_vic_num = j;
		return ret;
	}
	return ret;
hf_vsdb:
	/* hdmi spec2.0 Table10-6 */
	if (len < 7) {
		rx_pr("invalid HF_VSDB length: %d!\n", len);
		return 0;
	}
	edid_info->contain_hf_vsdb = true;
	edid_info->hf_vsdb.ieee_oui = ieee_oui;
	edid_info->hf_vsdb.version =
		buff[start+3];
	edid_info->hf_vsdb.max_tmds_rate =
		buff[start+4];
	edid_info->hf_vsdb.scdc_present =
		(buff[start+5] >> 7) & 0x1;
	edid_info->hf_vsdb.rr_cap =
		(buff[start+5] >> 6) & 0x1;
	edid_info->hf_vsdb.lte_340m_scramble =
		(buff[start+5] >> 3) & 0x1;
	edid_info->hf_vsdb.independ_view =
		(buff[start+5] >> 2) & 0x1;
	edid_info->hf_vsdb.dual_view =
		(buff[start+5] >> 1) & 0x1;
	edid_info->hf_vsdb._3d_osd_disparity =
		buff[start+5] & 0x1;

	edid_info->hf_vsdb.dc_48bit_420 =
		(buff[start+6] >> 2) & 0x1;
	edid_info->hf_vsdb.dc_36bit_420 =
		(buff[start+6] >> 1) & 0x1;
	edid_info->hf_vsdb.dc_30bit_420 =
		buff[start+6] & 0x1;
	return ret;
}

static void get_edid_vcdb(unsigned char *buff, unsigned char start,
	 unsigned char len, struct cta_blk_parse_info *edid_info)
{
	if (!buff || !edid_info)
		return;
	/* vcdb only contain 3 bytes data block. the source should
	 * ignore additional bytes (when present) and continue to
	 * parse the single byte as defined in CEA861-F Table 59.
	 */
	if (len != 2-1) {
		rx_pr("invalid length for video cap data blcok: %d!\n", len);
		/* return; */
	}
	edid_info->contain_vcdb = true;
	edid_info->vcdb.quanti_range_ycc = (buff[start] >> 7) & 0x1;
	edid_info->vcdb.quanti_range_rgb = (buff[start] >> 6) & 0x1;
	edid_info->vcdb.s_PT = (buff[start] >> 4) & 0x3;
	edid_info->vcdb.s_IT = (buff[start] >> 2) & 0x3;
	edid_info->vcdb.s_CE = buff[start] & 0x3;
}

static void get_edid_dv_data(unsigned char *buff, unsigned char start,
	 unsigned char len, struct cta_blk_parse_info *edid_info)
{
	unsigned int ieee_oui;

	if (!buff || !edid_info)
		return;
	if ((len != 0xE - 1) &&
		(len != 0x19 - 1)) {
		rx_pr("invalid length for dolby vision vsvdb:%d\n",
			len);
		return;
	}
	ieee_oui = (buff[start+2] << 16) |
		(buff[start+1] << 8) |
		buff[start];
	if (ieee_oui != 0x00D046) {
		rx_pr("invalid dolby vision ieee oui\n");
		return;
	}
	edid_info->contain_vsvdb = true;
	edid_info->dv_vsvdb.ieee_oui = ieee_oui;
	edid_info->dv_vsvdb.version = buff[start+3] >> 5;
	if (edid_info->dv_vsvdb.version == 0x0) {
		/* length except extend code */
		if (len != 0x19 - 1) {
			rx_pr("invalid length for dolby vision ver0\n");
			return;
		}
		edid_info->dv_vsvdb.sup_global_dimming =
			(buff[start+3] >> 2) & 0x1;
		edid_info->dv_vsvdb.sup_2160p60hz =
			(buff[start+3] >> 1) & 0x1;
		edid_info->dv_vsvdb.sup_yuv422_12bit =
			buff[start+3] & 0x1;
		edid_info->dv_vsvdb.Rx =
			((buff[start+4] >> 4) & 0xF) + (buff[start+5] << 4);
		edid_info->dv_vsvdb.Ry =
			(buff[start+4] & 0xF) + (buff[start+6] << 4);
		edid_info->dv_vsvdb.Gx =
			((buff[start+7] >> 4) & 0xF) + (buff[start+8] << 4);
		edid_info->dv_vsvdb.Gy =
			(buff[start+7] & 0xF) + (buff[start+9] << 4);
		edid_info->dv_vsvdb.Bx =
			((buff[start+10] >> 4) & 0xF) + (buff[start+11] << 4);
		edid_info->dv_vsvdb.By =
			(buff[start+10] & 0xF) + (buff[start+12] << 4);
		edid_info->dv_vsvdb.Wx =
			((buff[start+13] >> 4) & 0xF) + (buff[start+14] << 4);
		edid_info->dv_vsvdb.Wy =
			(buff[start+13] & 0xF) + (buff[start+15] << 4);
		edid_info->dv_vsvdb.tminPQ =
			((buff[start+16] >> 4) & 0xF) + (buff[start+17] << 4);
		edid_info->dv_vsvdb.tmaxPQ =
			(buff[start+16] & 0xF) + (buff[start+18] << 4);
		edid_info->dv_vsvdb.dm_major_ver =
			(buff[start+19] >> 4) & 0xF;
		edid_info->dv_vsvdb.dm_minor_ver =
			buff[start+19] & 0xF;
	} else if (edid_info->dv_vsvdb.version == 0x1) {
		/*length except extend code*/
		if (len != 0xE - 1) {
			rx_pr("invalid length for dolby vision ver1\n");
			return;
		}
		edid_info->dv_vsvdb.DM_version =
			(buff[start+3] >> 2) & 0x7;
		edid_info->dv_vsvdb.sup_2160p60hz =
			(buff[start+3] >> 1) & 0x1;
		edid_info->dv_vsvdb.sup_yuv422_12bit =
			buff[start+3] & 1;
		edid_info->dv_vsvdb.target_max_lum =
			(buff[start+4] >> 1) & 0x7F;
		edid_info->dv_vsvdb.sup_global_dimming =
			buff[start+4] & 0x1;
		edid_info->dv_vsvdb.target_min_lum =
			(buff[start+5] >> 1) & 0x7F;
		edid_info->dv_vsvdb.colormetry =
			buff[start+5] & 0x1;
		edid_info->dv_vsvdb.Rx = buff[start+7];
		edid_info->dv_vsvdb.Ry = buff[start+8];
		edid_info->dv_vsvdb.Gx = buff[start+9];
		edid_info->dv_vsvdb.Gy = buff[start+10];
		edid_info->dv_vsvdb.Bx = buff[start+11];
		edid_info->dv_vsvdb.By = buff[start+12];
	}
}

static void get_edid_colorimetry_data(unsigned char *buff, unsigned char start,
	unsigned char len, struct cta_blk_parse_info *edid_info)
{
	if (!buff || !edid_info)
		return;
	/* colorimetry DB only contain 3 bytes data block */
	if (len != 3-1) {
		rx_pr("invalid length for colorimetry data block:%d\n",
			len);
		return;
	}
	edid_info->contain_cdb = true;
	edid_info->color_db.BT2020_RGB = (buff[start] >> 7) & 0x1;
	edid_info->color_db.BT2020_YCC = (buff[start] >> 6) & 0x1;
	edid_info->color_db.BT2020_cYCC = (buff[start] >> 5) & 0x1;
	edid_info->color_db.Adobe_RGB = (buff[start] >> 4) & 0x1;
	edid_info->color_db.Adobe_YCC601 = (buff[start] >> 3) & 0x1;
	edid_info->color_db.sYCC601 = (buff[start] >> 2) & 0x1;
	edid_info->color_db.xvYCC709 = (buff[start] >> 1) & 0x1;
	edid_info->color_db.xvYCC601 = buff[start] & 0x1;

	edid_info->color_db.MD3 = (buff[start+1] >> 3) & 0x1;
	edid_info->color_db.MD2 = (buff[start+1] >> 2) & 0x1;
	edid_info->color_db.MD1 = (buff[start+1] >> 1) & 0x1;
	edid_info->color_db.MD0 = buff[start+1] & 0x1;
}

static void get_hdr_data(unsigned char *buff, unsigned char start,
	unsigned char len, struct cta_blk_parse_info *edid_info)
{
	if (!buff || !edid_info)
		return;
	/* Bytes 5-7 are optional to declare. 3 bytes payload at least */
	if (len < 3-1) {
		rx_pr("invalid hdr length: %d!\n", len);
		return;
	}
	edid_info->contain_hdr_db = true;
	edid_info->hdr_db.eotf_hlg = (buff[start] >> 3) & 0x1;
	edid_info->hdr_db.eotf_smpte_st_2084 = (buff[start] >> 2) & 0x1;
	edid_info->hdr_db.eotf_hdr = (buff[start] >> 1) & 0x1;
	edid_info->hdr_db.eotf_sdr = buff[start] & 0x1;
	edid_info->hdr_db.hdr_SMD_type1 =  buff[start+1] & 0x1;
	if (len > 2)
		edid_info->hdr_db.hdr_lum_max = buff[start+2];
	if (len > 3)
		edid_info->hdr_db.hdr_lum_avg = buff[start+3];
	if (len > 4)
		edid_info->hdr_db.hdr_lum_min = buff[start+4];
}

static void get_edid_y420_vid_data(unsigned char *buff, unsigned char start,
	unsigned char len, struct cta_blk_parse_info *edid_info)
{
	int i;

	if (!buff || !edid_info)
		return;
	if (len > 6) {
		rx_pr("y420vdb support only 4K50/60hz, smpte50/60hz, len:%d\n",
			len);
		return;
	}
	edid_info->contain_y420_vdb = true;
	edid_info->y420_vic_len = len;
	for (i = 0; i < len; i++)
		edid_info->y420_vdb_vic[i] = buff[start+i];
}

static void get_edid_y420_cap_map_data(unsigned char *buff, unsigned char start,
	unsigned char len, struct cta_blk_parse_info *edid_info)
{
	unsigned int i, bit_map = 0;

	if (!buff || !edid_info)
		return;
	/* 31 SVD maxmium, 4 bytes needed */
	if (len > 4) {
		rx_pr("31 SVD maxmium, all-zero data not needed\n");
		len = 4;
	}
	edid_info->contain_y420_cmdb = true;
	/* When the Length field is set to L==1, the Y420CMDB does not
	 * include a YCBCR 4:2:0 Capability Bit Map and all the SVDs in
	 * the regular Video Data Block support YCBCR 4:2:0 samplin mode.
	 */
	if (len == 0) {
		edid_info->y420_all_vic = 1;
		return;
	}
	/* bit0: first SVD, bit 1:the second SVD, and so on */
	for (i = 0; i < len; i++)
		bit_map |= (buff[start+i] << (i*8));
	/* '1' bit in the bit map indicate corresponding SVD support Y420 */
	for (i = 0; (i < len*8) && (i < 31); i++) {
		if ((bit_map >> i) & 0x1) {
			edid_info->y420_cmdb_vic[i] =
				edid_info->video_db.hdmi_vic[i];
		}
	}
}

static void get_edid_vsadb(unsigned char *buff, unsigned char start,
	unsigned char len, struct cta_blk_parse_info *edid_info)
{
	int i;

	if (!buff || !edid_info)
		return;
	if (len > 27-2) {
		/* CTA-861-G 7.5.8 */
		rx_pr("over VSADB max size(27 bytes), len:%d\n", len);
		return;
	} else if (len < 3) {
		rx_pr("no valid IEEE OUI, len:%d\n", len);
		return;
	}
	edid_info->contain_vsadb = true;
	edid_info->vsadb_ieee =
		buff[start] |
		(buff[start+1] << 8) |
		(buff[start+2] << 16);
	for (i = 0; i < len-3; i++)
		edid_info->vsadb_payload[i] = buff[start+3+i];
}

static void get_edid_rcdb(unsigned char *buff, unsigned char start,
	unsigned char len, struct cta_blk_parse_info *edid_info)
{
}

static void get_edid_sldb(unsigned char *buff, unsigned char start,
	unsigned char len, struct cta_blk_parse_info *edid_info)
{
}

/* parse multi block data buff */
void parse_cta_data_block(uint8_t *p_blk_buff, uint8_t buf_len,
	struct cta_blk_parse_info *blk_parse_info)
{
	unsigned char tag_data, tag_code, extend_tag_code;
	unsigned char data_blk_len;
	unsigned char index = 0;
	unsigned char i = 0;
	int ret;

	if (!p_blk_buff || !blk_parse_info || (buf_len == 0))
		return;

	while ((index < buf_len) && (i < DATA_BLK_MAX_NUM)) {
		tag_data = p_blk_buff[index];
		tag_code = (tag_data & 0xE0) >> 5;
		/* data block payload length */
		data_blk_len = tag_data & 0x1F;
		/* length beyond max offset, force to break */
		if ((index + data_blk_len + 1) > buf_len) {
			rx_pr("unexpected data blk len\n");
			break;
		}
		blk_parse_info->db_info[i].cta_blk_index = i;
		blk_parse_info->db_info[i].tag_code = tag_code;
		blk_parse_info->db_info[i].offset = index;
		/* length including header */
		blk_parse_info->db_info[i].blk_len = data_blk_len+1;
		blk_parse_info->data_blk_num = i+1;
		switch (tag_code) {
		/* data block payload offset: index+1
		 * length: payload length
		 */
		case VIDEO_TAG:
			get_edid_video_data(p_blk_buff, index+1,
				data_blk_len, blk_parse_info);
			break;
		case AUDIO_TAG:
			get_edid_audio_data(p_blk_buff, index+1,
				data_blk_len, blk_parse_info);
			break;
		case SPEAKER_TAG:
			get_edid_speaker_data(p_blk_buff, index+1,
				data_blk_len, blk_parse_info);
			break;
		case VENDOR_TAG:
			ret = get_edid_vsdb(p_blk_buff, index+1,
				data_blk_len, blk_parse_info);
			if (ret == 2)
				blk_parse_info->db_info[i].tag_code =
					tag_code + HF_VSDB_OFFSET;
			else if (ret == 0)
				rx_pr("illegal VSDB\n");
			break;
		case VDTCDB_TAG:
			break;
		case USE_EXTENDED_TAG:
			extend_tag_code = p_blk_buff[index+1];
			blk_parse_info->db_info[i].tag_code =
				(USE_EXTENDED_TAG << 8) | extend_tag_code;
			switch (extend_tag_code) {
			/* offset: start after extended tag code
			 * length: payload length except extend tag
			 */
			case VCDB_TAG:
				get_edid_vcdb(p_blk_buff, index+2,
					data_blk_len-1, blk_parse_info);
				break;
			case VSVDB_TAG:
				get_edid_dv_data(p_blk_buff, index+2,
					data_blk_len-1, blk_parse_info);
				break;
			case CDB_TAG:
				get_edid_colorimetry_data(p_blk_buff, index+2,
					data_blk_len-1, blk_parse_info);
				break;
			case HDR_STATIC_TAG:
				get_hdr_data(p_blk_buff, index+2,
					data_blk_len-1, blk_parse_info);
				break;
			case Y420VDB_TAG:
				get_edid_y420_vid_data(p_blk_buff, index+2,
					data_blk_len-1, blk_parse_info);
				break;
			case Y420CMDB_TAG:
				get_edid_y420_cap_map_data(p_blk_buff, index+2,
					data_blk_len-1, blk_parse_info);
				break;
			case VSADB_TAG:
				get_edid_vsadb(p_blk_buff, index+2,
					data_blk_len-1, blk_parse_info);
				break;
			case RCDB_TAG:
				get_edid_rcdb(p_blk_buff, index+2,
					data_blk_len-1, blk_parse_info);
				break;
			case SLDB_TAG:
				get_edid_sldb(p_blk_buff, index+2,
					data_blk_len-1, blk_parse_info);
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
		/* next tag offset */
		index += (data_blk_len+1);
		i++;
	}
}

/* parse CEA extension block */
void edid_parse_cea_ext_block(uint8_t *p_edid,
	struct cea_ext_parse_info *edid_info)
{
	unsigned int max_offset;
	unsigned int blk_start_offset;
	unsigned char data_blk_total_len;
	unsigned char i;

	if (!p_edid || !edid_info)
		return;
	if (p_edid[0] != 0x02) {
		rx_pr("not a valid CEA block!\n");
		return;
	}
	edid_info->cea_tag = p_edid[0];
	edid_info->cea_revision = p_edid[1];
	max_offset = p_edid[2];
	edid_info->dtd_offset = max_offset;
	edid_info->underscan_sup = (p_edid[3] >> 7) & 0x1;
	edid_info->basic_aud_sup = (p_edid[3] >> 6) & 0x1;
	edid_info->ycc444_sup = (p_edid[3] >> 5) & 0x1;
	edid_info->ycc422_sup = (p_edid[3] >> 4) & 0x1;
	edid_info->native_dtd_num = p_edid[3] & 0xF;

	blk_start_offset = 4;
	data_blk_total_len = max_offset - blk_start_offset;
	parse_cta_data_block(&p_edid[blk_start_offset],
		data_blk_total_len, &edid_info->blk_parse_info);

	for (i = 0; i < edid_info->blk_parse_info.data_blk_num; i++)
		edid_info->blk_parse_info.db_info[i].offset += blk_start_offset;
}

void rx_edid_parse(uint8_t *p_edid, struct edid_info_s *edid_info)
{
	if (!p_edid || !edid_info)
		return;
	edid_parse_block0(p_edid, edid_info);
	edid_parse_cea_ext_block(p_edid+EDID_EXT_BLK_OFF,
		&edid_info->cea_ext_info);

	edid_info->free_size =
		rx_edid_free_size(p_edid, EDID_SIZE);
	edid_info->total_free_size =
		rx_edid_total_free_size(p_edid, EDID_SIZE);
	edid_info->dtd_size =
		rx_get_cea_dtd_size(p_edid, EDID_SIZE);
}

void rx_parse_blk0_print(struct edid_info_s *edid_info)
{
	if (!edid_info)
		return;
	rx_pr("****EDID Basic Block****\n");
	rx_pr("manufacturer_name: %s\n", edid_info->manufacturer_name);
	rx_pr("product code: 0x%04x\n", edid_info->product_code);
	rx_pr("serial_number: 0x%08x\n", edid_info->serial_number);
	rx_pr("product week: %d\n", edid_info->product_week);
	rx_pr("product year: %d\n", edid_info->product_year);
	rx_pr("descriptor1.hactive: %d\n", edid_info->descriptor1.hactive);
	rx_pr("descriptor1.vactive: %d\n", edid_info->descriptor1.vactive);
	rx_pr("descriptor1.pix clk: %dKHz\n",
		edid_info->descriptor1.pixel_clk*10);
	rx_pr("monitor name: %s\n", edid_info->monitor_name);
	rx_pr("max support pixel clock: %dMhz\n",
		edid_info->max_sup_pixel_clk);
	rx_pr("extension_flag: %d\n", edid_info->extension_flag);
	rx_pr("block0_chk_sum: 0x%x\n", edid_info->block0_chk_sum);
}

void rx_parse_print_vdb(struct video_db_s *video_db)
{
	unsigned char i;
	unsigned char hdmi_vic;

	if (!video_db)
		return;
	rx_pr("****Video Data Block****\n");
	rx_pr("support SVD list:\n");
	for (i = 0; i < video_db->svd_num; i++) {
		hdmi_vic = video_db->hdmi_vic[i];
		rx_edid_print_vic_fmt(i, hdmi_vic);
	}
}

void rx_parse_print_adb(struct audio_db_s *audio_db)
{
	enum edid_audio_format_e fmt;
	union bit_rate_u *bit_rate;

	if (!audio_db)
		return;
	rx_pr("****Audio Data Block****\n");
	for (fmt = AUDIO_FORMAT_LPCM; fmt <= AUDIO_FORMAT_WMAPRO; fmt++) {
		if (audio_db->aud_fmt_sup[fmt]) {
			rx_pr("audio fmt: %s\n", aud_fmt[fmt]);
			rx_pr("\tmax channel: %d\n",
				audio_db->sad[fmt].max_channel+1);
			if (audio_db->sad[fmt].freq_192khz)
				rx_pr("\tfreq_192khz\n");
			if (audio_db->sad[fmt].freq_176_4khz)
				rx_pr("\tfreq_176.4khz\n");
			if (audio_db->sad[fmt].freq_96khz)
				rx_pr("\tfreq_96khz\n");
			if (audio_db->sad[fmt].freq_88_2khz)
				rx_pr("\tfreq_88.2khz\n");
			if (audio_db->sad[fmt].freq_48khz)
				rx_pr("\tfreq_48khz\n");
			if (audio_db->sad[fmt].freq_44_1khz)
				rx_pr("\tfreq_44.1khz\n");
			if (audio_db->sad[fmt].freq_32khz)
				rx_pr("\tfreq_32khz\n");
			bit_rate = &(audio_db->sad[fmt].bit_rate);
			if (fmt == AUDIO_FORMAT_LPCM) {
				rx_pr("sample size:\n");
				if (bit_rate->pcm.size_16bit)
					rx_pr("\t16bit\n");
				if (bit_rate->pcm.size_20bit)
					rx_pr("\t20bit\n");
				if (bit_rate->pcm.size_24bit)
					rx_pr("\t24bit\n");
			} else if ((fmt >= AUDIO_FORMAT_AC3) &&
				(fmt <= AUDIO_FORMAT_ATRAC)) {
				rx_pr("\tmax bit rate: %dkHz\n",
					bit_rate->others*8);
			} else {
				rx_pr("\tformat dependent value: 0x%x\n",
					bit_rate->others);
			}
		}
	}
}

/* may need extend spker alloc */
void rx_parse_print_spk_alloc(struct speaker_alloc_db_s *spk_alloc)
{
	if (!spk_alloc)
		return;
	rx_pr("****Speaker Allocation Data Block****\n");
	if (spk_alloc->flw_frw)
		rx_pr("FLW/FRW\n");
	if (spk_alloc->rlc_rrc)
		rx_pr("RLC/RRC\n");
	if (spk_alloc->flc_frc)
		rx_pr("FLC/FRC\n");
	if (spk_alloc->rc)
		rx_pr("RC\n");
	if (spk_alloc->rl_rr)
		rx_pr("RL/RR\n");
	if (spk_alloc->fc)
		rx_pr("FC\n");
	if (spk_alloc->lfe)
		rx_pr("LFE\n");
	if (spk_alloc->fl_fr)
		rx_pr("FL/FR\n");
	if (spk_alloc->fch)
		rx_pr("FCH\n");
	if (spk_alloc->tc)
		rx_pr("TC\n");
	if (spk_alloc->flh_frh)
		rx_pr("FLH_FRH\n");
}

void rx_parse_print_vsdb(struct cta_blk_parse_info *edid_info)
{
	unsigned char i;
	unsigned char hdmi_vic;
	unsigned char svd_num;
	unsigned char _2d_vic_order;
	unsigned char _3d_struct;
	unsigned char _3d_detail;

	if (!edid_info)
		return;

	svd_num = edid_info->video_db.svd_num;
	rx_pr("****Vender Specific Data Block****\n");
	rx_pr("IEEE OUI: %06X\n",
		edid_info->vsdb.ieee_oui);
	rx_pr("phy addr: %d.%d.%d.%d\n",
		edid_info->vsdb.a, edid_info->vsdb.b,
		edid_info->vsdb.c, edid_info->vsdb.d);
	if (edid_info->vsdb.support_AI)
		rx_pr("support AI\n");
	rx_pr("support deep clor:\n");
	if (edid_info->vsdb.DC_48bit)
		rx_pr("\t16bit\n");
	if (edid_info->vsdb.DC_36bit)
		rx_pr("\t12bit\n");
	if (edid_info->vsdb.DC_30bit)
		rx_pr("\t10bit\n");
	if (edid_info->vsdb.dvi_dual)
		rx_pr("support dvi dual channel\n");
	if (edid_info->vsdb.max_tmds_clk > 0)
		rx_pr("max tmds clk supported: %dMHz\n",
			edid_info->vsdb.max_tmds_clk*5);
	rx_pr("hdmi_video_present: %d\n",
		edid_info->vsdb.hdmi_video_present);
	rx_pr("Content types:\n");
	if (edid_info->vsdb.cnc3)
		rx_pr("\tcnc3: Game\n");
	if (edid_info->vsdb.cnc2)
		rx_pr("\tcnc2: Cinema\n");
	if (edid_info->vsdb.cnc1)
		rx_pr("\tcnc1: Photo\n");
	if (edid_info->vsdb.cnc0)
		rx_pr("\tcnc0: Grahpics(text)\n");

	if (edid_info->vsdb.hdmi_vic_len > 0)
		rx_pr("Supproted 4k2k format:\n");
	if (edid_info->vsdb.hdmi_4k2k_30hz_sup)
		rx_pr("\thdmi vic1: 4k30hz\n");
	if (edid_info->vsdb.hdmi_4k2k_25hz_sup)
		rx_pr("\thdmi vic2: 4k25hz\n");
	if (edid_info->vsdb.hdmi_4k2k_24hz_sup)
		rx_pr("\thdmi vic3: 4k24hz\n");
	if (edid_info->vsdb.hdmi_smpte_sup)
		rx_pr("\thdmi vic4: smpte\n");
	/* Mandatory 3D format: HDMI1.4 spec page157 */
	if (edid_info->vsdb._3d_present) {
		rx_pr("Basic(Mandatory) 3D formats supported\n");
		rx_pr("Image Size:\n");
		switch (edid_info->vsdb.image_size) {
		case 0:
			rx_pr("\tNo additional information\n");
			break;
		case 1:
			rx_pr("\tOnly indicate correct aspect ratio\n");
			break;
		case 2:
			rx_pr("\tCorrect size: Accurate to 1(cm)\n");
			break;
		case 3:
			rx_pr("\tCorrect size: multiply by 5(cm)\n");
			break;
		default:
			break;
		}
	} else
		rx_pr("No 3D support\n");
	if (edid_info->vsdb._3d_multi_present == 1) {
		/* For each bit in _3d_struct which is set (=1),
		 * Sink supports the corresponding 3D_Structure
		 * for all of the VICs listed in the first 16
		 * entries in the EDID.
		 */
		rx_pr("General 3D format, on the first 16 SVDs:\n");
		for (i = 0; i < 16; i++) {
			if ((edid_info->vsdb._3d_struct_all >> i) & 0x1)
				rx_pr("\t%s\n",	_3d_structure[i]);
		}
	} else if (edid_info->vsdb._3d_multi_present == 2) {
		/* Where a bit is set (=1), for the corresponding
		 * VIC within the first 16 entries in the EDID,
		 * the Sink indicates 3D support as designate
		 * by the 3D_Structure_ALL_15бн0 field.
		 */
		rx_pr("General 3D format, on the SVDs below:\n");
		for (i = 0; i < 16; i++) {
			if ((edid_info->vsdb._3d_struct_all >> i) & 0x1)
				rx_pr("\t%s\n",	_3d_structure[i]);
		}
		rx_pr("For SVDs:\n");
		for (i = 0; (i < svd_num) && (i < 16); i++) {
			hdmi_vic = edid_info->video_db.hdmi_vic[i];
			if ((edid_info->vsdb._3d_mask_15_0 >> i) & 0x1)
				rx_edid_print_vic_fmt(i, hdmi_vic);
		}
	}

	if (edid_info->vsdb._2d_vic_num > 0)
		rx_pr("Specific VIC 3D information:\n");
	for (i = 0; (i < edid_info->vsdb._2d_vic_num)
		&& (i < svd_num) && (i < 16); i++) {
		_2d_vic_order =
			edid_info->vsdb._2d_vic[i]._2d_vic_order;
		_3d_struct =
			edid_info->vsdb._2d_vic[i]._3d_struct;
		hdmi_vic =
			edid_info->video_db.hdmi_vic[_2d_vic_order];
		rx_edid_print_vic_fmt(_2d_vic_order, hdmi_vic);
		rx_pr("\t\t3d format: %s\n",
			_3d_structure[_3d_struct]);
		if ((_3d_struct >= 0x8) &&
			(_3d_struct <= 0xF)) {
			_3d_detail =
				edid_info->vsdb._2d_vic[i]._3d_detail;
			rx_pr("\t\t3d_detail: %s\n",
				_3d_detail_x[_3d_detail]);
		}
	}
}

void rx_parse_print_hf_vsdb(struct hf_vsdb_s *hf_vsdb)
{
	if (!hf_vsdb)
		return;
	rx_pr("****HF-VSDB****\n");
	rx_pr("IEEE OUI: %06X\n",
		hf_vsdb->ieee_oui);
	rx_pr("hf-vsdb version: %d\n",
		hf_vsdb->version);
	rx_pr("max_tmds_rate: %dMHz\n",
		hf_vsdb->max_tmds_rate*5);
	rx_pr("scdc_present: %d\n",
		hf_vsdb->scdc_present);
	rx_pr("rr_cap: %d\n",
		hf_vsdb->rr_cap);
	rx_pr("lte_340m_scramble: %d\n",
		hf_vsdb->lte_340m_scramble);
	rx_pr("independ_view: %d\n",
		hf_vsdb->independ_view);
	rx_pr("dual_view: %d\n",
		hf_vsdb->dual_view);
	rx_pr("_3d_osd_disparity: %d\n",
		hf_vsdb->_3d_osd_disparity);
	rx_pr("48bit 420 endode: %d\n",
		hf_vsdb->dc_48bit_420);
	rx_pr("36bit 420 endode: %d\n",
		hf_vsdb->dc_36bit_420);
	rx_pr("30bit 420 endode: %d\n",
		hf_vsdb->dc_30bit_420);
}

void rx_parse_print_vcdb(struct video_cap_db_s *vcdb)
{
	if (!vcdb)
		return;
	rx_pr("****Video Cap Data Block****\n");
	rx_pr("YCC Quant Range:\n");
	if (vcdb->quanti_range_ycc)
		rx_pr("\tSelectable(via AVI YQ)\n");
	else
		rx_pr("\tNo Data\n");

	rx_pr("RGB Quant Range:\n");
	if (vcdb->quanti_range_rgb)
		rx_pr("\tSelectable(via AVI Q)\n");
	else
		rx_pr("\tNo Data\n");

	rx_pr("PT Scan behavior:\n");
	switch (vcdb->s_PT) {
	case 0:
		rx_pr("\trefer to CE/IT fields\n");
		break;
	case 1:
		rx_pr("\tAlways Overscanned\n");
		break;
	case 2:
		rx_pr("\tAlways Underscanned\n");
		break;
	case 3:
		rx_pr("\tSupport both over and underscan\n");
		break;
	default:
		break;
	}
	rx_pr("IT Scan behavior:\n");
	switch (vcdb->s_IT) {
	case 0:
		rx_pr("\tIT video format not support\n");
		break;
	case 1:
		rx_pr("\tAlways Overscanned\n");
		break;
	case 2:
		rx_pr("\tAlways Underscanned\n");
		break;
	case 3:
		rx_pr("\tSupport both over and underscan\n");
		break;
	default:
		break;
	}
	rx_pr("CE Scan behavior:\n");
	switch (vcdb->s_CE) {
	case 0:
		rx_pr("\tCE video format not support\n");
		break;
	case 1:
		rx_pr("\tAlways Overscanned\n");
		break;
	case 2:
		rx_pr("\tAlways Underscanned\n");
		break;
	case 3:
		rx_pr("\tSupport both over and underscan\n");
		break;
	default:
		break;
	}
}

void rx_parse_print_vsvdb(struct dv_vsvdb_s *dv_vsvdb)
{
	if (!dv_vsvdb)
		return;
	rx_pr("****VSVDB(dolby vision)****\n");
	rx_pr("IEEE_OUI: %06X\n",
		dv_vsvdb->ieee_oui);
	rx_pr("vsvdb version: %d\n",
		dv_vsvdb->version);
	rx_pr("sup_global_dimming: %d\n",
		dv_vsvdb->sup_global_dimming);
	rx_pr("sup_2160p60hz: %d\n",
		dv_vsvdb->sup_2160p60hz);
	rx_pr("sup_yuv422_12bit: %d\n",
		dv_vsvdb->sup_yuv422_12bit);
	rx_pr("Rx: 0x%x\n", dv_vsvdb->Rx);
	rx_pr("Ry: 0x%x\n", dv_vsvdb->Ry);
	rx_pr("Gx: 0x%x\n", dv_vsvdb->Gx);
	rx_pr("Gy: 0x%x\n", dv_vsvdb->Gy);
	rx_pr("Bx: 0x%x\n", dv_vsvdb->Bx);
	rx_pr("By: 0x%x\n", dv_vsvdb->By);
	if (dv_vsvdb->version == 0) {
		rx_pr("target max pq: 0x%x\n",
			dv_vsvdb->tmaxPQ);
		rx_pr("target min pq: 0x%x\n",
			dv_vsvdb->tminPQ);
		rx_pr("dm_major_ver: 0x%x\n",
			dv_vsvdb->dm_major_ver);
		rx_pr("dm_minor_ver: 0x%x\n",
			dv_vsvdb->dm_minor_ver);
	} else if (dv_vsvdb->version == 1) {
		rx_pr("DM_version: 0x%x\n",
			dv_vsvdb->DM_version);
		rx_pr("target_max_lum: 0x%x\n",
			dv_vsvdb->target_max_lum);
		rx_pr("target_min_lum: 0x%x\n",
			dv_vsvdb->target_min_lum);
		rx_pr("colormetry: 0x%x\n",
			dv_vsvdb->colormetry);
	}
}
void rx_parse_print_cdb(struct colorimetry_db_s *color_db)
{
	if (!color_db)
		return;
	rx_pr("****Colorimetry Data Block****\n");
	rx_pr("supported colorimetry:\n");
	if (color_db->BT2020_RGB)
		rx_pr("\tBT2020_RGB\n");
	if (color_db->BT2020_YCC)
		rx_pr("\tBT2020_YCC\n");
	if (color_db->BT2020_cYCC)
		rx_pr("\tBT2020_cYCC\n");
	if (color_db->Adobe_RGB)
		rx_pr("\tAdobe_RGB\n");
	if (color_db->Adobe_YCC601)
		rx_pr("\tAdobe_YCC601\n");
	if (color_db->sYCC601)
		rx_pr("\tsYCC601\n");
	if (color_db->xvYCC709)
		rx_pr("\txvYCC709\n");
	if (color_db->xvYCC601)
		rx_pr("\txvYCC601\n");

	rx_pr("supported colorimetry metadata:\n");
	if (color_db->MD3)
		rx_pr("\tMD3\n");
	if (color_db->MD2)
		rx_pr("\tMD2\n");
	if (color_db->MD1)
		rx_pr("\tMD1\n");
	if (color_db->MD0)
		rx_pr("\tMD0\n");
}

void rx_parse_print_hdr_static(struct hdr_db_s *hdr_db)
{
	if (!hdr_db)
		return;
	rx_pr("****HDR Static Metadata Data Block****\n");
	rx_pr("eotf_hlg: %d\n",
		hdr_db->eotf_hlg);
	rx_pr("eotf_smpte_st_2084: %d\n",
		hdr_db->eotf_smpte_st_2084);
	rx_pr("eotf_hdr: %d\n",
		hdr_db->eotf_hdr);
	rx_pr("eotf_sdr: %d\n",
		hdr_db->eotf_sdr);
	rx_pr("hdr_SMD_type1: %d\n",
		hdr_db->hdr_SMD_type1);
	if (hdr_db->hdr_lum_max)
		rx_pr("Desired Content Max Luminance: 0x%x\n",
			hdr_db->hdr_lum_max);
	if (hdr_db->hdr_lum_avg)
		rx_pr("Desired Content Max Frame-avg Luminance: 0x%x\n",
			hdr_db->hdr_lum_avg);
	if (hdr_db->hdr_lum_min)
		rx_pr("Desired Content Min Luminance: 0x%x\n",
			hdr_db->hdr_lum_min);
}

void rx_parse_print_y420vdb(struct cta_blk_parse_info *edid_info)
{
	unsigned char i;

	if (!edid_info)
		return;
	rx_pr("****Y420 Video Data Block****\n");
	for (i = 0; i < edid_info->y420_vic_len; i++)
		rx_edid_print_vic_fmt(i,
			edid_info->y420_vdb_vic[i]);
}

void rx_parse_print_y420cmdb(struct cta_blk_parse_info *edid_info)
{
	unsigned char i;
	unsigned char hdmi_vic;

	if (!edid_info)
		return;
	rx_pr("****Yc420 capability map Data Block****\n");
	if (edid_info->y420_all_vic)
		rx_pr("all vic support y420\n");
	else {
		for (i = 0; i < 31; i++) {
			hdmi_vic = edid_info->y420_cmdb_vic[i];
			if (hdmi_vic)
				rx_edid_print_vic_fmt(i, hdmi_vic);
		}
	}
}

void rx_cea_ext_parse_print(struct cea_ext_parse_info *cea_ext_info)
{
	if (!cea_ext_info)
		return;
	rx_pr("****CEA Extension Block Header****\n");
	rx_pr("cea_tag: 0x%x\n", cea_ext_info->cea_tag);
	rx_pr("cea_revision: 0x%x\n", cea_ext_info->cea_revision);
	rx_pr("dtd offset: %d\n", cea_ext_info->dtd_offset);
	rx_pr("underscan_sup: %d\n", cea_ext_info->underscan_sup);
	rx_pr("basic_aud_sup: %d\n", cea_ext_info->basic_aud_sup);
	rx_pr("ycc444_sup: %d\n", cea_ext_info->ycc444_sup);
	rx_pr("ycc422_sup: %d\n", cea_ext_info->ycc422_sup);
	rx_pr("native_dtd_num: %d\n", cea_ext_info->native_dtd_num);

	rx_parse_print_vdb(&cea_ext_info->blk_parse_info.video_db);
	rx_parse_print_adb(&cea_ext_info->blk_parse_info.audio_db);
	rx_parse_print_spk_alloc(&cea_ext_info->blk_parse_info.speaker_alloc);
	rx_parse_print_vsdb(&cea_ext_info->blk_parse_info);
	if (cea_ext_info->blk_parse_info.contain_hf_vsdb)
		rx_parse_print_hf_vsdb(&cea_ext_info->blk_parse_info.hf_vsdb);
	if (cea_ext_info->blk_parse_info.contain_vcdb)
		rx_parse_print_vcdb(&cea_ext_info->blk_parse_info.vcdb);
	if (cea_ext_info->blk_parse_info.contain_cdb)
		rx_parse_print_cdb(&cea_ext_info->blk_parse_info.color_db);
	if (cea_ext_info->blk_parse_info.contain_vsvdb)
		rx_parse_print_vsvdb(&cea_ext_info->blk_parse_info.dv_vsvdb);
	if (cea_ext_info->blk_parse_info.contain_hdr_db)
		rx_parse_print_hdr_static(&cea_ext_info->blk_parse_info.hdr_db);
	if (cea_ext_info->blk_parse_info.contain_y420_vdb)
		rx_parse_print_y420vdb(&cea_ext_info->blk_parse_info);
	if (cea_ext_info->blk_parse_info.contain_y420_cmdb)
		rx_parse_print_y420cmdb(&cea_ext_info->blk_parse_info);
}

void rx_edid_parse_print(struct edid_info_s *edid_info)
{
	if (!edid_info)
		return;
	rx_parse_blk0_print(edid_info);
	rx_cea_ext_parse_print(&edid_info->cea_ext_info);

	rx_pr("CEA ext blk free size: %d\n", edid_info->free_size);
	rx_pr("CEA ext blk total free size(include dtd size): %d\n",
		edid_info->total_free_size);
	rx_pr("CEA ext blk dtd size: %d\n", edid_info->dtd_size);
}

void rx_data_blk_index_print(struct cta_data_blk_info *db_info)
{
	if (!db_info)
		return;
	rx_pr("%-7d\t 0x%-5X\t %-30s\t 0x%-8X\t %d\n",
		db_info->cta_blk_index,
		db_info->tag_code,
		rx_get_cta_blk_name(db_info->tag_code),
		db_info->offset,
		db_info->blk_len);
}

void rx_blk_index_print(struct cta_blk_parse_info *blk_info)
{
	int i;

	if (!blk_info)
		return;
	rx_pr("****CTA Data Block Index****\n");
	rx_pr("%-7s\t %-7s\t %-30s\t %-10s\t %s\n",
		"blk_idx", "blk_tag", "blk_name",
		"blk_offset", "blk_len");
	for (i = 0; i < blk_info->data_blk_num; i++)
		rx_data_blk_index_print(&blk_info->db_info[i]);
}

int rx_set_hdr_lumi(unsigned char *data, int len)
{
	if ((data == NULL) || (len == 0))
		return false;

	memset(receive_hdr_lum, 0, sizeof(receive_hdr_lum));
	if ((len == EDID_HDR_SIZE) && (*data != 0))
		memcpy(receive_hdr_lum, data + EDID_HDR_HEAD_LEN,
					len - EDID_HDR_HEAD_LEN);
	new_hdr_lum = true;
	return true;
}
EXPORT_SYMBOL(rx_set_hdr_lumi);

void rx_edid_physical_addr(int a, int b, int c, int d)
{
	tx_hpd_event = E_RCV;
	up_phy_addr = ((d & 0xf) << 12) |
		   ((c & 0xf) <<  8) |
		   ((b & 0xf) <<  4) |
		   ((a & 0xf) <<  0);

	/* if (log_level & EDID_LOG) */
	rx_pr("\nup_phy_addr = %x\n", up_phy_addr);
}
EXPORT_SYMBOL(rx_edid_physical_addr);

unsigned char rx_get_cea_dtd_size(unsigned char *cur_edid, unsigned int size)
{
	unsigned char dtd_block_offset;
	unsigned char dtd_size = 0;

	if (cur_edid == NULL)
		return 0;
	/* get description offset */
	dtd_block_offset =
		cur_edid[EDID_BLOCK1_OFFSET + EDID_DESCRIP_OFFSET];
	dtd_block_offset += EDID_BLOCK1_OFFSET;
	if (log_level & VIDEO_LOG)
		rx_pr("%s dtd offset start:%d\n", __func__, dtd_block_offset);
	/* dtd first two bytes are pixel clk != 0 */
	while ((dtd_block_offset+1 < size-1) &&
		(cur_edid[dtd_block_offset] ||
		cur_edid[dtd_block_offset+1])) {
		dtd_block_offset += DETAILED_TIMING_LEN;
		if (dtd_block_offset >= size-1)
			break;
		dtd_size += DETAILED_TIMING_LEN;
	}
	if (log_level & VIDEO_LOG)
		rx_pr("%s block_start end:%d\n", __func__, dtd_block_offset);

	return dtd_size;
}

/* rx_get_total_free_size
 * get total free size including dtd
 */
unsigned char rx_edid_total_free_size(unsigned char *cur_edid,
	unsigned int size)
{
	unsigned char dtd_block_offset;

	if (cur_edid == NULL)
		return 0;
	/* get description offset */
	dtd_block_offset =
		cur_edid[EDID_BLOCK1_OFFSET + EDID_DESCRIP_OFFSET];
	dtd_block_offset += EDID_BLOCK1_OFFSET;
	if (log_level & VIDEO_LOG)
		rx_pr("%s total free size: %d\n", __func__,
			size - dtd_block_offset - 1);
	/* free size except checksum */
	return (size - dtd_block_offset - 1);
}

bool rx_set_earc_cap_ds(unsigned char *data, unsigned int len)
{
	new_earc_cap_ds = true;
	memset(recv_earc_cap_ds, 0, sizeof(recv_earc_cap_ds));
	if ((data == NULL) ||
		(len > EARC_CAP_DS_MAX_LENGTH)) {
		return false;
	}

	memcpy(recv_earc_cap_ds, data, len);
	earc_cap_ds_len = len;

	rx_pr("*update earc cap_ds to edid*\n");
	hdmi_rx_top_edid_update();
	/* if currently in arc port, don't reset hpd */
	if (rx.open_fg && (rx.port != rx.arc_port)) {
		if (earc_cap_ds_update_hpd_en)
			rx_send_hpd_pulse();
	} else {
		pre_port = 0xff;
		rx_pr("update cap_ds later, in ARC port:%s\n",
			rx.port == rx.arc_port ? "Y" : "N");
	}
	return true;
}
EXPORT_SYMBOL(rx_set_earc_cap_ds);

/* cap_info need to be cleared firstly */
static bool parse_earc_cap_ds(unsigned char *cap_ds_in, unsigned int len_in,
	unsigned char *raw_edid_out, unsigned int *len_out,
	struct earc_cap_ds *cap_info)
{
	enum earc_cap_block_id edid_byte = EARC_CAP_DS_END_MARKER;
	unsigned int index_in = 0;
	unsigned int index_out = 0;
	unsigned char i = 0;

	if (!cap_ds_in || (len_in == 0) || (len_in > EARC_CAP_DS_MAX_LENGTH)) {
		rx_pr("invalid eARC Cap Data Structure\n");
		return false;
	}
	if (!cap_info)
		return false;
	if (cap_ds_in[index_in++] != CAP_DS_VER) {
		rx_pr("invalid eARC Cap Data Structure version\n");
		return false;
	}
	if (len_out)
		*len_out = 0;
	cap_info->cap_ds_len = len_in;
	cap_info->cap_ds_ver = CAP_DS_VER;

	if (index_in < len_in)
		edid_byte = cap_ds_in[index_in];

	while ((edid_byte != EARC_CAP_DS_END_MARKER) &&
		(index_in < len_in) &&
		(i < EARC_CAP_BLOCK_MAX)) {
		switch (edid_byte) {
		case EARC_CAP_BLOCK_ID_1:
		case EARC_CAP_BLOCK_ID_2:
		case EARC_CAP_BLOCK_ID_3:
			cap_info->cap_block[i].block_id = edid_byte;
			cap_info->cap_block[i].offset = index_in;
			cap_info->cap_block[i].payload_len =
				cap_ds_in[++index_in];
			/* payload index */
			index_in++;
			/* CAP_BLOCK1/2/3 maybe all need, currently only
			 * consider CAP_BLOCK_ID=1
			 */
			if (/* ((edid_byte == EARC_CAP_BLOCK_ID_1) || */
				/* (edid_byte == EARC_CAP_BLOCK_ID_2)) && */
				raw_edid_out &&
				len_out) {
				memcpy(&raw_edid_out[index_out],
					&cap_ds_in[index_in],
					cap_info->cap_block[i].payload_len);
				*len_out += cap_info->cap_block[i].payload_len;
			}
			index_in += cap_info->cap_block[i].payload_len;
			index_out += cap_info->cap_block[i].payload_len;
			i++;
			if (index_in < len_in)
				edid_byte = cap_ds_in[index_in];
			break;
		/* case EARC_CAP_BLOCK_ID_0: */
		default:
			edid_byte = EARC_CAP_DS_END_MARKER;
			break;
		}
	}

	if (raw_edid_out && len_out && *len_out)
		parse_cta_data_block(raw_edid_out, *len_out,
			&cap_info->blk_parse_info);
	return true;
}

void cap_blk_index_print(struct cap_block_s *cap_blk)
{
	if (!cap_blk)
		return;

	rx_pr("%-6d\t %-10d\t %d\n",
		cap_blk->block_id,
		cap_blk->offset,
		cap_blk->payload_len+1);
}

void earc_cap_ds_index_print(struct earc_cap_ds *cap_info)
{
	unsigned char i = 0;

	if (!cap_info)
		return;
	rx_pr("****eARC Cap Data Sturct Index****\n");
	rx_pr("cap_ds_len: %d\n", cap_info->cap_ds_len);
	rx_pr("cap_ds_ver: %d\n", cap_info->cap_ds_ver);
	if (cap_info->cap_ds_len > 1)
		rx_pr("%-6s\t %-10s\t %s\n",
			"Blk_ID", "Blk_Offset", "blk_len");
	while ((i < EARC_CAP_BLOCK_MAX) && cap_info->cap_block[i].block_id) {
		cap_blk_index_print(&cap_info->cap_block[i]);
		i++;
	}
}

void earc_cap_blk_info_print(struct cta_blk_parse_info *blk_info)
{
	if (!blk_info)
		return;
	rx_parse_print_adb(&blk_info->audio_db);
	rx_parse_print_spk_alloc(&blk_info->speaker_alloc);
	/* VSADB, RCDB, SLDB */
}

void rx_prase_earc_capds_dbg(void)
{
	bool ret;
	unsigned char raw_edid_out[EARC_CAP_DS_MAX_LENGTH] = {0};
	unsigned int raw_edid_len = 0;
	struct earc_cap_ds cap_info;

	memset(&cap_info, 0, sizeof(struct earc_cap_ds));
	ret = parse_earc_cap_ds(recv_earc_cap_ds, earc_cap_ds_len,
		raw_edid_out, &raw_edid_len, &cap_info);

	if (!ret)
		return;
	earc_cap_ds_index_print(&cap_info);
	if (raw_edid_len)
		earc_cap_blk_info_print(&cap_info.blk_parse_info);
}

/* extract data block with certain tag from EDID */
uint8_t *edid_tag_extract(uint8_t *p_edid, uint16_t tagid)
{
	unsigned int index = EDID_EXT_BLK_OFF;
	uint8_t tag_length;
	int tag_code;
	unsigned char max_offset;

	if (!p_edid)
		return NULL;
	 /* check if a cea extension block */
	if (p_edid[126]) {
		index += 4;
		max_offset = p_edid[130] + EDID_EXT_BLK_OFF;
		while (index < max_offset) {
			tag_code = rx_get_tag_code(p_edid+index);
			tag_length = BLK_LENGTH(p_edid[index]);
			if (tag_code == tagid)
				return &p_edid[index];
			index += tag_length + 1;
		}
	}
	return NULL;
}

/* extract data block with certain tag from block buf */
uint8_t *data_blk_extract(uint8_t *p_buf, unsigned int buf_len, uint16_t tagid)
{
	unsigned int index = 0;
	uint8_t tag_length;
	uint8_t tag_code;

	if (!p_buf || (buf_len > EDID_SIZE) ||
		(buf_len == 0))
		return NULL;
	while (index < buf_len) {
		/* Get the tag and length */
		tag_code = rx_get_tag_code(p_buf+index);
		tag_length = BLK_LENGTH(p_buf[index]);
		if (tagid == tag_code)
			return &p_buf[index];
		index += tag_length + 1;
	}
	return NULL;
}

/* combine short audio descriptors,
 * see FigureH-3 of HDMI2.1 SPEC
 */
unsigned char *compose_audio_db(uint8_t *aud_db, uint8_t *add_buf)
{
	uint8_t aud_db_len;
	uint8_t add_buf_len;
	uint8_t payload_len;
	uint8_t tmp_aud[DB_LEN_MAX-1] = {0};
	uint8_t *tmp_buf = add_buf;

	uint8_t i, j;
	uint8_t idx = 1;
	enum edid_audio_format_e aud_fmt;
	enum edid_audio_format_e tmp_fmt;

	if (!aud_db || !add_buf)
		return NULL;
	memset(com_aud, 0, sizeof(com_aud));
	aud_db_len = BLK_LENGTH(aud_db[0])+1;
	add_buf_len = BLK_LENGTH(add_buf[0])+1;

	for (i = 1; (i < aud_db_len) && (idx < DB_LEN_MAX-1); i += SAD_LEN) {
		aud_fmt = (aud_db[i] & 0x78) >> 3;
		for (j = 1; j < add_buf_len; j += SAD_LEN) {
			tmp_fmt = (tmp_buf[j] & 0x78) >> 3;
			if (aud_fmt == tmp_fmt)
				break;
		}
		/* copy this audio data to payload */
		if (j == add_buf_len)
			/* not find this fmt in add_buf */
			memcpy(com_aud+idx, aud_db+i, SAD_LEN);
		else {
			/* find this fmt in add_buf */
			memcpy(com_aud+idx, tmp_buf+j, SAD_LEN);
			/* delete this fmt from add_buf */
			memcpy(tmp_aud+1, tmp_buf+1, j-1);
			memcpy(tmp_aud+j, tmp_buf+j+SAD_LEN,
				add_buf_len-j-SAD_LEN);
			add_buf_len -= SAD_LEN;
			tmp_buf = tmp_aud;
		}
		idx += SAD_LEN;
	}
	/* copy ramin Short Audio Descriptors
	 * in add_buf, except blk header
	 */
	if (idx < sizeof(com_aud))
		if (idx + add_buf_len - 1 <= sizeof(com_aud))
			memcpy(com_aud + idx, tmp_buf + 1, add_buf_len - 1);
	payload_len = (idx - 1) + (add_buf_len - 1);
	/* data blk header */
	com_aud[0] = (AUDIO_TAG << 5) | payload_len;

	if (log_level & EDID_LOG) {
		rx_pr("++++after compose, audio data blk:\n");
		for (i = 0; i < payload_len+1; i++)
			rx_pr("%02x", com_aud[i]);
		rx_pr("\n");
	}
	return com_aud;
}

/* param[add_buf]: contain only one data blk.
 * param[blk_idx]: sequence number of the data blk
 * 1.if the data blk is not present in edid, then
 * add this data blk according to add_blk_idx,
 * otherwise compose and replace this data blk.
 * 2.if blk_idx is 0xFF, then add this data blk
 * after the last data blk, otherwise insert it
 * in the blk_idx place.
 */
void splice_data_blk_to_edid(uint8_t *p_edid, uint8_t *add_buf,
	uint8_t blk_idx)
{
	/* uint8_t *tag_data_blk = NULL; */
	uint8_t *add_data_blk = NULL;
	uint8_t tag_db_len = 0;
	uint8_t add_db_len = 0;
	uint8_t diff_len = 0;
	uint16_t tag_code = 0;
	uint8_t tag_offset = 0;

	int free_size = 0;
	uint8_t total_free_size = 0;
	uint8_t dtd_size = 0;
	uint8_t free_space_off;
	unsigned int i = 0;
	struct cea_ext_parse_info cea_ext;
	uint8_t num;

	if (!p_edid || !add_buf)
		return;

	free_size = rx_edid_free_size(p_edid, EDID_SIZE);
	if (free_size < 0) {
		rx_pr("wrong edid\n");
		return;
	}
	total_free_size =
		rx_edid_total_free_size(p_edid, EDID_SIZE);
	dtd_size = rx_get_cea_dtd_size(p_edid, EDID_SIZE);

	tag_code = (add_buf[0] >> 5) & 0x7;
	if (tag_code == USE_EXTENDED_TAG)
		tag_code = (tag_code << 8) | add_buf[1];
	/* tag_data_blk = edid_tag_extract(p_edid, tag_code); */
	tag_offset = rx_get_ceadata_offset(p_edid, add_buf);

	/* if (!tag_data_blk) { */
	if (tag_offset == 0) {
		/* not find the data blk, add it to edid */
		add_db_len = BLK_LENGTH(add_buf[0])+1;
		edid_parse_cea_ext_block(p_edid+EDID_EXT_BLK_OFF,
			&cea_ext);
		/* decide to add db after which data blk */
		num =
			cea_ext.blk_parse_info.data_blk_num;

		if (blk_idx >= num)
			/* add after the last data blk */
			tag_offset =
				cea_ext.blk_parse_info.db_info[num-1].offset+
				cea_ext.blk_parse_info.db_info[num-1].blk_len;
		else
			/* insert in blk_idx */
			tag_offset =
				cea_ext.blk_parse_info.db_info[blk_idx].offset;
		tag_offset += EDID_BLOCK1_OFFSET;
		if (add_db_len <= free_size) {
			/* move data behind added data block, except checksum */
			for (i = 0; i < EDID_SIZE-tag_offset-add_db_len-1; i++)
				p_edid[255-i-1] =
					p_edid[255-i-1-add_db_len];
		} else if (en_take_dtd_space) {
			if (add_db_len > total_free_size) {
				rx_pr("no enough space for splicing3, abort\n");
				return;
			}
			/* actually, total free size = free_size + dtd_size,
			 * add_db_len won't excess 32bytes, so may excess
			 * one dtd length, but mustn't excess 2 dtd length.
			 * need clear the replaced dtd to 0
			 */
			if (add_db_len <= free_size + DETAILED_TIMING_LEN) {
				free_space_off =
					255-free_size-DETAILED_TIMING_LEN;
				memset(&p_edid[free_space_off], 0,
					DETAILED_TIMING_LEN);
			} else {
				free_space_off =
					255-free_size-2*DETAILED_TIMING_LEN;
				memset(&p_edid[free_space_off], 0,
					2*DETAILED_TIMING_LEN);
			}
			/* move data behind current data
			 * block, except checksum
			 */
			for (i = 0; i < EDID_SIZE-tag_offset-add_db_len-1; i++)
				p_edid[255-i-1] =
					p_edid[255-i-1-add_db_len];
		} else {
			rx_pr("no enough space for splicing4, abort\n");
			return;
		}
		/* dtd offset modify */
		p_edid[EDID_BLOCK1_OFFSET + EDID_DESCRIP_OFFSET]
			+= add_db_len;
		/* copy added data block */
		memcpy(p_edid + tag_offset, add_buf, add_db_len);
	} else {
		/* tag_db_len = BLK_LENGTH(tag_data_blk[0])+1; */
		tag_db_len = BLK_LENGTH(p_edid[tag_offset])+1;
		add_data_blk = add_buf;
		/* compose and replace data blk */
		if (tag_code == AUDIO_TAG) {
			/* compose audio data blk */
			add_data_blk =
				compose_audio_db(&p_edid[tag_offset], add_buf);
				/* compose_audio_db(tag_data_blk, add_buf); */
		}
		/* replace data blk */
		if (!add_data_blk)
			return;
		add_db_len = BLK_LENGTH(add_data_blk[0])+1;
		if (tag_db_len >= add_db_len) {
			/* move data behind current data
			 * block, except checksum
			 */
			for (i = 0; i < EDID_SIZE-tag_offset-tag_db_len-1; i++)
				p_edid[tag_offset+i+add_db_len]	=
					p_edid[tag_offset+i+tag_db_len];
			/* need clear the new free space to 0 */
			free_size += (tag_db_len-add_db_len);
			free_space_off = 255-free_size;
			memset(&p_edid[free_space_off], 0, free_size);
		} else if (add_db_len - tag_db_len <= free_size) {
			/* move data behind current data
			 * block, except checksum
			 */
			for (i = 0; i < EDID_SIZE-tag_offset-add_db_len-1; i++)
				p_edid[255-i-1] =
					p_edid[255-i-1-(add_db_len-tag_db_len)];
		} else if (en_take_dtd_space) {
			diff_len = add_db_len - tag_db_len;
			if (diff_len > total_free_size) {
				rx_pr("no enough space for splicing, abort\n");
				return;
			}
			/* actually, total free size = free_size + dtd_size,
			 * diff_len won't excess 31bytes, so may excess
			 * one dtd length, but mustn't excess 2 dtd length.
			 * need clear the replaced dtd to 0
			 */
			if (diff_len <= free_size + DETAILED_TIMING_LEN) {
				free_space_off =
					255-free_size-DETAILED_TIMING_LEN;
				memset(&p_edid[free_space_off], 0,
					DETAILED_TIMING_LEN);
			} else {
				free_space_off =
					255-free_size-2*DETAILED_TIMING_LEN;
				memset(&p_edid[free_space_off], 0,
					2*DETAILED_TIMING_LEN);
			}
			/* move data behind current data
			 * block, except checksum
			 */
			for (i = 0; i < EDID_SIZE-tag_offset-add_db_len-1; i++)
				p_edid[255-i-1] =
					p_edid[255-i-1-(add_db_len-tag_db_len)];
		} else {
			rx_pr("no enough space for splicing2, abort\n");
			return;
		}
		/* dtd offset modify */
		p_edid[EDID_BLOCK1_OFFSET + EDID_DESCRIP_OFFSET]
			+= (add_db_len - tag_db_len);
		/* copy added data block */
		memcpy(p_edid + tag_offset, add_data_blk, add_db_len);
	}
	if (log_level & EDID_LOG) {
		rx_pr("++++edid data after splice:\n");
		for (i = 0; i < EDID_SIZE; i++)
			rx_pr("%02x", p_edid[i]);
		rx_pr("\n");
	}
}

/* param[add_buf] may contain multi data blk,
 * search the blk filtered by tag, and then
 * splice it with edid
 */
void splice_tag_db_to_edid(uint8_t *p_edid, uint8_t *add_buf,
	uint8_t buf_len, uint16_t tagid)
{
	uint8_t *tag_data_blk = NULL;
	unsigned int i;

	if (!p_edid || !add_buf)
		return;
	tag_data_blk = data_blk_extract(add_buf, buf_len, tagid);
	if (!tag_data_blk)
		return;
	if (log_level & EDID_LOG) {
		rx_pr("++++extracted data blk(tag=0x%x):\n", tagid);
		for (i = 0; i < BLK_LENGTH(tag_data_blk[0]) + 1; i++)
			rx_pr("%02x", tag_data_blk[i]);
		rx_pr("\n");
	}
	/* if db not exist in edid, then add it to the end */
	splice_data_blk_to_edid(p_edid, tag_data_blk, 0xFF);
}

/* romove cta data blk which tag = tagid */
void edid_rm_db_by_tag(uint8_t *p_edid, uint16_t tagid)
{
	int tag_offset;
	unsigned char tag_len;
	unsigned int i;
	int free_size;
	unsigned char free_space_off;
	uint8_t *tag_data_blk = NULL;

	if (!p_edid)
		return;
	free_size = rx_edid_free_size(p_edid, EDID_SIZE);
	if (free_size < 0) {
		rx_pr("wrong edid\n");
		return;
	}
	tag_data_blk = edid_tag_extract(p_edid, tagid);
	if (!tag_data_blk) {
		rx_pr("no this data blk in edid\n");
		return;
	}
	tag_offset = tag_data_blk - p_edid;
	tag_len = BLK_LENGTH(tag_data_blk[0]) + 1;
	/* move data behind the removed data block
	 * forward, except checksum & free size
	 */
	for (i = tag_offset; i < 255-tag_len-free_size; i++)
		p_edid[i] = p_edid[i+tag_len];
	free_size += tag_len;
	free_space_off = 255 - free_size;
	/* clear new free space to zero */
	memset(&p_edid[free_space_off], 0, tag_len);
	/* dtd offset modify */
	p_edid[EDID_BLOCK1_OFFSET + EDID_DESCRIP_OFFSET]
		-= tag_len;
	if (log_level & EDID_LOG) {
		rx_pr("++++edid data after rm db:\n");
		for (i = 0; i < EDID_SIZE; i++)
			rx_pr("%02x", p_edid[i]);
		rx_pr("\n");
	}
}

/* param[blk_idx]: start from 0
 * the sequence index of the data blk to be removed
 */
void edid_rm_db_by_idx(uint8_t *p_edid, uint8_t blk_idx)
{
	struct cea_ext_parse_info ext_blk_info;
	int tag_offset;
	unsigned char tag_len;
	unsigned int i;
	int free_size;
	unsigned char free_space_off;

	if (!p_edid)
		return;
	free_size = rx_edid_free_size(p_edid, EDID_SIZE);
	if (free_size < 0) {
		rx_pr("wrong edid\n");
		return;
	}
	edid_parse_cea_ext_block(p_edid+EDID_EXT_BLK_OFF,
		&ext_blk_info);
	if (blk_idx >= ext_blk_info.blk_parse_info.data_blk_num) {
		rx_pr("no this index data blk in edid\n");
		return;
	}
	tag_offset = EDID_EXT_BLK_OFF +
		ext_blk_info.blk_parse_info.db_info[blk_idx].offset;
	tag_len =
		ext_blk_info.blk_parse_info.db_info[blk_idx].blk_len;
	/* move data behind the removed data block
	 * forward, except checksum & free size
	 */
	for (i = tag_offset; i < 255-tag_len-free_size; i++)
		p_edid[i] = p_edid[i+tag_len];

	free_size += tag_len;
	free_space_off = 255 - free_size;
	/* clear new free space to zero */
	memset(&p_edid[free_space_off], 0, tag_len);
	/* dtd offset modify */
	p_edid[EDID_BLOCK1_OFFSET + EDID_DESCRIP_OFFSET]
		-= tag_len;
	if (log_level & EDID_LOG) {
		rx_pr("++++edid data after rm db:\n");
		for (i = 0; i < EDID_SIZE; i++)
			rx_pr("%02x", p_edid[i]);
		rx_pr("\n");
	}
}

void edid_splice_earc_capds(unsigned char *p_edid,
	unsigned char *earc_cap_ds, unsigned int len)
{
	bool ret;
	unsigned char raw_edid_out[EARC_CAP_DS_MAX_LENGTH] = {0};
	unsigned int raw_edid_len = 0;
	struct earc_cap_ds cap_info;
	unsigned int i;

	if (!p_edid || !earc_cap_ds ||
		(len == 0) || (len > EARC_CAP_DS_MAX_LENGTH)) {
		rx_pr("invalid splice data, abort\n");
		return;
	}
	memset(&cap_info, 0, sizeof(struct earc_cap_ds));
	ret = parse_earc_cap_ds(earc_cap_ds, len,
		raw_edid_out, &raw_edid_len, &cap_info);
	if (!ret) {
		rx_pr("earc cap ds parse failed\n");
		return;
	}
	if (log_level & EDID_LOG) {
		rx_pr("++++raw cta blks extracted from capds:\n");
		for (i = 0; i < raw_edid_len; i++)
			rx_pr("%02x", raw_edid_out[i]);
		rx_pr("\n");
	}
	splice_tag_db_to_edid(p_edid, raw_edid_out,
		raw_edid_len, AUDIO_TAG);
	splice_tag_db_to_edid(p_edid, raw_edid_out,
		raw_edid_len, SPEAKER_TAG);
}

void edid_splice_earc_capds_dbg(uint8_t *p_edid)
{
	struct edid_info_s edid_info;

	if (!p_edid)
		return;
	memset(&edid_info, 0, sizeof(struct edid_info_s));
	edid_splice_earc_capds(p_edid,
		recv_earc_cap_ds, earc_cap_ds_len);
	rx_edid_parse(p_edid, &edid_info);
	rx_edid_parse_print(&edid_info);
	rx_blk_index_print(&edid_info.cea_ext_info.blk_parse_info);
}

void edid_splice_data_blk_dbg(uint8_t *p_edid, uint8_t idx)
{
	struct edid_info_s edid_info;

	if (!p_edid)
		return;
	memset(&edid_info, 0, sizeof(struct edid_info_s));
	splice_data_blk_to_edid(p_edid, recv_earc_cap_ds, idx);
	rx_edid_parse(p_edid, &edid_info);
	rx_edid_parse_print(&edid_info);
	rx_blk_index_print(&edid_info.cea_ext_info.blk_parse_info);
}

