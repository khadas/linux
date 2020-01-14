/*
 * drivers/amlogic/atv_demod/atv_demod_driver.c
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

#define ATVDEMOD_DEVICE_NAME    "aml_atvdemod"
#define ATVDEMOD_DRIVER_NAME    "aml_atvdemod"
#define ATVDEMOD_MODULE_NAME    "aml_atvdemod"
#define ATVDEMOD_CLASS_NAME     "aml_atvdemod"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/stddef.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include <linux/amlogic/aml_atvdemod.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>

#include "drivers/media/dvb-core/dvb_frontend.h"

#include "atv_demod_debug.h"
#include "atv_demod_driver.h"
#include "atv_demod_v4l2.h"
#include "atv_demod_ops.h"
#include "atv_demod_access.h"

#include "atvdemod_func.h"
#include "atvauddemod_func.h"

/********************************CODE CHANGE LIST*****************************/
/* Date --- Version --- Note *************************************************/
/* 2019/11/05 --- V2.15 --- Add dynamic monitoring line frequency deviation. */
#define AMLATVDEMOD_VER "V2.15"

struct aml_atvdemod_device *amlatvdemod_devp;

unsigned int audio_gain_shift;
unsigned int audio_gain_lpr;

static ssize_t aml_atvdemod_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int n = 0;
	unsigned int ret = 0;
	char *buf_orig = NULL, *ps = NULL, *token = NULL;
	char *parm[4] = { NULL };
	unsigned int data_snr[128] = { 0 };
	unsigned int data_snr_avg = 0;
	int data_afc = 0, block_addr = 0, block_reg = 0, block_val = 0;
	int i = 0, val = 0;
	unsigned long tmp = 0, data = 0;
	struct aml_atvdemod_device *dev =
			container_of(class, struct aml_atvdemod_device, cls);
	struct atv_demod_priv *priv = dev->v4l2_fe.fe.analog_demod_priv;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	ps = buf_orig;

	while (1) {
		token = strsep(&ps, "\n ");
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if (parm[0] == NULL)
		goto EXIT;
#if 0
	if (priv->state != ATVDEMOD_STATE_WORK) {
		pr_info("atvdemod_state not work  ....\n");
		goto EXIT;
	}
#endif

	if (!strncmp(parm[0], "init", 4)) {
		ret = atv_demod_enter_mode(&dev->v4l2_fe.fe);
		if (ret)
			pr_info("atv init error.\n");
	} else if (!strncmp(parm[0], "audout_mode", 11)) {
		if (is_meson_txlx_cpu() || is_meson_txhd_cpu()
				|| is_meson_tl1_cpu() || is_meson_tm2_cpu()) {
			atvauddemod_set_outputmode();
			pr_info("atvauddemod_set_outputmode done ....\n");
		}
	} else if (!strncmp(parm[0], "signal_audmode", 14)) {
		int stereo_flag, sap_flag;
		if (is_meson_txlx_cpu() || is_meson_txhd_cpu()
				|| is_meson_tl1_cpu() || is_meson_tm2_cpu()) {
			update_btsc_mode(1, &stereo_flag, &sap_flag);
			pr_info("get signal_audmode done ....\n");
		}
	} else if (!strncmp(parm[0], "clk", 3)) {
		adc_set_pll_cntl(1, 0x1, NULL);
		atvdemod_clk_init();
		if (is_meson_txlx_cpu() || is_meson_txhd_cpu()
				|| is_meson_tl1_cpu() || is_meson_tm2_cpu())
			aud_demod_clk_gate(1);
		pr_info("atvdemod_clk_init done ....\n");
	} else if (!strncmp(parm[0], "tune", 4)) {
		/* val  = simple_strtol(parm[1], NULL, 10); */
	} else if (!strncmp(parm[0], "set", 3)) {
		if (!strncmp(parm[1], "avout_gain", 10)) {
			if (kstrtoul(buf + strlen("avout_offset") + 1,
					10, &tmp) == 0)
				val = tmp;
			atv_dmd_wr_byte(0x0c, 0x01, val & 0xff);
		} else if (!strncmp(parm[1], "avout_offset", 12)) {
			if (kstrtoul(buf + strlen("avout_offset") + 1,
					10, &tmp) == 0)
				val = tmp;
			atv_dmd_wr_byte(0x0c, 0x04, val & 0xff);
		} else if (!strncmp(parm[1], "atv_gain", 8)) {
			if (kstrtoul(buf + strlen("atv_gain") + 1,
					10, &tmp) == 0)
				val = tmp;
			atv_dmd_wr_byte(0x19, 0x01, val & 0xff);
		} else if (!strncmp(parm[1], "atv_offset", 10)) {
			if (kstrtoul(buf + strlen("atv_offset") + 1,
					10, &tmp) == 0)
				val = tmp;
			atv_dmd_wr_byte(0x19, 0x04, val & 0xff);
		}
	} else if (!strncmp(parm[0], "get", 3)) {
		if (!strncmp(parm[1], "avout_gain", 10)) {
			val = atv_dmd_rd_byte(0x0c, 0x01);
			pr_info("avout_gain:0x%x\n", val);
		} else if (!strncmp(parm[1], "avout_offset", 12)) {
			val = atv_dmd_rd_byte(0x0c, 0x04);
			pr_info("avout_offset:0x%x\n", val);
		} else if (!strncmp(parm[1], "atv_gain", 8)) {
			val = atv_dmd_rd_byte(0x19, 0x01);
			pr_info("atv_gain:0x%x\n", val);
		} else if (!strncmp(parm[1], "atv_offset", 10)) {
			val = atv_dmd_rd_byte(0x19, 0x04);
			pr_info("atv_offset:0x%x\n", val);
		}
	} else if (!strncmp(parm[0], "snr_hist", 8)) {
		data_snr_avg = 0;
		for (i = 0; i < 128; i++) {
			data_snr[i] = (atv_dmd_rd_long(APB_BLOCK_ADDR_VDAGC,
					0x50) >> 8);
			usleep_range(50 * 1000, 50 * 1000 + 100);
			data_snr_avg += data_snr[i];
		}
		data_snr_avg = data_snr_avg / 128;
		pr_info("**********snr_hist_128avg:0x%x(%d)*********\n",
				data_snr_avg,
				data_snr_avg);
	} else if (!strncmp(parm[0], "afc_info", 8)) {
		data_afc = retrieve_vpll_carrier_afc();
		pr_info("afc %d Khz.\n", data_afc);
	} else if (!strncmp(parm[0], "ver_info", 8)) {
		pr_info("aml_atvdemod_ver %s.\n",
				AMLATVDEMOD_VER);
	} else if (!strncmp(parm[0], "audio_autodet", 13)) {
		aml_audiomode_autodet(&dev->v4l2_fe);
	} else if (!strncmp(parm[0], "audio_gain_set", 14)) {
		if (kstrtoul(buf + strlen("audio_gain_set") + 1, 16, &tmp) == 0)
			val = tmp;
		aml_audio_valume_gain_set(val);
		pr_info("audio_gain_set : %d\n", val);
	} else if (!strncmp(parm[0], "audio_gain_get", 14)) {
		val = aml_audio_valume_gain_get();
		pr_info("audio_gain_get : %d\n", val);
	} else if (!strncmp(parm[0], "audio_gain_shift", 16)) {
		/* int db[] = {12, 6, 0, -6, -12, -18, -24, -30}; */
		tmp = adec_rd_reg(0x16);
		pr_info("audio_gain_shift : 0x%lx (%ld db).\n",
				(tmp & 0x7), (12 - (tmp & 0x7) * 6));
		if (parm[1] && kstrtoul(parm[1], 0, &data) == 0) {
			tmp = (tmp & (~0x07)) | (data & 0x7);
			pr_info("set audio_gain_shift : 0x%lx (%ld db).\n",
					(tmp & 0x7), (12 - (tmp & 0x7) * 6));
			adec_wr_reg(0x16, tmp);
		}
		audio_gain_shift = (tmp & 0x7);
	} else if (!strncmp(parm[0], "audio_gain_lpr", 14)) {
		tmp = adec_rd_reg(0x1c);
		pr_info("audio_gain_lpr : 0x%lx\n", tmp);
		if (parm[1] && kstrtoul(parm[1], 0, &tmp) == 0) {
			/*db = 20 * log(val / 0x200) */
			pr_info("set audio_gain_lpr : 0x%lx.\n", tmp);
			adec_wr_reg(0x1c, tmp);
		}
		audio_gain_lpr = tmp;
	} else if (!strncmp(parm[0], "fix_pwm_adj", 11)) {
		if (kstrtoul(parm[1], 10, &tmp) == 0) {
			val = tmp;
			aml_fix_PWM_adjust(val);
		}
	} else if (!strncmp(parm[0], "rs", 2)) {
		if (kstrtoul(parm[1], 16, &tmp) == 0)
			block_addr = tmp;
		if (kstrtoul(parm[2], 16, &tmp) == 0)
			block_reg = tmp;
		if (block_addr < APB_BLOCK_ADDR_TOP)
			block_val = atv_dmd_rd_long(block_addr, block_reg);
		pr_info("rs block_addr:0x%x,block_reg:0x%x,block_val:0x%x\n",
				block_addr,
				block_reg, block_val);
	} else if (!strncmp(parm[0], "ws", 2)) {
		if (kstrtoul(parm[1], 16, &tmp) == 0)
			block_addr = tmp;
		if (kstrtoul(parm[2], 16, &tmp) == 0)
			block_reg = tmp;
		if (kstrtoul(parm[3], 16, &tmp) == 0)
			block_val = tmp;
		if (block_addr < APB_BLOCK_ADDR_TOP)
			atv_dmd_wr_long(block_addr, block_reg, block_val);
		pr_info("ws block_addr:0x%x,block_reg:0x%x,block_val:0x%x\n",
				block_addr,
				block_reg, block_val);
		block_val = atv_dmd_rd_long(block_addr, block_reg);
		pr_info("readback_val:0x%x\n", block_val);
	} else if (!strncmp(parm[0], "snr_cur", 7)) {
		data_snr_avg = atvdemod_get_snr_val();
		pr_info("**********snr_cur:%d*********\n", data_snr_avg);
	} else if (!strncmp(parm[0], "pll_status", 10)) {
		int vpll_lock;

		retrieve_vpll_carrier_lock(&vpll_lock);
		if ((vpll_lock & 0x1) == 0)
			pr_info("visual carrier lock:locked\n");
		else
			pr_info("visual carrier lock:unlocked\n");
	} else if (!strncmp(parm[0], "line_lock", 9)) {
		int line_lock;

		retrieve_vpll_carrier_line_lock(&line_lock);
		if (line_lock == 0)
			pr_info("line lock:locked\n");
		else
			pr_info("line lock:unlocked\n");
	} else if (!strncmp(parm[0], "audio_power", 11)) {
		unsigned int audio_power = 0;

		retrieve_vpll_carrier_audio_power(&audio_power, 1);
		pr_info("audio_power: %d.\n", audio_power);
	} else if (!strncmp(parm[0], "adc_power", 9)) {
		int adc_power = 0;

		retrieve_adc_power(&adc_power);
		pr_info("adc_power:%d\n", adc_power);
	} else if (!strncmp(parm[0], "mode_set", 8)) {
		int priv_cfg = AML_ATVDEMOD_INIT;
		struct dvb_frontend *fe = NULL;

		fe = &dev->v4l2_fe.fe;

		if (parm[1] && kstrtoul(parm[1], 10, &tmp) == 0)
			priv_cfg = tmp;

		fe->ops.analog_ops.set_config(fe, &priv_cfg);

		pr_info("mode_set mode %d\n", priv_cfg);
	} else if (!strncmp(parm[0], "params_set", 8)) {
		struct dvb_frontend *fe = NULL;
		struct analog_parameters params;
		struct v4l2_analog_parameters *p = NULL;
		unsigned int std = 0;
		unsigned int freq = 0;

		fe = &dev->v4l2_fe.fe;
		p = &dev->v4l2_fe.params;

		if (parm[1] && kstrtoul(parm[1], 0, &tmp) == 0)
			std = tmp;
		else
			std = p->std;

		if (parm[2] && kstrtoul(parm[2], 0, &tmp) == 0)
			freq = tmp;
		else
			freq = p->frequency;

		params.frequency = freq;
		params.mode = p->afc_range;
		params.audmode = p->audmode;
		params.std = std;

		fe->ops.analog_ops.set_params(fe, &params);

		pr_info("params_set std 0x%x\n", std);
	} else if (!strncmp(parm[0], "audio_set", 9)) {
		int std = AUDIO_STANDARD_A2_K;

		if (parm[1] && kstrtoul(parm[1], 10, &tmp) == 0)
			std = tmp;

		configure_adec(std);
		adec_soft_reset();

		pr_info("audio_set std %d\n", std);
	} else if (!strncmp(parm[0], "atvdemod_status", 15)) {
		struct v4l2_analog_parameters *p = &dev->v4l2_fe.params;
		int vpll_lock = 0;
		int line_lock = 0;

		if (priv->state == ATVDEMOD_STATE_WORK) {
			retrieve_vpll_carrier_lock(&vpll_lock);
			pr_info("vpp lock: %s.\n",
				vpll_lock == 0 ? "Locked" : "Unlocked");

			retrieve_vpll_carrier_line_lock(&line_lock);
			pr_info("line lock: %s.\n",
				line_lock == 0 ? "Locked" : "Unlocked");

			data_afc = retrieve_vpll_carrier_afc();
			pr_info("afc: %d Khz.\n", data_afc);

			data_snr_avg = atvdemod_get_snr_val();
			pr_info("snr: %d.\n", data_snr_avg);
		}

		pr_info("[params] afc_range: %d\n", p->afc_range);
		pr_info("[params] frequency: %d\n", p->frequency);
		pr_info("[params] soundsys: %d\n", p->soundsys);
		pr_info("[params] std: 0x%x (%s %s)\n",
				(unsigned int) dev->std,
				v4l2_std_to_str((0xff000000 & dev->std)),
				v4l2_std_to_str((0xffffff & dev->std)));
		pr_info("[params] audmode: 0x%x\n", dev->audmode);
		pr_info("[params] flag: %d\n", p->flag);
		pr_info("[params] tuner_cur: %d\n", dev->tuner_cur);
		pr_info("[params] tuner_id: %d\n",
				dev->tuners[dev->tuner_cur].cfg.id);
		pr_info("[params] if_freq: %d\n", dev->if_freq);
		pr_info("[params] if_inv: %d\n", dev->if_inv);
		pr_info("[params] fre_offset: %d\n", dev->fre_offset);
		pr_info("version: %s.\n", AMLATVDEMOD_VER);
	} else if (!strncmp(parm[0], "attach_tuner", 12)) {
		int tuner_id = 0;

		if (parm[1] && kstrtoul(parm[1], 10, &tmp) == 0) {
			val = tmp;

			for (i = 0; i < dev->tuner_num; ++i) {
				if (dev->tuners[i].cfg.id == val) {
					tuner_id = dev->tuners[i].cfg.id;
					break;
				}
			}

			if (tuner_id == 0 || dev->tuner_cur == i) {
				pr_err("%s: set nonsupport or the same tuner %d.\n",
						__func__, val);
				goto EXIT;
			}

			dev->tuner_cur = i;

			ret = aml_attach_tuner(dev);
			if (ret)
				pr_info("attach_tuner error.\n");
			else
				pr_info("attach_tuner %d done.\n", tuner_id);
		}
	} else if (!strncmp(parm[0], "dump_demod", 10)) {
		int blk = 0, reg = 0;

		for (blk = 0; blk <= APB_BLOCK_ADDR_TOP; ++blk) {
			for (reg = 0; reg < 0x40; ++reg) {
				val = atv_dmd_rd_long(blk, reg);
				pr_err("[0x%04x] = 0x%x.\n",
						(blk << 8) + (reg << 2), val);
			}
		}
	} else if (!strncmp(parm[0], "dump_audemod", 12)) {
		int reg = 0;

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX)) {
			for (reg = 0; reg <= 0x1ff; ++reg) {
				val = adec_rd_reg(reg);
				pr_err("[0x%04x] = 0x%x.\n", (reg << 2), val);
			}
		}
	} else
		pr_info("invalid command\n");

