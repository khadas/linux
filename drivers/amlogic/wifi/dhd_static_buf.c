/*
 * drivers/amlogic/wifi/dhd_static_buf.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#define pr_fmt(fmt)	"Wifi: %s: " fmt, __func__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#include <linux/amlogic/dhd_buf.h>

#define	DHD_STATIC_VERSION_STR		"1.579.77.41.9"

#define BCMDHD_SDIO
#define BCMDHD_PCIE

enum dhd_prealloc_index {
	DHD_PREALLOC_PROT = 0,
#if defined(BCMDHD_SDIO)
	DHD_PREALLOC_RXBUF = 1,
	DHD_PREALLOC_DATABUF = 2,
#endif
	DHD_PREALLOC_OSL_BUF = 3,
	DHD_PREALLOC_SKB_BUF = 4,
	DHD_PREALLOC_WIPHY_ESCAN0 = 5,
	DHD_PREALLOC_WIPHY_ESCAN1 = 6,
	DHD_PREALLOC_DHD_INFO = 7,
	DHD_PREALLOC_DHD_WLFC_INFO = 8,
#ifdef BCMDHD_PCIE
	DHD_PREALLOC_IF_FLOW_LKUP = 9,
#endif
	DHD_PREALLOC_MEMDUMP_BUF = 10,
	DHD_PREALLOC_MEMDUMP_RAM = 11,
	DHD_PREALLOC_DHD_WLFC_HANGER = 12,
	DHD_PREALLOC_PKTID_MAP = 13,
	DHD_PREALLOC_PKTID_MAP_IOCTL = 14,
	DHD_PREALLOC_DHD_LOG_DUMP_BUF = 15,
	DHD_PREALLOC_DHD_LOG_DUMP_BUF_EX = 16,
	DHD_PREALLOC_DHD_PKTLOG_DUMP_BUF = 17,
	DHD_PREALLOC_STAT_REPORT_BUF = 18,
	DHD_PREALLOC_WL_ESCAN_INFO = 19,
	DHD_PREALLOC_FW_VERBOSE_RING = 20,
	DHD_PREALLOC_FW_EVENT_RING = 21,
	DHD_PREALLOC_DHD_EVENT_RING = 22,
	DHD_PREALLOC_NAN_EVENT_RING = 23,
	DHD_PREALLOC_MAX
};

#define STATIC_BUF_MAX_NUM	20
#define STATIC_BUF_SIZE	(PAGE_SIZE*2)

#define DHD_PREALLOC_PROT_SIZE	(16 * 1024)
#define DHD_PREALLOC_RXBUF_SIZE	(24 * 1024)
#define DHD_PREALLOC_DATABUF_SIZE	(64 * 1024)
#define DHD_PREALLOC_OSL_BUF_SIZE	(STATIC_BUF_MAX_NUM * STATIC_BUF_SIZE)
#define DHD_PREALLOC_WIPHY_ESCAN0_SIZE	(64 * 1024)
#define DHD_PREALLOC_DHD_INFO_SIZE	(32 * 1024)
#define DHD_PREALLOC_MEMDUMP_RAM_SIZE	(1280 * 1024)
#define DHD_PREALLOC_DHD_WLFC_HANGER_SIZE	(73 * 1024)
#define DHD_PREALLOC_WL_ESCAN_INFO_SIZE	(67 * 1024)
#ifdef CONFIG_64BIT
#define DHD_PREALLOC_IF_FLOW_LKUP_SIZE	(20 * 1024 * 2)
#else
#define DHD_PREALLOC_IF_FLOW_LKUP_SIZE	(20 * 1024)
#endif
#define FW_VERBOSE_RING_SIZE		(256 * 1024)
#define FW_EVENT_RING_SIZE		(64 * 1024)
#define DHD_EVENT_RING_SIZE		(64 * 1024)
#define NAN_EVENT_RING_SIZE		(64 * 1024)

#if defined(CONFIG_64BIT)
#define WLAN_DHD_INFO_BUF_SIZE	(24 * 1024)
#define WLAN_DHD_WLFC_BUF_SIZE	(64 * 1024)
#define WLAN_DHD_IF_FLOW_LKUP_SIZE	(64 * 1024)
#else
#define WLAN_DHD_INFO_BUF_SIZE	(16 * 1024)
#define WLAN_DHD_WLFC_BUF_SIZE	(29 * 1024)
#define WLAN_DHD_IF_FLOW_LKUP_SIZE	(20 * 1024)
#endif /* CONFIG_64BIT */
#define WLAN_DHD_MEMDUMP_SIZE	(800 * 1024)

