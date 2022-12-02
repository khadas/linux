/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_AUCPU_H_
#define _AML_AUCPU_H_

#include <linux/types.h>

/**
 * ERROR  definitions
 **/

#define AUCPU_NO_ERROR				0
#define AUCPU_ERROR_GENERAL			-1
#define AUCPU_ERROR_DEV_INVALID			-2
#define AUCPU_ERROR_BAD_PARAMETER		-3
#define AUCPU_ERROR_NOT_AVAILABLE		-4
#define AUCPU_ERROR_NOT_IMPLEMENTED		-5
#define AUCPU_ERROR_NOT_SUPPORTED		-6
#define AUCPU_ERROR_CALL_SEQUENCE		-7
#define AUCPU_ERROR_DMA_TIMEOUT			-8
#define AUCPU_ERROR_NO_SLOT			-9
#define AUCPU_ERROR_INVALID_MEDIA		-10
#define AUCPU_ERROR_INVALID_STAT		-11
#define AUCPU_ERROR_INPUT_TOO_MANY		-12
#define AUCPU_ERROR_INPUT_NO_MATCH		-13
#define AUCPU_ERROR_LIST_INVALID		-14
#define AUCPU_ERROR_INVALID_DESC		-15
#define AUCPU_DRVERR_CMD_TIMEOUT		-64
#define AUCPU_DRVERR_CMD_NOMATCH		-65
#define AUCPU_DRVERR_NO_INIT			-66
#define AUCPU_DRVERR_TOO_MANY_STRM		-67
#define AUCPU_DRVERR_NO_SLOT			-68
#define AUCPU_DRVERR_INVALID_TYPE		-69
#define AUCPU_DRVERR_NO_ACTIVE			-70
#define AUCPU_DRVERR_INVALID_HANDLE		-71
#define AUCPU_DRVERR_WRONG_STATE		-72
#define AUCPU_DRVERR_VALID_CONTEXT		-72
#define AUCPU_DRVERR_MEM_ALLOC			-73
#define AUCPU_DRVERR_FILE_OPEN			-74
#define AUCPU_DRVERR_MEM_MAP			-75
/**
 * struct dma_link_des
 *
 * DMA linklist structure descriptor
 *
 * @length:	size of the buffer in bytes
 * @irq:	send IRQ when this descriptor is done
 * @eoc:	end of chain, done, send IRQ out
 * @loop:	next descriptor is from beginning, support descriptor loop,
 *		send IRQ out when loop back
 * @error:	reserved
 * @owner:	reserved
 **/

struct dma_link_des {
	union {
		u32 d32;
		struct {
			unsigned length:27;
			unsigned irq:1;
			unsigned eoc:1;
			unsigned loop:1;
			unsigned error:1;
			unsigned owner:1;
		} b;
	} dsc_flag;
	u32 start_addr;
};

/**
 * struct aml_aucpu_strm_buf
 *
 * This struction define the buffer for input/output of the contents
 *
 * @phy_start:	Physical address of the buffer
 * @buf_size:	size of the buffer in bytes, multiple of 8
 * @buf_flags:	buffer configure flags:
 *		bit 0: ring buffer or linklist
 *			0, ring buffer
 *			1, the buffer is a linklist
 *		other bis: reserved
 *		if the buffer is linklist it shall be presented as
 *		struct dma_link_des
 **/
struct aml_aucpu_strm_buf {
	ulong phy_start; /* PHY address of the buffer (32bit-alignment) */
	u32 buf_size; /* bytes, shall be multiply of 8 */
	u32 buf_flags; /* bit 0: 0:the buffer is normal ring buffer */
				 /* 1: it is link list */
};

/**
 * struct aml_aucpu_buf_upd
 *
 * give the updates of a buffer
 *
 * @phy_cur_ptr: Physical address of the current buffer pointer,
 *		read pointer or write pointer based on the context that
 *		it is acted as consumer or producer
 * @byte_cnt: The total bytes count of the buffer was inputed (producer)
 *		or used (consumer)
 **/
struct aml_aucpu_buf_upd {
	ulong phy_cur_ptr; /* Physical address of the current buffer pointer */
			   /* (write pointer or read pointer)               */
	u32 byte_cnt; /* input/out bytes count(lower 32bits)           */
			   /*  of the buffer from start or reset            */
};

/**
 * struct aml_aucpu_inst_config
 *
 * @media_type:		aml_aucpu_stream_type stream types
 * @dma_chn_id:		DMX dma channel id used
 * @config_flags:	config options:
 * @buf_flags:	config options::
 *			bit 0: source wrptr update method
 *				0, automatically via DMX DMA
 *				1, manually by calling functions
 *			other bits: reserved
 *
 * This struction define configurations of an instance
 **/
struct aml_aucpu_inst_config {
	u32 media_type;	/* aml_aucpu_stream_type supported types   */
	u32 dma_chn_id;	/* stream DMX dma channel id               */
			/* Extensions when media_type is MEDIA_PTS_PACK    */
			/*    bit 0-7 : PTS pack dma channel id (>=64)     */
			/*    bit 8-24 :PID of the PTS pack                */
	u32 config_flags;	/* config options:                     */
			/* bit 0: source wrptr update method               */
			/*        0, automatically via DMX DMA dma_id used   */
			/*        1, manually via function call src_update */
			/* other bits reserved                             */
};

