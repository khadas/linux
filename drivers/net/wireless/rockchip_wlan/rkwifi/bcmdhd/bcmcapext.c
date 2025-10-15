/*
 * wlu capext procesing. Shared between DHD and WLU tool.
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

#if defined(__linux__) && !defined(BCMDRIVER)
// for 'uint'
#define USE_TYPEDEF_DEFAULTS
#endif

#include <typedefs.h>
#include <bcmdefs.h>

#if defined(__linux__)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
#include <linux/stdarg.h>
#else
#include <stdarg.h>
#endif /* LINUX_VERSION_CODE */
#else
#include <stdarg.h>
#endif /* CONFIG_BCMDHD && __linux__ */

#ifdef BCMDRIVER
#include <osl.h>
#else /* !BCMDRIVER */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifndef ASSERT
#define ASSERT(exp)
#endif
#endif /* !BCMDRIVER */

#include <bcmutils.h>
#include <bcmendian.h>
#include <bcmtlv.h>
#include <wlioctl.h>
#include <bcmstdlib_s.h>

#include <bcmcapext.h>

#define CAPEXT_SUBFEATURE_MAP(bitpos, str) { bitpos, str }
#ifdef BCMDONGLEHOST
#define CAPEXT_CAPS_NUM_MAX			(224u)
#else
#define CAPEXT_CAPS_NUM_MAX			(1024u)
#endif

typedef struct capext_output_buffer_ctx_s {
	const char *cap_strs[CAPEXT_CAPS_NUM_MAX];
	uint16 num_items_populated;
	uint16 type;
	uint16 num_strings;
	uint16 last_cap_bit_pos;
	uint8  more_caps;
} capext_output_buffer_ctx_t;

typedef struct capext_bitpos_to_string_map {
	uint16 bitpos;
	const char *str;
} capext_bitpos_to_string_map_t;

typedef struct fw_feature_xtlv_parser_map {
	uint16 type;
	const capext_bitpos_to_string_map_t *bitpos_to_str_map;
	uint16 num_strings;
} capext_fw_feature_xtlv_parser_map_t;

typedef struct capext_partition_init_info_s {
	uint16 base;
	uint16 max_feature_id;
	const capext_fw_feature_xtlv_parser_map_t *parser;
} capext_partition_init_info_t;

static int capext_get_caps(void *ctx, const capext_fw_feature_xtlv_parser_map_t *map,
	const uint8 *buf, uint16 len);

static int capext_parse_xtlvs(void *ctx, const uint8 *buf, uint16 type, uint16 len);
/* Generic XTLV parsing function */
static int capext_generic_xtlv_cbfn(void *ctx, const uint8 *buf, uint16 type, uint16 len);

#ifndef BCMDONGLEHOST
static int capext_str_comparator(const void *p1, const void *p2);
#endif
static int capext_print_caps(capext_output_buffer_ctx_t *ctx, char *outbuf, uint16 outbuflen);
static const capext_fw_feature_xtlv_parser_map_t *capext_get_xtlv_parser_map(uint16 type);

/* Bus features partition */
/* Bus features bit positions to string mapping.
 * Insert new entries in the array below in sorted order of output string to be printed
 */
static const capext_bitpos_to_string_map_t capext_bus_features_subfeature_map[] = {
	CAPEXT_SUBFEATURE_MAP(CAPEXT_BUS_FEATURE_BITPOS_HP2P, "hp2p"),
	CAPEXT_SUBFEATURE_MAP(CAPEXT_BUS_FEATURE_BITPOS_PTM, "ptm"),
	CAPEXT_SUBFEATURE_MAP(CAPEXT_BUS_FEATURE_BITPOS_PKTLAT, "pktlatency"),
	CAPEXT_SUBFEATURE_MAP(CAPEXT_BUS_FEATURE_BITPOS_BUSTPUT, "bustput"),
	CAPEXT_SUBFEATURE_MAP(CAPEXT_BUS_FEATURE_BITPOS_MAX, NULL)
};

/* Packet latency subfeatures bit positions to string mapping
 * Insert new entries in the array below in sorted order of output string to be printed
 */
