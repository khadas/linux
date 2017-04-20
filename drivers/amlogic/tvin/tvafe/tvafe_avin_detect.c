/*
 * tvaf avin detect char device driver.
 *
 * Copyright (c) 2017
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

/* Standard Linux headers */
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include <linux/irq.h>
#include <linux/poll.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include "tvafe_avin_detect.h"
#include "../tvin_global.h"


#define TVAFE_AVIN_CH1_MASK  (1 << 0)
#define TVAFE_AVIN_CH2_MASK  (2 << 0)
#define TVAFE_AVIN_MASK  (TVAFE_AVIN_CH1_MASK | TVAFE_AVIN_CH2_MASK)
#define TVAFE_MAX_AVIN_DEVICE_NUM  2
#define TVAFE_AVIN_NAME  "tvafe_avin_detect"
#define TVAFE_AVIN_NAME_CH1  "tvafe_avin_detect_ch1"
#define TVAFE_AVIN_NAME_CH2  "tvafe_avin_detect_ch2"

/*0:use internal VDC to bias CVBS_in
*1:use ground to bias CVBS_in*/
static unsigned int detect_mode;
module_param(detect_mode, int, 0664);
MODULE_PARM_DESC(detect_mode, "detect_mode");

/*0:460mv; 1:0.225mv*/
static unsigned int vdc_level;
module_param(vdc_level, int, 0664);
MODULE_PARM_DESC(vdc_level, "vdc_level");

/*0:50mv; 1:100mv; 2:150mv; 3:200mv; 4:250mv; 6:300mv; 7:310mv*/
static unsigned int sync_level;
module_param(sync_level, int, 0664);
MODULE_PARM_DESC(sync_level, "sync_level");

static unsigned int avplay_sync_level = 4;
module_param(avplay_sync_level, int, 0664);
MODULE_PARM_DESC(avplay_sync_level, "avplay_sync_level");

/*0:50mv; 1:100mv; 2:150mv; 3:200mv*/
static unsigned int sync_hys_adj = 1;
module_param(sync_hys_adj, int, 0664);
MODULE_PARM_DESC(sync_hys_adj, "sync_hys_adj");

static unsigned int irq_mode = 5;
module_param(irq_mode, int, 0664);
MODULE_PARM_DESC(irq_mode, "irq_mode");

static unsigned int trigger_sel = 1;
module_param(trigger_sel, int, 0664);
MODULE_PARM_DESC(trigger_sel, "trigger_sel");

static unsigned int irq_edge_en = 1;
module_param(irq_edge_en, int, 0664);
MODULE_PARM_DESC(irq_edge_en, "irq_edge_en");

static unsigned int irq_filter;
module_param(irq_filter, int, 0664);
MODULE_PARM_DESC(irq_filter, "irq_filter");

static unsigned int irq_pol;
module_param(irq_pol, int, 0664);
MODULE_PARM_DESC(irq_pol, "irq_pol");

static char *tvafe_avin_name_ch[2] = {TVAFE_AVIN_NAME_CH1, TVAFE_AVIN_NAME_CH2};

static DECLARE_WAIT_QUEUE_HEAD(tvafe_avin_waitq);

static irqreturn_t tvafe_avin_detect_handler(int irq, void *data)
{
	struct tvafe_avin_det_s *avdev = (struct tvafe_avin_det_s *)data;

	if (avdev->dts_param.device_mask == TVAFE_AVIN_CH1_MASK)
		avdev->irq_counter[0] = aml_read_cbus(CVBS_IRQ0_COUNTER);
	else if (avdev->dts_param.device_mask == TVAFE_AVIN_CH2_MASK)
		avdev->irq_counter[0] = aml_read_cbus(CVBS_IRQ1_COUNTER);
	else if (avdev->dts_param.device_mask == TVAFE_AVIN_MASK) {
		avdev->irq_counter[0] = aml_read_cbus(CVBS_IRQ0_COUNTER);
		avdev->irq_counter[1] = aml_read_cbus(CVBS_IRQ1_COUNTER);
	}
	return IRQ_HANDLED;
}

