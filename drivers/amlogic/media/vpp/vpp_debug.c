// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "vpp_common.h"
#include "vpp_debug.h"
#include "vpp_pq_mgr.h"
#include "vpp_vf_proc.h"
#include "vpp_modules_inc.h"
#include "vpp_ver.h"

static const char *vpp_debug_usage_str = {
	"Usage:\n"
	"echo dbg_lvl value > /sys/class/aml_vpp/vpp_debug\n"
	"echo dbg_dump > /sys/class/aml_vpp/vpp_debug\n"
	"echo dbg_dump_reg module_index > /sys/class/aml_vpp/vpp_debug\n"
	"echo dbg_dump_data module_index > /sys/class/aml_vpp/vpp_debug\n"
	"--> 0~11: vadj/gamma/go/ve/meter/matrix/cm/sr/lc/dnlp/lut3d/hdr\n"
	"echo value > /sys/class/aml_vpp/vpp_brightness\n"
	"echo value > /sys/class/aml_vpp/vpp_brightness_post\n"
	"echo value > /sys/class/aml_vpp/vpp_contrast\n"
	"echo value > /sys/class/aml_vpp/vpp_contrast_post\n"
	"echo value > /sys/class/aml_vpp/vpp_sat\n"
	"echo value > /sys/class/aml_vpp/vpp_sat_post\n"
	"echo value > /sys/class/aml_vpp/vpp_hue\n"
	"echo value > /sys/class/aml_vpp/vpp_hue_post\n"
	"echo value > /sys/class/aml_vpp/vpp_sharpness\n"
	"echo value > /sys/class/aml_vpp/vpp_pc_mode\n"
	"cat /sys/class/aml_vpp/vpp_hdr_type\n"
	"cat /sys/class/aml_vpp/vpp_pc_mode\n"
	"cat /sys/class/aml_vpp/vpp_csc_type\n"
	"cat /sys/class/aml_vpp/vpp_color_primary\n"
};

/*Internal functions*/
static void _parse_param(char *pbuf, char **param)
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

static void _d_convert_str(int num,
	int num_num, char cur_s[],
	int char_bit, int bit_chose)
{
	char buf[9] = {0};
	int i, count;

	if (bit_chose == 10)
		snprintf(buf, sizeof(buf), "%d", num);
	else if (bit_chose == 16)
		snprintf(buf, sizeof(buf), "%x", num);

	count = strlen(buf);
	if (count > 4)
		count = 4;

	for (i = 0; i < count; i++)
		buf[i + char_bit] = buf[i];

	for (i = 0; i < char_bit; i++)
		buf[i] = '0';

	count = strlen(buf);

	for (i = 0; i < char_bit; i++)
		buf[i] = buf[count - char_bit + i];

	if (num_num > 0) {
		for (i = 0; i < char_bit; i++)
			cur_s[i + num_num * char_bit] = buf[i];
	} else {
		for (i = 0; i < char_bit; i++)
			cur_s[i] = buf[i];
	}
}

static void _str_sapr_to_d(char *s, int *d, int n)
{
	int i, j, count;
	long value;
	char des[9] = {0};

	count = (strlen(s) + n - 2) / (n - 1);
	for (i = 0; i < count; i++) {
		for (j = 0; j < n - 1; j++)
			des[j] = s[j + i * (n - 1)];
		des[n - 1] = '\0';

		if (kstrtol(des, 10, &value) < 0)
			return;

		d[i] = value;
	}
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

static void _dump_reg_by_addr(unsigned int start, unsigned int end)
{
	unsigned int addr;
	int reg_value;
	enum io_mode_e io_mode = EN_MODE_DIR;

	if (end < start) {
		PR_DRV("reg start and end address not fit.\n");
		return;
	}

	for (addr = start; addr < end + 1; addr++) {
		reg_value = READ_VPP_REG_BY_MODE(io_mode, addr);
		PR_DRV("0x%x=0x%x\n", addr, reg_value);
	}
}

static void _dump_cm_reg_by_addr(unsigned int start, unsigned int end)
{
	unsigned int addr;
	int reg_value;

	if (end < start) {
		PR_DRV("reg start and end address not fit.\n");
		return;
	}

	for (addr = start; addr < end + 1; addr++) {
		reg_value = vpp_module_cm_get_reg(addr);
		PR_DRV("0x%x=0x%x\n", addr, reg_value);
	}
}

/*External functions*/
int vpp_debug_init(struct vpp_dev_s *pdev)
{
	return 0;
}

ssize_t vpp_debug_reg_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	PR_DRV("Usage:\n");
	PR_DRV("echo w addr value > /sys/class/aml_vpp/vpq_reg_rw\n");
	PR_DRV("echo bw addr value start length > /sys/class/aml_vpp/vpq_reg_rw\n");
	PR_DRV("echo r addr > /sys/class/aml_vpp/vpq_reg_rw\n");
	PR_DRV("echo re addr_start addr_end > /sys/class/aml_vpp/vpq_reg_rw\n");
	PR_DRV("--> addr and value must be hex.\n");

	return 0;
}

ssize_t vpp_debug_reg_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	unsigned int addr, bit_start, bit_len;
	unsigned int addr_start, addr_end;
	int reg_value;
	char *buf_org;
	char *param[5];
	enum io_mode_e io_mode = EN_MODE_DIR;

	buf_org = kstrdup(buf, GFP_KERNEL);
	if (!buf_org)
		return -ENOMEM;

	_parse_param(buf_org, param);

	if (!strncmp(param[0], "r", 1)) {
		if (kstrtouint(param[1], 16, &addr) < 0)
			goto free_buf;

		reg_value = READ_VPP_REG_BY_MODE(io_mode, addr);
		PR_DRV("0x%x=0x%x\n", addr, reg_value);
	} else if (!strncmp(param[0], "w", 1)) {
		if (kstrtouint(param[1], 16, &addr) < 0)
			goto free_buf;

		if (kstrtoint(param[2], 16, &reg_value) < 0)
			goto free_buf;

		WRITE_VPP_REG_BY_MODE(io_mode, addr, reg_value);
	} else if (!strncmp(param[0], "bw", 2)) {
		if (kstrtouint(param[1], 16, &addr) < 0)
			goto free_buf;

		if (kstrtoint(param[2], 16, &reg_value) < 0)
			goto free_buf;

		if (kstrtoint(param[3], 16, &bit_start) < 0)
			goto free_buf;

		if (kstrtoint(param[4], 16, &bit_len) < 0)
			goto free_buf;

		WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr,
			reg_value, bit_start, bit_len);
	} else if (!strncmp(param[0], "re", 2)) {
		if (kstrtouint(param[1], 16, &addr_start) < 0)
			goto free_buf;

		if (kstrtoint(param[2], 16, &addr_end) < 0)
			goto free_buf;

		_dump_reg_by_addr(addr_start, addr_end);
	} else {
		PR_DRV("[%s] param[0] = %s, param[1] = %s\n",
			__func__, param[0], param[1]);
	}

	kfree(buf_org);
	return count;
free_buf:
	kfree(buf_org);
	return -EINVAL;
}

ssize_t vpp_debug_cm_reg_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	PR_DRV("Usage:\n");
	PR_DRV("echo w addr value > /sys/class/aml_vpp/vpq_cm_reg_rw\n");
	PR_DRV("echo r addr > /sys/class/aml_vpp/vpq_cm_reg_rw\n");
	PR_DRV("echo re addr_start addr_end > /sys/class/aml_vpp/vpq_cm_reg_rw\n");
	PR_DRV("--> addr and value must be hex.\n");

	return 0;
}

ssize_t vpp_debug_cm_reg_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	unsigned int addr;
	unsigned int addr_start, addr_end;
	int reg_value;
	char *buf_org;
	char *param[5];

	buf_org = kstrdup(buf, GFP_KERNEL);
	if (!buf_org)
		return -ENOMEM;

	_parse_param(buf_org, param);

	if (!strncmp(param[0], "r", 1)) {
		if (kstrtouint(param[1], 16, &addr) < 0)
			goto free_buf;

		reg_value = vpp_module_cm_get_reg(addr);
		PR_DRV("0x%x=0x%x\n", addr, reg_value);
	} else if (!strncmp(param[0], "w", 1)) {
		if (kstrtouint(param[1], 16, &addr) < 0)
			goto free_buf;

		if (kstrtoint(param[2], 16, &reg_value) < 0)
			goto free_buf;

		vpp_module_cm_set_reg(addr, reg_value);
	} else if (!strncmp(param[0], "re", 2)) {
		if (kstrtouint(param[1], 16, &addr_start) < 0)
			goto free_buf;

		if (kstrtoint(param[2], 16, &addr_end) < 0)
			goto free_buf;

		_dump_cm_reg_by_addr(addr_start, addr_end);
	} else {
		PR_DRV("[%s] param[0] = %s, param[1] = %s\n",
			__func__, param[0], param[1]);
	}

	kfree(buf_org);
	return count;
