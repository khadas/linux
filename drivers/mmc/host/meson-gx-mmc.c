// SPDX-License-Identifier: GPL-2.0-only
/*
 * Amlogic SD/eMMC driver for the GX/S905 family SoCs
 *
 * Copyright (c) 2016 BayLibre, SAS.
 * Author: Kevin Hilman <khilman@baylibre.com>
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/iopoll.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/dma-mapping.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/slot-gpio.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/regulator/consumer.h>
#include <linux/reset.h>
#include <linux/interrupt.h>
#include <linux/bitfield.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/aml_sd.h>

#define DRIVER_NAME "meson-gx-mmc"

#define SD_EMMC_CLOCK 0x0
#define   CLK_DIV_MASK GENMASK(5, 0)
#define   CLK_SRC_MASK GENMASK(7, 6)
#define   CLK_CORE_PHASE_MASK GENMASK(9, 8)
#define   CLK_TX_PHASE_MASK GENMASK(11, 10)
#define   CLK_RX_PHASE_MASK GENMASK(13, 12)
#define   CLK_PHASE_0 0
#define   CLK_PHASE_180 2
#define   CLK_V2_TX_DELAY_MASK GENMASK(19, 16)
#define   CLK_V2_RX_DELAY_MASK GENMASK(23, 20)
#define   CLK_V2_ALWAYS_ON BIT(24)

#define   CLK_V3_TX_DELAY_MASK GENMASK(21, 16)
#define   CLK_V3_RX_DELAY_MASK GENMASK(27, 22)
#define   CLK_V3_ALWAYS_ON BIT(28)
#define   CFG_IRQ_SDIO_SLEEP BIT(29)
#define   CFG_IRQ_SDIO_SLEEP_DS BIT(30)

#define   CLK_TX_DELAY_MASK(h)		(h->data->tx_delay_mask)
#define   CLK_RX_DELAY_MASK(h)		(h->data->rx_delay_mask)
#define   CLK_ALWAYS_ON(h)		(h->data->always_on)

#define SD_EMMC_DELAY 0x4
#define SD_EMMC_ADJUST 0x8
#define   ADJUST_ADJ_DELAY_MASK GENMASK(21, 16)
#define   ADJUST_DS_EN BIT(15)
#define   ADJUST_ADJ_EN BIT(13)

#define SD_EMMC_DELAY1 0x4
#define SD_EMMC_DELAY2 0x8
#define SD_EMMC_V3_ADJUST 0xc
#define   CFG_ADJUST_ENABLE BIT(13)
#define   CLK_ADJUST_DELAY GENMASK(21, 16)

#define SD_EMMC_CALOUT 0x10
#define SD_EMMC_ADJ_IDX_LOG 0x20
#define SD_EMMC_CLKTEST_LOG 0x24
#define   CLKTEST_TIMES_MASK GENMASK(30, 0)
#define   CLKTEST_DONE BIT(31)
#define SD_EMMC_CLKTEST_OUT 0x28
#define SD_EMMC_EYETEST_LOG 0x2c
#define   EYETEST_TIMES_MASK GENMASK(30, 0)
#define   EYETEST_DONE BIT(31)
#define SD_EMMC_EYETEST_OUT0 0x30
#define SD_EMMC_EYETEST_OUT1 0x34
#define SD_EMMC_INTF3 0x38
#define   CLKTEST_EXP_MASK GENMASK(4, 0)
#define   CLKTEST_ON_M BIT(5)
#define   EYETEST_EXP_MASK GENMASK(10, 6)
#define   EYETEST_ON BIT(11)
#define   DS_SHT_M_MASK GENMASK(17, 12)
#define   DS_SHT_EXP_MASK GENMASK(21, 18)
#define   SD_INTF3 BIT(22)
#define SD_EMMC_START 0x40
#define   START_DESC_INIT BIT(0)
#define   START_DESC_BUSY BIT(1)
#define   START_DESC_ADDR_MASK GENMASK(31, 2)

#define SD_EMMC_CFG 0x44
#define   CFG_BUS_WIDTH_MASK GENMASK(1, 0)
#define   CFG_BUS_WIDTH_1 0x0
#define   CFG_BUS_WIDTH_4 0x1
#define   CFG_BUS_WIDTH_8 0x2
#define   CFG_DDR BIT(2)
#define   CFG_BLK_LEN_MASK GENMASK(7, 4)
#define   CFG_RESP_TIMEOUT_MASK GENMASK(11, 8)
#define   CFG_RC_CC_MASK GENMASK(15, 12)
#define   CFG_STOP_CLOCK BIT(22)
#define   CFG_CLK_ALWAYS_ON BIT(18)
#define   CFG_CHK_DS BIT(20)
#define   CFG_AUTO_CLK BIT(23)
#define   CFG_ERR_ABORT BIT(27)

#define SD_EMMC_STATUS 0x48
#define   STATUS_BUSY BIT(31)
#define   STATUS_DESC_BUSY BIT(30)
#define   STATUS_DATI GENMASK(23, 16)

#define SD_EMMC_IRQ_EN 0x4c
#define   IRQ_RXD_ERR_MASK GENMASK(7, 0)
#define   IRQ_TXD_ERR BIT(8)
#define   IRQ_DESC_ERR BIT(9)
#define   IRQ_RESP_ERR BIT(10)
#define   IRQ_CRC_ERR \
	(IRQ_RXD_ERR_MASK | IRQ_TXD_ERR | IRQ_DESC_ERR | IRQ_RESP_ERR)
#define   IRQ_RESP_TIMEOUT BIT(11)
#define   IRQ_DESC_TIMEOUT BIT(12)
#define   IRQ_TIMEOUTS \
	(IRQ_RESP_TIMEOUT | IRQ_DESC_TIMEOUT)
#define   IRQ_END_OF_CHAIN BIT(13)
#define   IRQ_RESP_STATUS BIT(14)
#define   IRQ_SDIO BIT(15)
#define   IRQ_EN_MASK \
	(IRQ_CRC_ERR | IRQ_TIMEOUTS | IRQ_END_OF_CHAIN | IRQ_RESP_STATUS |\
	 IRQ_SDIO)

#define SD_EMMC_CMD_CFG 0x50
#define SD_EMMC_CMD_ARG 0x54
#define SD_EMMC_CMD_DAT 0x58
#define SD_EMMC_CMD_RSP 0x5c
#define SD_EMMC_CMD_RSP1 0x60
#define SD_EMMC_CMD_RSP2 0x64
#define SD_EMMC_CMD_RSP3 0x68

#define SD_EMMC_RXD 0x94
#define SD_EMMC_TXD 0x94
#define SD_EMMC_LAST_REG SD_EMMC_TXD

#define SD_EMMC_SRAM_DATA_BUF_LEN 1536
#define SD_EMMC_SRAM_DATA_BUF_OFF 0x200

#define SD_EMMC_CFG_BLK_SIZE 512 /* internal buffer max: 512 bytes */
#define SD_EMMC_CFG_RESP_TIMEOUT 256 /* in clock cycles */
#define SD_EMMC_CMD_TIMEOUT 1024 /* in ms */
#define SD_EMMC_CMD_TIMEOUT_DATA 4096 /* in ms */
#define SD_EMMC_CFG_CMD_GAP 16 /* in clock cycles */
#define SD_EMMC_DESC_BUF_LEN PAGE_SIZE

#define SD_EMMC_PRE_REQ_DONE BIT(0)
#define SD_EMMC_DESC_CHAIN_MODE BIT(1)

#define MUX_CLK_NUM_PARENTS 2

#define CMD_CFG_LENGTH_MASK GENMASK(8, 0)
#define CMD_CFG_BLOCK_MODE BIT(9)
#define CMD_CFG_R1B BIT(10)
#define CMD_CFG_END_OF_CHAIN BIT(11)
#define CMD_CFG_TIMEOUT_MASK GENMASK(15, 12)
#define CMD_CFG_NO_RESP BIT(16)
#define CMD_CFG_NO_CMD BIT(17)
#define CMD_CFG_DATA_IO BIT(18)
#define CMD_CFG_DATA_WR BIT(19)
#define CMD_CFG_RESP_NOCRC BIT(20)
#define CMD_CFG_RESP_128 BIT(21)
#define CMD_CFG_RESP_NUM BIT(22)
#define CMD_CFG_DATA_NUM BIT(23)
#define CMD_CFG_CMD_INDEX_MASK GENMASK(29, 24)
#define CMD_CFG_ERROR BIT(30)
#define CMD_CFG_OWNER BIT(31)

