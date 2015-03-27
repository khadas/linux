#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#ifdef CONFIG_AMAUDIO
extern int resample_delta;
#endif

extern u32 timestamp_vpts_get(void);

extern void timestamp_vpts_set(u32 pts);

extern void timestamp_vpts_inc(s32 val);

extern u32 timestamp_apts_get(void);

extern void timestamp_apts_set(u32 pts);

extern void timestamp_apts_inc(s32 val);

extern u32 timestamp_pcrscr_get(void);

extern void timestamp_pcrscr_set(u32 pts);

extern void timestamp_pcrscr_inc(s32 val);

extern void timestamp_pcrscr_inc_scale(s32 inc, u32 base);

extern void timestamp_pcrscr_enable(u32 enable);

extern u32 timestamp_pcrscr_enable_state(void);

extern void timestamp_pcrscr_set_adj(s32 inc);

extern void timestamp_apts_enable(u32 enable);

extern void timestamp_apts_start(u32 enable);

extern u32 timestamp_apts_started(void);

extern void timestamp_firstvpts_set(u32 pts);

extern u32 timestamp_firstvpts_get(void);

extern void timestamp_checkin_firstvpts_set(u32 pts);

extern u32 timestamp_checkin_firstvpts_get(void);

extern void timestamp_firstapts_set(u32 pts);

extern u32 timestamp_firstapts_get(void);

#endif				/* TIMESTAMP_H */