/* supported stream media types */
enum aml_aucpu_stream_type {
	MEDIA_UNKNOWN = 0,	/* unknown media */
	MEDIA_MPX = 1,		/* mpeg audio MP2/MP3 */
	MEDIA_AC3 = 2,		/* Dolby AC3/EAC3 */
	MEDIA_AAC_ADTS = 3,	/* AAC-ADTS */
	MEDIA_AAC_LOAS = 4,	/* AAC-LOAS */
	MEDIA_DTS = 5,		/* DTS */
	MEDIA_TS_SYS = 6,	/* TS System information (PAT, PMT, etc) */
	MEDIA_PES_SUB = 7,	/* PES format subtitle/audio stream */
	MEDIA_PTS_PACK = 8,	/* PTS pack in proprietary format */
	MEDIA_MAX,
};

/* current running state of an instance */
enum aml_aucpu_states {
	AUCPU_STA_NONE = 0, /* NONE */
	AUCPU_STA_IDLE = 1, /*created but no start */
	AUCPU_STA_RUNNING = 2, /* normal running after start */
	AUCPU_STA_FLUSHING = 3, /* in FLUSHING before finish*/
	AUCPU_STA_MAX,
};

/* report the instance latest execute result */
enum aml_aucpu_report {
	AUCPU_STS_NONE = 0,
	AUCPU_STS_INST_IDLE = 1, //create but no start
	AUCPU_STS_SEARCH = 2, // search for sync
	AUCPU_STS_IN_SYNC = 3, // in sync, already sync, in good order
	AUCPU_STS_DROP = 4, // validation fail, dropping
	AUCPU_STS_MAX,
};

/* max supported instances number*/
#define AUCPU_MAX_INTST_NUM    32

/**
 * extern s32 aml_aucpu_strm_create()
 * create a stream instance with input output buffers and configurations
 *
 * @aml_aucpu_strm_buf *src:	input source content buffer(s) pointer
 * @aml_aucpu_strm_buf *dst:	output destination buffer(s) pointer
 * @aml_aucpu_inst_config *cfg:  configuration of settins of the instance

 * @return:	handle of the instance if value >=0
 *		error code <0 if errors on the operation
 **/
s32 aml_aucpu_strm_create(struct aml_aucpu_strm_buf *src,
			  struct aml_aucpu_strm_buf *dst,
			  struct aml_aucpu_inst_config *cfg);

/**
 * s32 aml_aucpu_strm_start(s32 handle)
 * start running of the  stream instance
 *
 * @handle :The handle of the created instance
 * @return:	0:success
 *		error code <0 if errors on the operation
 **/
s32 aml_aucpu_strm_start(s32 handle);

/**
 * s32 aml_aucpu_strm_start(s32 handle)
 * stop running of the  stream instance
 *
 * @handle:The handle of the created instance
 * @return:	0:success
 *		error code <0 if errors on the operation
 **/
s32 aml_aucpu_strm_stop(s32 handle);

/**
 * s32 aml_aucpu_strm_flush(s32 handle)
 *flushing the content of the stream instance
 * it will process the left content of the stream
 * then go to stop stage after finished
 *
 * @handle:The handle of the created instance
 * @return:	0:success
 *		error code <0 on the operation
 **/
s32 aml_aucpu_strm_flush(s32 handle);

/**
 * s32 aml_aucpu_strm_remove(s32 handle)
 * remove of the  stream instance
 *
 * @handle:The handle of the created instance
 * @return:	0:success
 *		error code <0 if errors on the operation
 **/
s32 aml_aucpu_strm_remove(s32 handle);

/**
 * s32 aml_aucpu_strm_update_src(s32 handle, struct aml_aucpu_buf_update *upd)
 *
 * this function tell the APUCPU the source buffer has been updated, it is
 * used for the manual update method. (config_flags bit 0 is 1)
 *
 * @handle:The handle of steam instance
 * @upd: The pointer of struct aml_aucpu_buf_upd to give the current stream
 *		soure buffer current write pointer and the total
 *		bytes (lower 32bits) has been written into.
 * @return:	0:success
 *		error code <0 if errors on the operation
 **/
s32 aml_aucpu_strm_update_src(s32 handle, struct aml_aucpu_buf_upd *upd);

/**
 * s32 aml_aucpu_strm_get_dst(s32 handle, struct aml_aucpu_buf_update *upd)
 *
 * this function to get(poll) the result of the output buffer available data
 *
 * @handle:The handle of steam instance
 * @upd: The pointer of struct aml_aucpu_buf_upd to give the current stream
 *		destination buffer current write pointer and the total
 *		bytes (lower 32bits) has been written into.
 * @return:	0:success
 *		error code <0 if errors on the operation
 **/
s32 aml_aucpu_strm_get_dst(s32 handle, struct aml_aucpu_buf_upd *upd);

/**
 * s32 aml_aucpu_strm_get_status(s32 handle, s32 *state, s32 *status)
 *
 * get status of the stream instance
 *
 * @handle:The handle of steam instance
 * @state: pointer to the instance running state (aml_aucpu_states)
 * @report:pointer to the instance latest running results (aml_aucpu_report)
 * @return:	0:success
 *		error code <0 if errors on the operation
 **/
s32 aml_aucpu_strm_get_status(s32 handle, s32 *state, s32 *report);

/**
 * aml_aucpu_strm_get_load_firmware_status(void)
 *
 * get status of load firmware status
 *
 * @return:	0:success
 *		error code <0 if errors on the operation
 **/
s32 aml_aucpu_strm_get_load_firmware_status(void);

#endif /* _AML_AUCPU_H_ */
