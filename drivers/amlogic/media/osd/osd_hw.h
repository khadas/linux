/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _OSD_HW_H_
#define _OSD_HW_H_

#include <linux/amlogic/media/vout/vinfo.h>
#include "osd.h"
#include "osd_sync.h"
#include "osd_drm.h"

size_t osd_canvas_align(size_t x);

#define CANVAS_ALIGNED(x) (((x) + 63) & ~63)
#define MAX_HOLD_LINE     0x1f
#define MIN_HOLD_LINE     0x04
#define VIU1_DEFAULT_HOLD_LINE  0x08
#define VIU2_DEFAULT_HOLD_LINE  0x04
//#define REG_OFFSET (0x20)
#define OSD_RELATIVE_BITS 0x33330
#include "osd_rdma.h"

extern int int_viu_vsync;
extern int int_viu2_vsync;
extern int int_viu3_vsync;
extern struct hw_para_s osd_hw;
extern struct osd_device_hw_s osd_dev_hw;
extern int enable_vd_zorder;
extern struct hw_osd_slice2ppc_reg_s hw_osd_reg_slice2ppc;

#ifdef CONFIG_HIBERNATION
void osd_freeze_hw(void);
void osd_thaw_hw(void);
void osd_restore_hw(void);
void osd_realdata_save_hw(void);
void osd_realdata_restore_hw(void);
#endif
void osd_set_color_key_hw(u32 index, u32 bpp, u32 colorkey);
void osd_srckey_enable_hw(u32  index, u8 enable);
void osd_set_gbl_alpha_hw(u32 index, u32 gbl_alpha);
u32 osd_get_gbl_alpha_hw(u32  index);
void osd_set_color_mode(u32 index,
			const struct color_bit_define_s *color);
void osd_update_disp_axis_hw(u32 index,
			     u32 display_h_start,
			     u32 display_h_end,
			     u32 display_v_start,
			     u32 display_v_end,
			     u32 xoffset,
			     u32 yoffset,
			     u32 mode_change);
void osd_setup_hw(u32 index,
		  struct osd_ctl_s *osd_ctl,
		  u32 xoffset,
		  u32 yoffset,
		  u32 xres,
		  u32 yres,
		  u32 xres_virtual,
		  u32 yres_virtual,
		  u32 disp_start_x,
		  u32 disp_start_y,
		  u32 disp_end_x,
		  u32 disp_end_y,
		  phys_addr_t fbmem,
		  phys_addr_t *afbc_fbmem,
		  const struct color_bit_define_s *color);
void osd_set_order_hw(u32 index, u32 order);
void osd_get_order_hw(u32 index, u32 *order);
void osd_set_free_scale_enable_hw(u32 index, u32 enable);
void osd_get_free_scale_enable_hw(u32 index, u32 *free_scale_enable);
void osd_set_free_scale_mode_hw(u32 index, u32 freescale_mode);
void osd_get_free_scale_mode_hw(u32 index, u32 *freescale_mode);
void osd_set_4k2k_fb_mode_hw(u32 fb_for_4k2k);
void osd_get_free_scale_mode_hw(u32 index, u32 *freescale_mode);
void osd_get_free_scale_width_hw(u32 index, u32 *free_scale_width);
void osd_get_free_scale_height_hw(u32 index, u32 *free_scale_height);
void osd_get_free_scale_axis_hw(u32 index, s32 *x0, s32 *y0,
				s32 *x1, s32 *y1);
void osd_set_free_scale_axis_hw(u32 index, s32 x0, s32 y0,
				s32 x1, s32 y1);
void osd_get_scale_axis_hw(u32 index, s32 *x0, s32 *y0,
			   s32 *x1, s32 *y1);
void osd_get_window_axis_hw(u32 index, s32 *x0, s32 *y0,
			    s32 *x1, s32 *y1);
void osd_set_window_axis_hw(u32 index, s32 x0, s32 y0, s32 x1, s32 y1);
void osd_set_scale_axis_hw(u32 index, s32 x0, s32 y0, s32 x1, s32 y1);
void osd_set_src_position_from_reg(u32 index,
	s32 src_x_start, s32 src_x_end,
	s32 src_y_start, s32 src_y_end);
