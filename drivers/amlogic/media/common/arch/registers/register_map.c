// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
//#define DEBUG
 #define pr_fmt(fmt) "Codec io: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/regmap.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/registers/register_map.h>
#include <linux/amlogic/media/utils/log.h>
/* media module used media/registers/cpu_version.h since kernel 5.4 */
#include <linux/amlogic/media/registers/cpu_version.h>

void set_cpu_type_from_media(int cpu_id);

static u32 regs_cmd_debug;

static struct codecio_device_data_s codecio_meson_dev;

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static struct codecio_device_data_s codecio_gxbb = {
	.cpu_id = MESON_CPU_MAJOR_ID_GXBB,
};

static struct codecio_device_data_s codecio_gxtvbb = {
	.cpu_id = MESON_CPU_MAJOR_ID_GXTVBB,
};

static struct codecio_device_data_s codecio_gxl = {
	.cpu_id = MESON_CPU_MAJOR_ID_GXL,
};

static struct codecio_device_data_s codecio_gxm = {
	.cpu_id = MESON_CPU_MAJOR_ID_GXM,
};

static struct codecio_device_data_s codecio_txl = {
	.cpu_id = MESON_CPU_MAJOR_ID_TXL,
};

static struct codecio_device_data_s codecio_txlx = {
	.cpu_id = MESON_CPU_MAJOR_ID_TXLX,
};

static struct codecio_device_data_s codecio_axg = {
	.cpu_id = MESON_CPU_MAJOR_ID_AXG,
};
#endif

static struct codecio_device_data_s codecio_tl1 = {
	.cpu_id = MESON_CPU_MAJOR_ID_TL1,
};

static struct codecio_device_data_s codecio_g12a = {
	.cpu_id = MESON_CPU_MAJOR_ID_G12A,
};

static struct codecio_device_data_s codecio_g12b = {
	.cpu_id = MESON_CPU_MAJOR_ID_G12B,
};

static struct codecio_device_data_s codecio_sm1 = {
	.cpu_id = MESON_CPU_MAJOR_ID_SM1,
};

static struct codecio_device_data_s codecio_tm2 = {
	.cpu_id = MESON_CPU_MAJOR_ID_TM2,
};

static struct codecio_device_data_s codecio_sc2 = {
	.cpu_id = MESON_CPU_MAJOR_ID_SC2,
};

static struct codecio_device_data_s codecio_t5 = {
	.cpu_id = MESON_CPU_MAJOR_ID_T5,
};

static struct codecio_device_data_s codecio_t5d = {
	.cpu_id = MESON_CPU_MAJOR_ID_T5D,
};

static struct codecio_device_data_s codecio_t7 = {
	.cpu_id = MESON_CPU_MAJOR_ID_T7,
};

static struct codecio_device_data_s codecio_s4 = {
	.cpu_id = MESON_CPU_MAJOR_ID_S4,
};

static struct codecio_device_data_s codecio_s4d = {
	.cpu_id = MESON_CPU_MAJOR_ID_S4D,
};

static struct codecio_device_data_s codecio_t3 = {
	.cpu_id = MESON_CPU_MAJOR_ID_T3,
};

static struct codecio_device_data_s codecio_t5w = {
	.cpu_id = MESON_CPU_MAJOR_ID_T5W,
};

static struct codecio_device_data_s codecio_s5 = {
	.cpu_id = MESON_CPU_MAJOR_ID_S5,
};

static struct codecio_device_data_s codecio_c3 = {
	.cpu_id = MESON_CPU_MAJOR_ID_C3,
};

static struct codecio_device_data_s codecio_a4 = {
	.cpu_id = MESON_CPU_MAJOR_ID_A4,
};

static const struct of_device_id codec_io_dt_match[] = {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible = "amlogic, meson-gxbb, codec-io",
		.data = &codecio_gxbb,

	},
	{
		.compatible = "amlogic, meson-gxtvbb, codec-io",
		.data = &codecio_gxtvbb,

	},
	{
		.compatible = "amlogic, meson-gxl, codec-io",
		.data = &codecio_gxl,
	},
	{
		.compatible = "amlogic, meson-gxm, codec-io",
		.data = &codecio_gxm,

	},
	{
		.compatible = "amlogic, meson-txl, codec-io",
		.data = &codecio_txl,
	},
	{
		.compatible = "amlogic, meson-txlx, codec-io",
		.data = &codecio_txlx,

	},
	{
		.compatible = "amlogic, meson-axg, codec-io",
		.data = &codecio_axg,

	},
