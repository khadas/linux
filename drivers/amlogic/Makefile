
##########################################
########## Amlogic Drivers ###############
##########################################


# GKI extern modules if enabled AMLOGIC_GKI
obj-$(CONFIG_AMLOGIC_MESON_CPUFREQ)	+= cpufreq/
obj-$(CONFIG_AMLOGIC_SEC)               += secmon/
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
obj-$(CONFIG_AMLOGIC_IRBLASTER)		+= irblaster/
obj-$(CONFIG_MTD_NAND_MESON)		+= mtd/nand/
obj-$(CONFIG_MTD_RESV_MESON)		+= mtd/reserve/
obj-$(CONFIG_MTD_SPI_NAND_MESON)	+= mtd/spi_nand/
obj-$(CONFIG_AMLOGIC_AUDIO_DSP)  	+= audiodsp/
obj-$(CONFIG_AMLOGIC_AMAUDIO)		+= amaudio/
obj-$(CONFIG_AMLOGIC_AUDIO_INFO) 	+= audioinfo/
obj-$(CONFIG_AMLOGIC_ETH_PRIVE)		+= ethernet/phy/
obj-$(CONFIG_AMLOGIC_MEDIA_ALGORITHM)	+= algorithm/
obj-$(CONFIG_AMLOGIC_GLOBAL_TIMER)	+= global_timer/
obj-$(CONFIG_AMLOGIC_IIO)		+= iio/
obj-$(CONFIG_AMLOGIC_MKL)		+= mkl/

#Always build in code/modules
obj-$(CONFIG_AMLOGIC_CPUIDLE)		+= cpuidle/
obj-$(CONFIG_AMLOGIC_DEFENDKEY)		+= defendkey/
obj-$(CONFIG_AMLOGIC_AUTO_CAPTURE)	+= free_reserved/
obj-$(CONFIG_AMLOGIC_GX_SUSPEND)	+= pm/
obj-$(CONFIG_AMLOGIC_POWER)		+= power/
obj-$(CONFIG_AMLOGIC_DEBUG)		+= debug/
obj-$(CONFIG_PWM_MESON)			+= pwm/
obj-$(CONFIG_AMLOGIC_LED)    		+= led/
obj-$(CONFIG_AMLOGIC_LEDRING)    	+= ledring/
obj-$(CONFIG_AMLOGIC_MEMORY_EXTEND)	+= memory_ext/
obj-$(CONFIG_AMLOGIC_WIFI)		+= wifi/
obj-$(CONFIG_AMLOGIC_UART)		+= uart/
obj-$(CONFIG_AMLOGIC_BT_DEVICE)		+= bluetooth/
obj-$(CONFIG_AMLOGIC_SMARTCARD)		+= smartcard/
obj-$(CONFIG_AMLOGIC_SMARTCARD)		+= smartcard_sc2/
obj-$(CONFIG_MMC_MESON_GX)		+= mmc/
obj-$(CONFIG_AMLOGIC_USB)               += usb/
obj-$(CONFIG_AMLOGIC_DVB_DMX)		+= dvb/
obj-$(CONFIG_DOLBY_FW) += dolby_fw/
obj-$(CONFIG_AMLOGIC_CPU_INFO)          += cpu_info/
obj-$(CONFIG_AMLOGIC_SOC_INFO) 		+= soc_info/
obj-$(CONFIG_GATOR)	+= gator-driver/
obj-$(CONFIG_AMLOGIC_MFD) += mfd/
obj-$(CONFIG_AMLOGIC_REGULATOR) += regulator/
obj-$(CONFIG_AMLOGIC_MFH_MODULE)     += mfh/
obj-$(CONFIG_MESON_I2C_SLAVE)     += i2c_slave/
obj-$(CONFIG_AMLOGIC_HWSPINLOCK)     += aml_hwspinlock/
obj-$(CONFIG_AMLOGIC_MESON_RTC)     += rtc/
obj-$(CONFIG_VAD_WAKEUP_ASR) += asr/
obj-$(CONFIG_AES_HWKEY_GCM) += aes_hwkey_gcm/
obj-$(CONFIG_AMLOGIC_FREERTOS)         += freertos/

KBUILD_CFLAGS_MODULE += $(GKI_EXT_MODULE_PREDEFINE)

all:
	@$(MAKE) -C $(KERNEL_SRC) M=$(M)  modules $(GKI_EXT_MODULE_CONFIG)

modules_install:
	@echo "$(MAKE) INSTALL_MOD_STRIP=1 M=$(M) -C $(KERNEL_SRC) modules_install"
#	@$(MAKE) INSTALL_MOD_STRIP=1 M=$(M) -C $(KERNEL_SRC) modules_install
#	mkdir -p ${OUT_DIR}/../vendor_lib/modules
#	cd ${OUT_DIR}/$(M)/; find -name "*.ko" -exec cp {} ${OUT_DIR}/../vendor_lib/modules/ \;
	mkdir -p ${OUT_DIR}/../vendor_lib/modules
	(cd ${OUT_DIR}/$(M)/; find -name "dvb_demux.ko" -exec cp {} ${OUT_DIR}/../vendor_lib/modules/ \;)
	(cd ${OUT_DIR}/$(M)/; find -name "aml_aucpu.ko" -exec cp {} ${OUT_DIR}/../vendor_lib/modules/ \;)

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) clean

