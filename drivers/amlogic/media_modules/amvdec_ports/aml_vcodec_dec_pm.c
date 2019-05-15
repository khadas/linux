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
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*
* Description:
*/
#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>

#include "aml_vcodec_dec_pm.h"
#include "aml_vcodec_util.h"
//#include "aml_vpu.h"

int aml_vcodec_init_dec_pm(struct aml_vcodec_dev *amldev)
{
	struct device_node *node;
	struct platform_device *pdev;
	struct aml_vcodec_pm *pm;
	int ret = 0;

	pdev = amldev->plat_dev;
	pm = &amldev->pm;
	pm->amldev = amldev;
	node = of_parse_phandle(pdev->dev.of_node, "larb", 0);
	if (!node) {
		aml_v4l2_err("of_parse_phandle larb fail!");
		return -1;
	}

	pdev = of_find_device_by_node(node);
	if (WARN_ON(!pdev)) {
		of_node_put(node);
		return -1;
	}
	pm->larbvdec = &pdev->dev;
	pdev = amldev->plat_dev;
	pm->dev = &pdev->dev;

	pm->vcodecpll = devm_clk_get(&pdev->dev, "vcodecpll");
	if (IS_ERR(pm->vcodecpll)) {
		aml_v4l2_err("devm_clk_get vcodecpll fail");
		ret = PTR_ERR(pm->vcodecpll);
	}

	pm->univpll_d2 = devm_clk_get(&pdev->dev, "univpll_d2");
	if (IS_ERR(pm->univpll_d2)) {
		aml_v4l2_err("devm_clk_get univpll_d2 fail");
		ret = PTR_ERR(pm->univpll_d2);
	}

	pm->clk_cci400_sel = devm_clk_get(&pdev->dev, "clk_cci400_sel");
	if (IS_ERR(pm->clk_cci400_sel)) {
		aml_v4l2_err("devm_clk_get clk_cci400_sel fail");
		ret = PTR_ERR(pm->clk_cci400_sel);
	}

	pm->vdec_sel = devm_clk_get(&pdev->dev, "vdec_sel");
	if (IS_ERR(pm->vdec_sel)) {
		aml_v4l2_err("devm_clk_get vdec_sel fail");
		ret = PTR_ERR(pm->vdec_sel);
	}

	pm->vdecpll = devm_clk_get(&pdev->dev, "vdecpll");
	if (IS_ERR(pm->vdecpll)) {
		aml_v4l2_err("devm_clk_get vdecpll fail");
		ret = PTR_ERR(pm->vdecpll);
	}

	pm->vencpll = devm_clk_get(&pdev->dev, "vencpll");
	if (IS_ERR(pm->vencpll)) {
		aml_v4l2_err("devm_clk_get vencpll fail");
		ret = PTR_ERR(pm->vencpll);
	}

	pm->venc_lt_sel = devm_clk_get(&pdev->dev, "venc_lt_sel");
	if (IS_ERR(pm->venc_lt_sel)) {
		aml_v4l2_err("devm_clk_get venc_lt_sel fail");
		ret = PTR_ERR(pm->venc_lt_sel);
	}

	pm->vdec_bus_clk_src = devm_clk_get(&pdev->dev, "vdec_bus_clk_src");
	if (IS_ERR(pm->vdec_bus_clk_src)) {
		aml_v4l2_err("devm_clk_get vdec_bus_clk_src");
		ret = PTR_ERR(pm->vdec_bus_clk_src);
	}

	pm_runtime_enable(&pdev->dev);

	return ret;
}

void aml_vcodec_release_dec_pm(struct aml_vcodec_dev *dev)
{
	pm_runtime_disable(dev->pm.dev);
}

void aml_vcodec_dec_pw_on(struct aml_vcodec_pm *pm)
{
	int ret;

	ret = pm_runtime_get_sync(pm->dev);
	if (ret)
		aml_v4l2_err("pm_runtime_get_sync fail %d", ret);
}

void aml_vcodec_dec_pw_off(struct aml_vcodec_pm *pm)
{
	int ret;

	ret = pm_runtime_put_sync(pm->dev);
	if (ret)
		aml_v4l2_err("pm_runtime_put_sync fail %d", ret);
}

void aml_vcodec_dec_clock_on(struct aml_vcodec_pm *pm)
{
	int ret;

	ret = clk_set_rate(pm->vcodecpll, 1482 * 1000000);
	if (ret)
		aml_v4l2_err("clk_set_rate vcodecpll fail %d", ret);

	ret = clk_set_rate(pm->vencpll, 800 * 1000000);
	if (ret)
		aml_v4l2_err("clk_set_rate vencpll fail %d", ret);

	ret = clk_prepare_enable(pm->vcodecpll);
	if (ret)
		aml_v4l2_err("clk_prepare_enable vcodecpll fail %d", ret);

	ret = clk_prepare_enable(pm->vencpll);
	if (ret)
		aml_v4l2_err("clk_prepare_enable vencpll fail %d", ret);

	ret = clk_prepare_enable(pm->vdec_bus_clk_src);
	if (ret)
		aml_v4l2_err("clk_prepare_enable vdec_bus_clk_src fail %d",
				ret);

	ret = clk_prepare_enable(pm->venc_lt_sel);
	if (ret)
		aml_v4l2_err("clk_prepare_enable venc_lt_sel fail %d", ret);

	ret = clk_set_parent(pm->venc_lt_sel, pm->vdec_bus_clk_src);
	if (ret)
		aml_v4l2_err("clk_set_parent venc_lt_sel vdec_bus_clk_src fail %d",
				ret);

	ret = clk_prepare_enable(pm->univpll_d2);
	if (ret)
		aml_v4l2_err("clk_prepare_enable univpll_d2 fail %d", ret);

	ret = clk_prepare_enable(pm->clk_cci400_sel);
	if (ret)
		aml_v4l2_err("clk_prepare_enable clk_cci400_sel fail %d", ret);

	ret = clk_set_parent(pm->clk_cci400_sel, pm->univpll_d2);
	if (ret)
		aml_v4l2_err("clk_set_parent clk_cci400_sel univpll_d2 fail %d",
				ret);

	ret = clk_prepare_enable(pm->vdecpll);
	if (ret)
		aml_v4l2_err("clk_prepare_enable vdecpll fail %d", ret);

	ret = clk_prepare_enable(pm->vdec_sel);
	if (ret)
		aml_v4l2_err("clk_prepare_enable vdec_sel fail %d", ret);

	ret = clk_set_parent(pm->vdec_sel, pm->vdecpll);
	if (ret)
		aml_v4l2_err("clk_set_parent vdec_sel vdecpll fail %d", ret);

	//ret = aml_smi_larb_get(pm->larbvdec);
	if (ret)
		aml_v4l2_err("aml_smi_larb_get larbvdec fail %d", ret);

}

void aml_vcodec_dec_clock_off(struct aml_vcodec_pm *pm)
{
	//aml_smi_larb_put(pm->larbvdec);
	clk_disable_unprepare(pm->vdec_sel);
	clk_disable_unprepare(pm->vdecpll);
	clk_disable_unprepare(pm->univpll_d2);
	clk_disable_unprepare(pm->clk_cci400_sel);
	clk_disable_unprepare(pm->venc_lt_sel);
	clk_disable_unprepare(pm->vdec_bus_clk_src);
	clk_disable_unprepare(pm->vencpll);
	clk_disable_unprepare(pm->vcodecpll);
}
