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

#define _GC_OBJ_ZONE 2
/*******************************************************************************
************************************ Define ************************************
********************************************************************************/

#define gcdMMU_MTLB_SHIFT           22
#define gcdMMU_STLB_4K_SHIFT        12
#define gcdMMU_STLB_64K_SHIFT       16

#define gcdMMU_MTLB_BITS            (32 - gcdMMU_MTLB_SHIFT)
#define gcdMMU_PAGE_4K_BITS         gcdMMU_STLB_4K_SHIFT
#define gcdMMU_STLB_4K_BITS         (32 - gcdMMU_MTLB_BITS - gcdMMU_PAGE_4K_BITS)
#define gcdMMU_PAGE_64K_BITS        gcdMMU_STLB_64K_SHIFT
#define gcdMMU_STLB_64K_BITS        (32 - gcdMMU_MTLB_BITS - gcdMMU_PAGE_64K_BITS)

#define gcdMMU_MTLB_ENTRY_NUM       (1 << gcdMMU_MTLB_BITS)
#define gcdMMU_MTLB_SIZE            (gcdMMU_MTLB_ENTRY_NUM << 2)
#define gcdMMU_STLB_4K_ENTRY_NUM    (1 << gcdMMU_STLB_4K_BITS)
#define gcdMMU_STLB_4K_SIZE         (gcdMMU_STLB_4K_ENTRY_NUM << 2)
#define gcdMMU_PAGE_4K_SIZE         (1 << gcdMMU_STLB_4K_SHIFT)
#define gcdMMU_STLB_64K_ENTRY_NUM   (1 << gcdMMU_STLB_64K_BITS)
#define gcdMMU_STLB_64K_SIZE        (gcdMMU_STLB_64K_ENTRY_NUM << 2)
#define gcdMMU_PAGE_64K_SIZE        (1 << gcdMMU_STLB_64K_SHIFT)

#define gcdMMU_MTLB_MASK            (~((1U << gcdMMU_MTLB_SHIFT)-1))
#define gcdMMU_STLB_4K_MASK         ((~0U << gcdMMU_STLB_4K_SHIFT) ^ gcdMMU_MTLB_MASK)
#define gcdMMU_PAGE_4K_MASK         (gcdMMU_PAGE_4K_SIZE - 1)
#define gcdMMU_STLB_64K_MASK        ((~((1U << gcdMMU_STLB_64K_SHIFT)-1)) ^ gcdMMU_MTLB_MASK)
#define gcdMMU_PAGE_64K_MASK        (gcdMMU_PAGE_64K_SIZE - 1)

/* Page offset definitions. */
#define gcdMMU_OFFSET_4K_BITS       (32 - gcdMMU_MTLB_BITS - gcdMMU_STLB_4K_BITS)
#define gcdMMU_OFFSET_4K_MASK       ((1U << gcdMMU_OFFSET_4K_BITS) - 1)
#define gcdMMU_OFFSET_64K_BITS      (32 - gcdMMU_MTLB_BITS - gcdMMU_STLB_64K_BITS)
#define gcdMMU_OFFSET_64K_MASK      ((1U << gcdMMU_OFFSET_64K_BITS) - 1)

#define gcdMMU_MTLB_PRESENT         0x00000001
#define gcdMMU_MTLB_EXCEPTION       0x00000002
#define gcdMMU_MTLB_4K_PAGE         (0 << 2)

#define gcdMMU_STLB_PRESENT         0x00000001
#define gcdMMU_STLB_EXCEPTION       0x00000002
#define gcdMMU_STLB_SECURITY        (1 << 4)

#define gcdUSE_MMU_EXCEPTION        1

typedef enum _gceMMU_TYPE
{
    gcvMMU_USED     = (0 << 4),
    gcvMMU_SINGLE   = (1 << 4),
    gcvMMU_FREE     = (2 << 4),
}
gceMMU_TYPE;

#define gcmENTRY_TYPE(x) (x & 0xF0)
/*
* We need flat mapping ta command buffer.

*/

