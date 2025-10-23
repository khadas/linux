/*
 * WFA specific types and constants relating to 802.11
 * Also, see WFA QoS Management spec:
 * https://drive.google.com/file/d/1dj4D92kUhLKrImWkOJZ0__Meg9fZm9fL/view?usp=share_link
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

#ifndef _802_11wfa_h_
#define _802_11wfa_h_

#ifndef _TYPEDEFS_H_
#include <typedefs.h>
#endif

#ifndef _NET_ETHERNET_H_
#include <ethernet.h>
#endif

#include <802.11.h>

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>

/* WME Elements */
#define WME_OUI			"\x00\x50\xf2"	/* WME OUI */
#define WME_OUI_LEN		3
#define WME_OUI_TYPE		2	/* WME type */
#define WME_TYPE		2	/* WME type, deprecated */
#define WME_SUBTYPE_IE		0	/* Information Element */
#define WME_SUBTYPE_PARAM_IE	1	/* Parameter Element */
#define WME_SUBTYPE_TSPEC	2	/* Traffic Specification */
#define WME_VER			1	/* WME version */

/** WME Information Element (IE) */
BWL_PRE_PACKED_STRUCT struct wme_ie {
	uint8 oui[3];
	uint8 type;
	uint8 subtype;
	uint8 version;
	uint8 qosinfo;
} BWL_POST_PACKED_STRUCT;
typedef struct wme_ie wme_ie_t;
#define WME_IE_LEN 7	/* WME IE length */

/** WME Parameter Element (PE) */
BWL_PRE_PACKED_STRUCT struct wme_param_ie {
	uint8 oui[3];
	uint8 type;
	uint8 subtype;
	uint8 version;
	uint8 qosinfo;
	uint8 rsvd;
	edcf_acparam_t acparam[AC_COUNT];
} BWL_POST_PACKED_STRUCT;
typedef struct wme_param_ie wme_param_ie_t;
#define WME_PARAM_IE_LEN            24          /* WME Parameter IE length */

/* QoS Info field for IE as sent from AP */
#define WME_QI_AP_APSD_MASK         0x80        /* U-APSD Supported mask */
#define WME_QI_AP_APSD_SHIFT        7           /* U-APSD Supported shift */
#define WME_QI_AP_COUNT_MASK        0x0f        /* Parameter set count mask */
#define WME_QI_AP_COUNT_SHIFT       0           /* Parameter set count shift */

/* QoS Info field for IE as sent from STA */
#define WME_QI_STA_MAXSPLEN_MASK    0x60        /* Max Service Period Length mask */
#define WME_QI_STA_MAXSPLEN_SHIFT   5           /* Max Service Period Length shift */
#define WME_QI_STA_APSD_ALL_MASK    0xf         /* APSD all AC bits mask */
#define WME_QI_STA_APSD_ALL_SHIFT   0           /* APSD all AC bits shift */
#define WME_QI_STA_APSD_BE_MASK     0x8         /* APSD AC_BE mask */
#define WME_QI_STA_APSD_BE_SHIFT    3           /* APSD AC_BE shift */
#define WME_QI_STA_APSD_BK_MASK     0x4         /* APSD AC_BK mask */
#define WME_QI_STA_APSD_BK_SHIFT    2           /* APSD AC_BK shift */
#define WME_QI_STA_APSD_VI_MASK     0x2         /* APSD AC_VI mask */
#define WME_QI_STA_APSD_VI_SHIFT    1           /* APSD AC_VI shift */
#define WME_QI_STA_APSD_VO_MASK     0x1         /* APSD AC_VO mask */
#define WME_QI_STA_APSD_VO_SHIFT    0           /* APSD AC_VO shift */

/* Default BE ACI value for non-WME connection STA */
#define NON_EDCF_AC_BE_ACI_STA          0x02

/* WME Action Codes */
#define WME_ADDTS_REQUEST	0	/* WME ADDTS request */
#define WME_ADDTS_RESPONSE	1	/* WME ADDTS response */
#define WME_DELTS_REQUEST	2	/* WME DELTS request */

/* WME Setup Response Status Codes */
#define WME_ADMISSION_ACCEPTED		0	/* WME admission accepted */
#define WME_INVALID_PARAMETERS		1	/* WME invalide parameters */
#define WME_ADMISSION_REFUSED		3	/* WME admission refused */

/* ************* WPA definitions. ************* */
#define WPA_OUI			"\x00\x50\xF2"	/* WPA OUI */
#define WPA_OUI_LEN		3		/* WPA OUI length */
#define WPA_OUI_TYPE		1
#define WPA_VERSION		1		/* WPA version */
#define WPA_VERSION_LEN 2 /* WPA version length */

/* ************* WPA2 definitions. ************* */
#define WPA2_OUI		"\x00\x0F\xAC"	/* WPA2 OUI */
#define WPA2_OUI_LEN		3		/* WPA2 OUI length */
#define WPA2_VERSION		1		/* WPA2 version */
#define WPA2_VERSION_LEN	2		/* WAP2 version length */
#define MAX_RSNE_SUPPORTED_VERSION  WPA2_VERSION /* Max supported version */

/* ************* WPS definitions. ************* */
#define WPS_OUI			"\x00\x50\xF2"	/* WPS OUI */
#define WPS_OUI_LEN		3		/* WPS OUI length */
#define WPS_OUI_TYPE		4

/* ************* TPC definitions. ************* */
#define TPC_OUI			"\x00\x50\xF2"	/* TPC OUI */
#define TPC_OUI_LEN		3		/* TPC OUI length */
#define TPC_OUI_TYPE		8
#define WFA_OUI_TYPE_TPC	8		/* deprecated */

/* ************* WFA definitions. ************* */
#define WFA_OUI			"\x50\x6F\x9A"	/* WFA OUI */
#define WFA_OUI_LEN		3		/* WFA OUI length */
#define WFA_OUI_TYPE_P2P	9

#define P2P_OUI         WFA_OUI
#define P2P_OUI_LEN     WFA_OUI_LEN
#define P2P_OUI_TYPE    WFA_OUI_TYPE_P2P

#ifdef WLTDLS
#define WFA_OUI_TYPE_TPQ	4	/* WFD Tunneled Probe ReQuest */
#define WFA_OUI_TYPE_TPS	5	/* WFD Tunneled Probe ReSponse */
#define WFA_OUI_TYPE_WFD	10
#endif /* WTDLS */
#define WFA_OUI_TYPE_HS20		0x10
#define WFA_OUI_TYPE_OSEN		0x12
#define WFA_OUI_TYPE_NAN		0x13
#define WFA_OUI_TYPE_MBO		0x16
#define WFA_OUI_TYPE_MBO_OCE		0x16
#define WFA_OUI_TYPE_OWE		0x1C
#define WFA_OUI_TYPE_SAE_PK		0x1F
#define WFA_OUI_TYPE_TD_INDICATION	0x20

