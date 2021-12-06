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

#define LCDUKEY(fmt, args...)     pr_info("lcd ukey: " fmt "", ## args)
#define LCDUKEYERR(fmt, args...)  \
		pr_info("lcd ukey err: error: " fmt "", ## args)

#ifdef CONFIG_AMLOGIC_UNIFYKEY
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

int lcd_unifykey_header_check(unsigned char *buf,
			      struct aml_lcd_unifykey_header_s *header)
{
	header->crc32 = (buf[0] | (buf[1] << 8) |
			(buf[2] << 16) | (buf[3] << 24));
	header->data_len = (buf[4] | (buf[5] << 8));
	header->version = buf[6];
	header->block_next_flag = buf[7];
	header->block_cur_size = (buf[8] | (buf[9] << 8));

	return 0;
}

int lcd_unifykey_check(char *key_name)
{
	unsigned int key_exist, keypermit, key_len;
	unsigned char *buf;
	struct aml_lcd_unifykey_header_s key_header;
	int retry_cnt = 0;
	unsigned int key_crc32;
	int ret;

	if (!key_name) {
		LCDUKEYERR("%s: key_name is null\n", __func__);
		return -1;
	}

	key_exist = 0;
	key_len = 0;
	ret = key_unify_query(get_ukdev(), key_name, &key_exist, &keypermit);
	if (ret < 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDUKEYERR("%s query exist error\n", key_name);
		return -1;
	}
	if (key_exist == 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDUKEYERR("%s is not exist\n", key_name);
		return -1;
	}

	ret = key_unify_size(get_ukdev(), key_name, &key_len);
	if (ret < 0) {
		LCDUKEYERR("%s query size error\n", key_name);
		return -1;
	}
	if (key_len == 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDUKEY("%s size is zero\n", key_name);
		return -1;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDUKEY("%s size: %d\n", key_name, key_len);

	buf = kzalloc((sizeof(unsigned char) * key_len), GFP_KERNEL);
	if (!buf) {
		LCDUKEYERR("%s: Not enough memory\n", __func__);
		return -1;
	}

lcd_unifykey_check_read:
	ret = key_unify_read(get_ukdev(), key_name, buf, key_len, &key_len);
	if (ret < 0) {
		LCDUKEYERR("%s unify read error\n", key_name);
		goto lcd_unifykey_check_err;
	}

	/* check header */
	if (key_len <= LCD_UKEY_HEAD_SIZE) {
		LCDUKEYERR("%s unify key_len %d error\n", key_name, key_len);
		goto lcd_unifykey_check_err;
	}
	lcd_unifykey_header_check(buf, &key_header);
	if (key_len != key_header.data_len) {  /* length check */
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDUKEYERR("data_len %d is not match key_len %d\n",
				   key_header.data_len, key_len);
		}
		if (retry_cnt < LCD_UKEY_RETRY_CNT_MAX) {
			retry_cnt++;
			memset(buf, 0, key_len);
			goto lcd_unifykey_check_read;
		}
		LCDUKEYERR("%s: load unifykey failed\n", key_name);
		goto lcd_unifykey_check_err;
	}
	key_crc32 = cal_crc32(0, &buf[4], (key_len - 4)); /* except crc32 */
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDUKEY("crc32: 0x%08x, header_crc32: 0x%08x\n",
			key_crc32, key_header.crc32);
	}
	if (key_crc32 != key_header.crc32) {  /* crc32 check */
		LCDUKEYERR("crc32 0x%08x is not match header_crc32 0x%08x\n",
			   key_header.crc32, key_crc32);
		if (retry_cnt < LCD_UKEY_RETRY_CNT_MAX) {
			retry_cnt++;
			memset(buf, 0, key_len);
			goto lcd_unifykey_check_read;
		}
		LCDUKEYERR("%s: load unifykey failed\n", key_name);
		goto lcd_unifykey_check_err;
	}

	kfree(buf);
	return 0;

lcd_unifykey_check_err:
	kfree(buf);
	return -1;
}