#define CMD_DATA_MASK GENMASK(31, 2)
#define CMD_DATA_BIG_ENDIAN BIT(1)
#define CMD_DATA_SRAM BIT(0)
#define CMD_RESP_MASK GENMASK(31, 1)
#define CMD_RESP_SRAM BIT(0)
#define EMMC_SDIO_CLOCK_FELD    0Xffff
#define CALI_HS_50M_ADJUST      0
#define ERROR   1
#define FIXED   2
#define RETUNING        3
static struct mmc_host *sdio_host;

static unsigned int meson_mmc_get_timeout_msecs(struct mmc_data *data)
{
	unsigned int timeout = data->timeout_ns / NSEC_PER_MSEC;

	if (!timeout)
		return SD_EMMC_CMD_TIMEOUT_DATA;

	timeout = roundup_pow_of_two(timeout);

	return min(timeout, 32768U); /* max. 2^15 ms */
}

static struct mmc_command *meson_mmc_get_next_command(struct mmc_command *cmd)
{
	if (cmd->opcode == MMC_SET_BLOCK_COUNT && !cmd->error)
		return cmd->mrq->cmd;
	else if (mmc_op_multi(cmd->opcode) &&
		 (!cmd->mrq->sbc || cmd->error || cmd->data->error))
		return cmd->mrq->stop;
	else
		return NULL;
}

static void meson_mmc_get_transfer_mode(struct mmc_host *mmc,
					struct mmc_request *mrq)
{
	struct meson_host *host = mmc_priv(mmc);
	struct mmc_data *data = mrq->data;
	struct scatterlist *sg;
	int i;
	bool use_desc_chain_mode = true;

	/*
	 * When Controller DMA cannot directly access DDR memory, disable
	 * support for Chain Mode to directly use the internal SRAM using
	 * the bounce buffer mode.
	 */
	if (host->dram_access_quirk)
		return;

	/*
	 * Broken SDIO with AP6255-based WiFi on Khadas VIM Pro has been
	 * reported. For some strange reason this occurs in descriptor
	 * chain mode only. So let's fall back to bounce buffer mode
	 * for command SD_IO_RW_EXTENDED.
	 */
	/*if (mrq->cmd->opcode == SD_IO_RW_EXTENDED)
	 *	return;
	 */

	for_each_sg(data->sg, sg, data->sg_len, i)
		/* check for 8 byte alignment */
		if (sg->offset & 7) {
			WARN_ONCE(1, "unaligned scatterlist buffer\n");
			use_desc_chain_mode = false;
			break;
		}

	if (use_desc_chain_mode)
		data->host_cookie |= SD_EMMC_DESC_CHAIN_MODE;
}

static inline bool meson_mmc_desc_chain_mode(const struct mmc_data *data)
{
	return data->host_cookie & SD_EMMC_DESC_CHAIN_MODE;
}

static inline bool meson_mmc_bounce_buf_read(const struct mmc_data *data)
{
	return data && data->flags & MMC_DATA_READ &&
	       !meson_mmc_desc_chain_mode(data);
}

static void meson_mmc_pre_req(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct mmc_data *data = mrq->data;

	if (!data)
		return;

	meson_mmc_get_transfer_mode(mmc, mrq);
	data->host_cookie |= SD_EMMC_PRE_REQ_DONE;

	if (!meson_mmc_desc_chain_mode(data))
		return;

	data->sg_count = dma_map_sg(mmc_dev(mmc), data->sg, data->sg_len,
                                   mmc_get_dma_dir(data));
	if (!data->sg_count)
		dev_err(mmc_dev(mmc), "dma_map_sg failed");
}

static void meson_mmc_post_req(struct mmc_host *mmc, struct mmc_request *mrq,
			       int err)
{
	struct mmc_data *data = mrq->data;

	if (data && meson_mmc_desc_chain_mode(data) && data->sg_count)
		dma_unmap_sg(mmc_dev(mmc), data->sg, data->sg_len,
			     mmc_get_dma_dir(data));
}

/*
 * Gating the clock on this controller is tricky.  It seems the mmc clock
 * is also used by the controller.  It may crash during some operation if the
 * clock is stopped.  The safest thing to do, whenever possible, is to keep
 * clock running at stop it at the pad using the pinmux.
 */
static void meson_mmc_clk_gate(struct meson_host *host)
{
	u32 cfg;

	if (host->pins_clk_gate) {
		pinctrl_select_state(host->pinctrl, host->pins_clk_gate);
	} else {
		/*
		 * If the pinmux is not provided - default to the classic and
		 * unsafe method
		 */
		cfg = readl(host->regs + SD_EMMC_CFG);
		cfg |= CFG_STOP_CLOCK;
		writel(cfg, host->regs + SD_EMMC_CFG);
	}
}

static void meson_mmc_clk_ungate(struct meson_host *host)
{
	u32 cfg;

	if (host->pins_clk_gate)
		pinctrl_select_state(host->pinctrl, host->pins_default);

	/* Make sure the clock is not stopped in the controller */
	cfg = readl(host->regs + SD_EMMC_CFG);
	cfg &= ~CFG_STOP_CLOCK;
	writel(cfg, host->regs + SD_EMMC_CFG);
}

static void meson_mmc_set_phase_delay(struct meson_host *host, u32 mask,
				      unsigned int phase)
{
	u32 val;

	val = readl(host->regs);
	val &= ~mask;
	val |= phase << __ffs(mask);
	writel(val, host->regs);
}

static int meson_mmc_clk_set(struct meson_host *host, unsigned long rate,
			     bool ddr)
{
	struct mmc_host *mmc = host->mmc;
	int ret;
	u32 cfg;

	/* Same request - bail-out */
	if (host->ddr == ddr && host->req_rate == rate)
		return 0;

	/* stop clock */
	meson_mmc_clk_gate(host);
	host->req_rate = 0;
	mmc->actual_clock = 0;

	/* return with clock being stopped */
	if (!rate)
		return 0;

	/* Stop the clock during rate change to avoid glitches */
	cfg = readl(host->regs + SD_EMMC_CFG);
	cfg |= CFG_STOP_CLOCK;
	writel(cfg, host->regs + SD_EMMC_CFG);

	if (ddr) {
		/* DDR modes require higher module clock */
		rate <<= 1;
		cfg |= CFG_DDR;
	} else {
		cfg &= ~CFG_DDR;
	}
	writel(cfg, host->regs + SD_EMMC_CFG);
	host->ddr = ddr;

	ret = clk_set_rate(host->mmc_clk, rate);
	if (ret) {
		dev_err(host->dev, "Unable to set cfg_div_clk to %lu. ret=%d\n",
			rate, ret);
		return ret;
	}

	host->req_rate = rate;
	mmc->actual_clock = clk_get_rate(host->mmc_clk);

	/* We should report the real output frequency of the controller */
	if (ddr) {
		host->req_rate >>= 1;
		mmc->actual_clock >>= 1;
	}

	dev_dbg(host->dev, "clk rate: %u Hz\n", mmc->actual_clock);
	if (rate != mmc->actual_clock)
		dev_dbg(host->dev, "requested rate was %lu\n", rate);

	/* (re)start clock */
	meson_mmc_clk_ungate(host);

	return 0;
}

/*
 * The SD/eMMC IP block has an internal mux and divider used for
 * generating the MMC clock.  Use the clock framework to create and
 * manage these clocks.
 */
