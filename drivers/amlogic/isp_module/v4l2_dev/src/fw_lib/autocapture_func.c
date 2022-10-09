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

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/spinlock_types.h>
#include <linux/wait.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/kfifo.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/interrupt.h>

#include "acamera_fw.h"
#include "acamera.h"
#include "autocapture_fsm.h"
#include "acamera_firmware_settings.h"
#include "system_autowrite.h"
#include "system_am_sc.h"
#include "acamera_autocap.h"
#include "acamera_autocap_api.h"
#include "system_stdlib.h"
#include "acamera_3aalg_preset.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_AUTOCAPTURE
#endif

#define AUTOCAP_DEV_NAME_LEN 20
#define AUTOCAP_DEV_FORMAT "ac_autocap%d"
#define AUTOCAP_DEV_PATH_FORMAT "/dev/" AUTOCAP_DEV_FORMAT
#define AUTOCAP_DEV_PATH_LEN ( AUTOCAP_DEV_NAME_LEN )

#define FW_FR_AUTO_CAP_BASE        0x70000000
#define FW_FR_AUTO_CAP_SIZE        0xBDD800
#define FW_DS1_AUTO_CAP_BASE       0x70C00000
#define FW_DS1_AUTO_CAP_SIZE       0x546000
#define FW_SC0_AUTO_CAP_BASE       0x71200000
#define FW_SC0_AUTO_CAP_SIZE       0x546000

#define ISP_FW_FRAME_BUF_INVALID 0 /* buffer data is invalid  */
#define ISP_FW_FRAME_BUF_VALID 1   /* buffer data is valid  */

#define SUPPORT_CHANNEL  3
#define V4L2_BUFFER_SIZE 6
extern resource_size_t paddr_md;

#if PLATFORM_C308X || PLATFORM_C305X == 1
#define AUTOWRITE_MODULE
#endif
struct autocapture_context {
	struct mutex fops_lock;
	struct miscdevice autocap_dev;
	int dev_minor_id;
	char dev_name[AUTOCAP_DEV_NAME_LEN];
	int dev_opened;

	int fw_id;
	int get_fr_ds;
	int hw_reset;

	int thread_stop;
	//uint32_t timestampe[2];
	int frame_cnt[SUPPORT_CHANNEL];
	tframe_t d_buff[SUPPORT_CHANNEL][V4L2_BUFFER_SIZE];
	resource_size_t idx_buf[SUPPORT_CHANNEL][V4L2_BUFFER_SIZE];
	struct kfifo read_fifo[SUPPORT_CHANNEL];
	isp_v4l2_stream_t *pstream[SUPPORT_CHANNEL];
	struct task_struct *kthread_stream;

	//struct timeval  t_frm;
	struct autocap_image autocap_frame[SUPPORT_CHANNEL];

	auto_write_cfg_t *auto_cap_cfg;

	autocapture_fsm_t *p_fsm;
};

static struct autocapture_context autocapture_context[FIRMWARE_CONTEXT_NUMBER];

struct module_cfg_info pipe_cfg[SUPPORT_CHANNEL];
struct tasklet_struct tasklet_memcpy;
struct timeval normal_fftt;

void fw_auto_cap_init(struct autocapture_context *p_ctx, void *cfg_info);
void fw_auto_cap_deinit(struct autocapture_context *p_ctx);
#ifdef AUTOWRITE_MODULES_V4L2_API
static int autocap_v4l2_stream_copy_thread( void *data );
#endif

extern void cache_flush(uint32_t buf_start, uint32_t buf_size);
static int autocapture_fops_mmap( struct file *f, struct vm_area_struct *vma )
{
	struct autocapture_context *p_ctx = (struct autocapture_context *)f->private_data;
	unsigned long user_buf_len = vma->vm_end - vma->vm_start;
	uint64_t fw_addr_read = 0;
	int rc;

	if(p_ctx->get_fr_ds == GET_FR)
		fw_addr_read = paddr_md;//FW_FR_AUTO_CAP_BASE;
	else if(p_ctx->get_fr_ds == GET_DS1)
		fw_addr_read = FW_DS1_AUTO_CAP_BASE;
	else if(p_ctx->get_fr_ds == GET_SC0)
		fw_addr_read = FW_SC0_AUTO_CAP_BASE;

#ifdef AUTOWRITE_MODULE
	if(p_ctx->get_fr_ds == GET_FR)
		fw_addr_read = autowrite_fr_start_address_read();
	else if(p_ctx->get_fr_ds == GET_DS1)
		fw_addr_read = autowrite_ds1_start_address_read();
	else if(p_ctx->get_fr_ds == GET_SC0)
		fw_addr_read = autowrite_sc0_start_address_read();
#endif

	LOG(LOG_CRIT, "p_ctx->get_fr_ds:%d, fw_addr_read:%llx", p_ctx->get_fr_ds, fw_addr_read);

	/* remap the kernel buffer into the user app address space. */
	rc = remap_pfn_range( vma, vma->vm_start, fw_addr_read >> PAGE_SHIFT, user_buf_len, vma->vm_page_prot );
	if ( rc < 0 ) {
		LOG( LOG_ERR, "remap of autocap failed, return: %d.", rc );
		return rc;
	}

	return 0;
}