/*
* Helper
*/
gctUINT32
_AddressToIndex(
    IN gcTA_MMU Mmu,
    IN gctUINT32 Address
    )
{
    gctUINT32 mtlbOffset = (Address & gcdMMU_MTLB_MASK) >> gcdMMU_MTLB_SHIFT;
    gctUINT32 stlbOffset = (Address & gcdMMU_STLB_4K_MASK) >> gcdMMU_STLB_4K_SHIFT;

    return (mtlbOffset - Mmu->dynamicMappingStart) * gcdMMU_STLB_4K_ENTRY_NUM + stlbOffset;
}

gctUINT32
_MtlbOffset(
    gctUINT32 Address
    )
{
    return (Address & gcdMMU_MTLB_MASK) >> gcdMMU_MTLB_SHIFT;
}

gctUINT32
_StlbOffset(
    gctUINT32 Address
    )
{
    return (Address & gcdMMU_STLB_4K_MASK) >> gcdMMU_STLB_4K_SHIFT;
}

static gctUINT32
_SetPage(gctUINT32 PageAddress)
{
    return PageAddress
           /* writable */
           | (1 << 2)
           /* Ignore exception */
           | (0 << 1)
           /* Present */
           | (1 << 0);
}

static void
_WritePageEntry(
    IN gctUINT32_PTR PageEntry,
    IN gctUINT32     EntryValue
    )
{
    *PageEntry = EntryValue;

    gctaOS_CacheClean(PageEntry, gcmSIZEOF(gctUINT32));
}

static gctUINT32
_ReadPageEntry(
    IN gctUINT32_PTR PageEntry
    )
{
    return *PageEntry;
}

static gceSTATUS
_FillPageTable(
    IN gctUINT32_PTR PageTable,
    IN gctUINT32     PageCount,
    IN gctUINT32     EntryValue
)
{
    gctUINT i;

    for (i = 0; i < PageCount; i++)
    {
        _WritePageEntry(PageTable + i, EntryValue);
    }

    return gcvSTATUS_OK;
}


static gceSTATUS
_AllocateStlb(
    IN gctaOS Os,
    OUT gcsMMU_STLB_PTR *Stlb
    )
{
    gceSTATUS status;
    gcsMMU_STLB_PTR stlb;
    gctPOINTER pointer;

    /* Allocate slave TLB record. */
    gcmkONERROR(gctaOS_Allocate(gcmSIZEOF(gcsMMU_STLB), &pointer));
    stlb = pointer;

    stlb->size = gcdMMU_STLB_4K_SIZE;

    /* Allocate slave TLB entries. */
    gcmkONERROR(gctaOS_AllocateSecurityMemory(
        Os,
        &stlb->size,
        &stlb->logical,
        &stlb->physical
        ));

    gcmkONERROR(gctaOS_GetPhysicalAddress(Os, stlb->logical, &stlb->physBase));

#if gcdUSE_MMU_EXCEPTION
    _FillPageTable(stlb->logical, stlb->size / 4, gcdMMU_STLB_EXCEPTION);
#else
    gctaOS_ZeroMemory(stlb->logical, stlb->size);
#endif

    *Stlb = stlb;

    return gcvSTATUS_OK;

OnError:
    return status;
}


gceSTATUS
_SetupProcessAddressSpace(
    IN gcTA_MMU Mmu
    )
{
    gceSTATUS status;
    gctINT numEntries = 0;
    gctUINT32_PTR map;
    gctUINT32 flatmappingEntries;
    gctUINT32 free;
    gctUINT32 i = 0;

    numEntries = 1024;

    Mmu->dynamicMappingStart = 0;

    Mmu->pageTableSize = numEntries * 4096;

    Mmu->pageTableEntries = Mmu->pageTableSize / gcmSIZEOF(gctUINT32);

    gcmkONERROR(gctaOS_Allocate(
        Mmu->pageTableSize,
        (void **)&Mmu->mapLogical
        ));

    /* Initilization. */
    map = Mmu->mapLogical;

    flatmappingEntries = Mmu->pageTableEntries / 2;

    map += flatmappingEntries;
    free = Mmu->pageTableEntries - flatmappingEntries;

    _WritePageEntry(map, (free << 8) | gcvMMU_FREE);
    _WritePageEntry(map + 1, ~0U);

    gctaOS_ZeroMemory(Mmu->mtlbLogical, flatmappingEntries * 4);

    Mmu->heapList  = flatmappingEntries;
    Mmu->freeNodes = gcvFALSE;

    for (i = 0; i < numEntries; i++)
    {
        gctUINT32 mtlbEntry;
        struct _gcsMMU_STLB *stlb;
        struct _gcsMMU_STLB **stlbs = (struct _gcsMMU_STLB **)Mmu->stlbs;

        gcmkONERROR(_AllocateStlb(Mmu->os, &stlb));

        mtlbEntry = stlb->physBase
                  | gcdMMU_MTLB_4K_PAGE
                  | gcdMMU_MTLB_PRESENT
                  ;

        /* Insert Slave TLB address to Master TLB entry.*/
        _WritePageEntry((gctUINT32_PTR)Mmu->mtlbLogical + i, mtlbEntry);

        /* Record stlb. */
        stlbs[i] = stlb;
    }

    return gcvSTATUS_OK;

OnError:
    return status;
}


