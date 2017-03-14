/*
 * drivers/amlogic/input/adc_keypad/adc_keypad.c
 *
 * ADC Keypad Driver
 *
 * Copyright (C) 2017 Amlogic, Inc.
 *
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
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/amlogic/saradc.h>
#include <linux/amlogic/input/adc_keypad.h>
#include <linux/amlogic/scpi_protocol.h>
#include <linux/amlogic/pm.h>

#define POLL_PERIOD_WHEN_KEY_DOWN 20 /* unit msec */
#define POLL_PERIOD_WHEN_KEY_UP   50
#define KEY_JITTER_COUNT  1  /*  1 * POLL_PERIOD_WHEN_KEY_DOWN msec  */
#define TMP_BUF_MAX 128

static char adc_key_mode_name[MAX_NAME_LEN] = "abcdef";
static char kernelkey_en_name[MAX_NAME_LEN] = "abcdef";
static bool keypad_enable_flag = 1;

static int kp_search_key(struct kp *kp)
{
	struct adc_key *key;
	int value, i;

	spin_lock(&kp->kp_lock);
	for (i = 0; i < kp->chan_num; i++) {
		value = get_adc_sample(0, kp->chan[i]);
		if (value < 0)
			continue;
		list_for_each_entry(key, &kp->adckey_head, list) {
			if ((key->chan == kp->chan[i])
			&& (value >= key->value - key->tolerance)
			&& (value <= key->value + key->tolerance)) {
				spin_unlock(&kp->kp_lock);
				return key->code;
			}
		}
	}
	spin_unlock(&kp->kp_lock);
	return 0;
}

static void kp_work(struct kp *kp)
{
	int code = kp_search_key(kp);
	if (code)
		kp->poll_period = POLL_PERIOD_WHEN_KEY_DOWN;
	if ((!code) && (!kp->report_code)) {
		if (kp->poll_period < POLL_PERIOD_WHEN_KEY_UP)
			kp->poll_period++;
		return;
	} else if (code != kp->code) {
		kp->code = code;
		kp->count = 0;
	} else if (kp->count < KEY_JITTER_COUNT) {
		kp->count++;
		} else {
			if ((kp->report_code != code) && keypad_enable_flag) {
				if (!code) { /* key up */
				dev_info(&kp->input->dev,
					"key %d up\n", kp->report_code);
				input_report_key(kp->input, kp->report_code, 0);
				input_sync(kp->input);
			} else if (!kp->report_code) { /* key down */
				input_report_key(kp->input, code, 1);
				input_sync(kp->input);
			} else {
				dev_info(&kp->input->dev,
					"key %d up(f)\n", kp->report_code);
				input_report_key(kp->input, kp->report_code, 0);
				input_report_key(kp->input, code, 1);
				input_sync(kp->input);
			}
			kp->report_code = code;
		}
	}
}

static void update_work_func(struct work_struct *work)
{
	struct kp *kp = container_of(work, struct kp, work_update);
	kp_work(kp);
}

