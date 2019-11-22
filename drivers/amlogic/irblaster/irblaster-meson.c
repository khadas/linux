/*
 * drivers/amlogic/irblaster/irblaster-meson.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#include <linux/of_device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/of_address.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/amlogic/irblaster.h>
#include <linux/amlogic/irblaster_consumer.h>
#include <linux/amlogic/irblaster_encoder.h>

/* Amlogic AO_IR_BLASTER_ADDR0 bits */
#define BLASTER_BUSY				BIT(26)
#define BLASTER_FIFO_FULL			BIT(25)
#define BLASTER_FIFO_EMPTY			BIT(24)
#define BLASTER_FIFO_LEVEL			(0xff << 16)
#define BLASTER_MODULATOR_TB_SYSTEM_CLOCK	(0x0 << 12)
#define BLASTER_MODULATOR_TB_XTAL3_TICK		(0x1 << 12)
#define BLASTER_MODULATOR_TB_1US_TICK		(0x2 << 12)
#define BLASTER_MODULATOR_TB_10US_TICK		(0x3 << 12)
#define BLASTER_SLOW_CLOCK_DIV			(0xff << 4)
#define BLASTER_SLOW_CLOCK_MODE			BIT(3)
#define BLASTER_INIT_HIGH			BIT(2)
#define BLASTER_INIT_LOW			BIT(1)
#define BLASTER_ENABLE				BIT(0)

/* Amlogic AO_IR_BLASTER_ADDR1 bits */
#define BLASTER_MODULATION_LOW_COUNT(c)		((c) << 16)
#define BLASTER_MODULATION_HIGH_COUNT(c)	((c) << 0)

/* Amlogic AO_IR_BLASTER_ADDR2 bits */
#define BLASTER_WRITE_FIFO			BIT(16)
#define BLASTER_MODULATION_ENABLE		BIT(12)
#define BLASTER_TIMEBASE_1US			(0x0 << 10)
#define BLASTER_TIMEBASE_10US			(0x1 << 10)
#define BLASTER_TIMEBASE_100US			(0x2 << 10)
#define BLASTER_TIMEBASE_MODULATION_CLOCK	(0x3 << 10)

/* Amlogic AO_IR_BLASTER_ADDR3 bits */
#define BLASTER_FIFO_THD_PENDING		BIT(16)
#define BLASTER_FIFO_IRQ_ENABLE			BIT(8)
#define BLASTER_FIFO_IRQ_THRESHOLD(c)		(((c) & 0xff) << 0)

#define DEFAULT_CARRIER_FREQ			(38000)
#define DEFAULT_DUTY_CYCLE			(50)

#ifdef CONFIG_AMLOGIC_IRBLASTER_PROTOCOL
#define DEFAULT_IRBLASTER_PROTOCOL		IRBLASTER_PROTOCOL_NEC
#endif

#define LIMIT_DUTY				(25)
#define MAX_DUTY				(75)
#define LIMIT_FREQ				(25000)
#define MAX_FREQ				(60000)
#define COUNT_DELAY_MASK			(0X3ff)
#define TIMEBASE_SHIFT				(10)
#define BLASTER_KFIFO_SIZE			(4)

#define AO_IR_BLASTER_ADDR0			(0x0)
#define AO_IR_BLASTER_ADDR1			(0x4)
#define AO_IR_BLASTER_ADDR2			(0x8)
#define AO_IR_BLASTER_ADDR3			(0xc)
#define AO_RTI_GEN_CTNL_REG0			(0x0)

#define CONSUMERIR_TRANSMIT     0x5500
#define GET_CARRIER         0x5501
#define SET_CARRIER         0x5502
#define SET_DUTYCYCLE         0x5503

struct meson_irblaster_dev {
	struct device *dev;
	struct work_struct blaster_work;
	struct irblaster_chip chip;
	struct completion blaster_completion;
	unsigned int count;
	unsigned int irq;
	unsigned int buffer_size;
	unsigned int *buffer;
	spinlock_t irblaster_lock; /* use to send data */
	void __iomem	*reg_base;
	void __iomem	*reset_base;
};

