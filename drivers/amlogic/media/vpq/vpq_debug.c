// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "vpq_debug.h"
#include "vpq_printk.h"
#include "vpq_table_logic.h"
#include "vpq_vfm.h"

unsigned int log_lev;

static const char *vpq_debug_usage_str = {
	"Description:\n"
	" vpq module support bellow debug cmd:\n"
	" 1. get|set log level\n"
	"    --> cat /sys/class/vpq/log_lev\n"
	"    --> echo log_lev xx > /sys/class/vpq/log_lev\n"
	"\n"
	" 2. get all pq modules config, ex: vajd1/gamma/di/nr... enable/disable\n"
	"    --> cat /sys/class/vpq/module_cfg\n"
	"\n"
	" 3. get|set current aml_vpp driver pq modules status, ex: dnlp/lc/... enable/disable\n"
	"    --> cat /sys/class/vpq/module_status\n"
	"    --> echo xx yy > /sys/class/vpq/module_status\n"
	"\n"
	" 4. get current input source signal infor, ex: src/h&w/rate/hdr/...\n"
	"    --> cat /sys/class/vpq/src_infor\n"
	"\n"
	" 5. get his information\n"
	"    --> cat /sys/class/vpq/his_avg\n"
	"\n"
	" 6. set each pq item value of current picture mode, ex: brightness -255 ~ 255\n"
	"    --> echo xx yy > /sys/class/vpq/picture_mode\n"
	"\n"
	" 7. set other pq items value that not in picture mode, ex: csc type\n"
	"    --> echo xx yy > /sys/class/vpq/others\n"
	"\n"
	" 8. dump table that sent to aml_vpp, ex: dump lc curve and reg table\n"
	"    --> echo xx > /sys/class/vpq/dump_table\n"
	"\n"
};

static const char *vpq_log_lev_usage_str = {
	"Usage:\n"
	"echo log_lev value > /sys/class/vpq/log_lev\n"
	"--> value: 1(ioctl); 2(table); 3(module); 4(vfm); 5(proc);\n"
	"\n"
};

static const char *vpq_module_status_usage_str = {
	"Usage:\n"
	"echo pq_module value1 value2 > /sys/class/vpq/module_status\n"
	"--> value1: vadj1; vadj2; pregamma; gamma; wb; dnlp; ccoring;\n"
	"            sr0; sr1; lc; cm; ble; bls; lut3d; dejaggy_sr0;\n"
	"            dejaggy_sr1; dering_sr0; dering_sr1; all;\n"
	"--> value2: 0~1\n"
	"\n"
};

static const char *vpq_picture_mode_usage_str = {
	"Usage:\n"
	"echo bri value > /sys/class/vpq/picture_mode\n"
	"--> value: -512 ~ +512\n"
	"\n"
	"echo con value > /sys/class/vpq/picture_mode\n"
	"--> value: -1023 ~ +1023\n"
	"\n"
	"echo sat value > /sys/class/vpq/picture_mode\n"
	"--> value: -127 ~ +127\n"
	"\n"
	"echo hue value > /sys/class/vpq/picture_mode\n"
	"--> value: -127 ~ +127\n"
	"\n"
	"echo sharp value > /sys/class/vpq/picture_mode\n"
	"--> value: 0 ~ 255\n"
	"\n"
	"echo bri_post value > /sys/class/vpq/picture_mode\n"
	"--> value: -512 ~ +512\n"
	"\n"
	"echo cont_post value > /sys/class/vpq/picture_mode\n"
	"--> value: -1023 ~ +1023\n"
	"\n"
	"echo sat_post value > /sys/class/vpq/picture_mode\n"
	"--> value: -127 ~ +127\n"
	"\n"
	"echo hue_post value > /sys/class/vpq/picture_mode\n"
	"--> value: -127 ~ +127\n"
	"\n"
	"echo dnlp value > /sys/class/vpq/picture_mode\n"
	"--> value: 0 ~ 3\n"
	"\n"
	"echo lc value > /sys/class/vpq/picture_mode\n"
	"--> value: 0 ~ 3\n"
	"\n"
	"echo blkext value > /sys/class/vpq/picture_mode\n"
	"--> value: 0 ~ 3\n"
	"\n"
	"echo rgb_ogo value1...value10 > /sys/class/vpq/picture_mode\n"
	"--> value1~value10: -1024~1023(pre_offset); 0~2047(gain); -1024~1023(post_offset)\n"
	"\n"
	"echo cms value1...value5 > /sys/class/vpq/picture_mode\n"
	"--> value1~value5: 0~1(color_type); 0~8(color_9); 0(color_14); 0~2(cms_type); -128~127(value)\n"
	"\n"
	"echo blus value > /sys/class/vpq/picture_mode\n"
	"--> value: 0 ~ 3\n"
	"\n"
	"echo aipq value > /sys/class/vpq/picture_mode\n"
	"--> value: 0 ~ 3\n"
	"\n"
	"echo aisr value > /sys/class/vpq/picture_mode\n"
	"--> value: 0 ~ 3\n"
	"\n"
};

