/*****************************************************************
**
**  Copyright (C) 2010 Amlogic,Inc.
**  All rights reserved
**        Filename : amlfrontend.h
**
**  comment:
**        Driver for aml demodulator
**
*****************************************************************/

#ifndef _AMLFRONTEND_H
#define _AMLFRONTEND_H

struct amlfe_config {
	int fe_mode;
	int i2c_id;
	int tuner_type;
	int tuner_addr;
};
enum Gxtv_Demod_Tuner_If {
	Si2176_5M_If = 5,
	Si2176_6M_If = 6
};
/* 0 -DVBC, 1-DVBT, ISDBT, 2-ATSC */
enum Gxtv_Demod_Dvb_Mode {
	Gxtv_Dvbc = 0,
	Gxtv_Dvbt_Isdbt = 1,
	Gxtv_Atsc = 2,
	Gxtv_Dtmb = 3,
};
#define Adc_Clk_35M             35714	/* adc clk    dvbc */
#define Demod_Clk_71M   71428	/* demod clk */

#define Adc_Clk_24M             24000
#define Demod_Clk_72M       72000
#define Demod_Clk_60M       60000

#define Adc_Clk_28M             28571	/* dvbt,isdbt */
#define Demod_Clk_66M   66666

#define Adc_Clk_26M                     26000	/* atsc  air */
#define Demod_Clk_78M     78000	/*  */

#define Adc_Clk_25_2M                   25200	/* atsc  cable */
#define Demod_Clk_75M     75600	/*  */

#define Adc_Clk_25M                     25000	/* dtmb */
#define Demod_Clk_100M    100000	/*  */
#define Demod_Clk_180M    180000	/*  */
#define Demod_Clk_200M    200000	/*  */
#define Demod_Clk_225M    225000

#define Adc_Clk_27M                     27777	/* atsc */
#define Demod_Clk_83M     83333	/*  */

enum M6_Demod_Pll_Mode {
	Cry_mode = 0,
	Adc_mode = 1
};

int M6_Demod_Dtmb_Init(struct aml_fe_dev *dev);
int convert_snr(int in_snr);

#endif
