// SPDX-License-Identifier: GPL-2.0
/*
 * IOMMU API for ARM architected SMMUv3 implementations.
 *
 * Copyright (C) 2015 ARM Limited
 *
 * Author: Will Deacon <will.deacon@arm.com>
 *
 * This driver is powered by bad coffee and bombay mix.
 */

#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/dma-iommu.h>
#include <linux/err.h>
#include <linux/iommu.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_iommu.h>
#include <linux/of_platform.h>
#include <linux/pci.h>
#include <linux/pci-ats.h>
#include <linux/platform_device.h>
#include <linux/of_reserved_mem.h>

#include <linux/amba/bus.h>
#ifdef CONFIG_AMLOGIC_MODIFY
#include <dt-bindings/memory/meson-p1-sid-map.h>
#include <linux/arm-smccc.h>
#endif
#include <trace/hooks/iommu.h>
#include <linux/amlogic/dma_pcie_mapping.h>
#include <linux/amlogic/tee.h>

struct aml_smmu_ll_queue {
	union {
		u64			val;
		struct {
			u32		prod;
			u32		cons;
		};
		struct {
			atomic_t	prod;
			atomic_t	cons;
		} atomic;
		u8			__pad[SMP_CACHE_BYTES];
	} ____cacheline_aligned_in_smp;
	u32				max_n_shift;
};

struct aml_smmu_queue {
	struct aml_smmu_ll_queue	llq;
	int				irq; /* Wired interrupt */

	__le64				*base;
	dma_addr_t			base_dma;
	u64				q_base;

	size_t				ent_dwords;

	u32 __iomem			*prod_reg;
	u32 __iomem			*cons_reg;
};

struct aml_smmu_cmdq {
	struct aml_smmu_queue		q;
	atomic_long_t			*valid_map;
	atomic_t			owner_prod;
	atomic_t			lock;
};

struct aml_smmu_evtq {
	struct aml_smmu_queue		q;
	u32				max_stalls;
};

struct aml_smmu_priq {
	struct aml_smmu_queue		q;
};

/* High-level stream table and context descriptor structures */
struct aml_smmu_strtab_l1_desc {
	u8				span;

	__le64				*l2ptr;
	dma_addr_t			l2ptr_dma;
};

struct aml_smmu_strtab_cfg {
	__le64				*strtab;
	dma_addr_t			strtab_dma;
	struct aml_smmu_strtab_l1_desc	*l1_desc;
	unsigned int			num_l1_ents;

	u64				strtab_base;
	u32				strtab_base_cfg;
};

/* An SMMUv3 instance */
struct aml_smmu_device {
	struct device			*dev;
	void __iomem			*base;

	u32				features;

	u32				options;

	struct aml_smmu_cmdq		cmdq;
	struct aml_smmu_evtq		evtq;
	struct aml_smmu_priq		priq;

	int				gerr_irq;
	int				combined_irq;

	unsigned long			ias; /* IPA */
	unsigned long			oas; /* PA */
	unsigned long			pgsize_bitmap;

#define ARM_SMMU_MAX_ASIDS		(1 << 16)
	unsigned int			asid_bits;
	DECLARE_BITMAP(asid_map, ARM_SMMU_MAX_ASIDS);

#define ARM_SMMU_MAX_VMIDS		(1 << 16)
	unsigned int			vmid_bits;
	DECLARE_BITMAP(vmid_map, ARM_SMMU_MAX_VMIDS);

	unsigned int			ssid_bits;
	unsigned int			sid_bits;

	struct aml_smmu_strtab_cfg	strtab_cfg;

	/* IOMMU core code handle */
	struct iommu_device		iommu;
};

static int aml_smmu_add_device(struct device *dev)
{
	return 0;
}

static int aml_smmu_of_xlate(struct device *dev, struct of_phandle_args *args)
{
	return 0;
}

static struct iommu_ops aml_smmu_ops = {
	.add_device	= aml_smmu_add_device,
	.of_xlate	= aml_smmu_of_xlate,
};

static inline int aml_smmu_device_acpi_probe(struct platform_device *pdev,
					     struct aml_smmu_device *smmu)
{
	return -ENODEV;
}

static int aml_smmu_device_dt_probe(struct platform_device *pdev,
				    struct aml_smmu_device *smmu)
{
	struct device *dev = &pdev->dev;
	u32 cells;
	int ret = -EINVAL;

	if (of_property_read_u32(dev->of_node, "#iommu-cells", &cells))
		dev_err(dev, "missing #iommu-cells property\n");
	else if (cells != 1)
		dev_err(dev, "invalid #iommu-cells value (%d)\n", cells);
	else
		ret = 0;

	return ret;
}

