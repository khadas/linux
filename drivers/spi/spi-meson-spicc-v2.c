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
	struct spicc_sg_link		*tx_sg;
	struct spicc_sg_link		*rx_sg;
};

struct spicc_xfer {
	const void			*tx_buf;
	void				*rx_buf;
	int				len;
	unsigned long			flags;
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
	u8				config_data_mode;
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

/* test flags for spicc, must keep with xfer flags in spi.h */
#define SPI_XFER_LANE		GENMASK(9, 8)	/* 0-single, 1-dual, 2-quad */
#define SPI_XFER_HALF_DUPLEX_EN	BIT(10)		/* 0-full, 1-half */
#define SPI_XFER_DC_MODE	BIT(11)		/* 0-pin, 1-9bit */
#define SPI_XFER_DC_LEVEL	BIT(12)		/* dc bit level */
#define SPI_XFER_NULL_CTL	BIT(13)
#define SPI_XFER_DUMMY_CTL	BIT(14)

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

static int meson_spicc_dma_map(struct spicc_device *spicc,
			       struct spi_transfer *t)
{
	struct device *dev = spicc->controller->dev.parent;

	if (t->tx_buf) {
		t->tx_dma = dma_map_single(dev, (void *)t->tx_buf, t->len,
					   DMA_TO_DEVICE);
		if (dma_mapping_error(dev, t->tx_dma)) {
			dev_err(dev, "tx_dma map failed\n");
			return -ENOMEM;
		}
	}

	if (t->rx_buf) {
		t->rx_dma = dma_map_single(dev, t->rx_buf, t->len,
					   DMA_FROM_DEVICE);
		if (dma_mapping_error(dev, t->rx_dma)) {
			if (t->tx_buf)
				dma_unmap_single(dev, t->tx_dma, t->len,
						 DMA_TO_DEVICE);
			dev_err(dev, "rx_dma map failed\n");
			return -ENOMEM;
		}
	}

	return 0;
}

static void meson_spicc_dma_unmap(struct spicc_device *spicc,
				  struct spi_transfer *t)
{
	struct device *dev = spicc->controller->dev.parent;

	if (t->tx_buf)
		dma_unmap_single(dev, t->tx_dma, t->len, DMA_TO_DEVICE);
	if (t->rx_buf)
		dma_unmap_single(dev, t->rx_dma, t->len, DMA_FROM_DEVICE);
}

static struct spicc_sg_link *spicc_map_buf(u64 *addr_table, int *len_table, int nents)
{
	struct spicc_sg_link *sg_table, *sg;
	int i;

	sg_table = kcalloc(nents, sizeof(*sg), GFP_KERNEL);
	if (!sg_table)
		return NULL;

	for (i = 0; i < nents; i++) {
		sg = &sg_table[i];
		sg->valid = 1;
		/* EOC specially for the last sg */
		sg->eoc = (i == nents - 1) ? 1 : 0;
		sg->ring = 0;
		sg->len = len_table[i];
#ifdef CONFIG_ARM64_SPICC
		sg->addr = addr_table[i];
#else
		sg->addr = (u32)addr_table[i];
#endif
	}

	return sg_table;
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
			struct spi_transfer *xfer)
{
	u64 tx_addr = (u64)xfer->tx_dma;
	u64 rx_addr = (u64)xfer->rx_dma;
	int block_size, xfer_blocks;
	int blocks, len, desc_used_num = 0;
	bool first_desc = 1;

	if (xfer->bits_per_word)
		block_size = xfer->bits_per_word >> 3;
	else
		block_size = spicc->bytes_per_word;
	xfer_blocks = xfer->len / block_size;

	spicc_set_speed(spicc, xfer->speed_hz);
	//spicc_import_config(spicc, xfer->flags);
	spicc->cfg_start.b.dc_level = 0;
	spicc->cfg_bus.b.dc_mode = 0;
	spicc->cfg_bus.b.half_duplex_en = 0;
	spicc->cfg_bus.b.null_ctl = 0;
	spicc->cfg_bus.b.dummy_ctl = 0;

