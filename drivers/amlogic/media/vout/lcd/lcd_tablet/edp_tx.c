// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "edp_tx.h"
#include "lcd_tablet.h"
#include "../lcd_reg.h"
#include "../lcd_common.h"

#define EDP_TX_AUX_REQ_TIMEOUT   1000
#define EDP_TX_AUX_REQ_INTERVAL  1
#define EDP_AUX_RETRY_CNT        5
#define EDP_AUX_TIMEOUT          1000
#define EDP_AUX_INTERVAL         200

static int dptx_aux_check(struct aml_lcd_drv_s *pdrv)
{
	if (dptx_reg_read(pdrv, EDP_TX_TRANSMITTER_OUTPUT_ENABLE))
		return 0;

	LCDERR("[%d]: %s: dptx is not enabled\n", pdrv->index, __func__);
	return -1;
}

static void dptx_aux_request(struct aml_lcd_drv_s *pdrv, struct dptx_aux_req_s *req)
{
	unsigned int state, timeout = 0;
	int i = 0;

	timeout = 0;
	while (timeout++ < EDP_TX_AUX_REQ_TIMEOUT) {
		state = dptx_reg_getb(pdrv, EDP_TX_AUX_STATE, 1, 1);
		if (state == 0)
			break;
		lcd_delay_us(EDP_TX_AUX_REQ_INTERVAL);
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
	char str[8];

	if (dptx_aux_check(pdrv))
		return -1;

	if (req->cmd_state)
		sprintf(str, "read");
	else
		sprintf(str, "write");

dptx_aux_submit_cmd_retry:
	dptx_aux_request(pdrv, req);

	timeout = 0;
	while (timeout++ < EDP_AUX_TIMEOUT) {
		lcd_delay_us(EDP_AUX_INTERVAL);
		reply = 0;
		status = dptx_reg_read(pdrv, EDP_TX_AUX_TRANSFER_STATUS);
		if (status & (1 << 0)) {
			reply = dptx_reg_read(pdrv, EDP_TX_AUX_REPLY_CODE);
			if (reply == DPTX_AUX_REPLY_CODE_ACK)
				return 0;
			if (reply & DPTX_AUX_REPLY_CODE_DEFER) {
				if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
					LCDPR("[%d]: edp aux %s addr 0x%x Defer!\n",
					      pdrv->index, str, req->address);
				}
			}
			if (reply & DPTX_AUX_REPLY_CODE_NACK) {
				if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
					LCDPR("[%d]: edp aux %s addr 0x%x NACK!\n",
					      pdrv->index, str, req->address);
				}
				//return -1;
			}
			if (reply & DPTX_AUX_REPLY_CODE_I2C_DEFER) {
				if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
					LCDPR("[%d]: edp aux i2c %s addr 0x%x Defer!\n",
					      pdrv->index, str, req->address);
				}
			}
			if (reply & DPTX_AUX_REPLY_CODE_I2C_NACK) {
				if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
					LCDPR("[%d]: edp aux i2c %s addr 0x%x NACK!\n",
					      pdrv->index, str, req->address);
				}
				//return -1;
			}
			break;
		}

		if (status & (1 << 3)) {
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: edp aux %s addr 0x%x Error!\n",
				      pdrv->index, str, req->address);
			}
			break;
		}
	}

	if (retry_cnt++ < EDP_AUX_RETRY_CNT) {
		lcd_delay_us(EDP_AUX_INTERVAL);
		LCDPR("[%d]: edp aux %s addr 0x%x timeout, status 0x%x, reply 0x%x, retry %d\n",
		      pdrv->index, str, req->address, status, reply, retry_cnt);
		goto dptx_aux_submit_cmd_retry;
	}

	LCDPR("[%d]: edp aux %s addr 0x%x failed\n", pdrv->index, str, req->address);
	return -1;
}

