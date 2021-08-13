
#ifndef _wl_iapsta_
#define _wl_iapsta_
typedef enum IFMODE {
	ISTA_MODE = 1,
	IAP_MODE,
	IGO_MODE,
	IGC_MODE,
	IMESH_MODE
} ifmode_t;

enum wl_if_list {
	IF_PIF,
	IF_VIF,
	IF_VIF2,
	MAX_IF_NUM
};

typedef enum WL_PRIO {
	PRIO_AP,
	PRIO_MESH,
	PRIO_STA
} wl_prio_t;

typedef enum APSTAMODE {
	IUNKNOWN_MODE = 0,
	ISTAONLY_MODE = 1,
	IAPONLY_MODE = 2,
	ISTAAP_MODE = 3,
	ISTAGO_MODE = 4,
	ISTASTA_MODE = 5,
	IDUALAP_MODE = 6,
	ISTAAPAP_MODE = 7,
	IMESHONLY_MODE = 8,
	ISTAMESH_MODE = 9,
	IMESHAP_MODE = 10,
	ISTAAPMESH_MODE = 11,
	IMESHAPAP_MODE = 12
} apstamode_t;

typedef enum BGNMODE {
	IEEE80211B = 1,
	IEEE80211G,
	IEEE80211BG,
	IEEE80211BGN,
	IEEE80211BGNAC
} bgnmode_t;

typedef enum AUTHMODE {
	AUTH_OPEN,
	AUTH_SHARED,
	AUTH_WPAPSK,
	AUTH_WPA2PSK,
	AUTH_WPAWPA2PSK,
	AUTH_SAE
} authmode_t;

typedef enum ENCMODE {
	ENC_NONE,
	ENC_WEP,
	ENC_TKIP,
	ENC_AES,
	ENC_TKIPAES
} encmode_t;

enum wl_ext_status {
	WL_EXT_STATUS_DISCONNECTING = 0,
	WL_EXT_STATUS_DISCONNECTED,
	WL_EXT_STATUS_SCAN,
	WL_EXT_STATUS_SCANNING,
	WL_EXT_STATUS_CONNECTING,
	WL_EXT_STATUS_CONNECTED,
	WL_EXT_STATUS_ADD_KEY,
	WL_EXT_STATUS_AP_ENABLED,
	WL_EXT_STATUS_DELETE_STA,
	WL_EXT_STATUS_STA_DISCONNECTED,
	WL_EXT_STATUS_STA_CONNECTED,
	WL_EXT_STATUS_AP_DISABLED
};

typedef struct wl_if_info {
	struct net_device *dev;
	ifmode_t ifmode;
	unsigned long status;
	char prefix;
	wl_prio_t prio;
	int ifidx;
	uint8 bssidx;
	char ifname[IFNAMSIZ+1];
	char ssid[DOT11_MAX_SSID_LEN];
	struct ether_addr bssid;
	bgnmode_t bgnmode;
	int hidden;
	int maxassoc;
	uint16 channel;
	authmode_t amode;
	encmode_t emode;
	char key[100];
#ifdef WL_ESCAN
#if (defined(WLMESH) || defined(ACS_MONITOR))
	struct wl_escan_info *escan;
#ifdef WLMESH
	timer_list_compat_t delay_scan;
#endif /* WLMESH */
#ifdef ACS_MONITOR
	timer_list_compat_t acs_timer;
#endif /* ACS_MONITOR */
#endif /* WLMESH || ACS_MONITOR */
#endif /* WL_ESCAN */
	struct delayed_work pm_enable_work;
	struct mutex pm_sync;
#ifdef PROPTX_MAXCOUNT
	int transit_maxcount;
#endif /* PROPTX_MAXCOUNT */
	uint eapol_status;
	uint16 prev_channel;
	uint16 post_channel;
#ifdef TPUT_MONITOR
	unsigned long last_tx;
	unsigned long last_rx;
	struct osl_timespec tput_ts;
	int32 tput_tx;
	int32 tput_rx;
	int32 tput_tx_kb;
	int32 tput_rx_kb;
#endif /* TPUT_MONITOR */
} wl_if_info_t;

