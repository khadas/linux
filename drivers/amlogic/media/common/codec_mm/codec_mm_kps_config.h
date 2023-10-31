/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

[DBUF_TRACE_FUNC_0] = {
	.symbol_name = "__fd_install",
	.pre_handler = kp_fd_install_pre,
	.post_handler = kp_fd_install_post
},
[DBUF_TRACE_FUNC_1] = {
	.symbol_name = "do_dup2",
	.pre_handler = kp_do_dup2_pre,
	.post_handler = kp_do_dup2_post
},
[DBUF_TRACE_FUNC_2] = {
	.symbol_name = "dma_buf_file_release",
	.pre_handler = kp_dbuf_file_release_pre,
	.post_handler = kp_dbuf_file_release_post
},
[DBUF_TRACE_FUNC_3] = {
	.symbol_name = "put_unused_fd",
	.pre_handler = kp_put_unused_fd_pre,
	.post_handler = kp_put_unused_fd_post
}

