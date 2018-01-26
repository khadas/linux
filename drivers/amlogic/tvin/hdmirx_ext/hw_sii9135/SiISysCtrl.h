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

void siiSaveRXInitParameters(BYTE *);
void siiGetRXInitParameters(BYTE *);
BYTE siiInitilizeSystemData(BOOL);

BYTE siiDoTasksTimeDiffrence(void);
WORD siiGetTicksNumber(void);
WORD siiConvertTicksInMS(WORD);
void siiMeasureProcLastAPI_Ticks(WORD);
void siiDiagnostic_GetAPI_ExeTime(BYTE *);
BYTE siiGetSMEventChanges(void);
void siiReInitRX(void);


