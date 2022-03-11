// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/gpio.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/module.h>
#include <dt-bindings/gpio/audio-gpio.h>

#include "../regs.h"
#include "../audio_io.h"
#include "../iomap.h"

#define DRV_NAME "pinctrl-audio-sm1"

#define AUDIO_PIN(x) PINCTRL_PIN(x, #x)

static const struct pinctrl_pin_desc sm1_audio_pins[] = {
	AUDIO_PIN(TDM_D0),
	AUDIO_PIN(TDM_D1),
	AUDIO_PIN(TDM_D2),
	AUDIO_PIN(TDM_D3),
	AUDIO_PIN(TDM_D4),
	AUDIO_PIN(TDM_D5),
	AUDIO_PIN(TDM_D6),
	AUDIO_PIN(TDM_D7),
	AUDIO_PIN(TDM_D8),
	AUDIO_PIN(TDM_D9),
	AUDIO_PIN(TDM_D10),
	AUDIO_PIN(TDM_D11),
	AUDIO_PIN(TDM_D12),
	AUDIO_PIN(TDM_D13),
	AUDIO_PIN(TDM_D14),
	AUDIO_PIN(TDM_D15),
	AUDIO_PIN(TDM_D16),
	AUDIO_PIN(TDM_D17),
	AUDIO_PIN(TDM_D18),
	AUDIO_PIN(TDM_D19),
	AUDIO_PIN(TDM_D20),
	AUDIO_PIN(TDM_D21),
	AUDIO_PIN(TDM_D22),
	AUDIO_PIN(TDM_D23),
	AUDIO_PIN(TDM_D24),
	AUDIO_PIN(TDM_D25),
	AUDIO_PIN(TDM_D26),
	AUDIO_PIN(TDM_D27),
	AUDIO_PIN(TDM_D28),
	AUDIO_PIN(TDM_D29),
	AUDIO_PIN(TDM_D30),
	AUDIO_PIN(TDM_D31),

	AUDIO_PIN(TDM_SCLK0),
	AUDIO_PIN(TDM_SCLK1),
	AUDIO_PIN(TDM_SCLK2),
	AUDIO_PIN(TDM_SCLK3),
	AUDIO_PIN(TDM_SCLK4),
	AUDIO_PIN(TDM_LRCLK0),
	AUDIO_PIN(TDM_LRCLK1),
	AUDIO_PIN(TDM_LRCLK2),
	AUDIO_PIN(TDM_LRCLK3),
	AUDIO_PIN(TDM_LRCLK4),
	AUDIO_PIN(TDM_MCLK0),
	AUDIO_PIN(TDM_MCLK1),
	AUDIO_PIN(TDM_MCLK2),
	AUDIO_PIN(TDM_MCLK3),
	AUDIO_PIN(TDM_MCLK4)
};

struct pin_group {
	const char *name;
	const unsigned int *pins;
	const unsigned int num_pins;
};

