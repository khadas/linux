// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <linux/clk-provider.h>
#include <linux/cdev.h>

#include "acc.h"
#include "acc_hw_eq.h"
#include "acc_hw_asrc.h"
#include "ddr_mngr.h"
#include "regs.h"
#include "iomap.h"

#define DRV_NAME "ACC"
//#define DEBUG

struct acc_chipinfo {
	int version;
	int eq_ram_count;
};

struct audio_acc {
	struct device *dev;
	struct acc_chipinfo *chipinfo;
	struct clk *acc_core_clk;
	dev_t devno;
	struct cdev cd;

	/*which module should be effected */
	enum acc_dest acc_module;
	struct aml_audio_controller *actrl;

	/* PEQ for 16ch, max peq index is 8 */
	int eq_enable;
	int eq_tuning_index;
	unsigned int m_mixer_tab[ACC_EQ_INDEX][ACC_EQ_MIXER_RAM_SIZE];
	unsigned int m_eq_tab[ACC_EQ_INDEX][ACC_EQ_FILTER_SIZE_CH][ACC_EQ_FILTER_PARAM_SIZE];

	/* asrc */
	int asrc_enable;
	int asrc_input_samplerate;
	int asrc_output_samplerate;
	int channel;
	int suspend_clk_off;
};

struct audio_acc *acc;

/* coeff = {b2, b1, a2, a1, b0}; format:3.25 */
static unsigned int ACC_EQ_BYPASS_COEFF[ACC_EQ_FILTER_PARAM_SIZE] = {
		0x0, 0x0, 0x0, 0x0, 0x02000000};

/* 20Hz, highpass filter */
static unsigned int ACC_EQ_DC_COEFF[ACC_EQ_DC_RAM_SIZE] = {
		0x01fea987, 0xfc02acf2, 0xfe02ac80, 0x03fd529b, 0x01fea987};

/* Mixer: LL/RL/LR/RR; format:5.23, 0dB:0x800000 */
static unsigned int ACC_EQ_MIXER_COEFF[ACC_EQ_MIXER_RAM_SIZE] = {
		0x00800000, 0, 0, 0x00800000};

static int acc_open(struct inode *ind, struct file *fp)
{
	pr_info("acc open\n");
	return 0;
}

static int acc_release(struct inode *ind, struct file *fp)
{
	pr_info("acc release\n");
	return 0;
}

static ssize_t acc_read(struct file *fp, char __user *buf, size_t size, loff_t *pos)
{
	int rc;
	char kbuf[32] = "read test\n";

	if (size > 32)
		size = 32;

	rc = copy_to_user(buf, kbuf, size);
	if (rc < 0) {
		pr_err("copy_to_user failed!");
		return -EFAULT;
	}
	return size;
}

static ssize_t acc_write(struct file *fp, const char __user *buf, size_t size, loff_t *pos)
{
	int rc;
	char kbuf[32];

	if (size > 32)
		size = 32;

	rc = copy_from_user(kbuf, buf, size);
	if (rc < 0) {
		return -EFAULT;
		pr_err("copy_from_user failed!");
	}
	pr_info("%s", kbuf);
	return size;
}

static const struct file_operations fops = {
	.owner      = THIS_MODULE,
	.open       = acc_open,
	.release    = acc_release,
	.read       = acc_read,
	.write      = acc_write,
};

static int mixer_acc_read(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct audio_acc *p_acc = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc = (struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int max = mc->max;
	unsigned int invert = mc->invert;
	unsigned int value = ((aml_acc_read(p_acc->actrl, reg)) >> shift) & max;

	if (invert)
		value = (~value) & max;
	ucontrol->value.integer.value[0] = value;

	return 0;
}

static int mixer_acc_write(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct audio_acc *p_acc = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc = (struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int max = mc->max;
	unsigned int invert = mc->invert;
	unsigned int value = ucontrol->value.integer.value[0];
	unsigned int new_val = aml_acc_read(p_acc->actrl, reg);

	if (invert)
		value = (~value) & max;
	max = ~(max << shift);
	new_val &= max;
	new_val |= (value << shift);

	aml_acc_write(p_acc->actrl, reg, new_val);

	return 0;
}

static int acc_asrc_sr_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct audio_acc *p_acc = snd_kcontrol_chip(kcontrol);

	ucontrol->value.integer.value[0] = p_acc->asrc_input_samplerate;

	return 0;
}

static int acc_asrc_sr_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct audio_acc *p_acc = snd_kcontrol_chip(kcontrol);

	p_acc->asrc_input_samplerate = ucontrol->value.integer.value[0];
	acc_asrc_set_ratio(p_acc->actrl, p_acc->asrc_input_samplerate,
				p_acc->asrc_output_samplerate,
				p_acc->channel);

	return 0;
}