s32 osd_get_position_from_reg(u32 index,
			      s32 *src_x_start, s32 *src_x_end,
			      s32 *src_y_start, s32 *src_y_end,
			      s32 *dst_x_start, s32 *dst_x_end,
			      s32 *dst_y_start, s32 *dst_y_end);
void osd_get_block_windows_hw(u32 index, u32 *windows);
void osd_set_block_windows_hw(u32 index, u32 *windows);
void osd_get_block_mode_hw(u32 index, u32 *mode);
void osd_set_block_mode_hw(u32 index, u32 mode);
void osd_enable_3d_mode_hw(u32 index, u32 enable);
void osd_set_2x_scale_hw(u32 index, u16 h_scale_enable,
			 u16 v_scale_enable);
void osd_get_flush_rate_hw(u32 index, u32 *break_rate);
void osd_set_reverse_hw(u32 index, u32 reverse, u32 update);
void osd_get_reverse_hw(u32 index, u32 *reverse);
void osd_set_antiflicker_hw(u32 index, struct vinfo_s *vinfo, u32 yres);
void osd_get_antiflicker_hw(u32 index, u32 *on_off);
void osd_get_angle_hw(u32 index, u32 *angle);
void osd_set_angle_hw(u32 index, u32 angle, u32  virtual_osd1_yres,
		      u32 virtual_osd2_yres);
void osd_get_clone_hw(u32 index, u32 *clone);
void osd_set_clone_hw(u32 index, u32 clone);
void osd_set_update_pan_hw(u32 index);
void osd_setpal_hw(u32 index, unsigned int regno,
		   unsigned int red, unsigned int green,
		   unsigned int blue, unsigned int transp);
void osd_enable_hw(u32 index, u32 enable);
void osd_set_enable_hw(u32 index, u32 enable);
void osd_pan_display_hw(u32 index, unsigned int xoffset,
			unsigned int yoffset);
int osd_sync_request(u32 index, u32 yres,
		     struct fb_sync_request_s *request);
int osd_sync_request_render(u32 index, u32 yres,
			    struct sync_req_render_s *request,
			    ulong phys_addr,
			    size_t len);
int osd_sync_do_hwc(u32 output_index, struct do_hwc_cmd_s *hwc_cmd);
s64 osd_wait_vsync_event(void);
s64 osd_wait_vsync_event_viu2(void);
s64 osd_wait_vsync_event_viu3(void);
void osd_cursor_hw(u32 index, s16 x, s16 y,
		   s16 xstart, s16 ystart,
		   u32 osd_w, u32 osd_h);
void osd_cursor_hw_no_scale(u32 index, s16 x, s16 y, s16 xstart,
			    s16 ystart, u32 osd_w, u32 osd_h);
void osd_init_scan_mode(void);
void osd_suspend_hw(void);
void osd_resume_hw(void);
void osd_shutdown_hw(void);
void osd_init_hw(u32 logo_loaded, u32 osd_probe,
		 struct osd_device_data_s *osd_meson);
void osd_init_viu2(void);
void osd_init_scan_mode(void);
void osd_set_logo_index(int index);
int osd_get_logo_index(void);
int osd_get_init_hw_flag(void);
void osd_get_hw_para(struct hw_para_s **para);
void osd_get_blending_para(struct hw_osd_blending_s **para);
int osd_set_debug_hw(u32 index, const char *buf);
char *osd_get_debug_hw(void);
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
void enable_rdma(int enable_flag);
#endif

#ifdef CONFIG_AM_FB_EXT
void osd_ext_clone_pan(u32 index);
#endif
void osd_set_pxp_mode(u32 mode);
void osd_set_afbc(u32 index, u32 enable);
u32 osd_get_afbc(u32 index);
u32 osd_get_reset_status(void);
void osd_switch_free_scale(u32 pre_index,
			   u32 pre_enable, u32 pre_scale,
			   u32 next_index, u32 next_enable,
			   u32 next_scale);
