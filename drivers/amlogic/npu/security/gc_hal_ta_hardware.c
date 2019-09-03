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
#include "gc_hal.h"


#define _GC_OBJ_ZONE     1
#define SRC_MAX          8
#define RECT_ADDR_OFFSET 3

#define INVALID_ADDRESS  ~0U

/* TA Debug macro. */
#define gcmTA_PRINT(...)

/*
* Parser functions.
*/

#define _STATE(reg)                                                            \
    _State(\
        Context, index, \
        reg ## _Address >> 2, \
        reg ## _ResetValue, \
        reg ## _Count, \
        gcvFALSE, gcvFALSE                                                     \
        )

#define _STATE_X(reg)                                                          \
    _State(\
        Context, index, \
        reg ## _Address >> 2, \
        reg ## _ResetValue, \
        reg ## _Count, \
        gcvTRUE, gcvFALSE                                                      \
        )

#define _STATE_HINT_BLOCK(reg, block, count)                                   \
    _State(\
        Context, index, \
        (reg ## _Address >> 2) + (block << reg ## _BLK), \
        reg ## _ResetValue, \
        count, \
        gcvFALSE, gcvTRUE                                                      \
        )

#define _BLOCK_COUNT(reg)                                                      \
    ((reg ## _Count) >> (reg ## _BLK))

static gctSIZE_T
_State(
    IN gcTA_PARSER Context,
    IN gctSIZE_T Index,
    IN gctUINT32 Address,
    IN gctUINT32 Value,
    IN gctSIZE_T Size,
    IN gctBOOL FixedPoint,
    IN gctBOOL Hinted
    )
{
    gctUINT32_PTR buffer;
    gctSIZE_T align, i;

    /* Determine if we need alignment. */
    align = (Index & 1) ? 1 : 0;

    /* Address correct index. */
    buffer = (Context->buffer == gcvNULL)
        ? gcvNULL
        : Context->buffer;

    if ((buffer == gcvNULL) && (Address + Size > Context->stateCount))
    {
        /* Determine maximum state. */
        Context->stateCount = Address + Size;
    }

    /* Do we need a new entry? */
    if ((Address != Context->lastAddress) || (FixedPoint != Context->lastFixed))
    {
        if (buffer != gcvNULL)
        {
            if (align)
            {
                /* Add filler. */
                buffer[Index++] = 0xDEADDEAD;
            }

            /* LoadState(Address, Count). */
            gcmkASSERT((Index & 1) == 0);

            if (FixedPoint)
            {
                buffer[Index]
                    = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 26:26) - (0 ?
 26:26) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 26:26) - (0 ?
 26:26) + 1))))))) << (0 ?
 26:26))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ?
 26:26) - (0 ?
 26:26) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 25:16) - (0 ?
 25:16) + 1))))))) << (0 ?
 25:16))) | (((gctUINT32) ((gctUINT32) (Size) & ((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:0) - (0 ?
 15:0) + 1))))))) << (0 ?
 15:0))) | (((gctUINT32) ((gctUINT32) (Address) & ((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));
            }
            else
            {
                buffer[Index]
                    = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 26:26) - (0 ?
 26:26) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 26:26) - (0 ?
 26:26) + 1))))))) << (0 ?
 26:26))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ?
 26:26) - (0 ?
 26:26) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 25:16) - (0 ?
 25:16) + 1))))))) << (0 ?
 25:16))) | (((gctUINT32) ((gctUINT32) (Size) & ((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:0) - (0 ?
 15:0) + 1))))))) << (0 ?
 15:0))) | (((gctUINT32) ((gctUINT32) (Address) & ((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));
            }

            /* Walk all the states. */
            for (i = 0; i < Size; i += 1)
            {
                /* Set state to uninitialized value. */
                buffer[Index + 1 + i] = Value;

                /* Set index in state mapping table. */
                Context->map[Address + i].index = Index + 1 + i;
            }
        }

        /* Save information for this LoadState. */
        Context->lastIndex   = Index;
        Context->lastAddress = Address + Size;
        Context->lastSize    = Size;
        Context->lastFixed   = FixedPoint;

        /* Return size for load state. */
        return align + 1 + Size;
    }

    /* Append this state to the previous one. */
    if (buffer != gcvNULL)
    {
        /* Update last load state. */
        buffer[Context->lastIndex] =
            ((((gctUINT32) (buffer[Context->lastIndex])) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 25:16) - (0 ?
 25:16) + 1))))))) << (0 ?
 25:16))) | (((gctUINT32) ((gctUINT32) (Context->lastSize + Size) & ((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)));

        /* Walk all the states. */
        for (i = 0; i < Size; i += 1)
        {
            /* Set state to uninitialized value. */
            buffer[Index + i] = Value;

            /* Set index in state mapping table. */
            Context->map[Address + i].index = Index + i;
        }
    }

    /* Update last address and size. */
    Context->lastAddress += Size;
    Context->lastSize    += Size;

    /* Return number of slots required. */
    return Size;
}

gceSTATUS
_GetValue(
    IN gcTA_PARSER Parser,
    IN gctUINT32 State,
    OUT gctUINT32 *Data
    )
{
    gctUINT32 index;

    /* Is it recorded? */
    if (State >= Parser->stateCount)
    {
        return gcvSTATUS_NOT_FOUND;
    }

    index = Parser->map[State].index;

    if (index == 0)
    {
        return gcvSTATUS_NOT_FOUND;
    }

    /* Get current value. */
    *Data = Parser->buffer[index];
    return gcvSTATUS_OK;
}

gctBOOL
_SecureBuffer(
    IN gcTA_PARSER Parser,
    IN gctUINT32 Address
    )
{
    gceSTATUS status;
    gcTA_MMU mmu = Parser->ta->mmu;
    gctBOOL secure = gcvFALSE;

    status =
        gctaMMU_CheckHardwareAddressSecure(mmu, Address, &secure);

    if (gcmIS_SUCCESS(status) && secure)
    {
        /* Address is marked as security buffer. */
        return gcvTRUE;
    }
    else
    {
        return gcvFALSE;
    }
}

void
_CheckResolve(
    IN OUT gcTA_PARSER Parser
    )
{
    gctUINT32 source, source1;
    gctUINT32 target, target1;
    gctUINT32 windowSize, width, height, srcStride, dstStride;
    gctUINT32 config;
    gctUINT32 resolvePipes = Parser->ta->hardware->resolvePipes;
    gctUINT32 clearControl;
    gctUINT32 sourceEnd, targetEnd;

    _GetValue(Parser, 0x058F, &clearControl);

    if ((((((gctUINT32) (clearControl)) >> (0 ? 17:16)) & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 17:16) - (0 ? 17:16) + 1)))))) ))
    {
        /* Clear doesn't leak data, ignore state check. */
        return;
    }

    if (resolvePipes > 1)
    {
        _GetValue(Parser, 0x05B0, &source);
        _GetValue(Parser, 0x05B8, &target);

        _GetValue(Parser, 0x05B0 + 1, &source1);
        _GetValue(Parser, 0x05B8 + 1, &target1);
    }
    else
    {
        _GetValue(Parser, 0x0582, &source);
        _GetValue(Parser, 0x0584, &target);
    }

    _GetValue(Parser, 0x0588, &windowSize);
    _GetValue(Parser, 0x0583, &srcStride);
    _GetValue(Parser, 0x0585, &dstStride);
    _GetValue(Parser, 0x0581, &config);

    width  = (((((gctUINT32) (windowSize)) >> (0 ? 15:0)) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1)))))) );
    height = (((((gctUINT32) (windowSize)) >> (0 ? 31:16)) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 31:16) - (0 ? 31:16) + 1)))))) );

    /* Get Stride. */
    srcStride = (((((gctUINT32) (srcStride)) >> (0 ? 19:0)) & ((gctUINT32) ((((1 ? 19:0) - (0 ? 19:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 19:0) - (0 ? 19:0) + 1)))))) );
    dstStride = (((((gctUINT32) (dstStride)) >> (0 ? 19:0)) & ((gctUINT32) ((((1 ? 19:0) - (0 ? 19:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 19:0) - (0 ? 19:0) + 1)))))) );

    /* Fixup Stride. */
    if ((((((gctUINT32) (config)) >> (0 ? 7:7)) & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 7:7) - (0 ? 7:7) + 1)))))) ))
    {
        srcStride /= 4;
    }

    if ((((((gctUINT32) (config)) >> (0 ? 14:14)) & ((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 14:14) - (0 ? 14:14) + 1)))))) ))
    {
        dstStride /= 4;
    }


    gcmTA_PRINT("Resolving: 0x%08x 0x%08x => 0x%08x 0x%08x (%d x %d) srcStride=%d destStride=%d",
              source, source1, target, target1, width, height, srcStride, dstStride);

    sourceEnd = source + srcStride * height;
    targetEnd = target + dstStride * height;

    gcmkPRINT("RESOLVE: [%08x,%08x) => [%08x,%08x)", source, sourceEnd, target, targetEnd);

    /* If any part of source is secure memory. */
    if (_SecureBuffer(Parser, source) || _SecureBuffer(Parser, sourceEnd))
    {
        /* None of target can be non-secure. */
        if (!_SecureBuffer(Parser, target) || !_SecureBuffer(Parser, targetEnd))
        {
            Parser->allow = gcvFALSE;
        }
    }
}