static const capext_bitpos_to_string_map_t capext_pktlat_subfeature_map[] = {
	CAPEXT_SUBFEATURE_MAP(CAPEXT_PKTLAT_BITPOS_IPC, "pktlat_ipc"),
	CAPEXT_SUBFEATURE_MAP(CAPEXT_PKTLAT_BITPOS_META, "pktlat_meta"),
	CAPEXT_SUBFEATURE_MAP(CAPEXT_PKTLAT_BITPOS_MAX, NULL)
};

/* RTE features partition */
/* RTE features bit positions to string mapping.
 * Insert new entries in the array below in sorted order of output string to be printed
 */
static const capext_bitpos_to_string_map_t capext_rte_features_subfeature_map[] = {
	CAPEXT_SUBFEATURE_MAP(CAPEXT_RTE_FEATURE_BITPOS_CST, "cst"),
	CAPEXT_SUBFEATURE_MAP(CAPEXT_RTE_FEATURE_BITPOS_ECOUNTERS, "ecounters"),
	CAPEXT_SUBFEATURE_MAP(CAPEXT_RTE_FEATURE_BITPOS_ETD, "etd_info"),
	CAPEXT_SUBFEATURE_MAP(CAPEXT_RTE_FEATURE_BITPOS_EVENT_LOG, "event_log"),
	CAPEXT_SUBFEATURE_MAP(CAPEXT_RTE_FEATURE_BITPOS_H2D_LOG_TIME_SYNC, "h2dlogts"),
	CAPEXT_SUBFEATURE_MAP(CAPEXT_RTE_FEATURE_BITPOS_HCHK, "hchk"),
	CAPEXT_SUBFEATURE_MAP(CAPEXT_RTE_FEATURE_BITPOS_HWRNG, "hwrng"),
	CAPEXT_SUBFEATURE_MAP(CAPEXT_RTE_FEATURE_BITPOS_LOGTRACE, "logtrace"),
	CAPEXT_SUBFEATURE_MAP(CAPEXT_RTE_FEATURE_BITPOS_SMD, "smd"),
	CAPEXT_SUBFEATURE_MAP(CAPEXT_RTE_FEATURE_BITPOS_SPMI, "spmi"),
	CAPEXT_SUBFEATURE_MAP(CAPEXT_RTE_FEATURE_BITPOS_MAX, NULL)
};

/* ecounters subfeatures bit positions to string mapping
 * Insert new entries in the array below in sorted order of output string to be printed
 */
static const capext_bitpos_to_string_map_t capext_ecounters_subfeature_map[] = {
	CAPEXT_SUBFEATURE_MAP(CAPEXT_ECOUNTERS_BITPOS_ADV, "adv_ecounters"),
	CAPEXT_SUBFEATURE_MAP(CAPEXT_ECOUNTERS_BITPOS_CHSTATS, "chstats"),
	CAPEXT_SUBFEATURE_MAP(CAPEXT_ECOUNTERS_BITPOS_PHY_CAL, "phy_cal_ecounter"),
	CAPEXT_SUBFEATURE_MAP(CAPEXT_ECOUNTERS_BITPOS_PHY, "phy_ecounter"),
	CAPEXT_SUBFEATURE_MAP(CAPEXT_ECOUNTERS_BITPOS_PEERSTATS, "peerstats"),
	CAPEXT_SUBFEATURE_MAP(CAPEXT_ECOUNTERS_BITPOS_TXHIST, "txhist"),
	CAPEXT_SUBFEATURE_MAP(CAPEXT_ECOUNTERS_BITPOS_DTIM_MISS, "dtim_miss"),
	CAPEXT_SUBFEATURE_MAP(CAPEXT_ECOUNTERS_BITPOS_SOFTAPSTATS, "softapstats"),
	CAPEXT_SUBFEATURE_MAP(CAPEXT_ECOUNTERS_BITPOS_MAX, NULL)
};

/* WL features partition */
/* AMPDU subfeatures bit positions to string mapping
 * Insert new entries in the array below in sorted order of output string to be printed
 */
static const capext_bitpos_to_string_map_t capext_ampdu_subfeature_map[] = {
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_AMPDU_BITPOS_RX, "ampdu_rx"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_AMPDU_BITPOS_TX, "ampdu_tx"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_AMPDU_BITPOS_MAX, NULL)
};