/*
* Address space management
*/

static gceSTATUS
_Link(
    IN gcTA_MMU Mmu,
    IN gctUINT32 Index,
    IN gctUINT32 Next
    )
{
    if (Index >= Mmu->pageTableEntries)
    {
    printf("XQ %s(%d) \n", __FUNCTION__, __LINE__);
        /* Just move heap pointer. */
        Mmu->heapList = Next;
    }
    else
    {
        /* Address page table. */
        gctUINT32_PTR map = Mmu->mapLogical;

        /* Dispatch on node type. */
        switch (gcmENTRY_TYPE(_ReadPageEntry(&map[Index])))
        {
        case gcvMMU_SINGLE:
            /* Set single index. */
            _WritePageEntry(&map[Index], (Next << 8) | gcvMMU_SINGLE);
            break;

        case gcvMMU_FREE:
            /* Set index. */
            _WritePageEntry(&map[Index + 1], Next);
            break;

        default:
            gcmkFATAL("MMU table correcupted at index %u!", Index);
            return gcvSTATUS_HEAP_CORRUPTED;
        }
    }

    /* Success. */
    return gcvSTATUS_OK;
}

static gceSTATUS
_AddFree(
    IN gcTA_MMU Mmu,
    IN gctUINT32 Index,
    IN gctUINT32 Node,
    IN gctUINT32 Count
    )
{
    gctUINT32_PTR map = Mmu->mapLogical;

    if (Count == 1)
    {
        /* Initialize a single page node. */
        _WritePageEntry(map + Node, (~((1U<<8)-1)) | gcvMMU_SINGLE);
    }
    else
    {
        /* Initialize the node. */
        _WritePageEntry(map + Node + 0, (Count << 8) | gcvMMU_FREE);
        _WritePageEntry(map + Node + 1, ~0U);
    }

    /* Append the node. */
    return _Link(Mmu, Index, Node);
}

static gceSTATUS
_Collect(
    IN gcTA_MMU Mmu
    )
{
    gctUINT32_PTR map = Mmu->mapLogical;
    gceSTATUS status;
    gctUINT32 i, previous, start = 0, count = 0;

    previous = Mmu->heapList = ~0U;
    Mmu->freeNodes = gcvFALSE;

    /* Walk the entire page table. */
    for (i = 0; i < Mmu->pageTableEntries; ++i)
    {
        /* Dispatch based on type of page. */
        switch (gcmENTRY_TYPE(_ReadPageEntry(&map[i])))
        {
        case gcvMMU_USED:
            /* Used page, so close any open node. */
            if (count > 0)
            {
                /* Add the node. */
                gcmkONERROR(_AddFree(Mmu, previous, start, count));

                /* Reset the node. */
                previous = start;
                count    = 0;
            }
            break;

        case gcvMMU_SINGLE:
            /* Single free node. */
            if (count++ == 0)
            {
                /* Start a new node. */
                start = i;
            }
            break;

        case gcvMMU_FREE:
            /* A free node. */
            if (count == 0)
            {
                /* Start a new node. */
                start = i;
            }

            /* Advance the count. */
            count += _ReadPageEntry(&map[i]) >> 8;

            /* Advance the index into the page table. */
            i     += (_ReadPageEntry(&map[i]) >> 8) - 1;
            break;

        default:
            gcmkFATAL("MMU page table correcupted at index %u!", i);
            return gcvSTATUS_HEAP_CORRUPTED;
        }
    }

    /* See if we have an open node left. */
    if (count > 0)
    {
        /* Add the node to the list. */
        gcmkONERROR(_AddFree(Mmu, previous, start, count));
    }

    gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_MMU,
                   "Performed a garbage collection of the MMU heap.");

    /* Success. */
    return gcvSTATUS_OK;

