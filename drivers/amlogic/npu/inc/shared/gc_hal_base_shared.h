/****************************************************************************
*
*    Copyright (c) 2005 - 2020 by Vivante Corp.  All rights reserved.
*
*    The material in this file is confidential and contains trade secrets
*    of Vivante Corporation. This is proprietary information owned by
*    Vivante Corporation. No part of this work may be disclosed,
*    reproduced, copied, transmitted, or used in any way for any purpose,
*    without the express written permission of Vivante Corporation.
*
*****************************************************************************/


#ifndef __gc_hal_base_shared_h_
#define __gc_hal_base_shared_h_

#ifdef __cplusplus
extern "C" {
#endif

#define gcdEXTERNAL_MEMORY_NAME_MAX 32
#define gcdEXTERNAL_MEMORY_DATA_MAX 8

typedef struct _gcsEXTERNAL_MEMORY_INFO
{
    /* Name of allocator used to attach this memory. */
    gctCHAR                allocatorName[gcdEXTERNAL_MEMORY_NAME_MAX];

    /* User defined data which will be passed to allocator. */
    gctUINT32              userData[gcdEXTERNAL_MEMORY_DATA_MAX];
}
gcsEXTERNAL_MEMORY_INFO;

#define gcdBINARY_TRACE_MESSAGE_SIZE 240

typedef struct _gcsBINARY_TRACE_MESSAGE * gcsBINARY_TRACE_MESSAGE_PTR;
typedef struct _gcsBINARY_TRACE_MESSAGE
{
    gctUINT32   signature;
    gctUINT32   pid;
    gctUINT32   tid;
    gctUINT32   line;
    gctUINT32   numArguments;
    gctUINT8    payload;
}
gcsBINARY_TRACE_MESSAGE;

/* gcsOBJECT object defintinon. */
typedef struct _gcsOBJECT
{
    /* Type of an object. */
    gceOBJECT_TYPE              type;
}
gcsOBJECT;

#ifdef __cplusplus
}
#endif

#endif /* __gc_hal_base_shared_h_ */



