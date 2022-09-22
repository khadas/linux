// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/gpio/driver.h>

/*
 * Register info for mclk analog GPIOQ group
 * GPIOQ_[N]:
 *      ANACTRL_MCLK_PLL_CNTL[4 + 2 * N]: 0xfe008310 + 8 * N
 *		bit[6:4]: 0x111 to select gpio function
 *      ANACTRL_MCLK_PLL_CNTL[5 + 2 * N]: 0xfe008310 + 8 * N + 4
 *		bit[18]:Mclk_gpio_in_en set to 1 to output
 *		bit[19]:Mclk_gpio_in, 1 to high and 0 to low level
 * where N = 0 ~ 4
 *
 */
#define GPIO_NUM			5
#define GPIO_MUX_REG_OFFSET(offset)	((offset) * 8)
#define GPIO_FUN_REG_OFFSET(offset)	((offset) * 8 + 4)
#define GPIO_MUX_MASK			(GENMASK(6, 4))
#define GPIO_DIR_BIT			(BIT(18))
#define GPIO_VALUE_BIT			(BIT(19))

struct meson_mclk_gpio {
	struct gpio_chip chip;
	void __iomem *reg_base;
	/* lock register writing */
	spinlock_t lock;
};

static void meson_mclk_gpio_free(struct gpio_chip *chip, unsigned int offset)
{
	struct meson_mclk_gpio *const mgpio = gpiochip_get_data(chip);
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&mgpio->lock, flags);
	val = readl(mgpio->reg_base + GPIO_MUX_REG_OFFSET(offset));
	val &= ~GPIO_MUX_MASK;
	writel(val, mgpio->reg_base + GPIO_MUX_REG_OFFSET(offset));
	spin_unlock_irqrestore(&mgpio->lock, flags);
}

static void meson_mclk_gpio_set(struct gpio_chip *chip, unsigned int offset,
				int value)
{
	struct meson_mclk_gpio *const mgpio = gpiochip_get_data(chip);
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&mgpio->lock, flags);
	val = readl(mgpio->reg_base + GPIO_FUN_REG_OFFSET(offset));

	if (value)
		val |= GPIO_VALUE_BIT;
	else
		val &= ~(GPIO_VALUE_BIT);

	writel(val, mgpio->reg_base + GPIO_FUN_REG_OFFSET(offset));
	spin_unlock_irqrestore(&mgpio->lock, flags);
}

static int meson_mclk_gpio_get(struct gpio_chip *chip, unsigned int offset)
{
	struct meson_mclk_gpio *const mgpio = gpiochip_get_data(chip);
	u32 val;
	unsigned long flags;

	/* no way to read input value, return output value here*/
	spin_lock_irqsave(&mgpio->lock, flags);
	val = readl(mgpio->reg_base + GPIO_FUN_REG_OFFSET(offset));
	spin_unlock_irqrestore(&mgpio->lock, flags);

	return val & GPIO_VALUE_BIT;
}

static int meson_mclk_gpio_direction_output(struct gpio_chip *chip,
					    unsigned int offset, int value)
{
	struct meson_mclk_gpio *const mgpio = gpiochip_get_data(chip);
	u32 val;
	unsigned long flags;

	/* set value */
	meson_mclk_gpio_set(chip, offset, value);

	/* set dir output */
	spin_lock_irqsave(&mgpio->lock, flags);
	val = readl(mgpio->reg_base + GPIO_FUN_REG_OFFSET(offset));
	val |= GPIO_DIR_BIT;
	writel(val, mgpio->reg_base + GPIO_FUN_REG_OFFSET(offset));
	spin_unlock_irqrestore(&mgpio->lock, flags);

	return 0;
}

static int meson_mclk_gpio_direction_input(struct gpio_chip *chip,
					   unsigned int offset)
{
	struct meson_mclk_gpio *const mgpio = gpiochip_get_data(chip);
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&mgpio->lock, flags);
	val = readl(mgpio->reg_base + GPIO_FUN_REG_OFFSET(offset));
	val &= ~GPIO_DIR_BIT;
	writel(val, mgpio->reg_base + GPIO_FUN_REG_OFFSET(offset));
	spin_unlock_irqrestore(&mgpio->lock, flags);

	return 0;
}