static int autocapture_fops_open( struct inode *inode, struct file *f )
{
	int rc;
	int i;
	struct autocapture_context *p_ctx = NULL;
	int minor = iminor( inode );

	for ( i = 0; i < acamera_get_context_number(); i++ ) {
		if ( autocapture_context[i].dev_minor_id == minor ) {
			p_ctx = &autocapture_context[i];
			break;
		}
	}

	if ( !p_ctx ) {
		LOG( LOG_CRIT, "Fatal error, auto contexts is crashed, contents dump:%d",minor );
		for ( i = 0; i < acamera_get_context_number(); i++ ) {
			p_ctx = &autocapture_context[i];
			LOG( LOG_CRIT, "%d): fw_id: %d, minor_id: %d, name: %s, p_fsm: %p.",
				i, p_ctx->fw_id, p_ctx->dev_minor_id, p_ctx->dev_name, p_ctx->p_fsm );
		}
		return -ERESTARTSYS;
	}

	rc = mutex_lock_interruptible( &p_ctx->fops_lock );
	if ( rc ) {
		LOG( LOG_ERR, "access lock failed, rc: %d.", rc );
		return rc;
	}

	if ( p_ctx->dev_opened ) {
		LOG( LOG_ERR, "open failed, already opened." );
		rc = -EBUSY;
	} else {
		p_ctx->dev_opened = 1;
		rc = 0;
		f->private_data = p_ctx;
	}

	mutex_unlock( &p_ctx->fops_lock );

	return rc;
}

static int autocapture_fops_release( struct inode *inode, struct file *f )
{
	int rc = 0;
	struct autocapture_context *p_ctx = (struct autocapture_context *)f->private_data;

	rc = mutex_lock_interruptible( &p_ctx->fops_lock );
	if ( rc ) {
		LOG( LOG_ERR, "Error: lock failed." );
		return rc;
	}

	if ( p_ctx->dev_opened ) {
		p_ctx->dev_opened = 0;
		f->private_data = NULL;
	} else {
		LOG( LOG_CRIT, "Fatal error: wrong state dev_opened: %d.", p_ctx->dev_opened );
		rc = -EINVAL;
	}

	mutex_unlock( &p_ctx->fops_lock );

	return 0;
}

static ssize_t autocapture_fops_read( struct file *file, char __user *buf, size_t count, loff_t *ppos )
{
	int rc = 0;
	struct autocapture_context *p_ctx = (struct autocapture_context *)file->private_data;

	if(p_ctx->hw_reset)
	{
#if 0
		uint32_t time_lose = (normal_fftt.tv_sec*1000000 + normal_fftt.tv_usec)/1000 - p_ctx->autocap_frame[dma_fr].endtime;
		time_lose = time_lose / 33;
		p_ctx->timestampe[1] = time_lose;
		LOG(LOG_CRIT, "RTOS ttff:%dus, Switch Lose frame:%d frames", p_ctx->timestampe[0], p_ctx->timestampe[1]);
		rc = copy_to_user( buf, p_ctx->timestampe, sizeof(uint32_t) * 2 );
		return rc;
#endif
	}

	if(p_ctx->get_fr_ds == GET_FR)
	{
		p_ctx->autocap_frame[dma_fr].format = system_hw_read_32(0x1c0ec) & 0xFF;
		p_ctx->autocap_frame[dma_fr].imagesize = acamera_isp_input_port_frame_width_read(0) << 16;
		p_ctx->autocap_frame[dma_fr].imagesize |= acamera_isp_input_port_frame_height_read(0);

#ifdef AUTOWRITE_MODULE
		p_ctx->autocap_frame[dma_fr].imagebufferstride = autowrite_fr_image_buffer_stride_read();
		p_ctx->autocap_frame[dma_fr].count = autowrite_fr_writer_frame_wcount_read();
		p_ctx->autocap_frame[dma_fr].memory_size = autowrite_fr_writer_memsize_read();
#endif
	}
	else if(p_ctx->get_fr_ds == GET_DS1)
	{
		p_ctx->autocap_frame[dma_ds1].format = system_hw_read_32(0x1c260) & 0xFF;
		p_ctx->autocap_frame[dma_ds1].imagesize = (system_hw_read_32(0x1c264) & 0xFFFF) << 16;
		p_ctx->autocap_frame[dma_ds1].imagesize |= ((system_hw_read_32(0x1c264) >> 16) & 0xFFFF);

#ifdef AUTOWRITE_MODULE
		p_ctx->autocap_frame[dma_ds1].imagebufferstride = autowrite_ds1_image_buffer_stride_read();
		p_ctx->autocap_frame[dma_ds1].count = autowrite_ds1_writer_frame_wcount_read();
		p_ctx->autocap_frame[dma_ds1].memory_size = autowrite_ds1_writer_memsize_read();
#endif
	}

	rc = copy_to_user( buf, &p_ctx->autocap_frame[p_ctx->get_fr_ds], sizeof(struct autocap_image) );
	if ( rc ) {
		LOG( LOG_ERR, "copy_to_user failed, rc: %d.", rc );
	}

	return rc;
}