static void kp_timer_sr(unsigned long data)
{
	struct kp *kp = (struct kp *)data;
	schedule_work(&(kp->work_update));
	mod_timer(&kp->timer, jiffies+msecs_to_jiffies(kp->poll_period));
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void kp_early_suspend(struct early_suspend *h)
{
	struct kp *kp = container_of(h, struct kp, early_suspend);
	del_timer_sync(&kp->timer);
	cancel_work_sync(&kp->work_update);
}

static void kp_late_resume(struct early_suspend *h)
{
	struct kp *kp = container_of(h, struct kp, early_suspend);
	 mod_timer(&kp->timer, jiffies+msecs_to_jiffies(kp->poll_period));
}
#endif

static void send_data_to_bl301(void)
{
	u32 val;
	if (!strcmp(adc_key_mode_name, "POWER_WAKEUP_POWER")) {
		val = 0;  /*only power key resume*/
		scpi_send_usr_data(SCPI_CL_POWER, &val, sizeof(val));
	} else if (!strcmp(adc_key_mode_name, "POWER_WAKEUP_ANY")) {
		val = 1; /*any key resume*/
		scpi_send_usr_data(SCPI_CL_POWER, &val, sizeof(val));
	} else if (!strcmp(adc_key_mode_name, "POWER_WAKEUP_NONE")) {
		val = 2; /*no key can resume*/
		scpi_send_usr_data(SCPI_CL_POWER, &val, sizeof(val));
	}

}

static void kernel_keypad_enable_mode_enable(void)
{

	if (!strcmp(kernelkey_en_name, "KEYPAD_UNLOCK")) {
		keypad_enable_flag = 1;  /*unlock, normal mode*/
	} else if (!strcmp(kernelkey_en_name, "KEYPAD_LOCK")) {
		keypad_enable_flag = 0;  /*lock, press key will be not usefull*/
	} else {
		keypad_enable_flag = 1;
	}
}

/*kp_valid_chan_update() - used to update valid adc channel
 *
 *@kp: to save number of channel in
 *
 */
static void kp_valid_chan_update(struct kp *kp)
{
	unsigned char incr;
	struct adc_key *key;

	spin_lock(&kp->kp_lock);
	kp->chan_num = 0; /*recalculate*/
	list_for_each_entry(key, &kp->adckey_head, list) {
		if (0 == kp->chan_num) {
			kp->chan[kp->chan_num++] = key->chan;
		} else {
			for (incr = 0; incr < kp->chan_num; incr++) {
				if (key->chan == kp->chan[incr])
					break;
				else
					if (incr == (kp->chan_num - 1))
						kp->chan[kp->chan_num++]
							= key->chan;
			}
		}
	}
	spin_unlock(&kp->kp_lock);
}
static int kp_get_devtree_pdata(struct platform_device *pdev, struct kp *kp)
{
	int ret;
	int state = 0;
	unsigned char cnt;
	const char *uname;
	unsigned int key_num;
	struct adc_key *key;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "pdev->dev.of_node == NULL!\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "key_num", &key_num);
	if (ret) {
		dev_err(&pdev->dev, "faild to get key_num!\n");
		return -EINVAL;
	}
	for (cnt = 0; cnt < key_num; cnt++) {
		key = kzalloc(sizeof(struct adc_key), GFP_KERNEL);
		if (!key) {
			dev_err(&pdev->dev, "alloc mem failed!\n");
			return -ENOMEM;
		}

		ret = of_property_read_string_index(pdev->dev.of_node,
			 "key_name", cnt, &uname);
		if (ret < 0) {
			dev_err(&pdev->dev, "invalid key name index[%d]\n",
				cnt);
			state = -EINVAL;
			goto err;
		}
		strncpy(key->name, uname, MAX_NAME_LEN);

		ret = of_property_read_u32_index(pdev->dev.of_node,
			"key_code", cnt, &key->code);
		if (ret < 0) {
			dev_err(&pdev->dev, "invalid key code index[%d]\n",
				cnt);
			state = -EINVAL;
			goto err;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node,
			"key_chan", cnt, &key->chan);
		if (ret < 0) {
			dev_err(&pdev->dev, "invalid key chan index[%d]\n",
				cnt);
			state = -EINVAL;
			goto err;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node,
			"key_val", cnt, &key->value);
		if (ret < 0) {
			dev_err(&pdev->dev, "invalid key value index[%d]\n",
				cnt);
			state = -EINVAL;
			goto err;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node,
			"key_tolerance", cnt, &key->tolerance);
		if (ret < 0) {
			dev_err(&pdev->dev, "invalid key tolerance index[%d]\n",
				cnt);
			state = -EINVAL;
			goto err;
		}
		spin_lock(&kp->kp_lock);
		set_bit(key->code, kp->input->keybit); /*set event code*/
		list_add_tail(&key->list, &kp->adckey_head);
		spin_unlock(&kp->kp_lock);
	}
	kp_valid_chan_update(kp);
	return 0;
err:
	kfree(key);
	return state;

}

static void kp_list_free(struct kp *kp)
{
	struct adc_key *key;
	struct adc_key *key_tmp;

	spin_lock(&kp->kp_lock);
	list_for_each_entry_safe(key, key_tmp, &kp->adckey_head, list) {
		list_del(&key->list);
		kfree(key);
	}
	spin_unlock(&kp->kp_lock);
}

static int kp_input_dev_register(struct platform_device *pdev, struct kp *kp)
{
	/*alloc input device*/
	kp->input = input_allocate_device();
	if (!kp->input) {
		dev_err(&pdev->dev, "alloc input device failed!\n");
		return -ENOMEM;
	}

	/* init input device */
	set_bit(EV_KEY, kp->input->evbit);
	set_bit(EV_REP, kp->input->evbit);
	kp->input->name = "adc_keypad";
	kp->input->phys = "adc_keypad/input0";
	kp->input->dev.parent = &pdev->dev;

	kp->input->id.bustype = BUS_ISA;
	kp->input->id.vendor = 0x0001;
	kp->input->id.product = 0x0001;
	kp->input->id.version = 0x0100;

	kp->input->rep[REP_DELAY] = 0xffffffff;
	kp->input->rep[REP_PERIOD] = 0xffffffff;

	kp->input->keycodesize = sizeof(unsigned short);
	kp->input->keycodemax = 0x1ff;

	/*register input device*/
	if (input_register_device(kp->input) < 0) {
		dev_err(&pdev->dev,
			 "unable to register keypad input device.\n");
		return -EINVAL;
	}

	return 0;
}

static ssize_t content_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	struct device *dev = kobj_to_dev(kobj->parent);
	struct platform_device *pdev = to_platform_device(dev);
	struct kp *kp = platform_get_drvdata(pdev);
	struct adc_key *key;
	unsigned char key_num = 1;
	int len = 0;

	spin_lock(&kp->kp_lock);
	list_for_each_entry(key, &kp->adckey_head, list) {
		len += sprintf(buf+len,
			"[%d]: name=%-21s code=%-5d channel=%-3d value=%-5d tolerance=%-5d\n",
			key_num,
			key->name,
			key->code,
			key->chan,
			key->value,
			key->tolerance);
		key_num++;
	}
	spin_unlock(&kp->kp_lock);

	return len;
}

