/*
 * include/dt-bindings/clock/t5d-clkc.h
 *
 * Copyright (C) 2018 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __DT_BINDINGS_T5D_AOCLKC_H
#define __DT_BINDINGS_T5D_AOCLKC_H

#define CLKID_AO_CLK81				0
#define CLKID_SARADC_MUX			1
#define CLKID_SARADC_DIV			2
#define CLKID_SARADC_GATE			3
#define CLKID_AO_AHB_BUS			4
#define CLKID_AO_IR				5
#define CLKID_AO_I2C_MASTER			6
#define CLKID_AO_I2C_SLAVE			7
#define CLKID_AO_UART1				8
#define CLKID_AO_PROD_I2C			9
#define CLKID_AO_UART2				10
#define CLKID_AO_IR_BLASTER			11
#define CLKID_AO_SAR_ADC			12
#define CLKID_CECB_32K_CLKIN			13
#define CLKID_CECB_32K_DIV			14
#define CLKID_CECB_32K_MUX_PRE			15
#define CLKID_CECB_32K_MUX			16
#define CLKID_CECB_32K_CLKOUT			17

#define NR_AOCLKS				18

#endif /* __DT_BINDINGS_T5D_AOCLKC_H */
