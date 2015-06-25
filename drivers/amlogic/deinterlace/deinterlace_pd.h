#ifndef _DI_PD_H
#define _DI_PD_H
#include "deinterlace.h"

extern struct pd_detect_threshold_s field_pd_th;

void reset_pulldown_state(void);

void cal_pd_parameters(pulldown_detect_info_t *cur_info,
	pulldown_detect_info_t *pre_info, pulldown_detect_info_t *next_info);

void pattern_check_pre_2(int idx, pulldown_detect_info_t *cur_info,
	pulldown_detect_info_t *pre_info, pulldown_detect_info_t *pre2_info,
	int *pre_pulldown_mode, int *pre2_pulldown_mode, int *type,
	pd_detect_threshold_t *pd_th);

void init_pd_para(void);
/* new pd algorithm */
void reset_pd_his(void);
void insert_pd_his(pulldown_detect_info_t *pd_info);
void reset_pd32_status(void);
int detect_pd32(void);

#endif
