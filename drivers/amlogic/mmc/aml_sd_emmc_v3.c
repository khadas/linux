#include <linux/module.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/timer.h>
#include <linux/clk.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/io.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/sdio.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/amlogic/sd.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/mmc/emmc_partitions.h>
#include <../drivers/mmc/core/mmc_ops.h>
#include "amlsd.h"
#include "aml_sd_emmc_internal.h"
#define CALI_BLK_CNT    80
static struct mmc_claim aml_sd_emmc_claim;
void aml_sd_emmc_reg_init_v3(struct amlsd_host *host)
{
	u32 vclkc = 0;
	struct sd_emmc_clock_v3 *pclkc = (struct sd_emmc_clock_v3 *)&vclkc;
	u32 vconf = 0;
	struct sd_emmc_config *pconf = (struct sd_emmc_config *)&vconf;
	struct sd_emmc_regs_v3 *sd_emmc_regs =
		(struct sd_emmc_regs_v3 *)host->sd_emmc_regs;

	pr_info("%s %d\n", __func__, __LINE__);

	/* clear controller's main register setting which set in uboot*/
	sd_emmc_regs->gdelay1 = 0;
	sd_emmc_regs->gdelay2 = 0;
	sd_emmc_regs->gadjust = 0;
	sd_emmc_regs->gclock = 0;
	sd_emmc_regs->gcfg = 0;

	vclkc = 0;
	pclkc->div = 60;	 /* 400KHz */
	pclkc->src = 0;	  /* 0: Crystal 24MHz */
	pclkc->core_phase = 3;	  /* 2: 180 phase */
	pclkc->rx_phase = 0;
	pclkc->tx_phase = 0;
	/*pclkc->tx_delay = 20;*/
	pclkc->always_on = 1;	  /* Keep clock always on */
	sd_emmc_regs->gclock = vclkc;

	vconf = 0;
	/* 1bit mode */
	pconf->bus_width = 0;
	/* 512byte block length */
	pconf->bl_len = 9;
	/* 64 CLK cycle, here 2^8 = 256 clk cycles */
	pconf->resp_timeout = 8;
	/* 1024 CLK cycle, Max. 100mS. */
	pconf->rc_cc = 4;
	pconf->err_abort = 0;
	pconf->auto_clk = 1;
	sd_emmc_regs->gcfg = vconf;

	/*Clear irq status first*/
#ifdef SD_EMMC_IRQ_EN_ALL_INIT
	/*Set Irq Control*/
	sd_emmc_regs->gstatus = 0xffff;
	sd_emmc_regs->girq_en = SD_EMMC_IRQ_ALL;

#endif

}

