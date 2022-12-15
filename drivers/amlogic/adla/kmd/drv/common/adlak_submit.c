/*******************************************************************************
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_submit.c
 * @brief
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   	Who				Date				Changes
 * ----------------------------------------------------------------------------
 * 1.00a shiwei.sun@amlogic.com	2021/06/13	Initial release
 * </pre>
 *
 ******************************************************************************/

/***************************** Include Files *********************************/
#include "adlak_submit.h"

#include "adlak_api.h"
#include "adlak_common.h"
#include "adlak_context.h"
#include "adlak_device.h"
#include "adlak_dpm.h"
#include "adlak_mm.h"
#include "adlak_queue.h"
/************************** Constant Definitions *****************************/
#ifndef ADLAK_DEBUG_CMQ_PATTTCHING_EN
#define ADLAK_DEBUG_CMQ_PATTTCHING_EN (0)
#endif
/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Variable Definitions *****************************/

/************************** Function Prototypes ******************************/

static uint32_t adlak_get_cmq_addr_offset(uint32_t cmq_offset, uint32_t cmq_wr_offset,
                                          uint32_t cmq_size_max) {
    uint32_t offset;
    offset = cmq_wr_offset + cmq_offset;
    if (offset >= cmq_size_max) {
        offset = offset - cmq_size_max;
    }
    return offset;
}
static int adlak_cmq_dump(struct adlak_device *padlak, int size) {
#if ADLAK_DEBUG_CMQ_PATTTCHING_EN
    uint32_t  start, i, cmq_size_dw;
    uint32_t *pcmq_buf = NULL;
    AML_LOG_DEBUG("Dump cmq buffer:");
    pcmq_buf    = padlak->cmq_buf_info.cmq_mm_info->cpu_addr;
    cmq_size_dw = size / sizeof(uint32_t);
    start       = ((padlak->cmq_buf_info.cmq_wr_offset + padlak->cmq_buf_info.total_size - size) %
             padlak->cmq_buf_info.total_size) /
            sizeof(uint32_t);

    for (i = 0; i < cmq_size_dw;) {
        if (((start + i) * sizeof(uint32_t)) >= padlak->cmq_buf_info.total_size) {
            // There's no exception here due to the size is align with 256
            cmq_size_dw = cmq_size_dw - i;
            start       = 0;
            i           = 0;
        }
        AML_LOG_DEFAULT("offset:0x%08X\t0x%08X 0x%08X 0x%08X 0x%08X \n",
                        (uint32_t)((start + i) * sizeof(uint32_t)), pcmq_buf[start + i],
                        pcmq_buf[start + i + 1], pcmq_buf[start + i + 2], pcmq_buf[start + i + 3]);
        i = i + 4;
    }
    AML_LOG_DEFAULT("\n");
#endif
    return 0;
}

static void adlak_debug_buf_dump(void *cpu_addr, int size) {
#if ADLAK_DEBUG_CMQ_PATTTCHING_EN
    uint32_t  start, i, cmq_size_dw;
    uint32_t *pcmq_buf = NULL;
    AML_LOG_DEBUG("Dump  buffer:");
    pcmq_buf    = cpu_addr;
    cmq_size_dw = size / sizeof(uint32_t);
    start       = 0;
    i           = 0;
    for (i = 0; i < cmq_size_dw;) {
        AML_LOG_DEFAULT("offset:0x%08X\t0x%08X 0x%08X 0x%08X 0x%08X \n",
                        (uint32_t)((start + i) * sizeof(uint32_t)), pcmq_buf[start + i],
                        pcmq_buf[start + i + 1], pcmq_buf[start + i + 2], pcmq_buf[start + i + 3]);
        i = i + 4;
    }
    AML_LOG_DEFAULT("\n");
#endif
}

#if CONFIG_ADLAK_EMU_EN
uint32_t g_adlak_emu_dev_wpt;
uint32_t g_adlak_emu_dev_rpt;
uint32_t g_adlak_emu_dev_cmq_size;
uint32_t g_adlak_emu_dev_cmq_total_size;

uint32_t adlak_emu_update_rpt(void) {
    g_adlak_emu_dev_rpt = g_adlak_emu_dev_wpt;
    return g_adlak_emu_dev_rpt;
}
uint32_t adlak_emu_hal_get_ps_rpt(void *data) {
    return adlak_get_cmq_addr_offset(g_adlak_emu_dev_cmq_size - 16, g_adlak_emu_dev_rpt,
                                     g_adlak_emu_dev_cmq_total_size);
}

#endif

#ifdef CONFIG_ADLAK_PRE_PATCH
int adlak_submit_exec(struct adlak_task *ptask) {
    struct adlak_device *padlak = ptask->padlak;
    uint32_t             invoke_num;
    AML_LOG_INFO("%s", __func__);

    if (ptask->state != ADLAK_SUBMIT_STATE_READY) {
        AML_LOG_ERR("ptask->state != ADLAK_SUBMIT_STATE_READY");
        return -1;
    }

    AML_LOG_INFO("invoke id[%d-%d]", ptask->invoke_start_idx, ptask->invoke_end_idx);

    padlak->cmq_buf_info.cmq_wr_offset =
        adlak_get_cmq_addr_offset(ptask->cmq_buf_info.size, padlak->cmq_buf_info.cmq_wr_offset,
                                  padlak->cmq_buf_info.total_size);

    adlak_cmq_dump(padlak, ptask->cmq_buf_info.size);
    ptask->hw_stat.irq_status.timeout = false;
    invoke_num                        = ptask->invoke_end_idx + 1 - ptask->invoke_start_idx;

    if (padlak->hw_timeout_ms) {
        ptask->hw_timeout_ms = padlak->hw_timeout_ms * invoke_num;
    } else {
        ptask->hw_timeout_ms = 10 * invoke_num;
    }
    /*invalid tlbs*/
    if (padlak->mm->use_smmu) {
        padlak->mm->smmu_ops->smmu_tlb_invalidate(padlak->mm);
    }

    adlak_profile_start(padlak, &ptask->profilling, ptask->invoke_start_idx);
    mb();
    adlak_hal_submit((void *)padlak, padlak->cmq_buf_info.cmq_wr_offset);
    AML_LOG_INFO("cmq_wr_offset=%u , cmq_rd_offset=%u, cmq_size=%u",
                 padlak->cmq_buf_info.cmq_wr_offset, padlak->cmq_buf_info.cmq_rd_offset,
                 ptask->cmq_buf_info.size);

#if CONFIG_ADLAK_EMU_EN
    g_adlak_emu_dev_wpt            = padlak->cmq_buf_info.cmq_wr_offset;
    g_adlak_emu_dev_cmq_size       = ptask->cmq_buf_info.size;
    g_adlak_emu_dev_cmq_total_size = padlak->cmq_buf_info.total_size;
#endif
    ptask->state = ADLAK_SUBMIT_STATE_RUNNING;
    return 0;
}
#endif

static bool adlak_has_dependency(int64_t dlid, int64_t flid, int32_t max_outstanding) {
    // true if:
    //  (1) dependent on output of previous tasks, and
    //  (2) flid is not too far ahead of dlid (within the range of [0, max_outstanding -1])
    //
    // note: condition (2) is necessary due to the limited bit count used in HW to store IDs

    ASSERT(flid >= dlid);

    if ((dlid >= 0) && ((flid - dlid) < max_outstanding)) {
        return true;
    }

    return false;
};
static int32_t adlak_get_max_outstanding_outputs(enum ADLAK_PLATFORM_MODULE module) {
    switch (module) {
        case ADLAK_PLATFORM_MODULE_PWE:
            return 4;

        case ADLAK_PLATFORM_MODULE_PWX:
            return 4;

        case ADLAK_PLATFORM_MODULE_RS:
            return 4;

        default:
            // should not reach here
            ASSERT(0);
            while (1)
                ;
            return 0;
    }
}
static uint32_t adlak_gen_dep_cmd(int dependency_mode, struct adlak_workqueue *pwq,
                                  const struct adlak_submit_task *task,
                                  struct adlak_submit_dep_fixup * pdep_fixups_base,
                                  int32_t pwe_flid_offset, int32_t pwx_flid_offset,
                                  int32_t rs_flid_offset) {
    uint32_t                       cmd = PS_CMD_SET_DEPENDENCY;  // default no dependency
    struct adlak_submit_dep_fixup *dep = NULL;

    {
        bool     has_pwe_dependency = false, has_pwx_dependency = false, has_rs_dependency = false;
        int64_t  pwe_dlid = -1, pwx_dlid = -1, rs_dlid = -1;
        int      i;
        uint32_t dependency;

        if (dependency_mode != ADLAK_DEPENDENCY_MODE_PARSER) {
            return cmd;
        }
#if ADLAK_DEBUG_CMQ_PATTTCHING_EN
        AML_LOG_DEBUG("%s", __func__);
        AML_LOG_DEBUG("pwe_flid_offset=%d,pwx_flid_offset=%d,rs_flid_offset=%d", pwe_flid_offset,
                      pwx_flid_offset, rs_flid_offset);
        AML_LOG_DEBUG("global_id_pwe=%d,global_id_pwx=%d,global_id_rs=%d", pwq->id_cur.start_id_pwe,
                      pwq->id_cur.start_id_pwx, pwq->id_cur.start_id_rs);
        AML_LOG_DEBUG("dep_info_offset=%d,dep_info_count=%d", task->dep_info_offset,
                      task->dep_info_count);
        AML_LOG_DEBUG("reg_fixup_offset=%d,reg_fixup_count=%d", task->reg_fixup_offset,
                      task->reg_fixup_count);
#endif
        for (i = 0; i < task->dep_info_count; ++i) {
            dep = &pdep_fixups_base[task->dep_info_offset + i];
            switch (dep->module) {
                case ADLAK_DEPENDENCY_MODULE_PWE:
                    pwe_dlid = max(pwe_dlid, (int64_t)dep->dep_id);
                    break;

                case ADLAK_DEPENDENCY_MODULE_PWX:
                    pwx_dlid = max(pwx_dlid, (int64_t)dep->dep_id);
                    break;

                case ADLAK_DEPENDENCY_MODULE_RS:
                    rs_dlid = max(rs_dlid, (int64_t)dep->dep_id);
                    break;

                default:
                    // should not reach here
                    AML_LOG_ERR("dep->module=%u", dep->module);
                    ASSERT(0);
                    break;
            }
        }
#if ADLAK_DEBUG_CMQ_PATTTCHING_EN
        AML_LOG_DEBUG("pwe_dlid =%lld, pwx_dlid =%lld,rs_dlid=%lld", pwe_dlid, pwx_dlid, rs_dlid);
#endif
        if (pwe_dlid >= 0) {
            if (pwe_dlid >= pwe_flid_offset) {
#if ADLAK_DEBUG_CMQ_PATTTCHING_EN
                AML_LOG_DEBUG("pwe_dlid =%lld, pwe_flid_offset =%d,start_pwe_flid=%d", pwe_dlid,
                              pwe_flid_offset, task->start_pwe_flid);
#endif
                has_pwe_dependency = adlak_has_dependency(
                    pwe_dlid, task->start_pwe_flid,
                    adlak_get_max_outstanding_outputs(ADLAK_PLATFORM_MODULE_PWE));
                pwe_dlid = pwq->id_cur.start_id_pwe + (pwe_dlid - pwe_flid_offset) + 1;
            }
        }
        if (pwx_dlid >= 0) {
            if (pwx_dlid >= pwx_flid_offset) {
#if ADLAK_DEBUG_CMQ_PATTTCHING_EN
                AML_LOG_DEBUG("pwx_dlid =%lld, pwx_flid_offset =%d,start_pwx_flid=%d", pwx_dlid,
                              pwx_flid_offset, task->start_pwx_flid);
#endif
                has_pwx_dependency = adlak_has_dependency(
                    pwx_dlid, task->start_pwx_flid,
                    adlak_get_max_outstanding_outputs(ADLAK_PLATFORM_MODULE_PWX));
                pwx_dlid = pwq->id_cur.start_id_pwx + (pwx_dlid - pwx_flid_offset) + 1;
            }
        }

        if (rs_dlid >= 0) {
            if (rs_dlid >= rs_flid_offset) {
#if ADLAK_DEBUG_CMQ_PATTTCHING_EN
                AML_LOG_DEBUG("rs_dlid =%lld, rs_flid_offset =%d,start_rs_flid=%d\n", rs_dlid,
                              rs_flid_offset, task->start_rs_flid);
#endif
                has_rs_dependency = adlak_has_dependency(
                    rs_dlid, task->start_rs_flid,
                    adlak_get_max_outstanding_outputs(ADLAK_PLATFORM_MODULE_RS));
                rs_dlid = pwq->id_cur.start_id_rs + (rs_dlid - rs_flid_offset) + 1;
            }
        }

        dependency =
            (has_rs_dependency ? PS_CMD_DEPENDENCY_RS_VALID_MASK : 0) |
            (has_pwe_dependency ? PS_CMD_DEPENDENCY_PWE_VALID_MASK : 0) |
            (has_pwx_dependency ? PS_CMD_DEPENDENCY_PWX_VALID_MASK : 0) |
            ((rs_dlid << PS_CMD_DEPENDENCY_RS_ID_SHIFT) & PS_CMD_DEPENDENCY_RS_ID_MASK) |
            ((pwe_dlid << PS_CMD_DEPENDENCY_PWE_ID_SHIFT) & PS_CMD_DEPENDENCY_PWE_ID_MASK) |
            ((pwx_dlid << PS_CMD_DEPENDENCY_PWX_ID_SHIFT) & PS_CMD_DEPENDENCY_PWX_ID_MASK);

        cmd |= dependency;
    }

    return cmd;
}
static int adlak_cmq_remain_space_check(struct adlak_device *padlak, uint32_t wr_offset,
                                        int size_required) {
    int size_remain;
    do {
        AML_LOG_DEBUG("%s", __func__);

        if (padlak->cmq_buf_info.cmq_rd_offset == wr_offset) {
            // empty
            size_remain = padlak->cmq_buf_info.total_size;
        } else {
            size_remain =
                (padlak->cmq_buf_info.total_size + padlak->cmq_buf_info.cmq_rd_offset - wr_offset) %
                padlak->cmq_buf_info.total_size;
        }

        AML_LOG_DEBUG(
            "need %d bytes space.remain %d bytes;\nrd_offset=%d,wr_offset=%d,cmq_total_size=%d.",
            size_required, size_remain, padlak->cmq_buf_info.cmq_rd_offset, wr_offset,
            padlak->cmq_buf_info.total_size);
        if (size_required < size_remain) {
            return 0;
        }
        // if there is not enough cmq space ,we need wait..
        // wait_for_completion
        AML_LOG_INFO("there is not enough cmq space,we need wait!");
        adlak_os_sema_take_timeout(padlak->paser_refresh, (10));
#if CONFIG_ADLAK_EMU_EN
        padlak->cmq_buf_info.cmq_rd_offset = adlak_emu_hal_get_ps_rpt(padlak);
#else
        padlak->cmq_buf_info.cmq_rd_offset = adlak_hal_get_ps_rpt(padlak);
#endif
    } while (1);
}