static int meson_mmc_clk_init(struct meson_host *host)
{
	struct clk_init_data init;
	struct clk_mux *mux;
	struct clk_divider *div;
	char clk_name[32];
	int i, ret = 0;
	const char *mux_parent_names[MUX_CLK_NUM_PARENTS];
	const char *clk_parent[1];
	u32 clk_reg;

	/* init SD_EMMC_CLOCK to sane defaults w/min clock rate */
	clk_reg = CLK_ALWAYS_ON(host);
	clk_reg |= CLK_DIV_MASK;
	clk_reg |= FIELD_PREP(CLK_CORE_PHASE_MASK, CLK_PHASE_180);
	clk_reg |= FIELD_PREP(CLK_TX_PHASE_MASK, CLK_PHASE_0);
	clk_reg |= FIELD_PREP(CLK_RX_PHASE_MASK, CLK_PHASE_0);
	writel(clk_reg, host->regs + SD_EMMC_CLOCK);

	/* get the mux parents */
	for (i = 0; i < MUX_CLK_NUM_PARENTS; i++) {
		struct clk *clk;
		char name[16];

		snprintf(name, sizeof(name), "clkin%d", i);
		clk = devm_clk_get(host->dev, name);
		if (IS_ERR(clk)) {
			if (clk != ERR_PTR(-EPROBE_DEFER))
				dev_err(host->dev, "Missing clock %s\n", name);
			return PTR_ERR(clk);
		}

		mux_parent_names[i] = __clk_get_name(clk);
	}

	/* create the mux */
	mux = devm_kzalloc(host->dev, sizeof(*mux), GFP_KERNEL);
	if (!mux)
		return -ENOMEM;

	snprintf(clk_name, sizeof(clk_name), "%s#mux", dev_name(host->dev));
	init.name = clk_name;
	init.ops = &clk_mux_ops;
	init.flags = 0;
	init.parent_names = mux_parent_names;
	init.num_parents = MUX_CLK_NUM_PARENTS;

	mux->reg = host->regs + SD_EMMC_CLOCK;
	mux->shift = __ffs(CLK_SRC_MASK);
	mux->mask = CLK_SRC_MASK >> mux->shift;
	mux->hw.init = &init;

	host->mux_clk = devm_clk_register(host->dev, &mux->hw);
	if (WARN_ON(IS_ERR(host->mux_clk)))
		return PTR_ERR(host->mux_clk);

	/* create the divider */
	div = devm_kzalloc(host->dev, sizeof(*div), GFP_KERNEL);
	if (!div)
		return -ENOMEM;

	snprintf(clk_name, sizeof(clk_name), "%s#div", dev_name(host->dev));
	init.name = clk_name;
	init.ops = &clk_divider_ops;
	init.flags = CLK_SET_RATE_PARENT;
	clk_parent[0] = __clk_get_name(host->mux_clk);
	init.parent_names = clk_parent;
	init.num_parents = 1;

	div->reg = host->regs + SD_EMMC_CLOCK;
	div->shift = __ffs(CLK_DIV_MASK);
	div->width = __builtin_popcountl(CLK_DIV_MASK);
	div->hw.init = &init;
	div->flags = CLK_DIVIDER_ONE_BASED;

	host->mmc_clk = devm_clk_register(host->dev, &div->hw);
	if (WARN_ON(IS_ERR(host->mmc_clk)))
		return PTR_ERR(host->mmc_clk);

	/* init SD_EMMC_CLOCK to sane defaults w/min clock rate */
	host->mmc->f_min = clk_round_rate(host->mmc_clk, 400000);
	ret = clk_set_rate(host->mmc_clk, host->mmc->f_min);
	if (ret)
		return ret;

	return clk_prepare_enable(host->mmc_clk);
}

static unsigned int aml_sd_emmc_clktest(struct mmc_host *mmc)
{
	struct meson_host *host = mmc_priv(mmc);
	u32 intf3 = readl(host->regs + SD_EMMC_INTF3);
	u32 clktest = 0, delay_cell = 0, clktest_log = 0, count = 0;
	u32 vcfg = readl(host->regs + SD_EMMC_CFG);
	int i = 0;
	unsigned int cycle = 0;

	writel(0, (host->regs + SD_EMMC_V3_ADJUST));
	cycle = (1000000000 / mmc->actual_clock) * 1000;
	vcfg &= ~(1 << 23);
	writel(vcfg, host->regs + SD_EMMC_CFG);
	writel(0, host->regs + SD_EMMC_DELAY1);
	writel(0, host->regs + SD_EMMC_DELAY2);
	intf3 &= ~CLKTEST_EXP_MASK;
	intf3 |= 8 << __ffs(CLKTEST_EXP_MASK);
	intf3 |= CLKTEST_ON_M;
	writel(intf3, host->regs + SD_EMMC_INTF3);
	clktest_log = readl(host->regs + SD_EMMC_CLKTEST_LOG);
	clktest = readl(host->regs + SD_EMMC_CLKTEST_OUT);
	while (!(clktest_log & 0x80000000)) {
		mdelay(1);
		i++;
		clktest_log = readl(host->regs + SD_EMMC_CLKTEST_LOG);
		clktest = readl(host->regs + SD_EMMC_CLKTEST_OUT);
		if (i > 4) {
			pr_warn("[%s] [%d] emmc clktest error\n",
				__func__, __LINE__);
			break;
		}
	}
	if (clktest_log & 0x80000000) {
		clktest = readl(host->regs + SD_EMMC_CLKTEST_OUT);
		count = clktest / (1 << 8);
		if (vcfg & 0x4)
			delay_cell = ((cycle / 2) / count);
		else
			delay_cell = (cycle / count);
	}
	pr_info("%s [%d] clktest : %u, delay_cell: %d, count: %u\n",
		__func__, __LINE__, clktest, delay_cell, count);
	intf3 = readl(host->regs + SD_EMMC_INTF3);
	intf3 &= ~CLKTEST_ON_M;
	writel(intf3, host->regs + SD_EMMC_INTF3);
	vcfg = readl(host->regs + SD_EMMC_CFG);
	vcfg |= (1 << 23);
	writel(vcfg, host->regs + SD_EMMC_CFG);
	host->delay_cell = delay_cell;
	return count;
}

static int meson_mmc_set_adjust(struct mmc_host *mmc, u32 value)
{
	u32 val;
	struct meson_host *host = mmc_priv(mmc);

	val = readl(host->regs + SD_EMMC_V3_ADJUST);
	val &= ~CLK_ADJUST_DELAY;
	val &= ~CFG_ADJUST_ENABLE;
	val |= CFG_ADJUST_ENABLE;
	val |= value << __ffs(CLK_ADJUST_DELAY);

	writel(val, host->regs + SD_EMMC_V3_ADJUST);
	return 0;
}

static void meson_mmc_set_delay(struct mmc_host *mmc, u32 delay)
{
	u32 val;
	u32 onedelay1 = 1 + BIT(6) + BIT(12) + BIT(18) + BIT(24);
	u32 onedelay2 = 1 + BIT(6) + BIT(12) + BIT(24);
	struct meson_host *host = mmc_priv(mmc);

	val = delay * onedelay1;
	writel(val, host->regs + SD_EMMC_DELAY1);
	val = delay * onedelay2;
	writel(val, host->regs + SD_EMMC_DELAY2);
}

int meson_mmc_tuning_transfer(struct mmc_host *mmc, u32 opcode)
{
	int tuning_err = 0;
	int n, nmatch;
	/* try ntries */
	for (n = 0, nmatch = 0; n < 40; n++) {
		tuning_err = mmc_send_tuning(mmc, opcode, NULL);
		if (!tuning_err)
			nmatch++;
		else
			break;
	}
	return nmatch;
}

static int meson_mmc_delay_tuning(struct mmc_host *mmc, u32 delay,
				  u32 value, u32 opcode)
{
	int ret = 0;
	struct meson_host *host = mmc_priv(mmc);
	u8 clk_div = readl(host->regs + SD_EMMC_CLOCK) & CLK_DIV_MASK;

	value = value % clk_div;
	meson_mmc_set_adjust(mmc, value);
	meson_mmc_set_delay(mmc, delay);
	ret = meson_mmc_tuning_transfer(mmc, opcode);

	return ret;
}

static void meson_mmc_shift_map(unsigned long *map,
				unsigned long shift, u8 clk_div)
{
	unsigned long left[1];
	unsigned long right[1];

	/*
	 * shift the bitmap right and reintroduce the dropped bits on the left
	 * of the bitmap
	 */
	bitmap_shift_right(right, map, shift, clk_div);
	bitmap_shift_left(left, map, clk_div - shift,
			  clk_div);
	bitmap_or(map, left, right, clk_div);
}

static void meson_mmc_find_next_region(unsigned long *map,
				       unsigned long *start,
				       unsigned long *stop,
				       u8 clk_div)
{
	*start = find_next_bit(map, clk_div, *start);
	*stop = find_next_zero_bit(map, clk_div, *start);
}

static void meson_mmc_find_next_zero_region(unsigned long *map,
					    unsigned long *start,
					    unsigned long *stop,
					    u8 clk_div)
{
	*start = find_next_zero_bit(map, clk_div, *start);
	*stop = find_next_bit(map, clk_div, *start);
}

static int meson_mmc_is_hole(struct mmc_host *mmc, u32 delay,
			     u8 adjust, u32 opcode)
{
	int ret1, ret2, ret = 0;

	ret1 = meson_mmc_delay_tuning(mmc, delay, adjust, opcode);
	ret2 = meson_mmc_delay_tuning(mmc, delay + 1, adjust, opcode);
	if (ret1 == 40 && ret2 == 40)
		ret = 1;

	return ret;
}

