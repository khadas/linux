/*
 * Source file for DHD Logger header prepatation and
 * implementation to send header and buffer to DHD logger.
 * This is a OS independent file
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

#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>
#include <dhd_dbg.h>
#include <wlioctl.h>
#include <dhd_logger.h>
#include <dhd.h>
#include <dhd_os_logger.h>
#include <bcmendian.h>

#define FILTER_IOCTL      0x00000001u
#define FILTER_EVENT      0x00000002u
#define FILTER_PCIE_IPC   0x00000004u
#define FILTER_EVENT_LOGS 0x00000008u

/*
 * DHD common logger info structure, private to DHD Logger module.
 * The rest of the DHD code need not know the implementation
 * During attach,
 * 1) osh from dhd_pub should be populated into dhd_logger_info
 * 2) Pointer to os logger(poslg) is stored in dhd_logger_info after os logger attach
 * 3) pointer to dhd_logger_info should be stored in the dhd_pub
 */
struct dhd_logger_info
{
	osl_t *osh;	/* OSL Handle	*/
	dhd_os_logger_t *poslg; /* os logger context which is opaque to this module  */
	/* Though dhdp is available, avoid/limit it's usage to access data outside logger module.
	 * Logger is a one way module where caller should provide all the data to log. Logger
	 * should not go back into rest of the modules to get data to log.
	 * ------------------------     --------     -----------
	 * |Rest of the DHD module| --> |Logger| --> |OS Logger|
	 * ------------------------     --------     -----------
	 */
	dhd_pub_t *dhdp; /* DHD pub handle */
	/*
	 * Filter to enable/disable logging of each type
	 * If the corresponding bit position for type is 1, logging is enabled
	 * If the corresponding bit position for type is 0, logging is disabled
	 */
	uint32 filter;
	/* if set route events to logger, else discard */
	bool route_events;

};

/*
 * The user space packet parsing applications like wireshark expect ethernet packets.
 * So for the logger packets which are originated from DHD prepend
 * a hardcoded ethernet header and then send the packet to network stack.
 * Below hardcoded ethernet header is prepended to all the logger packets originating
 * from the DHD.
 * 0x888A is reserved Broadcom ether type. Refer https://standards-oui.ieee.org/ethertype/eth.txt
 */
#define ETHER_TYPE_BROADCOM_SWAP 0x8A88          /* Broadcom Corp. */
struct ether_header logger_ether_header = {
	.ether_dhost = {LOGGER_ETHER_SRC_ADDR},
	.ether_shost = {LOGGER_ETHER_DEST_ADDR},
	.ether_type = ETHER_TYPE_BROADCOM_SWAP,
};

/* opcode to differentiate or idetify the packet type being logged */
#define BRCM_COMMANDER_OPCODE 0xa5a5000000000003 /* 8 bytes */
#define BRCM_DONGLE_ERROR_OPCODE 0xa5a500000000dead /* 8 bytes */

/*
 * IOCTL log format:-
 * -------------------------------------------------------------------------------
 * |  <ether_header> |           <wl_commander_t>               |<ioctl payload>|
 * -------------------------------------------------------------------------------
 * | Ethernet Header |    IOCTL Opcode, REQ/RES, Metadata       | IOCTL Payload |
 * -------------------------------------------------------------------------------
 */
#define COMMANDER_REQUEST 0 /* IOCTL/IOVAR request */
#define COMMANDER_RESPONSE 1 /* IOCTL/IOVAR response */

typedef struct wl_commander {
	uint64 opcode;	/* BRCM_COMMANDER_OPCODE */
	uint32 req_resp; /* COMMANDER_REQUEST / COMMANDER_RESPONSE */
	uint32 cmd;     /* common ioctl definition or command being fired  */
	uint32 len;     /* length of user buffer after wl_commander i.e this structure */
	uint32 set;     /* 1=set IOCTL; 0=query IOCTL */
	int32 ifidx;    /* interface index this is valid for */
	uint32 output_buf_len; /* maximum or expected response length, valid only ioclt req */
	int32 status;   /* response status */
	uint32 ioctl_trans_id; /* transaction id */
} __attribute__ ((packed)) wl_commander_t;