EXIT:
	kfree(buf_orig);

	return count;
}

static ssize_t aml_atvdemod_show(struct class *cls,
		struct class_attribute *attr, char *buff)
{
	pr_dbg("\n usage:\n");
	pr_dbg("[get soft version] echo ver_info > /sys/class/amlatvdemod/atvdemod_debug\n");
	pr_dbg("[get afc value] echo afc_info > /sys/class/amlatvdemod/atvdemod_debug\n");
	pr_dbg("[reinit atvdemod] echo init > /sys/class/amlatvdemod/atvdemod_debug\n");
	pr_dbg("[get av-out-gain/av-out-offset/atv-gain/atv-offset]:\n"
				"echo get av_gain/av_offset/atv_gain/atv_offset > /sys/class/amlatvdemod/atvdemod_debug\n");
	pr_dbg("[set av-out-gain/av-out-offset/atv-gain/atv-offset]:\n"
				"echo set av_gain/av_offset/atv_gain/atv_offset val(0~255) > /sys/class/amlatvdemod/atvdemod_debug\n");
	return 0;
}

static struct class_attribute aml_atvdemod_attrs[] = {
	__ATTR(atvdemod_debug, 0644, aml_atvdemod_show, aml_atvdemod_store),
	__ATTR_NULL
};

