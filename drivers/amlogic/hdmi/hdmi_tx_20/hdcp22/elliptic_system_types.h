/*-----------------------------------------------------------------------
//
// Proprietary Information of Elliptic Technologies.
// Copyright (C) 2001-2014, all rights reserved.
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
//   CAL.
//
// Description:
//
//
// This header file contains Elliptic system type Interface definitions.
//
//
//-----------------------------------------------------------------------
//
// Language:         C.
//
//
//
//
//-----------------------------------------------------------------------*/

#ifndef __ELLIPTIC_SYSTEM_TYPE_H__
#define __ELLIPTIC_SYSTEM_TYPE_H__

/**
 * \defgroup CALDef Common Definitions
 *
 * This section defines the common shared types
 *
 * \addtogroup CALDef
 * \{
 *
 */

/**
 * \defgroup SysTypes System Types
 *
 * This section defines the common SHARED SYSTEM types
 *
 * \addtogroup SysTypes
 * \{
 *
 */

/* System types definitions. */
#define ELP_STATUS                      int16_t
#define ELP_SYSTEM_INFO                 int16_t
#define ELP_SYSTEM_BIG_ENDIAN           0
#define ELP_SYSTEM_LITLLE_ENDIAN        1

/* PRNG definitions. */
#define PRINT_FUNC      printf_ptr
#define PRINT_FUNC_DECL int32_t (*PRINT_FUNC)(const void *str, ...)
#define PRNG_FUNC       prng
#define PRNG_FUNC2      prng2
#define PRNG_FUNC_DECL  uint8_t (*PRNG_FUNC)  (void *, void *, uint8_t)
#define PRNG_FUNC2_DECL uint32_t (*PRNG_FUNC2)(void *, void *, uint32_t)

struct elp_std_prng_info {
	void *prng_inst;
	PRNG_FUNC_DECL;
};

struct elp_std_prng_info2 {
	void *prng_inst;
	PRNG_FUNC2_DECL;
};

#define ELP_STD_PRNG_INFO  elp_std_prng_info
#define ELP_STD_PRNG_INFO2 elp_std_prng_info2

/**
 * \}
 */

/**
 * \}
 */

#endif

