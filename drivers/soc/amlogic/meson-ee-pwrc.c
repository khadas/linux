// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 BayLibre, SAS
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 */

#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/bitfield.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/of_device.h>
#include <linux/reset-controller.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/ctype.h>
#include <dt-bindings/power/meson-g12a-power.h>
#include <dt-bindings/power/meson-sm1-power.h>
#include <dt-bindings/power/meson-tm2-power.h>

/* AO Offsets */

#define AO_RTI_GEN_PWR_SLEEP0		(0x3a << 2)
#define AO_RTI_GEN_PWR_ISO0		(0x3b << 2)

/* HHI Offsets */

#define HHI_MEM_PD_REG0			(0x40 << 2)
#define HHI_VPU_MEM_PD_REG0		(0x41 << 2)
#define HHI_VPU_MEM_PD_REG1		(0x42 << 2)
#define HHI_VPU_MEM_PD_REG3		(0x43 << 2)
#define HHI_VPU_MEM_PD_REG4		(0x44 << 2)
#define HHI_AUDIO_MEM_PD_REG0		(0x45 << 2)
#define HHI_NANOQ_MEM_PD_REG0		(0x46 << 2)
#define HHI_NANOQ_MEM_PD_REG1		(0x47 << 2)
#define HHI_VPU_MEM_PD_REG2		(0x4d << 2)
#define TM2_HHI_VPU_MEM_PD_REG3		(0x4e << 2)
#define TM2_HHI_VPU_MEM_PD_REG4		(0x4c << 2)
#define HHI_DEMOD_MEM_PD_REG	(0x043 << 2)
#define HHI_DSP_MEM_PD_REG0		(0x044 << 2)

#define DOS_MEM_PD_VDEC			(0x0 << 2)
#define DOS_MEM_PD_HCODEC		(0x2 << 2)
#define DOS_MEM_PD_HEVC			(0x2 << 2)
#define DOS_MEM_PD_WAVE420L		(0x9 << 2)

struct meson_ee_pwrc;
struct meson_ee_pwrc_domain;

struct meson_ee_pwrc_mem_domain {
	unsigned int reg;
	unsigned int mask;
};

struct meson_ee_pwrc_top_domain {
	unsigned int sleep_reg;
	unsigned int sleep_mask;
	unsigned int iso_reg;
	unsigned int iso_mask;
};

struct meson_ee_pwrc_domain_desc {
	char *name;
	unsigned int reset_names_count;
	unsigned int clk_names_count;
	unsigned int domain_id;
	struct meson_ee_pwrc_top_domain *top_pd;
	unsigned int mem_pd_count;
	struct meson_ee_pwrc_mem_domain *mem_pd;
	unsigned int flags;
	bool (*get_power)(struct meson_ee_pwrc_domain *pwrc_domain);
};

struct meson_ee_pwrc_domain_data {
	unsigned int count;
	struct meson_ee_pwrc_domain_desc *domains;
};

/* TOP Power Domains */

static struct meson_ee_pwrc_top_domain g12a_pwrc_vpu = {
	.sleep_reg = AO_RTI_GEN_PWR_SLEEP0,
	.sleep_mask = BIT(8),
	.iso_reg = AO_RTI_GEN_PWR_SLEEP0,
	.iso_mask = BIT(9),
};

#define SM1_EE_PD(__bit)					\
	{							\
		.sleep_reg = AO_RTI_GEN_PWR_SLEEP0, 		\
		.sleep_mask = BIT(__bit), 			\
		.iso_reg = AO_RTI_GEN_PWR_ISO0, 		\
		.iso_mask = BIT(__bit), 			\
	}

static struct meson_ee_pwrc_top_domain sm1_pwrc_vpu = SM1_EE_PD(8);
static struct meson_ee_pwrc_top_domain sm1_pwrc_nna = SM1_EE_PD(16);
static struct meson_ee_pwrc_top_domain sm1_pwrc_usb = SM1_EE_PD(17);
static struct meson_ee_pwrc_top_domain sm1_pwrc_pci = SM1_EE_PD(18);
static struct meson_ee_pwrc_top_domain sm1_pwrc_ge2d = SM1_EE_PD(19);
static struct meson_ee_pwrc_top_domain sm1_pwrc_vdec = SM1_EE_PD(1);
static struct meson_ee_pwrc_top_domain sm1_pwrc_hcodec = SM1_EE_PD(0);
static struct meson_ee_pwrc_top_domain sm1_pwrc_hevc = SM1_EE_PD(2);
static struct meson_ee_pwrc_top_domain sm1_pwrc_wave420l = SM1_EE_PD(3);

