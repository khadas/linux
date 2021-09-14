// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/gpio_keys.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/mfd/core.h>
#include <linux/mfd/rohm-bd2657.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/regmap.h>
#include <linux/string.h>
#include <linux/types.h>

static struct gpio_keys_button button = {
	.code = KEY_POWER,
	.gpio = -1,
	.type = EV_KEY,
};

static struct gpio_keys_platform_data bd2657_powerkey_data = {
	.buttons = &button,
	.nbuttons = 1,
	.name = "bd2657-pwrkey",
};

enum {
	MFD_CELL_REGULATOR,
	MFD_CELL_GPIO,
	MFD_CELL_GPIO_KEYS,
};

static struct mfd_cell bd2657_mfd_cells[] = {
	[MFD_CELL_REGULATOR]	= { .name = "bd2657-regulator", },
	[MFD_CELL_GPIO]		= { .name = "bd2657-gpo", },
	[MFD_CELL_GPIO_KEYS]	= {
		.name = "gpio-keys",
		.platform_data = &bd2657_powerkey_data,
		.pdata_size = sizeof(bd2657_powerkey_data),
	},
};

const static struct resource regulator_cpu_irqs[] = {
	DEFINE_RES_IRQ_NAMED(BD2657_INT_THERM, "bd2657-therm-warn"),
};

/*
 * SETTLE_MECH_BIT controls use of CPU0_STAT pin. If SETTLE_MECH_BIT = 1, the
 * CPU0_STAT pin is used to provide the "EPU IRQs".
 */
const static struct resource regulator_epu_irqs[] = {
	DEFINE_RES_IRQ_NAMED(BD2657_INT_BUCK0_SETTLE, "bd2657-buck0-settle"),
	DEFINE_RES_IRQ_NAMED(BD2657_INT_BUCK1_SETTLE, "bd2657-buck1-settle"),
	DEFINE_RES_IRQ_NAMED(BD2657_INT_BUCK2_SETTLE, "bd2657-buck2-settle"),
	DEFINE_RES_IRQ_NAMED(BD2657_INT_BUCK0_REJECT, "bd2657-buck0-reject"),
	DEFINE_RES_IRQ_NAMED(BD2657_INT_BUCK1_REJECT, "bd2657-buck1-reject"),
	DEFINE_RES_IRQ_NAMED(BD2657_INT_BUCK2_REJECT, "bd2657-buck2-reject"),
	DEFINE_RES_IRQ_NAMED(BD2657_INT_BUCK3_SETTLE, "bd2657-buck3-settle"),
	DEFINE_RES_IRQ_NAMED(BD2657_INT_BUCK3_REJECT, "bd2657-buck3-reject"),
};

static const struct regmap_range volatile_ranges[] = {
	{
		.range_min = BD2657_REG_INT_EPU,
		.range_max = BD2657_REG_PWRGOOD,
	}, {
		.range_min = BD2657_REG_S3_STATUS,
		.range_max = BD2657_REG_S3_STATUS,
	}, {
		.range_min = BD2657_REG_INT_MAIN,
		.range_max = BD2657_REG_INT_PBTN,
	}, {
		.range_min = BD2657_REG_PBSTATUS,
		.range_max = BD2657_REG_PBSTATUS,
	}, {
		.range_min = BD2657_REG_RESETSRC,
		.range_max = BD2657_REG_REGLOCK,
	}
};

static const struct regmap_access_table volatile_regs = {
	.yes_ranges = &volatile_ranges[0],
	.n_yes_ranges = ARRAY_SIZE(volatile_ranges),
};

static struct regmap_config bd2657_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.volatile_table = &volatile_regs,
	.max_register = BD2657_MAX_REGISTER,
	.cache_type = REGCACHE_RBTREE,
};