/* AMSDU subfeatures bit positions to string mapping
 * Insert new entries in the array below in sorted order of output string to be printed
 */
static const capext_bitpos_to_string_map_t capext_amsdu_subfeature_map[] = {
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_AMSDU_BITPOS_DYNLEN, "amsdudynlen"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_AMSDU_BITPOS_RX, "amsdurx"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_AMSDU_BITPOS_TX, "amsdutx"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_AMSDU_BITPOS_MAX, NULL)
};

/* AP subfeatures bit positions to string mapping
 * Insert new entries in the array below in sorted order of output string to be printed
 */
static const capext_bitpos_to_string_map_t capext_ap_subfeature_map[] = {
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_AP_BITPOS_AX_5G_ONLY, "5g_he_ap"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_AP_BITPOS_NONAX, "non_he_ap"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_AP_BITPOS_SAE, "sae-ap"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_AP_BITPOS_BCNPROT_AP, "bcnprot-ap"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_AP_BITPOS_MAX, NULL)
};

/* COEX subfeatures bit positions to string mapping
 * Insert new entries in the array below in sorted order of output string to be printed
 */
static const capext_bitpos_to_string_map_t capext_coex_subfeature_map[] = {
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_COEX_BITPOS_BTC_WIFI_PROT, "btc_wifi_prot"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_COEX_BITPOS_LTE, "ltecx"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_COEX_BITPOS_LTECX_LBT, "ltecxlbt"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_COEX_BITPOS_RC1, "rc1cx"),
#if defined(WL_RC2COEX) || defined(RC2CX)
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_COEX_BITPOS_RC2, "rc2cx"),
#endif /* WL_RC2COEX */
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_COEX_BITPOS_SIB, "sib"),
#ifdef LR154CX
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_COEX_BITPOS_154, "lr154cx"),
#endif /* LR154CX */
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_COEX_BITPOS_BT2G, "bt2gcx"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_COEX_BITPOS_BT5G, "bt5gcx"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_COEX_BITPOS_MAX, NULL)
};

/* EHT subfeatures bit positions to string mapping
 * Insert new entries in the array below in sorted order of output string to be printed
 */
static const capext_bitpos_to_string_map_t capext_eht_subfeature_map[] = {
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_EHT_BITPOS_320MHZ, "320"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_EHT_BITPOS_MLO, "mlo"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_EHT_BITPOS_MAX, NULL)
};

/* FBT subfeatures bit positions to string mapping
 * Insert new entries in the array below in sorted order of output string to be printed
 */
static const capext_bitpos_to_string_map_t capext_fbt_subfeature_map[] = {
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FBT_BITPOS_ADPT, "fbt_adpt"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FBT_BITPOS_OVERDS, "fbtoverds"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FBT_BITPOS_MAX, NULL)
};

/* FW features bit positions to string mapping.
 * Insert new entries in the array below in sorted order of output string to be printed
 */
