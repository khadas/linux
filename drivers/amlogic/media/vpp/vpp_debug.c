// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "vpp_common.h"
#include "vpp_debug.h"
#include "vpp_pq_mgr.h"
#include "vpp_modules_inc.h"
#include "vpp_ver.h"

static const char *vpp_debug_usage_str = {
	"Usage:\n"
	"echo dbg_lvl value > /sys/class/aml_vpp/vpp_debug\n"
	"echo dbg_dump > /sys/class/aml_vpp/vpp_debug\n"
	"echo value > /sys/class/aml_vpp/vpp_brightness\n"
	"echo value > /sys/class/aml_vpp/vpp_brightness_post\n"
	"echo value > /sys/class/aml_vpp/vpp_contrast\n"
	"echo value > /sys/class/aml_vpp/vpp_contrast_post\n"
	"echo value > /sys/class/aml_vpp/vpp_sat\n"
	"echo value > /sys/class/aml_vpp/vpp_sat_post\n"
	"echo value > /sys/class/aml_vpp/vpp_hue\n"
	"echo value > /sys/class/aml_vpp/vpp_hue_post\n"
	"echo value_0 ... value_8 > /sys/class/aml_vpp/vpp_white_balance\n"
	"--> value_0~8 mean r/g/b_preoffset/gain/postoffset\n"
};

/*Internal functions*/
void _parse_param(char *pbuf, char **param)
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

static int _parse_param_int(const char *pbuf,
	int param_num, int *pdata)
{
	char *token;
	char *params;
	int *out = pdata;
	int len = 0;
	int count = 0;
	int res = 0;

	if (!pbuf)
		return 0;

	params = kstrdup(pbuf, GFP_KERNEL);
	if (!params)
		return -ENOMEM;

	token = params;
	len = strlen(token);

	do {
		token = strsep(&params, " ");
		while (token && len &&
			(isspace(*token) || !isgraph(*token))) {
			token++;
			len--;
		}

		if (len == 0 || !token || kstrtoint(token, 0, &res) < 0)
			break;

		len = strlen(token);
		*out++ = res;
		count++;
	} while ((token) && (count < param_num) && (len > 0));

	kfree(params);
	return count;
}

static void _dump_pq_mgr_settings(void)
{
	struct vpp_pq_mgr_settings *psettings;

	psettings = vpp_pq_mgr_get_settings();

	PR_DRV("vadj1_en = %d\n",
		psettings->pq_status.pq_cfg.vadj1_en);
	PR_DRV("vadj2_en = %d\n",
		psettings->pq_status.pq_cfg.vadj2_en);
	PR_DRV("pregamma_en = %d\n",
		psettings->pq_status.pq_cfg.pregamma_en);
	PR_DRV("gamma_en = %d\n",
		psettings->pq_status.pq_cfg.gamma_en);
	PR_DRV("wb_en = %d\n",
		psettings->pq_status.pq_cfg.wb_en);

	PR_DRV("brightness = %d\n", psettings->brightness);
	PR_DRV("contrast = %d\n", psettings->contrast);
	PR_DRV("hue = %d\n", psettings->hue);
	PR_DRV("saturation = %d\n", psettings->saturation);
	PR_DRV("brightness_post = %d\n", psettings->brightness_post);
	PR_DRV("contrast_post = %d\n", psettings->contrast_post);
	PR_DRV("hue_post = %d\n", psettings->hue);
	PR_DRV("saturation_post = %d\n", psettings->saturation_post);

	PR_DRV("video_rgb_ogo r/g/b pre_offset = %d/%d/%d\n",
		psettings->video_rgb_ogo.r_pre_offset,
		psettings->video_rgb_ogo.g_pre_offset,
		psettings->video_rgb_ogo.b_pre_offset);
	PR_DRV("video_rgb_ogo r/g/b gain = %d/%d/%d\n",
		psettings->video_rgb_ogo.r_gain,
		psettings->video_rgb_ogo.g_gain,
		psettings->video_rgb_ogo.b_gain);
	PR_DRV("video_rgb_ogo r/g/b post_offset = %d/%d/%d\n",
		psettings->video_rgb_ogo.r_post_offset,
		psettings->video_rgb_ogo.g_post_offset,
		psettings->video_rgb_ogo.b_post_offset);
}

/*External functions*/
int vpp_debug_init(struct vpp_dev_s *pdev)
{
	return 0;
}

