#include "../../firmware.h"

#define MicroCode vh264_4k2k_mc
#include "h264c_linux.h"

#undef MicroCode
#define MicroCode vh264_4k2k_mc_single
#include "h264c_linux_single.h"

#undef MicroCode
#define MicroCode vh264_4k2k_header_mc
#include "h264header_linux.h"

#undef MicroCode
#define MicroCode vh264_4k2k_header_mc_single
#include "h264header_linux_single.h"

#undef MicroCode
#define MicroCode vh264_4k2k_mmco_mc
#include "h264mmc_linux.h"

#undef MicroCode
#define MicroCode vh264_4k2k_mmco_mc_single
#include "h264mmc_linux_single.h"

#undef MicroCode
#define MicroCode vh264_4k2k_slice_mc
#include "h264slice_linux.h"

#undef MicroCode
#define MicroCode vh264_4k2k_slice_mc_single
#include "h264slice_linux_single.h"

#define FOR_CPUS {MESON_CPU_MAJOR_ID_M8M2, 0}
#define FOR_VFORMAT VFORMAT_H264_4K2K

#define DEF_FIRMEARE_FOR_M8(n) \
	REGISTER_FIRMARE_PER_CPU(MESON_CPU_MAJOR_ID_M8, FOR_VFORMAT, n)

#define DEF_FIRMEARE_FOR_M8M2(n) \
	REGISTER_FIRMARE_PER_CPU(MESON_CPU_MAJOR_ID_M8M2, FOR_VFORMAT, n)

#define REG_FIRMWARE_ALL()\
	do {\
		DEF_FIRMEARE_FOR_M8(vh264_4k2k_mc);\
		DEF_FIRMEARE_FOR_M8M2(vh264_4k2k_mc_single);\
		DEF_FIRMEARE_FOR_M8(vh264_4k2k_header_mc);\
		DEF_FIRMEARE_FOR_M8M2(vh264_4k2k_header_mc_single);\
		DEF_FIRMEARE_FOR_M8(vh264_4k2k_mmco_mc);\
		DEF_FIRMEARE_FOR_M8M2(vh264_4k2k_mmco_mc_single);\
		DEF_FIRMEARE_FOR_M8(vh264_4k2k_slice_mc);\
		DEF_FIRMEARE_FOR_M8M2(vh264_4k2k_slice_mc_single);\
	} while (0)

INIT_DEF_FIRMWARE();
