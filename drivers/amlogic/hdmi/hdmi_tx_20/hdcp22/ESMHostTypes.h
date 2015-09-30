/*-----------------------------------------------------------------------
//
// Proprietary Information of Elliptic Technologies
// Copyright (C) 2001-2015, all rights reserved
// Elliptic Technologies, Inc.
//
// As part of our confidentiality agreement, Elliptic Technologies and
// the Company, as a Receiving Party, of this information agrees to
// keep strictly confidential all Proprietary Information so received
// from Elliptic Technologies. Such Proprietary Information can be used
// solely for the purpose of evaluating and/or conducting a proposed
// business relationship or transaction between the parties. Each
// Party agrees that any and all Proprietary Information is and
// shall remain confidential and the property of Elliptic Technologies.
// The Company may not use any of the Proprietary Information of
// Elliptic Technologies for any purpose other than the above-stated
// purpose without the prior written consent of Elliptic Technologies.
//
//-----------------------------------------------------------------------
//
// Project:
//
// ESM Host Library.
//
// Description:
//
// API of the host library.
//
//-----------------------------------------------------------------------*/

#ifndef _ESMHOSTTYPES_H_
#define _ESMHOSTTYPES_H_

#include "elliptic_std_def.h"
#include "elliptic_system_types.h"

/* return type for all functions */
#define ESM_STATUS ELP_STATUS

/**
 * \defgroup InitFlags Initialization Flags
 *
 * Options which can be set when calling ESM_Initialize().
 *
 * \addtogroup InitFlags
 * @{
 */
#define ESM_INIT_FLAG_IRQ_SUPPORTED  (1ul << 0)
/**
 * @}
 */

/**
 * \defgroup LogTypes Log message types
 *
 * These are the defined log message types.
 *
 * \addtogroup LogTypes
 * @{
 */
#define ESM_LOG_TYPE_TEXT   0xfd /* Normal text log message. */
#define ESM_LOG_TYPE_BINARY 0xfc /* Binary dump message. */
/**
 * @}
 */

/**
 * \details
 *
 * This structure contains the ESM's last internal status when queried.
 *
 */
struct esm_status_t {
	/**
	* The ESM Exception vector has the following bitfields:
	*
	* <table>
	*  <tr align="center">
	*    <td><b>HW/SW</b></td>
	*    <td><b>Exception Line Number</b></td>
	*    <td><b>Exception Flag</b></td>
	*    <td><b>Type</b></td>
	*  </tr>
	*  <tr>
	*    <td>Bit 31<br>hardware = 1, software = 0</td>
	*    <td>Bits 30..10</td>
	*    <td>Bits 9..1<br>See \ref ExFlagDefines</td>
	*    <td>Bit 0<br>notify = 1, abort = 0</td>
	*  </tr>
	* </table>
	*/
	uint32_t esm_exception;
	/* Indicates that the synchronization lost. */
	uint32_t esm_sync_lost;
	/* Indicates that the last AKE Start command was passed. */
	uint32_t esm_auth_pass;
	/* Indicates that the last AKE Start command has failed. */
	uint32_t esm_auth_fail;
};

/**
 * \details
 *
 * This structure will be filled when ESM firmware successfully started
 * and it contains ESM buffers configuration values.
 *
 */
struct esm_config_t {
	/* Indicates what ESM firmware running: 0-unknown; 1-RX; 2-TX. */
	uint32_t esm_type;
	/* Indicates maximum size of a topology slot memory. */
	uint32_t topo_buffer_size;
	/* Indicates amount of topology slot memories. */
	uint8_t  topo_slots;
	/* Indicates maximum size of the topology seed memory. */
	uint8_t  topo_seed_buffer_size;
	/* Indicates maximum size of the logging memory. */
	uint32_t log_buffer_size;
	/* Indicates maximum size of the mailbox memory. */
	uint32_t mb_buffer_size;
	/* Indicates maximum size of the exceptions memory. */
	uint32_t exceptions_buffer_size;
	/* Indicates maximum size of the TX SRM memory. */
	uint32_t srm_buffer_size;
	/* Indicates maximum size of the TX Pairing memory.*/
	uint32_t pairing_buffer_size;
};

/**
 * \details
 *
 * This structure contains a text log message which can be displayed
 * directly to the user.
 */
struct esm_logmsg_text_t {
	char *file; /* Firmware source filename. */
	char *msg; /* Human-readable log message. */
	uint16_t line; /* Firmware source line number. */
};

struct esm_logmsg_bin_t {
	char *msg; /* Human-readable description. */
	uint8_t *data; /* Pointer to first byte of data. */
	uint16_t datalen; /* Number of data bytes. */
};

/**
 * \details
 *
 * This structure contains a parsed log message from the ESM.  The ESM
 * has different types of log messages, which are distinguished by the
 * type value.  The actual message (which depends on the type) can be
 * accessed through the appropriate union member.
 */
struct esm_logmsg_t {
	/* Raw (unparsed) message length (in bytes). */
	uint16_t rawlen;
	/* Type of log message.
	 * See \ref LogTypes for a list of supported message types. */
	uint8_t type;
	/* Log message ID (-1 if no ID). */
	int32_t id;
	/* Pointer to raw (unparsed) message. */
	uint8_t *raw;

	union {
		/* Normal text log message.
		 * Only valid if type is #ESM_LOG_TYPE_TEXT.*/
		struct esm_logmsg_text_t *text;
		/*Binary dump message.
		 * Only valid if type is #ESM_LOG_TYPE_BINARY.*/
		struct esm_logmsg_bin_t  *bin;
	} u; /* Union for decoded message data. */
};

#endif