static struct regmap_irq bd2657_epu_irqs[] = {
	REGMAP_IRQ_REG(BD2657_INT_BUCK0_SETTLE, 0,
						BD2657_INT_BUCK0_SETTLE_MASK),
	REGMAP_IRQ_REG(BD2657_INT_BUCK1_SETTLE, 0,
						BD2657_INT_BUCK1_SETTLE_MASK),
	REGMAP_IRQ_REG(BD2657_INT_BUCK2_SETTLE, 0,
						BD2657_INT_BUCK2_SETTLE_MASK),
	REGMAP_IRQ_REG(BD2657_INT_BUCK0_REJECT, 0,
						BD2657_INT_BUCK0_REJECT_MASK),
	REGMAP_IRQ_REG(BD2657_INT_BUCK1_REJECT, 0,
						BD2657_INT_BUCK1_REJECT_MASK),
	REGMAP_IRQ_REG(BD2657_INT_BUCK2_REJECT, 0,
						BD2657_INT_BUCK2_REJECT_MASK),
	REGMAP_IRQ_REG(BD2657_INT_BUCK3_SETTLE, 0,
						BD2657_INT_BUCK3_SETTLE_MASK),
	REGMAP_IRQ_REG(BD2657_INT_BUCK3_REJECT, 0,
						BD2657_INT_BUCK3_REJECT_MASK),
};

/*
 * bd2657 does also have the main IRQ register for EPU but we don't use it
 * because we only have single second level IRQ block for EPU.
 */
static struct regmap_irq_chip bd2657_epu_irq_chip = {
	.name = "bd2657_epu_irq",
	.irqs = &bd2657_epu_irqs[0],
	.num_irqs = ARRAY_SIZE(bd2657_epu_irqs),
	.status_base = BD2657_REG_INT_EPU,
	.mask_base = BD2657_REG_INT_MASK_EPU,
	.ack_base = BD2657_REG_INT_EPU,
	.mask_invert = true,
	.init_ack_masked = true,
	.num_regs = 1,
	.irq_reg_stride = 1,
};

static struct regmap_irq bd2657_cpu_irqs[] = {
	REGMAP_IRQ_REG(BD2657_INT_THERM, 0, BD2657_INT_THERM_MASK),
	REGMAP_IRQ_REG(BD2657_INT_REQ, 1, BD2657_INT_REQ_MASK),
	REGMAP_IRQ_REG(BD2657_INT_PBTN_OFF, 2, BD2657_INT_PBTN_OFF_MASK),
};

/*
 * bd2657 CPU IRQ model is a bit complex. Presumably the logic has been
 * inherited from an IC with more IRQs divided to sub-blocks.
 *
 * bd2657 CPU IRQ has main (mask and status) IRQ registers and 3 sub-IRQ
 * registers. Fine. It's just that each of the sub-IRQs have only one IRQ.
 * So for whooping 3 different IRQ reasons we have 4 status and 4 mask
 * registers... Well, lets quit the whining here and just implement the
 * handling :)
 */
static struct regmap_irq_chip bd2657_cpu_irq_chip = {
	.name = "bd2657_cpu_irq",
	.main_status = BD2657_REG_INT_MAIN,
	.irqs = &bd2657_cpu_irqs[0],
	.num_irqs = ARRAY_SIZE(bd2657_cpu_irqs),
	.status_base = BD2657_REG_INT_THERM,
	.mask_base = BD2657_REG_INT_MASK_THERM,
	.ack_base = BD2657_REG_INT_THERM,
	.mask_invert = true,
	.init_ack_masked = true,
	.num_regs = 3,
	.num_main_regs = 1,
//	.sub_reg_offsets = &bd2657_sub_irq_offsets[0],
	.num_main_status_bits = 3,
	.irq_reg_stride = 2,
};

struct regmap *regmap_all;

