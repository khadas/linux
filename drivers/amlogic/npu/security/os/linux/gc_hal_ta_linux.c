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


#include "tee_internal_api.h"
#include "gc_hal_base.h"
#include "gc_hal.h"
#include "gc_hal_ta.h"
#include "gc_hal_ta_linux.h"

gceSTATUS
gctaOS_ConstructOS(
    IN gckOS Os,
    OUT gctaOS *TAos
    )
{
    *TAos = (gctaOS)Os;
    return gcvSTATUS_OK;
}

gceSTATUS
gctaOS_AllocateSecurityMemory(
    IN gctaOS Os,
    IN gctSIZE_T  *Bytes,
    OUT gctPOINTER *Logical,
    OUT gctUINT32 *Physical
    )
{
    gceSTATUS status;
    uint32_t cb_paramTypes = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
            TEE_PARAM_TYPE_NONE,
            TEE_PARAM_TYPE_NONE,
            TEE_PARAM_TYPE_NONE);

    TEE_Param cb_params[4];
    TEE_Result result;

    cb_params[0].value.a = *Bytes;

    /* Call allocation callback. */
    result = TEE_Callback(gcvTA_CALLBACK_ALLOC_SECURE_MEM, cb_paramTypes, cb_params, 0, NULL);

    if (result != TEE_SUCCESS) {
        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

    *Physical = cb_params[0].value.b;
    *Logical = TEE_PhysToVirt(*Physical);

    return gcvSTATUS_OK;

OnError:
    return status;
}

gceSTATUS
gctaOS_FreeSecurityMemory(
    IN gctaOS Os,
    IN gctPOINTER Logical
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS
gctaOS_Allocate(
    IN gctUINT32 Bytes,
    OUT gctPOINTER *Pointer
    )
{
    gceSTATUS status;
    gctPOINTER memory;

    memory = TEE_Malloc(Bytes, 0);

    if (memory == gcvNULL)
    {
        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

    *Pointer = memory;

    return gcvSTATUS_OK;

OnError:
    return status;
}

gceSTATUS
gctaOS_Free(
    IN gctPOINTER Pointer
    )
{
    TEE_Free(Pointer);
    return gcvSTATUS_OK;
}

gceSTATUS
gctaOS_GetPhysicalAddress(
    IN gctaOS Os,
    IN gctPOINTER Logical,
    OUT gctUINT32 * Physical
    )
{
    *Physical = Logical;

    return gcvSTATUS_OK;
}

gceSTATUS
gctaOS_WriteRegister(
    IN gctaOS Os,
    IN gctUINT32 Address,
    IN gctUINT32 Data
    )
{
    TAContext *context = (TAContext *)Os;

    *((gctUINT32 *)(context->registerBase + Address)) = Data;
    return gcvSTATUS_OK;
}

gceSTATUS
gctaOS_ReadRegister(
    IN gctaOS Os,
    IN gctUINT32 Address,
    IN gctUINT32 *Data
    )
{
    TAContext *context = (TAContext *)Os;

    *Data = *((gctUINT32 *)(context->registerBase + Address));

    return gcvSTATUS_OK;
}


gceSTATUS
gctaOS_MemCopy(
    IN gctUINT8_PTR Dest,
    IN gctUINT8_PTR Src,
    IN gctUINT32 Bytes
    )
{
    memcpy(Dest, Src, Bytes);
    return gcvSTATUS_OK;
}

gceSTATUS
gctaOS_ZeroMemory(
    IN gctUINT8_PTR Dest,
    IN gctUINT32 Bytes
    )
{
    memset(Dest, 0, Bytes);
    return gcvSTATUS_OK;
}

void
gctaOS_CacheFlush(
    IN gctUINT8_PTR Dest,
    IN gctUINT32 Bytes
    )
{
    TEE_FlushCache(Dest, Bytes);
}

void
gctaOS_CacheClean(
    IN gctUINT8_PTR Dest,
    IN gctUINT32 Bytes
    )
{
    TEE_CleanCache(Dest, Bytes);
}

void
gctaOS_CacheInvalidate(
    IN gctUINT8_PTR Dest,
    IN gctUINT32 Bytes
    )
{
    TEE_InvalidateCache(Dest, Bytes);
}

void
gctaOS_Delay(
    IN gctUINT32 Milliseconds
    )
{
    TEE_Wait(Milliseconds);
}

void
gckOS_Print(
   IN gctCONST_STRING Message,
   ...
   )
{

}

void
gctaOS_WriteSecurityMemory(
    IN gctPOINTER Pointer,
    IN gctUINT8   Width,
    IN gctUINT32  Data
    )
{
    gctUINT32_PTR pointer = (gctUINT32_PTR)Pointer;

    *pointer = Data;

    gctaOS_CacheFlush(pointer, Width);
}

void
gctaOS_ReadSecurityMemory(
    IN gctPOINTER Pointer,
    IN gctUINT8   Width,
    IN gctUINT32  *Data
    )
{
    gctUINT32_PTR pointer = (gctUINT32_PTR)Pointer;

    gctaOS_CacheInvalidate(pointer, Width);

    *Data  = *pointer;
}

gceSTATUS
gctaOS_CreateMutex(
    IN gctaOS Os,
    OUT gctPOINTER * Mutex
    )
{
    TEE_Result result;

    result = TEE_MutexCreate(Mutex, "MMU");

    if (result != TEE_SUCCESS)
    {
        return gcvSTATUS_OUT_OF_RESOURCES;
    }

    return gcvSTATUS_OK;
}

gceSTATUS
gctaOS_DeleteMutex(
    IN gctaOS Os,
    OUT gctPOINTER Mutex
    )
{
    TEE_MutexDestroy(Mutex);
    return gcvSTATUS_OK;
}

gceSTATUS
gctaOS_AcquireMutex(
    IN gctaOS Os,
    IN gctPOINTER Mutex
    )
{
    TEE_MutexLock(Mutex);
    return gcvSTATUS_OK;
}

gceSTATUS
gctaOS_ReleaseMutex(
    IN gctaOS Os,
    IN gctPOINTER Mutex
    )
{
    TEE_MutexUnlock(Mutex);
    return gcvSTATUS_OK;
}

gceSTATUS
gctaOS_IsPhysicalSecure(
    IN gctaOS Os,
    IN gctUINT32 Physical,
    OUT gctBOOL *Secure
    )
{
    gctUINT32 index = 0;
    TAContext *context = (TAContext *)Os;
    TAMemoryRegion *sr = context->secureRegions;
    gctUINT32 nSr = gcmSIZEOF(context->secureRegions) / gcmSIZEOF(TAMemoryRegion);

    for (index = 0; index < nSr; index++)
    {
        if (Physical >= sr[index].base && Physical < sr[index].base + sr[index].size)
        {
            *Secure = gcvTRUE;
            return gcvSTATUS_OK;
        }
    }

    *Secure = gcvFALSE;

    return gcvSTATUS_OK;
}

