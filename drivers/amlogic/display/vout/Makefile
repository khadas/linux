obj-$(CONFIG_AM_VOUT) += vout_notify.o vout_serve.o

ifeq ($(CONFIG_ARCH_MESONG9TV),y)
else
obj-$(CONFIG_AM_TV_OUTPUT) += enc_clk_config.o
endif
obj-$(CONFIG_AM_TV_OUTPUT) += tv_vout.o

obj-$(CONFIG_AM_LCD_OUTPUT) += lcd_vout.o
obj-$(CONFIG_AM_LCD_OUTPUT) += lcd/

ccflags-y += -Idrivers/amlogic/display/
