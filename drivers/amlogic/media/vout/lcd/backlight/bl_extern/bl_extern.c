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
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/gki_module.h>
#include "bl_extern.h"

#include <linux/amlogic/gki_module.h>

static struct bl_extern_driver_s *bl_ext_driver[LCD_MAX_DRV];
static int bl_ext_index[LCD_MAX_DRV] = {0xff, 0xff, 0xff};
/* for driver global resource init:
 *  0: none
 *  n: initialized cnt
 */
static unsigned char ext_global_init_flag;

struct bl_extern_driver_s *bl_extern_get_driver(int index)
{
	if (index >= LCD_MAX_DRV) {
		BLEXERR("%s: invalid drv_index: %d\n", __func__, index);
		return NULL;
	}
	return bl_ext_driver[index];
}

int bl_extern_dev_index_add(int drv_index, int dev_index)
{
	if (drv_index >= LCD_MAX_DRV) {
		BLEXERR("%s: invalid drv_index: %d\n", __func__, drv_index);
		return -1;
	}

	bl_ext_index[drv_index] = dev_index;
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLEX("[%d]: %s: dev_index: %d\n", drv_index, __func__, dev_index);
	return 0;
}

static int bl_extern_set_level(struct bl_extern_driver_s *bext, unsigned int level)
{
	unsigned int level_max, level_min;
	unsigned int dim_max, dim_min;
	int ret = 0;

	bext->brightness = level;
	if (bext->status == 0)
		return 0;

	level_max = bext->level_max;
	level_min = bext->level_min;
	dim_max = bext->config.dim_max;
	dim_min = bext->config.dim_min;
	level = dim_min - ((level - level_min) * (dim_min - dim_max)) / (level_max - level_min);

	if (bext->device_bri_update)
		ret = bext->device_bri_update(bext, level);

	return ret;
}

static int bl_extern_power_on(struct bl_extern_driver_s *bext)
{
	int ret = 0;

	BLEX("[%d]: %s\n", bext->index, __func__);

	if (bext->device_power_on)
		ret = bext->device_power_on(bext);
	bext->status = 1;

	/* restore bl level */
	bl_extern_set_level(bext, bext->brightness);

	return ret;
}

static int bl_extern_power_off(struct bl_extern_driver_s *bext)
{
	int ret = 0;

	BLEX("[%d]: %s\n", bext->index, __func__);

	bext->status = 0;
	if (bext->device_power_off)
		ret = bext->device_power_off(bext);

	return ret;
}

#define EXT_LEN_MAX   500
static void bl_extern_init_table_dynamic_print(struct bl_extern_config_s *econf, int flag)
{
	int i, j, k, max_len;
	unsigned char cmd_size;
	char *str;
	unsigned char *table;

	str = kcalloc(EXT_LEN_MAX, sizeof(char), GFP_KERNEL);
	if (!str) {
		BLEXERR("%s: str malloc error\n", __func__);
		return;
	}
	if (flag) {
		pr_info("power on:\n");
		table = econf->init_on;
		max_len = econf->init_off_cnt;
	} else {
		pr_info("power off:\n");
		table = econf->init_off;
		max_len = econf->init_off_cnt;
	}
	if (!table) {
		BLEXERR("init_table %d is NULL\n", flag);
		kfree(str);
		return;
	}

	i = 0;
	while ((i + 1) < max_len) {
		if (table[i] == LCD_EXT_CMD_TYPE_END) {
			pr_info("  0x%02x,%d,\n", table[i], table[i + 1]);
			break;
		}
		cmd_size = table[i + 1];

		k = snprintf(str, EXT_LEN_MAX, "  0x%02x,%d,",
			     table[i], cmd_size);
		if (cmd_size == 0)
			goto init_table_dynamic_print_next;
		if (i + 2 + cmd_size > max_len) {
			pr_info("cmd_size out of support\n");
			break;
		}

		if (table[i] == LCD_EXT_CMD_TYPE_DELAY) {
			for (j = 0; j < cmd_size; j++) {
				k += snprintf(str + k, EXT_LEN_MAX,
					      "%d,", table[i + 2 + j]);
			}
		} else if (table[i] == LCD_EXT_CMD_TYPE_CMD) {
			for (j = 0; j < cmd_size; j++) {
				k += snprintf(str + k, EXT_LEN_MAX,
					      "0x%02x,", table[i + 2 + j]);
			}
		} else if (table[i] == LCD_EXT_CMD_TYPE_CMD_DELAY) {
			for (j = 0; j < (cmd_size - 1); j++) {
				k += snprintf(str + k, EXT_LEN_MAX,
					      "0x%02x,", table[i + 2 + j]);
			}
			snprintf(str + k, EXT_LEN_MAX,
				 "%d,", table[i + cmd_size + 1]);
		} else {
			for (j = 0; j < cmd_size; j++) {
				k += snprintf(str + k, EXT_LEN_MAX,
					      "0x%02x,", table[i + 2 + j]);
			}
		}
init_table_dynamic_print_next:
		pr_info("%s\n", str);
		i += (cmd_size + 2);
	}

	kfree(str);
}

