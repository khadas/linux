// SPDX-License-Identifier: GPL-2.0-only
/*
 * Goodix Firmware Update Driver.
 *
 * Copyright (C) 2019 - 2020 Goodix, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#include "goodix_ts_core.h"
#include "goodix_cfg_bin.h"
#include "goodix_default_fw.h"
/* COMMON PART - START */
#define TS_DEFAULT_FIRMWARE			"goodix_firmware.bin"

#define FW_HEADER_SIZE				256
#define FW_SUBSYS_INFO_SIZE			8
#define FW_SUBSYS_INFO_OFFSET			32
#define FW_SUBSYS_MAX_NUM			28

#define ISP_MAX_BUFFERSIZE			(1024 * 4)

/* gt738x update */
#define SS51_ISP_CODE				0x01
#define FW_BOOT_CODE				0x0e

#define HW_REG_ILM_ACCESS			0x50C0
#define HW_REG_ILM_ACCESS1			0x434C
#define HW_REG_WATCH_DOG			0x437C
#define HW_REG_BANK_SELECT			0x50C4
#define HW_REG_RAM_ADDR				0x8000
#define HW_REG_DOWNL_STATE			0x4195
#define HW_REG_DOWNL_COMMAND			0x4196
#define HW_REG_DOWNL_RESULT			0x4197
#define HW_REG_HOLD_CPU				0x4180
#define HW_REG_PACKET_SIZE			0xFFF0
#define HW_REG_PACKET_ADDR			0xFFF4
#define HW_REG_PACKET_CHECKSUM			0xFFF8
#define HW_REG_GREEN_CHN1			0xF7CC
#define HW_REG_GREEN_CHN2			0xF7EC
#define HW_REG_SPI_ACCESS			0x4319
#define HW_REG_SPI_STATE			0x431C
#define HW_REG_G1_ACCESS1			0x4255
#define HW_REG_G1_ACCESS2			0x4299

#define ISP_READY_FLAG				0x55
#define START_WRITE_FLASH			0xaa
#define WRITE_OVER2_FLAG			0xff
#define WRITE_TO_FLASH_FLAG			0xdd
#define SPI_STATUS_FLAG				0x02

/**
 * fw_subsys_info - subsystem firmware information
 * @type: subsystem type
 * @size: firmware size
 * @flash_addr: flash address
 * @data: firmware data
 */
struct fw_subsys_info {
	u8 type;
	u32 size;
	u16 flash_addr;
	const u8 *data;
};

#pragma pack(1)
/**
 * firmware_info
 * @size: fw total length
 * @checksum: checksum of fw
 * @hw_pid: mask pid string
 * @hw_pid: mask vid code
 * @fw_pid: fw pid string
 * @fw_vid: fw vid code
 * @subsys_num: number of fw subsystem
 * @chip_type: chip type
 * @protocol_ver: firmware packing
 *   protocol version
 * @subsys: subsystem info
 */
struct firmware_info {
	u32 size;
	u16 checksum;
	u8 hw_pid[6];
	u8 hw_vid[3];
	u8 fw_pid[8];
	u8 fw_vid[4];
	u8 subsys_num;
	u8 chip_type;
	u8 protocol_ver;
	u8 reserved[2];
	struct fw_subsys_info subsys[FW_SUBSYS_MAX_NUM];
};

#pragma pack()

/**
 * firmware_data - firmware data structure
 * @fw_info: firmware information
 * @firmware: firmware data structure
 */
struct firmware_data {
	struct firmware_info fw_info;
	const struct firmware *firmware;
};

enum update_status {
	UPSTA_NOTWORK = 0,
	UPSTA_PREPARING,
	UPSTA_UPDATING,
	UPSTA_ABORT,
	UPSTA_SUCCESS,
	UPSTA_FAILED
};

/**
 * fw_update_ctrl - structure used to control the
 *  firmware update process
 * @initialized: struct init state
 * @mode: indicate weather reflash config or not, fw data source,
 *        and run on block mode or not.
 * @status: update status
 * @progress: indicate the progress of update
 * @allow_reset: control the reset callback
 * @allow_irq: control the irq callback
 * @allow_suspend: control the suspend callback
 * @allow_resume: allow resume callback
 * @fw_data: firmware data
 * @ts_dev: touch device
 * @fw_name: firmware name
 * @attr_fwimage: sysfs bin attrs, for storing fw image
 * @fw_data_src: firmware data source form sysfs, request or head file
 */
struct fw_update_ctrl {
	/* Skip CHECK: definition without comment */
	struct mutex mutex;
	int initialized;
	int mode;
	enum update_status  status;
	unsigned int progress;

	bool allow_reset;
	bool allow_irq;
	bool allow_suspend;
	bool allow_resume;

	struct firmware_data fw_data;
	struct goodix_ts_device *ts_dev;
	struct goodix_ts_core *core_data;

	char fw_name[32];
	struct bin_attribute attr_fwimage;
};

static struct fw_update_ctrl goodix_fw_update_ctrl;
/**
 * goodix_parse_firmware - parse firmware header information
 *	and subsystem information from firmware data buffer
 *
 * @fw_data: firmware struct, contains firmware header info
 *	and firmware data.
 * return: 0 - OK, < 0 - error
 */
