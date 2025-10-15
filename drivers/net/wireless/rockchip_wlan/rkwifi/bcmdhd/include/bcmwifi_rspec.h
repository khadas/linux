/*
 * Common OS-independent driver header for rate management.
 *
 * Copyright (C) 2025 Synaptics Incorporated. All rights reserved.
 *
 * This software is licensed to you under the terms of the
 * GNU General Public License version 2 (the "GPL") with Broadcom special exception.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION
 * DOES NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES,
 * SYNAPTICS' TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT
 * EXCEED ONE HUNDRED U.S. DOLLARS
 *
 * Copyright (C) 2025, Broadcom.
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *
 * <<Broadcom-WL-IPTag/Dual:>>
 */

#ifndef _bcmwifi_rspec_h_
#define _bcmwifi_rspec_h_

#include <typedefs.h>

#ifdef BCMDRIVER
#define BCMWIFI_RSPEC_BW_COND	/* Conditional channel width support */
#ifdef WL_BW320MHZ
#define BCMWIFI_BW320MHZ	/* 320Mhz channel width support */
#endif
#endif /* BCMDRIVER */

#ifndef BCMWIFI_RSPEC_BW_COND
#define BCMWIFI_BW320MHZ
#endif /* !BCMWIFI_RSPEC_BW_COND */

/**
 * ===================================================================================
 * rate spec : holds rate and mode specific information required to generate a tx frame.
 * Legacy CCK and OFDM information is held in the same manner as was done in the past.
 * (in the lower byte) the upper 3 bytes primarily hold MIMO specific information
 * ===================================================================================
 */
typedef uint32 ratespec_t;
typedef uint32 ratespec_bw_t;

/* Rate spec. definitions */
/* for WL_RSPEC_ENCODING field >= WL_RSPEC_ENCODING_HE, backward compatible */
#define WL_RSPEC_RATE_MASK	0x000000FFu	/**< Legacy rate or MCS or MCS + NSS */

#define WL_RSPEC_TXEXP_MASK	0x00000300u	/**< Tx chain expansion beyond Nsts */
#define WL_RSPEC_TXEXP_SHIFT	8u

#define WL_RSPEC_UHR_GI_MASK	0x00000C00u	/* UHR GI indices */
#define WL_RSPEC_UHR_GI_SHIFT	10u
#define WL_RSPEC_EHT_GI_MASK	0x00000C00u	/* EHT GI indices */
#define WL_RSPEC_EHT_GI_SHIFT	10u
#define WL_RSPEC_HE_GI_MASK	0x00000C00u	/* HE GI indices */
#define WL_RSPEC_HE_GI_SHIFT	10u

#define WL_RSPEC_UHR_UEQM_MASK	0x00003000u	/* UHR UEQM */
#define WL_RSPEC_UHR_UEQM_SHIFT	12u
#define WL_RSPEC_UHR_UEQM_ENAB	0x00001000u	/* UHR UEQM Enable/Disable */
#define WL_RSPEC_UHR_UEQM_STEP	0x00002000u	/* UHR UEQM Steps */

#define WL_RSPEC_UHR_ELR_MASK	0x0000C000u	/**< UHR ELR Mask */
#define WL_RSPEC_UHR_ELR_SHIFT	14u
#define WL_RSPEC_UHR_ELR_ENAB	0x00004000u	/**< UHR ELR Enable/Disable */
#define WL_RSPEC_UHR_ELR_MOD	0x00008000u	/**< UHR ELR Modulation */
#define WL_RSPEC_ER_MASK	0x0000C000u	/**< Range extension mask */
#define WL_RSPEC_ER_SHIFT	14u
#define WL_RSPEC_ER_TONE_MASK	0x00004000u	/**< Range extension tone config */
#define WL_RSPEC_ER_TONE_SHIFT	14u
#define WL_RSPEC_ER_ENAB_MASK	0x00008000u	/**< Range extension enable */
#define WL_RSPEC_ER_ENAB_SHIFT	15u

