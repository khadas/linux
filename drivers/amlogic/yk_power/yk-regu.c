/*
 * Regulators driver for YEKER
 *
 * Copyright (C) 2013 yeker, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/module.h>
#include <linux/version.h>


#include "yk-cfg.h"
#include "yk-regu.h"


static inline struct device *to_yk_dev(struct regulator_dev *rdev)
{
	return rdev_get_dev(rdev)->parent->parent;
}

static inline int check_range(struct yk_regulator_info *info,
				int min_uV, int max_uV)
{
	if (min_uV < info->min_uV || min_uV > info->max_uV)
		return -EINVAL;

	return 0;
}


/* yk common operations */
static int yk_set_voltage(struct regulator_dev *rdev,
				  int min_uV, int max_uV,unsigned *selector)
{
	struct yk_regulator_info *info = rdev_get_drvdata(rdev);
	struct device *yk_dev = to_yk_dev(rdev);
	uint8_t val, mask;

  printk("[YK618-reg] yk_set_voltage 6666!\n");
	if (check_range(info, min_uV, max_uV)) {
		pr_err("invalid voltage range (%d, %d) uV\n", min_uV, max_uV);
		return -EINVAL;
	}
	if ((info->switch_uV != 0) && (info->step2_uV!= 0) &&
	(info->new_level_uV != 0) && (min_uV > info->switch_uV)) {
		val = (info->switch_uV- info->min_uV + info->step1_uV - 1) / info->step1_uV;
		if (min_uV <= info->new_level_uV){
			val += 1;
		} else {
			val += (min_uV - info->new_level_uV) / info->step2_uV;
			val += 1;
		}
		mask = ((1 << info->vol_nbits) - 1)  << info->vol_shift;
	} else if ((info->switch_uV != 0) && (info->step2_uV!= 0) &&
	(min_uV > info->switch_uV) && (info->new_level_uV == 0)) {
		val = (info->switch_uV- info->min_uV + info->step1_uV - 1) / info->step1_uV;
		val += (min_uV - info->switch_uV) / info->step2_uV;
		mask = ((1 << info->vol_nbits) - 1)  << info->vol_shift;
	} else {
		val = (min_uV - info->min_uV + info->step1_uV - 1) / info->step1_uV;
		val <<= info->vol_shift;
		mask = ((1 << info->vol_nbits) - 1)  << info->vol_shift;
	}
	//if(info->vol_reg==0x25 || info->vol_reg==0x10){
		printk("[YK618-reg] yk_regulator_set_voltage reg=0x%x,val=0x%x!\n",info->vol_reg,val);
	//}
	return yk_update(yk_dev, info->vol_reg, val, mask);
}

static int yk_get_voltage(struct regulator_dev *rdev)
{
	struct yk_regulator_info *info = rdev_get_drvdata(rdev);
	struct device *yk_dev = to_yk_dev(rdev);
	uint8_t val, mask;
	int ret, switch_val, vol;
  printk("[YK618-reg] yk_get_voltage 6666!\n");
	ret = yk_read(yk_dev, info->vol_reg, &val);
	if (ret)
		return ret;

	mask = ((1 << info->vol_nbits) - 1)  << info->vol_shift;
	switch_val = ((info->switch_uV- info->min_uV + info->step1_uV - 1) / info->step1_uV);
	val = (val & mask) >> info->vol_shift;

	if ((info->switch_uV != 0) && (info->step2_uV!= 0) &&
	(val > switch_val) && (info->new_level_uV != 0)) {
		val -= switch_val;
		vol = info->new_level_uV + info->step2_uV * val;
	} else if ((info->switch_uV != 0) && (info->step2_uV!= 0) &&
	(val > switch_val) && (info->new_level_uV == 0)) {
		val -= switch_val;
		vol = info->switch_uV + info->step2_uV * val;
	} else {
		vol = info->min_uV + info->step1_uV * val;
	}

	return vol;

}

static int yk_enable(struct regulator_dev *rdev)
{
	struct yk_regulator_info *info = rdev_get_drvdata(rdev);
	struct device *yk_dev = to_yk_dev(rdev);
  printk("[YK618-reg] yk_enable 6666!\n");
	return yk_set_bits(yk_dev, info->enable_reg,
					1 << info->enable_bit);
}