typedef struct wl_apsta_params {
	struct wl_if_info if_info[MAX_IF_NUM];
	struct dhd_pub *dhd;
	int ioctl_ver;
	bool init;
	int rsdb;
	bool vsdb;
	uint csa;
	uint acs;
#ifdef ACS_MONITOR
	uint acs_tmo;
#endif /* ACS_MONITOR */
	bool radar;
	apstamode_t apstamode;
	wait_queue_head_t netif_change_event;
	struct mutex usr_sync;
#if defined(WLMESH) && defined(WL_ESCAN)
	int macs;
	struct wl_mesh_params mesh_info;
#endif /* WLMESH && WL_ESCAN */
	struct mutex in4way_sync;
	uint sta_handshaking;
	int sta_btc_mode;
	struct osl_timespec sta_disc_ts;
	bool ap_recon_sta;
	wait_queue_head_t ap_recon_sta_event;
	struct ether_addr ap_disc_sta_bssid;
	struct osl_timespec ap_disc_sta_ts;
#ifdef TPUT_MONITOR
	timer_list_compat_t monitor_timer;
	int32 tput_sum;
	int32 tput_sum_kb;
#endif /* TPUT_MONITOR */
#ifdef SCAN_SUPPRESS
	struct osl_timespec scan_busy_ts;
	int scan_busy_cnt;
#endif /* SCAN_SUPPRESS */
} wl_apsta_params_t;

extern int op_mode;
void wl_ext_update_eapol_status(dhd_pub_t *dhd, int ifidx,
	uint eapol_status);
void wl_ext_iapsta_get_vif_macaddr(struct dhd_pub *dhd, int ifidx, u8 *mac_addr);
int wl_ext_iapsta_attach_netdev(struct net_device *net, int ifidx, uint8 bssidx);
int wl_ext_iapsta_attach_name(struct net_device *net, int ifidx);
int wl_ext_iapsta_dettach_netdev(struct net_device *net, int ifidx);
int wl_ext_iapsta_update_net_device(struct net_device *net, int ifidx);
int wl_ext_iapsta_alive_preinit(struct net_device *dev);
int wl_ext_iapsta_alive_postinit(struct net_device *dev);
int wl_ext_iapsta_attach(struct net_device *net);
void wl_ext_iapsta_dettach(struct net_device *net);
int wl_ext_iapsta_enable(struct net_device *dev, char *command, int total_len);
int wl_ext_iapsta_disable(struct net_device *dev, char *command, int total_len);
int wl_ext_isam_param(struct net_device *dev, char *command, int total_len);
int wl_ext_isam_status(struct net_device *dev, char *command, int total_len);
int wl_ext_isam_init(struct net_device *dev, char *command, int total_len);
int wl_ext_iapsta_config(struct net_device *dev, char *command, int total_len);
void wl_ext_add_remove_pm_enable_work(struct net_device *dev, bool add);
bool wl_ext_iapsta_other_if_enabled(struct net_device *net);
#ifdef WL_CFG80211
int wl_ext_in4way_sync(struct net_device *dev, uint action,
	enum wl_ext_status status, void *context);
void wl_ext_update_extsae_4way(struct net_device *dev,
	const struct ieee80211_mgmt *mgmt, bool tx);
#endif /* WL_CFG80211 */
#ifdef USE_IW
int wl_ext_in4way_sync_wext(struct net_device *dev, uint action,
	enum wl_ext_status status, void *context);
#endif /* USE_IW */
#ifdef WLMESH
int wl_ext_mesh_peer_status(struct net_device *dev, char *data, char *command,
	int total_len);
int wl_ext_isam_peer_path(struct net_device *dev, char *command, int total_len);
#endif
#ifdef WL_CFG80211
u32 wl_ext_iapsta_update_channel(dhd_pub_t *dhd, struct net_device *dev, u32 channel);
void wl_ext_iapsta_update_iftype(struct net_device *net, int ifidx, int wl_iftype);
bool wl_ext_iapsta_iftype_enabled(struct net_device *net, int wl_iftype);
void wl_ext_iapsta_enable_master_if(struct net_device *dev, bool post);
void wl_ext_iapsta_restart_master(struct net_device *dev);
void wl_ext_iapsta_ifadding(struct net_device *net, int ifidx);
bool wl_ext_iapsta_mesh_creating(struct net_device *net);
#ifdef SCAN_SUPPRESS
void wl_ext_populate_scan_channel(dhd_pub_t *dhd, u16 *channel_list,
	u32 channel, u32 n_channels);
uint16 wl_ext_scan_suppress(struct net_device *dev, void *scan_params, bool scan_v2);
#endif /* SCAN_SUPPRESS */
#endif
#ifdef PROPTX_MAXCOUNT
int wl_ext_get_wlfc_maxcount(struct dhd_pub *dhd, int ifidx);
#endif /* PROPTX_MAXCOUNT */
#ifdef TPUT_MONITOR
void wl_tput_monitor_set_timer(dhd_pub_t *dhd, uint sec, uint msec);
#endif /* TPUT_MONITOR */
#endif

