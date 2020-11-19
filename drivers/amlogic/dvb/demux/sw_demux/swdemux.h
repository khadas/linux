/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _SWDEMUX_H
#define _SWDEMUX_H

#include <linux/types.h>
//#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**Boolean value true.*/
#define SWDMX_TRUE  1
/**Boolean value false.*/
#define SWDMX_FALSE 0

/**Function result: OK.*/
#define SWDMX_OK   1
/**Function result: no error but do nothing.*/
#define SWDMX_NONE 0
/**Function result: error.*/
#define SWDMX_ERR -1

/**Section data callback function.*/
typedef void (*swdmx_sec_cb)(u8   *data,
			int     len,
			void      *udata);

/**Length of the section filter.*/
#define SWDMX_SEC_FILTER_LEN 16

/**TS packet filter's parameters.*/
struct swdmx_tsfilter_params {
	u16 pid; /**< PID of the stream.*/
};

/**Section filter's parameters.*/
struct swdmx_secfilter_params {
	u16 pid;                         /**< PID of the section.*/
	u8   crc32;                       /**< CRC32 check.*/
	u8  value[SWDMX_SEC_FILTER_LEN]; /**< Value array.*/
	u8  mask[SWDMX_SEC_FILTER_LEN];  /**< Mask array.*/
	u8  mode[SWDMX_SEC_FILTER_LEN];  /**< Match mode array.*/
};

/**TS packet.*/
struct swdmx_tspacket {
	u16  pid;           /**< PID.*/
	u8    payload_start; /**< Payload start flag.*/
	u8    priority;      /**< TS packet priority.*/
	u8    error;         /**< Error flag.*/
	u8   scramble;      /**< Scramble flag.*/
	u8   cc;            /**< Continuous counter.*/
	u8  *packet;        /**< Packet buffer.*/
	int     packet_len;    /**< Packet length.*/
	u8  *adp_field;     /**< Adaptation field buffer.*/
	int     adp_field_len; /**< Adaptation field length.*/
	u8  *payload;       /**< Payload buffer.*/
	int     payload_len;   /**< Payload length.*/
};

/**DVBCSA2 parameter.*/
enum {
	SWDMX_DVBCSA2_PARAM_ODD_KEY,  /**< 8 bytes odd key.*/
	SWDMX_DVBCSA2_PARAM_EVEN_KEY  /**< 8 bytes even key.*/
};

/**Scrambled data alignment.*/
enum {
	SWDMX_DESC_ALIGN_HEAD, /**< Align to payload head.*/
	SWDMX_DESC_ALIGN_TAIL  /**< Align to payload tail.*/
};

/**AES ECB parameter.*/
enum {
	SWDMX_AES_ECB_PARAM_ODD_KEY,  /**< 16 bytes odd key.*/
	SWDMX_AES_ECB_PARAM_EVEN_KEY, /**< 16 bytes even key.*/
	SWDMX_AES_ECB_PARAM_ALIGN     /**< Alignment.*/
};

/**AES CBC parameter.*/
enum {
	SWDMX_AES_CBC_PARAM_ODD_KEY,  /**< 16 bytes odd key.*/
	SWDMX_AES_CBC_PARAM_EVEN_KEY, /**< 16 bytes even key.*/
	SWDMX_AES_CBC_PARAM_ALIGN,    /**< Align to head.*/
	SWDMX_AES_CBC_PARAM_ODD_IV,   /**< 16 bytes odd key's IV data.*/
	SWDMX_AES_CBC_PARAM_EVEN_IV   /**< 16 bytes even key's ID data.*/
};

/**TS packet callback function.*/
typedef void (*swdmx_tspacket_cb)(struct swdmx_tspacket  *pkt,
			void        *udata);

/**
 * Create a new TS packet parser.
 * \return The new TS parser.
 */
extern struct swdmx_ts_parser*
swdmx_ts_parser_new(void);

/**
 * Set the TS packet size.
 * \param tsp The TS parser.
 * \param size The packet size.
 * \retval SWDMX_OK On success.
 * \retval SWDMX_ERR On error.
 */
extern int
swdmx_ts_parser_set_packet_size(struct swdmx_ts_parser *tsp,
			int       size);

/**
 * Add a TS packet callback function to the TS parser.
 * \param tsp The TS parser.
 * \param cb The callback function.
 * \param data The user defined data used as the callback's parameter.
 * \retval SWDMX_OK On success.
 * \retval SWDMX_ERR On error.
 */
extern int
swdmx_ts_parser_add_ts_packet_cb(struct swdmx_ts_parser   *tsp,
			swdmx_tspacket_cb  cb,
			void          *data);

/**
 * Remove a TS packet callback function from the TS parser.
 * \param tsp The TS parser.
 * \param cb The callback function.
 * \param data The user defined data used as the callback's parameter.
 * \retval SWDMX_OK On success.
 * \retval SWDMX_ERR On error.
 */
extern int
swdmx_ts_parser_remove_ts_packet_cb(struct swdmx_ts_parser   *tsp,
			swdmx_tspacket_cb  cb,
			void          *data);

