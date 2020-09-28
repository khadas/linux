/*
 * Battery charger driver for YEKER
 *
 * Copyright (C) 2013 yeker, Ltd.
 *  
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <asm/div64.h>
//#include <mach/gpio.h>

#include "yk-cfg.h"
#include "yk-sply.h"
#include "yk-rw.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif


#include <linux/gpio.h>

#define DBG_YK_PSY 1
#if  DBG_YK_PSY
#define DBG_PSY_MSG(format,args...)   printk( "[YK]"format,##args)
#else
#define DBG_PSY_MSG(format,args...)   do {} while (0)
#endif

static int yk_debug = 0;
static uint8_t yk_reg_addr = 0;
struct yk_adc_res adc;
struct delayed_work usbwork;
#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend yk_early_suspend;
int early_suspend_flag = 0;
#endif

int pmu_usbvolnew = 0;
int pmu_usbcurnew = 0;
int yk_usbcurflag = 0;
int yk_usbvolflag = 0;

int yk_chip_id_get(uint8_t chip_id[16])
{
	uint8_t ret;
	ret = yk_write(yk_charger->master,0xff,0x01);
	if(ret)
	{
		printk("[ykx] ykx write REG_ff fail!");
	}
	yk_reads(yk_charger->master,0x20,16,chip_id);
	if(ret)
	{
		printk("[ykx] ykx reads REG_12x fail!");
	}
	yk_write(yk_charger->master,0xff,0x00);
	if(ret)
	{
		printk("[ykx] ykx write REG_ff fail!");
	}

    return ret;
}
EXPORT_SYMBOL_GPL(yk_chip_id_get);
/*控制usb电压的开关，期望在外接usb host 充电时调用*/
int yk_usbvol(void)
{
	yk_usbvolflag = 1;
    return 0;
}
EXPORT_SYMBOL_GPL(yk_usbvol);

int yk_usb_det(void)
{
	uint8_t ret;
	yk_read(yk_charger->master,YK_CHARGE_STATUS,&ret);
	if(ret & 0x10)/*usb or usb adapter can be used*/
	{
		return 1;
	}
	else/*no usb or usb adapter*/
	{
		return 0;
	}
}
EXPORT_SYMBOL_GPL(yk_usb_det);

/*控制usb电流的开关，期望在外接usb host 充电时调用*/
int yk_usbcur(void)
{
    yk_usbcurflag = 1;
    return 0;
}
EXPORT_SYMBOL_GPL(yk_usbcur);
/*控制usb电压的开关，期望在从usb host 拔出时调用*/
int yk_usbvol_restore(void)
{
 	yk_usbvolflag = 0;
    return 0;
}
EXPORT_SYMBOL_GPL(yk_usbvol_restore);
/*控制usb电流的开关，期望在从usb host 拔出时调用*/
int yk_usbcur_restore(void)
{
	yk_usbcurflag = 0;
    return 0;
}
EXPORT_SYMBOL_GPL(yk_usbcur_restore);

/*调试接口*/
static ssize_t yk_reg_show(struct class *class, struct class_attribute *attr, char *buf)
{
    uint8_t val;
    yk_read(yk_charger->master,yk_reg_addr,&val);
    return sprintf(buf,"REG[%x]=%x\n",yk_reg_addr,val);
}

static ssize_t yk_reg_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int tmp;
    uint8_t val;
    tmp = simple_strtoul(buf, NULL, 16);
    if( tmp < 256 )
    	yk_reg_addr = tmp;
    else {
    	val = tmp & 0x00FF;
    	yk_reg_addr= (tmp >> 8) & 0x00FF;
    	yk_write(yk_charger->master,yk_reg_addr, val);
    }
    return count;
}

static ssize_t yk_regs_show(struct class *class, struct class_attribute *attr, char *buf)
{
    uint8_t val[4];
    yk_reads(yk_charger->master,yk_reg_addr,4,val);
    return sprintf(buf,"REG[0x%x]=0x%x,REG[0x%x]=0x%x,REG[0x%x]=0x%x,REG[0x%x]=0x%x\n",yk_reg_addr,val[0],yk_reg_addr+1,val[1],yk_reg_addr+2,val[2],yk_reg_addr+3,val[3]);
}

static ssize_t yk_regs_store(struct class *class,struct class_attribute *attr, const char *buf, size_t count)
{
	int tmp;
	uint8_t val[5];
	tmp = simple_strtoul(buf, NULL, 16);
	if( tmp < 256 )
        	yk_reg_addr = tmp;
	else {
		yk_reg_addr= (tmp >> 24) & 0xFF;
		val[0] = (tmp >> 16) & 0xFF;
		val[1] =  yk_reg_addr + 1;
		val[2] = (tmp >>  8)& 0xFF;
		val[3] =  yk_reg_addr + 2;
		val[4] = (tmp >>  0)& 0xFF;
		yk_writes(yk_charger->master,yk_reg_addr,5,val);
	}
	return count;
}

static ssize_t ykdebug_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    if(buf[0] == '1'){
        yk_debug = 1;
    }
    else{
        yk_debug = 0;
    }
    return count;
}

static ssize_t ykdebug_show(struct class *class,struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "bat-debug value is %d\n", yk_debug);
}

static uint8_t yk618_reg_addr;
static ssize_t yk618_read_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
	uint8_t reg_addr;
	reg_addr = simple_strtoul(buf, NULL, 16);
	yk618_reg_addr = reg_addr;
	return count;
}

static ssize_t yk618_read_show(struct class *class,struct class_attribute *attr, char *buf)
{
	int valtage = 0;
	uint8_t reg_addr, val;
	if (yk618_reg_addr == 0) {
		return sprintf(buf,"%s\n","please use the echo first");
	}
	reg_addr = yk618_reg_addr;
	yk618_reg_addr = 0;

	if (reg_addr == YK_DC2OUT_VOL || reg_addr ==YK_DC3OUT_VOL) {
		yk_read(yk_charger->master,reg_addr,&val);
	}
	else {
		return sprintf(buf,"%s\n","reg_addr error onle  support dcdc2/dcd3");
	}
	if (reg_addr ==YK_DC2OUT_VOL || reg_addr ==YK_DC3OUT_VOL) {
		valtage = ((val - 0x80) * 20) + 600;
    }
	return sprintf(buf, "%d\n", valtage);
}

static ssize_t yk618_write_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
	uint8_t reg_addr,valtage;
	char *str,*str_saved;
	char *str_addr,*str_val;
	str_saved = kstrdup(buf, GFP_KERNEL);
	str = str_saved;

	str_addr = strsep(&str," ");
	str_val = strsep(&str," ");

	reg_addr = simple_strtoul(str_addr, NULL, 16);
	valtage = simple_strtoul(str_val, NULL, 16);

	if (reg_addr == YK_DC2OUT_VOL || reg_addr ==YK_DC3OUT_VOL) {
		yk618_reg_addr = reg_addr;
		yk_write(yk_charger->master,reg_addr,valtage);
	}

	kfree(str_saved);
	return count;
}

static ssize_t yk618_write_show(struct class *class,struct class_attribute *attr, char *buf)
{
	uint8_t reg_addr, val;
	int valtage = 0;
	if (yk618_reg_addr == 0) {
		return sprintf(buf,"%s\n","please use the echo first");
	}
	reg_addr = yk618_reg_addr;
	yk618_reg_addr = 0;

	if (reg_addr == YK_DC2OUT_VOL || reg_addr ==YK_DC3OUT_VOL) {
		yk_read(yk_charger->master,reg_addr,&val);
    }
	else {
		return sprintf(buf,"%s\n","reg_addr error onle  support dcdc2/dcd3");
	}
    if (reg_addr ==YK_DC2OUT_VOL || reg_addr ==YK_DC3OUT_VOL) {
		valtage = ((val - 0x80) * 20) + 600;
	}

	return sprintf(buf, "%d\n", valtage);
}

static struct class_attribute ykpower_class_attrs[] = {
    __ATTR(ykdebug,S_IRUGO |S_IWUSR,ykdebug_show,ykdebug_store),
    __ATTR(ykreg,  S_IRUGO |S_IWUSR,yk_reg_show,yk_reg_store),
    __ATTR(ykregs, S_IRUGO |S_IWUSR,yk_regs_show,yk_regs_store),
    __ATTR(ykwrite, S_IRUGO |S_IWUSR,yk618_write_show,yk618_write_store),
    __ATTR(ykread, S_IRUGO |S_IWUSR,yk618_read_show,yk618_read_store),
    __ATTR_NULL
};

static struct class ykpower_class = {
    .name = "ykpower",
    .class_attrs = ykpower_class_attrs,
};