static void aml_atvdemod_dt_parse(struct aml_atvdemod_device *pdev)
{
	struct device_node *node = NULL;
	struct device_node *node_tuner = NULL;
	struct device_node *node_i2c = NULL;
	unsigned int val = 0;
	const char *str = NULL;
	char buf[20] = { 0 };
	int ret = 0;
	int i = 0;

	node = pdev->dev->of_node;
	if (node == NULL) {
		pr_err("atv demod node == NULL.\n");
		return;
	}

	ret = of_property_read_u32(node, "reg_23cf", &val);
	if (ret)
		pr_err("can't find reg_23cf.\n");
	else
		pdev->reg_23cf = val;

	ret = of_property_read_u32(node, "audio_gain_val", &val);
	if (ret)
		pr_err("can't find audio_gain_val.\n");
	else
		set_audio_gain_val(val);

	ret = of_property_read_u32(node, "video_gain_val", &val);
	if (ret)
		pr_err("can't find video_gain_val.\n");
	else
		set_video_gain_val(val);

	/* agc pin mux */
	ret = of_property_read_string(node, "pinctrl-names", &pdev->pin_name);
	if (ret) {
		pdev->agc_pin = NULL;
		pr_err("can't find agc pinmux.\n");
	} else {
		pr_err("atvdemod agc pinmux name: %s\n", pdev->pin_name);
	}

	ret = of_property_read_u32(node, "btsc_sap_mode", &val);
	if (ret)
		pr_err("can't find btsc_sap_mode.\n");
	else
		pdev->btsc_sap_mode = val;

	/* get tuner config node */
	node_tuner = of_parse_phandle(node, "tuner", 0);
	if (!node_tuner) {
		pr_err("can't find tuner node.\n");
		return;
	}

	ret = of_property_read_u32(node_tuner, "tuner_num", &val);
	if (ret == 0)
		pdev->tuner_num = val;
	else {
		pr_err("can't find tuner_num.\n");
		return;
	}

	pdev->tuners = kcalloc(pdev->tuner_num, sizeof(struct aml_tuner),
			GFP_KERNEL);
	if (!pdev->tuners) {
		/* pr_err("can't kcalloc for tuners.\n"); */
		return;
	}

	ret = of_property_read_u32(node_tuner, "tuner_cur", &val);
	if (ret) {
		pr_err("can't find tuner_cur, use default 0.\n");
		pdev->tuner_cur = -1;
	} else
		pdev->tuner_cur = val;

	for (i = 0; i < pdev->tuner_num; ++i) {
		snprintf(buf, sizeof(buf), "tuner_name_%d", i);
		ret = of_property_read_string(node_tuner, buf, &str);
		if (ret) {
			pr_err("can't find tuner %d.\n", i);
			continue;
		}

		if (!strncmp(str, "mxl661_tuner", 12))
			pdev->tuners[i].cfg.id = AM_TUNER_MXL661;
		else if (!strncmp(str, "si2151_tuner", 12))
			pdev->tuners[i].cfg.id = AM_TUNER_SI2151;
		else if (!strncmp(str, "si2159_tuner", 12))
			pdev->tuners[i].cfg.id = AM_TUNER_SI2159;
		else if (!strncmp(str, "r840_tuner", 10))
			pdev->tuners[i].cfg.id = AM_TUNER_R840;
		else if (!strncmp(str, "r842_tuner", 10))
			pdev->tuners[i].cfg.id = AM_TUNER_R842;
		else if (!strncmp(str, "atbm2040_tuner", 14))
			pdev->tuners[i].cfg.id = AM_TUNER_ATBM2040;
		else {
			pr_err("can't support tuner: %s.\n", str);
			pdev->tuners[i].cfg.id = AM_TUNER_NONE;
		}

		snprintf(buf, sizeof(buf), "tuner_i2c_adap_%d", i);
		node_i2c = of_parse_phandle(node_tuner, buf, 0);
		if (node_i2c) {
			pdev->tuners[i].i2c_adp =
					of_find_i2c_adapter_by_node(node_i2c);
			of_node_put(node_i2c);

			if (!pdev->tuners[i].i2c_adp)
				pr_err("can't find tuner_i2c_adap.\n");
		}

		snprintf(buf, sizeof(buf), "tuner_i2c_addr_%d", i);
		ret = of_property_read_u32(node_tuner, buf, &val);
		if (ret)
			pr_err("can't find tuner_i2c_addr.\n");
		else
			pdev->tuners[i].cfg.i2c_addr = val;

		snprintf(buf, sizeof(buf), "tuner_xtal_%d", i);
		ret = of_property_read_u32(node_tuner, buf, &val);
		if (ret)
			pr_err("can't find tuner_xtal.\n");
		else
			pdev->tuners[i].cfg.xtal = val;

		snprintf(buf, sizeof(buf), "tuner_xtal_mode_%d", i);
		ret = of_property_read_u32(node_tuner, buf, &val);
		if (ret)
			pr_err("can't find tuner_xtal_mode.\n");
		else
			pdev->tuners[i].cfg.xtal_mode = val;

		snprintf(buf, sizeof(buf), "tuner_xtal_cap_%d", i);
		ret = of_property_read_u32(node_tuner, buf, &val);
		if (ret)
			pr_err("can't find tuner_xtal_cap.\n");
		else
			pdev->tuners[i].cfg.xtal_cap = val;

		pr_err("find tuner %s [%d].\n", str, pdev->tuners[i].cfg.id);
	}

	of_node_put(node_tuner);
}

