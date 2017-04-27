#ifndef __AML_SD_EMMC_INTERNAL_H__

#define __AML_SD_EMMC_INTERNAL_H__

struct aml_tuning_data {
	const u8 *blk_pattern;
	unsigned int blksz;
};

extern void aml_host_bus_fsm_show(struct amlsd_host *host, int fsm_val);
extern void mmc_cmd_LBA_show(struct mmc_host *mmc,
		struct mmc_request *mrq);
extern void aml_sd_emmc_print_err(struct amlsd_host *host);
extern int aml_sd_emmc_read_response(struct mmc_host *mmc,
		struct mmc_command *cmd);
extern void aml_sd_emmc_send_stop(struct amlsd_host *host);
extern void aml_sd_emmc_request_done(struct mmc_host *mmc,
		struct mmc_request *mrq);
extern void aml_sd_emmc_clk_switch_on(struct amlsd_platform *pdata,
		int clk_div, int clk_src_sel);
extern void aml_sd_emmc_clk_switch_off(struct amlsd_host *host);
extern void aml_sd_emmc_clk_switch(struct amlsd_platform *pdata,
		int clk_div, int clk_src_sel);
extern int aml_sd_emmc_execute_tuning_(struct mmc_host *mmc, u32 opcode,
		struct aml_tuning_data *tuning_data, u32 adj_win_start);
extern int aml_sd_emmc_post_dma(struct amlsd_host *host,
		struct mmc_request *mrq);
#endif
