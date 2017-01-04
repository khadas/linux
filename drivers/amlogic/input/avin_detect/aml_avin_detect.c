#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/poll.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <uapi/linux/input.h>
#include <linux/of.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include "aml_avin_detect.h"
#include <linux/gpio.h>

#ifndef CONFIG_OF
#define CONFIG_OF
#endif

#undef pr_fmt
#define pr_fmt(fmt)    "avin-detect: " fmt

#define DEBUG_DEF  1
#define INPUT_REPORT_SWITCH 0
#define LOOP_DETECT_TIMES 3

#define MAX_AVIN_DEVICE_NUM  3
#define AVIN_NAME  "avin_detect"
#define AVIN_NAME_CH1  "avin_detect_ch1"
#define AVIN_NAME_CH2  "avin_detect_ch2"
#define AVIN_NAME_CH3  "avin_detect_ch3"
#define ABS_AVIN_1 0
#define ABS_AVIN_2 1
#define ABS_AVIN_3 2

static char *avin_name_ch[3] = {AVIN_NAME_CH1, AVIN_NAME_CH2, AVIN_NAME_CH3};
static char avin_ch[3] = {AVIN_CHANNEL1, AVIN_CHANNEL2, AVIN_CHANNEL3};

static DECLARE_WAIT_QUEUE_HEAD(avin_waitq);

#if 0
#define MASK_AVIRQ(i)\
	gpiod_mask_irq(avdev->hw_res.pin[i],\
	AML_GPIO_IRQ((avdev->hw_res.irq_num[i] - INT_GPIO_0),\
	FILTER_NUM7, GPIO_IRQ_FALLING))

#define ENABLE_AVIRQ(i)\
	gpiod_for_irq(avdev->hw_res.pin[i],\
	AML_GPIO_IRQ((avdev->hw_res.irq_num[i] - INT_GPIO_0),\
	FILTER_NUM7, GPIO_IRQ_FALLING))
#endif

static void MASK_AVIRQ(int i, struct avin_det_s *avdev)
{
	gpiod_mask_irq(avdev->hw_res.pin[i],
		AML_GPIO_IRQ((avdev->hw_res.irq_num[i] - INT_GPIO_0),
		FILTER_NUM7, GPIO_IRQ_FALLING));
}

static void ENABLE_AVIRQ(int i, struct avin_det_s *avdev)
{
	gpiod_for_irq(avdev->hw_res.pin[i],
		AML_GPIO_IRQ((avdev->hw_res.irq_num[i] - INT_GPIO_0),
		FILTER_NUM7, GPIO_IRQ_FALLING));
}

static irqreturn_t avin_detect_handler(int irq, void *data)
{
	int i;
	struct avin_det_s *avdev = (struct avin_det_s *)data;

	for (i = 0; i <= avdev->dts_param.dts_device_num; i++) {
		if (irq == avdev->hw_res.irq_num[i])
			break;
		else if (i == avdev->dts_param.dts_device_num)
			return IRQ_HANDLED;
	}

	if (avdev->code_variable.loop_detect_times[i]++
		== LOOP_DETECT_TIMES) {
		avdev->code_variable.irq_falling_times[
			i * avdev->dts_param.dts_detect_times +
			avdev->code_variable.detect_channel_times[i]]++;
		avdev->code_variable.pin_mask_irq_flag[i] = 1;
		/*avdev->code_variable.loop_detect_times[i] = 0;*/
		schedule_work(&(avdev->work_struct_maskirq));
	}
	return IRQ_HANDLED;
}

