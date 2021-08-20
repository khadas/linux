/***************************************************************************
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 *
***************************************************************************/

#ifndef __AML_CIMAX_H_
#define __AML_CIMAX_H_

#include "../aml_ci.h"
#include "dvb_ca_en50221_cimax.h"

struct aml_cimax;

struct aml_cimax_ops {
	int (*read_cis)(struct aml_cimax *cimax, int slot, u8 *buf, int size);
	int (*write_cor)(struct aml_cimax *cimax,
			int slot, int address, u8 *buf);
	int (*negotiate)(struct aml_cimax *cimax, int slot, int size);
	int (*read_lpdu)(struct aml_cimax *cimax, int slot, u8 *buf, int size);
	int (*write_lpdu)(struct aml_cimax *cimax, int slot, u8 *buf, int size);
	int (*read_cam_status)(struct aml_cimax *cimax, int slot);
	int (*cam_reset)(struct aml_cimax *cimax, int slot);
	int (*get_capblility)(struct aml_cimax *cimax, int slot);

	int (*slot_reset)(struct aml_cimax *cimax, int slot);
	int (*slot_shutdown)(struct aml_cimax *cimax, int slot);
	int (*slot_ts_enable)(struct aml_cimax *cimax, int slot);
	int (*slot_status)(struct aml_cimax *cimax, int slot);

	/*load fw etc.*/
	int (*start)(struct aml_cimax *cimax);
	int (*stop)(struct aml_cimax *cimax);

	/*cimax reg*/
	int (*read_reg)(struct aml_cimax *cimax, int addr, u8 *buf, int size);
	int (*write_reg)(struct aml_cimax *cimax, int addr, u8 *buf, int size);
};

struct aml_cimax {
	struct platform_device *pdev;
	struct device *dev;
	struct aml_ci *ci;

	int io_type;
#define IO_TYPE_SPI 0
#define IO_TYPE_USB 1
	struct aml_cimax_ops ops;
	void *priv;
};

int aml_cimax_init(struct platform_device *pdev, struct aml_ci *ci_dev);
int aml_cimax_exit(struct aml_ci *ci_dev);

int aml_cimax_camchanged(struct aml_cimax *cimax, int slot, int plugin);
int aml_cimax_slot_state_changed(struct aml_cimax *cimax, int slot, int state);
#endif
