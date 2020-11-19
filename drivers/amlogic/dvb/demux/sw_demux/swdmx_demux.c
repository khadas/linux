// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "swdemux_internal.h"
#include "linux/kernel.h"

/*Get the PID filter with the PID.*/
static struct swdmx_pid_filter *pid_filter_get(struct swdmx_demux *dmx, u16 pid)
{
	struct swdmx_pid_filter *f;

	SWDMX_LIST_FOR_EACH(f, &dmx->pid_filter_list, ln) {
		if (f->pid == pid)
			return f;
	}

	f = swdmx_malloc(sizeof(struct swdmx_pid_filter));
	SWDMX_ASSERT(f);

	f->pid = pid;
	f->sec_data = NULL;
	f->sec_recv = 0;

	swdmx_list_init(&f->sec_filter_list);
	swdmx_list_init(&f->ts_filter_list);

	swdmx_list_append(&dmx->pid_filter_list, &f->ln);

	return f;
}

/*Try to remove a PID filter.*/
static void pid_filter_remove(struct swdmx_pid_filter *f)
{
	if (!swdmx_list_is_empty(&f->ts_filter_list) ||
	    !swdmx_list_is_empty(&f->sec_filter_list))
		return;

	swdmx_list_remove(&f->ln);

	if (f->sec_data)
		swdmx_free(f->sec_data);

	swdmx_free(f);
}

/*Section filter match test.*/
static u8 sec_filter_match(u8 *data, int len, struct swdmx_secfilter *f)
{
	int i;
	u8 m = 0, n = 0;
	u8 has_n = 0;

	for (i = 0; i < SWDMX_SEC_FILTER_LEN; i++) {
		u8 d, v;

		d = data[i] & f->mam[i];
		v = f->value[i] & f->mam[i];
		m |= d ^ v;

		d = data[i] & f->manm[i];
		v = f->value[i] & f->manm[i];
		n |= d ^ v;

		has_n |= f->manm[i];
	}

	if (m)
		return SWDMX_FALSE;

	if (has_n && !n)
		return SWDMX_FALSE;

	return SWDMX_TRUE;
}

/*Section data resolve.*/
static int sec_data(struct swdmx_pid_filter *pid_filter, u8 *p, int len)
{
	int n, left = len, sec_len;
	u8 *sec = pid_filter->sec_data;
	u8 crc = SWDMX_FALSE;

	swdmx_log("%s enter\n", __func__);

	if (pid_filter->sec_recv < 3) {
		n = min(left, 3 - pid_filter->sec_recv);

		if (n) {
			memcpy(sec + pid_filter->sec_recv, p, n);

			p += n;
			left -= n;
			pid_filter->sec_recv += n;
		}
	}

	if (pid_filter->sec_recv < 3)
		return len;

	if (sec[0] == 0xff)
		return len;

	sec_len = (((sec[1] & 0xf) << 8) | sec[2]) + 3;
	n = min(sec_len - pid_filter->sec_recv, left);

	if (n) {
		memcpy(sec + pid_filter->sec_recv, p, n);

		p += n;
		left -= n;
		pid_filter->sec_recv += n;
	}

	if (pid_filter->sec_recv == sec_len) {
		struct swdmx_secfilter *sec_filter, *n_sec_filter;
		struct swdmx_cb_entry *ce, *nce;

		SWDMX_LIST_FOR_EACH_SAFE(sec_filter,
					 n_sec_filter,
					 &pid_filter->sec_filter_list, pid_ln) {
			if (sec_filter->params.crc32) {
				if (!crc) {
					if (swdmx_crc32(pid_filter->sec_data,
							pid_filter->sec_recv)) {
						swdmx_log("section crc error");
						break;
					}

					crc = SWDMX_TRUE;
				}
			}

			if (!sec_filter_match(pid_filter->sec_data,
					      pid_filter->sec_recv,
					      sec_filter)) {
				swdmx_log("not sec_filter_match\n");
				continue;
			}

			SWDMX_LIST_FOR_EACH_SAFE(ce, nce, &sec_filter->cb_list,
						 ln) {
				swdmx_sec_cb cb = ce->cb;

				cb(pid_filter->sec_data, pid_filter->sec_recv,
				   ce->data);
			}
		}

		pid_filter->sec_recv = 0;
	}
	swdmx_log("%s exit\n", __func__);
	return len - left;
}

