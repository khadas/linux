/*
 * DHD Linux header file (dhd_linux exports for cfg80211 and other components)
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

/* wifi platform functions for power, interrupt and pre-alloc, either
 * from Android-like platform device data, or Broadcom wifi platform
 * device data.
 *
 */
#ifndef __DHD_LINUX_H__
#define __DHD_LINUX_H__

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <dngl_stats.h>
#include <dhd.h>
#ifdef DHD_LOG_DUMP
#include <dhd_log_dump.h>
#endif
#ifdef DHD_WMF
#include <dhd_wmf_linux.h>
#endif
#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(DHD_USE_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif /* defined(CONFIG_HAS_EARLYSUSPEND) && defined(DHD_USE_EARLYSUSPEND) */

#ifdef BCMPCIE
#include <bcmmsgbuf.h>
#endif /* BCMPCIE */

#if defined(RTT_SUPPORT) && defined(WL_CFG80211)
#include <dhd_rtt.h>
#endif /* RTT_SUPPORT && WL_CFG80211 */

#ifdef PCIE_FULL_DONGLE
#include <etd.h>
#endif /* PCIE_FULL_DONGLE */

#ifdef WL_MONITOR
#ifdef HOST_RADIOTAP_CONV
#include <bcmwifi_monitor.h>
#else
#define MAX_RADIOTAP_SIZE      256 /* Maximum size to hold HE Radiotap header format */
/* SKB length to accommodate max AMSDU frame in monitor mode */
#define MAX_MON_PKT_SIZE       ((12 * 1024u) + MAX_RADIOTAP_SIZE)
#endif /* HOST_RADIOTAP_CONV */
#endif /* WL_MONITOR */

/* dongle status */
enum wifi_adapter_status {
	WIFI_STATUS_POWER_ON = 0,
	WIFI_STATUS_FW_READY,
	WIFI_STATUS_NET_ATTACHED,
	WIFI_STATUS_BUS_DISCONNECTED
};
#define wifi_chk_adapter_status(adapter, stat) (test_bit(stat, &(adapter)->status))
#define wifi_get_adapter_status(adapter, stat) (test_bit(stat, &(adapter)->status))
#define wifi_set_adapter_status(adapter, stat) (set_bit(stat, &(adapter)->status))
#define wifi_clr_adapter_status(adapter, stat) (clear_bit(stat, &(adapter)->status))
#define wifi_chg_adapter_status(adapter, stat) (change_bit(stat, &(adapter)->status))

#ifdef DHD_COREDUMP
#define PC_FOUND_BIT 0x01
#define LR_FOUND_BIT 0x02
#define ALL_ADDR_VAL (PC_FOUND_BIT | LR_FOUND_BIT)
#define READ_NUM_BYTES 1000
#define DHD_FUNC_STR_LEN 80
#define DHD_TRAP_CODE_LEN 12u
#define DHD_TRAP_STR_LEN (DHD_FUNC_STR_LEN * 2)

#define DHD_COREDUMP_MAGIC 0xDDCEDACF
#define DHD_COREDUMP_MAGIC_LEN	(4u)
#define TLV_TYPE_LENGTH_SIZE	(8u)
/* coredump is composed as following TLV format.
 * Type(32bit) | Length(32bit) | Value(x bit)
 * e.g) socram type | length | socram dump
 *      sssr core1 type | length | sssr core1 dump
 *      ...
 */
