asflags-y += -mfloat-abi=softfp -mfpu=neon
ccflags-y += -Idrivers/staging/android/

$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_VIDEO_COMPOSER) += video_processor/video_composer/video_composer.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_VIDEO_COMPOSER) += video_processor/video_composer/vframe_ge2d_composer.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_VIDEO_COMPOSER) += video_processor/video_composer/videodisplay.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_VIDEO_COMPOSER) += video_processor/video_composer/vframe_dewarp_composer.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_VIDEO_COMPOSER) += video_processor/video_composer/vframe_vicp_composer.o