static ssize_t content_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	struct device *dev = kobj_to_dev(kobj->parent);
	struct platform_device *pdev = to_platform_device(dev);
	struct kp *kp = platform_get_drvdata(pdev);
	struct adc_key *dkey;
	struct adc_key *key;
	struct adc_key *key_tmp;
	char nbuf[TMP_BUF_MAX];
	char *pbuf = nbuf;
	unsigned char colon_num = 0;
	int nsize = 0;
	int state = 0;
	char *pval;

	/*count inclued '\0'*/
	if (count > TMP_BUF_MAX) {
		dev_err(dev, "write data is too long[max:%d]: %ld\n",
			TMP_BUF_MAX, count);
		return -EINVAL;
	}

	/*trim all invisible characters include '\0', tab, space etc*/
	while (*buf) {
		if (*buf > ' ')
			nbuf[nsize++] = *buf;
		if (*buf == ':')
			colon_num++;
		buf++;
	}
	nbuf[nsize] = '\0';

	/*write "null" or "NULL" to clean up all key table*/
	if (strcasecmp("null", nbuf) == 0) {
		kp_list_free(kp);
		return count;
	}

	/*to judge write data format whether valid or not*/
	if (colon_num != 4) {
		dev_err(dev, "write data invalid: %s\n", nbuf);
		dev_err(dev, "=> [name]:[code]:[channel]:[value]:[tolerance]\n");
		return -EINVAL;
	}

	dkey = kzalloc(sizeof(struct adc_key), GFP_KERNEL);
	if (!dkey) {
		dev_err(&pdev->dev, "alloc mem failed!\n");
		return -ENOMEM;
	}

	/*save the key data in order*/
	pval = strsep(&pbuf, ":"); /*name*/
	if (pval)
		strncpy(dkey->name, pval, MAX_NAME_LEN);

	pval = strsep(&pbuf, ":"); /*code*/
	if (pval)
		if (kstrtoint(pval, 0, &dkey->code) < 0) {
			state = -EINVAL;
			goto err;
		}
	pval = strsep(&pbuf, ":"); /*channel*/
	if (pval)
		if (kstrtoint(pval, 0, &dkey->chan) < 0) {
			state = -EINVAL;
			goto err;
		}
	pval = strsep(&pbuf, ":"); /*value*/
	if (pval)
		if (kstrtoint(pval, 0, &dkey->value) < 0) {
			state = -EINVAL;
			goto err;
		}
	pval = strsep(&pbuf, ":"); /*tolerance*/
	if (pval)
		if (kstrtoint(pval, 0, &dkey->tolerance) < 0) {
			state = -EINVAL;
			goto err;
		}

	/*check channel data whether valid or not*/
	if (dkey->chan >= SARADC_CHAN_NUM) {
		dev_err(dev, "invalid channel[%d-%d]: %d\n", 0,
			SARADC_CHAN_NUM-1, dkey->chan);
		state = -EINVAL;
		goto err;
	}

	/*check sample data whether valid or not*/
	if (dkey->value > SAM_MAX) {
		dev_err(dev, "invalid sample value[%d-%d]: %d\n",
			SAM_MIN, SAM_MAX, dkey->value);
		state = -EINVAL;
		goto err;
	}

	/*check tolerance data whether valid or not*/
	if (dkey->tolerance > TOL_MAX) {
		dev_err(dev, "invalid tolerance[%d-%d]: %d\n",
			TOL_MIN, TOL_MAX, dkey->tolerance);
		state = -EINVAL;
		goto err;
	}

	spin_lock(&kp->kp_lock);
	list_for_each_entry_safe(key, key_tmp, &kp->adckey_head, list) {
		if ((key->code == dkey->code) ||
			((key->chan == dkey->chan) &&
			(key->value == dkey->value))) {
			dev_info(dev, "del older key => %s:%d:%d:%d:%d\n",
				key->name, key->code, key->chan,
				key->value, key->tolerance);
			clear_bit(key->code,  kp->input->keybit);
			list_del(&key->list);
			kfree(key);
		}
	}
	set_bit(dkey->code,  kp->input->keybit);
	list_add_tail(&dkey->list, &kp->adckey_head);
	dev_info(dev, "add newer key => %s:%d:%d:%d:%d\n", dkey->name,
		dkey->code, dkey->chan, dkey->value, dkey->tolerance);
	spin_unlock(&kp->kp_lock);

	kp_valid_chan_update(kp);

	return count;