void
_CheckDraw(
    IN OUT gcTA_PARSER Parser
    )
{
    gctUINT32 target, targetEnd, stride;
    gctUINT32 xscale, yscale, xoffset, yoffset;
    gctUINT32 left, top, right, bottom;
    gctUINT32 width, height;
    gctINT i, j;
    gctUINT32 sample;
    gctUINT32 textureSampleArray[] = {
        0x0900,
        0x0910,
        0x0920,
        0x0930,
        0x0940,
        0x0950,
        0x0960,
        0x0970,
        0x0980,
        0x0990,
        0x09A0,
        0x09B0,
        0x09C0,
        0x09D0
    };

    gctUINT32 textureAddress = 0x4200;

    gctUINT32 texBlockCount = ((512) >> (4));

    _GetValue(Parser, 0x0280, &xscale);
    _GetValue(Parser, 0x0281, &yscale);
    _GetValue(Parser, 0x0283, &xoffset);
    _GetValue(Parser, 0x0284, &yoffset);

    _GetValue(Parser, 0x0518, &target);
    _GetValue(Parser, 0x050D, &stride);

    /* Viewport. */
    left = (xoffset - xscale) >> 16;
    right = left + (xscale >> 15);

    if (yscale & 0x80000000)
    {
        top = (yoffset + yscale) >> 16;
        bottom = top + (((~yscale) + 1) >> 15);
    }
    else
    {
        top = (yoffset - yscale) >> 16;
        bottom = top + (yscale >> 15);
    }

    width = right - left;
    height = bottom - top;

    /* Memory range. */
    targetEnd = target + height * stride;

    gcmTA_PRINT("draw... %x %d %d %d %d", target, left, top, width, height);

    gcmkPRINT("DRAW   : [%08x,%08x)", target, target + height * stride);

    for (i = 0 ; i < texBlockCount; i++)
    {
        for (j = 0; j < 14; j++)
        {
            textureAddress = 0x4200 + (i << 4) + j;

            _GetValue(Parser, textureAddress, &sample);

            if ((sample != INVALID_ADDRESS) &&_SecureBuffer(Parser, sample))
            {
                /* There is a secure source, check target. */
                if (!_SecureBuffer(Parser, target) || !_SecureBuffer(Parser, targetEnd))
                {
                    printf("Offending DRAW %x => [%08x,%08x) \n", sample, target, target + height * stride);
                    Parser->allow = gcvFALSE;
                }
            }
        }
    }
}

void
_ConvertHwFormat(
    IN gctUINT32 HwFormat,
    OUT gctUINT32 *bytesPerPixel
    )
{
    switch(HwFormat)
    {
        case 0x05:
        case 0x06:
            *bytesPerPixel = 4;
            break;

        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x07:
        case 0x08:
            *bytesPerPixel = 2;
            break;

        case 0x10:
            *bytesPerPixel = 1;
            break;

        default:
            printf("HwFormat not supported!\n");
            break;
    }
}

gctBOOL
_IsYUVFormat(
    IN gctUINT32 HwFormat
    )
{
    /*YV12, NV12, NV16 calculate Yaddress, Uaddress, Vaddress or UVaddress separately*/
    return ((HwFormat == 0x0F) ||
            (HwFormat == 0x11) ||
            (HwFormat == 0x12)) ? 1 : 0;
}

void
_Get2DEndAddr(
    IN gctUINT32 baseAddr,
    IN gctUINT32 hwFormat,
    IN gctUINT32 stride,
    IN gctUINT32 bottom,
    IN gctUINT32 right,
    OUT gctUINT32 *endAddr
    )
{
    gctUINT32 bytesPerPixel;

    _ConvertHwFormat(hwFormat, &bytesPerPixel);
    *endAddr = baseAddr + ((bottom - 1) * stride + right * bytesPerPixel);
}

void
_GetYUVSrcAddr(
    IN gcTA_PARSER Parser,
    IN gctUINT32 Yaddr,
    IN gctUINT32 YaddrEnd,
    IN gctUINT32 srcFormat,
    IN gctUINT32 src_bottom,
    IN gctUINT32 src_right,
    OUT gctUINT32 sourceYUV[],
    OUT gctUINT32 sourceYUVEnd[]
    )
{
    gctUINT32 uStride, vStride;

    /* Y address */
    sourceYUV[0] = Yaddr;
    sourceYUVEnd[0] = YaddrEnd;

    _GetValue(Parser, 0x04A1, &sourceYUV[1]);
    _GetValue(Parser, 0x04A2, &uStride);
    uStride = (((((gctUINT32) (uStride)) >> (0 ? 17:0)) & ((gctUINT32) ((((1 ? 17:0) - (0 ? 17:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 17:0) - (0 ? 17:0) + 1)))))) );

    /* NV12 */
    if (srcFormat == 0x11)
    {
        /* UV address */
        sourceYUVEnd[1] = sourceYUV[1] + uStride * (src_bottom - 1) / 2 + src_right;
    }
    /* NV16 */
    else if (srcFormat == 0x12)
    {
        /* UV address */
        sourceYUVEnd[1] = sourceYUV[1] + uStride * (src_bottom - 1) + src_right * 2;
    }
    /* YV12 */
    else if (srcFormat == 0x0F)
    {
        _GetValue(Parser, 0x04A3, &sourceYUV[2]);
        _GetValue(Parser, 0x04A4, &vStride);
        vStride = (((((gctUINT32) (vStride)) >> (0 ? 17:0)) & ((gctUINT32) ((((1 ? 17:0) - (0 ? 17:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 17:0) - (0 ? 17:0) + 1)))))) );

        /* U address */
        sourceYUVEnd[1] = sourceYUV[1] + uStride * (src_bottom - 1) / 2 + src_right / 2;

        /* V address */
        sourceYUVEnd[2] = sourceYUV[2] + vStride * (src_bottom - 1) / 2 + src_right / 2;
    }
}

