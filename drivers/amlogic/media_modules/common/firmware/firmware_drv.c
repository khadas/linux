/*
 * drivers/amlogic/media/common/firmware/firmware.c
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/slab.h>

#include <linux/amlogic/media/utils/vformat.h>
#include <linux/amlogic/cpu_version.h>
#include "../../stream_input/amports/amports_priv.h"
#include "../../frame_provider/decoder/utils/vdec.h"
#include "firmware_priv.h"
#include "../chips/chips.h"
#include <linux/string.h>
#include <linux/amlogic/media/utils/log.h>
#include <linux/firmware.h>
#include <linux/amlogic/tee.h>
#include <linux/amlogic/major.h>
#include <linux/cdev.h>
#include <linux/crc32.h>
#include "../chips/decoder_cpu_ver_info.h"

/* major.minor */
#define PACK_VERS "v0.1"

#define CLASS_NAME	"firmware_codec"
#define DEV_NAME	"firmware_vdec"
#define DIR		"video"
#define FRIMWARE_SIZE	(64 * 1024) /*64k*/
#define BUFF_SIZE	(1024 * 1024 * 2)

#define FW_LOAD_FORCE	(0x1)
#define FW_LOAD_TRY	(0X2)

/*the first 256 bytes are signature data*/
#define SEC_OFFSET	(256)

#define PACK ('P' << 24 | 'A' << 16 | 'C' << 8 | 'K')
#define CODE ('C' << 24 | 'O' << 16 | 'D' << 8 | 'E')

static DEFINE_MUTEX(mutex);

static  struct ucode_file_info_s ucode_info[] = {
#include "firmware_cfg.h"
};

static const struct file_operations fw_fops = {
	.owner = THIS_MODULE
};

struct fw_mgr_s *g_mgr;
struct fw_dev_s *g_dev;

static u32 debug;
static u32 detail;

int get_firmware_data(unsigned int format, char *buf)
{
	int data_len, ret = -1;
	struct fw_mgr_s *mgr = g_mgr;
	struct fw_info_s *info;

	pr_info("[%s], the fw (%s) will be loaded.\n",
		tee_enabled() ? "TEE" : "LOCAL",
		get_fw_format_name(format));

	if (tee_enabled())
		return 0;

	mutex_lock(&mutex);

	if (list_empty(&mgr->fw_head)) {
		pr_info("the info list is empty.\n");
		goto out;
	}

	list_for_each_entry(info, &mgr->fw_head, node) {
		if (format != info->format)
			continue;

		data_len = info->data->head.data_size;
		memcpy(buf, info->data->data, data_len);
		ret = data_len;

		break;
	}
out:
	mutex_unlock(&mutex);

	return ret;
}
EXPORT_SYMBOL(get_firmware_data);

int get_data_from_name(const char *name, char *buf)
{
	int data_len, ret = -1;
	struct fw_mgr_s *mgr = g_mgr;
	struct fw_info_s *info;
	char *fw_name = __getname();

	if (fw_name == NULL)
		return -ENOMEM;

	strcat(fw_name, name);
	strcat(fw_name, ".bin");

	mutex_lock(&mutex);

	if (list_empty(&mgr->fw_head)) {
		pr_info("the info list is empty.\n");
		goto out;
	}

	list_for_each_entry(info, &mgr->fw_head, node) {
		if (strcmp(fw_name, info->name))
			continue;

		data_len = info->data->head.data_size;
		memcpy(buf, info->data->data, data_len);
		ret = data_len;

		break;
	}
out:
	mutex_unlock(&mutex);

	__putname(fw_name);

	return ret;
}
EXPORT_SYMBOL(get_data_from_name);

static int fw_probe(char *buf)
{
	int magic = 0;

	memcpy(&magic, buf, sizeof(int));
	return magic;
}

