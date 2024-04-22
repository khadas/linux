// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "demod_func.h"
#include "dvbs.h"

static unsigned char dvbs_iq_swap;
MODULE_PARM_DESC(dvbs_iq_swap, "");
module_param(dvbs_iq_swap, byte, 0644);

static unsigned int blind_search_agc2bandwidth = BLIND_SEARCH_AGC2BANDWIDTH_60;
MODULE_PARM_DESC(blind_search_agc2bandwidth, "");
module_param(blind_search_agc2bandwidth, int, 0644);

static unsigned int blind_search_bw_min = BLIND_SEARCH_BW_MIN;
MODULE_PARM_DESC(blind_search_bw_min, "");
module_param(blind_search_bw_min, int, 0644);

static unsigned int blind_search_pow_th = BLIND_SEARCH_POW_TH2;
MODULE_PARM_DESC(blind_search_pow_th, "");
module_param(blind_search_pow_th, int, 0644);

static struct fe_lla_lookup_t fe_l2a_s1_cn_lookup = {
	161, //size
	{
		// SFE(dB) Noise DVBS1  --db val
		//-3.5db  -3.0db -2.5db  -2.0db
		{ -35, 9474 }, { -30, 9449 }, { -25, 9381 }, { -20, 9300 }, //-3.5, -3.0,-2.5, -2.0
		{ -15, 9240 }, { -10, 9155 }, {  -5, 9046 }, {   0, 8943 },
		{   5, 8813 }, {  10, 8688 }, {  15, 8539 }, {  20, 8328 },
		{  25, 8151 }, {  30, 7989 }, {  33, 7762 }, {  36, 7701 },
		{  39, 7574 }, {  42, 7399 }, {  45, 7283 }, {  48, 7116 },
		{  51, 6952 }, {  54, 6796 }, {  57, 6641 }, {  60, 6477 },
		{  61, 6427 }, {  62, 6377 }, {  63, 6327 }, {  64, 6269 },
		{  65, 6210 }, {  66, 6151 }, {  67, 6097 }, {  68, 6042 },
		{  69, 5987 }, {  70, 5928 }, {  71, 5869 }, {  72, 5810 },
		{  73, 5760 }, {  74, 5711 }, {  75, 5661 }, {  76, 5605 },
		{  77, 5550 }, {  78, 5494 }, {  79, 5440 }, {  80, 5387 },
		{  81, 5333 }, {  82, 5281 }, {  83, 5228 }, {  84, 5176 },
		{  85, 5128 }, {  86, 5080 }, {  87, 5032 }, {  88, 4972 },
		{  89, 4911 }, {  90, 4851 }, {  91, 4800 }, {  92, 4750 },
		{  93, 4701 }, {  94, 4656 }, {  95, 4611 }, {  96, 4566 },
		{  97, 4522 }, {  98, 4479 }, {  99, 4435 }, { 100, 4383 },
		{ 101, 4331 }, { 102, 4279 }, { 103, 4236 }, { 104, 4192 },
		{ 105, 4149 }, { 106, 4106 }, { 107, 4064 }, { 108, 4021 },
		{ 109, 3971 }, { 110, 3922 }, { 111, 3872 }, { 112, 3835 },
		{ 113, 3799 }, { 114, 3762 }, { 115, 3720 }, { 116, 3679 },
		{ 117, 3637 }, { 118, 3600 }, { 119, 3563 }, { 120, 3526 },
		{ 121, 3488 }, { 122, 3450 }, { 123, 3412 }, { 124, 3374 },
		{ 125, 3336 }, { 126, 3298 }, { 127, 3263 }, { 128, 3227 },
		{ 129, 3192 }, { 130, 3156 }, { 131, 3119 }, { 132, 3083 },
		{ 133, 3051 }, { 134, 3020 }, { 135, 2988 }, { 136, 2954 },
		{ 137, 2920 }, { 138, 2886 }, { 139, 2856 }, { 140, 2826 },
		{ 141, 2796 }, { 142, 2765 }, { 143, 2734 }, { 144, 2703 },
		{ 145, 2676 }, { 146, 2648 }, { 147, 2621 }, { 148, 2592 },
		{ 149, 2563 }, { 150, 2534 }, { 151, 2507 }, { 152, 2479 },
		{ 153, 2452 }, { 154, 2425 }, { 155, 2397 }, { 156, 2370 },
		{ 157, 2343 }, { 158, 2316 }, { 159, 2288 }, { 160, 2261 },
		{ 161, 2238 }, { 162, 2215 }, { 170, 2030 }, { 180, 1827 },
		{ 190, 1618 }, { 200, 1464 }, { 210, 1308 }, { 220, 1177 },
		{ 230, 1067 }, { 240,  964 }, { 250,  871 }, { 260,  785 },
		{ 270,  722 }, { 280,  674 }, { 290,  623 }, { 300,  582 },
		{ 310,  534 }, { 320,  515 }, { 330,  482 }, { 340,  458 },
		{ 350,  440 }, { 360,  425 }, { 370,  412 }, { 380,  422 },
		{ 390,  408 }, { 400,  383 }, { 410,  385 }, { 420,  376 },
		{ 430,  384 }, { 440,  382 }, { 450,  376 }, { 460,  379 },
		{ 470,  370 }, { 480,  381 }, { 490,  371 }, { 500,  371 },
		{ 510,  369 }, //50db   51db
	}
};

static struct fe_lla_lookup_t fe_l2a_s2_cn_lookup = {
	161, //size
	{
		// SFE(dB), Noise DVBS2
		{ -35, 13780 }, { -30, 13393 }, { -25, 12984 }, { -20, 12618 },
		{ -15, 12265 }, { -10, 11823 }, {  -5, 11498 }, {   0, 11034 },
		{   5, 10660 }, {  10, 10330 }, {  15,  9828 }, {  20,  9515 },
		{  25,  9052 }, {  30,  8613 }, {  33,  8487 }, {  36,  8162 },
		{  39,  7956 }, {  42,  7753 }, {  45,  7547 }, {  48,  7328 },
		{  51,  7081 }, {  54,  6934 }, {  57,  6702 }, {  60,  6544 },
		{  61,  6483 }, {  62,  6423 }, {  63,  6362 }, {  64,  6355 },
		{  65,  6348 }, {  66,  6141 }, {  67,  6210 }, {  68,  6080 },
		{  69,  5949 }, {  70,  5883 }, {  71,  5816 }, {  72,  5750 },
		{  73,  5723 }, {  74,  5696 }, {  75,  5669 }, {  76,  5613 },
		{  77,  5558 }, {  78,  5502 }, {  79,  5422 }, {  80,  5343 },
		{  81,  5263 }, {  82,  5191 }, {  83,  5119 }, {  84,  5047 },
		{  85,  5016 }, {  86,  4985 }, {  87,  4954 }, {  88,  4914 },
		{  89,  4875 }, {  90,  4835 }, {  91,  4769 }, {  92,  4703 },
		{  93,  4637 }, {  94,  4601 }, {  95,  4565 }, {  96,  4529 },
		{  97,  4474 }, {  98,  4418 }, {  99,  4363 }, { 100,  4266 },
		{ 101,  4170 }, { 102,  4231 }, { 103,  4178 }, { 104,  4126 },
		{ 105,  4073 }, { 106,  4025 }, { 107,  3978 }, { 108,  3930 },
		{ 109,  3897 }, { 110,  3863 }, { 111,  3830 }, { 112,  3798 },
		{ 113,  3767 }, { 114,  3735 }, { 115,  3681 }, { 116,  3626 },
		{ 117,  3572 }, { 118,  3544 }, { 119,  3515 }, { 120,  3487 },
		{ 121,  3455 }, { 122,  3423 }, { 123,  3391 }, { 124,  3340 },
		{ 125,  3288 }, { 126,  3237 }, { 127,  3196 }, { 128,  3155 },
		{ 129,  3114 }, { 130,  3085 }, { 131,  3056 }, { 132,  3027 },
		{ 133,  3003 }, { 134,  2978 }, { 135,  2954 }, { 136,  2918 },
		{ 137,  2883 }, { 138,  2847 }, { 139,  2824 }, { 140,  2800 },
		{ 141,  2777 }, { 142,  2746 }, { 143,  2714 }, { 144,  2683 },
		{ 145,  2640 }, { 146,  2596 }, { 147,  2553 }, { 148,  2534 },
		{ 149,  2516 }, { 150,  2497 }, { 151,  2473 }, { 152,  2450 },
		{ 153,  2426 }, { 154,  2402 }, { 155,  2378 }, { 156,  2355 },
		{ 157,  2331 }, { 158,  2307 }, { 159,  2284 }, { 160,  2260 },
		{ 161,  2235 }, { 162,  2210 }, { 170,  2011 }, { 180,  1792 },
		{ 190,  1583 }, { 200,  1461 }, { 210,  1295 },	{ 220,  1167 },
		{ 230,  1050 }, { 240,   961 }, { 250,   884 }, { 260,   812 },
		{ 270,   735 }, { 280,   668 }, { 290,   623 }, { 300,   574 },
		{ 310,   555 }, { 320,   527 }, { 330,   495 }, { 340,   463 },
		{ 350,   460 }, { 360,   462 }, { 370,   430 }, { 380,   420 },
		{ 390,   434 }, { 400,   404 }, { 410,   408 }, { 420,   398 },
		{ 430,   401 }, { 440,   395 }, { 450,   400 }, { 460,   388 },
		{ 470,   419 }, { 480,   418 }, { 490,   403 }, { 500,   394 },
		{ 510,   396 },
	}
};

