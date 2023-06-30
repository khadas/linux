// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/ktime.h>
#include "meson_vpu_util.h"
#include "meson_vpu_reg.h"
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
#include <linux/amlogic/media/rdma/rdma_mgr.h>
#include "../media/common/rdma/rdma.h"
#endif
#include "vpu-hw/meson_osd_afbc.h"
#include "meson_vpu_pipeline.h"
#include "meson_print.h"

/*****drm reg access by rdma*****/
/*one item need two u32 = 8byte,total 512 item is enough for vpu*/
#define MESON_VPU_RDMA_TABLE_SIZE (512 * 8)

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
#define RDMA_COUNT 3
static int drm_vsync_rdma_handle[RDMA_COUNT] = {-1, -1, -1};

static void meson_vpu_vsync_rdma_irq(void *arg)
{
	//u32 *vpp_index = (u32 *)arg;
	u32 vpp_index = 0;

	if (drm_vsync_rdma_handle[vpp_index] == -1)
		return;

	vpu_pipeline_check_finish_reg(vpp_index);
}

static void meson_vpu1_vsync_rdma_irq(void *arg)
{
	//u32 *vpp_index = (u32 *)arg;
	u32 vpp_index = 1;

	if (drm_vsync_rdma_handle[vpp_index] == -1)
		return;

	vpu_pipeline_check_finish_reg(vpp_index);
}

static void meson_vpu2_vsync_rdma_irq(void *arg)
{
	//u32 *vpp_index = (u32 *)arg;
	u32 vpp_index = 2;

	if (drm_vsync_rdma_handle[vpp_index] == -1)
		return;

	vpu_pipeline_check_finish_reg(vpp_index);
}

static struct rdma_op_s meson_vpu_vsync_rdma_op = {
	meson_vpu_vsync_rdma_irq,
};

static struct rdma_op_s meson_vpu1_vsync_rdma_op = {
	meson_vpu1_vsync_rdma_irq,
};

static struct rdma_op_s meson_vpu2_vsync_rdma_op = {
	meson_vpu2_vsync_rdma_irq,
	NULL
};

void meson_vpu_reg_handle_register(u32 vpp_index)
{
	if (drm_vsync_rdma_handle[vpp_index] == -1) {
		if (vpp_index == 0) {
			drm_vsync_rdma_handle[vpp_index] = rdma_register(&meson_vpu_vsync_rdma_op,
				(void *)(uintptr_t)vpp_index, MESON_VPU_RDMA_TABLE_SIZE);
		}

		if (vpp_index == 1) {
			drm_vsync_rdma_handle[vpp_index] = rdma_register(&meson_vpu1_vsync_rdma_op,
				(void *)(uintptr_t)vpp_index, MESON_VPU_RDMA_TABLE_SIZE);
		}
		if (vpp_index == 2) {
			drm_vsync_rdma_handle[vpp_index] = rdma_register(&meson_vpu2_vsync_rdma_op,
				(void *)(uintptr_t)vpp_index, MESON_VPU_RDMA_TABLE_SIZE);
		}
	}

	DRM_INFO("%s, vpp%d-%d\n", __func__, vpp_index, drm_vsync_rdma_handle[vpp_index]);
}

/*suggestion: call this after atomic done*/
int meson_vpu_reg_vsync_config(u32 vpp_index)
{
	if (drm_vsync_rdma_handle[vpp_index] != -1) {
		DRM_DEBUG("%s, %d\n", __func__, vpp_index);
		if (vpp_index == 0)
			return rdma_config(drm_vsync_rdma_handle[vpp_index],
					   RDMA_TRIGGER_VSYNC_INPUT);
		else if (vpp_index == 1)
			return rdma_config(drm_vsync_rdma_handle[vpp_index],
					   RDMA_TRIGGER_VPP1_VSYNC_INPUT);
		else if (vpp_index == 2)
			return rdma_config(drm_vsync_rdma_handle[vpp_index],
					   RDMA_TRIGGER_VPP2_VSYNC_INPUT);
		else
			return -1;
	} else {
		DRM_ERROR("Error: RDMA is not registered!");
		return -1;
	}
}
#endif

