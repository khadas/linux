MODULE_NAME			= thermal

obj-$(CONFIG_AMLOGIC_THERMAL)	= $(MODULE_NAME).o
$(MODULE_NAME)-y = meson_tsensor.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_COOLDEV) += meson_cooldev.o cpu_hotplug.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_CPUCORE_THERMAL) += cpucore_cooling.o