static int dptx_aux_write(struct aml_lcd_drv_s *pdrv, unsigned int addr,
			  unsigned int len, unsigned char *buf)
{
	struct dptx_aux_req_s aux_req;
	int ret;

	aux_req.cmd_code = DPTX_AUX_CMD_WRITE;
	aux_req.cmd_state = 0;
	aux_req.address = addr;
	aux_req.byte_cnt = len;
	aux_req.data = buf;

	ret = dptx_aux_submit_cmd(pdrv, &aux_req);
	return ret;
}

static int dptx_aux_read(struct aml_lcd_drv_s *pdrv, unsigned int addr,
			 unsigned int len, unsigned char *buf)
{
	struct dptx_aux_req_s aux_req;
	int i, ret;

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

static int dptx_aux_i2c_read(struct aml_lcd_drv_s *pdrv, unsigned int dev_addr,
			     unsigned int reg_addr, unsigned int len,
			     unsigned char *buf)
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

static void dptx_edid_print(struct dptx_edid_s *edp_edid)
{
	pr_info("Manufacturer ID:       %s\n"
		"Product ID:            0x%04x\n"
		"Product SN:            0x%08x\n"
		"Week:                  %d\n"
		"Year:                  %d\n"
		"EDID Version:          %04x\n",
		edp_edid->manufacturer_id,
		edp_edid->product_id,
		edp_edid->product_sn,
		edp_edid->week,
		edp_edid->year,
		edp_edid->version);
	if (edp_edid->string_flag & (1 << 0))
		pr_info("Monitor Name:          %s\n", edp_edid->name);
	if (edp_edid->string_flag & (1 << 1))
		pr_info("Monitor AScii String:  %s\n", edp_edid->asc_string);
	if (edp_edid->string_flag & (1 << 2))
		pr_info("Monitor SN:            %s\n", edp_edid->serial_num);

	pr_info("Detail Timing:\n"
		"    Pixel Clock:   %d.%dMHz\n"
		"    H Active:      %d\n"
		"    H Blank:       %d\n"
		"    V Active:      %d\n"
		"    V Blank:       %d\n"
		"    H FP:          %d\n"
		"    H PW:          %d\n"
		"    V FP:          %d\n"
		"    V PW:          %d\n"
		"    H Size:        %dmm\n"
		"    V Size:        %dmm\n"
		"    H Border:      %d\n"
		"    V Border:      %d\n"
		"    Hsync Pol:     %d\n"
		"    Vsync Pol:     %d\n",
		edp_edid->preferred_timing.pclk / 1000000,
		(edp_edid->preferred_timing.pclk % 1000000) / 1000,
		edp_edid->preferred_timing.h_active,
		edp_edid->preferred_timing.h_blank,
		edp_edid->preferred_timing.v_active,
		edp_edid->preferred_timing.v_blank,
		edp_edid->preferred_timing.h_fp,
		edp_edid->preferred_timing.h_pw,
		edp_edid->preferred_timing.v_fp,
		edp_edid->preferred_timing.v_pw,
		edp_edid->preferred_timing.h_size,
		edp_edid->preferred_timing.v_size,
		edp_edid->preferred_timing.h_border,
		edp_edid->preferred_timing.v_border,
		(edp_edid->preferred_timing.timing_ctrl >> 1) & 0x1,
		(edp_edid->preferred_timing.timing_ctrl >> 2) & 0x1);
}

static int dptx_edid_valid_check(unsigned char *edid_buf)
{
	unsigned char header[8] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};
	unsigned int checksum = 0;
	int i;

	if (memcmp(edid_buf, header, 8)) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDERR("%s: invalid EDID header\n", __func__);
		return -1;
	}

	for (i = 0; i < 128; i++)
		checksum += edid_buf[i];
	if ((checksum & 0xff)) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDERR("%s: EDID checksum Wrong\n", __func__);
		return -1;
	}

	return 0;
}