/*
 * Dongle error log format:-
 * ---------------------------------------------------------------------------------------
 * |  <ether_header> |           <brcm_error_header>            |      <error buf>       |
 * ---------------------------------------------------------------------------------------
 * | Ethernet Header |          Dongle Error opcode             | Buffer with error name |
 * ---------------------------------------------------------------------------------------
 */
struct brcm_error_header {
	uint64 opcode; /* BRCM_DONGLE_ERROR_OPCODE */
} __attribute__ ((packed));

struct brcm_error_header log_error = {
	.opcode = BRCM_DONGLE_ERROR_OPCODE,
};

/**
 * FW infobuf/edl eventlogs format:-
 * --------------------------------------------------
 * |  log_bcm_eventlogs_t    |    event log buf     |
 * --------------------------------------------------
 */
log_bcm_eventlogs_t log_bcm_eventlogs = {
	.opcode = BCMWIFI_EVENTLOGS_OPCODE,
	.header = EVENT_LOGS_HEADER,
};

/*
 * API to log IOCTL request.
 * @param[in] pdl  Pointer to context structure
 * @param[in] cmd  IOCTL command this request is for
 * @param[in] action  IOCTL query / set
 * @param[in] ifidx  Interface on which the ioctl is fired
 * @param[in] trans_id  tansaction id of the ioctl request
 * @param[in] output_buf_len  expected length of the ioctl request i.e expected response length
 * @param[in] buf  IOCTL request payload buffer
 * @param[in] len  IOCTL request payload buffer length
 * @return none
 */
void
dhd_log_ioctlreq(dhd_logger_t *pdl, uint32 cmd, uint8 action, int ifidx, uint16 trans_id,
	uint16 output_buf_len, void *buf, int len)
{
	wl_commander_t commander_request;
	unsigned char *header;
	uint header_len;
	uint32 offset = 0;
	int ret = 0;

	/* If logging is disabled, return without logging */
	if (!(pdl->filter & FILTER_IOCTL)) {
		return;
	}

	header_len = sizeof(logger_ether_header) + sizeof(commander_request);
	/* can be called from non sleepable context, so avoid VMALLOCZ */
	if (!(header = (unsigned char *)MALLOCZ(pdl->osh, header_len))) {
		DHD_ERROR(("%s() mem alloc failed, not logged\n", __FUNCTION__));
		return;
	}
	ret = memcpy_s(header + offset, header_len - offset,
		&logger_ether_header, sizeof(logger_ether_header));
	if (ret != 0) {
		DHD_ERROR(("%s() memcpy_s error\n", __FUNCTION__));
		goto exit;
	}
	offset += sizeof(logger_ether_header);

	bzero(&commander_request, sizeof(commander_request));
	commander_request.opcode = BRCM_COMMANDER_OPCODE;
	commander_request.req_resp = COMMANDER_REQUEST;
	commander_request.cmd = cmd;
	commander_request.len = len;
	commander_request.set  = (action & WL_IOCTL_ACTION_SET);
	commander_request.ifidx = ifidx;
	commander_request.output_buf_len = output_buf_len;
	commander_request.ioctl_trans_id = trans_id;
	DHD_INFO(("cmd : %d set: %d trans_id: %d buf %p len %x\n",
		cmd, commander_request.set, trans_id, buf, len));
	ret = memcpy_s(header + offset, header_len - offset,
		&commander_request, sizeof(commander_request));
	if (ret != 0) {
		DHD_ERROR(("%s() memcpy_s error\n", __FUNCTION__));
		goto exit;
	}
	offset += sizeof(commander_request);
	dhd_os_log(pdl->poslg, LOG_TYPE_IOVAR, header, offset, buf, len);
exit:
	MFREE(pdl->osh, header, header_len);
}

/*
 * API to log IOCTL response.
 * @param[in] pdl  Pointer to context structure
 * @param[in] cmd  IOCTL command this response is for
 * @param[in] ifidx  Interface on which the ioctl was fired
 * @param[in] trans_id  tansaction id of this ioctl response. The same has to match with
 *                      the tansaction id of this ioctl request.
 * @param[in] status  status of the IOCTL fired ex: BCME_OK or BCME_ERROR ..etc
 * @param[in] buf  IOCTL response payload buffer
 * @param[in] len  IOCTL response payload buffer length
 * @return none
 */
