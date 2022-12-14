/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Amlogic AXI PCIe controller driver
 *
 * Copyright (c) 2018 Amlogic, Inc.
 *
 */

#ifndef _PCIE_AMLOGIC_V3_H
#define _PCIE_AMLOGIC_V3_H
#include <linux/pci.h>
#include <linux/kernel.h>
/*
 * The upper 16 bits of PCIE_CLIENT_CONFIG are a write mask for the lower 16
 * bits.  This allows atomic updates of the register without locking.
 */
#define PCIE_BASIC_STATUS	0x018
#define  LINK_UP_MASK		0xff

#define PCIE_CFGCTRL		0x084
#define PCIE_PCI_IDS0		0x098
#define PCIE_PCI_IDS1		0x09c
#define PCIE_PCI_IDS2		0x0a0
#define PCIE_PCI_IRQ		0x0a8
#define PCIE_BAR_0		0x0e4
#define PCIE_BAR_1		0x0e8
#define PCIE_BAR_2		0x0ec
#define PCIE_BAR_3		0x0f0
#define PCIE_BAR_4		0x0f4
#define PCIE_BAR_5		0x0f8
#define   PCIE_BAR_IO			BIT(1)
#define   PCIE_BAR_MEM_TYPE_64		BIT(2)
#define   PCIE_BAR_MEM_TYPE_PREFETCH	BIT(3)
#define   PCIE_BAR_MEM_MASK	(~0x0fUL)
#define   PCIE_BAR_IO_MASK	(~0x03UL)

#define PCIE_CFGNUM		0x140
#define IMASK_LOCAL		0x180
#define ISTATUS_LOCAL		0x184
#define IMASK_HOST		0x188
#define ISTATUS_HOST		0x18c
#define IMSI_ADDR		0x190
#define ISTATUS_MSI		0x194
#define ATR_PCIE_WIN0		0x600
#define ATR_PCIE_WIN1		0x700
#define ATR_AXI4_SLV0		0x800

#define ATR_TABLE_SIZE		0x20
#define ATR_SRC_ADDR_LOW	0x0
#define ATR_SRC_ADDR_HIGH	0x4
#define ATR_TRSL_ADDR_LOW	0x8
#define ATR_TRSL_ADDR_HIGH	0xc
#define ATR_TRSL_PARAM		0x10

#define ATR_TRSLID_AXIDEVICE	(0x420004)
/* Write-through, read/write allocate */
#define ATR_TRSLID_AXIMEMORY	(0x4e0004)
#define ATR_TRSLID_PCIE_CONF	(0x000001)
#define ATR_TRSLID_PCIE_IO	(0x020000)
/*#define ATR_TRSLID_PCIE_IO	(0x020001)*/
#define ATR_TRSLID_PCIE_MEMORY	(0x000000)

#define INT_AXI_POST_ERROR	BIT(16)
#define INT_AXI_FETCH_ERROR	BIT(17)
#define INT_AXI_DISCARD_ERROR	BIT(18)
#define INT_PCIE_POST_ERROR	BIT(20)
#define INT_PCIE_FETCH_ERROR	BIT(21)
#define INT_PCIE_DISCARD_ERROR	BIT(22)
#define INT_ERRORS		(INT_AXI_POST_ERROR | INT_AXI_FETCH_ERROR |    \
				 INT_AXI_DISCARD_ERROR | INT_PCIE_POST_ERROR | \
				 INT_PCIE_FETCH_ERROR | INT_PCIE_DISCARD_ERROR)

#define INTA_OFFSET		24
#define INTA			BIT(24)
#define INTB			BIT(25)
#define INTC			BIT(26)
#define INTD			BIT(27)
#define INT_MSI			BIT(28)
#define INT_INTX_MASK		(INTA | INTB | INTC | INTD)
#define INT_MASK		(INT_INTX_MASK | INT_MSI | INT_ERRORS)

#define INTX_NUM		4
#define INT_PCI_MSI_NR		32

#define DWORD_MASK		3

#define PCI_CFG_SPACE			0x1000
#define PCIE_HEADER_TYPE_OFFSET		(PCI_CFG_SPACE + 0x00)
#define PCIE_CAP_OFFSET			(PCI_CFG_SPACE + 0x80)

#define PCIE_ECAM_BUS(x)			(((x) & 0xff) << 20)
#define PCIE_ECAM_DEV(x)			(((x) & 0x1f) << 15)
#define PCIE_ECAM_FUNC(x)			(((x) & 0x7) << 12)
#define PCIE_ECAM_REG(x)			(((x) & 0xfff) << 0)
#define PCIE_ECAM_ADDR(bus, dev, func, reg) \
	  (PCIE_ECAM_BUS(bus) | PCIE_ECAM_DEV(dev) | \
	   PCIE_ECAM_FUNC(func) | PCIE_ECAM_REG(reg))

