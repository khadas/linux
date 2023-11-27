// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>

/* Base Block, Vendor/Product Information, byte[8]~[17] */
struct edid_venddat_t {
	unsigned char data[10];
};

static struct edid_venddat_t vendor_ratio[] = {
	/* Mi L55M2-AA */
	{ {0x61, 0xA4, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x19} }
	/* Add new vendor data here */
};

/* need to forcely change clk ratio for such TV when suspend/resume box */
bool hdmitx21_find_vendor_ratio(struct hdmitx_dev *hdev)
{
	int i;

	if (!hdev || !hdev->edid_ptr)
		return false;
	for (i = 0; i < ARRAY_SIZE(vendor_ratio); i++) {
		if (memcmp(&hdev->edid_ptr[8], vendor_ratio[i].data,
			   sizeof(vendor_ratio[i].data)) == 0)
			return true;
	}
	return false;
}

