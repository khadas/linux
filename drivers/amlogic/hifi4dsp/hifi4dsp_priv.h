/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _HIFI4DSP_PRIV_H
#define _HIFI4DSP_PRIV_H
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/miscdevice.h>

/*
 * #include <asm/dsp/hifi4dsp_control.h>
 * #include <asm/dsp/dsp_register.h>
 */
#include <linux/dma-mapping.h>

#include "hifi4dsp_api.h"
#include "hifi4dsp_dsp.h"
#include "hifi4dsp_firmware.h"

struct reg_iomem_t {
	void __iomem *dspa_addr;
	unsigned int	rega_size;
	void __iomem *dspb_addr;
	unsigned int	regb_size;
	void __iomem *hiu_addr;		/*HIU*/
	void __iomem *sram_base;
};

extern struct reg_iomem_t g_regbases;

struct class;

struct hifi4dsp_priv {
	char name[12];
	struct class  *class;
	struct device *dev;

	u32  dsp_freq;
	//bool dsp_is_started;

	struct hifi4dsp_dsp *dsp;
	struct hifi4dsp_dsp_device	*dsp_dev;
	struct hifi4dsp_firmware	*dsp_fw;
	struct hifi4dsp_pdata		*pdata;

	struct clk *p_clk;
	struct clk *p_clk_gate;
};

#define	HIFI4DSP_MAX_CNT	2
extern struct hifi4dsp_priv *hifi4dsp_p[HIFI4DSP_MAX_CNT];

struct hifi4dsp_miscdev_t {
	struct miscdevice dsp_miscdev;
	struct hifi4dsp_priv *priv;
};

#ifndef HIFI4DSP_PRNT
#define HIFI4DSP_PRNT(...)  pr_info(__VA_ARGS__)
#endif

/*power ctrl*/
#define PWR_ON    1
#define PWR_OFF   0

#endif /*_HIFI4DSP_PRIV_H*/
