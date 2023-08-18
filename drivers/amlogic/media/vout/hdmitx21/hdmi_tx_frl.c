// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_ddc.h>
#include "hw/common.h"

#undef pr_fmt
#define pr_fmt(fmt) "TX_FRL: " fmt

#define MAX_SUPPORTED_FRL_RATE FRL_12G4L /* TODO */
#define FRL_RATE_ERR 0xff

/* unit: ms, refer to HDMI2.1 Page 107 */
#define MAX_LINKTRAINING_TIME (HZ / 5)  // 200ms
#define FRL_TX_TASK_INTERVAL 40

ulong g_flt_1; /* record the start time of FLT_UPDATE as 1 */
ulong g_flt_1_e; /* record the clear time of FLT_UPDATE */

static bool frl_schedule_work(struct frl_train_t *p, u32 delay_ms, u32 period_ms);
static bool frl_stop_work(struct frl_train_t *p);

/* get the corresponding bandwidth of current FRL_RATE, Unit: MHz */
u32 get_frl_bandwidth(const enum frl_rate_enum rate)
{
	const u32 frl_bandwidth[] = {
		[FRL_NONE] = 0,
		[FRL_3G3L] = 9000,
		[FRL_6G3L] = 18000,
		[FRL_6G4L] = 24000,
		[FRL_8G4L] = 32000,
		[FRL_10G4L] = 40000,
		[FRL_12G4L] = 48000,
	};

	if (rate > FRL_12G4L)
		return frl_bandwidth[FRL_12G4L];
	return frl_bandwidth[rate];
}

u32 calc_frl_bandwidth(u32 pixel_freq, enum hdmi_colorspace cs,
	enum hdmi_color_depth cd)
{
	u32 bandwidth;

	bandwidth = calc_tmds_bandwidth(pixel_freq, cs, cd);

	/* bandwidth = tmds_bandwidth * 24 * 1.122 */
	bandwidth = bandwidth * 24;
	bandwidth = bandwidth * 561 / 500;

	return bandwidth;
}

u32 calc_tmds_bandwidth(u32 pixel_freq, enum hdmi_colorspace cs,
	enum hdmi_color_depth cd)
{
	u32 bandwidth = pixel_freq;

	if (cs == HDMI_COLORSPACE_YUV420)
		bandwidth /= 2;
	if (cs != HDMI_COLORSPACE_YUV422) {
		if (cd == COLORDEPTH_48B)
			bandwidth = bandwidth * 2;
		else if (cd == COLORDEPTH_36B)
			bandwidth = bandwidth * 3 / 2;
		else if (cd == COLORDEPTH_30B)
			bandwidth = bandwidth * 5 / 4;
		else
			bandwidth = bandwidth * 1;
	}

	return bandwidth;
}

/* for legacy HDMI2.0 or earlier modes, still select TMDS */
/* TODO DSC modes */
enum frl_rate_enum hdmitx21_select_frl_rate(bool dsc_en, enum hdmi_vic vic,
	enum hdmi_colorspace cs, enum hdmi_color_depth cd)
{
	const struct hdmi_timing *timing;
	enum frl_rate_enum rate = FRL_NONE;
	u32 tx_frl_bandwidth = 0;
	u32 tx_tmds_bandwidth = 0;

	pr_info("dsc_en %d  vic %d  cs %d  cd %d\n", dsc_en, vic, cs, cd);
	timing = hdmitx21_gettiming_from_vic(vic);
	if (!timing)
		return FRL_NONE;

