/* Lite-On TSL2751-ALS Linux Driver
 *
 * Copyright (C) 2011 Lite-On Technology Corp (Singapore)
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */
#include "tsl2571.h"

static struct tsl2571_data *the_data;
struct tsl2571_settings setting_data = {
	.als_time = 219,		/*ms*/
	.als_gain = 0,
	.als_gain_trim = 1000,
	.als_ga = 100, /*GA=1*/
	.wait_time = 245,
	.prox_config = 2,
	.interrupts_en = 0x10,
	.persistence = 20,
	.als_thresh_low = 64,
	.als_thresh_high = 3840,
};
char help_buf[42][256] = {
"*********************************help*********************************\n",
"als_time	: The ALS timing register controls the internal integration",
" time of the ALS ADCs in 2.72-ms increments.\n",
"			:[show] 3~696(ms)\n",
"			:[store] 3~696(ms)\n",
"enable		: enable tsl2571 interface .we should disable tsl2571 ,",
" when opration config parameter.\n",
"			:[show] 0:disable 1:enable\n",
"			:[store] 0:disable 1:enable\n",
"als_gain	: These bits typically control functions such as gain",
"settings and/or diode selection.\n",
"			:[show] 0~3:0->1*gain,1->8*g,2->16*g,3->120*g\n",
"			:[store] 0~3\n",
"als_ga		: (GA)glass attenuation. For a device in open air with no",
"aperture or glass/plastic above the device, GA = 1(100%)\n",
"			:[show] 0~100 :glass attenuation (0~100%);\n",
"			:[store] 0~100\n",
"wait_time	:Wait Time Register\n",
"			:[show] 0~256 : wait time\n",
"					WLONG=0 : [696~2.72]ms\n",
"					WLONG=1 : [32~8300]ms\n",
"			:[store] 0~256\n",
"persistence	: The persistence register controls the filtering interrupt",
" capabilities of the device.\n",
"			:[show] 0~15 :FIELD VALUE[0~15] ,onsecutive values",
" out of range[1~60];\n",
"			:[store] 0~15\n",
" als_low		: ALS Interrupt Threshold Registers.\n",
"			:[show] 0~65535 low threshold upper byte. you notice",
" als_low < als_high ;\n",
"			:[store] 0~65535\n",
"als_high	: ALS Interrupt Threshold Registers.\n",
"			:[show] 0~65535 high threshold upper byte.",
"you notice als_low < als_high ;\n",
"			:[store] 0~65535\n",
"als_lux		: get lux values\n",
"			:[show] 0~65535\n",
"als_help       : help\n",
"			:[show]\n",
"***************************************************************************\n"

};