static void dptx_edid_parse(unsigned char *edid_buf, struct dptx_edid_s *edp_edid)
{
	struct dptx_edid_range_limit_s *range;
	struct dptx_edid_timing_s *timing;
	unsigned int temp;
	int i, j;

	range = &edp_edid->range_limit;
	timing = &edp_edid->preferred_timing;

	temp = ((edid_buf[8] << 8) | edid_buf[9]);
	for (i = 0; i < 3; i++)
		edp_edid->manufacturer_id[i] = (((temp >> ((2 - i) * 5)) & 0x1f) - 1) + 'A';

	edp_edid->manufacturer_id[3] = '\0';
	temp = ((edid_buf[11] << 8) | edid_buf[10]);
	edp_edid->product_id = temp;
	temp = ((edid_buf[12] << 24) | (edid_buf[13] << 16) | (edid_buf[14] << 8) | edid_buf[15]);
	edp_edid->product_sn = temp;
	edp_edid->week = edid_buf[16];
	edp_edid->year = 1990 + edid_buf[17];
	temp = ((edid_buf[18] << 8) | edid_buf[19]);
	edp_edid->version = temp;

	edp_edid->string_flag = 0;
	for (i = 0; i < 4; i++) {
		j = 54 + i * 18;
		if ((edid_buf[j] + edid_buf[j + 1]) == 0) {
			if ((edid_buf[j + 2] + edid_buf[j + 4]) == 0) {
				switch (edid_buf[j + 3]) {
				case 0xfc: //monitor name
					memcpy(edp_edid->name, &edid_buf[j + 5], 13);
					edp_edid->name[13] = '\0';
					edp_edid->string_flag |= (1 << 0);
					break;
				case 0xfd: //monitor range limits
					range->min_vfreq = edid_buf[j + 5];
					range->max_v_freq = edid_buf[j + 6];
					range->min_hfreq = edid_buf[j + 7];
					range->max_hfreq = edid_buf[j + 8];
					range->max_pclk = edid_buf[j + 9];
					range->GTF_ctrl = ((edid_buf[j + 11] << 8) |
							   edid_buf[j + 10]);
					range->GTF_start_hfreq = edid_buf[j + 12] * 2000;
					range->GTF_C = edid_buf[j + 13] / 2;
					range->GTF_M = ((edid_buf[j + 15] << 8) |
							edid_buf[j + 14]);
					range->GTF_K = edid_buf[j + 16];
					range->GTF_J = edid_buf[j + 17] / 2;
					break;
				case 0xfe: //ascii string
					memcpy(edp_edid->asc_string, &edid_buf[j + 5], 13);
					edp_edid->asc_string[13] = '\0';
					edp_edid->string_flag |= (1 << 1);
					break;
				case 0xff: //monitor serial num
					memcpy(edp_edid->serial_num, &edid_buf[j + 5], 13);
					edp_edid->serial_num[13] = '\0';
					edp_edid->string_flag |= (1 << 2);
					break;
				default:
					break;
				}
			}
		} else {//detail timing
			temp = ((edid_buf[j + 1] << 8) | (edid_buf[j])) * 10000;
			timing->pclk = temp;
			temp = ((((edid_buf[j + 4] >> 4) & 0xf) << 8) | edid_buf[j + 2]);
			timing->h_active = temp;
			temp = ((((edid_buf[j + 4] >> 0) & 0xf) << 8) | edid_buf[j + 3]);
			timing->h_blank = temp;
			temp = ((((edid_buf[j + 7] >> 4) & 0xf) << 8) | edid_buf[j + 5]);
			timing->v_active = temp;
			temp = ((((edid_buf[j + 7] >> 0) & 0xf) << 8) | edid_buf[j + 6]);
			timing->v_blank = temp;
			temp = ((((edid_buf[j + 11] >> 6) & 0x3) << 8) | edid_buf[j + 8]);
			timing->h_fp = temp;
			temp = ((((edid_buf[j + 11] >> 4) & 0x3) << 8) | edid_buf[j + 9]);
			timing->h_pw = temp;
			temp = ((((edid_buf[j + 11] >> 2) & 0x3) << 4) |
				((edid_buf[j + 10] >> 4) & 0xf));
			timing->v_fp = temp;
			temp = ((((edid_buf[j + 11] >> 0) & 0x3) << 4) |
				((edid_buf[j + 10] >> 0) & 0xf));
			timing->v_pw = temp;
			temp = ((((edid_buf[j + 14] >> 4) & 0xf) << 8) | edid_buf[j + 12]);
			timing->h_size = temp;
			temp = ((((edid_buf[j + 14] >> 0) & 0xf) << 8) | edid_buf[j + 13]);
			timing->v_size = temp;
			timing->h_border = edid_buf[j + 15];
			timing->v_border = edid_buf[j + 16];
			timing->timing_ctrl = edid_buf[j + 17];
		}
	}

	edp_edid->ext_flag = edid_buf[126];
	edp_edid->checksum = edid_buf[127];
}