#define TM2_EE_PD(__bit)					\
	{							\
		.sleep_reg = AO_RTI_GEN_PWR_SLEEP0,		\
		.sleep_mask = BIT(__bit),			\
		.iso_reg = AO_RTI_GEN_PWR_ISO0,			\
		.iso_mask = BIT(__bit),				\
	}

static struct meson_ee_pwrc_top_domain tm2_pwrc_vpu = TM2_EE_PD(8);
static struct meson_ee_pwrc_top_domain tm2_pwrc_nna = TM2_EE_PD(16);
static struct meson_ee_pwrc_top_domain tm2_pwrc_usb = TM2_EE_PD(17);
static struct meson_ee_pwrc_top_domain tm2_pwrc_pciea = TM2_EE_PD(18);
static struct meson_ee_pwrc_top_domain tm2_pwrc_ge2d = TM2_EE_PD(19);
static struct meson_ee_pwrc_top_domain tm2_pwrc_vdec = TM2_EE_PD(1);
static struct meson_ee_pwrc_top_domain tm2_pwrc_hcodec = TM2_EE_PD(0);
static struct meson_ee_pwrc_top_domain tm2_pwrc_hevc = TM2_EE_PD(2);
static struct meson_ee_pwrc_top_domain tm2_pwrc_wave420l = TM2_EE_PD(3);
static struct meson_ee_pwrc_top_domain tm2_pwrc_pcieb = TM2_EE_PD(20);
static struct meson_ee_pwrc_top_domain tm2_pwrc_dspa = TM2_EE_PD(21);
static struct meson_ee_pwrc_top_domain tm2_pwrc_dspb = TM2_EE_PD(22);
static struct meson_ee_pwrc_top_domain tm2_pwrc_demod = TM2_EE_PD(23);

/* Memory PD Domains */

#define VPU_MEMPD(__reg)					\
	{ __reg, GENMASK(1, 0) },				\
	{ __reg, GENMASK(3, 2) },				\
	{ __reg, GENMASK(5, 4) },				\
	{ __reg, GENMASK(7, 6) },				\
	{ __reg, GENMASK(9, 8) },				\
	{ __reg, GENMASK(11, 10) },				\
	{ __reg, GENMASK(13, 12) },				\
	{ __reg, GENMASK(15, 14) },				\
	{ __reg, GENMASK(17, 16) },				\
	{ __reg, GENMASK(19, 18) },				\
	{ __reg, GENMASK(21, 20) },				\
	{ __reg, GENMASK(23, 22) },				\
	{ __reg, GENMASK(25, 24) },				\
	{ __reg, GENMASK(27, 26) },				\
	{ __reg, GENMASK(29, 28) },				\
	{ __reg, GENMASK(31, 30) }

#define VPU_HHI_MEMPD(__reg)					\
	{ __reg, BIT(8) },					\
	{ __reg, BIT(9) },					\
	{ __reg, BIT(10) },					\
	{ __reg, BIT(11) },					\
	{ __reg, BIT(12) },					\
	{ __reg, BIT(13) },					\
	{ __reg, BIT(14) },					\
	{ __reg, BIT(15) }

static struct meson_ee_pwrc_mem_domain g12a_pwrc_mem_vpu[] = {
	VPU_MEMPD(HHI_VPU_MEM_PD_REG0),
	VPU_MEMPD(HHI_VPU_MEM_PD_REG1),
	VPU_MEMPD(HHI_VPU_MEM_PD_REG2),
	VPU_HHI_MEMPD(HHI_MEM_PD_REG0),
};

static struct meson_ee_pwrc_mem_domain g12a_pwrc_mem_eth[] = {
	{ HHI_MEM_PD_REG0, GENMASK(3, 2) },
};

static struct meson_ee_pwrc_mem_domain sm1_pwrc_mem_vpu[] = {
	VPU_MEMPD(HHI_VPU_MEM_PD_REG0),
	VPU_MEMPD(HHI_VPU_MEM_PD_REG1),
	VPU_MEMPD(HHI_VPU_MEM_PD_REG2),
	VPU_MEMPD(HHI_VPU_MEM_PD_REG3),
	{ HHI_VPU_MEM_PD_REG4, GENMASK(1, 0) },
	{ HHI_VPU_MEM_PD_REG4, GENMASK(3, 2) },
	{ HHI_VPU_MEM_PD_REG4, GENMASK(5, 4) },
	{ HHI_VPU_MEM_PD_REG4, GENMASK(7, 6) },
	VPU_HHI_MEMPD(HHI_MEM_PD_REG0),
};

static struct meson_ee_pwrc_mem_domain sm1_pwrc_mem_nna[] = {
	{ HHI_NANOQ_MEM_PD_REG0, 0xff },
	{ HHI_NANOQ_MEM_PD_REG1, 0xff },
};