void
dhd_log_ioctlres(dhd_logger_t *pdl, uint32 cmd, int ifidx, uint16 trans_id, int16 status,
	void *buf, int len)
{
	wl_commander_t commander_response;
	unsigned char *header;
	uint header_len;
	uint32 offset = 0;
	int ret = 0;

	/* If logging is disabled, return without logging */
	if (!(pdl->filter & FILTER_IOCTL)) {
		return;
	}

	header_len = sizeof(logger_ether_header) + sizeof(commander_response);
	/* can be called from non sleepable context, so avoid VMALLOCZ */
	if (!(header = (unsigned char *)MALLOCZ(pdl->osh, header_len))) {
		DHD_ERROR(("%s: mem mallo failed, not logged\n", __FUNCTION__));
		return;
	}
	ret = memcpy_s(header + offset, header_len - offset,
		&logger_ether_header, sizeof(logger_ether_header));
	if (ret != 0) {
		DHD_ERROR(("%s() memcpy_s error\n", __FUNCTION__));
		goto exit;
	}
	offset += sizeof(logger_ether_header);

	bzero(&commander_response, sizeof(commander_response));
	commander_response.opcode = BRCM_COMMANDER_OPCODE;
	commander_response.req_resp = COMMANDER_RESPONSE;
	commander_response.cmd = cmd;
	commander_response.ifidx = ifidx;
	commander_response.ioctl_trans_id = trans_id;
	commander_response.status = status;
	commander_response.len = len;
	DHD_INFO(("cmd %d ifidx %d  trans_id %d status %d buf %p len %x \n",
		cmd, ifidx, trans_id, status, buf, len));
	ret = memcpy_s(header + offset, header_len - offset,
		&commander_response, sizeof(commander_response));
	if (ret != 0) {
		DHD_ERROR(("%s() memcpy_s error\n", __FUNCTION__));
		goto exit;
	}
	offset += sizeof(commander_response);
	dhd_os_log(pdl->poslg, LOG_TYPE_IOVAR, header, offset, buf, len);
exit:
	MFREE(pdl->osh, header, header_len);
}

/*
 * API to log dump file names like memdump, sssr dump, log_dump ..etc
 * upon any dongle errors like dongle trap, ROT ..etc detected in host.
 * @param[in] pdl  Pointer to context structure
 * @param[in] buf  Pointer to the buffer containing the name of the file
 * @param[in] len  Length of the buffer containing the name of the file
 * @return none
 */
void
dhd_log_error(dhd_logger_t *pdl, char *buf, int len)
{
	unsigned char *header;
	uint header_len;
	uint32 offset = 0;
	int ret = 0;

	/* If data packet logging is disabled, return without logging */

	header_len = sizeof(logger_ether_header) + sizeof(log_error);
	if (!(header = (unsigned char *)MALLOCZ(pdl->osh, header_len))) {
		DHD_ERROR(("%s: malloc failed, not logged\n", __FUNCTION__));
		return;
	}
	ret = memcpy_s(header + offset, header_len - offset,
		&logger_ether_header, sizeof(logger_ether_header));
	if (ret != 0) {
		DHD_ERROR(("%s() memcpy_s header error\n", __FUNCTION__));
		goto exit;
	}
	offset += sizeof(logger_ether_header);

	ret = memcpy_s(header + offset, header_len - offset,
		&log_error, sizeof(log_error));
	if (ret != 0) {
		DHD_ERROR(("%s() memcpy_s payload error\n", __FUNCTION__));
		goto exit;
	}
	offset += sizeof(log_error);
	dhd_os_log(pdl->poslg, LOG_TYPE_ERROR, header, offset, buf, len);
	DHD_INFO(("%s: dump reason: %s len %d\n", __FUNCTION__, buf, len));
exit:
	MFREE(pdl->osh, header, header_len);
}

/*
 * API to log bcmpcie_msg_type_t(PCIe IPC) transactions between host and dongle.
 * @param[in] pdl  Pointer to context structure.
 * @param[in] driver_state  struct containing DHD information.
 * @param[in] msgtype  one of the types of bcmpcie_msg_type_t for which the log is being logged.
 * @param[in] buf msgtype payload.
 * @param[in] len msgtype payload len.
 * @return none
 */
