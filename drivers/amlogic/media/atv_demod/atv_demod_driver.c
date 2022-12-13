// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
#include <linux/amlogic/media/vout/vdac_dev.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include <linux/amlogic/aml_atvdemod.h>
#include <linux/amlogic/aml_tuner.h>
#include <linux/amlogic/aml_dvb_extern.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <dvb_frontend.h>

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
/* 2019/11/25 --- V2.16 --- Add pal-m/n ring filter processing. */
/* 2019/12/31 --- V2.17 --- Fix atv vif setting and offset error. */
/* 2020/01/06 --- V2.18 --- Optimize MTS(Multi-channel sound) control flow. */
/* 2020/01/08 --- V2.19 --- Fix afc frequency offset. */
/* 2020/03/24 --- V2.20 --- Fix ntsc(443)-bg/dk/i no audio output. */
/* 2020/04/23 --- V2.21 --- Fix pal-n config. */
/*                          Fix r842 stable delay when scanning. */
/*                          Fix visual carrier if amp when playing. */
/* 2020/04/30 --- V2.22 --- Add demod power control in suspend and resume. */
/* 2020/06/30 --- V2.23 --- Optimize demod init and uninit. */
/* 2020/08/18 --- V2.24 --- Fix audio parameter configuration when searching. */
/* 2020/09/09 --- V2.25 --- Fix asynchronous exit of the tuning thread */
/*                          to prevent the call from blocking too long. */
/* 2020/09/24 --- V2.26 --- Bringup t5 */
/* 2020/11/03 --- V2.27 --- Bringup t5d. */
/* 2020/11/23 --- V2.28 --- Adapter multi tuner switch. */
/* 2021/04/15 --- V2.29 --- Bringup t3. */
/* 2021/05/12 --- V2.30 --- Fix adc filter enable/disable in tvafe and demod. */
/*                          Fix atbm253/2040 stable delay when scanning. */
/*                          Fix pal-dk and add pal-bg/i audio overmodulation. */
/* 2021/11/05 --- V2.31 --- Bringup t5w. */
/* 2021/12/24 --- V2.32 --- Fix shutdown. */
/* 2021/12/29 --- V2.33 --- Fix unable to find symbol aml_atvdemod_attach. */
/* 2022/06/16 --- V2.34 --- Fix audio setting and resume. */
/* 2022/08/27 --- V2.35 --- Fix ripples. */
#define AMLATVDEMOD_VER "V2.35"

struct aml_atvdemod_device *amlatvdemod_devp;

unsigned int audio_gain_shift;
unsigned int audio_gain_lpr;