static ssize_t autocapture_fops_write( struct file *file, const char __user *buf, size_t count, loff_t *ppos )
{
	int rc = 0;
	struct autocapture_context *p_ctx = (struct autocapture_context *)file->private_data;

	rc = copy_from_user( &p_ctx->get_fr_ds, buf, sizeof(int) );
	if ( rc ) {
		LOG( LOG_ERR, "copy_from_user failed, not copied: %d", rc );
	}

	rc = count;

	if(p_ctx->get_fr_ds == 0xAA)
	{
		acamera_isp_isp_global_interrupt_mask_vector_write( 0, ISP_IRQ_DISABLE_ALL_IRQ );
	}
	else if(p_ctx->get_fr_ds == 0x55)
	{
		acamera_isp_isp_global_interrupt_mask_vector_write( 0, ISP_IRQ_MASK_VECTOR );
	}
	else if(p_ctx->get_fr_ds == 0xBB)
	{
#ifdef AUTOWRITE_MODULE
		autowrite_fr_stop();
		autowrite_ds1_stop();
		autowrite_sc0_stop();
#endif
	}

	return rc;
}


uint32_t autocap_get_frame_info(struct autocapture_context *p_ctx, uint32_t type)
{
#ifdef AUTOWRITE_MODULE
    uint32_t realcount = 0;
	uint32_t fps = 30;
#endif

	if(p_ctx->hw_reset)
		return 0;

#ifdef AUTOWRITE_MODULE
	switch(type)
	{
		case dma_fr:
			realcount = autowrite_fr_writer_memsize_read()/autowrite_fr_image_buffer_stride_read();
			p_ctx->autocap_frame[dma_fr].format = system_hw_read_32(0x1c0ec) & 0xFF;
			p_ctx->autocap_frame[dma_fr].imagesize = acamera_isp_input_port_frame_width_read(0) << 16;
			p_ctx->autocap_frame[dma_fr].imagesize |= acamera_isp_input_port_frame_height_read(0);
			p_ctx->autocap_frame[dma_fr].imagebufferstride = autowrite_fr_image_buffer_stride_read();
			p_ctx->autocap_frame[dma_fr].memory_size = autowrite_fr_writer_memsize_read();
			p_ctx->autocap_frame[dma_fr].count = autowrite_fr_writer_frame_wcount_read();
			p_ctx->autocap_frame[dma_fr].s_address = autowrite_fr_start_address_read();
			if(autowrite_fr_drop_mode_status())
			{
				if(autowrite_fr_drop_write_read())
					fps = 30 * autowrite_fr_drop_write_read() / (autowrite_fr_drop_write_read() + autowrite_fr_drop_jump_read());
			}
		    break;
		case dma_ds1:
			realcount = autowrite_ds1_writer_memsize_read()/autowrite_ds1_image_buffer_stride_read();
			p_ctx->autocap_frame[dma_ds1].format = system_hw_read_32(0x1c260) & 0xFF;
			p_ctx->autocap_frame[dma_ds1].imagesize = (system_hw_read_32(0x1c264) & 0xFFFF) << 16;
			p_ctx->autocap_frame[dma_ds1].imagesize |= ((system_hw_read_32(0x1c264) >> 16) & 0xFFFF);
			p_ctx->autocap_frame[dma_ds1].imagebufferstride = autowrite_ds1_image_buffer_stride_read();
			p_ctx->autocap_frame[dma_ds1].memory_size = autowrite_ds1_writer_memsize_read();
			p_ctx->autocap_frame[dma_ds1].count = autowrite_ds1_writer_frame_wcount_read();
			p_ctx->autocap_frame[dma_ds1].s_address = autowrite_ds1_start_address_read();
			if(autowrite_ds1_drop_mode_status())
			{
				if(autowrite_ds1_drop_write_read())
					fps = 30 * autowrite_ds1_drop_write_read() / (autowrite_ds1_drop_write_read() + autowrite_ds1_drop_jump_read());
			}
		    break;
		case dma_sc0:
			realcount = autowrite_sc0_writer_memsize_read()/autowrite_sc0_image_buffer_stride_read();
			p_ctx->autocap_frame[dma_sc0].format = am_sc_get_output_format();
			p_ctx->autocap_frame[dma_sc0].imagesize = am_sc_get_width() << 16;
			p_ctx->autocap_frame[dma_sc0].imagesize |= am_sc_get_height();
			p_ctx->autocap_frame[dma_sc0].imagebufferstride = autowrite_sc0_image_buffer_stride_read();
			p_ctx->autocap_frame[dma_sc0].memory_size = autowrite_sc0_writer_memsize_read();
			p_ctx->autocap_frame[dma_sc0].count = autowrite_sc0_writer_frame_wcount_read();
			p_ctx->autocap_frame[dma_sc0].s_address = autowrite_sc0_start_address_read();
			if ( autowrite_sc0_drop_mode_status() ) {
				if ( autowrite_sc0_drop_write_read() )
					fps = 30 * autowrite_sc0_drop_write_read() / (autowrite_sc0_drop_write_read() + autowrite_sc0_drop_jump_read());
			}
		    break;
		default:
			realcount = 0;
			break;
	}

	p_ctx->autocap_frame[type].fps = fps;

	if(realcount > p_ctx->autocap_frame[type].count)
		p_ctx->autocap_frame[type].realcount = p_ctx->autocap_frame[type].count;
	else
		p_ctx->autocap_frame[type].realcount = realcount;
#endif

	return 0;
}

