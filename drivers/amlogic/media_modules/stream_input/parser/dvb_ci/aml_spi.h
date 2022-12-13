/*
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#ifndef __AML_SPI_H_
#define __AML_SPI_H_

//#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/gpio/consumer.h>
#include "aml_pcmcia.h"
#include "aml_ci.h"
#include "dvb_ca_en50221.h"

/*
aml spi dev
*/
struct aml_spi {
	spinlock_t spi_lock;

	/* add SPI DEV */
	struct spi_board_info *spi_bdinfo;
	struct spi_device *spi;
	struct platform_device *pdev;
	struct device *dev;

	/* spi otherconfig */
	int cs_hold_delay;
	int cs_clk_delay;
	int write_check;

	/* add gpio pin */
	struct gpio_desc *reset_pin;
	int reset_pin_value;
	struct gpio_desc *cd_pin1;
	int cd_pin1_value;
	struct gpio_desc *cd_pin2;
	int cd_pin2_value;
	struct gpio_desc *pwr_pin;
	int pwr_pin_value;

	/* cam and mcu irq */
	struct gpio_desc *irq_cam_pin;
	int irq_cam_pin_value;
	int irq;
	struct aml_pcmcia pc;
	void *priv;
	/*for AML_DVB_IO_TYPE_SPI_T312 device*/
	struct gpio_desc *mcu_irq_pin;
	int mcu_irq_pin_value;
	/*device type*/
	int io_device_type;
};


/* used to mcu */
#define DATASTART 0xef
#define DATAEND   0xfe

enum AM_SPI_RECEIVER_STEP {
		AM_SPI_STEP_INIT = 0,
		AM_SPI_STEP_START1,
		AM_SPI_STEP_START2,
		AM_SPI_STEP_CMD,
		AM_SPI_STEP_DATA,
		AM_SPI_STEP_ADDR1,
		AM_SPI_STEP_ADDR2,
		AM_SPI_STEP_END1,
		AM_SPI_STEP_END2
};

enum AM_CON_GPIO
{
	AM_CONGPIO_SEL_LVDS = 0,
	AM_CONGPIO_SCN_EN,
	AM_CONGPIO_LD_EN2,
	AM_CONGPIO_2D3D,
	AM_CONGPIO_AMP_RST,
};

extern int dirspi_xfer(struct spi_device *spi, u8 *tx_buf, u8 *rx_buf,
		       int len);
extern int dirspi_write(struct spi_device *spi, u8 *buf, int len);
extern int dirspi_read(struct spi_device *spi, u8 *buf, int len);
extern void dirspi_start(struct spi_device *spi);
extern void dirspi_stop(struct spi_device *spi);
extern int dirspi_register_board_info(struct spi_board_info const *info, unsigned n);
extern void dvb_ca_en50221_camready_irq(struct dvb_ca_en50221 *pubca, int slot);
extern int aml_spi_init(struct platform_device *pdev, struct aml_ci *ci_dev);
extern int aml_spi_exit(struct aml_ci *ci_dev);
extern int aml_spi_mod_init(void);
extern void aml_spi_mod_exit(void);

#endif				/* __AML_SPI_H_ */