/**
 * Parse TS data.
 * \param tsp The TS parser.
 * \param data Input TS data.
 * \param len Input data length in bytes.
 * \return Parse data length in bytes.
 */
extern int
swdmx_ts_parser_run(struct swdmx_ts_parser *tsp,
			u8    *data,
			int       len);

/**
 * Free an unused TS parser.
 * \param tsp The TS parser to be freed.
 */
extern void
swdmx_ts_parser_free(struct swdmx_ts_parser *tsp);

/**
 * Create a new demux.
 * \return The new demux.
 */
extern struct swdmx_demux*
swdmx_demux_new(void);

/**
 * Allocate a new TS packet filter from the demux.
 * \param dmx The demux.
 * \return The new TS packet filter.
 */
extern struct swdmx_tsfilter*
swdmx_demux_alloc_ts_filter(struct swdmx_demux *dmx);

/**
 * Allocate a new section filter from the demux.
 * \param dmx The demux.
 * \return The new section filter.
 */
extern struct swdmx_secfilter*
swdmx_demux_alloc_sec_filter(struct swdmx_demux *dmx);

/**
 * TS packet input function of the demux.
 * \param pkt Input TS packet.
 * \param dmx The demux.
 */
extern void
swdmx_demux_ts_packet_cb(struct swdmx_tspacket *pkt,
			void        *dmx);

/**
 * Free an unused demux.
 * \param dmx The demux to be freed.
 */
extern void
swdmx_demux_free(struct swdmx_demux *dmx);

/**
 * Set the TS filter's parameters.
 * \param filter The filter.
 * \param params Parameters of the filter.
 * \retval SWDMX_OK On success.
 * \retval SWDMX_ERR On error.
 */
extern int
swdmx_ts_filter_set_params(struct swdmx_tsfilter       *filter,
			struct swdmx_tsfilter_params *params);

/**
 * Add a TS packet callback to the TS filter.
 * \param filter The TS filter.
 * \param cb The callback function.
 * \param data User defined data used as callback's parameter.
 * \retval SWDMX_OK On success.
 * \retval SWDMX_ERR On error.
 */
extern int
swdmx_ts_filter_add_ts_packet_cb(struct swdmx_tsfilter   *filter,
			swdmx_tspacket_cb  cb,
			void          *data);

/**
 * Remove a TS packet callback from the TS filter.
 * \param filter The TS filter.
 * \param cb The callback function.
 * \param data User defined data used as callback's parameter.
 * \retval SWDMX_OK On success.
 * \retval SWDMX_ERR On error.
 */
extern int
swdmx_ts_filter_remove_ts_packet_cb(struct swdmx_tsfilter   *filter,
			swdmx_tspacket_cb  cb,
			void          *data);

/**
 * Enable the TS filter.
 * \param filter The TS filter.
 * \retval SWDMX_OK On success.
 * \retval SWDMX_ERR On error.
 */
extern int
swdmx_ts_filter_enable(struct swdmx_tsfilter *filter);

/**
 * Disable the TS filter.
 * \param filter The filter.
 * \retval SWDMX_OK On success.
 * \retval SWDMX_ERR On error.
 */
extern int
swdmx_ts_filter_disable(struct swdmx_tsfilter *filter);

/**
 * Free an unused TS filter.
 * \param filter The ts filter to be freed.
 */
extern void
swdmx_ts_filter_free(struct swdmx_tsfilter *filter);

/**
 * Set the section filter's parameters.
 * \param filter The section filter.
 * \param params Parameters of the filter.
 * \retval SWDMX_OK On success.
 * \retval SWDMX_ERR On error.
 */
extern int
swdmx_sec_filter_set_params(struct swdmx_secfilter       *filter,
			struct swdmx_secfilter_params *params);

/**
 * Add a section callback to the section filter.
 * \param filter The section filter.
 * \param cb The callback function.
 * \param data User defined data used as callback's parameter.
 * \retval SWDMX_OK On success.
 * \retval SWDMX_ERR On error.
 */
extern int
swdmx_sec_filter_add_section_cb(struct swdmx_secfilter *filter,
			swdmx_sec_cb      cb,
			void         *data);

/**
 * Remove a section callback from the section filter.
 * \param filter The section filter.
 * \param cb The callback function.
 * \param data User defined data used as callback's parameter.
 * \retval SWDMX_OK On success.
 * \retval SWDMX_ERR On error.
 */
extern int
swdmx_sec_filter_remove_section_cb(struct swdmx_secfilter *filter,
			swdmx_sec_cb      cb,
			void         *data);

/**
 * Enable the section filter.
 * \param filter The section filter.
 * \retval SWDMX_OK On success.
 * \retval SWDMX_ERR On error.
 */
extern int
swdmx_sec_filter_enable(struct swdmx_secfilter *filter);

/**
 * Disable the section filter.
 * \param filter The section filter.
 * \retval SWDMX_OK On success.
 * \retval SWDMX_ERR On error.
 */
extern int
swdmx_sec_filter_disable(struct swdmx_secfilter *filter);

/**
 * Free an unused section filter.
 * \param filter The section filter to be freed.
 */
extern void
swdmx_sec_filter_free(struct swdmx_secfilter *filter);

#ifdef __cplusplus
}
#endif

#endif