static struct meson_ee_pwrc_mem_domain sm1_pwrc_mem_usb[] = {
	{ HHI_MEM_PD_REG0, GENMASK(31, 30) },
};

static struct meson_ee_pwrc_mem_domain sm1_pwrc_mem_pcie[] = {
	{ HHI_MEM_PD_REG0, GENMASK(29, 26) },
};

static struct meson_ee_pwrc_mem_domain sm1_pwrc_mem_ge2d[] = {
	{ HHI_MEM_PD_REG0, GENMASK(25, 18) },
};

static struct meson_ee_pwrc_mem_domain sm1_pwrc_mem_audio[] = {
	{ HHI_MEM_PD_REG0, GENMASK(5, 4) },
	{ HHI_AUDIO_MEM_PD_REG0, GENMASK(1, 0) },
	{ HHI_AUDIO_MEM_PD_REG0, GENMASK(3, 2) },
	{ HHI_AUDIO_MEM_PD_REG0, GENMASK(5, 4) },
	{ HHI_AUDIO_MEM_PD_REG0, GENMASK(7, 6) },
	{ HHI_AUDIO_MEM_PD_REG0, GENMASK(13, 12) },
	{ HHI_AUDIO_MEM_PD_REG0, GENMASK(15, 14) },
	{ HHI_AUDIO_MEM_PD_REG0, GENMASK(17, 16) },
	{ HHI_AUDIO_MEM_PD_REG0, GENMASK(19, 18) },
	{ HHI_AUDIO_MEM_PD_REG0, GENMASK(21, 20) },
	{ HHI_AUDIO_MEM_PD_REG0, GENMASK(23, 22) },
	{ HHI_AUDIO_MEM_PD_REG0, GENMASK(25, 24) },
	{ HHI_AUDIO_MEM_PD_REG0, GENMASK(27, 26) },
};

static struct meson_ee_pwrc_mem_domain sm1_pwrc_mem_vdec[] = {
	{ DOS_MEM_PD_VDEC, GENMASK(31, 0) },
};

static struct meson_ee_pwrc_mem_domain sm1_pwrc_mem_hcodec[] = {
	{ DOS_MEM_PD_HCODEC, GENMASK(31, 0) },
};

static struct meson_ee_pwrc_mem_domain sm1_pwrc_mem_hevc[] = {
	{ DOS_MEM_PD_HEVC, GENMASK(31, 0) },
};

static struct meson_ee_pwrc_mem_domain sm1_pwrc_mem_wave420l[] = {
	{ DOS_MEM_PD_WAVE420L, GENMASK(31, 0) },
};

static struct meson_ee_pwrc_mem_domain tm2_pwrc_mem_vpu[] = {
	VPU_MEMPD(HHI_VPU_MEM_PD_REG0),
	VPU_MEMPD(HHI_VPU_MEM_PD_REG1),
	VPU_MEMPD(HHI_VPU_MEM_PD_REG2),
	VPU_MEMPD(TM2_HHI_VPU_MEM_PD_REG3),
	VPU_MEMPD(TM2_HHI_VPU_MEM_PD_REG4),
	VPU_HHI_MEMPD(HHI_MEM_PD_REG0),
};

static struct meson_ee_pwrc_mem_domain tm2_pwrc_mem_nna[] = {
	{ HHI_NANOQ_MEM_PD_REG0, GENMASK(31, 0) },
	{ HHI_NANOQ_MEM_PD_REG1, GENMASK(31, 0) },
};

static struct meson_ee_pwrc_mem_domain tm2_pwrc_mem_usb[] = {
	{ HHI_MEM_PD_REG0, GENMASK(31, 30) },
};

static struct meson_ee_pwrc_mem_domain tm2_pwrc_mem_pciea[] = {
	{ HHI_MEM_PD_REG0, GENMASK(29, 26) },
};

static struct meson_ee_pwrc_mem_domain tm2_pwrc_mem_ge2d[] = {
	{ HHI_MEM_PD_REG0, GENMASK(25, 18) },
};

static struct meson_ee_pwrc_mem_domain tm2_pwrc_mem_audio[] = {
	{ HHI_MEM_PD_REG0, GENMASK(5, 4) },
	{ HHI_AUDIO_MEM_PD_REG0, GENMASK(1, 0) },
	{ HHI_AUDIO_MEM_PD_REG0, GENMASK(3, 2) },
	{ HHI_AUDIO_MEM_PD_REG0, GENMASK(5, 4) },
	{ HHI_AUDIO_MEM_PD_REG0, GENMASK(7, 6) },
	{ HHI_AUDIO_MEM_PD_REG0, GENMASK(13, 12) },
	{ HHI_AUDIO_MEM_PD_REG0, GENMASK(15, 14) },
	{ HHI_AUDIO_MEM_PD_REG0, GENMASK(17, 16) },
	{ HHI_AUDIO_MEM_PD_REG0, GENMASK(19, 18) },
	{ HHI_AUDIO_MEM_PD_REG0, GENMASK(21, 20) },
	{ HHI_AUDIO_MEM_PD_REG0, GENMASK(23, 22) },
	{ HHI_AUDIO_MEM_PD_REG0, GENMASK(25, 24) },
	{ HHI_AUDIO_MEM_PD_REG0, GENMASK(27, 26) },
};

