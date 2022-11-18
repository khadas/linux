// SPDX-License-Identifier: GPL-2.0+
/*
 * Amlogic AXI PCIe endpoint controller driver
 *
 * Copyright (c) 2021 Amlogic, Inc.
 */

#include <linux/configfs.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/pci-epc.h>
#include <linux/platform_device.h>
#include <linux/pci-epf.h>
#include <linux/sizes.h>

#include <linux/of_gpio.h>

#include "pcie-amlogic-v3.h"

#define AML_PCI_MAX_RESOURCES	4

struct amlogic_pcie_ep {
	struct amlogic_pcie	amlogic;
	struct pci_epc		*epc;
	struct resource		*mem_res;
	struct gpio_desc	*reset_gpio;
	struct delayed_work	work;
	u32			max_regions;
	unsigned long		ob_region_map;
	phys_addr_t		*ob_addr;
	u8			max_functions;
};

static inline u32 amlogic_pcieinter_ep_read(struct amlogic_pcie *pcie, u32 reg)
{
	return readl(pcie->ecam_base + reg);
}

static inline void amlogic_pcieinter_ep_write(struct amlogic_pcie *pcie,
					      u32 val,
					      u32 reg)
{
	writel(val, pcie->ecam_base + EP_BASE_OFFSET + reg);
}

static int amlogic_pcie_ep_write_header(struct pci_epc *epc, u8 fn,
					struct pci_epf_header *hdr)
{
	struct amlogic_pcie_ep *ep = epc_get_drvdata(epc);
	struct amlogic_pcie *amlogic = &ep->amlogic;
	u32 val;

	/* All functions share the same vendor ID with function 0 */
	if (!fn)
		val = hdr->vendorid;
	else
		val = amlogic_pcieinter_read(amlogic, PCIE_PCI_IDS0);

	val |= hdr->deviceid << 16;
	amlogic_pcieinter_write(amlogic, val, PCIE_PCI_IDS0);

	val = hdr->revid;
	val |= hdr->progif_code << 8;
	val |= hdr->subclass_code << 16;
	val |= hdr->baseclass_code << 24;
	amlogic_pcieinter_write(amlogic, val, PCIE_PCI_IDS1);

	if (!fn)
		val = hdr->subsys_vendor_id;
	else
		val = amlogic_pcieinter_read(amlogic, PCIE_PCI_IDS2);
	val |= hdr->subsys_id << 16;
	amlogic_pcieinter_write(amlogic, val, PCIE_PCI_IDS2);

	if (hdr->interrupt_pin > PCI_INTERRUPT_INTD)
		return -EINVAL;
	val = amlogic_pcieinter_read(amlogic, PCIE_PCI_IRQ);
	val &= (~GENMASK(2, 0));
	val |= hdr->interrupt_pin;
	amlogic_pcieinter_write(amlogic, val, PCIE_PCI_IRQ);

	return 0;
}

static int amlogic_pcie_ep_set_bar(struct pci_epc *epc, u8 fn,
				   struct pci_epf_bar *epf_bar)
{
	struct amlogic_pcie_ep *ep = epc_get_drvdata(epc);
	struct amlogic_pcie *amlogic = &ep->amlogic;
	dma_addr_t cpu_addr = epf_bar->phys_addr;
	enum pci_barno bar = epf_bar->barno;
	int flags = epf_bar->flags;
	u64 size = 1ULL << fls64(epf_bar->size - 1);
	u64 reg = 0;
	int trsl_param = 0, atr_size = 0;
	bool is_prefetch = !!(flags & PCI_BASE_ADDRESS_MEM_PREFETCH);
	bool is_64bits = size > SZ_4G;

	atr_size = ilog2(size);

