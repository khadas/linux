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

static struct mmc_claim aml_sd_emmc_claim;
void aml_sd_emmc_reg_init_v3(struct amlsd_host *host)
{
	u32 vclkc = 0;
	struct sd_emmc_clock *pclkc = (struct sd_emmc_clock *)&vclkc;
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
	pclkc->core_phase = 2;	  /* 2: 180 phase */
	pclkc->rx_phase = 0;
	pclkc->tx_phase = 0;
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
	if (err) {
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
			mrq->cmd->retries = AML_ERROR_RETRY_COUNTER;
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

static void aml_sd_emmc_set_clk_rate_v3(struct mmc_host *mmc,
			unsigned int clk_ios)
{
	u32 clk_rate, clk_div, clk_src_sel;
	unsigned long flags;
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = (void *)pdata->host;
	struct sd_emmc_regs_v3 *sd_emmc_regs
		= (struct sd_emmc_regs_v3 *)host->sd_emmc_regs;

	if (clk_ios == 0) {
		aml_sd_emmc_clk_switch_off(host);
		return;
	}

	clk_src_sel = SD_EMMC_CLOCK_SRC_OSC;
	if (clk_ios < 20000000)
		clk_src_sel = SD_EMMC_CLOCK_SRC_OSC;
	else
		clk_src_sel = SD_EMMC_CLOCK_SRC_FCLK_DIV2;

	if (clk_ios > pdata->f_max)
		clk_ios = pdata->f_max;
	if (clk_ios < pdata->f_min)
		clk_ios = pdata->f_min;

	WARN_ON(clk_src_sel > SD_EMMC_CLOCK_SRC_FCLK_DIV2);

	switch (clk_src_sel) {
	case SD_EMMC_CLOCK_SRC_OSC:
		clk_rate = 24000000;
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

	aml_sd_emmc_clk_switch(pdata, clk_div, clk_src_sel);

	sd_emmc_dbg(AMLSD_DBG_CLKC, "sd_emmc_regs->gclock = 0x%x\n",
		sd_emmc_regs->gclock);
	pdata->clkc = sd_emmc_regs->gclock;

	pdata->mmc->actual_clock = clk_rate / clk_div;

	/*Wait for a while after clock setting*/
	/* udelay(100); */

	spin_unlock_irqrestore(&host->mrq_lock, flags);

	return;
}


static void aml_sd_emmc_set_timing_v3(struct amlsd_platform *pdata,
				u32 timing)
{
	struct amlsd_host *host = (void *)pdata->host;
	struct sd_emmc_regs_v3 *sd_emmc_regs
		= (struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	u32 vctrl = sd_emmc_regs->gcfg;
	struct sd_emmc_config *ctrl = (struct sd_emmc_config *)&vctrl;
	u32 vclkc = sd_emmc_regs->gclock;
	struct sd_emmc_clock *clkc = (struct sd_emmc_clock *)&vclkc;
	u32 adjust = sd_emmc_regs->gadjust;
	struct sd_emmc_adjust *gadjust = (struct sd_emmc_adjust *)&adjust;
	u8 clk_div;
	u32 clk_rate = 1000000000;
	if ((timing == MMC_TIMING_MMC_HS400) ||
			 (timing == MMC_TIMING_MMC_DDR52) ||
				 (timing == MMC_TIMING_UHS_DDR50)) {
		if (timing == MMC_TIMING_MMC_HS400) {
			ctrl->chk_ds = 1;
			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL) {
				adjust = sd_emmc_regs->gadjust;
				gadjust->ds_enable = 1;
				sd_emmc_regs->gadjust = adjust;
				host->tuning_mode = AUTO_TUNING_MODE;
			}
		}
		ctrl->ddr = 1;
		clk_div = clkc->div;
		if (clk_div & 0x01)
			clk_div++;
			clkc->div = clk_div / 2;
		sd_emmc_regs->gclock = vclkc;
		pdata->clkc = sd_emmc_regs->gclock;
		pdata->mmc->actual_clock = clk_rate / clk_div;
		pr_info("%s: try set sd/emmc to DDR mode\n",
			mmc_hostname(host->mmc));

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

int aml_sd_emmc_execute_tuning_v3(struct mmc_host *mmc, u32 opcode)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	struct sd_emmc_regs_v3 *sd_emmc_regs =
			(struct sd_emmc_regs_v3 *)host->sd_emmc_regs;
	struct aml_tuning_data tuning_data;
	/*u32 vclk = sd_emmc_regs->gclock;
	struct sd_emmc_clock *clkc = (struct sd_emmc_clock *)&(vclk);
	u32 adjust = sd_emmc_regs->gadjust;
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

	err = aml_sd_emmc_execute_tuning_(mmc, opcode,
			&tuning_data, adj_win_start);

	pr_info("%s: gclock =0x%x, gdelay1=0x%x, gdelay2=0x%x,gadjust=0x%x\n",
		mmc_hostname(mmc), sd_emmc_regs->gclock,
		sd_emmc_regs->gdelay1, sd_emmc_regs->gdelay2,
		sd_emmc_regs->gadjust);
	return err;
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

