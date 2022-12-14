// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 Amlogic, Inc.
 * Author: Yue Wang <yue.wang@amlogic.com>
 *
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of_gpio.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/signal.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/clk-provider.h>

//#include "../../pci/controller/dwc/pcie-designware.h"
#include "pcie-designware.h"
#include "pcie-amlogic.h"

struct amlogic_pcie {
	struct dw_pcie          *pci;
	struct pcie_phy		*phy;
	void __iomem		*elbi_base;	/* DT 0th resource */
	void __iomem		*cfg_base;	/* DT 2nd resource */
	int			reset_gpio;
	struct clk		*clk;
	struct clk		*bus_clk;
	struct clk		*mipi_gate;
	struct clk		*mipi_bandgap_gate;
	struct clk		*port_clk;
	struct clk		*general_clk;
	int			pcie_num;
	int			gpio_type;
	u32			port_num;
	u32			pm_enable;
	u32			device_attch;
};

#define to_amlogic_pcie(x) dev_get_drvdata((x)->dev)

struct pcie_phy_aml_regs pcie_aml_regs;
struct pcie_phy		*g_pcie_phy;

static void amlogic_elb_writel(struct amlogic_pcie *amlogic_pcie, u32 val,
								u32 reg)
{
	writel(val, amlogic_pcie->elbi_base + reg);
}

static u32 amlogic_elb_readl(struct amlogic_pcie *amlogic_pcie, u32 reg)
{
	return readl(amlogic_pcie->elbi_base + reg);
}

static void amlogic_cfg_writel(struct amlogic_pcie *amlogic_pcie, u32 val,
								u32 reg)
{
	writel(val, amlogic_pcie->cfg_base + reg);
}

static u32 amlogic_cfg_readl(struct amlogic_pcie *amlogic_pcie, u32 reg)
{
	return readl(amlogic_pcie->cfg_base + reg);
}

static void cr_bus_addr(unsigned int addr)
{
	union phy_r4 phy_r4 = {.d32 = 0};
	union phy_r5 phy_r5 = {.d32 = 0};

	phy_r4.b.phy_cr_data_in = addr;
	writel(phy_r4.d32, pcie_aml_regs.pcie_phy_r[4]);

	phy_r4.b.phy_cr_cap_addr = 0;
	writel(phy_r4.d32, pcie_aml_regs.pcie_phy_r[4]);
	phy_r4.b.phy_cr_cap_addr = 1;
	writel(phy_r4.d32, pcie_aml_regs.pcie_phy_r[4]);

	do {
		phy_r5.d32 = readl(pcie_aml_regs.pcie_phy_r[5]);
	} while (phy_r5.b.phy_cr_ack == 0);

	phy_r4.b.phy_cr_cap_addr = 0;
	writel(phy_r4.d32, pcie_aml_regs.pcie_phy_r[4]);

	do {
		phy_r5.d32 = readl(pcie_aml_regs.pcie_phy_r[5]);
	} while (phy_r5.b.phy_cr_ack == 1);
}

static int cr_bus_read(unsigned int addr)
{
	int data;
	union phy_r4 phy_r4 = {.d32 = 0};
	union phy_r5 phy_r5 = {.d32 = 0};

	cr_bus_addr(addr);

	phy_r4.b.phy_cr_read = 0;
	writel(phy_r4.d32, pcie_aml_regs.pcie_phy_r[4]);
	phy_r4.b.phy_cr_read = 1;
	writel(phy_r4.d32, pcie_aml_regs.pcie_phy_r[4]);

	do {
		phy_r5.d32 = readl(pcie_aml_regs.pcie_phy_r[5]);
	} while (phy_r5.b.phy_cr_ack == 0);

	data = phy_r5.b.phy_cr_data_out;

	phy_r4.b.phy_cr_read = 0;
	writel(phy_r4.d32, pcie_aml_regs.pcie_phy_r[4]);

	do {
		phy_r5.d32 = readl(pcie_aml_regs.pcie_phy_r[5]);
	} while (phy_r5.b.phy_cr_ack == 1);

	return data;
}