unsigned int dvbs2_diseqc_irq_check(void)
{
	unsigned int diseq_irq_flag;
	unsigned int diseq_irq_sts;/* enum diseq_irq_flag */

	diseq_irq_flag = dvbs_rd_byte(DVBS_REG_SYS_IRQSTATUS0);
	if (diseq_irq_flag & 0x1) {
		diseq_irq_sts = dvbs_rd_byte(DVBS_REG_DISIRQSTAT);
		return diseq_irq_sts;
	} else {
		return 0;
	}
}

unsigned int dvbs2_diseqc_rx_check(void)
{
	unsigned char rx_empty = 0;
	unsigned int len_fifo = 0;

	rx_empty = dvbs_rd_byte(DVBS_REG_DISRXSTAT1) & 0x04;
	if (!rx_empty)
		len_fifo = dvbs_rd_byte(DVBS_REG_DISRXBYTES);

	return len_fifo;
}

void dvbs2_diseqc_reset(void)
{
	dvbs_write_bits(DVBS_REG_DISRXCFG, 1, 7, 1);
	dvbs_write_bits(DVBS_REG_DISRXCFG, 0, 7, 1);
}

void dvbs2_diseqc_send_irq_en(bool onoff)
{
	unsigned char val = 0;

	if (onoff) {
		/*
		 * 1:enable IRQGAPBURST interrupt
		 * 1:enable IRQFIFO64B interrupt
		 * 1:enable IRQTXEND interrupt
		 * 1:enable IRQTIMEOUT interrupt
		 * 1:enable IRQTRFINISH interrupt
		 * 1:enable IRQRXFIFO8B interrupt
		 * 1:enable IRQRXEND interrupt
		 */
		dvbs_wr_byte(DVBS_REG_DISIRQCFG, 0x1f);
		/* Enable diseqc interrupt */
		val = dvbs_rd_byte(DVBS_REG_SYS_IRQMSK0);
		dvbs_wr_byte(DVBS_REG_SYS_IRQMSK0, val | 0x1);
	} else {
		val = dvbs_rd_byte(DVBS_REG_DISIRQCFG);
		dvbs_wr_byte(DVBS_REG_DISIRQCFG, val & (~0x1f));

		/* Disable diseqc interrupt*/
		val = dvbs_rd_byte(DVBS_REG_SYS_IRQMSK0);
		dvbs_wr_byte(DVBS_REG_SYS_IRQMSK0, val & (~0x1));
	}
}

void dvbs2_diseqc_recv_irq_en(bool onoff)
{
	unsigned char val = 0;

	if (onoff) {
		/*
		 * 1:enable IRQGAPBURST interrupt
		 * 1:enable IRQFIFO64B interrupt
		 * 1:enable IRQTXEND interrupt
		 * 1:enable IRQTIMEOUT interrupt
		 * 1:enable IRQTRFINISH interrupt
		 * 1:enable IRQRXFIFO8B interrupt
		 * 1:enable IRQRXEND interrupt
		 */
		val = dvbs_rd_byte(DVBS_REG_DISIRQCFG);
		dvbs_wr_byte(DVBS_REG_DISIRQCFG, val | 0x60);
		/* Enable diseqc interrupt */
		val = dvbs_rd_byte(DVBS_REG_SYS_IRQMSK0);
		dvbs_wr_byte(DVBS_REG_SYS_IRQMSK0, val | 0x1);
	} else {
		val = dvbs_rd_byte(DVBS_REG_DISIRQCFG);
		dvbs_wr_byte(DVBS_REG_DISIRQCFG, val | (~0x60));

		/* Disable diseqc interrupt*/
		val = dvbs_rd_byte(DVBS_REG_SYS_IRQMSK0);
		dvbs_wr_byte(DVBS_REG_SYS_IRQMSK0, val & (~0x1));
	}
}

void dvbs2_diseqc_init(void)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	/* set high resistance for diseqc input */
	dvbs_wr_byte(DVBS_REG_GPIO0CFG, 0xcc);

	/* DiSEqC transmission configuration 2:DiSEqC 2/3 */
	if (devp && devp->diseqc.lnbc.is_internal_tone)
		dvbs_wr_byte(DVBS_REG_DISTXCFG, dvbs_rd_byte(DVBS_REG_DISTXCFG) | 0x8);

	dvbs2_diseqc_continuous_tone(false);
	/* rx 22k tone, 125Mhz:b0, default 135Mhz:c0*/
	//dvbs_wr_byte(DVBS_REG_DISTXF22, 0xb0); //t5d
	/* number of bit to wait before starting the transmission */
	dvbs_wr_byte(DVBS_REG_DISTIMEOCFG, 0x84);
	/* rx 22k tone, 125Mhz:143, default 135Mhz:12b*/
	dvbs_wr_byte(DVBS_REG_DISRXF220, 0xf0);
	/* 9c for 125Mhz */
	dvbs_wr_byte(DVBS_REG_DISRXF100, 0x9c);
	/* for rx : enable rx, glitch, GPIO0  */

	dvbs_wr_byte(DVBS_REG_DISRXCFG, 0x84);
	dvbs_wr_byte(DVBS_REG_DISRXCFG, 0x4);

	/* number of bit to wait before starting the transmission */
	dvbs_wr_byte(DVBS_REG_DISTXWAIT, 0x1);
	dvbs_wr_byte(DVBS_REG_DSQADCINCFG, 0x67);
}

void dvbs2_diseqc_recv_en(bool onoff)
{
	unsigned char val = 0;

	val = dvbs_rd_byte(DVBS_REG_DISRXCFG);

	if (onoff)
		dvbs_wr_byte(DVBS_REG_DISRXCFG, val | 0x1);
	else
		dvbs_wr_byte(DVBS_REG_DISRXCFG, val & (~0x1));
}