free_buf:
	kfree(buf_org);
	return -EINVAL;
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
	char *buf_org;
	char *param[8];
	unsigned int val;
	enum vpp_dump_module_info_e info_type;

	buf_org = kstrdup(buf, GFP_KERNEL);
	if (!buf_org)
		return -ENOMEM;

	_parse_param(buf_org, param);

	if (!strcmp(param[0], "dbg_lvl")) {
		if (kstrtouint(param[1], 10, &pr_lvl) < 0)
			goto fr_bf;

		PR_DRV("pr_lvl = %d\n", pr_lvl);
	} else if (!strcmp(param[0], "dbg_dump")) {
		_dump_pq_mgr_settings();
	} else if (!strcmp(param[0], "dbg_dump_reg")) {
		if (kstrtouint(param[1], 10, &val) < 0)
			goto fr_bf;

		info_type = EN_DUMP_INFO_REG;

		switch (val) {
		case 0:
			vpp_module_vadj_dump_info(info_type);
			break;
		case 1:
			vpp_module_gamma_dump_info(info_type);
			break;
		case 2:
			vpp_module_go_dump_info(info_type);
			break;
		case 3:
			vpp_module_ve_dump_info(info_type);
			break;
		case 4:
			vpp_module_meter_dump_info(info_type);
			break;
		case 5:
			vpp_module_matrix_dump_info(info_type);
			break;
		case 6:
			vpp_module_cm_dump_info(info_type);
			break;
		case 7:
			vpp_module_sr_dump_info(info_type);
			break;
		case 8:
			vpp_module_lc_dump_info(info_type);
			break;
		case 9:
			vpp_module_dnlp_dump_info(info_type);
			break;
		case 10:
			vpp_module_lut3d_dump_info(info_type);
			break;
		case 11:
			vpp_module_hdr_dump_info(info_type);
			break;
		default:
			PR_DRV("module index not fit.\n");
			break;
		}
	} else if (!strcmp(param[0], "dbg_dump_data")) {
		if (kstrtouint(param[1], 10, &val) < 0)
			goto fr_bf;

		info_type = EN_DUMP_INFO_DATA;

		switch (val) {
		case 0:
			vpp_module_vadj_dump_info(info_type);
			break;
		case 1:
			vpp_module_gamma_dump_info(info_type);
			break;
		case 2:
			vpp_module_go_dump_info(info_type);
			break;
		case 3:
			vpp_module_ve_dump_info(info_type);
			break;
		case 4:
			vpp_module_meter_dump_info(info_type);
			break;
		case 5:
			vpp_module_matrix_dump_info(info_type);
			break;
		case 6:
			vpp_module_cm_dump_info(info_type);
			break;
		case 7:
			vpp_module_sr_dump_info(info_type);
			break;
		case 8:
			vpp_module_lc_dump_info(info_type);
			break;
		case 9:
			vpp_module_dnlp_dump_info(info_type);
			break;
		case 10:
			vpp_module_lut3d_dump_info(info_type);
			break;
		case 11:
			vpp_module_hdr_dump_info(info_type);
			break;
		default:
			PR_DRV("module index not fit.\n");
			break;
		}
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

ssize_t vpp_debug_sharpness_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	int val = 0;

	vpp_pq_mgr_get_sharpness(&val);

	return sprintf(buf, "%d\n", val);
}

ssize_t vpp_debug_sharpness_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	size_t r;
	int val = 0;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1)
		return -EINVAL;

	val = vpp_check_range(val, 0, 255);
	vpp_pq_mgr_set_sharpness(val);

	PR_DRV("[%s] set value = %d\n", __func__, val);

	return count;
}

ssize_t vpp_debug_pre_gamma_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct vpp_gamma_table_s *data = NULL;
	unsigned int len = 0;
	int i = 0;

	PR_DRV("Usage set data:\n");
	PR_DRV("echo g_type c_type > /sys/class/aml_vpp/vpp_pre_gamma\n");
	PR_DRV("--> g_type: pre_gamma, lcd_gamma\n");
	PR_DRV("--> c_type: 0=restore, 1=linear\n\n");

	PR_DRV("Show pre gamma data from ioctl:\n");
	len = vpp_pq_mgr_get_pre_gamma_table_len();
	data = vpp_pq_mgr_get_pre_gamma_table();

	for (i = 0; i < len; i++)
		pr_info("%d:%d/%d/%d\n", i,
			data->r_data[i],
			data->g_data[i],
			data->b_data[i]);

	PR_DRV("Show lcd gamma data from ioctl:\n");
	len = vpp_pq_mgr_get_gamma_table_len();
	data = vpp_pq_mgr_get_gamma_table();

	for (i = 0; i < len; i++)
		pr_info("%d:%d/%d/%d\n", i,
			data->r_data[i],
			data->g_data[i],
			data->b_data[i]);

	return 0;
}

ssize_t vpp_debug_pre_gamma_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	char *buf_org;
	char *param[4];
	unsigned int curve_type = 0;
	unsigned int *pg_data = NULL;
	struct vpp_gamma_table_s *pdata_pre = NULL;
	struct vpp_gamma_table_s *pdata_lcd = NULL;
	unsigned int buf_size;
	int i = 0;
	int max_val = 1020;
	unsigned int len = 0;

	buf_org = kstrdup(buf, GFP_KERNEL);
	if (!buf_org)
		return -ENOMEM;

	_parse_param(buf_org, param);

	PR_DRV("[%s] param[0] = %s, param[1] = %s\n",
		__func__, param[0], param[1]);

	if (kstrtouint(param[1], 10, &curve_type) < 0)
		goto fr_bf;

	if (!strcmp(param[0], "pre")) {
		len = vpp_module_pre_gamma_get_table_len();
		if (len == 0)
			goto fr_bf;

		buf_size = len * sizeof(unsigned int);
		pg_data = kmalloc(buf_size, GFP_KERNEL);
		if (!pg_data)
			goto fr_bf;

		switch (curve_type) {
		case 0:
			pdata_pre = vpp_pq_mgr_get_pre_gamma_table();
			vpp_pq_mgr_set_pre_gamma_table(pdata_pre);
			break;
		case 1:
			for (i = 0; i < len; i++)
				pg_data[i] = i * (max_val / len);

			vpp_module_pre_gamma_write(pg_data, pg_data, pg_data);
			break;
		default:
			break;
		}

		kfree(pg_data);
	} else if (!strcmp(param[0], "lcd")) {
		len = vpp_module_lcd_gamma_get_table_len();
		if (len == 0)
			goto fr_bf;

		buf_size = len * sizeof(unsigned int);
		pg_data = kmalloc(buf_size, GFP_KERNEL);
		if (!pg_data)
			goto fr_bf;

		switch (curve_type) {
		case 0:
			pdata_lcd = vpp_pq_mgr_get_gamma_table();
			vpp_pq_mgr_set_gamma_table(pdata_lcd);
			break;
		case 1:
			for (i = 0; i < len; i++)
				pg_data[i] = i * (max_val / len);

			vpp_module_lcd_gamma_write(pg_data, pg_data, pg_data);
			break;
		default:
			break;
		}

		kfree(pg_data);
	}

fr_bf:
	kfree(buf_org);
	return count;
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
	PR_DRV("Usage set data:\n");
	PR_DRV("echo sgr|sgg|sgb xxx...xx > /sys/class/aml_vpp/vpp_gamma\n");
	PR_DRV("Notes:\n");
	PR_DRV("if the string xxx......xx is less than 256*3,\n");
	PR_DRV("then the remaining will be set value 0.\n");
	PR_DRV("if the string xxx......xx is more than 256*3,\n");
	PR_DRV("then the remaining will be ignored.\n");

	PR_DRV("Usage get data:\n");
	PR_DRV("echo ggr|ggg|ggb xxx > /sys/class/aml_vpp/vpp_gamma\n");
	PR_DRV("Notes:\n");
	PR_DRV("read all as point......xxx is 'all'.\n");
	PR_DRV("read all as strings......xxx is 'all_str'.\n");
	PR_DRV("read one point......xxx is a value '0~255'.\n");

	return 0;
}

