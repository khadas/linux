/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef CODECIO_REGISTER_MAP_H
#define CODECIO_REGISTER_MAP_H

int aml_read_dmcbus(unsigned int reg);
void aml_write_dmcbus(unsigned int reg, unsigned int val);
int aml_read_efusebus(unsigned int reg);
void aml_write_efusebus(unsigned int reg, unsigned int val);

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
			   unsigned int mask, unsigned int val);
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
#endif
