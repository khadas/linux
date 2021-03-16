/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef _INC_LCD_MIPI_H
#define _INC_LCD_MIPI_H
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>

/* **********************************
 * mipi-dsi read/write api
 */

/* mipi command(payload) */
/* format:  data_type, num, data.... */
/* special: data_type=0xff,
 *	    num<0xff means delay ms, num=0xff means ending.
 */

/* *************************************************************
 * Function: dsi_write_cmd
 * Supported Data Type: DT_GEN_SHORT_WR_0, DT_GEN_SHORT_WR_1, DT_GEN_SHORT_WR_2,
			DT_DCS_SHORT_WR_0, DT_DCS_SHORT_WR_1,
			DT_GEN_LONG_WR, DT_DCS_LONG_WR,
			DT_SET_MAX_RET_PKT_SIZE
			DT_GEN_RD_0, DT_GEN_RD_1, DT_GEN_RD_2,
			DT_DCS_RD_0
 * Return:              command number
 */
int dsi_write_cmd(struct aml_lcd_drv_s *pdrv, unsigned char *payload);

/* *************************************************************
 * Function: dsi_read_single
 * Supported Data Type: DT_GEN_RD_0, DT_GEN_RD_1, DT_GEN_RD_2,
			DT_DCS_RD_0
 * Return:              data count
			0 for not support
 */
int dsi_read_single(struct aml_lcd_drv_s *pdrv, unsigned char *payload,
		    unsigned char *rd_data, unsigned int rd_byte_len);

#endif