#define PCIE_A_CTRL0	0x0
#define   PORT_TYPE	BIT(0)
#define PCIE_A_CTRL1	0x4
#define PCIE_A_CTRL2	0x8
#define PCIE_A_CTRL3	0xc
#define PCIE_A_CTRL4	0x10
#define PCIE_A_CTRL5	0x14
#define PCIE_A_CTRL6	0x18

#define EP_BASE_OFFSET		0
#define AMLOGIC_PCIE_EP_FUNC_BASE(fn)	(((fn) << 12) & GENMASK(19, 12))
#define EP_FUNC_MSI_CAP_OFFSET		0x0e0

#define RESETCTRL0_OFFSET	0x0
#define RESETCTRL1_OFFSET	0x4
#define RESETCTRL3_OFFSET	0xc

#define WAIT_LINKUP_TIMEOUT         9000

enum pcie_data_rate {
	PCIE_GEN1,
	PCIE_GEN2,
	PCIE_GEN3,
	PCIE_GEN4
};

enum pcie_phy_type {
	M31_PHY,
	M31_COMBPHY,
	AMLOGIC_PHY,
};

struct amlogic_pcie {
	void __iomem *apb_base;		/* pcie_axi_lite for internal*/
	void __iomem *axi_base;		/* pcie_ctrl for data*/
	void __iomem *pcictrl_base;		/* pcie_A_ctrl for oneself*/
	void __iomem *phy_base;		/* pcie_gen3_phy*/
	void __iomem *rst_base;		/* reset_ctrl*/
	void __iomem *ecam_base;	/* ecam*/
	phys_addr_t ecam_bus_base;
	u32 ecam_size;

	struct resource	*io;
	phys_addr_t io_bus_addr;
	u32 io_size;
	u32 mem_size;
	phys_addr_t mem_bus_addr;

	struct phy	*pcie_phy;
	u32 phy_type;

	int reset_gpio;
	u32 gpio_type;

	struct clk *pcie_400m_clk;
	struct clk *pcie_tl_clk;
	struct clk *cts_pcie_clk;
	struct clk *pcie_clk;
	struct clk *phy_clk;
	struct clk *refpll_clk;
	struct clk *dev_clk;

	struct reset_control *m31phy_rst;/*RESETCTRL_RESET1 bit 21*/
	struct reset_control *gen3_l0_rst; /*RESETCTRL_RESET1 bit 18*/
	struct reset_control *gen2_l0_rst;
	struct reset_control *pcie_apb_rst; /*RESETCTRL_RESET1 bit 14*/
	struct reset_control *pcie_phy_rst; /*RESETCTRL_RESET1 bit 13*/
	struct reset_control *pcie_a_rst;   /*RESETCTRL_RESET1 bit 12*/
	struct reset_control *pcie_rst0;    /*RESETCTRL_RESET3 bit 12*/
	struct reset_control *pcie_rst1;    /*RESETCTRL_RESET3 bit 13*/
	struct reset_control *pcie_rst2;    /*RESETCTRL_RESET3 bit 14*/
	struct reset_control *pcie_rst3;    /*RESETCTRL_RESET3 bit 15*/
	struct reset_control *pcie_rst4;    /*RESETCTRL_RESET3 bit 16*/
	struct reset_control *pcie_rst5;    /*RESETCTRL_RESET3 bit 17*/
	struct reset_control *pcie_rst6;    /*RESETCTRL_RESET3 bit 18*/
	struct reset_control *pcie_rst7;    /*RESETCTRL_RESET3 bit 19*/

	struct device *dev;

	struct pinctrl *p;

	u32 m31phy_rst_bit;
	u32 gen3_l0_rst_bit;
	u32 gen2_l0_rst_bit;
	u32 apb_rst_bit;
	u32 phy_rst_bit;
	u32 pcie_a_rst_bit;
	u32 pcie_rst_bit;
	u32 pcie_rst_mask;

	bool is_rc;

	u32 lanes;
	u32 port_num;

	u8 lanes_map;
	int link_gen;
	int offset;
	struct resource	*mem_res;
};

#define PTR_ALIGN_DOWN(p, a)    ((typeof(p))ALIGN_DOWN((unsigned long)(p), (a)))

static inline u32 amlogic_pcie_read_sz(void __iomem *addr, int size)
{
	void __iomem *aligned_addr = PTR_ALIGN_DOWN(addr, 0x4);
	unsigned int offset = (unsigned long)addr & 0x3;
	u32 val = readl(aligned_addr);

	if (!IS_ALIGNED((uintptr_t)addr, size)) {
		pr_warn("Address %p and size %d are not aligned\n", addr, size);
		return 0;
	}

	if (size > 2)
		return val;

	return (val >> (8 * offset)) & ((1 << (size * 8)) - 1);
}