#endif
	{
		.compatible = "amlogic, meson-tl1, codec-io",
		.data = &codecio_tl1,
	},
	{
		.compatible = "amlogic, meson-g12a, codec-io",
		.data = &codecio_g12a,
	},
	{
		.compatible = "amlogic, meson-g12b, codec-io",
		.data = &codecio_g12b,
	},
	{
		.compatible = "amlogic, meson-sm1, codec-io",
		.data = &codecio_sm1,
	},
	{
		.compatible = "amlogic, meson-tm2, codec-io",
		.data = &codecio_tm2,
	},
	{
		.compatible = "amlogic, meson-sc2, codec-io",
		.data = &codecio_sc2,
	},
	{
		.compatible = "amlogic, meson-t5, codec-io",
		.data = &codecio_t5,
	},
	{
		.compatible = "amlogic, meson-t5d, codec-io",
		.data = &codecio_t5d,
	},
	{
		.compatible = "amlogic, meson-t7, codec-io",
		.data = &codecio_t7,
	},
	{
		.compatible = "amlogic, meson-s4, codec-io",
		.data = &codecio_s4,
	},
	{
		.compatible = "amlogic, meson-s4d, codec-io",
		.data = &codecio_s4d,
	},
	{
		.compatible = "amlogic, meson-t3, codec-io",
		.data = &codecio_t3,
	},
	{
		.compatible = "amlogic, meson-t5w, codec-io",
		.data = &codecio_t5w,
	},
	{
		.compatible = "amlogic, meson-s5, codec-io",
		.data = &codecio_s5,
	},
	{
		.compatible = "amlogic, meson-c3, codec-io",
		.data = &codecio_c3,
	},
	{
		.compatible = "amlogic, meson-a4, codec-io",
		.data = &codecio_a4,
	},
	{},
};

static void __iomem *codecio_reg_map[CODECIO_BUS_MAX];
uint codecio_reg_start[CODECIO_BUS_MAX];
void __iomem *vpp_base;
void __iomem *hiu_base;

static int codecio_reg_read(u32 bus_type, unsigned int reg, unsigned int *val)
{
	if (bus_type < CODECIO_BUS_MAX) {
		if (!codecio_reg_map[bus_type]) {
			pr_err("No support bus type %d to read.\n", bus_type);
			return -1;
		}

		*val = readl((codecio_reg_map[bus_type] + reg));
		return 0;
	} else {
		dump_stack();
		return -1;
	}
}

static int codecio_reg_write(u32 bus_type, unsigned int reg, unsigned int val)
{
	if (bus_type < CODECIO_BUS_MAX) {
		if (!codecio_reg_map[bus_type]) {
			pr_err("No support bus type %d to write.\n", bus_type);
			return -1;
		}

		writel(val, (codecio_reg_map[bus_type] + reg));
		return 0;
	} else {
		dump_stack();
		return -1;
	}
}

int codecio_read_cbus(unsigned int reg)
{
	int ret, val;

	ret = codecio_reg_read(CODECIO_CBUS_BASE, reg << 2, &val);
	if (ret) {
		pr_err("read cbus reg %x error %d\n", reg, ret);
		return -1;
	} else {
		return val;
	}
}

void codecio_write_cbus(unsigned int reg, unsigned int val)
{
	int ret;

	ret = codecio_reg_write(CODECIO_CBUS_BASE, reg << 2, val);
	if (ret) {
		pr_err("write cbus reg %x error %d\n", reg, ret);
		return;
	} else {
		return;
	}
}

int codecio_read_dosbus(unsigned int reg)
{
	int ret, val = 0;

	ret = codecio_reg_read(CODECIO_DOSBUS_BASE, reg << 2, &val);
	if (ret) {
		pr_err("read cbus reg %x error %d\n", reg, ret);
		val = -1;
	} else {
		if (regs_cmd_debug)
			pr_info("DOS READ , reg: 0x%x, val: 0x%x\n", reg, val);
	}
	return val;
}