static int tsl2x7x_i2c_read(struct i2c_client *client, u8 reg, u8 *val)
{
	int ret = 0;

	/* select register to write */
	ret = i2c_smbus_write_byte(client, (TSL2X7X_CMD_REG | reg));
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to write register %x\n"
				, __func__, reg);
		return ret;
	}

	/* read the data */
	ret = i2c_smbus_read_byte(client);
	if (ret >= 0)
		*val = (u8)ret;
	else
		dev_err(&client->dev, "%s: failed to read register %x\n"
						, __func__, reg);

	return ret;
}
static int tsl2571_chip_on(void)
{
	int i;
	int ret = 0;
	u8 *dev_reg;
	u8 utmp;
	int als_count;
	int als_time;
	u8 reg_val = 0;

	/* and make sure we're not already on */
	if (the_data->tsl2x7x_chip_status == TSL2X7X_CHIP_WORKING) {
		/* if forcing a register update - turn off, then on */
		dev_info(&the_data->client->dev, "device is already enabled\n");
		return -EINVAL;
	}
	dprintk("%s mutex_lock1\n", __func__);
	/* Non calculated parameters */

	dprintk("%s mutex_lock1\n", __func__);
	tsl2571_config[TSL2X7X_WAIT_TIME] =
			setting_data.wait_time;
	tsl2571_config[TSL2X7X_PRX_CONFIG] =
			setting_data.prox_config;
	tsl2571_config[TSL2X7X_ALS_MINTHRESHLO] =
		(setting_data.als_thresh_low) & 0xFF;
	tsl2571_config[TSL2X7X_ALS_MINTHRESHHI] =
		(setting_data.als_thresh_low >> 8) & 0xFF;
	tsl2571_config[TSL2X7X_ALS_MAXTHRESHLO] =
		(setting_data.als_thresh_high) & 0xFF;
	tsl2571_config[TSL2X7X_ALS_MAXTHRESHHI] =
		(setting_data.als_thresh_high >> 8) & 0xFF;
	tsl2571_config[TSL2X7X_PERSISTENCE] =
		setting_data.persistence;


	/* determine als integration register */
	als_count = (setting_data.als_time * 100 + 135) / 270;
	if (als_count == 0)
		als_count = 1; /* ensure at least one cycle */

	/* convert back to time (encompasses overrides) */
	als_time = (als_count * 27 + 5) / 10;
	tsl2571_config[TSL2X7X_ALS_TIME] = 256 - als_count;

	/* Set the gain based on tsl2x7x_settings struct */
	tsl2571_config[TSL2X7X_GAIN] = setting_data.als_gain;

	/* set chip struct re scaling and saturation */
	the_data->als_saturation = als_count * 922; /* 90% of full scale */
	the_data->als_time_scale = (als_time + 25) / 50;

	/* TSL2X7X Specific power-on / adc enable sequence
	 * Power on the device 1st. */
	utmp = TSL2X7X_CNTL_PWR_ON;
	ret = i2c_smbus_write_byte_data(the_data->client,
		TSL2X7X_CMD_REG | TSL2X7X_CNTRL, utmp);
	if (ret < 0) {
		dev_err(&the_data->client->dev,
			"%s: failed on CNTRL reg.\n", __func__);
		return ret;
	}

	/* Use the following shadow copy for our delay before enabling ADC.
	 * Write all the registers. */
	for (i = 0, dev_reg = tsl2571_config;
			i < TSL2X7X_MAX_CONFIG_REG; i++) {
		ret = i2c_smbus_write_byte_data(the_data->client,
				TSL2X7X_CMD_REG + i, *dev_reg++);
		if (ret < 0) {
			dev_err(&the_data->client->dev,
			"%s: failed on write to reg %d.\n", __func__, i);
			return ret;
		}
	}

	mdelay(3);	/* Power-on settling time */

	/* NOW enable the ADC
	 * initialize the desired mode of operation */
	utmp = TSL2X7X_CNTL_PWR_ON |
			TSL2X7X_CNTL_ADC_ENBL |
			TSL2X7X_CNTL_PROX_DET_ENBL |
			TSL2X7X_CNTL_WAIT_TMR_ENBL;
	ret = i2c_smbus_write_byte_data(the_data->client,
			TSL2X7X_CMD_REG | TSL2X7X_CNTRL, utmp);
	if (ret < 0) {
		dev_err(&the_data->client->dev,
			"%s: failed on 2nd CTRL reg.\n", __func__);
		return ret;
	}

	the_data->tsl2x7x_chip_status = TSL2X7X_CHIP_WORKING;

	if (setting_data.interrupts_en != 0) {
		dev_info(&the_data->client->dev, "Setting Up Interrupt(s)\n");

		reg_val = TSL2X7X_CNTL_PWR_ON | TSL2X7X_CNTL_ADC_ENBL;

		reg_val |= setting_data.interrupts_en;
		ret = i2c_smbus_write_byte_data(the_data->client,
			(TSL2X7X_CMD_REG | TSL2X7X_CNTRL), reg_val);
		if (ret < 0)
			dev_err(&the_data->client->dev,
				"%s: failed in tsl2x7x_IOCTL_INT_SET.\n",
				__func__);

		/* Clear out any initial interrupts  */
		ret = i2c_smbus_write_byte(the_data->client,
			TSL2X7X_CMD_REG | TSL2X7X_CMD_SPL_FN |
			TSL2X7X_CMD_ALS_INT_CLR);
		if (ret < 0) {
			dev_err(&the_data->client->dev,
				"%s: Failed to clear Int status\n",
				__func__);
			return ret;
		}
	}

	return ret;
}
static int tsl2571_chip_off(void)
{
	int ret;

	/* turn device off */
	the_data->tsl2x7x_chip_status = TSL2X7X_CHIP_SUSPENDED;

	ret = i2c_smbus_write_byte_data(the_data->client,
		TSL2X7X_CMD_REG | TSL2X7X_CNTRL, 0x00);

	return ret;
}
static int tsl2571_get_lux(void)
{
	u16 ch0, ch1; /* separated ch0/ch1 data from device */
	u32 lux = 0; /* raw lux calculated from device data */
	u32 ratio;
	u8 buf[4];
	int i, ret;
	u32 ch0lux = 0;
	u32 ch1lux = 0;
	if (mutex_trylock(&the_data->als_mutex) == 0)
		goto return_max;

	if (the_data->tsl2x7x_chip_status != TSL2X7X_CHIP_WORKING) {
		/* device is not enabled */
		dev_err(&the_data->client->dev, "%s: device is not enabled\n",
				__func__);
		ret = -EBUSY;
		goto out_unlock;
	}
	ret = tsl2x7x_i2c_read(the_data->client,
		(TSL2X7X_CMD_REG | TSL2X7X_STATUS), &buf[0]);
	if (ret < 0) {
		dev_err(&the_data->client->dev,
			"%s: Failed to read STATUS Reg\n", __func__);
		goto out_unlock;
	}
	/* is data new & valid */
	if (!(buf[0] & TSL2X7X_STA_ADC_VALID)) {
		dev_err(&the_data->client->dev,
			"%s: data not valid yet\n", __func__);
		ret = the_data->als_cur_info.lux; /* return LAST VALUE */
		goto out_unlock;
	}
	for (i = 0; i < 4; i++) {
		ret = tsl2x7x_i2c_read(the_data->client,
			(TSL2X7X_CMD_REG | (TSL2X7X_ALS_CHAN0LO + i)),
			&buf[i]);
		if (ret < 0) {
			dev_err(&the_data->client->dev,
				"%s: failed to read. err=%x\n", __func__, ret);
			goto out_unlock;
		}
	}
	/* clear any existing interrupt status */
	ret = i2c_smbus_write_byte(the_data->client,
		(TSL2X7X_CMD_REG |
				TSL2X7X_CMD_SPL_FN |
				TSL2X7X_CMD_ALS_INT_CLR));
	if (ret < 0) {
		dev_err(&the_data->client->dev,
		"%s: i2c_write_command failed - err = %d\n",
			__func__, ret);
		goto out_unlock; /* have no data, so return failure */
	}
	/* extract ALS/lux data */
	ch0 = le16_to_cpup((const __le16 *)&buf[0]);
	ch1 = le16_to_cpup((const __le16 *)&buf[2]);
	the_data->als_cur_info.als_ch0 = ch0;
	the_data->als_cur_info.als_ch1 = ch1;
	if ((ch0 >= the_data->als_saturation) ||
			(ch1 >= the_data->als_saturation)) {
		lux = TSL2X7X_LUX_CALC_OVER_FLOW;
		goto return_max;
	}
	if (ch0 == 0) {
		/* have no data, so return LAST VALUE */
		ret = the_data->als_cur_info.lux;
		goto out_unlock;
	}
	/* calculate ratio */
	ratio = DIV_ROUND_UP(setting_data.als_ga*LUX_COEFF,
	setting_data.als_time*tsl2X7X_als_gainadj[setting_data.als_gain]*100);
	/* convert to unscaled lux using the pointer to the table */
	if (ch0 >= 2*ch1)
		ch0lux = ratio*(ch0-2*ch1);
	else
		ch0lux = 0;
	if (3*ch0 >= 5*ch1)
		ch1lux = ratio*(DIV_ROUND_UP(ch0*3 , 5)-ch1);
	else
		ch1lux = 0;
	if (ch0lux*ch1lux >= 0)
		lux = (ch0lux > ch1lux) ? ch0lux : ch1lux;
	else
		lux = 0;

	if (lux > TSL2X7X_LUX_CALC_OVER_FLOW) /* check for overflow */
		lux = TSL2X7X_LUX_CALC_OVER_FLOW;
	/* Update the structure with the latest lux. */
return_max:
	the_data->als_cur_info.lux = lux;
	ret = lux;

out_unlock:
	mutex_unlock(&the_data->als_mutex);
	enable_irq(the_data->client->irq);
	return ret;
}