	while (xfer_blocks) {
		blocks = min_t(int, xfer_blocks, SPICC_BLOCK_MAX);
		len = blocks * block_size;
		desc->cfg_start.d32 = spicc->cfg_start.d32;
		desc->cfg_bus.d32 = spicc->cfg_bus.d32;
		desc->cfg_start.b.block_size = block_size & 0x7;
		desc->cfg_start.b.block_num = blocks;

		if (tx_addr) {
			desc->cfg_bus.b.lane = nbits_to_lane[xfer->tx_nbits];
			desc->cfg_start.b.op_mode = SPICC_OP_MODE_WRITE;
			desc->cfg_start.b.tx_data_mode = spicc->config_data_mode;
			if (spicc->config_data_mode == SPICC_DATA_MODE_SG)
				desc->tx_sg = spicc_map_buf(&tx_addr, &len, 1);
			else if (first_desc)
				desc->tx_sg = (struct spicc_sg_link *)tx_addr;
			else
				desc->tx_sg = 0;
			tx_addr += len;
		}

		if (rx_addr) {
			desc->cfg_bus.b.lane = nbits_to_lane[xfer->rx_nbits];
			desc->cfg_start.b.op_mode = SPICC_OP_MODE_READ;
			desc->cfg_start.b.rx_data_mode = spicc->config_data_mode;
			desc->cfg_bus.b.read_turn_around = SPICC_READ_TURN_AROUND_DEFAULT;
			if (spicc->config_data_mode == SPICC_DATA_MODE_SG)
				desc->rx_sg = spicc_map_buf(&rx_addr, &len, 1);
			else if (first_desc)
				desc->rx_sg = (struct spicc_sg_link *)rx_addr;
			else
				desc->rx_sg = 0;
			rx_addr += len;
		}

		desc++;
		desc_used_num++;
		xfer_blocks -= blocks;
		first_desc = 0;
	}

	if (desc_used_num && spi_transfer_is_last(spicc->controller, xfer)) {
		desc--;
		desc->cfg_start.b.eoc = 1;
		desc->cfg_bus.b.keep_ss = 0;
	}

	return desc_used_num;
}

static struct spicc_descriptor *spicc_create_desc_table
			(struct spicc_device *spicc,
			struct spi_message *msg,
			int *desc_len,
			int *xfer_len)
{
	struct spi_transfer *xfer;
	struct spicc_descriptor *desc, *desc_bk;
	int blocks, desc_num = 0;
	int len = 0;

	/*calculate the desc num for all xfer */
	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		if (!msg->is_dma_mapped && meson_spicc_dma_map(spicc, xfer))
			return NULL;
		len += xfer->len;
		blocks = xfer->len / (xfer->bits_per_word >> 3);
		desc_num += DIV_ROUND_UP(blocks, SPICC_BLOCK_MAX);
	}

	desc = kcalloc(desc_num, sizeof(*desc), GFP_KERNEL);
	desc_bk = desc;
	*desc_len = sizeof(*desc) * desc_num;
	*xfer_len = len;
	if (!desc_num || !desc)
		return NULL;

	/* default */
	spicc->cfg_start.b.tx_data_mode = SPICC_DATA_MODE_NONE;
	spicc->cfg_start.b.rx_data_mode = SPICC_DATA_MODE_NONE;
	spicc->cfg_start.b.pending = 1;
	spicc->cfg_start.b.eoc = 0;
	spicc->cfg_bus.b.keep_ss = 1;

	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		desc_num = spicc_config_desc_one_transfer(spicc, desc, xfer);
		desc += desc_num;
	}

	return desc_bk;
}

static void spicc_destroy_desc_table(struct spicc_device *spicc,
				     struct spicc_descriptor *desc_table,
				     struct spi_message *msg)
{
	struct spicc_descriptor *desc = desc_table;
	struct spi_transfer *xfer;

