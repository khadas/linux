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
#include <linux/kfifo.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include "system_chardev.h"
#include "acamera_logger.h"

#define SYSTEM_CHARDEV_FIFO_SIZE 4096
#define SYSTEM_CHARDEV_NAME "ac_isp"

struct isp_dev_context {
    uint8_t dev_inited;
    int dev_minor_id;
    char *dev_name;
    int dev_opened;

    struct miscdevice isp_dev;
    struct mutex fops_lock;

    struct kfifo isp_kfifo_in;
    struct kfifo isp_kfifo_out;

    wait_queue_head_t kfifo_in_queue;
    wait_queue_head_t kfifo_out_queue;
};

/* First item for ACT Control, second for user-FW */
static struct isp_dev_context isp_dev_ctx;

static int isp_fops_open( struct inode *inode, struct file *f )
{
    int rc;
    struct isp_dev_context *p_ctx = NULL;
    int minor = iminor( inode );

    if ( !isp_dev_ctx.dev_inited ) {
        LOG( LOG_ERR, "dev is not inited, failed to open." );
        return -ERESTARTSYS;
    }

    if ( minor == isp_dev_ctx.dev_minor_id )
        p_ctx = &isp_dev_ctx;

    if ( !p_ctx ) {
        LOG( LOG_CRIT, "Fatal error, isp dev contexts is wrong, contents dump:" );
        LOG( LOG_CRIT, "    minor_id: %d, name: %s.", isp_dev_ctx.dev_minor_id, isp_dev_ctx.dev_name );

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

static int isp_fops_release( struct inode *inode, struct file *f )
{
    int rc;
    struct isp_dev_context *p_ctx = (struct isp_dev_context *)f->private_data;

    if ( p_ctx != &isp_dev_ctx ) {
        LOG( LOG_ERR, "Inalid paramter: %p.", p_ctx );
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
        kfifo_reset( &p_ctx->isp_kfifo_in );
        kfifo_reset( &p_ctx->isp_kfifo_out );
    } else {
        LOG( LOG_CRIT, "Fatal error: wrong state of dev: %s, dev_opened: %d.", p_ctx->dev_name, p_ctx->dev_opened );
        rc = -EINVAL;
    }

    mutex_unlock( &p_ctx->fops_lock );

    return 0;
}

static ssize_t isp_fops_write( struct file *file, const char __user *buf, size_t count, loff_t *ppos )
{
    int rc;
    unsigned int copied;
    struct isp_dev_context *p_ctx = (struct isp_dev_context *)file->private_data;

    /* isp_dev_uf device only support read at the moment */
    if ( p_ctx != &isp_dev_ctx ) {
        LOG( LOG_ERR, "Inalid paramter: %p.", p_ctx );
        return -EINVAL;
    }

    if ( mutex_lock_interruptible( &p_ctx->fops_lock ) ) {
        LOG( LOG_CRIT, "Fatal error: access lock failed." );
        return -ERESTARTSYS;
    }

    rc = kfifo_from_user( &p_ctx->isp_kfifo_in, buf, count, &copied );

    /* awake any reader */
    wake_up_interruptible( &p_ctx->kfifo_in_queue );

    LOG( LOG_DEBUG, "wake up reader." );

    mutex_unlock( &p_ctx->fops_lock );

    return rc ? rc : copied;
}

static ssize_t isp_fops_read( struct file *file, char __user *buf, size_t count, loff_t *ppos )
{
    int rc;
    unsigned int copied;
    struct isp_dev_context *p_ctx = (struct isp_dev_context *)file->private_data;

    if ( p_ctx != &isp_dev_ctx ) {
        LOG( LOG_ERR, "Inalid paramter: %p.", p_ctx );
        return -EINVAL;
    }

    if ( mutex_lock_interruptible( &p_ctx->fops_lock ) ) {
        LOG( LOG_CRIT, "Fatal error: access lock failed." );
        return -ERESTARTSYS;
    }

    if ( kfifo_is_empty( &p_ctx->isp_kfifo_out ) ) {
        long time_out_in_jiffies = 1;      /* jiffies is depend on HW, in x86 Ubuntu, it's 4 ms, 1 is 4ms. */
        mutex_unlock( &p_ctx->fops_lock ); /* unlock before we return or go sleeping */

        /* return if it's a non-blocking reading  */
        if ( file->f_flags & O_NONBLOCK )
            return -EAGAIN;

        /* wait for the event */
        LOG( LOG_DEBUG, "input FIFO is empty, wait for data, timeout_in_jiffies: %ld, HZ: %d.", time_out_in_jiffies, HZ );
        rc = wait_event_interruptible_timeout( p_ctx->kfifo_out_queue, !kfifo_is_empty( &p_ctx->isp_kfifo_out ), time_out_in_jiffies );

        LOG( LOG_DEBUG, "data is coming or timeout, kfifo_out size: %u, rc: %d.", kfifo_len( &p_ctx->isp_kfifo_out ), rc );

        /* after wake up, we need to re-gain the mutex */
        if ( mutex_lock_interruptible( &p_ctx->fops_lock ) ) {
            LOG( LOG_CRIT, "Fatal error: access lock failed." );
            return -ERESTARTSYS;
        }
    }

    rc = kfifo_to_user( &p_ctx->isp_kfifo_out, buf, count, &copied );
    mutex_unlock( &p_ctx->fops_lock );

    return rc ? rc : copied;
}

static struct file_operations isp_fops = {
    .owner = THIS_MODULE,
    .open = isp_fops_open,
    .release = isp_fops_release,
    .read = isp_fops_read,
    .write = isp_fops_write,
    .llseek = noop_llseek,
};

static int isp_dev_context_init( struct isp_dev_context *p_ctx )
{
    int rc;

    p_ctx->isp_dev.minor = MISC_DYNAMIC_MINOR;
    p_ctx->isp_dev.fops = &isp_fops;

    rc = misc_register( &p_ctx->isp_dev );
    if ( rc ) {
        LOG( LOG_ERR, "Error: register ISP device failed, ret: %d.", rc );
        return rc;
    }

    p_ctx->dev_minor_id = p_ctx->isp_dev.minor;

    rc = kfifo_alloc( &p_ctx->isp_kfifo_in, SYSTEM_CHARDEV_FIFO_SIZE, GFP_KERNEL );
    if ( rc ) {
        LOG( LOG_ERR, "Error: kfifo_in alloc failed, ret: %d.", rc );
        goto failed_kfifo_in_alloc;
    }

    rc = kfifo_alloc( &p_ctx->isp_kfifo_out, SYSTEM_CHARDEV_FIFO_SIZE, GFP_KERNEL );
    if ( rc ) {
        LOG( LOG_ERR, "Error: kfifo_out alloc failed, ret: %d.", rc );
        goto failed_kfifo_out_alloc;
    }

    mutex_init( &p_ctx->fops_lock );
    init_waitqueue_head( &p_ctx->kfifo_in_queue );
    init_waitqueue_head( &p_ctx->kfifo_out_queue );

    p_ctx->dev_inited = 1;

    LOG( LOG_INFO, "isp_dev_context(%s) init OK.", p_ctx->dev_name );

    return 0;

failed_kfifo_out_alloc:
    kfifo_free( &p_ctx->isp_kfifo_in );
failed_kfifo_in_alloc:
    misc_deregister( &p_ctx->isp_dev );

    return rc;
}

int system_chardev_init( void )
{
    int rc;

    struct isp_dev_context *p_ctx0 = NULL;

    LOG( LOG_INFO, "system init" );

    p_ctx0 = &isp_dev_ctx;
    memset( p_ctx0, 0, sizeof( *p_ctx0 ) );
    p_ctx0->isp_dev.name = SYSTEM_CHARDEV_NAME;
    p_ctx0->dev_name = SYSTEM_CHARDEV_NAME;
    rc = isp_dev_context_init( p_ctx0 );
    if ( rc ) {
        LOG( LOG_ERR, "Error: isp_dev_context_init failed for dev: %s.", p_ctx0->isp_dev.name );
        return rc;
    }

    return 0;
}


int system_chardev_read( char *data, int size )
{
    int rc;

    if ( !isp_dev_ctx.dev_inited ) {
        LOG( LOG_ERR, "dev is not inited, failed to read." );
        return -1;
    }

    mutex_lock( &isp_dev_ctx.fops_lock );

    if ( kfifo_is_empty( &isp_dev_ctx.isp_kfifo_in ) ) {
        long time_out_in_jiffies = 2;           /* jiffies is depend on HW, in x86 Ubuntu, it's 4 ms, 2 is 8ms. */
        mutex_unlock( &isp_dev_ctx.fops_lock ); /* unlock before we return or go sleeping */

        /* wait for the event */
        LOG( LOG_DEBUG, "input FIFO is empty, wait for data, timeout_in_jiffies: %ld, HZ: %d.", time_out_in_jiffies, HZ );
        rc = wait_event_interruptible_timeout( isp_dev_ctx.kfifo_in_queue, !kfifo_is_empty( &isp_dev_ctx.isp_kfifo_in ), time_out_in_jiffies );

        LOG( LOG_DEBUG, "data is coming or timeout, kfifo_in size: %u, rc: %d.", kfifo_len( &isp_dev_ctx.isp_kfifo_in ), rc );

        /* after wake up, we need to re-gain the mutex */
        mutex_lock( &isp_dev_ctx.fops_lock );
    }

    rc = kfifo_out( &isp_dev_ctx.isp_kfifo_in, data, size );

    mutex_unlock( &isp_dev_ctx.fops_lock );
    return rc;
}

int system_chardev_write( const char *data, int size )
{
    int rc;

    if ( !isp_dev_ctx.dev_inited ) {
        LOG( LOG_ERR, "dev is not inited, failed to write." );
        return -1;
    }

    mutex_lock( &isp_dev_ctx.fops_lock );

    rc = kfifo_in( &isp_dev_ctx.isp_kfifo_out, data, size );

    /* awake any reader */
    wake_up_interruptible( &isp_dev_ctx.kfifo_out_queue );
    LOG( LOG_DEBUG, "wake up reader who wait on kfifo out." );

    mutex_unlock( &isp_dev_ctx.fops_lock );
    return rc;
}

int system_chardev_destroy( void )
{
    if ( isp_dev_ctx.dev_inited ) {
        kfifo_free( &isp_dev_ctx.isp_kfifo_in );

        kfifo_free( &isp_dev_ctx.isp_kfifo_out );

        misc_deregister( &isp_dev_ctx.isp_dev );

        LOG( LOG_INFO, "misc_deregister dev: %s.", isp_dev_ctx.dev_name );
    } else {
        LOG( LOG_INFO, "dev not inited, do nothing." );
    }

    return 0;
}
