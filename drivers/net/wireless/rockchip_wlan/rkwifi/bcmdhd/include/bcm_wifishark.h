/*
 * Common header file for DHD logger and bcmwifi_dissector component.
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

#ifndef _BCM_WIFISHARK_H_
#define _BCM_WIFISHARK_H_

/*
 * 0x888A is reserved Broadcom ether type.
 * Refer https://standards-oui.ieee.org/ethertype/eth.txt
 */
#define ETHER_TYPE_BROADCOM 0x888A

#include <packed_section_start.h>
typedef BWL_PRE_PACKED_STRUCT struct log_msgbuf_ring {
	uint16 hlrd, hlwr; /* Host local rd wr */
	uint16 hdrd, hdwr; /* Host DMA rd wr */
} BWL_POST_PACKED_STRUCT log_msgbuf_ring_t;
#include <packed_section_end.h>

/*
 * DHD PCIe IPC(msgbuf) log format:-
 * -------------------------------------------------------------------------------------------
 * |<ether_header>|<log_bcmpcie_msg_type_t>|<driver_state_t>|<bcmpcie_msg_type_t>|<buf>|<len>|
 * -------------------------------------------------------------------------------------------
 * |    Ethernet  |    opcode, flags       |  driver state  | bcmpcie_msg_type_t | buf | len |
 * |     Header   |                        |     payload    |                    |     |     |
 * -------------------------------------------------------------------------------------------
 *  1. If driver_state 0, driver state payload is not present.
 *  2. Buf can be NULL and only driver state payload, can also be logged.
 *  3. But one of driver state payload or buf should be logged.
 */
#define BCMWIFI_PCIESTATE_OPCODE 0xa5a5000000000007u /* 8 bytes */
#include <packed_section_start.h>
typedef BWL_PRE_PACKED_STRUCT struct driver_state_info {
	uint8 version;
	uint8 length;
	uint16 max_eventbufpost;
	uint16 cur_event_bufs_posted;
	uint16 max_ioctlrespbufpost;
	uint16 cur_ioctlresp_bufs_posted;
	/* ctrl post and compl */
	log_msgbuf_ring_t log_h2dring_ctrl_subn; /* H2D ctrl message submission ring */
	log_msgbuf_ring_t log_d2hring_ctrl_cpln; /* D2H ctrl completion ring */
	/* inband stats */
	uint32 d3_inform_cnt;
	uint32 d0_inform_cnt;
	uint32 hostready_count; /* Number of hostready issued */
	ulong host_irq_enable_count;
	ulong host_irq_disable_count;
} BWL_POST_PACKED_STRUCT driver_state_t;
#include <packed_section_end.h>

#define PCIE_STATE_DRIVERSTATE 0x00000001u /* driver_state_t payload present or not */

#include <packed_section_start.h>
typedef BWL_PRE_PACKED_STRUCT struct bcmpcie_msg_type_header_info {
	uint64 opcode; /* BCM_PCIESTATE_OPCODE */
	uint32 flags; /* flags */
} BWL_POST_PACKED_STRUCT log_bcmpcie_msg_type_t;
#include <packed_section_end.h>

/*
 * Unlike the IPC enum bcmpcie_msg_type_t which start with 1, this is meant for logger.
 */
#define MSG_TYPE_DRIVER_STATE 0u

#define BCMWIFI_EVENTLOGS_OPCODE 0xa5a500000000000du /* 8 bytes */
/* To be compatable with the existing material this 4 bytes must be zero */
#define EVENT_LOGS_HEADER 0x00000000u

/**
 * FW infobuf/edl eventlogs format:-
 * --------------------------------------------------
 * | bcm_eventlogs_header_info |  event log buf     |
 * --------------------------------------------------
 */
#include <packed_section_start.h>
typedef BWL_PRE_PACKED_STRUCT struct bcm_eventlogs_header_info {
	uint64 opcode; /* BCMWIFI_EVENTLOGS_OPCODE */
	uint32 header;
} BWL_POST_PACKED_STRUCT log_bcm_eventlogs_t;
#include <packed_section_end.h>

#endif /* _BCM_WIFISHARK_H_ */
