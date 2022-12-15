/*******************************************************************************
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_api_base.h
 * @brief
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   	Who				Date				Changes
 * ----------------------------------------------------------------------------
 * 1.00a shiwei.sun@amlogic.com	2022/04/26	Initial release
 * </pre>
 *
 ******************************************************************************/

#ifndef __ADLAK_API_BASE_H__
#define __ADLAK_API_BASE_H__

/***************************** Include Files *********************************/

#ifdef __cplusplus
extern "C" {
#endif

/************************** Constant Definitions *****************************/

/**************************Global Variable************************************/

/**************************Type Definition and Structure**********************/

struct adlak_buf_desc {
    uint64_t iova_addr;     /* virtual address in smmu*/
    uint64_t va_user;       /* virtual address in user mode*/
    uint64_t va_kernel;     /* virtual address in kernel mode*/
    uint64_t phys_addr;     /* physical base address if mem_type is contiguous*/
    uint64_t bytes;         /*return real size*/
    uint32_t mem_type;      /*request info*/
    uint32_t mem_src;       /*request info*/
    uint32_t mem_direction; /*request info*/
} __packed;

enum adlak_mem_type {
    ADLAK_ENUM_MEMTYPE_CACHEABLE    = (1 << 0),
    ADLAK_ENUM_MEMTYPE_CONTIGUOUS   = (1 << 1),
    ADLAK_ENUM_MEMTYPE_INNER        = (1 << 2),  // For ADLA use only if value is true.
    ADLAK_ENUM_MEMTYPE_PA_WITHIN_4G = (1 << 4)   // physical address less than 4Gbytes
} __packed;

enum adlak_mem_direction {
    ADLAK_ENUM_MEM_DIR_READ_WRITE = 0,
    ADLAK_ENUM_MEM_DIR_READ_ONLY,
    ADLAK_ENUM_MEM_DIR_WRITE_ONLY
} __packed;

struct adlak_buf_req {
    uint64_t              bytes;         /* bytes requested to allocate */
    uint32_t              align_in_page; /* alignment requirements (in 4KB) */
    uint32_t              data_type;     /* type of data in the buffer to allocate */
    struct adlak_buf_desc ret_desc;      /* info of buffer successfully allocated */
    uint32_t              mmap_en;       /* the flag of mmap */
    uint32_t              errcode;       /* return err number */
} __packed;

struct adlak_extern_buf_info {
    uint64_t              buf_handle; /* buf handle */
    uint64_t              bytes;      /* bytes of buffer */
    uint32_t              buf_type;   /* type of buf handle */
    struct adlak_buf_desc ret_desc;   /* info of buffer successfully import */
    uint32_t              mmap_en;    /* the flag of mmap */
    uint32_t              errcode;    /* return err number */
} __packed;

enum adlak_flush_cache_direction {
    FLUSH_TO_DEVICE   = 1,
    FLUSH_FROM_DEVICE = 2,
    FLUSH_NONE        = 3,
};

struct adlak_buf_flush {
    struct adlak_buf_desc buf_desc; /* info of buffer  */
    uint32_t              direction;
    uint32_t              errcode; /* return err number */
} __packed;

struct adlak_network_desc {
    uint32_t config_size;
    uint32_t dep_fixups_num;
    uint32_t reg_fixups_num;
    uint32_t tasks_num;
    uint64_t config_va;
    uint64_t dep_fixups_va;
    uint64_t reg_fixups_va;
    uint64_t tasks_va;
    int32_t  profile_en;  // profilling enable
    uint64_t profile_iova;
    uint32_t profile_buf_size;
    int32_t  net_register_idx;  // return from kmd

} __packed;

struct adlak_network_del_desc {
    int32_t net_register_idx;
} __packed;

struct adlak_network_invoke_desc {
    int32_t  net_register_idx;
    int32_t  invoke_register_idx;  // return from kmd
    int32_t  start_idx;
    int32_t  end_idx;
    int32_t  addr_fixups_num;
    uint64_t addr_fixups_va;

} __packed;

struct adlak_network_invoke_del_desc {
    int32_t net_register_idx;
    int32_t invoke_register_idx;

} __packed;

struct adlak_get_stat_desc {
    int32_t  net_register_idx;
    int32_t  invoke_register_idx;
    int32_t  start_idx;       // return from kmd
    int32_t  end_idx;         // return from kmd
    int32_t  ret_state;       // 0: success,1:running,-1: timeout, -3: other err
    int32_t  profile_en;      // profilling enable
    uint32_t profile_rpt;     // profilling read point
    int32_t  invoke_time_us;  // invoke time which get from os

    uint64_t axi_freq_cur;      // adlak axi clock frequency currently
    uint64_t core_freq_cur;     // adlak core clock frequency currently
    uint64_t mem_alloced_base;  // alloced by kmd
    uint64_t mem_alloced_umd;   // alloced by umd in this context
    int64_t  mem_pool_size;     //-1:the limit base on the system
    uint64_t mem_pool_used;     // memory usage
    int32_t  efficiency;
} __packed;

struct adlak_profile_cfg_desc {
    int32_t  net_register_idx;
    int32_t  profile_en;  // profilling enable
    uint64_t profile_iova;
    uint32_t profile_buf_size;
    uint32_t errcode; /* return err number */

} __packed;

struct adlak_test_desc {
    uint64_t type;
} __packed;

struct adlak_caps_desc {
    uint32_t hw_ver;        /* adlak hardware version*/
    uint64_t axi_freq_max;  /* adlak axi clock frequency maximum */
    uint64_t core_freq_max; /* adlak core clock frequency maximum */
    uint32_t cmq_size;      /* cmq buffer size*/
    uint64_t sram_base;     /* axi sram base addr*/
    uint32_t sram_size;     /* axi sram buffer size*/
} __packed;

/************************** Function Prototypes ******************************/

#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_API_BASE_H__ end define*/
