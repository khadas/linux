// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/clk-provider.h>
#include <linux/dma-mapping.h>
#include <linux/amlogic/power_domain.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/reset.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <asm/cacheflush.h>
#include <linux/amlogic/aml_spi.h>
#ifdef CONFIG_SPICC_TEST
#include "spicc_test.h"
#endif
#define MESON_SPICC_HW_IF

/* Register Map */
#define SPICC_REG_CFG_READY		0x00
#define SPICC_REG_CFG_SPI		0x04
#define SPICC_REG_CFG_START		0x08
#define SPICC_REG_CFG_BUS		0x0C
#define SPICC_REG_PIO_TX_DATA_L		0x10
#define SPICC_REG_PIO_TX_DATA_H		0x14
#define SPICC_REG_PIO_RX_DATA_L		0x18
#define SPICC_REG_PIO_RX_DATA_H		0x1C
#define SPICC_REG_MEM_TX_ADDR_L		0x10
#define SPICC_REG_MEM_TX_ADDR_H		0x14
#define SPICC_REG_MEM_RX_ADDR_L		0x18
#define SPICC_REG_MEM_RX_ADDR_H		0x1C
#define SPICC_REG_DESC_LIST_L		0x20
#define SPICC_REG_DESC_LIST_H		0x24
#define SPICC_DESC_PENDING		BIT(31)
#define SPICC_REG_DESC_CURRENT_L	0x28
#define SPICC_REG_DESC_CURRENT_H	0x2c
#define SPICC_REG_IRQ_STS		0x30
#define SPICC_REG_IRQ_ENABLE		0x34
#define SPICC_RCH_DESC_EOC		BIT(0)
#define SPICC_RCH_DESC_INVALID		BIT(1)
#define SPICC_RCH_DESC_RESP		BIT(2)
#define SPICC_RCH_DATA_RESP		BIT(3)
#define SPICC_WCH_DESC_EOC		BIT(4)
#define SPICC_WCH_DESC_INVALID		BIT(5)
#define SPICC_WCH_DESC_RESP		BIT(6)
#define SPICC_WCH_DATA_RESP		BIT(7)
#define SPICC_DESC_ERR			BIT(8)
#define SPICC_SPI_READY			BIT(9)
#define SPICC_DESC_DONE			BIT(10)
#define SPICC_DESC_CHAIN_DONE		BIT(11)

union spicc_cfg_spi {
	u32			d32;
	struct  {
		u32		bus64_en:1;
		u32		slave_en:1;
		u32		ss:2;
		u32		flash_wp_pin_en:1;
		u32		flash_hold_pin_en:1;
		u32		hw_pos:1; /* start on vsync rising */
		u32		hw_neg:1; /* start on vsync falling */
		u32		rsv:24;
	} b;
};

union spicc_cfg_start {
	u32			d32;
	struct  {
		u32		block_num:20;
#define SPICC_BLOCK_MAX			0x100000

		u32		block_size:3;
		u32		dc_level:1;
#define SPICC_DC_DATA0_CMD1		0
#define SPICC_DC_DATA1_CMD0		1

		u32		op_mode:2;
#define SPICC_OP_MODE_WRITE_CMD		0
#define SPICC_OP_MODE_READ_STS		1
#define SPICC_OP_MODE_WRITE		2
#define SPICC_OP_MODE_READ		3

		u32		rx_data_mode:2;
		u32		tx_data_mode:2;
#define SPICC_DATA_MODE_NONE		0
#define SPICC_DATA_MODE_PIO		1
#define SPICC_DATA_MODE_MEM		2
#define SPICC_DATA_MODE_SG		3

		u32		eoc:1;
		u32		pending:1;
	} b;
};

union spicc_cfg_bus {
	u32			d32;
	struct  {
		u32		clk_div:8;
#define SPICC_CLK_DIV_MAX		255

		/* signed, -8~-1 early, 1~7 later */
		u32		rx_tuning:4;
		u32		tx_tuning:4;

		u32		ss_leading_gap:4;
		u32		lane:2;
#define SPICC_SINGLE_SPI		0
#define SPICC_DUAL_SPI			1
#define SPICC_QUAD_SPI			2

		u32		half_duplex_en:1;
		u32		little_endian_en:1;
		u32		dc_mode:1;
#define SPICC_DC_MODE_PIN		0
#define SPICC_DC_MODE_9BIT		1

		u32		null_ctl:1;
		u32		dummy_ctl:1;
		u32		read_turn_around:2;
#define SPICC_READ_TURN_AROUND_DEFAULT	1

		u32		keep_ss:1;
		u32		cpha:1;
		u32		cpol:1;
	} b;
};