/*sd_emmc controller irq*/
static irqreturn_t aml_sd_emmc_irq_v3(int irq, void *dev_id)
{
	struct amlsd_host *host = dev_id;
	struct sd_emmc_regs_v3 *sd_emmc_regs
		= (struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	struct mmc_host *mmc;
	struct amlsd_platform *pdata;
	struct mmc_request *mrq;
	unsigned long flags;
	u32 vstat = 0;
	u32 virqc = 0;
	u32 vstart = 0;
	u32 err = 0;

	struct sd_emmc_irq_en *irqc = (struct sd_emmc_irq_en *)&virqc;
	struct sd_emmc_status *ista = (struct sd_emmc_status *)&vstat;
	struct sd_emmc_start *desc_start = (struct sd_emmc_start *)&vstart;

	virqc = sd_emmc_regs->girq_en & 0xffff;
	vstat = sd_emmc_regs->gstatus & 0xffffffff;
	sd_emmc_dbg(AMLSD_DBG_REQ , "%s %d occurred, vstat:0x%x\n"
		"sd_emmc_regs->gstatus:%x\n",
		__func__, __LINE__, vstat, sd_emmc_regs->gstatus);

	host->ista = vstat;
	if (irqc->irq_sdio && ista->irq_sdio) {
		if ((host->mmc->sdio_irq_thread)
			&& (!atomic_read(&host->mmc->sdio_irq_thread_abort))) {
			mmc_signal_sdio_irq(host->mmc);
			if (!(vstat & 0x3fff))
				return IRQ_HANDLED;
			/*else
				pr_info("other irq also occurred 0x%x\n",
			vstat);*/
		}
	} else if (!(vstat & 0x3fff)) {
		return IRQ_HANDLED;
	}
	spin_lock_irqsave(&host->mrq_lock, flags);
	mrq = host->mrq;
	mmc = host->mmc;
	pdata = mmc_priv(mmc);
	vstart = sd_emmc_regs->gstart;
	if ((desc_start->busy == 1)
		&& (aml_card_type_mmc(pdata) ||
			(aml_card_type_non_sdio(pdata)))) {
		desc_start->busy = 0;
		sd_emmc_regs->gstart = vstart;
	}
	if (!mmc) {
		pr_info("sd_emmc_regs->girq_en = 0x%x at line %d\n",
			sd_emmc_regs->girq_en, __LINE__);
		pr_info("sd_emmc_regs->gstatus = 0x%x at line %d\n",
			sd_emmc_regs->gstatus, __LINE__);
		pr_info("sd_emmc_regs->gcfg = 0x%x at line %d\n",
			sd_emmc_regs->gcfg, __LINE__);
		pr_info("sd_emmc_regs->gclock = 0x%x at line %d\n",
			sd_emmc_regs->gclock, __LINE__);
	}

#ifdef CHOICE_DEBUG
	pr_info("%s %d cmd:%d arg:0x%x ",
		__func__, __LINE__, mrq->cmd->opcode, mrq->cmd->arg);
	if (mrq->cmd->data)
		pr_info("blksz:%d, blocks:%d\n",
			mrq->data->blksz, mrq->data->blocks);
#endif

	if (!mrq && !irqc->irq_sdio) {
		if (!ista->irq_sdio) {
			sd_emmc_err("NULL mrq in aml_sd_emmc_irq step %d",
				host->xfer_step);
			sd_emmc_err("status:0x%x,irq_c:0x%0x\n",
				sd_emmc_regs->gstatus, sd_emmc_regs->girq_en);
		}
		if (host->xfer_step == XFER_FINISHED ||
			host->xfer_step == XFER_TIMER_TIMEOUT){
			spin_unlock_irqrestore(&host->mrq_lock, flags);
			return IRQ_HANDLED;
		}
#ifdef CHOICE_DEBUG
	aml_sd_emmc_print_reg(host);
#endif
		spin_unlock_irqrestore(&host->mrq_lock, flags);
		return IRQ_HANDLED;
	}
#ifdef CHOICE_DEBUG
	if ((host->xfer_step != XFER_AFTER_START)
		&& (!host->cmd_is_stop) && !irqc->irq_sdio) {
		sd_emmc_err("%s: host->xfer_step=%d\n",
			mmc_hostname(mmc), host->xfer_step);
		pr_info("%%sd_emmc_regs->girq_en = 0x%x at line %d\n",
			sd_emmc_regs->girq_en, __LINE__);
		pr_info("%%sd_emmc_regs->gstatus = 0x%x at line %d\n",
			sd_emmc_regs->gstatus, __LINE__);
		pr_info("%%sd_emmc_regs->gcfg = 0x%x at line %d\n",
			sd_emmc_regs->gcfg, __LINE__);
		pr_info("%%sd_emmc_regs->gclock = 0x%x at line %d\n",
			sd_emmc_regs->gclock, __LINE__);
	}
#endif
	if (mrq) {
		if (host->cmd_is_stop)
			host->xfer_step = XFER_IRQ_TASKLET_BUSY;
		else
			host->xfer_step = XFER_IRQ_OCCUR;
	}

	sd_emmc_regs->gstatus &= 0xffff;
	spin_unlock_irqrestore(&host->mrq_lock, flags);

	if ((ista->end_of_chain) || (ista->desc_irq)) {
		if (mrq->data)
			host->status = HOST_TASKLET_DATA;
		else
			host->status = HOST_TASKLET_CMD;
		mrq->cmd->error = 0;
	}

	if ((vstat & 0x1FFF) && (!host->cmd_is_stop)) {
	#if 0
		pr_err("~~~~%s() %d, fail in %ld/%d, %s\n", __func__, __LINE__,
			((vstart & 0xFFFFFFFC)
			- (u32)host->desc_dma_addr)
			/ sizeof(struct sd_emmc_desc_info),
			pdata->desc_cnt,
			host->cmd_is_stop ? "STOP" : "NOP");
	#endif
		err = 1;
	}
	/* error */
	if ((ista->rxd_err) || (ista->txd_err)) {
		host->status = HOST_DAT_CRC_ERR;
		mrq->cmd->error = -EILSEQ;
		/*if (ista->rxd_err)
			pdata->rx_err |= (0xff & vstat);*/
		if (host->is_tunning == 0) {
			sd_emmc_err("%s: warning... data crc, vstat:0x%x, virqc:%x",
					mmc_hostname(host->mmc),
					vstat, virqc);
			sd_emmc_err("@ cmd %d with %p; stop %d, status %d\n",
					mrq->cmd->opcode, mrq->data,
					host->cmd_is_stop,
					host->status);
		}
	} else if (ista->resp_err) {
		if (host->is_tunning == 0)
			sd_emmc_err("%s: warning... response crc,vstat:0x%x,virqc:%x\n",
					mmc_hostname(host->mmc),
					vstat, virqc);
		host->status = HOST_RSP_CRC_ERR;
		mrq->cmd->error = -EILSEQ;
	} else if (ista->resp_timeout) {
		if (host->is_tunning == 0)
			sd_emmc_err("%s: resp_timeout,vstat:0x%x,virqc:%x\n",
					mmc_hostname(host->mmc),
					vstat, virqc);
		host->status = HOST_RSP_TIMEOUT_ERR;
		mrq->cmd->error = -ETIMEDOUT;
	} else if (ista->desc_timeout) {
		if (host->is_tunning == 0)
			sd_emmc_err("%s: desc_timeout,vstat:0x%x,virqc:%x\n",
					mmc_hostname(host->mmc),
					vstat, virqc);
		host->status = HOST_DAT_TIMEOUT_ERR;
		mrq->cmd->error = -ETIMEDOUT;
	}
#if 0
	else{
		host->xfer_step = XFER_IRQ_UNKNOWN_IRQ;
		sd_emmc_err("%s: %s Unknown Irq Ictl 0x%x, Ista 0x%x\n",
				mmc_hostname(host->mmc),
				pdata->pinname, virqc, vstat);
	}
#endif
	/* just for error show */
	if (err && (!host->is_timming)) {
		if (host->is_tunning == 0)
			aml_host_bus_fsm_show(host, ista->bus_fsm);
		if (aml_card_type_mmc(pdata))
			mmc_cmd_LBA_show(mmc, mrq);
	}

	if (host->xfer_step != XFER_IRQ_UNKNOWN_IRQ) {
#ifdef SD_EMMC_DATA_TASKLET
		tasklet_schedule(&sd_emmc_finish_tasklet);
		return IRQ_HANDLED;
#else
		return IRQ_WAKE_THREAD;
#endif
	} else
		return IRQ_HANDLED;
}

static irqreturn_t aml_sd_emmc_data_thread_v3(int irq, void *data)
{
#ifdef SD_EMMC_DATA_TASKLET
	struct amlsd_host *host = (struct amlsd_host *)data;
#else
	struct amlsd_host *host = data;
#endif
	/*struct sd_emmc_regs_v3 *sd_emmc_regs
		= (struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	u32 adjust = sd_emmc_regs->gadjust;
	struct sd_emmc_adjust *gadjust = (struct sd_emmc_adjust *)&adjust;
	u32 vclk = sd_emmc_regs->gclock;
	struct sd_emmc_clock *clkc = (struct sd_emmc_clock *)&(vclk);*/
	struct mmc_request *mrq;
	enum aml_mmc_waitfor xfer_step;
	unsigned long flags;
	struct amlsd_platform *pdata = mmc_priv(host->mmc);
	u32 status, xfer_bytes = 0;

	spin_lock_irqsave(&host->mrq_lock, flags);
	mrq = host->mrq;
	xfer_step = host->xfer_step;
	status = host->status;

	if ((xfer_step == XFER_FINISHED) || (xfer_step == XFER_TIMER_TIMEOUT)) {
		sd_emmc_err("Warning: %s xfer_step=%d, host->status=%d\n",
			mmc_hostname(host->mmc), xfer_step, status);
		spin_unlock_irqrestore(&host->mrq_lock, flags);
#ifdef SD_EMMC_DATA_TASKLET
		return;
#else
		return IRQ_HANDLED;
#endif
	}

	WARN_ON((host->xfer_step != XFER_IRQ_OCCUR)
		 && (host->xfer_step != XFER_IRQ_TASKLET_BUSY));

	if (!mrq) {
		sd_emmc_err("%s: !mrq xfer_step %d\n",
			mmc_hostname(host->mmc), xfer_step);
		if (xfer_step == XFER_FINISHED ||
			xfer_step == XFER_TIMER_TIMEOUT){
			spin_unlock_irqrestore(&host->mrq_lock, flags);
#ifdef SD_EMMC_DATA_TASKLET
			return;
#else
			return IRQ_HANDLED;
#endif
		}
		aml_sd_emmc_print_err(host);
	}
	/* process stop cmd we sent on porpos */
	if (host->cmd_is_stop) {
		/* --new irq enter, */
		host->cmd_is_stop = 0;
		mrq->cmd->error = host->error_bak;
		spin_unlock_irqrestore(&host->mrq_lock, flags);
		aml_sd_emmc_request_done(host->mmc, host->mrq);
#ifdef SD_EMMC_DATA_TASKLET
		return;
#else
		if (host->is_tunning == 0)
			pr_info("%s : %d\n", __func__, __LINE__);
		return IRQ_HANDLED;
#endif
	}
	spin_unlock_irqrestore(&host->mrq_lock, flags);

	BUG_ON(!host->mrq->cmd);

	switch (status) {
	case HOST_TASKLET_DATA:
	case HOST_TASKLET_CMD:
		/* WARN_ON(aml_sd_emmc_desc_check(host)); */
		sd_emmc_dbg(AMLSD_DBG_REQ, "%s %d cmd:%d\n",
			__func__, __LINE__, mrq->cmd->opcode);
		host->error_flag = 0;
		if (mrq->cmd->data &&  mrq->cmd->opcode) {
			xfer_bytes = mrq->data->blksz*mrq->data->blocks;
			/* copy buffer from dma to data->sg in read cmd*/
#ifdef SD_EMMC_REQ_DMA_SGMAP
			WARN_ON(aml_sd_emmc_post_dma(host, mrq));
#else
			if (host->mrq->data->flags & MMC_DATA_READ) {
				aml_sg_copy_buffer(mrq->data->sg,
				mrq->data->sg_len, host->bn_buf,
				 xfer_bytes, 0);
			}
#endif
			mrq->data->bytes_xfered = xfer_bytes;
			host->xfer_step = XFER_TASKLET_DATA;
		} else {
			host->xfer_step = XFER_TASKLET_CMD;
		}
		spin_lock_irqsave(&host->mrq_lock, flags);
		mrq->cmd->error = 0;
		spin_unlock_irqrestore(&host->mrq_lock, flags);

		aml_sd_emmc_read_response(host->mmc, mrq->cmd);
		aml_sd_emmc_request_done(host->mmc, mrq);

		break;

	case HOST_RSP_TIMEOUT_ERR:
	case HOST_DAT_TIMEOUT_ERR:
	case HOST_RSP_CRC_ERR:
	case HOST_DAT_CRC_ERR:
		if (host->is_tunning == 0)
			pr_info("%s %d %s: cmd:%d\n", __func__, __LINE__,
				mmc_hostname(host->mmc), mrq->cmd->opcode);
		if (mrq->cmd->data) {
			dma_unmap_sg(mmc_dev(host->mmc), mrq->cmd->data->sg,
				mrq->cmd->data->sg_len,
				(mrq->cmd->data->flags & MMC_DATA_READ) ?
					DMA_FROM_DEVICE : DMA_TO_DEVICE);
		}
		aml_sd_emmc_read_response(host->mmc, host->mrq->cmd);

		/* set retry @ 1st error happens! */
		if ((host->error_flag == 0)
			&& (aml_card_type_mmc(pdata)
				|| aml_card_type_non_sdio(pdata))
			&& (host->is_tunning == 0)) {

			sd_emmc_err("%s() %d: set 1st retry!\n",
				__func__, __LINE__);
			host->error_flag |= (1<<0);
			spin_lock_irqsave(&host->mrq_lock, flags);
			mrq->cmd->retries = 3;
			spin_unlock_irqrestore(&host->mrq_lock, flags);
		}

		if (aml_card_type_mmc(pdata) &&
			(host->error_flag & (1<<0)) && mrq->cmd->retries) {
			sd_emmc_err("retry cmd %d the %d-th time(s)\n",
				mrq->cmd->opcode, mrq->cmd->retries);
			/* chage configs on current host */
		}
		/* last retry effort! */
		if ((aml_card_type_mmc(pdata) || aml_card_type_non_sdio(pdata))
			&& host->error_flag && (mrq->cmd->retries == 0)) {
			host->error_flag |= (1<<30);
			sd_emmc_err("Command retried failed line:%d, cmd:%d\n",
				__LINE__, mrq->cmd->opcode);
		}
		/* retry need send a stop 2 emmc... */
		/* do not send stop for sdio wifi case */
		if (host->mrq->stop
			&& (aml_card_type_mmc(pdata)
				|| aml_card_type_non_sdio(pdata))
			&& pdata->is_in
			&& (host->mrq->cmd->opcode != MMC_SEND_TUNING_BLOCK)
			&& (host->mrq->cmd->opcode !=
					MMC_SEND_TUNING_BLOCK_HS200))
			aml_sd_emmc_send_stop(host);
		else
			aml_sd_emmc_request_done(host->mmc, mrq);
		break;

	default:
		sd_emmc_err("BUG %s: xfer_step=%d, host->status=%d\n",
			mmc_hostname(host->mmc),  xfer_step, status);
		aml_sd_emmc_print_err(host);
	}

#ifdef SD_EMMC_DATA_TASKLET
		return;
#else
		return IRQ_HANDLED;
#endif
}

#define SRC_24MHZ 0
#define SRC_400MHZ 1
static void aml_sd_emmc_set_clk_rate_v3(struct mmc_host *mmc,
	unsigned int clk_ios)
{
	u32 clk_rate, clk_div, clk_src_sel;
	unsigned long flags;
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = (void *)pdata->host;
	struct sd_emmc_regs_v3 *sd_emmc_regs =
		(struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	void __iomem *source_base = NULL;
	u32 second_src = 0;
	if (clk_ios == 0) {
		aml_sd_emmc_clk_switch_off(host);
		return;
	}

	clk_src_sel = SD_EMMC_CLOCK_SRC_OSC;
	if (clk_ios < 20000000) {
		second_src = SRC_24MHZ;
		clk_src_sel = SD_EMMC_CLOCK_SRC_OSC;
	} else {
		clk_src_sel = SD_EMMC_CLOCK_SRC_FCLK_DIV2;

		if ((host->ctrl_ver >= 3)
			&& (aml_card_type_mmc(pdata))) {
			clk_src_sel = SD_EMMC_CLOCK_SRC_OSC;
			second_src = SRC_400MHZ;
			source_base =
				ioremap_nocache(P_HHI_NAND_CLK_CNTL,
					sizeof(u32));
			writel((1<<7)|(3<<9), source_base);
			emmc_dbg(AMLSD_DBG_V3, "P_HHI_NAND_CLK_CNTL = 0x%x\n",
				readl(source_base));
			iounmap(source_base);
		}
	}
	emmc_dbg(AMLSD_DBG_V3, "clk_ios: %u\n", clk_ios);
	if (clk_ios > pdata->f_max)
		clk_ios = pdata->f_max;
	if (clk_ios < pdata->f_min)
		clk_ios = pdata->f_min;

	WARN_ON(clk_src_sel > SD_EMMC_CLOCK_SRC_FCLK_DIV2);

	switch (clk_src_sel) {
	case SD_EMMC_CLOCK_SRC_OSC:
		switch (second_src) {
		case SRC_24MHZ:
			clk_rate = 24000000;
			break;
		case SRC_400MHZ:
			clk_rate = 400000000;
			break;
		default:
			break;
		}
		break;
	case SD_EMMC_CLOCK_SRC_FCLK_DIV2:
		clk_rate = 1000000000;
		break;
	default:
		sdhc_err("%s: clock source error: %d\n",
			mmc_hostname(host->mmc), clk_src_sel);
		return;
	}

	spin_lock_irqsave(&host->mrq_lock, flags);
	clk_div = (clk_rate / clk_ios) + (!!(clk_rate % clk_ios));
	emmc_dbg(AMLSD_DBG_V3, "clk_div: %u, clk_rate: %u\n",
			clk_div, clk_rate);
	aml_sd_emmc_clk_switch(pdata, clk_div, clk_src_sel);

	sd_emmc_dbg(AMLSD_DBG_CLKC, "sd_emmc_regs->gclock = 0x%x\n",
		sd_emmc_regs->gclock);
	pdata->clkc = sd_emmc_regs->gclock;

	pdata->mmc->actual_clock = clk_rate / clk_div;
	emmc_dbg(AMLSD_DBG_V3, "actual_clock: %u\n",
			pdata->mmc->actual_clock);
	/*Wait for a while after clock setting*/
	/* udelay(100); */
	spin_unlock_irqrestore(&host->mrq_lock, flags);

	return;
}

static void aml_sd_emmc_set_timing_v3(struct amlsd_platform *pdata,
				u32 timing)
{
	struct amlsd_host *host = (void *)pdata->host;
	struct sd_emmc_regs_v3 *sd_emmc_regs =
		(struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	u32 vctrl = sd_emmc_regs->gcfg;
	struct sd_emmc_config *ctrl = (struct sd_emmc_config *)&vctrl;
	u32 vclkc = sd_emmc_regs->gclock;
	struct sd_emmc_clock_v3 *clkc = (struct sd_emmc_clock_v3 *)&vclkc;
	u32 adjust = sd_emmc_regs->gadjust;
	struct sd_emmc_adjust_v3 *gadjust = (struct sd_emmc_adjust_v3 *)&adjust;
	u8 clk_div;
	if ((timing == MMC_TIMING_MMC_HS400) ||
			 (timing == MMC_TIMING_MMC_DDR52) ||
				 (timing == MMC_TIMING_UHS_DDR50)) {
		if (timing == MMC_TIMING_MMC_HS400) {
			/*ctrl->chk_ds = 1;*/
			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXLX) {
				adjust = sd_emmc_regs->gadjust;
				gadjust->ds_enable = 1;
				sd_emmc_regs->gadjust = adjust;
				clkc->tx_delay = pdata->tx_delay;
				/*host->tuning_mode = AUTO_TUNING_MODE;*/
				pr_info("%s: try set sd/emmc to HS400 mode\n",
					mmc_hostname(host->mmc));
			}
		}
		ctrl->ddr = 1;
		clk_div = clkc->div;
		if (clk_div & 0x01)
			clk_div++;
			clkc->div = clk_div / 2;
		if (aml_card_type_mmc(pdata))
			clkc->core_phase = 2;
		sd_emmc_regs->gclock = vclkc;
		pdata->clkc = sd_emmc_regs->gclock;
		pr_info("%s: try set sd/emmc to DDR mode\n",
			mmc_hostname(host->mmc));

	} else if (timing == MMC_TIMING_MMC_HS) {
		clkc->core_phase = 3;
		sd_emmc_regs->gclock = vclkc;
		pdata->clkc = vclkc;
	} else if ((timing == MMC_TIMING_SD_HS) ||
			(aml_card_type_non_sdio(pdata))) {
		clkc->core_phase = 2;
		sd_emmc_regs->gclock = vclkc;
		pdata->clkc = vclkc;
	} else
		ctrl->ddr = 0;

	sd_emmc_regs->gcfg = vctrl;
	sd_emmc_dbg(AMLSD_DBG_IOS, "sd emmc is %s\n",
			ctrl->ddr?"DDR mode":"SDR mode");
	return;
}

/*setup bus width, 1bit, 4bits, 8bits*/
static void aml_sd_emmc_set_bus_width_v3(struct amlsd_platform *pdata,
						u32 busw_ios)
{
	struct amlsd_host *host = (void *)pdata->host;
	struct sd_emmc_regs_v3 *sd_emmc_regs
			= (struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	u32 vctrl = sd_emmc_regs->gcfg;
	struct sd_emmc_config *ctrl = (struct sd_emmc_config *)&vctrl;
	u32 width = 0;

	switch (busw_ios) {
	case MMC_BUS_WIDTH_1:
		width = 0;
		break;
	case MMC_BUS_WIDTH_4:
		width = 1;
		break;
	case MMC_BUS_WIDTH_8:
		width = 2;
		break;
	default:
		sd_emmc_err("%s: error Data Bus\n", mmc_hostname(host->mmc));
		break;
	}

	ctrl->bus_width = width;
	pdata->width = width;

	sd_emmc_regs->gcfg = vctrl;
	sd_emmc_dbg(AMLSD_DBG_IOS, "Bus Width Ios %d\n", busw_ios);
}

/*call by mmc, power on, power off ...*/
static void aml_sd_emmc_set_power_v3(struct amlsd_platform *pdata,
					u32 power_mode)
{
	struct amlsd_host *host = (void *)pdata->host;
	struct sd_emmc_regs_v3 *sd_emmc_regs
			= (struct sd_emmc_regs_v3 *)host->sd_emmc_regs;

	switch (power_mode) {
	case MMC_POWER_ON:
		if (pdata->pwr_pre)
			pdata->pwr_pre(pdata);
		if (pdata->pwr_on)
			pdata->pwr_on(pdata);
		break;
	case MMC_POWER_UP:
		break;
	case MMC_POWER_OFF:
		sd_emmc_regs->gdelay1 = 0;
		sd_emmc_regs->gdelay2 = 0;
		sd_emmc_regs->gadjust = 0;
		break;
	default:
		if (pdata->pwr_pre)
			pdata->pwr_pre(pdata);
		if (pdata->pwr_off)
			pdata->pwr_off(pdata);
		break;
	}
}

void aml_sd_emmc_set_ios_v3(struct mmc_host *mmc,
				struct mmc_ios *ios)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);

	if (!pdata->is_in)
		return;
	/*Set Power*/
	aml_sd_emmc_set_power_v3(pdata, ios->power_mode);

	/*Set Clock*/
	aml_sd_emmc_set_clk_rate_v3(mmc, ios->clock);

	/*Set Bus Width*/
	aml_sd_emmc_set_bus_width_v3(pdata, ios->bus_width);

	/* Set Date Mode */
	aml_sd_emmc_set_timing_v3(pdata, ios->timing);

	if (ios->chip_select == MMC_CS_HIGH) {
		aml_cs_high(pdata);
	} else if (ios->chip_select == MMC_CS_DONTCARE) {
		aml_cs_dont_care(pdata);
	} else { /* MMC_CS_LOW */
	/* Nothing to do */
	}
}


struct amlsd_host *aml_sd_emmc_init_host_v3(struct amlsd_host *host)
{
	spin_lock_init(&aml_sd_emmc_claim.lock);
	init_waitqueue_head(&aml_sd_emmc_claim.wq);

#ifdef SD_EMMC_DATA_TASKLET
	tasklet_init(&sd_emmc_finish_tasklet,
		aml_sd_emmc_data_tasklet, (unsigned long)host);

	if (request_irq(host->irq, aml_sd_emmc_irq,
		IRQF_DISABLED, "sd_emmc", (void *)host)) {
		sd_emmc_err("Request sd_emmc Irq Error!\n");
		return NULL;
	}
#else
	if (request_threaded_irq(host->irq, aml_sd_emmc_irq_v3,
		aml_sd_emmc_data_thread_v3, IRQF_DISABLED, "sd_emmc",
		(void *)host)) {
		sd_emmc_err("Request sd_emmc Irq Error!\n");
		return NULL;
	}
#endif

	/* alloc descriptor info */
	host->desc_buf = dma_alloc_coherent(host->dev,
		SD_EMMC_MAX_DESC_MUN*(sizeof(struct sd_emmc_desc_info)),
				&host->desc_dma_addr, GFP_KERNEL);
	if (NULL == host->desc_buf) {
		sd_emmc_err(" desc_buf Dma alloc Fail!\n");
		return NULL;
	}

	/* do not need malloc one dma buffer later */
	host->bn_buf = dma_alloc_coherent(host->dev, SD_EMMC_BOUNCE_REQ_SIZE,
				&host->bn_dma_buf, GFP_KERNEL);
	if (NULL == host->bn_buf) {
		sd_emmc_err("Dma alloc Fail!\n");
		return NULL;
	}
#ifdef AML_RESP_WR_EXT
	host->resp_buf = dma_alloc_coherent(host->dev, 4*sizeof(u32),
					&host->resp_dma_buf, GFP_KERNEL);
	if (NULL == host->bn_buf) {
		sd_emmc_err("Dma alloc Fail!\n");
		return NULL;
	}
#endif
	spin_lock_init(&host->mrq_lock);
	mutex_init(&host->pinmux_lock);
	host->xfer_step = XFER_INIT;

	INIT_LIST_HEAD(&host->sibling);

	host->init_flag = 1;

	host->version = AML_MMC_VERSION;
	host->pinctrl = NULL;
	host->is_gated = false;
	host->status = HOST_INVALID;
	host->msg_buf = kmalloc(MESSAGE_BUF_SIZE, GFP_KERNEL);
	if (!host->msg_buf)
		pr_info("malloc message buffer fail\n");

#ifdef CONFIG_MMC_AML_DEBUG
	host->req_cnt = 0;
	sd_emmc_err("CONFIG_MMC_AML_DEBUG is on!\n");
#endif

#ifdef CONFIG_AML_MMC_DEBUG_FORCE_SINGLE_BLOCK_RW
	sd_emmc_err("CONFIG_AML_MMC_DEBUG_FORCE_SINGLE_BLOCK_RW is on!\n");
#endif

	return host;
}

static int aml_sd_emmc_cali_v3(struct mmc_host *mmc,
	u8 opcode, u8 *blk_test, u32 blksz, u32 blocks)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	struct mmc_request mrq = {NULL};
	struct mmc_command cmd = {0};
	struct mmc_command stop = {0};
	struct mmc_data data = {0};
	struct scatterlist sg;

	cmd.opcode = opcode;
	cmd.arg = ((SZ_1M * (36 + 3)) / 512);
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

	stop.opcode = MMC_STOP_TRANSMISSION;
	stop.arg = 0;
	stop.flags = MMC_RSP_R1B | MMC_CMD_AC;

	data.blksz = blksz;
	data.blocks = blocks;
	data.flags = MMC_DATA_READ;
	data.sg = &sg;
	data.sg_len = 1;

	memset(blk_test, 0, blksz * data.blocks);
	sg_init_one(&sg, blk_test, blksz * data.blocks);

	mrq.cmd = &cmd;
	mrq.stop = &stop;
	mrq.data = &data;
	host->mrq = &mrq;
	mmc_wait_for_req(mmc, &mrq);
	return data.error | cmd.error;
}

