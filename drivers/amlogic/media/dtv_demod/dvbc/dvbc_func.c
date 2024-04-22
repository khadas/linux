// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include "aml_demod.h"
#include "demod_func.h"
#include <linux/kthread.h>

#define BLIND_SEARCH_POW_TH1_DVBC			70000
#define BLIND_SEARCH_POW_TH2_DVBC			100000
#define BLIND_SEARCH_AGC2BANDWIDTH1_DVBC	60//40tzx 0827
#define BLIND_SEARCH_AGC2BANDWIDTH2_DVBC	80//40tzx 0827
#define BLIND_SEARCH_BW_MIN_DVBC			6
#define BLIND_SEARCH_OFT_BW_DVBC			1

static unsigned char blind_spectrum_invert = 1;
module_param(blind_spectrum_invert, byte, 0644);
MODULE_PARM_DESC(blind_spectrum_invert, "");

struct timer_list mytimer;

static void dvbc_cci_timer(struct timer_list *timer)
{
}

#ifdef AML_DEMOD_SUPPORT_DVBC
int dvbc_timer_init(void)
{
	timer_setup(&mytimer, dvbc_cci_timer, 0);
	mytimer.expires = jiffies + 2 * HZ;
	add_timer(&mytimer);

	return 0;
}

void dvbc_timer_exit(void)
{
	del_timer(&mytimer);
}
#endif

int dvbc_set_ch(struct aml_dtvdemod *demod, struct aml_demod_dvbc *demod_dvbc,
		struct dvb_frontend *fe)
{
	int ret = 0;
	u16 symb_rate;
	u8 mode;
	u32 ch_freq;

	PR_DVBC("[id %d] f=%d, s=%d, q=%d\n", demod->id, demod_dvbc->ch_freq,
			demod_dvbc->symb_rate, demod_dvbc->mode);
	mode = demod_dvbc->mode;
	symb_rate = demod_dvbc->symb_rate;
	ch_freq = demod_dvbc->ch_freq;

	if (mode == QAM_MODE_AUTO) {
		/* auto QAM mode, force to QAM256 */
		mode = QAM_MODE_256;
		PR_DVBC("[id %d] auto QAM, set mode %d\n", demod->id, mode);
	}

	demod->demod_status.ch_mode = mode;
	/* 0:16, 1:32, 2:64, 3:128, 4:256 */
	demod->demod_status.agc_mode = 1;
	/* 0:NULL, 1:IF, 2:RF, 3:both */
	demod->demod_status.ch_freq = ch_freq;
	demod->demod_status.ch_bw = 8000;
	if (demod->demod_status.ch_if == 0)
		demod->demod_status.ch_if = DEMOD_5M_IF;
	demod->demod_status.symb_rate = symb_rate;

	if (!cpu_after_eq(MESON_CPU_MAJOR_ID_TL1) && cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX))
		demod->demod_status.adc_freq = demod_dvbc->dat0;

	demod_dvbc_qam_reset(demod);

	if (is_meson_gxtvbb_cpu() || is_meson_txl_cpu())
		dvbc_reg_initial_old(demod);
	else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX) && !is_meson_txhd_cpu())
		dvbc_reg_initial(demod, fe);

	return ret;
}

#ifdef AML_DEMOD_SUPPORT_DVBC
static unsigned int fe_l2a_blind_check_agc2_bandwidth(struct aml_dtvdemod *demod,
		struct fe_l2a_internal_param *pparams, unsigned int spectrum_invert)
{
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	//unsigned int minagc2level = 0xffff, maxagc2level = 0x0000, midagc2level,
	//unsigned int minagc2level = 0x3ffff, maxagc2level = 0x0000, midagc2level,
	unsigned int minagc2level = 0x3fffff, maxagc2level = 0x0000, midagc2level;
	unsigned int agc2level, agc2ratio;
	int init_freq, freq_step, tmp;
	unsigned long long tmp1, tmp2, tmp4;
	int tmp_f;
	unsigned int asperity = 0;
	unsigned int waitforfall = 0;
	unsigned int acculevel = 0;
	unsigned char div = 2;
	unsigned int agc2leveltab[20] = {0};
	int i, j = 0, k, l, m = 0, n = 0, nbsteps;

