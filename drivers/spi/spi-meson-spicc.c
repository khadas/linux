/*
 * Driver for Amlogic Meson SPI communication controller (SPICC)
 *
 * Copyright (C) BayLibre, SAS
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#ifdef CONFIG_AMLOGIC_MODIFY
#include <linux/of_device.h>
#include <linux/clk-provider.h>
#include <linux/dma-mapping.h>
#include <linux/amlogic/power_domain.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#endif
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/reset.h>
#include <linux/gpio.h>

/*
 * The Meson SPICC controller could support DMA based transfers, but is not
 * implemented by the vendor code, and while having the registers documentation
 * it has never worked on the GXL Hardware.
 * The PIO mode is the only mode implemented, and due to badly designed HW :
 * - all transfers are cutted in 16 words burst because the FIFO hangs on
 *   TX underflow, and there is no TX "Half-Empty" interrupt, so we go by
 *   FIFO max size chunk only
 * - CS management is dumb, and goes UP between every burst, so is really a
 *   "Data Valid" signal than a Chip Select, GPIO link should be used instead
 *   to have a CS go down over the full transfer
 * G12 change list:
 * - add a independent async clk for divider to raise the speed rate
 * - incapable of half-full and overflow interrupts because of the async clk
 * - burst max change from 16 to 15 because of the async clk
 * DMA support:
 * - use bits_per_word of xfer to distinguish what mode the protocol driver
     requested, 64 indicates DMA mode while the others indicate PIO.
 */

#define MESON_SPICC_HW_IF
#ifndef CONFIG_AMLOGIC_MODIFY
#define SPICC_MAX_FREQ	30000000
#endif
#define SPICC_MAX_BURST	128

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
#ifdef CONFIG_AMLOGIC_MODIFY
#define SPICC_DATARATE_SHIFT 16
#define SPICC_DATARATE_WIDTH 3
#endif
#define SPICC_DATARATE_DIV4	0
#define SPICC_DATARATE_DIV8	1
#define SPICC_DATARATE_DIV16	2
#define SPICC_DATARATE_DIV32	3
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
#define SPICC_TXFIFO_THRESHOLD_MASK	GENMASK(5, 1)
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
#ifdef CONFIG_AMLOGIC_MODIFY
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
#else
#define SPICC_LBC_RO		BIT(13)	/* Loop Back Control Read-Only */
#define SPICC_LBC_W1		BIT(14) /* Loop Back Control Write-Only */
#define SPICC_SWAP_RO		BIT(14) /* RX FIFO Data Swap Read-Only */
#define SPICC_SWAP_W1		BIT(15) /* RX FIFO Data Swap Write-Only */
#define SPICC_DLYCTL_RO_MASK	GENMASK(20, 15) /* Delay Control Read-Only */
#define SPICC_DLYCTL_W1_MASK	GENMASK(21, 16) /* Delay Control Write-Only */
#define SPICC_FIFORST_RO_MASK	GENMASK(22, 21) /* FIFO Softreset Read-Only */
#define SPICC_FIFORST_W1_MASK	GENMASK(23, 22) /* FIFO Softreset Write-Only */
#endif

#define SPICC_DRADDR	0x20	/* Read Address of DMA */

#define SPICC_DWADDR	0x24	/* Write Address of DMA */

#ifdef CONFIG_AMLOGIC_MODIFY
#define SPICC_ENH_CTL0	0x38	/* Enhanced Feature */
#define SPICC_ENH_CLK_CS_DELAY_MASK	GENMASK(15, 0)
#define SPICC_ENH_DATARATE_MASK		GENMASK(23, 16)
#define SPICC_ENH_DATARATE_SHIFT	16
#define SPICC_ENH_DATARATE_WIDTH	8
#define SPICC_ENH_DATARATE_EN		BIT(24)
#define SPICC_ENH_MOSI_OEN		BIT(25)
#define SPICC_ENH_CLK_OEN		BIT(26)
#define SPICC_ENH_CS_OEN		BIT(27)
#define SPICC_ENH_CLK_CS_DELAY_EN	BIT(28)
#define SPICC_ENH_MAIN_CLK_AO		BIT(29)
#endif

#define writel_bits_relaxed(mask, val, addr) \
	writel_relaxed((readl_relaxed(addr) & ~(mask)) | (val), addr)

#ifdef CONFIG_AMLOGIC_MODIFY
static int SPICC_BURST_MAX = 16;
#else
#define SPICC_BURST_MAX	16
#endif
#define SPICC_FIFO_HALF 10

#ifdef CONFIG_AMLOGIC_MODIFY
struct meson_spicc_data {
	unsigned int			max_speed_hz;
	bool				has_linear_div;
	bool				has_oen;
	bool				has_async_clk;
};
#endif

struct meson_spicc_device {
	struct spi_master		*master;
	struct platform_device		*pdev;
	void __iomem			*base;
	struct clk			*core;
#ifdef CONFIG_AMLOGIC_MODIFY
	struct clk			*async_clk;
	struct clk			*clk;
	const struct meson_spicc_data	*data;
	unsigned int			speed_hz;
	struct				class cls;
	bool				using_dma;
#endif
	struct spi_message		*message;
	struct spi_transfer		*xfer;
	u8				*tx_buf;
	u8				*rx_buf;
	unsigned int			bytes_per_word;
	unsigned long			tx_remain;
	unsigned long			txb_remain;
	unsigned long			rx_remain;
	unsigned long			rxb_remain;
	unsigned long			xfer_remain;
	bool				is_burst_end;
	bool				is_last_burst;
};

#ifdef CONFIG_AMLOGIC_MODIFY
static int meson_spicc_runtime_suspend(struct device *dev);
static int meson_spicc_runtime_resume(struct device *dev);

