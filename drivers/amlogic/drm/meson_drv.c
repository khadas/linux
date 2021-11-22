// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/component.h>

#include <uapi/linux/sched/types.h>

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

#include "meson_fbdev.h"
#ifdef CONFIG_DRM_MESON_USE_ION
#include "meson_gem.h"
#include "meson_fb.h"
#endif
#include "meson_drv.h"
#include "meson_vpu.h"
#include "meson_vpu_pipeline.h"
#include "meson_crtc.h"
#include "meson_sysfs.h"
#include "meson_writeback.h"

#include <linux/amlogic/media/osd/osd_logo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include "meson_hdmi.h"

#define DRIVER_NAME "meson"
#define DRIVER_DESC "Amlogic Meson DRM driver"

#define MAX_CONNECTOR_NUM (3)

static void am_meson_fb_output_poll_changed(struct drm_device *dev)
{
#ifdef CONFIG_DRM_MESON_EMULATE_FBDEV
	int i;
	struct meson_drm_fbdev *fbdev;
	struct meson_drm *priv = dev->dev_private;

	for (i = 0; i < MESON_MAX_OSD; i++) {
		fbdev = priv->osd_fbdevs[i];
		if (fbdev)
			drm_fb_helper_hotplug_event(&fbdev->base);
	}
#endif
}

static const struct drm_mode_config_funcs meson_mode_config_funcs = {
	.output_poll_changed = am_meson_fb_output_poll_changed,
	.atomic_check        = drm_atomic_helper_check,
	.atomic_commit       = meson_atomic_commit,
#ifdef CONFIG_DRM_MESON_USE_ION
	.fb_create           = am_meson_fb_create,
#else
	.fb_create           = drm_gem_fb_create,
#endif
};

static const struct drm_mode_config_helper_funcs meson_mode_config_helpers = {
	.atomic_commit_tail = meson_atomic_helper_commit_tail,
};

bool meson_drm_get_scannout_position(struct drm_device *dev,
	unsigned int pipe, bool in_vblank_irq,
	int *vpos, int *hpos, ktime_t *stime, ktime_t *etime,
	const struct drm_display_mode *mode)
{
	int ret = 0;
	struct am_meson_crtc *crtc = to_am_meson_crtc
		(drm_crtc_from_index(dev, pipe));

	ret = crtc->get_scannout_position(crtc,
		in_vblank_irq, vpos, hpos, stime, etime, mode);

	return (ret == 0) ? true : false;
}

static char *strmode;
struct am_meson_logo logo;

#ifdef MODULE

MODULE_PARM_DESC(outputmode, "outputmode");
module_param_named(outputmode, logo.outputmode_t, charp, 0600);

#else
core_param(fb_width, logo.width, uint, 0644);
core_param(fb_height, logo.height, uint, 0644);
core_param(display_bpp, logo.bpp, uint, 0644);
core_param(outputmode, logo.outputmode_t, charp, 0644);
#endif

static struct drm_framebuffer *am_meson_logo_init_fb(struct drm_device *dev)
{
	struct drm_mode_fb_cmd2 mode_cmd;
	struct drm_framebuffer *fb;
	struct am_meson_fb *meson_fb;
	u32 reverse_type, osd_index;

	/*TODO: get mode from vout api temp.*/
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	strcpy(logo.outputmode, get_vout_mode_uboot());
#endif
	logo.width = get_logo_fb_width();
	logo.height = get_logo_fb_height();
	logo.bpp = get_logo_display_bpp();
	if (!logo.bpp)
		logo.bpp = 16;

	get_logo_osd_reverse(&osd_index, &reverse_type);
	logo.osd_reverse = reverse_type;

	DRM_INFO("width=%d,height=%d,start_addr=0x%pa,size=%d\n",
		 logo.width, logo.height, &logo.start, logo.size);
	DRM_INFO("bpp=%d,alloc_flag=%d, osd_reverse=%d\n",
		 logo.bpp, logo.alloc_flag, logo.osd_reverse);
	DRM_INFO("outputmode=%s\n", logo.outputmode);
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
	if (IS_ERR_OR_NULL(fb))
		return NULL;
	meson_fb = to_am_meson_fb(fb);
	meson_fb->logo = &logo;