	if (((flags & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_IO)) {
		reg =  PCIE_BAR_IO;
	} else {
		if (is_64bits && is_prefetch) {
			reg |= PCIE_BAR_MEM_TYPE_64 |
				PCIE_BAR_MEM_TYPE_PREFETCH |
				PCIE_BAR_MEM_MASK;
			amlogic_pcie_ep_fn_writel(amlogic, fn,
						  PCIE_BAR_0 + bar * 4,
						  lower_32_bits(reg));
			bar += 1;
			amlogic_pcie_ep_fn_writel(amlogic, fn,
						  PCIE_BAR_0 + bar * 4,
						  upper_32_bits(reg));
		} else if (is_prefetch) {
			reg |= PCIE_BAR_MEM_TYPE_PREFETCH;
			amlogic_pcie_ep_fn_writel(amlogic, fn,
						  PCIE_BAR_0 + bar * 4,
						  lower_32_bits(reg));
			bar += 1;
			amlogic_pcie_ep_fn_writel(amlogic, fn,
						  PCIE_BAR_0 + bar * 4,
						  lower_32_bits(reg));

		} else if (is_64bits) {
			reg |= PCIE_BAR_MEM_TYPE_64;
			amlogic_pcie_ep_fn_writel(amlogic, fn,
						  PCIE_BAR_0 + bar * 4,
						  lower_32_bits(reg));
			bar += 1;
			amlogic_pcie_ep_fn_writel(amlogic, fn,
						  PCIE_BAR_0 + bar * 4,
						  upper_32_bits(reg));
		} else {
		}
		trsl_param = ATR_TRSLID_AXIMEMORY;
	}

	amlogic_pcie_ep_fn_writel(amlogic, fn, PCIE_BAR_0 + bar * 4, reg);

	amlogic_pcie_cfg_addr_map(amlogic, ATR_PCIE_WIN0 +
				  ATR_TABLE_SIZE * (bar - 2),
				  0, cpu_addr,
				  atr_size, trsl_param);

	return 0;
}

static void amlogic_pcie_ep_clear_bar(struct pci_epc *epc, u8 fn,
				       struct pci_epf_bar *epf_bar)
{
	struct amlogic_pcie_ep *ep = epc_get_drvdata(epc);
	struct amlogic_pcie *amlogic = &ep->amlogic;
	enum pci_barno bar = epf_bar->barno;