int aml_attach_demod(struct aml_atvdemod_device *dev)
{
	void *p = NULL;
	struct v4l2_frontend *v4l2_fe = &dev->v4l2_fe;
	struct dvb_frontend *fe = &v4l2_fe->fe;

	if (dev->tuner_cur < 0) {
		pr_err("%s: dev->tuner_cur [%d] error.\n",
				__func__, dev->tuner_cur);
		return -1;
	}

	p = v4l2_attach(aml_atvdemod_attach, fe, v4l2_fe,
				dev->tuners[dev->tuner_cur].i2c_adp,
				dev->tuners[dev->tuner_cur].cfg.i2c_addr,
				dev->tuners[dev->tuner_cur].cfg.id);
	if (p != NULL)
		dev->analog_attached = true;
	else {
		pr_err("%s: attach demod error.\n", __func__);
		return -1;
	}

	return 0;
}

int aml_attach_tuner(struct aml_atvdemod_device *dev)
{
	void *p = NULL;
	struct v4l2_frontend *v4l2_fe = &dev->v4l2_fe;
	struct dvb_frontend *fe = &v4l2_fe->fe;
	struct atv_demod_priv *priv = fe->analog_demod_priv;
	struct tuner_config *cfg = NULL;

	if (dev->tuner_cur < 0) {
		pr_err("%s: dev->tuner_cur [%d] error.\n",
				__func__, dev->tuner_cur);
		return -1;
	}

	cfg = &dev->tuners[dev->tuner_cur].cfg;

	switch (cfg->id) {
	case AM_TUNER_R840:
		p = v4l2_attach(r840_attach, fe,
				dev->tuners[dev->tuner_cur].i2c_adp, cfg);
		break;
	case AM_TUNER_R842:
		p = v4l2_attach(r842_attach, fe,
				dev->tuners[dev->tuner_cur].i2c_adp, cfg);
		break;
	case AM_TUNER_SI2151:
		p = v4l2_attach(si2151_attach, fe,
				dev->tuners[dev->tuner_cur].i2c_adp, cfg);
		break;
	case AM_TUNER_SI2159:
		p = v4l2_attach(si2159_attach, fe,
				dev->tuners[dev->tuner_cur].i2c_adp, cfg);
		break;
	case AM_TUNER_MXL661:
		p = v4l2_attach(mxl661_attach, fe,
				dev->tuners[dev->tuner_cur].i2c_adp, cfg);
		break;
	case AM_TUNER_ATBM2040:
		p = v4l2_attach(atbm2040_attach, fe,
				dev->tuners[dev->tuner_cur].i2c_adp, cfg);
		break;
	default:
		pr_err("%s: Nonsupport tuner [%d].\n", __func__, cfg->id);
		break;
	}

	if (p != NULL) {
		dev->tuner_attached = true;
		priv->atvdemod_param.tuner_id = cfg->id;
	} else {
		pr_err("%s: attach tuner [%d] error.\n", __func__, cfg->id);
		return -1;
	}

	return 0;
}

