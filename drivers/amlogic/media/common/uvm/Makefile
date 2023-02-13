#
# Makefile for amlogic uvm
#

ccflags-y += -Idrivers/staging/android/ -Idrivers/amlogic/

$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_UVM_ALLOCATOR)		+= common/uvm/meson_uvm_allocator.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_UVM_CORE)			+= common/uvm/meson_uvm_core.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_UVM_CORE)			+= common/uvm/meson_uvm_nn_processor.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_UVM_CORE)			+= common/uvm/meson_uvm_aipq_processor.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_UVM_CORE)			+= common/uvm/meson_uvm_dalton_processor.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_UVM_CORE)			+= common/uvm/meson_uvm_aiface_processor.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_UVM_CORE)			+= common/uvm/meson_uvm_aicolor_processor.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_UVM_CORE)			+= common/uvm/meson_uvm_buffer_info.o
