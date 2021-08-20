/*
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#define DEBUG
#include "vdec_power_ctrl.h"
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/power_ctrl.h>
//#include <dt-bindings/power/sc2-pd.h>
//#include <linux/amlogic/pwr_ctrl.h>
#include <linux/amlogic/power_domain.h>
#include <dt-bindings/power/sc2-pd.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include "../../../common/media_clock/switch/amports_gate.h"
#include "../../../common/chips/decoder_cpu_ver_info.h"
#include "../../../common/media_clock/clk/clk.h"

#define HEVC_TEST_LIMIT		(100)
#define GXBB_REV_A_MINOR	(0xa)

#define PDID_DSP            0
#define PDID_DOS_HCODEC             1
#define PDID_DOS_HEVC               2
#define PDID_DOS_VDEC               3
#define PDID_DOS_WAVE               4

extern int no_powerdown;
extern int hevc_max_reset_count;

struct pm_name_s {
	int type;
	const char *name;
};

static const struct pm_name_s pm_name[] = {
	{PM_POWER_CTRL_RW_REG,		"legacy"},
	{PM_POWER_CTRL_API,		"power-ctrl-api"},
	{PM_POWER_DOMAIN,		"power-domain"},
	{PM_POWER_DOMAIN_SEC_API,	"pd-sec-api"},
	{PM_POWER_DOMAIN_NONSEC_API,	"pd-non-sec-api"},
};

const char *get_pm_name(int type)
{
	const char *name = "unknown";
	int i, size = ARRAY_SIZE(pm_name);

	for (i = 0; i < size; i++) {
		if (type == pm_name[i].type)
			name = pm_name[i].name;
	}

	return name;
}
EXPORT_SYMBOL(get_pm_name);

static struct pm_pd_s pm_domain_data[] = {
	{ .name = "pwrc-vdec", },
	{ .name = "pwrc-hcodec",},
	{ .name = "pwrc-vdec-2", },
	{ .name = "pwrc-hevc", },
	{ .name = "pwrc-hevc-b", },
	{ .name = "pwrc-wave", },
};

static void pm_vdec_power_switch(struct pm_pd_s *pd, int id, bool on)
{
	struct device *dev = pd[id].dev;

	if (on)
		pm_runtime_get_sync(dev);
	else
		pm_runtime_put_sync(dev);

	pr_debug("the %-15s power %s\n",
		pd[id].name, on ? "on" : "off");
}

static int pm_vdec_power_domain_init(struct device *dev)
{
	int i, err;
	const struct power_manager_s *pm = of_device_get_match_data(dev);
	struct pm_pd_s *pd = pm->pd_data;

	for (i = 0; i < ARRAY_SIZE(pm_domain_data); i++) {
		pd[i].dev = dev_pm_domain_attach_by_name(dev, pd[i].name);
		if (IS_ERR_OR_NULL(pd[i].dev)) {
			err = PTR_ERR(pd[i].dev);
			dev_err(dev, "Get %s failed, pm-domain: %d\n",
				pd[i].name, err);
			continue;
		}

		pd[i].link = device_link_add(dev, pd[i].dev,
					     DL_FLAG_PM_RUNTIME |
					     DL_FLAG_STATELESS);
		if (IS_ERR_OR_NULL(pd[i].link)) {
			dev_err(dev, "Adding %s device link failed!\n",
				pd[i].name);
			return -ENODEV;
		}

		pr_debug("power domain: name: %s, dev: %px, link: %px\n",
			pd[i].name, pd[i].dev, pd[i].link);
	}

	return 0;
}

static void pm_vdec_power_domain_relese(struct device *dev)
{
	int i;
	const struct power_manager_s *pm = of_device_get_match_data(dev);
	struct pm_pd_s *pd = pm->pd_data;

	for (i = 0; i < ARRAY_SIZE(pm_domain_data); i++) {
		if (!IS_ERR_OR_NULL(pd[i].link))
			device_link_del(pd[i].link);

		if (!IS_ERR_OR_NULL(pd[i].dev))
			dev_pm_domain_detach(pd[i].dev, true);
	}
}

static void pm_vdec_clock_on(int id)
{
	if (id == VDEC_1) {
		amports_switch_gate("clk_vdec_mux", 1);
		vdec_clock_hi_enable();
	} else if (id == VDEC_HCODEC) {
		hcodec_clock_enable();
	} else if (id == VDEC_HEVC) {
		/* enable hevc clock */
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SC2 &&
			(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_T5) &&
			(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_T5D))
			amports_switch_gate("clk_hevcf_mux", 1);
		else
			amports_switch_gate("clk_hevc_mux", 1);
		if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A) &&
			!is_hevc_front_back_clk_combined())
			amports_switch_gate("clk_hevcb_mux", 1);

		hevc_clock_hi_enable();

		if (!is_hevc_front_back_clk_combined())
			hevc_back_clock_hi_enable();
	}
}