static void cr_bus_write(unsigned int addr, unsigned int data)
{
	union phy_r4 phy_r4 = {.d32 = 0};
	union phy_r5 phy_r5 = {.d32 = 0};

	cr_bus_addr(addr);

	phy_r4.b.phy_cr_data_in = data;
	writel(phy_r4.d32, pcie_aml_regs.pcie_phy_r[4]);

	phy_r4.b.phy_cr_cap_data = 0;
	writel(phy_r4.d32, pcie_aml_regs.pcie_phy_r[4]);
	phy_r4.b.phy_cr_cap_data = 1;
	writel(phy_r4.d32, pcie_aml_regs.pcie_phy_r[4]);

	do {
		phy_r5.d32 = readl(pcie_aml_regs.pcie_phy_r[5]);
	} while (phy_r5.b.phy_cr_ack == 0);

	phy_r4.b.phy_cr_cap_data = 0;
	writel(phy_r4.d32, pcie_aml_regs.pcie_phy_r[4]);

	do {
		phy_r5.d32 = readl(pcie_aml_regs.pcie_phy_r[5]);
	} while (phy_r5.b.phy_cr_ack == 1);

	phy_r4.b.phy_cr_write = 0;
	writel(phy_r4.d32, pcie_aml_regs.pcie_phy_r[4]);
	phy_r4.b.phy_cr_write = 1;
	writel(phy_r4.d32, pcie_aml_regs.pcie_phy_r[4]);

	do {
		phy_r5.d32 = readl(pcie_aml_regs.pcie_phy_r[5]);
	} while (phy_r5.b.phy_cr_ack == 0);

	phy_r4.b.phy_cr_write = 0;
	writel(phy_r4.d32, pcie_aml_regs.pcie_phy_r[4]);

	do {
		phy_r5.d32 = readl(pcie_aml_regs.pcie_phy_r[5]);
	} while (phy_r5.b.phy_cr_ack == 1);
}

static void amlogic_phy_cr_writel(u32 val, u32 reg)
{
	cr_bus_write(reg, val);
}

static u32 amlogic_phy_cr_readl(u32 reg)
{
	return cr_bus_read(reg);
}

static ssize_t phyread_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	u32 status = 0;

	return sprintf(buf, "%d\n", status);
}

static ssize_t phyread_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	u32 reg;
	u32 val;

	if (kstrtouint(buf, 16, &reg))
		return -EINVAL;

	val = amlogic_phy_cr_readl(reg);
	dev_info(dev, "reg 0x%x value is 0x%x\n", reg, val);

	return count;
}
DEVICE_ATTR_RW(phyread);

static ssize_t phywrite_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	u32 status = 0;

	return sprintf(buf, "%d\n", status);
}

static ssize_t phywrite_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf_p, size_t count)
{
	unsigned int reg, val;
	char buf[80];
	int ret;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	strncpy(buf, buf_p, (u32)count);

	buf[count] = 0;

	ret = sscanf(buf, "%x %x", &reg, &val);

	switch (ret) {
	case 2:
		amlogic_phy_cr_writel(val, reg);
		break;
	default:
		return -EINVAL;
	}

	return count;
}
DEVICE_ATTR_RW(phywrite);

static void amlogic_pcie_assert_reset(struct amlogic_pcie *amlogic_pcie)
{
	struct dw_pcie *pci = amlogic_pcie->pci;
	struct device *dev = pci->dev;
	int ret = 0;

	if (amlogic_pcie->gpio_type == 0) {
		dev_info(dev, "gpio multiplex, don't reset!\n");
	} else if (amlogic_pcie->gpio_type == 1) {
		dev_info(dev, "pad gpio\n");
		if (amlogic_pcie->reset_gpio >= 0)
			ret = devm_gpio_request(dev, amlogic_pcie->reset_gpio, "RESET");

		if (!ret && gpio_is_valid(amlogic_pcie->reset_gpio)) {
			dev_info(dev, "GPIO Pad: assert reset\n");
			gpio_direction_output(amlogic_pcie->reset_gpio, 0);
			usleep_range(5000, 6000);
			gpio_direction_input(amlogic_pcie->reset_gpio);
		}
	} else {
		dev_info(dev, "normal gpio\n");
		if (amlogic_pcie->reset_gpio >= 0) {
			ret = devm_gpio_request(dev, amlogic_pcie->reset_gpio, "RESET");

			if (!ret)
				gpio_direction_output(amlogic_pcie->reset_gpio, 0);
		}

		if (gpio_is_valid(amlogic_pcie->reset_gpio)) {
			dev_info(dev, "GPIO normal: assert reset\n");
			gpio_set_value_cansleep(amlogic_pcie->reset_gpio, 0);
			usleep_range(5000, 6000);
			gpio_set_value_cansleep(amlogic_pcie->reset_gpio, 1);
		}
	}
}