void
dhd_log_msgtype(dhd_logger_t *pdl, driver_state_t *driver_state,
	bcmpcie_msg_type_t msgtype, void *buf, int len)
{
	log_bcmpcie_msg_type_t log_bcmpcie_msg_type;
	unsigned char *header;
	uint header_len;
	uint32 offset = 0;
	int ret = 0;

	/* IPC first level filter. If logging is disabled, return without logging */
	if (!(pdl->filter & FILTER_PCIE_IPC)) {
		return;
	}

	/* TODO: IPC sub filter. If logging is disabled, return without logging */

	if ((driver_state == NULL) && (buf == NULL)) {
		DHD_ERROR(("%s(), both driver_state and buf are NULL\n", __FUNCTION__));
		ASSERT(0);
		return;
	}

	/* Ethernet Header, log_bcmpcie_msg_type_t */
	header_len = sizeof(logger_ether_header) + sizeof(log_bcmpcie_msg_type_t);
	/* driver state payload */
	if (driver_state != NULL) {
		header_len += sizeof(driver_state_t);
	}
	/* bcmpcie_msg_type_t */
	header_len += sizeof(bcmpcie_msg_type_t);

	if (!(header = (unsigned char *)MALLOCZ(pdl->osh, header_len))) {
		DHD_ERROR(("%s(), malloc failed, not logged\n", __FUNCTION__));
		return;
	}

	/* Ethernet Header */
	ret = memcpy_s(header + offset, header_len - offset,
		&logger_ether_header, sizeof(logger_ether_header));
	if (ret != 0) {
		DHD_ERROR(("%s() memcpy_s ether header error\n", __FUNCTION__));
		goto exit;
	}
	offset += sizeof(logger_ether_header);

	/* opcode, flags */
	bzero(&log_bcmpcie_msg_type, sizeof(log_bcmpcie_msg_type));
	log_bcmpcie_msg_type.opcode = BCMWIFI_PCIESTATE_OPCODE;
	if (driver_state != NULL) {
		log_bcmpcie_msg_type.flags |= PCIE_STATE_DRIVERSTATE;
	}
	ret = memcpy_s(header + offset, header_len - offset,
		&log_bcmpcie_msg_type, sizeof(log_bcmpcie_msg_type));
	if (ret != 0) {
		DHD_ERROR(("%s() memcpy_s log_bcmpcie_msg_type error\n", __FUNCTION__));
		goto exit;
	}
	offset += sizeof(log_bcmpcie_msg_type);

	/* driver state payload */
	if (driver_state != NULL) {
		ret = memcpy_s(header + offset, header_len - offset,
			driver_state, sizeof(driver_state_t));
		if (ret != 0) {
			DHD_ERROR(("%s() memcpy_s driver_state error\n", __FUNCTION__));
			goto exit;
		}
		offset += sizeof(driver_state_t);
	}

	/* bcmpcie_msg_type_t */
	ret = memcpy_s(header + offset, header_len - offset,
		&msgtype, sizeof(bcmpcie_msg_type_t));
	if (ret != 0) {
		DHD_ERROR(("%s() memcpy_s bcmpcie_msg_type_t error\n", __FUNCTION__));
		goto exit;
	}
	offset += sizeof(bcmpcie_msg_type_t);

	DHD_INFO(("%s() flags:0x%x driver_state len: %ld msgtype: 0x%x\n",
		__FUNCTION__,
		log_bcmpcie_msg_type.flags,
		sizeof(driver_state_t),
		msgtype));
	dhd_os_log(pdl->poslg, LOG_TYPE_PCIE_IPC, header, offset, buf, len);

exit:
	MFREE(pdl->osh, header, header_len);
}

/**
 * Called to route FW events
 *
 * @param[plg]  Pointer to context structure
 * @param[buf]  pointer to network buffer
 * @param[len]  length of data in network data buffer
 * @return      BCME_OK on success / Error code on failure
 */
