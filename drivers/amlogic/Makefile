
##########################################
########## Amlogic Drivers ###############
##########################################


# GKI extern modules if enabled AMLOGIC_GKI
obj-$(CONFIG_AMLOGIC_MESON_CPUFREQ)	+= cpufreq/
obj-$(CONFIG_AMLOGIC_EFUSE_UNIFYKEY)    += efuse_unifykey/
obj-$(CONFIG_AMLOGIC_DDR_TOOL)		+= ddr_tool/
obj-$(CONFIG_AMLOGIC_CRYPTO_DMA)	+= crypto/
obj-$(CONFIG_AMLOGIC_MHU_MBOX)		+= mailbox/
obj-$(CONFIG_AMLOGIC_REG_ACCESS)	+= reg_access/
obj-$(CONFIG_AMLOGIC_MEDIA_ENABLE)	+= media/
obj-$(CONFIG_AMLOGIC_PCIE)		+= pci/
obj-$(CONFIG_AMLOGIC_THERMAL)		+= thermal/
obj-$(CONFIG_AMLOGIC_CLK_DEBUG)		+= clk_debug/
obj-$(CONFIG_AMLOGIC_INPUT)		+= input/
obj-$(CONFIG_AMLOGIC_TEE)		+= tee/
obj-$(CONFIG_AMLOGIC_JTAG_MESON)	+= jtag/
obj-$(CONFIG_AMLOGIC_GX_REBOOT)		+= reboot/
obj-$(CONFIG_AMLOGIC_GKI_TOOL)		+= gki_tool/
obj-$(CONFIG_AMLOGIC_HIFI4DSP)          += hifi4dsp/
obj-$(CONFIG_AMLOGIC_DVB_COMPAT)        += dvb/
obj-$(CONFIG_AMLOGIC_DRM)		+= drm/


#Always build in code/modules
obj-$(CONFIG_AMLOGIC_CPUIDLE)		+= cpuidle/
obj-$(CONFIG_AMLOGIC_DEFENDKEY)		+= defendkey/
obj-$(CONFIG_AMLOGIC_AUTO_CAPTURE)	+= free_reserved/
obj-$(CONFIG_AMLOGIC_SUSPEND)		+= pm/
obj-$(CONFIG_AMLOGIC_POWER)		+= power/
obj-$(CONFIG_AMLOGIC_DEBUG)		+= debug/
obj-$(CONFIG_PWM_MESON)			+= pwm/
obj-$(CONFIG_AMLOGIC_MEMORY_EXTEND)	+= memory_ext/
obj-$(CONFIG_AMLOGIC_WIFI)		+= wifi/
obj-$(CONFIG_AMLOGIC_UART)		+= uart/
obj-$(CONFIG_AMLOGIC_BT_DEVICE)		+= bluetooth/
obj-$(CONFIG_AMLOGIC_SMARTCARD)		+= smartcard/
obj-$(CONFIG_MMC_MESON_GX)		+= mmc/
obj-$(CONFIG_AMLOGIC_USB)               += usb/

KBUILD_CFLAGS_MODULE += $(GKI_EXT_MODULE_PREDEFINE)

all:
	@$(MAKE) -C $(KERNEL_SRC) M=$(M)  modules $(GKI_EXT_MODULE_CONFIG) $(GKI_EXT_MODULE_CONFIG)

modules_install:
	@echo "$(MAKE) INSTALL_MOD_STRIP=1 M=$(M) -C $(KERNEL_SRC) modules_install"
	@$(MAKE) INSTALL_MOD_STRIP=1 M=$(M) -C $(KERNEL_SRC) modules_install
	mkdir -p ${OUT_DIR}/../vendor_lib/modules
	cd ${OUT_DIR}/$(M)/; find -name "*.ko" -exec cp {} ${OUT_DIR}/../vendor_lib/modules/ \;

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) clean