ssize_t vpp_debug_gamma_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	int i = 0;
	char *buf_org;
	char *param[4];
	char gamma[4] = {0};
	char *pstemp = NULL;
	unsigned int val;
	unsigned int *ptmp;
	unsigned int *pg_data;
	unsigned int *pg_data_g;
	unsigned int *pg_data_b;
	unsigned int gamma_count;
	unsigned int buf_size;
	enum vpp_rgb_mode_e mode = EN_MODE_R;
	unsigned int len = 0;

	len = vpp_module_lcd_gamma_get_table_len();
	if (len == 0)
		return -ENOMEM;

	buf_size = len * sizeof(unsigned int);

	pg_data = kmalloc(buf_size, GFP_KERNEL);
	if (!pg_data)
		return -ENOMEM;

	buf_org = kstrdup(buf, GFP_KERNEL);
	if (!buf_org) {
		kfree(pg_data);
		return -ENOMEM;
	}

	_parse_param(buf_org, param);

	PR_DRV("[%s] param[0] = %s, param[1] = %s\n",
		__func__, param[0], param[1]);

	if (param[0][1] == 'g') {
		if (param[0][0] == 's') {
			memset(pg_data, 0, len * sizeof(unsigned int));

			gamma_count = (strlen(param[1]) + 2) / 3;
			if (gamma_count > len)
				gamma_count = len;

			for (i = 0; i < gamma_count; i++) {
				gamma[0] = param[1][3 * i + 0];
				gamma[1] = param[1][3 * i + 1];
				gamma[2] = param[1][3 * i + 2];
				gamma[3] = '\0';

				if (kstrtouint(gamma, 16, &val) < 0)
					goto free_buf;

				pg_data[i] = val;
			}

			switch (param[0][2]) {
			case 'r':
				mode = EN_MODE_R;
				break;
			case 'g':
				mode = EN_MODE_G;
				break;
			case 'b':
				mode = EN_MODE_B;
				break;
			default:
				goto free_buf;
			}

			vpp_module_lcd_gamma_write_single(mode, pg_data);
		} else if (param[0][0] == 'g') {
			pg_data_g = kmalloc(buf_size, GFP_KERNEL);
			if (!pg_data_g)
				goto free_buf;

			pg_data_b = kmalloc(buf_size, GFP_KERNEL);
			if (!pg_data_b) {
				kfree(pg_data_g);
				goto free_buf;
			}

			vpp_module_lcd_gamma_read(pg_data, pg_data_g, pg_data_b);

			switch (param[0][2]) {
			case 'r':
				ptmp = pg_data;
				break;
			case 'g':
				ptmp = pg_data_g;
				break;
			case 'b':
				ptmp = pg_data_b;
				break;
			default:
				kfree(pg_data_b);
				kfree(pg_data_g);
				goto free_buf;
			}

			if (!strcmp(param[1], "all")) {
				for (i = 0; i < len; i++)
					PR_DRV("[%s] gamma_%d[%d] = %x\n",
						__func__, param[0][2], i, ptmp[i]);
			} else if (!strcmp(param[1], "all_str")) {
				pstemp = kmalloc(600, GFP_KERNEL);
				if (!pstemp) {
					kfree(pg_data_b);
					kfree(pg_data_g);
					goto free_buf;
				}

				for (i = 0; i < len; i++)
					_d_convert_str(ptmp[i], i, pstemp, 3, 16);
				PR_DRV("[%s] gamma_%d str: %s\n",
					__func__, param[0][2], pstemp);

				kfree(pstemp);
			} else {
				if (kstrtouint(param[1], 10, &val) < 0) {
					PR_DRV("[%s] Invalid command.\n", __func__);
				} else {
					if (val <= 255)
						PR_DRV("[%s] gamma_%d[%d] = %x\n",
							__func__, param[0][2], i, ptmp[i]);
				}
			}

			kfree(pg_data_b);
			kfree(pg_data_g);
		} else {
			PR_DRV("[%s] Invalid command.\n", __func__);
			PR_DRV("[%s] Please key in: cat /sys/class/aml_vpp/vpp_gamma", __func__);
		}
	} else {
		PR_DRV("[%s] Invalid command.\n", __func__);
		PR_DRV("[%s] Please key in: cat /sys/class/aml_vpp/vpp_gamma", __func__);
	}

	kfree(buf_org);
	kfree(pg_data);
	return count;
free_buf:
	kfree(buf_org);
	kfree(pg_data);
	return -EINVAL;
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
	struct vpp_white_balance_s data;

	vpp_pq_mgr_get_whitebalance(&data);

	PR_DRV("%d %d %d\n",
		data.r_pre_offset, data.g_pre_offset, data.b_pre_offset);
	PR_DRV("%d %d %d\n",
		data.r_gain, data.g_gain, data.b_gain);
	PR_DRV("%d %d %d\n",
		data.r_post_offset, data.g_post_offset, data.b_post_offset);

	PR_DRV("Usage set data:\n");
	PR_DRV("echo xxx value > /sys/class/aml_vpp/vpp_white_balance\n");
	PR_DRV("--> xxx including wb_en/gain_*/preofst_*/postofst_*\n");
	PR_DRV("--> * means r/g/b\n");
	PR_DRV("echo value0~8 > /sys/class/aml_vpp/vpp_white_balance\n");

	PR_DRV("Usage get data:\n");
	PR_DRV("cat /sys/class/aml_vpp/vpp_white_balance\n");
	PR_DRV("--> return value_0~8 including *_preoffset/*_gain/*_postoffset\n");
	PR_DRV("--> * means r/g/b\n");

	return 0;
}

ssize_t vpp_debug_whitebalance_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	char *buf_org;
	char *param[8];
	int val;
	int parsed[9] = {0};
	struct vpp_white_balance_s data;

	buf_org = kstrdup(buf, GFP_KERNEL);
	if (!buf_org)
		return -EINVAL;

	_parse_param(buf_org, param);

	if (kstrtoint(param[1], 10, &val) < 0)
		goto free_buf;

	PR_DRV("[%s] param[0] = %s, param[1] = %s, val = %d\n",
		__func__, param[0], param[1], val);

	vpp_pq_mgr_get_whitebalance(&data);

	if (!strncmp(param[0], "wb_en", 5)) {
		vpp_module_go_en((bool)val);
	} else if (!strncmp(param[0], "preofst_r", 9)) {
		data.r_pre_offset = val;
	} else if (!strncmp(param[0], "preofst_g", 9)) {
		data.g_pre_offset = val;
	} else if (!strncmp(param[0], "preofst_b", 9)) {
		data.b_pre_offset = val;
	} else if (!strncmp(param[0], "gain_r", 6)) {
		data.r_gain = val;
	} else if (!strncmp(param[0], "gain_g", 6)) {
		data.g_gain = val;
	} else if (!strncmp(param[0], "gain_b", 6)) {
		data.b_gain = val;
	} else if (!strncmp(param[0], "postofst_r", 10)) {
		data.r_post_offset = val;
	} else if (!strncmp(param[0], "postofst_g", 10)) {
		data.g_post_offset = val;
	} else if (!strncmp(param[0], "postofst_b", 10)) {
		data.b_post_offset = val;
	} else {
		if (likely(_parse_param_int(buf, 9, parsed) != 9))
			goto free_buf;

		data.r_pre_offset = parsed[0];
		data.g_pre_offset = parsed[1];
		data.b_pre_offset = parsed[2];
		data.r_gain = parsed[3];
		data.g_gain = parsed[4];
		data.b_gain = parsed[5];
		data.r_post_offset = parsed[6];
		data.g_post_offset = parsed[7];
		data.b_post_offset = parsed[8];

		PR_DRV("[%s] set r/g/b_pre_offset = %d/%d/%d\n",
			__func__, parsed[0], parsed[1], parsed[2]);
		PR_DRV("[%s] set r/g/b_gain = %d/%d/%d\n",
				__func__, parsed[3], parsed[4], parsed[5]);
		PR_DRV("[%s] set r/g/b_post_offset = %d/%d/%d\n",
				__func__, parsed[6], parsed[7], parsed[8]);
	}

	vpp_pq_mgr_set_whitebalance(&data);

	kfree(buf_org);
	return count;
free_buf:
	kfree(buf_org);
	return -EINVAL;
}

ssize_t vpp_debug_module_ctrl_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	PR_DRV("echo module_idx status > /sys/class/aml_vpp/vpp_module_ctrl\n");
	PR_DRV("--> module_idx = 0:vadj1/1:vadj2/2:pregamma/3:gamma/4:wb\n");
	PR_DRV("--> 5:dnlp/6:ccoring/7:sharpness0/8:sharpness1\n");
	PR_DRV("--> 9:lc/10:cm/11:ble/12:bls/13:3dlut\n");
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

	PR_DRV("[%s] set value = %d/%d\n",
		__func__, parsed[0], parsed[1]);

	vpp_pq_mgr_set_module_status((enum vpp_module_e)parsed[0], (bool)parsed[1]);

	return count;
}

#define DNLP_DBG_RD_UPDATE_PARAM 0x1
#define DNLP_DBG_RD_UPDATE_CV 0x2

static unsigned int dnlp_dbg_rd_flag;
static int dnlp_dbg_rd_param;
static char dnlp_dbg_rd_curve[400] = {0};

ssize_t vpp_debug_dnlp_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	if (dnlp_dbg_rd_flag & DNLP_DBG_RD_UPDATE_PARAM) {
		dnlp_dbg_rd_flag &= ~DNLP_DBG_RD_UPDATE_PARAM;
		return sprintf(buf, "%d\n", dnlp_dbg_rd_param);
	}

	if (dnlp_dbg_rd_flag & DNLP_DBG_RD_UPDATE_CV) {
		dnlp_dbg_rd_flag &= ~DNLP_DBG_RD_UPDATE_CV;
		return sprintf(buf, "%s\n", dnlp_dbg_rd_curve);
	}

	PR_DRV("Usage set data:\n");
	PR_DRV("echo lc enable > /sys/class/aml_vpp/vpp_dnlp\n");
	PR_DRV("echo lc disable > /sys/class/aml_vpp/vpp_dnlp\n");
	PR_DRV("echo lc_dbg value > /sys/class/\n");
	PR_DRV("then the remaining will be set value 0.\n");
	PR_DRV("if the string xxx......xx is more than 256*3,\n");
	PR_DRV("then the remaining will be ignored.\n");

	PR_DRV("Usage get data:\n");
	PR_DRV("echo ggr|ggg|ggb xxx > /sys/class/aml_vpp/vpp_gamma\n");
	PR_DRV("Notes:\n");
	PR_DRV("read all as point......xxx is 'all'.\n");
	PR_DRV("read all as strings......xxx is 'all_str'.\n");
	PR_DRV("read one point......xxx is a value '0~255'.\n");

	return 0;
}