static int meson_mmc_get_nok_point(struct meson_host *host,
				   unsigned long *test, u8 clk_div,
				   u32 opcode, u32 delay)
{
	unsigned long shift, stop, start = 0;
	struct mmc_host *mmc = host->mmc;
	int ret = 0, index = 0;

	shift = find_first_bit(test, clk_div);
	if (shift)
		meson_mmc_shift_map(test, shift, clk_div);

	while (start < clk_div) {
		meson_mmc_find_next_zero_region(test, &start, &stop, clk_div);
		host->hole[index].start = start + shift;
		host->hole[index].size = stop - start;
		if (host->debug_flag)
			dev_notice(host->dev, "hole start:%u,hole size:%u\n",
				   host->hole[index].start,
				   host->hole[index].size);
		start = stop;
		index++;
		if (find_next_zero_bit(test, clk_div, start) >= clk_div)
			break;
	}
	switch (index) {
	case 1:
		if (host->hole[0].size == 1) {
			ret = meson_mmc_is_hole(mmc, delay,
						host->hole[0].start + 1,
						opcode);
			if (ret) {
				host->fix_hole = host->hole[0].start;
				return RETUNING;
			}
		} else if (clk_div - host->hole[0].size == 2) {
			ret = meson_mmc_is_hole(mmc, delay,
						host->hole[0].start + 1,
						opcode);
			if (ret) {
				host->fix_hole = host->hole[0].start;
				return FIXED;
			}
			ret = meson_mmc_is_hole(mmc, delay,
						host->hole[0].start +
						host->hole[0].size,
						opcode);
			if (ret) {
				host->fix_hole = host->hole[0].start +
					host->hole[0].size - 1;
				return FIXED;
			}
		}
		return FIXED;
	case 2:
		if (host->hole[0].size == 1) {
			ret = meson_mmc_is_hole(mmc, delay,
						host->hole[0].start + 1,
						opcode);
			if (ret) {
				host->fix_hole = host->hole[0].start;
				return FIXED;
			}
		}
		if (host->hole[1].size == 1) {
			ret = meson_mmc_is_hole(mmc, delay,
						host->hole[0].start + 1,
						opcode);
			if (ret) {
				host->fix_hole = host->hole[1].start;
				return FIXED;
			}
		}
		return ERROR;
	default:
		return ERROR;
	}
}

static int meson_mmc_find_tuning_point(unsigned long *test, u8 clk_div)
{
	unsigned long shift, stop, offset = 0, start = 0, size = 0;

	shift = find_first_zero_bit(test, clk_div);
	if (shift != 0)
		meson_mmc_shift_map(test, shift, clk_div);

	while (start < clk_div) {
		meson_mmc_find_next_region(test, &start, &stop, clk_div);

		if ((stop - start) > size) {
			offset = start;
			size = stop - start;
		}

		start = stop;
	}
	pr_info("shift=%lu size=%lu\n", shift, size);
	/* Get the center point of the region */
	offset += (size / 2);

	/* Shift the result back */
	offset = (offset + shift) % clk_div;

	return offset;
}

static int meson_mmc_clk_phase_tuning(struct mmc_host *mmc, u32 opcode)
{
	int point, ret, need_fix_hole = 1;
	u32 val, old_dly1, old_dly2, delay_count = 0, print_val;
	u8 clk_div, delay = 0;
	unsigned long test[1], fix_test[1];
	struct meson_host *host = mmc_priv(mmc);

	old_dly1 = readl(host->regs + SD_EMMC_DELAY1);
	old_dly2 = readl(host->regs + SD_EMMC_DELAY2);
	if (host->fixadj_have_hole) {
		aml_sd_emmc_clktest(mmc);
		delay_count = 1000 / host->delay_cell;
	}
	host->fix_hole = 0xff;
tuning:
	print_val = 0;
	clk_div = readl(host->regs + SD_EMMC_CLOCK) & CLK_DIV_MASK;
	bitmap_zero(test, clk_div);

	/* Explore tuning points */
	for (point = 0; point < clk_div; point++) {
		meson_mmc_set_adjust(mmc, point);
		ret = meson_mmc_tuning_transfer(mmc, opcode);
		if (ret == 40) {
			set_bit(point, test);
			print_val |= 1 << (4 * point);
		}
	}

	if (host->fix_hole != 0xff) {
		set_bit(host->fix_hole, test);
		print_val |= 1 << (4 * host->fix_hole);
	}

	dev_notice(host->dev, "tuning result:%6x\n", print_val);

	if (bitmap_full(test, clk_div)) {
		dev_warn(host->dev,
			 "All the point tuning success,data1 add delay%d,continue tuning ...\n",
			 delay + 1);
		val = readl(host->regs + SD_EMMC_DELAY1);
		delay = (val >> 0x6) & 0x3F;
		if (++delay > 0x3F) {
			dev_err(host->dev, "Have no delay can add,tuning failed!\n");
			return -EINVAL;
		}
		meson_mmc_set_delay(mmc, delay);
		need_fix_hole = 0;
		goto tuning; /* All points are good so point 0 will do */
	} else if (bitmap_empty(test, clk_div)) {
		dev_warn(host->dev, "All point tuning failed!\n");
		return -EINVAL;/* No successful tuning point */
	}

	if (host->fixadj_have_hole && need_fix_hole) {
		if (host->debug_flag)
			dev_notice(host->dev, "hs200/sdr104 need fix fixadj hole\n");
		fix_test[0] = test[0];
		ret = meson_mmc_get_nok_point(host, fix_test, clk_div,
					      opcode, delay_count);

		switch (ret) {
		case RETUNING:
			if (host->debug_flag)
				dev_notice(host->dev, "Need retuning\n");
			need_fix_hole = 0;
			goto tuning;
		case FIXED:
			if (host->debug_flag)
				dev_notice(host->dev, "hole fixed!\n");
			if (host->fix_hole != 0xff) {
				dev_notice(host->dev, "find the hole,the hole is %u\n",
					   host->fix_hole);
				set_bit(host->fix_hole, test);
			}
			break;
		case ERROR:
			dev_warn(host->dev, "tuning failed!\n");
			return -EINVAL;
		}
	}

	/* Find the optimal tuning point and apply it */
	pr_info("delay=%u\n", delay);
	point = meson_mmc_find_tuning_point(test, clk_div);
	if (point < 0) {
		dev_err(host->dev,
			"The best fix adjust point is %d,tuning failed!\n",
			point);
		return point; /* tuning failed */
	}

	dev_notice(host->dev, "Tuning success! The best point is %d, clock is %ld\n",
		   point, clk_get_rate(host->mmc_clk));

	if (point == host->fix_hole) {
		if (host->debug_flag)
			dev_notice(host->dev, "%d is a hole,>>point:%d,delay:%u\n",
				   point, point + 1, delay_count);
		meson_mmc_set_adjust(mmc, point + 1);
		meson_mmc_set_delay(mmc, delay_count);
	} else {
		writel(old_dly1, host->regs + SD_EMMC_DELAY1);
		writel(old_dly2, host->regs + SD_EMMC_DELAY2);
		meson_mmc_set_adjust(mmc, point);
	}
	return 0;
}

static int meson_mmc_execute_tuning(struct mmc_host *mmc, u32 opcode)
{
	struct meson_host *host = mmc_priv(mmc);
	int err = 0;

	host->is_tuning = 1;
	err = meson_mmc_clk_phase_tuning(mmc, opcode);
	host->is_tuning = 0;

	return err;
}

static int meson_mmc_prepare_ios_clock(struct meson_host *host,
				       struct mmc_ios *ios)
{
	bool ddr;

	switch (ios->timing) {
	case MMC_TIMING_MMC_DDR52:
	case MMC_TIMING_UHS_DDR50:
		ddr = true;
		break;

	default:
		ddr = false;
		break;
	}

	return meson_mmc_clk_set(host, ios->clock, ddr);
}

static void meson_mmc_check_resampling(struct meson_host *host,
				       struct mmc_ios *ios)
{
	struct mmc_phase *mmc_phase_set;
	unsigned int val;

	switch (ios->timing) {
	case MMC_TIMING_MMC_HS:
	case MMC_TIMING_SD_HS:
		val = readl(host->regs + host->data->adjust);
		val |= CFG_ADJUST_ENABLE;
		val &= ~CLK_ADJUST_DELAY;
		val |= CALI_HS_50M_ADJUST << __ffs(CLK_ADJUST_DELAY);
		writel(val, host->regs + host->data->adjust);
		mmc_phase_set = &host->sdmmc.init;
		break;
	case MMC_TIMING_MMC_DDR52:
		mmc_phase_set = &host->sdmmc.init;
		break;
	default:
		mmc_phase_set = &host->sdmmc.init;
	}
	meson_mmc_set_phase_delay(host, CLK_CORE_PHASE_MASK,
				  mmc_phase_set->core_phase);
	meson_mmc_set_phase_delay(host, CLK_TX_PHASE_MASK,
				  mmc_phase_set->tx_phase);
	meson_mmc_set_phase_delay(host, CLK_TX_DELAY_MASK(host),
				  mmc_phase_set->tx_delay);
}

