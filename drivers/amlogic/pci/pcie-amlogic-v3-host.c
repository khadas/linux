// SPDX-License-Identifier: GPL-2.0+
/*
 * Amlogic AXI PCIe host controller driver
 *
 * Copyright (c) 2021 Amlogic, Inc.
 */

#include <linux/bitrev.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/irq.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_pci.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/regmap.h>

#include "../../pci/pci.h"
#include "pcie-amlogic-v3.h"

static int pcie_test;
module_param(pcie_test, int, 0444);
MODULE_PARM_DESC(pcie_test, "Facilitate pcie hardware signal test");

static int link_speed;
module_param(link_speed, int, 0444);
MODULE_PARM_DESC(link_speed, "select pcie link speed ");

static int link_times = WAIT_LINKUP_TIMEOUT - 10;
module_param(link_times, int, 0644);
MODULE_PARM_DESC(link_times, "select pcie link speed ");

/* MSI information */
struct amlogic_msi {
	DECLARE_BITMAP(used, INT_PCI_MSI_NR);
	struct irq_domain *msi_domain;
	struct irq_domain *inner_domain;
	int irq;
	struct mutex lock;		/* protect bitmap variable */
};

struct amlogic_pcie_rc {
	struct amlogic_pcie amlogic;
	struct pci_bus *root_bus;
	struct amlogic_msi msi;
	int inta_virq;
	int intb_virq;
	int intc_virq;
	int intd_virq;
	u8 root_bus_nr;
};

static void amlogic_pcie_free_irq_domain(struct amlogic_pcie_rc *rc);

static bool amlogic_pcie_valid_device(struct pci_bus *bus, unsigned int devfn)
{
	struct amlogic_pcie_rc *rc = bus->sysdata;
	struct amlogic_pcie *pcie = &rc->amlogic;

	/* Check link before accessing downstream ports */
	if (bus->number != rc->root_bus_nr) {
		if (!amlogic_pcie_link_up(pcie))
			return false;
	}

	/* Only one device down on each root port */
	if (bus->number == rc->root_bus_nr && devfn > 0)
		return false;

	return true;
}

static void __iomem *amlogic_pcie_map_bus(struct pci_bus *bus,
					  unsigned int devfn,
					  int where)
{
	struct amlogic_pcie_rc *rc = bus->sysdata;
	struct amlogic_pcie *pcie = &rc->amlogic;

	if (!amlogic_pcie_valid_device(bus, devfn))
		return NULL;

	return pcie->ecam_base +
		PCIE_ECAM_ADDR(bus->number, devfn, devfn, where);
}

static struct pci_ops amlogic_pcie_ops = {
	.map_bus = amlogic_pcie_map_bus,
	.read = pci_generic_config_read,
	.write = pci_generic_config_write,
};

static irqreturn_t amlogic_pcie_handle_msi_irq(struct amlogic_pcie_rc *rc)
{
	struct amlogic_pcie *amlogic = &rc->amlogic;
	struct device *dev = amlogic->dev;
	struct amlogic_msi *msi = &rc->msi;
	irqreturn_t ret = IRQ_NONE;
	u32 bit;
	u32 virq;
	unsigned long status = amlogic_pcieinter_read(amlogic, ISTATUS_MSI);

	for_each_set_bit(bit, &status, INT_PCI_MSI_NR) {
	/* clear interrupts */
		amlogic_pcieinter_write(amlogic, 1 << bit, ISTATUS_MSI);

		virq = irq_find_mapping(msi->inner_domain, bit);
		if (virq) {
			if (test_bit(bit, msi->used)) {
				ret = IRQ_HANDLED;
				generic_handle_irq(virq);
			} else {
				dev_err(dev,
					"unhandled MSI, MSI%d virq %d\n", bit,
					virq);
			}
		} else {
			dev_err(dev, "unexpected MSI, MSI%d\n", bit);
		}
	}
	amlogic_pcieinter_write(amlogic, INT_MSI, ISTATUS_LOCAL);

	return ret;
}