static ssize_t read_reg_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	int ret = 0;
	int reg = 0;
	u32 val = 0;

	ret = kstrtoint(buf, 16, &reg);
	if (ret) {
		pr_err("%s: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_read(regmap_all, reg, &val);
	if (ret)
		pr_err("%s: %d\n", __func__, __LINE__);

	pr_info("[0x%x] = 0x%x\n", reg, val);

	return strlen(buf);
}

static ssize_t write_reg_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int ret = 0;
	int i = 0;
	int j = 0;
	int reg = 0, val = 0;
	char tone[20];

	while (buf[i] != '\0') {
		if (buf[i] == 's') {
			tone[j] = '\0';
			ret = kstrtoint(tone, 16, &reg);
			if (ret) {
				pr_err("%s: %d\n", __func__, __LINE__);
				return ret;
			}
		break;
		}
		tone[j] = buf[i];
		i++;
		j++;
	}
	ret = kstrtoint(&buf[i + 1], 16, &val);
	if (ret) {
		pr_err("%s: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(regmap_all, reg, val);
	if (ret)
		pr_err("%s: %d\n", __func__, __LINE__);

	pr_info("[0x%x] = 0x%x\n", reg, val);

	return strlen(buf);
}

static DEVICE_ATTR_WO(read_reg);
static DEVICE_ATTR_WO(write_reg);

static struct attribute *bd2657_attrs[] = {
		&dev_attr_read_reg.attr,
		&dev_attr_write_reg.attr,
		NULL,
};

static struct attribute_group bd2657_attr_group = {
		.attrs = bd2657_attrs,
};

static int bd2657_i2c_probe(struct i2c_client *i2c)
{
	struct regmap_irq_chip_data *irq_data;
	int ret, cpu_irq, epu_irq, num_res;
	struct irq_domain *domain = NULL;
	struct resource *res;
	struct regmap *regmap;
	struct device_node *np;
	struct mfd_cell cells[3];
	struct mfd_cell *cellp = &cells[0];

	/*
	 * Currently we always add GPIO and regulator cells, the need of GPIO
	 * driver could be decided based on DT information.
	 */
	int num_cells = 0;
	unsigned int gpio0_mode;

	if (!i2c->irq) {
		dev_err(&i2c->dev, "No IRQ configured\n");
		return -EINVAL;
	}

	np = i2c->dev.of_node;
	cpu_irq = of_irq_get_byname(np, "cpu-irq");
	if (cpu_irq < 0) {
		dev_err(&i2c->dev, "Failed to get 'cpu' interrupt for %pOF\n",
			np);
		return cpu_irq;
	}
	epu_irq = of_irq_get_byname(np, "epu-irq");
	if (epu_irq < 0)
		dev_dbg(&i2c->dev, "No EPU IRQ provided\n");

	regmap = devm_regmap_init_i2c(i2c, &bd2657_regmap);
	if (IS_ERR(regmap)) {
		dev_err(&i2c->dev, "Failed to initialize Regmap\n");
		return PTR_ERR(regmap);
	}

	regmap_all = regmap;

	ret = sysfs_create_group(&(&i2c->dev)->kobj, &bd2657_attr_group);
	if (ret) {
		dev_err(&i2c->dev,
			"pmic sysfs group creation failed: %d\n", ret);
	}

	ret = devm_regmap_add_irq_chip(&i2c->dev, regmap, cpu_irq, IRQF_ONESHOT,
				       0, &bd2657_cpu_irq_chip, &irq_data);
	if (ret) {
		dev_err(&i2c->dev, "Failed to add CPU IRQ chip\n");
		return ret;
	}

	domain = regmap_irq_get_domain(irq_data);
	num_res = ARRAY_SIZE(regulator_cpu_irqs);
	res = (struct resource *)&regulator_cpu_irqs[0];

	/* Power-button on BD2657 is optional */
	if (of_property_read_bool(np, "rohm,power-button-connected")) {
		ret = regmap_irq_get_virq(irq_data, BD2657_INT_PBTN_OFF);
		if (ret < 0) {
			dev_err(&i2c->dev, "Failed to get the power-key IRQ\n");
			return ret;
		}
		button.irq = ret;
		memcpy(cellp, &bd2657_mfd_cells[MFD_CELL_GPIO_KEYS],
		       sizeof(*cellp));
		num_cells++;
		cellp++;
	}

	if (device_property_present(&i2c->dev, "rohm,output-power-state-gpio"))
		gpio0_mode = GPIO0_OUT_MODE_HWCTRL;
	else
		gpio0_mode = GPIO0_OUT_MODE_GPIO;

	ret = regmap_update_bits(regmap, BD2657_REG_GPIO0_OUT,
				 GPIO0_OUT_MODE_MASK, gpio0_mode);
	if (ret)
		return ret;

	/*
	 * Should we check the gpio-reserved-ranges and completely omit the GPIO
	 * cell if OTP configuration uses GPIO1 as PMIC_EN and if GPIO0 is used
	 * for pmic-en?
	 */
	memcpy(cellp, &bd2657_mfd_cells[MFD_CELL_GPIO], sizeof(*cellp));
	num_cells++;
	cellp++;

	if (epu_irq > 0) {
		/* We have two IRQ domains so we do IRQ mapping here */
		int num_cpu_res, i;

		num_cpu_res = num_res;
		num_res += ARRAY_SIZE(regulator_epu_irqs);
		res = devm_kzalloc(&i2c->dev, num_res * sizeof(struct resource),
				   GFP_KERNEL);
		if (!res)
			return -ENOMEM;

		for (i = 0; i < num_cpu_res; i++) {
			res[i] = regulator_cpu_irqs[i];
			/* Should I rather use regmap_irq_get_virq()? */
			res[i].end = irq_create_mapping(domain,
						   regulator_cpu_irqs[i].start);
			res[i].start = res[i].end;
		}

		ret = devm_regmap_add_irq_chip(&i2c->dev, regmap, epu_irq,
					       IRQF_ONESHOT, 0,
					       &bd2657_epu_irq_chip, &irq_data);
		if (ret) {
			dev_err(&i2c->dev, "Failed to add EPU IRQ chip\n");
			return ret;
		}

		domain = regmap_irq_get_domain(irq_data);

		for (i = 0; i < ARRAY_SIZE(regulator_epu_irqs); i++) {
			res[i + num_cpu_res] = regulator_epu_irqs[i];
			res[i + num_cpu_res].end = irq_create_mapping(domain,
					regulator_epu_irqs[i].start);
			res[i + num_cpu_res].start = res[i + num_cpu_res].end;
		}
		/* As we have IRQs mapped we do not provide domain to MFD */
		domain = NULL;
	}

	bd2657_mfd_cells[MFD_CELL_REGULATOR].resources = res;
	bd2657_mfd_cells[MFD_CELL_REGULATOR].num_resources = num_res;

	memcpy(cellp, &bd2657_mfd_cells[MFD_CELL_REGULATOR], sizeof(*cellp));
	num_cells++;
	cellp++;

	pr_info("kicking %u MFD subdevices\n", num_cells);

	ret = devm_mfd_add_devices(&i2c->dev, PLATFORM_DEVID_AUTO,
				   cells, num_cells, NULL, 0,
				   domain);
	if (ret)
		dev_err(&i2c->dev, "Failed to create subdevices\n");

	return ret;
}

static const struct of_device_id bd2657_of_match[] = {
	{ .compatible = "rohm,bd2657", },
	{ },
};
MODULE_DEVICE_TABLE(of, bd2657_of_match);

static struct i2c_driver bd2657_drv = {
	.driver = {
		.name = "rohm-bd2657",
		.of_match_table = bd2657_of_match,
	},
	.probe_new = &bd2657_i2c_probe,
};
module_i2c_driver(bd2657_drv);

MODULE_AUTHOR("Matti Vaittinen <matti.vaittinen@fi.rohmeurope.com>");
MODULE_DESCRIPTION("ROHM BD2657 Power Management IC driver");
MODULE_LICENSE("GPL");