static int meson_mclk_gpio_get_direction(struct gpio_chip *chip,
					 unsigned int offset)
{
	struct meson_mclk_gpio *const mgpio = gpiochip_get_data(chip);
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&mgpio->lock, flags);
	val = readl(mgpio->reg_base + GPIO_FUN_REG_OFFSET(offset));
	spin_unlock_irqrestore(&mgpio->lock, flags);

	return !(val & GPIO_DIR_BIT);
}

static int meson_mclk_gpio_request(struct gpio_chip *chip, unsigned int offset)
{
	struct meson_mclk_gpio *const mgpio = gpiochip_get_data(chip);
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&mgpio->lock, flags);

	val = readl(mgpio->reg_base + GPIO_MUX_REG_OFFSET(offset));
	val |= GPIO_MUX_MASK;
	writel(val, mgpio->reg_base + GPIO_MUX_REG_OFFSET(offset));

	spin_unlock_irqrestore(&mgpio->lock, flags);

	return 0;
}

static const char *gpio_names[GPIO_NUM] = {
	"GPIOQ_0", "GPIOQ_1",
	"GPIOQ_2", "GPIOQ_3",
	"GPIOQ_4",
};

static int meson_mclk_gpio_probe(struct platform_device *pdev)
{
	struct meson_mclk_gpio *mgpio;
	struct resource *res_reg;
	int err;

	mgpio = devm_kzalloc(&pdev->dev, sizeof(*mgpio), GFP_KERNEL);
	if (!mgpio)
		return -ENOMEM;

	mgpio->chip.label = dev_name(&pdev->dev);
	mgpio->chip.owner = THIS_MODULE;
	mgpio->chip.base = -1;
	mgpio->chip.ngpio = GPIO_NUM;
	mgpio->chip.names = gpio_names;
	mgpio->chip.get_direction = meson_mclk_gpio_get_direction;
	mgpio->chip.direction_input = meson_mclk_gpio_direction_input;
	mgpio->chip.direction_output = meson_mclk_gpio_direction_output;
	mgpio->chip.get = meson_mclk_gpio_get;
	mgpio->chip.set = meson_mclk_gpio_set;
	mgpio->chip.request = meson_mclk_gpio_request;
	mgpio->chip.free = meson_mclk_gpio_free;
	mgpio->chip.of_node = pdev->dev.of_node;
	mgpio->chip.of_gpio_n_cells = 2;
	mgpio->chip.can_sleep = false;

	spin_lock_init(&mgpio->lock);

	res_reg = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (IS_ERR_OR_NULL(res_reg)) {
		dev_err(&pdev->dev, "Get IORESOURCE_MEM failed\n");
		return PTR_ERR(res_reg);
	}

	mgpio->reg_base = ioremap(res_reg->start, resource_size(res_reg));
	if (!mgpio->reg_base)
		return -EIO;

	dev_info(&pdev->dev, "get reg start at  0x%llx, end 0x%llx\n",
		 res_reg->start, res_reg->end);

	err = devm_gpiochip_add_data(&pdev->dev, &mgpio->chip, mgpio);
	if (err) {
		dev_err(&pdev->dev, "GPIO registering failed (%d)\n", err);
		iounmap(mgpio->reg_base);
		return err;
	}

	/* free after adding to keep name /sys/class/gpio/gpioxxx */
	mgpio->chip.names = NULL;

	platform_set_drvdata(pdev, mgpio);

	return 0;
}

static const struct of_device_id meson_mclk_gpio_of_match[] = {
	{ .compatible = "amlogic,meson-mclk-gpio", },
	{ },
};

static struct platform_driver meson_mclk_gpio_driver = {
	.driver		= {
		.name	= "meson-mclk-gpio",
		.of_match_table = meson_mclk_gpio_of_match,
	},
	.probe		= meson_mclk_gpio_probe,
};

module_platform_driver(meson_mclk_gpio_driver);
