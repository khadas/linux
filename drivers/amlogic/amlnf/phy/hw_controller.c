/*
 * drivers/amlogic/amlnf/phy/hw_controller.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
*/

#include "../include/phynand.h"

static int controller_select_chip(struct hw_controller *controller,
	unsigned char chipnr)
{
	int  ret = 0;

	switch (chipnr) {
	case 0:
	case 1:
	case 2:
	case 3:
		controller->chip_selected = controller->ce_enable[chipnr];
		controller->rb_received = controller->rb_enable[chipnr];
		NFC_SEND_CMD_IDLE(controller, 0);
		break;
	default:
		BUG();
		controller->chip_selected = CE_NOT_SEL;
		ret = -NAND_SELECT_CHIP_FAILURE;
		aml_nand_msg("failed");
		break;
	}
	return ret;
}

#ifdef AML_NAND_DMA_POLLING
static struct completion controller_dma_completion;

static enum hrtimer_restart controller_dma_timerfuc(struct hrtimer *timer)
{
	struct hw_controller *controller = NULL;
	unsigned int fifo_cnt = 0;

	controller = container_of(timer, struct hw_controller, timer);
	fifo_cnt = NFC_CMDFIFO_SIZE(controller);

	/* */
	smp_rmb();
	/* */
	smp_wmb();
	if (fifo_cnt == 0)
		complete(&controller_dma_completion);
	else
		hrtimer_start(&controller->timer,
			ktime_set(0, DMA_TIME_CNT_20US),
			HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}

static int controller_dma_timer_handle(struct hw_controller *controller)
{
	struct amlnand_chip *aml_chip = controller->aml_chip;
	struct nand_flash *flash = &(aml_chip->flash);
	int timeout, time_start;

	time_start = (flash->pagesize + flash->oobsize)*50+5000;
	init_completion(&controller_dma_completion);
	hrtimer_start(&controller->timer,
		ktime_set(0, time_start),
		HRTIMER_MODE_REL);

	/* max 500mS */
	timeout = wait_for_completion_timeout(&controller_dma_completion, 150);
	if (timeout == 0) {
		aml_nand_msg("***[nand] dma irq timeout");
		return -NAND_BUSY_FAILURE;
	}
	return 0;
}
#endif /* AML_NAND_DMA_POLLING */

#ifdef AML_NAND_RB_IRQ
/* static struct completion rb_complete; */
DECLARE_COMPLETION(rb_complete);

void controller_open_interrupt(struct hw_controller *controller)
{
	/* NFC_ENABLE_STS_IRQ(); */
	NFC_ENABLE_IO_IRQ(controller);

}

void controller_close_interrupt(struct hw_controller *controller)
{
	/* NFC_DISABLE_STS_IRQ(); */
	NFC_DISABLE_IO_IRQ(controller);
}

static irqreturn_t controller_interrupt_monitor(int irq,
	void *dev_id)
{
	struct hw_controller *controller = (struct hw_controller *)dev_id;

	controller_close_interrupt(controller);
	complete(&rb_complete);

	return IRQ_HANDLED;
}

static int controller_queue_rb_irq(struct hw_controller *controller,
	unsigned char chipnr)
{
	int ret = 0, timeout = 0;

	if (chipnr != NAND_CHIP_UNDEFINE)/* skip dma operation */
		controller->select_chip(controller, chipnr);

	reinit_completion(&rb_complete);
	controller_open_interrupt(controller);
	NFC_SEND_CMD_IDLE(controller, NAND_TWB_TIME_CYCLE);
	controller->cmd_ctrl(controller, NAND_CMD_STATUS, NAND_CTRL_CLE);
	NFC_SEND_CMD_IDLE(controller, NAND_TWB_TIME_CYCLE);
	/* */
	smp_rmb();
	/* */
	smp_wmb();

	NFC_SEND_CMD_RB_IRQ(controller, 18);
	/* NFC_SEND_CMD_IDLE(controller, NAND_TWB_TIME_CYCLE); */

	timeout = wait_for_completion_timeout(&rb_complete, 800);
	if (timeout == 0) {
		aml_nand_msg("***[nand] rb irq timeout");
		ret = -NAND_BUSY_FAILURE;
	}

	controller_close_interrupt(controller);
	return ret;
}
#endif /* AML_NAND_RB_IRQ */
static int controller_quene_rb(struct hw_controller *controller,
	unsigned char chipnr)
{
	unsigned int time_out_limit, time_out_cnt = 0;
	struct amlnand_chip *aml_chip = controller->aml_chip;
	int ret = 0;

	if (aml_chip->state == CHIP_RESETING)
		time_out_limit = AML_NAND_ERASE_BUSY_TIMEOUT;
	else if (aml_chip->state == CHIP_WRITING)
		time_out_limit = AML_NAND_WRITE_BUSY_TIMEOUT;
	else
		time_out_limit = AML_NAND_READ_BUSY_TIMEOUT;

	controller->select_chip(controller, chipnr);

	NFC_SEND_CMD_IDLE(controller, 0);
	NFC_SEND_CMD_IDLE(controller, 0);
	while (NFC_CMDFIFO_SIZE(controller) > 0)
		;

#if 0
	NFC_SEND_CMD_RB(controller, aml_chip->chip_enable, 20);
	NFC_SEND_CMD_IDLE(controller, 0);
	NFC_SEND_CMD_IDLE(controller, 0);
	do {
		if (NFC_CMDFIFO_SIZE(controller) <= 0)
			break;
	} while (time_out_cnt++ <= AML_DMA_BUSY_TIMEOUT);
#endif

	/* udelay(2); */
	if (controller->option & NAND_CTRL_NONE_RB) {
		controller->cmd_ctrl(controller,
			NAND_CMD_STATUS, NAND_CTRL_CLE);
		/*
		aml_nand_dbg("controller->chip_selected =%d",
			controller->chip_selected);
		*/
		NFC_SEND_CMD_IDLE(controller, NAND_TWHR_TIME_CYCLE);
		do {
			/* udelay(chip->chip_delay); */
			if ((int)controller->readbyte(controller) &
				NAND_STATUS_READY)
				break;
			udelay(1);
		} while (time_out_cnt++ <= time_out_limit);/* 200ms max */
	} else {
		do {
			if (NFC_GET_RB_STATUS(controller,
				controller->rb_received))
				break;
			udelay(2);
		} while (time_out_cnt++ <= time_out_limit);
	}

	if (time_out_cnt >=  time_out_limit) {
		/*dbg code here!*/
#if 0
		dump_pinmux_regs(controller);
#endif
		ret = -NAND_BUSY_FAILURE;
	}

	/*
	fixme: Add standby here, it means release ce;avoid influence between
	two operations nearby. it have been found that when have finished
	reading one plane page and rb is checked ready,but before reading next
	plane, check rb busy.
	*/
	/* delay for 5 cycle. */
	NFC_SEND_CMD_STANDBY(controller, 5);

	return ret;
}

static int controller_hwecc_correct(struct hw_controller *controller,
	unsigned int size,
	unsigned char *oob_buf)
{
	unsigned int ecc_step_num, cur_ecc, usr_info;
	unsigned int info_times_int_len = PER_INFO_BYTE/sizeof(unsigned int);
	struct amlnand_chip *aml_chip = controller->aml_chip;
	int max_ecc = 0;
	int user_offset = 0;
	unsigned int tmp_value;

	if (controller->oob_mod == 1)
		user_offset = 4;

	if (size % controller->ecc_unit) {
		aml_nand_msg("err para size for ecc correct %x,and ecc_unit:%x",
			size,
			controller->ecc_unit);
		return -NAND_ARGUMENT_FAILURE;
	}

	controller->ecc_cnt_cur = 0;
	 for (ecc_step_num = 0;
		ecc_step_num < (size / controller->ecc_unit);
		ecc_step_num++) {
		/* check if there have uncorrectable sector */
		tmp_value = ecc_step_num*info_times_int_len + user_offset;
		usr_info = (*(u32 *)(&(controller->user_buf[tmp_value])));
		cur_ecc = NAND_ECC_CNT(usr_info);
		/*
		aml_nand_dbg("uncorrected for cur_ecc:%d, usr_buf[%d]:%x",
			cur_ecc,
			ecc_step_num,
			usr_info);
		*/
		if (cur_ecc == 0x3f) {
			controller->zero_cnt = NAND_ZERO_CNT(usr_info);
			if (max_ecc < controller->zero_cnt)
				max_ecc =  controller->zero_cnt;
			/*
			aml_nand_dbg("uncorrected for ecc_step_num:%d,
				zero_cnt:%d",
				ecc_step_num,
				controller->zero_cnt);
			*/
			return NAND_ECC_FAILURE;
		} else {
			controller->ecc_cnt_cur =
				(controller->ecc_cnt_cur > cur_ecc) ?
				controller->ecc_cnt_cur : cur_ecc;
			if (max_ecc < controller->ecc_cnt_cur)
				max_ecc =  controller->ecc_cnt_cur;
		}
	}
	aml_chip->max_ecc_per_page = max_ecc;
	return 0;
}

/* default enable ran mode */
static int controller_dma_read(struct hw_controller *controller,
	unsigned int len,
	unsigned char bch_mode)
{
	int count, dma_unit_size, info_times_int_len, time_out_cnt, dma_cnt;
	unsigned int *info_buf = 0;
	unsigned int tmp_value;
#if (!AML_CFG_CONHERENT_BUFFER)
	struct amlnand_chip *aml_chip = controller->aml_chip;
	struct nand_flash *flash = &(aml_chip->flash);
#endif
	int ret = NAND_SUCCESS;
	/* volatile int cmp=0; */
	/* int ret = 0; */

	dma_unit_size = 0;
	info_times_int_len = PER_INFO_BYTE/sizeof(unsigned int);
	if (bch_mode == NAND_ECC_NONE) {
		if (len > 0x3fff)
			len = 0x3ffe;
		count = 1;
	} else if (bch_mode == NAND_ECC_BCH_SHORT) {
		dma_unit_size = (controller->ecc_unit >> 3);
		count = len/controller->ecc_unit;
	} else
		count = controller->ecc_steps;

	dma_cnt = count;
	if ((controller->oob_mod == 1) && (bch_mode != NAND_ECC_NONE))
		count += 16 / PER_INFO_BYTE;

	tmp_value = (count-1)*info_times_int_len;
	info_buf = (unsigned int *)&(controller->user_buf[tmp_value]);
	memset((unsigned char *)controller->user_buf, 0, count*PER_INFO_BYTE);
#if (!AML_CFG_CONHERENT_BUFFER)
	controller->data_dma_addr =
		dma_map_single(aml_chip->device, controller->data_buf,
		(flash->pagesize + flash->oobsize),
		DMA_FROM_DEVICE);
#endif
	/* */
	smp_wmb();
	/* */
	wmb();

	/* while(NFC_CMDFIFO_SIZE() > 10); */
	NFC_SEND_CMD_ADL(controller, controller->data_dma_addr);
	NFC_SEND_CMD_ADH(controller, controller->data_dma_addr);
	NFC_SEND_CMD_AIL(controller, controller->info_dma_addr);
	NFC_SEND_CMD_AIH(controller, controller->info_dma_addr);

	/* setting page_addr used for seed */
	NFC_SEND_CMD_SEED(controller, controller->page_addr);

	if (bch_mode == NAND_ECC_NONE)
		NFC_SEND_CMD_N2M_RAW(controller, controller->ran_mode, len);
	else
		NFC_SEND_CMD_N2M(controller, controller->ran_mode,
		((bch_mode == NAND_ECC_BCH_SHORT)?NAND_ECC_BCH60_1K:bch_mode),
		((bch_mode == NAND_ECC_BCH_SHORT)?1:0), dma_unit_size, dma_cnt);

#if 0
	NFC_SEND_CMD_STS(controller, 20, 2);
#else
#ifdef AML_NAND_DMA_POLLING
	ret = controller_dma_timer_handle(controller);
#if 0   /* irq failed here */
	ret = controller_queue_rb_irq(controller, NAND_CHIP_UNDEFINE);
#endif
	if (ret) {
		time_out_cnt = AML_DMA_BUSY_TIMEOUT;
		aml_nand_msg("dma timeout here");
		ret = -NAND_DMA_FAILURE;
		goto _exit;
	}
#else
	NFC_SEND_CMD_IDLE(controller, 0);
	NFC_SEND_CMD_IDLE(controller, 0);

	time_out_cnt = 0;

	do {
		if (NFC_CMDFIFO_SIZE(controller) <= 0)
			break;
	} while (time_out_cnt++ <= AML_DMA_BUSY_TIMEOUT);

	if (time_out_cnt >= AML_DMA_BUSY_TIMEOUT) {
		aml_nand_msg("dma timeout here");
		ret = -NAND_DMA_FAILURE;
		goto _exit;
	}
#endif

#endif
	do {
		/* */
		smp_rmb();
	} while (NAND_INFO_DONE(*info_buf) == 0);
	/* */
	smp_wmb();
	/* */
	wmb();
#if (!AML_CFG_CONHERENT_BUFFER)
	dma_unmap_single(aml_chip->device, controller->data_dma_addr,
		(flash->pagesize + flash->oobsize),
		DMA_FROM_DEVICE);
#endif
	/*
	aml_nand_dbg("len:%d, count:%d, bch_mode:%d\n",
		len,
		count,
		bch_mode);
	*/
_exit:
	return ret;
}

static int controller_dma_write(struct hw_controller *controller,
	unsigned char *buf,
	unsigned int len,
	unsigned char bch_mode)
{
	int ret = 0, time_out_cnt = 0, oob_fill_cnt = 0;
	unsigned int dma_unit_size = 0, count = 0;
#if (!AML_CFG_CONHERENT_BUFFER)
	struct amlnand_chip *aml_chip = controller->aml_chip;
	struct nand_flash *flash = &(aml_chip->flash);
#endif

	if (bch_mode == NAND_ECC_NONE) {
		if (len > 0x3fff)
			len = 0x3ffe;
		count = 1;
	} else if (bch_mode == NAND_ECC_BCH_SHORT) {
		dma_unit_size = (controller->ecc_unit >> 3);
		count = len / controller->ecc_unit;
	} else
		count = controller->ecc_steps;

	memcpy(controller->data_buf, buf, len);
#if (!AML_CFG_CONHERENT_BUFFER)
	controller->data_dma_addr =
		dma_map_single(aml_chip->device, controller->data_buf,
		(flash->pagesize + flash->oobsize),
		DMA_TO_DEVICE);
#endif
	/* */
	smp_wmb();
	/* */
	wmb();

	NFC_SEND_CMD_ADL(controller, controller->data_dma_addr);
	NFC_SEND_CMD_ADH(controller, controller->data_dma_addr);
	NFC_SEND_CMD_AIL(controller, controller->info_dma_addr);
	NFC_SEND_CMD_AIH(controller, controller->info_dma_addr);
	NFC_SEND_CMD_SEED(controller, controller->page_addr);

	/* fixme, DEBUG CODE. send a idle to enable ce here!*/
	/* NFC_SEND_CMD_IDLE(controller, 0); */
	/* 12 cmds */
	if (!bch_mode)
		NFC_SEND_CMD_M2N_RAW(controller, 0, len);
	else
		NFC_SEND_CMD_M2N(controller, controller->ran_mode,
		((bch_mode == NAND_ECC_BCH_SHORT)?NAND_ECC_BCH60_1K:bch_mode),
		((bch_mode == NAND_ECC_BCH_SHORT)?1:0), dma_unit_size, count);

	if (bch_mode == NAND_ECC_BCH_SHORT)
		oob_fill_cnt = controller->oob_fill_boot;
	else if (bch_mode != NAND_ECC_NONE)
		oob_fill_cnt = controller->oob_fill_data;

	if (((bch_mode != NAND_ECC_NONE)) && (oob_fill_cnt > 0))
		/*
		aml_nand_dbg("fill oob controller oob_fill_cnt %d",\
			oob_fill_cnt);
		*/
		NFC_SEND_CMD_M2N_RAW(controller,
			controller->ran_mode,
			oob_fill_cnt);
	else if (bch_mode == NAND_ECC_NONE) {
		uint64_t data64;
		data64 = (uint64_t) controller->data_buf;
		NFC_SEND_CMD_ADL(controller, (int)data64);
		NFC_SEND_CMD_ADH(controller, (int)data64);
		NFC_SEND_CMD_M2N_RAW(controller, 0, controller->oobavail);
	}

#ifdef AML_NAND_DMA_POLLING
	ret = controller_dma_timer_handle(controller);
#if 0/* irq failed here */
	ret = controller_queue_rb_irq(controller, NAND_CHIP_UNDEFINE);
#endif
	if (ret) {
		time_out_cnt = AML_DMA_BUSY_TIMEOUT;
		aml_nand_msg("dma timeout here");
		ret = -NAND_DMA_FAILURE;
		goto _exit;
	}
#else
	NFC_SEND_CMD_IDLE(controller, 0);
	NFC_SEND_CMD_IDLE(controller, 0);

	time_out_cnt = 0;
	do {
		if (NFC_CMDFIFO_SIZE(controller) <= 0)
			break;
	} while (time_out_cnt++ <= AML_DMA_BUSY_TIMEOUT);
#if (!AML_CFG_CONHERENT_BUFFER)
	dma_unmap_single(aml_chip->device, controller->data_dma_addr,
		(flash->pagesize + flash->oobsize),
		DMA_TO_DEVICE);
#endif
	if (time_out_cnt >= AML_DMA_BUSY_TIMEOUT) {
		aml_nand_msg("dma timeout here");
		ret = -NAND_DMA_FAILURE;
		goto _exit;
	}
#endif
_exit:
	return ret;
}

/*
  * aml_nand_hw_init function.
  * init hwcontroller CFG register setting,
  *
*/
static int controller_hw_init(struct hw_controller *controller)
{
	int sys_clk_rate, sys_time, bus_cycle, bus_timing;
	/* int clk_delay; */
	int ret = 0;

	sys_clk_rate = 200;
	get_sys_clk_rate(controller, &sys_clk_rate);

	sys_time = (10000 / sys_clk_rate);

#if 0
	/* if( get_cpu_type() >=MESON_CPU_MAJOR_ID_M8) */
	/* clk_delay = ; */
	start_cycle = (((NAND_CYCLE_DELAY + T_REA * 10) * 10) / sys_time);
	start_cycle = (start_cycle + 9) / 10;

	for (bus_cycle = 4; bus_cycle <= MAX_CYCLE_NUM; bus_cycle++) {
		Tcycle = bus_cycle * sys_time;
		end_cycle =
		(((NAND_CYCLE_DELAY + Tcycle/2 + T_RHOH * 10) * 10)/sys_time);
		end_cycle = end_cycle / 10;
		if ((((start_cycle >= 3) && (start_cycle <= (bus_cycle + 1)))
			|| ((end_cycle >= 3) && (end_cycle <= (bus_cycle + 1))))
			&& (start_cycle <= end_cycle)) {
			break;
		}
	}

	if (bus_cycle > MAX_CYCLE_NUM) {
		aml_nand_msg("timming failed bus_cycle:%d", bus_cycle);
		return -NAND_FAILED;
	}

	bus_timing = (start_cycle + end_cycle) / 2;
#else
	bus_cycle  = 6;
	bus_timing = bus_cycle + 1;
#endif

	NFC_SET_CFG(controller, 0);
	NFC_SET_TIMING_ASYC(controller, bus_timing, (bus_cycle - 1));
	NFC_SEND_CMD(controller, 1<<31);
	aml_nand_dbg("init bus_cycle=%d, bus_timing=%d, system=%d.%dns",
		bus_cycle, bus_timing, sys_time/10, sys_time%10);

	return ret;
}

void controller_enter_standby(struct hw_controller *controller)
{
	/* just enter standby status. */
	NFC_SEND_CMD_STANDBY(controller, 5);/* delay for 5 cycle. */
}

static int controller_adjust_timing(struct hw_controller *controller)
{
	struct amlnand_chip *aml_chip = controller->aml_chip;
	struct nand_flash *flash = &(aml_chip->flash);
	int sys_clk_rate, sys_time, bus_cycle, bus_timing;

	if (!flash->T_REA || (flash->T_REA < 16))
		flash->T_REA = 16;
	if (!flash->T_RHOH || (flash->T_RHOH < 15))
		flash->T_RHOH = 15;

	if (flash->T_REA > 16)
		sys_clk_rate = 200;
	else
		sys_clk_rate = 250;

	get_sys_clk_rate(controller, &sys_clk_rate);

	aml_nand_msg("clk_reg = %x",
			AMLNF_READ_REG(controller->nand_clk_reg));

	sys_time = (10000 / sys_clk_rate);
	/* sys_time = (10000 / (sys_clk_rate / 1000000)); */

#if 0
	start_cycle = (((NAND_CYCLE_DELAY + flash->T_REA * 10) * 10)/sys_time);
	start_cycle = (start_cycle + 9) / 10;

	for (bus_cycle = 6; bus_cycle <= MAX_CYCLE_NUM; bus_cycle++) {
		Tcycle = bus_cycle * sys_time;
		end_cycle =
		(((NAND_CYCLE_DELAY+Tcycle/2+flash->T_RHOH*10)*10)/sys_time);
		end_cycle = end_cycle / 10;
		if ((((start_cycle >= 3) && (start_cycle <= (bus_cycle + 1)))
			|| ((end_cycle >= 3) && (end_cycle <= (bus_cycle + 1))))
			&& (start_cycle <= end_cycle)) {
			break;
		}
	}
	if (bus_cycle > MAX_CYCLE_NUM) {
		aml_nand_msg("timming fail,bus_c:%d,sys_t%d,T_REA:%d,T_RHOH:%d",
			bus_cycle,
			sys_time,
			flash->T_REA,
			flash->T_RHOH);
		return -NAND_FAILED;
	}

	bus_timing = (start_cycle + end_cycle) / 2;
#else
	bus_cycle  = 6;
	bus_timing = bus_cycle + 1;
#endif

	NFC_SET_CFG(controller , 0);
	NFC_SET_TIMING_ASYC(controller, bus_timing, (bus_cycle - 1));
	NFC_SEND_CMD(controller, 1<<31);
	aml_nand_msg("bus_c=%d,bus_t=%d,sys=%d.%dns,T_REA=%d,T_RHOH=%d",
		bus_cycle,
		bus_timing,
		sys_time/10,
		sys_time%10,
		flash->T_REA,
		flash->T_RHOH);

	return NAND_SUCCESS;
}

/*
  *options confirm here, including ecc mode
  */
static int controller_ecc_confirm(struct hw_controller *controller)
{
	struct amlnand_chip *aml_chip = controller->aml_chip;
	struct nand_flash *flash = &(aml_chip->flash);
	struct bch_desc *ecc_supports = controller->bch_desc;
	unsigned int max_bch_mode = controller->max_bch;
	unsigned int options_support = 0, ecc_bytes, ecc_page_cnt = 0, i;
	unsigned char bch_index = 0;
	unsigned short tmp_value;

	if (controller->option & NAND_ECC_SOFT_MODE) {
		controller->ecc_unit = flash->pagesize + flash->oobsize;
		controller->bch_mode = NAND_ECC_NONE;
		aml_nand_msg("soft ecc mode");
		return NAND_SUCCESS;
	}

	for (i = (max_bch_mode-1); i > 0; i--) {
		ecc_page_cnt = flash->pagesize/ecc_supports[i].unit_size;
		ecc_bytes = flash->oobsize / ecc_page_cnt;
		if (ecc_bytes >= ecc_supports[i].bytes +
			ecc_supports[i].usr_mode) {
			options_support = ecc_supports[i].mode;
			bch_index = ecc_supports[i].bch_index;
			break;
		}
	}

	controller->oob_mod = 0;
#if (AML_CFG_NEWOOB_EN)
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8) {
		if (flash->oobsize >= (16+ecc_supports[i].bytes*ecc_page_cnt)) {
			/* for backward compatbility 4k page mlc. The code
			we released before like below, which means old oob
			mode will be used when page size < 4k.
			------------------------------------
			if(flash->pagesize > 4096){
				aml_nand_msg("AML_NAND_NEW_OOB : new oob");
				NFC_SET_OOB_MODE(3<<26);
				controller->oob_mod = 1;
			}else{
				controller->oob_mod = 0;
			}
			------------------------------------*/
			if ((flash->pagesize == 4096)
				&& (flash->chipsize > 2048))
				controller->oob_mod = 0;
			else {
				aml_nand_msg("new oob mode");
				NFC_SET_OOB_MODE(controller, 3<<26);
				controller->oob_mod = 1;
			}
		}
	}
#endif

