/*******************************************************************************
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 * @author Shiwei.Sun (shiwei.sun@amlogic.com)
 * @brief
 * @version 0.1
 * @date 2022-05-24
 *
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
 *
 */

/***************************** Include Files *********************************/
#include "adlak_mm_smmu_tlb.h"

#include "adlak_device.h"
#include "adlak_mm.h"
#include "adlak_mm_smmu.h"

/***************** Macros (Inline Functions) Definitions *********************/
#ifdef CONFIG_64BIT
#define _write_page_entry(page_entry, entry_value) \
    *(uint64_t *)(page_entry) = (uint64_t)(entry_value)
#define _read_page_entry(page_entry) *(uint64_t *)(page_entry)
#else

#define _write_page_entry(page_entry, entry_value) \
    *(uint32_t *)(page_entry) = (uint32_t)(entry_value)
#define _read_page_entry(page_entry) *(uint32_t *)(page_entry)
#endif

/************************** Function Prototypes ******************************/
int adlak_smmu_tlb_alloc(struct adlak_device *padlak) {
    int                      ret = 0;
    struct adlak_mem_handle *mm_info;
    int                      idx1, idx2;
    struct __adlak_smmu *    psmmu = (struct __adlak_smmu *)padlak->psmmu;

    struct adlak_buf_req buf_req;
#if ADLAK_DEBUG_SMMU_EN
    AML_LOG_DEBUG("%s", __func__);
#endif
#if ADLAK_DEBUG_SMMU_EN
    AML_LOG_INFO("Alloc tlb1 buffer.");
#endif
    AML_LOG_INFO("IOVA max space is %dG byte.", SMMU_VA_SPACE_MAX);

    psmmu->tlb_l1.size = ADLAK_ALIGN(SMMU_TLB1_SIZE, SMMU_ALIGN_SIZE);

    buf_req.ret_desc.mem_type      = ADLAK_ENUM_MEMTYPE_CONTIGUOUS | ADLAK_ENUM_MEMTYPE_INNER;
    buf_req.ret_desc.mem_direction = ADLAK_ENUM_MEM_DIR_WRITE_ONLY;

    buf_req.bytes = psmmu->tlb_l1.size;
    mm_info       = adlak_mm_alloc(padlak->mm, &buf_req);
    if (ADLAK_IS_ERR_OR_NULL(mm_info)) {
        ret = -1;
        AML_LOG_ERR("adlak_os_alloc_contiguous failed.");
        goto err;
    }
    psmmu->tlb_l1.mm_info = mm_info;

#if ADLAK_DEBUG_SMMU_EN
    AML_LOG_INFO("Alloc tlb2 buffer.");
#endif
    for (idx1 = 0; idx1 < SMMU_TLB1_ENTRY_COUNT; idx1++) {
        psmmu->tlb_l2[idx1].size = ADLAK_ALIGN(SMMU_TLB2_SIZE, SMMU_ALIGN_SIZE);
        buf_req.bytes            = psmmu->tlb_l2[idx1].size;
        mm_info                  = adlak_mm_alloc(padlak->mm, &buf_req);
        if (ADLAK_IS_ERR_OR_NULL(mm_info)) {
            ret = -1;
            AML_LOG_ERR("adlak_os_alloc_contiguous failed.");
            goto err;
        }
        psmmu->tlb_l2[idx1].mm_info = mm_info;
    }
#if ADLAK_DEBUG_SMMU_EN
    AML_LOG_INFO("Alloc tlb3 buffer.");
#endif
    for (idx1 = 0; idx1 < SMMU_TLB1_ENTRY_COUNT; idx1++) {
        for (idx2 = 0; idx2 < SMMU_TLB2_ENTRY_COUNT_2M; idx2++) {
            psmmu->tlb_l3[idx1][idx2].size = ADLAK_ALIGN(SMMU_TLB3_SIZE, SMMU_ALIGN_SIZE);

            buf_req.bytes = psmmu->tlb_l3[idx1][idx2].size;
            mm_info       = adlak_mm_alloc(padlak->mm, &buf_req);
            if (ADLAK_IS_ERR_OR_NULL(mm_info)) {
                ret = -1;
                AML_LOG_ERR("adlak_os_alloc_contiguous failed.");
                goto err;
            }
            psmmu->tlb_l3[idx1][idx2].mm_info = mm_info;
        }
    }

    AML_LOG_INFO("tlb1 section size=%lu Bytes.", (uintptr_t)psmmu->tlb_l1.size);
    AML_LOG_INFO("tlb2 per-section size=%lu KBytes.", (uintptr_t)psmmu->tlb_l2[0].size / 1024);
    AML_LOG_INFO("tlb3 per-section size=%lu KBytes.", (uintptr_t)psmmu->tlb_l3[0][0].size / 1024);

    return 0;

err:

    return ret;
}