static void acc_eq_ram_init(struct audio_acc *acc)
{
	struct audio_acc *p_acc = acc;
	unsigned int *val, *param;
	unsigned int mixer_addr = ACC_EQ_MIXER_RAM_ADDR;
	unsigned int eq_addr = ACC_EQ_FILTER_RAM_ADDR;
	unsigned int i = 0, j = 0;

	/* set DC value to ram */
	val = &ACC_EQ_DC_COEFF[0];
	acc_eq_init_ram_coeff(p_acc->actrl, p_acc->chipinfo->eq_ram_count,
			ACC_EQ_DC_RAM_ADDR, ACC_EQ_DC_RAM_SIZE, val);

	for (i = 0; i < ACC_EQ_INDEX; i++) {
		/* set Mixer value to ram */
		val = &ACC_EQ_MIXER_COEFF[0];
		param = &p_acc->m_mixer_tab[i][0];
		acc_eq_init_ram_coeff(p_acc->actrl, p_acc->chipinfo->eq_ram_count,
				mixer_addr, ACC_EQ_MIXER_RAM_SIZE, val);
		memcpy(param, val, ACC_EQ_MIXER_RAM_SIZE * sizeof(unsigned int));
		mixer_addr += ACC_EQ_MIXER_FILTER_OFFSET_SIZE;

		/* set bypass filter coeff of EQ module to ram */
		for (j = 0; j < ACC_EQ_BAND; j++) {
			val = &ACC_EQ_BYPASS_COEFF[0];
			param = &p_acc->m_eq_tab[i][j][0];
			acc_eq_init_ram_coeff(p_acc->actrl, p_acc->chipinfo->eq_ram_count,
					(eq_addr + ACC_EQ_FILTER_PARAM_SIZE * j),
					ACC_EQ_FILTER_PARAM_SIZE, val);
			memcpy(param, val, ACC_EQ_FILTER_PARAM_SIZE * sizeof(unsigned int));
		}
		eq_addr += ACC_EQ_MIXER_FILTER_OFFSET_SIZE;
	}
}

static void acc_asrc_init(struct audio_acc *acc)
{
	struct audio_acc *p_acc = acc;

	/* set ram parameters */
	acc_asrc_arm_init(p_acc->actrl);

	/* set input & output sample rate, channel number */
	acc_asrc_set_ratio(p_acc->actrl, 96000, 48000, 2);
	p_acc->asrc_input_samplerate = 96000;
	p_acc->asrc_output_samplerate = 48000;
	p_acc->channel = 2;

	acc_asrc_enable(p_acc->actrl, true);

}

static int mixer_acc_tuning_index_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct audio_acc *p_acc = snd_kcontrol_chip(kcontrol);

	ucontrol->value.integer.value[0] = p_acc->eq_tuning_index;

	return 0;
}

static int mixer_acc_tuning_index_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct audio_acc *p_acc = snd_kcontrol_chip(kcontrol);

	p_acc->eq_tuning_index = ucontrol->value.integer.value[0];

	return 0;
}

static int mixer_acc_pregain_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct audio_acc *p_acc = snd_kcontrol_chip(kcontrol);
	int index = p_acc->eq_tuning_index;
	unsigned int *p = &p_acc->m_mixer_tab[index][0];
	unsigned int addr = ACC_EQ_MIXER_RAM_ADDR + index * ACC_EQ_MIXER_FILTER_OFFSET_SIZE;

	acc_eq_get_ram_coeff(p_acc->actrl, p_acc->chipinfo->eq_ram_count,
			addr, ACC_EQ_MIXER_RAM_SIZE, p);

	ucontrol->value.integer.value[0] = p_acc->m_mixer_tab[index][0];

	return 0;
}

