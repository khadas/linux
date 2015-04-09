#ifndef UCODE_MANAGER_HEADER
#define UCODE_MANAGER_HEADER
#include "firmware.h"
#include "clk.h"

struct chip_vdec_info_s {

	int cpu_type;

	struct video_firmware_s *firmware;

	struct chip_vdec_clk_s *clk_mgr[VDEC_MAX];
};

const char *get_cpu_type_name(void);
const char *get_video_format_name(enum vformat_e type);

struct chip_vdec_info_s *get_current_vdec_chip(void);

#endif				/*
				 */