enum coredump_types {
	DHD_COREDUMP_TYPE_SSSRDUMP_CORE0_BEFORE = 0,
	DHD_COREDUMP_TYPE_SSSRDUMP_CORE0_AFTER  = 1,
	DHD_COREDUMP_TYPE_SSSRDUMP_CORE1_BEFORE = 2,
	DHD_COREDUMP_TYPE_SSSRDUMP_CORE1_AFTER  = 3,
	DHD_COREDUMP_TYPE_SSSRDUMP_CORE2_BEFORE = 4,
	DHD_COREDUMP_TYPE_SSSRDUMP_CORE2_AFTER  = 5,
	DHD_COREDUMP_TYPE_SSSRDUMP_DIG_BEFORE   = 6,
	DHD_COREDUMP_TYPE_SSSRDUMP_DIG_AFTER    = 7,
	DHD_COREDUMP_TYPE_SOCRAMDUMP            = 8,
#ifdef DHD_SDTC_ETB_DUMP
	DHD_COREDUMP_TYPE_SDTC_ETB_DUMP         = 9,
#endif /* DHD_SDTC_ETB_DUMP */
	DHD_COREDUMP_TYPE_SSSRDUMP_SAQM_BEFORE  = 10,
	DHD_COREDUMP_TYPE_SSSRDUMP_SAQM_AFTER   = 11,
#ifdef COEX_CPU
	DHD_COREDUMP_TYPE_COEX_DUMP             = 12,
#endif /* COEX_CPU */
	DHD_COREDUMP_TYPE_SSSRDUMP_CMN		= 13,
	DHD_COREDUMP_TYPE_SSSRDUMP_SRCB		= 14,
	DHD_COREDUMP_TYPE_MAX			= 15
};

#ifdef DHD_SSSR_DUMP
typedef struct dhd_coredump {
	uint32 type;
	uint32 length;
	void *bufptr;
} dhd_coredump_t;
#endif /* DHD_SSSR_DUMP */
#endif /* DHD_COREDUMP */

#ifdef BCMDBUS
#define DBUS_NRXQ	50
#define DBUS_NTXQ	100
#endif /* BCMDBUS */

#ifdef DHD_WAKE_RX_STATUS
#define ETHER_ICMP6_HEADER	20
#define ETHER_IPV6_SADDR (ETHER_ICMP6_HEADER + 2)
#define ETHER_IPV6_DAADR (ETHER_IPV6_SADDR + IPV6_ADDR_LEN)
#define ETHER_ICMPV6_TYPE (ETHER_IPV6_DAADR + IPV6_ADDR_LEN)
#endif /* DHD_WAKE_RX_STATUS */

#ifdef SUPPORT_AP_POWERSAVE
#define RXCHAIN_PWRSAVE_PPS			10
#define RXCHAIN_PWRSAVE_QUIET_TIME		10
#define RXCHAIN_PWRSAVE_STAS_ASSOC_CHECK	0
#endif /* SUPPORT_AP_POWERSAVE */

#define DHD_REGISTRATION_TIMEOUT  12000  /* msec : allowed time to finished dhd registration */

/* FW initialised value for ocl_rssi_threshold */
#define FW_OCL_RSSI_THRESH_INITVAL -75

typedef struct wifi_adapter_info {
	const char	*name;
	uint		irq_num;
	uint		intr_flags;
	const char	*fw_path;
	const char	*nv_path;
	void		*wifi_plat_data;	/* wifi ctrl func, for backward compatibility */
	uint		bus_type;
	uint		bus_num;
	uint		slot_num;
	int			index;
	int 		gpio_wl_reg_on;
#ifdef CUSTOMER_OOB
	int 		gpio_wl_host_wake;
#endif
	wait_queue_head_t status_event;
	unsigned long status;
#if defined(BT_OVER_SDIO)
	const char	*btfw_path;
#endif /* defined (BT_OVER_SDIO) */
#if defined(BCMSDIO)
	struct sdio_func *sdio_func;
#endif /* BCMSDIO */
#if defined(BCMPCIE)
	struct pci_dev *pci_dev;
	struct pci_saved_state *pci_saved_state;
#endif /* BCMPCIE */
#ifdef BCMDHD_PLATDEV
	struct platform_device *pdev;
#endif /* BCMDHD_PLATDEV */
} wifi_adapter_info_t;

#if defined(CONFIG_WIFI_CONTROL_FUNC)
#include <linux/wlan_plat.h>
#else
#include <dhd_plat.h>
#endif /* CONFIG_WIFI_CONTROL_FUNC */