static void pm_vdec_clock_off(int id)
{
	if (id == VDEC_1) {
		vdec_clock_off();
	} else if (id == VDEC_HCODEC) {
		hcodec_clock_off();
	} else if (id == VDEC_HEVC) {
		/* disable hevc clock */
		hevc_clock_off();
		if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A) &&
			!is_hevc_front_back_clk_combined())
			hevc_back_clock_off();
	}
}

static void pm_vdec_power_domain_power_on(struct device *dev, int id)
{
	const struct power_manager_s *pm = of_device_get_match_data(dev);

	pm_vdec_clock_on(id);
	pm_vdec_power_switch(pm->pd_data, id, true);
}

static void pm_vdec_power_domain_power_off(struct device *dev, int id)
{
	const struct power_manager_s *pm = of_device_get_match_data(dev);

	pm_vdec_clock_off(id);
	pm_vdec_power_switch(pm->pd_data, id, false);
}

static bool pm_vdec_power_domain_power_state(struct device *dev, int id)
{
	const struct power_manager_s *pm = of_device_get_match_data(dev);

	return pm_runtime_active(pm->pd_data[id].dev);
}

static bool test_hevc(u32 decomp_addr, u32 us_delay)
{
	int i;

	/* SW_RESET IPP */
	WRITE_VREG(HEVCD_IPP_TOP_CNTL, 1);
	WRITE_VREG(HEVCD_IPP_TOP_CNTL, 0);

	/* initialize all canvas table */
	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, 0);
	for (i = 0; i < 32; i++)
		WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CMD_ADDR,
			0x1 | (i << 8) | decomp_addr);
	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, 1);
	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR, (0 << 8) | (0<<1) | 1);
	for (i = 0; i < 32; i++)
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR, 0);

	/* Initialize mcrcc */
	WRITE_VREG(HEVCD_MCRCC_CTL1, 0x2);
	WRITE_VREG(HEVCD_MCRCC_CTL2, 0x0);
	WRITE_VREG(HEVCD_MCRCC_CTL3, 0x0);
	WRITE_VREG(HEVCD_MCRCC_CTL1, 0xff0);

	/* Decomp initialize */
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, 0x0);
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL2, 0x0);

	/* Frame level initialization */
	WRITE_VREG(HEVCD_IPP_TOP_FRMCONFIG, 0x100 | (0x100 << 16));
	WRITE_VREG(HEVCD_IPP_TOP_TILECONFIG3, 0x0);
	WRITE_VREG(HEVCD_IPP_TOP_LCUCONFIG, 0x1 << 5);
	WRITE_VREG(HEVCD_IPP_BITDEPTH_CONFIG, 0x2 | (0x2 << 2));

	WRITE_VREG(HEVCD_IPP_CONFIG, 0x0);
	WRITE_VREG(HEVCD_IPP_LINEBUFF_BASE, 0x0);

	/* Enable SWIMP mode */
	WRITE_VREG(HEVCD_IPP_SWMPREDIF_CONFIG, 0x1);

	/* Enable frame */
	WRITE_VREG(HEVCD_IPP_TOP_CNTL, 0x2);
	WRITE_VREG(HEVCD_IPP_TOP_FRMCTL, 0x1);

	/* Send SW-command CTB info */
	WRITE_VREG(HEVCD_IPP_SWMPREDIF_CTBINFO, 0x1 << 31);

	/* Send PU_command */
	WRITE_VREG(HEVCD_IPP_SWMPREDIF_PUINFO0, (0x4 << 9) | (0x4 << 16));
	WRITE_VREG(HEVCD_IPP_SWMPREDIF_PUINFO1, 0x1 << 3);
	WRITE_VREG(HEVCD_IPP_SWMPREDIF_PUINFO2, 0x0);
	WRITE_VREG(HEVCD_IPP_SWMPREDIF_PUINFO3, 0x0);

	udelay(us_delay);

	WRITE_VREG(HEVCD_IPP_DBG_SEL, 0x2 << 4);

	return (READ_VREG(HEVCD_IPP_DBG_DATA) & 3) == 1;
}

