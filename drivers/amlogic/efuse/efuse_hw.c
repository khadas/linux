/*
 * drivers/amlogic/efuse/efuse_hw.c
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

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/amlogic/iomap.h>
#ifndef CONFIG_ARM64
#include <asm/opcodes-sec.h>
#endif
#include "efuse_regs.h"
#include "efuse.h"

#ifdef EFUSE_DEBUG

static unsigned long efuse_test_buf_32[EFUSE_DWORDS] = {0};
static unsigned char *efuse_test_buf_8 = (unsigned char *)efuse_test_buf_32;

static void __efuse_write_byte_debug(unsigned long addr, unsigned char data)
{
	efuse_test_buf_8[addr] = data;
}

static void __efuse_read_dword_debug(unsigned long addr, unsigned long *data)
{
	*data = efuse_test_buf_32[addr >> 2];
}

void __efuse_debug_init(void)
{
	/*__efuse_write_byte_debug(0, 0xbf);
	__efuse_write_byte_debug(1, 0xff);
	__efuse_write_byte_debug(2, 0x00);

	__efuse_write_byte_debug(3, 0x02);
	__efuse_write_byte_debug(4, 0x81);
	__efuse_write_byte_debug(5, 0x0f);
	__efuse_write_byte_debug(6, 0x00);
	__efuse_write_byte_debug(7, 0x00);

	__efuse_write_byte_debug(8, 0xaf);
	__efuse_write_byte_debug(9, 0x32);
	__efuse_write_byte_debug(10, 0x76);
	__efuse_write_byte_debug(135, 0xb2);*/
}
#endif

#ifdef CONFIG_ARM64
static long meson_efuse_fn_smc(struct efuse_hal_api_arg *arg)
{
	long ret;
	unsigned cmd, offset, size;
	unsigned long *retcnt = (unsigned long *)(arg->retcnt);

	register unsigned x0 asm("x0");
	register unsigned x1 asm("x1");
	register unsigned x2 asm("x2");

	if (!sharemem_input_base || !sharemem_output_base)
		return -1;

	if (arg->cmd == EFUSE_HAL_API_READ)
			cmd = efuse_read_cmd;
	else
			cmd = efuse_write_cmd;
	offset = arg->offset;
	size = arg->size;

	if (arg->cmd == EFUSE_HAL_API_WRITE)
		memcpy((void *)sharemem_input_base,
			(const void *)arg->buffer, size);

	asm __volatile__("" : : : "memory");

	x0 = cmd;
	x1 = offset;
	x2 = size;
	asm volatile(
			__asmeq("%0", "x0")
			__asmeq("%1", "x0")
			__asmeq("%2", "x1")
			__asmeq("%3", "x2")
			"smc	#0\n"
		: "=r"(x0)
		: "r"(x0), "r"(x1), "r"(x2));
	ret = x0;
	*retcnt = x0;

	if ((arg->cmd == EFUSE_HAL_API_READ) && (ret != 0))
		memcpy((void *)arg->buffer,
			(const void *)sharemem_output_base, ret);

	if (!ret)
		return -1;
	else
		return 0;
}
#else
static long meson_efuse_fn_smc(struct efuse_hal_api_arg *arg)
{
	long ret;
	unsigned cmd, offset, size;
	unsigned long *retcnt = (unsigned long *)(arg->retcnt);

	register unsigned r0 asm("r0");
	register unsigned r1 asm("r1");
	register unsigned r2 asm("r2");

	if (!sharemem_input_base || !sharemem_output_base)
		return -1;

	if (arg->cmd == EFUSE_HAL_API_READ)
			cmd = efuse_read_cmd;
	else
			cmd = efuse_write_cmd;
	offset = arg->offset;
	size = arg->size;

	if (arg->cmd == EFUSE_HAL_API_WRITE)
		memcpy((void *)sharemem_input_base,
			(const void *)arg->buffer, size);

	asm __volatile__("" : : : "memory");

	r0 = cmd;
	r1 = offset;
	r2 = size;
	asm volatile(
			__asmeq("%0", "r0")
			__asmeq("%1", "r0")
			__asmeq("%2", "r1")
			__asmeq("%3", "r2")
			__SMC(0)
		: "=r"(r0)
		: "r"(r0), "r"(r1), "r"(r2));
	ret = r0;
	*retcnt = r0;

	if ((arg->cmd == EFUSE_HAL_API_READ) && (ret != 0))
		memcpy((void *)arg->buffer,
			(const void *)sharemem_output_base, ret);

	if (!ret)
		return -1;
	else
		return 0;
}
#endif

