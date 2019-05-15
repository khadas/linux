/*
* Copyright (C) 2017 Amlogic, Inc. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*
* Description:
*/
#ifndef AVS_H_
#define AVS_H_

#ifdef CONFIG_AMLOGIC_AVSP_LONG_CABAC
#define AVSP_LONG_CABAC
#endif
/*#define BITSTREAM_READ_TMP_NO_CACHE*/

#ifdef AVSP_LONG_CABAC
#define MAX_CODED_FRAME_SIZE 1500000         /*!< bytes for one frame*/
#define LOCAL_HEAP_SIZE    (1024*1024*10)
/*
 *#define MAX_CODED_FRAME_SIZE  240000
 *#define MAX_CODED_FRAME_SIZE  700000
 */
#define SVA_STREAM_BUF_SIZE 1024

extern void *es_write_addr_virt;
extern dma_addr_t es_write_addr_phy;

extern void *bitstream_read_tmp;
extern dma_addr_t bitstream_read_tmp_phy;
extern void *avsp_heap_adr;

int avs_get_debug_flag(void);

int process_long_cabac(void);

/* bit [6] - skip_mode_flag
 * bit [5:4] - picture_type
 * bit [3] - picture_structure (0-Field, 1-Frame)
 * bit [2] - fixed_picture_qp
 * bit [1] - progressive_sequence
 * bit [0] - active
 */
#define LONG_CABAC_REQ        AV_SCRATCH_K
#define LONG_CABAC_SRC_ADDR   AV_SCRATCH_H
#define LONG_CABAC_DES_ADDR   AV_SCRATCH_I
/* bit[31:16] - vertical_size
 * bit[15:0] - horizontal_size
 */
#define LONG_CABAC_PIC_SIZE   AV_SCRATCH_J

#endif

/*
 *#define PERFORMANCE_DEBUG
 *#define DUMP_DEBUG
 */
#define AVS_DEBUG_PRINT         0x01
#define AVS_DEBUG_UCODE         0x02
#define AVS_DEBUG_OLD_ERROR_HANDLE	0x10
#define AVS_DEBUG_USE_FULL_SPEED 0x80
#define AEC_DUMP				0x100
#define STREAM_INFO_DUMP		0x200
#define SLICE_INFO_DUMP			0x400
#define MB_INFO_DUMP			0x800
#define MB_NUM_DUMP				0x1000
#define BLOCK_NUM_DUMP			0x2000
#define COEFF_DUMP				0x4000
#define ES_DUMP					0x8000
#define DQUANT_DUMP				0x10000
#define STREAM_INFO_DUMP_MORE   0x20000
#define STREAM_INFO_DUMP_MORE2  0x40000

extern void *es_write_addr_virt;
extern void *bitstream_read_tmp;
extern dma_addr_t bitstream_read_tmp_phy;
int read_bitstream(unsigned char *Buf, int size);
int u_v(int LenInBits, char *tracestring);

#endif
