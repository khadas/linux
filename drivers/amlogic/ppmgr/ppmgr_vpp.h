#ifndef _PPMGR_TV_H_
#define _PPMGR_TV_H_

void vf_ppmgr_unreg_provider(void);
void vf_ppmgr_reset(int type);
void ppmgr_vf_put_dec(struct vframe_s *vf);

#ifdef CONFIG_POST_PROCESS_MANAGER_PPSCALER
extern bool get_scaler_pos_reset(void);
extern void set_scaler_pos_reset(bool flag);
extern u32 amvideo_get_scaler_mode(void);
extern u32 amvideo_get_scaler_para(int *x, int *y, int *w, int *h, u32 *ratio);
#endif

extern u32 timestamp_pcrscr_enable_state(void);

extern int get_property_change(void);
extern void set_property_change(int flag);
extern int get_buff_change(void);
extern void set_buff_change(int flag);

#ifdef CONFIG_POST_PROCESS_MANAGER_3D_PROCESS
extern int is_mid_local_source(struct vframe_s *vf);
extern int is_mid_mvc_need_process(struct vframe_s *vf);
extern int get_mid_process_type(struct vframe_s *vf);
extern void ppmgr_vf_3d(
		struct vframe_s *vf,
		struct ge2d_context_s *context,
		struct config_para_ex_s *ge2d_config);

extern int Init3DBuff(int canvas_id);
extern void Reset3Dclear(void);
extern void ppmgr_vf_3d_tv(
		struct vframe_s *vf,
		struct ge2d_context_s *context,
		struct config_para_ex_s *ge2d_config);

extern int get_tv_process_type(struct vframe_s *vf);
#endif

extern int video_property_notify(int flag);
extern struct vframe_s *get_cur_dispbuf(void);
extern enum platform_type_t get_platform_type(void);

extern u32 timestamp_pcrscr_enable_state(void);



#endif