static int dptx_read_edid(struct aml_lcd_drv_s *pdrv, unsigned char *edid_buf)
{
	int i, ret;

	if (!edid_buf) {
		LCDERR("[%d]: %s: edid buf is null\n", pdrv->index, __func__);
		return -1;
	}

	for (i = 0; i < 128; i += 16) {
		ret = dptx_aux_i2c_read(pdrv, 0x50, i, 16, &edid_buf[i]);
		if (ret)
			break;
	}

	return ret;
}

static void edp_edid_timing_update(struct aml_lcd_drv_s *pdrv, struct dptx_edid_s *edp_edid)
{
	struct lcd_config_s *pconf = &pdrv->config;
	struct dptx_edid_timing_s *timing = &edp_edid->preferred_timing;
	unsigned int sync_duration;

	lcd_vout_notify_mode_change_pre(pdrv);

	pconf->basic.h_active = timing->h_active;
	pconf->basic.v_active = timing->v_active;
	pconf->basic.h_period = timing->h_active + timing->h_blank;
	pconf->basic.v_period = timing->v_active + timing->v_blank;

	pconf->timing.lcd_clk = timing->pclk;
	pconf->timing.lcd_clk_dft = pconf->timing.lcd_clk;
	sync_duration = timing->pclk / pconf->basic.h_period;
	sync_duration = sync_duration * 100 / pconf->basic.v_period;
	pconf->timing.frame_rate = sync_duration / 100;
	pconf->timing.sync_duration_num = sync_duration;
	pconf->timing.sync_duration_den = 100;

	pconf->timing.hsync_width = timing->h_pw;
	pconf->timing.hsync_bp = timing->h_blank - timing->h_fp - timing->h_pw;
	pconf->timing.hsync_pol = (timing->timing_ctrl >> 1) & 0x1;
	pconf->timing.vsync_width = timing->v_pw;
	pconf->timing.vsync_bp = timing->v_blank - timing->v_fp - timing->v_pw;
	pconf->timing.vsync_pol = (timing->timing_ctrl >> 2) & 0x1;

	pconf->basic.screen_width = timing->h_size;
	pconf->basic.screen_height = timing->v_size;

	lcd_timing_init_config(pdrv);
	lcd_tablet_config_update(pdrv);
	lcd_tablet_config_post_update(pdrv);
#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_clk_request(pdrv->lcd_vpu_dev, pdrv->config.timing.lcd_clk);
#endif
	lcd_set_clk(pdrv);
	lcd_set_venc_timing(pdrv);
	lcd_vinfo_update(pdrv);
}

int dptx_edid_dump(struct aml_lcd_drv_s *pdrv)
{
	struct dptx_edid_s edp_edid;
	unsigned char *edid_buf;
	char *str;
	int i, n, retry_cnt = 0, ret;

	edid_buf = kcalloc(128, sizeof(unsigned char), GFP_KERNEL);
	if (!edid_buf)
		return -1;

dptx_edid_dump_retry:
	ret = dptx_read_edid(pdrv, edid_buf);
	if (ret) {
		if (retry_cnt++ > DPTX_EDID_READ_RETRY_MAX) {
			LCDERR("[%d]: %s: failed to read EDID from sink\n",
				pdrv->index, __func__);
			goto dptx_edid_dump_err;
		}
		memset(edid_buf, 0, 128 * sizeof(unsigned char));
		goto dptx_edid_dump_retry;
	}
	ret = dptx_edid_valid_check(edid_buf);
	if (ret) {
		if (retry_cnt++ > DPTX_EDID_READ_RETRY_MAX) {
			LCDERR("[%d]: %s: edid data check error\n",
				pdrv->index, __func__);
			goto dptx_edid_dump_err;
		}

		memset(edid_buf, 0, 128 * sizeof(unsigned char));
		goto dptx_edid_dump_retry;
	}

	str = kcalloc(400, sizeof(char), GFP_KERNEL);
	if (str) {
		LCDPR("[%d]: EDID Raw data:\n", pdrv->index);
		n = 0;
		for (i = 0; i < 128; i++) {
			n += sprintf(str + n, " %02x", edid_buf[i]);
			if (i % 16 == 15)
				n += sprintf(str + n, "\n");
		}
		pr_info("%s\n", str);
		kfree(str);
	}

	dptx_edid_parse(edid_buf, &edp_edid);
	dptx_edid_print(&edp_edid);

	kfree(edid_buf);
	return 0;

dptx_edid_dump_err:
	kfree(edid_buf);
	return -1;
}

