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

void
_DumpCommand(
    gctUINT32 * Buffer
    )
{
        gctUINT32 * buffer = Buffer;
        int i = 0;

        for (i = 0; i < 10; i++)
        {
            printf("%08x %08x %08x %08x %08x %08x %08x %08x \n",
                       buffer[0],
                       buffer[1],
                       buffer[2],
                       buffer[3],
                       buffer[4],
                       buffer[5],
                       buffer[6],
                       buffer[7]);
            buffer += 8;
        }

        printf("====================================\n");
}

void
_DumpCommandRaw(
    gctUINT32 * Buffer,
    gctUINT32 Bytes
    )
{
        gctUINT32 * buffer = Buffer;
        gctUINT32 row, left;

        row = Bytes / 8;
        left = Bytes % 8;
        int i = 0;

        for (i = 0; i < row; i++)
        {
            printf("%08x %08x %08x %08x %08x %08x %08x %08x\n ",
                       buffer[0],
                       buffer[1],
                       buffer[2],
                       buffer[3],
                       buffer[4],
                       buffer[5],
                       buffer[6],
                       buffer[7]);
            buffer += 8;
        }

        for (i = 0; i < left; i++)
        {
            printf("%08X ");
        }

        if (left)
        {
            printf("\n");
        }

        gcmkPRINT("====================================\n");
}




gctUINT8_PTR
_Handle2Logical(
    gctUINT32 Handle
    )
{
    gcTA_NODE node = (gcTA_NODE) Handle;

    return node->logical;
}

gctUINT32
_Logical2Handle(
    gctPOINTER Logical
    )
{
    return (gctUINT32)Logical;
}

/*******************************************************************************
**
**  gcTA_Construct
**
**  Construct a new gcTA object.
*/
int
gcTA_Construct(
    IN gctaOS Os,
    OUT gcTA *TA
    )
{
    gceSTATUS status;
    gctPOINTER pointer;
    gcTA ta;

    /* Construct a gcTA object. */
    gcmkONERROR(gctaOS_Allocate(sizeof(struct _gcTA), &pointer));

    ta = (gcTA)pointer;

    ta->os = Os;

    /* Hardware. */
    gcmkONERROR(gctaHARDWARE_Construct(ta, &ta->hardware));

        /* MMU. */
    /* Maybe passed with a global mmu later. */
    gcmkONERROR(gctaMMU_Construct(ta, &ta->mmu));

    /* Command. */
    gcmkONERROR(gctaCOMMAND_Construct(ta, &ta->command));

    /* Parser. */
    gcmkONERROR(gctaPARSER_Construct(ta, &ta->parser));

    status = gctaMMU_FlatMapping2G(ta->mmu);

    *TA = ta;

    return 0;

OnError:
    return status;
}

/*******************************************************************************
**
**  gcTA_Construct
**
**  Destroy a gcTA object.
*/
int
gcTA_Destroy(
    IN gcTA TA
    )
{

    /* Destroy. */
    return 0;
}

gceSTATUS
gcTA_AllocateSecurityMemory(
    IN gcTA TA,
    IN gctUINT32 Bytes,
    OUT gctUINT32 * Handle
    )
{
    gceSTATUS status;
    gctPOINTER logical;
    gctUINT32 physical;
    gcTA_NODE node = gcvNULL;

    gcmkONERROR(
        gctaOS_Allocate(gcmSIZEOF(*node), (gctPOINTER *)&node));

    node->bytes = Bytes;
    node->type = gcvTA_USER_COMMAND;

    gcmkONERROR(
        gctaOS_AllocateSecurityMemory(TA->os, &node->bytes, &logical, &physical));

    node->logical = logical;
    node->physical = physical;

    *Handle = _Logical2Handle(node);

    return gcvSTATUS_OK;

OnError:
    return status;
}