static int goodix_parse_firmware(struct firmware_data *fw_data)
{
	const struct firmware *firmware;
	struct firmware_info *fw_info;
	unsigned int i, fw_offset, info_offset;
	u16 checksum;
	int r = 0;

	if (!fw_data || !fw_data->firmware) {
		ts_err("Invalid firmware data");
		return -EINVAL;
	}
	fw_info = &fw_data->fw_info;

	/* copy firmware head info */
	firmware = fw_data->firmware;
	if (firmware->size < FW_SUBSYS_INFO_OFFSET) {
		ts_err("Invalid firmware size:%zu", firmware->size);
		r = -EINVAL;
		goto err_size;
	}
	memcpy(fw_info, firmware->data, FW_SUBSYS_INFO_OFFSET);

	/* check firmware size */
	fw_info->size = be32_to_cpu(fw_info->size);
	if (firmware->size != fw_info->size + 6) {
		ts_err("Bad firmware, size not match");
		r = -EINVAL;
		goto err_size;
	}

	/* calculate checksum, note: sum of bytes, but check
	 * by u16 checksum
	 */
	for (i = 6, checksum = 0; i < firmware->size; i++)
		checksum += firmware->data[i];

	/* byte order change, and check */
	fw_info->checksum = be16_to_cpu(fw_info->checksum);
	if (checksum != fw_info->checksum) {
		ts_err("Bad firmware, checksum error %x(file) != %x(cal)",
			fw_info->checksum, checksum);
		r = -EINVAL;
		goto err_size;
	}

	if (fw_info->subsys_num > FW_SUBSYS_MAX_NUM) {
		ts_err("Bad firmware, invalid subsys num: %d",
		       fw_info->subsys_num);
		r = -EINVAL;
		goto err_size;
	}

	/* parse subsystem info */
	fw_offset = FW_HEADER_SIZE;
	for (i = 0; i < fw_info->subsys_num; i++) {
		info_offset = FW_SUBSYS_INFO_OFFSET +
					i * FW_SUBSYS_INFO_SIZE;

		fw_info->subsys[i].type = firmware->data[info_offset];
		fw_info->subsys[i].size =
				be32_to_cpup((__be32 *)&firmware->data[info_offset + 1]);
		fw_info->subsys[i].flash_addr =
				be16_to_cpup((__be16 *)&firmware->data[info_offset + 5]);

		if (fw_offset > firmware->size) {
			ts_err("Subsys offset exceed Firmware size");
			goto err_size;
		}

		fw_info->subsys[i].data = firmware->data + fw_offset;
		fw_offset += fw_info->subsys[i].size;
	}

	ts_info("Firmware package protocol: V%u", fw_info->protocol_ver);
	ts_info("Firmware PID:GT%s", fw_info->fw_pid);
	ts_info("Firmware VID:%02X%02X%02X%02x", fw_info->fw_vid[0],
				fw_info->fw_vid[1], fw_info->fw_vid[2], fw_info->fw_vid[3]);
	ts_info("Firmware chip type:%02X", fw_info->chip_type);
	ts_info("Firmware size:%u", fw_info->size);
	ts_info("Firmware subsystem num:%u", fw_info->subsys_num);
#ifdef CONFIG_GOODIX_DEBUG
	for (i = 0; i < fw_info->subsys_num; i++) {
		ts_debug("------------------------------------------");
		ts_debug("Index:%d", i);
		ts_debug("Subsystem type:%02X", fw_info->subsys[i].type);
		ts_debug("Subsystem size:%u", fw_info->subsys[i].size);
		ts_debug("Subsystem flash_addr:%08X", fw_info->subsys[i].flash_addr);
		ts_debug("Subsystem Ptr:%p", fw_info->subsys[i].data);
	}
	ts_debug("------------------------------------------");
#endif

err_size:
	return r;
}

/**
 * goodix_check_update - compare the version of firmware running in
 *  touch device with the version getting from the firmware file.
 * @fw_info: firmware information to be compared
 * return: 0 no need do update,
 *         otherwise need do update
 */
static int goodix_check_update(struct goodix_ts_device *dev,
		const struct firmware_info *fw_info)
{
	struct goodix_ts_version fw_ver;
	int ret = -EINVAL;

	/* read version from chip, if we got invalid
	 * firmware version, maybe firmware in flash is
	 * incorrect, so we need to update firmware
	 */
	ret = dev->hw_ops->read_version(dev, &fw_ver);
	if (ret) {
		ts_info("failed get active pid");
		return -EINVAL;
	}

	if (fw_ver.valid) {
		// should we compare PID before fw update?
		// if fw patch damage the PID may unmatch but
		// we should de update to recover it.
		// TODO skip PID check
		//if (memcmp(fw_ver.pid, fw_info->fw_pid, dev->reg.pid_len)) {
		//	ts_err("Product ID is not match %s != %s",
		//			fw_ver.pid, fw_info->fw_pid);
		//	return -EPERM;
		//}
		ts_info("ic pid:%*ph\n", 4, fw_ver.pid);
		ts_info("fw pid:%*ph\n", 8, fw_info->fw_pid);
		ts_info("pid len:%d\n", dev->reg.pid_len);
		if (memcmp(fw_ver.pid, &fw_info->fw_pid[4], dev->reg.pid_len)) {
			ts_err("Product ID is not match\n");
			return -EPERM;
		}

		ts_info("ic vid:%*ph\n", 4, fw_ver.vid);
		ts_info("fw vid:%*ph\n", 4, fw_info->fw_vid);

		ret = memcmp(fw_ver.vid, fw_info->fw_vid, dev->reg.vid_len);
		if (ret == 0) {
			ts_info("FW version is equal to the IC's");
			return 0;
		} else if (ret > 0) {
			ts_info("Warning: fw version is lower the IC's");
		}
	} /* else invalid firmware, update firmware */

	ts_info("Firmware needs to be updated");
	return ret;
}

/**
 * goodix_reg_write_confirm - write register and confirm the value
 *  in the register.
 * @dev: pointer to touch device
 * @addr: register address
 * @data: pointer to data buffer
 * @len: data length
 * return: 0 write success and confirm ok
 *		   < 0 failed
 */
static int goodix_reg_write_confirm(struct goodix_ts_device *dev,
		unsigned int addr, unsigned char *data, unsigned int len)
{
	u8 *cfm, cfm_buf[32];
	int r, i;

	if (len > sizeof(cfm_buf)) {
		cfm = kzalloc(len, GFP_KERNEL);
		if (!cfm)
			return -ENOMEM;
	} else {
		cfm = &cfm_buf[0];
	}

	for (i = 0; i < GOODIX_BUS_RETRY_TIMES; i++) {
		r = dev->hw_ops->write(dev, addr, data, len);
		if (r < 0)
			goto exit;

		r = dev->hw_ops->read(dev, addr, cfm, len);
		if (r < 0)
			goto exit;

		if (memcmp(data, cfm, len)) {
			r = -EMEMCMP;
			continue;
		} else {
			r = 0;
			break;
		}
	}

exit:
	if (cfm != &cfm_buf[0])
		kfree(cfm);
	return r;
}

static inline int goodix_reg_write(struct goodix_ts_device *dev,
		unsigned int addr, unsigned char *data, unsigned int len)
{
	return dev->hw_ops->write(dev, addr, data, len);
}

static inline int goodix_reg_read(struct goodix_ts_device *dev,
		unsigned int addr, unsigned char *data, unsigned int len)
{
	return dev->hw_ops->read(dev, addr, data, len);
}

static int goodix_hold_dsp(struct goodix_ts_device *ts_dev)
{
	int retry_times = 50; /* if failed, retry, for max times 50 */
	int ret = 0;
	u8 index = 0;
	u8 write_value = 0x06; /* ILM access enable */
	u8 buf_tmp[2];

	ts_info("%s run\n", __func__);

	/* the same effect between write 0x04 to 0x8040 and write 0x06 to 0x50c0,
	 * if use  write 0x04 to 0x8040
	 */
	for (index = 0; index < retry_times; index++) {
		buf_tmp[0] = write_value;
		ret = goodix_reg_write(ts_dev, HW_REG_ILM_ACCESS, buf_tmp, 1);
		if (ret < 0) {
			ts_err("write 0x50c0 failed\n");
			continue;
		}

		usleep_range(10000, 11000);
		write_value = 0;
		ret = goodix_reg_read(ts_dev, HW_REG_ILM_ACCESS, &(write_value), 1);
		if (write_value == buf_tmp[0]) {
			ret = 0;
			break;
		}
	}
	if (index >= retry_times) {
		ts_err("%s:write 0x06 to 0x50c0 and check fail!", __func__);
		return -EINVAL;
	}

	/* clear watchdog */
	write_value = 0x00;
	/* 0x40b0 is the timer of watchdog, 0x437c is the enable of watchdog */
	ret = goodix_reg_write(ts_dev, HW_REG_WATCH_DOG, &write_value, 1);
	if (ret < 0)
		ts_info("%s: clear watch dog, write 0x00 to 0x437c fail!", __func__);
	ts_info("%s eixt, ret:%d\n", __func__, ret);
	return ret;
}

