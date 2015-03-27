#include <linux/types.h>

#define MicroCode vh264_mc
#include "h264c_linux.h"

#undef MicroCode
#define MicroCode vh264_header_mc
#include "h264header_linux.h"

#undef MicroCode
#define MicroCode vh264_data_mc
#include "h264data_linux.h"

#undef MicroCode
#define MicroCode vh264_mmco_mc
#include "h264mmc_linux.h"

#undef MicroCode
#define MicroCode vh264_list_mc
#include "h264list_linux.h"

#undef MicroCode
#define MicroCode vh264_slice_mc
#include "h264slice_linux.h"