/* must open irq >100ms later,then into timer handler */
static void avin_timer_sr(unsigned long data)
{
	int i;
	struct avin_det_s *avdev = (struct avin_det_s *)data;
	for (i = 0; i < avdev->dts_param.dts_device_num; i++) {
		if (avdev->code_variable.detect_channel_times[i] <
			(avdev->dts_param.dts_detect_times-1)) {
			avdev->code_variable.detect_channel_times[i]++;
			if (avdev->code_variable.irq_falling_times[
				i * avdev->dts_param.dts_detect_times +
				avdev->code_variable.detect_channel_times[
					i]-1] != 0) {
				avdev->code_variable.loop_detect_times[i] = 0;
				/*ENABLE_AVIRQ(i, avdev);*/
			}
			ENABLE_AVIRQ(i, avdev);
			if (avdev->code_variable.detect_channel_times[
				i] == 1) {
				avdev->code_variable.irq_falling_times[
				(i+1) * avdev->dts_param.dts_detect_times
				- 1] = 0;
			} else if (avdev->code_variable.detect_channel_times[i]
			== (avdev->dts_param.dts_detect_times-1)) {
				schedule_work(&(avdev->work_struct_update));
			}
		} else {
			avdev->code_variable.detect_channel_times[i] = 0;
			avdev->code_variable.loop_detect_times[i] = 0;
			/*if (avdev->code_variable.irq_falling_times[
				(i+1) * avdev->dts_param.dts_detect_times
				-1] != 0)*/
				ENABLE_AVIRQ(i, avdev);
		}
	}
	mod_timer(&avdev->timer,
	jiffies+msecs_to_jiffies(avdev->dts_param.dts_interval_length));
}

static void kp_work_channel1(struct avin_det_s *avdev)
{
	int i, j;
	mutex_lock(&avdev->lock);
	for (i = 0; i < avdev->dts_param.dts_device_num; i++) {
		for (j = 0; j < (avdev->dts_param.dts_detect_times-1); j++) {
			if (avdev->code_variable.irq_falling_times[
				i * avdev->dts_param.dts_detect_times + j] == 0)
				avdev->code_variable.actual_into_irq_times[i]++;

			avdev->code_variable.irq_falling_times[
				i * avdev->dts_param.dts_detect_times + j] = 0;
		}

		if (avdev->code_variable.actual_into_irq_times[i] >=
			((avdev->dts_param.dts_detect_times - 1)
			- avdev->dts_param.dts_fault_tolerance)) {
			if (avdev->code_variable.ch_current_status[i]
				!= AVIN_STATUS_OUT) {
				avdev->code_variable.ch_current_status[i]
					= AVIN_STATUS_OUT;
				#if INPUT_REPORT_SWITCH
				input_report_abs(avdev->input_dev,
				 ABS_AVIN_1, AVIN_STATUS_OUT);
				input_sync(avdev->input_dev);
				#endif
				avdev->code_variable.report_data_s[i].channel
				= avin_ch[i];
				avdev->code_variable.report_data_s[i].status
					= AVIN_STATUS_OUT;
				avdev->code_variable.report_data_flag = 1;
				wake_up_interruptible(&avin_waitq);
				#if DEBUG_DEF
				pr_info("avin ch%d current_status out!\n", i);
				#endif
			}
		} else if (avdev->code_variable.actual_into_irq_times[i] <=
			avdev->dts_param.dts_fault_tolerance) {
			if (avdev->code_variable.ch_current_status[i]
				!= AVIN_STATUS_IN) {
				avdev->code_variable.ch_current_status[i]
					= AVIN_STATUS_IN;
				#if INPUT_REPORT_SWITCH
				input_report_abs(avdev->input_dev,
				 ABS_AVIN_1, AVIN_STATUS_IN);
				input_sync(avdev->input_dev);
				#endif
				avdev->code_variable.report_data_s[i].channel
				= avin_ch[i];
				avdev->code_variable.report_data_s[i].status
					= AVIN_STATUS_IN;
				avdev->code_variable.report_data_flag = 1;
				wake_up_interruptible(&avin_waitq);
				#if DEBUG_DEF
				pr_info("avin ch%d current_status in!\n", i);
				#endif
			}
		} else {
			/*keep current status*/
		}
	}
	memset(avdev->code_variable.actual_into_irq_times, 0,
		sizeof(avdev->code_variable.actual_into_irq_times[0]) *
		avdev->dts_param.dts_device_num);

	mutex_unlock(&avdev->lock);
}

static void update_work_update_status(struct work_struct *work)
{
	struct avin_det_s *avin_data =
	container_of(work, struct avin_det_s, work_struct_update);
	kp_work_channel1(avin_data);
}