int adlak_smmu_tlb_fill_init(struct __adlak_smmu *psmmu) {
    int       idx1, idx2, idx3;
    uintptr_t tlb_logic;
    uint64_t  entry_val = 0;

#if ADLAK_DEBUG_SMMU_EN
    AML_LOG_DEBUG("%s", __func__);
#endif

    /*fill tlb level 1*/
    tlb_logic = (uintptr_t)psmmu->tlb_l1.mm_info->cpu_addr;
    AML_LOG_DEBUG("l1 cpu_addr= %lX,mem_type=%u", (uintptr_t)psmmu->tlb_l1.mm_info->cpu_addr,
                  psmmu->tlb_l1.mm_info->mem_type);

    for (idx1 = 0; idx1 < SMMU_TLB1_ENTRY_COUNT; idx1++) {
        entry_val = psmmu->tlb_l2[idx1].mm_info->phys_addr;
        entry_val = entry_val | SMMU_ENTRY_FLAG_L1_VALID;
#ifdef CONFIG_64BIT
        _write_page_entry(tlb_logic + (idx1 * SMMU_TLB_ENTRY_SIZE), entry_val);
#else
        _write_page_entry(tlb_logic + (idx1 * SMMU_TLB_ENTRY_SIZE), (uint32_t)entry_val);
        _write_page_entry(tlb_logic + (idx1 * SMMU_TLB_ENTRY_SIZE) + sizeof(uint32_t),
                          (uint32_t)(entry_val >> 32));
#endif
    }
    psmmu->smmu_entry = psmmu->tlb_l1.mm_info->phys_addr;

    /*fill tlb level 2*/
    for (idx1 = 0; idx1 < SMMU_TLB1_ENTRY_COUNT; idx1++) {
        tlb_logic = (uintptr_t)psmmu->tlb_l2[idx1].mm_info->cpu_addr;
        for (idx2 = 0; idx2 < SMMU_TLB2_ENTRY_COUNT_2M; idx2++) {
            entry_val = psmmu->tlb_l3[idx1][idx2].mm_info->phys_addr;
            entry_val = entry_val | SMMU_ENTRY_FLAG_L2_VALID_4K;
#ifdef CONFIG_64BIT
            _write_page_entry(tlb_logic + (idx2 * SMMU_TLB_ENTRY_SIZE), entry_val);
#else
            _write_page_entry(tlb_logic + (idx2 * SMMU_TLB_ENTRY_SIZE), (uint32_t)entry_val);
            _write_page_entry(tlb_logic + (idx2 * SMMU_TLB_ENTRY_SIZE) + sizeof(uint32_t),
                              (uint32_t)(entry_val >> 32));
#endif
        }
    }

    /*fill tlb level 3*/
    for (idx1 = 0; idx1 < SMMU_TLB1_ENTRY_COUNT; idx1++) {
        for (idx2 = 0; idx2 < SMMU_TLB2_ENTRY_COUNT_2M; idx2++) {
            tlb_logic = (uint64_t)(uintptr_t)psmmu->tlb_l3[idx1][idx2].mm_info->cpu_addr;
            for (idx3 = 0; idx3 < SMMU_TLB3_ENTRY_COUNT_4K; idx3++) {
                entry_val = 0;
                entry_val = entry_val | SMMU_ENTRY_FLAG_L3_INVALID;
#ifdef CONFIG_64BIT
                _write_page_entry(tlb_logic + (idx3 * SMMU_TLB_ENTRY_SIZE), entry_val);
#else
                _write_page_entry(tlb_logic + (idx3 * SMMU_TLB_ENTRY_SIZE), entry_val);
                _write_page_entry(tlb_logic + (idx3 * SMMU_TLB_ENTRY_SIZE) + sizeof(uint32_t),
                                  (uint32_t)(entry_val >> 32));
#endif
            }
        }
    }

    return 0;
}