	tx_tmds_bandwidth = calc_tmds_bandwidth(timing->pixel_freq / 1000, cs, cd);
	pr_info("Hactive=%d Vactive=%d Vfreq=%d TMDS_BandWidth=%d\n",
		timing->h_active, timing->v_active,
		timing->v_freq, tx_tmds_bandwidth);
	/* If the tmds bandwidth is less than 594MHz, then select the tmds mode */
	/* the HxVp48hz is new introduced in HDMI 2.1 / CEA-861-H */
	if (timing->h_active <= 4096 && timing->v_active <= 2160 &&
		timing->v_freq != 48000 && tx_tmds_bandwidth <= 594 &&
		timing->pixel_freq / 1000 < 600)
		return FRL_NONE;
	/* tx_frl_bandwidth = tmds_bandwidth * 24 * 1.122 */
	tx_frl_bandwidth = tx_tmds_bandwidth * 24;
	tx_frl_bandwidth = tx_frl_bandwidth * 561 / 500;
	for (rate = FRL_3G3L; rate < FRL_12G4L + 1; rate++) {
		if (tx_frl_bandwidth <= get_frl_bandwidth(rate)) {
			pr_info("select frl_rate as %d\n", rate);
			return rate;
		}
	}

	return FRL_NONE;
}

#define CALC_COEFF 10000

bool frl_check_full_bw(enum hdmi_colorspace cs, enum hdmi_color_depth cd, u32 pixel_clock,
	u32 h_active, enum frl_rate_enum frl_rate, u32 *tri_bytes)
{
	u32 tmds_clock = 0;
	u32 overhead_max_num = 0;
	u32 overhead_max_den = 10000;
	u32 tri_bytes_per_line = 0;
	u32 time_for_1_active_video_line = 0;
	u32 effective_link_rate = 0;
	u32 effective_chars_per_sec = 0;
	u32 effective_active_bytes_per_line = 0;
	u32 bytes_per_active_line = 0;
	u32 frl_mega_bits_rate = 0;
	u32 temp = 0;

	if (!frl_rate || !tri_bytes)
		return 0;

	if (cs == HDMI_COLORSPACE_YUV420)
		pixel_clock = pixel_clock / 2;
	tmds_clock = pixel_clock;
	tri_bytes_per_line = h_active;
	bytes_per_active_line = h_active;
	frl_mega_bits_rate = pixel_clock;

	/* TCLK update */
	if (cs != HDMI_COLORSPACE_YUV422) {
		if (cd == COLORDEPTH_30B)
			tmds_clock = (pixel_clock * 5) / 4;
		else if (cd == COLORDEPTH_36B)
			tmds_clock = (pixel_clock * 3) / 2;
		else
			tmds_clock = pixel_clock;
	}
	if (cs == HDMI_COLORSPACE_YUV420)
		tri_bytes_per_line >>= 1;

	if (cs != HDMI_COLORSPACE_YUV422) {
		if (cd == COLORDEPTH_30B)
			tri_bytes_per_line = (tri_bytes_per_line * 5) / 4;
		else if (cd == COLORDEPTH_36B)
			tri_bytes_per_line = ((tri_bytes_per_line * 3) / 2);
	}
	tmds_clock = tmds_clock / CALC_COEFF;
	time_for_1_active_video_line = tri_bytes_per_line * 200;
	time_for_1_active_video_line = time_for_1_active_video_line / tmds_clock;
	time_for_1_active_video_line = time_for_1_active_video_line / 201;

	frl_mega_bits_rate = get_frl_bandwidth(frl_rate);
	if (frl_rate == FRL_3G3L || frl_rate == FRL_6G3L) {
		/* for 3 lanes, overhead max is 2.136% */
		overhead_max_num = 267;
		overhead_max_den = 12500;
	} else {
		/* for 4 lanes, overhead max is 2.184% */
		overhead_max_num = 273;
		overhead_max_den = 12500;
	}

	effective_link_rate = frl_mega_bits_rate * 1000 / CALC_COEFF;
	temp = effective_link_rate * 3 / 10000;
	effective_link_rate = effective_link_rate - temp;
	effective_chars_per_sec = effective_link_rate / 18;
	temp = effective_chars_per_sec * overhead_max_num / overhead_max_den;
	effective_chars_per_sec = effective_chars_per_sec - temp;
	effective_active_bytes_per_line =
		(effective_chars_per_sec * time_for_1_active_video_line) * 2;
	bytes_per_active_line = tri_bytes_per_line * 3;
	temp = bytes_per_active_line / 200;
	bytes_per_active_line = bytes_per_active_line + temp + 6;
	tri_bytes_per_line = ((tri_bytes_per_line * 3) >> 1) + 3;
	*tri_bytes = tri_bytes_per_line;

	pr_info("bytes_per_active_line %d  effective_active_bytes_per_line %d\n",
		bytes_per_active_line, effective_active_bytes_per_line);
	if (bytes_per_active_line >= effective_active_bytes_per_line)
		return 1;
	else
		return 0;
}