static void amlogic_pcie_msi_handler(struct irq_desc *desc)
{
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct amlogic_pcie_rc *rc = irq_desc_get_handler_data(desc);
	struct amlogic_pcie *amlogic = &rc->amlogic;
	u32 status;

	chained_irq_enter(chip, desc);

	status = (amlogic_pcieinter_read(amlogic, ISTATUS_LOCAL) & INT_MASK);
	while (status) {
		if (status & INT_MSI)
			amlogic_pcie_handle_msi_irq(rc);

		status = (amlogic_pcieinter_read(amlogic, ISTATUS_LOCAL) &
			  INT_MASK);
	}
	chained_irq_exit(chip, desc);
}

static int amlogic_pcie_enable_msi(struct amlogic_pcie_rc *rc)
{
	struct amlogic_pcie *amlogic = &rc->amlogic;
	struct device *dev = amlogic->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct amlogic_msi *msi = &rc->msi;
	u32 reg;

	mutex_init(&msi->lock);

	msi->irq = platform_get_irq(pdev, 0);
	if (msi->irq < 0) {
		dev_err(dev, "failed to get IRQ: %d\n", msi->irq);
		return msi->irq;
	}

	irq_set_chained_handler_and_data(msi->irq,
					 amlogic_pcie_msi_handler,
					 rc);

	/* Enable MSI */
	reg = amlogic_pcieinter_read(amlogic, IMASK_LOCAL);
	reg |= INT_MSI;
	amlogic_pcieinter_write(amlogic, reg, IMASK_LOCAL);

	return 0;
}

static int amlogic_pcie_host_init_port(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	int err = 0;
	u32 val = 0;

	err = amlogic_pcie_init_port(amlogic);
	if (err) {
		dev_err(dev, "failed init port\n");
		return err;
	}

	/* Setup RC BARs */
	amlogic_pcieinter_write(amlogic, 0x1,
				PCI_CFG_SPACE + PCI_BASE_ADDRESS_0);
	amlogic_pcieinter_write(amlogic, 0,
				PCI_CFG_SPACE + PCI_BASE_ADDRESS_1);

	/* Setup interrupt pins */
	val = amlogic_pcieinter_read(amlogic,
				     PCI_CFG_SPACE + PCI_INTERRUPT_LINE);
	val &= 0xffff00ff;
	val |= 0x00000100;
	amlogic_pcieinter_write(amlogic, val,
				PCI_CFG_SPACE + PCI_INTERRUPT_LINE);

	/* Setup bus numbers */
	val = amlogic_pcieinter_read(amlogic,
				     PCI_CFG_SPACE + PCI_PRIMARY_BUS);
	val &= 0xff000000;
	val |= 0x00ff0100;
	amlogic_pcieinter_write(amlogic, val,
				PCI_CFG_SPACE + PCI_PRIMARY_BUS);

	/* Setup command register */
	val = amlogic_pcieinter_read(amlogic,
				     PCI_CFG_SPACE + PCI_COMMAND);
	val &= 0xffff0000;
	val |= PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
		PCI_COMMAND_MASTER | PCI_COMMAND_SERR;
	amlogic_pcieinter_write(amlogic, val,
				PCI_CFG_SPACE + PCI_COMMAND);

	/* Program correct class for RC */
	val = amlogic_pcieinter_read(amlogic, PCIE_PCI_IDS1);
	val &= 0x0000ffff;
	val |= (PCI_CLASS_BRIDGE_PCI << 16);
	amlogic_pcieinter_write(amlogic, val, PCIE_PCI_IDS1);

	amlogic_set_max_payload(amlogic, 128);
	amlogic_set_max_rd_req_size(amlogic, 128);

	return err;
}

static void amlogic_pcie_intx_handler(struct irq_desc *desc)
{
	struct amlogic_pcie_rc *rc = irq_desc_get_handler_data(desc);
	struct amlogic_pcie *amlogic = &rc->amlogic;
	u32 mask = 0;
	u32 reg;

	if (rc->inta_virq == irq_desc_get_irq(desc))
		mask = INTA;
	else if (rc->intb_virq == irq_desc_get_irq(desc))
		mask = INTB;
	else if (rc->intc_virq == irq_desc_get_irq(desc))
		mask = INTC;
	else if (rc->intd_virq == irq_desc_get_irq(desc))
		mask = INTD;
	else
		dev_err_once(amlogic->dev, "error number irq =%d\n", irq_desc_get_irq(desc));

	reg = amlogic_pcieinter_read(amlogic, IMASK_LOCAL);
	reg &= ~mask;
	amlogic_pcieinter_write(amlogic, reg, IMASK_LOCAL);

	handle_fasteoi_irq(desc);

	amlogic_pcieinter_write(amlogic, mask, ISTATUS_LOCAL);

	reg = amlogic_pcieinter_read(amlogic, IMASK_LOCAL);
	reg |= mask;
	amlogic_pcieinter_write(amlogic, reg, IMASK_LOCAL);
}

