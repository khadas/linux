/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Rockchip Electronics Co.Ltd
 * Author: Felix Zeng <felix.zeng@rock-chips.com>
 */

#ifndef __LINUX_RKNPU_DRV_H_
#define __LINUX_RKNPU_DRV_H_

#include <linux/completion.h>
#include <linux/device.h>
#include <linux/kref.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/regulator/consumer.h>
#include <linux/version.h>
#include <linux/hrtimer.h>
#include <linux/miscdevice.h>

#include <soc/rockchip/rockchip_opp_select.h>
#include <soc/rockchip/rockchip_system_monitor.h>
#include <soc/rockchip/rockchip_ipa.h>

#include "rknpu_job.h"
#include "rknpu_fence.h"
#include "rknpu_debugger.h"
#include "rknpu_mm.h"

#define DRIVER_NAME "rknpu"
#define DRIVER_DESC "RKNPU driver"
#define DRIVER_DATE "20240828"
#define DRIVER_MAJOR 0
#define DRIVER_MINOR 9
#define DRIVER_PATCHLEVEL 8

#define LOG_TAG "RKNPU"

/* sample interval: 1000ms */
#define RKNPU_LOAD_INTERVAL 1000000000

#define LOG_INFO(fmt, args...) pr_info(LOG_TAG ": " fmt, ##args)
#if KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE
#define LOG_WARN(fmt, args...) pr_warn(LOG_TAG ": " fmt, ##args)
#else
#define LOG_WARN(fmt, args...) pr_warning(LOG_TAG ": " fmt, ##args)
#endif
#define LOG_DEBUG(fmt, args...) pr_devel(LOG_TAG ": " fmt, ##args)
#define LOG_ERROR(fmt, args...) pr_err(LOG_TAG ": " fmt, ##args)

#define LOG_DEV_INFO(dev, fmt, args...) dev_info(dev, LOG_TAG ": " fmt, ##args)
#define LOG_DEV_WARN(dev, fmt, args...) dev_warn(dev, LOG_TAG ": " fmt, ##args)
#define LOG_DEV_DEBUG(dev, fmt, args...) dev_dbg(dev, LOG_TAG ": " fmt, ##args)
#define LOG_DEV_ERROR(dev, fmt, args...) dev_err(dev, LOG_TAG ": " fmt, ##args)

#define RKNPU_MAX_IOMMU_DOMAIN_NUM 16
#define RKNPU_CACHE_SG_TABLE_NUM 2

struct rknpu_irqs_data {
	const char *name;
	irqreturn_t (*irq_hdl)(int irq, void *ctx);
};

struct rknpu_amount_data {
	uint16_t offset_clr_all;
	uint16_t offset_dt_wr;
	uint16_t offset_dt_rd;
	uint16_t offset_wt_rd;
};

struct rknpu_config {
	__u32 bw_priority_addr;
	__u32 bw_priority_length;
	__u64 dma_mask;
	__u32 pc_data_amount_scale;
	__u32 pc_task_number_bits;
	__u32 pc_task_number_mask;
	__u32 pc_task_status_offset;
	__u32 pc_dma_ctrl;
	const struct rknpu_irqs_data *irqs;
	int num_irqs;
	__u64 nbuf_phyaddr;
	__u64 nbuf_size;
	__u64 max_submit_number;
	__u32 core_mask;
	const struct rknpu_amount_data *amount_top;
	const struct rknpu_amount_data *amount_core;
	void (*state_init)(struct rknpu_device *rknpu_dev);
	int (*cache_sgt_init)(struct rknpu_device *rknpu_dev);
};

struct rknpu_timer {
	ktime_t busy_time;
	ktime_t total_busy_time;
};

struct rknpu_subcore_data {
	struct list_head todo_list;
	wait_queue_head_t job_done_wq;
	struct rknpu_job *job;
	int64_t task_num;
	struct rknpu_timer timer;
};

/**
 * RKNPU device
 *
 * @base: IO mapped base address for device
 * @dev: Device instance
 * @drm_dev: DRM device instance
 */
struct rknpu_device {
	void __iomem *base[RKNPU_MAX_CORES];
	struct device *dev;
#ifdef CONFIG_ROCKCHIP_RKNPU_DRM_GEM
	struct device *fake_dev;
	struct drm_device *drm_dev;
#endif
#ifdef CONFIG_ROCKCHIP_RKNPU_DMA_HEAP
	struct miscdevice miscdev;
	struct rk_dma_heap *heap;
#endif
	atomic_t sequence;
	spinlock_t lock;
	spinlock_t irq_lock;
	struct mutex power_lock;
	struct mutex reset_lock;
	struct mutex domain_lock;
	struct rknpu_subcore_data subcore_datas[RKNPU_MAX_CORES];
	const struct rknpu_config *config;
	void __iomem *bw_priority_base;
	struct rknpu_fence_context *fence_ctx;
	bool iommu_en;
	struct reset_control **srsts;
	int num_srsts;
	struct clk_bulk_data *clks;
	int num_clks;
	struct regulator *vdd;
	struct regulator *mem;
	struct monitor_dev_info *mdev_info;
	struct ipa_power_model_data *model_data;
	struct thermal_cooling_device *devfreq_cooling;
	struct devfreq *devfreq;
	unsigned long ondemand_freq;
	struct rockchip_opp_info opp_info;
	unsigned long current_freq;
	unsigned long current_volt;
	int bypass_irq_handler;
	int bypass_soft_reset;
	bool soft_reseting;
	struct device *genpd_dev_npu0;
	struct device *genpd_dev_npu1;
	struct device *genpd_dev_npu2;
	bool multiple_domains;
	atomic_t power_refcount;
	atomic_t cmdline_power_refcount;
	struct delayed_work power_off_work;
	struct workqueue_struct *power_off_wq;
	struct rknpu_debugger debugger;
	struct hrtimer timer;
	ktime_t kt;
	phys_addr_t sram_start;
	phys_addr_t sram_end;
	phys_addr_t nbuf_start;
	phys_addr_t nbuf_end;
	uint32_t sram_size;
	uint32_t nbuf_size;
	void __iomem *sram_base_io;
	void __iomem *nbuf_base_io;
	struct rknpu_mm *sram_mm;
	unsigned long power_put_delay;
	struct iommu_group *iommu_group;
	int iommu_domain_num;
	int iommu_domain_id;
	struct iommu_domain *iommu_domains[RKNPU_MAX_IOMMU_DOMAIN_NUM];
	struct sg_table *cache_sgt[RKNPU_CACHE_SG_TABLE_NUM];
	atomic_t iommu_domain_refcount;
};

struct rknpu_session {
	struct rknpu_device *rknpu_dev;
	struct list_head list;
};

int rknpu_power_get(struct rknpu_device *rknpu_dev);
int rknpu_power_put(struct rknpu_device *rknpu_dev);
int rknpu_power_put_delay(struct rknpu_device *rknpu_dev);

#endif /* __LINUX_RKNPU_DRV_H_ */
