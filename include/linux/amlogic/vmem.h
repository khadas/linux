/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __LINUX_VMEM_H
#define __LINUX_VMEM_H

struct vmem_controller;
struct vmem_device;

struct vmem_controller {
	struct device dev;
	struct list_head list;
	struct list_head device_list;
	int bus_num;
	int (*setup)(struct vmem_device *vmemdev);
	int (*cleanup)(struct vmem_device *vmemdev);
};

struct vmem_device {
	struct device dev;
	struct vmem_controller *vmemctlr;
	struct list_head list;
	u8 dev_num;
	u8 *mem;
	int size;
	int (*cmd_notify)(u8 cmd_val, int offset, int size);
	int (*data_notify)(u8 cmd_val, int offset, int size);
};

struct vmem_controller *vmem_get_controller(int bus_num);
struct vmem_controller *
vmem_create_controller(struct device *dev, int size);
void vmem_destroy_controller(struct vmem_controller *vmemctlr);

struct vmem_device *
vmem_get_device(struct vmem_controller *vmemctlr, int dev_num);
struct vmem_device *
vmem_create_device(struct vmem_controller *vmemctlr, int dev_num, int mem_size);
void vmem_destroy_device(struct vmem_device *vmemdev);

int vmem_setup(struct vmem_device *vmemdev);
int vmem_cleanup(struct vmem_device *vmemdev);

#endif /* __LINUX_VMEM_H */