ssize_t vpp_debug_dnlp_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	int i = 0;
	int j = 0;
	int data_len = 0;
	int curve_val[VPP_DNLP_SCURV_LEN] = {0};
	long val = 0;
	char *buf_org;
	char *param[8];
	char *pstemp = NULL;
	struct dnlp_dbg_parse_cmd_s *pdnlp_dbg_parse_cmd;

	if (!vpp_module_dnlp_get_insmod_status()) {
		PR_DRV("[%s] DNLP no insmod.\n", __func__);
		return count;
	}

	buf_org = kstrdup(buf, GFP_KERNEL);
	if (!buf_org)
		return -ENOMEM;

	_parse_param(buf_org, param);

	PR_DRV("[%s] param[0] = %s, param[1] = %s\n",
		__func__, param[0], param[1]);

	pdnlp_dbg_parse_cmd = vpp_module_dnlp_get_dbg_info();

	if (!strcmp(param[0], "r")) {/*read param*/
		if (!strcmp(param[1], "param")) {
			if (!strcmp(param[2], "all")) {
				for (i = 0; pdnlp_dbg_parse_cmd[i].value; i++)
					PR_DRV("%d ", *pdnlp_dbg_parse_cmd[i].value);
				PR_DRV("\n");
			} else {
				PR_DRV("[%s] error cmd\n", __func__);
			}
		} else {
			for (i = 0; pdnlp_dbg_parse_cmd[i].value; i++) {
				if (!strcmp(param[1], pdnlp_dbg_parse_cmd[i].parse_string)) {
					dnlp_dbg_rd_param = *pdnlp_dbg_parse_cmd[i].value;
					dnlp_dbg_rd_flag |= DNLP_DBG_RD_UPDATE_PARAM;
					break;
				}
			}
		}
	} else if (!strcmp(param[0], "w")) {/*write param*/
		for (i = 0; pdnlp_dbg_parse_cmd[i].value; i++) {
			if (!strcmp(param[1], pdnlp_dbg_parse_cmd[i].parse_string)) {
				if (kstrtoul(param[2], 10, &val) < 0)
					goto free_buf;

				*pdnlp_dbg_parse_cmd[i].value = val;

				PR_DRV("[%s] %s: %d\n", __func__,
					pdnlp_dbg_parse_cmd[i].parse_string,
					*pdnlp_dbg_parse_cmd[i].value);
				break;
			}
		}
	} else if (!strcmp(param[0], "rc")) {/*read curve*/
		pstemp = kzalloc(400, GFP_KERNEL);
		if (!pstemp)
			goto free_buf;

		memset(pstemp, 0, 400);

		pdnlp_dbg_parse_cmd = vpp_module_dnlp_get_dbg_cv_info();

		for (i = 0; pdnlp_dbg_parse_cmd[i].value; i++) {
			if (!strcmp(param[1], pdnlp_dbg_parse_cmd[i].parse_string)) {
				data_len = pdnlp_dbg_parse_cmd[i].data_len;
				if (!param[2]) {
					PR_DRV("[%s] error cmd\n", __func__);
					kfree(pstemp);
					goto free_buf;
				} else if (strcmp(param[2], "all")) {
					if (kstrtoul(param[2], 10, &val) < 0) {
						kfree(pstemp);
						goto free_buf;
					}

					if (val > (data_len - 1) || val < 0)
						PR_DRV("[%s] error cmd\n", __func__);
					else
						PR_DRV("%d\n", pdnlp_dbg_parse_cmd[i].value[val]);
				} else {
					for (j = 0; j < data_len; j++)
						_d_convert_str(pdnlp_dbg_parse_cmd[i].value[j],
							j, pstemp, 4, 10);

					memcpy(dnlp_dbg_rd_curve, pstemp, sizeof(int) * data_len);
					dnlp_dbg_rd_flag |= DNLP_DBG_RD_UPDATE_CV;
					break;
				}
			}
		}

		kfree(pstemp);
	} else if (!strcmp(param[0], "wc")) {/*write curve*/
		pdnlp_dbg_parse_cmd = vpp_module_dnlp_get_dbg_cv_info();

		for (i = 0; pdnlp_dbg_parse_cmd[i].value; i++) {
			if (!strcmp(param[1], pdnlp_dbg_parse_cmd[i].parse_string)) {
				data_len = pdnlp_dbg_parse_cmd[i].data_len;
				if (!param[2]) {
					PR_DRV("[%s] error cmd\n", __func__);
					goto free_buf;
				} else if (strcmp(param[2], "all")) {
					if (kstrtoul(param[2], 10, &val) < 0)
						goto free_buf;
					if (val < data_len)
						j = val;
					else
						goto free_buf;

					if (kstrtoul(param[3], 10, &val) < 0)
						goto free_buf;
					pdnlp_dbg_parse_cmd[i].value[j] = val;
				} else {
					_str_sapr_to_d(param[3], curve_val, 5);
					for (j = 0; j < data_len; j++)
						pdnlp_dbg_parse_cmd[i].value[j] = curve_val[j];
					break;
				}
			}
		}
	} else if (!strcmp(param[0], "ro")) {
		pdnlp_dbg_parse_cmd = vpp_module_dnlp_get_dbg_ro_info();

		for (i = 0; pdnlp_dbg_parse_cmd[i].value; i++) {
			if (!strcmp(param[1], pdnlp_dbg_parse_cmd[i].parse_string)) {
				dnlp_dbg_rd_param = *pdnlp_dbg_parse_cmd[i].value;
				dnlp_dbg_rd_flag |= DNLP_DBG_RD_UPDATE_PARAM;
				break;
			}
		}
	}

	kfree(buf_org);
	return count;
free_buf:
	kfree(buf_org);
	return -EINVAL;
}

#define LC_DBG_RD_UPDATE_PARAM 0x1
#define LC_DBG_RD_UPDATE_CV 0x2
#define LC_DBG_RD_UPDATE_CV_DBG 0x4

static enum lc_lut_type_e lc_lut_type;
static enum lc_algorithm_param_e lc_alg_type;
static enum lc_config_param_e lc_cfg_type;
static enum lc_curve_tune_param_e lc_tune_type;
static unsigned int lc_dbg_rd_flag;
static int lc_dbg_rd_param;
static char lc_dbg_rd_curve_dbg[100] = {0};