static int yk_disable(struct regulator_dev *rdev)
{
	struct yk_regulator_info *info = rdev_get_drvdata(rdev);
	struct device *yk_dev = to_yk_dev(rdev);
 printk("[YK618-reg] yk_disable 77777!\n");
	return yk_clr_bits(yk_dev, info->enable_reg,
					1 << info->enable_bit);
}

static int yk_is_enabled(struct regulator_dev *rdev)
{
	struct yk_regulator_info *info = rdev_get_drvdata(rdev);
	struct device *yk_dev = to_yk_dev(rdev);
	uint8_t reg_val;
	int ret;
  printk("[YK618-reg] yk_is_enabled 6666!\n");
	ret = yk_read(yk_dev, info->enable_reg, &reg_val);
	if (ret)
		return ret;

	return !!(reg_val & (1 << info->enable_bit));
}

static int yk_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	struct yk_regulator_info *info = rdev_get_drvdata(rdev);
	int ret;

	ret = info->min_uV + info->step1_uV * selector;
	if ((info->switch_uV != 0) && (info->step2_uV != 0) &&
	(ret > info->switch_uV) && (info->new_level_uV != 0)) {
		selector -= ((info->switch_uV-info->min_uV)/info->step1_uV);
		ret = info->new_level_uV + info->step2_uV * selector;
	} else if ((info->switch_uV != 0) && (info->step2_uV != 0) &&
	(ret > info->switch_uV) && (info->new_level_uV == 0)) {
		selector -= ((info->switch_uV-info->min_uV)/info->step1_uV);
		ret = info->switch_uV + info->step2_uV * selector;
	}
	if (ret > info->max_uV)
		return -EINVAL;
	return ret;
}

static int yk_set_suspend_voltage(struct regulator_dev *rdev, int uV)
{
	return yk_set_voltage(rdev, uV, uV,NULL);
}


static struct regulator_ops yk_ops = {
	.set_voltage	= yk_set_voltage,
	.get_voltage	= yk_get_voltage,
	.list_voltage	= yk_list_voltage,
	.enable		= yk_enable,
	.disable	= yk_disable,
	.is_enabled	= yk_is_enabled,
	.set_suspend_enable		= yk_enable,
	.set_suspend_disable	= yk_disable,
	.set_suspend_voltage	= yk_set_suspend_voltage,
};


static int yk_ldoio01_enable(struct regulator_dev *rdev)
{
	struct yk_regulator_info *info = rdev_get_drvdata(rdev);
	struct device *yk_dev = to_yk_dev(rdev);

	 yk_set_bits(yk_dev, info->enable_reg,0x03);
	 return yk_clr_bits(yk_dev, info->enable_reg,0x04);
}

static int yk_ldoio01_disable(struct regulator_dev *rdev)
{
	struct yk_regulator_info *info = rdev_get_drvdata(rdev);
	struct device *yk_dev = to_yk_dev(rdev);

	return yk_clr_bits(yk_dev, info->enable_reg,0x07);
}

static int yk_ldoio01_is_enabled(struct regulator_dev *rdev)
{
	struct yk_regulator_info *info = rdev_get_drvdata(rdev);
	struct device *yk_dev = to_yk_dev(rdev);
	uint8_t reg_val;
	int ret;

	ret = yk_read(yk_dev, info->enable_reg, &reg_val);
	if (ret)
		return ret;

	return (((reg_val &= 0x07)== 0x03)?1:0);
}

static struct regulator_ops yk_ldoio01_ops = {
	.set_voltage	= yk_set_voltage,
	.get_voltage	= yk_get_voltage,
	.list_voltage	= yk_list_voltage,
	.enable		= yk_ldoio01_enable,
	.disable	= yk_ldoio01_disable,
	.is_enabled	= yk_ldoio01_is_enabled,
	.set_suspend_enable		= yk_ldoio01_enable,
	.set_suspend_disable	= yk_ldoio01_disable,
	.set_suspend_voltage	= yk_set_suspend_voltage,
};


#define YK618_LDO(_id, min, max, step1, vreg, shift, nbits, ereg, ebit, switch_vol, step2, new_level)	\
	YK_LDO(YK, _id, min, max, step1, vreg, shift, nbits, ereg, ebit, switch_vol, step2, new_level)