	amlogic_pcieinter_write(amlogic, 0,
				ATR_PCIE_WIN0 + ATR_TABLE_SIZE * (bar - 2) +
				ATR_SRC_ADDR_LOW);
}

static int amlogic_pcie_ep_map_addr(struct pci_epc *epc, u8 fn,
				    phys_addr_t addr, u64 pci_addr,
				    size_t size)
{
	struct amlogic_pcie_ep *ep = epc_get_drvdata(epc);
	struct amlogic_pcie *amlogic = &ep->amlogic;
	u32 r;

	/*
	 * find_first_zero_bit() is an arch-specific function controlled by marco find_first_bit.
	 */
	/* coverity[callee_ptr_arith:SUPPRESS] */
	r = find_first_zero_bit(&ep->ob_region_map,
				sizeof(ep->ob_region_map) * BITS_PER_LONG);
	if (r >= ep->max_regions - 1) {
		dev_err(&epc->dev, "no free outbound region\n");
		return -EINVAL;
	}

	amlogic_pcie_cfg_addr_map(amlogic,
				  ATR_AXI4_SLV0 + ATR_TABLE_SIZE * (r + 1),
				  addr, pci_addr,
				  ilog2(size), ATR_TRSLID_PCIE_MEMORY);

	set_bit(r, &ep->ob_region_map);
	ep->ob_addr[r] = addr;

	return 0;
}

static void amlogic_pcie_ep_unmap_addr(struct pci_epc *epc, u8 fn,
					phys_addr_t addr)
{
	struct amlogic_pcie_ep *ep = epc_get_drvdata(epc);
	struct amlogic_pcie *amlogic = &ep->amlogic;
	u32 r;

	for (r = 0; r < ep->max_regions - 1; r++)
		if (ep->ob_addr[r] == addr)
			break;

	if (r == ep->max_regions - 1)
		return;

	amlogic_pcieinter_write(amlogic, 0,
				ATR_AXI4_SLV0 + ATR_TABLE_SIZE * (r + 1) +
				ATR_SRC_ADDR_LOW);

	ep->ob_addr[r] = 0;
	clear_bit(r, &ep->ob_region_map);
}

static int amlogic_pcie_ep_set_msi(struct pci_epc *epc, u8 fn,
				   u8 mmc)
{
	struct amlogic_pcie_ep *ep = epc_get_drvdata(epc);
	struct amlogic_pcie *amlogic = &ep->amlogic;
	u16 flags;

	/* Multiple Message Capable BIT[3:1] is Ro,
	 * so this write invalid. Default is 32 vectors requested
	 */
	flags = amlogic_pcie_ep_fn_readw(amlogic, fn,
					 EP_FUNC_MSI_CAP_OFFSET +
					 PCI_MSI_FLAGS);
	flags = (flags & ~PCI_MSI_FLAGS_QMASK) | (mmc << 1);
	flags |= PCI_MSI_FLAGS_64BIT;
	flags &= ~PCI_MSI_FLAGS_MASKBIT;
	amlogic_pcie_ep_fn_writew(amlogic, fn,
				  EP_FUNC_MSI_CAP_OFFSET +
				  PCI_MSI_FLAGS, flags);

	return 0;
}

static int amlogic_pcie_ep_get_msi(struct pci_epc *epc, u8 fn)
{
	struct amlogic_pcie_ep *ep = epc_get_drvdata(epc);
	struct amlogic_pcie *amlogic = &ep->amlogic;
	u16 flags, mme;

	flags = amlogic_pcie_ep_fn_readw(amlogic, fn,
					 EP_FUNC_MSI_CAP_OFFSET +
					 PCI_MSI_FLAGS);
	if (!(flags & PCI_MSI_FLAGS_ENABLE))
		return -EINVAL;

	mme = ((flags & PCI_MSI_FLAGS_QSIZE) >> 4);

	return mme;
}

static int amlogic_pcie_ep_send_legacy_irq(struct amlogic_pcie_ep *ep, u8 fn,
					   u8 intx)
{
	struct amlogic_pcie *amlogic = &ep->amlogic;
	u16 cmd;
	u32 val = 0;

	amlogic_pcieinter_write(amlogic, 0xffffffff, IMASK_HOST);

	cmd = amlogic_pcie_ep_fn_readw(&ep->amlogic, fn,
				       PCI_COMMAND);

	if (cmd & PCI_COMMAND_INTX_DISABLE)
		return -EINVAL;

	val = amlogic_pciectrl_read(&ep->amlogic, PCIE_A_CTRL0);
	val |= INT_INTX_MASK;
	amlogic_pciectrl_write(&ep->amlogic, val, PCIE_A_CTRL0);
	mdelay(1);
	val = amlogic_pcieinter_read(&ep->amlogic, ISTATUS_HOST);
	val |= INT_INTX_MASK;
	amlogic_pcieinter_write(&ep->amlogic, val, ISTATUS_HOST);

	amlogic_pcieinter_write(amlogic, 0xffffffff, IMASK_HOST);

	return 0;
}

static int amlogic_pcie_ep_send_msi_irq(struct amlogic_pcie_ep *ep, u8 fn,
					 u8 interrupt_num)
{
	struct amlogic_pcie *amlogic = &ep->amlogic;
	u32 val = 0;

	amlogic_pcieinter_write(amlogic, 0xffffffff, IMASK_HOST);

	val = amlogic_pciectrl_read(&ep->amlogic, PCIE_A_CTRL0);
	val |= INT_MSI;
	amlogic_pciectrl_write(&ep->amlogic, val, PCIE_A_CTRL0);
	val = amlogic_pcieinter_read(&ep->amlogic, ISTATUS_HOST);
	val |= INT_MSI;
	amlogic_pcieinter_write(&ep->amlogic, val, ISTATUS_HOST);

	amlogic_pcieinter_write(amlogic, 0xffffffff, IMASK_HOST);

	return 0;
}

static int amlogic_pcie_ep_raise_irq(struct pci_epc *epc, u8 fn,
				     enum pci_epc_irq_type type,
				     u16 interrupt_num)
{
	struct amlogic_pcie_ep *ep = epc_get_drvdata(epc);

	switch (type) {
	case PCI_EPC_IRQ_LEGACY:
		return amlogic_pcie_ep_send_legacy_irq(ep, fn, 0);
	case PCI_EPC_IRQ_MSI:
		return amlogic_pcie_ep_send_msi_irq(ep, fn, interrupt_num);
	default:
		return -EINVAL;
	}
}

static int amlogic_pcie_ep_init_port(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	u32 regs;
	u32 val;

	val = readl(amlogic->rst_base + RESETCTRL1_OFFSET);
	val &= ~(1 << amlogic->m31phy_rst_bit);
	writel(val, amlogic->rst_base + RESETCTRL1_OFFSET);
	val = readl(amlogic->rst_base + RESETCTRL1_OFFSET);
	val |= (1 << amlogic->m31phy_rst_bit);
	writel(val, amlogic->rst_base + RESETCTRL1_OFFSET);

	regs = readl(amlogic->phy_base + 0x470);
	regs |= (1 << 6);
	writel(regs, amlogic->phy_base + 0x470);

	/*set phy for gen3 device*/
	regs = readl(amlogic->phy_base);
	regs |= BIT(19);
	writel(regs, amlogic->phy_base);

	regs = amlogic_pciectrl_read(amlogic, PCIE_A_CTRL0);

	if (amlogic->is_rc)
		regs |= PORT_TYPE;
	else
		regs &= ~PORT_TYPE;
	amlogic_pciectrl_write(amlogic, regs, PCIE_A_CTRL0);

	if (!amlogic_pcie_link_up(amlogic))
		return -ETIMEDOUT;
	regs = amlogic_pcieinter_read(amlogic, PCIE_BASIC_STATUS);

	dev_info(dev, "current linK speed is GEN%d,link width is x%d\n",
		 ((regs >> 8) & 0x3f), (regs & 0xff));

	return 0;
}

static void amlogic_reset_gpio_irq_work(struct work_struct *work)
{
	struct amlogic_pcie_ep *ep = container_of(work,
						  struct amlogic_pcie_ep,
						  work.work);
	struct amlogic_pcie *amlogic = &ep->amlogic;

	amlogic_pcie_ep_init_port(amlogic);
}

static int amlogic_pcie_ep_start(struct pci_epc *epc)
{
	struct amlogic_pcie_ep *ep = epc_get_drvdata(epc);

	INIT_DELAYED_WORK(&ep->work, amlogic_reset_gpio_irq_work);

	return 0;
}

static const struct pci_epc_features amlogic_pcie_epc_features = {
	.linkup_notifier = false,
	.msi_capable = true,
	.msix_capable = false,
	.reserved_bar = 1 << BAR_0 | 1 << BAR_1,
	.bar_fixed_64bit = 1 << BAR_0,
	.bar_fixed_size[2] = 256,
};

static const struct pci_epc_features*
amlogic_pcie_ep_get_features(struct pci_epc *epc, u8 func_no)
{
	return &amlogic_pcie_epc_features;
}

static const struct pci_epc_ops amlogic_pcie_epc_ops = {
	.write_header	= amlogic_pcie_ep_write_header,
	.set_bar	= amlogic_pcie_ep_set_bar,
	.clear_bar	= amlogic_pcie_ep_clear_bar,
	.map_addr	= amlogic_pcie_ep_map_addr,
	.unmap_addr	= amlogic_pcie_ep_unmap_addr,
	.set_msi	= amlogic_pcie_ep_set_msi,
	.get_msi	= amlogic_pcie_ep_get_msi,
	.raise_irq	= amlogic_pcie_ep_raise_irq,
	.start		= amlogic_pcie_ep_start,
	.get_features	= amlogic_pcie_ep_get_features,
};

static int amlogic_pcie_parse_ep_dt(struct amlogic_pcie *amlogic,
				    struct amlogic_pcie_ep *ep)
{
	struct device *dev = amlogic->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct resource *res;
	int err;