static const capext_bitpos_to_string_map_t capext_wl_features_subfeature_map[] = {
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_11AZ, "11az"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_160MHZ_SUPPORT, "160"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_6G, "6g"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_802_11d, "802.11d"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_802_11h, "802.11h"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_AMPDU, "ampdu"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_AMSDU, "amsdu"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_ANQPO, "anqpo"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_ANTGAIN6G, "antgain6g"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_AP, "ap"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_APF, "apf"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_ARB, "arb"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_ARPOE, "arpoe"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_AVOID_BSSID, "avoid-bssid"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_BCMDCS, "bcmdcs"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_BCNPROT, "bcnprot"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_BCNTRIM, "bcntrim"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_BDO, "bdo"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_BGDFS, "bgdfs"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_BKOFF_EVT, "bkoff_evt"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_BSSTRANS, "bsstrans"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_CAC, "cac"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_CDEF, "cdef"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_CLM_RESTRICT, "clm_restrict"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_COEX, "coex"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_CQA, "cqa"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_CSI, "csi"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_CSO, "cso"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_D11STATUS, "d11status"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_D3CBUF, "d3cbuf"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_DFRTS, "dfrts"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_DSA, "dsa"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_DTPC, "dtpc"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_DUALBAND, "dualband"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_DYN_BW, "dynbw"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_EHT, "eht"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_ESTM, "estm"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_EVT_EXT, "evt_ext"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_EXTSAE, "extsae"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_FBT, "fbt"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_FIE, "fie"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_EPNO, "epno"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_GCMP, "gcmp"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_HE, "he"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_HOST_SFHLLC, "host_sfhllc"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_ICMP, "icmp"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_IDAUTH, "idauth"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_IDSUP, "idsup"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_IFST, "ifst"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_IFVER, "ifver"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_IGMPOE, "igmpoe"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_IOT_BD, "iot_bd"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_IOT_BM, "iot_bm"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_LPAS, "lpas"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_LPC, "lpc"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_LPR_SCAN, "lpr_scan"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_MBO, "mbo"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_MBO_MIN, "mbomin"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_MBSS, "mbss"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_MFP, "mfp"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_MCHAN, "mchan"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_MIMO_PS, "mimo_ps"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_MONITOR, "monitor"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_MONITOR_MULTI, "monitor_multi"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_MP2P, "mp2p"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_MU_BEAMFORMEE, "multi-user-beamformee"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_MU_BEAMFORMER, "multi-user-beanformer"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_NAN, "nan"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_NAP, "nap"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_NATOE, "natoe"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_NDOE, "ndoe"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_OBSS_HW, "obss_hw"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_OCL, "ocl"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_OCT, "oct"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_OPS, "ops"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_OWE, "owe"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_P2P, "p2p"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_P2P0, "p2po"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_PASN, "pasn"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_PFNX, "pfnx"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_PKT_FILTER, "pkt_filter"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_PLATCFG, "platcfg"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_PMR, "pmr"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_PPR, "ppr"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_PRBRESP_MAC_FLTR,
	"probresp_mac_filter"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_PROP_TXSTATUS, "proptxstatus"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_PROXD, "proxd"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_PRUNE_WPA, "prune_wpa"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_PS_PRETEND, "pspretend"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_PSBW, "psbw"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_QOS_MGMT, "qos_mgmt"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_RM, "rm"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_RSDB, "rsdb"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_SLIMEMLSR, "slimemlsr"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_SCF, "scf"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_TDLS, "tdls"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_TDMTX, "tdmtx"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_TKO, "tko"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_TOE, "toe"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_TSYNC, "tsync"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_TVPM, "tvpm"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_TX_PROF, "tx-prof"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_TXPWRCAP, "txcap"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_TXPWRCACHE, "txpwrcache"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_TXSHAPER, "txshaper"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_RADIO_PWRSAVE, "radio_pwrsave"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_RCO, "rco"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_ROAMSTATS, "roamstats"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_RSSI_MON, "rssi_mon"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_RXCHAIN_PWRSAVE, "rxchain_pwrsave"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_SAE, "sae"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_SAE_H2E, "sae_h2e"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_SAE_PK, "sae_pk"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_SC, "sc"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_SCANCACHE, "scancache"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_SCANMAC, "scanmac"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_SCCA, "scca"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_SCR, "scr"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_SU_BEAMFORMEE,
	"single-user-beamformee"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_SU_BEAMFORMER,
	"single-user-beamformer"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_STA, "sta"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_STBC, "stbc"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_UCM, "ucm"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_VE, "ve"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_VHT_PROP_RATES, "vht-prop-rates"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_WME, "wme"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_WMF, "wmf"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_WNM, "wnm"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_OCV, "ocv"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_OCV_AP, "ocv_ap"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_SAE_EXT, "sae_ext"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_SC_6G_HE, "sc_6g_he"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_SPMI_SCAN_FWD, "spmi_scan_fwd"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_OWE_AP, "owe_ap"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_MPF_SCAN, "mpf_scan"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_MRSNO, "mrsno"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_AOP_SCAN, "aop_scan"),

	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_FEATURE_BITPOS_MAX, NULL)
};

/* MBSS subfeatures bit positions to string mapping
 * Insert new entries in the array below in sorted order of output string to be printed
 */
