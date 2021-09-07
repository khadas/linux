// SPDX-License-Identifier: GPL-2.0+
/*
 * Amlogic AXI PCIe host controller driver
 *
 * Copyright (c) 2016 Amlogic, Inc.
 *
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/of_pci.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/of_gpio.h>

#include "pcie-amlogic-v3.h"

static u32 amlogic_pciectrl_read(struct amlogic_pcie *pcie, u32 reg)
{
	return readl(pcie->pcictrl_base + reg);
}

static void amlogic_pciectrl_write(struct amlogic_pcie *pcie, u32 val,
				   u32 reg)
{
	writel(val, pcie->pcictrl_base + reg);
}

int amlogic_pcie_get_reset(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct device_node *node = dev->of_node;
	struct resource *rst_regs;
	int ret = 0;

	/*m31phy_pcie_wrapper_rst*/
	amlogic->m31phy_rst = devm_reset_control_get_exclusive(dev,
							       "m31phy_rst");
	if (IS_ERR(amlogic->m31phy_rst)) {
		if (PTR_ERR(amlogic->m31phy_rst) != -EPROBE_DEFER)
			goto get_rst_reg;
	}

	/*pcie_gen3_l0_rst*/
	amlogic->gen3_l0_rst = devm_reset_control_get_exclusive(dev,
								"gen3_lo_rst");
	if (IS_ERR(amlogic->gen3_l0_rst)) {
		if (PTR_ERR(amlogic->gen3_l0_rst) != -EPROBE_DEFER)
			dev_err(dev, "gen3_l0_rst reset property in node\n");
		return PTR_ERR(amlogic->gen3_l0_rst);
	}

	amlogic->pcie_apb_rst = devm_reset_control_get_exclusive(dev,
								 "apb_rst");
	if (IS_ERR(amlogic->pcie_apb_rst)) {
		if (PTR_ERR(amlogic->pcie_apb_rst) != -EPROBE_DEFER)
			dev_err(dev, "pcie_apb_rstreset property in node\n");
		return PTR_ERR(amlogic->pcie_apb_rst);
	}

	amlogic->pcie_phy_rst = devm_reset_control_get_exclusive(dev,
								 "phy_rst");
	if (IS_ERR(amlogic->pcie_phy_rst)) {
		if (PTR_ERR(amlogic->pcie_phy_rst) != -EPROBE_DEFER)
			dev_err(dev, "pcie_phy_rst property in node\n");
		return PTR_ERR(amlogic->pcie_phy_rst);
	}

	amlogic->pcie_a_rst = devm_reset_control_get_exclusive(dev,
							       "pcie_a_rst");
	if (IS_ERR(amlogic->pcie_a_rst)) {
		if (PTR_ERR(amlogic->pcie_a_rst) != -EPROBE_DEFER)
			dev_err(dev, "pcie_a_rst reset property in node\n");
		return PTR_ERR(amlogic->pcie_a_rst);
	}

	amlogic->pcie_rst0 = devm_reset_control_get_exclusive(dev, "pcie_rst0");
	if (IS_ERR(amlogic->pcie_rst0)) {
		if (PTR_ERR(amlogic->pcie_rst0) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst0 property in node\n");
		return PTR_ERR(amlogic->pcie_rst0);
	}

	amlogic->pcie_rst1 = devm_reset_control_get_exclusive(dev, "pcie_rst1");
	if (IS_ERR(amlogic->pcie_rst1)) {
		if (PTR_ERR(amlogic->pcie_rst1) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst1 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst1);
	}

	amlogic->pcie_rst2 = devm_reset_control_get_exclusive(dev, "pcie_rst2");
	if (IS_ERR(amlogic->pcie_rst2)) {
		if (PTR_ERR(amlogic->pcie_rst2) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst2 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst2);
	}

	amlogic->pcie_rst3 = devm_reset_control_get_exclusive(dev, "pcie_rst3");
	if (IS_ERR(amlogic->pcie_rst3)) {
		if (PTR_ERR(amlogic->pcie_rst3) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst3 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst3);
	}

	amlogic->pcie_rst4 = devm_reset_control_get_exclusive(dev, "pcie_rst4");
	if (IS_ERR(amlogic->pcie_rst4)) {
		if (PTR_ERR(amlogic->pcie_rst4) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst4 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst4);
	}

	amlogic->pcie_rst5 = devm_reset_control_get_exclusive(dev, "pcie_rst5");
	if (IS_ERR(amlogic->pcie_rst5)) {
		if (PTR_ERR(amlogic->pcie_rst5) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst5 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst5);
	}

	amlogic->pcie_rst6 = devm_reset_control_get_exclusive(dev, "pcie_rst6");
	if (IS_ERR(amlogic->pcie_rst6)) {
		if (PTR_ERR(amlogic->pcie_rst6) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst6 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst6);
	}

	amlogic->pcie_rst7 = devm_reset_control_get_exclusive(dev, "pcie_rst7");
	if (IS_ERR(amlogic->pcie_rst7)) {
		if (PTR_ERR(amlogic->pcie_rst7) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst7 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst7);
	}

	return 0;

get_rst_reg:
	rst_regs = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"reset-base");
	amlogic->rst_base = devm_ioremap(dev, rst_regs->start,
					 resource_size(rst_regs));
	if (IS_ERR(amlogic->rst_base)) {
		dev_err(dev, "failed to request rst_base\n");
		return PTR_ERR(amlogic->rst_base);
	}

	ret = of_property_read_u32(node, "pcie-m31phy-rst-bit",
				   &amlogic->m31phy_rst_bit);
	if (ret) {
		dev_err(dev, "failed to request m31phy_rst_bit\n");
		return ret;
	}

	ret = of_property_read_u32(node, "pcie-gen3-l0-rst-bit",
				   &amlogic->gen3_l0_rst_bit);
	if (ret) {
		dev_err(dev, "failed to request gen3_l0_rst_bit\n");
		return ret;
	}

	ret = of_property_read_u32(node, "pcie-apb-rst-bit",
				   &amlogic->apb_rst_bit);
	if (ret) {
		dev_err(dev, "failed to request apb_rst_bit\n");
		return ret;
	}

	ret = of_property_read_u32(node, "pcie-phy-rst-bit",
				   &amlogic->phy_rst_bit);
	if (ret) {
		dev_err(dev, "failed to request phy_rst_bit\n");
		return ret;
	}

	ret = of_property_read_u32(node, "pcie-a-rst-bit",
				   &amlogic->pcie_a_rst_bit);
	if (ret) {
		dev_err(dev, "failed to request pcie_a_rst_bit\n");
		return ret;
	}

	ret = of_property_read_u32(node, "pcie-rst-bit",
				   &amlogic->pcie_rst_bit);
	if (ret) {
		dev_err(dev, "failed to request pcie_rst_bit\n");
		return ret;
	}

	ret = of_property_read_u32(node, "pcie-rst-mask",
				   &amlogic->pcie_rst_mask);
	if (ret) {
		dev_err(dev, "failed to request pcie_rst_size\n");
		return ret;
	}

	return 0;
}