#define WL_RSPEC_BW_MASK	0x00070000u	/**< Band width */
#define WL_RSPEC_BW_SHIFT	16u

#define WL_RSPEC_DCM		0x00080000u	/**< Dual Carrier Modulation */
#define WL_RSPEC_DCM_SHIFT	19u
#define WL_RSPEC_STBC		0x00100000u	/**< STBC expansion, Nsts = 2 * Nss */
#define WL_RSPEC_TXBF		0x00200000u
#define WL_RSPEC_LDPC		0x00400000u
#define WL_RSPEC_SGI		0x00800000u	/* HT/VHT SGI indication */
#define WL_RSPEC_SHORT_PREAMBLE	0x00800000u	/**< DSSS short preable - Encoding 0 */
#if defined(WL11BE) || defined(WLC_SIGB_RX_11BE_RATE_DECODE) || defined(WL11BN)
#define WL_RSPEC_ENCODING_MASK	0x07000000u	/**< Encoding of RSPEC_RATE field */
#else
#define WL_RSPEC_ENCODING_MASK	0x03000000u	/**< Encoding of RSPEC_RATE field */
#endif
#define WL_RSPEC_ENCODING_SHIFT	24u

#define WL_RSPEC_UHR_MCSEXT_SHIFT	29u	/**< BIT4 of UHR MCS Index */
#define WL_RSPEC_UHR_MCSEXT_MASK	0x20000000u

#define WL_RSPEC_OVERRIDE_RATE	0x40000000u	/**< override rate only */
#define WL_RSPEC_OVERRIDE_MODE	0x80000000u	/**< override both rate & mode */

/* ======== RSPEC_UHR_GI|RSPEC_SGI fields for UHR ======== */
/* 11be Draft 0.1 Table 38-24:Common field for a UHR SU transmission and non-OFDMA transmission
 * to multiple users
 */
#define RSPEC_UHR_LTF_GI(rspec)	(((rspec) & WL_RSPEC_UHR_GI_MASK) >> WL_RSPEC_UHR_GI_SHIFT)
#define WL_RSPEC_UHR_2x_LTF_GI_0_8us	(0x0u)
#define WL_RSPEC_UHR_2x_LTF_GI_1_6us	(0x1u)
#define WL_RSPEC_UHR_4x_LTF_GI_0_8us	(0x2u)
#define WL_RSPEC_UHR_4x_LTF_GI_3_2us	(0x3u)

#define UHR_GI_TO_RSPEC(gi)	\
	((ratespec_t)(((gi) << WL_RSPEC_UHR_GI_SHIFT) & WL_RSPEC_UHR_GI_MASK))
#define UHR_GI_TO_RSPEC_SET(rspec, gi)	((rspec & (~WL_RSPEC_UHR_GI_MASK)) | \
					UHR_GI_TO_RSPEC(gi))

/* Macros for UHR LTF and GI */
#define UHR_IS_2X_LTF(gi)	(((gi) == WL_RSPEC_UHR_2x_LTF_GI_0_8us) || \
				((gi) == WL_RSPEC_UHR_2x_LTF_GI_1_6us))
#define UHR_IS_4X_LTF(gi)	(((gi) == WL_RSPEC_UHR_4x_LTF_GI_0_8us) || \
				((gi) == WL_RSPEC_UHR_4x_LTF_GI_3_2us))

#define UHR_IS_GI_0_8us(gi)	(((gi) == WL_RSPEC_UHR_2x_LTF_GI_0_8us) || \
				((gi) == WL_RSPEC_UHR_4x_LTF_GI_0_8us))
#define UHR_IS_GI_1_6us(gi)	((gi) == WL_RSPEC_UHR_2x_LTF_GI_1_6us)
#define UHR_IS_GI_3_2us(gi)	((gi) == WL_RSPEC_UHR_4x_LTF_GI_3_2us)

/* ======== RSPEC_EHT_GI|RSPEC_SGI fields for EHT ======== */
/* 11be Draft 0.4 Table 36-35:Common field for non-OFDMA transmission.
 * Table 36-32 Common field for OFDMA transmission
 */
