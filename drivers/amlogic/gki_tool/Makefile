MODULE_NAME = aml_gki_tool

obj-$(CONFIG_AMLOGIC_GKI_TOOL) += $(MODULE_NAME).o

$(MODULE_NAME)-y = gki_tool.o
$(MODULE_NAME)-y += gki_module.o
$(MODULE_NAME)-y += gki_config.o

$(obj)/gki_config.o: $(obj)/.gki_config

$(obj)/.gki_config: FORCE
	cp $(srctree)/arch/arm64/configs/meson64_gki_module_config drivers/amlogic/gki_tool/.gki_config
