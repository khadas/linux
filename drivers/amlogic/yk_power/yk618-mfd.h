#include "yk-rw.h"
#include "yk-cfg.h"


static int  yk_init_chip(struct yk_mfd_chip *chip)
{
	uint8_t chip_id;
	uint8_t v[19] = {0xd8,YK_INTEN2, 0xff,YK_INTEN3,0x03,
						  YK_INTEN4, 0x01,YK_INTEN5, 0x00,
						  YK_INTSTS1,0xff,YK_INTSTS2, 0xff,
						  YK_INTSTS3,0xff,YK_INTSTS4, 0xff,
						  YK_INTSTS5,0xff};
	int err;

	/*read chip id*/	//???which int should enable must check with SD4
	err =  __yk_read(chip->client, YK_IC_TYPE, &chip_id);
	if (err) {
	    printk("[YK618-MFD] try to read chip id failed!\n");
		return err;
	}

	/*enable irqs and clear*/
	err =  __yk_writes(chip->client, YK_INTEN1, 19, v);
	if (err) {
	    printk("[YK618-MFD] try to clear irq failed!\n");
		return err;
	}

	dev_info(chip->dev, "YK (CHIP ID: 0x%02x) detected\n", chip_id);
	chip->type = YK618;
	
	err =  __yk_write(chip->client, 0x81, 0xc1);
	if (err) {
	    printk("[YK618-MFD] try 0x81 failed!\n");
		return err;
	}
	err =  __yk_read(chip->client, 0x81, &chip_id);
	if (err) {
	    //printk("[YK618-MFD] read 0x81 0x%02x!\n",chip_id);
		return err;
	}
	printk("[YK618-MFD] read 0x81 0x%02x!\n",chip_id);

	/* mask and clear all IRQs */
	chip->irqs_enabled = 0xffffffff | (uint64_t)0xff << 32;
	chip->ops->disable_irqs(chip, chip->irqs_enabled);

	return 0;
}

static int yk_disable_irqs(struct yk_mfd_chip *chip, uint64_t irqs)
{
	uint8_t v[9];
	int ret;

	chip->irqs_enabled &= ~irqs;

	v[0] = ((chip->irqs_enabled) & 0xff);
	v[1] = YK_INTEN2;
	v[2] = ((chip->irqs_enabled) >> 8) & 0xff;
	v[3] = YK_INTEN3;
	v[4] = ((chip->irqs_enabled) >> 16) & 0xff;
	v[5] = YK_INTEN4;
	v[6] = ((chip->irqs_enabled) >> 24) & 0xff;
	v[7] = YK_INTEN5;
	v[8] = ((chip->irqs_enabled) >> 32) & 0xff;
	ret =  __yk_writes(chip->client, YK_INTEN1, 9, v);

	return ret;
}

static int yk_enable_irqs(struct yk_mfd_chip *chip, uint64_t irqs)
{
	uint8_t v[9];
	int ret;

	chip->irqs_enabled |=  irqs;

	v[0] = ((chip->irqs_enabled) & 0xff);
	v[1] = YK_INTEN2;
	v[2] = ((chip->irqs_enabled) >> 8) & 0xff;
	v[3] = YK_INTEN3;
	v[4] = ((chip->irqs_enabled) >> 16) & 0xff;
	v[5] = YK_INTEN4;
	v[6] = ((chip->irqs_enabled) >> 24) & 0xff;
	v[7] = YK_INTEN5;
	v[8] = ((chip->irqs_enabled) >> 32) & 0xff;
	ret =  __yk_writes(chip->client, YK_INTEN1, 9, v);

	return ret;
}

static int yk_read_irqs(struct yk_mfd_chip *chip, uint64_t *irqs)
{
	uint8_t v[5] = {0, 0, 0, 0, 0};
	int ret;
	ret =  __yk_reads(chip->client, YK_INTSTS1, 5, v);
	if (ret < 0)
		return ret;
	
	*irqs =(((uint64_t) v[4]) << 32) |(((uint64_t) v[3]) << 24) | (((uint64_t) v[2])<< 16) | (((uint64_t)v[1]) << 8) | ((uint64_t) v[0]);
	printk("!!!!!!!!!!!!!!!yk_read_irqs irqs=0x%llx\n",*irqs);
	return 0;
}