static int amlogic_pcie_setup_intx_irq(struct amlogic_pcie_rc *rc)
{
	struct amlogic_pcie *amlogic = &rc->amlogic;
	struct device *dev = amlogic->dev;
	struct platform_device *pdev = to_platform_device(dev);
	u32 reg;

	rc->inta_virq = platform_get_irq(pdev, 1);
	if (rc->inta_virq < 0) {
		dev_err(dev, "failed to get IRQ: %d\n", rc->inta_virq);
		return rc->inta_virq;
	}

	irq_set_handler(rc->inta_virq, amlogic_pcie_intx_handler);
	irq_set_handler_data(rc->inta_virq, rc);

	rc->intb_virq = platform_get_irq(pdev, 2);
	if (rc->intb_virq < 0) {
		dev_err(dev, "failed to get IRQ: %d\n", rc->intb_virq);
		return rc->intb_virq;
	}

	irq_set_handler(rc->intb_virq, amlogic_pcie_intx_handler);
	irq_set_handler_data(rc->intb_virq, rc);

	rc->intc_virq = platform_get_irq(pdev, 3);
	if (rc->intc_virq < 0) {
		dev_err(dev, "failed to get IRQ: %d\n", rc->intc_virq);
		return rc->intc_virq;
	}

	irq_set_handler(rc->intc_virq, amlogic_pcie_intx_handler);
	irq_set_handler_data(rc->intc_virq, rc);

	rc->intd_virq = platform_get_irq(pdev, 4);
	if (rc->intd_virq < 0) {
		dev_err(dev, "failed to get IRQ: %d\n", rc->intd_virq);
		return rc->intd_virq;
	}

	irq_set_handler(rc->intd_virq, amlogic_pcie_intx_handler);
	irq_set_handler_data(rc->intd_virq, rc);

	/* Enable INTX */
	reg = amlogic_pcieinter_read(amlogic, IMASK_LOCAL);
	reg |= INT_INTX_MASK;
	amlogic_pcieinter_write(amlogic, reg, IMASK_LOCAL);

	return 0;
}

static int amlogic_pcie_parse_host_dt(struct amlogic_pcie_rc *rc)
{
	struct amlogic_pcie *amlogic = &rc->amlogic;
	struct device *dev = amlogic->dev;
	struct device_node *node = dev->of_node;
	int ret;

	ret = of_property_read_u32(node, "phy-type",
				   &amlogic->phy_type);
	if (ret)
		amlogic->phy_type = M31_PHY;
	dev_dbg(amlogic->dev, "PCIE phy type is %d\n", amlogic->phy_type);

	if (of_property_read_bool(node, "max-link-speed"))
		of_property_read_u32(node, "max-link-speed",
				     &amlogic->link_gen);

	if (link_speed)
		amlogic->link_gen = link_speed;

	if (link_times)
		amlogic->link_times = link_times;

	ret = amlogic_pcie_parse_dt(amlogic);
	if (ret)
		return ret;

	return 0;
}

#ifdef CONFIG_PCI_MSI
static struct irq_chip amlogic_msi_irq_chip = {
	.name = "AMLOGIC_PCIe_MSI",
	.irq_mask = pci_msi_mask_irq,
	.irq_unmask = pci_msi_unmask_irq,

};

static struct msi_domain_info amlogic_msi_domain_info = {
	.flags = (MSI_FLAG_USE_DEF_DOM_OPS | MSI_FLAG_USE_DEF_CHIP_OPS |
		  MSI_FLAG_MULTI_PCI_MSI | MSI_FLAG_PCI_MSIX),
	.chip = &amlogic_msi_irq_chip,

};
#endif