static void amlogic_set_max_payload(struct amlogic_pcie *amlogic_pcie, int size)
{
	int max_payload_size = 1;
	u32 val = 0;

	switch (size) {
	case 128:
		max_payload_size = 0;
		break;
	case 256:
		max_payload_size = 1;
		break;
	case 512:
		max_payload_size = 2;
		break;
	case 1024:
		max_payload_size = 3;
		break;
	case 2048:
		max_payload_size = 4;
		break;
	case 4096:
		max_payload_size = 5;
		break;
	default:
		max_payload_size = 1;
		break;
	}

	val = amlogic_elb_readl(amlogic_pcie, PCIE_DEV_CTRL_DEV_STUS);
	val &= (~(0x7 << 5));
	amlogic_elb_writel(amlogic_pcie, val, PCIE_DEV_CTRL_DEV_STUS);

	val = amlogic_elb_readl(amlogic_pcie, PCIE_DEV_CTRL_DEV_STUS);
	val |= (max_payload_size << 5);
	amlogic_elb_writel(amlogic_pcie, val, PCIE_DEV_CTRL_DEV_STUS);
}

static void amlogic_set_max_rd_req_size(struct amlogic_pcie *amlogic_pcie, int size)
{
	int max_rd_req_size = 1;
	u32 val = 0;

	switch (size) {
	case 128:
		max_rd_req_size = 0;
		break;
	case 256:
		max_rd_req_size = 1;
		break;
	case 512:
		max_rd_req_size = 2;
		break;
	case 1024:
		max_rd_req_size = 3;
		break;
	case 2048:
		max_rd_req_size = 4;
		break;
	case 4096:
		max_rd_req_size = 5;
		break;
	default:
		max_rd_req_size = 1;
		break;
	}

	val = amlogic_elb_readl(amlogic_pcie, PCIE_DEV_CTRL_DEV_STUS);
	val &= (~(0x7 << 12));
	amlogic_elb_writel(amlogic_pcie, val, PCIE_DEV_CTRL_DEV_STUS);

	val = amlogic_elb_readl(amlogic_pcie, PCIE_DEV_CTRL_DEV_STUS);
	val |= (max_rd_req_size << 12);
	amlogic_elb_writel(amlogic_pcie, val, PCIE_DEV_CTRL_DEV_STUS);
}

static void amlogic_pcie_init_dw(struct amlogic_pcie *amlogic_pcie)
{
	u32 val = 0;

	val = amlogic_cfg_readl(amlogic_pcie, PCIE_CFG0);
	val |= APP_LTSSM_ENABLE;
	amlogic_cfg_writel(amlogic_pcie, val, PCIE_CFG0);

	val = amlogic_elb_readl(amlogic_pcie, PCIE_PORT_LINK_CTRL_OFF);
	val |= (1 << 7);
	amlogic_elb_writel(amlogic_pcie, val, PCIE_PORT_LINK_CTRL_OFF);

	val = amlogic_elb_readl(amlogic_pcie, PCIE_PORT_LINK_CTRL_OFF);
	val &= (~(0x3f << 16));
	amlogic_elb_writel(amlogic_pcie, val, PCIE_PORT_LINK_CTRL_OFF);

	val = amlogic_elb_readl(amlogic_pcie, PCIE_PORT_LINK_CTRL_OFF);
	val |= (0x1 << 16);
	amlogic_elb_writel(amlogic_pcie, val, PCIE_PORT_LINK_CTRL_OFF);

	val = amlogic_elb_readl(amlogic_pcie, PCIE_GEN2_CTRL_OFF);
	val &= (~(0x1f << 8));
	amlogic_elb_writel(amlogic_pcie, val, PCIE_GEN2_CTRL_OFF);

	val = amlogic_elb_readl(amlogic_pcie, PCIE_GEN2_CTRL_OFF);
	val |= (0x1 << 8);
	amlogic_elb_writel(amlogic_pcie, val, PCIE_GEN2_CTRL_OFF);

	val = amlogic_elb_readl(amlogic_pcie, PCIE_GEN2_CTRL_OFF);
	val |= (1 << 17);
	amlogic_elb_writel(amlogic_pcie, val, PCIE_GEN2_CTRL_OFF);

	amlogic_elb_writel(amlogic_pcie, 0x0, PCIE_BASE_ADDR0);
	amlogic_elb_writel(amlogic_pcie, 0x0, PCIE_BASE_ADDR1);
}

static void amlogic_enable_memory_space(struct amlogic_pcie *amlogic_pcie)
{
	u32 val = 0;

	dev_info(amlogic_pcie->pci->dev,
		"Set the RC Bus Master, Memory Space and I/O Space enables.\n");
	val = amlogic_elb_readl(amlogic_pcie, 0x04);
	val = 7;
	amlogic_elb_writel(amlogic_pcie, val, 0x04);
}