static void update_work_maskirq(struct work_struct *work)
{
	int i;
	struct avin_det_s *avdev =
	container_of(work, struct avin_det_s, work_struct_maskirq);
	for (i = 0; i < avdev->dts_param.dts_device_num; i++) {
		if (avdev->code_variable.pin_mask_irq_flag[i] == 1) {
			MASK_AVIRQ(i, avdev);
			avdev->code_variable.pin_mask_irq_flag[i] = 0;
		}
	}
}

static int aml_sysavin_dts_parse(struct platform_device *pdev)
{
	int ret;
	int i;
	int state;
	int value;
	struct pinctrl *p;
	struct avin_det_s *avdev;

	avdev = platform_get_drvdata(pdev);

	ret = of_property_read_u32(pdev->dev.of_node,
	"avin_device_num", &value);
	avdev->dts_param.dts_device_num = value;
	if (ret) {
		pr_info("Failed to get dts_device_num.\n");
		goto get_avin_param_failed;
	} else {
		if (avdev->dts_param.dts_device_num == 0) {
			pr_info("avin device num is 0\n");
			goto get_avin_param_failed;
		} else if (avdev->dts_param.dts_device_num >
		MAX_AVIN_DEVICE_NUM) {
			pr_info("avin device num is > MAX NUM\n");
			goto get_avin_param_failed;
		}
	}

	ret = of_property_read_u32(pdev->dev.of_node,
	"detect_interval_length", &value);
	if (ret) {
		pr_info("Failed to get dts_interval_length.\n");
		goto get_avin_param_failed;
	}
	avdev->dts_param.dts_interval_length = value;

	ret = of_property_read_u32(pdev->dev.of_node,
	"set_detect_times", &value);
	if (ret) {
		pr_info("Failed to get dts_detect_times.\n");
		goto get_avin_param_failed;
	}
	avdev->dts_param.dts_detect_times = value + 1;

	ret = of_property_read_u32(pdev->dev.of_node,
	"set_fault_tolerance", &value);
	if (ret) {
		pr_info("Failed to get dts_fault_tolerance.\n");
		goto get_avin_param_failed;
	}
	avdev->dts_param.dts_fault_tolerance = value;

	p = devm_pinctrl_get_select(&pdev->dev,
		"avin_gpio_disable_pullup");
	if (IS_ERR(p)) {
		pr_info("avin_gpio_disbale_pull init fail, %ld\n",
		PTR_ERR(p));
		return 1;
	}

	/* request resource of pin */
	avdev->hw_res.pin =
		kzalloc((sizeof(struct gpio_desc *)
		* avdev->dts_param.dts_device_num), GFP_KERNEL);
	if (!avdev->hw_res.pin) {
		state = -ENOMEM;
		goto get_avin_param_failed;
	}
	for (i = 0; i < avdev->dts_param.dts_device_num; i++) {
		avdev->hw_res.pin[i] = of_get_named_gpiod_flags
		(pdev->dev.of_node, "avin_det_pin", i, NULL);
	}

	/* request resource of irq num */
	avdev->hw_res.irq_num =
		kzalloc((sizeof(avdev->hw_res.irq_num[0])
		* avdev->dts_param.dts_device_num), GFP_KERNEL);
	if (!avdev->hw_res.irq_num) {
		state = -ENOMEM;
		goto get_avin_param_failed;
	}

	avdev->hw_res.irq_num[0] =
		irq_of_parse_and_map(pdev->dev.of_node, 0);
	for (i = 0; i < avdev->dts_param.dts_device_num; i++)
		avdev->hw_res.irq_num[i] =
		irq_of_parse_and_map(pdev->dev.of_node, i);

	return 0;
get_avin_param_failed:
	return -EINVAL;
}

static int avin_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct avin_det_s *avindev;
	avindev = container_of(inode->i_cdev, struct avin_det_s, avin_cdev);
	file->private_data = avindev;
	return ret;
}