static void kp_work_channel1(struct tvafe_avin_det_s *avdev)
{
	unsigned int state_changed, irq_counter_threshold;
	mutex_lock(&avdev->lock);

	switch (irq_mode) {
	case 0:
		irq_counter_threshold = 1;
		break;
	case 1:
		irq_counter_threshold = 1 << 1;
		break;
	case 2:
		irq_counter_threshold = 1 << 3;
		break;
	case 3:
		irq_counter_threshold = 1 << 5;
		break;
	case 4:
		irq_counter_threshold = 1 << 7;
		break;
	case 5:
		irq_counter_threshold = 1 << 10;
		break;
	case 6:
		irq_counter_threshold = 1 << 12;
		break;
	case 7:
		irq_counter_threshold = 1 << 15;
		break;
	default:
		irq_counter_threshold = 1;
		break;

	}
	if ((avdev->device_num >= 1) &&
		(avdev->irq_counter[0] >= irq_counter_threshold)) {
		if (avdev->report_data_s[0].status != TVAFE_AVIN_STATUS_IN) {
			avdev->report_data_s[0].status = TVAFE_AVIN_STATUS_IN;
			state_changed = 1;
		} else if (avdev->report_data_s[0].status !=
			TVAFE_AVIN_STATUS_OUT){
			avdev->report_data_s[0].status = TVAFE_AVIN_STATUS_OUT;
			state_changed = 1;
		}
	}
	if ((avdev->device_num == 2) &&
		(avdev->irq_counter[1] >= irq_counter_threshold)) {
		if (avdev->report_data_s[1].status != TVAFE_AVIN_STATUS_IN) {
			avdev->report_data_s[1].status = TVAFE_AVIN_STATUS_IN;
			state_changed = 1;
		} else if (avdev->report_data_s[1].status !=
			TVAFE_AVIN_STATUS_OUT){
			avdev->report_data_s[1].status = TVAFE_AVIN_STATUS_OUT;
			state_changed = 1;
		}
	}
	if (state_changed)
		wake_up_interruptible(&tvafe_avin_waitq);
	state_changed = 0;
	mutex_unlock(&avdev->lock);
}

static void tvafe_update_work_update_status(struct work_struct *work)
{
	struct tvafe_avin_det_s *avin_data =
	container_of(work, struct tvafe_avin_det_s, work_struct_update);
	kp_work_channel1(avin_data);
}

static int tvafe_avin_dts_parse(struct platform_device *pdev)
{
	int ret;
	int i;
	int value;
	struct resource *res;
	struct tvafe_avin_det_s *avdev;

	avdev = platform_get_drvdata(pdev);

	ret = of_property_read_u32(pdev->dev.of_node,
		"device_mask", &value);
	if (ret) {
		pr_info("Failed to get device_mask.\n");
		goto get_avin_param_failed;
	} else {
		avdev->dts_param.device_mask = value;
		if (avdev->dts_param.device_mask == TVAFE_AVIN_MASK) {
			avdev->device_num = 2;
			avdev->report_data_s[0].channel = TVAFE_AVIN_CHANNEL1;
			avdev->report_data_s[1].channel = TVAFE_AVIN_CHANNEL2;
		} else if (avdev->dts_param.device_mask ==
			TVAFE_AVIN_CH1_MASK) {
			avdev->device_num = 1;
			avdev->report_data_s[0].channel = TVAFE_AVIN_CHANNEL1;
		} else if (avdev->dts_param.device_mask
			== TVAFE_AVIN_CH2_MASK) {
			avdev->device_num = 1;
			avdev->report_data_s[0].channel = TVAFE_AVIN_CHANNEL2;
		} else {
			avdev->device_num = 0;
			pr_info("avin device mask is %x\n",
				avdev->dts_param.device_mask);
			goto get_avin_param_failed;
		}
		avdev->report_data_s[0].status = TVAFE_AVIN_STATUS_UNKNOW;
		avdev->report_data_s[1].status = TVAFE_AVIN_STATUS_UNKNOW;
	}
	/* get irq no*/
	for (i = 0; i < avdev->device_num; i++) {
		res = platform_get_resource(pdev, IORESOURCE_IRQ, i);
		if (!res) {
			pr_err("%s: can't get avin(%d) irq resource\n",
				__func__, i);
			ret = -ENXIO;
			goto fail_get_resource_irq;
		}
		avdev->dts_param.irq[i] = res->start;
	}
	return 0;
get_avin_param_failed:
fail_get_resource_irq:
	return -EINVAL;
}

