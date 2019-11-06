/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2011-2018 ARM or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#ifndef __ACAMERA_FPGA_CONFIG_H__
#define __ACAMERA_FPGA_CONFIG_H__


#include "system_sw_io.h"

#include "system_hw_io.h"

// ------------------------------------------------------------------------------ //
// Instance 'fpga' of module 'fpga_config_misc'
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_BASE_ADDR (0x209000L)
#define ACAMERA_FPGA_SIZE (0x1000)

// ------------------------------------------------------------------------------ //
// Group: FPGA
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// FPGA
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: gdc_axi_write_waddr_39_32
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  Higher part [39:32] of address bus write part of GDC out AXI
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_GDC_AXI_WRITE_WADDR_39_32_DEFAULT (0x00)
#define ACAMERA_FPGA_FPGA_GDC_AXI_WRITE_WADDR_39_32_DATASIZE (8)
#define ACAMERA_FPGA_FPGA_GDC_AXI_WRITE_WADDR_39_32_OFFSET (0x4)
#define ACAMERA_FPGA_FPGA_GDC_AXI_WRITE_WADDR_39_32_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_fpga_gdc_axi_write_waddr_39_32_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209004L);
    system_hw_write_32(0x209004L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_fpga_gdc_axi_write_waddr_39_32_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209004L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: gdc_axi_read_raddr_39_32
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  Higher part [39:32] of address bus read part of GDC out AXI
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_GDC_AXI_READ_RADDR_39_32_DEFAULT (0x00)
#define ACAMERA_FPGA_FPGA_GDC_AXI_READ_RADDR_39_32_DATASIZE (8)
#define ACAMERA_FPGA_FPGA_GDC_AXI_READ_RADDR_39_32_OFFSET (0x4)
#define ACAMERA_FPGA_FPGA_GDC_AXI_READ_RADDR_39_32_MASK (0xff00)

// args: data (8-bit)
static __inline void acamera_fpga_fpga_gdc_axi_read_raddr_39_32_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209004L);
    system_hw_write_32(0x209004L, (((uint32_t) (data & 0xff)) << 8) | (curr & 0xffff00ff));
}
static __inline uint8_t acamera_fpga_fpga_gdc_axi_read_raddr_39_32_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209004L) & 0xff00) >> 8);
}
// ------------------------------------------------------------------------------ //
// Register: frame_reader_raddr_39_32
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  Higher part [39:32] of address bus read part of FPGA Frame Reader AXI
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_FRAME_READER_RADDR_39_32_DEFAULT (0x00)
#define ACAMERA_FPGA_FPGA_FRAME_READER_RADDR_39_32_DATASIZE (8)
#define ACAMERA_FPGA_FPGA_FRAME_READER_RADDR_39_32_OFFSET (0x8)
#define ACAMERA_FPGA_FPGA_FRAME_READER_RADDR_39_32_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_fpga_frame_reader_raddr_39_32_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209008L);
    system_hw_write_32(0x209008L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_fpga_frame_reader_raddr_39_32_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209008L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: dma_wdr_frame_buffer1_highaddr
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_DMA_WDR_FRAME_BUFFER1_HIGHADDR_DEFAULT (0x00)
#define ACAMERA_FPGA_FPGA_DMA_WDR_FRAME_BUFFER1_HIGHADDR_DATASIZE (8)
#define ACAMERA_FPGA_FPGA_DMA_WDR_FRAME_BUFFER1_HIGHADDR_OFFSET (0xc)
#define ACAMERA_FPGA_FPGA_DMA_WDR_FRAME_BUFFER1_HIGHADDR_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_fpga_dma_wdr_frame_buffer1_highaddr_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20900cL);
    system_hw_write_32(0x20900cL, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_fpga_dma_wdr_frame_buffer1_highaddr_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20900cL) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: dma_writer1_highaddr
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_DMA_WRITER1_HIGHADDR_DEFAULT (0x00)
#define ACAMERA_FPGA_FPGA_DMA_WRITER1_HIGHADDR_DATASIZE (8)
#define ACAMERA_FPGA_FPGA_DMA_WRITER1_HIGHADDR_OFFSET (0x10)
#define ACAMERA_FPGA_FPGA_DMA_WRITER1_HIGHADDR_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_fpga_dma_writer1_highaddr_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209010L);
    system_hw_write_32(0x209010L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_fpga_dma_writer1_highaddr_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209010L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: dma_writer2_highaddr
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_DMA_WRITER2_HIGHADDR_DEFAULT (0x00)
#define ACAMERA_FPGA_FPGA_DMA_WRITER2_HIGHADDR_DATASIZE (8)
#define ACAMERA_FPGA_FPGA_DMA_WRITER2_HIGHADDR_OFFSET (0x14)
#define ACAMERA_FPGA_FPGA_DMA_WRITER2_HIGHADDR_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_fpga_dma_writer2_highaddr_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209014L);
    system_hw_write_32(0x209014L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_fpga_dma_writer2_highaddr_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209014L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: dma_writer3_highaddr
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_DMA_WRITER3_HIGHADDR_DEFAULT (0x00)
#define ACAMERA_FPGA_FPGA_DMA_WRITER3_HIGHADDR_DATASIZE (8)
#define ACAMERA_FPGA_FPGA_DMA_WRITER3_HIGHADDR_OFFSET (0x18)
#define ACAMERA_FPGA_FPGA_DMA_WRITER3_HIGHADDR_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_fpga_dma_writer3_highaddr_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209018L);
    system_hw_write_32(0x209018L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_fpga_dma_writer3_highaddr_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209018L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: dma_writer4_highaddr
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_DMA_WRITER4_HIGHADDR_DEFAULT (0x00)
#define ACAMERA_FPGA_FPGA_DMA_WRITER4_HIGHADDR_DATASIZE (8)
#define ACAMERA_FPGA_FPGA_DMA_WRITER4_HIGHADDR_OFFSET (0x1c)
#define ACAMERA_FPGA_FPGA_DMA_WRITER4_HIGHADDR_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_fpga_dma_writer4_highaddr_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20901cL);
    system_hw_write_32(0x20901cL, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_fpga_dma_writer4_highaddr_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20901cL) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: dma_input_highaddr
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_DMA_INPUT_HIGHADDR_DEFAULT (0x00)
#define ACAMERA_FPGA_FPGA_DMA_INPUT_HIGHADDR_DATASIZE (8)
#define ACAMERA_FPGA_FPGA_DMA_INPUT_HIGHADDR_OFFSET (0x20)
#define ACAMERA_FPGA_FPGA_DMA_INPUT_HIGHADDR_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_fpga_dma_input_highaddr_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209020L);
    system_hw_write_32(0x209020L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_fpga_dma_input_highaddr_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209020L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: dma_writer_fr_highaddr
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_DMA_WRITER_FR_HIGHADDR_DEFAULT (0x00)
#define ACAMERA_FPGA_FPGA_DMA_WRITER_FR_HIGHADDR_DATASIZE (8)
#define ACAMERA_FPGA_FPGA_DMA_WRITER_FR_HIGHADDR_OFFSET (0x24)
#define ACAMERA_FPGA_FPGA_DMA_WRITER_FR_HIGHADDR_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_fpga_dma_writer_fr_highaddr_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209024L);
    system_hw_write_32(0x209024L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_fpga_dma_writer_fr_highaddr_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209024L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: dma_writer_fr_uv_highaddr
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_DMA_WRITER_FR_UV_HIGHADDR_DEFAULT (0x00)
#define ACAMERA_FPGA_FPGA_DMA_WRITER_FR_UV_HIGHADDR_DATASIZE (8)
#define ACAMERA_FPGA_FPGA_DMA_WRITER_FR_UV_HIGHADDR_OFFSET (0x28)
#define ACAMERA_FPGA_FPGA_DMA_WRITER_FR_UV_HIGHADDR_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_fpga_dma_writer_fr_uv_highaddr_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209028L);
    system_hw_write_32(0x209028L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_fpga_dma_writer_fr_uv_highaddr_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209028L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: dma_writer_capture1_highaddr
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_DMA_WRITER_CAPTURE1_HIGHADDR_DEFAULT (0x00)
#define ACAMERA_FPGA_FPGA_DMA_WRITER_CAPTURE1_HIGHADDR_DATASIZE (8)
#define ACAMERA_FPGA_FPGA_DMA_WRITER_CAPTURE1_HIGHADDR_OFFSET (0x2c)
#define ACAMERA_FPGA_FPGA_DMA_WRITER_CAPTURE1_HIGHADDR_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_fpga_dma_writer_capture1_highaddr_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20902cL);
    system_hw_write_32(0x20902cL, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_fpga_dma_writer_capture1_highaddr_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20902cL) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: dma_writer_capture2_highaddr
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_DMA_WRITER_CAPTURE2_HIGHADDR_DEFAULT (0x00)
#define ACAMERA_FPGA_FPGA_DMA_WRITER_CAPTURE2_HIGHADDR_DATASIZE (8)
#define ACAMERA_FPGA_FPGA_DMA_WRITER_CAPTURE2_HIGHADDR_OFFSET (0x30)
#define ACAMERA_FPGA_FPGA_DMA_WRITER_CAPTURE2_HIGHADDR_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_fpga_dma_writer_capture2_highaddr_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209030L);
    system_hw_write_32(0x209030L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_fpga_dma_writer_capture2_highaddr_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209030L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: dma_writer_capture3_highaddr
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_DMA_WRITER_CAPTURE3_HIGHADDR_DEFAULT (0x00)
#define ACAMERA_FPGA_FPGA_DMA_WRITER_CAPTURE3_HIGHADDR_DATASIZE (8)
#define ACAMERA_FPGA_FPGA_DMA_WRITER_CAPTURE3_HIGHADDR_OFFSET (0x34)
#define ACAMERA_FPGA_FPGA_DMA_WRITER_CAPTURE3_HIGHADDR_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_fpga_dma_writer_capture3_highaddr_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209034L);
    system_hw_write_32(0x209034L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_fpga_dma_writer_capture3_highaddr_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209034L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Stereo enable
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Enable dual stream operation
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_STEREO_ENABLE_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_STEREO_ENABLE_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_STEREO_ENABLE_OFFSET (0x38)
#define ACAMERA_FPGA_FPGA_STEREO_ENABLE_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_stereo_enable_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209038L);
    system_hw_write_32(0x209038L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_fpga_stereo_enable_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209038L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Global FSM reset
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Global reset of FSMs
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_GLOBAL_FSM_RESET_DEFAULT (0x0)
#define ACAMERA_FPGA_FPGA_GLOBAL_FSM_RESET_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_GLOBAL_FSM_RESET_OFFSET (0x3c)
#define ACAMERA_FPGA_FPGA_GLOBAL_FSM_RESET_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_global_fsm_reset_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20903cL);
    system_hw_write_32(0x20903cL, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_fpga_global_fsm_reset_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20903cL) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Group: Frame Reader
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: format
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FRAME_READER_FORMAT_DEFAULT (0)
#define ACAMERA_FPGA_FRAME_READER_FORMAT_DATASIZE (8)
#define ACAMERA_FPGA_FRAME_READER_FORMAT_OFFSET (0x40)
#define ACAMERA_FPGA_FRAME_READER_FORMAT_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_frame_reader_format_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209040L);
    system_hw_write_32(0x209040L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_frame_reader_format_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209040L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rbase load
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FRAME_READER_RBASE_LOAD_DEFAULT (0x0)
#define ACAMERA_FPGA_FRAME_READER_RBASE_LOAD_DATASIZE (1)
#define ACAMERA_FPGA_FRAME_READER_RBASE_LOAD_OFFSET (0x4c)
#define ACAMERA_FPGA_FRAME_READER_RBASE_LOAD_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_frame_reader_rbase_load_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20904cL);
    system_hw_write_32(0x20904cL, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_frame_reader_rbase_load_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20904cL) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rbase load sel
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Selector for rbase_load strobe: 0-field, 1-configuration bit rbase_load
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FRAME_READER_RBASE_LOAD_SEL_DEFAULT (0x0)
#define ACAMERA_FPGA_FRAME_READER_RBASE_LOAD_SEL_DATASIZE (1)
#define ACAMERA_FPGA_FRAME_READER_RBASE_LOAD_SEL_OFFSET (0x4c)
#define ACAMERA_FPGA_FRAME_READER_RBASE_LOAD_SEL_MASK (0x10)

// args: data (1-bit)
static __inline void acamera_fpga_frame_reader_rbase_load_sel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20904cL);
    system_hw_write_32(0x20904cL, (((uint32_t) (data & 0x1)) << 4) | (curr & 0xffffffef));
}
static __inline uint8_t acamera_fpga_frame_reader_rbase_load_sel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20904cL) & 0x10) >> 4);
}
// ------------------------------------------------------------------------------ //
// Register: rbase
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Base address for frame buffer, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FRAME_READER_RBASE_DEFAULT (0x0)
#define ACAMERA_FPGA_FRAME_READER_RBASE_DATASIZE (32)
#define ACAMERA_FPGA_FRAME_READER_RBASE_OFFSET (0x50)
#define ACAMERA_FPGA_FRAME_READER_RBASE_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_frame_reader_rbase_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x209050L, data);
}
static __inline uint32_t acamera_fpga_frame_reader_rbase_read(uintptr_t base) {
    return system_hw_read_32(0x209050L);
}
// ------------------------------------------------------------------------------ //
// Register: Line_offset
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Indicates offset in bytes from the start of one line to the next line. Should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FRAME_READER_LINE_OFFSET_DEFAULT (0x1000)
#define ACAMERA_FPGA_FRAME_READER_LINE_OFFSET_DATASIZE (32)
#define ACAMERA_FPGA_FRAME_READER_LINE_OFFSET_OFFSET (0x54)
#define ACAMERA_FPGA_FRAME_READER_LINE_OFFSET_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_frame_reader_line_offset_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x209054L, data);
}
static __inline uint32_t acamera_fpga_frame_reader_line_offset_read(uintptr_t base) {
    return system_hw_read_32(0x209054L);
}
// ------------------------------------------------------------------------------ //
// Register: axi_port_enable
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FRAME_READER_AXI_PORT_ENABLE_DEFAULT (0x0)
#define ACAMERA_FPGA_FRAME_READER_AXI_PORT_ENABLE_DATASIZE (1)
#define ACAMERA_FPGA_FRAME_READER_AXI_PORT_ENABLE_OFFSET (0x58)
#define ACAMERA_FPGA_FRAME_READER_AXI_PORT_ENABLE_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_frame_reader_axi_port_enable_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209058L);
    system_hw_write_32(0x209058L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_frame_reader_axi_port_enable_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209058L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: config
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FRAME_READER_CONFIG_DEFAULT (0x0000)
#define ACAMERA_FPGA_FRAME_READER_CONFIG_DATASIZE (32)
#define ACAMERA_FPGA_FRAME_READER_CONFIG_OFFSET (0x60)
#define ACAMERA_FPGA_FRAME_READER_CONFIG_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_frame_reader_config_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x209060L, data);
}
static __inline uint32_t acamera_fpga_frame_reader_config_read(uintptr_t base) {
    return system_hw_read_32(0x209060L);
}
// ------------------------------------------------------------------------------ //
// Register: status
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FRAME_READER_STATUS_DEFAULT (0x0000)
#define ACAMERA_FPGA_FRAME_READER_STATUS_DATASIZE (32)
#define ACAMERA_FPGA_FRAME_READER_STATUS_OFFSET (0x64)
#define ACAMERA_FPGA_FRAME_READER_STATUS_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_frame_reader_status_read(uintptr_t base) {
    return system_hw_read_32(0x209064L);
}
// ------------------------------------------------------------------------------ //
// Register: active_width
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FRAME_READER_ACTIVE_WIDTH_DEFAULT (0x0780)
#define ACAMERA_FPGA_FRAME_READER_ACTIVE_WIDTH_DATASIZE (16)
#define ACAMERA_FPGA_FRAME_READER_ACTIVE_WIDTH_OFFSET (0x68)
#define ACAMERA_FPGA_FRAME_READER_ACTIVE_WIDTH_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_frame_reader_active_width_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209068L);
    system_hw_write_32(0x209068L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_frame_reader_active_width_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209068L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: active_height
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FRAME_READER_ACTIVE_HEIGHT_DEFAULT (0x0438)
#define ACAMERA_FPGA_FRAME_READER_ACTIVE_HEIGHT_DATASIZE (16)
#define ACAMERA_FPGA_FRAME_READER_ACTIVE_HEIGHT_OFFSET (0x6c)
#define ACAMERA_FPGA_FRAME_READER_ACTIVE_HEIGHT_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_frame_reader_active_height_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20906cL);
    system_hw_write_32(0x20906cL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_frame_reader_active_height_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20906cL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Group: Frame Reader UV
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: format
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FRAME_READER_UV_FORMAT_DEFAULT (0x0)
#define ACAMERA_FPGA_FRAME_READER_UV_FORMAT_DATASIZE (8)
#define ACAMERA_FPGA_FRAME_READER_UV_FORMAT_OFFSET (0x80)
#define ACAMERA_FPGA_FRAME_READER_UV_FORMAT_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_frame_reader_uv_format_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209080L);
    system_hw_write_32(0x209080L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_frame_reader_uv_format_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209080L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: disable422_uv_interleave
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FRAME_READER_UV_DISABLE422_UV_INTERLEAVE_DEFAULT (0x0)
#define ACAMERA_FPGA_FRAME_READER_UV_DISABLE422_UV_INTERLEAVE_DATASIZE (1)
#define ACAMERA_FPGA_FRAME_READER_UV_DISABLE422_UV_INTERLEAVE_OFFSET (0x8c)
#define ACAMERA_FPGA_FRAME_READER_UV_DISABLE422_UV_INTERLEAVE_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_fpga_frame_reader_uv_disable422_uv_interleave_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20908cL);
    system_hw_write_32(0x20908cL, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_frame_reader_uv_disable422_uv_interleave_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20908cL) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: repeat_downsampled_lines
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FRAME_READER_UV_REPEAT_DOWNSAMPLED_LINES_DEFAULT (0x0)
#define ACAMERA_FPGA_FRAME_READER_UV_REPEAT_DOWNSAMPLED_LINES_DATASIZE (1)
#define ACAMERA_FPGA_FRAME_READER_UV_REPEAT_DOWNSAMPLED_LINES_OFFSET (0x8c)
#define ACAMERA_FPGA_FRAME_READER_UV_REPEAT_DOWNSAMPLED_LINES_MASK (0x4)

// args: data (1-bit)
static __inline void acamera_fpga_frame_reader_uv_repeat_downsampled_lines_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20908cL);
    system_hw_write_32(0x20908cL, (((uint32_t) (data & 0x1)) << 2) | (curr & 0xfffffffb));
}
static __inline uint8_t acamera_fpga_frame_reader_uv_repeat_downsampled_lines_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20908cL) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: repeat_downsampled_pixels
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FRAME_READER_UV_REPEAT_DOWNSAMPLED_PIXELS_DEFAULT (0x0)
#define ACAMERA_FPGA_FRAME_READER_UV_REPEAT_DOWNSAMPLED_PIXELS_DATASIZE (1)
#define ACAMERA_FPGA_FRAME_READER_UV_REPEAT_DOWNSAMPLED_PIXELS_OFFSET (0x8c)
#define ACAMERA_FPGA_FRAME_READER_UV_REPEAT_DOWNSAMPLED_PIXELS_MASK (0x8)

// args: data (1-bit)
static __inline void acamera_fpga_frame_reader_uv_repeat_downsampled_pixels_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20908cL);
    system_hw_write_32(0x20908cL, (((uint32_t) (data & 0x1)) << 3) | (curr & 0xfffffff7));
}
static __inline uint8_t acamera_fpga_frame_reader_uv_repeat_downsampled_pixels_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20908cL) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: rbase load
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FRAME_READER_UV_RBASE_LOAD_DEFAULT (0x0)
#define ACAMERA_FPGA_FRAME_READER_UV_RBASE_LOAD_DATASIZE (1)
#define ACAMERA_FPGA_FRAME_READER_UV_RBASE_LOAD_OFFSET (0x90)
#define ACAMERA_FPGA_FRAME_READER_UV_RBASE_LOAD_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_frame_reader_uv_rbase_load_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209090L);
    system_hw_write_32(0x209090L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_frame_reader_uv_rbase_load_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209090L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rbase load sel
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Selector for rbase_load strobe: 0-field, 1-configuration bit rbase_load
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FRAME_READER_UV_RBASE_LOAD_SEL_DEFAULT (0x0)
#define ACAMERA_FPGA_FRAME_READER_UV_RBASE_LOAD_SEL_DATASIZE (1)
#define ACAMERA_FPGA_FRAME_READER_UV_RBASE_LOAD_SEL_OFFSET (0x90)
#define ACAMERA_FPGA_FRAME_READER_UV_RBASE_LOAD_SEL_MASK (0x10)

// args: data (1-bit)
static __inline void acamera_fpga_frame_reader_uv_rbase_load_sel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209090L);
    system_hw_write_32(0x209090L, (((uint32_t) (data & 0x1)) << 4) | (curr & 0xffffffef));
}
static __inline uint8_t acamera_fpga_frame_reader_uv_rbase_load_sel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209090L) & 0x10) >> 4);
}
// ------------------------------------------------------------------------------ //
// Register: rbase
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Base address for frame buffer, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FRAME_READER_UV_RBASE_DEFAULT (0x0)
#define ACAMERA_FPGA_FRAME_READER_UV_RBASE_DATASIZE (32)
#define ACAMERA_FPGA_FRAME_READER_UV_RBASE_OFFSET (0x94)
#define ACAMERA_FPGA_FRAME_READER_UV_RBASE_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_frame_reader_uv_rbase_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x209094L, data);
}
static __inline uint32_t acamera_fpga_frame_reader_uv_rbase_read(uintptr_t base) {
    return system_hw_read_32(0x209094L);
}
// ------------------------------------------------------------------------------ //
// Register: Line_offset
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Indicates offset in bytes from the start of one line to the next line. Should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FRAME_READER_UV_LINE_OFFSET_DEFAULT (0x1000)
#define ACAMERA_FPGA_FRAME_READER_UV_LINE_OFFSET_DATASIZE (32)
#define ACAMERA_FPGA_FRAME_READER_UV_LINE_OFFSET_OFFSET (0x98)
#define ACAMERA_FPGA_FRAME_READER_UV_LINE_OFFSET_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_frame_reader_uv_line_offset_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x209098L, data);
}
static __inline uint32_t acamera_fpga_frame_reader_uv_line_offset_read(uintptr_t base) {
    return system_hw_read_32(0x209098L);
}
// ------------------------------------------------------------------------------ //
// Register: axi_port_enable
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FRAME_READER_UV_AXI_PORT_ENABLE_DEFAULT (0x0)
#define ACAMERA_FPGA_FRAME_READER_UV_AXI_PORT_ENABLE_DATASIZE (1)
#define ACAMERA_FPGA_FRAME_READER_UV_AXI_PORT_ENABLE_OFFSET (0x9c)
#define ACAMERA_FPGA_FRAME_READER_UV_AXI_PORT_ENABLE_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_frame_reader_uv_axi_port_enable_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20909cL);
    system_hw_write_32(0x20909cL, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_frame_reader_uv_axi_port_enable_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20909cL) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: config
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FRAME_READER_UV_CONFIG_DEFAULT (0x0000)
#define ACAMERA_FPGA_FRAME_READER_UV_CONFIG_DATASIZE (32)
#define ACAMERA_FPGA_FRAME_READER_UV_CONFIG_OFFSET (0xa0)
#define ACAMERA_FPGA_FRAME_READER_UV_CONFIG_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_frame_reader_uv_config_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x2090a0L, data);
}
static __inline uint32_t acamera_fpga_frame_reader_uv_config_read(uintptr_t base) {
    return system_hw_read_32(0x2090a0L);
}
// ------------------------------------------------------------------------------ //
// Register: status
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FRAME_READER_UV_STATUS_DEFAULT (0x0000)
#define ACAMERA_FPGA_FRAME_READER_UV_STATUS_DATASIZE (32)
#define ACAMERA_FPGA_FRAME_READER_UV_STATUS_OFFSET (0xa4)
#define ACAMERA_FPGA_FRAME_READER_UV_STATUS_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_frame_reader_uv_status_read(uintptr_t base) {
    return system_hw_read_32(0x2090a4L);
}
// ------------------------------------------------------------------------------ //
// Register: active_width
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FRAME_READER_UV_ACTIVE_WIDTH_DEFAULT (0x0780)
#define ACAMERA_FPGA_FRAME_READER_UV_ACTIVE_WIDTH_DATASIZE (16)
#define ACAMERA_FPGA_FRAME_READER_UV_ACTIVE_WIDTH_OFFSET (0xa8)
#define ACAMERA_FPGA_FRAME_READER_UV_ACTIVE_WIDTH_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_frame_reader_uv_active_width_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2090a8L);
    system_hw_write_32(0x2090a8L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_frame_reader_uv_active_width_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2090a8L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: active_height
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FRAME_READER_UV_ACTIVE_HEIGHT_DEFAULT (0x0438)
#define ACAMERA_FPGA_FRAME_READER_UV_ACTIVE_HEIGHT_DATASIZE (16)
#define ACAMERA_FPGA_FRAME_READER_UV_ACTIVE_HEIGHT_OFFSET (0xac)
#define ACAMERA_FPGA_FRAME_READER_UV_ACTIVE_HEIGHT_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_frame_reader_uv_active_height_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2090acL);
    system_hw_write_32(0x2090acL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_frame_reader_uv_active_height_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2090acL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Group: Horizontal Shift
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: Offset
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// :Pixel resolution shift offset
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_HORIZONTAL_SHIFT_OFFSET_DEFAULT (0x0)
#define ACAMERA_FPGA_HORIZONTAL_SHIFT_OFFSET_DATASIZE (5)
#define ACAMERA_FPGA_HORIZONTAL_SHIFT_OFFSET_OFFSET (0x120)
#define ACAMERA_FPGA_HORIZONTAL_SHIFT_OFFSET_MASK (0x1f)

// args: data (5-bit)
static __inline void acamera_fpga_horizontal_shift_offset_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209120L);
    system_hw_write_32(0x209120L, (((uint32_t) (data & 0x1f)) << 0) | (curr & 0xffffffe0));
}
static __inline uint8_t acamera_fpga_horizontal_shift_offset_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209120L) & 0x1f) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Enable
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_HORIZONTAL_SHIFT_ENABLE_DEFAULT (0x0)
#define ACAMERA_FPGA_HORIZONTAL_SHIFT_ENABLE_DATASIZE (1)
#define ACAMERA_FPGA_HORIZONTAL_SHIFT_ENABLE_OFFSET (0x120)
#define ACAMERA_FPGA_HORIZONTAL_SHIFT_ENABLE_MASK (0x10000)

// args: data (1-bit)
static __inline void acamera_fpga_horizontal_shift_enable_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209120L);
    system_hw_write_32(0x209120L, (((uint32_t) (data & 0x1)) << 16) | (curr & 0xfffeffff));
}
static __inline uint8_t acamera_fpga_horizontal_shift_enable_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209120L) & 0x10000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Group: CS YR
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Conversion of RGB to YUV data using a 3x3 color matrix plus offsets
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: Enable matrix
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Color matrix enable: 0=off 1=on
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_ENABLE_MATRIX_DEFAULT (1)
#define ACAMERA_FPGA_YR_CS_CONV_ENABLE_MATRIX_DATASIZE (1)
#define ACAMERA_FPGA_YR_CS_CONV_ENABLE_MATRIX_OFFSET (0x230)
#define ACAMERA_FPGA_YR_CS_CONV_ENABLE_MATRIX_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_yr_cs_conv_enable_matrix_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209230L);
    system_hw_write_32(0x209230L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_yr_cs_conv_enable_matrix_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209230L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Enable Horizontal downsample
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Downsample enable: 0=off 1=on
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_ENABLE_HORIZONTAL_DOWNSAMPLE_DEFAULT (0)
#define ACAMERA_FPGA_YR_CS_CONV_ENABLE_HORIZONTAL_DOWNSAMPLE_DATASIZE (1)
#define ACAMERA_FPGA_YR_CS_CONV_ENABLE_HORIZONTAL_DOWNSAMPLE_OFFSET (0x230)
#define ACAMERA_FPGA_YR_CS_CONV_ENABLE_HORIZONTAL_DOWNSAMPLE_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_fpga_yr_cs_conv_enable_horizontal_downsample_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209230L);
    system_hw_write_32(0x209230L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_yr_cs_conv_enable_horizontal_downsample_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209230L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: Enable Vertical downsample
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Downsample enable: 0=off 1=on
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_ENABLE_VERTICAL_DOWNSAMPLE_DEFAULT (0)
#define ACAMERA_FPGA_YR_CS_CONV_ENABLE_VERTICAL_DOWNSAMPLE_DATASIZE (1)
#define ACAMERA_FPGA_YR_CS_CONV_ENABLE_VERTICAL_DOWNSAMPLE_OFFSET (0x230)
#define ACAMERA_FPGA_YR_CS_CONV_ENABLE_VERTICAL_DOWNSAMPLE_MASK (0x4)

// args: data (1-bit)
static __inline void acamera_fpga_yr_cs_conv_enable_vertical_downsample_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209230L);
    system_hw_write_32(0x209230L, (((uint32_t) (data & 0x1)) << 2) | (curr & 0xfffffffb));
}
static __inline uint8_t acamera_fpga_yr_cs_conv_enable_vertical_downsample_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209230L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: Enable filter
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Downsample enable: 0=off 1=on
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_ENABLE_FILTER_DEFAULT (0)
#define ACAMERA_FPGA_YR_CS_CONV_ENABLE_FILTER_DATASIZE (1)
#define ACAMERA_FPGA_YR_CS_CONV_ENABLE_FILTER_OFFSET (0x230)
#define ACAMERA_FPGA_YR_CS_CONV_ENABLE_FILTER_MASK (0x8)

// args: data (1-bit)
static __inline void acamera_fpga_yr_cs_conv_enable_filter_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209230L);
    system_hw_write_32(0x209230L, (((uint32_t) (data & 0x1)) << 3) | (curr & 0xfffffff7));
}
static __inline uint8_t acamera_fpga_yr_cs_conv_enable_filter_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209230L) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: Coefft 11
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Matrix coefficient for R-Y multiplier
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_11_DEFAULT (0x012A)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_11_DATASIZE (16)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_11_OFFSET (0x200)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_11_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_yr_cs_conv_coefft_11_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209200L);
    system_hw_write_32(0x209200L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_yr_cs_conv_coefft_11_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209200L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Coefft 12
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Matrix coefficient for G-Y multiplier
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_12_DEFAULT (0x0000)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_12_DATASIZE (16)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_12_OFFSET (0x204)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_12_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_yr_cs_conv_coefft_12_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209204L);
    system_hw_write_32(0x209204L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_yr_cs_conv_coefft_12_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209204L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Coefft 13
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Matrix coefficient for B-Y multiplier
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_13_DEFAULT (0x01D2)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_13_DATASIZE (16)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_13_OFFSET (0x208)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_13_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_yr_cs_conv_coefft_13_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209208L);
    system_hw_write_32(0x209208L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_yr_cs_conv_coefft_13_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209208L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Coefft 21
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Matrix coefficient for R-Cb multiplier
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_21_DEFAULT (0x012A)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_21_DATASIZE (16)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_21_OFFSET (0x20c)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_21_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_yr_cs_conv_coefft_21_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20920cL);
    system_hw_write_32(0x20920cL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_yr_cs_conv_coefft_21_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20920cL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Coefft 22
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Matrix coefficient for G-Cb multiplier
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_22_DEFAULT (0x0000)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_22_DATASIZE (16)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_22_OFFSET (0x210)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_22_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_yr_cs_conv_coefft_22_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209210L);
    system_hw_write_32(0x209210L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_yr_cs_conv_coefft_22_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209210L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Coefft 23
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Matrix coefficient for B-Cb multiplier
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_23_DEFAULT (0x8088)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_23_DATASIZE (16)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_23_OFFSET (0x214)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_23_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_yr_cs_conv_coefft_23_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209214L);
    system_hw_write_32(0x209214L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_yr_cs_conv_coefft_23_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209214L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Coefft 31
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Matrix coefficient for R-Cr multiplier
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_31_DEFAULT (0x012A)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_31_DATASIZE (16)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_31_OFFSET (0x218)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_31_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_yr_cs_conv_coefft_31_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209218L);
    system_hw_write_32(0x209218L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_yr_cs_conv_coefft_31_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209218L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Coefft 32
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Matrix coefficient for G-Cr multiplier
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_32_DEFAULT (0x021D)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_32_DATASIZE (16)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_32_OFFSET (0x21c)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_32_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_yr_cs_conv_coefft_32_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20921cL);
    system_hw_write_32(0x20921cL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_yr_cs_conv_coefft_32_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20921cL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Coefft 33
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Matrix coefficient for B-Cr multiplier
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_33_DEFAULT (0x0000)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_33_DATASIZE (16)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_33_OFFSET (0x220)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_33_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_yr_cs_conv_coefft_33_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209220L);
    system_hw_write_32(0x209220L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_yr_cs_conv_coefft_33_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209220L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Coefft o1
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Offset for Y
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_O1_DEFAULT (0x420)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_O1_DATASIZE (11)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_O1_OFFSET (0x224)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_O1_MASK (0x7ff)

// args: data (11-bit)
static __inline void acamera_fpga_yr_cs_conv_coefft_o1_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209224L);
    system_hw_write_32(0x209224L, (((uint32_t) (data & 0x7ff)) << 0) | (curr & 0xfffff800));
}
static __inline uint16_t acamera_fpga_yr_cs_conv_coefft_o1_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209224L) & 0x7ff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Coefft o2
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Offset for Cb
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_O2_DEFAULT (0x134)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_O2_DATASIZE (11)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_O2_OFFSET (0x228)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_O2_MASK (0x7ff)

// args: data (11-bit)
static __inline void acamera_fpga_yr_cs_conv_coefft_o2_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209228L);
    system_hw_write_32(0x209228L, (((uint32_t) (data & 0x7ff)) << 0) | (curr & 0xfffff800));
}
static __inline uint16_t acamera_fpga_yr_cs_conv_coefft_o2_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209228L) & 0x7ff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Coefft o3
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Offset for Cr
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_O3_DEFAULT (0x401)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_O3_DATASIZE (11)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_O3_OFFSET (0x22c)
#define ACAMERA_FPGA_YR_CS_CONV_COEFFT_O3_MASK (0x7ff)

// args: data (11-bit)
static __inline void acamera_fpga_yr_cs_conv_coefft_o3_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20922cL);
    system_hw_write_32(0x20922cL, (((uint32_t) (data & 0x7ff)) << 0) | (curr & 0xfffff800));
}
static __inline uint16_t acamera_fpga_yr_cs_conv_coefft_o3_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20922cL) & 0x7ff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Clip min Y
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Minimal value for Y.  Values below this are clipped.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_CLIP_MIN_Y_DEFAULT (0x000)
#define ACAMERA_FPGA_YR_CS_CONV_CLIP_MIN_Y_DATASIZE (10)
#define ACAMERA_FPGA_YR_CS_CONV_CLIP_MIN_Y_OFFSET (0x238)
#define ACAMERA_FPGA_YR_CS_CONV_CLIP_MIN_Y_MASK (0x3ff)

// args: data (10-bit)
static __inline void acamera_fpga_yr_cs_conv_clip_min_y_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209238L);
    system_hw_write_32(0x209238L, (((uint32_t) (data & 0x3ff)) << 0) | (curr & 0xfffffc00));
}
static __inline uint16_t acamera_fpga_yr_cs_conv_clip_min_y_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209238L) & 0x3ff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Clip max Y
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Maximal value for Y.  Values above this are clipped.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_CLIP_MAX_Y_DEFAULT (0x3FF)
#define ACAMERA_FPGA_YR_CS_CONV_CLIP_MAX_Y_DATASIZE (10)
#define ACAMERA_FPGA_YR_CS_CONV_CLIP_MAX_Y_OFFSET (0x238)
#define ACAMERA_FPGA_YR_CS_CONV_CLIP_MAX_Y_MASK (0x3ff0000)

// args: data (10-bit)
static __inline void acamera_fpga_yr_cs_conv_clip_max_y_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209238L);
    system_hw_write_32(0x209238L, (((uint32_t) (data & 0x3ff)) << 16) | (curr & 0xfc00ffff));
}
static __inline uint16_t acamera_fpga_yr_cs_conv_clip_max_y_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209238L) & 0x3ff0000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: Clip min UV
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Minimal value for Cb, Cr.  Values below this are clipped.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_CLIP_MIN_UV_DEFAULT (0x000)
#define ACAMERA_FPGA_YR_CS_CONV_CLIP_MIN_UV_DATASIZE (10)
#define ACAMERA_FPGA_YR_CS_CONV_CLIP_MIN_UV_OFFSET (0x23c)
#define ACAMERA_FPGA_YR_CS_CONV_CLIP_MIN_UV_MASK (0x3ff)

// args: data (10-bit)
static __inline void acamera_fpga_yr_cs_conv_clip_min_uv_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20923cL);
    system_hw_write_32(0x20923cL, (((uint32_t) (data & 0x3ff)) << 0) | (curr & 0xfffffc00));
}
static __inline uint16_t acamera_fpga_yr_cs_conv_clip_min_uv_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20923cL) & 0x3ff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Clip max UV
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Maximal value for Cb, Cr.  Values above this are clipped.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_CLIP_MAX_UV_DEFAULT (0x3FF)
#define ACAMERA_FPGA_YR_CS_CONV_CLIP_MAX_UV_DATASIZE (10)
#define ACAMERA_FPGA_YR_CS_CONV_CLIP_MAX_UV_OFFSET (0x23c)
#define ACAMERA_FPGA_YR_CS_CONV_CLIP_MAX_UV_MASK (0x3ff0000)

// args: data (10-bit)
static __inline void acamera_fpga_yr_cs_conv_clip_max_uv_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20923cL);
    system_hw_write_32(0x20923cL, (((uint32_t) (data & 0x3ff)) << 16) | (curr & 0xfc00ffff));
}
static __inline uint16_t acamera_fpga_yr_cs_conv_clip_max_uv_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20923cL) & 0x3ff0000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: Enable dither
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Enables dithering module
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_ENABLE_DITHER_DEFAULT (0x0)
#define ACAMERA_FPGA_YR_CS_CONV_ENABLE_DITHER_DATASIZE (1)
#define ACAMERA_FPGA_YR_CS_CONV_ENABLE_DITHER_OFFSET (0x244)
#define ACAMERA_FPGA_YR_CS_CONV_ENABLE_DITHER_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_yr_cs_conv_enable_dither_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209244L);
    system_hw_write_32(0x209244L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_yr_cs_conv_enable_dither_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209244L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Dither amount
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0= dither to 9 bits; 1=dither to 8 bits; 2=dither to 7 bits; 3=dither to 6 bits 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_DITHER_AMOUNT_DEFAULT (0x0)
#define ACAMERA_FPGA_YR_CS_CONV_DITHER_AMOUNT_DATASIZE (2)
#define ACAMERA_FPGA_YR_CS_CONV_DITHER_AMOUNT_OFFSET (0x244)
#define ACAMERA_FPGA_YR_CS_CONV_DITHER_AMOUNT_MASK (0x6)

// args: data (2-bit)
static __inline void acamera_fpga_yr_cs_conv_dither_amount_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209244L);
    system_hw_write_32(0x209244L, (((uint32_t) (data & 0x3)) << 1) | (curr & 0xfffffff9));
}
static __inline uint8_t acamera_fpga_yr_cs_conv_dither_amount_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209244L) & 0x6) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: Shift mode
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0= output is LSB aligned; 1=output is MSB aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_SHIFT_MODE_DEFAULT (0x0)
#define ACAMERA_FPGA_YR_CS_CONV_SHIFT_MODE_DATASIZE (1)
#define ACAMERA_FPGA_YR_CS_CONV_SHIFT_MODE_OFFSET (0x244)
#define ACAMERA_FPGA_YR_CS_CONV_SHIFT_MODE_MASK (0x10)

// args: data (1-bit)
static __inline void acamera_fpga_yr_cs_conv_shift_mode_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209244L);
    system_hw_write_32(0x209244L, (((uint32_t) (data & 0x1)) << 4) | (curr & 0xffffffef));
}
static __inline uint8_t acamera_fpga_yr_cs_conv_shift_mode_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209244L) & 0x10) >> 4);
}
// ------------------------------------------------------------------------------ //
// Register: Data mask RY
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Data mask for channel 1 (R or Y).  Bit-wise and of this value and video data.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_DATA_MASK_RY_DEFAULT (0x3FF)
#define ACAMERA_FPGA_YR_CS_CONV_DATA_MASK_RY_DATASIZE (10)
#define ACAMERA_FPGA_YR_CS_CONV_DATA_MASK_RY_OFFSET (0x258)
#define ACAMERA_FPGA_YR_CS_CONV_DATA_MASK_RY_MASK (0x3ff)

// args: data (10-bit)
static __inline void acamera_fpga_yr_cs_conv_data_mask_ry_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209258L);
    system_hw_write_32(0x209258L, (((uint32_t) (data & 0x3ff)) << 0) | (curr & 0xfffffc00));
}
static __inline uint16_t acamera_fpga_yr_cs_conv_data_mask_ry_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209258L) & 0x3ff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Data mask GU
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Data mask for channel 2 (G or U).  Bit-wise and of this value and video data.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_DATA_MASK_GU_DEFAULT (0x3FF)
#define ACAMERA_FPGA_YR_CS_CONV_DATA_MASK_GU_DATASIZE (10)
#define ACAMERA_FPGA_YR_CS_CONV_DATA_MASK_GU_OFFSET (0x258)
#define ACAMERA_FPGA_YR_CS_CONV_DATA_MASK_GU_MASK (0x3ff0000)

// args: data (10-bit)
static __inline void acamera_fpga_yr_cs_conv_data_mask_gu_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209258L);
    system_hw_write_32(0x209258L, (((uint32_t) (data & 0x3ff)) << 16) | (curr & 0xfc00ffff));
}
static __inline uint16_t acamera_fpga_yr_cs_conv_data_mask_gu_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209258L) & 0x3ff0000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: Data mask BV
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Data mask for channel 3 (B or V).  Bit-wise and of this value and video data.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_YR_CS_CONV_DATA_MASK_BV_DEFAULT (0x3FF)
#define ACAMERA_FPGA_YR_CS_CONV_DATA_MASK_BV_DATASIZE (10)
#define ACAMERA_FPGA_YR_CS_CONV_DATA_MASK_BV_OFFSET (0x25c)
#define ACAMERA_FPGA_YR_CS_CONV_DATA_MASK_BV_MASK (0x3ff)

// args: data (10-bit)
static __inline void acamera_fpga_yr_cs_conv_data_mask_bv_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20925cL);
    system_hw_write_32(0x20925cL, (((uint32_t) (data & 0x3ff)) << 0) | (curr & 0xfffffc00));
}
static __inline uint16_t acamera_fpga_yr_cs_conv_data_mask_bv_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20925cL) & 0x3ff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Group: Video-Capture-1 Input port
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Controls video input port.  
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: preset
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//        Allows selection of various input port presets for standard sensor inputs.  See ISP Guide for details of available presets.
//        0-14: Frequently used presets.  If using one of available presets, remaining bits in registers 0x100 and 0x104 are not used.
//        15:   Input port configured according to registers in 0x100 and 0x104.  Consult Apical support for special input port requirements.
//      
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_PRESET_DEFAULT (2)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_PRESET_DATASIZE (4)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_PRESET_OFFSET (0x264)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_PRESET_MASK (0xf)

// args: data (4-bit)
static __inline void acamera_fpga_video_capture1_input_port_preset_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209264L);
    system_hw_write_32(0x209264L, (((uint32_t) (data & 0xf)) << 0) | (curr & 0xfffffff0));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_preset_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209264L) & 0xf) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: vs_use field
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VS_USE_FIELD_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VS_USE_FIELD_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VS_USE_FIELD_OFFSET (0x264)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VS_USE_FIELD_MASK (0x100)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VS_USE_FIELD_USE_VSYNC_I_PORT_FOR_VERTICAL_SYNC (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VS_USE_FIELD_USE_FIELD_I_PORT_FOR_VERTICAL_SYNC (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_vs_use_field_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209264L);
    system_hw_write_32(0x209264L, (((uint32_t) (data & 0x1)) << 8) | (curr & 0xfffffeff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_vs_use_field_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209264L) & 0x100) >> 8);
}
// ------------------------------------------------------------------------------ //
// Register: vs toggle
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VS_TOGGLE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VS_TOGGLE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VS_TOGGLE_OFFSET (0x264)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VS_TOGGLE_MASK (0x200)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VS_TOGGLE_VSYNC_IS_PULSETYPE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VS_TOGGLE_VSYNC_IS_TOGGLETYPE_FIELD_SIGNAL (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_vs_toggle_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209264L);
    system_hw_write_32(0x209264L, (((uint32_t) (data & 0x1)) << 9) | (curr & 0xfffffdff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_vs_toggle_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209264L) & 0x200) >> 9);
}
// ------------------------------------------------------------------------------ //
// Register: vs polarity
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VS_POLARITY_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VS_POLARITY_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VS_POLARITY_OFFSET (0x264)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VS_POLARITY_MASK (0x400)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VS_POLARITY_HORIZONTAL_COUNTER_RESET_ON_RISING_EDGE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VS_POLARITY_HORIZONTAL_COUNTER_RESET_ON_FALLING_EDGE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_vs_polarity_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209264L);
    system_hw_write_32(0x209264L, (((uint32_t) (data & 0x1)) << 10) | (curr & 0xfffffbff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_vs_polarity_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209264L) & 0x400) >> 10);
}
// ------------------------------------------------------------------------------ //
// Register: vs_polarity acl
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VS_POLARITY_ACL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VS_POLARITY_ACL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VS_POLARITY_ACL_OFFSET (0x264)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VS_POLARITY_ACL_MASK (0x800)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VS_POLARITY_ACL_DONT_INVERT_POLARITY_FOR_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VS_POLARITY_ACL_INVERT_POLARITY_FOR_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_vs_polarity_acl_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209264L);
    system_hw_write_32(0x209264L, (((uint32_t) (data & 0x1)) << 11) | (curr & 0xfffff7ff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_vs_polarity_acl_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209264L) & 0x800) >> 11);
}
// ------------------------------------------------------------------------------ //
// Register: hs_use acl
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_USE_ACL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_USE_ACL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_USE_ACL_OFFSET (0x264)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_USE_ACL_MASK (0x1000)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_USE_ACL_USE_HSYNC_I_PORT_FOR_ACTIVELINE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_USE_ACL_USE_ACL_I_PORT_FOR_ACTIVELINE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_hs_use_acl_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209264L);
    system_hw_write_32(0x209264L, (((uint32_t) (data & 0x1)) << 12) | (curr & 0xffffefff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_hs_use_acl_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209264L) & 0x1000) >> 12);
}
// ------------------------------------------------------------------------------ //
// Register: vc_c select
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VC_C_SELECT_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VC_C_SELECT_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VC_C_SELECT_OFFSET (0x264)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VC_C_SELECT_MASK (0x4000)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VC_C_SELECT_VERTICAL_COUNTER_COUNTS_ON_HS (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VC_C_SELECT_VERTICAL_COUNTER_COUNTS_ON_HORIZONTAL_COUNTER_OVERFLOW_OR_RESET (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_vc_c_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209264L);
    system_hw_write_32(0x209264L, (((uint32_t) (data & 0x1)) << 14) | (curr & 0xffffbfff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_vc_c_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209264L) & 0x4000) >> 14);
}
// ------------------------------------------------------------------------------ //
// Register: vc_r select
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VC_R_SELECT_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VC_R_SELECT_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VC_R_SELECT_OFFSET (0x264)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VC_R_SELECT_MASK (0x8000)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VC_R_SELECT_VERTICAL_COUNTER_IS_RESET_ON_EDGE_OF_VS (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VC_R_SELECT_VERTICAL_COUNTER_IS_RESET_AFTER_TIMEOUT_ON_HS (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_vc_r_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209264L);
    system_hw_write_32(0x209264L, (((uint32_t) (data & 0x1)) << 15) | (curr & 0xffff7fff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_vc_r_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209264L) & 0x8000) >> 15);
}
// ------------------------------------------------------------------------------ //
// Register: hs_xor vs
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_XOR_VS_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_XOR_VS_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_XOR_VS_OFFSET (0x264)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_XOR_VS_MASK (0x10000)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_XOR_VS_NORMAL_MODE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_XOR_VS_HVALID__HSYNC_XOR_VSYNC (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_hs_xor_vs_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209264L);
    system_hw_write_32(0x209264L, (((uint32_t) (data & 0x1)) << 16) | (curr & 0xfffeffff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_hs_xor_vs_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209264L) & 0x10000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: hs polarity
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_POLARITY_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_POLARITY_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_POLARITY_OFFSET (0x264)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_POLARITY_MASK (0x20000)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_POLARITY_DONT_INVERT_POLARITY_OF_HS_FOR_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_POLARITY_INVERT_POLARITY_OF_HS_FOR_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_hs_polarity_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209264L);
    system_hw_write_32(0x209264L, (((uint32_t) (data & 0x1)) << 17) | (curr & 0xfffdffff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_hs_polarity_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209264L) & 0x20000) >> 17);
}
// ------------------------------------------------------------------------------ //
// Register: hs_polarity acl
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_POLARITY_ACL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_POLARITY_ACL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_POLARITY_ACL_OFFSET (0x264)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_POLARITY_ACL_MASK (0x40000)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_POLARITY_ACL_DONT_INVERT_POLARITY_OF_HS_FOR_HS_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_POLARITY_ACL_INVERT_POLARITY_OF_HS_FOR_HS_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_hs_polarity_acl_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209264L);
    system_hw_write_32(0x209264L, (((uint32_t) (data & 0x1)) << 18) | (curr & 0xfffbffff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_hs_polarity_acl_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209264L) & 0x40000) >> 18);
}
// ------------------------------------------------------------------------------ //
// Register: hs_polarity hs
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_POLARITY_HS_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_POLARITY_HS_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_POLARITY_HS_OFFSET (0x264)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_POLARITY_HS_MASK (0x80000)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_POLARITY_HS_HORIZONTAL_COUNTER_IS_RESET_ON_RISING_EDGE_OF_HS (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_POLARITY_HS_HORIZONTAL_COUNTER_IS_RESET_ON_VSYNC_EG_WHEN_HSYNC_IS_NOT_AVAILABLE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_hs_polarity_hs_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209264L);
    system_hw_write_32(0x209264L, (((uint32_t) (data & 0x1)) << 19) | (curr & 0xfff7ffff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_hs_polarity_hs_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209264L) & 0x80000) >> 19);
}
// ------------------------------------------------------------------------------ //
// Register: hs_polarity vc
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_POLARITY_VC_DEFAULT (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_POLARITY_VC_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_POLARITY_VC_OFFSET (0x264)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_POLARITY_VC_MASK (0x100000)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_POLARITY_VC_VERTICAL_COUNTER_INCREMENTS_ON_RISING_EDGE_OF_HS (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HS_POLARITY_VC_VERTICAL_COUNTER_INCREMENTS_ON_FALLING_EDGE_OF_HS (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_hs_polarity_vc_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209264L);
    system_hw_write_32(0x209264L, (((uint32_t) (data & 0x1)) << 20) | (curr & 0xffefffff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_hs_polarity_vc_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209264L) & 0x100000) >> 20);
}
// ------------------------------------------------------------------------------ //
// Register: hc_r select
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_R_SELECT_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_R_SELECT_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_R_SELECT_OFFSET (0x264)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_R_SELECT_MASK (0x800000)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_R_SELECT_VERTICAL_COUNTER_IS_RESET_ON_RISING_EDGE_OF_HS (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_R_SELECT_VERTICAL_COUNTER_IS_RESET_ON_RISING_EDGE_OF_VS (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_hc_r_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209264L);
    system_hw_write_32(0x209264L, (((uint32_t) (data & 0x1)) << 23) | (curr & 0xff7fffff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_hc_r_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209264L) & 0x800000) >> 23);
}
// ------------------------------------------------------------------------------ //
// Register: acl polarity
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACL_POLARITY_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACL_POLARITY_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACL_POLARITY_OFFSET (0x264)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACL_POLARITY_MASK (0x1000000)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACL_POLARITY_DONT_INVERT_ACL_I_FOR_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACL_POLARITY_INVERT_ACL_I_FOR_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_acl_polarity_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209264L);
    system_hw_write_32(0x209264L, (((uint32_t) (data & 0x1)) << 24) | (curr & 0xfeffffff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_acl_polarity_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209264L) & 0x1000000) >> 24);
}
// ------------------------------------------------------------------------------ //
// Register: field polarity
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELD_POLARITY_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELD_POLARITY_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELD_POLARITY_OFFSET (0x268)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELD_POLARITY_MASK (0x1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELD_POLARITY_DONT_INVERT_FIELD_I_FOR_FIELD_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELD_POLARITY_INVERT_FIELD_I_FOR_FIELD_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_field_polarity_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209268L);
    system_hw_write_32(0x209268L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_field_polarity_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209268L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: field toggle
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELD_TOGGLE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELD_TOGGLE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELD_TOGGLE_OFFSET (0x268)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELD_TOGGLE_MASK (0x2)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELD_TOGGLE_FIELD_IS_PULSETYPE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELD_TOGGLE_FIELD_IS_TOGGLETYPE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_field_toggle_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209268L);
    system_hw_write_32(0x209268L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_field_toggle_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209268L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: aclg window0
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_WINDOW0_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_WINDOW0_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_WINDOW0_OFFSET (0x268)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_WINDOW0_MASK (0x100)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_WINDOW0_EXCLUDE_WINDOW0_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_WINDOW0_INCLUDE_WINDOW0_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_aclg_window0_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209268L);
    system_hw_write_32(0x209268L, (((uint32_t) (data & 0x1)) << 8) | (curr & 0xfffffeff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_aclg_window0_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209268L) & 0x100) >> 8);
}
// ------------------------------------------------------------------------------ //
// Register: aclg hsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_HSYNC_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_HSYNC_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_HSYNC_OFFSET (0x268)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_HSYNC_MASK (0x200)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_HSYNC_EXCLUDE_HSYNC_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_HSYNC_INCLUDE_HSYNC_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_aclg_hsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209268L);
    system_hw_write_32(0x209268L, (((uint32_t) (data & 0x1)) << 9) | (curr & 0xfffffdff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_aclg_hsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209268L) & 0x200) >> 9);
}
// ------------------------------------------------------------------------------ //
// Register: aclg window2
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_WINDOW2_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_WINDOW2_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_WINDOW2_OFFSET (0x268)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_WINDOW2_MASK (0x400)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_WINDOW2_EXCLUDE_WINDOW2_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_WINDOW2_INCLUDE_WINDOW2_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_aclg_window2_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209268L);
    system_hw_write_32(0x209268L, (((uint32_t) (data & 0x1)) << 10) | (curr & 0xfffffbff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_aclg_window2_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209268L) & 0x400) >> 10);
}
// ------------------------------------------------------------------------------ //
// Register: aclg acl
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_ACL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_ACL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_ACL_OFFSET (0x268)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_ACL_MASK (0x800)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_ACL_EXCLUDE_ACL_I_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_ACL_INCLUDE_ACL_I_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_aclg_acl_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209268L);
    system_hw_write_32(0x209268L, (((uint32_t) (data & 0x1)) << 11) | (curr & 0xfffff7ff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_aclg_acl_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209268L) & 0x800) >> 11);
}
// ------------------------------------------------------------------------------ //
// Register: aclg vsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_VSYNC_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_VSYNC_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_VSYNC_OFFSET (0x268)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_VSYNC_MASK (0x1000)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_VSYNC_EXCLUDE_VSYNC_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_ACLG_VSYNC_INCLUDE_VSYNC_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_aclg_vsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209268L);
    system_hw_write_32(0x209268L, (((uint32_t) (data & 0x1)) << 12) | (curr & 0xffffefff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_aclg_vsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209268L) & 0x1000) >> 12);
}
// ------------------------------------------------------------------------------ //
// Register: hsg window1
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HSG_WINDOW1_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HSG_WINDOW1_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HSG_WINDOW1_OFFSET (0x268)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HSG_WINDOW1_MASK (0x10000)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HSG_WINDOW1_EXCLUDE_WINDOW1_SIGNAL_IN_HS_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HSG_WINDOW1_INCLUDE_WINDOW1_SIGNAL_IN_HS_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_hsg_window1_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209268L);
    system_hw_write_32(0x209268L, (((uint32_t) (data & 0x1)) << 16) | (curr & 0xfffeffff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_hsg_window1_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209268L) & 0x10000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: hsg hsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HSG_HSYNC_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HSG_HSYNC_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HSG_HSYNC_OFFSET (0x268)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HSG_HSYNC_MASK (0x20000)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HSG_HSYNC_EXCLUDE_HSYNC_SIGNAL_IN_HS_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HSG_HSYNC_INCLUDE_HSYNC_SIGNAL_IN_HS_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_hsg_hsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209268L);
    system_hw_write_32(0x209268L, (((uint32_t) (data & 0x1)) << 17) | (curr & 0xfffdffff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_hsg_hsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209268L) & 0x20000) >> 17);
}
// ------------------------------------------------------------------------------ //
// Register: hsg vsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HSG_VSYNC_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HSG_VSYNC_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HSG_VSYNC_OFFSET (0x268)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HSG_VSYNC_MASK (0x40000)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HSG_VSYNC_EXCLUDE_VSYNC_SIGNAL_IN_HS_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HSG_VSYNC_INCLUDE_VSYNC_SIGNAL_IN_HS_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_hsg_vsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209268L);
    system_hw_write_32(0x209268L, (((uint32_t) (data & 0x1)) << 18) | (curr & 0xfffbffff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_hsg_vsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209268L) & 0x40000) >> 18);
}
// ------------------------------------------------------------------------------ //
// Register: hsg window2
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HSG_WINDOW2_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HSG_WINDOW2_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HSG_WINDOW2_OFFSET (0x268)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HSG_WINDOW2_MASK (0x80000)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HSG_WINDOW2_EXCLUDE_WINDOW2_SIGNAL_IN_HS_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HSG_WINDOW2_INCLUDE_WINDOW2_SIGNAL_IN_HS_GATE_MASK_OUT_SPURIOUS_HS_DURING_BLANK (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_hsg_window2_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209268L);
    system_hw_write_32(0x209268L, (((uint32_t) (data & 0x1)) << 19) | (curr & 0xfff7ffff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_hsg_window2_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209268L) & 0x80000) >> 19);
}
// ------------------------------------------------------------------------------ //
// Register: fieldg vsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELDG_VSYNC_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELDG_VSYNC_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELDG_VSYNC_OFFSET (0x268)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELDG_VSYNC_MASK (0x1000000)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELDG_VSYNC_EXCLUDE_VSYNC_SIGNAL_IN_FIELD_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELDG_VSYNC_INCLUDE_VSYNC_SIGNAL_IN_FIELD_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_fieldg_vsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209268L);
    system_hw_write_32(0x209268L, (((uint32_t) (data & 0x1)) << 24) | (curr & 0xfeffffff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_fieldg_vsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209268L) & 0x1000000) >> 24);
}
// ------------------------------------------------------------------------------ //
// Register: fieldg window2
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELDG_WINDOW2_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELDG_WINDOW2_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELDG_WINDOW2_OFFSET (0x268)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELDG_WINDOW2_MASK (0x2000000)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELDG_WINDOW2_EXCLUDE_WINDOW2_SIGNAL_IN_FIELD_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELDG_WINDOW2_INCLUDE_WINDOW2_SIGNAL_IN_FIELD_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_fieldg_window2_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209268L);
    system_hw_write_32(0x209268L, (((uint32_t) (data & 0x1)) << 25) | (curr & 0xfdffffff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_fieldg_window2_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209268L) & 0x2000000) >> 25);
}
// ------------------------------------------------------------------------------ //
// Register: fieldg field
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELDG_FIELD_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELDG_FIELD_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELDG_FIELD_OFFSET (0x268)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELDG_FIELD_MASK (0x4000000)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELDG_FIELD_EXCLUDE_FIELD_I_SIGNAL_IN_FIELD_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELDG_FIELD_INCLUDE_FIELD_I_SIGNAL_IN_FIELD_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_fieldg_field_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209268L);
    system_hw_write_32(0x209268L, (((uint32_t) (data & 0x1)) << 26) | (curr & 0xfbffffff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_fieldg_field_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209268L) & 0x4000000) >> 26);
}
// ------------------------------------------------------------------------------ //
// Register: field mode
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELD_MODE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELD_MODE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELD_MODE_OFFSET (0x268)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELD_MODE_MASK (0x8000000)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELD_MODE_PULSE_FIELD (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FIELD_MODE_TOGGLE_FIELD (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_field_mode_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209268L);
    system_hw_write_32(0x209268L, (((uint32_t) (data & 0x1)) << 27) | (curr & 0xf7ffffff));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_field_mode_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209268L) & 0x8000000) >> 27);
}
// ------------------------------------------------------------------------------ //
// Register: hc limit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// horizontal counter limit value (counts: 0,1,...hc_limit-1,hc_limit,0,1,...)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_LIMIT_DEFAULT (0xFFFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_LIMIT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_LIMIT_OFFSET (0x26c)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_LIMIT_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture1_input_port_hc_limit_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20926cL);
    system_hw_write_32(0x20926cL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture1_input_port_hc_limit_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20926cL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: hc start0
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window0 start for ACL gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_START0_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_START0_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_START0_OFFSET (0x270)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_START0_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture1_input_port_hc_start0_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209270L);
    system_hw_write_32(0x209270L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture1_input_port_hc_start0_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209270L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: hc size0
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window0 size for ACL gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_SIZE0_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_SIZE0_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_SIZE0_OFFSET (0x274)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_SIZE0_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture1_input_port_hc_size0_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209274L);
    system_hw_write_32(0x209274L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture1_input_port_hc_size0_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209274L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: hc start1
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window1 start for HS gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_START1_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_START1_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_START1_OFFSET (0x278)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_START1_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture1_input_port_hc_start1_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209278L);
    system_hw_write_32(0x209278L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture1_input_port_hc_start1_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209278L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: hc size1
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window1 size for HS gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_SIZE1_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_SIZE1_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_SIZE1_OFFSET (0x27c)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_HC_SIZE1_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture1_input_port_hc_size1_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20927cL);
    system_hw_write_32(0x20927cL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture1_input_port_hc_size1_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20927cL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: vc limit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// vertical counter limit value (counts: 0,1,...vc_limit-1,vc_limit,0,1,...)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VC_LIMIT_DEFAULT (0xFFFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VC_LIMIT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VC_LIMIT_OFFSET (0x280)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VC_LIMIT_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture1_input_port_vc_limit_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209280L);
    system_hw_write_32(0x209280L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture1_input_port_vc_limit_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209280L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: vc start
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window2 start for ACL gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VC_START_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VC_START_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VC_START_OFFSET (0x284)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VC_START_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture1_input_port_vc_start_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209284L);
    system_hw_write_32(0x209284L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture1_input_port_vc_start_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209284L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: vc size
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window2 size for ACL gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VC_SIZE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VC_SIZE_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VC_SIZE_OFFSET (0x288)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_VC_SIZE_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture1_input_port_vc_size_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209288L);
    system_hw_write_32(0x209288L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture1_input_port_vc_size_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209288L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: frame width
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// detected frame width.  Read only value.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FRAME_WIDTH_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FRAME_WIDTH_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FRAME_WIDTH_OFFSET (0x28c)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FRAME_WIDTH_MASK (0xffff)

// args: data (16-bit)
static __inline uint16_t acamera_fpga_video_capture1_input_port_frame_width_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20928cL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: frame height
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// detected frame height.  Read only value.  
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FRAME_HEIGHT_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FRAME_HEIGHT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FRAME_HEIGHT_OFFSET (0x290)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FRAME_HEIGHT_MASK (0xffff)

// args: data (16-bit)
static __inline uint16_t acamera_fpga_video_capture1_input_port_frame_height_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209290L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: mode request
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Used to stop and start input port.  See ISP guide for further details. 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_MODE_REQUEST_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_MODE_REQUEST_DATASIZE (3)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_MODE_REQUEST_OFFSET (0x294)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_MODE_REQUEST_MASK (0x7)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_MODE_REQUEST_SAFE_STOP (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_MODE_REQUEST_SAFE_START (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_MODE_REQUEST_URGENT_STOP (2)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_MODE_REQUEST_URGENT_START (3)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_MODE_REQUEST_RESERVED4 (4)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_MODE_REQUEST_SAFER_START (5)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_MODE_REQUEST_RESERVED6 (6)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_MODE_REQUEST_RESERVED7 (7)

// args: data (3-bit)
static __inline void acamera_fpga_video_capture1_input_port_mode_request_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209294L);
    system_hw_write_32(0x209294L, (((uint32_t) (data & 0x7)) << 0) | (curr & 0xfffffff8));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_mode_request_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209294L) & 0x7) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: freeze config
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Used to freeze input port configuration.  Used when multiple register writes are required to change input port configuration. 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FREEZE_CONFIG_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FREEZE_CONFIG_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FREEZE_CONFIG_OFFSET (0x294)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FREEZE_CONFIG_MASK (0x80)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FREEZE_CONFIG_NORMAL_OPERATION (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_FREEZE_CONFIG_HOLD_PREVIOUS_INPUT_PORT_CONFIG_STATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_input_port_freeze_config_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209294L);
    system_hw_write_32(0x209294L, (((uint32_t) (data & 0x1)) << 7) | (curr & 0xffffff7f));
}
static __inline uint8_t acamera_fpga_video_capture1_input_port_freeze_config_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209294L) & 0x80) >> 7);
}
// ------------------------------------------------------------------------------ //
// Register: mode status
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//          Used to monitor input port status:
//          bit 0: 1=running, 0=stopped, bits 1,2-reserved
//        
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_MODE_STATUS_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_MODE_STATUS_DATASIZE (3)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_MODE_STATUS_OFFSET (0x298)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_INPUT_PORT_MODE_STATUS_MASK (0x7)

// args: data (3-bit)
static __inline uint8_t acamera_fpga_video_capture1_input_port_mode_status_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209298L) & 0x7) >> 0);
}
// ------------------------------------------------------------------------------ //
// Group: Video-Capture-1 Frame Stats
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: stats reset
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_STATS_RESET_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_STATS_RESET_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_STATS_RESET_OFFSET (0x29c)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_STATS_RESET_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_frame_stats_stats_reset_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20929cL);
    system_hw_write_32(0x20929cL, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture1_frame_stats_stats_reset_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20929cL) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: stats hold
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_STATS_HOLD_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_STATS_HOLD_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_STATS_HOLD_OFFSET (0x2a0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_STATS_HOLD_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_frame_stats_stats_hold_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2092a0L);
    system_hw_write_32(0x2092a0L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture1_frame_stats_stats_hold_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2092a0L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: active width min
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_WIDTH_MIN_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_WIDTH_MIN_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_WIDTH_MIN_OFFSET (0x2ac)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_WIDTH_MIN_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture1_frame_stats_active_width_min_read(uintptr_t base) {
    return system_hw_read_32(0x2092acL);
}
// ------------------------------------------------------------------------------ //
// Register: active width max
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_WIDTH_MAX_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_WIDTH_MAX_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_WIDTH_MAX_OFFSET (0x2b0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_WIDTH_MAX_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture1_frame_stats_active_width_max_read(uintptr_t base) {
    return system_hw_read_32(0x2092b0L);
}
// ------------------------------------------------------------------------------ //
// Register: active width sum
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_WIDTH_SUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_WIDTH_SUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_WIDTH_SUM_OFFSET (0x2b4)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_WIDTH_SUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture1_frame_stats_active_width_sum_read(uintptr_t base) {
    return system_hw_read_32(0x2092b4L);
}
// ------------------------------------------------------------------------------ //
// Register: active width num
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_WIDTH_NUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_WIDTH_NUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_WIDTH_NUM_OFFSET (0x2b8)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_WIDTH_NUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture1_frame_stats_active_width_num_read(uintptr_t base) {
    return system_hw_read_32(0x2092b8L);
}
// ------------------------------------------------------------------------------ //
// Register: active height min
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_HEIGHT_MIN_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_HEIGHT_MIN_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_HEIGHT_MIN_OFFSET (0x2bc)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_HEIGHT_MIN_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture1_frame_stats_active_height_min_read(uintptr_t base) {
    return system_hw_read_32(0x2092bcL);
}
// ------------------------------------------------------------------------------ //
// Register: active height max
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_HEIGHT_MAX_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_HEIGHT_MAX_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_HEIGHT_MAX_OFFSET (0x2c0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_HEIGHT_MAX_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture1_frame_stats_active_height_max_read(uintptr_t base) {
    return system_hw_read_32(0x2092c0L);
}
// ------------------------------------------------------------------------------ //
// Register: active height sum
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_HEIGHT_SUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_HEIGHT_SUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_HEIGHT_SUM_OFFSET (0x2c4)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_HEIGHT_SUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture1_frame_stats_active_height_sum_read(uintptr_t base) {
    return system_hw_read_32(0x2092c4L);
}
// ------------------------------------------------------------------------------ //
// Register: active height num
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_HEIGHT_NUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_HEIGHT_NUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_HEIGHT_NUM_OFFSET (0x2c8)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_ACTIVE_HEIGHT_NUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture1_frame_stats_active_height_num_read(uintptr_t base) {
    return system_hw_read_32(0x2092c8L);
}
// ------------------------------------------------------------------------------ //
// Register: hblank min
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_HBLANK_MIN_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_HBLANK_MIN_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_HBLANK_MIN_OFFSET (0x2cc)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_HBLANK_MIN_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture1_frame_stats_hblank_min_read(uintptr_t base) {
    return system_hw_read_32(0x2092ccL);
}
// ------------------------------------------------------------------------------ //
// Register: hblank max
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_HBLANK_MAX_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_HBLANK_MAX_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_HBLANK_MAX_OFFSET (0x2d0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_HBLANK_MAX_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture1_frame_stats_hblank_max_read(uintptr_t base) {
    return system_hw_read_32(0x2092d0L);
}
// ------------------------------------------------------------------------------ //
// Register: hblank sum
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_HBLANK_SUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_HBLANK_SUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_HBLANK_SUM_OFFSET (0x2d4)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_HBLANK_SUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture1_frame_stats_hblank_sum_read(uintptr_t base) {
    return system_hw_read_32(0x2092d4L);
}
// ------------------------------------------------------------------------------ //
// Register: hblank num
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_HBLANK_NUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_HBLANK_NUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_HBLANK_NUM_OFFSET (0x2d8)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_HBLANK_NUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture1_frame_stats_hblank_num_read(uintptr_t base) {
    return system_hw_read_32(0x2092d8L);
}
// ------------------------------------------------------------------------------ //
// Register: vblank min
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_VBLANK_MIN_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_VBLANK_MIN_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_VBLANK_MIN_OFFSET (0x2dc)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_VBLANK_MIN_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture1_frame_stats_vblank_min_read(uintptr_t base) {
    return system_hw_read_32(0x2092dcL);
}
// ------------------------------------------------------------------------------ //
// Register: vblank max
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_VBLANK_MAX_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_VBLANK_MAX_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_VBLANK_MAX_OFFSET (0x2e0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_VBLANK_MAX_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture1_frame_stats_vblank_max_read(uintptr_t base) {
    return system_hw_read_32(0x2092e0L);
}
// ------------------------------------------------------------------------------ //
// Register: vblank sum
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_VBLANK_SUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_VBLANK_SUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_VBLANK_SUM_OFFSET (0x2e4)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_VBLANK_SUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture1_frame_stats_vblank_sum_read(uintptr_t base) {
    return system_hw_read_32(0x2092e4L);
}
// ------------------------------------------------------------------------------ //
// Register: vblank num
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_VBLANK_NUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_VBLANK_NUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_VBLANK_NUM_OFFSET (0x2e8)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_FRAME_STATS_VBLANK_NUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture1_frame_stats_vblank_num_read(uintptr_t base) {
    return system_hw_read_32(0x2092e8L);
}
// ------------------------------------------------------------------------------ //
// Group: Video-Capture-1 video test gen
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Video test generator controls.  See ISP Guide for further details
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: test_pattern_off on
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Test pattern off-on: 0=off, 1=on
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_TEST_PATTERN_OFF_ON_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_TEST_PATTERN_OFF_ON_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_TEST_PATTERN_OFF_ON_OFFSET (0x2f0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_TEST_PATTERN_OFF_ON_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_video_test_gen_test_pattern_off_on_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2092f0L);
    system_hw_write_32(0x2092f0L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture1_video_test_gen_test_pattern_off_on_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2092f0L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: bayer_rgb_i sel
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Bayer or rgb select for input video: 0=bayer, 1=rgb
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_BAYER_RGB_I_SEL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_BAYER_RGB_I_SEL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_BAYER_RGB_I_SEL_OFFSET (0x2f0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_BAYER_RGB_I_SEL_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_video_test_gen_bayer_rgb_i_sel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2092f0L);
    system_hw_write_32(0x2092f0L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_video_capture1_video_test_gen_bayer_rgb_i_sel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2092f0L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: bayer_rgb_o sel
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Bayer or rgb select for output video: 0=bayer, 1=rgb
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_BAYER_RGB_O_SEL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_BAYER_RGB_O_SEL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_BAYER_RGB_O_SEL_OFFSET (0x2f0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_BAYER_RGB_O_SEL_MASK (0x4)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_video_test_gen_bayer_rgb_o_sel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2092f0L);
    system_hw_write_32(0x2092f0L, (((uint32_t) (data & 0x1)) << 2) | (curr & 0xfffffffb));
}
static __inline uint8_t acamera_fpga_video_capture1_video_test_gen_bayer_rgb_o_sel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2092f0L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: pattern type
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Pattern type select: 0=Flat field,1=Horizontal gradient,2=Vertical Gradient,3=Vertical Bars,4=Rectangle,5-255=Default white frame on black
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_PATTERN_TYPE_DEFAULT (0x03)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_PATTERN_TYPE_DATASIZE (8)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_PATTERN_TYPE_OFFSET (0x2f4)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_PATTERN_TYPE_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_video_capture1_video_test_gen_pattern_type_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2092f4L);
    system_hw_write_32(0x2092f4L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_video_capture1_video_test_gen_pattern_type_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2092f4L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: r backgnd
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Red background  value 16bit, MSB aligned to used width 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_R_BACKGND_DEFAULT (0xFFFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_R_BACKGND_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_R_BACKGND_OFFSET (0x2f8)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_R_BACKGND_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture1_video_test_gen_r_backgnd_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2092f8L);
    system_hw_write_32(0x2092f8L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture1_video_test_gen_r_backgnd_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2092f8L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: g backgnd
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Green background value 16bit, MSB aligned to used width
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_G_BACKGND_DEFAULT (0xFFFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_G_BACKGND_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_G_BACKGND_OFFSET (0x2fc)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_G_BACKGND_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture1_video_test_gen_g_backgnd_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2092fcL);
    system_hw_write_32(0x2092fcL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture1_video_test_gen_g_backgnd_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2092fcL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: b backgnd
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Blue background value 16bit, MSB aligned to used width
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_B_BACKGND_DEFAULT (0xFFFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_B_BACKGND_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_B_BACKGND_OFFSET (0x300)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_B_BACKGND_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture1_video_test_gen_b_backgnd_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209300L);
    system_hw_write_32(0x209300L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture1_video_test_gen_b_backgnd_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209300L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: r foregnd
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Red foreground  value 16bit, MSB aligned to used width 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_R_FOREGND_DEFAULT (0x8FFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_R_FOREGND_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_R_FOREGND_OFFSET (0x304)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_R_FOREGND_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture1_video_test_gen_r_foregnd_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209304L);
    system_hw_write_32(0x209304L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture1_video_test_gen_r_foregnd_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209304L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: g foregnd
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Green foreground value 16bit, MSB aligned to used width
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_G_FOREGND_DEFAULT (0x8FFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_G_FOREGND_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_G_FOREGND_OFFSET (0x308)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_G_FOREGND_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture1_video_test_gen_g_foregnd_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209308L);
    system_hw_write_32(0x209308L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture1_video_test_gen_g_foregnd_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209308L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: b foregnd
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Blue foreground value 16bit, MSB aligned to used width
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_B_FOREGND_DEFAULT (0x8FFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_B_FOREGND_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_B_FOREGND_OFFSET (0x30c)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_B_FOREGND_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture1_video_test_gen_b_foregnd_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20930cL);
    system_hw_write_32(0x20930cL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture1_video_test_gen_b_foregnd_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20930cL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rgb gradient
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// RGB gradient increment per pixel (0-15)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_RGB_GRADIENT_DEFAULT (0x3CAA)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_RGB_GRADIENT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_RGB_GRADIENT_OFFSET (0x310)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_RGB_GRADIENT_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture1_video_test_gen_rgb_gradient_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209310L);
    system_hw_write_32(0x209310L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture1_video_test_gen_rgb_gradient_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209310L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rgb_gradient start
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// RGB gradient start value 16bit, MSB aligned to used width
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_RGB_GRADIENT_START_DEFAULT (0x0000)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_RGB_GRADIENT_START_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_RGB_GRADIENT_START_OFFSET (0x314)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_RGB_GRADIENT_START_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture1_video_test_gen_rgb_gradient_start_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209314L);
    system_hw_write_32(0x209314L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture1_video_test_gen_rgb_gradient_start_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209314L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rect top
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  Rectangle top line number 1-n
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_RECT_TOP_DEFAULT (0x0001)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_RECT_TOP_DATASIZE (14)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_RECT_TOP_OFFSET (0x318)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_RECT_TOP_MASK (0x3fff)

// args: data (14-bit)
static __inline void acamera_fpga_video_capture1_video_test_gen_rect_top_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209318L);
    system_hw_write_32(0x209318L, (((uint32_t) (data & 0x3fff)) << 0) | (curr & 0xffffc000));
}
static __inline uint16_t acamera_fpga_video_capture1_video_test_gen_rect_top_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209318L) & 0x3fff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rect bot
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  Rectangle bottom line number 1-n
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_RECT_BOT_DEFAULT (0x0100)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_RECT_BOT_DATASIZE (14)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_RECT_BOT_OFFSET (0x31c)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_RECT_BOT_MASK (0x3fff)

// args: data (14-bit)
static __inline void acamera_fpga_video_capture1_video_test_gen_rect_bot_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20931cL);
    system_hw_write_32(0x20931cL, (((uint32_t) (data & 0x3fff)) << 0) | (curr & 0xffffc000));
}
static __inline uint16_t acamera_fpga_video_capture1_video_test_gen_rect_bot_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20931cL) & 0x3fff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rect left
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  Rectangle left pixel number 1-n
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_RECT_LEFT_DEFAULT (0x0001)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_RECT_LEFT_DATASIZE (14)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_RECT_LEFT_OFFSET (0x320)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_RECT_LEFT_MASK (0x3fff)

// args: data (14-bit)
static __inline void acamera_fpga_video_capture1_video_test_gen_rect_left_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209320L);
    system_hw_write_32(0x209320L, (((uint32_t) (data & 0x3fff)) << 0) | (curr & 0xffffc000));
}
static __inline uint16_t acamera_fpga_video_capture1_video_test_gen_rect_left_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209320L) & 0x3fff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rect right
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  Rectangle right pixel number 1-n
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_RECT_RIGHT_DEFAULT (0x0100)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_RECT_RIGHT_DATASIZE (14)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_RECT_RIGHT_OFFSET (0x328)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_VIDEO_TEST_GEN_RECT_RIGHT_MASK (0x3fff)

// args: data (14-bit)
static __inline void acamera_fpga_video_capture1_video_test_gen_rect_right_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209328L);
    system_hw_write_32(0x209328L, (((uint32_t) (data & 0x3fff)) << 0) | (curr & 0xffffc000));
}
static __inline uint16_t acamera_fpga_video_capture1_video_test_gen_rect_right_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209328L) & 0x3fff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Group: Video-Capture-1 DMA Writer
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Video-Capture DMA writer controls
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: Format
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Format
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_FORMAT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_FORMAT_DATASIZE (8)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_FORMAT_OFFSET (0x32c)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_FORMAT_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_format_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20932cL);
    system_hw_write_32(0x20932cL, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_format_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20932cL) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Base mode
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Base DMA packing mode for RGB/RAW/YUV etc (see ISP guide)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BASE_MODE_DEFAULT (0x0D)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BASE_MODE_DATASIZE (4)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BASE_MODE_OFFSET (0x32c)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BASE_MODE_MASK (0xf)

// args: data (4-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_base_mode_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20932cL);
    system_hw_write_32(0x20932cL, (((uint32_t) (data & 0xf)) << 0) | (curr & 0xfffffff0));
}
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_base_mode_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20932cL) & 0xf) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Plane select
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Plane select for planar base modes.  Only used if planar outputs required.  Not used.  Should be set to 0
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_PLANE_SELECT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_PLANE_SELECT_DATASIZE (2)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_PLANE_SELECT_OFFSET (0x32c)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_PLANE_SELECT_MASK (0xc0)

// args: data (2-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_plane_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20932cL);
    system_hw_write_32(0x20932cL, (((uint32_t) (data & 0x3)) << 6) | (curr & 0xffffff3f));
}
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_plane_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20932cL) & 0xc0) >> 6);
}
// ------------------------------------------------------------------------------ //
// Register: single frame
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0 = All frames are written(after frame_write_on= 1), 1= only 1st frame written ( after frame_write_on =1)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_SINGLE_FRAME_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_SINGLE_FRAME_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_SINGLE_FRAME_OFFSET (0x330)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_SINGLE_FRAME_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_single_frame_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209330L);
    system_hw_write_32(0x209330L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_single_frame_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209330L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: frame write on
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//    0 = no frames written(when switched from 1, current frame completes writing before stopping),
//    1= write frame(s) (write single or continous frame(s) )
//    
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_FRAME_WRITE_ON_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_FRAME_WRITE_ON_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_FRAME_WRITE_ON_OFFSET (0x330)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_FRAME_WRITE_ON_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_frame_write_on_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209330L);
    system_hw_write_32(0x209330L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_frame_write_on_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209330L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: half irate
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0 = normal operation , 1= write half(alternate) of input frames( only valid for continuous mode)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_HALF_IRATE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_HALF_IRATE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_HALF_IRATE_OFFSET (0x330)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_HALF_IRATE_MASK (0x4)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_half_irate_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209330L);
    system_hw_write_32(0x209330L, (((uint32_t) (data & 0x1)) << 2) | (curr & 0xfffffffb));
}
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_half_irate_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209330L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: axi xact comp
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0 = dont wait for axi transaction completion at end of frame(just all transfers accepted). 1 = wait for all transactions completed
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_XACT_COMP_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_XACT_COMP_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_XACT_COMP_OFFSET (0x330)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_XACT_COMP_MASK (0x8)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_axi_xact_comp_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209330L);
    system_hw_write_32(0x209330L, (((uint32_t) (data & 0x1)) << 3) | (curr & 0xfffffff7));
}
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_axi_xact_comp_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209330L) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: active width
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Active video width in pixels 128-8000
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_ACTIVE_WIDTH_DEFAULT (0x780)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_ACTIVE_WIDTH_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_ACTIVE_WIDTH_OFFSET (0x334)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_ACTIVE_WIDTH_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_active_width_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209334L);
    system_hw_write_32(0x209334L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture1_dma_writer_active_width_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209334L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: active height
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Active video height in lines 128-8000
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_ACTIVE_HEIGHT_DEFAULT (0x438)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_ACTIVE_HEIGHT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_ACTIVE_HEIGHT_OFFSET (0x338)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_ACTIVE_HEIGHT_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_active_height_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209338L);
    system_hw_write_32(0x209338L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture1_dma_writer_active_height_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209338L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: bank0_base
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// bank 0 base address for frame buffer, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BANK0_BASE_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BANK0_BASE_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BANK0_BASE_OFFSET (0x33c)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BANK0_BASE_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_bank0_base_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x20933cL, data);
}
static __inline uint32_t acamera_fpga_video_capture1_dma_writer_bank0_base_read(uintptr_t base) {
    return system_hw_read_32(0x20933cL);
}
// ------------------------------------------------------------------------------ //
// Register: bank1_base
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// bank 1 base address for frame buffer, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BANK1_BASE_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BANK1_BASE_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BANK1_BASE_OFFSET (0x340)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BANK1_BASE_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_bank1_base_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x209340L, data);
}
static __inline uint32_t acamera_fpga_video_capture1_dma_writer_bank1_base_read(uintptr_t base) {
    return system_hw_read_32(0x209340L);
}
// ------------------------------------------------------------------------------ //
// Register: bank2_base
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// bank 2 base address for frame buffer, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BANK2_BASE_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BANK2_BASE_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BANK2_BASE_OFFSET (0x344)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BANK2_BASE_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_bank2_base_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x209344L, data);
}
static __inline uint32_t acamera_fpga_video_capture1_dma_writer_bank2_base_read(uintptr_t base) {
    return system_hw_read_32(0x209344L);
}
// ------------------------------------------------------------------------------ //
// Register: bank3_base
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// bank 3 base address for frame buffer, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BANK3_BASE_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BANK3_BASE_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BANK3_BASE_OFFSET (0x348)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BANK3_BASE_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_bank3_base_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x209348L, data);
}
static __inline uint32_t acamera_fpga_video_capture1_dma_writer_bank3_base_read(uintptr_t base) {
    return system_hw_read_32(0x209348L);
}
// ------------------------------------------------------------------------------ //
// Register: bank4_base
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// bank 4 base address for frame buffer, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BANK4_BASE_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BANK4_BASE_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BANK4_BASE_OFFSET (0x34c)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BANK4_BASE_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_bank4_base_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x20934cL, data);
}
static __inline uint32_t acamera_fpga_video_capture1_dma_writer_bank4_base_read(uintptr_t base) {
    return system_hw_read_32(0x20934cL);
}
// ------------------------------------------------------------------------------ //
// Register: max bank
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// highest bank*_base to use for frame writes before recycling to bank0_base, only 0 to 4 are valid
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_MAX_BANK_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_MAX_BANK_DATASIZE (3)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_MAX_BANK_OFFSET (0x350)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_MAX_BANK_MASK (0x7)

// args: data (3-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_max_bank_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209350L);
    system_hw_write_32(0x209350L, (((uint32_t) (data & 0x7)) << 0) | (curr & 0xfffffff8));
}
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_max_bank_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209350L) & 0x7) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: bank0 restart
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0 = normal operation, 1= restart bank counter to bank0 for next frame write
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BANK0_RESTART_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BANK0_RESTART_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BANK0_RESTART_OFFSET (0x350)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BANK0_RESTART_MASK (0x8)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_bank0_restart_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209350L);
    system_hw_write_32(0x209350L, (((uint32_t) (data & 0x1)) << 3) | (curr & 0xfffffff7));
}
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_bank0_restart_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209350L) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: Line_offset
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//    Indicates the offset in bytes from the start of one line to the next line.
//    This value should be equal to or larger than one line of image data and should be word-aligned
//    
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_LINE_OFFSET_DEFAULT (0x4000)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_LINE_OFFSET_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_LINE_OFFSET_OFFSET (0x354)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_LINE_OFFSET_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_line_offset_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x209354L, data);
}
static __inline uint32_t acamera_fpga_video_capture1_dma_writer_line_offset_read(uintptr_t base) {
    return system_hw_read_32(0x209354L);
}
// ------------------------------------------------------------------------------ //
// Register: frame write cancel
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0 = normal operation, 1= cancel current/future frame write(s), any unstarted AXI bursts cancelled and fifo flushed
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_FRAME_WRITE_CANCEL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_FRAME_WRITE_CANCEL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_FRAME_WRITE_CANCEL_OFFSET (0x358)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_FRAME_WRITE_CANCEL_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_frame_write_cancel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209358L);
    system_hw_write_32(0x209358L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_frame_write_cancel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209358L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: axi_port_enable
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// enables axi, active high, 1=enables axi write transfers, 0= reset axi domain( via reset synchroniser)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_PORT_ENABLE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_PORT_ENABLE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_PORT_ENABLE_OFFSET (0x358)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_PORT_ENABLE_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_axi_port_enable_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209358L);
    system_hw_write_32(0x209358L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_axi_port_enable_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209358L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: wbank curr
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// write bank currently active. valid values =0-4. updated at start of frame write
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBANK_CURR_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBANK_CURR_DATASIZE (3)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBANK_CURR_OFFSET (0x35c)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBANK_CURR_MASK (0x7)

// args: data (3-bit)
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_wbank_curr_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20935cL) & 0x7) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: wbank last
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// write bank last active. valid values = 0-4. updated at start of frame write
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBANK_LAST_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBANK_LAST_DATASIZE (3)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBANK_LAST_OFFSET (0x35c)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBANK_LAST_MASK (0x38)

// args: data (3-bit)
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_wbank_last_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20935cL) & 0x38) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: wbank active
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1 = wbank_curr is being written to. Goes high at start of writes, low at last write transfer/completion on axi. 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBANK_ACTIVE_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBANK_ACTIVE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBANK_ACTIVE_OFFSET (0x360)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBANK_ACTIVE_MASK (0x1)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_wbank_active_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209360L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: wbank start
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1 = High pulse at start of frame write to bank. 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBANK_START_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBANK_START_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBANK_START_OFFSET (0x360)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBANK_START_MASK (0x2)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_wbank_start_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209360L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: wbank stop
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1 = High pulse at end of frame write to bank. 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBANK_STOP_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBANK_STOP_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBANK_STOP_OFFSET (0x360)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBANK_STOP_MASK (0x4)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_wbank_stop_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209360L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: wbase curr
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// currently active bank base addr - in bytes. updated at start of frame write
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBASE_CURR_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBASE_CURR_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBASE_CURR_OFFSET (0x364)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBASE_CURR_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture1_dma_writer_wbase_curr_read(uintptr_t base) {
    return system_hw_read_32(0x209364L);
}
// ------------------------------------------------------------------------------ //
// Register: wbase last
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// last active bank base addr - in bytes. Updated at start of frame write
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBASE_LAST_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBASE_LAST_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBASE_LAST_OFFSET (0x368)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WBASE_LAST_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture1_dma_writer_wbase_last_read(uintptr_t base) {
    return system_hw_read_32(0x209368L);
}
// ------------------------------------------------------------------------------ //
// Register: frame icount
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// count of incomming frames (starts) to vdma_writer on video input, non resetable, rolls over, updates at pixel 1 of new frame on video in
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_FRAME_ICOUNT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_FRAME_ICOUNT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_FRAME_ICOUNT_OFFSET (0x36c)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_FRAME_ICOUNT_MASK (0xffff)

// args: data (16-bit)
static __inline uint16_t acamera_fpga_video_capture1_dma_writer_frame_icount_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20936cL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: frame wcount
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// count of outgoing frame writes (starts) from vdma_writer sent to AXI output, non resetable, rolls over, updates at pixel 1 of new frame on video in
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_FRAME_WCOUNT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_FRAME_WCOUNT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_FRAME_WCOUNT_OFFSET (0x370)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_FRAME_WCOUNT_MASK (0xffff)

// args: data (16-bit)
static __inline uint16_t acamera_fpga_video_capture1_dma_writer_frame_wcount_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209370L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: clear alarms
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0>1 transition(synchronous detection) causes local axi/video alarm clear
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_CLEAR_ALARMS_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_CLEAR_ALARMS_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_CLEAR_ALARMS_OFFSET (0x374)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_CLEAR_ALARMS_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_clear_alarms_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209374L);
    system_hw_write_32(0x209374L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_clear_alarms_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209374L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: max_burst_length_is_8
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1= Reduce default AXI max_burst_length from 16 to 8, 0= Dont reduce
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_MAX_BURST_LENGTH_IS_8_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_MAX_BURST_LENGTH_IS_8_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_MAX_BURST_LENGTH_IS_8_OFFSET (0x374)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_MAX_BURST_LENGTH_IS_8_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_max_burst_length_is_8_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209374L);
    system_hw_write_32(0x209374L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_max_burst_length_is_8_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209374L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: max_burst_length_is_4
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1= Reduce default AXI max_burst_length from 16 to 4, 0= Dont reduce( has priority overmax_burst_length_is_8!)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_MAX_BURST_LENGTH_IS_4_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_MAX_BURST_LENGTH_IS_4_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_MAX_BURST_LENGTH_IS_4_OFFSET (0x374)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_MAX_BURST_LENGTH_IS_4_MASK (0x4)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_max_burst_length_is_4_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209374L);
    system_hw_write_32(0x209374L, (((uint32_t) (data & 0x1)) << 2) | (curr & 0xfffffffb));
}
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_max_burst_length_is_4_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209374L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: write timeout disable
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//    At end of frame an optional timeout is applied to wait for AXI writes to completed/accepted befotre caneclling and flushing.
//    0= Timeout Enabled, timeout count can decrement.
//    1 = Disable timeout, timeout count can't decrement.
//    
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WRITE_TIMEOUT_DISABLE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WRITE_TIMEOUT_DISABLE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WRITE_TIMEOUT_DISABLE_OFFSET (0x374)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WRITE_TIMEOUT_DISABLE_MASK (0x8)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_write_timeout_disable_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209374L);
    system_hw_write_32(0x209374L, (((uint32_t) (data & 0x1)) << 3) | (curr & 0xfffffff7));
}
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_write_timeout_disable_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209374L) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: awmaxwait_limit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// awvalid maxwait limit(cycles) to raise axi_fail_awmaxwait alarm . zero disables alarm raise.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AWMAXWAIT_LIMIT_DEFAULT (0x00)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AWMAXWAIT_LIMIT_DATASIZE (8)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AWMAXWAIT_LIMIT_OFFSET (0x378)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AWMAXWAIT_LIMIT_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_awmaxwait_limit_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209378L);
    system_hw_write_32(0x209378L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_awmaxwait_limit_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209378L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: wmaxwait_limit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// wvalid maxwait limit(cycles) to raise axi_fail_wmaxwait alarm . zero disables alarm raise
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WMAXWAIT_LIMIT_DEFAULT (0x00)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WMAXWAIT_LIMIT_DATASIZE (8)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WMAXWAIT_LIMIT_OFFSET (0x37c)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WMAXWAIT_LIMIT_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_wmaxwait_limit_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20937cL);
    system_hw_write_32(0x20937cL, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_wmaxwait_limit_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20937cL) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: wxact_ostand_limit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// number oustsanding write transactions(bursts)(responses..1 per burst) limit to raise axi_fail_wxact_ostand. zero disables alarm raise
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WXACT_OSTAND_LIMIT_DEFAULT (0x00)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WXACT_OSTAND_LIMIT_DATASIZE (8)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WXACT_OSTAND_LIMIT_OFFSET (0x380)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_WXACT_OSTAND_LIMIT_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_wxact_ostand_limit_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209380L);
    system_hw_write_32(0x209380L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_wxact_ostand_limit_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209380L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: axi_fail_bresp
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high to indicate bad  bresp captured 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_FAIL_BRESP_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_FAIL_BRESP_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_FAIL_BRESP_OFFSET (0x384)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_FAIL_BRESP_MASK (0x1)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_axi_fail_bresp_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209384L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: axi_fail_awmaxwait
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high when awmaxwait_limit reached 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_FAIL_AWMAXWAIT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_FAIL_AWMAXWAIT_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_FAIL_AWMAXWAIT_OFFSET (0x384)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_FAIL_AWMAXWAIT_MASK (0x2)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_axi_fail_awmaxwait_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209384L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: axi_fail_wmaxwait
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high when wmaxwait_limit reached 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_FAIL_WMAXWAIT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_FAIL_WMAXWAIT_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_FAIL_WMAXWAIT_OFFSET (0x384)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_FAIL_WMAXWAIT_MASK (0x4)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_axi_fail_wmaxwait_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209384L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: axi_fail_wxact_ostand
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high when wxact_ostand_limit reached 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_FAIL_WXACT_OSTAND_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_FAIL_WXACT_OSTAND_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_FAIL_WXACT_OSTAND_OFFSET (0x384)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_FAIL_WXACT_OSTAND_MASK (0x8)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_axi_fail_wxact_ostand_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209384L) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: vi_fail_active_width
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high to indicate mismatched active_width detected 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_VI_FAIL_ACTIVE_WIDTH_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_VI_FAIL_ACTIVE_WIDTH_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_VI_FAIL_ACTIVE_WIDTH_OFFSET (0x384)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_VI_FAIL_ACTIVE_WIDTH_MASK (0x10)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_vi_fail_active_width_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209384L) & 0x10) >> 4);
}
// ------------------------------------------------------------------------------ //
// Register: vi_fail_active_height
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high to indicate mismatched active_height detected ( also raised on missing field!) 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_VI_FAIL_ACTIVE_HEIGHT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_VI_FAIL_ACTIVE_HEIGHT_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_VI_FAIL_ACTIVE_HEIGHT_OFFSET (0x384)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_VI_FAIL_ACTIVE_HEIGHT_MASK (0x20)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_vi_fail_active_height_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209384L) & 0x20) >> 5);
}
// ------------------------------------------------------------------------------ //
// Register: vi_fail_interline_blanks
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high to indicate interline blanking below min 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_VI_FAIL_INTERLINE_BLANKS_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_VI_FAIL_INTERLINE_BLANKS_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_VI_FAIL_INTERLINE_BLANKS_OFFSET (0x384)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_VI_FAIL_INTERLINE_BLANKS_MASK (0x40)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_vi_fail_interline_blanks_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209384L) & 0x40) >> 6);
}
// ------------------------------------------------------------------------------ //
// Register: vi_fail_interframe_blanks
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high to indicate interframe blanking below min 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_VI_FAIL_INTERFRAME_BLANKS_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_VI_FAIL_INTERFRAME_BLANKS_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_VI_FAIL_INTERFRAME_BLANKS_OFFSET (0x384)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_VI_FAIL_INTERFRAME_BLANKS_MASK (0x80)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_vi_fail_interframe_blanks_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209384L) & 0x80) >> 7);
}
// ------------------------------------------------------------------------------ //
// Register: video_alarm
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  active high, problem found on video port(s) ( active width/height or interline/frame blanks failure) 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_VIDEO_ALARM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_VIDEO_ALARM_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_VIDEO_ALARM_OFFSET (0x388)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_VIDEO_ALARM_MASK (0x1)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_video_alarm_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209388L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: axi_alarm
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  active high, problem found on axi port(s)( bresp or awmaxwait or wmaxwait or wxact_ostand failure ) 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_ALARM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_ALARM_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_ALARM_OFFSET (0x388)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_AXI_ALARM_MASK (0x2)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture1_dma_writer_axi_alarm_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209388L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: blk_config
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// block configuration (reserved)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BLK_CONFIG_DEFAULT (0x0000)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BLK_CONFIG_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BLK_CONFIG_OFFSET (0x38c)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BLK_CONFIG_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture1_dma_writer_blk_config_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x20938cL, data);
}
static __inline uint32_t acamera_fpga_video_capture1_dma_writer_blk_config_read(uintptr_t base) {
    return system_hw_read_32(0x20938cL);
}
// ------------------------------------------------------------------------------ //
// Register: blk_status
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// block status output (reserved)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BLK_STATUS_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BLK_STATUS_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BLK_STATUS_OFFSET (0x390)
#define ACAMERA_FPGA_VIDEO_CAPTURE1_DMA_WRITER_BLK_STATUS_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture1_dma_writer_blk_status_read(uintptr_t base) {
    return system_hw_read_32(0x209390L);
}
// ------------------------------------------------------------------------------ //
// Group: Sony IMX290 Decoder
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Settings related to sensor decoder and its peripherals
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: FIFO Watermark
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// FIFO Data count at which decoder starts streaming data out
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_SENSOR_FIFO_WATERMARK_DEFAULT (0)
#define ACAMERA_FPGA_SENSOR_FIFO_WATERMARK_DATASIZE (16)
#define ACAMERA_FPGA_SENSOR_FIFO_WATERMARK_OFFSET (0x394)
#define ACAMERA_FPGA_SENSOR_FIFO_WATERMARK_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_sensor_fifo_watermark_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209394L);
    system_hw_write_32(0x209394L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_sensor_fifo_watermark_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209394L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Enable 4 Channel
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1=4 Channel Mode, 0=8 Channel Mode
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_SENSOR_ENABLE_4_CHANNEL_DEFAULT (0)
#define ACAMERA_FPGA_SENSOR_ENABLE_4_CHANNEL_DATASIZE (1)
#define ACAMERA_FPGA_SENSOR_ENABLE_4_CHANNEL_OFFSET (0x398)
#define ACAMERA_FPGA_SENSOR_ENABLE_4_CHANNEL_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_sensor_enable_4_channel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209398L);
    system_hw_write_32(0x209398L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_sensor_enable_4_channel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209398L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Enable 10 Bit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1=10-bit Mode, 0=12-bit Mode
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_SENSOR_ENABLE_10_BIT_DEFAULT (0)
#define ACAMERA_FPGA_SENSOR_ENABLE_10_BIT_DATASIZE (1)
#define ACAMERA_FPGA_SENSOR_ENABLE_10_BIT_OFFSET (0x398)
#define ACAMERA_FPGA_SENSOR_ENABLE_10_BIT_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_fpga_sensor_enable_10_bit_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209398L);
    system_hw_write_32(0x209398L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_sensor_enable_10_bit_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209398L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: Sensor Reset_n
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1=XCLR (System Clear) pin of IMX290
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_SENSOR_SENSOR_RESET_N_DEFAULT (1)
#define ACAMERA_FPGA_SENSOR_SENSOR_RESET_N_DATASIZE (1)
#define ACAMERA_FPGA_SENSOR_SENSOR_RESET_N_OFFSET (0x398)
#define ACAMERA_FPGA_SENSOR_SENSOR_RESET_N_MASK (0x4)

// args: data (1-bit)
static __inline void acamera_fpga_sensor_sensor_reset_n_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209398L);
    system_hw_write_32(0x209398L, (((uint32_t) (data & 0x1)) << 2) | (curr & 0xfffffffb));
}
static __inline uint8_t acamera_fpga_sensor_sensor_reset_n_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209398L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: Start Auto-Calibration FSM - 0123
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Start auto calibration state machine by setting this flag after initialisation of the sensor is complete
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_SENSOR_START_AUTO_CALIBRATION_FSM___0123_DEFAULT (0)
#define ACAMERA_FPGA_SENSOR_START_AUTO_CALIBRATION_FSM___0123_DATASIZE (1)
#define ACAMERA_FPGA_SENSOR_START_AUTO_CALIBRATION_FSM___0123_OFFSET (0x398)
#define ACAMERA_FPGA_SENSOR_START_AUTO_CALIBRATION_FSM___0123_MASK (0x8)

// args: data (1-bit)
static __inline void acamera_fpga_sensor_start_auto_calibration_fsm___0123_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209398L);
    system_hw_write_32(0x209398L, (((uint32_t) (data & 0x1)) << 3) | (curr & 0xfffffff7));
}
static __inline uint8_t acamera_fpga_sensor_start_auto_calibration_fsm___0123_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209398L) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: Auto Calibration Done - 0123
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Implies that auto calibration is successful
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_SENSOR_AUTO_CALIBRATION_DONE___0123_DEFAULT (0)
#define ACAMERA_FPGA_SENSOR_AUTO_CALIBRATION_DONE___0123_DATASIZE (1)
#define ACAMERA_FPGA_SENSOR_AUTO_CALIBRATION_DONE___0123_OFFSET (0x398)
#define ACAMERA_FPGA_SENSOR_AUTO_CALIBRATION_DONE___0123_MASK (0x10)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_sensor_auto_calibration_done___0123_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209398L) & 0x10) >> 4);
}
// ------------------------------------------------------------------------------ //
// Register: Start Auto-Calibration FSM - 4567
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Start auto calibration state machine by setting this flag after initialisation of the sensor is complete
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_SENSOR_START_AUTO_CALIBRATION_FSM___4567_DEFAULT (0)
#define ACAMERA_FPGA_SENSOR_START_AUTO_CALIBRATION_FSM___4567_DATASIZE (1)
#define ACAMERA_FPGA_SENSOR_START_AUTO_CALIBRATION_FSM___4567_OFFSET (0x398)
#define ACAMERA_FPGA_SENSOR_START_AUTO_CALIBRATION_FSM___4567_MASK (0x20)

// args: data (1-bit)
static __inline void acamera_fpga_sensor_start_auto_calibration_fsm___4567_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209398L);
    system_hw_write_32(0x209398L, (((uint32_t) (data & 0x1)) << 5) | (curr & 0xffffffdf));
}
static __inline uint8_t acamera_fpga_sensor_start_auto_calibration_fsm___4567_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209398L) & 0x20) >> 5);
}
// ------------------------------------------------------------------------------ //
// Register: Auto Calibration Done - 4567
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Implies that auto calibration is successful
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_SENSOR_AUTO_CALIBRATION_DONE___4567_DEFAULT (0)
#define ACAMERA_FPGA_SENSOR_AUTO_CALIBRATION_DONE___4567_DATASIZE (1)
#define ACAMERA_FPGA_SENSOR_AUTO_CALIBRATION_DONE___4567_OFFSET (0x398)
#define ACAMERA_FPGA_SENSOR_AUTO_CALIBRATION_DONE___4567_MASK (0x40)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_sensor_auto_calibration_done___4567_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209398L) & 0x40) >> 6);
}
// ------------------------------------------------------------------------------ //
// Register: calib_maxwait0123
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_SENSOR_CALIB_MAXWAIT0123_DEFAULT (0x000FFFFF)
#define ACAMERA_FPGA_SENSOR_CALIB_MAXWAIT0123_DATASIZE (32)
#define ACAMERA_FPGA_SENSOR_CALIB_MAXWAIT0123_OFFSET (0x39c)
#define ACAMERA_FPGA_SENSOR_CALIB_MAXWAIT0123_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_sensor_calib_maxwait0123_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x20939cL, data);
}
static __inline uint32_t acamera_fpga_sensor_calib_maxwait0123_read(uintptr_t base) {
    return system_hw_read_32(0x20939cL);
}
// ------------------------------------------------------------------------------ //
// Register: calib_maxwait4567
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_SENSOR_CALIB_MAXWAIT4567_DEFAULT (0x000FFFFF)
#define ACAMERA_FPGA_SENSOR_CALIB_MAXWAIT4567_DATASIZE (32)
#define ACAMERA_FPGA_SENSOR_CALIB_MAXWAIT4567_OFFSET (0x3a0)
#define ACAMERA_FPGA_SENSOR_CALIB_MAXWAIT4567_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_sensor_calib_maxwait4567_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x2093a0L, data);
}
static __inline uint32_t acamera_fpga_sensor_calib_maxwait4567_read(uintptr_t base) {
    return system_hw_read_32(0x2093a0L);
}
// ------------------------------------------------------------------------------ //
// Group: FPGA Input Port
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Controls video input port
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: preset
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0-14 frequently used presets, 15 - full control
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_PRESET_DEFAULT (2)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_PRESET_DATASIZE (4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_PRESET_OFFSET (0x3a4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_PRESET_MASK (0xf)

// args: data (4-bit)
static __inline void acamera_fpga_fpga_input_port_preset_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a4L);
    system_hw_write_32(0x2093a4L, (((uint32_t) (data & 0xf)) << 0) | (curr & 0xfffffff0));
}
static __inline uint8_t acamera_fpga_fpga_input_port_preset_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a4L) & 0xf) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: vs_use field
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_VS_USE_FIELD_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VS_USE_FIELD_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VS_USE_FIELD_OFFSET (0x3a4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VS_USE_FIELD_MASK (0x100)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VS_USE_FIELD_USE_VSYNC_I_PORT_FOR_VERTICAL_SYNC (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VS_USE_FIELD_USE_FIELD_I_PORT_FOR_VERTICAL_SYNC (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_vs_use_field_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a4L);
    system_hw_write_32(0x2093a4L, (((uint32_t) (data & 0x1)) << 8) | (curr & 0xfffffeff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_vs_use_field_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a4L) & 0x100) >> 8);
}
// ------------------------------------------------------------------------------ //
// Register: vs toggle
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_VS_TOGGLE_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VS_TOGGLE_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VS_TOGGLE_OFFSET (0x3a4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VS_TOGGLE_MASK (0x200)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VS_TOGGLE_VSYNC_IS_PULSETYPE (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VS_TOGGLE_VSYNC_IS_TOGGLETYPE_FIELD_SIGNAL (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_vs_toggle_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a4L);
    system_hw_write_32(0x2093a4L, (((uint32_t) (data & 0x1)) << 9) | (curr & 0xfffffdff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_vs_toggle_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a4L) & 0x200) >> 9);
}
// ------------------------------------------------------------------------------ //
// Register: vs polarity
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_VS_POLARITY_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VS_POLARITY_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VS_POLARITY_OFFSET (0x3a4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VS_POLARITY_MASK (0x400)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VS_POLARITY_HORIZONTAL_COUNTER_RESET_ON_RISING_EDGE (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VS_POLARITY_HORIZONTAL_COUNTER_RESET_ON_FALLING_EDGE (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_vs_polarity_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a4L);
    system_hw_write_32(0x2093a4L, (((uint32_t) (data & 0x1)) << 10) | (curr & 0xfffffbff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_vs_polarity_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a4L) & 0x400) >> 10);
}
// ------------------------------------------------------------------------------ //
// Register: vs_polarity acl
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_VS_POLARITY_ACL_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VS_POLARITY_ACL_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VS_POLARITY_ACL_OFFSET (0x3a4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VS_POLARITY_ACL_MASK (0x800)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VS_POLARITY_ACL_DONT_INVERT_POLARITY_FOR_ACL_GATE (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VS_POLARITY_ACL_INVERT_POLARITY_FOR_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_vs_polarity_acl_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a4L);
    system_hw_write_32(0x2093a4L, (((uint32_t) (data & 0x1)) << 11) | (curr & 0xfffff7ff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_vs_polarity_acl_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a4L) & 0x800) >> 11);
}
// ------------------------------------------------------------------------------ //
// Register: hs_use acl
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_USE_ACL_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_USE_ACL_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_USE_ACL_OFFSET (0x3a4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_USE_ACL_MASK (0x1000)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_USE_ACL_USE_HSYNC_I_PORT_FOR_ACTIVELINE (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_USE_ACL_USE_ACL_I_PORT_FOR_ACTIVELINE (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_hs_use_acl_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a4L);
    system_hw_write_32(0x2093a4L, (((uint32_t) (data & 0x1)) << 12) | (curr & 0xffffefff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_hs_use_acl_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a4L) & 0x1000) >> 12);
}
// ------------------------------------------------------------------------------ //
// Register: vc_c select
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_VC_C_SELECT_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VC_C_SELECT_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VC_C_SELECT_OFFSET (0x3a4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VC_C_SELECT_MASK (0x4000)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VC_C_SELECT_VERTICAL_COUNTER_COUNTS_ON_HS (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VC_C_SELECT_VERTICAL_COUNTER_COUNTS_ON_HORIZONTAL_COUNTER_OVERFLOW_OR_RESET (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_vc_c_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a4L);
    system_hw_write_32(0x2093a4L, (((uint32_t) (data & 0x1)) << 14) | (curr & 0xffffbfff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_vc_c_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a4L) & 0x4000) >> 14);
}
// ------------------------------------------------------------------------------ //
// Register: vc_r select
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_VC_R_SELECT_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VC_R_SELECT_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VC_R_SELECT_OFFSET (0x3a4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VC_R_SELECT_MASK (0x8000)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VC_R_SELECT_VERTICAL_COUNTER_IS_RESET_ON_EDGE_OF_VS (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VC_R_SELECT_VERTICAL_COUNTER_IS_RESET_AFTER_TIMEOUT_ON_HS (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_vc_r_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a4L);
    system_hw_write_32(0x2093a4L, (((uint32_t) (data & 0x1)) << 15) | (curr & 0xffff7fff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_vc_r_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a4L) & 0x8000) >> 15);
}
// ------------------------------------------------------------------------------ //
// Register: hs_xor vs
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_XOR_VS_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_XOR_VS_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_XOR_VS_OFFSET (0x3a4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_XOR_VS_MASK (0x10000)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_XOR_VS_NORMAL_MODE (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_XOR_VS_HVALID__HSYNC_XOR_VSYNC (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_hs_xor_vs_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a4L);
    system_hw_write_32(0x2093a4L, (((uint32_t) (data & 0x1)) << 16) | (curr & 0xfffeffff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_hs_xor_vs_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a4L) & 0x10000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: hs polarity
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_POLARITY_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_POLARITY_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_POLARITY_OFFSET (0x3a4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_POLARITY_MASK (0x20000)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_POLARITY_DONT_INVERT_POLARITY_OF_HS_FOR_ACL_GATE (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_POLARITY_INVERT_POLARITY_OF_HS_FOR_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_hs_polarity_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a4L);
    system_hw_write_32(0x2093a4L, (((uint32_t) (data & 0x1)) << 17) | (curr & 0xfffdffff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_hs_polarity_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a4L) & 0x20000) >> 17);
}
// ------------------------------------------------------------------------------ //
// Register: hs_polarity acl
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_POLARITY_ACL_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_POLARITY_ACL_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_POLARITY_ACL_OFFSET (0x3a4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_POLARITY_ACL_MASK (0x40000)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_POLARITY_ACL_DONT_INVERT_POLARITY_OF_HS_FOR_HS_GATE (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_POLARITY_ACL_INVERT_POLARITY_OF_HS_FOR_HS_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_hs_polarity_acl_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a4L);
    system_hw_write_32(0x2093a4L, (((uint32_t) (data & 0x1)) << 18) | (curr & 0xfffbffff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_hs_polarity_acl_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a4L) & 0x40000) >> 18);
}
// ------------------------------------------------------------------------------ //
// Register: hs_polarity hs
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_POLARITY_HS_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_POLARITY_HS_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_POLARITY_HS_OFFSET (0x3a4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_POLARITY_HS_MASK (0x80000)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_POLARITY_HS_HORIZONTAL_COUNTER_IS_RESET_ON_RISING_EDGE_OF_HS (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_POLARITY_HS_HORIZONTAL_COUNTER_IS_RESET_ON_VSYNC_EG_WHEN_HSYNC_IS_NOT_AVAILABLE (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_hs_polarity_hs_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a4L);
    system_hw_write_32(0x2093a4L, (((uint32_t) (data & 0x1)) << 19) | (curr & 0xfff7ffff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_hs_polarity_hs_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a4L) & 0x80000) >> 19);
}
// ------------------------------------------------------------------------------ //
// Register: hs_polarity vc
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_POLARITY_VC_DEFAULT (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_POLARITY_VC_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_POLARITY_VC_OFFSET (0x3a4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_POLARITY_VC_MASK (0x100000)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_POLARITY_VC_VERTICAL_COUNTER_INCREMENTS_ON_RISING_EDGE_OF_HS (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HS_POLARITY_VC_VERTICAL_COUNTER_INCREMENTS_ON_FALLING_EDGE_OF_HS (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_hs_polarity_vc_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a4L);
    system_hw_write_32(0x2093a4L, (((uint32_t) (data & 0x1)) << 20) | (curr & 0xffefffff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_hs_polarity_vc_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a4L) & 0x100000) >> 20);
}
// ------------------------------------------------------------------------------ //
// Register: hc_r select
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_R_SELECT_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_R_SELECT_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_R_SELECT_OFFSET (0x3a4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_R_SELECT_MASK (0x800000)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_R_SELECT_VERTICAL_COUNTER_IS_RESET_ON_RISING_EDGE_OF_HS (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_R_SELECT_VERTICAL_COUNTER_IS_RESET_ON_RISING_EDGE_OF_VS (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_hc_r_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a4L);
    system_hw_write_32(0x2093a4L, (((uint32_t) (data & 0x1)) << 23) | (curr & 0xff7fffff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_hc_r_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a4L) & 0x800000) >> 23);
}
// ------------------------------------------------------------------------------ //
// Register: acl polarity
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACL_POLARITY_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACL_POLARITY_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACL_POLARITY_OFFSET (0x3a4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACL_POLARITY_MASK (0x1000000)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACL_POLARITY_DONT_INVERT_ACL_I_FOR_ACL_GATE (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACL_POLARITY_INVERT_ACL_I_FOR_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_acl_polarity_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a4L);
    system_hw_write_32(0x2093a4L, (((uint32_t) (data & 0x1)) << 24) | (curr & 0xfeffffff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_acl_polarity_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a4L) & 0x1000000) >> 24);
}
// ------------------------------------------------------------------------------ //
// Register: sid_latch_hs_pos
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_SID_LATCH_HS_POS_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_SID_LATCH_HS_POS_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_SID_LATCH_HS_POS_OFFSET (0x3a4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_SID_LATCH_HS_POS_MASK (0x10000000)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_SID_LATCH_HS_POS_DONT_LATCH_STREAM_ID_ON_RISING_EDGE_OF_HS (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_SID_LATCH_HS_POS_LATCH_STREAM_ID_ON_RISING_EDGE_OF_HS (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_sid_latch_hs_pos_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a4L);
    system_hw_write_32(0x2093a4L, (((uint32_t) (data & 0x1)) << 28) | (curr & 0xefffffff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_sid_latch_hs_pos_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a4L) & 0x10000000) >> 28);
}
// ------------------------------------------------------------------------------ //
// Register: sid_latch_hs_neg
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_SID_LATCH_HS_NEG_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_SID_LATCH_HS_NEG_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_SID_LATCH_HS_NEG_OFFSET (0x3a4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_SID_LATCH_HS_NEG_MASK (0x20000000)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_SID_LATCH_HS_NEG_DONT_LATCH_STREAM_ID_ON_FALLING_EDGE_OF_HS (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_SID_LATCH_HS_NEG_LATCH_STREAM_ID_ON_FALLING_EDGE_OF_HS (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_sid_latch_hs_neg_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a4L);
    system_hw_write_32(0x2093a4L, (((uint32_t) (data & 0x1)) << 29) | (curr & 0xdfffffff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_sid_latch_hs_neg_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a4L) & 0x20000000) >> 29);
}
// ------------------------------------------------------------------------------ //
// Register: sid_latch_acl_pos
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_SID_LATCH_ACL_POS_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_SID_LATCH_ACL_POS_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_SID_LATCH_ACL_POS_OFFSET (0x3a4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_SID_LATCH_ACL_POS_MASK (0x40000000)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_SID_LATCH_ACL_POS_DONT_LATCH_STREAM_ID_ON_RISING_EDGE_OF_ACL (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_SID_LATCH_ACL_POS_LATCH_STREAM_ID_ON_RISING_EDGE_OF_ACL (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_sid_latch_acl_pos_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a4L);
    system_hw_write_32(0x2093a4L, (((uint32_t) (data & 0x1)) << 30) | (curr & 0xbfffffff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_sid_latch_acl_pos_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a4L) & 0x40000000) >> 30);
}
// ------------------------------------------------------------------------------ //
// Register: sid_latch_acl_neg
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_SID_LATCH_ACL_NEG_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_SID_LATCH_ACL_NEG_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_SID_LATCH_ACL_NEG_OFFSET (0x3a4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_SID_LATCH_ACL_NEG_MASK (0x80000000)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_SID_LATCH_ACL_NEG_DONT_LATCH_STREAM_ID_ON_FALLING_EDGE_OF_ACL (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_SID_LATCH_ACL_NEG_LATCH_STREAM_ID_ON_FALLING_EDGE_OF_ACL (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_sid_latch_acl_neg_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a4L);
    system_hw_write_32(0x2093a4L, (((uint32_t) (data & 0x1)) << 31) | (curr & 0x7fffffff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_sid_latch_acl_neg_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a4L) & 0x80000000) >> 31);
}
// ------------------------------------------------------------------------------ //
// Register: field polarity
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELD_POLARITY_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELD_POLARITY_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELD_POLARITY_OFFSET (0x3a8)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELD_POLARITY_MASK (0x1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELD_POLARITY_DONT_INVERT_FIELD_I_FOR_FIELD_GATE (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELD_POLARITY_INVERT_FIELD_I_FOR_FIELD_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_field_polarity_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a8L);
    system_hw_write_32(0x2093a8L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_fpga_input_port_field_polarity_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a8L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: field toggle
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELD_TOGGLE_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELD_TOGGLE_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELD_TOGGLE_OFFSET (0x3a8)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELD_TOGGLE_MASK (0x2)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELD_TOGGLE_FIELD_IS_PULSETYPE (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELD_TOGGLE_FIELD_IS_TOGGLETYPE (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_field_toggle_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a8L);
    system_hw_write_32(0x2093a8L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_fpga_input_port_field_toggle_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a8L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: aclg window0
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_WINDOW0_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_WINDOW0_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_WINDOW0_OFFSET (0x3a8)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_WINDOW0_MASK (0x100)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_WINDOW0_EXCLUDE_WINDOW0_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_WINDOW0_INCLUDE_WINDOW0_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_aclg_window0_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a8L);
    system_hw_write_32(0x2093a8L, (((uint32_t) (data & 0x1)) << 8) | (curr & 0xfffffeff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_aclg_window0_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a8L) & 0x100) >> 8);
}
// ------------------------------------------------------------------------------ //
// Register: aclg hsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_HSYNC_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_HSYNC_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_HSYNC_OFFSET (0x3a8)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_HSYNC_MASK (0x200)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_HSYNC_EXCLUDE_HSYNC_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_HSYNC_INCLUDE_HSYNC_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_aclg_hsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a8L);
    system_hw_write_32(0x2093a8L, (((uint32_t) (data & 0x1)) << 9) | (curr & 0xfffffdff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_aclg_hsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a8L) & 0x200) >> 9);
}
// ------------------------------------------------------------------------------ //
// Register: aclg window2
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_WINDOW2_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_WINDOW2_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_WINDOW2_OFFSET (0x3a8)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_WINDOW2_MASK (0x400)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_WINDOW2_EXCLUDE_WINDOW2_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_WINDOW2_INCLUDE_WINDOW2_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_aclg_window2_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a8L);
    system_hw_write_32(0x2093a8L, (((uint32_t) (data & 0x1)) << 10) | (curr & 0xfffffbff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_aclg_window2_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a8L) & 0x400) >> 10);
}
// ------------------------------------------------------------------------------ //
// Register: aclg acl
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_ACL_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_ACL_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_ACL_OFFSET (0x3a8)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_ACL_MASK (0x800)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_ACL_EXCLUDE_ACL_I_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_ACL_INCLUDE_ACL_I_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_aclg_acl_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a8L);
    system_hw_write_32(0x2093a8L, (((uint32_t) (data & 0x1)) << 11) | (curr & 0xfffff7ff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_aclg_acl_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a8L) & 0x800) >> 11);
}
// ------------------------------------------------------------------------------ //
// Register: aclg vsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_VSYNC_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_VSYNC_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_VSYNC_OFFSET (0x3a8)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_VSYNC_MASK (0x1000)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_VSYNC_EXCLUDE_VSYNC_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_ACLG_VSYNC_INCLUDE_VSYNC_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_aclg_vsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a8L);
    system_hw_write_32(0x2093a8L, (((uint32_t) (data & 0x1)) << 12) | (curr & 0xffffefff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_aclg_vsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a8L) & 0x1000) >> 12);
}
// ------------------------------------------------------------------------------ //
// Register: hsg window1
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_HSG_WINDOW1_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HSG_WINDOW1_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HSG_WINDOW1_OFFSET (0x3a8)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HSG_WINDOW1_MASK (0x10000)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HSG_WINDOW1_EXCLUDE_WINDOW1_SIGNAL_IN_HS_GATE (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HSG_WINDOW1_INCLUDE_WINDOW1_SIGNAL_IN_HS_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_hsg_window1_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a8L);
    system_hw_write_32(0x2093a8L, (((uint32_t) (data & 0x1)) << 16) | (curr & 0xfffeffff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_hsg_window1_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a8L) & 0x10000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: hsg hsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_HSG_HSYNC_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HSG_HSYNC_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HSG_HSYNC_OFFSET (0x3a8)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HSG_HSYNC_MASK (0x20000)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HSG_HSYNC_EXCLUDE_HSYNC_SIGNAL_IN_HS_GATE (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HSG_HSYNC_INCLUDE_HSYNC_SIGNAL_IN_HS_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_hsg_hsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a8L);
    system_hw_write_32(0x2093a8L, (((uint32_t) (data & 0x1)) << 17) | (curr & 0xfffdffff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_hsg_hsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a8L) & 0x20000) >> 17);
}
// ------------------------------------------------------------------------------ //
// Register: hsg vsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_HSG_VSYNC_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HSG_VSYNC_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HSG_VSYNC_OFFSET (0x3a8)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HSG_VSYNC_MASK (0x40000)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HSG_VSYNC_EXCLUDE_VSYNC_SIGNAL_IN_HS_GATE (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HSG_VSYNC_INCLUDE_VSYNC_SIGNAL_IN_HS_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_hsg_vsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a8L);
    system_hw_write_32(0x2093a8L, (((uint32_t) (data & 0x1)) << 18) | (curr & 0xfffbffff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_hsg_vsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a8L) & 0x40000) >> 18);
}
// ------------------------------------------------------------------------------ //
// Register: hsg window2
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_HSG_WINDOW2_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HSG_WINDOW2_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HSG_WINDOW2_OFFSET (0x3a8)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HSG_WINDOW2_MASK (0x80000)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HSG_WINDOW2_EXCLUDE_WINDOW2_SIGNAL_IN_HS_GATE (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HSG_WINDOW2_INCLUDE_WINDOW2_SIGNAL_IN_HS_GATE_MASK_OUT_SPURIOUS_HS_DURING_BLANK (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_hsg_window2_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a8L);
    system_hw_write_32(0x2093a8L, (((uint32_t) (data & 0x1)) << 19) | (curr & 0xfff7ffff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_hsg_window2_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a8L) & 0x80000) >> 19);
}
// ------------------------------------------------------------------------------ //
// Register: fieldg vsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELDG_VSYNC_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELDG_VSYNC_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELDG_VSYNC_OFFSET (0x3a8)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELDG_VSYNC_MASK (0x1000000)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELDG_VSYNC_EXCLUDE_VSYNC_SIGNAL_IN_FIELD_GATE (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELDG_VSYNC_INCLUDE_VSYNC_SIGNAL_IN_FIELD_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_fieldg_vsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a8L);
    system_hw_write_32(0x2093a8L, (((uint32_t) (data & 0x1)) << 24) | (curr & 0xfeffffff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_fieldg_vsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a8L) & 0x1000000) >> 24);
}
// ------------------------------------------------------------------------------ //
// Register: fieldg window2
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELDG_WINDOW2_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELDG_WINDOW2_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELDG_WINDOW2_OFFSET (0x3a8)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELDG_WINDOW2_MASK (0x2000000)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELDG_WINDOW2_EXCLUDE_WINDOW2_SIGNAL_IN_FIELD_GATE (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELDG_WINDOW2_INCLUDE_WINDOW2_SIGNAL_IN_FIELD_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_fieldg_window2_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a8L);
    system_hw_write_32(0x2093a8L, (((uint32_t) (data & 0x1)) << 25) | (curr & 0xfdffffff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_fieldg_window2_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a8L) & 0x2000000) >> 25);
}
// ------------------------------------------------------------------------------ //
// Register: fieldg field
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELDG_FIELD_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELDG_FIELD_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELDG_FIELD_OFFSET (0x3a8)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELDG_FIELD_MASK (0x4000000)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELDG_FIELD_EXCLUDE_FIELD_I_SIGNAL_IN_FIELD_GATE (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELDG_FIELD_INCLUDE_FIELD_I_SIGNAL_IN_FIELD_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_fieldg_field_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a8L);
    system_hw_write_32(0x2093a8L, (((uint32_t) (data & 0x1)) << 26) | (curr & 0xfbffffff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_fieldg_field_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a8L) & 0x4000000) >> 26);
}
// ------------------------------------------------------------------------------ //
// Register: field mode
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELD_MODE_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELD_MODE_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELD_MODE_OFFSET (0x3a8)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELD_MODE_MASK (0x8000000)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELD_MODE_PULSE_FIELD (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FIELD_MODE_TOGGLE_FIELD (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_field_mode_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093a8L);
    system_hw_write_32(0x2093a8L, (((uint32_t) (data & 0x1)) << 27) | (curr & 0xf7ffffff));
}
static __inline uint8_t acamera_fpga_fpga_input_port_field_mode_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093a8L) & 0x8000000) >> 27);
}
// ------------------------------------------------------------------------------ //
// Register: hc limit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// horizontal counter limit value (counts: 0,1,...hc_limit-1,hc_limit,0,1,...)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_LIMIT_DEFAULT (0xFFFF)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_LIMIT_DATASIZE (16)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_LIMIT_OFFSET (0x3ac)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_LIMIT_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_fpga_input_port_hc_limit_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2093acL);
    system_hw_write_32(0x2093acL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_fpga_input_port_hc_limit_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2093acL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: hc start0
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window0 start for ACL gate
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_START0_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_START0_DATASIZE (16)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_START0_OFFSET (0x3b0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_START0_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_fpga_input_port_hc_start0_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2093b0L);
    system_hw_write_32(0x2093b0L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_fpga_input_port_hc_start0_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2093b0L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: hc size0
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window0 size for ACL gate
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_SIZE0_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_SIZE0_DATASIZE (16)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_SIZE0_OFFSET (0x3b4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_SIZE0_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_fpga_input_port_hc_size0_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2093b4L);
    system_hw_write_32(0x2093b4L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_fpga_input_port_hc_size0_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2093b4L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: hc start1
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window1 start for HS gate
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_START1_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_START1_DATASIZE (16)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_START1_OFFSET (0x3b8)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_START1_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_fpga_input_port_hc_start1_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2093b8L);
    system_hw_write_32(0x2093b8L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_fpga_input_port_hc_start1_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2093b8L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: hc size1
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window1 size for HS gate
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_SIZE1_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_SIZE1_DATASIZE (16)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_SIZE1_OFFSET (0x3bc)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_HC_SIZE1_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_fpga_input_port_hc_size1_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2093bcL);
    system_hw_write_32(0x2093bcL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_fpga_input_port_hc_size1_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2093bcL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: vc limit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// vertical counter limit value (counts: 0,1,...vc_limit-1,vc_limit,0,1,...)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_VC_LIMIT_DEFAULT (0xFFFF)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VC_LIMIT_DATASIZE (16)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VC_LIMIT_OFFSET (0x3c0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VC_LIMIT_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_fpga_input_port_vc_limit_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2093c0L);
    system_hw_write_32(0x2093c0L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_fpga_input_port_vc_limit_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2093c0L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: vc start
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window2 start for ACL gate
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_VC_START_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VC_START_DATASIZE (16)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VC_START_OFFSET (0x3c4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VC_START_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_fpga_input_port_vc_start_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2093c4L);
    system_hw_write_32(0x2093c4L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_fpga_input_port_vc_start_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2093c4L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: vc size
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window2 size for ACL gate
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_VC_SIZE_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VC_SIZE_DATASIZE (16)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VC_SIZE_OFFSET (0x3c8)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_VC_SIZE_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_fpga_input_port_vc_size_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2093c8L);
    system_hw_write_32(0x2093c8L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_fpga_input_port_vc_size_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2093c8L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: frame width
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// detected frame width
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_FRAME_WIDTH_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FRAME_WIDTH_DATASIZE (16)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FRAME_WIDTH_OFFSET (0x3cc)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FRAME_WIDTH_MASK (0xffff)

// args: data (16-bit)
static __inline uint16_t acamera_fpga_fpga_input_port_frame_width_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2093ccL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: frame height
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// detected frame height
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_FRAME_HEIGHT_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FRAME_HEIGHT_DATASIZE (16)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FRAME_HEIGHT_OFFSET (0x3d0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FRAME_HEIGHT_MASK (0xffff)

// args: data (16-bit)
static __inline uint16_t acamera_fpga_fpga_input_port_frame_height_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2093d0L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: freeze config
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_FREEZE_CONFIG_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FREEZE_CONFIG_DATASIZE (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FREEZE_CONFIG_OFFSET (0x3d4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FREEZE_CONFIG_MASK (0x80)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FREEZE_CONFIG_NORMAL_OPERATION (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_FREEZE_CONFIG_HOLD_PREVIOUS_CONFIG_STATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_fpga_input_port_freeze_config_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093d4L);
    system_hw_write_32(0x2093d4L, (((uint32_t) (data & 0x1)) << 7) | (curr & 0xffffff7f));
}
static __inline uint8_t acamera_fpga_fpga_input_port_freeze_config_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093d4L) & 0x80) >> 7);
}
// ------------------------------------------------------------------------------ //
// Register: mode request
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_MODE_REQUEST_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_MODE_REQUEST_DATASIZE (3)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_MODE_REQUEST_OFFSET (0x3d4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_MODE_REQUEST_MASK (0x7)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_MODE_REQUEST_SAFE_STOP (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_MODE_REQUEST_SAFE_START (1)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_MODE_REQUEST_URGENT_STOP (2)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_MODE_REQUEST_URGENT_START (3)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_MODE_REQUEST_RESERVED4 (4)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_MODE_REQUEST_SAFER_START (5)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_MODE_REQUEST_RESERVED6 (6)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_MODE_REQUEST_RESERVED7 (7)

// args: data (3-bit)
static __inline void acamera_fpga_fpga_input_port_mode_request_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2093d4L);
    system_hw_write_32(0x2093d4L, (((uint32_t) (data & 0x7)) << 0) | (curr & 0xfffffff8));
}
static __inline uint8_t acamera_fpga_fpga_input_port_mode_request_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093d4L) & 0x7) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: mode status
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// bit 0: 1=started, 0=stopped, bits 1,2-reserved
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_INPUT_PORT_MODE_STATUS_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_MODE_STATUS_DATASIZE (3)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_MODE_STATUS_OFFSET (0x3d8)
#define ACAMERA_FPGA_FPGA_INPUT_PORT_MODE_STATUS_MASK (0x7)

// args: data (3-bit)
static __inline uint8_t acamera_fpga_fpga_input_port_mode_status_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2093d8L) & 0x7) >> 0);
}
// ------------------------------------------------------------------------------ //
// Group: FPGA WDR Frame Buffer1
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: Active Width
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Active video width in pixels
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_ACTIVE_WIDTH_DEFAULT (0x780)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_ACTIVE_WIDTH_DATASIZE (16)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_ACTIVE_WIDTH_OFFSET (0x4e4)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_ACTIVE_WIDTH_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_isp_frame_stitch_frame_buffer_active_width_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2094e4L);
    system_hw_write_32(0x2094e4L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_isp_frame_stitch_frame_buffer_active_width_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2094e4L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Active Height
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Active video height in lines
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_ACTIVE_HEIGHT_DEFAULT (0x438)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_ACTIVE_HEIGHT_DATASIZE (16)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_ACTIVE_HEIGHT_OFFSET (0x4e4)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_ACTIVE_HEIGHT_MASK (0xffff0000)

// args: data (16-bit)
static __inline void acamera_fpga_isp_frame_stitch_frame_buffer_active_height_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2094e4L);
    system_hw_write_32(0x2094e4L, (((uint32_t) (data & 0xffff)) << 16) | (curr & 0xffff));
}
static __inline uint16_t acamera_fpga_isp_frame_stitch_frame_buffer_active_height_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2094e4L) & 0xffff0000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: axi_port_enable
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_AXI_PORT_ENABLE_DEFAULT (0x0)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_AXI_PORT_ENABLE_DATASIZE (1)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_AXI_PORT_ENABLE_OFFSET (0x4e8)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_AXI_PORT_ENABLE_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_isp_frame_stitch_frame_buffer_axi_port_enable_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2094e8L);
    system_hw_write_32(0x2094e8L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_isp_frame_stitch_frame_buffer_axi_port_enable_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2094e8L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: write_enable
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_WRITE_ENABLE_DEFAULT (0x1)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_WRITE_ENABLE_DATASIZE (1)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_WRITE_ENABLE_OFFSET (0x4e8)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_WRITE_ENABLE_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_fpga_isp_frame_stitch_frame_buffer_write_enable_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2094e8L);
    system_hw_write_32(0x2094e8L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_isp_frame_stitch_frame_buffer_write_enable_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2094e8L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: frame cancel
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_FRAME_CANCEL_DEFAULT (0x0)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_FRAME_CANCEL_DATASIZE (1)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_FRAME_CANCEL_OFFSET (0x4e8)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_FRAME_CANCEL_MASK (0x4)

// args: data (1-bit)
static __inline void acamera_fpga_isp_frame_stitch_frame_buffer_frame_cancel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2094e8L);
    system_hw_write_32(0x2094e8L, (((uint32_t) (data & 0x1)) << 2) | (curr & 0xfffffffb));
}
static __inline uint8_t acamera_fpga_isp_frame_stitch_frame_buffer_frame_cancel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2094e8L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: config
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_CONFIG_DEFAULT (0x0000)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_CONFIG_DATASIZE (32)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_CONFIG_OFFSET (0x4ec)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_CONFIG_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_isp_frame_stitch_frame_buffer_config_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x2094ecL, data);
}
static __inline uint32_t acamera_fpga_isp_frame_stitch_frame_buffer_config_read(uintptr_t base) {
    return system_hw_read_32(0x2094ecL);
}
// ------------------------------------------------------------------------------ //
// Register: status
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_STATUS_DEFAULT (0x0000)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_STATUS_DATASIZE (32)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_STATUS_OFFSET (0x4f0)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_STATUS_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_isp_frame_stitch_frame_buffer_status_read(uintptr_t base) {
    return system_hw_read_32(0x2094f0L);
}
// ------------------------------------------------------------------------------ //
// Register: bank0_base
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// bank 0 base address for frame buffer, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_BANK0_BASE_DEFAULT (0x0)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_BANK0_BASE_DATASIZE (32)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_BANK0_BASE_OFFSET (0x4f8)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_BANK0_BASE_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_isp_frame_stitch_frame_buffer_bank0_base_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x2094f8L, data);
}
static __inline uint32_t acamera_fpga_isp_frame_stitch_frame_buffer_bank0_base_read(uintptr_t base) {
    return system_hw_read_32(0x2094f8L);
}
// ------------------------------------------------------------------------------ //
// Register: bank1_base
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// bank 1 base address for frame buffer, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_BANK1_BASE_DEFAULT (0x0)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_BANK1_BASE_DATASIZE (32)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_BANK1_BASE_OFFSET (0x4fc)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_BANK1_BASE_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_isp_frame_stitch_frame_buffer_bank1_base_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x2094fcL, data);
}
static __inline uint32_t acamera_fpga_isp_frame_stitch_frame_buffer_bank1_base_read(uintptr_t base) {
    return system_hw_read_32(0x2094fcL);
}
// ------------------------------------------------------------------------------ //
// Register: Line_offset
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// distance between lines, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_LINE_OFFSET_DEFAULT (0x1000)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_LINE_OFFSET_DATASIZE (32)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_LINE_OFFSET_OFFSET (0x504)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_LINE_OFFSET_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_isp_frame_stitch_frame_buffer_line_offset_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x209504L, data);
}
static __inline uint32_t acamera_fpga_isp_frame_stitch_frame_buffer_line_offset_read(uintptr_t base) {
    return system_hw_read_32(0x209504L);
}
// ------------------------------------------------------------------------------ //
// Register: Frame write on
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_FRAME_WRITE_ON_DEFAULT (0x0)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_FRAME_WRITE_ON_DATASIZE (1)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_FRAME_WRITE_ON_OFFSET (0x508)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_FRAME_WRITE_ON_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_isp_frame_stitch_frame_buffer_frame_write_on_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209508L);
    system_hw_write_32(0x209508L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_isp_frame_stitch_frame_buffer_frame_write_on_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209508L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Frame read on
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_FRAME_READ_ON_DEFAULT (0x0)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_FRAME_READ_ON_DATASIZE (1)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_FRAME_READ_ON_OFFSET (0x508)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_FRAME_READ_ON_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_fpga_isp_frame_stitch_frame_buffer_frame_read_on_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209508L);
    system_hw_write_32(0x209508L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_isp_frame_stitch_frame_buffer_frame_read_on_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209508L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: Frame write cancel
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_FRAME_WRITE_CANCEL_DEFAULT (0x0)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_FRAME_WRITE_CANCEL_DATASIZE (1)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_FRAME_WRITE_CANCEL_OFFSET (0x508)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_FRAME_WRITE_CANCEL_MASK (0x10)

// args: data (1-bit)
static __inline void acamera_fpga_isp_frame_stitch_frame_buffer_frame_write_cancel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209508L);
    system_hw_write_32(0x209508L, (((uint32_t) (data & 0x1)) << 4) | (curr & 0xffffffef));
}
static __inline uint8_t acamera_fpga_isp_frame_stitch_frame_buffer_frame_write_cancel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209508L) & 0x10) >> 4);
}
// ------------------------------------------------------------------------------ //
// Register: Frame read cancel
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_FRAME_READ_CANCEL_DEFAULT (0x0)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_FRAME_READ_CANCEL_DATASIZE (1)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_FRAME_READ_CANCEL_OFFSET (0x508)
#define ACAMERA_FPGA_ISP_FRAME_STITCH_FRAME_BUFFER_FRAME_READ_CANCEL_MASK (0x20)

// args: data (1-bit)
static __inline void acamera_fpga_isp_frame_stitch_frame_buffer_frame_read_cancel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209508L);
    system_hw_write_32(0x209508L, (((uint32_t) (data & 0x1)) << 5) | (curr & 0xffffffdf));
}
static __inline uint8_t acamera_fpga_isp_frame_stitch_frame_buffer_frame_read_cancel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209508L) & 0x20) >> 5);
}
// ------------------------------------------------------------------------------ //
// Group: FPGA WDR multiplexor
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: Master Channel
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Master input channel
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_MASTER_CHANNEL_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_MASTER_CHANNEL_DATASIZE (3)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_MASTER_CHANNEL_OFFSET (0x54c)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_MASTER_CHANNEL_MASK (0x7)

// args: data (3-bit)
static __inline void acamera_fpga_fpga_wdr_multiplexor_master_channel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20954cL);
    system_hw_write_32(0x20954cL, (((uint32_t) (data & 0x7)) << 0) | (curr & 0xfffffff8));
}
static __inline uint8_t acamera_fpga_fpga_wdr_multiplexor_master_channel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20954cL) & 0x7) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Buffer1 Channel
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Input channel for buffer 1
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_BUFFER1_CHANNEL_DEFAULT (3)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_BUFFER1_CHANNEL_DATASIZE (3)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_BUFFER1_CHANNEL_OFFSET (0x54c)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_BUFFER1_CHANNEL_MASK (0x700)

// args: data (3-bit)
static __inline void acamera_fpga_fpga_wdr_multiplexor_buffer1_channel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20954cL);
    system_hw_write_32(0x20954cL, (((uint32_t) (data & 0x7)) << 8) | (curr & 0xfffff8ff));
}
static __inline uint8_t acamera_fpga_fpga_wdr_multiplexor_buffer1_channel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20954cL) & 0x700) >> 8);
}
// ------------------------------------------------------------------------------ //
// Register: Buffer2 Channel
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Input channel for buffer 2
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_BUFFER2_CHANNEL_DEFAULT (3)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_BUFFER2_CHANNEL_DATASIZE (3)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_BUFFER2_CHANNEL_OFFSET (0x54c)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_BUFFER2_CHANNEL_MASK (0x70000)

// args: data (3-bit)
static __inline void acamera_fpga_fpga_wdr_multiplexor_buffer2_channel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20954cL);
    system_hw_write_32(0x20954cL, (((uint32_t) (data & 0x7)) << 16) | (curr & 0xfff8ffff));
}
static __inline uint8_t acamera_fpga_fpga_wdr_multiplexor_buffer2_channel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20954cL) & 0x70000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: Buffer3 Channel
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Input channel for buffer 3
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_BUFFER3_CHANNEL_DEFAULT (3)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_BUFFER3_CHANNEL_DATASIZE (3)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_BUFFER3_CHANNEL_OFFSET (0x550)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_BUFFER3_CHANNEL_MASK (0x70000)

// args: data (3-bit)
static __inline void acamera_fpga_fpga_wdr_multiplexor_buffer3_channel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209550L);
    system_hw_write_32(0x209550L, (((uint32_t) (data & 0x7)) << 16) | (curr & 0xfff8ffff));
}
static __inline uint8_t acamera_fpga_fpga_wdr_multiplexor_buffer3_channel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209550L) & 0x70000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: Current Channel
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Override input channel
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_CURRENT_CHANNEL_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_CURRENT_CHANNEL_DATASIZE (3)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_CURRENT_CHANNEL_OFFSET (0x550)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_CURRENT_CHANNEL_MASK (0x7)

// args: data (3-bit)
static __inline void acamera_fpga_fpga_wdr_multiplexor_current_channel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209550L);
    system_hw_write_32(0x209550L, (((uint32_t) (data & 0x7)) << 0) | (curr & 0xfffffff8));
}
static __inline uint8_t acamera_fpga_fpga_wdr_multiplexor_current_channel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209550L) & 0x7) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Current Channel out
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Override input channel
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_CURRENT_CHANNEL_OUT_DEFAULT (0x0)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_CURRENT_CHANNEL_OUT_DATASIZE (2)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_CURRENT_CHANNEL_OUT_OFFSET (0x554)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_CURRENT_CHANNEL_OUT_MASK (0x3)

// args: data (2-bit)
static __inline uint8_t acamera_fpga_fpga_wdr_multiplexor_current_channel_out_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209554L) & 0x3) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Master Gain
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Output gain for channel, format uint8.8
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_MASTER_GAIN_DEFAULT (0x100)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_MASTER_GAIN_DATASIZE (16)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_MASTER_GAIN_OFFSET (0x554)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_MASTER_GAIN_MASK (0xffff0000)

// args: data (16-bit)
static __inline void acamera_fpga_fpga_wdr_multiplexor_master_gain_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209554L);
    system_hw_write_32(0x209554L, (((uint32_t) (data & 0xffff)) << 16) | (curr & 0xffff));
}
static __inline uint16_t acamera_fpga_fpga_wdr_multiplexor_master_gain_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209554L) & 0xffff0000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: Buffer1 Gain
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Output gain for channel, format uint8.8
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_BUFFER1_GAIN_DEFAULT (0x100)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_BUFFER1_GAIN_DATASIZE (16)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_BUFFER1_GAIN_OFFSET (0x558)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_BUFFER1_GAIN_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_fpga_wdr_multiplexor_buffer1_gain_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209558L);
    system_hw_write_32(0x209558L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_fpga_wdr_multiplexor_buffer1_gain_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209558L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Buffer2 Gain
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Output gain for channel, format uint8.8
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_BUFFER2_GAIN_DEFAULT (0x100)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_BUFFER2_GAIN_DATASIZE (16)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_BUFFER2_GAIN_OFFSET (0x558)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_BUFFER2_GAIN_MASK (0xffff0000)

// args: data (16-bit)
static __inline void acamera_fpga_fpga_wdr_multiplexor_buffer2_gain_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209558L);
    system_hw_write_32(0x209558L, (((uint32_t) (data & 0xffff)) << 16) | (curr & 0xffff));
}
static __inline uint16_t acamera_fpga_fpga_wdr_multiplexor_buffer2_gain_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209558L) & 0xffff0000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: Buffer3 Gain
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Output gain for channel, format uint8.8
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_BUFFER3_GAIN_DEFAULT (0x100)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_BUFFER3_GAIN_DATASIZE (16)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_BUFFER3_GAIN_OFFSET (0x55c)
#define ACAMERA_FPGA_FPGA_WDR_MULTIPLEXOR_BUFFER3_GAIN_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_fpga_wdr_multiplexor_buffer3_gain_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20955cL);
    system_hw_write_32(0x20955cL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_fpga_wdr_multiplexor_buffer3_gain_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20955cL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Group: Video-Capture-2 Input port
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Controls video input port.  
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: preset
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//        Allows selection of various input port presets for standard sensor inputs.  See ISP Guide for details of available presets.
//        0-14: Frequently used presets.  If using one of available presets, remaining bits in registers 0x100 and 0x104 are not used.
//        15:   Input port configured according to registers in 0x100 and 0x104.  Consult Apical support for special input port requirements.
//      
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_PRESET_DEFAULT (2)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_PRESET_DATASIZE (4)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_PRESET_OFFSET (0x658)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_PRESET_MASK (0xf)

// args: data (4-bit)
static __inline void acamera_fpga_video_capture2_input_port_preset_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209658L);
    system_hw_write_32(0x209658L, (((uint32_t) (data & 0xf)) << 0) | (curr & 0xfffffff0));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_preset_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209658L) & 0xf) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: vs_use field
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VS_USE_FIELD_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VS_USE_FIELD_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VS_USE_FIELD_OFFSET (0x658)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VS_USE_FIELD_MASK (0x100)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VS_USE_FIELD_USE_VSYNC_I_PORT_FOR_VERTICAL_SYNC (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VS_USE_FIELD_USE_FIELD_I_PORT_FOR_VERTICAL_SYNC (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_vs_use_field_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209658L);
    system_hw_write_32(0x209658L, (((uint32_t) (data & 0x1)) << 8) | (curr & 0xfffffeff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_vs_use_field_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209658L) & 0x100) >> 8);
}
// ------------------------------------------------------------------------------ //
// Register: vs toggle
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VS_TOGGLE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VS_TOGGLE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VS_TOGGLE_OFFSET (0x658)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VS_TOGGLE_MASK (0x200)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VS_TOGGLE_VSYNC_IS_PULSETYPE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VS_TOGGLE_VSYNC_IS_TOGGLETYPE_FIELD_SIGNAL (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_vs_toggle_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209658L);
    system_hw_write_32(0x209658L, (((uint32_t) (data & 0x1)) << 9) | (curr & 0xfffffdff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_vs_toggle_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209658L) & 0x200) >> 9);
}
// ------------------------------------------------------------------------------ //
// Register: vs polarity
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VS_POLARITY_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VS_POLARITY_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VS_POLARITY_OFFSET (0x658)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VS_POLARITY_MASK (0x400)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VS_POLARITY_HORIZONTAL_COUNTER_RESET_ON_RISING_EDGE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VS_POLARITY_HORIZONTAL_COUNTER_RESET_ON_FALLING_EDGE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_vs_polarity_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209658L);
    system_hw_write_32(0x209658L, (((uint32_t) (data & 0x1)) << 10) | (curr & 0xfffffbff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_vs_polarity_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209658L) & 0x400) >> 10);
}
// ------------------------------------------------------------------------------ //
// Register: vs_polarity acl
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VS_POLARITY_ACL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VS_POLARITY_ACL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VS_POLARITY_ACL_OFFSET (0x658)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VS_POLARITY_ACL_MASK (0x800)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VS_POLARITY_ACL_DONT_INVERT_POLARITY_FOR_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VS_POLARITY_ACL_INVERT_POLARITY_FOR_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_vs_polarity_acl_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209658L);
    system_hw_write_32(0x209658L, (((uint32_t) (data & 0x1)) << 11) | (curr & 0xfffff7ff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_vs_polarity_acl_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209658L) & 0x800) >> 11);
}
// ------------------------------------------------------------------------------ //
// Register: hs_use acl
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_USE_ACL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_USE_ACL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_USE_ACL_OFFSET (0x658)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_USE_ACL_MASK (0x1000)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_USE_ACL_USE_HSYNC_I_PORT_FOR_ACTIVELINE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_USE_ACL_USE_ACL_I_PORT_FOR_ACTIVELINE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_hs_use_acl_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209658L);
    system_hw_write_32(0x209658L, (((uint32_t) (data & 0x1)) << 12) | (curr & 0xffffefff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_hs_use_acl_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209658L) & 0x1000) >> 12);
}
// ------------------------------------------------------------------------------ //
// Register: vc_c select
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VC_C_SELECT_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VC_C_SELECT_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VC_C_SELECT_OFFSET (0x658)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VC_C_SELECT_MASK (0x4000)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VC_C_SELECT_VERTICAL_COUNTER_COUNTS_ON_HS (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VC_C_SELECT_VERTICAL_COUNTER_COUNTS_ON_HORIZONTAL_COUNTER_OVERFLOW_OR_RESET (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_vc_c_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209658L);
    system_hw_write_32(0x209658L, (((uint32_t) (data & 0x1)) << 14) | (curr & 0xffffbfff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_vc_c_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209658L) & 0x4000) >> 14);
}
// ------------------------------------------------------------------------------ //
// Register: vc_r select
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VC_R_SELECT_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VC_R_SELECT_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VC_R_SELECT_OFFSET (0x658)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VC_R_SELECT_MASK (0x8000)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VC_R_SELECT_VERTICAL_COUNTER_IS_RESET_ON_EDGE_OF_VS (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VC_R_SELECT_VERTICAL_COUNTER_IS_RESET_AFTER_TIMEOUT_ON_HS (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_vc_r_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209658L);
    system_hw_write_32(0x209658L, (((uint32_t) (data & 0x1)) << 15) | (curr & 0xffff7fff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_vc_r_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209658L) & 0x8000) >> 15);
}
// ------------------------------------------------------------------------------ //
// Register: hs_xor vs
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_XOR_VS_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_XOR_VS_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_XOR_VS_OFFSET (0x658)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_XOR_VS_MASK (0x10000)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_XOR_VS_NORMAL_MODE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_XOR_VS_HVALID__HSYNC_XOR_VSYNC (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_hs_xor_vs_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209658L);
    system_hw_write_32(0x209658L, (((uint32_t) (data & 0x1)) << 16) | (curr & 0xfffeffff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_hs_xor_vs_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209658L) & 0x10000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: hs polarity
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_POLARITY_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_POLARITY_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_POLARITY_OFFSET (0x658)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_POLARITY_MASK (0x20000)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_POLARITY_DONT_INVERT_POLARITY_OF_HS_FOR_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_POLARITY_INVERT_POLARITY_OF_HS_FOR_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_hs_polarity_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209658L);
    system_hw_write_32(0x209658L, (((uint32_t) (data & 0x1)) << 17) | (curr & 0xfffdffff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_hs_polarity_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209658L) & 0x20000) >> 17);
}
// ------------------------------------------------------------------------------ //
// Register: hs_polarity acl
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_POLARITY_ACL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_POLARITY_ACL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_POLARITY_ACL_OFFSET (0x658)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_POLARITY_ACL_MASK (0x40000)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_POLARITY_ACL_DONT_INVERT_POLARITY_OF_HS_FOR_HS_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_POLARITY_ACL_INVERT_POLARITY_OF_HS_FOR_HS_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_hs_polarity_acl_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209658L);
    system_hw_write_32(0x209658L, (((uint32_t) (data & 0x1)) << 18) | (curr & 0xfffbffff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_hs_polarity_acl_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209658L) & 0x40000) >> 18);
}
// ------------------------------------------------------------------------------ //
// Register: hs_polarity hs
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_POLARITY_HS_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_POLARITY_HS_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_POLARITY_HS_OFFSET (0x658)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_POLARITY_HS_MASK (0x80000)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_POLARITY_HS_HORIZONTAL_COUNTER_IS_RESET_ON_RISING_EDGE_OF_HS (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_POLARITY_HS_HORIZONTAL_COUNTER_IS_RESET_ON_VSYNC_EG_WHEN_HSYNC_IS_NOT_AVAILABLE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_hs_polarity_hs_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209658L);
    system_hw_write_32(0x209658L, (((uint32_t) (data & 0x1)) << 19) | (curr & 0xfff7ffff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_hs_polarity_hs_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209658L) & 0x80000) >> 19);
}
// ------------------------------------------------------------------------------ //
// Register: hs_polarity vc
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_POLARITY_VC_DEFAULT (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_POLARITY_VC_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_POLARITY_VC_OFFSET (0x658)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_POLARITY_VC_MASK (0x100000)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_POLARITY_VC_VERTICAL_COUNTER_INCREMENTS_ON_RISING_EDGE_OF_HS (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HS_POLARITY_VC_VERTICAL_COUNTER_INCREMENTS_ON_FALLING_EDGE_OF_HS (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_hs_polarity_vc_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209658L);
    system_hw_write_32(0x209658L, (((uint32_t) (data & 0x1)) << 20) | (curr & 0xffefffff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_hs_polarity_vc_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209658L) & 0x100000) >> 20);
}
// ------------------------------------------------------------------------------ //
// Register: hc_r select
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_R_SELECT_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_R_SELECT_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_R_SELECT_OFFSET (0x658)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_R_SELECT_MASK (0x800000)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_R_SELECT_VERTICAL_COUNTER_IS_RESET_ON_RISING_EDGE_OF_HS (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_R_SELECT_VERTICAL_COUNTER_IS_RESET_ON_RISING_EDGE_OF_VS (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_hc_r_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209658L);
    system_hw_write_32(0x209658L, (((uint32_t) (data & 0x1)) << 23) | (curr & 0xff7fffff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_hc_r_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209658L) & 0x800000) >> 23);
}
// ------------------------------------------------------------------------------ //
// Register: acl polarity
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACL_POLARITY_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACL_POLARITY_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACL_POLARITY_OFFSET (0x658)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACL_POLARITY_MASK (0x1000000)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACL_POLARITY_DONT_INVERT_ACL_I_FOR_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACL_POLARITY_INVERT_ACL_I_FOR_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_acl_polarity_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209658L);
    system_hw_write_32(0x209658L, (((uint32_t) (data & 0x1)) << 24) | (curr & 0xfeffffff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_acl_polarity_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209658L) & 0x1000000) >> 24);
}
// ------------------------------------------------------------------------------ //
// Register: field polarity
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELD_POLARITY_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELD_POLARITY_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELD_POLARITY_OFFSET (0x65c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELD_POLARITY_MASK (0x1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELD_POLARITY_DONT_INVERT_FIELD_I_FOR_FIELD_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELD_POLARITY_INVERT_FIELD_I_FOR_FIELD_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_field_polarity_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20965cL);
    system_hw_write_32(0x20965cL, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_field_polarity_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20965cL) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: field toggle
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELD_TOGGLE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELD_TOGGLE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELD_TOGGLE_OFFSET (0x65c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELD_TOGGLE_MASK (0x2)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELD_TOGGLE_FIELD_IS_PULSETYPE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELD_TOGGLE_FIELD_IS_TOGGLETYPE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_field_toggle_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20965cL);
    system_hw_write_32(0x20965cL, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_field_toggle_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20965cL) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: aclg window0
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_WINDOW0_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_WINDOW0_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_WINDOW0_OFFSET (0x65c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_WINDOW0_MASK (0x100)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_WINDOW0_EXCLUDE_WINDOW0_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_WINDOW0_INCLUDE_WINDOW0_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_aclg_window0_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20965cL);
    system_hw_write_32(0x20965cL, (((uint32_t) (data & 0x1)) << 8) | (curr & 0xfffffeff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_aclg_window0_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20965cL) & 0x100) >> 8);
}
// ------------------------------------------------------------------------------ //
// Register: aclg hsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_HSYNC_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_HSYNC_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_HSYNC_OFFSET (0x65c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_HSYNC_MASK (0x200)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_HSYNC_EXCLUDE_HSYNC_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_HSYNC_INCLUDE_HSYNC_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_aclg_hsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20965cL);
    system_hw_write_32(0x20965cL, (((uint32_t) (data & 0x1)) << 9) | (curr & 0xfffffdff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_aclg_hsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20965cL) & 0x200) >> 9);
}
// ------------------------------------------------------------------------------ //
// Register: aclg window2
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_WINDOW2_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_WINDOW2_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_WINDOW2_OFFSET (0x65c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_WINDOW2_MASK (0x400)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_WINDOW2_EXCLUDE_WINDOW2_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_WINDOW2_INCLUDE_WINDOW2_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_aclg_window2_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20965cL);
    system_hw_write_32(0x20965cL, (((uint32_t) (data & 0x1)) << 10) | (curr & 0xfffffbff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_aclg_window2_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20965cL) & 0x400) >> 10);
}
// ------------------------------------------------------------------------------ //
// Register: aclg acl
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_ACL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_ACL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_ACL_OFFSET (0x65c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_ACL_MASK (0x800)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_ACL_EXCLUDE_ACL_I_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_ACL_INCLUDE_ACL_I_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_aclg_acl_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20965cL);
    system_hw_write_32(0x20965cL, (((uint32_t) (data & 0x1)) << 11) | (curr & 0xfffff7ff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_aclg_acl_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20965cL) & 0x800) >> 11);
}
// ------------------------------------------------------------------------------ //
// Register: aclg vsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_VSYNC_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_VSYNC_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_VSYNC_OFFSET (0x65c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_VSYNC_MASK (0x1000)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_VSYNC_EXCLUDE_VSYNC_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_ACLG_VSYNC_INCLUDE_VSYNC_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_aclg_vsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20965cL);
    system_hw_write_32(0x20965cL, (((uint32_t) (data & 0x1)) << 12) | (curr & 0xffffefff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_aclg_vsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20965cL) & 0x1000) >> 12);
}
// ------------------------------------------------------------------------------ //
// Register: hsg window1
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HSG_WINDOW1_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HSG_WINDOW1_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HSG_WINDOW1_OFFSET (0x65c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HSG_WINDOW1_MASK (0x10000)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HSG_WINDOW1_EXCLUDE_WINDOW1_SIGNAL_IN_HS_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HSG_WINDOW1_INCLUDE_WINDOW1_SIGNAL_IN_HS_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_hsg_window1_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20965cL);
    system_hw_write_32(0x20965cL, (((uint32_t) (data & 0x1)) << 16) | (curr & 0xfffeffff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_hsg_window1_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20965cL) & 0x10000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: hsg hsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HSG_HSYNC_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HSG_HSYNC_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HSG_HSYNC_OFFSET (0x65c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HSG_HSYNC_MASK (0x20000)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HSG_HSYNC_EXCLUDE_HSYNC_SIGNAL_IN_HS_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HSG_HSYNC_INCLUDE_HSYNC_SIGNAL_IN_HS_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_hsg_hsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20965cL);
    system_hw_write_32(0x20965cL, (((uint32_t) (data & 0x1)) << 17) | (curr & 0xfffdffff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_hsg_hsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20965cL) & 0x20000) >> 17);
}
// ------------------------------------------------------------------------------ //
// Register: hsg vsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HSG_VSYNC_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HSG_VSYNC_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HSG_VSYNC_OFFSET (0x65c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HSG_VSYNC_MASK (0x40000)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HSG_VSYNC_EXCLUDE_VSYNC_SIGNAL_IN_HS_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HSG_VSYNC_INCLUDE_VSYNC_SIGNAL_IN_HS_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_hsg_vsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20965cL);
    system_hw_write_32(0x20965cL, (((uint32_t) (data & 0x1)) << 18) | (curr & 0xfffbffff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_hsg_vsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20965cL) & 0x40000) >> 18);
}
// ------------------------------------------------------------------------------ //
// Register: hsg window2
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HSG_WINDOW2_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HSG_WINDOW2_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HSG_WINDOW2_OFFSET (0x65c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HSG_WINDOW2_MASK (0x80000)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HSG_WINDOW2_EXCLUDE_WINDOW2_SIGNAL_IN_HS_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HSG_WINDOW2_INCLUDE_WINDOW2_SIGNAL_IN_HS_GATE_MASK_OUT_SPURIOUS_HS_DURING_BLANK (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_hsg_window2_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20965cL);
    system_hw_write_32(0x20965cL, (((uint32_t) (data & 0x1)) << 19) | (curr & 0xfff7ffff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_hsg_window2_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20965cL) & 0x80000) >> 19);
}
// ------------------------------------------------------------------------------ //
// Register: fieldg vsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELDG_VSYNC_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELDG_VSYNC_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELDG_VSYNC_OFFSET (0x65c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELDG_VSYNC_MASK (0x1000000)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELDG_VSYNC_EXCLUDE_VSYNC_SIGNAL_IN_FIELD_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELDG_VSYNC_INCLUDE_VSYNC_SIGNAL_IN_FIELD_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_fieldg_vsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20965cL);
    system_hw_write_32(0x20965cL, (((uint32_t) (data & 0x1)) << 24) | (curr & 0xfeffffff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_fieldg_vsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20965cL) & 0x1000000) >> 24);
}
// ------------------------------------------------------------------------------ //
// Register: fieldg window2
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELDG_WINDOW2_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELDG_WINDOW2_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELDG_WINDOW2_OFFSET (0x65c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELDG_WINDOW2_MASK (0x2000000)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELDG_WINDOW2_EXCLUDE_WINDOW2_SIGNAL_IN_FIELD_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELDG_WINDOW2_INCLUDE_WINDOW2_SIGNAL_IN_FIELD_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_fieldg_window2_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20965cL);
    system_hw_write_32(0x20965cL, (((uint32_t) (data & 0x1)) << 25) | (curr & 0xfdffffff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_fieldg_window2_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20965cL) & 0x2000000) >> 25);
}
// ------------------------------------------------------------------------------ //
// Register: fieldg field
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELDG_FIELD_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELDG_FIELD_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELDG_FIELD_OFFSET (0x65c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELDG_FIELD_MASK (0x4000000)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELDG_FIELD_EXCLUDE_FIELD_I_SIGNAL_IN_FIELD_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELDG_FIELD_INCLUDE_FIELD_I_SIGNAL_IN_FIELD_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_fieldg_field_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20965cL);
    system_hw_write_32(0x20965cL, (((uint32_t) (data & 0x1)) << 26) | (curr & 0xfbffffff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_fieldg_field_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20965cL) & 0x4000000) >> 26);
}
// ------------------------------------------------------------------------------ //
// Register: field mode
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELD_MODE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELD_MODE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELD_MODE_OFFSET (0x65c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELD_MODE_MASK (0x8000000)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELD_MODE_PULSE_FIELD (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FIELD_MODE_TOGGLE_FIELD (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_field_mode_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20965cL);
    system_hw_write_32(0x20965cL, (((uint32_t) (data & 0x1)) << 27) | (curr & 0xf7ffffff));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_field_mode_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20965cL) & 0x8000000) >> 27);
}
// ------------------------------------------------------------------------------ //
// Register: hc limit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// horizontal counter limit value (counts: 0,1,...hc_limit-1,hc_limit,0,1,...)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_LIMIT_DEFAULT (0xFFFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_LIMIT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_LIMIT_OFFSET (0x660)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_LIMIT_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture2_input_port_hc_limit_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209660L);
    system_hw_write_32(0x209660L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture2_input_port_hc_limit_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209660L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: hc start0
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window0 start for ACL gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_START0_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_START0_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_START0_OFFSET (0x664)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_START0_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture2_input_port_hc_start0_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209664L);
    system_hw_write_32(0x209664L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture2_input_port_hc_start0_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209664L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: hc size0
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window0 size for ACL gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_SIZE0_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_SIZE0_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_SIZE0_OFFSET (0x668)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_SIZE0_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture2_input_port_hc_size0_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209668L);
    system_hw_write_32(0x209668L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture2_input_port_hc_size0_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209668L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: hc start1
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window1 start for HS gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_START1_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_START1_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_START1_OFFSET (0x66c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_START1_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture2_input_port_hc_start1_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20966cL);
    system_hw_write_32(0x20966cL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture2_input_port_hc_start1_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20966cL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: hc size1
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window1 size for HS gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_SIZE1_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_SIZE1_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_SIZE1_OFFSET (0x670)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_HC_SIZE1_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture2_input_port_hc_size1_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209670L);
    system_hw_write_32(0x209670L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture2_input_port_hc_size1_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209670L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: vc limit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// vertical counter limit value (counts: 0,1,...vc_limit-1,vc_limit,0,1,...)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VC_LIMIT_DEFAULT (0xFFFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VC_LIMIT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VC_LIMIT_OFFSET (0x674)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VC_LIMIT_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture2_input_port_vc_limit_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209674L);
    system_hw_write_32(0x209674L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture2_input_port_vc_limit_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209674L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: vc start
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window2 start for ACL gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VC_START_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VC_START_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VC_START_OFFSET (0x678)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VC_START_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture2_input_port_vc_start_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209678L);
    system_hw_write_32(0x209678L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture2_input_port_vc_start_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209678L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: vc size
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window2 size for ACL gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VC_SIZE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VC_SIZE_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VC_SIZE_OFFSET (0x67c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_VC_SIZE_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture2_input_port_vc_size_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20967cL);
    system_hw_write_32(0x20967cL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture2_input_port_vc_size_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20967cL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: frame width
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// detected frame width.  Read only value.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FRAME_WIDTH_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FRAME_WIDTH_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FRAME_WIDTH_OFFSET (0x680)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FRAME_WIDTH_MASK (0xffff)

// args: data (16-bit)
static __inline uint16_t acamera_fpga_video_capture2_input_port_frame_width_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209680L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: frame height
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// detected frame height.  Read only value.  
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FRAME_HEIGHT_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FRAME_HEIGHT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FRAME_HEIGHT_OFFSET (0x684)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FRAME_HEIGHT_MASK (0xffff)

// args: data (16-bit)
static __inline uint16_t acamera_fpga_video_capture2_input_port_frame_height_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209684L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: mode request
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Used to stop and start input port.  See ISP guide for further details. 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_MODE_REQUEST_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_MODE_REQUEST_DATASIZE (3)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_MODE_REQUEST_OFFSET (0x688)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_MODE_REQUEST_MASK (0x7)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_MODE_REQUEST_SAFE_STOP (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_MODE_REQUEST_SAFE_START (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_MODE_REQUEST_URGENT_STOP (2)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_MODE_REQUEST_URGENT_START (3)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_MODE_REQUEST_RESERVED4 (4)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_MODE_REQUEST_SAFER_START (5)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_MODE_REQUEST_RESERVED6 (6)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_MODE_REQUEST_RESERVED7 (7)

// args: data (3-bit)
static __inline void acamera_fpga_video_capture2_input_port_mode_request_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209688L);
    system_hw_write_32(0x209688L, (((uint32_t) (data & 0x7)) << 0) | (curr & 0xfffffff8));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_mode_request_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209688L) & 0x7) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: freeze config
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Used to freeze input port configuration.  Used when multiple register writes are required to change input port configuration. 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FREEZE_CONFIG_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FREEZE_CONFIG_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FREEZE_CONFIG_OFFSET (0x688)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FREEZE_CONFIG_MASK (0x80)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FREEZE_CONFIG_NORMAL_OPERATION (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_FREEZE_CONFIG_HOLD_PREVIOUS_INPUT_PORT_CONFIG_STATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_input_port_freeze_config_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209688L);
    system_hw_write_32(0x209688L, (((uint32_t) (data & 0x1)) << 7) | (curr & 0xffffff7f));
}
static __inline uint8_t acamera_fpga_video_capture2_input_port_freeze_config_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209688L) & 0x80) >> 7);
}
// ------------------------------------------------------------------------------ //
// Register: mode status
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//          Used to monitor input port status:
//          bit 0: 1=running, 0=stopped, bits 1,2-reserved
//        
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_MODE_STATUS_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_MODE_STATUS_DATASIZE (3)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_MODE_STATUS_OFFSET (0x68c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_INPUT_PORT_MODE_STATUS_MASK (0x7)

// args: data (3-bit)
static __inline uint8_t acamera_fpga_video_capture2_input_port_mode_status_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20968cL) & 0x7) >> 0);
}
// ------------------------------------------------------------------------------ //
// Group: Video-Capture-2 Frame Stats
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: stats reset
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_STATS_RESET_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_STATS_RESET_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_STATS_RESET_OFFSET (0x690)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_STATS_RESET_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_frame_stats_stats_reset_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209690L);
    system_hw_write_32(0x209690L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture2_frame_stats_stats_reset_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209690L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: stats hold
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_STATS_HOLD_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_STATS_HOLD_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_STATS_HOLD_OFFSET (0x694)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_STATS_HOLD_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_frame_stats_stats_hold_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209694L);
    system_hw_write_32(0x209694L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture2_frame_stats_stats_hold_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209694L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: active width min
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_WIDTH_MIN_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_WIDTH_MIN_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_WIDTH_MIN_OFFSET (0x6a0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_WIDTH_MIN_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture2_frame_stats_active_width_min_read(uintptr_t base) {
    return system_hw_read_32(0x2096a0L);
}
// ------------------------------------------------------------------------------ //
// Register: active width max
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_WIDTH_MAX_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_WIDTH_MAX_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_WIDTH_MAX_OFFSET (0x6a4)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_WIDTH_MAX_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture2_frame_stats_active_width_max_read(uintptr_t base) {
    return system_hw_read_32(0x2096a4L);
}
// ------------------------------------------------------------------------------ //
// Register: active width sum
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_WIDTH_SUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_WIDTH_SUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_WIDTH_SUM_OFFSET (0x6a8)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_WIDTH_SUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture2_frame_stats_active_width_sum_read(uintptr_t base) {
    return system_hw_read_32(0x2096a8L);
}
// ------------------------------------------------------------------------------ //
// Register: active width num
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_WIDTH_NUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_WIDTH_NUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_WIDTH_NUM_OFFSET (0x6ac)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_WIDTH_NUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture2_frame_stats_active_width_num_read(uintptr_t base) {
    return system_hw_read_32(0x2096acL);
}
// ------------------------------------------------------------------------------ //
// Register: active height min
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_HEIGHT_MIN_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_HEIGHT_MIN_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_HEIGHT_MIN_OFFSET (0x6b0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_HEIGHT_MIN_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture2_frame_stats_active_height_min_read(uintptr_t base) {
    return system_hw_read_32(0x2096b0L);
}
// ------------------------------------------------------------------------------ //
// Register: active height max
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_HEIGHT_MAX_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_HEIGHT_MAX_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_HEIGHT_MAX_OFFSET (0x6b4)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_HEIGHT_MAX_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture2_frame_stats_active_height_max_read(uintptr_t base) {
    return system_hw_read_32(0x2096b4L);
}
// ------------------------------------------------------------------------------ //
// Register: active height sum
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_HEIGHT_SUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_HEIGHT_SUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_HEIGHT_SUM_OFFSET (0x6b8)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_HEIGHT_SUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture2_frame_stats_active_height_sum_read(uintptr_t base) {
    return system_hw_read_32(0x2096b8L);
}
// ------------------------------------------------------------------------------ //
// Register: active height num
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_HEIGHT_NUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_HEIGHT_NUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_HEIGHT_NUM_OFFSET (0x6bc)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_ACTIVE_HEIGHT_NUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture2_frame_stats_active_height_num_read(uintptr_t base) {
    return system_hw_read_32(0x2096bcL);
}
// ------------------------------------------------------------------------------ //
// Register: hblank min
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_HBLANK_MIN_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_HBLANK_MIN_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_HBLANK_MIN_OFFSET (0x6c0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_HBLANK_MIN_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture2_frame_stats_hblank_min_read(uintptr_t base) {
    return system_hw_read_32(0x2096c0L);
}
// ------------------------------------------------------------------------------ //
// Register: hblank max
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_HBLANK_MAX_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_HBLANK_MAX_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_HBLANK_MAX_OFFSET (0x6c4)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_HBLANK_MAX_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture2_frame_stats_hblank_max_read(uintptr_t base) {
    return system_hw_read_32(0x2096c4L);
}
// ------------------------------------------------------------------------------ //
// Register: hblank sum
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_HBLANK_SUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_HBLANK_SUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_HBLANK_SUM_OFFSET (0x6c8)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_HBLANK_SUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture2_frame_stats_hblank_sum_read(uintptr_t base) {
    return system_hw_read_32(0x2096c8L);
}
// ------------------------------------------------------------------------------ //
// Register: hblank num
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_HBLANK_NUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_HBLANK_NUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_HBLANK_NUM_OFFSET (0x6cc)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_HBLANK_NUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture2_frame_stats_hblank_num_read(uintptr_t base) {
    return system_hw_read_32(0x2096ccL);
}
// ------------------------------------------------------------------------------ //
// Register: vblank min
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_VBLANK_MIN_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_VBLANK_MIN_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_VBLANK_MIN_OFFSET (0x6d0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_VBLANK_MIN_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture2_frame_stats_vblank_min_read(uintptr_t base) {
    return system_hw_read_32(0x2096d0L);
}
// ------------------------------------------------------------------------------ //
// Register: vblank max
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_VBLANK_MAX_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_VBLANK_MAX_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_VBLANK_MAX_OFFSET (0x6d4)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_VBLANK_MAX_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture2_frame_stats_vblank_max_read(uintptr_t base) {
    return system_hw_read_32(0x2096d4L);
}
// ------------------------------------------------------------------------------ //
// Register: vblank sum
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_VBLANK_SUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_VBLANK_SUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_VBLANK_SUM_OFFSET (0x6d8)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_VBLANK_SUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture2_frame_stats_vblank_sum_read(uintptr_t base) {
    return system_hw_read_32(0x2096d8L);
}
// ------------------------------------------------------------------------------ //
// Register: vblank num
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_VBLANK_NUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_VBLANK_NUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_VBLANK_NUM_OFFSET (0x6dc)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_FRAME_STATS_VBLANK_NUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture2_frame_stats_vblank_num_read(uintptr_t base) {
    return system_hw_read_32(0x2096dcL);
}
// ------------------------------------------------------------------------------ //
// Group: Video-Capture-2 video test gen
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Video test generator controls.  See ISP Guide for further details
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: test_pattern_off on
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Test pattern off-on: 0=off, 1=on
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_TEST_PATTERN_OFF_ON_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_TEST_PATTERN_OFF_ON_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_TEST_PATTERN_OFF_ON_OFFSET (0x6e4)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_TEST_PATTERN_OFF_ON_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_video_test_gen_test_pattern_off_on_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2096e4L);
    system_hw_write_32(0x2096e4L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture2_video_test_gen_test_pattern_off_on_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2096e4L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: bayer_rgb_i sel
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Bayer or rgb select for input video: 0=bayer, 1=rgb
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_BAYER_RGB_I_SEL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_BAYER_RGB_I_SEL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_BAYER_RGB_I_SEL_OFFSET (0x6e4)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_BAYER_RGB_I_SEL_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_video_test_gen_bayer_rgb_i_sel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2096e4L);
    system_hw_write_32(0x2096e4L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_video_capture2_video_test_gen_bayer_rgb_i_sel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2096e4L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: bayer_rgb_o sel
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Bayer or rgb select for output video: 0=bayer, 1=rgb
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_BAYER_RGB_O_SEL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_BAYER_RGB_O_SEL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_BAYER_RGB_O_SEL_OFFSET (0x6e4)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_BAYER_RGB_O_SEL_MASK (0x4)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_video_test_gen_bayer_rgb_o_sel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2096e4L);
    system_hw_write_32(0x2096e4L, (((uint32_t) (data & 0x1)) << 2) | (curr & 0xfffffffb));
}
static __inline uint8_t acamera_fpga_video_capture2_video_test_gen_bayer_rgb_o_sel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2096e4L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: pattern type
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Pattern type select: 0=Flat field,1=Horizontal gradient,2=Vertical Gradient,3=Vertical Bars,4=Rectangle,5-255=Default white frame on black
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_PATTERN_TYPE_DEFAULT (0x03)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_PATTERN_TYPE_DATASIZE (8)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_PATTERN_TYPE_OFFSET (0x6e8)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_PATTERN_TYPE_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_video_capture2_video_test_gen_pattern_type_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2096e8L);
    system_hw_write_32(0x2096e8L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_video_capture2_video_test_gen_pattern_type_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2096e8L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: r backgnd
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Red background  value 16bit, MSB aligned to used width 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_R_BACKGND_DEFAULT (0xFFFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_R_BACKGND_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_R_BACKGND_OFFSET (0x6ec)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_R_BACKGND_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture2_video_test_gen_r_backgnd_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2096ecL);
    system_hw_write_32(0x2096ecL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture2_video_test_gen_r_backgnd_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2096ecL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: g backgnd
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Green background value 16bit, MSB aligned to used width
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_G_BACKGND_DEFAULT (0xFFFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_G_BACKGND_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_G_BACKGND_OFFSET (0x6f0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_G_BACKGND_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture2_video_test_gen_g_backgnd_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2096f0L);
    system_hw_write_32(0x2096f0L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture2_video_test_gen_g_backgnd_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2096f0L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: b backgnd
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Blue background value 16bit, MSB aligned to used width
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_B_BACKGND_DEFAULT (0xFFFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_B_BACKGND_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_B_BACKGND_OFFSET (0x6f4)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_B_BACKGND_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture2_video_test_gen_b_backgnd_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2096f4L);
    system_hw_write_32(0x2096f4L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture2_video_test_gen_b_backgnd_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2096f4L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: r foregnd
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Red foreground  value 16bit, MSB aligned to used width 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_R_FOREGND_DEFAULT (0x8FFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_R_FOREGND_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_R_FOREGND_OFFSET (0x6f8)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_R_FOREGND_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture2_video_test_gen_r_foregnd_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2096f8L);
    system_hw_write_32(0x2096f8L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture2_video_test_gen_r_foregnd_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2096f8L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: g foregnd
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Green foreground value 16bit, MSB aligned to used width
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_G_FOREGND_DEFAULT (0x8FFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_G_FOREGND_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_G_FOREGND_OFFSET (0x6fc)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_G_FOREGND_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture2_video_test_gen_g_foregnd_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2096fcL);
    system_hw_write_32(0x2096fcL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture2_video_test_gen_g_foregnd_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2096fcL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: b foregnd
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Blue foreground value 16bit, MSB aligned to used width
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_B_FOREGND_DEFAULT (0x8FFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_B_FOREGND_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_B_FOREGND_OFFSET (0x700)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_B_FOREGND_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture2_video_test_gen_b_foregnd_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209700L);
    system_hw_write_32(0x209700L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture2_video_test_gen_b_foregnd_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209700L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rgb gradient
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// RGB gradient increment per pixel (0-15)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_RGB_GRADIENT_DEFAULT (0x3CAA)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_RGB_GRADIENT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_RGB_GRADIENT_OFFSET (0x704)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_RGB_GRADIENT_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture2_video_test_gen_rgb_gradient_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209704L);
    system_hw_write_32(0x209704L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture2_video_test_gen_rgb_gradient_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209704L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rgb_gradient start
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// RGB gradient start value 16bit, MSB aligned to used width
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_RGB_GRADIENT_START_DEFAULT (0x0000)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_RGB_GRADIENT_START_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_RGB_GRADIENT_START_OFFSET (0x708)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_RGB_GRADIENT_START_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture2_video_test_gen_rgb_gradient_start_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209708L);
    system_hw_write_32(0x209708L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture2_video_test_gen_rgb_gradient_start_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209708L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rect top
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  Rectangle top line number 1-n
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_RECT_TOP_DEFAULT (0x0001)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_RECT_TOP_DATASIZE (14)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_RECT_TOP_OFFSET (0x70c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_RECT_TOP_MASK (0x3fff)

// args: data (14-bit)
static __inline void acamera_fpga_video_capture2_video_test_gen_rect_top_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20970cL);
    system_hw_write_32(0x20970cL, (((uint32_t) (data & 0x3fff)) << 0) | (curr & 0xffffc000));
}
static __inline uint16_t acamera_fpga_video_capture2_video_test_gen_rect_top_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20970cL) & 0x3fff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rect bot
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  Rectangle bottom line number 1-n
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_RECT_BOT_DEFAULT (0x0100)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_RECT_BOT_DATASIZE (14)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_RECT_BOT_OFFSET (0x710)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_RECT_BOT_MASK (0x3fff)

// args: data (14-bit)
static __inline void acamera_fpga_video_capture2_video_test_gen_rect_bot_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209710L);
    system_hw_write_32(0x209710L, (((uint32_t) (data & 0x3fff)) << 0) | (curr & 0xffffc000));
}
static __inline uint16_t acamera_fpga_video_capture2_video_test_gen_rect_bot_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209710L) & 0x3fff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rect left
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  Rectangle left pixel number 1-n
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_RECT_LEFT_DEFAULT (0x0001)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_RECT_LEFT_DATASIZE (14)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_RECT_LEFT_OFFSET (0x714)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_RECT_LEFT_MASK (0x3fff)

// args: data (14-bit)
static __inline void acamera_fpga_video_capture2_video_test_gen_rect_left_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209714L);
    system_hw_write_32(0x209714L, (((uint32_t) (data & 0x3fff)) << 0) | (curr & 0xffffc000));
}
static __inline uint16_t acamera_fpga_video_capture2_video_test_gen_rect_left_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209714L) & 0x3fff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rect right
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  Rectangle right pixel number 1-n
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_RECT_RIGHT_DEFAULT (0x0100)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_RECT_RIGHT_DATASIZE (14)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_RECT_RIGHT_OFFSET (0x71c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_VIDEO_TEST_GEN_RECT_RIGHT_MASK (0x3fff)

// args: data (14-bit)
static __inline void acamera_fpga_video_capture2_video_test_gen_rect_right_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20971cL);
    system_hw_write_32(0x20971cL, (((uint32_t) (data & 0x3fff)) << 0) | (curr & 0xffffc000));
}
static __inline uint16_t acamera_fpga_video_capture2_video_test_gen_rect_right_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20971cL) & 0x3fff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Group: Video-Capture-2 DMA Writer
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Video-Capture DMA writer controls
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: Format
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Format
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_FORMAT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_FORMAT_DATASIZE (8)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_FORMAT_OFFSET (0x720)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_FORMAT_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_format_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209720L);
    system_hw_write_32(0x209720L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_format_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209720L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Base mode
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Base DMA packing mode for RGB/RAW/YUV etc (see ISP guide)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BASE_MODE_DEFAULT (0x0D)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BASE_MODE_DATASIZE (4)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BASE_MODE_OFFSET (0x720)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BASE_MODE_MASK (0xf)

// args: data (4-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_base_mode_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209720L);
    system_hw_write_32(0x209720L, (((uint32_t) (data & 0xf)) << 0) | (curr & 0xfffffff0));
}
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_base_mode_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209720L) & 0xf) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Plane select
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Plane select for planar base modes.  Only used if planar outputs required.  Not used.  Should be set to 0
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_PLANE_SELECT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_PLANE_SELECT_DATASIZE (2)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_PLANE_SELECT_OFFSET (0x720)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_PLANE_SELECT_MASK (0xc0)

// args: data (2-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_plane_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209720L);
    system_hw_write_32(0x209720L, (((uint32_t) (data & 0x3)) << 6) | (curr & 0xffffff3f));
}
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_plane_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209720L) & 0xc0) >> 6);
}
// ------------------------------------------------------------------------------ //
// Register: single frame
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0 = All frames are written(after frame_write_on= 1), 1= only 1st frame written ( after frame_write_on =1)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_SINGLE_FRAME_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_SINGLE_FRAME_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_SINGLE_FRAME_OFFSET (0x724)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_SINGLE_FRAME_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_single_frame_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209724L);
    system_hw_write_32(0x209724L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_single_frame_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209724L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: frame write on
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//    0 = no frames written(when switched from 1, current frame completes writing before stopping),
//    1= write frame(s) (write single or continous frame(s) )
//    
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_FRAME_WRITE_ON_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_FRAME_WRITE_ON_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_FRAME_WRITE_ON_OFFSET (0x724)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_FRAME_WRITE_ON_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_frame_write_on_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209724L);
    system_hw_write_32(0x209724L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_frame_write_on_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209724L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: half irate
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0 = normal operation , 1= write half(alternate) of input frames( only valid for continuous mode)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_HALF_IRATE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_HALF_IRATE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_HALF_IRATE_OFFSET (0x724)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_HALF_IRATE_MASK (0x4)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_half_irate_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209724L);
    system_hw_write_32(0x209724L, (((uint32_t) (data & 0x1)) << 2) | (curr & 0xfffffffb));
}
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_half_irate_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209724L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: axi xact comp
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0 = dont wait for axi transaction completion at end of frame(just all transfers accepted). 1 = wait for all transactions completed
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_XACT_COMP_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_XACT_COMP_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_XACT_COMP_OFFSET (0x724)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_XACT_COMP_MASK (0x8)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_axi_xact_comp_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209724L);
    system_hw_write_32(0x209724L, (((uint32_t) (data & 0x1)) << 3) | (curr & 0xfffffff7));
}
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_axi_xact_comp_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209724L) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: active width
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Active video width in pixels 128-8000
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_ACTIVE_WIDTH_DEFAULT (0x780)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_ACTIVE_WIDTH_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_ACTIVE_WIDTH_OFFSET (0x728)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_ACTIVE_WIDTH_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_active_width_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209728L);
    system_hw_write_32(0x209728L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture2_dma_writer_active_width_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209728L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: active height
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Active video height in lines 128-8000
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_ACTIVE_HEIGHT_DEFAULT (0x438)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_ACTIVE_HEIGHT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_ACTIVE_HEIGHT_OFFSET (0x72c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_ACTIVE_HEIGHT_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_active_height_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20972cL);
    system_hw_write_32(0x20972cL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture2_dma_writer_active_height_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20972cL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: bank0_base
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// bank 0 base address for frame buffer, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BANK0_BASE_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BANK0_BASE_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BANK0_BASE_OFFSET (0x730)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BANK0_BASE_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_bank0_base_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x209730L, data);
}
static __inline uint32_t acamera_fpga_video_capture2_dma_writer_bank0_base_read(uintptr_t base) {
    return system_hw_read_32(0x209730L);
}
// ------------------------------------------------------------------------------ //
// Register: bank1_base
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// bank 1 base address for frame buffer, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BANK1_BASE_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BANK1_BASE_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BANK1_BASE_OFFSET (0x734)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BANK1_BASE_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_bank1_base_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x209734L, data);
}
static __inline uint32_t acamera_fpga_video_capture2_dma_writer_bank1_base_read(uintptr_t base) {
    return system_hw_read_32(0x209734L);
}
// ------------------------------------------------------------------------------ //
// Register: bank2_base
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// bank 2 base address for frame buffer, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BANK2_BASE_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BANK2_BASE_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BANK2_BASE_OFFSET (0x738)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BANK2_BASE_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_bank2_base_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x209738L, data);
}
static __inline uint32_t acamera_fpga_video_capture2_dma_writer_bank2_base_read(uintptr_t base) {
    return system_hw_read_32(0x209738L);
}
// ------------------------------------------------------------------------------ //
// Register: bank3_base
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// bank 3 base address for frame buffer, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BANK3_BASE_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BANK3_BASE_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BANK3_BASE_OFFSET (0x73c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BANK3_BASE_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_bank3_base_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x20973cL, data);
}
static __inline uint32_t acamera_fpga_video_capture2_dma_writer_bank3_base_read(uintptr_t base) {
    return system_hw_read_32(0x20973cL);
}
// ------------------------------------------------------------------------------ //
// Register: bank4_base
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// bank 4 base address for frame buffer, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BANK4_BASE_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BANK4_BASE_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BANK4_BASE_OFFSET (0x740)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BANK4_BASE_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_bank4_base_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x209740L, data);
}
static __inline uint32_t acamera_fpga_video_capture2_dma_writer_bank4_base_read(uintptr_t base) {
    return system_hw_read_32(0x209740L);
}
// ------------------------------------------------------------------------------ //
// Register: max bank
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// highest bank*_base to use for frame writes before recycling to bank0_base, only 0 to 4 are valid
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_MAX_BANK_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_MAX_BANK_DATASIZE (3)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_MAX_BANK_OFFSET (0x744)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_MAX_BANK_MASK (0x7)

// args: data (3-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_max_bank_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209744L);
    system_hw_write_32(0x209744L, (((uint32_t) (data & 0x7)) << 0) | (curr & 0xfffffff8));
}
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_max_bank_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209744L) & 0x7) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: bank0 restart
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0 = normal operation, 1= restart bank counter to bank0 for next frame write
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BANK0_RESTART_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BANK0_RESTART_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BANK0_RESTART_OFFSET (0x744)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BANK0_RESTART_MASK (0x8)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_bank0_restart_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209744L);
    system_hw_write_32(0x209744L, (((uint32_t) (data & 0x1)) << 3) | (curr & 0xfffffff7));
}
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_bank0_restart_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209744L) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: Line_offset
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//    Indicates the offset in bytes from the start of one line to the next line.
//    This value should be equal to or larger than one line of image data and should be word-aligned
//    
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_LINE_OFFSET_DEFAULT (0x4000)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_LINE_OFFSET_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_LINE_OFFSET_OFFSET (0x748)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_LINE_OFFSET_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_line_offset_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x209748L, data);
}
static __inline uint32_t acamera_fpga_video_capture2_dma_writer_line_offset_read(uintptr_t base) {
    return system_hw_read_32(0x209748L);
}
// ------------------------------------------------------------------------------ //
// Register: frame write cancel
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0 = normal operation, 1= cancel current/future frame write(s), any unstarted AXI bursts cancelled and fifo flushed
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_FRAME_WRITE_CANCEL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_FRAME_WRITE_CANCEL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_FRAME_WRITE_CANCEL_OFFSET (0x74c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_FRAME_WRITE_CANCEL_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_frame_write_cancel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20974cL);
    system_hw_write_32(0x20974cL, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_frame_write_cancel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20974cL) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: axi_port_enable
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// enables axi, active high, 1=enables axi write transfers, 0= reset axi domain( via reset synchroniser)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_PORT_ENABLE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_PORT_ENABLE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_PORT_ENABLE_OFFSET (0x74c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_PORT_ENABLE_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_axi_port_enable_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20974cL);
    system_hw_write_32(0x20974cL, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_axi_port_enable_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20974cL) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: wbank curr
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// write bank currently active. valid values =0-4. updated at start of frame write
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBANK_CURR_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBANK_CURR_DATASIZE (3)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBANK_CURR_OFFSET (0x750)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBANK_CURR_MASK (0x7)

// args: data (3-bit)
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_wbank_curr_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209750L) & 0x7) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: wbank last
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// write bank last active. valid values = 0-4. updated at start of frame write
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBANK_LAST_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBANK_LAST_DATASIZE (3)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBANK_LAST_OFFSET (0x750)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBANK_LAST_MASK (0x38)

// args: data (3-bit)
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_wbank_last_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209750L) & 0x38) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: wbank active
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1 = wbank_curr is being written to. Goes high at start of writes, low at last write transfer/completion on axi. 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBANK_ACTIVE_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBANK_ACTIVE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBANK_ACTIVE_OFFSET (0x754)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBANK_ACTIVE_MASK (0x1)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_wbank_active_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209754L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: wbank start
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1 = High pulse at start of frame write to bank. 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBANK_START_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBANK_START_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBANK_START_OFFSET (0x754)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBANK_START_MASK (0x2)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_wbank_start_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209754L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: wbank stop
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1 = High pulse at end of frame write to bank. 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBANK_STOP_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBANK_STOP_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBANK_STOP_OFFSET (0x754)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBANK_STOP_MASK (0x4)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_wbank_stop_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209754L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: wbase curr
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// currently active bank base addr - in bytes. updated at start of frame write
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBASE_CURR_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBASE_CURR_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBASE_CURR_OFFSET (0x758)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBASE_CURR_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture2_dma_writer_wbase_curr_read(uintptr_t base) {
    return system_hw_read_32(0x209758L);
}
// ------------------------------------------------------------------------------ //
// Register: wbase last
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// last active bank base addr - in bytes. Updated at start of frame write
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBASE_LAST_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBASE_LAST_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBASE_LAST_OFFSET (0x75c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WBASE_LAST_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture2_dma_writer_wbase_last_read(uintptr_t base) {
    return system_hw_read_32(0x20975cL);
}
// ------------------------------------------------------------------------------ //
// Register: frame icount
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// count of incomming frames (starts) to vdma_writer on video input, non resetable, rolls over, updates at pixel 1 of new frame on video in
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_FRAME_ICOUNT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_FRAME_ICOUNT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_FRAME_ICOUNT_OFFSET (0x760)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_FRAME_ICOUNT_MASK (0xffff)

// args: data (16-bit)
static __inline uint16_t acamera_fpga_video_capture2_dma_writer_frame_icount_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209760L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: frame wcount
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// count of outgoing frame writes (starts) from vdma_writer sent to AXI output, non resetable, rolls over, updates at pixel 1 of new frame on video in
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_FRAME_WCOUNT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_FRAME_WCOUNT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_FRAME_WCOUNT_OFFSET (0x764)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_FRAME_WCOUNT_MASK (0xffff)

// args: data (16-bit)
static __inline uint16_t acamera_fpga_video_capture2_dma_writer_frame_wcount_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209764L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: clear alarms
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0>1 transition(synchronous detection) causes local axi/video alarm clear
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_CLEAR_ALARMS_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_CLEAR_ALARMS_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_CLEAR_ALARMS_OFFSET (0x768)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_CLEAR_ALARMS_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_clear_alarms_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209768L);
    system_hw_write_32(0x209768L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_clear_alarms_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209768L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: max_burst_length_is_8
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1= Reduce default AXI max_burst_length from 16 to 8, 0= Dont reduce
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_MAX_BURST_LENGTH_IS_8_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_MAX_BURST_LENGTH_IS_8_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_MAX_BURST_LENGTH_IS_8_OFFSET (0x768)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_MAX_BURST_LENGTH_IS_8_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_max_burst_length_is_8_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209768L);
    system_hw_write_32(0x209768L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_max_burst_length_is_8_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209768L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: max_burst_length_is_4
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1= Reduce default AXI max_burst_length from 16 to 4, 0= Dont reduce( has priority overmax_burst_length_is_8!)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_MAX_BURST_LENGTH_IS_4_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_MAX_BURST_LENGTH_IS_4_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_MAX_BURST_LENGTH_IS_4_OFFSET (0x768)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_MAX_BURST_LENGTH_IS_4_MASK (0x4)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_max_burst_length_is_4_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209768L);
    system_hw_write_32(0x209768L, (((uint32_t) (data & 0x1)) << 2) | (curr & 0xfffffffb));
}
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_max_burst_length_is_4_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209768L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: write timeout disable
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//    At end of frame an optional timeout is applied to wait for AXI writes to completed/accepted befotre caneclling and flushing.
//    0= Timeout Enabled, timeout count can decrement.
//    1 = Disable timeout, timeout count can't decrement.
//    
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WRITE_TIMEOUT_DISABLE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WRITE_TIMEOUT_DISABLE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WRITE_TIMEOUT_DISABLE_OFFSET (0x768)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WRITE_TIMEOUT_DISABLE_MASK (0x8)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_write_timeout_disable_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209768L);
    system_hw_write_32(0x209768L, (((uint32_t) (data & 0x1)) << 3) | (curr & 0xfffffff7));
}
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_write_timeout_disable_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209768L) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: awmaxwait_limit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// awvalid maxwait limit(cycles) to raise axi_fail_awmaxwait alarm . zero disables alarm raise.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AWMAXWAIT_LIMIT_DEFAULT (0x00)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AWMAXWAIT_LIMIT_DATASIZE (8)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AWMAXWAIT_LIMIT_OFFSET (0x76c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AWMAXWAIT_LIMIT_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_awmaxwait_limit_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20976cL);
    system_hw_write_32(0x20976cL, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_awmaxwait_limit_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20976cL) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: wmaxwait_limit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// wvalid maxwait limit(cycles) to raise axi_fail_wmaxwait alarm . zero disables alarm raise
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WMAXWAIT_LIMIT_DEFAULT (0x00)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WMAXWAIT_LIMIT_DATASIZE (8)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WMAXWAIT_LIMIT_OFFSET (0x770)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WMAXWAIT_LIMIT_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_wmaxwait_limit_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209770L);
    system_hw_write_32(0x209770L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_wmaxwait_limit_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209770L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: wxact_ostand_limit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// number oustsanding write transactions(bursts)(responses..1 per burst) limit to raise axi_fail_wxact_ostand. zero disables alarm raise
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WXACT_OSTAND_LIMIT_DEFAULT (0x00)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WXACT_OSTAND_LIMIT_DATASIZE (8)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WXACT_OSTAND_LIMIT_OFFSET (0x774)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_WXACT_OSTAND_LIMIT_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_wxact_ostand_limit_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209774L);
    system_hw_write_32(0x209774L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_wxact_ostand_limit_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209774L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: axi_fail_bresp
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high to indicate bad  bresp captured 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_FAIL_BRESP_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_FAIL_BRESP_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_FAIL_BRESP_OFFSET (0x778)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_FAIL_BRESP_MASK (0x1)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_axi_fail_bresp_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209778L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: axi_fail_awmaxwait
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high when awmaxwait_limit reached 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_FAIL_AWMAXWAIT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_FAIL_AWMAXWAIT_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_FAIL_AWMAXWAIT_OFFSET (0x778)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_FAIL_AWMAXWAIT_MASK (0x2)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_axi_fail_awmaxwait_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209778L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: axi_fail_wmaxwait
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high when wmaxwait_limit reached 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_FAIL_WMAXWAIT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_FAIL_WMAXWAIT_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_FAIL_WMAXWAIT_OFFSET (0x778)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_FAIL_WMAXWAIT_MASK (0x4)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_axi_fail_wmaxwait_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209778L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: axi_fail_wxact_ostand
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high when wxact_ostand_limit reached 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_FAIL_WXACT_OSTAND_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_FAIL_WXACT_OSTAND_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_FAIL_WXACT_OSTAND_OFFSET (0x778)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_FAIL_WXACT_OSTAND_MASK (0x8)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_axi_fail_wxact_ostand_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209778L) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: vi_fail_active_width
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high to indicate mismatched active_width detected 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_VI_FAIL_ACTIVE_WIDTH_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_VI_FAIL_ACTIVE_WIDTH_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_VI_FAIL_ACTIVE_WIDTH_OFFSET (0x778)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_VI_FAIL_ACTIVE_WIDTH_MASK (0x10)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_vi_fail_active_width_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209778L) & 0x10) >> 4);
}
// ------------------------------------------------------------------------------ //
// Register: vi_fail_active_height
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high to indicate mismatched active_height detected ( also raised on missing field!) 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_VI_FAIL_ACTIVE_HEIGHT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_VI_FAIL_ACTIVE_HEIGHT_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_VI_FAIL_ACTIVE_HEIGHT_OFFSET (0x778)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_VI_FAIL_ACTIVE_HEIGHT_MASK (0x20)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_vi_fail_active_height_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209778L) & 0x20) >> 5);
}
// ------------------------------------------------------------------------------ //
// Register: vi_fail_interline_blanks
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high to indicate interline blanking below min 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_VI_FAIL_INTERLINE_BLANKS_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_VI_FAIL_INTERLINE_BLANKS_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_VI_FAIL_INTERLINE_BLANKS_OFFSET (0x778)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_VI_FAIL_INTERLINE_BLANKS_MASK (0x40)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_vi_fail_interline_blanks_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209778L) & 0x40) >> 6);
}
// ------------------------------------------------------------------------------ //
// Register: vi_fail_interframe_blanks
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high to indicate interframe blanking below min 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_VI_FAIL_INTERFRAME_BLANKS_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_VI_FAIL_INTERFRAME_BLANKS_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_VI_FAIL_INTERFRAME_BLANKS_OFFSET (0x778)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_VI_FAIL_INTERFRAME_BLANKS_MASK (0x80)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_vi_fail_interframe_blanks_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209778L) & 0x80) >> 7);
}
// ------------------------------------------------------------------------------ //
// Register: video_alarm
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  active high, problem found on video port(s) ( active width/height or interline/frame blanks failure) 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_VIDEO_ALARM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_VIDEO_ALARM_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_VIDEO_ALARM_OFFSET (0x77c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_VIDEO_ALARM_MASK (0x1)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_video_alarm_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20977cL) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: axi_alarm
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  active high, problem found on axi port(s)( bresp or awmaxwait or wmaxwait or wxact_ostand failure ) 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_ALARM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_ALARM_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_ALARM_OFFSET (0x77c)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_AXI_ALARM_MASK (0x2)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture2_dma_writer_axi_alarm_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20977cL) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: blk_config
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// block configuration (reserved)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BLK_CONFIG_DEFAULT (0x0000)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BLK_CONFIG_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BLK_CONFIG_OFFSET (0x780)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BLK_CONFIG_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture2_dma_writer_blk_config_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x209780L, data);
}
static __inline uint32_t acamera_fpga_video_capture2_dma_writer_blk_config_read(uintptr_t base) {
    return system_hw_read_32(0x209780L);
}
// ------------------------------------------------------------------------------ //
// Register: blk_status
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// block status output (reserved)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BLK_STATUS_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BLK_STATUS_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BLK_STATUS_OFFSET (0x784)
#define ACAMERA_FPGA_VIDEO_CAPTURE2_DMA_WRITER_BLK_STATUS_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture2_dma_writer_blk_status_read(uintptr_t base) {
    return system_hw_read_32(0x209784L);
}
// ------------------------------------------------------------------------------ //
// Group: Video-Capture-3 Input port
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Controls video input port.  
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: preset
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//        Allows selection of various input port presets for standard sensor inputs.  See ISP Guide for details of available presets.
//        0-14: Frequently used presets.  If using one of available presets, remaining bits in registers 0x100 and 0x104 are not used.
//        15:   Input port configured according to registers in 0x100 and 0x104.  Consult Apical support for special input port requirements.
//      
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_PRESET_DEFAULT (2)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_PRESET_DATASIZE (4)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_PRESET_OFFSET (0x788)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_PRESET_MASK (0xf)

// args: data (4-bit)
static __inline void acamera_fpga_video_capture3_input_port_preset_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209788L);
    system_hw_write_32(0x209788L, (((uint32_t) (data & 0xf)) << 0) | (curr & 0xfffffff0));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_preset_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209788L) & 0xf) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: vs_use field
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VS_USE_FIELD_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VS_USE_FIELD_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VS_USE_FIELD_OFFSET (0x788)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VS_USE_FIELD_MASK (0x100)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VS_USE_FIELD_USE_VSYNC_I_PORT_FOR_VERTICAL_SYNC (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VS_USE_FIELD_USE_FIELD_I_PORT_FOR_VERTICAL_SYNC (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_vs_use_field_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209788L);
    system_hw_write_32(0x209788L, (((uint32_t) (data & 0x1)) << 8) | (curr & 0xfffffeff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_vs_use_field_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209788L) & 0x100) >> 8);
}
// ------------------------------------------------------------------------------ //
// Register: vs toggle
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VS_TOGGLE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VS_TOGGLE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VS_TOGGLE_OFFSET (0x788)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VS_TOGGLE_MASK (0x200)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VS_TOGGLE_VSYNC_IS_PULSETYPE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VS_TOGGLE_VSYNC_IS_TOGGLETYPE_FIELD_SIGNAL (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_vs_toggle_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209788L);
    system_hw_write_32(0x209788L, (((uint32_t) (data & 0x1)) << 9) | (curr & 0xfffffdff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_vs_toggle_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209788L) & 0x200) >> 9);
}
// ------------------------------------------------------------------------------ //
// Register: vs polarity
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VS_POLARITY_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VS_POLARITY_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VS_POLARITY_OFFSET (0x788)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VS_POLARITY_MASK (0x400)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VS_POLARITY_HORIZONTAL_COUNTER_RESET_ON_RISING_EDGE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VS_POLARITY_HORIZONTAL_COUNTER_RESET_ON_FALLING_EDGE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_vs_polarity_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209788L);
    system_hw_write_32(0x209788L, (((uint32_t) (data & 0x1)) << 10) | (curr & 0xfffffbff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_vs_polarity_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209788L) & 0x400) >> 10);
}
// ------------------------------------------------------------------------------ //
// Register: vs_polarity acl
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VS_POLARITY_ACL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VS_POLARITY_ACL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VS_POLARITY_ACL_OFFSET (0x788)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VS_POLARITY_ACL_MASK (0x800)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VS_POLARITY_ACL_DONT_INVERT_POLARITY_FOR_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VS_POLARITY_ACL_INVERT_POLARITY_FOR_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_vs_polarity_acl_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209788L);
    system_hw_write_32(0x209788L, (((uint32_t) (data & 0x1)) << 11) | (curr & 0xfffff7ff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_vs_polarity_acl_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209788L) & 0x800) >> 11);
}
// ------------------------------------------------------------------------------ //
// Register: hs_use acl
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_USE_ACL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_USE_ACL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_USE_ACL_OFFSET (0x788)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_USE_ACL_MASK (0x1000)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_USE_ACL_USE_HSYNC_I_PORT_FOR_ACTIVELINE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_USE_ACL_USE_ACL_I_PORT_FOR_ACTIVELINE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_hs_use_acl_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209788L);
    system_hw_write_32(0x209788L, (((uint32_t) (data & 0x1)) << 12) | (curr & 0xffffefff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_hs_use_acl_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209788L) & 0x1000) >> 12);
}
// ------------------------------------------------------------------------------ //
// Register: vc_c select
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VC_C_SELECT_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VC_C_SELECT_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VC_C_SELECT_OFFSET (0x788)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VC_C_SELECT_MASK (0x4000)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VC_C_SELECT_VERTICAL_COUNTER_COUNTS_ON_HS (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VC_C_SELECT_VERTICAL_COUNTER_COUNTS_ON_HORIZONTAL_COUNTER_OVERFLOW_OR_RESET (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_vc_c_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209788L);
    system_hw_write_32(0x209788L, (((uint32_t) (data & 0x1)) << 14) | (curr & 0xffffbfff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_vc_c_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209788L) & 0x4000) >> 14);
}
// ------------------------------------------------------------------------------ //
// Register: vc_r select
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VC_R_SELECT_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VC_R_SELECT_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VC_R_SELECT_OFFSET (0x788)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VC_R_SELECT_MASK (0x8000)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VC_R_SELECT_VERTICAL_COUNTER_IS_RESET_ON_EDGE_OF_VS (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VC_R_SELECT_VERTICAL_COUNTER_IS_RESET_AFTER_TIMEOUT_ON_HS (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_vc_r_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209788L);
    system_hw_write_32(0x209788L, (((uint32_t) (data & 0x1)) << 15) | (curr & 0xffff7fff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_vc_r_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209788L) & 0x8000) >> 15);
}
// ------------------------------------------------------------------------------ //
// Register: hs_xor vs
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_XOR_VS_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_XOR_VS_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_XOR_VS_OFFSET (0x788)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_XOR_VS_MASK (0x10000)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_XOR_VS_NORMAL_MODE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_XOR_VS_HVALID__HSYNC_XOR_VSYNC (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_hs_xor_vs_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209788L);
    system_hw_write_32(0x209788L, (((uint32_t) (data & 0x1)) << 16) | (curr & 0xfffeffff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_hs_xor_vs_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209788L) & 0x10000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: hs polarity
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_POLARITY_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_POLARITY_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_POLARITY_OFFSET (0x788)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_POLARITY_MASK (0x20000)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_POLARITY_DONT_INVERT_POLARITY_OF_HS_FOR_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_POLARITY_INVERT_POLARITY_OF_HS_FOR_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_hs_polarity_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209788L);
    system_hw_write_32(0x209788L, (((uint32_t) (data & 0x1)) << 17) | (curr & 0xfffdffff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_hs_polarity_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209788L) & 0x20000) >> 17);
}
// ------------------------------------------------------------------------------ //
// Register: hs_polarity acl
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_POLARITY_ACL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_POLARITY_ACL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_POLARITY_ACL_OFFSET (0x788)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_POLARITY_ACL_MASK (0x40000)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_POLARITY_ACL_DONT_INVERT_POLARITY_OF_HS_FOR_HS_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_POLARITY_ACL_INVERT_POLARITY_OF_HS_FOR_HS_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_hs_polarity_acl_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209788L);
    system_hw_write_32(0x209788L, (((uint32_t) (data & 0x1)) << 18) | (curr & 0xfffbffff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_hs_polarity_acl_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209788L) & 0x40000) >> 18);
}
// ------------------------------------------------------------------------------ //
// Register: hs_polarity hs
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_POLARITY_HS_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_POLARITY_HS_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_POLARITY_HS_OFFSET (0x788)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_POLARITY_HS_MASK (0x80000)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_POLARITY_HS_HORIZONTAL_COUNTER_IS_RESET_ON_RISING_EDGE_OF_HS (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_POLARITY_HS_HORIZONTAL_COUNTER_IS_RESET_ON_VSYNC_EG_WHEN_HSYNC_IS_NOT_AVAILABLE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_hs_polarity_hs_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209788L);
    system_hw_write_32(0x209788L, (((uint32_t) (data & 0x1)) << 19) | (curr & 0xfff7ffff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_hs_polarity_hs_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209788L) & 0x80000) >> 19);
}
// ------------------------------------------------------------------------------ //
// Register: hs_polarity vc
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_POLARITY_VC_DEFAULT (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_POLARITY_VC_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_POLARITY_VC_OFFSET (0x788)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_POLARITY_VC_MASK (0x100000)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_POLARITY_VC_VERTICAL_COUNTER_INCREMENTS_ON_RISING_EDGE_OF_HS (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HS_POLARITY_VC_VERTICAL_COUNTER_INCREMENTS_ON_FALLING_EDGE_OF_HS (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_hs_polarity_vc_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209788L);
    system_hw_write_32(0x209788L, (((uint32_t) (data & 0x1)) << 20) | (curr & 0xffefffff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_hs_polarity_vc_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209788L) & 0x100000) >> 20);
}
// ------------------------------------------------------------------------------ //
// Register: hc_r select
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_R_SELECT_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_R_SELECT_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_R_SELECT_OFFSET (0x788)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_R_SELECT_MASK (0x800000)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_R_SELECT_VERTICAL_COUNTER_IS_RESET_ON_RISING_EDGE_OF_HS (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_R_SELECT_VERTICAL_COUNTER_IS_RESET_ON_RISING_EDGE_OF_VS (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_hc_r_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209788L);
    system_hw_write_32(0x209788L, (((uint32_t) (data & 0x1)) << 23) | (curr & 0xff7fffff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_hc_r_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209788L) & 0x800000) >> 23);
}
// ------------------------------------------------------------------------------ //
// Register: acl polarity
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACL_POLARITY_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACL_POLARITY_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACL_POLARITY_OFFSET (0x788)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACL_POLARITY_MASK (0x1000000)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACL_POLARITY_DONT_INVERT_ACL_I_FOR_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACL_POLARITY_INVERT_ACL_I_FOR_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_acl_polarity_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209788L);
    system_hw_write_32(0x209788L, (((uint32_t) (data & 0x1)) << 24) | (curr & 0xfeffffff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_acl_polarity_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209788L) & 0x1000000) >> 24);
}
// ------------------------------------------------------------------------------ //
// Register: field polarity
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELD_POLARITY_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELD_POLARITY_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELD_POLARITY_OFFSET (0x78c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELD_POLARITY_MASK (0x1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELD_POLARITY_DONT_INVERT_FIELD_I_FOR_FIELD_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELD_POLARITY_INVERT_FIELD_I_FOR_FIELD_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_field_polarity_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20978cL);
    system_hw_write_32(0x20978cL, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_field_polarity_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20978cL) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: field toggle
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELD_TOGGLE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELD_TOGGLE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELD_TOGGLE_OFFSET (0x78c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELD_TOGGLE_MASK (0x2)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELD_TOGGLE_FIELD_IS_PULSETYPE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELD_TOGGLE_FIELD_IS_TOGGLETYPE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_field_toggle_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20978cL);
    system_hw_write_32(0x20978cL, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_field_toggle_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20978cL) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: aclg window0
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_WINDOW0_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_WINDOW0_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_WINDOW0_OFFSET (0x78c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_WINDOW0_MASK (0x100)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_WINDOW0_EXCLUDE_WINDOW0_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_WINDOW0_INCLUDE_WINDOW0_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_aclg_window0_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20978cL);
    system_hw_write_32(0x20978cL, (((uint32_t) (data & 0x1)) << 8) | (curr & 0xfffffeff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_aclg_window0_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20978cL) & 0x100) >> 8);
}
// ------------------------------------------------------------------------------ //
// Register: aclg hsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_HSYNC_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_HSYNC_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_HSYNC_OFFSET (0x78c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_HSYNC_MASK (0x200)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_HSYNC_EXCLUDE_HSYNC_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_HSYNC_INCLUDE_HSYNC_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_aclg_hsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20978cL);
    system_hw_write_32(0x20978cL, (((uint32_t) (data & 0x1)) << 9) | (curr & 0xfffffdff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_aclg_hsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20978cL) & 0x200) >> 9);
}
// ------------------------------------------------------------------------------ //
// Register: aclg window2
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_WINDOW2_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_WINDOW2_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_WINDOW2_OFFSET (0x78c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_WINDOW2_MASK (0x400)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_WINDOW2_EXCLUDE_WINDOW2_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_WINDOW2_INCLUDE_WINDOW2_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_aclg_window2_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20978cL);
    system_hw_write_32(0x20978cL, (((uint32_t) (data & 0x1)) << 10) | (curr & 0xfffffbff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_aclg_window2_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20978cL) & 0x400) >> 10);
}
// ------------------------------------------------------------------------------ //
// Register: aclg acl
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_ACL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_ACL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_ACL_OFFSET (0x78c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_ACL_MASK (0x800)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_ACL_EXCLUDE_ACL_I_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_ACL_INCLUDE_ACL_I_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_aclg_acl_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20978cL);
    system_hw_write_32(0x20978cL, (((uint32_t) (data & 0x1)) << 11) | (curr & 0xfffff7ff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_aclg_acl_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20978cL) & 0x800) >> 11);
}
// ------------------------------------------------------------------------------ //
// Register: aclg vsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_VSYNC_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_VSYNC_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_VSYNC_OFFSET (0x78c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_VSYNC_MASK (0x1000)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_VSYNC_EXCLUDE_VSYNC_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_ACLG_VSYNC_INCLUDE_VSYNC_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_aclg_vsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20978cL);
    system_hw_write_32(0x20978cL, (((uint32_t) (data & 0x1)) << 12) | (curr & 0xffffefff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_aclg_vsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20978cL) & 0x1000) >> 12);
}
// ------------------------------------------------------------------------------ //
// Register: hsg window1
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HSG_WINDOW1_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HSG_WINDOW1_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HSG_WINDOW1_OFFSET (0x78c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HSG_WINDOW1_MASK (0x10000)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HSG_WINDOW1_EXCLUDE_WINDOW1_SIGNAL_IN_HS_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HSG_WINDOW1_INCLUDE_WINDOW1_SIGNAL_IN_HS_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_hsg_window1_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20978cL);
    system_hw_write_32(0x20978cL, (((uint32_t) (data & 0x1)) << 16) | (curr & 0xfffeffff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_hsg_window1_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20978cL) & 0x10000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: hsg hsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HSG_HSYNC_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HSG_HSYNC_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HSG_HSYNC_OFFSET (0x78c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HSG_HSYNC_MASK (0x20000)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HSG_HSYNC_EXCLUDE_HSYNC_SIGNAL_IN_HS_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HSG_HSYNC_INCLUDE_HSYNC_SIGNAL_IN_HS_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_hsg_hsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20978cL);
    system_hw_write_32(0x20978cL, (((uint32_t) (data & 0x1)) << 17) | (curr & 0xfffdffff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_hsg_hsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20978cL) & 0x20000) >> 17);
}
// ------------------------------------------------------------------------------ //
// Register: hsg vsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HSG_VSYNC_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HSG_VSYNC_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HSG_VSYNC_OFFSET (0x78c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HSG_VSYNC_MASK (0x40000)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HSG_VSYNC_EXCLUDE_VSYNC_SIGNAL_IN_HS_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HSG_VSYNC_INCLUDE_VSYNC_SIGNAL_IN_HS_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_hsg_vsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20978cL);
    system_hw_write_32(0x20978cL, (((uint32_t) (data & 0x1)) << 18) | (curr & 0xfffbffff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_hsg_vsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20978cL) & 0x40000) >> 18);
}
// ------------------------------------------------------------------------------ //
// Register: hsg window2
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HSG_WINDOW2_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HSG_WINDOW2_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HSG_WINDOW2_OFFSET (0x78c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HSG_WINDOW2_MASK (0x80000)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HSG_WINDOW2_EXCLUDE_WINDOW2_SIGNAL_IN_HS_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HSG_WINDOW2_INCLUDE_WINDOW2_SIGNAL_IN_HS_GATE_MASK_OUT_SPURIOUS_HS_DURING_BLANK (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_hsg_window2_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20978cL);
    system_hw_write_32(0x20978cL, (((uint32_t) (data & 0x1)) << 19) | (curr & 0xfff7ffff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_hsg_window2_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20978cL) & 0x80000) >> 19);
}
// ------------------------------------------------------------------------------ //
// Register: fieldg vsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELDG_VSYNC_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELDG_VSYNC_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELDG_VSYNC_OFFSET (0x78c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELDG_VSYNC_MASK (0x1000000)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELDG_VSYNC_EXCLUDE_VSYNC_SIGNAL_IN_FIELD_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELDG_VSYNC_INCLUDE_VSYNC_SIGNAL_IN_FIELD_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_fieldg_vsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20978cL);
    system_hw_write_32(0x20978cL, (((uint32_t) (data & 0x1)) << 24) | (curr & 0xfeffffff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_fieldg_vsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20978cL) & 0x1000000) >> 24);
}
// ------------------------------------------------------------------------------ //
// Register: fieldg window2
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELDG_WINDOW2_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELDG_WINDOW2_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELDG_WINDOW2_OFFSET (0x78c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELDG_WINDOW2_MASK (0x2000000)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELDG_WINDOW2_EXCLUDE_WINDOW2_SIGNAL_IN_FIELD_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELDG_WINDOW2_INCLUDE_WINDOW2_SIGNAL_IN_FIELD_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_fieldg_window2_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20978cL);
    system_hw_write_32(0x20978cL, (((uint32_t) (data & 0x1)) << 25) | (curr & 0xfdffffff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_fieldg_window2_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20978cL) & 0x2000000) >> 25);
}
// ------------------------------------------------------------------------------ //
// Register: fieldg field
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELDG_FIELD_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELDG_FIELD_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELDG_FIELD_OFFSET (0x78c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELDG_FIELD_MASK (0x4000000)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELDG_FIELD_EXCLUDE_FIELD_I_SIGNAL_IN_FIELD_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELDG_FIELD_INCLUDE_FIELD_I_SIGNAL_IN_FIELD_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_fieldg_field_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20978cL);
    system_hw_write_32(0x20978cL, (((uint32_t) (data & 0x1)) << 26) | (curr & 0xfbffffff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_fieldg_field_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20978cL) & 0x4000000) >> 26);
}
// ------------------------------------------------------------------------------ //
// Register: field mode
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELD_MODE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELD_MODE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELD_MODE_OFFSET (0x78c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELD_MODE_MASK (0x8000000)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELD_MODE_PULSE_FIELD (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FIELD_MODE_TOGGLE_FIELD (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_field_mode_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20978cL);
    system_hw_write_32(0x20978cL, (((uint32_t) (data & 0x1)) << 27) | (curr & 0xf7ffffff));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_field_mode_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20978cL) & 0x8000000) >> 27);
}
// ------------------------------------------------------------------------------ //
// Register: hc limit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// horizontal counter limit value (counts: 0,1,...hc_limit-1,hc_limit,0,1,...)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_LIMIT_DEFAULT (0xFFFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_LIMIT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_LIMIT_OFFSET (0x790)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_LIMIT_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture3_input_port_hc_limit_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209790L);
    system_hw_write_32(0x209790L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture3_input_port_hc_limit_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209790L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: hc start0
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window0 start for ACL gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_START0_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_START0_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_START0_OFFSET (0x794)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_START0_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture3_input_port_hc_start0_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209794L);
    system_hw_write_32(0x209794L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture3_input_port_hc_start0_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209794L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: hc size0
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window0 size for ACL gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_SIZE0_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_SIZE0_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_SIZE0_OFFSET (0x798)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_SIZE0_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture3_input_port_hc_size0_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209798L);
    system_hw_write_32(0x209798L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture3_input_port_hc_size0_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209798L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: hc start1
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window1 start for HS gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_START1_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_START1_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_START1_OFFSET (0x79c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_START1_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture3_input_port_hc_start1_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20979cL);
    system_hw_write_32(0x20979cL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture3_input_port_hc_start1_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20979cL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: hc size1
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window1 size for HS gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_SIZE1_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_SIZE1_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_SIZE1_OFFSET (0x7a0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_HC_SIZE1_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture3_input_port_hc_size1_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2097a0L);
    system_hw_write_32(0x2097a0L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture3_input_port_hc_size1_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2097a0L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: vc limit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// vertical counter limit value (counts: 0,1,...vc_limit-1,vc_limit,0,1,...)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VC_LIMIT_DEFAULT (0xFFFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VC_LIMIT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VC_LIMIT_OFFSET (0x7a4)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VC_LIMIT_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture3_input_port_vc_limit_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2097a4L);
    system_hw_write_32(0x2097a4L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture3_input_port_vc_limit_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2097a4L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: vc start
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window2 start for ACL gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VC_START_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VC_START_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VC_START_OFFSET (0x7a8)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VC_START_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture3_input_port_vc_start_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2097a8L);
    system_hw_write_32(0x2097a8L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture3_input_port_vc_start_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2097a8L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: vc size
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window2 size for ACL gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VC_SIZE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VC_SIZE_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VC_SIZE_OFFSET (0x7ac)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_VC_SIZE_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture3_input_port_vc_size_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2097acL);
    system_hw_write_32(0x2097acL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture3_input_port_vc_size_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2097acL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: frame width
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// detected frame width.  Read only value.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FRAME_WIDTH_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FRAME_WIDTH_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FRAME_WIDTH_OFFSET (0x7b0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FRAME_WIDTH_MASK (0xffff)

// args: data (16-bit)
static __inline uint16_t acamera_fpga_video_capture3_input_port_frame_width_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2097b0L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: frame height
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// detected frame height.  Read only value.  
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FRAME_HEIGHT_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FRAME_HEIGHT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FRAME_HEIGHT_OFFSET (0x7b4)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FRAME_HEIGHT_MASK (0xffff)

// args: data (16-bit)
static __inline uint16_t acamera_fpga_video_capture3_input_port_frame_height_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2097b4L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: mode request
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Used to stop and start input port.  See ISP guide for further details. 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_MODE_REQUEST_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_MODE_REQUEST_DATASIZE (3)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_MODE_REQUEST_OFFSET (0x7b8)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_MODE_REQUEST_MASK (0x7)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_MODE_REQUEST_SAFE_STOP (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_MODE_REQUEST_SAFE_START (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_MODE_REQUEST_URGENT_STOP (2)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_MODE_REQUEST_URGENT_START (3)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_MODE_REQUEST_RESERVED4 (4)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_MODE_REQUEST_SAFER_START (5)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_MODE_REQUEST_RESERVED6 (6)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_MODE_REQUEST_RESERVED7 (7)

// args: data (3-bit)
static __inline void acamera_fpga_video_capture3_input_port_mode_request_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2097b8L);
    system_hw_write_32(0x2097b8L, (((uint32_t) (data & 0x7)) << 0) | (curr & 0xfffffff8));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_mode_request_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2097b8L) & 0x7) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: freeze config
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Used to freeze input port configuration.  Used when multiple register writes are required to change input port configuration. 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FREEZE_CONFIG_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FREEZE_CONFIG_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FREEZE_CONFIG_OFFSET (0x7b8)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FREEZE_CONFIG_MASK (0x80)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FREEZE_CONFIG_NORMAL_OPERATION (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_FREEZE_CONFIG_HOLD_PREVIOUS_INPUT_PORT_CONFIG_STATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_input_port_freeze_config_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2097b8L);
    system_hw_write_32(0x2097b8L, (((uint32_t) (data & 0x1)) << 7) | (curr & 0xffffff7f));
}
static __inline uint8_t acamera_fpga_video_capture3_input_port_freeze_config_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2097b8L) & 0x80) >> 7);
}
// ------------------------------------------------------------------------------ //
// Register: mode status
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//          Used to monitor input port status:
//          bit 0: 1=running, 0=stopped, bits 1,2-reserved
//        
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_MODE_STATUS_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_MODE_STATUS_DATASIZE (3)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_MODE_STATUS_OFFSET (0x7bc)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_INPUT_PORT_MODE_STATUS_MASK (0x7)

// args: data (3-bit)
static __inline uint8_t acamera_fpga_video_capture3_input_port_mode_status_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2097bcL) & 0x7) >> 0);
}
// ------------------------------------------------------------------------------ //
// Group: Video-Capture-3 Frame Stats
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: stats reset
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_STATS_RESET_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_STATS_RESET_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_STATS_RESET_OFFSET (0x7c0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_STATS_RESET_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_frame_stats_stats_reset_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2097c0L);
    system_hw_write_32(0x2097c0L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture3_frame_stats_stats_reset_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2097c0L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: stats hold
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_STATS_HOLD_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_STATS_HOLD_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_STATS_HOLD_OFFSET (0x7c4)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_STATS_HOLD_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_frame_stats_stats_hold_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2097c4L);
    system_hw_write_32(0x2097c4L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture3_frame_stats_stats_hold_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2097c4L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: active width min
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_WIDTH_MIN_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_WIDTH_MIN_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_WIDTH_MIN_OFFSET (0x7d0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_WIDTH_MIN_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture3_frame_stats_active_width_min_read(uintptr_t base) {
    return system_hw_read_32(0x2097d0L);
}
// ------------------------------------------------------------------------------ //
// Register: active width max
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_WIDTH_MAX_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_WIDTH_MAX_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_WIDTH_MAX_OFFSET (0x7d4)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_WIDTH_MAX_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture3_frame_stats_active_width_max_read(uintptr_t base) {
    return system_hw_read_32(0x2097d4L);
}
// ------------------------------------------------------------------------------ //
// Register: active width sum
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_WIDTH_SUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_WIDTH_SUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_WIDTH_SUM_OFFSET (0x7d8)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_WIDTH_SUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture3_frame_stats_active_width_sum_read(uintptr_t base) {
    return system_hw_read_32(0x2097d8L);
}
// ------------------------------------------------------------------------------ //
// Register: active width num
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_WIDTH_NUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_WIDTH_NUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_WIDTH_NUM_OFFSET (0x7dc)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_WIDTH_NUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture3_frame_stats_active_width_num_read(uintptr_t base) {
    return system_hw_read_32(0x2097dcL);
}
// ------------------------------------------------------------------------------ //
// Register: active height min
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_HEIGHT_MIN_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_HEIGHT_MIN_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_HEIGHT_MIN_OFFSET (0x7e0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_HEIGHT_MIN_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture3_frame_stats_active_height_min_read(uintptr_t base) {
    return system_hw_read_32(0x2097e0L);
}
// ------------------------------------------------------------------------------ //
// Register: active height max
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_HEIGHT_MAX_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_HEIGHT_MAX_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_HEIGHT_MAX_OFFSET (0x7e4)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_HEIGHT_MAX_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture3_frame_stats_active_height_max_read(uintptr_t base) {
    return system_hw_read_32(0x2097e4L);
}
// ------------------------------------------------------------------------------ //
// Register: active height sum
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_HEIGHT_SUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_HEIGHT_SUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_HEIGHT_SUM_OFFSET (0x7e8)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_HEIGHT_SUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture3_frame_stats_active_height_sum_read(uintptr_t base) {
    return system_hw_read_32(0x2097e8L);
}
// ------------------------------------------------------------------------------ //
// Register: active height num
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_HEIGHT_NUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_HEIGHT_NUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_HEIGHT_NUM_OFFSET (0x7ec)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_ACTIVE_HEIGHT_NUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture3_frame_stats_active_height_num_read(uintptr_t base) {
    return system_hw_read_32(0x2097ecL);
}
// ------------------------------------------------------------------------------ //
// Register: hblank min
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_HBLANK_MIN_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_HBLANK_MIN_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_HBLANK_MIN_OFFSET (0x7f0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_HBLANK_MIN_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture3_frame_stats_hblank_min_read(uintptr_t base) {
    return system_hw_read_32(0x2097f0L);
}
// ------------------------------------------------------------------------------ //
// Register: hblank max
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_HBLANK_MAX_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_HBLANK_MAX_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_HBLANK_MAX_OFFSET (0x7f4)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_HBLANK_MAX_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture3_frame_stats_hblank_max_read(uintptr_t base) {
    return system_hw_read_32(0x2097f4L);
}
// ------------------------------------------------------------------------------ //
// Register: hblank sum
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_HBLANK_SUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_HBLANK_SUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_HBLANK_SUM_OFFSET (0x7f8)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_HBLANK_SUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture3_frame_stats_hblank_sum_read(uintptr_t base) {
    return system_hw_read_32(0x2097f8L);
}
// ------------------------------------------------------------------------------ //
// Register: hblank num
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_HBLANK_NUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_HBLANK_NUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_HBLANK_NUM_OFFSET (0x7fc)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_HBLANK_NUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture3_frame_stats_hblank_num_read(uintptr_t base) {
    return system_hw_read_32(0x2097fcL);
}
// ------------------------------------------------------------------------------ //
// Register: vblank min
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_VBLANK_MIN_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_VBLANK_MIN_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_VBLANK_MIN_OFFSET (0x800)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_VBLANK_MIN_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture3_frame_stats_vblank_min_read(uintptr_t base) {
    return system_hw_read_32(0x209800L);
}
// ------------------------------------------------------------------------------ //
// Register: vblank max
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_VBLANK_MAX_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_VBLANK_MAX_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_VBLANK_MAX_OFFSET (0x804)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_VBLANK_MAX_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture3_frame_stats_vblank_max_read(uintptr_t base) {
    return system_hw_read_32(0x209804L);
}
// ------------------------------------------------------------------------------ //
// Register: vblank sum
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_VBLANK_SUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_VBLANK_SUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_VBLANK_SUM_OFFSET (0x808)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_VBLANK_SUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture3_frame_stats_vblank_sum_read(uintptr_t base) {
    return system_hw_read_32(0x209808L);
}
// ------------------------------------------------------------------------------ //
// Register: vblank num
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_VBLANK_NUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_VBLANK_NUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_VBLANK_NUM_OFFSET (0x80c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_FRAME_STATS_VBLANK_NUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture3_frame_stats_vblank_num_read(uintptr_t base) {
    return system_hw_read_32(0x20980cL);
}
// ------------------------------------------------------------------------------ //
// Group: Video-Capture-3 video test gen
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Video test generator controls.  See ISP Guide for further details
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: test_pattern_off on
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Test pattern off-on: 0=off, 1=on
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_TEST_PATTERN_OFF_ON_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_TEST_PATTERN_OFF_ON_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_TEST_PATTERN_OFF_ON_OFFSET (0x814)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_TEST_PATTERN_OFF_ON_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_video_test_gen_test_pattern_off_on_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209814L);
    system_hw_write_32(0x209814L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture3_video_test_gen_test_pattern_off_on_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209814L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: bayer_rgb_i sel
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Bayer or rgb select for input video: 0=bayer, 1=rgb
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_BAYER_RGB_I_SEL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_BAYER_RGB_I_SEL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_BAYER_RGB_I_SEL_OFFSET (0x814)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_BAYER_RGB_I_SEL_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_video_test_gen_bayer_rgb_i_sel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209814L);
    system_hw_write_32(0x209814L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_video_capture3_video_test_gen_bayer_rgb_i_sel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209814L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: bayer_rgb_o sel
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Bayer or rgb select for output video: 0=bayer, 1=rgb
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_BAYER_RGB_O_SEL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_BAYER_RGB_O_SEL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_BAYER_RGB_O_SEL_OFFSET (0x814)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_BAYER_RGB_O_SEL_MASK (0x4)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_video_test_gen_bayer_rgb_o_sel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209814L);
    system_hw_write_32(0x209814L, (((uint32_t) (data & 0x1)) << 2) | (curr & 0xfffffffb));
}
static __inline uint8_t acamera_fpga_video_capture3_video_test_gen_bayer_rgb_o_sel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209814L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: pattern type
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Pattern type select: 0=Flat field,1=Horizontal gradient,2=Vertical Gradient,3=Vertical Bars,4=Rectangle,5-255=Default white frame on black
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_PATTERN_TYPE_DEFAULT (0x03)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_PATTERN_TYPE_DATASIZE (8)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_PATTERN_TYPE_OFFSET (0x818)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_PATTERN_TYPE_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_video_capture3_video_test_gen_pattern_type_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209818L);
    system_hw_write_32(0x209818L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_video_capture3_video_test_gen_pattern_type_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209818L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: r backgnd
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Red background  value 16bit, MSB aligned to used width 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_R_BACKGND_DEFAULT (0xFFFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_R_BACKGND_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_R_BACKGND_OFFSET (0x81c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_R_BACKGND_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture3_video_test_gen_r_backgnd_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20981cL);
    system_hw_write_32(0x20981cL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture3_video_test_gen_r_backgnd_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20981cL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: g backgnd
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Green background value 16bit, MSB aligned to used width
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_G_BACKGND_DEFAULT (0xFFFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_G_BACKGND_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_G_BACKGND_OFFSET (0x820)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_G_BACKGND_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture3_video_test_gen_g_backgnd_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209820L);
    system_hw_write_32(0x209820L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture3_video_test_gen_g_backgnd_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209820L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: b backgnd
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Blue background value 16bit, MSB aligned to used width
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_B_BACKGND_DEFAULT (0xFFFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_B_BACKGND_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_B_BACKGND_OFFSET (0x824)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_B_BACKGND_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture3_video_test_gen_b_backgnd_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209824L);
    system_hw_write_32(0x209824L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture3_video_test_gen_b_backgnd_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209824L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: r foregnd
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Red foreground  value 16bit, MSB aligned to used width 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_R_FOREGND_DEFAULT (0x8FFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_R_FOREGND_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_R_FOREGND_OFFSET (0x828)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_R_FOREGND_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture3_video_test_gen_r_foregnd_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209828L);
    system_hw_write_32(0x209828L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture3_video_test_gen_r_foregnd_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209828L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: g foregnd
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Green foreground value 16bit, MSB aligned to used width
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_G_FOREGND_DEFAULT (0x8FFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_G_FOREGND_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_G_FOREGND_OFFSET (0x82c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_G_FOREGND_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture3_video_test_gen_g_foregnd_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20982cL);
    system_hw_write_32(0x20982cL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture3_video_test_gen_g_foregnd_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20982cL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: b foregnd
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Blue foreground value 16bit, MSB aligned to used width
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_B_FOREGND_DEFAULT (0x8FFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_B_FOREGND_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_B_FOREGND_OFFSET (0x830)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_B_FOREGND_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture3_video_test_gen_b_foregnd_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209830L);
    system_hw_write_32(0x209830L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture3_video_test_gen_b_foregnd_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209830L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rgb gradient
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// RGB gradient increment per pixel (0-15)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_RGB_GRADIENT_DEFAULT (0x3CAA)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_RGB_GRADIENT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_RGB_GRADIENT_OFFSET (0x834)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_RGB_GRADIENT_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture3_video_test_gen_rgb_gradient_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209834L);
    system_hw_write_32(0x209834L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture3_video_test_gen_rgb_gradient_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209834L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rgb_gradient start
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// RGB gradient start value 16bit, MSB aligned to used width
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_RGB_GRADIENT_START_DEFAULT (0x0000)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_RGB_GRADIENT_START_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_RGB_GRADIENT_START_OFFSET (0x838)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_RGB_GRADIENT_START_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture3_video_test_gen_rgb_gradient_start_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209838L);
    system_hw_write_32(0x209838L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture3_video_test_gen_rgb_gradient_start_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209838L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rect top
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  Rectangle top line number 1-n
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_RECT_TOP_DEFAULT (0x0001)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_RECT_TOP_DATASIZE (14)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_RECT_TOP_OFFSET (0x83c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_RECT_TOP_MASK (0x3fff)

// args: data (14-bit)
static __inline void acamera_fpga_video_capture3_video_test_gen_rect_top_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20983cL);
    system_hw_write_32(0x20983cL, (((uint32_t) (data & 0x3fff)) << 0) | (curr & 0xffffc000));
}
static __inline uint16_t acamera_fpga_video_capture3_video_test_gen_rect_top_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20983cL) & 0x3fff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rect bot
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  Rectangle bottom line number 1-n
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_RECT_BOT_DEFAULT (0x0100)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_RECT_BOT_DATASIZE (14)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_RECT_BOT_OFFSET (0x840)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_RECT_BOT_MASK (0x3fff)

// args: data (14-bit)
static __inline void acamera_fpga_video_capture3_video_test_gen_rect_bot_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209840L);
    system_hw_write_32(0x209840L, (((uint32_t) (data & 0x3fff)) << 0) | (curr & 0xffffc000));
}
static __inline uint16_t acamera_fpga_video_capture3_video_test_gen_rect_bot_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209840L) & 0x3fff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rect left
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  Rectangle left pixel number 1-n
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_RECT_LEFT_DEFAULT (0x0001)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_RECT_LEFT_DATASIZE (14)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_RECT_LEFT_OFFSET (0x844)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_RECT_LEFT_MASK (0x3fff)

// args: data (14-bit)
static __inline void acamera_fpga_video_capture3_video_test_gen_rect_left_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209844L);
    system_hw_write_32(0x209844L, (((uint32_t) (data & 0x3fff)) << 0) | (curr & 0xffffc000));
}
static __inline uint16_t acamera_fpga_video_capture3_video_test_gen_rect_left_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209844L) & 0x3fff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rect right
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  Rectangle right pixel number 1-n
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_RECT_RIGHT_DEFAULT (0x0100)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_RECT_RIGHT_DATASIZE (14)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_RECT_RIGHT_OFFSET (0x84c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_VIDEO_TEST_GEN_RECT_RIGHT_MASK (0x3fff)

// args: data (14-bit)
static __inline void acamera_fpga_video_capture3_video_test_gen_rect_right_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20984cL);
    system_hw_write_32(0x20984cL, (((uint32_t) (data & 0x3fff)) << 0) | (curr & 0xffffc000));
}
static __inline uint16_t acamera_fpga_video_capture3_video_test_gen_rect_right_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20984cL) & 0x3fff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Group: Video-Capture-3 DMA Writer
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Video-Capture DMA writer controls
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: Format
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Format
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_FORMAT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_FORMAT_DATASIZE (8)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_FORMAT_OFFSET (0x850)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_FORMAT_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_format_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209850L);
    system_hw_write_32(0x209850L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_format_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209850L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Base mode
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Base DMA packing mode for RGB/RAW/YUV etc (see ISP guide)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BASE_MODE_DEFAULT (0x0D)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BASE_MODE_DATASIZE (4)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BASE_MODE_OFFSET (0x850)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BASE_MODE_MASK (0xf)

// args: data (4-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_base_mode_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209850L);
    system_hw_write_32(0x209850L, (((uint32_t) (data & 0xf)) << 0) | (curr & 0xfffffff0));
}
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_base_mode_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209850L) & 0xf) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Plane select
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Plane select for planar base modes.  Only used if planar outputs required.  Not used.  Should be set to 0
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_PLANE_SELECT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_PLANE_SELECT_DATASIZE (2)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_PLANE_SELECT_OFFSET (0x850)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_PLANE_SELECT_MASK (0xc0)

// args: data (2-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_plane_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209850L);
    system_hw_write_32(0x209850L, (((uint32_t) (data & 0x3)) << 6) | (curr & 0xffffff3f));
}
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_plane_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209850L) & 0xc0) >> 6);
}
// ------------------------------------------------------------------------------ //
// Register: single frame
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0 = All frames are written(after frame_write_on= 1), 1= only 1st frame written ( after frame_write_on =1)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_SINGLE_FRAME_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_SINGLE_FRAME_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_SINGLE_FRAME_OFFSET (0x854)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_SINGLE_FRAME_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_single_frame_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209854L);
    system_hw_write_32(0x209854L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_single_frame_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209854L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: frame write on
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//    0 = no frames written(when switched from 1, current frame completes writing before stopping),
//    1= write frame(s) (write single or continous frame(s) )
//    
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_FRAME_WRITE_ON_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_FRAME_WRITE_ON_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_FRAME_WRITE_ON_OFFSET (0x854)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_FRAME_WRITE_ON_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_frame_write_on_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209854L);
    system_hw_write_32(0x209854L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_frame_write_on_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209854L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: half irate
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0 = normal operation , 1= write half(alternate) of input frames( only valid for continuous mode)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_HALF_IRATE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_HALF_IRATE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_HALF_IRATE_OFFSET (0x854)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_HALF_IRATE_MASK (0x4)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_half_irate_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209854L);
    system_hw_write_32(0x209854L, (((uint32_t) (data & 0x1)) << 2) | (curr & 0xfffffffb));
}
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_half_irate_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209854L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: axi xact comp
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0 = dont wait for axi transaction completion at end of frame(just all transfers accepted). 1 = wait for all transactions completed
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_XACT_COMP_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_XACT_COMP_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_XACT_COMP_OFFSET (0x854)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_XACT_COMP_MASK (0x8)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_axi_xact_comp_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209854L);
    system_hw_write_32(0x209854L, (((uint32_t) (data & 0x1)) << 3) | (curr & 0xfffffff7));
}
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_axi_xact_comp_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209854L) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: active width
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Active video width in pixels 128-8000
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_ACTIVE_WIDTH_DEFAULT (0x780)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_ACTIVE_WIDTH_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_ACTIVE_WIDTH_OFFSET (0x858)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_ACTIVE_WIDTH_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_active_width_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209858L);
    system_hw_write_32(0x209858L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture3_dma_writer_active_width_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209858L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: active height
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Active video height in lines 128-8000
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_ACTIVE_HEIGHT_DEFAULT (0x438)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_ACTIVE_HEIGHT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_ACTIVE_HEIGHT_OFFSET (0x85c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_ACTIVE_HEIGHT_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_active_height_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20985cL);
    system_hw_write_32(0x20985cL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture3_dma_writer_active_height_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20985cL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: bank0_base
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// bank 0 base address for frame buffer, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BANK0_BASE_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BANK0_BASE_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BANK0_BASE_OFFSET (0x860)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BANK0_BASE_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_bank0_base_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x209860L, data);
}
static __inline uint32_t acamera_fpga_video_capture3_dma_writer_bank0_base_read(uintptr_t base) {
    return system_hw_read_32(0x209860L);
}
// ------------------------------------------------------------------------------ //
// Register: bank1_base
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// bank 1 base address for frame buffer, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BANK1_BASE_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BANK1_BASE_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BANK1_BASE_OFFSET (0x864)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BANK1_BASE_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_bank1_base_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x209864L, data);
}
static __inline uint32_t acamera_fpga_video_capture3_dma_writer_bank1_base_read(uintptr_t base) {
    return system_hw_read_32(0x209864L);
}
// ------------------------------------------------------------------------------ //
// Register: bank2_base
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// bank 2 base address for frame buffer, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BANK2_BASE_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BANK2_BASE_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BANK2_BASE_OFFSET (0x868)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BANK2_BASE_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_bank2_base_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x209868L, data);
}
static __inline uint32_t acamera_fpga_video_capture3_dma_writer_bank2_base_read(uintptr_t base) {
    return system_hw_read_32(0x209868L);
}
// ------------------------------------------------------------------------------ //
// Register: bank3_base
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// bank 3 base address for frame buffer, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BANK3_BASE_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BANK3_BASE_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BANK3_BASE_OFFSET (0x86c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BANK3_BASE_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_bank3_base_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x20986cL, data);
}
static __inline uint32_t acamera_fpga_video_capture3_dma_writer_bank3_base_read(uintptr_t base) {
    return system_hw_read_32(0x20986cL);
}
// ------------------------------------------------------------------------------ //
// Register: bank4_base
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// bank 4 base address for frame buffer, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BANK4_BASE_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BANK4_BASE_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BANK4_BASE_OFFSET (0x870)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BANK4_BASE_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_bank4_base_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x209870L, data);
}
static __inline uint32_t acamera_fpga_video_capture3_dma_writer_bank4_base_read(uintptr_t base) {
    return system_hw_read_32(0x209870L);
}
// ------------------------------------------------------------------------------ //
// Register: max bank
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// highest bank*_base to use for frame writes before recycling to bank0_base, only 0 to 4 are valid
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_MAX_BANK_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_MAX_BANK_DATASIZE (3)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_MAX_BANK_OFFSET (0x874)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_MAX_BANK_MASK (0x7)

// args: data (3-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_max_bank_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209874L);
    system_hw_write_32(0x209874L, (((uint32_t) (data & 0x7)) << 0) | (curr & 0xfffffff8));
}
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_max_bank_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209874L) & 0x7) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: bank0 restart
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0 = normal operation, 1= restart bank counter to bank0 for next frame write
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BANK0_RESTART_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BANK0_RESTART_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BANK0_RESTART_OFFSET (0x874)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BANK0_RESTART_MASK (0x8)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_bank0_restart_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209874L);
    system_hw_write_32(0x209874L, (((uint32_t) (data & 0x1)) << 3) | (curr & 0xfffffff7));
}
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_bank0_restart_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209874L) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: Line_offset
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//    Indicates the offset in bytes from the start of one line to the next line.
//    This value should be equal to or larger than one line of image data and should be word-aligned
//    
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_LINE_OFFSET_DEFAULT (0x4000)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_LINE_OFFSET_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_LINE_OFFSET_OFFSET (0x878)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_LINE_OFFSET_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_line_offset_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x209878L, data);
}
static __inline uint32_t acamera_fpga_video_capture3_dma_writer_line_offset_read(uintptr_t base) {
    return system_hw_read_32(0x209878L);
}
// ------------------------------------------------------------------------------ //
// Register: frame write cancel
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0 = normal operation, 1= cancel current/future frame write(s), any unstarted AXI bursts cancelled and fifo flushed
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_FRAME_WRITE_CANCEL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_FRAME_WRITE_CANCEL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_FRAME_WRITE_CANCEL_OFFSET (0x87c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_FRAME_WRITE_CANCEL_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_frame_write_cancel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20987cL);
    system_hw_write_32(0x20987cL, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_frame_write_cancel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20987cL) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: axi_port_enable
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// enables axi, active high, 1=enables axi write transfers, 0= reset axi domain( via reset synchroniser)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_PORT_ENABLE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_PORT_ENABLE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_PORT_ENABLE_OFFSET (0x87c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_PORT_ENABLE_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_axi_port_enable_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20987cL);
    system_hw_write_32(0x20987cL, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_axi_port_enable_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20987cL) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: wbank curr
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// write bank currently active. valid values =0-4. updated at start of frame write
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBANK_CURR_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBANK_CURR_DATASIZE (3)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBANK_CURR_OFFSET (0x880)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBANK_CURR_MASK (0x7)

// args: data (3-bit)
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_wbank_curr_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209880L) & 0x7) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: wbank last
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// write bank last active. valid values = 0-4. updated at start of frame write
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBANK_LAST_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBANK_LAST_DATASIZE (3)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBANK_LAST_OFFSET (0x880)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBANK_LAST_MASK (0x38)

// args: data (3-bit)
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_wbank_last_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209880L) & 0x38) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: wbank active
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1 = wbank_curr is being written to. Goes high at start of writes, low at last write transfer/completion on axi. 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBANK_ACTIVE_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBANK_ACTIVE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBANK_ACTIVE_OFFSET (0x884)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBANK_ACTIVE_MASK (0x1)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_wbank_active_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209884L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: wbank start
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1 = High pulse at start of frame write to bank. 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBANK_START_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBANK_START_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBANK_START_OFFSET (0x884)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBANK_START_MASK (0x2)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_wbank_start_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209884L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: wbank stop
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1 = High pulse at end of frame write to bank. 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBANK_STOP_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBANK_STOP_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBANK_STOP_OFFSET (0x884)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBANK_STOP_MASK (0x4)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_wbank_stop_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209884L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: wbase curr
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// currently active bank base addr - in bytes. updated at start of frame write
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBASE_CURR_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBASE_CURR_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBASE_CURR_OFFSET (0x888)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBASE_CURR_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture3_dma_writer_wbase_curr_read(uintptr_t base) {
    return system_hw_read_32(0x209888L);
}
// ------------------------------------------------------------------------------ //
// Register: wbase last
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// last active bank base addr - in bytes. Updated at start of frame write
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBASE_LAST_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBASE_LAST_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBASE_LAST_OFFSET (0x88c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WBASE_LAST_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture3_dma_writer_wbase_last_read(uintptr_t base) {
    return system_hw_read_32(0x20988cL);
}
// ------------------------------------------------------------------------------ //
// Register: frame icount
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// count of incomming frames (starts) to vdma_writer on video input, non resetable, rolls over, updates at pixel 1 of new frame on video in
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_FRAME_ICOUNT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_FRAME_ICOUNT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_FRAME_ICOUNT_OFFSET (0x890)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_FRAME_ICOUNT_MASK (0xffff)

// args: data (16-bit)
static __inline uint16_t acamera_fpga_video_capture3_dma_writer_frame_icount_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209890L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: frame wcount
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// count of outgoing frame writes (starts) from vdma_writer sent to AXI output, non resetable, rolls over, updates at pixel 1 of new frame on video in
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_FRAME_WCOUNT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_FRAME_WCOUNT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_FRAME_WCOUNT_OFFSET (0x894)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_FRAME_WCOUNT_MASK (0xffff)

// args: data (16-bit)
static __inline uint16_t acamera_fpga_video_capture3_dma_writer_frame_wcount_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209894L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: clear alarms
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0>1 transition(synchronous detection) causes local axi/video alarm clear
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_CLEAR_ALARMS_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_CLEAR_ALARMS_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_CLEAR_ALARMS_OFFSET (0x898)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_CLEAR_ALARMS_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_clear_alarms_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209898L);
    system_hw_write_32(0x209898L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_clear_alarms_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209898L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: max_burst_length_is_8
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1= Reduce default AXI max_burst_length from 16 to 8, 0= Dont reduce
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_MAX_BURST_LENGTH_IS_8_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_MAX_BURST_LENGTH_IS_8_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_MAX_BURST_LENGTH_IS_8_OFFSET (0x898)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_MAX_BURST_LENGTH_IS_8_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_max_burst_length_is_8_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209898L);
    system_hw_write_32(0x209898L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_max_burst_length_is_8_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209898L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: max_burst_length_is_4
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1= Reduce default AXI max_burst_length from 16 to 4, 0= Dont reduce( has priority overmax_burst_length_is_8!)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_MAX_BURST_LENGTH_IS_4_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_MAX_BURST_LENGTH_IS_4_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_MAX_BURST_LENGTH_IS_4_OFFSET (0x898)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_MAX_BURST_LENGTH_IS_4_MASK (0x4)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_max_burst_length_is_4_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209898L);
    system_hw_write_32(0x209898L, (((uint32_t) (data & 0x1)) << 2) | (curr & 0xfffffffb));
}
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_max_burst_length_is_4_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209898L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: write timeout disable
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//    At end of frame an optional timeout is applied to wait for AXI writes to completed/accepted befotre caneclling and flushing.
//    0= Timeout Enabled, timeout count can decrement.
//    1 = Disable timeout, timeout count can't decrement.
//    
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WRITE_TIMEOUT_DISABLE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WRITE_TIMEOUT_DISABLE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WRITE_TIMEOUT_DISABLE_OFFSET (0x898)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WRITE_TIMEOUT_DISABLE_MASK (0x8)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_write_timeout_disable_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209898L);
    system_hw_write_32(0x209898L, (((uint32_t) (data & 0x1)) << 3) | (curr & 0xfffffff7));
}
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_write_timeout_disable_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209898L) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: awmaxwait_limit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// awvalid maxwait limit(cycles) to raise axi_fail_awmaxwait alarm . zero disables alarm raise.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AWMAXWAIT_LIMIT_DEFAULT (0x00)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AWMAXWAIT_LIMIT_DATASIZE (8)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AWMAXWAIT_LIMIT_OFFSET (0x89c)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AWMAXWAIT_LIMIT_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_awmaxwait_limit_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x20989cL);
    system_hw_write_32(0x20989cL, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_awmaxwait_limit_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x20989cL) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: wmaxwait_limit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// wvalid maxwait limit(cycles) to raise axi_fail_wmaxwait alarm . zero disables alarm raise
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WMAXWAIT_LIMIT_DEFAULT (0x00)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WMAXWAIT_LIMIT_DATASIZE (8)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WMAXWAIT_LIMIT_OFFSET (0x8a0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WMAXWAIT_LIMIT_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_wmaxwait_limit_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098a0L);
    system_hw_write_32(0x2098a0L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_wmaxwait_limit_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098a0L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: wxact_ostand_limit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// number oustsanding write transactions(bursts)(responses..1 per burst) limit to raise axi_fail_wxact_ostand. zero disables alarm raise
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WXACT_OSTAND_LIMIT_DEFAULT (0x00)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WXACT_OSTAND_LIMIT_DATASIZE (8)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WXACT_OSTAND_LIMIT_OFFSET (0x8a4)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_WXACT_OSTAND_LIMIT_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_wxact_ostand_limit_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098a4L);
    system_hw_write_32(0x2098a4L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_wxact_ostand_limit_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098a4L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: axi_fail_bresp
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high to indicate bad  bresp captured 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_FAIL_BRESP_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_FAIL_BRESP_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_FAIL_BRESP_OFFSET (0x8a8)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_FAIL_BRESP_MASK (0x1)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_axi_fail_bresp_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098a8L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: axi_fail_awmaxwait
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high when awmaxwait_limit reached 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_FAIL_AWMAXWAIT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_FAIL_AWMAXWAIT_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_FAIL_AWMAXWAIT_OFFSET (0x8a8)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_FAIL_AWMAXWAIT_MASK (0x2)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_axi_fail_awmaxwait_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098a8L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: axi_fail_wmaxwait
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high when wmaxwait_limit reached 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_FAIL_WMAXWAIT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_FAIL_WMAXWAIT_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_FAIL_WMAXWAIT_OFFSET (0x8a8)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_FAIL_WMAXWAIT_MASK (0x4)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_axi_fail_wmaxwait_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098a8L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: axi_fail_wxact_ostand
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high when wxact_ostand_limit reached 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_FAIL_WXACT_OSTAND_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_FAIL_WXACT_OSTAND_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_FAIL_WXACT_OSTAND_OFFSET (0x8a8)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_FAIL_WXACT_OSTAND_MASK (0x8)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_axi_fail_wxact_ostand_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098a8L) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: vi_fail_active_width
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high to indicate mismatched active_width detected 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_VI_FAIL_ACTIVE_WIDTH_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_VI_FAIL_ACTIVE_WIDTH_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_VI_FAIL_ACTIVE_WIDTH_OFFSET (0x8a8)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_VI_FAIL_ACTIVE_WIDTH_MASK (0x10)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_vi_fail_active_width_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098a8L) & 0x10) >> 4);
}
// ------------------------------------------------------------------------------ //
// Register: vi_fail_active_height
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high to indicate mismatched active_height detected ( also raised on missing field!) 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_VI_FAIL_ACTIVE_HEIGHT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_VI_FAIL_ACTIVE_HEIGHT_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_VI_FAIL_ACTIVE_HEIGHT_OFFSET (0x8a8)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_VI_FAIL_ACTIVE_HEIGHT_MASK (0x20)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_vi_fail_active_height_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098a8L) & 0x20) >> 5);
}
// ------------------------------------------------------------------------------ //
// Register: vi_fail_interline_blanks
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high to indicate interline blanking below min 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_VI_FAIL_INTERLINE_BLANKS_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_VI_FAIL_INTERLINE_BLANKS_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_VI_FAIL_INTERLINE_BLANKS_OFFSET (0x8a8)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_VI_FAIL_INTERLINE_BLANKS_MASK (0x40)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_vi_fail_interline_blanks_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098a8L) & 0x40) >> 6);
}
// ------------------------------------------------------------------------------ //
// Register: vi_fail_interframe_blanks
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high to indicate interframe blanking below min 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_VI_FAIL_INTERFRAME_BLANKS_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_VI_FAIL_INTERFRAME_BLANKS_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_VI_FAIL_INTERFRAME_BLANKS_OFFSET (0x8a8)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_VI_FAIL_INTERFRAME_BLANKS_MASK (0x80)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_vi_fail_interframe_blanks_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098a8L) & 0x80) >> 7);
}
// ------------------------------------------------------------------------------ //
// Register: video_alarm
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  active high, problem found on video port(s) ( active width/height or interline/frame blanks failure) 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_VIDEO_ALARM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_VIDEO_ALARM_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_VIDEO_ALARM_OFFSET (0x8ac)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_VIDEO_ALARM_MASK (0x1)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_video_alarm_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098acL) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: axi_alarm
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  active high, problem found on axi port(s)( bresp or awmaxwait or wmaxwait or wxact_ostand failure ) 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_ALARM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_ALARM_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_ALARM_OFFSET (0x8ac)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_AXI_ALARM_MASK (0x2)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture3_dma_writer_axi_alarm_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098acL) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: blk_config
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// block configuration (reserved)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BLK_CONFIG_DEFAULT (0x0000)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BLK_CONFIG_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BLK_CONFIG_OFFSET (0x8b0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BLK_CONFIG_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture3_dma_writer_blk_config_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x2098b0L, data);
}
static __inline uint32_t acamera_fpga_video_capture3_dma_writer_blk_config_read(uintptr_t base) {
    return system_hw_read_32(0x2098b0L);
}
// ------------------------------------------------------------------------------ //
// Register: blk_status
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// block status output (reserved)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BLK_STATUS_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BLK_STATUS_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BLK_STATUS_OFFSET (0x8b4)
#define ACAMERA_FPGA_VIDEO_CAPTURE3_DMA_WRITER_BLK_STATUS_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture3_dma_writer_blk_status_read(uintptr_t base) {
    return system_hw_read_32(0x2098b4L);
}
// ------------------------------------------------------------------------------ //
// Group: Video-Capture-4 Input port
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Controls video input port.  
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: preset
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//        Allows selection of various input port presets for standard sensor inputs.  See ISP Guide for details of available presets.
//        0-14: Frequently used presets.  If using one of available presets, remaining bits in registers 0x100 and 0x104 are not used.
//        15:   Input port configured according to registers in 0x100 and 0x104.  Consult Apical support for special input port requirements.
//      
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_PRESET_DEFAULT (2)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_PRESET_DATASIZE (4)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_PRESET_OFFSET (0x8b8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_PRESET_MASK (0xf)

// args: data (4-bit)
static __inline void acamera_fpga_video_capture4_input_port_preset_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098b8L);
    system_hw_write_32(0x2098b8L, (((uint32_t) (data & 0xf)) << 0) | (curr & 0xfffffff0));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_preset_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098b8L) & 0xf) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: vs_use field
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VS_USE_FIELD_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VS_USE_FIELD_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VS_USE_FIELD_OFFSET (0x8b8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VS_USE_FIELD_MASK (0x100)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VS_USE_FIELD_USE_VSYNC_I_PORT_FOR_VERTICAL_SYNC (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VS_USE_FIELD_USE_FIELD_I_PORT_FOR_VERTICAL_SYNC (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_vs_use_field_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098b8L);
    system_hw_write_32(0x2098b8L, (((uint32_t) (data & 0x1)) << 8) | (curr & 0xfffffeff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_vs_use_field_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098b8L) & 0x100) >> 8);
}
// ------------------------------------------------------------------------------ //
// Register: vs toggle
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VS_TOGGLE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VS_TOGGLE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VS_TOGGLE_OFFSET (0x8b8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VS_TOGGLE_MASK (0x200)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VS_TOGGLE_VSYNC_IS_PULSETYPE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VS_TOGGLE_VSYNC_IS_TOGGLETYPE_FIELD_SIGNAL (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_vs_toggle_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098b8L);
    system_hw_write_32(0x2098b8L, (((uint32_t) (data & 0x1)) << 9) | (curr & 0xfffffdff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_vs_toggle_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098b8L) & 0x200) >> 9);
}
// ------------------------------------------------------------------------------ //
// Register: vs polarity
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VS_POLARITY_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VS_POLARITY_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VS_POLARITY_OFFSET (0x8b8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VS_POLARITY_MASK (0x400)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VS_POLARITY_HORIZONTAL_COUNTER_RESET_ON_RISING_EDGE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VS_POLARITY_HORIZONTAL_COUNTER_RESET_ON_FALLING_EDGE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_vs_polarity_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098b8L);
    system_hw_write_32(0x2098b8L, (((uint32_t) (data & 0x1)) << 10) | (curr & 0xfffffbff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_vs_polarity_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098b8L) & 0x400) >> 10);
}
// ------------------------------------------------------------------------------ //
// Register: vs_polarity acl
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VS_POLARITY_ACL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VS_POLARITY_ACL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VS_POLARITY_ACL_OFFSET (0x8b8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VS_POLARITY_ACL_MASK (0x800)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VS_POLARITY_ACL_DONT_INVERT_POLARITY_FOR_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VS_POLARITY_ACL_INVERT_POLARITY_FOR_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_vs_polarity_acl_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098b8L);
    system_hw_write_32(0x2098b8L, (((uint32_t) (data & 0x1)) << 11) | (curr & 0xfffff7ff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_vs_polarity_acl_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098b8L) & 0x800) >> 11);
}
// ------------------------------------------------------------------------------ //
// Register: hs_use acl
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_USE_ACL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_USE_ACL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_USE_ACL_OFFSET (0x8b8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_USE_ACL_MASK (0x1000)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_USE_ACL_USE_HSYNC_I_PORT_FOR_ACTIVELINE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_USE_ACL_USE_ACL_I_PORT_FOR_ACTIVELINE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_hs_use_acl_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098b8L);
    system_hw_write_32(0x2098b8L, (((uint32_t) (data & 0x1)) << 12) | (curr & 0xffffefff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_hs_use_acl_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098b8L) & 0x1000) >> 12);
}
// ------------------------------------------------------------------------------ //
// Register: vc_c select
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VC_C_SELECT_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VC_C_SELECT_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VC_C_SELECT_OFFSET (0x8b8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VC_C_SELECT_MASK (0x4000)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VC_C_SELECT_VERTICAL_COUNTER_COUNTS_ON_HS (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VC_C_SELECT_VERTICAL_COUNTER_COUNTS_ON_HORIZONTAL_COUNTER_OVERFLOW_OR_RESET (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_vc_c_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098b8L);
    system_hw_write_32(0x2098b8L, (((uint32_t) (data & 0x1)) << 14) | (curr & 0xffffbfff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_vc_c_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098b8L) & 0x4000) >> 14);
}
// ------------------------------------------------------------------------------ //
// Register: vc_r select
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VC_R_SELECT_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VC_R_SELECT_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VC_R_SELECT_OFFSET (0x8b8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VC_R_SELECT_MASK (0x8000)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VC_R_SELECT_VERTICAL_COUNTER_IS_RESET_ON_EDGE_OF_VS (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VC_R_SELECT_VERTICAL_COUNTER_IS_RESET_AFTER_TIMEOUT_ON_HS (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_vc_r_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098b8L);
    system_hw_write_32(0x2098b8L, (((uint32_t) (data & 0x1)) << 15) | (curr & 0xffff7fff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_vc_r_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098b8L) & 0x8000) >> 15);
}
// ------------------------------------------------------------------------------ //
// Register: hs_xor vs
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_XOR_VS_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_XOR_VS_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_XOR_VS_OFFSET (0x8b8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_XOR_VS_MASK (0x10000)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_XOR_VS_NORMAL_MODE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_XOR_VS_HVALID__HSYNC_XOR_VSYNC (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_hs_xor_vs_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098b8L);
    system_hw_write_32(0x2098b8L, (((uint32_t) (data & 0x1)) << 16) | (curr & 0xfffeffff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_hs_xor_vs_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098b8L) & 0x10000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: hs polarity
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_POLARITY_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_POLARITY_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_POLARITY_OFFSET (0x8b8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_POLARITY_MASK (0x20000)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_POLARITY_DONT_INVERT_POLARITY_OF_HS_FOR_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_POLARITY_INVERT_POLARITY_OF_HS_FOR_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_hs_polarity_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098b8L);
    system_hw_write_32(0x2098b8L, (((uint32_t) (data & 0x1)) << 17) | (curr & 0xfffdffff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_hs_polarity_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098b8L) & 0x20000) >> 17);
}
// ------------------------------------------------------------------------------ //
// Register: hs_polarity acl
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_POLARITY_ACL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_POLARITY_ACL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_POLARITY_ACL_OFFSET (0x8b8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_POLARITY_ACL_MASK (0x40000)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_POLARITY_ACL_DONT_INVERT_POLARITY_OF_HS_FOR_HS_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_POLARITY_ACL_INVERT_POLARITY_OF_HS_FOR_HS_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_hs_polarity_acl_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098b8L);
    system_hw_write_32(0x2098b8L, (((uint32_t) (data & 0x1)) << 18) | (curr & 0xfffbffff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_hs_polarity_acl_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098b8L) & 0x40000) >> 18);
}
// ------------------------------------------------------------------------------ //
// Register: hs_polarity hs
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_POLARITY_HS_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_POLARITY_HS_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_POLARITY_HS_OFFSET (0x8b8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_POLARITY_HS_MASK (0x80000)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_POLARITY_HS_HORIZONTAL_COUNTER_IS_RESET_ON_RISING_EDGE_OF_HS (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_POLARITY_HS_HORIZONTAL_COUNTER_IS_RESET_ON_VSYNC_EG_WHEN_HSYNC_IS_NOT_AVAILABLE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_hs_polarity_hs_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098b8L);
    system_hw_write_32(0x2098b8L, (((uint32_t) (data & 0x1)) << 19) | (curr & 0xfff7ffff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_hs_polarity_hs_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098b8L) & 0x80000) >> 19);
}
// ------------------------------------------------------------------------------ //
// Register: hs_polarity vc
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_POLARITY_VC_DEFAULT (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_POLARITY_VC_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_POLARITY_VC_OFFSET (0x8b8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_POLARITY_VC_MASK (0x100000)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_POLARITY_VC_VERTICAL_COUNTER_INCREMENTS_ON_RISING_EDGE_OF_HS (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HS_POLARITY_VC_VERTICAL_COUNTER_INCREMENTS_ON_FALLING_EDGE_OF_HS (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_hs_polarity_vc_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098b8L);
    system_hw_write_32(0x2098b8L, (((uint32_t) (data & 0x1)) << 20) | (curr & 0xffefffff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_hs_polarity_vc_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098b8L) & 0x100000) >> 20);
}
// ------------------------------------------------------------------------------ //
// Register: hc_r select
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_R_SELECT_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_R_SELECT_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_R_SELECT_OFFSET (0x8b8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_R_SELECT_MASK (0x800000)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_R_SELECT_VERTICAL_COUNTER_IS_RESET_ON_RISING_EDGE_OF_HS (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_R_SELECT_VERTICAL_COUNTER_IS_RESET_ON_RISING_EDGE_OF_VS (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_hc_r_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098b8L);
    system_hw_write_32(0x2098b8L, (((uint32_t) (data & 0x1)) << 23) | (curr & 0xff7fffff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_hc_r_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098b8L) & 0x800000) >> 23);
}
// ------------------------------------------------------------------------------ //
// Register: acl polarity
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACL_POLARITY_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACL_POLARITY_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACL_POLARITY_OFFSET (0x8b8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACL_POLARITY_MASK (0x1000000)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACL_POLARITY_DONT_INVERT_ACL_I_FOR_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACL_POLARITY_INVERT_ACL_I_FOR_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_acl_polarity_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098b8L);
    system_hw_write_32(0x2098b8L, (((uint32_t) (data & 0x1)) << 24) | (curr & 0xfeffffff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_acl_polarity_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098b8L) & 0x1000000) >> 24);
}
// ------------------------------------------------------------------------------ //
// Register: field polarity
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELD_POLARITY_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELD_POLARITY_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELD_POLARITY_OFFSET (0x8bc)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELD_POLARITY_MASK (0x1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELD_POLARITY_DONT_INVERT_FIELD_I_FOR_FIELD_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELD_POLARITY_INVERT_FIELD_I_FOR_FIELD_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_field_polarity_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098bcL);
    system_hw_write_32(0x2098bcL, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_field_polarity_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098bcL) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: field toggle
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELD_TOGGLE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELD_TOGGLE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELD_TOGGLE_OFFSET (0x8bc)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELD_TOGGLE_MASK (0x2)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELD_TOGGLE_FIELD_IS_PULSETYPE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELD_TOGGLE_FIELD_IS_TOGGLETYPE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_field_toggle_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098bcL);
    system_hw_write_32(0x2098bcL, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_field_toggle_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098bcL) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: aclg window0
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_WINDOW0_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_WINDOW0_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_WINDOW0_OFFSET (0x8bc)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_WINDOW0_MASK (0x100)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_WINDOW0_EXCLUDE_WINDOW0_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_WINDOW0_INCLUDE_WINDOW0_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_aclg_window0_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098bcL);
    system_hw_write_32(0x2098bcL, (((uint32_t) (data & 0x1)) << 8) | (curr & 0xfffffeff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_aclg_window0_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098bcL) & 0x100) >> 8);
}
// ------------------------------------------------------------------------------ //
// Register: aclg hsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_HSYNC_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_HSYNC_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_HSYNC_OFFSET (0x8bc)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_HSYNC_MASK (0x200)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_HSYNC_EXCLUDE_HSYNC_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_HSYNC_INCLUDE_HSYNC_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_aclg_hsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098bcL);
    system_hw_write_32(0x2098bcL, (((uint32_t) (data & 0x1)) << 9) | (curr & 0xfffffdff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_aclg_hsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098bcL) & 0x200) >> 9);
}
// ------------------------------------------------------------------------------ //
// Register: aclg window2
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_WINDOW2_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_WINDOW2_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_WINDOW2_OFFSET (0x8bc)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_WINDOW2_MASK (0x400)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_WINDOW2_EXCLUDE_WINDOW2_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_WINDOW2_INCLUDE_WINDOW2_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_aclg_window2_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098bcL);
    system_hw_write_32(0x2098bcL, (((uint32_t) (data & 0x1)) << 10) | (curr & 0xfffffbff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_aclg_window2_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098bcL) & 0x400) >> 10);
}
// ------------------------------------------------------------------------------ //
// Register: aclg acl
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_ACL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_ACL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_ACL_OFFSET (0x8bc)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_ACL_MASK (0x800)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_ACL_EXCLUDE_ACL_I_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_ACL_INCLUDE_ACL_I_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_aclg_acl_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098bcL);
    system_hw_write_32(0x2098bcL, (((uint32_t) (data & 0x1)) << 11) | (curr & 0xfffff7ff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_aclg_acl_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098bcL) & 0x800) >> 11);
}
// ------------------------------------------------------------------------------ //
// Register: aclg vsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_VSYNC_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_VSYNC_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_VSYNC_OFFSET (0x8bc)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_VSYNC_MASK (0x1000)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_VSYNC_EXCLUDE_VSYNC_SIGNAL_IN_ACL_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_ACLG_VSYNC_INCLUDE_VSYNC_SIGNAL_IN_ACL_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_aclg_vsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098bcL);
    system_hw_write_32(0x2098bcL, (((uint32_t) (data & 0x1)) << 12) | (curr & 0xffffefff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_aclg_vsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098bcL) & 0x1000) >> 12);
}
// ------------------------------------------------------------------------------ //
// Register: hsg window1
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HSG_WINDOW1_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HSG_WINDOW1_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HSG_WINDOW1_OFFSET (0x8bc)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HSG_WINDOW1_MASK (0x10000)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HSG_WINDOW1_EXCLUDE_WINDOW1_SIGNAL_IN_HS_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HSG_WINDOW1_INCLUDE_WINDOW1_SIGNAL_IN_HS_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_hsg_window1_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098bcL);
    system_hw_write_32(0x2098bcL, (((uint32_t) (data & 0x1)) << 16) | (curr & 0xfffeffff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_hsg_window1_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098bcL) & 0x10000) >> 16);
}
// ------------------------------------------------------------------------------ //
// Register: hsg hsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HSG_HSYNC_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HSG_HSYNC_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HSG_HSYNC_OFFSET (0x8bc)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HSG_HSYNC_MASK (0x20000)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HSG_HSYNC_EXCLUDE_HSYNC_SIGNAL_IN_HS_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HSG_HSYNC_INCLUDE_HSYNC_SIGNAL_IN_HS_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_hsg_hsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098bcL);
    system_hw_write_32(0x2098bcL, (((uint32_t) (data & 0x1)) << 17) | (curr & 0xfffdffff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_hsg_hsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098bcL) & 0x20000) >> 17);
}
// ------------------------------------------------------------------------------ //
// Register: hsg vsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HSG_VSYNC_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HSG_VSYNC_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HSG_VSYNC_OFFSET (0x8bc)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HSG_VSYNC_MASK (0x40000)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HSG_VSYNC_EXCLUDE_VSYNC_SIGNAL_IN_HS_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HSG_VSYNC_INCLUDE_VSYNC_SIGNAL_IN_HS_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_hsg_vsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098bcL);
    system_hw_write_32(0x2098bcL, (((uint32_t) (data & 0x1)) << 18) | (curr & 0xfffbffff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_hsg_vsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098bcL) & 0x40000) >> 18);
}
// ------------------------------------------------------------------------------ //
// Register: hsg window2
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HSG_WINDOW2_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HSG_WINDOW2_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HSG_WINDOW2_OFFSET (0x8bc)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HSG_WINDOW2_MASK (0x80000)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HSG_WINDOW2_EXCLUDE_WINDOW2_SIGNAL_IN_HS_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HSG_WINDOW2_INCLUDE_WINDOW2_SIGNAL_IN_HS_GATE_MASK_OUT_SPURIOUS_HS_DURING_BLANK (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_hsg_window2_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098bcL);
    system_hw_write_32(0x2098bcL, (((uint32_t) (data & 0x1)) << 19) | (curr & 0xfff7ffff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_hsg_window2_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098bcL) & 0x80000) >> 19);
}
// ------------------------------------------------------------------------------ //
// Register: fieldg vsync
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELDG_VSYNC_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELDG_VSYNC_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELDG_VSYNC_OFFSET (0x8bc)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELDG_VSYNC_MASK (0x1000000)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELDG_VSYNC_EXCLUDE_VSYNC_SIGNAL_IN_FIELD_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELDG_VSYNC_INCLUDE_VSYNC_SIGNAL_IN_FIELD_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_fieldg_vsync_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098bcL);
    system_hw_write_32(0x2098bcL, (((uint32_t) (data & 0x1)) << 24) | (curr & 0xfeffffff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_fieldg_vsync_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098bcL) & 0x1000000) >> 24);
}
// ------------------------------------------------------------------------------ //
// Register: fieldg window2
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELDG_WINDOW2_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELDG_WINDOW2_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELDG_WINDOW2_OFFSET (0x8bc)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELDG_WINDOW2_MASK (0x2000000)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELDG_WINDOW2_EXCLUDE_WINDOW2_SIGNAL_IN_FIELD_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELDG_WINDOW2_INCLUDE_WINDOW2_SIGNAL_IN_FIELD_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_fieldg_window2_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098bcL);
    system_hw_write_32(0x2098bcL, (((uint32_t) (data & 0x1)) << 25) | (curr & 0xfdffffff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_fieldg_window2_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098bcL) & 0x2000000) >> 25);
}
// ------------------------------------------------------------------------------ //
// Register: fieldg field
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELDG_FIELD_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELDG_FIELD_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELDG_FIELD_OFFSET (0x8bc)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELDG_FIELD_MASK (0x4000000)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELDG_FIELD_EXCLUDE_FIELD_I_SIGNAL_IN_FIELD_GATE (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELDG_FIELD_INCLUDE_FIELD_I_SIGNAL_IN_FIELD_GATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_fieldg_field_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098bcL);
    system_hw_write_32(0x2098bcL, (((uint32_t) (data & 0x1)) << 26) | (curr & 0xfbffffff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_fieldg_field_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098bcL) & 0x4000000) >> 26);
}
// ------------------------------------------------------------------------------ //
// Register: field mode
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELD_MODE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELD_MODE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELD_MODE_OFFSET (0x8bc)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELD_MODE_MASK (0x8000000)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELD_MODE_PULSE_FIELD (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FIELD_MODE_TOGGLE_FIELD (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_field_mode_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098bcL);
    system_hw_write_32(0x2098bcL, (((uint32_t) (data & 0x1)) << 27) | (curr & 0xf7ffffff));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_field_mode_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098bcL) & 0x8000000) >> 27);
}
// ------------------------------------------------------------------------------ //
// Register: hc limit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// horizontal counter limit value (counts: 0,1,...hc_limit-1,hc_limit,0,1,...)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_LIMIT_DEFAULT (0xFFFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_LIMIT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_LIMIT_OFFSET (0x8c0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_LIMIT_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture4_input_port_hc_limit_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2098c0L);
    system_hw_write_32(0x2098c0L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture4_input_port_hc_limit_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2098c0L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: hc start0
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window0 start for ACL gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_START0_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_START0_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_START0_OFFSET (0x8c4)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_START0_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture4_input_port_hc_start0_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2098c4L);
    system_hw_write_32(0x2098c4L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture4_input_port_hc_start0_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2098c4L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: hc size0
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window0 size for ACL gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_SIZE0_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_SIZE0_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_SIZE0_OFFSET (0x8c8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_SIZE0_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture4_input_port_hc_size0_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2098c8L);
    system_hw_write_32(0x2098c8L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture4_input_port_hc_size0_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2098c8L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: hc start1
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window1 start for HS gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_START1_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_START1_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_START1_OFFSET (0x8cc)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_START1_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture4_input_port_hc_start1_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2098ccL);
    system_hw_write_32(0x2098ccL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture4_input_port_hc_start1_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2098ccL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: hc size1
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window1 size for HS gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_SIZE1_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_SIZE1_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_SIZE1_OFFSET (0x8d0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_HC_SIZE1_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture4_input_port_hc_size1_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2098d0L);
    system_hw_write_32(0x2098d0L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture4_input_port_hc_size1_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2098d0L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: vc limit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// vertical counter limit value (counts: 0,1,...vc_limit-1,vc_limit,0,1,...)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VC_LIMIT_DEFAULT (0xFFFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VC_LIMIT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VC_LIMIT_OFFSET (0x8d4)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VC_LIMIT_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture4_input_port_vc_limit_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2098d4L);
    system_hw_write_32(0x2098d4L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture4_input_port_vc_limit_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2098d4L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: vc start
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window2 start for ACL gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VC_START_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VC_START_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VC_START_OFFSET (0x8d8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VC_START_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture4_input_port_vc_start_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2098d8L);
    system_hw_write_32(0x2098d8L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture4_input_port_vc_start_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2098d8L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: vc size
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// window2 size for ACL gate.  See ISP guide for further details.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VC_SIZE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VC_SIZE_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VC_SIZE_OFFSET (0x8dc)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_VC_SIZE_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture4_input_port_vc_size_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x2098dcL);
    system_hw_write_32(0x2098dcL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture4_input_port_vc_size_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2098dcL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: frame width
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// detected frame width.  Read only value.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FRAME_WIDTH_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FRAME_WIDTH_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FRAME_WIDTH_OFFSET (0x8e0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FRAME_WIDTH_MASK (0xffff)

// args: data (16-bit)
static __inline uint16_t acamera_fpga_video_capture4_input_port_frame_width_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2098e0L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: frame height
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// detected frame height.  Read only value.  
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FRAME_HEIGHT_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FRAME_HEIGHT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FRAME_HEIGHT_OFFSET (0x8e4)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FRAME_HEIGHT_MASK (0xffff)

// args: data (16-bit)
static __inline uint16_t acamera_fpga_video_capture4_input_port_frame_height_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2098e4L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: mode request
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Used to stop and start input port.  See ISP guide for further details. 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_MODE_REQUEST_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_MODE_REQUEST_DATASIZE (3)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_MODE_REQUEST_OFFSET (0x8e8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_MODE_REQUEST_MASK (0x7)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_MODE_REQUEST_SAFE_STOP (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_MODE_REQUEST_SAFE_START (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_MODE_REQUEST_URGENT_STOP (2)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_MODE_REQUEST_URGENT_START (3)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_MODE_REQUEST_RESERVED4 (4)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_MODE_REQUEST_SAFER_START (5)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_MODE_REQUEST_RESERVED6 (6)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_MODE_REQUEST_RESERVED7 (7)

// args: data (3-bit)
static __inline void acamera_fpga_video_capture4_input_port_mode_request_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098e8L);
    system_hw_write_32(0x2098e8L, (((uint32_t) (data & 0x7)) << 0) | (curr & 0xfffffff8));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_mode_request_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098e8L) & 0x7) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: freeze config
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Used to freeze input port configuration.  Used when multiple register writes are required to change input port configuration. 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FREEZE_CONFIG_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FREEZE_CONFIG_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FREEZE_CONFIG_OFFSET (0x8e8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FREEZE_CONFIG_MASK (0x80)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FREEZE_CONFIG_NORMAL_OPERATION (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_FREEZE_CONFIG_HOLD_PREVIOUS_INPUT_PORT_CONFIG_STATE (1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_input_port_freeze_config_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098e8L);
    system_hw_write_32(0x2098e8L, (((uint32_t) (data & 0x1)) << 7) | (curr & 0xffffff7f));
}
static __inline uint8_t acamera_fpga_video_capture4_input_port_freeze_config_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098e8L) & 0x80) >> 7);
}
// ------------------------------------------------------------------------------ //
// Register: mode status
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//          Used to monitor input port status:
//          bit 0: 1=running, 0=stopped, bits 1,2-reserved
//        
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_MODE_STATUS_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_MODE_STATUS_DATASIZE (3)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_MODE_STATUS_OFFSET (0x8ec)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_INPUT_PORT_MODE_STATUS_MASK (0x7)

// args: data (3-bit)
static __inline uint8_t acamera_fpga_video_capture4_input_port_mode_status_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098ecL) & 0x7) >> 0);
}
// ------------------------------------------------------------------------------ //
// Group: Video-Capture-4 Frame Stats
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: stats reset
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_STATS_RESET_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_STATS_RESET_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_STATS_RESET_OFFSET (0x8f0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_STATS_RESET_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_frame_stats_stats_reset_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098f0L);
    system_hw_write_32(0x2098f0L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture4_frame_stats_stats_reset_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098f0L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: stats hold
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_STATS_HOLD_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_STATS_HOLD_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_STATS_HOLD_OFFSET (0x8f4)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_STATS_HOLD_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_frame_stats_stats_hold_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2098f4L);
    system_hw_write_32(0x2098f4L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture4_frame_stats_stats_hold_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2098f4L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: active width min
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_WIDTH_MIN_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_WIDTH_MIN_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_WIDTH_MIN_OFFSET (0x900)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_WIDTH_MIN_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture4_frame_stats_active_width_min_read(uintptr_t base) {
    return system_hw_read_32(0x209900L);
}
// ------------------------------------------------------------------------------ //
// Register: active width max
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_WIDTH_MAX_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_WIDTH_MAX_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_WIDTH_MAX_OFFSET (0x904)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_WIDTH_MAX_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture4_frame_stats_active_width_max_read(uintptr_t base) {
    return system_hw_read_32(0x209904L);
}
// ------------------------------------------------------------------------------ //
// Register: active width sum
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_WIDTH_SUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_WIDTH_SUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_WIDTH_SUM_OFFSET (0x908)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_WIDTH_SUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture4_frame_stats_active_width_sum_read(uintptr_t base) {
    return system_hw_read_32(0x209908L);
}
// ------------------------------------------------------------------------------ //
// Register: active width num
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_WIDTH_NUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_WIDTH_NUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_WIDTH_NUM_OFFSET (0x90c)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_WIDTH_NUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture4_frame_stats_active_width_num_read(uintptr_t base) {
    return system_hw_read_32(0x20990cL);
}
// ------------------------------------------------------------------------------ //
// Register: active height min
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_HEIGHT_MIN_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_HEIGHT_MIN_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_HEIGHT_MIN_OFFSET (0x910)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_HEIGHT_MIN_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture4_frame_stats_active_height_min_read(uintptr_t base) {
    return system_hw_read_32(0x209910L);
}
// ------------------------------------------------------------------------------ //
// Register: active height max
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_HEIGHT_MAX_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_HEIGHT_MAX_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_HEIGHT_MAX_OFFSET (0x914)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_HEIGHT_MAX_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture4_frame_stats_active_height_max_read(uintptr_t base) {
    return system_hw_read_32(0x209914L);
}
// ------------------------------------------------------------------------------ //
// Register: active height sum
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_HEIGHT_SUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_HEIGHT_SUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_HEIGHT_SUM_OFFSET (0x918)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_HEIGHT_SUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture4_frame_stats_active_height_sum_read(uintptr_t base) {
    return system_hw_read_32(0x209918L);
}
// ------------------------------------------------------------------------------ //
// Register: active height num
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_HEIGHT_NUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_HEIGHT_NUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_HEIGHT_NUM_OFFSET (0x91c)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_ACTIVE_HEIGHT_NUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture4_frame_stats_active_height_num_read(uintptr_t base) {
    return system_hw_read_32(0x20991cL);
}
// ------------------------------------------------------------------------------ //
// Register: hblank min
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_HBLANK_MIN_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_HBLANK_MIN_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_HBLANK_MIN_OFFSET (0x920)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_HBLANK_MIN_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture4_frame_stats_hblank_min_read(uintptr_t base) {
    return system_hw_read_32(0x209920L);
}
// ------------------------------------------------------------------------------ //
// Register: hblank max
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_HBLANK_MAX_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_HBLANK_MAX_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_HBLANK_MAX_OFFSET (0x924)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_HBLANK_MAX_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture4_frame_stats_hblank_max_read(uintptr_t base) {
    return system_hw_read_32(0x209924L);
}
// ------------------------------------------------------------------------------ //
// Register: hblank sum
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_HBLANK_SUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_HBLANK_SUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_HBLANK_SUM_OFFSET (0x928)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_HBLANK_SUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture4_frame_stats_hblank_sum_read(uintptr_t base) {
    return system_hw_read_32(0x209928L);
}
// ------------------------------------------------------------------------------ //
// Register: hblank num
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_HBLANK_NUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_HBLANK_NUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_HBLANK_NUM_OFFSET (0x92c)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_HBLANK_NUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture4_frame_stats_hblank_num_read(uintptr_t base) {
    return system_hw_read_32(0x20992cL);
}
// ------------------------------------------------------------------------------ //
// Register: vblank min
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_VBLANK_MIN_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_VBLANK_MIN_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_VBLANK_MIN_OFFSET (0x930)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_VBLANK_MIN_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture4_frame_stats_vblank_min_read(uintptr_t base) {
    return system_hw_read_32(0x209930L);
}
// ------------------------------------------------------------------------------ //
// Register: vblank max
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_VBLANK_MAX_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_VBLANK_MAX_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_VBLANK_MAX_OFFSET (0x934)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_VBLANK_MAX_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture4_frame_stats_vblank_max_read(uintptr_t base) {
    return system_hw_read_32(0x209934L);
}
// ------------------------------------------------------------------------------ //
// Register: vblank sum
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_VBLANK_SUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_VBLANK_SUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_VBLANK_SUM_OFFSET (0x938)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_VBLANK_SUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture4_frame_stats_vblank_sum_read(uintptr_t base) {
    return system_hw_read_32(0x209938L);
}
// ------------------------------------------------------------------------------ //
// Register: vblank num
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_VBLANK_NUM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_VBLANK_NUM_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_VBLANK_NUM_OFFSET (0x93c)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_FRAME_STATS_VBLANK_NUM_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture4_frame_stats_vblank_num_read(uintptr_t base) {
    return system_hw_read_32(0x20993cL);
}
// ------------------------------------------------------------------------------ //
// Group: Video-Capture-4 video test gen
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Video test generator controls.  See ISP Guide for further details
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: test_pattern_off on
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Test pattern off-on: 0=off, 1=on
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_TEST_PATTERN_OFF_ON_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_TEST_PATTERN_OFF_ON_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_TEST_PATTERN_OFF_ON_OFFSET (0x944)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_TEST_PATTERN_OFF_ON_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_video_test_gen_test_pattern_off_on_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209944L);
    system_hw_write_32(0x209944L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture4_video_test_gen_test_pattern_off_on_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209944L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: bayer_rgb_i sel
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Bayer or rgb select for input video: 0=bayer, 1=rgb
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_BAYER_RGB_I_SEL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_BAYER_RGB_I_SEL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_BAYER_RGB_I_SEL_OFFSET (0x944)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_BAYER_RGB_I_SEL_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_video_test_gen_bayer_rgb_i_sel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209944L);
    system_hw_write_32(0x209944L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_video_capture4_video_test_gen_bayer_rgb_i_sel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209944L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: bayer_rgb_o sel
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Bayer or rgb select for output video: 0=bayer, 1=rgb
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_BAYER_RGB_O_SEL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_BAYER_RGB_O_SEL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_BAYER_RGB_O_SEL_OFFSET (0x944)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_BAYER_RGB_O_SEL_MASK (0x4)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_video_test_gen_bayer_rgb_o_sel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209944L);
    system_hw_write_32(0x209944L, (((uint32_t) (data & 0x1)) << 2) | (curr & 0xfffffffb));
}
static __inline uint8_t acamera_fpga_video_capture4_video_test_gen_bayer_rgb_o_sel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209944L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: pattern type
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Pattern type select: 0=Flat field,1=Horizontal gradient,2=Vertical Gradient,3=Vertical Bars,4=Rectangle,5-255=Default white frame on black
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_PATTERN_TYPE_DEFAULT (0x03)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_PATTERN_TYPE_DATASIZE (8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_PATTERN_TYPE_OFFSET (0x948)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_PATTERN_TYPE_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_video_capture4_video_test_gen_pattern_type_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209948L);
    system_hw_write_32(0x209948L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_video_capture4_video_test_gen_pattern_type_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209948L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: r backgnd
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Red background  value 16bit, MSB aligned to used width 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_R_BACKGND_DEFAULT (0xFFFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_R_BACKGND_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_R_BACKGND_OFFSET (0x94c)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_R_BACKGND_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture4_video_test_gen_r_backgnd_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20994cL);
    system_hw_write_32(0x20994cL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture4_video_test_gen_r_backgnd_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20994cL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: g backgnd
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Green background value 16bit, MSB aligned to used width
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_G_BACKGND_DEFAULT (0xFFFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_G_BACKGND_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_G_BACKGND_OFFSET (0x950)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_G_BACKGND_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture4_video_test_gen_g_backgnd_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209950L);
    system_hw_write_32(0x209950L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture4_video_test_gen_g_backgnd_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209950L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: b backgnd
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Blue background value 16bit, MSB aligned to used width
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_B_BACKGND_DEFAULT (0xFFFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_B_BACKGND_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_B_BACKGND_OFFSET (0x954)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_B_BACKGND_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture4_video_test_gen_b_backgnd_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209954L);
    system_hw_write_32(0x209954L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture4_video_test_gen_b_backgnd_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209954L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: r foregnd
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Red foreground  value 16bit, MSB aligned to used width 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_R_FOREGND_DEFAULT (0x8FFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_R_FOREGND_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_R_FOREGND_OFFSET (0x958)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_R_FOREGND_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture4_video_test_gen_r_foregnd_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209958L);
    system_hw_write_32(0x209958L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture4_video_test_gen_r_foregnd_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209958L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: g foregnd
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Green foreground value 16bit, MSB aligned to used width
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_G_FOREGND_DEFAULT (0x8FFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_G_FOREGND_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_G_FOREGND_OFFSET (0x95c)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_G_FOREGND_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture4_video_test_gen_g_foregnd_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20995cL);
    system_hw_write_32(0x20995cL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture4_video_test_gen_g_foregnd_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20995cL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: b foregnd
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Blue foreground value 16bit, MSB aligned to used width
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_B_FOREGND_DEFAULT (0x8FFF)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_B_FOREGND_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_B_FOREGND_OFFSET (0x960)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_B_FOREGND_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture4_video_test_gen_b_foregnd_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209960L);
    system_hw_write_32(0x209960L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture4_video_test_gen_b_foregnd_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209960L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rgb gradient
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// RGB gradient increment per pixel (0-15)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_RGB_GRADIENT_DEFAULT (0x3CAA)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_RGB_GRADIENT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_RGB_GRADIENT_OFFSET (0x964)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_RGB_GRADIENT_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture4_video_test_gen_rgb_gradient_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209964L);
    system_hw_write_32(0x209964L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture4_video_test_gen_rgb_gradient_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209964L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rgb_gradient start
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// RGB gradient start value 16bit, MSB aligned to used width
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_RGB_GRADIENT_START_DEFAULT (0x0000)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_RGB_GRADIENT_START_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_RGB_GRADIENT_START_OFFSET (0x968)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_RGB_GRADIENT_START_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture4_video_test_gen_rgb_gradient_start_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209968L);
    system_hw_write_32(0x209968L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture4_video_test_gen_rgb_gradient_start_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209968L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rect top
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  Rectangle top line number 1-n
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_RECT_TOP_DEFAULT (0x0001)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_RECT_TOP_DATASIZE (14)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_RECT_TOP_OFFSET (0x96c)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_RECT_TOP_MASK (0x3fff)

// args: data (14-bit)
static __inline void acamera_fpga_video_capture4_video_test_gen_rect_top_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20996cL);
    system_hw_write_32(0x20996cL, (((uint32_t) (data & 0x3fff)) << 0) | (curr & 0xffffc000));
}
static __inline uint16_t acamera_fpga_video_capture4_video_test_gen_rect_top_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20996cL) & 0x3fff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rect bot
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  Rectangle bottom line number 1-n
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_RECT_BOT_DEFAULT (0x0100)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_RECT_BOT_DATASIZE (14)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_RECT_BOT_OFFSET (0x970)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_RECT_BOT_MASK (0x3fff)

// args: data (14-bit)
static __inline void acamera_fpga_video_capture4_video_test_gen_rect_bot_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209970L);
    system_hw_write_32(0x209970L, (((uint32_t) (data & 0x3fff)) << 0) | (curr & 0xffffc000));
}
static __inline uint16_t acamera_fpga_video_capture4_video_test_gen_rect_bot_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209970L) & 0x3fff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rect left
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  Rectangle left pixel number 1-n
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_RECT_LEFT_DEFAULT (0x0001)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_RECT_LEFT_DATASIZE (14)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_RECT_LEFT_OFFSET (0x974)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_RECT_LEFT_MASK (0x3fff)

// args: data (14-bit)
static __inline void acamera_fpga_video_capture4_video_test_gen_rect_left_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209974L);
    system_hw_write_32(0x209974L, (((uint32_t) (data & 0x3fff)) << 0) | (curr & 0xffffc000));
}
static __inline uint16_t acamera_fpga_video_capture4_video_test_gen_rect_left_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209974L) & 0x3fff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: rect right
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  Rectangle right pixel number 1-n
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_RECT_RIGHT_DEFAULT (0x0100)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_RECT_RIGHT_DATASIZE (14)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_RECT_RIGHT_OFFSET (0x97c)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_VIDEO_TEST_GEN_RECT_RIGHT_MASK (0x3fff)

// args: data (14-bit)
static __inline void acamera_fpga_video_capture4_video_test_gen_rect_right_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20997cL);
    system_hw_write_32(0x20997cL, (((uint32_t) (data & 0x3fff)) << 0) | (curr & 0xffffc000));
}
static __inline uint16_t acamera_fpga_video_capture4_video_test_gen_rect_right_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20997cL) & 0x3fff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Group: Video-Capture-4 DMA Writer
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Video-Capture DMA writer controls
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: Format
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Format
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_FORMAT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_FORMAT_DATASIZE (8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_FORMAT_OFFSET (0x980)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_FORMAT_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_format_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209980L);
    system_hw_write_32(0x209980L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_format_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209980L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Base mode
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Base DMA packing mode for RGB/RAW/YUV etc (see ISP guide)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BASE_MODE_DEFAULT (0x0D)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BASE_MODE_DATASIZE (4)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BASE_MODE_OFFSET (0x980)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BASE_MODE_MASK (0xf)

// args: data (4-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_base_mode_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209980L);
    system_hw_write_32(0x209980L, (((uint32_t) (data & 0xf)) << 0) | (curr & 0xfffffff0));
}
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_base_mode_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209980L) & 0xf) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Plane select
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Plane select for planar base modes.  Only used if planar outputs required.  Not used.  Should be set to 0
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_PLANE_SELECT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_PLANE_SELECT_DATASIZE (2)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_PLANE_SELECT_OFFSET (0x980)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_PLANE_SELECT_MASK (0xc0)

// args: data (2-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_plane_select_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209980L);
    system_hw_write_32(0x209980L, (((uint32_t) (data & 0x3)) << 6) | (curr & 0xffffff3f));
}
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_plane_select_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209980L) & 0xc0) >> 6);
}
// ------------------------------------------------------------------------------ //
// Register: single frame
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0 = All frames are written(after frame_write_on= 1), 1= only 1st frame written ( after frame_write_on =1)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_SINGLE_FRAME_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_SINGLE_FRAME_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_SINGLE_FRAME_OFFSET (0x984)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_SINGLE_FRAME_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_single_frame_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209984L);
    system_hw_write_32(0x209984L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_single_frame_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209984L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: frame write on
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//    0 = no frames written(when switched from 1, current frame completes writing before stopping),
//    1= write frame(s) (write single or continous frame(s) )
//    
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_FRAME_WRITE_ON_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_FRAME_WRITE_ON_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_FRAME_WRITE_ON_OFFSET (0x984)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_FRAME_WRITE_ON_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_frame_write_on_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209984L);
    system_hw_write_32(0x209984L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_frame_write_on_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209984L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: half irate
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0 = normal operation , 1= write half(alternate) of input frames( only valid for continuous mode)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_HALF_IRATE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_HALF_IRATE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_HALF_IRATE_OFFSET (0x984)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_HALF_IRATE_MASK (0x4)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_half_irate_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209984L);
    system_hw_write_32(0x209984L, (((uint32_t) (data & 0x1)) << 2) | (curr & 0xfffffffb));
}
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_half_irate_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209984L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: axi xact comp
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0 = dont wait for axi transaction completion at end of frame(just all transfers accepted). 1 = wait for all transactions completed
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_XACT_COMP_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_XACT_COMP_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_XACT_COMP_OFFSET (0x984)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_XACT_COMP_MASK (0x8)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_axi_xact_comp_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x209984L);
    system_hw_write_32(0x209984L, (((uint32_t) (data & 0x1)) << 3) | (curr & 0xfffffff7));
}
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_axi_xact_comp_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x209984L) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: active width
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Active video width in pixels 128-8000
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_ACTIVE_WIDTH_DEFAULT (0x780)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_ACTIVE_WIDTH_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_ACTIVE_WIDTH_OFFSET (0x988)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_ACTIVE_WIDTH_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_active_width_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x209988L);
    system_hw_write_32(0x209988L, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture4_dma_writer_active_width_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x209988L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: active height
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Active video height in lines 128-8000
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_ACTIVE_HEIGHT_DEFAULT (0x438)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_ACTIVE_HEIGHT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_ACTIVE_HEIGHT_OFFSET (0x98c)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_ACTIVE_HEIGHT_MASK (0xffff)

// args: data (16-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_active_height_write(uintptr_t base, uint16_t data) {
    uint32_t curr = system_hw_read_32(0x20998cL);
    system_hw_write_32(0x20998cL, (((uint32_t) (data & 0xffff)) << 0) | (curr & 0xffff0000));
}
static __inline uint16_t acamera_fpga_video_capture4_dma_writer_active_height_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x20998cL) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: bank0_base
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// bank 0 base address for frame buffer, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BANK0_BASE_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BANK0_BASE_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BANK0_BASE_OFFSET (0x990)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BANK0_BASE_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_bank0_base_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x209990L, data);
}
static __inline uint32_t acamera_fpga_video_capture4_dma_writer_bank0_base_read(uintptr_t base) {
    return system_hw_read_32(0x209990L);
}
// ------------------------------------------------------------------------------ //
// Register: bank1_base
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// bank 1 base address for frame buffer, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BANK1_BASE_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BANK1_BASE_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BANK1_BASE_OFFSET (0x994)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BANK1_BASE_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_bank1_base_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x209994L, data);
}
static __inline uint32_t acamera_fpga_video_capture4_dma_writer_bank1_base_read(uintptr_t base) {
    return system_hw_read_32(0x209994L);
}
// ------------------------------------------------------------------------------ //
// Register: bank2_base
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// bank 2 base address for frame buffer, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BANK2_BASE_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BANK2_BASE_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BANK2_BASE_OFFSET (0x998)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BANK2_BASE_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_bank2_base_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x209998L, data);
}
static __inline uint32_t acamera_fpga_video_capture4_dma_writer_bank2_base_read(uintptr_t base) {
    return system_hw_read_32(0x209998L);
}
// ------------------------------------------------------------------------------ //
// Register: bank3_base
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// bank 3 base address for frame buffer, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BANK3_BASE_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BANK3_BASE_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BANK3_BASE_OFFSET (0x99c)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BANK3_BASE_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_bank3_base_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x20999cL, data);
}
static __inline uint32_t acamera_fpga_video_capture4_dma_writer_bank3_base_read(uintptr_t base) {
    return system_hw_read_32(0x20999cL);
}
// ------------------------------------------------------------------------------ //
// Register: bank4_base
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// bank 4 base address for frame buffer, should be word-aligned
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BANK4_BASE_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BANK4_BASE_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BANK4_BASE_OFFSET (0x9a0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BANK4_BASE_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_bank4_base_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x2099a0L, data);
}
static __inline uint32_t acamera_fpga_video_capture4_dma_writer_bank4_base_read(uintptr_t base) {
    return system_hw_read_32(0x2099a0L);
}
// ------------------------------------------------------------------------------ //
// Register: max bank
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// highest bank*_base to use for frame writes before recycling to bank0_base, only 0 to 4 are valid
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_MAX_BANK_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_MAX_BANK_DATASIZE (3)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_MAX_BANK_OFFSET (0x9a4)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_MAX_BANK_MASK (0x7)

// args: data (3-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_max_bank_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2099a4L);
    system_hw_write_32(0x2099a4L, (((uint32_t) (data & 0x7)) << 0) | (curr & 0xfffffff8));
}
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_max_bank_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099a4L) & 0x7) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: bank0 restart
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0 = normal operation, 1= restart bank counter to bank0 for next frame write
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BANK0_RESTART_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BANK0_RESTART_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BANK0_RESTART_OFFSET (0x9a4)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BANK0_RESTART_MASK (0x8)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_bank0_restart_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2099a4L);
    system_hw_write_32(0x2099a4L, (((uint32_t) (data & 0x1)) << 3) | (curr & 0xfffffff7));
}
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_bank0_restart_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099a4L) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: Line_offset
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//    Indicates the offset in bytes from the start of one line to the next line.
//    This value should be equal to or larger than one line of image data and should be word-aligned
//    
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_LINE_OFFSET_DEFAULT (0x4000)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_LINE_OFFSET_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_LINE_OFFSET_OFFSET (0x9a8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_LINE_OFFSET_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_line_offset_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x2099a8L, data);
}
static __inline uint32_t acamera_fpga_video_capture4_dma_writer_line_offset_read(uintptr_t base) {
    return system_hw_read_32(0x2099a8L);
}
// ------------------------------------------------------------------------------ //
// Register: frame write cancel
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0 = normal operation, 1= cancel current/future frame write(s), any unstarted AXI bursts cancelled and fifo flushed
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_FRAME_WRITE_CANCEL_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_FRAME_WRITE_CANCEL_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_FRAME_WRITE_CANCEL_OFFSET (0x9ac)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_FRAME_WRITE_CANCEL_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_frame_write_cancel_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2099acL);
    system_hw_write_32(0x2099acL, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_frame_write_cancel_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099acL) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: axi_port_enable
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// enables axi, active high, 1=enables axi write transfers, 0= reset axi domain( via reset synchroniser)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_PORT_ENABLE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_PORT_ENABLE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_PORT_ENABLE_OFFSET (0x9ac)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_PORT_ENABLE_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_axi_port_enable_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2099acL);
    system_hw_write_32(0x2099acL, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_axi_port_enable_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099acL) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: wbank curr
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// write bank currently active. valid values =0-4. updated at start of frame write
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBANK_CURR_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBANK_CURR_DATASIZE (3)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBANK_CURR_OFFSET (0x9b0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBANK_CURR_MASK (0x7)

// args: data (3-bit)
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_wbank_curr_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099b0L) & 0x7) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: wbank last
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// write bank last active. valid values = 0-4. updated at start of frame write
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBANK_LAST_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBANK_LAST_DATASIZE (3)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBANK_LAST_OFFSET (0x9b0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBANK_LAST_MASK (0x38)

// args: data (3-bit)
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_wbank_last_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099b0L) & 0x38) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: wbank active
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1 = wbank_curr is being written to. Goes high at start of writes, low at last write transfer/completion on axi. 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBANK_ACTIVE_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBANK_ACTIVE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBANK_ACTIVE_OFFSET (0x9b4)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBANK_ACTIVE_MASK (0x1)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_wbank_active_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099b4L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: wbank start
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1 = High pulse at start of frame write to bank. 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBANK_START_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBANK_START_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBANK_START_OFFSET (0x9b4)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBANK_START_MASK (0x2)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_wbank_start_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099b4L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: wbank stop
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1 = High pulse at end of frame write to bank. 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBANK_STOP_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBANK_STOP_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBANK_STOP_OFFSET (0x9b4)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBANK_STOP_MASK (0x4)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_wbank_stop_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099b4L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: wbase curr
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// currently active bank base addr - in bytes. updated at start of frame write
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBASE_CURR_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBASE_CURR_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBASE_CURR_OFFSET (0x9b8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBASE_CURR_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture4_dma_writer_wbase_curr_read(uintptr_t base) {
    return system_hw_read_32(0x2099b8L);
}
// ------------------------------------------------------------------------------ //
// Register: wbase last
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// last active bank base addr - in bytes. Updated at start of frame write
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBASE_LAST_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBASE_LAST_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBASE_LAST_OFFSET (0x9bc)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WBASE_LAST_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture4_dma_writer_wbase_last_read(uintptr_t base) {
    return system_hw_read_32(0x2099bcL);
}
// ------------------------------------------------------------------------------ //
// Register: frame icount
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// count of incomming frames (starts) to vdma_writer on video input, non resetable, rolls over, updates at pixel 1 of new frame on video in
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_FRAME_ICOUNT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_FRAME_ICOUNT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_FRAME_ICOUNT_OFFSET (0x9c0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_FRAME_ICOUNT_MASK (0xffff)

// args: data (16-bit)
static __inline uint16_t acamera_fpga_video_capture4_dma_writer_frame_icount_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2099c0L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: frame wcount
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// count of outgoing frame writes (starts) from vdma_writer sent to AXI output, non resetable, rolls over, updates at pixel 1 of new frame on video in
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_FRAME_WCOUNT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_FRAME_WCOUNT_DATASIZE (16)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_FRAME_WCOUNT_OFFSET (0x9c4)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_FRAME_WCOUNT_MASK (0xffff)

// args: data (16-bit)
static __inline uint16_t acamera_fpga_video_capture4_dma_writer_frame_wcount_read(uintptr_t base) {
    return (uint16_t)((system_hw_read_32(0x2099c4L) & 0xffff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: clear alarms
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 0>1 transition(synchronous detection) causes local axi/video alarm clear
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_CLEAR_ALARMS_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_CLEAR_ALARMS_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_CLEAR_ALARMS_OFFSET (0x9c8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_CLEAR_ALARMS_MASK (0x1)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_clear_alarms_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2099c8L);
    system_hw_write_32(0x2099c8L, (((uint32_t) (data & 0x1)) << 0) | (curr & 0xfffffffe));
}
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_clear_alarms_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099c8L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: max_burst_length_is_8
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1= Reduce default AXI max_burst_length from 16 to 8, 0= Dont reduce
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_MAX_BURST_LENGTH_IS_8_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_MAX_BURST_LENGTH_IS_8_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_MAX_BURST_LENGTH_IS_8_OFFSET (0x9c8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_MAX_BURST_LENGTH_IS_8_MASK (0x2)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_max_burst_length_is_8_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2099c8L);
    system_hw_write_32(0x2099c8L, (((uint32_t) (data & 0x1)) << 1) | (curr & 0xfffffffd));
}
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_max_burst_length_is_8_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099c8L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: max_burst_length_is_4
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1= Reduce default AXI max_burst_length from 16 to 4, 0= Dont reduce( has priority overmax_burst_length_is_8!)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_MAX_BURST_LENGTH_IS_4_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_MAX_BURST_LENGTH_IS_4_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_MAX_BURST_LENGTH_IS_4_OFFSET (0x9c8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_MAX_BURST_LENGTH_IS_4_MASK (0x4)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_max_burst_length_is_4_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2099c8L);
    system_hw_write_32(0x2099c8L, (((uint32_t) (data & 0x1)) << 2) | (curr & 0xfffffffb));
}
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_max_burst_length_is_4_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099c8L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: write timeout disable
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//    At end of frame an optional timeout is applied to wait for AXI writes to completed/accepted befotre caneclling and flushing.
//    0= Timeout Enabled, timeout count can decrement.
//    1 = Disable timeout, timeout count can't decrement.
//    
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WRITE_TIMEOUT_DISABLE_DEFAULT (0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WRITE_TIMEOUT_DISABLE_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WRITE_TIMEOUT_DISABLE_OFFSET (0x9c8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WRITE_TIMEOUT_DISABLE_MASK (0x8)

// args: data (1-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_write_timeout_disable_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2099c8L);
    system_hw_write_32(0x2099c8L, (((uint32_t) (data & 0x1)) << 3) | (curr & 0xfffffff7));
}
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_write_timeout_disable_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099c8L) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: awmaxwait_limit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// awvalid maxwait limit(cycles) to raise axi_fail_awmaxwait alarm . zero disables alarm raise.
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AWMAXWAIT_LIMIT_DEFAULT (0x00)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AWMAXWAIT_LIMIT_DATASIZE (8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AWMAXWAIT_LIMIT_OFFSET (0x9cc)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AWMAXWAIT_LIMIT_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_awmaxwait_limit_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2099ccL);
    system_hw_write_32(0x2099ccL, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_awmaxwait_limit_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099ccL) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: wmaxwait_limit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// wvalid maxwait limit(cycles) to raise axi_fail_wmaxwait alarm . zero disables alarm raise
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WMAXWAIT_LIMIT_DEFAULT (0x00)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WMAXWAIT_LIMIT_DATASIZE (8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WMAXWAIT_LIMIT_OFFSET (0x9d0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WMAXWAIT_LIMIT_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_wmaxwait_limit_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2099d0L);
    system_hw_write_32(0x2099d0L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_wmaxwait_limit_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099d0L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: wxact_ostand_limit
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// number oustsanding write transactions(bursts)(responses..1 per burst) limit to raise axi_fail_wxact_ostand. zero disables alarm raise
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WXACT_OSTAND_LIMIT_DEFAULT (0x00)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WXACT_OSTAND_LIMIT_DATASIZE (8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WXACT_OSTAND_LIMIT_OFFSET (0x9d4)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_WXACT_OSTAND_LIMIT_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_wxact_ostand_limit_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2099d4L);
    system_hw_write_32(0x2099d4L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_wxact_ostand_limit_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099d4L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: axi_fail_bresp
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high to indicate bad  bresp captured 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_FAIL_BRESP_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_FAIL_BRESP_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_FAIL_BRESP_OFFSET (0x9d8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_FAIL_BRESP_MASK (0x1)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_axi_fail_bresp_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099d8L) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: axi_fail_awmaxwait
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high when awmaxwait_limit reached 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_FAIL_AWMAXWAIT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_FAIL_AWMAXWAIT_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_FAIL_AWMAXWAIT_OFFSET (0x9d8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_FAIL_AWMAXWAIT_MASK (0x2)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_axi_fail_awmaxwait_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099d8L) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: axi_fail_wmaxwait
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high when wmaxwait_limit reached 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_FAIL_WMAXWAIT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_FAIL_WMAXWAIT_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_FAIL_WMAXWAIT_OFFSET (0x9d8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_FAIL_WMAXWAIT_MASK (0x4)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_axi_fail_wmaxwait_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099d8L) & 0x4) >> 2);
}
// ------------------------------------------------------------------------------ //
// Register: axi_fail_wxact_ostand
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high when wxact_ostand_limit reached 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_FAIL_WXACT_OSTAND_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_FAIL_WXACT_OSTAND_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_FAIL_WXACT_OSTAND_OFFSET (0x9d8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_FAIL_WXACT_OSTAND_MASK (0x8)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_axi_fail_wxact_ostand_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099d8L) & 0x8) >> 3);
}
// ------------------------------------------------------------------------------ //
// Register: vi_fail_active_width
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high to indicate mismatched active_width detected 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_VI_FAIL_ACTIVE_WIDTH_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_VI_FAIL_ACTIVE_WIDTH_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_VI_FAIL_ACTIVE_WIDTH_OFFSET (0x9d8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_VI_FAIL_ACTIVE_WIDTH_MASK (0x10)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_vi_fail_active_width_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099d8L) & 0x10) >> 4);
}
// ------------------------------------------------------------------------------ //
// Register: vi_fail_active_height
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high to indicate mismatched active_height detected ( also raised on missing field!) 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_VI_FAIL_ACTIVE_HEIGHT_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_VI_FAIL_ACTIVE_HEIGHT_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_VI_FAIL_ACTIVE_HEIGHT_OFFSET (0x9d8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_VI_FAIL_ACTIVE_HEIGHT_MASK (0x20)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_vi_fail_active_height_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099d8L) & 0x20) >> 5);
}
// ------------------------------------------------------------------------------ //
// Register: vi_fail_interline_blanks
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high to indicate interline blanking below min 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_VI_FAIL_INTERLINE_BLANKS_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_VI_FAIL_INTERLINE_BLANKS_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_VI_FAIL_INTERLINE_BLANKS_OFFSET (0x9d8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_VI_FAIL_INTERLINE_BLANKS_MASK (0x40)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_vi_fail_interline_blanks_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099d8L) & 0x40) >> 6);
}
// ------------------------------------------------------------------------------ //
// Register: vi_fail_interframe_blanks
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  clearable alarm, high to indicate interframe blanking below min 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_VI_FAIL_INTERFRAME_BLANKS_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_VI_FAIL_INTERFRAME_BLANKS_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_VI_FAIL_INTERFRAME_BLANKS_OFFSET (0x9d8)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_VI_FAIL_INTERFRAME_BLANKS_MASK (0x80)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_vi_fail_interframe_blanks_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099d8L) & 0x80) >> 7);
}
// ------------------------------------------------------------------------------ //
// Register: video_alarm
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  active high, problem found on video port(s) ( active width/height or interline/frame blanks failure) 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_VIDEO_ALARM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_VIDEO_ALARM_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_VIDEO_ALARM_OFFSET (0x9dc)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_VIDEO_ALARM_MASK (0x1)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_video_alarm_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099dcL) & 0x1) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: axi_alarm
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
//  active high, problem found on axi port(s)( bresp or awmaxwait or wmaxwait or wxact_ostand failure ) 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_ALARM_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_ALARM_DATASIZE (1)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_ALARM_OFFSET (0x9dc)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_AXI_ALARM_MASK (0x2)

// args: data (1-bit)
static __inline uint8_t acamera_fpga_video_capture4_dma_writer_axi_alarm_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099dcL) & 0x2) >> 1);
}
// ------------------------------------------------------------------------------ //
// Register: blk_config
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// block configuration (reserved)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BLK_CONFIG_DEFAULT (0x0000)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BLK_CONFIG_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BLK_CONFIG_OFFSET (0x9e0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BLK_CONFIG_MASK (0xffffffff)

// args: data (32-bit)
static __inline void acamera_fpga_video_capture4_dma_writer_blk_config_write(uintptr_t base, uint32_t data) {
    system_hw_write_32(0x2099e0L, data);
}
static __inline uint32_t acamera_fpga_video_capture4_dma_writer_blk_config_read(uintptr_t base) {
    return system_hw_read_32(0x2099e0L);
}
// ------------------------------------------------------------------------------ //
// Register: blk_status
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// block status output (reserved)
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BLK_STATUS_DEFAULT (0x0)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BLK_STATUS_DATASIZE (32)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BLK_STATUS_OFFSET (0x9e4)
#define ACAMERA_FPGA_VIDEO_CAPTURE4_DMA_WRITER_BLK_STATUS_MASK (0xffffffff)

// args: data (32-bit)
static __inline uint32_t acamera_fpga_video_capture4_dma_writer_blk_status_read(uintptr_t base) {
    return system_hw_read_32(0x2099e4L);
}
// ------------------------------------------------------------------------------ //
// Group: FPGA Status Misc
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register settings for some added status observation logic
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// Register: Processing Started
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 
//          Register indicating frame processing started. It shall be set when processing starts (which may be FE interrupt) and
//          cleared when processing ends by software. During it is set, if an interrupt is observed from ISP, this will be reported
//          as an error condition at the below register called vblank_processing_err
//        
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_STATUS_PROCESSING_STARTED_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_STATUS_PROCESSING_STARTED_DATASIZE (8)
#define ACAMERA_FPGA_FPGA_STATUS_PROCESSING_STARTED_OFFSET (0x9e8)
#define ACAMERA_FPGA_FPGA_STATUS_PROCESSING_STARTED_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_fpga_status_processing_started_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2099e8L);
    system_hw_write_32(0x2099e8L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_fpga_status_processing_started_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099e8L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: vblank Processing Error
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// When an interrupt is observed while Processing Started register is set, this flag is set to indicate an error condition
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_STATUS_VBLANK_PROCESSING_ERROR_DEFAULT (0x0)
#define ACAMERA_FPGA_FPGA_STATUS_VBLANK_PROCESSING_ERROR_DATASIZE (8)
#define ACAMERA_FPGA_FPGA_STATUS_VBLANK_PROCESSING_ERROR_OFFSET (0x9ec)
#define ACAMERA_FPGA_FPGA_STATUS_VBLANK_PROCESSING_ERROR_MASK (0xff)

// args: data (8-bit)
static __inline uint8_t acamera_fpga_fpga_status_vblank_processing_error_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099ecL) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Clear vblank Processing Error
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// in order to clear vblank processing error flag, the LS bit of this register should transit from 0 to 1. 
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_STATUS_CLEAR_VBLANK_PROCESSING_ERROR_DEFAULT (0)
#define ACAMERA_FPGA_FPGA_STATUS_CLEAR_VBLANK_PROCESSING_ERROR_DATASIZE (8)
#define ACAMERA_FPGA_FPGA_STATUS_CLEAR_VBLANK_PROCESSING_ERROR_OFFSET (0x9f0)
#define ACAMERA_FPGA_FPGA_STATUS_CLEAR_VBLANK_PROCESSING_ERROR_MASK (0xff)

// args: data (8-bit)
static __inline void acamera_fpga_fpga_status_clear_vblank_processing_error_write(uintptr_t base, uint8_t data) {
    uint32_t curr = system_hw_read_32(0x2099f0L);
    system_hw_write_32(0x2099f0L, (((uint32_t) (data & 0xff)) << 0) | (curr & 0xffffff00));
}
static __inline uint8_t acamera_fpga_fpga_status_clear_vblank_processing_error_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099f0L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
// Register: Is vblank
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// 1=vertical blanking 0=active lines are feeded into ISP
// ------------------------------------------------------------------------------ //

#define ACAMERA_FPGA_FPGA_STATUS_IS_VBLANK_DEFAULT (0x0)
#define ACAMERA_FPGA_FPGA_STATUS_IS_VBLANK_DATASIZE (8)
#define ACAMERA_FPGA_FPGA_STATUS_IS_VBLANK_OFFSET (0x9f4)
#define ACAMERA_FPGA_FPGA_STATUS_IS_VBLANK_MASK (0xff)

// args: data (8-bit)
static __inline uint8_t acamera_fpga_fpga_status_is_vblank_read(uintptr_t base) {
    return (uint8_t)((system_hw_read_32(0x2099f4L) & 0xff) >> 0);
}
// ------------------------------------------------------------------------------ //
#endif //__ACAMERA_FPGA_CONFIG_H__