static int goodix_ilm_access(struct goodix_ts_device *ts_dev, u8 enable)
{
	u8 data;

	if (enable) {
		data = 6;
		return goodix_reg_write(ts_dev, HW_REG_ILM_ACCESS, &data, 1);
	}
	data = 0;
	return goodix_reg_write(ts_dev, HW_REG_ILM_ACCESS, &data, 1);
}

static int goodix_bank_select(struct goodix_ts_device *ts_dev, u8 bank)
{
	return goodix_reg_write(ts_dev, HW_REG_BANK_SELECT, &bank, 1);
}

static int goodix_set_boot_from_sram(struct goodix_ts_device *ts_dev)
{
	int ret = 0;
	u8 reg0[4] = { 0x00, 0x00, 0x00, 0x00 };
	u8 reg1[4] = { 0xb8, 0x3f, 0x35, 0x56 };
	u8 reg2[4] = { 0xb9, 0x3e, 0xb5, 0x54 };

	/* enable ILM*/
	ret = goodix_reg_write(ts_dev, HW_REG_ILM_ACCESS1, reg0, 1);
	if (ret < 0) {
		ts_err("%s:[set_boot_from_sram] write 0x434c fail\n", __func__);
		return ret;
	}
	/* set green channel */
	ret = goodix_reg_write(ts_dev, HW_REG_GREEN_CHN1, reg1, 4);
	if (ret < 0) {
		ts_err("%s:[set_boot_from_sram] write 0xf7cc fail\n", __func__);
		return ret;
	}
	ret = goodix_reg_write(ts_dev, HW_REG_GREEN_CHN2, reg2, 4);
	if (ret < 0)
		ts_err("%s:[set_boot_from_sram] write 0xf7ec fail\n", __func__);

	return ret;
}

static int goodix_wait_sta(struct goodix_ts_device *ts_dev, u16 sta_addr,
			   u8 sta, u16 timeout)
{
	u16 i, cnt;
	u8 data;
	int len = 1; /* read/write data len */

	data = sta - 1;
	cnt = timeout / 10; /* delay step 10ms */

	for (i = 0; i < cnt; i++) {
		goodix_reg_read(ts_dev,  sta_addr, &data, len);
		if (data == sta) {
			if (sta == 0xff && timeout == 3000)
				ts_info("retry times:%d\n", i);
			return 0;
		}
		usleep_range(10000, 11000);
	}
	ts_err("wait 0x%04x eq 0x%02x failed, real status:0x%02x\n", sta_addr,
				sta, data);
	return -EINVAL;
}

/**
 * goodix_load_isp - load ISP program to device ram
 * @dev: pointer to touch device
 * @fw_data: firmware data
 * return 0 ok, <0 error
 */
static int goodix_load_isp(struct goodix_ts_device *ts_dev,
			   struct firmware_data *fw_data)
{
	int ret = 0;
	int i = 0;
	struct fw_subsys_info *fw_isp = NULL;
	u8 retry_cnt = GOODIX_RETRY_NUM_3;
	u8 data = 0;

	ts_info("%s run\n", __func__);

	ret = goodix_ilm_access(ts_dev, 1); /* 0: disable, else: enable */
	if (ret < 0) {
		ts_err("%s:ilm_access enable fail\n", __func__);
		goto exit;
	}

	/* select bank 2; */
	ret = goodix_bank_select(ts_dev, 2);
	if (ret < 0) {
		ts_err("%s:select bank 2 failed\n", __func__);
		goto exit;
	}

	//download isp
	for (i = 0; i < fw_data->fw_info.subsys_num; i++) {
		if (fw_data->fw_info.subsys[i].type == SS51_ISP_CODE) {
			fw_isp = &fw_data->fw_info.subsys[i];
			break;
		}
	}

	if (!fw_isp) {
		ret = -EINVAL;
		ts_err("%s no isp in bin file\n", __func__);
		goto exit;
	}

	for (i = 0; i < retry_cnt; i++) {
		ret = goodix_reg_write_confirm(ts_dev, HW_REG_RAM_ADDR,
				     (u8 *)fw_isp->data, fw_isp->size);
		if (ret == 0)
			break;
	}

	if (i >= retry_cnt) {
		ts_info("%s:down load isp code to ram fail\n", __func__);
		goto exit;
	}

	ts_info("Success send ISP data to IC\n");

	data = 0;
	ret = goodix_reg_write(ts_dev, HW_REG_DOWNL_STATE, &data, 1);
	if (ret < 0) {
		ts_err("%s:download sta addr[0x4195] write 0 fail\n", __func__);
		goto exit;
	}

	ret = goodix_reg_write(ts_dev, HW_REG_DOWNL_COMMAND, &data, 1);
	if (ret < 0) {
		ts_err("%s:download cmd addr[0x4196] write 0 fail\n", __func__);
		goto exit;
	}

	ret = goodix_set_boot_from_sram(ts_dev);
	if (ret < 0) {
		ts_err("%s:set_boot_from_sram fail\n", __func__);
		goto exit;
	}

	ret = goodix_ilm_access(ts_dev, 0);
	if (ret < 0) {
		ts_err("%s:ILM access to 0 fail\n", __func__);
		goto exit;
	}

	data = 0;
	ret = goodix_reg_write(ts_dev, HW_REG_HOLD_CPU, &data, 1);
	if (ret < 0) {
		ts_err("%s:hold cpu fail\n", __func__);
		goto exit;
	}

	/* Check ISP Run */
	ret = goodix_wait_sta(ts_dev, HW_REG_DOWNL_STATE, 0xff, 3000);
	if (ret < 0) {
		ts_err("%s: phoenix fw update isp don't run\n", __func__);
		goto exit;
	}
	ret = 0;
	ts_info("%s, isp running\n", __func__);
exit:

	ts_info("%s exit:%d\n", __func__, ret);
	return ret;
}

/**
 * goodix_update_prepare - update prepare, loading ISP program
 *  and make sure the ISP is running.
 * @fwu_ctrl: pointer to firmware control structure
 * return: 0 ok, <0 error
 */
