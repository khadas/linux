MODULE_NAME = aml_mailbox
obj-$(CONFIG_AMLOGIC_MHU_MBOX) += $(MODULE_NAME).o
$(MODULE_NAME)-y += meson_mhu.o
$(MODULE_NAME)-$(CONFIG_ARM_SCPI_PROTOCOL) += scpi_protocol.o

MODULE_NAME_DSP = aml_mailbox_dsp
obj-$(CONFIG_AMLOGIC_MHU_MBOX_DSP) += $(MODULE_NAME_DSP).o
$(MODULE_NAME_DSP)-y += meson_mhu_dsp.o
$(MODULE_NAME_DSP)-$(CONFIG_ARM_SCPI_PROTOCOL) += scpi_protocol_dsp.o


