/*
 * drivers/amlogic/irblaster/irblaster.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/major.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/cpu_version.h>
#include "irblaster.h"
/* #define DEBUG */
#ifdef DEBUG
#define irblaster_dbg(fmt, args...) pr_info("ir_blaster: " fmt, ##args)
#else
#define irblaster_dbg(fmt, args...)
#endif

#define DEVICE_NAME "meson-irblaster"
#define DEIVE_COUNT 32
static dev_t amirblaster_id;
static struct class *irblaster_class;
static struct device *irblaster_dev;
static struct cdev amirblaster_device;
static struct aml_blaster *irblaster;
static DEFINE_MUTEX(irblaster_file_mutex);
static void aml_consumerir_transmit(struct aml_blaster *aml_transmit)
{
	int i, k;
	unsigned int consumerir_freqs =
		1000 / (irblaster->consumerir_freqs / 1000);
	unsigned int high_level_modulation_enable = 1 << 12;
	unsigned int high_level_modulation_disable = ~(1 << 12);
	if (irblaster->fisrt_pulse_width == fisrt_low) {
		high_level_modulation_enable = 1 << 12;
		high_level_modulation_disable = ~(1 << 12);
	}
	if (irblaster->fisrt_pulse_width == fisrt_high) {
		high_level_modulation_disable = 1 << 12;
		high_level_modulation_enable = ~(1 << 12);
	}
	/*set init_high valid and enable the ir_blaster*/
	if (!is_meson_m8_cpu()) {
		/* code */
		aml_write_aobus(AO_IR_BLASTER_ADDR0,
		aml_read_aobus(AO_IR_BLASTER_ADDR0) & (~(1 << 0)));
		udelay(1);
		aml_write_aobus(AO_IR_BLASTER_ADDR2, 0x10000);
	}
	irblaster_dbg("Enable blaster & create format!! consumerir_freqs %d\n",
		      consumerir_freqs);
	aml_write_aobus(AO_IR_BLASTER_ADDR0,
			aml_read_aobus(AO_IR_BLASTER_ADDR0) |
		(2 << 12));   /*set the modulator_tb = 2'10; 1us*/
	aml_write_aobus(AO_IR_BLASTER_ADDR1,
			aml_read_aobus(AO_IR_BLASTER_ADDR1) |
		(((consumerir_freqs / 2) - 1) << 16));
		/*set mod_high_count = 13;*/
	aml_write_aobus(AO_IR_BLASTER_ADDR1,
	aml_read_aobus(AO_IR_BLASTER_ADDR1) |
		(((consumerir_freqs / 2) - 1) << 0));
		/*set mod_low_count = 13;*/
	aml_write_aobus(AO_IR_BLASTER_ADDR0,
			aml_read_aobus(AO_IR_BLASTER_ADDR0) |
			(1 << 2) | (1 << 0));
	udelay(1);
	aml_write_aobus(AO_IR_BLASTER_ADDR0,
			aml_read_aobus(AO_IR_BLASTER_ADDR0) |
			(1 << 2) | (1 << 0));
	k = aml_transmit->pattern_len;
	if (aml_transmit->pattern_len) {
		for (i = 0; i < k / 2; i++) {
			aml_write_aobus(AO_IR_BLASTER_ADDR2,
			(0x10000 &  high_level_modulation_disable)
		/*timeleve = 0;*/
			| (3 << 10)
		/*[11:10] = 2'b01,then set the timebase 10us.*/
			| ((((aml_transmit->pattern[2 * i]) - 1) /
				consumerir_freqs) << 0)
		/*[9:0] = 10'd,the timecount = N+1;*/
			);
			udelay((aml_transmit->pattern[2 * i]));
			aml_write_aobus(AO_IR_BLASTER_ADDR2, (0x10000 |
				high_level_modulation_enable)
		/*timeleve = 1;[11:10] = 2'b11,
			then set the timebase 26.5us. */
			| (1 << 10)
			| ((((aml_transmit->pattern[2 * i + 1] / 10) - 1))
				<< 0)
			/*[9:0] = 10'd,the timecount = N+1;*/
				       );
			udelay((aml_transmit->pattern[2 * i + 1]));
	/*irblaster_dbg("aml_transmit->pattern[%d]:%d
	aml_transmit->pattern[%d]:%d\n",
	2*i,aml_transmit->pattern[2*i],(2*i)+1,
	aml_transmit->pattern[2*i+1]/10);*/
		}
	}
	/*for (j=0; j<72-k; j++)
		aml_write_aobus( AO_IR_BLASTER_ADDR2,0x10000);
	*/
	irblaster_dbg("The all frame finished !!\n");

}

/**
  * Function to set the irblaster Carrier Frequency,
  * The modulator is typically run between 32khz and 56khz.
  *
  * @param[in] pointer to irblaster structure.
  * @param[in] carrirer freqs value.
  * \return Reuturns 0 on success else return the error value.
 */

int set_consumerir_freqs(struct aml_blaster *irblaster, int consumerir_freqs)
{
	return	((irblaster->consumerir_freqs = consumerir_freqs) >= 32000
		 && (irblaster->consumerir_freqs <= 56000)) ? 0 : -1;
}

/**
  * Function to set the irblaster High or low modulation.
  *
  * @param[in] pointer to irblaster structure.
  * @param[in] level 0  mod low level 1 mod high.
  * \return Reutrn set value.
 */


int set_consumerir_mod_level(struct aml_blaster *irblaster,
			     int modulation_level)
{
	irblaster->fisrt_pulse_width = modulation_level;
	return 0;
}


/**
  * Function to get the irblaster cur Carrier Frequency.
  *
  * @param[in] pointer to irblaster structure.
  * \return Reuturns freqs.
 */