	if (devp->blind_scan_stop)
		return 0;

	if (pparams->state == 1)
		waitforfall = 1;
	else
		waitforfall = 0;

	dvbs_wr_byte(0x924, 0x1c);//demod stop
	tmp2 = dvbs_rd_byte(0x9b0);
	dvbs_wr_byte(0x9b0, 0x6);

	tmp4 = dvbs_rd_byte(0x9b5);
	dvbs_wr_byte(0x9b5, 0x00);
	dvbs_wr_byte(0x991, 0x40);

	tmp1 = dvbs_rd_byte(0x922);
	dvbs_write_bits(0x922, 1, 6, 1);
	dvbs_write_bits(0x922, 1, 7, 1);
	/* Enable the SR SCAN */
	dvbs_write_bits(0x922, 0, 4, 1);
	/* activate the carrier frequency search loop*/
	dvbs_write_bits(0x922, 0, 3, 1);
	//AGC2 bandwidth is 1/div MHz //=0.5M
	fe_l2a_set_symbol_rate(pparams, (unsigned int)(1000000 / div));

	nbsteps = ((signed int)pparams->tuner_bw / 2000000) * div;//4 //8=45000000/2000000*2
	PR_DVBC("nbsteps %d(44)\n", nbsteps);
	if (nbsteps <= 0)
		nbsteps = 1;

	/* AGC2 step is 1/div MHz */
	freq_step = (signed int)(((1000000 << 8) / (pparams->master_clock >> 8)) / div);//0.5M
	freq_step = (signed int)(freq_step << 8);
	PR_DVBC("freq_step %d\n", freq_step);
	//init_freq = 0;////////----------------------1--------------------------
	if (spectrum_invert == 0)
		init_freq = 0 + freq_step * nbsteps;
	else
		init_freq = 0 - freq_step * nbsteps;