/*PID filter receive TS data.*/
static void
pid_filter_data(struct swdmx_pid_filter *pid_filter, struct swdmx_tspacket *pkt)
{
	struct swdmx_tsfilter *ts_filter, *n_ts_filter;
	struct swdmx_cb_entry *ce, *nce;
	u8 *p;
	int left;

	swdmx_log("%s line:%d\n", __func__, __LINE__);

	/*TS filters. */
	SWDMX_LIST_FOR_EACH_SAFE(ts_filter,
				 n_ts_filter,
				 &pid_filter->ts_filter_list, pid_ln) {
		SWDMX_LIST_FOR_EACH_SAFE(ce, nce, &ts_filter->cb_list, ln) {
			swdmx_tspacket_cb cb = ce->cb;

			cb(pkt, ce->data);
		}
	}

	if (swdmx_list_is_empty(&pid_filter->sec_filter_list))
		return;

	if (!pkt->payload || pkt->scramble)
		return;

	/*Solve section data. */
	if (!pid_filter->sec_data) {
		pid_filter->sec_data = swdmx_malloc(4096 + 3);
		SWDMX_ASSERT(pid_filter->sec_data);
	}

	p = pkt->payload;
	left = pkt->payload_len;
	if (pkt->payload_start) {
		if (left) {
			int ptr = p[0];

			p++;
			left--;

			if (ptr) {
				if (ptr > left) {
					swdmx_log("illegal section field");
					return;
				}

				if (pid_filter->sec_recv)
					sec_data(pid_filter, p, ptr);

				p += ptr;
				left -= ptr;
			}
		}

		pid_filter->sec_recv = 0;
	}

	while (left > 0) {
		int n;

		n = sec_data(pid_filter, p, left);

		p += n;
		left -= n;
	}
}

struct swdmx_demux *swdmx_demux_new(void)
{
	struct swdmx_demux *dmx;

	dmx = swdmx_malloc(sizeof(struct swdmx_demux));
	SWDMX_ASSERT(dmx);

	swdmx_list_init(&dmx->pid_filter_list);
	swdmx_list_init(&dmx->ts_filter_list);
	swdmx_list_init(&dmx->sec_filter_list);

	return dmx;
}

struct swdmx_tsfilter *swdmx_demux_alloc_ts_filter(struct swdmx_demux *dmx)
{
	struct swdmx_tsfilter *f;

	SWDMX_ASSERT(dmx);

	f = swdmx_malloc(sizeof(struct swdmx_tsfilter));
	SWDMX_ASSERT(f);

	f->dmx = dmx;
	f->state = SWDMX_FILTER_STATE_INIT;
	f->pid_filter = NULL;

	swdmx_list_init(&f->cb_list);

	swdmx_list_append(&dmx->ts_filter_list, &f->ln);

	return f;
}

struct swdmx_secfilter *swdmx_demux_alloc_sec_filter(struct swdmx_demux *dmx)
{
	struct swdmx_secfilter *f;

	SWDMX_ASSERT(dmx);

	f = swdmx_malloc(sizeof(struct swdmx_secfilter));

	f->dmx = dmx;
	f->state = SWDMX_FILTER_STATE_INIT;
	f->pid_filter = NULL;

	swdmx_list_init(&f->cb_list);

	swdmx_list_append(&dmx->sec_filter_list, &f->ln);

	return f;
}

void swdmx_demux_ts_packet_cb(struct swdmx_tspacket *pkt, void *data)
{
	struct swdmx_demux *dmx = (struct swdmx_demux *)data;
	struct swdmx_pid_filter *pid_filter;

	SWDMX_ASSERT(pkt && dmx);
	SWDMX_LIST_FOR_EACH(pid_filter, &dmx->pid_filter_list, ln) {
		if (pkt->pid == pid_filter->pid) {
			pid_filter_data(pid_filter, pkt);
			break;
		}
	}
}