#define YK618_DCDC(_id, min, max, step1, vreg, shift, nbits, ereg, ebit, switch_vol, step2, new_level)	\
	YK_DCDC(YK, _id, min, max, step1, vreg, shift, nbits, ereg, ebit, switch_vol, step2, new_level)



static struct yk_regulator_info yk_regulator_info[] = {
	YK618_LDO(RTC,	3000,	3000,	0,	LDO1,	0,	0,	LDO1EN,	0, 0, 0, 0),//ldo1 for rtc
	YK618_LDO(ALDO1,	700,	3300,	100,	LDO2,	0,	5,	LDO2EN,	6, 0, 0, 0),//ldo2 for aldo1
	YK618_LDO(ALDO2,	700,	3300,	100,	LDO3,	0,	5,	LDO3EN,	7, 0, 0, 0),//ldo3 for aldo2
	YK618_LDO(ALDO3,	700,	3300,	100,	LDO4,	0,	5,	LDO4EN,	5, 0, 0, 0),//ldo3 for aldo3

	YK618_LDO(ELDO1,	700,	3300,	100,	LDO9,	0,	5,	LDO9EN,	0, 0, 0, 0),//ldo9 for eldo1
	YK618_LDO(ELDO2,	700,	3300,	100,	LDO10,	0,	5,	LDO10EN,1, 0, 0, 0),//ldo10 for eldo2

	YK618_DCDC(1,	1600,	3400,	100,	DCDC1,	0,	5,	DCDC1EN,1, 0, 0, 0),//buck1 for io
	YK618_DCDC(2,	600,	1540,	20,	DCDC2,	0,	6,	DCDC2EN,2, 0, 0, 0),//buck2 for cpu
	YK618_DCDC(3,	600,	1860,	20,	DCDC3,	0,	6,	DCDC3EN,3, 0, 0, 0),//buck3 for gpu
	YK618_DCDC(4,	600,	2600,	20,	DCDC4,	0,	6,	DCDC4EN,4, 1540, 100, 1800),//buck4 for core
	YK618_DCDC(5,	1000,	2550,	50,	DCDC5,	0,	5,	DCDC5EN,5, 0, 0, 0),//buck5 for ddr

	YK618_LDO(LDOIO1,	700,	3300,	100,	LDOIO1,	0,	5,	LDOIO1EN,0, 0, 0, 0),//ldoio1
};

static ssize_t workmode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct regulator_dev *rdev = dev_get_drvdata(dev);
	struct yk_regulator_info *info = rdev_get_drvdata(rdev);
	struct device *yk_dev = to_yk_dev(rdev);
	int ret;
	uint8_t val;
	ret = yk_read(yk_dev, YK_BUCKMODE, &val);
	if (ret)
		return sprintf(buf, "IO ERROR\n");

	if(info->desc.id == YK_ID_DCDC1){
		switch (val & 0x04) {
			case 0:return sprintf(buf, "AUTO\n");
			case 4:return sprintf(buf, "PWM\n");
			default:return sprintf(buf, "UNKNOWN\n");
		}
	}
	else if(info->desc.id == YK_ID_DCDC2){
		switch (val & 0x02) {
			case 0:return sprintf(buf, "AUTO\n");
			case 2:return sprintf(buf, "PWM\n");
			default:return sprintf(buf, "UNKNOWN\n");
		}
	}
	else if(info->desc.id == YK_ID_DCDC3){
		switch (val & 0x02) {
			case 0:return sprintf(buf, "AUTO\n");
			case 2:return sprintf(buf, "PWM\n");
			default:return sprintf(buf, "UNKNOWN\n");
		}
	}
	else if(info->desc.id == YK_ID_DCDC4){
		switch (val & 0x02) {
			case 0:return sprintf(buf, "AUTO\n");
			case 2:return sprintf(buf, "PWM\n");
			default:return sprintf(buf, "UNKNOWN\n");
		}
	}
	else if(info->desc.id == YK_ID_DCDC5){
		switch (val & 0x02) {
			case 0:return sprintf(buf, "AUTO\n");
			case 2:return sprintf(buf, "PWM\n");
			default:return sprintf(buf, "UNKNOWN\n");
		}
	}
	else
		return sprintf(buf, "IO ID ERROR\n");
}

