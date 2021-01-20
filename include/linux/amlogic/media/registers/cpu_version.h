/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __REGISTERS_MESON_CPU_H
#define __REGISTERS_MESON_CPU_H

#define MESON_CPU_TYPE_MESON1		0x10
#define MESON_CPU_TYPE_MESON2		0x20
#define MESON_CPU_TYPE_MESON3		0x30
#define MESON_CPU_TYPE_MESON6		0x60
#define MESON_CPU_TYPE_MESON6TV		0x70
#define MESON_CPU_TYPE_MESON6TVD	0x75
#define MESON_CPU_TYPE_MESON8		0x80
#define MESON_CPU_TYPE_MESON8B		0x8B
#define MESON_CPU_TYPE_MESONG9TV	0x90

/*
 *	Read back value for P_ASSIST_HW_REV
 *
 *	Please note: M8M2 readback value same as M8 (0x19)
 *			     We changed it to 0x1D in software,
 *			     Please ALWAYS use get_meson_cpu_version()
 *			     to get the version of Meson CPU
 */
#define MESON_CPU_MAJOR_ID_M6		0x16
#define MESON_CPU_MAJOR_ID_M6TV		0x17
#define MESON_CPU_MAJOR_ID_M6TVL	0x18
#define MESON_CPU_MAJOR_ID_M8		0x19
#define MESON_CPU_MAJOR_ID_MTVD		0x1A
#define MESON_CPU_MAJOR_ID_MG9TV	0x1C
#define MESON_CPU_MAJOR_ID_M8M2		0x1D

#define MESON_CPU_VERSION_LVL_PACK	2

enum meson_cpuid_type_e {
	MESON_CPU_MAJOR_ID_M8B = 0x1B,
	MESON_CPU_MAJOR_ID_GXBB = 0x1F,
	MESON_CPU_MAJOR_ID_GXTVBB,
	MESON_CPU_MAJOR_ID_GXL,
	MESON_CPU_MAJOR_ID_GXM,
	MESON_CPU_MAJOR_ID_TXL,
	MESON_CPU_MAJOR_ID_TXLX,
	MESON_CPU_MAJOR_ID_AXG,
	MESON_CPU_MAJOR_ID_GXLX,
	MESON_CPU_MAJOR_ID_TXHD,
	MESON_CPU_MAJOR_ID_G12A,
	MESON_CPU_MAJOR_ID_G12B,
	MESON_CPU_MAJOR_ID_SM1 = 0x2B,
	MESON_CPU_MAJOR_ID_TL1 = 0x2E,
	MESON_CPU_MAJOR_ID_TM2,
	MESON_CPU_MAJOR_ID_C1,
	MESON_CPU_MAJOR_ID_SC2 = 0x32,
	MESON_CPU_MAJOR_ID_T5 = 0x34,
	MESON_CPU_MAJOR_ID_T5D = 0x35,
	MESON_CPU_MAJOR_ID_T7 = 0x36,
	MESON_CPU_MAJOR_ID_S4 = 0x38,
	MESON_CPU_MAJOR_ID_UNKNOWN,
};

#define MESON_CPU_VERSION_LVL_MAJOR	0
#define MESON_CPU_VERSION_LVL_MINOR	1
#define MESON_CPU_VERSION_LVL_PACK	2
#define MESON_CPU_VERSION_LVL_MISC	3
#define MESON_CPU_VERSION_LVL_MAX	MESON_CPU_VERSION_LVL_MISC

struct codecio_device_data_s {
	enum meson_cpuid_type_e cpu_id;
};

extern struct codecio_device_data_s codecio_meson_dev;

int meson_cpu_version_init(void);

unsigned char get_meson_cpu_version(int level);

static inline int get_cpu_type(void)
{
	return get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR);
}

static inline u32 get_cpu_package(void)
{
	unsigned int pk;

	pk = get_meson_cpu_version(MESON_CPU_VERSION_LVL_PACK) & 0xF0;
	return pk;
}

static inline bool package_id_is(unsigned int id)
{
	return get_cpu_package() == id;
}

static inline bool is_meson_m8_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_M8;
}

static inline bool is_meson_mtvd_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_MTVD;
}

static inline bool is_meson_m8m2_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_M8M2;
}

static inline bool is_meson_g9tv_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_MG9TV;
}

/* new added*/
static inline bool is_meson_m8b_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_M8B;
}

static inline bool is_meson_gxbb_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_GXBB;
}

static inline bool is_meson_gxtvbb_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_GXTVBB;
}

static inline bool is_meson_gxl_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_GXL;
}

static inline bool is_meson_gxl_package_905D(void)
{
	return is_meson_gxl_cpu() && package_id_is(0x0);
}

static inline bool is_meson_gxl_package_905X(void)
{
	return is_meson_gxl_cpu() && package_id_is(0x80);
}

static inline bool is_meson_gxl_package_905L(void)
{
	return is_meson_gxl_cpu() && package_id_is(0xc0);
}

static inline bool is_meson_gxl_package_905M2(void)
{
	return is_meson_gxl_cpu() && package_id_is(0xe0);
}

static inline bool is_meson_gxl_package_805X(void)
{
	return is_meson_gxl_cpu() && package_id_is(0x30);
}

static inline bool is_meson_gxl_package_805Y(void)
{
	return is_meson_gxl_cpu() && package_id_is(0xb0);
}

static inline bool is_meson_gxm_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_GXM;
}

static inline bool is_meson_txl_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_TXL;
}

static inline bool is_meson_txlx_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_TXLX;
}

static inline bool is_meson_axg_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_AXG;
}

static inline bool is_meson_gxlx_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_GXLX;
}

static inline bool is_meson_txhd_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_TXHD;
}

static inline bool is_meson_g12a_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_G12A;
}

static inline bool is_meson_g12b_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_G12B;
}

static inline bool is_meson_sm1_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_SM1;
}

static inline bool is_meson_tl1_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_TL1;
}

static inline bool is_meson_tm2_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_TM2;
}

static inline bool is_meson_sc2_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_SC2;
}

static inline bool is_meson_t5_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_T5;
}

static inline bool is_meson_t5d_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_T5D;
}

static inline bool is_meson_t7_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_T7;
}

static inline bool is_meson_s4_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_S4;
}

static inline bool cpu_after_eq(unsigned int id)
{
	return get_cpu_type() >= id;
}

static inline bool is_meson_txlx_package_962X(void)
{
	return is_meson_txlx_cpu() && package_id_is(0x10);
}

static inline bool is_meson_txlx_package_962E(void)
{
	return is_meson_txlx_cpu() && package_id_is(0x20);
}

static inline bool is_meson_rev_a(void)
{
	return (get_meson_cpu_version(MESON_CPU_VERSION_LVL_MINOR) == 0xA);
}

static inline bool is_meson_rev_b(void)
{
	return (get_meson_cpu_version(MESON_CPU_VERSION_LVL_MINOR) == 0xB);
}

static inline bool is_meson_rev_c(void)
{
	return (get_meson_cpu_version(MESON_CPU_VERSION_LVL_MINOR) == 0xC);
}

#endif
