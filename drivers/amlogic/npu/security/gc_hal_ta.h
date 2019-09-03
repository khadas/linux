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


#ifndef _GC_HAL_TA_H_
#define _GC_HAL_TA_H_
#include "gc_hal_types.h"
#include "gc_hal_security_interface.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct _gctaOS * gctaOS;

typedef struct _gcTA_COMMAND    * gcTA_COMMAND;
typedef struct _gcTA_PARSER     * gcTA_PARSER;
typedef struct _gcTA_HARDWARE   * gcTA_HARDWARE;
typedef struct _gcTA_MMU        * gcTA_MMU;
typedef struct _gcTA            * gcTA;

typedef struct _gcsMMU_STLB *gcsMMU_STLB_PTR;

typedef struct _gcsMMU_STLB
{
    gctPHYS_ADDR    physical;
    gctUINT32_PTR   logical;
    gctSIZE_T       size;
    gctUINT32       physBase;
    gctSIZE_T       pageCount;
    gctUINT32       mtlbIndex;
    gctUINT32       mtlbEntryNum;
    gcsMMU_STLB_PTR next;
} gcsMMU_STLB;

typedef struct _gcTA_MMU {
    gctaOS      os;

    gctPOINTER  mtlbLogical;
    gctUINT32   mtlbBytes;
    gctUINT32   mtlbPhysical;

    gctUINT32   dynamicMappingStart;
    gctPOINTER  stlbs;


    gctUINT32   pageTableEntries;
    gctUINT32   heapList;

    gctUINT32_PTR mapLogical;
    gctBOOL     freeNodes;
    gctUINT32   pageTableSize;

    gctPOINTER safePageLogical;
    gctUINT32  safePagePhysical;

    gctBOOL    pageTableChanged;
    gctPOINTER mutex;
}
gcsTA_MMU;


/*
    Trust Application is a object needed to be created as a context in trust zone.
    One client for a core.
*/
typedef struct _gcTA {
    /* gctaOS object */
    gctaOS          os;

    /* Command manager. */
    gcTA_COMMAND    command;

    /* Parser. */
    gcTA_PARSER     parser;

    /* Hardware. */
    gcTA_HARDWARE   hardware;

    /* Page table. */
    gcTA_MMU        mmu;

    gctBOOL         mmuEnabled;
} gcsTA;


typedef struct _gcTA_HARDWARE {
    gcTA        ta;
    gctUINT32   chipFeatures;

    /* Supported minor feature fields. */
    gctUINT32   chipMinorFeatures;

    /* Supported minor feature 1 fields. */
    gctUINT32   chipMinorFeatures1;

    /* Supported minor feature 2 fields. */
    gctUINT32   chipMinorFeatures2;

    /* Supported minor feature 3 fields. */
    gctUINT32   chipMinorFeatures3;

    /*specs.*/
    gctUINT8    pixelPipes;

    gctUINT8    resolvePipes;
} gcsTA_HARDWARE;

/* Maps state locations within the context buffer. */
typedef struct _gcsSTATE_MAP * gcsSTATE_MAP_PTR;
typedef struct _gcsSTATE_MAP
{
    /* Index of the state in the context buffer. */
    gctUINT                     index;

    /* State mask. */
    gctUINT32                   mask;
}
gcsSTATE_MAP;

typedef struct _gcTA_PARSER {
    gcTA            ta;

    gctUINT8_PTR    currentCmdBufferAddr;

    /* Current command. */
    gctUINT32       lo;
    gctUINT32       hi;

    gctUINT8        cmdOpcode;
    gctUINT16       cmdAddr;
    gctUINT32       cmdSize;
    gctUINT32       cmdRectCount;
    gctUINT8        skip;
    gctUINT32       skipCount;

    gctBOOL         allow;
    gctBOOL         isFilterBlit;

    /* Borrow from kernel context buffer. */
    gctUINT32_PTR   buffer;
    gctUINT32       stateCount;
    gctUINT32       lastAddress;
    gctUINT32       lastFixed;
    gctUINT32       lastIndex;
    gctUINT32       lastSize;
    gctUINT32       totalSize;
    gcsSTATE_MAP_PTR map;
}
gcsTA_PARSER;

typedef enum {
    gcvTA_COMMAND,
    gcvTA_USER_COMMAND,
    gcvTA_CONTEXT
} gceTA_NODE_TYPE;

typedef struct _gcTA_NODE * gcTA_NODE;
typedef struct _gcTA_NODE {
    /* Type. */
    gceTA_NODE_TYPE type;

    /* Storage. */
    gctPOINTER logical;
    gctUINT32 physical;
    gctSIZE_T bytes;
} gcsTA_NODE;

gceSTATUS HALDECL
TAEmulator(
    void
    );

int
gcTA_Construct(
    IN gctaOS Os,
    OUT gcTA *TA
);

int
gcTA_Destroy(

);

int
gcTA_Dispatch(
    IN gcTA TA,
    IN OUT gcsTA_INTERFACE * Interface
);

/*************************************
* Porting layer
*/

gceSTATUS
gctaOS_ConstructOS(
    IN gckOS Os,
    OUT gctaOS *TAos
    );

gceSTATUS
gctaOS_Allocate(
    IN gctUINT32 Bytes,
    OUT gctPOINTER *Pointer
    );

gceSTATUS
gctaOS_Free(
    IN gctPOINTER Pointer
    );

gceSTATUS
gctaOS_AllocateSecurityMemory(
    IN gctaOS Os,
    IN gctSIZE_T *Bytes,
    OUT gctPOINTER *Logical,
    OUT gctUINT32 *Physical
    );

gceSTATUS
gctaOS_GetPhysicalAddress(
    IN gctaOS Os,
    IN gctPOINTER Logical,
    OUT gctUINT32 * Physical
    );