static ssize_t workmode_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct regulator_dev *rdev = dev_get_drvdata(dev);
	struct yk_regulator_info *info = rdev_get_drvdata(rdev);
	struct device *yk_dev = to_yk_dev(rdev);
	char mode;
	uint8_t val;
	if(  buf[0] > '0' && buf[0] < '9' )// 1/AUTO: auto mode; 2/PWM: pwm mode;
		mode = buf[0];
	else
		mode = buf[1];

	switch(mode){
	 case 'U':
	 case 'u':
	 case '1':
		val = 0;break;
	 case 'W':
	 case 'w':
	 case '2':
	 	val = 1;break;
	 default:
	    val =0;
	}

	if(info->desc.id == YK_ID_DCDC1){
		if(val)
			yk_set_bits(yk_dev, YK_BUCKMODE,0x01);
		else
			yk_clr_bits(yk_dev, YK_BUCKMODE,0x01);
	}
	else if(info->desc.id == YK_ID_DCDC2){
		if(val)
			yk_set_bits(yk_dev, YK_BUCKMODE,0x02);
		else
			yk_clr_bits(yk_dev, YK_BUCKMODE,0x02);
	}
	else if(info->desc.id == YK_ID_DCDC3){
		if(val)
			yk_set_bits(yk_dev, YK_BUCKMODE,0x04);
		else
			yk_clr_bits(yk_dev, YK_BUCKMODE,0x04);
	}
	else if(info->desc.id == YK_ID_DCDC4){
		if(val)
			yk_set_bits(yk_dev, YK_BUCKMODE,0x08);
		else
			yk_clr_bits(yk_dev, YK_BUCKMODE,0x08);
	}
	else if(info->desc.id == YK_ID_DCDC5){
		if(val)
			yk_set_bits(yk_dev, YK_BUCKMODE,0x10);
		else
			yk_clr_bits(yk_dev, YK_BUCKMODE,0x10);
	}
	return count;
}

static ssize_t frequency_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct regulator_dev *rdev = dev_get_drvdata(dev);
	struct device *yk_dev = to_yk_dev(rdev);
	int ret;
	uint8_t val;
	ret = yk_read(yk_dev, YK_BUCKFREQ, &val);
	if (ret)
		return ret;
	ret = val & 0x0F;
	return sprintf(buf, "%d\n",(ret*75 + 750));
}

static ssize_t frequency_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct regulator_dev *rdev = dev_get_drvdata(dev);
	struct device *yk_dev = to_yk_dev(rdev);
	uint8_t val,tmp;
	int var;
	var = simple_strtoul(buf, NULL, 10);
	if(var < 750)
		var = 750;
	if(var > 1875)
		var = 1875;

	val = (var -750)/75;
	val &= 0x0F;

	yk_read(yk_dev, YK_BUCKFREQ, &tmp);
	tmp &= 0xF0;
	val |= tmp;
	yk_write(yk_dev, YK_BUCKFREQ, val);
	return count;
}


static struct device_attribute yk_regu_attrs[] = {
	YK_REGU_ATTR(workmode),
	YK_REGU_ATTR(frequency),
};

int yk_regu_create_attrs(struct platform_device *pdev)
{
	int j,ret;
	for (j = 0; j < ARRAY_SIZE(yk_regu_attrs); j++) {
		ret = device_create_file(&pdev->dev,&yk_regu_attrs[j]);
		if (ret)
			goto sysfs_failed;
	}
    goto succeed;

sysfs_failed:
	while (j--)
		device_remove_file(&pdev->dev,&yk_regu_attrs[j]);
succeed:
	return ret;
}

static inline struct yk_regulator_info *find_regulator_info(int id)
{
	struct yk_regulator_info *ri;
	int i;

	for (i = 0; i < ARRAY_SIZE(yk_regulator_info); i++) {
		ri = &yk_regulator_info[i];
		if (ri->desc.id == id)
			return ri;
	}
	return NULL;
}