void swdmx_demux_free(struct swdmx_demux *dmx)
{
	SWDMX_ASSERT(dmx);

	while (!swdmx_list_is_empty(&dmx->ts_filter_list)) {
		struct swdmx_tsfilter *f;

		f = SWDMX_CONTAINEROF(dmx->ts_filter_list.next,
				      struct swdmx_tsfilter, ln);

		swdmx_ts_filter_free(f);
	}

	while (!swdmx_list_is_empty(&dmx->sec_filter_list)) {
		struct swdmx_secfilter *f;

		f = SWDMX_CONTAINEROF(dmx->sec_filter_list.next,
				      struct swdmx_secfilter, ln);

		swdmx_sec_filter_free(f);
	}

	while (!swdmx_list_is_empty(&dmx->pid_filter_list)) {
		struct swdmx_pid_filter *f;

		f = SWDMX_CONTAINEROF(dmx->pid_filter_list.next,
				      struct swdmx_pid_filter, ln);

		pid_filter_remove(f);
	}

	swdmx_free(dmx);
}

int
swdmx_ts_filter_set_params(struct swdmx_tsfilter *filter,
			   struct swdmx_tsfilter_params *params)
{
	SWDMX_ASSERT(filter && params);

	if (!swdmx_is_valid_pid(params->pid) || params->pid == 0x1fff) {
		swdmx_log("illegal PID 0x%04x", params->pid);
		return SWDMX_ERR;
	}

	if (filter->state == SWDMX_FILTER_STATE_RUN) {
		SWDMX_ASSERT(filter->pid_filter);

		if (filter->params.pid != params->pid) {
			swdmx_list_remove(&filter->pid_ln);
			pid_filter_remove(filter->pid_filter);

			filter->pid_filter = NULL;
		}
	}

	filter->params = *params;
	if (filter->state == SWDMX_FILTER_STATE_INIT)
		filter->state = SWDMX_FILTER_STATE_SET;

	if (filter->state == SWDMX_FILTER_STATE_RUN && !filter->pid_filter) {
		struct swdmx_pid_filter *pid_filter;

		pid_filter = pid_filter_get(filter->dmx, params->pid);

		filter->pid_filter = pid_filter;

		swdmx_list_append(&pid_filter->ts_filter_list, &filter->pid_ln);
	}

	return SWDMX_OK;
}

int
swdmx_ts_filter_add_ts_packet_cb(struct swdmx_tsfilter *filter,
				 swdmx_tspacket_cb cb, void *data)
{
	SWDMX_ASSERT(filter && cb);

	swdmx_cb_list_add(&filter->cb_list, cb, data);

	return SWDMX_OK;
}

int
swdmx_ts_filter_remove_ts_packet_cb(struct swdmx_tsfilter *filter,
				    swdmx_tspacket_cb cb, void *data)
{
	SWDMX_ASSERT(filter && cb);

	swdmx_cb_list_remove(&filter->cb_list, cb, data);

	return SWDMX_OK;
}

int swdmx_ts_filter_enable(struct swdmx_tsfilter *filter)
{
	SWDMX_ASSERT(filter);

	if (filter->state == SWDMX_FILTER_STATE_INIT) {
		swdmx_log("the ts filter's parameters has not been set");
		return SWDMX_ERR;
	}

	if (filter->state == SWDMX_FILTER_STATE_SET) {
		struct swdmx_pid_filter *pid_filter;

		pid_filter = pid_filter_get(filter->dmx, filter->params.pid);

		filter->pid_filter = pid_filter;
		filter->state = SWDMX_FILTER_STATE_RUN;

		swdmx_list_append(&pid_filter->ts_filter_list, &filter->pid_ln);
	}

	return SWDMX_OK;
}

int swdmx_ts_filter_disable(struct swdmx_tsfilter *filter)
{
	SWDMX_ASSERT(filter);

	if (filter->state == SWDMX_FILTER_STATE_RUN) {
		SWDMX_ASSERT(filter->pid_filter);

		swdmx_list_remove(&filter->pid_ln);
		pid_filter_remove(filter->pid_filter);

		filter->pid_filter = NULL;
		filter->state = SWDMX_FILTER_STATE_SET;
	}

	return SWDMX_OK;
}

void swdmx_ts_filter_free(struct swdmx_tsfilter *filter)
{
	SWDMX_ASSERT(filter);

	swdmx_ts_filter_disable(filter);

	swdmx_list_remove(&filter->ln);
	swdmx_cb_list_clear(&filter->cb_list);

	swdmx_free(filter);
}

