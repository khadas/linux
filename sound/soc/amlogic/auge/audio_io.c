// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/module.h>
#include "audio_io.h"
#include "audio_top_reg_list.h"

int aml_audiobus_write(struct aml_audio_controller *actrlr,
		       unsigned int reg, unsigned int value)
{
	if (actrlr->ops->write) {
		pr_debug("audio top reg:[%s] addr: [%#x] val: [%#x]\n",
			top_register_table[reg].name,
			top_register_table[reg].addr,
			value);
		return actrlr->ops->write(actrlr->audioio_regmap, reg, value);
	}

	return -1;
}

unsigned int aml_audiobus_read(struct aml_audio_controller *actrlr,
			       unsigned int reg)
{
	if (actrlr->ops->read)
		return actrlr->ops->read(actrlr->audioio_regmap, reg);

	return 0;
}

int aml_audiobus_update_bits(struct aml_audio_controller *actrlr,
			     unsigned int reg, unsigned int mask,
			     unsigned int value)
{
	if (actrlr->ops->update_bits) {
		pr_debug("audio top reg:[%s] addr: [%#x] mask: [%#x] val: [%#x]\n",
			top_register_table[reg].name,
			top_register_table[reg].addr,
			mask, value);
		return actrlr->ops->update_bits(actrlr->audioio_regmap, reg, mask, value);
	}

	return -1;
}

int aml_acc_write(struct aml_audio_controller *actrlr,
		       unsigned int reg, unsigned int value)
{
	if (actrlr->ops->write && actrlr->acc_regmap) {
		pr_debug("audio acc reg addr: [%#x] val: [%#x]\n", reg, value);
		return actrlr->ops->write(actrlr->acc_regmap, reg, value);
	}

	return -1;
}

unsigned int aml_acc_read(struct aml_audio_controller *actrlr,
			       unsigned int reg)
{
	if (actrlr->ops->read && actrlr->acc_regmap)
		return actrlr->ops->read(actrlr->acc_regmap, reg);

	return 0;
}

int aml_acc_update_bits(struct aml_audio_controller *actrlr,
			     unsigned int reg, unsigned int mask,
			     unsigned int value)
{
	if (actrlr->ops->update_bits && actrlr->acc_regmap) {
		pr_debug("audio acc reg addr: [%#x] mask: [%#x] val: [%#x]\n", reg, mask, value);
		return actrlr->ops->update_bits(actrlr->acc_regmap, reg, mask, value);
	}

	return -1;
}

/* Module information */
MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("ALSA Soc Aml Audio Utils");
MODULE_LICENSE("GPL v2");