static int goodix_update_prepare(struct fw_update_ctrl *fwu_ctrl)
{
	struct goodix_ts_device *ts_dev = fwu_ctrl->ts_dev;
	int reset_gpio = ts_dev->board_data.reset_gpio;
	int retry_cnt = GOODIX_RETRY_NUM_20;
	int ret = 0;
	int i = 0;
	u8 spi_dis = 0x00;
	int len;

	ts_info("%s run\n", __func__);
	/* rest ic */
	fwu_ctrl->allow_reset = true;
	ts_info("fwupdate chip reset\n");
	gpio_direction_output(reset_gpio, 0);
	/* WARNING: long udelay - prefer mdelay; see arch/arm/include/asm/delay.h
	 * udelay(10000);
	 */
	mdelay(10);
	gpio_direction_output(reset_gpio, 1);

	/* WARNING: long udelay - prefer mdelay; see arch/arm/include/asm/delay.h
	 * udelay(10000);
	 */
	mdelay(10);
	fwu_ctrl->allow_reset = false;

	/* hold dsp */
	ret = goodix_hold_dsp(ts_dev);
	if (ret < 0) {
		ts_info("%s: hold dsp failed\n", __func__);
		goto exit;
	}
	ts_info("hold dsp sucessful\n");

	len = 1; /* read/write data len */
	for (i = 0; i < retry_cnt; i++) {
		/* disable spi between GM11 and G1 */
		spi_dis = 0; /* disable spi */
		ret = goodix_reg_write(ts_dev, HW_REG_SPI_ACCESS, &spi_dis,
				       len);
		if (ret >= 0) {
			ret = goodix_reg_read(ts_dev, HW_REG_SPI_STATE,
					      &spi_dis, len);
			if (ret >= 0 && (spi_dis & SPI_STATUS_FLAG) == 0)
				break;
		}

		spi_dis = 2; /* disable G1 */
		ret = goodix_reg_write(ts_dev, HW_REG_G1_ACCESS1, &spi_dis,
				       len);
		if (ret < 0 && i == retry_cnt - 1)
			ts_err("write 0x4255 fail\n");
		ret = goodix_reg_write(ts_dev, HW_REG_G1_ACCESS2, &spi_dis,
				       len);
		if (ret < 0 && i == retry_cnt - 1)
			ts_err("write 0x4299 fail\n");
	}
	if (i >= retry_cnt) {
		ret = -EINVAL;
		ts_err("disable G1 failed\n");
		goto exit;
	}

	ret = goodix_load_isp(ts_dev, &fwu_ctrl->fw_data);
	if (ret < 0) {
		ts_info("%s: load isp failed\n", __func__);
		goto exit;
	}
	ret = 0;

exit:
	ts_info("%s exit\n", __func__);
	return ret;
}

static s32 goodix_write_u32(struct goodix_ts_device *ts_dev, u16 addr, u32 data)
{
	u8 buf[4];
	u16 len = 4; /* write data len */

	buf[0] = data & 0xff;
	buf[1] = (data >> 8) & 0xff;
	buf[2] = (data >> 16) & 0xff;
	buf[3] = (data >> 24) & 0xff;

	return goodix_reg_write(ts_dev, addr, buf, len);
}

static int goodix_set_checksum(struct goodix_ts_device *ts_dev, u8 *code,
			       u32 len)
{
	u32 chksum = 0;
	u32 tmp;
	u32 i;

	for (i = 0; i < len; i += 4) {
		tmp = le32_to_cpup((__le32 *)(code + i));
		chksum += tmp;
	}
	return goodix_write_u32(ts_dev, HW_REG_PACKET_CHECKSUM, chksum);
}

static int goodix_ph_write_cmd(struct goodix_ts_device *ts_dev, u8 cmd)
{
	int len = 1;

	return goodix_reg_write(ts_dev, HW_REG_DOWNL_COMMAND, &cmd, len);
}

static int goodix_send_fw_packet(struct goodix_ts_device *ts_dev,
				 struct fw_subsys_info *fw_x)
{
	u32 remain_len;
	u32 current_len;
	u32 write_addr;
	s32 retry_cnt = GOODIX_RETRY_NUM_3;
	int ret = 0;
	int j = 0;
	u8 *fw_addr = 0;
	int time_delay;

	ts_info("%s run\n", __func__);
	write_addr = fw_x->flash_addr * 256; /* get real flash addr */
	remain_len = fw_x->size;
	fw_addr = (u8 *)(fw_x->data);
	ts_info("subsystem type:0x%x, addr:0x%x, len:%d\n",
		fw_x->type, write_addr, remain_len);

	while (remain_len > 0) {
		current_len = remain_len;
		if (current_len > ISP_MAX_BUFFERSIZE)
			current_len = ISP_MAX_BUFFERSIZE;

		for (j = 0; j < retry_cnt; j++) {
			/* set packet size */
			ret = goodix_write_u32(ts_dev, HW_REG_PACKET_SIZE,
					       current_len);
			if (ret < 0) {
				ts_err("%s:set packet size failed\n", __func__);
				continue;
			}
			/* set flash addr */
			ret = goodix_write_u32(ts_dev, HW_REG_PACKET_ADDR,
					       write_addr);
			if (ret < 0) {
				ts_err("%s:set flash addr failed\n", __func__);
				continue;
			}
			/* set checksum */
			ret = goodix_set_checksum(ts_dev, fw_addr, current_len);
			if (ret < 0) {
				ts_err("%s:set check sum failed\n", __func__);
				continue;
			}

			/* Inform ISP to be ready for writing to flash */
			ret = goodix_ph_write_cmd(ts_dev, ISP_READY_FLAG);
			if (ret < 0) {
				ts_err("%s, send cmd 0x55 failed\n", __func__);
				continue;
			}
			ret = goodix_reg_write_confirm(ts_dev, HW_REG_RAM_ADDR,
						       fw_addr, current_len);
			if (ret < 0) {
				ts_err("%s: write fw failed\n", __func__);
				continue;
			}

			time_delay = 1000; /* delay 1000ms to wait start to  write */
			ret = goodix_wait_sta(ts_dev, HW_REG_DOWNL_STATE,
					      START_WRITE_FLASH, time_delay);
			if (ret < 0) {
				ts_err("%s: write over1\n", __func__);
				continue;
			}
			ret = goodix_ph_write_cmd(ts_dev, START_WRITE_FLASH);
			if (ret < 0) {
				ts_err("%s, Inform ISP start to write flash fail\n", __func__);
				continue;
			}

			time_delay = 600; /* delay 600ms to wait download state */
			ret = goodix_wait_sta(ts_dev, HW_REG_DOWNL_STATE,
					      WRITE_OVER2_FLAG, time_delay);
			if (ret < 0) {
				ts_err("%s:write over2 failed\n", __func__);
				continue;
			}
			time_delay = 600; /* delay 600ms to write flash */
			ret = goodix_wait_sta(ts_dev, HW_REG_DOWNL_RESULT,
					      WRITE_TO_FLASH_FLAG, time_delay);
			if (ret < 0) {
				ts_info("%s:write flash failed\n", __func__);
				continue;
			}

			break;
		}
		if (j >= retry_cnt)
			goto FW_UPDATE_END;

		remain_len -= current_len;
		fw_addr += current_len;
		write_addr += current_len;
	}

FW_UPDATE_END:
	ts_info("%s exit, ret:%d\n", __func__, ret);
	return ret;
}