#define DHD_SKB_1PAGE_BUFSIZE	(PAGE_SIZE*1)
#define DHD_SKB_2PAGE_BUFSIZE	(PAGE_SIZE*2)
#define DHD_SKB_4PAGE_BUFSIZE	(PAGE_SIZE*4)

#define DHD_SKB_1PAGE_BUF_NUM	8
#define DHD_SKB_2PAGE_BUF_NUM	64
#define DHD_SKB_4PAGE_BUF_NUM	1

/* The number is defined in linux_osl.c
 * WLAN_SKB_1_2PAGE_BUF_NUM => STATIC_PKT_1_2PAGE_NUM
 * WLAN_SKB_BUF_NUM => STATIC_PKT_MAX_NUM
 */
#define WLAN_SKB_1_2PAGE_BUF_NUM ((DHD_SKB_1PAGE_BUF_NUM) + \
		(DHD_SKB_2PAGE_BUF_NUM))
#define WLAN_SKB_BUF_NUM ((WLAN_SKB_1_2PAGE_BUF_NUM) + (DHD_SKB_4PAGE_BUF_NUM))

void *wlan_static_prot;
void *wlan_static_rxbuf;
void *wlan_static_databuf;
void *wlan_static_osl_buf;
void *wlan_static_scan_buf0;
void *wlan_static_scan_buf1;
void *wlan_static_dhd_info_buf;
void *wlan_static_dhd_wlfc_info_buf;
void *wlan_static_if_flow_lkup;
void *wlan_static_dhd_memdump_ram_buf;
void *wlan_static_dhd_wlfc_hanger_buf;
void *wlan_static_wl_escan_info_buf;
void *wlan_static_fw_verbose_ring_buf;
void *wlan_static_fw_event_ring_buf;
void *wlan_static_dhd_event_ring_buf;
void *wlan_static_nan_event_ring_buf;

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