struct spicc_sg_link {
	u32			valid:1;
	u32			eoc:1;
	u32			irq:1;
	u32			act:3;
	u32			ring:1;
	u32			rsv:1;
	u32			len:24;
#define SPICC_SG_LEN_MAX		0x1000000
#define SPICC_SG_TX_LEN_MAX		SPICC_SG_LEN_MAX
#define SPICC_SG_RX_LEN_MAX		SPICC_SG_LEN_MAX

#ifdef CONFIG_ARM64_SPICC
	u64			addr;
	u32			addr_rsv;
#else
	u32			addr;
#endif
};

struct spicc_descriptor {
	union spicc_cfg_start		cfg_start;
	union spicc_cfg_bus		cfg_bus;
	dma_addr_t			tx_paddr;
	dma_addr_t			rx_paddr;
};

struct spicc_device {
	struct spi_controller		*controller;
	struct platform_device		*pdev;
	void __iomem			*base;
	struct clk			*sys_clk;
	struct clk			*spi_clk;
	struct completion		completion;
	u32				status;
	u32			speed_hz;
	u32			effective_speed_hz;
	u32			bytes_per_word;
	union spicc_cfg_spi		cfg_spi;
	union spicc_cfg_start		cfg_start;
	union spicc_cfg_bus		cfg_bus;
	u8				config_ss_trailing_gap;
#ifdef MESON_SPICC_HW_IF
	void (*dirspi_complete)(void *context);
	void *dirspi_context;
	dma_addr_t			dirspi_tx_dma;
	dma_addr_t			dirspi_rx_dma;
	int				dirspi_len;
#endif
};

