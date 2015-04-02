#ifndef _PPMGR_TV_H_
#define _PPMGR_TV_H_

extern void ppmgr_vf_put_dec(struct vframe_s *vf);
extern u32 index2canvas(u32 index);
extern struct vfq_s q_ready;
extern struct vfq_s q_free;
extern int get_bypass_mode(void);

#endif