/* OUI_TYPE assignments for MRSNO */
#define WFA_OUI_TYPE_RSN_OVERRIDE		0x29
#define WFA_OUI_TYPE_RSN_OVERRIDE_2		0x2A
#define WFA_OUI_TYPE_RSNXE_OVERRIDE		0x2B
#define WFA_OUI_TYPE_RSN_SELECTION		0x2C
#define WFA_OUI_TYPE_RSN_OVERRIDE_MLO_LINK_KDE	0x2D

/* Snonce Cookie for MRSNO 4way-HS and FT reassociation */
#define EAPOL_WPA2_WFA_SNONCE_COOKIE		"\x50\x6F\x9A\x00\x00\x29"
#define EAPOL_WPA2_WFA_SNONCE_COOKIE_LEN	6u

/* WCN */
#define WCN_OUI			"\x00\x50\xf2"	/* WCN OUI */
#define WCN_TYPE		4	/* WCN type */

/* ************* WMM Parameter definitions. ************* */
#define WMM_OUI			"\x00\x50\xF2"	/* WNN OUI */
#define WMM_OUI_LEN		3		/* WMM OUI length */
#define WMM_OUI_TYPE		2		/* WMM OUT type */
#define WMM_VERSION		1
#define WMM_VERSION_LEN		1

/* WMM OUI subtype */
#define WMM_OUI_SUBTYPE_PARAMETER	1
#define WMM_PARAMETER_IE_LEN		24

#define SAE_PK_MOD_LEN		32u
BWL_PRE_PACKED_STRUCT struct dot11_sae_pk_element {
	uint8 id;			/* IE ID, 221, DOT11_MNG_PROPR_ID */
	uint8 len;			/* IE length */
	uint8 oui[WFA_OUI_LEN];		/* WFA_OUI */
	uint8 type;			/* SAE-PK */
	uint8 data[SAE_PK_MOD_LEN];	/* Modifier. 32Byte fixed */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_sae_pk_element dot11_sae_pk_element_t;

/* WPA3 Transition Mode bits */
#define TRANSITION_MODE_WPA3_PSK		BCM_BIT(0)
#define TRANSITION_MODE_SAE_PK			BCM_BIT(1)
#define TRANSITION_MODE_WPA3_ENTERPRISE		BCM_BIT(2)
#define TRANSITION_MODE_ENHANCED_OPEN		BCM_BIT(3)

#define TRANSITION_MODE_SUPPORTED_MASK (\
	TRANSITION_MODE_WPA3_PSK | \
	TRANSITION_MODE_SAE_PK | \
	TRANSITION_MODE_WPA3_ENTERPRISE | \
	TRANSITION_MODE_ENHANCED_OPEN)

/** Transition Disable Indication element */
BWL_PRE_PACKED_STRUCT struct dot11_tdi_element {
	uint8 id;	/* DOT11_MNG_VS_ID */
	uint8 len;	/* IE length */
	uint8 oui[3];	/* WFA_OUI */
	uint8 type;	/* WFA_OUI_TYPE_TD_INDICATION */
	uint8 tdi;	/* Transition Disable Indication bitmap */
} BWL_POST_PACKED_STRUCT;
typedef struct dot11_tdi_element dot11_tdi_element_t;

#define DOT11_TDI_ELEM_LENGTH	sizeof(dot11_tdi_element_t)

/* WiFi OWE transition OUI values */
#define OWE_TRANS_OUI       WFA_OUI         /* WiFi OUI 50:6F:9A */
/* oui_type field identifying the type and version of the OWE transition mode IE. */
#define OWE_OUI_TYPE        WFA_OUI_TYPE_OWE /* OUI Type/Version */
/* IEEE 802.11 vendor specific information element. */
#define OWE_IE_ID           0xdd

/*  2.3.1 OWE transition mode IE (WFA OWE spec 1.1) */
typedef BWL_PRE_PACKED_STRUCT struct wifi_owe_ie_s {
	uint8 id;               /* IE ID: OWE_IE_ID 0xDD */
	uint8 len;              /* IE length */
	uint8 oui[WFA_OUI_LEN]; /* OWE OUI 50:6F:9A */
	uint8 oui_type;         /* WFA_OUI_TYPE_OWE 0x1A */
	uint8 attr[];           /* var len attributes */
} wifi_owe_ie_t;

/* owe transition mode ie */
typedef BWL_PRE_PACKED_STRUCT struct owe_transition_mode_ie_s {
	uint8 bssid[ETHER_ADDR_LEN];  /* Contains the BSSID of the other virtual AP */
	uint8 ssid_len;
	uint8 *ssid;     /* SSID of the other virtual AP */
	uint8 band_info; /* operating band info of the other virtual AP */
	uint8 chan;      /* operating channel number of the other virtual AP */
} BWL_POST_PACKED_STRUCT owe_transition_mode_ie_t;
#define OWE_IE_HDR_SIZE (OFFSETOF(wifi_owe_ie_t, attr))
/* oui:3 bytes + oui type:1 byte */
#define OWE_IE_NO_ATTR_LEN  4

/** hotspot2.0 indication element (vendor specific) */
BWL_PRE_PACKED_STRUCT struct hs20_ie {
	uint8 oui[3];
	uint8 type;
	uint8 config;
} BWL_POST_PACKED_STRUCT;
typedef struct hs20_ie hs20_ie_t;
#define HS20_IE_LEN 5	/* HS20 IE length */

/* QoS Mgmt vendor specific IE OUI type */
#define QOS_MGMT_VSIE_OUI_TYPE		0x22u
#define QOS_MGMT_VSIE_MAX_ATTR_SIZE \
	(BCM_TLV_MAX_DATA_SIZE - (QOS_MGMT_IE_HDR_SIZE - TLV_HDR_LEN))

/* WFA Capabilities vendor specific IE OUI type */
#define WFA_CAP_VSIE_OUI_TYPE		0x23u

/* OUI Type - Identifying the type and version of the
 * QoS Management action frames.
 */
#define QOS_MGMT_AF_OUI_TYPE		0x1Au

/* DSCP Policy Action Frame OUI type */
#define DSCP_POLICY_AF_OUI_TYPE		QOS_MGMT_AF_OUI_TYPE

/* Wi-Fi Alliance Capabilities frame, OUI type */
#define WFA_CAP_AF_OUI_TYPE		0x1Bu

/* DSCP Policy Action Frame OUI subtypes */
#define DSCP_POLICY_QUERY_FRAME		0u
#define DSCP_POLICY_REQ_FRAME		1u
#define DSCP_POLICY_RESP_FRAME		2u

/* DSCP Policy request types */
#define POLICY_REQ_TYPE_ADD		0u
#define POLICY_REQ_TYPE_REMOVE		1u

/* DSCP Policy attribute field offsets */
#define DSCP_POLICY_ATTR_ID_OFF		0u
#define DSCP_POLICY_ATTR_LEN_OFF	1u
#define DSCP_POLICY_ATTR_DATA_OFF	2u

/* DSCP Policy attribute various length fields */
#define DSCP_POLICY_ATTR_ID_LEN		1u	/* attr id field length */
#define DSCP_POLICY_ATTR_LEN_LEN	1u	/* attr field length */
#define DSCP_POLICY_ATTR_HDR_LEN	2u	/* id + 1-byte length field */

