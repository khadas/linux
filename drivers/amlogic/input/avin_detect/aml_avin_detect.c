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
#include "aml_avin_detect.h"

#ifndef CONFIG_OF
#define CONFIG_OF
#endif

#define DEBUG_DEF  0
#define INPUT_REPORT_SWITCH 0

#define AVIN_NAME  "avin_detect"
#define AVIN_NAME_CH1  "avin_detect_ch1"
#define AVIN_NAME_CH2  "avin_detect_ch2"

/*#ifdef USE_INPUT_EVENT_REPORT*/
#define ABS_AVIN_1 0
#define ABS_AVIN_2 1
/*#endif*/

static DECLARE_WAIT_QUEUE_HEAD(avin_waitq);

#define avin_det_info(x...) dev_info(&pdev->dev, x)
#define avin_det_dbg(x...) /* dev_info(&pdev->dev, x) */
#define avin_det_err(x...) dev_err(&pdev->dev, x)

static irqreturn_t avin_detect_handler1(int irq, void *data)
{
	struct avin_det_s *avdev = (struct avin_det_s *)data;
	avdev->irq1_falling_times[avdev->detect_channel1_times]++;
	disable_irq_nosync(avdev->irq_num1);
	return IRQ_HANDLED;
}
static irqreturn_t avin_detect_handler2(int irq, void *data)
{
	struct avin_det_s *avdev = (struct avin_det_s *)data;
	avdev->irq2_falling_times[avdev->detect_channel2_times]++;
	disable_irq_nosync(avdev->irq_num2);
	return IRQ_HANDLED;
}

void avin_timer_sr(unsigned long data)
{
	struct avin_det_s *avdev = (struct avin_det_s *)data;
	if (avdev->first_time_into_loop == 0) {
		avdev->first_time_into_loop = 1;
		enable_irq(avdev->irq_num1);
		enable_irq(avdev->irq_num2);
	} else {
		if (++avdev->detect_channel1_times <= avdev->set_detect_times) {
			if (avdev->irq1_falling_times[
			avdev->detect_channel1_times - 1] == 0) {
				disable_irq_nosync(avdev->irq_num1);
			}

			if (avdev->detect_channel1_times !=
					avdev->set_detect_times) {
				enable_irq(avdev->irq_num1);
			} else {
				avdev->detect_channel1_times = 0;
				avdev->first_time_into_loop = 0;
				schedule_work(&(avdev->work_update1));
			}
		}

		if (++avdev->detect_channel2_times <=
				avdev->set_detect_times) {
			if (avdev->irq2_falling_times[
				avdev->detect_channel2_times - 1] == 0) {
				disable_irq_nosync(avdev->irq_num2);
			}

			if (avdev->detect_channel2_times !=
					avdev->set_detect_times) {
				enable_irq(avdev->irq_num2);
			} else {
				avdev->detect_channel2_times = 0;
				avdev->first_time_into_loop = 0;
				schedule_work(&(avdev->work_update2));
			}
		}
	}

	mod_timer(&avdev->timer,
	jiffies+msecs_to_jiffies(avdev->detect_interval_length));
}

static void kp_work_channel1(struct avin_det_s *avin_data)
{
	int i = 0;
	int num = 0;

	#if 0
	pr_info("av-in1 low times = ");
	for (i = 0; i < avin_data->set_detect_times; i++)
		pr_info("%d ", avin_data->irq1_falling_times[i]);
	pr_info("\n");
	#endif

	for (i = 0; i < avin_data->set_detect_times; i++) {
		if (avin_data->irq1_falling_times[i] == 0)
			num++;
		avin_data->irq1_falling_times[i] = 0;
	}

	if (num >= (avin_data->set_detect_times
		 - avin_data->set_fault_tolerance)) {
		if (avin_data->ch1_current_status != AVIN_STATUS_OUT) {
			avin_data->ch1_current_status = AVIN_STATUS_OUT;
			#if INPUT_REPORT_SWITCH
			input_report_abs(avin_data->input_dev,
			 ABS_AVIN_1, AVIN_STATUS_OUT);
			input_sync(avin_data->input_dev);
			#endif
			avin_data->report_data_s[0].channel = AVIN_CHANNEL1;
			avin_data->report_data_s[0].status = AVIN_STATUS_OUT;
			avin_data->report_data_flag = 1;
			wake_up_interruptible(&avin_waitq);
		}
		#if DEBUG_DEF
		pr_info("avin ch1_current_status out!\n");
		#endif
	} else if (num <= avin_data->set_fault_tolerance) {
		if (avin_data->ch1_current_status != AVIN_STATUS_IN) {
			avin_data->ch1_current_status = AVIN_STATUS_IN;
			#if INPUT_REPORT_SWITCH
			input_report_abs(avin_data->input_dev,
			 ABS_AVIN_1, AVIN_STATUS_IN);
			input_sync(avin_data->input_dev);
			#endif
			avin_data->report_data_s[0].channel = AVIN_CHANNEL1;
			avin_data->report_data_s[0].status = AVIN_STATUS_IN;
			avin_data->report_data_flag = 1;
			wake_up_interruptible(&avin_waitq);
		}
		#if DEBUG_DEF
		pr_info("avin ch1_current_status in!\n");
		#endif
	} else {
		/*keep current status*/
	}
}