#define RSPEC_EHT_LTF_GI(rspec)	(((rspec) & WL_RSPEC_EHT_GI_MASK) >> WL_RSPEC_EHT_GI_SHIFT)
#define WL_RSPEC_EHT_2x_LTF_GI_0_8us	(0x0u)
#define WL_RSPEC_EHT_2x_LTF_GI_1_6us	(0x1u)
#define WL_RSPEC_EHT_4x_LTF_GI_0_8us	(0x2u)
#define WL_RSPEC_EHT_4x_LTF_GI_3_2us	(0x3u)
#define WL_EHT_GI_TO_RSPEC(gi)	\
	((ratespec_t)(((gi) << WL_RSPEC_EHT_GI_SHIFT) & WL_RSPEC_EHT_GI_MASK))
#define WL_EHT_GI_TO_RSPEC_SET(rspec, gi)	((rspec & (~WL_RSPEC_EHT_GI_MASK)) | \
					WL_EHT_GI_TO_RSPEC(gi))

/* Macros for EHT LTF and GI */
#define EHT_IS_2X_LTF(gi)	(((gi) == WL_RSPEC_EHT_2x_LTF_GI_0_8us) || \
				((gi) == WL_RSPEC_EHT_2x_LTF_GI_1_6us))
#define EHT_IS_4X_LTF(gi)	(((gi) == WL_RSPEC_EHT_4x_LTF_GI_0_8us) || \
				((gi) == WL_RSPEC_EHT_4x_LTF_GI_3_2us))

#define EHT_IS_GI_0_8us(gi)	(((gi) == WL_RSPEC_EHT_2x_LTF_GI_0_8us) || \
				((gi) == WL_RSPEC_EHT_4x_LTF_GI_0_8us))
#define EHT_IS_GI_1_6us(gi)	((gi) == WL_RSPEC_EHT_2x_LTF_GI_1_6us)
#define EHT_IS_GI_3_2us(gi)	((gi) == WL_RSPEC_EHT_4x_LTF_GI_3_2us)

/* ======== RSPEC_HE_GI|RSPEC_SGI fields for HE ======== */

/* GI for HE */
#define RSPEC_HE_LTF_GI(rspec)	(((rspec) & WL_RSPEC_HE_GI_MASK) >> WL_RSPEC_HE_GI_SHIFT)
#define WL_RSPEC_HE_1x_LTF_GI_0_8us	(0x0u)
#define WL_RSPEC_HE_2x_LTF_GI_0_8us	(0x1u)
#define WL_RSPEC_HE_2x_LTF_GI_1_6us	(0x2u)
#define WL_RSPEC_HE_4x_LTF_GI_3_2us	(0x3u)
#define RSPEC_ISHEGI(rspec)	(RSPEC_HE_LTF_GI(rspec) > WL_RSPEC_HE_1x_LTF_GI_0_8us)
#define HE_GI_TO_RSPEC(gi)	(((ratespec_t)(gi) << WL_RSPEC_HE_GI_SHIFT) & WL_RSPEC_HE_GI_MASK)
#define HE_GI_TO_RSPEC_SET(rspec, gi)	((rspec & (~WL_RSPEC_HE_GI_MASK)) | \
					HE_GI_TO_RSPEC(gi))

/* Macros for HE LTF and GI */
#define HE_IS_1X_LTF(gi)	((gi) == WL_RSPEC_HE_1x_LTF_GI_0_8us)
#define HE_IS_2X_LTF(gi)	(((gi) == WL_RSPEC_HE_2x_LTF_GI_0_8us) || \
				((gi) == WL_RSPEC_HE_2x_LTF_GI_1_6us))
#define HE_IS_4X_LTF(gi)	((gi) == WL_RSPEC_HE_4x_LTF_GI_3_2us)

#define HE_IS_GI_0_8us(gi)	(((gi) == WL_RSPEC_HE_1x_LTF_GI_0_8us) || \
				((gi) == WL_RSPEC_HE_2x_LTF_GI_0_8us))