void dvbs2_diseqc_continuous_tone(bool onoff)
{
	unsigned char val = 0;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	val = dvbs_rd_byte(DVBS_REG_DISTXCFG) & 0xfc;

	/* DiSEqC transmission configuration 2:DiSEqC 2/3 */
	if (devp && devp->diseqc.lnbc.is_internal_tone)
		val |= 0x8;

	if (onoff) {
		dvbs_wr_byte(DVBS_REG_DISTXCFG, val);
	} else {
		val |= 0x2;
		dvbs_wr_byte(DVBS_REG_DISTXCFG, val);
	}
}

void dvbs2_diseqc_send_msg(unsigned int len, unsigned char *msg)
{
	unsigned int i = 0;
	unsigned int len_fifo = 0;

	dvbs2_diseqc_init();
	dvbs2_diseqc_send_irq_en(true);
	for (i = 0; i < len ; i++)
		dvbs_wr_byte(DVBS_REG_DISTXFIFO, msg[i]);
		/*PR_INFO("0x%x\n", msg[i]);*/

	len_fifo = dvbs_rd_byte(DVBS_REG_DISTXBYTES);
	/*PR_INFO("tx fifo num:0x%x\n", len_fifo);*/
}

unsigned int dvbs2_diseqc_read_msg(unsigned int len, unsigned char *msg)
{
	unsigned int i = 0;
	unsigned int len_fifo = 0;

	len_fifo = dvbs_rd_byte(DVBS_REG_DISRXBYTES);
	for (i = 0; i < len_fifo && i < len ; i++)
		msg[i] = dvbs_rd_byte(DVBS_REG_DISRXFIFO);

	return i;
}

void demod_dump_reg_diseqc(void)
{
	unsigned int i = 0;

	for (i = DVBS_REG_SYS_IRQMSK1; i <= DVBS_REG_SYS_IRQFORCE0; i++)
		PR_INFO("diseqc reg:0x%x val:0x%x\n", i, dvbs_rd_byte(i));

	for (i = DVBS_REG_GPIO0CFG; i <= DVBS_REG_I2CMAPO20; i++)
		PR_INFO("diseqc reg:0x%x val:0x%x\n", i, dvbs_rd_byte(i));

	for (i = DVBS_REG_DISIRQCFG; i <= DVBS_REG_ACRDIV; i++)
		PR_INFO("diseqc reg:0x%x val:0x%x\n", i, dvbs_rd_byte(i));
}

void dvbs_check_status(struct seq_file *seq)
{
	char *roll_off;

	switch (dvbs_rd_byte(0xa05) & 0x7) {
	case 0:
		roll_off = "0.35";
		break;

	case 1:
		roll_off = "0.25";
		break;

	case 2:
		roll_off = "0.20";
		break;

	case 3:
		roll_off = "0.10";
		break;

	case 5:
		roll_off = "0.15";
		break;

	case 4:
		roll_off = "0.05";
		break;

	default:
		roll_off = "Unknown Roll Off";
		break;
	}

	if (seq) {
		seq_printf(seq, "Roll Off:%s,SNR 0x%x,0x152=0x%x,0x153=0x%x,TS_ok:%d\n",
			roll_off,
			(dvbs_rd_byte(CNR_HIGH) << 8) | dvbs_rd_byte(CNR_LOW), dvbs_rd_byte(0x152),
			dvbs_rd_byte(0x153), (dvbs_rd_byte(0x160) >> 3) & 0x1);
		seq_printf(seq, "PER1:%d,PER2:%d\n",
			((dvbs_rd_byte(0xe61) & 0x7f)  << 16) +
			((dvbs_rd_byte(0xe62) & 0xff) << 8) +
			(dvbs_rd_byte(0xe63) & 0xff),
			((dvbs_rd_byte(0xe65) & 0x7f)  << 16) +
			((dvbs_rd_byte(0xe66) & 0xff) << 8) +
			(dvbs_rd_byte(0xe67) & 0xff));
	} else {
		PR_DVBS("Roll Off:%s,SNR 0x%x,0x152=0x%x,0x153=0x%x,TS_ok:%d\n",
			roll_off,
			(dvbs_rd_byte(CNR_HIGH) << 8) | dvbs_rd_byte(CNR_LOW), dvbs_rd_byte(0x152),
			dvbs_rd_byte(0x153), (dvbs_rd_byte(0x160) >> 3) & 0x1);
		PR_DVBS("PER1:%d,PER2:%d\n",
			((dvbs_rd_byte(0xe61) & 0x7f)  << 16) +
			((dvbs_rd_byte(0xe62) & 0xff) << 8) +
			(dvbs_rd_byte(0xe63) & 0xff),
			((dvbs_rd_byte(0xe65) & 0x7f)  << 16) +
			((dvbs_rd_byte(0xe66) & 0xff) << 8) +
			(dvbs_rd_byte(0xe67) & 0xff));
	}
}

unsigned int dvbs_get_quality(void)
{
	unsigned int noisefield1, noisefield0;
	unsigned int c_n = -100, regval, imin, imax, i;
	unsigned int fld_value[2];
	struct fe_lla_lookup_t *lookup;

	if ((dvbs_rd_byte(0x932) & 0x60) >> 5 == 0x2) {
		lookup = &fe_l2a_s2_cn_lookup;
		//If DVBS2 use PLH normalized noise indicators
		noisefield1 = 0xaa4;
		noisefield0 = 0xaa5;
	} else {
		lookup = &fe_l2a_s1_cn_lookup;
		//if not DVBS2 use symbol normalized noise indicators
		noisefield1 = 0xaa0;
		noisefield0 = 0xaa1;
	}

	fld_value[0] = dvbs_rd_byte(0x934) & 0x08;
	if (fld_value[0]) {
		if (lookup->size) {
			regval = 0;
			for (i = 0; i < 25; i++) {
				fld_value[0] = dvbs_rd_byte(noisefield1) & 0xff;
				fld_value[1] = dvbs_rd_byte(noisefield0) & 0xff;
				regval += (fld_value[0] << 8) + fld_value[1];
				usleep_range(2000, 2001);
			}
			regval /= 25;
			imin = 0;
			imax = lookup->size - 1;
			if ((lookup->table[imin].regval <= regval &&
				regval <= lookup->table[imax].regval) ||
				(lookup->table[imin].regval >= regval &&
				regval >= lookup->table[imax].regval)) {
				while ((imax - imin) > 1) {
					i = (imax + imin) >> 1;
					//PR_DVBS("Imax=%d, Imin=%d, i=%d\n", imax, imin, i);
					if ((lookup->table[imin].regval <= regval &&
						regval <= lookup->table[i].regval) ||
						(lookup->table[imin].regval >= regval &&
						regval >= lookup->table[i].regval))
						imax = i;
					else
						imin = i;
				}
				//PR_DVBS("Imax=%d, Imin=%d\n",
				//	lookup->table[imin].regval, lookup->table[imax].realval);

				//c_n = ((regval - lookup->table[imin].regval) *
				//(lookup->table[imax].realval - lookup->table[imin].realval) /
				//(lookup->table[imax].regval - lookup->table[imin].regval)) +
				//lookup->table[imin].realval;
				c_n = ((regval - lookup->table[imin].regval)
					* (lookup->table[imax].realval
					- lookup->table[imin].realval) * 10
					/ (lookup->table[imax].regval
					- lookup->table[imin].regval))
					+ lookup->table[imin].realval * 10;
				if ((c_n - c_n / 10 * 10) > 5)
					c_n = c_n / 10 + 1;
				else
					c_n = c_n / 10;
				//PR_DVBS("regval is %d\n", regval);
				//PR_DVBS("c_n is %d\n", c_n);
			} else if (regval > lookup->table[imin].regval) {
				c_n = -100;
			} else if (regval < lookup->table[imax].regval) {
				c_n = 1000;
			}
		}
	}
	return c_n;
}

