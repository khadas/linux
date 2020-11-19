// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "swdemux_internal.h"

void swdmx_cb_list_add(struct swdmx_list *l, void *cb, void *data)
{
	struct swdmx_cb_entry *ent;

	SWDMX_ASSERT(l && cb);

	ent = swdmx_malloc(sizeof(struct swdmx_cb_entry));
	SWDMX_ASSERT(ent);

	ent->cb = cb;
	ent->data = data;

	swdmx_list_append(l, &ent->ln);
}

void swdmx_cb_list_remove(struct swdmx_list *l, void *cb, void *data)
{
	struct swdmx_cb_entry *ent, *nent;

	SWDMX_LIST_FOR_EACH_SAFE(ent, nent, l, ln) {
		if (ent->cb == cb && ent->data == data) {
			swdmx_list_remove(&ent->ln);
			swdmx_free(ent);
			break;
		}
	}
}

void swdmx_cb_list_clear(struct swdmx_list *l)
{
	struct swdmx_cb_entry *ent, *nent;

	SWDMX_LIST_FOR_EACH_SAFE(ent, nent, l, ln) {
		swdmx_list_remove(&ent->ln);
		swdmx_free(ent);
	}

	swdmx_list_init(l);
}
