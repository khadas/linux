/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _HIFI4DSP_DSP_H
#define _HIFI4DSP_DSP_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/irqreturn.h>

#include "hifi4dsp_firmware.h"

struct firmware;
struct hifi4dsp_pdata;
struct hifi4dsp_dsp;

/*
 * DSP memory offsets and addresses.
 */
struct hifi4dsp_addr {
	u32 smem_paddr;
	u32 smem_size;
	u32 reg_paddr;
	u32 reg_size;
	u32 fw_paddr;
	u32 fw_size;
	void __iomem *smem;
	void __iomem *reg;
	void __iomem *fw;
};

/*
 * DSP Operations exported by platform Audio DSP driver.
 */
struct hifi4dsp_ops {
	int (*boot)(struct hifi4dsp_dsp *dsp);
	int (*reset)(struct hifi4dsp_dsp *dsp);
	int (*wake)(struct hifi4dsp_dsp *dsp);
	int (*sleep)(struct hifi4dsp_dsp *dsp);
	/* Shim IO */
	void (*write)(void __iomem *addr, u32 offset, u32 value);
	u32 (*read)(void __iomem *addr, u32 offset);

	/* DSP I/DRAM IO */
	void (*ram_read)(struct hifi4dsp_dsp *dsp,
			 void  *dest, void __iomem *src, size_t bytes);
	void (*ram_write)(struct hifi4dsp_dsp *dsp,
			  void __iomem *dest, void *src, size_t bytes);
	void (*write64)(void __iomem *addr, u32 offset, u64 value);
	u64 (*read64)(void __iomem *addr, u32 offset);

	void (*dump)(struct hifi4dsp_dsp *dsp);

	/* IRQ handlers */
	irqreturn_t (*irq_handler)(int irq, void *context);

	/* hifi4dsp init and free */
	int (*init)(struct hifi4dsp_dsp *dsp, struct hifi4dsp_pdata *pdata);
	void (*free)(struct hifi4dsp_dsp *dsp);

	/* FW module parser/loader */
	int (*parse_fw)(struct hifi4dsp_firmware *dsp_fw, void *pinfo);
};

/*
 * hifi4dsp dsp device.
 */
struct hifi4dsp_dsp_device {
	struct hifi4dsp_ops *ops;
	irqreturn_t (*thread)(int irq, void *thread_context);
	void *thread_context;
};

/*
 * hifi4dsp Platform Data.
 */
struct hifi4dsp_pdata {
	int id;
	const char *name;
	int irq;
	unsigned int clk_freq;
	phys_addr_t	reg_paddr;
	unsigned int	reg_size;
	void *reg;

	/* Firmware */
	char fw_name[32];
	phys_addr_t	fw_paddr;   /*physical address of fw data*/
	void *fw_buf;
	int	 fw_size;
	int  fw_max_size;

	void *ops;
	void *dsp;		/*pointer to dsp*/
	void *priv;     /*pointer to priv*/
};

struct hifi4dsp_dsp {
	u32 id;
	int irq;
	int freq;
	/* runtime */
	spinlock_t spinlock;	/* used for IPC */
	struct mutex mutex;		/* used for fw */
	struct device *dev;
	u32 major_id;
	void *thread_context;

	struct hifi4dsp_dsp_device *dsp_dev;
	/* operations */
	struct hifi4dsp_ops *ops;

	/* base addresses */
	struct hifi4dsp_addr addr;

	/* platform data */
	struct hifi4dsp_pdata *pdata;

	/*fw support*/
	struct hifi4dsp_firmware *dsp_fw;/*def fw*/
	u32 fw_cnt;
	spinlock_t fw_spinlock; /*spinlock*/
	struct list_head fw_list;

	struct firmware *fw;

	struct clk *dsp_clk;
	struct clk *dsp_gate;

	void *info;
	void *priv;
};

/* Internal generic low-level hifi4dsp share memory write/read functions*/
void hifi4dsp_smem_write(void __iomem *addr, u32 offset, u32 value);
u32  hifi4dsp_smem_read(void __iomem *addr, u32 offset);
void hifi4dsp_smem_write64(void __iomem *addr, u32 offset, u64 value);
u64  hifi4dsp_smem_read64(void __iomem *addr, u32 offset);
void hifi4dsp_memcpy_toio_32(struct hifi4dsp_dsp *dsp,
			     void __iomem *dest,
			     void *src, size_t bytes);
void hifi4dsp_memcpy_fromio_32(struct hifi4dsp_dsp *dsp,
			       void *dest,
			       void __iomem *src,
			       size_t bytes);

void hifi4dsp_dsp_smem_write(struct hifi4dsp_dsp *dsp,
			     u32 offset, u32 value);
u32 hifi4dsp_dsp_smem_read(struct hifi4dsp_dsp *dsp, u32 offset);
void hifi4dsp_dsp_smem_write64(struct hifi4dsp_dsp *dsp,
			       u32 offset, u64 value);
u64 hifi4dsp_dsp_smem_read64(struct hifi4dsp_dsp *dsp,
			     u32 offset);
void hifi4dsp_dsp_smem_write_unlocked(struct hifi4dsp_dsp *dsp,
				      u32 offset, u32 value);
u32 hifi4dsp_dsp_smem_read_unlocked(struct hifi4dsp_dsp *dsp, u32 offset);
void hifi4dsp_dsp_smem_write64_unlocked(struct hifi4dsp_dsp *dsp,
					u32 offset, u64 value);
u64 hifi4dsp_dsp_smem_read64_unlocked(struct hifi4dsp_dsp *dsp,
				      u32 offset);
int hifi4dsp_dsp_smem_update_bits_unlocked
(struct hifi4dsp_dsp *dsp, u32 offset, u32 mask, u32 value);
void hifi4dsp_dsp_smem_update_bits_forced_unlocked
(struct hifi4dsp_dsp *dsp, u32 offset, u32 mask, u32 value);
int hifi4dsp_dsp_smem_update_bits64_unlocked
(struct hifi4dsp_dsp *dsp, u32 offset, u64 mask, u64 value);
int hifi4dsp_dsp_smem_update_bits(struct hifi4dsp_dsp *dsp,
				  u32 offset,
				  u32 mask,
				  u32 value);
void hifi4dsp_dsp_smem_update_bits_forced(struct hifi4dsp_dsp *dsp,
					  u32 offset,
					  u32 mask,
					  u32 value);
int hifi4dsp_dsp_smem_update_bits64(struct hifi4dsp_dsp *dsp,
				    u32 offset,
				    u64 mask,
				    u64 value);

int hifi4dsp_dsp_boot(struct hifi4dsp_dsp *dsp);
void hifi4dsp_dsp_reset(struct hifi4dsp_dsp *dsp);
void hifi4dsp_dsp_sleep(struct hifi4dsp_dsp *dsp);
int hifi4dsp_dsp_wake(struct hifi4dsp_dsp *dsp);
void hifi4dsp_dsp_dump(struct hifi4dsp_dsp *dsp);
//struct hifi4dsp_dsp * hifi4dsp_dsp_new(struct hifi4dsp_priv *priv,
//					 struct hifi4dsp_pdata *pdata,
//					 struct hifi4dsp_ops *ops);
struct hifi4dsp_dsp *hifi4dsp_dsp_new(struct hifi4dsp_priv *priv,
				      struct hifi4dsp_pdata *pdata,
				      struct hifi4dsp_dsp_device *dsp_dev);

void create_hifi4_syslog(void);
void hifi4_syslog_reomve(void);

#endif /*_HIFI4DSP_DSP_H*/