void tvafe_avin_detect_ch1_anlog_enable(bool enable)
{
	if (enable)
		W_HIU_BIT(HHI_CVBS_DETECT_CNTL, 1,
			AFE_CH1_EN_DETECT_BIT, AFE_CH1_EN_DETECT_WIDTH);
	else
		W_HIU_BIT(HHI_CVBS_DETECT_CNTL, 0,
			AFE_CH1_EN_DETECT_BIT, AFE_CH1_EN_DETECT_WIDTH);
}

void tvafe_avin_detect_ch2_anlog_enable(bool enable)
{
	if (enable)
		W_HIU_BIT(HHI_CVBS_DETECT_CNTL, 1,
			AFE_CH2_EN_DETECT_BIT, AFE_CH2_EN_DETECT_WIDTH);
	else
		W_HIU_BIT(HHI_CVBS_DETECT_CNTL, 0,
			AFE_CH2_EN_DETECT_BIT, AFE_CH2_EN_DETECT_WIDTH);
}

static void tvafe_avin_enable(struct tvafe_avin_det_s *avin_data)
{
	int i;
	/*enable irq */
	if (avin_data->dts_param.device_mask & TVAFE_AVIN_CH1_MASK)
		tvafe_avin_detect_ch1_anlog_enable(1);
	if (avin_data->dts_param.device_mask & TVAFE_AVIN_CH2_MASK)
		tvafe_avin_detect_ch2_anlog_enable(1);
	for (i = 0; i < avin_data->device_num; i++)
		enable_irq(avin_data->dts_param.irq[i]);

}

static void tvafe_avin_disable(struct tvafe_avin_det_s *avin_data)
{
	int i;
	/*enable irq */
	if (avin_data->dts_param.device_mask & TVAFE_AVIN_CH1_MASK)
		tvafe_avin_detect_ch1_anlog_enable(0);
	if (avin_data->dts_param.device_mask & TVAFE_AVIN_CH2_MASK)
		tvafe_avin_detect_ch2_anlog_enable(0);
	for (i = 0; i < avin_data->device_num; i++)
		free_irq(avin_data->dts_param.irq[i], (void *)avin_data);

}

static int tvafe_avin_open(struct inode *inode, struct file *file)
{
	struct tvafe_avin_det_s *avin_data;
	avin_data = container_of(inode->i_cdev,
		struct tvafe_avin_det_s, avin_cdev);
	file->private_data = avin_data;
	/*enable irq */
	tvafe_avin_enable(avin_data);
	return 0;
}

static ssize_t tvafe_avin_read(struct file *file, char __user *buf,
	size_t count, loff_t *ppos)
{
	unsigned long ret;
	struct tvafe_avin_det_s *avin_data =
		(struct tvafe_avin_det_s *)file->private_data;

	ret = copy_to_user(buf,
		(void *)(&avin_data->report_data_s[0]),
		sizeof(struct tvafe_report_data_s)
		* avin_data->device_num);

	return 0;
}