int adlak_smmu_tlb_dump(struct __adlak_smmu *psmmu) {
#if !ADLAK_DEBUG_SMMU_EN
    return 0;
#else
    int idx1, idx2, idx3;
    uintptr_t tlb_logic;

    AML_LOG_DEBUG("%s", __func__);

    AML_LOG_DEBUG("TLB level 1");
    tlb_logic = (uintptr_t)psmmu->tlb_l1.mm_info->cpu_addr;
    for (idx1 = 0; idx1 < SMMU_TLB1_ENTRY_COUNT; idx1++) {
#ifdef CONFIG_64BIT
        AML_LOG_DEFAULT("offset:0x%08X\t0x%llX \n", (uint32_t)(idx1 * sizeof(uint64_t)),
                        _read_page_entry(tlb_logic + (idx1 * sizeof(uint64_t))));
#else
        AML_LOG_DEFAULT("offset:0x%08X\t0x%08X 0x%08X\n", (uint32_t)(idx1 * sizeof(uint64_t)),
                        _read_page_entry(tlb_logic + (idx1 * sizeof(uint32_t))),
                        _read_page_entry(tlb_logic + ((idx1 + 1) * sizeof(uint32_t))));

#endif
    }
    AML_LOG_DEBUG("TLB level 2");
    // for (idx1 = 0; idx1 < SMMU_TLB1_ENTRY_COUNT; idx1++) {
    for (idx1 = 0; idx1 < 1; idx1++) {
        AML_LOG_DEBUG("TLB level 2 idx=%d", idx1);
        tlb_logic = (uintptr_t)psmmu->tlb_l2[idx1].mm_info->cpu_addr;
        for (idx2 = 0; idx2 < SMMU_TLB2_ENTRY_COUNT_2M;) {
#ifdef CONFIG_64BIT
            AML_LOG_DEFAULT("offset:0x%08X\t0x%llX 0x%llX 0x%llX 0x%llX \n",
                            (uint32_t)(idx2 * sizeof(uint64_t)),
                            _read_page_entry(tlb_logic + (idx2 * sizeof(uint64_t))),
                            _read_page_entry(tlb_logic + ((idx2 + 1) * sizeof(uint64_t))),
                            _read_page_entry(tlb_logic + ((idx2 + 2) * sizeof(uint64_t))),
                            _read_page_entry(tlb_logic + ((idx2 + 3) * sizeof(uint64_t))));
#else
            AML_LOG_DEFAULT(
                "offset:0x%08X\t0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X\n",
                (uint32_t)(idx2 * sizeof(uint64_t)),
                _read_page_entry(tlb_logic + (idx2 * sizeof(uint32_t))),
                _read_page_entry(tlb_logic + ((idx2 + 1) * sizeof(uint32_t))),
                _read_page_entry(tlb_logic + ((idx2 + 2) * sizeof(uint32_t))),
                _read_page_entry(tlb_logic + ((idx2 + 3) * sizeof(uint32_t))),
                _read_page_entry(tlb_logic + ((idx2 + 4) * sizeof(uint32_t))),
                _read_page_entry(tlb_logic + ((idx2 + 5) * sizeof(uint32_t))),
                _read_page_entry(tlb_logic + ((idx2 + 6) * sizeof(uint32_t))),
                _read_page_entry(tlb_logic + ((idx2 + 7) * sizeof(uint32_t))));

#endif
            idx2 = idx2 + 4;
        }

        AML_LOG_DEFAULT("\n");
    }

    AML_LOG_DEBUG("TLB level 3");

    // for (idx1 = 0; idx1 < SMMU_TLB1_ENTRY_COUNT; idx1++) {
    //  for (idx2 = 0; idx2 < SMMU_TLB2_ENTRY_COUNT_2M; idx2++) {
    for (idx1 = 0; idx1 < 1; idx1++) {
        for (idx2 = 0; idx2 < 1; idx2++) {
            AML_LOG_DEBUG("TLB level 3 [%d][%d]", idx1, idx2);
            tlb_logic = (uintptr_t)psmmu->tlb_l3[idx1][idx2].mm_info->cpu_addr;
            for (idx3 = 0; idx3 < SMMU_TLB2_ENTRY_COUNT_2M;) {
#ifdef CONFIG_64BIT
                AML_LOG_DEFAULT("offset:0x%08X\t0x%llX 0x%llX 0x%llX 0x%llX \n",
                                (uint32_t)(idx3 * sizeof(uint64_t)),
                                _read_page_entry(tlb_logic + (idx3 * sizeof(uint64_t))),
                                _read_page_entry(tlb_logic + ((idx3 + 1) * sizeof(uint64_t))),
                                _read_page_entry(tlb_logic + ((idx3 + 2) * sizeof(uint64_t))),
                                _read_page_entry(tlb_logic + ((idx3 + 3) * sizeof(uint64_t))));
#else
                AML_LOG_DEFAULT(
                    "offset:0x%08X\t0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X\n",
                    (uint32_t)(idx3 * sizeof(uint64_t)),
                    _read_page_entry(tlb_logic + (idx3 * sizeof(uint32_t))),
                    _read_page_entry(tlb_logic + ((idx3 + 1) * sizeof(uint32_t))),
                    _read_page_entry(tlb_logic + ((idx3 + 2) * sizeof(uint32_t))),
                    _read_page_entry(tlb_logic + ((idx3 + 3) * sizeof(uint32_t))),
                    _read_page_entry(tlb_logic + ((idx3 + 4) * sizeof(uint32_t))),
                    _read_page_entry(tlb_logic + ((idx3 + 5) * sizeof(uint32_t))),
                    _read_page_entry(tlb_logic + ((idx3 + 6) * sizeof(uint32_t))),
                    _read_page_entry(tlb_logic + ((idx3 + 7) * sizeof(uint32_t))));

#endif
                idx3 = idx3 + 4;
            }
        }
    }
    return 0;

#endif
}

