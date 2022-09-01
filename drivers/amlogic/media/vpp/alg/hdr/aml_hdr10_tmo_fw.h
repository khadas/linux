/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_HDR10_TMO_FW_H
#define _AML_HDR10_TMO_FW_H

void hdr10_tmo_gen_data(u32 *oo_gain, int *hdr_hist);
int hdr10_tmo_debug(char **param);
void hdr10_tmo_params_show(void);
void hdr10_tmo_set_reg(struct hdr_tmo_sw *pre_tmo_reg);
void hdr10_tmo_get_reg(struct hdr_tmo_sw *pre_tmo_reg_s);
void hdr10_tmo_adb_show(char *str);

#endif

