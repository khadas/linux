/*Amlogic Meson DWMAC glue layer
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/device.h>
#include <linux/ethtool.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/delay.h>
#include <linux/of_device.h>
#include <linux/gpio/consumer.h>
#include <linux/stmmac.h>
#include "stmmac.h"
#define ETHMAC_SPEED_100	BIT(1)
void __iomem *PREG_ETH_REG2;
void __iomem *PREG_ETH_REG3;
void __iomem *PREG_ETH_REG4;

struct meson_dwmac {
	struct device *dev;
	void __iomem *reg;
	void __iomem *preg_power_reg;
};

static void meson_dwmac_fix_mac_speed(void *priv, unsigned int speed)
{
}
static void __iomem *network_interface_setup(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct gpio_desc *gdesc;
	struct gpio_desc *gdesc_z4;
	struct gpio_desc *gdesc_z5;
	struct pinctrl *pin_ctl;
	struct resource *res;
	struct resource *res2;
	u32 mc_val, cali_val, internal_phy;
	void __iomem *addr = NULL;
	void __iomem *addr2 = NULL;

	/*map reg0 and reg 1 addr.*/
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	addr = devm_ioremap_resource(dev, res);
	PREG_ETH_REG0 = addr;
	PREG_ETH_REG1 = addr+4;
	pr_debug("REG0:REG1 = %p :%p\n", PREG_ETH_REG0, PREG_ETH_REG1);

	/* Get mec mode & ting value  set it in cbus2050 */
	pr_debug("mem start:0x%llx , %llx , [%p]\n", (long long)res->start,
					(long long)(res->end - res->start),
					addr);
	if (of_property_read_u32(np, "mc_val", &mc_val)) {
		pr_debug("detect cbus[2050]=null, plesae setting mc_val\n");
		pr_debug(" IF RGMII setting 0x7d21 else rmii setting 0x1000");
	} else {
		pr_debug("Ethernet :got mc_val 0x%x .set it\n", mc_val);
		writel(mc_val, addr);
	}
	if (of_property_read_u32(np, "cali_val", &cali_val)) {
		pr_debug("detect cbus[2051]=null, plesae setting cali_val\n");
	} else {
		pr_debug("Ethernet :got cali_val 0x%x .set it\n", cali_val);
		writel(cali_val, addr+4);
	}
	if (!of_property_read_u32(np, "internal_phy", &internal_phy)) {
		res2 = platform_get_resource(pdev, IORESOURCE_MEM, 2);
		addr2 = devm_ioremap_resource(dev, res2);
		PREG_ETH_REG2 = addr2;
		PREG_ETH_REG3 = addr2+4;
		PREG_ETH_REG4 = addr2+8;
		if (internal_phy == 1) {
			pr_debug("internal phy\n");
			writel(0x10110181, PREG_ETH_REG2);
			writel(0xe489087f, PREG_ETH_REG3);
			pin_ctl = devm_pinctrl_get_select(&pdev->dev,
				"internal_eth_pins");
		} else {
			writel(0x10110181, PREG_ETH_REG2);
			writel(0x2009087f, PREG_ETH_REG3);
			/* pull reset pin for resetting phy  */
			gdesc = gpiod_get(&pdev->dev, "rst_pin");
			gdesc_z4 = gpiod_get(&pdev->dev, "GPIOZ4_pin");
			gdesc_z5 = gpiod_get(&pdev->dev, "GPIOZ5_pin");
			if (!IS_ERR(gdesc) && !IS_ERR(gdesc_z4)) {
				gpiod_direction_output(gdesc_z4, 0);
				gpiod_direction_output(gdesc_z5, 0);
				gpiod_direction_output(gdesc, 0);
				mdelay(20);
				gpiod_direction_output(gdesc, 1);
				mdelay(100);
				gpiod_put(gdesc_z4);
				gpiod_put(gdesc_z5);
				pr_debug("Ethernet: gpio reset ok\n");
			}
			pin_ctl = devm_pinctrl_get_select(&pdev->dev,
			"external_eth_pins");
		}
		pr_debug("REG2:REG3:REG4 = 0x%x :0x%x :0x%x\n",
			readl(PREG_ETH_REG2), readl(PREG_ETH_REG3),
			readl(PREG_ETH_REG4));
	} else {
		pin_ctl = devm_pinctrl_get_select(&pdev->dev, "eth_pins");
	}
	pr_debug("Ethernet: pinmux setup ok\n");
	return addr;
}

static void *meson_dwmac_setup(struct platform_device *pdev)
{
	struct meson_dwmac *dwmac;
	struct resource  *res;
	struct device *dev = &pdev->dev;
	dwmac = devm_kzalloc(&pdev->dev, sizeof(*dwmac), GFP_KERNEL);
	if (!dwmac)
		return ERR_PTR(-ENOMEM);

	dwmac->reg = network_interface_setup(pdev);
	if (IS_ERR(dwmac->reg))
		return dwmac->reg;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (res) {
		dwmac->preg_power_reg = devm_ioremap_resource(dev, res);
		if (dwmac->preg_power_reg != NULL)
			writel(readl(dwmac->preg_power_reg) |
				(0x1<<11), dwmac->preg_power_reg);
	}

	return dwmac;
}

static void meson_dwmac_exit(struct platform_device *pdev, void *priv)
{
	struct meson_dwmac *dwmac = priv;
	if (dwmac->preg_power_reg != NULL)
		writel(readl(dwmac->preg_power_reg) & ~(0x1<<11),
			dwmac->preg_power_reg);
	return;
}

static int meson_dwmac_init(struct platform_device *pdev, void *priv)
{
	struct meson_dwmac *dwmac = priv;

	if (dwmac->preg_power_reg != NULL)
		writel(readl(dwmac->preg_power_reg) | (0x1<<11),
			dwmac->preg_power_reg);
	return 0;
}
const struct stmmac_of_data meson_dwmac_data = {
	.setup = meson_dwmac_setup,
	.init = meson_dwmac_init,
	.exit = meson_dwmac_exit,
	.fix_mac_speed = meson_dwmac_fix_mac_speed,
};
