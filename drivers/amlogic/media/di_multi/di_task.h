/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DI_TASK_H__
#define __DI_TASK_H__

extern unsigned int di_dbg_task_flg;	/*debug only*/

enum ETSK_STATE {
	ETSK_STATE_IDLE,
	ETSK_STATE_WORKING,
};

void task_stop(void);
int task_start(void);

void dbg_task(void);

bool task_send_cmd(unsigned int cmd);
void task_send_ready(void);

#endif /*__DI_TASK_H__*/