static ssize_t atvdemod_debug_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int n = 0;
	int ret = 0;
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
		int priv_cfg = AML_ATVDEMOD_INIT;
		struct dvb_frontend *fe = &dev->v4l2_fe.fe;

		if (parm[1] && kstrtoul(parm[1], 10, &tmp) == 0) {
			if (tmp)
				priv_cfg = AML_ATVDEMOD_INIT;
			else
				priv_cfg = AML_ATVDEMOD_UNINIT;
		}

		if (fe->ops.analog_ops.set_config)
			fe->ops.analog_ops.set_config(fe, &priv_cfg);

		pr_info("%s.\n", priv_cfg ? "init" : "uninit");
	} else if (!strncmp(parm[0], "reinit", 6)) {
		ret = atv_demod_enter_mode(&dev->v4l2_fe.fe);
		if (ret)
			pr_info("atv init error.\n");
	} else if (!strncmp(parm[0], "audout_mode", 11)) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX)) {
			atvauddemod_set_outputmode();
			pr_info("atvauddemod_set_outputmode done ....\n");
		}
	} else if (!strncmp(parm[0], "signal_audmode", 14)) {
		int stereo_flag = 0, sap_flag = 0;

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX)) {
			update_btsc_mode(1, &stereo_flag, &sap_flag);
			pr_info("get signal_audmode done ....\n");
		}
	} else if (!strncmp(parm[0], "adc_pll", 7)) {
#ifdef CONFIG_AMLOGIC_MEDIA_ADC
		if (kstrtoul(parm[1], 16, &tmp) == 0)
			val = tmp;
		else
			val = 1;

		adc_set_pll_cntl(!!val, ADC_ATV_DEMOD, NULL);
		adc_set_filter_ctrl(!!val, FILTER_ATV_DEMOD, NULL);
#endif
		pr_info("adc_pll %s done ....\n", val ? "ON" : "OFF");
	} else if (!strncmp(parm[0], "vdac", 4)) {
#ifdef CONFIG_AMLOGIC_VDAC
		if (kstrtoul(parm[1], 16, &tmp) == 0)
			val = tmp;
		else
			val = 1;

		vdac_enable(!!val, VDAC_MODULE_AVOUT_ATV);
#endif
		pr_info("vdac %s done ....\n", val ? "ON" : "OFF");
	} else if (!strncmp(parm[0], "clock", 5)) {
		atvdemod_clk_init();
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX))
			aud_demod_clk_gate(1);
		pr_info("atvdemod_clk_init done ....\n");
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
	} else if (!strncmp(parm[0], "audio_autodet", 13)) {
		aml_audiomode_autodet(&dev->v4l2_fe);
	} else if (!strncmp(parm[0], "audio_gain_set", 14)) {
		if (kstrtoul(buf + strlen("audio_gain_set") + 1, 16, &tmp) == 0)
			val = tmp;
		aml_audio_volume_gain_set(val);
		pr_info("audio_gain_set : %d\n", val);
	} else if (!strncmp(parm[0], "audio_gain_get", 14)) {
		val = aml_audio_volume_gain_get();
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
	} else if (!strncmp(parm[0], "tune", 4)) {
		struct dvb_frontend *fe = NULL;
		struct analog_parameters params;
		struct v4l2_analog_parameters *p = NULL;

		fe = &dev->v4l2_fe.fe;
		p = &dev->v4l2_fe.params;

		if (!strncmp(parm[1], "pal", 3)) {
			p->std = V4L2_COLOR_STD_PAL;
		} else if (!strncmp(parm[1], "ntsc", 4)) {
			p->std = V4L2_COLOR_STD_NTSC;
		} else if (!strncmp(parm[1], "secam", 5)) {
			p->std = V4L2_COLOR_STD_SECAM;
		} else {
			pr_err("Invalid video std, force to pal.\n");
			p->std = V4L2_COLOR_STD_PAL;
		}

		if (!strncmp(parm[2], "dk", 2)) {
			p->audmode = V4L2_STD_PAL_DK;
			p->std |= V4L2_STD_PAL_I;
		} else if (!strncmp(parm[2], "bg", 2)) {
			p->audmode = V4L2_STD_BG;
			p->std |= V4L2_STD_PAL_I;
		} else if (!strncmp(parm[2], "i", 1)) {
			p->audmode = V4L2_STD_PAL_I;
			p->std |= V4L2_STD_PAL_I;
		} else if (!strncmp(parm[2], "nm", 2)) {
			p->audmode = V4L2_STD_NTSC_M;
			p->std |= V4L2_STD_NTSC_M;
		} else if (!strncmp(parm[2], "pm", 2)) {
			p->audmode = V4L2_STD_PAL_M;
			p->std |= V4L2_STD_PAL_M;
		} else if (!strncmp(parm[2], "lc", 2)) {
			p->audmode = V4L2_STD_SECAM_LC;
			p->std |= V4L2_STD_SECAM_LC;
		} else if (!strncmp(parm[2], "l", 1)) {
			p->audmode = V4L2_STD_SECAM_L;
			p->std |= V4L2_STD_SECAM_L;
		} else if (!strncmp(parm[2], "n", 1)) {
			p->audmode = V4L2_STD_PAL_M;
			p->std |= V4L2_STD_PAL_N;
		} else {
			pr_err("Invalid audio std, force to i.\n");
			p->audmode = V4L2_STD_PAL_I;
			p->std |= V4L2_STD_PAL_I;
		}

		if (parm[3] && kstrtoul(parm[3], 0, &tmp) == 0) {
			p->frequency = tmp;
		} else {
			pr_err("Invalid frequency, force to 144250000 Hz.\n");
			p->frequency = 144250000;
		}

		p->afc_range = 0;

		params.frequency = p->frequency;
		params.mode = p->afc_range;
		params.audmode = p->audmode;
		params.std = p->std;

		if (fe->ops.analog_ops.set_params)
			fe->ops.analog_ops.set_params(fe, &params);

		pr_info("tune std color %s, audio %s, cvbs %s, freq %d Hz.\n",
				v4l2_std_to_str((0xff000000 & p->std)),
				v4l2_std_to_str((0xffffff & p->audmode)),
				v4l2_std_to_str((0xffffff & p->std)), p->frequency);
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
		int adc_power = 0;
		unsigned int audio_power = 0;

		if (priv && priv->state == ATVDEMOD_STATE_WORK) {
			retrieve_vpll_carrier_lock(&vpll_lock);
			pr_info("vpp lock: %s.\n",
				vpll_lock == 0 ? "Locked" : "Unlocked");

			retrieve_vpll_carrier_line_lock(&line_lock);
			pr_info("line lock: %s (0x%x).\n",
				line_lock == 0 ? "Locked" : "Unlocked",
				line_lock);

			data_afc = retrieve_vpll_carrier_afc();
			pr_info("afc: %d Khz.\n", data_afc);

			data_snr_avg = atvdemod_get_snr_val();
			pr_info("snr: %d.\n", data_snr_avg);

			retrieve_vpll_carrier_audio_power(&audio_power, 1);
			pr_info("audio_power: %d.\n", audio_power);

			retrieve_adc_power(&adc_power);
			pr_info("adc_power: %d.\n", adc_power);
		} else {
			pr_info("Not yet initialized.\n");
		}

		pr_info("[params] afc_range: %d\n", p->afc_range);
		pr_info("[params] frequency: %d Hz\n", p->frequency);
		pr_info("[params] soundsys: %d\n", p->soundsys);
		pr_info("[params] std: 0x%x (%s, %s)\n",
				(unsigned int) dev->std,
				v4l2_std_to_str((0xff000000 & dev->std)),
				v4l2_std_to_str((0xffffff & dev->std)));
		pr_info("[params] audmode: 0x%x (%s)\n", dev->audmode,
				v4l2_std_to_str(0xffffff & dev->audmode));
		pr_info("[params] flag: %d\n", p->flag);
		pr_info("[params] tuner_id: %d\n", dev->tuner_id);
		pr_info("[params] if_freq: %d\n", dev->if_freq);
		pr_info("[params] if_inv: %d\n", dev->if_inv);
		pr_info("[params] fre_offset: %d\n", dev->fre_offset);
		pr_info("version: %s.\n", AMLATVDEMOD_VER);
	} else if (!strncmp(parm[0], "attach", 6)) {
		if (!dev->analog_attached) {
			ret = aml_atvdemod_attach_demod(dev);
			if (ret < 0)
				pr_err("attach_demod error ret %d.\n", ret);
		} else {
			pr_err("attach_demod already done.\n");
		}

		if (!dev->tuner_attached) {
			ret = aml_atvdemod_attach_tuner(dev);
			if (ret < 0)
				pr_err("attach_tuner error ret %d.\n", ret);
		} else {
			pr_err("attach_tuner already done.\n");
		}
	} else if (!strncmp(parm[0], "dump_demod", 10)) {
		int blk = 0, reg = 0;

		for (blk = 0; blk <= APB_BLOCK_ADDR_TOP; ++blk) {
			for (reg = 0; reg < 0x40; ++reg) {
				val = atv_dmd_rd_long(blk, reg << 2);
				pr_err("[0x%04x] = 0x%X\n",
						(blk << 8) + (reg << 2), val);
			}
		}
	} else if (!strncmp(parm[0], "dump_audemod", 12)) {
		int reg = 0;

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX)) {
			for (reg = 0; reg <= 0x1ff; ++reg) {
				val = adec_rd_reg(reg);
				pr_err("[0x%04x] = 0x%X\n", reg << 2, val);
			}
		}
	} else {
		pr_info("invalid command\n");
	}

