// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <sound/asoundef.h>

#include "regs.h"
#include "pcpd_monitor.h"
#include "../common/iec_info.h"

#if (defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI ||\
defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI_MODULE)
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#endif

#define DRV_NAME "pcpd_monitor"

static int pcpd_monitor_get_audio_type(struct pcpd_monitor *pc_pd)
{
	struct aml_audio_controller *actrl;
	unsigned int pc;
	unsigned int offset, reg;
	int id;

	if (!pc_pd)
		return 0;
	actrl = pc_pd->actrl;
	id = pc_pd->pcpd_id;
	offset = EE_AUDIO_PCPD_MON_B_STAT1 - EE_AUDIO_PCPD_MON_A_STAT1;
	reg = EE_AUDIO_PCPD_MON_A_STAT1 + offset * id;
	pc = (aml_audiobus_read(actrl, reg) >> 16) & 0xff;
	return pc;
}

int pcpd_monitor_check_audio_type(struct pcpd_monitor *pc_pd)
{
	int total_num = sizeof(type_texts) / sizeof(struct spdif_audio_info);
	int pc = pcpd_monitor_get_audio_type(pc_pd);
	int audio_type = 0;
	int i;
	bool is_raw = true;
	/*need get hdmi pcm info*/
#if (defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI ||\
defined CONFIG_AMLOGIC_MEDIA_TVIN_HDMI_MODULE)
	struct rx_audio_stat_s aud_sts;

	rx_get_audio_status(&aud_sts);
	is_raw = aud_sts.ch_sts[0] & IEC958_AES0_NONAUDIO;
#endif
	if (!is_raw)
		return 0;
	for (i = 0; i < total_num; i++) {
		if (pc == type_texts[i].pc) {
			audio_type = type_texts[i].aud_type;
			break;
		}
	}

	return audio_type;
}

static int aml_pcpd_format_set(struct pcpd_monitor *pc_pd)
{
	unsigned int mask, val;
	struct aml_audio_controller *actrl = pc_pd->actrl;
	int id = pc_pd->pcpd_id;
	struct toddr_fmt *fmt = &pc_pd->tddr->fmt;
	unsigned int offset = EE_AUDIO_PCPD_MON_B_CTRL0 - EE_AUDIO_PCPD_MON_A_CTRL0;
	unsigned int reg = EE_AUDIO_PCPD_MON_A_CTRL0 + offset * id;

	mask = 0x7 << 20 | 0x1f << 8 | 0x1f;
	val = fmt->type << 20 | fmt->msb << 8 | fmt->lsb;
	aml_audiobus_update_bits(actrl, reg, mask, val);

	return 0;
}

static int aml_pcpd_monitor_mask(struct pcpd_monitor *pc_pd)
{
	unsigned int mask, val;
	unsigned int reg;
	struct aml_audio_controller *actrl = pc_pd->actrl;
	int id = pc_pd->pcpd_id;
	unsigned int offset = EE_AUDIO_PCPD_MON_B_CTRL2 - EE_AUDIO_PCPD_MON_A_CTRL2;

	reg = EE_AUDIO_PCPD_MON_A_CTRL2 + offset * id;
	aml_audiobus_write(actrl, reg, 0xffffffff);
	reg = EE_AUDIO_PCPD_MON_A_CTRL3 + offset * id;
	mask = 0x3ff << 20 | 1 << 16;
	val = 0x1f << 20 | 1 << 16;
	aml_audiobus_update_bits(actrl, reg, mask, val);

	return 0;
}

int aml_pcpd_monitor_enable(struct pcpd_monitor *pc_pd, int enable)
{
	struct aml_audio_controller *actrl = pc_pd->actrl;
	int id = pc_pd->pcpd_id;
	unsigned int reg;
	unsigned int offset = EE_AUDIO_PCPD_MON_B_CTRL0 - EE_AUDIO_PCPD_MON_A_CTRL0;

	reg = EE_AUDIO_PCPD_MON_A_CTRL0 + offset * id;
	aml_audiobus_update_bits(actrl, reg, 1 << 31, enable << 31);
	reg = EE_AUDIO_PCPD_MON_A_CTRL5 + offset * id;
	aml_audiobus_update_bits(actrl, reg, 0x7 | 0x7 << 4, 0x7);

	return 0;
}