static irqreturn_t tsl2571_irq_handler(int irq, void *handle)
{
	unsigned long delay = msecs_to_jiffies(atomic_read(&the_data->delay));
	disable_irq_nosync(the_data->client->irq);
	schedule_delayed_work(&the_data->work, delay);
	/*dprintk("tsl2571_irq_handler(%d)\n", irq);*/
	return IRQ_HANDLED;

}
static int tsl2571_dev_init(void){
	int ret;
	unsigned char device_id;
	/*request interruput*/
	/*dprintk(" tsl2571_dev_init!!\n");*/
	ret = tsl2x7x_i2c_read(the_data->client,
		TSL2X7X_CHIPID, &device_id);
	if (ret < 0)
		return ret;
	tsl2571_chip_on();
	return 0;
}
static void tsl2571_schedwork(struct work_struct *work)
{
	tsl2571_get_lux();
	input_report_abs(the_data->input_dev, ABS_X,
					the_data->als_cur_info.als_ch0);
	input_report_abs(the_data->input_dev, ABS_Y,
					the_data->als_cur_info.als_ch1);
	input_report_abs(the_data->input_dev, ABS_Z,
						the_data->als_cur_info.lux);
	input_sync(the_data->input_dev);
}

/*****************************************************************************
*	DEVICE_ATTR interface sys/class/
*****************************************************************************/
/*als_time*/
static ssize_t tsl2571_als_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", setting_data.als_time);
}
static ssize_t tsl2571_als_time_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	unsigned long data;
	ret = kstrtoul(buf, 10, &data);
	if (ret)
		return ret;
	if (TSL2X7X_CHIP_WORKING == the_data->tsl2x7x_chip_status) {
		dprintk("tsl2571 busy .... in %s\n", __func__);
		return count;
	} else if (data > 696 && data < 3) {
		dprintk("Input parameter range wrong .... in %s\n", __func__);
		return count;
	}
	setting_data.als_time = data;
	return count;
}
/*enable*/
static ssize_t tsl2571_als_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	dprintk("%s\n", __func__);
	return sprintf(buf, "%d\n", the_data->tsl2x7x_chip_status);
}
static ssize_t tsl2571_als_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	unsigned long data;
	dprintk("%s = %s\n", __func__, buf);
	ret = kstrtoul(buf, 10, &data);
	if (ret)
		return ret;
	if (data == 0) {
		ret = tsl2571_chip_off();
		if (ret != 0) {
			dprintk("Faile tsl2571_chip_off %s\n", __func__);
			return ret;
		}
	} else if (data == 1) {
		ret = tsl2571_chip_on();
	if (ret != 0) {
			dprintk("Faile tsl2571_chip_on %s\n", __func__);
			return ret;
	}
	} else {
		ret = -1;
	}
	return count;
}
/*als_gain*/
static ssize_t tsl2571_als_gain_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", setting_data.als_gain);
}
static ssize_t tsl2571_als_gain_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	unsigned long data;
	ret = kstrtoul(buf, 10, &data);
	if (ret)
		return ret;
	if (TSL2X7X_CHIP_WORKING == the_data->tsl2x7x_chip_status) {
		dprintk("tsl2571 busy .... in %s\n", __func__);
		return count;
	} else if (data > 4 && data < 0) {
		dprintk("Input parameter range wrong .... in %s\n", __func__);
		return count;
	}
	setting_data.als_gain = data;
	return count;

}
/*als_ga*/
static ssize_t tsl2571_als_ga_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", setting_data.als_ga);
}
static ssize_t tsl2571_als_ga_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	unsigned long data;
	ret = kstrtoul(buf, 10, &data);
	if (ret)
		return ret;
	if (TSL2X7X_CHIP_WORKING == the_data->tsl2x7x_chip_status) {
		dprintk("tsl2571 busy .... in %s\n", __func__);
		return count;
	} else if (data > 100 && data < 0) {
		dprintk("Input parameter range wrong .... in %s\n", __func__);
		return count;
	}
	setting_data.als_ga = data;
	return count;

}
/*wait_time*/
static ssize_t tsl2571_als_wait_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", setting_data.wait_time);
}
static ssize_t tsl2571_als_wait_time_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	unsigned long data;
	ret = kstrtoul(buf, 10, &data);
	if (ret)
		return ret;
	if (TSL2X7X_CHIP_WORKING == the_data->tsl2x7x_chip_status) {
		dprintk("tsl2571 busy .... in %s\n", __func__);
		return count;
	} else if (data > 256 && data < 0) {
		dprintk("Input parameter range wrong .... in %s\n", __func__);
		return count;
	}
	setting_data.wait_time = data;
	return count;

}
/*persistence*/
static ssize_t tsl2571_persistence_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", setting_data.persistence);
}
static ssize_t tsl2571_persistence_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	unsigned long data;
	ret = kstrtoul(buf, 10, &data);
	if (ret)
		return ret;
	if (TSL2X7X_CHIP_WORKING == the_data->tsl2x7x_chip_status) {
		dprintk("tsl2571 busy .... in %s\n", __func__);
		return count;
	} else if (data > 15 && data < 0) {
		dprintk("Input parameter range wrong .... in %s\n", __func__);
		return count;
	}
	setting_data.persistence = data;
	return count;

}
/*als_low*/
static ssize_t tsl2571_als_low_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", setting_data.als_thresh_low);
}
static ssize_t tsl2571_als_low_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	unsigned long data;
	ret = kstrtoul(buf, 10, &data);
	if (ret)
		return ret;
	if (TSL2X7X_CHIP_WORKING == the_data->tsl2x7x_chip_status) {
		dprintk("tsl2571 busy .... in %s\n", __func__);
		return count;
	} else if (data > 65535 && data < 0) {
		dprintk("Input parameter range wrong .... in %s\n", __func__);
		return count;
	} else if (setting_data.als_thresh_low >
				setting_data.als_thresh_high) {
		dprintk("Input parameter als_low more than als_high in %s\n",
				__func__);
		return count;
	}
	setting_data.als_thresh_low = data;
	return count;

}
/*als_high*/
static ssize_t tsl2571_als_high_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", setting_data.als_thresh_high);
}
static ssize_t tsl2571_als_high_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	unsigned long data;
	ret = kstrtoul(buf, 10, &data);
	if (ret)
		return ret;
	if (TSL2X7X_CHIP_WORKING == the_data->tsl2x7x_chip_status) {
		dprintk("tsl2571 busy .... in %s\n", __func__);
		return count;
	} else if (data > 65535 && data < 0) {
		dprintk("Input parameter range wrong .... in %s\n", __func__);
		return count;
	} else if (setting_data.als_thresh_low >
				setting_data.als_thresh_high) {
		dprintk("Input parameter range wrong in %s\n", __func__);
		return count;
	}
	setting_data.als_thresh_high = data;
	return count;

}
/*als_lux*/
static ssize_t tsl2571_als_lux_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", the_data->als_cur_info.lux);
}
/*help*/
static ssize_t tsl2571_als_help_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i = 0;
	for (i = 0; i < 42 ; i++)
		dprintk("%s", help_buf[i]);
	return 0;
}
static DEVICE_ATTR(als_time, S_IRUGO|S_IWUSR|S_IWGRP,
		tsl2571_als_time_show, tsl2571_als_time_store);