/* In LTS:3, increase FFE level for the specified lane */
static bool check_ffe_change_request(struct frl_train_t *p, u8 lane)
{
	bool ret;

	if (lane >= p->lane_count)
		return false;

	if (p->ffe_level[lane] == p->max_ffe_level)
		return false;

	p->ffe_level[lane]++;

	/* pr_info("FFE level(%d): %d\n", */
	/*    (uint16_t)lane, (uint16_t)p->ffe_level[lane]); */
	if (p->auto_ffe_update)
		ret = flt_tx_cmd_execute(LT_TX_CMD_TXFFE_UPDATE);
	else
		ret = frl_tx_ffe_set(p->ffe_level[lane], lane);

	return ret;
}

/*
 * source transmits the link training pattern on each
 * lane as requested in the RX Ln(x)_LTP_req register(1~8)
 */
static bool check_pattern_change(struct frl_train_t *p, u8 lane, u16 ltp0123)
{
	bool ret;
	u8 lane_pattern = (ltp0123 >> (lane * 4)) & 0x0F;

	if (p->auto_pattern_update) /* TODO */
		ret = flt_tx_cmd_execute(LT_TX_CMD_LTP_UPDATE);
	else
		ret = frl_tx_pattern_set(lane_pattern, lane);

	return ret;
}

/* Init all lanes to a starting FFE level 0 */
static void init_ffe_level(struct frl_train_t *p)
{
	memset(p->ffe_level, FFE_LEVEL0, sizeof(p->ffe_level));

	/* Set all lanes to FFE_LEVEL0 */
	frl_tx_ffe_set(FFE_LEVEL0, 0x0F);
}

/* In LTS:4, here will determines next lower FRL rate */
static bool update_frl_rate(struct frl_train_t *p)
{
	bool ret = true;

	/* TODO min_frl_rate */
	if (p->frl_rate > FRL_3G3L && p->frl_rate > p->min_frl_rate) {
		p->frl_rate--;
		pr_info("Reduce rate to %d\n", p->frl_rate);
		p->frl_rate_no_change = false;
	} else {
		pr_info("Limit to: %d\n", p->frl_rate);
		ret = false;
		p->frl_rate_no_change = true;
	}
	return ret;
}

/*
 * In LTS:P, start FRL transmission
 * Step1, gap_only = 1, send the GAP with Gap Characters, scrambling,
 * Reed-Solomon FEC, and SuperBlocks. Sink will set its FRL_Start flag.
 * Step2, gap_only = 0, send FRL with normal A/V, and control packets.
 */
static void start_frl_transmission(struct frl_train_t *p, bool gap_only)
{
	if (gap_only)
		frl_tx_av_enable(false); /* Disable the video */
	else
		frl_tx_av_enable(true); /* Enable the video */
	pr_info("FRL with %s\n", gap_only ? "Gap" : "Video");
	/* enable the super block */
	frl_tx_sb_enable(true, p->frl_rate);
}

/* stop FRL transmission */
static void stop_frl_transmission(struct frl_train_t *p)
{
	frl_tx_av_enable(false);
	frl_tx_sb_enable(false, p->frl_rate);
}

/* check if sink has set the flt_no_timeout bit */
static bool query_no_timeout(struct frl_train_t *p)
{
	p->src_test_cfg = scdc_tx_source_test_cfg_get();
	if (p->src_test_cfg)
		pr_notice("TE test config 0x%x\n", p->src_test_cfg);

	return (p->src_test_cfg & FLT_NO_TIMEOUT) != 0;
}