/* QoS Management attributes as defined in the spec */
typedef enum qos_mgmt_attrs {

	/* DSCP Port Range attribute */
	DSCP_POLICY_PORT_RANGE_ATTR		= 1,

	/* DSCP Policy attribute */
	DSCP_POLICY_ATTR			= 2,

	/* DSCP Policy TCLAS attribute for classifier type 4 */
	DSCP_POLICY_TCLAS_ATTR			= 3,

	/* DSCP Domain Name attribute */
	DSCP_POLICY_DOMAIN_NAME_ATTR		= 4,

	/* 5G QoS Identifier (5QI) attribute */
	QI_5G_ATTR				= 5,

	/* 3GPP defined priority level for a 5G QoS flow carried over Wi-Fi access */
	PRIORITY_LEVEL_ATTR			= 6,

	/* DAR Request attribute */
	DAR_REQ_ATTR				= 7,

	/* DAR Response attribute */
	DAR_RESP_ATTR				= 8,

	/* DAR Report attribute */
	DAR_REPORT_ATTR				= 9,

	/* Latency statistics attribute */
	DAR_LATENCY_STATISTICS_ATTR		= 10,

	/* Radio Counters Statistics attribute */
	DAR_RADIO_COUNTERS_STATISTICS_ATTR	= 11,

	/* Control plane events attribute */
	DAR_CONTROL_PLANE_EVENTS_ATTR		= 12,

	DAR_FRAGMENT_ATTR			= 255

} qos_mgmt_attrs_t;

/* DSCP Policy Query frame header */
typedef BWL_PRE_PACKED_STRUCT struct dscp_policy_query_action_vs_frmhdr {
	uint8 category;		/* category VS/VSP */
	uint8 oui[WFA_OUI_LEN];	/* WFA OUI */
	uint8 oui_type;		/* DSCP_POLICY_AF_OUI_TYPE */
	uint8 oui_subtype;	/* DSCP Policy Query frame */
	uint8 dialog_token;	/* to match req/resp */
	uint8 data[];		/* zero or more QoS Mgmt elements */
} BWL_POST_PACKED_STRUCT dscp_policy_query_action_vs_frmhdr_t;
#define DSCP_POLICY_QUERY_ACTION_FRAME_HDR_SIZE (sizeof(dscp_policy_query_action_vs_frmhdr_t))

/* DSCP Policy request frame header */
typedef BWL_PRE_PACKED_STRUCT struct dscp_policy_req_action_vs_frmhdr {
	uint8 category;		/* category VS/VSP */
	uint8 oui[WFA_OUI_LEN];	/* WFA OUI */
	uint8 oui_type;		/* DSCP_POLICY_AF_OUI_TYPE */
	uint8 oui_subtype;	/* DSCP Policy Request frame */
	uint8 dialog_token;	/* to match req/resp */
	uint8 control;		/* request control */
	uint8 data[];		/* zero or more QoS Mgmt elements */
} BWL_POST_PACKED_STRUCT dscp_policy_req_action_vs_frmhdr_t;
#define DSCP_POLICY_REQ_ACTION_FRAME_HDR_SIZE (sizeof(dscp_policy_req_action_vs_frmhdr_t))

/* DSCP Policy Response frame header */
typedef BWL_PRE_PACKED_STRUCT struct dscp_policy_resp_action_vs_frmhdr {
	uint8 category;		/* category VS/VSP */
	uint8 oui[WFA_OUI_LEN];	/* WFA OUI */
	uint8 oui_type;		/* DSCP_POLICY_AF_OUI_TYPE */
	uint8 oui_subtype;	/* DSCP Policy Response frame type */
	uint8 dialog_token;	/* to validate req/resp frames */
	uint8 control;		/* response control */
	uint8 count;		/* Number of items in the data (Stauts List) */
	uint8 data[];		/* Status List */
} BWL_POST_PACKED_STRUCT dscp_policy_resp_action_vs_frmhdr_t;
#define DSCP_POLICY_RESP_ACTION_FRAME_HDR_SIZE (sizeof(dscp_policy_resp_action_vs_frmhdr_t))

/* WFA Capabilities IE */
typedef BWL_PRE_PACKED_STRUCT struct wfa_cap_ie {
	uint8 id;		/* 0xDD, IEEE 802.11 vendor specific information element */
	uint8 len;		/* length of data following */
	uint8 oui[WFA_OUI_LEN];	/* WFA OUI */
	uint8 oui_type;		/* WFA_CAP_VSIE_OUI_TYPE */
	uint8 capabilities_len;	/* WFA capabilities length */
	uint8 capabilities[];	/* WFA capability data + optional attributes */
} BWL_POST_PACKED_STRUCT wfa_cap_ie_t;
#define WFA_CAP_IE_HDR_SIZE (sizeof(wfa_cap_ie_t))

/* QoS Mgmt IE */
typedef BWL_PRE_PACKED_STRUCT struct qos_mgmt_ie {
	uint8 id;		/* 0xDD, IEEE 802.11 vendor specific information element */
	uint8 len;		/* length of data + 4u */
	uint8 oui[WFA_OUI_LEN];	/* WFA OUI */
	uint8 oui_type;		/* QOS_MGMT_VSIE_OUI_TYPE */
	uint8 data[];		/* One or more QoS Mgmt attributes */
} BWL_POST_PACKED_STRUCT qos_mgmt_ie_t;
#define QOS_MGMT_IE_HDR_SIZE (sizeof(qos_mgmt_ie_t))

/* DSCP Policy capability attribute */
typedef BWL_PRE_PACKED_STRUCT struct dscp_policy_capability_attr {
	uint8 id;		/* attribute id */
	uint8 len;		/* length of data */
	uint8 capabilities;	/* capabilities, 1 indicates DSCP Policy enabled */
} BWL_POST_PACKED_STRUCT dscp_policy_capability_attr_t;
#define DSCP_POLICY_CAPABILITY_ATTR_SIZE (sizeof(dscp_policy_capability_attr_t))

/* QoS Mgmt capability bits */
typedef enum qos_mgmt_cap_bits {

	/* When bit 0 is set, indicates the DSCP Policy support */
	QOS_MGMT_CAP_DSCP_POLICY			= (1u << 0u),

	/* When bit 1 is set, means that AP intends to send an unsolicited
	 * DSCP Policy Request frame to the STA imminently once association
	 * (and any security exchanges) is complete, or set to 0 to indicate it
	 * does not intend to do so.
	 */
	QOS_MGMT_CAP_UNSOLICIT_DSCP_POLICY_AT_ASSOC	= (1u << 1u),

	/* Indicates the support for SCS traffic description when bit2 is set to 1, otherwise
	 * not supported.
	 */
	QOS_MGMT_CAP_SCS_TRAFFIC_DESCRIPTION		= (1u << 2u),

	/* 5G QoS to Wi-Fi QoS Mapping */
	QOS_MGMT_CAP_5G_QOS_TO_WIFI_QOS_MAPPING		= (1u << 3u),

	/* Indicating the Dynamic Analytics Report (DAR) capability bits */
	/* Latency Statistics */
	QOS_MGMT_CAP_DAR_LATENCY_STATISTICS		= (1u << 4u),

	/* Radio Counters Statistics */
	QOS_MGMT_CAP_DAR_RADIO_COUNTERS_STATISTICS	= (1u << 5u),

	/* Control Plane Events */
	QOS_MGMT_CAP_DAR_CONTROL_PLANE_EVENTS		= (1u << 6u),

	/* STA or AP accepts the recpetio of Unsolicited reports */
	QOS_MGMT_CAP_DAR_UNSOLICIT_REPORT_RECEPTION	= (1u << 7u)
} qos_mgmt_cap_bits_t;

