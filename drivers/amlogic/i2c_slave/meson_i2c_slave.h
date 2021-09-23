/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/i2c.h>

/*I2C_S_CONTROL_REG   0x2150*/
struct aml_i2c_slave_reg_ctrl {
	/*[6-0]*/
	/*Sampling rate. */
	unsigned int rate:7;
	/*[7]*/
	/*Enable:  enables the I2C slave state machine.. */
	unsigned int en_irq;
	/*15-8*/
	/*HOLD TIME:  Data hold time after the falling edge of SCL.  T
	 *his hold time is computed as
	 *Hold time = (MPEG system clock period) * (value + 1).
	 */
	unsigned int hold_time:8;
	/*23-16*/
	/*Slave Address:  .*/
	unsigned int slave_addr:8;
	/*24*/
	/*ACK Always:  */
	unsigned int ack:1;

	/*25*/
	/*IRQ_EN: */
	unsigned int irq_status:1;

	/*26*/
	/*BUSY: .*/
	unsigned int reg_status:1;

	/*27*/
	/*RECEIVE READY: .*/
	unsigned int rev_ready:1;

	/*28*/
	/*SEND READY:  */
	unsigned int send_ready:1;

	/*31-29*/
	/*REG POINTER:  */
	unsigned int reg_pointer:3;
};

struct aml_i2c_reg_slave {
	unsigned int s_reg_ctrl;    /*I2C_S_CONTROL_REG 0x50*/
	unsigned int s_send_reg;    /*I2C_S_SEND_REG: Send Data 0x51*/
	unsigned int s_rev_reg;     /*I2C_S_RECV_REG: Received Data  0x52*/
	unsigned int s_ctrl1_reg;   /*I2C_S_CNTL1_REG   0x53*/
};

struct aml_i2c_slave {
	struct aml_i2c_reg_slave __iomem *slave_regs;
	unsigned int  __iomem *s_reset;
	const char *slave_state_name;
	unsigned int irq;
	struct class *cls;
	struct timer_list timer;
	unsigned int time_out;
	struct mutex *lock; /* protect i2c slave reg */
};
