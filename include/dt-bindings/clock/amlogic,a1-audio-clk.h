/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __A1_AUDIO_CLK_H__
#define __A1_AUDIO_CLK_H__

/*
 * CLKID audio index values
 */

/* Gate En0 */
#define CLKID_AUDIO_GATE_DDR_ARB     0
#define CLKID_AUDIO_GATE_TDMINA      1
#define CLKID_AUDIO_GATE_TDMINB      2
#define CLKID_AUDIO_GATE_TDMINLB     3
#define CLKID_AUDIO_GATE_LOOPBACK    4
#define CLKID_AUDIO_GATE_TDMOUTA     5
#define CLKID_AUDIO_GATE_TDMOUTB     6
#define CLKID_AUDIO_GATE_FRDDRA      7
#define CLKID_AUDIO_GATE_FRDDRB      8
#define CLKID_AUDIO_GATE_TODDRA      9
#define CLKID_AUDIO_GATE_TODDRB      10
#define CLKID_AUDIO_GATE_SPDIFIN     11
#define CLKID_AUDIO_GATE_RESAMPLEA   12
#define CLKID_AUDIO_GATE_EQDRC       13
#define CLKID_AUDIO_GATE_LOCKER      14

#define CLKID_AUDIO_GATE_MAX         15

#define CLKID_AUDIO2_GATE_DDR_ARB    15
#define CLKID_AUDIO2_GATE_PDM        16
#define CLKID_AUDIO2_GATE_TDMIN_VAD  17
#define CLKID_AUDIO2_GATE_TODDR_VAD  18
#define CLKID_AUDIO2_GATE_TOVAD      19
#define CLKID_AUDIO2_GATE_TOAUDIOTOP 20

#define CLKID_AUDIO2_GATE_MAX        21

#define MCLK_BASE                    CLKID_AUDIO2_GATE_MAX
#define CLKID_AUDIO_MCLK_A           (MCLK_BASE + 0)
#define CLKID_AUDIO_MCLK_B           (MCLK_BASE + 1)
#define CLKID_AUDIO_MCLK_C           (MCLK_BASE + 2)
#define CLKID_AUDIO_MCLK_D           (MCLK_BASE + 3)

#define CLKID_AUDIO_SPDIFIN          (MCLK_BASE + 4)
#define CLKID_AUDIO_RESAMPLEA        (MCLK_BASE + 5)
#define CLKID_AUDIO_LOCKER_OUT       (MCLK_BASE + 6)
#define CLKID_AUDIO_LOCKER_IN        (MCLK_BASE + 7)
#define CLKID_AUDIO_EQDRC            (MCLK_BASE + 8)

/* AUDIO TOP VAD */
#define CLKID_AUDIO2_MCLK_VAD        (MCLK_BASE + 9)
#define CLKID_AUDIO2_VAD_CLK         (MCLK_BASE + 10)
#define CLKID_AUDIO2_PDM_DCLK        (MCLK_BASE + 11)
#define CLKID_AUDIO2_PDM_SYSCLK      (MCLK_BASE + 12)

#define NUM_AUDIO_CLKS               (MCLK_BASE + 13)

#endif /* __A1_AUDIO_CLK_H__ */

