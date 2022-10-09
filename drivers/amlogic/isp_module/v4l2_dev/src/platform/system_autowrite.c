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

#include <linux/kernel.h> 
#include <asm/uaccess.h>
#include <linux/gfp.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <asm/types.h>
#include <asm/io.h>
#include <linux/time.h>
#include <linux/delay.h>
#include "acamera_types.h"
#include "acamera_logger.h"
#include "system_autowrite.h"

static void *p_hw_fr_base = NULL;
static void *p_hw_ds1_base = NULL;
static void *p_hw_sc0_base = NULL;

void auto_cap_init(void)
{
	p_hw_fr_base = ioremap(FR_AUTO_WRITE_BASE, 0x80);
	p_hw_ds1_base = ioremap(DS1_AUTO_WRITE_BASE, 0x80);
	p_hw_sc0_base = ioremap(SC0_AUTO_WRITE_BASE, 0x80);
}

void auto_cap_deinit(void)
{
    iounmap( p_hw_fr_base );
	iounmap( p_hw_ds1_base );
	iounmap( p_hw_sc0_base );
}

uint32_t autowrite_fr_ping_start( void )
{
    uint32_t result = 0;

	if ( p_hw_fr_base != NULL ) {
       result = ioread32( p_hw_fr_base + MIPI_BL_PING_ADDR0_ST );
	}

    return result;
}

uint32_t autowrite_fr_pong_start( void )
{
    uint32_t result = 0;

	if ( p_hw_fr_base != NULL ) {
       result = ioread32( p_hw_fr_base + MIPI_BL_PONG_ADDR0_ST );
	}

    return result;
}

uint32_t autowrite_fr_image_buffer_stride_read( void )
{
    uint32_t result = 0;

	if ( p_hw_fr_base != NULL ) {
       result = ioread32( p_hw_fr_base + MIPI_BL_FRAME_SIZE0 );
	}
	
    return result;
}

uint32_t autowrite_fr_image_buffer_stride_read1( void )
{
    uint32_t result = 0;

	if ( p_hw_fr_base != NULL ) {
       result = ioread32( p_hw_fr_base + MIPI_BL_FRAME_SIZE1 );
	}
	
    return result;
}

uint32_t autowrite_fr_writer_frame_wcount_read( void )
{
    uint32_t result = 0;

	if ( p_hw_fr_base != NULL ) {
       result = ioread32( p_hw_fr_base + MIPI_BL_COUNT );
	}
	
    return result;
}

uint32_t autowrite_fr_writer_memsize_read( void )
{
    uint32_t result = 0;

	if ( p_hw_fr_base != NULL ) {
       result = ioread32( p_hw_fr_base + MIPI_BL_FRAME_BUF_SIZE0 );
	}
	
    return result;
}

uint32_t autowrite_fr_writer_memsize_read1( void )
{
    uint32_t result = 0;

	if ( p_hw_fr_base != NULL ) {
       result = ioread32( p_hw_fr_base + MIPI_BL_FRAME_BUF_SIZE1 );
	}
	
    return result;
}

uint32_t autowrite_fr_drop_jump_read( void )
{
	uint32_t result = 0;

	if ( p_hw_fr_base != NULL ) {
		result = (ioread32( p_hw_fr_base + MIPI_BL_CTRL1 ) & 0xFF0000) >> 16;
	}

	return result;
}

uint32_t autowrite_fr_drop_write_read( void )
{
	uint32_t result = 0;

	if ( p_hw_fr_base != NULL ) {
		result = (ioread32( p_hw_fr_base + MIPI_BL_CTRL1 ) & 0xFF00) >> 8;
	}

	return result;
}

uint32_t autowrite_fr_drop_mode_status( void )
{
	uint32_t result = 0;

	if ( p_hw_fr_base != NULL ) {
		result = (ioread32( p_hw_fr_base + MIPI_BL_CTRL0 ) & (1 << 5));
	}

	return result;
}