static int lcd_unifykey_check_tcon(char *key_name)
{
	unsigned int key_exist, keypermit, key_len, data_size;
	unsigned char *buf;
	int retry_cnt = 0;
	unsigned int key_crc32, raw_crc32;
	int ret;

	if (!key_name) {
		LCDUKEYERR("%s: key_name is null\n", __func__);
		return -1;
	}

	key_exist = 0;
	key_len = 0;
	ret = key_unify_query(get_ukdev(), key_name, &key_exist, &keypermit);
	if (ret < 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDUKEYERR("%s query exist error\n", key_name);
		return -1;
	}
	if (key_exist == 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDUKEYERR("%s is not exist\n", key_name);
		return -1;
	}

	ret = key_unify_size(get_ukdev(), key_name, &key_len);
	if (ret < 0) {
		LCDUKEYERR("%s query size error\n", key_name);
		return -1;
	}
	if (key_len == 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDUKEY("%s size is zero\n", key_name);
		return -1;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDUKEY("%s size: %d\n", key_name, key_len);

	buf = kzalloc((sizeof(unsigned char) * key_len), GFP_KERNEL);
	if (!buf) {
		LCDUKEYERR("%s: Not enough memory\n", __func__);
		return -1;
	}

lcd_unifykey_check_tcon_read:
	ret = key_unify_read(get_ukdev(), key_name, buf, key_len, &key_len);
	if (ret < 0) {
		LCDUKEYERR("%s unify read error\n", key_name);
		goto lcd_unifykey_check_tcon_err;
	}

	/* check header */
	if (key_len <= LCD_TCON_DATA_BLOCK_HEADER_SIZE) {
		LCDUKEYERR("%s unify key_len %d error\n", key_name, key_len);
		goto lcd_unifykey_check_tcon_err;
	}
	data_size = (buf[8] | (buf[9] << 8) |
		     (buf[10] << 16) | (buf[11] << 24));
	if (key_len != data_size) {  /* length check */
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDUKEYERR("data_len %d is not match key_len %d\n",
				   data_size, key_len);
		}
		if (retry_cnt < LCD_UKEY_RETRY_CNT_MAX) {
			retry_cnt++;
			memset(buf, 0, key_len);
			goto lcd_unifykey_check_tcon_read;
		}
		LCDUKEYERR("%s: load unifykey failed\n", key_name);
		goto lcd_unifykey_check_tcon_err;
	}
	raw_crc32 = (buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24));
	key_crc32 = cal_crc32(0, &buf[4], (key_len - 4)); /* except crc32 */
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDUKEY("crc32: 0x%08x, header_crc32: 0x%08x\n",
			key_crc32, raw_crc32);
	}
	if (key_crc32 != raw_crc32) {  /* crc32 check */
		LCDUKEYERR("crc32 0x%08x is not match header_crc32 0x%08x\n",
			   raw_crc32, key_crc32);
		if (retry_cnt < LCD_UKEY_RETRY_CNT_MAX) {
			retry_cnt++;
			memset(buf, 0, key_len);
			goto lcd_unifykey_check_tcon_read;
		}
		LCDUKEYERR("%s: load unifykey failed\n", key_name);
		goto lcd_unifykey_check_tcon_err;
	}

	kfree(buf);
	return 0;

lcd_unifykey_check_tcon_err:
	kfree(buf);
	return -1;
}

int lcd_unifykey_get(char *key_name, unsigned char *buf, int *len)
{
	int key_len;
	int ret;

	key_len = 0;
	ret = lcd_unifykey_check(key_name);
	if (ret < 0)
		return -1;
	ret = key_unify_size(get_ukdev(), key_name, &key_len);
	if (key_len > *len) {
		LCDUKEYERR("%s size(%d) is bigger than buf_size(%d)\n",
			key_name, key_len, *len);
		return -1;
	}
	*len = key_len;

	ret = key_unify_read(get_ukdev(), key_name, buf, key_len, &key_len);
	if (ret < 0) {
		LCDUKEYERR("%s unify read error\n", key_name);
		return -1;
	}
	return 0;
}

int lcd_unifykey_get_tcon(char *key_name, unsigned char *buf, int *len)
{
	int key_len;
	int ret;

	key_len = 0;
	ret = lcd_unifykey_check_tcon(key_name);
	if (ret < 0)
		return -1;
	ret = key_unify_size(get_ukdev(), key_name, &key_len);
	if (key_len > *len) {
		LCDUKEYERR("%s size(0x%x) is bigger than buf_size(0x%x)\n",
			key_name, key_len, *len);
		return -1;
	}
	*len = key_len;

	ret = key_unify_read(get_ukdev(), key_name, buf, key_len, &key_len);
	if (ret < 0) {
		LCDUKEYERR("%s unify read error\n", key_name);
		return -1;
	}
	return 0;
}

