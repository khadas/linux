/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __AMVIDEO_H__
#define __AMVIDEO_H__

#define _A_M  'S'
#define AMSTREAM_IOC_TRICKMODE                    _IOW((_A_M), 0x12, int)
#define AMSTREAM_IOC_TRICK_STAT                   _IOR((_A_M), 0x14, int)
#define AMSTREAM_IOC_VPAUSE                       _IOW((_A_M), 0x17, int)
#define AMSTREAM_IOC_AVTHRESH                     _IOW((_A_M), 0x18, int)
#define AMSTREAM_IOC_SYNCTHRESH                   _IOW((_A_M), 0x19, int)
#define AMSTREAM_IOC_CLEAR_VIDEO                  _IOW((_A_M), 0x1f, int)
#define AMSTREAM_IOC_GLOBAL_GET_VIDEO_OUTPUT      _IOR((_A_M), 0x21, int)
#define AMSTREAM_IOC_GLOBAL_SET_VIDEO_OUTPUT      _IOW((_A_M), 0x22, int)
#define AMSTREAM_IOC_GET_VIDEO_LAYER1_ON          _IOR((_A_M), 0x23, int)

/*video pip IOCTL command list*/
#define AMSTREAM_IOC_CLEAR_VIDEOPIP	          _IOW((_A_M), 0x24, int)
#define AMSTREAM_IOC_CLEAR_PIP_VBUF               _IO((_A_M), 0x25)

#define AMSTREAM_IOC_GET_DISPLAYPATH  _IOW((_A_M), 0x26, int)
#define AMSTREAM_IOC_SET_DISPLAYPATH  _IOW((_A_M), 0x27, int)

#define AMSTREAM_IOC_GET_PIP_DISPLAYPATH  _IOW((_A_M), 0x28, int)
#define AMSTREAM_IOC_SET_PIP_DISPLAYPATH  _IOW((_A_M), 0x29, int)
#define AMSTREAM_IOC_GLOBAL_GET_VIDEOPIP_OUTPUT   _IOR((_A_M), 0x2b, int)
#define AMSTREAM_IOC_GLOBAL_SET_VIDEOPIP_OUTPUT   _IOW((_A_M), 0x2c, int)
#define AMSTREAM_IOC_GET_VIDEOPIP_DISABLE         _IOR((_A_M), 0x2d, int)
#define AMSTREAM_IOC_SET_VIDEOPIP_DISABLE         _IOW((_A_M), 0x2e, int)
#define AMSTREAM_IOC_GET_VIDEOPIP_AXIS            _IOR((_A_M), 0x2f, int)
#define AMSTREAM_IOC_SET_VIDEOPIP_AXIS            _IOW((_A_M), 0x30, int)
#define AMSTREAM_IOC_GET_VIDEOPIP_CROP            _IOR((_A_M), 0x31, int)
#define AMSTREAM_IOC_SET_VIDEOPIP_CROP            _IOW((_A_M), 0x32, int)
#define AMSTREAM_IOC_GET_PIP_SCREEN_MODE          _IOR((_A_M), 0x33, int)
#define AMSTREAM_IOC_SET_PIP_SCREEN_MODE          _IOW((_A_M), 0x34, int)
#define AMSTREAM_IOC_GET_PIP_ZORDER               _IOW((_A_M), 0x35, u32)
#define AMSTREAM_IOC_SET_PIP_ZORDER               _IOW((_A_M), 0x36, u32)

#define AMSTREAM_IOC_GET_ZORDER                   _IOW((_A_M), 0x37, u32)
#define AMSTREAM_IOC_SET_ZORDER                   _IOW((_A_M), 0x38, u32)

#define AMSTREAM_IOC_QUERY_LAYER                  _IOW((_A_M), 0x39, u32)
#define AMSTREAM_IOC_ALLOC_LAYER                  _IOW((_A_M), 0x3a, u32)
#define AMSTREAM_IOC_FREE_LAYER                   _IOW((_A_M), 0x3b, u32)

/* VPP.3D IOCTL command list */
#define  AMSTREAM_IOC_SET_3D_TYPE                 _IOW((_A_M), 0x3c, u32)
#define  AMSTREAM_IOC_GET_3D_TYPE                 _IOW((_A_M), 0x3d, u32)
#define  AMSTREAM_IOC_GET_SOURCE_VIDEO_3D_TYPE    _IOW((_A_M), 0x3e, u32)

#define AMSTREAM_IOC_SYNCENABLE                   _IOW((_A_M), 0x43, int)
#define AMSTREAM_IOC_GET_SYNC_ADISCON             _IOR((_A_M), 0x44, int)
#define AMSTREAM_IOC_SET_SYNC_ADISCON             _IOW((_A_M), 0x45, int)
#define AMSTREAM_IOC_GET_SYNC_VDISCON             _IOR((_A_M), 0x46, int)
#define AMSTREAM_IOC_SET_SYNC_VDISCON             _IOW((_A_M), 0x47, int)
#define AMSTREAM_IOC_GET_VIDEO_DISABLE            _IOR((_A_M), 0x48, int)
#define AMSTREAM_IOC_SET_VIDEO_DISABLE            _IOW((_A_M), 0x49, int)

#define AMSTREAM_IOC_GET_VIDEO_AXIS               _IOR((_A_M), 0x4b, int)
#define AMSTREAM_IOC_SET_VIDEO_AXIS               _IOW((_A_M), 0x4c, int)
#define AMSTREAM_IOC_GET_VIDEO_CROP               _IOR((_A_M), 0x4d, int)
#define AMSTREAM_IOC_SET_VIDEO_CROP               _IOW((_A_M), 0x4e, int)

