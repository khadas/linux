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
#include <bcmutils.h>
#include <bcmendian.h>
#include <bcmwifi_channels.h>
#include <bcmiov.h>
#include <net/rtnetlink.h>

#include <wl_cfg80211.h>
#include <wl_cfgscan.h>
#include <wl_android.h>
#include <wl_cfgtwt.h>

#if defined(BCMDONGLEHOST)
#include <dngl_stats.h>
#include <dhd.h>
#endif /* BCMDONGLEHOST */
#include <wl_cfgvendor.h>
#include <bcmbloom.h>
#include <wl_cfgp2p.h>
#include <wl_cfgvif.h>
#ifdef RTT_SUPPORT
#include <dhd_rtt.h>
#endif /* RTT_SUPPORT */
#include <bcmstdlib_s.h>

#ifdef WL_TWT_HAL_IF
int wl_cfgtwt_cmd_reply(struct wiphy *wiphy, twt_hal_resp_t *twt_hal_resp)
{
	int err = 0;
	struct sk_buff *skb;
	int mem_needed;
	struct nlattr *twt_cap_nl_hdr;

	mem_needed = VENDOR_REPLY_OVERHEAD + ATTRIBUTE_U32_LEN + ATTRIBUTE_U16_LEN +
		sizeof(wifi_twt_capabilities) * VENDOR_DATA_OVERHEAD;

	/* Alloc the SKB for vendor_event */
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, mem_needed);
	if (unlikely(!skb)) {
		WL_ERR(("skb alloc failed"));
		err = BCME_NOMEM;
		goto exit;
	}

	err = nla_put_s8(skb, ANDR_TWT_WIFI_ERROR, twt_hal_resp->wifi_error);
	if (unlikely(err)) {
		WL_ERR(("Failed to put error_code\n"));
		goto exit;
	}

	if (twt_hal_resp->twt_cap) {
		twt_cap_nl_hdr = nla_nest_start(skb, ANDR_TWT_ATTR_CAP);
		if (!twt_cap_nl_hdr) {
			WL_ERR(("twt_cap_nl_hdr is NULL\n"));
			err = BCME_NOMEM;
			goto exit;
		}

		err = nla_put_u8(skb, ANDR_TWT_IS_REQUESTOR_SUPPORTED,
				twt_hal_resp->twt_cap->is_twt_requester_supported);
		if (unlikely(err)) {
			WL_ERR(("Failed to put is_req_support\n"));
			goto exit;
		}

		err = nla_put_u8(skb, ANDR_TWT_IS_RESPONDER_SUPPORTED,
				twt_hal_resp->twt_cap->is_twt_responder_supported);
		if (unlikely(err)) {
			WL_ERR(("Failed to put is_resp_support\n"));
			goto exit;
		}
		err = nla_put_u8(skb, ANDR_TWT_IS_BROADCAST_SUPPORTED,
				twt_hal_resp->twt_cap->is_broadcast_twt_supported);
		if (unlikely(err)) {
			WL_ERR(("Failed to put is_broadcast_support\n"));
			goto exit;
		}
		err = nla_put_u8(skb, ANDR_TWT_IS_FLEXIBLE_SUPPORTED,
				twt_hal_resp->twt_cap->is_flexible_twt_supported);
		if (unlikely(err)) {
			WL_ERR(("Failed to put is_flex_support\n"));
			goto exit;
		}
		err = nla_put_u32(skb, ANDR_TWT_ATTR_WAKE_DURATION_MIN,
				twt_hal_resp->twt_cap->min_wake_duration_micros);
		if (unlikely(err)) {
			WL_ERR(("Failed to put min_wake_dur\n"));
			goto exit;
		}
		err = nla_put_u32(skb, ANDR_TWT_ATTR_WAKE_DURATION_MAX,
				twt_hal_resp->twt_cap->max_wake_duration_micros);
		if (unlikely(err)) {
			WL_ERR(("Failed to put max_wake_dur\n"));
			goto exit;
		}
		err = nla_put_u32(skb, ANDR_TWT_ATTR_WAKE_INTERVAL_MIN,
				twt_hal_resp->twt_cap->min_wake_interval_micros);
		if (unlikely(err)) {
			WL_ERR(("Failed to put min_wake_int\n"));
			goto exit;
		}
		err = nla_put_u32(skb, ANDR_TWT_ATTR_WAKE_INTERVAL_MAX,
				twt_hal_resp->twt_cap->max_wake_interval_micros);
		if (unlikely(err)) {
			WL_ERR(("Failed to put max_wake_int\n"));
			goto exit;
		}

		nla_nest_end(skb, twt_cap_nl_hdr);
	}

	err =  cfg80211_vendor_cmd_reply(skb);
	if (unlikely(err)) {
		WL_ERR(("Vendor Command reply failed ret:%d \n", err));
	}

	return err;

exit:
	if (unlikely(err) && skb) {
		kfree_skb(skb);
	}

	return err;
}

wifi_error_t wl_cfgtwt_brcm_to_hal_error_code(int ret)
{
	wifi_error_t error_code;
	switch (ret) {
	case BCME_OK:
		error_code = WIFI_SUCCESS;
		break;
	case BCME_NOTAP:
	case BCME_NOTUP:
	case BCME_NOTENABLED:
	case BCME_NOTASSOCIATED:
		error_code = WIFI_ERROR_UNINITIALIZED;
		break;
	case BCME_BADLEN:
	case BCME_BADBAND:
	case BCME_USAGE_ERROR:
	case BCME_BADARG:
	case BCME_BUFTOOSHORT:
	case BCME_RANGE:
		error_code = WIFI_ERROR_INVALID_ARGS;
		break;
	case BCME_NOTFOUND:
		error_code = WIFI_ERROR_NOT_AVAILABLE;
		break;
	case BCME_BUSY:
	case BCME_NOTREADY:
		error_code = WIFI_ERROR_BUSY;
		break;
	case BCME_NOMEM:
	case BCME_NORESOURCE:
		error_code = WIFI_ERROR_OUT_OF_MEMORY;
		break;
	case BCME_IN_PROGRESS:
	case BCME_ACTIVE:
		error_code = WIFI_ERROR_TOO_MANY_REQUESTS;
		break;
	case BCME_UNSUPPORTED:
	case BCME_EPERM:
		error_code = WIFI_ERROR_NOT_SUPPORTED;
		break;
	default:
		WL_ERR(("%s Unmapped error code, error = %d\n",
				__func__, ret));
		/* Generic error */
		error_code = WIFI_ERROR_UNKNOWN;
	}

	return error_code;
}

