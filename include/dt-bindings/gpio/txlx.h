/*
 * include/dt-bindings/gpio/txlx.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#ifndef __DT_BINDINGS_GPIO_TXLX_H
#define __DT_BINDINGS_GPIO_TXLX_H

/*
 * NOTICE: The gpio number sequence according to the gpio interrupts.
 */


/* AO Bank */
#define GPIO_AO_BASE		(0)
#define	GPIO_AO_SIZE		(14)

/* gpioao [12-0] */
#define GPIOAO_BASE		(0)
#define	GPIOAO_SIZE		(14)

#define	GPIOAO_0		(GPIOAO_BASE + 0)
#define	GPIOAO_1		(GPIOAO_BASE + 1)
#define	GPIOAO_2		(GPIOAO_BASE + 2)
#define	GPIOAO_3		(GPIOAO_BASE + 3)
#define	GPIOAO_4		(GPIOAO_BASE + 4)
#define	GPIOAO_5		(GPIOAO_BASE + 5)
#define	GPIOAO_6		(GPIOAO_BASE + 6)
#define	GPIOAO_7		(GPIOAO_BASE + 7)
#define	GPIOAO_8		(GPIOAO_BASE + 8)
#define	GPIOAO_9		(GPIOAO_BASE + 9)
#define	GPIOAO_10		(GPIOAO_BASE + 10)
#define	GPIOAO_11		(GPIOAO_BASE + 11)
#define	GPIOAO_12		(GPIOAO_BASE + 12)
#define	GPIOAO_13		(GPIOAO_BASE + 13)


/* EE Bank */
#define GPIO_EE_BASE		(0)
#define GPIO_EE_SIZE		(107)

/* gpioz [19-0] */
#define	GPIOZ_BASE		(0)
#define	GPIOZ_SIZE		(20)

#define	GPIOZ_0			(GPIOZ_BASE + 0)
#define	GPIOZ_1			(GPIOZ_BASE + 1)
#define	GPIOZ_2			(GPIOZ_BASE + 2)
#define	GPIOZ_3			(GPIOZ_BASE + 3)
#define	GPIOZ_4			(GPIOZ_BASE + 4)
#define	GPIOZ_5			(GPIOZ_BASE + 5)
#define	GPIOZ_6			(GPIOZ_BASE + 6)
#define	GPIOZ_7			(GPIOZ_BASE + 7)
#define	GPIOZ_8			(GPIOZ_BASE + 8)
#define	GPIOZ_9			(GPIOZ_BASE + 9)
#define	GPIOZ_10		(GPIOZ_BASE + 10)
#define	GPIOZ_11		(GPIOZ_BASE + 11)
#define	GPIOZ_12		(GPIOZ_BASE + 12)
#define	GPIOZ_13		(GPIOZ_BASE + 13)
#define	GPIOZ_14		(GPIOZ_BASE + 14)
#define	GPIOZ_15		(GPIOZ_BASE + 15)
#define	GPIOZ_16		(GPIOZ_BASE + 16)
#define	GPIOZ_17		(GPIOZ_BASE + 17)
#define	GPIOZ_18		(GPIOZ_BASE + 18)
#define	GPIOZ_19		(GPIOZ_BASE + 19)


/* gpioh [10-0] */
#define	GPIOH_BASE		(GPIOZ_BASE + GPIOZ_SIZE)
#define	GPIOH_SIZE		(11)

#define	GPIOH_0			(GPIOH_BASE + 0)
#define	GPIOH_1			(GPIOH_BASE + 1)
#define	GPIOH_2			(GPIOH_BASE + 2)
#define	GPIOH_3			(GPIOH_BASE + 3)
#define	GPIOH_4			(GPIOH_BASE + 4)
#define	GPIOH_5			(GPIOH_BASE + 5)
#define	GPIOH_6			(GPIOH_BASE + 6)
#define	GPIOH_7			(GPIOH_BASE + 7)
#define	GPIOH_8			(GPIOH_BASE + 8)
#define	GPIOH_9			(GPIOH_BASE + 9)
#define	GPIOH_10		(GPIOH_BASE + 10)


/* boot [11-0] */
#define	BOOT_BASE		(GPIOH_BASE + GPIOH_SIZE)
#define	BOOT_SIZE		(12)

#define	BOOT_0			(BOOT_BASE + 0)
#define	BOOT_1			(BOOT_BASE + 1)
#define	BOOT_2			(BOOT_BASE + 2)
#define	BOOT_3			(BOOT_BASE + 3)
#define	BOOT_4			(BOOT_BASE + 4)
#define	BOOT_5			(BOOT_BASE + 5)
#define	BOOT_6			(BOOT_BASE + 6)
#define	BOOT_7			(BOOT_BASE + 7)
#define	BOOT_8			(BOOT_BASE + 8)
#define	BOOT_9			(BOOT_BASE + 9)
#define	BOOT_10			(BOOT_BASE + 10)
#define	BOOT_11			(BOOT_BASE + 11)


/* gpioc [5-0] */
#define	GPIOC_BASE		(BOOT_BASE + BOOT_SIZE)
#define	GPIOC_SIZE		(6)