static struct meson_ee_pwrc_mem_domain tm2_pwrc_mem_vdec[] = {
	{ DOS_MEM_PD_VDEC, GENMASK(31, 0) },
};

static struct meson_ee_pwrc_mem_domain tm2_pwrc_mem_hcodec[] = {
	{ DOS_MEM_PD_HCODEC, GENMASK(31, 0) },
};

static struct meson_ee_pwrc_mem_domain tm2_pwrc_mem_hevc[] = {
	{ DOS_MEM_PD_HEVC, GENMASK(31, 0) },
};

static struct meson_ee_pwrc_mem_domain tm2_pwrc_mem_wave420l[] = {
	{ DOS_MEM_PD_WAVE420L, GENMASK(31, 0) },
};

static struct meson_ee_pwrc_mem_domain tm2_pwrc_mem_pcieb[] = {
	{ HHI_MEM_PD_REG0, GENMASK(7, 4) },
};

static struct meson_ee_pwrc_mem_domain tm2_pwrc_mem_dspa[] = {
	{ HHI_DSP_MEM_PD_REG0, GENMASK(15, 0) },
};

static struct meson_ee_pwrc_mem_domain tm2_pwrc_mem_dspb[] = {
	{ HHI_DSP_MEM_PD_REG0, GENMASK(31, 16) },
};

static struct meson_ee_pwrc_mem_domain tm2_pwrc_mem_demod[] = {
	{ HHI_DEMOD_MEM_PD_REG, GENMASK(11, 0) },
	{ HHI_DEMOD_MEM_PD_REG, BIT(13) },
};

#define VPU_PD(__name, __top_pd, __mem, __get_power, __resets,		\
		__clks, __dom_id)					\
	{								\
		.name = __name,						\
		.reset_names_count = __resets,				\
		.clk_names_count = __clks,				\
		.domain_id = __dom_id,					\
		.top_pd = __top_pd,					\
		.mem_pd_count = ARRAY_SIZE(__mem),			\
		.mem_pd = __mem,					\
		.get_power = __get_power,				\
	}

#define TOP_PD(__name, __top_pd, __mem, __get_power, __resets,		\
		__dom_id, __flag)					\
	{								\
		.name = __name,						\
		.reset_names_count = __resets,				\
		.domain_id = __dom_id,					\
		.top_pd = __top_pd,					\
		.mem_pd_count = ARRAY_SIZE(__mem),			\
		.mem_pd = __mem,					\
		.flags = __flag,					\
		.get_power = __get_power,				\
	}

#define MEM_PD(__name, __mem, __dom_id, __flag)				\
	TOP_PD(__name, NULL, __mem, NULL, 0, __dom_id, __flag)

static bool pwrc_ee_get_power(struct meson_ee_pwrc_domain *pwrc_domain);

static struct meson_ee_pwrc_domain_desc g12a_pwrc_domains[] = {
	[PWRC_G12A_VPU_ID]  = VPU_PD("VPU", &g12a_pwrc_vpu, g12a_pwrc_mem_vpu,
				     pwrc_ee_get_power, 11, 2,
				     PWRC_G12A_VPU_ID),
	[PWRC_G12A_ETH_ID] = MEM_PD("ETH", g12a_pwrc_mem_eth, PWRC_G12A_ETH_ID,
					0),
};