void
_Check2D(
    IN OUT gcTA_PARSER Parser
    )
{
    gctUINT32 source[SRC_MAX], sourceEnd[SRC_MAX];
    gctUINT32 target, targetEnd;
    gctUINT32 srcConfig, srcStride, srcOrigin, srcSize, srcFormat;
    gctUINT32 dstConfig, dstStride, bottomRight, dstFormat;
    gcsPOINT src_orign, src_size;
    gcsPOINT dst_orign, dst_size;
    gctUINT32 src_bottom, src_right, dst_bottom, dst_right;
    gctUINT32 srcType, vrConfigType;
    gctUINT32 i, j, maxSrc, multiSrc0;
    gctUINT32 srcLow, srcHigh, windowLow, windowHigh, imageLow, imageHigh;
    gctUINT32 sourceYUV[3], sourceYUVEnd[3], destYUV[3], destYUVEnd[3];
    gctBOOL isYUV, srcHasSecure = gcvFALSE;

    memset(source, 0, sizeof(source));
    memset(sourceEnd, 0, sizeof(sourceEnd));
    memset(sourceYUV, 0, sizeof(sourceYUV));
    memset(sourceYUVEnd, 0, sizeof(sourceYUVEnd));
    memset(destYUV, 0, sizeof(destYUV));
    memset(destYUVEnd, 0, sizeof(destYUVEnd));

    _GetValue(Parser, 0x048D, &dstConfig);
    srcType = (((((gctUINT32) (dstConfig)) >> (0 ? 15:12)) & ((gctUINT32) ((((1 ? 15:12) - (0 ? 15:12) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 15:12) - (0 ? 15:12) + 1)))))) );

    /* Bitblt. */
    if ((srcType == 0x2) ||
        (srcType == 0x4) )
    {
        /* SRC */
        _GetValue(Parser, 0x0480, &source[0]);
        _GetValue(Parser, 0x0483, &srcConfig);
        _GetValue(Parser, 0x0481, &srcStride);
        _GetValue(Parser, 0x0484, &srcOrigin);
        _GetValue(Parser, 0x0485, &srcSize);

        src_orign.x = (((((gctUINT32) (srcOrigin)) >> (0 ? 15:0)) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1)))))) );
        src_orign.y = (((((gctUINT32) (srcOrigin)) >> (0 ? 31:16)) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 31:16) - (0 ? 31:16) + 1)))))) );

        src_size.x  = (((((gctUINT32) (srcSize)) >> (0 ? 15:0)) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1)))))) );
        src_size.y  = (((((gctUINT32) (srcSize)) >> (0 ? 31:16)) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 31:16) - (0 ? 31:16) + 1)))))) );

        src_bottom  = src_orign.y + src_size.y;
        src_right   = src_orign.x + src_size.x;

        srcStride   = (((((gctUINT32) (srcStride)) >> (0 ? 17:0)) & ((gctUINT32) ((((1 ? 17:0) - (0 ? 17:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 17:0) - (0 ? 17:0) + 1)))))) );

        srcFormat   = (((((gctUINT32) (srcConfig)) >> (0 ? 3:0)) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 3:0) - (0 ? 3:0) + 1)))))) );

        _Get2DEndAddr(source[0], srcFormat, srcStride, src_bottom, src_right, &sourceEnd[0]);

        isYUV = _IsYUVFormat(srcFormat);
        if (isYUV)
        {
            _GetYUVSrcAddr(Parser, source[0], sourceEnd[0], srcFormat,
                            src_bottom, src_right, sourceYUV, sourceYUVEnd);

            /* if any of source is security */
            for (i = 0; i < 3; i++)
            {
                if ((sourceYUV[i] && _SecureBuffer(Parser, sourceYUV[i])) ||
                    (sourceYUVEnd[i] && _SecureBuffer(Parser, sourceYUVEnd[i])))
                {
                    srcHasSecure = gcvTRUE;
                    break;
                }
            }
        }
        else
        {
            /* if any of source is security */
            if (_SecureBuffer(Parser, source[0]) || _SecureBuffer(Parser, sourceEnd[0]))
            {
                srcHasSecure = gcvTRUE;
            }
        }

        /* DST (StartDE)*/
        _GetValue(Parser, 0x048A, &target);
        _GetValue(Parser, 0x048B, &dstStride);
        _GetValue(Parser, 0x048D, &dstConfig);

        dstStride  = (((((gctUINT32) (dstStride)) >> (0 ? 17:0)) & ((gctUINT32) ((((1 ? 17:0) - (0 ? 17:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 17:0) - (0 ? 17:0) + 1)))))) );

        dstFormat  = (((((gctUINT32) (dstConfig)) >> (0 ? 4:0)) & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 4:0) - (0 ? 4:0) + 1)))))) );

        if (srcHasSecure)
        {
            gctUINT32 * buffer = (gctUINT32 *)Parser->currentCmdBufferAddr;

            for (i = 0; i < Parser->cmdRectCount; i++)
            {
                dst_bottom = (((((gctUINT32) (*(buffer + RECT_ADDR_OFFSET + i * 2))) >> (0 ? 31:16)) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 31:16) - (0 ? 31:16) + 1)))))) );
                dst_right  = (((((gctUINT32) (*(buffer + RECT_ADDR_OFFSET + i * 2))) >> (0 ? 15:0)) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1)))))) );

                _Get2DEndAddr(target, dstFormat, dstStride, dst_bottom, dst_right, &targetEnd);

                /* None of target can be non-secure. */
                if (!_SecureBuffer(Parser, target) || !_SecureBuffer(Parser, targetEnd))
                {
                    Parser->allow = gcvFALSE;
                    break;
                }
            }
        }
    }

    /* Multi source blt. */
    if (srcType == 0x8)
    {
        /* SRC */
        _GetValue(Parser, 0x04C2, &maxSrc);
        maxSrc = (((((gctUINT32) (maxSrc)) >> (0 ? 2:0)) & ((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 2:0) - (0 ? 2:0) + 1)))))) );

        for (i = 0; i < maxSrc; i++)
        {
            /* First source */
            if (i == 0)
            {
                _GetValue(Parser, 0x0480, &source[0]);
                _GetValue(Parser, 0x0483, &srcConfig);
                _GetValue(Parser, 0x0481, &srcStride);
                _GetValue(Parser, 0x0484, &srcOrigin);
                _GetValue(Parser, 0x0485, &srcSize);

                src_orign.x = (((((gctUINT32) (srcOrigin)) >> (0 ? 15:0)) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1)))))) );
                src_orign.y = (((((gctUINT32) (srcOrigin)) >> (0 ? 31:16)) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 31:16) - (0 ? 31:16) + 1)))))) );

                src_size.x  = (((((gctUINT32) (srcSize)) >> (0 ? 15:0)) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1)))))) );
                src_size.y  = (((((gctUINT32) (srcSize)) >> (0 ? 31:16)) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 31:16) - (0 ? 31:16) + 1)))))) );

                src_bottom  = src_orign.y + src_size.y;
                src_right   = src_orign.x + src_size.x;

                srcStride = (((((gctUINT32) (srcStride)) >> (0 ? 17:0)) & ((gctUINT32) ((((1 ? 17:0) - (0 ? 17:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 17:0) - (0 ? 17:0) + 1)))))) );

                srcFormat = (((((gctUINT32) (srcConfig)) >> (0 ? 3:0)) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 3:0) - (0 ? 3:0) + 1)))))) );

                _Get2DEndAddr(source[0], srcFormat, srcStride, src_bottom, src_right, &sourceEnd[0]);

                isYUV = _IsYUVFormat(srcFormat);
                if (isYUV)
                {
                    _GetYUVSrcAddr(Parser, source[0], sourceEnd[0], srcFormat,
                                    src_bottom, src_right, sourceYUV, sourceYUVEnd);

                    /* if any of source is security */
                    for (i = 0; i < 3; i++)
                    {
                        if ((sourceYUV[i] && _SecureBuffer(Parser, sourceYUV[i])) ||
                            (sourceYUVEnd[i] && _SecureBuffer(Parser, sourceYUVEnd[i])))
                        {
                            srcHasSecure = gcvTRUE;
                            break;
                        }
                    }
                }
                else
                {
                    /* if any of source is security */
                    if (_SecureBuffer(Parser, source[0]) || _SecureBuffer(Parser, sourceEnd[0]))
                    {
                        srcHasSecure = gcvTRUE;
                    }
                }

                if (srcHasSecure)
                {
                    break;
                }

                continue;
            }

            /* Other sources */
            _GetValue(Parser, 0x4A80 + i, &source[i]);
            _GetValue(Parser, 0x4AA0 + i, &srcOrigin);
            _GetValue(Parser, 0x4A98 + i, &srcConfig);
            _GetValue(Parser, 0x4A88 + i, &srcStride);
            _GetValue(Parser, 0x4AA8 + i, &srcSize);

            src_orign.x = (((((gctUINT32) (srcOrigin)) >> (0 ? 15:0)) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1)))))) );
            src_orign.y = (((((gctUINT32) (srcOrigin)) >> (0 ? 31:16)) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 31:16) - (0 ? 31:16) + 1)))))) );

            src_size.x  = (((((gctUINT32) (srcSize)) >> (0 ? 15:0)) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1)))))) );
            src_size.y  = (((((gctUINT32) (srcSize)) >> (0 ? 31:16)) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 31:16) - (0 ? 31:16) + 1)))))) );

            src_bottom  = src_orign.y + src_size.y;
            src_right   = src_orign.x + src_size.x;

            srcStride   = (((((gctUINT32) (srcStride)) >> (0 ? 17:0)) & ((gctUINT32) ((((1 ? 17:0) - (0 ? 17:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 17:0) - (0 ? 17:0) + 1)))))) );

            /* Bpp */
            srcFormat   = (((((gctUINT32) (srcConfig)) >> (0 ? 28:24)) & ((gctUINT32) ((((1 ? 28:24) - (0 ? 28:24) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 28:24) - (0 ? 28:24) + 1)))))) );

            _Get2DEndAddr(source[i], srcFormat, srcStride, src_bottom, src_right, &sourceEnd[i]);

            isYUV = _IsYUVFormat(srcFormat);
            if (isYUV)
            {
                _GetYUVSrcAddr(Parser, source[i], sourceEnd[i], srcFormat,
                                src_bottom, src_right, sourceYUV, sourceYUVEnd);

                /* if any of source is security */
                for (j = 0; j < 3; j++)
                {
                    if ((sourceYUV[j] && _SecureBuffer(Parser, sourceYUV[j])) ||
                        (sourceYUVEnd[j] && _SecureBuffer(Parser, sourceYUVEnd[j])))
                    {
                        srcHasSecure = gcvTRUE;
                        break;
                    }
                }
            }
            else
            {
                /* if any of source is security */
                if (_SecureBuffer(Parser, source[i]) || _SecureBuffer(Parser, sourceEnd[i]))
                {
                    srcHasSecure = gcvTRUE;
                }
            }

            if (srcHasSecure)
                break;
        }

        /* DST */
        _GetValue(Parser, 0x048A, &target);
        _GetValue(Parser, 0x048B, &dstStride);
        _GetValue(Parser, 0x048D, &dstConfig);
        _GetValue(Parser, 0x4B58, &bottomRight);

        dst_right  = (((((gctUINT32) (bottomRight)) >> (0 ? 14:0)) & ((gctUINT32) ((((1 ? 14:0) - (0 ? 14:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 14:0) - (0 ? 14:0) + 1)))))) );
        dst_bottom = (((((gctUINT32) (bottomRight)) >> (0 ? 30:16)) & ((gctUINT32) ((((1 ? 30:16) - (0 ? 30:16) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 30:16) - (0 ? 30:16) + 1)))))) );

        dstStride  = (((((gctUINT32) (dstStride)) >> (0 ? 17:0)) & ((gctUINT32) ((((1 ? 17:0) - (0 ? 17:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 17:0) - (0 ? 17:0) + 1)))))) );

        dstFormat  = (((((gctUINT32) (dstConfig)) >> (0 ? 4:0)) & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 4:0) - (0 ? 4:0) + 1)))))) );

        _Get2DEndAddr(target, dstFormat, dstStride, dst_bottom, dst_right, &targetEnd);

        /* If any part of source is secure memory. */
        if (srcHasSecure)
        {
            isYUV = _IsYUVFormat(dstFormat);
            if (isYUV)
            {
                _GetYUVSrcAddr(Parser, target, targetEnd, dstFormat,
                                dst_bottom, dst_right, destYUV, destYUVEnd);

                for (j = 0; j < 2; j++)
                {
                    /* None of target can be non-secure. */
                    if (!_SecureBuffer(Parser, destYUV[j]) || !_SecureBuffer(Parser, destYUVEnd[j]))
                    {
                        Parser->allow = gcvFALSE;
                    }
                }

                if (Parser->allow)
                {
                    if (destYUV[2] && destYUVEnd[2])
                    {
                        if (!_SecureBuffer(Parser, destYUV[2]) || !_SecureBuffer(Parser, destYUVEnd[2]))
                        {
                            Parser->allow = gcvFALSE;
                        }
                    }
                }
            }
            /* None of target can be non-secure. */
            else if (!_SecureBuffer(Parser, target) || !_SecureBuffer(Parser, targetEnd))
            {
                Parser->allow = gcvFALSE;
            }
        }
    }

    /* FilterBlit */
    if (Parser->isFilterBlit)
    {
        _GetValue(Parser, 0x04A5, &vrConfigType);
        vrConfigType = (((((gctUINT32) (vrConfigType)) >> (0 ? 1:0)) & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 1:0) - (0 ? 1:0) + 1)))))) );

        if ((vrConfigType == 0x0) ||
            (vrConfigType == 0x1) ||
            (vrConfigType == 0x2))
        {
            /* DST */
            _GetValue(Parser, 0x048A, &target);
            _GetValue(Parser, 0x048B, &dstStride);
            _GetValue(Parser, 0x048D, &dstConfig);
            _GetValue(Parser, 0x04AA, &windowLow);
            _GetValue(Parser, 0x04AB, &windowHigh);

            dst_orign.y = (((((gctUINT32) (windowLow)) >> (0 ? 31:16)) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 31:16) - (0 ? 31:16) + 1)))))) );
            dst_orign.x = (((((gctUINT32) (windowLow)) >> (0 ? 15:0)) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1)))))) );
            dst_bottom  = (((((gctUINT32) (windowHigh)) >> (0 ? 31:16)) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 31:16) - (0 ? 31:16) + 1)))))) );
            dst_right   = (((((gctUINT32) (windowHigh)) >> (0 ? 15:0)) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1)))))) );

            dstStride = (((((gctUINT32) (dstStride)) >> (0 ? 17:0)) & ((gctUINT32) ((((1 ? 17:0) - (0 ? 17:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 17:0) - (0 ? 17:0) + 1)))))) );

            dstFormat = (((((gctUINT32) (dstConfig)) >> (0 ? 4:0)) & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 4:0) - (0 ? 4:0) + 1)))))) );

            _Get2DEndAddr(target, dstFormat, dstStride, dst_bottom, dst_right, &targetEnd);

            /* SRC */
            _GetValue(Parser, 0x0480, &source[0]);
            _GetValue(Parser, 0x0483, &srcConfig);
            _GetValue(Parser, 0x0481, &srcStride);
            _GetValue(Parser, 0x04A6, &imageLow);
            _GetValue(Parser, 0x04A7, &imageHigh);

            src_orign.y = (((((gctUINT32) (imageLow)) >> (0 ? 31:16)) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 31:16) - (0 ? 31:16) + 1)))))) );
            src_orign.x = (((((gctUINT32) (imageLow)) >> (0 ? 15:0)) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1)))))) );
            src_bottom  = (((((gctUINT32) (imageHigh)) >> (0 ? 31:16)) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 31:16) - (0 ? 31:16) + 1)))))) );
            src_right   = (((((gctUINT32) (imageHigh)) >> (0 ? 15:0)) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1)))))) );

            srcStride = (((((gctUINT32) (srcStride)) >> (0 ? 17:0)) & ((gctUINT32) ((((1 ? 17:0) - (0 ? 17:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 17:0) - (0 ? 17:0) + 1)))))) );

            srcFormat = (((((gctUINT32) (srcConfig)) >> (0 ? 3:0)) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 3:0) - (0 ? 3:0) + 1)))))) );

            _Get2DEndAddr(source[0], srcFormat, srcStride, src_bottom, src_right, &sourceEnd[0]);

            isYUV = _IsYUVFormat(srcFormat);
            if (isYUV)
            {
                _GetYUVSrcAddr(Parser, source[0], sourceEnd[0], srcFormat,
                                src_bottom, src_right, sourceYUV, sourceYUVEnd);
                /* if any of source is security */
                for (i = 0; i < 3; i++)
                {
                    if ((sourceYUV[i] && _SecureBuffer(Parser, sourceYUV[i])) ||
                        (sourceYUVEnd[i] && _SecureBuffer(Parser, sourceYUVEnd[i])))
                    {
                        srcHasSecure = gcvTRUE;
                        break;
                    }
                }
            }
            else
            {
                /* if any of source is security */
                if (_SecureBuffer(Parser, source[0]) || _SecureBuffer(Parser, sourceEnd[0]))
                {
                    srcHasSecure = gcvTRUE;
                }
            }

            if (srcHasSecure)
            {
                /* None of target can be non-secure. */
                if (!_SecureBuffer(Parser, target) || !_SecureBuffer(Parser, targetEnd))
                {
                    Parser->allow = gcvFALSE;
                }
            }
        }
    }
}