	//for(i = 0; i < nbsteps; i++) {
	for (i = 0; i < nbsteps * 2; i++) {////////-------------------3------------------
		// Scan on the positive part of the tuner Bw

		if (devp->blind_scan_stop)
			break;

		dvbs_wr_byte(0x924, 0x1c);
		dvbs_wr_byte(0x9c3, (init_freq >> 16) & 0xff);
		dvbs_wr_byte(0x9c4, (init_freq >> 8) & 0xff);
		dvbs_wr_byte(0x9c5, init_freq  & 0xff);

		if (dvbs_rd_byte(0x9c3) > 127)
			tmp = (long long)(((255) * 65536LL * 256) +
				((dvbs_rd_byte(0x9c3)) * 65536) + ((dvbs_rd_byte(0x9c4)) * 256) +
				(dvbs_rd_byte(0x9c5)));
		else
			tmp = (long long)(((dvbs_rd_byte(0x9c3)) * 65536) +
					((dvbs_rd_byte(0x9c4)) * 256) + (dvbs_rd_byte(0x9c5)));
		PR_DVBC("hw carrier_freq1 %d\n", tmp);

		tmp_f = tmp * 135 / 16777216;
		PR_DVBC("hw carrier_freq2 %d %d Mhz\n", tmp, tmp_f);
		dvbs_wr_byte(0x924, 0x18);//Warm start
		//dvbs2_write_byte(0x912, 0x0);
		//WAIT_N_MS(5);
		usleep_range(5000, 5001);

		agc2level = 0;
		//printf("fe_l2a_GetAGC2Accu---------\n");

		fe_l2a_get_agc2accu(pparams, &agc2level);

		if (agc2level > 0x1fffff)//0x1ffff
			agc2level = 0x1fffff;//0x1ffff
		//printf("clip agc2level is %d\n",agc2level);

		if (i == 0) {
			minagc2level = agc2level;
			maxagc2level = agc2level;
			midagc2level = agc2level;

			for (l = 0; l < 20; l++)
				agc2leveltab[l] = agc2level;
		} else {
			k = i % (20);

			if (i == 44)
				PR_DVBC("i %d,agc2level %d,maxagc2level %d\n",
						i, agc2level, maxagc2level);
			PR_DVBC("minagc2level %d, midagc2level %d\n",
					minagc2level, midagc2level);

			if ((minagc2level > (agc2level * 2)) && i == 44)
				agc2leveltab[k] = midagc2level;
			else
				agc2leveltab[k] = agc2level;

			minagc2level = 0x3fffff;//0x3ffff
			maxagc2level = 0x0000;
			acculevel = 0;

			for (l = 0; l < 20; l++) { // Min and max detection
				if (agc2leveltab[l] < minagc2level)
					minagc2level = agc2leveltab[l];

				if (agc2leveltab[l] > maxagc2level)
					maxagc2level = agc2leveltab[l];

				acculevel = acculevel + agc2leveltab[l];
				//printf("acculevel is %d\n",acculevel);
			}

			midagc2level = acculevel / (20);

			//if (waitforfall == 0)
			agc2ratio = (maxagc2level - minagc2level) * 128 / midagc2level;
			//else
				//agc2ratio = (maxagc2level - minagc2level) * 128 / midagc2level;

			if (agc2ratio > 0xffff)
				agc2ratio = 0xffff;

			PR_DVBC("i %d,j %d,agc2level %d,maxagc2level %d,minagc2level %d\n",
					i, j, agc2level, maxagc2level, minagc2level);
			PR_DVBC("midagc2level %d,agc2ratio %d,waitforfall %d\n",
					midagc2level, agc2ratio, waitforfall);

			if (agc2ratio > BLIND_SEARCH_AGC2BANDWIDTH1_DVBC && //40
					agc2level == minagc2level &&
					agc2level < BLIND_SEARCH_POW_TH1_DVBC &&
					waitforfall == 0 && pparams->state == 0 && i < 84) {
				//asperity = 1; // The first edge is rising

				//if (2 == waitforfall)
				//	n = m;

				waitforfall = 1;
				j = 0;

				PR_DVBC("The first edge rising\n");
				PR_DVBC("i %d,j %d,m %d,n %d,asperity %d\n",
						i, j, m, n, asperity);
				PR_DVBC("init_freq %d,agc2ratio %d\n", init_freq, agc2ratio);
				for (l = 0; l < 20; l++)
					agc2leveltab[l] = agc2level;
			} else if ((agc2ratio > (BLIND_SEARCH_AGC2BANDWIDTH2_DVBC)) &&
					(agc2level > BLIND_SEARCH_POW_TH2_DVBC) &&
					(agc2level == maxagc2level) && (waitforfall == 1))
			// Falling edge //
			{
				if (pparams->state == 0) {
					if (m > BLIND_SEARCH_BW_MIN_DVBC)
						n = m;

					if (m == 0)
						waitforfall = 3;
					else if (m <= BLIND_SEARCH_BW_MIN_DVBC)
						waitforfall = 0;
					else
						waitforfall = 2;

					m = 0;
				} else {
					waitforfall = 0;
				}

				PR_DVBC("The first edge falling, agc2ratio %d\n",
						agc2ratio);
				PR_DVBC("i %d,j %d,m %d,n %d,asperity %d\n",
						i, j, m, n, asperity);
				PR_DVBC("init_freq %d, agc2ratio %d\n", init_freq, agc2ratio);

				for (l = 0; l < 20; l++)
					agc2leveltab[l] = agc2level;
			}
			/*else if(agc2level > (BLIND_SEARCH_POW_TH_DVBC) && (1 == waitforfall))
			 *{
			 *	waitforfall = 0;
			 *	m = 0;
			 *}
			 */

			if (waitforfall == 1)
				m += 1;

			if (waitforfall == 2 || waitforfall == 3)
				j += 1;
		} // if (i == 0)  //

		if (spectrum_invert == 0)
			init_freq = init_freq - freq_step;//edit by yong
		else
			init_freq = init_freq + freq_step;

		PR_DVBC("init_freq %d,freq_step %d,nbsteps %d\n", init_freq, freq_step, i);
	}  // End of for (i=0; i < nbsteps) //