static void meson_spicc_auto_io_delay(struct meson_spicc_device *spicc,
				      bool loop)
{
	u32 div, hz;
	u32 mi_delay, cap_delay;
	u32 conf;

	if (spicc->data->has_linear_div) {
		div = readl_relaxed(spicc->base + SPICC_ENH_CTL0);
		div &= SPICC_ENH_DATARATE_MASK;
		div >>= SPICC_ENH_DATARATE_SHIFT;
		div++;
		div <<= 1;
	} else {
		div = readl_relaxed(spicc->base + SPICC_CONREG);
		div &= SPICC_DATARATE_MASK;
		div >>= SPICC_DATARATE_SHIFT;
		div += 2;
		div = 1 << div;
	}

	mi_delay = SPICC_MI_NO_DELAY;
	cap_delay = SPICC_CAP_AHEAD_2_CYCLE;
	hz = clk_get_rate(spicc->clk);

	if (loop)
		cap_delay = SPICC_CAP_AHEAD_1_CYCLE;
	else if (hz >= 100000000)
		cap_delay = SPICC_CAP_DELAY_1_CYCLE;
	else if (hz >= 80000000)
		cap_delay = SPICC_CAP_NO_DELAY;
	else if (hz >= 40000000)
		cap_delay = SPICC_CAP_AHEAD_1_CYCLE;
	else if (div >= 16)
		mi_delay = SPICC_MI_DELAY_3_CYCLE;
	else if (div >= 8)
		mi_delay = SPICC_MI_DELAY_2_CYCLE;
	else if (div >= 6)
		mi_delay = SPICC_MI_DELAY_1_CYCLE;

	conf = readl_relaxed(spicc->base + SPICC_TESTREG);
	conf &= ~(SPICC_MO_DELAY_MASK | SPICC_MI_DELAY_MASK
		  | SPICC_MI_CAP_DELAY_MASK);
	conf |= FIELD_PREP(SPICC_MI_DELAY_MASK, mi_delay);
	conf |= FIELD_PREP(SPICC_MI_CAP_DELAY_MASK, cap_delay);
	writel_relaxed(conf, spicc->base + SPICC_TESTREG);
}

static int meson_spicc_dma_map(struct meson_spicc_device *spicc,
			       struct spi_transfer *t)
{
	struct device *dev = spicc->master->dev.parent;

	t->tx_dma = dma_map_single(dev, (void *)t->tx_buf, t->len,
				   DMA_TO_DEVICE);
	if (dma_mapping_error(dev, t->tx_dma)) {
		dev_err(dev, "tx_dma map failed\n");
		return -ENOMEM;
	}

	t->rx_dma = dma_map_single(dev, t->rx_buf, t->len, DMA_FROM_DEVICE);
	if (dma_mapping_error(dev, t->rx_dma)) {
		dma_unmap_single(dev, t->tx_dma, t->len, DMA_TO_DEVICE);
		dev_err(dev, "rx_dma map failed\n");
		return -ENOMEM;
	}

	return 0;
}

static void meson_spicc_dma_unmap(struct meson_spicc_device *spicc,
				  struct spi_transfer *t)
{
	struct device *dev = spicc->master->dev.parent;

	dma_unmap_single(dev, t->tx_dma, t->len, DMA_TO_DEVICE);
	dma_unmap_single(dev, t->rx_dma, t->len, DMA_FROM_DEVICE);
}

static void meson_spicc_setup_dma_burst(struct meson_spicc_device *spicc)
{
	unsigned int dma, burst_len, thres;

	burst_len = min_t(unsigned int,
			  spicc->xfer_remain / spicc->bytes_per_word,
			  SPICC_BURST_MAX << 3);

	thres = burst_len % SPICC_BURST_MAX;
	if (thres == 0) {
		thres = SPICC_BURST_MAX;
	} else if (burst_len != thres) {
		burst_len -= thres;
		thres = SPICC_BURST_MAX;
	}

	/* Setup Xfer variables */
	spicc->xfer_remain -= burst_len * spicc->bytes_per_word;

	/* Setup burst length */
	writel_bits_relaxed(SPICC_BURSTLENGTH_MASK,
			    FIELD_PREP(SPICC_BURSTLENGTH_MASK,
				       burst_len - 1),
			    spicc->base + SPICC_CONREG);

	dma = SPICC_DMA_ENABLE;
	dma |= FIELD_PREP(SPICC_TXFIFO_THRESHOLD_MASK, 4);
	dma |= FIELD_PREP(SPICC_READ_BURST_MASK, SPICC_BURST_MAX - 4);
	dma |= FIELD_PREP(SPICC_RXFIFO_THRESHOLD_MASK, thres - 1);
	dma |= FIELD_PREP(SPICC_WRITE_BURST_MASK, thres - 1);
	writel_relaxed(dma, spicc->base + SPICC_DMAREG);
}

static void meson_spicc_dma_irq(struct meson_spicc_device *spicc)
{
	writel_bits_relaxed(SPICC_TC, SPICC_TC, spicc->base + SPICC_STATREG);

	if (!spicc->xfer_remain) {
		/* Disable all IRQs */
		writel_relaxed(0, spicc->base + SPICC_INTREG);
		writel_relaxed(0, spicc->base + SPICC_DMAREG);
		if (!spicc->message->is_dma_mapped)
			meson_spicc_dma_unmap(spicc, spicc->xfer);
		spi_finalize_current_transfer(spicc->master);
		spicc->using_dma = 0;
		return;
	}

	/* Setup burst */
	meson_spicc_setup_dma_burst(spicc);

	/* Start burst */
	writel_bits_relaxed(SPICC_XCH, SPICC_XCH, spicc->base + SPICC_CONREG);
}
#endif

static inline bool meson_spicc_txfull(struct meson_spicc_device *spicc)
{
	return !!FIELD_GET(SPICC_TF,
			   readl_relaxed(spicc->base + SPICC_STATREG));
}

static inline bool meson_spicc_rxready(struct meson_spicc_device *spicc)
{
	return FIELD_GET(SPICC_RH | SPICC_RR | SPICC_RF_EN,
			 readl_relaxed(spicc->base + SPICC_STATREG));
}

