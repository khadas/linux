# # Makefile for the Post Process Manager device #
#ifeq ($(TARGET_BUILD_VARIANT),userdebug)
#ccflags-y += -D DEBUG_SUPPORT
#ccflags-y += -DDEBUG
#else
#ccflags-y += -DDEBUG
#endif


# AMLOGIC VLOCK
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_VLOCK) += framelock/vlock/vlock_proc.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_VLOCK) += framelock/vlock/vlock_drv.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_VLOCK) += framelock/vlock/frame_lock_proc.o

