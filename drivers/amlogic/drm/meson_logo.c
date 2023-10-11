// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/dma-contiguous.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <uapi/linux/sched/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/notifier.h>
#include <linux/clk.h>

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_flip_work.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_rect.h>
#include <drm/drm_fb_helper.h>

#ifdef CONFIG_DRM_MESON_USE_ION
#include "meson_gem.h"
#include "meson_fb.h"
#endif
#include "meson_drv.h"
#include "meson_vpu.h"
#include "meson_vpu_pipeline.h"
#include "meson_crtc.h"
#include "meson_logo.h"
#include "meson_hdmi.h"
#include "meson_plane.h"

#include <linux/amlogic/media/osd/osd_logo.h>
#include <linux/amlogic/media/vout/vout_notify.h>

static char *strmode;
struct am_meson_logo logo;
static struct platform_device *gp_dev;
static unsigned long gem_mem_start, gem_mem_size;

#ifdef MODULE
MODULE_PARM_DESC(outputmode, "outputmode");
module_param_named(outputmode, logo.outputmode_t, charp, 0600);

#else
core_param(fb_width, logo.width, uint, 0644);
core_param(fb_height, logo.height, uint, 0644);
core_param(display_bpp, logo.bpp, uint, 0644);
core_param(outputmode, logo.outputmode_t, charp, 0644);
#endif

struct para_pair_s {
	char *name;
	int value;
};

void am_meson_free_logo_memory(void)
{
	phys_addr_t logo_addr = page_to_phys(logo.logo_page);

	if (logo.size > 0 && logo.alloc_flag) {
#ifdef CONFIG_CMA
		DRM_INFO("%s, free memory: addr:0x%pa,size:0x%x\n",
			 __func__, &logo_addr, logo.size);

		dma_release_from_contiguous(&gp_dev->dev,
					    logo.logo_page,
					    logo.size >> PAGE_SHIFT);
#endif
	}
	logo.alloc_flag = 0;
}

static int am_meson_logo_info_update(struct meson_drm *priv)
{
	logo.start = page_to_phys(logo.logo_page);
	logo.alloc_flag = 1;
	/*config 1080p logo as default*/
	if (!logo.width || !logo.height) {
		logo.width = 1920;
		logo.height = 1080;
	}
	if (!logo.bpp)
		logo.bpp = 16;
	if (!logo.outputmode_t) {
		strcpy(logo.outputmode, "1080p60hz");
	} else {
		strncpy(logo.outputmode, logo.outputmode_t, VMODE_NAME_LEN_MAX);
		logo.outputmode[VMODE_NAME_LEN_MAX - 1] = '\0';
	}
	priv->logo = &logo;

	return 0;
}

static int am_meson_logo_init_fb(struct drm_device *dev,
		struct drm_framebuffer *fb, int idx)
{
	struct am_meson_fb *meson_fb;
	struct am_meson_logo *slogo;
	struct meson_drm *priv = dev->dev_private;

	slogo = kzalloc(sizeof(*slogo), GFP_KERNEL);
	if (!slogo)
		return -EFAULT;

	memcpy(slogo, &logo, sizeof(struct am_meson_logo));
	if (idx == VPP0) {
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
		strcpy(slogo->outputmode, get_vout_mode_uboot());
#endif
	} else if (idx == VPP1) {
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
		strcpy(slogo->outputmode, get_vout2_mode_uboot());
#endif
	} else if (idx == VPP2) {
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
		strcpy(slogo->outputmode, get_vout3_mode_uboot());
#endif
	}

	if (!strcmp("null", slogo->outputmode) ||
		!strcmp("dummy_l", slogo->outputmode)) {
		DRM_INFO("NULL MODE or DUMMY MODE, nothing to do.");
		kfree(slogo);
		return -EINVAL;
	}

	slogo->logo_page = logo.logo_page;
	slogo->panel_index = priv->primary_plane_index[idx];
	slogo->vpp_index = idx;

	DRM_INFO("logo%d width=%d,height=%d,start_addr=0x%pa,size=%d\n",
		 idx, slogo->width, slogo->height, &slogo->start, slogo->size);
	DRM_INFO("bpp=%d,alloc_flag=%d, osd_reverse=%d\n",
		 slogo->bpp, slogo->alloc_flag, slogo->osd_reverse);
	DRM_INFO("outputmode=%s\n", slogo->outputmode);