#define HE_IS_GI_1_6us(gi)	((gi) == WL_RSPEC_HE_2x_LTF_GI_1_6us)
#define HE_IS_GI_3_2us(gi)	((gi) == WL_RSPEC_HE_4x_LTF_GI_3_2us)

/* RSPEC Macros for extracting and using UHR UEQM */
#ifdef WL11BN
#define RSPEC_UHR_UEAQM_ENAB(rspec)		!!((rspec) & WL_RSPEC_UHR_UEQM_ENAB)
#else
#define RSPEC_UHR_UEAQM_ENAB(rspec)		FALSE
#endif
#define RSPEC_UHR_UEAQM_STEP(rspec)		!!((rspec) & WL_RSPEC_UHR_UEQM_STEP)

/* RSPEC Macros for extracting and using HE-ER and DCM */
#define RSPEC_HE_DCM(rspec)		(((rspec) & WL_RSPEC_DCM) >> WL_RSPEC_DCM_SHIFT)
#define RSPEC_HE_ER(rspec)		(((rspec) & WL_RSPEC_ER_MASK) >> WL_RSPEC_ER_SHIFT)
#ifdef WL11AX
#define RSPEC_HE_ER_ENAB(rspec)		(((rspec) & WL_RSPEC_ER_ENAB_MASK) >> \
					WL_RSPEC_ER_ENAB_SHIFT)
#else
#define RSPEC_HE_ER_ENAB(rspec)		FALSE
#endif
#define RSPEC_HE_ER_TONE(rspec)		(((rspec) & WL_RSPEC_ER_TONE_MASK) >> \
					WL_RSPEC_ER_TONE_SHIFT)

/* RSPEC Macros for extracting and using UHR ELR */
#ifdef WL11BN
#define RSPEC_UHR_ELR_ENAB(rspec)		!!((rspec) & WL_RSPEC_UHR_ELR_ENAB)
#else
#define RSPEC_UHR_ELR_ENAB(rspec)		FALSE
#endif
#define RSPEC_UHR_ELR_MOD(rspec)		!!((rspec) & WL_RSPEC_UHR_ELR_MOD)

/* ======== RSPEC_RATE field ======== */

/* Encoding 0 - legacy rate */
/* DSSS, CCK, and OFDM rates in [500kbps] units */
#define WL_RSPEC_LEGACY_RATE_MASK	0x0000007F
#define WLC_RATE_1M	2
#define WLC_RATE_2M	4
#define WLC_RATE_5M5	11
#define WLC_RATE_11M	22
#define WLC_RATE_6M	12
#define WLC_RATE_9M	18
#define WLC_RATE_12M	24
#define WLC_RATE_18M	36
#define WLC_RATE_24M	48
#define WLC_RATE_36M	72
#define WLC_RATE_48M	96
#define WLC_RATE_54M	108

/* Encoding 1 - HT MCS */
#define WL_RSPEC_HT_MCS_MASK		0x0000007F	/**< HT MCS value mask in rspec */

/* Encoding >= 2 & <= 4 */
#define WL_RSPEC_NSS_MCS_MASK		0x000000FF	/* NSS & MCS values mask in rspec */
#define WL_RSPEC_MCS_MASK		0x0000000F	/* mimo MCS value mask in rspec */
#define WL_RSPEC_NSS_MASK		0x000000F0	/* mimo NSS value mask in rspec */
#define WL_RSPEC_NSS_SHIFT		4		/* mimo NSS value shift in rspec */

/* Encoding 2 - VHT MCS + NSS */
#define WL_RSPEC_VHT_MCS_MASK		WL_RSPEC_MCS_MASK	/**< VHT MCS value mask in rspec */
#define WL_RSPEC_VHT_NSS_MASK		WL_RSPEC_NSS_MASK	/**< VHT Nss value mask in rspec */
#define WL_RSPEC_VHT_NSS_SHIFT		WL_RSPEC_NSS_SHIFT	/**< VHT Nss value shift in rspec */