static int aml_detach_demod_tuner(struct aml_atvdemod_device *dev)
{
	struct v4l2_frontend *v4l2_fe = &dev->v4l2_fe;

	v4l2_frontend_detach(v4l2_fe);

	dev->analog_attached = false;
	dev->tuner_attached = false;

	return 0;
}

static int aml_atvdemod_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res = NULL;
	int size_io_reg = 0;
	struct aml_atvdemod_device *dev = NULL;

	dev = kzalloc(sizeof(struct aml_atvdemod_device), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	amlatvdemod_devp = dev;

	dev->name = ATVDEMOD_DEVICE_NAME;
	dev->dev = &pdev->dev;
	dev->cls.name = ATVDEMOD_DEVICE_NAME;
	dev->cls.owner = THIS_MODULE;
	dev->cls.class_attrs = aml_atvdemod_attrs;

	if (class_register(&dev->cls)) {
		pr_err("class register fail.\n");
		goto fail_class_register;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev->irq = -1;
		pr_err("can't get irq resource.\n");
	} else {
		dev->irq = res->start;
		pr_err("get irq resource %d.\n", dev->irq);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("no demod memory resource.\n");
		goto fail_get_resource;
	}

	size_io_reg = resource_size(res);
	dev->demod_reg_base = devm_ioremap_nocache(&pdev->dev,
			res->start, size_io_reg);
	if (!dev->demod_reg_base) {
		pr_err("demod ioremap failed.\n");
		goto fail_get_resource;
	}

	pr_info("demod start = 0x%p, size = 0x%x, base = 0x%p.\n",
			(void *) res->start, size_io_reg,
			dev->demod_reg_base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		pr_err("no hiu memory resource.\n");
		dev->hiu_reg_base = NULL;
	} else {
		size_io_reg = resource_size(res);
		dev->hiu_reg_base = devm_ioremap_nocache(
				&pdev->dev, res->start, size_io_reg);
		if (!dev->hiu_reg_base) {
			pr_err("hiu ioremap failed.\n");
			goto fail_get_resource;
		}

		pr_info("hiu start = 0x%p, size = 0x%x, base = 0x%p.\n",
					(void *) res->start, size_io_reg,
					dev->hiu_reg_base);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!res) {
		pr_err("no periphs memory resource.\n");
		dev->periphs_reg_base = NULL;
	} else {
		size_io_reg = resource_size(res);
		dev->periphs_reg_base = devm_ioremap_nocache(
				&pdev->dev, res->start, size_io_reg);
		if (!dev->periphs_reg_base) {
			pr_err("periphs ioremap failed.\n");
			goto fail_get_resource;
		}

		pr_info("periphs start = 0x%p, size = 0x%x, base = 0x%p.\n",
					(void *) res->start, size_io_reg,
					dev->periphs_reg_base);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (!res) {
		pr_err("no audiodemod memory resource.\n");
		dev->audiodemod_reg_base = NULL;
	} else {
		size_io_reg = resource_size(res);
		dev->audiodemod_reg_base = devm_ioremap_nocache(
				&pdev->dev, res->start, size_io_reg);
		if (!dev->audiodemod_reg_base) {
			pr_err("audiodemod ioremap failed.\n");
			goto fail_get_resource;
		}

		pr_info("audiodemod start = 0x%p, size = 0x%x, base = 0x%p.\n",
					(void *) res->start, size_io_reg,
					dev->audiodemod_reg_base);
	}

	/* add for audio system control */
	if (is_meson_txlx_cpu() || is_meson_txhd_cpu()) {
		dev->audio_reg_base = ioremap(round_down(0xffd0d340, 0x3), 4);

		pr_info("audio_reg_base = 0x%p.\n", dev->audio_reg_base);
	} else if (is_meson_tl1_cpu() || is_meson_tm2_cpu()) {
		dev->audio_reg_base = ioremap(round_down(0xff60074c, 0x3), 4);

		pr_info("audio_reg_base = 0x%p.\n", dev->audio_reg_base);
	} else {
		dev->audio_reg_base = NULL;

		pr_info("audio_reg_base = NULL.\n");
	}

	aml_atvdemod_dt_parse(dev);

	aml_attach_demod(dev);
	aml_attach_tuner(dev);

	dev->v4l2_fe.dev = dev->dev;

	ret = v4l2_resister_frontend(&dev->v4l2_fe);
	if (ret < 0) {
		pr_err("resister v4l2 fail.\n");
		goto fail_register_v4l2;
	}

	platform_set_drvdata(pdev, dev);

	pr_info("%s: OK.\n", __func__);

	return 0;

fail_register_v4l2:
fail_get_resource:
	class_unregister(&dev->cls);
fail_class_register:
	kfree(dev->tuners);
	kfree(dev);
	amlatvdemod_devp = NULL;

	pr_info("%s: fail.\n", __func__);

	return ret;
}

static int aml_atvdemod_remove(struct platform_device *pdev)
{
	struct aml_atvdemod_device *dev = platform_get_drvdata(pdev);

	if (dev == NULL)
		return -1;

	v4l2_unresister_frontend(&dev->v4l2_fe);
	aml_detach_demod_tuner(dev);

	class_unregister(&dev->cls);

	amlatvdemod_devp = NULL;

	kfree(dev->tuners);
	kfree(dev);

	pr_info("%s: OK.\n", __func__);

	return 0;
}

static void aml_atvdemod_shutdown(struct platform_device *pdev)
{
	struct aml_atvdemod_device *dev = platform_get_drvdata(pdev);

	v4l2_frontend_shutdown(&dev->v4l2_fe);
	adc_pll_down();

	pr_info("%s: OK.\n", __func__);
}

static int aml_atvdemod_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	struct aml_atvdemod_device *dev = platform_get_drvdata(pdev);

	v4l2_frontend_suspend(&dev->v4l2_fe);

	pr_info("%s: OK.\n", __func__);

	return 0;
}