err:
	kfree(dkey);
	return state;
}

static struct kobj_attribute content_attribute =
	__ATTR(content, 0666, content_show, content_store);

/*
 * Create a group of attributes so that we can create and destroy them all
 * at once.
 */
static struct attribute *attrs[] = {
	&content_attribute.attr,
	NULL,	/* need to NULL terminate the list of attributes */
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};
static int kp_probe(struct platform_device *pdev)
{
	struct kp *kp;
	int state = 0;

	send_data_to_bl301();
	kernel_keypad_enable_mode_enable();

	kp = kzalloc(sizeof(struct kp), GFP_KERNEL);
	if (!kp) {
		dev_err(&pdev->dev, "alloc kp memory failed!\n");
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, kp);
	spin_lock_init(&kp->kp_lock);
	INIT_LIST_HEAD(&kp->adckey_head);
	kp->report_code = 0;
	kp->code = 0;
	kp->poll_period = POLL_PERIOD_WHEN_KEY_UP;
	kp->count = 0;
	INIT_WORK(&(kp->work_update), update_work_func);
	setup_timer(&kp->timer, kp_timer_sr, (unsigned long)kp);
	if (kp_input_dev_register(pdev, kp) < 0) {
		state = -EINVAL;
		goto err;
	}
	if (kp_get_devtree_pdata(pdev, kp) < 0) {
		kp_list_free(kp);
		state = -EINVAL;
		goto err;
	}
	#ifdef CONFIG_HAS_EARLYSUSPEND
	kp->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	kp->early_suspend.suspend = kp_early_suspend;
	kp->early_suspend.resume = kp_late_resume;
	register_early_suspend(&kp->early_suspend);
	#endif
	/*create directory /dev/devices/adc_keypad.34/table*/
	kp->adckey_kobj = kobject_create_and_add("table", &pdev->dev.kobj);
	if  (!kp->adckey_kobj) {
		dev_err(&pdev->dev, "create kobject failed!\n");
		state =  -ENOMEM;
		goto err;
	}
	/* create the files associated with this kobject */
	if (sysfs_create_group(kp->adckey_kobj, &attr_group) < 0) {
		dev_err(&pdev->dev, "create sysfs failed!\n");
		kobject_put(kp->adckey_kobj);
		state = -EINVAL;
		goto err;
	}
	/*enable timer*/
	mod_timer(&kp->timer, jiffies+msecs_to_jiffies(100));
	return 0;
err:
	if (kp->input)
		input_free_device(kp->input);
	kfree(kp);
	return state;
}

