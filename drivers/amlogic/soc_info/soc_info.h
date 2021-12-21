/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __SOC_INFO_H
#define __SOC_INFO_H

#define SOCDATA_DEVICE_NAME	"socdata"
#define SOCDATA_CLASS_NAME "socdata"
#define NOCS_DATA_LENGTH (48)
#define CMD_SOCVER1_DATA _IO('s', 0x01)
#define CMD_SOCVER2_DATA  _IO('s', 0x02)
#define CMD_POC_DATA  _IO('s', 0x03)
#define CMD_NOCSDATA_READ  _IO('s', 0x04)
#define CMD_NOCSDATA_WRITE  _IO('s', 0x05)
#define CMD_AUTH_REGION_SET  _IO('s', 0x06)
#define CMD_AUTH_REGION_RST  _IO('s', 0x07)
#define CMD_AUTH_REGION_GET_ALL  _IO('s', 0x08)
#define CMD_SOC_CHIP_NOCS_INFO  _IO('s', 0x09)

#define EFUSE_HAL_NOCS_API_READ	 4
#define EFUSE_HAL_NOCS_API_WRITE 5
#define AUTH_REG_NUM (16)
#define AUTH_REG_MAGIC 0x4152
#define AUTH_REG_SET_IDX 1
#define AUTH_REG_SET_RST 2
#define AUTH_REG_GET_ALL 3
#define S905C2_CHIP_NUM 0x32
#define CAS_NOCS_MODE 0x05

/* efuse HAL_API arg */
struct efuse_hal_api_arg {
	unsigned int cmd;		/* R/W */
	unsigned int offset;
	unsigned int size;
	unsigned long buffer;
	unsigned long retcnt;
};

struct authnt_region {
	u64 base;
	u64 size;
	u32 attr;
} __attribute__((__packed__));

extern unsigned int read_nocsdata_cmd;
extern unsigned int write_nocsdata_cmd;
extern unsigned int auth_reg_ops_cmd;
ssize_t nocsdata_read(char *buf, size_t count, loff_t *ppos);
ssize_t nocsdata_write(const char *buf, size_t count, loff_t *ppos);
int auth_region_set(void *auth_input);
int auth_region_get_all(void *auth_input);
int auth_region_rst(void);

#endif