/* DSCP Port Range attribute */
typedef BWL_PRE_PACKED_STRUCT struct dscp_policy_port_range_attr {
	uint8 id;		/* attribute id */
	uint8 len;		/* length of data */
	uint16 start_port;	/* port range (both are in network byte order): start port */
	uint16 end_port;	/* end port */
} BWL_POST_PACKED_STRUCT dscp_policy_port_range_attr_t;
#define DSCP_POLICY_PORT_RANGE_ATTR_SIZE (sizeof(dscp_policy_port_range_attr_t))

/* DSCP Policy attribute */
typedef BWL_PRE_PACKED_STRUCT struct dscp_policy_attr {
	uint8 id;		/* attribute id */
	uint8 len;		/* length of data */
	uint8 policy_id;	/* policy id: 1..255, 0 is reserved */
	uint8 req_type;		/* 0(Add), 1(Remove), 2..255 (Reserved) */
	uint8 dscp;		/* DSCP value associated with the policy */
} BWL_POST_PACKED_STRUCT dscp_policy_attr_t;
#define DSCP_POLICY_ATTR_SIZE (sizeof(dscp_policy_attr_t))

/* DSCP Policy TCLAS attribute */
typedef BWL_PRE_PACKED_STRUCT struct dscp_policy_tclas_attr {
	uint8 id;		/* attribute id */
	uint8 len;		/* length of data */
	uint8 data[];		/* frame classifier type 4 data */
} BWL_POST_PACKED_STRUCT dscp_policy_tclas_attr_t;
#define DSCP_POLICY_TCLAS_ATTR_SIZE (sizeof(dscp_policy_tclas_attr_t))

/* DSCP Policy Domain Name attribute */
typedef BWL_PRE_PACKED_STRUCT struct dscp_policy_domain_name_attr {
	uint8 id;		/* attribute id */
	uint8 len;		/* length of data */
	uint8 data[];		/* domain name */
} BWL_POST_PACKED_STRUCT dscp_policy_domain_name_attr_t;
#define DSCP_POLICY_DOMAIN_NAME_ATTR_SIZE (sizeof(dscp_policy_domain_name_attr_t))

/* WFA RSN/RSNXE Override Vendor Specific IE */
#define WFA_RSN_OVERRIDE_OUI_TYPE_OFFSET	5u
#define WFA_RSN_OVERRIDE_IE_DATA_OFFSET		6u
typedef BWL_PRE_PACKED_STRUCT struct wfa_rsn_override_ie {
	uint8 id;               /* DOT11_MNG_VS_ID 0xDD(221) */
	uint8 len;              /* IE length */
	uint8 oui[WFA_OUI_LEN]; /* WFA OUI 50:6F:9A */
	uint8 oui_type;         /* WFA_OUI_TYPE_RSN(XE)_OVERRIDE */
	uint8 data[];           /* RSN/RSNXE IE attributes */
} BWL_POST_PACKED_STRUCT wfa_rsn_override_ie_t;
#define WFA_RSN_OVERRIDE_IE_SIZE (sizeof(wfa_rsn_override_ie_t))

/* WFA_RSN_SELECTION_IE definitions */
#define RSN_SELECTION_RSNE	0u	/* RSNE */
#define RSN_SELECTION_RSNO1E	1u	/* RSN Override Element */
#define RSN_SELECTION_RSNO2E	2u	/* RSN Override 2 Element */
typedef BWL_PRE_PACKED_STRUCT struct wfa_rsn_selection_ie {
	uint8 id;               /* DOT11_MNG_VS_ID 0xDD(221) */
	uint8 len;              /* IE length */
	uint8 oui[WFA_OUI_LEN]; /* WFA OUI 50:6F:9A */
	uint8 oui_type;         /* WFA_OUI_TYPE_RSN_SELECTION */
	uint8 data;		/* 0 = RSNE, 1 = RSNE Override , 2 = RSN Override 2 */
} BWL_POST_PACKED_STRUCT wfa_rsn_selection_ie_t;
#define WFA_RSN_SELECTION_IE_SIZE (sizeof(wfa_rsn_selection_ie_t))

#define WFA_RSNOV_LINK_KDE_IE_DATA_OFFSET 7u
typedef BWL_PRE_PACKED_STRUCT struct wfa_rsnov_link_kde_ie {
	uint8 id;               /* DOT11_MNG_PROPR_ID 0xDD(221) */
	uint8 len;              /* IE length */
	uint8 oui[WFA_OUI_LEN]; /* WFA OUI 50:6F:9A */
	uint8 oui_type;         /* WFA_OUI_TYPE_RSN_OVERRIDE_MLO_LINK_KDE */
	uint8 link_id;		/* Link identifier */
	uint8 data[];		/* The set of RSN Override, Override 2, RSNXE Overrie IEs */
} BWL_POST_PACKED_STRUCT wfa_rsnov_link_kde_ie_t;
#define WFA_RSNOV_LINK_KDE_IE_SIZE (sizeof(wfa_rsnov_link_kde_ie_t))

/* Wi-Fi Alliance Capabilities frame header */
typedef BWL_PRE_PACKED_STRUCT struct wfa_capabilities_vs_frmhdr {
	uint8 category;			/* Category VS/VSP; see 802.11.h */
	uint8 oui[WFA_OUI_LEN];		/* WFA OUI */
	uint8 oui_type;			/* DSCP_POLICY_AF_OUI_TYPE */
	uint8 capabilities_length;	/* Length of the Capabilities field in octets */
	uint8 data[];			/* zero or more capabilities and zero or more
					 * Wi-Fi Alliance Capabilities attributes
					 */
} BWL_POST_PACKED_STRUCT wfa_capabilities_action_vs_frmhdr_t;
#define WFA_CAPABILITIES_ACTION_FRAME_HDR_SIZE (sizeof(wfa_capabilities_action_vs_frmhdr_t))

/* Wi-Fi Alliance Capabilities attributes */
typedef enum wfa_capabilities_attrs {
	/* Generational Capabilities Indication attribute */
	WFA_GEN_CAP_INDICATION_ATTR	= 1
} wfa_capabilities_attrs_t;

/* Supported Generations field */
typedef enum wifi_supported_gen_bits {
	/* When bit is set to 1, the corresponding technology is supported */
	SUPPORTED_GEN_WIFI_4			= (1u << 0u),
	SUPPORTED_GEN_WIFI_5			= (1u << 1u),
	SUPPORTED_GEN_WIFI_6			= (1u << 2u),
	SUPPORTED_GEN_WIFI_7			= (1u << 3u)
} wifi_supported_gen_bits_t;