int meson_trustzone_efuse(struct efuse_hal_api_arg *arg)
{
	int ret;
	if (!arg)
		return -1;

	set_cpus_allowed_ptr(current, cpumask_of(0));
	ret = meson_efuse_fn_smc(arg);
	set_cpus_allowed_ptr(current, cpu_all_mask);
	return ret;
}

static ssize_t __efuse_read(char *buf, size_t count, loff_t *ppos)
{
	unsigned long *contents = kzalloc(sizeof(unsigned long)*EFUSE_DWORDS,
		GFP_KERNEL);
	unsigned pos = *ppos;
#ifdef EFUSE_DEBUG
	unsigned long *pdw;
	char *tmp_p;
	/*pos may not align to 4*/
	unsigned int dwsize = (count + 3 +  pos%4) >> 2;
#else
	struct efuse_hal_api_arg arg;
	unsigned long retcnt;
	int ret;
#endif

	if (!contents) {
		pr_info("memory not enough\n");
		return -ENOMEM;
	}

	if (pos >= EFUSE_BYTES)
		return 0;

	if (count > EFUSE_BYTES - pos)
		count = EFUSE_BYTES - pos;
	if (count > EFUSE_BYTES)
		return -EFAULT;

#ifdef EFUSE_DEBUG
	for (pdw = contents + pos/4; dwsize-- > 0 && pos < EFUSE_BYTES;
		pos += 4, ++pdw)
		__efuse_read_dword_debug(pos, pdw);

	tmp_p = (char *)contents;
	tmp_p += *ppos;
	memcpy(buf, tmp_p, count);
	*ppos += count;
#else
	arg.cmd = EFUSE_HAL_API_READ;
	arg.offset = pos;
	arg.size = count;
	arg.buffer = (unsigned long)contents;
	arg.retcnt = (unsigned long)(&retcnt);
	ret = meson_trustzone_efuse(&arg);

	if (ret == 0) {
		count = retcnt;
		*ppos += retcnt;
		memcpy(buf, contents, retcnt);
	} else
		count = 0;
#endif

	/*if (contents)*/
		kfree(contents);
	return count;
}

static ssize_t __efuse_write(const char *buf, size_t count, loff_t *ppos)
{
	unsigned pos = *ppos;
#ifdef EFUSE_DEBUG
	unsigned char *pc;
#else
	struct efuse_hal_api_arg arg;
	unsigned int retcnt;
	int ret;
#endif

	if (pos >= EFUSE_BYTES)
		return 0;       /* Past EOF */
	if (count > EFUSE_BYTES - pos)
		count = EFUSE_BYTES - pos;
	if (count > EFUSE_BYTES)
		return -EFAULT;

#ifdef EFUSE_DEBUG
	for (pc = (char *)buf; count--; ++pos, ++pc)
		__efuse_write_byte_debug(pos, *pc);

	*ppos = pos;
	return (const char *)pc - buf;
#else
	arg.cmd = EFUSE_HAL_API_WRITE;
	arg.offset = pos;
	arg.size = count;
	arg.buffer = (unsigned long)buf;
	arg.retcnt = (unsigned long)(&retcnt);
	ret = meson_trustzone_efuse(&arg);
	if (ret == 0) {
		*ppos = pos+retcnt;
		return retcnt;
	} else
		return 0;
#endif
}

ssize_t aml__efuse_read(char *buf, size_t count, loff_t *ppos)
{
	return __efuse_read(buf, count, ppos);
}
ssize_t aml__efuse_write(const char *buf, size_t count, loff_t *ppos)
{
	return __efuse_write(buf, count, ppos);
}

/* ================================================ */

/* #define SOC_CHIP_TYPE_TEST */
#ifdef SOC_CHIP_TYPE_TEST
static char *soc_chip[] = {
	{"efuse soc chip m0"},
	{"efuse soc chip m1"},
	{"efuse soc chip m3"},
	{"efuse soc chip m6"},
	{"efuse soc chip m6tv"},
	{"efuse soc chip m6tvlite"},
	{"efuse soc chip m8"},
	{"efuse soc chip m6tvd"},
	{"efuse soc chip m8baby"},
	{"efuse soc chip unknow"},
};
#endif