static bool hevc_workaround_needed(void)
{
	return (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_GXBB) &&
		(get_meson_cpu_version(MESON_CPU_VERSION_LVL_MINOR)
			== GXBB_REV_A_MINOR);
}

static void pm_vdec_legacy_power_off(struct device *dev, int id);

static void pm_vdec_legacy_power_on(struct device *dev, int id)
{
	void *decomp_addr = NULL;
	ulong decomp_dma_addr;
	ulong mem_handle;
	u32 decomp_addr_aligned = 0;
	int hevc_loop = 0;
	int sleep_val, iso_val;
	bool is_power_ctrl_ver2 = false;

	is_power_ctrl_ver2 =
		((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) &&
		(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_TL1)) ? true : false;

	if (hevc_workaround_needed() &&
		(id == VDEC_HEVC)) {
		decomp_addr = codec_mm_dma_alloc_coherent(&mem_handle,
			&decomp_dma_addr, SZ_64K + SZ_4K, "vdec_prealloc");
		if (decomp_addr) {
			decomp_addr_aligned = ALIGN(decomp_dma_addr, SZ_64K);
			memset((u8 *)decomp_addr +
				(decomp_addr_aligned - decomp_dma_addr),
				0xff, SZ_4K);
		} else
			pr_err("vdec: alloc HEVC gxbb decomp buffer failed.\n");
	}

	if (id == VDEC_1) {
		sleep_val = is_power_ctrl_ver2 ? 0x2 : 0xc;
		iso_val = is_power_ctrl_ver2 ? 0x2 : 0xc0;

		/* vdec1 power on */
#ifdef CONFIG_AMLOGIC_POWER
		if (is_support_power_ctrl()) {
			if (power_ctrl_sleep_mask(true, sleep_val, 0)) {
				pr_err("vdec-1 power on ctrl sleep fail.\n");
				return;
			}
		} else {
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
				READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & ~sleep_val);
		}
#else
		WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
			READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & ~sleep_val);
#endif
		/* wait 10uS */
		udelay(10);
		/* vdec1 soft reset */
		WRITE_VREG(DOS_SW_RESET0, 0xfffffffc);
		WRITE_VREG(DOS_SW_RESET0, 0);
		/* enable vdec1 clock */
		/*
		 *add power on vdec clock level setting,only for m8 chip,
		 * m8baby and m8m2 can dynamic adjust vdec clock,
		 * power on with default clock level
		 */
		amports_switch_gate("clk_vdec_mux", 1);
		vdec_clock_hi_enable();
		/* power up vdec memories */
		WRITE_VREG(DOS_MEM_PD_VDEC, 0);

		/* remove vdec1 isolation */
#ifdef CONFIG_AMLOGIC_POWER
		if (is_support_power_ctrl()) {
			if (power_ctrl_iso_mask(true, iso_val, 0)) {
				pr_err("vdec-1 power on ctrl iso fail.\n");
				return;
			}
		} else {
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
				READ_AOREG(AO_RTI_GEN_PWR_ISO0) & ~iso_val);
		}