int dptx_edid_timing_probe(struct aml_lcd_drv_s *pdrv)
{
	struct edp_config_s *edp_cfg;
	unsigned char *edid_buf;
	struct dptx_edid_s edp_edid;
	char *str;
	int i, n, retry_cnt = 0, ret;

	edp_cfg = &pdrv->config.control.edp_cfg;
	edid_buf = edp_cfg->edid_data;
	if (edp_cfg->edid_en == 0)
		return 0;

	if ((edp_cfg->edid_state & EDP_EDID_STATE_LOAD) == 0) {
dptx_edid_timing_probe_retry:
		memset(edid_buf, 0, 128 * sizeof(unsigned char));
		ret = dptx_read_edid(pdrv, edid_buf);
		if (ret) {
			if (retry_cnt++ > DPTX_EDID_READ_RETRY_MAX) {
				LCDERR("[%d]: %s: failed to read EDID from sink, retry %d\n",
				       pdrv->index, __func__, retry_cnt);
				goto dptx_edid_timing_probe_err;
			}
			goto dptx_edid_timing_probe_retry;
		}
		ret = dptx_edid_valid_check(edid_buf);
		if (ret) {
			if (retry_cnt++ > DPTX_EDID_READ_RETRY_MAX) {
				LCDERR("[%d]: %s: edid data check error, retry %d\n",
				       pdrv->index, __func__, retry_cnt);
				goto dptx_edid_timing_probe_err;
			}
			goto dptx_edid_timing_probe_retry;
		}
		edp_cfg->edid_state |= EDP_EDID_STATE_LOAD;

		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			str = kcalloc(400, sizeof(char), GFP_KERNEL);
			if (str) {
				LCDPR("[%d]: EDID Raw data:\n", pdrv->index);
				n = 0;
				for (i = 0; i < 128; i++) {
					if (i % 16 == 0)
						n += sprintf(str + n, "\n");
					n += sprintf(str + n, " %02x", edid_buf[i]);
				}
				pr_info("%s\n", str);
				kfree(str);
			}
		}
	}

	if ((edp_cfg->edid_state & EDP_EDID_STATE_APPLY) == 0) {
		if (edp_cfg->edid_retry_cnt++ > EDP_EDID_RETRY_MAX)
			return -1;
		dptx_edid_parse(edid_buf, &edp_edid);
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			dptx_edid_print(&edp_edid);
		edp_edid_timing_update(pdrv, &edp_edid);
		edp_cfg->edid_state |= EDP_EDID_STATE_APPLY;
	}

	LCDPR("%s ok\n", __func__);
	return 0;

dptx_edid_timing_probe_err:
	LCDPR("%s failed\n", __func__);
	return -1;
}

