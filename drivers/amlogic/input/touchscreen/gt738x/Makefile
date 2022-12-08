# Goodix Touchscreen Makefile
# This Makefile is only for temporary compiling use
# Use xxxproject/Makefile-release when release driver
#
#obj-$(CONFIG_TOUCHSCREEN_GOODIX_GT738X)           += goodix_core.o
#goodix_core-y					:= goodix_ts_i2c.o goodix_ts_core.o goodix_cfg_bin.o
#obj-$(CONFIG_TOUCHSCREEN_GOODIX_GT738X_UPDATE)    += goodix_gt738x_update.o
#obj-$(CONFIG_TOUCHSCREEN_GOODIX_GT738X_GESTURE)   += goodix_ts_gesture.o
#obj-$(CONFIG_TOUCHSCREEN_GOODIX_GT738X_TOOLS)     += goodix_ts_tools.o

TOUCH_PANEL_MODULE = goodix_gt738x
obj-$(CONFIG_TOUCHSCREEN_GOODIX_GT738X) = $(TOUCH_PANEL_MODULE).o
$(TOUCH_PANEL_MODULE)-$(CONFIG_TOUCHSCREEN_GOODIX_GT738X) += goodix_ts_i2c.o goodix_ts_core.o goodix_cfg_bin.o

obj-$(CONFIG_TOUCHSCREEN_GOODIX_GT738X_UPDATE) += goodix_gt738x_update.o
obj-$(CONFIG_TOUCHSCREEN_GOODIX_GT738X_GESTURE) += goodix_ts_gesture.o
obj-$(CONFIG_TOUCHSCREEN_GOODIX_GT738X_TOOLS) += goodix_ts_tools.o