static void update_work_func_channel1(struct work_struct *work)
{
	struct avin_det_s *avin_data =
	container_of(work, struct avin_det_s, work_update1);
	kp_work_channel1(avin_data);
}

static void kp_work_channel2(struct avin_det_s *avin_data)
{
	int i = 0;
	int num = 0;

	#if 0
	pr_info("av-in2 low times = ");
	for (i = 0; i < avin_data->set_detect_times; i++)
		pr_info("%d ", avin_data->irq2_falling_times[i]);
	pr_info("\n");
	#endif

	for (i = 0; i < avin_data->set_detect_times; i++) {
		if (avin_data->irq2_falling_times[i] == 0)
			num++;
		avin_data->irq2_falling_times[i] = 0;
	}

	if (num >= (avin_data->set_detect_times
		 - avin_data->set_fault_tolerance)) {
		if (avin_data->ch2_current_status != AVIN_STATUS_OUT) {
			avin_data->ch2_current_status = AVIN_STATUS_OUT;
			#if INPUT_REPORT_SWITCH
			input_report_abs(avin_data->input_dev,
			 ABS_AVIN_2, AVIN_STATUS_OUT);
			input_sync(avin_data->input_dev);
			#endif
			avin_data->report_data_s[1].channel = AVIN_CHANNEL2;
			avin_data->report_data_s[1].status = AVIN_STATUS_OUT;
			avin_data->report_data_flag = 1;
			wake_up_interruptible(&avin_waitq);
		}
		#if DEBUG_DEF
		pr_info("avin ch2_current_status out!\n");
		#endif
	} else if (num <= avin_data->set_fault_tolerance) {
		if (avin_data->ch2_current_status != AVIN_STATUS_IN) {
			avin_data->ch2_current_status = AVIN_STATUS_IN;
			#if INPUT_REPORT_SWITCH
			input_report_abs(avin_data->input_dev,
			 ABS_AVIN_2, AVIN_STATUS_IN);
			input_sync(avin_data->input_dev);
			#endif
			avin_data->report_data_s[1].channel = AVIN_CHANNEL2;
			avin_data->report_data_s[1].status = AVIN_STATUS_IN;
			avin_data->report_data_flag = 1;
			wake_up_interruptible(&avin_waitq);
		}
		#if DEBUG_DEF
		pr_info("avin ch2_current_status in!\n");
		#endif
	} else {
		/*keep current status*/
	}
}

static void update_work_func_channel2(struct work_struct *work)
{
	struct avin_det_s *avin_data =
	container_of(work, struct avin_det_s, work_update2);
	kp_work_channel2(avin_data);
}

static int aml_sysavin_dt_parse(struct platform_device *pdev)
{
	int ret = 0;
	struct pinctrl *p;
	const char *str = "none";
	struct avin_det_s *avdev;

	avdev = platform_get_drvdata(pdev);
	ret = of_property_read_u32(pdev->dev.of_node,
	"detect_interval_length", &(avdev->detect_interval_length));
	if (ret) {
		avin_det_err("Failed to get detect_interval_length.\n");
		goto get_avin_param_failed;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
	"set_detect_times", &(avdev->set_detect_times));
	if (ret) {
		avin_det_err("Failed to get detect_interval_length.\n");
		goto get_avin_param_failed;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
	"set_fault_tolerance", &(avdev->set_fault_tolerance));
	if (ret) {
		avin_det_err("Failed to get detect_interval_length.\n");
		goto get_avin_param_failed;
	}

	p = devm_pinctrl_get_select(&pdev->dev, "avin_gpio_disable_pullup");
	if (IS_ERR(p)) {
		avin_det_err("avin_gpio_disbale_pull init fail, %ld\n",
			PTR_ERR(p));
		return 1;
	}

	of_property_read_string(pdev->dev.of_node, "avin_pin", &str);
	avdev->pin1 = of_get_named_gpiod_flags
	(pdev->dev.of_node, "avin1_pin", 0, NULL);
	avdev->pin2 = of_get_named_gpiod_flags
	(pdev->dev.of_node, "avin2_pin", 0, NULL);

	avdev->irq_num1 = irq_of_parse_and_map(pdev->dev.of_node, 0);
	avdev->irq_num2 = irq_of_parse_and_map(pdev->dev.of_node, 1);
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
	ret = copy_to_user(buf, (void *)(avin_data->report_data_s),
		sizeof(avin_data->report_data_s[0]) * 2);
	avin_data->report_data_flag = 0;
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

	if (avin_data->report_data_flag)
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
	/* request irq */
	irq_ret = request_irq(avdev->irq_num1,
	avin_detect_handler1, IRQF_DISABLED,
	AVIN_NAME_CH1, (void *)avdev);
	if (irq_ret)
		return -EINVAL;

	disable_irq(avdev->irq_num1);

	irq_ret = request_irq(avdev->irq_num2,
	avin_detect_handler2, IRQF_DISABLED, AVIN_NAME_CH2,
	(void *)avdev);
	if (irq_ret) {
		free_irq(avdev->irq_num1, (void *)avdev);
		return -EINVAL;
	}

	disable_irq(avdev->irq_num2);

	/* set timer */
	setup_timer(&avdev->timer, avin_timer_sr, (unsigned long)avdev);
	mod_timer(&avdev->timer, jiffies+msecs_to_jiffies(1000));

	INIT_WORK(&(avdev->work_update1), update_work_func_channel1);
	INIT_WORK(&(avdev->work_update2), update_work_func_channel2);
	return 0;
}

