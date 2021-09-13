// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/reset.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/amlogic/vmem.h>

/* Register Map */
#define SPICC_RXDATA	0x00

#define SPICC_TXDATA	0x04

#define SPICC_CONREG	0x08
#define SPICC_ENABLE		BIT(0)
#define SPICC_MODE_MASTER	BIT(1)
#define SPICC_XCH		BIT(2)
#define SPICC_SMC		BIT(3)
#define SPICC_POL		BIT(4)
#define SPICC_PHA		BIT(5)
#define SPICC_SSCTL		BIT(6)
#define SPICC_SSPOL		BIT(7)
#define SPICC_DRCTL_MASK	GENMASK(9, 8)
#define SPICC_DRCTL_IGNORE	0
#define SPICC_DRCTL_FALLING	1
#define SPICC_DRCTL_LOWLEVEL	2
#define SPICC_CS_MASK		GENMASK(13, 12)
#define SPICC_DATARATE_MASK	GENMASK(18, 16)
#define SPICC_FIX_FACTOR_MULT	1
#define SPICC_FIX_FACTOR_DIV	4
#define SPICC_BITLENGTH_MASK	GENMASK(24, 19)
#define SPICC_BURSTLENGTH_MASK	GENMASK(31, 25)

#define SPICC_INTREG	0x0c
#define SPICC_TE_EN	BIT(0) /* TX FIFO Empty Interrupt */
#define SPICC_TH_EN	BIT(1) /* TX FIFO Half-Full Interrupt */
#define SPICC_TF_EN	BIT(2) /* TX FIFO Full Interrupt */
#define SPICC_RR_EN	BIT(3) /* RX FIFO Ready Interrupt */
#define SPICC_RH_EN	BIT(4) /* RX FIFO Half-Full Interrupt */
#define SPICC_RF_EN	BIT(5) /* RX FIFO Full Interrupt */
#define SPICC_RO_EN	BIT(6) /* RX FIFO Overflow Interrupt */
#define SPICC_TC_EN	BIT(7) /* Transfert Complete Interrupt */

#define SPICC_DMAREG	0x10
#define SPICC_DMA_ENABLE		BIT(0)
/* When txfifo_count<threshold, request a read(dma->txfifo) burst */
#define SPICC_TXFIFO_THRESHOLD_MASK	GENMASK(5, 1)
#define SPICC_TXFIFO_THRESHOLD_DEFAULT	10
/* When rxfifo count>threshold, request a write(rxfifo->dma) burst */
#define SPICC_RXFIFO_THRESHOLD_MASK	GENMASK(10, 6)
#define SPICC_READ_BURST_MASK		GENMASK(14, 11)
#define SPICC_WRITE_BURST_MASK		GENMASK(18, 15)
#define SPICC_DMA_URGENT		BIT(19)
#define SPICC_DMA_THREADID_MASK		GENMASK(25, 20)
#define SPICC_DMA_BURSTNUM_MASK		GENMASK(31, 26)

#define SPICC_STATREG	0x14
#define SPICC_TE	BIT(0) /* TX FIFO Empty Interrupt */
#define SPICC_TH	BIT(1) /* TX FIFO Half-Full Interrupt */
#define SPICC_TF	BIT(2) /* TX FIFO Full Interrupt */
#define SPICC_RR	BIT(3) /* RX FIFO Ready Interrupt */
#define SPICC_RH	BIT(4) /* RX FIFO Half-Full Interrupt */
#define SPICC_RF	BIT(5) /* RX FIFO Full Interrupt */
#define SPICC_RO	BIT(6) /* RX FIFO Overflow Interrupt */
#define SPICC_TC	BIT(7) /* Transfert Complete Interrupt */

#define SPICC_PERIODREG	0x18
#define SPICC_PERIOD	GENMASK(14, 0)	/* Wait cycles */