int adlak_smmu_tlb_add(struct __adlak_smmu *psmmu, dma_addr_t iova_addr, phys_addr_t phys_addr,
                       size_t size) {
    uint32_t  tlb_l1_offset, tlb_l2_offset, tlb_l3_offset;
    uintptr_t tlb_logic;
    uint64_t  entry_val = 0;

#if ADLAK_DEBUG_SMMU_EN
    AML_LOG_DEBUG("%s", __func__);
#endif
#if ADLAK_DEBUG_SMMU_EN

    AML_LOG_DEBUG("iova_addr = 0x%llx, phys_addr = 0x%llx ", (uint64_t)iova_addr,
                  (uint64_t)phys_addr);
#endif
    tlb_l1_offset = GET_SMMU_TLB1_ENTRY_OFFSEST(iova_addr);
    tlb_l2_offset = GET_SMMU_TLB2_ENTRY_OFFSEST(iova_addr);
    tlb_l3_offset = GET_SMMU_TLB3_ENTRY_OFFSEST(iova_addr);
#if ADLAK_DEBUG_SMMU_EN
    AML_LOG_DEBUG("tlb_l1_offset = %d, tlb_l2_offset = %d, tlb_l3_offset = %d ", tlb_l1_offset,
                  tlb_l2_offset, tlb_l3_offset);
#endif

    tlb_logic = (uintptr_t)(psmmu->tlb_l3[tlb_l1_offset][tlb_l2_offset].mm_info->cpu_addr);
    entry_val = phys_addr;
    entry_val = entry_val | SMMU_ENTRY_FLAG_L3_VALID;
#if ADLAK_DEBUG_SMMU_EN
#ifdef CONFIG_PHYS_ADDR_T_64BIT
    AML_LOG_DEBUG("tlb_logic = 0x%llx+0x%x, entry_val = 0x%llx ", (uint64_t)tlb_logic,
                  (tlb_l3_offset * SMMU_TLB_ENTRY_SIZE), (uint64_t)entry_val);
#else
    AML_LOG_DEBUG("tlb_logic = 0x%llx+0x%x, entry_val = 0x%llx ",
                  (uint64_t)(tlb_logic & 0xFFFFFFFF), (tlb_l3_offset * SMMU_TLB_ENTRY_SIZE),
                  (uint64_t)entry_val);
#endif
#endif

#ifdef CONFIG_64BIT
    _write_page_entry(tlb_logic + (tlb_l3_offset * SMMU_TLB_ENTRY_SIZE), entry_val);
#else
    _write_page_entry(tlb_logic + (tlb_l3_offset * SMMU_TLB_ENTRY_SIZE), (uint32_t)entry_val);
    _write_page_entry(tlb_logic + (tlb_l3_offset * SMMU_TLB_ENTRY_SIZE) + sizeof(uint32_t),
                      (uint32_t)(entry_val >> 32));
#endif
    return 0;
}