int ADC_Freq_Get(struct yk_charger *charger)
{
	uint8_t  temp;
	int  rValue = 25;

	yk_read(charger->master, YK_ADC_CONTROL3,&temp);
	temp &= 0xc0;
	switch(temp >> 6)
	{
		case 0:
			rValue = 100;
			break;
		case 1:
			rValue = 200;
			break;
		case 2:
			rValue = 400;
			break;
		case 3:
			rValue = 800;
			break;
		default:
			break;
	}
	return rValue;
}
static inline int yk_vts_to_mV(uint16_t reg)
{
    return ((int)(((reg >> 8) << 4 ) | (reg & 0x000F))) * 800 / 1000;
}
static inline int yk_vbat_to_mV(uint16_t reg)
{
  return ((int)((( reg >> 8) << 4 ) | (reg & 0x000F))) * 1100 / 1000;
}

static inline int yk_ocvbat_to_mV(uint16_t reg)
{
  return ((int)((( reg >> 8) << 4 ) | (reg & 0x000F))) * 1100 / 1000;
}


static inline int yk_vdc_to_mV(uint16_t reg)
{
  return ((int)(((reg >> 8) << 4 ) | (reg & 0x000F))) * 1700 / 1000;
}


static inline int yk_ibat_to_mA(uint16_t reg)
{
    return ((int)(((reg >> 8) << 4 ) | (reg & 0x000F))) ;
}

static inline int yk_icharge_to_mA(uint16_t reg)
{
    return ((int)(((reg >> 8) << 4 ) | (reg & 0x000F)));
}

static inline int yk_iac_to_mA(uint16_t reg)
{
    return ((int)(((reg >> 8) << 4 ) | (reg & 0x000F))) * 625 / 1000;
}

static inline int yk_iusb_to_mA(uint16_t reg)
{
    return ((int)(((reg >> 8) << 4 ) | (reg & 0x000F))) * 375 / 1000;
}


static inline void yk_read_adc(struct yk_charger *charger,
  struct yk_adc_res *adc)
{
  uint8_t tmp[8];
//
//  yk_reads(charger->master,YK_VACH_RES,8,tmp);
  adc->vac_res = 0;
  adc->iac_res = 0;
  adc->vusb_res = 0;
  adc->iusb_res = 0;
  yk_reads(charger->master,YK_VBATH_RES,6,tmp);
  adc->vbat_res = ((uint16_t) tmp[0] << 8 )| tmp[1];
  adc->ichar_res = ((uint16_t) tmp[2] << 8 )| tmp[3];
  adc->idischar_res = ((uint16_t) tmp[4] << 8 )| tmp[5];
  yk_reads(charger->master,YK_OCVBATH_RES,2,tmp);
  adc->ocvbat_res = ((uint16_t) tmp[0] << 8 )| tmp[1];
}


static void yk_charger_update_state(struct yk_charger *charger)
{
  uint8_t val[2];
  uint16_t tmp;
  struct i2c_client *client;
  int ret;

  client = to_i2c_client(charger->master);

  ret = yk_reads(charger->master,YK_CHARGE_STATUS,2,val);

  tmp = (val[1] << 8 )+ val[0];
  charger->is_on = (val[1] & YK_IN_CHARGE) ? 1 : 0;
  charger->fault = val[1];
  charger->bat_det = (tmp & YK_STATUS_BATEN)?1:0;
  charger->ac_det = (tmp & YK_STATUS_ACEN)?1:0;
  charger->usb_det = (tmp & YK_STATUS_USBEN)?1:0;
  charger->usb_valid = (tmp & YK_STATUS_USBVA)?1:0;
  charger->ac_valid = (tmp & YK_STATUS_ACVA)?1:0;
  charger->ext_valid = charger->ac_valid | charger->usb_valid;
  charger->bat_current_direction = (tmp & YK_STATUS_BATCURDIR)?1:0;
  charger->in_short = (tmp& YK_STATUS_ACUSBSH)?1:0;
  charger->batery_active = (tmp & YK_STATUS_BATINACT)?1:0;
  charger->int_over_temp = (tmp & YK_STATUS_ICTEMOV)?1:0;
  yk_read(charger->master,YK_CHARGE_CONTROL1,val);
  charger->charge_on = ((val[0] >> 7) & 0x01);
}

static void yk_charger_update(struct yk_charger *charger)
{
  uint16_t tmp;
  uint8_t val[2];
	//int bat_temp_mv;
  charger->adc = &adc;
  yk_read_adc(charger, &adc);
  tmp = charger->adc->vbat_res;
  charger->vbat = yk_vbat_to_mV(tmp);
  tmp = charger->adc->ocvbat_res;
  charger->ocv = yk_ocvbat_to_mV(tmp);
   //tmp = charger->adc->ichar_res + charger->adc->idischar_res;
  charger->ibat = ABS(yk_icharge_to_mA(charger->adc->ichar_res)-yk_ibat_to_mA(charger->adc->idischar_res));
  tmp = 00;
  charger->vac = yk_vdc_to_mV(tmp);
  tmp = 00;
  charger->iac = yk_iac_to_mA(tmp);
  tmp = 00;
  charger->vusb = yk_vdc_to_mV(tmp);
  tmp = 00;
  charger->iusb = yk_iusb_to_mA(tmp);
  yk_reads(charger->master,YK_INTTEMP,2,val);
  //DBG_PSY_MSG("TEMPERATURE:val1=0x%x,val2=0x%x\n",val[1],val[0]);
  tmp = (val[0] << 4 ) + (val[1] & 0x0F);
  charger->ic_temp = (int) tmp *1063/10000  - 2667/10;
  charger->disvbat =  charger->vbat;
  charger->disibat =  yk_ibat_to_mA(charger->adc->idischar_res);
}

#if defined  (CONFIG_YK_CHARGEINIT)
static void yk_set_charge(struct yk_charger *charger)
{
    uint8_t val=0x00;
    uint8_t tmp=0x00;
  	if(charger->chgvol < 4200000){
  		val &= ~(3 << 5);
  	}else if (charger->chgvol<4240000){
  		val &= ~(3 << 5);
  		val |= 1 << 5;
  	}else if (charger->chgvol<4350000){
  		val &= ~(3 << 5);
  		val |= 1 << 6;
  	}else
    {
  		val |= 3 << 5;
    }

		if(charger->chgcur == 0)
			charger->chgen = 0;

    if(charger->chgcur< 300000)
      charger->chgcur = 300000;
    else if(charger->chgcur > 2550000)
     charger->chgcur = 2550000;

    val |= (charger->chgcur - 300000) / 150000 ;
    if(charger ->chgend == 10){
      val &= ~(1 << 4);
    }
    else {
      val |= 1 << 4;
    }
    val &= 0x7F;
    val |= charger->chgen << 7;
      if(charger->chgpretime < 30)
      charger->chgpretime = 30;
    if(charger->chgcsttime < 360)
      charger->chgcsttime = 360;

    tmp = ((((charger->chgpretime - 40) / 10) << 6)  \
      | ((charger->chgcsttime - 360) / 120));
	yk_write(charger->master, YK_CHARGE_CONTROL1,val);
	yk_update(charger->master, YK_CHARGE_CONTROL2,tmp,0xC2);
}
#else
static void yk_set_charge(struct yk_charger *charger)
{

}
#endif

static enum power_supply_property yk_battery_props[] = {
  POWER_SUPPLY_PROP_MODEL_NAME,
  POWER_SUPPLY_PROP_STATUS,
  POWER_SUPPLY_PROP_PRESENT,
  POWER_SUPPLY_PROP_ONLINE,
  POWER_SUPPLY_PROP_HEALTH,
  POWER_SUPPLY_PROP_TECHNOLOGY,
  POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
  POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
  POWER_SUPPLY_PROP_VOLTAGE_NOW,
  POWER_SUPPLY_PROP_CURRENT_NOW,
  //POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
  //POWER_SUPPLY_PROP_CHARGE_FULL,
  POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
  POWER_SUPPLY_PROP_CAPACITY,
  //POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
  //POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
  //POWER_SUPPLY_PROP_TEMP,
};

static enum power_supply_property yk_ac_props[] = {
  POWER_SUPPLY_PROP_MODEL_NAME,
  POWER_SUPPLY_PROP_PRESENT,
  POWER_SUPPLY_PROP_ONLINE,
  POWER_SUPPLY_PROP_VOLTAGE_NOW,
  POWER_SUPPLY_PROP_CURRENT_NOW,
};