#define	GPIOC_0			(GPIOC_BASE + 0)
#define	GPIOC_1			(GPIOC_BASE + 1)
#define	GPIOC_2			(GPIOC_BASE + 2)
#define	GPIOC_3			(GPIOC_BASE + 3)
#define	GPIOC_4			(GPIOC_BASE + 4)
#define	GPIOC_5			(GPIOC_BASE + 5)

/* gpiodv [10-0] */
#define	GPIODV_BASE		(GPIOC_BASE + GPIOC_SIZE)
#define	GPIODV_SIZE		(11)

#define	GPIODV_0		(GPIODV_BASE + 0)
#define	GPIODV_1		(GPIODV_BASE + 1)
#define	GPIODV_2		(GPIODV_BASE + 2)
#define	GPIODV_3		(GPIODV_BASE + 3)
#define	GPIODV_4		(GPIODV_BASE + 4)
#define	GPIODV_5		(GPIODV_BASE + 5)
#define	GPIODV_6		(GPIODV_BASE + 6)
#define	GPIODV_7		(GPIODV_BASE + 7)
#define	GPIODV_8		(GPIODV_BASE + 8)
#define	GPIODV_9		(GPIODV_BASE + 9)
#define	GPIODV_10		(GPIODV_BASE + 10)

/* gpiow [15-0] */
#define	GPIOW_BASE		(GPIODV_BASE + GPIODV_SIZE)
#define	GPIOW_SIZE		(16)

#define	GPIOW_0			(GPIOW_BASE + 0)
#define	GPIOW_1			(GPIOW_BASE + 1)
#define	GPIOW_2			(GPIOW_BASE + 2)
#define	GPIOW_3			(GPIOW_BASE + 3)
#define	GPIOW_4			(GPIOW_BASE + 4)
#define	GPIOW_5			(GPIOW_BASE + 5)
#define	GPIOW_6			(GPIOW_BASE + 6)
#define	GPIOW_7			(GPIOW_BASE + 7)
#define	GPIOW_8			(GPIOW_BASE + 8)
#define	GPIOW_9			(GPIOW_BASE + 9)
#define	GPIOW_10		(GPIOW_BASE + 10)
#define	GPIOW_11		(GPIOW_BASE + 11)
#define	GPIOW_12		(GPIOW_BASE + 12)
#define	GPIOW_13		(GPIOW_BASE + 13)
#define	GPIOW_14		(GPIOW_BASE + 14)
#define	GPIOW_15		(GPIOW_BASE + 15)

/* gpioy [27-0] */
#define	GPIOY_BASE		(GPIOW_BASE + GPIOW_SIZE)
#define	GPIOY_SIZE		(28)

#define	GPIOY_0			(GPIOY_BASE + 0)
#define	GPIOY_1			(GPIOY_BASE + 1)
#define	GPIOY_2			(GPIOY_BASE + 2)
#define	GPIOY_3			(GPIOY_BASE + 3)
#define	GPIOY_4			(GPIOY_BASE + 4)
#define	GPIOY_5			(GPIOY_BASE + 5)
#define	GPIOY_6			(GPIOY_BASE + 6)
#define	GPIOY_7			(GPIOY_BASE + 7)
#define	GPIOY_8			(GPIOY_BASE + 8)
#define	GPIOY_9			(GPIOY_BASE + 9)
#define	GPIOY_10		(GPIOY_BASE + 10)
#define	GPIOY_11		(GPIOY_BASE + 11)
#define	GPIOY_12		(GPIOY_BASE + 12)
#define	GPIOY_13		(GPIOY_BASE + 13)
#define	GPIOY_14		(GPIOY_BASE + 14)
#define	GPIOY_15		(GPIOY_BASE + 15)
#define	GPIOY_16		(GPIOY_BASE + 16)
#define	GPIOY_17		(GPIOY_BASE + 17)
#define	GPIOY_18		(GPIOY_BASE + 18)
#define	GPIOY_19		(GPIOY_BASE + 19)
#define	GPIOY_20		(GPIOY_BASE + 20)
#define	GPIOY_21		(GPIOY_BASE + 21)
#define	GPIOY_22		(GPIOY_BASE + 22)
#define	GPIOY_23		(GPIOY_BASE + 23)
#define	GPIOY_24		(GPIOY_BASE + 24)
#define	GPIOY_25		(GPIOY_BASE + 25)
#define	GPIOY_26		(GPIOY_BASE + 26)
#define	GPIOY_27		(GPIOY_BASE + 27)


/* test_n */
#define	GPIO_TEST_N_BASE	(GPIOY_BASE + GPIOY_SIZE)
#define	GPIO_TEST_N_SIZE	(1)

#define	GPIO_TEST_N		(GPIO_TEST_N_BASE + 0)

/* usb_ */
#define GPIO_USB_BASE		(GPIO_TEST_N_BASE + GPIO_TEST_N_SIZE)
#define GPIO_USB_SIZE		(2)

#define GPIO_USB_DPDM		(GPIO_USB_BASE + 0)
#define GPIO_USB_DMDP		(GPIO_USB_BASE + 1)


/* AO REG */
#define	AO		0x10
#define	AO2		0x11

#endif
