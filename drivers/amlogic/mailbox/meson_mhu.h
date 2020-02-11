/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define CONTROLLER_NAME		"mhu_ctlr"

#define CHANNEL_MAX		2
#define CHANNEL_LOW_PRIORITY	"cpu_to_scp_low"
#define CHANNEL_HIGH_PRIORITY	"cpu_to_scp_high"

struct mhu_data_buf {
	u32 cmd;
	int tx_size;
	void *tx_buf;
	int rx_size;
	void *rx_buf;
	void *cl_data;
};

extern struct device *the_scpi_device;
extern struct device *dsp_scpi_device;
extern u32 num_scp_chans;
extern u32 send_listen_chans;