static enum power_supply_property yk_usb_props[] = {
  POWER_SUPPLY_PROP_MODEL_NAME,
  POWER_SUPPLY_PROP_PRESENT,
  POWER_SUPPLY_PROP_ONLINE,
  POWER_SUPPLY_PROP_VOLTAGE_NOW,
  POWER_SUPPLY_PROP_CURRENT_NOW,
};

static void yk_battery_check_status(struct yk_charger *charger,
            union power_supply_propval *val)
{
  if (charger->bat_det) {
    if (charger->ext_valid){
    	if( charger->rest_vol == 100)
        val->intval = POWER_SUPPLY_STATUS_FULL;
    	else if(charger->charge_on)
    		val->intval = POWER_SUPPLY_STATUS_CHARGING;
    	else
    		val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
    }
    else
      val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
  }
  else
    val->intval = POWER_SUPPLY_STATUS_FULL;
}

static void yk_battery_check_health(struct yk_charger *charger,
            union power_supply_propval *val)
{
    if (charger->fault & YK_FAULT_LOG_BATINACT)
    val->intval = POWER_SUPPLY_HEALTH_DEAD;
  else if (charger->fault & YK_FAULT_LOG_OVER_TEMP)
    val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
  else if (charger->fault & YK_FAULT_LOG_COLD)
    val->intval = POWER_SUPPLY_HEALTH_COLD;
  else
    val->intval = POWER_SUPPLY_HEALTH_GOOD;
}

static int yk_battery_get_property(struct power_supply *psy,
           enum power_supply_property psp,
           union power_supply_propval *val)
{
  struct yk_charger *charger;
  int ret = 0;
  //charger = container_of(&psy, struct yk_charger, batt);
  charger = power_supply_get_drvdata(psy);

  switch (psp) {
  case POWER_SUPPLY_PROP_STATUS:
    yk_battery_check_status(charger, val);
    break;
  case POWER_SUPPLY_PROP_HEALTH:
    yk_battery_check_health(charger, val);
    break;
  case POWER_SUPPLY_PROP_TECHNOLOGY:
    val->intval = charger->battery_info->technology;
    break;
  case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
    val->intval = charger->battery_info->voltage_max_design;
    break;
  case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
    val->intval = charger->battery_info->voltage_min_design;
    break;
  case POWER_SUPPLY_PROP_VOLTAGE_NOW:
    val->intval = charger->vbat * 1000;
    break;
  case POWER_SUPPLY_PROP_CURRENT_NOW:
    val->intval = charger->ibat * 1000;
    break;
  case POWER_SUPPLY_PROP_MODEL_NAME:
  val->strval = charger->batt_desc.name;
    break;
/*  case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
  case POWER_SUPPLY_PROP_CHARGE_FULL:
    val->intval = charger->battery_info->charge_full_design;
        break;
*/
  case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
  case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
    val->intval = charger->battery_info->energy_full_design;
  //  DBG_PSY_MSG("POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:%d\n",val->intval);
       break;
  case POWER_SUPPLY_PROP_CAPACITY:
    val->intval = charger->rest_vol;
    //printk("rest_vol = %d\n", charger->rest_vol);
    break;
/*  case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
    if(charger->bat_det && !(charger->is_on) && !(charger->ext_valid))
      val->intval = charger->rest_time;
    else
      val->intval = 0;
    break;
  case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
    if(charger->bat_det && charger->is_on)
      val->intval = charger->rest_time;
    else
      val->intval = 0;
    break;
*/
  case POWER_SUPPLY_PROP_ONLINE:
  {
    yk_charger_update_state(charger);
    val->intval = charger->bat_current_direction;
   // printk("yk battery hardware current direction %d\n", charger->bat_current_direction);
		if (charger->bat_temp > 50 || -5 < charger->bat_temp)
			val->intval = 0;
    break;
  }
  case POWER_SUPPLY_PROP_PRESENT:
    val->intval = charger->bat_det;
    break;
 // case POWER_SUPPLY_PROP_TEMP:
    //val->intval = charger->ic_temp - 200;
    //val->intval =  300;
//			val->intval =  charger->bat_temp * 10;

//    break;
  default:
    ret = -EINVAL;
    break;
  }

  return ret;
}

static int yk_ac_get_property(struct power_supply *psy,
           enum power_supply_property psp,
           union power_supply_propval *val)
{
  struct yk_charger *charger;
  int ret = 0;
  //charger = container_of(&psy, struct yk_charger, ac);
  charger = power_supply_get_drvdata(psy);

  switch(psp){
  case POWER_SUPPLY_PROP_MODEL_NAME:
    val->strval = charger->ac_desc.name;break;
  case POWER_SUPPLY_PROP_PRESENT:
    val->intval = charger->ac_det;
    break;
  case POWER_SUPPLY_PROP_ONLINE:
    val->intval = charger->ac_valid;break;
  case POWER_SUPPLY_PROP_VOLTAGE_NOW:
    val->intval = charger->vac * 1000;
    break;
  case POWER_SUPPLY_PROP_CURRENT_NOW:
    val->intval = charger->iac * 1000;
    break;
  default:
    ret = -EINVAL;
    break;
  }
   return ret;
}

static int yk_usb_get_property(struct power_supply *psy,
           enum power_supply_property psp,
           union power_supply_propval *val)
{
  struct yk_charger *charger;
  int ret = 0;
  //charger = container_of(&psy, struct yk_charger, usb);
  charger = power_supply_get_drvdata(psy);

  switch(psp){
  case POWER_SUPPLY_PROP_MODEL_NAME:
    val->strval = charger->usb_desc.name;break;
  case POWER_SUPPLY_PROP_PRESENT:
    val->intval = charger->usb_det;
    break;
  case POWER_SUPPLY_PROP_ONLINE:
    val->intval = charger->usb_valid;
    break;
  case POWER_SUPPLY_PROP_VOLTAGE_NOW:
    val->intval = charger->vusb * 1000;
    break;
  case POWER_SUPPLY_PROP_CURRENT_NOW:
    val->intval = charger->iusb * 1000;
    break;
  default:
    ret = -EINVAL;
    break;
  }
   return ret;
}

static char *supply_list[] = {
	"battery",
};

static void yk_battery_setup_psy(struct yk_charger *charger)
{
  struct power_supply_desc *batt_desc = &charger->batt_desc;
  struct power_supply_desc *ac_desc = &charger->ac_desc;
  struct power_supply_desc *usb_desc = &charger->usb_desc;
  struct power_supply_info *info = charger->battery_info;
  
   batt_desc->name = "battery";
   batt_desc->use_for_apm = info->use_for_apm;
   batt_desc->type = POWER_SUPPLY_TYPE_BATTERY;
   batt_desc->get_property = yk_battery_get_property;
   batt_desc->properties = yk_battery_props;
   batt_desc->num_properties = ARRAY_SIZE(yk_battery_props);

 
  ac_desc->name = "ac";
  ac_desc->type = POWER_SUPPLY_TYPE_MAINS;
  ac_desc->get_property = yk_ac_get_property;
  ac_desc->properties = yk_ac_props;
  ac_desc->num_properties = ARRAY_SIZE(yk_ac_props);

  

  usb_desc->name = "usb";
  usb_desc->type = POWER_SUPPLY_TYPE_USB;
  usb_desc->get_property = yk_usb_get_property;
  usb_desc->properties = yk_usb_props;
  usb_desc->num_properties = ARRAY_SIZE(yk_usb_props);
//  printk("\n\n\nguojunbin add for battery setup 4444\n\n\n");

};



#if defined  (CONFIG_YK_CHARGEINIT)
static int yk_battery_adc_set(struct yk_charger *charger)
{
   int ret ;
   uint8_t val;

  /*enable adc and set adc */
  val= YK_ADC_BATVOL_ENABLE | YK_ADC_BATCUR_ENABLE;

	ret = yk_update(charger->master, YK_ADC_CONTROL, val , val);
  if (ret)
  {
      return ret;
  }
  
  ret = yk_read(charger->master, YK_ADC_CONTROL3, &val);
  switch (charger->sample_time/100){
  case 1: val &= ~(3 << 6);break;
  case 2: val &= ~(3 << 6);val |= 1 << 6;break;
  case 4: val &= ~(3 << 6);val |= 2 << 6;break;
  case 8: val |= 3 << 6;break;
  default: break;
  }
  ret = yk_write(charger->master, YK_ADC_CONTROL3, val);
  if (ret)
    return ret;

  return 0;
}
#else
static int yk_battery_adc_set(struct yk_charger *charger)
{
  return 0;
}
#endif

