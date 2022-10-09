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
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/kfifo.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/compat.h>
#include "acamera_logger.h"
#include "system_stdlib.h"
#include "acamera_command_api.h"
#include "acamera_ctrl_channel.h"
#include "acamera_3aalg_preset.h"

#define CTRL_CHANNEL_FIFO_INPUT_SIZE ( 4 * 1024 )
#define CTRL_CHANNEL_FIFO_OUTPUT_SIZE ( 8 * 1024 )

struct ctrl_channel_dev_context {
    uint8_t dev_inited;
    int dev_minor_id;
    char *dev_name;
    int dev_opened;

    struct miscdevice ctrl_dev;
    struct mutex fops_lock;

    struct kfifo ctrl_kfifo_in;
    struct kfifo ctrl_kfifo_out;

    struct ctrl_cmd_item cmd_item;

	isp_ae_preset_t ae_param;
	isp_awb_preset_t awb_param;
	isp_gamma_preset_t gamma_param;
	isp_iridix_preset_t iridix_param;
};

static struct ctrl_channel_dev_context ctrl_channel_ctx;

static int ctrl_channel_fops_open( struct inode *inode, struct file *f )
{
    int rc;
    struct ctrl_channel_dev_context *p_ctx = &ctrl_channel_ctx;
    int minor = iminor( inode );

    LOG( LOG_DEBUG, "client is opening..., minor: %d.", minor );

    if ( minor != p_ctx->dev_minor_id ) {
        LOG( LOG_CRIT, "Not matched ID, minor_id: %d, exptect: %d(dev name: %s)", minor, p_ctx->dev_minor_id, p_ctx->dev_name );
        return -ERESTARTSYS;
    }

    rc = mutex_lock_interruptible( &p_ctx->fops_lock );
    if ( rc ) {
        LOG( LOG_ERR, "Error: lock failed of dev: %s.", p_ctx->dev_name );
        goto lock_failure;
    }

    if ( p_ctx->dev_opened ) {
        LOG( LOG_ERR, "open(%s) failed, already opened.", p_ctx->dev_name );
        rc = -EBUSY;
    } else {
        p_ctx->dev_opened = 1;
        rc = 0;
        f->private_data = p_ctx;
    }

    mutex_unlock( &p_ctx->fops_lock );

lock_failure:
    return rc;
}

static int ctrl_channel_fops_release( struct inode *inode, struct file *f )
{
    int rc;
    struct ctrl_channel_dev_context *p_ctx = (struct ctrl_channel_dev_context *)f->private_data;

    if ( p_ctx != &ctrl_channel_ctx ) {
        LOG( LOG_ERR, "Inalid paramter: %p, exptected: %p.", p_ctx, &ctrl_channel_ctx );
        return -EINVAL;
    }

    rc = mutex_lock_interruptible( &p_ctx->fops_lock );
    if ( rc ) {
        LOG( LOG_ERR, "Error: lock failed of dev: %s.", p_ctx->dev_name );
        return rc;
    }

    if ( p_ctx->dev_opened ) {
        p_ctx->dev_opened = 0;
        f->private_data = NULL;
        kfifo_reset( &p_ctx->ctrl_kfifo_in );
        kfifo_reset( &p_ctx->ctrl_kfifo_out );
        LOG( LOG_DEBUG, "close(%s) succeed, private_data: %p.", p_ctx->dev_name, p_ctx );
    } else {
        LOG( LOG_CRIT, "Fatal error: wrong state of dev: %s, dev_opened: %d.", p_ctx->dev_name, p_ctx->dev_opened );
        rc = -EINVAL;
    }

    mutex_unlock( &p_ctx->fops_lock );

    return 0;
}