ssize_t vpp_debug_lc_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	char *pstemp = NULL;
	int i = 0;
	int len = 0;
	int lut_data[63] = {0};

	if (!vpp_module_lc_get_support()) {
		PR_DRV("No support local contrast.\n");
		return 0;
	}

	if (lc_dbg_rd_flag & LC_DBG_RD_UPDATE_PARAM) {
		lc_dbg_rd_flag &= ~LC_DBG_RD_UPDATE_PARAM;
		return sprintf(buf, "%d\n", lc_dbg_rd_param);
	}

	if (lc_dbg_rd_flag & LC_DBG_RD_UPDATE_CV) {
		len = vpp_module_lc_read_lut(lc_lut_type,
			&lut_data[0], sizeof(lut_data));
		if (len == 0)
			return 0;

		pstemp = kmalloc(300, GFP_KERNEL);
		if (!pstemp)
			return 0;

		memset(pstemp, 0, 300);

		for (i = 0; i < len; i++)
			_d_convert_str(lut_data[i], i, pstemp, 4, 10);

		lc_dbg_rd_flag &= ~LC_DBG_RD_UPDATE_CV;
		len = sprintf(buf, "%s\n", pstemp);

		kfree(pstemp);
		return len;
	}

	if (lc_dbg_rd_flag & LC_DBG_RD_UPDATE_CV_DBG) {
		lc_dbg_rd_flag &= ~LC_DBG_RD_UPDATE_CV_DBG;
		return sprintf(buf, "%s\n", lc_dbg_rd_curve_dbg);
	}

	PR_DRV("Usage set data:\n");
	PR_DRV("echo lc enable/disable > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("echo lc_demo_mode enable/disable > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("echo lc_wr_lut type_val lut_data > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("--> type_val: 0~7=sat/ymin_lmt/ypkbv_ymax_lmt/ymax_lmt\n");
	PR_DRV("--> ypkbv_lmt/ypkbv_rat/ypkbv_slp_lmt/cntst_lmt\n");
	PR_DRV("echo lc_curve_isr value > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("--> value: 0/1=no used/used\n");
	PR_DRV("echo lc_osd_setting set value0~5 > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("--> value0~5: start_below/end_below/start_above/end_above\n");
	PR_DRV("--> invalid_blk/min_bv_percent_th\n");
	PR_DRV("echo osd_iir_en value > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("echo iir_refresh set value0~3 > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("--> value0~3: alpha1/alpha2/refresh_bit/ts\n");
	PR_DRV("echo scene_change_th value > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("echo detect_signal_range_en value > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("echo detect_signal_range_threshold value0~3 > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("--> value0~1: blk/wht\n");
	PR_DRV("echo reg_lmtrat_sigbin value > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("echo reg_lmtrat_thd_max value > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("echo reg_lmtrat_thd_black value > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("echo reg_thd_black value > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("echo ypkbv_black_thd value > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("echo yminv_black_thd value > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("echo tune_curve_en value > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("echo set_alg_param type_val value > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("echo set_cfg_param type_val value > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("echo set_tune_param type_val value > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("echo set_lc_param index_val > /sys/class/aml_vpp/vpp_lc\n");

	PR_DRV("Usage get data:\n");
	PR_DRV("echo get_blk_region htotal/vtotal > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("echo dump_lut_data type_val > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("echo dump_lut_str type_val > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("echo lc_osd_setting show > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("echo iir_refresh show > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("echo dump_alg_param type_val > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("echo dump_cfg_param type_val > /sys/class/aml_vpp/vpp_lc\n");
	PR_DRV("echo dump_tune_param type_val > /sys/class/aml_vpp/vpp_lc\n");

	return 0;
}

ssize_t vpp_debug_lc_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	int i = 0;
	int len = 0;
	int lut_data[63] = {0};
	long val = 0;
	char *buf_org;
	char *param[8];
	enum lc_curve_tune_mode_e tune_mode;
	struct vpp_lc_param_s *pdata = NULL;

	if (!vpp_module_lc_get_support()) {
		PR_DRV("No support local contrast.\n");
		return count;
	}

	buf_org = kstrdup(buf, GFP_KERNEL);
	if (!buf_org)
		return -ENOMEM;

	_parse_param(buf_org, param);

	PR_DRV("[%s] param[0] = %s, param[1] = %s\n",
		__func__, param[0], param[1]);

	if (!strcmp(param[0], "lc")) {
		if (!strcmp(param[1], "enable"))
			vpp_module_lc_en(true);
		else if (!strcmp(param[1], "disable"))
			vpp_module_lc_en(false);
	} else if (!strcmp(param[0], "lc_demo_mode")) {
		if (!strcmp(param[1], "enable"))
			vpp_module_lc_set_demo_mode(true, true);
		else if (!strcmp(param[1], "disable"))
			vpp_module_lc_set_demo_mode(false, true);
	} else if (!strcmp(param[0], "lc_curve_isr")) {
		if (kstrtoul(param[1], 10, &val) < 0)
			goto free_buf;
		vpp_module_lc_set_isr_use(val);
	} else if (!strcmp(param[0], "lc_wr_lut")) {
		if (kstrtoul(param[1], 16, &val) < 0)
			goto free_buf;
		lc_lut_type = val;
		if (lc_lut_type > EN_LC_MAX)
			goto free_buf;

		if (!param[2])
			goto free_buf;
		_str_sapr_to_d(param[2], lut_data, 5);

		vpp_module_lc_write_lut(lc_lut_type, &lut_data[0]);
		PR_DRV("[%s] set lut %d data.\n", __func__, lc_lut_type);
	} else if (!strcmp(param[0], "dump_lut_data")) {
		if (kstrtoul(param[1], 16, &val) < 0)
			goto free_buf;
		lc_lut_type = val;
		if (lc_lut_type > EN_LC_MAX)
			goto free_buf;

		len = vpp_module_lc_read_lut(lc_lut_type,
			&lut_data[0], sizeof(lut_data));
		if (len == 0)
			goto free_buf;

		for (i = 0; i < len; i++)
			PR_DRV("lut[%d] = %4d.\n", i, lut_data[i]);
	} else if (!strcmp(param[0], "get_blk_region")) {
		if (!strcmp(param[1], "htotal")) {
			lc_dbg_rd_param = vpp_module_lc_get_cfg_param(EN_LC_BLK_HNUM);
			lc_dbg_rd_flag |= LC_DBG_RD_UPDATE_PARAM;
		} else if (!strcmp(param[1], "vtotal")) {
			lc_dbg_rd_param = vpp_module_lc_get_cfg_param(EN_LC_BLK_VNUM);
			lc_dbg_rd_flag |= LC_DBG_RD_UPDATE_PARAM;
		} else {
			PR_DRV("[%s] error cmd\n", __func__);
		}
	} else if (!strcmp(param[0], "dump_lut_str")) {
		if (kstrtoul(param[1], 16, &val) < 0)
			goto free_buf;
		lc_lut_type = val;
		if (lc_lut_type > EN_LC_MAX)
			goto free_buf;

		lc_dbg_rd_flag |= LC_DBG_RD_UPDATE_CV;
	} else if (!strcmp(param[0], "lc_osd_setting")) {
		if (!strcmp(param[1], "show")) {
			PR_DRV("VNUM_STRT_BELOW: %d, VNUM_END_BELOW: %d\n",
				vpp_module_lc_get_alg_param(EN_LC_VNUM_START_BELOW),
				vpp_module_lc_get_alg_param(EN_LC_VNUM_END_BELOW));
			PR_DRV("VNUM_STRT_ABOVE: %d, VNUM_END_ABOVE: %d\n",
				vpp_module_lc_get_alg_param(EN_LC_VNUM_START_ABOVE),
				vpp_module_lc_get_alg_param(EN_LC_VNUM_END_ABOVE));
			PR_DRV("INVALID_BLK: %d, MIN_BV_PERCENT_TH: %d\n",
				vpp_module_lc_get_alg_param(EN_LC_INVALID_BLK),
				vpp_module_lc_get_alg_param(EN_LC_MIN_BV_PERCENT_TH));
		} else if (!strcmp(param[1], "set")) {
			if (!param[7])
				goto free_buf;

			if (kstrtoul(param[2], 10, &val) < 0)
				goto free_buf;
			vpp_module_lc_set_alg_param(EN_LC_VNUM_START_BELOW, val);

			if (kstrtoul(param[3], 10, &val) < 0)
				goto free_buf;
			vpp_module_lc_set_alg_param(EN_LC_VNUM_END_BELOW, val);

			if (kstrtoul(param[4], 10, &val) < 0)
				goto free_buf;
			vpp_module_lc_set_alg_param(EN_LC_VNUM_START_ABOVE, val);

			if (kstrtoul(param[5], 10, &val) < 0)
				goto free_buf;
			vpp_module_lc_set_alg_param(EN_LC_VNUM_END_ABOVE, val);

			if (kstrtoul(param[6], 10, &val) < 0)
				goto free_buf;
			vpp_module_lc_set_alg_param(EN_LC_INVALID_BLK, val);

			if (kstrtoul(param[7], 10, &val) < 0)
				goto free_buf;
			vpp_module_lc_set_alg_param(EN_LC_MIN_BV_PERCENT_TH, val);
		} else {
			PR_DRV("[%s] error cmd\n", __func__);
		}
	} else if (!strcmp(param[0], "osd_iir_en")) {
		if (kstrtoul(param[1], 10, &val) < 0)
			goto free_buf;
		vpp_module_lc_set_alg_param(EN_LC_OSD_IIR_EN, val);
	} else if (!strcmp(param[0], "iir_refresh")) {
		if (!strcmp(param[1], "show")) {
			PR_DRV("current state: alpha1: %d, alpha2: %d, refresh_bit: %d, ts: %d\n",
				vpp_module_lc_get_alg_param(EN_LC_ALPHA1),
				vpp_module_lc_get_alg_param(EN_LC_ALPHA2),
				vpp_module_lc_get_alg_param(EN_LC_REFRESH_BIT),
				vpp_module_lc_get_alg_param(EN_LC_TS));
		} else if (!strcmp(param[1], "set")) {
			if (!param[5])
				goto free_buf;

			if (kstrtoul(param[2], 10, &val) < 0)
				goto free_buf;
			vpp_module_lc_set_alg_param(EN_LC_ALPHA1, val);

			if (kstrtoul(param[3], 10, &val) < 0)
				goto free_buf;
			vpp_module_lc_set_alg_param(EN_LC_ALPHA2, val);

			if (kstrtoul(param[4], 10, &val) < 0)
				goto free_buf;
			vpp_module_lc_set_alg_param(EN_LC_REFRESH_BIT, val);

			if (kstrtoul(param[5], 10, &val) < 0)
				goto free_buf;
			vpp_module_lc_set_alg_param(EN_LC_TS, val);
		}  else {
			PR_DRV("[%s] error cmd\n", __func__);
		}
	} else if (!strcmp(param[0], "scene_change_th")) {
		PR_DRV("current value: %d\n",
			vpp_module_lc_get_alg_param(EN_LC_SCENE_CHANGE_TH));

		if (kstrtoul(param[1], 10, &val) < 0)
			goto free_buf;
		vpp_module_lc_set_alg_param(EN_LC_SCENE_CHANGE_TH, val);
	} else if (!strcmp(param[0], "detect_signal_range_en")) {
		if (kstrtoul(param[1], 10, &val) < 0)
			goto free_buf;
		vpp_module_lc_set_alg_param(EN_LC_DET_RANGE_MODE, val);
	} else if (!strcmp(param[0], "detect_signal_range_threshold")) {
		if (kstrtoul(param[1], 10, &val) < 0)
			goto free_buf;
		vpp_module_lc_set_alg_param(EN_LC_DET_RANGE_TH_BLK, val);

		if (kstrtoul(param[2], 10, &val) < 0)
			goto free_buf;
		vpp_module_lc_set_alg_param(EN_LC_DET_RANGE_TH_WHT, val);
	} else if (!strcmp(param[0], "reg_lmtrat_sigbin")) {
		if (kstrtoul(param[1], 10, &val) < 0)
			goto free_buf;
		vpp_module_lc_set_tune_param(EN_LC_LMTRAT_SIGBIN, val);
	} else if (!strcmp(param[0], "reg_lmtrat_thd_max")) {
		if (kstrtoul(param[1], 10, &val) < 0)
			goto free_buf;
		vpp_module_lc_set_tune_param(EN_LC_LMTRAT_THD_MAX, val);
	} else if (!strcmp(param[0], "reg_lmtrat_thd_black")) {
		if (kstrtoul(param[1], 10, &val) < 0)
			goto free_buf;
		vpp_module_lc_set_tune_param(EN_LC_LMTRAT_THD_BLACK, val);
	} else if (!strcmp(param[0], "reg_thd_black")) {
		if (kstrtoul(param[1], 10, &val) < 0)
			goto free_buf;
		vpp_module_lc_set_tune_param(EN_LC_THD_BLACK, val);
	} else if (!strcmp(param[0], "ypkbv_black_thd")) {
		if (kstrtoul(param[1], 10, &val) < 0)
			goto free_buf;
		vpp_module_lc_set_tune_param(EN_LC_YPKBV_BLACK_THD, val);
	} else if (!strcmp(param[0], "yminv_black_thd")) {
		if (kstrtoul(param[1], 10, &val) < 0)
			goto free_buf;
		vpp_module_lc_set_tune_param(EN_LC_YMINV_BLACK_THD, val);
	} else if (!strcmp(param[0], "tune_curve_en")) {
		if (kstrtoul(param[1], 10, &val) < 0)
			goto free_buf;
		if (val == 1)
			tune_mode = EN_LC_TUNE_LINEAR;
		else if (val == 2)
			tune_mode = EN_LC_TUNE_CORRECT;
		else
			tune_mode = EN_LC_TUNE_OFF;
		vpp_module_lc_set_tune_mode(tune_mode);
	} else if (!strcmp(param[0], "set_alg_param")) {
		if (kstrtoul(param[1], 10, &val) < 0)
			goto free_buf;
		lc_alg_type = val;
		if (lc_alg_type > EN_LC_ALG_PARAM_MAX)
			goto free_buf;

		if (kstrtoul(param[2], 10, &val) < 0)
			goto free_buf;
		vpp_module_lc_set_alg_param(lc_alg_type, val);
	} else if (!strcmp(param[0], "set_cfg_param")) {
		if (kstrtoul(param[1], 10, &val) < 0)
			goto free_buf;
		lc_cfg_type = val;
		if (lc_cfg_type > EN_LC_CFG_PARAM_MAX)
			goto free_buf;

		if (kstrtoul(param[2], 10, &val) < 0)
			goto free_buf;
		vpp_module_lc_set_cfg_param(lc_cfg_type, val);
	} else if (!strcmp(param[0], "set_tune_param")) {
		if (kstrtoul(param[1], 10, &val) < 0)
			goto free_buf;
		lc_tune_type = val;
		if (lc_tune_type > EN_LC_CURVE_TUNE_PARAM_MAX)
			goto free_buf;

		if (kstrtoul(param[2], 10, &val) < 0)
			goto free_buf;
		vpp_module_lc_set_tune_param(lc_tune_type, val);
	} else if (!strcmp(param[0], "dump_alg_param")) {
		if (kstrtoul(param[1], 10, &val) < 0)
			goto free_buf;
		lc_alg_type = val;
		if (lc_alg_type > EN_LC_ALG_PARAM_MAX)
			goto free_buf;
		PR_DRV("alg_param[%d] = %d\n", lc_alg_type,
			vpp_module_lc_get_alg_param(lc_alg_type));
	} else if (!strcmp(param[0], "dump_cfg_param")) {
		if (kstrtoul(param[1], 10, &val) < 0)
			goto free_buf;
		lc_cfg_type = val;
		if (lc_cfg_type > EN_LC_CFG_PARAM_MAX)
			goto free_buf;
		PR_DRV("cfg_param[%d] = %d\n", lc_cfg_type,
			vpp_module_lc_get_cfg_param(lc_cfg_type));
	} else if (!strcmp(param[0], "dump_tune_param")) {
		if (kstrtoul(param[1], 10, &val) < 0)
			goto free_buf;
		lc_tune_type = val;
		if (lc_tune_type > EN_LC_CURVE_TUNE_PARAM_MAX)
			goto free_buf;
		PR_DRV("tune_param[%d] = %d\n", lc_tune_type,
			vpp_module_lc_get_tune_param(lc_tune_type));
	} else if (!strcmp(param[0], "set_lc_param")) {
		if (kstrtoul(param[1], 10, &val) < 0)
			goto free_buf;

		len = sizeof(struct vpp_lc_param_s);
		pdata = kmalloc(len, GFP_KERNEL);
		if (!pdata)
			goto free_buf;
		memset(pdata, 0, len);

		switch (val) {
		case 1:
			pdata->param[EN_LC_CURVE_NODES_VLPF] = 2;
			pdata->param[EN_LC_CURVE_NODES_HLPF] = 3;
			pdata->param[EN_LC_LMT_RAT_VALID] = 32;
			pdata->param[EN_LC_LMT_RAT_MIN_MAX] = 8;
			pdata->param[EN_LC_CONTRAST_GAIN_HIGH] = 32;
			pdata->param[EN_LC_CONTRAST_GAIN_LOW] = 16;
			pdata->param[EN_LC_CONTRAST_LMT_HIGH_1] = 255;
			pdata->param[EN_LC_CONTRAST_LMT_LOW_1] = 255;
			pdata->param[EN_LC_CONTRAST_LMT_HIGH_0] = 32;
			pdata->param[EN_LC_CONTRAST_LMT_LOW_0] = 16;
			pdata->param[EN_LC_CONTRAST_SCALE_HIGH] = 32;
			pdata->param[EN_LC_CONTRAST_SCALE_LOW] = 16;
			pdata->param[EN_LC_CONTRAST_BVN_HIGH] = 32;
			pdata->param[EN_LC_CONTRAST_BVN_LOW] = 16;
			pdata->param[EN_LC_SLOPE_MAX_FACE] = 64;
			pdata->param[EN_LC_NUM_M_CORING] = 4;
			pdata->param[EN_LC_YPKBV_SLOPE_MAX] = 96;
			pdata->param[EN_LC_YPKBV_SLOPE_MIN] = 48;
			break;
		default:
			break;
		}

		vpp_pq_mgr_set_lc_param(pdata);
		kfree(pdata);
		PR_DRV("set_lc_param done.\n");
	} else {
		PR_DRV("[%s] error cmd\n", __func__);
	}

	kfree(buf_org);
	return count;
free_buf:
	kfree(buf_org);
	return -EINVAL;
}

ssize_t vpp_debug_hdr_type_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	enum vpp_hdr_type_e val = EN_TYPE_NONE;

	val = vpp_pq_mgr_get_hdr_type();

	return sprintf(buf, "%x\n", val);
}

ssize_t vpp_debug_hdr_type_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	return count;
}

ssize_t vpp_debug_pc_mode_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	int val = 0;

	val = vpp_pq_mgr_get_pc_mode();

	return sprintf(buf, "%d\n", val);
}

ssize_t vpp_debug_pc_mode_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1)
		return -EINVAL;

	vpp_pq_mgr_set_pc_mode(val);

	PR_DRV("[%s] set value = %d\n", __func__, val);

	return count;
}

ssize_t vpp_debug_csc_type_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	enum vpp_csc_type_e val = EN_CSC_MATRIX_NULL;
	enum vpp_vd_path_e vd_path = EN_VD1_PATH;

	val = vpp_vf_get_csc_type(vd_path);

	return sprintf(buf, "%x\n", val);
}

ssize_t vpp_debug_csc_type_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1)
		return -EINVAL;

	vpp_pq_mgr_set_csc_type(val);

	PR_DRV("[%s] set value = %d\n", __func__, val);

	return count;
}

ssize_t vpp_debug_color_primary_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	enum vpp_color_primary_e val = EN_COLOR_PRI_NULL;

	val = vpp_pq_mgr_get_color_primary();

	return sprintf(buf, "%x\n", val);
}

ssize_t vpp_debug_color_primary_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	return count;
}

ssize_t vpp_debug_histogram_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct vpp_histgm_param_s *pdata = NULL;
	unsigned int buf_size;
	int i = 0;

	buf_size = sizeof(struct vpp_histgm_param_s);
	pdata = kmalloc(buf_size, GFP_KERNEL);
	if (!pdata)
		return 0;

	vpp_pq_mgr_get_histogram(pdata);

	pr_info("\n\t dump hist_pow/luma_sum/pixel_sum\n");
	pr_info("%d/%d/%d\n",
		pdata->hist_pow, pdata->luma_sum, pdata->pixel_sum);

	pr_info("\n\t dump_dnlp_hist begin\n");
	for (i = 0; i < VPP_HIST_BIN_COUNT; i++) {
		pr_info("[%d]0x%-8x\t", i, pdata->histgm[i]);
		if ((i + 1) % 8 == 0)
			pr_info("\n");
	}
	pr_info("\n\t dump_dnlp_hist done\n");

	pr_info("\n\t dump_hue_hist begin\n");
	for (i = 0; i < VPP_COLOR_HIST_BIN_COUNT; i++) {
		pr_info("[%d]0x%-8x\t", i, pdata->hue_histgm[i]);
		if ((i + 1) % 8 == 0)
			pr_info("\n");
	}
	pr_info("\n\t dump_hue_hist done\n");

	pr_info("\n\t dump_sat_hist begin\n");
	for (i = 0; i < VPP_COLOR_HIST_BIN_COUNT; i++) {
		pr_info("[%d]0x%-8x\t", i, pdata->sat_histgm[i]);
		if ((i + 1) % 8 == 0)
			pr_info("\n");
	}
	pr_info("\n\t dump_sat_hist done\n");

	if (vpp_module_meter_get_dark_hist_support()) {
		pr_info("\n\t dump_dark_hist begin\n");
		for (i = 0; i < VPP_HIST_BIN_COUNT; i++) {
			pr_info("[%d]0x%-8x\t", i, pdata->dark_histgm[i]);
			if ((i + 1) % 8 == 0)
				pr_info("\n");
		}
		pr_info("\n\t dump_dark_hist done\n");
	} else {
		pr_info("\n\t No support dark_hist\n");
	}

	kfree(pdata);
	return 0;
}

ssize_t vpp_debug_histogram_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	return count;
}

ssize_t vpp_debug_histogram_ave_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct vpp_histgm_ave_s data;

	vpp_pq_mgr_get_histogram_ave(&data);

	pr_info("\n\t dump sum/width/height/ave\n");
	pr_info("%d/%d/%d/%d\n",
		data.sum, data.width, data.height, data.ave);

	return 0;
}

