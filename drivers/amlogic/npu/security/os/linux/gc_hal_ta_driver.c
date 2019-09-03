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


#include "gc_hal_types.h"
#include "gc_hal_base.h"
#include "gc_hal_security_interface.h"
#include "gc_hal_ta.h"
#include "tee_internal_api.h"

#include "gc_hal_ta_linux.h"

#define TA_EXPORT

#define gcmCOUNTOF(a) \
(\
    sizeof(a) / sizeof(a[0]) \
)

static TEE_Result TA_EXPORT TA_CreateEntryPoint(void)
{
    printf("TA_CreateEntryPoint\n");
    return TEE_SUCCESS;
}

static void TA_EXPORT TA_DestroyEntryPoint(void)
{
    printf("TA_DestroyEntryPoint\n");
}

static TEE_Result TA_EXPORT TA_OpenSessionEntryPoint(
    uint32_t        paramTypes,
    TEE_Param        params[4],
    void**            sessionContext)
{
    gceSTATUS status;
    TAContext *context;
    gctUINT32 core;
    uint32_t i = 0;
    uint32_t nRegions = 0;
    TEE_MemRegion *regions = NULL;

    gcmkONERROR(
        gctaOS_Allocate(sizeof(TAContext), (gctPOINTER *)&context));

    gctaOS_ZeroMemory(context, sizeof(TAContext));

    if (params[0].value.a == 0)
    {
        context->registerBase = gcv3D_REGISTER_BASE;
    }
    else
    {
        context->registerBase = gcv2D_REGISTER_BASE;
    }

    /* Query secure memory regions. */
    nRegions = TEE_GetMemRegionCount(TZ_M_NRW, TZ_M_NRW);
    gcmkONERROR(
        gctaOS_Allocate(nRegions * sizeof(TEE_MemRegion), (gctPOINTER *)&regions));
    TEE_GetMemRegionList(regions, nRegions, TZ_M_NRW, TZ_M_NRW);

    if (nRegions >= gcmCOUNTOF(context->secureRegions))
    {
        printf("Too many secure pools.\n");
    }

    for (i = 0; i < nRegions; i++)
    {
        printf("===region %d, name %s, attrib 0x%x, (0x%8x, 0x%8x)===\n",
               i, regions[i].name, regions[i].attr, regions[i].base, regions[i].base + regions[i].size);
        context->secureRegions[i].base = regions[i].base;
        context->secureRegions[i].size = regions[i].size;
    }

    /* Construct gctaOS object. */
    gcmkONERROR(gctaOS_ConstructOS((gckOS)context, &context->os));

    /* Store context. */
    *sessionContext = context;

    return TEE_SUCCESS;

OnError:
    if (context)
    {
    }

    return TEE_ERROR_STORAGE_NO_SPACE;
}

static void TA_EXPORT TA_CloseSessionEntryPoint(void*  sessionContext)
{
    return;
}

static TEE_Result TA_EXPORT TA_InvokeCommandEntryPoint(
    void*            sessionContext,
    uint32_t        commandID,
    uint32_t        paramTypes,
    TEE_Param        params[4])
{
    gceSTATUS status;
    TAContext *context = sessionContext;
    void *buffer;
    gctUINT32 len;
    gcsTA_INTERFACE interface;

    switch (commandID)
    {
    case gcvTA_COMMAND_INIT:
        /* Construct gcTA object. */
        gcmkONERROR(gcTA_Construct(context->os, &context->ta));

        break;

    case gcvTA_COMMAND_DISPATCH:
        buffer = params[0].memref.buffer;
        len = params[0].memref.size;

        if (buffer == gcvNULL)
        {
            gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
        }

        if (len != sizeof(gcsTA_INTERFACE))
        {
            gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
        }

        TEE_MemMove(&interface, buffer, len);

        gcmkONERROR(gcTA_Dispatch(context->ta, &interface));

        TEE_MemMove(buffer, &interface, len);

        break;

    default:
        break;
    }

    return TEE_SUCCESS;

OnError:
    return TEE_ERROR_GENERIC;
}

/******************************************************************************\
****************************************  Entry ********************************
\******************************************************************************/

#define GPU3D_UUID   { 0xcc9f80ea, 0xa836, 0x11e3, { 0x9b, 0x07, 0x78, 0x2b, 0xcb, 0x5c, 0xf3, 0xe3 } }

const TEE_TA galcoreTA = {
    .uuid               = GPU3D_UUID,
    .name               = "galcore",
    .singleInstance     = false,
    .multiSession       = false,
    .instanceKeepAlive  = false,

    .Create             = TA_CreateEntryPoint,
    .Destroy            = TA_DestroyEntryPoint,
    .OpenSession        = TA_OpenSessionEntryPoint,
    .CloseSession       = TA_CloseSessionEntryPoint,
    .InvokeCommand      = TA_InvokeCommandEntryPoint,
};

uint32_t __init(void)
{
    return (uint32_t)&galcoreTA;
}

uint32_t __fini(void)
{
    return 0;
}

