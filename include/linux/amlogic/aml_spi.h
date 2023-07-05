/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_SPI_H_
#define __AML_SPI_H_

struct spicc_controller_data {
	unsigned	ccxfer_en:1;
	unsigned	timing_en:1;
	unsigned	ss_leading_gap:4;
	unsigned	ss_trailing_gap:4;
	unsigned	tx_tuning:4;
	unsigned	rx_tuning:4;
	unsigned	dummy_ctl:1;
	void (*dirspi_start)(struct spi_device *spi);
	void (*dirspi_stop)(struct spi_device *spi);
	int (*dirspi_async)(struct spi_device *spi,
			    u8 *tx_buf,
			    u8 *rx_buf,
			    int len,
			    void (*complete)(void *context),
			    void *context);
	int (*dirspi_sync)(struct spi_device *spi,
			   u8 *tx_buf,
			   u8 *rx_buf,
			   int len);
};

struct spicc_transfer {
	struct spi_transfer xfer;
	unsigned	dc_level:1;
#define DC_LEVEL_LOW	0
#define DC_LEVEL_HIGH	1
	unsigned	read_turn_around:2;
	unsigned	dc_mode:2;
#define DC_MODE_NONE	0
#define DC_MODE_PIN	1
#define DC_MODE_9BIT	2
	void		*tx_ccsg;
	void		*rx_ccsg;
	int		tx_ccsg_len;
	int		rx_ccsg_len;
};

#endif