static void amlogic_compose_msi_msg(struct irq_data *data,
				    struct msi_msg *msg)
{
	struct amlogic_pcie_rc *rc = irq_data_get_irq_chip_data(data);
	struct amlogic_pcie *amlogic = &rc->amlogic;
	struct device *dev = amlogic->dev;
	phys_addr_t msi_addr = amlogic_pcieinter_read(amlogic, IMSI_ADDR);

	msg->address_lo = lower_32_bits(msi_addr);
	msg->address_hi = upper_32_bits(msi_addr);
	msg->data = data->hwirq;

	dev_dbg(dev, "msi#%d address_hi %#x address_lo %#x\n",
		(int)data->hwirq, msg->address_hi, msg->address_lo);
}

static int amlogic_msi_set_affinity(struct irq_data *irq_data,
				    const struct cpumask *mask, bool force)
{
	return -EINVAL;
}

static struct irq_chip amlogic_irq_chip = {
	.name = "AMLOGIC_MSI",
	.irq_compose_msi_msg = amlogic_compose_msi_msg,
	.irq_set_affinity = amlogic_msi_set_affinity,

};

static int amlogic_msi_alloc(struct irq_domain *domain, unsigned int virq,
			     unsigned int nr_irqs, void *args)
{
	struct amlogic_pcie_rc *rc = domain->host_data;
	struct amlogic_msi *msi = &rc->msi;
	int bit;

	WARN_ON(nr_irqs != 1);
	mutex_lock(&msi->lock);

	bit = find_first_zero_bit(msi->used, INT_PCI_MSI_NR);
	if (bit >= INT_PCI_MSI_NR) {
		mutex_unlock(&msi->lock);
		return -ENOSPC;
	}

	set_bit(bit, msi->used);

	irq_domain_set_info(domain, virq, bit, &amlogic_irq_chip,
			    domain->host_data, handle_simple_irq,
			    NULL, NULL);
	mutex_unlock(&msi->lock);
	return 0;
}

static void amlogic_msi_free(struct irq_domain *domain, unsigned int virq,
			     unsigned int nr_irqs)
{
	struct irq_data *data = irq_domain_get_irq_data(domain, virq);
	struct amlogic_pcie_rc *rc = irq_data_get_irq_chip_data(data);
	struct amlogic_pcie *amlogic = &rc->amlogic;
	struct device *dev = amlogic->dev;
	struct amlogic_msi *msi = &rc->msi;

	mutex_lock(&msi->lock);

	if (!test_bit(data->hwirq, msi->used))
		dev_err(dev, "trying to free unused MSI#%lu\n",
			data->hwirq);
	else
		__clear_bit(data->hwirq, msi->used);

	mutex_unlock(&msi->lock);
}

static const struct irq_domain_ops dev_msi_domain_ops = {
	.alloc  = amlogic_msi_alloc,
	.free   = amlogic_msi_free,

};

static int amlogic_pcie_init_msi_irq_domain(struct amlogic_pcie_rc *rc)
{
#ifdef CONFIG_PCI_MSI
	struct amlogic_pcie *amlogic = &rc->amlogic;
	struct device *dev = amlogic->dev;
	struct fwnode_handle *fwn = of_node_to_fwnode(dev->of_node);
	struct amlogic_msi *msi = &rc->msi;

	msi->inner_domain = irq_domain_add_linear(NULL, INT_PCI_MSI_NR,
						  &dev_msi_domain_ops, rc);
	if (!msi->inner_domain) {
		dev_err(dev, "failed to create dev IRQ domain\n");
		return -ENOMEM;
	}
	msi->msi_domain = pci_msi_create_irq_domain(fwn,
						    &amlogic_msi_domain_info,
						    msi->inner_domain);
	if (!msi->msi_domain) {
		dev_err(dev, "failed to create msi IRQ domain\n");
		irq_domain_remove(msi->inner_domain);
		return -ENOMEM;
	}
#endif
	return 0;
}

static int amlogic_pcie_init_irq_domain(struct amlogic_pcie_rc *rc)
{
	if (pci_msi_enabled())
		return amlogic_pcie_init_msi_irq_domain(rc);

	return 0;
}