#define SPICC_TESTREG	0x1c
#define SPICC_TXCNT_MASK	GENMASK(4, 0)	/* TX FIFO Counter */
#define SPICC_RXCNT_MASK	GENMASK(9, 5)	/* RX FIFO Counter */
#define SPICC_SMSTATUS_MASK	GENMASK(12, 10)	/* State Machine Status */
#define SPICC_LBC		BIT(14) /* Loop Back Control */
#define SPICC_SWAP		BIT(15) /* RX FIFO Data Swap */
#define SPICC_MO_DELAY_MASK	GENMASK(17, 16) /* Master Output Delay */
#define SPICC_MO_NO_DELAY	0
#define SPICC_MO_DELAY_1_CYCLE	1
#define SPICC_MO_DELAY_2_CYCLE	2
#define SPICC_MO_DELAY_3_CYCLE	3
#define SPICC_MI_DELAY_MASK	GENMASK(19, 18) /* Master Input Delay */
#define SPICC_MI_NO_DELAY	0
#define SPICC_MI_DELAY_1_CYCLE	1
#define SPICC_MI_DELAY_2_CYCLE	2
#define SPICC_MI_DELAY_3_CYCLE	3
#define SPICC_MI_CAP_DELAY_MASK	GENMASK(21, 20) /* Master Capture Delay */
#define SPICC_CAP_AHEAD_2_CYCLE	0
#define SPICC_CAP_AHEAD_1_CYCLE	1
#define SPICC_CAP_NO_DELAY	2
#define SPICC_CAP_DELAY_1_CYCLE	3
#define SPICC_FIFORST_MASK	GENMASK(23, 22) /* FIFO Softreset */

#define SPICC_DRADDR	0x20	/* Read Address of DMA */

#define SPICC_DWADDR	0x24	/* Write Address of DMA */

#define SPICC_LD_CNTL0	0x28
#define SPICC_LD_CNTL1	0x2c

#define SPICC_ENH_CTL0	0x38	/* Enhanced Feature 0 */
#define SPICC_ENH_CS_PRE_DELAY_MASK	GENMASK(15, 0)
#define SPICC_ENH_DATARATE_MASK		GENMASK(23, 16)
#define SPICC_ENH_FIX_FACTOR_MULT	1
#define SPICC_ENH_FIX_FACTOR_DIV	2
#define SPICC_ENH_DATARATE_EN		BIT(24)
#define SPICC_ENH_MOSI_OEN		BIT(25)
#define SPICC_ENH_CLK_OEN		BIT(26)
#define SPICC_ENH_CS_OEN		BIT(27)
#define SPICC_ENH_OEN			GENMASK(27, 25)
#define SPICC_ENH_CS_PRE_DELAY_EN	BIT(28)
#define SPICC_ENH_MAIN_CLK_AO		BIT(29)

#define SPICC_ENH_CTL1	0x3c	/* Enhanced Feature 1 */
#define SPICC_ENH_MI_CAP_DELAY_EN	BIT(0)
#define SPICC_ENH_MI_CAP_DELAY_MASK	GENMASK(9, 1)
#define SPICC_ENH_SI_CAP_DELAY_EN	BIT(14)		/* slave mode */
#define SPICC_ENH_DELAY_EN		BIT(15)
#define SPICC_ENH_SI_DELAY_EN		BIT(16)		/* slave mode */
#define SPICC_ENH_SI_DELAY_MASK		GENMASK(19, 17)	/* slave mode */
#define SPICC_ENH_MI_DELAY_EN		BIT(20)
#define SPICC_ENH_MI_DELAY_MASK		GENMASK(23, 21)
#define SPICC_ENH_MO_DELAY_EN		BIT(24)
#define SPICC_ENH_MO_DELAY_MASK		GENMASK(27, 25)
#define SPICC_ENH_MO_OEN_DELAY_EN	BIT(28)
#define SPICC_ENH_MO_OEN_DELAY_MASK	GENMASK(31, 29)

#define SPICC_ENH_CTL2	0x40	/* Enhanced Feature */
#define SPICC_ENH_TI_DELAY_MASK		GENMASK(14, 0)
#define SPICC_ENH_TI_DELAY_EN		BIT(15)
#define SPICC_ENH_TT_DELAY_MASK		GENMASK(30, 16)
#define SPICC_ENH_TT_DELAY_EN		BIT(31)

static inline void writel_bits_relaxed(u32 mask, u32 val, void __iomem *addr)
{
	writel_relaxed((readl_relaxed(addr) & ~(mask)) | (val), addr);
}

#define SPICC_BYTES_PER_WORD	8

