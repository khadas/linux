/****************************************************************************
*
*    Copyright (c) 2005 - 2019 by Vivante Corp.  All rights reserved.
*
*    The material in this file is confidential and contains trade secrets
*    of Vivante Corporation. This is proprietary information owned by
*    Vivante Corporation. No part of this work may be disclosed,
*    reproduced, copied, transmitted, or used in any way for any purpose,
*    without the express written permission of Vivante Corporation.
*
*****************************************************************************/


#define MAX_SECURE_POOL 16

typedef struct _TAMemoryRegion{
    gctUINT32 base;
    gctUINT32 size;
} TAMemoryRegion;

typedef struct _TAContext{
    gctaOS os;
    gcTA   ta;
    gctUINT32 registerBase;
    TAMemoryRegion secureRegions[MAX_SECURE_POOL];
} TAContext;

#define gcv3D_REGISTER_BASE 0xF7BC0000
#define gcv2D_REGISTER_BASE 0xF7EF0000