static inline u32 meson_spicc_pull_data(struct meson_spicc_device *spicc)
{
	unsigned int bytes = spicc->bytes_per_word;
	unsigned int byte_shift = 0;
	u32 data = 0;
	u8 byte;

	spicc->tx_remain--;
	if (!spicc->tx_buf)
		return 0;

	while (bytes--) {
		byte = *spicc->tx_buf++;
		data |= (byte & 0xff) << byte_shift;
		byte_shift += 8;
	}

	return data;
}

static inline void meson_spicc_push_data(struct meson_spicc_device *spicc,
					 u32 data)
{
	unsigned int bytes = spicc->bytes_per_word;
	unsigned int byte_shift = 0;
	u8 byte;

	spicc->rx_remain--;
	if (!spicc->rx_buf)
		return;

	while (bytes--) {
		byte = (data >> byte_shift) & 0xff;
		*spicc->rx_buf++ = byte;
		byte_shift += 8;
	}
}

static inline void meson_spicc_rx(struct meson_spicc_device *spicc)
{
	/* Empty RX FIFO */
	while (spicc->rx_remain &&
	       meson_spicc_rxready(spicc))
		meson_spicc_push_data(spicc,
				readl_relaxed(spicc->base + SPICC_RXDATA));
}

static inline void meson_spicc_tx(struct meson_spicc_device *spicc)
{
	/* Fill Up TX FIFO */
	while (spicc->tx_remain &&
	       !meson_spicc_txfull(spicc))
		writel_relaxed(meson_spicc_pull_data(spicc),
			       spicc->base + SPICC_TXDATA);
}

static inline u32 meson_spicc_setup_rx_irq(struct meson_spicc_device *spicc,
					   u32 irq_ctrl)
{
#ifdef CONFIG_AMLOGIC_MODIFY
	if (spicc->using_dma) {
		irq_ctrl |= SPICC_TC_EN;
		return irq_ctrl;
	}
	/* SoCs has async-clk is incapable of half-full interrupt */
	if (spicc->rx_remain > SPICC_FIFO_HALF && !spicc->data->has_async_clk)
#else
	if (spicc->rx_remain > SPICC_FIFO_HALF)
#endif
		irq_ctrl |= SPICC_RH_EN;
	else
		irq_ctrl |= SPICC_RR_EN;

	return irq_ctrl;
}

static inline void meson_spicc_setup_burst(struct meson_spicc_device *spicc,
					   unsigned int burst_len)
{
	/* Setup Xfer variables */
	spicc->tx_remain = burst_len;
	spicc->rx_remain = burst_len;
	spicc->xfer_remain -= burst_len * spicc->bytes_per_word;
	spicc->is_burst_end = false;
	if (burst_len < SPICC_BURST_MAX || !spicc->xfer_remain)
		spicc->is_last_burst = true;
	else
		spicc->is_last_burst = false;

	/* Setup burst length */
	writel_bits_relaxed(SPICC_BURSTLENGTH_MASK,
			FIELD_PREP(SPICC_BURSTLENGTH_MASK, burst_len - 1),
			spicc->base + SPICC_CONREG);

	/* Fill TX FIFO */
	meson_spicc_tx(spicc);
}

static void meson_spicc_hw_prepare(struct meson_spicc_device *spicc,
				   u16 mode,
				   u8 bits_per_word,
				   u32 speed_hz)
{
	u32 conf = 0;

	/* Disable all IRQs */
	writel_relaxed(0, spicc->base + SPICC_INTREG);
	/* Setup no wait cycles by default */
	writel_relaxed(0, spicc->base + SPICC_PERIODREG);

	conf = readl_relaxed(spicc->base + SPICC_TESTREG);
	conf &= ~SPICC_LBC;
	if (mode & SPI_LOOP)
		conf |= SPICC_LBC;
	writel_relaxed(conf, spicc->base + SPICC_TESTREG);

	conf = readl_relaxed(spicc->base + SPICC_CONREG);
	/* clear all except following */
	conf &= SPICC_ENABLE | SPICC_MODE_MASTER | SPICC_DATARATE_MASK;

	/* Setup transfer mode */
	if (mode & SPI_CPOL)
		conf |= SPICC_POL;

	if (mode & SPI_CPHA)
		conf |= SPICC_PHA;

	if (mode & SPI_CS_HIGH)
		conf |= SPICC_SSPOL;

	if (mode & SPI_READY)
		conf |= FIELD_PREP(SPICC_DRCTL_MASK, SPICC_DRCTL_LOWLEVEL);

	/* Default 8bit word */
	if (!bits_per_word)
		bits_per_word = 8;
	conf |= FIELD_PREP(SPICC_BITLENGTH_MASK, bits_per_word - 1);

	writel_relaxed(conf, spicc->base + SPICC_CONREG);

	/* Setup clock speed */
	if (speed_hz && spicc->speed_hz != speed_hz) {
		spicc->speed_hz = speed_hz;
		if (clk_set_rate(spicc->clk, speed_hz)) {
			dev_err(&spicc->pdev->dev, "set clk rate failed\n");
			return;
		}
		meson_spicc_auto_io_delay(spicc, !!(mode & SPI_LOOP));
	}

	/* Reset tx/rx-fifo maybe there are some residues left by DMA */
	writel_bits_relaxed(SPICC_FIFORST_MASK, SPICC_FIFORST_MASK,
			    spicc->base + SPICC_TESTREG);
	/* Clean out rx-fifo */
	while (meson_spicc_rxready(spicc))
		conf = readl_relaxed(spicc->base + SPICC_RXDATA);
}

