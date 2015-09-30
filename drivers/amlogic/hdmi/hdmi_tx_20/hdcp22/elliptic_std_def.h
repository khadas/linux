/*-----------------------------------------------------------------------
//
// Proprietary Information of Elliptic Technologies
// Copyright (C) 2001-2011, all rights reserved
// Elliptic Technologies, Inc.
//
// As part of our confidentiality  agreement, Elliptic Technologies and
// the Company, as  a  Receiving Party, of  this  information  agrees to
// keep strictly  confidential  all Proprietary Information  so received
// from Elliptic Technologies. Such Proprietary Information can be used
// solely for  the  purpose  of evaluating  and/or conducting a proposed
// business  relationship  or  transaction  between  the  parties.  Each
// Party  agrees  that  any  and  all  Proprietary  Information  is  and
// shall remain confidential and the property of Elliptic Technologies.
// The  Company  may  not  use  any of  the  Proprietary  Information of
// Elliptic Technologies for any purpose other  than  the  above-stated
// purpose  without the prior written consent of Elliptic Technologies.
//.
//-----------------------------------------------------------------------
//
// Project:
//
// TEE (Trusted Execution Environment)
//
// Description:
//
//
// This file defines the basic standard types for tee_sdk.
// Version 0.0 October 11, 2011
//
//-----------------------------------------------------------------------*/

#ifndef __ELLIPTIC_STD_DEF_H__
#define __ELLIPTIC_STD_DEF_H__

#ifndef __KERNEL__
/**
* \defgroup CALDef Common Definitions
*
* This section defines the common shared types
*
* \addtogroup CALDef
* @{
*
*/

/**
* \defgroup SysStdTypes Standard Types
* This section defines the common SHARED STANDARD types
* \addtogroup SysStdTypes
* @{
*
*/

/**
* \ingroup SysStdTypes Standard Types
* include <stddef.h>\n
* > - C89 compliant: if this is not available then add your definition here
*
*/
/* C89 compliant - if this is not available then add your definition here */
#include <stddef.h>
/**
* \ingroup SysStdTypes Standard Types
* include <stdint.h>\n
* > - C99 compliant: if this is not available then add your definition here
*
*/
/* C99 compliant - if this is not available then add your definition here */
#include <stdint.h>
/**
* \ingroup SysStdTypes Standard Types
* include <stdarg.h>\n
* > - C89 compliant: if this is not available then add your definition here
*
*/
/* C89 compliant -  if this is not available then add your definition here */
#include <stdarg.h>

/**
* @}
*/
/**
* @}
*/

#else
#include <linux/types.h>
#include <linux/kernel.h>
#endif

#endif /* __ELLIPTIC_STD_DEF_H__ */