static ssize_t avin_read(struct file *file, char __user *buf,
	size_t count, loff_t *ppos)
{
	unsigned long ret;
	struct avin_det_s *avin_data = (struct avin_det_s *)file->private_data;

	/*wait_event_interruptible(avin_waitq, avin_data->report_data_flag);*/
	ret = copy_to_user(buf,
		(void *)(avin_data->code_variable.report_data_s),
		sizeof(avin_data->code_variable.report_data_s[0])
		* avin_data->dts_param.dts_device_num);
	avin_data->code_variable.report_data_flag = 0;
	return 0;
}

static int avin_config_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static unsigned avin_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;
	struct avin_det_s *avin_data = (struct avin_det_s *)file->private_data;
	poll_wait(file, &avin_waitq, wait);

	if (avin_data->code_variable.report_data_flag)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

static const struct file_operations avin_fops = {
	.owner      = THIS_MODULE,
	.open       = avin_open,
	.read       = avin_read,
	.poll       = avin_poll,
	.release    = avin_config_release,
};

static int register_avin_dev(struct avin_det_s *avin_data)
{
	int ret = 0;

	ret = alloc_chrdev_region(&avin_data->avin_devno,
		0, 1, "avin_detect_region");
	if (ret < 0) {
		pr_err("avin: failed to allocate major number\n");
		return -ENODEV;
	}

	/* connect the file operations with cdev */
	cdev_init(&avin_data->avin_cdev, &avin_fops);
	avin_data->avin_cdev.owner = THIS_MODULE;
	/* connect the major/minor number to the cdev */
	ret = cdev_add(&avin_data->avin_cdev, avin_data->avin_devno, 1);
	if (ret) {
		pr_err("avin: failed to add device\n");
		return -ENODEV;
	}

	strcpy(avin_data->config_name, "avin_detect");
	avin_data->config_class = class_create(THIS_MODULE,
		avin_data->config_name);
	avin_data->config_dev = device_create(avin_data->config_class, NULL,
		avin_data->avin_devno, NULL, avin_data->config_name);
	if (IS_ERR(avin_data->config_dev)) {
		pr_err("avin: failed to create device node\n");
		ret = PTR_ERR(avin_data->config_dev);
		return ret;
	}

	return ret;
}

static int init_resource(struct avin_det_s *avdev)
{
	int irq_ret;
	int i, j;
	INIT_WORK(&(avdev->work_struct_update),  update_work_update_status);
	INIT_WORK(&(avdev->work_struct_maskirq), update_work_maskirq);
	for (i = 0; i < avdev->dts_param.dts_device_num; i++) {
		for (j = 0; j < avdev->dts_param.dts_detect_times; j++)
			avdev->code_variable.irq_falling_times[
			i * avdev->dts_param.dts_detect_times + j] = 0;

		avdev->code_variable.loop_detect_times[i] = 0;
	}

	/* set timer */
	setup_timer(&avdev->timer, avin_timer_sr, (unsigned long)avdev);
	mod_timer(&avdev->timer, jiffies+msecs_to_jiffies(2000));

	/* request irq num*/
	for (i = 0; i < avdev->dts_param.dts_device_num; i++) {
		irq_ret = request_irq(avdev->hw_res.irq_num[i],
		avin_detect_handler, IRQF_DISABLED,
		avin_name_ch[i], (void *)avdev);
		if (irq_ret)
			return -EINVAL;
	}
	return 0;
}

