// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/dma-fence.h>
#include <linux/uaccess.h>
#include <linux/sync_file.h>
#include <linux/file.h>
#include <linux/types.h>

#include "meson_async_atomic.h"
#include "meson_plane.h"
#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_mode.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_property.h>

#include "meson_drv.h"
#include "meson_async_atomic.h"
#include "meson_plane.h"

struct drm_property *meson_mode_obj_find_prop_id(struct drm_mode_object *obj,
					       uint32_t prop_id)
{
	int i;

	for (i = 0; i < obj->properties->count; i++)
		if (obj->properties->properties[i]->base.id == prop_id)
			return obj->properties->properties[i];

	return NULL;
}

static int
meson_atomic_replace_property_blob_from_id(struct drm_device *dev,
					 struct drm_property_blob **blob,
					 u64 blob_id,
					 ssize_t expected_size,
					 ssize_t expected_elem_size,
					 bool *replaced)
{
	struct drm_property_blob *new_blob = NULL;

	if (blob_id != 0) {
		new_blob = drm_property_lookup_blob(dev, blob_id);
		if (!new_blob)
			return -EINVAL;

		if (expected_size > 0 &&
		    new_blob->length != expected_size) {
			drm_property_blob_put(new_blob);
			return -EINVAL;
		}
		if (expected_elem_size > 0 &&
		    new_blob->length % expected_elem_size != 0) {
			drm_property_blob_put(new_blob);
			return -EINVAL;
		}
	}

	*replaced |= drm_property_replace_blob(blob, new_blob);
	drm_property_blob_put(new_blob);

	return 0;
}

static int meson_atomic_plane_set_property(struct drm_plane *plane,
		struct drm_plane_state *state, struct drm_file *file_priv,
		struct drm_property *property, u64 val)
{
	struct drm_device *dev = plane->dev;
	struct drm_mode_config *config = &dev->mode_config;
	bool replaced = false;
	int ret;

	if (property == config->prop_fb_id) {
		struct drm_framebuffer *fb;

		fb = drm_framebuffer_lookup(dev, file_priv, val);
		drm_atomic_set_fb_for_plane(state, fb);
		if (fb)
			drm_framebuffer_put(fb);
	} else if (property == config->prop_in_fence_fd) {
		if (state->fence)
			return -EINVAL;

		if (U642I64(val) == -1)
			return 0;

		state->fence = sync_file_get_fence(val);
		if (!state->fence)
			return -EINVAL;

	} else if (property == config->prop_crtc_id) {
		struct drm_crtc *crtc = drm_crtc_find(dev, file_priv, val);

		if (val && !crtc)
			return -EACCES;
		return drm_atomic_set_crtc_for_plane(state, crtc);
	} else if (property == config->prop_crtc_x) {
		state->crtc_x = U642I64(val);
	} else if (property == config->prop_crtc_y) {
		state->crtc_y = U642I64(val);
	} else if (property == config->prop_crtc_w) {
		state->crtc_w = val;
	} else if (property == config->prop_crtc_h) {
		state->crtc_h = val;
	} else if (property == config->prop_src_x) {
		state->src_x = val;
	} else if (property == config->prop_src_y) {
		state->src_y = val;
	} else if (property == config->prop_src_w) {
		state->src_w = val;
	} else if (property == config->prop_src_h) {
		state->src_h = val;
	} else if (property == plane->alpha_property) {
		state->alpha = val;
	} else if (property == plane->blend_mode_property) {
		state->pixel_blend_mode = val;
	} else if (property == plane->rotation_property) {
		if (!is_power_of_2(val & DRM_MODE_ROTATE_MASK)) {
			DRM_DEBUG_ATOMIC("[PLANE:%d:%s] bad rotation bitmask: 0x%llx\n",
					 plane->base.id, plane->name, val);
			return -EINVAL;
		}
		state->rotation = val;
	} else if (property == plane->zpos_property) {
		state->zpos = val;
	} else if (property == plane->color_encoding_property) {
		state->color_encoding = val;
	} else if (property == plane->color_range_property) {
		state->color_range = val;
	} else if (property == config->prop_fb_damage_clips) {
		ret = meson_atomic_replace_property_blob_from_id(dev,
					&state->fb_damage_clips,
					val,
					-1,
					sizeof(struct drm_rect),
					&replaced);
		return ret;
	} else if (property == plane->scaling_filter_property) {
		state->scaling_filter = val;
	} else if (plane->funcs->atomic_set_property) {
		return plane->funcs->atomic_set_property(plane, state,
				property, val);
	} else {
		DRM_DEBUG_ATOMIC("[PLANE:%d:%s] unknown property [PROP:%d:%s]]\n",
				 plane->base.id, plane->name,
				 property->base.id, property->name);
		return -EINVAL;
	}

