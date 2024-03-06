// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>

#define LCDUKEY(fmt, args...)     pr_info("lcd: ukey: " fmt "", ## args)
#define LCDUKEYERR(fmt, args...)  pr_info("lcd: ukey error: " fmt "", ## args)

unsigned int cal_crc32(unsigned int crc, const unsigned char *buf,
		       int buf_len)
{
	static unsigned int s_crc32[16] = {
		0, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
		0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
		0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
		0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c,
	};
	unsigned int crcu32 = crc;
	unsigned char b;

	if (buf_len <= 0)
		return 0;
	if (!buf)
		return 0;

	crcu32 = ~crcu32;
	while (buf_len--) {
		b = *buf++;
		crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xf) ^ (b & 0xf)];
		crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xf) ^ (b >> 4)];
	}

	return ~crcu32;
}

#ifdef CONFIG_AMLOGIC_UNIFYKEY
bool lcd_unifykey_init_get(void)
{
	if (key_unify_get_init_flag())
		return true;
	return false;
}

int lcd_unifykey_len_check(int key_len, int len)
{
	if (key_len < len) {
		LCDUKEYERR("invalid unifykey length %d, need %d\n",
			   key_len, len);
		return -1;
	}
	return 0;
}

int lcd_unifykey_check(char *key_name)
{
	unsigned int key_exist = 0, keypermit;
	int ret;

	if (!key_name) {
		LCDUKEYERR("%s: key_name is null\n", __func__);
		return -1;
	}

	ret = key_unify_query(get_ukdev(), key_name, &key_exist, &keypermit);
	if (ret < 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDUKEYERR("%s: %s query exist error\n", __func__, key_name);
		return -1;
	}
	if (key_exist == 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDUKEYERR("%s: %s is not exist\n", __func__, key_name);
		return -1;
	}

	return 0;
}

int lcd_unifykey_get_size(char *key_name, int *len)
{
	int key_len;
	int ret;

	key_len = 0;
	ret = lcd_unifykey_check(key_name);
	if (ret < 0)
		return -1;
	ret = key_unify_size(get_ukdev(), key_name, &key_len);
	if (ret < 0)
		return -1;
	if (key_len == 0) {
		LCDUKEYERR("%s: %s size 0!\n", __func__, key_name);
		return -1;
	}
	*len = key_len;

	return 0;
}

int lcd_unifykey_get(char *key_name, unsigned char *buf, int len)
{
	struct aml_lcd_unifykey_header_s *key_header;
	unsigned int retry_cnt = 0, key_crc32;
	int key_len = 0;
	int ret;

	ret = lcd_unifykey_check(key_name);
	if (ret < 0)
		return -1;

lcd_unifykey_get_retry:
	ret = key_unify_read(get_ukdev(), key_name, buf, len, &key_len);
	if (ret < 0) {
		LCDUKEYERR("%s: %s error\n", __func__, key_name);
		return -1;
	}
	if (key_len != len) {
		LCDUKEYERR("%s: %s key_len(0x%x) and buf_size(0x%x) mismatch\n",
			__func__, key_name, key_len, len);
		return -1;
	}

	/* check header */
	if (key_len <= LCD_UKEY_HEAD_SIZE) {
		LCDUKEYERR("%s: %s key_len %d error\n", __func__, key_name, key_len);
		return -1;
	}
	key_header = (struct aml_lcd_unifykey_header_s *)buf;
	if (key_len != key_header->data_len) {  /* length check */
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDUKEYERR("%s: %s data_len %d is not match key_len %d\n",
				   __func__, key_name, key_header->data_len, key_len);
		}
		if (retry_cnt++ < LCD_UKEY_RETRY_CNT_MAX) {
			memset(buf, 0, key_len);
			goto lcd_unifykey_get_retry;
		}
		LCDUKEYERR("%s: %s failed\n", __func__, key_name);
		return -1;
	}
	key_crc32 = cal_crc32(0, &buf[4], (key_len - 4)); /* except crc32 */
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDUKEY("%s: %s crc32: 0x%08x, header_crc32: 0x%08x\n",
			__func__, key_name, key_crc32, key_header->crc32);
	}
	if (key_crc32 != key_header->crc32) {  /* crc32 check */
		LCDUKEYERR("%s: %s crc32 0x%08x is not match header_crc32 0x%08x\n",
			   __func__, key_name, key_crc32, key_header->crc32);
		if (retry_cnt++ < LCD_UKEY_RETRY_CNT_MAX) {
			memset(buf, 0, key_len);
			goto lcd_unifykey_get_retry;
		}
		LCDUKEYERR("%s: %s failed\n", __func__, key_name);
		return -1;
	}

	return 0;
}