/**
 * goodix_flash_firmware - flash firmware
 * @dev: pointer to touch device
 * @fw_data: firmware data
 * return: 0 ok, < 0 error
 */
static int goodix_flash_firmware(struct goodix_ts_device *ts_dev,
				struct firmware_data *fw_data)
{
	struct fw_update_ctrl *fw_ctrl = NULL;
	struct firmware_info  *fw_info = NULL;
	struct fw_subsys_info *fw_x = NULL;
	int retry = GOODIX_RETRY_NUM_3;
	int i = 0;
	int fw_num = 0;
	int prog_step = 0;
	int ret = 0;

	/* start from subsystem 1,
	 * subsystem 0 is the ISP program
	 */
	fw_ctrl = container_of(fw_data, struct fw_update_ctrl, fw_data);
	fw_info = &fw_data->fw_info;
	fw_num = fw_info->subsys_num;

	/* we have 80% work here */
	prog_step = 80 / (fw_num - 1);

	for (i = 0; i < fw_num && retry; ) {
		ts_info("---Start to flash subsystem[%d] ---\n", i);
		fw_x = &fw_info->subsys[i];
		if (fw_x->type == SS51_ISP_CODE) {
			ts_info("current subsystem is isp, no need to upgrade\n");
			i++;
			continue;
		}
		/* no need update bootcode */
		if (fw_x->type == FW_BOOT_CODE && fw_info->reserved[0] == 1) {
			ts_info("current subsystem is bootcode, no need to upgrade\n");
			i++;
			continue;
		}

		ret = goodix_send_fw_packet(ts_dev, fw_x);

		if (ret == 0) {
			ts_info("--- End flash subsystem[%d]: OK ---\n", i);
			fw_ctrl->progress += prog_step;
			i++;
		} else if (ret == -EAGAIN) {
			retry--;
			ts_err("--- End flash subsystem%d: Fail, errno:%d, retry:%d ---\n",
				i, ret, GOODIX_RETRY_NUM_3 - retry);
		} else if (ret < 0) { /* bus error */
			ts_err("--- End flash subsystem%d: Fatal error:%d exit ---\n",
				i, ret);
			ret = -ETIMEOUT;
			goto exit_flash;
		}
	}
	ret = 0;

exit_flash:

	return ret;
}

/**
 * goodix_update_finish - update finished, free resource
 *  and reset flags---
 * @fwu_ctrl: pointer to fw_update_ctrl structrue
 * return: 0 ok, < 0 error
 */
static int goodix_update_finish(struct goodix_ts_device *ts_dev,
				struct fw_update_ctrl *fwu_ctrl)
{
	int r = 0;
	int reset_gpio = ts_dev->board_data.reset_gpio;
	/*reset*/
	gpio_direction_output(reset_gpio, 0);
	/* WARNING: long udelay - prefer mdelay; see arch/arm/include/asm/delay.h
	 * udelay(10000);
	 */
	mdelay(10);
	gpio_direction_output(reset_gpio, 1);

	msleep(500);

	if (ts_dev->hw_ops->init_esd)
		r = ts_dev->hw_ops->init_esd(ts_dev);

	return r;
}

/**
 * goodix_fw_update_proc - firmware update process, the entry of
 *  firmware update flow
 * @fwu_ctrl: firmware control
 * return: 0 ok, < 0 error
 */
static int goodix_fw_update_proc(struct fw_update_ctrl *fwu_ctrl)
{
#define FW_UPDATE_RETRY		2
	int retry0 = FW_UPDATE_RETRY;
	int retry1 = FW_UPDATE_RETRY;
	int r = 0;

	if (fwu_ctrl->status == UPSTA_PREPARING ||
			fwu_ctrl->status == UPSTA_UPDATING) {
		ts_err("Firmware update already in progress");
		return -EINVAL;
	}

	fwu_ctrl->progress = 0;
	fwu_ctrl->status = UPSTA_PREPARING;

	r = goodix_parse_firmware(&fwu_ctrl->fw_data);
	if (r < 0) {
		fwu_ctrl->status = UPSTA_ABORT;
		goto err_parse_fw;
	}

	fwu_ctrl->progress = 10;
	if (!(fwu_ctrl->mode & UPDATE_MODE_FORCE)) {
		r = goodix_check_update(fwu_ctrl->ts_dev,
					&fwu_ctrl->fw_data.fw_info);
		if (!r) {
			fwu_ctrl->status = UPSTA_ABORT;
			ts_info("fw update skiped");
			goto err_check_update;
		}
	} else {
		ts_info("force update mode");
	}

start_update:
	fwu_ctrl->progress = 20;
	fwu_ctrl->status = UPSTA_UPDATING; /* show upgrading status */
	r = goodix_update_prepare(fwu_ctrl);
	if ((r == -EAGAIN) && --retry0 > 0) {
		ts_err("Bus error, retry prepare ISP:%d",
			FW_UPDATE_RETRY - retry0);
		goto start_update;
	} else if (r < 0) {
		ts_err("Failed to prepare ISP, exit update:%d", r);
		fwu_ctrl->status = UPSTA_FAILED;
		goto err_fw_prepare;
	}

	/* progress: 20%~100% */
	r = goodix_flash_firmware(fwu_ctrl->ts_dev, &fwu_ctrl->fw_data);
	if ((r == -ETIMEOUT) && --retry1 > 0) {
		/* we will retry[twice] if returns bus error[i2c/spi]
		 * we will do hardware reset and re-prepare ISP and then retry
		 * flashing
		 */
		ts_err("Bus error, retry firmware update:%d",
				FW_UPDATE_RETRY - retry1);
		goto start_update;
	} else if (r < 0) {
		ts_err("Fatal error, exit update:%d", r);
		fwu_ctrl->status = UPSTA_FAILED;
		goto err_fw_flash;
	}

	fwu_ctrl->status = UPSTA_SUCCESS;

err_fw_flash:
err_fw_prepare:
	goodix_update_finish(fwu_ctrl->ts_dev, fwu_ctrl);
err_check_update:
err_parse_fw:
	if (fwu_ctrl->status == UPSTA_SUCCESS)
		ts_info("Firmware update successfully");
	else if (fwu_ctrl->status == UPSTA_FAILED)
		ts_err("Firmware update failed");

	fwu_ctrl->progress = 100; /* 100% */
	ts_info("fw update ret %d", r);
	return r;
}

