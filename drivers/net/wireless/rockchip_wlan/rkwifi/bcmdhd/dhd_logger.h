/*
 * Header file for DHD Logger
 *
 * DHD Logger module exports APIs that can be called by the rest of the
 * DHD code to log debug information. This debgu data is managed and
 * exported in OS specific way so that it can be post processed and
 * viewed using Protcol Viewers.
 *
 * The interface is OS independent/common.
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
 *
 */

#ifndef _DHD_LOGGER_H_
#define _DHD_LOGGER_H_
#include <bcm_wifishark.h>
#include <bcmmsgbuf.h>

/*
 * A note on packet logger used in conjuction with dhd_logger:
 * ----------------------------------------------------------------
 *
 * When DHD is built with dhd_logger, the packet logger would use
 * its internal data structure which is a list of "cloned" packets
 * to store the packet information.
 *
 * When DHD Logger is used, DHD Logger throws the packet up to the "logger interface"
 * DHD Logger is primarily used to send the IOCTL, EVENT, IPC ..etc information
 * originating from DHD to the user space. Filters say LOG_TYPE_IOVAR, LOG_TYPE_EVENTS,
 * LOG_TYPE_PCIE_IPC can be used to further tune the data being sent to the userspace
 * via DHD Logger interface.
 *
 */
struct dhd_logger_info;
typedef struct dhd_logger_info dhd_logger_t;

/*
 * TODO: Filter APIs that is APIs to be able to pick and choose what information should be logged
 * will be added subsequently and there will be run-time control provided for the same.
 * This way when a specific problem is being debugged and if the developers want to filter
 * the logs not just at the Wireshark level,
 * but at the source itself (what information is to be logged) it will be made possible
 */

/*
 * Enum that indicates the "type" of the log. This forms the basis
 * of filtering functionality implementaiton. The following is the
 * working principle
 *
 * - Each call to dhd_log carries the "type" inforamtion.
 * - type indicates the type of log indicated in the enum
 * - Before logging the logger will look into a filter
 * - There is a filter mask, all types enabled by default
 * - Interface to enable/disable each type getting logged
 * - If the log of the given type is disabled information is not
 *   logged
 */
/* TODO: Add instructions to add new "type" */
typedef enum dhd_log_type {
	/* ONBOARD PKT = ARP, DHCP */
	LOG_TYPE_IOVAR		= 0,
	LOG_TYPE_EVENTS		= 1,
	LOG_TYPE_PCIE_IPC	= 2,
	LOG_TYPE_ERROR		= 3, /* Error messages like FW trap ..etc */
	LOG_TYPE_DATA_PKT	= 4,
	LOG_TYPE_EVENT_LOGS	= 5
} dhd_log_type_t;

/*
 * The user space applications like wireshark expect ethernet packet.
 * So for the logger packets which are originated from DHD prepend
 * a hardcoded ethernet header and then send the packet to network stack.
 * TODO: The src and dest mac addresses are randomply picked. Will
 * this conflict any existing macaddr ?
 */
#define LOGGER_ETHER_SRC_ADDR 0x00, 0x90, 0x4c, 0xdb, 0xdb, 0x11
#define LOGGER_ETHER_DEST_ADDR 0x00, 0x90, 0x4c, 0xdb, 0xdb, 0x22

struct dhd_pub;
#ifdef DHD_LOGGER
/* Attach & detach to be called during module load & unload */
dhd_logger_t *dhd_logger_attach(struct dhd_pub *dhdp);

void
dhd_logger_detach(dhd_logger_t *pdl);

/* Init & De-init to be called during ifconfig up and down */
int32
dhd_logger_init(dhd_logger_t *pdl);

int32
dhd_logger_deinit(dhd_logger_t *pdl);

/* API to add packet log where any header need not be prepended for userspace */
int32
dhd_log_pkt(dhd_logger_t *pdl, uint32 type, void *pkt, uint32 len);

void
dhd_log_ioctlreq(dhd_logger_t *pdl, uint32 cmd, uint8 action, int ifidx,
	uint16 trans_id, uint16 output_buf_len, void *buf, int len);
void
dhd_log_ioctlres(dhd_logger_t *pdl, uint32 cmd, int ifidx,
	uint16 trans_id, int16 status, void *buf, int len);
void
dhd_log_error(dhd_logger_t *pdl, char *buf, int len);

void
dhd_log_msgtype(dhd_logger_t *pdl, driver_state_t *driver_state,
	bcmpcie_msg_type_t msgtype, void *buf, int len);

int32
dhd_log_route_events(dhd_logger_t *pdl, void *pkt, uint32 len);

int32 dhd_log_infobuf_eventlogs(dhd_logger_t *pdl, void *pkt);
int32 dhd_log_edl_eventlogs(dhd_logger_t *pdl, uint8 *msg);

#define DHD_LOG_IOCTL_REQ(pdl, cmd, action, ifidx, \
		trans_id, output_buf_len, ioct_buf, input_buf_len) \
do { \
	if (dhd_logger == TRUE) { \
		dhd_log_ioctlreq(pdl, cmd, action, ifidx, \
				trans_id, output_buf_len, ioct_buf, input_buf_len); \
	} \
} while (0)

#define DHD_LOG_IOCTL_RES(pdl, cmd, ifidx, xt_id, ioctl_status, retbuf_va, ioctl_resplen) \
	do { \
		if (dhd_logger == TRUE) { \
			dhd_log_ioctlres(pdl, cmd, ifidx, \
					xt_id, ioctl_status, retbuf_va, ioctl_resplen); \
		} \
	} while (0)