static int tvafe_avin_release(struct inode *inode, struct file *file)
{
	struct tvafe_avin_det_s *avin_data =
		(struct tvafe_avin_det_s *)file->private_data;
	tvafe_avin_disable(avin_data);
	file->private_data = NULL;
	return 0;
}

static unsigned tvafe_avin_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;
	poll_wait(file, &tvafe_avin_waitq, wait);
	mask |= POLLIN | POLLRDNORM;

	return mask;
}

static const struct file_operations tvafe_avin_fops = {
	.owner      = THIS_MODULE,
	.open       = tvafe_avin_open,
	.read       = tvafe_avin_read,
	.poll       = tvafe_avin_poll,
	.release    = tvafe_avin_release,
};

static int tvafe_register_avin_dev(struct tvafe_avin_det_s *avin_data)
{
	int ret = 0;

	ret = alloc_chrdev_region(&avin_data->avin_devno,
		0, 1, TVAFE_AVIN_NAME);
	if (ret < 0) {
		pr_err("avin: failed to allocate major number\n");
		return -ENODEV;
	}

	/* connect the file operations with cdev */
	cdev_init(&avin_data->avin_cdev, &tvafe_avin_fops);
	avin_data->avin_cdev.owner = THIS_MODULE;
	/* connect the major/minor number to the cdev */
	ret = cdev_add(&avin_data->avin_cdev, avin_data->avin_devno, 1);
	if (ret) {
		pr_err("avin: failed to add device\n");
		return -ENODEV;
	}

	strcpy(avin_data->config_name, TVAFE_AVIN_NAME);
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

static int tvafe_avin_init_resource(struct tvafe_avin_det_s *avdev)
{
	int irq_ret, i;
	INIT_WORK(&(avdev->work_struct_update),
		tvafe_update_work_update_status);

	/* request irq num*/
	for (i = 0; i < avdev->device_num; i++) {
		irq_ret = request_irq(avdev->dts_param.irq[i],
			tvafe_avin_detect_handler, IRQF_DISABLED,
			tvafe_avin_name_ch[i], (void *)avdev);
		if (irq_ret)
			return -EINVAL;
		/*disable irq untill avin is opened completely*/
		disable_irq_nosync(avdev->dts_param.irq[i]);
	}
	return 0;
}

void tvafe_avin_detect_anlog_config(void)
{
	if (detect_mode == 0) {
		/*for ch1*/
		W_HIU_BIT(HHI_CVBS_DETECT_CNTL, 0,
			AFE_DETECT_RSV1_BIT, AFE_DETECT_RSV1_WIDTH);
		/*for ch2*/
		W_HIU_BIT(HHI_CVBS_DETECT_CNTL, 0,
			AFE_DETECT_RSV3_BIT, AFE_DETECT_RSV3_WIDTH);
	} else {
		W_HIU_BIT(HHI_CVBS_DETECT_CNTL, 1,
			AFE_DETECT_RSV1_BIT, AFE_DETECT_RSV1_WIDTH);
		W_HIU_BIT(HHI_CVBS_DETECT_CNTL, 1,
			AFE_DETECT_RSV3_BIT, AFE_DETECT_RSV3_WIDTH);
	}

	/*ch1 config*/
	W_HIU_BIT(HHI_CVBS_DETECT_CNTL, sync_level,
		AFE_CH1_SYNC_LEVEL_ADJ_BIT, AFE_CH1_SYNC_LEVEL_ADJ_WIDTH);
	W_HIU_BIT(HHI_CVBS_DETECT_CNTL, sync_hys_adj,
		AFE_CH1_SYNC_HYS_ADJ_BIT, AFE_CH1_SYNC_HYS_ADJ_WIDTH);
	W_HIU_BIT(HHI_CVBS_DETECT_CNTL, vdc_level,
		AFE_DETECT_RSV0_BIT, AFE_DETECT_RSV0_WIDTH);
	/*after adc and afe is enable,this bit must be set to "0"*/
	W_HIU_BIT(HHI_CVBS_DETECT_CNTL, 1, AFE_CH1_EN_SYNC_TIP_BIT,
		AFE_CH1_EN_SYNC_TIP_WIDTH);

	/***ch2 config***/
	W_HIU_BIT(HHI_CVBS_DETECT_CNTL, sync_level,
		AFE_CH2_SYNC_LEVEL_ADJ_BIT, AFE_CH2_SYNC_LEVEL_ADJ_WIDTH);
	W_HIU_BIT(HHI_CVBS_DETECT_CNTL, sync_hys_adj,
		AFE_CH2_SYNC_HYS_ADJ_BIT, AFE_CH2_SYNC_HYS_ADJ_WIDTH);
	W_HIU_BIT(HHI_CVBS_DETECT_CNTL, vdc_level,
		AFE_DETECT_RSV2_BIT, AFE_DETECT_RSV2_WIDTH);
	W_HIU_BIT(HHI_CVBS_DETECT_CNTL, 1, AFE_CH2_EN_SYNC_TIP_BIT,
		AFE_CH2_EN_SYNC_TIP_WIDTH);
}

void tvafe_avin_detect_digital_config(void)
{
	aml_cbus_update_bits(CVBS_IRQ0_CNTL,
		CVBS_IRQ_MODE_MASK << CVBS_IRQ_MODE_BIT,
		irq_mode << CVBS_IRQ_MODE_BIT);
	aml_cbus_update_bits(CVBS_IRQ0_CNTL,
		CVBS_IRQ_TRIGGER_SEL_MASK << CVBS_IRQ_TRIGGER_SEL_BIT,
		trigger_sel << CVBS_IRQ_TRIGGER_SEL_BIT);
	aml_cbus_update_bits(CVBS_IRQ0_CNTL,
		CVBS_IRQ_EDGE_EN_MASK << CVBS_IRQ_EDGE_EN_BIT,
		irq_edge_en << CVBS_IRQ_EDGE_EN_BIT);
	aml_cbus_update_bits(CVBS_IRQ0_CNTL,
		CVBS_IRQ_FILTER_MASK << CVBS_IRQ_FILTER_BIT,
		irq_filter << CVBS_IRQ_FILTER_BIT);
	aml_cbus_update_bits(CVBS_IRQ0_CNTL,
		CVBS_IRQ_POL_MASK << CVBS_IRQ_POL_BIT,
		irq_pol << CVBS_IRQ_POL_BIT);
}

static void tvafe_avin_detect_state(struct tvafe_avin_det_s *avdev)
{
	pr_info("device_num: %d\n", avdev->device_num);
	pr_info("\t*****dts param*****\n");
	pr_info("device_mask: %d\n", avdev->dts_param.device_mask);
	pr_info("irq0: %d\n", avdev->dts_param.irq[0]);
	pr_info("irq1: %d\n", avdev->dts_param.irq[1]);
	pr_info("irq_counter[0]: 0x%x\n", avdev->irq_counter[0]);
	pr_info("irq_counter[1]: 0x%x\n", avdev->irq_counter[1]);
	pr_info("\t*****channel status:0->in;1->out*****\n");
	pr_info("channel[%d] status: %d\n", avdev->report_data_s[0].channel,
		avdev->report_data_s[0].status);
	pr_info("channel[%d] status: %d\n", avdev->report_data_s[1].channel,
		avdev->report_data_s[1].status);
	pr_info("\t*****global param*****\n");
	pr_info("detect_mode: %d\n", detect_mode);
	pr_info("vdc_level: %d\n", vdc_level);
	pr_info("sync_level: %d\n", sync_level);
	pr_info("sync_hys_adj: %d\n", sync_hys_adj);
	pr_info("irq_mode: %d\n", irq_mode);
	pr_info("trigger_sel: %d\n", trigger_sel);
	pr_info("irq_edge_en: %d\n", irq_edge_en);
	pr_info("irq_filter: %d\n", irq_filter);
	pr_info("irq_pol: %d\n", irq_pol);
}

static void tvafe_avin_detect_parse_param(char *buf_orig, char **parm)
{
	char *ps, *token;
	char delim1[2] = " ";
	char delim2[2] = "\n";
	unsigned int n = 0;
	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}

static ssize_t tvafe_avin_detect_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	len += sprintf(buf+len,
		"\t*****usage:*****\n");
	len += sprintf(buf+len,
		"echo detect_mode val(D) > debug\n");
	len += sprintf(buf+len,
		"echo vdc_level val(D) > debug\n");
	len += sprintf(buf+len,
		"echo sync_level val(D) > debug\n");
	len += sprintf(buf+len,
		"echo sync_hys_adj val(D) > debug\n");
	len += sprintf(buf+len,
		"echo irq_mode val(D) > debug\n");
	len += sprintf(buf+len,
		"echo trigger_sel val(D) > debug\n");
	len += sprintf(buf+len,
		"echo irq_edge_en val(D) > debug\n");
	len += sprintf(buf+len,
		"echo irq_filter val(D) > debug\n");
	len += sprintf(buf+len,
		"echo irq_pol val(D) > debug\n");
	len += sprintf(buf+len,
		"echo ch1_enable val(D) > debug\n");
	len += sprintf(buf+len,
		"echo ch2_enable val(D) > debug\n");
	len += sprintf(buf+len,
		"echo enable > debug\n");
	len += sprintf(buf+len,
		"echo disable > debug\n");
	len += sprintf(buf+len,
		"echo status > debug\n");
	return len;
}