u32 meson_vpu_read_reg(u32 addr)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	MESON_DRM_REG("%s, 0x%x\n", __func__, addr);
	return rdma_read_reg(drm_vsync_rdma_handle[0], addr);
#else
	return aml_read_vcbus(addr);
#endif
}

int meson_vpu_write_reg(u32 addr, u32 val)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	MESON_DRM_REG("%s, 0x%x, 0x%x\n", __func__, addr, val);
	return rdma_write_reg(drm_vsync_rdma_handle[0], addr, val);
#else
	aml_write_vcbus(addr, val);
	return 0;
#endif
}

int meson_vpu_write_reg_bits(u32 addr, u32 val, u32 start, u32 len)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	MESON_DRM_REG("%s, 0x%x, 0x%x, %d, %d\n", __func__, addr, val, start, len);
	return rdma_write_reg_bits(drm_vsync_rdma_handle[0], addr, val, start, len);
#else
	aml_vcbus_update_bits(addr, ((1 << len) - 1) << start, val << start);
	return 0;
#endif
}

static u32 meson_vpu1_read_reg(u32 addr)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	MESON_DRM_REG("%s, 0x%x\n", __func__, addr);
	return rdma_read_reg(drm_vsync_rdma_handle[1], addr);
#else
	return aml_read_vcbus(addr);
#endif
}

static int meson_vpu1_write_reg(u32 addr, u32 val)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	MESON_DRM_REG("%s, 0x%x, 0x%x\n", __func__, addr, val);
	return rdma_write_reg(drm_vsync_rdma_handle[1], addr, val);
#else
	aml_write_vcbus(addr, val);
	return 0;
#endif
}

static int meson_vpu1_write_reg_bits(u32 addr, u32 val, u32 start, u32 len)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	MESON_DRM_REG("%s, 0x%x, 0x%x, %d, %d\n", __func__, addr, val, start, len);
	return rdma_write_reg_bits(drm_vsync_rdma_handle[1], addr, val, start, len);
#else
	aml_vcbus_update_bits(addr, ((1 << len) - 1) << start, val << start);
	return 0;
#endif
}

static u32 meson_vpu2_read_reg(u32 addr)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	return rdma_read_reg(drm_vsync_rdma_handle[2], addr);
#else
	return aml_read_vcbus(addr);
#endif
}

static int meson_vpu2_write_reg(u32 addr, u32 val)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	MESON_DRM_REG("%s, 0x%x, 0x%x\n", __func__, addr, val);
	return rdma_write_reg(drm_vsync_rdma_handle[2], addr, val);
#else
	aml_write_vcbus(addr, val);
	return 0;
#endif
}

static int meson_vpu2_write_reg_bits(u32 addr, u32 val, u32 start, u32 len)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	MESON_DRM_REG("%s, 0x%x, 0x%x, %d, %d\n", __func__, addr, val, start, len);
	return rdma_write_reg_bits(drm_vsync_rdma_handle[2], addr, val, start, len);
#else
	aml_vcbus_update_bits(addr, ((1 << len) - 1) << start, val << start);
	return 0;
#endif
}

static struct rdma_reg_ops t7_reg_ops[3] = {
	{
		.rdma_read_reg = meson_vpu_read_reg,
		.rdma_write_reg = meson_vpu_write_reg,
		.rdma_write_reg_bits = meson_vpu_write_reg_bits,
	},
	{
		.rdma_read_reg = meson_vpu1_read_reg,
		.rdma_write_reg = meson_vpu1_write_reg,
		.rdma_write_reg_bits = meson_vpu1_write_reg_bits,

	},
	{
		.rdma_read_reg = meson_vpu2_read_reg,
		.rdma_write_reg = meson_vpu2_write_reg,
		.rdma_write_reg_bits = meson_vpu2_write_reg_bits,
	},
};

void meson_rdma_ops_init(struct meson_vpu_pipeline *pipeline, int num_crtc)
{
	int i;

	for (i = 0; i < num_crtc; i++)
		pipeline->subs[i].reg_ops = &t7_reg_ops[i];
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