#else
		WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
			READ_AOREG(AO_RTI_GEN_PWR_ISO0) & ~iso_val);
#endif
		/* reset DOS top registers */
		WRITE_VREG(DOS_VDEC_MCRCC_STALL_CTRL, 0);
	} else if (id == VDEC_2) {
		if (has_vdec2()) {
			/* vdec2 power on */
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) &
					~0x30);
			/* wait 10uS */
			udelay(10);
			/* vdec2 soft reset */
			WRITE_VREG(DOS_SW_RESET2, 0xffffffff);
			WRITE_VREG(DOS_SW_RESET2, 0);
			/* enable vdec1 clock */
			vdec2_clock_hi_enable();
			/* power up vdec memories */
			WRITE_VREG(DOS_MEM_PD_VDEC2, 0);
			/* remove vdec2 isolation */
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) &
					~0x300);
			/* reset DOS top registers */
			WRITE_VREG(DOS_VDEC2_MCRCC_STALL_CTRL, 0);
		}
	} else if (id == VDEC_HCODEC) {
		if (has_hdec()) {
			sleep_val = is_power_ctrl_ver2 ? 0x1 : 0x3;
			iso_val = is_power_ctrl_ver2 ? 0x1 : 0x30;

			/* hcodec power on */
#ifdef CONFIG_AMLOGIC_POWER
			if (is_support_power_ctrl()) {
				if (power_ctrl_sleep_mask(true, sleep_val, 0)) {
					pr_err("hcodec power on ctrl sleep fail.\n");
					return;
				}
			} else {
				WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & ~sleep_val);
			}
#else
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
				READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & ~sleep_val);
#endif
			/* wait 10uS */
			udelay(10);
			/* hcodec soft reset */
			WRITE_VREG(DOS_SW_RESET1, 0xffffffff);
			WRITE_VREG(DOS_SW_RESET1, 0);
			/* enable hcodec clock */
			hcodec_clock_enable();
			/* power up hcodec memories */
			WRITE_VREG(DOS_MEM_PD_HCODEC, 0);
			/* remove hcodec isolation */
#ifdef CONFIG_AMLOGIC_POWER
			if (is_support_power_ctrl()) {
				if (power_ctrl_iso_mask(true, iso_val, 0)) {
					pr_err("hcodec power on ctrl iso fail.\n");
					return;
				}
			} else {
				WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) & ~iso_val);
			}
#else
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
				READ_AOREG(AO_RTI_GEN_PWR_ISO0) & ~iso_val);
#endif
		}
	} else if (id == VDEC_HEVC) {
		if (has_hevc_vdec()) {
			bool hevc_fixed = false;

			sleep_val = is_power_ctrl_ver2 ? 0x4 : 0xc0;
			iso_val = is_power_ctrl_ver2 ? 0x4 : 0xc00;

			while (!hevc_fixed) {
				/* hevc power on */
#ifdef CONFIG_AMLOGIC_POWER
				if (is_support_power_ctrl()) {
					if (power_ctrl_sleep_mask(true, sleep_val, 0)) {
						pr_err("hevc power on ctrl sleep fail.\n");
						return;
					}
				} else {
					WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
						READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & ~sleep_val);
				}
#else
				WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & ~sleep_val);
#endif
				/* wait 10uS */
				udelay(10);
				/* hevc soft reset */
				WRITE_VREG(DOS_SW_RESET3, 0xffffffff);
				WRITE_VREG(DOS_SW_RESET3, 0);
				/* enable hevc clock */
				amports_switch_gate("clk_hevc_mux", 1);
				if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A)
					amports_switch_gate("clk_hevcb_mux", 1);
				hevc_clock_hi_enable();
				hevc_back_clock_hi_enable();
				/* power up hevc memories */
				WRITE_VREG(DOS_MEM_PD_HEVC, 0);
				/* remove hevc isolation */