static int request_firmware_from_sys(const char *file_name,
		char *buf, int size)
{
	int ret = -1;
	const struct firmware *fw;
	int magic, offset = 0;

	pr_info("Try to load %s  ...\n", file_name);

	ret = request_firmware(&fw, file_name, g_dev->dev);
	if (ret < 0) {
		pr_info("Error : %d can't load the %s.\n", ret, file_name);
		goto err;
	}

	if (fw->size > size) {
		pr_info("Not enough memory size for ucode.\n");
		ret = -ENOMEM;
		goto release;
	}

	magic = fw_probe((char *)fw->data);
	if (magic != PACK && magic != CODE) {
		if (fw->size < SEC_OFFSET) {
			pr_info("This is an invalid firmware file.\n");
			goto release;
		}

		magic = fw_probe((char *)fw->data + SEC_OFFSET);
		if (magic != PACK) {
			pr_info("The firmware file is not packet.\n");
			goto release;
		}

		offset = SEC_OFFSET;
	}

	memcpy(buf, (char *)fw->data + offset, fw->size - offset);

	pr_info("load firmware size : %zd, Name : %s.\n",
		fw->size, file_name);
	ret = fw->size;
release:
	release_firmware(fw);
err:
	return ret;
}

int request_decoder_firmware_on_sys(enum vformat_e format,
	const char *file_name, char *buf, int size)
{
	int ret;

	ret = get_data_from_name(file_name, buf);
	if (ret < 0)
		pr_info("Get firmware fail.\n");

	if (ret > size) {
		pr_info("Not enough memory.\n");
		return -ENOMEM;
	}

	return ret;
}
int get_decoder_firmware_data(enum vformat_e format,
	const char *file_name, char *buf, int size)
{
	int ret;

	ret = request_decoder_firmware_on_sys(format, file_name, buf, size);
	if (ret < 0)
		pr_info("get_decoder_firmware_data %s for format %d failed!\n",
				file_name, format);

	return ret;
}
EXPORT_SYMBOL(get_decoder_firmware_data);

static unsigned long fw_mgr_lock(struct fw_mgr_s *mgr)
{
	unsigned long flags;

	spin_lock_irqsave(&mgr->lock, flags);
	return flags;
}

static void fw_mgr_unlock(struct fw_mgr_s *mgr, unsigned long flags)
{
	spin_unlock_irqrestore(&mgr->lock, flags);
}

static void fw_add_info(struct fw_info_s *info)
{
	unsigned long flags;
	struct fw_mgr_s *mgr = g_mgr;

	flags = fw_mgr_lock(mgr);
	list_add(&info->node, &mgr->fw_head);
	fw_mgr_unlock(mgr, flags);
}

static void fw_del_info(struct fw_info_s *info)
{
	unsigned long flags;
	struct fw_mgr_s *mgr = g_mgr;

	flags = fw_mgr_lock(mgr);
	list_del(&info->node);
	kfree(info);
	fw_mgr_unlock(mgr, flags);
}

static void fw_info_walk(void)
{
	struct fw_mgr_s *mgr = g_mgr;
	struct fw_info_s *info;

	if (list_empty(&mgr->fw_head)) {
		pr_info("the info list is empty.\n");
		return;
	}

	list_for_each_entry(info, &mgr->fw_head, node) {
		if (IS_ERR_OR_NULL(info->data))
			continue;

		pr_info("name : %s.\n", info->name);
		pr_info("ver  : %s.\n",
			info->data->head.version);
		pr_info("crc  : 0x%x.\n",
			info->data->head.checksum);
		pr_info("size : %d.\n",
			info->data->head.data_size);
		pr_info("maker: %s.\n",
			info->data->head.maker);
		pr_info("from : %s.\n", info->src_from);
		pr_info("date : %s.\n\n",
			info->data->head.date);
	}
}

static void fw_files_info_walk(void)
{
	struct fw_mgr_s *mgr = g_mgr;
	struct fw_files_s *files;

	if (list_empty(&mgr->files_head)) {
		pr_info("the file list is empty.\n");
		return;
	}

	list_for_each_entry(files, &mgr->files_head, node) {
		pr_info("type : %s.\n", !files->fw_type ?
			"VIDEO_DECODE" : files->fw_type == 1 ?
			"VIDEO_ENCODE" : "VIDEO_MISC");
		pr_info("from : %s.\n", !files->file_type ?
			"VIDEO_PACKAGE" : "VIDEO_FW_FILE");
		pr_info("path : %s.\n", files->path);
		pr_info("name : %s.\n\n", files->name);
	}
}

