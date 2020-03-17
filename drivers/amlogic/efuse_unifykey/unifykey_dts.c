// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/of.h>
#include "unifykey.h"

#undef pr_fmt
#define pr_fmt(fmt) "unifykey: " fmt

static int uk_item_parse_dt(struct aml_uk_dev *ukdev, struct device_node *node,
			    int id)
{
	int		  count;
	int		  ret;
	const char	  *propname;
	struct key_item_t *tmp_item = NULL;

	tmp_item = kzalloc(sizeof(*tmp_item), GFP_KERNEL);
	if (!tmp_item)
		return -ENOMEM;

	propname = NULL;
	ret = of_property_read_string(node, "key-name", &propname);
	if (ret < 0) {
		pr_err("%s:%d,get key-name fail\n", __func__, __LINE__);
		ret = -EINVAL;
		goto exit;
	}
	if (propname) {
		count = strlen(propname);
		if (count >= KEY_UNIFY_NAME_LEN)
			count = KEY_UNIFY_NAME_LEN - 1;
		memcpy(tmp_item->name, propname, count);
	}

	propname = NULL;
	ret = of_property_read_string(node, "key-device", &propname);
	if (ret < 0) {
		pr_err("%s:%d,get key-device fail\n", __func__, __LINE__);
		ret = -EINVAL;
		goto exit;
	}
	if (propname) {
		if (strcmp(propname, "efuse") == 0)
			tmp_item->dev = KEY_EFUSE;
		else if (strcmp(propname, "normal") == 0)
			tmp_item->dev = KEY_NORMAL;
		else if (strcmp(propname, "secure") == 0)
			tmp_item->dev = KEY_SECURE;
		else
			tmp_item->dev = KEY_UNKNOWN_DEV;
	}

	tmp_item->perm = 0;
	if (of_property_match_string(node, "key-permit", "read") >= 0)
		tmp_item->perm |= KEY_PERM_READ;
	if (of_property_match_string(node, "key-permit", "write") >= 0)
		tmp_item->perm |= KEY_PERM_WRITE;
	tmp_item->id = id;

	tmp_item->attr = 0;
	if (of_property_read_bool(node, "key-encrypt"))
		tmp_item->attr = KEY_ATTR_ENCRYPT;

	list_add(&tmp_item->node, &ukdev->uk_hdr);
	return 0;
exit:
	kfree(tmp_item);
	return ret;
}

static int uk_item_create(struct platform_device *pdev, int *num)
{
	int			ret =  -1;
	int			index;
	struct device_node	*child;
	struct device_node	*np = pdev->dev.of_node;
	struct aml_uk_dev *ukdev = platform_get_drvdata(pdev);

	of_node_get(np);
	index = 0;
	for_each_child_of_node(np, child) {
		ret = uk_item_parse_dt(ukdev, child, index);
		if (!ret)
			index++;
	}
	*num = index;
	pr_info("unifykey num is %d\n", *num);

	return 0;
}

int uk_dt_create(struct platform_device *pdev)
{
	int			key_num, ret;
	struct aml_uk_dev *ukdev = platform_get_drvdata(pdev);

	ukdev->uk_info.encrypt_type = -1;
	/* do not care whether unifykey-encrypt really exists*/
	ret = of_property_read_u32(pdev->dev.of_node, "unifykey-encrypt",
				   &ukdev->uk_info.encrypt_type);
	if (ret < 0) {
		pr_err("failed to get unifykey-encrypt\n");
		return -1;
	}

	if (!(ukdev->uk_info.key_flag)) {
		uk_item_create(pdev, &key_num);
		ukdev->uk_info.key_num = key_num;
		ukdev->uk_info.key_flag = 1;
	}

	return 0;
}

int uk_dt_release(struct platform_device *pdev)
{
	struct aml_uk_dev *ukdev = platform_get_drvdata(pdev);
	struct key_item_t *item;
	struct key_item_t *tmp;

	if (pdev->dev.of_node)
		of_node_put(pdev->dev.of_node);

	list_for_each_entry_safe(item, tmp, &ukdev->uk_hdr, node) {
		list_del(&item->node);
		kfree(item);
	}
	ukdev->uk_info.key_flag = 0;
	return 0;
}