static const char *vpq_others_usage_str = {
	"Usage:\n"
	"echo csc_type value > /sys/class/vpq/others\n"
	"--> value: 0 ~ 3\n"
	"\n"
	"echo ccorring value > /sys/class/vpq/others\n"
	"--> value: 0 ~ 3\n"
	"\n"
};

static const char *vpq_dump_table_usage_str = {
	"Usage:\n"
	"echo dump_data value > /sys/class/vpq/dump_table\n"
	"--> value: 0(vadj1); 1(gamma); 2(wb); 3(dnlp); 4(lc); 5(cm); 6(ble);\n"
	"           7(bls); 8(tmo); 9(aipq); 10(aisr); 11(ccoring);\n"
	"\n"
};

static void parse_param(char *pbuf, char **param)
{
	char *ps, *token;
	unsigned int n = 0;
	char delim1[3] = " ";
	char delim2[2] = "\n";

	ps = pbuf;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		param[n++] = token;
	}
}

static int atoi(const char *str)
{
	int n = 0;

	while (*str != '\0') {
		n = n * 10 + (*str - '0');
		str++;
	}

	return n;
}

ssize_t vpq_debug_cmd_show(struct class *class,
			struct class_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%s\n", vpq_debug_usage_str);
}

ssize_t vpq_debug_cmd_store(struct class *class,
			struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	return count;
}

ssize_t vpq_log_lev_show(struct class *class,
			struct class_attribute *attr,
			char *buf)
{
	VPQ_PR_DRV("show log_lev: %d\n", log_lev);
	VPQ_PR_DRV("\n");

	return sprintf(buf, "%s\n", vpq_log_lev_usage_str);
}

ssize_t vpq_log_lev_store(struct class *class,
			struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	char *buf_org;
	char *param[8];

	buf_org = kstrdup(buf, GFP_KERNEL);
	if (!buf_org)
		return -ENOMEM;

	parse_param(buf_org, param);

	if (!strcmp(param[0], "log_lev")) {
		if (kstrtouint(param[1], 10, &log_lev) < 0)
			goto free_buf;

		VPQ_PR_DRV("%s log_lev %d\n", __func__, log_lev);
	} else {
		VPQ_PR_DRV("%s cmd error\n", __func__);
		goto free_buf;
	}

free_buf:
	kfree(buf_org);

	return count;
}

ssize_t vpq_picture_mode_item_show(struct class *class,
			struct class_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%s\n", vpq_picture_mode_usage_str);
}