#define DHD_LOG_ERROR(pdl, buf, len) \
	do { \
		if (dhd_logger == TRUE) { \
			dhd_log_error(pdl, buf, len); \
		} \
	} while (0)

/*
 * For scenarios where,
 * 1. Ring updates are aggregated and then sent to FW
 *      a. Descriptor log is logged on to logger interface. In this case driver_state is NULL.
 *      b. When the treshold is reached or after timeout,
 *         when dorbell is rung driver state is logged. In this case buf is NULL.
 * 2. Dorebell is rung as well as ring is updated, both are logged on to logger interface.
 *    In this case both driver_state and buf are not NULL
 */
#define DHD_LOG_MSGTYPE(dhdp, pdl, driver_state, msgtype, buf, len) \
	do { \
		if (dhd_logger == TRUE) { \
			dhd_prot_get_driver_state(dhdp, driver_state); \
			dhd_log_msgtype(pdl, driver_state, msgtype, buf, len); \
		} \
	} while (0)

#define DHD_LOG_ROUTE_EVENTS(pdl, pkt, len) \
	do { \
		if (dhd_logger == TRUE) { \
			dhd_log_route_events(pdl, pkt, len); \
		} \
	} while (0)

#define DHD_LOG_INFOBUF_EVENTLOGS(pdl, pkt) \
	do { \
		if (dhd_logger == TRUE) { \
			dhd_log_infobuf_eventlogs(pdl, pkt); \
		} \
	} while (0)

#define DHD_LOG_EDL_EVENTLOGS(pdl, msg) \
	do { \
		if (dhd_logger == TRUE) { \
			dhd_log_edl_eventlogs(pdl, msg); \
		} \
	} while (0)

/* Log Filter APIs */
int32
dhd_log_enable_type(dhd_logger_t *pdl, uint32 type);

int32
dhd_log_disable_type(dhd_logger_t *pdl, uint32 type);

/* Sysfs control APIs */
uint32
dhd_log_get_qdump(dhd_logger_t *pdl);
int32
dhd_log_set_qdump(dhd_logger_t *pdl, int32 qdump);
uint32
dhd_log_show_filter(dhd_logger_t *pdl);
int32
dhd_log_set_filter(dhd_logger_t *pdl, uint32 filter);
bool
dhd_log_show_route_events(dhd_logger_t *pdl);
int32
dhd_log_set_route_events(dhd_logger_t *pdl, bool route_events);

#else
/*
 * Feature Disabled dummy implementations. Callers can introduce dhd_log APIs
 * without worrying to surrond them with #defines
 */
static INLINE dhd_logger_t *
dhd_logger_attach(struct dhd_pub *dhdp)
{
	BCM_REFERENCE(dhdp);
	return 0;
}

static INLINE void
dhd_logger_detach(dhd_logger_t *pdl)
{
	BCM_REFERENCE(pdl);
	return;
}

static INLINE int32
dhd_logger_init(dhd_logger_t *pdl)
{
	BCM_REFERENCE(pdl);
	return 0;
}

static INLINE int32
dhd_logger_deinit(dhd_logger_t *pdl)
{
	BCM_REFERENCE(pdl);
	return 0;
}

static INLINE int32
dhd_log_pkt(dhd_logger_t *pdl, uint32 type, void *pkt, uint32 len)
{
	BCM_REFERENCE(pdl);
	return 0;
}

#define DHD_LOG_IOCTL_REQ(pdl, cmd, action, ifidx, \
		trans_id, output_buf_len, ioct_buf, input_buf_len)
#define DHD_LOG_IOCTL_RES(pdl, cmd, ifidx, xt_id, ioctl_status, retbuf_va, ioctl_resplen)
#define DHD_LOG_ERROR(pdl, buf, len)
#define DHD_LOG_MSGTYPE(dhdp, pdl, driver_state, msgtype, buf, len) \
	do { \
		BCM_REFERENCE(driver_state); \
	} while (0)
#define DHD_LOG_ROUTE_EVENTS(pdl, pkt, len)
#define DHD_LOG_INFOBUF_EVENTLOGS(pdl, pkt)
#define DHD_LOG_EDL_EVENTLOGS(pdl, msg)

/* Log Filter APIs */
static INLINE int32
dhd_log_enable_type(dhd_logger_t *pdl, uint32 type)
{
	BCM_REFERENCE(pdl);
	BCM_REFERENCE(type);
	return 0;
}

static INLINE int32
dhd_log_disable_type(dhd_logger_t *pdl, uint32 type)
{
	BCM_REFERENCE(pdl);
	BCM_REFERENCE(type);
	return 0;
}

/* Sysfs control APIs */
static INLINE uint32
dhd_log_get_qdump(dhd_logger_t *pdl)
{
	return 0;
}

static INLINE int32
dhd_log_set_qdump(dhd_logger_t *pdl, int32 qdump)
{
	return 0;
}

static INLINE uint8
dhd_log_show_filter(dhd_logger_t *pdl)
{
	return 0;
}

static INLINE uint32
dhd_log_set_filter(dhd_logger_t *pdl, uint32 filter)
{
	return 0;
}

static INLINE bool
dhd_log_show_route_events(dhd_logger_t *pdl)
{
	return 0;
}

static INLINE int32
dhd_log_set_route_events(dhd_logger_t *pdl, bool route_events)
{
	return 0;
}

static INLINE int32
dhd_log_infobuf_eventlogs(dhd_logger_t *pdl, void *pkt)
{
	return 0;
}

static INLINE int32
dhd_log_edl_eventlogs(dhd_logger_t *pdl, uint8 *msg)
{
	return 0;
}
#endif /* DHD_LOGGER */
#endif /* _DHD_LOGGER_H_ */