static void meson_mmc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct meson_host *host = mmc_priv(mmc);
	u32 bus_width, val;
	int err;

	/*
	 * GPIO regulator, only controls switching between 1v8 and
	 * 3v3, doesn't support MMC_POWER_OFF, MMC_POWER_ON.
	 */
	switch (ios->power_mode) {
	case MMC_POWER_OFF:
		if (!IS_ERR(mmc->supply.vmmc))
			mmc_regulator_set_ocr(mmc, mmc->supply.vmmc, 0);

		if (!IS_ERR(mmc->supply.vqmmc) && host->vqmmc_enabled) {
			regulator_disable(mmc->supply.vqmmc);
			host->vqmmc_enabled = false;
		}

		break;

	case MMC_POWER_UP:
		if (!IS_ERR(mmc->supply.vmmc))
			mmc_regulator_set_ocr(mmc, mmc->supply.vmmc, ios->vdd);

		break;

	case MMC_POWER_ON:
		if (!IS_ERR(mmc->supply.vqmmc) && !host->vqmmc_enabled) {
			int ret = regulator_enable(mmc->supply.vqmmc);

			if (ret < 0)
				dev_err(host->dev,
					"failed to enable vqmmc regulator\n");
			else
				host->vqmmc_enabled = true;
		}

		break;
	}

	/* Bus width */
	switch (ios->bus_width) {
	case MMC_BUS_WIDTH_1:
		bus_width = CFG_BUS_WIDTH_1;
		break;
	case MMC_BUS_WIDTH_4:
		bus_width = CFG_BUS_WIDTH_4;
		break;
	case MMC_BUS_WIDTH_8:
		bus_width = CFG_BUS_WIDTH_8;
		break;
	default:
		dev_err(host->dev, "Invalid ios->bus_width: %u.  Setting to 4.\n",
			ios->bus_width);
		bus_width = CFG_BUS_WIDTH_4;
	}

	val = readl(host->regs + SD_EMMC_CFG);
	val &= ~CFG_BUS_WIDTH_MASK;
	val |= FIELD_PREP(CFG_BUS_WIDTH_MASK, bus_width);
	writel(val, host->regs + SD_EMMC_CFG);

	meson_mmc_check_resampling(host, ios);
	err = meson_mmc_prepare_ios_clock(host, ios);
	if (err)
		dev_err(host->dev, "Failed to set clock: %d\n,", err);

	dev_dbg(host->dev, "SD_EMMC_CFG:  0x%08x\n", val);
}

static void aml_sd_emmc_check_sdio_irq(struct meson_host *host)
{
	u32 vstat = readl(host->regs + SD_EMMC_STATUS);

	if (host->sdio_irqen) {
		if (((vstat & IRQ_SDIO) || (!(vstat & (1 << 17)))) &&
		    host->mmc->sdio_irq_thread &&
		    (!atomic_read(&host->mmc->sdio_irq_thread_abort))) {
			/* pr_info("signalling irq 0x%x\n", vstat); */
			mmc_signal_sdio_irq(host->mmc);
		}
	}
}

static void meson_mmc_request_done(struct mmc_host *mmc,
				   struct mmc_request *mrq)
{
	struct meson_host *host = mmc_priv(mmc);

	host->cmd = NULL;
	if (host->needs_pre_post_req)
		meson_mmc_post_req(mmc, mrq, 0);
	aml_sd_emmc_check_sdio_irq(host);
	mmc_request_done(host->mmc, mrq);
}

static void meson_mmc_set_blksz(struct mmc_host *mmc, unsigned int blksz)
{
	struct meson_host *host = mmc_priv(mmc);
	u32 cfg, blksz_old;

	cfg = readl(host->regs + SD_EMMC_CFG);
	blksz_old = FIELD_GET(CFG_BLK_LEN_MASK, cfg);

	if (!is_power_of_2(blksz))
		dev_err(host->dev, "blksz %u is not a power of 2\n", blksz);

	blksz = ilog2(blksz);

	/* check if block-size matches, if not update */
	if (blksz == blksz_old)
		return;

	dev_dbg(host->dev, "%s: update blk_len %d -> %d\n", __func__,
		blksz_old, blksz);

	cfg &= ~CFG_BLK_LEN_MASK;
	cfg |= FIELD_PREP(CFG_BLK_LEN_MASK, blksz);
	writel(cfg, host->regs + SD_EMMC_CFG);
}

static void meson_mmc_set_response_bits(struct mmc_command *cmd, u32 *cmd_cfg)
{
	if (cmd->flags & MMC_RSP_PRESENT) {
		if (cmd->flags & MMC_RSP_136)
			*cmd_cfg |= CMD_CFG_RESP_128;
		*cmd_cfg |= CMD_CFG_RESP_NUM;

		if (!(cmd->flags & MMC_RSP_CRC))
			*cmd_cfg |= CMD_CFG_RESP_NOCRC;

		if (cmd->flags & MMC_RSP_BUSY)
			*cmd_cfg |= CMD_CFG_R1B;
	} else {
		*cmd_cfg |= CMD_CFG_NO_RESP;
	}
}

static void meson_mmc_desc_chain_transfer(struct mmc_host *mmc, u32 cmd_cfg)
{
	struct meson_host *host = mmc_priv(mmc);
	struct sd_emmc_desc *desc = host->descs;
	struct mmc_data *data = host->cmd->data;
	struct scatterlist *sg;
	u32 start;
	int i;

	if (data->flags & MMC_DATA_WRITE)
		cmd_cfg |= CMD_CFG_DATA_WR;

	if (data->blocks > 1) {
		cmd_cfg |= CMD_CFG_BLOCK_MODE;
		meson_mmc_set_blksz(mmc, data->blksz);
	}

	for_each_sg(data->sg, sg, data->sg_count, i) {
		unsigned int len = sg_dma_len(sg);

		if (data->blocks > 1)
			len /= data->blksz;

		desc[i].cmd_cfg = cmd_cfg;
		desc[i].cmd_cfg |= FIELD_PREP(CMD_CFG_LENGTH_MASK, len);
		if (i > 0)
			desc[i].cmd_cfg |= CMD_CFG_NO_CMD;
		desc[i].cmd_arg = host->cmd->arg;
		desc[i].cmd_resp = 0;
		desc[i].cmd_data = sg_dma_address(sg);
	}
	desc[data->sg_count - 1].cmd_cfg |= CMD_CFG_END_OF_CHAIN;

	dma_wmb(); /* ensure descriptor is written before kicked */
	start = host->descs_dma_addr | START_DESC_BUSY;
	writel(start, host->regs + SD_EMMC_START);
}

static int aml_cmd_invalid(struct mmc_host *mmc, struct mmc_request *mrq)
{
	mrq->cmd->error = -EINVAL;
	mmc_request_done(mmc, mrq);

	return -EINVAL;
}

int aml_check_unsupport_cmd(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct meson_host *host = mmc_priv(mmc);
	u32 opcode, arg;

	opcode = mrq->cmd->opcode;
	arg = mrq->cmd->arg;
	/* CMD3 means the first time initialized flow is running */
	if (opcode == 3)
		mmc->first_init_flag = false;

	if (mmc->caps & MMC_CAP_NONREMOVABLE) { /* nonremovable device */
		if (mmc->first_init_flag) { /* init for the first time */
				/* for 8189ETV needs ssdio reset when starts */
			if (aml_card_type_sdio(host)) {
				/* if (opcode == SD_IO_RW_DIRECT
				 * || opcode == SD_IO_RW_EXTENDED
				 * || opcode == SD_SEND_IF_COND)
				 * return aml_cmd_invalid(mmc, mrq);
				 */
				return 0;
			} else if (aml_card_type_mmc(host)) {
				if (opcode == SD_IO_SEND_OP_COND ||
				    opcode == SD_IO_RW_DIRECT ||
				    opcode == SD_IO_RW_EXTENDED ||
				    opcode == SD_SEND_IF_COND ||
				    opcode == MMC_APP_CMD)
					return aml_cmd_invalid(mmc, mrq);
			} else if (aml_card_type_sd(host) ||
				   aml_card_type_non_sdio(host)) {
				if (opcode == SD_IO_SEND_OP_COND ||
				    opcode == SD_IO_RW_DIRECT ||
				    opcode == SD_IO_RW_EXTENDED)
					return aml_cmd_invalid(mmc, mrq);
			}
		}
	} else { /* removable device */
		/* filter cmd 5/52/53 for a non-sdio device */
		if (!aml_card_type_sdio(host) &&
		    !aml_card_type_unknown(host)) {
			if (opcode == SD_IO_SEND_OP_COND ||
			    opcode == SD_IO_RW_DIRECT ||
			    opcode == SD_IO_RW_EXTENDED)
				return aml_cmd_invalid(mmc, mrq);
		}
	}
	return 0;
}