OnError:
    /* Return the staus. */
    return status;
}

gcTA_MMU sharedMMU = gcvNULL;

gceSTATUS
gctaMMU_Construct(
    IN gcTA TA,
    OUT gcTA_MMU *Mmu
    )
{
    gceSTATUS status;
    gctUINT32 bytes = 4096;

    gcTA_MMU mmu;

    if (sharedMMU)
    {
        *Mmu = sharedMMU;
        return gcvSTATUS_OK;
    }

    gcmkONERROR(gctaOS_Allocate(
        gcmSIZEOF(gcsTA_MMU),
        (gctPOINTER *)&mmu
        ));

    mmu->os = TA->os;

    /* MTLB bytes. */
    mmu->mtlbBytes = gcdMMU_MTLB_SIZE;

    /* Allocate MTLB. */
    gcmkONERROR(gctaOS_AllocateSecurityMemory(
        TA->os,
        &mmu->mtlbBytes,
        &mmu->mtlbLogical,
        &mmu->mtlbPhysical
        ));

    /* Allocate a array to store stlbs. */
    gcmkONERROR(gctaOS_Allocate(mmu->mtlbBytes, &mmu->stlbs));

    gctaOS_ZeroMemory((gctUINT8_PTR)mmu->stlbs, mmu->mtlbBytes);

    _SetupProcessAddressSpace(mmu);

    /* Allocate MTLB. */
    gcmkONERROR(gctaOS_AllocateSecurityMemory(
        TA->os,
        &bytes,
        &mmu->safePageLogical,
        &mmu->safePagePhysical
        ));

    gcmkONERROR(gctaOS_CreateMutex(TA->os, &mmu->mutex));

    mmu->pageTableChanged = gcvTRUE;

    *Mmu = mmu;

    sharedMMU = mmu;

    return gcvSTATUS_OK;

OnError:
    return status;
}

gceSTATUS
gctaMMU_GetPageEntry(
    IN gcTA_MMU Mmu,
    IN gctUINT32 Address,
    IN gctUINT32_PTR *PageTable
    )
{
    gceSTATUS status;
    struct _gcsMMU_STLB *stlb;
    struct _gcsMMU_STLB **stlbs = (struct _gcsMMU_STLB **)Mmu->stlbs;
    gctUINT32 offset = _MtlbOffset(Address);
    gctUINT32 mtlbEntry;

    gcmkHEADER_ARG("Mmu=0x%x", Mmu);

    /* Verify the arguments. */
    gcmkVERIFY_ARGUMENT((Address & 0xFFF) == 0);

    stlb = stlbs[offset];


    if (stlb == gcvNULL)
    {
        gcmkONERROR(_AllocateStlb(Mmu->os, &stlb));

        mtlbEntry = stlb->physBase
                  | gcdMMU_MTLB_4K_PAGE
                  | gcdMMU_MTLB_PRESENT
                  ;


        /* Insert Slave TLB address to Master TLB entry.*/
        _WritePageEntry((gctUINT32_PTR)Mmu->mtlbLogical + offset, mtlbEntry);

        /* Record stlb. */
        stlbs[offset] = stlb;
    }

    *PageTable = &stlb->logical[_StlbOffset(Address)];

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmkFOOTER();
    return status;
}