	return 0;
}

static bool meson_property_change_valid_get(struct drm_property *property,
				   u64 value, struct drm_mode_object **ref)
{
	int i;

	if (property->flags & DRM_MODE_PROP_IMMUTABLE)
		return false;

	*ref = NULL;

	if (drm_property_type_is(property, DRM_MODE_PROP_RANGE)) {
		if (value < property->values[0] || value > property->values[1])
			return false;
		return true;
	} else if (drm_property_type_is(property, DRM_MODE_PROP_SIGNED_RANGE)) {
		s64 svalue = U642I64(value);

		if (svalue < U642I64(property->values[0]) ||
				svalue > U642I64(property->values[1]))
			return false;
		return true;
	} else if (drm_property_type_is(property, DRM_MODE_PROP_BITMASK)) {
		u64 valid_mask = 0;

		for (i = 0; i < property->num_values; i++)
			valid_mask |= (1ULL << property->values[i]);
		return !(value & ~valid_mask);
	} else if (drm_property_type_is(property, DRM_MODE_PROP_BLOB)) {
		struct drm_property_blob *blob;

		if (value == 0)
			return true;

		blob = drm_property_lookup_blob(property->dev, value);
		if (blob) {
			*ref = &blob->base;
			return true;
		} else {
			return false;
		}
	} else if (drm_property_type_is(property, DRM_MODE_PROP_OBJECT)) {
		/* a zero value for an object property translates to null: */
		if (value == 0)
			return true;

		*ref = drm_mode_object_find(property->dev, NULL, value,
					      property->values[0]);
		if (*ref)
			return true;
		else
			return false;
	}

	for (i = 0; i < property->num_values; i++)
		if (property->values[i] == value)
			return true;
	return false;
}

static void meson_property_change_valid_put(struct drm_property *property,
		struct drm_mode_object *ref)
{
	if (!ref)
		return;

	if (drm_property_type_is(property, DRM_MODE_PROP_OBJECT))
		drm_mode_object_put(ref);
	else if (drm_property_type_is(property, DRM_MODE_PROP_BLOB))
		drm_property_blob_put(obj_to_blob(ref));
}

static int atomic_set_prop(struct drm_atomic_state *state,
		struct drm_mode_object *obj, struct drm_file *file_priv,
		struct drm_property *prop, u64 prop_value)
{
	struct drm_mode_object *ref;
	int ret;

	if (!meson_property_change_valid_get(prop, prop_value, &ref))
		return -EINVAL;

	switch (obj->type) {
	case DRM_MODE_OBJECT_PLANE: {
		struct drm_plane *plane = obj_to_plane(obj);
		struct drm_plane_state *plane_state;

		plane_state = drm_atomic_get_plane_state(state, plane);
		if (IS_ERR(plane_state)) {
			ret = PTR_ERR(plane_state);
			break;
		}

		ret = meson_atomic_plane_set_property(plane,
				plane_state, file_priv, prop, prop_value);
		break;
	}
	default:
		ret = -EINVAL;
		break;
	}