#define spicc_info(fmt, args...) \
	pr_info("[info]%s: " fmt, __func__, ## args)

#define spicc_err(fmt, args...) \
	pr_info("[error]%s: " fmt, __func__, ## args)

//#define SPICC_DEBUG_EN
#ifdef SPICC_DEBUG_EN
#define spicc_dbg(fmt, args...) \
	pr_info("[debug]%s: " fmt, __func__, ## args)
#else
#define spicc_dbg(fmt, args...)
#endif

#define spicc_writel(_spicc, _val, _offset) \
	 writel(_val, (_spicc)->base + (_offset))
#define spicc_readl(_spicc, _offset) \
	readl((_spicc)->base + (_offset))

#ifdef MESON_SPICC_HW_IF
static void dirspi_start(struct spi_device *spi);
static void dirspi_stop(struct spi_device *spi);
static int dirspi_async(struct spi_device *spi,
			u8 *tx_buf,
			u8 *rx_buf,
			int len,
			void (*complete)(void *context),
			void *context);
static int dirspi_sync(struct spi_device *spi,
			u8 *tx_buf,
			u8 *rx_buf,
			int len);
#endif

static inline int spicc_sem_down_read(struct spicc_device *spicc)
{
	int ret;

	ret = spicc_readl(spicc, SPICC_REG_CFG_READY);
	if (ret)
		spicc_writel(spicc, 0, SPICC_REG_CFG_READY);

	return ret;
}

static inline void spicc_sem_up_write(struct spicc_device *spicc)
{
	spicc_writel(spicc, 1, SPICC_REG_CFG_READY);
}

static int spicc_set_speed(struct spicc_device *spicc, uint speed_hz)
{
	u32 pclk_rate;
	u32 div;

	if (!speed_hz || speed_hz == spicc->speed_hz)
		return 0;

	spicc->speed_hz = speed_hz;
	/* speed = spi_clk rate / (div + 1) */
	pclk_rate = clk_get_rate(spicc->spi_clk);
	div = DIV_ROUND_UP(pclk_rate, speed_hz);
	if (div)
		div--;
	if (div > SPICC_CLK_DIV_MAX)
		div = SPICC_CLK_DIV_MAX;

	spicc->cfg_bus.b.clk_div = div;
	spicc->effective_speed_hz = pclk_rate / (div + 1);
	spicc_dbg("set speed %dHz (effective %dHz)\n", speed_hz, spicc->effective_speed_hz);

	return 0;
}

static inline unsigned long spicc_xfer_time_max(struct spicc_device *spicc,
						int len)
{
	unsigned long ms;

	ms = 8LL * 1000LL * len / spicc->effective_speed_hz;
	ms += ms + 20; /* some tolerance */

	return ms;
}

static bool meson_spicc_can_dma(struct spi_controller *ctlr,
				struct spi_device *spi,
				struct spi_transfer *xfer)
{
	return (xfer->len > 128) ? true : false;
}

static void spicc_sg_xlate(struct sg_table *sgt, struct spicc_sg_link *ccsg)
{
	struct scatterlist *sg;
	int i;

	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		ccsg->valid = 1;
		/* EOC specially for the last sg */
		ccsg->eoc = sg_is_last(sg);
		ccsg->ring = 0;
		ccsg->len = sg_dma_len(sg);
#ifdef CONFIG_ARM64_SPICC
		ccsg->addr = sg_dma_address(sg);
#else
		ccsg->addr = (u32)sg_dma_address(sg);
#endif
		ccsg++;
	}
}

static int nbits_to_lane[] = {
	SPICC_SINGLE_SPI,
	SPICC_SINGLE_SPI,
	SPICC_DUAL_SPI,
	-EINVAL,
	SPICC_QUAD_SPI
};

static int spicc_config_desc_one_transfer(struct spicc_device *spicc,
			struct spicc_descriptor *desc,
			struct spi_transfer *xfer,
			bool is_dma_mapped,
			bool ccxfer_en)
{
	int block_size, blocks;
	struct spicc_transfer *ccxfer = ccxfer_en ?
		container_of(xfer, struct spicc_transfer, xfer) : NULL;
	struct device *dev = spicc->controller->dev.parent;

	spicc_set_speed(spicc, xfer->speed_hz);
	desc->cfg_start.d32 = spicc->cfg_start.d32;
	desc->cfg_bus.d32 = spicc->cfg_bus.d32;

	block_size = xfer->bits_per_word >> 3;
	blocks = xfer->len / block_size;

	desc->cfg_start.b.tx_data_mode = SPICC_DATA_MODE_NONE;
	desc->cfg_start.b.rx_data_mode = SPICC_DATA_MODE_NONE;
	desc->cfg_start.b.eoc = 0;
	desc->cfg_bus.b.keep_ss = 1;
	desc->cfg_bus.b.null_ctl = 0;
	if (ccxfer && ccxfer->dc_mode) {
		desc->cfg_start.b.dc_level = ccxfer->dc_level;
		desc->cfg_bus.b.read_turn_around = ccxfer->read_turn_around;
		desc->cfg_bus.b.dc_mode = ccxfer->dc_mode - 1;
	}

	if (xfer->tx_buf) {
		desc->cfg_bus.b.lane = nbits_to_lane[xfer->tx_nbits];
		desc->cfg_start.b.op_mode = (ccxfer && ccxfer->dc_mode) ?
			SPICC_OP_MODE_WRITE_CMD : SPICC_OP_MODE_WRITE;
	}
	if (xfer->rx_buf) {
		desc->cfg_bus.b.lane = nbits_to_lane[xfer->rx_nbits];
		desc->cfg_start.b.op_mode = (ccxfer && ccxfer->dc_mode) ?
			SPICC_OP_MODE_READ_STS : SPICC_OP_MODE_READ;
	}

	if (desc->cfg_start.b.op_mode == SPICC_OP_MODE_READ_STS) {
		desc->cfg_start.b.block_size = blocks;
		desc->cfg_start.b.block_num = 1;
	} else if ((block_size == 1) && (blocks > SPICC_BLOCK_MAX)) {
		/* use 8byte + little_endian to transfer more than 1MB data */
		desc->cfg_bus.b.little_endian_en = 1;
		desc->cfg_start.b.block_size = 0;
		blocks >>= 3;
		blocks = min_t(int, blocks, SPICC_BLOCK_MAX);
		desc->cfg_start.b.block_num = blocks;
	} else {
		desc->cfg_start.b.block_size = block_size & 0x7;
		blocks = min_t(int, blocks, SPICC_BLOCK_MAX);
		desc->cfg_start.b.block_num = blocks;
	}

	if (is_dma_mapped) {
		if (xfer->tx_buf) {
			desc->tx_paddr = xfer->tx_dma;
			desc->cfg_start.b.tx_data_mode = SPICC_DATA_MODE_MEM;
		}
		if (xfer->rx_buf) {
			desc->rx_paddr = xfer->rx_dma;
			desc->cfg_start.b.rx_data_mode = SPICC_DATA_MODE_MEM;
		}
	} else if (ccxfer_en && (xfer->tx_sg.sgl || xfer->rx_sg.sgl)) {
		if (xfer->tx_buf) {
			ccxfer->tx_ccsg_len = xfer->tx_sg.nents * sizeof(void *);
			ccxfer->tx_ccsg = dma_alloc_coherent(dev,
					ccxfer->tx_ccsg_len,
					&desc->tx_paddr,
					GFP_KERNEL | GFP_DMA);
			if (!ccxfer->tx_ccsg) {
				spicc_err("alloc tx_ccsg failed\n");
				return -ENOMEM;
			}
			spicc_sg_xlate(&xfer->tx_sg, ccxfer->tx_ccsg);
			desc->cfg_start.b.tx_data_mode = SPICC_DATA_MODE_SG;
		}

		if (xfer->rx_buf) {
			ccxfer->rx_ccsg_len = xfer->rx_sg.nents * sizeof(void *);
			ccxfer->rx_ccsg = dma_alloc_coherent(dev,
					ccxfer->rx_ccsg_len,
					&desc->rx_paddr,
					GFP_KERNEL | GFP_DMA);
			if (!ccxfer->rx_ccsg) {
				if (ccxfer->tx_ccsg)
					dma_free_coherent(dev,
							ccxfer->tx_ccsg_len,
							ccxfer->tx_ccsg,
							desc->tx_paddr);
				spicc_err("alloc rx_ccsg failed\n");
				return -ENOMEM;
			}

			spicc_sg_xlate(&xfer->rx_sg, ccxfer->rx_ccsg);
			desc->cfg_start.b.rx_data_mode = SPICC_DATA_MODE_SG;
		}
	} else {
		if (xfer->tx_buf) {
			xfer->tx_dma = dma_map_single(dev,
					(void *)xfer->tx_buf,
					xfer->len,
					DMA_TO_DEVICE);
			if (dma_mapping_error(dev, xfer->tx_dma)) {
				dev_err(dev, "tx_dma map failed\n");
				return -ENOMEM;
			}
			desc->tx_paddr = xfer->tx_dma;
			desc->cfg_start.b.tx_data_mode = SPICC_DATA_MODE_MEM;
		}

		if (xfer->rx_buf) {
			xfer->rx_dma = dma_map_single(dev,
					xfer->rx_buf,
					xfer->len,
					DMA_FROM_DEVICE);
			if (dma_mapping_error(dev, xfer->rx_dma)) {
				if (xfer->tx_buf)
					dma_unmap_single(dev,
							xfer->tx_dma,
							xfer->len,
							DMA_TO_DEVICE);
				dev_err(dev, "rx_dma map failed\n");
				return -ENOMEM;
			}
			desc->rx_paddr = xfer->rx_dma;
			desc->cfg_start.b.rx_data_mode = SPICC_DATA_MODE_MEM;
		}
	}

	return 0;
}

static struct spicc_descriptor *spicc_create_desc_table
			(struct spicc_device *spicc,
			struct spi_message *msg,
			dma_addr_t *paddr,
			int *desc_len,
			int *xfer_len)
{
	struct spi_transfer *xfer;
	struct spicc_descriptor *desc, *desc_bk;
	int desc_num = 0;
	int len = 0;
	struct spicc_controller_data *cdata = msg->spi->controller_data;

	/*calculate the desc num for all xfer */
	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		len += xfer->len;
		desc_num++;
	}

	/* additional descriptor to achieve a ss trailing gap */
	if (spicc->config_ss_trailing_gap)
		desc_num++;
	*desc_len = sizeof(*desc) * desc_num;
	desc = dma_alloc_coherent(spicc->controller->dev.parent,
				  *desc_len, paddr, GFP_KERNEL | GFP_DMA);
	desc_bk = desc;
	*xfer_len = len;
	if (!desc_num || !desc) {
		spicc_err("alloc desc failed\n");
		return NULL;
	}

	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		if (spicc_config_desc_one_transfer(spicc, desc, xfer,
				msg->is_dma_mapped,
				cdata ? cdata->ccxfer_en : 0))
			return NULL;
		desc++;
	}

	/* configure the additional descriptor null */
	if (spicc->config_ss_trailing_gap) {
		desc->cfg_start.d32 = spicc->cfg_start.d32;
		desc->cfg_bus.d32 = spicc->cfg_bus.d32;
		desc->cfg_start.b.op_mode = SPICC_OP_MODE_WRITE;
		desc->cfg_start.b.block_size = 1;
		desc->cfg_start.b.block_num = spicc->config_ss_trailing_gap;
		desc->cfg_bus.b.null_ctl = 1;
	} else {
		desc--;
	}

	desc->cfg_bus.b.keep_ss = 0;
	desc->cfg_start.b.eoc = 1;

	return desc_bk;
}