static int yk_battery_first_init(struct yk_charger *charger)
{
   int ret;
   uint8_t val;
   yk_set_charge(charger);
   ret = yk_battery_adc_set(charger);
   if(ret)
    return ret;

   ret = yk_read(charger->master, YK_ADC_CONTROL3, &val);
   switch ((val >> 6) & 0x03){
  case 0: charger->sample_time = 100;break;
  case 1: charger->sample_time = 200;break;
  case 2: charger->sample_time = 400;break;
  case 3: charger->sample_time = 800;break;
  default:break;
  }
  return ret;
}
#if defined (CONFIG_YK_DEBUG)
static ssize_t chgen_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct yk_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  yk_read(charger->master, YK_CHARGE_CONTROL1, &val);
  charger->chgen  = val >> 7;
  return sprintf(buf, "%d\n",charger->chgen);
}

static ssize_t chgen_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct yk_charger *charger = dev_get_drvdata(dev);
  int var;
  var = simple_strtoul(buf, NULL, 10);
  if(var){
    charger->chgen = 1;
    yk_set_bits(charger->master,YK_CHARGE_CONTROL1,0x80);
  }
  else{
    charger->chgen = 0;
    yk_clr_bits(charger->master,YK_CHARGE_CONTROL1,0x80);
  }
  return count;
}

static ssize_t chgmicrovol_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct yk_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  yk_read(charger->master, YK_CHARGE_CONTROL1, &val);
  switch ((val >> 5) & 0x03){
    case 0: charger->chgvol = 4100000;break;
    case 1: charger->chgvol = 4200000;break;
    case 2: charger->chgvol = 4240000;break;
    case 3: charger->chgvol = 4350000;break;
  }
  return sprintf(buf, "%d\n",charger->chgvol);
}

static ssize_t chgmicrovol_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct yk_charger *charger = dev_get_drvdata(dev);
  int var;
  uint8_t tmp, val;
  var = simple_strtoul(buf, NULL, 10);
  switch(var){
    case 4100000:tmp = 0;break;
    case 4200000:tmp = 1;break;
    case 4240000:tmp = 2;break;
    case 4350000:tmp = 3;break;
    default:  tmp = 4;break;
  }
  if(tmp < 4){
    charger->chgvol = var;
    yk_read(charger->master, YK_CHARGE_CONTROL1, &val);
    val &= 0x9F;
    val |= tmp << 5;
    yk_write(charger->master, YK_CHARGE_CONTROL1, val);
  }
  return count;
}

static ssize_t chgintmicrocur_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct yk_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  yk_read(charger->master, YK_CHARGE_CONTROL1, &val);
  charger->chgcur = (val & 0x0F) * 150000 +300000;
  return sprintf(buf, "%d\n",charger->chgcur);
}

static ssize_t chgintmicrocur_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct yk_charger *charger = dev_get_drvdata(dev);
  int var;
  uint8_t val,tmp;
  var = simple_strtoul(buf, NULL, 10);
  if(var >= 300000 && var <= 2550000){
    tmp = (var -200001)/150000;
    charger->chgcur = tmp *150000 + 300000;
    yk_read(charger->master, YK_CHARGE_CONTROL1, &val);
    val &= 0xF0;
    val |= tmp;
    yk_write(charger->master, YK_CHARGE_CONTROL1, val);
  }
  return count;
}

static ssize_t chgendcur_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct yk_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  yk_read(charger->master, YK_CHARGE_CONTROL1, &val);
  charger->chgend = ((val >> 4)& 0x01)? 15 : 10;
  return sprintf(buf, "%d\n",charger->chgend);
}

static ssize_t chgendcur_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct yk_charger *charger = dev_get_drvdata(dev);
  int var;
  var = simple_strtoul(buf, NULL, 10);
  if(var == 10 ){
    charger->chgend = var;
    yk_clr_bits(charger->master ,YK_CHARGE_CONTROL1,0x10);
  }
  else if (var == 15){
    charger->chgend = var;
    yk_set_bits(charger->master ,YK_CHARGE_CONTROL1,0x10);

  }
  return count;
}

static ssize_t chgpretimemin_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct yk_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  yk_read(charger->master,YK_CHARGE_CONTROL2, &val);
  charger->chgpretime = (val >> 6) * 10 +40;
  return sprintf(buf, "%d\n",charger->chgpretime);
}

static ssize_t chgpretimemin_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct yk_charger *charger = dev_get_drvdata(dev);
  int var;
  uint8_t tmp,val;
  var = simple_strtoul(buf, NULL, 10);
  if(var >= 40 && var <= 70){
    tmp = (var - 40)/10;
    charger->chgpretime = tmp * 10 + 40;
    yk_read(charger->master,YK_CHARGE_CONTROL2,&val);
    val &= 0x3F;
    val |= (tmp << 6);
    yk_write(charger->master,YK_CHARGE_CONTROL2,val);
  }
  return count;
}

static ssize_t chgcsttimemin_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct yk_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  yk_read(charger->master,YK_CHARGE_CONTROL2, &val);
  charger->chgcsttime = (val & 0x03) *120 + 360;
  return sprintf(buf, "%d\n",charger->chgcsttime);
}

static ssize_t chgcsttimemin_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct yk_charger *charger = dev_get_drvdata(dev);
  int var;
  uint8_t tmp,val;
  var = simple_strtoul(buf, NULL, 10);
  if(var >= 360 && var <= 720){
    tmp = (var - 360)/120;
    charger->chgcsttime = tmp * 120 + 360;
    yk_read(charger->master,YK_CHARGE_CONTROL2,&val);
    val &= 0xFC;
    val |= tmp;
    yk_write(charger->master,YK_CHARGE_CONTROL2,val);
  }
  return count;
}

static ssize_t adcfreq_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct yk_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  yk_read(charger->master, YK_ADC_CONTROL3, &val);
  switch ((val >> 6) & 0x03){
     case 0: charger->sample_time = 100;break;
     case 1: charger->sample_time = 200;break;
     case 2: charger->sample_time = 400;break;
     case 3: charger->sample_time = 800;break;
     default:break;
  }
  return sprintf(buf, "%d\n",charger->sample_time);
}

static ssize_t adcfreq_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct yk_charger *charger = dev_get_drvdata(dev);
  int var;
  uint8_t val;
  var = simple_strtoul(buf, NULL, 10);
  yk_read(charger->master, YK_ADC_CONTROL3, &val);
  switch (var/25){
    case 1: val &= ~(3 << 6);charger->sample_time = 100;break;
    case 2: val &= ~(3 << 6);val |= 1 << 6;charger->sample_time = 200;break;
    case 4: val &= ~(3 << 6);val |= 2 << 6;charger->sample_time = 400;break;
    case 8: val |= 3 << 6;charger->sample_time = 800;break;
    default: break;
    }
  yk_write(charger->master, YK_ADC_CONTROL3, val);
  return count;
}


static ssize_t vholden_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct yk_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  yk_read(charger->master,YK_CHARGE_VBUS, &val);
  val = (val>>6) & 0x01;
  return sprintf(buf, "%d\n",val);
}

static ssize_t vholden_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct yk_charger *charger = dev_get_drvdata(dev);
  int var;
  var = simple_strtoul(buf, NULL, 10);
  if(var)
    yk_set_bits(charger->master, YK_CHARGE_VBUS, 0x40);
  else
    yk_clr_bits(charger->master, YK_CHARGE_VBUS, 0x40);

  return count;
}

static ssize_t vhold_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct yk_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  int vhold;
  yk_read(charger->master,YK_CHARGE_VBUS, &val);
  vhold = ((val >> 3) & 0x07) * 100000 + 4000000;
  return sprintf(buf, "%d\n",vhold);
}

static ssize_t vhold_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct yk_charger *charger = dev_get_drvdata(dev);
  int var;
  uint8_t val,tmp;
  var = simple_strtoul(buf, NULL, 10);
  if(var >= 4000000 && var <=4700000){
    tmp = (var - 4000000)/100000;
    //printk("tmp = 0x%x\n",tmp);
    yk_read(charger->master, YK_CHARGE_VBUS,&val);
    val &= 0xC7;
    val |= tmp << 3;
    //printk("val = 0x%x\n",val);
    yk_write(charger->master, YK_CHARGE_VBUS,val);
  }
  return count;
}

static ssize_t iholden_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct yk_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  yk_read(charger->master,YK_CHARGE_VBUS, &val);
  return sprintf(buf, "%d\n",((val & 0x03) == 0x03)?0:1);
}