static int request_mem_resource(struct platform_device *pdev)
{
	int i;
	int state;
	struct avin_det_s *avdev;
	avdev = platform_get_drvdata(pdev);

	avdev->code_variable.pin_mask_irq_flag =
		kzalloc((sizeof(avdev->code_variable.pin_mask_irq_flag[0]) *
		avdev->dts_param.dts_device_num), GFP_KERNEL);
	if (!avdev->code_variable.pin_mask_irq_flag) {
		state = -ENOMEM;
		goto request_mem_failed;
	}

	avdev->code_variable.loop_detect_times =
		kzalloc((sizeof(avdev->code_variable.loop_detect_times[0]) *
		avdev->dts_param.dts_device_num), GFP_KERNEL);
	if (!avdev->code_variable.loop_detect_times) {
		state = -ENOMEM;
		goto request_mem_failed;
	}

	avdev->code_variable.detect_channel_times =
		kzalloc((sizeof(avdev->code_variable.detect_channel_times[0]) *
		avdev->dts_param.dts_device_num), GFP_KERNEL);
	if (!avdev->code_variable.detect_channel_times) {
		state = -ENOMEM;
		goto request_mem_failed;
	}

	avdev->code_variable.report_data_s =
		kzalloc((sizeof(avdev->code_variable.report_data_s[0]) *
		avdev->dts_param.dts_device_num), GFP_KERNEL);
	if (!avdev->code_variable.report_data_s) {
		state = -ENOMEM;
		goto request_mem_failed;
	}

	avdev->code_variable.irq_falling_times =
		kzalloc((sizeof(avdev->code_variable.irq_falling_times[0]) *
		avdev->dts_param.dts_device_num
		* (avdev->dts_param.dts_detect_times)), GFP_KERNEL);
	if (!avdev->code_variable.irq_falling_times) {
		state = -ENOMEM;
		goto request_mem_failed;
	}

	avdev->code_variable.actual_into_irq_times =
		kzalloc((sizeof(avdev->code_variable.actual_into_irq_times[0]) *
		avdev->dts_param.dts_device_num), GFP_KERNEL);
	if (!avdev->code_variable.actual_into_irq_times) {
		state = -ENOMEM;
		goto request_mem_failed;
	}

	avdev->code_variable.ch_current_status =
		kzalloc((sizeof(avdev->code_variable.ch_current_status[0]) *
		avdev->dts_param.dts_device_num), GFP_KERNEL);
	if (!avdev->code_variable.ch_current_status) {
		state = -ENOMEM;
		goto request_mem_failed;
	}
	for (i = 0; i < avdev->dts_param.dts_device_num; i++)
		avdev->code_variable.ch_current_status[i] = AVIN_STATUS_UNKNOW;

	return 0;
request_mem_failed:
	return -EINVAL;
}

int avin_detect_probe(struct platform_device *pdev)
{
	int i;
	int ret;
	int state = 0;
	struct avin_det_s *avdev = NULL;
	avdev = kzalloc(sizeof(struct avin_det_s), GFP_KERNEL);
	if (!avdev) {
		pr_info("kzalloc error\n");
		state = -ENOMEM;
		goto get_param_mem_fail;
	}

	platform_set_drvdata(pdev, avdev);

	ret = aml_sysavin_dts_parse(pdev);
	if (ret) {
		state = ret;
		goto get_dts_dat_fail;
	}

	ret = request_mem_resource(pdev);
	if (ret) {
		state = ret;
		goto get_param_mem_fail_1;
	}

	/* request irq: gpio for irq */
	for (i = 0; i < avdev->dts_param.dts_device_num; i++)
		ENABLE_AVIRQ(i, avdev);

	ret = init_resource(avdev);
	if (ret < 0) {
		pr_info("Unable to init irq resource.\n");
		state = -EINVAL;
		goto irq_request_fail;
	}

	mutex_init(&avdev->lock);

	/* register input device */
	avdev->input_dev = input_allocate_device();
	if (avdev->input_dev == 0) {
		state = -ENOMEM;
		goto allocate_input_fail;
	}
	set_bit(EV_ABS, avdev->input_dev->evbit);
	input_set_abs_params(avdev->input_dev,
	 ABS_AVIN_1, 0, 2, 0, 0);
	input_set_abs_params(avdev->input_dev,
	 ABS_AVIN_2, 0, 2, 0, 0);
	avdev->input_dev->name = AVIN_NAME;
    /*avdev->input_dev->phys = "gpio_keypad/input0";*/
	avdev->input_dev->dev.parent = &pdev->dev;
	avdev->input_dev->id.bustype = BUS_ISA;
	avdev->input_dev->id.vendor  = 0x5f5f;
	avdev->input_dev->id.product = 0x6f6f;
	avdev->input_dev->id.version = 0x7f7f;

	ret = input_register_device(avdev->input_dev);
	if (ret < 0) {
		pr_info("Unable to register avin input device.\n");
		state = -EINVAL;
		goto register_input_fail;
	}

	/* register char device */
	ret = register_avin_dev(avdev);
	return 0;
register_input_fail:
	input_free_device(avdev->input_dev);
allocate_input_fail:
irq_request_fail:
	kfree(avdev->code_variable.actual_into_irq_times);
	kfree(avdev->code_variable.ch_current_status);
	kfree(avdev->code_variable.report_data_s);
	kfree(avdev->code_variable.detect_channel_times);
	kfree(avdev->code_variable.irq_falling_times);
	kfree(avdev->code_variable.pin_mask_irq_flag);
	kfree(avdev->code_variable.loop_detect_times);
get_param_mem_fail_1:
	kfree(avdev->hw_res.pin);
	kfree(avdev->hw_res.irq_num);
get_dts_dat_fail:
	kfree(avdev);
get_param_mem_fail:
	return state;
}