void
_FillState(
    IN OUT gcTA_PARSER Parser
)
{
    gctINT i;
    gctUINT32_PTR data = (gctUINT32_PTR)Parser->currentCmdBufferAddr;
    gctUINT32 cmdAddr = Parser->cmdAddr;
    gctUINT32 index;
    gcsSTATE_MAP_PTR map = Parser->map;
    gctUINT32_PTR buffer = Parser->buffer;

    for (i = 0; i < Parser->cmdSize; i++)
    {
        if (cmdAddr == 0x0580)
        {
            _CheckResolve(Parser);
            continue;
        }

        if (cmdAddr >= Parser->stateCount)
        {
            continue;
        }

        index = map[cmdAddr].index;

        if (index)
        {
            buffer[index] = *data;
        }

        /* Advance to next state. */
        cmdAddr++;
        data++;
    }
}

static void
_GetCommand(
    IN OUT gcTA_PARSER Parser
    )
{
    gctUINT32 low, high;
    gctUINT32 * buffer = (gctUINT32 *)Parser->currentCmdBufferAddr;
    gctUINT32 skipCount;
    gctUINT32 vrConfig;

    gctUINT16 cmdRectCount;
    gctUINT16 cmdDataCount;

    Parser->hi = buffer[0];
    Parser->lo = buffer[1];

    Parser->cmdOpcode = (((((gctUINT32) (Parser->hi)) >> (0 ? 31:27)) & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1)))))) );
    Parser->cmdRectCount = 1;

    switch (Parser->cmdOpcode)
    {
    case 0x01:
        // Extract count.
        Parser->cmdSize = (((((gctUINT32) (Parser->hi)) >> (0 ? 25:16)) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 25:16) - (0 ? 25:16) + 1)))))) );
        if (Parser->cmdSize == 0)
        {
            // 0 means 1024.
            Parser->cmdSize = 1024;
        }
        Parser->skip = (Parser->cmdSize & 0x1) ? 0 : 1;

         // Extract address.
        Parser->cmdAddr = (((((gctUINT32) (Parser->hi)) >> (0 ? 15:0)) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1)))))) );

        if (Parser->cmdAddr == 0x04A5)
        {
            _GetValue(Parser, 0x04A5, &vrConfig);
            if (!(((((gctUINT32) (vrConfig)) >> (0 ? 3:3)) & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 3:3) - (0 ? 3:3) + 1)))))) ))
            {
                // FilterBlit
                Parser->isFilterBlit = gcvTRUE;
            }
        }


        Parser->currentCmdBufferAddr = Parser->currentCmdBufferAddr + 4;
        Parser->skipCount = Parser->cmdSize + Parser->skip;
        break;

     case 0x05:
        Parser->cmdSize   = 4;
        Parser->skipCount = gcmALIGN(Parser->cmdSize, 2);
        break;

    case 0x06:
        Parser->cmdSize   = 5;
        Parser->skipCount = gcmALIGN(Parser->cmdSize, 2);
        break;

    case 0x0C:
        Parser->cmdSize   = 3;
        Parser->skipCount = gcmALIGN(Parser->cmdSize, 2);
        break;

    case 0x09:
        Parser->cmdSize   = 2;
        Parser->cmdAddr   = 0x0F16;
        Parser->skipCount = gcmALIGN(Parser->cmdSize, 2);
        break;

     case 0x04:
        Parser->cmdSize = 1;
        Parser->cmdAddr = 0x0F06;

        cmdRectCount = (((((gctUINT32) (Parser->hi)) >> (0 ? 15:8)) & ((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 15:8) - (0 ? 15:8) + 1)))))) );
        cmdDataCount = (((((gctUINT32) (Parser->hi)) >> (0 ? 26:16)) & ((gctUINT32) ((((1 ? 26:16) - (0 ? 26:16) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 26:16) - (0 ? 26:16) + 1)))))) );

        gcmTA_PRINT("FE: @ 0x%08x StartDE(%u, %u)\n",
                    currentCmdBufferAddr,
                    cmdRectCount,
                    cmdDataCount);

        Parser->skipCount = gcmALIGN(Parser->cmdSize, 2)
                          + cmdRectCount * 2
                          + gcmALIGN(cmdDataCount, 2);

        Parser->cmdRectCount = cmdRectCount;
        break;

    case 0x03:
        gcmTA_PRINT("FE: @ 0x%08x Nop\n",
                    currentCmdBufferAddr);

        Parser->currentCmdBufferAddr = Parser->currentCmdBufferAddr + 8;
        Parser->skipCount = 0;
        break;

    case 0x02:
        Parser->currentCmdBufferAddr = Parser->currentCmdBufferAddr + 8;
        Parser->skipCount = 0;
        break;

    default:
        /* Unknown command is a risk. */
        Parser->allow = gcvFALSE;
        break;
    }
}