static void spicc_destroy_desc_table(struct spicc_device *spicc,
				     struct spicc_descriptor *desc_table,
				     struct spi_message *msg,
				     dma_addr_t paddr,
				     int desc_len)
{
	struct device *dev = spicc->controller->dev.parent;
	struct spicc_descriptor *desc = desc_table;
	struct spi_transfer *xfer;
	struct spicc_controller_data *cdata = msg->spi->controller_data;
	bool ccxfer_en = cdata ? cdata->ccxfer_en : 0;
	struct spicc_transfer *ccxfer;

	if (!desc)
		return;
	if (msg->is_dma_mapped)
		goto end;

	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		if (ccxfer_en && (xfer->tx_sg.sgl || xfer->rx_sg.sgl)) {
			ccxfer = container_of(xfer, struct spicc_transfer, xfer);
			if (xfer->tx_buf) {
				dma_free_coherent(dev,
						ccxfer->tx_ccsg_len,
						ccxfer->tx_ccsg,
						desc->tx_paddr);
			}

			if (xfer->rx_buf) {
				dma_free_coherent(dev,
						ccxfer->rx_ccsg_len,
						ccxfer->rx_ccsg,
						desc->rx_paddr);
			}
		} else {
			if (xfer->tx_buf)
				dma_unmap_single(dev,
						xfer->tx_dma,
						xfer->len,
						DMA_TO_DEVICE);

			if (xfer->rx_buf)
				dma_unmap_single(dev,
						xfer->rx_dma,
						xfer->len,
						DMA_FROM_DEVICE);
		}

		desc++;
	}