ssize_t vpq_picture_mode_item_store(struct class *class,
			struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	char *buf_org;
	char *param[8];
	int value = 0;

	buf_org = kstrdup(buf, GFP_KERNEL);
	if (!buf_org)
		return -ENOMEM;

	parse_param(buf_org, param);

	if (!strcmp(param[0], "bri")) {
		if (kstrtoint(param[1], 10, &value) < 0)
			goto free_buf;

		VPQ_PR_DRV("%s brightness %d\n", __func__, value);
		vpq_set_brightness(value);
	} else if (!strcmp(param[0], "con")) {
		if (kstrtoint(param[1], 10, &value) < 0)
			goto free_buf;

		VPQ_PR_DRV("%s contrast %d\n", __func__, value);
		vpq_set_contrast(value);
	} else if (!strcmp(param[0], "sat")) {
		if (kstrtoint(param[1], 10, &value) < 0)
			goto free_buf;

		VPQ_PR_DRV("%s saturation %d\n", __func__, value);
		vpq_set_saturation(value);
	} else if (!strcmp(param[0], "hue")) {
		if (kstrtoint(param[1], 10, &value) < 0)
			goto free_buf;

		VPQ_PR_DRV("%s hue %d\n", __func__, value);
		vpq_set_hue(value);
	} else if (!strcmp(param[0], "sharp")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		VPQ_PR_DRV("%s sharpness %d\n", __func__, value);
		vpq_set_sharpness(value);
	} else if (!strcmp(param[0], "bri_post")) {
		if (kstrtoint(param[1], 10, &value) < 0)
			goto free_buf;

		VPQ_PR_DRV("%s brightness_post %d\n", __func__, value);
		vpq_set_brightness_post(value);
	} else if (!strcmp(param[0], "con_post")) {
		if (kstrtoint(param[1], 10, &value) < 0)
			goto free_buf;

		VPQ_PR_DRV("%s contrast_post %d\n", __func__, value);
		vpq_set_contrast_post(value);
	} else if (!strcmp(param[0], "sat_post")) {
		if (kstrtoint(param[1], 10, &value) < 0)
			goto free_buf;

		VPQ_PR_DRV("%s saturation_post %d\n", __func__, value);
		vpq_set_saturation_post(value);
	} else if (!strcmp(param[0], "hue_post")) {
		if (kstrtoint(param[1], 10, &value) < 0)
			goto free_buf;

		VPQ_PR_DRV("%s hue_post %d\n", __func__, value);
		vpq_set_hue_post(value);
	} else if (!strcmp(param[0], "rgb_ogo")) {
		struct vpq_rgb_ogo_s rgb_ogo;

		memset(&rgb_ogo, 0, sizeof(struct vpq_rgb_ogo_s));

		if (kstrtoint(param[1], 10, &rgb_ogo.r_pre_offset) < 0)
			goto free_buf;
		if (kstrtoint(param[2], 10, &rgb_ogo.g_pre_offset) < 0)
			goto free_buf;
		if (kstrtoint(param[3], 10, &rgb_ogo.b_pre_offset) < 0)
			goto free_buf;
		if (kstrtouint(param[4], 10, &rgb_ogo.r_gain) < 0)
			goto free_buf;
		if (kstrtouint(param[5], 10, &rgb_ogo.g_gain) < 0)
			goto free_buf;
		if (kstrtouint(param[6], 10, &rgb_ogo.b_gain) < 0)
			goto free_buf;
		if (kstrtoint(param[7], 10, &rgb_ogo.r_post_offset) < 0)
			goto free_buf;
		if (kstrtoint(param[8], 10, &rgb_ogo.g_post_offset) < 0)
			goto free_buf;
		if (kstrtoint(param[9], 10, &rgb_ogo.b_post_offset) < 0)
			goto free_buf;

		VPQ_PR_DRV("%s rgb_ogo %d %d %d %d %d %d %d %d %d\n", __func__,
			rgb_ogo.r_pre_offset, rgb_ogo.g_pre_offset, rgb_ogo.b_pre_offset,
			rgb_ogo.r_gain, rgb_ogo.g_gain, rgb_ogo.b_gain,
			rgb_ogo.r_post_offset, rgb_ogo.g_post_offset, rgb_ogo.b_post_offset);
		vpq_set_rgb_ogo(&rgb_ogo);
	} else if (!strcmp(param[0], "dnlp")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		VPQ_PR_DRV("%s dnlp %d\n", __func__, value);
		vpq_set_dnlp_mode(value);
	} else if (!strcmp(param[0], "lc")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		VPQ_PR_DRV("%s local contrast %d\n", __func__, value);
		vpq_set_lc_mode(value);
	} else if (!strcmp(param[0], "cms")) {
		struct vpq_cms_s cms_param;

		memset(&cms_param, 0, sizeof(struct vpq_cms_s));

		if (kstrtouint(param[1], 10, &cms_param.color_type) < 0)
			goto free_buf;
		if (kstrtouint(param[2], 10, &cms_param.color_9) < 0)
			goto free_buf;
		if (kstrtouint(param[3], 10, &cms_param.color_14) < 0)
			goto free_buf;
		if (kstrtouint(param[4], 10, &cms_param.cms_type) < 0)
			goto free_buf;
		if (kstrtoint(param[5], 10, &cms_param.value) < 0)
			goto free_buf;

		VPQ_PR_DRV("%s cms_param %d %d %d %d %d\n", __func__,
			cms_param.color_type, cms_param.color_9,
			cms_param.color_14, cms_param.cms_type,
			cms_param.value);
		vpq_set_color_customize(&cms_param);
	} else if (!strcmp(param[0], "blkext")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		VPQ_PR_DRV("%s black stretch %d\n", __func__, value);
		vpq_set_black_stretch(value);
	} else if (!strcmp(param[0], "blus")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		VPQ_PR_DRV("%s blue_stretch %d\n", __func__, value);
		vpq_set_blue_stretch(value);
	} else if (!strcmp(param[0], "aipq")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		VPQ_PR_DRV("%s aipq %d\n", __func__, value);
		vpq_set_aipq_mode(value);
	} else if (!strcmp(param[0], "aisr")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		VPQ_PR_DRV("%s aisr %d\n", __func__, value);
		vpq_set_aisr_mode(value);
	} else {
		VPQ_PR_DRV("%s cmd error\n", __func__);
		goto free_buf;
	}

free_buf:
	kfree(buf_org);

	return count;
}

