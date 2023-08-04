// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "DP_tx.h"
#include "../../lcd_reg.h"
#include "../../lcd_common.h"

#define DPTX_AUX_REQ_TIMEOUT  1000
#define DPTX_AUX_REQ_INTERVAL 10
#define EDP_AUX_RETRY_CNT     3
#define EDP_AUX_TIMEOUT       30
#define EDP_AUX_INTERVAL      100
//AUX operation

#define DPTX_AUX_CMD_WRITE            0x8
#define DPTX_AUX_CMD_READ             0x9
#define DPTX_AUX_CMD_I2C_WRITE        0x0
#define DPTX_AUX_CMD_I2C_WRITE_MOT    0x4
#define DPTX_AUX_CMD_I2C_READ         0x1
#define DPTX_AUX_CMD_I2C_READ_MOT     0x5
#define DPTX_AUX_CMD_I2C_WRITE_STATUS 0x2

#define AUX_STATUS_REPLY_ERROR         BIT(3)
#define AUX_STATUS_REQUEST_IN_PROGRESS BIT(2)
#define AUX_STATUS_REPLY_IN_PROGRESS   BIT(1)
#define AUX_STATUS_REPLY_RECEIVED      BIT(0)

#define AUX_REPLY_CODE_ACK       0x0
#define AUX_REPLY_CODE_AUX_NACK  0x1
#define AUX_REPLY_CODE_AUX_Defer 0x2
#define AUX_REPLY_CODE_I2C_NACK  0x4
#define AUX_REPLY_CODE_I2C_Defer 0x8

static void dptx_aux_request(struct aml_lcd_drv_s *pdrv, struct dptx_aux_req_s *req)
{
	unsigned int state, timeout = 0;
	int i = 0;

	timeout = 0;
	while (timeout++ < DPTX_AUX_REQ_TIMEOUT) {
		state = dptx_reg_getb(pdrv, EDP_TX_AUX_STATE, 1, 1);
		if (state == 0)
			break;
		udelay(DPTX_AUX_REQ_TIMEOUT);
	};

	dptx_reg_write(pdrv, EDP_TX_AUX_ADDRESS, req->address);
	/*submit data only for write commands*/
	if (req->cmd_state == 0) {
		for (i = 0; i < req->byte_cnt; i++)
			dptx_reg_write(pdrv, EDP_TX_AUX_WRITE_FIFO, req->data[i]);
	}
	/*submit the command and the data size*/
	dptx_reg_write(pdrv, EDP_TX_AUX_COMMAND,
		       ((req->cmd_code << 8) | ((req->byte_cnt - 1) & 0xf)));
}

static int dptx_aux_submit_cmd(struct aml_lcd_drv_s *pdrv, struct dptx_aux_req_s *req)
{
	unsigned int status = 0, reply = 0;
	unsigned int retry_cnt = 0, timeout = 0;
	char str[6];

	if (!pdrv)
		return -1;

	if (!dptx_reg_read(pdrv, EDP_TX_TRANSMITTER_OUTPUT_ENABLE)) {
		LCDERR("[%d]: %s: DPtx is not enabled\n", pdrv->index, __func__);
		return -1;
	}

	if (req->cmd_state)
		sprintf(str, "read");
	else
		sprintf(str, "write");

dptx_aux_submit_cmd_retry:
	dptx_aux_request(pdrv, req);

	timeout = 0;
	while (timeout++ < EDP_AUX_TIMEOUT) {
		usleep_range(EDP_AUX_INTERVAL, 2 * EDP_AUX_INTERVAL);
		status = dptx_reg_read(pdrv, EDP_TX_AUX_TRANSFER_STATUS);
		if (status & AUX_STATUS_REPLY_ERROR) {
			LCDPR("[%d]: aux %s 0x%x reply status error!\n",
				pdrv->index, str, req->address);
			break;
		}

		if (status & AUX_STATUS_REPLY_RECEIVED) {
			reply = dptx_reg_read(pdrv, EDP_TX_AUX_REPLY_CODE);
			if (reply == AUX_REPLY_CODE_ACK)
				return 0;
			timeout++;
			if (!(lcd_debug_print_flag & LCD_DBG_PR_ADV))
				break;
			if (reply & AUX_REPLY_CODE_AUX_Defer)
				LCDPR("[%d]: aux %s 0x%x aux Defer\n",
					pdrv->index, str, req->address);
			if (reply & AUX_REPLY_CODE_AUX_NACK)
				LCDPR("[%d]: aux %s 0x%x aux NACK\n",
					pdrv->index, str, req->address);
			if (reply & AUX_REPLY_CODE_I2C_Defer)
				LCDPR("[%d]: aux %s 0x%x i2c Defer\n",
					pdrv->index, str, req->address);
			if (reply & AUX_REPLY_CODE_I2C_NACK)
				LCDPR("[%d]: aux %s 0x%x i2c NACK\n",
					pdrv->index, str, req->address);
			break;
		}
	}

	if (retry_cnt++ < EDP_AUX_RETRY_CNT) {
		udelay(EDP_AUX_INTERVAL);
		LCDPR("[%d]: aux %s addr 0x%x timeout, status 0x%x, reply 0x%x, retry %d\n",
		      pdrv->index, str, req->address, status, reply, retry_cnt);
		goto dptx_aux_submit_cmd_retry;
	}

	LCDERR("[%d]: aux %s addr 0x%x failed\n", pdrv->index, str, req->address);
	return -1;
}

