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

enum SiI_PixPrepl {

	SiI_PixRepl1 = 0x00,
	SiI_PixRepl2 = 0x01,
	SiI_PixRepl4 = 0x03

};

BYTE siiGetVideoFormatData(BYTE */*VidFormatData*/);
BYTE siiSetVideoFormatData(BYTE */*VidFormatData*/);
BYTE siiPrepVideoPathSelect(BYTE, BYTE, BYTE */*VidFormatData*/);
BYTE siiPrepSyncSelect(BYTE/*bSyncSelect*/, BYTE */*VidFormatData*/);
BYTE siiPrepSyncCtrl(BYTE/*bSyncCtrl*/, BYTE */*VidFormatData*/);
BYTE siiPrepVideoCtrl(BYTE, BYTE *);

void siiSetVidResDependentVidPath(BYTE, BYTE);
void siiSetVideoPathColorSpaceDependent(BYTE, BYTE);
void siiSetStaticVideoPath(BYTE, BYTE);
void siiMuteVideo(BYTE);