ssize_t vpq_pq_module_cfg_show(struct class *class,
			struct class_attribute *attr,
			char *buf)
{
	VPQ_PR_DRV("show all pq modules cfg:\n");
	VPQ_PR_DRV("  pq_en:%s\n",
		(pq_module_cfg.pq_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  vadj1_en:%s\n",
		(pq_module_cfg.vadj1_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  vd1_ctrst_en:%s\n",
		(pq_module_cfg.vd1_ctrst_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  vadj2_en:%s\n",
		(pq_module_cfg.vadj2_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  post_ctrst_en:%s\n",
		(pq_module_cfg.post_ctrst_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  pregamma_en:%s\n",
		(pq_module_cfg.pregamma_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  gamma_en:%s\n",
		(pq_module_cfg.gamma_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  wb_en:%s\n",
		(pq_module_cfg.wb_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  dnlp_en:%s\n",
		(pq_module_cfg.dnlp_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  lc_en:%s\n",
		(pq_module_cfg.lc_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  black_ext_en:%s\n",
		(pq_module_cfg.black_ext_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  blue_stretch_en:%s\n",
		(pq_module_cfg.blue_stretch_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  chroma_cor_en:%s\n",
		(pq_module_cfg.chroma_cor_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  sharpness0_en:%s\n",
		(pq_module_cfg.sharpness0_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  sharpness1_en:%s\n",
		(pq_module_cfg.sharpness1_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  cm_en:%s\n",
		(pq_module_cfg.cm_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  lut3d_en:%s\n",
		(pq_module_cfg.lut3d_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  dejaggy_sr0_en:%s\n",
		(pq_module_cfg.dejaggy_sr0_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  dejaggy_sr1_en:%s\n",
		(pq_module_cfg.dejaggy_sr1_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  dering_sr0_en:%s\n",
		(pq_module_cfg.dering_sr0_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  dering_sr1_en:%s\n",
		(pq_module_cfg.dering_sr1_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  di_en:%s\n",
		(pq_module_cfg.di_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  mcdi_en:%s\n",
		(pq_module_cfg.mcdi_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  deblock_en:%s\n",
		(pq_module_cfg.deblock_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  demosquito_en:%s\n",
		(pq_module_cfg.demosquito_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  smoothplus_en:%s\n",
		(pq_module_cfg.smoothplus_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  nr_en:%s\n",
		(pq_module_cfg.nr_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  hdrtmo_en:%s\n",
		(pq_module_cfg.hdrtmo_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  ai_en:%s\n",
		(pq_module_cfg.ai_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  aisr_en:%s\n",
		(pq_module_cfg.aisr_en == 1) ? "enable" : "disable");

	VPQ_PR_DRV("--------------------------------\n");

	return 0;
}

ssize_t vpq_pq_module_cfg_store(struct class *class,
			struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	return count;
}

ssize_t vpq_pq_module_status_show(struct class *class,
			struct class_attribute *attr,
			char *buf)
{
	struct vpq_pq_state_s pq_state;

	memset(&pq_state, 0, sizeof(struct vpq_pq_state_s));

	vpq_get_pq_module_status(&pq_state);

	VPQ_PR_DRV("show vpp pq module status:\n");
	VPQ_PR_DRV("  pq:%s\n",
		(pq_state.pq_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  vadj1:%s\n",
		(pq_state.pq_cfg.vadj1_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  vd1_ctrst:%s\n",
		(pq_state.pq_cfg.vd1_ctrst_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  vadj2:%s\n",
		(pq_state.pq_cfg.vadj2_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  post_ctrst:%s\n",
		(pq_state.pq_cfg.post_ctrst_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  pregamma:%s\n",
		(pq_state.pq_cfg.pregamma_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  gamma:%s\n",
		(pq_state.pq_cfg.gamma_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  wb:%s\n",
		(pq_state.pq_cfg.wb_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  dnlp:%s\n",
		(pq_state.pq_cfg.dnlp_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  lc:%s\n",
		(pq_state.pq_cfg.lc_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  black_ext:%s\n",
		(pq_state.pq_cfg.black_ext_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  blue_stretch:%s\n",
		(pq_state.pq_cfg.blue_stretch_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  chroma_cor:%s\n",
		(pq_state.pq_cfg.chroma_cor_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  sharpness0:%s\n",
		(pq_state.pq_cfg.sharpness0_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  sharpness1:%s\n",
		(pq_state.pq_cfg.sharpness1_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  cm:%s\n",
		(pq_state.pq_cfg.cm_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  lut3d:%s\n",
		(pq_state.pq_cfg.lut3d_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  dejaggy_sr0_en:%s\n",
		(pq_state.pq_cfg.dejaggy_sr0_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  dejaggy_sr1_en:%s\n",
		(pq_state.pq_cfg.dejaggy_sr0_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  dering_sr0_en:%s\n",
		(pq_state.pq_cfg.dering_sr0_en == 1) ? "enable" : "disable");
	VPQ_PR_DRV("  dering_sr1_en:%s\n",
		(pq_state.pq_cfg.dering_sr0_en == 1) ? "enable" : "disable");

	VPQ_PR_DRV("--------------------------------\n");
	VPQ_PR_DRV("\n");

	return sprintf(buf, "%s\n", vpq_module_status_usage_str);
}

ssize_t vpq_pq_module_status_store(struct class *class,
			struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	char *buf_org;
	char *param[8];
	int value = 0;

	buf_org = kstrdup(buf, GFP_KERNEL);
	if (!buf_org)
		return -ENOMEM;

	parse_param(buf_org, param);

	if (!strcmp(param[0], "pq_module")) {
		enum vpq_module_e module = VPQ_MODULE_VADJ1;

		if (!strcmp(param[1], "vadj1"))
			module = VPQ_MODULE_VADJ1;
		else if (!strcmp(param[1], "vadj2"))
			module = VPQ_MODULE_VADJ2;
		else if (!strcmp(param[1], "pregamma"))
			module = VPQ_MODULE_PREGAMMA;
		else if (!strcmp(param[1], "gamma"))
			module = VPQ_MODULE_GAMMA;
		else if (!strcmp(param[1], "wb"))
			module = VPQ_MODULE_WB;
		else if (!strcmp(param[1], "dnlp"))
			module = VPQ_MODULE_DNLP;
		else if (!strcmp(param[1], "ccoring"))
			module = VPQ_MODULE_CCORING;
		else if (!strcmp(param[1], "sr0"))
			module = VPQ_MODULE_SR0;
		else if (!strcmp(param[1], "sr1"))
			module = VPQ_MODULE_SR1;
		else if (!strcmp(param[1], "lc"))
			module = VPQ_MODULE_LC;
		else if (!strcmp(param[1], "cm"))
			module = VPQ_MODULE_CM;
		else if (!strcmp(param[1], "ble"))
			module = VPQ_MODULE_BLE;
		else if (!strcmp(param[1], "bls"))
			module = VPQ_MODULE_BLS;
		else if (!strcmp(param[1], "lut3d"))
			module = VPQ_MODULE_LUT3D;
		else if (!strcmp(param[1], "dejaggy_sr0"))
			module = VPQ_MODULE_DEJAGGY_SR0;
		else if (!strcmp(param[1], "dejaggy_sr1"))
			module = VPQ_MODULE_DEJAGGY_SR1;
		else if (!strcmp(param[1], "dering_sr0"))
			module = VPQ_MODULE_DERING_SR0;
		else if (!strcmp(param[1], "dering_sr1"))
			module = VPQ_MODULE_DERING_SR1;
		else if (!strcmp(param[1], "all"))
			module = VPQ_MODULE_ALL;
		else
			;

		if (kstrtouint(param[2], 10, &value) < 0)
			goto free_buf;

		VPQ_PR_DRV("%s pq_module_status %d %d\n", __func__, module, value);
		vpq_set_pq_module_status(module, value);
	}

free_buf:
	kfree(buf_org);

	return count;
}

static char *src_type_tostring(vpq_vfm_source_type_e src_type)
{
	if (src_type == VFRAME_SOURCE_TYPE_OTHERS) {
		return "MPEG";
	} else if (src_type == VFRAME_SOURCE_TYPE_TUNER) {
		return "DTV";
	} else if (src_type == VFRAME_SOURCE_TYPE_CVBS) {
		return "AV";
	} else if (src_type == VFRAME_SOURCE_TYPE_HDMI) {
		vpq_vfm_port_e src_port = vpq_vfm_get_source_port();

		if (src_port == TVIN_PORT_HDMI0)
			return "HDMI1";
		else if (src_port == TVIN_PORT_HDMI1)
			return "HDMI2";
		else if (src_port == TVIN_PORT_HDMI2)
			return "HDMI3";
		else if (src_port == TVIN_PORT_HDMI3)
			return "HDMI4";
	}

	return "NULL";
}

static char *hdr_type_tostring(enum vpq_vfm_hdr_type_e hdr_type)
{
	if (hdr_type == VPQ_VFM_HDR_TYPE_HDR10)
		return "HDR10";
	else if (hdr_type == VPQ_VFM_HDR_TYPE_HDR10PLUS)
		return "HDR10+";
	else if (hdr_type == VPQ_VFM_HDR_TYPE_DOBVI)
		return "DV";
	else if (hdr_type == VPQ_VFM_HDR_TYPE_PRIMESL)
		return "PRIMESL";
	else if (hdr_type == VPQ_VFM_HDR_TYPE_HLG)
		return "HLG";
	else if (hdr_type == VPQ_VFM_HDR_TYPE_SDR)
		return "SDR";
	else if (hdr_type == VPQ_VFM_HDR_TYPE_MVC)
		return "MVC";
	else
		return "NONE";
}

ssize_t vpq_src_infor_show(struct class *class,
			struct class_attribute *attr,
			char *buf)
{
	vpq_vfm_source_type_e src_type = VFRAME_SOURCE_TYPE_OTHERS;
	vpq_vfm_sig_status_e sig_status = TVIN_SIG_STATUS_NULL;
	vpq_vfm_sig_fmt_e sig_fmt = TVIN_SIG_FMT_NULL;
	vpq_vfm_trans_fmt_e trans_fmt = TVIN_TFMT_2D;
	enum vpq_vfm_hdr_type_e hdr_type = VPQ_VFM_HDR_TYPE_SDR;
	enum vpq_vfm_scan_mode_e scan_mode = VPQ_VFM_SCAN_MODE_NULL;
	bool is_dlby = false;
	unsigned int height = 0, width = 0;
	unsigned int fps = 0;

	src_type = vpq_vfm_get_source_type();
	sig_status = vpq_vfm_get_signal_status();
	sig_fmt = vpq_vfm_get_signal_format();
	trans_fmt = vpq_vfm_get_trans_format();
	hdr_type = vpq_vfm_get_hdr_type();
	scan_mode = vpq_vfm_get_signal_scan_mode();

	is_dlby = vpq_vfm_get_is_dlby();
	vpq_frm_get_height_width(&height, &width);
	vpq_frm_get_fps(&fps);

	VPQ_PR_DRV("current signal information:\n");
	VPQ_PR_DRV("  source:%s\n", src_type_tostring(src_type));
	VPQ_PR_DRV("  signal status:%s\n",
				(sig_status == 4) ? "stable" :
				((sig_status == 2) ? "unstable" : "no signal"));
	VPQ_PR_DRV("  signal fmt:%d(0x%x)\n", sig_fmt, sig_fmt);
	VPQ_PR_DRV("  trans fmt:%d\n", trans_fmt);
	VPQ_PR_DRV("  height:%d  width:%d\n", height, width);
	VPQ_PR_DRV("  fps:%dhz\n", fps);
	VPQ_PR_DRV("  hdr type:%s\n", hdr_type_tostring(hdr_type));
	VPQ_PR_DRV("  isdv:%s\n", ((is_dlby == 1) ? "true" : "false"));
	VPQ_PR_DRV("  scan_mode:%s\n",
				(scan_mode == VPQ_VFM_SCAN_MODE_PROGRESSIVE) ? "P" :
				((scan_mode == VPQ_VFM_SCAN_MODE_INTERLACED) ? "I" : "NULL"));
	VPQ_PR_DRV("--------------------------------\n");

	return 0;
}

ssize_t vpq_src_infor_store(struct class *class,
			struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	return count;
}

char *csctype[29][2] = {
	//csc type                      value
	{"NULL",                        "0"},
	{"RGB_YUV601",                  "1"},
	{"RGB_YUV601F",                 "2"},
	{"RGB_YUV709",                  "3"},
	{"RGB_YUV709F",                 "4"},
	{"YUV601_RGB",                  "16"},
	{"YUV601_YUV601F",              "17"},
	{"YUV601_YUV709",               "18"},
	{"YUV601_YUV709F",              "19"},
	{"YUV601F_RGB",                 "20"},
	{"YUV601F_YUV601",              "21"},
	{"YUV601F_YUV709",              "22"},
	{"YUV601F_YUV709F",             "23"},
	{"YUV709_RGB",                  "32"},
	{"YUV709_YUV601",               "33"},
	{"YUV709_YUV601F",              "34"},
	{"YUV709_YUV709F",              "35"},
	{"YUV709F_RGB",                 "36"},
	{"YUV709F_YUV601",              "37"},
	{"YUV709F_YUV709",              "38"},
	{"YUV601L_YUV709L",             "39"},
	{"YUV709L_YUV601L",             "40"},
	{"YUV709F_YUV601F",             "41"},
	{"BT2020YUV_BT2020RGB",         "64"},
	{"BT2020RGB_709RGB",            "65"},
	{"BT2020RGB_CUSRGB",            "66"},
	{"BT2020YUV_BT2020RGB_DYNAMIC", "80"},
	{"BT2020YUV_BT2020RGB_CUVA",    "81"},
	{"\0"},
};

static void cat_csc_type(void)
{
	int i = 0;

	for (i = 0; i < 29; i++) {
		if (strcmp(csctype[i][0], "\0") == 0) {
			VPQ_PR_DRV("show csc_type: NULL\n");
			break;
		}

		if (vpq_get_csc_type() == atoi(csctype[i][1])) {
			VPQ_PR_DRV("show csc_type: %s\n", csctype[i][0]);
			break;
		}
	}
}

ssize_t vpq_src_hist_avg_show(struct class *class,
			struct class_attribute *attr,
			char *buf)
{
	struct vpq_histgm_ave_s hist_avg;

	memset(&hist_avg, 0, sizeof(struct vpq_histgm_ave_s));

	vpq_get_hist_avg(&hist_avg);
	VPQ_PR_DRV("current hist avg:\n");
	VPQ_PR_DRV("  sum:%d\n", hist_avg.sum);
	VPQ_PR_DRV("  width:%d height:%d\n", hist_avg.width, hist_avg.height);
	VPQ_PR_DRV("  ave:%d\n", hist_avg.ave);
	VPQ_PR_DRV("--------------------------------\n");

	return 0;
}

ssize_t vpq_src_hist_avg_store(struct class *class,
			struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	return count;
}

ssize_t vpq_debug_other_show(struct class *class,
			struct class_attribute *attr,
			char *buf)
{
	cat_csc_type();
	VPQ_PR_DRV("\n");

	return sprintf(buf, "%s\n", vpq_others_usage_str);
}

ssize_t vpq_debug_other_store(struct class *class,
			struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	char *buf_org;
	char *param[8];
	int value = 0;

	buf_org = kstrdup(buf, GFP_KERNEL);
	if (!buf_org)
		return -ENOMEM;

	parse_param(buf_org, param);

	if (!strcmp(param[0], "csc_type")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		VPQ_PR_DRV("%s csc_type %d\n", __func__, value);
		vpq_set_csc_type(value);
	} else if (!strcmp(param[0], "ccorring")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		VPQ_PR_DRV("%s chroma_corring %d\n", __func__, value);
		vpq_set_chroma_coring(value);
	} else {
		VPQ_PR_DRV("%s cmd error\n", __func__);
		goto free_buf;
	}

free_buf:
	kfree(buf_org);

	return count;
}

ssize_t vpq_dump_show(struct class *class,
			struct class_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%s\n", vpq_dump_table_usage_str);
}

ssize_t vpq_dump_store(struct class *class,
			struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	char *buf_org;
	char *param[8];
	int value = 0;

	buf_org = kstrdup(buf, GFP_KERNEL);
	if (!buf_org)
		return -ENOMEM;

	parse_param(buf_org, param);

	if (!strcmp(param[0], "dump_data")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		VPQ_PR_DRV("%s dump_module %d\n", __func__, value);
		vpq_dump_pq_table((enum vpq_dump_type_e)value);
	} else {
		VPQ_PR_DRV("%s cmd error\n", __func__);
		goto free_buf;
	}

free_buf:
	kfree(buf_org);

	return count;
}