	return fb;
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
	int ret, i;
	char hdmitx_attr[16];
	struct am_hdmitx_connector_state *hdmitx_state;

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

/*simaler with __drm_atomic_helper_set_config,
 *TODO:sync with __drm_atomic_helper_set_config
 */
int __am_meson_drm_set_config(struct drm_mode_set *set,
			      struct drm_atomic_state *state)
{
	struct drm_crtc_state *crtc_state;
	struct drm_plane_state *primary_state;
	struct drm_crtc *crtc = set->crtc;
	struct meson_drm *private = crtc->dev->dev_private;
	struct am_meson_crtc_state *meson_crtc_state;
	int hdisplay, vdisplay;
	int ret;

	crtc_state = drm_atomic_get_crtc_state(state, crtc);
	if (IS_ERR(crtc_state))
		return PTR_ERR(crtc_state);

	meson_crtc_state = to_am_meson_crtc_state(crtc_state);
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	meson_crtc_state->uboot_mode_init = get_vout_mode_uboot_state();
#endif

	primary_state = drm_atomic_get_plane_state(state, crtc->primary);
	if (IS_ERR(primary_state))
		return PTR_ERR(primary_state);

	if (!set->mode) {
		WARN_ON(set->fb);
		WARN_ON(set->num_connectors);

		ret = drm_atomic_set_mode_for_crtc(crtc_state, NULL);
		if (ret != 0)
			return ret;

		crtc_state->active = false;

		ret = drm_atomic_set_crtc_for_plane(primary_state, NULL);
		if (ret != 0)
			return ret;

		drm_atomic_set_fb_for_plane(primary_state, NULL);

		goto commit;
	}

	WARN_ON(!set->fb);
	WARN_ON(!set->num_connectors);

	ret = drm_atomic_set_mode_for_crtc(crtc_state, set->mode);
	if (ret != 0)
		return ret;

	crtc_state->active = true;

	ret = drm_atomic_set_crtc_for_plane(primary_state, crtc);
	if (ret != 0)
		return ret;

	drm_mode_get_hv_timing(set->mode, &hdisplay, &vdisplay);
	drm_atomic_set_fb_for_plane(primary_state, set->fb);
	primary_state->crtc_x = 0;
	primary_state->crtc_y = 0;
	primary_state->crtc_w = hdisplay;
	primary_state->crtc_h = vdisplay;
	primary_state->src_x = set->x << 16;
	primary_state->src_y = set->y << 16;
	if (logo.osd_reverse)
		primary_state->rotation = DRM_MODE_REFLECT_MASK;
	else
		primary_state->rotation = DRM_MODE_ROTATE_0;
	if (drm_rotation_90_or_270(primary_state->rotation)) {
		if (private->ui_config.ui_h)
			primary_state->src_w = private->ui_config.ui_h << 16;
		else
			primary_state->src_w = set->fb->height << 16;
		if (private->ui_config.ui_w)
			primary_state->src_h = private->ui_config.ui_w << 16;
		else
			primary_state->src_h = set->fb->width << 16;
	} else {
		if (private->ui_config.ui_w)
			primary_state->src_w = private->ui_config.ui_w << 16;
		else
			primary_state->src_w = set->fb->width << 16;
		if (private->ui_config.ui_h)
			primary_state->src_h = private->ui_config.ui_h << 16;
		else
			primary_state->src_h = set->fb->height << 16;
	}

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
				   struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_atomic_state *state;
	struct drm_crtc *crtc = set->crtc;
	int ret = 0;

	state = drm_atomic_state_alloc(crtc->dev);
	if (!state)
		return -ENOMEM;

	state->acquire_ctx = ctx;
	ret = __am_meson_drm_set_config(set, state);
	if (ret != 0)
		goto fail;