/* Flat mapping physical address to GPU virtual address. */
gceSTATUS
gctaMMU_FlatMapping(
    IN gcTA_MMU Mmu,
    IN gctUINT32 Physical
    )
{
    gceSTATUS status;
    gctUINT32 index = _AddressToIndex(Mmu, Physical);
    gctUINT32 i;
    gctBOOL gotIt = gcvFALSE;
    /* gctUINT32_PTR map = Mmu->mapLogical; */
    gctUINT32_PTR map = gcvNULL;
    gctUINT32 previous = ~0U;
    gctUINT32_PTR pageTable;
    gctBOOL secure = gcvFALSE;

    gctaMMU_GetPageEntry(Mmu, Physical, &pageTable);

    gcmkVERIFY_OK(gctaOS_IsPhysicalSecure(Mmu->os, Physical, &secure));

    if (secure)
    {
        _WritePageEntry(pageTable, _SetPage(Physical | gcdMMU_STLB_SECURITY));
    }
    else
    {
        _WritePageEntry(pageTable, _SetPage(Physical));
    }

    if (map)
    {
        /* Find node which contains index. */
        for (i = 0; !gotIt && (i < Mmu->pageTableEntries);)
        {
            gctUINT32 numPages;

            switch (gcmENTRY_TYPE(_ReadPageEntry(&map[i])))
            {
            case gcvMMU_SINGLE:
                if (i == index)
                {
                    gotIt = gcvTRUE;
                }
                else
                {
                    previous = i;
                    i = _ReadPageEntry(&map[i]) >> 8;
                }
                break;

            case gcvMMU_FREE:
                numPages = _ReadPageEntry(&map[i]) >> 8;
                if (index >= i && index < i + numPages)
                {
                    gotIt = gcvTRUE;
                }
                else
                {
                    previous = i;
                    i = _ReadPageEntry(&map[i + 1]);
                }
                break;

            default:
                gcmkFATAL("MMU table correcupted at index %u!", index);
                printf("XQ %s(%d) %x = \n", __FUNCTION__, __LINE__, index);
                gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
            }
        }


        switch (gcmENTRY_TYPE(_ReadPageEntry(&map[i])))
        {
        case gcvMMU_SINGLE:
            /* Unlink single node from free list. */
            gcmkONERROR(
                _Link(Mmu, previous, _ReadPageEntry(&map[i]) >> 8));
            break;

        case gcvMMU_FREE:
            /* Split the node. */
            {
                gctUINT32 start;
                gctUINT32 next = _ReadPageEntry(&map[i+1]);
                gctUINT32 total = _ReadPageEntry(&map[i]) >> 8;
                gctUINT32 countLeft = index - i;
                gctUINT32 countRight = total - countLeft - 1;


                if (countLeft)
                {
                    start = i;
                    _AddFree(Mmu, previous, start, countLeft);
                    previous = start;
                }

                if (countRight)
                {
                    start = index + 1;
                    _AddFree(Mmu, previous, start, countRight);
                    previous = start;
                }

                _Link(Mmu, previous, next);

            }
            break;
        }

        map[index] = gcvMMU_USED;




    }

    return gcvSTATUS_OK;

OnError:

    /* Roll back. */
    return status;
}

