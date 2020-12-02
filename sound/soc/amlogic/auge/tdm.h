/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_TDM_H__
#define __AML_TDM_H__

struct aml_tdm;

void aml_tdmin_set_src(struct aml_tdm *p_tdm);
int aml_tdm_hw_setting_init(struct aml_tdm *p_tdm,
			unsigned int rate,
			unsigned int channels,
			int stream);
void aml_tdm_hw_setting_free(struct aml_tdm *p_tdm, int stream);
void aml_tdm_trigger(struct aml_tdm *p_tdm, int stream, bool enable);
void tdm_mute_capture(struct aml_tdm *p_tdm, bool mute);

#endif
