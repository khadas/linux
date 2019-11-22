/*
 * drivers/amlogic/irblaster/aml-irblaster.c
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/kfifo.h>
#include <linux/amlogic/irblaster.h>
#include <linux/amlogic/irblaster_consumer.h>
#include <linux/amlogic/irblaster_encoder.h>

#define AO_IR_BLASTER_ADDR0 (0x0)
#define AO_IR_BLASTER_ADDR1 (0x4)
#define AO_IR_BLASTER_ADDR2 (0x8)
#define AO_IR_BLASTER_ADDR3 (0xc)

#define DEFAULT_CARRIER_FREQ				(38000)
#define DEFAULT_DUTY_CYCLE				(50)
#define BLASTER_DEVICE_COUNT				(32)

#ifdef CONFIG_AMLOGIC_IRBLASTER_PROTOCOL
#define DEFAULT_IRBLASTER_PROTOCOL		IRBLASTER_PROTOCOL_NEC
#endif

#define IR_TX_EVENT_SIZE 4
#define IR_TX_BUFFER_SIZE 1024

struct tx_event {
	struct list_head list;
	unsigned int size;
	unsigned int buffer[IR_TX_BUFFER_SIZE];
};

DECLARE_KFIFO(fifo, struct tx_event *, IR_TX_EVENT_SIZE);

struct aml_irblaster_dev {
	struct irblaster_chip chip;
	struct device *dev;
	struct task_struct	*thread;
	struct completion blaster_completion;
	struct mutex lock;
	spinlock_t irblaster_lock; /* use to send data */
	void __iomem	*reg_base;
	void __iomem	*reset_base;
	unsigned int winnum;
	unsigned int winarray[MAX_PLUSE];
};

static struct aml_irblaster_dev *
to_aml_irblaster(struct irblaster_chip *chip)
{
	return container_of(chip, struct aml_irblaster_dev, chip);
}

static struct tx_event *event_get(struct aml_irblaster_dev *cw)
{
	struct tx_event *ev = NULL;

	ev = devm_kzalloc(cw->dev,
			sizeof(struct tx_event), GFP_KERNEL);
	return ev;
}

static void event_put(struct aml_irblaster_dev *cw, struct tx_event *ev)
{
	devm_kfree(cw->dev, ev);
}

static int send_bit(struct aml_irblaster_dev *cw, unsigned int hightime,
		    unsigned int lowtime, unsigned int cycle)
{
	unsigned int count_delay;
	uint32_t val;
	int n = 0;
	int tb[3] = {
		1, 10, 100
	};
	/*
	 * MODULATOR_TB:
	 *	00:	system clock clk
	 *	01:	mpeg_xtal3_tick
	 *	10:	mpeg_1uS_tick
	 *	11:	mpeg_10uS_tick
	 * lowtime<1024,n=0,timebase=1us
	 * 1024<=lowtime<10240,n=1,timebase=10us
	 * AO_IR_BLASTER_ADDR2
	 * bit12: output level(or modulation enable/disable:1=enable)
	 * bit[11:10]: Timebase :
	 *			00=1us
	 *			01=10us
	 *			10=100us
	 *			11=Modulator clock
	 * bit[9:0]: Count of timebase units to delay
	 */
	count_delay = (((hightime + cycle/2) / cycle) - 1) & 0x3ff;
	val = (0x10000 | (1 << 12)) | (3 << 10) | (count_delay << 0);
	writel_relaxed(val, cw->reg_base + AO_IR_BLASTER_ADDR2);

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
	lowtime = (lowtime + (tb[n] >> 1))/tb[n];
	count_delay = (lowtime-1) & 0x3ff;
	val = (0x10000 | (0 << 12)) |
		(n << 10) | (count_delay << 0);
	writel_relaxed(val, cw->reg_base + AO_IR_BLASTER_ADDR2);

	return 0;
}