static int mixer_acc_pregain_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct audio_acc *p_acc = snd_kcontrol_chip(kcontrol);
	int index = p_acc->eq_tuning_index;
	unsigned int *p = &p_acc->m_mixer_tab[index][0];
	unsigned int addr = ACC_EQ_MIXER_RAM_ADDR + index * ACC_EQ_MIXER_FILTER_OFFSET_SIZE;

	/* Mixer: LL = RR = pregain; */
	p_acc->m_mixer_tab[index][0] = ucontrol->value.integer.value[0];
	p_acc->m_mixer_tab[index][1] = 0;
	p_acc->m_mixer_tab[index][2] = 0;
	p_acc->m_mixer_tab[index][3] = ucontrol->value.integer.value[0];

	acc_eq_set_ram_coeff(p_acc->actrl, p_acc->chipinfo->eq_ram_count,
			addr, ACC_EQ_MIXER_RAM_SIZE, p);

	return 0;
}

static int mixer_acc_params_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audio_acc *p_acc = snd_kcontrol_chip(kcontrol);
	int index = p_acc->eq_tuning_index;
	unsigned int *value = (unsigned int *)ucontrol->value.bytes.data;
	unsigned int *p = &p_acc->m_eq_tab[index][0][0];
	unsigned int addr = ACC_EQ_FILTER_RAM_ADDR + index * ACC_EQ_MIXER_FILTER_OFFSET_SIZE;
	int i;

	acc_eq_get_ram_coeff(p_acc->actrl, p_acc->chipinfo->eq_ram_count,
			addr, ACC_EQ_FILTER_RAM_SIZE, p);

	for (i = 0; i < ACC_EQ_FILTER_RAM_SIZE; i++)
		*value++ = cpu_to_be32(*p++);

	return 0;
}

static int str2int(char *str, unsigned int *data, int size)
{
	int num = 0;
	unsigned int temp = 0;
	char *ptr = str;
	unsigned int *val = data;

	while (size-- != 0) {
		if ((*ptr >= '0') && (*ptr <= '9')) {
			temp = temp * 16 + (*ptr - '0');
		} else if ((*ptr >= 'a') && (*ptr <= 'f')) {
			temp = temp * 16 + (*ptr - 'a' + 10);
		} else if (*ptr == ' ') {
			*(val + num) = temp;
			temp = 0;
			num++;
		}
		ptr++;
	}

	return num;
}

static int mixer_acc_params_set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audio_acc *p_acc = snd_kcontrol_chip(kcontrol);
	int index = p_acc->eq_tuning_index;
	unsigned int tmp_data[ACC_FILTER_PARAM_BYTE + 1];
	unsigned int *p_data = &tmp_data[0];
	char tmp_string[ACC_FILTER_PARAM_BYTE];
	char *p_string = &tmp_string[0];
	unsigned int *p;
	int num, i, band_id, addr;
	char *val = (char *)ucontrol->value.bytes.data;

	if (!val)
		return -ENOMEM;
	memcpy(p_string, val, ACC_FILTER_PARAM_BYTE);

	num = str2int(p_string, p_data, ACC_FILTER_PARAM_BYTE);
	band_id = tmp_data[0];
	if (num != (ACC_EQ_FILTER_PARAM_SIZE + 1) || band_id >= ACC_EQ_BAND) {
		pr_info("Error: parma_num = %d, band_id = %d\n",
			num, tmp_data[0]);
		return 0;
	}

	p_data = &tmp_data[1];
	p = &p_acc->m_eq_tab[index][band_id][0];
	for (i = 0; i < ACC_EQ_FILTER_PARAM_SIZE; i++, p++, p_data++)
		*p = *p_data;

	p = &p_acc->m_eq_tab[index][band_id][0];
	addr = ACC_EQ_FILTER_RAM_ADDR + index * ACC_EQ_MIXER_FILTER_OFFSET_SIZE +
			band_id * ACC_EQ_FILTER_PARAM_SIZE;
	acc_eq_set_ram_coeff(p_acc->actrl, p_acc->chipinfo->eq_ram_count,
			addr, ACC_EQ_FILTER_PARAM_SIZE, p);

	return 0;
}