end:
	dma_free_coherent(dev, desc_len, desc_table, paddr);
}

static int spicc_xfer_desc(struct spicc_device *spicc,
			 struct spicc_descriptor *desc_table,
			 int xfer_len, dma_addr_t paddr)
{
	int ret;
	unsigned long ms = spicc_xfer_time_max(spicc, xfer_len);

	reinit_completion(&spicc->completion);
	spicc_writel(spicc, SPICC_DESC_CHAIN_DONE, SPICC_REG_IRQ_ENABLE);
	spicc_writel(spicc, spicc->cfg_spi.d32, SPICC_REG_CFG_SPI);
	spicc_writel(spicc, 0, SPICC_REG_CFG_BUS);
	spicc_writel(spicc, 0, SPICC_REG_CFG_START);
	spicc_writel(spicc, (u64)paddr & 0xffffffff,
		     SPICC_REG_DESC_LIST_L);
	spicc_writel(spicc, ((u64)paddr >> 32) | SPICC_DESC_PENDING,
		     SPICC_REG_DESC_LIST_H);
	ret = wait_for_completion_timeout(&spicc->completion,
			spi_controller_is_slave(spicc->controller) ?
			MAX_SCHEDULE_TIMEOUT : msecs_to_jiffies(ms));

	return ret ? (spicc->status ? -EIO : 0) : -ETIMEDOUT;
}

static irqreturn_t meson_spicc_irq(int irq, void *data)
{
	struct spicc_device *spicc = (void *)data;
	u32 sts;

	spicc->status = 0;
	sts = spicc_readl(spicc, SPICC_REG_IRQ_STS);
	spicc_writel(spicc, sts, SPICC_REG_IRQ_STS);
	if (sts & (SPICC_RCH_DESC_INVALID |
		   SPICC_RCH_DESC_RESP |
		   SPICC_RCH_DATA_RESP |
		   SPICC_WCH_DESC_INVALID |
		   SPICC_WCH_DESC_RESP |
		   SPICC_WCH_DATA_RESP |
		   SPICC_DESC_ERR)) {
		spicc->status = sts;
	}

#ifdef MESON_SPICC_HW_IF
	if (spicc->dirspi_complete) {
		spicc_sem_up_write(spicc);
		if (spicc->dirspi_tx_dma)
			dma_unmap_single(spicc->controller->dev.parent,
					spicc->dirspi_tx_dma,
					spicc->dirspi_len,
					DMA_TO_DEVICE);
		if (spicc->dirspi_rx_dma)
			dma_unmap_single(spicc->controller->dev.parent,
					spicc->dirspi_rx_dma,
					spicc->dirspi_len,
					DMA_FROM_DEVICE);
		spicc->dirspi_complete(spicc->dirspi_context);
		spicc->dirspi_complete = NULL;
		return IRQ_HANDLED;
	}
#endif

	complete(&spicc->completion);

	return IRQ_HANDLED;
}

/*
 * spi_transfer_one_message - Default implementation of transfer_one_message()
 *
 * This is a standard implementation of transfer_one_message() for
 * drivers which implement a transfer_one() operation.  It provides
 * standard handling of delays and chip select management.
 */
static int meson_spicc_transfer_one_message(struct spi_controller *ctlr,
					    struct spi_message *msg)
{
	struct spicc_device *spicc = spi_controller_get_devdata(ctlr);
	struct spicc_descriptor *desc_table;
	dma_addr_t paddr;
	int desc_len = 0, xfer_len = 0;
	int ret = -EIO;

	msg->actual_length = 0;
	if (!spicc_sem_down_read(spicc)) {
		spicc_err("controller busy\n");
		return -EBUSY;
	}

	desc_table = spicc_create_desc_table(spicc, msg, &paddr, &desc_len, &xfer_len);
	if (desc_table) {
		ret = spicc_xfer_desc(spicc, desc_table, xfer_len, paddr);
		if (!ret)
			msg->actual_length = xfer_len;
		spicc_destroy_desc_table(spicc, desc_table, msg, paddr, desc_len);
	}

	msg->status = ret;
	spi_finalize_current_message(ctlr);
	spicc_sem_up_write(spicc);

	return ret;
}

static int meson_spicc_prepare_message(struct spi_controller *ctlr,
				       struct spi_message *message)
{
	//struct spicc_device *spicc = spi_controller_get_devdata(ctlr);
	//struct spi_device *spi = message->spi;

	return 0;
}

static int meson_spicc_unprepare_transfer(struct spi_controller *ctlr)
{
	//struct spicc_device *spicc = spi_controller_get_devdata(ctlr);

	return 0;
}