/* Encoding 3 - HE MCS + NSS */
#define WL_RSPEC_HE_MCS_MASK		WL_RSPEC_MCS_MASK	/**< HE MCS value mask in rspec */
#define WL_RSPEC_HE_NSS_MASK		WL_RSPEC_NSS_MASK	/**< HE Nss value mask in rspec */
#define WL_RSPEC_HE_NSS_SHIFT		WL_RSPEC_NSS_SHIFT	/**< HE Nss value shift in rpsec */

/* Encoding 4 - EHT MCS + NSS */
#define WL_RSPEC_EHT_MCS_MASK		WL_RSPEC_MCS_MASK	/**< EHT MCS value mask in rspec */
#define WL_RSPEC_EHT_NSS_MASK		WL_RSPEC_NSS_MASK	/**< EHT Nss value mask in rspec */
#define WL_RSPEC_EHT_NSS_SHIFT		WL_RSPEC_NSS_SHIFT	/**< EHT Nss value shift in rpsec */

/* Encoding 5 - UHR MCS + NSS */
#define WL_RSPEC_UHR_MCS_HI_MASK	0x10			/**<UHR MCS BIT4 mask */
#define WL_RSPEC_UHR_MCS_HI_SHIFT	4u
#define WL_RSPEC_UHR_MCS_LO_MASK	0xF			/**<UHR MCS BIT3:0 mask in rspec */
#define WL_RSPEC_UHR_MCS_MASK		(WL_RSPEC_UHR_MCS_HI_MASK | \
					WL_RSPEC_UHR_MCS_LO_MASK)	/**< UHR MCS value mask */
#define WL_RSPEC_UHR_NSS_MASK		WL_RSPEC_NSS_MASK	/**< UHR Nss value mask in rspec */
#define WL_RSPEC_UHR_NSS_SHIFT		WL_RSPEC_NSS_SHIFT	/**< UHR Nss value shift in rpsec */

#define RSPEC_UHR_MCS_HI_GET(rspec)	(((rspec) & WL_RSPEC_UHR_MCSEXT_MASK) >> \
						(WL_RSPEC_UHR_MCSEXT_SHIFT - \
						WL_RSPEC_UHR_MCS_HI_SHIFT))
#define RSPEC_UHR_MCS_LO_GET(rspec)	((rspec) & WL_RSPEC_UHR_MCS_LO_MASK)

#define RSPEC_UHR_MCS_HI_SET(mcs)	(((mcs) & WL_RSPEC_UHR_MCS_HI_MASK) << \
						(WL_RSPEC_UHR_MCSEXT_SHIFT - \
						WL_RSPEC_UHR_MCS_HI_SHIFT))
#define RSPEC_UHR_MCS_LO_SET(mcs)	((mcs) & WL_RSPEC_UHR_MCS_LO_MASK)

#define RSPEC_GET_UHR_MCS(rspec)	wf_uhr_rspec_get_mcs(rspec)
#define RSPEC_SET_UHR_MCS(rspec, mcs)	((rspec) = wf_uhr_rspec_set_mcs(rspec, mcs))

/* Macro to get MCS/NSS for encoding >= 2 i.e VHT, HE, EHT and UHR */
#define RSPEC_GET_VHTEXT_MCS(rspec)	(RSPEC_ISUHR(rspec) ? RSPEC_GET_UHR_MCS(rspec) : \
						((rspec) & WL_RSPEC_MCS_MASK))
#define RSPEC_GET_VHTEXT_NSS(rspec)	(((rspec) & WL_RSPEC_NSS_MASK) >> WL_RSPEC_NSS_SHIFT)

/* ======== RSPEC_BW field ======== */

#define WL_RSPEC_BW_UNSPECIFIED	0x00000000u	/* 0 */
#define WL_RSPEC_BW_20MHZ	0x00010000u	/* 1 */
#define WL_RSPEC_BW_40MHZ	0x00020000u	/* 2 */
#define WL_RSPEC_BW_80MHZ	0x00030000u	/* 3 */
#define WL_RSPEC_BW_160MHZ	0x00040000u	/* 4 */
// unused			0x00050000u	/* 5 */
#define WL_RSPEC_BW_320MHZ	0x00060000u	/* 6 */

