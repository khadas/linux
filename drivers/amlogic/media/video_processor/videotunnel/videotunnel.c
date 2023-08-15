// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/video_processor/videotunnel/videotunnel.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/freezer.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/debugfs.h>
#include <linux/random.h>
#include <linux/sched/clock.h>
#include <linux/amlogic/aml_sync_api.h>
#include <linux/sched/task.h>
#include <linux/sync_file.h>

#include <asm-generic/bug.h>

#include "videotunnel_priv.h"
#include "videotunnel.h"

#define DEVICE_NAME "videotunnel"
#define MIN_VIDEO_TUNNEL_ID 10
#define MAX_VIDEO_TUNNEL_ID 64
#define VT_CMD_FIFO_SIZE 128

static struct vt_dev *vdev;
static struct mutex debugfs_mutex;
static struct vt_instance dummy_instance;

enum {
	VT_DEBUG_NONE             = 0,
	VT_DEBUG_USER             = 1U << 0,
	VT_DEBUG_BUFFERS          = 1U << 1,
	VT_DEBUG_CMD              = 1U << 2,
	VT_DEBUG_FILE             = 1U << 3,
	VT_DEBUG_VSYNC            = 1U << 4,
};

static u32 vt_debug_mask = VT_DEBUG_USER;
module_param_named(debug_mask, vt_debug_mask, uint, 0644);

#define vt_debug(mask, x...) \
	do { \
		if (vt_debug_mask & (mask)) \
			pr_info(x); \
	} while (0)

static const char *vt_debug_buffer_status_to_string(int status)
{
	const char *status_str;

	switch (status) {
	case VT_BUFFER_QUEUE:
		status_str = "queued";
		break;
	case VT_BUFFER_DEQUEUE:
		status_str = "dequeued";
		break;
	case VT_BUFFER_ACQUIRE:
		status_str = "acquired";
		break;
	case VT_BUFFER_RELEASE:
		status_str = "released";
		break;
	case VT_BUFFER_FREE:
		status_str = "free";
		break;
	default:
		status_str = "unknown";
	}

	return status_str;
}

static const char *vt_debug_mode_status_to_string(int status)
{
	const char *status_str;

	switch (status) {
	case VT_MODE_BLOCK:
		status_str = "BLOCK";
		break;
	case VT_MODE_NONE_BLOCK:
		status_str = "NONE BLOCK";
		break;
	case VT_MODE_GAME:
		status_str = "GAME";
		break;
	default:
		status_str = "unknown";
	}

	return status_str;
}

static int vt_debug_instance_show(struct seq_file *s, void *unused)
{
	struct vt_instance *instance = s->private;
	int i;
	int size_to_con;
	int size_to_pro;
	int size_cmd;
	int ref_count;

	mutex_lock(&debugfs_mutex);
	mutex_lock(&instance->lock);
	size_to_con = kfifo_len(&instance->fifo_to_consumer);
	size_to_pro = kfifo_len(&instance->fifo_to_producer);
	size_cmd = kfifo_len(&instance->fifo_cmd);
	ref_count = atomic_read(&instance->ref.refcount.refs);

	seq_printf(s, "tunnel (%p) id=%d, ref=%d, fcount=%d, mode=%s\n",
		   instance,
		   instance->id,
		   ref_count,
		   instance->fcount,
		   vt_debug_mode_status_to_string(instance->mode));
	seq_puts(s, "-----------------------------------------------\n");
	if (instance->consumer)
		seq_printf(s, "consumer session (%s) %p\n",
			   instance->consumer->display_name,
			   instance->consumer);
	if (instance->producer)
		seq_printf(s, "producer session (%s) %p\n",
			   instance->producer->display_name,
			   instance->producer);
	seq_puts(s, "-----------------------------------------------\n");

	seq_printf(s, "to consumer fifo size:%d\n", size_to_con);
	seq_printf(s, "to producer fifo size:%d\n", size_to_pro);
	seq_printf(s, "cmd fifo size:%d\n", size_cmd);
	seq_puts(s, "-----------------------------------------------\n");

	seq_puts(s, "buffers:\n");

	for (i = 0; i < VT_POOL_SIZE; i++) {
		struct vt_buffer *buffer = &instance->vt_buffers[i];
		int status = buffer->item.buffer_status;

		if (status == VT_BUFFER_QUEUE || status == VT_BUFFER_ACQUIRE ||
				status == VT_BUFFER_RELEASE)
			seq_printf(s, "    buffer produce_fd(%d) status(%s) timestamp(%lld)\n",
				   buffer->buffer_fd_pro,
				   vt_debug_buffer_status_to_string(status),
				   buffer->item.time_stamp);
	}
	seq_puts(s, "-----------------------------------------------\n");
	seq_printf(s, "total acquire: %ld\n", instance->state.acquire_count);
	seq_printf(s, "total release: %ld (%ld+%ld(prev))\n",
		   instance->state.release_count +
		   instance->state.release_invalid,
		   instance->state.release_count,
		   instance->state.release_invalid);
	seq_printf(s, "total queue: %ld\n", instance->state.queue_count);
	seq_printf(s, "total dequeue: %ld (%ld+%ld(prev))\n",
		   instance->state.dequeue_count +
		   instance->state.dequeue_invalid,
		   instance->state.dequeue_count,
		   instance->state.dequeue_invalid);
	seq_puts(s, "-----------------------------------------------\n");

	mutex_unlock(&instance->lock);
	mutex_unlock(&debugfs_mutex);

	return 0;
}

static int vt_debug_instance_open(struct inode *inode, struct file *file)
{
	return single_open(file,
			   vt_debug_instance_show,
			   inode->i_private);
}

