/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI21_VERSION_H__
#define __HDMI21_VERSION_H__
/****************************************************************/
/***************** HDMITx Version Description *******************/
/* V[aa].[bb].[cc].[dd].[ee].[ff].[gg] */
/* aa: big version, kernel version, New IP
 *     01: kernel 4.9
 *     02: kernel 4.9-q
 *     03: kernel 5.4
 */
/* bb: phy update */
/* cc: audio update */
/* dd: HDR/DV update */
/* ee: HDCP update */
/* ff: EDID update */
/* gg: bugfix, compatibility, etc */
/****************************************************************/

#define HDMITX21_VERSIONS_LOG \
	"V03.00.00.00.00.01.00 [20220616] [EDID] add MPEG-H edid parse\n" \
	"V03.00.00.00.00.01.01 [20220628] [Compatibility] optimize for hotplug during bootup\n"\
	"V03.00.00.00.00.02.01 [20220707] [EDID] add hdr_priority  = 1 parse hdr10+\n" \
	"V03.00.00.00.01.02.01 [20220708] [HDCP] fix hdcp1.4 repeater cts issues\n" \
	"V03.00.00.00.01.02.02 [20220711] [COMP] add hdcp version protect for drm\n" \
	"V03.00.00.00.01.03.02 [20220711] [EDID] HF1-23 add the ddc delay to 20ms\n" \
	"V03.00.00.00.01.03.03 [20220713] [SYSFS] add phy show sysfs\n" \
	"V03.00.00.00.02.03.03 [20220715] [HDCP] T7 DVI 1080p60 1A-09 test fail\n" \
	"V03.00.00.00.02.03.04 [20220726] [COMP] add avmute interface\n" \
	"V03.00.00.00.03.03.04 [20220805] [HDCP] fix hdcp1.4 repeater cts 3C-II-07\n" \
	"V03.00.00.00.03.03.05 [20220811] [NEWF] hdmitx21 add aspect ratio support\n" \
	"V03.00.00.00.03.03.06 [20220829] [NEWF] support avi content type\n" \
	"V03.00.00.00.03.03.07 [20220901] [NEWF] add dual vsif interface for allm\n" \
	"V03.00.00.00.03.03.08 [20220903] [BUG] add DDC reset before do EDID transaction\n" \
	"V03.00.00.00.03.03.09 [20220919] [BUG] y422 mapping and Enable the dither\n" \
	"V03.00.00.00.03.03.10 [20220919] [BUG] Don't reset variables when parse a new block\n" \
	"V03.00.00.00.03.03.11 [20221021] [BUG] not read EDID again if EDID already read done\n" \
	"V03.00.00.00.03.03.12 [20221025] [COM] when set mode 4x3 and 16x9, return valid mode 1\n" \
	"V03.00.00.01.03.03.12 [20221107] [AUD] fix no ddp mat audio issue\n" \
	"V03.00.00.01.03.03.13 [20221128] [FRL] fix random no FRL output issue\n" \
	"V03.00.00.01.03.03.14 [20221129] [FRL] update the cur_enc_ppc on FRL TMDS modes\n" \
	"V03.00.00.01.03.03.15 [20221202] [FRL] T7 not support FRL\n" \
	"V03.00.00.01.03.03.16 [20221212] [FRL] add FRL link training procedure\n" \
	"V03.01.00.01.03.03.16 [20221219] [PHY] refine phy setting sequence\n" \
	"V03.01.00.01.03.03.17 [20221226] [CLK] refine clock setting sequence\n" \
	"V03.02.00.01.03.03.17 [20221221] [PHY] test pixel clk and adj phy reg for 70hz issue\n" \
	"V03.02.00.01.03.03.18 [20221230] [COM] hdmitx_setup_attr slab out of bounds\n" \
	"V03.02.00.01.03.03.19 [20230104] [COM] set the same sname and send H14b-VSIF\n" \
	"V03.02.00.01.03.03.20 [20230111] [NEW] add support for 480i/576i\n" \
	"V03.02.00.01.04.03.20 [20230203] [HDCP] hdcp 1b-10 fail, need send smng times\n" \
	"V03.02.01.01.04.03.20 [20230210] [AUD] add the acr_clk selection under FRL\n" \
	"V03.02.01.01.04.04.20 [20230210] [EDID] HFR1-69 edid parse EEODB\n"

#endif // __HDMI21_VERSION_H__