unsigned int dvbs_get_freq_offset(unsigned int *polarity)
{
	unsigned int carrier_offset = 0, freq_offset = 0;

	carrier_offset = dvbs_rd_byte(CFR12) << 16;
	carrier_offset |= dvbs_rd_byte(CFR11) << 8;
	carrier_offset |= dvbs_rd_byte(CFR10);

	*polarity = carrier_offset >> 23 & 0x1;
	/* negative val, convert to original code */
	if (*polarity) {
		carrier_offset ^= 0xffffff;
		carrier_offset += 1;
	}

	if (dvbs_iq_swap)
		*polarity = (*polarity) ? 0 : 1;

	/* fre offset = carrier_offset * Fs(adc) / 2^24 */
	freq_offset = carrier_offset * (ADC_CLK_135M / 1000); //ADC_CLK_135M
	freq_offset = (freq_offset + ALIGN_24 / 2000) / (ALIGN_24 / 1000);

	PR_DVBS("%s: iq_swap %d, polarity %d, carrier_offset 0x%x, freq_offset %dKHz\n",
		__func__, dvbs_iq_swap, *polarity, carrier_offset, freq_offset);

	return freq_offset;
}

unsigned int dvbs_get_symbol_rate(void)
{
#ifdef DVBS_NEW_GET_SR
	unsigned int value = 0, sr_kbps = 0;
	unsigned int samples = 0, clock = 0;
	unsigned int value_h = 0, value_l = 0, clock_h = 0, clock_l = 0;

	value = dvbs_rd_byte(0x9fc) << 24;
	value |= dvbs_rd_byte(0x9fd) << 16;
	value |= dvbs_rd_byte(0x9fe) << 8;
	value |= dvbs_rd_byte(0x9ff);

	samples = dvbs_rd_byte(0x94a) & 0x1f;
	clock = samples * ADC_CLK_135M * 1000;

	clock_h = clock >> 16;
	value_h = value >> 16;
	clock_l = clock & 0xffff;
	value_l = value & 0xffff;

	sr_kbps = (clock_h * value_h) +
			((clock_h * value_l) >> 16) +
			((value_h * clock_l) >> 16);

	PR_INFO("%s: value 0x%0x, samples 0x%x, clock %d, sr_kbps %d bps\n",
			__func__, value, samples, clock, sr_kbps);

	return (sr_kbps / 1000);
#else
	unsigned int value = 0, sr_kbps = 0;

	value = dvbs_rd_byte(0x9fc) << 16;
	value |= dvbs_rd_byte(0x9fd) << 8;
	value |= dvbs_rd_byte(0x9fe);
	value = value * (ADC_CLK_135M / 1000);

	sr_kbps = (value + ALIGN_24 / 2000) / (ALIGN_24 / 1000);

	PR_INFO("%s: value %d, sr_kbps %d Kbps\n", __func__, value, sr_kbps);

	return sr_kbps;
#endif
}

#define SIGNAL_STRENGTH_READ_TIMES 50
static unsigned char sastrengthval[] = {
	0xd3,
	0xd5, 0xd2, 0xd0, 0xce, 0xcb,
	0xc8, 0xc5, 0xc2, 0xbe, 0xb9,
	0xb4, 0xad, 0xa7, 0xa3, 0xa1,
	0x9e, 0x9c, 0x99, 0x97, 0x94,
	0x91, 0x8b, 0x8e, 0x87, 0x83,
	0x7e, 0x79, 0x73, 0x68
};

int dvbs_get_signal_strength_off(void)
{
	int i;
	unsigned int val = 0;

	for (i = 0; i < SIGNAL_STRENGTH_READ_TIMES; i++)
		val += dvbs_rd_byte(0x91a);
	val /= SIGNAL_STRENGTH_READ_TIMES;

	for (i = 1; i < sizeof(sastrengthval); i++) {
		if (val >= sastrengthval[i])
			break;
	}

	PR_DVBS("average value level val=0x%x offset %d\n", val, -i);

	return -i;
}

void dvbs_fft_reg_init(unsigned int *reg_val)
{
	unsigned int fld_value;
	int i = 0;

	fld_value = (dvbs_rd_byte(0x8c0)) & 0x1;
	reg_val[i++] = fld_value;
	fld_value = ((dvbs_rd_byte(0x942)) & 0xc0) >> 6;
	reg_val[i++] = fld_value;
	fld_value = ((dvbs_rd_byte(0x940)) & 0xc0) >> 6;
	reg_val[i++] = fld_value;
	fld_value = (dvbs_rd_byte(0x924)) & 0x1f;
	reg_val[i++] = fld_value;
	fld_value = ((dvbs_rd_byte(0xb31)) & 0x80) >> 7;
	reg_val[i++] = fld_value;

	fld_value = ((dvbs_rd_byte(0x9a3)) & 0x80) >> 7;
	reg_val[i++] = fld_value;
	fld_value = ((dvbs_rd_byte(0x99f)) & 0x80) >> 7;
	reg_val[i++] = fld_value;
	fld_value = (dvbs_rd_byte(0x990)) & 0x7;
	reg_val[i++] = fld_value;

	fld_value = dvbs_rd_byte(0x970);
	reg_val[i++] = fld_value;
	fld_value = dvbs_rd_byte(0x971);
	reg_val[i++] = fld_value;
	fld_value = dvbs_rd_byte(0x9a0);
	reg_val[i++] = fld_value;
	fld_value = dvbs_rd_byte(0x9a1);
	reg_val[i++] = fld_value;

	fld_value = ((dvbs_rd_byte(0x8c0)) & 0x3c) >> 2;
	reg_val[i++] = fld_value;
	fld_value = (dvbs_rd_byte(0x8c5)) & 0xff;
	reg_val[i++] = fld_value;
	fld_value = ((dvbs_rd_byte(0x8c0)) & 0x40) >> 6;
	reg_val[i++] = fld_value;

	fld_value = dvbs_rd_byte(0x8c1);
	reg_val[i++] = fld_value;
	fld_value = dvbs_rd_byte(0x8c4);
	reg_val[i++] = fld_value;
	fld_value = dvbs_rd_byte(0x8c3);
	reg_val[i++] = fld_value;
	fld_value = dvbs_rd_byte(0x8ec);
	reg_val[i++] = fld_value;

	dvbs_write_bits(0x942, 0x01, 6, 2);
	dvbs_write_bits(0x940, 0x01, 6, 2);
	dvbs_write_bits(0x924, 0x02, 0, 5);//demod_mode
	dvbs_write_bits(0xb31, dvbs_iq_swap ? 1 : 0, 7, 1);
	dvbs_write_bits(0x8a1, 0x02, 4, 2);
	/* Agc blocked */
	dvbs_write_bits(0x9a3, 0x1, 7, 1);
	dvbs_write_bits(0x99f, 0x1, 7, 1);
	dvbs_write_bits(0x990, 0x0, 0, 3);

	dvbs_write_bits(0x970, 0x1, 0, 1);
	dvbs_write_bits(0x970, 0x1, 1, 1);
	dvbs_write_bits(0x970, 0x1, 2, 1);

	dvbs_write_bits(0x971, 0x0, 0, 2);
	dvbs_write_bits(0x971, 0x0, 2, 2);
	dvbs_write_bits(0x971, 0x0, 4, 2);

	dvbs_wr_byte(0x9a0, 0x40);
	dvbs_wr_byte(0x9a1, 0x42);

	/* fft reading registers adjustment */
	dvbs_write_bits(0x8c0, 0x1, 2, 4); // input mode  1-cte 2-z4
	dvbs_write_bits(0x8c5, 0x2, 0, 8);
	dvbs_write_bits(0x8c0, 0x0, 6, 1);

	dvbs_write_bits(0x8c1, 0x0, 0, 3);
	dvbs_write_bits(0x8c1, 0x1, 3, 1);
	dvbs_write_bits(0x8c1, 0x4, 4, 3);//cte mode
	dvbs_write_bits(0x8c1, 0x0, 7, 1);

	dvbs_write_bits(0x8c4, 0x2, 0, 7);
	dvbs_write_bits(0x8c4, 0x0, 7, 1);

	dvbs_write_bits(0x8c3, 0x4, 0, 4);//MAX_THRESHOLD
	dvbs_write_bits(0x8c3, 0x0, 4, 1);//NO_STOP

	dvbs_write_bits(0x8ec, 0x0, 0, 1);
	dvbs_write_bits(0x8ec, 0x0, 1, 1);
	dvbs_write_bits(0x8ec, 0x0, 2, 1);
	dvbs_write_bits(0x8ec, 0x1, 3, 1);
	dvbs_write_bits(0x8ec, 0x0, 4, 1);
	dvbs_write_bits(0x8ec, 0x1, 5, 1);
	dvbs_write_bits(0x8ec, 0x0, 6, 1);

	dvbs_write_bits(0x9a3, 0x88, 0, 7);
	dvbs_write_bits(0x99f, 0x56, 0, 7);

	dvbs_wr_byte(0x990, 0x0);
	dvbs_write_bits(0x912, 0x0, 0, 3);
	dvbs_write_bits(0x910, 0x1, 5, 1);
	dvbs_write_bits(0x910, 0x1, 3, 1);
	dvbs_wr_byte(0x913, 0x58);
	dvbs_write_bits(0x912, 0x0, 0, 3);

	dvbs_wr_byte(0x918, 0x00);
	dvbs_wr_byte(0x919, 0x00);
	dvbs_wr_byte(0x993, 0x02);
	dvbs_wr_byte(0x9b1, 0x00);
	dvbs_wr_byte(0x9b2, 0x00);
	dvbs_wr_byte(0x9b3, 0x00);
	dvbs_wr_byte(0x9b5, 0x00);
	dvbs_wr_byte(0x9b6, 0x00);
	dvbs_wr_byte(0x9ba, 0x00);
	dvbs_wr_byte(0x9d7, 0x00);

	dvbs_wr_byte(0x9d8, 0x00);
	dvbs_wr_byte(0x9d9, 0x00);
	dvbs_wr_byte(0x9e0, 0xc3);
	dvbs_wr_byte(0x9e1, 0x00);
	dvbs_wr_byte(0xa00, 0x00);
	dvbs_wr_byte(0xa01, 0x00);
	dvbs_wr_byte(0xa02, 0x00);
	dvbs_wr_byte(0xa61, 0x00);
	dvbs_wr_byte(0xa63, 0x00);
	dvbs_wr_byte(0xa64, 0x00);
	dvbs_wr_byte(0xa65, 0x00);
	msleep(100);
}