#ifdef CONFIG_AMLOGIC_POWER
				if (is_support_power_ctrl()) {
					if (power_ctrl_iso_mask(true, iso_val, 0)) {
						pr_err("hevc power on ctrl iso fail.\n");
						return;
					}
				} else {
					WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
						READ_AOREG(AO_RTI_GEN_PWR_ISO0) & ~iso_val);
				}
#else
				WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) & ~iso_val);
#endif
				if (!hevc_workaround_needed())
					break;

				if (decomp_addr)
					hevc_fixed = test_hevc(
						decomp_addr_aligned, 20);

				if (!hevc_fixed) {
					hevc_loop++;
					if (hevc_loop >= HEVC_TEST_LIMIT) {
						pr_warn("hevc power sequence over limit\n");
						pr_warn("=====================================================\n");
						pr_warn(" This chip is identified to have HW failure.\n");
						pr_warn(" Please contact sqa-platform to replace the platform.\n");
						pr_warn("=====================================================\n");

						panic("Force panic for chip detection !!!\n");

						break;
					}

					pm_vdec_legacy_power_off(NULL, VDEC_HEVC);

					mdelay(10);
				}
			}

			if (hevc_loop > hevc_max_reset_count)
				hevc_max_reset_count = hevc_loop;

			WRITE_VREG(DOS_SW_RESET3, 0xffffffff);
			udelay(10);
			WRITE_VREG(DOS_SW_RESET3, 0);
		}
	}

	if (decomp_addr)
		codec_mm_dma_free_coherent(mem_handle);
}

static void pm_vdec_legacy_power_off(struct device *dev, int id)
{
	int sleep_val, iso_val;
	bool is_power_ctrl_ver2 = false;

	is_power_ctrl_ver2 =
		((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) &&
		(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_TL1)) ? true : false;

	if (id == VDEC_1) {
		sleep_val = is_power_ctrl_ver2 ? 0x2 : 0xc;
		iso_val = is_power_ctrl_ver2 ? 0x2 : 0xc0;

		/* enable vdec1 isolation */
#ifdef CONFIG_AMLOGIC_POWER
		if (is_support_power_ctrl()) {
			if (power_ctrl_iso_mask(false, iso_val, 0)) {
				pr_err("vdec-1 power off ctrl iso fail.\n");
				return;
			}
		} else {
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
				READ_AOREG(AO_RTI_GEN_PWR_ISO0) | iso_val);
		}
#else
		WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
			READ_AOREG(AO_RTI_GEN_PWR_ISO0) | iso_val);
#endif
		/* power off vdec1 memories */
		WRITE_VREG(DOS_MEM_PD_VDEC, 0xffffffffUL);
		/* disable vdec1 clock */
		vdec_clock_off();
		/* vdec1 power off */
#ifdef CONFIG_AMLOGIC_POWER
		if (is_support_power_ctrl()) {
			if (power_ctrl_sleep_mask(false, sleep_val, 0)) {
				pr_err("vdec-1 power off ctrl sleep fail.\n");
				return;
			}
		} else {
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
				READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) | sleep_val);
		}
#else
		WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
			READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) | sleep_val);
#endif
	} else if (id == VDEC_2) {
		if (has_vdec2()) {
			/* enable vdec2 isolation */
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) |
					0x300);
			/* power off vdec2 memories */
			WRITE_VREG(DOS_MEM_PD_VDEC2, 0xffffffffUL);
			/* disable vdec2 clock */
			vdec2_clock_off();
			/* vdec2 power off */
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) |
					0x30);
		}
	} else if (id == VDEC_HCODEC) {
		if (has_hdec()) {
			sleep_val = is_power_ctrl_ver2 ? 0x1 : 0x3;
			iso_val = is_power_ctrl_ver2 ? 0x1 : 0x30;

			/* enable hcodec isolation */
#ifdef CONFIG_AMLOGIC_POWER
			if (is_support_power_ctrl()) {
				if (power_ctrl_iso_mask(false, iso_val, 0)) {
					pr_err("hcodec power off ctrl iso fail.\n");
					return;
				}
			} else {
				WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) | iso_val);
			}
#else
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
				READ_AOREG(AO_RTI_GEN_PWR_ISO0) | iso_val);
