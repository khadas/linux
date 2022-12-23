/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __TB_TASK_H__
#define __TB_TASK_H__

int tbtask_start(void);
void tbtask_stop(void);
bool tbtsk_alloc_block(unsigned int ch, struct tbtsk_cmd_s *cmd);
void tbtask_send_ready(unsigned int id);
void tb_polling(unsigned int ch, struct tbtsk_cmd_s *cmd);

#endif /*__TB_TASK_H__*/
