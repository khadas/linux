MODULE_NAME			= thermal

obj-$(CONFIG_AMLOGIC_THERMAL)	= $(MODULE_NAME).o
$(MODULE_NAME)-y = meson_tsensor.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_COOLDEV) += meson_cooldev.o cpu_hotplug.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_CPUCORE_THERMAL) += cpucore_cooling.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_GPU_THERMAL) += gpu_cooling.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_GPUCORE_THERMAL) += gpucore_cooling.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_DDR_THERMAL) += ddr_cooling.o
obj-$(CONFIG_AMLOGIC_GX_TEMP_SENSOR)	+= aml_thermal_hw.o