static struct goodix_ext_module goodix_fwu_module;

/**
 * goodix_request_firmware - request firmware data from user space
 *
 * @fw_data: firmware struct, contains firmware header info
 *	and firmware data pointer.
 * return: 0 - OK, < 0 - error
 */
static int goodix_request_firmware(struct firmware_data *fw_data,
				const char *name)
{
	struct fw_update_ctrl *fw_ctrl =
		container_of(fw_data, struct fw_update_ctrl, fw_data);
	struct device *dev = fw_ctrl->ts_dev->dev;
	int r;

	ts_info("Request firmware image [%s]", name);
	r = request_firmware(&fw_data->firmware, name, dev);
	if (r < 0)
		ts_err("Firmware image [%s] not available, errno:%d", name, r);
	else
		ts_info("Firmware image [%s] is ready", name);
	return r;
}

/**
 * relase firmware resources
 *
 */
static inline void goodix_release_firmware(struct firmware_data *fw_data)
{
	if (fw_data->firmware) {
		release_firmware(fw_data->firmware);
		fw_data->firmware = NULL;
	}
}

static int goodix_fw_update_thread(void *data)
{
	struct fw_update_ctrl *fwu_ctrl = data;
	struct firmware *temp_firmware = NULL;
	int r = -EINVAL;

	mutex_lock(&fwu_ctrl->mutex);

	if (fwu_ctrl->mode & UPDATE_MODE_SRC_HEAD) {
		ts_info("Firmware header update starts");
		temp_firmware = kzalloc(sizeof(*temp_firmware), GFP_KERNEL);
		if (!temp_firmware) {
			ts_err("Failed to allocate memory for firmware");
			goto out;
		}
		temp_firmware->size = sizeof(goodix_default_fw);
		temp_firmware->data = goodix_default_fw;
		fwu_ctrl->fw_data.firmware = temp_firmware;
	} else if (fwu_ctrl->mode & UPDATE_MODE_SRC_REQUEST) {
		ts_info("Firmware request update starts");
		r = goodix_request_firmware(&fwu_ctrl->fw_data,
						fwu_ctrl->fw_name);
		if (r < 0) {
			fwu_ctrl->status = UPSTA_ABORT;
			fwu_ctrl->progress = 100;
			goto out;
		}
	} else if (fwu_ctrl->mode & UPDATE_MODE_SRC_SYSFS) {
		if (!fwu_ctrl->fw_data.firmware) {
			ts_err("Invalid firmware from sysfs");
			fwu_ctrl->status = UPSTA_ABORT;
			fwu_ctrl->progress = 100;
			r = -EINVAL;
			goto out;
		}
	} else {
		ts_err("unknown update mode 0x%x", fwu_ctrl->mode);
		r = -EINVAL;
		goto out;
	}

	goodix_register_ext_module(&goodix_fwu_module);
	/* DONT allow reset/irq/suspend/resume during update */
	fwu_ctrl->allow_irq = false;
	fwu_ctrl->allow_suspend = false;
	fwu_ctrl->allow_resume = false;
	fwu_ctrl->allow_reset = false;
	ts_debug("notify update start");
	goodix_ts_blocking_notify(NOTIFY_FWUPDATE_START, NULL);

	/* ready to update */
	ts_debug("start update proc");
	r = goodix_fw_update_proc(fwu_ctrl);

	fwu_ctrl->allow_reset = true;
	fwu_ctrl->allow_irq = true;
	fwu_ctrl->allow_suspend = true;
	fwu_ctrl->allow_resume = true;

	/* clean */
	if (fwu_ctrl->mode & UPDATE_MODE_SRC_HEAD) {
		kfree(fwu_ctrl->fw_data.firmware);
		fwu_ctrl->fw_data.firmware = NULL;
		temp_firmware = NULL;
	} else if (fwu_ctrl->mode & UPDATE_MODE_SRC_REQUEST) {
		goodix_release_firmware(&fwu_ctrl->fw_data);
	} else if (fwu_ctrl->mode & UPDATE_MODE_SRC_SYSFS) {
		vfree(fwu_ctrl->fw_data.firmware);
		fwu_ctrl->fw_data.firmware = NULL;
	}
	goodix_unregister_ext_module(&goodix_fwu_module);
out:
	fwu_ctrl->mode = UPDATE_MODE_DEFAULT;
	mutex_unlock(&fwu_ctrl->mutex);

	if (r) {
		ts_err("fw update failed, %d", r);
		goodix_ts_blocking_notify(NOTIFY_FWUPDATE_FAILED, NULL);
	} else {
		ts_info("fw update success");
		goodix_ts_blocking_notify(NOTIFY_FWUPDATE_SUCCESS, NULL);
	}
	return r;
}

/*
 * goodix_sysfs_update_en_store: start fw update manually
 * @buf: '1'[001] update in blocking mode with fwdata from sysfs
 *       '2'[010] update in blocking mode with fwdata from request
 *       '5'[101] update in unblocking mode with fwdata from sysfs
 *       '6'[110] update in unblocking mode with fwdata from request
 */
static ssize_t goodix_sysfs_update_en_store(struct goodix_ext_module *module,
					    const char *buf, size_t count)
{
	int ret = 0;
	int mode = 0;
	struct fw_update_ctrl *fw_ctrl = module->priv_data;

	if (!buf || count <= 0) {
		ts_err("invalid params");
		return -EINVAL;
	}
	if (!fw_ctrl || !fw_ctrl->initialized) {
		ts_err("fw module uninit");
		return -EINVAL;
	}

	ts_info("set update mode:0x%x", buf[0]);
	if (buf[0] == '1') {
		mode = UPDATE_MODE_FORCE | UPDATE_MODE_BLOCK |
		       UPDATE_MODE_SRC_SYSFS;
	} else if (buf[0] == '2') {
		mode = UPDATE_MODE_FORCE | UPDATE_MODE_BLOCK |
		       UPDATE_MODE_SRC_REQUEST;
	} else if (buf[0] == '5') {
		mode = UPDATE_MODE_FORCE | UPDATE_MODE_SRC_SYSFS;
	} else if (buf[0] == '6') {
		mode = UPDATE_MODE_FORCE | UPDATE_MODE_SRC_REQUEST;
	} else {
		ts_err("invalid update mode:0x%x", buf[0]);
		return -EINVAL;
	}

	ret = goodix_do_fw_update(mode);
	if (!ret) {
		ts_info("success start update work");
		return count;
	}
	ts_err("failed start fw update work");
	return -EINVAL;
}

