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

#define gcvTA_COMMAND_BUSY (~0U)
#define gcvTA_COMMAND_IDLE (0)

#define gcvTA_COMMAND_QUEUE_NUMBER 4

typedef struct _gcTA_COMMAND {
    gcTA        ta;

    /* gctUINT32   bytes; */
    gctSIZE_T   bytes;

    gctPOINTER  logical;
    gctUINT32   offset;
    gctPOINTER  lastWaitLink;

    gctSIZE_T   waitLinkBytes;
    gctUINT32   fenceBytes;

    struct _gcsTA_COMMAND_QUEUE {
        gctPOINTER  logical;
        gctUINT32   physical;

        /* Fence address. */
        gctPOINTER  fenceLogical;
        gctUINT32   fencePhysical;
    }
    queues[gcvTA_COMMAND_QUEUE_NUMBER];

    gctUINT32   index;
    gctPOINTER  syncFenceLogical;
    gctUINT32   syncFencePhysical;
} gcsTA_COMMAND;

#define gcdCOMMAND_BUFFER_SIZE (4096 * 80)

extern void
_DumpCommand(
    gctUINT32 * Buffer
    );

#define gcmkPTR_OFFSET(ptr, offset) \
(\
        (gctUINT8_PTR)(ptr) + (offset) \
)

gctPOINTER
_CurrentLogical(
    gcTA_COMMAND Command
    )
{
    return gcmkPTR_OFFSET(Command->logical, Command->offset);
}

void
_WaitLink(
    IN gcTA_COMMAND Command,
    IN gctUINT32 Offset
    )
{
    gcTA ta = Command->ta;

    gctUINT8_PTR waitLinkLogical;

    waitLinkLogical = gcmkPTR_OFFSET(Command->logical, Offset);

    gctaHARDWARE_WaitLink(
        ta,
        waitLinkLogical,
        0,
        &Command->waitLinkBytes,
        gcvNULL
        );

    gctaOS_CacheClean(waitLinkLogical, Command->waitLinkBytes);

    Command->lastWaitLink = waitLinkLogical;
}

static gctBOOL
_QueueIdle(
    IN gcTA_COMMAND Command,
    IN gctUINT32 Index
    )
{
    gctUINT32_PTR fenceLogical = (gctUINT32_PTR)Command->queues[Index].fenceLogical;

    gctaOS_CacheInvalidate(fenceLogical, sizeof(gctUINT32));

    return (*fenceLogical == gcvTA_COMMAND_IDLE);
}

static void
_SetQueueBusy(
    IN gcTA_COMMAND Command,
    IN gctUINT32 Index
    )
{
    gctUINT32_PTR fenceLogical = (gctUINT32_PTR)Command->queues[Index].fenceLogical;

    *fenceLogical = gcvTA_COMMAND_BUSY;

    gctaOS_CacheClean(fenceLogical, gcmSIZEOF(gctUINT32));
}

static gceSTATUS
_NewQueue(
    IN gcTA_COMMAND Command,
    OUT gctINT32 * PreviousIndex
    )
{
    gctUINT32 currentIndex = Command->index;
    gctUINT32 newIndex = (currentIndex + 1) % gcvTA_COMMAND_QUEUE_NUMBER;
    gctUINT32 fenceBytes;
    gctUINT32 waitLinkOffset = 0;

    while (!_QueueIdle(Command, newIndex))
    {
        printf("XQ %s(%d) \n", __FUNCTION__, __LINE__);
        gctaOS_Delay(1000);
        /* We don't wait in trust zone but go back to kernel and ask kernel to call again. */
    }

    /* Set current. */
    Command->logical = Command->queues[newIndex].logical;
    Command->offset  = 0;
    Command->index   = newIndex;

    _SetQueueBusy(Command, newIndex);

    *PreviousIndex = currentIndex;

    return gcvSTATUS_OK;
}

gceSTATUS
_FlatMapCommandBuffer(
    IN gcTA TA,
    IN gctPOINTER Logical,
    IN gctUINT32 PageCount
)
{
    gceSTATUS status;
    gcTA_MMU mmu = TA->mmu;
    gctINT i;
    gctUINT32 physical;

    gctaOS_GetPhysicalAddress(TA->os, Logical, &physical);

    for (i = 0; i < PageCount; i++)
    {
        gcmkONERROR(gctaMMU_FlatMapping(mmu, physical + i * 4096));
    }

    return gcvSTATUS_OK;
OnError:
    return status;
}