void codecio_write_dosbus(unsigned int reg, unsigned int val)
{
	int ret;

	ret = codecio_reg_write(CODECIO_DOSBUS_BASE, reg << 2, val);
	if (ret) {
		pr_err("write cbus reg %x error %d\n", reg, ret);
	} else {
		if (regs_cmd_debug)
			pr_info("DOS WRITE, reg: 0x%x, val: 0x%x\n", reg, val);
	}
}

int codecio_read_hiubus(unsigned int reg)
{
	int ret, val;

	ret = codecio_reg_read(CODECIO_HIUBUS_BASE, reg << 2, &val);
	if (ret) {
		pr_err("read cbus reg %x error %d\n", reg, ret);
		return -1;
	} else {
		return val;
	}
}

void codecio_write_hiubus(unsigned int reg, unsigned int val)
{
	int ret;

	ret = codecio_reg_write(CODECIO_HIUBUS_BASE, reg << 2, val);
	if (ret) {
		pr_err("write cbus reg %x error %d\n", reg, ret);
		return;
	} else {
		return;
	}
}

int codecio_read_aobus(unsigned int reg)
{
	int ret, val;

	/*
	 *reg don't left thift for AOBUS
	 */
	ret = codecio_reg_read(CODECIO_AOBUS_BASE, reg, &val);
	if (ret) {
		pr_err("read cbus reg %x error %d\n", reg, ret);
		return -1;
	} else {
		return val;
	}
}

void codecio_write_aobus(unsigned int reg, unsigned int val)
{
	int ret;

	ret = codecio_reg_write(CODECIO_AOBUS_BASE, reg, val);
	if (ret) {
		pr_err("write cbus reg %x error %d\n", reg, ret);
		return;
	} else {
		return;
	}
}

int codecio_read_vcbus(unsigned int reg)
{
	int ret, val;

	if (reg >= 0x1900 && reg < 0x1a00) {
		pr_err("read vcbus reg %x error!\n", reg);
		return 0;
	}
	ret = codecio_reg_read(CODECIO_VCBUS_BASE, reg << 2, &val);
	if (ret) {
		pr_err("read vcbus reg %x error %d\n", reg, ret);
		return -1;
	} else {
		return val;
	}
}

void codecio_write_vcbus(unsigned int reg, unsigned int val)
{
	int ret;

	if (reg >= 0x1900 && reg < 0x1a00) {
		pr_err("write vcbus reg %x error!\n", reg);
		return;
	}
	ret = codecio_reg_write(CODECIO_VCBUS_BASE, reg << 2, val);
	if (ret) {
		pr_err("write vcbus reg %x error %d\n", reg, ret);
		return;
	} else {
		return;
	}
}

int codecio_read_dmcbus(unsigned int reg)
{
	int ret, val;

	ret = codecio_reg_read(CODECIO_DMCBUS_BASE, reg << 2, &val);
	if (ret) {
		pr_err("read cbus reg %x error %d\n", reg, ret);
		return -1;
	} else {
		return val;
	}
}
EXPORT_SYMBOL(codecio_read_dmcbus);

void codecio_write_dmcbus(unsigned int reg, unsigned int val)
{
	int ret;

	ret = codecio_reg_write(CODECIO_DMCBUS_BASE, reg << 2, val);
	if (ret) {
		pr_err("write dmcbus reg %x error %d\n", reg, ret);
		return;
	} else {
		return;
	}
}
EXPORT_SYMBOL(codecio_write_dmcbus);

int codecio_read_parsbus(unsigned int reg)
{
	int ret, val;

	ret = codecio_reg_read(CODECIO_CBUS_BASE, reg << 2, &val);
	if (ret) {
		pr_err("read parser reg %x error %d\n", reg, ret);
		return -1;
	}

	return val;
}

void codecio_write_parsbus(unsigned int reg, unsigned int val)
{
	int ret;

	ret = codecio_reg_write(CODECIO_CBUS_BASE, reg << 2, val);
	if (ret)
		pr_err("write parser reg %x error %d\n", reg, ret);
}

int codecio_read_aiubus(unsigned int reg)
{
	int ret, val;

	ret = codecio_reg_read(CODECIO_CBUS_BASE, reg << 2, &val);
	if (ret) {
		pr_err("read aiu reg %x error %d\n", reg, ret);
		return -1;
	}

	return val;
}

void codecio_write_aiubus(unsigned int reg, unsigned int val)
{
	int ret;

	ret = codecio_reg_write(CODECIO_CBUS_BASE, reg << 2, val);
	if (ret)
		pr_err("write aiu reg %x error %d\n", reg, ret);
}