/* check if sink has set the FRL_Start bit */
static bool frl_start_query(struct frl_train_t *p)
{
	p->update_flags = scdc_tx_update_flags_get();
	return (p->update_flags & FRL_START) != 0;
}

/* clear the DS FLT_UPDATE flag */
static void clrear_flt_update(struct frl_train_t *p)
{
	p->update_flags |= FLT_UPDATE;
	scdc_tx_update_flags_set(p->update_flags);
	g_flt_1_e = jiffies;
	/* pr_info("Clr FLT_update\n"); */
}

/* checks if Sink has set the FLT_update bit */
static bool query_flt_update(struct frl_train_t *p)
{
	p->update_flags = scdc_tx_update_flags_get();

	if ((p->update_flags & FLT_UPDATE) != 0)
		g_flt_1 = jiffies;
/*	pr_info("FLT_update: 1\n"); */
	return (p->update_flags & FLT_UPDATE) != 0;
}

/* clear the DS FRL_start flag */
static void clear_frl_start(struct frl_train_t *p)
{
	p->update_flags |= FRL_START;
	scdc_tx_update_flags_set(p->update_flags);
/*	pr_info("Clr FRL_start\n"); */
}

/* scdc bus stall operation is async with other ddc operation
 * (such as hdcp version read), also it's used in FRL training
 * process which is time-sensitive, so not use it with mutex
 * with other ddc operation which may delay frl status polling,
 * place other ddc operation in positions where no polling
 * operation of frl status.
 */
static void poll_update_flags(struct frl_train_t *p)
{
	scdc_bus_stall_set(true);
	p->update_flags = scdc_tx_update_flags_get();
	if (p->update_flags & SOURCE_TEST_UPDATE) {
		p->src_test_cfg = scdc_tx_source_test_cfg_get();
		p->flt_no_timeout = (p->src_test_cfg & FLT_NO_TIMEOUT) != 0;
		p->txffe_pre_shoot_only = (p->src_test_cfg & TXFFE_PRE_SHOOT_ONLY) != 0;
		p->txffe_de_emphasis_only = (p->src_test_cfg & TXFFE_DE_EMPHASIS_ONLY) != 0;
		p->txffe_no_ffe = (p->src_test_cfg & TXFFE_NO_FFE) != 0;
	}
	scdc_bus_stall_set(false);
}

#define FRL_EVENT_CAPABLE   BIT(0) /* DS support FRL */
#define FRL_EVENT_INCAPABLE BIT(1) /* DS not support FRL */
#define FRL_EVENT_TIMEOUT   BIT(2) /* training timeout/200ms */
#define FRL_EVENT_PASSED    BIT(3) /* training passed, sending FRL */
#define FRL_EVENT_VIDEO     BIT(4) /* Sending video */
#define FRL_EVENT_STOP      BIT(5) /* stop FRL, moving to LTS_3 */
#define FRL_EVENT_LEGACY    BIT(6) /* select TMDS mode */
#define FRL_EVENT_CHANGE    BIT(7) /* Change FRL Rate */

/* TODO */
void frl_tx_callback(u8 event)
{
}

/* init the FRL mode */
bool frl_tx_frl_mode_init(struct frl_train_t *p,
	struct rx_cap *rxcap, bool force_legacy)
{
	u8 sink_ver;
	u8 edid_max_frl;
	bool scdc_present;

	if (!p || !rxcap)
		return false;

	edid_max_frl = rxcap->max_frl_rate;
	scdc_present = rxcap->scdc_present;
	p->flt_no_timeout = query_no_timeout(p);

	if (force_legacy) {
		p->req_legacy_mode = true;
		pr_info("set legacy TMDS mode\n");
	} else {
		p->req_legacy_mode = false;
		pr_info("set FRL mode\n");
	}

	p->max_edid_frl_rate = edid_max_frl;
	sink_ver = scdc_tx_sink_version_get();
	if (sink_ver != 1)
		pr_err("sink version %d\n", sink_ver);
	scdc_tx_source_version_set(1);