/* Certified Generations field */
typedef enum wifi_certified_gen_bits {
	/* When bit is set to 1, the corresponding technnology is certified */
	CERTIFIED_GEN_WIFI_n			= (1u << 0u),
	CERTIFIED_GEN_WIFI_ac			= (1u << 1u),
	CERTIFIED_GEN_WIFI_6			= (1u << 2u),
	CERTIFIED_GEN_WIFI_7			= (1u << 3u)
} wifi_certified_gen_bits_t;

/* Wi-Fi Alliance Capabilities attribute */
typedef BWL_PRE_PACKED_STRUCT struct wfa_gen_cap_attr {
	uint8 id;		/* Attribute id */
	uint8 len;		/* Length of data */
	uint8 data[];		/* Supported Generations Length (1 octet)
				 * Supported Generations n (>= 1)
				 * Certified Generations Length (0 or 1)
				 * Certified Generations (0 or n (>=1))
				 */
} BWL_POST_PACKED_STRUCT wfa_gen_cap_attr_t;
#define WFA_GEN_CAP_ATTR_SIZE (sizeof(wfa_gen_cap_attr_t))

/* DAR (Dynamic Analytics Report) frames */
/* DAR action frame subtypes */
#define DAR_AF_REQ_OUI_SUBTYPE			3u
#define DAR_AF_RESP_OUI_SUBTYPE			4u
#define DAR_AF_REPORT_OUI_SUBTYPE		5u

/* DAR request type add or remove */
#define DAR_REQ_TYPE_ADD			0u	/* Add */
#define DAR_REQ_TYPE_REMOVE			1u	/* Remove */

/* DAR report type */
#define DAR_REPORT_TYPE_HISTOGRAM		0u
#define DAR_REPORT_TYPE_PERCENTILE		1u
#define DAR_REPORT_TYPE_NO_PREFERENCE		2u

/* DAR report type threshold unit size */
#define DAR_REPORT_TYPE_HIST_TH_UNIT_SIZE	4u
#define DAR_REPORT_TYPE_PECT_TH_UNIT_SIZE	2u
#define DAR_REPORT_TYPE_DFLT_TH_UNIT_SIZE	1u

/* DAR report granularity */
#define DAR_REPORT_GRANULARITY_TID		0u
#define DAR_REPORT_GRANULARITY_AC		1u
#define DAR_REPORT_GRANULARITY_AGG_AC		2u

/* DAR link graularity */
#define DAR_LINK_GRANULARITY_DEVICE_OR_MLD	0u
#define DAR_LINK_GRANULARITY_LINK_LEVEL		1u

/* DAR resposne status codes */
#define DAR_RESP_ATTR_SC_SUCCESS				0u
#define DAR_RESP_ATTR_SC_ACCEPTED_WITH_CHANGES			1u
#define DAR_RESP_ATTR_SC_REQ_DECLINED				2u
#define DAR_RESP_ATTR_SC_REQUESTED_REPORT_NOT_SUPPORTED		3u
#define DAR_RESP_ATTR_SC_INSUFFICIENT_PROCESSING_RESOURCES	4u
/* 5..254 are reserved */
#define DAR_RESP_ATTR_TERMINATE					255u

/* DAR request action frame header */
typedef BWL_PRE_PACKED_STRUCT struct dar_af_req {
	uint8 category;		/* DOT11_ACTION_CAT_VSP, Vendor-specific Protected */
	uint8 oui[WFA_OUI_LEN];	/* WFA OUI */
	uint8 oui_type;		/* QOS_MGMT_AF_OUI_TYPE */
	uint8 oui_subtype;	/* DAR_AF_REQ_OUI_SUBTYPE */
	uint8 dialog_token;	/* Unique non-zero value to match req/resp */
	uint8 data[];		/* One or more QoS Management elements containing
				 * DAR attributes
				 */
} BWL_POST_PACKED_STRUCT dar_af_req_t;
#define DAR_REQUEST_AF_REQ_HDR_SIZE (sizeof(dar_af_req_t))

/* DAR response action frame header */
typedef BWL_PRE_PACKED_STRUCT struct dar_af_resp {
	uint8 category;		/* DOT11_ACTION_CAT_VSP, Vendor-specific Protected */
	uint8 oui[WFA_OUI_LEN];	/* WFA OUI */
	uint8 oui_type;		/* QOS_MGMT_AF_OUI_TYPE */
	uint8 oui_subtype;	/* DAR_AF_RESP_OUI_SUBTYPE */
	uint8 dialog_token;	/* Unique non-zero value to match req/resp */
	uint8 data[];		/* One or more QoS Management elements containing
				 * DAR attributes.
				 */
} BWL_POST_PACKED_STRUCT dar_af_resp_t;
#define DAR_RESP_AF_RESP_HDR_SIZE (sizeof(dar_af_resp_t))

/* DAR report action frame header */
typedef BWL_PRE_PACKED_STRUCT struct dar_af_report {
	uint8 category;		/* DOT11_ACTION_CAT_VSP, Vendor-specific Protected */
	uint8 oui[WFA_OUI_LEN];	/* WFA OUI */
	uint8 oui_type;		/* QOS_MGMT_AF_OUI_TYPE */
	uint8 oui_subtype;	/* DAR_AF_REPORT_OUI_SUBTYPE */
	uint8 data[];		/* One or more QoS Management elements containing
				 * DAR attributes.
				 */
}  BWL_POST_PACKED_STRUCT dar_af_report_t;
#define DAR_REPORT_AF_HDR_SIZE (sizeof(dar_af_report_t))

/* DAR request attribute */
typedef BWL_PRE_PACKED_STRUCT struct dar_req_attr {
	uint8 id;			/* Attribute id: DAR_REQ_ATTR */
	uint8 len;			/* Length of the following fields */
	uint8 request_id;		/* Non-zero value */
	uint8 request_type;		/* 0: Add, 1:Remove, 2-255 reserved */
	uint16 measurement_duration;	/* Indicats the requested measurement period
					 * in units of ms.
					 */
	uint16 number_of_measurements;	/* Indicates the a non-zero value for the requested
					 * number of measurements for periodic reporting.
					 * This field is reserved if request_type is not 0 (Add)
					 * or if report_method is not 0 (Periodic).
					 */
} BWL_POST_PACKED_STRUCT dar_req_attr_t;
#define DAR_REQUEST_ATTR_SIZE (sizeof(dar_req_attr_t))

/* DAR response attribute */
typedef BWL_PRE_PACKED_STRUCT struct dar_resp_attr {
	uint8 id;			/* Attribute id: DAR_RESP_ATTR */
	uint8 len;			/* Length of the following fields in this attribute */
	uint8 request_id;		/* Non-zero value */
	uint8 status_code;		/* status code */
} BWL_POST_PACKED_STRUCT dar_resp_attr_t;
#define DAR_RESP_ATTR_SIZE (sizeof(dar_resp_attr_t))

