#ifndef _PPMGR_3D_TV_H_
#define _PPMGR_3D_TV_H_
extern struct vfq_s q_ready;
extern struct vfq_s q_free;

extern int get_ppmgr_vertical_sample(void);
extern int get_ppmgr_scale_width(void);
extern int get_ppmgr_view_mode(void);
extern u32 index2canvas(u32 index);
extern void ppmgr_vf_put_dec(struct vframe_s *vf);

static int cur_process_type;
extern int ppmgr_cutwin_top;
extern int ppmgr_cutwin_left;
extern struct frame_info_t frame_info;

extern int get_depth(void);
#endif