int adlak_smmu_tlb_del(struct __adlak_smmu *psmmu, dma_addr_t iova_addr) {
    uint32_t  tlb_l1_offset, tlb_l2_offset, tlb_l3_offset;
    uintptr_t tlb_logic;
    uint64_t  entry_val = 0;

#if ADLAK_DEBUG_SMMU_EN
    AML_LOG_DEBUG("%s", __func__);
    AML_LOG_DEBUG("tlb_del iova_addr = 0x%llx ", (uint64_t)iova_addr);
#endif
    tlb_l1_offset = GET_SMMU_TLB1_ENTRY_OFFSEST(iova_addr);
    tlb_l2_offset = GET_SMMU_TLB2_ENTRY_OFFSEST(iova_addr);
    tlb_l3_offset = GET_SMMU_TLB3_ENTRY_OFFSEST(iova_addr);
    tlb_logic     = (uintptr_t)psmmu->tlb_l3[tlb_l1_offset][tlb_l2_offset].mm_info->cpu_addr;
    entry_val     = 0;
    entry_val     = entry_val | SMMU_ENTRY_FLAG_L3_INVALID;
#ifdef CONFIG_64BIT
    _write_page_entry(tlb_logic + (tlb_l3_offset * SMMU_TLB_ENTRY_SIZE), entry_val);
#else
    _write_page_entry(tlb_logic + (tlb_l3_offset * SMMU_TLB_ENTRY_SIZE), (uint32_t)entry_val);
    _write_page_entry(tlb_logic + (tlb_l3_offset * SMMU_TLB_ENTRY_SIZE) + sizeof(uint32_t),
                      (uint32_t)(entry_val >> 32));
#endif
    return 0;
}

static int adlak_smmu_tlb_deinit(struct adlak_mem *mm);
static int adlak_smmu_tlb_init(struct adlak_mem *mm) {
    struct adlak_device *padlak = mm->padlak;
    AML_LOG_DEBUG("%s", __func__);

    padlak->psmmu = adlak_os_zalloc(sizeof(struct __adlak_smmu), ADLAK_GFP_KERNEL);
    if (!padlak->psmmu) {
        return ERR(ENOMEM);
    }

    if (adlak_smmu_tlb_alloc(padlak)) {
        adlak_smmu_tlb_deinit(mm);
    }
    adlak_smmu_tlb_fill_init((struct __adlak_smmu *)padlak->psmmu);
    padlak->smmu_entry = ((struct __adlak_smmu *)padlak->psmmu)->smmu_entry;
    adlak_smmu_tlb_dump((struct __adlak_smmu *)padlak->psmmu);
    return 0;
}

