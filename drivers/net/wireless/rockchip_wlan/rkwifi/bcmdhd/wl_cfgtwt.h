/*
 * Target Wake Time
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
#ifndef _wl_cfgtwt_h_
#define _wl_cfgtwt_h_

#ifdef WL_TWT_HAL_IF
#define BRCM_TWT_HAL_VENDOR_EVENT_BUF_LEN	500u
#define TWT_EVENT_BUFFER_SIZE_LARGE		1024u
#define TWT_INFO_DESC_RESUME_TIME_IMMEDIATE	1000u

typedef enum wifi_error {
	WIFI_SUCCESS			= 0,
	WIFI_ERROR_NONE			= 0,
	WIFI_ERROR_UNKNOWN		= -1,
	WIFI_ERROR_UNINITIALIZED	= -2,
	WIFI_ERROR_NOT_SUPPORTED	= -3,
	WIFI_ERROR_NOT_AVAILABLE	= -4,
	WIFI_ERROR_INVALID_ARGS		= -5,
	WIFI_ERROR_INVALID_REQUEST_ID	= -6,
	WIFI_ERROR_TIMED_OUT		= -7,
	WIFI_ERROR_TOO_MANY_REQUESTS	= -8,
	WIFI_ERROR_OUT_OF_MEMORY	= -9,
	WIFI_ERROR_BUSY			= -10
} wifi_error_t;

typedef struct {
	uint8 is_twt_requester_supported; // 0 for not supporting twt requester
	uint8 is_twt_responder_supported; // 0 for not supporting twt responder
	uint8 is_broadcast_twt_supported; // 0 for not supporting broadcast twt
	uint8 is_flexible_twt_supported;  // 0 for not supporting flexible twt schedules
	uint32 min_wake_duration_micros;  // minimum twt wake duration capable in microseconds
	uint32 max_wake_duration_micros;  // maximum twt wake duration capable in microseconds
	uint64 min_wake_interval_micros;  // minimum twt wake interval capable in microseconds
	uint64 max_wake_interval_micros;  // maximum twt wake interval capable in microseconds
} wifi_twt_capabilities;

typedef struct _twt_hal_resp {
	wifi_error_t wifi_error;
	wifi_twt_capabilities *twt_cap;
} twt_hal_resp_t;

extern int wl_cfgtwt_cmd_reply(struct wiphy *wiphy, twt_hal_resp_t *twt_req_resp);

/* TWT negotiation types */
typedef enum {
	WIFI_TWT_NEGO_TYPE_INDIVIDUAL	= 0,
	WIFI_TWT_NEGO_TYPE_BROADCAST	= 1
} wifi_twt_negotiation_type;

/* TWT teardown reason codes */
typedef enum {
	/* unknown reason */
	WIFI_TWT_TEARDOWN_REASON_CODE_UNKNOWN			= 0,
	/* teardown requested by the framework */
	WIFI_TWT_TEARDOWN_REASON_CODE_LOCALLY_REQUESTED		= 1,
	/* teardown initiated internally by the firmware or driver. */
	WIFI_TWT_TEARDOWN_REASON_CODE_INTERNALLY_INITIATED	= 2,
	/* teardown initiated by the peer */
	WIFI_TWT_TEARDOWN_REASON_CODE_PEER_INITIATED		= 3
} wifi_twt_teardown_reason_code;

/* TWT error codes */
typedef enum {
	/* unknown failure */
	WIFI_TWT_ERROR_CODE_FAILURE_UNKNOWN	= 0,
	/* TWT session is already resumed */
	WIFI_TWT_ERROR_CODE_ALREADY_RESUMED	= 1,
	/* TWT session is already suspended */
	WIFI_TWT_ERROR_CODE_ALREADY_SUSPENDED	= 2,
	/* invalid parameters */
	WIFI_TWT_ERROR_CODE_INVALID_PARAMS	= 3,
	/* maximum number of sessions reached */
	WIFI_TWT_ERROR_CODE_MAX_SESSION_REACHED	= 4,
	/* requested operation is not available */
	WIFI_TWT_ERROR_CODE_NOT_AVAILABLE	= 5,
	/* requested operation is not supported */
	WIFI_TWT_ERROR_CODE_NOT_SUPPORTED	= 6,
	/* requested operation is not supported by the peer */
	WIFI_TWT_ERROR_CODE_PEER_NOT_SUPPORTED	= 7,
	/* requested operation is rejected by the peer */
	WIFI_TWT_ERROR_CODE_PEER_REJECTED	= 8,
	/* requested operation is timed out */
	WIFI_TWT_ERROR_CODE_TIMEOUT		= 9
} wifi_twt_error_code;