void *bcmdhd_mem_prealloc(int section, unsigned long size)
{
	pr_info("sectoin %d, size %ld\n", section, size);
	if (section == DHD_PREALLOC_PROT)
		return wlan_static_prot;

#if defined(BCMDHD_SDIO)
	if (section == DHD_PREALLOC_RXBUF)
		return wlan_static_rxbuf;

	if (section == DHD_PREALLOC_DATABUF)
		return wlan_static_databuf;
#endif /* BCMDHD_SDIO */

	if (section == DHD_PREALLOC_SKB_BUF)
		return wlan_static_skb;

	if (section == DHD_PREALLOC_WIPHY_ESCAN0)
		return wlan_static_scan_buf0;

	if (section == DHD_PREALLOC_WIPHY_ESCAN1)
		return wlan_static_scan_buf1;

	if (section == DHD_PREALLOC_OSL_BUF) {
		if (size > DHD_PREALLOC_OSL_BUF_SIZE) {
			pr_err("request OSL_BUF(%lu) > %ld\n",
				size, DHD_PREALLOC_OSL_BUF_SIZE);
			return NULL;
		}
		return wlan_static_osl_buf;
	}

	if (section == DHD_PREALLOC_DHD_INFO) {
		if (size > DHD_PREALLOC_DHD_INFO_SIZE) {
			pr_err("request DHD_INFO size(%lu) > %d\n",
				size, DHD_PREALLOC_DHD_INFO_SIZE);
			return NULL;
		}
		return wlan_static_dhd_info_buf;
	}
	if (section == DHD_PREALLOC_DHD_WLFC_INFO) {
		if (size > WLAN_DHD_WLFC_BUF_SIZE) {
			pr_err("request DHD_WLFC_INFO size(%lu) > %d\n",
				size, WLAN_DHD_WLFC_BUF_SIZE);
			return NULL;
		}
		return wlan_static_dhd_wlfc_info_buf;
	}
#ifdef BCMDHD_PCIE
	if (section == DHD_PREALLOC_IF_FLOW_LKUP)  {
		if (size > DHD_PREALLOC_IF_FLOW_LKUP_SIZE) {
			pr_err("request DHD_IF_FLOW_LKUP size(%lu) > %d\n",
				size, DHD_PREALLOC_IF_FLOW_LKUP_SIZE);
			return NULL;
		}

		return wlan_static_if_flow_lkup;
	}
#endif /* BCMDHD_PCIE */
	if (section == DHD_PREALLOC_MEMDUMP_RAM) {
		if (size > DHD_PREALLOC_MEMDUMP_RAM_SIZE) {
			pr_err("request DHD_PREALLOC_MEMDUMP_RAM_SIZE(%lu) > %d\n",
				size, DHD_PREALLOC_MEMDUMP_RAM_SIZE);
			return NULL;
		}

		return wlan_static_dhd_memdump_ram_buf;
	}
	if (section == DHD_PREALLOC_DHD_WLFC_HANGER) {
		if (size > DHD_PREALLOC_DHD_WLFC_HANGER_SIZE) {
			pr_err("request DHD_WLFC_HANGER size(%lu) > %d\n",
				size, DHD_PREALLOC_DHD_WLFC_HANGER_SIZE);
			return NULL;
		}
		return wlan_static_dhd_wlfc_hanger_buf;
	}
	if (section == DHD_PREALLOC_WL_ESCAN_INFO) {
		if (size > DHD_PREALLOC_WL_ESCAN_INFO_SIZE) {
			pr_err("request DHD_PREALLOC_WL_ESCAN_INFO_SIZE(%lu) > %d\n",
				size, DHD_PREALLOC_WL_ESCAN_INFO_SIZE);
			return NULL;
		}

		return wlan_static_wl_escan_info_buf;
	}
	if (section == DHD_PREALLOC_FW_VERBOSE_RING) {
		if (size > FW_VERBOSE_RING_SIZE) {
			pr_err("request DHD_PREALLOC_FW_VERBOSE_RING(%lu) > %d\n",
				size, FW_VERBOSE_RING_SIZE);
			return NULL;
		}

		return wlan_static_fw_verbose_ring_buf;
	}
	if (section == DHD_PREALLOC_FW_EVENT_RING) {
		if (size > FW_EVENT_RING_SIZE) {
			pr_err("request DHD_PREALLOC_FW_EVENT_RING(%lu) > %d\n",
				size, FW_EVENT_RING_SIZE);
			return NULL;
		}

		return wlan_static_fw_event_ring_buf;
	}
	if (section == DHD_PREALLOC_DHD_EVENT_RING) {
		if (size > DHD_EVENT_RING_SIZE) {
			pr_err("request DHD_PREALLOC_DHD_EVENT_RING(%lu) > %d\n",
				size, DHD_EVENT_RING_SIZE);
			return NULL;
		}

		return wlan_static_dhd_event_ring_buf;
	}
	if (section == DHD_PREALLOC_NAN_EVENT_RING) {
		if (size > NAN_EVENT_RING_SIZE) {
			pr_err("request DHD_PREALLOC_NAN_EVENT_RING(%lu) > %d\n",
				size, NAN_EVENT_RING_SIZE);
			return NULL;
		}

		return wlan_static_nan_event_ring_buf;
	}
	if ((section < 0) || (section > DHD_PREALLOC_MAX))
		pr_err("request section id(%d) is out of max index %d\n",
				section, DHD_PREALLOC_MAX);

	pr_err("failed to alloc section %d, size=%ld\n",
			 section, size);

	return NULL;
}
EXPORT_SYMBOL(bcmdhd_mem_prealloc);