uint32_t autowrite_ds1_ping_start( void )
{
    uint32_t result = 0;

	if ( p_hw_ds1_base != NULL ) {
       result = ioread32( p_hw_ds1_base + MIPI_BL_PING_ADDR0_ST );
	}

   return result;
}

uint32_t autowrite_ds1_pong_start( void )
{
    uint32_t result = 0;

	if ( p_hw_ds1_base != NULL ) {
       result = ioread32( p_hw_ds1_base + MIPI_BL_PONG_ADDR0_ST );
	}

    return result;
}

uint32_t autowrite_ds1_image_buffer_stride_read( void )
{
    uint32_t result = 0;

	if ( p_hw_ds1_base != NULL ) {
       result = ioread32( p_hw_ds1_base + MIPI_BL_FRAME_SIZE0 );
	}
	
    return result;
}

uint32_t autowrite_ds1_image_buffer_stride_read1( void )
{
    uint32_t result = 0;

	if ( p_hw_ds1_base != NULL ) {
       result = ioread32( p_hw_ds1_base + MIPI_BL_FRAME_SIZE1 );
	}
	
    return result;
}

uint32_t autowrite_ds1_writer_frame_wcount_read( void )
{
    uint32_t result = 0;

	if ( p_hw_ds1_base != NULL ) {
       result = ioread32( p_hw_ds1_base + MIPI_BL_COUNT );
	}
	
    return result;
}

uint32_t autowrite_ds1_writer_memsize_read( void )
{
    uint32_t result = 0;

	if ( p_hw_ds1_base != NULL ) {
       result = ioread32( p_hw_ds1_base + MIPI_BL_FRAME_BUF_SIZE0 );
	}
	
    return result;
}

uint32_t autowrite_ds1_writer_memsize_read1( void )
{
    uint32_t result = 0;

	if ( p_hw_ds1_base != NULL ) {
       result = ioread32( p_hw_ds1_base + MIPI_BL_FRAME_BUF_SIZE1 );
	}
	
    return result;
}

uint32_t autowrite_ds1_drop_jump_read( void )
{
	uint32_t result = 0;

	if ( p_hw_ds1_base != NULL ) {
		result = (ioread32( p_hw_ds1_base + MIPI_BL_CTRL1 ) & 0xFF0000) >> 16;
	}

	return result;
}

uint32_t autowrite_ds1_drop_write_read( void )
{
	uint32_t result = 0;

	if ( p_hw_ds1_base != NULL ) {
		result = (ioread32( p_hw_ds1_base + MIPI_BL_CTRL1 ) & 0xFF00) >> 8;
	}

	return result;
}

uint32_t autowrite_ds1_drop_mode_status( void )
{
	uint32_t result = 0;

	if ( p_hw_ds1_base != NULL ) {
		result = (ioread32( p_hw_ds1_base + MIPI_BL_CTRL0 ) & (1 << 5));
	}

	return result;
}

uint32_t autowrite_sc0_ping_start( void )
{
    uint32_t result = 0;

	if ( p_hw_sc0_base != NULL ) {
       result = ioread32( p_hw_sc0_base + MIPI_BL_PING_ADDR0_ST );
	}

    return result;
}

uint32_t autowrite_sc0_pong_start( void )
{
    uint32_t result = 0;

	if ( p_hw_sc0_base != NULL ) {
       result = ioread32( p_hw_sc0_base + MIPI_BL_PONG_ADDR0_ST );
	}

    return result;
}

uint32_t autowrite_sc0_image_buffer_stride_read( void )
{
    uint32_t result = 0;

	if ( p_hw_sc0_base != NULL ) {
       result = ioread32( p_hw_sc0_base + MIPI_BL_FRAME_SIZE0 );
	}

    return result;
}

uint32_t autowrite_sc0_image_buffer_stride_read1( void )
{
    uint32_t result = 0;

	if ( p_hw_sc0_base != NULL ) {
       result = ioread32( p_hw_sc0_base + MIPI_BL_FRAME_SIZE1 );
	}

    return result;
}