static int meson_spicc_setup(struct spi_device *spi)
{
	struct spicc_device *spicc;
	struct  spicc_controller_data *cdata;

	spicc = spi_controller_get_devdata(spi->controller);
	if (!spi->bits_per_word || spi->bits_per_word % 8) {
		spicc_err("invalid wordlen %d\n", spi->bits_per_word);
		return -EINVAL;
	}

	spicc_set_speed(spicc, spi->max_speed_hz);
	spicc->bytes_per_word = spi->bits_per_word >> 3;
	spicc->cfg_start.b.block_size = spicc->bytes_per_word & 0x7;
	spicc->cfg_spi.b.ss = spi->chip_select;

	spicc->cfg_bus.b.cpol = !!(spi->mode & SPI_CPOL);
	spicc->cfg_bus.b.cpha = !!(spi->mode & SPI_CPHA);
	spicc->cfg_bus.b.little_endian_en = !!(spi->mode & SPI_LSB_FIRST);
	spicc->cfg_bus.b.half_duplex_en = !!(spi->mode & SPI_3WIRE);

	cdata = (struct spicc_controller_data *)spi->controller_data;
	if (cdata && cdata->timing_en) {
		/* SCLK * N */
		spicc->cfg_bus.b.ss_leading_gap = cdata->ss_leading_gap;
		/* 2.75us + SCLK * 9 * N */
		spicc->config_ss_trailing_gap = cdata->ss_trailing_gap;
		/* 4bit signed, SCLK * N */
		spicc->cfg_bus.b.tx_tuning = cdata->tx_tuning;
		spicc->cfg_bus.b.rx_tuning = cdata->rx_tuning;
		spicc->cfg_bus.b.dummy_ctl = cdata->dummy_ctl;
	} else if (spi_controller_is_slave(spicc->controller)) {
		spicc->cfg_bus.b.ss_leading_gap = 0;
		spicc->config_ss_trailing_gap = 0;
		spicc->cfg_bus.b.tx_tuning = 15; /* -1 SCLK */
		spicc->cfg_bus.b.rx_tuning = 0;
		spicc->cfg_bus.b.dummy_ctl = 0;
	} else {
		spicc->cfg_bus.b.ss_leading_gap = 5;
		spicc->config_ss_trailing_gap = 1;
		spicc->cfg_bus.b.tx_tuning = 0;
		spicc->cfg_bus.b.rx_tuning = 7; /* 7 SCLK */
		spicc->cfg_bus.b.dummy_ctl = 0;
	}

#ifdef MESON_SPICC_HW_IF
	if (cdata) {
		cdata->dirspi_start = dirspi_start;
		cdata->dirspi_stop = dirspi_stop;
		cdata->dirspi_async = dirspi_async;
		cdata->dirspi_sync = dirspi_sync;
	}
#endif

	spicc_dbg("set mode 0x%x\n", spi->mode);

	return 0;
}

static void meson_spicc_cleanup(struct spi_device *spi)
{
	spi->controller_state = NULL;
}

static int meson_spicc_slave_abort(struct spi_controller *ctlr)
{
	struct spicc_device *spicc = spi_controller_get_devdata(ctlr);

	spicc->status = 0;
	spicc_writel(spicc, 0, SPICC_REG_DESC_LIST_H);
	complete(&spicc->completion);

	return 0;
}

#ifdef MESON_SPICC_HW_IF
static int spicc_wait_complete(struct spicc_device *spicc, u32 flags,
				unsigned long timeout)
{
	u32 sts;

	timeout += jiffies;
	while (time_before(jiffies, timeout)) {
		sts = spicc_readl(spicc, SPICC_REG_IRQ_STS);
		if (sts & (SPICC_RCH_DESC_INVALID |
			   SPICC_RCH_DESC_RESP |
			   SPICC_RCH_DATA_RESP |
			   SPICC_WCH_DESC_INVALID |
			   SPICC_WCH_DESC_RESP |
			   SPICC_WCH_DATA_RESP |
			   SPICC_DESC_ERR)) {
			spicc_err("controller error sts=0x%x\n", sts);
			return -EIO;
		}

		if (sts & flags) {
			spicc_writel(spicc, sts, SPICC_REG_IRQ_STS);
			return 0;
		}

		cpu_relax();
	}

	spicc_err("timedout, sts=0x%x\n", sts);
	return -ETIMEDOUT;
}

static void dirspi_start(struct spi_device *spi)
{
	if (spi->controller->auto_runtime_pm)
		pm_runtime_get_sync(spi->controller->dev.parent);
}

static void dirspi_stop(struct spi_device *spi)
{
}

/*
 * Return
 * <0: error code if the transfer is failed
 * 0: if the transfer is finished.(callback executed)
 * >0: if the transfer is still in progress
 */