uint32_t autocap_get_frame_addr(struct autocapture_context *p_ctx, struct frame_info *frm)
{
	uint32_t type = 0;
	uint32_t index = 0;
	uint32_t ret = 0;
	struct frame_info t_frm;

	if(frm == NULL)
	{
		p_ctx->autocap_frame[type].n_address = 0;
		LOG(LOG_CRIT, "input param null");
	 	return -1;
	}

	ret = copy_from_user(&t_frm, frm, sizeof(t_frm));
	if(ret)
	{
		p_ctx->autocap_frame[type].n_address = 0;
		LOG(LOG_CRIT, "input param failed");
		return -1;
	}

	type = t_frm.type;
	index = t_frm.index;

	if(p_ctx->autocap_frame[type].realcount == 0 || p_ctx->autocap_frame[type].count == 0)
	{
		p_ctx->autocap_frame[type].n_address = 0;
		LOG(LOG_CRIT, "Please stop autocap firstly");
	 	return -1;
	}

	if ( (index > p_ctx->autocap_frame[type].realcount) || (type > dma_sc0) )
	{
		p_ctx->autocap_frame[type].n_address = 0;
		LOG(LOG_CRIT, "input param error");
	 	return -1;
	}

	if(p_ctx->autocap_frame[type].realcount >= p_ctx->autocap_frame[type].count)
	{
		if(index == 0)
			p_ctx->autocap_frame[type].n_address = p_ctx->autocap_frame[type].s_address + (p_ctx->autocap_frame[type].realcount) * p_ctx->autocap_frame[type].imagebufferstride;
		else
			p_ctx->autocap_frame[type].n_address = p_ctx->autocap_frame[type].s_address + (index - 1) * p_ctx->autocap_frame[type].imagebufferstride;
	}
	else
	{
		uint32_t real_pos = 0;
		uint32_t pos_offset = p_ctx->autocap_frame[type].count % p_ctx->autocap_frame[type].realcount;

		real_pos = index + pos_offset;
		if(real_pos >= p_ctx->autocap_frame[type].realcount)
			real_pos = real_pos % p_ctx->autocap_frame[type].realcount;

		p_ctx->autocap_frame[type].n_address = p_ctx->autocap_frame[type].s_address + real_pos * p_ctx->autocap_frame[type].imagebufferstride;
	}

	t_frm.framesize = p_ctx->autocap_frame[type].imagebufferstride;
	t_frm.phy_addr = p_ctx->autocap_frame[type].n_address;

	ret = copy_to_user((void *)frm, &t_frm, sizeof(struct frame_info));
	if ( ret ) {
		LOG( LOG_CRIT, "copy_to_user failed, ret: %d.", ret );
		return -1;
	}

	return 0;
}

long autocapture_fops_ioctl (struct file *pfile, unsigned int cmd, unsigned long args)
{
	int rc = 0;
	struct autocapture_context *p_ctx = (struct autocapture_context *)pfile->private_data;

	switch (cmd)
	{
		case IOCTL_GET_FR_INFO:
			rc = copy_to_user((void *)args, &p_ctx->autocap_frame[dma_fr], sizeof(struct autocap_image));
			if ( rc ) {
				LOG( LOG_CRIT, "copy_to_user failed, rc: %d.", rc );
			}
			break;
		case IOCTL_GET_DS1_INFO:
			rc = copy_to_user((void *)args, &p_ctx->autocap_frame[dma_ds1], sizeof(struct autocap_image));
			if ( rc ) {
				 LOG( LOG_CRIT, "copy_to_user failed, rc: %d.", rc );
			}
			break;
		case IOCTL_GET_SC0_INFO:
			rc = copy_to_user((void *)args, &p_ctx->autocap_frame[dma_sc0], sizeof(struct autocap_image));
			if ( rc ) {
				 LOG( LOG_CRIT, "copy_to_user failed, rc: %d.", rc );
			}
			break;
#ifdef AUTOWRITE_MODULE
		case IOCTL_STOP_AUTO:
			autowrite_fr_stop();
			autowrite_ds1_stop();
			autowrite_sc0_stop();
			autocap_get_frame_info(p_ctx, dma_fr);
			autocap_get_frame_info(p_ctx, dma_ds1);
			autocap_get_frame_info(p_ctx, dma_sc0);
			break;
		case IOCTL_STOP_AUTO_FR:
			autowrite_fr_stop();
			autocap_get_frame_info(p_ctx, dma_fr);
		    p_ctx->get_fr_ds = GET_FR;
			break;
		case IOCTL_STOP_AUTO_DS1:
			autowrite_ds1_stop();
			autocap_get_frame_info(p_ctx, dma_ds1);
		    p_ctx->get_fr_ds = GET_DS1;
			break;
		case IOCTL_STOP_AUTO_SC0:
			autowrite_sc0_stop();
			autocap_get_frame_info(p_ctx, dma_sc0);
		    p_ctx->get_fr_ds = GET_SC0;
			break;
		case IOCTL_DBGPATH_INFO:
			autocap_get_frame_info(p_ctx, dma_fr);
			autocap_get_frame_info(p_ctx, dma_ds1);
			autocap_get_frame_info(p_ctx, dma_sc0);
			break;
		case IOCTL_GET_FRAME_INFO:
			autocap_get_frame_addr(p_ctx, (void *)args);
			break;
		case IOCTL_GET_FR_RTL_INFO:
			p_ctx->get_fr_ds = GET_FR;
			autocap_get_frame_info(p_ctx, dma_fr);
			rc = copy_to_user((void *)args, &p_ctx->autocap_frame[dma_fr], sizeof(struct autocap_image));
			if ( rc ) {
                 LOG( LOG_CRIT, "copy_to_user failed, rc: %d.", rc );
		}
			break;
		case IOCTL_GET_DS1_RTL_INFO:
			p_ctx->get_fr_ds = GET_DS1;
			autocap_get_frame_info(p_ctx, dma_ds1);
			rc = copy_to_user((void *)args, &p_ctx->autocap_frame[dma_ds1], sizeof(struct autocap_image));
			if ( rc ) {
				 LOG( LOG_CRIT, "copy_to_user failed, rc: %d.", rc );
			}
			break;
		case IOCTL_GET_SC0_RTL_INFO:
			p_ctx->get_fr_ds = GET_SC0;
			autocap_get_frame_info(p_ctx, dma_sc0);
			rc = copy_to_user((void *)args, &p_ctx->autocap_frame[dma_sc0], sizeof(struct autocap_image));
			if ( rc ) {
				 LOG( LOG_CRIT, "copy_to_user failed, rc: %d.", rc );
			}
			break;
#endif
	    default:
		    LOG(LOG_CRIT, "invalid cmd");
		    break;
	}

	return rc;
}