void dvbs_fft_reg_term(unsigned int reg_val[60])
{
	int i = 0;

	/* Restore params */
	dvbs_write_bits(0x8c0, reg_val[i++], 0, 1);
	dvbs_write_bits(0x942, reg_val[i++], 6, 2);
	dvbs_write_bits(0x940, reg_val[i++], 6, 2);
	dvbs_write_bits(0x924, reg_val[i++], 0, 5);//demod_mode
	dvbs_write_bits(0xb31, reg_val[i++], 7, 1);

	dvbs_write_bits(0x9a3, reg_val[i++], 7, 1);
	dvbs_write_bits(0x99f, reg_val[i++], 7, 1);
	dvbs_write_bits(0x990, reg_val[i++], 0, 3);

	dvbs_wr_byte(0x970, reg_val[i++]);
	dvbs_wr_byte(0x971, reg_val[i++]);

	dvbs_wr_byte(0x9a0, reg_val[i++]);
	dvbs_wr_byte(0x9a1, reg_val[i++]);

	dvbs_write_bits(0x8c0, reg_val[i++], 2, 4);
	dvbs_write_bits(0x8c5, reg_val[i++], 0, 8);
	dvbs_write_bits(0x8c0, reg_val[i++], 6, 1);

	dvbs_wr_byte(0x8c1, reg_val[i++]);
	dvbs_wr_byte(0x8c4, reg_val[i++]);
	dvbs_wr_byte(0x8c3, reg_val[i++]);
	dvbs_wr_byte(0x8ec, reg_val[i++]);
	// Forbid the use of RAMs by the UFBS
	dvbs_write_bits(0x8a1, 0, 4, 2);
}

void dvbs_blind_fft_work(struct fft_threadcontrols *spectr_ana_data,
	int frq, struct fft_search_result *search_result)
{
	unsigned int frc_demod_set = 0;
	unsigned int fld_value = 0;
	unsigned int contmode = 0;
	int cte_freq_ok = 0;
	int timeout = 0;
	int range_m = 0;
	int bin_max = 0;

	dvbs_write_bits(0x8c1, spectr_ana_data->mode, 0, 3);
	dvbs_write_bits(0x8c2, spectr_ana_data->acc, 0, 8);
	dvbs_write_bits(0x8c0, 0x1, 0, 1);//UFBS_ENABLE

	frc_demod_set = frq * ALIGN_24 / (ADC_CLK_135M / 1000);
	dvbs_wr_byte(0x9c3, ((char)(frc_demod_set >> 16)));
	dvbs_wr_byte(0x9c4, ((char)(frc_demod_set >> 8)));
	dvbs_wr_byte(0x9c5, (char)frc_demod_set);

	// Set bandwidth
	fe_l2a_set_symbol_rate(NULL, 2 * spectr_ana_data->range * 1000000);

	// start acquisition
	dvbs_write_bits(0x8c0, 0x1, 1, 1);
	msleep(20);
	dvbs_write_bits(0x8c0, 0x0, 1, 1);

	fld_value = (dvbs_rd_byte(0x8c4) & 0x80) >> 7;
	if (!fld_value) {
		contmode = (dvbs_rd_byte(0x8c6)) & 0x1;
		while ((contmode != 1) && (timeout < 40)) {
			contmode = (dvbs_rd_byte(0x8c6)) & 0x1;
			timeout = timeout + 1;
			msleep(20);
		}
	}

	//change range from hz to M
	range_m = spectr_ana_data->range;
	//search_result->result_cfo_est = (dvbs_rd_byte(0x8da) << 8) + dvbs_rd_byte(0x8db);
	//indicates the bin of the highest peak
	bin_max = (dvbs_rd_byte(0x8c8) << 8) + dvbs_rd_byte(0x8c9);

	if (bin_max > 4095)
		bin_max = bin_max - 8191;
	search_result->result_bw = range_m * 2000 * (8192 - bin_max) / 8192;
	cte_freq_ok = ((dvbs_rd_byte(0x8c6)) & 0x10) >> 4;

	if (cte_freq_ok == 1) {
		if ((search_result->result_bw < 1100 * range_m * 2) &
			(search_result->result_bw > 550 * range_m * 2)) {
			if (bin_max >= 0) {
				search_result->result_frc = spectr_ana_data->in_bw_center_frc - frq;
				search_result->found_ok = 1;
			} else if ((bin_max < 0) & (search_result->result_bw >= (range_m * 2000)) &
				((abs(bin_max * 10) / 8192) < 1)) {
				search_result->result_frc = spectr_ana_data->in_bw_center_frc - frq;
				search_result->found_ok = 1;
			} else {
				search_result->found_ok = 0;
			}
		} else {
			search_result->found_ok = 0;
		}
	} else {
		search_result->found_ok = 0;
	}

	// Empty hardware fft
	dvbs_write_bits(0x8ed, 0x0, 0, 3);
	dvbs_wr_byte(0x8ee, 0x0);
	// Sleep down hardware fft
	dvbs_write_bits(0x8c0, 0x1, 1, 1);
}