static irqreturn_t meson_spicc_irq(int irq, void *data)
{
	struct meson_spicc_device *spicc = (void *) data;
	u32 ctrl = readl_relaxed(spicc->base + SPICC_INTREG);
	u32 stat = readl_relaxed(spicc->base + SPICC_STATREG) & ctrl;

	ctrl &= ~(SPICC_RH_EN | SPICC_RR_EN);

#ifdef CONFIG_AMLOGIC_MODIFY
	if (spicc->using_dma) {
		meson_spicc_dma_irq(spicc);
		return IRQ_HANDLED;
	}
#endif

	/* Empty RX FIFO */
	meson_spicc_rx(spicc);

	/* Enable TC interrupt since we transferred everything */
	if (!spicc->tx_remain && !spicc->rx_remain) {
		spicc->is_burst_end = true;

		/* Enable TC interrupt */
		ctrl |= SPICC_TC_EN;

		/* Reload IRQ status */
		stat = readl_relaxed(spicc->base + SPICC_STATREG) & ctrl;
	}

	/* Check transfer complete */
	if ((stat & SPICC_TC) && spicc->is_burst_end) {
		unsigned int burst_len;

		/* Clear TC bit */
		writel_relaxed(SPICC_TC, spicc->base + SPICC_STATREG);

		/* Disable TC interrupt */
		ctrl &= ~SPICC_TC_EN;

		if (spicc->is_last_burst) {
			/* Disable all IRQs */
			writel(0, spicc->base + SPICC_INTREG);

			spi_finalize_current_transfer(spicc->master);

			return IRQ_HANDLED;
		}

		burst_len = min_t(unsigned int,
				  spicc->xfer_remain / spicc->bytes_per_word,
				  SPICC_BURST_MAX);

		/* Setup burst */
		meson_spicc_setup_burst(spicc, burst_len);

		/* Restart burst */
		writel_bits_relaxed(SPICC_XCH, SPICC_XCH,
				    spicc->base + SPICC_CONREG);
	}

	/* Setup RX interrupt trigger */
	ctrl = meson_spicc_setup_rx_irq(spicc, ctrl);

	/* Reconfigure interrupts */
	writel(ctrl, spicc->base + SPICC_INTREG);

	return IRQ_HANDLED;
}

static int meson_spicc_transfer_one(struct spi_master *master,
				    struct spi_device *spi,
				    struct spi_transfer *xfer)
{
	struct meson_spicc_device *spicc = spi_master_get_devdata(master);
	unsigned int burst_len;
	u32 irq = 0;

	/* Store current transfer */
	spicc->xfer = xfer;

	/* Setup transfer parameters */
	spicc->tx_buf = (u8 *)xfer->tx_buf;
	spicc->rx_buf = (u8 *)xfer->rx_buf;
	spicc->xfer_remain = xfer->len;

	/* Pre-calculate word size */
	spicc->bytes_per_word =
	   DIV_ROUND_UP(spicc->xfer->bits_per_word, 8);

	/* Setup transfer parameters */
	meson_spicc_hw_prepare(spicc, spi->mode, xfer->bits_per_word,
			       xfer->speed_hz);

#ifdef CONFIG_AMLOGIC_MODIFY
	spicc->using_dma = 0;
	writel_relaxed(0, spicc->base + SPICC_DMAREG);
	if (xfer->bits_per_word == 64 && (spicc->message->is_dma_mapped ||
					  !meson_spicc_dma_map(spicc, xfer))) {
		spicc->using_dma = 1;
		writel_relaxed(xfer->tx_dma, spicc->base + SPICC_DRADDR);
		writel_relaxed(xfer->rx_dma, spicc->base + SPICC_DWADDR);
		writel_relaxed(xfer->speed_hz >> 25,
			       spicc->base + SPICC_PERIODREG);
		meson_spicc_setup_dma_burst(spicc);
	} else {
		burst_len = min_t(unsigned int,
				  spicc->xfer_remain / spicc->bytes_per_word,
				  SPICC_BURST_MAX);
		meson_spicc_setup_burst(spicc, burst_len);
	}
#else
	burst_len = min_t(unsigned int,
			  spicc->xfer_remain / spicc->bytes_per_word,
			  SPICC_BURST_MAX);

	meson_spicc_setup_burst(spicc, burst_len);
#endif

	irq = meson_spicc_setup_rx_irq(spicc, irq);

	/* Start burst */
	writel_bits_relaxed(SPICC_XCH, SPICC_XCH, spicc->base + SPICC_CONREG);

	/* Enable interrupts */
	writel_relaxed(irq, spicc->base + SPICC_INTREG);

	return 1;
}

static int meson_spicc_prepare_message(struct spi_master *master,
				       struct spi_message *message)
{
	struct meson_spicc_device *spicc = spi_master_get_devdata(master);
	struct spi_device *spi = message->spi;

	/* Store current message */
	spicc->message = message;
	meson_spicc_hw_prepare(spicc, spi->mode, spi->bits_per_word,
			       spi->max_speed_hz);
	return 0;
}

static int meson_spicc_unprepare_transfer(struct spi_master *master)
{
	struct meson_spicc_device *spicc = spi_master_get_devdata(master);

	/* Disable all IRQs */
	writel(0, spicc->base + SPICC_INTREG);

#ifndef CONFIG_AMLOGIC_MODIFY
	/* Disable controller */
	writel_bits_relaxed(SPICC_ENABLE, 0, spicc->base + SPICC_CONREG);
#endif

	device_reset_optional(&spicc->pdev->dev);

	return 0;
}

static int meson_spicc_setup(struct spi_device *spi)
{
	int ret = 0;

	if (!spi->controller_state)
		spi->controller_state = spi_master_get_devdata(spi->master);
	else if (gpio_is_valid(spi->cs_gpio))
		goto out_gpio;
	else if (spi->cs_gpio == -ENOENT)
		return 0;

	if (gpio_is_valid(spi->cs_gpio)) {
		ret = gpio_request(spi->cs_gpio, dev_name(&spi->dev));
		if (ret) {
			dev_err(&spi->dev, "failed to request cs gpio\n");
			return ret;
		}
#ifndef CONFIG_AMLOGIC_MODIFY
	}
#else
	} else {
		dev_err(&spi->dev, "cs gpio invalid\n");
		return 0;
	}
