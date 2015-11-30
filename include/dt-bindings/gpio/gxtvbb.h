/*
 * include/dt-bindings/gpio/gxtvbb.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#ifndef __DT_BINDINGS_GPIO_GXTVBB_GPIO_H
#define __DT_BINDINGS_GPIO_GXTVBB_GPIO_H


/* AO Bank */
#define	GPIOAO_0	0
#define	GPIOAO_1	1
#define	GPIOAO_2	2
#define	GPIOAO_3	3
#define	GPIOAO_4	4
#define	GPIOAO_5	5
#define	GPIOAO_6	6
#define	GPIOAO_7	7
#define	GPIOAO_8	8
#define	GPIOAO_9	9
#define	GPIOAO_10	10
#define	GPIOAO_11	11
#define	GPIOAO_12	12
#define	GPIOAO_13	13

/* EE Bank */
#define	GPIOZ_0		0
#define	GPIOZ_1		1
#define	GPIOZ_2		2
#define	GPIOZ_3		3
#define	GPIOZ_4		4
#define	GPIOZ_5		5
#define	GPIOZ_6		6
#define	GPIOZ_7		7
#define	GPIOZ_8		8
#define	GPIOZ_9		9
#define	GPIOZ_10	10
#define	GPIOZ_11	11
#define	GPIOZ_12	12
#define	GPIOZ_13	13
#define	GPIOZ_14	14
#define	GPIOZ_15	15
#define	GPIOZ_16	16
#define	GPIOZ_17	17
#define	GPIOZ_18	18
#define	GPIOZ_19	19
#define	GPIOZ_20	20

#define	GPIOH_0		21
#define	GPIOH_1		22
#define	GPIOH_2		23
#define	GPIOH_3		24
#define	GPIOH_4		25
#define	GPIOH_5		26
#define	GPIOH_6		27
#define	GPIOH_7		28
#define	GPIOH_8		29
#define	GPIOH_9		30
#define	GPIOH_10	31

#define	BOOT_0		32
#define	BOOT_1		33
#define	BOOT_2		34
#define	BOOT_3		35
#define	BOOT_4		36
#define	BOOT_5		37
#define	BOOT_6		38
#define	BOOT_7		39
#define	BOOT_8		40	/* BOOT_8  undefined no interrupt */
#define	BOOT_9		41
#define	BOOT_10		42	/* BOOT_10 undefined no interrupt */
#define	BOOT_11		43
#define	BOOT_12		44
#define	BOOT_13		45
#define	BOOT_14		46
#define	BOOT_15		47
#define	BOOT_16		48
#define	BOOT_17		49
#define	BOOT_18		50

#define	CARD_0		51
#define	CARD_1		52
#define	CARD_2		53
#define	CARD_3		54
#define	CARD_4		55
#define	CARD_5		56
#define	CARD_6		57
#define	CARD_7		58	/* CARD_7  undefined no interrupt */
#define	CARD_8		59	/* CARD_8  undefined no interrupt */

#define	GPIOW_0		60
#define	GPIOW_1		61
#define	GPIOW_2		62
#define	GPIOW_3		63
#define	GPIOW_4		64
#define	GPIOW_5		65
#define	GPIOW_6		66
#define	GPIOW_7		67
#define	GPIOW_8		68
#define	GPIOW_9		69
#define	GPIOW_10	70
#define	GPIOW_11	71
#define	GPIOW_12	72
#define	GPIOW_13	73
#define	GPIOW_14	74
#define	GPIOW_15	75
#define	GPIOW_16	76
#define	GPIOW_17	77
#define	GPIOW_18	78
#define	GPIOW_19	79
#define	GPIOW_20	80

#define	GPIOY_0		81
#define	GPIOY_1		82
#define	GPIOY_2		83
#define	GPIOY_3		84
#define	GPIOY_4		85
#define	GPIOY_5		86
#define	GPIOY_6		87
#define	GPIOY_7		88
#define	GPIOY_8		89
#define	GPIOY_9		90
#define	GPIOY_10	91
#define	GPIOY_11	92
#define	GPIOY_12	93
#define	GPIOY_13	94

#define	GPIOX_0		95
#define	GPIOX_1		96
#define	GPIOX_2		97
#define	GPIOX_3		98
#define	GPIOX_4		99
#define	GPIOX_5		100
#define	GPIOX_6		101
#define	GPIOX_7		102
#define	GPIOX_8		103
#define	GPIOX_9		104
#define	GPIOX_10	105
#define	GPIOX_11	106
#define	GPIOX_12	107
#define	GPIOX_13	108
#define	GPIOX_14	109
#define	GPIOX_15	110
#define	GPIOX_16	111
#define	GPIOX_17	112
#define	GPIOX_18	113
#define	GPIOX_19	114
#define	GPIOX_20	115
#define	GPIOX_21	116
#define	GPIOX_22	117
#define	GPIOX_23	118
#define	GPIOX_24	119
#define	GPIOX_25	120
#define	GPIOX_26	121
#define	GPIOX_27	122

#define	GPIOCLK_0	123

#define	GPIO_TEST_N	124

/* AO REG */
#define	AO		0x10
#define	AO2		0x11


#endif
