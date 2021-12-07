/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef __VAD_API_H__
#define __VAD_API_H__

/* only supports 1 channels, 16bit, 16Khz.
 * And, the wIvwWrite function only supports 160 frames(10ms) per write.
 */
int aml_asr_feed(char *buf, u32 frames);
int aml_asr_enable(void);
int aml_asr_disable(void);

#endif /* __VAD_API_H__ */
