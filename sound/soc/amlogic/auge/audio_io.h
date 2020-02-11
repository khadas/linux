/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_AUDIO_IO_H_
#define __AML_AUDIO_IO_H_

struct aml_audio_controller;

struct aml_audio_ctrl_ops {
	unsigned int (*read)(struct aml_audio_controller *actrlr,
			     unsigned int reg);
	int (*write)(struct aml_audio_controller *actrlr,
		     unsigned int reg, unsigned int value);
	int (*update_bits)(struct aml_audio_controller *actrlr,
			   unsigned int reg, unsigned int mask,
			   unsigned int value);
};

struct aml_audio_controller {
	struct regmap *regmap;
	const struct aml_audio_ctrl_ops *ops;
};

/* audio io controller */
int aml_init_audio_controller(struct aml_audio_controller *actrlr,
			      struct regmap *regmap,
			      struct aml_audio_ctrl_ops *ops);
int aml_audiobus_write(struct aml_audio_controller *actrlr,
		       unsigned int reg, unsigned int value);
unsigned int aml_audiobus_read(struct aml_audio_controller *actrlr,
			       unsigned int reg);
int aml_audiobus_update_bits(struct aml_audio_controller *actrlr,
			     unsigned int reg, unsigned int mask,
			     unsigned int value);
#endif
