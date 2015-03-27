#ifndef TSYNC_PCR_H
#define TSYNC_PCR_H

extern void tsync_pcr_avevent_locked(enum avevent_e event, u32 param);

extern int tsync_pcr_start(void);

extern void tsync_pcr_stop(void);

extern int tsync_pcr_set_apts(unsigned pts);

extern int get_vsync_pts_inc_mode(void);

#endif