	dvbs_wr_byte(0x922, tmp1);
	dvbs_wr_byte(0x9b0, tmp2);
	dvbs_wr_byte(0x9b5, tmp4);
	// Demod Stop //
	dvbs_wr_byte(0x924, 0x1c);
	dvbs_wr_byte(0x9c4, 0x0);
	dvbs_wr_byte(0x9c5, 0x0);

	if (devp->blind_scan_stop)
		return 0;

	if (n == 0 && j == 0) { //rising edge followed by a constant level or a falling edge
		if (waitforfall == 1 && pparams->state == 0) {
			asperity = 0;
			pparams->tuner_index_jump = 7200;
			pparams->tuner_index_jump1 = (180 / div) * (m - 44);
			pparams->state = 1;
		} else if (waitforfall == 0 && pparams->state == 1) {
			asperity = 0;
			pparams->tuner_index_jump = 0;
			pparams->tuner_index_jump1 = (180 / div) * (m - 44);
			pparams->state = 2;
		} else {
			asperity = 0;
			pparams->tuner_index_jump = 7200;
			//pparams->tuner_index_jump =
					//40000 - (1000 / div) * (m - 4 + BLIND_SEARCH_OFT_BW);
		}

		PR_DVBC("asperity 0,tuner_index_jump %d,i %d,j %d,m %d,n %d\n",
				pparams->tuner_index_jump, i, j, m, n);
		PR_DVBC("tuner_index_jump1 %d,state %d\n",
				pparams->tuner_index_jump1, pparams->state);
	} else if ((n != 0) && (j == 0)) {
	//rising edge followed by a constant level or a falling edge //
		asperity = 1;
		if (m != 0)
			pparams->tuner_index_jump = 7200 - (180 / div) * (m);
		else
			pparams->tuner_index_jump = 7200;

		PR_DVBC("asperity 1,tuner_index_jump %d,i %d,j %d\n",
				pparams->tuner_index_jump, i, j);
		PR_DVBC("m %d,n %d\n", m, n);
	} else {
		asperity = 1;

		pparams->tuner_index_jump = (180 / div) * ((44 - j) - n / 2);
		pparams->tuner_index_jump1 = 3600 + (180 / div) * (n / 2);
		pparams->sr = (180 / div) * (n);

		pparams->state = 1;
		PR_DVBC("asperity 1,tuner_index_jump %d,i %d,j %d\n",
				pparams->tuner_index_jump, i, j);
		PR_DVBC("m %d,n %d\n", m, n);
		PR_DVBC("tuner_index_jump1 %d,sr %d,state %d\n",
				pparams->tuner_index_jump1, pparams->sr, pparams->state);
	}

	PR_DVBC("asperity %d,init_freq %d,pParams->tuner_index_jump %d\n",
			asperity, init_freq, pparams->tuner_index_jump);
	return asperity;
}