static ssize_t goodix_sysfs_update_progress_show(struct goodix_ext_module
						 *module, char *buf)
{
	struct fw_update_ctrl *fw_ctrl = module->priv_data;

	return scnprintf(buf, PAGE_SIZE, "%d\n", fw_ctrl->progress);
}

static ssize_t goodix_sysfs_update_result_show(struct goodix_ext_module
					       *module, char *buf)
{
	char *result = NULL;
	struct fw_update_ctrl *fw_ctrl = module->priv_data;

	ts_info("result show");
	switch (fw_ctrl->status) {
	case UPSTA_NOTWORK:
		result = "notwork";
		break;
	case UPSTA_PREPARING:
		result = "preparing";
		break;
	case UPSTA_UPDATING:
		result = "upgrading";
		break;
	case UPSTA_ABORT:
		result = "abort";
		break;
	case UPSTA_SUCCESS:
		result = "success";
		break;
	case UPSTA_FAILED:
		result = "failed";
		break;
	default:
		break;
	}

	return scnprintf(buf, PAGE_SIZE, "%s\n", result);
}

static ssize_t goodix_sysfs_update_fwversion_show(struct goodix_ext_module
						  *module, char *buf)
{
	struct goodix_ts_version fw_ver;
	struct fw_update_ctrl *fw_ctrl = module->priv_data;
	int r = 0;
	char str[5];

	/* read version from chip */
	r = fw_ctrl->ts_dev->hw_ops->read_version(fw_ctrl->ts_dev,
			&fw_ver);
	if (!r) {
		memcpy(str, fw_ver.pid, 4);
		str[4] = '\0';
		return scnprintf(buf, PAGE_SIZE, "PID:%s VID:%02x %02x %02x %02x SENSOR_ID:%d\n",
				str, fw_ver.vid[0], fw_ver.vid[1],
				fw_ver.vid[2], fw_ver.vid[3], fw_ver.sensor_id);
	}
	return 0;
}

static ssize_t goodix_sysfs_fwsize_show(struct goodix_ext_module *module,
		char *buf)
{
	struct fw_update_ctrl *fw_ctrl = module->priv_data;
	int r = -EINVAL;

	if (fw_ctrl && fw_ctrl->fw_data.firmware)
		r = snprintf(buf, PAGE_SIZE, "%zu\n",
				fw_ctrl->fw_data.firmware->size);
	return r;
}

static ssize_t goodix_sysfs_fwsize_store(struct goodix_ext_module *module,
		const char *buf, size_t count)
{
	struct fw_update_ctrl *fw_ctrl = module->priv_data;
	struct firmware *fw;
	u8 **data;
	size_t size = 0;

	if (!fw_ctrl)
		return -EINVAL;

	if (sscanf(buf, "%zu", &size) < 0 || !size) {
		ts_err("Failed to get fwsize");
		return -EFAULT;
	}

	/* use vmalloc to alloc huge memory */
	fw = vmalloc(sizeof(*fw) + size);
	if (!fw)
		return -ENOMEM;

	mutex_lock(&fw_ctrl->mutex);
	memset(fw, 0x00, sizeof(*fw) + size);
	data = (u8 **)&fw->data;
	*data = (u8 *)fw + sizeof(struct firmware);
	fw->size = size;
	fw_ctrl->fw_data.firmware = fw;
	fw_ctrl->mode = UPDATE_MODE_SRC_SYSFS;
	mutex_unlock(&fw_ctrl->mutex);
	return count;
}

static ssize_t goodix_sysfs_fwimage_store(struct file *file,
		struct kobject *kobj, struct bin_attribute *attr,
		char *buf, loff_t pos, size_t count)
{
	struct fw_update_ctrl *fw_ctrl;
	struct firmware_data *fw_data;

	fw_ctrl = container_of(attr, struct fw_update_ctrl,
			attr_fwimage);
	fw_data = &fw_ctrl->fw_data;

	if (!fw_data->firmware) {
		ts_err("Need set fw image size first");
		return -ENOMEM;
	}

	if (fw_data->firmware->size == 0) {
		ts_err("Invalid firmware size");
		return -EINVAL;
	}

	if (pos + count > fw_data->firmware->size)
		return -EFAULT;
	mutex_lock(&fw_ctrl->mutex);
	memcpy((u8 *)&fw_data->firmware->data[pos], buf, count);
	mutex_unlock(&fw_ctrl->mutex);
	return count;
}

/* this interface has ben deprecated */
static ssize_t goodix_sysfs_force_update_store(struct goodix_ext_module *module,
					       const char *buf, size_t count)
{
	return count;
}

static struct goodix_ext_attribute goodix_fwu_attrs[] = {
	__EXTMOD_ATTR(update_en, 0222, NULL, goodix_sysfs_update_en_store),
	__EXTMOD_ATTR(progress, 0444, goodix_sysfs_update_progress_show, NULL),
	__EXTMOD_ATTR(result, 0444, goodix_sysfs_update_result_show, NULL),
	__EXTMOD_ATTR(fwversion, 0444,
			goodix_sysfs_update_fwversion_show, NULL),
	__EXTMOD_ATTR(fwsize, 0666, goodix_sysfs_fwsize_show,
			goodix_sysfs_fwsize_store),
	__EXTMOD_ATTR(force_update, 0222, NULL,
			goodix_sysfs_force_update_store),
};

static int goodix_fw_sysfs_init(struct goodix_ts_core *core_data,
		struct goodix_ext_module *module)
{
	struct fw_update_ctrl *fw_ctrl = module->priv_data;
	struct kobj_type *ktype;
	int ret = 0, i;

	ktype = goodix_get_default_ktype();
	ret = kobject_init_and_add(&module->kobj,
			ktype,
			&core_data->pdev->dev.kobj,
			"fwupdate");
	if (ret) {
		ts_err("Create fwupdate sysfs node error!");
		goto exit_sysfs_init;
	}

	ret = 0;
	for (i = 0; i < ARRAY_SIZE(goodix_fwu_attrs) && !ret; i++)
		ret = sysfs_create_file(&module->kobj, &goodix_fwu_attrs[i].attr);
	if (ret) {
		ts_err("failed create fwu sysfs files");
		while (--i >= 0)
			sysfs_remove_file(&module->kobj, &goodix_fwu_attrs[i].attr);

		kobject_put(&module->kobj);
		ret = -EINVAL;
		goto exit_sysfs_init;
	}

	fw_ctrl->attr_fwimage.attr.name = "fwimage";
	fw_ctrl->attr_fwimage.attr.mode = 0666;
	fw_ctrl->attr_fwimage.size = 0;
	fw_ctrl->attr_fwimage.write = goodix_sysfs_fwimage_store;
	ret = sysfs_create_bin_file(&module->kobj,
			&fw_ctrl->attr_fwimage);

exit_sysfs_init:
	return ret;
}