static int amlogic_pcie_establish_link(struct amlogic_pcie *amlogic_pcie)
{
	struct dw_pcie *pci = amlogic_pcie->pci;
	struct pcie_port *pp = &pci->pp;

	amlogic_pcie_init_dw(amlogic_pcie);
	amlogic_set_max_payload(amlogic_pcie, 256);
	amlogic_set_max_rd_req_size(amlogic_pcie, 256);

	dw_pcie_setup_rc(pp);
	amlogic_enable_memory_space(amlogic_pcie);
	amlogic_pcie_assert_reset(amlogic_pcie);

	/* check if the link is up or not */
	if (!dw_pcie_wait_for_link(pci))
		return 0;
	return -ETIMEDOUT;
}

static void amlogic_pcie_msi_init(struct amlogic_pcie *amlogic_pcie)
{
	struct pcie_port *pp = &amlogic_pcie->pci->pp;

	dw_pcie_msi_init(pp);
}

static void amlogic_pcie_enable_interrupts(struct amlogic_pcie *amlogic_pcie)
{
	if (IS_ENABLED(CONFIG_PCI_MSI))
		amlogic_pcie_msi_init(amlogic_pcie);
}

static int amlogic_pcie_rd_own_conf(struct pcie_port *pp, int where, int size,
				    u32 *val)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct amlogic_pcie *amlogic_pcie = to_amlogic_pcie(pci);

	if (amlogic_pcie->device_attch == 0)
		return 0;

	/* the device class is not reported correctly from the register */
	if (where == PCI_CLASS_REVISION) {
		*val = readl(pci->dbi_base + PCI_CLASS_REVISION);
		*val &= 0xff;	/* keep revision id */
		*val |= PCI_CLASS_BRIDGE_PCI << 16;
		return PCIBIOS_SUCCESSFUL;
	}

	return dw_pcie_read(pci->dbi_base + where, size, val);
}

static int amlogic_pcie_link_up(struct dw_pcie *pci)
{
	u32   smlh_up = 0;
	u32   rdlh_up = 0;
	u32   ltssm_up = 0;
	u32   speed_okay = 0;
	u32   current_data_rate;
	int   cnt = 0;
	u32   val = 0;
	u32   linkup = 0;
	struct amlogic_pcie *amlogic_pcie = to_amlogic_pcie(pci);

	val = readl(pci->dbi_base + PCIE_PHY_DEBUG_R1);
	linkup = ((val & PCIE_PHY_DEBUG_R1_LINK_UP) &&
		(!(val & PCIE_PHY_DEBUG_R1_LINK_IN_TRAINING)));
	if (linkup)
		return linkup;

	while (smlh_up == 0 || rdlh_up == 0 || ltssm_up == 0 || speed_okay == 0) {
		smlh_up = amlogic_cfg_readl(amlogic_pcie, PCIE_CFG_STATUS12);
		smlh_up = (smlh_up >> 6) & 0x1;

		rdlh_up = amlogic_cfg_readl(amlogic_pcie, PCIE_CFG_STATUS12);
		rdlh_up = (rdlh_up >> 16) & 0x1;
		ltssm_up = amlogic_cfg_readl(amlogic_pcie, PCIE_CFG_STATUS12);
		ltssm_up = ((ltssm_up >> 10) & 0x1f) == 0x11 ? 1 : 0;
		current_data_rate = amlogic_cfg_readl(amlogic_pcie, PCIE_CFG_STATUS17);
		current_data_rate = (current_data_rate >> 7) & 0x1;

		if (current_data_rate == PCIE_GEN2 || current_data_rate == PCIE_GEN1)
			speed_okay = 1;

		if (smlh_up)
			dev_dbg(pci->dev, "smlh_link_up is on\n");
		if (rdlh_up)
			dev_dbg(pci->dev, "rdlh_link_up is on\n");
		if (ltssm_up)
			dev_dbg(pci->dev, "ltssm_up is on\n");
		if (speed_okay)
			dev_dbg(pci->dev, "speed_okay\n");

		cnt++;

		if (cnt >= WAIT_LINKUP_TIMEOUT) {
			dev_err(pci->dev, "Error: Wait linkup timeout.\n");
			return 0;
		}

		udelay(20);
	}

	if (current_data_rate == PCIE_GEN2)
		dev_info(pci->dev, "PCIE SPEED IS GEN2\n");

	return 1;
}

static int amlogic_pcie_host_init(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct amlogic_pcie *amlogic_pcie = (struct amlogic_pcie *)to_amlogic_pcie(pci);
	int ret;

	ret = amlogic_pcie_establish_link(amlogic_pcie);
	if (ret)
		if (amlogic_pcie->phy->device_attch == 0)
			return ret;

	amlogic_pcie->phy->device_attch = 1;
	if (!ret)
		amlogic_pcie->device_attch = 1;

	amlogic_pcie_enable_interrupts(amlogic_pcie);

	return ret;
}

