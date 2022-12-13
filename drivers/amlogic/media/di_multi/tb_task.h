/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __TB_TASK_H__
#define __TB_TASK_H__

int tb_task_start(void);
void tb_task_stop(void);
bool tb_task_alloc_block(unsigned int ch, struct tb_task_cmd_s *cmd);
void tb_task_send_ready(unsigned int id);
void tb_polling(unsigned int ch, struct tb_task_cmd_s *cmd);

#endif /*__TB_TASK_H__*/