static ssize_t ctrl_channel_fops_write( struct file *file, const char __user *buf, size_t count, loff_t *ppos )
{
    int rc;
    unsigned int copied;
    struct ctrl_channel_dev_context *p_ctx = (struct ctrl_channel_dev_context *)file->private_data;

    if ( p_ctx != &ctrl_channel_ctx ) {
        LOG( LOG_ERR, "Inalid paramter: %p, exptected: %p.", p_ctx, &ctrl_channel_ctx );
        return -EINVAL;
    }

    if ( mutex_lock_interruptible( &p_ctx->fops_lock ) ) {
        LOG( LOG_CRIT, "Fatal error: access lock failed." );
        return -ERESTARTSYS;
    }

    rc = kfifo_from_user( &p_ctx->ctrl_kfifo_in, buf, count, &copied );

    mutex_unlock( &p_ctx->fops_lock );

    LOG( LOG_DEBUG, "wake up reader." );

    return rc ? rc : copied;
}

static ssize_t ctrl_channel_fops_read( struct file *file, char __user *buf, size_t count, loff_t *ppos )
{
    int rc = 0;
    unsigned int copied = 0;
    struct ctrl_channel_dev_context *p_ctx = (struct ctrl_channel_dev_context *)file->private_data;

    if ( p_ctx != &ctrl_channel_ctx ) {
        LOG( LOG_ERR, "Inalid paramter: %p, exptected: %p.", p_ctx, &ctrl_channel_ctx );
        return -EINVAL;
    }

    if ( mutex_lock_interruptible( &p_ctx->fops_lock ) ) {
        LOG( LOG_CRIT, "Fatal error: access lock failed." );
        return -ERESTARTSYS;
    }

    if ( !kfifo_is_empty( &p_ctx->ctrl_kfifo_out ) ) {
        rc = kfifo_to_user( &p_ctx->ctrl_kfifo_out, buf, count, &copied );
    }

    mutex_unlock( &p_ctx->fops_lock );

    return rc ? rc : copied;
}

long ctrl_channel_fops_ioctl (struct file *pfile, unsigned int cmd, unsigned long args)
{
    int rc;

	switch (cmd)
	{
		case IOCTL_3AALG_AEPRE:
			rc = copy_to_user((void *)args, &ctrl_channel_ctx.ae_param, sizeof(isp_ae_preset_t));
			if ( rc ) {
                 LOG( LOG_CRIT, "ae copy_to_user failed, rc: %d.", rc );
            }
			break;
		case IOCTL_3AALG_AWBPRE:
			rc = copy_to_user((void *)args, &ctrl_channel_ctx.awb_param, sizeof(isp_awb_preset_t));
			if ( rc ) {
                 LOG( LOG_CRIT, "awb copy_to_user failed, rc: %d.", rc );
            }			
			break;
		case IOCTL_3AALG_GAMMAPRE:
			rc = copy_to_user((void *)args, &ctrl_channel_ctx.gamma_param, sizeof(isp_gamma_preset_t));
			if ( rc ) {
                 LOG( LOG_CRIT, "gamma copy_to_user failed, rc: %d.", rc );
            }
			break;
		case IOCTL_3AALG_IRIDIXPRE:
			rc = copy_to_user((void *)args, &ctrl_channel_ctx.iridix_param, sizeof(isp_iridix_preset_t));
			if ( rc ) {
                 LOG( LOG_CRIT, "iridix copy_to_user failed, rc: %d.", rc );
            }
			break;
		default:
		    LOG(LOG_CRIT, "invalid cmd");
		    break;
	}

    return 0;
}

