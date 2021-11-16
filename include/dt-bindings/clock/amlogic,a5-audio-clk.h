/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __A5_AUDIO_CLK_H__
#define __A5_AUDIO_CLK_H__

/*
 * CLKID audio index values
 */

/* Audio CLK Gate En0 */
#define CLKID_AUDIO_GATE_TDMINA                 0
#define CLKID_AUDIO_GATE_TDMINLB                1
#define CLKID_AUDIO_GATE_TDMOUTA                2
#define CLKID_AUDIO_GATE_TDMOUTB                3
#define CLKID_AUDIO_GATE_FRDDRA                 4
#define CLKID_AUDIO_GATE_FRDDRB                 5
#define CLKID_AUDIO_GATE_FRDDRC                 6
#define CLKID_AUDIO_GATE_TODDRA                 7
#define CLKID_AUDIO_GATE_TODDRB                 8
#define CLKID_AUDIO_GATE_LOOPBACKA              9
#define CLKID_AUDIO_GATE_SPDIFIN                10
#define CLKID_AUDIO_GATE_SPDIFOUT_A             11
#define CLKID_AUDIO_GATE_RESAMPLEA              12
#define CLKID_AUDIO_GATE_EQDRC                  13
#define CLKID_AUDIO_GATE_AUDIOLOCKER            14

#define CLKID_AUDIO_GATE_MAX                    15

/* Audio2 CLK Gate En0 */
#define CLKID_AUDIO2_GATE_DDR_ARB               15
#define CLKID_AUDIO2_GATE_PDM                   16
#define CLKID_AUDIO2_GATE_TDMINC                17
#define CLKID_AUDIO2_GATE_TODDRC                18
#define CLKID_AUDIO2_GATE_TOVAD                 19
#define CLKID_AUDIO2_GATE_CLKRST                20
#define CLKID_AUDIO2_GATE_LRQFIFOCNT1           21
#define CLKID_AUDIO2_GATE_LRQFIFOCNT2           22

#define CLKID_AUDIO2_GATE_MAX        23

#define MCLK_BASE                               CLKID_AUDIO2_GATE_MAX
#define CLKID_AUDIO_MCLK_A                      (MCLK_BASE + 0)
#define CLKID_AUDIO_MCLK_B                      (MCLK_BASE + 1)
#define CLKID_AUDIO_MCLK_C                      (MCLK_BASE + 2)
#define CLKID_AUDIO_MCLK_D                      (MCLK_BASE + 3)
#define CLKID_AUDIO_MCLK_E                      (MCLK_BASE + 4)
#define CLKID_AUDIO_MCLK_F                      (MCLK_BASE + 5)

#define CLKID_AUDIO_SPDIFIN                     (MCLK_BASE + 6)
#define CLKID_AUDIO_SPDIFOUT_A                  (MCLK_BASE + 7)
#define CLKID_AUDIO_RESAMPLE_A                  (MCLK_BASE + 8)
#define CLKID_AUDIO_LOCKER_OUT                  (MCLK_BASE + 9)
#define CLKID_AUDIO_LOCKER_IN                   (MCLK_BASE + 10)
#define CLKID_AUDIO_EQDRC                       (MCLK_BASE + 11)

#define CLKID_AUDIO_MCLK_PAD0                   (MCLK_BASE + 12)
#define CLKID_AUDIO_MCLK_PAD1                   (MCLK_BASE + 13)
#define CLKID_AUDIO_MCLK_PAD2                   (MCLK_BASE + 14)
#define CLKID_AUDIO_MCLK_PAD3                   (MCLK_BASE + 15)

/* AUDIO TOP VAD */
#define CLKID_AUDIO2_MCLK_VAD        (MCLK_BASE + 16)
#define CLKID_AUDIO2_VAD_CLK         (MCLK_BASE + 17)
#define CLKID_AUDIO2_PDM_DCLK        (MCLK_BASE + 18)
#define CLKID_AUDIO2_PDM_SYSCLK      (MCLK_BASE + 19)

#define NUM_AUDIO_CLKS                          (MCLK_BASE + 20)
#endif /* __A5_AUDIO_CLK_H__ */
