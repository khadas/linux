/*------------------------------------------------------------------------------
// Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
//
// No part of this work may be reproduced, modified, distributed, transmitted,
// transcribed, or translated into any language or computer format, in any form
// or by any means without written permission of: Silicon Image, Inc.,
// 1060 East Arques Avenue, Sunnyvale, California 94085
//----------------------------------------------------------------------------*/

#include "SiITypeDefs.h"
#include "SiIRXAPIDefs.h"
#include "SiIGlob.h"

/*------------------------------------------------------------------------------
// System Commands
//----------------------------------------------------------------------------*/
BYTE SiI_RX_GetAPI_Info(BYTE *);
BYTE SiI_RX_InitializeSystem(BYTE *);
BYTE SiI_RX_GlobalPower(BYTE);
BYTE SiI_RX_SetVideoInput(BYTE);
BYTE SiI_RX_SetAudioVideoMute(BYTE);
BYTE SiI_RX_DoTasks(BYTE *);
BYTE SiI_RX_GetSystemInformation(BYTE *);

/*------------------------------------------------------------------------------
// Packet Commands
//----------------------------------------------------------------------------*/
BYTE SiI_RX_GetPackets(BYTE, BYTE *);

/*------------------------------------------------------------------------------
// Video Commands
//----------------------------------------------------------------------------*/
BYTE SiI_RX_SetVideoOutputFormat(BYTE, BYTE, BYTE, BYTE);
BYTE SiI_RX_GetVideoOutputFormat(BYTE *);
BYTE SiI_RX_GetVideoInputResolution(BYTE *);

/*------------------------------------------------------------------------------
// Audio Commands
//----------------------------------------------------------------------------*/
BYTE SiI_RX_SetAudioOutputFormat(WORD, WORD, BYTE, BYTE);
BYTE SiI_RX_GetAudioOutputFormat(BYTE *);
BYTE SiI_RX_GetAudioInputStatus(BYTE *);

/*------------------------------------------------------------------------------
// Common Commands
//----------------------------------------------------------------------------*/
BYTE SiI_GetWarnings(BYTE *);
BYTE SiI_GetErrors(BYTE *);

/*------------------------------------------------------------------------------
// Diagnostics Commands
//----------------------------------------------------------------------------*/
BYTE SiI_Diagnostic_GetNCTS(BYTE *);
BYTE SiI_Diagnostic_GetABKSV(BYTE *);
BYTE SiI_Diagnostic_GetAPIExeTime(BYTE *);

/* Command has been removed from API 1.0 specification
BYTE SiI_RX_GetTasksSchedule(BYTE *);
*/