static int aml_smmu_set_bus_ops(struct iommu_ops *ops)
{
	int err;

#ifdef CONFIG_PCI
	if (pci_bus_type.iommu_ops != ops) {
		err = bus_set_iommu(&pci_bus_type, ops);
		if (err)
			return err;
	}
#endif
#ifdef CONFIG_ARM_AMBA
	if (amba_bustype.iommu_ops != ops) {
		err = bus_set_iommu(&amba_bustype, ops);
		if (err)
			goto err_reset_pci_ops;
	}
#endif
	if (platform_bus_type.iommu_ops != ops) {
		err = bus_set_iommu(&platform_bus_type, ops);
		if (err)
			goto err_reset_amba_ops;
	}

	return 0;

err_reset_amba_ops:
#ifdef CONFIG_ARM_AMBA
	bus_set_iommu(&amba_bustype, NULL);
#endif
err_reset_pci_ops: __maybe_unused;
#ifdef CONFIG_PCI
	bus_set_iommu(&pci_bus_type, NULL);
#endif
	return err;
}

void set_dma_ops_hook(void *data, struct device *dev, u64 dma_base, u64 size)
{
	set_dma_ops(dev, &aml_pcie_dma_ops);
}

static int aml_smmu_device_probe(struct platform_device *pdev)
{
	int ret;
	resource_size_t ioaddr;
	struct aml_smmu_device *smmu;
	struct device *dev = &pdev->dev;
	struct reserved_mem *rmem = NULL;
	struct device_node *mem_node;
	u32 handle;

	smmu = devm_kzalloc(dev, sizeof(*smmu), GFP_KERNEL);
	if (!smmu) {
		dev_err(dev, "failed to allocate amlogic smmu\n");
		return -ENOMEM;
	}
	smmu->dev = dev;

	if (dev->of_node) {
		ret = aml_smmu_device_dt_probe(pdev, smmu);
		if (ret == -EINVAL)
			return ret;
	} else {
		ret = aml_smmu_device_acpi_probe(pdev, smmu);
		if (ret == -ENODEV)
			return ret;
	}

	/* Record our private device structure */
	platform_set_drvdata(pdev, smmu);

	ret = iommu_device_sysfs_add(&smmu->iommu, dev, NULL,
				     "smmu3.%pa", &ioaddr);
	if (ret)
		return ret;

	iommu_device_set_ops(&smmu->iommu, &aml_smmu_ops);
	iommu_device_set_fwnode(&smmu->iommu, dev->fwnode);

	ret = iommu_device_register(&smmu->iommu);
	if (ret) {
		dev_err(dev, "Failed to register iommu\n");
		return ret;
	}

	ret = of_reserved_mem_device_init(dev);
	if (ret) {
		dev_err(dev, "reserve memory init fail:%d\n", ret);
		return ret;
	}

	mem_node = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (!mem_node) {
		dev_err(dev, "parse memory region failed.\n");
		return -1;
	}
	rmem = of_reserved_mem_lookup(mem_node);
	of_node_put(mem_node);
	if (rmem) {
		dev_info(dev, "tee protect memory: %lu MiB at 0x%lx\n",
			(unsigned long)rmem->size / SZ_1M, (unsigned long)rmem->base);
		ret = tee_protect_mem_by_type(TEE_MEM_TYPE_PCIE,
				rmem->base, rmem->size, &handle);
		if (ret) {
			dev_err(dev, "pcie tee mem protect fail: 0x%x\n", ret);
			return -1;
		}
	} else {
		dev_err(dev, "Can't get reserve memory region\n");
		return -1;
	}

	pcie_swiotlb_init(dev);
	aml_dma_atomic_pool_init(dev);
	register_trace_android_vh_iommu_setup_dma_ops(set_dma_ops_hook, NULL);
	//unregister_trace_android_vh_iommu_setup_dma_ops(set_dma_ops_hook, NULL);

	return aml_smmu_set_bus_ops(&aml_smmu_ops);
}

static int aml_smmu_device_remove(struct platform_device *pdev)
{
	struct aml_smmu_device *smmu = platform_get_drvdata(pdev);

	aml_smmu_set_bus_ops(NULL);
	iommu_device_unregister(&smmu->iommu);
	iommu_device_sysfs_remove(&smmu->iommu);

	return 0;
}

static void aml_smmu_device_shutdown(struct platform_device *pdev)
{
	aml_smmu_device_remove(pdev);
}

static const struct of_device_id aml_smmu_of_match[] = {
	{ .compatible = "amlogic,smmu", },
	{ },
};
MODULE_DEVICE_TABLE(of, aml_smmu_of_match);

static struct platform_driver aml_smmu_driver = {
	.driver	= {
		.name			= "aml_smmu",
		.of_match_table		= of_match_ptr(aml_smmu_of_match),
		.suppress_bind_attrs	= true,
	},
	.probe	= aml_smmu_device_probe,
	.remove	= aml_smmu_device_remove,
	.shutdown = aml_smmu_device_shutdown,
};

//module_platform_driver(aml_smmu_driver);
static int __init aml_smmu_init(void)
{
	return platform_driver_register(&aml_smmu_driver);
}
core_initcall(aml_smmu_init);

MODULE_LICENSE("GPL v2");
