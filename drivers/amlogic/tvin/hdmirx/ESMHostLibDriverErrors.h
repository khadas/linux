/**
* \defgroup APIErrors ESM Error Code Definitions
*
* \addtogroup APIErrors
* @{
*
*/

/*-----------------------------------------------------------------------
//
// Proprietary Information of Elliptic Technologies
// Copyright (C) 2001-2014, all rights reserved
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
//
//-----------------------------------------------------------------------
//
// Project:
//
// ESM Host Library.
//
// Description:
//
// Error codes.
//
//-----------------------------------------------------------------------*/

#ifndef _ESMDRIVERERROR_H_
#define _ESMDRIVERERROR_H_

/**
* \defgroup HostLibErrors General Library Errors
*
* The following are error code definitions produced
* by the API library.
*
* \addtogroup HostLibErrors
* @{
*
*/

#define ESM_HL_DRIVER_SUCCESS                0
#define ESM_HL_DRIVER_FAILED               (-1)
#define ESM_HL_DRIVER_NO_MEMORY            (-2)
#define ESM_HL_DRIVER_NO_ACCESS            (-3)
#define ESM_HL_DRIVER_INVALID_PARAM        (-4)
#define ESM_HL_DRIVER_TOO_MANY_ESM_DEVICES (-5)
#define ESM_HL_DRIVER_USER_DEFINED_ERROR   (-6)
/* anything beyond this error code is user defined */

/* End of APIErrors group */
/**
* @}
*/
/**
* @}
*/

#endif
