/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __SEC_MON_H__
#define __SEC_MON_H__

void __iomem *get_secmon_sharemem_input_base(void);
void __iomem *get_secmon_sharemem_output_base(void);
long get_secmon_phy_input_base(void);
long get_secmon_phy_output_base(void);

void secmon_clear_cma_mmu(void);

int within_secmon_region(unsigned long addr);
void meson_sm_mutex_lock(void);
void meson_sm_mutex_unlock(void);
void __iomem *get_meson_sm_input_base(void);
void __iomem *get_meson_sm_output_base(void);

#endif
