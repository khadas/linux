dtb-$(CONFIG_ARM64) += kvim_linux.dtb
dtb-$(CONFIG_ARM64) += kvim2_linux.dtb
dtb-$(CONFIG_ARM64) += kvim3l_linux.dtb
dtb-$(CONFIG_ARM64) += kvim3_linux.dtb

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
subdir-y	+= overlays/kvim
subdir-y	+= overlays/kvim2
subdir-y	+= overlays/kvim3
subdir-y	+= overlays/kvim3l
clean-files	:= *.dtb *.dtbo
