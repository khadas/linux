/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __DEPEND_H__
#define __DEPEND_H__

#include <linux/device.h>	/**/


/*dma_alloc_from_contiguous*/
struct page *aml_dma_alloc_contiguous(struct device *dev, int count,
						unsigned int order);
/*dma_release_from_contiguous*/
bool aml_dma_release_contiguous(struct device *dev, struct page *pages,
						int count);

struct aml_exp_func {
	int (*leave_mode)(enum fe_delivery_system delsys);
};

#endif	/*__DEPEND_H__*/