typedef struct bcmdhd_wifi_platdata {
	uint				num_adapters;
	wifi_adapter_info_t	*adapters;
} bcmdhd_wifi_platdata_t;

/** Per STA params. A list of dhd_sta objects are managed in dhd_if */
typedef struct dhd_sta {
	cumm_ctr_t cumm_ctr;    /* cummulative queue length of child flowrings */
	uint16 flowid[NUMPRIO]; /* allocated flow ring ids (by priority) */
	void *ifp;             /* associated dhd_if */
	struct ether_addr ea;   /* stations ethernet mac address */
	struct list_head list;  /* link into dhd_if::sta_list */
	int idx;                /* index of self in dhd_pub::sta_pool[] */
	int ifidx;              /* index of interface in dhd */
#ifdef DHD_WMF
	struct dhd_sta *psta_prim; /* primary index of psta interface */
#endif /* DHD_WMF */

#if defined(DHD_MESH)
	bool mesh_flag;		/* indicatates the mesh capability */
#endif /* defined(DHD_MESH) */
	chanspec_t chanspec;    /* sta chanspec info */
#ifdef WL_MLO
	dhd_mlo_peer_info_t *peer_info;
#endif /* WL_MLO */
} dhd_sta_t;
typedef dhd_sta_t dhd_sta_pool_t;

#ifdef DHD_4WAYM4_FAIL_DISCONNECT
typedef enum {
	NONE_4WAY,
	M1_4WAY,
	M2_4WAY,
	M3_4WAY,
	M4_4WAY
} msg_4way_t;
typedef enum {
	M3_RXED,
	M4_TXFAILED
} msg_4way_state_t;
#define MAX_4WAY_TIMEOUT_MS 2000
#endif /* DHD_4WAYM4_FAIL_DISCONNECT */

#if defined(DHD_LB)
/* Dynamic CPU selection for load balancing. */
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/notifier.h>
#include <linux/workqueue.h>
#include <asm/atomic.h>

/* FIXME: Make this a module param or a sysfs. */
/* CPUs are divided into 3 sets -
 * SET_0 --> CPUs 0-3
 * SET_4 --> CPUs 4-7
 * SET_8 --> CPUs 8-11
 */
#if !defined(DHD_LB_CPU_SET8)
#define DHD_LB_CPU_SET8 0x0u /* Bigger CPU coreids mask */
#endif
#if !defined(DHD_LB_CPU_SET4)
#define DHD_LB_CPU_SET4 0x0u /* Big CPU coreids mask */
#endif
#if !defined(DHD_LB_CPU_SET0)
#define DHD_LB_CPU_SET0 0xFEu /* Little CPU coreids mask */
#endif

#define HIST_BIN_SIZE	9

#if defined(DHD_LB_TXP)
/* Pkttag not compatible with PROP_TXSTATUS or WLFC */
typedef struct dhd_tx_lb_pkttag_fr {
	struct net_device *net;
	int ifidx;
} dhd_tx_lb_pkttag_fr_t;

#define DHD_LB_TX_PKTTAG_SET_NETDEV(tag, netdevp)	((tag)->net = netdevp)
#define DHD_LB_TX_PKTTAG_NETDEV(tag)			((tag)->net)

#define DHD_LB_TX_PKTTAG_SET_IFIDX(tag, ifidx)	((tag)->ifidx = ifidx)
#define DHD_LB_TX_PKTTAG_IFIDX(tag)		((tag)->ifidx)
#endif /* DHD_LB_TXP */
#endif /* DHD_LB */

#define FILE_DUMP_MAX_WAIT_TIME 4000

#ifdef IL_BIGENDIAN
#include <bcmendian.h>
#define htod32(i) (bcmswap32(i))
#define htod16(i) (bcmswap16(i))
#define dtoh32(i) (bcmswap32(i))
#define dtoh16(i) (bcmswap16(i))
#define htodchanspec(i) htod16(i)
#define dtohchanspec(i) dtoh16(i)
#else
#define htod32(i) (i)
#define htod16(i) (i)
#define dtoh32(i) (i)
#define dtoh16(i) (i)
#define htodchanspec(i) (i)
#define dtohchanspec(i) (i)
#endif /* IL_BIGENDINA */