void
_ParseCommand(
    IN OUT gcTA_PARSER Parser
    )
{
    switch(Parser->cmdOpcode)
    {
    case 0x01:
        _FillState(Parser);
        if (Parser->isFilterBlit)
        {
            _Check2D(Parser);
        }
        break;
    case 0x05:
    case 0x06:
    case 0x0C:
        _CheckDraw(Parser);
        break;
    case 0x04:
        _Check2D(Parser);
        break;
    default:
        break;
    }

    /* Advance to next command. */
    Parser->currentCmdBufferAddr = Parser->currentCmdBufferAddr
                                 + (Parser->skipCount << 2);
}

gceSTATUS
_InitializeContextBuffer(
    IN gcTA_PARSER Context
)
{
    gctUINT32 index;
    gcTA_HARDWARE hardware = Context->ta->hardware;
    gctUINT32 pixelPipes = hardware->pixelPipes;

    gctUINT32 resolvePipes = hardware->resolvePipes;
    gctINT i;
    gctBOOL halti1;

    halti1 = (((((gctUINT32) (hardware->chipMinorFeatures2)) >> (0 ? 11:11)) & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 11:11) - (0 ? 11:11) + 1)))))) );

    index = 0;

    /* 2D states. */
    index += _State(Context, index, 0x01200 >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);
    index += _State(Context, index, 0x0120C >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);
    index += _State(Context, index, 0x01204 >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);
    index += _State(Context, index, 0x01210 >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);
    index += _State(Context, index, 0x01214 >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);

    index += _State(Context, index, 0x01228 >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);
    index += _State(Context, index, 0x01234 >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);
    index += _State(Context, index, 0x0122C >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);

    index += _State(Context, index, 0x01294 >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);
    index += _State(Context, index, 0x012A8 >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);
    index += _State(Context, index, 0x012AC >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);
    index += _State(Context, index, 0x01298 >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);
    index += _State(Context, index, 0x0129C >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);

    index += _State(Context, index, 0x12A00 >> 2, 0x00000000, 8, gcvFALSE, gcvFALSE);
    index += _State(Context, index, 0x12A60 >> 2, 0x00000000, 8, gcvFALSE, gcvFALSE);
    index += _State(Context, index, 0x12A20 >> 2, 0x00000000, 8, gcvFALSE, gcvFALSE);
    index += _State(Context, index, 0x12A80 >> 2, 0x00000000, 8, gcvFALSE, gcvFALSE);
    index += _State(Context, index, 0x12AA0 >> 2, 0x00000000, 8, gcvFALSE, gcvFALSE);

    index += _State(Context, index, 0x01308 >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);

    index += _State(Context, index, 0x12D60 >> 2, 0x00000000, 8, gcvFALSE, gcvFALSE);

    index += _State(Context, index, 0x01288 >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);
    index += _State(Context, index, 0x01290 >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);

    /* Resolve states. */
    index += _State(Context, index, 0x01608 >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);
    index += _State(Context, index, 0x01610 >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);

    if (resolvePipes > 1)
    {
        index += _State(Context, index, (0x016C0 >> 2) + (0 << 3), 0x00000000, resolvePipes, gcvFALSE, gcvTRUE);
        index += _State(Context, index, (0x016E0 >> 2) + (0 << 3), 0x00000000, resolvePipes, gcvFALSE, gcvTRUE);
    }

    index += _State(Context, index, 0x01620 >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);
    index += _State(Context, index, 0x0160C >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);
    index += _State(Context, index, 0x01614 >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);
    index += _State(Context, index, 0x0163C >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);
    index += _State(Context, index, 0x01604 >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);

    /* Draw states. */
    index += _State(Context, index, 0x0165C >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);
    index += _State(Context, index, 0x00A00 >> 2, 0x00000000, 1, gcvTRUE, gcvFALSE);
    index += _State(Context, index, 0x00A04 >> 2, 0x00000000, 1, gcvTRUE, gcvFALSE);
    index += _State(Context, index, 0x00A0C >> 2, 0x00000000, 1, gcvTRUE, gcvFALSE);
    index += _State(Context, index, 0x00A10 >> 2, 0x00000000, 1, gcvTRUE, gcvFALSE);
    index += _State(Context, index, 0x01434 >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);

     if (pixelPipes == 1)
    {
        index += _State(Context, index, 0x01460 >> 2, 0x00000000, 8, gcvFALSE, gcvFALSE);

        index += _State(Context, index, 0x01430 >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);
        index += _State(Context, index, 0x01410 >> 2, 0x00000000, 1, gcvFALSE, gcvFALSE);
    }
    else
    {
        index += _State(Context, index, (0x01460 >> 2) + (0 << 3), 0x00000000, pixelPipes, gcvFALSE, gcvTRUE);
        index += _State(Context, index, (0x01480 >> 2) + (0 << 3), 0x00000000, pixelPipes, gcvFALSE, gcvTRUE);

        for (i = 0; i < 3; i++)
        {
            index += _State(Context, index, (0x01500 >> 2) + (i << 3), 0x00000000, pixelPipes, gcvFALSE, gcvTRUE);
        }
    }

    if (halti1)
    {
        gctUINT texBlockCount;

        texBlockCount = ((512) >> (4));

        for (i = 0; i < texBlockCount; i += 1)
        {
            index += _State(Context, index, (0x10800 >> 2) + (i << 4), 0x00000000, 14, gcvFALSE, gcvTRUE);
        }
    }
    else
    {
        index += _State(Context, index, (0x02400 >> 2) + (0 << 4), 0x00000000, 12, gcvFALSE, gcvTRUE);
        index += _State(Context, index, (0x02440 >> 2) + (0 << 4), 0x00000000, 12, gcvFALSE, gcvTRUE);
        index += _State(Context, index, (0x02480 >> 2) + (0 << 4), 0x00000000, 12, gcvFALSE, gcvTRUE);
        index += _State(Context, index, (0x024C0 >> 2) + (0 << 4), 0x00000000, 12, gcvFALSE, gcvTRUE);
        index += _State(Context, index, (0x02500 >> 2) + (0 << 4), 0x00000000, 12, gcvFALSE, gcvTRUE);
        index += _State(Context, index, (0x02540 >> 2) + (0 << 4), 0x00000000, 12, gcvFALSE, gcvTRUE);
        index += _State(Context, index, (0x02580 >> 2) + (0 << 4), 0x00000000, 12, gcvFALSE, gcvTRUE);
        index += _State(Context, index, (0x025C0 >> 2) + (0 << 4), 0x00000000, 12, gcvFALSE, gcvTRUE);
        index += _State(Context, index, (0x02600 >> 2) + (0 << 4), 0x00000000, 12, gcvFALSE, gcvTRUE);
        index += _State(Context, index, (0x02640 >> 2) + (0 << 4), 0x00000000, 12, gcvFALSE, gcvTRUE);
        index += _State(Context, index, (0x02680 >> 2) + (0 << 4), 0x00000000, 12, gcvFALSE, gcvTRUE);
        index += _State(Context, index, (0x026C0 >> 2) + (0 << 4), 0x00000000, 12, gcvFALSE, gcvTRUE);
        index += _State(Context, index, (0x02700 >> 2) + (0 << 4), 0x00000000, 12, gcvFALSE, gcvTRUE);
        index += _State(Context, index, (0x02740 >> 2) + (0 << 4), 0x00000000, 12, gcvFALSE, gcvTRUE);
    }



    Context->totalSize = index * gcmSIZEOF(gctUINT32);

    return gcvSTATUS_OK;
}