void osd_get_urgent(u32 index, u32 *urgent);
void osd_set_urgent(u32 index, u32 urgent);
void osd_get_deband(u32 index, u32 *osd_deband_enable);
void osd_set_deband(u32 index, u32 osd_deband_enable);
void osd_get_fps(u32 index, u32 *osd_fps);
void osd_set_fps(u32 index, u32 osd_fps_start);
void osd_get_info(u32 index, ulong *addr, u32 *width, u32 *height);
void osd_update_scan_mode(void);
void osd_update_3d_mode(void);
void osd_update_vsync_hit(void);
void osd_update_vsync_hit_viu2(void);
void osd_update_vsync_hit_viu3(void);
void osd_update_vsync_timestamp(void);
void osd_hw_reset(u32 output_index);
void osd_mali_afbc_start(u32 output_index);
int logo_work_init(void);
int get_logo_loaded(void);
void set_logo_loaded(void);
int set_osd_logo_freescaler(void);
int is_interlaced(struct vinfo_s *vinfo);
void osd_get_display_debug(u32 index, u32 *osd_display_debug_enable);
void osd_set_display_debug(u32 index, u32 osd_display_debug_enable);
void osd_get_background_size(u32 index, struct display_flip_info_s *disp_info);
void osd_set_background_size(u32 index, struct display_flip_info_s *disp_info);
void osd_get_hdr_used(u32 *val);
void osd_set_hdr_used(u32 val);
void osd_get_afbc_format(u32 index, u32 *format, u32 *inter_format);
void osd_set_afbc_format(u32 index, u32 format, u32 inter_format);
void osd_get_hwc_enable(u32 index, u32 *hwc_enable);
void osd_set_hwc_enable(u32 index, u32 hwc_enable);
void osd_do_hwc(u32 index);
int osd_get_capbility(u32 index);
void osd_backup_screen_info(u32 index,
			    unsigned long screen_base,
			    unsigned long screen_size);
void osd_get_screen_info(u32 index,
			 char __iomem **screen_base,
			 unsigned long *screen_size);
int get_vmap_addr(u32 index, u8 __iomem **buf);
ssize_t dd_vmap_write(u32 index, const char __user *buf,
		      size_t count, loff_t *ppos);
int osd_set_clear(u32 index);
void osd_page_flip(struct osd_plane_map_s *plane_map);
void walk_through_update_list(void);
int osd_setting_blend(u32 output_index);
void osd_set_hwc_enable(u32 index, u32 hwc_enable);
void osd_set_urgent_info(u32 ports, u32 basic_urgent);
void osd_get_urgent_info(u32 *ports, u32 *basic_urgent);
void osd_set_single_step_mode(u32 index, u32 osd_single_step_mode);
void osd_set_single_step(u32 index, u32 osd_single_step);
void output_save_info(void);
void osd_get_rotate(u32 index, u32 *osd_rotate);
void osd_set_rotate(u32 index, u32 osd_rotate);
void osd_get_afbc_err_cnt(u32 index, u32 *err_cnt);
void osd_get_dimm_info(u32 index, u32 *osd_dimm_layer, u32 *osd_dimm_color);
void osd_set_dimm_info(u32 index, u32 osd_dimm_layer, u32 osd_dimm_color);
u32 osd_get_line_n_rdma(void);
void  osd_set_line_n_rdma(u32 line_n_rdma);
u32 get_output_device_id(u32 index);
u32 to_osd_hw_index(u32 osd_index);
void osd_set_hold_line(u32 index, int hold_line);
u32 osd_get_hold_line(u32 index);
void osd_set_blend_bypass(int index, u32 blend_bypass);
u32 osd_get_blend_bypass(u32 index);
void set_viu2_format(u32 format);
void osd_init_viu2(void);
u32 viu2_osd_reg_read(u32 addr);
void viu2_osd_reg_set(u32 addr, u32 val);
void viu2_osd_reg_set_bits(u32 addr, u32 val, u32 start, u32 len);
void viu2_osd_reg_set_mask(u32 addr, u32 _mask);
void viu2_osd_reg_clr_mask(u32 addr, u32 _mask);
int notify_preblend_to_amvideo(u32 preblend_en);
void osd_get_display_fb(u32 index, u32 *osd_display_fb);
void osd_set_display_fb(u32 index, u32 osd_display_fb);
void osd_get_sc_depend(u32 *osd_sc_depend);
void osd_set_sc_depend(u32 osd_sc_depend);
void osd_get_fence_count(u32 index, u32 *fence_cnt, u32 *timeline_cnt);
int get_encp_line(u32 viu_type);
u32 get_cur_begin_line(u32 output_index);
#endif