#define SPICC_STATE_IDLE	0
#define SPICC_STATE_CMD		1
#define SPICC_STATE_DATA	2

#define VMEM_CMD_WRITE	1
#define VMEM_CMD_READ	2

#define VMEM_DEV_OF(d32)	(((d32) >> 0) & 0x3)
#define VMEM_CMD_OF(d32)	(((d32) >> 2) & 0xF)
#define VMEM_LEN_OF(d32)	(((d32) >> 6) & 0x3FF)
#define VMEM_OFFSET_OF(d32)	(((d32) >> 16) & 0xFFFF)

struct slave_spicc {
	void __iomem		*base;
	spinlock_t		lock;	/*for spicc interrupt handle */
	struct vmem_controller	*vmemctlr;
	struct vmem_device	*vmemdev;
	struct workqueue_struct *xfer_wq;
	struct work_struct	xfer_work;
	struct completion       xfer_completion;
	u16			mode;
	u8			cs_num;

	u8			state;
	u8			vmem_cmd_val;
	u16			vmem_offset;
	u16			vmem_len;
	u8			*vmem_buf;
	u16			vmem_count;
#ifdef SSPICC_TEST_ENTRY
	struct class		cls;
#endif
};

static void sspicc_hw_init(struct slave_spicc *spicc)
{
	u32 conf;

	/* Set slave mode and enable controller */
	conf = SPICC_ENABLE;
	/* Setup transfer mode */
	if (spicc->mode & SPI_CPOL)
		conf |= SPICC_POL;
	if (spicc->mode & SPI_CPHA)
		conf |= SPICC_PHA;
	if (spicc->mode & SPI_CS_HIGH)
		conf |= SPICC_SSPOL;
	if (spicc->mode & SPI_READY)
		conf |= FIELD_PREP(SPICC_DRCTL_MASK, SPICC_DRCTL_LOWLEVEL);
	/* Select CS 0 */
	conf |= FIELD_PREP(SPICC_CS_MASK, spicc->cs_num);
	/* Setup word width */
	conf |= FIELD_PREP(SPICC_BITLENGTH_MASK, 64 - 1);
	writel_relaxed(conf, spicc->base + SPICC_CONREG);

	/* Disable all IRQs */
	writel_relaxed(SPICC_RR_EN, spicc->base + SPICC_INTREG);

	/* Disable DMA */
	writel_relaxed(0, spicc->base + SPICC_DMAREG);

	/* Setup no wait cycles by default */
	writel_relaxed(0, spicc->base + SPICC_PERIODREG);

	/* Set sampling time point at middle */
	writel_relaxed(SPICC_ENH_DELAY_EN | SPICC_ENH_SI_CAP_DELAY_EN,
		       spicc->base + SPICC_ENH_CTL1);

	//writel_relaxed(SPICC_ENH_OEN, spicc->base + SPICC_ENH_CTL0);
}

static inline bool sspicc_txfull(struct slave_spicc *spicc)
{
	return !!FIELD_GET(SPICC_TF,
			   readl_relaxed(spicc->base + SPICC_STATREG));
}

static inline bool sspicc_rxready(struct slave_spicc *spicc)
{
	return FIELD_GET(SPICC_RH | SPICC_RR | SPICC_RF,
			 readl_relaxed(spicc->base + SPICC_STATREG));
}

static u32 sspicc_reset_fifo(struct slave_spicc *spicc)
{
	u32 data = 0;

	writel_bits_relaxed(SPICC_ENH_MAIN_CLK_AO,
			    SPICC_ENH_MAIN_CLK_AO,
			    spicc->base + SPICC_ENH_CTL0);

	writel_bits_relaxed(SPICC_FIFORST_MASK,
			    FIELD_PREP(SPICC_FIFORST_MASK, 3),
			    spicc->base + SPICC_TESTREG);

	while (sspicc_rxready(spicc))
		data  += readl_relaxed(spicc->base + SPICC_RXDATA);
	data += readl_relaxed(spicc->base + SPICC_RXDATA);
	data += readl_relaxed(spicc->base + SPICC_RXDATA);

	writel_bits_relaxed(SPICC_ENH_MAIN_CLK_AO, 0,
			    spicc->base + SPICC_ENH_CTL0);

	return data;
}

