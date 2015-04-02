#ifndef PPMGR_DEV_INCLUDE_H
#define PPMGR_DEV_INCLUDE_H
#include <linux/amlogic/amports/vframe.h>
struct ppmgr_device_t {
	struct class *cla;
	struct device *dev;
	char name[20];
	unsigned int open_count;
	int major;
	unsigned int dbg_enable;
	char *buffer_start;
	unsigned int buffer_size;

	unsigned angle;
	unsigned orientation;
	unsigned videoangle;

	int bypass;
	int disp_width;
	int disp_height;
	int canvas_width;
	int canvas_height;
#ifdef CONFIG_POST_PROCESS_MANAGER_PPSCALER
	int ppscaler_flag;
	int scale_h_start;
	int scale_h_end;
	int scale_v_start;
	int scale_v_end;
#endif
#ifdef CONFIG_POST_PROCESS_MANAGER_3D_PROCESS
	unsigned ppmgr_3d_mode;
	unsigned direction_3d;
	int viewmode;
	unsigned scale_down;
#endif
	const struct vinfo_s *vinfo;
	int left;
	int top;
	int width;
	int height;
	int receiver;
	int receiver_format;
	int display_mode;
	int mirror_flag;
	int use_prot;
	int disable_prot;
	int started;
	int global_angle;
};

struct ppmgr_dev_reg_s {
	unsigned long mem_start;
	unsigned long mem_end;
	struct device *cma_dev;
	struct dec_sysinfo *sys_info;
};

struct ppframe_s {
	struct vframe_s frame;
	int index;
	int angle;
	struct vframe_s *dec_frame;
};

#define to_ppframe(vf)	\
	container_of(vf, struct ppframe_s, frame)

extern struct ppmgr_device_t ppmgr_device;

#ifdef CONFIG_POST_PROCESS_MANAGER_3D_PROCESS
extern void Reset3Dclear(void);
extern void Set3DProcessPara(unsigned mode);
#endif

#ifdef CONFIG_POST_PROCESS_MANAGER_PPSCALER
extern int video_scaler_notify(int flag);
extern void amvideo_set_scaler_para(int x, int y, int w, int h, int flag);

extern int video_scaler_notify(int flag);
extern void amvideo_set_scaler_para(int x, int y, int w, int h, int flag);

#endif

extern int vf_ppmgr_get_states(struct vframe_states *states);

#endif /* PPMGR_DEV_INCLUDE_H. */
