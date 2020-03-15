#!/bin/sh

insmod aml_mailbox.ko
#insmod aml_power.ko
insmod meson_jtag.ko
insmod reboot.ko
insmod thermal.ko
insmod meson-rng.ko
insmod tee.ko
insmod spi-meson-spifc.ko
insmod meson-ir.ko
insmod aml_efuse_unifykey.ko
insmod aml_media.ko
insmod ion_cma_heap.ko
insmod aml_ddr_tool.ko
insmod aml_crypto_dma.ko
insmod reg_access.ko
insmod pcie-amlogic-v2.ko
insmod mdio-mux.ko
insmod dwmac-meson.ko
insmod dwmac-meson8b.ko
insmod snd_soc.ko
insmod snd-soc-aml_t9015.ko
insmod snd-soc-dummy_codec.ko
#mdio-mux.ko
insmod mdio-mux-meson-g12a.ko
insmod i2c-meson.ko
insmod rtc-meson-vrtc.ko
insmod arm_scpi.ko
#arm_scpi.ko
insmod scpi_pm_domain.ko
insmod adc_keypad.ko