gceSTATUS
gctaPARSER_Construct(
    IN gcTA TA,
    OUT gcTA_PARSER *Parser
    )
{
    gceSTATUS status;
    gctPOINTER pointer;
    gcTA_PARSER parser;

    gcmkONERROR(gctaOS_Allocate(gcmSIZEOF(gcsTA_PARSER), &pointer));

    parser = (gcTA_PARSER)pointer;

    gctaOS_ZeroMemory(pointer, gcmSIZEOF(gcsTA_PARSER));

    parser->ta = TA;

    gcmkONERROR(_InitializeContextBuffer(parser));

    gcmkONERROR(gctaOS_Allocate(
        parser->totalSize,
        (gctPOINTER *)&parser->buffer
        ));

    gcmkONERROR(gctaOS_Allocate(
        parser->stateCount * gcmSIZEOF(gcsSTATE_MAP),
        (gctPOINTER *)&parser->map));

    gcmkONERROR(gctaOS_ZeroMemory(
        (gctPOINTER)parser->map,
        parser->stateCount * gcmSIZEOF(gcsSTATE_MAP)
        ));

    gcmkONERROR(_InitializeContextBuffer(parser));

    *Parser = parser;

    return gcvSTATUS_OK;

OnError:
    return status;
}

/*
* Hardare layer.
*/

gceSTATUS
gctaHARDWARE_Link(
    IN gcTA TA,
    IN gctPOINTER Logical,
    IN gctPOINTER FetchAddress,
    IN gctSIZE_T FetchSize,
    IN OUT gctSIZE_T * Bytes
    )
{
    gceSTATUS status;
    gctSIZE_T bytes;
    gctUINT32 address;
    gctUINT32 link;
    gctUINT32_PTR logical = (gctUINT32_PTR) Logical;

    gcmkHEADER_ARG("Logical=0x%x FetchAddress=0x%x FetchSize=%lu "
                   "*Bytes=%lu",
                   Logical, FetchAddress, FetchSize,
                   gcmOPT_VALUE(Bytes));

    /* Verify the arguments. */

    gcmkVERIFY_ARGUMENT((Logical == gcvNULL) || (Bytes != gcvNULL));

    if (Logical != gcvNULL)
    {
        if (*Bytes < 8)
        {
        printf("XQ %s(%d) \n", __FUNCTION__, __LINE__);
            /* Command queue too small. */
            gcmkONERROR(gcvSTATUS_BUFFER_TOO_SMALL);
        }

        /* Convert logical address to hardware address. */
        gcmkONERROR(
            gctaOS_GetPhysicalAddress(TA->os, FetchAddress, &address));

        if (address == 0)
        {
            printf("XQ %s(%d) \n", __FUNCTION__, __LINE__);
        }

        *(logical + 1) = address;

        gctaOS_CacheClean(logical + 1, 4);

        /* Compute number of 64-byte aligned bytes to fetch. */
        bytes = gcmALIGN(address + FetchSize, 64) - address;

        /* Append LINK(bytes / 8), FetchAddress. */
        link = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x08 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
             | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:0) - (0 ?
 15:0) + 1))))))) << (0 ?
 15:0))) | (((gctUINT32) ((gctUINT32) (bytes >> 3) & ((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

        *logical = link;

        gctaOS_CacheClean(logical, 4);
    }

    if (Bytes != gcvNULL)
    {
        /* Return number of bytes required by the LINK command. */
        *Bytes = 8;
    }

    /* Success. */
    gcmkFOOTER_ARG("*Bytes=%lu", gcmOPT_VALUE(Bytes));
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

gceSTATUS
gctaHARDWARE_WaitLink(
    IN gcTA TA,
    IN gctPOINTER Logical,
    IN gctUINT32 Offset,
    IN OUT gctSIZE_T * Bytes,
    OUT gctUINT32 * WaitOffset
    )
{
    static const gctUINT waitCount = 200;

    gceSTATUS status;
    gctUINT32 address;
    gctUINT32_PTR logical;
    gctSIZE_T bytes;

    gcmkHEADER_ARG("Logical=0x%x Offset=0x%08x *Bytes=%lu",
                    Logical, Offset, gcmOPT_VALUE(Bytes));

    /* Verify the arguments. */
    gcmkVERIFY_ARGUMENT((Logical != gcvNULL) || (Bytes != gcvNULL));

    /* Compute number of bytes required. */
    bytes = gcmALIGN(Offset + 16, 8) - Offset;

    /* Cast the input pointer. */
    logical = (gctUINT32_PTR) Logical;

    if (logical != gcvNULL)
    {
        /* Not enough space? */
        if (*Bytes < bytes)
        {
            /* Command queue too small. */
            gcmkONERROR(gcvSTATUS_BUFFER_TOO_SMALL);
        }

        /* Convert logical into hardware specific address. */
        gcmkONERROR(gctaOS_GetPhysicalAddress(TA->os, logical, &address));

        /* Append WAIT(count). */
        logical[0]
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:0) - (0 ?
 15:0) + 1))))))) << (0 ?
 15:0))) | (((gctUINT32) ((gctUINT32) (waitCount) & ((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

        /* Append LINK(2, address). */
        logical[2]
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x08 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:0) - (0 ?
 15:0) + 1))))))) << (0 ?
 15:0))) | (((gctUINT32) ((gctUINT32) (bytes >> 3) & ((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

        logical[3] = address;

        gcmkTRACE_ZONE(
            gcvLEVEL_INFO, gcvZONE_HARDWARE,
            "0x%08x: WAIT %u", address, waitCount
            );

        gcmkTRACE_ZONE(
            gcvLEVEL_INFO, gcvZONE_HARDWARE,
            "0x%08x: LINK 0x%08x, #%lu",
            address + 8, address, bytes
            );

        if (WaitOffset != gcvNULL)
        {
            /* Return the offset pointer to WAIT command. */
            *WaitOffset = 0;
        }

    }

    if (Bytes != gcvNULL)
    {
        /* Return number of bytes required by the WAIT/LINK command
        ** sequence. */
        *Bytes = bytes;
    }

    /* Success. */
    gcmkFOOTER_ARG("*Bytes=%lu *WaitOffset=0x%x",
                   gcmOPT_VALUE(Bytes), gcmOPT_VALUE(WaitOffset));
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

gceSTATUS
gctaHARDWARE_VerifyCommand(
    IN gcTA TA,
    IN gctUINT32 * CommandBuffer,
    IN gctUINT32 Length,
    OUT gctBOOL *Allow
    )
{
    gcTA_PARSER parser = TA->parser;
    gctUINT8_PTR end = (gctUINT8_PTR)CommandBuffer + Length;

    /* Initialize parser. */
    parser->currentCmdBufferAddr = (gctUINT8_PTR)CommandBuffer;
    parser->allow = gcvTRUE;
    parser->isFilterBlit = gcvFALSE;
    parser->skip = 0;

    /* Go through command buffer until reaching the end
    ** or meeting illegal behavior. */
    do
    {
        _GetCommand(parser);

        _ParseCommand(parser);
    }
    while ((parser->currentCmdBufferAddr < end) && (parser->allow == gcvTRUE));

    *Allow = parser->allow;

    return gcvSTATUS_OK;
}

gceSTATUS
gctaHARDWARE_Execute(
    IN gcTA TA,
    IN gctPOINTER Logical,
    IN gctSIZE_T Bytes
    )
{
    gceSTATUS status;
    gctUINT32 address = 0, control;

    gcmkHEADER_ARG("Logical=0x%x Bytes=%lu",
                   Logical, Bytes);

    /* Verify the arguments. */
    gcmkVERIFY_ARGUMENT(Logical != gcvNULL);


    /* Convert logical into hardware specific address. */
    gcmkONERROR(
        gctaOS_GetPhysicalAddress(TA->os, Logical, &address));


    /* Enable all events. */
    gcmkONERROR(
        gctaOS_WriteRegister(TA->os, 0x00014, ~0U));

    /* Write address register. */
    gcmkONERROR(
        gctaOS_WriteRegister(TA->os, 0x00654, address));

    /* Build control register. */
    control = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 16:16) - (0 ?
 16:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 16:16) - (0 ?
 16:16) + 1))))))) << (0 ?
 16:16))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ?
 16:16) - (0 ?
 16:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:0) - (0 ?
 15:0) + 1))))))) << (0 ?
 15:0))) | (((gctUINT32) ((gctUINT32) ((Bytes + 7) >> 3) & ((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));


    /* Write control register. */
    gcmkONERROR(
        gctaOS_WriteRegister(TA->os, 0x00658, control));

    gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_HARDWARE,
                  "Started command buffer @ 0x%08x",
                  address);

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

gceSTATUS
gctaHARDWARE_Fence(
    gcTA TA,
    gctPOINTER Logical,
    gctPOINTER FenceAddress,
    gctUINT32 * FenceBytes
    )
{
    gctUINT32_PTR logical = (gctUINT32_PTR) Logical;
    gctUINT32 fencePhysical;
    gctUINT32 bytes;

    gctBOOL only2D;

    only2D = gctaHARDWARE_IsFeatureAvailable(TA->hardware, gcvFEATURE_PIPE_2D)
         && !gctaHARDWARE_IsFeatureAvailable(TA->hardware, gcvFEATURE_PIPE_3D);

    bytes = only2D
          ? 20 * gcmSIZEOF(gctUINT32)
          : 4  * gcmSIZEOF(gctUINT32);

    if (logical)
    {
        if (*FenceBytes < bytes)
        {
            return gcvSTATUS_BUFFER_TOO_SMALL;
        }

        gctaOS_GetPhysicalAddress(TA->os, FenceAddress, &fencePhysical);

        if (only2D)
        {
            *logical++
                = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 25:16) - (0 ?
 25:16) + 1))))))) << (0 ?
 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:0) - (0 ?
 15:0) + 1))))))) << (0 ?
 15:0))) | (((gctUINT32) ((gctUINT32) (0x048A) & ((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

            *logical++
                = fencePhysical;

            *logical++
                = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 25:16) - (0 ?
 25:16) + 1))))))) << (0 ?
 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:0) - (0 ?
 15:0) + 1))))))) << (0 ?
 15:0))) | (((gctUINT32) ((gctUINT32) (0x04B0) & ((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

            *logical++
                = 0;

            *logical++
                = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 25:16) - (0 ?
 25:16) + 1))))))) << (0 ?
 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:0) - (0 ?
 15:0) + 1))))))) << (0 ?
 15:0))) | (((gctUINT32) ((gctUINT32) (0x048D) & ((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

            *logical++
                = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 29:29) - (0 ?
 29:29) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 29:29) - (0 ?
 29:29) + 1))))))) << (0 ?
 29:29))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ?
 29:29) - (0 ?
 29:29) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:12) - (0 ?
 15:12) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:12) - (0 ?
 15:12) + 1))))))) << (0 ?
 15:12))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ?
 15:12) - (0 ?
 15:12) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:12) - (0 ? 15:12) + 1))))))) << (0 ? 15:12)));

            *logical++
                = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 25:16) - (0 ?
 25:16) + 1))))))) << (0 ?
 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:0) - (0 ?
 15:0) + 1))))))) << (0 ?
 15:0))) | (((gctUINT32) ((gctUINT32) (0x0497) & ((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

            *logical++
                = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 21:20) - (0 ?
 21:20) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 21:20) - (0 ?
 21:20) + 1))))))) << (0 ?
 21:20))) | (((gctUINT32) (0x3 & ((gctUINT32) ((((1 ?
 21:20) - (0 ?
 21:20) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:8) - (0 ?
 15:8) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:8) - (0 ?
 15:8) + 1))))))) << (0 ?
 15:8))) | (((gctUINT32) ((gctUINT32) (0xCC) & ((gctUINT32) ((((1 ?
 15:8) - (0 ?
 15:8) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 7:0) - (0 ?
 7:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 7:0) - (0 ?
 7:0) + 1))))))) << (0 ?
 7:0))) | (((gctUINT32) ((gctUINT32) (0xCC) & ((gctUINT32) ((((1 ?
 7:0) - (0 ?
 7:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 7:0) - (0 ? 7:0) + 1))))))) << (0 ? 7:0)));

            *logical++
                = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 25:16) - (0 ?
 25:16) + 1))))))) << (0 ?
 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:0) - (0 ?
 15:0) + 1))))))) << (0 ?
 15:0))) | (((gctUINT32) ((gctUINT32) (0x049F) & ((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

            *logical++
                = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 0:0) - (0 ?
 0:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 0:0) - (0 ?
 0:0) + 1))))))) << (0 ?
 0:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ?
 0:0) - (0 ?
 0:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)));

            *logical++
                = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 25:16) - (0 ?
 25:16) + 1))))))) << (0 ?
 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:0) - (0 ?
 15:0) + 1))))))) << (0 ?
 15:0))) | (((gctUINT32) ((gctUINT32) (0x4B40) & ((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

            *logical++
                = 0;

            *logical++
                = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 25:16) - (0 ?
 25:16) + 1))))))) << (0 ?
 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:0) - (0 ?
 15:0) + 1))))))) << (0 ?
 15:0))) | (((gctUINT32) ((gctUINT32) (0x0499) & ((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

            *logical++
                = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 14:0) - (0 ?
 14:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 14:0) - (0 ?
 14:0) + 1))))))) << (0 ?
 14:0))) | (((gctUINT32) ((gctUINT32) (16) & ((gctUINT32) ((((1 ?
 14:0) - (0 ?
 14:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 14:0) - (0 ? 14:0) + 1))))))) << (0 ? 14:0)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 30:16) - (0 ?
 30:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 30:16) - (0 ?
 30:16) + 1))))))) << (0 ?
 30:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ?
 30:16) - (0 ?
 30:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 30:16) - (0 ? 30:16) + 1))))))) << (0 ? 30:16)));

            *logical++
                = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x04 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:8) - (0 ?
 15:8) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:8) - (0 ?
 15:8) + 1))))))) << (0 ?
 15:8))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ?
 15:8) - (0 ?
 15:8) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8)));

            *logical++
                = 0;

            *logical++
                = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:0) - (0 ?
 15:0) + 1))))))) << (0 ?
 15:0))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:16) - (0 ?
 31:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:16) - (0 ?
 31:16) + 1))))))) << (0 ?
 31:16))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ?
 31:16) - (0 ?
 31:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)));

            *logical++
                = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:0) - (0 ?
 15:0) + 1))))))) << (0 ?
 15:0))) | (((gctUINT32) ((gctUINT32) (16) & ((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:16) - (0 ?
 31:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:16) - (0 ?
 31:16) + 1))))))) << (0 ?
 31:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ?
 31:16) - (0 ?
 31:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)));

            *logical++
                = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:0) - (0 ?
 15:0) + 1))))))) << (0 ?
 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E03) & ((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 25:16) - (0 ?
 25:16) + 1))))))) << (0 ?
 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)));

            *logical++
                = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 3:3) - (0 ?
 3:3) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 3:3) - (0 ?
 3:3) + 1))))))) << (0 ?
 3:3))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ?
 3:3) - (0 ?
 3:3) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3)));
        }
        else
        {
            *logical++
                = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 25:16) - (0 ?
 25:16) + 1))))))) << (0 ?
 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:0) - (0 ?
 15:0) + 1))))))) << (0 ?
 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E09) & ((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

            *logical++
                = fencePhysical;

            *logical++
                = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 25:16) - (0 ?
 25:16) + 1))))))) << (0 ?
 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:0) - (0 ?
 15:0) + 1))))))) << (0 ?
 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E0C) & ((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

            *logical++
                = 0;
        }
    }

    if (FenceBytes)
    {
        *FenceBytes = bytes;
    }

    return gcvSTATUS_OK;
}