int avin_detect_probe(struct platform_device *pdev)
{
	int ret;
	int state = 0;
	struct avin_det_s *avdev = NULL;

	avdev = kzalloc(sizeof(struct avin_det_s), GFP_KERNEL);
	if (!avdev) {
		avin_det_err("kzalloc error\n");
		state = -ENOMEM;
		goto get_param_mem_fail;
	}
	platform_set_drvdata(pdev, avdev);

	avdev->ch1_current_status = AVIN_STATUS_UNKNOW;
	avdev->ch2_current_status = AVIN_STATUS_UNKNOW;

	ret = aml_sysavin_dt_parse(pdev);
	if (ret) {
		state = ret;
		goto get_dts_dat_fail;
	}

	/* init */
	avdev->irq1_falling_times =
	kzalloc((sizeof(unsigned int) * avdev->set_detect_times), GFP_KERNEL);
	if (!avdev->irq1_falling_times) {
		state = -ENOMEM;
		goto get_param_mem_fail_1;
	}
	avdev->irq2_falling_times =
	 kzalloc((sizeof(unsigned int) * avdev->set_detect_times), GFP_KERNEL);
	if (!avdev->irq2_falling_times) {
		state = -ENOMEM;
		goto get_param_mem_fail_1;
	}

	/* request irq */
	gpio_for_irq(desc_to_gpio(avdev->pin1),
	AML_GPIO_IRQ((avdev->irq_num1 - INT_GPIO_0),
	 FILTER_NUM7, GPIO_IRQ_FALLING));

	gpio_for_irq(desc_to_gpio(avdev->pin2),
	 AML_GPIO_IRQ((avdev->irq_num2 - INT_GPIO_0),
	  FILTER_NUM7, GPIO_IRQ_FALLING));

	ret = init_resource(avdev);
	if (ret < 0) {
		avin_det_err("Unable to init iqr resource.\n");
		state = -EINVAL;
		goto irq_request_fail;
	}

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
		avin_det_err("Unable to register avin input device.\n");
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
	kfree(avdev->irq1_falling_times);
	kfree(avdev->irq2_falling_times);
get_param_mem_fail_1:
get_dts_dat_fail:
	kfree(avdev);
get_param_mem_fail:
	return state;
}

static int avin_detect_suspend(struct platform_device *pdev ,
	pm_message_t state)
{
	struct avin_det_s *avdev = platform_get_drvdata(pdev);
	avdev->first_time_into_loop = 0;
	del_timer_sync(&avdev->timer);
	cancel_work_sync(&avdev->work_update1);
	cancel_work_sync(&avdev->work_update2);
	free_irq(avdev->irq_num1, (void *)avdev);
	free_irq(avdev->irq_num2, (void *)avdev);
	avin_det_info("avin_detect_suspend ok.\n");
	return 0;
}

static int avin_detect_resume(struct platform_device *pdev)
{
	struct avin_det_s *avdev = platform_get_drvdata(pdev);
	init_resource(avdev);
	avin_det_info("avin_detect_resume ok.\n");
	return 0;
}


int avin_detect_remove(struct platform_device *pdev)
{
	struct avin_det_s *avdev = platform_get_drvdata(pdev);
	input_unregister_device(avdev->input_dev);
	input_free_device(avdev->input_dev);
	cdev_del(&avdev->avin_cdev);
	del_timer_sync(&avdev->timer);
	cancel_work_sync(&avdev->work_update1);
	cancel_work_sync(&avdev->work_update2);
	free_irq(avdev->irq_num1, (void *)avdev);
	free_irq(avdev->irq_num2, (void *)avdev);
	gpio_free(desc_to_gpio(avdev->pin1));
	gpio_free(desc_to_gpio(avdev->pin2));
	kfree(avdev->irq1_falling_times);
	kfree(avdev->irq2_falling_times);
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