static void meson_mmc_start_cmd(struct mmc_host *mmc, struct mmc_command *cmd)
{
	struct meson_host *host = mmc_priv(mmc);
	struct mmc_data *data = cmd->data;
	u32 val, cmd_cfg = 0, cmd_data = 0;
	unsigned int xfer_bytes = 0;

	/* Setup descriptors */
	dma_rmb();

	host->cmd = cmd;

	cmd_cfg |= FIELD_PREP(CMD_CFG_CMD_INDEX_MASK, cmd->opcode);
	cmd_cfg |= CMD_CFG_OWNER;  /* owned by CPU */
	cmd_cfg |= CMD_CFG_ERROR; /* stop in case of error */

	meson_mmc_set_response_bits(cmd, &cmd_cfg);

	if (cmd->opcode == SD_SWITCH_VOLTAGE) {
		val = readl(host->regs + SD_EMMC_CFG);
		val &= ~CFG_AUTO_CLK;
		writel(val, host->regs + SD_EMMC_CFG);
	}

	/* data? */
	if (data) {
		data->bytes_xfered = 0;
		cmd_cfg |= CMD_CFG_DATA_IO;
		cmd_cfg |= FIELD_PREP(CMD_CFG_TIMEOUT_MASK,
				      ilog2(meson_mmc_get_timeout_msecs(data)));

		if (meson_mmc_desc_chain_mode(data)) {
			meson_mmc_desc_chain_transfer(mmc, cmd_cfg);
			return;
		}

		if (data->blocks > 1) {
			cmd_cfg |= CMD_CFG_BLOCK_MODE;
			cmd_cfg |= FIELD_PREP(CMD_CFG_LENGTH_MASK,
					      data->blocks);
			meson_mmc_set_blksz(mmc, data->blksz);
		} else {
			cmd_cfg |= FIELD_PREP(CMD_CFG_LENGTH_MASK, data->blksz);
		}

		xfer_bytes = data->blksz * data->blocks;
		if (data->flags & MMC_DATA_WRITE) {
			cmd_cfg |= CMD_CFG_DATA_WR;
			WARN_ON(xfer_bytes > host->bounce_buf_size);
			sg_copy_to_buffer(data->sg, data->sg_len,
					  host->bounce_buf, xfer_bytes);
			dma_wmb();
		}

		cmd_data = host->bounce_dma_addr & CMD_DATA_MASK;
	} else {
		cmd_cfg |= FIELD_PREP(CMD_CFG_TIMEOUT_MASK,
				      ilog2(SD_EMMC_CMD_TIMEOUT));
	}

	/* Last descriptor */
	cmd_cfg |= CMD_CFG_END_OF_CHAIN;
	writel(cmd_cfg, host->regs + SD_EMMC_CMD_CFG);
	writel(cmd_data, host->regs + SD_EMMC_CMD_DAT);
	writel(0, host->regs + SD_EMMC_CMD_RSP);
	wmb(); /* ensure descriptor is written before kicked */
	writel(cmd->arg, host->regs + SD_EMMC_CMD_ARG);
}

static void meson_mmc_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct meson_host *host = mmc_priv(mmc);

	if (aml_check_unsupport_cmd(mmc, mrq))
		return;
	host->needs_pre_post_req = mrq->data &&
			!(mrq->data->host_cookie & SD_EMMC_PRE_REQ_DONE);

	if (host->needs_pre_post_req) {
		meson_mmc_get_transfer_mode(mmc, mrq);
		if (!meson_mmc_desc_chain_mode(mrq->data))
			host->needs_pre_post_req = false;
	}

	if (host->needs_pre_post_req)
		meson_mmc_pre_req(mmc, mrq);

	/* Stop execution */
	writel(0, host->regs + SD_EMMC_START);

	meson_mmc_start_cmd(mmc, mrq->sbc ?: mrq->cmd);
}

static void aml_sd_emmc_enable_sdio_irq(struct mmc_host *mmc, int enable)
{
	struct meson_host *host = mmc_priv(mmc);
	u32 vclkc = 0, virqc = 0;

	host->sdio_irqen = enable;

	virqc = readl(host->regs + SD_EMMC_IRQ_EN);
	virqc &= ~IRQ_SDIO;
	if (enable)
		virqc |= IRQ_SDIO;
	writel(virqc, host->regs + SD_EMMC_IRQ_EN);

	if (!host->irq_sdio_sleep) {
		vclkc = readl(host->regs + SD_EMMC_CLOCK);
		vclkc |= CFG_IRQ_SDIO_SLEEP;
		vclkc &= ~CFG_IRQ_SDIO_SLEEP_DS;
		writel(vclkc, host->regs + SD_EMMC_CLOCK);
		host->irq_sdio_sleep = 1;
	}

	/* check if irq already occurred */
	aml_sd_emmc_check_sdio_irq(host);
}

static void meson_mmc_read_resp(struct mmc_host *mmc, struct mmc_command *cmd)
{
	struct meson_host *host = mmc_priv(mmc);

	if (cmd->flags & MMC_RSP_136) {
		cmd->resp[0] = readl(host->regs + SD_EMMC_CMD_RSP3);
		cmd->resp[1] = readl(host->regs + SD_EMMC_CMD_RSP2);
		cmd->resp[2] = readl(host->regs + SD_EMMC_CMD_RSP1);
		cmd->resp[3] = readl(host->regs + SD_EMMC_CMD_RSP);
	} else if (cmd->flags & MMC_RSP_PRESENT) {
		cmd->resp[0] = readl(host->regs + SD_EMMC_CMD_RSP);
	}
}

static irqreturn_t meson_mmc_irq(int irq, void *dev_id)
{
	struct meson_host *host = dev_id;
	struct mmc_command *cmd;
	struct mmc_data *data;
	u32 irq_en, status, raw_status;
	irqreturn_t ret = IRQ_NONE;

	if (WARN_ON(!host))
		return IRQ_NONE;

	irq_en = readl(host->regs + SD_EMMC_IRQ_EN);
	raw_status = readl(host->regs + SD_EMMC_STATUS);
	status = raw_status & irq_en;

	if (status & IRQ_SDIO) {
		if (host->mmc->sdio_irq_thread &&
		    (!atomic_read(&host->mmc->sdio_irq_thread_abort))) {
			mmc_signal_sdio_irq(host->mmc);
			if (!(status & 0x3fff))
				return IRQ_HANDLED;
		}
	} else if (!(status & 0x3fff)) {
		return IRQ_HANDLED;
	}

	cmd = host->cmd;
	data = cmd->data;
	if (WARN_ON(!host->cmd)) {
		dev_err(host->dev, "host->cmd is NULL.\n");
		return IRQ_HANDLED;
	}

	cmd->error = 0;
	if (status & IRQ_CRC_ERR) {
		if (!host->is_tuning)
			dev_err(host->dev, "%d [0x%x], CRC[0x%04x]\n",
				cmd->opcode, cmd->arg, status);
		if (host->debug_flag && !host->is_tuning) {
			dev_notice(host->dev, "clktree : 0x%x,host_clock: 0x%x\n",
				   readl(host->clk_tree_base),
				   readl(host->regs));
			dev_notice(host->dev, "adjust: 0x%x,cfg: 0x%x,intf3: 0x%x\n",
				   readl(host->regs + SD_EMMC_V3_ADJUST),
				   readl(host->regs + SD_EMMC_CFG),
				   readl(host->regs + SD_EMMC_INTF3));
			dev_notice(host->dev, "delay1: 0x%x,delay2: 0x%x\n",
				   readl(host->regs + SD_EMMC_DELAY1),
				   readl(host->regs + SD_EMMC_DELAY2));
			dev_notice(host->dev, "pinmux: 0x%x\n",
				   readl(host->pin_mux_base));
		}
		cmd->error = -EILSEQ;
		ret = IRQ_WAKE_THREAD;
		goto out;
	}

	if (status & IRQ_TIMEOUTS) {
		if (!host->is_tuning)
			dev_err(host->dev, "%d [0x%x], TIMEOUT[0x%04x]\n",
				cmd->opcode, cmd->arg, status);
		if (host->debug_flag && !host->is_tuning) {
			dev_notice(host->dev, "clktree : 0x%x,host_clock: 0x%x\n",
				   readl(host->clk_tree_base),
				   readl(host->regs));
			dev_notice(host->dev, "adjust: 0x%x,cfg: 0x%x,intf3: 0x%x\n",
				   readl(host->regs + SD_EMMC_V3_ADJUST),
				   readl(host->regs + SD_EMMC_CFG),
				   readl(host->regs + SD_EMMC_INTF3));
			dev_notice(host->dev, "delay1: 0x%x,delay2: 0x%x\n",
				   readl(host->regs + SD_EMMC_DELAY1),
				   readl(host->regs + SD_EMMC_DELAY2));
			dev_notice(host->dev, "pinmux: 0x%x\n",
				   readl(host->pin_mux_base));
		}
		cmd->error = -ETIMEDOUT;
		ret = IRQ_WAKE_THREAD;
		goto out;
	}

	meson_mmc_read_resp(host->mmc, cmd);

	if (status & IRQ_SDIO) {
		dev_dbg(host->dev, "IRQ: SDIO TODO.\n");
		ret = IRQ_HANDLED;
	}

	if (status & (IRQ_END_OF_CHAIN | IRQ_RESP_STATUS)) {
		if (data && !cmd->error)
			data->bytes_xfered = data->blksz * data->blocks;
		if (meson_mmc_bounce_buf_read(data) ||
		    meson_mmc_get_next_command(cmd))
			ret = IRQ_WAKE_THREAD;
		else
			ret = IRQ_HANDLED;
	}

out:
	/* ack all raised interrupts */
	writel(0x7fff, host->regs + SD_EMMC_STATUS);
	if (cmd->error) {
		/* Stop desc in case of errors */
		u32 start = readl(host->regs + SD_EMMC_START);

		start &= ~START_DESC_BUSY;
		writel(start, host->regs + SD_EMMC_START);
	}

	if (ret == IRQ_HANDLED)
		meson_mmc_request_done(host->mmc, cmd->mrq);

	return ret;
}