#ifdef BLOCK_IPV6_PACKET
#define HEX_PREF_STR	"0x"
#define UNI_FILTER_STR	"010000000000"
#define ZERO_ADDR_STR	"000000000000"
#define ETHER_TYPE_STR	"0000"
#define IPV6_FILTER_STR	"20"
#define ZERO_TYPE_STR	"00"
#endif /* BLOCK_IPV6_PACKET */

#if defined(SOFTAP)
extern bool ap_cfg_running;
extern bool ap_fw_loaded;
#endif

#if defined(BCMPCIE)
extern int dhd_get_suspend_bcn_li_dtim(dhd_pub_t *dhd, int *dtim_period, int *bcn_interval);
#else
extern int dhd_get_suspend_bcn_li_dtim(dhd_pub_t *dhd);
#endif /* OEM_ANDROID && BCMPCIE */

#ifdef DHD_SEND_HANG_PRIVCMD_ERRORS
extern uint32 report_hang_privcmd_err;
#endif /* DHD_SEND_HANG_PRIVCMD_ERRORS */

#if defined(SOFTAP_TPUT_ENHANCE)
extern void dhd_bus_setidletime(dhd_pub_t *dhdp, int idle_time);
extern void dhd_bus_getidletime(dhd_pub_t *dhdp, int *idle_time);
#endif /* SOFTAP_TPUT_ENHANCE */

#if defined(BCM_ROUTER_DHD)
void traffic_mgmt_pkt_set_prio(dhd_pub_t *dhdp, void *pktbuf);
#endif /* BCM_ROUTER_DHD */

typedef struct dhd_if_event {
	struct list_head	list;
	wl_event_data_if_t	event;
	char			name[IFNAMSIZ+1];
	uint8			mac[ETHER_ADDR_LEN];
} dhd_if_event_t;

