/*
 * $Copyright Open Broadcom Corporation$
 *
 * Fundamental constants relating to 802.3
 *
 * $Id: 802.3.h 417943 2013-08-13 07:54:04Z $
 */

#ifndef _802_3_h_
#define _802_3_h_

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>

#define SNAP_HDR_LEN	6	/* 802.3 SNAP header length */
#define DOT3_OUI_LEN	3	/* 802.3 oui length */

BWL_PRE_PACKED_STRUCT struct dot3_mac_llc_snap_header {
	uint8	ether_dhost[ETHER_ADDR_LEN];	/* dest mac */
	uint8	ether_shost[ETHER_ADDR_LEN];	/* src mac */
	uint16	length;				/* frame length incl header */
	uint8	dsap;				/* always 0xAA */
	uint8	ssap;				/* always 0xAA */
	uint8	ctl;				/* always 0x03 */
	uint8	oui[DOT3_OUI_LEN];		/* RFC1042: 0x00 0x00 0x00
						 * Bridge-Tunnel: 0x00 0x00 0xF8
						 */
	uint16	type;				/* ethertype */
} BWL_POST_PACKED_STRUCT;

/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#endif	/* #ifndef _802_3_h_ */