void dvbs_blind_fft_result_handle(struct fft_total_result *result)
{
	int m = 0;
	int n = 0;
	unsigned int frc_data_tmp = 0;
	unsigned int bw_data_tmp = 0;
	unsigned int iq_data_tmp = 0;

	for (m = 0; m < (result->tp_num - 1); m++) {
		for (n = 0; n < (result->tp_num - 1 - m); n++) {
			if (result->freq[n] > result->freq[n + 1]) {
				frc_data_tmp = result->freq[n];
				result->freq[n] = result->freq[n + 1];
				result->freq[n + 1] = frc_data_tmp;

				bw_data_tmp = result->bw[n];
				result->bw[n] = result->bw[n + 1];
				result->bw[n + 1] = bw_data_tmp;

				iq_data_tmp = result->iq_swap[n];
				result->iq_swap[n] = result->iq_swap[n + 1];
				result->iq_swap[n + 1] = iq_data_tmp;
			}
		}
	}
}

unsigned int fe_l2a_get_AGC2_accu(void)
{
	unsigned int AGC2_accu = 0;
	unsigned int mantissa = 0;
	int exponent = 0;
	unsigned int AGC2I1 = 0, AGC2I0 = 0;
#ifdef DVBS_BLIND_SCAN_DEBUG
	unsigned int fld_value[2] = { 0 };

	fld_value[0] = dvbs_rd_byte(0x9a0);
	fld_value[1] = (dvbs_rd_byte(0x9a1) & 0xc0) >> 6;
	mantissa = fld_value[1] + (fld_value[0] << 2);
	fld_value[0] = dvbs_rd_byte(0x9a1) & 0x3f;
	exponent = (int)(fld_value[0] - 4); //5 - 9

	if (exponent >= 0)
		AGC2_accu = mantissa * (1 << exponent);

	PR_DVBS("1 AGC2_accu: %d(0x%x), mantissa: %d, exponent: %d\n",
			AGC2_accu, AGC2_accu, mantissa, exponent);

	/* Georg's method */
	fld_value[0] = dvbs_rd_byte(0x9a0);
	fld_value[1] = (dvbs_rd_byte(0x9a1) & 0xc0) >> 6;
	mantissa = ((fld_value[0] << 8) | fld_value[1]) >> 6;
	exponent = (int)((dvbs_rd_byte(0x9a1) & 0x3f) - 9);

	if (exponent >= 0)
		AGC2_accu = mantissa * (1 << exponent);

	PR_DVBS("2 AGC2_accu: %d(0x%x), mantissa: %d, exponent: %d\n",
			AGC2_accu, AGC2_accu, mantissa, exponent);
#endif
	/* Damien's method */
	/*************** calculate AGC2 **************************/
	/* Agc2 = (AGC2I1 * 4 + AGC2I1) * 2^X toPowerY (exp - 9) */
	/* exp min = 5, max = 15                                 */
	/* so (exp - 9) min = -4, max = 6                        */
	/*********************************************************/
	AGC2I1 = dvbs_rd_byte(0x9a0);
	AGC2I0 = dvbs_rd_byte(0x9a1);
	mantissa = ((AGC2I1 * 4) + ((AGC2I0 >> 6) & 0x3)) & 0xffff;
	exponent = AGC2I0 & 0x3f;

	/* divide the mantissa by 2^abs(exponent - 9) */
	exponent = exponent - 9;
	if (exponent < 0) {
		AGC2_accu = 2 << abs(exponent);
		AGC2_accu = (1000 * mantissa) / AGC2_accu;
	} else {
		AGC2_accu = 2 << exponent;
		AGC2_accu = (1000 * mantissa) * AGC2_accu;
	}

	PR_DVBS("3 AGC2_accu: %d(0x%x), mantissa: %d, exponent: %d\n",
		AGC2_accu, AGC2_accu, mantissa, exponent);

	return AGC2_accu;
}

unsigned int dvbs_blind_check_AGC2_bandwidth_new(int *next_step_khz,
		int *next_step_khz1, int *signal_state)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();
	int i = 0, j = 0, k = 0, l = 0, m = 0, n = 0;
	int nb_steps = 0, init_freq = 0, freq_step = 0;
	unsigned int tmp1 = 0, tmp2 = 0, tmp3 = 0;
	unsigned int agc2_level = 0, agc2_ratio = 0, accu_level = 0;
	unsigned int min_agc2_level = 0, max_agc2_level = 0, mid_agc2_level = 0;
	unsigned int agc2_level_tab[20] = { 0 };
	unsigned char div = 2, wait_for_fall = 0;
	unsigned int asperity = 0;
#ifdef DVBS_BLIND_SCAN_DEBUG
	int tmp = 0;