gceSTATUS
gctaMMU_AllocatePages(
    IN gcTA_MMU Mmu,
    IN gctSIZE_T PageCount,
    OUT gctUINT32 * Address
    )
{
    gceSTATUS status;
    gctUINT32 index = 0, previous = ~0U, left;
    gctUINT32_PTR map;
    gctBOOL gotIt;
    gctUINT32 address;
    gctBOOL acquired = gcvFALSE;

    gcmkHEADER_ARG("Mmu=0x%x PageCount=%lu", Mmu, PageCount);

    /* Verify the arguments. */
    gcmkVERIFY_ARGUMENT(PageCount > 0);

    if (PageCount > Mmu->pageTableEntries)
    {
        /* Not enough pages avaiable. */
        gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
    }

    gctaOS_AcquireMutex(Mmu->os, Mmu->mutex);
    acquired = gcvTRUE;

    /* Cast pointer to page table. */
    for (map = Mmu->mapLogical, gotIt = gcvFALSE; !gotIt;)
    {
        /* printf("XQ %s(%d) heapList=%d\n", __FUNCTION__, __LINE__, Mmu->heapList); */
        /* Walk the heap list. */
        for (index = Mmu->heapList; !gotIt && (index < Mmu->pageTableEntries);)
        {
            /* Check the node type. */
            switch (gcmENTRY_TYPE(_ReadPageEntry(&map[index])))
            {
            case gcvMMU_SINGLE:
                /* Single odes are valid if we only need 1 page. */
                if (PageCount == 1)
                {
                    gotIt = gcvTRUE;
                }
                else
                {
                    /* Move to next node. */
                    previous = index;
                    index    = _ReadPageEntry(&map[index]) >> 8;
                }
                break;

            case gcvMMU_FREE:
                /* Test if the node has enough space. */
                if (PageCount <= (_ReadPageEntry(&map[index]) >> 8))
                {
                    gotIt = gcvTRUE;
                }
                else
                {
                    /* Move to next node. */
                    previous = index;
                    index    = _ReadPageEntry(&map[index + 1]);
                }
                break;

            default:
                gcmkFATAL("MMU table correcupted at index %u!", index);
                printf("XQ %s(%d) \n", __FUNCTION__, __LINE__);
                gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
            }
        }

        /* Test if we are out of memory. */
        if (index >= Mmu->pageTableEntries)
        {
            if (Mmu->freeNodes)
            {
                /* Time to move out the trash! */
                gcmkONERROR(_Collect(Mmu));
            }
            else
            {
                /* Out of resources. */
                gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
            }
        }
    }

    switch (gcmENTRY_TYPE(_ReadPageEntry(&map[index])))
    {
    case gcvMMU_SINGLE:
        /* Unlink single node from free list. */
        gcmkONERROR(
            _Link(Mmu, previous, _ReadPageEntry(&map[index]) >> 8));
        break;

    case gcvMMU_FREE:
        /* Check how many pages will be left. */
        left = (_ReadPageEntry(&map[index]) >> 8) - PageCount;
        switch (left)
        {
        case 0:
            /* The entire node is consumed, just unlink it. */
            gcmkONERROR(
                _Link(Mmu, previous, _ReadPageEntry(&map[index + 1])));
            break;

        case 1:
            /* One page will remain.  Convert the node to a single node and
            ** advance the index. */
            _WritePageEntry(&map[index], (_ReadPageEntry(&map[index + 1]) << 8) | gcvMMU_SINGLE);
            index ++;
            break;

        default:
            /* Enough pages remain for a new node.  However, we will just adjust
            ** the size of the current node and advance the index. */
            _WritePageEntry(&map[index], (left << 8) | gcvMMU_FREE);
            index += left;
            break;
        }
        break;
    }

    /* Mark node as used. */
    gcmkONERROR(_FillPageTable(&map[index], PageCount, gcvMMU_USED));

    gctaOS_ReleaseMutex(Mmu->os, Mmu->mutex);

    /* Build virtual address. */
    {
        gctUINT32 masterOffset = index / gcdMMU_STLB_4K_ENTRY_NUM
                               + Mmu->dynamicMappingStart;
        gctUINT32 slaveOffset = index % gcdMMU_STLB_4K_ENTRY_NUM;

        address = (masterOffset << gcdMMU_MTLB_SHIFT)
                | (slaveOffset << gcdMMU_STLB_4K_SHIFT);
    }

    if (Address != gcvNULL)
    {
        *Address = address;
    }

    Mmu->pageTableChanged = gcvTRUE;

    /* Success. */
    gcmkFOOTER_ARG("*Address=%08x",
                    gcmOPT_VALUE(Address));
    return gcvSTATUS_OK;

OnError:
    if (acquired)
    {
        gctaOS_ReleaseMutex(Mmu->os, Mmu->mutex);
    }

    /* Return the status. */
    gcmkFOOTER();
    return status;
}

gceSTATUS
gctaMMU_FreePages(
    IN gcTA_MMU Mmu,
    IN gctUINT32 Address,
    IN gctSIZE_T PageCount
    )
{
    gctUINT32_PTR node;
    gceSTATUS status;

#if gcdUSE_MMU_EXCEPTION
    gctUINT32 i;
    struct _gcsMMU_STLB *stlb;
    struct _gcsMMU_STLB **stlbs = Mmu->stlbs;
#endif

    gcmkHEADER_ARG("Mmu=0x%x Address=0x%x PageCount=%lu",
                   Mmu, Address, PageCount);

    /* Verify the arguments. */
    gcmkVERIFY_ARGUMENT(PageCount > 0);

    /* Get the node by index. */
    node = Mmu->mapLogical + _AddressToIndex(Mmu, Address);

    gctaOS_AcquireMutex(Mmu->os, Mmu->mutex);

    if (PageCount == 1)
    {
       /* Single page node. */
        _WritePageEntry(node, (~((1U<<8)-1)) | gcvMMU_SINGLE);
    }
    else
    {
        /* Mark the node as free. */
        _WritePageEntry(node, (PageCount << 8) | gcvMMU_FREE);
        _WritePageEntry(node + 1, ~0U);
    }

    /* We have free nodes. */
    Mmu->freeNodes = gcvTRUE;

    gctaOS_ReleaseMutex(Mmu->os, Mmu->mutex);

#if gcdUSE_MMU_EXCEPTION
    for (i = 0; i < PageCount; i++)
    {
        /* Get */
        stlb = stlbs[_MtlbOffset(Address)];

        /* Enable exception */
        stlb->logical[_StlbOffset(Address)] = gcdMMU_STLB_EXCEPTION;
    }
#endif

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmkFOOTER();
    return status;
}