#endif

out_gpio:
	ret = gpio_direction_output(spi->cs_gpio,
			!(spi->mode & SPI_CS_HIGH));

	return ret;
}

static void meson_spicc_cleanup(struct spi_device *spi)
{
	if (gpio_is_valid(spi->cs_gpio))
		gpio_free(spi->cs_gpio);

	spi->controller_state = NULL;
}

#ifdef CONFIG_AMLOGIC_MODIFY
static int meson_spicc_clk_enable(struct meson_spicc_device *spicc)
{
	int ret;

	ret = clk_prepare_enable(spicc->core);
	if (ret)
		return ret;

	ret = clk_prepare_enable(spicc->clk);
	if (ret)
		return ret;

	if (spicc->data->has_async_clk) {
		ret = clk_prepare_enable(spicc->async_clk);
		if (ret)
			return ret;
	}

	return 0;
}

static void meson_spicc_clk_disable(struct meson_spicc_device *spicc)
{
	if (!IS_ERR_OR_NULL(spicc->core))
		clk_disable_unprepare(spicc->core);

	if (!IS_ERR_OR_NULL(spicc->clk))
		clk_disable_unprepare(spicc->clk);

	if (spicc->data->has_async_clk && !IS_ERR_OR_NULL(spicc->async_clk))
		clk_disable_unprepare(spicc->async_clk);
}

static int meson_spicc_hw_init(struct meson_spicc_device *spicc)
{
	/* For GX/AXG, all registers except CONREG are unavailable if
	 * SPICC_ENABLE is 0, which will be also reset to default value
	 * if it changed from 1 to 0.
	 * Set master mode and enable controller ahead of others here,
	 * and never disable them.
	 */
	writel_relaxed(SPICC_ENABLE | SPICC_MODE_MASTER,
		       spicc->base + SPICC_CONREG);

	if (spicc->data->has_oen)
		writel_relaxed(readl_relaxed(spicc->base + SPICC_ENH_CTL0) |
			       SPICC_ENH_MOSI_OEN | SPICC_ENH_CLK_OEN |
			       SPICC_ENH_CS_OEN, spicc->base + SPICC_ENH_CTL0);

	/* Disable all IRQs */
	writel_relaxed(0, spicc->base + SPICC_INTREG);

	return 0;
}

#ifdef MESON_SPICC_HW_IF
static int dirspi_wait_complete(struct meson_spicc_device *spicc, int times)
{
	int ret = -ETIMEDOUT;
	u32 sta;

	while (times--) {
		sta = readl_relaxed(spicc->base + SPICC_STATREG);
		sta &= SPICC_TC | SPICC_TE;
		if (sta == (u32)(SPICC_TC | SPICC_TE)) {
			/* set 1 to clear */
			writel_bits_relaxed(SPICC_TC, SPICC_TC,
					    spicc->base + SPICC_STATREG);
			ret = 0;
			break;
		}
	}

	return ret;
}

int dirspi_xfer(struct spi_device *spi, u8 *tx_buf, u8 *rx_buf, int len)
{
	struct meson_spicc_device *spicc = spi_master_get_devdata(spi->master);
	unsigned int burst_len, rx_remain;
	int ret = 0;

	spicc->tx_buf = tx_buf;
	spicc->rx_buf = rx_buf;
	spicc->rx_remain = 0;
	spicc->xfer_remain = len;

	/* Pre-calculate word size */
	spicc->bytes_per_word = DIV_ROUND_UP(spi->bits_per_word, 8);

	writel_relaxed(0, spicc->base + SPICC_INTREG);
	writel_relaxed(0, spicc->base + SPICC_DMAREG);

	while (spicc->xfer_remain) {
		rx_remain = spicc->rx_remain;
		burst_len = min_t(unsigned int,
				  spicc->xfer_remain / spicc->bytes_per_word,
				  SPICC_BURST_MAX - rx_remain);
		meson_spicc_setup_burst(spicc, burst_len);
		spicc->rx_remain += rx_remain;

		/* Start burst */
		writel_bits_relaxed(SPICC_XCH, SPICC_XCH,
				    spicc->base + SPICC_CONREG);

		ret = dirspi_wait_complete(spicc, burst_len << 13);
		if (ret) {
			dev_err(&spi->dev, "wait TC/TE timeout\n");
			break;
		}

		meson_spicc_rx(spicc);
	}

	return ret;
}
EXPORT_SYMBOL(dirspi_xfer);

int dirspi_write(struct spi_device *spi, u8 *buf, int len)
{
	return dirspi_xfer(spi, buf, 0, len);
}
EXPORT_SYMBOL(dirspi_write);

int dirspi_read(struct spi_device *spi, u8 *buf, int len)
{
	return dirspi_xfer(spi, 0, buf, len);
}
EXPORT_SYMBOL(dirspi_read);

static void dirspi_set_cs(struct spi_device *spi, bool enable)
{
	if (spi->mode & SPI_NO_CS)
		return;

	if (spi->mode & SPI_CS_HIGH)
		enable = !enable;

	if (spi->cs_gpiod)
		gpiod_set_value_cansleep(spi->cs_gpiod, !enable);
	else if (gpio_is_valid(spi->cs_gpio))
		gpio_set_value_cansleep(spi->cs_gpio, !enable);
}

void dirspi_start(struct spi_device *spi)
{
	struct meson_spicc_device *spicc = spi_master_get_devdata(spi->master);

	meson_spicc_hw_prepare(spicc, spi->mode, spi->bits_per_word,
			       spi->max_speed_hz);

	dirspi_set_cs(spi, true);
}
EXPORT_SYMBOL(dirspi_start);

void dirspi_stop(struct spi_device *spi)
{
	dirspi_set_cs(spi, false);
}
EXPORT_SYMBOL(dirspi_stop);
#endif