static const struct file_operations debug_instance_fops = {
	.owner = THIS_MODULE,
	.open = vt_debug_instance_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int vt_debug_session_show(struct seq_file *s, void *unused)
{
	struct vt_session *session = s->private;

	mutex_lock(&debugfs_mutex);
	seq_printf(s, "session(%s) %p\n",
		   session->display_name, session);
	seq_printf(s, "role: %s cid: %ld\n",
		   session->role == VT_ROLE_PRODUCER ?
		   "producer" : (session->role == VT_ROLE_CONSUMER ?
		   "consumer" : "invalid"), session->cid);
	seq_printf(s, "mode: %s\n",
		   vt_debug_mode_status_to_string(session->mode));
	seq_puts(s, "-----------------------------------------------\n");
	mutex_unlock(&debugfs_mutex);

	return 0;
}

static int vt_debug_session_open(struct inode *inode, struct file *file)
{
	return single_open(file,
			   vt_debug_session_show,
			   inode->i_private);
}

static const struct file_operations debug_session_fops = {
	.owner = THIS_MODULE,
	.open = vt_debug_session_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int vt_debug_state_show(struct seq_file *s, void *unused)
{
	struct vt_state *state = s->private;

	mutex_lock(&debugfs_mutex);
	seq_puts(s, "-----------------------------------------------\n");
	seq_puts(s, "fence status:\n");
	seq_printf(s, "total fence fget: %ld\n", state->fence_get);
	seq_printf(s, "total fence fput: %ld (%ld(dequeue)-%ld(null)+%ld(fput))\n",
		   state->dequeue_count - state->null_fence + state->fence_put,
		   state->dequeue_count, state->null_fence, state->fence_put);

	seq_puts(s, "-----------------------------------------------\n");
	seq_puts(s, "buffer status:\n");
	seq_printf(s, "total acquire: %ld\n", state->acquire_count);
	seq_printf(s, "total release: %ld (%ld+%ld(prev))\n",
		   state->release_count + state->release_invalid,
		   state->release_count, state->release_invalid);
	seq_printf(s, "total queue: %ld\n", state->queue_count);
	seq_printf(s, "total dequeue: %ld (%ld+%ld(prev))\n",
		   state->dequeue_count + state->dequeue_invalid,
		   state->dequeue_count, state->dequeue_invalid);
	seq_puts(s, "-----------------------------------------------\n");
	seq_printf(s, "total buffer fget: %ld\n", state->buffer_get);
	seq_printf(s, "total buffer fput: %ld (%ld(fput)+%ld(close))\n",
		   state->buffer_put + state->buffer_close,
		   state->buffer_put, state->buffer_close);
	seq_puts(s, "-----------------------------------------------\n");

	mutex_unlock(&debugfs_mutex);

	return 0;
}

static int vt_debug_state_open(struct inode *inode, struct file *file)
{
	return single_open(file,
			   vt_debug_state_show,
			   inode->i_private);
}

static const struct file_operations debug_state_fops = {
	.owner = THIS_MODULE,
	.open = vt_debug_state_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int vt_debug_limit_show(struct seq_file *sf, void *data)
{
	struct vt_dev *vdev = sf->private;

	seq_puts(sf, "when the buffer size in vt research the limit size then you can dequeue\n");
	seq_printf(sf, "current: %d\n", vdev->limit);
	return 0;
}

static int vt_debug_limit_open(struct inode *inode, struct file *file)
{
	return single_open(file,
			   vt_debug_limit_show,
			   inode->i_private);
}

static ssize_t vt_debug_limit_write(struct file *file, const char __user *ubuf,
				size_t len, loff_t *offp)
{
	char buf[8];
	int limits = 0;
	struct seq_file *sf = file->private_data;
	struct vt_dev *vdev = sf->private;

	if (len > sizeof(buf) - 1)
		return -EINVAL;

	if (copy_from_user(buf, ubuf, len))
		return -EFAULT;
	if (buf[len - 1] == '\n')
		buf[len - 1] = '\0';
	buf[len] = '\0';

	if (kstrtoint(buf, 0, &limits) == 0)
		vdev->limit = (limits > 0) ? limits : 0;

	return len;
}

static const struct file_operations debug_limit_fops = {
	.owner = THIS_MODULE,
	.open = vt_debug_limit_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = vt_debug_limit_write,
};

static void vt_instance_destroy(struct kref *kref)
{
	struct vt_instance *instance =
		container_of(kref, struct vt_instance, ref);
	struct vt_dev *dev = instance->dev;
	struct vt_buffer *buffer = NULL;
	struct vt_cmd *vcmd = NULL;
	int i;

	mutex_lock(&debugfs_mutex);
	mutex_lock(&dev->instance_lock);
	rb_erase(&instance->node, &dev->instances);
	if (idr_find(&dev->instance_idr, instance->id))
		idr_remove(&dev->instance_idr, instance->id);

	for (i = 0; i < instance->id; i++) {
		/* remove the dummy pointer ID */
		if (idr_find(&dev->instance_idr, i) == &dummy_instance)
			idr_remove(&dev->instance_idr, i);
	}

	/* destroy fifo to consumer */
	mutex_lock(&instance->lock);
	if (!kfifo_is_empty(&instance->fifo_to_consumer)) {
		while (kfifo_get(&instance->fifo_to_consumer, &buffer)) {
			/* put file */
			if (buffer->file_buffer) {
				fput(buffer->file_buffer);
				instance->fcount--;
				dev->state.buffer_put++;
				instance->state.buffer_put++;
			}
			buffer->item.buffer_status = VT_BUFFER_FREE;
		}
	}
	kfifo_free(&instance->fifo_to_consumer);

	/* destroy fifo to producer */
	if (!kfifo_is_empty(&instance->fifo_to_producer)) {
		while (kfifo_get(&instance->fifo_to_producer, &buffer)) {
			if (buffer->file_fence) {
				fput(buffer->file_fence);
				dev->state.fence_put++;
				instance->state.fence_put++;
			}
			buffer->item.buffer_status = VT_BUFFER_FREE;
		}
	}
	kfifo_free(&instance->fifo_to_producer);
	mutex_unlock(&instance->lock);

	/* destroy fifo cmd */
	mutex_lock(&instance->cmd_lock);
	if (!kfifo_is_empty(&instance->fifo_cmd)) {
		while (kfifo_get(&instance->fifo_cmd, &vcmd))
			kfree(vcmd);
	}
	kfifo_free(&instance->fifo_cmd);
	mutex_unlock(&instance->cmd_lock);

	debugfs_remove_recursive(instance->debug_root);

	vt_debug(VT_DEBUG_USER, "vt [%d] destroy fcount:%d\n",
		 instance->id, instance->fcount);

	kfree(instance);
	mutex_unlock(&dev->instance_lock);
	mutex_unlock(&debugfs_mutex);
}

static void vt_instance_get(struct vt_instance *instance)
{
	kref_get(&instance->ref);
}

static int vt_instance_put(struct vt_instance *instance)
{
	if (!instance)
		return -ENOENT;

	return kref_put(&instance->ref, vt_instance_destroy);
}

static struct vt_instance *vt_instance_create_lock(struct vt_dev *dev)
{
	struct vt_instance *instance;
	struct vt_instance *entry;
	struct rb_node **p;
	struct rb_node *parent = NULL;
	int status;
	int i;

	instance = kzalloc(sizeof(*instance), GFP_KERNEL);
	if (!instance)
		return ERR_PTR(-ENOMEM);

	instance->dev = dev;
	instance->fcount = 0;
	instance->mode = VT_MODE_NONE_BLOCK;
	instance->used = false;

	mutex_init(&instance->lock);
	mutex_init(&instance->cmd_lock);
	kref_init(&instance->ref);

	status = kfifo_alloc(&instance->fifo_to_consumer,
			     VT_POOL_SIZE, GFP_KERNEL);
	if (status)
		goto setup_fail;

	status = kfifo_alloc(&instance->fifo_to_producer,
			     VT_POOL_SIZE, GFP_KERNEL);
	if (status)
		goto setup_fail;

	/* init fifo cmd */
	status = kfifo_alloc(&instance->fifo_cmd, VT_CMD_FIFO_SIZE, GFP_KERNEL);
	if (status)
		goto setup_fail;

	init_waitqueue_head(&instance->wait_producer);
	init_waitqueue_head(&instance->wait_consumer);
	init_waitqueue_head(&instance->wait_cmd);

	/* set the buffer pool status to free */
	for (i = 0; i < VT_POOL_SIZE; i++)
		instance->vt_buffers[i].item.buffer_status = VT_BUFFER_FREE;

	/* insert it to dev instances rb tree */
	p = &dev->instances.rb_node;
	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct vt_instance, node);

		if (instance < entry)
			p = &(*p)->rb_left;
		else if (instance > entry)
			p = &(*p)->rb_right;
		else
			break;
	}
	rb_link_node(&instance->node, parent, p);
	rb_insert_color(&instance->node, &dev->instances);

	return instance;

setup_fail:
	kfree(instance);
	return ERR_PTR(status);
}

static int vt_get_session_serial(const struct rb_root *root,
				 const char *name)
{
	int serial = -1;
	struct rb_node *node;

	for (node = rb_first(root); node; node = rb_next(node)) {
		struct vt_session *session =
		    rb_entry(node, struct vt_session, node);

		if (strcmp(session->name, name))
			continue;
		serial = max(serial, session->display_serial);
	}
	return serial + 1;
}

static struct vt_session *vt_session_create_internal(struct vt_dev *dev,
						     const char *name)
{
	struct vt_session *session;
	struct task_struct *task = NULL;
	struct rb_node **p;
	struct rb_node *parent = NULL;
	struct vt_session *entry;

	if (!name) {
		pr_err("%s: Name can not be null\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	session = kzalloc(sizeof(*session), GFP_KERNEL);
	if (!session)
		goto err_put_task_struct;

	get_task_struct(current->group_leader);
	task_lock(current->group_leader);
	session->pid = task_pid_nr(current->group_leader);
	/*
	 * don't bother to store task struct for kernel threads,
	 * they can't be killed anyway
	 */
	if (current->group_leader->flags & PF_KTHREAD) {
		put_task_struct(current->group_leader);
		task = NULL;
	} else {
		task = current->group_leader;
	}

	task_unlock(current->group_leader);

	session->dev = dev;
	session->task = task;
	session->role = VT_ROLE_INVALID;
	session->mode = VT_MODE_BLOCK;
	session->cmd_status = 0;

	init_waitqueue_head(&session->wait_producer);
	init_waitqueue_head(&session->wait_consumer);
	init_waitqueue_head(&session->wait_cmd);

	session->name = kstrdup(name, GFP_KERNEL);
	if (!session->name)
		goto err_free_session;

	down_write(&dev->session_lock);
	session->display_serial =
		vt_get_session_serial(&dev->sessions, name);
	session->display_name = kasprintf(GFP_KERNEL, "%s-%d",
					  name, session->display_serial);
	if (!session->display_name) {
		up_write(&dev->session_lock);
		goto err_free_session_name;
	}

	/* insert session to device rb tree */
	p = &dev->sessions.rb_node;
	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct vt_session, node);

		if (session < entry)
			p = &(*p)->rb_left;
		else if (session > entry)
			p = &(*p)->rb_right;
		else
			break;
	}
	rb_link_node(&session->node, parent, p);
	rb_insert_color(&session->node, &dev->sessions);

	/* add debug fs */
	session->debug_root = debugfs_create_file(session->display_name,
						  0664,
						  dev->debug_root,
						  session,
						  &debug_session_fops);

	up_write(&dev->session_lock);

	vt_debug(VT_DEBUG_USER, "vt session %s create\n",
		 session->display_name);

	return session;

err_free_session_name:
	kfree(session->name);
err_free_session:
	kfree(session);
err_put_task_struct:
	if (task)
		put_task_struct(current->group_leader);

	return ERR_PTR(-ENOMEM);
}

/*
 * when disconnect, release the buffer in session
 */
static void vt_session_trim_lock(struct vt_session *session,
			    struct vt_instance *instance)
{
	struct vt_buffer *buffer = NULL;

	if (!session || !instance)
		return;

	if (instance->producer && instance->producer == session) {
		while (kfifo_get(&instance->fifo_to_producer, &buffer)) {
			if (buffer->file_fence) {
				fput(buffer->file_fence);
				session->dev->state.fence_put++;
				instance->state.fence_put++;
			}

			buffer->item.buffer_status = VT_BUFFER_FREE;
		}
	}

	if (instance->consumer && instance->consumer == session) {
		while (kfifo_get(&instance->fifo_to_consumer, &buffer)) {
			if (buffer->file_buffer) {
				fput(buffer->file_buffer);
				instance->fcount--;
				session->dev->state.buffer_put++;
				instance->state.buffer_put++;

				vt_debug(VT_DEBUG_FILE,
					 "vt [%d] session trim file(%px) buffer(%p) fcount=%d\n",
					 instance->id, buffer->file_buffer,
					 buffer, instance->fcount);
			}
			/* if still has producer, return the buffer to producer */
			if (instance->producer) {
				buffer->item.buffer_fd = buffer->buffer_fd_pro;
				buffer->item.buffer_status = VT_BUFFER_RELEASE;
				kfifo_put(&instance->fifo_to_producer, buffer);

				vt_debug(VT_DEBUG_FILE,
					 "vt [%d] session trim buffer(%p) back to producer\n",
					 instance->id, buffer);
			} else {
				buffer->item.buffer_status = VT_BUFFER_FREE;
			}
		}
	}
}

/*
 * called when vt session released
 * clean up instance connected session
 */
static int vt_instance_trim(struct vt_session *session)
{
	struct vt_dev *dev = session->dev;
	struct vt_instance *instance = NULL;
	struct vt_buffer *buffer = NULL;
	struct rb_node *n = NULL;
	int i;

	mutex_lock(&dev->instance_lock);
	for (n = rb_first(&dev->instances); n; n = rb_next(n)) {
		instance = rb_entry(n, struct vt_instance, node);
		mutex_lock(&instance->lock);
		if (instance->producer && instance->producer == session) {
			while (kfifo_get(&instance->fifo_to_producer,
					 &buffer)) {
				if (buffer->file_fence) {
					fput(buffer->file_fence);
					dev->state.fence_put++;
					instance->state.fence_put++;
				}
				buffer->item.buffer_status = VT_BUFFER_FREE;
			}
			instance->producer = NULL;
			instance->mode = VT_MODE_NONE_BLOCK;
		}
		if (instance->consumer && instance->consumer == session) {
			while (kfifo_get(&instance->fifo_to_consumer,
					 &buffer)) {
				if (buffer->file_buffer) {
					fput(buffer->file_buffer);
					instance->fcount--;
					dev->state.buffer_put++;
					instance->state.buffer_put++;

					vt_debug(VT_DEBUG_FILE,
						 "vt [%d] instance trim file(%px) buffer(%p), fcount=%d\n",
						 instance->id,
						 buffer->file_buffer,
						 buffer, instance->fcount);
				}
				buffer->item.buffer_status = VT_BUFFER_FREE;
			}
			instance->consumer = NULL;
		}

		if (!instance->consumer && !instance->producer) {
			while (kfifo_get(&instance->fifo_to_producer,
					 &buffer)) {
				if (buffer->file_fence) {
					fput(buffer->file_fence);
					dev->state.fence_put++;
					instance->state.fence_put++;
				}
			}
			while (kfifo_get(&instance->fifo_to_consumer,
					 &buffer)) {
				if (buffer->file_buffer) {
					fput(buffer->file_buffer);
					instance->fcount--;
					dev->state.buffer_put++;
					instance->state.buffer_put++;

					vt_debug(VT_DEBUG_FILE,
						 "vt [%d] instance trim file(%px) buffer(%p) fcount=%d\n",
						 instance->id,
						 buffer->file_buffer,
						 buffer, instance->fcount);
				}
			}

			/* set all instance buffer to free */
			for (i = 0; i < VT_POOL_SIZE; i++)
				instance->vt_buffers[i].item.buffer_status = VT_BUFFER_FREE;

			/* reset status */
			memset(&instance->state, 0, sizeof(instance->state));
			instance->mode = VT_MODE_NONE_BLOCK;
		}

		mutex_unlock(&instance->lock);
	}
	mutex_unlock(&dev->instance_lock);

	return 0;
}

void vt_session_destroy(struct vt_session *session)
{
	struct vt_dev *dev = session->dev;

	vt_debug(VT_DEBUG_USER, "vt session %s destroy\n",
		 session->display_name);

	/* release dev session rb tree node */
	down_write(&dev->session_lock);
	if (session->task)
		put_task_struct(session->task);
	rb_erase(&session->node, &dev->sessions);
	debugfs_remove_recursive(session->debug_root);
	up_write(&dev->session_lock);

	vt_instance_trim(session);

	kfree(session->display_name);
	kfree(session->name);
	kfree(session);
}
EXPORT_SYMBOL(vt_session_destroy);

static int vt_open(struct inode *inode, struct file *filp)
{
	struct miscdevice *miscdev = filp->private_data;
	struct vt_dev *dev =
		container_of(miscdev, struct vt_dev, mdev);
	struct vt_session *session;
	char debug_name[64];

	snprintf(debug_name, 64, "%u", task_pid_nr(current->group_leader));
	session = vt_session_create_internal(dev, debug_name);
	if (IS_ERR(session))
		return PTR_ERR(session);

	filp->private_data = session;

	return 0;
}

static int vt_release(struct inode *inode, struct file *filp)
{
	struct vt_session *session = filp->private_data;

	vt_session_destroy(session);
	return 0;
}

static long vt_get_connected_id(void)
{
	long cid;

	cid = get_random_long();
	if (cid == -1)
		return 0;

	return cid;
}

static int vt_alloc_id_process(struct vt_alloc_id_data *data,
			       struct vt_session *session)
{
	int i;
	int ret = 0;
	char name[64];
	struct vt_dev *dev = session->dev;
	struct vt_instance *instance =  NULL;
	struct rb_node *n = NULL;

	mutex_lock(&dev->instance_lock);
	/* find an unused vt instance */
	for (n = rb_first(&dev->instances); n; n = rb_next(n)) {
		instance = rb_entry(n, struct vt_instance, node);
		mutex_lock(&instance->lock);
		/* no consumer and producer, not new alloced */
		if (!instance->consumer &&
		    !instance->producer &&
		    instance->used &&
		    instance->id >= MIN_VIDEO_TUNNEL_ID) {
			data->tunnel_id = instance->id;
			instance->used = false;
			vt_debug(VT_DEBUG_USER, "vt alloc find instance [%d], ref %d\n",
				 instance->id,
				 atomic_read(&instance->ref.refcount.refs));
			mutex_unlock(&instance->lock);
			mutex_unlock(&dev->instance_lock);
			return 0;
		}
		mutex_unlock(&instance->lock);
	}

	/* not find, create one */
	instance = vt_instance_create_lock(session->dev);
	if (IS_ERR(instance)) {
		mutex_unlock(&dev->instance_lock);
		return PTR_ERR(instance);
	}

	for (i = 0; i < MAX_VIDEO_TUNNEL_ID; i++) {
		/* remove the dummy pointer ID */
		if (idr_find(&dev->instance_idr, i) == &dummy_instance)
			idr_remove(&dev->instance_idr, i);
	}

	/* [0 ~ 9] for hardcode id; [10 ~ 64] for dynamic allocate id */
	ret = idr_alloc(&dev->instance_idr, instance, MIN_VIDEO_TUNNEL_ID,
			MAX_VIDEO_TUNNEL_ID, GFP_KERNEL);
	/* allocate ID failed */
	if (ret < 0) {
		mutex_unlock(&dev->instance_lock);
		vt_debug(VT_DEBUG_USER, "vt alloc instance [%d] idr alloc failed ret %d\n",
			instance->id, ret);
		vt_instance_put(instance);
		return ret;
	}

	instance->id = ret;
	snprintf(name, 64, "instance-%d", instance->id);
	instance->debug_root =
		debugfs_create_file(name, 0664, dev->debug_root,
				    instance, &debug_instance_fops);
	data->tunnel_id = instance->id;
	mutex_unlock(&dev->instance_lock);

	vt_debug(VT_DEBUG_USER, "vt alloc instance [%d], ref %d\n",
		 instance->id,
		 atomic_read(&instance->ref.refcount.refs));

	return 0;
}

static int vt_free_id_process(struct vt_alloc_id_data *data,
			      struct vt_session *session)
{
	int ret = 0;
	struct vt_dev *dev = session->dev;
	struct vt_instance *instance = NULL;

	instance = idr_find(&dev->instance_idr,
			    data->tunnel_id);
	/* to do free id operation check */
	if (!instance) {
		pr_err("destroy unknown videotunnel instance:%d\n",
		       data->tunnel_id);
		ret = -EINVAL;
	} else {
		vt_debug(VT_DEBUG_USER, "vt free instance [%d], ref %d\n",
			 instance->id,
			 atomic_read(&instance->ref.refcount.refs));

		ret = vt_instance_put(instance);
	}

	return ret;
}

static int vt_connect_process(struct vt_ctrl_data *data,
			      struct vt_session *session)
{
	struct vt_dev *dev = session->dev;
	struct vt_instance *instance;
	struct vt_instance *replace;
	struct vt_cmd *cmd;
	struct vt_cmd *cmd_sourcecrop;
	struct vt_cmd *cmd_displayframe;
	int id = data->tunnel_id;
	int ret = 0;
	char name[64];

	mutex_lock(&dev->instance_lock);
	instance = idr_find(&dev->instance_idr, id);
	if (!instance || instance == &dummy_instance) {
		if (!instance) {
			while ((ret = idr_alloc(&dev->instance_idr,
						&dummy_instance, 0,
						MAX_VIDEO_TUNNEL_ID,
						GFP_KERNEL))
					<= id) {
				if (ret == id) {
					break;
				} else if (ret < 0) {
					pr_err("Connect to vt [%d] idr alloc fail:%d\n",
					       id, ret);
					mutex_unlock(&dev->instance_lock);
					return ret;
				}
			}
		}

		instance = vt_instance_create_lock(dev);
		if (IS_ERR(instance)) {
			mutex_unlock(&dev->instance_lock);
			return PTR_ERR(instance);
		}

		replace = idr_replace(&dev->instance_idr, instance, id);

		if (IS_ERR(replace)) {
			mutex_unlock(&dev->instance_lock);
			vt_instance_put(instance);
			return PTR_ERR(replace);
		}
		if (!replace)
			vt_instance_put(replace);

		instance->id = id;
		snprintf(name, 64, "instance-%d", instance->id);
		instance->debug_root =
			debugfs_create_file(name, 0664, dev->debug_root,
					    instance,
					    &debug_instance_fops);

		vt_debug(VT_DEBUG_USER, "vt [%d] create\n", instance->id);
	} else {
		vt_instance_get(instance);
	}
	mutex_unlock(&dev->instance_lock);

	mutex_lock(&instance->lock);
	if (data->role == VT_ROLE_PRODUCER) {
		if (instance->producer &&
		    instance->producer != session) {
			mutex_unlock(&instance->lock);
			vt_instance_put(instance);
			pr_err("Connect to vt [%d] err, already has producer\n",
			       id);
			return -EINVAL;
		}
		instance->producer = session;
		memset(&instance->backup_sourcecrop, -1, sizeof(instance->backup_sourcecrop));
		memset(&instance->backup_displayframe, -1, sizeof(instance->backup_displayframe));
	} else if (data->role == VT_ROLE_CONSUMER) {
		if (instance->consumer &&
		    instance->consumer != session) {
			mutex_unlock(&instance->lock);
			vt_instance_put(instance);
			pr_err("Connect to vt [%d] err, already has consumer\n",
			       id);
			return -EINVAL;
		}

		/* consumer connect, send game mode cmd if needed */
		if (instance->mode == VT_MODE_GAME) {
			cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
			if (!cmd) {
				mutex_unlock(&instance->lock);
				vt_instance_put(instance);
				pr_err("Connect to vt [%d] err, resend cmd fail\n",
				    id);
				return -ENOMEM;
			}

			cmd->cmd = VT_VIDEO_SET_GAME_MODE;
			cmd->cmd_data = 1;

			mutex_lock(&instance->cmd_lock);
			kfifo_put(&instance->fifo_cmd, cmd);

			session->cmd_status++;
			vt_debug(VT_DEBUG_CMD, "vt [%d] resend cmd:%d data:%d\n",
				instance->id, cmd->cmd, cmd->cmd_data);
			mutex_unlock(&instance->cmd_lock);
		}

		if (instance->backup_sourcecrop.left >= 0 ||
			instance->backup_sourcecrop.top >= 0 ||
			instance->backup_sourcecrop.right >= 0 ||
			instance->backup_sourcecrop.bottom >= 0) {
			cmd_sourcecrop = kzalloc(sizeof(*cmd_sourcecrop), GFP_KERNEL);
			if (!cmd_sourcecrop) {
				mutex_unlock(&instance->lock);
				vt_instance_put(instance);
				pr_err("Connect to vt [%d] err, resend cmd fail\n",
				    id);
				return -ENOMEM;
			}

			cmd_sourcecrop->cmd = VT_VIDEO_SET_SOURCE_CROP;
			cmd_sourcecrop->rect = instance->backup_sourcecrop;

			mutex_lock(&instance->cmd_lock);
			kfifo_put(&instance->fifo_cmd, cmd_sourcecrop);
			session->cmd_status++;
			vt_debug(VT_DEBUG_CMD, "restore source crop rect (%d %d %d %d)\n",
				cmd_sourcecrop->rect.left, cmd_sourcecrop->rect.top,
				cmd_sourcecrop->rect.right, cmd_sourcecrop->rect.bottom);
			mutex_unlock(&instance->cmd_lock);
		}

		if (instance->backup_displayframe.left >= 0 ||
			instance->backup_displayframe.top >= 0 ||
			instance->backup_displayframe.right >= 0 ||
			instance->backup_displayframe.bottom >= 0) {
			cmd_displayframe = kzalloc(sizeof(*cmd_displayframe), GFP_KERNEL);
			if (!cmd_displayframe) {
				mutex_unlock(&instance->lock);
				vt_instance_put(instance);
				pr_err("Connect to vt [%d] err, resend cmd fail\n",
				    id);
				return -ENOMEM;
			}

			cmd_displayframe->cmd = VT_VIDEO_SET_DISPLAY_FRAME;
			cmd_displayframe->rect = instance->backup_displayframe;

			mutex_lock(&instance->cmd_lock);
			kfifo_put(&instance->fifo_cmd, cmd_displayframe);
			session->cmd_status++;
			vt_debug(VT_DEBUG_CMD, "restore display frame rect (%d %d %d %d)\n",
				cmd_displayframe->rect.left, cmd_displayframe->rect.top,
				cmd_displayframe->rect.right, cmd_displayframe->rect.bottom);
			mutex_unlock(&instance->cmd_lock);
		}

		instance->consumer = session;
	}
	session->cid = vt_get_connected_id();
	session->role = data->role;
	instance->used = true;

	vt_debug(VT_DEBUG_USER, "vt [%d] %s-%d connect, instance ref %d\n",
		 instance->id,
		 data->role == VT_ROLE_PRODUCER ? "producer" : "consumer",
		 session->pid,
		 atomic_read(&instance->ref.refcount.refs));
	mutex_unlock(&instance->lock);

	return 0;
}

static int vt_disconnect_process(struct vt_ctrl_data *data,
				 struct vt_session *session)
{
	struct vt_dev *dev = session->dev;
	struct vt_instance *instance;
	int id = data->tunnel_id;

	/* find the instance with the tunnel_id */
	instance = idr_find(&dev->instance_idr, id);

	if (!instance || session->role != data->role)
		goto find_fail;

	mutex_lock(&instance->lock);
	if (data->role == VT_ROLE_PRODUCER) {
		if (!instance->producer)
			goto disconnect_fail;
		if (instance->producer != session)
			goto disconnect_fail;

		vt_session_trim_lock(session, instance);
		instance->producer = NULL;
		instance->mode = VT_MODE_NONE_BLOCK;
		memset(&instance->backup_sourcecrop, -1, sizeof(instance->backup_sourcecrop));
		memset(&instance->backup_displayframe, -1, sizeof(instance->backup_displayframe));
	} else if (data->role == VT_ROLE_CONSUMER) {
		if (!instance->consumer)
			goto disconnect_fail;
		if (instance->consumer != session)
			goto disconnect_fail;

		vt_session_trim_lock(session, instance);
		instance->consumer = NULL;
	}

	vt_debug(VT_DEBUG_USER, "vt [%d] %s-%d disconnect, instance ref %d, fcount %d\n",
		 instance->id,
		 data->role == VT_ROLE_PRODUCER ? "producer" : "consumer",
		 session->pid,
		 atomic_read(&instance->ref.refcount.refs),
		 instance->fcount);
	mutex_unlock(&instance->lock);
	vt_instance_put(instance);

	session->cid = -1;

	return 0;

disconnect_fail:
	mutex_unlock(&instance->lock);
find_fail:
	return -EINVAL;
}

static int vt_send_cmd_process(struct vt_ctrl_data *data,
			       struct vt_session *session)
{
	struct vt_dev *dev = session->dev;
	struct vt_instance *instance;
	struct vt_cmd *cmd;
	int id = data->tunnel_id;

	instance = idr_find(&dev->instance_idr, id);

	if (data->video_cmd == VT_VIDEO_SET_COLOR_BLACK ||
			data->video_cmd == VT_VIDEO_SET_COLOR_BLUE ||
			data->video_cmd == VT_VIDEO_SET_COLOR_GREEN ||
			data->video_cmd == VT_VIDEO_SET_STATUS ||
			data->video_cmd == VT_VIDEO_SET_SOURCE_CROP ||
			data->video_cmd == VT_VIDEO_SET_DISPLAY_FRAME) {
		/* no instance or instance has no consumer */
		if (!instance || !instance->consumer) {
			vt_debug(VT_DEBUG_CMD, "vt [%d] set solid color, no consumer", id);
			return -ENOTCONN;
		}
	} else {
		if (data->video_cmd != VT_VIDEO_SET_SOURCE_CROP &&
				data->video_cmd != VT_VIDEO_SET_DISPLAY_FRAME) {
			if (!instance || session->role != VT_ROLE_PRODUCER)
				return -EINVAL;
		}
	}

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	mutex_lock(&instance->cmd_lock);
	cmd->cmd = data->video_cmd;
	cmd->cmd_data = data->video_cmd_data;
	cmd->client_id = session->pid;
	cmd->rect = data->rect;
	if (cmd->cmd == VT_VIDEO_SET_GAME_MODE)
		instance->mode =
		    cmd->cmd_data ?  VT_MODE_GAME : VT_MODE_NONE_BLOCK;
	kfifo_put(&instance->fifo_cmd, cmd);

	vt_debug(VT_DEBUG_CMD, "vt [%d] send cmd:%d ", instance->id, cmd->cmd);

	if (cmd->cmd == VT_VIDEO_SET_SOURCE_CROP)
		instance->backup_sourcecrop = cmd->rect;
	if (cmd->cmd == VT_VIDEO_SET_DISPLAY_FRAME)
		instance->backup_displayframe = cmd->rect;

	if (cmd->cmd == VT_VIDEO_SET_SOURCE_CROP ||
		cmd->cmd == VT_VIDEO_SET_DISPLAY_FRAME)
		vt_debug(VT_DEBUG_CMD, "rect (%d %d %d %d)\n",
			 cmd->rect.left, cmd->rect.top,
			 cmd->rect.right, cmd->rect.bottom);
	else
		vt_debug(VT_DEBUG_CMD, "data:%d\n", cmd->cmd_data);
	mutex_unlock(&instance->cmd_lock);

	mutex_lock(&instance->lock);
	wake_up_interruptible(&instance->wait_cmd);
	if (instance->consumer) {
		instance->consumer->cmd_status++;
		wake_up_interruptible(&instance->consumer->wait_cmd);
	}
	mutex_unlock(&instance->lock);

	return 0;
}

static int vt_has_cmd(struct vt_instance *instance)
{
	int ret = !kfifo_is_empty(&instance->fifo_cmd);
	return ret;
}

static int vt_recv_cmd_process(struct vt_ctrl_data *data,
			       struct vt_session *session)
{
	struct vt_dev *dev = session->dev;
	struct vt_instance *instance;
	struct vt_cmd *vcmd = NULL;
	int id = data->tunnel_id;
	int ret;

	instance = idr_find(&dev->instance_idr, id);

	if (!instance || session->role != VT_ROLE_CONSUMER)
		return -EINVAL;

	/* empty need wait */
	if (kfifo_is_empty(&instance->fifo_cmd)) {
		if (session->mode != VT_MODE_BLOCK)
			return -EAGAIN;

		ret = wait_event_interruptible_timeout(instance->wait_cmd,
						       vt_has_cmd(instance),
						       msecs_to_jiffies(VT_CMD_WAIT_MS));

		/* timeout */
		if (ret == 0)
			return -EAGAIN;
	}

	mutex_lock(&instance->cmd_lock);
	ret = kfifo_get(&instance->fifo_cmd, &vcmd);
	mutex_unlock(&instance->cmd_lock);
	if (!ret || !vcmd) {
		pr_err("vt [%d] recv cmd got null\n", instance->id);
		return -EAGAIN;
	}

	vt_debug(VT_DEBUG_CMD, "vt [%d] recv cmd:%d ", instance->id, vcmd->cmd);

	if (vcmd->cmd == VT_VIDEO_SET_SOURCE_CROP ||
		vcmd->cmd == VT_VIDEO_SET_DISPLAY_FRAME)
		vt_debug(VT_DEBUG_CMD, "rect (%d %d %d %d)\n",
			 vcmd->rect.left, vcmd->rect.top,
			 vcmd->rect.right, vcmd->rect.bottom);
	else
		vt_debug(VT_DEBUG_CMD, "data:%d\n", vcmd->cmd_data);

	data->video_cmd = vcmd->cmd;
	data->video_cmd_data = vcmd->cmd_data;
	data->client_id = vcmd->client_id;
	data->rect = vcmd->rect;

	if (vcmd->cmd == VT_VIDEO_SET_GAME_MODE) {
		if (!vcmd->cmd_data)
			session->mode = VT_MODE_NONE_BLOCK;
		else
			session->mode = VT_MODE_GAME;

		vt_debug(VT_DEBUG_USER, "vt [%d] set mode to:%s\n",
			 instance->id,
			 vt_debug_mode_status_to_string(session->mode));
	}

	/* free the vt_cmd buffer allocated in vt_send_cmd_process() */
	kfree(vcmd);

	return 0;
}

/*
 * buffer_or_cmd indicate poll buffer or cmd, 1 is buffer and 0 is cmd
 */
static int vt_poll_ready(struct vt_session *session, int buffer_or_cmd)
{
	struct vt_dev *dev = session->dev;
	struct vt_instance *instance = NULL;
	struct rb_node *n = NULL;
	int size = 0;

	mutex_lock(&dev->instance_lock);
	for (n = rb_first(&dev->instances); n; n = rb_next(n)) {
		instance = rb_entry(n, struct vt_instance, node);
		mutex_lock(&instance->lock);
		if (instance->producer && instance->producer == session) {
			size += kfifo_len(&instance->fifo_to_producer);
		} else if (instance->consumer &&
			   instance->consumer == session) {
			if (buffer_or_cmd == 1)
				size += kfifo_len(&instance->fifo_to_consumer);
			if (buffer_or_cmd == 0)
				size += kfifo_len(&instance->fifo_cmd);
		}
		mutex_unlock(&instance->lock);
	}
	mutex_unlock(&dev->instance_lock);

	return size;
}

static int vt_poll_cmd_process(struct vt_ctrl_data *data,
			       struct vt_session *session)
{
	int time_out = data->video_cmd_data;
	int ret = 0;

	if (vt_poll_ready(session, 0) > 0)
		return POLLIN | POLLRDNORM;

	/* no ready cmd */
	session->cmd_status = 0;
	ret = wait_event_interruptible_timeout(session->wait_cmd,
					       session->cmd_status > 0,
					       msecs_to_jiffies(time_out));
	/* timeout */
	if (ret == 0)
		return 0;

	if (vt_poll_ready(session, 0) > 0)
		return POLLIN | POLLRDNORM;
	else
		return -EAGAIN;
}

static int vt_cancel_buffer_process(struct vt_ctrl_data *data,
				    struct vt_session *session)
{
	struct vt_dev *dev = session->dev;
	struct vt_instance *instance = NULL;
	struct vt_buffer *buffer = NULL;
	int id = data->tunnel_id;

	instance = idr_find(&dev->instance_idr, id);

	if (!instance || !instance->producer)
		return -EINVAL;
	if (instance->producer && instance->producer != session)
		return -EINVAL;

	mutex_lock(&instance->lock);
	while (kfifo_get(&instance->fifo_to_consumer, &buffer)) {
		if (buffer->file_buffer) {
			fput(buffer->file_buffer);
			instance->fcount--;
			buffer->item.buffer_status = VT_BUFFER_RELEASE;

			vt_debug(VT_DEBUG_FILE,
				 "vt [%d] cancel buffer file(%px) buffer(%p), fcount=%d\n",
				 instance->id,
				 buffer->file_buffer,
				 buffer, instance->fcount);
		}
		kfifo_put(&instance->fifo_to_producer, buffer);
	}
	mutex_unlock(&instance->lock);

	return 0;
}

static int vt_ctrl_process(struct vt_ctrl_data *data,
			   struct vt_session *session)
{
	int id = data->tunnel_id;
	int ret = 0;

	if (id < 0 || id > MAX_VIDEO_TUNNEL_ID)
		return -EINVAL;
	if (data->role == VT_ROLE_INVALID)
		return -EINVAL;

	switch (data->ctrl_cmd) {
	case VT_CTRL_CONNECT: {
		ret = vt_connect_process(data, session);
		break;
	}
	case VT_CTRL_DISCONNECT: {
		ret = vt_disconnect_process(data, session);
		break;
	}
	case VT_CTRL_SEND_CMD: {
		ret = vt_send_cmd_process(data, session);
		break;
	}
	case VT_CTRL_RECV_CMD: {
		ret = vt_recv_cmd_process(data, session);
		break;
	}
	case VT_CTRL_SET_NONBLOCK_MODE: {
		session->mode = VT_MODE_NONE_BLOCK;
		break;
	}
	case VT_CTRL_SET_BLOCK_MODE: {
		session->mode = VT_MODE_BLOCK;
		break;
	}
	case VT_CTRL_POLL_CMD: {
		ret = vt_poll_cmd_process(data, session);
		break;
	}
	case VT_CTRL_CANCEL_BUFFER: {
		ret = vt_cancel_buffer_process(data, session);
		break;
	}
	default:
		pr_err("unknown videotunnel cmd:%d\n", data->ctrl_cmd);
		return -EINVAL;
	}

	return ret;
}

static struct vt_buffer *vt_buffer_get_locked(struct vt_instance *instance, int key)
{
	struct vt_buffer *buffer = NULL;
	int i;

	for (i = 0; i < VT_POOL_SIZE; i++) {
		buffer = &instance->vt_buffers[i];

		if (buffer->item.buffer_status == VT_BUFFER_ACQUIRE &&
				buffer->buffer_fd_con == key)
			return buffer;
	}

	return NULL;
}

static int vt_has_buffer(struct vt_instance *instance, enum vt_role_e role)
{
	int ret = 0;

	if (role == VT_ROLE_PRODUCER)
		ret = !kfifo_is_empty(&instance->fifo_to_producer);
	else
		ret = !kfifo_is_empty(&instance->fifo_to_consumer);

	return ret;
}

static struct vt_buffer *vt_get_free_buffer(struct vt_instance *instance)
{
	struct vt_buffer *buffer = NULL;
	int i, status;

	mutex_lock(&instance->lock);
	for (i = 0; i < VT_POOL_SIZE; i++) {
		status = instance->vt_buffers[i].item.buffer_status;
		if (status == VT_BUFFER_FREE || status == VT_BUFFER_DEQUEUE) {
			buffer = &instance->vt_buffers[i];
			buffer->file_buffer = NULL;
			buffer->file_fence = NULL;
			buffer->buffer_fd_con = -1;
			break;
		}
	}
	mutex_unlock(&instance->lock);

	return buffer;
}

static int vt_queue_buffer_process(struct vt_buffer_data *data,
				   struct vt_session *session,
				   struct file *vt_buffer_file)
{
	struct vt_dev *dev = session->dev;
	struct vt_instance *instance =
		idr_find(&dev->instance_idr, data->tunnel_id);
	struct vt_buffer *buffer = NULL;

	if (!instance || !instance->producer)
		return -EINVAL;
	if (instance->producer && instance->producer != session)
		return -EINVAL;

	/* in game mode, but has no consumer */
	if (instance->mode == VT_MODE_GAME && !instance->consumer) {
		vt_debug(VT_DEBUG_BUFFERS,
			"vt [%d] game mode, but no consumer\n", instance->id);
		return -ENOTCONN;
	}

	vt_debug(VT_DEBUG_BUFFERS,
		 "vt [%d] queuebuffer start\n", instance->id);

	buffer = vt_get_free_buffer(instance);
	if (!buffer)
		return -ENOMEM;

	if (vt_buffer_file) {
		buffer->file_buffer = vt_buffer_file;
		get_file(buffer->file_buffer);
	} else {
		buffer->file_buffer = fget(data->buffer_fd);
	}

	if (!buffer->file_buffer)
		return -EBADF;

	mutex_lock(&instance->lock);
	instance->fcount++;
	dev->state.buffer_get++;
	instance->state.buffer_get++;

	buffer->buffer_fd_pro = data->buffer_fd;
	buffer->buffer_fd_con = -1;
	buffer->session_pro = session;
	buffer->cid_pro = session->cid;
	buffer->item = *data;
	buffer->item.buffer_status = VT_BUFFER_QUEUE;

	vt_debug(VT_DEBUG_FILE,
		 "vt [%d] queuebuffer fget file(%px) buffer(%p) buffer session(%p) fcount=%d\n",
		 instance->id, buffer->file_buffer,
		 buffer, buffer->session_pro, instance->fcount);

	kfifo_put(&instance->fifo_to_consumer, buffer);

	if (instance->consumer) {
		wake_up_interruptible(&instance->wait_consumer);
		wake_up_interruptible(&instance->consumer->wait_consumer);
	}
	mutex_unlock(&instance->lock);

	dev->state.queue_count++;
	instance->state.queue_count++;

	vt_debug(VT_DEBUG_BUFFERS,
		 "vt [%d] queuebuffer pfd: %d, buffer(%p) buffer file(%px) timestamp(%lld), now(%lld)\n",
		 instance->id, buffer->buffer_fd_pro,
		 buffer, buffer->file_buffer,
		 buffer->item.time_stamp, ktime_to_us(ktime_get()));

	return 0;
}

static int vt_instance_buffer_size(struct vt_instance *instance)
{
	int size = 0;
	int i;

	mutex_lock(&instance->lock);
	for (i = 0; i < VT_POOL_SIZE; i++) {
		struct vt_buffer *buffer = &instance->vt_buffers[i];
		int status = buffer->item.buffer_status;

		if (status == VT_BUFFER_QUEUE || status == VT_BUFFER_ACQUIRE ||
				status == VT_BUFFER_RELEASE)
			size++;
	}
	mutex_unlock(&instance->lock);

	return size;
}

static int vt_dequeue_buffer_process(struct vt_buffer_data *data,
				     struct vt_session *session,
				     struct file **vt_buffer_file,
				     struct file **vt_fence_file)
{
	struct vt_dev *dev = session->dev;
	struct vt_instance *instance =
		idr_find(&dev->instance_idr, data->tunnel_id);
	struct vt_buffer *buffer = NULL;
	int ret = -1;
	int fd = -1;
	struct sync_file *sync_file = NULL;
	struct dma_fence *fence_obj = NULL;

	if (!instance || !instance->producer)
		return -EINVAL;
	if (instance->producer && instance->producer != session)
		return -EINVAL;

	/* empty need wait */
	if (kfifo_is_empty(&instance->fifo_to_producer)) {
		ret =
		  wait_event_interruptible_timeout(instance->wait_producer,
						   vt_has_buffer(instance, VT_ROLE_PRODUCER),
						   msecs_to_jiffies(VT_MAX_WAIT_MS));
		/* timeout */
		if (ret == 0) {
			if (!instance->consumer) {
				vt_debug(VT_DEBUG_BUFFERS,
					"vt [%d] dequeue buffer, no consumer\n",
					instance->id);
				return -ENOTCONN;
			} else {
				return -EAGAIN;
			}
		}
	}

	if (vt_instance_buffer_size(instance) < dev->limit)
		return -EAGAIN;

	mutex_lock(&instance->lock);
	ret = kfifo_get(&instance->fifo_to_producer, &buffer);
	if (!ret || !buffer) {
		pr_err("vt [%d] dequeue buffer got null buffer ret(%d)\n",
		       instance->id, ret);
		mutex_unlock(&instance->lock);
		return -EAGAIN;
	}

	buffer->item.buffer_status = VT_BUFFER_DEQUEUE;
	/* it's previous connect buffer */
	if (buffer->cid_pro != session->cid) {
		if (buffer->file_fence) {
			fput(buffer->file_fence);
			dev->state.fence_put++;
			instance->state.fence_put++;
		}
		dev->state.dequeue_invalid++;
		instance->state.dequeue_invalid++;
		pr_info("vt [%d] dequeuebuffer, previous connect buffer!!\n",
			    instance->id);

		mutex_unlock(&instance->lock);
		return -EAGAIN;
	}

	/* only install fence fd if vt_fence_file is null */
	if (buffer->file_fence && !vt_fence_file) {
		fd = get_unused_fd_flags(O_CLOEXEC);
		if (fd < 0) {
			pr_info("vt [%d] dequeuebuffer install fence fd error, Suspected fd leak!!\n",
			       instance->id);
			/* could not get unused fd, put the file fence */
			fput(buffer->file_fence);
			dev->state.fence_put++;
			instance->state.fence_put++;
			mutex_unlock(&instance->lock);
			return -ENOMEM;
		}

		fd_install(fd, buffer->file_fence);

		sync_file = (struct sync_file *)buffer->file_fence->private_data;
		fence_obj = sync_file_get_fence(fd);
		if (fence_obj) {
			vt_debug(VT_DEBUG_FILE, "vt [%d] dequeuebuffer fence file=%px, sync_file=%px, seqno=%lld\n",
			instance->id, buffer->file_fence,
			sync_file, fence_obj->seqno);
			dma_fence_put(fence_obj);
		}

		vt_debug(VT_DEBUG_FILE,
			"vt [%d] dequeubuffer fence file(%px) install fence fd(%d) buffer(%p)\n",
			instance->id, buffer->file_fence, fd, buffer);
	}

	buffer->item.fence_fd = fd;
	buffer->item.buffer_fd = buffer->buffer_fd_pro;
	buffer->item.tunnel_id = instance->id;
	buffer->item.buffer_status = VT_BUFFER_DEQUEUE;
	if (vt_buffer_file)
		*vt_buffer_file = buffer->file_buffer;
	if (vt_fence_file)
		*vt_fence_file = buffer->file_fence;

	/* return the buffer */
	*data = buffer->item;
	mutex_unlock(&instance->lock);

	dev->state.dequeue_count++;
	instance->state.dequeue_count++;

	vt_debug(VT_DEBUG_BUFFERS, "vt [%d] dequeuebuffer buffer(%p) end pfd(%d) cfd(%d) fence fd(%d) timestamp(%lld)\n",
		 instance->id, buffer, buffer->buffer_fd_pro, buffer->buffer_fd_con,
		 buffer->item.fence_fd, buffer->item.time_stamp);

	return 0;
}

static int vt_acquire_buffer_process(struct vt_buffer_data *data,
				     struct vt_session *session)
{
	struct vt_dev *dev = session->dev;
	struct vt_instance *instance =
		idr_find(&dev->instance_idr, data->tunnel_id);
	struct vt_buffer *buffer = NULL;
	int fd, ret = -1;

	if (!instance || !instance->consumer)
		return -EINVAL;
	if (instance->consumer && instance->consumer != session)
		return -EINVAL;

	/* empty need wait */
	if (kfifo_is_empty(&instance->fifo_to_consumer)) {
		if (session->mode != VT_MODE_BLOCK)
			return -EAGAIN;

		ret = wait_event_interruptible_timeout(instance->wait_consumer,
						       vt_has_buffer(instance, VT_ROLE_CONSUMER),
						       msecs_to_jiffies(VT_MAX_WAIT_MS));

		/* timeout */
		if (ret == 0)
			return -EAGAIN;
	}

	mutex_lock(&instance->lock);
	ret = kfifo_get(&instance->fifo_to_consumer, &buffer);
	if (!ret || !buffer) {
		pr_err("vt [%d] acquirebuffer got null buffer\n", instance->id);
		mutex_unlock(&instance->lock);
		return -EAGAIN;
	}

	/* get the fd in consumer */
	if (buffer->buffer_fd_con < 0) {
		fd = get_unused_fd_flags(O_CLOEXEC);
		if (fd < 0) {
			/* back to producer */
			pr_info("vt [%d] acquirebuffer install fd error\n",
			       instance->id);
			buffer->item.buffer_status = VT_BUFFER_RELEASE;
			if (buffer->file_buffer) {
				fput(buffer->file_buffer);
				instance->fcount--;
				dev->state.buffer_put++;
				instance->state.buffer_put++;

				vt_debug(VT_DEBUG_FILE,
					 "vt [%d] acquirebuffer install fd error file(%px) buffer(%p) fcount=%d\n",
					 instance->id, buffer->file_buffer,
					 buffer, instance->fcount);
			}
			kfifo_put(&instance->fifo_to_producer, buffer);
			mutex_unlock(&instance->lock);
			return -ENOMEM;
		}

		fd_install(fd, buffer->file_buffer);
		buffer->buffer_fd_con = fd;
		vt_debug(VT_DEBUG_BUFFERS,
			 "vt [%d] acquirebuffer install buffer fd:%d\n",
			 instance->id, fd);
	}

	buffer->item.buffer_fd = buffer->buffer_fd_con;
	buffer->item.tunnel_id = instance->id;
	buffer->item.buffer_status = VT_BUFFER_ACQUIRE;

	/* return the buffer */
	*data = buffer->item;
	mutex_unlock(&instance->lock);

	dev->state.acquire_count++;
	instance->state.acquire_count++;

	vt_debug(VT_DEBUG_BUFFERS,
		 "vt [%d] acquirebuffer pfd: %d, cfd: %d, buffer(%p) buffer file(%px), timestamp(%lld), now(%lld)\n",
		 instance->id, buffer->buffer_fd_pro,
		 buffer->buffer_fd_con, buffer, buffer->file_buffer,
		 buffer->item.time_stamp, ktime_to_us(ktime_get()));

	return 0;
}

static int vt_release_buffer_process(struct vt_buffer_data *data,
				     struct vt_session *session)
{
	struct vt_dev *dev = session->dev;
	struct vt_instance *instance =
		idr_find(&dev->instance_idr, data->tunnel_id);
	struct vt_buffer *buffer = NULL;

	if (!instance || !instance->consumer)
		return -EINVAL;
	if (instance->consumer && instance->consumer != session)
		return -EINVAL;

	if (data->buffer_fd < 0)
		return -EINVAL;

	mutex_lock(&instance->lock);
	buffer = vt_buffer_get_locked(instance, data->buffer_fd);
	if (!buffer) {
		pr_err("vt [%d] releasebuffer cann't find buffer:%d\n",
		      instance->id, data->buffer_fd);
		mutex_unlock(&instance->lock);
		return -EINVAL;
	}

	instance->fcount--;
	dev->state.buffer_close++;
	instance->state.buffer_close++;

	vt_debug(VT_DEBUG_FILE,
		 "vt [%d] releasebuffer file(%px) buffer(%p) buffer session(%p) fcount=%d\n",
		 instance->id, buffer->file_buffer, buffer,
		 buffer->session_pro, instance->fcount);

	buffer->item.buffer_fd = buffer->buffer_fd_pro;
	buffer->item.buffer_status = VT_BUFFER_RELEASE;

	/* if producer has disconnect */
	if (!instance->producer || (buffer->session_pro &&
	    buffer->session_pro != instance->producer)) {
		vt_debug(VT_DEBUG_BUFFERS,
			 "vt [%d] releasebuffer buffer, no producer or previous producer\n",
			 instance->id);
		buffer->item.buffer_status = VT_BUFFER_FREE;
		dev->state.release_invalid++;
		instance->state.release_invalid++;
		mutex_unlock(&instance->lock);
		return 0;
	}

	buffer->file_fence = NULL;
	if (data->fence_fd >= 0) {
		buffer->file_fence = fget(data->fence_fd);
		dev->state.fence_get++;
		instance->state.fence_get++;
	} else {
		dev->state.null_fence++;
		instance->state.null_fence++;
	}

	if (!buffer->file_fence)
		vt_debug(VT_DEBUG_BUFFERS,
			 "vt [%d] releasebuffer fence file is null\n",
			 instance->id);
	else
		vt_debug(VT_DEBUG_FILE,
			"vt [%d] releasebuffer fence file(%px) fence fd(%d)\n",
			instance->id, buffer->file_fence, data->fence_fd);

	kfifo_put(&instance->fifo_to_producer, buffer);
	mutex_unlock(&instance->lock);

	if (instance->producer)
		wake_up_interruptible(&instance->wait_producer);

	vt_debug(VT_DEBUG_BUFFERS,
		 "vt [%d] releasebuffer pfd: %d, cfd: %d, buffer(%p) buffer file(%px) timestamp(%lld)\n",
		 instance->id, buffer->buffer_fd_pro,
		 buffer->buffer_fd_con, buffer, buffer->file_buffer, buffer->item.time_stamp);

	dev->state.release_count++;
	instance->state.release_count++;

	return 0;
}

static int vt_set_vsync_info(struct vt_display_vsync *data,
			     struct vt_session *session)
{
	struct vt_dev *dev = session->dev;

	if (session->role != VT_ROLE_CONSUMER || !dev)
		return -EINVAL;

	mutex_lock(&dev->vsync_lock);
	dev->vsync_timestamp = data->timestamp;
	dev->vsync_period = data->period;
	mutex_unlock(&dev->vsync_lock);

	vt_debug(VT_DEBUG_VSYNC,
		 "vt set vsync timestamp:%llu period:%u\n",
		 data->timestamp, data->period);

	return 0;
}

static int vt_get_vsync_info(struct vt_display_vsync *data,
			     struct vt_session *session)
{
	struct vt_dev *dev = session->dev;

	if (session->role != VT_ROLE_PRODUCER || !dev)
		return -EINVAL;

	mutex_lock(&dev->vsync_lock);
	data->timestamp = dev->vsync_timestamp;
	data->period = dev->vsync_period;
	mutex_unlock(&dev->vsync_lock);

	vt_debug(VT_DEBUG_VSYNC,
		 "vt [%d] get vsync timestamp:%llu period:%u\n",
		 data->tunnel_id, data->timestamp, data->period);

	return 0;
}

static unsigned int vt_ioctl_dir(unsigned int cmd)
{
	switch (cmd) {
	case VT_IOC_ALLOC_ID:
	case VT_IOC_DEQUEUE_BUFFER:
	case VT_IOC_ACQUIRE_BUFFER:
	case VT_IOC_CTRL:
	case VT_IOC_GET_VSYNCTIME:
		return _IOC_READ;
	case VT_IOC_QUEUE_BUFFER:
	case VT_IOC_RELEASE_BUFFER:
	case VT_IOC_FREE_ID:
	case VT_IOC_SET_VSYNCTIME:
		return _IOC_WRITE;
	default:
		return _IOC_DIR(cmd);
	}
}

static long vt_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	union vt_ioctl_arg data;
	struct vt_session *session = filp->private_data;
	unsigned int dir = vt_ioctl_dir(cmd);

	if (_IOC_SIZE(cmd) > sizeof(data))
		return -EINVAL;

	if (copy_from_user(&data, (void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	switch (cmd) {
	case VT_IOC_ALLOC_ID: {
		ret = vt_alloc_id_process(&data.alloc_data, session);
		break;
	}
	case VT_IOC_FREE_ID: {
		ret = vt_free_id_process(&data.alloc_data, session);
		break;
	}
	case VT_IOC_CTRL:
		ret = vt_ctrl_process(&data.ctrl_data, session);
		break;
	case VT_IOC_QUEUE_BUFFER:
		ret = vt_queue_buffer_process(&data.buffer_data, session, NULL);
		break;
	case VT_IOC_DEQUEUE_BUFFER:
		ret = vt_dequeue_buffer_process(&data.buffer_data,
						session,
						NULL,
						NULL);
		break;
	case VT_IOC_RELEASE_BUFFER:
		ret = vt_release_buffer_process(&data.buffer_data, session);
		break;
	case VT_IOC_ACQUIRE_BUFFER:
		ret = vt_acquire_buffer_process(&data.buffer_data, session);
		break;
	case VT_IOC_SET_VSYNCTIME:
		ret = vt_set_vsync_info(&data.vsync_data, session);
		break;
	case VT_IOC_GET_VSYNCTIME:
		ret = vt_get_vsync_info(&data.vsync_data, session);
		break;
	default:
		return -ENOTTY;
	}

	if (dir & _IOC_READ) {
		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	return ret;
}

/*
 * support poll cmds and buffer.
 * poll cmds return POLLWRNORM | POLLOUT
 * poll buffer return POLLRDNORM | POLLIN
 * if not game mode ret with POLLRDBAND
 */
static __poll_t vt_poll(struct file *filp, struct poll_table_struct *wait)
{
	__poll_t ret = 0;
	struct vt_session *session = filp->private_data;

	/* not connected */
	if (session->role == VT_ROLE_INVALID)
		return POLLERR;

	if (session->role == VT_ROLE_PRODUCER)
		poll_wait(filp, &session->wait_producer, wait);
	else if (session->role == VT_ROLE_CONSUMER)
		poll_wait(filp, &session->wait_consumer, wait);

	/* has cmds ready*/
	if (vt_poll_ready(session, 0) > 0)
		ret |= POLLOUT | POLLWRNORM;

	/* has buffer ready */
	if (vt_poll_ready(session, 1) > 0)
		ret |= POLLIN | POLLRDNORM;

	if (session->mode != VT_MODE_GAME && ret != 0)
		ret |= POLLRDBAND;

	return ret;
}

struct vt_session *vt_session_create(const char *name)
{
	return vt_session_create_internal(vdev, name);
}
EXPORT_SYMBOL(vt_session_create);

int vt_alloc_id(struct vt_session *session, int *tunnel_id)
{
	int ret = 0;
	struct vt_alloc_id_data data = { 0 };

	ret = vt_alloc_id_process(&data, session);

	if (ret < 0)
		return ret;

	*tunnel_id = data.tunnel_id;
	return ret;
}
EXPORT_SYMBOL(vt_alloc_id);

int vt_free_id(struct vt_session *session, int tunnel_id)
{
	struct vt_alloc_id_data data = { 0 };

	data.tunnel_id = tunnel_id;
	return vt_free_id_process(&data, session);
}
EXPORT_SYMBOL(vt_free_id);

int vt_producer_connect(struct vt_session *session, int tunnel_id)
{
	struct vt_ctrl_data data = { 0 };

	data.tunnel_id = tunnel_id;
	data.role = VT_ROLE_PRODUCER;

	return vt_connect_process(&data, session);
}
EXPORT_SYMBOL(vt_producer_connect);

int vt_producer_disconnect(struct vt_session *session, int tunnel_id)
{
	struct vt_ctrl_data data = { 0 };

	data.tunnel_id = tunnel_id;
	data.role = VT_ROLE_PRODUCER;

	return vt_disconnect_process(&data, session);
}
EXPORT_SYMBOL(vt_producer_disconnect);

int vt_queue_buffer(struct vt_session *session, int tunnel_id,
		    struct file *buffer_file, int fence_fd, int64_t time_stamp)
{
	struct vt_buffer_data data = { 0 };

	data.tunnel_id = tunnel_id;
	data.buffer_fd = -1;
	data.fence_fd = fence_fd;
	data.time_stamp = time_stamp;

	return vt_queue_buffer_process(&data, session, buffer_file);
}
EXPORT_SYMBOL(vt_queue_buffer);

int vt_dequeue_buffer(struct vt_session *session, int tunnel_id,
		      struct file **buffer_file, struct file **fence_file)
{
	struct vt_buffer_data data = { 0 };
	int ret;

	data.tunnel_id = tunnel_id;
	ret = vt_dequeue_buffer_process(&data, session, buffer_file, fence_file);

	return ret;
}
EXPORT_SYMBOL(vt_dequeue_buffer);

int vt_send_cmd(struct vt_session *session, int tunnel_id,
		enum vt_video_cmd_e cmd, int cmd_data)
{
	struct vt_ctrl_data data = { 0 };

	data.tunnel_id = tunnel_id;
	data.video_cmd = cmd;
	data.video_cmd_data = cmd_data;

	return vt_send_cmd_process(&data, session);
}
EXPORT_SYMBOL(vt_send_cmd);

static const struct file_operations vt_fops = {
	.owner = THIS_MODULE,
	.open = vt_open,
	.release = vt_release,
	.unlocked_ioctl = vt_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vt_ioctl,
#endif
	.poll = vt_poll,
};

static int vt_probe(struct platform_device *pdev)
{
	int ret;

	vdev = kzalloc(sizeof(*vdev), GFP_KERNEL);
	if (!vdev)
		return -ENOMEM;

	vdev->dev_name = DEVICE_NAME;
	vdev->mdev.minor = MISC_DYNAMIC_MINOR;
	vdev->mdev.name = DEVICE_NAME;
	vdev->mdev.fops = &vt_fops;

	ret = misc_register(&vdev->mdev);
	if (ret) {
		pr_err("videotunnel: misc_register fail.\n");
		goto failed_alloc_dev;
	}

	mutex_init(&vdev->instance_lock);
	mutex_init(&vdev->vsync_lock);
	idr_init(&vdev->instance_idr);
	vdev->instances = RB_ROOT;
	init_rwsem(&vdev->session_lock);
	vdev->sessions = RB_ROOT;

	mutex_init(&debugfs_mutex);
	vdev->debug_root = debugfs_create_dir("videotunnel", NULL);

	if (!vdev->debug_root) {
		pr_err("videotunnel: failed to create debugfs root directory.\n");
	} else {
		vdev->state.debug_root =
		debugfs_create_file("state", 0664, vdev->debug_root,
					    &vdev->state,
					    &debug_state_fops);

		debugfs_create_file("limit", 0644, vdev->debug_root,
					    vdev,
					    &debug_limit_fops);
	}

	return 0;

failed_alloc_dev:
	kfree(vdev);

	return ret;
}

static int vt_remove(struct platform_device *pdev)
{
	idr_destroy(&vdev->instance_idr);
	debugfs_remove_recursive(vdev->debug_root);
	misc_deregister(&vdev->mdev);
	kfree(vdev);

	return 0;
}

static const struct of_device_id meson_vt_match[] = {
	{.compatible = "amlogic, meson_videotunnel"},
	{},
};

static struct platform_driver meson_vt_driver = {
	.driver = {
		.name = "meson_videotunnel_driver",
		.owner = THIS_MODULE,
		.of_match_table = meson_vt_match,
	},
	.probe = vt_probe,
	.remove = vt_remove,
};

int __init meson_videotunnel_init(void)
{
	pr_info("videotunnel init\n");

	if (platform_driver_register(&meson_vt_driver)) {
		pr_err("failed to register videotunnel\n");
		return -ENODEV;
	}

	return 0;
}

void __exit meson_videotunnel_exit(void)
{
	platform_driver_unregister(&meson_vt_driver);
}

