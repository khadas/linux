dtb-$(CONFIG_ARCH_ROCKCHIP) += rk3399-khadas-edge-linux.dtb
dtb-$(CONFIG_ARCH_ROCKCHIP) += rk3399-khadas-edgev-linux.dtb
dtb-$(CONFIG_ARCH_ROCKCHIP) += rk3399-khadas-edgev-mipi-linux.dtb
dtb-$(CONFIG_ARCH_ROCKCHIP) += rk3399-khadas-captain-linux.dtb
dtb-$(CONFIG_ARCH_ROCKCHIP) += rk3399-khadas-captain-mipi-linux.dtb

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
subdir-y	+= overlays/edgev
subdir-y	+= overlays/captain
clean-files	:= *.dtb *.dtbo