static void dptx_link_fast_training(struct aml_lcd_drv_s *pdrv)
{
	int index = pdrv->index;
	unsigned char p_data = 0;
	int ret;

	/* disable scrambling */
	dptx_reg_write(pdrv, EDP_TX_SCRAMBLING_DISABLE, 0x1);

	/* set training pattern 1 */
	dptx_reg_write(pdrv, EDP_TX_TRAINING_PATTERN_SET, 0x1);
	p_data = 0x21;
	ret = dptx_aux_write(pdrv, EDP_DPCD_TRAINING_PATTERN_SET, 1, &p_data);
	if (ret)
		LCDERR("[%d]: edp training pattern 1 failed.....\n", index);
	lcd_delay_us(10);

	/* set training pattern 2 */
	dptx_reg_write(pdrv, EDP_TX_TRAINING_PATTERN_SET, 0x2);
	p_data = 0x22;
	ret = dptx_aux_write(pdrv, EDP_DPCD_TRAINING_PATTERN_SET, 1, &p_data);
	if (ret)
		LCDERR("[%d]: edp training pattern 2 failed.....\n", index);
	lcd_delay_us(10);

	/* set training pattern 3 */
	dptx_reg_write(pdrv, EDP_TX_TRAINING_PATTERN_SET, 0x3);
	p_data = 0x23;
	ret = dptx_aux_write(pdrv, EDP_DPCD_TRAINING_PATTERN_SET, 1, &p_data);
	if (ret)
		LCDERR("[%d]: edp training pattern 3 failed.....\n", index);
	lcd_delay_us(10);

	/* disable the training pattern */
	p_data = 0x20;
	ret = dptx_aux_write(pdrv, EDP_DPCD_TRAINING_PATTERN_SET, 1, &p_data);
	if (ret)
		LCDERR("[%d]: edp training pattern off failed.....\n", index);
	dptx_reg_write(pdrv, EDP_TX_TRAINING_PATTERN_SET, 0x0);
}

int dptx_dpcd_read(struct aml_lcd_drv_s *pdrv, unsigned char *buf,
		   unsigned int reg, int len)
{
	int index = pdrv->index;
	int ret;

	if (!buf)
		return -1;
	if (index > 1) {
		LCDERR("[%d]: %s: invalid drv_index\n", index, __func__);
		return -1;
	}

	ret = dptx_aux_read(pdrv, reg, len, buf);
	return ret;
}

int dptx_dpcd_write(struct aml_lcd_drv_s *pdrv, unsigned int reg, unsigned char val)
{
	int index = pdrv->index;
	unsigned char auxdata;
	int ret;

	if (index > 1) {
		LCDERR("[%d]: %s: invalid drv_index\n", index, __func__);
		return -1;
	}

	auxdata = val;
	ret = dptx_aux_write(pdrv, reg, 1, &auxdata);
	return ret;
}

void dptx_dpcd_dump(struct aml_lcd_drv_s *pdrv)
{
	int index = pdrv->index;
	unsigned char p_data[12];
	int ret, i;

	if (index > 1) {
		LCDERR("[%d]: %s: invalid drv_index\n", index, __func__);
		return;
	}

	memset(p_data, 0, 12);
	LCDPR("[%d]: edp DPCD link status:\n", index);
	ret = dptx_aux_read(pdrv, 0x100, 8, p_data);
	if (ret == 0) {
		for (i = 0; i < 8; i++)
			pr_info("0x%04x: 0x%02x\n", (0x100 + i), p_data[i]);
		pr_info("\n");
	}

	memset(p_data, 0, 12);
	LCDPR("[%d]: edp DPCD training status:\n", index);
	ret = dptx_aux_read(pdrv, 0x200, 12, p_data);
	if (ret == 0) {
		for (i = 0; i < 12; i++)
			pr_info("0x%04x: 0x%02x\n", (0x200 + i), p_data[i]);
		pr_info("\n");
	}
}

