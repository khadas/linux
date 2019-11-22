obj-$(CONFIG_AMLOGIC_MEDIA_FB) += fb.o
fb-objs = osd_hw.o osd_fb.o osd_debug.o osd_backup.o osd_logo.o osd_io.o
fb-objs += osd_antiflicker.o osd_clone.o

fb-objs += osd_drm.o osd_virtual.o
fb-objs += osd_rdma.o osd_sw_sync.o

obj-$(CONFIG_INSTABOOT) += osd_progressbar.o

ccflags-y += -Idrivers/amlogic/media/common/ion_dev/
ccflags-y += -Idrivers/staging/android/