EXIT:
	kfree(buf_orig);

	return count;
}

static ssize_t atvdemod_debug_show(struct class *class,
		struct class_attribute *attr, char *buff)
{
	struct aml_atvdemod_device *dev =
			container_of(class, struct aml_atvdemod_device, cls);
	struct atv_demod_priv *priv = dev->v4l2_fe.fe.analog_demod_priv;
	struct v4l2_analog_parameters *p = &dev->v4l2_fe.params;
	int vpll_lock = 0;
	int line_lock = 0;
	int adc_power = 0;
	unsigned int audio_power = 0;
	int data = 0;
	int len = 0;
	const char *patch = "/sys/class/aml_atvdemod/atvdemod_debug";

	len += sprintf(buff + len, "\nATV Demod Usage:\n");
	len += sprintf(buff + len, "[atvdemod_status]\n\techo atvdemod_status > %s\n", patch);
	len += sprintf(buff + len, "[init/uninit atvdemod]\n\techo init/uninit > %s\n", patch);
	len += sprintf(buff + len, "[reinit atvdemod]\n\techo reinit > %s\n", patch);
	len += sprintf(buff + len, "[tune]\n\techo tune vstd astd freq > %s\n", patch);
	len += sprintf(buff + len, "[adc_pll]\n\techo adc_pll on_off > %s\n", patch);
	len += sprintf(buff + len, "[vdac\n\techo vdac on_off > %s\n", patch);
	len += sprintf(buff + len, "[attach\n\techo attach > %s\n", patch);
	len += sprintf(buff + len, "[dump_demod\n\techo dump_demod > %s\n", patch);
	len += sprintf(buff + len, "[dump_audemod\n\techo dump_audemod > %s\n", patch);