	err = amlogic_pcie_parse_dt(amlogic);
	if (err)
		return err;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mem0");
	if (!res) {
		dev_err(dev, "missing \"mem0\"\n");
		return -EINVAL;
	}
	ep->mem_res = res;

	ep->max_regions = AML_PCI_MAX_RESOURCES;
	of_property_read_u32(dev->of_node, "max-outbound-regions",
			     &ep->max_regions);

	ep->ob_addr = devm_kcalloc(dev,
				   ep->max_regions, sizeof(*ep->ob_addr),
				   GFP_KERNEL);
	if (!ep->ob_addr)
		return -ENOMEM;

	err = of_property_read_u8(dev->of_node, "max-functions",
				  &ep->max_functions);
	if (err < 0 || ep->max_functions > 1)
		ep->max_functions = 1;

	return 0;
}

static irqreturn_t amlogic_reset_gpio_handler(int irq, void *data)
{
	struct amlogic_pcie_ep *ep = data;

	schedule_delayed_work(&ep->work, 0);

	return IRQ_HANDLED;
}

static const struct of_device_id amlogic_pcie_ep_of_match[] = {
	{ .compatible = "amlogic,amlogic-pcie-ep"},
	{ .compatible = "amlogic, amlogic-pcie-ep"},
	{},
};

static int amlogic_pcie_ep_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct amlogic_pcie_ep *ep;
	struct amlogic_pcie *amlogic;
	struct pci_epc *epc;
	int err;