typedef enum {
	ANDR_TWT_ATTR_NONE			= 0,
	ANDR_TWT_ATTR_SESSION_ID		= 1,
	ANDR_TWT_ATTR_MLO_LINK_ID               = 2,
	ANDR_TWT_ATTR_WAKE_DURATION		= 3,
	ANDR_TWT_ATTR_WAKE_INTERVAL		= 4,
	ANDR_TWT_ATTR_NEG_TYPE			= 5,
	ANDR_TWT_ATTR_IS_TRIGGER_ENABLED        = 6,
	ANDR_TWT_ATTR_IS_ANNOUNCED              = 7,
	ANDR_TWT_ATTR_IS_IMPLICIT               = 8,
	ANDR_TWT_ATTR_IS_PROTECTED              = 9,
	ANDR_TWT_ATTR_IS_UPDATABLE              = 10,
	ANDR_TWT_ATTR_IS_SUSPENDABLE            = 11,
	ANDR_TWT_ATTR_IS_RESP_PM_MODE_ENABLED   = 12,
	ANDR_TWT_ATTR_REASON_CODE               = 13,
	ANDR_TWT_ATTR_AVG_PKT_NUM_TX		= 14,
	ANDR_TWT_ATTR_AVG_PKT_NUM_RX		= 15,
	ANDR_TWT_ATTR_AVG_PKT_SIZE_TX           = 16,
	ANDR_TWT_ATTR_AVG_PKT_SIZE_RX           = 17,
	ANDR_TWT_ATTR_AVG_EOSP_DUR		= 18,
	ANDR_TWT_ATTR_EOSP_CNT			= 19,
	ANDR_TWT_ATTR_WAKE_DURATION_MIN		= 20,
	ANDR_TWT_ATTR_WAKE_DURATION_MAX		= 21,
	ANDR_TWT_ATTR_WAKE_INTERVAL_MIN		= 22,
	ANDR_TWT_ATTR_WAKE_INTERVAL_MAX		= 23,
	ANDR_TWT_ATTR_ERROR_CODE                = 24,
	ANDR_TWT_ATTR_SUB_EVENT			= 25,
	ANDR_TWT_ATTR_CAP			= 26,
	ANDR_TWT_IS_REQUESTOR_SUPPORTED		= 27,
	ANDR_TWT_IS_RESPONDER_SUPPORTED		= 28,
	ANDR_TWT_IS_BROADCAST_SUPPORTED		= 29,
	ANDR_TWT_IS_FLEXIBLE_SUPPORTED		= 30,
	ANDR_TWT_WIFI_ERROR			= 31,
	ANDR_TWT_ATTR_MAX
} andr_twt_attribute;

typedef enum {
	ANDR_TWT_EVENT_INVALID			= 0,
	ANDR_TWT_SESSION_EVENT_FAILURE		= 1,
	ANDR_TWT_SESSION_EVENT_CREATE		= 2,
	ANDR_TWT_SESSION_EVENT_UPDATE		= 3,
	ANDR_TWT_SESSION_EVENT_TEARDOWN		= 4,
	ANDR_TWT_SESSION_EVENT_STATS		= 5,
	ANDR_TWT_SESSION_EVENT_SUSPEND		= 6,
	ANDR_TWT_SESSION_EVENT_RESUME		= 7,
	ANDR_TWT_EVENT_LAST
} andr_twt_sub_event;

extern wifi_twt_error_code wl_cfgtwt_map_fw_to_hal_error_code(int fw_error_code);
extern wifi_error_t wl_cfgtwt_brcm_to_hal_error_code(int ret);
extern void wl_cfgtwt_send_stats_event(struct wireless_dev *wdev, struct bcm_cfg80211 *cfg,
	wl_twt_stats_v2_t *stats, bool get_stats);
extern int wl_cfgtwt_get_range(struct bcm_cfg80211 *cfg, struct wireless_dev *wdev,
	wl_twt_range_t *range_result);
extern s32 wl_cfgtwt_notify_event(struct bcm_cfg80211 *cfg,
	bcm_struct_cfgdev *cfgdev, const wl_event_msg_t *e, void *data);

#define TWT_DBG_ENTER() {WL_DBG(("Enter\n")); }
#define TWT_DBG_EXIT() {WL_DBG(("Exit\n")); }
#endif /* WL_TWT_HAL_IF */
#endif  /* _wl_cfgtwt_h_ */