#ifdef CONFIG_COMPAT
long ctrl_compat_ioctl (struct file *pfile, unsigned int cmd, unsigned long args)
{
	int rc;
	void  *arg64 = compat_ptr(args);

	switch (cmd)
	{
		case IOCTL_3AALG_AEPRE:
			rc = copy_to_user((void *)arg64, &ctrl_channel_ctx.ae_param, sizeof(isp_ae_preset_t));
			if ( rc ) {
                 LOG( LOG_CRIT, "ae copy_to_user failed, rc: %d.", rc );
            }
			break;
		case IOCTL_3AALG_AWBPRE:
			rc = copy_to_user((void *)arg64, &ctrl_channel_ctx.awb_param, sizeof(isp_awb_preset_t));
			if ( rc ) {
                 LOG( LOG_CRIT, "awb copy_to_user failed, rc: %d.", rc );
            }
			break;
		case IOCTL_3AALG_GAMMAPRE:
			rc = copy_to_user((void *)arg64, &ctrl_channel_ctx.gamma_param, sizeof(isp_gamma_preset_t));
			if ( rc ) {
                 LOG( LOG_CRIT, "gamma copy_to_user failed, rc: %d.", rc );
            }
			break;
		case IOCTL_3AALG_IRIDIXPRE:
			rc = copy_to_user((void *)arg64, &ctrl_channel_ctx.iridix_param, sizeof(isp_iridix_preset_t));
			if ( rc ) {
                 LOG( LOG_CRIT, "iridix copy_to_user failed, rc: %d.", rc );
            }
			break;
		default:
		    LOG(LOG_CRIT, "invalid cmd");
		    break;
	}

    return 0;
}
#endif

int ctrl_channel_3aalg_param_init(isp_ae_preset_t *ae, isp_awb_preset_t *awb, isp_gamma_preset_t *gamma, isp_iridix_preset_t *iridix)
{
    system_memcpy(&ctrl_channel_ctx.ae_param, ae, sizeof(isp_ae_preset_t));
	system_memcpy(&ctrl_channel_ctx.awb_param, awb, sizeof(isp_awb_preset_t));
	system_memcpy(&ctrl_channel_ctx.gamma_param, gamma, sizeof(isp_gamma_preset_t));
	system_memcpy(&ctrl_channel_ctx.iridix_param, iridix, sizeof(isp_iridix_preset_t));

	LOG(LOG_INFO, "ctrl_channel_3aalg_param_init: %d,%d,%d,%d,%d",ctrl_channel_ctx.ae_param.exposure_log2,\
		ctrl_channel_ctx.ae_param.integrator,ctrl_channel_ctx.ae_param.error_log2,ctrl_channel_ctx.iridix_param.strength_target,ctrl_channel_ctx.iridix_param.iridix_global_DG);

	return 0;
}

static struct file_operations isp_fops = {
    .owner = THIS_MODULE,
    .open = ctrl_channel_fops_open,
    .release = ctrl_channel_fops_release,
    .read = ctrl_channel_fops_read,
    .write = ctrl_channel_fops_write,
    .unlocked_ioctl = ctrl_channel_fops_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ctrl_compat_ioctl,
#endif
    .llseek = noop_llseek,
};

#if defined( CTRL_CHANNEL_BIDIRECTION )
int ctrl_channel_read( char *data, int size )
{
    int rc = 0;

    if ( !ctrl_channel_ctx.dev_inited ) {
        LOG( LOG_ERR, "dev is not inited, failed to read." );
        return -1;
    }

    mutex_lock( &ctrl_channel_ctx.fops_lock );

    if ( !kfifo_is_empty( &ctrl_channel_ctx.ctrl_kfifo_in ) ) {
        rc = kfifo_out( &ctrl_channel_ctx.ctrl_kfifo_in, data, size );
    }

    mutex_unlock( &ctrl_channel_ctx.fops_lock );

    return rc;
}
#endif

static int ctrl_channel_write( const struct ctrl_cmd_item *p_cmd, const void *data, uint32_t data_size )
{
    int rc;

    if ( !ctrl_channel_ctx.dev_inited ) {
        LOG( LOG_ERR, "dev is not inited, failed to write." );
        return -1;
    }

    mutex_lock( &ctrl_channel_ctx.fops_lock );

    rc = kfifo_in( &ctrl_channel_ctx.ctrl_kfifo_out, p_cmd, sizeof( struct ctrl_cmd_item ) );
    if ( data )
        kfifo_in( &ctrl_channel_ctx.ctrl_kfifo_out, data, data_size );

    mutex_unlock( &ctrl_channel_ctx.fops_lock );

    return rc;
}

