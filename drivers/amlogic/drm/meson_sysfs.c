// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "meson_sysfs.h"
#include "meson_crtc.h"

static const char vpu_group_name[] = "vpu";
EXPORT_SYMBOL_GPL(vpu_group_name);

static ssize_t vpu_blank_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	ssize_t len = 0;
	struct drm_minor *minor = dev_get_drvdata(dev);
	struct drm_crtc *crtc;
	struct am_meson_crtc *amc;

	if (!minor || !minor->dev)
		return -EINVAL;

	crtc = drm_crtc_from_index(minor->dev, 0);
	if (!crtc)
		return -EINVAL;

	amc = to_am_meson_crtc(crtc);

	len += scnprintf(&buf[len], PAGE_SIZE - len, "%s\n",
			"echo 1 > vpu_blank to blank the osd plane");
	len += scnprintf(&buf[len], PAGE_SIZE - len, "%s\n",
			"echo 0 > vpu_blank to unblank the osd plane");
	len += scnprintf(&buf[len], PAGE_SIZE - len,
			"blank_enable: %d\n", amc->blank_enable);

	return len;
}

static ssize_t vpu_blank_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t n)
{
	struct drm_minor *minor = dev_get_drvdata(dev);
	struct drm_crtc *crtc;
	struct am_meson_crtc *amc;

	if (!minor || !minor->dev)
		return -EINVAL;

	crtc = drm_crtc_from_index(minor->dev, 0);
	if (!crtc)
		return -EINVAL;

	amc = to_am_meson_crtc(crtc);

	if (sysfs_streq(buf, "1")) {
		amc->blank_enable = 1;
		DRM_INFO("enable the osd blank\n");
	} else if (sysfs_streq(buf, "0")) {
		amc->blank_enable = 0;
		DRM_INFO("disable the osd blank\n");
	} else {
		return -EINVAL;
	}

	return n;
}

static DEVICE_ATTR_RW(vpu_blank);

static struct attribute *vpu_attrs[] = {
	&dev_attr_vpu_blank.attr,
	NULL,
};

static const struct attribute_group vpu_attr_group = {
	.name	= vpu_group_name,
	.attrs	= vpu_attrs,
};

int meson_drm_sysfs_register(struct drm_device *drm_dev)
{
	int rc;
	struct device *dev = drm_dev->primary->kdev;

	rc = sysfs_create_group(&dev->kobj, &vpu_attr_group);

	return rc;
}

void meson_drm_sysfs_unregister(struct drm_device *drm_dev)
{
	struct device *dev = drm_dev->primary->kdev;

	sysfs_remove_group(&dev->kobj, &vpu_attr_group);
}