	meson_fb = to_am_meson_fb(fb);
	meson_fb->logo = slogo;

	return 0;
}

/*copy from update_output_state,
 *TODO:sync with update_output_state
 */
static int am_meson_update_output_state(struct drm_atomic_state *state,
					struct drm_mode_set *set)
{
	struct drm_device *dev = set->crtc->dev;
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	struct drm_connector *connector;
	struct drm_connector_state *new_conn_state;
	struct am_hdmitx_connector_state *hdmitx_state;
	int ret, i;
	char hdmitx_attr[16];

	ret = drm_modeset_lock(&dev->mode_config.connection_mutex,
			       state->acquire_ctx);
	if (ret)
		return ret;

	/* First disable all connectors on the target crtc. */
	ret = drm_atomic_add_affected_connectors(state, set->crtc);
	if (ret)
		return ret;

	for_each_new_connector_in_state(state, connector, new_conn_state, i) {
		if (new_conn_state->crtc == set->crtc) {
			ret = drm_atomic_set_crtc_for_connector(new_conn_state,
								NULL);
			if (ret)
				return ret;

			/* Make sure legacy setCrtc always re-trains */
			new_conn_state->link_status = DRM_LINK_STATUS_GOOD;
		}
	}

	/* Then set all connectors from set->connectors on the target crtc */
	for (i = 0; i < set->num_connectors; i++) {
		new_conn_state =
			drm_atomic_get_connector_state(state,
						       set->connectors[i]);
		if (IS_ERR(new_conn_state))
			return PTR_ERR(new_conn_state);

		ret = drm_atomic_set_crtc_for_connector(new_conn_state,
							set->crtc);
		if (ret)
			return ret;

		if (new_conn_state->connector->connector_type == DRM_MODE_CONNECTOR_HDMIA) {
			/*read attr from hdmitx, its from uboot*/
			am_hdmi_info.hdmitx_dev->get_attr(hdmitx_attr);
			hdmitx_state = to_am_hdmitx_connector_state(new_conn_state);
			convert_attrstr(hdmitx_attr, &hdmitx_state->color_attr_para);
			hdmitx_state->pref_hdr_policy = am_hdmi_info.hdmitx_dev->get_hdr_priority();
			hdmitx_state->frac_rate_policy = am_hdmi_info.hdmitx_dev->get_frac();
		}
	}

	for_each_new_crtc_in_state(state, crtc, new_crtc_state, i) {
		/* Don't update ->enable for the CRTC in the set_config request,
		 * since a mismatch would indicate a bug in the upper layers.
		 * The actual modeset code later on will catch any
		 * inconsistencies here.
		 */
		if (crtc == set->crtc)
			continue;

		if (!new_crtc_state->connector_mask) {
			ret = drm_atomic_set_mode_prop_for_crtc(new_crtc_state,
								NULL);
			if (ret < 0)
				return ret;

			new_crtc_state->active = false;
		}
	}

	return 0;
}

static int _am_meson_occupy_plane_config(struct drm_atomic_state *state,
					struct drm_mode_set *set)
{
	struct drm_crtc *crtc = set->crtc;
	struct meson_drm *private = crtc->dev->dev_private;
	struct am_osd_plane *osd_plane;
	struct drm_plane_state *plane_state;
	int i, hdisplay, vdisplay, ret;

	for (i = 0; i < MESON_MAX_OSD; i++) {
		osd_plane = private->osd_planes[i];
		if (!osd_plane || osd_plane->osd_occupied)
			break;
	}

	if (!osd_plane || !osd_plane->osd_occupied)
		return 0;

	plane_state = drm_atomic_get_plane_state(state, &osd_plane->base);
	if (IS_ERR(plane_state))
		return PTR_ERR(plane_state);

	ret = drm_atomic_set_crtc_for_plane(plane_state, crtc);
	if (ret != 0)
		return ret;

	drm_mode_get_hv_timing(set->mode, &hdisplay, &vdisplay);
	drm_atomic_set_fb_for_plane(plane_state, set->fb);
	plane_state->crtc_x = 0;
	plane_state->crtc_y = 0;
	plane_state->crtc_w = hdisplay;
	plane_state->crtc_h = vdisplay;
	plane_state->src_x = 0;
	plane_state->src_y = 0;
	plane_state->src_w = 1280/*set->fb->width*/ << 16;
	plane_state->src_h = 720/*set->fb->height*/ << 16;
	plane_state->alpha = 1;
	plane_state->zpos = 128;

