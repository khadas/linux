/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_AUDIO_LOOPBACK_H__
#define __AML_AUDIO_LOOPBACK_H__
#include <sound/soc.h>
#include <sound/tlv.h>

/* datain src
 * [4]: pdmin;
 * [3]: spdifin;
 * [2]: tdmin_c;
 * [1]: tdmin_b;
 * [0]: tdmin_a;
 */
enum datain_src {
	DATAIN_TDMA = 0,
	DATAIN_TDMB,
	DATAIN_TDMC,
	DATAIN_SPDIF,
	DATAIN_PDM,
	DATAIN_LOOPBACK,
	DATAIN_TDMD,
	DATAIN_PDMB,
	DATAIN_MAX
};

/* datalb src
 * /tdmin_lb src
 *     [0]: tdmoutA
 *     [1]: tdmoutB
 *     [2]: tdmoutC
 *     [3]: PAD_tdminA
 *     [4]: PAD_tdminB
 *     [5]: PAD_tdminC
 * /spdifin_lb src
 *     spdifout_a
 *     spdifout_b
 */
enum datalb_src {
	TDMINLB_TDMOUTA = 0,
	TDMINLB_TDMOUTB,
	TDMINLB_TDMOUTC,
	TDMINLB_TDMOUTD,
	TDMINLB_PAD_TDMINA,
	TDMINLB_PAD_TDMINB,
	TDMINLB_PAD_TDMINC,
	TDMINLB_PAD_TDMIND,
	TDMINLB_PAD_TDMINA_D,
	TDMINLB_PAD_TDMINB_D,
	TDMINLB_PAD_TDMINC_D,
	TDMINLB_HDMIRX,
	TDMINLB_ACODEC,

	SPDIFINLB_SPDIFOUTA,
	SPDIFINLB_SPDIFOUTB,
	TDMINLB_SRC_MAX
};

unsigned int loopback_get_lb_channel(int id);

#endif