ssize_t vpp_debug_histogram_ave_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	return count;
}

ssize_t vpp_debug_histogram_hdr_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct vpp_hdr_histgm_param_s *pdata = NULL;
	unsigned int buf_size;
	int i = 0;

	buf_size = sizeof(struct vpp_hdr_histgm_param_s);
	pdata = kmalloc(buf_size, GFP_KERNEL);
	if (!pdata)
		return 0;

	vpp_pq_mgr_get_hdr_histogram(pdata);

	pr_info("\n\t dump_hdr_hist begin\n");
	for (i = 0; i < VPP_HDR_HIST_BIN_COUNT; i++) {
		pr_info("[%d]0x%-8x\t", i, pdata->data_rgb_max[i]);
		if ((i + 1) % 8 == 0)
			pr_info("\n");
	}
	pr_info("\n\t dump_hdr_hist done\n");

	kfree(pdata);
	return 0;
}

ssize_t vpp_debug_histogram_hdr_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	return count;
}

ssize_t vpp_debug_hdr_metadata_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct vpp_hdr_metadata_s data;

	vpp_pq_mgr_get_hdr_metadata(&data);

	pr_info("\n\t dump hdr_metadata begin\n");
	pr_info("primaries:\n");
	pr_info("%d/%d\n", data.primaries[0][0], data.primaries[0][1]);
	pr_info("%d/%d\n", data.primaries[1][0], data.primaries[1][1]);
	pr_info("%d/%d\n", data.primaries[2][0], data.primaries[2][1]);

	pr_info("white point:\n");
	pr_info("%d/%d\n", data.white_point[0], data.white_point[1]);

	pr_info("luminance:\n");
	pr_info("%d/%d\n", data.luminance[0], data.luminance[1]);
	pr_info("\n\t hdr_metadata done\n");

	return 0;
}