static int emmc_eyetest_log(struct mmc_host *mmc, u32 line_x)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	struct sd_emmc_regs_v3 *sd_emmc_regs =
		(struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	u32 adjust = sd_emmc_regs->gadjust;
	struct sd_emmc_adjust_v3 *gadjust =
		(struct sd_emmc_adjust_v3 *)&adjust;
	u32 eyetest_log = 0;
	struct eyetest_log *geyetest_log = (struct eyetest_log *)&(eyetest_log);
	u32 eyetest_out0 = 0, eyetest_out1 = 0;
	u32 intf3 = sd_emmc_regs->intf3;
	struct intf3 *gintf3 = (struct intf3 *)&(intf3);
	u32 vcfg = sd_emmc_regs->gcfg;
	int retry = 3;
	u64 tmp = 0;
	u32 blksz = 512;
	u8 *blk_test;
		blk_test = kmalloc(blksz * CALI_BLK_CNT, GFP_KERNEL);
	if (!blk_test)
		return -ENOMEM;
	host->is_tunning = 1;
	/****** calculate  line_x ***************************/
	/******* init eyetest register ************************/
	emmc_dbg(AMLSD_DBG_V3, "delay1: 0x%x , delay2: 0x%x, line_x: %d\n",
	    sd_emmc_regs->gdelay1, sd_emmc_regs->gdelay2, line_x);
	gadjust->cali_enable = 1;
	gadjust->cali_sel = line_x;
	sd_emmc_regs->gadjust = adjust;
	if (line_x < 9)
		gintf3->eyetest_exp = 7;
	else
		gintf3->eyetest_exp = 3;
RETRY:

	gintf3->eyetest_on = 1;
	sd_emmc_regs->intf3 = intf3;
	/*emmc_dbg(AMLSD_DBG_V3, "intf3: 0x%x\n", sd_emmc_regs->intf3);*/

	/*****test start*************/
	udelay(5);
	if (line_x < 9)
		aml_sd_emmc_cali_v3(mmc,
				MMC_READ_MULTIPLE_BLOCK,
				blk_test, blksz, 40);
	else
		aml_sd_emmc_cali_v3(mmc,
				MMC_READ_MULTIPLE_BLOCK,
				blk_test, blksz, 80);
	udelay(1);
	eyetest_out0 = sd_emmc_regs->eyetest_out0;
	eyetest_out1 = sd_emmc_regs->eyetest_out1;
	eyetest_log = sd_emmc_regs->eyetest_log;

	if (!(geyetest_log->eyetest_done & 0x1)) {
		pr_warn("testint eyetest times: 0x%x, out: 0x%x, 0x%x\n",
				geyetest_log->eyetest_times,
				eyetest_out0, eyetest_out1);
		gintf3->eyetest_on = 0;
		sd_emmc_regs->intf3 = intf3;
		retry--;
		if (retry == 0) {
			pr_warn("[%s][%d] retry eyetest failed\n",
					__func__, __LINE__);
			return 1;
		}
		goto RETRY;
	}
	/*emmc_dbg(AMLSD_DBG_V3,
		"test done! eyetest times: 0x%x, out: 0x%x, 0x%x\n",
			geyetest_log->eyetest_times,
			eyetest_out0, eyetest_out1);*/
	gintf3->eyetest_on = 0;
	sd_emmc_regs->intf3 = intf3;
	/*emmc_dbg(AMLSD_DBG_V3, "intf3: 0x%x,adjust: 0x%x\n",
			sd_emmc_regs->intf3, sd_emmc_regs->gadjust);*/
	if (vcfg & 0x4) {
		if (pdata->count > 32) {
			eyetest_out1 <<= (32 - (pdata->count - 32));
			eyetest_out1 >>= (32 - (pdata->count - 32));
		} else
			eyetest_out1 = 0x0;
	}
	pdata->align[line_x] = ((tmp | eyetest_out1) << 32) | eyetest_out0;
	emmc_dbg(AMLSD_DBG_V3, "u64 eyetestout 0x%llx\n",
			pdata->align[line_x]);
	host->is_tunning = 0;
	kfree(blk_test);
	return 0;
}

