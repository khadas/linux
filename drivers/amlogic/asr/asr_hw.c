// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include <linux/amlogic/major.h>
#include <linux/amlogic/vad_api.h>

#include "iflytek/ivw_defines.h"
#include "iflytek/param_module.h"
#include "iflytek/w_ivw.h"

#define PERIOD_FRAMES  (IVW_FIXED_WRITE_BUF_LEN / 2)

static ivChar *g_callback_param = "ivw engine from iflytek";
static bool g_need_wakeup;
static WIVW_INST g_ivw_inst;
u16 g_temp_buf16[PERIOD_FRAMES];
u32 g_remain_frames;

ivInt IvwCallBackWakeup(const ivChar *param, void *p_user_param)
{
	const char *p_user_info = (const char *)p_user_param;

	pr_info("aml_asr:%s ivw param=%s user_param=%s\n", __func__, param, p_user_info);
	g_need_wakeup = true;
	return 0;
}

ivInt IvwCallBackWarmup(const ivChar *param, void *p_user_param)
{
	const char *p_user_info = (const char *)p_user_param;

	pr_info("aml_asr:%s ivw param=%s user_param=%s\n", __func__, param, p_user_info);
	return 0;
}

ivInt IvwCallBackPreWakeup(const ivChar *param, void *p_user_param)
{
	const char *p_user_info = (const char *)p_user_param;

	pr_info("aml_asr:%s ivw param=%s user_param=%s\n", __func__, param, p_user_info);
	return 0;
}

int aml_asr_enable(void)
{
	int inst_size = 0;
	int value = 0;
	ivInt ret;

	if (g_ivw_inst) {
		pr_err("aml_asr:%s wakeup_init has been inited, return\n", __func__);
		return -1;
	}

	inst_size = wIvwGetInstSize();
	pr_info("aml_asr:%s version:%s, inst_size:%d\n", __func__, wIvwGetVersion(), inst_size);
	g_ivw_inst = (WIVW_INST)kzalloc(inst_size, GFP_KERNEL);
	if (!g_ivw_inst)
		return -1;

	ret = wIvwCreate(g_ivw_inst, NULL, NULL);
	//ret = wIvwCreate(&g_ivw_inst, mlp_res, filler_res);
	ret = wIvwRegisterCallBacks(g_ivw_inst, CallBackFuncNameWakeUp,
			IvwCallBackWakeup, g_callback_param);
	ret = wIvwRegisterCallBacks(g_ivw_inst, CallBackFuncNameWarmUp,
			IvwCallBackWarmup, g_callback_param);
	//ret = wIvwRegisterCallBacks(g_ivw_inst, CallBackFuncNamePreWakeup,
	//IvwCallBackPreWakeup, g_callback_param);
	//ret = wIvwStart(g_ivw_inst);

	wIvwGetParameter(g_ivw_inst, PARAM_W_IVW_VADON, &value);
	pr_debug("PARAM_W_IVW_VADON: %d\n", value);
	wIvwGetParameter(g_ivw_inst, PARAM_W_FEA_MLP_JUMPFREAM_NUM, &value);
	pr_debug("PARAM_W_FEA_MLP_JUMPFREAM_NUM: %d\n", value);
	wIvwGetParameter(g_ivw_inst, PARAM_W_FEA_DO_CMN, &value);
	pr_debug("PARAM_W_FEA_DO_CMN: %d\n", value);
	wIvwGetParameter(g_ivw_inst, PARAM_W_DEC_NCMTHRESHOLD_KEYWORD1_ARC1, &value);
	pr_debug("PARAM_W_DEC_NCMTHRESHOLD_KEYWORD1_ARC1: %d\n", value);
	wIvwGetParameter(g_ivw_inst, PARAM_W_DEC_NCMTHRESHOLD_KEYWORD2_ARC1, &value);
	pr_debug("PARAM_W_DEC_NCMTHRESHOLD_KEYWORD2_ARC1: %d\n", value);
	wIvwGetParameter(g_ivw_inst, PARAM_W_DEC_NLMPENALTY, &value);
	pr_debug("PARAM_W_DEC_NLMPENALTY: %d\n", value);

	value = 0;
	wIvwSetParameter(g_ivw_inst, PARAM_W_IVW_VADON, value);
	wIvwGetParameter(g_ivw_inst, PARAM_W_IVW_VADON, &value);
	pr_debug("PARAM_W_IVW_VADON: %d\n", value);

	value = 1;
	wIvwSetParameter(g_ivw_inst, PARAM_W_FEA_MLP_JUMPFREAM_NUM, value);
	wIvwGetParameter(g_ivw_inst, PARAM_W_FEA_MLP_JUMPFREAM_NUM, &value);
	pr_debug("PARAM_W_FEA_MLP_JUMPFREAM_NUM: %d\n", value);

	wIvwGetParameter(g_ivw_inst, PARAM_W_DEC_MAX_STATE_LOOP, &value);
	pr_debug("PARAM_W_DEC_MAX_STATE_LOOP: %d\n", value);
	value = 50;
	wIvwSetParameter(g_ivw_inst, PARAM_W_DEC_MAX_STATE_LOOP, value);
	wIvwGetParameter(g_ivw_inst, PARAM_W_DEC_MAX_STATE_LOOP, &value);
	pr_debug("PARAM_W_DEC_MAX_STATE_LOOP: %d\n", value);

	wIvwGetParameter(g_ivw_inst, PARAM_W_DEC_NCMTHRESHOLD_KEYWORD1_ARC1, &value);
	pr_debug("PARAM_W_DEC_NCMTHRESHOLD_KEYWORD1_ARC1: %d\n", value);

	wIvwGetParameter(g_ivw_inst, PARAM_W_DEC_NCMTHRESHOLD_PREWAKE_KEYWORD1_ARC1, &value);
	pr_debug("PARAM_W_DEC_NCMTHRESHOLD_PREWAKE_KEYWORD1_ARC1: %d\n", value);

	wIvwGetParameter(g_ivw_inst, PARAM_W_DEC_NCMTHRESHOLD_KEYWORD2_ARC1, &value);
	pr_debug("PARAM_W_DEC_NCMTHRESHOLD_KEYWORD2_ARC1: %d\n", value);

	wIvwGetParameter(g_ivw_inst, PARAM_W_DEC_NCMTHRESHOLD_PREWAKE_KEYWORD2_ARC1, &value);
	pr_debug("PARAM_W_DEC_NCMTHRESHOLD_PREWAKE_KEYWORD2_ARC1: %d\n", value);

	wIvwGetParameter(g_ivw_inst, PARAM_W_DEC_NLMPENALTY, &value);
	pr_debug("PARAM_W_DEC_NLMPENALTY: %d\n", value);
	ret = wIvwStart(g_ivw_inst);
	pr_info("aml_asr:%s wakeup_init\n", __func__);
	return ret;
}
EXPORT_SYMBOL_GPL(aml_asr_enable);

