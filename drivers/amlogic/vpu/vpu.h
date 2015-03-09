#ifndef __VPU_PARA_H__
#define __VPU_PARA_H__

enum VPU_Chip_e {
	VPU_CHIP_M8 = 0,
	VPU_CHIP_M8B,
	VPU_CHIP_M8M2,
	VPU_CHIP_G9TV,
	VPU_CHIP_G9BB,
	VPU_CHIP_MAX,
};

static char *vpu_chip_name[] = {
	"m8",
	"m8baby",
	"m8m2",
	"g9tv",
	"g9baby",
	"invalid",
};

struct VPU_Conf_t {
	enum VPU_Chip_e  chip_type;
	unsigned int     clk_level_dft;
	unsigned int     clk_level_max;
	unsigned int     clk_level;
	unsigned int     mem_pd0;
	unsigned int     mem_pd1;
};


/* ************************************************ */
/* VPU frequency table, important. DO NOT modify!! */
/* ************************************************ */

/* M8: */
/* freq Max=364M, default=255M */
#define CLK_LEVEL_DFT_M8       3
#define CLK_LEVEL_MAX_M8       7
/* M8M2: */
/* freq Max=364M, default=255M */
#define CLK_LEVEL_DFT_M8M2     3
#define CLK_LEVEL_MAX_M8M2     7
/* M8baby */
/* freq max=212MHz, default=182M. */
#define CLK_LEVEL_DFT_M8B      2
#define CLK_LEVEL_MAX_M8B      4
/* G9TV */
/* freq max=696M, default=637M */
#define CLK_LEVEL_DFT_G9TV     10
#define CLK_LEVEL_MAX_G9TV     11
/* G9BB */
/* freq max=212M, default=212M */
#define CLK_LEVEL_DFT_G9BB     3
#define CLK_LEVEL_MAX_G9BB     4
static unsigned int vpu_clk_table[12][3] = {/* compatible for all chip */
	/* frequency   clk_mux   div */
	{106250000,    1,        7}, /* 0 */
	{159375000,    0,        3}, /* 1 */
	{182150000,    3,        1}, /* 2 */
	{212500000,    1,        3}, /* 3 */
	{255000000,    2,        1}, /* 4 */
	{318750000,    0,        1}, /* 5 */
	{364300000,    3,        0}, /* 6 */ /* M8M2 use gp_pll */
	{425000000,    1,        1}, /* 7 */
	{510000000,    2,        0}, /* 8 */
	{637500000,    0,        0}, /* 9 */
	{696000000,    7,        0}, /* 10 */ /* G9TV use gp1_pll */
	{850000000,    1,        0}, /* 11 */
};
/* ************************************************ */

/* ************************************************ */
/* VPU module name table */
/* ************************************************ */
static char *vpu_mod_table[] = {
	"viu_osd1",
	"viu_osd2",
	"viu_vd1",
	"viu_vd2",
	"viu_chroma",
	"viu_ofifo",
	"viu_scale",
	"viu_osd_scale",
	"viu_vdin0",
	"viu_vdin1",
	"pic_rot1",
	"pic_rot2",
	"pic_rot3",
	"di_pre",
	"di_post",
	"viu_sharpness_line_buffer",
	"viu2_osd1",
	"viu2_osd2",
	"viu2_vd1",
	"viu2_chroma",
	"viu2_ofifo",
	"viu2_scale",
	"viu2_osd_scale",
	"vdin_arbitor_am_sync",
	"display_arbitor_am_sync",
	"vpu_arbitor_am_sync",
	"vencp",
	"vencl",
	"venci",
	"isp",
	"cvd2",
	"atv_dmd",
	"viu_video_super_scaler",
	"viu_osd_super_scaler",
	"reserved",
	"d2d3",
	"none",
};

#endif
