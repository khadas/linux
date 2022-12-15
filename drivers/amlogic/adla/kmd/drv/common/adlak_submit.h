/*******************************************************************************
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_submit.h
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

#ifndef __ADLAK_SUBMIT_H__
#define __ADLAK_SUBMIT_H__

/***************************** Include Files *********************************/

#include "adlak_api.h"
#include "adlak_common.h"
#include "adlak_context.h"
#include "adlak_device.h"
#include "adlak_hw.h"
#include "adlak_profile.h"

#ifdef __cplusplus
extern "C" {
#endif

/************************** Constant Definitions *****************************/

enum ADLAK_SUBMIT_STATE {
    ADLAK_SUBMIT_STATE_IDLE,
    ADLAK_SUBMIT_STATE_PENDING,
    ADLAK_SUBMIT_STATE_READY,
    ADLAK_SUBMIT_STATE_RUNNING,
    ADLAK_SUBMIT_STATE_FINISHED,
    ADLAK_SUBMIT_STATE_FAIL,
};

/**************************Global Variable************************************/

/**************************Type Definition and Structure**********************/
#define ADLAK_TASK_CANCELED (1 << 0)

#define PS_CMD_NOP 0x70000000
#define PS_CMD_RESET_ID 0x71000000
#define PS_CMD_SET_FENCE 0x72000000
#define PS_CMD_SET_TIME_STAMP 0x73000000
#define PS_CMD_SET_DEPENDENCY 0x74000000
#define PS_CMD_EXECUTE 0x75000000
#define PS_CMD_CONFIG 0x76000000
#define PS_CMD_CONFIG_WITH_ADDRESS 0x77000000
#define PS_CMD_SET_SW_ID 0x7E000000
#define PS_CMD_STOP 0x7F000000
// time stamp command
#define PS_CMD_TIME_STAMP_IRQ_MASK 0x00000001

// dependency command
#define PS_CMD_DEPENDENCY_PWX_VALID_MASK 0x00100000
#define PS_CMD_DEPENDENCY_PWE_VALID_MASK 0x00200000
#define PS_CMD_DEPENDENCY_RS_VALID_MASK 0x00400000

#define PS_CMD_DEPENDENCY_PWX_ID_MASK 0x0000000f
#define PS_CMD_DEPENDENCY_PWE_ID_MASK 0x000000f0
#define PS_CMD_DEPENDENCY_RS_ID_MASK 0x00000f00

#define PS_CMD_DEPENDENCY_PWX_ID_SHIFT 0
#define PS_CMD_DEPENDENCY_PWE_ID_SHIFT 4
#define PS_CMD_DEPENDENCY_RS_ID_SHIFT 8

// exectue command
#define PS_CMD_EXECUTE_OUTPUT_PWX_MASK 0x00100000
#define PS_CMD_EXECUTE_OUTPUT_PWE_MASK 0x00200000
#define PS_CMD_EXECUTE_OUTPUT_RS_MASK 0x00400000

// config command
#define PS_CMD_CONFIG_PWX_MASK 0x00000001
#define PS_CMD_CONFIG_PWE_MASK 0x00000002
#define PS_CMD_CONFIG_PX_MASK 0x00000004
#define PS_CMD_CONFIG_DMDW_MASK 0x00000008
#define PS_CMD_CONFIG_DMDF_MASK 0x00000010
#define PS_CMD_CONFIG_DW_MASK 0x00000020
#define PS_CMD_CONFIG_PE_MASK 0x00000040
#define PS_CMD_CONFIG_DMCW_MASK 0x00000080
#define PS_CMD_CONFIG_DMCF_MASK 0x00000100
#define PS_CMD_CONFIG_MC_MASK 0x00000200
#define PS_CMD_CONFIG_RS_MASK 0x00000400
#define PS_CMD_CONFIG_PX_LUT_MASK 0x00100000
#define PS_CMD_CONFIG_PE_LUT_MASK 0x00200000
#define PS_CMD_CONFIG_RS_CAT_MASK 0x00400000

// sw id command
#define PS_CMD_SW_ID_MASK 0x00ffffff

#define CMD_ALIGN_SIZE (16)  // 16byte

enum ADLAK_PLATFORM_MODULE {
    ADLAK_PLATFORM_MODULE_RS     = 0,  // reshape:format conversion
    ADLAK_PLATFORM_MODULE_RS_CAT = 1,
    ADLAK_PLATFORM_MODULE_MC     = 2,  // convolution
    ADLAK_PLATFORM_MODULE_DMCF   = 3,  // dma command feature
    ADLAK_PLATFORM_MODULE_DMCW   = 4,  // dma command weight
    ADLAK_PLATFORM_MODULE_PE     = 5,  // per-element wise
    ADLAK_PLATFORM_MODULE_PE_LUT = 6,
    ADLAK_PLATFORM_MODULE_DW     = 7,   // depth wise
    ADLAK_PLATFORM_MODULE_DMDF   = 8,   // dma data feature
    ADLAK_PLATFORM_MODULE_DMDW   = 9,   // dma data weight
    ADLAK_PLATFORM_MODULE_PX     = 10,  // per-element
    ADLAK_PLATFORM_MODULE_PX_LUT = 11,
    ADLAK_PLATFORM_MODULE_PWE    = 12,  // post write pe
    ADLAK_PLATFORM_MODULE_PWX    = 13,  // post write px
    ADLAK_PLATFORM_MODULE_COUNT  = 14
};

enum ADLAK_DEPENDENCY_MODE {
    ADLAK_DEPENDENCY_MODE_PARSER         = 0,
    ADLAK_DEPENDENCY_MODE_MODULE_LAYER   = 1,
    ADLAK_DEPENDENCY_MODE_MODULE_H_COUNT = 2,
    ADLAK_DEPENDENCY_MODE_COUNT          = 3
};

enum ADLAK_DEPENDENCY_MODULE {
    ADLAK_DEPENDENCY_MODULE_PWE = 0,
    ADLAK_DEPENDENCY_MODULE_PWX = 1,
    ADLAK_DEPENDENCY_MODULE_RS  = 2,
    ADLAK_DEPENDENCY_MODULE_SW  = 3,
};

enum ADLAK_PLATFORM_REG_FIXUP_TYPE { ADLAK_REG_FIXUP_TYPE_PW_COMP_FLUSH_MODE = 0 };

struct adlak_submit_dep_fixup {
    uint32_t module;
    int32_t  dep_id;
    int32_t  id_loc;
    uint32_t id_shift;
    uint32_t id_mask;
    uint32_t dep_modes[ADLAK_DEPENDENCY_MODE_COUNT];
    int32_t  mode_loc;
    uint32_t mode_shift;
    uint32_t mode_mask;
};

struct adlak_submit_reg_fixup {
#define MAX_REG_FIXUP_MODES 3

    uint32_t type;
    int32_t  loc;
    uint32_t shift;
    uint32_t mask;
    uint32_t unit;
    uint32_t modes[MAX_REG_FIXUP_MODES];
};
struct adlak_submit_addr_fixup {
    int32_t  loc;
    uint32_t shift;
    uint32_t mask;
    uint32_t unit;
    uint64_t addr;
};

struct adlak_submit_task {
    uint32_t active_modules;
    uint32_t output_modules;
    uint32_t memory_access_modules;
    int32_t  config_offset;
    int32_t  config_size;
    int32_t  addr_fixup_offset;
    int32_t  addr_fixup_count;
    int32_t  dep_info_offset;
    int32_t  dep_info_count;
    int32_t  reg_fixup_offset;
    int32_t  reg_fixup_count;
    int32_t  start_pwe_flid;
    int32_t  start_pwx_flid;
    int32_t  start_rs_flid;
    int32_t  memory_access_types[12]; /*not used in kmd*/
};

struct adlak_cmq_buf_info {
    struct adlak_mem_handle *mm_info;
    uint64_t                 offset;
    uint32_t                 size;
    uint32_t                 rpt;
};

struct adlak_task {
    struct list_head                head;
    int32_t                         net_id;
    int32_t                         invoke_idx;
    int                             state;
    uint32_t                        flag;  // task_canceled
    struct adlak_cmq_buf_info       cmq_buf_info;
    struct adlak_submit_task *      submit_tasks;
    struct adlak_submit_dep_fixup * submit_dep_fixups;
    struct adlak_submit_reg_fixup * submit_reg_fixups;
    struct adlak_submit_addr_fixup *submit_addr_fixups;
    uint8_t *                       config;
    uint32_t                        submit_tasks_num;
    uint32_t                        dep_fixups_num;
    uint32_t                        reg_fixups_num;
    uint32_t                        addr_fixups_num;
    uint32_t                        config_size;
    struct adlak_hw_stat            hw_stat;
    int32_t                         invoke_start_idx;
    int32_t                         invoke_end_idx;
    void *                          padlak;
    struct adlak_context *          context;
    uint32_t                        time_stamp;
    uint32_t                        hw_timeout_ms;

    struct adlak_profile profilling;

    adlak_os_sema_t invoke_done;
};

/************************** Function Prototypes ******************************/

int  adlak_net_register_request(struct adlak_context *     context,
                                struct adlak_network_desc *psubmit_desc);
void adlak_irq_bottom_handler(void *arg);
void adlak_queue_schedule(struct adlak_device *padlak);

void adlak_task_destroy(struct adlak_task *ptask);
void adlak_invoke_destroy(struct adlak_task *ptask);
int  adlak_net_unregister_request(struct adlak_context *         context,
                                  struct adlak_network_del_desc *submit_del);

int adlak_invoke_request(struct adlak_context *            context,
                         struct adlak_network_invoke_desc *pinvoke_desc);
int adlak_uninvoke_request(struct adlak_context *                context,
                           struct adlak_network_invoke_del_desc *pinvoke_del);
int adlak_get_status_request(struct adlak_context *context, struct adlak_get_stat_desc *stat_desc);
int adlak_profile_config(struct adlak_context *context, struct adlak_profile_cfg_desc *profile_cfg);

int  adlak_invoke_del_all(struct adlak_device *padlak, int32_t net_id);
int  adlak_clear_sch_list(struct adlak_device *padlak);
int  adlak_move_ready_to_pendding_list(struct adlak_device *padlak);
void adlak_queue_schedule_nolock(struct adlak_device *padlak);

int adlak_invoke_pattching(struct adlak_task *ptask);
int adlak_queue_update_task_state(struct adlak_device *padlak, struct adlak_task *ptask);

int adlak_queue_schedule_update(struct adlak_device *padlak, struct adlak_task **ptask_sch_cur);
#ifdef CONFIG_ADLAK_PRE_PATCH
int adlak_submit_exec(struct adlak_task *ptask);
#else
int adlak_submit_patch_and_exec(struct adlak_task *ptask);
#endif
int adlak_wait_until_finished(struct adlak_context *context, struct adlak_get_stat_desc *stat_desc);

#if CONFIG_ADLAK_EMU_EN
uint32_t adlak_emu_update_rpt(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_SUBMIT_H__ end define*/
