#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/amlogic/media/vout/lcd/aml_lcd.h>
#include <linux/amlogic/media/vout/lcd/aml_bl_extern.h>
#include <linux/amlogic/gki_module.h>
#include "bl_extern.h"

static struct bl_extern_i2c_dev_s *i2c_device[BL_EXT_I2C_DEV_MAX];
static unsigned int i2c_dev_cnt;

struct bl_extern_i2c_dev_s *bl_extern_i2c_get_dev(unsigned char addr)
{
	struct bl_extern_i2c_dev_s *i2c_dev = NULL;
	int i;

	/*pr_info("%s: addr=0x%02x\n", __func__, addr);*/
	for (i = 0; i < i2c_dev_cnt; i++) {
		if (!i2c_device[i])
			break;
		if (i2c_device[i]->client->addr == addr) {
			i2c_dev = i2c_device[i];
			break;
		}
	}
	return i2c_dev;
}

int bl_extern_i2c_write(struct i2c_client *i2client,
			unsigned char *buff, unsigned int len)
{
	struct i2c_msg msg;
	int ret = 0;

	if (!i2client) {
		BLEXERR("i2client is null\n");
		return -1;
	}

	msg.addr = i2client->addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = buff;

	ret = i2c_transfer(i2client->adapter, &msg, 1);
	if (ret < 0) {
		BLEXERR("i2c write failed [addr 0x%02x]\n", i2client->addr);
		return -1;
	}

	return 0;
}

int bl_extern_i2c_read(struct i2c_client *i2client,
		       unsigned char *buff, unsigned int len)
{
	struct i2c_msg msgs[2];
	int ret = 0;

	if (!i2client) {
		BLEXERR("i2client is null\n");
		return -1;
	}

	msgs[0].addr = i2client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = buff;
	msgs[1].addr = i2client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = buff;

	ret = i2c_transfer(i2client->adapter, msgs, 2);
	if (ret < 0) {
		BLEXERR("%s: i2c transfer failed [addr 0x%02x]\n",
			__func__, i2client->addr);
		return -1;
	}

	return 0;
}

static int bl_extern_i2c_config_from_dts(struct device *dev,
					 struct bl_extern_i2c_dev_s *i2c_dev)
{
	int ret;
	struct device_node *np = dev->of_node;
	const char *str;

	ret = of_property_read_string(np, "dev_name", &str);
	if (ret) {
		BLEXERR("failed to get i2c dev_name\n");
		strcpy(i2c_dev->name, "none");
	} else {
		strncpy(i2c_dev->name, str, 29);
	}

	return 0;
}

static int aml_bl_extern_i2c_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	if (i2c_dev_cnt >= BL_EXT_I2C_DEV_MAX) {
		BLEXERR("i2c_dev_cnt reach max\n");
		return -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		BLEXERR("i2c_dev check functionality failed.");
		return -ENODEV;
	}

	i2c_device[i2c_dev_cnt] = kzalloc(sizeof(*i2c_device[i2c_dev_cnt]), GFP_KERNEL);
	if (!i2c_device[i2c_dev_cnt]) {
		BLEXERR("i2c_dev %d driver malloc error\n", i2c_dev_cnt);
		return -ENOMEM;
	}

	i2c_device[i2c_dev_cnt]->client = client;
	bl_extern_i2c_config_from_dts(&client->dev, i2c_device[i2c_dev_cnt]);
	BLEX("i2c_dev probe: %s address 0x%02x OK",
	      i2c_device[i2c_dev_cnt]->name,
	      i2c_device[i2c_dev_cnt]->client->addr);

	i2c_dev_cnt++;

	return 0;
}

static int aml_bl_extern_i2c_remove(struct i2c_client *client)
{
	int i;

	if (i2c_dev_cnt == 0)
		return 0;

	for (i = 0; i < i2c_dev_cnt; i++) {
		kfree(i2c_device[i]);
		i2c_device[i] = NULL;
	}
	i2c_dev_cnt = 0;

	return 0;
}

static const struct i2c_device_id aml_bl_extern_i2c_id[] = {
	{"lcd_ext_i2c", 0},
	{}
};

#ifdef CONFIG_OF
static const struct of_device_id aml_bl_extern_i2c_dt_match[] = {
	{
		.compatible = "bl_ext, i2c",
	},
	{},
};
#endif

static struct i2c_driver aml_bl_extern_i2c_driver = {
	.probe  = aml_bl_extern_i2c_probe,
	.remove = aml_bl_extern_i2c_remove,
	.id_table   = aml_bl_extern_i2c_id,
	.driver = {
		.name  = "bl_ext_i2c",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = aml_bl_extern_i2c_dt_match,
#endif
	},
};

int __init aml_bl_extern_i2c_init(void)
{
	int ret;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLEX("%s\n", __func__);

	ret = i2c_add_driver(&aml_bl_extern_i2c_driver);
	if (ret) {
		BLEXERR("driver register failed\n");
		return -ENODEV;
	}
	return ret;
}

void __exit aml_bl_extern_i2c_exit(void)
{
	i2c_del_driver(&aml_bl_extern_i2c_driver);
}

//MODULE_AUTHOR("AMLOGIC");
//MODULE_DESCRIPTION("bl extern driver");
//MODULE_LICENSE("GPL");

