/*
 * AMLOGIC Audio/Video streaming port driver.
 *
 *
 */

#ifndef VDEC_H
#define VDEC_H
#include "amports_config.h"

#include <linux/platform_device.h>

struct vdec_dev_reg_s {
	unsigned long mem_start;
	unsigned long mem_end;
	struct device *cma_dev;
	struct dec_sysinfo *sys_info;
} /*vdec_dev_reg_t */;

extern void vdec_set_decinfo(struct dec_sysinfo *p);
extern int vdec_set_resource(unsigned long start, unsigned long end,
							 struct device *p);

extern s32 vdec_init(enum vformat_e vf);
extern s32 vdec_release(enum vformat_e vf);

s32 vdec_dev_register(void);
s32 vdec_dev_unregister(void);
void vdec_power_mode(int level);

enum vdec_type_e {
	VDEC_1 = 0,
	VDEC_HCODEC,
	VDEC_2,
	VDEC_HEVC,
	VDEC_MAX
};

extern void vdec2_power_mode(int level);
extern void vdec_poweron(enum vdec_type_e core);
extern void vdec_poweroff(enum vdec_type_e core);
extern bool vdec_on(enum vdec_type_e core);

enum vdec2_usage_e {
	USAGE_NONE,
	USAGE_DEC_4K2K,
	USAGE_ENCODE,
};

extern void set_vdec2_usage(enum vdec2_usage_e usage);
extern enum vdec2_usage_e get_vdec2_usage(void);

#endif				/* VDEC_H */
