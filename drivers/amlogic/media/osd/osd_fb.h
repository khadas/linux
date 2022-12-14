/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _OSD_FB_H_
#define _OSD_FB_H_

/* Linux Headers */
#include <linux/list.h>
#include <linux/fb.h>
#include <linux/types.h>

/* Local Headers */
#include "osd.h"

#define OSD_COUNT (HW_OSD_COUNT)
#define INVALID_BPP_ITEM {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
#define FBIO_WAITFORVSYNC          _IOW('F', 0x20, __u32)
#define FBIO_WAITFORVSYNC_64       _IOW('F', 0x21, __u32)

struct osd_fb_dev_s {
	struct mutex lock;/*osd dev mutex*/
	struct fb_info *fb_info;
	struct platform_device *dev;
	phys_addr_t fb_mem_paddr;
	void __iomem *fb_mem_vaddr;
	ulong fb_len;
	phys_addr_t fb_mem_afbc_paddr[OSD_MAX_BUF_NUM];
	void __iomem *fb_mem_afbc_vaddr[OSD_MAX_BUF_NUM];
	ulong fb_afbc_len[OSD_MAX_BUF_NUM];
	const struct color_bit_define_s *color;
	enum vmode_e vmode;
	struct osd_ctl_s osd_ctl;
	u32 order;
	u32 scale;
	u32 enable_3d;
	u32 preblend_enable;
	u32 enable_key_flag;
	u32 color_key;
	u32 fb_index;
	u32 open_count;
	bool dis_osd_mchange;
};

struct fb_dmabuf_export {
	__u32 buffer_idx;
	__u32 fd;
	__u32 flags;
};

#define OSD_INVALID_INFO 0xffffffff
#define OSD_FIRST_GROUP_START 1
#define OSD_SECOND_GROUP_START 4
#define OSD_END 7

void *aml_mm_vmap(phys_addr_t phys, unsigned long size);
void *aml_map_phyaddr_to_virt(dma_addr_t phys, unsigned long size);
void aml_unmap_phyaddr(u8 *vaddr);
phys_addr_t get_fb_rmem_paddr(int index);
void __iomem *get_fb_rmem_vaddr(int index);
size_t get_fb_rmem_size(int index);
int osd_blank(int blank_mode, struct fb_info *info);
const struct color_bit_define_s *
	_find_color_format(struct fb_var_screeninfo *var);
extern struct osd_fb_dev_s *gp_fbdev_list[];
extern const struct color_bit_define_s default_color_format_array[];
extern unsigned int osd_game_mode[];
extern unsigned int osd_pi_debug, osd_pi_enable;
extern unsigned int osd_slice2ppc_debug, osd_slice2ppc_enable;
#endif
