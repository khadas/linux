/*
 * include/linux/amlogic/hdmi_tx/hdmi_config.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
*/


#ifndef __PLAT_MESON_HDMI_CONFIG_H
#define __PLAT_MESON_HDMI_CONFIG_H

struct vendor_info_data {
	unsigned char *vendor_name; /* Max Chars: 8 */
	/* vendor_id, 3 Bytes, Refer to
	 * http://standards.ieee.org/develop/regauth/oui/oui.txt
	 */
	unsigned int vendor_id;
	unsigned char *product_desc; /* Max Chars: 16 */
	unsigned char *cec_osd_string; /* Max Chars: 14 */
	unsigned int cec_config; /* 4 bytes: use to control cec switch on/off */
	/* switch between ao-cec and ee-cec:: 1: ao-cec; 0: ee-cec */
	unsigned int ao_cec;
};

struct hdmi5v_pwr_ctrl {
	const char *name;
	unsigned int pin;
	unsigned int active;
	unsigned int state;
};

struct hdmi_config_platform_data {
	struct vendor_info_data *vend_data;
	struct hdmi5v_pwr_ctrl *pwr_ctl;
};

#endif /* __PLAT_MESON_HDMI_CONFIG_H */