void autocap_do_tasklet( unsigned long data )
{
	struct autocapture_context *p_ctx;
	p_ctx = (struct autocapture_context *)data;

	acamera_alg_preset_t *p_pst;
	char *reserve_virt_addr = phys_to_virt(autowrite_ds1_start_address_read() + autowrite_ds1_writer_memsize_read());
	p_pst = (acamera_alg_preset_t *)reserve_virt_addr;
	//p_ctx->timestampe[0] = p_pst->other_param.first_frm_timestamp;

	LOG(LOG_CRIT, "Autocap get Pong Buff:%x", p_ctx->autocap_frame[dma_ds1].realcount);
	uint32_t addr_pong = autowrite_ds1_pong_start();
	if( autowrite_ds1_ping_start() > autowrite_ds1_pong_start() )
		addr_pong = autowrite_ds1_ping_start();

	uint32_t addr = p_ctx->autocap_frame[dma_ds1].s_address + (p_ctx->autocap_frame[dma_ds1].realcount) * p_ctx->autocap_frame[dma_ds1].imagebufferstride;
	memcpy(phys_to_virt(addr), phys_to_virt(addr_pong), p_ctx->autocap_frame[dma_ds1].imagebufferstride);
}

static struct file_operations autocapture_mgr_fops = {
	.owner = THIS_MODULE,
	.open = autocapture_fops_open,
	.release = autocapture_fops_release,
	.read = autocapture_fops_read,
	.write = autocapture_fops_write,
	.unlocked_ioctl = autocapture_fops_ioctl,
	.llseek = noop_llseek,
	.mmap = autocapture_fops_mmap,
};

void autocapture_initialize( autocapture_fsm_t *p_fsm )
{
	int rc;
	uint32_t fw_id = p_fsm->cmn.ctx_id;
	struct miscdevice *p_dev;
	struct autocapture_context *p_ctx;

	if ( fw_id >= acamera_get_context_number() ) {
		LOG( LOG_CRIT, "Fatal error: Invalid FW context ID: %d, max is: %d", fw_id, acamera_get_context_number() - 1 );
		return;
	}

	p_ctx = &( autocapture_context[fw_id] );
	memset( p_ctx, 0, sizeof( *p_ctx ) );
	p_dev = &p_ctx->autocap_dev;
	snprintf( p_ctx->dev_name, AUTOCAP_DEV_NAME_LEN, AUTOCAP_DEV_FORMAT, fw_id );
	p_dev->name = p_ctx->dev_name;
	p_dev->minor = MISC_DYNAMIC_MINOR,
	p_dev->fops = &autocapture_mgr_fops,

	rc = misc_register( p_dev );
	if ( rc ) {
		LOG( LOG_ERR, "init failed, error: register autocap device failed, ret: %d.", rc );
		return;
	}

	p_ctx->fw_id = fw_id;
	p_ctx->dev_minor_id = p_dev->minor;
	p_ctx->p_fsm = p_fsm;
	p_ctx->dev_opened = 0;
	p_ctx->thread_stop = 0;
	p_ctx->hw_reset = 0;

	mutex_init( &p_ctx->fops_lock );

#ifdef AUTOWRITE_MODULE
	auto_cap_init();

	tasklet_init( &tasklet_memcpy, autocap_do_tasklet, (unsigned long)p_ctx );
#endif

#ifdef LINUX_AUTOWRITE_MODULE_TEST
	struct module_cfg_info m_cfg_fr;
	struct module_cfg_info m_cfg_ds1;
	struct module_cfg_info m_cfg_sc0;

	memset(&m_cfg_fr, 0, sizeof(m_cfg_fr));
	memset(&m_cfg_ds1, 0, sizeof(m_cfg_ds1));
	memset(&m_cfg_sc0, 0, sizeof(m_cfg_sc0));

	LOG( LOG_CRIT, "zgs autocapture_initialize: %d", __LINE__);

	auto_cap_init();

	m_cfg_fr.p_type = dma_fr;
	m_cfg_fr.planes = 1;
	m_cfg_fr.frame_size0 = (1920 * 1080 * 3);
	fw_auto_cap_init(p_ctx, (void *)&m_cfg_fr);

	m_cfg_ds1.p_type = dma_ds1;
	m_cfg_ds1.planes = 1;
	m_cfg_ds1.frame_size0 = (1280 * 720 * 3);
	fw_auto_cap_init(p_ctx, (void *)&m_cfg_ds1);

	m_cfg_sc0.p_type = dma_sc0;
	m_cfg_sc0.planes = 1;
	m_cfg_sc0.frame_size0 = (1280 * 720 * 3);
	fw_auto_cap_init(p_ctx, (void *)&m_cfg_sc0);
#endif

#ifdef AUTOWRITE_MODULES_V4L2_API
	int kfifo_ret;
	int i = 0;

	//p_ctx->get_fr_ds = AUTOCAP_MODE0;
	if(p_ctx->get_fr_ds == AUTOCAP_MODE0)
	{
	    for(i = 0; i < SUPPORT_CHANNEL; i++)
	    {
	        p_ctx->frame_cnt[i] = 0;
	        kfifo_ret = kfifo_alloc(&p_ctx->read_fifo[i], PAGE_SIZE, GFP_KERNEL);
	        if (kfifo_ret) {
		         pr_info("alloc adapter fifo failed.\n");
		        return;
	        }
	    }

	    acamera_isp_input_port_mode_request_write( 0, ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_SAFE_START );
	    p_ctx->kthread_stream = kthread_run( autocap_v4l2_stream_copy_thread, p_ctx, "autocap_stream" );
	}
#endif
	return;
}

