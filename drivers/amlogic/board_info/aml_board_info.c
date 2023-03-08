// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>

#define DRIVE_NAME "board_info"

struct board_info {
	struct class cls;
	int id;
};

static struct board_info info;

static ssize_t id_show(struct class *cls, struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", info.id);
}
static CLASS_ATTR_RO(id);

static struct attribute *aml_board_info_attrs[] = {
	&class_attr_id.attr,
	NULL
};
ATTRIBUTE_GROUPS(aml_board_info);

static int aml_reg_read(unsigned long reg, unsigned int *readval)
{
	void __iomem *vaddr;

	reg = round_down(reg, 0x3);
	vaddr = ioremap(reg, 0x4);
	if (!vaddr)
		return -ENOMEM;
	*readval = readl_relaxed(vaddr);
	iounmap(vaddr);

	return 0;
}

static int aml_board_id_update(void)
{
	struct device_node *np;
	int ret;
	u32 reg[3];
	unsigned int val;

	np = of_find_node_by_path("/board_info");
	if (!np)
		return -ENODEV;

	ret = of_property_read_u32_array(np, "id-reg", reg, ARRAY_SIZE(reg));
	if (ret)
		return ret;

	pr_debug("board id register is 0x%08x, width %d, offset %d\n",
		 reg[0], reg[1], reg[2]);

	ret = aml_reg_read(reg[0], &val);
	if (ret)
		return ret;

	info.id = (int)((val >> reg[2]) & ((0x1 << reg[1]) - 1));
	pr_info("board id: %d\n", info.id);

	return 0;
}

int aml_board_info_id_read(void)
{
	return info.id;
}
EXPORT_SYMBOL(aml_board_info_id_read);

static int __init aml_board_info_init(void)
{
	int ret;

	ret = aml_board_id_update();
	if (ret) {
		pr_info("no register found on device tree\n");
		return ret;
	}

	info.cls.name = DRIVE_NAME;
	info.cls.owner = THIS_MODULE;
	info.cls.class_groups = aml_board_info_groups;
	ret = class_register(&info.cls);

	if (ret) {
		pr_err("fail to create board information class\n");
		return ret;
	}

	return 0;
}

static void __exit aml_board_info_exit(void)
{
	class_unregister(&info.cls);
}

arch_initcall(aml_board_info_init);
module_exit(aml_board_info_exit);
MODULE_AUTHOR("Amlogic");
MODULE_DESCRIPTION("Board Information Driver");
MODULE_LICENSE("GPL");