static struct dw_pcie_host_ops amlogic_pcie_host_ops = {
	.rd_own_conf = amlogic_pcie_rd_own_conf,
	.host_init = amlogic_pcie_host_init,
};

static const struct dw_pcie_ops dw_pcie_ops = {
	.link_up = amlogic_pcie_link_up,
};

static int  amlogic_add_pcie_port(struct amlogic_pcie *amlogic_pcie,
				  struct platform_device *pdev)
{
	struct pcie_port *pp = &amlogic_pcie->pci->pp;
	struct device *dev = &pdev->dev;
	int ret;

	if (IS_ENABLED(CONFIG_PCI_MSI)) {
		pp->msi_irq = platform_get_irq(pdev, 0);
		if (!pp->msi_irq) {
			dev_err(dev, "failed to get msi irq\n");
			return -ENODEV;
		}
	}

	pp->root_bus_nr = -1;
	pp->ops = &amlogic_pcie_host_ops;
	amlogic_pcie->pci->dbi_base = amlogic_pcie->elbi_base;

	ret = dw_pcie_host_init(pp);
	if (ret) {
		dev_err(dev, "failed to initialize host\n");
		return ret;
	}

	if (amlogic_pcie->device_attch == 0) {
		dev_err(dev, "link timeout, disable PCIE PLL\n");
		clk_disable_unprepare(amlogic_pcie->port_clk);
		clk_disable_unprepare(amlogic_pcie->general_clk);
		clk_disable_unprepare(amlogic_pcie->mipi_bandgap_gate);
		clk_disable_unprepare(amlogic_pcie->mipi_gate);
		clk_disable_unprepare(amlogic_pcie->bus_clk);
		clk_disable_unprepare(amlogic_pcie->clk);
		if (amlogic_pcie->pcie_num == 2) {
			if (amlogic_pcie->phy->device_attch == 0) {
				dev_err(dev, "power down pcie phy\n");
				writel(0x1d, pcie_aml_regs.pcie_phy_r[0]);
				amlogic_pcie->phy->power_state = 0;
			}
		}
	}

	return 0;
}