void autocapture_deinit( autocapture_fsm_ptr_t p_fsm )
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 3, 0))
	int rc;
#endif
	int i;
	uint32_t fw_id = p_fsm->cmn.ctx_id;
	struct autocapture_context *p_ctx = NULL;
	struct miscdevice *p_dev = NULL;

	p_ctx = &( autocapture_context[fw_id] );
	if ( p_ctx->fw_id != fw_id ) {
		LOG( LOG_CRIT, "Error: ctx_id not match, fsm fw_id: %d, ctx_id: %d.", fw_id, p_ctx->fw_id );
		return;
	}

	p_dev = &p_ctx->autocap_dev;
	if ( !p_dev->name ) {
		LOG( LOG_CRIT, "skip sbuf[%d] deregister due to NULL name", fw_id );
		return;
	}

	p_ctx->thread_stop = 1;

	if(p_ctx->kthread_stream)
	    kthread_stop( p_ctx->kthread_stream );

	for(i = 0 ; i < SUPPORT_CHANNEL; i++)
	{
	    if(&p_ctx->read_fifo[i])
	        kfifo_free(&p_ctx->read_fifo[i]);
	}

#ifdef AUTOWRITE_MODULE
	auto_cap_deinit();
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0))
	misc_deregister( p_dev );
#else
	rc = misc_deregister( p_dev );
	if ( rc ) {
		LOG( LOG_ERR, "deregister autocap dev '%s' failed, ret: %d.", p_dev->name, rc );
	} else {
		LOG( LOG_INFO, "deregister autocap dev '%s' ok.", p_dev->name );
	}
#endif

	return;
}

void autocapture_hwreset(autocapture_fsm_ptr_t p_fsm )
{
#ifdef AUTOWRITE_MODULE
	uint32_t fw_id = p_fsm->cmn.ctx_id;
	struct autocapture_context *p_ctx = NULL;

	p_ctx = &( autocapture_context[fw_id] );

	if( p_ctx->hw_reset )
		return;

	system_hw_write_32(0x9c, 0);

	autocap_get_frame_info(p_ctx, dma_fr);
	autocap_get_frame_info(p_ctx, dma_ds1);
	autocap_get_frame_info(p_ctx, dma_sc0);

	system_hw_write_32(0x1C260, 0);
	system_hw_write_32(0x1C2B8, 0);
	system_hw_write_32(0x34220, 0);
	system_hw_write_32(0x34278, 0);

	p_ctx->hw_reset = 1;

	if(p_ctx->autocap_frame[dma_ds1].realcount && (p_ctx->autocap_frame[dma_ds1].realcount >= p_ctx->autocap_frame[dma_ds1].count))
	{
		tasklet_schedule(&tasklet_memcpy);
	}

	mdelay(5);
	autowrite_path_reset();
#endif

	return;
}