static void adlak_update_addr_fixups(const struct adlak_submit_task *task,
                                     struct adlak_submit_addr_fixup *paddr_fixups_base,
                                     uint32_t *pcmq_buf, uint32_t cmq_offset_cfg,
                                     uint32_t cmq_wr_offset_u32, uint32_t cmq_size_max_u32) {
    struct adlak_submit_addr_fixup *addr_fixup = NULL;
    int32_t                         i;
    uint32_t                        cmq_offset;
    AML_LOG_DEBUG("%s", __func__);
    for (i = 0; i < task->addr_fixup_count; ++i) {
        addr_fixup = &paddr_fixups_base[task->addr_fixup_offset + i];

        cmq_offset =
            adlak_get_cmq_addr_offset(cmq_offset_cfg + (addr_fixup->loc / sizeof(uint32_t)),
                                      cmq_wr_offset_u32, cmq_size_max_u32);
        pcmq_buf[cmq_offset] =
            (pcmq_buf[cmq_offset] & (~addr_fixup->mask)) |
            ((((uintptr_t)addr_fixup->addr / addr_fixup->unit) << addr_fixup->shift) &
             addr_fixup->mask);
        // TODO(shiwei.sun) flush cache of update addr
    }
}

static void adlak_update_reg_fixups(int dependency_mode, const struct adlak_submit_task *task,
                                    struct adlak_submit_reg_fixup *preg_fixups_base,
                                    uint32_t *pcmq_buf, uint32_t cmq_offset_cfg,
                                    uint32_t cmq_wr_offset_u32, uint32_t cmq_size_max_u32) {
    struct adlak_submit_reg_fixup *reg_fixup = NULL;
    int32_t                        i;
    uint32_t                       mode, cmq_offset;

    AML_LOG_DEBUG("%s", __func__);
    for (i = 0; i < task->reg_fixup_count; ++i) {
        reg_fixup = &preg_fixups_base[task->reg_fixup_offset + i];
        if (ADLAK_REG_FIXUP_TYPE_PW_COMP_FLUSH_MODE == reg_fixup->type) {
            mode = reg_fixup->modes[dependency_mode];
            cmq_offset =
                adlak_get_cmq_addr_offset(cmq_offset_cfg + (reg_fixup->loc / sizeof(uint32_t)),
                                          cmq_wr_offset_u32, cmq_size_max_u32);
            pcmq_buf[cmq_offset] = (pcmq_buf[cmq_offset] & (~reg_fixup->mask)) |
                                   ((mode << reg_fixup->shift) & reg_fixup->mask);
        } else {
            // should not reach here
            AML_LOG_ERR("reg_fixup->type=%u", reg_fixup->type);
            ASSERT(0);
        }
    }
}

static void adlak_update_module_dependency(int dependency_mode, struct adlak_workqueue *pwq,
                                           const struct adlak_submit_task *task,
                                           struct adlak_submit_dep_fixup * pdep_fixups_base,
                                           int32_t pwe_flid_offset, int32_t pwx_flid_offset,
                                           int32_t rs_flid_offset, uint32_t *pcmq_buf,
                                           uint32_t cmq_offset_cfg, uint32_t cmq_wr_offset_u32,
                                           uint32_t cmq_size_max_u32) {
    struct adlak_submit_dep_fixup *dep = NULL;
    int64_t                        invoke_start_flid, dlid;
    int32_t                        i, task_start_flid, flid_offset, max_outstanding_outputs;
    uint32_t                       mode, cmq_offset;

    if ((dependency_mode != ADLAK_DEPENDENCY_MODE_MODULE_LAYER) &&
        (dependency_mode != ADLAK_DEPENDENCY_MODE_MODULE_H_COUNT)) {
        return;
    }
    AML_LOG_DEBUG("%s", __func__);
    for (i = 0; i < task->dep_info_count; ++i) {
        invoke_start_flid       = -1;
        task_start_flid         = -1;
        flid_offset             = 0;
        max_outstanding_outputs = 0;
        dep                     = &pdep_fixups_base[task->dep_info_offset + i];
        switch (dep->module) {
            case ADLAK_DEPENDENCY_MODULE_PWE:
                invoke_start_flid = pwq->id_cur.start_id_pwe;
                task_start_flid   = task->start_pwe_flid;
                flid_offset       = pwe_flid_offset;
                max_outstanding_outputs =
                    adlak_get_max_outstanding_outputs(ADLAK_PLATFORM_MODULE_PWE);
                break;

            case ADLAK_DEPENDENCY_MODULE_PWX:
                invoke_start_flid = pwq->id_cur.start_id_pwx;
                task_start_flid   = task->start_pwx_flid;
                flid_offset       = pwx_flid_offset;
                max_outstanding_outputs =
                    adlak_get_max_outstanding_outputs(ADLAK_PLATFORM_MODULE_PWX);
                break;

            case ADLAK_DEPENDENCY_MODULE_RS:
                invoke_start_flid = pwq->id_cur.start_id_rs;
                task_start_flid   = task->start_rs_flid;
                flid_offset       = rs_flid_offset;
                max_outstanding_outputs =
                    adlak_get_max_outstanding_outputs(ADLAK_PLATFORM_MODULE_RS);
                break;

            default:
                // should not reach here
                AML_LOG_ERR("dep->module=%u", dep->module);
                ASSERT(0);
                break;
        }
        if (dep->dep_id >= flid_offset) {
            if (adlak_has_dependency(dep->dep_id, task_start_flid, max_outstanding_outputs)) {
                dlid = invoke_start_flid + (dep->dep_id - flid_offset) + 1;
                mode = dep->dep_modes[dependency_mode];
                cmq_offset =
                    adlak_get_cmq_addr_offset(cmq_offset_cfg + (dep->id_loc / sizeof(uint32_t)),
                                              cmq_wr_offset_u32, cmq_size_max_u32);

                pcmq_buf[cmq_offset] = (pcmq_buf[cmq_offset] & (~dep->id_mask)) |
                                       ((dlid << dep->id_shift) & dep->id_mask);

                cmq_offset =
                    adlak_get_cmq_addr_offset(cmq_offset_cfg + (dep->mode_loc / sizeof(uint32_t)),
                                              cmq_wr_offset_u32, cmq_size_max_u32);
                pcmq_buf[cmq_offset] = (pcmq_buf[cmq_offset] & (~dep->mode_mask)) |
                                       ((mode << dep->mode_shift) & dep->mode_mask);
            }
        }
    }
}