/* tdm data pins */
static const unsigned int tdm_d0_pins[] = {TDM_D0};
static const unsigned int tdm_d1_pins[] = {TDM_D1};
static const unsigned int tdm_d2_pins[] = {TDM_D2};
static const unsigned int tdm_d3_pins[] = {TDM_D3};
static const unsigned int tdm_d4_pins[] = {TDM_D4};
static const unsigned int tdm_d5_pins[] = {TDM_D5};
static const unsigned int tdm_d6_pins[] = {TDM_D6};
static const unsigned int tdm_d7_pins[] = {TDM_D7};
static const unsigned int tdm_d8_pins[] = {TDM_D8};
static const unsigned int tdm_d9_pins[] = {TDM_D9};
static const unsigned int tdm_d10_pins[] = {TDM_D10};
static const unsigned int tdm_d11_pins[] = {TDM_D11};
static const unsigned int tdm_d12_pins[] = {TDM_D12};
static const unsigned int tdm_d13_pins[] = {TDM_D13};
static const unsigned int tdm_d14_pins[] = {TDM_D14};
static const unsigned int tdm_d15_pins[] = {TDM_D15};
static const unsigned int tdm_d16_pins[] = {TDM_D16};
static const unsigned int tdm_d17_pins[] = {TDM_D17};
static const unsigned int tdm_d18_pins[] = {TDM_D18};
static const unsigned int tdm_d19_pins[] = {TDM_D19};
static const unsigned int tdm_d20_pins[] = {TDM_D20};
static const unsigned int tdm_d21_pins[] = {TDM_D21};
static const unsigned int tdm_d22_pins[] = {TDM_D22};
static const unsigned int tdm_d23_pins[] = {TDM_D23};
static const unsigned int tdm_d24_pins[] = {TDM_D24};
static const unsigned int tdm_d25_pins[] = {TDM_D25};
static const unsigned int tdm_d26_pins[] = {TDM_D26};
static const unsigned int tdm_d27_pins[] = {TDM_D27};
static const unsigned int tdm_d28_pins[] = {TDM_D28};
static const unsigned int tdm_d29_pins[] = {TDM_D29};
static const unsigned int tdm_d30_pins[] = {TDM_D30};
static const unsigned int tdm_d31_pins[] = {TDM_D31};

/* tdm sclk pins */
static const unsigned int tdm_sclk0_pins[] = {TDM_SCLK0};
static const unsigned int tdm_sclk1_pins[] = {TDM_SCLK1};
static const unsigned int tdm_sclk2_pins[] = {TDM_SCLK2};
static const unsigned int tdm_sclk3_pins[] = {TDM_SCLK3};
static const unsigned int tdm_sclk4_pins[] = {TDM_SCLK4};

/* tdm lrclk pins */
static const unsigned int tdm_lrclk0_pins[] = {TDM_LRCLK0};
static const unsigned int tdm_lrclk1_pins[] = {TDM_LRCLK1};
static const unsigned int tdm_lrclk2_pins[] = {TDM_LRCLK2};
static const unsigned int tdm_lrclk3_pins[] = {TDM_LRCLK3};
static const unsigned int tdm_lrclk4_pins[] = {TDM_LRCLK4};

/* tdm mclk pins */
static const unsigned int tdm_mclk0_pins[] = {TDM_MCLK0};
static const unsigned int tdm_mclk1_pins[] = {TDM_MCLK1};
static const unsigned int tdm_mclk2_pins[] = {TDM_MCLK2};
static const unsigned int tdm_mclk3_pins[] = {TDM_MCLK3};
static const unsigned int tdm_mclk4_pins[] = {TDM_MCLK4};

#define GROUP(n)  \
	{			\
		.name = #n,	\
		.pins = n ## _pins,	\
		.num_pins = ARRAY_SIZE(n ## _pins),	\
	}

static const struct pin_group sm1_audio_pin_groups[] = {
	GROUP(tdm_d0),
	GROUP(tdm_d1),
	GROUP(tdm_d2),
	GROUP(tdm_d3),
	GROUP(tdm_d4),
	GROUP(tdm_d5),
	GROUP(tdm_d6),
	GROUP(tdm_d7),
	GROUP(tdm_d8),
	GROUP(tdm_d9),
	GROUP(tdm_d10),
	GROUP(tdm_d11),
	GROUP(tdm_d12),
	GROUP(tdm_d13),
	GROUP(tdm_d14),
	GROUP(tdm_d15),
	GROUP(tdm_d16),
	GROUP(tdm_d17),
	GROUP(tdm_d18),
	GROUP(tdm_d19),
	GROUP(tdm_d20),
	GROUP(tdm_d21),
	GROUP(tdm_d22),
	GROUP(tdm_d23),
	GROUP(tdm_d24),
	GROUP(tdm_d25),
	GROUP(tdm_d26),
	GROUP(tdm_d27),
	GROUP(tdm_d28),
	GROUP(tdm_d29),
	GROUP(tdm_d30),
	GROUP(tdm_d31),

	GROUP(tdm_sclk0),
	GROUP(tdm_sclk1),
	GROUP(tdm_sclk2),
	GROUP(tdm_sclk3),
	GROUP(tdm_sclk4),

	GROUP(tdm_lrclk0),
	GROUP(tdm_lrclk1),
	GROUP(tdm_lrclk2),
	GROUP(tdm_lrclk3),
	GROUP(tdm_lrclk4),

	GROUP(tdm_mclk0),
	GROUP(tdm_mclk1),
	GROUP(tdm_mclk2),
	GROUP(tdm_mclk3),
	GROUP(tdm_mclk4)

};

static int sm1_ap_get_groups_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(sm1_audio_pin_groups);
}

