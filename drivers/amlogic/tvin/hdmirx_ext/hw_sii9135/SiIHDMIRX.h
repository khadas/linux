/*------------------------------------------------------------------------------
// Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
//
// No part of this work may be reproduced, modified, distributed, transmitted,
// transcribed, or translated into any language or computer format, in any form
// or by any means without written permission of: Silicon Image, Inc.,
// 1060 East Arques Avenue, Sunnyvale, California 94085
//----------------------------------------------------------------------------*/

void siiGetRXDeviceInfo(BYTE *);
BOOL siiIsHDMI_Mode(void);
BOOL siiInitializeRX(BYTE *);
void siiRX_PowerDown(BYTE);
void siiRX_GlobalPower(BYTE);
void siiSetMasterClock(BYTE);

BOOL siiRX_CheckCableHPD(void);
void siiClearBCHCounter(void);
void siiRX_DisableTMDSCores(void);
void sii_SetVideoOutputPowerDown(BYTE);
void siiSetAutoSWReset(BOOL);
void siiRXHardwareReset(void);
void siiSetAFEClockDelay(void);
BOOL siiCheckSupportDeepColorMode(void);

void siiSetHBRFs(BOOL);
void siiSetNormalTerminationValueCh1(BOOL);
void siiSetNormalTerminationValueCh2(BOOL);