	ret = drm_atomic_commit(state);

fail:
	drm_atomic_state_put(state);
	return ret;
}

static void am_meson_load_logo(struct drm_device *dev)
{
	struct drm_mode_set set;
	struct drm_framebuffer *fb;
	struct drm_display_mode *mode;
	struct drm_connector **connector_set;
	struct drm_connector *connector;
	struct drm_modeset_acquire_ctx *ctx;
	struct meson_drm *private = dev->dev_private;
	u32 found, num_modes;
	char *vmode_name;

	if (!logo.alloc_flag) {
		DRM_INFO("%s: logo memory is not cma alloc\n", __func__);
		return;
	}
	fb = am_meson_logo_init_fb(dev);
	if (!fb) {
		DRM_INFO("%s:framebuffer is NULL!\n", __func__);
		return;
	}
	if (!strcmp("null", logo.outputmode)) {
		DRM_INFO("NULL MODE, nothing to do.");
		return;
	}

	if (!strcmp("dummy_l", logo.outputmode)) {
		DRM_INFO("Skip showing logo in dummy mode!");
		return;
	}

	/*init all connecotr and found matched uboot mode.*/
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
				vmode_name = am_meson_crtc_get_voutmode(mode);
				if (!strcmp(vmode_name, logo.outputmode)) {
					found = 1;
					break;
				}
			}
			if (found)
				break;
		}

		DRM_ERROR("Connecotr[%d] status[%d]\n",
			connector->connector_type, connector->status);
	}

	if (found) {
		DRM_ERROR("Found Connecotr[%d] mode[%s]\n",
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

	DRM_ERROR("mode private flag %x\n", mode->private_flags);

	connector_set[0] = connector;
	set.crtc = &private->crtcs[0]->base;
	set.x = 0;
	set.y = 0;
	set.mode = mode;
	set.crtc->mode = *mode;
	set.connectors = connector_set;
	set.num_connectors = 1;
	set.fb = fb;

	drm_modeset_lock_all(dev);
	ctx = dev->mode_config.acquire_ctx;
	if (am_meson_drm_set_config(&set, ctx))
		DRM_INFO("[%s]am_meson_drm_set_config fail\n", __func__);
	if (drm_framebuffer_read_refcount(fb) > 1)
		drm_framebuffer_put(fb);
	drm_modeset_unlock_all(dev);

	kfree(connector_set);
}

#ifdef CONFIG_DRM_MESON_USE_ION
static const struct drm_ioctl_desc meson_ioctls[] = {
	#ifdef CONFIG_DRM_MESON_USE_ION
	DRM_IOCTL_DEF_DRV(MESON_GEM_CREATE, am_meson_gem_create_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	#endif
};
#endif

static const struct file_operations fops = {
	.owner		= THIS_MODULE,
	.open		= drm_open,
	.release	= drm_release,
	.unlocked_ioctl	= drm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= drm_compat_ioctl,
#endif
	.poll		= drm_poll,
	.read		= drm_read,
	.llseek		= no_llseek,
#ifdef CONFIG_DRM_MESON_USE_ION
	.mmap		= am_meson_gem_mmap,
#else
	.mmap		= drm_gem_cma_mmap,
#endif
};

static struct drm_driver meson_driver = {
	/*driver_features setting move to probe functions*/
	.driver_features	= 0,
	.get_scanout_position = meson_drm_get_scannout_position,
	.get_vblank_timestamp = drm_calc_vbltimestamp_from_scanoutpos,
#ifdef CONFIG_DEBUG_FS
	.debugfs_init = meson_debugfs_init,
#endif
#ifdef CONFIG_DRM_MESON_USE_ION
	/* PRIME Ops */
	.prime_handle_to_fd	= drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle	= drm_gem_prime_fd_to_handle,

	.gem_prime_export	= am_meson_drm_gem_prime_export,
	.gem_prime_get_sg_table	= am_meson_gem_prime_get_sg_table,

	.gem_prime_import	= am_meson_drm_gem_prime_import,
	/*
	 * If gem_prime_import_sg_table is NULL,only buffer created
	 * by meson driver can be imported ok.
	 */
	.gem_prime_import_sg_table = am_meson_gem_prime_import_sg_table,

	.gem_prime_vmap		= am_meson_gem_prime_vmap,
	.gem_prime_vunmap	= am_meson_gem_prime_vunmap,
	.gem_prime_mmap		= am_meson_gem_prime_mmap,

	/* GEM Ops */
	.dumb_create			= am_meson_gem_dumb_create,
	.dumb_destroy		= am_meson_gem_dumb_destroy,
	.dumb_map_offset		= am_meson_gem_dumb_map_offset,
	.gem_free_object_unlocked	= am_meson_gem_object_free,
	.gem_vm_ops			= &drm_gem_cma_vm_ops,
	.ioctls			= meson_ioctls,
	.num_ioctls		= ARRAY_SIZE(meson_ioctls),
#else
	/* PRIME Ops */
	.prime_handle_to_fd	= drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle	= drm_gem_prime_fd_to_handle,
	.gem_prime_import	= drm_gem_prime_import,
	.gem_prime_export	= drm_gem_prime_export,
	.gem_prime_get_sg_table	= drm_gem_cma_prime_get_sg_table,
	.gem_prime_import_sg_table = drm_gem_cma_prime_import_sg_table,
	.gem_prime_vmap		= drm_gem_cma_prime_vmap,
	.gem_prime_vunmap	= drm_gem_cma_prime_vunmap,
	.gem_prime_mmap		= drm_gem_cma_prime_mmap,

	/* GEM Ops */
	.dumb_create		= drm_gem_cma_dumb_create,
	.dumb_destroy		= drm_gem_dumb_destroy,
	.dumb_map_offset	= drm_gem_dumb_map_offset,
	.gem_free_object_unlocked = drm_gem_cma_free_object,
	.gem_vm_ops		= &drm_gem_cma_vm_ops,
#endif

	/* Misc */
	.fops			= &fops,
	.name			= DRIVER_NAME,
	.desc			= DRIVER_DESC,
	.date			= "20180321",
	.major			= 1,
	.minor			= 0,
};

static int meson_worker_thread_init(struct meson_drm *priv)
{
	int ret;
	struct sched_param param;
	struct drm_device *drm = priv->drm;

	param.sched_priority = 16;

	kthread_init_worker(&priv->commit_thread[0].worker);
	priv->commit_thread[0].dev = drm;
	priv->commit_thread[0].thread = kthread_run(kthread_worker_fn,
						    &priv->commit_thread[0].worker,
						    "crtc_commit");
	if (IS_ERR(priv->commit_thread[0].thread)) {
		DRM_ERROR("failed to create commit thread\n");
		priv->commit_thread[0].thread = NULL;
		return -1;
	}

	ret = sched_setscheduler(priv->commit_thread[0].thread, SCHED_FIFO, &param);
	if (ret)
		DRM_ERROR("failed to set priority\n");

	return 0;
}

static int am_meson_drm_bind(struct device *dev)
{
	struct meson_drm *priv;
	struct drm_device *drm;
	struct platform_device *pdev = to_platform_device(dev);
	int ret = 0;

	meson_driver.driver_features = DRIVER_HAVE_IRQ | DRIVER_GEM |
		DRIVER_MODESET | DRIVER_ATOMIC | DRIVER_RENDER;

	drm = drm_dev_alloc(&meson_driver, dev);
	if (!drm)
		return -ENOMEM;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto err_free1;
	}
	drm->dev_private = priv;
	priv->drm = drm;
	priv->dev = dev;
	/*bound data to be used by component driver.*/
	priv->bound_data.drm = drm;
	priv->bound_data.connector_component_bind = meson_connector_dev_bind;
	priv->bound_data.connector_component_unbind = meson_connector_dev_unbind;

	dev_set_drvdata(dev, priv);

#ifdef CONFIG_DRM_MESON_USE_ION
	ret = am_meson_gem_create(priv);
	if (ret)
		goto err_free2;
#endif

	drm_mode_config_init(drm);

	vpu_topology_init(pdev, priv);
	meson_vpu_block_state_init(priv, priv->pipeline);

	/* init meson config before bind other component,
	 * other component may use it.
	 */
	drm->mode_config.max_width = 4096;
	drm->mode_config.max_height = 4096;
	drm->mode_config.funcs = &meson_mode_config_funcs;
	drm->mode_config.helper_private	= &meson_mode_config_helpers;
	drm->mode_config.allow_fb_modifiers = true;

	/* Try to bind all sub drivers. */
	ret = component_bind_all(dev, &priv->bound_data);
	if (ret)
		goto err_gem;

	/* Writeback should be registered after HDMI registration. */
	ret = am_meson_writeback_create(drm);
	if (ret)
		goto err_gem;
	DRM_INFO("mode_config crtc number:%d\n", drm->mode_config.num_crtc);

	ret = meson_worker_thread_init(priv);
	if (ret)
		goto err_unbind_all;

	ret = drm_vblank_init(drm, drm->mode_config.num_crtc);
	if (ret)
		goto err_unbind_all;

	drm_mode_config_reset(drm);
	/*
	 * enable drm irq mode.
	 * - with irq_enabled = true, we can use the vblank feature.
	 */
	drm->irq_enabled = true;

	drm_kms_helper_poll_init(drm);

	/*Todo: the condition may need change according to the boot args*/
	if (strmode && !strcmp("4", strmode))
		DRM_INFO("current is strmode\n");
	else
		am_meson_load_logo(drm);

#ifdef CONFIG_DRM_MESON_EMULATE_FBDEV
	ret = am_meson_drm_fbdev_init(drm);
	if (ret)
		goto err_poll_fini;
#endif
	ret = drm_dev_register(drm, 0);
	if (ret)
		goto err_fbdev_fini;
	ret = meson_drm_sysfs_register(drm);
	if (ret)
		goto err_drm_dev_unregister;

	return 0;

err_drm_dev_unregister:
	drm_dev_unregister(drm);

err_fbdev_fini:
#ifdef CONFIG_DRM_MESON_EMULATE_FBDEV
	am_meson_drm_fbdev_fini(drm);
err_poll_fini:
#endif
	drm_kms_helper_poll_fini(drm);
	drm->irq_enabled = false;
err_unbind_all:
	component_unbind_all(dev, drm);
err_gem:
	drm_mode_config_cleanup(drm);
#ifdef CONFIG_DRM_MESON_USE_ION
	am_meson_gem_cleanup(drm->dev_private);
err_free2:
#endif
	drm->dev_private = NULL;
	dev_set_drvdata(dev, NULL);
err_free1:
	drm_dev_put(drm);

	return ret;
}

static void am_meson_drm_unbind(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	meson_drm_sysfs_unregister(drm);
	drm_dev_unregister(drm);
#ifdef CONFIG_DRM_MESON_EMULATE_FBDEV
	am_meson_drm_fbdev_fini(drm);
#endif
	drm_kms_helper_poll_fini(drm);
	drm->irq_enabled = false;
	component_unbind_all(dev, drm);
	drm_mode_config_cleanup(drm);
#ifdef CONFIG_DRM_MESON_USE_ION
	am_meson_gem_cleanup(drm->dev_private);
#endif
	drm->dev_private = NULL;
	dev_set_drvdata(dev, NULL);
	drm_dev_put(drm);
}

static int compare_of(struct device *dev, void *data)
{
	struct device_node *np = data;

	return dev->of_node == np;
}

static void am_meson_add_endpoints(struct device *dev,
				   struct component_match **match,
				   struct device_node *port)
{
	struct device_node *ep, *remote;

	for_each_child_of_node(port, ep) {
		remote = of_graph_get_remote_port_parent(ep);
		if (!remote || !of_device_is_available(remote)) {
			of_node_put(remote);
			continue;
		} else if (!of_device_is_available(remote->parent)) {
			of_node_put(remote);
			continue;
		}
		component_match_add(dev, match, compare_of, remote);
		of_node_put(remote);
	}
}

static const struct component_master_ops am_meson_drm_ops = {
	.bind = am_meson_drm_bind,
	.unbind = am_meson_drm_unbind,
};

static bool am_meson_drv_use_osd(void)
{
	struct device_node *node;
	const  char *str;
	int ret;

	node = of_find_node_by_path("/meson-fb");
	if (node) {
		ret = of_property_read_string(node, "status", &str);
		if (ret) {
			DRM_INFO("get 'status' failed:%d\n", ret);
			return false;
		}

		if (strcmp(str, "okay") && strcmp(str, "ok")) {
			DRM_INFO("device %s status is %s\n",
				 node->name, str);
		} else {
			DRM_INFO("device %s status is %s\n",
				 node->name, str);
			return true;
		}
	}
	return false;
}

static int am_meson_drv_probe_prune(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct meson_drm *priv;
	struct drm_device *drm;
	int ret;

	/*driver_features reset to DRIVER_GEM | DRIVER_PRIME, for prune drm*/
	meson_driver.driver_features = DRIVER_GEM;

	drm = drm_dev_alloc(&meson_driver, dev);
	if (!drm)
		return -ENOMEM;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto err_free1;
	}
	drm->dev_private = priv;
	priv->drm = drm;
	priv->dev = dev;

	platform_set_drvdata(pdev, priv);

#ifdef CONFIG_DRM_MESON_USE_ION
	ret = am_meson_gem_create(priv);
	if (ret)
		goto err_free2;
#endif

	ret = drm_dev_register(drm, 0);
	if (ret)
		goto err_gem;

	return 0;

err_gem:
#ifdef CONFIG_DRM_MESON_USE_ION
	am_meson_gem_cleanup(drm->dev_private);
err_free2:
#endif
	drm->dev_private = NULL;
	platform_set_drvdata(pdev, NULL);
err_free1:
	drm_dev_put(drm);
	return ret;
}