static int dirspi_async(struct spi_device *spi,
			u8 *tx_buf,
			u8 *rx_buf,
			int len,
			void (*complete)(void *context),
			void *context)
{
	struct spicc_device *spicc = spi_controller_get_devdata(spi->controller);
	struct device *dev = spicc->controller->dev.parent;
	dma_addr_t tx_dma = 0, rx_dma = 0;
	int ret = -EINVAL;
	unsigned long ms = spicc_xfer_time_max(spicc, len);

	if (tx_buf) {
		tx_dma = dma_map_single(dev, (void *)tx_buf, len, DMA_TO_DEVICE);
		ret = dma_mapping_error(dev, tx_dma);
		if (ret)
			goto end;
		spicc->cfg_start.b.tx_data_mode = SPICC_DATA_MODE_MEM;
		spicc->cfg_start.b.op_mode = SPICC_OP_MODE_WRITE;
	} else {
		tx_dma = 0;
		spicc->cfg_start.b.tx_data_mode = SPICC_DATA_MODE_NONE;
	}

	if (rx_buf) {
		rx_dma = dma_map_single(dev, (void *)rx_buf, len, DMA_FROM_DEVICE);
		ret = dma_mapping_error(dev, rx_dma);
		if (ret)
			goto end;
		spicc->cfg_start.b.rx_data_mode = SPICC_DATA_MODE_MEM;
		spicc->cfg_start.b.op_mode = SPICC_OP_MODE_READ;
	} else {
		rx_dma = 0;
		spicc->cfg_start.b.rx_data_mode = SPICC_DATA_MODE_NONE;
	}

	spicc->cfg_bus.b.lane = SPICC_SINGLE_SPI;
	spicc->cfg_start.b.block_size = (spi->bits_per_word >> 3) & 7;
	spicc->cfg_start.b.block_num = len / (spi->bits_per_word >> 3);
	spicc->cfg_start.b.eoc = 1;
	spicc->cfg_bus.b.keep_ss = 0;
	spicc->cfg_bus.b.null_ctl = 0;

	spicc->dirspi_complete = complete;
	spicc->dirspi_context = context;
	spicc->dirspi_tx_dma = tx_dma;
	spicc->dirspi_rx_dma = rx_dma;
	spicc->dirspi_len = len;

	if (!spicc_sem_down_read(spicc)) {
		spicc_err("controller busy\n");
		ret = -EBUSY;
		goto end;
	}

	spicc_writel(spicc, complete ? SPICC_DESC_DONE : 0, SPICC_REG_IRQ_ENABLE);
	spicc_writel(spicc, tx_dma, SPICC_REG_MEM_TX_ADDR_L);
	spicc_writel(spicc, 0, SPICC_REG_MEM_TX_ADDR_H);
	spicc_writel(spicc, rx_dma, SPICC_REG_MEM_RX_ADDR_L);
	spicc_writel(spicc, 0, SPICC_REG_MEM_RX_ADDR_H);
	spicc_writel(spicc, spicc->cfg_spi.d32, SPICC_REG_CFG_SPI);
	spicc_writel(spicc, spicc->cfg_bus.d32, SPICC_REG_CFG_BUS);
	spicc_writel(spicc, spicc->cfg_start.d32 | SPICC_DESC_PENDING, SPICC_REG_CFG_START);

	if (complete)
		return 1;

	ret = spicc_wait_complete(spicc, SPICC_DESC_DONE, msecs_to_jiffies(ms));
	spicc_sem_up_write(spicc);

end:
	if (rx_dma)
		dma_unmap_single(dev, rx_dma, len, DMA_FROM_DEVICE);
	if (tx_dma)
		dma_unmap_single(dev, tx_dma, len, DMA_TO_DEVICE);

	return ret;
}

static int dirspi_sync(struct spi_device *spi,
			u8 *tx_buf,
			u8 *rx_buf,
			int len)
{
	return dirspi_async(spi, tx_buf, rx_buf, len, NULL, NULL);
}
#endif	/* end of MESON_SPICC_HW_IF */

#ifdef CONFIG_SPICC_TEST
static DEVICE_ATTR_WO(test);
static DEVICE_ATTR_RW(testdev);
#endif