/* ======== RSPEC_ENCODING field ======== */

/* NOTE: Assuming the rate field is always NSS+MCS starting from VHT encoding!
 *       Modify/fix RSPEC_ISNSSMCS() macro if above condition changes any time.
 */
#define WL_RSPEC_ENCODE_RATE	0x00000000u	/**< Legacy rate is stored in RSPEC_RATE */
#define WL_RSPEC_ENCODE_HT	0x01000000u	/**< HT MCS is stored in RSPEC_RATE */
#define WL_RSPEC_ENCODE_VHT	0x02000000u	/**< VHT MCS and NSS are stored in RSPEC_RATE */
#define WL_RSPEC_ENCODE_HE	0x03000000u	/**< HE MCS and NSS are stored in RSPEC_RATE */
#define WL_RSPEC_ENCODE_EHT	0x04000000u	/**< EHT MCS and NSS are stored in RSPEC_RATE */
#define WL_RSPEC_ENCODE_UHR	0x05000000u	/**< UHR MCS and NSS are stored in RSPEC_RATE */

/**
 * ===============================
 * Handy macros to parse rate spec
 * ===============================
 */
#define RSPEC_BW(rspec)		((rspec) & WL_RSPEC_BW_MASK)
#define RSPEC_IS20MHZ(rspec)	(RSPEC_BW(rspec) == WL_RSPEC_BW_20MHZ)
#define RSPEC_IS40MHZ(rspec)	(RSPEC_BW(rspec) == WL_RSPEC_BW_40MHZ)
#define RSPEC_IS80MHZ(rspec)	(RSPEC_BW(rspec) == WL_RSPEC_BW_80MHZ)
#define RSPEC_IS160MHZ(rspec)	(RSPEC_BW(rspec) == WL_RSPEC_BW_160MHZ)
#ifdef BCMWIFI_BW320MHZ
#define RSPEC_IS320MHZ(rspec)	(RSPEC_BW(rspec) == WL_RSPEC_BW_320MHZ)
#else
#define RSPEC_IS320MHZ(rspec)	(FALSE)
#endif /* BCMWIFI_BW320MHZ */

#define WL_RSPEC_BW(rspec)	(RSPEC_BW(rspec) >> WL_RSPEC_BW_SHIFT)
#define RSPEC_BW_REPL(rspec, rspec_bw)	\
	(((rspec) & ~WL_RSPEC_BW_MASK) | (rspec_bw))

#define RSPEC_BW_GE(rspec, rspec_bw) (RSPEC_BW(rspec) >= rspec_bw)
#define RSPEC_BW_LE(rspec, rspec_bw) (RSPEC_BW(rspec) <= rspec_bw)
#define RSPEC_BW_GT(rspec, rspec_bw) (!RSPEC_BW_LE(rspec, rspec_bw))
#define RSPEC_BW_LT(rspec, rspec_bw) (!RSPEC_BW_GE(rspec, rspec_bw))

#define RSPEC_ISSGI(rspec)	(((rspec) & WL_RSPEC_SGI) != 0)
#define RSPEC_ISLDPC(rspec)	(((rspec) & WL_RSPEC_LDPC) != 0)
#define RSPEC_ISSTBC(rspec)	(((rspec) & WL_RSPEC_STBC) != 0)
#define RSPEC_ISTXBF(rspec)	(((rspec) & WL_RSPEC_TXBF) != 0)

#define RSPEC_TXEXP(rspec)	(((rspec) & WL_RSPEC_TXEXP_MASK) >> WL_RSPEC_TXEXP_SHIFT)

/* deprecated! */
#define RSPEC_ENCODE(rspec)	(((rspec) & WL_RSPEC_ENCODING_MASK) >> WL_RSPEC_ENCODING_SHIFT)

