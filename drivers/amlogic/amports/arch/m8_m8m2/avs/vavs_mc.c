#include "../../firmware.h"

#define MicroCode vavs_mc
#include "avs_linux.h"
#undef MicroCode
#define MicroCode vavs_mc_debug
#include "avs_linux_debug.h"

#define FOR_CPUS {MESON_CPU_MAJOR_ID_M8, MESON_CPU_MAJOR_ID_M8M2, 0}
#define FOR_VFORMAT VFORMAT_AVS

#define REG_FIRMWARE_ALL()\
	do {\
		DEF_FIRMWARE(vavs_mc);\
		DEF_FIRMWARE(vavs_mc_debug);\
	} while (0)

INIT_DEF_FIRMWARE();