/* Interface control information */
typedef struct dhd_if {
	struct dhd_info *info;			/* back pointer to dhd_info */
	/* OS/stack specifics */
	struct net_device *net;
	int				idx;			/* iface idx in dongle */
	uint			subunit;		/* subunit */
	uint8			mac_addr[ETHER_ADDR_LEN];	/* assigned MAC address */
	bool			set_macaddress;
	bool			set_multicast;
	uint8			bssidx;			/* bsscfg index for the interface */
	bool			attached;		/* Delayed attachment when unset */
	bool			txflowcontrol;	/* Per interface flow control indicator */
	char			name[IFNAMSIZ+1]; /* linux interface name */
	char			dngl_name[IFNAMSIZ+1]; /* corresponding dongle interface name */
	struct net_device_stats stats;

	// upper layer suppose different interface has different
	// 802.11 mode especially the multiply AP scenario and
	// bottom layer depends on its implementation
	int8			gmode;
	int8			nmode;
	int8			vhtmode;
	int8			hemode;
	int8			onlymode;

#ifdef DHD_WMF
	dhd_wmf_t		wmf;		/* per bsscfg wmf setting */
	bool	wmf_psta_disable;		/* enable/disable MC pkt to each mac
						 * of MC group behind PSTA
						 */
#endif /* DHD_WMF */
#ifdef PCIE_FULL_DONGLE
	struct list_head sta_list;		/* sll of associated stations */
	spinlock_t	sta_list_lock;		/* lock for manipulating sll */
#endif /* PCIE_FULL_DONGLE */
	uint32  ap_isolate;			/* ap-isolation settings */
#ifdef DHD_L2_FILTER
	bool parp_enable;
	bool parp_discard;
	bool parp_allnode;
	arp_table_t *phnd_arp_table;
	/* for Per BSS modification */
	bool dhcp_unicast;
	bool block_ping;
	bool grat_arp;
	bool block_tdls;
#endif /* DHD_L2_FILTER */
#if (defined(BCM_ROUTER_DHD) && defined(QOS_MAP_SET))
	uint8	 *qosmap_up_table;		/* user priority table, size is UP_TABLE_MAX */
	bool qosmap_up_table_enable;	/* flag set only when app want to set additional UP */
#endif /* BCM_ROUTER_DHD && QOS_MAP_SET */
#ifdef DHD_MCAST_REGEN
	bool mcast_regen_bss_enable;
#endif
	bool rx_pkt_chainable;		/* set all rx packet to chainable config by default */
	cumm_ctr_t cumm_ctr;		/* cummulative queue length of child flowrings */
#ifdef BCM_ROUTER_DHD
	bool	primsta_dwds;		/* DWDS status of primary sta interface */
#endif /* BCM_ROUTER_DHD */
	uint8 tx_paths_active;
	bool del_in_progress;
	bool static_if;			/* used to avoid some operations on static_if */
	bool mgmt_if;			/* differentiate interfaces exposed only for mgmt purpose */
#ifdef DHD_4WAYM4_FAIL_DISCONNECT
	struct delayed_work m4state_work;
	atomic_t m4state;
#endif /* DHD_4WAYM4_FAIL_DISCONNECT */
#ifdef DHDTCPSYNC_FLOOD_BLK
	uint32 tsync_rcvd;
	uint32 tsyncack_txed;
	u64 last_sync;
	struct work_struct  blk_tsfl_work;
	uint32 tsync_per_sec;
	bool disconnect_tsync_flood;
#endif /* DHDTCPSYNC_FLOOD_BLK */
#ifdef DHD_POST_EAPOL_M1_AFTER_ROAM_EVT
	bool recv_reassoc_evt;
	bool post_roam_evt;
#endif /* DHD_POST_EAPOL_M1_AFTER_ROAM_EVT */
	uint64 rx_pkts;		/* per interface total rx pkts, can be cleared with iovar */
	uint64 tx_pkts;		/* per interface total tx pkts, can be cleared with iovar */
	bool	llc_enabled;	/* Indicate/configure  Additional llc header enabled */
	uint8	*llc_hdr;	/* Additional llc header data */
	uint8	llc_hdr_len;	/* Additional llc header data length */
	uint8	llc_headroom_added_len;	/* Headroom length added to this net dev for LLC */
	bool	dhcp_request_pending;
} dhd_if_t;

struct ipv6_work_info_t {
	uint8			if_idx;
	char			ipv6_addr[IPV6_ADDR_LEN];
	unsigned long		event;
};

typedef struct dhd_dump {
	uint8 *buf;
	int bufsize;
	uint8 *hscb_buf;
	int hscb_bufsize;
#ifdef COEX_CPU
	uint8 *coex_buf;
	int coex_bufsize;
#endif /* COEX_CPU */
} dhd_dump_t;
#ifdef DNGL_AXI_ERROR_LOGGING
typedef struct dhd_axi_error_dump {
	ulong fault_address;
	uint32 axid;
	struct hnd_ext_trap_axi_error_v1 etd_axi_error_v1;
} dhd_axi_error_dump_t;
#endif /* DNGL_AXI_ERROR_LOGGING */
#ifdef BCM_ROUTER_DHD
typedef struct dhd_write_file {
	char file_path[64];
	uint32 file_flags;
	uint8 *buf;
	int bufsize;
} dhd_write_file_t;
#endif

#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
struct dhd_rx_tx_work {
	struct work_struct work;
	struct sk_buff *skb;
	struct net_device *net;
	struct dhd_pub *pub;
};
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */

#ifdef FILTER_IE
#define FILTER_IE_PATH "filter_ie"
#define FILTER_IE_BUFSZ 1024 /* ioc buffsize for FILTER_IE */
#define FILE_BLOCK_READ_SIZE 256
#define WL_FILTER_IE_IOV_HDR_SIZE OFFSETOF(wl_filter_ie_iov_v1_t, tlvs)
#endif /* FILTER_IE */