struct efuse_chip_identify_t {
	unsigned int chiphw_mver;
	unsigned int chiphw_subver;
	unsigned int chiphw_thirdver;
	enum efuse_socchip_type_e type;
};
static const struct efuse_chip_identify_t efuse_chip_hw_info[] = {
	{
		.chiphw_mver = 27,
		.chiphw_subver = 0,
		.chiphw_thirdver = 0,
		.type = EFUSE_SOC_CHIP_M8BABY
	},
	{
		.chiphw_mver = 25,
		.chiphw_subver = 0,
		.chiphw_thirdver = 0,
		.type = EFUSE_SOC_CHIP_M8
	},
};
#define EFUSE_CHIP_HW_INFO_NUM  (sizeof(efuse_chip_hw_info)/ \
	sizeof(efuse_chip_hw_info[0]))


enum efuse_socchip_type_e efuse_get_socchip_type(void)
{
	enum efuse_socchip_type_e type;
	unsigned int regval;
	int i;
	struct efuse_chip_identify_t *pinfo =
		(struct efuse_chip_identify_t *)&efuse_chip_hw_info[0];

	type = EFUSE_SOC_CHIP_UNKNOW;

		regval = aml_read_cbus(ASSIST_HW_REV);
		/* pr_info("chip ASSIST_HW_REV reg:%d\n",regval); */
		for (i = 0; i < EFUSE_CHIP_HW_INFO_NUM; i++) {
			if (pinfo->chiphw_mver == regval) {
				type = pinfo->type;
				break;
			}
			pinfo++;
		}

#ifdef SOC_CHIP_TYPE_TEST
	pr_info("%s\n", soc_chip[type]);
#endif
	return type;
}

static int efuse_checkversion(char *buf)
{
	enum efuse_socchip_type_e soc_type;
	int i;
	int ver = buf[0];

	for (i = 0; i < efuseinfo_num; i++) {
		if (efuseinfo[i].version == ver) {
			soc_type = efuse_get_socchip_type();
			switch (soc_type) {
			case EFUSE_SOC_CHIP_M8:
			case EFUSE_SOC_CHIP_M8BABY:
				if (ver != M8_EFUSE_VERSION_SERIALNUM_V1)
					ver = -1;
				break;
			case EFUSE_SOC_CHIP_UNKNOW:
			default:
				pr_info("%s:%d soc is unknow\n",
					__func__, __LINE__);
				ver = -1;
				break;
			}
			return ver;
		}
	}

	return -1;
}


static int efuse_set_versioninfo(struct efuseinfo_item_t *info)
{
	int ret =  -1;
	enum efuse_socchip_type_e soc_type;
	strcpy(info->title, "version");
	info->id = EFUSE_VERSION_ID;
	info->bch_reverse = 0;

	soc_type = efuse_get_socchip_type();
	switch (soc_type) {

	case EFUSE_SOC_CHIP_M8:
	case EFUSE_SOC_CHIP_M8BABY:
		info->offset = M8_EFUSE_VERSION_OFFSET; /* 509 */
		info->data_len = M8_EFUSE_VERSION_DATA_LEN;
		info->enc_len = M8_EFUSE_VERSION_ENC_LEN;
		info->bch_en = M8_EFUSE_VERSION_BCH_EN;
		ret = 0;
		break;
	case EFUSE_SOC_CHIP_UNKNOW:
	default:
		pr_info("%s:%d chip is unknow, use default M8 chip\n",
			 __func__, __LINE__);
		info->offset = M8_EFUSE_VERSION_OFFSET; /* 509 */
		info->data_len = M8_EFUSE_VERSION_DATA_LEN;
		info->enc_len = M8_EFUSE_VERSION_ENC_LEN;
		info->bch_en = M8_EFUSE_VERSION_BCH_EN;
		ret = 0;
		break;
		break;
	}

	return ret;
}


