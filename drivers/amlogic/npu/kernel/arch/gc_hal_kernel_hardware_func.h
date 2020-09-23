/****************************************************************************
*
*    The MIT License (MIT)
*
*    Copyright (c) 2014 - 2020 Vivante Corporation
*
*    Permission is hereby granted, free of charge, to any person obtaining a
*    copy of this software and associated documentation files (the "Software"),
*    to deal in the Software without restriction, including without limitation
*    the rights to use, copy, modify, merge, publish, distribute, sublicense,
*    and/or sell copies of the Software, and to permit persons to whom the
*    Software is furnished to do so, subject to the following conditions:
*
*    The above copyright notice and this permission notice shall be included in
*    all copies or substantial portions of the Software.
*
*    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
*    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
*    DEALINGS IN THE SOFTWARE.
*
*****************************************************************************
*
*    The GPL License (GPL)
*
*    Copyright (C) 2014 - 2020 Vivante Corporation
*
*    This program is free software; you can redistribute it and/or
*    modify it under the terms of the GNU General Public License
*    as published by the Free Software Foundation; either version 2
*    of the License, or (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program; if not, write to the Free Software Foundation,
*    Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*****************************************************************************
*
*    Note: This software is released under dual MIT and GPL licenses. A
*    recipient may use this file under the terms of either the MIT license or
*    GPL License. If you wish to use only one license not the other, you can
*    indicate your decision by deleting one of the above license notices in your
*    version of this file.
*
*****************************************************************************/


#ifndef __gc_hal_kernel_hardware_func_h_
#define __gc_hal_kernel_hardware_func_h_
#include "gc_hal.h"
#ifdef __cplusplus
extern "C" {
#endif

#define gcdINITIALIZE_PPU       0
#define gcdRESET_USC            0

typedef struct _gcsFUNCTION_EXECUTION * gcsFUNCTION_EXECUTION_PTR;

typedef enum {
    gcvFUNCTION_EXECUTION_MMU,
    gcvFUNCTION_EXECUTION_FLUSH,
#if gcdRESET_USC
    gcvFUNCTION_EXECUTION_RESET_USC,
    gcvFUNCTION_EXECUTION_RESET_USC2,
#endif
#if gcdINITIALIZE_PPU
    gcvFUNCTION_EXECUTION_INITIALIZE_PPU,
#endif

    gcvFUNCTION_EXECUTION_NUM
}
gceFUNCTION_EXECUTION;

typedef struct {
    gckVIDMEM_NODE      bufVidMem;
    gctUINT32           address;
    /* CPU address of the function. */
    gctPOINTER          logical;
}
gcsFUNCTION_EXECUTION_DATA, *gcsFUNCTION_EXECUTION_DATA_PTR;

typedef struct {
    gceSTATUS (*validate)(IN gcsFUNCTION_EXECUTION_PTR Execution);
    gceSTATUS (*init)(IN gcsFUNCTION_EXECUTION_PTR Execution);
    gceSTATUS (*execute)(IN gcsFUNCTION_EXECUTION_PTR Execution);
    gceSTATUS (*release)(IN gcsFUNCTION_EXECUTION_PTR Execution);
}
gcsFUNCTION_API, *gcsFUNCTION_API_PTR;


typedef struct _gcsFUNCTION_EXECUTION
{
    gctPOINTER                  hardware;

    /* Function name */
    gctCHAR                     funcName[16];

    /* Function ID */
    gceFUNCTION_EXECUTION       funcId;

    /* Function Vidmem object */
    gckVIDMEM_NODE              funcVidMem;

    /* Total bytes of the function. */
    gctSIZE_T                   funcVidMemBytes;

    /* Entry of the function. */
    gctUINT32                   address;

    /* CPU address of the function. */
    gctPOINTER                  logical;

    /* Actually bytes of the function. */
    gctUINT32                   bytes;

    /* Hardware address of END in this function. */
    gctUINT32                   endAddress;

    /* Logical of END in this function. */
    gctUINT8_PTR                endLogical;

    /* Function private data */
    gcsFUNCTION_EXECUTION_DATA_PTR data;

    /* API of functions */
    gcsFUNCTION_API funcExecution;

    /* Need this function or not */
    gctBOOL valid;

    /* Function has inited */
    gctBOOL inited;
}
gcsFUNCTION_EXECUTION;

gceSTATUS gckFUNCTION_Construct(IN         gctPOINTER Hardware);
gceSTATUS gckFUNCTION_Destory(IN    gctPOINTER Hardware);

gceSTATUS gckFUNCTION_Validate(IN gcsFUNCTION_EXECUTION_PTR Execution,
                                       IN OUT gctBOOL_PTR Valid);
gceSTATUS gckFUNCTION_Init(IN gcsFUNCTION_EXECUTION_PTR Execution);
gceSTATUS gckFUNCTION_Execute(IN gcsFUNCTION_EXECUTION_PTR Execution);
gceSTATUS gckFUNCTION_Release(IN gcsFUNCTION_EXECUTION_PTR Execution);
void gckFUNCTION_Dump(IN gcsFUNCTION_EXECUTION_PTR Execution);
#ifdef __cplusplus
}
#endif
#endif

