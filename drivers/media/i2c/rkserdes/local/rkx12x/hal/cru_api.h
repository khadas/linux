/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 Rockchip Electronics Co. Ltd.
 *
 * Author: Joseph Chen <chenjh@rock-chips.com>
 */

#ifndef __RKX12X_CRU_API_H__
#define __RKX12X_CRU_API_H__

#include "hal_def.h"
#include "hal_os_def.h"
#include "cru_core.h"
#include "cru_rkx120.h"
#include "cru_rkx121.h"

static HAL_DEFINE_MUTEX(rkx12x_cru_top_lock);

static inline bool rkx12x_hwclk_is_enabled(struct hwclk *hw, uint32_t clk)
{
    bool ret;

    HAL_MutexLock(&hw->lock);
    ret = RKX12x_HAL_CRU_CORE_ClkIsEnabled(hw, clk);
    HAL_MutexUnlock(&hw->lock);

    return ret;
}

static inline int rkx12x_hwclk_enable(struct hwclk *hw, uint32_t clk)
{
    int ret;

    HAL_MutexLock(&hw->lock);
    ret = RKX12x_HAL_CRU_CORE_ClkEnable(hw, clk);
    HAL_MutexUnlock(&hw->lock);

    return ret;
}

static inline int rkx12x_hwclk_disable(struct hwclk *hw, uint32_t clk)
{
    int ret;

    HAL_MutexLock(&hw->lock);
    ret = RKX12x_HAL_CRU_CORE_ClkDisable(hw, clk);
    HAL_MutexUnlock(&hw->lock);

    return ret;
}

static inline bool rkx12x_hwclk_is_reset(struct hwclk *hw, uint32_t clk)
{
    bool ret;

    HAL_MutexLock(&hw->lock);
    ret = RKX12x_HAL_CRU_CORE_ClkIsReset(hw, clk);
    HAL_MutexUnlock(&hw->lock);

    return ret;
}

static inline int rkx12x_hwclk_reset(struct hwclk *hw, uint32_t clk)
{
    int ret;

    HAL_MutexLock(&hw->lock);
    ret = RKX12x_HAL_CRU_CORE_ClkResetAssert(hw, clk);
    HAL_MutexUnlock(&hw->lock);

    return ret;
}

static inline int rkx12x_hwclk_reset_deassert(struct hwclk *hw, uint32_t clk)
{
    int ret;

    HAL_MutexLock(&hw->lock);
    ret = RKX12x_HAL_CRU_CORE_ClkResetDeassert(hw, clk);
    HAL_MutexUnlock(&hw->lock);

    return ret;
}

static inline int rkx12x_hwclk_set_div(struct hwclk *hw, uint32_t clk, uint32_t div)
{
    int ret;

    HAL_MutexLock(&hw->lock);
    ret = RKX12x_HAL_CRU_CORE_ClkSetDiv(hw, clk, div);
    HAL_MutexUnlock(&hw->lock);

    return ret;
}

static inline uint32_t rkx12x_hwclk_get_div(struct hwclk *hw, uint32_t clk)
{
    uint32_t ret;

    HAL_MutexLock(&hw->lock);
    ret = RKX12x_HAL_CRU_CORE_ClkGetDiv(hw, clk);
    HAL_MutexUnlock(&hw->lock);

    return ret;
}

static inline int rkx12x_hwclk_set_mux(struct hwclk *hw, uint32_t clk, uint32_t mux)
{
    int ret;

    HAL_MutexLock(&hw->lock);
    ret = RKX12x_HAL_CRU_CORE_ClkSetMux(hw, clk, mux);
    HAL_MutexUnlock(&hw->lock);

    return ret;
}

static inline uint32_t rkx12x_hwclk_get_mux(struct hwclk *hw, uint32_t clk)
{
    uint32_t ret;

    HAL_MutexLock(&hw->lock);
    ret = RKX12x_HAL_CRU_CORE_ClkGetMux(hw, clk);
    HAL_MutexUnlock(&hw->lock);

    return ret;
}

static inline uint32_t rkx12x_hwclk_get_rate(struct hwclk *hw, uint32_t composite_clk)
{
    uint32_t ret;

    HAL_MutexLock(&hw->lock);
    ret = RKX12x_HAL_CRU_CORE_ClkGetFreq(hw, composite_clk);
    HAL_MutexUnlock(&hw->lock);

    return ret;
}

static inline int rkx12x_hwclk_set_rate(struct hwclk *hw, uint32_t composite_clk, uint32_t rate)
{
    int ret;

    HAL_MutexLock(&hw->lock);
    ret = RKX12x_HAL_CRU_CORE_ClkSetFreq(hw, composite_clk, rate);
    HAL_MutexUnlock(&hw->lock);

    return ret;
}

static inline int rkx12x_hwclk_set_testout(struct hwclk *hw, uint32_t composite_clk,
                                           uint32_t mux, uint32_t div)
{
    int ret;

    HAL_MutexLock(&hw->lock);
    ret = RKX12x_HAL_CRU_CORE_ClkSetTestout(hw, composite_clk, mux, div);
    HAL_MutexUnlock(&hw->lock);

    return ret;
}

static inline void rkx12x_hwclk_dump_tree(struct hwclk *hw)
{
    HAL_MutexLock(&rkx12x_cru_top_lock);
    RKX12x_HAL_CRU_CORE_ClkDumpTree(hw);
    HAL_MutexUnlock(&rkx12x_cru_top_lock);
}

static inline int rkx12x_hwclk_set_glbsrst(struct hwclk *hw, uint32_t type)
{
    int ret;

    HAL_MutexLock(&hw->lock);
    ret = RKX12x_HAL_CRU_CORE_SetGlbSrst(hw, type);
    HAL_MutexUnlock(&hw->lock);

    return ret;
}

static inline int rkx12x_hwclk_register(struct hwclk *hw, struct xferclk *xfer)
{
    int ret = 0;

    HAL_MutexLock(&rkx12x_cru_top_lock);
    ret = RKX12x_HAL_CRU_CORE_Register(hw, xfer);
    HAL_MutexUnlock(&rkx12x_cru_top_lock);

    return ret;
}

static inline int rkx12x_hwclk_init(struct hwclk *hw)
{
    int ret = 0;

    HAL_MutexLock(&rkx12x_cru_top_lock);
    ret = RKX12x_HAL_CRU_CORE_ClkInit(hw);
    HAL_MutexUnlock(&rkx12x_cru_top_lock);

    return ret;
}

#endif /* __RKX12X_CRU_API_H__ */