int lcd_unifykey_get_tcon(char *key_name, unsigned char *buf, int len)
{
	struct lcd_tcon_init_block_header_s *init_header;
	unsigned int retry_cnt = 0, key_crc32;
	int key_len = 0;
	int ret;

	ret = lcd_unifykey_check(key_name);
	if (ret < 0)
		return -1;

lcd_unifykey_get_tcon_retry:
	ret = key_unify_read(get_ukdev(), key_name, buf, len, &key_len);
	if (ret < 0) {
		LCDUKEYERR("%s: %s error\n", __func__, key_name);
		return -1;
	}
	if (key_len != len) {
		LCDUKEYERR("%s: %s key_len(0x%x) and buf_size(0x%x) mismatch\n",
			__func__, key_name, key_len, len);
		return -1;
	}

	/* check header */
	if (key_len <= LCD_TCON_DATA_BLOCK_HEADER_SIZE) {
		LCDUKEYERR("%s: %s key_len %d error\n", __func__, key_name, key_len);
		return -1;
	}
	init_header = (struct lcd_tcon_init_block_header_s *)buf;
	if (key_len != init_header->block_size) {  /* length check */
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDUKEYERR("%s: %s block_size %d is not match key_len %d\n",
				   __func__, key_name, init_header->block_size, key_len);
		}
		if (retry_cnt++ < LCD_UKEY_RETRY_CNT_MAX) {
			memset(buf, 0, key_len);
			goto lcd_unifykey_get_tcon_retry;
		}
		LCDUKEYERR("%s: %s failed\n", __func__, key_name);
		return -1;
	}
	key_crc32 = cal_crc32(0, &buf[4], (key_len - 4)); /* except crc32 */
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDUKEY("%s: %s crc32: 0x%08x, header_crc32: 0x%08x\n",
			__func__, key_name, key_crc32, init_header->crc32);
	}
	if (key_crc32 != init_header->crc32) {  /* crc32 check */
		LCDUKEYERR("%s: %s crc32 0x%08x is not match header_crc32 0x%08x\n",
			   __func__, key_name, key_crc32, init_header->crc32);
		if (retry_cnt++ < LCD_UKEY_RETRY_CNT_MAX) {
			memset(buf, 0, key_len);
			goto lcd_unifykey_get_tcon_retry;
		}
		LCDUKEYERR("%s: %s failed\n", __func__, key_name);
		return -1;
	}

	return 0;
}

int lcd_unifykey_get_no_header(char *key_name, unsigned char *buf, int len)
{
	int key_len = 0;
	int ret;

	ret = lcd_unifykey_check(key_name);
	if (ret < 0)
		return -1;

	ret = key_unify_read(get_ukdev(), key_name, buf, len, &key_len);
	if (ret < 0) {
		LCDUKEYERR("%s: %s error\n", __func__, key_name);
		return -1;
	}
	if (key_len != len) {
		LCDUKEYERR("%s: %s key_len(0x%x) and buf_size(0x%x) mismatch\n",
			__func__, key_name, key_len, len);
		return -1;
	}

	return 0;
}