#endif

	PR_DVBS("[blind_search_agc2bandwidth %d, blind_search_bw_min %d]",
			blind_search_agc2bandwidth, blind_search_bw_min);
	PR_DVBS("[blind_search_pow_th %d]\n",
			blind_search_pow_th);

	dvbs_wr_byte(0x924, 0x1c); //demod stop
	tmp2 = dvbs_rd_byte(0x9b0);
	dvbs_wr_byte(0x9b0, 0x6);

	tmp3 = dvbs_rd_byte(0x9b5);
	dvbs_wr_byte(0x9b5, 0x00);
	dvbs_wr_byte(0x991, 0x40); //default 0x38

	tmp1 = dvbs_rd_byte(0x922);
	dvbs_write_bits(0x922, 1, 6, 1);
	dvbs_write_bits(0x922, 1, 7, 1);
	/* Enable the SR SCAN */
	dvbs_write_bits(0x922, 0, 4, 1);
	/* activate the carrier frequency search loop */
	dvbs_write_bits(0x922, 0, 3, 1);

	/* AGC2 bandwidth * 0.5M */
	fe_l2a_set_symbol_rate(NULL, 1000000 / div);

	nb_steps = (45000000 / 2000000) * div;
	//if (nb_steps <= 0)
	//	nb_steps = 1;

	/* AGC2 step is 1 / div MHz */
	//freq_step = 1000 * ((ALIGN_24 + ADC_CLK_135M / 2) / ADC_CLK_135M) / div;
	//PR_DVBS("1 freq_step: %d\n", freq_step);
	freq_step = ((1000000 << 8) / ((ADC_CLK_135M * 1000) >> 8)) / div;
	freq_step = freq_step << 8;
	PR_DVBS("2 freq_step: %d\n", freq_step);

	init_freq = dvbs_iq_swap ? (0 - (freq_step * nb_steps)) : freq_step * nb_steps;

	if (*signal_state == 1)
		wait_for_fall = 1;
	else
		wait_for_fall = 0;

	for (i = 0; i < nb_steps * 2; ++i) {
		if (devp->blind_scan_stop)
			break;

		PR_DVBS("init_freq: %d, freq_step: %d, nb_steps: %d\n",
				init_freq, freq_step, nb_steps);

		dvbs_wr_byte(0x924, 0x1c);

		dvbs_wr_byte(0x9c3, (init_freq >> 16) & 0xff);
		dvbs_wr_byte(0x9c4, (init_freq >> 8) & 0xff);
		dvbs_wr_byte(0x9c5, init_freq  & 0xff);
#ifdef DVBS_BLIND_SCAN_DEBUG
		tmp = ((dvbs_rd_byte(0x9c3) & 0xff) << 16) |
				((dvbs_rd_byte(0x9c4) & 0xff) << 8) |
				(dvbs_rd_byte(0x9c5) & 0xff);
		if (tmp & 0x800000)
			tmp = (((~tmp) & 0x7fffff) + 1) * (-1);

		PR_DVBS("get hw carrier_freq1 = %d\n", tmp);
#endif
		dvbs_wr_byte(0x924, 0x18); //Warm start
		usleep_range(5 * 1000, 6 * 1000);//msleep(5);

		agc2_level = fe_l2a_get_AGC2_accu();
		if (agc2_level > 0x1fffff)
			agc2_level = 0x1fffff;

		PR_DVBS("agc2_level: %d(0x%x), i: %d, j: %d\n",
				agc2_level, agc2_level, i, j);

		if (i == 0) {
			//min_agc2_level = agc2_level;
			//max_agc2_level = agc2_level;
			//mid_agc2_level = agc2_level;

			for (l = 0; l < 10 * div; ++l)
				agc2_level_tab[l] = agc2_level;

		} else {
			k = i % (10 * div);
			agc2_level_tab[k] = agc2_level;

			min_agc2_level = 0x3fffff;
			max_agc2_level = 0x0000;
			accu_level = 0;

			/* Min and max detection */
			for (l = 0; l < 10 * div; ++l) {
				if (agc2_level_tab[l] < min_agc2_level)
					min_agc2_level = agc2_level_tab[l];

				if (agc2_level_tab[l] > max_agc2_level)
					max_agc2_level = agc2_level_tab[l];

				accu_level = accu_level + agc2_level_tab[l];
			}

			mid_agc2_level = accu_level / (10 * div);

			if (!wait_for_fall) {
				agc2_ratio = (max_agc2_level - min_agc2_level) * 128;
				agc2_ratio = agc2_ratio / mid_agc2_level;
			} else {
				agc2_ratio = (agc2_level - min_agc2_level) * 128;
				agc2_ratio = agc2_ratio / mid_agc2_level;
			}

			if (agc2_ratio > 0xffff)
				agc2_ratio = 0xffff;

			PR_DVBS("i: %d, j: %d\n", i, j);
			PR_DVBS("agc2_level     %d\n", agc2_level);
			PR_DVBS("max_agc2_level %d\n", max_agc2_level);
			PR_DVBS("min_agc2_level %d\n", min_agc2_level);
			PR_DVBS("mid_agc2_level %d\n", mid_agc2_level);
			PR_DVBS("agc2_ratio     %d\n", agc2_ratio);

			if (agc2_ratio > blind_search_agc2bandwidth &&
				agc2_level == min_agc2_level &&
				agc2_level < blind_search_pow_th &&
				wait_for_fall == 0 &&
				*signal_state == 0 &&
				i < 84) {
				/* rising edge */

				PR_DVBS("Find first edge is rising ...\n");
				PR_DVBS("agc2_ratio 0x%x, min_agc2_level 0x%x, agc2_level 0x%x)\n",
						agc2_ratio, min_agc2_level, agc2_level);

				/* The first edge is rising */
				//asperity = 1;
				wait_for_fall = 1;
				j = 0;

			PR_DVBS("rising: i %d, j %d, m %d, n %d, asperity %d, init_freq %d\n",
					i, j, m, n, asperity, init_freq);

				for (l = 0; l < 10 * div; l++)
					agc2_level_tab[l] = agc2_level;

			} else if (agc2_ratio > blind_search_agc2bandwidth &&
				min_agc2_level < blind_search_pow_th &&
				agc2_level == max_agc2_level && 1 == wait_for_fall) {
				/* falling edge */

				if (*signal_state == 0) {
					if (m > blind_search_bw_min)
						n = m;

					if (m == 0)
						wait_for_fall = 3;
					else if (m <= blind_search_bw_min)
						wait_for_fall = 0;
					else
						wait_for_fall = 2;

					m = 0;
				} else {
					wait_for_fall = 0;
				}

				PR_DVBS("Find first edge is falling\n");
				PR_DVBS("agc2_ratio 0x%x, min_agc2_level 0x%x)\n",
						agc2_ratio, min_agc2_level);

				/* the first edge is falling */
				//if (!wait_for_fall)
				//	asperity = 2;
				//else
				//	asperity = 1;

			PR_DVBS("falling: i %d, j %d, m %d, n %d, asperity %d, init_freq %d\n",
					i, j, m, n, asperity, init_freq);

				//if (j == 1) {
					for (l = 0; l < 10 * div; l++)
						agc2_level_tab[l] = agc2_level;

					/* All reset */
					//j = 0;
					//wait_for_fall = 0;
					//asperity = 0;
				//} else {
				//	break;
				//}
			} else if (agc2_ratio > blind_search_pow_th && 1 == wait_for_fall) {
				wait_for_fall = 0;
				m = 0;
			}

			//if (wait_for_fall == 1 && j == (5 * div))
			//	break;

			if (wait_for_fall == 1)
				m += 1;

			if (wait_for_fall == 2 || wait_for_fall == 3)
				j += 1;
		}

		init_freq = dvbs_iq_swap ? (init_freq + freq_step) : (init_freq - freq_step);
	}

	dvbs_wr_byte(0x922, tmp1);
	dvbs_wr_byte(0x9b0, tmp2);
	dvbs_wr_byte(0x9b5, tmp3);

	/* Demod Stop */
	dvbs_wr_byte(0x924, 0x1c);
	dvbs_wr_byte(0x9c4, 0x0);
	dvbs_wr_byte(0x9c5, 0x0);

	/* asperity == 1, means rising edge followed by a constant level
	 * or a falling edge.
	 * others, means falling edge.
	 */

	if (n == 0 && j == 0) {
		if (wait_for_fall == 1 && *signal_state == 0) {
			asperity = 0;
			*next_step_khz = 40000;
			*next_step_khz1 = (1000 / div) * (m - 44);
			*signal_state = 1;
		} else if (wait_for_fall == 0 && *signal_state == 1) {
			asperity = 0;
			*next_step_khz = 0;
			*next_step_khz1 = (1000 / div) * (m - 44);
			*signal_state = 2;
		} else {
			asperity = 0;
			*next_step_khz = 40000;
		}
	} else if (n != 0 && j == 0) {
		asperity = 1;
		if (m != 0)
			*next_step_khz = 40000 - (1000 / div) * m;
		else
			*next_step_khz = 40000;
	} else {
		asperity = 1;
		if (m != 0)
			*next_step_khz = 40000 - (1000 / div) * m;
		else
			*next_step_khz = 40000;
	}

	PR_DVBS("asperity %d, next_step_khz %d, next_step_khz1 %d\n",
			asperity, *next_step_khz, *next_step_khz1);
	PR_DVBS("i %d, j %d, m %d, n %d, state %d\n",
			i, j, m, n, *signal_state);

	return asperity;
}

unsigned int dvbs_blind_check_AGC2_bandwidth_old(int *next_step_khz)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();
	int i = 0, j = 0, k = 0, l = 0;
	int nb_steps = 0, init_freq = 0, freq_step = 0;
	unsigned int tmp1 = 0, tmp2 = 0, tmp3 = 0;
	unsigned int agc2_level = 0, agc2_ratio = 0, accu_level = 0;
	unsigned int min_agc2_level = 0, max_agc2_level = 0, mid_agc2_level = 0;
	unsigned int agc2_level_tab[20] = { 0 };
	unsigned char div = 2, wait_for_fall = 0;
	unsigned int asperity = 0;