gceSTATUS
gctaHARDWARE_Construct(
    IN gcTA TA,
    OUT gcTA_HARDWARE * Hardware
    )
{
    gceSTATUS status;

    gcTA_HARDWARE hardware;

    gctaOS os = TA->os;
    gctUINT32 specs;

    gcmkONERROR(gctaOS_Allocate(
        gcmSIZEOF(gcsTA_HARDWARE),
        (gctPOINTER *)&hardware
        ));

    hardware->ta = TA;

    /*************************************/
    /********  Get chip information ******/
    /*************************************/

    /* Read chip feature register. */
    gcmkONERROR(gctaOS_ReadRegister(os, 0x0001C, &hardware->chipFeatures));

    /* Read chip minor featuress register #1. */
    gcmkONERROR(
        gctaOS_ReadRegister(os,
                             0x00074,
                             &hardware->chipMinorFeatures1));

    /* Read chip minor featuress register #2. */
    gcmkONERROR(
        gctaOS_ReadRegister(os,
                             0x00084,
                             &hardware->chipMinorFeatures2));

    /* Read chip minor featuress register #3. */
    gcmkONERROR(
        gctaOS_ReadRegister(os,
                             0x00088,
                             &hardware->chipMinorFeatures3));

    /* Read gcChipSpecs register. */
    gcmkONERROR(gctaOS_ReadRegister(os, 0x00048, &specs));

    hardware->pixelPipes = gcmMAX((((((gctUINT32) (specs)) >> (0 ? 27:25)) & ((gctUINT32) ((((1 ? 27:25) - (0 ? 27:25) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 27:25) - (0 ? 27:25) + 1)))))) ), 1);

    hardware->resolvePipes = hardware->pixelPipes;

    *Hardware = hardware;

    return gcvSTATUS_OK;
OnError:
    return status;
}