static void dptx_set_msa(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int hactive, vactive, htotal, vtotal, hsw, hbp, vsw, vbp;
	unsigned int bpc, data_per_lane, misc0_data, bit_depth, sync_mode;
	unsigned int m_vid; /*pclk/1000 */
	unsigned int n_vid; /*162000, 270000, 540000 */
	unsigned int ppc = 1; /* 1 pix per clock pix0 only */
	unsigned int cfmt = 0; /* RGB */

	hactive = pconf->basic.h_active;
	vactive = pconf->basic.v_active;
	htotal = pconf->basic.h_period;
	vtotal = pconf->basic.v_period;
	hsw = pconf->timing.hsync_width;
	hbp = pconf->timing.hsync_bp;
	vsw = pconf->timing.vsync_width;
	vbp = pconf->timing.vsync_bp;

	m_vid = pconf->timing.lcd_clk / 1000;
	switch (pconf->control.edp_cfg.link_rate) {
	case 1: /* 2.7G */
		n_vid = 270000;
		break;
	case 0: /* 1.62G */
	default:
		n_vid = 162000;
		break;
	}
	 /*6bit:0x0, 8bit:0x1, 10bit:0x2, 12bit:0x3 */
	switch (pconf->basic.lcd_bits) {
	case 6:
		bit_depth = 0x0;
		break;
	case 8:
		bit_depth = 0x1;
		break;
	case 10:
		bit_depth = 0x2;
		break;
	default:
		bit_depth = 0x7;
		break;
	}
	bpc = pconf->basic.lcd_bits; /* bits per color */
	sync_mode = pconf->control.edp_cfg.sync_clk_mode;
	data_per_lane = ((hactive * bpc * 3) + 15) / 16 - 1;

	/*bit[0] sync mode (1=sync 0=async) */
	misc0_data = (cfmt << 1) | (sync_mode << 0);
	misc0_data |= (bit_depth << 5);

	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_HTOTAL, htotal);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_VTOTAL, vtotal);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_POLARITY, (0 << 1) | (0 << 0));
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_HSWIDTH, hsw);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_VSWIDTH, vsw);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_HRES, hactive);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_VRES, vactive);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_HSTART, (hsw + hbp));
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_VSTART, (vsw + vbp));
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_MISC0, misc0_data);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_MISC1, 0x00000000);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_M_VID, m_vid); /*unit: 1kHz */
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_N_VID, n_vid); /*unit: 10kHz */
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_TRANSFER_UNIT_SIZE, 48);
		/*Temporary change to 48 */
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_DATA_COUNT_PER_LANE,
		       data_per_lane);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_USER_PIXEL_WIDTH, ppc);
}

static void dptx_reset(struct aml_lcd_drv_s *pdrv)
{
	unsigned int bit;

	if (pdrv->index)
		bit = 18;
	else
		bit = 17;

	lcd_reset_setb(pdrv, RESETCTRL_RESET1_MASK, 0, bit, 1);
	lcd_reset_setb(pdrv, RESETCTRL_RESET1_LEVEL, 0, bit, 1);
	lcd_delay_us(1);
	lcd_reset_setb(pdrv, RESETCTRL_RESET1_LEVEL, 1, bit, 1);
	lcd_delay_us(1);
}

static void dptx_phy_reset(struct aml_lcd_drv_s *pdrv)
{
	unsigned int bit;

	if (pdrv->index)
		bit = 20;
	else
		bit = 19;

	lcd_reset_setb(pdrv, RESETCTRL_RESET1_MASK, 0, bit, 1);
	lcd_reset_setb(pdrv, RESETCTRL_RESET1_LEVEL, 0, bit, 1);
	lcd_delay_us(1);
	lcd_reset_setb(pdrv, RESETCTRL_RESET1_LEVEL, 1, bit, 1);
	lcd_delay_us(1);
}

static int dptx_wait_phy_ready(struct aml_lcd_drv_s *pdrv)
{
	int index = pdrv->index;
	unsigned int data = 0;
	unsigned int done = 100;

	do {
		data = dptx_reg_read(pdrv, EDP_TX_PHY_STATUS);
		if (done < 20) {
			LCDPR("dptx%d wait phy ready: reg_val=0x%x, wait_count=%u\n",
			      index, data, (100 - done));
		}
		done--;
		lcd_delay_us(100);
	} while (((data & 0x7f) != 0x7f) && (done > 0));

	if ((data & 0x7f) == 0x7f)
		return 0;

	LCDERR("[%d]: edp tx phy error!\n", index);
	return -1;
}

