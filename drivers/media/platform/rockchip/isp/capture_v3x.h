/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2021 Rockchip Electronics Co., Ltd. */

#ifndef _RKISP_CAPTURE_V3X_H
#define _RKISP_CAPTURE_V3X_H

/* memory align for mpp */
#define RK_MPP_ALIGN 4096
//#define RKISP_STREAM_BP_EN 1

void rkisp_mipi_v3x_isr(u32 phy, u32 packet, u32 overflow, u32 state, struct rkisp_device *dev);

#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V30)
int rkisp_register_stream_v30(struct rkisp_device *dev);
void rkisp_unregister_stream_v30(struct rkisp_device *dev);
void rkisp_mi_v30_isr(u32 mis_val, struct rkisp_device *dev);
#else
static inline int rkisp_register_stream_v30(struct rkisp_device *dev) { return -EINVAL; }
static inline void rkisp_unregister_stream_v30(struct rkisp_device *dev) {}
static inline void rkisp_mi_v30_isr(u32 mis_val, struct rkisp_device *dev) {}
#endif

#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V32)
int rkisp_register_stream_v32(struct rkisp_device *dev);
void rkisp_unregister_stream_v32(struct rkisp_device *dev);
void rkisp_mi_v32_isr(u32 mis_val, struct rkisp_device *dev);
#else
static inline int rkisp_register_stream_v32(struct rkisp_device *dev) { return -EINVAL; }
static inline void rkisp_unregister_stream_v32(struct rkisp_device *dev) {}
static inline void rkisp_mi_v32_isr(u32 mis_val, struct rkisp_device *dev) {}
#endif

#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V33)
int rkisp_register_stream_v33(struct rkisp_device *dev);
void rkisp_unregister_stream_v33(struct rkisp_device *dev);
void rkisp_mi_v33_isr(u32 mis_val, struct rkisp_device *dev);
#else
static inline int rkisp_register_stream_v33(struct rkisp_device *dev) { return -EINVAL; }
static inline void rkisp_unregister_stream_v33(struct rkisp_device *dev) {}
static inline void rkisp_mi_v33_isr(u32 mis_val, struct rkisp_device *dev) {}
#endif

#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V32) || IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V33)
void rkisp_rockit_buf_state_clear(struct rkisp_stream *stream);
int rkisp_rockit_buf_free(struct rkisp_stream *stream);
void rkisp_rockit_dev_init(struct rkisp_device *dev);
void rkisp_rockit_dev_deinit(void);
void rkisp_rockit_frame_start(struct rkisp_device *dev);
int rkisp_rockit_fps_set(int *dst_fps, struct rkisp_stream *stream);
int rkisp_rockit_fps_get(int *dst_fps, struct rkisp_stream *stream);
int rkisp_rockit_buf_done(struct rkisp_stream *stream, int cmd, struct rkisp_buffer *curr_buf);
#else
static inline void rkisp_rockit_buf_state_clear(struct rkisp_stream *stream) { return; }
static inline int rkisp_rockit_buf_free(struct rkisp_stream *stream) { return -EINVAL; }
static inline void rkisp_rockit_dev_init(struct rkisp_device *dev) { return; }
static inline void rkisp_rockit_dev_deinit(void) {}
static inline void rkisp_rockit_frame_start(struct rkisp_device *dev) {}
static inline int rkisp_rockit_fps_set(int *dst_fps, struct rkisp_stream *stream) { return -EINVAL; }
static inline int rkisp_rockit_fps_get(int *dst_fps, struct rkisp_stream *stream) { return -EINVAL; }
static inline int rkisp_rockit_buf_done(struct rkisp_stream *stream, int cmd,
					struct rkisp_buffer *curr_buf) { return -EINVAL; }
#endif

#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V39)
int rkisp_register_stream_v39(struct rkisp_device *dev);
void rkisp_unregister_stream_v39(struct rkisp_device *dev);
void rkisp_mi_v39_isr(u32 mis_val, struct rkisp_device *dev);
void rkisp_stream_ldc_end_v39(struct rkisp_device *dev);
#else
static inline int rkisp_register_stream_v39(struct rkisp_device *dev) { return -EINVAL; }
static inline void rkisp_unregister_stream_v39(struct rkisp_device *dev) {}
static inline void rkisp_mi_v39_isr(u32 mis_val, struct rkisp_device *dev) {}
static inline void rkisp_stream_ldc_end_v39(struct rkisp_device *dev) {}
#endif

#if IS_ENABLED(CONFIG_ROCKCHIP_DVBM)
int rkisp_dvbm_get(struct rkisp_device *dev);
int rkisp_dvbm_init(struct rkisp_stream *stream);
void rkisp_dvbm_deinit(struct rkisp_device *dev);
int rkisp_dvbm_event(struct rkisp_device *dev, u32 event);
#else
static inline int rkisp_dvbm_get(struct rkisp_device *dev) { return -EINVAL; }
static inline int rkisp_dvbm_init(struct rkisp_stream *stream) { return -EINVAL; }
static inline void rkisp_dvbm_deinit(struct rkisp_device *dev) {}
static inline int rkisp_dvbm_event(struct rkisp_device *dev, u32 event) { return -EINVAL; }
#endif

#endif