static const char *sm1_ap_get_group_name(struct pinctrl_dev *pctldev,
	       unsigned int selector)
{
	return sm1_audio_pin_groups[selector].name;
}

static int sm1_ap_get_group_pins(struct pinctrl_dev *pctldev, unsigned int selector,
	       const unsigned int **pins, unsigned int *num_pins)
{
	*pins = (unsigned int *)sm1_audio_pin_groups[selector].pins;
	*num_pins = sm1_audio_pin_groups[selector].num_pins;
	return 0;
}

struct audio_pmx_func {
	const char *name;
	const char * const *groups;
	const unsigned int num_groups;
};

static const char * const tdm_groups[] = {
	"tdm_d0", "tdm_d1", "tdm_d2", "tdm_d3", "tdm_d4",
	"tdm_d5", "tdm_d6", "tdm_d7", "tdm_d8", "tdm_d9",
	"tdm_d10", "tdm_d11", "tdm_d12", "tdm_d13", "tdm_d14",
	"tdm_d15", "tdm_d16", "tdm_d17", "tdm_d18", "tdm_d19",
	"tdm_d20", "tdm_d21", "tdm_d22", "tdm_d23", "tdm_d24",
	"tdm_d25", "tdm_d26", "tdm_d27", "tdm_d28", "tdm_d29",
	"tdm_d30", "tdm_d31"
};

static const char * const tdm_clk_groups[] = {
	"tdm_sclk0",
	"tdm_sclk1",
	"tdm_sclk2",
	"tdm_sclk3",
	"tdm_sclk4",
	"tdm_lrclk0",
	"tdm_lrclk1",
	"tdm_lrclk2",
	"tdm_lrclk3",
	"tdm_lrclk4",
	"tdm_mclk0",
	"tdm_mclk1",
	"tdm_mclk2",
	"tdm_mclk3",
	"tdm_mclk4",
};

