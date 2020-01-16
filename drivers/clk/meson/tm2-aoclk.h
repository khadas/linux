/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __TM2_AOCLKC_H
#define __TM2_AOCLKC_H

/*
 * CLKID index values
 *
 * These indices are entirely contrived and do not map onto the hardware.
 * It has now been decided to expose everything by default in the DT header:
 * include/dt-bindings/clock/g12a-aoclkc.h. Only the clocks ids we don't want
 * to expose, such as the internal muxes and dividers of composite clocks,
 * will remain defined here.
 */
#define CLKID_AO_SAR_ADC_DIV	21
#define CLKID_AO_32K_PRE	22
#define CLKID_AO_32K_DIV	23
#define CLKID_AO_32K_SEL	24
#define CLKID_AO_CEC_PRE	25
#define CLKID_AO_CEC_DIV	26
#define CLKID_AO_CEC_SEL	27

#define NR_CLKS	28

#include <dt-bindings/clock/tm2-aoclkc.h>

#endif /* __TM2_AOCLKC_H */