gceSTATUS
gctaOS_WriteRegister(
    IN gctaOS Os,
    IN gctUINT32 Address,
    IN gctUINT32 Data
    );

gceSTATUS
gctaOS_ReadRegister(
    IN gctaOS Os,
    IN gctUINT32 Address,
    IN gctUINT32 *Data
    );

gceSTATUS
gctaOS_MemCopy(
    IN gctUINT8_PTR Dest,
    IN gctUINT8_PTR Src,
    IN gctUINT32 Bytes
    );

gceSTATUS
gctaOS_ZeroMemory(
    IN gctUINT8_PTR Dest,
    IN gctUINT32 Bytes
    );

void
gctaOS_CacheFlush(
    IN gctUINT8_PTR Dest,
    IN gctUINT32 Bytes
    );

void
gctaOS_CacheClean(
    IN gctUINT8_PTR Dest,
    IN gctUINT32 Bytes
    );

void
gctaOS_CacheInvalidate(
    IN gctUINT8_PTR Dest,
    IN gctUINT32 Bytes
    );

gceSTATUS
gctaOS_IsPhysicalSecure(
    IN gctaOS Os,
    IN gctUINT32 Physical,
    OUT gctBOOL *Secure
    );

/**
** Hardware layer
**/
gceSTATUS
gctaHARDWARE_IsFeatureAvailable(
    IN gcTA_HARDWARE Hardware,
    IN gceFEATURE Feature
    );

gceSTATUS
gctaHARDWARE_Link(
    IN gcTA TA,
    IN gctPOINTER Logical,
    IN gctPOINTER FetchAddress,
    IN gctSIZE_T FetchSize,
    IN OUT gctSIZE_T * Bytes
    );

gceSTATUS
gctaHARDWARE_WaitLink(
    IN gcTA TA,
    IN gctPOINTER Logical,
    IN gctUINT32 Offset,
    IN OUT gctSIZE_T * Bytes,
    OUT gctUINT32 * WaitOffset
    );

gceSTATUS
gctaHARDWARE_Execute(
    IN gcTA TA,
    IN gctPOINTER Logical,
    IN gctSIZE_T Bytes
    );

gceSTATUS
gctaHARDWARE_Fence(
    gcTA TA,
    gctPOINTER Logical,
    gctPOINTER FenceAddress,
    gctUINT32 * FenceBytes
    );

gceSTATUS
gctaHARDWARE_VerifyCommand(
    IN gcTA TA,
    IN gctUINT32 *CommandBuffer,
    IN gctUINT32 Length,
    OUT gctBOOL  *Allow
    );

gceSTATUS
gctaHARDWARE_Construct(
    IN gcTA TA,
    OUT gcTA_HARDWARE * Hardware
    );

gceSTATUS
gctaHARDWARE_MMU(
    IN gcTA_HARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctUINT32 MtlbAddress,
    IN gctUINT32 SafeAddress,
    OUT gctUINT32 *Bytes
    );

gceSTATUS
gctaHARDWARE_MmuEnable(
    IN gcTA_HARDWARE Hardware
    );

gceSTATUS
gctaHARDWARE_End(
    IN gcTA_HARDWARE Hardware,
    IN gctPOINTER Logical,
    IN OUT gctUINT32 * Bytes
    );

gceSTATUS
gctaHARDWARE_Nop(
    IN gcTA_HARDWARE Hardware,
    IN gctPOINTER Logical,
    IN OUT gctSIZE_T * Bytes
    );

/*
*Command.
*/

gceSTATUS
gctaCOMMAND_Construct(
    IN gcTA TA,
    OUT gcTA_COMMAND *Command
    );

gceSTATUS
gctaCOMMAND_Destory(
    IN gcTA TA,
    OUT gcTA_COMMAND Command
    );

gceSTATUS
gctaCOMMAND_Start(
    IN gcTA_COMMAND Command
    );

gceSTATUS
gctaCOMMAND_Commit(
    IN gcTA_COMMAND Command,
    IN gctUINT8_PTR Buffer,
    IN gctUINT32 Bytes
    );


/*
* Parser
*/
gceSTATUS
gctaPARSER_Construct(
    IN gcTA TA,
    OUT gcTA_PARSER *Parser
    );

/*
* MMU.
*/
gceSTATUS
gctaMMU_Construct(
    IN gcTA TA,
    OUT gcTA_MMU *Mmu
    );

gceSTATUS
gctaMMU_FlatMapping(
    IN gcTA_MMU Mmu,
    IN gctUINT32 Physical
    );

gceSTATUS
gctaMMU_AllocatePages(
    IN gcTA_MMU Mmu,
    IN gctSIZE_T PageCount,
    OUT gctUINT32 * Address
    );

gceSTATUS
gctaMMU_SetPage(
    IN gcTA_MMU Mmu,
    IN gctUINT32 PageAddress,
    IN gctBOOL Secure,
    IN gctUINT32 *PageEntry
    );

gceSTATUS
gctaMMU_GetPageEntry(
    IN gcTA_MMU Mmu,
    IN gctUINT32 Address,
    IN gctUINT32_PTR *PageTable
    );

gceSTATUS
gctaMMU_FreePages(
    IN gcTA_MMU Mmu,
    IN gctUINT32 Address,
    IN gctSIZE_T PageCount
    );

gceSTATUS
gctaMMU_Enable(
    IN gcTA_MMU Mmu,
    IN gcTA TA
    );

gceSTATUS
gctaMMU_CheckHardwareAddressSecure(
    IN gcTA_MMU Mmu,
    IN gctUINT32 Address,
    IN gctBOOL *Secure
    );

#ifdef __cplusplus
}
#endif
#endif

