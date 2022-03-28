// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/ktime.h>
#include "meson_vpu_util.h"
#include "meson_vpu_reg.h"
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
#include <linux/amlogic/media/rdma/rdma_mgr.h>
#endif
#include "vpu-hw/meson_osd_afbc.h"
#include "meson_vpu_pipeline.h"

/*****drm reg access by rdma*****/
/*one item need two u32 = 8byte,total 512 item is enough for vpu*/
#define MESON_VPU_RDMA_TABLE_SIZE (512 * 8)

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
static int meson_vpu_reg_handle = -1;
static void meson_vpu_vsync_rdma_irq(void *arg)
{
	if (meson_vpu_reg_handle == -1)
		return;

	vpu_pipeline_check_finish_reg();
}

static struct rdma_op_s meson_vpu_vsync_rdma_op = {
	meson_vpu_vsync_rdma_irq,
	NULL
};

void meson_vpu_reg_handle_register(void)
{
	if (meson_vpu_reg_handle == -1)
		meson_vpu_reg_handle = rdma_register(&meson_vpu_vsync_rdma_op,
			NULL, MESON_VPU_RDMA_TABLE_SIZE);
}

/*suggestion: call this after atomic done*/
int meson_vpu_reg_vsync_config(void)
{
	return rdma_config(meson_vpu_reg_handle, RDMA_TRIGGER_VSYNC_INPUT);
}
#endif

u32 meson_vpu_read_reg(u32 addr)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	return rdma_read_reg(meson_vpu_reg_handle, addr);
#else
	return aml_read_vcbus(addr);
#endif
}

int meson_vpu_write_reg(u32 addr, u32 val)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	return rdma_write_reg(meson_vpu_reg_handle, addr, val);
#else
	aml_write_vcbus(addr, val);
	return 0;
#endif
}

int meson_vpu_write_reg_bits(u32 addr, u32 val, u32 start, u32 len)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	return rdma_write_reg_bits(meson_vpu_reg_handle,
				   addr, val, start, len);
#else
	aml_vcbus_update_bits(addr, ((1 << len) - 1) << start, val << start);
	return 0;
#endif
}

/** reg direct access without rdma **/
u32 meson_drm_read_reg(u32 addr)
{
	u32 val;

	val = aml_read_vcbus(addr);

	return val;
}

void meson_drm_write_reg(u32 addr, u32 val)
{
	aml_write_vcbus(addr, val);
}

void meson_drm_write_reg_bits(u32 addr, u32 val, u32 start, u32 len)
{
	aml_vcbus_update_bits(addr, ((1 << len) - 1) << start, val << start);
}

/** canvas config  **/

void meson_drm_canvas_config(u32 index, unsigned long addr, u32 width,
			     u32 height, u32 wrap, u32 blkmode)
{
	canvas_config(index, addr, width, height, wrap, blkmode);
}

int meson_drm_canvas_pool_alloc_table(const char *owner, u32 *table, int size,
				      enum canvas_map_type_e type)
{
	return canvas_pool_alloc_canvas_table(owner, table, size, type);
}

/*vpu mem power on/off*/
void meson_vpu_power_config(enum vpu_mod_e mode, bool en)
{
#ifdef CONFIG_AMLOGIC_VPU
	struct vpu_dev_s *vpu_dev;

	vpu_dev = vpu_dev_register(mode, "meson_drm");
	if (en)
		vpu_dev_mem_power_on(vpu_dev);
	else
		vpu_dev_mem_power_down(vpu_dev);
#endif
}

void osd_vpu_power_on(void)
{
#ifdef CONFIG_AMLOGIC_VPU
	struct vpu_dev_s *osd1_vpu_dev;
	struct vpu_dev_s *osd2_vpu_dev;
	struct vpu_dev_s *osd_scale_vpu_dev;

	osd1_vpu_dev = vpu_dev_register(VPU_VIU_OSD1, "OSD1");
	vpu_dev_mem_power_on(osd1_vpu_dev);
	osd2_vpu_dev = vpu_dev_register(VPU_VIU_OSD2, "OSD2");
	vpu_dev_mem_power_on(osd2_vpu_dev);
	osd_scale_vpu_dev =
		vpu_dev_register(VPU_VIU_OSD_SCALE, "OSD_SCALE");
	vpu_dev_mem_power_on(osd_scale_vpu_dev);
	if (1) {
		struct vpu_dev_s *osd2_scale_vpu_dev;
		struct vpu_dev_s *osd3_vpu_dev;
		struct vpu_dev_s *blend34_vpu_dev;

		osd2_scale_vpu_dev =
			vpu_dev_register(VPU_VD2_OSD2_SCALE, "OSD2_SCALE");
		vpu_dev_mem_power_on(osd2_scale_vpu_dev);
		osd3_vpu_dev = vpu_dev_register(VPU_VIU_OSD3, "OSD3");
		vpu_dev_mem_power_on(osd3_vpu_dev);
		blend34_vpu_dev =
			vpu_dev_register(VPU_OSD_BLD34, "BLEND34_SCALE");
		vpu_dev_mem_power_on(blend34_vpu_dev);
	}

	if (1) {
		struct vpu_dev_s *mali_afbc_vpu_dev;

		mali_afbc_vpu_dev =
			vpu_dev_register(VPU_MAIL_AFBCD, "MALI_AFBC");
		vpu_dev_mem_power_on(mali_afbc_vpu_dev);
	}
#endif
}

