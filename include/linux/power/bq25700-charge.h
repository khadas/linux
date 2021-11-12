/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __CHARGER_BQ25700_H_
#define __CHARGER_BQ25700_H_

#define CHARGER_CURRENT_EVENT	0x01
#define INPUT_CURRENT_EVENT	0x02

void bq25700_charger_set_current(unsigned long event, int current_value);

#endif /* __CHARGER_BQ25700_H_ */