static int avin_detect_suspend(struct platform_device *pdev ,
	pm_message_t state)
{
	int i;
	struct avin_det_s *avdev = platform_get_drvdata(pdev);
	del_timer_sync(&avdev->timer);
	cancel_work_sync(&avdev->work_struct_update);
	cancel_work_sync(&avdev->work_struct_maskirq);
	for (i = 0; i < avdev->dts_param.dts_device_num; i++) {
		free_irq(avdev->hw_res.irq_num[i], (void *)avdev);
		avdev->code_variable.irq_falling_times[i] = 0;
		avdev->code_variable.detect_channel_times[i] = 0;
		avdev->code_variable.loop_detect_times[i] = 0;
	}
	pr_info("avin_detect_suspend ok.\n");
	return 0;
}

static int avin_detect_resume(struct platform_device *pdev)
{
	int i;
	struct avin_det_s *avdev = platform_get_drvdata(pdev);
	for (i = 0; i < avdev->dts_param.dts_device_num; i++)
		ENABLE_AVIRQ(i, avdev);
	init_resource(avdev);
	pr_info("avin_detect_resume ok.\n");
	return 0;
}

int avin_detect_remove(struct platform_device *pdev)
{
	int i;
	struct avin_det_s *avdev = platform_get_drvdata(pdev);
	input_unregister_device(avdev->input_dev);
	input_free_device(avdev->input_dev);
	cdev_del(&avdev->avin_cdev);
	del_timer_sync(&avdev->timer);
	cancel_work_sync(&avdev->work_struct_update);
	cancel_work_sync(&avdev->work_struct_maskirq);
	for (i = 0; i < avdev->dts_param.dts_device_num; i++) {
		free_irq(avdev->hw_res.irq_num[i], (void *)avdev);
		gpio_free(desc_to_gpio(avdev->hw_res.pin[i]));
	}
	kfree(avdev->code_variable.actual_into_irq_times);
	kfree(avdev->code_variable.ch_current_status);
	kfree(avdev->code_variable.report_data_s);
	kfree(avdev->code_variable.detect_channel_times);
	kfree(avdev->code_variable.irq_falling_times);
	kfree(avdev->code_variable.pin_mask_irq_flag);
	kfree(avdev->code_variable.loop_detect_times);
	kfree(avdev->hw_res.pin);
	kfree(avdev->hw_res.irq_num);
	kfree(avdev);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id avin_dt_match[] = {
	{	.compatible = "amlogic, avin_detect",
	},
	{},
};
#else
#define avin_dt_match NULL
#endif

static struct platform_driver avin_driver = {
	.probe      = avin_detect_probe,
	.remove     = avin_detect_remove,
	.suspend    = avin_detect_suspend,
	.resume     = avin_detect_resume,
	.driver     = {
		.name   = "avin_detect",
		.of_match_table = avin_dt_match,
	},
};

static int __init avin_detect_init(void)
{
	return platform_driver_register(&avin_driver);
}

static void __exit avin_detect_exit(void)
{
	platform_driver_unregister(&avin_driver);
}

module_init(avin_detect_init);
module_exit(avin_detect_exit);

MODULE_DESCRIPTION("Meson AVIN Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");
