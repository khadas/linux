/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __LINUX_AMLOGIC_ION_H__
#define __LINUX_AMLOGIC_ION_H__

#include <linux/types.h>
#include <linux/ion.h>

/**
 * CUSTOM IOCTL - CMD
 */

#define ION_IOC_MESON_PHYS_ADDR             8

struct meson_phys_data {
	int handle;
	unsigned int phys_addr;
	unsigned int size;
};

/**
 * meson_ion_share_fd_to_phys -
 * associate with a fd
 * @fd: passed from the user space
 * @addr point to the physical address
 * @size point to the size of this ion buffer
 */

int meson_ion_share_fd_to_phys(int fd, phys_addr_t *addr, size_t *len);
void meson_ion_buffer_to_phys(struct ion_buffer *buffer,
			      phys_addr_t *addr, size_t *len);
unsigned int meson_ion_cma_heap_id_get(void);

#endif