int
wl_cfgtwt_get_range(struct bcm_cfg80211 *cfg, struct wireless_dev *wdev,
	wl_twt_range_t *range_result)
{
	int ret = BCME_OK;
	char iovbuf[WLC_IOCTL_SMLEN] = {0, };
	uint8 *pxtlv = NULL;
	uint8 *iovresp = NULL;
	wl_twt_range_cmd_t cmd_range;
	uint16 buflen = 0, bufstart = 0;

	bzero(&cmd_range, sizeof(cmd_range));

	cmd_range.version = WL_TWT_RANGE_CMD_VERSION_1;
	cmd_range.length = sizeof(cmd_range) - OFFSETOF(wl_twt_range_cmd_t, peer);

	iovresp = (uint8 *)MALLOCZ(cfg->osh, WLC_IOCTL_MEDLEN);
	if (iovresp == NULL) {
		WL_ERR(("%s: iov resp memory alloc exited\n", __FUNCTION__));
		ret = BCME_NOMEM;
		goto exit;
	}

	buflen = bufstart = WLC_IOCTL_SMLEN;
	pxtlv = (uint8 *)iovbuf;

	ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_TWT_CMD_RANGE,
			sizeof(cmd_range), (uint8 *)&cmd_range, BCM_XTLV_OPTION_ALIGN32);
	if (ret != BCME_OK) {
		WL_ERR(("%s : Error during pack xtlv :%d\n", __FUNCTION__, ret));
		goto exit;
	}

	ret = wldev_iovar_getbuf(wdev_to_ndev(wdev), "twt", iovbuf, bufstart-buflen,
			iovresp, WLC_IOCTL_MEDLEN, NULL);
	if (ret) {
		WL_ERR(("Getting twt range failed with err=%d \n", ret));
		goto exit;
	}

	if (sizeof(*range_result) > WLC_IOCTL_MEDLEN) {
		WL_ERR(("range result buffer len (%ld) > alloc buffer\n", sizeof(*range_result)));
		ret = BCME_BADLEN;
		goto exit;
	}

	ret = memcpy_s(range_result, sizeof(*range_result), iovresp, sizeof(*range_result));
	if (ret) {
		WL_ERR(("Failed to copy range result: %d\n", ret));
		goto exit;
	}

	if (dtoh16(range_result->version) == WL_TWT_RANGE_CMD_VERSION_1) {
		WL_DBG_MEM(("range ver %d, \n", dtoh16(range_result->version)));
	} else {
		ret = BCME_UNSUPPORTED;
		WL_ERR(("Unsupported iovar version:%d\n", dtoh16(range_result->version)));
		goto exit;
	}

exit:
	if (iovresp) {
		MFREE(cfg->osh, iovresp, WLC_IOCTL_MEDLEN);
	}

	return ret;
}

int
wl_cfgtwt_cap(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int ret = BCME_OK;
	int cmd_reply = BCME_OK;
	char iovbuf[WLC_IOCTL_SMLEN] = {0, };
	uint8 *pxtlv = NULL;
	uint8 *iovresp = NULL;
	wl_twt_cap_cmd_t cmd_cap;
	wl_twt_cap_t result;
	uint16 buflen = 0, bufstart = 0;
	struct bcm_cfg80211 *cfg = wl_get_cfg(wdev_to_ndev(wdev));
	twt_hal_resp_t twt_hal_resp;
	wifi_twt_capabilities twt_cap;
	wl_twt_range_t range;

	bzero(&cmd_cap, sizeof(cmd_cap));
	bzero(&twt_cap, sizeof(twt_cap));
	bzero(&twt_hal_resp, sizeof(twt_hal_resp));
	bzero(&range, sizeof(wl_twt_range_t));

	TWT_DBG_ENTER();
	cmd_cap.version = WL_TWT_CAP_CMD_VERSION_1;
	cmd_cap.length = sizeof(cmd_cap) - OFFSETOF(wl_twt_cap_cmd_t, peer);

	iovresp = (uint8 *)MALLOCZ(cfg->osh, WLC_IOCTL_MEDLEN);
	if (iovresp == NULL) {
		WL_ERR(("%s: iov resp memory alloc exited\n", __FUNCTION__));
		ret = BCME_NOMEM;
		goto exit;
	}

	buflen = bufstart = WLC_IOCTL_SMLEN;
	pxtlv = (uint8 *)iovbuf;

	ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_TWT_CMD_CAP,
			sizeof(cmd_cap), (uint8 *)&cmd_cap, BCM_XTLV_OPTION_ALIGN32);
	if (ret != BCME_OK) {
		WL_ERR(("%s : Error during pack xtlv :%d\n", __FUNCTION__, ret));
		goto exit;
	}

	ret = wldev_iovar_getbuf(wdev_to_ndev(wdev), "twt", iovbuf, bufstart-buflen,
			iovresp, WLC_IOCTL_MEDLEN, NULL);
	if (ret) {
		WL_ERR(("Getting twt cap failed with err=%d \n", ret));
		goto exit;
	}

	if (sizeof(result) > sizeof(iovresp)) {
		WL_ERR(("twt result buffer len (%ld) > alloced buffer\n", sizeof(result)));
		ret = BCME_BADLEN;
		goto exit;
	}

	ret = memcpy_s(&result, sizeof(result), iovresp, sizeof(result));
	if (ret) {
		WL_ERR(("Failed to copy result: %d\n", ret));
		goto exit;
	}

	if (dtoh16(result.version) == WL_TWT_CAP_CMD_VERSION_1) {
		WL_DBG_MEM(("capability ver %d, \n", dtoh16(result.version)));
		twt_cap.is_twt_requester_supported =
			result.device_cap & WL_TWT_CAP_FLAGS_REQ_SUPPORT;
		twt_cap.is_twt_responder_supported =
			result.device_cap & WL_TWT_CAP_FLAGS_RESP_SUPPORT;
		twt_cap.is_broadcast_twt_supported =
			result.device_cap & WL_TWT_CAP_FLAGS_BTWT_SUPPORT;
		twt_cap.is_flexible_twt_supported =
			result.device_cap & WL_TWT_CAP_FLAGS_FLEX_SUPPORT;

		ret = wl_cfgtwt_get_range(cfg, wdev, &range);
		if (ret < 0) {
			WL_ERR(("Failed to get the wake range values, ret:%d\n", ret));
			goto exit;
		}

		twt_cap.min_wake_duration_micros = range.wake_dur_min;
		twt_cap.max_wake_duration_micros = range.wake_dur_max;
		twt_cap.min_wake_interval_micros = range.wake_int_min;
		twt_cap.max_wake_interval_micros = range.wake_int_max;
	} else {
		ret = BCME_UNSUPPORTED;
		WL_ERR(("Version 1 unsupported. ver %d, \n", dtoh16(result.version)));
		goto exit;
	}

exit:
	if (iovresp) {
		MFREE(cfg->osh, iovresp, WLC_IOCTL_MEDLEN);
	}

	twt_hal_resp.wifi_error = wl_cfgtwt_brcm_to_hal_error_code(ret);
	twt_hal_resp.twt_cap = &twt_cap;
	cmd_reply = wl_cfgtwt_cmd_reply(wiphy, &twt_hal_resp);
	TWT_DBG_EXIT();
	return OSL_ERROR((ret == 0) ? cmd_reply : ret);
}