static int am_meson_drv_remove_prune(struct platform_device *pdev)
{
	struct drm_device *drm = platform_get_drvdata(pdev);

	drm_dev_unregister(drm);
#ifdef CONFIG_DRM_MESON_USE_ION
	am_meson_gem_cleanup(drm->dev_private);
#endif
	drm->dev_private = NULL;
	platform_set_drvdata(pdev, NULL);
	drm_dev_put(drm);

	return 0;
}

static int am_meson_drv_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *port;
	struct component_match *match = NULL;
	int i;

	DRM_INFO("[%s] in\n", __func__);
	if (am_meson_drv_use_osd())
		return am_meson_drv_probe_prune(pdev);

	if (!np)
		return -ENODEV;

	/*
	 * Bind the crtc ports first, so that
	 * drm_of_find_possible_crtcs called from encoder .bind callbacks
	 * works as expected.
	 */
	for (i = 0;; i++) {
		port = of_parse_phandle(np, "ports", i);
		if (!port)
			break;

		if (!of_device_is_available(port->parent)) {
			of_node_put(port);
			continue;
		}

		component_match_add(dev, &match, compare_of, port->parent);
		of_node_put(port);
	}

	if (i == 0) {
		dev_err(dev, "missing 'ports' property.\n");
		return -ENODEV;
	}

	if (!match) {
		dev_err(dev, "No available vout found for display-subsystem.\n");
		return -ENODEV;
	}

	/*
	 * For each bound crtc, bind the encoders attached to its
	 * remote endpoint.
	 */
	for (i = 0;; i++) {
		port = of_parse_phandle(np, "ports", i);
		if (!port)
			break;

		if (!of_device_is_available(port->parent)) {
			of_node_put(port);
			continue;
		}

		am_meson_add_endpoints(dev, &match, port);
		of_node_put(port);
	}
	DRM_INFO("[%s] out\n", __func__);
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	disable_vout_mode_set_sysfs();
#endif
	return component_master_add_with_match(dev, &am_meson_drm_ops, match);
}