ssize_t vpp_debug_hdr_metadata_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	return count;
}

ssize_t vpp_debug_matrix_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	PR_DRV("echo value0~16 > /sys/class/aml_vpp/vpp_matrix\n");
	PR_DRV("--> value0:matrix_mode, value1~3:pre-offset\n");
	PR_DRV("--> value4~12:coef, value13~15:post-offset\n");
	PR_DRV("--> value16:right_shift\n");

	return 0;
}

ssize_t vpp_debug_matrix_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	int parsed[17] = {0};
	enum matrix_mode_e mode;
	struct vpp_mtrx_param_s *pdata = NULL;
	unsigned int buf_size;
	int i = 0;

	if (likely(_parse_param_int(buf, 17, parsed) != 17))
		return -EINVAL;

	PR_DRV("[%s] set type value = %d\n", __func__, parsed[0]);
	PR_DRV("[%s] set pre-offset value = %d/%d/%d\n",
		__func__, parsed[1], parsed[2], parsed[3]);
	PR_DRV("[%s] set coef value =\n %d/%d/%d\n%d/%d/%d\n%d/%d/%d\n",
		__func__, parsed[4], parsed[5], parsed[6],
		parsed[7], parsed[8], parsed[9],
		parsed[10], parsed[11], parsed[12]);
	PR_DRV("[%s] set post-offset value = %d/%d/%d\n",
		__func__, parsed[13], parsed[14], parsed[15]);
	PR_DRV("[%s] set right_shift value = %d\n", __func__, parsed[16]);

	if (parsed[0] >= EN_MTRX_MODE_MAX)
		return count;

	buf_size = sizeof(struct vpp_mtrx_param_s);
	pdata = kmalloc(buf_size, GFP_KERNEL);
	if (!pdata)
		return count;

	mode = parsed[0];

	for (i = 0; i < VPP_MTRX_OFFSET_LEN; i++) {
		pdata->pre_offset[i] = parsed[1 + i];
		pdata->post_offset[i] = parsed[13 + i];
	}

	for (i = 0; i < VPP_MTRX_COEF_LEN; i++)
		pdata->matrix_coef[i] = parsed[4 + i];

	pdata->right_shift = parsed[16];

	vpp_module_matrix_rs(mode, pdata->right_shift);
	vpp_module_matrix_set_pre_offset(mode, &pdata->pre_offset[0]);
	vpp_module_matrix_set_offset(mode, &pdata->post_offset[0]);
	vpp_module_matrix_set_coef_3x3(mode, &pdata->matrix_coef[0]);

	kfree(pdata);
	return count;
}

ssize_t vpp_debug_eye_protect_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	PR_DRV("echo value0~3 > /sys/class/aml_vpp/vpp_eye_protect\n");
	PR_DRV("--> value0:0/1=enable/disable\n");
	PR_DRV("--> value1~3:r/g/b_gain\n");

	return 0;
}

ssize_t vpp_debug_eye_protect_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	int parsed[4] = {0};
	struct vpp_eye_protect_s data;
	int i = 0;

	if (likely(_parse_param_int(buf, 4, parsed) != 4))
		return -EINVAL;

	PR_DRV("[%s] set value = %d/%d/%d/%d\n",
		__func__, parsed[0], parsed[1], parsed[2], parsed[3]);

	if (parsed[0] > 0)
		data.enable = true;
	else
		data.enable = false;

	for (i = 0; i < EN_MODE_RGB_MAX; i++)
		data.rgb[i] = parsed[1 + i];

	vpp_pq_mgr_set_eye_protect(&data);

	return count;
}

ssize_t vpp_debug_color_curve_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	PR_DRV("echo type index is_offset > /sys/class/aml_vpp/vpp_color_curve\n");
	PR_DRV("--> type:0~3=luma/sat/hue/hue_by_hs\n");
	PR_DRV("--> index:0/1/2=bypass/add_p5x/sub_p5x\n");
	PR_DRV("--> is_offset:0/1=curve_data/offset_data\n");
	PR_DRV("echo dump_cm2_curve type > /sys/class/aml_vpp/vpp_color_curve\n");
	PR_DRV("--> type:0~6=luma/sat/sat_by_l/sat_by_hl/hue/hue_by_hs/hue_by_hl\n");

	return 0;
}

