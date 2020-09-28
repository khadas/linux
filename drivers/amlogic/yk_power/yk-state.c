/*
 * Battery charger driver for YEKER
 *
 * Copyright (C) 2014 YEKER Ltd.
 *  
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "yk-cfg.h"

struct yk_adc_res adc;

static inline int yk_vts_to_temp(int data, const struct yk_config_info *yk_config)
{
	 int temp;
	 if (data < 80)
		 return 30;
	 else if (data < yk_config->pmu_temp_para16)
		 return 80;
	 else if (data <= yk_config->pmu_temp_para15) {
		 temp = 70 + (yk_config->pmu_temp_para15-data)*10/(yk_config->pmu_temp_para15-yk_config->pmu_temp_para16);
	 } else if (data <= yk_config->pmu_temp_para14) {
		 temp = 60 + (yk_config->pmu_temp_para14-data)*10/(yk_config->pmu_temp_para14-yk_config->pmu_temp_para15);
	 } else if (data <= yk_config->pmu_temp_para13) {
		 temp = 55 + (yk_config->pmu_temp_para13-data)*5/(yk_config->pmu_temp_para13-yk_config->pmu_temp_para14);
	 } else if (data <= yk_config->pmu_temp_para12) {
		 temp = 50 + (yk_config->pmu_temp_para12-data)*5/(yk_config->pmu_temp_para12-yk_config->pmu_temp_para13);
	 } else if (data <= yk_config->pmu_temp_para11) {
		 temp = 45 + (yk_config->pmu_temp_para11-data)*5/(yk_config->pmu_temp_para11-yk_config->pmu_temp_para12);
	 } else if (data <= yk_config->pmu_temp_para10) {
		 temp = 40 + (yk_config->pmu_temp_para10-data)*5/(yk_config->pmu_temp_para10-yk_config->pmu_temp_para11);
	 } else if (data <= yk_config->pmu_temp_para9) {
		 temp = 30 + (yk_config->pmu_temp_para9-data)*10/(yk_config->pmu_temp_para9-yk_config->pmu_temp_para10);
	 } else if (data <= yk_config->pmu_temp_para8) {
		 temp = 20 + (yk_config->pmu_temp_para8-data)*10/(yk_config->pmu_temp_para8-yk_config->pmu_temp_para9);
	 } else if (data <= yk_config->pmu_temp_para7) {
		 temp = 10 + (yk_config->pmu_temp_para7-data)*10/(yk_config->pmu_temp_para7-yk_config->pmu_temp_para8);
	 } else if (data <= yk_config->pmu_temp_para6) {
		 temp = 5 + (yk_config->pmu_temp_para6-data)*5/(yk_config->pmu_temp_para6-yk_config->pmu_temp_para7);
	 } else if (data <= yk_config->pmu_temp_para5) {
		 temp = 0 + (yk_config->pmu_temp_para5-data)*5/(yk_config->pmu_temp_para5-yk_config->pmu_temp_para6);
	 } else if (data <= yk_config->pmu_temp_para4) {
		 temp = -5 + (yk_config->pmu_temp_para4-data)*5/(yk_config->pmu_temp_para4-yk_config->pmu_temp_para5);
	 } else if (data <= yk_config->pmu_temp_para3) {
		 temp = -10 + (yk_config->pmu_temp_para3-data)*5/(yk_config->pmu_temp_para3-yk_config->pmu_temp_para4);
	 } else if (data <= yk_config->pmu_temp_para2) {
		 temp = -15 + (yk_config->pmu_temp_para2-data)*5/(yk_config->pmu_temp_para2-yk_config->pmu_temp_para3);
	 } else if (data <= yk_config->pmu_temp_para1) {
		 temp = -25 + (yk_config->pmu_temp_para1-data)*10/(yk_config->pmu_temp_para1-yk_config->pmu_temp_para2);
	 } else
		 temp = -25;
    return temp;
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
	 yk_reads(charger->master,YK_VTS_RES,2,tmp);
	 adc->ts_res = ((uint16_t) tmp[0] << 8 )| tmp[1];
}


void yk_charger_update_state(struct yk_charger *charger)
{
	 uint8_t val[2];
	 uint16_t tmp;
	 
	 yk_reads(charger->master,YK_CHARGE_STATUS,2,val);
	 tmp = (val[1] << 8 )+ val[0];
	 spin_lock(&charger->charger_lock);
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
	 spin_unlock(&charger->charger_lock);
	 yk_read(charger->master,YK_CHARGE_CONTROL1,val);
	 spin_lock(&charger->charger_lock);
	 charger->charge_on = ((val[0] >> 7) & 0x01);
	 spin_unlock(&charger->charger_lock);
}

void yk_charger_update(struct yk_charger *charger, const struct yk_config_info *yk_config)
{
	uint16_t tmp;
	uint8_t val[2];
	int bat_temp_mv;

	charger->adc = &adc;
	yk_read_adc(charger, &adc);
	tmp = charger->adc->vbat_res;

	spin_lock(&charger->charger_lock);
	charger->vbat = yk_vbat_to_mV(tmp);
	spin_unlock(&charger->charger_lock);

	tmp = charger->adc->ocvbat_res;

	spin_lock(&charger->charger_lock);
	charger->ocv = yk_ocvbat_to_mV(tmp);
	spin_unlock(&charger->charger_lock);

	//tmp = charger->adc->ichar_res + charger->adc->idischar_res;
	spin_lock(&charger->charger_lock);
	charger->ibat = ABS(yk_icharge_to_mA(charger->adc->ichar_res)-yk_ibat_to_mA(charger->adc->idischar_res));
	tmp = 00;
	charger->vac = yk_vdc_to_mV(tmp);
	tmp = 00;
	charger->iac = yk_iac_to_mA(tmp);
	tmp = 00;
	charger->vusb = yk_vdc_to_mV(tmp);
	tmp = 00;
	charger->iusb = yk_iusb_to_mA(tmp);
	spin_unlock(&charger->charger_lock);

	yk_reads(charger->master,YK_INTTEMP,2,val);
	//DBG_PSY_MSG("TEMPERATURE:val1=0x%x,val2=0x%x\n",val[1],val[0]);
	tmp = (val[0] << 4 ) + (val[1] & 0x0F);

	spin_lock(&charger->charger_lock);
	charger->ic_temp = (int) tmp *1063/10000  - 2667/10;
	charger->disvbat =  charger->vbat;
	charger->disibat =  yk_ibat_to_mA(charger->adc->idischar_res);
	spin_unlock(&charger->charger_lock);
	
	tmp = charger->adc->ts_res;
	bat_temp_mv = yk_vts_to_mV(tmp);
	
	spin_lock(&charger->charger_lock);
	charger->bat_temp = yk_vts_to_temp(bat_temp_mv, yk_config);
	spin_unlock(&charger->charger_lock);
}