static void meson_irblaster_tasklet(unsigned long data);
DECLARE_TASKLET_DISABLED(irblaster_tasklet, meson_irblaster_tasklet, 0);

static struct meson_irblaster_dev *
to_meson_irblaster(struct irblaster_chip *chip)
{
	return container_of(chip, struct meson_irblaster_dev, chip);
}

static void blaster_initialize(struct meson_irblaster_dev *dev)
{
	unsigned int carrier_cycle = 1000 / (dev->chip.state.freq / 1000);
	unsigned int high_ct, low_ct;

	/*
	 *1. disable ir blaster
	 *2. set the modulator_tb = 2'10; mpeg_1uS_tick 1us
	 *3. set initializes the output to be high
	 */
	writel_relaxed((~BLASTER_ENABLE) & (BLASTER_MODULATOR_TB_1US_TICK |
		BLASTER_INIT_HIGH), dev->reg_base + AO_IR_BLASTER_ADDR0);
	/*
	 *1. set mod_high_count = 13
	 *2. set mod_low_count = 13
	 *3. 60khz-8us, 38k-13us
	 */
	high_ct = carrier_cycle * dev->chip.state.duty / 100;
	low_ct = carrier_cycle - high_ct;
	writel_relaxed((BLASTER_MODULATION_LOW_COUNT(low_ct - 1) |
		BLASTER_MODULATION_HIGH_COUNT(high_ct - 1)),
			dev->reg_base + AO_IR_BLASTER_ADDR1);
	/*mask initialize output to be high*/
	writel_relaxed(readl_relaxed(dev->reg_base + AO_IR_BLASTER_ADDR0) &
			~BLASTER_INIT_HIGH,
			dev->reg_base + AO_IR_BLASTER_ADDR0);
	/*
	 *1. set fifo irq enable
	 *2. set fifo irq threshold
	 */
	writel_relaxed(BLASTER_FIFO_IRQ_ENABLE |
		BLASTER_FIFO_IRQ_THRESHOLD(8),
		dev->reg_base + AO_IR_BLASTER_ADDR3);
	/*enable irblaster*/
	writel_relaxed(readl_relaxed(dev->reg_base + AO_IR_BLASTER_ADDR0) |
		BLASTER_ENABLE,
		dev->reg_base + AO_IR_BLASTER_ADDR0);
}

static int write_to_fifo(struct meson_irblaster_dev *dev,
			 unsigned int hightime,
			 unsigned int lowtime)
{
	unsigned int count_delay;
	unsigned int cycle = 1000 / (dev->chip.state.freq / 1000);
	u32 val;
	int n = 0;
	int tb[3] = {
		1, 10, 100
	};

	/*
	 * hightime: modulator signal.
	 * MODULATOR_TB:
	 *	00:	system clock
	 *	01:	mpeg_xtal3_tick
	 *	10:	mpeg_1uS_tick
	 *	11:	mpeg_10uS_tick
	 *
	 * AO_IR_BLASTER_ADDR2
	 * bit12: output level(or modulation enable/disable:1=enable)
	 * bit[11:10]: Timebase :
	 *			00=1us
	 *			01=10us
	 *			10=100us
	 *			11=Modulator clock
	 * bit[9:0]: Count of timebase units to delay
	 */

	count_delay = (((hightime + cycle / 2) / cycle) - 1) & COUNT_DELAY_MASK;
	val = (BLASTER_WRITE_FIFO | BLASTER_MODULATION_ENABLE |
		BLASTER_TIMEBASE_MODULATION_CLOCK | (count_delay << 0));
	writel_relaxed(val, dev->reg_base + AO_IR_BLASTER_ADDR2);

	/*
	 * lowtime<1024,n=0,timebase=1us
	 * 1024<=lowtime<10240,n=1,timebase=10us
	 * 10240<=lowtime,n=2,timebase=100us
	 */
	n = lowtime >> 10;
	if (n > 0 && n < 10)
		n = 1;
	else if (n >= 10)
		n = 2;
	lowtime = (lowtime + (tb[n] >> 1)) / tb[n];
	count_delay = (lowtime - 1) & COUNT_DELAY_MASK;
	val = (BLASTER_WRITE_FIFO & (~BLASTER_MODULATION_ENABLE)) |
		(n << TIMEBASE_SHIFT) | (count_delay << 0);
	writel_relaxed(val, dev->reg_base + AO_IR_BLASTER_ADDR2);

	return 0;
}