static void goodix_fw_sysfs_remove(struct goodix_ext_module *module)
{
	struct fw_update_ctrl *fw_ctrl = module->priv_data;
	int i;

	sysfs_remove_bin_file(&module->kobj, &fw_ctrl->attr_fwimage);

	for (i = 0; i < ARRAY_SIZE(goodix_fwu_attrs); i++)
		sysfs_remove_file(&module->kobj,
				&goodix_fwu_attrs[i].attr);

	kobject_put(&module->kobj);
}

int goodix_do_fw_update(int mode)
{
	struct task_struct *fwu_thrd;
	struct fw_update_ctrl *fwu_ctrl = &goodix_fw_update_ctrl;
	int ret = 0;

	if (!fwu_ctrl->initialized) {
		ts_err("fw mode uninit");
		return -EINVAL;
	}

	fwu_ctrl->mode = mode;
	ts_debug("fw update mode 0x%x", mode);
	if (fwu_ctrl->mode & UPDATE_MODE_BLOCK) {
		ret = goodix_fw_update_thread(fwu_ctrl);
		ts_info("fw update return %d", ret);
		//return ret;
	} else {
		/* create and run update thread */
		fwu_thrd = kthread_run(goodix_fw_update_thread,
				fwu_ctrl, "goodix-fwu");
		if (IS_ERR_OR_NULL(fwu_thrd)) {
			ts_err("Failed to create update thread:%ld",
					PTR_ERR(fwu_thrd));
			return -EFAULT;
		}
		ts_info("success create fw update thread");
		//return 0;
	}
	return ret;
}

#ifdef CONFIG_AMLOGIC_MODIFY
EXPORT_SYMBOL_GPL(goodix_do_fw_update);
#endif

static int goodix_fw_update_init(struct goodix_ts_core *core_data,
				struct goodix_ext_module *module)
{
	int ret = 0;
	struct goodix_ts_board_data *ts_bdata = board_data(core_data);

	if (goodix_fw_update_ctrl.initialized) {
		ts_info("no need reinit");
		return ret;
	}

	if (!core_data || !ts_bdata || !core_data->ts_dev) {
		ts_err("core_data && ts_dev cann't be null");
		return -ENODEV;
	}

	mutex_lock(&goodix_fw_update_ctrl.mutex);
	module->priv_data = &goodix_fw_update_ctrl;

	goodix_fw_update_ctrl.ts_dev = core_data->ts_dev;
	goodix_fw_update_ctrl.allow_reset = true;
	goodix_fw_update_ctrl.allow_irq = true;
	goodix_fw_update_ctrl.allow_suspend = true;
	goodix_fw_update_ctrl.allow_resume = true;
	goodix_fw_update_ctrl.core_data = core_data;
	goodix_fw_update_ctrl.mode = 0;
	/* find a valid firmware image name */
	if (ts_bdata && ts_bdata->fw_name)
		strlcpy(goodix_fw_update_ctrl.fw_name, ts_bdata->fw_name,
			sizeof(goodix_fw_update_ctrl.fw_name));
	else
		strlcpy(goodix_fw_update_ctrl.fw_name, TS_DEFAULT_FIRMWARE,
			sizeof(goodix_fw_update_ctrl.fw_name));

	ret = goodix_fw_sysfs_init(core_data, module);
	if (ret) {
		ts_err("failed create fwupate sysfs node");
		goto err_out;
	}

	goodix_fw_update_ctrl.initialized = 1;
err_out:
	mutex_unlock(&goodix_fw_update_ctrl.mutex);
	return ret;
}

static int goodix_fw_update_exit(struct goodix_ts_core *core_data,
				struct goodix_ext_module *module)
{
	return 0;
}

static int goodix_fw_before_suspend(struct goodix_ts_core *core_data,
				struct goodix_ext_module *module)
{
	struct fw_update_ctrl *fwu_ctrl = module->priv_data;

	return fwu_ctrl->allow_suspend ?
				EVT_HANDLED : EVT_CANCEL_SUSPEND;
}

static int goodix_fw_before_resume(struct goodix_ts_core *core_data,
				struct goodix_ext_module *module)
{
	struct fw_update_ctrl *fwu_ctrl = module->priv_data;

	return fwu_ctrl->allow_resume ?
				EVT_HANDLED : EVT_CANCEL_RESUME;
}

static int goodix_fw_after_resume(struct goodix_ts_core *core_data,
				struct goodix_ext_module *module)
{
	return 0;
}

static int goodix_fw_irq_event(struct goodix_ts_core *core_data,
				struct goodix_ext_module *module)
{
	struct fw_update_ctrl *fwu_ctrl = module->priv_data;

	return fwu_ctrl->allow_irq ?
				EVT_HANDLED : EVT_CANCEL_IRQEVT;
}

static int goodix_fw_before_reset(struct goodix_ts_core *core_data,
				struct goodix_ext_module *module)
{
	struct fw_update_ctrl *fwu_ctrl = module->priv_data;

	return fwu_ctrl->allow_reset ?
				EVT_HANDLED : EVT_CANCEL_RESET;
}

static const struct goodix_ext_module_funcs goodix_ext_funcs = {
	.init = goodix_fw_update_init,
	.exit = goodix_fw_update_exit,
	.before_reset = goodix_fw_before_reset,
	.after_reset = NULL,
	.before_suspend = goodix_fw_before_suspend,
	.after_suspend = NULL,
	.before_resume = goodix_fw_before_resume,
	.after_resume = goodix_fw_after_resume,
	.irq_event = goodix_fw_irq_event,
};

static struct goodix_ext_module goodix_fwu_module = {
	.name = "goodix-fwu",
	.funcs = &goodix_ext_funcs,
	.priority = EXTMOD_PRIO_FWUPDATE,
};

static int __init goodix_fwu_module_init(void)
{
	ts_info("goodix_fwupdate_module_ini IN");
	mutex_init(&goodix_fw_update_ctrl.mutex);
	return goodix_register_ext_module(&goodix_fwu_module);
}

static void __exit goodix_fwu_module_exit(void)
{
	mutex_lock(&goodix_fw_update_ctrl.mutex);
	goodix_unregister_ext_module(&goodix_fwu_module);
	if (goodix_fw_update_ctrl.initialized) {
		goodix_fw_sysfs_remove(&goodix_fwu_module);
		goodix_fw_update_ctrl.initialized = 0;
	}
	mutex_lock(&goodix_fw_update_ctrl.mutex);
}

module_init(goodix_fwu_module_init);
module_exit(goodix_fwu_module_exit);

MODULE_DESCRIPTION("Goodix FWU Module");
MODULE_AUTHOR("Goodix, Inc.");
MODULE_LICENSE("GPL v2");