#define TEST_PARAM_NUM 5
static ssize_t test_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct meson_spicc_device *spicc = dev_get_drvdata(dev);
	unsigned int cs_gpio, speed, mode, bits_per_word, num;
	u8 *tx_buf, *rx_buf;
	unsigned long value;
	char *kstr, *str_temp, *token;
	int i, ret;
	struct spi_transfer t;
	struct spi_message m;

	if (sscanf(buf, "%d%d%x%d%d", &cs_gpio, &speed,
		   &mode, &bits_per_word, &num) != TEST_PARAM_NUM) {
		dev_err(dev, "error format\n");
		return count;
	}

	kstr = kstrdup(buf, GFP_KERNEL);
	tx_buf = kzalloc(num, GFP_KERNEL | GFP_DMA);
	rx_buf = kzalloc(num, GFP_KERNEL | GFP_DMA);
	if (IS_ERR(kstr) || IS_ERR(tx_buf) || IS_ERR(rx_buf)) {
		dev_err(dev, "failed to alloc tx rx buffer\n");
		return count;
	}

	str_temp = kstr;
	/* pass over "cs_gpio speed mode bits_per_word num" */
	for (i = 0; i < TEST_PARAM_NUM; i++)
		strsep(&str_temp, ", ");

	/* filled tx buffer with user data */
	for (i = 0; i < num; i++) {
		token = strsep(&str_temp, ", ");
		if (token == 0 || kstrtoul(token, 16, &value))
			break;
		tx_buf[i] = (u8)(value & 0xff);
	}

	/* set first tx data default 1 if no any user data */
	if (i == 0) {
		tx_buf[0] = 0x1;
		i++;
	}

	/* filled next buffer incrementally if user data not enough */
	for (; i < num; i++)
		tx_buf[i] = tx_buf[i - 1] + 1;

	spi_message_init(&m);
	m.spi = spi_alloc_device(spicc->master);
	if (IS_ERR_OR_NULL(m.spi)) {
		dev_err(dev, "spi alloc failed\n");
		goto test_end;
	}

	m.spi->cs_gpio = (cs_gpio > 0) ? cs_gpio : -ENOENT;
	m.spi->max_speed_hz = speed;
	m.spi->mode = mode & 0xffff;
	m.spi->bits_per_word = bits_per_word;
	if (spi_setup(m.spi)) {
		dev_err(dev, "setup failed\n");
		goto test_end;
	}

	memset(&t, 0, sizeof(t));
	t.tx_buf = (void *)tx_buf;
	t.rx_buf = (void *)rx_buf;
	t.len = num;
	spi_message_add_tail(&t, &m);
	ret = spi_sync(m.spi, &m);

	if (!ret && (mode & (SPI_LOOP | (1 << 16)))) {
		ret = 0;
		for (i = 0; i < num; i++) {
			if (tx_buf[i] != rx_buf[i]) {
				ret++;
				pr_info("[%d]: 0x%x, 0x%x\n",
					i, tx_buf[i], rx_buf[i]);
			}
		}
		dev_info(dev, "total %d, failed %d\n", num, ret);
	}
	dev_info(dev, "test end @%d\n", (u32)clk_get_rate(spicc->clk));

test_end:
	spi_dev_put(m.spi);
	kfree(kstr);
	kfree(tx_buf);
	kfree(rx_buf);
	return count;
}

static DEVICE_ATTR_WO(test);

/*
 * The Clock Mux
 *            x-----------------x   x---------------x    x------\
 *        |---| 0) fixed factor |---| 1) power2 div |----|      |
 *        |   x-----------------x   x---------------x    |      |
 * src ---|                                              |5) mux|-- out
 *        |   x-----------------x   x---------------x    |      |
 *        |---| 2) fixed factor |---| 3) linear div |----|      |
 *            x-----------------x   x---------------x    x------/
 *
 * Clk path for GX series:
 *    src = core clk
 *    src -> 0 -> 1 -> out
 *
 * Clk path for AXG series:
 *    src = core clk
 *    src -> 0 -> 1 -> 5 -> out
 *    src -> 2 -> 3 -> 5 -> out
 *
 * Clk path for G12 series:
 *    src = async clk
 *    src -> 0 -> 1 -> 5 -> out
 *    src -> 2 -> 3 -> 5 -> out
 */

static struct clk *
meson_spicc_divider_clk_get(struct meson_spicc_device *spicc, bool is_linear)
{
	struct device *dev = &spicc->pdev->dev;
	struct clk_init_data init;
	struct clk_fixed_factor *ff;
	struct clk_divider *div;
	struct clk *clk;
	const char *parent_names[1];
	char name[32], *which;

	ff = devm_kzalloc(dev, sizeof(*ff) + sizeof(*div), GFP_KERNEL);
	if (!ff)
		return ERR_PTR(-ENOMEM);
	div = (struct clk_divider *)&ff[1];

	if (is_linear) {
		which = "linear";
		ff->mult = 1;
		ff->div = 2;
		div->reg = spicc->base + SPICC_ENH_CTL0;
		div->shift = SPICC_ENH_DATARATE_SHIFT;
		div->width = SPICC_ENH_DATARATE_WIDTH;
		div->flags = CLK_DIVIDER_ROUND_CLOSEST;
//		if (spicc->data->has_async_clk)
//			div->flags |= CLK_DIVIDER_PROHIBIT_ZERO;
	} else {
		which = "power2";
		ff->mult = 1;
		ff->div = 4;
		div->reg = spicc->base + SPICC_CONREG;
		div->shift = SPICC_DATARATE_SHIFT;
		div->width = SPICC_DATARATE_WIDTH;
		div->flags = CLK_DIVIDER_ROUND_CLOSEST
			     | CLK_DIVIDER_POWER_OF_TWO;
	}

	/* Register clk-fixed-factor */
	clk = spicc->data->has_async_clk ? spicc->async_clk : spicc->core;
	parent_names[0] = __clk_get_name(clk);
	snprintf(name, sizeof(name), "%s_%s_ff", dev_name(dev), which);
	init.name = name;
	init.ops = &clk_fixed_factor_ops;
	init.flags = CLK_SET_RATE_PARENT;
	init.parent_names = parent_names;
	init.num_parents = 1;
	ff->hw.init = &init;
	clk = devm_clk_register(dev, &ff->hw);
	if (IS_ERR_OR_NULL(clk))
		return clk;

	/* Register clk-divider, which parent the clk-fixed-factor */
	parent_names[0] = __clk_get_name(clk);
	snprintf(name, sizeof(name), "%s_%s_div", dev_name(dev), which);
	init.name = name;
	init.ops = &clk_divider_ops;
	init.flags = CLK_SET_RATE_PARENT;
	init.parent_names = parent_names;
	init.num_parents = 1;
	div->hw.init = &init;
	clk = devm_clk_register(dev, &div->hw);

	return clk;
}