static ssize_t yk_offvol_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
    uint8_t val = 0;
	yk_read(dev,YK_VOFF_SET,&val);
	return sprintf(buf,"%d\n",(val & 0x07) * 100 + 2600);
}

static ssize_t yk_offvol_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int tmp;
	uint8_t val;
	tmp = simple_strtoul(buf, NULL, 10);
	if (tmp < 2600)
		tmp = 2600;
	if (tmp > 3300)
		tmp = 3300;

	yk_read(dev,YK_VOFF_SET,&val);
	val &= 0xf8;
	val |= ((tmp - 2600) / 100);
	yk_write(dev,YK_VOFF_SET,val);
	return count;
}

static ssize_t yk_noedelay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
    uint8_t val;
	yk_read(dev,YK_OFF_CTL,&val);
	if( (val & 0x03) == 0)
		return sprintf(buf,"%d\n",128);
	else
		return sprintf(buf,"%d\n",(val & 0x03) * 1000);
}

static ssize_t yk_noedelay_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int tmp;
	uint8_t val;
	tmp = simple_strtoul(buf, NULL, 10);
	if (tmp < 1000)
		tmp = 128;
	if (tmp > 3000)
		tmp = 3000;
	yk_read(dev,YK_OFF_CTL,&val);
	val &= 0xfc;
	val |= ((tmp) / 1000);
	yk_write(dev,YK_OFF_CTL,val);
	return count;
}

static ssize_t yk_pekopen_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
    uint8_t val;
	int tmp = 0;
	yk_read(dev,YK_POK_SET,&val);
	switch(val >> 6){
		case 0: tmp = 128;break;
		case 1: tmp = 3000;break;
		case 2: tmp = 1000;break;
		case 3: tmp = 2000;break;
		default:
			tmp = 0;break;
	}
	return sprintf(buf,"%d\n",tmp);
}

static ssize_t yk_pekopen_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int tmp;
	uint8_t val;
	tmp = simple_strtoul(buf, NULL, 10);
	yk_read(dev,YK_POK_SET,&val);
	if (tmp < 1000)
		val &= 0x3f;
	else if(tmp < 2000){
		val &= 0x3f;
		val |= 0x80;
	}
	else if(tmp < 3000){
		val &= 0x3f;
		val |= 0xc0;
	}
	else {
		val &= 0x3f;
		val |= 0x40;
	}
	yk_write(dev,YK_POK_SET,val);
	return count;
}

static ssize_t yk_peklong_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
    uint8_t val = 0;
	yk_read(dev,YK_POK_SET,&val);
	return sprintf(buf,"%d\n",((val >> 4) & 0x03) * 500 + 1000);
}

static ssize_t yk_peklong_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int tmp;
	uint8_t val;
	tmp = simple_strtoul(buf, NULL, 10);
	if(tmp < 1000)
		tmp = 1000;
	if(tmp > 2500)
		tmp = 2500;
	yk_read(dev,YK_POK_SET,&val);
	val &= 0xcf;
	val |= (((tmp - 1000) / 500) << 4);
	yk_write(dev,YK_POK_SET,val);
	return count;
}

static ssize_t yk_peken_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
    uint8_t val;
	yk_read(dev,YK_POK_SET,&val);
	return sprintf(buf,"%d\n",((val >> 3) & 0x01));
}

static ssize_t yk_peken_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int tmp;
	uint8_t val;
	tmp = simple_strtoul(buf, NULL, 10);
	if(tmp)
		tmp = 1;
	yk_read(dev,YK_POK_SET,&val);
	val &= 0xf7;
	val |= (tmp << 3);
	yk_write(dev,YK_POK_SET,val);
	return count;
}

