/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_AUDIO_UTILS_H__
#define __AML_AUDIO_UTILS_H__

#include <sound/soc.h>
#include <sound/control.h>

int snd_card_add_kcontrols(struct snd_soc_card *card);
void audio_locker_set(int enable);
int audio_locker_get(void);
void fratv_enable(bool enable);
void cec_arc_enable(int src, bool enable);
#endif