/* DAR report attribute */
typedef BWL_PRE_PACKED_STRUCT struct dar_report_attr {

	/* Attribute id: DAR_REPORT_ATTR */
	uint8 id;

	/* Length of the following fields in this attribute */
	uint8 len;

	/* Set to the same value as the Request ID in the corresponding DAR Request attribute,
	 * or to 0 for an unsolicited report.
	 */
	uint8 request_id;

	/* Indicates a timestamp at which the measurements for this report ended.
	 * The timestamp is represented as the four lower octets of the TSF timer.
	 * For an MLD, the TSF timer corresponds to the link specified in the
	 * Report Timestamp LinkID field.
	 */
	uint32 report_timestamp;

	/* The four LSBs of the Report Timestamp LinkID field indicates the
	 * link identifiers that correspond to the link for which the TSF timer
	 * is used to indicate the Report Timestamp. The four MSBs are reserved.
	 * The field is reserved if the DAR Responder is not an MLD.
	 */
	uint8 report_timestamp_linkid;

	/* The actual measurement duration for the report in units of ms. */
	uint16 report_measurement_duration;
} BWL_POST_PACKED_STRUCT dar_report_attr_t;
#define DAR_REPORT_ATTR_SIZE (sizeof(dar_report_attr_t))

/* DAR Latency statistics parameter presence bitmap */
typedef enum dar_latency_statistics_parameter_presence_bits {
	DAR_LATENCY_STATISTICS_PARAMETER_PRESENCE_BITS_THRESHOLD	= (1u << 0u),
	DAR_LATENCY_STATISTICS_PARAMETER_PRESENCE_BITS_STATISTICS	= (1u << 1u)
} dar_latency_statistics_parameter_presence_bits_t;

/* DAR Latency statistics attribute */
typedef BWL_PRE_PACKED_STRUCT struct dar_latency_statistics_attr {

	/* Attribute id: LATENCY_STATISTICS_ATTR */
	uint8 id;

	/* Length of the following fields in this attribute */
	uint8 len;

	/* Bit 0 indicates presence of the Thresholds field. This bit is set to 0 if
	 * the attribute is in a DAR Report frame.
	 * Bit 1 indicates presence of the Number of Latency Statistics Entries and
	 * Latency Statistics List fields. This bit is set to 1 if the attribute is in a
	 * DAR Report frame; otherwise it is set to 0.
	 * Bits 2-7: Reserved.
	 */
	uint8 parameter_presence_bitmap;

	/* 0: indicates the Latency Statistics List field contains, or is requested to contain,
	 * histogram(s) of latency measurements.
	 * 1: indicates the Latency Statistics List field contains, or is requested to contain,
	 * set(s) of percentile values of latency
	 * 2: indicates the request has no preference as to the contents of the Latency
	 * Statistics List field; this value is reserved when the attribute is included
	 * in a DAR Response or DAR Report frame
	 * 3-255: Reserved.
	 */
	uint8 report_type;

	/* 0: indicates the Latency Statistics List field contains, or is requested to contain,
	 * values with granularity at the TID level
	 * 1: indicates the Latency Statistics List field contains, or is requested to contain,
	 * values with granularity at the AC level
	 * 2: indicates the Latency Statistics List field contains, or is requested to contain,
	 * values with granularity aggregated across all ACs
	 * 3-255: Reserved.
	 */
	uint8 report_granularity;

	/* If the Report Granularity field is 0, Bit k is set to 1 if the latency statistics
	 * for TID k are reported or requested, otherwise Bit k is set to 0.
	 * If the Report Granularity field value is 1, each of bits 0-3 are set to 1 if the
	 * latency statistics for AC_BK, AC_BE, AC_VI and AC_VO, respectively, are reported
	 * or requested; otherwise the bit is set to 0. Bits 4-15 are reserved.
	 * This field is reserved if Report Granularity is not 0 (TID level) or 1 (AC level).
	 */
	uint16 report_granularity_bitmap;

	/* 0: indicates the Latency Statistics List field contains, or is requested to contain,
	 * values at the device level. If the DAR responder is an MLD, this means at the MLD level
	 * 1: indicates the Latency Statistics List field contains, or is requested to contain,
	 * values at the link level (for an MLD). This value is only used if the DAR responder
	 * is an MLD
	 * Values 2-255 are reserved.
	 */
	uint8 link_granularity;

	/* Bit i is set to 1 if Link i's latency statistics are reported or requested,
	 * else it is set to 0.
	 * This field is reserved if Link Granularity is not 1.
	 */
	uint8 link_granularity_bitmap;

	/* Based on the parameter_presense_bitmap, data contains:
	 * thresholds, number of latency statistics entries, latency statistics list
	 */
	uint8 data[];
} BWL_POST_PACKED_STRUCT dar_latency_statistics_attr_t;
#define DAR_LATENCY_STATISTICS_ATTR_SIZE (sizeof(dar_latency_statistics_attr_t))

/* DAR Latency statistics entry for report_type 0 */
typedef BWL_PRE_PACKED_STRUCT struct dar_latency_statistics_entry_rt0 {

	/* The lower bound of the latency range represented by this Latency Statistics Entry
	 * subfield, in units of ms.
	 * The values of the Lower Bound Latency subfield in consecutive Latency Statistics
	 * Entry subfields strictly increase within a given Latency Statistics Information
	 * subfield.
	 */
	uint8 lower_bound_latency;

	/* The number of unicast MSDUs transmitted by the DAR responder to the DAR initiator
	 * for whose latency falls between the value in Lower Bound Latency subfield in this
	 * Latency Statistics Entry subfield and the value in the Lower Bound Latency subfield
	 * of the next Latency Statistics Entry subfield in this Latency Statistics List field.
	 * For the final Latency Statistics Entry subfield in the Latency Statistics field,
	 * the latency falls above the value in the Lower Bound Latency subfield of this Latency
	 * Statistics Entry subfield.
	 */
	uint32 msdu_count;
} BWL_POST_PACKED_STRUCT dar_latency_statistics_entry_rt0_t;

/* DAR Latency statistics entry for report_type 1 */
typedef BWL_PRE_PACKED_STRUCT struct dar_latency_statistics_entry_rt1 {

	/* The percentile of the point on the empirical cumulative distribution function (ECDF)
	 * represented by this Latency Statistics Entry subfield.
	 */
	uint8 percentile;

	/* Latency, in units of ms, at the specified Percentile of the ECDF for MPDUs
	 * transmitted by the DAR responder to the DAR initiator during the measurement
	 * interval.
	 */
	uint16 latency;
} BWL_POST_PACKED_STRUCT dar_latency_statistics_entry_rt1_t;

