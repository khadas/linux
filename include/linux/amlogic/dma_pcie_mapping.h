/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

extern const struct dma_map_ops aml_pcie_dma_ops;

void  pcie_swiotlb_init(struct device *dma_dev);

int aml_dma_atomic_pool_init(struct device *dev);