int
wl_cfgtwt_session_setup_update(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	wl_twt_config_t val;
	s32 ret, cmd_reply;
	s32 type, rem_attr;
	u8 mybuf[WLC_IOCTL_SMLEN] = {0};
	u8 resp_buf[WLC_IOCTL_SMLEN] = {0};
	const struct nlattr *iter;
	uint8 *rem = mybuf;
	uint16 rem_len = sizeof(mybuf);
	uint8 mlo_link_id = 0;
	u8 link_idx = NON_ML_LINK;
	struct net_device *inet_ndev = wdev_to_ndev(wdev);
	struct bcm_cfg80211 *cfg = wl_get_cfg(wdev_to_ndev(wdev));
#ifdef WL_MLO
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)) || defined(WL_MLO_BKPORT)
	struct net_info *netinfo = NULL;
	wl_mlo_link_t *linkinfo = NULL;
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)) || defined(WL_MLO_BKPORT) */
#endif /* WL_MLO */
	twt_hal_resp_t twt_hal_resp;
	wl_twt_range_t range;

	TWT_DBG_ENTER();

	bzero(&val, sizeof(val));
	bzero(&twt_hal_resp, sizeof(twt_hal_resp));
	bzero(&range, sizeof(wl_twt_range_t));

	val.version = WL_TWT_SETUP_VER;
	val.length = sizeof(val) - (sizeof(val.version) + sizeof(val.length));

	val.desc.configID = WL_TWT_CONFIG_ID_AUTO;
	val.desc.negotiation_type = WIFI_TWT_NEGO_TYPE_INDIVIDUAL;
	val.desc.avg_pkt_num  = 0xFFFFFFFF;
	val.desc.avg_pkt_size = 0xFFFFFFFF;

	nla_for_each_attr(iter, data, len, rem_attr) {
		type = nla_type(iter);
		switch (type) {
		case ANDR_TWT_ATTR_WAKE_INTERVAL_MIN:
			/* Minimum allowed Wake interval */
			val.desc.wake_int_min = nla_get_u32(iter);
			break;
		case ANDR_TWT_ATTR_WAKE_INTERVAL_MAX:
			/* Max Allowed Wake interval */
			val.desc.wake_int_max = nla_get_u32(iter);
			break;
		case ANDR_TWT_ATTR_WAKE_DURATION_MIN:
			/* Minimum allowed Wake duration */
			val.desc.wake_dur_min = nla_get_u32(iter);
			break;
		case ANDR_TWT_ATTR_WAKE_DURATION_MAX:
			/* Maximum allowed Wake duration */
			val.desc.wake_dur_max = nla_get_u32(iter);
			break;
		case ANDR_TWT_ATTR_MLO_LINK_ID:
			/* mlo link id */
			mlo_link_id = nla_get_s8(iter);
			break;
		case ANDR_TWT_ATTR_SESSION_ID:
			/* all twt */
			val.desc.configID = nla_get_u32(iter);
			break;
		default:
			WL_ERR(("Invalid setup attribute type %d\n", type));
			ret = BCME_BADARG;
			goto fail;
		}
	}

	if (!wl_get_drv_status(cfg, CONNECTED, inet_ndev)) {
		WL_ERR(("Sta is not connected to an AP!\n"));
		ret = BCME_UNSUPPORTED;
		goto fail;
	}

	/*
	 * Validate incoming wake dur and wake interval
	 * range against already fw configured range values
	 */
	ret = wl_cfgtwt_get_range(cfg, wdev, &range);
	if (ret < 0) {
		WL_ERR(("Failed to get the wake range values, ret:%d\n", ret));
		goto fail;
	}

	if (val.desc.wake_dur_min < range.wake_dur_min) {
		WL_ERR(("Invalid wake dur min value %d, expected to be greater than %d\n",
			val.desc.wake_dur_min, range.wake_dur_min));
		ret = BCME_BADARG;
		goto fail;
	}

	if (val.desc.wake_int_min < range.wake_int_min) {
		WL_ERR(("Invalid wake interval min value %d, expected to be greater than %d\n",
			val.desc.wake_int_min, range.wake_int_min));
		ret = BCME_BADARG;
		goto fail;
	}

	if (val.desc.wake_dur_max > range.wake_dur_max) {
		WL_ERR(("Invalid wake dur max value %d, expected to be lesser than %d\n",
			val.desc.wake_dur_max, range.wake_dur_max));
		ret = BCME_BADARG;
		goto fail;
	}

	if (val.desc.wake_int_max > range.wake_int_max) {
		WL_ERR(("Invalid wake interval max value %d, expected to be lesser than %d\n",
			val.desc.wake_int_max, range.wake_int_max));
		ret = BCME_BADARG;
		goto fail;
	}

	if (val.desc.wake_dur_min == val.desc.wake_dur_max) {
		/* Upper layer will set same value for max and min
		 * if it wants to use a specific value.
		 * so use value as is for those cases
		 */
		val.desc.wake_dur = val.desc.wake_dur_min;
	} else {
#ifdef TWT_USE_MAX_PWR_SAVE
		val.desc.wake_dur = val.desc.wake_dur_max;
#else
		val.desc.wake_dur = ((val.desc.wake_dur_min + val.desc.wake_dur_max)/2);
#endif /* TWT_USE_MAX_PWR_SAVE */
	}

	if (val.desc.wake_int_min == val.desc.wake_int_max) {
		/* Upper layer will set same value for max and min
		 * if it wants to use a specific value.
		 * so use value as is for those cases
		 */
		val.desc.wake_int = val.desc.wake_int_min;
	} else {
#ifdef TWT_USE_MAX_PWR_SAVE
		val.desc.wake_int = val.desc.wake_int_max;
#else
		val.desc.wake_int = ((val.desc.wake_int_min + val.desc.wake_int_max)/2);
#endif /* TWT_USE_MAX_PWR_SAVE */
	}

	ret = bcm_pack_xtlv_entry(&rem, &rem_len, WL_TWT_CMD_CONFIG,
			sizeof(val), (uint8 *)&val, BCM_XTLV_OPTION_ALIGN32);
	if (ret != BCME_OK) {
		WL_ERR(("twt config pack values failed. ret:%d\n", ret));
		goto fail;
	}