int codecio_read_demuxbus(unsigned int reg)
{
	int ret, val;

	ret = codecio_reg_read(CODECIO_CBUS_BASE, reg << 2, &val);
	if (ret) {
		pr_err("read demux reg %x error %d\n", reg, ret);
		return -1;
	}

	return val;
}

void codecio_write_demuxbus(unsigned int reg, unsigned int val)
{
	int ret;

	ret = codecio_reg_write(CODECIO_CBUS_BASE, reg << 2, val);
	if (ret)
		pr_err("write demux reg %x error %d\n", reg, ret);
}

int codecio_read_resetbus(unsigned int reg)
{
	int ret, val;

	ret = codecio_reg_read(CODECIO_CBUS_BASE, reg << 2, &val);
	if (ret) {
		pr_err("read reset reg %x error %d\n", reg, ret);
		return -1;
	}

	return val;
}

void codecio_write_resetbus(unsigned int reg, unsigned int val)
{
	int ret;

	ret = codecio_reg_write(CODECIO_CBUS_BASE, reg << 2, val);
	if (ret)
		pr_err("write reset reg %x error %d\n", reg, ret);
}

int codecio_read_efusebus(unsigned int reg)
{
	int ret, val;

	ret = codecio_reg_read(CODECIO_EFUSE_BASE, reg << 2, &val);
	if (ret) {
		pr_err("read reset reg %x error %d\n", reg, ret);
		return -1;
	}

	return val;
}

void codecio_write_efusebus(unsigned int reg, unsigned int val)
{
	int ret;

	ret = codecio_reg_write(CODECIO_EFUSE_BASE, reg << 2, val);
	if (ret)
		pr_err("write reset reg %x error %d\n", reg, ret);
}

int codecio_read_nocbus(unsigned int reg)
{
	int ret, val;

	ret = codecio_reg_read(CODECIO_NOC_BASE, reg, &val);
	if (ret) {
		pr_err("read reset reg %x error %d\n", reg, ret);
		return -1;
	}

	return val;
}

void codecio_write_nocbus(unsigned int reg, unsigned int val)
{
	int ret;

	ret = codecio_reg_write(CODECIO_NOC_BASE, reg, val);
	if (ret)
		pr_err("write reset reg %x error %d\n", reg, ret);
}

/* aml_read_xxx/ aml_write_xxx apis*/
int aml_reg_read(u32 bus_type, unsigned int reg, unsigned int *val)
{
	return codecio_reg_read(bus_type, reg, val);
}
EXPORT_SYMBOL(aml_reg_read);

int aml_reg_write(u32 bus_type, unsigned int reg, unsigned int val)
{
	return codecio_reg_write(bus_type, reg, val);
}
EXPORT_SYMBOL(aml_reg_write);

int aml_regmap_update_bits(u32 bus_type,
			   unsigned int reg,
			   unsigned int mask,
			   unsigned int val)
{
	int ret = 0;
	unsigned int tmp, orig;

	ret = aml_reg_read(bus_type, reg, &orig);
	if (ret) {
		pr_err("write bus reg %x error %d\n", reg, ret);
		return ret;
	}
	tmp = orig & ~mask;
	tmp |= val & mask;
	ret = aml_reg_write(bus_type, reg, tmp);
	if (ret)
		pr_err("write bus reg %x error %d\n", reg, ret);
	return ret;
}
EXPORT_SYMBOL(aml_regmap_update_bits);

int aml_read_cbus(unsigned int reg)
{
	int ret, val;

	ret = aml_reg_read(CODECIO_CBUS_BASE, reg << 2, &val);
	if (ret) {
		pr_err("read cbus reg %x error %d\n", reg, ret);
		return -1;
	} else {
		return val;
	}
}
EXPORT_SYMBOL(aml_read_cbus);

void aml_write_cbus(unsigned int reg, unsigned int val)
{
	int ret;

	ret = aml_reg_write(CODECIO_CBUS_BASE, reg << 2, val);
	if (ret)
		pr_err("write cbus reg %x error %d\n", reg, ret);
}
EXPORT_SYMBOL(aml_write_cbus);