int bcmdhd_init_wlan_mem(void)
{
	int i;
	int j;

	pr_info("bcmdhd_init_wlan_mem %s\n", DHD_STATIC_VERSION_STR);

	for (i = 0; i < DHD_SKB_1PAGE_BUF_NUM; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_1PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	for (i = DHD_SKB_1PAGE_BUF_NUM; i < WLAN_SKB_1_2PAGE_BUF_NUM; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_2PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

#if defined(BCMDHD_SDIO)
	wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_4PAGE_BUFSIZE);
	if (!wlan_static_skb[i])
		goto err_skb_alloc;
#endif /* BCMDHD_SDIO */

	wlan_static_prot = kmalloc(DHD_PREALLOC_PROT_SIZE, GFP_KERNEL);
	if (!wlan_static_prot)
		goto err_mem_alloc;

#if defined(BCMDHD_SDIO)
	wlan_static_rxbuf = kmalloc(DHD_PREALLOC_RXBUF_SIZE, GFP_KERNEL);
	if (!wlan_static_rxbuf)
		goto err_mem_alloc;

	wlan_static_databuf = kmalloc(DHD_PREALLOC_DATABUF_SIZE, GFP_KERNEL);
	if (!wlan_static_databuf)
		goto err_mem_alloc;
#endif /* BCMDHD_SDIO */

	wlan_static_osl_buf = kmalloc(DHD_PREALLOC_OSL_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_osl_buf)
		goto err_mem_alloc;

	wlan_static_scan_buf0 = kmalloc(DHD_PREALLOC_WIPHY_ESCAN0_SIZE,
					 GFP_KERNEL);
	if (!wlan_static_scan_buf0)
		goto err_mem_alloc;

	wlan_static_dhd_info_buf = kmalloc(DHD_PREALLOC_DHD_INFO_SIZE,
						GFP_KERNEL);
	if (!wlan_static_dhd_info_buf)
		goto err_mem_alloc;

	wlan_static_dhd_wlfc_info_buf = kmalloc(WLAN_DHD_WLFC_BUF_SIZE,
						 GFP_KERNEL);
	if (!wlan_static_dhd_wlfc_info_buf)
		goto err_mem_alloc;

#ifdef BCMDHD_PCIE
	wlan_static_if_flow_lkup = kmalloc(DHD_PREALLOC_IF_FLOW_LKUP_SIZE,
						 GFP_KERNEL);
	if (!wlan_static_if_flow_lkup)
		goto err_mem_alloc;
#endif /* BCMDHD_PCIE */

	wlan_static_dhd_memdump_ram_buf = kmalloc(DHD_PREALLOC_MEMDUMP_RAM_SIZE,
						 GFP_KERNEL);
	if (!wlan_static_dhd_memdump_ram_buf)
		goto err_mem_alloc;

	wlan_static_dhd_wlfc_hanger_buf = kmalloc(
				DHD_PREALLOC_DHD_WLFC_HANGER_SIZE,
				GFP_KERNEL);
	if (!wlan_static_dhd_wlfc_hanger_buf)
		goto err_mem_alloc;

	wlan_static_wl_escan_info_buf = kmalloc(DHD_PREALLOC_WL_ESCAN_INFO_SIZE,
						GFP_KERNEL);
	if (!wlan_static_wl_escan_info_buf)
		goto err_mem_alloc;

	wlan_static_fw_verbose_ring_buf = kmalloc(
						FW_VERBOSE_RING_SIZE,
						GFP_KERNEL);
	if (!wlan_static_fw_verbose_ring_buf)
		goto err_mem_alloc;

	wlan_static_fw_event_ring_buf = kmalloc(FW_EVENT_RING_SIZE,
						GFP_KERNEL);
	if (!wlan_static_fw_event_ring_buf)
		goto err_mem_alloc;

	wlan_static_dhd_event_ring_buf = kmalloc(DHD_EVENT_RING_SIZE,
						GFP_KERNEL);
	if (!wlan_static_dhd_event_ring_buf)
		goto err_mem_alloc;

	wlan_static_nan_event_ring_buf = kmalloc(NAN_EVENT_RING_SIZE,
						GFP_KERNEL);
	if (!wlan_static_nan_event_ring_buf)
		goto err_mem_alloc;

	pr_info("bcmdhd_init_wlan_mem prealloc ok\n");
	return 0;

err_mem_alloc:

	kfree(wlan_static_prot);
#if defined(BCMDHD_SDIO)
	kfree(wlan_static_rxbuf);
	kfree(wlan_static_databuf);
#endif /* BCMDHD_SDIO */
	kfree(wlan_static_osl_buf);
	kfree(wlan_static_scan_buf0);
	kfree(wlan_static_scan_buf1);
	kfree(wlan_static_dhd_info_buf);
	kfree(wlan_static_dhd_wlfc_info_buf);
#ifdef BCMDHD_PCIE
	kfree(wlan_static_if_flow_lkup);
#endif /* BCMDHD_PCIE */
	kfree(wlan_static_dhd_memdump_ram_buf);
	kfree(wlan_static_dhd_wlfc_hanger_buf);
	kfree(wlan_static_wl_escan_info_buf);
#ifdef BCMDHD_PCIE
	kfree(wlan_static_fw_verbose_ring_buf);
	kfree(wlan_static_fw_event_ring_buf);
	kfree(wlan_static_dhd_event_ring_buf);
	kfree(wlan_static_nan_event_ring_buf);
#endif /* BCMDHD_PCIE */

	pr_err("Failed to mem_alloc for WLAN\n");

	i = WLAN_SKB_BUF_NUM;

err_skb_alloc:
	pr_err("Failed to skb_alloc for WLAN\n");
	for (j = 0; j < i; j++)
		dev_kfree_skb(wlan_static_skb[j]);

	return -ENOMEM;
}
EXPORT_SYMBOL(bcmdhd_init_wlan_mem);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("wifi device tree driver");