static int kp_remove(struct platform_device *pdev)
{
	struct kp *kp = platform_get_drvdata(pdev);

	#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&kp->early_suspend);
	#endif
	del_timer_sync(&kp->timer);
	cancel_work_sync(&kp->work_update);
	input_unregister_device(kp->input);
	input_free_device(kp->input);
	kobject_put(kp->adckey_kobj);
	kp_list_free(kp);
	kfree(kp);
	return 0;
}

static int kp_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int kp_resume(struct platform_device *pdev)
{
	struct kp *kp = platform_get_drvdata(pdev);

	if (get_resume_method() == POWER_KEY_WAKEUP) {
		dev_info(&pdev->dev, "adc keypad wakeup\n");
		input_report_key(kp->input ,  KEY_POWER ,  1);
		input_sync(kp->input);
		input_report_key(kp->input ,  KEY_POWER ,  0);
		input_sync(kp->input);
	}
	return 0;
}

static const struct of_device_id key_dt_match[] = {
	{.compatible = "amlogic, adc_keypad",},
	{},
};

static struct platform_driver kp_driver = {
	.probe      = kp_probe,
	.remove     = kp_remove,
	.suspend    = kp_suspend,
	.resume     = kp_resume,
	.driver     = {
		.name   = DRIVE_NAME,
		.of_match_table = key_dt_match,
	},
};

static int __init kp_init(void)
{
	return platform_driver_register(&kp_driver);
}

static void __exit kp_exit(void)
{
	platform_driver_unregister(&kp_driver);
}

static int __init adc_key_mode_para_setup(char *s)
{
	if (NULL != s)
		sprintf(adc_key_mode_name, "%s", s);

	return 0;
}
__setup("adckeyswitch=", adc_key_mode_para_setup);

static int __init kernel_keypad_enable_setup(char *s)
{
	if (NULL != s)
		sprintf(kernelkey_en_name, "%s", s);

	return 0;
}
__setup("kernelkey_enable=", kernel_keypad_enable_setup);

module_init(kp_init);
module_exit(kp_exit);
MODULE_AUTHOR("Amlogic");
MODULE_DESCRIPTION("ADC Keypad Driver");
MODULE_LICENSE("GPL V2");