	/* check DS FRL capability */
	p->ds_frl_support = p->max_edid_frl_rate && scdc_present && sink_ver;

	p->req_frl_mode = p->req_legacy_mode ? false : p->ds_frl_support;
	/* get the maximum FRL rate between the capabilities of both the source and the sink */
	/* TODO */
	if (p->req_frl_mode) {
		p->max_frl_rate = p->user_max_frl_rate;
		p->max_frl_rate = (p->max_edid_frl_rate > p->max_frl_rate) ?
			p->max_frl_rate : p->max_edid_frl_rate;
		p->max_frl_rate = (p->max_frl_rate > MAX_SUPPORTED_FRL_RATE) ?
			MAX_SUPPORTED_FRL_RATE : p->max_frl_rate;
		p->min_frl_rate = (p->min_frl_rate > p->max_frl_rate) ?
			FRL_RATE_ERR : p->min_frl_rate;

		if (p->min_frl_rate == FRL_RATE_ERR) {
			p->min_frl_rate = p->max_frl_rate;
			p->req_frl_mode = false;
		} else {
			/* Set lane count for selected maximum FRL rate */
			p->lane_count = (p->frl_rate <= FRL_6G3L) ? 3 : 4;
		}
	} else {
		p->max_frl_rate = 0;
	}
	if (p->flt_tx_state == FLT_TX_LTS_P3 && p->max_frl_rate == p->frl_rate) {
		pr_info("Link already setup as %d. No change\n", p->frl_rate);
		return false;
	}
	/* TODO */
	p->flt_tx_state = FLT_TX_LTS_1;
	p->flt_running = true;

	frl_schedule_work(p, 0, FRL_TX_TASK_INTERVAL);

	return p->req_frl_mode;
}

const char *flt_tx_string[] = {
	[FLT_TX_LTS_L] = "LTS:L",
	[FLT_TX_LTS_1] = "LTS:1",
	[FLT_TX_LTS_2] = "LTS:2",
	[FLT_TX_LTS_3] = "LTS:3",
	[FLT_TX_LTS_4] = "LTS:4",
	[FLT_TX_LTS_P1] = "LTS:P(w/o video)",
	[FLT_TX_LTS_P2] = "LTS:P(w/ video)",
	[FLT_TX_LTS_P3] = "LTS:P(maintain)",
};

/*
 * The FRL link training procedure
 * the specification defines 10ms requirement from FLT_update set to
 * FLT_update cleared, not under TE
 */