static ssize_t iholden_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct yk_charger *charger = dev_get_drvdata(dev);
  int var;
  var = simple_strtoul(buf, NULL, 10);
  if(var)
    yk_clr_bits(charger->master, YK_CHARGE_VBUS, 0x01);
  else
    yk_set_bits(charger->master, YK_CHARGE_VBUS, 0x03);

  return count;
}

static ssize_t ihold_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct yk_charger *charger = dev_get_drvdata(dev);
  uint8_t val,tmp;
  int ihold;
  yk_read(charger->master,YK_CHARGE_VBUS, &val);
  tmp = (val) & 0x03;
  switch(tmp){
    case 0: ihold = 900000;break;
    case 1: ihold = 500000;break;
    default: ihold = 0;break;
  }
  return sprintf(buf, "%d\n",ihold);
}

static ssize_t ihold_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct yk_charger *charger = dev_get_drvdata(dev);
  int var;
  var = simple_strtoul(buf, NULL, 10);
  if(var == 900000)
    yk_clr_bits(charger->master, YK_CHARGE_VBUS, 0x03);
  else if (var == 500000){
    yk_clr_bits(charger->master, YK_CHARGE_VBUS, 0x02);
    yk_set_bits(charger->master, YK_CHARGE_VBUS, 0x01);
  }
  return count;
}
static ssize_t acin_enable_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
	struct yk_charger *charger = dev_get_drvdata(dev);
	uint8_t val;

	yk_read(charger->master,YK_CHARGE3,&val);
	val = (val) & 0x10;
	if(val)
		return 0;
	else
		return 1;
}

static ssize_t acin_enable_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
	struct yk_charger *charger = dev_get_drvdata(dev);
	int var;
	uint8_t val;

	var = simple_strtoul(buf, NULL, 10);
	yk_read(charger->master,YK_CHARGE3,&val);
	if(var) {
		val &= ~(0x10);
		yk_write(charger->master,YK_CHARGE3,val);
	}
	else {
		val |= 0x10;
		yk_write(charger->master,YK_CHARGE3,val);
	}
	return count;
}

static struct device_attribute yk_charger_attrs[] = {
  YK_CHG_ATTR(chgen),
  YK_CHG_ATTR(chgmicrovol),
  YK_CHG_ATTR(chgintmicrocur),
  YK_CHG_ATTR(chgendcur),
  YK_CHG_ATTR(chgpretimemin),
  YK_CHG_ATTR(chgcsttimemin),
  YK_CHG_ATTR(adcfreq),
  YK_CHG_ATTR(vholden),
  YK_CHG_ATTR(vhold),
  YK_CHG_ATTR(iholden),
  YK_CHG_ATTR(ihold),
  YK_CHG_ATTR(acin_enable),
};
#endif

#if defined CONFIG_HAS_EARLYSUSPEND
static void yk_earlysuspend(struct early_suspend *h)
{
	uint8_t tmp;
	DBG_PSY_MSG("======early suspend=======\n");

#if defined (CONFIG_YK_CHGCHANGE)
  	early_suspend_flag = 1;
  	if(EARCHGCUR == 0)
  		yk_clr_bits(yk_charger->master,YK_CHARGE_CONTROL1,0x80);
  	else
  		yk_set_bits(yk_charger->master,YK_CHARGE_CONTROL1,0x80);

    if(EARCHGCUR >= 300000 && EARCHGCUR <= 2550000){
    	tmp = (EARCHGCUR -200001)/150000;
    	yk_update(yk_charger->master, YK_CHARGE_CONTROL1, tmp,0x0F);
    }
#endif

}
static void yk_lateresume(struct early_suspend *h)
{
	uint8_t tmp;
	DBG_PSY_MSG("======late resume=======\n");

#if defined (CONFIG_YK_CHGCHANGE)
	early_suspend_flag = 0;
	if(STACHGCUR == 0)
  		yk_clr_bits(yk_charger->master,YK_CHARGE_CONTROL1,0x80);
  else
  		yk_set_bits(yk_charger->master,YK_CHARGE_CONTROL1,0x80);

    if(STACHGCUR >= 300000 && STACHGCUR <= 2550000){
        tmp = (STACHGCUR -200001)/150000;
        yk_update(yk_charger->master, YK_CHARGE_CONTROL1, tmp,0x0F);
    }
    else if(STACHGCUR < 300000){
    	yk_clr_bits(yk_charger->master, YK_CHARGE_CONTROL1,0x0F);
    }
    else{
    	yk_set_bits(yk_charger->master, YK_CHARGE_CONTROL1,0x0F);
    }
#endif

}
#endif

#if defined (CONFIG_YK_DEBUG)
int yk_charger_create_attrs(struct power_supply *psy)
{
  int j,ret;
  for (j = 0; j < ARRAY_SIZE(yk_charger_attrs); j++) {
    ret = device_create_file(&psy->dev,
          &yk_charger_attrs[j]);
    if (ret)
      goto sysfs_failed;
  }
    goto succeed;

sysfs_failed:
  while (j--)
    device_remove_file(&psy->dev,
         &yk_charger_attrs[j]);
succeed:
  return ret;
}
#endif

static void yk_charging_monitor(struct work_struct *work)
{
	struct yk_charger *charger;
	uint8_t	val,temp_val[4];
	int	pre_rest_vol,pre_bat_curr_dir;
	uint8_t	batt_max_cap_val[3];	
	int	batt_max_cap,coulumb_counter;

	charger = container_of(work, struct yk_charger, work.work);
	pre_rest_vol = charger->rest_vol;
	pre_bat_curr_dir = charger->bat_current_direction;
	yk_charger_update_state(charger);
	yk_charger_update(charger);

	yk_read(charger->master, YK_CAP,&val);
	charger->rest_vol	= (int)	(val & 0x7F);

		yk_reads(charger->master,0xe2,2,temp_val);	
		coulumb_counter = (((temp_val[0] & 0x7f) <<8) + temp_val[1])*1456/1000;
		yk_reads(charger->master,0xe0,2,temp_val);	
		batt_max_cap = (((temp_val[0] & 0x7f) <<8) + temp_val[1])*1456/1000;
		/* Avoid the power stay in 100% for a long time. */
		if(coulumb_counter > batt_max_cap){		
			batt_max_cap_val[0] = temp_val[0] | (0x1<<7);	
			batt_max_cap_val[1] = 0xe3;		
			batt_max_cap_val[2] = temp_val[1];		
			yk_writes(charger->master, 0xe2,3,batt_max_cap_val);	
			DBG_PSY_MSG("yk618 coulumb_counter = %d\n",batt_max_cap);	

		}
			
	if(yk_debug){
		DBG_PSY_MSG("charger->ic_temp = %d\n",charger->ic_temp);
		DBG_PSY_MSG("charger->vbat = %d\n",charger->vbat);
		DBG_PSY_MSG("charger->ibat = %d\n",charger->ibat);
		DBG_PSY_MSG("charger->ocv = %d\n",charger->ocv);
		DBG_PSY_MSG("charger->disvbat = %d\n",charger->disvbat);
		DBG_PSY_MSG("charger->disibat = %d\n",charger->disibat);
		DBG_PSY_MSG("charger->rest_vol = %d\n",charger->rest_vol);
		yk_reads(charger->master,0xba,2,temp_val);
		DBG_PSY_MSG("YK Rdc = %d\n",(((temp_val[0] & 0x1f) <<8) + temp_val[1])*10742/10000);
		yk_reads(charger->master,0xe0,2,temp_val);
		DBG_PSY_MSG("YK batt_max_cap = %d\n",(((temp_val[0] & 0x7f) <<8) + temp_val[1])*1456/1000);
		yk_reads(charger->master,0xe2,2,temp_val);
		DBG_PSY_MSG("YK coulumb_counter = %d\n",(((temp_val[0] & 0x7f) <<8) + temp_val[1])*1456/1000);
		yk_read(charger->master,0xb8,temp_val);
		DBG_PSY_MSG("YK REG_B8 = %x\n",temp_val[0]);
		yk_reads(charger->master,0xe4,2,temp_val);
		DBG_PSY_MSG("YK OCV_percentage = %d\n",(temp_val[0] & 0x7f));
		DBG_PSY_MSG("YK Coulumb_percentage = %d\n",(temp_val[1] & 0x7f));
		DBG_PSY_MSG("charger->is_on = %d\n",charger->is_on);
		DBG_PSY_MSG("charger->bat_current_direction = %d\n",charger->bat_current_direction);
		DBG_PSY_MSG("charger->charge_on = %d\n",charger->charge_on);
		DBG_PSY_MSG("charger->ext_valid = %d\n",charger->ext_valid);
		DBG_PSY_MSG("pmu_runtime_chgcur           = %d\n",STACHGCUR);
		DBG_PSY_MSG("pmu_earlysuspend_chgcur   = %d\n",EARCHGCUR);
		DBG_PSY_MSG("pmu_suspend_chgcur        = %d\n",SUSCHGCUR);
		DBG_PSY_MSG("pmu_shutdown_chgcur       = %d\n\n\n",CLSCHGCUR);
//		yk_chip_id_get(chip_id);
	}


	/* if battery volume changed, inform uevent */
	if((charger->rest_vol - pre_rest_vol) || (charger->bat_current_direction != pre_bat_curr_dir)){
		if(yk_debug)
			{
				yk_reads(charger->master,0xe2,2,temp_val);
				yk_reads(charger->master,0xe4,2,(temp_val+2));
				printk("battery vol change: %d->%d \n", pre_rest_vol, charger->rest_vol);
				printk("for test %d %d %d %d %d %d\n",charger->vbat,charger->ocv,charger->ibat,(temp_val[2] & 0x7f),(temp_val[3] & 0x7f),(((temp_val[0] & 0x7f) <<8) + temp_val[1])*1456/1000);
			}
		pre_rest_vol = charger->rest_vol;
		power_supply_changed(charger->batt);
	}
	/* reschedule for the next time */
	schedule_delayed_work(&charger->work, charger->interval);
}

