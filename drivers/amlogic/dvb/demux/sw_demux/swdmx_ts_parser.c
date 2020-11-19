// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "swdemux_internal.h"

struct swdmx_ts_parser *swdmx_ts_parser_new(void)
{
	struct swdmx_ts_parser *tsp;

	tsp = swdmx_malloc(sizeof(struct swdmx_ts_parser));
	SWDMX_ASSERT(tsp);

	tsp->packet_size = 188;

	swdmx_list_init(&tsp->cb_list);

	return tsp;
}

int swdmx_ts_parser_set_packet_size(struct swdmx_ts_parser *tsp, int size)
{
	SWDMX_ASSERT(tsp);

	if (size < 188) {
		swdmx_log("packet size should >= 188");
		return SWDMX_ERR;
	}

	tsp->packet_size = size;

	return SWDMX_OK;
}

int
swdmx_ts_parser_add_ts_packet_cb(struct swdmx_ts_parser *tsp,
				 swdmx_tspacket_cb cb, void *data)
{
	SWDMX_ASSERT(tsp && cb);

	swdmx_cb_list_add(&tsp->cb_list, cb, data);

	return SWDMX_OK;
}

int
swdmx_ts_parser_remove_ts_packet_cb(struct swdmx_ts_parser *tsp,
				    swdmx_tspacket_cb cb, void *data)
{
	SWDMX_ASSERT(tsp && cb);

	swdmx_cb_list_remove(&tsp->cb_list, cb, data);

	return SWDMX_OK;
}

/*Parse the TS packet.*/
static void ts_packet(struct swdmx_ts_parser *tsp, u8 *data)
{
	struct swdmx_tspacket pkt;
	u8 *p = data;
	int len;
	u8 afc;
	struct swdmx_cb_entry *e, *ne;

	pkt.pid = ((p[1] & 0x1f) << 8) | p[2];
	if (pkt.pid == 0x1fff)
		return;

	pkt.packet = p;
	pkt.packet_len = tsp->packet_size;
	pkt.error = (p[1] & 0x80) >> 7;
	pkt.payload_start = (p[1] & 0x40) >> 6;
	pkt.priority = (p[1] & 0x20) >> 5;
	pkt.scramble = p[3] >> 6;
	afc = (p[3] >> 4) & 0x03;
	pkt.cc = p[3] & 0x0f;

	p += 4;
	len = 184;

	if (afc & 2) {
		pkt.adp_field_len = p[0];

		p++;
		len--;

		pkt.adp_field = p;

		p += pkt.adp_field_len;
		len -= pkt.adp_field_len;

		if (len < 0) {
			swdmx_log("kernel:illegal adaptation field length\n");
			return;
		}
	} else {
		pkt.adp_field = NULL;
		pkt.adp_field_len = 0;
	}

	if (afc & 1) {
		pkt.payload = p;
		pkt.payload_len = len;
	} else {
		pkt.payload = NULL;
		pkt.payload_len = 0;
	}

	SWDMX_LIST_FOR_EACH_SAFE(e, ne, &tsp->cb_list, ln) {
		swdmx_tspacket_cb cb = e->cb;

		cb(&pkt, e->data);
	}
}

int swdmx_ts_parser_run(struct swdmx_ts_parser *tsp, u8 *data, int len)
{
	u8 *p = data;
	int left = len;

	SWDMX_ASSERT(tsp && data);

	while (left >= tsp->packet_size) {
		if (*p == 0x47) {
			ts_packet(tsp, p);

			p += tsp->packet_size;
			left -= tsp->packet_size;
		} else {
			p++;
			left--;
		}
	}

	return len - left;
}

void swdmx_ts_parser_free(struct swdmx_ts_parser *tsp)
{
	SWDMX_ASSERT(tsp);

	swdmx_cb_list_clear(&tsp->cb_list);
	swdmx_free(tsp);
}