int32
dhd_log_route_events(dhd_logger_t *pdl, void *pkt, uint32 len)
{
	/* If event routing is disabled, return without logging */
	if (pdl->route_events == FALSE) {
		return BCME_OK;
	}

	/* send to os logger */
	dhd_os_log_route_events(pdl->poslg, pkt, len);
	return BCME_OK;
}

/**
 * Called to log infobuf eventlogs
 *
 * @param[plg]  Pointer to context structure
 * @param[buf]  pointer to network buffer
 * @param[len]  length of data in network data buffer
 * @return      BCME_OK on success / Error code on failure
 */
int32
dhd_log_infobuf_eventlogs(dhd_logger_t *pdl, void *pkt)
{
	/* If infobuf event logging is disabled, return without logging */
	if (!(pdl->filter & FILTER_EVENT_LOGS)) {
		return BCME_OK;
	}
	/* send to os logger */
	dhd_os_log_infobuf_eventlogs(pdl->poslg,
		&log_bcm_eventlogs, sizeof(log_bcm_eventlogs), pkt);
	return BCME_OK;
}

/**
 * Called to log edl eventlogs
 *
 * @param[plg]  Pointer to context structure
 * @param[buf]  pointer to network buffer
 * @param[len]  length of data in network data buffer
 * @return      BCME_OK on success / Error code on failure
 */
int32
dhd_log_edl_eventlogs(dhd_logger_t *pdl, uint8 *msg)
{
	uint32 msglen = 0;
	info_buf_payload_hdr_t *infobuf = NULL;

	/* If edl method of event logging is disabled, return without logging */
	if (!(pdl->filter & FILTER_EVENT_LOGS)) {
		return BCME_OK;
	}

	if (!msg) {
		DHD_ERROR(("%s msg is NULL\n", __FUNCTION__));
		return BCME_ERROR;
	}

	/* msg = |infobuf_ver(u32)|info_buf_payload_hdr_t|msgtrace_hdr_t|<var len data>| */
	infobuf = (info_buf_payload_hdr_t *)(msg + sizeof(uint32));
	msglen = (uint32)(ltoh16(infobuf->length) + sizeof(info_buf_payload_hdr_t) +
			sizeof(uint32));
	/* send to os logger */
	dhd_os_log(pdl->poslg, LOG_TYPE_EVENT_LOGS,
		&log_bcm_eventlogs, sizeof(log_bcm_eventlogs), msg, msglen);
	return BCME_OK;
}

/**
 * Called to log network packets
 *
 * @param[plg]  Pointer to context structure
 * @param[type] Type of the data to log (one of dhd_log_type)
 * @param[buf]  pointer to network buffer
 * @param[len]  length of data in network data buffer
 * @return      BCME_OK on success / Error code on failure
 */
int32
dhd_log_pkt(dhd_logger_t *pdl, uint32 type, void *pkt, uint32 len)
{
	/* If data packet logging is disabled, return without logging */

	/* send to os logger */
	dhd_os_log_pkt(pdl->poslg, type, pkt, len);
	return BCME_OK;
}

/* Log Filter APIs */

/**
 * Called to enable a dhd_log_type in filter
 *
 * @param[pdl]  Pointer to context structure
 * @param[type] Type of the data (one of dhd_log_type) to enable
 * @return      BCME_OK on success/error code on failure
 */
int32
dhd_log_enable_type(dhd_logger_t *pdl, uint32 type)
{
	return BCME_OK;
}

/**
 * Called to disable a dhd_log_type in filter
 *
 * @param[pdl]  Pointer to context structure
 * @param[type] Type of the data (one of dhd_log_type) to enable
 * @return      BCME_OK on success/error code on failure
 */
int32
dhd_log_disable_type(dhd_logger_t *pdl, uint32 type)
{
	return BCME_OK;
}

/* Sysfs control APIs */

/**
 * Called to get the queue debug dump level being set
 *
 * @param[pdl]  Pointer to context structure
 * @return      queue debug dump value
 */
uint32
dhd_log_get_qdump(dhd_logger_t *pdl)
{
	/*
	 * Filter to enable/disable logging of each type
	 * If the corresponding bit position for type is 1, logging is enabled
	 * If the corresponding bit position for type is 0, logging is disabled
	 */
	return dhd_os_log_get_qdump(pdl->poslg);
}