#define FUNCTION(n, g)				\
	{					\
		.name = #n,			\
		.groups = g ## _groups,		\
		.num_groups = ARRAY_SIZE(g ## _groups),	\
	}

static const struct audio_pmx_func sm1_audio_functions[] = {
	FUNCTION(tdmina_lane0, tdm),
	FUNCTION(tdmina_lane1, tdm),
	FUNCTION(tdmina_lane2, tdm),
	FUNCTION(tdmina_lane3, tdm),
	FUNCTION(tdmina_lane4, tdm),
	FUNCTION(tdmina_lane5, tdm),
	FUNCTION(tdmina_lane6, tdm),
	FUNCTION(tdmina_lane7, tdm),
	FUNCTION(tdminb_lane0, tdm),
	FUNCTION(tdminb_lane1, tdm),
	FUNCTION(tdminb_lane2, tdm),
	FUNCTION(tdminb_lane3, tdm),
	FUNCTION(tdminb_lane4, tdm),
	FUNCTION(tdminb_lane5, tdm),
	FUNCTION(tdminb_lane6, tdm),
	FUNCTION(tdminb_lane7, tdm),
	FUNCTION(tdminc_lane0, tdm),
	FUNCTION(tdminc_lane1, tdm),
	FUNCTION(tdminc_lane2, tdm),
	FUNCTION(tdminc_lane3, tdm),
	FUNCTION(tdminc_lane4, tdm),
	FUNCTION(tdminc_lane5, tdm),
	FUNCTION(tdminc_lane6, tdm),
	FUNCTION(tdminc_lane7, tdm),

	FUNCTION(tdmouta_lane0, tdm),
	FUNCTION(tdmouta_lane1, tdm),
	FUNCTION(tdmouta_lane2, tdm),
	FUNCTION(tdmouta_lane3, tdm),
	FUNCTION(tdmouta_lane4, tdm),
	FUNCTION(tdmouta_lane5, tdm),
	FUNCTION(tdmouta_lane6, tdm),
	FUNCTION(tdmouta_lane7, tdm),
	FUNCTION(tdmoutb_lane0, tdm),
	FUNCTION(tdmoutb_lane1, tdm),
	FUNCTION(tdmoutb_lane2, tdm),
	FUNCTION(tdmoutb_lane3, tdm),
	FUNCTION(tdmoutb_lane4, tdm),
	FUNCTION(tdmoutb_lane5, tdm),
	FUNCTION(tdmoutb_lane6, tdm),
	FUNCTION(tdmoutb_lane7, tdm),
	FUNCTION(tdmoutc_lane0, tdm),
	FUNCTION(tdmoutc_lane1, tdm),
	FUNCTION(tdmoutc_lane2, tdm),
	FUNCTION(tdmoutc_lane3, tdm),
	FUNCTION(tdmoutc_lane4, tdm),
	FUNCTION(tdmoutc_lane5, tdm),
	FUNCTION(tdmoutc_lane6, tdm),
	FUNCTION(tdmoutc_lane7, tdm),

	FUNCTION(tdm_clk_outa, tdm_clk),
	FUNCTION(tdm_clk_outb, tdm_clk),
	FUNCTION(tdm_clk_outc, tdm_clk),
	FUNCTION(tdm_clk_outd, tdm_clk),
	FUNCTION(tdm_clk_oute, tdm_clk),
	FUNCTION(tdm_clk_outf, tdm_clk),

	FUNCTION(tdm_clk_in, tdm_clk),

	FUNCTION(tdmind_lane0, tdm),
	FUNCTION(tdmind_lane1, tdm),
	FUNCTION(tdmind_lane2, tdm),
	FUNCTION(tdmind_lane3, tdm),
	FUNCTION(tdmind_lane4, tdm),
	FUNCTION(tdmind_lane5, tdm),
	FUNCTION(tdmind_lane6, tdm),
	FUNCTION(tdmind_lane7, tdm),

	FUNCTION(tdmoutd_lane0, tdm),
	FUNCTION(tdmoutd_lane1, tdm),
	FUNCTION(tdmoutd_lane2, tdm),
	FUNCTION(tdmoutd_lane3, tdm),
	FUNCTION(tdmoutd_lane4, tdm),
	FUNCTION(tdmoutd_lane5, tdm),
	FUNCTION(tdmoutd_lane6, tdm),
	FUNCTION(tdmoutd_lane7, tdm),
};

static int sm1_ap_get_functions_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(sm1_audio_functions);
}

static const char *sm1_ap_get_fname(struct pinctrl_dev *pctldev,
		unsigned int selector)
{
	return sm1_audio_functions[selector].name;
}

static int sm1_ap_get_groups(struct pinctrl_dev *pctldev, unsigned int selector,
			  const char * const **groups,
			  unsigned int * const num_groups)
{
	*groups = sm1_audio_functions[selector].groups;
	*num_groups = sm1_audio_functions[selector].num_groups;

	return 0;
}

struct ap_data {
	struct aml_audio_controller *actrl;
	struct device *dev;
};