int
swdmx_sec_filter_set_params(struct swdmx_secfilter *filter,
			    struct swdmx_secfilter_params *params)
{
	int i;

	SWDMX_ASSERT(filter && params);

	if (!swdmx_is_valid_pid(params->pid) || params->pid == 0x1fff) {
		swdmx_log("illegal PID 0x%04x", params->pid);
		return SWDMX_ERR;
	}

	if (filter->state == SWDMX_FILTER_STATE_RUN) {
		SWDMX_ASSERT(filter->pid_filter);

		if (filter->params.pid != params->pid) {
			swdmx_list_remove(&filter->pid_ln);
			pid_filter_remove(filter->pid_filter);

			filter->pid_filter = NULL;
		}
	}

	filter->params = *params;

	for (i = 0; i < SWDMX_SEC_FILTER_LEN; i++) {
		int j = i ? i + 2 : i;
		u8 v, mask, mode;

		v = params->value[i];
		mask = params->mask[i];
		mode = ~params->mode[i];

		filter->value[j] = v;
		filter->mam[j] = mask & mode;
		filter->manm[j] = mask & ~mode;
	}

	filter->value[1] = 0;
	filter->mam[1] = 0;
	filter->manm[1] = 0;
	filter->value[2] = 0;
	filter->mam[2] = 0;
	filter->manm[2] = 0;

	if (filter->state == SWDMX_FILTER_STATE_INIT)
		filter->state = SWDMX_FILTER_STATE_SET;

	if (filter->state == SWDMX_FILTER_STATE_RUN && !filter->pid_filter) {
		struct swdmx_pid_filter *pid_filter;

		pid_filter = pid_filter_get(filter->dmx, params->pid);

		filter->pid_filter = pid_filter;

		swdmx_list_append(&pid_filter->sec_filter_list,
				  &filter->pid_ln);
	}

	return SWDMX_OK;
}

int
swdmx_sec_filter_add_section_cb(struct swdmx_secfilter *filter,
				swdmx_sec_cb cb, void *data)
{
	SWDMX_ASSERT(filter && cb);

	swdmx_cb_list_add(&filter->cb_list, cb, data);

	return SWDMX_OK;
}

int
swdmx_sec_filter_remove_section_cb(struct swdmx_secfilter *filter,
				   swdmx_sec_cb cb, void *data)
{
	SWDMX_ASSERT(filter && cb);

	swdmx_cb_list_remove(&filter->cb_list, cb, data);

	return SWDMX_OK;
}

int swdmx_sec_filter_enable(struct swdmx_secfilter *filter)
{
	SWDMX_ASSERT(filter);

	if (filter->state == SWDMX_FILTER_STATE_INIT) {
		swdmx_log("the section filter's parameters has not been set");
		return SWDMX_ERR;
	}

	if (filter->state == SWDMX_FILTER_STATE_SET) {
		struct swdmx_pid_filter *pid_filter;

		pid_filter = pid_filter_get(filter->dmx, filter->params.pid);

		filter->pid_filter = pid_filter;
		filter->state = SWDMX_FILTER_STATE_RUN;

		swdmx_list_append(&pid_filter->sec_filter_list,
				  &filter->pid_ln);
	}

	return SWDMX_OK;
}

int swdmx_sec_filter_disable(struct swdmx_secfilter *filter)
{
	SWDMX_ASSERT(filter);

	if (filter->state == SWDMX_FILTER_STATE_RUN) {
		SWDMX_ASSERT(filter->pid_filter);

		swdmx_list_remove(&filter->pid_ln);
		pid_filter_remove(filter->pid_filter);

		filter->pid_filter = NULL;
		filter->state = SWDMX_FILTER_STATE_SET;
	}

	return SWDMX_OK;
}

void swdmx_sec_filter_free(struct swdmx_secfilter *filter)
{
	SWDMX_ASSERT(filter);
	swdmx_sec_filter_disable(filter);

	swdmx_list_remove(&filter->ln);
	swdmx_cb_list_clear(&filter->cb_list);

	swdmx_free(filter);
}
