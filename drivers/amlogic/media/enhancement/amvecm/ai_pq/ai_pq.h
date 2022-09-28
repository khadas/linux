/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/enhancement/amvecm/ai_pq/ai_pq.h
 *
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
 */

#ifndef AI_PQ_H
#define AI_PQ_H
/*adaptive dnlp parameters for ai pq*/
struct adap_dnlp_param_s {
	/*base value, from db*/
	int dnlp_final_gain;
	/*offset from customer*/
	int offset;
};

/*adaptive saturation parameters for ai pq*/
struct adap_satur_param_s {
	/*base value, from db*/
	int satur_hue_mab;
	/*offset from customer*/
	int offset;
};

/*adaptive peaking parameters for ai pq*/
struct adap_peaking_param_s {
	/*base value, from db*/
	int sr0_hp_final_gain;
	int sr0_bp_final_gain;
	int sr1_hp_final_gain;
	int sr1_bp_final_gain;
	/*offset from customer*/
	int offset;
};

/*adaptive parameters for ai pq*/
struct adap_param_setting_s {
	struct adap_dnlp_param_s dnlp_param;
	struct adap_satur_param_s satur_param;
	struct adap_peaking_param_s peaking_param;
};

extern struct adap_param_setting_s adaptive_param;

int aipq_saturation_hue_get_base_val(void);
int ai_detect_scene_init(void);
int adaptive_param_init(void);

/*temporary solution for base value*/
int aipq_base_dnlp_param(unsigned int final_gain);
int aipq_base_peaking_param(unsigned int reg,
			    unsigned int mask,
			    unsigned int value);
int aipq_base_satur_param(int value);
#endif