uint32_t autowrite_sc0_writer_frame_wcount_read( void )
{
    uint32_t result = 0;

	if ( p_hw_sc0_base != NULL ) {
       result = ioread32( p_hw_sc0_base + MIPI_BL_COUNT );
	}
	
    return result;
}

uint32_t autowrite_sc0_writer_memsize_read( void )
{
    uint32_t result = 0;

	if ( p_hw_sc0_base != NULL ) {
       result = ioread32( p_hw_sc0_base + MIPI_BL_FRAME_BUF_SIZE0 );
	}
	
    return result;
}

uint32_t autowrite_sc0_writer_memsize_read1( void )
{
    uint32_t result = 0;

	if ( p_hw_sc0_base != NULL ) {
       result = ioread32( p_hw_sc0_base + MIPI_BL_FRAME_BUF_SIZE1 );
	}
	
    return result;
}

uint32_t autowrite_sc0_drop_jump_read( void )
{
	uint32_t result = 0;

	if ( p_hw_sc0_base != NULL ) {
		result = (ioread32( p_hw_sc0_base + MIPI_BL_CTRL1 ) & 0xFF0000) >> 16;
	}

	return result;
}

uint32_t autowrite_sc0_drop_write_read( void )
{
	uint32_t result = 0;

	if ( p_hw_sc0_base != NULL ) {
		result = (ioread32( p_hw_sc0_base + MIPI_BL_CTRL1 ) & 0xFF00) >> 8;
	}

	return result;
}

uint32_t autowrite_sc0_drop_mode_status( void )
{
	uint32_t result = 0;

	if ( p_hw_sc0_base != NULL ) {
		result = (ioread32( p_hw_sc0_base + MIPI_BL_CTRL0 ) & (1 << 5));
	}

	return result;
}

uint32_t autowrite_fr_start_address_read( void )
{
    uint32_t result = 0;

	if ( p_hw_fr_base != NULL ) {
       result = ioread32( p_hw_fr_base + MIPI_BL_FRAME_BUFFER_START_ADDR0 );
	}
	
    return result;
}

uint32_t autowrite_ds1_start_address_read( void )
{
    uint32_t result = 0;

	if ( p_hw_ds1_base != NULL ) {
       result = ioread32( p_hw_ds1_base + MIPI_BL_FRAME_BUFFER_START_ADDR0 );
	}
	
    return result;
}

uint32_t autowrite_sc0_start_address_read( void )
{
    uint32_t result = 0;

	if ( p_hw_sc0_base != NULL ) {
       result = ioread32( p_hw_sc0_base + MIPI_BL_FRAME_BUFFER_START_ADDR0 );
	}
	
    return result;
}

uint32_t autowrite_fr_stop( void )
{
    uint32_t result = 0;
    uint32_t val = 0;
	
	if ( p_hw_fr_base != NULL ) {
		val = ioread32( p_hw_fr_base + MIPI_BL_CTRL0);
		val = val | (0x1 << 4);
	    void *ptr = (void *)( p_hw_fr_base + MIPI_BL_CTRL0 );
		iowrite32(val, ptr); // ladder stop next frame
		
		//val = read_reg(base_addr + MIPI_BL_CTRL0);
		//val = val & (~(1 << 13)) & (~(1 << 0));
		//write_reg(val, base_addr + MIPI_BL_CTRL0); // ladder disable
	}
	
    return result;
}

uint32_t autowrite_ds1_stop( void )
{
    uint32_t result = 0;
    uint32_t val = 0;
	
	if ( p_hw_ds1_base != NULL ) {
		val = ioread32( p_hw_ds1_base + MIPI_BL_CTRL0);
		val = val | (0x1 << 4);
	    void *ptr = (void *)( p_hw_ds1_base + MIPI_BL_CTRL0 );
		iowrite32(val, ptr); // ladder stop next frame
		
		//val = read_reg(base_addr + MIPI_BL_CTRL0);
		//val = val & (~(1 << 13)) & (~(1 << 0));
		//write_reg(val, base_addr + MIPI_BL_CTRL0); // ladder disable
	}
	
    return result;
}