static void yk_usb(struct work_struct *work)
{
	int var;
	uint8_t tmp,val;
	struct yk_charger *charger;

	charger = yk_charger;
	if(yk_debug)
	{
		printk("[yk_usb]yk_usbcurflag = %d\n",yk_usbcurflag);
	}
	if(yk_usbcurflag){
		if(yk_debug)
		{
			printk("set usbcur_pc %d mA\n",USBCURLIMPC);
		}
		if(USBCURLIMPC){
			yk_clr_bits(charger->master, YK_CHARGE_VBUS, 0x01);
			var = USBCURLIMPC * 1000;
			if(var >= 900000)
				yk_clr_bits(charger->master, YK_CHARGE_VBUS, 0x03);
			else if ((var >= 500000)&& (var < 900000)){
				yk_clr_bits(charger->master, YK_CHARGE_VBUS, 0x02);
				yk_set_bits(charger->master, YK_CHARGE_VBUS, 0x01);
			}
			else{
				printk("set usb limit current error,%d mA\n",USBCURLIMPC);
			}
		}
		else//not limit
			yk_set_bits(charger->master, YK_CHARGE_VBUS, 0x03);
	}else {
		if(yk_debug)
		{
			printk("set usbcur %d mA\n",USBCURLIM);
		}
		if((USBCURLIM) && (USBCURLIMEN)){
			yk_clr_bits(charger->master, YK_CHARGE_VBUS, 0x01);
			var = USBCURLIM * 1000;
			if(var >= 900000)
				yk_clr_bits(charger->master, YK_CHARGE_VBUS, 0x03);
			else if ((var >= 500000)&& (var < 900000)){
				yk_clr_bits(charger->master, YK_CHARGE_VBUS, 0x02);
				yk_set_bits(charger->master, YK_CHARGE_VBUS, 0x01);
			}
			else
				printk("set usb limit current error,%d mA\n",USBCURLIM);
		}
		else //not limit
			yk_set_bits(charger->master, YK_CHARGE_VBUS, 0x03);
	}

	if(yk_usbvolflag){
		if(yk_debug)
		{
			printk("set usbvol_pc %d mV\n",USBVOLLIMPC);
		}
		if(USBVOLLIMPC){
		    yk_set_bits(charger->master, YK_CHARGE_VBUS, 0x40);
		  	var = USBVOLLIMPC * 1000;
		  	if(var >= 4000000 && var <=4700000){
		    	tmp = (var - 4000000)/100000;
		    	yk_read(charger->master, YK_CHARGE_VBUS,&val);
		    	val &= 0xC7;
		    	val |= tmp << 3;
		    	yk_write(charger->master, YK_CHARGE_VBUS,val);
		  	}
		  	else
		  		printk("set usb limit voltage error,%d mV\n",USBVOLLIMPC);
		}
		else
		    yk_clr_bits(charger->master, YK_CHARGE_VBUS, 0x40);
	}else {
		if(yk_debug)
		{
			printk("set usbvol %d mV\n",USBVOLLIM);
		}
		if((USBVOLLIM) && (USBVOLLIMEN)){
		    yk_set_bits(charger->master, YK_CHARGE_VBUS, 0x40);
		  	var = USBVOLLIM * 1000;
		  	if(var >= 4000000 && var <=4700000){
		    	tmp = (var - 4000000)/100000;
		    	yk_read(charger->master, YK_CHARGE_VBUS,&val);
		    	val &= 0xC7;
		    	val |= tmp << 3;
		    	yk_write(charger->master, YK_CHARGE_VBUS,val);
		  	}
		  	else
		  		printk("set usb limit voltage error,%d mV\n",USBVOLLIM);
		}
		else
		    yk_clr_bits(charger->master, YK_CHARGE_VBUS, 0x40);
	}
	schedule_delayed_work(&usbwork, msecs_to_jiffies(5* 1000));
}

