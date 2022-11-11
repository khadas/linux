/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __DVBS_DISEQC_H__
#define __DVBS_DISEQC_H__

#include <linux/interrupt.h>

#include "lnb_controller.h"

#define IRQ_STS_GAPBURST	0x01
#define IRQ_STS_TXFIFO64B	0x02
#define IRQ_STS_TXEND		0x04
#define IRQ_STS_TIMEOUT		0x08
#define IRQ_STS_TRFINISH	0x10
#define IRQ_STS_RXFIFO8B	0x20
#define IRQ_STS_RXEND		0x40

#define DISEQC_MSG_MAX_LEN	20

struct aml_diseqc {
	const char *name;
	unsigned int irq_num;
	bool irq_en;
	struct mutex mutex_tx_msg;/*protect tx cec msg*/
	struct mutex mutex_rx_msg;/*protect rx cec msg*/

	struct lnbc lnbc;
	unsigned int tone_on;
	unsigned int voltage;/*fe_sec_voltage*/

	struct completion rx_msg_ok;
	struct completion tx_msg_ok;

	bool attached;
	bool sar_adc_enable;
};

extern u32 sendburst_on;
extern u32 diseqc_cmd_bypass;

void demod_dump_reg_diseqc(void);
void aml_diseqc_attach(struct device *dev, struct dvb_frontend *fe);
u32 aml_diseqc_send_cmd(struct aml_diseqc *diseqc,
		struct dvb_diseqc_master_cmd *cmd);
void aml_diseqc_dbg_en(unsigned int val);
void aml_diseqc_isr(struct aml_diseqc *diseqc);
void aml_diseqc_status(struct aml_diseqc *diseqc);
void aml_diseqc_toneburst_sa(void);
void aml_diseqc_toneburst_sb(void);
void aml_diseqc_tone_on(struct aml_diseqc *diseqc, bool onoff);
void aml_diseqc_flag_tone_on(u32 onoff);
int aml_diseqc_set_voltage(struct dvb_frontend *fe,
		enum fe_sec_voltage voltage);
irqreturn_t aml_diseqc_isr_handler(int irq, void *data);
void aml_diseqc_isr_en(struct aml_diseqc *diseqc, bool en);
#endif