static const capext_bitpos_to_string_map_t capext_mbss_subfeature_map[] = {
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_MBSS_BITPOS_UCODE_BSS_0, "mbss-0"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_MBSS_BITPOS_UCODE_BSS_1, "mbss-1"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_MBSS_BITPOS_UCODE_BSS_2, "mbss-2"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_MBSS_BITPOS_MAX, NULL)
};

/* NAN subfeatures bit positions to string mapping
 * Insert new entries in the array below in sorted order of output string to be printed
 */
static const capext_bitpos_to_string_map_t capext_nan_subfeature_map[] = {
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_NAN_BITPOS_AUTODAM, "autodam"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_NAN_BITPOS_MESH, "meshnan"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_NAN_BITPOS_P2P, "nanp2p"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_NAN_BITPOS_RANGE, "nanrange"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_NAN_BITPOS_MAX, NULL)
};

/* Packet filter subfeatures bit positions to string mapping
 * Insert new entries in the array below in sorted order of output string to be printed
 */
static const capext_bitpos_to_string_map_t capext_pkt_filter_subfeature_map[] = {
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_PKT_FILTER_BITPOS_PKT_FILTER6, "pf6"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_PKT_FILTER_BITPOS_PKT_FILTER2, "pktfltr2"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_PKT_FILTER_BITPOS_MAX, NULL)
};

/* PPR subfeatures bit positions to string mapping
 * Insert new entries in the array below in sorted order of output string to be printed
 */
static const capext_bitpos_to_string_map_t capext_ppr_subfeature_map[] = {
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_PPR_BITPOS_TLV_VER_1, "cptlv-1"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_PPR_BITPOS_TLV_VER_2, "cptlv-2"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_PPR_BITPOS_TLV_VER_3, "cptlv-3"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_PPR_BITPOS_TLV_VER_4, "cptlv-4"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_PPR_BITPOS_MAX, NULL)
};

/* STBC subfeatures bit positions to string mapping
 * Insert new entries in the array below in sorted order of output string to be printed
 */
static const capext_bitpos_to_string_map_t capext_stbc_subfeature_map[] = {
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_STBC_BITPOS_RX_1SS, "stbc-rx-1ss"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_STBC_BITPOS_TX, "stbc-tx"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_STBC_BITPOS_MAX, NULL)
};

/* TX power cap subfeatures bit positions to string mapping
 * Insert new entries in the array below in sorted order of output string to be printed
 */
static const capext_bitpos_to_string_map_t capext_txpwrcap_subfeature_map[] = {
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_TXPWRCAP_BITPOS_TXPWRCAP_1, "txcap1"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_TXPWRCAP_BITPOS_TXPWRCAP_2, "txcap2"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_TXPWRCAP_BITPOS_TXPWRCAP_3, "txcap3"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_TXPWRCAP_BITPOS_TXPWRCAP_4, "txcap4"),
	CAPEXT_SUBFEATURE_MAP(WLC_CAPEXT_TXPWRCAP_BITPOS_MAX, NULL)
};

/* Bus partition: Info about parsing BUS XTLV caps */
static const capext_fw_feature_xtlv_parser_map_t capext_bus_feature_xtlv_parser_map[] = {
	{ CAPEXT_BUS_FEATURE_RSVD, NULL, 0},
	{ CAPEXT_BUS_FEATURE_BUS_FEATURES, capext_bus_features_subfeature_map,
	CAPEXT_BUS_FEATURE_BITPOS_MAX},
	{ CAPEXT_BUS_FEATURE_PKTLAT, capext_pktlat_subfeature_map, CAPEXT_PKTLAT_BITPOS_MAX},
	{ CAPEXT_BUS_FEATURE_MAX, NULL, 0}
};

/* Rte partition: Info about parsing RTE XTLV caps */
static const capext_fw_feature_xtlv_parser_map_t capext_rte_feature_xtlv_parser_map[] = {
	{ CAPEXT_RTE_FEATURE_RSVD, NULL, 0},
	{ CAPEXT_RTE_FEATURE_RTE_FEATURES, capext_rte_features_subfeature_map,
	CAPEXT_RTE_FEATURE_BITPOS_MAX},
	{ CAPEXT_RTE_FEATURE_ECOUNTERS, capext_ecounters_subfeature_map,
	CAPEXT_ECOUNTERS_BITPOS_MAX},
	{ CAPEXT_RTE_FEATURE_MAX, NULL, 0}
};