static int yk_battery_probe(struct platform_device *pdev)
{
 
  struct yk_charger *charger;
  struct yk_supply_init_data *pdata = pdev->dev.platform_data;
  int ret,var,tmp;
  uint8_t val2,val;
  uint8_t ocv_cap[63];
  int Cur_CoulombCounter,rdc;
  
  struct power_supply_config psy_cfg = {};
  
  powerkeydev = input_allocate_device();
  if (!powerkeydev) {
    kfree(powerkeydev);
    return -ENODEV;
  }

  powerkeydev->name = pdev->name;
  powerkeydev->phys = "m1kbd/input2";
  powerkeydev->id.bustype = BUS_HOST;
  powerkeydev->id.vendor = 0x0001;
  powerkeydev->id.product = 0x0001;
  powerkeydev->id.version = 0x0100;
  powerkeydev->open = NULL;
  powerkeydev->close = NULL;
  powerkeydev->dev.parent = &pdev->dev;

  set_bit(EV_KEY, powerkeydev->evbit);
  set_bit(EV_REL, powerkeydev->evbit);
  set_bit(EV_REP, powerkeydev->evbit);
  set_bit(KEY_POWER, powerkeydev->keybit);

  ret = input_register_device(powerkeydev);
  
  if(ret) {
    printk("Unable to Register the power key\n");
    }

  if (pdata == NULL)
    return -EINVAL;

  printk("yk charger not limit now\n");
  if (pdata->chgcur > 2550000 ||
      pdata->chgvol < 4100000 ||
      pdata->chgvol > 4240000){
        printk("charger milliamp is too high or target voltage is over range\n");
        return -EINVAL;
    }

  if (pdata->chgpretime < 40 || pdata->chgpretime >70 ||
    pdata->chgcsttime < 360 || pdata->chgcsttime > 720){
            printk("prechaging time or constant current charging time is over range\n");
        return -EINVAL;
  }

  charger = kzalloc(sizeof(*charger), GFP_KERNEL);
  if (charger == NULL)
    return -ENOMEM;

  charger->master = pdev->dev.parent;
  {
    struct i2c_client *client;
    client = to_i2c_client(charger->master);
    printk("client = %d\n", client->addr);
  }

  charger->chgcur      = pdata->chgcur;
  charger->chgvol     = pdata->chgvol;
  charger->chgend           = pdata->chgend;
  charger->sample_time          = pdata->sample_time;
  charger->chgen                   = pdata->chgen;
  charger->chgpretime      = pdata->chgpretime;
  charger->chgcsttime = pdata->chgcsttime;
  charger->battery_info         = pdata->battery_info;
  charger->disvbat			= 0;
  charger->disibat			= 0;


  ret = yk_battery_first_init(charger);
  if (ret)
    goto err_charger_init;

 yk_battery_setup_psy(charger);
  
  psy_cfg.drv_data = charger;
  psy_cfg.supplied_to = supply_list;
  psy_cfg.num_supplicants = ARRAY_SIZE(supply_list);
  
    charger->batt = power_supply_register(&pdev->dev, &charger->batt_desc,&psy_cfg);
  
  	if (IS_ERR(charger->batt)) {
		ret = PTR_ERR(charger->batt);
		goto err_ps_register;
	}


	yk_read(charger->master,YK_CHARGE_STATUS,&val);
	if(!((val >> 1) & 0x01)){
   charger->ac = power_supply_register(&pdev->dev, &charger->ac_desc,&psy_cfg);
  
	if (IS_ERR(charger->ac)) {
		ret = PTR_ERR(charger->ac);
		power_supply_unregister(charger->batt);
		goto err_ps_register;
	}
  	
  }

  charger->usb = power_supply_register(&pdev->dev, &charger->usb_desc,&psy_cfg);
  	if (IS_ERR(charger->usb)) {
		ret = PTR_ERR(charger->usb);
		power_supply_unregister(charger->ac);
		power_supply_unregister(charger->batt);
		goto err_ps_register;
	}

#if defined (CONFIG_YK_DEBUG)
  ret = yk_charger_create_attrs(charger->batt);
  if(ret){
  	printk("cat notyk_charger_create_attrs!!!===\n ");
    return ret;
  }
#endif

  platform_set_drvdata(pdev, charger);

  /* usb voltage limit */
  if((USBVOLLIM) && (USBVOLLIMEN)){
    yk_set_bits(charger->master, YK_CHARGE_VBUS, 0x40);
  	var = USBVOLLIM * 1000;
  	if(var >= 4000000 && var <=4700000){
    	tmp = (var - 4000000)/100000;
    	yk_read(charger->master, YK_CHARGE_VBUS,&val);
    	val &= 0xC7;
    	val |= tmp << 3;
    	yk_write(charger->master, YK_CHARGE_VBUS,val);
  	}
  }
  else
    yk_clr_bits(charger->master, YK_CHARGE_VBUS, 0x40);

	/*usb current limit*/
  if((USBCURLIM) && (USBCURLIMEN)){
    yk_clr_bits(charger->master, YK_CHARGE_VBUS, 0x02);
    var = USBCURLIM * 1000;
  	if(var == 900000)
    	yk_clr_bits(charger->master, YK_CHARGE_VBUS, 0x03);
  	else if (var == 500000){
    	yk_set_bits(charger->master, YK_CHARGE_VBUS, 0x01);
  	}
  }
  else
    yk_set_bits(charger->master, YK_CHARGE_VBUS, 0x03);


  // set lowe power warning/shutdown level
  yk_write(charger->master, YK_WARNING_LEVEL,((BATLOWLV1-5) << 4)+BATLOWLV2);

  ocv_cap[0]  = OCVREG0;
  ocv_cap[1]  = 0xC1;
  ocv_cap[2]  = OCVREG1;
  ocv_cap[3]  = 0xC2;
  ocv_cap[4]  = OCVREG2;
  ocv_cap[5]  = 0xC3;
  ocv_cap[6]  = OCVREG3;
  ocv_cap[7]  = 0xC4;
  ocv_cap[8]  = OCVREG4;
  ocv_cap[9]  = 0xC5;
  ocv_cap[10] = OCVREG5;
  ocv_cap[11] = 0xC6;
  ocv_cap[12] = OCVREG6;
  ocv_cap[13] = 0xC7;
  ocv_cap[14] = OCVREG7;
  ocv_cap[15] = 0xC8;
  ocv_cap[16] = OCVREG8;
  ocv_cap[17] = 0xC9;
  ocv_cap[18] = OCVREG9;
  ocv_cap[19] = 0xCA;
  ocv_cap[20] = OCVREGA;
  ocv_cap[21] = 0xCB;
  ocv_cap[22] = OCVREGB;
  ocv_cap[23] = 0xCC;
  ocv_cap[24] = OCVREGC;
  ocv_cap[25] = 0xCD;
  ocv_cap[26] = OCVREGD;
  ocv_cap[27] = 0xCE;
  ocv_cap[28] = OCVREGE;
  ocv_cap[29] = 0xCF;
  ocv_cap[30] = OCVREGF;
  ocv_cap[31] = 0xD0;
  ocv_cap[32] = OCVREG10;
  ocv_cap[33] = 0xD1;
  ocv_cap[34] = OCVREG11;
  ocv_cap[35] = 0xD2;
  ocv_cap[36] = OCVREG12;
  ocv_cap[37] = 0xD3;
  ocv_cap[38] = OCVREG13;
  ocv_cap[39] = 0xD4;
  ocv_cap[40] = OCVREG14;
  ocv_cap[41] = 0xD5;
  ocv_cap[42] = OCVREG15;
  ocv_cap[43] = 0xD6;
  ocv_cap[44] = OCVREG16;
  ocv_cap[45] = 0xD7;
  ocv_cap[46] = OCVREG17;
  ocv_cap[47] = 0xD8;
  ocv_cap[48] = OCVREG18;
  ocv_cap[49] = 0xD9;
  ocv_cap[50] = OCVREG19;
  ocv_cap[51] = 0xDA;
  ocv_cap[52] = OCVREG1A;
  ocv_cap[53] = 0xDB;
  ocv_cap[54] = OCVREG1B;
  ocv_cap[55] = 0xDC;
  ocv_cap[56] = OCVREG1C;
  ocv_cap[57] = 0xDD;
  ocv_cap[58] = OCVREG1D;
  ocv_cap[59] = 0xDE;
  ocv_cap[60] = OCVREG1E;
  ocv_cap[61] = 0xDF;
  ocv_cap[62] = OCVREG1F;
  yk_writes(charger->master, 0xC0,31,ocv_cap);
  yk_writes(charger->master, 0xD0,31,ocv_cap+32);
	/* pok open time set */
	yk_read(charger->master,YK_POK_SET,&val);
	if (PEKOPEN < 1000)
		val &= 0x3f;
	else if(PEKOPEN < 2000){
		val &= 0x3f;
		val |= 0x40;
	}
	else if(PEKOPEN < 3000){
		val &= 0x3f;
		val |= 0x80;
	}
	else {
		val &= 0x3f;
		val |= 0xc0;
	}
	yk_write(charger->master,YK_POK_SET,val);

	/* pok long time set*/
	if(PEKLONG < 1000)
		tmp = 1000;
	else if(PEKLONG > 2500)
		tmp = 2500;
	else
		tmp = PEKLONG;
	yk_read(charger->master,YK_POK_SET,&val);
	val &= 0xcf;
	val |= (((tmp - 1000) / 500) << 4);
	yk_write(charger->master,YK_POK_SET,val);

	/* pek offlevel poweroff en set*/
	if(PEKOFFEN)
	{
			tmp = 1;
	}
	else
	{
			tmp = 0;
	}
	yk_read(charger->master,YK_POK_SET,&val);
	val &= 0xf7;
	val |= (tmp << 3);
	yk_write(charger->master,YK_POK_SET,val);

	/*Init offlevel restart or not */
	if(PEKOFFRESTART)
	{
			yk_set_bits(charger->master,YK_POK_SET,0x04); //restart
	}
	else
	{
			yk_clr_bits(charger->master,YK_POK_SET,0x04); //not restart
	}

	/* pek delay set */
	yk_read(charger->master,YK_OFF_CTL,&val);
	val &= 0xfc;
	val |= ((PEKDELAY / 8) - 1);
	yk_write(charger->master,YK_OFF_CTL,val);

	/* pek offlevel time set */
	if(PEKOFF < 4000)
		tmp = 4000;
	else if(PEKOFF > 10000)
		tmp =10000;
	else
		tmp = PEKOFF;
	tmp = (tmp - 4000) / 2000 ;
	yk_read(charger->master,YK_POK_SET,&val);
	val &= 0xfc;
	val |= tmp ;
	yk_write(charger->master,YK_POK_SET,val);
	/*Init 16's Reset PMU en */
	if(PMURESET)
	{
		yk_set_bits(charger->master,0x8F,0x08); //enable
	}
	else
	{
		yk_clr_bits(charger->master,0x8F,0x08); //disable
	}

	/*Init IRQ wakeup en*/
	if(IRQWAKEUP)
	{
		yk_set_bits(charger->master,0x8F,0x80); //enable
	}
	else
	{
		yk_clr_bits(charger->master,0x8F,0x80); //disable
	}

	/*Init N_VBUSEN status*/
	if(VBUSEN)
	{
		yk_set_bits(charger->master,0x8F,0x10); //output
	}
	else
	{
		yk_clr_bits(charger->master,0x8F,0x10); //input
	}

	/*Init InShort status*/
	if(VBUSACINSHORT)
	{
		yk_set_bits(charger->master,0x8F,0x60); //InShort
	}
	else
	{
		yk_clr_bits(charger->master,0x8F,0x60); //auto detect
	}

	/*Init CHGLED function*/
	if(CHGLEDFUN)
	{
		yk_set_bits(charger->master,0x32,0x08); //control by charger
	}
	else
	{
		yk_clr_bits(charger->master,0x32,0x08); //drive MOTO
	}

	/*set CHGLED Indication Type*/
	if(CHGLEDTYPE)
	{
		yk_set_bits(charger->master,0x34,0x10); //Type B
	}
	else
	{
		yk_clr_bits(charger->master,0x34,0x10); //Type A
	}

	/*Init PMU Over Temperature protection*/
	if(OTPOFFEN)
	{
		yk_set_bits(charger->master,0x8f,0x04); //enable
	}
	else
	{
		yk_clr_bits(charger->master,0x8f,0x04); //disable
	}

	/*Init battery capacity correct function*/
	if(BATCAPCORRENT)
	{
		yk_set_bits(charger->master,0xb8,0x20); //enable
	}
	else
	{
		yk_clr_bits(charger->master,0xb8,0x20); //disable
	}
	/* Init battery regulator enable or not when charge finish*/
	if(BATREGUEN)
	{
		yk_set_bits(charger->master,0x34,0x20); //enable
	}
	else
	{
		yk_clr_bits(charger->master,0x34,0x20); //disable
	}

	if(!BATDET)
		yk_clr_bits(charger->master,YK_PDBC,0x40);
	else
		yk_set_bits(charger->master,YK_PDBC,0x40);


/* RDC initial */
	yk_read(charger->master, YK_RDC0,&val2);
	if((BATRDC) && (!(val2 & 0x40)))		//如果配置电池内阻，则手动配置
	{
		rdc = (BATRDC * 10000 + 5371) / 10742;
		yk_write(charger->master, YK_RDC0, ((rdc >> 8) & 0x1F)|0x80);
		yk_write(charger->master,YK_RDC1,rdc & 0x00FF);
	}

//probe 时初始化RDC，使其提前计算正确的OCV，然后在此处启动计量系统
	yk_read(charger->master,YK_BATCAP0,&val2);
	if((BATCAP) && (!(val2 & 0x80)))
	{
		Cur_CoulombCounter = BATCAP * 1000 / 1456;
		yk_write(charger->master, YK_BATCAP0, ((Cur_CoulombCounter >> 8) | 0x80));
		yk_write(charger->master,YK_BATCAP1,Cur_CoulombCounter & 0x00FF);
	}
	else if(!BATCAP)
	{
		yk_write(charger->master, YK_BATCAP0, 0x00);
		yk_write(charger->master,YK_BATCAP1,0x00);
	}

  yk_charger_update_state((struct yk_charger *)charger);

  yk_read(charger->master, YK_CAP,&val2);
	charger->rest_vol = (int) (val2 & 0x7F);

  charger->interval = msecs_to_jiffies(3 * 1000);
  INIT_DELAYED_WORK(&charger->work, yk_charging_monitor);
  schedule_delayed_work(&charger->work, charger->interval);

  /* set usb cur-vol limit*/
	INIT_DELAYED_WORK(&usbwork, yk_usb);
	schedule_delayed_work(&usbwork, msecs_to_jiffies(7 * 1000));
	/*给局部变量赋值*/
	yk_charger = charger;
#ifdef CONFIG_HAS_EARLYSUSPEND

    yk_early_suspend.suspend = yk_earlysuspend;
    yk_early_suspend.resume = yk_lateresume;
    yk_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 2;
    register_early_suspend(&yk_early_suspend);
#endif
	/* 调试接口注册 */
	class_register(&ykpower_class);
    return ret;

err_ps_register:
  cancel_delayed_work_sync(&charger->work);

err_charger_init:
  kfree(charger);
  input_unregister_device(powerkeydev);
  kfree(powerkeydev);
  return ret;
}