#ifdef CONFIG_ADLAK_PRE_PATCH
int adlak_invoke_pattching(struct adlak_task *ptask) {
    struct adlak_device *           padlak = ptask->padlak;
    struct adlak_workqueue *        pwq    = &padlak->queue;
    uint32_t                        cmq_offset, cmq_offset_cfg;
    uint32_t                        cmq_size;
    struct adlak_submit_task *      psubmitask_base = NULL, *psubmitask = NULL;
    uint32_t                        task_idx, module_index, active_modules = 0, output_modules = 0;
    struct adlak_submit_dep_fixup * psubmit_dep_fixup_base   = NULL;
    struct adlak_submit_reg_fixup * psubmit_reg_fixup_base   = NULL;
    struct adlak_submit_addr_fixup *psubmit_addr_fixups_base = NULL;
    uint8_t *                       config_base              = NULL;
    uint32_t *                      pcmq_buf                 = NULL;
    uint32_t                        nop_size, cmq_size_max_u32, cmq_wr_offset_u32, cmq_size_tmp;
    int32_t                         start_pwe_flid, start_pwx_flid, start_rs_flid;

    const uint32_t parser_active_modules[ADLAK_PLATFORM_MODULE_COUNT] = {
        PS_CMD_CONFIG_RS_MASK,      // ADLAK_PLATFORM_MODULE_RS
        PS_CMD_CONFIG_RS_CAT_MASK,  // ADLAK_PLATFORM_MODULE_RS_CAT
        PS_CMD_CONFIG_MC_MASK,      // ADLAK_PLATFORM_MODULE_MC
        PS_CMD_CONFIG_DMCF_MASK,    // ADLAK_PLATFORM_MODULE_DMCF
        PS_CMD_CONFIG_DMCW_MASK,    // ADLAK_PLATFORM_MODULE_DMCW
        PS_CMD_CONFIG_PE_MASK,      // ADLAK_PLATFORM_MODULE_PE
        PS_CMD_CONFIG_PE_LUT_MASK,  // ADLAK_PLATFORM_MODULE_PE_LUT
        PS_CMD_CONFIG_DW_MASK,      // ADLAK_PLATFORM_MODULE_DW
        PS_CMD_CONFIG_DMDF_MASK,    // ADLAK_PLATFORM_MODULE_DMDF
        PS_CMD_CONFIG_DMDW_MASK,    // ADLAK_PLATFORM_MODULE_DMDW
        PS_CMD_CONFIG_PX_MASK,      // ADLAK_PLATFORM_MODULE_PX
        PS_CMD_CONFIG_PX_LUT_MASK,  // ADLAK_PLATFORM_MODULE_PX_LUT
        PS_CMD_CONFIG_PWE_MASK,     // ADLAK_PLATFORM_MODULE_PWE
        PS_CMD_CONFIG_PWX_MASK      // ADLAK_PLATFORM_MODULE_PWX
    };
    AML_LOG_INFO("%s", __func__);
    if (ptask->state != ADLAK_SUBMIT_STATE_PENDING) {
        ASSERT(0);
        return -1;
    }

    ////////////////////////pattching start////////////////////////////////////////
    adlak_wq_globalid_backup(padlak);
    // pattching to cmq buffer
    pcmq_buf                = padlak->cmq_buf_info.cmq_mm_info->cpu_addr;
    cmq_wr_offset_u32       = padlak->cmq_buf_info.cmq_wr_offset / sizeof(uint32_t);
    cmq_size_max_u32        = padlak->cmq_buf_info.total_size / sizeof(uint32_t);
    cmq_offset              = 0;
    ptask->cmq_buf_info.rpt = padlak->cmq_buf_info.cmq_wr_offset;

    psubmitask_base          = ptask->submit_tasks;
    psubmit_dep_fixup_base   = ptask->submit_dep_fixups;
    psubmit_reg_fixup_base   = ptask->submit_reg_fixups;
    psubmit_addr_fixups_base = ptask->submit_addr_fixups;
    config_base              = ptask->config;

    AML_LOG_DEFAULT("tasks_num=%u, ", ptask->submit_tasks_num);
    AML_LOG_DEFAULT("invoke id[%d-%d], ", ptask->invoke_start_idx, ptask->invoke_end_idx);
    AML_LOG_DEFAULT("dep_fixups_num=%u, ", ptask->dep_fixups_num);
    AML_LOG_DEFAULT("reg_fixups_num=%u, ", ptask->reg_fixups_num);
    AML_LOG_DEFAULT("config_size=%u, \n", ptask->config_size);
    start_pwe_flid = (psubmitask_base + ptask->invoke_start_idx)->start_pwe_flid + 1;
    start_pwx_flid = (psubmitask_base + ptask->invoke_start_idx)->start_pwx_flid + 1;
    start_rs_flid  = (psubmitask_base + ptask->invoke_start_idx)->start_rs_flid + 1;

    pwq->id_cur.start_id_pwe = pwq->id_cur.global_id_pwe;
    pwq->id_cur.start_id_pwx = pwq->id_cur.global_id_pwx;
    pwq->id_cur.start_id_rs  = pwq->id_cur.global_id_rs;
    for (task_idx = ptask->invoke_start_idx; task_idx <= ptask->invoke_end_idx; task_idx++) {
        psubmitask = psubmitask_base + task_idx;

#if ADLAK_DEBUG_CMQ_PATTTCHING_EN
        AML_LOG_INFO("task_idx=%u", task_idx);
        AML_LOG_DEFAULT("config_offset=%d, ", psubmitask->config_offset);
        AML_LOG_DEFAULT("config_size=%d \n", psubmitask->config_size);
#endif
        active_modules = 0;
        for (module_index = 0; module_index < ADLAK_PLATFORM_MODULE_COUNT; ++module_index) {
            if (psubmitask->active_modules & (1 << module_index)) {
                active_modules |= parser_active_modules[module_index];
            }
        }
        output_modules = 0;
        if (psubmitask->output_modules & (1 << ADLAK_PLATFORM_MODULE_PWE)) {
            output_modules |= PS_CMD_EXECUTE_OUTPUT_PWE_MASK;
        }

        if (psubmitask->output_modules & (1 << ADLAK_PLATFORM_MODULE_PWX)) {
            output_modules |= PS_CMD_EXECUTE_OUTPUT_PWX_MASK;
        }

        if (psubmitask->output_modules & (1 << ADLAK_PLATFORM_MODULE_RS)) {
            output_modules |= PS_CMD_EXECUTE_OUTPUT_RS_MASK;
        }

        // generate commands
        if (psubmitask->config_size % sizeof(uint32_t)) {
            AML_LOG_INFO("The config_size[%d] is not divisible by 4!", psubmitask->config_size);
            ASSERT(0);
        }
        cmq_size = cmq_offset;
        adlak_cmq_remain_space_check(
            padlak, sizeof(uint32_t) * (cmq_offset + cmq_wr_offset_u32),
            ADLAK_ALIGN(sizeof(uint32_t) * 6 + psubmitask->config_size, 16));

        pcmq_buf[adlak_get_cmq_addr_offset(cmq_offset++, cmq_wr_offset_u32, cmq_size_max_u32)] =
            adlak_cmd_get_sw_id(pwq);  // cmd_sw_id
        pcmq_buf[adlak_get_cmq_addr_offset(cmq_offset++, cmq_wr_offset_u32, cmq_size_max_u32)] =
            adlak_gen_dep_cmd(padlak->dependency_mode, pwq, psubmitask, psubmit_dep_fixup_base,
                              start_pwe_flid, start_pwx_flid,
                              start_rs_flid);  // cmd_dependcy
        pcmq_buf[adlak_get_cmq_addr_offset(cmq_offset++, cmq_wr_offset_u32, cmq_size_max_u32)] =
            PS_CMD_EXECUTE | output_modules;  // cmd_execute
        pcmq_buf[adlak_get_cmq_addr_offset(cmq_offset++, cmq_wr_offset_u32, cmq_size_max_u32)] =
            PS_CMD_CONFIG | active_modules;  // cmd_config
        // AML_LOG_INFO( "config start idx = %u", cmq_offset);
        cmq_offset_cfg = cmq_offset;
        if (adlak_get_cmq_addr_offset(cmq_offset + (psubmitask->config_size / sizeof(uint32_t)),
                                      cmq_wr_offset_u32, cmq_size_max_u32) >=
            adlak_get_cmq_addr_offset(cmq_offset, cmq_wr_offset_u32, cmq_size_max_u32))

        {
            adlak_os_memcpy((void *)&pcmq_buf[adlak_get_cmq_addr_offset(
                                cmq_offset, cmq_wr_offset_u32, cmq_size_max_u32)],
                            (void *)(config_base + psubmitask->config_offset),
                            psubmitask->config_size);  // config data
            cmq_offset += (psubmitask->config_size / sizeof(uint32_t));
        } else {
            cmq_size_tmp = (cmq_size_max_u32 - (cmq_offset + cmq_wr_offset_u32)) * sizeof(uint32_t);
            adlak_os_memcpy((void *)&pcmq_buf[adlak_get_cmq_addr_offset(
                                cmq_offset, cmq_wr_offset_u32, cmq_size_max_u32)],
                            (void *)(config_base + psubmitask->config_offset),
                            cmq_size_tmp);  // config data
            cmq_offset += (cmq_size_tmp / 4);
            adlak_os_memcpy((void *)&pcmq_buf[adlak_get_cmq_addr_offset(
                                cmq_offset, cmq_wr_offset_u32, cmq_size_max_u32)],
                            (void *)(config_base + psubmitask->config_offset + cmq_size_tmp),
                            psubmitask->config_size - cmq_size_tmp);  // config data
            cmq_offset += ((psubmitask->config_size - cmq_size_tmp) / sizeof(uint32_t));
        }

        // sAML_LOG_INFO( "config end idx = %u", cmq_offset);

        adlak_update_addr_fixups(psubmitask, psubmit_addr_fixups_base, pcmq_buf, cmq_offset_cfg,
                                 cmq_wr_offset_u32, cmq_size_max_u32);
        adlak_update_module_dependency(padlak->dependency_mode, pwq, psubmitask,
                                       psubmit_dep_fixup_base, start_pwe_flid, start_pwx_flid,
                                       start_rs_flid, pcmq_buf, cmq_offset_cfg, cmq_wr_offset_u32,
                                       cmq_size_max_u32);
        adlak_update_reg_fixups(padlak->dependency_mode, psubmitask, psubmit_reg_fixup_base,
                                pcmq_buf, cmq_offset_cfg, cmq_wr_offset_u32, cmq_size_max_u32);
        if (task_idx == ptask->invoke_end_idx) {
            pcmq_buf[adlak_get_cmq_addr_offset(cmq_offset++, cmq_wr_offset_u32, cmq_size_max_u32)] =
                PS_CMD_SET_TIME_STAMP | PS_CMD_TIME_STAMP_IRQ_MASK;  // cmd_time_stamp
        } else {
            pcmq_buf[adlak_get_cmq_addr_offset(cmq_offset++, cmq_wr_offset_u32, cmq_size_max_u32)] =
                PS_CMD_SET_TIME_STAMP;  // cmd_time_stamp
        }
        pcmq_buf[adlak_get_cmq_addr_offset(cmq_offset++, cmq_wr_offset_u32, cmq_size_max_u32)] =
            pwq->id_cur.global_time_stamp;  // time_stamp
#if 0
        pcmq_buf[adlak_get_cmq_addr_offset(cmq_offset++, cmq_wr_offset_u32, cmq_size_max_u32)] =
            PS_CMD_SET_FENCE | (7 << 20);  // TODO(shiwei.sun) only for hardware debug
#endif
        if (task_idx == ptask->invoke_end_idx) {
            ptask->time_stamp = pwq->id_cur.global_time_stamp;
        }
        pwq->id_cur.global_time_stamp++;
        cmq_size = (cmq_offset - cmq_size);
        nop_size = ADLAK_ALIGN(cmq_size, (16 / sizeof(uint32_t))) - cmq_size;
        while (nop_size) {
            pcmq_buf[adlak_get_cmq_addr_offset(cmq_offset++, cmq_wr_offset_u32, cmq_size_max_u32)] =
                PS_CMD_NOP;  // cmd_nop
            nop_size--;
        }
        // update states
        if (psubmitask->output_modules & (1 << ADLAK_PLATFORM_MODULE_PWE)) {
            pwq->id_cur.global_id_pwe++;
        }

        if (psubmitask->output_modules & (1 << ADLAK_PLATFORM_MODULE_PWX)) {
            pwq->id_cur.global_id_pwx++;
        }

        if (psubmitask->output_modules & (1 << ADLAK_PLATFORM_MODULE_RS)) {
            pwq->id_cur.global_id_rs++;
        }
    }

    ptask->cmq_buf_info.mm_info = padlak->cmq_buf_info.cmq_mm_info;

    ptask->cmq_buf_info.size = cmq_offset * sizeof(uint32_t);
    if (ptask->cmq_buf_info.size >= padlak->cmq_buf_info.size) {
        AML_LOG_ERR(
            "the maximum size limit is exceeded which the cmq buffer size.need (%d),max(%d)",
            ptask->cmq_buf_info.size, padlak->cmq_buf_info.size);
        ASSERT(0);
    }
    /*flush cmq*/
    adlak_flush_cache(padlak, padlak->cmq_buf_info.cmq_mm_info);

    /*flush tlbs*/
    if (padlak->mm->use_smmu) {
        padlak->mm->smmu_ops->smmu_tlb_flush_cache(padlak->mm);
    }
    //////////////////////pattching finished//////////////////////////
    ptask->state = ADLAK_SUBMIT_STATE_READY;
    // TODO(shiwei.sun) /*flush extern buffer,flush input&output buffer*/
    AML_LOG_DEBUG("%s End", __func__);
    return 0;
}
#endif