#ifdef WL_MLO
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)) || defined(WL_MLO_BKPORT)
	netinfo = wl_get_netinfo_by_netdev(cfg, inet_ndev);
	if (netinfo && netinfo->mlinfo.num_links) {
		linkinfo = wl_cfg80211_get_ml_linkinfo_by_linkid(cfg, netinfo, mlo_link_id);
		if (linkinfo && linkinfo->link_idx) {
			link_idx = linkinfo->link_idx;
			WL_DBG_MEM(("mlo_link_id %d based link_idx %d\n",
				mlo_link_id, link_idx));
		}
	}
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 0)) || defined(WL_MLO_BKPORT) */
#endif /* WL_MLO */

	ret = wldev_link_iovar_setbuf(inet_ndev, link_idx, "twt",
		mybuf, sizeof(mybuf) - rem_len, resp_buf, WLC_IOCTL_SMLEN, NULL);
	if (ret < 0) {
		if (val.desc.configID == WL_TWT_CONFIG_ID_AUTO) {
			WL_ERR(("twt config setup failed. ret:%d\n", ret));
		} else {
			WL_ERR(("twt update failed. session_id: %d ret:%d\n",
				val.desc.configID, ret));
		}
	} else {
		WL_DBG_MEM(("twt config setup succeeded, mlo_link_id %d config ID %d "
			"Negotiation type %d flow flags %d\n", mlo_link_id, val.desc.configID,
			val.desc.negotiation_type, val.desc.flow_flags));
	}

fail:
	twt_hal_resp.wifi_error = wl_cfgtwt_brcm_to_hal_error_code(ret);
	cmd_reply = wl_cfgtwt_cmd_reply(wiphy, &twt_hal_resp);
	TWT_DBG_EXIT();
	return OSL_ERROR((ret == 0) ? cmd_reply : ret);
}

int
wl_cfgtwt_session_teardown(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	wl_twt_teardown_t val;
	s32 ret = BCME_OK;
	int cmd_reply;
	s32 type, rem_attr;
	u8 mybuf[WLC_IOCTL_SMLEN] = {0};
	u8 res_buf[WLC_IOCTL_SMLEN] = {0};
	const struct nlattr *iter;
	uint8 *rem = mybuf;
	uint16 rem_len = sizeof(mybuf);
	twt_hal_resp_t twt_hal_resp;

	TWT_DBG_ENTER();

	bzero(&val, sizeof(val));
	bzero(&twt_hal_resp, sizeof(twt_hal_resp));

	val.version = WL_TWT_TEARDOWN_VER;
	val.length = sizeof(val) - (sizeof(val.version) + sizeof(val.length));

	/* Default values, Override Below */
	val.teardesc.flow_id = WL_TWT_INV_FLOW_ID;
	val.teardesc.bid = WL_TWT_INV_BCAST_ID;

	nla_for_each_attr(iter, data, len, rem_attr) {
		type = nla_type(iter);
		switch (type) {
		case ANDR_TWT_ATTR_SESSION_ID:
			/* all twt */
			val.configID = nla_get_u32(iter);
			break;
		default:
			ret = BCME_BADARG;
			WL_ERR(("Invalid teardown attribute type %d\n", type));
			goto fail;
		}
	}

	val.teardesc.negotiation_type = WIFI_TWT_NEGO_TYPE_INDIVIDUAL;
	val.teardesc.alltwt = 0;

	ret = bcm_pack_xtlv_entry(&rem, &rem_len, WL_TWT_CMD_TEARDOWN,
		sizeof(val), (uint8 *)&val, BCM_XTLV_OPTION_ALIGN32);
	if (ret != BCME_OK) {
		WL_ERR(("twt config pack values failed. ret:%d\n", ret));
		goto fail;
	}

	ret = wldev_iovar_setbuf(wdev_to_ndev(wdev), "twt",
		mybuf, sizeof(mybuf) - rem_len, res_buf, WLC_IOCTL_SMLEN, NULL);
	if (ret < 0) {
		WL_ERR(("twt teardown failed. ret:%d\n", ret));
		goto fail;
	} else {
		WL_DBG_MEM(("twt teardown succeeded, config ID %d "
			"Negotiation type %d alltwt %d\n", val.configID,
			val.teardesc.negotiation_type, val.teardesc.alltwt));
	}

fail:
	twt_hal_resp.wifi_error = wl_cfgtwt_brcm_to_hal_error_code(ret);
	cmd_reply = wl_cfgtwt_cmd_reply(wiphy, &twt_hal_resp);
	TWT_DBG_EXIT();
	return OSL_ERROR((ret == 0) ? cmd_reply : ret);
}

static int
wl_cfgtwt_stats(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len, bool is_get_stats)
{
	wl_twt_stats_cmd_v1_t query;
	wl_twt_stats_v2_t stats_v2;
	s32 type, rem_attr;
	const struct nlattr *iter;
	int ret = BCME_OK;
	int cmd_reply;
	char iovbuf[WLC_IOCTL_SMLEN] = {0, };
	uint8 *pxtlv = NULL;
	uint8 *iovresp = NULL;
	uint16 buflen = 0, bufstart = 0;
	struct bcm_cfg80211 *cfg = wl_get_cfg(wdev_to_ndev(wdev));
	twt_hal_resp_t twt_hal_resp;

	TWT_DBG_ENTER();

	bzero(&query, sizeof(query));
	bzero(&twt_hal_resp, sizeof(twt_hal_resp));

	query.version = WL_TWT_STATS_CMD_VERSION_1;
	query.length = sizeof(query) - OFFSETOF(wl_twt_stats_cmd_v1_t, peer);

	/* Default values, Override Below */
	query.num_bid = WL_TWT_INV_BCAST_ID;
	query.num_fid = WL_TWT_INV_FLOW_ID;

	if (!is_get_stats) {
		query.flags |= WL_TWT_STATS_CMD_FLAGS_RESET;
	}

	nla_for_each_attr(iter, data, len, rem_attr) {
		type = nla_type(iter);
		switch (type) {
		case ANDR_TWT_ATTR_SESSION_ID:
			/* Session ID */
			query.configID = nla_get_u32(iter);
			break;
		default:
			WL_ERR(("Invalid setup attribute type %d\n", type));
			ret = BCME_BADARG;
			goto exit;
		}
	}

	iovresp = (uint8 *)MALLOCZ(cfg->osh, WLC_IOCTL_MEDLEN);
	if (iovresp == NULL) {
		WL_ERR(("%s: iov resp memory alloc exited\n", __FUNCTION__));
		ret = BCME_NOMEM;
		goto exit;
	}

	buflen = bufstart = WLC_IOCTL_SMLEN;
	pxtlv = (uint8 *)iovbuf;
	ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_TWT_CMD_STATS,
			sizeof(query), (uint8 *)&query, BCM_XTLV_OPTION_ALIGN32);
	if (ret != BCME_OK) {
		WL_ERR(("%s : Error return during pack xtlv :%d\n", __FUNCTION__, ret));
		goto exit;
	}

	ret = wldev_iovar_getbuf(wdev_to_ndev(wdev), "twt", iovbuf, bufstart-buflen,
			iovresp, WLC_IOCTL_MEDLEN, NULL);
	if (ret) {
		WL_ERR(("twt stats cmd failed with err=%d \n", ret));
		goto exit;
	}

	if (sizeof(stats_v2) > sizeof(iovresp)) {
		WL_ERR(("stats buffer len (%ld) > alloc buffer\n", sizeof(stats_v2)));
		ret = BCME_BADLEN;
		goto exit;
	}

	ret = memcpy_s(&stats_v2, sizeof(stats_v2), iovresp, sizeof(stats_v2));
	if (ret) {
		WL_ERR(("Failed to copy result: %d\n", ret));
		goto exit;
	}

	if (dtoh16(stats_v2.version) == WL_TWT_STATS_VERSION_2) {
		WL_DBG_MEM(("stats query ver %d, \n", dtoh16(stats_v2.version)));
		wl_cfgtwt_send_stats_event(wdev, cfg, (wl_twt_stats_v2_t *)iovresp, is_get_stats);
	} else {
		WL_ERR(("Unsupported iovar version:%d, \n", dtoh16(stats_v2.version)));
	}