/**
 * Called to set the queue debug dump level
 *
 * @param[pdl]  Pointer to context structure
 * @param[qdump] queue/queues to dump
 * @return      BCME_OK on success/error code on failure
 */
int32
dhd_log_set_qdump(dhd_logger_t *pdl, int32 qdump)
{
	return dhd_os_log_set_qdump(pdl->poslg, qdump);
}

/**
 * Called to show route_events
 *
 * @param[pdl]  Pointer to context structure
 * @return      route_events value
 */
bool
dhd_log_show_route_events(dhd_logger_t *pdl)
{
	return pdl->route_events;
}

/**
 * Called to set route_events value
 *
 * @param[pdl]  Pointer to context structure
 * @param[route_events] log route_events value to set
 * @return      BCME_OK on success/error code on failure
 */
int32
dhd_log_set_route_events(dhd_logger_t *pdl, bool route_events)
{
	pdl->route_events = route_events;
	return BCME_OK;
}

/**
 * Called to show log filter
 *
 * @param[pdl]  Pointer to context structure
 * @return      log filter value
 */
uint32
dhd_log_show_filter(dhd_logger_t *pdl)
{
	return pdl->filter;
}

/**
 * Called to set log filter value
 *
 * @param[pdl]  Pointer to context structure
 * @param[filter] log filter value to set
 * @return      BCME_OK on success/error code on failure
 */
int32
dhd_log_set_filter(dhd_logger_t *pdl, uint32 filter)
{
	pdl->filter = filter;
	return BCME_OK;
}

/**
 * Called to set defulat log filtering during init.
 *
 * @param[pdl]  Pointer to context structure
 */
static void
dhd_logger_init_filter_defaults(dhd_logger_t *pdl)
{

	pdl->filter = (FILTER_IOCTL | FILTER_EVENT | FILTER_PCIE_IPC | FILTER_EVENT_LOGS);
}

/**
 * Called when DHD gets inserted into the Kernel
 * Perform context structure allocation, and attach
 * os logger attach
 *
 * @param[in] dhdp DHD pub handle
 * @return  pointer to the logger context
 */
dhd_logger_t *
dhd_logger_attach(dhd_pub_t *dhdp)
{
	dhd_logger_t *pdl = NULL;

	/* Input parameter validation */
	if (dhdp == NULL) {
		DHD_ERROR(("%s, invalid input\n", __FUNCTION__));
		return NULL;
	}

	/* Allocate the context structure */
	pdl = (dhd_logger_t *)VMALLOCZ(dhdp->osh, sizeof(dhd_logger_t));
	if (unlikely(!pdl)) {
		DHD_ERROR(("%s(): OOM to alloc dhd_logger_t\n", __FUNCTION__));
		return NULL;
	}

	/* Save higher/above layer fields */
	pdl->osh = dhdp->osh;
	pdl->dhdp = dhdp;
	DHD_PRINT(("%s pdl %p osh %p\n", __FUNCTION__, pdl, pdl->osh));

	/* Attach the os logger interface */
	pdl->poslg = dhd_os_logger_attach(pdl->osh);
	if (unlikely(!pdl->poslg)) {
		DHD_ERROR(("%s(), logger attach failed\n", __FUNCTION__));
		goto exit;
	}

	/* Initialize the logger filter defaults */
	dhd_logger_init_filter_defaults(pdl);

	DHD_PRINT(("%s poslg  %p\n", __FUNCTION__, pdl->poslg));

	return pdl;
exit:
	return NULL;
}

/**
 * Called when DHD gets removed from the Kernel
 * Perform context structure de-allocation
 *
 * @param[pdl] Pointer to logger context structure
 */
void
dhd_logger_detach(dhd_logger_t *pdl)
{
	/* Input parameter validation */
	if (!pdl) {
		DHD_ERROR(("%s(), NULL dhd_logger_t\n", __FUNCTION__));
		goto exit;
	}

	/* Detach os logger detach */
	dhd_os_logger_detach(pdl->poslg);

	/* Free any context memory */

	/* Free pdl itself */
	VMFREE(pdl->osh, pdl, sizeof(dhd_logger_t));
exit:
	return;
}