static void amlogic_pcie_cfg_atr(struct amlogic_pcie *amlogic)
{
	amlogic_pcie_cfg_addr_map(amlogic, ATR_PCIE_WIN0 + ATR_TABLE_SIZE * 0,
				  0, 0,
				  31, ATR_TRSLID_AXIMEMORY);

	amlogic_pcie_cfg_addr_map(amlogic, ATR_AXI4_SLV0 + ATR_TABLE_SIZE * 1,
				  amlogic->mem_bus_addr, amlogic->mem_bus_addr,
				  ilog2(amlogic->mem_size) - 1,
				  ATR_TRSLID_PCIE_MEMORY);

	/*
	 *amlogic_pcie_cfg_addr_map(amlogic, ATR_AXI4_SLV0 + ATR_TABLE_SIZE * 2,
	 *			     amlogic->io_bus_addr, amlogic->io_bus_addr,
	 *			     ilog2(amlogic->io_size) - 1, ATR_TRSLID_PCIE_IO);
	 */
}

static int __maybe_unused amlogic_pcie_suspend_noirq(struct device *dev)
{
	struct amlogic_pcie *amlogic = dev_get_drvdata(dev);
	int err;
	u32 value;

	err = readl_poll_timeout(amlogic->pcictrl_base + PCIE_A_CTRL5, value,
				 PCIE_LINK_STATE_CHECK(value, LTSSM_L1_IDLE) |
				 PCIE_LINK_STATE_CHECK(value, LTSSM_L2_IDLE) |
				 PCIE_LINK_STATE_CHECK(value, LTSSM_L0),
				 20, jiffies_to_msecs(5 * HZ));

	if (err)
		dev_dbg(amlogic->dev, "PCIe link enter LP timeout!,LTSSM=0x%lx\n",
			((((value) >> 18)) & GENMASK(4, 0)));

	amlogic_pcie_deinit_phys(amlogic);

	amlogic_pcie_disable_clocks(amlogic);

	return 0;
}

static int __maybe_unused amlogic_pcie_resume_noirq(struct device *dev)
{
	struct amlogic_pcie *amlogic = dev_get_drvdata(dev);
	int err;

	err = amlogic_pcie_enable_clocks(amlogic);
	if (err)
		return err;

	err = amlogic_pcie_host_init_port(amlogic);
	if (err)
		goto err_pcie_resume;

	amlogic_pcie_cfg_atr(amlogic);

	return 0;

err_pcie_resume:
	amlogic_pcie_disable_clocks(amlogic);
	return err;
}

