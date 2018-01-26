ge2d-objs = bitblt.o \
	fillrect.o \
	ge2d_hw.o \
	ge2dgen.o \
	ge2d_wq.o \
	stretchblt.o \
	ge2d_main.o \
	blend.o

obj-$(CONFIG_AM_GE2D) += ge2d.o
ccflags-y += -Iinclude/linux/ge2d/
ccflags-y += -Idrivers/amlogic/display/osd/
ccflags-y += -I$(srctree)/drivers/staging/android
ccflags-y += -Idrivers/amlogic/ion_dev/