static int  get_consumerir_freqs(struct aml_blaster *irblaster)
{
	return irblaster->consumerir_freqs;
}


static int aml_irblaster_open(struct inode *inode, struct file *file)
{
	irblaster_dbg("aml_irblaster_open()\n");
	aml_write_aobus(AO_RTI_PIN_MUX_REG ,
		aml_read_aobus(AO_RTI_PIN_MUX_REG) | (1 << 31));
	return 0;
}

static long aml_irblaster_ioctl(struct file *filp, unsigned int cmd,
				unsigned long args)
{

	int consumerir_freqs = 0;
	int modulation_level = 0;
	s32 r = 0;
	unsigned long flags;
	static struct aml_blaster consumerir_transmit;
	void __user *argp = (void __user *)args;
	irblaster_dbg("aml_irblaster_ioctl()  0x%4x\n ", cmd);
	switch (cmd) {
	case CONSUMERIR_TRANSMIT:
		if (copy_from_user(&consumerir_transmit, argp,
					sizeof(struct aml_blaster)))
			return -EFAULT;

		/*	for(i=0; i<consumerir_transmit.pattern_len; i++)
				irblaster_dbg("idx[%d]:[%d]\n", i,
				consumerir_transmit.pattern[i]);*/
		irblaster_dbg("Transmit [%d]\n",
			consumerir_transmit.pattern_len);
		local_irq_save(flags);
		aml_consumerir_transmit(&consumerir_transmit);
		local_irq_restore(flags);
		break;

	case GET_CARRIER:
		consumerir_freqs = get_consumerir_freqs(irblaster);
		if (copy_to_user(argp, &consumerir_freqs, sizeof(int)))
			return -EFAULT;

		break;
	case SET_CARRIER:
		if (copy_from_user(&consumerir_freqs, argp, sizeof(int)))
			return -EFAULT;
		return set_consumerir_freqs(irblaster, consumerir_freqs);

	case SET_MODLEVEL:  /*Modulation level*/
		if (copy_from_user(&consumerir_freqs, argp, sizeof(int)))
			return -EFAULT;
		return set_consumerir_mod_level(irblaster, modulation_level);

	default:
		r = -ENOIOCTLCMD;
		break;
	}

	return r;
}
static int aml_irblaster_release(struct inode *inode, struct file *file)
{
	irblaster_dbg("aml_ir_irblaster_release()\n");
	file->private_data = NULL;
	aml_write_aobus(AO_RTI_PIN_MUX_REG ,
		aml_read_aobus(AO_RTI_PIN_MUX_REG) & ~(1 << 31));
	return 0;

}
static const struct file_operations aml_irblaster_fops = {
	.owner		= THIS_MODULE,
	.open		= aml_irblaster_open,
	.unlocked_ioctl = aml_irblaster_ioctl,
	.release	= aml_irblaster_release,
};

static int  aml_irblaster_probe(struct platform_device *pdev)
{
	int r;
	pr_info("irblaster probe\n");
	irblaster = kmalloc(sizeof(struct aml_blaster), GFP_KERNEL);
	if (irblaster == NULL)
		return -1;
	memset(irblaster, 0, sizeof(struct aml_blaster));

	if (!pdev->dev.of_node) {
		pr_info("aml_irblaster: pdev->dev.of_node == NULL!\n");
		return -1;
	}
	r = alloc_chrdev_region(&amirblaster_id, 0, DEIVE_COUNT, DEVICE_NAME);
	if (r < 0) {
		pr_err("Can't register major for ir irblaster device\n");
		return r;
	}
	cdev_init(&amirblaster_device, &aml_irblaster_fops);
	amirblaster_device.owner = THIS_MODULE;
	cdev_add(&(amirblaster_device), amirblaster_id, DEIVE_COUNT);
	irblaster_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(irblaster_class)) {
		unregister_chrdev_region(amirblaster_id, DEIVE_COUNT);
		pr_info("Can't create class for ir irblaster device\n");
		return -1;
	}
	irblaster_dev = device_create(irblaster_class, NULL,
					amirblaster_id, NULL,
				      "irblaster%d", 1);
	if (irblaster_dev == NULL) {
		pr_err("irblaster_dev create error\n");
		class_destroy(irblaster_class);
		return -EEXIST;
	}
	return 0;
}

static int aml_irblaster_remove(struct platform_device *pdev)
{
	pr_info("remove IRBLASTER\n");
	kfree(irblaster);
	cdev_del(&amirblaster_device);
	device_destroy(irblaster_class, amirblaster_id);
	class_destroy(irblaster_class);
	unregister_chrdev_region(amirblaster_id, DEIVE_COUNT);
	return 0;
}
static const struct of_device_id irblaster_dt_match[] = {
	{
		.compatible	= "amlogic, am_irblaster",
	},
	{},
};
static struct platform_driver aml_irblaster_driver = {
	.probe		= aml_irblaster_probe,
	.remove		= aml_irblaster_remove,
	.suspend	= NULL,
	.resume		= NULL,
	.driver = {
		.name = "meson-irblaster",
		.owner  = THIS_MODULE,
		.of_match_table = irblaster_dt_match,
	},
};



static int __init aml_irblaster_init(void)
{
	pr_info("BLASTER Driver Init\n");
	if (platform_driver_register(&aml_irblaster_driver)) {
		irblaster_dbg("failed to register aml_ir_irblaster_driver module\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit aml_irblaster_exit(void)
{
	pr_info("IRBLASTER Driver exit\n");
	platform_driver_unregister(&aml_irblaster_driver);
}
module_init(aml_irblaster_init);
module_exit(aml_irblaster_exit);

MODULE_AUTHOR("platform-beijing");
MODULE_DESCRIPTION("Irblaster Driver");
MODULE_LICENSE("GPL");