static void send_all_frame(struct aml_irblaster_dev *cw)
{
	int i, k;
	int exp = 0x00;
	unsigned int *pData;
	unsigned int consumerir_cycle;
	unsigned int high_ct, low_ct;
	unsigned long cnt, flags;

	consumerir_cycle = 1000 / (cw->chip.state.freq / 1000);

	/*reset*/
	writel_relaxed(readl_relaxed(cw->reset_base) |
		       (1 << 23), cw->reset_base);
	udelay(2);
	writel_relaxed(readl_relaxed(cw->reset_base) &
		       ~(1 << 23), cw->reset_base);

	/*
	 * 1.disable ir blaster
	 * 2.set the modulator_tb = 2'10; mpeg_1uS_tick 1us
	 */
	writel_relaxed((1 << 2) | (2 << 12) | (1<<2),
		       cw->reg_base + AO_IR_BLASTER_ADDR0);

	/*
	 * 1. set mod_high_count = 13
	 * 2. set mod_low_count = 13
	 * 3. 60khz 8, 38k-13us, 12
	 */
	high_ct = consumerir_cycle * cw->chip.state.duty/100;
	low_ct = consumerir_cycle - high_ct;
	writel_relaxed(((high_ct - 1) << 16) | ((low_ct - 1) << 0),
		       cw->reg_base + AO_IR_BLASTER_ADDR1);

	/* Setting this bit to 1 initializes the output to be high.*/
	writel_relaxed(readl_relaxed(cw->reg_base + AO_IR_BLASTER_ADDR0) &
		       ~(1 << 2), cw->reg_base + AO_IR_BLASTER_ADDR0);

	/*enable irblaster*/
	writel_relaxed(readl_relaxed(cw->reg_base + AO_IR_BLASTER_ADDR0) |
		       (1 << 0), cw->reg_base + AO_IR_BLASTER_ADDR0);

	k = cw->winnum;
#define SEND_BIT_NUM 64
	exp = cw->winnum / SEND_BIT_NUM;
	pData = cw->winarray;

	while (exp) {
		spin_lock_irqsave(&cw->irblaster_lock, flags);
		for (i = 0; i < SEND_BIT_NUM/2; i++) {
			send_bit(cw, *pData, *(pData+1), consumerir_cycle);
			pData += 2;
		}

		spin_unlock_irqrestore(&cw->irblaster_lock, flags);
		cnt = jiffies + msecs_to_jiffies(1000);
		while (!(readl_relaxed(cw->reg_base + AO_IR_BLASTER_ADDR0) &
			(1<<24)) && time_is_after_eq_jiffies(cnt))
			;

		cnt = jiffies + msecs_to_jiffies(1000);
		while ((readl_relaxed(cw->reg_base + AO_IR_BLASTER_ADDR0) &
			(1<<26)) && time_is_after_eq_jiffies(cnt))
			;

		/*reset*/
		writel_relaxed(readl_relaxed(cw->reset_base) | (1 << 23),
			       cw->reset_base);
		udelay(2);
		writel_relaxed(readl_relaxed(cw->reset_base) & ~(1 << 23),
			       cw->reset_base);
		exp--;
	}

	exp = (cw->winnum % SEND_BIT_NUM) & (~(1));
	spin_lock_irqsave(&cw->irblaster_lock, flags);
	for (i = 0; i < exp; ) {
		send_bit(cw, *pData, *(pData+1), consumerir_cycle);
		pData += 2;
		i += 2;
	}

	spin_unlock_irqrestore(&cw->irblaster_lock, flags);
	cnt = jiffies + msecs_to_jiffies(1000);
	while (!(readl_relaxed(cw->reg_base + AO_IR_BLASTER_ADDR0) &
		(1<<24)) && time_is_after_eq_jiffies(cnt))
		;

	cnt = jiffies + msecs_to_jiffies(1000);
	while ((readl_relaxed(cw->reg_base + AO_IR_BLASTER_ADDR0) &
		(1<<26)) && time_is_after_eq_jiffies(cnt))
		;

	complete(&cw->blaster_completion);
}

int aml_irblaster_send(struct irblaster_chip *chip,
			 unsigned int *data,
			 unsigned int len)
{
	int i, ret;
	struct tx_event *ev;
	struct aml_irblaster_dev *irblaster_dev = to_aml_irblaster(chip);

	init_completion(&irblaster_dev->blaster_completion);
	ev = event_get(irblaster_dev);
	ev->size = len;
	for (i = 0; i < ev->size; i++)
		ev->buffer[i] = data[i];

	/* to send cycle */
	kfifo_put(&fifo, (const struct tx_event *)ev);
	/* to wake up ir_tx_thread */
	wake_up_process(irblaster_dev->thread);
	/* return after processing */
	ret = wait_for_completion_interruptible_timeout
			(&irblaster_dev->blaster_completion,
			msecs_to_jiffies(chip->sum_time / 1000));
	if (!ret) {
		pr_err("failed to send all data ret = %d\n", ret);
		return -ETIMEDOUT;
	}

	return 0;
}