int adlak_queue_schedule_update(struct adlak_device *padlak, struct adlak_task **ptask_sch_cur) {
    struct adlak_workqueue *pwq   = &padlak->queue;
    struct adlak_task *     ptask = NULL;
    int                     ret   = 0;
    AML_LOG_INFO("%s", __func__);
    adlak_os_mutex_lock(&pwq->wq_mutex);
    if (list_empty(&pwq->ready_list)) {
        AML_LOG_WARN("ready_list is empty!,pwq->ready_num=%d", pwq->ready_num);
        *ptask_sch_cur = NULL;
        ret            = -1;
        pwq->ready_num--;
        goto end;
    }
    ptask = list_first_entry(&pwq->ready_list, typeof(struct adlak_task), head);
    if (ptask) {
        list_move_tail(&ptask->head, &pwq->scheduled_list);
        pwq->ready_num--;
        pwq->sched_num++;
        *ptask_sch_cur = ptask;
    } else {
        ret = -1;
    }
end:
    adlak_os_mutex_unlock(&pwq->wq_mutex);
    return ret;
}
/**
 * adlak_queue_schedule() - Schedule a queue inference.
 * @core:	adlak core.
 *
 * Pop the inference queue until either the queue is empty or an inference has
 * been successfully scheduled.
 */

static struct adlak_task *adlak_task_create(struct adlak_context *     context,
                                            struct adlak_network_desc *psubmit_desc) {
    struct adlak_task *  ptask;
    int                  ret;
    struct adlak_device *padlak = context->padlak;

    AML_LOG_INFO("%s", __func__);
    ptask =
        adlak_os_zalloc(sizeof(struct adlak_task) * 2,
                        ADLAK_GFP_KERNEL); /*the first buffer for backup,the second use for invoke*/
    if (!ptask) {
        return ADLAK_ERR_PTR(ERR(ENOMEM));
    }

    ptask->context = context;

#ifdef CONFIG_ADLAK_DEBUG_INNNER
    adlak_dbg_inner_reset(context);
    adlak_dbg_inner_update(context, "task_create");
#endif

    ptask->state = ADLAK_SUBMIT_STATE_IDLE;

    adlak_os_sema_init(&ptask->invoke_done, 1, 0);

    ptask->invoke_idx              = -1;
    ptask->net_id                  = context->net_id;
    psubmit_desc->net_register_idx = ptask->net_id;
#ifndef CONFIG_ADLA_FREERTOS
    ptask->submit_tasks = adlak_os_vmalloc(
        sizeof(struct adlak_submit_task) * psubmit_desc->tasks_num, ADLAK_GFP_KERNEL);
    if (!ptask->submit_tasks) {
        AML_LOG_ERR("alloc buffer for save submit data failed!");
        ret = ERR(ENOMEM);
        goto err_alloc_submit_task;
    }
    ptask->config = adlak_os_vmalloc(sizeof(uint8_t) * psubmit_desc->config_size, ADLAK_GFP_KERNEL);
    if (!ptask->config) {
        AML_LOG_ERR("alloc buffer for save submit data failed!");
        ret = ERR(ENOMEM);
        goto err_alloc_config;
    }
    if (psubmit_desc->dep_fixups_num) {
        ptask->submit_dep_fixups = adlak_os_vmalloc(
            sizeof(struct adlak_submit_dep_fixup) * psubmit_desc->dep_fixups_num, ADLAK_GFP_KERNEL);
        if (!ptask->submit_dep_fixups) {
            AML_LOG_ERR("alloc buffer for save submit data failed!");
            ret = ERR(ENOMEM);
            goto err_alloc_dep_fixups;
        }
    } else {
        ptask->submit_dep_fixups = NULL;
    }
    if (psubmit_desc->reg_fixups_num) {
        ptask->submit_reg_fixups = adlak_os_vmalloc(
            sizeof(struct adlak_submit_reg_fixup) * psubmit_desc->reg_fixups_num, ADLAK_GFP_KERNEL);
        if (!ptask->submit_reg_fixups) {
            AML_LOG_ERR("alloc buffer for save submit data failed!");
            ret = ERR(ENOMEM);
            goto err_alloc_reg_fixups;
        }
    } else {
        ptask->submit_reg_fixups = NULL;
    }

    /*****copy data from user*****/
    ret = copy_from_user((void *)ptask->config, (void __user *)(uintptr_t)psubmit_desc->config_va,
                         sizeof(uint8_t) * psubmit_desc->config_size);
    if (ret) {
        AML_LOG_ERR("copy from user failed!");
        ret = ERR(EFAULT);
        goto err_copy_from_user;
    }
    ret = copy_from_user((void *)ptask->submit_tasks,
                         (void __user *)(uintptr_t)psubmit_desc->tasks_va,
                         sizeof(struct adlak_submit_task) * psubmit_desc->tasks_num);
    if (ret) {
        AML_LOG_ERR("copy from user failed!");
        ret = ERR(EFAULT);
        goto err_copy_from_user;
    }
    if (ptask->submit_dep_fixups) {
        ret = copy_from_user((void *)ptask->submit_dep_fixups,
                             (void __user *)(uintptr_t)psubmit_desc->dep_fixups_va,
                             sizeof(struct adlak_submit_dep_fixup) * psubmit_desc->dep_fixups_num);
        if (ret) {
            AML_LOG_ERR("copy from user failed!");
            ret = ERR(EFAULT);
            goto err_copy_from_user;
        }
    }
    if (ptask->submit_reg_fixups) {
        ret = copy_from_user((void *)ptask->submit_reg_fixups,
                             (void __user *)(uintptr_t)psubmit_desc->reg_fixups_va,
                             sizeof(struct adlak_submit_reg_fixup) * psubmit_desc->reg_fixups_num);
        if (ret) {
            AML_LOG_ERR("copy from user failed!");
            ret = ERR(EFAULT);
            goto err_copy_from_user;
        }
    }

#else
    ptask->config = (void *)(uintptr_t)psubmit_desc->config_va;
    ptask->submit_tasks = (void *)(uintptr_t)psubmit_desc->tasks_va;
    ptask->submit_dep_fixups = (void *)(uintptr_t)psubmit_desc->dep_fixups_va;
    if (psubmit_desc->reg_fixups_num) {
        ptask->submit_reg_fixups = (void *)(uintptr_t)psubmit_desc->reg_fixups_va;
    } else {
        ptask->submit_reg_fixups = NULL;
    }
#endif

    AML_LOG_DEBUG("submit_dep_fixups dump...");
    adlak_debug_buf_dump((void *)ptask->submit_dep_fixups,
                         sizeof(struct adlak_submit_dep_fixup) * psubmit_desc->dep_fixups_num);
    AML_LOG_DEBUG("submit_reg_fixups dump...");
    adlak_debug_buf_dump((void *)ptask->submit_reg_fixups,
                         sizeof(struct adlak_submit_reg_fixup) * psubmit_desc->reg_fixups_num);

    ptask->config_size                 = psubmit_desc->config_size;
    ptask->submit_tasks_num            = psubmit_desc->tasks_num;
    ptask->dep_fixups_num              = psubmit_desc->dep_fixups_num;
    ptask->reg_fixups_num              = psubmit_desc->reg_fixups_num;
    ptask->profilling.profile_en       = psubmit_desc->profile_en;
    ptask->profilling.profile_iova     = psubmit_desc->profile_iova;
    ptask->profilling.profile_buf_size = psubmit_desc->profile_buf_size;
    ptask->padlak                      = padlak;

    INIT_LIST_HEAD(&ptask->head);
#ifdef CONFIG_ADLAK_DEBUG_INNNER
    adlak_dbg_inner_update(context, "task_create done");
#endif
    return ptask;

err_copy_from_user:
    if (ptask->submit_reg_fixups) {
        adlak_os_vfree(ptask->submit_reg_fixups);
    }
err_alloc_reg_fixups:
    if (ptask->submit_dep_fixups) {
        adlak_os_vfree(ptask->submit_dep_fixups);
    }
err_alloc_dep_fixups:
    adlak_os_vfree(ptask->config);

err_alloc_config:

    adlak_os_vfree(ptask->submit_tasks);
err_alloc_submit_task:
    adlak_os_free(ptask);
    return ADLAK_ERR_PTR(ret);
}
void adlak_task_destroy(struct adlak_task *ptask) {
    struct adlak_device *padlak = ptask->padlak;
    AML_LOG_DEBUG("%s", __func__);

#ifdef CONFIG_ADLAK_DEBUG_INNNER
    adlak_dbg_inner_update(ptask->context, "task destroy");
    adlak_dbg_inner_dump_info(ptask->context);
#endif

#ifndef CONFIG_ADLA_FREERTOS
    if (ptask->submit_reg_fixups) {
        adlak_os_vfree(ptask->submit_reg_fixups);
    }
    if (ptask->submit_dep_fixups) {
        adlak_os_vfree(ptask->submit_dep_fixups);
    }
    adlak_os_vfree(ptask->config);
    adlak_os_vfree(ptask->submit_tasks);
    if (ptask->submit_addr_fixups) {
        adlak_os_free(ptask->submit_addr_fixups);
    }
#endif

    adlak_os_sema_destroy(&ptask->invoke_done);

    ptask->submit_addr_fixups = NULL;
    adlak_os_free(ptask);
    padlak->all_task_num--;
    ptask = NULL;
}
static int adlak_invoke_pre_check(struct adlak_task *               ptask,
                                  struct adlak_network_invoke_desc *pinvoke_desc) {
    int                  ret    = 0;
    struct adlak_device *padlak = ptask->padlak;

    /* 1. whether the cmq's size exceeds the maximum limit?
    If the maximum size limit is exceeded,return error.*/

    uint32_t                  task_idx;
    uint32_t                  size_max, size_per_task;
    struct adlak_submit_task *psubmitask_base = NULL, *psubmitask = NULL;

    AML_LOG_DEBUG("%s", __func__);
    psubmitask_base = ptask->submit_tasks;
    size_max        = 0;
    for (task_idx = pinvoke_desc->start_idx; task_idx <= pinvoke_desc->end_idx; task_idx++) {
        psubmitask    = psubmitask_base + task_idx;
        size_per_task = 0;
        size_per_task += 6;
        size_per_task += (psubmitask->config_size / sizeof(uint32_t));

        size_per_task = ADLAK_ALIGN(size_per_task, 4);
        size_max += (size_per_task * sizeof(uint32_t));
    }
    size_max = ADLAK_ALIGN(size_max, 256);
    AML_LOG_INFO("invoke cmq need %d bytes", size_max);
    if (size_max >= padlak->cmq_buf_info.size) {
        ret = -1;
        AML_LOG_ERR(
            "the maximum size limit is exceeded which the cmq buffer size.need (%d),max(%d)",
            size_max, padlak->cmq_buf_info.size);
        goto err;
    }

    return 0;
err:
    return ret;
}
static struct adlak_task *adlak_get_task_from_context_by_netid(struct adlak_context *context,
                                                               int32_t               net_id) {
    bool               found = false;
    struct adlak_task *ptask_net, *ptask_net_tmp;

    if (!list_empty(&context->net_list)) {
        list_for_each_entry_safe(ptask_net, ptask_net_tmp, &context->net_list, head) {
            if ((ptask_net) && (net_id == ptask_net->net_id)) {
                found = true;
                break;
            }
        }
    }
    if (!found) {
        return NULL;
    }
    return ptask_net;
}
static struct adlak_task *adlak_invoke_create(struct adlak_context *            context,
                                              struct adlak_network_invoke_desc *pinvoke_desc) {
    struct adlak_task *ptask, *ptask_net;
    int                ret;
    AML_LOG_DEBUG("%s", __func__);
    ptask_net = adlak_get_task_from_context_by_netid(context, pinvoke_desc->net_register_idx);
    if (!ptask_net) {
        AML_LOG_ERR("not found network!");
        return ADLAK_ERR_PTR(ERR(ENXIO));
    }
    if (pinvoke_desc->start_idx < 0 || pinvoke_desc->end_idx >= ptask_net->submit_tasks_num ||
        pinvoke_desc->start_idx > pinvoke_desc->end_idx) {
        AML_LOG_ERR("invoke id is invalid!");
        return ADLAK_ERR_PTR(ERR(EINVAL));
    }

    ret = adlak_invoke_pre_check(ptask_net, pinvoke_desc);
    if (ret) {
        AML_LOG_ERR("adlak_invoke_pre_check fail!");
        return ADLAK_ERR_PTR(ERR(EINVAL));
    }
    if (pinvoke_desc->addr_fixups_num) {
#ifndef CONFIG_ADLA_FREERTOS
        if (ptask_net->submit_addr_fixups &&
            (ptask_net->addr_fixups_num != pinvoke_desc->addr_fixups_num)) {
            adlak_os_free(ptask_net->submit_addr_fixups);
            ptask_net->submit_addr_fixups = NULL;
        }
        ptask_net->addr_fixups_num = pinvoke_desc->addr_fixups_num;

        if (NULL == ptask_net->submit_addr_fixups) {
            ptask_net->submit_addr_fixups =
                adlak_os_malloc(sizeof(struct adlak_submit_addr_fixup) * ptask_net->addr_fixups_num,
                                ADLAK_GFP_KERNEL);
            if (!ptask_net->submit_addr_fixups) {
                AML_LOG_ERR("alloc buffer for save submit_addr_fixups failed!");
                return ADLAK_ERR_PTR(ERR(EINVAL));
            }
        }
        /*****copy data from user*****/
        ret = copy_from_user((void *)ptask_net->submit_addr_fixups,
                             (void __user *)(uintptr_t)pinvoke_desc->addr_fixups_va,
                             sizeof(struct adlak_submit_addr_fixup) * ptask_net->addr_fixups_num);
        if (ret) {
            AML_LOG_ERR("copy from user failed!");
            ptask = ADLAK_ERR_PTR(ERR(EFAULT));
            goto err_copy_from_user;
        }
#else

        ptask_net->addr_fixups_num = pinvoke_desc->addr_fixups_num;
        ptask_net->submit_addr_fixups = (void *)(uintptr_t)pinvoke_desc->addr_fixups_va;

#endif
    }

    ptask = (struct adlak_task *)ptask_net + 1;
    if (!ptask) {
        AML_LOG_ERR("adlak_os_zalloc fail!");
        ptask = ADLAK_ERR_PTR(ERR(ENOMEM));
        goto err_alloc_task;
    }

    adlak_os_memcpy((void *)ptask, (void *)ptask_net, sizeof(struct adlak_task));
    ptask->context = context;

    ++ptask_net->invoke_idx;
    if (ptask_net->invoke_idx < 0) {
        ptask_net->invoke_idx = 0;
    }
    ptask->invoke_idx                 = ptask_net->invoke_idx;
    pinvoke_desc->invoke_register_idx = ptask->invoke_idx;
    ptask->invoke_start_idx           = pinvoke_desc->start_idx;
    ptask->invoke_end_idx             = pinvoke_desc->end_idx;
    ptask->profilling.profile_wpt     = 0;
    ptask->profilling.profile_rpt     = 0;

    INIT_LIST_HEAD(&ptask->head);

    return ptask;
err_alloc_task:
err_copy_from_user:
#ifndef CONFIG_ADLA_FREERTOS
    if (ptask_net->submit_addr_fixups) {
        adlak_os_free(ptask_net->submit_addr_fixups);
    }
#endif
    ptask_net->submit_addr_fixups = NULL;
    return NULL;
}
void adlak_invoke_destroy(struct adlak_task *ptask) {
    AML_LOG_DEBUG("%s", __func__);
    if (ptask) {
        ptask->context->invoke_cnt--;
        ptask = NULL;
    }
}

