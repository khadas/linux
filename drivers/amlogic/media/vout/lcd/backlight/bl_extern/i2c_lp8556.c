#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/amlogic/media/vout/lcd/aml_bl_extern.h>
#include "bl_extern.h"

#define BL_EXTERN_NAME			"i2c_lp8556"

static int bl_extern_power_cmd_dynamic_size(struct bl_extern_driver_s *bext,
					    unsigned char *table, int flag)
{
	int i = 0, j, step = 0, max_len = 0;
	unsigned char type, cmd_size;
	int delay_ms, ret = 0;

	if (flag)
		max_len = bext->config.init_on_cnt;
	else
		max_len = bext->config.init_off_cnt;

	while ((i + 1) < max_len) {
		type = table[i];
		if (type == LCD_EXT_CMD_TYPE_END)
			break;
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
			BLEX("[%d]: %s: step %d: type=0x%02x, cmd_size=%d\n",
			     bext->index, __func__, step, type, table[i + 1]);
		}
		cmd_size = table[i + 1];
		if (!cmd_size)
			goto power_cmd_dynamic_next;
		if ((i + 2 + cmd_size) > max_len)
			break;

		if (type == LCD_EXT_CMD_TYPE_NONE) {
			/* do nothing */
		} else if (type == LCD_EXT_CMD_TYPE_DELAY) {
			delay_ms = 0;
			for (j = 0; j < cmd_size; j++)
				delay_ms += table[i + 2 + j];
			if (delay_ms > 0)
				mdelay(delay_ms);
		} else if (type == LCD_EXT_CMD_TYPE_CMD) {
			ret = bl_extern_i2c_write(i2c_dev->client,
						  &table[i + 2], cmd_size);
		} else if (type == LCD_EXT_CMD_TYPE_CMD_DELAY) {
			ret = bl_extern_i2c_write(i2c_dev->client,
						  &table[i + 2],
						  (cmd_size - 1));
			if (table[i + cmd_size + 1] > 0)
				mdelay(table[i + cmd_size + 1]);
		} else {
			BLEXERR("[%d]: %s: %s(%d): type 0x%02x invalid\n",
				bext->config, __func__, bext->config.name,
				bext->config.index, type);
		}
power_cmd_dynamic_next:
		i += (cmd_size + 2);
		step++;
	}

	return ret;
}

static int i2c_lp8556_power_ctrl(struct bl_extern_driver_s *bext, int flag)
{
	unsigned char *table;
	unsigned char cmd_size;
	int ret = 0;

	cmd_size = bext->config.cmd_size;
	if (flag)
		table =  bext->config.init_on;
	else
		table =  bext->config.init_off;
	if (cmd_size != LCD_EXT_CMD_SIZE_DYNAMIC) {
		BLEXERR("[%d]: %s: cmd_size %d is invalid\n",
			bext->index, __func__, cmd_size);
		return -1;
	}
	if (!bext->i2c_dev) {
		BLEXERR("[%d]: %s: invalid i2c device\n", bext->index, __func__);
		return -1;
	}

	ret = bl_extern_power_cmd_dynamic_size(bext, table, flag);

	BLEX("[%d]: %s: %s(%d): %d\n",
	     bext->index, __func__,  bext->config.name,  bext->config.index, flag);
	return ret;
}

static int i2c_lp8556_power_on(struct bl_extern_driver_s *bext)
{
	int ret;

	BLEX("[%d]: %s\n", bext->index, __func__);

	ret = i2c_lp8556_power_ctrl(bext, 1);

	return ret;
}

static int i2c_lp8556_power_off(struct bl_extern_driver_s *bext)
{
	int ret;

	BLEX("[%d]: %s\n", bext->index, __func__);

	ret = i2c_lp8556_power_ctrl(bext, 0);
	return ret;
}

static int i2c_lp8556_set_level(struct bl_extern_driver_s *bext, unsigned int level)
{
	unsigned char tdata[5];
	int ret = 0;

	if (!i2c_dev) {
		BLEXERR("invalid i2c device\n");
		return -1;
	}

	if (bl_extern->config.dim_max > 255) {
		tdata[0] = 0x10;
		tdata[1] = level & 0xff;
		tdata[2] = 0x11;
		tdata[3] = (level >> 8) & 0xf;
		ret = bl_extern_i2c_write(i2c_dev->client, tdata, 4);
	} else {
		tdata[0] = 0x0;
		tdata[1] = level & 0xff;
		ret = bl_extern_i2c_write(i2c_dev->client, tdata, 2);
	}
	return ret;
}

static int i2c_lp8556_update(struct bl_extern_driver_s *bext)
{
	ext_config = &bl_extern->config;
	i2c_dev = aml_bl_extern_i2c_get_dev();

	bl_extern->device_power_on = i2c_lp8556_power_on;
	bl_extern->device_power_off = i2c_lp8556_power_off;
	bl_extern->device_bri_update = i2c_lp8556_set_level;

	bl_extern->config.cmd_size = BL_EXTERN_CMD_SIZE;
	if (!bl_extern->config.init_loaded) {
		bl_extern->config.init_on = init_on_table;
		bl_extern->config.init_on_cnt = sizeof(init_on_table);
		bl_extern->config.init_off = init_off_table;
		bl_extern->config.init_off_cnt = sizeof(init_off_table);
	}

	return 0;
}

int i2c_lp8556_probe(struct bl_extern_driver_s *bext)
{
	int ret = 0;

	if (!bext)
		return;

	ret = i2c_lp8556_update(bext);

	BLEX("%s: %d\n", __func__, ret);
	return ret;
}

int i2c_lp8556_remove(struct bl_extern_driver_s *bext)
{
	return 0;
}