	switch (options_support) {
	case NAND_ECC_BCH8_MODE:
		controller->ecc_unit = NAND_ECC_UNIT_SIZE;
		controller->ecc_bytes = NAND_BCH8_ECC_SIZE;
		controller->ecc_cnt_limit = 6;
		controller->ecc_max = 8;
		break;

	case NAND_ECC_BCH8_1K_MODE:
		controller->ecc_unit = NAND_ECC_UNIT_1KSIZE;
		controller->ecc_bytes = NAND_BCH8_1K_ECC_SIZE;
		controller->ecc_cnt_limit = 6;
		controller->ecc_max = 8;
		break;

	case NAND_ECC_BCH16_1K_MODE:
		controller->ecc_unit = NAND_ECC_UNIT_1KSIZE;
		controller->ecc_bytes = NAND_BCH16_1K_ECC_SIZE;
		controller->ecc_cnt_limit = 14;
		controller->ecc_max = 16;
		break;

	case NAND_ECC_BCH24_1K_MODE:
		controller->ecc_unit = NAND_ECC_UNIT_1KSIZE;
		controller->ecc_bytes = NAND_BCH24_1K_ECC_SIZE;
		controller->ecc_cnt_limit = 22;
		controller->ecc_max = 24;
		break;

	case NAND_ECC_BCH30_1K_MODE:
		controller->ecc_unit = NAND_ECC_UNIT_1KSIZE;
		controller->ecc_bytes = NAND_BCH30_1K_ECC_SIZE;
		controller->ecc_cnt_limit = 26;
		controller->ecc_max = 30;
		break;

	case NAND_ECC_BCH40_1K_MODE:
		controller->ecc_unit = NAND_ECC_UNIT_1KSIZE;
		controller->ecc_bytes = NAND_BCH40_1K_ECC_SIZE;
		controller->ecc_cnt_limit = 34;
		controller->ecc_max = 40;
		break;

	case NAND_ECC_BCH50_1K_MODE:
		controller->ecc_unit = NAND_ECC_UNIT_1KSIZE;
		controller->ecc_bytes = NAND_BCH50_1K_ECC_SIZE;
		controller->ecc_cnt_limit = 45;
		controller->ecc_max = 50;
		break;

	case NAND_ECC_BCH60_1K_MODE:
		controller->ecc_unit = NAND_ECC_UNIT_1KSIZE;
		controller->ecc_bytes = NAND_BCH60_1K_ECC_SIZE;
		controller->ecc_cnt_limit = 55;
		controller->ecc_max = 60;
		break;

	case NAND_ECC_SHORT_MODE:
		controller->ecc_unit = NAND_ECC_UNIT_SHORT;
		controller->ecc_bytes = NAND_BCH60_1K_ECC_SIZE;
		controller->ecc_cnt_limit = 55;
		controller->ecc_max = 60;
		break;

	default:
		aml_nand_msg("no match ecc mode here");
		return -NAND_ARGUMENT_FAILURE;
		break;
	}