	meson_property_change_valid_put(prop, ref);
	return ret;
}

/*modified from drm_mode_atomic_ioctl() */
int meson_async_atomic_ioctl(struct drm_device *dev,
			  void *data, struct drm_file *file_priv)
{
	struct drm_mode_atomic *arg = data;
	u32 __user *objs_ptr = (u32 __user *)(unsigned long)(arg->objs_ptr);
	u32 __user *count_props_ptr = (u32 __user *)(unsigned long)(arg->count_props_ptr);
	u32 __user *props_ptr = (u32 __user *)(unsigned long)(arg->props_ptr);
	u64 __user *prop_values_ptr = (u64 __user *)(unsigned long)(arg->prop_values_ptr);
	unsigned int copied_objs, copied_props;
	struct drm_atomic_state *state;
	struct drm_modeset_acquire_ctx ctx;
	struct drm_out_fence_state *fence_state;
	int ret = 0;
	unsigned int i, j, num_fences;

	drm_modeset_acquire_init(&ctx, 0);

	state = drm_atomic_state_alloc(dev);
	if (!state)
		return -ENOMEM;

	state->allow_modeset = false;
	state->acquire_ctx = &ctx;

retry:
	copied_objs = 0;
	copied_props = 0;
	fence_state = NULL;
	num_fences = 0;

	if (arg->count_objs > 1) {
		DRM_ERROR("only accept plane object update(%d).\n", arg->count_objs);
		ret = -EINVAL;
		goto out;
	}

	for (i = 0; i < arg->count_objs; i++) {
		u32 obj_id, count_props;
		struct drm_mode_object *obj;

		if (get_user(obj_id, objs_ptr + copied_objs)) {
			ret = -EFAULT;
			goto out;
		}

		obj = drm_mode_object_find(dev, file_priv, obj_id, DRM_MODE_OBJECT_ANY);
		if (!obj) {
			ret = -ENOENT;
			goto out;
		}

		if (obj->type != DRM_MODE_OBJECT_PLANE) {
			DRM_ERROR("only accept plane object update, canot set (%d)\n", obj->type);

			ret = -EINVAL;
			goto out;
		}

		if (!obj->properties) {
			drm_mode_object_put(obj);
			ret = -ENOENT;
			goto out;
		}

		if (get_user(count_props, count_props_ptr + copied_objs)) {
			drm_mode_object_put(obj);
			ret = -EFAULT;
			goto out;
		}

		copied_objs++;

		for (j = 0; j < count_props; j++) {
			u32 prop_id;
			u64 prop_value;
			struct drm_property *prop;

			if (get_user(prop_id, props_ptr + copied_props)) {
				drm_mode_object_put(obj);
				ret = -EFAULT;
				goto out;
			}

			prop = meson_mode_obj_find_prop_id(obj, prop_id);
			if (!prop) {
				drm_mode_object_put(obj);
				ret = -ENOENT;
				goto out;
			}

			if (copy_from_user(&prop_value,
					   prop_values_ptr + copied_props,
					   sizeof(prop_value))) {
				drm_mode_object_put(obj);
				ret = -EFAULT;
				goto out;
			}

			ret = atomic_set_prop(state, obj, file_priv, prop, prop_value);
			if (ret) {
				drm_mode_object_put(obj);
				goto out;
			}

			copied_props++;
		}

		drm_mode_object_put(obj);
	}

	state->legacy_cursor_update = true;
	ret = drm_atomic_commit(state);
if (ret)
	DRM_ERROR("%s failed .\n", __func__);
else
	DRM_DEBUG("%s OUT\n", __func__);

out:
	if (ret == -EDEADLK) {
		drm_atomic_state_clear(state);
		drm_modeset_backoff(&ctx);
		goto retry;
	}

	drm_atomic_state_put(state);

	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);
	return ret;
}