static void send_all_data(struct meson_irblaster_dev *dev)
{
	int i;
	unsigned int *pdata = NULL;
	unsigned long flags;

	pdata = &dev->buffer[dev->count];
	spin_lock_irqsave(&dev->irblaster_lock, flags);
	for (i = 0; (i < 120) && (dev->count < dev->buffer_size);) {
		write_to_fifo(dev, *pdata, *(pdata + 1));
		pdata += 2;
		dev->count += 2;
		i += 2;
	}
	spin_unlock_irqrestore(&dev->irblaster_lock, flags);
}

int meson_irblaster_send(struct irblaster_chip *chip,
			 unsigned int *data,
			 unsigned int len)
{
	int ret, i, sum_time = 0;
	unsigned int high_ct, low_ct;
	unsigned int cycle;
	struct meson_irblaster_dev *irblaster_dev = to_meson_irblaster(chip);

	init_completion(&irblaster_dev->blaster_completion);
	irblaster_dev->buffer = data;
	irblaster_dev->buffer_size = len;
	irblaster_dev->count = 0;

	for (i = 0; i < irblaster_dev->buffer_size; i++)
		sum_time = sum_time + data[i];

	/*
	 * 1. set mod_high_count = 13
	 * 2. set mod_low_count = 13
	 * 3. 60khz-8us, 38k-13us
	 */
	cycle = 1000 / (irblaster_dev->chip.state.freq / 1000);
	high_ct = cycle * irblaster_dev->chip.state.duty / 100;
	low_ct = cycle - high_ct;
	writel_relaxed((BLASTER_MODULATION_LOW_COUNT(low_ct - 1) |
		BLASTER_MODULATION_HIGH_COUNT(high_ct - 1)),
			irblaster_dev->reg_base + AO_IR_BLASTER_ADDR1);

	send_all_data(irblaster_dev);
	ret = wait_for_completion_interruptible_timeout
			(&irblaster_dev->blaster_completion,
			msecs_to_jiffies(sum_time / 1000));
	if (!ret) {
		pr_err("failed to send all data ret = %d\n", ret);
		return -ETIMEDOUT;
	}

	return 0;
}

static struct irblaster_ops meson_irblaster_ops = {
	.send = meson_irblaster_send,
};

static void meson_irblaster_tasklet(unsigned long data)
{
	struct meson_irblaster_dev *dev = (struct meson_irblaster_dev *)data;

	if (dev->count < dev->buffer_size)
		send_all_data(dev);
}

static irqreturn_t meson_blaster_interrupt(int irq, void *dev_id)
{
	struct meson_irblaster_dev *dev = dev_id;

	/*clear pending bit*/
	writel_relaxed(readl_relaxed(dev->reg_base + AO_IR_BLASTER_ADDR3) &
			(~BLASTER_FIFO_THD_PENDING),
				dev->reg_base + AO_IR_BLASTER_ADDR3);

	if (dev->count >= dev->buffer_size) {
		complete(&dev->blaster_completion);
		return IRQ_HANDLED;
	}

	tasklet_schedule(&irblaster_tasklet);
	return IRQ_HANDLED;
}