static ssize_t info_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;
	struct fw_mgr_s *mgr = g_mgr;
	struct fw_info_s *info;
	unsigned int secs = 0;
	struct tm tm;

	mutex_lock(&mutex);

	if (list_empty(&mgr->fw_head)) {
		pbuf += sprintf(pbuf, "No firmware.\n");
		goto out;
	}

	list_for_each_entry(info, &mgr->fw_head, node) {
		if (IS_ERR_OR_NULL(info->data))
			continue;

		if (detail) {
			pr_info("%-5s: %s\n", "name", info->name);
			pr_info("%-5s: %s\n", "ver",
				info->data->head.version);
			pr_info("%-5s: 0x%x\n", "sum",
				info->data->head.checksum);
			pr_info("%-5s: %d\n", "size",
				info->data->head.data_size);
			pr_info("%-5s: %s\n", "maker",
				info->data->head.maker);
			pr_info("%-5s: %s\n", "from",
				info->src_from);
			pr_info("%-5s: %s\n\n", "date",
				info->data->head.date);
			continue;
		}

		secs = info->data->head.time
			- sys_tz.tz_minuteswest * 60;
		time_to_tm(secs, 0, &tm);

		pr_info("%s %-16s, %02d:%02d:%02d %d/%d/%ld, %s %-8s, %s %-8s, %s %s\n",
			"fmt:", info->data->head.format,
			tm.tm_hour, tm.tm_min, tm.tm_sec,
			tm.tm_mon + 1, tm.tm_mday, tm.tm_year + 1900,
			"cmtid:", info->data->head.commit,
			"chgid:", info->data->head.change_id,
			"mk:", info->data->head.maker);
	}
out:
	mutex_unlock(&mutex);

	return pbuf - buf;
}

static ssize_t info_store(struct class *cls,
	struct class_attribute *attr, const char *buf, size_t count)
{
	if (kstrtoint(buf, 0, &detail) < 0)
		return -EINVAL;

	return count;
}

static int fw_info_fill(void)
{
	int ret = 0, i, len;
	struct fw_mgr_s *mgr = g_mgr;
	struct fw_files_s *files;
	int info_size = ARRAY_SIZE(ucode_info);
	char *path = __getname();
	const char *name;

	if (path == NULL)
		return -ENOMEM;

	for (i = 0; i < info_size; i++) {
		name = ucode_info[i].name;
		if (IS_ERR_OR_NULL(name))
			break;

		len = snprintf(path, PATH_MAX, "%s/%s", DIR,
			ucode_info[i].name);
		if (len >= PATH_MAX)
			continue;

		files = kzalloc(sizeof(struct fw_files_s), GFP_KERNEL);
		if (files == NULL) {
			__putname(path);
			return -ENOMEM;
		}

		files->file_type = ucode_info[i].file_type;
		files->fw_type = ucode_info[i].fw_type;
		strncpy(files->path, path, sizeof(files->path));
		files->path[sizeof(files->path) - 1] = '\0';
		strncpy(files->name, name, sizeof(files->name));
		files->name[sizeof(files->name) - 1] = '\0';

		list_add(&files->node, &mgr->files_head);
	}

	__putname(path);

	if (debug)
		fw_files_info_walk();

	return ret;
}

static int fw_data_check_sum(struct firmware_s *fw)
{
	unsigned int crc;

	crc = crc32_le(~0U, fw->data, fw->head.data_size);

	/*pr_info("firmware crc result : 0x%x\n", crc ^ ~0U);*/

	return fw->head.checksum != (crc ^ ~0U) ? 0 : 1;
}

static int fw_data_filter(struct firmware_s *fw,
	struct fw_info_s *fw_info)
{
	struct fw_mgr_s *mgr = g_mgr;
	struct fw_info_s *info, *tmp;
	int cpu = fw_get_cpu(fw->head.cpu);

	if (mgr->cur_cpu < cpu) {
		kfree(fw_info);
		kfree(fw);
		return -1;
	}

	/* the encode fw need to ignoring filtering rules. */
	if (fw_info->format == FIRMWARE_MAX)
		return 0;