// return deleted count
static int adlak_invoke_del_from_nosch_list(struct list_head *hd, int32_t net_id,
                                            int32_t invoke_id) {
    int                ret   = 0;
    struct adlak_task *ptask = NULL, *ptask_tmp = NULL;
    AML_LOG_DEBUG("%s", __func__);
    if (!list_empty(hd)) {
        list_for_each_entry_safe(ptask, ptask_tmp, hd, head) {
            if (ptask) {
                if ((net_id != -1) && (net_id != ptask->net_id)) {
                    continue;
                }
                if ((invoke_id != -1) && (invoke_id != ptask->invoke_idx)) {
                    continue;
                }
                ret++;
                list_del(&ptask->head);
                adlak_invoke_destroy(ptask);
            }
        }
    }
    return ret;
}

// return deleted count
static int adlak_invoke_del_from_sch_list(struct list_head *hd, int32_t net_id, int32_t invoke_id) {
    int                ret   = 0;
    struct adlak_task *ptask = NULL, *ptask_tmp = NULL;
    AML_LOG_DEBUG("%s", __func__);
    if (!list_empty(hd)) {
        list_for_each_entry_safe(ptask, ptask_tmp, hd, head) {
            if (ptask) {
                if ((net_id != -1) && (net_id != ptask->net_id)) {
                    continue;
                }
                if ((invoke_id != -1) && (invoke_id != ptask->invoke_idx)) {
                    continue;
                }
                ret++;
                ptask->flag = ptask->flag | ADLAK_TASK_CANCELED;
            }
        }
    }
    return ret;
}
static int adlak_invoke_del_with_invokeid(struct adlak_device *padlak, int32_t net_id,
                                          int32_t invoke_id) {
    /*
   - if in pendding list,
     - mutex lock;
     - remove from it,and release buffer
     - mutex unlock
   - if in ready list,
     - mutex lock;
     - remove from it,and release buffer
     - roll-back the global idxs;
     - mutex unlock
     - call **"queue_schedule"**
   - if in schedule list,set invalid flag.
   - if in done list ? del or **TODO**
     */

    int                     ret         = 0;
    struct adlak_workqueue *pwq         = &padlak->queue;
    bool                    net_is_used = false;
    adlak_os_mutex_lock(&pwq->wq_mutex);
    AML_LOG_DEBUG("%s", __func__);
    AML_LOG_INFO("invoke del: net_id=%d,invoke_id=%d.", net_id, invoke_id);

    ret = adlak_invoke_del_from_nosch_list(&pwq->finished_list, net_id, invoke_id);
    if (ret > 0) {
        ret = 0;
        if (invoke_id >= 0) {
            goto end;
        }
    }
    ret = adlak_invoke_del_from_nosch_list(&pwq->ready_list, net_id, invoke_id);
    if (0 < ret) {
        ret = 0;
        adlak_wq_globalid_rollback(padlak);
        pwq->ready_num = pwq->ready_num - ret;
        if (invoke_id >= 0) {
            goto end;
        }
    }
    ret = adlak_invoke_del_from_nosch_list(&pwq->pending_list, net_id, invoke_id);
    if (0 < ret) {
        ret              = 0;
        pwq->pending_num = pwq->pending_num - ret;
        if (invoke_id >= 0) {
            goto end;
        }
    }

    ret = adlak_invoke_del_from_sch_list(&pwq->scheduled_list, net_id, invoke_id);
    if (0 < ret) {
        ret         = -1;
        net_is_used = true;
    }
end:
    adlak_os_mutex_unlock(&pwq->wq_mutex);
    return ret;
}

int adlak_invoke_del_all(struct adlak_device *padlak, int32_t net_id) {
    AML_LOG_DEBUG("%s", __func__);
    return adlak_invoke_del_with_invokeid(padlak, net_id, -1);
}

int adlak_move_ready_to_pendding_list(struct adlak_device *padlak) {
    int                     ret   = 0;
    struct adlak_workqueue *pwq   = &padlak->queue;
    struct adlak_task *     ptask = NULL, *ptask_tmp = NULL;
    AML_LOG_INFO("%s", __func__);
    if (!list_empty(&pwq->ready_list)) {
        list_for_each_entry_safe(ptask, ptask_tmp, &pwq->ready_list, head) {
            if (ptask) {
                ret = 0;
                list_move(&ptask->head, &pwq->pending_list);
                pwq->ready_num--;
                pwq->pending_num++;
            }
        }
    }
    AML_LOG_INFO("pwq->pending_num = %d\n", pwq->pending_num);
    return ret;
}
static int adlak_net_attach_to_queue(struct adlak_context *     context,
                                     struct adlak_network_desc *psubmit_desc) {
    int                  ret    = 0;
    struct adlak_device *padlak = context->padlak;
    // struct adlak_workqueue *pwq    = &padlak->queue;
    struct adlak_task *ptask = NULL;

    AML_LOG_DEBUG("%s", __func__);
    /*create task*/
    ptask = adlak_task_create(context, psubmit_desc);
    if (!ptask) {
        AML_LOG_ERR("adlak task create fail!");
        ret = -1;
        goto err;
    }

    /*add list to workqueue*/

    if (ret) {
        AML_LOG_ERR("mutex lock fail!");
        ret = -1;
        goto err;
    }

    adlak_os_mutex_lock(&context->context_mutex);
    list_add_tail(&ptask->head, &context->net_list);
    ptask->state = ADLAK_SUBMIT_STATE_PENDING;
    padlak->all_task_num++;
    adlak_os_mutex_unlock(&context->context_mutex);

    return 0;
err:
    return ret;
}