static int adlak_smmu_tlb_deinit(struct adlak_mem *mm) {
    struct adlak_device *padlak = mm->padlak;
    int                  idx1, idx2;
    struct __adlak_smmu *psmmu = padlak->psmmu;
    AML_LOG_DEBUG("%s", __func__);

    if (!psmmu) {
        if (psmmu->tlb_l1.mm_info) {
            adlak_mm_free(mm, psmmu->tlb_l1.mm_info);
        }

        for (idx1 = 0; idx1 < ADLAK_ARRAY_SIZE(psmmu->tlb_l2); idx1++) {
            if (psmmu->tlb_l2[idx1].mm_info) {
                adlak_mm_free(mm, psmmu->tlb_l2[idx1].mm_info);
            }
        }
        for (idx1 = 0; idx1 < SMMU_TLB1_ENTRY_COUNT; idx1++) {
            for (idx2 = 0; idx2 < SMMU_TLB2_ENTRY_COUNT_2M; idx2++) {
                if (psmmu->tlb_l3[idx1][idx2].mm_info) {
                    adlak_mm_free(mm, psmmu->tlb_l3[idx1][idx2].mm_info);
                }
            }
        }

        adlak_os_free(psmmu);
    }
    return 0;
}

static int adlak_smmu_tlb_flush_cache(struct adlak_mem *mm) {
    struct adlak_device *padlak = mm->padlak;
    struct __adlak_smmu *psmmu  = padlak->psmmu;

    uint32_t idx1, idx2;
    if (!mm->use_smmu) {
        return 0;
    }
    AML_LOG_DEBUG("%s", __func__);

    /*flush cache level 1*/
    adlak_flush_cache(padlak, psmmu->tlb_l1.mm_info);

    /*flush cache level 2*/
    for (idx1 = 0; idx1 < SMMU_TLB1_ENTRY_COUNT; idx1++) {
        adlak_flush_cache(padlak, psmmu->tlb_l2[idx1].mm_info);
    }

    /*flush cache level 3*/
    for (idx1 = 0; idx1 < SMMU_TLB1_ENTRY_COUNT; idx1++) {
        for (idx2 = 0; idx2 < SMMU_TLB2_ENTRY_COUNT_2M; idx2++) {
            adlak_flush_cache(padlak, psmmu->tlb_l3[idx1][idx2].mm_info);
        }
    }

    return 0;
}

static int adlak_smmu_tlb_invalidate(struct adlak_mem *mm) {
    struct adlak_device *padlak = mm->padlak;
    if (!mm->use_smmu) {
        return 0;
    }
    AML_LOG_DEBUG("%s", __func__);

    adlak_check_dev_is_idle(padlak);
    adlak_hal_smmu_cache_invalid((void *)padlak, 0);
    return 0;
}

int adlak_smmu_init(struct adlak_mem *mm) {
    static struct adlak_smmu_ops ops = {
        .smmu_tlb_init        = adlak_smmu_tlb_init,
        .smmu_tlb_deinit      = adlak_smmu_tlb_deinit,
        .smmu_tlb_flush_cache = adlak_smmu_tlb_flush_cache,
        .smmu_tlb_invalidate  = adlak_smmu_tlb_invalidate,
    };
    AML_LOG_DEBUG("%s", __func__);
    mm->smmu_ops = &ops;
    adlak_smmu_tlb_init(mm);

    return 0;
}

void adlak_smmu_deinit(struct adlak_mem *mm) { adlak_smmu_tlb_deinit(mm); }