static int efuse_readversion(void)
{
	char ver_buf[4], buf[4];
	struct efuseinfo_item_t info;
	int ret;

	ret = efuse_set_versioninfo(&info);
	if (ret < 0)
		return ret;

	memset(ver_buf, 0, sizeof(ver_buf));
	memset(buf, 0, sizeof(buf));

	__efuse_read(buf, info.enc_len, &info.offset);
	memcpy(ver_buf, buf, sizeof(buf));

	ret = efuse_checkversion(ver_buf);   /* m3,m6,m8 */
	if ((ret > 0) && (ver_buf[0] != 0))
		return ver_buf[0];  /* version right */
	else
		return -1; /* version err */
}

static int efuse_getinfo_byPOS(unsigned pos, struct efuseinfo_item_t *info)
{
	int ver;
	int i;
	struct efuseinfo_t *vx = NULL;
	struct efuseinfo_item_t *item = NULL;
	int size;
	int ret = -1;
	enum efuse_socchip_type_e soc_type;

	unsigned versionPOS;

	soc_type = efuse_get_socchip_type();
	switch (soc_type) {

	case EFUSE_SOC_CHIP_M8:
	case EFUSE_SOC_CHIP_M8BABY:
		versionPOS = M8_EFUSE_VERSION_OFFSET; /* 509 */
		break;

	case EFUSE_SOC_CHIP_UNKNOW:
	default:
		pr_info("%s:%d chip is unknow\n", __func__, __LINE__);
		return -1;
		/* break; */
	}

	if (pos == versionPOS) {
		ret = efuse_set_versioninfo(info);
		return ret;
	}

	ver = efuse_readversion();
		if (ver < 0) {
			pr_info("efuse version is not selected.\n");
			return -1;
		}

		for (i = 0; i < efuseinfo_num; i++) {
			if (efuseinfo[i].version == ver) {
				vx = &(efuseinfo[i]);
				break;
			}
		}
		if (!vx) {
			pr_info("efuse version %d is not supported.\n", ver);
			return -1;
		}

		item = vx->efuseinfo_version;
		size = vx->size;
		ret = -1;
		for (i = 0; i < size; i++, item++) {
			if (pos == item->offset) {
				strcpy(info->title, item->title);
				info->offset = item->offset;
				info->id = item->id;
				info->data_len = item->data_len;
				info->enc_len = item->enc_len;
				info->bch_en = item->bch_en;
				info->bch_reverse = item->bch_reverse;
					/* /what's up ? typo error? */
				ret = 0;
				break;
			}
		}

		/* if((ret < 0) && (efuse_getinfoex != NULL)) */
		/* ret = efuse_getinfoex(id, info); */
		if (ret < 0)
			pr_info("POS:%d is not found.\n", pos);

		return ret;
}

/* ================================================ */
/* public interface */
/* ================================================ */
int efuse_getinfo_byID(unsigned id, struct efuseinfo_item_t *info)
{
	int ver;
	int i;
	struct efuseinfo_t *vx = NULL;
	struct efuseinfo_item_t *item = NULL;
	int size;
	int ret = -1;

	if (id == EFUSE_VERSION_ID) {
		ret = efuse_set_versioninfo(info);
		return ret;
	}

	ver = efuse_readversion();
	if (ver < 0) {
		pr_info("efuse version is not selected.\n");
		return -1;
	}

	for (i = 0; i < efuseinfo_num; i++) {
		if (efuseinfo[i].version == ver) {
			vx = &(efuseinfo[i]);
			break;
		}
	}
	if (!vx) {
		pr_info("efuse version %d is not supported.\n", ver);
		return -1;
	}

		item = vx->efuseinfo_version;
		size = vx->size;
		ret = -1;
		for (i = 0; i < size; i++, item++) {
			if (id == item->id) {
				strcpy(info->title, item->title);
				info->offset = item->offset;
				info->id = item->id;
				info->data_len = item->data_len;
				info->enc_len = item->enc_len;
				info->bch_en = item->bch_en;
				info->bch_reverse = item->bch_reverse;
				ret = 0;
				break;
			}
		}

		if (ret < 0)
			pr_info("ID:%d is not found.\n", id);

		return ret;
}