static inline void amlogic_pcie_write_sz(void __iomem *addr, int size,
					 u32 value)
{
	void __iomem *aligned_addr = PTR_ALIGN_DOWN(addr, 0x4);
	unsigned int offset = (unsigned long)addr & 0x3;
	u32 mask;
	u32 val;

	if (!IS_ALIGNED((uintptr_t)addr, size)) {
		pr_warn("Address %p and size %d are not aligned\n", addr, size);
		return;
	}

	if (size > 2) {
		writel(value, addr);
		return;
	}

	mask = ~(((1 << (size * 8)) - 1) << (offset * 8));
	val = readl(aligned_addr) & mask;
	val |= value << (offset * 8);
	writel(val, aligned_addr);
}

static inline void amlogic_pcie_ep_fn_writeb(struct amlogic_pcie *pcie, u8 fn,
					     u32 reg, u8 value)
{
	void __iomem *addr = pcie->ecam_base + EP_BASE_OFFSET +
				AMLOGIC_PCIE_EP_FUNC_BASE(fn) + reg;

	amlogic_pcie_write_sz(addr, 0x1, value);
}

static inline void amlogic_pcie_ep_fn_writew(struct amlogic_pcie *pcie, u8 fn,
					     u32 reg, u16 value)
{
	void __iomem *addr = pcie->ecam_base + EP_BASE_OFFSET +
				AMLOGIC_PCIE_EP_FUNC_BASE(fn) + reg;

	amlogic_pcie_write_sz(addr, 0x2, value);
}

static inline void amlogic_pcie_ep_fn_writel(struct amlogic_pcie *pcie, u8 fn,
					  u32 reg, u32 value)
{
	writel(value, pcie->ecam_base +  EP_BASE_OFFSET +
		AMLOGIC_PCIE_EP_FUNC_BASE(fn) + reg);
}

static inline u16 amlogic_pcie_ep_fn_readw(struct amlogic_pcie *pcie,
					   u8 fn, u32 reg)
{
	void __iomem *addr = pcie->ecam_base + EP_BASE_OFFSET +
			     AMLOGIC_PCIE_EP_FUNC_BASE(fn) + reg;

	return amlogic_pcie_read_sz(addr, 0x2);
}

static inline u32 amlogic_pcie_ep_fn_readl(struct amlogic_pcie *pcie,
					   u8 fn, u32 reg)
{
	return readl(pcie->ecam_base + EP_BASE_OFFSET +
		     AMLOGIC_PCIE_EP_FUNC_BASE(fn) + reg);
}

static inline u32 amlogic_pcieinter_read(struct amlogic_pcie *pcie, u32 reg)
{
	if (reg < PCI_CFG_SPACE)
		return readl(pcie->apb_base + reg);
	else
		return readl(pcie->ecam_base +
			     PCIE_ECAM_ADDR(0, 0, 0, reg - PCI_CFG_SPACE));
}

static inline void amlogic_pcieinter_write(struct amlogic_pcie *pcie, u32 val,
				    u32 reg)
{
	if (reg < PCI_CFG_SPACE)
		writel(val, pcie->apb_base + reg);
	else
		writel(val, pcie->ecam_base +
		       PCIE_ECAM_ADDR(0, 0, 0, reg - PCI_CFG_SPACE));
}

static inline u32 amlogic_pciectrl_read(struct amlogic_pcie *pcie, u32 reg)
{
	return readl(pcie->pcictrl_base + reg);
}

static inline void amlogic_pciectrl_write(struct amlogic_pcie *pcie, u32 val,
					  u32 reg)
{
	writel(val, pcie->pcictrl_base + reg);
}

int amlogic_pcie_set_reset(struct amlogic_pcie *amlogic, bool set);
int amlogic_pcie_parse_dt(struct amlogic_pcie *amlogic);
int amlogic_pcie_init_port(struct amlogic_pcie *amlogic);
int amlogic_pcie_get_phys(struct amlogic_pcie *amlogic);
void amlogic_pcie_deinit_phys(struct amlogic_pcie *amlogic);
int amlogic_pcie_enable_clocks(struct amlogic_pcie *amlogic);
void amlogic_pcie_disable_clocks(struct amlogic_pcie *amlogic);
bool amlogic_pcie_link_up(struct amlogic_pcie *amlogic);
void amlogic_set_max_rd_req_size(struct amlogic_pcie *amlogic, int size);
void amlogic_set_max_payload(struct amlogic_pcie *amlogic, int size);
void amlogic_pcie_cfg_addr_map(struct amlogic_pcie *amlogic,
			       unsigned int atr_base,
			       u64 src_addr,
			       u64 trsl_addr,
			       int size,
			       int trsl_param);

#endif /* _PCIE_AMLOGIC_V3_H */