	return 0;
}

/*similar with __drm_atomic_helper_set_config,
 *TODO:sync with __drm_atomic_helper_set_config
 */
int __am_meson_drm_set_config(struct drm_mode_set *set,
			      struct drm_atomic_state *state, int idx)
{
	struct drm_crtc_state *crtc_state;
	struct drm_plane_state *plane_state;
	struct drm_crtc *crtc = set->crtc;
	struct meson_drm *private = crtc->dev->dev_private;
	struct am_meson_crtc_state *meson_crtc_state;
	struct am_osd_plane *osd_plane;
	struct am_meson_fb *meson_fb;
	int hdisplay, vdisplay, ret;
	unsigned int zpos = OSD_PLANE_BEGIN_ZORDER;

	crtc_state = drm_atomic_get_crtc_state(state, crtc);
	if (IS_ERR(crtc_state))
		return PTR_ERR(crtc_state);

	meson_fb = to_am_meson_fb(set->fb);
	meson_crtc_state = to_am_meson_crtc_state(crtc_state);
	if (meson_fb->logo->vpp_index == VPP0) {
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
		meson_crtc_state->uboot_mode_init = get_vout_mode_uboot_state();
#endif
	} else if (meson_fb->logo->vpp_index == VPP1) {
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
		meson_crtc_state->uboot_mode_init = get_vout2_mode_uboot_state();
#endif
	} else if (meson_fb->logo->vpp_index == VPP2) {
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
		meson_crtc_state->uboot_mode_init = get_vout3_mode_uboot_state();
#endif
	}
	DRM_INFO("uboot_mode_init=%d\n", meson_crtc_state->uboot_mode_init);

	osd_plane = private->osd_planes[idx];
	if (!osd_plane)
		return -EINVAL;

	plane_state = drm_atomic_get_plane_state(state, &osd_plane->base);
	if (IS_ERR(plane_state))
		return PTR_ERR(plane_state);

	if (!set->mode) {
		WARN_ON(set->fb);
		WARN_ON(set->num_connectors);

		ret = drm_atomic_set_mode_for_crtc(crtc_state, NULL);
		if (ret != 0)
			return ret;

		crtc_state->active = false;

		ret = drm_atomic_set_crtc_for_plane(plane_state, NULL);
		if (ret != 0)
			return ret;

		drm_atomic_set_fb_for_plane(plane_state, NULL);

		goto commit;
	}

	WARN_ON(!set->fb);
	WARN_ON(!set->num_connectors);

	ret = drm_atomic_set_mode_for_crtc(crtc_state, set->mode);
	if (ret != 0)
		return ret;

	crtc_state->active = true;

	ret = drm_atomic_set_crtc_for_plane(plane_state, crtc);
	if (ret != 0)
		return ret;

	drm_mode_get_hv_timing(set->mode, &hdisplay, &vdisplay);
	drm_atomic_set_fb_for_plane(plane_state, set->fb);
	plane_state->crtc_x = 0;
	plane_state->crtc_y = 0;
	plane_state->crtc_w = hdisplay;
	plane_state->crtc_h = vdisplay;
	plane_state->src_x = set->x << 16;
	plane_state->src_y = set->y << 16;
	plane_state->zpos = zpos + osd_plane->plane_index;

	switch (logo.osd_reverse) {
	case 1:
		plane_state->rotation = DRM_MODE_REFLECT_MASK;
		break;
	case 2:
		plane_state->rotation = DRM_MODE_REFLECT_X;
		break;
	case 3:
		plane_state->rotation = DRM_MODE_REFLECT_Y;
		break;
	default:
		plane_state->rotation = DRM_MODE_ROTATE_0;
		break;
	}

	if (drm_rotation_90_or_270(plane_state->rotation)) {
		if (private->ui_config.ui_h)
			plane_state->src_w = private->ui_config.ui_h << 16;
		else
			plane_state->src_w = set->fb->height << 16;
		if (private->ui_config.ui_w)
			plane_state->src_h = private->ui_config.ui_w << 16;
		else
			plane_state->src_h = set->fb->width << 16;
	} else {
		if (private->ui_config.ui_w)
			plane_state->src_w = private->ui_config.ui_w << 16;
		else
			plane_state->src_w = set->fb->width << 16;
		if (private->ui_config.ui_h)
			plane_state->src_h = private->ui_config.ui_h << 16;
		else
			plane_state->src_h = set->fb->height << 16;
	}

