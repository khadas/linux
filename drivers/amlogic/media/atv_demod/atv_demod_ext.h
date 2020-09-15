/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __ATV_DEMOD_EXT_H__
#define __ATV_DEMOD_EXT_H__

/* This module is atv demod interacts with other modules */

extern bool aml_fe_has_hook_up(void);
extern bool aml_fe_hook_call_get_fmt(int *fmt);
extern bool aml_fe_hook_call_set_mode(bool mode);

extern void atvdemod_power_switch(bool on);

#endif /* __ATV_DEMOD_EXT_H__ */