static int fbinary(u64 x)
{
	int i;
	for (i = 0; i < 64; i++) {
		if ((x >> i) & 0x1)
			return i;
	}
	return -1;
}
/*
static u64 get_base_line(u64 temp)
{
	int i;
	u64 base_line = 0;
	for (i = 0; i < 64; ) {
		if((temp >> i) & 0xf)
			base_line |= (15ULL << i);
		pr_info("base_line: 0x%llx\n", base_line);
		i += 4;
	}
	return base_line;
}

static int compare_line_alignment(u64 base, u64 line)
{
	int i;
	u64 tmp;
	int count1 = -1;
	int count2 = -1;
	for (i = 0; i <= 64; )
	{
		if((base >> i) & 0xf) {
			tmp = base & line;
			if (!((tmp >> i) & 0xf))
				return 0;
			if (count1 == -1)
				count1 = 1;
		} else {
			tmp = base | line;
			if ((tmp >> i) & 0xf)
				return 0;
			if (count2 == -1)
				count2 = 1;
		}
		if ((count1 + count2) == 2)
			return 1;
		i += 4;
	}
	return 1;
}
*/
/*
static int emmc_detect_base_line(u64 *arr)
{
	u32 i = 0, first[10] = {0};
	u32  max = 0, l_max = 0xff;
	for (i = 0; i < 8; i++) {
		first[i] = fbinary(arr[i]);
		if((first[i] >= 0) && (first[i] <= 6)) {
			if (first[i] >= max) {
				l_max = i;
				max = first[i];
			}
		}
	}
	if (l_max == 0xff) {
		for (i = 0; i < 8; i++) {
			if (first[i] > max) {
				l_max = i;
				max = first[i];
			}
		}
	}

	temp = arr[l_max];
	pr_info("temp: %llx\n", temp);
	pr_warn("%s [%d] detect line:%d, max: %u\n",
			__func__, __LINE__, l_max, max);
	return max;
}
*/


static int emmc_detect_base_line(u64 *arr)
{
	u32 i = 0, first[10] = {0};
	u32  max = 0, l_max = 0xff;
	for (i = 0; i < 8; i++) {
		first[i] = fbinary(arr[i]);
		if (first[i] > max) {
			l_max = i;
			max = first[i];
		}
	}
	emmc_dbg(AMLSD_DBG_V3, "%s [%d] detect line:%d, max: %u\n",
			__func__, __LINE__, l_max, max);
	return max;
}