static struct meson_ee_pwrc_domain_desc sm1_pwrc_domains[] = {
	[PWRC_SM1_VPU_ID]  = VPU_PD("VPU", &sm1_pwrc_vpu, sm1_pwrc_mem_vpu,
				    pwrc_ee_get_power, 11, 2, PWRC_SM1_VPU_ID),
	[PWRC_SM1_NNA_ID]  = TOP_PD("NNA", &sm1_pwrc_nna, sm1_pwrc_mem_nna,
				    pwrc_ee_get_power, 3, PWRC_SM1_NNA_ID, 0),
	[PWRC_SM1_USB_ID]  = TOP_PD("USB", &sm1_pwrc_usb, sm1_pwrc_mem_usb,
				    pwrc_ee_get_power, 1, PWRC_SM1_USB_ID,
				    0),
	[PWRC_SM1_PCIE_ID] = TOP_PD("PCIE", &sm1_pwrc_pci, sm1_pwrc_mem_pcie,
				    pwrc_ee_get_power, 3, PWRC_SM1_PCIE_ID,
				    0),
	[PWRC_SM1_GE2D_ID] = TOP_PD("GE2D", &sm1_pwrc_ge2d, sm1_pwrc_mem_ge2d,
				    pwrc_ee_get_power, 1, PWRC_SM1_GE2D_ID, 0),
	[PWRC_SM1_AUDIO_ID] = MEM_PD("AUDIO", sm1_pwrc_mem_audio,
				     PWRC_SM1_AUDIO_ID, 0),
	[PWRC_SM1_ETH_ID] = MEM_PD("ETH", g12a_pwrc_mem_eth, PWRC_SM1_ETH_ID,
					0),
	[PWRC_SM1_VDEC_ID] = TOP_PD("VDEC", &sm1_pwrc_vdec, sm1_pwrc_mem_vdec,
				pwrc_ee_get_power, 0, PWRC_SM1_VDEC_ID, 0),
	[PWRC_SM1_HCODEC_ID] = TOP_PD("HCODEC", &sm1_pwrc_hcodec,
				      sm1_pwrc_mem_hcodec, pwrc_ee_get_power,
				      0, PWRC_SM1_HCODEC_ID, 0),
	[PWRC_SM1_HEVC_ID] = TOP_PD("HEVC", &sm1_pwrc_hevc, sm1_pwrc_mem_hevc,
				pwrc_ee_get_power, 0, PWRC_SM1_HEVC_ID, 0),
	[PWRC_SM1_WAVE420L_ID] = TOP_PD("WAVE420L", &sm1_pwrc_wave420l,
				sm1_pwrc_mem_wave420l, pwrc_ee_get_power,
				0, PWRC_SM1_WAVE420L_ID, 0),
};

static struct meson_ee_pwrc_domain_desc tm2_pwrc_domains[] = {
	[PWRC_TM2_VPU_ID]  = VPU_PD("VPU", &tm2_pwrc_vpu, tm2_pwrc_mem_vpu,
				    pwrc_ee_get_power, 11, 2, PWRC_TM2_VPU_ID),
	[PWRC_TM2_NNA_ID]  = TOP_PD("NNA", &tm2_pwrc_nna, tm2_pwrc_mem_nna,
				    pwrc_ee_get_power, 1, PWRC_TM2_NNA_ID, 0),
	[PWRC_TM2_USB_ID]  = TOP_PD("USB", &tm2_pwrc_usb, tm2_pwrc_mem_usb,
				    pwrc_ee_get_power, 1, PWRC_TM2_USB_ID,
				    0),
	[PWRC_TM2_PCIE_ID] = TOP_PD("PCIEA", &tm2_pwrc_pciea, tm2_pwrc_mem_pciea,
				    pwrc_ee_get_power, 3, PWRC_TM2_PCIE_ID,
				    0),
	[PWRC_TM2_GE2D_ID] = TOP_PD("GE2D", &tm2_pwrc_ge2d, tm2_pwrc_mem_ge2d,
				    pwrc_ee_get_power, 1, PWRC_TM2_GE2D_ID, 0),
	[PWRC_TM2_AUDIO_ID] = MEM_PD("AUDIO", tm2_pwrc_mem_audio,
				     PWRC_TM2_AUDIO_ID, 0),
	[PWRC_TM2_ETH_ID] = MEM_PD("ETH", g12a_pwrc_mem_eth, PWRC_TM2_ETH_ID,
					0),
	[PWRC_TM2_VDEC_ID] = TOP_PD("VDEC", &tm2_pwrc_vdec, tm2_pwrc_mem_vdec,
				pwrc_ee_get_power, 0, PWRC_TM2_VDEC_ID, 0),
	[PWRC_TM2_HCODEC_ID] = TOP_PD("HCODEC", &tm2_pwrc_hcodec,
				      tm2_pwrc_mem_hcodec, pwrc_ee_get_power,
				      0, PWRC_TM2_HCODEC_ID, 0),
	[PWRC_TM2_HEVC_ID] = TOP_PD("HEVC", &tm2_pwrc_hevc, tm2_pwrc_mem_hevc,
				pwrc_ee_get_power, 0, PWRC_TM2_HEVC_ID, 0),
	[PWRC_TM2_WAVE420L_ID] = TOP_PD("WAVE420L", &tm2_pwrc_wave420l,
				tm2_pwrc_mem_wave420l, pwrc_ee_get_power,
				0, PWRC_TM2_WAVE420L_ID, 0),
	[PWRC_TM2_PCIE1_ID] = TOP_PD("PCIEB", &tm2_pwrc_pcieb,
				tm2_pwrc_mem_pcieb, pwrc_ee_get_power,
				3, PWRC_TM2_PCIE1_ID, 0),
	[PWRC_TM2_DSPA_ID] = TOP_PD("DSPA", &tm2_pwrc_dspa,
				tm2_pwrc_mem_dspa, pwrc_ee_get_power,
				2, PWRC_TM2_DSPA_ID, 0),
	[PWRC_TM2_DSPB_ID] = TOP_PD("DSPB", &tm2_pwrc_dspb,
				tm2_pwrc_mem_dspb, pwrc_ee_get_power,
				2, PWRC_TM2_DSPB_ID, 0),
	[PWRC_TM2_DEMOD_ID] = TOP_PD("DEMOD", &tm2_pwrc_demod,
				tm2_pwrc_mem_demod, pwrc_ee_get_power,
				1, PWRC_TM2_DEMOD_ID, 0),
};