static ssize_t tvafe_avin_detect_store(struct device *dev,
	struct device_attribute *attr, const char *buff, size_t count)
{
	struct tvafe_avin_det_s *avdev;
	unsigned long val;
	char *buf_orig, *parm[10] = {NULL};
	avdev = dev_get_drvdata(dev);

	if (!buff)
		return count;
	buf_orig = kstrdup(buff, GFP_KERNEL);
	tvafe_avin_detect_parse_param(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "detect_mode")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		detect_mode = val;
	} else if (!strcmp(parm[0], "vdc_level")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		vdc_level = val;
	} else if (!strcmp(parm[0], "sync_level")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sync_level = val;
	} else if (!strcmp(parm[0], "sync_hys_adj")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sync_hys_adj = val;
	} else if (!strcmp(parm[0], "irq_mode")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		irq_mode = val;
	} else if (!strcmp(parm[0], "trigger_sel")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		trigger_sel = val;
	} else if (!strcmp(parm[0], "irq_edge_en")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		irq_edge_en = val;
	} else if (!strcmp(parm[0], "irq_filter")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		irq_filter = val;
	} else if (!strcmp(parm[0], "irq_pol")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		irq_pol = val;
	} else if (!strcmp(parm[0], "ch1_enable")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		if (val)
			tvafe_avin_detect_ch1_anlog_enable(1);
		else
			tvafe_avin_detect_ch1_anlog_enable(0);
	} else if (!strcmp(parm[0], "ch2_enable")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		if (val)
			tvafe_avin_detect_ch2_anlog_enable(1);
		else
			tvafe_avin_detect_ch2_anlog_enable(0);
	} else if (!strcmp(parm[0], "enable")) {
		tvafe_avin_enable(avdev);
	} else if (!strcmp(parm[0], "disable")) {
		tvafe_avin_disable(avdev);
	} else if (!strcmp(parm[0], "status")) {
		tvafe_avin_detect_state(avdev);
	} else
		pr_info("[%s]:invaild command.\n", __func__);
	return count;
}

