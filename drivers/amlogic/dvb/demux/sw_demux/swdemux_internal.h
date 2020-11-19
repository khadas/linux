/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _SWDEMUX_INTERNAL_H
#define _SWDEMUX_INTERNAL_H

#include "swdemux.h"

#ifdef __KERNEL__
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/stddef.h>
#include <linux/module.h>
#include "../dmx_log.h"
#else
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**Double linked list node.*/
struct swdmx_list {
	struct swdmx_list *prev; /**< The previous node in the list.*/
	struct swdmx_list *next; /**< The next node in the list.*/
};

/**Callback entry.*/
struct swdmx_cb_entry {
	struct swdmx_list ln;    /**< List node data.*/
	void *cb;    /**< Callback function.*/
	void *data;  /**< User defined data.*/
};

/**TS packet parser.*/
struct swdmx_ts_parser {
	int  packet_size; /**< Packet size.*/
	struct swdmx_list cb_list;     /**< Callback list.*/
};

/**Demux PID filter.*/
struct swdmx_pid_filter {
	struct swdmx_list      ln;              /**< List node data.*/
	u16    pid;             /**< PID.*/
	struct swdmx_list      ts_filter_list;  /**< TS filter list.*/
	struct swdmx_list      sec_filter_list; /**< Section filter list.*/
	u8    *sec_data;        /**< Section data buffer.*/
	int       sec_recv;        /**< Section data received*/
};

/**Demux.*/
struct swdmx_demux {
	struct swdmx_list pid_filter_list; /**< PID filter list.*/
	struct swdmx_list ts_filter_list;  /**< TS filter list.*/
	struct swdmx_list sec_filter_list; /**< Section filter list.*/
};

/**Filter's state.*/
enum swdmx_filter_state {
	SWDMX_FILTER_STATE_INIT, /**< Initialized.*/
	SWDMX_FILTER_STATE_SET,  /**< Parameters has been set.*/
	SWDMX_FILTER_STATE_RUN   /**< Running.*/
};

/**TS filter.*/
struct swdmx_tsfilter {
	struct swdmx_demux          *dmx;
	/**< The demux contains this filter.*/
	struct swdmx_list            ln;
	/**< List node data.*/
	struct swdmx_list            pid_ln;
	/**< List node used in PID filter.*/
	struct swdmx_pid_filter      *pid_filter;
	/**< The PID filter contains this TS filter.*/
	enum swdmx_filter_state     state;
	/**< State of the filter.*/
	struct swdmx_tsfilter_params  params;
	/**< Parameters.*/
	struct swdmx_list            cb_list;
	/**< Callback list.*/
};

/**Section filter.*/
struct swdmx_secfilter {
	struct swdmx_demux           *dmx;
	/**< The demux contains this filter.*/
	struct swdmx_list             ln;
	/**< List node data.*/
	struct swdmx_list             pid_ln;
	/**< List node used in PID filter.*/
	struct swdmx_pid_filter       *pid_filter;
	/**< The PID filter contains this section filter.*/
	enum swdmx_filter_state      state;
	/**< State of the filter.*/
	struct swdmx_secfilter_params  params;
	/**< Parameters.*/
	u8  value[SWDMX_SEC_FILTER_LEN + 2];
	/**< Value array.*/
	u8  mam[SWDMX_SEC_FILTER_LEN + 2];
	/**< Mask & mode array.*/
	u8  manm[SWDMX_SEC_FILTER_LEN + 2];
	/**< Mask & ~mode array.*/
	struct swdmx_list             cb_list;    /**< Callback list.*/
};

#define SWDMX_ASSERT(a)   do {} while (!(a))
#define SWDMX_PTR2SIZE(p) ((size_t)(p))
#define SWDMX_SIZE2PTR(s) ((void *)(size_t)(s))
#define SWDMX_OFFSETOF(s, m)\
	SWDMX_PTR2SIZE(&((s *)0)->m)
#define SWDMX_CONTAINEROF(p, s, m)\
	((s *)(SWDMX_PTR2SIZE(p) - SWDMX_OFFSETOF(s, m)))

