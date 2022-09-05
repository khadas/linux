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
	"V03.00.00.00.03.03.10 [20220919] [BUG] Don't reset variables when parse a new block\n"

#endif // __HDMI21_VERSION_H__