int amlogic_pcie_set_reset(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	int err = 0, val = 0;

	if (amlogic->rst_base)
		goto set_rst_reg;

	err = reset_control_deassert(amlogic->m31phy_rst);
	if (err < 0) {
		dev_err(dev, "deassert m31phy_rst err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->m31phy_rst);
	if (err < 0) {
		dev_err(dev, "assert m31phy_rst err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->gen3_l0_rst);
	if (err < 0) {
		dev_err(dev, "deassert gen3_l0_rst err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->gen3_l0_rst);
	if (err < 0) {
		dev_err(dev, "assert gen3_l0_rst err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_apb_rst);
	if (err < 0) {
		dev_err(dev, "deassert pcie_apb_rst err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_apb_rst);
	if (err < 0) {
		dev_err(dev, "assert pcie_apb_rst err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_phy_rst);
	if (err) {
		dev_err(dev, "deassert pcie_phy_rst err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_phy_rst);
	if (err < 0) {
		dev_err(dev, "assert pcie_phy_rst err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_a_rst);
	if (err) {
		dev_err(dev, "deassert pcie_a_rst err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_a_rst);
	if (err < 0) {
		dev_err(dev, "assert pcie_a_rst err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst0);
	if (err) {
		dev_err(dev, "deassert pcie_rst0 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst0);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst0 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst1);
	if (err) {
		dev_err(dev, "deassert pcie_rst1 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst1);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst1 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst2);
	if (err) {
		dev_err(dev, "deassert pcie_rst2 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst2);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst2 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst3);
	if (err) {
		dev_err(dev, "deassert pcie_rst3 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst3);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst3 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst4);
	if (err) {
		dev_err(dev, "deassert pcie_rst4 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst4);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst4 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst5);
	if (err) {
		dev_err(dev, "deassert pcie_rst5 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst5);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst5 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst6);
	if (err) {
		dev_err(dev, "deassert pcie_rst6 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst6);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst6 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst7);
	if (err) {
		dev_err(dev, "deassert pcie_rst7 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst7);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst7 err %d\n", err);
		return err;
	}

set_rst_reg:
	val = readl(amlogic->rst_base + RESETCTRL1_OFFSET);
	val &= ~(1 << amlogic->m31phy_rst_bit);
	writel(val, amlogic->rst_base + RESETCTRL1_OFFSET);
	val = readl(amlogic->rst_base + RESETCTRL1_OFFSET);
	val |= (1 << amlogic->m31phy_rst_bit);
	writel(val, amlogic->rst_base + RESETCTRL1_OFFSET);

	val = readl(amlogic->rst_base + RESETCTRL1_OFFSET);
	val &= ~((1 << amlogic->pcie_a_rst_bit) |
		 (1 << amlogic->phy_rst_bit) |
		 (1 << amlogic->apb_rst_bit));
		 //(1 << amlogic->gen3_l0_rst_bit));
	writel(val, amlogic->rst_base + RESETCTRL1_OFFSET);
	val = readl(amlogic->rst_base + RESETCTRL1_OFFSET);
	val |= ((1 << amlogic->pcie_a_rst_bit) |
		 (1 << amlogic->phy_rst_bit) |
		 (1 << amlogic->apb_rst_bit));
		 //(1 << amlogic->gen3_l0_rst_bit));
	writel(val, amlogic->rst_base + RESETCTRL1_OFFSET);

	val = readl(amlogic->rst_base + RESETCTRL3_OFFSET);
	val &= ~(amlogic->pcie_rst_mask << amlogic->pcie_rst_bit);
	writel(val, amlogic->rst_base + RESETCTRL3_OFFSET);
	val = readl(amlogic->rst_base + RESETCTRL3_OFFSET);
	val |= (amlogic->pcie_rst_mask << amlogic->pcie_rst_bit);
	writel(val, amlogic->rst_base + RESETCTRL3_OFFSET);

	return 0;
}

void amlogic_pcie_set_reset_gpio(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	int ret = 0;

	/*reset-gpio-type 0:Shared pad(no reset)1:OD pad2:Normal pad*/
	if (amlogic->gpio_type == 0) {
		dev_info(dev, "gpio multiplex, don't reset!\n");
	} else if (amlogic->gpio_type == 1) {
		dev_info(dev, "pad gpio\n");
		if (amlogic->reset_gpio >= 0)
			ret = devm_gpio_request(dev,
						amlogic->reset_gpio,
						"RESET");

		if (!ret && gpio_is_valid(amlogic->reset_gpio)) {
			dev_info(dev, "GPIO pad: assert reset\n");
			gpio_direction_output(amlogic->reset_gpio, 0);
			usleep_range(5000, 6000);
			gpio_direction_input(amlogic->reset_gpio);
		}
	} else {
		dev_info(dev, "normal gpio\n");
		if (amlogic->reset_gpio >= 0) {
			ret = devm_gpio_request(dev,
						amlogic->reset_gpio,
						"RESET");
			if (!ret)
				gpio_direction_output(amlogic->reset_gpio, 0);
		}
		if (gpio_is_valid(amlogic->reset_gpio)) {
			dev_info(dev, "GPIO normal: assert reset\n");
			gpio_set_value_cansleep(amlogic->reset_gpio, 0);
			usleep_range(5000, 6000);
			gpio_set_value_cansleep(amlogic->reset_gpio, 1);
		}
	}
}

int amlogic_pcie_parse_dt(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct device_node *node = dev->of_node;
	struct resource *res;
	int err;

	if (of_get_property(dev->of_node, "pinctrl-names", NULL))
		amlogic->p = devm_pinctrl_get_select_default(dev);

	err = of_property_read_u32(node, "gpio-type",
				   &amlogic->gpio_type);

	amlogic->reset_gpio = of_get_named_gpio(node, "reset-gpio", 0);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "apb-base");
	amlogic->apb_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(amlogic->apb_base)) {
		dev_err(dev, "failed to request apb_base\n");
		return PTR_ERR(amlogic->apb_base);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "pcictrl-base");
	amlogic->pcictrl_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(amlogic->pcictrl_base)) {
		dev_err(dev, "failed to request pcictrl_base\n");
		return PTR_ERR(amlogic->pcictrl_base);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "ecam-base");
	if (res) {
		amlogic->ecam_size = resource_size(res);
		amlogic->ecam_bus_base  = res->start;
	} else {
		dev_err(dev, "Missing *ecam-base* reg space\n");
		return -ENODEV;
	}
	amlogic->ecam_base = devm_pci_remap_cfg_resource(dev, res);
	if (IS_ERR(amlogic->ecam_base)) {
		dev_err(dev, "failed to request ecam_base\n");
		return PTR_ERR(amlogic->ecam_base);
	}

	err = amlogic_pcie_get_phys(amlogic);
	if (err)
		return err;

	amlogic->lanes = 1;
	err = of_property_read_u32(node, "num-lanes", &amlogic->lanes);
	if (!err && amlogic->lanes == 0) {
		dev_err(dev, "invalid num-lanes, default to use one lane\n");
		amlogic->lanes = 1;
	}

	err = amlogic_pcie_get_reset(amlogic);
	if (err)
		return err;

	amlogic->pcie_400m_clk = devm_clk_get(dev, "pcie_400m_clk");
	if (IS_ERR(amlogic->pcie_400m_clk)) {
		dev_err(dev, "pcie_400m_clk not found\n");
		return PTR_ERR(amlogic->pcie_400m_clk);
	}

	amlogic->pcie_tl_clk = devm_clk_get(dev, "pcie_tl_clk");
	if (IS_ERR(amlogic->pcie_tl_clk)) {
		dev_err(dev, "pcie_tl_clk not found\n");
		return PTR_ERR(amlogic->pcie_tl_clk);
	}

	amlogic->cts_pcie_clk = devm_clk_get(dev, "cts_pcie_clk");
	if (IS_ERR(amlogic->cts_pcie_clk)) {
		dev_err(dev, "cts_pcie_clk not found\n");
		return PTR_ERR(amlogic->cts_pcie_clk);
	}

	amlogic->pcie_clk = devm_clk_get(dev, "pcie");
	if (IS_ERR(amlogic->pcie_clk)) {
		dev_err(dev, "pcie_clk not found\n");
		return PTR_ERR(amlogic->pcie_clk);
	}

	amlogic->phy_clk = devm_clk_get(dev, "pcie_phy");
	if (IS_ERR(amlogic->phy_clk)) {
		dev_err(dev, "phy_clk not found\n");
		return PTR_ERR(amlogic->phy_clk);
	}

	amlogic->refpll_clk = devm_clk_get(dev, "pcie_refpll");
	if (IS_ERR(amlogic->refpll_clk)) {
		dev_err(dev, "refpll_clk not found\n");
		return PTR_ERR(amlogic->refpll_clk);
	}

	amlogic->dev_clk = devm_clk_get(dev, "pcie_hcsl");
	if (IS_ERR(amlogic->dev_clk)) {
		dev_err(dev, "dev_clk not found\n");
		return PTR_ERR(amlogic->dev_clk);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(amlogic_pcie_parse_dt);

bool amlogic_pcie_link_up(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	u32 ltssm_up = 0;
	u32 neg_link_speed = 0;
	int cnt = 0;
	u32 val = 0;

	do {
		ltssm_up = amlogic_pciectrl_read(amlogic, PCIE_A_CTRL5);
		ltssm_up = ((ltssm_up >> 18) & 0x1f) == 0x10 ? 1 : 0;

		val = amlogic_pcieinter_read(amlogic, PCIE_BASIC_STATUS);
		neg_link_speed = (val >> 8) & 0xf;

		if (ltssm_up)
			dev_dbg(dev, "ltssm_up is on\n");
		if (neg_link_speed)
			dev_dbg(dev, "speed_okay\n");

		if (cnt >= WAIT_LINKUP_TIMEOUT) {
			dev_err(dev, "Error: Wait linkup timeout.\n");
			return false;
		}

		cnt++;
		udelay(10);
	} while ((ltssm_up == 0) || (!(val & LINK_UP_MASK)));
	return true;
}

void amlogic_set_max_rd_req_size(struct amlogic_pcie *amlogic, int size)
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

	val = amlogic_pcieinter_read(amlogic,
				     PCIE_CAP_OFFSET + PCI_EXP_DEVCTL);
	val &= (~PCI_EXP_DEVCTL_READRQ);
	val |= (max_rd_req_size << 12);
	amlogic_pcieinter_write(amlogic, val,
				PCIE_CAP_OFFSET + PCI_EXP_DEVCTL);
}

void amlogic_set_max_payload(struct amlogic_pcie *amlogic, int size)
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

	val = amlogic_pcieinter_read(amlogic,
				     PCIE_CAP_OFFSET + PCI_EXP_DEVCTL);
	val &= (~PCI_EXP_DEVCTL_PAYLOAD);
	val |= (max_payload_size << 5);
	amlogic_pcieinter_write(amlogic, val,
				PCIE_CAP_OFFSET + PCI_EXP_DEVCTL);
}

int amlogic_pcie_init_port(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	int err;
	u32 regs;

	err = amlogic_pcie_set_reset(amlogic);
	if (err)
		return err;

	/*set phy for gen3 device*/
	regs = readl(amlogic->phy_base);
	regs |= BIT(19);
	writel(regs, amlogic->phy_base);
	usleep_range(20, 30);

	amlogic_pcie_set_reset_gpio(amlogic);

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
EXPORT_SYMBOL_GPL(amlogic_pcie_init_port);

int amlogic_pcie_get_phys(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct resource *res;
	struct phy *phy;

	phy = devm_of_phy_get(dev, dev->of_node, "pcie-phy");
	if (IS_ERR(phy)) {
		if (PTR_ERR(phy) != -EPROBE_DEFER)
			goto get_phy_reg;
	}

get_phy_reg:
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "phy-base");
	amlogic->phy_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(amlogic->phy_base)) {
		dev_err(dev, "failed to request phy_base\n");
		return PTR_ERR(amlogic->phy_base);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(amlogic_pcie_get_phys);

void amlogic_pcie_deinit_phys(struct amlogic_pcie *amlogic)
{
}
EXPORT_SYMBOL_GPL(amlogic_pcie_deinit_phys);

void amlogic_pcie_init_phys(struct amlogic_pcie *amlogic)
{
}
EXPORT_SYMBOL_GPL(amlogic_pcie_init_phys);

int amlogic_pcie_enable_clocks(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	int err = 0, ret = 0;

	ret = clk_set_rate(amlogic->pcie_400m_clk, 400000000);
	err = clk_prepare_enable(amlogic->pcie_400m_clk);
	if (err || ret) {
		dev_err(dev, "unable to enable pcie_400m_clk clock\n");
		return err;
	}

	ret = clk_set_rate(amlogic->pcie_tl_clk, 125000000);
	err = clk_prepare_enable(amlogic->pcie_tl_clk);
	if (err || ret) {
		dev_err(dev, "unable to enable pcie_tl_clk clock\n");
		goto err_400m_clk;
	}

	ret = clk_set_rate(amlogic->cts_pcie_clk, 200000000);
	err = clk_prepare_enable(amlogic->cts_pcie_clk);
	if (err || ret) {
		dev_err(dev, "unable to enable cts_pcie_clk clock\n");
		goto err_tl_clk;
	}

	err = clk_prepare_enable(amlogic->dev_clk);
	if (err) {
		dev_err(dev, "unable to enable dev_clk clock\n");
		goto err_cts_pcie_clk;
	}

	err = clk_prepare_enable(amlogic->phy_clk);
	if (err) {
		dev_err(dev, "unable to enable phy_clk clock\n");
		goto err_dev_clk;
	}

	err = clk_prepare_enable(amlogic->refpll_clk);
	if (err) {
		dev_err(dev, "unable to enable refpll_clk clock\n");
		goto err_phy_clk;
	}

	err = clk_prepare_enable(amlogic->pcie_clk);
	if (err) {
		dev_err(dev, "unable to enable pcie_clk clock\n");
		goto err_refpll_clk;
	}

	return 0;

err_refpll_clk:
	clk_disable_unprepare(amlogic->refpll_clk);
err_phy_clk:
	clk_disable_unprepare(amlogic->phy_clk);
err_dev_clk:
	clk_disable_unprepare(amlogic->dev_clk);
err_cts_pcie_clk:
	clk_disable_unprepare(amlogic->cts_pcie_clk);
err_tl_clk:
	clk_disable_unprepare(amlogic->pcie_tl_clk);
err_400m_clk:
	clk_disable_unprepare(amlogic->pcie_400m_clk);
	return err;
}
EXPORT_SYMBOL_GPL(amlogic_pcie_enable_clocks);

void amlogic_pcie_disable_clocks(struct amlogic_pcie *amlogic)
{
	clk_disable_unprepare(amlogic->pcie_clk);
	clk_disable_unprepare(amlogic->refpll_clk);
	clk_disable_unprepare(amlogic->phy_clk);
	clk_disable_unprepare(amlogic->dev_clk);
	clk_disable_unprepare(amlogic->cts_pcie_clk);
	clk_disable_unprepare(amlogic->pcie_tl_clk);
	clk_disable_unprepare(amlogic->pcie_400m_clk);
}
EXPORT_SYMBOL_GPL(amlogic_pcie_disable_clocks);

void amlogic_pcie_cfg_addr_map(struct amlogic_pcie *amlogic,
			       unsigned int atr_base,
			       u64 src_addr,
			       u64 trsl_addr,
			       int size,
			       int trsl_param)
{
	struct device *dev = amlogic->dev;
	u32 val;

	/* ATR_SRC_ADDR_LOW:
	 *  - bit 0: enable entry,
	 *  - bits 1-6: ATR window size: total size in bytes: 2^(ATR_WSIZE + 1)
	 *  - bits 7-11: reserved
	 *  - bits 12-31: start of source address
	 */
	val = (src_addr & 0xfffff000) | ((size & 0x3f) << 1) | (1 << 0);
	amlogic_pcieinter_write(amlogic, val, atr_base + ATR_SRC_ADDR_LOW);

	amlogic_pcieinter_write(amlogic,
				(src_addr >> 32),
				atr_base + ATR_SRC_ADDR_HIGH);
	amlogic_pcieinter_write(amlogic,
				(trsl_addr & 0xfffff000),
				atr_base + ATR_TRSL_ADDR_LOW);
	amlogic_pcieinter_write(amlogic,
				(trsl_addr >> 32),
				atr_base + ATR_TRSL_ADDR_HIGH);
	amlogic_pcieinter_write(amlogic, trsl_param,
				atr_base + ATR_TRSL_PARAM);

	dev_dbg(dev,
		 "ATR Map:0x%010llx %s 0x%010llx [0x%010llx] (param: 0x%06x)\n",
		 src_addr, (trsl_param & 0x400000) ? "<-" : "->", trsl_addr,
		 ((u64)1) << (size + 1), trsl_param);
}
EXPORT_SYMBOL_GPL(amlogic_pcie_cfg_addr_map);