	len += sprintf(buff + len, "ATV Demod Status:\n");
	if (priv && priv->state == ATVDEMOD_STATE_WORK) {
		retrieve_vpll_carrier_lock(&vpll_lock);
		len += sprintf(buff + len, "atv_demod: vpp lock: %s.\n",
			vpll_lock == 0 ? "Locked" : "Unlocked");

		retrieve_vpll_carrier_line_lock(&line_lock);
		len += sprintf(buff + len, "atv_demod: line lock: %s(0x%x).\n",
			line_lock == 0 ? "Locked" : "Unlocked", line_lock);

		data = retrieve_vpll_carrier_afc();
		len += sprintf(buff + len, "atv_demod: afc: %d Khz.\n", data);

		data = atvdemod_get_snr_val();
		len += sprintf(buff + len, "atv_demod: snr: %d.\n", data);

		retrieve_vpll_carrier_audio_power(&audio_power, 1);
		len += sprintf(buff + len, "atv_demod: audio_power: %d.\n", audio_power);

		retrieve_adc_power(&adc_power);
		len += sprintf(buff + len, "atv_demod: adc_power: %d.\n", adc_power);
	} else {
		len += sprintf(buff + len, "atv_demod: Not yet initialized.\n");
	}

	len += sprintf(buff + len, "atv_demod: [params] afc_range: %d\n",
			p->afc_range);
	len += sprintf(buff + len, "atv_demod: [params] frequency: %d Hz\n",
			p->frequency);
	len += sprintf(buff + len, "atv_demod: [params] soundsys: %d\n",
			p->soundsys);
	len += sprintf(buff + len, "atv_demod: [params] std: 0x%x (%s, %s)\n",
			(unsigned int) dev->std,
			v4l2_std_to_str((0xff000000 & dev->std)),
			v4l2_std_to_str((0xffffff & dev->std)));
	len += sprintf(buff + len, "atv_demod: [params] audmode: 0x%x (%s)\n",
			dev->audmode, v4l2_std_to_str(0xffffff & dev->audmode));
	len += sprintf(buff + len, "atv_demod: [params] flag: %d\n", p->flag);
	len += sprintf(buff + len, "atv_demod: [params] tuner_id: %d\n",
			dev->tuner_id);
	len += sprintf(buff + len, "atv_demod: [params] if_freq: %d\n",
			dev->if_freq);
	len += sprintf(buff + len, "atv_demod: [params] if_inv: %d\n",
			dev->if_inv);
	len += sprintf(buff + len, "atv_demod: [params] fre_offset: %d\n",
			dev->fre_offset);
	len += sprintf(buff + len, "atv_demod: version: %s.\n",
			AMLATVDEMOD_VER);

	return len;
}

static CLASS_ATTR_RW(atvdemod_debug);

static struct attribute *atvdemod_class_attrs[] = {
	&class_attr_atvdemod_debug.attr,
	NULL,
};

ATTRIBUTE_GROUPS(atvdemod_class);

static void aml_atvdemod_dt_parse(struct aml_atvdemod_device *pdev)
{
	struct device_node *node = NULL;
	unsigned int val = 0;
	int ret = 0;

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
}

int aml_atvdemod_attach_demod(struct aml_atvdemod_device *dev)
{
	void *p = NULL;
	struct v4l2_frontend *v4l2_fe = &dev->v4l2_fe;
	struct dvb_frontend *fe = &v4l2_fe->fe;

	p = aml_atvdemod_attach(fe, v4l2_fe, &dev->i2c_adap, dev->i2c_addr, dev->tuner_id);
	if (p) {
		dev->analog_attached = true;
	} else {
		pr_err("%s: attach demod error.\n", __func__);
		return -1;
	}

	return 0;
}