#define NULL_CHECK(p, s, err)  \
			do { \
				if (!(p)) { \
					printk("NULL POINTER (%s) : %s\n", __FUNCTION__, (s)); \
					err = BCME_ERROR; \
					return err; \
				} \
			} while (0)

int dhd_wifi_platform_register_drv(void);
void dhd_wifi_platform_unregister_drv(void);
wifi_adapter_info_t *dhd_wifi_platform_get_adapter(uint32 bus_type, uint32 bus_num,
	uint32 slot_num);
int wifi_platform_set_power(wifi_adapter_info_t *adapter, bool on, unsigned long msec);
int wifi_platform_bus_enumerate(wifi_adapter_info_t *adapter, bool device_present);
int wifi_platform_get_irq_number(wifi_adapter_info_t *adapter, unsigned long *irq_flags_ptr);
extern int wifi_platform_get_irq_level(wifi_adapter_info_t *adapter);
int wifi_platform_get_mac_addr(wifi_adapter_info_t *adapter, unsigned char *buf, int ifidx);
#ifdef DHD_COREDUMP
int wifi_platform_set_coredump(wifi_adapter_info_t *adapter, const char *buf, int buf_len,
	const char *info);
#endif /* DHD_COREDUMP */
#ifdef CUSTOM_COUNTRY_CODE
void *wifi_platform_get_country_code(wifi_adapter_info_t *adapter, char *ccode,
	u32 flags);
#else
void *wifi_platform_get_country_code(wifi_adapter_info_t *adapter, char *ccode);
#endif /* CUSTOM_COUNTRY_CODE */
void *wifi_platform_prealloc(wifi_adapter_info_t *adapter, int section, unsigned long size);
void *wifi_platform_get_prealloc_func_ptr(wifi_adapter_info_t *adapter);

int dhd_get_fw_mode(struct dhd_info *dhdinfo);
bool dhd_update_fw_nv_path(struct dhd_info *dhdinfo);
void dhd_update_fw_path(dhd_pub_t *dhdpub, const char *fw_path);

#ifdef BCM_ROUTER_DHD
void dhd_update_dpsta_interface_for_sta(dhd_pub_t *dhdp, int ifidx, void *event_data);
#endif /* BCM_ROUTER_DHD */
#ifdef DHD_WMF
dhd_wmf_t *dhd_wmf_conf(dhd_pub_t *dhdp, uint32 idx);
int dhd_get_wmf_psta_disable(dhd_pub_t *dhdp, uint32 idx);
int dhd_set_wmf_psta_disable(dhd_pub_t *dhdp, uint32 idx, int val);
void dhd_update_psta_interface_for_sta(dhd_pub_t *dhdp, char *ifname,
	void *mac_addr, void *event_data);
#endif /* DHD_WMF */
#if defined(BT_OVER_SDIO)
int dhd_net_bus_get(struct net_device *dev);
int dhd_net_bus_put(struct net_device *dev);
#endif /* BT_OVER_SDIO */
#if defined(WLADPS)
#define ADPS_ENABLE	1
#define ADPS_DISABLE	0