void aml_cbus_update_bits(unsigned int reg,
			  unsigned int mask,
			  unsigned int val)
{
	int ret;

	ret = aml_regmap_update_bits(CODECIO_CBUS_BASE, reg << 2, mask, val);
	if (ret)
		pr_err("write cbus reg %x error %d\n", reg, ret);
}
EXPORT_SYMBOL(aml_cbus_update_bits);

int aml_read_aobus(unsigned int reg)
{
	int ret, val;

	ret = aml_reg_read(CODECIO_AOBUS_BASE, reg, &val);
	if (ret) {
		pr_err("read ao bus reg %x error %d\n", reg, ret);
		return -1;
	} else {
		return val;
	}
}
EXPORT_SYMBOL(aml_read_aobus);

void aml_write_aobus(unsigned int reg, unsigned int val)
{
	int ret;

	ret = aml_reg_write(CODECIO_AOBUS_BASE, reg, val);
	if (ret)
		pr_err("write ao bus reg %x error %d\n", reg, ret);
}
EXPORT_SYMBOL(aml_write_aobus);

void aml_aobus_update_bits(unsigned int reg,
			   unsigned int mask,
			   unsigned int val)
{
	int ret;

	ret = aml_regmap_update_bits(CODECIO_AOBUS_BASE, reg, mask, val);
	if (ret)
		pr_err("write aobus reg %x error %d\n", reg, ret);
}
EXPORT_SYMBOL(aml_aobus_update_bits);

int aml_read_vcbus(unsigned int reg)
{
	int ret, val;

	ret = aml_reg_read(CODECIO_VCBUS_BASE, reg << 2, &val);

	if (ret) {
		pr_err("read vcbus reg %x error %d\n", reg, ret);
		return -1;
	} else {
		return val;
	}
}
EXPORT_SYMBOL(aml_read_vcbus);

void aml_write_vcbus(unsigned int reg, unsigned int val)
{
	int ret;

	ret = aml_reg_write(CODECIO_VCBUS_BASE, reg << 2, val);

	if (ret)
		pr_err("write vcbus reg %x error %d\n", reg, ret);
}
EXPORT_SYMBOL(aml_write_vcbus);

void aml_vcbus_update_bits(unsigned int reg,
			   unsigned int mask,
			   unsigned int val)
{
	int ret;

	ret = aml_regmap_update_bits(CODECIO_VCBUS_BASE,
				     reg << 2, mask, val);
	if (ret)
		pr_err("write vcbus reg %x error %d\n", reg, ret);
}
EXPORT_SYMBOL(aml_vcbus_update_bits);

int aml_read_hiubus(unsigned int reg)
{
	int ret, val;

	ret = aml_reg_read(CODECIO_HIUBUS_BASE, reg << 2, &val);
	if (ret) {
		pr_err("read cbus reg %x error %d\n", reg, ret);
		return -1;
	} else {
		return val;
	}
}
EXPORT_SYMBOL(aml_read_hiubus);

void aml_write_hiubus(unsigned int reg, unsigned int val)
{
	int ret;

	ret = aml_reg_write(CODECIO_HIUBUS_BASE, reg << 2, val);
	if (ret)
		pr_err("write cbus reg %x error %d\n", reg, ret);
}
EXPORT_SYMBOL(aml_write_hiubus);

void aml_hiubus_update_bits(unsigned int reg,
			    unsigned int mask, unsigned int val)
{
	int ret;

	ret = aml_regmap_update_bits(CODECIO_HIUBUS_BASE,
				     reg << 2, mask, val);
	if (ret)
		pr_err("write cbus reg %x error %d\n", reg, ret);
}
EXPORT_SYMBOL(aml_hiubus_update_bits);

int aml_read_dmcbus(unsigned int reg)
{
	int ret, val;

	ret = aml_reg_read(CODECIO_DMCBUS_BASE, reg << 2, &val);
	if (ret) {
		pr_err("read cbus reg %x error %d\n", reg, ret);
		return -1;
	} else {
		return val;
	}
}
EXPORT_SYMBOL(aml_read_dmcbus);

void aml_write_dmcbus(unsigned int reg, unsigned int val)
{
	int ret;

	ret = aml_reg_write(CODECIO_DMCBUS_BASE, reg << 2, val);
	if (ret)
		pr_err("write cbus reg %x error %d\n", reg, ret);
}
EXPORT_SYMBOL(aml_write_dmcbus);