	ep = devm_kzalloc(dev, sizeof(*ep), GFP_KERNEL);
	if (!ep)
		return -ENOMEM;

	amlogic = &ep->amlogic;
	amlogic->is_rc = false;
	amlogic->dev = dev;

	err = amlogic_pcie_parse_ep_dt(amlogic, ep);
	if (err)
		return err;

	ep->reset_gpio = gpio_to_desc(amlogic->reset_gpio);

	err = devm_request_threaded_irq(dev, gpiod_to_irq(ep->reset_gpio),
					NULL, amlogic_reset_gpio_handler,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"amlogic-pcie-v3-ep-reset", ep);

	epc = devm_pci_epc_create(dev, &amlogic_pcie_epc_ops);
	if (IS_ERR(epc)) {
		dev_err(dev, "failed to create epc device\n");
		return PTR_ERR(epc);
	}

	ep->epc = epc;
	epc->max_functions = ep->max_functions;
	epc_set_drvdata(epc, ep);

	err = amlogic_pcie_enable_clocks(amlogic);
	if (err)
		return err;

	err = pci_epc_mem_init(epc, ep->mem_res->start,
			       resource_size(ep->mem_res));
	if (err < 0) {
		dev_err(dev, "failed to initialize the memory space\n");
		goto err_disable_clocks;
	}

	return 0;

err_disable_clocks:
	amlogic_pcie_deinit_phys(amlogic);
	amlogic_pcie_disable_clocks(amlogic);
	return err;
}

static struct platform_driver amlogic_pcie_ep_driver = {
	.driver = {
		.name = "amlogic-pcie-ep",
		.of_match_table = amlogic_pcie_ep_of_match,
	},
	.probe = amlogic_pcie_ep_probe,
};

builtin_platform_driver(amlogic_pcie_ep_driver);
