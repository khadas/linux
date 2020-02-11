/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef __VAD_API_H__
#define __VAD_API_H__

int register_vad_callback(int (*callback)(char *, int, int, int, int));
void unregister_vad_callback(void);

#endif /* __VAD_API_H__ */