uint32_t autowrite_sc0_stop( void )
{
    uint32_t result = 0;
    uint32_t val = 0;
	
	if ( p_hw_sc0_base != NULL ) {
		val = ioread32( p_hw_sc0_base + MIPI_BL_CTRL0);
		val = val | (0x1 << 4);
	    void *ptr = (void *)( p_hw_sc0_base + MIPI_BL_CTRL0 );
		iowrite32(val, ptr); // ladder stop next frame
		
		//val = read_reg(base_addr + MIPI_BL_CTRL0);
		//val = val & (~(1 << 13)) & (~(1 << 0));
		//write_reg(val, base_addr + MIPI_BL_CTRL0); // ladder disable
	}
	
    return result;
}

/*****************************************************************/
static void auto_write_fr_cfg_buff(void *cfg)
{
	auto_write_cfg_t *a_cfg = NULL;

	if (cfg == NULL) {
		LOG(LOG_ERR, "Error input param");
		return;
	}

	a_cfg = cfg;

	iowrite32(0x00000000, p_hw_fr_base + MIPI_BL_FRAME_TH);
	iowrite32(a_cfg->frame_buffer_start0, p_hw_fr_base + MIPI_BL_FRAME_BUFFER_START_ADDR0P);
	iowrite32(a_cfg->frame_buffer_start0, p_hw_fr_base + MIPI_BL_FRAME_BUFFER_START_ADDR0);
	iowrite32(a_cfg->frame_size0, p_hw_fr_base + MIPI_BL_FRAME_SIZE0);
	iowrite32(a_cfg->frame_buffer_size0, p_hw_fr_base + MIPI_BL_FRAME_BUF_SIZE0);
	iowrite32(a_cfg->frame_buffer_start1, p_hw_fr_base + MIPI_BL_FRAME_BUFFER_START_ADDR1);
	iowrite32(a_cfg->frame_size1, p_hw_fr_base + MIPI_BL_FRAME_SIZE1);
	iowrite32(a_cfg->frame_buffer_size1, p_hw_fr_base + MIPI_BL_FRAME_BUF_SIZE1);
}

static void auto_write_ds1_cfg_buff(void *cfg)
{
	auto_write_cfg_t *a_cfg = NULL;

	if (cfg == NULL) {
		LOG(LOG_ERR, "Error input param");
		return;
	}

	a_cfg = cfg;

	iowrite32(0x00000000, p_hw_ds1_base + MIPI_BL_FRAME_TH);
	iowrite32(a_cfg->frame_buffer_start0, p_hw_ds1_base + MIPI_BL_FRAME_BUFFER_START_ADDR0P);
	iowrite32(a_cfg->frame_buffer_start0, p_hw_ds1_base + MIPI_BL_FRAME_BUFFER_START_ADDR0);
	iowrite32(a_cfg->frame_size0, p_hw_ds1_base + MIPI_BL_FRAME_SIZE0);
	iowrite32(a_cfg->frame_buffer_size0, p_hw_ds1_base + MIPI_BL_FRAME_BUF_SIZE0);
	iowrite32(a_cfg->frame_buffer_start1, p_hw_ds1_base + MIPI_BL_FRAME_BUFFER_START_ADDR1);
	iowrite32(a_cfg->frame_size1, p_hw_ds1_base + MIPI_BL_FRAME_SIZE1);
	iowrite32(a_cfg->frame_buffer_size1, p_hw_ds1_base + MIPI_BL_FRAME_BUF_SIZE1);
}

