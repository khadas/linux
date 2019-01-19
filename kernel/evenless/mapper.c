/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <evenless/factory.h>
#include <uapi/evenless/mapper.h>

struct evl_mapper {
	struct file *mapfilp;
	struct evl_element element;
};

static int mapper_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct evl_mapper *mapper = element_of(filp, struct evl_mapper);
	struct file *mapfilp = mapper->mapfilp;
	int ret;

	if (mapfilp->f_op->mmap == NULL)
		return -ENODEV;

	vma->vm_file = get_file(mapfilp);

	ret = call_mmap(mapfilp, vma);
	if (ret)
		fput(mapfilp);
	else
		fput(filp);

	return ret;
}

static const struct file_operations mapper_fops = {
	.open		= evl_open_element,
	.release	= evl_close_element,
	.mmap		= mapper_mmap,
};

static struct evl_element *
mapper_factory_build(struct evl_factory *fac, const char *name,
		     void __user *u_attrs, u32 *state_offp)
{
	struct evl_mapper_attrs attrs;
	struct evl_mapper *mapper;
	struct file *mapfilp;
	int ret;

	ret = copy_from_user(&attrs, u_attrs, sizeof(attrs));
	if (ret)
		return ERR_PTR(-EFAULT);

	mapfilp = fget(attrs.fd);
	if (mapfilp == NULL)
		return ERR_PTR(-EINVAL);

	mapper = kzalloc(sizeof(*mapper), GFP_KERNEL);
	if (mapper == NULL) {
		ret = -ENOMEM;
		goto fail_mapper;
	}

	ret = evl_init_element(&mapper->element, &evl_mapper_factory);
	if (ret)
		goto fail_element;

	mapper->mapfilp = mapfilp;
	evl_index_element(&mapper->element);

	return &mapper->element;

fail_element:
	kfree(mapper);
fail_mapper:
	fput(mapfilp);

	return ERR_PTR(ret);
}

static void mapper_factory_dispose(struct evl_element *e)
{
	struct evl_mapper *mapper;

	mapper = container_of(e, struct evl_mapper, element);

	evl_unindex_element(&mapper->element);
	fput(mapper->mapfilp);
	evl_destroy_element(&mapper->element);

	kfree_rcu(mapper, element.rcu);
}

struct evl_factory evl_mapper_factory = {
	.name	=	"mapper",
	.fops	=	&mapper_fops,
	.build =	mapper_factory_build,
	.dispose =	mapper_factory_dispose,
	.nrdev	=	CONFIG_EVENLESS_NR_MAPPERS,
	.flags	=	EVL_FACTORY_CLONE,
};