static void sspicc_vmem_data_out(struct slave_spicc *spicc)
{
	u64 data = 0;
	int i;

	for (i = 0; i < SPICC_BYTES_PER_WORD; i++) {
		data <<= 8;
		if (spicc->vmem_count < spicc->vmem_len) {
			data |= *spicc->vmem_buf++;
			spicc->vmem_count++;
		}
	}

	writel_relaxed(data >> 32, spicc->base + SPICC_TXDATA);
	writel_relaxed(data, spicc->base + SPICC_TXDATA);
}

static void sspicc_vmem_data_in(struct slave_spicc *spicc, u64 data)
{
	int i;

	for (i = 0; i < SPICC_BYTES_PER_WORD; i++) {
		if (spicc->vmem_count < spicc->vmem_len) {
			*spicc->vmem_buf++ = data;
			data >>= 8;
			spicc->vmem_count++;
		}
	}
}

static void sspicc_vmem_ready(struct slave_spicc *spicc)
{
	sspicc_reset_fifo(spicc);
	writel_bits_relaxed(SPICC_BITLENGTH_MASK,
			    FIELD_PREP(SPICC_BITLENGTH_MASK, 31),
			    spicc->base + SPICC_CONREG);

	spicc->state = SPICC_STATE_CMD;
}

static int sspicc_vmem_cmd(struct slave_spicc *spicc)
{
	struct vmem_device *vmemdev;
	u32 data;
	int ret = -ENODEV;

	data = readl_relaxed(spicc->base + SPICC_RXDATA);
	vmemdev = vmem_get_device(spicc->vmemctlr, VMEM_DEV_OF(data));
	if (vmemdev) {
		spicc->vmem_cmd_val = VMEM_CMD_OF(data);
		spicc->vmem_offset = VMEM_OFFSET_OF(data);
		spicc->vmem_len = VMEM_LEN_OF(data);
		spicc->vmem_buf = vmemdev->mem + spicc->vmem_offset;
		spicc->vmem_count = 0;
		spicc->vmemdev = vmemdev;
		if (vmemdev->cmd_notify)
			vmemdev->cmd_notify(spicc->vmem_cmd_val,
					    spicc->vmem_offset,
					    spicc->vmem_len);
		spicc->state = SPICC_STATE_DATA;

		sspicc_reset_fifo(spicc);
		writel_bits_relaxed(SPICC_BITLENGTH_MASK,
				    FIELD_PREP(SPICC_BITLENGTH_MASK, 63),
				    spicc->base + SPICC_CONREG);
		ret = 0;
	}

	return ret;
}

static void sspicc_xfer_work(struct work_struct *work)
{
	struct slave_spicc *spicc =
		container_of(work, struct slave_spicc, xfer_work);

	while (1) {
		if (!wait_for_completion_timeout(&spicc->xfer_completion,
						 msecs_to_jiffies(500)))
			break;

		if (spicc->state == SPICC_STATE_IDLE)
			break;
	}

	if (spicc->vmemdev->data_notify)
		spicc->vmemdev->data_notify(spicc->vmem_cmd_val,
					    spicc->vmem_offset,
					    spicc->vmem_count);

	sspicc_vmem_ready(spicc);
}

static irqreturn_t sspicc_irq(int irq, void *context)
{
	struct slave_spicc *spicc = (void *)context;
	unsigned long flags;
	u64 data;

	spin_lock_irqsave(&spicc->lock, flags);

	if (spicc->state == SPICC_STATE_CMD) {
		if (!sspicc_vmem_cmd(spicc)) {
			if (spicc->vmem_cmd_val == VMEM_CMD_READ)
				sspicc_vmem_data_out(spicc);
			queue_work(spicc->xfer_wq, &spicc->xfer_work);
		}
	} else if (spicc->state == SPICC_STATE_DATA) {
		data = readl_relaxed(spicc->base + SPICC_RXDATA);
		data |= (u64)readl_relaxed(spicc->base + SPICC_RXDATA) << 32;
		if (spicc->vmem_cmd_val == VMEM_CMD_READ) {
			if (spicc->vmem_count >= spicc->vmem_len)
				spicc->state = SPICC_STATE_IDLE;
			else
				sspicc_vmem_data_out(spicc);
		}

		else if (spicc->vmem_cmd_val == VMEM_CMD_WRITE) {
			sspicc_vmem_data_in(spicc, data);
			if (spicc->vmem_count >= spicc->vmem_len)
				spicc->state = SPICC_STATE_IDLE;
		}

		complete(&spicc->xfer_completion);
	}

	spin_unlock_irqrestore(&spicc->lock, flags);

	return IRQ_HANDLED;
}