static int amlogic_pcie_rc_probe(struct platform_device *pdev)
{
	struct amlogic_pcie_rc *rc;
	struct amlogic_pcie *amlogic;
	struct device *dev = &pdev->dev;
	struct pci_host_bridge *bridge;
	struct resource_entry *win;
	resource_size_t io_base;
	struct resource	*mem;
	struct resource	*io;
	int err;

	LIST_HEAD(res);

	if (!dev->of_node)
		return -ENODEV;

	bridge = devm_pci_alloc_host_bridge(dev, sizeof(*rc));
	if (!bridge)
		return -ENOMEM;

	rc = pci_host_bridge_priv(bridge);

	platform_set_drvdata(pdev, rc);
	amlogic = &rc->amlogic;
	amlogic->dev = dev;
	amlogic->is_rc = true;

	err = amlogic_pcie_parse_host_dt(rc);
	if (err)
		return err;

	err = amlogic_pcie_enable_clocks(amlogic);
	if (err)
		return err;

	err = amlogic_pcie_host_init_port(amlogic);
	if (err)
		goto err_disable_clk;

	err = amlogic_pcie_init_irq_domain(rc);
	if (err < 0)
		goto err_deinit_port;

	err = devm_of_pci_get_host_bridge_resources(dev, 0, 0xff,
						    &res, &io_base);
	if (err)
		goto err_deinit_irq_domain;

	err = devm_request_pci_bus_resources(dev, &res);
	if (err)
		goto err_deinit_irq_domain;

	/* Get the I/O and memory ranges from DT */
	resource_list_for_each_entry(win, &res) {
		switch (resource_type(win->res)) {
		case IORESOURCE_IO:
			io = win->res;
			io->name = "I/O";
			amlogic->io_size = resource_size(io);
			amlogic->io_bus_addr = io->start - win->offset;
			err = pci_remap_iospace(io, io_base);
			if (err) {
				dev_warn(dev, "error %d: failed to map resource %pR\n",
					 err, io);
				continue;
			}
			amlogic->io = io;
			break;
		case IORESOURCE_MEM:
			mem = win->res;
			mem->name = "MEM";
			amlogic->mem_size = resource_size(mem);
			amlogic->mem_bus_addr = mem->start - win->offset;
			break;
		default:
			continue;
		}
	}

	amlogic_pcieinter_write(amlogic, 0xffffffff, ISTATUS_LOCAL);

	amlogic_pcie_cfg_atr(amlogic);

	list_splice_init(&res, &bridge->windows);
	bridge->dev.parent = dev;
	bridge->sysdata = rc;
	bridge->busnr = 0;
	bridge->ops = &amlogic_pcie_ops;
	bridge->map_irq = of_irq_parse_and_map_pci;
	bridge->swizzle_irq = pci_common_swizzle;

	err = pci_host_probe(bridge);
	if (err < 0) {
		dev_err(dev, "failed to set vpcie regulator\n");
		goto err_unmap_iospace;
	}

	amlogic_pcieinter_write(amlogic, 0x0,
				PCI_CFG_SPACE + PCI_BASE_ADDRESS_0);

	err = amlogic_pcie_setup_intx_irq(rc);
	if (err) {
		dev_err(dev, "failed to INTX support: %d\n", err);
		goto err_root_bus;
	}

	if (IS_ENABLED(CONFIG_PCI_MSI)) {
		err = amlogic_pcie_enable_msi(rc);
		if (err < 0) {
			dev_err(dev, "failed to enable MSI support: %d\n", err);
			goto err_root_bus;
		}
	}

	rc->root_bus = bridge->bus;

	return 0;

err_root_bus:
	pci_stop_root_bus(rc->root_bus);
	pci_remove_root_bus(rc->root_bus);
err_unmap_iospace:
	pci_unmap_iospace(amlogic->io);
	pci_free_resource_list(&res);
err_deinit_irq_domain:
	amlogic_pcie_free_irq_domain(rc);
err_deinit_port:
	amlogic_pcie_deinit_phys(amlogic);
err_disable_clk:
	if (pcie_test)
		return 0;

	amlogic_pcie_disable_clocks(amlogic);
	return err;
}

static void amlogic_msi_free_irq_domain(struct amlogic_pcie_rc *rc)
{
#ifdef CONFIG_PCI_MSI
	struct amlogic_msi *msi = &rc->msi;
	u32 irq;
	int i;

	for (i = 0; i < INT_PCI_MSI_NR; i++) {
		irq = irq_find_mapping(msi->inner_domain, i);
		if (irq > 0)
			irq_dispose_mapping(irq);
	}

	if (msi->msi_domain)
		irq_domain_remove(msi->msi_domain);

	if (msi->inner_domain)
		irq_domain_remove(msi->inner_domain);
#endif
}

static void amlogic_pcie_free_irq_domain(struct amlogic_pcie_rc *rc)
{
	struct amlogic_pcie *amlogic = &rc->amlogic;
	struct amlogic_msi *msi = &rc->msi;

	/* Disable all interrupts */
	amlogic_pcieinter_write(amlogic, 0, IMASK_LOCAL);

	if (pci_msi_enabled())
		amlogic_msi_free_irq_domain(rc);

	irq_set_chained_handler_and_data(msi->irq, NULL, NULL);
}

static const struct dev_pm_ops amlogic_pcie_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(amlogic_pcie_suspend_noirq,
				      amlogic_pcie_resume_noirq)
};

static const struct of_device_id amlogic_pcie_of_match[] = {
	{ .compatible = "amlogic, amlogic-pcie-v3", },
	{ .compatible = "amlogic,amlogic-pcie-v3", },
	{}
};
MODULE_DEVICE_TABLE(of, amlogic_pcie_of_match);

static struct platform_driver amlogic_pcie_driver = {
	.driver = {
		.name = "amlogic-pcie-v3",
		.of_match_table = amlogic_pcie_of_match,
		.pm = &amlogic_pcie_pm_ops,
	},
	.probe = amlogic_pcie_rc_probe,
};
builtin_platform_driver(amlogic_pcie_driver);

MODULE_AUTHOR("Amlogic Inc");
MODULE_DESCRIPTION("Amlogic AXI PCIe Host driver");
MODULE_LICENSE("GPL v2");