static int  meson_irblaster_probe(struct platform_device *pdev)
{
	struct meson_irblaster_dev *irblaster_dev = NULL;
	struct resource *reg_mem = NULL;
	void __iomem *reg_base = NULL;
	int err, ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "pdev->dev.of_node == NULL!\n");
		return -EINVAL;
	}

	irblaster_dev = devm_kzalloc(&pdev->dev,
				     sizeof(struct meson_irblaster_dev),
				     GFP_KERNEL);
	if (!irblaster_dev)
		return -ENOMEM;

	spin_lock_init(&irblaster_dev->irblaster_lock);
	platform_set_drvdata(pdev, irblaster_dev);
	irblaster_dev->dev = &pdev->dev;

	reg_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!IS_ERR_OR_NULL(reg_mem)) {
		reg_base = devm_ioremap_resource(&pdev->dev, reg_mem);
		if (IS_ERR(reg_base)) {
			dev_err(&pdev->dev, "reg0: cannot obtain I/O memory region.\n");
			return PTR_ERR(reg_base);
		}
	} else {
		dev_err(&pdev->dev, "get IORESOURCE_MEM error.\n");
		return PTR_ERR(reg_base);
	}

	irblaster_dev->irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (!irblaster_dev->irq) {
		dev_err(&pdev->dev, "irq: Failed to request irq number.\n");
		return -ENODEV;
	}

	init_completion(&irblaster_dev->blaster_completion);
	ret = devm_request_irq(&pdev->dev, irblaster_dev->irq,
			       meson_blaster_interrupt,
			       IRQF_TRIGGER_RISING,
			       dev_name(&pdev->dev),
			       irblaster_dev);
	if (ret) {
		pr_err("Failed to request irq.\n");
		return ret;
	}

	irblaster_dev->reg_base = reg_base;
	irblaster_dev->chip.dev = &pdev->dev;
	irblaster_dev->chip.ops = &meson_irblaster_ops;
	irblaster_dev->chip.of_irblaster_n_cells = 2;
	irblaster_dev->chip.state.freq = DEFAULT_CARRIER_FREQ;
	irblaster_dev->chip.state.duty = DEFAULT_DUTY_CYCLE;
#ifdef CONFIG_AMLOGIC_IRBLASTER_PROTOCOL
	irblaster_dev->chip.state.protocol = DEFAULT_IRBLASTER_PROTOCOL;
	irblaster_set_protocol(&irblaster_dev->chip,
			       DEFAULT_IRBLASTER_PROTOCOL);
#endif
	err = irblasterchip_add(&irblaster_dev->chip);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to register irblaster chip: %d\n",
			err);
		return err;
	}

	irblaster_tasklet.data = (unsigned long)irblaster_dev;
	tasklet_enable(&irblaster_tasklet);

	/*initial blaster*/
	blaster_initialize(irblaster_dev);

	return 0;
}

static int meson_irblaster_remove(struct platform_device *pdev)
{
	struct meson_irblaster_dev *irblaster_dev = platform_get_drvdata(pdev);

	tasklet_disable(&irblaster_tasklet);
	tasklet_kill(&irblaster_tasklet);
	irblasterchip_remove(&irblaster_dev->chip);

	return 0;
}

static const struct of_device_id irblaster_dt_match[] = {
	{
		.compatible = "amlogic, meson_irblaster",
	},
	{},
};
MODULE_DEVICE_TABLE(of, irblaster_dt_match);

static struct platform_driver meson_irblaster_driver = {
	.probe = meson_irblaster_probe,
	.remove = meson_irblaster_remove,
	.driver = {
		.name = "meson_irblaster",
		.owner  = THIS_MODULE,
		.of_match_table = irblaster_dt_match,
	},
};

static int __init meson_irblaster_init(void)
{
	return platform_driver_register(&meson_irblaster_driver);
}

static void __exit meson_irblaster_exit(void)
{
	platform_driver_unregister(&meson_irblaster_driver);
}

fs_initcall_sync(meson_irblaster_init);
module_exit(meson_irblaster_exit);
MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic ir blaster driver");
MODULE_LICENSE("GPL");