#define AMSTREAM_IOC_GET_BLACKOUT_POLICY          _IOR((_A_M), 0x52, int)
#define AMSTREAM_IOC_SET_BLACKOUT_POLICY          _IOW((_A_M), 0x53, int)

#define AMSTREAM_IOC_GET_SCREEN_MODE              _IOR((_A_M), 0x58, int)
#define AMSTREAM_IOC_SET_SCREEN_MODE              _IOW((_A_M), 0x59, int)
#define AMSTREAM_IOC_GET_VIDEO_DISCONTINUE_REPORT _IOR((_A_M), 0x5a, int)
#define AMSTREAM_IOC_SET_VIDEO_DISCONTINUE_REPORT _IOW((_A_M), 0x5b, int)

#define AMSTREAM_IOC_VF_STATUS                    _IOR((_A_M), 0x60, int)
#define AMSTREAM_IOC_GET_BLACKOUT_PIP_POLICY      _IOR((_A_M), 0x62, int)
#define AMSTREAM_IOC_SET_BLACKOUT_PIP_POLICY      _IOW((_A_M), 0x63, int)
#define AMSTREAM_IOC_GET_BLACKOUT_PIP2_POLICY      _IOR((_A_M), 0x64, int)
#define AMSTREAM_IOC_SET_BLACKOUT_PIP2_POLICY      _IOW((_A_M), 0x65, int)

#define AMSTREAM_IOC_GET_PIP2_DISPLAYPATH  _IOW((_A_M), 0x66, int)
#define AMSTREAM_IOC_SET_PIP2_DISPLAYPATH  _IOW((_A_M), 0x67, int)
#define AMSTREAM_IOC_GLOBAL_GET_VIDEOPIP2_OUTPUT   _IOR((_A_M), 0x68, int)
#define AMSTREAM_IOC_GLOBAL_SET_VIDEOPIP2_OUTPUT   _IOW((_A_M), 0x69, int)
#define AMSTREAM_IOC_GET_VIDEOPIP2_DISABLE         _IOR((_A_M), 0x70, int)
#define AMSTREAM_IOC_SET_VIDEOPIP2_DISABLE         _IOW((_A_M), 0x71, int)
#define AMSTREAM_IOC_GET_VIDEOPIP2_AXIS            _IOR((_A_M), 0x72, int)
#define AMSTREAM_IOC_SET_VIDEOPIP2_AXIS            _IOW((_A_M), 0x73, int)
#define AMSTREAM_IOC_GET_VIDEOPIP2_CROP            _IOR((_A_M), 0x74, int)
#define AMSTREAM_IOC_SET_VIDEOPIP2_CROP            _IOW((_A_M), 0x75, int)
#define AMSTREAM_IOC_GET_PIP2_SCREEN_MODE          _IOR((_A_M), 0x76, int)
#define AMSTREAM_IOC_SET_PIP2_SCREEN_MODE          _IOW((_A_M), 0x77, int)
#define AMSTREAM_IOC_GET_PIP2_ZORDER               _IOW((_A_M), 0x78, u32)
#define AMSTREAM_IOC_SET_PIP2_ZORDER               _IOW((_A_M), 0x79, u32)
#define AMSTREAM_IOC_CLEAR_VIDEOPIP2	          _IOW((_A_M), 0x7a, int)
#define AMSTREAM_IOC_CLEAR_PIP2_VBUF               _IO((_A_M), 0x7b)

#define AMSTREAM_IOC_CLEAR_VBUF                   _IO((_A_M), 0x80)

#define AMSTREAM_IOC_GET_SYNC_ADISCON_DIFF        _IOR((_A_M), 0x83, int)
#define AMSTREAM_IOC_GET_SYNC_VDISCON_DIFF        _IOR((_A_M), 0x84, int)
#define AMSTREAM_IOC_SET_SYNC_ADISCON_DIFF        _IOW((_A_M), 0x85, int)
#define AMSTREAM_IOC_SET_SYNC_VDISCON_DIFF        _IOW((_A_M), 0x86, int)
#define AMSTREAM_IOC_GET_FREERUN_MODE             _IOR((_A_M), 0x87, int)
#define AMSTREAM_IOC_SET_FREERUN_MODE             _IOW((_A_M), 0x88, int)
#define AMSTREAM_IOC_SET_VSYNC_UPINT              _IOW((_A_M), 0x89, int)
#define AMSTREAM_IOC_GET_VSYNC_SLOW_FACTOR        _IOW((_A_M), 0x8a, int)
#define AMSTREAM_IOC_SET_VSYNC_SLOW_FACTOR        _IOW((_A_M), 0x8b, int)

#define AMSTREAM_IOC_SET_OMX_VPTS                 _IOW((_A_M), 0xaf, int)
#define AMSTREAM_IOC_GET_OMX_VPTS                 _IOW((_A_M), 0xb0, int)
#define AMSTREAM_IOC_GET_OMX_VERSION              _IOW((_A_M), 0xb1, int)
#define AMSTREAM_IOC_GET_OMX_INFO                 _IOR((_A_M), 0xb2, u32)
#define AMSTREAM_IOC_SET_HDR_INFO                 _IOW((_A_M), 0xb3, int)

#define AMSTREAM_IOC_SET_TUNNEL_MODE _IOR(_A_M, 0xbd, unsigned int)
#define AMSTREAM_IOC_GET_FIRST_FRAME_TOGGLED      _IOR(_A_M, 0xbe, u32)

/* av sycn event */
#define AMSTREAM_IOC_SET_VIDEOPEEK                _IOW(_A_M, 0xbf, u32)

#define AMSTREAM_IOC_GET_TRICK_VPTS               _IOR((_A_M), 0xf0, int)
#define AMSTREAM_IOC_DISABLE_SLOW_SYNC            _IOW((_A_M), 0xf1, int)

#endif
