// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_ddc.h>
#include "hw/common.h"

void scdc_config(struct hdmitx_dev *hdev)
{
	/* from hdmi2.1/2.0 spec chapter 10.4, prior to accessing
	 * the SCDC, source devices shall verify that the attached
	 * sink Device incorporates a valid HF-VSDB in the E-EDID
	 * in which the SCDC Present bit is set (=1). Source
	 * devices shall not attempt to access the SCDC unless the
	 * SCDC Present bit is set (=1).
	 * For some special TV(bug#164688), it support 6G 4k60hz,
	 * but not declare scdc_present in EDID, so still force to
	 * send 1:40 tmds bit clk ratio when output >3.4Gbps signal
	 * to cover such non-standard TV.
	 */
	/* if change to > 3.4Gbps mode, or change from > 3.4Gbps
	 * to < 3.4Gbps mode, need to forcely update clk ratio
	 */
	if (hdev->para->tmds_clk_div40)
		scdc_wr_sink(TMDS_CFG, 3);
	else if (hdev->rxcap.scdc_present ||
		hdev->pre_tmds_clk_div40)
		scdc_wr_sink(TMDS_CFG, 0);
	else
		pr_info("ERR: SCDC not present, should not send 1:10\n");
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

	if (!hdev->rxcap.scdc_present)
		pr_debug("ERR: SCDC not present, should not access SCDC\n");
	chksum = 0;
	for (i = 0; i < 7; i++) {
		scdc_rd_sink(ERR_DET_0_L + i, &raw[i]);
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
int scdc_status_flags(struct hdmitx_dev *hdev)
{
	u8 st = 0;
	u8 locked_st = 0;

	if (!hdev->rxcap.scdc_present)
		pr_debug("ERR: SCDC not present, should not access SCDC\n");

	scdc_rd_sink(UPDATE_0, &st);
	if (st & STATUS_UPDATE) {
		scdc_rd_sink(STATUS_FLAGS_0, &locked_st);
		hdev->chlocked_st.clock_detected = locked_st & (1 << 0);
		hdev->chlocked_st.ch0_locked = !!(locked_st & (1 << 1));
		hdev->chlocked_st.ch1_locked = !!(locked_st & (1 << 2));
		hdev->chlocked_st.ch2_locked = !!(locked_st & (1 << 3));
	}
	if (st & CED_UPDATE)
		scdc_ced_cnt(hdev);
	if (st & (STATUS_UPDATE | CED_UPDATE))
		scdc_wr_sink(UPDATE_0, st & (STATUS_UPDATE | CED_UPDATE));
	if (st & CED_UPDATE) {
		if (!hdev->chlocked_st.clock_detected)
			pr_info("ced: clock undetected\n");
		if (!hdev->chlocked_st.ch0_locked)
			pr_info("ced: ch0 unlocked\n");
		if (!hdev->chlocked_st.ch1_locked)
			pr_info("ced: ch1 unlocked\n");
		if (!hdev->chlocked_st.ch2_locked)
			pr_info("ced: ch2 unlocked\n");
	}

	return st & (STATUS_UPDATE | CED_UPDATE);
}