	list_for_each_entry_safe(info, tmp, &mgr->fw_head, node) {
		if (info->format != fw_info->format)
			continue;

		if (IS_ERR_OR_NULL(info->data)) {
			fw_del_info(info);
			return 0;
		}

		/* high priority of VIDEO_FW_FILE */
		if (info->file_type == VIDEO_FW_FILE) {
			pr_info("the %s need to priority proc.\n",info->name);
			kfree(fw_info);
			kfree(fw);
			return 1;
		}

		/* the cpu ver is lower and needs to be filtered */
		if (cpu < fw_get_cpu(info->data->head.cpu)) {
			if (debug)
				pr_info("keep the newer fw (%s) and ignore the older fw (%s).\n",
					info->name, fw_info->name);
			kfree(fw_info);
			kfree(fw);
			return 1;
		}

		/* removes not match fw from info list */
		if (debug)
			pr_info("drop the old fw (%s) will be load the newer fw (%s).\n",
					info->name, fw_info->name);
		kfree(info->data);
		fw_del_info(info);
	}

	return 0;
}

static int fw_check_pack_version(char *buf)
{
	struct package_s *pack = NULL;
	int major, minor, major_fw, minor_fw, ver = 0;
	int ret;

	pack = (struct package_s *) buf;
	ret = sscanf(PACK_VERS, "v%x.%x", &major, &minor);
	if (ret != 2)
		return -1;

	ver = (major << 16 | minor);

	if (debug)
		pr_info("the package has %d fws totally.\n", pack->head.total);

	major_fw = (pack->head.version >> 16) & 0xff;
	minor_fw = pack->head.version & 0xff;

	if (major < major_fw) {
		pr_info("the pack ver v%d.%d too higher to unsupport.\n",
			major_fw, minor_fw);
		return -1;
	}

	if (ver != pack->head.version) {
		pr_info("the fw pack ver v%d.%d is too lower.\n", major_fw, minor_fw);
		pr_info("it may work abnormally so need to be update in time.\n");
	}

	return 0;
}

static int fw_package_parse(struct fw_files_s *files,
	char *buf, int size)
{
	int ret = 0;
	struct package_info_s *pack_info;
	struct fw_info_s *info;
	struct firmware_s *data;
	char *pack_data;
	int info_len, len;
	int try_cnt = 100;
	char *path = __getname();

	if (path == NULL)
		return -ENOMEM;

	pack_data = ((struct package_s *)buf)->data;
	pack_info = (struct package_info_s *)pack_data;
	info_len = sizeof(struct package_info_s);

	do {
		if (!pack_info->head.length)
			break;

		len = snprintf(path, PATH_MAX, "%s/%s", DIR,
			pack_info->head.name);
		if (len >= PATH_MAX)
			continue;

		info = kzalloc(sizeof(struct fw_info_s), GFP_KERNEL);
		if (info == NULL) {
			ret = -ENOMEM;
			goto out;
		}

		data = kzalloc(FRIMWARE_SIZE, GFP_KERNEL);
		if (data == NULL) {
			kfree(info);
			ret = -ENOMEM;
			goto out;
		}

		info->file_type = files->file_type;
		strncpy(info->src_from, files->name,
			sizeof(info->src_from));
		info->src_from[sizeof(info->src_from) - 1] = '\0';
		strncpy(info->name, pack_info->head.name,
			sizeof(info->name));
		info->name[sizeof(info->name) - 1] = '\0';
		info->format = get_fw_format(pack_info->head.format);

		len = pack_info->head.length;
		memcpy(data, pack_info->data, len);

		pack_data += (pack_info->head.length + info_len);
		pack_info = (struct package_info_s *)pack_data;

		if (!fw_data_check_sum(data)) {
			pr_info("check sum fail !\n");
			kfree(data);
			kfree(info);
			goto out;
		}

		if (fw_data_filter(data, info))
			continue;

		if (debug)
			pr_info("adds %s to the fw list.\n", info->name);

		info->data = data;
		fw_add_info(info);
	} while (try_cnt--);
out:
	__putname(path);

	return ret;
}