static struct clk *
meson_spicc_mux_clk_get(struct meson_spicc_device *spicc,
			const char * const *parent_names)
{
	struct device *dev = &spicc->pdev->dev;
	struct clk_init_data init;
	struct clk_mux *mux;
	struct clk *clk;
	char name[32];

	mux = devm_kzalloc(dev, sizeof(*mux), GFP_KERNEL);
	if (!mux)
		return ERR_PTR(-ENOMEM);

	snprintf(name, sizeof(name), "%s_mux", dev_name(dev));
	init.name = name;
	init.ops = &clk_mux_ops;
	init.parent_names = parent_names;
	init.num_parents = 2;
	init.flags = CLK_SET_RATE_PARENT;

	mux->mask = 0x1;
	mux->shift = 24;
	mux->reg = spicc->base + SPICC_ENH_CTL0;
	mux->hw.init = &init;
	clk = devm_clk_register(dev, &mux->hw);

	return clk;
}

static struct clk *meson_spicc_clk_get(struct meson_spicc_device *spicc)
{
	struct clk *clk;
	const char *parent_names[2];

	/* Get power-of-two divider clk */
	clk = meson_spicc_divider_clk_get(spicc, 0);
	if (!IS_ERR_OR_NULL(clk) && spicc->data->has_linear_div) {
		/* Set power-of-two as the 1st parent */
		parent_names[0] = __clk_get_name(clk);

		/* Get linear divider clk */
		clk = meson_spicc_divider_clk_get(spicc, 1);
		if (!IS_ERR_OR_NULL(clk)) {
			/* Set linear as the 2nd parent */
			parent_names[1] = __clk_get_name(clk);

			/* Get mux clk */
			clk = meson_spicc_mux_clk_get(spicc, parent_names);
		}
	}

	return clk;
}
#endif

static int meson_spicc_probe(struct platform_device *pdev)
{
	struct spi_master *master;
	struct meson_spicc_device *spicc;
	int ret, irq, rate;

	master = spi_alloc_master(&pdev->dev, sizeof(*spicc));
	if (!master) {
		dev_err(&pdev->dev, "master allocation failed\n");
		return -ENOMEM;
	}
	spicc = spi_master_get_devdata(master);
	spicc->master = master;

	spicc->pdev = pdev;
	platform_set_drvdata(pdev, spicc);

	spicc->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR_OR_NULL(spicc->base)) {
		dev_err(&pdev->dev, "io resource mapping failed\n");
		ret = PTR_ERR(spicc->base);
		goto out_master;
	}

#ifdef CONFIG_AMLOGIC_MODIFY
	spicc->data = (const struct meson_spicc_data *)
		of_device_get_match_data(&pdev->dev);

	meson_spicc_hw_init(spicc);
#else
	/* Disable all IRQs */
	writel_relaxed(0, spicc->base + SPICC_INTREG);
#endif

	irq = platform_get_irq(pdev, 0);
	ret = devm_request_irq(&pdev->dev, irq, meson_spicc_irq,
			       0, NULL, spicc);
	if (ret) {
		dev_err(&pdev->dev, "irq request failed\n");
		goto out_master;
	}

	spicc->core = devm_clk_get(&pdev->dev, "core");
	if (IS_ERR_OR_NULL(spicc->core)) {
		dev_err(&pdev->dev, "core clock request failed\n");
		ret = PTR_ERR(spicc->core);
		goto out_master;
	}

	ret = clk_prepare_enable(spicc->core);
	if (ret) {
		dev_err(&pdev->dev, "core clock enable failed\n");
		goto out_master;
	}
	rate = clk_get_rate(spicc->core);

#ifdef CONFIG_AMLOGIC_MODIFY
	if (spicc->data->has_async_clk) {
		/* SoCs has async-clk is incapable of using full burst */
		SPICC_BURST_MAX = 15;

		spicc->async_clk = devm_clk_get(&pdev->dev, "async");
		if (IS_ERR_OR_NULL(spicc->async_clk)) {
			dev_err(&pdev->dev, "async clock request failed\n");
			ret = PTR_ERR(spicc->async_clk);
			goto out_clk_1;
		}

		ret = clk_prepare_enable(spicc->async_clk);
		if (ret) {
			dev_err(&pdev->dev, "async clock enable failed\n");
			goto out_clk_1;
		}
	}

	spicc->clk = meson_spicc_clk_get(spicc);
	if (IS_ERR_OR_NULL(spicc->clk)) {
		dev_err(&pdev->dev, "divider clock get failed\n");
		ret = PTR_ERR(spicc->clk);
		goto out_clk_2;
	}

	ret = clk_prepare_enable(spicc->clk);
	if (ret) {
		dev_err(&pdev->dev, "divider clock enable failed\n");
		goto out_clk_2;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_test);
	if (ret) {
		dev_err(&pdev->dev, "Create test attribute failed\n");
		goto out_clk_3;
	}