int aml_asr_disable(void)
{
	ivInt ret = 0;

	if (!g_ivw_inst) {
		pr_err("aml_asr:%s wakeup_init has been deinited, return\n", __func__);
		return -1;
	}
	ret = wIvwStop(g_ivw_inst);
	if (ret != 0)
		pr_err("aml_asr:%s wIvwStop ret:%d\n", __func__, ret);
	ret = wIvwDestroy(g_ivw_inst);
	if (ret != 0)
		pr_err("aml_asr:%s wIvwDestroy ret:%d\n", __func__, ret);
	kfree(g_ivw_inst);
	g_ivw_inst = NULL;
	g_need_wakeup = false;

	return ret;
}
EXPORT_SYMBOL_GPL(aml_asr_disable);

/* only supports 1 channels, 16bit, 16Khz.
 * And, the wIvwWrite function only supports 160 frames(10ms) per write.
 */
int aml_asr_feed(char *buf, u32 frames)
{
	u16 *p_temp_buf16 = NULL;
	int i, ret = -1;
	u32 frames_total = frames + g_remain_frames;
//	static int cnt = 1;
//
//	if (cnt % 100 == 0) {
//		pr_info("aml_asr:%s frames:%d\n", __func__, frames);
//		cnt = 1;
//	} else {
//		cnt++;
//	}
	p_temp_buf16 = (u16 *)buf;
	if (!g_ivw_inst) {
		pr_err("aml_asr:%s asr is not init!\n", __func__);
		return 0;
	}

	for (i = 0; i < frames_total / PERIOD_FRAMES; i++) {
		if (g_remain_frames) {
			int extra_frames = PERIOD_FRAMES - g_remain_frames;

			memcpy(g_temp_buf16 + g_remain_frames, p_temp_buf16, extra_frames * 2);
			p_temp_buf16 += extra_frames;
			ret = wIvwWrite(g_ivw_inst, g_temp_buf16, IVW_FIXED_WRITE_BUF_LEN);
			if (ret != 0)
				pr_err("aml_asr:%s wIvwWrite fail ret:%d\n", __func__, ret);
			g_remain_frames = 0;
		} else {
			memcpy(g_temp_buf16, p_temp_buf16, IVW_FIXED_WRITE_BUF_LEN);
			p_temp_buf16 += PERIOD_FRAMES;
			ret = wIvwWrite(g_ivw_inst, g_temp_buf16, IVW_FIXED_WRITE_BUF_LEN);
			if (ret != 0)
				pr_err("aml_asr:%s wIvwWrite fail ret:%d\n", __func__, ret);
		}
		if (g_need_wakeup) {
			TWakeUpResult wakeup_rst;

			wIvwGetLastWakeupRst(g_ivw_inst, &wakeup_rst);
			g_need_wakeup = false;
			pr_info("aml_asr:%s VAD wakeup now ^_^!\n", __func__);
			pr_info("aml_asr:%s +++++++++++++++++++++++++\n", __func__);
			return 1;
		}
	}

	g_remain_frames = frames_total % PERIOD_FRAMES;
	if (g_remain_frames)
		memcpy(g_temp_buf16, p_temp_buf16, g_remain_frames * 2);

	return 0;
}
EXPORT_SYMBOL_GPL(aml_asr_feed);

static int __init aml_asr_module_init(void)
{
	g_ivw_inst = NULL;
	g_need_wakeup = false;
	g_remain_frames = 0;
	return 0;
}

static void __exit aml_asr_module_exit(void)
{
}

module_init(aml_asr_module_init);
module_exit(aml_asr_module_exit);

MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("AMLOGIC ASR");
MODULE_LICENSE("GPL v2");

