#include <linux/types.h>

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