static const struct regmap_config dos_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

struct meson_ee_pwrc_domain {
	struct generic_pm_domain base;
	bool enabled;
	struct meson_ee_pwrc *pwrc;
	struct meson_ee_pwrc_domain_desc desc;
	struct clk_bulk_data *clks;
	int num_clks;
	struct reset_control *rstc;
	int num_rstc;
};

struct meson_ee_pwrc {
	struct regmap *regmap_ao;
	struct regmap *regmap_hhi;
	struct regmap *regmap_dos;
	struct meson_ee_pwrc_domain *domains;
	struct genpd_onecell_data xlate;
};

static bool pwrc_ee_get_power(struct meson_ee_pwrc_domain *pwrc_domain)
{
	u32 reg;

	regmap_read(pwrc_domain->pwrc->regmap_ao,
		    pwrc_domain->desc.top_pd->sleep_reg, &reg);

	return (reg & pwrc_domain->desc.top_pd->sleep_mask);
}

static int meson_ee_pwrc_off(struct generic_pm_domain *domain)
{
	struct meson_ee_pwrc_domain *pwrc_domain =
		container_of(domain, struct meson_ee_pwrc_domain, base);
	int i, ret;

	if (!strcmp(pwrc_domain->desc.name, "USB") ||
	    !strcmp(pwrc_domain->desc.name, "PCIE") ||
	    !strcmp(pwrc_domain->desc.name, "PCIEA") ||
	    !strcmp(pwrc_domain->desc.name, "PCIEB"))
		return 0;

	ret = reset_control_deassert(pwrc_domain->rstc);
	if (ret)
		return ret;

	ret = reset_control_assert(pwrc_domain->rstc);
	if (ret)
		return ret;

	if (pwrc_domain->desc.top_pd)
		regmap_update_bits(pwrc_domain->pwrc->regmap_ao,
				   pwrc_domain->desc.top_pd->iso_reg,
				   pwrc_domain->desc.top_pd->iso_mask,
				   pwrc_domain->desc.top_pd->iso_mask);
	udelay(20);

	if (pwrc_domain->desc.domain_id >= PWRC_SM1_VDEC_ID) {
		for (i = 0 ; i < pwrc_domain->desc.mem_pd_count ; ++i)
			regmap_update_bits(pwrc_domain->pwrc->regmap_dos,
					   pwrc_domain->desc.mem_pd[i].reg,
					   pwrc_domain->desc.mem_pd[i].mask,
					   pwrc_domain->desc.mem_pd[i].mask);
	} else {
		for (i = 0 ; i < pwrc_domain->desc.mem_pd_count ; ++i)
			regmap_update_bits(pwrc_domain->pwrc->regmap_hhi,
					   pwrc_domain->desc.mem_pd[i].reg,
					   pwrc_domain->desc.mem_pd[i].mask,
					   pwrc_domain->desc.mem_pd[i].mask);
	}
	udelay(20);

	if (pwrc_domain->desc.top_pd)
		regmap_update_bits(pwrc_domain->pwrc->regmap_ao,
				   pwrc_domain->desc.top_pd->sleep_reg,
				   pwrc_domain->desc.top_pd->sleep_mask,
				   pwrc_domain->desc.top_pd->sleep_mask);

	if (pwrc_domain->num_clks) {
		msleep(20);
		clk_bulk_disable_unprepare(pwrc_domain->num_clks,
					   pwrc_domain->clks);
	}

	return 0;
}