	if (!desc)
		return;

	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		meson_spicc_dma_unmap(spicc, xfer);
	}

	while (1) {
		if (desc->tx_sg && desc->cfg_start.b.tx_data_mode == SPICC_DATA_MODE_SG)
			kfree(desc->tx_sg);
		if (desc->rx_sg && desc->cfg_start.b.rx_data_mode == SPICC_DATA_MODE_SG)
			kfree(desc->rx_sg);

		if (desc->cfg_start.b.eoc)
			break;
		desc++;
	}

	kfree(desc_table);
}

static int spicc_xfer_desc(struct spicc_device *spicc,
			 struct spicc_descriptor *desc_table, int xfer_len)
{
	u64 phy_addr = virt_to_phys(desc_table);
	int ret;
	unsigned long long ms;

	ms = 8LL * 1000LL * xfer_len / spicc->effective_speed_hz; /* unit ms */
	ms += ms + 20; /* some tolerance */
	reinit_completion(&spicc->completion);

	spicc_writel(spicc, SPICC_DESC_CHAIN_DONE, SPICC_REG_IRQ_ENABLE);
	spicc_writel(spicc, spicc->cfg_spi.d32, SPICC_REG_CFG_SPI);
	spicc_writel(spicc, 0, SPICC_REG_CFG_BUS);
	spicc_writel(spicc, 0, SPICC_REG_CFG_START);
	spicc_writel(spicc, phy_addr & 0xffffffff,
		     SPICC_REG_DESC_LIST_L);
	spicc_writel(spicc, (phy_addr >> 32) | SPICC_DESC_PENDING,
		     SPICC_REG_DESC_LIST_H);
	ret = wait_for_completion_timeout(&spicc->completion, msecs_to_jiffies(ms));
	if (ret == 0) {
		spicc_err("transfer timed out\n");
		return -ETIMEDOUT;
	} else if (spicc->status) {
		spicc_err("transfer status error\n");
		return -EIO;
	}

	return 0;
}

static irqreturn_t meson_spicc_irq(int irq, void *data)
{
	struct spicc_device *spicc = (void *)data;
	u32 sts;

	sts = spicc_readl(spicc, SPICC_REG_IRQ_STS);
	//spicc_info("controller sts=0x%x\n", sts);
	if (sts & (SPICC_RCH_DESC_INVALID |
		   SPICC_RCH_DESC_RESP |
		   SPICC_RCH_DATA_RESP |
		   SPICC_WCH_DESC_INVALID |
		   SPICC_WCH_DESC_RESP |
		   SPICC_WCH_DATA_RESP |
		   SPICC_DESC_ERR)) {
		spicc->status = sts;
		complete(&spicc->completion);
	}

	else if (sts & SPICC_DESC_CHAIN_DONE) {
		spicc_writel(spicc, sts, SPICC_REG_IRQ_STS);
		spicc->status = 0;
		complete(&spicc->completion);
	}

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
	int desc_len = 0, xfer_len = 0;
	int ret = 0;

	msg->status = 0;
	msg->actual_length = 0;
	if (!spicc_sem_down_read(spicc)) {
		spicc_err("controller busy\n");
		return -EBUSY;
	}

	desc_table = spicc_create_desc_table(spicc, msg, &desc_len, &xfer_len);
	if (desc_table) {
		__flush_dcache_area(desc_table, desc_len);
		ret = spicc_xfer_desc(spicc, desc_table, xfer_len);
		__inval_dcache_area(desc_table, desc_len);
		spicc_destroy_desc_table(spicc, desc_table, msg);
	}

	msg->status = ret;
	msg->actual_length = xfer_len;
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

	spicc = spi_controller_get_devdata(spi->controller);
	if (!spi->bits_per_word || spi->bits_per_word % 8) {
		spicc_err("invalid wordlen %d\n", spi->bits_per_word);
		return -EINVAL;
	}

	spicc->bytes_per_word = spi->bits_per_word >> 3;
	spicc->cfg_start.b.block_size = spicc->bytes_per_word & 0x7;
	spicc->cfg_spi.b.ss = spi->chip_select;

	spicc->cfg_bus.b.cpol = !!(spi->mode & SPI_CPOL);
	spicc->cfg_bus.b.cpha = !!(spi->mode & SPI_CPHA);
	spicc->cfg_bus.b.little_endian_en = !!(spi->mode & SPI_LSB_FIRST);
	spicc->cfg_bus.b.half_duplex_en = !!(spi->mode & SPI_3WIRE);

	spicc_dbg("set mode 0x%x\n", spi->mode);

	return 0;
}

