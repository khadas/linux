/*
 * include/dt-bindings/gpio/txl.h
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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

#ifndef __DT_BINDINGS_GPIO_TXL_H
#define __DT_BINDINGS_GPIO_TXL_H

/*
 * NOTICE: The gpio number sequence according to the gpio interrupts.
 */


/* AO Bank */
#define GPIO_AO_BASE		(0)
#define	GPIO_AO_SIZE		(12)

/* gpioao */
#define GPIOAO_BASE		(0)
#define	GPIOAO_SIZE		(12)

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


/* EE Bank */
#define GPIO_EE_BASE		(0)

/* gpioz */
#define	GPIOZ_BASE		(0)
#define	GPIOZ_SIZE		(22)

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
#define	GPIOZ_20		(GPIOZ_BASE + 20)
#define	GPIOZ_21		(GPIOZ_BASE + 21)


/* gpioh */
#define	GPIOH_BASE		(GPIOZ_BASE + GPIOZ_SIZE) /* 22 */
#define	GPIOH_SIZE		(10)

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


/* boot */
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


/* card */
#define	CARD_BASE		(BOOT_BASE + BOOT_SIZE)
#define	CARD_SIZE		(7)

#define	CARD_0			(CARD_BASE + 0)
#define	CARD_1			(CARD_BASE + 1)
#define	CARD_2			(CARD_BASE + 2)
#define	CARD_3			(CARD_BASE + 3)
#define	CARD_4			(CARD_BASE + 4)
#define	CARD_5			(CARD_BASE + 5)
#define	CARD_6			(CARD_BASE + 6)

/* gpiodv */
#define	GPIODV_BASE		(CARD_BASE + CARD_SIZE)
#define	GPIODV_SIZE		(12)

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
#define	GPIODV_11		(GPIODV_BASE + 11)

/* gpiow */
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

/* gpioclk */
#define	GPIOCLK_BASE		(GPIOW_BASE + GPIOW_SIZE)
#define	GPIOCLK_SIZE		(2)

#define	GPIOCLK_0		(GPIOCLK_BASE + 0)
#define	GPIOCLK_1		(GPIOCLK_BASE + 1)

/* test_n */
#define	GPIO_TEST_N_BASE	(GPIOCLK_BASE + GPIOCLK_SIZE)
#define	GPIO_TEST_N_SIZE	(1)

#define	GPIO_TEST_N		(GPIO_TEST_N_BASE + 0)

/* usb_ */
#define GPIO_USB_BASE		(GPIO_TEST_N_BASE + GPIO_TEST_N_SIZE)
#define GPIO_USB_SIZE		(2)

#define GPIO_USB_DPDM		(GPIO_USB_BASE + 0)
#define GPIO_USB_DMDP		(GPIO_USB_BASE + 1)

#define GPIO_EE_SIZE		(84)

/* AO REG */
#define	AO		0x10
#define	AO2		0x11

#endif
