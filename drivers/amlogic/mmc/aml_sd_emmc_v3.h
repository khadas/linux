#ifndef __AML_SD_EMMC_V3_H__

#define __AML_SD_EMMC_V3_H__

struct amlsd_host *aml_sd_emmc_init_host_v3(struct amlsd_host *host);
void aml_sd_emmc_reg_init_v3(struct amlsd_host *host);
void aml_sd_emmc_set_clk_rate_v3(struct mmc_host *mmc, unsigned int clk_ios);
void aml_sd_emmc_set_timing_v3(struct amlsd_platform *pdata, u32 timing);
void aml_sd_emmc_set_bus_width_v3(struct amlsd_platform *pdata, u32 busw_ios);
void aml_sd_emmc_set_power_v3(struct amlsd_platform *pdata, u32 power_mode);
void aml_sd_emmc_set_ios_v3(struct mmc_host *mmc, struct mmc_ios *ios);
int aml_sd_emmc_execute_tuning_v3(struct mmc_host *mmc, u32 opcode);

#endif