static int adlak_invoke_add_queue(struct adlak_context *            context,
                                  struct adlak_network_invoke_desc *pinvoke_desc) {
    int                     ret    = 0;
    struct adlak_device *   padlak = context->padlak;
    struct adlak_workqueue *pwq    = &padlak->queue;
    struct adlak_task *     ptask  = NULL;
    // struct adlak_network_desc *psubmit_desc = NULL;
    AML_LOG_INFO("%s net_id[%d]", __func__, context->net_id);

    /*create task*/

    ptask = adlak_invoke_create(context, pinvoke_desc);
    if (ADLAK_IS_ERR_OR_NULL(ptask)) {
        AML_LOG_ERR("adlak task create fail!");
        ret = -1;
        goto err;
    }

    /*add list to workqueue*/

    if (ret) {
        AML_LOG_ERR("mutex lock fail!");
        ret = -1;
        goto err;
    }
    ptask->state = ADLAK_SUBMIT_STATE_PENDING;

    context->state = CONTEXT_STATE_USED;
    context->invoke_cnt++;

    ptask->hw_stat.hw_info = padlak->hw_info;

    adlak_os_mutex_lock(&pwq->wq_mutex);
    list_add_tail(&ptask->head, &pwq->pending_list);
    pwq->pending_num++;

    AML_LOG_INFO("pend++,pwq->pending_num = %d\n", pwq->pending_num);
    adlak_os_mutex_unlock(&pwq->wq_mutex);

    return 0;
err:
    return ret;
}

static int adlak_net_register_pre_check(struct adlak_context *     context,
                                        struct adlak_network_desc *psubmit_desc) {
    int                  ret    = 0;
    struct adlak_device *padlak = context->padlak;
    AML_LOG_DEBUG("%s", __func__);
    /*1.if Suppose the maximum count has been reached,then **return err***/
    if (padlak->all_task_num >= padlak->all_task_num_max) {
        ret = -1;
        AML_LOG_ERR("Suppose the maximum count has been reached,Please try again later.");
        goto err;
    }
#if 0
  /* 2. check cmq's size
  If the maximum size limit is exceeded,return error.*/

  uint32_t task_num = psubmit_desc->tasks_num;
  uint32_t task_idx;
  uint32_t size_max, size_per_task;

  struct adlak_submit_task *psubmitask_base = NULL, *psubmitask = NULL;

  psubmitask_base = adlak_os_zalloc(sizeof(struct adlak_submit_task) * psubmit_desc->tasks_num, ADLAK_GFP_KERNEL);
  copy_from_user((void *)psubmitask_base, psubmit_desc->tasks_va,
                 sizeof(struct adlak_submit_task) * psubmit_desc->tasks_num);
  size_max = 0;
  for (task_idx = 0; task_idx < task_num; task_idx++) {
    psubmitask = psubmitask_base + task_idx;

    // AML_LOG_DEBUG( "task_idx=%d", task_idx);
    // AML_LOG_DEBUG( "config_offset=%d", psubmitask->config_offset);
    // AML_LOG_DEBUG( "config_size=%d", psubmitask->config_size);
    // AML_LOG_DEBUG( "pwe_dep_info_index=%d", psubmitask->pwe_dep_info_index);
    // AML_LOG_DEBUG( "pwx_dep_info_index=%d", psubmitask->pwx_dep_info_index);
    // AML_LOG_DEBUG( "rs_dep_info_index=%d", psubmitask->rs_dep_info_index);
    // AML_LOG_DEBUG( "start_pwe_flid=%d", psubmitstart_pwe_flid);
    // AML_LOG_DEBUG( "start_pwx_flid=%d", psubmitstart_pwx_flid);
    // AML_LOG_DEBUG( "start_rs_flid=%d", psubmitstart_rs_flid);
    size_per_task = 0;
    size_per_task += 6;
    // if (psubmitask->config_size < sizeof(uint32_t)) {
    //   psubmitask->config_size = sizeof(uint32_t);
    // }
    size_per_task += (psubmitask->config_size / sizeof(uint32_t));

    size_per_task = ADLAK_ALIGN(size_per_task, 4);
    size_max += (size_per_task * sizeof(uint32_t));
  }
  adlak_os_free(psubmitask_base);

  if (size_max >= padlak->cmq_buf_info.size) {
    ret = -1;
    AML_LOG_ERR( "the maximum size limit is exceeded which the cmq buffer size.need (%d),max(%d)",
        size_max, padlak->cmq_buf_info.size);
    goto err;
  }
#endif
    return 0;
err:
    return ret;
}

int adlak_net_register_request(struct adlak_context *     context,
                               struct adlak_network_desc *psubmit_desc) {
    int ret = 0;
    AML_LOG_INFO("%s", __func__);
#ifndef CONFIG_ADLA_FREERTOS
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
    if (!adlak_access_ok((void __user *)(uintptr_t)psubmit_desc->tasks_va,
                         sizeof(struct adlak_submit_task) * psubmit_desc->tasks_num))
        return ERR(EFAULT);
    if (!adlak_access_ok((void __user *)(uintptr_t)psubmit_desc->dep_fixups_va,
                         sizeof(struct adlak_submit_dep_fixup) * psubmit_desc->dep_fixups_num))
        return ERR(EFAULT);
    if (!adlak_access_ok((void __user *)(uintptr_t)psubmit_desc->config_va,
                         sizeof(uint8_t) * psubmit_desc->config_size))
        return ERR(EFAULT);

#else
    if (!adlak_access_ok(VERIFY_READ, (void __user *)((uintptr_t)psubmit_desc->tasks_va),
                         sizeof(struct adlak_submit_task) * psubmit_desc->tasks_num))
        return ERR(EFAULT);
    if (!adlak_access_ok(VERIFY_READ, (void __user *)(uintptr_t)psubmit_desc->dep_fixups_va,
                         sizeof(struct adlak_submit_dep_fixup) * psubmit_desc->dep_fixups_num))
        return ERR(EFAULT);
    if (!adlak_access_ok(VERIFY_READ, (void __user *)(uintptr_t)psubmit_desc->config_va,
                         sizeof(uint8_t) * psubmit_desc->config_size))
        return ERR(EFAULT);
#endif

#endif

    ret = adlak_net_register_pre_check(context, psubmit_desc);
    if (ret) {
        goto err;
    }
    /*2.add to workqueue*/
    ret = adlak_net_attach_to_queue(context, psubmit_desc);
    if (ret) {
        goto err;
    }

    /*flush context memory DMA_TO_DEVICE*/
    adlak_context_flush_cache(context);

    return 0;
err:
    return ret;
}
int adlak_net_unregister_request(struct adlak_context *         context,
                                 struct adlak_network_del_desc *submit_del) {
    int                  ret    = 0;
    struct adlak_device *padlak = context->padlak;

    ret = adlak_invoke_del_with_invokeid(padlak, submit_del->net_register_idx, -1);
    if (0 == ret) {
        adlak_net_dettach_by_id(context, submit_del->net_register_idx);
    }
    return 0;
}

int adlak_invoke_request(struct adlak_context *            context,
                         struct adlak_network_invoke_desc *pinvoke_desc) {
    int                     ret    = 0;
    struct adlak_device *   padlak = NULL;
    struct adlak_workqueue *pwq    = NULL;
    // struct adlak_device *padlak = context->padlak;
    AML_LOG_INFO("%s", __func__);
    AML_LOG_DEBUG("net_id=%d", pinvoke_desc->net_register_idx);
    AML_LOG_DEBUG("invoke_id=%d", pinvoke_desc->invoke_register_idx);
    AML_LOG_DEBUG("invoke start_idx=%d", pinvoke_desc->start_idx);
    AML_LOG_DEBUG("invoke end_idx=%d", pinvoke_desc->end_idx);
#ifdef CONFIG_ADLAK_DEBUG_INNNER
    adlak_dbg_inner_update(context, "invoke_request");
#endif
#ifndef CONFIG_ADLA_FREERTOS
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
    if (!adlak_access_ok((void __user *)(uintptr_t)pinvoke_desc->addr_fixups_va,
                         sizeof(struct adlak_submit_addr_fixup) * pinvoke_desc->addr_fixups_num))
        return ERR(EFAULT);

#else
    if (!adlak_access_ok(VERIFY_READ, (void __user *)((uintptr_t)pinvoke_desc->addr_fixups_va),
                         sizeof(struct adlak_submit_addr_fixup) * pinvoke_desc->addr_fixups_num))
        return ERR(EFAULT);
#endif

#endif

    adlak_os_mutex_lock(&context->context_mutex);
    ret = adlak_invoke_add_queue(context, pinvoke_desc);

    adlak_os_mutex_unlock(&context->context_mutex);

    padlak = context->padlak;
    pwq    = &padlak->queue;

#ifdef CONFIG_ADLAK_DEBUG_INNNER
    adlak_dbg_inner_update(context, "invoke_request done");
#endif
    pwq->dev_inference.dmp_timeout = 0;
    adlak_os_sema_give(pwq->wk_update);
    adlak_os_thread_yield();

    if (ret) {
        goto err;
    }
    return 0;
err:
    return ret;
}
int adlak_uninvoke_request(struct adlak_context *                context,
                           struct adlak_network_invoke_del_desc *pinvoke_del) {
    int                  ret    = 0;
    struct adlak_device *padlak = context->padlak;
#ifdef CONFIG_ADLAK_DEBUG_INNNER
    adlak_dbg_inner_update(context, "uninvoke_request");
#endif

    ret = adlak_invoke_del_with_invokeid(padlak, pinvoke_del->net_register_idx,
                                         pinvoke_del->invoke_register_idx);
#ifdef CONFIG_ADLAK_DEBUG_INNNER
    adlak_dbg_inner_update(context, "uninvoke_request done");
#endif
    return 0;
}
int adlak_get_status_request(struct adlak_context *context, struct adlak_get_stat_desc *stat_desc) {
    struct adlak_device *   padlak = context->padlak;
    struct adlak_workqueue *pwq    = &padlak->queue;
    struct list_head *      hd;
    struct adlak_task *     ptask = NULL, *ptask_tmp = NULL;
    AML_LOG_DEBUG("%s", __func__);
#ifdef CONFIG_ADLAK_DEBUG_INNNER
    adlak_dbg_inner_update(context, "status_request");
#endif
    hd                   = &pwq->finished_list;
    stat_desc->ret_state = 1;  // invoke busy

    if (!list_empty(hd)) {
        list_for_each_entry_safe(ptask, ptask_tmp, hd, head) {
            if (ptask) {
                if (stat_desc->net_register_idx != ptask->net_id) {
                    continue;
                }
                if (stat_desc->invoke_register_idx != ptask->invoke_idx) {
                    continue;
                }
                stat_desc->profile_en       = ptask->profilling.profile_en;
                stat_desc->invoke_time_us   = ptask->profilling.time_elapsed_us;
                stat_desc->start_idx        = ptask->invoke_start_idx;
                stat_desc->end_idx          = ptask->invoke_end_idx;
                stat_desc->profile_rpt      = ptask->profilling.profile_rpt;
                stat_desc->axi_freq_cur     = padlak->clk_axi_freq_real;
                stat_desc->core_freq_cur    = padlak->clk_core_freq_real;
                stat_desc->mem_alloced_umd  = context->mem_alloced;
                stat_desc->mem_alloced_base = padlak->mm->usage.mem_alloced_kmd;
                stat_desc->mem_pool_size    = padlak->mm->usage.mem_pools_size;
                stat_desc->mem_pool_used =
                    padlak->mm->usage.mem_alloced_kmd + padlak->mm->usage.mem_alloced_umd;
                stat_desc->efficiency = adlak_dmp_get_efficiency(padlak);
                if (ADLAK_SUBMIT_STATE_FINISHED == ptask->state) {
                    stat_desc->ret_state = 0;
                } else {
                    stat_desc->ret_state = -3;  // TODO
                }
                break;
            }
        }
    }
    return 0;
}