static int __init amlogic_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct amlogic_pcie *amlogic_pcie;
	struct dw_pcie *pci;
	struct device_node *np = dev->of_node;
	struct pcie_phy		*phy;
	struct resource *elbi_base;
	struct resource *phy_base;
	struct resource *cfg_base;
	struct resource *reset_base;
	int ret;
	int pcie_num = 0;
	int gpio_type = 0;
	unsigned long rate = 100000000;
	int err;
	int j = 0;
	u32 val = 0;
	static u32 port_num;
	u32 pm_enable = 1;

	amlogic_pcie = devm_kzalloc(dev, sizeof(*amlogic_pcie), GFP_KERNEL);
	if (!amlogic_pcie)
		return -ENOMEM;

	pci = devm_kzalloc(dev, sizeof(*pci), GFP_KERNEL);
	if (!pci)
		return -ENOMEM;

	pci->dev = &pdev->dev;
	pci->ops = &dw_pcie_ops;
	amlogic_pcie->pci = pci;
	amlogic_pcie->device_attch = 0;
	port_num++;
	amlogic_pcie->port_num = port_num;
	if (amlogic_pcie->port_num == 1) {
		phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
		if (!phy) {
			port_num--;
			return -ENOMEM;
		}
		g_pcie_phy = phy;
	}

	amlogic_pcie->phy = g_pcie_phy;

	ret = of_property_read_u32(np, "pcie-num", &pcie_num);
	if (ret)
		amlogic_pcie->pcie_num = 0;

	amlogic_pcie->pcie_num = pcie_num;

	ret = of_property_read_u32(np, "pm-enable", &pm_enable);
	if (ret)
		amlogic_pcie->pm_enable = 1;
	else
		amlogic_pcie->pm_enable = pm_enable;

	if (!amlogic_pcie->phy->phy_base) {
		phy_base = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy");
		amlogic_pcie->phy->phy_base =
			 devm_ioremap_resource(dev, phy_base);
		if (IS_ERR(amlogic_pcie->phy->phy_base)) {
			ret = PTR_ERR(amlogic_pcie->phy->phy_base);
			port_num--;
			return ret;
		}
	}

	if (!amlogic_pcie->phy->power_state) {
		for (j = 0; j < 7; j++)
			pcie_aml_regs.pcie_phy_r[j] = (void __iomem *)
				((unsigned long)amlogic_pcie->phy->phy_base + 4 * j);
		writel(0x1c, pcie_aml_regs.pcie_phy_r[0]);
		amlogic_pcie->phy->power_state = 1;
	}

	ret = of_property_read_u32(np, "gpio-type", &gpio_type);
	amlogic_pcie->gpio_type = gpio_type;

	amlogic_pcie->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);

	if (!amlogic_pcie->phy->reset_base) {
		reset_base = platform_get_resource_byname(pdev, IORESOURCE_MEM, "reset");
		amlogic_pcie->phy->reset_base = devm_ioremap_resource(dev, reset_base);
		if (IS_ERR(amlogic_pcie->phy->reset_base)) {
			ret = PTR_ERR(amlogic_pcie->phy->reset_base);
			goto fail_pcie;
		}
	}

	/* RESET0[1,2,6,7] = 0*/
	if (!amlogic_pcie->phy->reset_state) {
		val = readl(amlogic_pcie->phy->reset_base);
		val &= ~((0x3 << 6) | (0x3 << 1));
		writel(val, amlogic_pcie->phy->reset_base);
	}

	amlogic_pcie->bus_clk = devm_clk_get(dev, "pcie_refpll");
	if (IS_ERR(amlogic_pcie->bus_clk)) {
		dev_err(dev, "Failed to get pcie bus clock\n");
		ret = PTR_ERR(amlogic_pcie->bus_clk);
		goto fail_pcie;
	}

	if (!amlogic_pcie->phy->reset_state) {
		err = clk_set_rate(amlogic_pcie->bus_clk, rate);
		if (err) {
			ret = err;
			goto fail_pcie;
		}

		if (clk_get_rate(amlogic_pcie->bus_clk) != rate) {
			ret = -ENODEV;
			goto fail_pcie;
		}
	}

	ret = clk_prepare_enable(amlogic_pcie->bus_clk);
	if (ret)
		goto fail_pcie;

	amlogic_pcie->mipi_gate = devm_clk_get(dev, "pcie_mipi_enable_gate");
	if (IS_ERR(amlogic_pcie->mipi_gate)) {
		dev_err(dev, "Failed to get pcie mipi gate clock\n");
		ret = PTR_ERR(amlogic_pcie->mipi_gate);
		goto fail_bus_clk;
	}
	/*pcie pll: mipi enable and bandgap share with mipi clk */
	ret = clk_prepare_enable(amlogic_pcie->mipi_gate);
	if (ret)
		goto fail_bus_clk;

	amlogic_pcie->mipi_bandgap_gate = devm_clk_get(dev, "pcie_mipi_bandgap_gate");
	if (IS_ERR(amlogic_pcie->mipi_bandgap_gate)) {
		dev_err(dev, "Failed to get pcie mipi bandgap gate clock\n");
		ret = PTR_ERR(amlogic_pcie->mipi_bandgap_gate);
		goto fail_mipi_gate;
	}
	/*pcie pll: mipi enable and bandgap share with mipi clk */
	ret = clk_prepare_enable(amlogic_pcie->mipi_bandgap_gate);
	if (ret)
		goto fail_mipi_gate;

	/*RESET0[6,7] = 1*/
	if (!amlogic_pcie->phy->reset_state) {
		val = readl(amlogic_pcie->phy->reset_base);
		val |=	  (0x3 << 6);
		writel(val, amlogic_pcie->phy->reset_base);
		mdelay(10);
	}

	amlogic_pcie->general_clk = devm_clk_get(dev, "pcie_general");
	if (IS_ERR(amlogic_pcie->general_clk)) {
		dev_err(dev, "Failed to get pcie general clock\n");
		ret = PTR_ERR(amlogic_pcie->general_clk);
		goto fail_mipi_bandgap_gate;
	}

	ret = clk_prepare_enable(amlogic_pcie->general_clk);
	if (ret)
		goto fail_mipi_bandgap_gate;

	amlogic_pcie->clk = devm_clk_get(dev, "pcie");
	if (IS_ERR(amlogic_pcie->clk)) {
		dev_err(dev, "Failed to get pcie rc clock\n");
		ret = PTR_ERR(amlogic_pcie->clk);
		goto fail_general_clk;
	}

	ret = clk_prepare_enable(amlogic_pcie->clk);
	if (ret)
		goto fail_general_clk;

	/*RESET0[1,2] = 1*/
	if (amlogic_pcie->pcie_num == 1) {
		val = readl(amlogic_pcie->phy->reset_base);
		val |=	(0x1 << 1);
		writel(val, amlogic_pcie->phy->reset_base);
		usleep_range(500, 510);
	} else {
		val = readl(amlogic_pcie->phy->reset_base);
		val |= (0x1 << 2);
		writel(val, amlogic_pcie->phy->reset_base);
		usleep_range(500, 510);
	}

	amlogic_pcie->phy->reset_state = 1;
	amlogic_pcie->port_clk = devm_clk_get(dev, "port");

	if (IS_ERR(amlogic_pcie->port_clk)) {
		dev_err(dev, "Failed to get pcie rc clock\n");
		ret = PTR_ERR(amlogic_pcie->port_clk);
		goto fail_clk;
	}

	ret = clk_prepare_enable(amlogic_pcie->port_clk);
	if (ret)
		goto fail_clk;

	elbi_base = platform_get_resource_byname(pdev, IORESOURCE_MEM, "elbi");
	amlogic_pcie->elbi_base = devm_ioremap_resource(dev, elbi_base);
	if (IS_ERR(amlogic_pcie->elbi_base)) {
		ret = PTR_ERR(amlogic_pcie->elbi_base);
		goto fail_port_clk;
	}

	cfg_base = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cfg");
	amlogic_pcie->cfg_base = devm_ioremap_resource(dev, cfg_base);
	if (IS_ERR(amlogic_pcie->cfg_base)) {
		ret = PTR_ERR(amlogic_pcie->cfg_base);
		goto fail_port_clk;
	}

	platform_set_drvdata(pdev, amlogic_pcie);

	ret = amlogic_add_pcie_port(amlogic_pcie, pdev);
	if (ret < 0)
		goto fail_port_clk;

	device_create_file(&pdev->dev, &dev_attr_phyread);
	device_create_file(&pdev->dev, &dev_attr_phywrite);
	return 0;