static void auto_write_sc0_cfg_buff(void *cfg)
{
	auto_write_cfg_t *a_cfg = NULL;

	if (cfg == NULL) {
		LOG(LOG_ERR, "Error input param");
		return;
	}

	a_cfg = cfg;

	iowrite32(0x00000000, p_hw_sc0_base + MIPI_BL_FRAME_TH);
	iowrite32(a_cfg->frame_buffer_start0, p_hw_sc0_base + MIPI_BL_FRAME_BUFFER_START_ADDR0P);
	iowrite32(a_cfg->frame_buffer_start0, p_hw_sc0_base + MIPI_BL_FRAME_BUFFER_START_ADDR0);
	iowrite32(a_cfg->frame_size0, p_hw_sc0_base + MIPI_BL_FRAME_SIZE0);
	iowrite32(a_cfg->frame_buffer_size0, p_hw_sc0_base + MIPI_BL_FRAME_BUF_SIZE0);
	iowrite32(a_cfg->frame_buffer_start1, p_hw_sc0_base + MIPI_BL_FRAME_BUFFER_START_ADDR1);
	iowrite32(a_cfg->frame_size1, p_hw_sc0_base + MIPI_BL_FRAME_SIZE1);
	iowrite32(a_cfg->frame_buffer_size1, p_hw_sc0_base + MIPI_BL_FRAME_BUF_SIZE1);
}

static void auto_write_fr_reset(uint32_t p_type)
{
	iowrite32(1 << 15, p_hw_fr_base + MIPI_BL_CTRL0);
	mdelay(1);
	iowrite32(0 << 15, p_hw_fr_base + MIPI_BL_CTRL0);
}

static void auto_write_ds1_reset(uint32_t p_type)
{
	iowrite32(1 << 15, p_hw_ds1_base + MIPI_BL_CTRL0);
	mdelay(1);
	iowrite32(0 << 15, p_hw_ds1_base + MIPI_BL_CTRL0);
}

static void auto_write_sc0_reset(uint32_t p_type)
{
	iowrite32(1 << 15, p_hw_sc0_base + MIPI_BL_CTRL0);
	mdelay(1);
	iowrite32(0 << 15, p_hw_sc0_base + MIPI_BL_CTRL0);
}

void autowrite_path_reset(void)
{
	auto_write_fr_reset(0);
	auto_write_ds1_reset(0);
	auto_write_sc0_reset(0);
}

static void auto_write_fr_cfg_drop(void *cfg)
{
	auto_write_cfg_t *a_cfg = NULL;
	uint32_t val = 0;

	if (cfg == NULL) {
		LOG(LOG_ERR, "Error input param");
		return;
	}

	a_cfg = cfg;

	val = (a_cfg->wreay_delay & 0xf) << 28 | (a_cfg->bvalid_delay & 0xf) << 24 |
			(a_cfg->drop_jump & 0xff) << 16 | (a_cfg->drop_write & 0xff) << 8 |
			(a_cfg->drop_skip & 0xff);

	iowrite32(val, p_hw_fr_base + MIPI_BL_CTRL1);
}

static void auto_write_ds1_cfg_drop(void *cfg)
{
	auto_write_cfg_t *a_cfg = NULL;
	uint32_t val = 0;

	if (cfg == NULL) {
		LOG(LOG_ERR, "Error input param");
		return;
	}

	a_cfg = cfg;

	val = (a_cfg->wreay_delay & 0xf) << 28 | (a_cfg->bvalid_delay & 0xf) << 24 |
			(a_cfg->drop_jump & 0xff) << 16 | (a_cfg->drop_write & 0xff) << 8 |
			(a_cfg->drop_skip & 0xff);

	iowrite32(val, p_hw_ds1_base + MIPI_BL_CTRL1);
}

static void auto_write_sc0_cfg_drop(void *cfg)
{
	auto_write_cfg_t *a_cfg = NULL;
	uint32_t val = 0;

	if (cfg == NULL) {
		LOG(LOG_ERR, "Error input param");
		return;
	}

	a_cfg = cfg;

	val = (a_cfg->wreay_delay & 0xf) << 28 | (a_cfg->bvalid_delay & 0xf) << 24 |
			(a_cfg->drop_jump & 0xff) << 16 | (a_cfg->drop_write & 0xff) << 8 |
			(a_cfg->drop_skip & 0xff);

	iowrite32(val, p_hw_sc0_base + MIPI_BL_CTRL1);
}