#endif
			/* power off hcodec memories */
			WRITE_VREG(DOS_MEM_PD_HCODEC, 0xffffffffUL);
			/* disable hcodec clock */
			hcodec_clock_off();
			/* hcodec power off */
#ifdef CONFIG_AMLOGIC_POWER
			if (is_support_power_ctrl()) {
				if (power_ctrl_sleep_mask(false, sleep_val, 0)) {
					pr_err("hcodec power off ctrl sleep fail.\n");
					return;
				}
			} else {
				WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) | sleep_val);
			}
#else
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
				READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) | sleep_val);
#endif
		}
	} else if (id == VDEC_HEVC) {
		if (has_hevc_vdec()) {
			sleep_val = is_power_ctrl_ver2 ? 0x4 : 0xc0;
			iso_val = is_power_ctrl_ver2 ? 0x4 : 0xc00;

			if (no_powerdown == 0) {
				/* enable hevc isolation */
#ifdef CONFIG_AMLOGIC_POWER
				if (is_support_power_ctrl()) {
					if (power_ctrl_iso_mask(false, iso_val, 0)) {
						pr_err("hevc power off ctrl iso fail.\n");
						return;
					}
				} else {
					WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
						READ_AOREG(AO_RTI_GEN_PWR_ISO0) | iso_val);
				}
#else
				WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) | iso_val);
#endif
				/* power off hevc memories */
				WRITE_VREG(DOS_MEM_PD_HEVC, 0xffffffffUL);

				/* disable hevc clock */
				hevc_clock_off();
				if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A)
					hevc_back_clock_off();

				/* hevc power off */
#ifdef CONFIG_AMLOGIC_POWER
				if (is_support_power_ctrl()) {
					if (power_ctrl_sleep_mask(false, sleep_val, 0)) {
						pr_err("hevc power off ctrl sleep fail.\n");
						return;
					}
				} else {
					WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
						READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) | sleep_val);
				}
#else
				WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) | sleep_val);
#endif
			} else {
				pr_info("!!!!!!!!not power down\n");
				hevc_reset_core(NULL);
				no_powerdown = 0;
			}
		}
	}
}

static bool pm_vdec_legacy_power_state(struct device *dev, int id)
{
	bool ret = false;

	if (id == VDEC_1) {
		if (((READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) &
			(((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) &&
			(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_TL1))
			? 0x2 : 0xc)) == 0) &&
			(READ_HHI_REG(HHI_VDEC_CLK_CNTL) & 0x100))
			ret = true;
	} else if (id == VDEC_2) {
		if (has_vdec2()) {
			if (((READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & 0x30) == 0) &&
				(READ_HHI_REG(HHI_VDEC2_CLK_CNTL) & 0x100))
				ret = true;
		}
	} else if (id == VDEC_HCODEC) {
		if (has_hdec()) {
			if (((READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) &
				(((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) &&
				(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_TL1))
				? 0x1 : 0x3)) == 0) &&
				(READ_HHI_REG(HHI_VDEC_CLK_CNTL) & 0x1000000))
				ret = true;
		}
	} else if (id == VDEC_HEVC) {
		if (has_hevc_vdec()) {
			if (((READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) &
				(((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) &&
				(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_TL1))
				? 0x4 : 0xc0)) == 0) &&
				(READ_HHI_REG(HHI_VDEC2_CLK_CNTL) & 0x1000000))
				ret = true;
		}
	}

	return ret;
}

static void pm_vdec_pd_sec_api_power_on(struct device *dev, int id)
{
	int pd_id = (id == VDEC_1) ? PDID_DOS_VDEC :
		    (id == VDEC_HEVC) ? PDID_DOS_HEVC :
		    PDID_DOS_HCODEC;

	pm_vdec_clock_on(id);
	pwr_ctrl_psci_smc(pd_id, PWR_ON);
}