int dptx_aux_write(struct aml_lcd_drv_s *pdrv, unsigned int addr, int len, unsigned char *buf)
{
	struct dptx_aux_req_s aux_req;
	int ret;

	if (!buf)
		return -1;

	aux_req.cmd_code = DPTX_AUX_CMD_WRITE;
	aux_req.cmd_state = 0;
	aux_req.address = addr;
	aux_req.byte_cnt = len;
	aux_req.data = buf;

	ret = dptx_aux_submit_cmd(pdrv, &aux_req);
	return ret;
}

int dptx_aux_write_single(struct aml_lcd_drv_s *pdrv, unsigned int addr, unsigned char val)
{
	struct dptx_aux_req_s aux_req;
	unsigned char auxdata;
	int ret;

	auxdata = val;

	aux_req.cmd_code = DPTX_AUX_CMD_WRITE;
	aux_req.cmd_state = 0;
	aux_req.address = addr;
	aux_req.byte_cnt = 1;
	aux_req.data = &auxdata;

	ret = dptx_aux_submit_cmd(pdrv, &aux_req);
	return ret;
}

int dptx_aux_read(struct aml_lcd_drv_s *pdrv, unsigned int addr, int len, unsigned char *buf)
{
	struct dptx_aux_req_s aux_req;
	int ret, i;

	if (!buf)
		return -1;

	aux_req.cmd_code = DPTX_AUX_CMD_READ;
	aux_req.cmd_state = 0;
	aux_req.address = addr;
	aux_req.byte_cnt = len;
	aux_req.data = buf;

	ret = dptx_aux_submit_cmd(pdrv, &aux_req);
	if (ret)
		return -1;

	for (i = 0; i < len; i++)
		buf[i] = (unsigned char)(dptx_reg_read(pdrv, EDP_TX_AUX_REPLY_DATA));

	return 0;
}

int dptx_aux_i2c_read(struct aml_lcd_drv_s *pdrv, unsigned int dev_addr, unsigned int reg_addr,
					unsigned int len, unsigned char *buf)
{
	struct dptx_aux_req_s aux_req;
	unsigned char aux_data[4];
	unsigned int n = 0, reply_count = 0;
	int i, ret;

	len = (len > 16) ? 16 : len; /*cap the byte count*/

	aux_data[0] = reg_addr;
	aux_data[1] = 0x00;

	/*send the dev_addr write*/
	aux_req.cmd_code = DPTX_AUX_CMD_I2C_WRITE_MOT;
	aux_req.cmd_state = 0;
	aux_req.address = dev_addr;
	aux_req.byte_cnt = 1;
	aux_req.data = aux_data;

	ret = dptx_aux_submit_cmd(pdrv, &aux_req);
	if (ret)
		return -1;

	/*submit the read command to hardware*/
	aux_req.cmd_code = DPTX_AUX_CMD_I2C_READ;
	aux_req.cmd_state = 1;
	aux_req.address = dev_addr;
	aux_req.byte_cnt = len;

	while (n < len) {
		ret = dptx_aux_submit_cmd(pdrv, &aux_req);
		if (ret)
			return -1;

		reply_count = dptx_reg_read(pdrv, EDP_TX_AUX_REPLY_DATA_COUNT);
		for (i = 0; i < reply_count; i++) {
			buf[n] = dptx_reg_read(pdrv, EDP_TX_AUX_REPLY_DATA);
			n++;
		}

		aux_req.byte_cnt -= reply_count;
		/*increment the address for the next transaction*/
		aux_data[0] += reply_count;
	}

	return 0;
}