static const struct snd_kcontrol_new snd_acc_controls[] = {
	SOC_SINGLE_EXT("ACC EQ enable",
				AEQ_STATUS0_CTRL, 0x1, 0x1, 0,
				mixer_acc_read,
				mixer_acc_write),

	/* tap: 2~20 */
	SOC_SINGLE_EXT("ACC EQ tap",
				AEQ_STATUS0_CTRL, 0x2, 0x14, 0,
				mixer_acc_read,
				mixer_acc_write),

	/* index: 0~7 */
	SOC_SINGLE_EXT("ACC EQ Tuning Index",
				0, 0, 0x7, 0,
				mixer_acc_tuning_index_get,
				mixer_acc_tuning_index_set),

	/* index: 0~7, Pregain set */
	SOC_SINGLE_EXT("ACC EQ Pregain",
				0, 0, 0x800000, 0,
				mixer_acc_pregain_get,
				mixer_acc_pregain_set),

	/* index: 0~7, EQ Parameters*/
	SND_SOC_BYTES_EXT("ACC EQ Parameters",
				(ACC_EQ_FILTER_SIZE_CH * 4),
				mixer_acc_params_get,
				mixer_acc_params_set),

	/* enable/disable asrc */
	SOC_SINGLE_EXT("ACC ASRC enable",
				AUDIO_RSAMP_ACC_CTRL1, 0x18, 0x1, 0x1,
				mixer_acc_read,
				mixer_acc_write),

	/* input sample rate for asrc */
	SOC_SINGLE_EXT("ACC ASRC input sample rate",
				0, 0, 192000, 0,
				acc_asrc_sr_get,
				acc_asrc_sr_set),
};

int card_add_acc_kcontrols(struct snd_soc_card *card)
{
	unsigned int idx;
	int err;

	if (!acc) {
		pr_info("acc is disable!\n");
		return 0;
	}

	for (idx = 0; idx < ARRAY_SIZE(snd_acc_controls); idx++) {
		err = snd_ctl_add(card->snd_card, snd_ctl_new1(&snd_acc_controls[idx], acc));
		if (err < 0)
			return err;
	}

	return 0;
}

static void acc_init(struct platform_device *pdev)
{
	struct audio_acc *p_acc = dev_get_drvdata(&pdev->dev);

	acc_eq_clk_enable(p_acc->actrl, true);
	acc_asrc_clk_enable(p_acc->actrl, true);

	/* frddr1->EQ->ASRC->toIO/toddr0 */
	acc_eq_set_wrapper(p_acc->actrl);
	acc_asrc_set_wrapper(p_acc->actrl);

	acc_eq_ram_init(p_acc);
	acc_asrc_init(p_acc);

	pr_info("%s: [0x%p]\n", __func__, p_acc);
}

void create_acc_device_node(struct audio_acc *p_acc)
{
	int ret;
	struct class *acc_class;
	struct device *acc_device;

	ret = alloc_chrdev_region(&p_acc->devno, 0, 1, "audio_acc");
	if (ret < 0) {
		pr_err("audio_acc alloc_chrdev_region failed!");
		return;
	}
	cdev_init(&p_acc->cd, &fops);

	ret = cdev_add(&p_acc->cd, p_acc->devno, 1);
	if (ret < 0) {
		pr_err("cdev_add failed!");
		return;
	}

	acc_class = class_create(THIS_MODULE, "audio_acc");
	if (!acc_class) {
		pr_info("create class failed\n");
		cdev_del(&p_acc->cd);
		unregister_chrdev_region(p_acc->devno, 1);
		return;
	}

	acc_device = device_create(acc_class, NULL, p_acc->devno, NULL, "audio_acc");
	if (!acc_device) {
		pr_info("create device failed\n");
		cdev_del(&p_acc->cd);
		unregister_chrdev_region(p_acc->devno, 1);
		class_destroy(acc_class);
		return;
	}
}

static struct acc_chipinfo a4_acc_chipinfo = {
	.version = VERSION1,
	.eq_ram_count = 1,
};

