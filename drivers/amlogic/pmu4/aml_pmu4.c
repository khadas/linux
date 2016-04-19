#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/of_irq.h>

#undef pr_fmt
#define pr_fmt(fmt) "PMU4: " fmt

#define AML1220_ADDR    0x35
#define AML1220_ANALOG_ADDR  0x20

static int use_24m_clock;
struct i2c_client *g_aml_pmu4_client = NULL;

#define CHECK_DRIVER()      \
{		\
	if (!g_aml_pmu4_client) {        \
		pr_err("driver is not ready right now, wait...\n");   \
		dump_stack();       \
		return -ENODEV;     \
	} \
}

unsigned int pmu4_analog_reg[15] = {
	0x00, 0x00, 0x00, 0x00, 0x00,	/* Reg   0x20 - 0x24 */
	0x00, 0x00, 0x00, 0x00, 0x51,	/* Reg  0x25 - 0x29 */
	0x82, 0x00, 0x42, 0x41, 0x02	/* Reg   0x2a - 0x2e */
};

#define PMU4_ANALOG_REG_LEN ARRAY_SIZE(pmu4_analog_reg)

int aml_pmu4_write(int32_t add, uint8_t val)
{
	int ret;
	uint8_t buf[3] = { };
	struct i2c_client *pdev;
	struct i2c_msg msg[] = {
		{
		 .addr = AML1220_ADDR,
		 .flags = 0,
		 .len = sizeof(buf),
		 .buf = buf,
		 }
	};

	CHECK_DRIVER();
	pdev = g_aml_pmu4_client;

	buf[0] = add & 0xff;
	buf[1] = (add >> 8) & 0x0f;
	buf[2] = val & 0xff;
	ret = i2c_transfer(pdev->adapter, msg, 1);
	if (ret < 0) {
		pr_err("%s: i2c transfer failed, ret:%d\n", __func__, ret);
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(aml_pmu4_write);

int aml_pmu4_write16(int32_t add, uint16_t val)
{
	int ret;
	uint8_t buf[4] = { };
	struct i2c_client *pdev;
	struct i2c_msg msg[] = {
		{
		 .addr = AML1220_ADDR,
		 .flags = 0,
		 .len = sizeof(buf),
		 .buf = buf,
		 }
	};

	CHECK_DRIVER();
	pdev = g_aml_pmu4_client;

	buf[0] = add & 0xff;
	buf[1] = (add >> 8) & 0x0f;
	buf[2] = val & 0xff;
	buf[3] = (val >> 8) & 0xff;
	ret = i2c_transfer(pdev->adapter, msg, 1);
	if (ret < 0) {
		pr_err("%s: i2c transfer failed, ret:%d\n", __func__, ret);
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(aml_pmu4_write16);

int aml_pmu4_read(int add, uint8_t *val)
{
	int ret;
	uint8_t buf[2] = { };
	struct i2c_client *pdev;
	struct i2c_msg msg[] = {
		{
		 .addr = AML1220_ADDR,
		 .flags = 0,
		 .len = sizeof(buf),
		 .buf = buf,
		 }
		,
		{
		 .addr = AML1220_ADDR,
		 .flags = I2C_M_RD,
		 .len = 1,
		 .buf = val,
		 }
	};

	CHECK_DRIVER();
	pdev = g_aml_pmu4_client;

	buf[0] = add & 0xff;
	buf[1] = (add >> 8) & 0x0f;
	ret = i2c_transfer(pdev->adapter, msg, 2);
	if (ret < 0) {
		pr_err("%s: i2c transfer failed, ret:%d\n", __func__, ret);
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(aml_pmu4_read);

int aml_pmu4_read16(int add, uint16_t *val)
{
	int ret;
	uint8_t buf[2] = { };
	struct i2c_client *pdev;
	struct i2c_msg msg[] = {
		{
		 .addr = AML1220_ADDR,
		 .flags = 0,
		 .len = sizeof(buf),
		 .buf = buf,
		 }
		,
		{
		 .addr = AML1220_ADDR,
		 .flags = I2C_M_RD,
		 .len = 2,
		 .buf = (uint8_t *) val,
		 }
	};

	CHECK_DRIVER();
	pdev = g_aml_pmu4_client;

	buf[0] = add & 0xff;
	buf[1] = (add >> 8) & 0x0f;
	ret = i2c_transfer(pdev->adapter, msg, 2);
	if (ret < 0) {
		pr_err("%s: i2c transfer failed, ret:%d\n", __func__, ret);
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(aml_pmu4_read16);

int aml_pmu4_version(uint8_t *version)
{
	if (!g_aml_pmu4_client)
		return -ENODEV;

	return aml_pmu4_read(0x0, version);
}
EXPORT_SYMBOL_GPL(aml_pmu4_version);

#ifdef CONFIG_DEBUG_FS

static struct dentry *debugfs_root;
static struct dentry *debugfs_regs;

static ssize_t aml_pmu4_reg_read_file(struct file *file,
				      char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	ssize_t ret;
	char *buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = sprintf(buf, "Usage:\n"
		      "	echo reg val >pmu4_reg\t(set the register)\n"
		      "	echo reg >pmu4_reg\t(show the register)\n"
		      "	echo a >pmu4_reg\t(show all register)\n");

	if (ret >= 0)
		ret = simple_read_from_buffer(user_buf, count, ppos, buf, ret);
	kfree(buf);

	return ret;
}

static int read_regs(int reg)
{
	uint8_t val = 0;
	aml_pmu4_read(reg, &val);
	pr_warn("\tReg %x = %x\n", reg, val);
	return val;
}

static void write_regs(int reg, int val)
{
	aml_pmu4_write(reg, val);
	pr_warn("Write reg:%x = %x\n", reg, val);
}

static void dump_all_register(void)
{
	int i = 0;
	uint8_t val = 0;

	pr_warn(" dump aml_pmu4 pmu4 all register:\n");

	for (i = 0; i < 0xb0; i++) {
		aml_pmu4_read(i, &val);
		msleep(20);
		pr_warn("0x%02x: 0x%02x\n", i, val);
	}

	return;
}

static ssize_t aml_pmu4_reg_write_file(struct file *file,
				       const char __user *user_buf,
				       size_t count, loff_t *ppos)
{
	char buf[32];
	int buf_size = 0;
	char *start = buf;
	unsigned long reg, value;
	char all_reg;

	buf_size = min(count, (sizeof(buf) - 1));

	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	while (*start == ' ')
		start++;

	all_reg = *start;
	start++;
	if (all_reg == 'a') {

		dump_all_register();
		return buf_size;
	} else {
		start--;
	}

	while (*start == ' ')
		start++;

	if (kstrtoul(start, 16, &reg))
		return -EINVAL;

	while (*start == ' ')
		start++;

	if (kstrtoul(start, 16, &value)) {
		read_regs(reg);
		return -EINVAL;
	}

	write_regs(reg, value);

	return buf_size;
}

static const struct file_operations aml_pmu4_reg_fops = {
	.open = simple_open,
	.read = aml_pmu4_reg_read_file,
	.write = aml_pmu4_reg_write_file,
};
#endif

static int aml_pmu4_reg_init(unsigned int reg_base, unsigned int *val,
			      unsigned int reg_len)
{
	int ret = 0;
	unsigned int i = 0;

	for (i = 0; i < reg_len; i++) {
		ret = aml_pmu4_write(reg_base + i, val[i]);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief PMU4_PHY_CONFIG
 * @param void
 * @return void
 */
/* --------------------------------------------------------------------------*/
void pmu4_phy_conifg(void)
{
	int i = 0;
	uint8_t value = 0;
#if 0
	int data;
	uint8_t data_lo;
	uint8_t data_hi;
#endif
	/* eth ldo */
	aml_pmu4_write(0x04, 0x01);
	aml_pmu4_write(0x05, 0x01);

	mdelay(10);

	/* pinmux */
	aml_pmu4_write(0x2c, 0x51);
	aml_pmu4_write(0x2d, 0x41);
	aml_pmu4_write(0x20, 0x0);
	aml_pmu4_write(0x21, 0x3);
#ifdef EXT_CLK
	aml_pmu4_write(0x14, 0x01);
#else
	aml_pmu4_write(0x14, 0x00);
#endif

	aml_pmu4_write(0x15, 0x3f);

	/* pll */
	if (use_24m_clock)
		aml_pmu4_write(0x78, 0x00);
	else
		aml_pmu4_write(0x78, 0x06);
	aml_pmu4_write(0x79, 0x05);
	aml_pmu4_write(0x7a, 0xa1);
	aml_pmu4_write(0x7b, 0xac);
	aml_pmu4_write(0x7c, 0x5b);
	aml_pmu4_write(0x7d, 0xa0);
	aml_pmu4_write(0x7e, 0x20);
	aml_pmu4_write(0x7f, 0x49);
	aml_pmu4_write(0x80, 0xd6);
	aml_pmu4_write(0x81, 0x0b);
	aml_pmu4_write(0x82, 0xd1);
	aml_pmu4_write(0x83, 0x00);
	if (use_24m_clock) {
		aml_pmu4_write(0x84, 0x55);
		aml_pmu4_write(0x85, 0x0d);
	} else {
		aml_pmu4_write(0x84, 0x00);
		aml_pmu4_write(0x85, 0x00);
	}
	/* reset PMU4 PLL */
	aml_pmu4_write(0x8e, 0x1);
	aml_pmu4_write(0x8e, 0x0);
	do {
		aml_pmu4_read(0x9c, &value);
		mdelay(10);
	} while (((value & 0x01) == 0) && (i++ < 10));
	if (!(value&0x01))
		pr_err("WARING: PMU4 PLL not lock!");

	/*cfg4- --- cfg 45 */
	aml_pmu4_write(0x88, 0x0);
	aml_pmu4_write(0x89, 0x0);
	aml_pmu4_write(0x8A, 0x22);
	aml_pmu4_write(0x8B, 0x01);
	aml_pmu4_write(0x8C, 0xd0);

	aml_pmu4_write(0x8D, 0x01);
	aml_pmu4_write(0x8E, 0x00);

	aml_pmu4_write(0x93, 0x81);

	/* pmu4 phyid = 20142014 */
	aml_pmu4_write(0x94, 0x14);
	aml_pmu4_write(0x95, 0x20);

	aml_pmu4_write(0x96, 0x14);
	aml_pmu4_write(0x97, 0x20);

	/*phyadd & mode
	   eth_cfg_56   0x98    7:3     R/W     0       co_st_phyadd[4:0]
	   2:0  R/W     0       co_st_mode[2:0]
	   eth_phy_co_st_mode
	   //           000 - 10Base-T Half Duplex, auto neg disabled
	   //           001 - 10Base-T Full Duplex, auto neg disabled
	   //           010 - 100Base-TX Half Duplex, auto neg disabled
	   //           011 - 100Base-TX Full Duplex, auto neg disabled
	   //           100 - 100Base-TX Half Duplex, auto neg enabled
	   //           101 - Repeater mode, auto neg enabled
	   //           110 - Power Down Mode
	   //           111 - All capable, auto neg enabled, automdix enabled
	 */
#ifdef EXT_CLK
	aml_pmu4_write(0x98, 0x73);
#else
	aml_pmu4_write(0x98, 0x47);
#endif

	/*
	   0x99 7:6     R/W     0       co_st_miimode[1:0]
	   5    R/W     0       co_smii_source_sync
	   4    R/W     0       co_st_pllbp
	   3    R/W     0       co_st_adcbp
	   2    R/W     0       co_st_fxmode
	   1    R/W     0       co_en_high
	   0    R/W     0       co_automdix_en
	   0x9A 7                       reserved
	   6    R/W     0       co_pwruprst_byp
	   5    R/W     0       co_clk_ext
	   4    R/W     0       co_st_scan
	   3    R/W     0       co_rxclk_inv
	   2    R/W     0       co_phy_enb
	   1    R/W     0       co_clkfreq
	   0    R/W     0       eth_clk_enable
	 */
	aml_pmu4_write(0x99, 0x61);
	/*
	   eth_cfg_58   0x9a
	 */
	aml_pmu4_write(0x9a, 0x07);
	/*
	   eth_cfg_59   0x9b
	 */
	aml_pmu4_write(0x04, 0x01);
	aml_pmu4_write(0x05, 0x01);
	aml_pmu4_write(0x9b, 0x00);
	aml_pmu4_write(0x9b, 0x80);
	aml_pmu4_write(0x9b, 0x00);
#if 0
	pr_info("phy init though i2c done\n");
	for (i = 0; i < 0xb0; i++) {
		aml_pmu4_read(i, &value);
		pr_info("  i2c[%x]=0x%x\n", i, value);
	}
	pr_info("phy reg dump though i2c:\n");
	for (i = 0; i < 0x20; i++) {
		aml_pmu4_write(0xa6, i);
		aml_pmu4_read(0xa7, &data_lo);
		aml_pmu4_read(0xa8, &data_hi);
		data = (data_hi << 8) | data_lo;
		pr_info("  phy[%x]=0x%x\n", i, data);
	}
#endif
}

static int aml_pmu4_power_init(void)
{
	int ret;
	uint8_t ver = 0;

	/* pmu4 analog register init */
	ret = aml_pmu4_reg_init(AML1220_ANALOG_ADDR, &pmu4_analog_reg[0],
			  PMU4_ANALOG_REG_LEN);
	if (ret < 0)
		return ret;

	pmu4_phy_conifg();

	ret = aml_pmu4_version(&ver);
	if (!ret)
		pr_info("pmu4 version: 0x%x", ver);

	return 0;

}

static int aml_pmu4_i2c_probe(struct i2c_client *client,
			      const struct i2c_device_id *id)
{
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c check error, dev_id=%s--\n", id->name);
		return -ENODEV;
	}
	i2c_set_clientdata(client, NULL);
	g_aml_pmu4_client = client;

	/* aml_pmu4 power init */
	aml_pmu4_power_init();

#ifdef CONFIG_DEBUG_FS
	/* add debug interface */
	debugfs_regs = debugfs_create_file("pmu4_reg", 0644,
					   debugfs_root,
					   NULL, &aml_pmu4_reg_fops);
#endif

	return 0;
}

static int aml_pmu4_i2c_remove(struct i2c_client *client)
{
	pr_info("enter %s\n", __func__);
	kfree(i2c_get_clientdata(client));

	return 0;
}
static int aml_pmu4_i2c_resume(struct i2c_client *client)
{
	pr_info("aml_pmu4_i2c_resume");
	/*reinit pmu4 in the resume to make phy work*/
	aml_pmu4_power_init();
	return 0;
}


#define AML_I2C_BUS_AO		0
#define AML_I2C_BUS_A		1
#define AML_I2C_BUS_B		2
#define AML_I2C_BUS_C		3

static int aml_pmu4_probe(struct platform_device *pdev)
{
	struct device_node *pmu_node = pdev->dev.of_node;
	struct device_node *child;
	struct i2c_board_info board_info;
	struct i2c_adapter *adapter;
	struct i2c_client *client;
	int err;
	int addr;
	int bus_type = -1;
	const char *str;

	for_each_child_of_node(pmu_node, child) {
		/* register exist pmu */
		pr_info("%s, child name:%s\n", __func__, child->name);
		err = of_property_read_string(child, "i2c_bus", &str);
		if (err) {
			pr_info("get 'i2c_bus' failed, ret:%d\n", err);
			continue;
		}
		if (!strncmp(str, "i2c_bus_ao", 10))
			bus_type = AML_I2C_BUS_AO;
		else if (!strncmp(str, "i2c_bus_c", 9))
			bus_type = AML_I2C_BUS_C;
		else if (!strncmp(str, "i2c_bus_b", 9))
			bus_type = AML_I2C_BUS_B;
		else if (!strncmp(str, "i2c_bus_a", 9))
			bus_type = AML_I2C_BUS_A;
		else
			bus_type = AML_I2C_BUS_AO;
		err = of_property_read_string(child, "status", &str);
		if (err) {
			pr_info("get 'status' failed, ret:%d\n", err);
			continue;
		}
		if (strcmp(str, "okay") && strcmp(str, "ok")) {
			/* status is not OK, do not probe it */
			pr_info("device %s status is %s, stop probe it\n",
				child->name, str);
			continue;
		}
		err = of_property_read_u32(child, "reg", &addr);
		if (err) {
			pr_info("get 'reg' failed, ret:%d\n", err);
			continue;
		}
		err = of_property_read_u32(child, "use_24m_clock",
				&use_24m_clock);
		if (err) {
			pr_info("get 'use_24m_clock' failed, ret:%d\n", err);
			continue;
		}
		memset(&board_info, 0, sizeof(board_info));
		adapter = i2c_get_adapter(bus_type);
		if (!adapter)
			pr_info("wrong i2c adapter:%d\n", bus_type);
		err = of_property_read_string(child, "compatible", &str);
		if (err) {
			pr_info("get 'compatible' failed, ret:%d\n", err);
			continue;
		}
		strncpy(board_info.type, str, I2C_NAME_SIZE);
		board_info.addr = addr;
		board_info.of_node = child;	/* for device driver */
		board_info.irq = irq_of_parse_and_map(child, 0);
		client = i2c_new_device(adapter, &board_info);
		if (!client) {
			pr_info("%s, allocate i2c_client failed\n", __func__);
			continue;
		}
		pr_info("%s: adapter:%d, addr:0x%x, node name:%s, type:%s\n",
			"Allocate new i2c device",
			bus_type, addr, child->name, str);
	}
	return 0;
}

static int aml_pmu4_remove(struct platform_device *pdev)
{
	/* nothing to do */
	return 0;
}

static const struct of_device_id aml_pmu_dt_match[] = {
	{
	 .compatible = "amlogic, pmu4_prober",
	 },
	{}
};

static struct platform_driver aml_pmu_prober = {
	.probe = aml_pmu4_probe,
	.remove = aml_pmu4_remove,
	.driver = {
		   .name = "pmu4_prober",
		   .owner = THIS_MODULE,
		   .of_match_table = aml_pmu_dt_match,
		   },
};

#ifdef CONFIG_OF
static const struct of_device_id aml_pmu4_match_id = {
	.compatible = "amlogic, pmu4",
};
#endif

static const struct i2c_device_id aml_pmu4_id_table[] = {
	{"aml_pmu4", 0},
	{}
};

static struct i2c_driver aml_pmu4_i2c_driver = {
	.driver = {
		   .name = "pmu4",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = &aml_pmu4_match_id,
#endif
		   },
	.probe = aml_pmu4_i2c_probe,
	.remove = aml_pmu4_i2c_remove,
	.resume = aml_pmu4_i2c_resume,
	.id_table = aml_pmu4_id_table,
};

static int __init aml_pmu4_modinit(void)
{
	int ret;

	ret = platform_driver_register(&aml_pmu_prober);
	return i2c_add_driver(&aml_pmu4_i2c_driver);
}

arch_initcall(aml_pmu4_modinit);

static void __exit aml_pmu4_modexit(void)
{
	i2c_del_driver(&aml_pmu4_i2c_driver);
	platform_driver_unregister(&aml_pmu_prober);
}

module_exit(aml_pmu4_modexit);

MODULE_DESCRIPTION("Amlogic PMU4 device driver");
MODULE_AUTHOR("Chengshun Wang <chengshun.wang@amlogic.com>");
MODULE_LICENSE("GPL");
