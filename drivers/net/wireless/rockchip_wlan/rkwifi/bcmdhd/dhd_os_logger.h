/*
 * Header file for DHD OS Logger.This interface is OS dependent.
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

#ifndef _DHD_LINUX_LOGGER_H_
#define _DHD_LINUX_LOGGER_H_

struct dhd_os_logger_info;
typedef struct dhd_os_logger_info dhd_os_logger_t;

/* Attach & detach to be called during module load & unload */
dhd_os_logger_t *
dhd_os_logger_attach(osl_t *osh);

void
dhd_os_logger_detach(dhd_os_logger_t *posdl);

/* Init & De-init to be called during ifconfig up and down */
int32
dhd_os_logger_init(dhd_os_logger_t *posdl);

int32
dhd_os_logger_deinit(dhd_os_logger_t *posdl);

/* API to add the log */
int32
dhd_os_log(dhd_os_logger_t *posdl, uint32 type, const void *header,
	const int header_len, const void *buf, const uint32 len);

/* API to route FW events */
int32
dhd_os_log_route_events(dhd_os_logger_t *posdl, void *pkt, uint32 len);
/* API to log infobuf eventlogs */
int32 dhd_os_log_infobuf_eventlogs(dhd_os_logger_t *poslg,
	const void *header, int header_len, struct sk_buff *pktbuf);

/* API to add packet log */
int32
dhd_os_log_pkt(dhd_os_logger_t *posdl, uint32 type, void *pkt, uint32 len);

/* Sysfs control APIs */
uint32 dhd_os_log_get_qdump(dhd_os_logger_t *poslg);
int32 dhd_os_log_set_qdump(dhd_os_logger_t *poslg, int32 qdump);
#endif /* _DHD_LINUX_LOGGER_H_ */