exit:
	if (iovresp) {
		MFREE(cfg->osh, iovresp, WLC_IOCTL_MEDLEN);
	}

	twt_hal_resp.wifi_error = wl_cfgtwt_brcm_to_hal_error_code(ret);
	cmd_reply = wl_cfgtwt_cmd_reply(wiphy, &twt_hal_resp);
	TWT_DBG_EXIT();
	return OSL_ERROR((ret == 0) ? cmd_reply : ret);
}

int
wl_cfgtwt_session_get_stats(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	/* Results in asynchronous callback
	* wifi_twt_events.on_twt_session_stats on success or
	* wifi_twt_events.on_twt_failure with error code.
	*/
	return wl_cfgtwt_stats(wiphy, wdev, data, len, true);
}

int
wl_cfgtwt_session_clear_stats(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	/* Results in asynchronous callback
	* wifi_twt_events.on_twt_session_stats on success or
	* wifi_twt_events.on_twt_failure with error code.
	*/
	return wl_cfgtwt_stats(wiphy, wdev, data, len, false);
}

int
wl_cfgtwt_session_suspend(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int ret = BCME_OK;
	int cmd_reply;
	wl_twt_info_t val;
	s32 type, rem_attr;
	const struct nlattr *iter;
	u8 mybuf[WLC_IOCTL_SMLEN] = {0};
	u8 res_buf[WLC_IOCTL_SMLEN] = {0};
	uint8 *rem = mybuf;
	uint16 rem_len = sizeof(mybuf);
	twt_hal_resp_t twt_hal_resp;

	/* Send async event
	 *  Success : on_twt_session_suspend
	 *  data: session_id
	 *  Failure: OnTwtSessionFailure
	 *  wifi_twt_error_code error_code;
	 */
	TWT_DBG_ENTER();

	bzero(&val, sizeof(val));
	bzero(&twt_hal_resp, sizeof(twt_hal_resp));

	val.version = WL_TWT_INFO_VER;
	val.length = sizeof(val) - (sizeof(val.version) + sizeof(val.length));
	/* Default values, Override Below */
	val.infodesc.flow_id = 0xFF;

	nla_for_each_attr(iter, data, len, rem_attr) {
		type = nla_type(iter);
		switch (type) {
		case ANDR_TWT_ATTR_SESSION_ID:
			val.configID = nla_get_u32(iter);
			break;
		default:
			WL_ERR(("Invalid suspend attribute type %d\n", type));
			ret = BCME_BADARG;
			goto exit;
		}
	}

	val.infodesc.next_twt_h = val.infodesc.next_twt_l = 0;

	ret = bcm_pack_xtlv_entry(&rem, &rem_len, WL_TWT_CMD_INFO,
			sizeof(val), (uint8 *)&val, BCM_XTLV_OPTION_ALIGN32);
	if (ret != BCME_OK) {
		WL_ERR(("%s : Error during pack xtlv :%d\n", __FUNCTION__, ret));
		goto exit;
	}
	ret = wldev_iovar_setbuf(wdev_to_ndev(wdev), "twt",
			mybuf, sizeof(mybuf) - rem_len, res_buf, WLC_IOCTL_SMLEN, NULL);
	if (ret < 0) {
		WL_ERR(("twt suspend failed. ret:%d\n", ret));
	} else {
		WL_DBG_MEM(("twt suspend succeeded, session ID %d\n", val.configID));
	}

exit:
	twt_hal_resp.wifi_error = wl_cfgtwt_brcm_to_hal_error_code(ret);
	cmd_reply = wl_cfgtwt_cmd_reply(wiphy, &twt_hal_resp);
	TWT_DBG_EXIT();
	return OSL_ERROR((ret == 0) ? cmd_reply : ret);
}

int
wl_cfgtwt_session_resume(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int ret = BCME_OK;
	int cmd_reply;
	wl_twt_info_t val;
	s32 type, rem_attr;
	const struct nlattr *iter;
	u8 mybuf[WLC_IOCTL_SMLEN] = {0};
	u8 res_buf[WLC_IOCTL_SMLEN] = {0};
	uint8 *rem = mybuf;
	uint16 rem_len = sizeof(mybuf);
	twt_hal_resp_t twt_hal_resp;

	/* Send async event
	 *  Success : on_twt_session_resume
	 *  data: session_id
	 *  Failure: OnTwtSessionFailure
	 *  wifi_twt_error_code error_code;
	 */
	TWT_DBG_ENTER();

	bzero(&val, sizeof(val));
	bzero(&twt_hal_resp, sizeof(twt_hal_resp));

	val.version = WL_TWT_INFO_VER;
	val.length = sizeof(val) - (sizeof(val.version) + sizeof(val.length));
	/* Default values, Override Below */
	val.infodesc.flow_id = 0xFF;

	nla_for_each_attr(iter, data, len, rem_attr) {
		type = nla_type(iter);
		switch (type) {
		case ANDR_TWT_ATTR_SESSION_ID:
			val.configID = nla_get_u32(iter);
			break;
		default:
			WL_ERR(("Invalid suspend attribute type %d\n", type));
			ret = BCME_BADARG;
			goto exit;
		}
	}

	val.infodesc.next_twt_l = TWT_INFO_DESC_RESUME_TIME_IMMEDIATE;
	val.infodesc.next_twt_h = 0;
	val.infodesc.flow_flags |= WL_TWT_INFO_FLAG_RESUME;

	ret = bcm_pack_xtlv_entry(&rem, &rem_len, WL_TWT_CMD_INFO,
			sizeof(val), (uint8 *)&val, BCM_XTLV_OPTION_ALIGN32);
	if (ret != BCME_OK) {
		WL_ERR(("%s : Error during pack xtlv :%d\n", __FUNCTION__, ret));
		goto exit;
	}

	ret = wldev_iovar_setbuf(wdev_to_ndev(wdev), "twt",
			mybuf, sizeof(mybuf) - rem_len, res_buf, WLC_IOCTL_SMLEN, NULL);
	if (ret < 0) {
		WL_ERR(("twt resume failed. ret:%d\n", ret));
	} else {
		WL_DBG_MEM(("twt resume succeeded, session ID %d\n", val.configID));
	}

exit:
	twt_hal_resp.wifi_error = wl_cfgtwt_brcm_to_hal_error_code(ret);
	cmd_reply = wl_cfgtwt_cmd_reply(wiphy, &twt_hal_resp);
	TWT_DBG_EXIT();
	return OSL_ERROR((ret == 0) ? cmd_reply : ret);
}