static int fw_code_parse(struct fw_files_s *files,
	char *buf, int size)
{
	struct fw_info_s *info;

	info = kzalloc(sizeof(struct fw_info_s), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;

	info->data = kzalloc(FRIMWARE_SIZE, GFP_KERNEL);
	if (info->data == NULL) {
		kfree(info);
		return -ENOMEM;
	}

	info->file_type = files->file_type;
	strncpy(info->src_from, files->name,
		sizeof(info->src_from));
	info->src_from[sizeof(info->src_from) - 1] = '\0';
	memcpy(info->data, buf, size);

	if (!fw_data_check_sum(info->data)) {
		pr_info("check sum fail !\n");
		kfree(info->data);
		kfree(info);
		return -1;
	}

	if (debug)
		pr_info("adds %s to the fw list.\n", info->name);

	fw_add_info(info);

	return 0;
}

static int get_firmware_from_sys(const char *path,
	char *buf, int size)
{
	int len = 0;

	len = request_firmware_from_sys(path, buf, size);
	if (len < 0)
		pr_info("get data from fsys fail.\n");

	return len;
}

static int fw_data_binding(void)
{
	int ret = 0, magic = 0;
	struct fw_mgr_s *mgr = g_mgr;
	struct fw_files_s *files, *tmp;
	char *buf = NULL;
	int size;

	if (list_empty(&mgr->files_head)) {
		pr_info("the file list is empty.\n");
		return 0;
	}

	buf = vmalloc(BUFF_SIZE);
	if (IS_ERR_OR_NULL(buf))
		return -ENOMEM;

	memset(buf, 0, BUFF_SIZE);

	list_for_each_entry_safe(files, tmp, &mgr->files_head, node) {
		size = get_firmware_from_sys(files->path, buf, BUFF_SIZE);
		magic = fw_probe(buf);

		if (files->file_type == VIDEO_PACKAGE && magic == PACK) {
			if (!fw_check_pack_version(buf))
				ret = fw_package_parse(files, buf, size);
		} else if (files->file_type == VIDEO_FW_FILE && magic == CODE) {
			ret = fw_code_parse(files, buf, size);
		} else {
			list_del(&files->node);
			kfree(files);
			pr_info("invaild file type.\n");
		}

		memset(buf, 0, BUFF_SIZE);
	}

	if (debug)
		fw_info_walk();

	vfree(buf);

	return ret;
}

static int fw_pre_load(void)
{
	if (fw_info_fill() < 0) {
		pr_info("Get path fail.\n");
		return -1;
	}

	if (fw_data_binding() < 0) {
		pr_info("Set data fail.\n");
		return -1;
	}

	return 0;
}

static int fw_mgr_init(void)
{
	g_mgr = kzalloc(sizeof(struct fw_mgr_s), GFP_KERNEL);
	if (IS_ERR_OR_NULL(g_mgr))
		return -ENOMEM;

	g_mgr->cur_cpu = get_cpu_major_id();
	INIT_LIST_HEAD(&g_mgr->files_head);
	INIT_LIST_HEAD(&g_mgr->fw_head);
	spin_lock_init(&g_mgr->lock);

	return 0;
}

static void fw_ctx_clean(void)
{
	struct fw_mgr_s *mgr = g_mgr;
	struct fw_files_s *files;
	struct fw_info_s *info;
	unsigned long flags;

	flags = fw_mgr_lock(mgr);
	while (!list_empty(&mgr->files_head)) {
		files = list_entry(mgr->files_head.next,
			struct fw_files_s, node);
		list_del(&files->node);
		kfree(files);
	}

	while (!list_empty(&mgr->fw_head)) {
		info = list_entry(mgr->fw_head.next,
			struct fw_info_s, node);
		list_del(&info->node);
		kfree(info->data);
		kfree(info);
	}
	fw_mgr_unlock(mgr, flags);
}

int video_fw_reload(int mode)
{
	int ret = 0;
	struct fw_mgr_s *mgr = g_mgr;

	if (tee_enabled())
		return 0;

	mutex_lock(&mutex);

	if (mode & FW_LOAD_FORCE) {
		fw_ctx_clean();

		ret = fw_pre_load();
		if (ret < 0)
			pr_err("The fw reload fail.\n");
	} else if (mode & FW_LOAD_TRY) {
		if (!list_empty(&mgr->fw_head)) {
			pr_info("The fw has been loaded.\n");
			goto out;
		}

		ret = fw_pre_load();
		if (ret < 0)
			pr_err("The fw try to reload fail.\n");
	}
out:
	mutex_unlock(&mutex);

	return ret;
}
EXPORT_SYMBOL(video_fw_reload);

static ssize_t reload_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;

	pbuf += sprintf(pbuf, "The fw reload usage.\n");
	pbuf += sprintf(pbuf, "> set 1 means that the fw is forced to update\n");
	pbuf += sprintf(pbuf, "> set 2 means that the fw is try to reload\n");

	return pbuf - buf;
}

