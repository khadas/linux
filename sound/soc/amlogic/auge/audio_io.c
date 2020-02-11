// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/module.h>
#include "audio_io.h"

int aml_init_audio_controller(struct aml_audio_controller *actrlr,
			      struct regmap *regmap,
			      struct aml_audio_ctrl_ops *ops)
{
	actrlr->regmap = regmap;
	actrlr->ops = ops;

	return 0;
}

int aml_audiobus_write(struct aml_audio_controller *actrlr,
		       unsigned int reg, unsigned int value)
{
	if (actrlr->ops->write)
		return actrlr->ops->write(actrlr, reg, value);

	return -1;
}

unsigned int aml_audiobus_read(struct aml_audio_controller *actrlr,
			       unsigned int reg)
{
	if (actrlr->ops->read)
		return actrlr->ops->read(actrlr, reg);

	return 0;
}

int aml_audiobus_update_bits(struct aml_audio_controller *actrlr,
			     unsigned int reg, unsigned int mask,
			     unsigned int value)
{
	if (actrlr->ops->update_bits)
		return actrlr->ops->update_bits(actrlr, reg, mask, value);

	return -1;
}

/* Module information */
MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("ALSA Soc Aml Audio Utils");
MODULE_LICENSE("GPL v2");
