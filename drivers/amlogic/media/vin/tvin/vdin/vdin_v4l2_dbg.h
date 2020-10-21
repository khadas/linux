/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

extern unsigned int vdin_v4l_debug;

 /*external test function*/
void vdin_parse_param(char *buf_orig, char **parm);
void vdin_v4l2_create_device_files(struct device *dev);
void vdin_v4l2_remove_device_files(struct device *dev);