gceSTATUS
gctaHARDWARE_IsFeatureAvailable(
    IN gcTA_HARDWARE Hardware,
    IN gceFEATURE Feature
    )
{
    gctBOOL available;

    gcmkHEADER_ARG("Hardware=0x%x Feature=%d", Hardware, Feature);

    /* Only features needed by common kernel logic added here. */
    switch (Feature)
    {
    case gcvFEATURE_PIPE_2D:
        available = ((((gctUINT32) (Hardware->chipFeatures)) >> (0 ?
 9:9) & ((gctUINT32) ((((1 ?
 9:9) - (0 ?
 9:9) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 9:9) - (0 ?
 9:9) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ?
 9:9) - (0 ?
 9:9) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 9:9) - (0 ? 9:9) + 1)))))));
        break;

    case gcvFEATURE_PIPE_3D:
        available = ((((gctUINT32) (Hardware->chipFeatures)) >> (0 ?
 2:2) & ((gctUINT32) ((((1 ?
 2:2) - (0 ?
 2:2) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 2:2) - (0 ?
 2:2) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ?
 2:2) - (0 ?
 2:2) + 1) == 32) ? ~0U : (~(~0U << ((1 ? 2:2) - (0 ? 2:2) + 1)))))));
        break;

    default:
        gcmkFATAL("Invalid feature has been requested.");
        available = gcvFALSE;
    }

    /* Return result. */
    gcmkFOOTER_ARG("%d", available ? gcvSTATUS_TRUE : gcvSTATUS_OK);
    return available ? gcvSTATUS_TRUE : gcvSTATUS_OK;
}

gceSTATUS
gctaHARDWARE_MMU(
    IN gcTA_HARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctUINT32 MtlbAddress,
    IN gctUINT32 SafeAddress,
    OUT gctUINT32 *Bytes
    )
{
    gctUINT32_PTR logical = (gctUINT32_PTR)Logical;
    gctUINT32 config;

    gctUINT32 bytes;

    bytes = 4 * gcmSIZEOF(gctUINT32);

    if (logical)
    {
        config = MtlbAddress
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 0:0) - (0 ?
 0:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 0:0) - (0 ?
 0:0) + 1))))))) << (0 ?
 0:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ?
 0:0) - (0 ?
 0:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)));

        *logical++
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:0) - (0 ?
 15:0) + 1))))))) << (0 ?
 15:0))) | (((gctUINT32) ((gctUINT32) (0x0061) & ((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 25:16) - (0 ?
 25:16) + 1))))))) << (0 ?
 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)));

        *logical++
            = config;

        *logical++
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:0) - (0 ?
 15:0) + 1))))))) << (0 ?
 15:0))) | (((gctUINT32) ((gctUINT32) (0x0060) & ((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 25:16) - (0 ?
 25:16) + 1))))))) << (0 ?
 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)));

        *logical++
            = SafeAddress;
    }

    if (Bytes)
    {
        *Bytes = bytes;
    }

    return gcvSTATUS_OK;
}

gceSTATUS
gctaHARDWARE_MmuEnable(
    IN gcTA_HARDWARE Hardware
    )
{
    gctaOS_WriteRegister(
        Hardware->ta->os,
        0x0018C,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 0:0) - (0 ?
 0:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 0:0) - (0 ?
 0:0) + 1))))))) << (0 ?
 0:0))) | (((gctUINT32) ((gctUINT32) (1 ) & ((gctUINT32) ((((1 ?
 0:0) - (0 ?
 0:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))));

    return gcvSTATUS_OK;
}

gceSTATUS
gctaHARDWARE_FlushMMU(
    IN gcTA_HARDWARE Hardware,
    IN gctPOINTER Logical,
    OUT gctUINT32 *Bytes
    )
{
    gctUINT32_PTR logical = (gctUINT32_PTR) Logical;

    gctUINT32 bytes = 10 * gcmSIZEOF(gctUINT32);

    if (logical)
    {
        /* Arm the PE-FE Semaphore. */
        *logical++
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 25:16) - (0 ?
 25:16) + 1))))))) << (0 ?
 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:0) - (0 ?
 15:0) + 1))))))) << (0 ?
 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E02) & ((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

        *logical++
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 4:0) - (0 ?
 4:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 4:0) - (0 ?
 4:0) + 1))))))) << (0 ?
 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ?
 4:0) - (0 ?
 4:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 12:8) - (0 ?
 12:8) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 12:8) - (0 ?
 12:8) + 1))))))) << (0 ?
 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ?
 12:8) - (0 ?
 12:8) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)));

        /* STALL FE until PE is done flushing. */
        *logical++
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x09 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));

        *logical++
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 4:0) - (0 ?
 4:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 4:0) - (0 ?
 4:0) + 1))))))) << (0 ?
 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ?
 4:0) - (0 ?
 4:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 12:8) - (0 ?
 12:8) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 12:8) - (0 ?
 12:8) + 1))))))) << (0 ?
 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ?
 12:8) - (0 ?
 12:8) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)));


        /* Flush MMU cache. */
        *logical++
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:0) - (0 ?
 15:0) + 1))))))) << (0 ?
 15:0))) | (((gctUINT32) ((gctUINT32) (0x0061) & ((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 25:16) - (0 ?
 25:16) + 1))))))) << (0 ?
 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)));

        *logical++
            = (((((gctUINT32) (~0U)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 4:4) - (0 ?
 4:4) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 4:4) - (0 ?
 4:4) + 1))))))) << (0 ?
 4:4))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ?
 4:4) - (0 ?
 4:4) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 4:4) - (0 ?
 4:4) + 1))))))) << (0 ?
 4:4))) &  ((((gctUINT32) (~0U)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 7:7) - (0 ?
 7:7) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 7:7) - (0 ?
 7:7) + 1))))))) << (0 ?
 7:7))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ?
 7:7) - (0 ?
 7:7) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))));

        *logical++
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 25:16) - (0 ?
 25:16) + 1))))))) << (0 ?
 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ?
 25:16) - (0 ?
 25:16) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 15:0) - (0 ?
 15:0) + 1))))))) << (0 ?
 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E02) & ((gctUINT32) ((((1 ?
 15:0) - (0 ?
 15:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

        *logical++
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 4:0) - (0 ?
 4:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 4:0) - (0 ?
 4:0) + 1))))))) << (0 ?
 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ?
 4:0) - (0 ?
 4:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 12:8) - (0 ?
 12:8) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 12:8) - (0 ?
 12:8) + 1))))))) << (0 ?
 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ?
 12:8) - (0 ?
 12:8) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)));

        /* STALL FE until PE is done flushing. */
        *logical++
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x09 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));

        *logical++
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 4:0) - (0 ?
 4:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 4:0) - (0 ?
 4:0) + 1))))))) << (0 ?
 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ?
 4:0) - (0 ?
 4:0) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 12:8) - (0 ?
 12:8) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 12:8) - (0 ?
 12:8) + 1))))))) << (0 ?
 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ?
 12:8) - (0 ?
 12:8) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)));
    }

    if (Bytes)
    {
        *Bytes = bytes;
    }

    return gcvSTATUS_OK;
}

gceSTATUS
gctaHARDWARE_End(
    IN gcTA_HARDWARE Hardware,
    IN gctPOINTER Logical,
    IN OUT gctUINT32 * Bytes
    )
{
    gctUINT32_PTR logical = (gctUINT32_PTR) Logical;
    gceSTATUS status;

    gcmkHEADER_ARG("Hardware=0x%x Logical=0x%x *Bytes=%lu",
                   Hardware, Logical, gcmOPT_VALUE(Bytes));

    /* Verify the arguments. */
    /* gcmkVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE); */
    gcmkVERIFY_ARGUMENT((Logical == gcvNULL) || (Bytes != gcvNULL));

    if (Logical != gcvNULL)
    {
        if (*Bytes < 8)
        {
            /* Command queue too small. */
            gcmkONERROR(gcvSTATUS_BUFFER_TOO_SMALL);
        }

        /* Append END. */
       logical[0] =
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x02 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));

        gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_HARDWARE, "0x%x: END", Logical);

    }

    if (Bytes != gcvNULL)
    {
        /* Return number of bytes required by the END command. */
        *Bytes = 8;
    }

    /* Success. */
    gcmkFOOTER_ARG("*Bytes=%lu", gcmOPT_VALUE(Bytes));
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

gceSTATUS
gctaHARDWARE_Nop(
    IN gcTA_HARDWARE Hardware,
    IN gctPOINTER Logical,
    IN OUT gctSIZE_T * Bytes
    )
{
    gctUINT32_PTR logical = (gctUINT32_PTR) Logical;
    gceSTATUS status;

    gcmkHEADER_ARG("Hardware=0x%x Logical=0x%x *Bytes=%lu",
                   Hardware, Logical, gcmOPT_VALUE(Bytes));

    /* Verify the arguments. */
    /* gcmkVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE); */
    gcmkVERIFY_ARGUMENT((Logical == gcvNULL) || (Bytes != gcvNULL));

    if (Logical != gcvNULL)
    {
        if (*Bytes < 8)
        {
            /* Command queue too small. */
            gcmkONERROR(gcvSTATUS_BUFFER_TOO_SMALL);
        }

        /* Append NOP. */
        logical[0] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ?
 31:27) - (0 ?
 31:27) + 1))))))) << (0 ?
 31:27))) | (((gctUINT32) (0x03 & ((gctUINT32) ((((1 ?
 31:27) - (0 ?
 31:27) + 1) == 32) ?
 ~0U : (~(~0U << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));

        gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_HARDWARE, "0x%x: NOP", Logical);
    }

    if (Bytes != gcvNULL)
    {
        /* Return number of bytes required by the NOP command. */
        *Bytes = 8;
    }

    /* Success. */
    gcmkFOOTER_ARG("*Bytes=%lu", gcmOPT_VALUE(Bytes));
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}


