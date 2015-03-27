#include <linux/types.h>

#define MicroCode vh264mvc_mc
#include "h264c_mvc_linux.h"

#undef MicroCode
#define MicroCode vh264mvc_header_mc
#include "h264header_mvc_linux.h"

#undef MicroCode
#define MicroCode vh264mvc_mmco_mc
#include "h264mmc_mvc_linux.h"

#undef MicroCode
#define MicroCode vh264mvc_slice_mc
#include "h264slice_mvc_linux.h"
