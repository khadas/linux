/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2023 Amlogic, Inc. All rights reserved.
 */

#ifndef _BRIDGE_UAC_EXT_H
#define _BRIDGE_UAC_EXT_H

int uac_pcm_start_capture(void);
int uac_pcm_start_playback(void);
int uac_pcm_stop_capture(void);
int uac_pcm_stop_playback(void);

int uac_pcm_get_capture_status(void);
int uac_pcm_get_playback_status(void);
int uac_pcm_get_capture_hw(int *chmask, int *srate, int *ssize);
int uac_pcm_get_playback_hw(int *chmask, int *srate, int *ssize);
int uac_pcm_write_data(char *buf, unsigned int size);
int uac_pcm_read_data(char *buf, unsigned int size);
int uac_pcm_ctl_capture(int cmd, int value);
int uac_pcm_ctl_playback(int cmd, int value);
int uac_pcm_get_default_volume_capture(void);
int uac_pcm_get_default_volume_playback(void);

#endif /*_BRIDGE_UAC_EXT_H*/

