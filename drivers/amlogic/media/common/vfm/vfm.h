/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_VFM_H
#define __AML_VFM_H

int vfm_map_add(char *id, char *name_chain);

int vfm_map_remove(char *id);

char *vf_get_provider_name(const char *receiver_name);

char *vf_get_receiver_name(const char *provider_name);

void vf_update_active_map(void);

int provider_list(char *buf);

int receiver_list(char *buf);

struct vframe_provider_s *vf_get_provider_by_name(const char *provider_name);

int dump_vfm_state(char *buf);

extern int vfm_mode;

extern int vfm_debug_flag;
extern int vfm_trace_enable;	/* 1; */
extern int vfm_trace_num;	/*  */

struct vfmctl {
	char name[10];
	char val[300];
};

#define VFM_IOC_MAGIC  'V'
#define VFM_IOCTL_CMD_ADD   _IOW(VFM_IOC_MAGIC, 0x00, struct vfmctl)
#define VFM_IOCTL_CMD_RM    _IOW(VFM_IOC_MAGIC, 0x01, struct vfmctl)
#define VFM_IOCTL_CMD_DUMP   _IOW(VFM_IOC_MAGIC, 0x02, struct vfmctl)
#define VFM_IOCTL_CMD_ADDDUMMY    _IOW(VFM_IOC_MAGIC, 0x03, struct vfmctl)
#define VFM_IOCTL_CMD_SET   _IOW(VFM_IOC_MAGIC, 0x04, struct vfmctl)
#define VFM_IOCTL_CMD_GET   _IOWR(VFM_IOC_MAGIC, 0x05, struct vfmctl)

#endif