static void bl_extern_config_print(struct bl_extern_driver_s *bext)
{
	BLEX("[%d]: %s:\n", bext->index, __func__);
	pr_info("index:          %d\n"
		"name:          %s\n",
		bext->config.index,
		bext->config.name);
	switch (bext->config.type) {
	case BL_EXTERN_I2C:
		pr_info("type:          i2c(%d)\n"
			"i2c_addr:      0x%02x\n"
			"i2c_bus:       %d\n"
			"dim_min:       %d\n"
			"dim_max:       %d\n",
			bext->config.type,
			bext->config.i2c_addr,
			bext->config.i2c_bus,
			bext->config.dim_min,
			bext->config.dim_max);
		if (bext->i2c_dev) {
			pr_info("i2c_dev_name:      %s\n"
				"i2c_client_name:   %s\n"
				"i2c_client_addr:   0x%02x\n",
				bext->i2c_dev->name,
				bext->i2c_dev->client->name,
				bext->i2c_dev->client->addr);
		} else {
			pr_info("invalid i2c device\n");
		}
		if (bext->config.cmd_size != LCD_EXT_CMD_SIZE_DYNAMIC)
			break;
		pr_info("table_loaded:       %d\n"
			"cmd_size:           %d\n"
			"init_on_cnt:        %d\n"
			"init_off_cnt:       %d\n",
			bext->config.init_loaded,
			bext->config.cmd_size,
			bext->config.init_on_cnt,
			bext->config.init_off_cnt);
		bl_extern_init_table_dynamic_print(&bext->config, 1);
		bl_extern_init_table_dynamic_print(&bext->config, 0);
		break;
	case BL_EXTERN_SPI:
		pr_info("type:          spi(%d)\n"
			"dim_min:       %d\n"
			"dim_max:       %d\n",
			bext->config.type,
			bext->config.dim_min,
			bext->config.dim_max);
		if (bext->config.cmd_size != LCD_EXT_CMD_SIZE_DYNAMIC)
			break;
		pr_info("table_loaded:       %d\n"
			"cmd_size:           %d\n"
			"init_on_cnt:        %d\n"
			"init_off_cnt:       %d\n",
			bext->config.init_loaded,
			bext->config.cmd_size,
			bext->config.init_on_cnt,
			bext->config.init_off_cnt);
		bl_extern_init_table_dynamic_print(&bext->config, 1);
		bl_extern_init_table_dynamic_print(&bext->config, 0);
		break;
	case BL_EXTERN_MIPI:
		pr_info("type:          mipi(%d)\n"
			"dim_min:       %d\n"
			"dim_max:       %d\n",
			bext->config.type,
			bext->config.dim_min,
			bext->config.dim_max);
		break;
	default:
		break;
	}
}