static int sspicc_probe(struct platform_device *pdev)
{
	struct vmem_controller *vmemctlr;
	struct slave_spicc *spicc;
	struct resource *res;
	struct clk *clk;
	int ret, irq;

	vmemctlr = vmem_create_controller(&pdev->dev, sizeof(*spicc));
	if (!vmemctlr) {
		dev_err(&pdev->dev, "vmem controller alloc failed\n");
		return -ENODEV;
	}

	spicc = dev_get_drvdata(&vmemctlr->dev);
	spicc->vmemctlr = vmemctlr;
	platform_set_drvdata(pdev, spicc);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	spicc->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(spicc->base)) {
		ret = PTR_ERR(spicc->base);
		dev_err(&pdev->dev, "io resource mapping failed\n");
		goto out;
	}

	/* Get core clk */
	clk = devm_clk_get(&pdev->dev, "core");
	if (WARN_ON(IS_ERR(clk))) {
		ret = PTR_ERR(clk);
		dev_err(&pdev->dev, "get core clk failed\n");
		goto out;
	}
	clk_prepare_enable(clk);

	/* Get composite clk */
	clk = devm_clk_get(&pdev->dev, "comp");
	if (WARN_ON(IS_ERR(clk))) {
		ret = PTR_ERR(clk);
		dev_err(&pdev->dev, "get comp clk failed\n");
		goto out;
	}
	clk_prepare_enable(clk);
	clk_set_rate(clk, 200000000);

	irq = platform_get_irq(pdev, 0);
	ret = devm_request_irq(&pdev->dev, irq, sspicc_irq, 0,
			       dev_name(&pdev->dev), spicc);
	if (ret) {
		dev_err(&pdev->dev, "irq request failed\n");
		goto out;
	}

	spin_lock_init(&spicc->lock);
	spicc->mode = 0;
	spicc->cs_num = 0;
	sspicc_hw_init(spicc);
	sspicc_vmem_ready(spicc);

	spicc->xfer_wq = create_singlethread_workqueue(dev_name(&pdev->dev));
	if (!spicc->xfer_wq) {
		dev_err(&pdev->dev, "create workqueue failed\n");
		ret = PTR_ERR(spicc->xfer_wq);
		goto out;
	}
	INIT_WORK(&spicc->xfer_work, sspicc_xfer_work);
	init_completion(&spicc->xfer_completion);
	dev_info(&pdev->dev, "sspicc registration success\n");

#ifdef SSPICC_TEST_ENTRY
	spicc->cls.name = dev_name(&pdev->dev);
	spicc->cls.class_attrs = spicc_class_attrs;
	class_register(&spicc->cls);
#endif

	return 0;

out:
	vmem_destroy_controller(vmemctlr);
	dev_err(&pdev->dev, "sspicc registration failed(%d)\n", ret);
	return ret;
}

static int sspicc_remove(struct platform_device *pdev)
{
	struct slave_spicc *spicc = platform_get_drvdata(pdev);

	/* Disable SPI */
	writel(0, spicc->base + SPICC_CONREG);
	destroy_workqueue(spicc->xfer_wq);

	vmem_destroy_controller(spicc->vmemctlr);

	return 0;
}

static const struct of_device_id sspicc_of_match[] = {
	{.compatible = "amlogic,slave-spicc"},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sspicc_of_match);

static struct platform_driver sspicc_driver = {
	.probe   = sspicc_probe,
	.remove  = sspicc_remove,
	.driver  = {
		.name = "slave-spicc",
		.of_match_table = of_match_ptr(sspicc_of_match),
	},
};

module_platform_driver(sspicc_driver);

MODULE_DESCRIPTION("Meson Slave SPICC driver");
MODULE_AUTHOR("Amlogic");
MODULE_LICENSE("GPL");
