/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 */

#ifndef _EVL_UAPI_DEVICES_SPIDEV_H
#define _EVL_UAPI_DEVICES_SPIDEV_H

/* Manage out-of-band mode (master only) */
struct spi_ioc_oob_setup {
	/* Input */
	__u32 frame_len;
	__u32 speed_hz;
	__u8 bits_per_word;
	/* Output */
	__u32 iobuf_len;
	__u32 tx_offset;
	__u32 rx_offset;
};

#define SPI_IOC_ENABLE_OOB_MODE		_IOWR(SPI_IOC_MAGIC, 50, struct spi_ioc_oob_setup)
#define SPI_IOC_DISABLE_OOB_MODE	_IO(SPI_IOC_MAGIC, 51)
#define SPI_IOC_RUN_OOB_XFER		_IO(SPI_IOC_MAGIC, 52)

#endif /* _EVL_UAPI_DEVICES_SPIDEV_H */