#ifdef AUTOWRITE_MODULES_V4L2_API
static int autocap_v4l2_stream_copy_thread( void *data )
{
	int read_count = 0;
	int res = 0;
	tframe_t f_buff;
	struct autocapture_context *p_ctx = (struct autocapture_context *)data;

	LOG( LOG_CRIT, "autocap_v4l2_stream_copy_thread enter" );
	acamera_isp_isp_global_interrupt_mask_vector_write( 0, ISP_IRQ_DISABLE_ALL_IRQ );

	while(!p_ctx->thread_stop)
	{
		msleep(100);
		if(kfifo_len(&p_ctx->read_fifo[0]) == V4L2_BUFFER_SIZE * 8)
			break;
	}

	msleep(100);

	LOG( LOG_CRIT, "autocap_v4l2_stream_copy_thread enter" );

	while ( !kthread_should_stop() ) {

		 if(p_ctx->pstream[dma_fr] && p_ctx->pstream[dma_fr]->stream_started)
		 {
			 res = autocap_pullbuf(p_ctx->fw_id, dma_fr, f_buff);
			 if(res == 0)
				continue;

#ifdef AUTOWRITE_MODULE
	         if(read_count ++ > autowrite_fr_writer_frame_wcount_read())
#else
		     if(read_count ++ > 600)
#endif
		 	    break;
		}

		if ( p_ctx->pstream[dma_ds1] && p_ctx->pstream[dma_ds1]->stream_started ) {
			 res = autocap_pullbuf(p_ctx->fw_id, dma_ds1, f_buff);
			 if(res == 0)
				continue;
		}

		if ( p_ctx->pstream[dma_sc0] && p_ctx->pstream[dma_sc0]->stream_started ) {
			 res = autocap_pullbuf(p_ctx->fw_id, dma_sc0, f_buff);
			 if(res == 0)
				continue;
		}

		 msleep(1);
		 //pr_info("read_count :%d, %d, %d", read_count, p_ctx->pstream[0]->stream_started,res);
	}

	LOG( LOG_CRIT, "autocap_v4l2_stream_copy_thread stop" );
	acamera_isp_isp_global_interrupt_mask_vector_write( 0, ISP_IRQ_MASK_VECTOR );
	p_ctx->kthread_stream = NULL;
	p_ctx->get_fr_ds = 0;
	return 0;
}

uint32_t autocap_pullbuf(int ctx_id, int d_type, tframe_t f_buff)
{
	int kfifo_ret = 0;
	resource_size_t p_buff;

	struct autocapture_context *p_ctx = NULL;
	p_ctx = &( autocapture_context[ctx_id] );

	if(kfifo_len(&p_ctx->read_fifo[d_type]) > 0)
	{
		kfifo_ret = kfifo_out(&p_ctx->read_fifo[d_type], &p_buff, sizeof(resource_size_t));
		if (kfifo_ret <= 0) {
			pr_info("kfifo out failed.%d\n", kfifo_len(&p_ctx->read_fifo[d_type]));
			return kfifo_ret;
		}

		//pr_info("\n kfifo out success, p_buff :%lld, %x.\n", p_buff, p_ctx->d_buff[d_type][p_buff].primary.address);
		p_ctx->pstream[d_type]->frame_mgr.frame_buffer.state = ISP_FW_FRAME_BUF_INVALID;
		if(d_type == dma_fr)
		{
		    system_memcpy(phys_to_virt(p_ctx->d_buff[d_type][p_buff].primary.address), phys_to_virt(FW_FR_AUTO_CAP_BASE),
			    p_ctx->d_buff[d_type][p_buff].primary.size+p_ctx->d_buff[d_type][p_buff].secondary.size);

		    p_ctx->p_fsm->p_fsm_mgr->p_ctx->settings.callback_fr(ctx_id, &p_ctx->d_buff[d_type][p_buff], NULL);
		}
		else if(d_type == dma_ds1)
		{
		    system_memcpy(phys_to_virt(p_ctx->d_buff[d_type][p_buff].primary.address), phys_to_virt(FW_DS1_AUTO_CAP_BASE), \
			    p_ctx->d_buff[d_type][p_buff].primary.size+p_ctx->d_buff[d_type][p_buff].secondary.size);
			p_ctx->p_fsm->p_fsm_mgr->p_ctx->settings.callback_ds1(ctx_id, &p_ctx->d_buff[d_type][p_buff], NULL);
		}
		else if(d_type == dma_sc0)
		{
		    system_memcpy(phys_to_virt(p_ctx->d_buff[d_type][p_buff].primary.address), phys_to_virt(FW_SC0_AUTO_CAP_BASE), \
			    p_ctx->d_buff[d_type][p_buff].primary.size+p_ctx->d_buff[d_type][p_buff].secondary.size);

		    p_ctx->p_fsm->p_fsm_mgr->p_ctx->settings.callback_sc0(ctx_id, &p_ctx->d_buff[d_type][p_buff], NULL);
		}
	}

	return kfifo_ret;
}

uint32_t autocap_pushbuf(int ctx_id, int d_type, tframe_t f_buff, isp_v4l2_stream_t *pstream)
{
	int kfifo_ret = 0;
	int frame_cnt = 0;
	struct autocapture_context *p_ctx = NULL;
	p_ctx = &( autocapture_context[ctx_id] );
	frame_cnt = p_ctx->frame_cnt[d_type];
	p_ctx->pstream[d_type] = pstream;

	acamera_isp_isp_global_interrupt_mask_vector_write( 0, ISP_IRQ_DISABLE_ALL_IRQ );

	system_memcpy(&p_ctx->d_buff[d_type][frame_cnt], &f_buff, sizeof(tframe_t));
	p_ctx->idx_buf[d_type][frame_cnt] = frame_cnt;
	if(!kfifo_is_full(&p_ctx->read_fifo[d_type]))
	{
		kfifo_ret = kfifo_in(&p_ctx->read_fifo[d_type], &p_ctx->idx_buf[d_type][frame_cnt], sizeof(resource_size_t));
		if (kfifo_ret == 0) {
			pr_info("kfifo in failed,%d.\n",ctx_id);
			return kfifo_ret;
		}
	}

	//pr_info("\n kfifo in success, cnt=%x, %x\n",p_ctx->frame_cnt[d_type], p_ctx->d_buff[d_type][frame_cnt].primary.address);

	p_ctx->frame_cnt[d_type] ++;
	if(p_ctx->frame_cnt[d_type] == V4L2_BUFFER_SIZE)
		p_ctx->frame_cnt[d_type] = 0;

	return 0;
}
#endif