#define WL_RSPEC_ENCODE(rspec)	(((rspec) & WL_RSPEC_ENCODING_MASK) >> WL_RSPEC_ENCODING_SHIFT)

#define RSPEC_ISLEGACY(rspec)	(((rspec) & WL_RSPEC_ENCODING_MASK) == WL_RSPEC_ENCODE_RATE)

#define	RSPEC_ISCCK(rspec)	(RSPEC_ISLEGACY(rspec) && \
				 (int8)rate_info[(rspec) & WL_RSPEC_LEGACY_RATE_MASK] > 0)
#define	RSPEC_ISOFDM(rspec)	(RSPEC_ISLEGACY(rspec) && \
				 (int8)rate_info[(rspec) & WL_RSPEC_LEGACY_RATE_MASK] < 0)

#define RSPEC_ISHT(rspec)	(((rspec) & WL_RSPEC_ENCODING_MASK) == WL_RSPEC_ENCODE_HT)
#define RSPEC_ISVHT(rspec)	(((rspec) & WL_RSPEC_ENCODING_MASK) == WL_RSPEC_ENCODE_VHT)
#ifdef WL11AX
#define RSPEC_ISHE(rspec)	(((rspec) & WL_RSPEC_ENCODING_MASK) == WL_RSPEC_ENCODE_HE)
#else /* WL11AX */
#define RSPEC_ISHE(rspec)	0
#endif /* WL11AX */
#if defined(WL11BE) || defined(WLC_SIGB_RX_11BE_RATE_DECODE)
#define RSPEC_ISEHT(rspec)	(((rspec) & WL_RSPEC_ENCODING_MASK) == WL_RSPEC_ENCODE_EHT)
#else /* WL11BE */
#define RSPEC_ISEHT(rspec)	0
#endif /* WL11BE */

#if defined(WL11BN)
#define RSPEC_ISUHR(rspec)	(((rspec) & WL_RSPEC_ENCODING_MASK) == WL_RSPEC_ENCODE_UHR)
#else /* WL11BN */
#define RSPEC_ISUHR(rspec)	0
#endif /* WL11BN */

/* fast check if rate field is NSS+MCS format (starting from VHT ratespec) */
#define RSPEC_ISVHTEXT(rspec)	(((rspec) & WL_RSPEC_ENCODING_MASK) >= WL_RSPEC_ENCODE_VHT)
/* fast check if rate field is NSS+MCS format (starting from HE ratespec) */
#define RSPEC_ISHEEXT(rspec)	(((rspec) & WL_RSPEC_ENCODING_MASK) >= WL_RSPEC_ENCODE_HE)
/* fast check if rate field is NSS+MCS format (starting from EHT ratespec) */
#define RSPEC_ISEHTEXT(rspec)	(((rspec) & WL_RSPEC_ENCODING_MASK) >= WL_RSPEC_ENCODE_EHT)
/**
 * ================================
 * Handy macros to create rate spec
 * ================================
 */
/* create ratespecs */
#define LEGACY_RSPEC(rate)	(WL_RSPEC_ENCODE_RATE | WL_RSPEC_BW_20MHZ | \
				 ((rate) & WL_RSPEC_LEGACY_RATE_MASK))
#define CCK_RSPEC(cck)		LEGACY_RSPEC(cck)
#define OFDM_RSPEC(ofdm)	LEGACY_RSPEC(ofdm)
#define HT_RSPEC(mcs)		(WL_RSPEC_ENCODE_HT | ((mcs) & WL_RSPEC_HT_MCS_MASK))
#define VHT_RSPEC(mcs, nss)	(WL_RSPEC_ENCODE_VHT | \
				 (((nss) << WL_RSPEC_VHT_NSS_SHIFT) & WL_RSPEC_VHT_NSS_MASK) | \
				 ((mcs) & WL_RSPEC_VHT_MCS_MASK))
