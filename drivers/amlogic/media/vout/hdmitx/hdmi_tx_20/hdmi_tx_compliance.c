// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
/* Base Block, Vendor/Product Information, byte[8]~[17] */
struct edid_venddat_t {
	unsigned char data[10];
};

static struct edid_venddat_t vendor_6g[] = {
	/* SAMSUNG UA55KS7300JXXZ */
	{ {0x4c, 0x2d, 0x3b, 0x0d, 0x00, 0x06, 0x00, 0x01, 0x01, 0x1a} }
	/* Add new vendor data here */
};

static struct edid_venddat_t vendor_ratio[] = {
	/* Mi L55M2-AA */
	{ {0x61, 0xA4, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x19} }
	/* Add new vendor data here */
};

static struct edid_venddat_t vendor_null_pkt[] = {
	/* changhong LT32876 */
	{ {0x0d, 0x04, 0x21, 0x90, 0x01, 0x00, 0x00, 0x00, 0x20, 0x12} }
	/* Add new vendor data here */
};

bool hdmitx_find_vendor_6g(struct hdmitx_dev *hdev)
{
	int i;

	if (!hdev || !hdev->edid_ptr)
		return false;
	for (i = 0; i < ARRAY_SIZE(vendor_6g); i++) {
		if (memcmp(&hdev->edid_ptr[8], vendor_6g[i].data,
			   sizeof(vendor_6g[i].data)) == 0)
			return true;
	}
	return false;
}

/* need to forcely change clk ratio for such TV when suspend/resume box */
bool hdmitx_find_vendor_ratio(struct hdmitx_dev *hdev)
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

/* need to forcely enable null packet for such TV */
bool hdmitx_find_vendor_null_pkt(struct hdmitx_dev *hdev)
{
	int i;

	if (!hdev || !hdev->edid_ptr)
		return false;
	for (i = 0; i < ARRAY_SIZE(vendor_null_pkt); i++) {
		if (memcmp(&hdev->edid_ptr[8], vendor_null_pkt[i].data,
		    sizeof(vendor_null_pkt[i].data)) == 0)
			return true;
	}
	return false;
}