/* WL partition: Info about parsing WL XTLV caps */
static const capext_fw_feature_xtlv_parser_map_t capext_wl_feature_xtlv_parser_map[] = {
	{ CAPEXT_WL_FEATURE_RSVD, NULL, 0},
	{ CAPEXT_WL_FEATURE_WL_FEATURES, capext_wl_features_subfeature_map,
	WLC_CAPEXT_FEATURE_BITPOS_MAX},

	{ CAPEXT_WL_FEATURE_AMPDU, capext_ampdu_subfeature_map, WLC_CAPEXT_AMPDU_BITPOS_MAX},
	{ CAPEXT_WL_FEATURE_AMSDU, capext_amsdu_subfeature_map, WLC_CAPEXT_AMSDU_BITPOS_MAX},
	{ CAPEXT_WL_FEATURE_STBC, capext_stbc_subfeature_map, WLC_CAPEXT_STBC_BITPOS_MAX},
	{ CAPEXT_WL_FEATURE_NAN, capext_nan_subfeature_map, WLC_CAPEXT_NAN_BITPOS_MAX},
	{ CAPEXT_WL_FEATURE_COEX, capext_coex_subfeature_map, WLC_CAPEXT_COEX_BITPOS_MAX},
	{ CAPEXT_WL_FEATURE_FBT, capext_fbt_subfeature_map, WLC_CAPEXT_FBT_BITPOS_MAX},
	{ CAPEXT_WL_FEATURE_MBSS, capext_mbss_subfeature_map, WLC_CAPEXT_MBSS_BITPOS_MAX},
	{ CAPEXT_WL_FEATURE_TXPWRCAP, capext_txpwrcap_subfeature_map,
	WLC_CAPEXT_TXPWRCAP_BITPOS_MAX},

	{ CAPEXT_WL_FEATURE_PPR, capext_ppr_subfeature_map, WLC_CAPEXT_PPR_BITPOS_MAX},
	{ CAPEXT_WL_FEATURE_PKT_FILTER, capext_pkt_filter_subfeature_map,
	WLC_CAPEXT_PKT_FILTER_BITPOS_MAX},

	{ CAPEXT_WL_FEATURE_EHT, capext_eht_subfeature_map, WLC_CAPEXT_EHT_BITPOS_MAX},
	{ CAPEXT_WL_FEATURE_AP, capext_ap_subfeature_map, WLC_CAPEXT_AP_BITPOS_MAX},
	{ CAPEXT_WL_FEATURE_MAX, NULL, 0}
};

/* Partition info for parsing various XTLV types */
static const capext_partition_init_info_t cat_init_info[CAPEXT_FEATURE_ID_NUM_PARTITIONS] = {
	{ CAPEXT_BUS_FEATURE_ID_BASE, CAPEXT_BUS_FEATURE_MAX, capext_bus_feature_xtlv_parser_map },
	{ CAPEXT_RTE_FEATURE_ID_BASE, CAPEXT_RTE_FEATURE_MAX, capext_rte_feature_xtlv_parser_map},
	{ CAPEXT_WL_FEATURE_ID_BASE, CAPEXT_WL_FEATURE_MAX, capext_wl_feature_xtlv_parser_map},
};

#ifndef BCMDONGLEHOST
/* Sort comparator routine */
static int
capext_str_comparator(const void *p1, const void *p2)
{
	/* From man qsort: The actual arguments to this function are "pointers to
	 * pointers to char", but strcmp arguments are "pointers
	 * to char", hence the following cast plus dereference
	 */
	return strcmp(* (const char **) p1, * (const char **) p2);
}
#endif /* BCMDONGLEHOST */