static int  yk_regulator_probe(struct platform_device *pdev)
{
	uint8_t val;
	struct regulator_dev *rdev;
# if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)	
	struct regulator_config config = { };
#endif	
	struct  yk_reg_init * platform_data = (struct  yk_reg_init *)(pdev->dev.platform_data);
	//struct yk_regulator_info *info = platform_data->info;
	struct yk_regulator_info *ri = NULL;
	int ret;
	
	printk("[YK618-MFD] yk_regulator_probe start444!\n");
       printk("[YK618-MFD] yk_regulator_probe start6666!\n");

	ri = find_regulator_info(pdev->id);

	if (ri == NULL) {
		dev_err(&pdev->dev, "invalid regulator ID specified\n");
			printk("[YK618-MFD] ri == NULL\n");
		return -EINVAL;
	}
	printk("[YK618-MFD] ri->desc.id=%d\n!",ri->desc.id);
	if (ri->desc.id == YK_ID_RTC || ri->desc.id == YK_ID_ALDO1 \
		|| ri->desc.id == YK_ID_ALDO2 || ri->desc.id == YK_ID_ALDO3 \
		|| ri->desc.id == YK_ID_ELDO1 || ri->desc.id == YK_ID_ELDO2 \
		|| ri->desc.id == YK_ID_DCDC1 || ri->desc.id == YK_ID_DCDC2 \
		|| ri->desc.id == YK_ID_DCDC3 || ri->desc.id == YK_ID_DCDC4 \
		|| ri->desc.id == YK_ID_DCDC5 )
		ri->desc.ops = &yk_ops;
	if (ri->desc.id == YK_ID_LDOIO1 )
		ri->desc.ops = &yk_ldoio01_ops;


	
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
        rdev = regulator_register(&ri->desc, &pdev->dev, pdev->dev.platform_data, ri);
#endif

# if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
	rdev = regulator_register(&ri->desc, &pdev->dev, pdev->dev.platform_data, ri, NULL);
#else
	    config.dev = &pdev->dev;
		config.init_data = &(platform_data->yk_reg_init_data);
		config.driver_data = ri;

	rdev = regulator_register(&ri->desc, &config);
#endif
//	ri->desc.irq = 32;
	if (IS_ERR(rdev)) {
		dev_err(&pdev->dev, "failed to register regulator %s\n",
				ri->desc.name);
		return PTR_ERR(rdev);
	}
	printk("[YK618-MFD] yk_regulator_probe test id=%d!\n",ri->desc.id);
	platform_set_drvdata(pdev, rdev);


	
	struct device *yk_dev = to_yk_dev(rdev);
	//printk("\nwrite test 2222 \n"); 
	//yk_write(yk_dev, 0x10, 0xef);
	ret = yk_read(yk_dev, 0x10, &val);
	printk("\n\nguojunbin add in probe 1111 for read reg:10h = %x\n\n",val); 

	if(ri->desc.id == YK_ID_DCDC1 ||ri->desc.id == YK_ID_DCDC2 \
		|| ri->desc.id == YK_ID_DCDC3 ||ri->desc.id == YK_ID_DCDC4 \
		|| ri->desc.id == YK_ID_DCDC5){
		ret = yk_regu_create_attrs(pdev);
		printk("\n\n guojunbin add for yk_regu_create_attrs success!!\n\n\n");
		if(ret){
			return ret;
		}
	}
		printk("\n\n guojunbin add for yk_regulator_probe success end!!\n\n\n");
	return 0;
}

static int  yk_regulator_remove(struct platform_device *pdev)
{
	struct regulator_dev *rdev = platform_get_drvdata(pdev);

	regulator_unregister(rdev);
	return 0;
}

static struct platform_driver yk_regulator_driver = {
	.driver	= {
		.name	= "yk618-regulator",
		.owner	= THIS_MODULE,
	},
	.probe		= yk_regulator_probe,
	.remove		= yk_regulator_remove,
};

static int __init yk_regulator_init(void)
{
	return platform_driver_register(&yk_regulator_driver);
}
subsys_initcall(yk_regulator_init);

static void __exit yk_regulator_exit(void)
{
	platform_driver_unregister(&yk_regulator_driver);
}
module_exit(yk_regulator_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yeker");
MODULE_DESCRIPTION("Regulator Driver for YEKER PMIC");
MODULE_ALIAS("platform:yk-regulator");
