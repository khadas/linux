dtb-$(CONFIG_ARM64) += 	kvim_nougat.dtb \
			kvim_multios.dtb \
			kvim2_nougat.dtb \
			kvim2_dualos.dtb \
			kvim2_multios.dtb \
			kvim2_libreelec.dtb \
			kvim2_linux.dtb

targets += dtbs

DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtb,$(DTB_NAMES))
else
DTB_LIST := $(dtb-y)
endif
targets += $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := dts/*.dtb *.dtb
