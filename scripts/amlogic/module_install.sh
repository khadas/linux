#!/bin/sh

insmod aml_malibox.ko
insmod meson_jtag.ko
insmod reboot.ko
insmod thermal.ko
insmod meson-rng.ko
insmod tee.ko
insmod spi-meson-spifc.ko
insmod meson-ir.ko
insmod aml_efuse_unifykey.ko
insmod aml_ddr_tool.ko
insmod aml_crypto_dma.ko
insmod reg_access.ko
insmod pcie-amlogic-v2.ko
insmod mdio-mux.ko
insmod dwmac-meson.ko
insmod dwmac-meson8b.ko
#mdio-mux.ko
insmod mdio-mux-meson-g12a.ko
insmod i2c-meson.ko
#aml_malibox.ko
insmod rtc-meson-vrtc.ko
insmod arm_scpi.ko
#arm_scpi.ko
insmod scpi_pm_domain.ko
#aml_malibox
insmod adc_keypad.ko