/* Radio Counters attribute parameter presence bitmap */
typedef enum dar_radio_counters_parameter_presence_bits {
	/* Transmit Power field */
	DAR_RADIO_COUNTERS_PARAMETER_PRESENCE_BITS_TRANSMIT_POWER	= (1u << 0u),
	/* Observed Channel Utilization Fraction Field */
	DAR_RADIO_COUNTERS_PARAMETER_PRESENCE_BITS_OCU_FRACTION		= (1u << 1u),
	/* MPDU Count Statistics field */
	DAR_RADIO_COUNTERS_PARAMETER_PRESENCE_BITS_MPDU_STAT		= (1u << 2u),
	/* RTS Statistics field */
	DAR_RADIO_COUNTERS_PARAMETER_PRESENCE_BITS_RTS_STAT		= (1u << 3u),
	/* FCS Failure field */
	DAR_RADIO_COUNTERS_PARAMETER_PRESENCE_BITS_FCS_FAILURE		= (1u << 4u),
	/* 5 reserved */
	/* Threshold fields */
	DAR_RADIO_COUNTERS_PARAMETER_PRESENCE_BITS_THRESHOLD		= (1u << 6u),
	/* List fields contating analystics information */
	DAR_RADIO_COUNTERS_PARAMETER_PRESENCE_BITS_LIST_FIELD		= (1u << 7u),
	/* 8 .. 15 reserved */
} dar_radio_counters_parameter_presence_bits_t;

/* DAR Radio Counters attribute */
typedef BWL_PRE_PACKED_STRUCT struct dar_radio_counters_attr {
	/* Attribute id: LATENCY_STATISTICS_ATTR */
	uint8 id;

	/* Length of the following fields in this attribute */
	uint8 len;

	/* Bits 0 thru 5 indicate the presence of, or a request for the presence of,
	 * a field as follows:
	 * Bit 0: Transmit Power field
	 * Bit 1: Observed Channel Utilization Fraction field
	 * Bit 2: MPDU Count Statistics field
	 * Bit 3: RTS Statistics field
	 * Bit 4: FCS Failure field
	 * Bit 5: reserved.
	 * Bit 6 indicates the presence of Threshold fields within certain fields
	 * (when present) of this attribute - see definitions below.
	 * This bit is set to 0 if the attribute is in a DAR Report frame.
	 * Bit 7 indicates the presence of list fields containing analytics information
	 * within certain fields (when present) of this attribute see definitions below
	 * This bit is set to 1 if the attribute is in a DAR Report frame; otherwise it is set to 0.
	 * Bits 8 thru 15: reserved
	 */
	uint16 parameter_presence_bitmap;

	/* Based on the parameter_presense_bitmap, data contains:
	 * Report transmit power, Obeserved Channel Utilization Fraction, MPDU Count Statistics,
	 * RTS Statistics, FCS Failure
	 */
	uint8 data[];
} BWL_POST_PACKED_STRUCT dar_radio_counters_attr_t;
#define DAR_RADIO_COUNTERS_ATTR_SIZE (sizeof(dar_radio_counters_attr_t))

/* DAR radio counters attribute - Report Transmit Power field */
typedef BWL_PRE_PACKED_STRUCT struct report_transmit_power {

	/* 0: indicates the Transmit Power List field contains,
	 * or is requested to contain, values at the device level.
	 * This value is always used if the DAR Responder is not an MLD.
	 * 1: indicates the Transmit Power List field contains,
	 * or is requested to contain, values at the link level (for an MLD).
	 * This value is always used if the DAR Responder if an MLD.
	 * Values 2-255 are reserved
	 */
	uint8 link_granularity;

	/* Bit i is set to 1 if the transmit power for the link with link identifier equal to i
	 * (see 35.3.3.2 of 802.11BE draft 7.0) are reported or requested;
	 * otherwise that bit is set to 0.
	 * This field is reserved if Link Granularity is not 1
	 */
	uint8 link_granularity_bitmap;

	/* This field is present when Bit 7 of the Parameter Presence Bitmap is set to 1;
	 * otherwise it is absent
	 * Contains a list of Transmit Power subfields for each reported link.
	 * 0 or B where
	 * B is the Hamming weight of the Link Granularity Bitmap field
	 * if the Link Granularity field is 1, otherwise B is 1
	 */
	uint8 transmit_power_list[];
} BWL_POST_PACKED_STRUCT report_transmit_power_t;
#define REPORT_TRANSMIT_POWER_SIZE (sizeof(report_transmit_power_t))

/* DAR radio counters attribute - Observed Cahnnel Utilization Fraction field */
typedef BWL_PRE_PACKED_STRUCT struct observed_channel_utilization_fraction {

	/* 0: indicates the Observed Channel Utilization Fraction List field contains,
	 * or is requested to contain, values at the device level.
	 * This value is always used if the DAR Responder is not an MLD
	 * 1: indicates the Observed Channel Utilization Fraction List field contains
	 * or is requested to contain, values at the link level (for an MLD).
	 * This value is always used if the DAR Responder if an MLD.
	 * Values 2-255 are reserved
	 */
	uint8 link_granularity;

	/* Bit i is set to 1 if the observed channel utilization for the link with link
	 * identifier equal to i (see 35.3.3.2 of 802.11BE draft 7.0) are reported or requested;
	 * otherwise that bit is set to 0.
	 * This field is reserved if Link Granularity is not 1
	 */
	uint8 link_granularity_bitmap;

	/* Thresholds and Observed channel utilization fraction list
	 * depneds on link granularity bitmap fields
	 */
	uint8 data[];
} BWL_POST_PACKED_STRUCT observed_channel_utilization_fraction_t;
#define OBSERVED_CHANNEL_UTILIZATION_FRACTION_SIZE (sizeof(observed_channel_utilization_fraction_t))

/* DAR radio counters attribute - MPDU Count statistics field */
typedef BWL_PRE_PACKED_STRUCT struct mpdu_count_statistics {
	/* indicates the MPDU Count Statistics List field contains,
	 * or is requested to contain, values with granularity at the
	 * 0: TID level
	 * 1: AC level
	 * 2: Aggregated across all ACs
	 * 3-255: Reserved
	 */
	uint8 report_granularity;

	/* If the Report Granularity subfield value is 0,
	 * Bit k is set to 1 if the MPDU Count statistics for TID k is reported or requested,
	 * otherwise Bit k is set to 0
	 * If the Report Granularity subfield value is 1,
	 * each of bits 0-3 are set to 1 if the MPDU Count statistics for AC_BK, AC_BE, AC_VI
	 * and AC_VO, respectively, are requested or reported;
	 * otherwise the bit is set to 0. Bit 4-15 are reserved
	 */
	uint16 report_granularity_bitmap;

	/* 0: non-MLD Level, 1: MLD Level */
	uint8 link_granularity;

	/* Bit i is set to 1 if the statistics for the link with link
	 * identifier equal to i (see 35.3.3.2 of 802.11BE draft 7.0) are reported or requested;
	 * otherwise that bit is set to 0.
	 * This field is reserved if Link Granularity is not 1
	 */
	uint8 link_granularity_bitmap;

	/* Thresholds and MPDU count statistics list
	 * depend on report and link granularity bitmap fields
	 */
	uint8 data[];
} BWL_POST_PACKED_STRUCT mpdu_count_statistics_t;
#define MPDU_COUNT_STATISTICS_SIZE (sizeof(mpdu_count_statistics_t))