int lcd_unifykey_check_no_header(char *key_name)
{
	unsigned int key_exist, keypermit, key_len;
	int ret;

	key_exist = 0;
	key_len = 0;
	ret = key_unify_query(get_ukdev(), key_name, &key_exist, &keypermit);
	if (ret < 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDUKEYERR("%s query exist error\n", key_name);
		return -1;
	}
	if (key_exist == 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDUKEYERR("%s is not exist\n", key_name);
		return -1;
	}

	ret = key_unify_size(get_ukdev(), key_name, &key_len);
	if (ret < 0) {
		LCDUKEYERR("%s query size error\n", key_name);
		return -1;
	}
	if (key_len == 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDUKEY("%s size is zero\n", key_name);
		return -1;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDUKEY("%s size: %d\n", key_name, key_len);

	return 0;
}

int lcd_unifykey_get_no_header(char *key_name, unsigned char *buf, int *len)
{
	int key_len;
	int ret;

	key_len = 0;
	ret = lcd_unifykey_check_no_header(key_name);
	if (ret < 0)
		return -1;
	ret = key_unify_size(get_ukdev(), key_name, &key_len);
	if (key_len > *len) {
		LCDUKEYERR("%s size(%d) is bigger than buf_size(%d)\n",
			   key_name, key_len, *len);
		return -1;
	}
	*len = key_len;

	ret = key_unify_read(get_ukdev(), key_name, buf, key_len, &key_len);
	if (ret < 0) {
		LCDUKEYERR("%s unify read error\n", key_name);
		return -1;
	}
	return 0;
}

void lcd_unifykey_print(void)
{
	unsigned char *buf;
	char *key_name;
	unsigned int key_len;
	int i, j;
	int ret;

	buf = kmalloc(600 * sizeof(unsigned char), GFP_KERNEL);
	if (!buf) {
		LCDUKEY("lcd_unifykey buf malloc error\n");
		return;
	}
	key_name = "lcd";
	key_len = LCD_UKEY_LCD_SIZE;
	ret = lcd_unifykey_get(key_name, buf, &key_len);
	if (ret < 0)
		goto exit_print_backlight;
	LCDUKEY("%s: %s: %d\n", __func__, key_name, key_len);
	i = 0;
	while (1) {
		pr_info("0x%08x: ", (i * 16));
		for (j = 0; j < 16; j++) {
			if ((i * 16 + j) < key_len) {
				pr_info("0x%02x ", buf[i * 16 + j]);
			} else {
				pr_info("\n");
				goto exit_print_lcd;
			}
		}
		pr_info("\n");
		i++;
	}

exit_print_lcd:
	key_name = "lcd_extern";
	key_len = LCD_UKEY_LCD_EXT_SIZE;
	ret = lcd_unifykey_get(key_name, buf, &key_len);
	if (ret < 0)
		goto exit_print_backlight;
	LCDUKEY("%s: %s: %d\n", __func__, key_name, key_len);
	i = 0;
	while (1) {
		pr_info("0x%08x: ", (i * 16));
		for (j = 0; j < 16; j++) {
			if ((i * 16 + j) < key_len) {
				pr_info("0x%02x ", buf[i * 16 + j]);
			} else {
				pr_info("\n");
				goto exit_print_lcd_ext;
			}
		}
		pr_info("\n");
		i++;
	}

exit_print_lcd_ext:
	key_name = "backlight";
	key_len = LCD_UKEY_BL_SIZE;
	ret = lcd_unifykey_get(key_name, buf, &key_len);
	if (ret < 0)
		goto exit_print_backlight;
	LCDUKEY("%s: %s: %d\n", __func__, key_name, key_len);
	i = 0;
	while (1) {
		pr_info("0x%08x: ", (i * 16));
		for (j = 0; j < 16; j++) {
			if ((i * 16 + j) < key_len) {
				pr_info("0x%02x ", buf[i * 16 + j]);
			} else {
				pr_info("\n");
				goto exit_print_backlight;
			}
		}
		pr_info("\n");
		i++;
	}

exit_print_backlight:
	kfree(buf);
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

int lcd_unifykey_header_check(unsigned char *buf,
			      struct aml_lcd_unifykey_header_s *header)
{
	LCDUKEYERR("Don't support unifykey\n");
	return -1;
}

int lcd_unifykey_check(char *key_name)
{
	LCDUKEYERR("Don't support unifykey\n");
	return -1;
}

int lcd_unifykey_get(char *key_name, unsigned char *buf, int *len)
{
	LCDUKEYERR("Don't support unifykey\n");
	return -1;
}

int lcd_unifykey_get_tcon(char *key_name, unsigned char *buf, int *len)
{
	LCDUKEYERR("Don't support unifykey\n");
	return -1;
}

int lcd_unifykey_check_no_header(char *key_name)
{
	LCDUKEYERR("Don't support unifykey\n");
	return -1;
}

int lcd_unifykey_get_no_header(char *key_name, unsigned char *buf, int *len)
{
	LCDUKEYERR("Don't support unifykey\n");
	return -1;
}

void lcd_unifykey_print(void)
{
	LCDUKEYERR("Don't support unifykey\n");
}

#endif