/**************** start all data align ********************/
static int emmc_all_data_line_alignment(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	struct sd_emmc_regs_v3 *sd_emmc_regs =
		(struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	u32 delay1 = 0, delay2 = 0;
	int result;
	int temp = 0, base_line = 0;

	pdata->base_line = emmc_detect_base_line(pdata->align);
	base_line = pdata->base_line;
	for (line_x = 0; line_x < 9; line_x++) {
		if (line_x == 8)
			continue;
		if (pdata->align[line_x] & 0xf)
			continue;
		temp = fbinary(pdata->align[line_x]);
		result = base_line - temp;
		emmc_dbg(AMLSD_DBG_V3, "*****line_x: %d, result: %d\n",
				line_x, result);
	    if (line_x < 5)
			delay1 |= result << (6 * line_x);
	    else
			delay2 |= result << (6 * (line_x - 5));
	}
	sd_emmc_regs->gdelay1 += delay1;
	sd_emmc_regs->gdelay2 += delay2;
	emmc_dbg(AMLSD_DBG_V3, "delay1: 0x%x, delay2: 0x%x\n",
			delay1, delay2);
	emmc_dbg(AMLSD_DBG_V3, "gdelay1: 0x%x, gdelay2: 0x%x\n",
			sd_emmc_regs->gdelay1, sd_emmc_regs->gdelay2);

	return 0;
}

static int emmc_ds_data_alignment(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	struct sd_emmc_regs_v3 *sd_emmc_regs =
		(struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	u32 delay1 = sd_emmc_regs->gdelay1;
	u32 delay2 = sd_emmc_regs->gdelay2;
	int i, line_x = 4;

	for (i = 0; i < 64; i++) {
		emmc_dbg(AMLSD_DBG_V3, "i = %d, delay1: 0x%x, delay2: 0x%x\n",
			i, sd_emmc_regs->gdelay1,
			sd_emmc_regs->gdelay2);
		delay1 += (1<<0)|(1<<6)|(1<<12)|(1<<18)|(1<<24);
		delay2 += (1<<0)|(1<<6)|(1<<12);
		sd_emmc_regs->gdelay1 = delay1;
		sd_emmc_regs->gdelay2 = delay2;
		emmc_eyetest_log(mmc, line_x);
		if (pdata->align[line_x] & 0xf0)
			break;
	}
	if (i == 64) {
		pr_warn("%s [%d] Don't find line delay which aligned with DS\n",
			__func__, __LINE__);
		return 1;
	}
	return 0;
}


static void print_all_line_eyetest(struct mmc_host *mmc)
{
	for (line_x = 0; line_x < 10; line_x++)
		emmc_eyetest_log(mmc, line_x);
	return;
}
/* first step*/
static int emmc_ds_core_align(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	struct sd_emmc_regs_v3 *sd_emmc_regs =
		(struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	u32 delay1 = sd_emmc_regs->gdelay1;
	u32 delay2 = sd_emmc_regs->gdelay2;
	u32 delay2_bak = delay2;
	u32 count = 0;
	u32 ds_count = 0;
	ds_count = fbinary(pdata->align[8]);
	if (ds_count == 0)
		if ((pdata->align[8] & 0xf0) == 0)
			return 0;
	emmc_dbg(AMLSD_DBG_V3, "ds_count:%d,delay1:0x%x,delay2:0x%x\n",
			ds_count, sd_emmc_regs->gdelay1, sd_emmc_regs->gdelay2);
	if (ds_count < 20) {
		delay2 += ((20 - ds_count) << 18);
		sd_emmc_regs->gdelay2 = delay2;
	} else
		sd_emmc_regs->gdelay2 += (1<<18);
	emmc_eyetest_log(mmc, 8);
	while (!(pdata->align[8] & 0xf)) {
		sd_emmc_regs->gdelay2 += (1<<18);
		emmc_eyetest_log(mmc, 8);
	}
	delay1 = sd_emmc_regs->gdelay1;
	delay2 = sd_emmc_regs->gdelay2;
	ds_count = fbinary(pdata->align[8]);
	count = ((delay2>>18) & 0x3f) - ((delay2_bak>>18) & 0x3f);
	delay1 += (count<<0)|(count<<6)|(count<<12)|(count<<18)|(count<<24);
	delay2 += (count<<0)|(count<<6)|(count<<12)|(count<<24);
	sd_emmc_regs->gdelay1 = delay1;
	sd_emmc_regs->gdelay2 = delay2;
	emmc_dbg(AMLSD_DBG_V3,
		"ds_count:%d,delay1:0x%x,delay2:0x%x,count: %u\n",
		ds_count, sd_emmc_regs->gdelay1,
		sd_emmc_regs->gdelay2, count);
	return 0;
}

#if 1
static int emmc_ds_manual_sht(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	struct sd_emmc_regs_v3 *sd_emmc_regs =
		(struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	u32 intf3 = sd_emmc_regs->intf3;
	struct intf3 *gintf3 = (struct intf3 *)&(intf3);
	u8 *blk_test = NULL;
	u32 blksz = 512;
	int i, err = 0;
	int match[32];
	int best_start = -1, best_size = -1;
	int cur_start  = -1, cur_size = 0;
	pr_info("[%s] 2017-7-4 emmc HS400 Timming\n", __func__);
	sd_emmc_debug = 0x2000;
	blk_test = kmalloc(blksz * CALI_BLK_CNT, GFP_KERNEL);
	if (!blk_test)
		return -ENOMEM;

	print_all_line_eyetest(mmc);
	emmc_ds_core_align(mmc);
	print_all_line_eyetest(mmc);
	emmc_all_data_line_alignment(mmc);
	print_all_line_eyetest(mmc);
	emmc_ds_data_alignment(mmc);
	print_all_line_eyetest(mmc);
	host->is_tunning = 1;
	for (i = 0; i < 32; i++) {
		gintf3->ds_sht_m += 1;
		sd_emmc_regs->intf3 = intf3;
		err = aml_sd_emmc_cali_v3(mmc,
			MMC_READ_MULTIPLE_BLOCK,
			blk_test, blksz, 20);
		pr_warn("intf3: 0x%x, err[%d]: %d\n",
				sd_emmc_regs->intf3, i, err);
		if (!err)
			match[i] = 0;
		else
			match[i] = -1;
	}
	for (i = 0; i < 32; i++) {
		if (match[i] == 0) {
			if (cur_start < 0)
				cur_start = i;
			cur_size++;
		} else {
			if (cur_start >= 0) {
				if (best_start < 0) {
					best_start = cur_start;
					best_size = cur_size;
				} else {
					if (best_size < cur_size) {
						best_start = cur_start;
						best_size = cur_size;
					}
				}
				cur_start = -1;
				cur_size = 0;
			}
		}
	}
	if (cur_start >= 0) {
		if (best_start < 0) {
			best_start = cur_start;
			best_size = cur_size;
		} else if (best_size < cur_size) {
			best_start = cur_start;
			best_size = cur_size;
		}
		cur_start = -1;
		cur_size = -1;
	}
	gintf3->ds_sht_m = best_start + best_size / 2;
	sd_emmc_regs->intf3 = intf3;
	pr_info("ds_sht:%u, window:%d, intf3:0x%x",
			gintf3->ds_sht_m, best_size, sd_emmc_regs->intf3);
	host->is_tunning = 0;
	kfree(blk_test);
	blk_test = NULL;
	return 0;
}
#endif


/* test clock, return delay cells for one cycle
 */
static unsigned int emmc_clktest(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	struct sd_emmc_regs_v3 *sd_emmc_regs =
		(struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	u32 intf3 = sd_emmc_regs->intf3;
	struct intf3 *gintf3 = (struct intf3 *)&(intf3);
	u32 clktest = 0, delay_cell = 0, clktest_log = 0, count = 0;
	u32 vcfg = sd_emmc_regs->gcfg;
	int i = 0;

	sd_emmc_regs->gcfg &= ~(1 << 23);
	sd_emmc_regs->gdelay1 = 0;
	sd_emmc_regs->gdelay2 = 0;
	gintf3->clktest_exp = 8;
	gintf3->clktest_on_m = 1;
	sd_emmc_regs->intf3 = intf3;

	clktest_log = sd_emmc_regs->clktest_log;
	clktest = sd_emmc_regs->clktest_out;
	while (!(clktest_log & 0x80000000)) {
		mdelay(1);
		i++;
		clktest_log = sd_emmc_regs->clktest_log;
		clktest = sd_emmc_regs->clktest_out;
		if (i > 4) {
			pr_warn("[%s] [%d] emmc clktest error\n",
				__func__, __LINE__);
			break;
		}
	}
	if (clktest_log & 0x80000000) {
		clktest = sd_emmc_regs->clktest_out;
		count = clktest / (1 << 8);
		if (vcfg & 0x4)
			delay_cell = (2500 / count);
		else
			delay_cell = (5000 / count);
	}
	pr_info("%s [%d] clktest : %u, delay_cell: %d, count: %u\n",
			__func__, __LINE__, clktest, delay_cell, count);
	gintf3->clktest_on_m = 0;
	sd_emmc_regs->intf3 = intf3;
	sd_emmc_regs->gcfg |= (1 << 23);
	pdata->count = count;
	pdata->delay_cell = delay_cell;
	return count;
}


int aml_post_hs400_timming(struct mmc_host *mmc)
{
	int ret = 0;
	emmc_clktest(mmc);
	ret = emmc_ds_manual_sht(mmc);
	return 0;
}

#if 0
static int emmc_check_line_err(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	struct sd_emmc_regs_v3 *sd_emmc_regs =
		(struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	u32 delay1 = sd_emmc_regs->gdelay1;
	u32 delay2 = sd_emmc_regs->gdelay2;
	u32 rx_err = pdata->rx_err;
	emmc_dbg(EMMC_TIMMING_DBG, "+++++++++rx_err: 0x%x\n", pdata->rx_err);
	if (rx_err == 0xff)
		return 1;
	for (line_x = 0; line_x < 8; line_x++) {
		if (rx_err & 0x1) {
			if (line_x < 5)
				delay1 += (1 << (line_x * 6));
			else
				delay2 += (1 << ((line_x - 5) * 6));
		}
		rx_err >>= 1;
	}
	sd_emmc_regs->gdelay1 = delay1;
	sd_emmc_regs->gdelay2 = delay2;
	emmc_dbg(EMMC_TIMMING_DBG, "++++++++ refix line delay1: 0x%x, delay2: 0x%x\n",
			sd_emmc_regs->gdelay1, sd_emmc_regs->gdelay2);
	return 0;
}

static int emmc_bit_skew(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	struct sd_emmc_regs_v3 *sd_emmc_regs =
		(struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	/*u32 intf3 = sd_emmc_regs->intf3;
	struct intf3 *gintf3 = (struct intf3 *)&(intf3);
	u32 delay1 = sd_emmc_regs->gdelay1;
	u32 delay2 = sd_emmc_regs->gdelay2;*/
	u8 *blk_test = NULL;
	u32 blksz = 512;
	int i, err = 0;
	blk_test = kmalloc(blksz * CALI_BLK_CNT, GFP_KERNEL);
	if (!blk_test)
		return -ENOMEM;

	/*for (line_x = 0; line_x < 10; line_x++)
		emmc_eyetest_log(mmc ,line_x);*/

	host->is_tunning = 0;
	host->is_timming = 1;
	emmc_dbg(EMMC_TIMMING_DBG, "INTF3: 0x%x\n", sd_emmc_regs->intf3);
	pr_info("\n.................................................................\n");
	for (i = 0; i < 63; i++) {
		emmc_dbg(EMMC_TIMMING_DBG, "****start new ds delay *i:%d,delay1:0x%x, delay2: 0x%x\n",
			i, sd_emmc_regs->gdelay1, sd_emmc_regs->gdelay2);
		pdata->rx_err = 0;
		err = aml_sd_emmc_cali_v3(mmc,
			    MMC_READ_MULTIPLE_BLOCK,
				blk_test, blksz, 40);
		while (err) {
			err = emmc_check_line_err(mmc);
			if (!err) {
				pdata->rx_err = 0;
				err = aml_sd_emmc_cali_v3(mmc,
					MMC_READ_MULTIPLE_BLOCK,
					blk_test, blksz, 40);
				emmc_dbg(EMMC_TIMMING_DBG, "++++++retry +i:%d,delay1:0x%x, delay2: 0x%x, err: %d\n",
					i, sd_emmc_regs->gdelay1,
					sd_emmc_regs->gdelay2, err);
			} else
				break;
		}
		if (err == 1)
			break;
		sd_emmc_regs->gdelay2 += (1 << 18);
		emmc_dbg(EMMC_TIMMING_DBG,
			"**next dsdelay*i:%d,delay1:0x%x,delay2:0x%x,err:%d\n",
				i, sd_emmc_regs->gdelay1,
				sd_emmc_regs->gdelay2, err);
	}
	for (line_x = 0; line_x < 10; line_x++)
		emmc_eyetest_log(mmc, line_x);
	kfree(blk_test);
	blk_test = NULL;
	host->is_timming = 0;
	return 0;
}
#endif

/*
static int emmc_all_data_line_alignment(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	struct sd_emmc_regs_v3 *sd_emmc_regs =
		(struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	u32 delay1, delay2, delay1_bak = 0, delay2_bak = 0;
	int i, result;
	int temp = 0, base_line = 0, flag;

	pdata->base_line = emmc_detect_base_line(pdata->align);
	base_line = pdata->base_line;
	for (line_x = 0; line_x < 8; line_x++) {
		if (line_x == 8)
			continue;
		i = 0;
		delay1 = 0;
		delay2 =0;
		temp = fbinary(pdata->align[line_x]);
		result = base_line - temp;
		if (result > 4) {
			i = result - 4;
			flag = 1;
		} else if ((result > 0) && (result <= 4))
			flag = 1;
		else
			flag = 0;
		emmc_dbg(EMMC_TIMMING_DBG,
			"*****line_x: %d, i: %d\n", line_x, i);
		while (result) {
			i++
			if (i == 64) {
				 pr_warn("%s [%d] failed find valid delay\n",
							__func__, __LINE__);
				 break;
			}
			if (line_x < 5)
				delay1 = i << (6 * line_x);
			else
				delay2 = i << (6 * (line_x - 5));
			sd_emmc_regs->gdelay1 = delay1;
			sd_emmc_regs->gdelay2 = delay2;
			emmc_eyetest_log(mmc, line_x);
			temp = fbinary(pdata->align[line_x]);
			result = base_line - temp;
			if (result == 0) {
				emmc_dbg(EMMC_TIMMING_DBG,
					"line_x: %d, i: %d*****  break\n",
					line_x, i);
				break;
			}
			if ((result > 0) && (result <= 4))
				flag = 1;
			if((flag == 1) && ((result > 4) || (result < 0))) {
				if (i > 2) {
					if (line_x < 5)
						delay1 =
							(i-2) << (6*line_x);
					else
						delay2 =
							(i-2) << (6*(line_x-5));
				} else {
					if (line_x < 5)
						delay1 = 0;
					else
						delay2 = 0;
				}
				emmc_dbg(EMMC_TIMMING_DBG,
					"line_x: %d, i: %d*****  break\n",
						line_x, i);
				break;
			}
	    }
	    delay1_bak |= delay1;
	    delay2_bak |= delay2;
	}
	sd_emmc_regs->gdelay1 = delay1_bak;
	sd_emmc_regs->gdelay2 = delay2_bak;
	return 0;
}
*/

#define TXLX_DATA_ALIGN_DEBUG
#if 0
static int emmc_ds_data_alignment(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	struct sd_emmc_regs *sd_emmc_regs =
		(struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	/*u32 intf3 = sd_emmc_regs->intf3;
	struct intf3 *gintf3 = (struct intf3 *)&(intf3);*/
	u32 delay1 = sd_emmc_regs->gdelay1;
	/*struct sd_emmc_delay1 *gdelay1 = (struct sd_emmc_delay1 *)&delay1;*/
	u32 delay2 = sd_emmc_regs->gdelay2;
	/*struct sd_emmc_delay2 *gdelay2 = (struct sd_emmc_delay2 *)&delay2;
	u32 delay1_bak = 0, delay2_bak = 0;*/
	u64 base_tmp = 0;
	int i, result = 0;
	u32 blksz = 512;
	u8 *blk_test = NULL;
	u32 comp1, comp2;

#ifdef TXLX_DATA_ALIGN_DEBUG
	for (line_x = 0; line_x < 10; line_x++) {
		emmc_eyetest_log(mmc, line_x);
		pr_info("u64 eyetest line_x: %u,  0x%llx++++++++++++++++\n",
			line_x, pdata->align[line_x]);
	}
#endif
	blk_test = kmalloc(blksz * CALI_BLK_CNT, GFP_KERNEL);
	if (!blk_test)
		return -ENOMEM;

	comp1 = fbinary(pdata->align[8]);
	comp2 = fbinary(pdata->align[3]);
	if (comp1 > comp2) {
		pdata->order = 1;/*delay data*/
		base_tmp = get_base_line(pdata->align[8]);
	} else if (comp1 < comp2) {
		pdata->order = -1;/*delay ds*/
		base_tmp = get_base_line(pdata->align[0]);
	} else {
		pdata->order = 0;/*ok*/
		pr_info("ds and data0~7 is aligned\n");
		return 0;
	}
	for (i = 0; i < 64; i++) {
		pr_info("++++++++++++++++++++i = %d\n", i);
		pr_info("+++++++++++++++++++++++++++delay1: 0x%x\n",
			sd_emmc_regs->gdelay1);
		pr_info("+++++++++++++++++++++++++++delay2: 0x%x\n",
			sd_emmc_regs->gdelay2);
		if (pdata->order == 1) {
			delay1 += (1<<0)|(1<<6)|(1<<12)|(1<<18)|(1<<24);
			delay2 += (1<<0)|(1<<6)|(1<<12)|(1<<24);
			line_x = 3;
		} else if (pdata->order == -1) {
			delay2 += (1<<18);
			line_x = 8;
		}
		sd_emmc_regs->gdelay1 = delay1;
		sd_emmc_regs->gdelay2 = delay2;
		emmc_eyetest_log(mmc, 3);
		emmc_eyetest_log(mmc, 8);
		result = compare_line_alignment(base_tmp, pdata->align[line_x]);
		if (result)
			break;
	}
	if (i == 64) {
		pr_warn("%s [%d] Don't find line delay which aligned with DS\n",
				__func__, __LINE__);
		return 1;
	}
	kfree(blk_test);
	return 0;
}
#endif

/*  ds - data = 0.5ns, so delay data
 *  then turn on ds auto
 */
#if 0
static int emmc_ds_auto_sht(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	struct sd_emmc_regs *sd_emmc_regs =
		(struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	u32 intf3 = sd_emmc_regs->intf3;
	struct intf3 *gintf3 = (struct intf3 *)&(intf3);
	u32 delay1 = sd_emmc_regs->gdelay1;
	u32 delay2 = sd_emmc_regs->gdelay2;
	u32 tmp;

	u8 *blk_test = NULL;
	u32 blksz = 512;
	int i;
	blk_test = kmalloc(blksz * CALI_BLK_CNT, GFP_KERNEL);
	if (!blk_test)
		return -ENOMEM;

	for (line_x = 0; line_x < 10; line_x++)
		emmc_eyetest_log(mmc, line_x);


	gintf3->ds_sht_exp = 0;

	sd_emmc_regs->intf3 = intf3;
	pr_warn("%s [%d] intf3: 0x%x\n",
			__func__, __LINE__, sd_emmc_regs->intf3);

	delay2 += (13<<18);
	sd_emmc_regs->gdelay2 = delay2;

	for (i = 0; i < 64; i++) {
		tmp = 1;
		pr_warn("=============%s [%d] 0.5ns equal to i: %u\n",
			__func__, __LINE__, i);
		delay1 += (tmp<<0)|(tmp<<6)|
			(tmp<<12)|(tmp<<18)|(tmp<<24);
		delay2 += (tmp<<0)|(tmp<<6)|
			(tmp<<12)|(tmp<<24);

		sd_emmc_regs->gdelay1 = delay1;
		sd_emmc_regs->gdelay2 = delay2;
		pr_warn("[%s] [%d] ===========delay1: 0x%x=====delay2: 0x%x\n",
				__func__, __LINE__,
				sd_emmc_regs->gdelay1, sd_emmc_regs->gdelay2);
		emmc_eyetest_log(mmc, 8);
		aml_sd_emmc_cali_v3(mmc,
			MMC_READ_MULTIPLE_BLOCK,
			blk_test, blksz, 20);
	}
	return 0;
}
#endif

#if 0
static int emmc_test_ds_delay(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	struct sd_emmc_regs_v3 *sd_emmc_regs =
		(struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	/*u32 delay1 = sd_emmc_regs->reg.v3.gdelay1;
	struct sd_emmc_delay1 *gdelay1 = (struct sd_emmc_delay1 *)&delay1;*/
	u32 delay2 = sd_emmc_regs->gdelay2;
	u32 adjust = sd_emmc_regs->gadjust;
	struct sd_emmc_adjust *gadjust = (struct sd_emmc_adjust *)&adjust;
	u32 eyetest_out0 = 0, eyetest_out1 = 0;
	u32 intf3 = sd_emmc_regs->intf3;
	struct intf3 *gintf3 = (struct intf3 *)&(intf3);
	int i;
	u64 temp;
	u32 blksz = 512;
	u8 *blk_test;
		blk_test = kmalloc(blksz * CALI_BLK_CNT, GFP_KERNEL);
	if (!blk_test)
		return -ENOMEM;
	line_x = 8;
	for (i = 0; i < 8; i++) {
		emmc_dbg(EMMC_TIMMING_DBG, "++++++++++++++++++++i = %d\n", i);
		emmc_dbg(EMMC_TIMMING_DBG, "++++++++++++++++++++delay2: 0x%x\n",
			sd_emmc_regs->gdelay2);
		temp = 0;
		gadjust->cali_enable = 1;
		gadjust->cali_sel = line_x;
		sd_emmc_regs->gadjust = adjust;
		gintf3->eyetest_exp = 9;
		gintf3->eyetest_on = 1;
		sd_emmc_regs->intf3 = intf3;
		delay2 &= ~(0x3f << (6 * (line_x - 5)));
		delay2 |= i << (6 * (line_x - 5));
		sd_emmc_regs->gdelay2 = delay2;
		/*pr_info("+++++++++++++++++++++++++++delay2: 0x%x\n",
			sd_emmc_regs->gdelay2);*/
		aml_sd_emmc_cali_v3(mmc,
			MMC_READ_MULTIPLE_BLOCK,
			blk_test, blksz, 20);
		emmc_eyetest_log(mmc, 8);
		emmc_eyetest_log(mmc, 1);
		eyetest_out0 = sd_emmc_regs->eyetest_out0;
		eyetest_out1 = 0;
		gintf3->eyetest_on = 0;
		sd_emmc_regs->intf3 = intf3;
		temp = ((temp | eyetest_out1) << 32) | eyetest_out0;
		emmc_dbg(EMMC_TIMMING_DBG,
			"++++++++++++++ eyetestout 0x%llx............\n", temp);
	}
	kfree(blk_test);
	return 0;
}
#endif

#if 0
static int emmc_test_ds_sht(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	struct sd_emmc_regs_v3 *sd_emmc_regs =
		(struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	/*u32 delay1 = sd_emmc_regs->reg.v3.gdelay1;
	struct sd_emmc_delay1 *gdelay1 = (struct sd_emmc_delay1 *)&delay1;
	u32 delay2 = sd_emmc_regs->reg.v3.gdelay2;
	struct sd_emmc_delay2 *gdelay2 = (struct sd_emmc_delay2 *)&delay2;*/
	u32 adjust = sd_emmc_regs->gadjust;
	struct sd_emmc_adjust *gadjust = (struct sd_emmc_adjust *)&adjust;
	u32 intf3 = sd_emmc_regs->intf3;
	struct intf3 *gintf3 = (struct intf3 *)&(intf3);
	int i;
	u32 blksz = 512;
	u8 *blk_test;
		blk_test = kmalloc(blksz * CALI_BLK_CNT, GFP_KERNEL);
	if (!blk_test)
		return -ENOMEM;
	line_x = 8;
	/*pr_info("+++++++++++++++++++++++++++delay2: 0x%x\n",
			sd_emmc_regs->gdelay2);*/
	for (i = 0; i < 64; i++) {
		emmc_dbg(EMMC_TIMMING_DBG, "++++++++++++++++++++i = %d\n", i);
		gadjust->cali_enable = 0;
		gadjust->cali_sel = line_x;
		sd_emmc_regs->gadjust = adjust;
		gintf3->ds_sht_m += 1;
		sd_emmc_regs->intf3 = intf3;
		emmc_dbg(EMMC_TIMMING_DBG, "+++++++++++++++++++++++++++intf3: 0x%x\n",
			sd_emmc_regs->intf3);
		aml_sd_emmc_cali_v3(mmc,
			MMC_READ_MULTIPLE_BLOCK,
			blk_test, blksz, 20);
	    emmc_eyetest_log(mmc, 8);
	}
	kfree(blk_test);
	return 0;
}
#endif

#if 0
static int emmc_test_line_sht(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	struct sd_emmc_regs_v3 *sd_emmc_regs =
		(struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	u32 delay1 = sd_emmc_regs->gdelay1;
	u32 delay2 = sd_emmc_regs->gdelay2;
	/*struct sd_emmc_delay2 *gdelay2 = (struct sd_emmc_delay2 *)&delay2;
	u32 adjust = sd_emmc_regs->reg.v3.gadjust;
	struct sd_emmc_adjust *gadjust = (struct sd_emmc_adjust *)&adjust;
	u32 intf3 = sd_emmc_regs->reg.v3.intf3;
	struct intf3 *gintf3 = (struct intf3 *)&(intf3);*/
	int i;
	u32 blksz = 512;
	u8 *blk_test;
		blk_test = kmalloc(blksz * CALI_BLK_CNT, GFP_KERNEL);
	if (!blk_test)
		return -ENOMEM;
	line_x = 8;
	/*pr_info("+++++++++++++++++++++++++++delay2: 0x%x\n",
			sd_emmc_regs->gdelay2);*/
	for (i = 0; i < 64; i++) {
		pr_info("++++++++++++++++++++i = %d\n", i);
		pr_info("+++++++++++++++++++++++++++delay1: 0x%x\n",
			sd_emmc_regs->gdelay1);
		pr_info("+++++++++++++++++++++++++++delay2: 0x%x\n",
			sd_emmc_regs->gdelay2);
		delay1 += (1<<0)|(1<<6)|(1<<12)|(1<<18)|(1<<24);
		delay2 += (1<<0)|(1<<6)|(1<<12);
		sd_emmc_regs->gdelay1 = delay1;
		sd_emmc_regs->gdelay2 = delay2;
		aml_sd_emmc_cali_v3(mmc,
			MMC_READ_MULTIPLE_BLOCK,
			blk_test, blksz, 20);
		emmc_eyetest_log(mmc, 0);
		emmc_eyetest_log(mmc, 8);
	}
	kfree(blk_test);
	return 0;
}
#endif

int aml_emmc_hs200_timming(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	struct sd_emmc_regs_v3 *sd_emmc_regs =
			(struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	u32 adjust = sd_emmc_regs->gadjust;
	struct sd_emmc_adjust *gadjust = (struct sd_emmc_adjust *)&adjust;
	emmc_clktest(mmc);
	print_all_line_eyetest(mmc);
	emmc_all_data_line_alignment(mmc);
	gadjust->cali_enable = 1;
	gadjust->adj_auto = 1;
	sd_emmc_regs->gadjust = adjust;
	return 0;
}

static int _aml_sd_emmc_execute_tuning(struct mmc_host *mmc, u32 opcode,
					struct aml_tuning_data *tuning_data,
					u32 adj_win_start)
{
#if 1 /* need finish later */
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	struct sd_emmc_regs_v3 *sd_emmc_regs =
		(struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	u32 vclk;
	struct sd_emmc_clock_v3 *clkc = (struct sd_emmc_clock_v3 *)&(vclk);
	u32 vctrl;
	struct sd_emmc_config *ctrl = (struct sd_emmc_config *)&vctrl;
	u32 adjust = sd_emmc_regs->gadjust;
	struct sd_emmc_adjust_v3 *gadjust =
		(struct sd_emmc_adjust_v3 *)&adjust;
	u32 clk_rate = 1000000000;
	const u8 *blk_pattern = tuning_data->blk_pattern;
	unsigned int blksz = tuning_data->blksz;
	unsigned long flags;
	int ret = 0;
	u32 nmatch = 0;
	int adj_delay = 0;
	u8 *blk_test;
	u8 tuning_num = 0;
	u32 clock, clk_div;
	u32 adj_delay_find;
	int wrap_win_start = -1, wrap_win_size = 0;
	int best_win_start = -1, best_win_size = 0;
	int curr_win_start = -1, curr_win_size = 0;
	sd_emmc_regs->gadjust = 0;

tunning:
	spin_lock_irqsave(&host->mrq_lock, flags);
	pdata->need_retuning = false;
	spin_unlock_irqrestore(&host->mrq_lock, flags);
	vclk = sd_emmc_regs->gclock;
	vctrl = sd_emmc_regs->gcfg;
	clk_div = clkc->div;
	clock = clk_rate / clk_div;/*200MHz, bus_clk */
	pdata->mmc->actual_clock = ctrl->ddr ?
				(clock / 2) : clock;/*100MHz in ddr */

	if (ctrl->ddr == 1) {
		blksz = 512;
		opcode = 17;
	}
	blk_test = kmalloc(blksz, GFP_KERNEL);
	if (!blk_test)
		return -ENOMEM;

	host->is_tunning = 1;
	pr_info("%s: clk %d %s tuning start\n",
		mmc_hostname(mmc), (ctrl->ddr ? (clock / 2) : clock),
			(ctrl->ddr ? "DDR mode" : "SDR mode"));
	for (adj_delay = 0; adj_delay < clk_div; adj_delay++) {
		gadjust->adj_delay = adj_delay;
		gadjust->adj_enable = 1;
		gadjust->cali_enable = 0;
		gadjust->cali_rise = 0;
		sd_emmc_regs->gadjust = adjust;
		nmatch = aml_sd_emmc_tuning_transfer(mmc, opcode,
				blk_pattern, blk_test, blksz);
		/*get a ok adjust point!*/
		if (nmatch == TUNING_NUM_PER_POINT) {
			if (adj_delay == 0)
				wrap_win_start = adj_delay;

			if (wrap_win_start >= 0)
				wrap_win_size++;

			if (curr_win_start < 0)
				curr_win_start = adj_delay;

			curr_win_size++;
			pr_info("%s: rx_tuning_result[%d] = %d\n",
				mmc_hostname(host->mmc), adj_delay, nmatch);
		} else {
			if (curr_win_start >= 0) {
				if (best_win_start < 0) {
					best_win_start = curr_win_start;
					best_win_size = curr_win_size;
				} else {
					if (best_win_size < curr_win_size) {
						best_win_start = curr_win_start;
						best_win_size = curr_win_size;
					}
				}

				wrap_win_start = -1;
				curr_win_start = -1;
				curr_win_size = 0;
			}
		}

	}
	/* last point is ok! */
	if (curr_win_start >= 0) {
		if (best_win_start < 0) {
			best_win_start = curr_win_start;
			best_win_size = curr_win_size;
		} else if (wrap_win_size > 0) {
			/* Wrap around case */
			if (curr_win_size + wrap_win_size > best_win_size) {
				best_win_start = curr_win_start;
				best_win_size = curr_win_size + wrap_win_size;
			}
		} else if (best_win_size < curr_win_size) {
			best_win_start = curr_win_start;
			best_win_size = curr_win_size;
		}

		curr_win_start = -1;
		curr_win_size = 0;
	}
	if (best_win_size <= 0) {
		if ((tuning_num++ > MAX_TUNING_RETRY)
			|| (clkc->div >= 10)) {
			kfree(blk_test);
			pr_info("%s: final result of tuning failed\n",
				 mmc_hostname(host->mmc));
			return -1;
		}
		clkc->div += 1;
		sd_emmc_regs->gclock = vclk;
		pdata->clkc = sd_emmc_regs->gclock;
		pr_info("%s: tuning failed, reduce freq and retuning\n",
			mmc_hostname(host->mmc));
		goto tunning;
	} else {
		pr_info("%s: best_win_start =%d, best_win_size =%d\n",
			mmc_hostname(host->mmc), best_win_start, best_win_size);
	}

	if ((best_win_size != clk_div)
		|| (aml_card_type_sdio(pdata)
			&& (get_cpu_type() == MESON_CPU_MAJOR_ID_GXM))) {
		adj_delay_find = best_win_start + (best_win_size - 1) / 2
						+ (best_win_size - 1) % 2;
		adj_delay_find = adj_delay_find % clk_div;
	} else
		adj_delay_find = 0;

	/* fixme, for retry debug. */
	if (aml_card_type_mmc(pdata)
		&& (clk_div <= 5) && (adj_win_start != 100)
		&& (get_cpu_type() == MESON_CPU_MAJOR_ID_GXBB)) {
		pr_info("%s: adj_win_start %d\n",
			mmc_hostname(host->mmc), adj_win_start);
		adj_delay_find = adj_win_start % clk_div;
	}
	gadjust->adj_delay = adj_delay_find;
	gadjust->adj_enable = 1;
	gadjust->cali_enable = 0;
	gadjust->cali_rise = 0;
	sd_emmc_regs->gadjust = adjust;
	host->is_tunning = 0;

	pr_info("%s: sd_emmc_regs->gclock=0x%x,sd_emmc_regs->gadjust=0x%x\n",
			mmc_hostname(host->mmc), sd_emmc_regs->gclock,
			sd_emmc_regs->gadjust);
	kfree(blk_test);
	/* do not dynamical tuning for no emmc device */
	if ((pdata->is_in) && !aml_card_type_mmc(pdata))
		schedule_delayed_work(&pdata->retuning, 15*HZ);
	return ret;
#endif
	return 0;
}
int aml_sd_emmc_execute_tuning_v3(struct mmc_host *mmc, u32 opcode)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	struct sd_emmc_regs_v3 *sd_emmc_regs =
			(struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	struct aml_tuning_data tuning_data;
	/*u32 vclk = sd_emmc_regs->gclock;
	struct sd_emmc_clock *clkc = (struct sd_emmc_clock *)&(vclk);*/
	/*u32 adjust = sd_emmc_regs->gadjust;
	struct sd_emmc_adjust *gadjust = (struct sd_emmc_adjust *)&adjust;*/
	int err = -ENOSYS;
	u32 adj_win_start = 100;

	if (opcode == MMC_SEND_TUNING_BLOCK_HS200) {
		if (mmc->ios.bus_width == MMC_BUS_WIDTH_8) {
			tuning_data.blk_pattern = tuning_blk_pattern_8bit;
			tuning_data.blksz = sizeof(tuning_blk_pattern_8bit);
		} else if (mmc->ios.bus_width == MMC_BUS_WIDTH_4) {
			tuning_data.blk_pattern = tuning_blk_pattern_4bit;
			tuning_data.blksz = sizeof(tuning_blk_pattern_4bit);
		} else {
			return -EINVAL;
		}
	} else if (opcode == MMC_SEND_TUNING_BLOCK) {
		tuning_data.blk_pattern = tuning_blk_pattern_4bit;
		tuning_data.blksz = sizeof(tuning_blk_pattern_4bit);
	} else {
		sd_emmc_err("Undefined command(%d) for tuning\n", opcode);
		return -EINVAL;
	}

	if (aml_card_type_sdio(pdata))
		err = _aml_sd_emmc_execute_tuning(mmc, opcode,
			&tuning_data, adj_win_start);
	else if (!(pdata->caps2 & MMC_CAP2_HS400)) {
		sd_emmc_regs->intf3 |= (1 << 22);
		err = aml_emmc_hs200_timming(mmc);
	} else {
		sd_emmc_regs->intf3 |= (1 << 22);
		err = 0;
	}

	pr_info("%s: gclock=0x%x, gdelay1=0x%x, gdelay2=0x%x,intf3=0x%x\n",
		mmc_hostname(mmc), sd_emmc_regs->gclock,
		sd_emmc_regs->gdelay1, sd_emmc_regs->gdelay2,
		sd_emmc_regs->intf3);
	return err;
}

ssize_t emmc_eyetest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct amlsd_host *host = dev_get_drvdata(dev);
	struct mmc_host *mmc = host->mmc;
	mmc_claim_host(mmc);
	print_all_line_eyetest(mmc);
	mmc_release_host(mmc);
	return sprintf(buf, "%s\n", "Emmc all lines eyetest.\n");
}

ssize_t emmc_clktest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct amlsd_host *host = dev_get_drvdata(dev);
	struct mmc_host *mmc = host->mmc;
	struct sd_emmc_regs_v3 *sd_emmc_regs =
		(struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	u32 intf3 = sd_emmc_regs->intf3;
	struct intf3 *gintf3 = (struct intf3 *)&(intf3);
	u32 clktest = 0, clktest_log = 0;
	mmc_claim_host(mmc);
	gintf3->clktest_exp = 8;
	gintf3->clktest_on_m = 1;
	sd_emmc_regs->intf3 = intf3;
	sd_emmc_regs->gcfg &= ~(1 << 23);

	clktest_log = sd_emmc_regs->clktest_log;
	clktest = sd_emmc_regs->clktest_out;
	while (!(clktest_log & 0x80000000)) {
		mdelay(1);
		clktest_log = sd_emmc_regs->clktest_log;
		clktest = sd_emmc_regs->clktest_out;
	}
	gintf3->clktest_on_m = 0;
	sd_emmc_regs->intf3 = intf3;
	sd_emmc_regs->gcfg |= (1 << 23);
	mmc_release_host(mmc);
	pr_info("%s [%d] clktest : 0x%x\n", __func__, __LINE__, clktest);
	return sprintf(buf, "%s\n", "Emmc all lines clktest.\n");
}