fail_port_clk:
	clk_disable_unprepare(amlogic_pcie->port_clk);
fail_clk:
	clk_disable_unprepare(amlogic_pcie->clk);
fail_general_clk:
	clk_disable_unprepare(amlogic_pcie->general_clk);
fail_mipi_bandgap_gate:
	clk_disable_unprepare(amlogic_pcie->mipi_bandgap_gate);
fail_mipi_gate:
	clk_disable_unprepare(amlogic_pcie->mipi_gate);
fail_bus_clk:
	clk_disable_unprepare(amlogic_pcie->bus_clk);
fail_pcie:
	port_num--;

	return ret;
}

static int __exit amlogic_pcie_remove(struct platform_device *pdev)
{
	struct amlogic_pcie *amlogic_pcie = platform_get_drvdata(pdev);

	if (amlogic_pcie->phy->power_state == 0) {
		dev_info(&pdev->dev, "PCIE phy power off, no remove\n");
		return 0;
	}

	device_remove_file(&pdev->dev, &dev_attr_phywrite);
	device_remove_file(&pdev->dev, &dev_attr_phyread);

	clk_disable_unprepare(amlogic_pcie->port_clk);
	clk_disable_unprepare(amlogic_pcie->clk);
	clk_disable_unprepare(amlogic_pcie->general_clk);
	clk_disable_unprepare(amlogic_pcie->mipi_bandgap_gate);
	clk_disable_unprepare(amlogic_pcie->mipi_gate);
	clk_disable_unprepare(amlogic_pcie->bus_clk);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int amlogic_pcie_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct amlogic_pcie *amlogic_pcie = platform_get_drvdata(pdev);
	struct dw_pcie *pci = amlogic_pcie->pci;
	u32 val;

	if (!amlogic_pcie->pm_enable) {
		dev_info(dev, "don't suspend amlogic pcie\n");
		return 0;
	}

	if (amlogic_pcie->device_attch == 0) {
		dev_info(dev, "controller power off, no suspend\n");
		return 0;
	}

	dev_info(dev, "%s\n", __func__);

	/* clear MSE */
	val = dw_pcie_readl_dbi(pci, PCI_COMMAND);
	val &= ~PCI_COMMAND_MEMORY;
	dw_pcie_writel_dbi(pci, PCI_COMMAND, val);

	return 0;
}

static int amlogic_pcie_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct amlogic_pcie *amlogic_pcie = platform_get_drvdata(pdev);
	struct dw_pcie *pci = amlogic_pcie->pci;
	u32 val;

	if (!amlogic_pcie->pm_enable) {
		dev_info(dev, "don't resume amlogic pcie\n");
		return 0;
	}

	if (amlogic_pcie->device_attch == 0) {
		dev_info(dev, "controller power off, no resume\n");
		return 0;
	}

	dev_info(dev, "%s\n", __func__);

	/* set MSE */
	val = dw_pcie_readl_dbi(pci, PCI_COMMAND);
	val |= PCI_COMMAND_MEMORY;
	dw_pcie_writel_dbi(pci, PCI_COMMAND, val);

	return 0;
}