gceSTATUS
gctaCOMMAND_Construct(
    IN gcTA TA,
    OUT gcTA_COMMAND *Command
    )
{
    gceSTATUS status;
    gcTA_COMMAND command;
    gctPOINTER pointer;

    gctSIZE_T bytes = 4096;
    gctaOS os = TA->os;
    gctINT i;

    gcmkONERROR(gctaOS_Allocate(gcmSIZEOF(gcsTA_COMMAND), &pointer));

    command = (gcTA_COMMAND)pointer;

    command->bytes   = gcdCOMMAND_BUFFER_SIZE;
    command->offset  = 0;
    command->ta      = TA;
    command->logical = gcvNULL;
    command->index   = 0;

    for (i = 0; i < gcvTA_COMMAND_QUEUE_NUMBER; i++)
    {
        /* Allocate command buffer. */
        gcmkONERROR(gctaOS_AllocateSecurityMemory(
            os,
            &command->bytes,
            &command->queues[i].logical,
            &command->queues[i].physical
            ));

        /* Allocate fence buffer */
        gcmkONERROR(gctaOS_AllocateSecurityMemory(
            os,
            &bytes,
            &command->queues[i].fenceLogical,
            &command->queues[i].fencePhysical
            ));

        /* Mark this queue a IDLE queue. */
        *(gctUINT32 *)command->queues[i].fenceLogical = gcvTA_COMMAND_IDLE;

        gctaOS_CacheFlush(command->queues[i].fenceLogical, sizeof(gctUINT32));

        /* Set up page table for this command buffer. */
        gcmkONERROR(_FlatMapCommandBuffer(
            TA,
            command->queues[i].logical,
            command->bytes/4096
            ));
    }

    /* Allocate sync fence buffer. */
    gcmkONERROR(gctaOS_AllocateSecurityMemory(
        os,
        &bytes,
        &command->syncFenceLogical,
        &command->syncFencePhysical
        ));

    /* Query fence bytes. */
    gcmkONERROR(gctaHARDWARE_Fence(
        TA,
        gcvNULL,
        gcvNULL,
        &command->fenceBytes
        ));

    /* Query WAIT/LINK bytes. */
    gcmkONERROR(gctaHARDWARE_WaitLink(
        TA,
        gcvNULL,
        0,
        &command->waitLinkBytes,
        gcvNULL
        ));

    *Command = command;

    return gcvSTATUS_OK;

OnError:
    return status;
}

gceSTATUS
gctaCOMMAND_Destory(
    IN gcTA TA,
    OUT gcTA_COMMAND Command
    )
{
    gctINT i;

    for (i = 0; i < gcvTA_COMMAND_QUEUE_NUMBER; i++)
    {

    }

    gcmkVERIFY_OK(gctaOS_Free(Command));

    return gcvSTATUS_OK;
}

gceSTATUS
gctaCOMMAND_Start(
    IN gcTA_COMMAND Command
    )
{
    gceSTATUS  status;
    gcTA       ta = Command->ta;
    gctPOINTER waitLinkLogical = 0;
    gctINT32   previousIndex = -1;

    if (Command->logical == gcvNULL)
    {
        gcmkONERROR(_NewQueue(Command, &previousIndex));
    }

    /* Start at beginning of page. */
    Command->offset = 0;

    waitLinkLogical = gcmkPTR_OFFSET(Command->logical, Command->offset);

    /* Append a WAIT/LINK. */
    gcmkONERROR(gctaHARDWARE_WaitLink(
        ta,
        waitLinkLogical,
        0,
        &Command->waitLinkBytes,
        gcvNULL
        ));

    gctaOS_CacheClean(waitLinkLogical, Command->waitLinkBytes);

    /* Advance current position. */
    Command->offset += Command->waitLinkBytes;

    /* Store wait location. */
    Command->lastWaitLink = waitLinkLogical;

    /* Start FE. */
    gcmkONERROR(gctaHARDWARE_Execute(
        ta,
        waitLinkLogical,
        Command->waitLinkBytes
        ));

    /* Enable MMU if needed. */


    return gcvSTATUS_OK;

OnError:
    return status;
}

gceSTATUS
gctaCOMMAND_Stop(
    IN gcTA_COMMAND Command
    )
{
    gceSTATUS status;
    gctUINT32 bytes = 8;
    gcmkONERROR(gctaHARDWARE_End(Command->ta->hardware, Command->lastWaitLink, &bytes));
    return gcvSTATUS_OK;

OnError:
    return status;
}