int dhd_enable_adps(dhd_pub_t *dhd, uint8 on);
#endif
#ifdef DHDTCPSYNC_FLOOD_BLK
extern void dhd_reset_tcpsync_info_by_ifp(dhd_if_t *ifp);
extern void dhd_reset_tcpsync_info_by_dev(struct net_device *dev);
#endif /* DHDTCPSYNC_FLOOD_BLK */
extern void dhd_set_del_in_progress(dhd_pub_t *dhdp, struct net_device *ndev);
extern void dhd_clear_del_in_progress(dhd_pub_t *dhdp, struct net_device *ndev);
#ifdef PCIE_FULL_DONGLE
extern void dhd_net_del_flowrings_sta(dhd_pub_t *dhd, struct net_device *ndev);
#endif /* PCIE_FULL_DONGLE */
int dhd_get_fw_capabilities(dhd_pub_t *dhd);
#ifdef WL_CFGVENDOR_SEND_ALERT_EVENT
void dhd_alert_process(struct work_struct *work_data);
#endif /* WL_CFGVENDOR_SEND_ALERT_EVENT */
void dhd_event_logtrace_enqueue(dhd_pub_t *dhdp, int ifidx, void *pktbuf);
int dhd_get_platform_naming_for_nvram_clmblob_file(download_type_t component, char* file_name);
#if defined(SUPPORT_MULTIPLE_NVRAM) || defined(SUPPORT_MULTIPLE_CLMBLOB)
#ifdef USE_CID_CHECK
void dhd_set_platform_ext_name_for_chip_version(char *chip_version);
#endif /* USE_CID_CHECK */
#endif /* SUPPORT_MULTIPLE_NVRAM || SUPPORT_MULTIPLE_CLMBLOB */
extern void dhd_os_skbq_dump(struct sk_buff_head *qdump, char *qname);
#if defined(RTT_SUPPORT) && defined(WL_CFG80211)
int dhd_dev_rtt_capability_mc_az(struct net_device *dev, rtt_capabilities_mc_az_t *capa);
#endif /* RTT_SUPPORT && WL_CFG80211 */
void dhd_netif_rx_ni(struct sk_buff * skb);
#ifdef WL_MONITOR
extern bool dhd_monitor_enabled(dhd_pub_t *dhd, int ifidx);
#ifdef BCMPCIE
extern void dhd_rx_mon_pkt(dhd_pub_t *dhdp, host_rxbuf_cmpl_t* msg, void *pkt, int ifidx);
#endif /* BCMPCIE */
#endif /* WL_MONITOR */
#if defined(DBG_PKT_MON)
#ifdef PCIE_FULL_DONGLE
extern void dhd_80211_mon_pkt(dhd_pub_t *dhdp, host_rxbuf_cmpl_t *msg, void *pkt, int ifidx);
#else
extern bool dhd_80211_mon_pkt(dhd_pub_t *dhdp, void *pkt, int ifidx);
#endif /* PCIE_FULL_DONGLE */
#endif /* DBG_PKT_MON */
extern int dhd_ioctl_entry_local(struct net_device *net, wl_ioctl_t *ioc, int cmd);
extern int dhd_change_mtu(dhd_pub_t *dhdp, int new_mtu, int ifidx);
extern void wl_update_roamscan_cache_by_band(struct net_device *dev, int band);
extern int dhd_dev_init_ioctl(struct net_device *dev);
extern void dhd_set_packet_filter(dhd_pub_t *dhd);
#ifdef BCMPCIE
extern void dhd_dpc_tasklet_kill(dhd_pub_t *dhdp);
extern void dhd_dpc_enable(dhd_pub_t *dhdp);
#endif /* BCMPCIE */
#ifdef DHD_SSSR_DUMP
int dhdpcie_sssr_dump_get_before_after_len(dhd_pub_t *dhd, uint32 *arr_len);
#endif /* DHD_SSSR_DUMP */
#ifdef WBRC
extern int wbrc_init(void);
extern void wbrc_exit(void);
extern int wbrc_wl2bt_reset(void);
extern int wbrc_wlan_on_ack(void);
extern int wbrc_wlan_on_started(void);
#endif /* WBRC */
#ifdef DHD_COREDUMP
extern int dhd_collect_coredump(dhd_pub_t *dhdp, dhd_dump_t *dump,
	bool collect_sssr, bool collect_fis);
#endif /* DHD_COREDUMP */
extern void dhd_force_collect_init_fail_dumps(dhd_pub_t *dhdp);
#if defined(DHD_LB_RXP)
extern uint dhd_rx_emerge_queue_len(dhd_pub_t *dhdp);
#endif /* DHD_LB_RXP */

#endif /* __DHD_LINUX_H__ */
