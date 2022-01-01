/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef MESON_CONNECTOR_DEV_H_
#define MESON_CONNECTOR_DEV_H_
#include <drm/drm_connector.h>
#include <linux/amlogic/media/vout/vout_notify.h>

enum {
	MESON_DRM_CONNECTOR_V10 = 0,
};

struct meson_connector_dev {
	int ver;

	/*copy from vout_op_s*/
	struct vinfo_s *(*get_vinfo)(void *data);
	int (*set_vmode)(enum vmode_e vmode, void *data);
	enum vmode_e (*validate_vmode)(char *name, unsigned int frac,
				       void *data);
	int (*check_same_vmodeattr)(char *name, void *data);
	int (*vmode_is_supported)(enum vmode_e vmode, void *data);
	int (*disable)(enum vmode_e vmode, void *data);
	int (*set_state)(int state, void *data);
	int (*clr_state)(int state, void *data);
	int (*get_state)(void *data);
	int (*get_disp_cap)(char *buf, void *data);
	int (*set_vframe_rate_hint)(int policy, void *data);
	int (*get_vframe_rate_hint)(void *data);
	void (*set_bist)(unsigned int num, void *data);
	int (*vout_suspend)(void *data);
	int (*vout_resume)(void *data);
	int (*vout_shutdown)(void *data);
};

/*hdmitx specified struct*/
/*hdcp*/
enum {
	HDCP_NULL = 0,
	HDCP_MODE14 = 1 << 0,
	HDCP_MODE22 = 1 << 1,
	HDCP_KEY_UPDATE = 1 << 2
};

enum {
	HDCP_AUTH_FAIL = 0,
	HDCP_AUTH_OK = 1,
	HDCP_AUTH_UNKNOWN = 0xff,
};

struct connector_hpd_cb {
	void (*callback)(void *data);
	void *data;
};

struct connector_hdcp_cb {
	void (*hdcp_notify)(void *data, int type, int auth_result);
	void *data;
};

struct meson_hdmitx_dev {
	struct meson_connector_dev base;
	/*add hdmitx specified function pointer and struct.*/

	int (*detect)(void);
	int (*register_hpd_cb)(struct connector_hpd_cb *hpd_cb);

	unsigned char *(*get_raw_edid)(void);
	int (*get_vic_list)(int **vics);

	unsigned int (*get_content_types)(void);
	int (*set_content_type)(int content_type);

	void (*setup_attr)(const char *buf);
	void (*get_attr)(char attr[16]);
	bool (*test_attr)(char *mode, char *attr);

	const struct dv_info *(*get_dv_info)(void);
	const struct hdr_info *(*get_hdr_info)(void);
	int (*get_hdr_priority)(void);

	void (*avmute)(unsigned char mute);
	void (*set_phy)(unsigned char en);

	/*hdcp apis*/
	void (*hdcp_init)(void);
	void (*hdcp_exit)(void);
	void (*hdcp_enable)(int hdcp_type);
	void (*hdcp_disable)(void);
	void (*hdcp_disconnect)(void);

	unsigned int (*get_tx_hdcp_cap)(void);
	unsigned int (*get_rx_hdcp_cap)(void);
	void (*register_hdcp_notify)(struct connector_hdcp_cb *cb);
};

#define to_meson_hdmitx_dev(x)	container_of(x, struct meson_hdmitx_dev, base)

/*hdmitx specified struct end.*/

/*cvbs specified struct*/
struct meson_cvbs_dev {
	struct meson_connector_dev base;
};

/*cvbs specified struct*/

/*lcd specified struct*/
struct meson_lcd_dev {
	struct meson_connector_dev base;
};

/*lcd specified struct*/

#endif
