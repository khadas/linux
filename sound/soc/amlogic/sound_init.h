/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __SOUND_INIT_H__
#define __SOUND_INIT_H__

int __init earc_init(void);
int __init audio_clocks_init(void);
int __init auge_snd_iomap_init(void);
int __init audio_controller_init(void);
int __init asoc_simple_init(void);
int __init audio_ddr_init(void);
int __init effect_platform_init(void);
int __init extn_init(void);
int __init audio_locker_init(void);
int __init loopback_init(void);
int __init pdm_init(void);
int __init pwrdet_init(void);
int __init resample_drv_init(void);
int __init spdif_init(void);
int __init tdm_init(void);
int __init vad_drv_init(void);
int __init vad_dev_init(void);

void __exit vad_dev_exit(void);
void __exit vad_drv_exit(void);
void __exit tdm_exit(void);
void __exit spdif_exit(void);
void __exit resample_drv_exit(void);
void __exit pwrdet_exit(void);
void __exit pdm_exit(void);
void __exit loopback_exit(void);
void __exit audio_locker_exit(void);
void __exit extn_exit(void);
void __exit effect_platform_exit(void);
void __exit audio_ddr_exit(void);
void __exit asoc_simple_exit(void);
void __exit audio_controller_exit(void);
void __exit auge_snd_iomap_exit(void);
void __exit audio_clocks_exit(void);
void __exit earc_exit(void);
#endif