static int am_meson_drv_remove(struct platform_device *pdev)
{
	if (am_meson_drv_use_osd())
		return am_meson_drv_remove_prune(pdev);

	component_master_del(&pdev->dev, &am_meson_drm_ops);
	return 0;
}

static const struct of_device_id am_meson_drm_dt_match[] = {
	{ .compatible = "amlogic, drm-subsystem" },
	{}
};
MODULE_DEVICE_TABLE(of, am_meson_drm_dt_match);

#ifdef CONFIG_PM_SLEEP
static void am_meson_drm_fb_suspend(struct drm_device *drm)
{
#ifdef CONFIG_DRM_MESON_EMULATE_FBDEV
	int i;
	struct meson_drm_fbdev *fbdev;
	struct meson_drm *priv = drm->dev_private;

	for (i = 0; i < MESON_MAX_OSD; i++) {
		fbdev = priv->osd_fbdevs[i];
		if (fbdev)
			drm_fb_helper_set_suspend(&fbdev->base, 1);
	}
#endif
}

static void am_meson_drm_fb_resume(struct drm_device *drm)
{
#ifdef CONFIG_DRM_MESON_EMULATE_FBDEV
	int i;
	struct meson_drm_fbdev *fbdev;
	struct meson_drm *priv = drm->dev_private;

	for (i = 0; i < MESON_MAX_OSD; i++) {
		fbdev = priv->osd_fbdevs[i];
		if (fbdev)
			drm_fb_helper_set_suspend(&fbdev->base, 0);
	}
#endif
}