int ctrl_channel_init( void )
{
    int rc;

    memset( &ctrl_channel_ctx, 0, sizeof( ctrl_channel_ctx ) );
    ctrl_channel_ctx.ctrl_dev.name = CTRL_CHANNEL_DEV_NAME;
    ctrl_channel_ctx.dev_name = CTRL_CHANNEL_DEV_NAME;
    ctrl_channel_ctx.ctrl_dev.minor = MISC_DYNAMIC_MINOR;
    ctrl_channel_ctx.ctrl_dev.fops = &isp_fops;

    rc = misc_register( &ctrl_channel_ctx.ctrl_dev );
    if ( rc ) {
        LOG( LOG_ERR, "Error: register ISP ctrl channel device failed, ret: %d.", rc );
        return rc;
    }

    ctrl_channel_ctx.dev_minor_id = ctrl_channel_ctx.ctrl_dev.minor;
    mutex_init( &ctrl_channel_ctx.fops_lock );

    rc = kfifo_alloc( &ctrl_channel_ctx.ctrl_kfifo_in, CTRL_CHANNEL_FIFO_INPUT_SIZE, GFP_KERNEL );
    if ( rc ) {
        LOG( LOG_ERR, "Error: kfifo_in alloc failed, ret: %d.", rc );
        goto failed_kfifo_in_alloc;
    }

    rc = kfifo_alloc( &ctrl_channel_ctx.ctrl_kfifo_out, CTRL_CHANNEL_FIFO_OUTPUT_SIZE, GFP_KERNEL );
    if ( rc ) {
        LOG( LOG_ERR, "Error: kfifo_out alloc failed, ret: %d.", rc );
        goto failed_kfifo_out_alloc;
    }

    ctrl_channel_ctx.dev_inited = 1;

    return 0;

failed_kfifo_out_alloc:
    kfifo_free( &ctrl_channel_ctx.ctrl_kfifo_in );
failed_kfifo_in_alloc:
    misc_deregister( &ctrl_channel_ctx.ctrl_dev );

    LOG( LOG_ERR, "Error: init failed for dev: %s.", ctrl_channel_ctx.ctrl_dev.name );
    return rc;
}

void ctrl_channel_deinit( void )
{
    if ( ctrl_channel_ctx.dev_inited ) {
        kfifo_free( &ctrl_channel_ctx.ctrl_kfifo_in );

        kfifo_free( &ctrl_channel_ctx.ctrl_kfifo_out );

        misc_deregister( &ctrl_channel_ctx.ctrl_dev );
    } else {
        LOG( LOG_DEBUG, "dev not inited, do nothing." );
    }
}

static uint8_t is_uf_needed_command( uint8_t command_type, uint8_t command, uint8_t direction )
{
    uint8_t rc = 0;

    // we only support SET in user-FW.
    if ( COMMAND_GET == direction )
        return rc;

    switch ( command_type ) {
#ifdef TALGORITHMS
    case TALGORITHMS:
        LOG( LOG_DEBUG, "TALGORITHMS is supported, cmd_type: %u, cmd: %u, direction: %u",
             command_type, command, direction );
        rc = 1;
        break;
#endif

#ifdef TGENERAL
    case TGENERAL:
        switch ( command ) {
        case ACTIVE_CONTEXT:
            LOG( LOG_DEBUG, "TGENERAL is supported, cmd_type: %u, cmd: %u, direction: %u",
                 command_type, command, direction );
            rc = 1;
            break;
        }

        break;
#endif

#ifdef TSENSOR
    case TSENSOR:
        LOG( LOG_DEBUG, "TSENSOR is supported, cmd_type: %u, cmd: %u, direction: %u",
             command_type, command, direction );
        rc = 1;
        break;
#endif

#ifdef TSYSTEM
    case TSYSTEM:

        if ( command != SYSTEM_FREEZE_FIRMWARE ) {
            rc = 1;
            LOG( LOG_DEBUG, "TSYSTEM is supported, cmd_type: %u, cmd: %u, direction: %u",
                 command_type, command, direction );
        } else {
            LOG( LOG_INFO, "SYSTEM_FREEZE_FIRMWARE is not needed" );
        }

        break;
#endif

#ifdef TSCENE_MODES
    case TSCENE_MODES:
        LOG( LOG_DEBUG, "TSCENE_MODES is supported, cmd_type: %u, cmd: %u, direction: %u",
             command_type, command, direction );
        rc = 1;
        break;
#endif

#ifdef TISP_MODULES
    case TISP_MODULES:
        LOG( LOG_DEBUG, "TISP_MODULES is supported, cmd_type: %u, cmd: %u, direction: %u",
             command_type, command, direction );
        rc = 1;
        break;
#endif
    }

    return rc;
}

