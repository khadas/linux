/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef VFM_EXT_H
#define VFM_EXT_H

int vfm_map_add(char *id, char *name_chain);
int vfm_map_remove(char *id);
bool vf_check_node(const char *name);

#endif /* VFM_EXT_H */