void auto_write_cfg_fr_ping_pong(void *cfg)
{
	auto_write_cfg_t *a_cfg = NULL;

	if (cfg == NULL) {
		LOG(LOG_ERR, "Error input param");
		return;
	}

	a_cfg = cfg;

	iowrite32(a_cfg->ping_st0, p_hw_fr_base + MIPI_BL_PING_ADDR0_ST);
	iowrite32(a_cfg->ping_ed0, p_hw_fr_base + MIPI_BL_PING_ADDR0_ED);
	iowrite32(a_cfg->pong_st0, p_hw_fr_base + MIPI_BL_PONG_ADDR0_ST);
	iowrite32(a_cfg->pong_ed0, p_hw_fr_base + MIPI_BL_PONG_ADDR0_ED);
	iowrite32(a_cfg->ping_st1, p_hw_fr_base + MIPI_BL_PING_ADDR1_ST);
	iowrite32(a_cfg->ping_ed1, p_hw_fr_base + MIPI_BL_PING_ADDR1_ED);
	iowrite32(a_cfg->pong_st1, p_hw_fr_base + MIPI_BL_PONG_ADDR1_ST);
	iowrite32(a_cfg->pong_ed1, p_hw_fr_base + MIPI_BL_PONG_ADDR1_ED);

	LOG(LOG_CRIT, "%d channel finished cfg ping pong.\n", ((auto_write_cfg_t*)cfg)->p_type);
}

void auto_write_cfg_ds1_ping_pong(void *cfg)
{
	auto_write_cfg_t *a_cfg = NULL;

	if (cfg == NULL) {
		LOG(LOG_ERR, "Error input param");
		return;
	}

	a_cfg = cfg;

	iowrite32(a_cfg->ping_st0, p_hw_ds1_base + MIPI_BL_PING_ADDR0_ST);
	iowrite32(a_cfg->ping_ed0, p_hw_ds1_base + MIPI_BL_PING_ADDR0_ED);
	iowrite32(a_cfg->pong_st0, p_hw_ds1_base + MIPI_BL_PONG_ADDR0_ST);
	iowrite32(a_cfg->pong_ed0, p_hw_ds1_base + MIPI_BL_PONG_ADDR0_ED);
	iowrite32(a_cfg->ping_st1, p_hw_ds1_base + MIPI_BL_PING_ADDR1_ST);
	iowrite32(a_cfg->ping_ed1, p_hw_ds1_base + MIPI_BL_PING_ADDR1_ED);
	iowrite32(a_cfg->pong_st1, p_hw_ds1_base + MIPI_BL_PONG_ADDR1_ST);
	iowrite32(a_cfg->pong_ed1, p_hw_ds1_base + MIPI_BL_PONG_ADDR1_ED);

	LOG(LOG_CRIT, "%d channel finished cfg ping pong.\n", ((auto_write_cfg_t*)cfg)->p_type);
}

void auto_write_cfg_sc0_ping_pong(void *cfg)
{
	auto_write_cfg_t *a_cfg = NULL;

	if (cfg == NULL) {
		LOG(LOG_ERR, "Error input param");
		return;
	}

	LOG(LOG_CRIT, "%d channel start cfg ping pong.\n", ((auto_write_cfg_t*)cfg)->p_type);

	a_cfg = cfg;

	iowrite32(a_cfg->ping_st0, p_hw_sc0_base + MIPI_BL_PING_ADDR0_ST);
	iowrite32(a_cfg->ping_ed0, p_hw_sc0_base + MIPI_BL_PING_ADDR0_ED);
	iowrite32(a_cfg->pong_st0, p_hw_sc0_base + MIPI_BL_PONG_ADDR0_ST);
	iowrite32(a_cfg->pong_ed0, p_hw_sc0_base + MIPI_BL_PONG_ADDR0_ED);
	iowrite32(a_cfg->ping_st1, p_hw_sc0_base + MIPI_BL_PING_ADDR1_ST);
	iowrite32(a_cfg->ping_ed1, p_hw_sc0_base + MIPI_BL_PING_ADDR1_ED);
	iowrite32(a_cfg->pong_st1, p_hw_sc0_base + MIPI_BL_PONG_ADDR1_ST);
	iowrite32(a_cfg->pong_ed1, p_hw_sc0_base + MIPI_BL_PONG_ADDR1_ED);

	LOG(LOG_CRIT, "%d channel finished cfg ping pong.\n", ((auto_write_cfg_t*)cfg)->p_type);
}