ssize_t vpp_debug_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	PR_DRV("Current dbg_lvl value = %d\n", pr_lvl);

	return sprintf(buf, "%s\n", vpp_debug_usage_str);
}

ssize_t vpp_debug_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	ulong val;
	char *buf_org;
	char *param[8];

	buf_org = kstrdup(buf, GFP_KERNEL);
	if (!buf_org)
		return -ENOMEM;

	_parse_param(buf_org, param);

	if (!strcmp(param[0], "dbg_lvl")) {
		if (kstrtoul(param[1], 10, &val) < 0)
			goto fr_bf;

		pr_lvl = (uint)val;
		PR_DRV("pr_lvl = %d\n", pr_lvl);
	} else if (!strcmp(param[0], "dbg_dump")) {
		_dump_pq_mgr_settings();
	}

fr_bf:
	kfree(buf_org);
	return count;
}

ssize_t vpp_debug_brightness_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	int val = 0;

	vpp_pq_mgr_get_brightness(&val);

	return sprintf(buf, "%d\n", val);
}

ssize_t vpp_debug_brightness_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	size_t r;
	int val = 0;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1)
		return -EINVAL;

	val = vpp_check_range(val, (-1024), 1023);
	vpp_pq_mgr_set_brightness(val);

	PR_DRV("[%s] set value = %d\n", __func__, val);

	return count;
}

ssize_t vpp_debug_brightness_post_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	int val = 0;

	vpp_pq_mgr_get_brightness_post(&val);

	return sprintf(buf, "%d\n", val);
}

ssize_t vpp_debug_brightness_post_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	size_t r;
	int val = 0;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1)
		return -EINVAL;

	val = vpp_check_range(val, (-1024), 1023);
	vpp_pq_mgr_set_brightness_post(val);

	PR_DRV("[%s] set value = %d\n", __func__, val);

	return count;
}

ssize_t vpp_debug_contrast_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	int val = 0;

	vpp_pq_mgr_get_contrast(&val);

	return sprintf(buf, "%d\n", val);
}

ssize_t vpp_debug_contrast_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	size_t r;
	int val = 0;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1)
		return -EINVAL;

	val = vpp_check_range(val, (-1024), 1023);
	vpp_pq_mgr_set_contrast(val);

	PR_DRV("[%s] set value = %d\n", __func__, val);

	return count;
}

ssize_t vpp_debug_contrast_post_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	int val = 0;

	vpp_pq_mgr_get_contrast_post(&val);

	return sprintf(buf, "%d\n", val);
}

ssize_t vpp_debug_contrast_post_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	size_t r;
	int val = 0;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1)
		return -EINVAL;

	val = vpp_check_range(val, (-1024), 1023);
	vpp_pq_mgr_set_contrast_post(val);

	PR_DRV("[%s] set value = %d\n", __func__, val);

	return count;
}

ssize_t vpp_debug_saturation_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	int val = 0;

	vpp_pq_mgr_get_saturation(&val);

	return sprintf(buf, "%d\n", val);
}

ssize_t vpp_debug_saturation_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	size_t r;
	int val = 0;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1)
		return -EINVAL;

	val = vpp_check_range(val, (-128), 127);
	vpp_pq_mgr_set_saturation(val);

	PR_DRV("[%s] set value = %d\n", __func__, val);

	return count;
}

ssize_t vpp_debug_saturation_post_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	int val = 0;

	vpp_pq_mgr_get_saturation_post(&val);

	return sprintf(buf, "%d\n", val);
}

ssize_t vpp_debug_saturation_post_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	size_t r;
	int val = 0;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1)
		return -EINVAL;

	val = vpp_check_range(val, (-128), 127);
	vpp_pq_mgr_set_saturation_post(val);

	PR_DRV("[%s] set value = %d\n", __func__, val);

	return count;
}

ssize_t vpp_debug_hue_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	int val = 0;

	vpp_pq_mgr_get_hue(&val);

	return sprintf(buf, "%d\n", val);
}

ssize_t vpp_debug_hue_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	size_t r;
	int val = 0;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1)
		return -EINVAL;

	val = vpp_check_range(val, (-25), 25);
	vpp_pq_mgr_set_hue(val);

	PR_DRV("[%s] set value = %d\n", __func__, val);

	return count;
}