#define HE_RSPEC(mcs, nss)	(WL_RSPEC_ENCODE_HE | \
				 (((nss) << WL_RSPEC_HE_NSS_SHIFT) & WL_RSPEC_HE_NSS_MASK) | \
				 ((mcs) & WL_RSPEC_HE_MCS_MASK))
#define EHT_RSPEC(mcs, nss)	(WL_RSPEC_ENCODE_EHT | \
				 (((nss) << WL_RSPEC_EHT_NSS_SHIFT) & WL_RSPEC_EHT_NSS_MASK) | \
				 ((mcs) & WL_RSPEC_EHT_MCS_MASK))
#define UHR_RSPEC(mcs, nss)	(WL_RSPEC_ENCODE_UHR | \
				 (((nss) << WL_RSPEC_UHR_NSS_SHIFT) & WL_RSPEC_UHR_NSS_MASK) | \
				 (RSPEC_UHR_MCS_HI_SET(mcs) | RSPEC_UHR_MCS_LO_SET(mcs)))
/* EHT Invalid rspec for non-last MPDU of an AMPDU */
#define EHT_INVRSPEC		EHT_RSPEC(0xf, 0xf)

#define LEGACY_RSPEC_DUP(rate, bw)	(WL_RSPEC_ENCODE_RATE | bw | \
					((rate) & WL_RSPEC_LEGACY_RATE_MASK))
/**
 * ==================
 * Other handy macros
 * ==================
 */
/* return rate in unit of Kbps */
#define RSPEC2KBPS(rspec)	wf_rspec_to_rate(rspec)

/* return rate in unit of 500Kbps */
/* works only for legacy rate */
#ifdef BCMDBG
#define RSPEC2RATE(rspec)	wf_rspec_to_rate_legacy(rspec)
#else
#define RSPEC2RATE(rspec)	((rspec) & WL_RSPEC_LEGACY_RATE_MASK)
#endif

/**
 * =================================
 * Macros to use the rate_info table
 * =================================
 */
/* phy_rate table index is in [500kbps] units */
#define WLC_MAXRATE	108	/**< in 500kbps units */
/* BSS membership selectors are 108 - 127;
 * so keeping the number as 20 to address max value
 */
#define WLC_MAX_BSS_MEMBERSHIP_SELECTOR_NUM 20u
extern const uint8 rate_info[];
/* phy_rate table value is encoded */
#define	RATE_INFO_OFDM_MASK	0x80	/* ofdm mask */
#define	RATE_INFO_RATE_MASK	0x7f	/* rate signal index mask */
#define	RATE_INFO_M_RATE_MASK	0x0f	/* M_RATE_TABLE index mask */
#define	RATE_INFO_RATE_ISCCK(r)	((r) <= WLC_MAXRATE && (int8)rate_info[r] > 0)
#define	RATE_INFO_RATE_ISOFDM(r) ((r) <= WLC_MAXRATE && (int8)rate_info[r] < 0)

/**
 * ===================
 * function prototypes
 * ===================
 */
ratespec_t wf_vht_plcp_to_rspec(const uint8 *plcp);
ratespec_t wf_he_plcp_to_rspec(const uint8 *plcp);
ratespec_t wf_ht_plcp_to_rspec(const uint8 *plcp);

#ifdef BCMDBG
uint wf_rspec_to_rate_legacy(ratespec_t rspec) BCMCONSTFN;
#endif
uint wf_rspec_to_rate(ratespec_t rspec) BCMCONSTFN;
uint wf_rspec_to_rate_rsel(ratespec_t rspec) BCMCONSTFN;

/* WL_CHANSPEC_BW_x to WL_RSPEC_BW_x mapping.
 * Indexed by WL_CHSPEC_BW(chspec) and return WL_RSPEC_BW_x.
 */
/* deprecated! */
ratespec_t wf_chspec2rspec_bw(uint8 chspec_bw) BCMCONSTFN;

uint32 wf_uhr_rspec_get_mcs(ratespec_t rspec);
ratespec_t wf_uhr_rspec_set_mcs(ratespec_t rspec, uint32 mcs);
#endif /* _bcmwifi_rspec_h_ */
