#ifndef AMVDEC_H
#define AMVDEC_H
#include "amports_config.h"
#include <linux/amlogic/amports/vformat.h>

#define UCODE_ALIGN         8
#define UCODE_ALIGN_MASK    7UL

struct amvdec_dec_reg_s {
	unsigned long mem_start;
	unsigned long mem_end;
	struct device *cma_dev;
	struct dec_sysinfo *dec_sysinfo;
};				/*amvdec_dec_reg_t */


extern void amvdec_start(void);
extern void amvdec_stop(void);
extern void amvdec_enable(void);
extern void amvdec_disable(void);
s32 amvdec_loadmc_ex(enum vformat_e type, const char *name, char *def);


extern void amvdec2_start(void);
extern void amvdec2_stop(void);
extern void amvdec2_enable(void);
extern void amvdec2_disable(void);
s32 amvdec2_loadmc_ex(enum vformat_e type, const char *name, char *def);



extern void amhevc_start(void);
extern void amhevc_stop(void);
extern void amhevc_enable(void);
extern void amhevc_disable(void);
s32 amhevc_loadmc_ex(enum vformat_e type, const char *name, char *def);



extern void amhcodec_start(void);
extern void amhcodec_stop(void);
s32 amhcodec_loadmc_ex(enum vformat_e type, const char *name, char *def);


extern int amvdev_pause(void);
extern int amvdev_resume(void);

#ifdef CONFIG_PM
extern int amvdec_suspend(struct platform_device *dev, pm_message_t event);
extern int amvdec_resume(struct platform_device *dec);
#endif

#if 1				/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
#define AMVDEC_CLK_GATE_ON(a)
#define AMVDEC_CLK_GATE_OFF(a)
#else
#define AMVDEC_CLK_GATE_ON(a) CLK_GATE_ON(a)
#define AMVDEC_CLK_GATE_OFF(a) CLK_GATE_OFF(a)
#endif

/* TODO: move to register headers */
#define RESET_VCPU          (1<<7)
#define RESET_CCPU          (1<<8)

#endif				/* AMVDEC_H */