/* refer to sm1_audio_functions[] */
#define FUNC_TDM_DIN_START          0
#define FUNC_TDM_DIN_LAST          23
#define FUNC_TDM_DOUT_START        24
#define FUNC_TDM_DOUT_LAST         47
#define FUNC_TDM_CLK_OUT_START     48
#define FUNC_TDM_CLK_OUT_LAST      53
#define FUNC_TDM_CLK_IN_START      54
#define FUNC_TDM_CLK_IN_LAST       54
#define FUNC_TDMD_DIN_START        55
#define FUNC_TDMD_DIN_LAST         62
#define FUNC_TDMD_DOUT_START       63
#define FUNC_TDMD_DOUT_LAST        70

#define GRP_TDM_SCLK_START         32

static int sm1_ap_set_mux(struct pinctrl_dev *pctldev,
		unsigned int selector, unsigned int group)
{
	struct ap_data *ap = pinctrl_dev_get_drvdata(pctldev);
	struct aml_audio_controller *actrl = ap->actrl;
	unsigned int base = 0, offset = 0, addr = 0, val = 0;

	if (selector <= FUNC_TDM_DIN_LAST) {
		/* tdmin */
		base = EE_AUDIO_DAT_PAD_CTRL0;
		addr = selector / 4 + base;
		offset = (selector % 4) * 8;
		val = group;
		aml_audiobus_update_bits(actrl, addr,
			0x1f << offset, val << offset);
		aml_audiobus_update_bits(actrl, EE_AUDIO_DAT_PAD_CTRLF,
			1 << val, 1 << val);
	} else if (selector <= FUNC_TDM_DOUT_LAST) {
		/* tdmout */
		base = EE_AUDIO_DAT_PAD_CTRL6;
		addr = group / 4 + base;
		offset = (group % 4) * 8;
		val = selector - FUNC_TDM_DOUT_START;
		aml_audiobus_update_bits(actrl, addr,
			0x1f << offset, val << offset);
		aml_audiobus_update_bits(actrl, EE_AUDIO_DAT_PAD_CTRLF,
			1 << group, 0);
	} else if (selector <= FUNC_TDM_CLK_IN_LAST) {
		int tmp = group - GRP_TDM_SCLK_START;

		base = EE_AUDIO_MST_PAD_CTRL1(1);
		addr = base;
		offset = ((tmp >= 5) ? (tmp - 1) : tmp) * 4;

		if (selector <= FUNC_TDM_CLK_OUT_LAST)
			val = selector - FUNC_TDM_CLK_OUT_START;
		else
			val = selector - FUNC_TDM_CLK_IN_START;

		if (selector == FUNC_TDM_CLK_IN_START)
			val |= 1 << 3;
		aml_audiobus_update_bits(actrl, addr,
			0xf << offset, val << offset);
	} else if (selector < FUNC_TDMD_DIN_LAST) {
		base = EE_AUDIO_DAT_PAD_CTRLG;
		addr = base + (selector - FUNC_TDMD_DIN_START) / 4;
		offset = ((selector - FUNC_TDMD_DIN_START) % 4) * 8;
		val = group;
		aml_audiobus_update_bits(actrl, addr,
					0x1f << offset, val << offset);
		aml_audiobus_update_bits(actrl, EE_AUDIO_DAT_PAD_CTRLF,
					1 << val, 1 << val);
	} else if (selector < FUNC_TDMD_DOUT_LAST) {
		base = EE_AUDIO_DAT_PAD_CTRL6;
		addr = group / 4 + base;
		offset = (group % 4) * 8;
		val = (selector - FUNC_TDMD_DOUT_START) + 24;
		aml_audiobus_update_bits(actrl, addr,
			0x1f << offset, val << offset);
		aml_audiobus_update_bits(actrl, EE_AUDIO_DAT_PAD_CTRLF,
			1 << group, 0);
	} else {
		dev_err(ap->dev, "%s() unsupport selector: %d, grp %d\n",
			__func__, selector, group);
	}
	pr_debug("%s(), addr %#x, offset %d, val %d\n",
		__func__, addr, offset, val);
	return 0;
}

