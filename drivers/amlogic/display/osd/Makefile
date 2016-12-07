obj-$(CONFIG_AM_FB) += fb.o
fb-objs = osd_hw.o osd_fb.o osd_debug.o osd_backup.o
fb-objs += osd_antiflicker.o osd_clone.o

obj-$(CONFIG_FB_OSD_VSYNC_RDMA) += osd_rdma.o

obj-$(CONFIG_INSTABOOT) += osd_progressbar.o

ccflags-y += -Idrivers/amlogic/ion_dev/
ccflags-y += -Idrivers/staging/android/
ccflags-y += -Idrivers/amlogic/display/