static int aml_pcpd_monitor_src(struct pcpd_monitor *pc_pd)
{
	unsigned int reg;
	enum toddr_src src = pc_pd->tddr->src;
	int id = pc_pd->pcpd_id;
	struct toddr *to = pc_pd->tddr;
	struct aml_audio_controller *actrl = pc_pd->actrl;
	unsigned int offset = EE_AUDIO_PCPD_MON_B_CTRL0 - EE_AUDIO_PCPD_MON_A_CTRL0;

	reg = EE_AUDIO_PCPD_MON_A_CTRL0 + offset * id;
	src = toddr_src_get_reg(to, src);
	aml_audiobus_update_bits(actrl, reg, 0x1f << 24, src << 24);

	return 0;
}

int aml_pcpd_monitor_init(struct pcpd_monitor *pc_pd)
{
	aml_pcpd_format_set(pc_pd);
	aml_pcpd_monitor_mask(pc_pd);
	aml_pcpd_monitor_src(pc_pd);

	return 0;
}

static int aml_pcpd_monitor_status(struct pcpd_monitor *pc_pd)
{
	int status;
	int i;
	unsigned int reg;
	int id = pc_pd->pcpd_id;
	struct aml_audio_controller *actrl = pc_pd->actrl;
	unsigned int offset = EE_AUDIO_PCPD_MON_B_STAT0 - EE_AUDIO_PCPD_MON_A_STAT0;

	reg = EE_AUDIO_PCPD_MON_A_STAT0 + offset * id;
	status = aml_audiobus_read(actrl, reg);
	offset = EE_AUDIO_PCPD_MON_B_CTRL5 - EE_AUDIO_PCPD_MON_A_CTRL5;
	reg = EE_AUDIO_PCPD_MON_A_CTRL5 + offset * id;
	for (i = 0; i < 3; i++) {
		if (status & (1 << i))
			aml_audiobus_update_bits(actrl, reg, 0x1 << (8 + i), 1 << (8 + i));
	}

	return 0;
}

static irqreturn_t aml_pcpd_monitor_status_isr(int irq, void *devid)
{
	struct pcpd_monitor *pc_pd = (struct pcpd_monitor *)devid;

	aml_pcpd_monitor_status(pc_pd);
	return IRQ_HANDLED;
}

static int aml_pcpd_monitor_platform_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct pcpd_monitor_chipinfo *chip_info;
	struct pcpd_monitor *pc_pd;
	struct device *dev = &pdev->dev;
	struct device_node *node_prt = NULL;
	struct platform_device *pdev_parent;
	struct aml_audio_controller *actrl = NULL;
	int ret = 0;

	pc_pd = devm_kzalloc(dev, sizeof(struct pcpd_monitor), GFP_KERNEL);
	if (!pc_pd)
		return -ENOMEM;
	pc_pd->dev = dev;
	dev_set_drvdata(dev, pc_pd);
	chip_info = (struct pcpd_monitor_chipinfo *)
		of_device_get_match_data(dev);

	pc_pd->pcpd_id = chip_info->id;
	/* get audio controller */
	node_prt = of_get_parent(node);
	if (!node_prt)
		return -ENXIO;

	pdev_parent = of_find_device_by_node(node_prt);
	of_node_put(node_prt);
	actrl = (struct aml_audio_controller *)
				platform_get_drvdata(pdev_parent);

	pc_pd->actrl = actrl;

	pc_pd->pcpd_irq =
	platform_get_irq_byname(pdev, "irq_pcpd");
	if (pc_pd->pcpd_irq < 0)
		dev_err(dev, "platform_get_irq_byname failed\n");
	ret = request_irq(pc_pd->pcpd_irq,
				aml_pcpd_monitor_status_isr, 0, "irq_pcpd", pc_pd);
	aml_audiobus_update_bits(actrl, EE_AUDIO_CLK_GATE_EN1, 3 << 9, 3 << 9);

	return ret;
}

struct platform_driver aml_pcpd_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = aml_pcpd_device_id,
	},
	.probe	 = aml_pcpd_monitor_platform_probe,
};

int __init pcpd_monitor_init(void)
{
	return platform_driver_register(&aml_pcpd_driver);
}

void __exit pcpd_monitor_exit(void)
{
	platform_driver_unregister(&aml_pcpd_driver);
}

#ifndef MODULE
module_init(pcpd_monitor_init);
module_exit(pcpd_monitor_exit);
MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic TDM ASoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, aml_pcpd_device_id);
#endif