	controller->bch_mode = bch_index;
	if (controller->oob_mod)
		controller->user_mode =
			16 / (flash->pagesize / controller->ecc_unit);
	else
		controller->user_mode = 2;

	tmp_value = controller->ecc_unit + controller->ecc_bytes +
		controller->user_mode;
	controller->ecc_steps = (flash->pagesize+flash->oobsize)/tmp_value;
	controller->oobavail = controller->ecc_steps*controller->user_mode;

	controller->oobtail = flash->pagesize - controller->ecc_steps*tmp_value;
	controller->oob_fill_data = (flash->oobsize -
	(controller->ecc_steps*(controller->ecc_bytes+controller->user_mode)));
	controller->oob_fill_boot = (flash->pagesize+flash->oobsize) - 512;
	controller->ran_mode = 1;
	aml_nand_dbg("ecc_unit:%d, ecc_bytes:%d, ecc_steps:%d, ecc_max:%d",
		controller->ecc_unit,
		controller->ecc_bytes,
		controller->ecc_steps,
		controller->ecc_max);
	aml_nand_dbg("bch_mode:%d,user_mode:%d, oobavail:%d,oobtail:%d",
		controller->bch_mode,
		controller->user_mode,
		controller->oobavail,
		controller->oobtail);
	aml_nand_dbg("oob_fill_data %d,controller->oob_fill_boot %d",
		controller->oob_fill_data,
		controller->oob_fill_boot);