extern void
wl_cfgtwt_send_stats_event(struct wireless_dev *wdev, struct bcm_cfg80211 *cfg,
	wl_twt_stats_v2_t *stats, bool get_stats)
{
	int ret = BCME_OK;
	struct wiphy *wiphy = wdev->wiphy;
	gfp_t kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	struct sk_buff *msg;
	wl_twt_peer_stats_v2_t *peer_stats = NULL;

	/* Allocate the skb for vendor event */
	msg = CFG80211_VENDOR_EVENT_ALLOC(wiphy,
			bcmcfg_to_prmry_wdev(cfg), BRCM_TWT_HAL_VENDOR_EVENT_BUF_LEN,
			BRCM_VENDOR_EVENT_TWT, kflags);
	if (!msg) {
		WL_ERR(("%s: fail to allocate skb for vendor event\n", __FUNCTION__));
		return;
	}

	/*
	 * As stats queried per session id,
	 * it is expected the fw will send the stats of the same session
	 */
	peer_stats = &stats->peer_stats_list[0];

	WL_DBG_MEM(("session_id: %d, eosp_dur_avg: %u, tx_pkt_avg: %u, rx_pkt_avg: %u,"
			"tx_pkt_sz_avg: %u, rx_pkt_sz_avg: %u, eosp_count %d\n",
			peer_stats->configID, peer_stats->eosp_dur_avg,
			peer_stats->tx_pkts_avg, peer_stats->rx_pkts_avg,
			peer_stats->tx_pkt_sz_avg, peer_stats->rx_pkt_sz_avg,
			peer_stats->eosp_count));

	ret = nla_put_u8(msg, ANDR_TWT_ATTR_SUB_EVENT, ANDR_TWT_SESSION_EVENT_STATS);
	if (unlikely(ret)) {
		WL_ERR(("nla_put_u8 WIFI_TWT_ATTR_SUB_EVENT failed\n"));
		goto fail;
	}

	ret = nla_put_u32(msg, ANDR_TWT_ATTR_SESSION_ID, peer_stats->configID);
	if (unlikely(ret)) {
		WL_ERR(("nla_put_u32 ANDR_TWT_ATTR_SESSION_ID failed\n"));
		goto fail;
	}

	ret = nla_put_u32(msg, ANDR_TWT_ATTR_AVG_PKT_NUM_TX, peer_stats->tx_pkts_avg);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put avg_pkt_num_tx, ret=%d\n", ret));
		goto fail;
	}

	ret = nla_put_u32(msg, ANDR_TWT_ATTR_AVG_PKT_NUM_RX, peer_stats->rx_pkts_avg);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put avg_pkt_num_rx, ret=%d\n", ret));
		goto fail;
	}

	ret = nla_put_u32(msg, ANDR_TWT_ATTR_AVG_PKT_SIZE_TX, peer_stats->tx_pkt_sz_avg);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put avg_tx_pkt_size, ret=%d\n", ret));
		goto fail;
	}

	ret = nla_put_u32(msg, ANDR_TWT_ATTR_AVG_PKT_SIZE_RX, peer_stats->rx_pkt_sz_avg);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put avg_rx_pkt_size, ret=%d\n", ret));
		goto fail;
	}

	ret = nla_put_u32(msg, ANDR_TWT_ATTR_AVG_EOSP_DUR, peer_stats->eosp_dur_avg);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put avg_eosp_dur_us, ret=%d\n", ret));
		goto fail;
	}

	ret = nla_put_u32(msg, ANDR_TWT_ATTR_EOSP_CNT, peer_stats->eosp_count);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put eosp_count, ret=%d\n", ret));
		goto fail;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
	cfg80211_vendor_event(msg, kflags);
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0) */

	WL_DBG_MEM(("Successfully sent TWT vendor event type %d\n", ANDR_TWT_SESSION_EVENT_STATS));
	TWT_DBG_EXIT();
	return;

fail:
	/* Free skb for failure cases */
	if (msg) {
		dev_kfree_skb_any(msg);
	}
	TWT_DBG_EXIT();
	return;
}

