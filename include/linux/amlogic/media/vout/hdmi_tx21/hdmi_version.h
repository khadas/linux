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
	"V03.00.00.00.00.02.01 [20220707] [EDID] add hdr_priority  = 1 parse hdr10+\n"
#endif // __HDMI21_VERSION_H__