#ifdef DVBS_BLIND_SCAN_DEBUG
	int tmp = 0;
#endif

	dvbs_wr_byte(0x924, 0x1c); //demod stop
	tmp2 = dvbs_rd_byte(0x9b0);
	dvbs_wr_byte(0x9b0, 0x6);

	tmp3 = dvbs_rd_byte(0x9b5);
	dvbs_wr_byte(0x9b5, 0x00);
	dvbs_wr_byte(0x991, 0x40); //default 0x38

	tmp1 = dvbs_rd_byte(0x922);
	dvbs_write_bits(0x922, 1, 6, 1);
	dvbs_write_bits(0x922, 1, 7, 1);
	/* Enable the SR SCAN */
	dvbs_write_bits(0x922, 0, 4, 1);
	/* activate the carrier frequency search loop */
	dvbs_write_bits(0x922, 0, 3, 1);

	/* AGC2 bandwidth * 0.5M */
	fe_l2a_set_symbol_rate(NULL, 1000000 / div);

	nb_steps = (45000000 / 2000000) * div;
	//if (nb_steps <= 0)
	//	nb_steps = 1;

	/* AGC2 step is 1 / div MHz */
	//freq_step = 1000 * ((ALIGN_24 + ADC_CLK_135M / 2) / ADC_CLK_135M) / div;
	//PR_DVBS("1 freq_step: %d\n", freq_step);
	freq_step = ((1000000 << 8) / ((ADC_CLK_135M * 1000) >> 8)) / div;
	freq_step = freq_step << 8;
	PR_DVBS("2 freq_step: %d\n", freq_step);

	init_freq = freq_step * nb_steps;

	for (i = 0; i < nb_steps * 2; ++i) {
		if (devp->blind_scan_stop)
			break;

		PR_DVBS("init_freq: %d, freq_step: %d, nb_steps: %d\n",
				init_freq, freq_step, nb_steps);

		dvbs_wr_byte(0x924, 0x1c);

		dvbs_wr_byte(0x9c3, (init_freq >> 16) & 0xff);
		dvbs_wr_byte(0x9c4, (init_freq >> 8) & 0xff);
		dvbs_wr_byte(0x9c5, init_freq  & 0xff);
#ifdef DVBS_BLIND_SCAN_DEBUG
		tmp = ((dvbs_rd_byte(0x9c3) & 0xff) << 16) |
				((dvbs_rd_byte(0x9c4) & 0xff) << 8) |
				(dvbs_rd_byte(0x9c5) & 0xff);
		if (tmp & 0x800000)
			tmp = (((~tmp) & 0x7fffff) + 1) * (-1);

		PR_DVBS("get hw carrier_freq1 = %d\n", tmp);
#endif
		dvbs_wr_byte(0x924, 0x18); //Warm start
		usleep_range(5 * 1000, 6 * 1000);//msleep(5);

		agc2_level = fe_l2a_get_AGC2_accu();
		if (agc2_level > 0x1fffff)
			agc2_level = 0x1fffff;

		PR_DVBS("agc2_level: %d(0x%x), i: %d, j: %d\n",
				agc2_level, agc2_level, i, j);

		if (i == 0) {
			//min_agc2_level = agc2_level;
			//max_agc2_level = agc2_level;
			//mid_agc2_level = agc2_level;

			for (l = 0; l < 5 * div; ++l)
				agc2_level_tab[l] = agc2_level;

		} else {
			k = i % (5 * div);
			agc2_level_tab[k] = agc2_level;

			min_agc2_level = 0x3fffff;
			max_agc2_level = 0x0000;
			accu_level = 0;

			/* Min and max detection */
			for (l = 0; l < 5 * div; ++l) {
				if (agc2_level_tab[l] < min_agc2_level)
					min_agc2_level = agc2_level_tab[l];

				if (agc2_level_tab[l] > max_agc2_level)
					max_agc2_level = agc2_level_tab[l];

				accu_level = accu_level + agc2_level_tab[l];
			}

			mid_agc2_level = accu_level / (5 * div);

			if (!wait_for_fall) {
				agc2_ratio = (max_agc2_level - min_agc2_level) * 128;
				agc2_ratio = agc2_ratio / mid_agc2_level;
			} else {
				agc2_ratio = (agc2_level - min_agc2_level) * 128;
				agc2_ratio = agc2_ratio / mid_agc2_level;
			}

			if (agc2_ratio > 0xffff)
				agc2_ratio = 0xffff;

			PR_DVBS("i: %d, j: %d\n", i, j);
			PR_DVBS("agc2_level     %d\n", agc2_level);
			PR_DVBS("max_agc2_level %d\n", max_agc2_level);
			PR_DVBS("min_agc2_level %d\n", min_agc2_level);
			PR_DVBS("mid_agc2_level %d\n", mid_agc2_level);
			PR_DVBS("agc2_ratio     %d\n", agc2_ratio);

			if (agc2_ratio > BLIND_SEARCH_AGC2BANDWIDTH_40 &&
				agc2_level == min_agc2_level &&
				agc2_level < BLIND_SEARCH_POW_TH) {
				/* rising edge */

				PR_DVBS("Find first edge is rising\n");
				PR_DVBS("agc2_ratio 0x%x, min_agc2_level 0x%x, agc2_level 0x%x)\n",
						agc2_ratio, min_agc2_level, agc2_level);

				/* The first edge is rising */
				asperity = 1;
				wait_for_fall = 1;

				PR_DVBS("rising: i %d, j %d, asperity %d, init_freq %d\n",
						i, j, asperity, init_freq);

				for (l = 0; l < 5 * div; l++)
					agc2_level_tab[l] = agc2_level;

			} else if (agc2_ratio > BLIND_SEARCH_AGC2BANDWIDTH_40 &&
						min_agc2_level < BLIND_SEARCH_POW_TH) {
				/* falling edge */

				PR_DVBS("Find first edge is falling\n");
				PR_DVBS("agc2_ratio 0x%x, min_agc2_level 0x%x).\n",
						agc2_ratio, min_agc2_level);

				/* the first edge is falling */
				if (!wait_for_fall)
					asperity = 2;
				else
					asperity = 1;

				PR_DVBS("falling: i %d, j %d, asperity %d, init_freq %d\n",
						i, j, asperity, init_freq);

				if (j == 1) {
					for (l = 0; l < 5 * div; l++)
						agc2_level_tab[l] = agc2_level;

					/* All reset */
					j = 0;
					wait_for_fall = 0;
					asperity = 0;
				} else {
					break;
				}
			}

			if (wait_for_fall == 1 && j == (5 * div))
				break;

			if (wait_for_fall == 1)
				j += 1;
		}

		init_freq = init_freq - freq_step;
	}

	dvbs_wr_byte(0x922, tmp1);
	dvbs_wr_byte(0x9b0, tmp2);
	dvbs_wr_byte(0x9b5, tmp3);

	/* Demod Stop */
	dvbs_wr_byte(0x924, 0x1c);
	dvbs_wr_byte(0x9c4, 0x0);
	dvbs_wr_byte(0x9c5, 0x0);

	/* asperity == 1, means rising edge followed by a constant level
	 * or a falling edge.
	 * others, means falling edge.
	 */

	*next_step_khz = 0;

	return asperity;
}

void dvbs_set_iq_swap(unsigned int iq_swap)
{
	dvbs_iq_swap = iq_swap;
}

unsigned int dvbs_get_iq_swap(void)
{
	return dvbs_iq_swap;
}