static int
wl_cfgtwt_update_setup_response(struct sk_buff *skb, void *event_data)
{
	s32 err = BCME_OK;
	const wl_twt_setup_cplt_t *setup_cplt = (wl_twt_setup_cplt_t *)event_data;
	const wl_twt_sdesc_v0_t *sdesc = (const wl_twt_sdesc_v0_t *)&setup_cplt[1];

	WL_DBG_MEM(("TWT_SETUP: status %d, reason %d, configID %d, mlo_link_id %d setup_cmd %d,"
		" flow_flags 0x%x, flow_id %d, channel %d, negotiation_type %d, "
		"wake_time_h %u, wake_time_l %u, wake_dur %u, wake_int %u, flow_flags2 0x%x,\n",
		(int)setup_cplt->status, (int)setup_cplt->reason_code, (int)setup_cplt->configID,
		(int)setup_cplt->mlo_link_id, (int)sdesc->setup_cmd, sdesc->flow_flags,
		(int)sdesc->flow_id, (int)sdesc->channel,
		(int)sdesc->negotiation_type, sdesc->wake_time_h, sdesc->wake_time_l,
		sdesc->wake_dur, sdesc->wake_int, sdesc->flow_flags2));

	if ((int)setup_cplt->status == BCME_OK) {
		if (!!(sdesc->flow_flags2 & WL_TWT_FLOW_FLAG2_PARAM_UPDATE)) {
			err = nla_put_u8(skb, ANDR_TWT_ATTR_SUB_EVENT,
				ANDR_TWT_SESSION_EVENT_UPDATE);
		} else {
			err = nla_put_u8(skb, ANDR_TWT_ATTR_SUB_EVENT,
				ANDR_TWT_SESSION_EVENT_CREATE);
		}
		if (unlikely(err)) {
			WL_ERR(("nla_put_u8 WIFI_TWT_ATTR_SUB_EVENT failed\n"));
			goto fail;
		}

		err = nla_put_u32(skb, ANDR_TWT_ATTR_SESSION_ID, setup_cplt->configID);
		if (unlikely(err)) {
			WL_ERR(("nla_put_u32 ANDR_TWT_ATTR_SESSION_ID failed\n"));
			goto fail;
		}

		err = nla_put_s8(skb, ANDR_TWT_ATTR_MLO_LINK_ID, setup_cplt->mlo_link_id);
		if (unlikely(err)) {
			WL_ERR(("nla_put_s8 ANDR_TWT_ATTR_MLO_LINK_ID failed\n"));
			goto fail;
		}

		err = nla_put_u32(skb, ANDR_TWT_ATTR_WAKE_DURATION, sdesc->wake_dur);
		if (unlikely(err)) {
			WL_ERR(("nla_put_u32 WIFI_TWT_ATTR_WAKE_DURATION failed\n"));
			goto fail;
		}

		err = nla_put_u32(skb, ANDR_TWT_ATTR_WAKE_INTERVAL, sdesc->wake_int);
		if (unlikely(err)) {
			WL_ERR(("nla_put_u32 WIFI_TWT_ATTR_WAKE_INTERVAL failed\n"));
			goto fail;
		}

		err = nla_put_u8(skb, ANDR_TWT_ATTR_NEG_TYPE, sdesc->negotiation_type);
		if (unlikely(err)) {
			WL_ERR(("nla_put_u8 ANDR_TWT_ATTR_NEG_TYPE failed\n"));
			goto fail;
		}

		err = nla_put_u8(skb, ANDR_TWT_ATTR_IS_TRIGGER_ENABLED,
				!!(sdesc->flow_flags & WL_TWT_FLOW_FLAG_TRIGGER));
		if (unlikely(err)) {
			WL_ERR(("nla_put_u8 ANDR_TWT_ATTR_IS_TRIGGER_ENABLED failed\n"));
			goto fail;
		}

		err = nla_put_u8(skb, ANDR_TWT_ATTR_IS_RESP_PM_MODE_ENABLED,
				!!(sdesc->flow_flags & WL_TWT_FLOW_FLAG_RESPONDER_PM));
		if (unlikely(err)) {
			WL_ERR(("Failed to put is_responder_pm_mode_enabled, ret=%d\n", err));
			goto fail;
		}

		err = nla_put_u8(skb, ANDR_TWT_ATTR_IS_ANNOUNCED,
				!!(sdesc->flow_flags & WL_TWT_FLOW_FLAG_UNANNOUNCED));
		if (unlikely(err)) {
			WL_ERR(("Failed to put is_announced, ret=%d\n", err));
			goto fail;
		}

		err = nla_put_u8(skb, ANDR_TWT_ATTR_IS_IMPLICIT,
				!!(sdesc->flow_flags2 & WL_TWT_FLOW_FLAG2_IMPLICIT));
		if (unlikely(err)) {
			WL_ERR(("Failed to put is_implicit, ret=%d\n", err));
			goto fail;
		}

		err = nla_put_u8(skb, ANDR_TWT_ATTR_IS_PROTECTED,
				!!(sdesc->flow_flags & WL_TWT_FLOW_FLAG_PROTECT));
		if (unlikely(err)) {
			WL_ERR(("Failed to put is_protected, ret=%d\n", err));
			goto fail;
		}

		err = nla_put_u8(skb, ANDR_TWT_ATTR_IS_UPDATABLE,
				!!(sdesc->flow_flags2 & WL_TWT_FLOW_FLAG2_UPDATABLE));
		if (unlikely(err)) {
			WL_ERR(("Failed to put updatable, ret=%d\n", err));
			goto fail;
		}

		err = nla_put_u8(skb, ANDR_TWT_ATTR_IS_SUSPENDABLE,
				!(sdesc->flow_flags & WL_TWT_FLOW_FLAG_INFO_FRM_DISABLED));
		if (unlikely(err)) {
			WL_ERR(("Failed to put suspendable, ret=%d\n", err));
			goto fail;
		}
	} else {
		err = nla_put_u8(skb, ANDR_TWT_ATTR_SUB_EVENT, ANDR_TWT_SESSION_EVENT_FAILURE);
		if (unlikely(err)) {
			WL_ERR(("nla_put_u8 WIFI_TWT_ATTR_SUB_EVENT failed\n"));
			goto fail;
		}

		err = nla_put_u8(skb, ANDR_TWT_ATTR_ERROR_CODE,
			wl_cfgtwt_map_fw_to_hal_error_code((int)setup_cplt->reason_code));
		if (unlikely(err)) {
			WL_ERR(("Failed to put error code: %d, err=%d\n",
				(int)setup_cplt->reason_code, err));
			goto fail;
		}
	}

fail:
	return err;
}

static wifi_twt_teardown_reason_code
wl_cfgtwt_map_fw_to_hal_reason_code(wl_twt_td_rc_t fw_reason_code)
{
	wifi_twt_teardown_reason_code reason_code;
	switch (fw_reason_code) {
	case WL_TWT_TD_RC_HOST:
	case WL_TWT_TD_RC_SCHED:
		reason_code = WIFI_TWT_TEARDOWN_REASON_CODE_LOCALLY_REQUESTED;
		break;
	case WL_TWT_TD_RC_PEER:
	case WL_TWT_TD_RC_TIMEOUT:
		reason_code = WIFI_TWT_TEARDOWN_REASON_CODE_PEER_INITIATED;
		break;
	case WL_TWT_TD_RC_MCHAN:
	case WL_TWT_TD_RC_MCNX:
	case WL_TWT_TD_RC_CSA:
	case WL_TWT_TD_RC_BTCX:
	case WL_TWT_TD_RC_SETUP_FAIL:
	case WL_TWT_TD_RC_PM_OFF:
		reason_code = WIFI_TWT_TEARDOWN_REASON_CODE_INTERNALLY_INITIATED;
		break;
	default:
		reason_code = WIFI_TWT_TEARDOWN_REASON_CODE_UNKNOWN;
	}
	return reason_code;
}

wifi_twt_error_code wl_cfgtwt_map_fw_to_hal_error_code(int fw_error_code)
{
	wifi_twt_error_code error_code;
	switch (fw_error_code) {
	case WL_TWT_TD_RC_TIMEOUT:
		error_code = WIFI_TWT_ERROR_CODE_TIMEOUT;
		break;
	case WL_TWT_TD_RC_SETUP_FAIL:
		error_code = WIFI_TWT_ERROR_CODE_INVALID_PARAMS;
		break;
	case WL_TWT_TD_RC_PEER:
		error_code = WIFI_TWT_ERROR_CODE_PEER_REJECTED;
		break;
	default:
		error_code = WIFI_TWT_ERROR_CODE_FAILURE_UNKNOWN;
	}
	return error_code;
}