/* Invoke a chain of function calls to parse output of dngl:capext iovar */
int
bcmcapext_parse_output(void *bufptr,  uint16 maxlen, char *outbuf, uint16 outbuflen)
{
	capext_info_t *capext;
	capext_output_buffer_ctx_t ctx;
	uint16 payload_len;
	int rc;

	capext = (capext_info_t *)bufptr;

	if (bufptr == NULL || outbuf == NULL || maxlen > WLC_IOCTL_MAXLEN ||
		outbuflen == 0 || outbuflen > WLC_IOCTL_MAXLEN) {
		return BCME_BADARG;
	}

	if (ltoh16(capext->version) != CAPEXT_INFO_VERSION_1) {
		return BCME_VERSION;
	}

	payload_len = ltoh16(capext->datalen);

	if (payload_len > maxlen) {
		return BCME_BADLEN;
	}

	ctx.num_items_populated = 0;
	ctx.more_caps = FALSE;

	rc = bcm_unpack_xtlv_buf(&ctx, capext->data, payload_len,
		BCM_XTLV_OPTION_ALIGN32, capext_parse_xtlvs);

	if (rc != BCME_OK) {
		return rc;
	}
#ifndef BCMDONGLEHOST
	qsort(ctx.cap_strs, ctx.num_items_populated, sizeof(char *), capext_str_comparator);
#endif
	return capext_print_caps(&ctx, outbuf, outbuflen);
}

/* Parse value payload on an XTLV */
static int
capext_parse_xtlvs(void *ctx, const uint8 *buf, uint16 type, uint16 len)
{
	capext_output_buffer_ctx_t *out_ctx;

	out_ctx = (capext_output_buffer_ctx_t *) ctx;

	if (out_ctx == NULL) {
		return BCME_BADARG;
	}

	if (len == 0) {
		return BCME_OK; /* Nothing to do */
	}

	return capext_generic_xtlv_cbfn(ctx, buf, type, len);
}

/* Find position of the last set bit in this XTLV payload */
static int
capext_last_cap_bitpos(const uint8 *buf, uint16 len)
{
	uint8 bitmap = 0;
	uint16 index;
	int position = -1;

	/* Necesary length checks are already performed prior to calling this function.  */
	index = len - 1; /* Point to the last byte of the payload mentioned by XTLV len field */

	/* Get index of the first non-zero byte from the end */
	while (buf[index] == 0) {
		if (index == 0) {
			return BCME_NOTFOUND;
		}
		index--;
	}

	/* buf[index] is now a non-zero value */
	bitmap = buf[index];

	/* Find out position of the most significant bit set */
	while (bitmap) {
		position++;
		bitmap >>= 1;
	}

	/* calculate position of the last cap bit set */
	return (index * NBBY) + position;
}

/* Go through bitmaps present in input buffer and print names of features/sub-features
 * corresponding to bits that are set.
 */
static int
capext_get_caps(void *ctx, const capext_fw_feature_xtlv_parser_map_t *map,
	const uint8 *buf, uint16 len)
{
	uint16 bitindex = 0, strindex = 0;
	capext_output_buffer_ctx_t *out_ctx;
	const char *str;
	uint16 num_strings;
	int last_cap_bit_pos;

	out_ctx = (capext_output_buffer_ctx_t *) ctx;
	last_cap_bit_pos = capext_last_cap_bitpos(buf, len);

	if (last_cap_bit_pos == BCME_NOTFOUND) {
		num_strings = 0; /* There is nothing to parse */
	} else if (map->num_strings < ((uint16) last_cap_bit_pos + 1)) {
		/* Version compatibility check */
		/* Perhaps this is new FW and old WL tool. IF the last capability bit position
		 * populated is more than the number of mappings that are in the tool,
		 * the user needs to update the WL tool.
		 */
		num_strings = map->num_strings;
		if (out_ctx->more_caps == FALSE) {
			out_ctx->type = map->type;
			out_ctx->num_strings = map->num_strings;
			out_ctx->last_cap_bit_pos = last_cap_bit_pos;
		}
		out_ctx->more_caps = TRUE;
	} else {
		/* If last capability position is less than or equal the number of mappings
		 * in the tool, the tool has necessary mapping to continue parsing this XTLV.
		 * In that case, parse only upto last capability bit position populated
		 */
		num_strings = (uint16) last_cap_bit_pos + 1;
	}

	while (bitindex < num_strings) {
		if (isset(buf, bitindex)) {
			str = NULL;

			/* Get string corresponding to the set bit position
			 * The map is holding strings in sorted order. So search through the
			 * enire string table for that map.
			 */
			for (strindex = 0; strindex < map->num_strings; strindex++) {
				if (bitindex != map->bitpos_to_str_map[strindex].bitpos) {
					continue;
				}
				/* Note string at matching index */
				str = map->bitpos_to_str_map[strindex].str;
				break;
			}

			/* The bit is set in the XTLV payload but corresponding matching entry is
			 * not found in the bitpos_to_str_map table. This can also happen
			 * in case the feature does not need implementation in the host driver
			 * and the corresponding map entry is not compiled in into the host driver.
			 * Therefore do not exit, but continue to the next bitindex.
			 */
			if (strindex == map->num_strings) {
				bitindex++;
				continue;
			}

			/* No string is defined at matching entry in bitpos_to_str map */
			if (str == NULL) {
				bitindex++;
				continue;
			}

			if (out_ctx->num_items_populated >= CAPEXT_CAPS_NUM_MAX) {
				return BCME_BUFTOOSHORT;
			}

			/* Store pointer to string corresponding to the set bit position for later
			 * processing.
			 */
			out_ctx->cap_strs[out_ctx->num_items_populated] = str;
			out_ctx->num_items_populated++;
		}
		bitindex++;
	}

	return BCME_OK;
}

