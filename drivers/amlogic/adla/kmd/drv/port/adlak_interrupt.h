/*******************************************************************************
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_interrupt.h
 * @brief
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   	Who				Date				Changes
 * ----------------------------------------------------------------------------
 * 1.00a shiwei.sun@amlogic.com	2021/06/05	Initial release
 * </pre>
 *
 ******************************************************************************/

#ifndef __ADLAK_INTERRUPT_H__
#define __ADLAK_INTERRUPT_H__

/***************************** Include Files *********************************/
#include "adlak_typedef.h"
#ifdef __cplusplus
extern "C" {
#endif

/************************** Constant Definitions *****************************/
/**
 *  ADLAK  Interrupts status
 */
#define ADLAK_IRQ_NONE 0x0
#define ADLAK_IRQ_QEMPTY 0x1
#define ADLAK_IRQ_DONE 0x2
#define ADLAK_IRQ_EXCEP 0x4
#define ADLAK_IRQ_FAULT 0x8

/**************************Global Variable************************************/

/**************************Type Definition and Structure**********************/

/************************** Function Prototypes ******************************/
int  adlak_irq_init(void *data);
void adlak_irq_deinit(void *data);

#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_INTERRUPT_H__ end define*/