ssize_t vpp_debug_hue_post_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	int val = 0;

	vpp_pq_mgr_get_hue_post(&val);

	return sprintf(buf, "%d\n", val);
}

ssize_t vpp_debug_hue_post_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	size_t r;
	int val = 0;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1)
		return -EINVAL;

	val = vpp_check_range(val, (-25), 25);
	vpp_pq_mgr_set_hue_post(val);

	PR_DRV("[%s] set value = %d\n", __func__, val);

	return count;
}

ssize_t vpp_debug_pre_gamma_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	return 0;
}

ssize_t vpp_debug_pre_gamma_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	return 0;
}

ssize_t vpp_debug_pre_gamma_pattern_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	PR_DRV("echo status r g b > /sys/class/aml_vpp/vpp_pre_gamma_pattern\n");
	PR_DRV("--> status = 0:disable/1:enable pre_gamma pattern\n");
	PR_DRV("--> 10bit r/g/b input 0~1023\n");

	return 0;
}

ssize_t vpp_debug_pre_gamma_pattern_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	int parsed[4];

	if (likely(_parse_param_int(buf, 4, parsed) != 4))
		return -ENOMEM;

	PR_DRV("[%s] set value = %d/%d/%d/%d\n",
	__func__, parsed[0], parsed[1], parsed[2], parsed[3]);

	vpp_module_pre_gamma_pattern(parsed[0], parsed[1],
		parsed[2], parsed[3]);

	return count;
}

ssize_t vpp_debug_gamma_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	return 0;
}

ssize_t vpp_debug_gamma_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	return 0;
}

ssize_t vpp_debug_gamma_pattern_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	PR_DRV("echo status r g b > /sys/class/aml_vpp/vpp_gamma_pattern\n");
	PR_DRV("--> status = 0:disable/1:enable gamma pattern\n");
	PR_DRV("--> 10bit r/g/b input 0~1023\n");

	return 0;
}

ssize_t vpp_debug_gamma_pattern_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	int parsed[4];

	if (likely(_parse_param_int(buf, 4, parsed) != 4))
		return -ENOMEM;

	PR_DRV("[%s] set value = %d/%d/%d/%d\n",
	__func__, parsed[0], parsed[1], parsed[2], parsed[3]);

	vpp_module_lcd_gamma_pattern(parsed[0], parsed[1],
		parsed[2], parsed[3]);

	return count;
}

ssize_t vpp_debug_whitebalance_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	size_t count;
	struct vpp_white_balance_s data;

	vpp_pq_mgr_get_whitebalance(&data);

	count = sprintf(buf, "%d %d %d\n",
		data.r_pre_offset, data.g_pre_offset, data.b_pre_offset);
	count += sprintf(buf, "%d %d %d\n",
		data.r_gain, data.g_gain, data.b_gain);
	count += sprintf(buf, "%d %d %d\n",
		data.r_post_offset, data.g_post_offset, data.b_post_offset);

	return count;
}

ssize_t vpp_debug_whitebalance_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	int parsed[9] = {0};
	struct vpp_white_balance_s data;

	if (likely(_parse_param_int(buf, 9, parsed) != 9))
		return -EINVAL;

	data.r_pre_offset = parsed[0];
	data.g_pre_offset = parsed[1];
	data.b_pre_offset = parsed[2];
	data.r_gain = parsed[3];
	data.g_gain = parsed[4];
	data.b_gain = parsed[5];
	data.r_post_offset = parsed[6];
	data.g_post_offset = parsed[7];
	data.b_post_offset = parsed[8];

	vpp_pq_mgr_set_whitebalance(&data);

	return count;
}

ssize_t vpp_debug_module_ctrl_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	PR_DRV("echo module_idx status > /sys/class/aml_vpp/vpp_module_ctrl\n");
	PR_DRV("--> module_idx = 0:vadj1/1:vadj2/2:pregamma/3:gamma/4:wb\n");
	PR_DRV("--> status = 0:disable/1:enable\n");

	return 0;
}

ssize_t vpp_debug_module_ctrl_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	int parsed[2] = {0};

	if (likely(_parse_param_int(buf, 2, parsed) != 2))
		return -EINVAL;

	vpp_pq_mgr_set_module_status((enum vpp_module_e)parsed[0], (bool)parsed[1]);

	PR_DRV("[%s] set value = %d/%d\n",
		__func__, parsed[0], parsed[1]);

	return count;
}

