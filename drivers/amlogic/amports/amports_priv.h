#ifndef AMPORTS_PRIV_HEAD_HH
#define AMPORTS_PRIV_HEAD_HH
#include "streambuf.h"
#include <linux/amlogic/amports/vframe.h>

struct stream_buf_s *get_buf_by_type(u32 type);

extern void amvenc_dos_top_reg_fix(void);
/*video.c provide*/
extern u32 trickmode_i;
struct amvideocap_req;
extern u32 set_blackout_policy(int policy);
extern u32 get_blackout_policy(void);
int calculation_stream_ext_delayed_ms(u8 type);
int ext_get_cur_video_frame(struct vframe_s **vf, int *canvas_index);
int ext_put_video_frame(struct vframe_s *vf);
int ext_register_end_frame_callback(struct amvideocap_req *req);
int request_video_firmware(const char *file_name, char *buf, int size);
void set_vsync_pts_inc_mode(int inc);

extern struct platform_device *amstream_pdev;
int amports_switch_gate(const char *name, int enable);
void set_real_audio_info(void *arg);
#define dbg() pr_info("on %s,line %d\n", __func__, __LINE__);

#endif