static void tx_train_fsm(struct work_struct *work)
{
	ulong frl_tmo;
	bool b_time_out;
	struct frl_work *frl_work = container_of((struct delayed_work *)work,
		struct frl_work, dwork);
	struct frl_train_t *p = container_of(frl_work,
		struct frl_train_t, timer_frl_flt);
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (p->last_state != p->flt_tx_state) {
		pr_info("FRL: %s to %s\n", flt_tx_string[p->last_state],
			flt_tx_string[p->flt_tx_state]);
		p->last_state = p->flt_tx_state;
	}

	if (!p->flt_running)
		return;
	/* poll the source for status updates */
	poll_update_flags(p);
	switch (p->flt_tx_state) {
	case FLT_TX_LTS_1:
		if (!p->flt_running)
			return;
		pr_info("FRL: %s\n", flt_tx_string[FLT_TX_LTS_1]);
		if (p->frl_rate)
			frl_tx_tx_phy_set();
		else
			tmds_tx_phy_set();
		frl_tx_lts_1_hdmi21_config();
		stop_frl_transmission(p);
		/* LTS:1 Source reads EDID
		 * if Source selects legacy TMDS Mode, EXIT to LTS:L
		 */
		if (p->req_legacy_mode) {
			p->flt_tx_state = FLT_TX_LTS_L;
			break;
		}
		p->req_frl_mode = false;

		if (!p->ds_frl_support) {
			pr_info("Sink not support FRL\n");
			p->flt_tx_state = FLT_TX_LTS_L;
			break;
		}

		frl_tx_callback(FRL_EVENT_CAPABLE);
		p->flt_tx_state = FLT_TX_LTS_2;
		goto tx_lts_2;

	case FLT_TX_LTS_2:
tx_lts_2:
		if (!p->flt_running)
			return;
		pr_info("FRL: %s\n", flt_tx_string[FLT_TX_LTS_2]);
		/*
		 * LTS:2 Source Prepares for FRL Link Training
		 * Source polls FLT_READY
		 */
		if (!scdc_tx_flt_ready_status_get())
			break;

		/* Now FLT_READY = 1 and Source sets FRL_Rate and FFE */
		/* Source sets TxFFE= TxFFE0 for all active lanes */
		init_ffe_level(p);
		pr_info("Set FRL Rate: %d\n", p->frl_rate);

		scdc_tx_frl_cfg1_set((p->max_ffe_level << 4) | p->frl_rate);
		if (p->frl_rate == FRL_NONE) {
			frl_stop_work(p);
			return;
		}

		/*
		 * EXIT to LTS:3
		 */
		p->flt_tx_state = FLT_TX_LTS_3;
		goto tx_lts_3;

	case FLT_TX_LTS_3:
tx_lts_3:
		if (!p->flt_running)
			return;
		pr_info("FRL: %s\n", flt_tx_string[FLT_TX_LTS_3]);
		hdmitx_soft_reset(BIT(0));
		/* LTS:3 Source conducts Link Training for the specified FRL_Rate */
		frl_tx_pattern_init(0x8765);

		/* STEP1 Source starts FLT Timer */
		/* the timeout value MAX_LINKTRAINING_TIME is for each FRL_Rate */
		p->flt_timeout = false;
		frl_tmo = jiffies + MAX_LINKTRAINING_TIME;
		while (!(b_time_out = time_after(jiffies, frl_tmo)) || p->flt_no_timeout) {
			u8 lane;
			u16 ltp0123;
			static u16 ltp_pre;

			/* waits for FLT_update flag, poll every 2 ms or less */
			if (!query_flt_update(p))
				continue;
			/* FLT_update flag = 1, Source reads Ln(x)_LTP_req */
			ltp0123 = scdc_tx_ltp0123_get();
			/* handle each Ln(x)_LTP_req registers */
			if (ltp0123 == 0x0000) {
				/* Training passed, goto to LTS:P */
				p->flt_tx_state = FLT_TX_LTS_P1;
				/* directly goto tx_lts_p1 to clear LPT_update within 10ms */
				goto tx_lts_p1;
			} else if (ltp0123 == 0xFFFF) {
				/* test at a different Link rate */
				p->flt_tx_state = FLT_TX_LTS_4;
				goto tx_lts_4;
			} else {
				/* completed within 10ms to meet the specification */
				for (lane = 0; lane < p->lane_count; lane++) {
					u16 mask = 0x000F << (lane * 4);

					/* Source updates FFE setting for the specific Lane */
					if ((ltp0123 & mask) == (0xEEEE & mask))
						check_ffe_change_request(p, lane);
					else if ((ltp0123 & mask) == (0x3333 & mask))
						check_pattern_change(p, lane,
							p->flt_no_timeout ? ltp0123 : ltp_pre);
					/* Source transmits Ln(x)_LTP_req register 1 ~ 8 */
					else if ((ltp0123 & mask) != 0)
						check_pattern_change(p, lane, ltp0123);
				}
			}
			if (ltp_pre != ltp0123)
				ltp_pre = ltp0123;
			/* clear the FLT_update within 10 ms */
			clrear_flt_update(p);
			pr_info("LTS:3 LTP=0x%04x  cost %ld ms\n", ltp0123,
				(g_flt_1_e - g_flt_1 + 3) / 4);
		}
		/* time out */
		if (b_time_out && !p->flt_no_timeout) {
			pr_info("Timeout in LTS:3, --> LTS:L\n");
			frl_tx_callback(FRL_EVENT_TIMEOUT);
			p->flt_timeout = true;
			p->flt_tx_state = FLT_TX_LTS_L;
			break;
		}
		break;

	case FLT_TX_LTS_4:
tx_lts_4:
		if (!p->flt_running)
			return;
		pr_info("FRL: %s\n", flt_tx_string[FLT_TX_LTS_4]);
		/* LTS:4 start Link Training for a new rate */
		stop_frl_transmission(p);

		/* Source sets TxFFE0 for all active Lanes */
		init_ffe_level(p);

		/* reduce FRL rate and number of FFE settings for new FRL_Rate */
		if (p->req_legacy_mode || !update_frl_rate(p)) {
			p->flt_tx_state = FLT_TX_LTS_L;
		} else {
			/* set new FRL_Rate  and number of Lanes */
			scdc_tx_frl_cfg1_set((p->max_ffe_level << 4) | p->frl_rate);

			clrear_flt_update(p);
			pr_info("LTS:4 cost %ld ms\n", (g_flt_1_e - g_flt_1 + 3) / 4);
			p->flt_tx_state = FLT_TX_LTS_3;
		}
		break;

	case FLT_TX_LTS_L:
		if (!p->flt_running)
			return;
		pr_info("FRL: %s\n", flt_tx_string[FLT_TX_LTS_L]);
		/* LTS:L training failed or FRL not selected, goto legacy TMDS */
		if (p->flt_timeout || p->frl_rate_no_change || p->req_legacy_mode ||
			 (!p->ds_frl_support && p->frl_rate != FRL_NONE)) {
			/* clears FRL_Rate for TMDS */
			stop_frl_transmission(p);
			frl_tx_pattern_stop();
			p->frl_rate = FRL_NONE;
			scdc_tx_frl_cfg1_set(FRL_NONE);
			clrear_flt_update(p);
			pr_info("LTS:L cost %ld ms\n", (g_flt_1_e - g_flt_1 + 3) / 4);
			frl_tx_callback(FRL_EVENT_LEGACY);
			/* disable hdcp if fallback to legacy mode */
			hdmitx21_disable_hdcp(hdev);
			p->flt_timeout = 0;
		}
		// if (p->ds_frl_support && p->req_frl_mode)
		//	// requests a new FRL mode, goto LTS:2
		//	p->flt_tx_state = FLT_TX_LTS_2;

		p->req_legacy_mode = false;

		break;

	case FLT_TX_LTS_P1:
		/* LTS:P only run once */
tx_lts_p1:
		if (!p->flt_running)
			return;
		pr_info("FRL: %s\n", flt_tx_string[FLT_TX_LTS_P1]);
		/* LTS:P Training has passed, FRL transmission initiated
		 * STEP1 starts FRL transmission with only Gap, scrambling, ReedSolomon FEC
		 * and Super Block structure
		 */
		start_frl_transmission(p, true);
		fifo_flow_enable_intrs(1);
		frl_tx_pattern_stop();
		clrear_flt_update(p);
		pr_info("LTS:P cost %ld ms\n", (g_flt_1_e - g_flt_1 + 3) / 4);
		p->flt_tx_state = FLT_TX_LTS_P2;
		goto tx_lts_p2;
	case FLT_TX_LTS_P2:
tx_lts_p2:
		if (!p->flt_running)
			return;
		pr_info("FRL: %s\n", flt_tx_string[FLT_TX_LTS_P2]);
		/* STEP2 poll Update Flags every 2 ms or less */
		/* wait FRL_start = 1, then transmit active normal A/V, and packets */
		if (frl_start_query(p)) {
			/* starts FRL operation */
			start_frl_transmission(p, false);
			clear_frl_start(p);
			frl_tx_callback(FRL_EVENT_VIDEO | FRL_EVENT_PASSED);
			p->flt_tx_state = FLT_TX_LTS_P3;
		}
		goto tx_lts_p3;

	case FLT_TX_LTS_P3:
tx_lts_p3:
		if (!p->flt_running)
			return;
		//pr_info("FRL: %s\n", flt_tx_string[FLT_TX_LTS_P3]); // massive print
		/* if FLT_update = 1, stop FRL transmission, goto LTS:3 */
		if (query_flt_update(p)) {
			stop_frl_transmission(p);
			clrear_flt_update(p);
			frl_tx_callback(FRL_EVENT_STOP);
			p->flt_tx_state = FLT_TX_LTS_3;
			break;
		} else {
			/* start hdcp after training pass */
			/* if no hdcp2.2 key on board, then skip */
			if (get_hdcp2_lstore() &&
				hdmitx21_get_hdcp_mode() == 0) {
				/* get downstream hdcp2.2 version in certain place,
				 * as ddc stall request in poll_update_flags() may
				 * affect hdcp version read.
				 */
				if (!hdev->dw_hdcp22_cap)
					hdev->dw_hdcp22_cap = is_rx_hdcp2ver();
				queue_delayed_work(hdev->hdmi_wq, &hdev->work_start_hdcp, HZ / 4);
			}
		}
		break;
	}
	if (p->flt_running) {
		u32 frl_tick = FRL_TX_TASK_INTERVAL;

		if (p->flt_tx_state == FLT_TX_LTS_P3)
			frl_tick = 180; /* hfr1-68 requires at least 200ms */
		frl_schedule_work(p, 0, frl_tick);
	}
}