int adlak_profile_config(struct adlak_context *         context,
                         struct adlak_profile_cfg_desc *profile_cfg) {
    int                ret = 0;
    struct adlak_task *ptask_net;
    // struct adlak_device *padlak = context->padlak;
    AML_LOG_DEBUG("%s", __func__);
    AML_LOG_DEBUG("net_idx=%d", profile_cfg->net_register_idx);
    AML_LOG_INFO("profile_en=%d", profile_cfg->profile_en);
    AML_LOG_INFO("profile_iova=0x%lX", (uintptr_t)profile_cfg->profile_iova);
    AML_LOG_INFO("profile_buf_size=%lu KByte", (uintptr_t)(profile_cfg->profile_buf_size / 1024));

    adlak_os_mutex_lock(&context->context_mutex);
    ptask_net = adlak_get_task_from_context_by_netid(context, profile_cfg->net_register_idx);
    if (!ptask_net) {
        AML_LOG_ERR("not found network!");
        profile_cfg->errcode = 1;
        ret                  = -1;
        goto err;
    }
    ptask_net->profilling.profile_en       = profile_cfg->profile_en;
    ptask_net->profilling.profile_iova     = profile_cfg->profile_iova;
    ptask_net->profilling.profile_buf_size = profile_cfg->profile_buf_size;
    adlak_os_mutex_unlock(&context->context_mutex);
    if (ret) {
        profile_cfg->errcode = 1;
        ret                  = -1;
        goto err;
    }
    profile_cfg->errcode = 0;
    return 0;
err:
    return ret;
}

int adlak_queue_update_task_state(struct adlak_device *padlak, struct adlak_task *ptask) {
    struct adlak_workqueue *pwq = &padlak->queue;
    // struct adlak_task *     ptask   = NULL;
    struct adlak_context *context = NULL;
    // bool                 found   = false;

    struct adlak_hw_stat *phw_stat = &ptask->hw_stat;
    AML_LOG_DEBUG("%s", __func__);

    ASSERT(ptask);
    if (ADLAK_SUBMIT_STATE_FINISHED != ptask->state && ADLAK_SUBMIT_STATE_FAIL != ptask->state) {
        return -1;
    }

    adlak_status_dump(phw_stat);
    {
        // adlak_context_invalid_cache(ptask->context);

        mb();
        pwq->sched_num--;
        context = ptask->context;
        if (ptask->flag & ADLAK_TASK_CANCELED) {
            AML_LOG_DEBUG("delete the task had canceled which the owner's net_id is %d.",
                          ptask->net_id);
            list_del(&ptask->head);
            adlak_invoke_destroy(ptask);

        } else {
            /*move to finished queue*/
            list_move_tail(&ptask->head, &pwq->finished_list);

            pwq->finished_num++;
#ifdef CONFIG_ADLAK_DEBUG_INNNER
            adlak_dbg_inner_update(context, "submit_done");
#endif

            adlak_to_umd_sinal_give(context->wait); /*give signal to umd*/
        }

        if (CONTEXT_STATE_CLOSED == context->state) {
            adlak_os_sema_give(context->ctx_idle);
        }

        adlak_os_sema_give(ptask->invoke_done);
    }
    return 0;
}