static int amlogic_pcie_suspend_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct amlogic_pcie *amlogic_pcie = platform_get_drvdata(pdev);

	if (!amlogic_pcie->pm_enable) {
		dev_info(dev, "don't noirq suspend amlogic pcie\n");
		return 0;
	}

	if (amlogic_pcie->phy->device_attch == 0) {
		dev_info(dev, "PCIE phy power off, no suspend noirq\n");
		return 0;
	}

	if (amlogic_pcie->device_attch == 0) {
		dev_info(dev, "controller power off, no suspend noirq\n");
		if (amlogic_pcie->pcie_num == 1) {
			writel(0x1d, pcie_aml_regs.pcie_phy_r[0]);
			amlogic_pcie->phy->power_state = 0;
		}
		return 0;
	}

	dev_info(dev, "%s\n", __func__);

	clk_disable_unprepare(amlogic_pcie->port_clk);
	clk_disable_unprepare(amlogic_pcie->clk);
	clk_disable_unprepare(amlogic_pcie->general_clk);
	clk_disable_unprepare(amlogic_pcie->mipi_bandgap_gate);
	clk_disable_unprepare(amlogic_pcie->mipi_gate);
	clk_disable_unprepare(amlogic_pcie->bus_clk);
	amlogic_pcie->phy->reset_state = 0;

	if (amlogic_pcie->pcie_num == 1) {
		writel(0x1d, pcie_aml_regs.pcie_phy_r[0]);
		amlogic_pcie->phy->power_state = 0;
	}

	return 0;
}

static int amlogic_pcie_resume_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct amlogic_pcie *amlogic_pcie = platform_get_drvdata(pdev);
	struct pcie_port *pp = &amlogic_pcie->pci->pp;
	unsigned long rate = 100000000;
	u32 val;

	if (!amlogic_pcie->pm_enable) {
		dev_info(dev, "don't noirq resume amlogic pcie\n");
		return 0;
	}

	if (amlogic_pcie->phy->device_attch == 0) {
		dev_info(dev, "PCIE phy power off, no resume noirq\n");
		return 0;
	}

	if (amlogic_pcie->pcie_num == 1) {
		writel(0x1c, pcie_aml_regs.pcie_phy_r[0]);
		amlogic_pcie->phy->power_state = 1;
		usleep_range(500, 510);
	}

	if (amlogic_pcie->device_attch == 0) {
		dev_info(dev, "controller power off, no resume noirq\n");
		return 0;
	}

	dev_info(dev, "%s\n", __func__);
	if (!amlogic_pcie->phy->reset_state)
		clk_set_rate(amlogic_pcie->bus_clk, rate);

	amlogic_pcie->phy->reset_state = 1;

	clk_prepare_enable(amlogic_pcie->bus_clk);
	usleep_range(500, 510);
	clk_prepare_enable(amlogic_pcie->mipi_gate);
	clk_prepare_enable(amlogic_pcie->mipi_bandgap_gate);
	clk_prepare_enable(amlogic_pcie->general_clk);
	clk_prepare_enable(amlogic_pcie->clk);
	clk_prepare_enable(amlogic_pcie->port_clk);
	usleep_range(500, 510);

	dw_pcie_setup_rc(pp);

	val = amlogic_cfg_readl(amlogic_pcie, PCIE_CFG0);
	val |= (APP_LTSSM_ENABLE);
	amlogic_cfg_writel(amlogic_pcie, val, PCIE_CFG0);
	usleep_range(500, 510);

	return 0;
}

#else
#define amlogic_pcie_suspend NULL
#define amlogic_pcie_resume NULL
#define amlogic_pcie_suspend_noirq NULL
#define amlogic_pcie_resume_noirq NULL
#endif

static const struct dev_pm_ops amlogic_pcie_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(amlogic_pcie_suspend, amlogic_pcie_resume)
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(amlogic_pcie_suspend_noirq,
				      amlogic_pcie_resume_noirq)
};

static const struct of_device_id amlogic_pcie_of_match[] = {
	{ .compatible = "amlogic, amlogic-pcie", },
	{},
};

static struct platform_driver amlogic_pcie_driver = {
	.remove		= __exit_p(amlogic_pcie_remove),
	.driver = {
		.name	= "aml-pcie",
		.of_match_table = amlogic_pcie_of_match,
		.pm	= &amlogic_pcie_pm_ops,
	},
	.probe = amlogic_pcie_probe,
};

builtin_platform_driver(amlogic_pcie_driver);
MODULE_LICENSE("GPL v2");
