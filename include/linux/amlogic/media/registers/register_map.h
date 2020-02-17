/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef CODECIO_REGISTER_MAP_H
#define CODECIO_REGISTER_MAP_H

#include <linux/io.h>

struct codecio_device_data_s {
	int cpu_id;
};

enum {
	CODECIO_CBUS_BASE = 0,
	CODECIO_DOSBUS_BASE,
	CODECIO_HIUBUS_BASE,
	CODECIO_AOBUS_BASE,
	CODECIO_VCBUS_BASE,
	CODECIO_DMCBUS_BASE,
	CODECIO_EFUSE_BASE,
	CODECIO_BUS_MAX,
};

extern void __iomem *vpp_base;
extern void __iomem *hiu_base;
extern uint codecio_reg_start[CODECIO_BUS_MAX];

int codecio_read_cbus(unsigned int reg);
void codecio_write_cbus(unsigned int reg, unsigned int val);
int codecio_read_dosbus(unsigned int reg);
void codecio_write_dosbus(unsigned int reg, unsigned int val);
int codecio_read_hiubus(unsigned int reg);
void codecio_write_hiubus(unsigned int reg, unsigned int val);
int codecio_read_aobus(unsigned int reg);
void codecio_write_aobus(unsigned int reg, unsigned int val);
int codecio_read_vcbus(unsigned int reg);
void codecio_write_vcbus(unsigned int reg, unsigned int val);
int codecio_read_dmcbus(unsigned int reg);
void codecio_write_dmcbus(unsigned int reg, unsigned int val);
int codecio_read_parsbus(unsigned int reg);
void codecio_write_parsbus(unsigned int reg, unsigned int val);
int codecio_read_aiubus(unsigned int reg);
void codecio_write_aiubus(unsigned int reg, unsigned int val);
int codecio_read_demuxbus(unsigned int reg);
void codecio_write_demuxbus(unsigned int reg, unsigned int val);
int codecio_read_resetbus(unsigned int reg);
void codecio_write_resetbus(unsigned int reg, unsigned int val);
int codecio_read_efusebus(unsigned int reg);
void codecio_write_efusebus(unsigned int reg, unsigned int val);

int aml_reg_read(u32 bus_type, unsigned int reg, unsigned int *val);
int aml_reg_write(u32 bus_type, unsigned int reg, unsigned int val);
int aml_regmap_update_bits(u32 bus_type,
			   unsigned int reg,
			   unsigned int mask,
			   unsigned int val);
/*
 ** CBUS REG Read Write and Update some bits
 */
int aml_read_cbus(unsigned int reg);
void aml_write_cbus(unsigned int reg, unsigned int val);
void aml_cbus_update_bits(unsigned int reg,
			  unsigned int mask,
			  unsigned int val);
/*
 ** AO REG Read Write and Update some bits
 */
int aml_read_aobus(unsigned int reg);
void aml_write_aobus(unsigned int reg, unsigned int val);
void aml_aobus_update_bits(unsigned int reg,
			   unsigned int mask,
			   unsigned int val);
/*
 ** VCBUS Bus REG Read Write and Update some bits
 */
int aml_read_vcbus(unsigned int reg);
void aml_write_vcbus(unsigned int reg, unsigned int val);
void aml_vcbus_update_bits(unsigned int reg,
			   unsigned int mask,
			   unsigned int val);
/*
 ** DOS BUS Bus REG Read Write and Update some bits
 */
int aml_read_dosbus(unsigned int reg);
void aml_write_dosbus(unsigned int reg, unsigned int val);
void aml_dosbus_update_bits(unsigned int reg,
			    unsigned int mask, unsigned int val);
int  aml_read_sec_reg(unsigned int reg);
void  aml_write_sec_reg(unsigned int reg, unsigned int val);

/*
 ** HIUBUS REG Read Write and Update some bits
 */
int aml_read_hiubus(unsigned int reg);
void aml_write_hiubus(unsigned int reg, unsigned int val);
void aml_hiubus_update_bits(unsigned int reg,
			    unsigned int mask, unsigned int val);

/*
 ** DMCBUS REG Read Write and Update some bits
 */
int aml_read_dmcbus(unsigned int reg);
void aml_write_dmcbus(unsigned int reg, unsigned int val);
void aml_dmcbus_update_bits(unsigned int reg,
			    unsigned int mask, unsigned int val);

#endif
