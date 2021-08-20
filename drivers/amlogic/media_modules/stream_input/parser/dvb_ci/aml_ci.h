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
#ifndef __AML_CI_H_
#define __AML_CI_H_

//#include "drivers/media/dvb-core/dvb_ca_en50221.h"
#include "cimcu/dvb_ca_en50221_cimcu.h"
//#include "cimax/dvb_ca_en50221_cimax.h"
#include <linux/platform_device.h>

enum aml_dvb_io_type_e {
	AML_DVB_IO_TYPE_IOBUS = 0,
	AML_DVB_IO_TYPE_SPI,
	AML_DVB_IO_TYPE_CIMAX,
	AML_DVB_IO_TYPE_SPI_T312,
	AML_DVB_IO_TYPE_CIBUS,
	AML_DVB_IO_TYPE_MAX,
};

enum aml_gpio_level_e {
		AML_GPIO_LOW = 0,
		AML_GPIO_HIGH
};
enum AM_CI_CMD {
		AM_CI_CMD_IOR = 0,
		AM_CI_CMD_IOW,
		AM_CI_CMD_MEMR,
		AM_CI_CMD_MEMW,
		AM_CI_CMD_FULLTEST,
		AM_CI_CMD_CISTEST,
		AM_CI_CMD_GETCD12,
		AM_CI_CMD_POWER,
		AM_CI_CMD_RESET,
		AM_CI_CMD_CONGPIO,
};

struct aml_ci {
//	struct dvb_ca_en50221		en50221;
	struct dvb_ca_en50221_cimcu	en50221_cimcu;
	struct mutex			ci_lock;
	int				io_type;
	void				*priv;
	int				id;
	struct class			class;
	int				raw_mode;

	int (*ci_init)(struct platform_device *pdev, struct aml_ci *ci);
	int (*ci_exit)(struct aml_ci *ci);

	/* NOTE: the read_*, write_* and poll_slot_status functions will be
	 * called for different slots concurrently and need to use locks where
	 * and if appropriate. There will be no concurrent access to one slot.
	 */

	/* functions for accessing attribute memory on the CAM */
	int (*ci_mem_read)(struct aml_ci *ca, int slot, int address);
	int (*ci_mem_write)(struct aml_ci *ca, int slot, int address, u8 value);

	/* functions for accessing the control interface on the CAM */
	int (*ci_io_read)(struct aml_ci *ca, int slot, int address);
	int (*ci_io_write)(struct aml_ci *ca, int slot, int address, u8 value);

	/* Functions for controlling slots */
	int (*ci_slot_reset)(struct aml_ci *ca, int slot);
	int (*ci_slot_shutdown)(struct aml_ci *ca, int slot);
	int (*ci_slot_ts_enable)(struct aml_ci *ca, int slot);

	/*
	* Poll slot status.
	* Only necessary if DVB_CA_FLAG_EN50221_IRQ_CAMCHANGE is not set
	*/
	int (*ci_poll_slot_status)(struct aml_ci *ca, int slot, int open);

	int (*ci_get_slot_wakeup)(struct aml_ci *ca, int slot);

	//struct dvb_ca_en50221_cimax en50221_cimax;

	//int (*ci_read_cis)(struct aml_ci *ca, int slot, u8 *buf, int size);
	//int (*ci_write_cor)(struct aml_ci *ca, int slot, int address, u8 *buf);
      /*return the final size or -1 for error*/
	//int (*ci_negotiate)(struct aml_ci *ca, int slot, int size);

	/* functions for accessing the control interface on the CAM */
	//int (*ci_read_lpdu)(struct aml_ci *ca, int slot, u8 *buf, int size);
	//int (*ci_write_lpdu)(struct aml_ci *ca, int slot, u8 *buf, int size);

	//int (*ci_get_capbility)(struct aml_ci *ca, int slot);

	//int (*ci_cam_reset)(struct aml_ci *ca, int slot);
	//int (*ci_read_cam_status)(struct aml_ci *ca, int slot);

	/* private data, used by caller */
	void *data;
};

struct ci_dev_config_s {
	char name[20];
	unsigned char type;
	int cs_hold_delay;
	int cs_clk_delay;
};
extern int aml_ci_init(struct platform_device *pdev,
		struct dvb_adapter *dvb_adapter, struct aml_ci **cip);
extern void aml_ci_exit(struct aml_ci *ci);

#endif /* __AML_CI_H_ */