static int meson_ee_pwrc_on(struct generic_pm_domain *domain)
{
	struct meson_ee_pwrc_domain *pwrc_domain =
		container_of(domain, struct meson_ee_pwrc_domain, base);
	int i, ret;

	if (!strcmp(pwrc_domain->desc.name, "USB") ||
	    !strcmp(pwrc_domain->desc.name, "PCIE") ||
	    !strcmp(pwrc_domain->desc.name, "PCIEA") ||
	    !strcmp(pwrc_domain->desc.name, "PCIEB")) {
		if (!pwrc_domain->desc.get_power(pwrc_domain))
			return 0;
	}

	ret = reset_control_deassert(pwrc_domain->rstc);
	if (ret)
		return ret;

	if (pwrc_domain->desc.top_pd)
		regmap_update_bits(pwrc_domain->pwrc->regmap_ao,
				   pwrc_domain->desc.top_pd->sleep_reg,
				   pwrc_domain->desc.top_pd->sleep_mask, 0);
	udelay(20);

	if (pwrc_domain->desc.domain_id >= PWRC_SM1_VDEC_ID) {
		for (i = 0 ; i < pwrc_domain->desc.mem_pd_count ; ++i)
			regmap_update_bits(pwrc_domain->pwrc->regmap_dos,
					   pwrc_domain->desc.mem_pd[i].reg,
					   pwrc_domain->desc.mem_pd[i].mask, 0);
	} else {
		for (i = 0 ; i < pwrc_domain->desc.mem_pd_count ; ++i)
			regmap_update_bits(pwrc_domain->pwrc->regmap_hhi,
					   pwrc_domain->desc.mem_pd[i].reg,
					   pwrc_domain->desc.mem_pd[i].mask, 0);
	}

	udelay(20);

	ret = reset_control_assert(pwrc_domain->rstc);
	if (ret)
		return ret;

	if (pwrc_domain->desc.top_pd)
		regmap_update_bits(pwrc_domain->pwrc->regmap_ao,
				   pwrc_domain->desc.top_pd->iso_reg,
				   pwrc_domain->desc.top_pd->iso_mask, 0);

	ret = reset_control_deassert(pwrc_domain->rstc);
	if (ret)
		return ret;

	return clk_bulk_prepare_enable(pwrc_domain->num_clks,
				       pwrc_domain->clks);
}

static int meson_ee_pwrc_init_domain(struct platform_device *pdev,
				     struct meson_ee_pwrc *pwrc,
				     struct meson_ee_pwrc_domain *dom)
{
	struct device_node *np = NULL;
	int ret;
	char buf[16] = {0};
	int i;

	dom->pwrc = pwrc;
	dom->num_rstc = dom->desc.reset_names_count;
	dom->num_clks = dom->desc.clk_names_count;

	if (dom->num_rstc) {
		for (i = 0; i < strlen(dom->desc.name); i++)
			buf[i] = tolower(*(dom->desc.name + i));

		sprintf(buf, "%s,%s", buf, "reset");
		np = of_parse_phandle(pdev->dev.of_node, buf, 0);

		dom->rstc = of_reset_control_array_get(np, true, false, true);
		if (IS_ERR(dom->rstc))
			return PTR_ERR(dom->rstc);
	}

	if (dom->num_clks) {
		int ret = devm_clk_bulk_get_all(&pdev->dev, &dom->clks);
		if (ret < 0)
			return ret;

		if (dom->num_clks != ret) {
			dev_warn(&pdev->dev, "Invalid clocks count %d for domain %s\n",
				 ret, dom->desc.name);
			dom->num_clks = ret;
		}
	}

	dom->base.name = dom->desc.name;
	dom->base.power_on = meson_ee_pwrc_on;
	dom->base.power_off = meson_ee_pwrc_off;
	dom->base.flags = dom->desc.flags;

	/*
         * TOFIX: This is a special case for the VPU power domain, which can
	 * be enabled previously by the bootloader. In this case the VPU
         * pipeline may be functional but no driver maybe never attach
         * to this power domain, and if the domain is disabled it could
         * cause system errors. This is why the pm_domain_always_on_gov
         * is used here.
         * For the same reason, the clocks should be enabled in case
         * we need to power the domain off, otherwise the internal clocks
         * prepare/enable counters won't be in sync.
         */
	if (dom->num_clks && dom->desc.get_power && !dom->desc.get_power(dom)) {
		ret = clk_bulk_prepare_enable(dom->num_clks, dom->clks);
		if (ret)
			return ret;

		ret = pm_genpd_init(&dom->base, &pm_domain_always_on_gov,
				    false);
		if (ret)
			return ret;
	} else {
		ret = pm_genpd_init(&dom->base, NULL,
				    (dom->desc.get_power ?
				     dom->desc.get_power(dom) : true));
		if (ret)
			return ret;
	}

	return 0;
}