#ifndef CONFIG_ADLAK_PRE_PATCH
int adlak_submit_patch_and_exec(struct adlak_task *ptask) {
    struct adlak_device *           padlak = ptask->padlak;
    struct adlak_workqueue *        pwq    = &padlak->queue;
    uint32_t                        cmq_offset, cmq_offset_cfg;
    uint32_t                        cmq_size;
    struct adlak_submit_task *      psubmitask_base = NULL, *psubmitask = NULL;
    uint32_t                        task_idx, module_index, active_modules = 0, output_modules = 0;
    struct adlak_submit_dep_fixup * psubmit_dep_fixup_base   = NULL;
    struct adlak_submit_reg_fixup * psubmit_reg_fixup_base   = NULL;
    struct adlak_submit_addr_fixup *psubmit_addr_fixups_base = NULL;
    uint8_t *                       config_base              = NULL;
    uint32_t *                      pcmq_buf                 = NULL;
    uint32_t                        nop_size, cmq_size_max_u32, cmq_wr_offset_u32, cmq_size_tmp;
    int32_t                         start_pwe_flid, start_pwx_flid, start_rs_flid;
    uint32_t                        cmq_wr_offset, invoke_num;

    const uint32_t parser_active_modules[ADLAK_PLATFORM_MODULE_COUNT] = {
        PS_CMD_CONFIG_RS_MASK,      // ADLAK_PLATFORM_MODULE_RS
        PS_CMD_CONFIG_RS_CAT_MASK,  // ADLAK_PLATFORM_MODULE_RS_CAT
        PS_CMD_CONFIG_MC_MASK,      // ADLAK_PLATFORM_MODULE_MC
        PS_CMD_CONFIG_DMCF_MASK,    // ADLAK_PLATFORM_MODULE_DMCF
        PS_CMD_CONFIG_DMCW_MASK,    // ADLAK_PLATFORM_MODULE_DMCW
        PS_CMD_CONFIG_PE_MASK,      // ADLAK_PLATFORM_MODULE_PE
        PS_CMD_CONFIG_PE_LUT_MASK,  // ADLAK_PLATFORM_MODULE_PE_LUT
        PS_CMD_CONFIG_DW_MASK,      // ADLAK_PLATFORM_MODULE_DW
        PS_CMD_CONFIG_DMDF_MASK,    // ADLAK_PLATFORM_MODULE_DMDF
        PS_CMD_CONFIG_DMDW_MASK,    // ADLAK_PLATFORM_MODULE_DMDW
        PS_CMD_CONFIG_PX_MASK,      // ADLAK_PLATFORM_MODULE_PX
        PS_CMD_CONFIG_PX_LUT_MASK,  // ADLAK_PLATFORM_MODULE_PX_LUT
        PS_CMD_CONFIG_PWE_MASK,     // ADLAK_PLATFORM_MODULE_PWE
        PS_CMD_CONFIG_PWX_MASK      // ADLAK_PLATFORM_MODULE_PWX
    };
    AML_LOG_INFO("%s", __func__);
    if (ptask->state != ADLAK_SUBMIT_STATE_PENDING) {
        ASSERT(0);
        return -1;
    }

    if (padlak->mm->use_smmu) {
        /*flush tlbs*/
        padlak->mm->smmu_ops->smmu_tlb_flush_cache(padlak->mm);
        /*invalid tlbs*/
        padlak->mm->smmu_ops->smmu_tlb_invalidate(padlak->mm);
    }
#ifdef CONFIG_ADLAK_DEBUG_INNNER
    adlak_dbg_inner_update(ptask->context, "submit_start");
#endif

    ////////////////////////pattching start////////////////////////////////////////
    adlak_wq_globalid_backup(padlak);
    // pattching to cmq buffer
    pcmq_buf                = padlak->cmq_buf_info.cmq_mm_info->cpu_addr;
    cmq_wr_offset_u32       = padlak->cmq_buf_info.cmq_wr_offset / sizeof(uint32_t);
    cmq_size_max_u32        = padlak->cmq_buf_info.total_size / sizeof(uint32_t);
    cmq_offset              = 0;
    ptask->cmq_buf_info.rpt = padlak->cmq_buf_info.cmq_wr_offset;

    psubmitask_base          = ptask->submit_tasks;
    psubmit_dep_fixup_base   = ptask->submit_dep_fixups;
    psubmit_reg_fixup_base   = ptask->submit_reg_fixups;
    psubmit_addr_fixups_base = ptask->submit_addr_fixups;
    config_base              = ptask->config;

    AML_LOG_DEFAULT("tasks_num=%u, ", ptask->submit_tasks_num);
    AML_LOG_DEFAULT("invoke id[%d-%d], ", ptask->invoke_start_idx, ptask->invoke_end_idx);
    AML_LOG_DEFAULT("dep_fixups_num=%u, ", ptask->dep_fixups_num);
    AML_LOG_DEFAULT("reg_fixups_num=%u, ", ptask->reg_fixups_num);
    AML_LOG_DEFAULT("config_size=%u, \n", ptask->config_size);
    start_pwe_flid = (psubmitask_base + ptask->invoke_start_idx)->start_pwe_flid + 1;
    start_pwx_flid = (psubmitask_base + ptask->invoke_start_idx)->start_pwx_flid + 1;
    start_rs_flid  = (psubmitask_base + ptask->invoke_start_idx)->start_rs_flid + 1;

    pwq->id_cur.start_id_pwe = pwq->id_cur.global_id_pwe;
    pwq->id_cur.start_id_pwx = pwq->id_cur.global_id_pwx;
    pwq->id_cur.start_id_rs  = pwq->id_cur.global_id_rs;

    adlak_profile_start(padlak, &ptask->profilling, ptask->invoke_start_idx);
    ptask->state = ADLAK_SUBMIT_STATE_RUNNING;

    for (task_idx = ptask->invoke_start_idx; task_idx <= ptask->invoke_end_idx; task_idx++) {
        psubmitask = psubmitask_base + task_idx;

#if ADLAK_DEBUG_CMQ_PATTTCHING_EN
        AML_LOG_INFO("task_idx=%u", task_idx);
        AML_LOG_DEFAULT("config_offset=%d, ", psubmitask->config_offset);
        AML_LOG_DEFAULT("config_size=%d \n", psubmitask->config_size);
#endif
        active_modules = 0;
        for (module_index = 0; module_index < ADLAK_PLATFORM_MODULE_COUNT; ++module_index) {
            if (psubmitask->active_modules & (1 << module_index)) {
                active_modules |= parser_active_modules[module_index];
            }
        }
        output_modules = 0;
        if (psubmitask->output_modules & (1 << ADLAK_PLATFORM_MODULE_PWE)) {
            output_modules |= PS_CMD_EXECUTE_OUTPUT_PWE_MASK;
        }

        if (psubmitask->output_modules & (1 << ADLAK_PLATFORM_MODULE_PWX)) {
            output_modules |= PS_CMD_EXECUTE_OUTPUT_PWX_MASK;
        }

        if (psubmitask->output_modules & (1 << ADLAK_PLATFORM_MODULE_RS)) {
            output_modules |= PS_CMD_EXECUTE_OUTPUT_RS_MASK;
        }

        // generate commands
        if (psubmitask->config_size % sizeof(uint32_t)) {
            AML_LOG_INFO("The config_size[%d] is not divisible by 4!", psubmitask->config_size);
            ASSERT(0);
        }
        cmq_size = cmq_offset;
        adlak_cmq_remain_space_check(
            padlak, sizeof(uint32_t) * (cmq_offset + cmq_wr_offset_u32),
            ADLAK_ALIGN(sizeof(uint32_t) * 6 + psubmitask->config_size, 16));

        pcmq_buf[adlak_get_cmq_addr_offset(cmq_offset++, cmq_wr_offset_u32, cmq_size_max_u32)] =
            adlak_cmd_get_sw_id(pwq);  // cmd_sw_id
        pcmq_buf[adlak_get_cmq_addr_offset(cmq_offset++, cmq_wr_offset_u32, cmq_size_max_u32)] =
            adlak_gen_dep_cmd(padlak->dependency_mode, pwq, psubmitask, psubmit_dep_fixup_base,
                              start_pwe_flid, start_pwx_flid,
                              start_rs_flid);  // cmd_dependcy
        pcmq_buf[adlak_get_cmq_addr_offset(cmq_offset++, cmq_wr_offset_u32, cmq_size_max_u32)] =
            PS_CMD_EXECUTE | output_modules;  // cmd_execute
        pcmq_buf[adlak_get_cmq_addr_offset(cmq_offset++, cmq_wr_offset_u32, cmq_size_max_u32)] =
            PS_CMD_CONFIG | active_modules;  // cmd_config
        // AML_LOG_INFO( "config start idx = %u", cmq_offset);
        cmq_offset_cfg = cmq_offset;
        if (adlak_get_cmq_addr_offset(cmq_offset + (psubmitask->config_size / sizeof(uint32_t)),
                                      cmq_wr_offset_u32, cmq_size_max_u32) >=
            adlak_get_cmq_addr_offset(cmq_offset, cmq_wr_offset_u32, cmq_size_max_u32))

        {
            adlak_os_memcpy((void *)&pcmq_buf[adlak_get_cmq_addr_offset(
                                cmq_offset, cmq_wr_offset_u32, cmq_size_max_u32)],
                            (void *)(config_base + psubmitask->config_offset),
                            psubmitask->config_size);  // config data
            cmq_offset += (psubmitask->config_size / sizeof(uint32_t));
        } else {
            cmq_size_tmp = (cmq_size_max_u32 - (cmq_offset + cmq_wr_offset_u32)) * sizeof(uint32_t);
            adlak_os_memcpy((void *)&pcmq_buf[adlak_get_cmq_addr_offset(
                                cmq_offset, cmq_wr_offset_u32, cmq_size_max_u32)],
                            (void *)(config_base + psubmitask->config_offset),
                            cmq_size_tmp);  // config data
            cmq_offset += (cmq_size_tmp / 4);
            adlak_os_memcpy((void *)&pcmq_buf[adlak_get_cmq_addr_offset(
                                cmq_offset, cmq_wr_offset_u32, cmq_size_max_u32)],
                            (void *)(config_base + psubmitask->config_offset + cmq_size_tmp),
                            psubmitask->config_size - cmq_size_tmp);  // config data
            cmq_offset += ((psubmitask->config_size - cmq_size_tmp) / sizeof(uint32_t));
        }

        // sAML_LOG_INFO( "config end idx = %u", cmq_offset);

        adlak_update_addr_fixups(psubmitask, psubmit_addr_fixups_base, pcmq_buf, cmq_offset_cfg,
                                 cmq_wr_offset_u32, cmq_size_max_u32);
        adlak_update_module_dependency(padlak->dependency_mode, pwq, psubmitask,
                                       psubmit_dep_fixup_base, start_pwe_flid, start_pwx_flid,
                                       start_rs_flid, pcmq_buf, cmq_offset_cfg, cmq_wr_offset_u32,
                                       cmq_size_max_u32);
        adlak_update_reg_fixups(padlak->dependency_mode, psubmitask, psubmit_reg_fixup_base,
                                pcmq_buf, cmq_offset_cfg, cmq_wr_offset_u32, cmq_size_max_u32);
        if (task_idx == ptask->invoke_end_idx) {
            pcmq_buf[adlak_get_cmq_addr_offset(cmq_offset++, cmq_wr_offset_u32, cmq_size_max_u32)] =
                PS_CMD_SET_TIME_STAMP | PS_CMD_TIME_STAMP_IRQ_MASK;  // cmd_time_stamp
        } else {
            pcmq_buf[adlak_get_cmq_addr_offset(cmq_offset++, cmq_wr_offset_u32, cmq_size_max_u32)] =
                PS_CMD_SET_TIME_STAMP;  // cmd_time_stamp
        }
        pcmq_buf[adlak_get_cmq_addr_offset(cmq_offset++, cmq_wr_offset_u32, cmq_size_max_u32)] =
            pwq->id_cur.global_time_stamp;  // time_stamp
#if 0
        pcmq_buf[adlak_get_cmq_addr_offset(cmq_offset++, cmq_wr_offset_u32, cmq_size_max_u32)] =
            PS_CMD_SET_FENCE | (7 << 20);  // TODO(shiwei.sun) only for hardware debug
#endif
        if (task_idx == ptask->invoke_end_idx) {
            ptask->time_stamp = pwq->id_cur.global_time_stamp;
        }
        pwq->id_cur.global_time_stamp++;
        cmq_size = (cmq_offset - cmq_size);
        nop_size = ADLAK_ALIGN(cmq_size, (16 / sizeof(uint32_t))) - cmq_size;
        while (nop_size) {
            pcmq_buf[adlak_get_cmq_addr_offset(cmq_offset++, cmq_wr_offset_u32, cmq_size_max_u32)] =
                PS_CMD_NOP;  // cmd_nop
            nop_size--;
        }
        // update states
        if (psubmitask->output_modules & (1 << ADLAK_PLATFORM_MODULE_PWE)) {
            pwq->id_cur.global_id_pwe++;
        }

        if (psubmitask->output_modules & (1 << ADLAK_PLATFORM_MODULE_PWX)) {
            pwq->id_cur.global_id_pwx++;
        }

        if (psubmitask->output_modules & (1 << ADLAK_PLATFORM_MODULE_RS)) {
            pwq->id_cur.global_id_rs++;
        }

        ptask->cmq_buf_info.size = cmq_offset * sizeof(uint32_t);
        /*flush cmq*/
        adlak_flush_cache(padlak, padlak->cmq_buf_info.cmq_mm_info);
        cmq_wr_offset =
            adlak_get_cmq_addr_offset(ptask->cmq_buf_info.size, padlak->cmq_buf_info.cmq_wr_offset,
                                      padlak->cmq_buf_info.total_size);

        mb();
        adlak_hal_submit((void *)padlak, cmq_wr_offset);
    }

    padlak->cmq_buf_info.cmq_wr_offset = cmq_wr_offset;
    ptask->cmq_buf_info.mm_info        = padlak->cmq_buf_info.cmq_mm_info;

    AML_LOG_INFO("cmq_wr_offset=%u , cmq_rd_offset=%u, cmq_size=%u",
                 padlak->cmq_buf_info.cmq_wr_offset, padlak->cmq_buf_info.cmq_rd_offset,
                 ptask->cmq_buf_info.size);
    adlak_cmq_dump(padlak, ptask->cmq_buf_info.size);

    ptask->hw_stat.irq_status.timeout = false;
    invoke_num                        = ptask->invoke_end_idx + 1 - ptask->invoke_start_idx;

    if (padlak->hw_timeout_ms) {
        ptask->hw_timeout_ms = padlak->hw_timeout_ms * invoke_num;
    } else {
        ptask->hw_timeout_ms = 10 * invoke_num;
    }
#if CONFIG_ADLAK_EMU_EN
    g_adlak_emu_dev_wpt            = padlak->cmq_buf_info.cmq_wr_offset;
    g_adlak_emu_dev_cmq_size       = ptask->cmq_buf_info.size;
    g_adlak_emu_dev_cmq_total_size = padlak->cmq_buf_info.total_size;
#endif

    AML_LOG_DEBUG("%s End", __func__);
    return 0;
}
#endif

static struct adlak_task *adlak_get_task_with_id(struct adlak_context *context, int32_t net_id,
                                                 int32_t invoke_id) {
    struct list_head * hd;
    struct adlak_task *ptask, *ptask_tmp, *ret = NULL;
    int                idx;
    for (idx = 0; idx < 4; idx++) {
        if (0 == idx) {
            hd = &context->padlak->queue.finished_list;
        } else if (1 == idx) {
            hd = &context->padlak->queue.pending_list;
        } else if (2 == idx) {
            hd = &context->padlak->queue.ready_list;
        } else if (3 == idx) {
            hd = &context->padlak->queue.scheduled_list;
        }
        if (!list_empty(hd)) {
            list_for_each_entry_safe(ptask, ptask_tmp, hd, head) {
                if (ptask) {
                    if ((net_id != -1) && (net_id != ptask->net_id)) {
                        continue;
                    }
                    if ((invoke_id != -1) && (invoke_id != ptask->invoke_idx)) {
                        continue;
                    }
                    ret = ptask;
                    break;
                }
            }
        }
    }

    return ret;
}

int adlak_wait_until_finished(struct adlak_context *      context,
                              struct adlak_get_stat_desc *stat_desc) {
    struct adlak_device *   padlak = context->padlak;
    struct adlak_workqueue *pwq    = &padlak->queue;

    struct adlak_task *ptask     = NULL;  //, *ptask_tmp = NULL;
    int                retry_cnt = 10;
    AML_LOG_DEBUG("%s", __func__);
#ifdef CONFIG_ADLAK_DEBUG_INNNER
    adlak_dbg_inner_update(context, "adlak_wait");
#endif
    adlak_os_mutex_lock(&pwq->wq_mutex);

    ptask = adlak_get_task_with_id(context, stat_desc->net_register_idx,
                                   stat_desc->invoke_register_idx);
    adlak_os_mutex_unlock(&pwq->wq_mutex);
    if (NULL == ptask) {
        return -1;
    }
    while (ADLAK_SUBMIT_STATE_FINISHED != ptask->state || ADLAK_SUBMIT_STATE_FAIL != ptask->state) {
        // wait signal
        if (ERR(EINTR) == adlak_os_sema_take_timeout(ptask->invoke_done, 1000)) {
            retry_cnt--;
            if (!retry_cnt) {
                break;
            }
        } else {
            break;
        }
    }
    adlak_os_mutex_lock(&padlak->dev_mutex);
    stat_desc->profile_en       = ptask->profilling.profile_en;
    stat_desc->invoke_time_us   = ptask->profilling.time_elapsed_us;
    stat_desc->start_idx        = ptask->invoke_start_idx;
    stat_desc->end_idx          = ptask->invoke_end_idx;
    stat_desc->profile_rpt      = ptask->profilling.profile_rpt;
    stat_desc->axi_freq_cur     = padlak->clk_axi_freq_real;
    stat_desc->core_freq_cur    = padlak->clk_core_freq_real;
    stat_desc->mem_alloced_umd  = context->mem_alloced;
    stat_desc->mem_alloced_base = padlak->mm->usage.mem_alloced_kmd;
    stat_desc->mem_pool_size    = padlak->mm->usage.mem_pools_size;
    stat_desc->mem_pool_used =
        padlak->mm->usage.mem_alloced_kmd + padlak->mm->usage.mem_alloced_umd;
    stat_desc->efficiency = adlak_dmp_get_efficiency(padlak);
    if (ADLAK_SUBMIT_STATE_FINISHED == ptask->state) {
        stat_desc->ret_state = 0;
    } else {
        stat_desc->ret_state = -3;  // TODO
    }
    adlak_os_mutex_unlock(&padlak->dev_mutex);

    return 0;
}