gceSTATUS
gctaMMU_SetPage(
    IN gcTA_MMU Mmu,
    IN gctUINT32 PageAddress,
    IN gctBOOL Secure,
    IN gctUINT32 *PageEntry
    )
{
    gctBOOL secure;

    gcmkHEADER_ARG("Mmu=0x%x", Mmu);

    /* Verify the arguments. */
    gcmkVERIFY_ARGUMENT(PageEntry != gcvNULL);
    gcmkVERIFY_ARGUMENT(!(PageAddress & 0xFFF));

    if (Secure)
    {
        PageAddress |= gcdMMU_STLB_SECURITY;
    }

    _WritePageEntry(PageEntry, _SetPage(PageAddress));

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gctaMMU_Enable(
    IN gcTA_MMU Mmu,
    IN gcTA TA
    )
{
    gceSTATUS status;
    gctUINT8 buffer[4 * 4];
    gctUINT32 address;
    gctUINT32 safeAddress;
    gctUINT32 bytes = 4 * 4;

    gcTA_HARDWARE hardware = TA->hardware;
    gcTA_COMMAND command = TA->command;

    gctaOS_GetPhysicalAddress(Mmu->os, Mmu->mtlbLogical, &address);

    gctaOS_GetPhysicalAddress(Mmu->os, Mmu->safePageLogical, &safeAddress);

    gcmkONERROR(gctaHARDWARE_MMU(hardware, buffer, address, safeAddress, &bytes));

    gcmkONERROR(gctaCOMMAND_Commit(command, buffer, bytes));

    gcmkONERROR(gctaCOMMAND_Stall(command));

    gcmkONERROR(gctaHARDWARE_MmuEnable(hardware));

    return gcvSTATUS_OK;

OnError:
    return status;
}

gceSTATUS
gctaMMU_FlatMapping2G(
    IN gcTA_MMU Mmu
    )
{
    gceSTATUS status;
    gctUINT32 physical = 0;

    for (physical = 0; physical < 0x80000000; physical += 4096)
    {
        gcmkVERIFY_OK(gctaMMU_FlatMapping(Mmu, physical));
    }

    /* gctaMMU_CheckFlatMapping2G(Mmu); */

    return gcvSTATUS_OK;
OnError:

    return status;
}

gceSTATUS
gctaMMU_CheckFlatMapping2G(
    IN gcTA_MMU Mmu
    )
{
    gceSTATUS status;
    gctUINT32 physical = 0;
    gctUINT32 *entry;

    for (physical = 0; physical < 0x80000000; physical += 4096)
    {
        gctaMMU_GetPageEntry(Mmu, physical, &entry);

        if ((*entry & 0xfffffff0) != physical)
        {
            printf("XQ %s(%d) \n", __FUNCTION__, __LINE__);
        }
    }

    return gcvSTATUS_OK;
OnError:

    return status;
}

gceSTATUS
gctaMMU_CheckHardwareAddressSecure(
    IN gcTA_MMU Mmu,
    IN gctUINT32 Address,
    IN gctBOOL *Secure
    )
{
    gceSTATUS status;
    gctUINT32_PTR entry;

    if (Address & 0xFFF)
    {
        Address = Address & (~0xFFF);
    }

    gcmkONERROR(gctaMMU_GetPageEntry(Mmu, Address, &entry));

    *Secure = *entry & gcdMMU_STLB_SECURITY;

    return gcvSTATUS_OK;

OnError:
    return status;
}