gceSTATUS
gctaCOMMAND_Commit(
    IN gcTA_COMMAND Command,
    IN gctUINT8_PTR Buffer,
    IN gctUINT32 Bytes
    )
{
    gceSTATUS   status;
    gctPOINTER  commandLogical;
    gctUINT32   commandBytes;
    gctUINT32   reservedBytes;
    gctUINT32   waitLinkOffset;
    gctPOINTER  lastWaitLink;
    gctSIZE_T   bytes = 8;
    gctINT      previousIndex = -1;
    gctPOINTER  fenceLogical;
    gcTA        ta;
    gctPOINTER  entryLogical;
    gctUINT32   fenceBytes = 0;
    gctBOOL     allow;

    /* Shortcut to ta. */
    ta = Command->ta;

    /* Aligned input buffer to 8 bytes. */
    commandBytes = gcmALIGN(Bytes, 8);

    /* Bytes needed by WAIT/LINK and command buffer from non-secure world. */
    reservedBytes = commandBytes + Command->waitLinkBytes;

    /* Enough space ? */
    if (gcdCOMMAND_BUFFER_SIZE - Command->offset < reservedBytes)
    {
        gcmkONERROR(_NewQueue(Command, &previousIndex));
        gcmkPRINT("wrap\n");
    }

    /* Location of LINK. */
    lastWaitLink = Command->lastWaitLink;

    entryLogical = commandLogical = _CurrentLogical(Command);

    if (previousIndex >= 0)
    {
        fenceBytes = Command->fenceBytes;

        /* Need add a fence for previous command. */
        gcmkONERROR(gctaHARDWARE_Fence(
            Command->ta,
            entryLogical,
            Command->queues[previousIndex].fenceLogical,
            &Command->fenceBytes
            ));

        gctaOS_CacheClean(entryLogical, Command->fenceBytes);

        commandLogical = gcmkPTR_OFFSET(entryLogical, fenceBytes);
        reservedBytes += fenceBytes;
    }

    /* After commit buffer executed, FE will stop here (offset). */
    waitLinkOffset = Command->offset + commandBytes + fenceBytes;

    _WaitLink(Command, waitLinkOffset);

    /* Do we need add padding nop???. */
    gctaOS_MemCopy((gctUINT8_PTR)commandLogical, Buffer, commandBytes);

    gcmkONERROR(gctaHARDWARE_VerifyCommand(
        ta,
        (gctUINT32_PTR)commandLogical,
        commandBytes,
        &allow
        ));

    if (allow == gcvFALSE)
    {
        gctUINT32_PTR secureBuffer = (gctUINT32_PTR)commandLogical;
        gctINT i;
        gctUINT32 bytes = 8;

        /* memset command buffer to NOP. */
        for (i = 0; i < commandBytes; i += 4)
        {
            gcmkVERIFY_OK(gctaHARDWARE_Nop(
                ta->hardware,
                secureBuffer,
                &bytes
                ));

            secureBuffer++;
        }
    }

    gctaOS_CacheClean(commandLogical, commandBytes);

    /* Link to copied command buffer. */
    gcmkONERROR(gctaHARDWARE_Link(
        ta,
        lastWaitLink,
        entryLogical,
        reservedBytes,
        &bytes
        ));

    Command->offset += reservedBytes;


        /* _DumpCommand(Command->logical); */

    return gcvSTATUS_OK;

OnError:
    return status;
}

gceSTATUS
gctaCOMMAND_Stall(
    IN gcTA_COMMAND Command
    )
{
    gceSTATUS status;
    gctUINT8 buffer[32 * sizeof(gctUINT32)];
    gctUINT32 bytes = 32 * sizeof(gctUINT32);
    gctUINT32 data;
    gctUINT32 delay = 1;
    gctUINT32 timer = 0;

    gctUINT32_PTR logical = Command->syncFenceLogical;

    gctaOS_WriteSecurityMemory(logical, 4, gcvTA_COMMAND_BUSY);

    gcmkONERROR(gctaHARDWARE_Fence(
        Command->ta,
        buffer,
        Command->syncFenceLogical,
        &bytes
        ));

    gcmkONERROR(gctaCOMMAND_Commit(
        Command,
        buffer,
        bytes
        ));

    do {
        gctaOS_ReadSecurityMemory(logical, 4, &data);

        if (data == gcvTA_COMMAND_IDLE)
        {
            break;
        }
        else
        {
            gctaOS_Delay(delay);
            timer += delay;
            delay *= 2;

            if (timer > gcdGPU_TIMEOUT)
            {
                gcmkONERROR(gcvSTATUS_TIMEOUT);
            }
        }
    }
    while (gcvTRUE);

    return gcvSTATUS_OK;

OnError:
    return status;
}