int aml_atvdemod_resume(struct platform_device *pdev)
{
	struct aml_atvdemod_device *dev = platform_get_drvdata(pdev);

	v4l2_frontend_resume(&dev->v4l2_fe);

	pr_info("%s: OK.\n", __func__);

	return 0;
}

static const struct of_device_id aml_atvdemod_dt_match[] = {
	{
		.compatible = "amlogic, atv-demod",
	},
	{
	},
};

static struct platform_driver aml_atvdemod_driver = {
	.driver = {
		.name = ATVDEMOD_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = aml_atvdemod_dt_match,
	},
	.probe    = aml_atvdemod_probe,
	.remove   = aml_atvdemod_remove,
	.shutdown = aml_atvdemod_shutdown,
	.suspend  = aml_atvdemod_suspend,
	.resume   = aml_atvdemod_resume,
};

static int __init aml_atvdemod_init(void)
{
	int ret = 0;

	ret = aml_atvdemod_create_debugfs(ATVDEMOD_DRIVER_NAME);
	if (ret < 0) {
		pr_err("%s: failed to create debugfs.\n", __func__);
		return ret;
	}

	ret = platform_driver_register(&aml_atvdemod_driver);
	if (ret < 0) {
		pr_err("%s: failed to register driver.\n", __func__);
		aml_atvdemod_remove_debugfs();
		return ret;
	}

	pr_info("%s: OK, atv demod version: %s.\n", __func__, AMLATVDEMOD_VER);

	return 0;
}

static void __exit aml_atvdemod_exit(void)
{
	platform_driver_unregister(&aml_atvdemod_driver);
	aml_atvdemod_remove_debugfs();

	pr_info("%s: OK.\n", __func__);
}

MODULE_AUTHOR("nengwen.chen <nengwen.chen@amlogic.com>");
MODULE_DESCRIPTION("aml atv demod device driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(AMLATVDEMOD_VER);

module_init(aml_atvdemod_init);
module_exit(aml_atvdemod_exit);