static int meson_ee_pwrc_probe(struct platform_device *pdev)
{
	const struct meson_ee_pwrc_domain_data *match;
	struct regmap *regmap_ao, *regmap_hhi, *regmap_dos;
	struct meson_ee_pwrc *pwrc;
	struct resource *dos_res;
	void __iomem *base;
	int i, ret;

	match = of_device_get_match_data(&pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "failed to get match data\n");
		return -ENODEV;
	}

	pwrc = devm_kzalloc(&pdev->dev, sizeof(*pwrc), GFP_KERNEL);
	if (!pwrc)
		return -ENOMEM;

	pwrc->xlate.domains = devm_kcalloc(&pdev->dev, match->count,
					   sizeof(*pwrc->xlate.domains),
					   GFP_KERNEL);
	if (!pwrc->xlate.domains)
		return -ENOMEM;

	pwrc->domains = devm_kcalloc(&pdev->dev, match->count,
				     sizeof(*pwrc->domains), GFP_KERNEL);
	if (!pwrc->domains)
		return -ENOMEM;

	pwrc->xlate.num_domains = match->count;

	dos_res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					       "dos");
	if (!dos_res)
		return -ENOMEM;

	base = devm_ioremap_resource(&pdev->dev, dos_res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	regmap_dos = devm_regmap_init_mmio(&pdev->dev, base,
					   &dos_regmap_config);
	if (IS_ERR(regmap_dos))
		return PTR_ERR(regmap_dos);

	regmap_hhi = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						     "amlogic,hhi-sysctrl");
	if (IS_ERR(regmap_hhi)) {
		dev_err(&pdev->dev, "failed to get HHI regmap\n");
		return PTR_ERR(regmap_hhi);
	}

	regmap_ao = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						    "amlogic,ao-sysctrl");
	if (IS_ERR(regmap_ao)) {
		dev_err(&pdev->dev, "failed to get AO regmap\n");
		return PTR_ERR(regmap_ao);
	}

	pwrc->regmap_ao = regmap_ao;
	pwrc->regmap_hhi = regmap_hhi;
	pwrc->regmap_dos = regmap_dos;

	platform_set_drvdata(pdev, pwrc);
	for (i = 0 ; i < match->count ; ++i) {
		struct meson_ee_pwrc_domain *dom = &pwrc->domains[i];

		memcpy(&dom->desc, &match->domains[i], sizeof(dom->desc));

		ret = meson_ee_pwrc_init_domain(pdev, pwrc, dom);
		if (ret)
			return ret;

		pwrc->xlate.domains[i] = &dom->base;
	}

	return of_genpd_add_provider_onecell(pdev->dev.of_node, &pwrc->xlate);
}

static void meson_ee_pwrc_shutdown(struct platform_device *pdev)
{
#if 0
	struct meson_ee_pwrc *pwrc = platform_get_drvdata(pdev);
	int i;

	for (i = 0 ; i < pwrc->xlate.num_domains ; ++i) {
		struct meson_ee_pwrc_domain *dom = &pwrc->domains[i];

		if (dom->desc.get_power && !dom->desc.get_power(dom))
			meson_ee_pwrc_off(&dom->base);
	}
#endif
}

static struct meson_ee_pwrc_domain_data meson_ee_g12a_pwrc_data = {
	.count = ARRAY_SIZE(g12a_pwrc_domains),
	.domains = g12a_pwrc_domains,
};

static struct meson_ee_pwrc_domain_data meson_ee_sm1_pwrc_data = {
	.count = ARRAY_SIZE(sm1_pwrc_domains),
	.domains = sm1_pwrc_domains,
};

static struct meson_ee_pwrc_domain_data meson_ee_tm2_pwrc_data = {
	.count = ARRAY_SIZE(tm2_pwrc_domains),
	.domains = tm2_pwrc_domains,
};

static const struct of_device_id meson_ee_pwrc_match_table[] = {
	{
		.compatible = "amlogic,meson-g12a-pwrc",
		.data = &meson_ee_g12a_pwrc_data,
	},
	{
		.compatible = "amlogic,meson-sm1-pwrc",
		.data = &meson_ee_sm1_pwrc_data,
	},
	{
		.compatible = "amlogic,meson-tm2-pwrc",
		.data = &meson_ee_tm2_pwrc_data,
	},
	{ /* sentinel */ }
};

static struct platform_driver meson_ee_pwrc_driver = {
	.probe = meson_ee_pwrc_probe,
	.shutdown = meson_ee_pwrc_shutdown,
	.driver = {
		.name		= "meson_ee_pwrc",
		.of_match_table	= meson_ee_pwrc_match_table,
	},
};
builtin_platform_driver(meson_ee_pwrc_driver);
