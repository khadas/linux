/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI20_VERSION_H__
#define __HDMI20_VERSION_H__
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

#define HDMITX20_VERSIONS_LOG \
	"V03.00.00.00.01.00.00 [20220614] [HDCP] correct the hdcp init condition\n" \
	"V03.00.00.00.01.01.00 [20220616] [EDID] add MPEG-H edid parse\n" \
	"V03.00.00.00.01.01.01 [20220623] [NEWFEATURE] add aspect ratio support\n"\
	"V03.00.00.00.01.02.01 [20220707] [EDID] add hdr_priority  = 1 parse hdr10+\n" \
	"V03.00.00.00.01.03.01 [20220708] [EDID] parse dtd for dvi case\n" \
	"V03.00.00.00.01.03.02 [20220711] [COMP] add hdcp version protect for drm\n" \
	"V03.00.00.00.01.03.03 [20220713] [SYSFS] add phy show sysfs\n" \
	"V03.00.01.00.01.03.03 [20220713] [AUDIO] audio callback turn on the judgement\n" \
	"V03.00.01.00.01.03.04 [20220803] [BUG] echo 1 > /sys/class/display/bist lead to panic\n" \
	"V03.00.01.00.01.03.05 [20220903] [BUG] add DDC reset before do EDID transaction\n" \
	"V03.00.01.00.01.03.06 [20220919] [BUG] Don't reset variables when parse a new block\n" \
	"V03.00.01.00.01.03.07 [20220926] [BUG] enable null packt for special TV\n" \
	"V03.00.02.00.01.03.07 [20221018] [AUD] optimise the audio setting flow\n" \
	"V03.00.02.00.01.03.08 [20221021] [BUG] not read EDID again if EDID already read done\n" \
	"V03.00.02.00.01.03.09 [20221025] [COM] when set mode 4x3 and 16x9, return valid mode 1\n" \
	"V03.00.02.00.01.03.10 [20221031] [NEW] add new format 2560x1440p60hz\n" \
	"V03.00.02.00.01.04.10 [20221102] [EDID] adjust edid parsing for tv_ts\n" \
	"V03.00.02.00.01.04.11 [20221111] [HPD] add hpd GPI status\n" \
	"V03.00.02.00.01.04.12 [20221117] [BUG] remove audio mute when adaptive from sdr to hdr\n" \
	"V03.00.02.00.01.04.13 [20221121] [NEW] add dump_debug_reg for debug\n" \
	"V03.00.02.00.02.04.13 [20221130] [HDCP] optimise drm hdcp flow when switch mode\n" \
	"V03.00.02.00.02.05.13 [20221214] [EDID] Adjust the maximum supported TMDS clk\n" \
	"V03.00.02.00.03.05.13 [20221215] [HDCP] dma map and sync for allocat for hdcp22 panic\n" \
	"V03.01.02.00.03.05.13 [20221221] [PHY] test pixel clk and adj phy reg for 70hz issue\n" \
	"V03.01.02.00.03.05.14 [20221230] [COM] hdmitx_setup_attr slab out of bounds\n" \
	"V03.01.02.00.03.05.15 [20230112] [COM] adjust avi packet placement\n" \
	"V03.01.02.00.03.05.16 [20230210] [COM] not send hdmi/audio uevent under suspend\n" \
	"V03.01.02.00.03.05.17 [20230328] [NEW] optimise color depth for hdr output\n" \
	"V03.01.02.00.03.05.18 [20230404] [COM] Extend the check time for linux hdcp\n"

#endif // __HDMI20_VERSION_H__