static int
wl_cfgtwt_update_infoframe_response(struct sk_buff *skb, void *event_data)
{
	s32 err = BCME_OK;
	int status = 0;
	int suspend = 0;
	const wl_twt_info_cplt_t *info_cplt = (wl_twt_info_cplt_t *)event_data;
	const wl_twt_infodesc_t *infodesc = (const wl_twt_infodesc_t *)&info_cplt[1];

	WL_DBG_MEM(("TWT_INFOFRM: status %d, reason %d, configID %d, flow_flags 0x%x, flow_id %d,"
		" next_twt_h %u, next_twt_l %u\n", (int)info_cplt->status,
		(int)info_cplt->reason_code, (int)info_cplt->configID, infodesc->flow_flags,
		(int)infodesc->flow_id, infodesc->next_twt_h, infodesc->next_twt_l));

	status = !!(info_cplt->status);
	suspend = !(infodesc->flow_flags & WL_TWT_INFO_FLAG_RESUME);

	if (status == BCME_OK) {
		if (suspend) {
			err = nla_put_u8(skb, ANDR_TWT_ATTR_SUB_EVENT,
					ANDR_TWT_SESSION_EVENT_SUSPEND);
			if (unlikely(err)) {
				WL_ERR(("nla_put_u8 WIFI_TWT_ATTR_SUB_EVENT failed\n"));
				goto fail;
			}
		} else {
			err = nla_put_u8(skb, ANDR_TWT_ATTR_SUB_EVENT,
					ANDR_TWT_SESSION_EVENT_RESUME);
			if (unlikely(err)) {
				WL_ERR(("nla_put_u8 WIFI_TWT_ATTR_SUB_EVENT failed\n"));
				goto fail;
			}
		}

		err = nla_put_u32(skb, ANDR_TWT_ATTR_SESSION_ID, info_cplt->configID);
		if (unlikely(err)) {
			WL_ERR(("nla_put_u32 ANDR_TWT_ATTR_SESSION_ID failed\n"));
			goto fail;
		}
	} else {
		err = nla_put_u8(skb, ANDR_TWT_ATTR_SUB_EVENT, ANDR_TWT_SESSION_EVENT_FAILURE);
		if (unlikely(err)) {
			WL_ERR(("nla_put_u8 WIFI_TWT_ATTR_SUB_EVENT failed\n"));
			goto fail;
		}

		err = nla_put_u8(skb, ANDR_TWT_ATTR_ERROR_CODE,
				wl_cfgtwt_map_fw_to_hal_error_code((int)info_cplt->reason_code));
		if (unlikely(err)) {
			WL_ERR(("Failed to put reason_code: %d, err=%d\n",
				info_cplt->reason_code, err));
			goto fail;
		}
	}
fail:
	return err;
}

static int
wl_cfgtwt_update_teardown_response(struct sk_buff *skb, void *event_data)
{
	s32 err = BCME_OK;
	const wl_twt_teardown_cplt_t *td_cplt = (wl_twt_teardown_cplt_t *)event_data;
	const wl_twt_teardesc_t *teardesc = (const wl_twt_teardesc_t *)&td_cplt[1];

	if ((int)td_cplt->status == BCME_OK) {
		err = nla_put_u32(skb, ANDR_TWT_ATTR_SESSION_ID, td_cplt->configID);
		if (unlikely(err)) {
			WL_ERR(("Failed to put session id, err=%d\n", err));
			goto fail;
		}

		err = nla_put_u8(skb, ANDR_TWT_ATTR_REASON_CODE,
				wl_cfgtwt_map_fw_to_hal_reason_code(td_cplt->reason_code));
		if (unlikely(err)) {
			WL_ERR(("Failed to put reason_code: %d, err=%d\n",
				td_cplt->reason_code, err));
			goto fail;
		}

		err = nla_put_u8(skb, ANDR_TWT_ATTR_SUB_EVENT, ANDR_TWT_SESSION_EVENT_TEARDOWN);
		if (unlikely(err)) {
			WL_ERR(("nla_put_u8 ANDR_TWT_SESSION_EVENT_TEARDOWN failed\n"));
			goto fail;
		}
	} else {
		err = nla_put_u8(skb, ANDR_TWT_ATTR_SUB_EVENT, ANDR_TWT_SESSION_EVENT_FAILURE);
		if (unlikely(err)) {
			WL_ERR(("nla_put_u8 WIFI_TWT_ATTR_SUB_EVENT failed\n"));
			goto fail;
		}

		err = nla_put_u8(skb, ANDR_TWT_ATTR_ERROR_CODE,
				wl_cfgtwt_map_fw_to_hal_error_code((int)td_cplt->reason_code));
		if (unlikely(err)) {
			WL_ERR(("Failed to put error code: %d, err=%d\n",
					(int)td_cplt->reason_code, err));
			goto fail;
		}
	}

fail:
	WL_DBG_MEM(("TWT_TEARDOWN: err %d status %d, reason %d, configID %d,"
		" flow_id %d, negotiation_type %d, bid %d, alltwt %d\n",
		err, (int)td_cplt->status, (int)td_cplt->reason_code,
		(int)td_cplt->configID, (int)teardesc->flow_id, (int)teardesc->negotiation_type,
		(int)teardesc->bid, (int)teardesc->alltwt));
	return err;
}

/* Event handler for fw gen events of twt */
s32
wl_cfgtwt_notify_event(struct bcm_cfg80211 *cfg,
		bcm_struct_cfgdev *cfgdev, const wl_event_msg_t *e, void *data)
{
	struct sk_buff *skb = NULL;
	gfp_t kflags;
	struct wiphy *wiphy = bcmcfg_to_wiphy(cfg);
	int err = BCME_OK;
	struct net_device *ndev = bcmcfg_to_prmry_ndev(cfg);
	const wl_twt_event_t *twt_event = (wl_twt_event_t *)data;

	kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	skb = CFG80211_VENDOR_EVENT_ALLOC(wiphy, ndev_to_wdev(ndev),
		BRCM_TWT_HAL_VENDOR_EVENT_BUF_LEN, BRCM_VENDOR_EVENT_TWT, kflags);
	if (!skb) {
		WL_ERR(("skb alloc failed"));
		err = BCME_NOMEM;
		goto fail;
	}

	switch (twt_event->event_type) {
	case WL_TWT_EVENT_SETUP:
		err = wl_cfgtwt_update_setup_response(skb,
			(void *)twt_event->event_info);
		break;
	case WL_TWT_EVENT_TEARDOWN:
		err = wl_cfgtwt_update_teardown_response(skb,
			(void *)twt_event->event_info);
		break;
	case WL_TWT_EVENT_INFOFRM:
		err = wl_cfgtwt_update_infoframe_response(skb,
			(void *)twt_event->event_info);
		break;
	case WL_TWT_EVENT_NOTIFY:
		WL_DBG_MEM(("WL_TWT_EVENT_NOTIFY sub event type received\n"));
		break;
	default:
		WL_ERR(("Invalid TWT sub event type %d", twt_event->event_type));
		err = BCME_UNSUPPORTED;
		break;
	}

	if (err) {
		goto fail;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
	cfg80211_vendor_event(skb, kflags);
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0) */

	WL_DBG_MEM(("Successfully sent fw generated TWT vendor event type %d\n",
			twt_event->event_type));
	return BCME_OK;

fail:
	/* Free skb for failure cases */
	if (skb) {
		kfree_skb(skb);
	}

	return err;
}
#endif /* WL_TWT_HAL_IF */