static int meson_mmc_wait_desc_stop(struct meson_host *host)
{
	u32 status;

	/*
	 * It may sometimes take a while for it to actually halt. Here, we
	 * are giving it 5ms to comply
	 *
	 * If we don't confirm the descriptor is stopped, it might raise new
	 * IRQs after we have called mmc_request_done() which is bad.
	 */

	return readl_poll_timeout(host->regs + SD_EMMC_STATUS, status,
				  !(status & (STATUS_BUSY | STATUS_DESC_BUSY)),
				  100, 5000);
}

static irqreturn_t meson_mmc_irq_thread(int irq, void *dev_id)
{
	struct meson_host *host = dev_id;
	struct mmc_command *next_cmd, *cmd = host->cmd;
	struct mmc_data *data;
	unsigned int xfer_bytes;

	if (WARN_ON(!cmd))
		return IRQ_NONE;

	if (cmd->error) {
		meson_mmc_wait_desc_stop(host);
		meson_mmc_request_done(host->mmc, cmd->mrq);

		return IRQ_HANDLED;
	}

	data = cmd->data;
	if (meson_mmc_bounce_buf_read(data)) {
		xfer_bytes = data->blksz * data->blocks;
		WARN_ON(xfer_bytes > host->bounce_buf_size);
		sg_copy_from_buffer(data->sg, data->sg_len,
				    host->bounce_buf, xfer_bytes);
	}

	next_cmd = meson_mmc_get_next_command(cmd);
	if (next_cmd)
		meson_mmc_start_cmd(host->mmc, next_cmd);
	else
		meson_mmc_request_done(host->mmc, cmd->mrq);

	return IRQ_HANDLED;
}

/*
 * NOTE: we only need this until the GPIO/pinctrl driver can handle
 * interrupts.  For now, the MMC core will use this for polling.
 */
static int meson_mmc_get_cd(struct mmc_host *mmc)
{
	int status = mmc_gpio_get_cd(mmc);

	if (status == -ENOSYS)
		return 1; /* assume present */

	return status;
}

static void meson_mmc_cfg_init(struct meson_host *host)
{
	u32 cfg = 0;

	cfg |= FIELD_PREP(CFG_RESP_TIMEOUT_MASK,
			  ilog2(SD_EMMC_CFG_RESP_TIMEOUT));
	cfg |= FIELD_PREP(CFG_RC_CC_MASK, ilog2(SD_EMMC_CFG_CMD_GAP));
	cfg |= FIELD_PREP(CFG_BLK_LEN_MASK, ilog2(SD_EMMC_CFG_BLK_SIZE));

	/* abort chain on R/W errors */
	cfg |= CFG_ERR_ABORT;

	writel(cfg, host->regs + SD_EMMC_CFG);
}

static int meson_mmc_card_busy(struct mmc_host *mmc)
{
	struct meson_host *host = mmc_priv(mmc);
	u32 regval, val;

	regval = readl(host->regs + SD_EMMC_STATUS);
	if (!aml_card_type_mmc(host) && host->sd_sdio_switch_volat_done) {
		val = readl(host->regs + SD_EMMC_CFG);
		val |= CFG_AUTO_CLK;
		writel(val, host->regs + SD_EMMC_CFG);
		host->sd_sdio_switch_volat_done = 0;
	}

	/* We are only interrested in lines 0 to 3, so mask the other ones */
	return !(FIELD_GET(STATUS_DATI, regval) & 0xf);
}

static int meson_mmc_voltage_switch(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct meson_host *host = mmc_priv(mmc);
	int err;

	/* vqmmc regulator is available */
	if (!IS_ERR(mmc->supply.vqmmc)) {
		/*
		 * The usual amlogic setup uses a GPIO to switch from one
		 * regulator to the other. While the voltage ramp up is
		 * pretty fast, care must be taken when switching from 3.3v
		 * to 1.8v. Please make sure the regulator framework is aware
		 * of your own regulator constraints
		 */
		err = mmc_regulator_set_vqmmc(mmc, ios);

		if (!err && ios->signal_voltage == MMC_SIGNAL_VOLTAGE_180)
			host->sd_sdio_switch_volat_done = 1;

		return err;
	}

	/* no vqmmc regulator, assume fixed regulator at 3/3.3V */
	if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_330)
		return 0;

	return -EINVAL;
}

static void sdio_rescan(struct mmc_host *mmc)
{
	int ret;

	mmc->rescan_entered = 0;
/*	mmc->host_rescan_disable = false;*/
	mmc_detect_change(mmc, 0);
	/* start the delayed_work */
	ret = flush_work(&mmc->detect.work);
	/* wait for the delayed_work to finish */
	if (!ret)
		pr_info("Error: delayed_work mmc_rescan() already idle!\n");
}

void sdio_reinit(void)
{
	if (sdio_host) {
		if (sdio_host->card)
			sdio_reset_comm(sdio_host->card);
		else
			sdio_rescan(sdio_host);
	} else {
		pr_info("Error: sdio_host is NULL\n");
	}

	pr_info("[%s] finish\n", __func__);
}
EXPORT_SYMBOL(sdio_reinit);

/*this function tells wifi is using sd(sdiob) or sdio(sdioa)*/
const char *get_wifi_inf(void)
{
	if (sdio_host)
		return mmc_hostname(sdio_host);
	else
		return "sdio";
}
EXPORT_SYMBOL(get_wifi_inf);

static const struct mmc_host_ops meson_mmc_ops = {
	.request	= meson_mmc_request,
	.set_ios	= meson_mmc_set_ios,
	.enable_sdio_irq = aml_sd_emmc_enable_sdio_irq,
	.get_cd         = meson_mmc_get_cd,
	.pre_req	= meson_mmc_pre_req,
	.post_req	= meson_mmc_post_req,
	.execute_tuning = meson_mmc_execute_tuning,
	.card_busy	= meson_mmc_card_busy,
	.start_signal_voltage_switch = meson_mmc_voltage_switch,
};