	return NAND_SUCCESS;
}

static void controller_cmd_ctrl(struct hw_controller *controller,
	unsigned cmd,
	unsigned int ctrl)
{
	if (cmd == NAND_CMD_NONE)
		return;

	if (ctrl & NAND_CLE)
		cmd = NFC_CMD_CLE(controller->chip_selected, cmd);
	else
		cmd = NFC_CMD_ALE(controller->chip_selected, cmd);

	NFC_SEND_CMD(controller, cmd);
}

static void controller_write_byte(struct hw_controller *controller,
	unsigned char data)
{
	NFC_SEND_CMD_IDLE(controller, NAND_TWB_TIME_CYCLE);
	NFC_SEND_CMD_DWR(controller, controller->chip_selected, data);
	NFC_SEND_CMD_IDLE(controller, NAND_TWB_TIME_CYCLE);

	NFC_SEND_CMD_IDLE(controller, 0);
	NFC_SEND_CMD_IDLE(controller, 0);

	while (NFC_CMDFIFO_SIZE(controller) > 0)
		;

}

static unsigned char controller_read_byte(struct hw_controller *controller)
{
	NFC_SEND_CMD_DRD(controller, controller->chip_selected, 0);
	NFC_SEND_CMD_IDLE(controller, NAND_TWB_TIME_CYCLE);

	NFC_SEND_CMD_IDLE(controller, 0);
	NFC_SEND_CMD_IDLE(controller, 0);

	while (NFC_CMDFIFO_SIZE(controller) > 0)
		;

	return amlnf_read_reg32(controller->reg_base + P_NAND_BUF);

}