static DEVICE_ATTR(als_enable, S_IRUGO|S_IWUSR|S_IWGRP,
		tsl2571_als_enable_show, tsl2571_als_enable_store);
static DEVICE_ATTR(als_gain, S_IRUGO|S_IWUSR|S_IWGRP,
		tsl2571_als_gain_show, tsl2571_als_gain_store);
static DEVICE_ATTR(als_ga, S_IRUGO|S_IWUSR|S_IWGRP,
		tsl2571_als_ga_show, tsl2571_als_ga_store);
static DEVICE_ATTR(als_wait_time, S_IRUGO|S_IWUSR|S_IWGRP,
		tsl2571_als_wait_time_show, tsl2571_als_wait_time_store);
static DEVICE_ATTR(als_persistence, S_IRUGO|S_IWUSR|S_IWGRP,
		tsl2571_persistence_show, tsl2571_persistence_store);
static DEVICE_ATTR(als_low, S_IRUGO|S_IWUSR|S_IWGRP,
		tsl2571_als_low_show, tsl2571_als_low_store);
static DEVICE_ATTR(als_high, S_IRUGO|S_IWUSR|S_IWGRP,
		tsl2571_als_high_show, tsl2571_als_high_store);
static DEVICE_ATTR(als_lux, S_IRUGO|S_IWGRP,
		tsl2571_als_lux_show, NULL);