static int meson_mmc_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct meson_host *host;
	struct mmc_host *mmc;
	int ret;
	u32 val;

	mmc = mmc_alloc_host(sizeof(struct meson_host), &pdev->dev);
	if (!mmc)
		return -ENOMEM;
	host = mmc_priv(mmc);
	host->mmc = mmc;
	host->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, host);

	/* The G12A SDIO Controller needs an SRAM bounce buffer */
	host->dram_access_quirk = device_property_read_bool(&pdev->dev,
					"amlogic,dram-access-quirk");

	/* Get regulators and the supported OCR mask */
	host->vqmmc_enabled = false;
	ret = mmc_regulator_get_supply(mmc);
	if (ret) {
		dev_warn(&pdev->dev, "power regulator get failed!\n");
		goto free_host;
	}

	ret = mmc_of_parse(mmc);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_warn(&pdev->dev, "error parsing DT: %d\n", ret);
		goto free_host;
	}

	device_property_read_u32(&pdev->dev, "card_type",
				 &host->card_type);

	host->data = (struct meson_mmc_data *)
		of_device_get_match_data(&pdev->dev);
	if (!host->data) {
		ret = -EINVAL;
		goto free_host;
	}

	ret = device_reset_optional(&pdev->dev);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "device reset failed: %d\n", ret);

		return ret;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	host->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(host->regs)) {
		ret = PTR_ERR(host->regs);
		goto free_host;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	host->clk_tree_base = ioremap(res->start, resource_size(res));
	if (IS_ERR(host->clk_tree_base)) {
		ret = PTR_ERR(host->clk_tree_base);
		goto free_host;
	}

	val = readl(host->clk_tree_base);
	if (aml_card_type_non_sdio(host))
		val &= EMMC_SDIO_CLOCK_FELD;
	else
		val &= ~EMMC_SDIO_CLOCK_FELD;
	writel(val, host->clk_tree_base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	host->pin_mux_base = ioremap(res->start, resource_size(res));
	if (IS_ERR(host->pin_mux_base)) {
		ret = PTR_ERR(host->pin_mux_base);
		goto free_host;
	}

	host->irq = platform_get_irq(pdev, 0);
	if (host->irq <= 0) {
		ret = -EINVAL;
		goto free_host;
	}

	host->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(host->pinctrl)) {
		ret = PTR_ERR(host->pinctrl);
		goto free_host;
	}

	host->pins_default = pinctrl_lookup_state(host->pinctrl,
						  PINCTRL_STATE_DEFAULT);
	if (IS_ERR(host->pins_default)) {
		ret = PTR_ERR(host->pins_default);
		goto free_host;
	}

	host->pins_clk_gate = pinctrl_lookup_state(host->pinctrl,
						   "clk-gate");
	if (IS_ERR(host->pins_clk_gate)) {
		dev_warn(&pdev->dev,
			 "can't get clk-gate pinctrl, using clk_stop bit\n");
		host->pins_clk_gate = NULL;
	}

	host->core_clk = devm_clk_get(&pdev->dev, "core");
	if (IS_ERR(host->core_clk)) {
		ret = PTR_ERR(host->core_clk);
		goto free_host;
	}

	ret = clk_prepare_enable(host->core_clk);
	if (ret)
		goto free_host;

	ret = meson_mmc_clk_init(host);
	if (ret)
		goto err_core_clk;

	/* set config to sane default */
	meson_mmc_cfg_init(host);

	/* Stop execution */
	writel(0, host->regs + SD_EMMC_START);

	/* clear, ack and enable interrupts */
	writel(0, host->regs + SD_EMMC_IRQ_EN);
	writel(IRQ_CRC_ERR | IRQ_TIMEOUTS | IRQ_END_OF_CHAIN,
	       host->regs + SD_EMMC_STATUS);
	writel(IRQ_CRC_ERR | IRQ_TIMEOUTS | IRQ_END_OF_CHAIN,
	       host->regs + SD_EMMC_IRQ_EN);

	ret = request_threaded_irq(host->irq, meson_mmc_irq,
				   meson_mmc_irq_thread, IRQF_ONESHOT,
				   dev_name(&pdev->dev), host);
	if (ret)
		goto err_init_clk;
	if (aml_card_type_mmc(host))
		mmc->caps |= MMC_CAP_CMD23;
	if (host->dram_access_quirk) {
		/* Limit to the available sram memory */
		mmc->max_segs = SD_EMMC_SRAM_DATA_BUF_LEN / mmc->max_blk_size;
		mmc->max_blk_count = mmc->max_segs;
	} else {
		mmc->max_blk_count = CMD_CFG_LENGTH_MASK;
		mmc->max_segs = SD_EMMC_DESC_BUF_LEN /
				sizeof(struct sd_emmc_desc);
	}
	mmc->max_req_size = mmc->max_blk_count * mmc->max_blk_size;
	mmc->max_seg_size = mmc->max_req_size;

	/*
	 * At the moment, we don't know how to reliably enable HS400.
	 * From the different datasheets, it is not even clear if this mode
	 * is officially supported by any of the SoCs
	 */
	mmc->caps2 &= ~MMC_CAP2_HS400;

	if (host->dram_access_quirk) {
		/*
		 * The MMC Controller embeds 1,5KiB of internal SRAM
		 * that can be used to be used as bounce buffer.
		 * In the case of the G12A SDIO controller, use these
		 * instead of the DDR memory
		 */
		host->bounce_buf_size = SD_EMMC_SRAM_DATA_BUF_LEN;
		host->bounce_buf = host->regs + SD_EMMC_SRAM_DATA_BUF_OFF;
		host->bounce_dma_addr = res->start + SD_EMMC_SRAM_DATA_BUF_OFF;
	} else {
		/* data bounce buffer */
		host->bounce_buf_size = mmc->max_req_size;
		host->bounce_buf =
			dma_alloc_coherent(host->dev, host->bounce_buf_size,
					   &host->bounce_dma_addr, GFP_KERNEL);
		if (host->bounce_buf == NULL) {
			dev_err(host->dev, "Unable to map allocate DMA bounce buffer.\n");
			ret = -ENOMEM;
			goto err_free_irq;
		}
	}

	host->descs = dma_alloc_coherent(host->dev, SD_EMMC_DESC_BUF_LEN,
		      &host->descs_dma_addr, GFP_KERNEL);
	if (!host->descs) {
		dev_err(host->dev, "Allocating descriptor DMA buffer failed\n");
		ret = -ENOMEM;
		goto err_bounce_buf;
	}

	if (aml_card_type_sdio(host)) {
		/* do NOT run mmc_rescan for the first time */
		mmc->rescan_entered = 1;
	} else {
		mmc->rescan_entered = 0;
	}

	mmc->ops = &meson_mmc_ops;
	mmc_add_host(mmc);

	if (aml_card_type_sdio(host)) {/* if sdio_wifi */
		sdio_host = mmc;
	}

	dev_notice(host->dev, "host probe success!\n");
	return 0;

err_bounce_buf:
	if (!host->dram_access_quirk)
		dma_free_coherent(host->dev, host->bounce_buf_size,
				  host->bounce_buf, host->bounce_dma_addr);
err_free_irq:
	free_irq(host->irq, host);
err_init_clk:
	clk_disable_unprepare(host->mmc_clk);
err_core_clk:
	clk_disable_unprepare(host->core_clk);
free_host:
	mmc_free_host(mmc);
	dev_notice(host->dev, "host probe failed!\n");
	return ret;
}

static int meson_mmc_remove(struct platform_device *pdev)
{
	struct meson_host *host = dev_get_drvdata(&pdev->dev);

	mmc_remove_host(host->mmc);

	/* disable interrupts */
	writel(0, host->regs + SD_EMMC_IRQ_EN);
	free_irq(host->irq, host);

	dma_free_coherent(host->dev, SD_EMMC_DESC_BUF_LEN,
			  host->descs, host->descs_dma_addr);

	if (!host->dram_access_quirk)
		dma_free_coherent(host->dev, host->bounce_buf_size,
				  host->bounce_buf, host->bounce_dma_addr);

	clk_disable_unprepare(host->mmc_clk);
	clk_disable_unprepare(host->core_clk);

	mmc_free_host(host->mmc);
	return 0;
}

static const struct meson_mmc_data meson_gx_data = {
	.tx_delay_mask	= CLK_V2_TX_DELAY_MASK,
	.rx_delay_mask	= CLK_V2_RX_DELAY_MASK,
	.always_on	= CLK_V2_ALWAYS_ON,
	.adjust		= SD_EMMC_ADJUST,
};

static const struct meson_mmc_data meson_axg_data = {
	.tx_delay_mask	= CLK_V3_TX_DELAY_MASK,
	.rx_delay_mask	= CLK_V3_RX_DELAY_MASK,
	.always_on	= CLK_V3_ALWAYS_ON,
	.adjust		= SD_EMMC_V3_ADJUST,
};

static const struct of_device_id meson_mmc_of_match[] = {
	{ .compatible = "amlogic,meson-gx-mmc",		.data = &meson_gx_data },
	{ .compatible = "amlogic,meson-gxbb-mmc", 	.data = &meson_gx_data },
	{ .compatible = "amlogic,meson-gxl-mmc",	.data = &meson_gx_data },
	{ .compatible = "amlogic,meson-gxm-mmc",	.data = &meson_gx_data },
	{ .compatible = "amlogic,meson-axg-mmc",	.data = &meson_axg_data },
	{}
};
MODULE_DEVICE_TABLE(of, meson_mmc_of_match);

static struct platform_driver meson_mmc_driver = {
	.probe		= meson_mmc_probe,
	.remove		= meson_mmc_remove,
	.driver		= {
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr(meson_mmc_of_match),
	},
};

module_platform_driver(meson_mmc_driver);

MODULE_DESCRIPTION("Amlogic S905*/GX*/AXG SD/eMMC driver");
MODULE_AUTHOR("Kevin Hilman <khilman@baylibre.com>");
MODULE_LICENSE("GPL v2");