static void pm_vdec_pd_sec_api_power_off(struct device *dev, int id)
{
	int pd_id = (id == VDEC_1) ? PDID_DOS_VDEC :
		    (id == VDEC_HEVC) ? PDID_DOS_HEVC :
		    PDID_DOS_HCODEC;

	pm_vdec_clock_off(id);
	pwr_ctrl_psci_smc(pd_id, PWR_OFF);
}

static bool pm_vdec_pd_sec_api_power_state(struct device *dev, int id)
{
	int pd_id = (id == VDEC_1) ? PDID_DOS_VDEC :
		    (id == VDEC_HEVC) ? PDID_DOS_HEVC :
		    PDID_DOS_HCODEC;

	return !pwr_ctrl_status_psci_smc(pd_id);
}

static void pm_vdec_pd_nosec_api_power_on(struct device *dev, int id)
{
#if 0
	int pd_id = (id == VDEC_1) ? PM_DOS_VDEC :
		    (id == VDEC_HEVC) ? PM_DOS_HEVC :
			PM_DOS_HCODEC;

	pm_vdec_clock_on(id);
	power_domain_switch(pd_id, PWR_ON);
#endif
}

static void pm_vdec_pd_nosec_api_power_off(struct device *dev, int id)
{
#if 0
	int pd_id = (id == VDEC_1) ? PM_DOS_VDEC :
		    (id == VDEC_HEVC) ? PM_DOS_HEVC :
		    PM_DOS_HCODEC;

	pm_vdec_clock_off(id);
	power_domain_switch(pd_id, PWR_OFF);
#endif
}

static bool pm_vdec_pd_nosec_api_power_state(struct device *dev, int id)
{
	return pm_vdec_legacy_power_state(dev, id);
}

static const struct power_manager_s pm_rw_reg_data = {
	.pm_type	= PM_POWER_CTRL_RW_REG,
	.power_on	= pm_vdec_legacy_power_on,
	.power_off	= pm_vdec_legacy_power_off,
	.power_state	= pm_vdec_legacy_power_state,
};

static const struct power_manager_s pm_ctrl_api_data = {
	.pm_type	= PM_POWER_CTRL_API,
	.power_on	= pm_vdec_legacy_power_on,
	.power_off	= pm_vdec_legacy_power_off,
	.power_state	= pm_vdec_legacy_power_state,
};

static const struct power_manager_s pm_pd_data = {
	.pm_type	= PM_POWER_DOMAIN,
	.pd_data	= pm_domain_data,
	.init		= pm_vdec_power_domain_init,
	.release	= pm_vdec_power_domain_relese,
	.power_on	= pm_vdec_power_domain_power_on,
	.power_off	= pm_vdec_power_domain_power_off,
	.power_state	= pm_vdec_power_domain_power_state,
};

static const struct power_manager_s pm_pd_sec_api_data = {
	.pm_type	= PM_POWER_DOMAIN_SEC_API,
	.power_on	= pm_vdec_pd_sec_api_power_on,
	.power_off	= pm_vdec_pd_sec_api_power_off,
	.power_state	= pm_vdec_pd_sec_api_power_state,
};

static const struct power_manager_s pm_pd_nosec_api_data = {
	.pm_type	= PM_POWER_DOMAIN_NONSEC_API,
	.power_on	= pm_vdec_pd_nosec_api_power_on,
	.power_off	= pm_vdec_pd_nosec_api_power_off,
	.power_state	= pm_vdec_pd_nosec_api_power_state,
};

const struct of_device_id amlogic_vdec_matches[] = {
	{ .compatible = "amlogic, vdec",		.data = &pm_rw_reg_data },
	{ .compatible = "amlogic, vdec-pm-api",		.data = &pm_ctrl_api_data },
	{ .compatible = "amlogic, vdec-pm-pd",		.data = &pm_pd_data },
	{ .compatible = "amlogic, vdec-pm-pd-sec-api",	.data = &pm_pd_sec_api_data },
	{ .compatible = "amlogic, vdec-pm-pd-nsec-api",	.data = &pm_pd_nosec_api_data },
	{},
};
EXPORT_SYMBOL(amlogic_vdec_matches);