static int sm1_ap_pmx_request(struct pinctrl_dev *pctldev, unsigned int offset)
{
	pr_debug("%s(), offset %d\n", __func__, offset);
	return 0;
}

static struct pinmux_ops sm1_ap_pmxops = {
	.request = sm1_ap_pmx_request,
	.get_functions_count = sm1_ap_get_functions_count,
	.get_function_name = sm1_ap_get_fname,
	.get_function_groups = sm1_ap_get_groups,
	.set_mux = sm1_ap_set_mux,
	.strict = true,
};

static void audio_pin_dbg_show(struct pinctrl_dev *pcdev, struct seq_file *s,
			       unsigned int offset)
{
	seq_printf(s, " %s", __func__);
}

static const struct pinctrl_ops sm1_audio_pctrl_ops = {
	.get_groups_count	= sm1_ap_get_groups_count,
	.get_group_name		= sm1_ap_get_group_name,
	.get_group_pins		= sm1_ap_get_group_pins,
	.dt_node_to_map		= pinconf_generic_dt_node_to_map_all,
	.dt_free_map		= pinconf_generic_dt_free_map,
	.pin_dbg_show		= audio_pin_dbg_show,
};

struct pinctrl_desc sm1_audio_pin_desc = {
	.name = DRV_NAME,
	.pctlops = &sm1_audio_pctrl_ops,
	.pmxops = &sm1_ap_pmxops,
	.pins = sm1_audio_pins,
	.npins = ARRAY_SIZE(sm1_audio_pins),
	.owner = THIS_MODULE,
};

static const struct of_device_id sm1_audio_pinctrl_of_match[] = {
	{
		.compatible = "amlogic, sm1-audio-pinctrl",
	},
	{}
};
MODULE_DEVICE_TABLE(of, sm1_audio_pinctrl_of_match);

static int sm1_audio_pinctrl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct device_node *node_prt = NULL;
	struct platform_device *pdev_parent;
	struct pinctrl_dev *ap_dev;
	struct ap_data *ap;

	ap = devm_kzalloc(dev, sizeof(*ap), GFP_KERNEL);
	if (!ap)
		return -ENOMEM;

	ap->dev = dev;
	/* get audio controller */
	node_prt = of_get_parent(node);
	if (!node_prt) {
		dev_err(dev, "%s() line %d err\n", __func__, __LINE__);
		return -ENXIO;
	}

	pdev_parent = of_find_device_by_node(node_prt);
	of_node_put(node_prt);
	ap->actrl = (struct aml_audio_controller *)
				platform_get_drvdata(pdev_parent);
	if (!ap->actrl) {
		dev_err(dev, "%s() line %d err\n", __func__, __LINE__);
		return -ENXIO;
	}

	ap_dev = devm_pinctrl_register(&pdev->dev, &sm1_audio_pin_desc, ap);
	if (IS_ERR(ap_dev)) {
		dev_err(dev, "%s() line %d err\n", __func__, __LINE__);
		return PTR_ERR(ap_dev);
	}

	return 0;
}

static struct platform_driver sm1_audio_pinctrl_driver = {
	.driver = {
		.name           = DRV_NAME,
		.of_match_table = sm1_audio_pinctrl_of_match,
	},
	.probe  = sm1_audio_pinctrl_probe,
};

int __init sm1_audio_pinctrl_init(void)
{
	return platform_driver_register(&sm1_audio_pinctrl_driver);
}

void __exit sm1_audio_pinctrl_exit(void)
{
	platform_driver_unregister(&sm1_audio_pinctrl_driver);
}

#ifndef MODULE
module_init(sm1_audio_pinctrl_init);
module_exit(sm1_audio_pinctrl_exit);
MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic sm1 audio pinctrl driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
#endif