ssize_t vpp_debug_color_curve_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	char *buf_org;
	char *param[2];
	long val;
	int len;
	int parsed[3] = {0};
	int *ptmp = NULL;
	char *ptmp_char = NULL;
	int i = 0;
	unsigned int buf_size;
	enum vpp_cm_curve_type_e type;

	buf_org = kstrdup(buf, GFP_KERNEL);
	if (!buf_org)
		return -ENOMEM;

	_parse_param(buf_org, param);

	PR_DRV("[%s] param[0] = %s, param[1] = %s\n",
		__func__, param[0], param[1]);

	if (!strcmp(param[0], "dump_cm2_curve")) {
		if (kstrtoul(param[1], 10, &val) < 0)
			goto free_buf;

		switch (val) {
		case 0:
			buf_size = CM2_CURVE_SIZE * sizeof(int);
			ptmp = kmalloc(buf_size, GFP_KERNEL);
			if (!ptmp)
				goto free_buf;
			memset(ptmp, 0, buf_size);

			len = vpp_module_cm_get_cm2_luma(ptmp);
			if (len == 0)
				goto free_buf;
			break;
		case 1:
			buf_size = CM2_CURVE_SIZE * 3 * sizeof(int);
			ptmp = kmalloc(buf_size, GFP_KERNEL);
			if (!ptmp)
				goto free_buf;
			memset(ptmp, 0, buf_size);

			len = vpp_module_cm_get_cm2_sat(ptmp);
			if (len == 0)
				goto free_buf;
			break;
		case 2:
			buf_size = 9 * sizeof(int);
			ptmp = kmalloc(buf_size, GFP_KERNEL);
			if (!ptmp)
				goto free_buf;
			memset(ptmp, 0, buf_size);

			len = vpp_module_cm_get_cm2_sat_by_l(ptmp);
			if (len == 0)
				goto free_buf;
			break;
		case 3:
			buf_size = CM2_CURVE_SIZE * 5 * sizeof(int);
			ptmp = kmalloc(buf_size, GFP_KERNEL);
			if (!ptmp)
				goto free_buf;
			memset(ptmp, 0, buf_size);

			len = vpp_module_cm_get_cm2_sat_by_hl(ptmp);
			if (len == 0)
				goto free_buf;
			break;
		case 4:
			buf_size = CM2_CURVE_SIZE * sizeof(int);
			ptmp = kmalloc(buf_size, GFP_KERNEL);
			if (!ptmp)
				goto free_buf;
			memset(ptmp, 0, buf_size);

			len = vpp_module_cm_get_cm2_hue(ptmp);
			if (len == 0)
				goto free_buf;
			break;
		case 5:
			buf_size = CM2_CURVE_SIZE * 5 * sizeof(int);
			ptmp = kmalloc(buf_size, GFP_KERNEL);
			if (!ptmp)
				goto free_buf;
			memset(ptmp, 0, buf_size);

			len = vpp_module_cm_get_cm2_hue_by_hs(ptmp);
			if (len == 0)
				goto free_buf;
			break;
		case 6:
			buf_size = CM2_CURVE_SIZE * 5 * sizeof(int);
			ptmp = kmalloc(buf_size, GFP_KERNEL);
			if (!ptmp)
				goto free_buf;
			memset(ptmp, 0, buf_size);

			len = vpp_module_cm_get_cm2_hue_by_hl(ptmp);
			if (len == 0)
				goto free_buf;
			break;
		default:
			goto free_buf;
		}

		for (i = 0; i < len; i++)
			PR_DRV("curve[%d] = %4d.\n", i, ptmp[i]);

		kfree(ptmp);
	} else {
		if (likely(_parse_param_int(buf, 3, parsed) != 3))
			goto free_buf;

		PR_DRV("[%s] set value = %d/%d/%d\n",
			__func__, parsed[0], parsed[1], parsed[2]);

		switch (parsed[0]) {
		case 0:
			type = EN_CM_LUMA;
			if (!parsed[2]) {
				buf_size = CM2_CURVE_SIZE * sizeof(int);
				ptmp = kmalloc(buf_size, GFP_KERNEL);
				if (!ptmp)
					goto free_buf;
				memset(ptmp, 0, buf_size);

				if (parsed[1] == 1) {
					ptmp[5] = 100;
					ptmp[10] = 100;
					ptmp[15] = 100;
					ptmp[20] = 100;
				} else if (parsed[1] == 2) {
					ptmp[5] = -100;
					ptmp[10] = -100;
					ptmp[15] = -100;
					ptmp[20] = -100;
				}
			} else {
				buf_size = CM2_CURVE_SIZE * sizeof(char);
				ptmp_char = kmalloc(buf_size, GFP_KERNEL);
				if (!ptmp_char)
					goto free_buf;
				memset(ptmp_char, 0x0, buf_size);

				if (parsed[1] == 1) {
					ptmp_char[5] = 80;
					ptmp_char[10] = 80;
					ptmp_char[15] = 80;
					ptmp_char[20] = 80;
				} else if (parsed[1] == 2) {
					ptmp_char[5] = -80;
					ptmp_char[10] = -80;
					ptmp_char[15] = -80;
					ptmp_char[20] = -80;
				}
			}
			break;
		case 1:
			type = EN_CM_SAT;
			if (!parsed[2]) {
				buf_size = CM2_CURVE_SIZE * 3 * sizeof(int);
				ptmp = kmalloc(buf_size, GFP_KERNEL);
				if (!ptmp)
					goto free_buf;
				memset(ptmp, 0, buf_size);

				if (parsed[1] == 1) {
					for (i = 0; i < 3; i++) {
						ptmp[5 + 32 * i] = 100;
						ptmp[10 + 32 * i] = 100;
						ptmp[15 + 32 * i] = 100;
						ptmp[20 + 32 * i] = 100;
					}
				} else if (parsed[1] == 2) {
					for (i = 0; i < 3; i++) {
						ptmp[5 + 32 * i] = -100;
						ptmp[10 + 32 * i] = -100;
						ptmp[15 + 32 * i] = -100;
						ptmp[20 + 32 * i] = -100;
					}
				}
			} else {
				buf_size = CM2_CURVE_SIZE * 3 * sizeof(char);
				ptmp_char = kmalloc(buf_size, GFP_KERNEL);
				if (!ptmp_char)
					goto free_buf;
				memset(ptmp_char, 0x0, buf_size);

				if (parsed[1] == 1) {
					for (i = 0; i < 3; i++) {
						ptmp_char[5 + 32 * i] = 80;
						ptmp_char[10 + 32 * i] = 80;
						ptmp_char[15 + 32 * i] = 80;
						ptmp_char[20 + 32 * i] = 80;
					}
				} else if (parsed[1] == 2) {
					for (i = 0; i < 3; i++) {
						ptmp_char[5 + 32 * i] = -80;
						ptmp_char[10 + 32 * i] = -80;
						ptmp_char[15 + 32 * i] = -80;
						ptmp_char[20 + 32 * i] = -80;
					}
				}
			}
			break;
		case 2:
			type = EN_CM_HUE;
			if (!parsed[2]) {
				buf_size = CM2_CURVE_SIZE * sizeof(int);
				ptmp = kmalloc(buf_size, GFP_KERNEL);
				if (!ptmp)
					goto free_buf;
				memset(ptmp, 0, buf_size);

				if (parsed[1] == 1) {
					ptmp[5] = 100;
					ptmp[10] = 100;
					ptmp[15] = 100;
					ptmp[20] = 100;
				} else if (parsed[1] == 2) {
					ptmp[5] = -100;
					ptmp[10] = -100;
					ptmp[15] = -100;
					ptmp[20] = -100;
				}
			} else {
				buf_size = CM2_CURVE_SIZE * sizeof(char);
				ptmp_char = kmalloc(buf_size, GFP_KERNEL);
				if (!ptmp_char)
					goto free_buf;
				memset(ptmp_char, 0x0, buf_size);

				if (parsed[1] == 1) {
					ptmp_char[5] = 80;
					ptmp_char[10] = 80;
					ptmp_char[15] = 80;
					ptmp_char[20] = 80;
				} else if (parsed[1] == 2) {
					ptmp_char[5] = -80;
					ptmp_char[10] = -80;
					ptmp_char[15] = -80;
					ptmp_char[20] = -80;
				}
			}
			break;
		case 3:
			type = EN_CM_HUE_BY_HS;
			if (!parsed[2]) {
				buf_size = CM2_CURVE_SIZE * 5 * sizeof(int);
				ptmp = kmalloc(buf_size, GFP_KERNEL);
				if (!ptmp)
					goto free_buf;
				memset(ptmp, 0, buf_size);

				if (parsed[1] == 1) {
					for (i = 0; i < 5; i++) {
						ptmp[5 + 32 * i] = 100;
						ptmp[10 + 32 * i] = 100;
						ptmp[15 + 32 * i] = 100;
						ptmp[20 + 32 * i] = 100;
					}
				} else if (parsed[1] == 2) {
					for (i = 0; i < 5; i++) {
						ptmp[5 + 32 * i] = -100;
						ptmp[10 + 32 * i] = -100;
						ptmp[15 + 32 * i] = -100;
						ptmp[20 + 32 * i] = -100;
					}
				}
			} else {
				buf_size = CM2_CURVE_SIZE * 5 * sizeof(char);
				ptmp_char = kmalloc(buf_size, GFP_KERNEL);
				if (!ptmp_char)
					goto free_buf;
				memset(ptmp_char, 0x0, buf_size);

				if (parsed[1] == 1) {
					for (i = 0; i < 5; i++) {
						ptmp_char[5 + 32 * i] = 80;
						ptmp_char[10 + 32 * i] = 80;
						ptmp_char[15 + 32 * i] = 80;
						ptmp_char[20 + 32 * i] = 80;
					}
				} else if (parsed[1] == 2) {
					for (i = 0; i < 5; i++) {
						ptmp_char[5 + 32 * i] = -80;
						ptmp_char[10 + 32 * i] = -80;
						ptmp_char[15 + 32 * i] = -80;
						ptmp_char[20 + 32 * i] = -80;
					}
				}
			}
			break;
		default:
			return count;
		}

		if (!parsed[2]) {
			vpp_pq_mgr_set_cm_curve(type, ptmp);
			kfree(ptmp);
		} else {
			vpp_pq_mgr_set_cm_offset_curve(type, ptmp_char);
			kfree(ptmp_char);
		}
	}

	kfree(buf_org);
	return count;
free_buf:
	kfree(buf_org);
	return -EINVAL;
}

ssize_t vpp_debug_overscan_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	vpp_vf_dump_data(EN_DUMP_DATA_OVERSCAN);

	return 0;
}

ssize_t vpp_debug_overscan_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	return count;
}