static int yk_battery_remove(struct platform_device *dev)
{
    struct yk_charger *charger = platform_get_drvdata(dev);

    if(main_task){
        kthread_stop(main_task);
        main_task = NULL;
    }

    cancel_delayed_work_sync(&charger->work);
    power_supply_unregister(charger->usb);
    power_supply_unregister(charger->ac);
    power_supply_unregister(charger->batt);

    kfree(charger);
    input_unregister_device(powerkeydev);
    kfree(powerkeydev);

    return 0;
}


static int yk_suspend(struct platform_device *dev, pm_message_t state)
{
    uint8_t irq_w[9];
    uint8_t tmp;

    struct yk_charger *charger = platform_get_drvdata(dev);

    cancel_delayed_work_sync(&charger->work);
    cancel_delayed_work_sync(&usbwork);

    /*clear all irqs events*/
    irq_w[0] = 0xff;
    irq_w[1] = YK_INTSTS2;
    irq_w[2] = 0xff;
    irq_w[3] = YK_INTSTS3;
    irq_w[4] = 0xff;
    irq_w[5] = YK_INTSTS4;
    irq_w[6] = 0xff;
    irq_w[7] = YK_INTSTS5;
    irq_w[8] = 0xff;
    yk_writes(charger->master, YK_INTSTS1, 9, irq_w);

#if defined (CONFIG_YK_CHGCHANGE)
    if(SUSCHGCUR == 0)
        yk_clr_bits(charger->master,YK_CHARGE_CONTROL1,0x80);
    else
        yk_set_bits(charger->master,YK_CHARGE_CONTROL1,0x80);

    if(SUSCHGCUR >= 300000 && SUSCHGCUR <= 2550000){
        tmp = (SUSCHGCUR -200001)/150000;
        charger->chgcur = tmp *150000 + 300000;
        yk_update(charger->master, YK_CHARGE_CONTROL1, tmp,0x0F);
    }
#endif

    return 0;
}

static int yk_resume(struct platform_device *dev)
{
    struct yk_charger *charger = platform_get_drvdata(dev);

    int pre_rest_vol;
    uint8_t val,tmp;

    yk_charger_update_state(charger);

    pre_rest_vol = charger->rest_vol;

    yk_read(charger->master, YK_CAP,&val);
    charger->rest_vol = val & 0x7f;

    if (charger->rest_vol - pre_rest_vol) {
        pre_rest_vol = charger->rest_vol;
        yk_write(charger->master,YK_DATA_BUFFER1,charger->rest_vol | 0x80);
        power_supply_changed(charger->batt);
    }

#if defined (CONFIG_YK_CHGCHANGE)
    if(STACHGCUR == 0)
    	yk_clr_bits(charger->master,YK_CHARGE_CONTROL1,0x80);
    else
    	yk_set_bits(charger->master,YK_CHARGE_CONTROL1,0x80);

    if(STACHGCUR >= 300000 && STACHGCUR <= 2550000){
        tmp = (STACHGCUR -200001)/150000;
        charger->chgcur = tmp *150000 + 300000;
        yk_update(charger->master, YK_CHARGE_CONTROL1, tmp,0x0F);
    }else if(STACHGCUR < 300000){
    	yk_clr_bits(yk_charger->master, YK_CHARGE_CONTROL1,0x0F);
    }
    else{
    	yk_set_bits(yk_charger->master, YK_CHARGE_CONTROL1,0x0F);
    }
#endif

    charger->disvbat = 0;
    charger->disibat = 0;
    schedule_delayed_work(&charger->work, charger->interval);
    schedule_delayed_work(&usbwork, msecs_to_jiffies(7 * 1000));

    return 0;
}

static void yk_shutdown(struct platform_device *dev)
{
    uint8_t tmp;
    struct yk_charger *charger = platform_get_drvdata(dev);

    cancel_delayed_work_sync(&charger->work);

#if defined (CONFIG_YK_CHGCHANGE)
    if(CLSCHGCUR == 0)
        yk_clr_bits(charger->master,YK_CHARGE_CONTROL1,0x80);
    else
        yk_set_bits(charger->master,YK_CHARGE_CONTROL1,0x80);

    if(CLSCHGCUR >= 300000 && CLSCHGCUR <= 2550000){
    	tmp = (CLSCHGCUR -200001)/150000;
    	charger->chgcur = tmp *150000 + 300000;
    	yk_update(charger->master, YK_CHARGE_CONTROL1, tmp, 0x0F);
    }
#endif
}

static struct platform_driver yk_battery_driver = {
  .driver = {
    .name = "yk618-supplyer",
    .owner  = THIS_MODULE,
  },
  .probe = yk_battery_probe,
  .remove = yk_battery_remove,
  .suspend = yk_suspend,
  .resume = yk_resume,
  .shutdown = yk_shutdown,
};

static int yk_battery_init(void)
{
    int ret =0;
    ret = platform_driver_register(&yk_battery_driver);
    return ret;
}

static void yk_battery_exit(void)
{
    platform_driver_unregister(&yk_battery_driver);
}

subsys_initcall(yk_battery_init);
module_exit(yk_battery_exit);

MODULE_DESCRIPTION("YEKER battery charger driver");
MODULE_AUTHOR("yeker");
MODULE_LICENSE("GPL");
