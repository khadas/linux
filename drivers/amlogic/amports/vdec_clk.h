/*
 * AMLOGIC Audio/Video streaming port driver.
 *
 *
 */

#ifndef VDEC_CLK_H
#define VDEC_CLK_H

extern void vdec_clock_enable(void);
extern void vdec_clock_hi_enable(void);
extern void vdec2_clock_enable(void);
extern void vdec2_clock_hi_enable(void);
extern void hcodec_clock_enable(void);
extern void hevc_clock_enable(void);
extern void hevc_clock_hi_enable(void);

extern void vdec_clock_on(void);
extern void vdec_clock_off(void);
extern void vdec2_clock_on(void);
extern void vdec2_clock_off(void);
extern void hcodec_clock_on(void);
extern void hcodec_clock_off(void);
extern void hevc_clock_on(void);
extern void hevc_clock_off(void);
extern int vdec_clock_level(enum vdec_type_e core);

extern void vdec_clock_prepare_switch(void);
extern void hevc_clock_prepare_switch(void);

#endif				/* VDEC_CLK_H */