static int ir_tx_thread(void *data)
{
	int retval, i;
	unsigned long cnt;
	struct aml_irblaster_dev *irblaster_dev
		= (struct aml_irblaster_dev *)data;
	struct tx_event *ev = NULL;

	while (!kthread_should_stop()) {
		retval = kfifo_len(&fifo);
		if (retval <= 0) {
			set_current_state(TASK_INTERRUPTIBLE);
			if (kthread_should_stop())
				set_current_state(TASK_RUNNING);
			schedule();
			continue;
		}

		retval = kfifo_get(&fifo, &ev);
		if (retval) {
			irblaster_dev->winnum = ev->size;
			for (i = 0; i < irblaster_dev->winnum; i++)
				irblaster_dev->winarray[i] = ev->buffer[i];

			send_all_frame(irblaster_dev);
			event_put(irblaster_dev, ev);
			cnt = jiffies + msecs_to_jiffies(1000);
			while (!(readl_relaxed(irblaster_dev->reg_base +
				AO_IR_BLASTER_ADDR0) & (1<<24)) &&
				time_is_after_eq_jiffies(cnt))
				;

			cnt = jiffies + msecs_to_jiffies(1000);
			while ((readl_relaxed(irblaster_dev->reg_base +
				AO_IR_BLASTER_ADDR0) & (1<<26)) &&
				time_is_after_eq_jiffies(cnt))
				;

		} else
			pr_err("kfifo_get fail\n");
	}

	return 0;
}

static struct irblaster_ops aml_irblaster_ops = {
	.send = aml_irblaster_send,
};

static int aml_irblaster_probe(struct platform_device *pdev)
{
	struct aml_irblaster_dev *irblaster_dev = NULL;
	struct resource *reg_mem = NULL;
	struct resource *reset_mem = NULL;
	void __iomem *reg_base = NULL;
	void __iomem *reset_base = NULL;
	int err;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "pdev->dev.of_node == NULL!\n");
		return -EINVAL;
	}

	irblaster_dev = devm_kzalloc(&pdev->dev,
				     sizeof(struct aml_irblaster_dev),
				     GFP_KERNEL);
	if (!irblaster_dev)
		return -ENOMEM;

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

	reset_mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!IS_ERR_OR_NULL(reset_mem)) {
		reset_base = devm_ioremap_resource(&pdev->dev,
					reset_mem);
		if (IS_ERR(reset_base)) {
			dev_err(&pdev->dev, "reg1: cannot obtain I/O memory region.\n");
			return PTR_ERR(reset_base);
		}
	} else {
		dev_err(&pdev->dev, "get IORESOURCE_MEM error.\n");
		return PTR_ERR(reset_mem);
	}

	spin_lock_init(&irblaster_dev->irblaster_lock);
	init_completion(&irblaster_dev->blaster_completion);
	irblaster_dev->reg_base = reg_base;
	irblaster_dev->reset_base = reset_base;
	irblaster_dev->chip.dev = &pdev->dev;
	irblaster_dev->chip.ops = &aml_irblaster_ops;
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

	irblaster_dev->thread = kthread_run(ir_tx_thread, irblaster_dev,
		 "ir-blaster-thread");

	return 0;
}

static int aml_irblaster_remove(struct platform_device *pdev)
{
	struct aml_irblaster_dev *irblaster_dev = platform_get_drvdata(pdev);

	irblasterchip_remove(&irblaster_dev->chip);

	return 0;
}

static const struct of_device_id irblaster_dt_match[] = {
	{
		.compatible = "amlogic, aml_irblaster",
	},
	{},
};

static struct platform_driver aml_irblaster_driver = {
	.probe		= aml_irblaster_probe,
	.remove		= aml_irblaster_remove,
	.suspend	= NULL,
	.resume		= NULL,
	.driver = {
		.name = "aml-irblaster",
		.owner	= THIS_MODULE,
		.of_match_table = irblaster_dt_match,
	},
};

static int __init aml_irblaster_init(void)
{
	return platform_driver_register(&aml_irblaster_driver);
}

static void __exit aml_irblaster_exit(void)
{
	platform_driver_unregister(&aml_irblaster_driver);
}

fs_initcall_sync(aml_irblaster_init);
module_exit(aml_irblaster_exit);
MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic ir blaster driver");
MODULE_LICENSE("GPL");