/**
 * Output log message.
 * \param fmt Output format string.
 * \param ... Variable arguments.
 */
/*static inline void
 *swdmx_log(const char *fmt, ...)
 *{
 *	va_list ap;
 *
 *	fprintf(stderr, "SWDMX: ");
 *
 *	va_start(ap, fmt);
 *	vfprintf(stderr, fmt, ap);
 *	va_end(ap);
 *
 *	fprintf(stderr, "\n");
 *}
 */

#define swdmx_log(f, a...) \
	dprintk(LOG_DBG, 0, "%s:" f, __func__, ## a)

/**
 * Check if the PID is valid.
 * \param pid The PID.
 * \retval SWDMX_TRUE The PID is valid.
 * \retval SWDMX_FALSE The PID is invalid.
 */
static inline u8
swdmx_is_valid_pid(u16 pid)
{
	return (pid <= 0x1fff);
}

/**
 * Allocate a new memory buffer.
 * \param size The size of the buffer.
 * \return The new buffer's pointer.
 */
static inline void *
swdmx_malloc(size_t size)
{
	return kmalloc(size, GFP_KERNEL);
}

/**
 * Free an unused buffer.
 * \param ptr The buffer to be freed.
 */
static inline void
swdmx_free(void *ptr)
{
	kfree(ptr);
}

/**
 * Check if the list is empty.
 * \param l The list.
 * \retval SWDMX_TRUE The list is empty.
 * \retval SWDMX_FALSE The list is not empty.
 */
static inline u8
swdmx_list_is_empty(struct swdmx_list *l)
{
	return l->next == l;
}

/**
 * Initialize a list node.
 * \param n The node to be initialize.
 */
static inline void
swdmx_list_init(struct swdmx_list *n)
{
	n->prev = n;
	n->next = n;
}

/**
 * Append a node to the list.
 * \param l The list.
 * \param n The node to be added.
 */
static inline void
swdmx_list_append(struct swdmx_list *l, struct swdmx_list *n)
{
	n->prev = l->prev;
	n->next = l;
	l->prev->next = n;
	l->prev = n;
}

/**
 * Remove a node from the list.
 * \param n The node to be removed.
 */
static inline void
swdmx_list_remove(struct swdmx_list *n)
{
	n->prev->next = n->next;
	n->next->prev = n->prev;
}

/*Traverse the nodes in the list.*/
#define SWDMX_LIST_FOR_EACH(i, l, m)\
	for ((i) = SWDMX_CONTAINEROF((l)->next, typeof(*i), m);\
				(i) != SWDMX_CONTAINEROF(l, typeof(*i), m);\
				(i) = SWDMX_CONTAINEROF((i)->m.next,\
					typeof(*i), m))

/*Traverse the nodes in the list safely.*/
#define SWDMX_LIST_FOR_EACH_SAFE(i, n, l, m)\
	for ((i) = SWDMX_CONTAINEROF((l)->next, typeof(*i), m),\
				(n) = SWDMX_CONTAINEROF((i)->m.next,\
					typeof(*i), m);\
				(i) != SWDMX_CONTAINEROF(l, typeof(*i), m);\
				(i) = (n),\
				(n) = SWDMX_CONTAINEROF((i)->m.next,\
					typeof(*i), m))

/**
 * Add a callback to the list.
 * \param l The callback list.
 * \param cb The callback function.
 * \param data The user defined parameter.
 */
extern void
swdmx_cb_list_add(struct swdmx_list *l, void *cb, void *data);

/**
 * Remove a callback from the list.
 * \param l The callback list.
 * \param cb The callback function to be removed.
 * \param data The user defined parameter.
 */
extern void
swdmx_cb_list_remove(struct swdmx_list *l, void *cb, void *data);

/**
 * Clear the callback list.
 * \param l The callback list to be cleared.
 */
extern void
swdmx_cb_list_clear(struct swdmx_list *l);

/**
 * CRC32.
 */
extern u32
swdmx_crc32(const u8 *data, int len);

#ifdef __cplusplus
}
#endif

#endif

