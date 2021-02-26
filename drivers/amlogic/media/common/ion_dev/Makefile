#
# Makefile for the amlogic ion device drivers.
#

#
# include driver/staging for header files
#
ccflags-y += -I$(srctree)/drivers/staging/android

$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_ION) += common/ion_dev/dev_ion.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_ION) += common/ion_dev/ion_cma_heap.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_ION) += common/ion_dev/ion_codec_mm_heap.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_ION) += common/ion_dev/ion_secure_heap.o