static DEVICE_ATTR(debug, 0644, tvafe_avin_detect_show,
	tvafe_avin_detect_store);

int tvafe_avin_detect_probe(struct platform_device *pdev)
{
	int i;
	int ret;
	int state = 0;
	struct tvafe_avin_det_s *avdev = NULL;
	avdev = kzalloc(sizeof(struct tvafe_avin_det_s), GFP_KERNEL);
	if (!avdev) {
		pr_info("kzalloc error\n");
		state = -ENOMEM;
		goto get_param_mem_fail;
	}

	platform_set_drvdata(pdev, avdev);

	ret = tvafe_avin_dts_parse(pdev);
	if (ret) {
		state = ret;
		goto get_dts_dat_fail;
	}

	INIT_WORK(&(avdev->work_struct_update),
		tvafe_update_work_update_status);
	/* request irq num*/
	for (i = 0; i < avdev->device_num; i++) {
		ret = request_irq(avdev->dts_param.irq[i],
			tvafe_avin_detect_handler, IRQF_DISABLED,
			tvafe_avin_name_ch[i], (void *)avdev);
		if (ret) {
			pr_info("Unable to init irq resource.\n");
			state = -EINVAL;
			goto irq_request_fail;
		}
		/*disable irq untill avin is opened completely*/
		disable_irq_nosync(avdev->dts_param.irq[i]);
	}
	/*config analog part setting*/
	tvafe_avin_detect_anlog_config();
	/*config digital part setting*/
	tvafe_avin_detect_digital_config();

	mutex_init(&avdev->lock);

	/* register char device */
	ret = tvafe_register_avin_dev(avdev);
	/* create class attr file */
	ret = device_create_file(avdev->config_dev, &dev_attr_debug);
	if (ret < 0) {
		pr_err("tvafe_av_detect: fail to create dbg attribute file\n");
		goto fail_create_dbg_file;
	}
	return 0;

fail_create_dbg_file:
irq_request_fail:
get_dts_dat_fail:
	kfree(avdev);
get_param_mem_fail:
	return state;
}