void ctrl_channel_handle_command( uint32_t cmd_ctx_id, uint8_t type, uint8_t command, uint32_t value, uint8_t direction )
{
    struct ctrl_cmd_item *p_cmd = &ctrl_channel_ctx.cmd_item;

    if ( !ctrl_channel_ctx.dev_inited ) {
        LOG( LOG_ERR, "FW ctrl channel is not inited." );
        return;
    }

    if ( !ctrl_channel_ctx.dev_opened ) {
        LOG( LOG_DEBUG, "FW ctrl channel is not opened, skip." );
        return;
    }

    /* If user-FW is not needed this command, we do nothing */
    if ( !is_uf_needed_command( type, command, direction ) ) {
        return;
    }

    p_cmd->cmd_category = CTRL_CMD_CATEGORY_API_COMMAND;
    p_cmd->cmd_len = sizeof( struct ctrl_cmd_item );
#if FIRMWARE_CONTEXT_NUMBER >= 2
    p_cmd->cmd_ctx_id = cmd_ctx_id;
#endif
    p_cmd->cmd_type = type;
    p_cmd->cmd_id = command;
    p_cmd->cmd_direction = direction;
    p_cmd->cmd_value = value;

    LOG( LOG_INFO, "api_command: cmd_type: %u, cmd_id: %u, cmd_direction: %u, cmd_value: %u.", p_cmd->cmd_type, p_cmd->cmd_id, p_cmd->cmd_direction, p_cmd->cmd_value );

    ctrl_channel_write( p_cmd, NULL, 0 );
}

void ctrl_channel_handle_api_calibration( uint32_t cmd_ctx_id, uint8_t type, uint8_t id, uint8_t direction, void *data, uint32_t data_size )
{
    struct ctrl_cmd_item *p_cmd = &ctrl_channel_ctx.cmd_item;

    if ( !ctrl_channel_ctx.dev_inited ) {
        LOG( LOG_ERR, "FW ctrl channel is not inited." );
        return;
    }

    if ( !ctrl_channel_ctx.dev_opened ) {
        return;
    }

    p_cmd->cmd_category = CTRL_CMD_CATEGORY_API_CALIBRATION;
    p_cmd->cmd_len = sizeof( struct ctrl_cmd_item ) + data_size;
    p_cmd->cmd_type = type;
#if FIRMWARE_CONTEXT_NUMBER >= 2
    p_cmd->cmd_ctx_id = cmd_ctx_id;
#endif
    p_cmd->cmd_id = id;
    p_cmd->cmd_direction = direction;
    p_cmd->cmd_value = data_size;

    LOG( LOG_INFO, "api_alibration: cmd_type: %u, cmd_id: %u, cmd_direction: %u, cmd_value: %u.", p_cmd->cmd_type, p_cmd->cmd_id, p_cmd->cmd_direction, p_cmd->cmd_value );

    ctrl_channel_write( p_cmd, data, data_size );
}

/* Below empty interface functions is used for build in kern-FW */
void ctrl_channel_process( void )
{
}