static ssize_t reload_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	int ret = -1;
	unsigned int val;

	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;

	ret = video_fw_reload(val);
	if (ret < 0)
		pr_err("fw reload fail.\n");

	return size;
}

static ssize_t debug_show(struct class *cls,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", debug);
}

static ssize_t debug_store(struct class *cls,
	struct class_attribute *attr, const char *buf, size_t count)
{
	if (kstrtoint(buf, 0, &debug) < 0)
		return -EINVAL;

	return count;
}

static struct class_attribute fw_class_attrs[] = {
	__ATTR(info, 0664, info_show, info_store),
	__ATTR(reload, 0664, reload_show, reload_store),
	__ATTR(debug, 0664, debug_show, debug_store),
	__ATTR_NULL
};

static struct class fw_class = {
	.name = CLASS_NAME,
	.class_attrs = fw_class_attrs,
};

static int fw_driver_init(void)
{
	int ret = -1;

	g_dev = kzalloc(sizeof(struct fw_dev_s), GFP_KERNEL);
	if (IS_ERR_OR_NULL(g_dev))
		return -ENOMEM;

	g_dev->dev_no = MKDEV(AMSTREAM_MAJOR, 100);

	ret = register_chrdev_region(g_dev->dev_no, 1, DEV_NAME);
	if (ret < 0) {
		pr_info("Can't get major number %d.\n", AMSTREAM_MAJOR);
		goto err;
	}

	cdev_init(&g_dev->cdev, &fw_fops);
	g_dev->cdev.owner = THIS_MODULE;

	ret = cdev_add(&g_dev->cdev, g_dev->dev_no, 1);
	if (ret) {
		pr_info("Error %d adding cdev fail.\n", ret);
		goto err;
	}

	ret = class_register(&fw_class);
	if (ret < 0) {
		pr_info("Failed in creating class.\n");
		goto err;
	}

	g_dev->dev = device_create(&fw_class, NULL,
		g_dev->dev_no, NULL, DEV_NAME);
	if (IS_ERR_OR_NULL(g_dev->dev)) {
		pr_info("Create device failed.\n");
		ret = -ENODEV;
		goto err;
	}

	pr_info("Registered firmware driver success.\n");
err:
	return ret;
}

static void fw_driver_exit(void)
{
	cdev_del(&g_dev->cdev);
	device_destroy(&fw_class, g_dev->dev_no);
	class_unregister(&fw_class);
	unregister_chrdev_region(g_dev->dev_no, 1);
	kfree(g_dev);
	kfree(g_mgr);
}

static int __init fw_module_init(void)
{
	int ret = -1;

	ret = fw_driver_init();
	if (ret) {
		pr_info("Error %d firmware driver init fail.\n", ret);
		goto err;
	}

	ret = fw_mgr_init();
	if (ret) {
		pr_info("Error %d firmware mgr init fail.\n", ret);
		goto err;
	}

	ret = fw_pre_load();
	if (ret) {
		pr_info("Error %d firmware pre load fail.\n", ret);
		goto err;
	}
err:
	return ret;
}

static void __exit fw_module_exit(void)
{
	fw_ctx_clean();
	fw_driver_exit();
	pr_info("Firmware driver cleaned up.\n");
}

module_init(fw_module_init);
module_exit(fw_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nanxin Qin <nanxin.qin@amlogic.com>");