static bool frl_schedule_work(struct frl_train_t *p, u32 delay_ms, u32 period_ms)
{
	struct frl_work *work = &p->timer_frl_flt;

	//pr_info("schedule %s: delay %d ms  period %d ms\n", work->name, delay_ms, period_ms);

	delay_ms = (delay_ms + 3) / 4;
	period_ms = (period_ms + 3) / 4;

	work->delay_ms = 0;
	work->period_ms = period_ms;

	if (delay_ms == 0 && period_ms == 0) {
		frl_stop_work(p);
		return 1;
	}

	if (delay_ms)
		return queue_delayed_work(p->frl_wq, &work->dwork, delay_ms);
	else
		return queue_delayed_work(p->frl_wq, &work->dwork, period_ms);
}

static bool frl_stop_work(struct frl_train_t *p)
{
	struct frl_work *work = &p->timer_frl_flt;

	cancel_delayed_work(&work->dwork);
	pr_info("stop %s\n", work->name);
	return 0;
}

static struct frl_train_t frl_train_inst;
static struct frl_train_t *p_frl_train;

/* Stop TX training FSM when HPD is out or in the beginning of mode setting */
void frl_tx_stop(struct hdmitx_dev *hdev)
{
	if (!p_frl_train)
		return;

	frl_stop_work(p_frl_train);
	p_frl_train->flt_tx_state = FLT_TX_LTS_1;
	p_frl_train->flt_running = false;
	stop_frl_transmission(p_frl_train);
}

void frl_tx_training_handler(struct hdmitx_dev *hdev)
{
	struct rx_cap *rxcap;

	if (!p_frl_train) {
		p_frl_train = &frl_train_inst;
		p_frl_train->frl_wq = alloc_workqueue("hdmitx21_frl",
			WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
		INIT_DELAYED_WORK(&p_frl_train->timer_frl_flt.dwork, tx_train_fsm);
		p_frl_train->timer_frl_flt.name = "hdmitx21_frl";
		p_frl_train->auto_pattern_update = false;
	}

	/* TODO hard code */
	rxcap = &hdev->rxcap;
	p_frl_train->min_frl_rate = FRL_3G3L;
	/* configured in dts, maximum FRL_10G4L */
	p_frl_train->user_max_frl_rate = hdev->tx_max_frl_rate;
	p_frl_train->max_frl_rate = hdev->tx_max_frl_rate;
	p_frl_train->frl_rate = hdev->frl_rate;
	p_frl_train->flt_tx_state = FLT_TX_LTS_L;
	frl_tx_frl_mode_init(p_frl_train, rxcap, false);
}