static void meson_spicc_cleanup(struct spi_device *spi)
{
	spi->controller_state = NULL;
}

#define TEST_PARAM_NUM 5
static ssize_t test_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct spicc_device *spicc = dev_get_drvdata(dev);
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
		goto test_end2;
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
	m.spi = spi_alloc_device(spicc->controller);
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
	dev_info(dev, "test end @%d\n", spicc->effective_speed_hz);

test_end:
	meson_spicc_cleanup(m.spi);
	spi_dev_put(m.spi);
test_end2:
	kfree(kstr);
	kfree(tx_buf);
	kfree(rx_buf);
	return count;
}

static DEVICE_ATTR_WO(test);

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
	if (IS_ERR_OR_NULL(spicc->sys_clk)) {
		dev_err(&pdev->dev, "sys clock request failed\n");
		ret = PTR_ERR(spicc->sys_clk);
		goto out_controller;
	}
	ret = clk_prepare_enable(spicc->sys_clk);
	if (ret) {
		dev_err(&pdev->dev, "sys clock enable failed\n");
		goto out_controller;
	}

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
	ctlr->mode_bits = SPI_CPHA | SPI_CPOL | SPI_LSB_FIRST;
	ctlr->max_speed_hz = 100000000;
	ctlr->min_speed_hz = 1000000;
	ctlr->setup = meson_spicc_setup;
	ctlr->cleanup = meson_spicc_cleanup;
	ctlr->prepare_message = meson_spicc_prepare_message;
	ctlr->unprepare_transfer_hardware = meson_spicc_unprepare_transfer;
	ctlr->transfer_one_message = meson_spicc_transfer_one_message;

	ret = devm_spi_register_master(&pdev->dev, ctlr);
	if (ret) {
		dev_err(&pdev->dev, "spi controller registration failed\n");
		goto out_clk;
	}

	init_completion(&spicc->completion);

	ret = device_create_file(&pdev->dev, &dev_attr_test);
	if (ret)
		dev_warn(&pdev->dev, "Create test attribute failed\n");

	spicc->config_data_mode = SPICC_DATA_MODE_MEM;
	spicc->cfg_spi.d32 = 0;
	spicc->cfg_start.d32 = 0;
	spicc->cfg_bus.d32 = 0;

	spicc->cfg_spi.b.flash_wp_pin_en = 1;
	spicc->cfg_spi.b.flash_hold_pin_en = 1;
	/* default no eoc */
	spicc->cfg_start.b.eoc = 0;
	/* default pending */
	spicc->cfg_start.b.pending = 1;
	/* default keep ss */
	spicc->cfg_bus.b.keep_ss = 1;

	return 0;

out_clk:
	clk_disable_unprepare(spicc->sys_clk);
	clk_disable_unprepare(spicc->spi_clk);
out_controller:
	spi_controller_put(ctlr);

	return ret;
}

static int meson_spicc_remove(struct platform_device *pdev)
{
	struct spicc_device *spicc = platform_get_drvdata(pdev);

	clk_disable_unprepare(spicc->sys_clk);
	clk_disable_unprepare(spicc->spi_clk);
	device_remove_file(&pdev->dev, &dev_attr_test);

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