void auto_write_fr_init(void *cfg)
{
	auto_write_cfg_t *a_cfg = NULL;

	if (cfg == NULL) {
		LOG(LOG_ERR, "Error input param");
		return;
	}

	a_cfg = cfg;

	auto_write_cfg_fr_ping_pong(cfg);

	auto_write_fr_reset(a_cfg->p_type);

	auto_write_fr_cfg_buff(cfg);

	auto_write_fr_cfg_drop(cfg);
}

void auto_write_ds1_init(void *cfg)
{
	auto_write_cfg_t *a_cfg = NULL;

	if (cfg == NULL) {
		LOG(LOG_ERR, "Error input param");
		return;
	}

	a_cfg = cfg;

	auto_write_cfg_ds1_ping_pong(cfg);

	auto_write_ds1_reset(a_cfg->p_type);

	auto_write_ds1_cfg_buff(cfg);

	auto_write_ds1_cfg_drop(cfg);
}

void auto_write_sc0_init(void *cfg)
{
	auto_write_cfg_t *a_cfg = NULL;

	if (cfg == NULL) {
		LOG(LOG_ERR, "Error input param");
		return;
	}

	a_cfg = cfg;

	auto_write_cfg_sc0_ping_pong(cfg);

	auto_write_sc0_reset(a_cfg->p_type);

	auto_write_sc0_cfg_buff(cfg);

	auto_write_sc0_cfg_drop(cfg);
}

void auto_write_fr_start(void *cfg)
{
	auto_write_cfg_t *a_cfg = NULL;
	uint32_t val = 0;

	if (cfg == NULL) {
		LOG(LOG_ERR, "Error input param");
		return;
	}

	a_cfg = cfg;

	val = ioread32(p_hw_fr_base + MIPI_BL_CTRL0);
	val = val | 0x2;
	iowrite32(val, p_hw_fr_base + MIPI_BL_CTRL0);// reset based addr

	val = ioread32(p_hw_fr_base + MIPI_BL_CTRL0);
	val = val & (~0x2);
	iowrite32(val, p_hw_fr_base + MIPI_BL_CTRL0);// reset based addr

	val = ioread32(p_hw_fr_base + MIPI_BL_CTRL0);
	val = val | ((a_cfg->th_enable & 0x1) << 2);
	val = val | ((a_cfg->th_cnt & 0xffff) << 16);
	iowrite32(val, p_hw_fr_base + MIPI_BL_CTRL0);// reset based addr

	val = ioread32(p_hw_fr_base + MIPI_BL_CTRL0);

	val = ioread32(p_hw_fr_base + MIPI_BL_CTRL0);
	val = val | ((a_cfg->drop_enable & 0x1) << 5);
	iowrite32(val, p_hw_fr_base + MIPI_BL_CTRL0); // ladder enable drop mode

	val = val | (1 << 13) | (1 << 0);
	iowrite32(val, p_hw_fr_base + MIPI_BL_CTRL0);// ladder enable

	LOG(LOG_CRIT, "%d channel write finished.\n", ((auto_write_cfg_t*)cfg)->p_type);
}