static int meson_spicc_probe(struct platform_device *pdev)
{
	struct spi_controller *ctlr;
	struct spicc_device *spicc;
	int ret, irq;

	ctlr = __spi_alloc_controller(&pdev->dev, sizeof(*spicc),
			of_property_read_bool(pdev->dev.of_node, "slave"));
	if (!ctlr) {
		dev_err(&pdev->dev, "controller allocation failed\n");
		return -ENOMEM;
	}
	spicc = spi_controller_get_devdata(ctlr);
	spicc->controller = ctlr;

	spicc->pdev = pdev;
	platform_set_drvdata(pdev, spicc);

	spicc->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR_OR_NULL(spicc->base)) {
		dev_err(&pdev->dev, "io resource mapping failed\n");
		ret = PTR_ERR(spicc->base);
		goto out_controller;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = irq;
		goto out_controller;
	}

	ret = devm_request_irq(&pdev->dev, irq, meson_spicc_irq,
			       IRQF_TRIGGER_RISING, NULL, spicc);
	if (ret) {
		dev_err(&pdev->dev, "irq request failed\n");
		goto out_controller;
	}

	spicc->sys_clk = devm_clk_get(&pdev->dev, "sys");
	if (IS_ERR_OR_NULL(spicc->sys_clk))
		dev_warn(&pdev->dev, "no sys clock\n");
	else
		clk_prepare_enable(spicc->sys_clk);

	spicc->spi_clk = devm_clk_get(&pdev->dev, "spi");
	if (IS_ERR_OR_NULL(spicc->spi_clk)) {
		dev_err(&pdev->dev, "spi clock request failed\n");
		ret = PTR_ERR(spicc->spi_clk);
		goto out_controller;
	}
	ret = clk_prepare_enable(spicc->spi_clk);
	if (ret) {
		dev_err(&pdev->dev, "spi clock enable failed\n");
		goto out_controller;
	}

	device_reset_optional(&pdev->dev);
	ctlr->num_chipselect = 4;
	ctlr->dev.of_node = pdev->dev.of_node;
	ctlr->mode_bits = SPI_CPHA | SPI_CPOL | SPI_LSB_FIRST |
			  SPI_3WIRE | SPI_TX_QUAD | SPI_RX_QUAD;
	ctlr->max_speed_hz = 100000000;
	ctlr->min_speed_hz = 1000000;
	ctlr->setup = meson_spicc_setup;
	ctlr->cleanup = meson_spicc_cleanup;
	ctlr->prepare_message = meson_spicc_prepare_message;
	ctlr->unprepare_transfer_hardware = meson_spicc_unprepare_transfer;
	ctlr->transfer_one_message = meson_spicc_transfer_one_message;
	ctlr->slave_abort = meson_spicc_slave_abort;
	ctlr->can_dma = meson_spicc_can_dma;
	ctlr->max_dma_len = SPICC_BLOCK_MAX;
	dma_set_max_seg_size(&pdev->dev, SPICC_BLOCK_MAX);
	ret = devm_spi_register_master(&pdev->dev, ctlr);
	if (ret) {
		dev_err(&pdev->dev, "spi controller registration failed\n");
		goto out_clk;
	}

	init_completion(&spicc->completion);

#ifdef CONFIG_SPICC_TEST
	device_create_file(&ctlr->dev, &dev_attr_test);
	device_create_file(&ctlr->dev, &dev_attr_testdev);
#endif

	spicc->cfg_spi.d32 = 0;
	spicc->cfg_start.d32 = 0;
	spicc->cfg_bus.d32 = 0;

	spicc->cfg_spi.b.flash_wp_pin_en = 1;
	spicc->cfg_spi.b.flash_hold_pin_en = 1;
	if (spi_controller_is_slave(ctlr))
		spicc->cfg_spi.b.slave_en = true;
	/* default pending */
	spicc->cfg_start.b.pending = 1;

	return 0;

out_clk:
	if (spicc->sys_clk)
		clk_disable_unprepare(spicc->sys_clk);
	clk_disable_unprepare(spicc->spi_clk);
out_controller:
	spi_controller_put(ctlr);

	return ret;
}

static int meson_spicc_remove(struct platform_device *pdev)
{
	struct spicc_device *spicc = platform_get_drvdata(pdev);

	if (spicc->sys_clk)
		clk_disable_unprepare(spicc->sys_clk);
	clk_disable_unprepare(spicc->spi_clk);
#ifdef CONFIG_SPICC_TEST
	device_remove_file(&pdev->dev, &dev_attr_test);
	device_remove_file(&pdev->dev, &dev_attr_testdev);
#endif
	return 0;
}

static int meson_spicc_suspend(struct device *dev)
{
	struct spicc_device *spicc = dev_get_drvdata(dev);

	return spi_controller_suspend(spicc->controller);
}

static int meson_spicc_resume(struct device *dev)
{
	struct spicc_device *spicc = dev_get_drvdata(dev);

	return spi_controller_resume(spicc->controller);
}

static const struct dev_pm_ops meson_spicc_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(meson_spicc_suspend, meson_spicc_resume)
};

static const struct of_device_id meson_spicc_v2_of_match[] = {
	{
		.compatible = "amlogic,meson-a4-spicc-v2",
		.data = 0,
	},

	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, meson_spicc_v2_of_match);

static struct platform_driver meson_spicc_v2_driver = {
	.probe   = meson_spicc_probe,
	.remove  = meson_spicc_remove,
	.driver  = {
		.name = "meson-spicc-v2",
		.of_match_table = of_match_ptr(meson_spicc_v2_of_match),
		.pm = &meson_spicc_pm_ops,
	},
};

module_platform_driver(meson_spicc_v2_driver);

MODULE_DESCRIPTION("Meson SPI Communication Controller(v2) driver");
MODULE_AUTHOR("Sunny.luo <sunny.luo@amlogic.com>");
MODULE_LICENSE("GPL");
