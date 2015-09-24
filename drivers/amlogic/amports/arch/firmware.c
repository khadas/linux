/*
 * drivers/amlogic/amports/arch/firmware.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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
#include <linux/vmalloc.h>
#include <linux/slab.h>

#include <linux/amlogic/amports/vformat.h>
#include <linux/amlogic/cpu_version.h>
#include "../amports_priv.h"
#include "../vdec.h"
#include "firmware.h"
#include "chips.h"
#include "linux/string.h"
#include "log.h"

/* /#define ENABLE_LOAD_FIRMWARE_FROM_FS */
/*
android firmware path:
/etc/firamre/
path value:
path=video/{cpu_type name}/{format name}/{code name}
example:
path=video/m8/h264/{name}
*/
int request_decoder_firmware_on_sys(enum vformat_e type,
		const char *file_name,
		char *buf, int size)
{

	char *dir_path;

	int len = strlen(file_name) + 128;

	int ret;

	dir_path = kmalloc(len, GFP_KERNEL);

	if (!dir_path)

		return -ENOMEM;

	memset(dir_path, 0, len);

	strcat(dir_path, "video/");

	strcat(dir_path, get_cpu_type_name());

	strcat(dir_path, "/decoder/");

	strcat(dir_path, get_video_format_name(type));

	strcat(dir_path, "/");

	strcat(dir_path, file_name);

	pr_info("try get firmware: %s\n", dir_path);

	ret = amstream_request_firmware_from_sys(dir_path, buf, size);

	kfree(dir_path);

	return ret;
}

static int register_video_firamre_in(struct video_firmware_s *firmware)
{

	struct chip_vdec_info_s *chip_info = get_current_vdec_chip();

	struct video_firmware_s *f_header;

	f_header = chip_info->firmware;

	while (f_header && f_header->next)
		f_header = f_header->next;

	if (!f_header)

		chip_info->firmware = firmware;

	else

		f_header->next = firmware;

	return 0;
}

int register_video_firamre_per_cpu(int cputype, enum vformat_e type,
	const char *name, const char *code,
	int size, const char *version)
{
	struct video_firmware_s *firmware;
	if (cputype != get_cpu_type()) {
		/*pr_info("ignored firmware %s for cpu:%x\n", name, cputype);*/
		return 0;	/* ignore don't needed firmare. */
	}
	firmware = kmalloc(sizeof(struct video_firmware_s) + size, GFP_KERNEL);
	if (!firmware)
		return -ENOMEM;
	firmware->cpu_type = get_cpu_type();
	firmware->type = type;
	firmware->name = name;
	firmware->version = version;
	firmware->size = size;
	firmware->next = NULL;
	firmware->ref_cnt = 0;
	memcpy((void *)firmware->ucode, (void *)code, size);
	/* pr_info("register firmware %s for cpu:%x\n", name, cputype);*/
	return register_video_firamre_in(firmware);
}

int register_video_firamre(int cpus[],
		enum vformat_e type, const char *name,
		const char *code, int size, const char *ver)
{
	int i = 0;
	while (cpus[i] > 0) {
		register_video_firamre_per_cpu(cpus[i],
					type, name, code, size, ver);
		i++;
	}
	return 0;
}

int get_decoder_firmware_from_buildin(enum vformat_e type,
		const char *file_name, struct video_firmware_s **firmware)
{
	struct video_firmware_s *f_header;
	f_header = get_current_vdec_chip()->firmware;
	while (f_header) {
		if (f_header->type == type &&
			!strcmp(file_name, f_header->name)) {
			*firmware = f_header;
			return 0;
		}
		f_header = f_header->next;
	}
	return -1;
}

int get_decoder_firmware_data_from_buildin(enum vformat_e type,
		const char *file_name, char *buf, int size)
{
	struct video_firmware_s *firmware = NULL;
	int ret;
	ret = get_decoder_firmware_from_buildin(type, file_name, &firmware);
	if (!ret && firmware) {
		int l = min(size, firmware->size);
		memcpy(buf, firmware->ucode, l);
		if (l < size) {
			/*set the ext buf to 0 */
			char *p = buf + l;
			p[0] = p[1] = p[2] = p[3] = 0;
			p[4] = p[5] = p[6] = p[7] = 0;
			p = (char *)(((ulong)p + 7) & (~0x7));
			/*for buf aligned.*/
			memset(p, 0, (size - l) & (~7));
		}
		return l;
	}
	return -1;
}

int get_decoder_firmware_data(enum vformat_e type,
		const char *file_name,
		char *buf, int size)
{
	int ret;
#ifdef ENABLE_LOAD_FIRMWARE_FROM_FS
	ret = request_decoder_firmware_on_sys(type,
		file_name, buf, size);
	if (ret > 0)
		return ret;

#endif
	ret = get_decoder_firmware_data_from_buildin(type,
		file_name, buf, size);
	if (ret > 0)
		return ret;
	pr_info("get_decoder_firmware_data %s for format %d failed!\n",
		file_name,
		type);
	return -1;
}

int show_all_buildin_firmwares(void)
{
	struct video_firmware_s *f_header;
	int index = 0;
	f_header = get_current_vdec_chip()->firmware;
	while (f_header) {
		pr_info("firmware[%d]:\n", index++);
		pr_info("\tformart:%s(%d)\n",
			get_video_format_name(f_header->type),
			f_header->type);
		pr_info("\tname:%s\n", f_header->name);
		pr_info("\tversion:%s\n", f_header->version);
		pr_info("\tfirmware size:%d\n", f_header->size);
		pr_info("\tref_cnt:%d\n", f_header->ref_cnt);
		f_header = f_header->next;
	}
	return 0;
}