void auto_write_ds1_start(void *cfg)
{
	auto_write_cfg_t *a_cfg = NULL;
	uint32_t val = 0;

	if (cfg == NULL) {
		LOG(LOG_ERR, "Error input param");
		return;
	}

	a_cfg = cfg;

	val = ioread32(p_hw_ds1_base + MIPI_BL_CTRL0);
	val = val | 0x2;
	iowrite32(val, p_hw_ds1_base + MIPI_BL_CTRL0);// reset based addr

	val = ioread32(p_hw_ds1_base + MIPI_BL_CTRL0);
	val = val & (~0x2);
	iowrite32(val, p_hw_ds1_base + MIPI_BL_CTRL0);// reset based addr

	val = ioread32(p_hw_ds1_base + MIPI_BL_CTRL0);
	val = val | ((a_cfg->th_enable & 0x1) << 2);
	val = val | ((a_cfg->th_cnt & 0xffff) << 16);
	iowrite32(val, p_hw_ds1_base + MIPI_BL_CTRL0);// reset based addr

	val = ioread32(p_hw_ds1_base + MIPI_BL_CTRL0);

	val = ioread32(p_hw_ds1_base + MIPI_BL_CTRL0);
	val = val | ((a_cfg->drop_enable & 0x1) << 5);
	iowrite32(val, p_hw_ds1_base + MIPI_BL_CTRL0); // ladder enable drop mode

	val = val | (1 << 13) | (1 << 0);
	iowrite32(val, p_hw_ds1_base + MIPI_BL_CTRL0);// ladder enable

	LOG(LOG_CRIT, "%d channel write finished.\n", ((auto_write_cfg_t*)cfg)->p_type);
}

void auto_write_sc0_start(void *cfg)
{
	auto_write_cfg_t *a_cfg = NULL;
	uint32_t val = 0;

	if (cfg == NULL) {
		LOG(LOG_ERR, "Error input param");
		return;
	}

	a_cfg = cfg;

	val = ioread32(p_hw_sc0_base + MIPI_BL_CTRL0);
	val = val | 0x2;
	iowrite32(val, p_hw_sc0_base + MIPI_BL_CTRL0);// reset based addr

	val = ioread32(p_hw_sc0_base + MIPI_BL_CTRL0);
	val = val & (~0x2);
	iowrite32(val, p_hw_sc0_base + MIPI_BL_CTRL0);// reset based addr

	val = ioread32(p_hw_sc0_base + MIPI_BL_CTRL0);

	val = ioread32(p_hw_sc0_base + MIPI_BL_CTRL0);
	val = val | ((a_cfg->th_enable & 0x1) << 2);
	val = val | ((a_cfg->th_cnt & 0xffff) << 16);
	iowrite32(val, p_hw_sc0_base + MIPI_BL_CTRL0);// reset based addr

	val = ioread32(p_hw_sc0_base + MIPI_BL_CTRL0);
	val = val | ((a_cfg->drop_enable & 0x1) << 5);
	iowrite32(val, p_hw_sc0_base + MIPI_BL_CTRL0); // ladder enable drop mode

	val = val | (1 << 13) | (1 << 0);
	iowrite32(val, p_hw_sc0_base + MIPI_BL_CTRL0);// ladder enable

	LOG(LOG_CRIT, "%d channel write finished.\n", ((auto_write_cfg_t*)cfg)->p_type);
}

void auto_write_stop(void *cfg)
{
	auto_write_cfg_t *a_cfg = NULL;
	uint32_t val = 0;

	if (cfg == NULL) {
		LOG(LOG_ERR, "Error input param");
		return;
	}

	a_cfg = cfg;

	val = ioread32(p_hw_fr_base + MIPI_BL_CTRL0);
	val = val | (0x1 << 4);
	iowrite32(val, p_hw_fr_base + MIPI_BL_CTRL0); // ladder stop next frame

	//val = read_reg(base_addr + MIPI_BL_CTRL0);
	//val = val & (~(1 << 13)) & (~(1 << 0));
	//write_reg(val, base_addr + MIPI_BL_CTRL0); // ladder disable
}