int aml_atvdemod_attach_tuner(struct aml_atvdemod_device *dev)
{
	void *p = NULL;
	struct v4l2_frontend *v4l2_fe = &dev->v4l2_fe;
	struct dvb_frontend *fe = &v4l2_fe->fe;
	struct atv_demod_priv *priv = fe->analog_demod_priv;
	int tuner_id = 0;

	p = dvb_tuner_attach(fe);
	if (p != NULL) {
		dev->tuner_attached = true;
		tuner_id = aml_get_tuner_type(fe->ops.tuner_ops.info.name);
		if (tuner_id != AM_TUNER_NONE) {
			dev->tuner_id = tuner_id;
			priv->atvdemod_param.tuner_id = tuner_id;
		} else {
			dev->tuner_id = AM_TUNER_NONE;
			priv->atvdemod_param.tuner_id = AM_TUNER_NONE;

			pr_err("%s: get tuner type error.\n", __func__);

			return -1;
		}
	} else {
		pr_err("%s: attach tuner error.\n", __func__);
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
	dev->cls.class_groups = atvdemod_class_groups;

	ret = class_register(&dev->cls);
	if (ret) {
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
		ret = -ENXIO;
		pr_err("no demod memory resource.\n");
		goto fail_get_resource;
	}

	size_io_reg = resource_size(res);
	dev->demod_reg_base = devm_ioremap_nocache(&pdev->dev,
			res->start, size_io_reg);
	if (!dev->demod_reg_base) {
		ret = -ENXIO;
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
			ret = -ENXIO;
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
			/* ret = -ENXIO; */
			pr_err("periphs ioremap failed.\n");
			/* goto fail_get_resource; */
		}

		pr_info("periphs start = 0x%p, size = 0x%x, base = 0x%p.\n",
					(void *) res->start, size_io_reg,
					dev->periphs_reg_base);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (!res) {
		pr_err("no audio demod memory resource.\n");
		dev->audiodemod_reg_base = NULL;
	} else {
		size_io_reg = resource_size(res);
		dev->audiodemod_reg_base = devm_ioremap_nocache(
				&pdev->dev, res->start, size_io_reg);
		if (!dev->audiodemod_reg_base) {
			ret = -ENXIO;
			pr_err("audiodemod ioremap failed.\n");
			goto fail_get_resource;
		}

		pr_info("audiodemod start = 0x%p, size = 0x%x, base = 0x%p.\n",
					(void *) res->start, size_io_reg,
					dev->audiodemod_reg_base);
	}

	/* add for audio system control */
	if (is_meson_txlx_cpu() || is_meson_txhd_cpu()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		dev->audio_reg_base = ioremap(round_down(0xffd0d340, 0x3), 4);

		pr_info("audio_reg_base = 0x%p.\n", dev->audio_reg_base);
#endif
	} else if (is_meson_t3_cpu()) {
		dev->audio_reg_base = ioremap(round_down(0xfe33074c, 0x3), 4);

		pr_info("audio_reg_base = 0x%p.\n", dev->audio_reg_base);
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		dev->audio_reg_base = ioremap(round_down(0xff60074c, 0x3), 4);

		pr_info("audio_reg_base = 0x%p.\n", dev->audio_reg_base);
	} else {
		dev->audio_reg_base = NULL;

		pr_info("audio_reg_base = NULL.\n");
	}

	aml_atvdemod_dt_parse(dev);

	/* aml_atvdemod_attach_demod(dev); */
	/* aml_atvdemod_attach_tuner(dev); */

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

	v4l2_unregister_frontend(&dev->v4l2_fe);
	aml_detach_demod_tuner(dev);

	class_unregister(&dev->cls);

	amlatvdemod_devp = NULL;

	kfree(dev);

	pr_info("%s: OK.\n", __func__);

	return 0;
}

static void aml_atvdemod_shutdown(struct platform_device *pdev)
{
	struct aml_atvdemod_device *dev = platform_get_drvdata(pdev);

	v4l2_frontend_shutdown(&dev->v4l2_fe);

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

int __init aml_atvdemod_init(void)
{
	int ret = 0;

	ret = aml_atvdemod_create_debugfs(ATVDEMOD_DRIVER_NAME);
	if (ret) {
		pr_err("%s: failed to create debugfs, ret %d.\n",
				__func__, ret);
		return ret;
	}

	ret = platform_driver_register(&aml_atvdemod_driver);
	if (ret) {
		pr_err("%s: failed to register driver, ret %d.\n",
				__func__, ret);
		aml_atvdemod_remove_debugfs();
		return ret;
	}

	pr_info("%s: OK, atv demod version: %s.\n", __func__, AMLATVDEMOD_VER);

	return 0;
}

void __exit aml_atvdemod_exit(void)
{
	platform_driver_unregister(&aml_atvdemod_driver);
	aml_atvdemod_remove_debugfs();

	pr_info("%s: OK.\n", __func__);
}

//#ifndef MODULE
//MODULE_AUTHOR("nengwen.chen <nengwen.chen@amlogic.com>");
//MODULE_DESCRIPTION("aml atv demod device driver");
//MODULE_LICENSE("GPL");
//MODULE_VERSION(AMLATVDEMOD_VER);

//module_init(aml_atvdemod_init);
//module_exit(aml_atvdemod_exit);
//#endif