static void controller_get_user_byte(struct hw_controller *controller,
	unsigned char *oob_buf,
	unsigned char byte_num)
{
	int read_times = 0;
	unsigned int len = PER_INFO_BYTE/sizeof(unsigned int);

	if (controller->oob_mod == 1) {
		memcpy(oob_buf,
			(unsigned char *)controller->user_buf,
			byte_num);
		return;
	}
	while (byte_num > 0) {
		*oob_buf++ = (controller->user_buf[read_times*len] & 0xff);
		byte_num--;
		*oob_buf++ =
		((controller->user_buf[read_times*len] >> 8) & 0xff);
		byte_num--;
		read_times++;
	}
}

static void controller_set_user_byte(struct hw_controller *controller,
	unsigned char *oob_buf,
	unsigned char byte_num)
{
	int write_times = 0;
	unsigned int len = PER_INFO_BYTE/sizeof(unsigned int);
	unsigned char *usr_info;
	usr_info = (unsigned char *)controller->user_buf;
	if (controller->oob_mod == 1) {
		memcpy((unsigned char *)controller->user_buf,
			oob_buf,
			byte_num);
		return;
	}
	while (byte_num > 0) {
		controller->user_buf[write_times*len] = *oob_buf++;
		byte_num--;
		controller->user_buf[write_times*len] |=
			(*oob_buf++ << 8);
		byte_num--;
		write_times++;
	}
}

