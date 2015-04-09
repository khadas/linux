#include "../../firmware.h"

#define VERSTR "00000"

#define MicroCode vc1_mc
#include "vc1_vc1_linux.h"

#define FOR_CPUS {MESON_CPU_MAJOR_ID_M8, MESON_CPU_MAJOR_ID_M8M2, 0}
#define FOR_VFORMAT VFORMAT_VC1

#define REG_FIRMWARE_ALL()\
		DEF_FIRMWARE_VER(vc1_mc, VERSTR);\

INIT_DEF_FIRMWARE();