void aml_dmcbus_update_bits(unsigned int reg,
			    unsigned int mask, unsigned int val)
{
	int ret;

	ret = aml_regmap_update_bits(CODECIO_DMCBUS_BASE,
				     reg << 2, mask, val);
	if (ret)
		pr_err("write cbus reg %x error %d\n", reg, ret);
}
EXPORT_SYMBOL(aml_dmcbus_update_bits);

int aml_read_dosbus(unsigned int reg)
{
	int ret, val;

	ret = aml_reg_read(CODECIO_DOSBUS_BASE, reg << 2, &val);
	if (ret) {
		pr_err("read cbus reg %x error %d\n", reg, ret);
		return -1;
	} else {
		return val;
	}
}
EXPORT_SYMBOL(aml_read_dosbus);

void aml_write_dosbus(unsigned int reg, unsigned int val)
{
	int ret;

	ret = aml_reg_write(CODECIO_DOSBUS_BASE, reg << 2, val);
	if (ret)
		pr_err("write cbus reg %x error %d\n", reg, ret);
}
EXPORT_SYMBOL(aml_write_dosbus);

void aml_dosbus_update_bits(unsigned int reg,
			    unsigned int mask, unsigned int val)
{
	int ret;

	ret = aml_regmap_update_bits(CODECIO_DOSBUS_BASE,
				     reg << 2, mask, val);
	if (ret)
		pr_err("write cbus reg %x error %d\n", reg, ret);
}
EXPORT_SYMBOL(aml_dosbus_update_bits);

/* end of aml_read_xxx/ aml_write_xxx apis*/
static int __init codec_io_probe(struct platform_device *pdev)
{
	int i = 0;
	struct resource res;
	//void __iomem *base;

	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		struct codecio_device_data_s *codecio_meson;
		struct device_node	*of_node = pdev->dev.of_node;

		match = of_match_node(codec_io_dt_match, of_node);
		if (match) {
			codecio_meson =
				(struct codecio_device_data_s *)match->data;
			if (codecio_meson) {
				memcpy(&codecio_meson_dev, codecio_meson,
				       sizeof(struct codecio_device_data_s));
			} else {
				pr_err("%s data NOT match\n", __func__);
				return -ENODEV;
			}
		} else {
			pr_err("%s NOT match\n", __func__);
			return -ENODEV;
		}
	}

	for (i = CODECIO_CBUS_BASE; i < CODECIO_BUS_MAX; i++) {
		if (of_address_to_resource(pdev->dev.of_node, i, &res)) {
			pr_debug("i=%d, skip ioremap\n", i);
		}
		if (res.start != 0) {
			codecio_reg_map[i] =
				ioremap(res.start, resource_size(&res));
			codecio_reg_start[i] = res.start;
			if (!codecio_reg_map[i]) {
				pr_err("cannot map codec_io registers\n");
				return -ENOMEM;
			}
			pr_debug("codec map io source 0x%lx,size=%d to 0x%lx\n",
			       (unsigned long)res.start,
				 (int)resource_size(&res),
				 (unsigned long)codecio_reg_map[i]);
		} else {
			codecio_reg_map[i] = 0;
			codecio_reg_start[i] = 0;
			pr_debug("ignore io source start %lx,size=%d\n",
			       (unsigned long)res.start, (int)resource_size(&res));
		}

		if (i == CODECIO_HIUBUS_BASE)
			hiu_base = codecio_reg_map[i];
		if (i == CODECIO_VCBUS_BASE)
			vpp_base = codecio_reg_map[i];

	}

	set_cpu_type_from_media(codecio_meson_dev.cpu_id);
	pr_info("%s success, cpu_type=0x%x\n", __func__, get_cpu_type());

	return 0;
}

static struct platform_driver codec_io_platform_driver = {
	.driver = {
			.owner = THIS_MODULE,
			.name = "codec_io",
			.of_match_table = codec_io_dt_match,
		},
};

int __init codec_io_init(void)
{
	int ret;

	ret = platform_driver_probe(&codec_io_platform_driver,
				    codec_io_probe);
	if (ret) {
		pr_err("Unable to register codec_io driver\n");
		return ret;
	}

	return ret;
}

module_param(regs_cmd_debug, uint, 0664);
MODULE_PARM_DESC(regs_cmd_debug, "\n register commands sequence debug.\n");