/*
*   Map a scatter gather list into gpu address space.
*
*/
gceSTATUS
gcTA_MapMemory (
    IN gcTA TA,
    IN gctUINT32 *PhysicalArray,
    IN gctUINT32 PageCount,
    OUT gctUINT32 *GPUAddress
    )
{
    gceSTATUS status;
    gcTA_MMU mmu;
    gctUINT32 pageCount = PageCount;
    gctUINT32 i;
    gctUINT32 gpuAddress;
    gctBOOL allocated = gcvFALSE;
    gctBOOL secure = gcvFALSE;

    mmu = TA->mmu;

    for (i = 0; i < pageCount; i++)
    {
        gctUINT32 physical = PhysicalArray[i];

        gcmkVERIFY_OK(gctaOS_IsPhysicalSecure(TA->os, physical, &secure));

        if (secure)
        {
            break;
        }
    }


    /* Reserve space. */
    gcmkONERROR(gctaMMU_AllocatePages(
        mmu,
        pageCount,
        &gpuAddress
        ));

    *GPUAddress = gpuAddress;

    allocated = gcvTRUE;

    /* Fill in page table. */
    for (i = 0; i < pageCount; i++)
    {
        gctUINT32 physical = PhysicalArray[i];
        gctUINT32_PTR entry;

        gcmkONERROR(gctaMMU_GetPageEntry(mmu, gpuAddress, &entry));

        gctaMMU_SetPage(mmu, physical, secure, entry);

        gpuAddress += 4096;
    }

    return gcvSTATUS_OK;

OnError:
    if (allocated == gcvTRUE)
    {
        gctaMMU_FreePages(mmu, gpuAddress, pageCount);
    }

    return status;
}

gceSTATUS
gcTA_UnmapMemory (
    IN gcTA TA,
    IN gctUINT32 GPUAddress,
    IN gctUINT32 PageCount
    )
{
    gctaMMU_FreePages(TA->mmu, GPUAddress, PageCount);
    return gcvSTATUS_OK;
}

int
gcTA_Dispatch(
    IN gcTA TA,
    IN gcsTA_INTERFACE * Interface
    )
{
    int command = Interface->command;

    gceSTATUS status = gcvSTATUS_OK;

    switch (command)
    {
    case KERNEL_START_COMMAND:
        status = gctaCOMMAND_Start(TA->command);

        if (status == gcvSTATUS_OK && !TA->mmuEnabled)
        {
            status = gctaMMU_Enable(TA->mmu, TA);
            TA->mmuEnabled = gcvTRUE;
        }

        break;

    case KERNEL_ALLOCATE_SECRUE_MEMORY:
        status = gcTA_AllocateSecurityMemory(
            TA,
            Interface->u.AllocateSecurityMemory.bytes,
            &Interface->u.AllocateSecurityMemory.memory_handle
            );
        break;

    case KERNEL_EXECUTE:
        if (TA->mmu->pageTableChanged || 1)
        {
            gctUINT32 flushMmu[10 * 4];
            gctUINT32 bytes = 10 * 4;

            gctaHARDWARE_FlushMMU(gcvNULL, flushMmu, &bytes);

            status = gctaCOMMAND_Commit(
                TA->command,
                (gctUINT8_PTR)flushMmu,
                bytes
                );

            TA->mmu->pageTableChanged = gcvFALSE;
        }

        status = gctaCOMMAND_Commit(
            TA->command,
            (gctUINT8_PTR)Interface->u.Execute.command_buffer,
            Interface->u.Execute.command_buffer_length
            );

        break;

    case KERNEL_MAP_MEMORY:
        status = gcTA_MapMemory(
            TA,
            Interface->u.MapMemory.physicals,
            Interface->u.MapMemory.pageCount,
            &Interface->u.MapMemory.gpuAddress
            );

        break;

    case KERNEL_UNMAP_MEMORY:
        status = gcTA_UnmapMemory(
            TA,
            Interface->u.UnmapMemory.gpuAddress,
            Interface->u.UnmapMemory.pageCount
            );
        break;

    default:
        gcmkASSERT(0);

        status = gcvSTATUS_INVALID_ARGUMENT;
        break;
    }

    Interface->result = status;

    return 0;
}