#define EDP_HPD_TIMEOUT    1000
static void edp_tx_init(struct aml_lcd_drv_s *pdrv)
{
	unsigned int hpd_state = 0;
	unsigned char auxdata[2];
	int i, index, ret;

	index = pdrv->index;
	if (index > 1) {
		LCDERR("%s: invalid drv_index %d\n", __func__, index);
		return;
	}

	dptx_phy_reset(pdrv);
	dptx_reset(pdrv);
	mdelay(2);

	lcd_venc_enable(pdrv, 0);

	/* Set Aux channel clk-div: 24MHz */
	dptx_reg_write(pdrv, EDP_TX_AUX_CLOCK_DIVIDER, 24);

	/* Enable the transmitter */
	/* remove the reset on the PHY */
	dptx_reg_write(pdrv, EDP_TX_PHY_RESET, 0);
	dptx_wait_phy_ready(pdrv);
	mdelay(2);
	dptx_reg_write(pdrv, EDP_TX_TRANSMITTER_OUTPUT_ENABLE, 0x1);

	i = 0;
	while (i++ < EDP_HPD_TIMEOUT) {
		hpd_state = dptx_reg_getb(pdrv, EDP_TX_AUX_STATE, 0, 1);
		if (hpd_state)
			break;
		mdelay(2);
	}
	LCDPR("[%d]: edp HPD state: %d, i=%d\n", index, hpd_state, i);

	dptx_edid_timing_probe(pdrv);

	/* tx Link-rate and Lane_count */
	dptx_reg_write(pdrv, EDP_TX_LINK_BW_SET, 0x0a); /* Link-rate */
	dptx_reg_write(pdrv, EDP_TX_LINK_COUNT_SET, 0x02); /* Lanes */

	/* sink Link-rate and Lane_count */
	auxdata[0] = 0x0a; /* 2.7GHz //EDP_DPCD_LINK_BANDWIDTH_SET */
	auxdata[1] = 2;              /*EDP_DPCD_LANE_COUNT_SET */
	ret = dptx_aux_write(pdrv, EDP_DPCD_LINK_BANDWIDTH_SET, 2, auxdata);
	if (ret)
		LCDERR("[%d]: edp sink set lane rate & count failed.....\n", index);

	/* Power up link */
	auxdata[0] = 0x1;
	ret = dptx_aux_write(pdrv, EDP_DPCD_SET_POWER, 1, auxdata);
	if (ret)
		LCDERR("[%d]: edp sink power up link failed.....\n", index);

	dptx_link_fast_training(pdrv);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		dptx_dpcd_dump(pdrv);

	dptx_set_msa(pdrv);
	lcd_venc_enable(pdrv, 1);

	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_ENABLE, 0x1);
	LCDPR("[%d]: edp enable main stream video\n", index);
}

static void edp_tx_disable(struct aml_lcd_drv_s *pdrv)
{
	unsigned char auxdata;
	int index, ret;

	index = pdrv->index;
	if (index > 1) {
		LCDERR("%s: invalid drv_index %d\n", __func__, index);
		return;
	}

	/* Power down link */
	auxdata = 0x2;
	ret = dptx_aux_write(pdrv, EDP_DPCD_SET_POWER, 1, &auxdata);
	if (ret)
		LCDERR("[%d]: edp sink power down link failed.....\n", index);

	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_ENABLE, 0x0);
	LCDPR("[%d]: edp disable main stream video\n", index);

	/* disable the transmitter */
	dptx_reg_write(pdrv, EDP_TX_TRANSMITTER_OUTPUT_ENABLE, 0x0);
}

static void edp_power_init(int index)
{
#ifdef CONFIG_SECURE_POWER_CONTROL
/*#define PM_EDP0          48 */
/*#define PM_EDP1          49 */
	if (index)
		pwr_ctrl_psci_smc(PM_EDP1, 1);
	else
		pwr_ctrl_psci_smc(PM_EDP0, 1);
	LCDPR("[%d]: edp power domain on\n", index);
#endif
}

void edp_tx_ctrl(struct aml_lcd_drv_s *pdrv, int flag)
{
	if (flag) {
		edp_power_init(pdrv->index);
		edp_tx_init(pdrv);
	} else {
		edp_tx_disable(pdrv);
	}
}
