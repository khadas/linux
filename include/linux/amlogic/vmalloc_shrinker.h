/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __VMALLOC_SHRINKER_H__
#define __VMALLOC_SHRINKER_H__

struct vmap_area *get_vm(unsigned long addr);
void put_vm(void);
int handle_vmalloc_fault(struct pt_regs *regs, unsigned long addr);
void release_vshrinker_page(unsigned long addr, unsigned long size);
#endif /* __VMALLOC_SHRINKER_H__ */