void lcd_unifykey_print(int index)
{
	unsigned char *buf, *pr_buf;
	char key_name[32];
	unsigned int key_len;
	int i, j, pr_len;
	int ret;

	pr_buf = kcalloc(128, sizeof(unsigned char), GFP_KERNEL);
	if (!pr_buf) {
		LCDUKEY("%s: buf malloc error\n", __func__);
		return;
	}

	if (index > 0)
		sprintf(key_name, "lcd%d", index);
	else
		sprintf(key_name, "lcd");
	ret = lcd_unifykey_get_size(key_name, &key_len);
	if (ret)
		goto lcd_ukey_print_next1;
	buf = kcalloc(key_len, sizeof(unsigned char), GFP_KERNEL);
	if (!buf) {
		LCDUKEY("%s: buf malloc error\n", __func__);
		return;
	}
	ret = lcd_unifykey_get(key_name, buf, key_len);
	if (ret < 0) {
		kfree(buf);
		goto lcd_ukey_print_next1;
	}
	LCDUKEY("%s: %s: %d\n", __func__, key_name, key_len);
	i = 0;
	while (1) {
		pr_len = sprintf(pr_buf, "0x%04x:", (i * 16));
		for (j = 0; j < 16; j++) {
			if ((i * 16 + j) < key_len) {
				pr_len += sprintf(pr_buf + pr_len, " %02x", buf[i * 16 + j]);
			} else {
				pr_info("%s\n", pr_buf);
				kfree(buf);
				goto lcd_ukey_print_next1;
			}
		}
		pr_info("%s\n", pr_buf);
		i++;
	}
	kfree(buf);

lcd_ukey_print_next1:
	if (index > 0)
		sprintf(key_name, "lcd%d_extern", index);
	else
		sprintf(key_name, "lcd_extern");
	ret = lcd_unifykey_get_size(key_name, &key_len);
	if (ret)
		goto lcd_ukey_print_next2;
	buf = kcalloc(key_len, sizeof(unsigned char), GFP_KERNEL);
	if (!buf) {
		LCDUKEY("%s: buf malloc error\n", __func__);
		return;
	}
	ret = lcd_unifykey_get(key_name, buf, key_len);
	if (ret < 0) {
		kfree(buf);
		goto lcd_ukey_print_next2;
	}
	LCDUKEY("%s: %s: %d\n", __func__, key_name, key_len);
	i = 0;
	while (1) {
		pr_len = sprintf(pr_buf, "0x%04x:", (i * 16));
		for (j = 0; j < 16; j++) {
			if ((i * 16 + j) < key_len) {
				pr_len += sprintf(pr_buf + pr_len, " %02x", buf[i * 16 + j]);
			} else {
				pr_info("%s\n", pr_buf);
				kfree(buf);
				goto lcd_ukey_print_next2;
			}
		}
		pr_info("%s\n", pr_buf);
		i++;
	}
	kfree(buf);

lcd_ukey_print_next2:
	if (index > 0)
		sprintf(key_name, "backlight%d", index);
	else
		sprintf(key_name, "backlight");
	ret = lcd_unifykey_get_size(key_name, &key_len);
	if (ret)
		goto lcd_ukey_print_next3;
	buf = kcalloc(key_len, sizeof(unsigned char), GFP_KERNEL);
	if (!buf) {
		LCDUKEY("%s: buf malloc error\n", __func__);
		return;
	}
	ret = lcd_unifykey_get(key_name, buf, key_len);
	if (ret < 0) {
		kfree(buf);
		goto lcd_ukey_print_next3;
	}
	LCDUKEY("%s: %s: %d\n", __func__, key_name, key_len);
	i = 0;
	while (1) {
		pr_len = sprintf(pr_buf, "0x%04x:", (i * 16));
		for (j = 0; j < 16; j++) {
			if ((i * 16 + j) < key_len) {
				pr_len += sprintf(pr_buf + pr_len, " %02x", buf[i * 16 + j]);
			} else {
				pr_info("%s\n", pr_buf);
				kfree(buf);
				goto lcd_ukey_print_next3;
			}
		}
		pr_info("%s\n", pr_buf);
		i++;
	}
	kfree(buf);

lcd_ukey_print_next3:
	kfree(pr_buf);
}

#else
/* dummy driver */
bool lcd_unifykey_init_get(void)
{
	return false;
}

int lcd_unifykey_len_check(int key_len, int len)
{
	LCDUKEYERR("Don't support unifykey\n");
	return -1;
}

int lcd_unifykey_check(char *key_name)
{
	LCDUKEYERR("Don't support unifykey\n");
	return -1;
}

int lcd_unifykey_get_size(char *key_name, int *len)
{
	LCDUKEYERR("Don't support unifykey\n");
	return -1;
}

int lcd_unifykey_get(char *key_name, unsigned char *buf, int len)
{
	LCDUKEYERR("Don't support unifykey\n");
	return -1;
}

int lcd_unifykey_get_tcon(char *key_name, unsigned char *buf, int len)
{
	LCDUKEYERR("Don't support unifykey\n");
	return -1;
}

int lcd_unifykey_get_no_header(char *key_name, unsigned char *buf, int len)
{
	LCDUKEYERR("Don't support unifykey\n");
	return -1;
}

void lcd_unifykey_print(int index)
{
	LCDUKEYERR("Don't support unifykey\n");
	return -1;
}

#endif