/* DAR radio counters attribute - MPDU Count statistics information subfield */
typedef BWL_PRE_PACKED_STRUCT struct mpdu_count_statistics_info_subfield {
	/* Count of unicast MPDUs that were sent by the DAR responder
	 * to the DAR initiator on the link, and successfully acknowledged
	 */
	uint32 successful_mpdu_count;
	/* Count of unicast MPDUs that were sent by the DAR responder
	 * to the DAR initiator on the link, were not acknowledged properly,
	 * were retried one or more times, then were successfully acknowledged
	 * before reaching the sender retry limit
	 */
	uint32 mpdu_retry_count;
} BWL_POST_PACKED_STRUCT mpdu_count_statistics_info_subfield_t;

/* DAR radio counters attribute - RTS statistics field */
typedef BWL_PRE_PACKED_STRUCT struct rts_statistics {
	/* indicates the RTS Statistics List field contains,
	 * or is requested to contain, values with granularity at the
	 * 0: TID level
	 * 1: AC level
	 * 2: Aggregated across all ACs
	 * 3-255: Reserved
	 */
	uint8 report_granularity;

	/* If the Report Granularity subfield value is 0,
	 * Bit k is set to 1 if the RTS statistics for TID k is reported or requested,
	 * otherwise Bit k is set to 0
	 * If the Report Granularity subfield value is 1,
	 * each of bits 0-3 are set to 1 if the RTS statistics for AC_BK, AC_BE, AC_VI
	 * and AC_VO, respectively, are requested or reported;
	 * otherwise the bit is set to 0. Bit 4-15 are reserved
	 */
	uint16 report_granularity_bitmap;

	/* 0: non-MLD Level, 1: MLD Level */
	uint8 link_granularity;

	/* Bit i is set to 1 if the statistics for the link with link
	 * identifier equal to i (see 35.3.3.2 of 802.11BE draft 7.0) are reported or requested;
	 * otherwise that bit is set to 0.
	 * This field is reserved if Link Granularity is not 1
	 */
	uint8 link_granularity_bitmap;

	/* Thresholds and RTS statistics list
	 * depend on report and link granularity bitmap fields
	 */
	uint8 data[];
} BWL_POST_PACKED_STRUCT rts_statistics_t;
#define RTS_STATISTICS_SIZE (sizeof(rts_statistics_t))

/* DAR radio counters attribute - RTS statistics information subfield */
typedef BWL_PRE_PACKED_STRUCT struct rts_statistics_info_subfield {
	/* The count of RTS frames that were sent by the DAR responder
	 * to the DAR initiator using the TID/AC on the link and
	 * for which a CTS frame was successfully received
	 */
	uint32 successful_rts_count;
	/* The count of RTS frames that were sent by the DAR responder
	 * to the DAR initiator using the TID/AC on the link and
	 * for which a CTS frame was not successfully received
	 */
	uint32 rts_failure_count;
} BWL_POST_PACKED_STRUCT rts_statistics_info_subfield_t;

/* DAR radio counters attribute - fcs failure count subfield */
typedef BWL_PRE_PACKED_STRUCT struct fcs_failure_count_subfield {
	/* Each FCS Failure Count subfield is 4-octets, and indicates
	 * the count of frames received by the DAR responder
	 * (irrespective of the transmitter of those frames) on the link,
	 * for which FCS validation failed
	 */
	uint32 fcs_failure_count;
} BWL_POST_PACKED_STRUCT fcs_failure_count_subfield_t;
#define FCS_FAILURE_COUNT_SUBFIELD_SIZE (sizeof(fcs_failure_count_subfield_t))

/* DAR radio counters attribute - FCS failure statistics */
typedef BWL_PRE_PACKED_STRUCT struct fcs_failure_statistics {
	/* 0: MLD-level, 1: Link-level only used if the DAR responder is an MLD */
	uint8 link_granularity;

	/* Bit i is set to 1 if the fcs statistics for the link with link
	 * identifier equal to i (see 35.3.3.2 of 802.11BE draft 7.0) are reported or requested
	 * otherwise that bit is set to 0.
	 * This field is reserved if Link Granularity is not 1
	 */
	uint8 link_granularity_bitmap;

	/* This field is present when Bit 7 of the Parameter Presence Bitmap of this field
	 * is set to 1, otherwise it is absent.
	 * Contains a list of FCS Failure Count subfields for each reported link.
	 * If the Link Granularity field is 0, the subfields are ordered by link,
	 * as specified in the Link Granularity Bitmap.
	 * Each FCS Failure Count subfield is 4-octets and indicates the count of frames
	 * received by the DAR responder (irrespective of the transmitter of those frames)
	 * on the link for which FCS validation failed.
	 */
	uint8 fcs_failure_list[];
} BWL_POST_PACKED_STRUCT fcs_failure_statistics_t;
#define FCS_FAILURE_STATISTICS_SIZE (sizeof(fcs_failure_statistics_t))

/* DAR Control Plane Events attribute */
typedef BWL_PRE_PACKED_STRUCT struct dar_control_plane_events_attr {
	/* Attribute id: DAR_RADIO_COUNTERS_STATISTICS_ATTR */
	uint8 id;

	/* Length of the following fields in this attribute */
	uint8 len;

	/* The number of Control Plane Event Count */
	uint8 count;

	/* Control Plane Events Tuple List */
	uint8 list[];
} BWL_POST_PACKED_STRUCT dar_control_plane_events_attr_t;
#define DAR_CONTROL_PLANE_EVENTS_ATTR_SIZE (sizeof(dar_control_plane_events_attr_t))

typedef enum dar_control_plane_event_sub_category {
	DAR_CONTROL_PLANE_EVENT_SUB_CATEGORY_RESERVED		= 0u,
	DAR_CONTROL_PLANE_EVENT_SUB_CATEGORY_DFS_EVENTS		= 1u,
	DAR_CONTROL_PLANE_EVENT_SUB_CATEGORY_BEACON_LOSS	= 2u,
	DAR_CONTROL_PLANE_EVENT_SUB_CATEGORY_SEQ_SN_JUMP	= 3u,
	DAR_CONTROL_PLANE_EVENT_SUB_CATEGORY_RTS_CTS_FLOODS	= 4u,
	DAR_CONTROL_PLANE_EVENT_SUB_CATEGORY_BA_NEGO_FAILUE	= 5u
	/* 6 - 255 Reserved */
} dar_control_plane_event_sub_category_t;

#define DAR_CONTROL_PLANE_EVENT_RESERVED_LINK_ID	(0xFF)
/* DAR Control Plane Event Tuple field */
typedef BWL_PRE_PACKED_STRUCT struct dar_control_plane_event_tuple {
	/* Link ID, the four LSBs of the LinkID field indicates the link identifier,
	 * 0xFF set for non-MLD or not link-specific. the four MSBs are reserved
	 */
	uint8 link_id;
	/* Category Code */
	uint8 category_code;
	/* Sub Category Code */
	uint8 sub_category_code;
} BWL_POST_PACKED_STRUCT dar_control_plane_event_tuple_t;
#define DAR_CONTROL_PLANE_EVENT_TUPLE_SIZE (sizeof(dar_control_plane_event_tuple_t))

/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#endif /* _802_11wfa_h_ */