uint32_t autocap_get_mode(uint32_t ctx_id)
{
	return autocapture_context[ctx_id].get_fr_ds;
}

/**********************************/

void autocap_set_new_param(struct module_cfg_info *m_cfg)
{
	LOG(LOG_DEBUG, "type:%d, %x, %x", m_cfg->p_type,m_cfg->frame_buffer_start0, m_cfg->frame_size0);

	pipe_cfg[m_cfg->p_type].frame_buffer_start0 = m_cfg->frame_buffer_start0;
	pipe_cfg[m_cfg->p_type].frame_buffer_start1 = m_cfg->frame_buffer_start1;
	pipe_cfg[m_cfg->p_type].frame_size0 = m_cfg->frame_size0;
	pipe_cfg[m_cfg->p_type].frame_size1 = m_cfg->frame_size1;
}

void fw_auto_cap_init(struct autocapture_context *p_ctx, void *cfg_info)
{
	auto_write_cfg_t *a_cfg = NULL;
	struct module_cfg_info *m_cfg = (struct module_cfg_info *)cfg_info;

	a_cfg = vmalloc(sizeof(*p_ctx->auto_cap_cfg));
	if (a_cfg == NULL) {
		LOG(LOG_ERR, "Failed to alloc mem");
		return;
	}

	memset(a_cfg, 0, sizeof(*a_cfg));

	a_cfg->p_type = m_cfg->p_type;
	a_cfg->frame_size0 = m_cfg->frame_size0;
	a_cfg->frame_size1 = m_cfg->frame_size1;

	switch (a_cfg->p_type) {
	case dma_fr:
	    if (m_cfg->planes == 1) {
		    a_cfg->frame_buffer_start0 = FW_FR_AUTO_CAP_BASE;
		    a_cfg->frame_buffer_size0 = FW_FR_AUTO_CAP_SIZE;
		    a_cfg->frame_buffer_start1 = 0x00000000;
		    a_cfg->frame_buffer_size1 = 0x0;
	    } else if (m_cfg->planes == 2) {
			a_cfg->frame_buffer_start0 = FW_FR_AUTO_CAP_BASE;
			a_cfg->frame_buffer_size0 = FW_FR_AUTO_CAP_SIZE / 2;
			a_cfg->frame_buffer_start1 = FW_FR_AUTO_CAP_BASE + (FW_FR_AUTO_CAP_SIZE / 2);
			a_cfg->frame_buffer_size1 = FW_FR_AUTO_CAP_SIZE / 2;
		}
		break;
	case dma_ds1:
		if ( m_cfg->planes ==1) {
			a_cfg->frame_buffer_start0 = FW_DS1_AUTO_CAP_BASE;
			a_cfg->frame_buffer_size0 = FW_DS1_AUTO_CAP_SIZE;
			a_cfg->frame_buffer_start1 = 0x00000000;
			a_cfg->frame_buffer_size1 = 0x0;
		} else if (m_cfg->planes == 2) {
			a_cfg->frame_buffer_start0 = FW_DS1_AUTO_CAP_BASE;
			a_cfg->frame_buffer_size0 = FW_DS1_AUTO_CAP_SIZE / 2;
			a_cfg->frame_buffer_start1 = FW_DS1_AUTO_CAP_BASE + FW_DS1_AUTO_CAP_SIZE / 2;
			a_cfg->frame_buffer_size1 = FW_DS1_AUTO_CAP_SIZE / 2;
		}
		break;
	case dma_sc0:
		if ( m_cfg->planes ==1) {
			a_cfg->frame_buffer_start0 = FW_SC0_AUTO_CAP_BASE;
			a_cfg->frame_buffer_size0 = FW_SC0_AUTO_CAP_BASE;
			a_cfg->frame_buffer_start1 = 0x00000000;
			a_cfg->frame_buffer_size1 = 0x0;
		} else if (m_cfg->planes == 2) {
			a_cfg->frame_buffer_start0 = FW_DS1_AUTO_CAP_BASE;
			a_cfg->frame_buffer_size0 = FW_DS1_AUTO_CAP_SIZE / 2;
			a_cfg->frame_buffer_start1 = FW_DS1_AUTO_CAP_BASE + FW_DS1_AUTO_CAP_SIZE / 2;
			a_cfg->frame_buffer_size1 = FW_DS1_AUTO_CAP_SIZE / 2;
		}
		break;
	default:
		vfree(a_cfg);
		LOG(LOG_ERR, "Error path type");
		return;
	}

	if (a_cfg->p_type == dma_fr) {
		auto_write_fr_init(a_cfg);
	} else if (a_cfg->p_type == dma_ds1) {
		auto_write_ds1_init(a_cfg);
	} else if (a_cfg->p_type == dma_sc0) {
		auto_write_sc0_init(a_cfg);
	} else {
		LOG(LOG_CRIT, "can't support autocapture type\n");
	}

	p_ctx->auto_cap_cfg = a_cfg;
}

void fw_auto_cap_deinit(struct autocapture_context *p_ctx)
{
	if (p_ctx->auto_cap_cfg != NULL) {
		vfree(p_ctx->auto_cap_cfg);
		p_ctx->auto_cap_cfg = NULL;
	}
}