/* Get parsing info for an XTLV type */
static const capext_fw_feature_xtlv_parser_map_t *
capext_get_xtlv_parser_map(uint16 type)
{
	uint16 partition;
	const capext_partition_init_info_t *cat_init_entry;

	/* Partitions are contiguous and fixed size. O(1) lookup for partition */
	partition = type / CAPEXT_FEATURE_ID_PARTITION_SIZE;
	if (partition >= CAPEXT_FEATURE_ID_NUM_PARTITIONS) {
		return NULL;
	}

	cat_init_entry = &cat_init_info[partition];
	/* type in range? */
	if (type > cat_init_entry->base && type < cat_init_entry->max_feature_id) {
		type -= cat_init_entry->base; /* Offset from base to index in parser[] */
		return &cat_init_entry->parser[type];	/* Return appropriate parser */
	}

	return NULL;
}

/* Generic XTLV parsing function */
static int
capext_generic_xtlv_cbfn(void *ctx, const uint8 *buf, uint16 type, uint16 len)
{
	const capext_fw_feature_xtlv_parser_map_t *ptr;

	ptr = capext_get_xtlv_parser_map(type);

	if (ptr == NULL) {
		return BCME_BADARG;
	}

	if (ptr->bitpos_to_str_map == NULL || ptr->num_strings == 0) {
		return BCME_ERROR;
	}

	return capext_get_caps(ctx, ptr, buf, len);
}

/* Prints caps in a provided buffer. */
static int
capext_print_caps(capext_output_buffer_ctx_t *ctx, char *outbuf, uint16 outbuflen)
{
	uint16 i = 0;
	int rc = BCME_OK;
	uint16 clen = 0;
	uint16 cap_str_size;
	char more_cap_str[30u];

	/* Put a null terminator at the end of the buffer */
	outbuf[outbuflen - 1] = '\0';
	outbuflen--; /* available size */

	if (ctx->more_caps == TRUE) {
		if (ctx->num_items_populated >= CAPEXT_CAPS_NUM_MAX) {
			return BCME_BUFTOOSHORT;
		}

		/* Print type:starting_bit_pos-ending_bit_pos that could not be
		 * translated as FW is new and WL tool is old.
		 * The translation table is does not have enough entries to parse new bits
		 */
		(void)snprintf(more_cap_str, sizeof(more_cap_str), "unknown:%u:%u-%u",
			ctx->type, ctx->num_strings, ctx->last_cap_bit_pos);
		ctx->cap_strs[ctx->num_items_populated] = more_cap_str;
		ctx->num_items_populated++;
	}

	while (i < ctx->num_items_populated) {
		/* cap string size is actual string + space character */
		cap_str_size = strlen(ctx->cap_strs[i]) + 1;
		if (snprintf(outbuf, (outbuflen - clen), "%s ", ctx->cap_strs[i]) <
			(int)cap_str_size) {
			rc = BCME_BUFTOOSHORT;
			break;
		}
		clen += cap_str_size;
		outbuf += cap_str_size;
		i++;
	}
	return rc;
}
