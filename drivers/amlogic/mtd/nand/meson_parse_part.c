// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/mtd/mtd.h>
#include <linux/slab.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/rawnand.h>
#include <linux/amlogic/aml_rsv.h>
#include <linux/amlogic/aml_mtd_nand.h>
//#include "linux/amlogic/cpu_version.h"

static bool node_has_compatible(struct device_node *pp)
{
	return of_get_property(pp, "compatible", NULL);
}

static int adjust_part_offset(struct mtd_info *master, u8 nr_parts,
			      struct mtd_partition *parts)
{
	u8 i = 0, bl_mode, reserved_part_blk_num = NAND_RSV_BLOCK_NUM;
	u8 internal_part_count = 0;
	u64 part_size, start_blk = 0, part_blk = 0;
	loff_t offset;
	int phys_erase_shift, error = 0;
	loff_t adjust_offset = 0;
	u32 fip_part_size = 0;
	struct nand_chip *nand = mtd_to_nand(master);
	struct meson_nfc *nfc = nand_get_controller_data(mtd_to_nand(master));

	phys_erase_shift = nand->phys_erase_shift;

	adjust_offset = BOOT_TOTAL_PAGES * (loff_t)master->writesize;
	bl_mode = nfc->param_from_dts.bl_mode;
	if (bl_mode == NAND_FIPMODE_DISCRETE) {
		if (nfc->data->bl2ex_mode) {
			aml_nand_param_check_and_layout_init();
			fip_part_size =
				g_ssp.boot_entry[BOOT_AREA_DEVFIP].size *
				nfc->param_from_dts.fip_copies;
			adjust_offset =
				g_ssp.boot_entry[BOOT_AREA_DEVFIP].offset +
				fip_part_size;
			internal_part_count = 4;
			for (i = 0; i < internal_part_count; i++) {
				parts[i].offset =
					g_ssp.boot_entry[i + 1].offset;
				if (i == internal_part_count - 1)
					parts[i].size = fip_part_size;
				else
					parts[i].size = g_ssp.boot_entry[i + 1].size *
					g_ssp.boot_backups;
				pr_info("%s: off %llx, size %llx\n",
					parts[i].name, parts[i].offset,
					parts[i].size);
			}
		} else {
			fip_part_size = nfc->param_from_dts.fip_copies *
				nfc->param_from_dts.fip_size;
			internal_part_count = 1;

			parts[i].offset = adjust_offset +
				reserved_part_blk_num *
				master->erasesize;
			parts[i].size = fip_part_size;
			pr_info("%s: off %llx, size %llx\n", parts[i].name,
				parts[i].offset, parts[i].size);
			adjust_offset += reserved_part_blk_num *
			master->erasesize + fip_part_size;
		}
	}

	for (i = internal_part_count; i < nr_parts; i++) {
		if (master->size < adjust_offset) {
			pr_info("%s %d error: over the nand size!!!\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		parts[i].offset = adjust_offset;
		part_size = parts[i].size;
		if (i == nr_parts - 1)
			part_size = master->size - adjust_offset;
		if (nfc->param_from_dts.skip_bad_block) {
			offset = 0;
			start_blk = 0;
			part_blk = part_size >> phys_erase_shift;
			do {
				offset = adjust_offset + start_blk *
					master->erasesize;
				error =
				master->_block_isbad(master, offset);
				if (error) {
					pr_info("%s %d factory bad addr=%llx\n",
						__func__, __LINE__, (u64)(offset >>
						phys_erase_shift));
					if (i != nr_parts - 1) {
						adjust_offset += master->erasesize;
						continue;
					}
				}
				start_blk++;
			} while (start_blk < part_blk);
		}
		adjust_offset += part_size;
		parts[i].size = adjust_offset - parts[i].offset;
	}

	return 0;
}

static int parse_meson_partitions(struct mtd_info *master,
				  const struct mtd_partition **pparts,
				  struct mtd_part_parser_data *data)
{
	struct mtd_partition *parts;
	struct device_node *mtd_node;
	struct device_node *ofpart_node;
	const char *partname;
	struct device_node *pp;
	u8 nr_parts, i;
	int ret = 0;
	bool dedicated = true;

	/* Pull of_node from the master device node */
	mtd_node = mtd_get_of_node(master);
	if (!mtd_node)
		return 0;

	ofpart_node = of_get_child_by_name(mtd_node, "partitions");
	if (!ofpart_node) {
		/*
		 * We might get here even when ofpart isn't used at all (e.g.,
		 * when using another parser), so don't be louder than
		 * KERN_DEBUG
		 */
		pr_debug("%s: 'partitions' subnode not found on %pOF. Trying to parse direct subnodes as partitions.\n",
			 master->name, mtd_node);
		ofpart_node = mtd_node;
		dedicated = false;
	} else if (!of_device_is_compatible(ofpart_node, "meson-partitions")) {
		/* The 'partitions' subnode might be used by another parser */
		return 0;
	}

	/* First count the subnodes */
	nr_parts = 0;
	for_each_child_of_node(ofpart_node,  pp) {
		if (!dedicated && node_has_compatible(pp))
			continue;

		nr_parts++;
	}

	if (nr_parts == 0)
		return 0;

	parts = kcalloc(nr_parts, sizeof(*parts), GFP_KERNEL);
	if (!parts)
		return -ENOMEM;
	i = 0;
	for_each_child_of_node(ofpart_node,  pp) {
		const __be32 *reg;
		int len;
		int a_cells, s_cells;

		if (!dedicated && node_has_compatible(pp))
			continue;

		reg = of_get_property(pp, "reg", &len);
		if (!reg) {
			if (dedicated) {
				pr_debug("%s: ofpart partition %pOF (%pOF) missing reg property.\n",
					 master->name, pp,
					 mtd_node);
				goto part_fail;
			} else {
				nr_parts--;
				continue;
			}
		}

		a_cells = of_n_addr_cells(pp);
		s_cells = of_n_size_cells(pp);
		if (len / 4 != a_cells + s_cells) {
			pr_debug("%s: ofpart partition %pOF (%pOF) error parsing reg property.\n",
				 master->name, pp,
				 mtd_node);
			goto part_fail;
		}

		parts[i].offset = of_read_number(reg, a_cells);
		parts[i].size = of_read_number(reg + a_cells, s_cells);
		parts[i].of_node = pp;

		partname = of_get_property(pp, "label", &len);
		if (!partname)
			partname = of_get_property(pp, "name", &len);
		parts[i].name = partname;

		if (of_get_property(pp, "read-only", &len))
			parts[i].mask_flags |= MTD_WRITEABLE;

		if (of_get_property(pp, "lock", &len))
			parts[i].mask_flags |= MTD_POWERUP_LOCK;

		i++;
	}

	if (!nr_parts)
		goto part_none;

	i = 0;
	if (!strncmp((char *)parts[i].name, NAND_BOOT_NAME,
		     strlen((const char *)NAND_BOOT_NAME))) {
		parts[i].offset = 0;
		parts[i].size = (master->writesize * BOOT_TOTAL_PAGES);
	} else {
		ret = adjust_part_offset(master, nr_parts, parts);
		if (ret)
			goto part_none;
	}

	*pparts = parts;
	return nr_parts;

part_fail:
	pr_err("%s: error parsing ofpart partition %pOF (%pOF)\n",
	       master->name, pp, mtd_node);
	ret = -EINVAL;
part_none:
	of_node_put(pp);
	kfree(parts);
	return ret;
}

struct mtd_part_parser ofpart_meson_parser = {
	.parse_fn = parse_meson_partitions,
	.name = "meson-partitions",
};