	if (meson_fb->logo->vpp_index == VPP0)
		_am_meson_occupy_plane_config(state, set);

commit:
	ret = am_meson_update_output_state(state, set);
	if (ret)
		return ret;

	return 0;
}

/*copy from drm_atomic_helper_set_config,
 *TODO:sync with drm_atomic_helper_set_config
 */
static int am_meson_drm_set_config(struct drm_mode_set *set,
				   struct drm_modeset_acquire_ctx *ctx, int idx)
{
	struct drm_atomic_state *state;
	struct drm_crtc *crtc = set->crtc;
	int ret = 0;

	state = drm_atomic_state_alloc(crtc->dev);
	if (!state)
		return -ENOMEM;

	state->acquire_ctx = ctx;
	ret = __am_meson_drm_set_config(set, state, idx);
	if (ret != 0)
		goto fail;

	ret = drm_atomic_commit(state);

fail:
	drm_atomic_state_put(state);
	return ret;
}

static void am_meson_load_logo(struct drm_device *dev,
	struct drm_framebuffer *fb, int idx)
{
	struct drm_mode_set set;
	struct drm_display_mode *mode;
	struct drm_connector **connector_set;
	struct drm_connector *connector;
	struct drm_modeset_acquire_ctx *ctx;
	struct meson_drm *private = dev->dev_private;
	struct am_meson_fb *meson_fb;
	u32 found, num_modes;

	DRM_DEBUG("%s idx[%d]\n", __func__, idx);

	if (!logo.alloc_flag) {
		DRM_INFO("%s: logo memory is not cma alloc\n", __func__);
		return;
	}

	if (am_meson_logo_init_fb(dev, fb, idx)) {
		DRM_INFO("vout%d logo is disabled!\n", idx + 1);
		return;
	}

	meson_fb = to_am_meson_fb(fb);
	/*init all connector and found matched uboot mode.*/
	found = 0;
	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		drm_modeset_lock_all(dev);
		if (drm_modeset_is_locked(&dev->mode_config.connection_mutex))
			drm_modeset_unlock(&dev->mode_config.connection_mutex);
		num_modes = connector->funcs->fill_modes(connector,
							 dev->mode_config.max_width,
							 dev->mode_config.max_height);
		drm_modeset_unlock_all(dev);

		if (num_modes) {
			list_for_each_entry(mode, &connector->modes, head) {
				if (!strcmp(mode->name, meson_fb->logo->outputmode)) {
					found = 1;
					break;
				}
			}
			if (found)
				break;
		}

		DRM_INFO("connector[%d] status[%d]\n",
			connector->connector_type, connector->status);
	}

	if (found) {
		DRM_INFO("Found connector[%d] mode[%s]\n",
			connector->connector_type, mode->name);
		if (!strcmp("null", mode->name)) {
			DRM_INFO("NULL MODE, nothing to do.");
			return;
		}
	} else {
		connector = NULL;
		mode = NULL;
		return;
	}

	connector_set = kmalloc_array(1, sizeof(struct drm_connector *),
				      GFP_KERNEL);
	if (!connector_set)
		return;

	DRM_INFO("mode private flag %x\n", mode->private_flags);

	connector_set[0] = connector;
	set.crtc = &private->crtcs[idx]->base;
	set.x = 0;
	set.y = 0;
	set.mode = mode;
	set.crtc->mode = *mode;
	set.connectors = connector_set;
	set.num_connectors = 1;
	set.fb = &meson_fb->base;

	drm_modeset_lock_all(dev);
	ctx = dev->mode_config.acquire_ctx;
	if (am_meson_drm_set_config(&set, ctx, meson_fb->logo->panel_index))
		DRM_INFO("[%s]am_meson_drm_set_config fail\n", __func__);
	drm_modeset_unlock_all(dev);

	kfree(connector_set);
}