static int bl_extern_init_table_dynamic_load_dts(struct device_node *np,
						 struct bl_extern_config_s *econf,
						 int flag)
{
	unsigned char cmd_size, type;
	int i = 0, j, val, max_len, step = 0, ret = 0;
	unsigned char *table;
	char propname[20];

	if (flag) {
		max_len = BL_EXTERN_INIT_ON_MAX;
		sprintf(propname, "init_on");
	} else {
		max_len = BL_EXTERN_INIT_OFF_MAX;
		sprintf(propname, "init_off");
	}
	table = kcalloc(max_len, sizeof(unsigned char), GFP_KERNEL);
	if (!table)
		return -1;
	table[0] = LCD_EXT_CMD_TYPE_END;
	table[1] = 0;

	while ((i + 1) < max_len) {
		/* type */
		ret = of_property_read_u32_index(np, propname, i, &val);
		if (ret) {
			BLEXERR("%s: get %s type failed, step %d\n",
				econf->name, propname, step);
			table[i] = LCD_EXT_CMD_TYPE_END;
			table[i + 1] = 0;
			return -1;
		}
		table[i] = (unsigned char)val;
		type = table[i];
		/* cmd_size */
		ret = of_property_read_u32_index(np, propname, (i + 1), &val);
		if (ret) {
			BLEXERR("%s: get %s cmd_size failed, step %d\n",
				econf->name, propname, step);
			table[i] = LCD_EXT_CMD_TYPE_END;
			table[i + 1] = 0;
			return -1;
		}
		table[i + 1] = (unsigned char)val;
		cmd_size = table[i + 1];

		if (type == LCD_EXT_CMD_TYPE_END)
			break;
		if (cmd_size == 0)
			goto init_table_dynamic_dts_next;
		if ((i + 2 + cmd_size) > max_len) {
			BLEXERR("%s: %s cmd_size out of support, step %d\n",
				econf->name, propname, step);
			table[i] = LCD_EXT_CMD_TYPE_END;
			table[i + 1] = 0;
			return -1;
		}

		/* data */
		for (j = 0; j < cmd_size; j++) {
			ret = of_property_read_u32_index(np, propname, (i + 2 + j), &val);
			if (ret) {
				BLEXERR("%s: get %s data failed, step %d\n",
					econf->name, propname, step);
				table[i] = LCD_EXT_CMD_TYPE_END;
				table[i + 1] = 0;
				return -1;
			}
			table[i + 2 + j] = (unsigned char)val;
		}

init_table_dynamic_dts_next:
		i += (cmd_size + 2);
		step++;
	}
	if (flag) {
		econf->init_on_cnt = i + 2;
		econf->init_on = kcalloc(econf->init_on_cnt,
					   sizeof(unsigned char), GFP_KERNEL);
		if (!econf->init_on)
			goto init_table_dynamic_dts_err;
		memcpy(econf->init_on, table, econf->init_on_cnt);
	} else {
		econf->init_off_cnt = i + 2;
		econf->init_off = kcalloc(econf->init_off_cnt,
					    sizeof(unsigned char), GFP_KERNEL);
		if (!econf->init_off)
			goto init_table_dynamic_dts_err;
		memcpy(econf->init_off, table, econf->init_off_cnt);
	}

	kfree(table);
	return 0;

init_table_dynamic_dts_err:
	kfree(table);
	return -1;
}