static void dvbc_blind_check_signal(struct aml_dtvdemod *demod,
		unsigned int freq_khz, unsigned int *freq_add,
		unsigned int *freq_add1, unsigned int *state, unsigned int *asperity,
		unsigned int *sr_est, unsigned int spectrum_invert)
{
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	unsigned int i;
	struct fe_l2a_internal_param pparams;
	//internal_param_ptr handle={0};

	signed int	iqpower, agc1power;
	//unsigned int asperity = 0;
	unsigned int fld_value[2] = {0};
	//char cmd[50] = {0};
	bool satellite_scan = true;
	//unsigned int demodTimeout = 4500;
	//unsigned int fecTimeout = 450;

	if (devp->blind_scan_stop)
		return;

	pparams.demod_search_algo = 0;
	pparams.master_clock = 135000000;
	pparams.tuner_bw = 45000000;
	pparams.demod_search_standard = FE_SAT_AUTO_SEARCH;
	pparams.demod_symbol_rate = 1000000;

	//ChipWaitOrAbort(pParams->handle_demod, 10);
	//usleep(1000);
	//usleep(2000);//1
	// NO signal Detection //
	// Read PowerI and PowerQ To check the signal Presence //

	dvbs_wr_byte(0x120, 0x05);//adc signed

	fld_value[0] = dvbs_rd_byte(0x91a);
	fld_value[1] = dvbs_rd_byte(0x91b);

	agc1power = (signed int)(MAKEWORD(fld_value[0], fld_value[1]));

	iqpower = 0;

	if (agc1power == 0) {
		// if AGC1 integrator value is 0 then read POWERI, POWERQ registers
		// Read the IQ power value
		for (i = 0; i < 5; i++) {
			fld_value[0] = dvbs_rd_byte(0x916);
			fld_value[1] = dvbs_rd_byte(0x917);
			iqpower = iqpower + (signed int)fld_value[0] + (signed int)fld_value[1];
		}
		iqpower /= 5;
	}
	PR_DVBC("agc1power %d,iqpower %d,state %d,asperity %d\n",
			agc1power, iqpower, *state, *asperity);
	pparams.state = *state;

		//if (((agc1power != 0) || (iqpower >= powerThreshold))
	if ((*state == 0 || *state == 1) && *asperity == 0 &&
			pparams.demod_search_algo == FE_SAT_BLIND_SEARCH &&
			satellite_scan) { /* Perform edge detection */
		PR_DVBC("satellite_scan TRUE satellite_scan %d\n", satellite_scan);
		*asperity = fe_l2a_blind_check_agc2_bandwidth(demod, &pparams, spectrum_invert);
		PR_DVBC("after fe_l2a_BlindCheckAGC2BandWidth tuner_index_jump %d\n",
				pparams.tuner_index_jump);
		PR_DVBC("tuner_index_jump1 %d\n", pparams.tuner_index_jump1);

		*freq_add = pparams.tuner_index_jump;
		*freq_add1 = pparams.tuner_index_jump1;
		*sr_est = pparams.sr;

		*state = pparams.state;
		PR_DVBC("asperity %d, freq_add %d, state %d\n",
				*asperity, *freq_add, *state);
		//io_printf("agc1power is %d,iqpower is %d\n",agc1power,iqpower);
	} else if ((*state == 2) && (*asperity == 0)) {
		*asperity = 2;
	} else if ((*state == 1) && (*asperity == 1)) {
		*asperity = 2;
		*state = 2;
	}
}