int check_if_efused(loff_t pos, size_t count)
{
	loff_t local_pos = pos;
	int i;
	unsigned char *buf = NULL;
	struct efuseinfo_item_t info;
	unsigned enc_len;

	if (efuse_getinfo_byPOS(pos, &info) < 0) {
		pr_info("not found the position:%lld.\n", pos);
		return -1;
	}
	 if (count > info.data_len) {
		pr_info("data length: %zd is out of EFUSE layout!\n", count);
		return -1;
	}
	if (count == 0) {
		pr_info("data length: 0 is error!\n");
		return -1;
	}

	enc_len = info.enc_len;
	buf = kzalloc(sizeof(char)*enc_len, GFP_KERNEL);
	if (buf) {
		if (__efuse_read(buf, enc_len, &local_pos) == enc_len) {
			for (i = 0; i < enc_len; i++) {
				if (buf[i]) {
					pr_info("pos %zd value is %d",
						(size_t)(pos + i), buf[i]);
					return 1;
				}
			}
		}
	} else {
		pr_info("no memory\n");
		return -ENOMEM;
	}
	kfree(buf);
	buf = NULL;
	return 0;
}

int efuse_read_item(char *buf, size_t count, loff_t *ppos)
{

	unsigned enc_len;
	char *enc_buf = NULL;
	char *data_buf = NULL;

	char *penc = NULL;
	char *pdata = NULL;
	int reverse = 0;
	unsigned pos = (unsigned)*ppos;
	struct efuseinfo_item_t info;

	if (efuse_getinfo_byPOS(pos, &info) < 0) {
		pr_info("not found the position:%d.\n", pos);
		return -1;
	}

	if (count > info.data_len) {
		pr_info("data length: %zd is out of EFUSE layout!\n", count);
		return -1;
	}
	if (count == 0) {
		pr_info("data length: 0 is error!\n");
		return -1;
	}

	enc_len = info.enc_len;
	reverse = info.bch_reverse;
	enc_buf = kzalloc(sizeof(char)*EFUSE_BYTES, GFP_KERNEL);
	if (!enc_buf) {
		pr_info("memory not enough\n");
		return -ENOMEM;
	}
	data_buf = kzalloc(sizeof(char)*EFUSE_BYTES, GFP_KERNEL);
	if (!data_buf) {
		/*if (enc_buf)*/
			kfree(enc_buf);
		pr_info("memory not enough\n");
		return -ENOMEM;
	}

	penc = enc_buf;
	pdata = data_buf;
	__efuse_read(pdata, enc_len, ppos);

	memcpy(buf, data_buf, count);

	/*if (enc_buf)*/
		kfree(enc_buf);
	/*if (data_buf)*/
		kfree(data_buf);
	return count;
}

int efuse_write_item(char *buf, size_t count, loff_t *ppos)
{
	char *enc_buf = NULL;
	char *data_buf = NULL;
	char *pdata = NULL;
	char *penc = NULL;
	unsigned enc_len, data_len, reverse;
	unsigned pos = (unsigned)*ppos;
	struct efuseinfo_item_t info;

	if (efuse_getinfo_byPOS(pos, &info) < 0) {
		pr_info("not found the position:%d.\n", pos);
		return -1;
	}
#ifndef CONFIG_EFUSE_WRITE_VERSION_PERMIT
	if (strcmp(info.title, "version") == 0) {
		pr_info("prohibit write version in kernel\n");
		return 0;
	}
#endif

	if (count > info.data_len) {
		pr_info("data length: %zd is out of EFUSE layout!\n", count);
		return -1;
	}
	if (count == 0) {
		pr_info("data length: 0 is error!\n");
		return -1;
	}

	enc_buf = kzalloc(sizeof(char)*EFUSE_BYTES, GFP_KERNEL);
	if (!enc_buf) {
		pr_info("memory not enough\n");
		return -ENOMEM;
	}
	data_buf = kzalloc(sizeof(char)*EFUSE_BYTES, GFP_KERNEL);
	if (!data_buf) {
		/*if (enc_buf)*/
			kfree(enc_buf);
		pr_info("memory not enough\n");
		return -ENOMEM;
	}

	memcpy(data_buf, buf, count);

	pdata = data_buf;
	penc = enc_buf;
	enc_len = info.enc_len;
	data_len = info.data_len;
	reverse = info.bch_reverse;

	memcpy(penc, pdata, enc_len);

	__efuse_write(enc_buf, enc_len, ppos);

	kfree(enc_buf);
	kfree(data_buf);

	return enc_len;
}