static int bl_extern_config_from_dts(struct bl_extern_driver_s *bext)
{
	struct device_node *np;
	int index;
	char propname[20];
	struct device_node *child;
	const char *str;
	unsigned int temp[5], val;
	int ret = 0;

	index = bext->config.index;
	np = bext->dev->of_node;
	if (!np) {
		BLEXERR("[%d]: %s: of_node is null\n", bext->index, __func__);
		return -1;
	}

	ret = of_property_read_string(np, "i2c_bus", &str);
	if (ret)
		bext->config.i2c_bus = LCD_EXT_I2C_BUS_MAX;
	else
		bext->config.i2c_bus = aml_lcd_i2c_bus_get_str(str);

	/* get device config */
	sprintf(propname, "extern_%d", index);
	child = of_get_child_by_name(np, propname);
	if (!child) {
		BLEXERR("[%d]: failed to get %s\n", bext->index, propname);
		return -1;
	}
	BLEX("[%d]: load: %s\n", bext->index, propname);

	ret = of_property_read_string(child, "extern_name", &str);
	if (ret) {
		BLEXERR("[%d]: failed to get extern_name\n", bext->index);
		strcpy(bext->config.name, "none");
	} else {
		strncpy(bext->config.name, str, (BL_EXTERN_NAME_LEN_MAX - 1));
	}

	ret = of_property_read_u32(child, "type", &val);
	if (ret) {
		BLEXERR("[%d]: failed to get type\n", bext->index);
	} else {
		bext->config.type = val;
		BLEX("[%d]: type: %d\n", bext->index, bext->config.type);
	}
	if (bext->config.type >= BL_EXTERN_MAX) {
		BLEXERR("[%d]: invalid type %d\n", bext->index, bext->config.type);
		return -1;
	}

	ret = of_property_read_u32_array(child, "dim_max_min", &temp[0], 2);
	if (ret) {
		BLEXERR("[%d]: failed to get dim_max_min\n", bext->index);
	} else {
		bext->config.dim_max = temp[0];
		bext->config.dim_min = temp[1];
	}

	switch (bext->config.type) {
	case BL_EXTERN_I2C:
		if (bext->config.i2c_bus >= LCD_EXT_I2C_BUS_MAX) {
			BLEXERR("[%d]: failed to get i2c_bus\n", bext->index);
			break;
		}
		BLEX("[%d]: %s: i2c_bus=%d\n",
		     bext->index, bext->config.name, bext->config.i2c_bus);

		ret = of_property_read_u32(child, "i2c_address", &val);
		if (ret) {
			BLEXERR("[%d]: failed to get i2c_address\n", bext->index);
			return -1;
		}
		bext->config.i2c_addr = (unsigned char)val;
		BLEX("[%d]: %s: i2c_address=0x%02x\n",
		     bext->index, bext->config.name, bext->config.i2c_addr);
		bext->i2c_dev = bl_extern_i2c_get_dev(bext->config.i2c_addr);
		if (!bext->i2c_dev)
			return -1;

		ret = of_property_read_u32(child, "cmd_size", &val);
		if (ret) {
			BLEX("[%d]: %s: no cmd_size\n", bext->index, bext->config.name);
			bext->config.cmd_size = 0xff;
		} else {
			bext->config.cmd_size = (unsigned char)val;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
			BLEX("[%d]: %s: cmd_size = %d\n",
			     bext->index, bext->config.name, bext->config.cmd_size);
		}
		if (bext->config.cmd_size != LCD_EXT_CMD_SIZE_DYNAMIC)
			break;

		ret = bl_extern_init_table_dynamic_load_dts(child, &bext->config, 1);
		if (ret)
			break;
		ret = bl_extern_init_table_dynamic_load_dts(child, &bext->config, 0);
		if (ret == 0)
			bext->config.init_loaded = 1;
		break;
	case BL_EXTERN_SPI:
		ret = of_property_read_u32(child, "cmd_size", &val);
		if (ret) {
			BLEX("[%d]: %s: no cmd_size\n", bext->index, bext->config.name);
			bext->config.cmd_size = 0xff;
		} else {
			bext->config.cmd_size = (unsigned char)val;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
			BLEX("[%d]: %s: cmd_size = %d\n",
			     bext->index, bext->config.name, bext->config.cmd_size);
		}
		if (bext->config.cmd_size != LCD_EXT_CMD_SIZE_DYNAMIC)
			break;

		ret = bl_extern_init_table_dynamic_load_dts(child, &bext->config, 1);
		if (ret)
			break;
		ret = bl_extern_init_table_dynamic_load_dts(child, &bext->config, 0);
		if (ret == 0)
			bext->config.init_loaded = 1;
		break;
	case BL_EXTERN_MIPI:
		break;
	default:
		break;
	}

	return 0;
}

static int bl_extern_add_device(struct bl_extern_driver_s *bext)
{
	int ret = -1;

	if (strcmp(bext->config.name, "i2c_lp8556") == 0) {
#ifdef CONFIG_AMLOGIC_BL_EXTERN_I2C_LP8556
		ret = i2c_lp8556_probe(bext);
#endif
	} else if (strcmp(bext->config.name, "mipi_lt070me05") == 0) {
#ifdef CONFIG_AMLOGIC_BL_EXTERN_MIPI_LT070ME05
		ret = mipi_lt070me05_probe(bext);
#endif
	} else {
		BLEXERR("[%d]: invalid device name: %s\n",
			bext->index, bext->config.name);
	}

	if (ret) {
		BLEXERR("[%d]: add device driver %s(%d) failed\n",
			bext->index, bext->config.name, bext->config.index);
	} else {
		BLEX("[%d]: add device driver %s(%d) ok\n",
		     bext->index, bext->config.name, bext->config.index);
	}

	return ret;
}