void am_meson_logo_init(struct drm_device *dev)
{
	struct drm_mode_fb_cmd2 mode_cmd;
	struct drm_framebuffer *fb;
	struct meson_drm *private = dev->dev_private;
	struct platform_device *pdev = to_platform_device(private->dev);
#ifdef CONFIG_CMA
	struct cma *cma;
	struct reserved_mem *rmem = NULL;
	struct device_node *np, *mem_node;
#endif
	u32 reverse_type, osd_index;
	int i, ret;

	DRM_INFO("%s\n", __func__);

	gp_dev = pdev;
	/* init reserved memory */
	ret = of_reserved_mem_device_init(&gp_dev->dev);
	if (ret != 0) {
		DRM_ERROR("failed to init reserved memory\n");
	} else {
#ifdef CONFIG_CMA
		np = gp_dev->dev.of_node;
		mem_node = of_parse_phandle(np, "memory-region", 0);
		if (mem_node) {
			rmem = of_reserved_mem_lookup(mem_node);
			of_node_put(mem_node);
			if (rmem) {
				logo.size = rmem->size;
				DRM_INFO("of read reserve memsize=0x%x\n",
					 logo.size);
			}
		} else {
			DRM_ERROR("no memory-region\n");
		}

		cma = dev_get_cma_area(&gp_dev->dev);
		if (cma) {
			if (logo.size > 0) {
				logo.logo_page =
				dma_alloc_from_contiguous(&gp_dev->dev,
							  logo.size >>
							  PAGE_SHIFT,
							  0, 0);
				if (!logo.logo_page)
					DRM_ERROR("allocate buffer failed\n");
				else
					am_meson_logo_info_update(private);
			}
		} else {
			DRM_INFO("------ NO CMA\n");
		}
#endif
		if (gem_mem_start) {
			dma_declare_coherent_memory(dev->dev,
							gem_mem_start,
							gem_mem_start,
							gem_mem_size);
			DRM_INFO("meson drm mem_start = 0x%x, size = 0x%x\n",
				(u32)gem_mem_start, (u32)gem_mem_size);
		} else {
			DRM_INFO("------ NO reserved dma\n");
		}
	}

	get_logo_osd_reverse(&osd_index, &reverse_type);
	logo.osd_reverse = reverse_type;
	logo.width = get_logo_fb_width();
	logo.height = get_logo_fb_height();
	logo.bpp = get_logo_display_bpp();
	if (!logo.bpp)
		logo.bpp = 16;

	if (logo.bpp == 16)
		mode_cmd.pixel_format = DRM_FORMAT_RGB565;
	else if (logo.bpp == 24)
		mode_cmd.pixel_format = DRM_FORMAT_RGB888;
	else
		mode_cmd.pixel_format = DRM_FORMAT_XRGB8888;

	mode_cmd.offsets[0] = 0;
	mode_cmd.width = logo.width;
	mode_cmd.height = logo.height;
	mode_cmd.modifier[0] = DRM_FORMAT_MOD_LINEAR;
	/*ToDo*/
	mode_cmd.pitches[0] = ALIGN(mode_cmd.width * logo.bpp, 32) / 8;
	fb = am_meson_fb_alloc(dev, &mode_cmd, NULL);
	if (IS_ERR_OR_NULL(fb)) {
		DRM_ERROR("drm fb allocate failed\n");
		private->logo_show_done = true;
		return;
	}

	/*Todo: the condition may need change according to the boot args*/
	if (strmode && !strcmp("4", strmode))
		DRM_INFO("current is strmode\n");
	else
		for (i = 0; i < MESON_MAX_CRTC; i++)
			am_meson_load_logo(dev, fb, i);

	DRM_INFO("logo_drm_fb[id:%d,ref:%d]\n", fb->base.id,
		kref_read(&fb->base.refcount));
	drm_framebuffer_put(fb);

	private->logo_show_done = true;

	DRM_INFO("%s end\n", __func__);
}

static int gem_mem_device_init(struct reserved_mem *rmem, struct device *dev)
{
	s32 ret = 0;

	if (!rmem) {
		pr_info("Can't get reverse mem!\n");
		ret = -EFAULT;
		return ret;
	}
	gem_mem_start = rmem->base;
	gem_mem_size = rmem->size;
	DRM_INFO("init gem mem_source addr:0x%x size:0x%x\n",
		(u32)gem_mem_start, (u32)gem_mem_size);

	return 0;
}

static const struct reserved_mem_ops rmem_gem_ops = {
	.device_init = gem_mem_device_init,
};

static int __init gem_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_gem_ops;
	DRM_INFO("gem mem setup\n");
	return 0;
}

RESERVEDMEM_OF_DECLARE(gem, "amlogic, gem_memory", gem_mem_setup);