static DEVICE_ATTR(als_help, S_IRUGO|S_IWGRP,
		tsl2571_als_help_show, NULL);

static struct attribute *tsl2571_attributes[] = {
	&dev_attr_als_time.attr,
	&dev_attr_als_enable.attr,
	&dev_attr_als_gain.attr,
	&dev_attr_als_ga.attr,
	&dev_attr_als_wait_time.attr,
	&dev_attr_als_persistence.attr,
	&dev_attr_als_low.attr,
	&dev_attr_als_high.attr,
	&dev_attr_als_lux.attr,
	&dev_attr_als_help.attr,
	NULL
};
static struct attribute_group tsl2571_attribute_group = {
	.attrs = tsl2571_attributes
};
static int tsl2571_probe(struct i2c_client *client,
	const struct i2c_device_id *id){
	int ret = 0;
	struct input_dev *idev;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	dprintk(" start TSL2571 probe !!\n");
	/* Return 1 if adapter supports everything we need, 0 if not. */
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WRITE_BYTE |
		I2C_FUNC_SMBUS_READ_BYTE_DATA)) {
		dprintk(KERN_ALERT
			"%s: TSL2571-ALS functionality check failed.\n",
			__func__);
		ret = -EIO;
		return ret;
	}
	/* data memory allocation */
	the_data = kzalloc(sizeof(struct tsl2571_data), GFP_KERNEL);
	if (the_data == NULL) {
		dprintk(KERN_ALERT "%s: LTR501-ALS kzalloc failed.\n",
			__func__);
		ret = -ENOMEM;
		return ret;
	}
	the_data->client = client;
	i2c_set_clientdata(client, the_data);

	ret = tsl2571_dev_init();
	if (ret) {
		dprintk(KERN_ALERT "%s: tsl2571-ALS device init failed.\n",
			__func__);
		goto kfree_exit;
	}
	/*********************** interrupt & workqueue ***********************/
	INIT_DELAYED_WORK(&the_data->work, tsl2571_schedwork);
	atomic_set(&the_data->delay, TSL2571_SCHE_DELAY);
	ret = request_irq(the_data->client->irq, tsl2571_irq_handler,
				IRQF_TRIGGER_RISING, "tsl2571", NULL);
	if (ret)
		dprintk("could not request irq =%d\n", the_data->client->irq);
	/********************** input ****************************/
	idev = input_allocate_device();
	if (!idev) {
		dprintk(KERN_ALERT
			"%s: tsl2571-ALS allocate input device failed.\n",
			__func__);
		goto kfree_exit;
	}

	idev->name = TSL2571_DEVICE_NAME;
	idev->id.bustype = BUS_I2C;
	input_set_capability(idev, EV_ABS, ABS_MISC);
	input_set_abs_params(idev, ABS_MISC, 0, 64*1024, 0, 0);
	input_set_abs_params(idev, ABS_X, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(idev, ABS_Y, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(idev, ABS_Z, ABSMIN, ABSMAX, 0, 0);

	the_data->input_dev = idev;
	input_set_drvdata(idev, the_data);

	ret = input_register_device(idev);
	if (ret < 0) {
		input_free_device(idev);
		goto kfree_exit;
	}

	mutex_init(&the_data->als_mutex);
	/* register the attributes */
	ret = sysfs_create_group(&the_data->input_dev->dev.kobj,
			&tsl2571_attribute_group);
	if (ret < 0)
		goto error_sysfs;
	return 0;

error_sysfs:
	sysfs_remove_group(&the_data->input_dev->dev.kobj,
						&tsl2571_attribute_group);
	input_unregister_device(the_data->input_dev);
	input_free_device(idev);
kfree_exit:
	kfree(the_data);
	return ret;

}
static int tsl2571_remove(struct i2c_client *client)
{
	int ret = tsl2571_chip_off();
	if (ret != 0) {
		dprintk("Faile %s\n", __func__);
		return ret;
	}
	free_irq(the_data->client->irq, NULL);
	cancel_delayed_work_sync(&the_data->work);
	sysfs_remove_group(&the_data->input_dev->dev.kobj,
			&tsl2571_attribute_group);
	input_unregister_device(the_data->input_dev);
	input_free_device(the_data->input_dev);
	kfree(the_data);
	return 0;
}

static const struct i2c_device_id tsl2571_id[] = {
	{ TSL2571_DEVICE_NAME, 0 },
	{}

};

static struct i2c_driver tsl2571_driver = {
	.probe	= tsl2571_probe,
	.remove	= tsl2571_remove,
	.id_table = tsl2571_id,
	.driver	= {
		.owner = THIS_MODULE,
		.name  = TSL2571_DEVICE_NAME,
	},
};
static int __init tsl2571_driver_init(void){
	return i2c_add_driver(&tsl2571_driver);
}
static void __exit tsl2571_driver_exit(void){
	i2c_del_driver(&tsl2571_driver);
}
MODULE_AUTHOR("Amlogic 2016");
MODULE_DESCRIPTION("TSL2571-ALS Driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(DRIVER_VERSION);

module_init(tsl2571_driver_init);
module_exit(tsl2571_driver_exit);