static void bl_extern_remove_driver(struct bl_extern_driver_s *bext)
{
	if (strcmp(bext->config.name, "i2c_lp8556") == 0) {
#ifdef CONFIG_AMLOGIC_BL_EXTERN_I2C_LP8556
		ret = i2c_lp8556_remove(bext);
#endif
	} else if (strcmp(bext->config.name, "mipi_lt070me05") == 0) {
#ifdef CONFIG_AMLOGIC_BL_EXTERN_MIPI_LT070ME05
		ret = mipi_lt070me05_remove(bext);
#endif
	} else {
		BLEXERR("[%d]: %s: invalid device name: %s\n",
			bext->index, __func__, bext->config.name);
	}
}

int aml_bl_extern_probe(struct platform_device *pdev)
{
	struct aml_bl_drv_s *bdrv;
	struct bl_extern_driver_s *bext;
	int index, ret = 0;

	ret = of_property_read_u32(pdev->dev.of_node, "index", &index);
	if (ret) {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
			BLEX("%s: no index exist, default to 0\n", __func__);
		index = 0;
	}
	if (index >= LCD_MAX_DRV) {
		BLEXERR("%s: invalid index %d\n", __func__, index);
		return -1;
	}
	if (ext_global_init_flag & (1 << index)) {
		BLEXERR("%s: index %d driver already registered\n",
		       __func__, index);
		return -1;
	}
	ext_global_init_flag |= (1 << index);

	bext = kzalloc(sizeof(*bext), GFP_KERNEL);
	if (!bext)
		return -ENOMEM;

	bext->config.index = bl_ext_index[index];
	bext->config.type = BL_EXTERN_MAX;
	bext->config.i2c_addr = 0xff;
	bext->config.i2c_bus = LCD_EXT_I2C_BUS_MAX;
	bext->config.dim_min = 10;
	bext->config.dim_max = 255;
	bext->power_on = bl_extern_power_on;
	bext->power_off = bl_extern_power_off;
	bext->set_level = bl_extern_set_level;
	bext->config_print = bl_extern_config_print;
	bext->dev = &pdev->dev;

	bdrv = aml_bl_get_driver(index);
	bext->level_max = bdrv->bconf.level_max;
	bext->level_min = bdrv->bconf.level_min;

	/* set drvdata */
	platform_set_drvdata(pdev, bext);
	bl_ext_driver[index] = bext;

	ret = bl_extern_config_from_dts(bext);
	if (ret)
		goto aml_bl_extern_probe_err;
	ret = bl_extern_add_device(bext);
	if (ret)
		goto aml_bl_extern_probe_err;

	BLEX("[%d]: %s OK\n", index, __func__);

	return 0;

aml_bl_extern_probe_err:
	bl_ext_driver[index] = NULL;
	kfree(bext);
	BLEXERR("[%d]: %s: failed\n", index, __func__);
	return -1;
}

static int aml_bl_extern_remove(struct platform_device *pdev)
{
	struct bl_extern_driver_s *bext = platform_get_drvdata(pdev);
	int index;

	if (!bext)
		return -1;

	index = bext->index;
	bl_extern_remove_driver(bext);
	bl_ext_driver[index] = NULL;
	kfree(bext);
	ext_global_init_flag &= ~(1 << index);

	BLEX("[%d]: %s, init_state:0x%x\n", index, __func__, ext_global_init_flag);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id aml_bl_extern_dt_match[] = {
	{
		.compatible = "amlogic, bl_extern",
	},
	{},
};
#endif

static struct platform_driver aml_bl_extern_driver = {
	.probe  = aml_bl_extern_probe,
	.remove = aml_bl_extern_remove,
	.driver = {
		.name  = "bl_extern",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = aml_bl_extern_dt_match,
#endif
	},
};

int __init aml_bl_extern_init(void)
{
	int ret;

	ret = platform_driver_register(&aml_bl_extern_driver);
	if (ret) {
		BLEXERR("driver register failed\n");
		return -ENODEV;
	}
	return ret;
}

void __exit aml_bl_extern_exit(void)
{
	platform_driver_unregister(&aml_bl_extern_driver);
}

//MODULE_AUTHOR("AMLOGIC");
//MODULE_DESCRIPTION("bl extern driver");
//MODULE_LICENSE("GPL");