//dvbc blindscan stage 1 is a special mode which really works with dvbs module, uses frontend agc.
//it's different from normal dvbs mode and dvbc mode.
int dvbc_blind_scan_process(struct aml_dtvdemod *demod)
{
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	struct dvb_frontend *fe = NULL;
	struct dtv_frontend_properties *c = NULL;
	const unsigned int MIN_FREQ_KHZ = devp->blind_min_fre / 1000 - 4000;
	const unsigned int MAX_FREQ_KHZ = devp->blind_max_fre / 1000 + 4000; //44-863M
	unsigned int found_tp_num = 0;

	unsigned int f_min, f_max, sr_est;
	int freq_add, freq_add1, freq_add_next, freq_add_dly;
	unsigned int state;
	//unsigned int fld_value[2] = {0}, agcrfin;
	unsigned int asperity = 0;
	enum fe_status status = FE_NONE;
	const unsigned int FREQ_STEP_KHZ = (MAX_FREQ_KHZ - MIN_FREQ_KHZ + 1000) / 100;
	unsigned int last_step_num = 0, cur_step_num = 0;

	//int dvbc_mode;
	//int mode;
	//int symbol_rate2;

	fe = &demod->frontend;
	if (unlikely(!fe)) {
		PR_ERR("fe NULL\n");
		return -1;
	}

	if (devp->blind_scan_stop)
		return -1;

	c = &fe->dtv_property_cache;
	c->bandwidth_hz = 8000000;

	PR_INFO("spectrum %d\n", blind_spectrum_invert);

	f_min = MIN_FREQ_KHZ - 3600;
	f_max = f_min + 3600;
	freq_add = 0;
	freq_add1 = 0;
	freq_add_next = 0;
	freq_add_dly = 0;
	state = 0;
	asperity = 0;
	sr_est = 0;

	while (f_max < MAX_FREQ_KHZ) {
		if (devp->blind_scan_stop)
			return -1;

		PR_DVBC("f_max %d KHz,freq_add %d KHz,state %d,asperity %d,freq_add_dly %d\n",
				f_max, freq_add, state, asperity, freq_add_dly);

		if (state == 1 && asperity == 0) {
			f_max = f_max + 7200;
			freq_add_dly = freq_add1;
		} else if (state == 2 && asperity == 0) {
			freq_add_next = 7200 + 0 + (freq_add_dly + freq_add1) / 2;
			f_max = f_max - 3600 + (freq_add1 - freq_add_dly) / 2;
			sr_est = 7200 + 0 + (freq_add_dly + freq_add1);
			PR_DVBC("f_max %d KHz,freq_add %d,freq_add1 %d,freq_add_dly %d\n",
					f_max, freq_add, freq_add1, freq_add_dly);
			PR_DVBC("freq_add_next %d\n", freq_add_next);
		} else if (state == 1 && asperity == 1) {
			f_max = f_max + freq_add;
			freq_add_next = freq_add1;
		} else {
			f_max = f_max + freq_add;
		}

		PR_DVBC("begin launch_BlindCheckAGC2BandWidth2 f_max %d\n", f_max);

		c->frequency = f_max * 1000;
		c->symbol_rate = 0;
		c->bandwidth_hz = 8000000;
		c->modulation = QAM_AUTO;
		c->rolloff = ROLLOFF_AUTO;
		tuner_set_params(fe);

		c->delivery_system = SYS_DVBC_ANNEX_A;
		c->symbol_rate = 7250000;//6875000;

		dvbs2_reg_initial(20000, 0);

		dvbc_blind_check_signal(demod, f_max, &freq_add, &freq_add1, &state, &asperity,
				&sr_est, blind_spectrum_invert);
		PR_DVBC("end launch_BlindCheckAGC2BandWidth2 f_max %d,freq_add %d\n",
				f_max, freq_add);
		PR_DVBC("state %d, asperity=%d\n", state, asperity);

		//when a signal is detected, report blind scan result
		if (asperity == 2 && f_max >= devp->blind_min_fre / 1000) {
			if (devp->blind_scan_stop)
				return -1;

			demod->blind_result_frequency = f_max * 1000;
			//demod->blind_result_symbol_rate = sr_est * 1000;
			//demod->symbol_rate_manu = sr_est;
			status = BLINDSCAN_UPDATERESULTFREQ | FE_HAS_LOCK;
			dvb_frontend_add_event(fe, status);

			found_tp_num++;
			PR_INFO("check_signal_result: freq=%d Hz, sr_est=%d bd, %d\n",
					f_max * 1000, sr_est * 1000, found_tp_num);
		}

		if (asperity == 2 && state == 2) {
			state = 0;
			freq_add = freq_add_next;
			asperity = 0;
		}

		//report blind scan progress
		cur_step_num = (f_max - MIN_FREQ_KHZ) / FREQ_STEP_KHZ;
		if (f_max >= MIN_FREQ_KHZ && cur_step_num > last_step_num) {
			PR_DVBC("last %d cur %d %d\n", last_step_num, cur_step_num, FREQ_STEP_KHZ);
			last_step_num = cur_step_num;
			demod->blind_result_frequency = cur_step_num;
			demod->blind_result_symbol_rate = 0;

			status = BLINDSCAN_UPDATEPROCESS | FE_HAS_LOCK;
			PR_DBGL("blind scan process: [%d%%].\n", demod->blind_result_frequency);
			if (demod->blind_result_frequency < 100)
				dvb_frontend_add_event(fe, status);
		}
	}

	PR_INFO("%d tps found\n", found_tp_num);
	// Get saturation level of input stages //
	//fld_value[1] = dvbs2_read_byte(0x91a);
	//fld_value[0] = dvbs2_read_byte(0x91b);
	//agcrfin = (fld_value[1] << 8) + fld_value[0];

	return 0;
}
#endif
