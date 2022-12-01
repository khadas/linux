// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_ddc.h>
#include "hw/common.h"

void scdc21_config(struct hdmitx_dev *hdev)
{
	/* TMDS 1/40 & Scramble */
	scdc21_wr_sink(SCDC_TMDS_CFG, hdev->para->tmds_clk_div40 ? 0x3 : 0);
}

/* update CED, 10.4.1.8 */
static int scdc_ced_cnt(struct hdmitx_dev *hdev)
{
	struct ced_cnt *ced = &hdev->ced_cnt;
	u8 raw[7];
	u8 chksum;
	int i;

	memset(raw, 0, sizeof(raw));
	memset(ced, 0, sizeof(struct ced_cnt));

	chksum = 0;
	for (i = 0; i < 7; i++) {
		scdc21_rd_sink(SCDC_CH0_ERRCNT_0 + i, &raw[i]);
		chksum += raw[i];
	}

	ced->ch0_cnt = raw[0] + ((raw[1] & 0x7f) << 8);
	ced->ch0_valid = (raw[1] >> 7) & 0x1;
	ced->ch1_cnt = raw[2] + ((raw[3] & 0x7f) << 8);
	ced->ch1_valid = (raw[3] >> 7) & 0x1;
	ced->ch2_cnt = raw[4] + ((raw[5] & 0x7f) << 8);
	ced->ch2_valid = (raw[5] >> 7) & 0x1;

	/* Do checksum */
	if (chksum != 0)
		pr_info("ced check sum error\n");
	if (ced->ch0_cnt)
		pr_info("ced: ch0_cnt = %d %s\n", ced->ch0_cnt,
			ced->ch0_valid ? "" : "invalid");
	if (ced->ch1_cnt)
		pr_info("ced: ch1_cnt = %d %s\n", ced->ch1_cnt,
			ced->ch1_valid ? "" : "invalid");
	if (ced->ch2_cnt)
		pr_info("ced: ch2_cnt = %d %s\n", ced->ch2_cnt,
			ced->ch2_valid ? "" : "invalid");

	return chksum != 0;
}

/* update scdc status flags, 10.4.1.7 */
/* ignore STATUS_FLAGS_1, all bits are RSVD */
int scdc21_status_flags(struct hdmitx_dev *hdev)
{
	u8 st = 0;
	u8 locked_st = 0;

	scdc21_rd_sink(SCDC_UPDATE_0, &st);
	if (st & STATUS_UPDATE) {
		scdc21_rd_sink(SCDC_STATUS_FLAGS_0, &locked_st);
		hdev->chlocked_st.clock_detected = locked_st & (1 << 0);
		hdev->chlocked_st.ch0_locked = !!(locked_st & (1 << 1));
		hdev->chlocked_st.ch1_locked = !!(locked_st & (1 << 2));
		hdev->chlocked_st.ch2_locked = !!(locked_st & (1 << 3));
	}
	if (st & CED_UPDATE)
		scdc_ced_cnt(hdev);
	if (st & (STATUS_UPDATE | CED_UPDATE))
		scdc21_wr_sink(SCDC_UPDATE_0, st & (STATUS_UPDATE | CED_UPDATE));
	if (!hdev->chlocked_st.clock_detected)
		pr_info("ced: clock undetected\n");
	if (!hdev->chlocked_st.ch0_locked)
		pr_info("ced: ch0 unlocked\n");
	if (!hdev->chlocked_st.ch1_locked)
		pr_info("ced: ch1 unlocked\n");
	if (!hdev->chlocked_st.ch2_locked)
		pr_info("ced: ch2 unlocked\n");

	return st & (STATUS_UPDATE | CED_UPDATE);
}

u16 scdc_tx_ltp0123_get(void)
{
	u8 data;
	u16 ltp;

	scdc21_rd_sink(SCDC_STATUS_FLAGS_1, &data);
	ltp = data;
	scdc21_rd_sink(SCDC_STATUS_FLAGS_2, &data);
	ltp |= data << 8;

	return ltp;
}

bool scdc_tx_frl_cfg1_set(u8 cfg1)
{
	scdc21_wr_sink(SCDC_CONFIG_1, cfg1);

	return true;
}

u8 scdc_tx_update_flags_get(void)
{
	u8 data;

	scdc21_rd_sink(SCDC_UPDATE_0, &data);
	if (data && (data & HDMI20_UPDATE_FLAGS)) {
		/* clear the flags after reading */
		scdc21_wr_sink(SCDC_UPDATE_0, HDMI20_UPDATE_FLAGS & data);
	}

	return data;
}

bool scdc_tx_update_flags_set(u8 update_flags)
{
	u8 data;

	data = update_flags & HDMI21_UPDATE_FLAGS;
	scdc21_wr_sink(SCDC_UPDATE_0, data);

	return true;
}

u8 scdc_tx_flt_ready_status_get(void)
{
	u8 data;

	scdc21_rd_sink(SCDC_STATUS_FLAGS_0, &data);
	return !!(data & FLT_READY);
}

u8 scdc_tx_sink_version_get(void)
{
	u8 data;

	scdc21_rd_sink(SCDC_SINK_VER, &data);
	return data;
}

void scdc_tx_source_version_set(u8 src_ver)
{
	scdc21_wr_sink(SCDC_SOURCE_VER, src_ver);
}

u8 scdc_tx_source_test_cfg_get(void)
{
	u8 data;

	scdc21_rd_sink(SCDC_SOURCE_TEST_CONFIG, &data);
	return data;
}