static int tvafe_avin_detect_suspend(struct platform_device *pdev ,
	pm_message_t state)
{
	struct tvafe_avin_det_s *avdev = platform_get_drvdata(pdev);
	cancel_work_sync(&avdev->work_struct_update);
	tvafe_avin_disable(avdev);
	pr_info("tvafe_avin_detect_suspend ok.\n");
	return 0;
}

static int tvafe_avin_detect_resume(struct platform_device *pdev)
{
	struct tvafe_avin_det_s *avdev = platform_get_drvdata(pdev);
	tvafe_avin_init_resource(avdev);
	pr_info("tvafe_avin_detect_resume ok.\n");
	return 0;
}

int tvafe_avin_detect_remove(struct platform_device *pdev)
{
	struct tvafe_avin_det_s *avdev = platform_get_drvdata(pdev);
	cdev_del(&avdev->avin_cdev);
	cancel_work_sync(&avdev->work_struct_update);
	tvafe_avin_disable(avdev);
	device_remove_file(avdev->config_dev, &dev_attr_debug);
	kfree(avdev);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id tvafe_avin_dt_match[] = {
	{	.compatible = "amlogic, avin_detect",
	},
	{},
};
#else
#define tvafe_avin_dt_match NULL
#endif

static struct platform_driver tvafe_avin_driver = {
	.probe      = tvafe_avin_detect_probe,
	.remove     = tvafe_avin_detect_remove,
	.suspend    = tvafe_avin_detect_suspend,
	.resume     = tvafe_avin_detect_resume,
	.driver     = {
		.name   = "tvafe_avin_detect",
		.of_match_table = tvafe_avin_dt_match,
	},
};

static int __init tvafe_avin_detect_init(void)
{
	return platform_driver_register(&tvafe_avin_driver);
}

static void __exit tvafe_avin_detect_exit(void)
{
	platform_driver_unregister(&tvafe_avin_driver);
}

module_init(tvafe_avin_detect_init);
module_exit(tvafe_avin_detect_exit);

MODULE_DESCRIPTION("Meson TVAFE AVIN detect Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");