static ssize_t yk_pekdelay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
    uint8_t val;
	yk_read(dev,YK_POK_SET,&val);

	return sprintf(buf,"%d\n",((val >> 2) & 0x01)? 64:8);
}

static ssize_t yk_pekdelay_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int tmp;
	uint8_t val;
	tmp = simple_strtoul(buf, NULL, 10);
	if(tmp <= 8)
		tmp = 0;
	else
		tmp = 1;
	yk_read(dev,YK_POK_SET,&val);
	val &= 0xfb;
	val |= tmp << 2;
	yk_write(dev,YK_POK_SET,val);
	return count;
}

static ssize_t yk_pekclose_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
    uint8_t val;
	yk_read(dev,YK_POK_SET,&val);
	return sprintf(buf,"%d\n",((val & 0x03) * 2000) + 4000);
}

static ssize_t yk_pekclose_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int tmp;
	uint8_t val;
	tmp = simple_strtoul(buf, NULL, 10);
	if(tmp < 4000)
		tmp = 4000;
	if(tmp > 10000)
		tmp =10000;
	tmp = (tmp - 4000) / 2000 ;
	yk_read(dev,YK_POK_SET,&val);
	val &= 0xfc;
	val |= tmp ;
	yk_write(dev,YK_POK_SET,val);
	return count;
}

static ssize_t yk_ovtemclsen_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
    uint8_t val;
	yk_read(dev,YK_HOTOVER_CTL,&val);
	return sprintf(buf,"%d\n",((val >> 2) & 0x01));
}

static ssize_t yk_ovtemclsen_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int tmp;
	uint8_t val;
	tmp = simple_strtoul(buf, NULL, 10);
	if(tmp)
		tmp = 1;
	yk_read(dev,YK_HOTOVER_CTL,&val);
	val &= 0xfb;
	val |= tmp << 2 ;
	yk_write(dev,YK_HOTOVER_CTL,val);
	return count;
}

static ssize_t yk_reg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
    uint8_t val;
	yk_read(dev,yk_reg_addr,&val);
	return sprintf(buf,"REG[%x]=%x\n",yk_reg_addr,val);
}

static ssize_t yk_reg_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int tmp;
	uint8_t val;
	tmp = simple_strtoul(buf, NULL, 16);
	if( tmp < 256 )
		yk_reg_addr = tmp;
	else {
		val = tmp & 0x00FF;
		yk_reg_addr= (tmp >> 8) & 0x00FF;
		yk_write(dev,yk_reg_addr, val);
	}
	return count;
}

static ssize_t yk_regs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
  uint8_t val[2];
	yk_reads(dev,yk_reg_addr,2,val);
	return sprintf(buf,"REG[0x%x]=0x%x,REG[0x%x]=0x%x\n",yk_reg_addr,val[0],yk_reg_addr+1,val[1]);
}

static ssize_t yk_regs_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int tmp;
	uint8_t val[3];
	tmp = simple_strtoul(buf, NULL, 16);
	if( tmp < 256 )
		yk_reg_addr = tmp;
	else {
		yk_reg_addr= (tmp >> 16) & 0xFF;
		val[0] = (tmp >> 8) & 0xFF;
		val[1] = yk_reg_addr + 1;
		val[2] = tmp & 0xFF;
		yk_writes(dev,yk_reg_addr,3,val);
	}
	return count;
}

static struct device_attribute yk_mfd_attrs[] = {
	YK_MFD_ATTR(yk_offvol),
	YK_MFD_ATTR(yk_noedelay),
	YK_MFD_ATTR(yk_pekopen),
	YK_MFD_ATTR(yk_peklong),
	YK_MFD_ATTR(yk_peken),
	YK_MFD_ATTR(yk_pekdelay),
	YK_MFD_ATTR(yk_pekclose),
	YK_MFD_ATTR(yk_ovtemclsen),
	YK_MFD_ATTR(yk_reg),
	YK_MFD_ATTR(yk_regs),
};