static const struct of_device_id acc_device_id[] = {
	{
		.compatible = "amlogic, snd-effect-acc",
		.data		= &a4_acc_chipinfo,
	},
	{}
};
MODULE_DEVICE_TABLE(of, acc_device_id);

static int acc_platform_probe(struct platform_device *pdev)
{
	struct audio_acc *p_acc;
	struct device *dev = &pdev->dev;
	struct acc_chipinfo *p_chipinfo;
	struct device_node *node = pdev->dev.of_node;
	struct device_node *node_prt = NULL;
	struct platform_device *pdev_parent;
	struct aml_audio_controller *actrl = NULL;
	int output_module = -1;
	int ret;

	p_acc = devm_kzalloc(&pdev->dev, sizeof(struct audio_acc), GFP_KERNEL);
	if (!p_acc)
		return -ENOMEM;

	p_chipinfo = (struct acc_chipinfo *)of_device_get_match_data(dev);
	if (!p_chipinfo)
		dev_warn_once(dev, "check whether to update effect chipinfo\n");
	p_acc->chipinfo = p_chipinfo;

	p_acc->acc_core_clk = devm_clk_get(&pdev->dev, "acc_core");
	if (!IS_ERR(p_acc->acc_core_clk)) {
		dev_info(&pdev->dev, "get acc core clock\n");
		clk_prepare_enable(p_acc->acc_core_clk);
	}
	ret = of_property_read_u32(node, "suspend-clk-off",
			&p_acc->suspend_clk_off);
	if (ret < 0)
		dev_err(&pdev->dev, "Can't retrieve suspend-clk-off\n");
	ret = of_property_read_u32(pdev->dev.of_node, "output_module", &output_module);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve output_module\n");
		return -EINVAL;
	}
	pr_info("%s: acc output module id [%d]\n", __func__, output_module);
	p_acc->acc_module = output_module;

	/* get audio controller */
	node_prt = of_get_parent(node);
	if (!node_prt)
		return -ENXIO;

	pdev_parent = of_find_device_by_node(node_prt);
	of_node_put(node_prt);
	actrl = (struct aml_audio_controller *)platform_get_drvdata(pdev_parent);
	if (!actrl)
		return -ENXIO;
	p_acc->actrl = actrl;

	p_acc->dev = dev;
	acc = p_acc;
	dev_set_drvdata(&pdev->dev, p_acc);

	acc_init(pdev);

	create_acc_device_node(p_acc);

	return 0;
}

static int acc_platform_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct audio_acc *p_acc = dev_get_drvdata(&pdev->dev);

	acc_eq_clk_enable(p_acc->actrl, false);
	acc_asrc_clk_enable(p_acc->actrl, false);
	if (p_acc->suspend_clk_off && !IS_ERR(p_acc->acc_core_clk)) {
		while (__clk_is_enabled(p_acc->acc_core_clk))
			clk_disable_unprepare(p_acc->acc_core_clk);
	}
	return 0;
}

static int acc_platform_resume(struct platform_device *pdev)
{
	struct audio_acc *p_acc = dev_get_drvdata(&pdev->dev);
	if (p_acc->suspend_clk_off && !IS_ERR(p_acc->acc_core_clk)) {
		dev_info(&pdev->dev, "get acc core clock\n");
		clk_prepare_enable(p_acc->acc_core_clk);
	}

	acc_eq_clk_enable(p_acc->actrl, true);
	acc_asrc_clk_enable(p_acc->actrl, true);

	return 0;
}

static struct platform_driver acc_platform_driver = {
	.driver = {
		.name           = DRV_NAME,
		.owner          = THIS_MODULE,
		.of_match_table = of_match_ptr(acc_device_id),
	},
	.probe  = acc_platform_probe,
	.suspend = acc_platform_suspend,
	.resume = acc_platform_resume,
};

int __init acc_platform_init(void)
{
	return platform_driver_register(&(acc_platform_driver));
}

void __exit acc_platform_exit(void)
{
	platform_driver_unregister(&acc_platform_driver);
}

#ifndef MODULE
module_init(acc_platform_init);
module_exit(acc_platform_exit);
MODULE_AUTHOR("AMLogic, Inc.");
MODULE_DESCRIPTION("Amlogic Audio ACC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
#endif