#endif

	device_reset_optional(&pdev->dev);

	master->num_chipselect = 4;
	master->dev.of_node = pdev->dev.of_node;
#ifdef CONFIG_AMLOGIC_MODIFY
	master->mode_bits = SPI_CPHA | SPI_CPOL | SPI_CS_HIGH | SPI_LOOP;
#else
	master->mode_bits = SPI_CPHA | SPI_CPOL | SPI_CS_HIGH;
	master->bits_per_word_mask = SPI_BPW_MASK(32) |
				     SPI_BPW_MASK(24) |
				     SPI_BPW_MASK(16) |
				     SPI_BPW_MASK(8);
#endif
	master->flags = (SPI_MASTER_MUST_RX | SPI_MASTER_MUST_TX);
	master->min_speed_hz = rate >> 9;
	master->setup = meson_spicc_setup;
	master->cleanup = meson_spicc_cleanup;
	master->prepare_message = meson_spicc_prepare_message;
	master->unprepare_transfer_hardware = meson_spicc_unprepare_transfer;
	master->transfer_one = meson_spicc_transfer_one;
#ifdef CONFIG_AMLOGIC_MODIFY
	master->auto_runtime_pm = true;
	pm_runtime_enable(&pdev->dev);
#endif

	/* Setup max rate according to the Meson GX datasheet */
#ifdef CONFIG_AMLOGIC_MODIFY
	master->max_speed_hz = spicc->data->max_speed_hz;
#else
	if ((rate >> 2) > SPICC_MAX_FREQ)
		master->max_speed_hz = SPICC_MAX_FREQ;
	else
		master->max_speed_hz = rate >> 2;
#endif

	ret = devm_spi_register_master(&pdev->dev, master);
	if (ret) {
		dev_err(&pdev->dev, "spi master registration failed\n");
		goto out_clk;
	}

	return 0;

out_clk:
#ifdef CONFIG_AMLOGIC_MODIFY
	device_remove_file(&pdev->dev, &dev_attr_test);
out_clk_3:
	clk_disable_unprepare(spicc->clk);
out_clk_2:
	if (spicc->data->has_async_clk)
		clk_disable_unprepare(spicc->async_clk);
out_clk_1:
#endif
	clk_disable_unprepare(spicc->core);

out_master:
	spi_master_put(master);

	return ret;
}

static int meson_spicc_remove(struct platform_device *pdev)
{
	struct meson_spicc_device *spicc = platform_get_drvdata(pdev);

	/* Disable SPI */
	writel(0, spicc->base + SPICC_CONREG);

	clk_disable_unprepare(spicc->core);
#ifdef CONFIG_AMLOGIC_MODIFY
	if (spicc->data->has_async_clk)
		clk_disable_unprepare(spicc->async_clk);
	clk_disable_unprepare(spicc->clk);
	device_remove_file(&pdev->dev, &dev_attr_test);
#endif

	return 0;
}

#ifdef CONFIG_AMLOGIC_MODIFY
#ifdef CONFIG_PM_SLEEP
static int meson_spicc_suspend(struct device *dev)
{
	struct meson_spicc_device *spicc = dev_get_drvdata(dev);

	return spi_master_suspend(spicc->master);
}

static int meson_spicc_resume(struct device *dev)
{
	struct meson_spicc_device *spicc = dev_get_drvdata(dev);

	return spi_master_resume(spicc->master);
}
#endif /* CONFIG_PM_SLEEP */

static int meson_spicc_runtime_suspend(struct device *dev)
{
	struct meson_spicc_device *spicc = dev_get_drvdata(dev);

	meson_spicc_clk_disable(spicc);

	return 0;
}

static int meson_spicc_runtime_resume(struct device *dev)
{
	struct meson_spicc_device *spicc = dev_get_drvdata(dev);

	meson_spicc_hw_init(spicc);

	return meson_spicc_clk_enable(spicc);
}

static const struct dev_pm_ops meson_spicc_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(meson_spicc_suspend, meson_spicc_resume)
	SET_RUNTIME_PM_OPS(meson_spicc_runtime_suspend,
			   meson_spicc_runtime_resume,
			   NULL)
};

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static const struct meson_spicc_data meson_spicc_gx_data = {
	.max_speed_hz = 30000000,
};

static const struct meson_spicc_data meson_spicc_axg_data = {
	.max_speed_hz = 80000000,
	.has_linear_div = true,
	.has_oen = true,
};
#endif

static const struct meson_spicc_data meson_spicc_g12_data = {
	.max_speed_hz = 124000000,
	.has_linear_div = true,
	.has_oen = true,
	.has_async_clk = true,
};
#endif

static const struct of_device_id meson_spicc_of_match[] = {
#ifdef CONFIG_AMLOGIC_MODIFY
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible = "amlogic,meson-gx-spicc",
		.data = &meson_spicc_gx_data,
	},
	{
		.compatible = "amlogic,meson-axg-spicc",
		.data = &meson_spicc_axg_data,
	},
#endif
	{
		.compatible = "amlogic,meson-g12-spicc",
		.data = &meson_spicc_g12_data,
	},
#else
	.compatible = "amlogic,meson-gx-spicc",
	.compatible = "amlogic,meson-axg-spicc"
#endif
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, meson_spicc_of_match);

static struct platform_driver meson_spicc_driver = {
	.probe   = meson_spicc_probe,
	.remove  = meson_spicc_remove,
	.driver  = {
		.name = "meson-spicc",
		.of_match_table = of_match_ptr(meson_spicc_of_match),
#ifdef CONFIG_AMLOGIC_MODIFY
		.pm = &meson_spicc_pm_ops,
#endif
	},
};

module_platform_driver(meson_spicc_driver);

MODULE_DESCRIPTION("Meson SPI Communication Controller driver");
MODULE_AUTHOR("Neil Armstrong <narmstrong@baylibre.com>");
MODULE_LICENSE("GPL");
