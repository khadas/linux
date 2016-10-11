#
# Makefile for the linux kernel.
#

# Object file lists.



obj-$(CONFIG_AM_STREAMING) += amports.o

AMPORTS_OBJS = amstream.o streambuf.o vdec.o adec.o tsdemux.o psparser.o esparser.o amvdec.o amstream_profile.o tsync_pcr.o amports_gate.o gp_pll.o thread_rw.o vdec_input.o

ifeq ($(CONFIG_MULTI_DEC),y)
AMPORTS_OBJS += vdec_profile.o
endif

AMPORTS_OBJS += config_parser.o

amports-objs := $(AMPORTS_OBJS)
amports-y    := $(AMPORTS_OBJS)


obj-$(CONFIG_AM_VDEC_MPEG12) += amvdec_mpeg12.o
amvdec_mpeg12-objs := vmpeg12.o
amvdec_mpeg12-y    := vmpeg12.o

#obj-$(CONFIG_AM_VDEC_MMPEG12) += amvdec_mmpeg12.o
#amvdec_mmpeg12-objs := vmmpeg12.o
#amvdec_mmpeg12-y    := vmmpeg12.o

obj-$(CONFIG_AM_VDEC_MPEG4) += amvdec_mpeg4.o
amvdec_mpeg4-objs := vmpeg4.o
amvdec_mpeg4-y    := vmpeg4.o

obj-$(CONFIG_AM_VDEC_MMPEG4) += amvdec_mmpeg4.o
amvdec_mmpeg4-objs := vmmpeg4.o
amvdec_mmpeg4-y    := vmmpeg4.o

obj-$(CONFIG_AM_VDEC_VC1) += amvdec_vc1.o
amvdec_vc1-objs := vvc1.o
amvdec_vc1-y    := vvc1.o

obj-$(CONFIG_AM_VDEC_H264) += amvdec_h264.o
amvdec_h264-objs := vh264.o
amvdec_h264-y    := vh264.o

obj-$(CONFIG_AM_VDEC_MH264) += amvdec_mh264.o
amvdec_mh264-objs := vmh264.o h264_dpb.o
amvdec_mh264-y    := vmh264.o h264_dpb.o

obj-$(CONFIG_AM_VDEC_H264MVC) += amvdec_h264mvc.o
amvdec_h264mvc-objs := vh264mvc.o
amvdec_h264mvc-y    := vh264mvc.o

obj-$(CONFIG_AM_VDEC_H264_4K2K) += amvdec_h264_4k2k.o
amvdec_h264_4k2k-objs := vh264_4k2k.o
amvdec_h264_4k2k-y    := vh264_4k2k.o

obj-$(CONFIG_AM_VDEC_H265) += amvdec_h265.o
amvdec_h265-objs := vh265.o
amvdec_h265-y    := vh265.o

obj-$(CONFIG_AM_VDEC_VP9) += amvdec_vp9.o
amvdec_vp9-objs := vvp9.o
amvdec_vp9-y    := vvp9.o

obj-$(CONFIG_AM_VDEC_MJPEG) += amvdec_mjpeg.o
amvdec_mjpeg-objs := vmjpeg.o
amvdec_mjpeg-y    := vmjpeg.o

obj-$(CONFIG_AM_VDEC_MMJPEG) += amvdec_mmjpeg.o
amvdec_mmjpeg-objs := vmmjpeg.o
amvdec_mmjpeg-y    := vmmjpeg.o

obj-$(CONFIG_AM_VDEC_REAL) += amvdec_real.o
amvdec_real-objs := vreal.o rmparser.o
amvdec_real-y    := vreal.o rmparser.o

obj-$(CONFIG_AM_VDEC_AVS) += amvdec_avs.o
amvdec_avs-objs := vavs.o avsp_trans.o
amvdec_avs-y    := vavs.o avsp_trans.o

obj-$(CONFIG_AM_JPEGDEC) += jpegdec.o

obj-$(CONFIG_AM_ENCODER) += amvenc_avc.o
amvenc_avc-objs := encoder.o
amvenc_avc-y    := encoder.o

obj-$(CONFIG_AM_JPEG_ENCODER) += amvenc_jpeg.o
amvenc_jpeg-objs := jpegenc.o
amvenc_jpeg-y    := jpegenc.o

obj-$(CONFIG_AM_PIC_DEC) += picdec.o
obj-$(CONFIG_AM_VIDEO) += amvideo.o

amvideo-objs := video.o vpp.o video_keeper.o
amvideo-y    := video.o vpp.o video_keeper.o

#amvideo-objs := video_prot.o
#amvideo-y    := video_prot.o

ifeq ($(CONFIG_AM_VIDEOCAPTURE),y)
amports-objs +=  amvideocap.o
amports-y   +=  amvideocap.o
endif

obj-$(CONFIG_AM_VIDEO2) += amvideo2.o
amvideo2-objs := video2.o vpp2.o
amvideo2-y    := video2.o vpp2.o

obj-$(CONFIG_AM_DEINTERLACE) += deinterlace.o

obj-$(CONFIG_AM_TIMESYNC) += amtsync.o
amtsync-objs := tsync.o timestamp.o
amtsync-y    := tsync.o timestamp.o

obj-$(CONFIG_AM_PTSSERVER) += ptsserv.o

obj-$(CONFIG_AM_SUBTITLE) += subtitle.o

obj-$(CONFIG_VSYNC_RDMA) += rdma.o

obj-y += arch/
obj-y += cnm/
obj-y += decoder/

ifneq ($(CONFIG_AM_DVB),)
##EXTRA_CFLAGS = -DENABLE_DEMUX_DRIVER -DMODIFY_TIMESTAMP_INC_WITH_PLL -DCALC_CACHED_TIME #-DUSE_VMASTER_MODE
ccflags-y = -DENABLE_DEMUX_DRIVER -DMODIFY_TIMESTAMP_INC_WITH_PLL -DCALC_CACHED_TIME #-DUSE_VMASTER_MODE
endif