static int am_meson_drm_pm_suspend(struct device *dev)
{
	struct drm_device *drm;
	struct meson_drm *priv;

	priv = dev_get_drvdata(dev);
	if (!priv) {
		DRM_ERROR("%s: Failed to get meson drm!\n", __func__);
		return 0;
	}
	drm = priv->drm;
	if (!drm) {
		DRM_ERROR("%s: Failed to get drm device!\n", __func__);
		return 0;
	}
	drm_kms_helper_poll_disable(drm);
	am_meson_drm_fb_suspend(drm);
	priv->state = drm_atomic_helper_suspend(drm);
	if (IS_ERR(priv->state)) {
		am_meson_drm_fb_resume(drm);
		drm_kms_helper_poll_enable(drm);
		DRM_INFO("%s: drm_atomic_helper_suspend fail\n", __func__);
		return PTR_ERR(priv->state);
	}
	DRM_INFO("%s: done\n", __func__);
	return 0;
}

static int am_meson_drm_pm_resume(struct device *dev)
{
	struct drm_device *drm;
	struct meson_drm *priv;

	priv = dev_get_drvdata(dev);
	if (!priv) {
		DRM_ERROR("%s: Failed to get meson drm!\n", __func__);
		return 0;
	}
	drm = priv->drm;
	if (!drm) {
		DRM_ERROR("%s: Failed to get drm device!\n", __func__);
		return 0;
	}
	drm_atomic_helper_resume(drm, priv->state);
	am_meson_drm_fb_resume(drm);
	drm_kms_helper_poll_enable(drm);
	DRM_INFO("%s: done\n", __func__);
	return 0;
}
#endif

static const struct dev_pm_ops am_meson_drm_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(am_meson_drm_pm_suspend,
				am_meson_drm_pm_resume)
};

static struct platform_driver am_meson_drm_platform_driver = {
	.probe      = am_meson_drv_probe,
	.remove     = am_meson_drv_remove,
	.driver     = {
		.owner  = THIS_MODULE,
		.name   = DRIVER_NAME,
		.of_match_table = am_meson_drm_dt_match,
		.pm = &am_meson_drm_pm_ops,
	},
};

int __init am_meson_drm_init(void)
{
	return platform_driver_register(&am_meson_drm_platform_driver);
}

void __exit am_meson_drm_exit(void)
{
	platform_driver_unregister(&am_meson_drm_platform_driver);
}

#ifndef MODULE
module_init(am_meson_drm_init);
module_exit(am_meson_drm_exit);
#endif

MODULE_AUTHOR("Jasper St. Pierre <jstpierre@mecheye.net>");
MODULE_AUTHOR("Neil Armstrong <narmstrong@baylibre.com>");
MODULE_AUTHOR("MultiMedia Amlogic <multimedia-sh@amlogic.com>");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
