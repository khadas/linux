/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __EARC_H__
#define __EARC_H__
#include "../common/iec_info.h"
#include "earc_hw.h"
/* earc probe is at arch_initcall stage which is earlier to normal driver */
bool is_earc_spdif(void);
void aml_earctx_enable(bool enable);
int sharebuffer_earctx_prepare(struct snd_pcm_substream *substream,
	struct frddr *fr, enum aud_codec_types type, int lane_i2s);
bool aml_get_earctx_enable(void);
bool get_earcrx_chnum_mult_mode(void);
enum attend_type aml_get_earctx_connected_device_type(void);
bool aml_get_earctx_reset_hpd(void);
void aml_earctx_enable_d2a(int enable);
void aml_earctx_dmac_mute(int enable);

#endif

