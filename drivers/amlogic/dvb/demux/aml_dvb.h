/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_DVB_H_
#define _AML_DVB_H_

#include "aml_dmx.h"
#include "aml_dsc.h"
//#include "hw_demux/hwdemux.h"

#define DMX_DEV_COUNT     32
#define DSC_DEV_COUNT     DMX_DEV_COUNT
/*TSIN just 4, but maxlinear maybe send 8 TS at one TSIN.*/
/*will save all dts config*/
#define FE_DEV_COUNT	  8
#define TS_HEADER_LEN    12

enum {
	AM_TS_DISABLE,
	AM_TS_PARALLEL,
	AM_TS_SERIAL_3WIRE,
	AM_TS_SERIAL_4WIRE
};

struct aml_s2p {
	int invert;
};

struct aml_ts_input {
	int mode;
	struct pinctrl *pinctrl;
	int control;
	int ts_sid;
	int header_len;
	int header[TS_HEADER_LEN];
	int sid_offset;
};

struct aml_dvb {
	struct dvb_device dvb_dev;
	//struct dvb_adapter dvb_adapter;

	struct device *dev;
	struct platform_device *pdev;

	struct swdmx_ts_parser *tsp[DMX_DEV_COUNT];
	struct swdmx_demux *swdmx[DMX_DEV_COUNT];
	struct aml_dmx dmx[DMX_DEV_COUNT];
	struct aml_dsc dsc[DSC_DEV_COUNT];
	struct aml_ts_input ts[FE_DEV_COUNT];
	/*protect many user operate*/
	struct mutex mutex;

	/*1: dsc connect demod
	 *0: dsc connect local
	 */
	unsigned int dsc_pipeline;
	unsigned int tsn_flag;
	unsigned char loop_tsn;
	unsigned char ts_clone;
};

struct aml_dvb *aml_get_dvb_device(void);
struct device *aml_get_device(void);
int tsn_set_double_out(int flag);
void set_dvb_loop_tsn(int flag);
int get_dvb_loop_tsn(void);

extern int is_security_dmx;
ssize_t dmx_setting_show(struct class *class,
	struct class_attribute *attr, char *buf);

enum {
	SUPPORT_ES_HEADER_NEED_AUCPU,
	SUPPORT_TSD,
	SUPPORT_PSCP,
	SUPPORT_TEMI,
	SUPPORT_PES_HEADER,
};

int get_demux_feature(int support_feature);
unsigned int get_dmx_version(void);
#endif
