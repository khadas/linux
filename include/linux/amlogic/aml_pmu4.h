/*
 * include/linux/amlogic/aml_pmu4.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
*/

#ifndef __AML_PMU4_H__
#define __AML_PMU4_H__

#define PMU4_VA_ID		0xBF
#define PMU4_VB_ID		0x42

extern int  aml_pmu4_write(int32_t add, uint8_t val);
extern int  aml_pmu4_write16(int32_t add, uint16_t val);
extern int  aml_pmu4_read(int add, uint8_t *val);
extern int  aml_pmu4_read16(int add, uint16_t *val);
extern int  aml_pmu4_version(uint8_t *val);

#endif /* __AML_PMU4_H__ */