/*
  * fill hw_controller struct.
  * including hw init, option setting and operation function.
  *
  */
int amlnand_hwcontroller_init(struct amlnand_chip *aml_chip)
{
	struct hw_controller *controller = &(aml_chip->controller);

	int i, tmp_num = 0, ret = 0;

	/*IO base mapping address*/
	controller->reg_base = aml_nand_dev->platform_data->nf_reg_base;
	/*external register mapping address for nand clock cfg*/
	controller->nand_clk_reg = aml_nand_dev->platform_data->ext_clk_reg;
	controller->irq = aml_nand_dev->platform_data->irq;
	/* pinmux register... */
	controller->pinmux_base = aml_nand_dev->platform_data->pinmux_base;

	if (!controller->init)
		controller->init = controller_hw_init;
	if (!controller->adjust_timing)
		controller->adjust_timing = controller_adjust_timing;
	if (!controller->ecc_confirm)
		controller->ecc_confirm = controller_ecc_confirm;
	if (!controller->cmd_ctrl)
		controller->cmd_ctrl = controller_cmd_ctrl;
	if (!controller->select_chip)
		controller->select_chip = controller_select_chip;
	if (!controller->quene_rb)
		controller->quene_rb = controller_quene_rb;
#ifdef AML_NAND_RB_IRQ
	if (!controller->quene_rb_irq)
		controller->quene_rb_irq = controller_queue_rb_irq;
#endif
	if (!controller->dma_read)
		controller->dma_read = controller_dma_read;
	if (!controller->dma_write)
		controller->dma_write = controller_dma_write;
	if (!controller->hwecc_correct)
		controller->hwecc_correct = controller_hwecc_correct;
	if (!controller->readbyte)
		controller->readbyte = controller_read_byte;
	if (!controller->writebyte)
		controller->writebyte = controller_write_byte;
	if (!controller->get_usr_byte)
		controller->get_usr_byte = controller_get_user_byte;
	if (!controller->set_usr_byte)
		controller->set_usr_byte = controller_set_user_byte;
	if (!controller->enter_standby)
		controller->enter_standby = controller_enter_standby;

	for (i = 0; i < MAX_CHIP_NUM; i++) {
		controller->ce_enable[i] =
			(((CE_PAD_DEFAULT >> i*4) & 0xf) << 10);
		controller->rb_enable[i] =
			(((RB_PAD_DEFAULT >> i*4) & 0xf) << 10);
	}

	/*setting default value for option.*/
	controller->option |= NAND_CTRL_NONE_RB;
	controller->option |= NAND_ECC_BCH60_1K_MODE;

	controller->aml_chip = aml_chip;

#ifdef AML_NAND_RB_IRQ
	aml_nand_msg("######STS IRQ mode for nand driver");
	if (request_irq(controller->irq,
		controller_interrupt_monitor,
		IRQF_DISABLED,
		"aml_nand",
		(void *)controller)) {
		aml_nand_msg("request nand status irq error!!!");
		return -1;
	}
#endif

#ifdef AML_NAND_DMA_POLLING
	aml_nand_msg("######timer mode for nand driver");
	hrtimer_init(&controller->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	controller->timer.function = controller_dma_timerfuc;
#endif
	amlphy_prepare(0);
	ret = controller->init(controller);
	if (ret)
		aml_nand_msg("controller hw init failed");

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
		controller->bch_desc = (struct bch_desc *)&bch_list_m8[0];
	else
		controller->bch_desc = (struct bch_desc *)&bch_list[0];
	for (i = 0; i < MAX_ECC_MODE_NUM; i++) {
		if (controller->bch_desc[i].name == NULL)
			break;
		tmp_num++;
	}
	controller->max_bch  = tmp_num;
	/*
	controller->max_bch = sizeof(bch_list) / sizeof(bch_list[0]);
	*/
	return ret;
}

