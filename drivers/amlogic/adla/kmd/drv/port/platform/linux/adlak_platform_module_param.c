/*******************************************************************************
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_platform_module_param.c
 * @brief
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   	Who				Date				Changes
 * ----------------------------------------------------------------------------
 * 1.00a shiwei.sun@amlogic.com	2022/09/22	Initial release
 * </pre>
 *
 ******************************************************************************/

module_param(irqline, uint, 0644);
MODULE_PARM_DESC(irqline, "[ignored if device tree enabled]irq line number");

module_param(registerMemBase, uint, 0644);
MODULE_PARM_DESC(registerMemBase,
                 "[ignored if device tree enabled]AHB register base physic address");

module_param(registerMemSize, uint, 0644);
MODULE_PARM_DESC(registerMemSize, "[ignored if device tree enabled]AHB register size in bytes");

module_param(contiguousMemBase, uint, 0644);
MODULE_PARM_DESC(
    contiguousMemBase,
    "[ignored if device tree enabled]default base physical address of adlak memory heap");

module_param(contiguousMemSize, uint, 0644);
MODULE_PARM_DESC(contiguousMemSize,
                 "[ignored if device tree enabled]default size of adlak memory heap in bytes");

module_param(contiguousSramBase, uint, 0644);
MODULE_PARM_DESC(
    contiguousSramBase,
    "[ignored if device tree enabled]default base physical address of adlak sram heap");

module_param(contiguousSramSize, uint, 0644);
MODULE_PARM_DESC(contiguousSramSize,
                 "[ignored if device tree enabled]default size of adlak sram heap");

module_param_named(has_smmu, adlak_has_smmu, int, 0644);
MODULE_PARM_DESC(has_smmu, "[ignored if device tree enabled]has smmu");

module_param_named(dependency_mode, adlak_dependency_mode, int, 0440);
MODULE_PARM_DESC(dependency_mode, "[DEBUG only] config of dependency mode");

module_param_named(axi_freq, adlak_axi_freq, int, 0644);
MODULE_PARM_DESC(axi_freq, "max number of the axi clock in Hz");

module_param_named(core_freq, adlak_core_freq, int, 0644);
MODULE_PARM_DESC(core_freq, "max number of the core clock in Hz");

module_param_named(cmd_queue_size, adlak_cmd_queue_size, int, 0440);
MODULE_PARM_DESC(cmd_queue_size, "default size of command queue");

module_param_named(sch_time, adlak_sch_time_max_ms, int, 0440);
MODULE_PARM_DESC(sch_time, "soft-timeout per layer");

module_param_named(dpm_period, adlak_dpm_period, int, 0644);
MODULE_PARM_DESC(dpm_period, "the check-period of the dynamic power management in ms");

module_param_named(log_level, adlak_log_level, int, 0644);
MODULE_PARM_DESC(log_level, "the default log_level of kmd");
