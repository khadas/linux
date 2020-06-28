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
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/debugfs.h>
#include <linux/random.h>
#include <linux/amlogic/aml_sync_api.h>

#include <asm-generic/bug.h>

#include "videotunnel_priv.h"

#define DEVICE_NAME "videotunnel"
#define MAX_VIDEO_INSTANCE_NUM 16
#define VIDEOTUNNEL_CMD_FIFO_SIZE 256

static struct videotunnel_dev *vdev;
static struct mutex debugfs_mutex;

enum {
	VT_DEBUG_NONE             = 0,
	VT_DEBUG_USER             = 1U << 0,
	VT_DEBUG_BUFFERS          = 1U << 1,
};

static u32 videotunnel_debug_mask = VT_DEBUG_NONE;
module_param_named(debug_mask, videotunnel_debug_mask, uint, 0644);

#define videotunnel_debug(mask, x...) \
	do { \
		if (videotunnel_debug_mask & (mask)) \
			pr_info_ratelimited(x); \
	} while (0)

static int videotunnel_debug_instance_show(struct seq_file *s, void *unused)
{
	struct videotunnel_instance *instance = s->private;
	int size_to_con = kfifo_len(&instance->fifo_to_consumer);
	int size_to_pro = kfifo_len(&instance->fifo_to_producer);
	int ref_count = atomic_read(&instance->ref.refcount.refs);

	mutex_lock(&debugfs_mutex);
	seq_printf(s, "tunnel id=%d, ref=%d\n", instance->id, ref_count);
	seq_puts(s, "-----------------------------------------------\n");
	if (instance->consumer)
		seq_printf(s, "consumer session (%s) %p\n",
			   instance->consumer->display_name, instance->consumer);
	if (instance->producer)
		seq_printf(s, "producer session (%s) %p\n",
			   instance->producer->display_name, instance->producer);
	seq_puts(s, "-----------------------------------------------\n");
	mutex_unlock(&debugfs_mutex);

	seq_printf(s, "producer transfer to consumer fifo size:%d\n",
		   size_to_con);
	seq_printf(s, "consumer transfer to producer fifo size:%d\n",
		   size_to_pro);
	seq_puts(s, "-----------------------------------------------\n");

	return 0;
}

static int videotunnel_debug_instance_open(struct inode *inode,
					   struct file *file)
{
	return single_open(file, videotunnel_debug_instance_show, inode->i_private);
}

static const struct file_operations debug_instance_fops = {
	.open = videotunnel_debug_instance_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int videotunnel_debug_session_show(struct seq_file *s, void *unused)
{
	struct videotunnel_session *session = s->private;
	struct rb_node *n = NULL;

	mutex_lock(&debugfs_mutex);
	seq_printf(s, "session(%s) %p role %s cid %ld:\n",
		   session->display_name, session,
		   session->role == VT_ROLE_PRODUCER ?
		   "producer" : (session->role == VT_ROLE_CONSUMER ?
		   "consumer" : "invalid"), session->cid);
	seq_puts(s, "-----------------------------------------------\n");
	seq_puts(s, "session buffers:\n");
	mutex_lock(&session->lock);
	for (n = rb_first(&session->buffers); n; n = rb_next(n)) {
		struct videotunnel_buffer *buffer = rb_entry(n,
				struct videotunnel_buffer, node);

		seq_printf(s, "    tunnel id:%d, buffer fd:%d, status:%d\n",
			   buffer->item.tunnel_id,
			   session->role == VT_ROLE_PRODUCER
			   ? buffer->buffer_fd_pro : buffer->buffer_fd_con,
			   buffer->item.buffer_status);
	}
	seq_puts(s, "-----------------------------------------------\n");
	mutex_unlock(&session->lock);
	mutex_unlock(&debugfs_mutex);

	return 0;
}

static int videotunnel_debug_session_open(struct inode *inode,
					  struct file *file)
{
	return single_open(file, videotunnel_debug_session_show, inode->i_private);
}

static const struct file_operations debug_session_fops = {
	.open = videotunnel_debug_session_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int videotunnel_close_fd(struct videotunnel_session *session,
				unsigned int fd)
{
	int ret;

	if (!session->task)
		return -ESRCH;

	ret = __close_fd(session->task->files, fd);
	/* can't restart close syscall because file table entry was cleared */
	if (unlikely(ret == -ERESTARTSYS ||
		     ret == -ERESTARTNOINTR ||
		     ret == -ERESTARTNOHAND ||
		     ret == -ERESTART_RESTARTBLOCK))
		ret = -EINTR;

	return ret;
}

static void videotunnel_instance_destroy(struct kref *kref)
{
	struct videotunnel_instance *instance =
		container_of(kref, struct videotunnel_instance, ref);
	struct videotunnel_dev *dev = instance->dev;
	struct videotunnel_buffer *buffer = NULL;

	mutex_lock(&debugfs_mutex);
	mutex_lock(&dev->instance_lock);
	rb_erase(&instance->node, &dev->instances);
	if (idr_find(&dev->instance_idr, instance->id))
		idr_remove(&dev->instance_idr, instance->id);

	list_del(&instance->entry);
	videotunnel_debug(VT_DEBUG_USER,
			  "vt %d destroy\n", instance->id);

	mutex_lock(&instance->lock);
	if (!kfifo_is_empty(&instance->fifo_to_consumer)) {
		while (kfifo_get(&instance->fifo_to_consumer, &buffer)) {
			if (instance->producer) {
				mutex_lock(&instance->producer->lock);
				/* erase node from producer rb tree */
				rb_erase(&buffer->node,
					 &instance->producer->buffers);
				/* put file */
				if (buffer->file_buffer)
					fput(buffer->file_buffer);
				mutex_unlock(&instance->producer->lock);
			}
			kfree(buffer);
		}
	}

	if (!kfifo_is_empty(&instance->fifo_to_producer)) {
		while (kfifo_get(&instance->fifo_to_producer, &buffer)) {
			if (instance->consumer) {
				mutex_lock(&instance->consumer->lock);
				/* erase node from producer rb tree */
				rb_erase(&buffer->node,
					 &instance->consumer->buffers);
				if (buffer->file_fence)
					aml_sync_put_fence(buffer->file_fence);
				mutex_unlock(&instance->consumer->lock);
			}
			kfree(buffer);
		}
	}

	kfifo_free(&instance->fifo_to_consumer);
	kfifo_free(&instance->fifo_to_producer);
	debugfs_remove_recursive(instance->debug_root);
	mutex_unlock(&instance->lock);

	kfree(instance);
	mutex_unlock(&dev->instance_lock);
	mutex_unlock(&debugfs_mutex);
}

static void videotunnel_instance_get(struct videotunnel_instance *instance)
{
	kref_get(&instance->ref);
}

static int videotunnel_instance_put(struct videotunnel_instance *instance)
{
	if (!instance)
		return -ENOENT;

	return kref_put(&instance->ref, videotunnel_instance_destroy);
}

static struct videotunnel_instance *videotunnel_instance_create(struct videotunnel_dev *dev)
{
	struct videotunnel_instance *instance;
	struct videotunnel_instance *entry;
	struct rb_node **p;
	struct rb_node *parent = NULL;
	int status;

	instance = kzalloc(sizeof(*instance), GFP_KERNEL);
	if (!instance)
		return ERR_PTR(-ENOMEM);

	instance->dev = dev;
	mutex_init(&instance->lock);
	INIT_LIST_HEAD(&instance->entry);
	kref_init(&instance->ref);

	status = kfifo_alloc(&instance->fifo_to_consumer,
			     VIDEO_TUNNEL_POOL_SIZE, GFP_KERNEL);
	if (status)
		goto setup_fail;

	status = kfifo_alloc(&instance->fifo_to_producer,
			     VIDEO_TUNNEL_POOL_SIZE, GFP_KERNEL);
	if (status)
		goto setup_fail;

	init_waitqueue_head(&instance->wait_producer);
	init_waitqueue_head(&instance->wait_consumer);

	/* insert it to dev instances rb tree */
	mutex_lock(&dev->instance_lock);
	p = &dev->instances.rb_node;
	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct videotunnel_instance, node);

		if (instance < entry)
			p = &(*p)->rb_left;
		else if (instance > entry)
			p = &(*p)->rb_right;
		else
			break;
	}
	rb_link_node(&instance->node, parent, p);
	rb_insert_color(&instance->node, &dev->instances);
	mutex_unlock(&dev->instance_lock);

	return instance;

setup_fail:
	kfree(instance);
	return ERR_PTR(status);
}

static int videotunnel_get_session_serial(const struct rb_root *root,
					  const unsigned char *name)
{
	int serial = -1;
	struct rb_node *node;

	for (node = rb_first(root); node; node = rb_next(node)) {
		struct videotunnel_session *session =
		    rb_entry(node, struct videotunnel_session, node);

		if (strcmp(session->name, name))
			continue;
		serial = max(serial, session->display_serial);
	}
	return serial + 1;
}

static struct videotunnel_session *videotunnel_session_create(struct videotunnel_dev *dev,
							      const char *name)
{
	struct videotunnel_session *session;
	struct task_struct *task = NULL;
	struct rb_node **p;
	struct rb_node *parent = NULL;
	struct videotunnel_session *entry;

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

	INIT_LIST_HEAD(&session->instances_head);
	mutex_init(&session->lock);

	session->name = kstrdup(name, GFP_KERNEL);
	if (!session->name)
		goto err_free_session;

	down_write(&dev->session_lock);
	session->display_serial =
		videotunnel_get_session_serial(&dev->sessions, name);
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
		entry = rb_entry(parent, struct videotunnel_session, node);

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
static void videotunnel_session_trim(struct videotunnel_session *session,
				     struct videotunnel_instance *instance)
{
	struct videotunnel_buffer *buffer = NULL;

	if (!session || !instance)
		return;

	mutex_lock(&instance->lock);
	if (instance->producer && instance->producer == session) {
		while (kfifo_get(&instance->fifo_to_producer, &buffer)) {
			mutex_lock(&session->lock);
			if (buffer->file_fence)
				aml_sync_put_fence(buffer->file_fence);

			rb_erase(&buffer->node, &session->buffers);
			kfree(buffer);
			mutex_unlock(&session->lock);
		}
	}

	if (instance->consumer && instance->consumer == session) {
		while (kfifo_get(&instance->fifo_to_consumer, &buffer)) {
			mutex_lock(&session->lock);
			rb_erase(&buffer->node, &session->buffers);
			if (buffer->file_buffer)
				fput(buffer->file_buffer);
			kfree(buffer);
			mutex_unlock(&session->lock);
		}
	}
	mutex_unlock(&instance->lock);
}

/*
 * called when vt session released
 * clean up instance connected session
 */
static int videotunnel_instance_trim(struct videotunnel_session *session)
{
	struct videotunnel_dev *dev = session->dev;
	struct videotunnel_instance *instance = NULL;
	struct videotunnel_buffer *buffer = NULL;
	struct rb_node *n = NULL;

	mutex_lock(&dev->instance_lock);
	for (n = rb_first(&dev->instances); n; n = rb_next(n)) {
		instance = rb_entry(n, struct videotunnel_instance, node);
		if (instance->producer && instance->producer == session) {
			while (kfifo_get(&instance->fifo_to_producer,
					 &buffer)) {
				if (buffer->file_fence)
					aml_sync_put_fence(buffer->file_fence);
				kfree(buffer);
			}
			instance->producer = NULL;
		}
		if (instance->consumer && instance->consumer == session) {
			while (kfifo_get(&instance->fifo_to_consumer,
					 &buffer)) {
				kfree(buffer);
			}
			instance->consumer = NULL;
		}

		if (!instance->consumer && !instance->producer) {
			while (kfifo_get(&instance->fifo_to_producer,
					 &buffer)) {
				if (buffer->file_fence)
					aml_sync_put_fence(buffer->file_fence);
				kfree(buffer);
			}
			while (kfifo_get(&instance->fifo_to_consumer,
					 &buffer))
				kfree(buffer);
		}
	}

	mutex_unlock(&dev->instance_lock);
	return 0;
}

static void videotunnel_session_destroy(struct videotunnel_session *session)
{
	struct videotunnel_dev *dev = session->dev;
	struct videotunnel_instance *instance = NULL, *tmp = NULL;

	/* videotunnel instances cleanup */
	mutex_lock(&session->lock);
	list_for_each_entry_safe(instance, tmp, &session->instances_head, entry)
		videotunnel_instance_put(instance);
	mutex_unlock(&session->lock);

	/* release dev session rb tree node */
	down_write(&dev->session_lock);
	if (session->task)
		put_task_struct(session->task);
	rb_erase(&session->node, &dev->sessions);
	debugfs_remove_recursive(session->debug_root);
	up_write(&dev->session_lock);

	videotunnel_instance_trim(session);

	kfree(session->display_name);
	kfree(session->name);
	kfree(session);
}

static int videotunnel_open(struct inode *inode, struct file *filp)
{
	struct miscdevice *miscdev = filp->private_data;
	struct videotunnel_dev *dev =
		container_of(miscdev, struct videotunnel_dev, mdev);
	struct videotunnel_session *session;
	char debug_name[64];

	snprintf(debug_name, 64, "%u", task_pid_nr(current->group_leader));
	session = videotunnel_session_create(dev, debug_name);
	if (IS_ERR(session))
		return PTR_ERR(session);

	filp->private_data = session;

	return 0;
}

static int videotunnel_release(struct inode *inode, struct file *filp)
{
	struct videotunnel_session *session = filp->private_data;

	videotunnel_session_destroy(session);
	return 0;
}

static long videotunnel_get_connected_id(void)
{
	long cid;

	cid = get_random_long();
	if (cid == -1)
		return 0;

	return cid;
}

static int videotunnel_ctrl_process(struct vt_ctrl_data *data,
				    struct videotunnel_session *session)
{
	struct videotunnel_dev *dev = session->dev;
	struct videotunnel_instance *instance;
	struct videotunnel_instance *replace;
	int id = data->tunnel_id;
	int ret;

	if (id < 0 || id > MAX_VIDEO_INSTANCE_NUM)
		return -EINVAL;
	if (data->role == VT_ROLE_INVALID)
		return -EINVAL;

	/* find the instance with the tunnel_id */
	instance = idr_find(&dev->instance_idr, id);
	switch (data->cmd) {
	case VT_CTRL_CONNECT:
	{
		char name[64];

		if (!instance) {
			while ((ret = idr_alloc(&dev->instance_idr,
						NULL, 0, MAX_VIDEO_TUNNEL, GFP_KERNEL))
					<= id) {
				if (ret == id)
					break;
				else if (ret < 0)
					return ret;
				videotunnel_debug(VT_DEBUG_BUFFERS,
						  "connect alloc id:%d\n", ret);
			}

			instance = videotunnel_instance_create(dev);
			if (IS_ERR(instance))
				return PTR_ERR(instance);

			mutex_lock(&dev->instance_lock);
			replace = idr_replace(&dev->instance_idr, instance, id);
			mutex_unlock(&dev->instance_lock);

			if (IS_ERR(replace)) {
				videotunnel_instance_put(instance);
				return PTR_ERR(replace);
			}
			if (!replace)
				videotunnel_instance_put(replace);

			instance->id = id;
			snprintf(name, 64, "instance-%d", instance->id);
			instance->debug_root =
				debugfs_create_file(name, 0664, dev->debug_root,
						    instance, &debug_instance_fops);

			mutex_lock(&session->lock);
			list_add_tail(&session->instances_head,
				      &instance->entry);
			mutex_unlock(&session->lock);
		} else {
			videotunnel_instance_get(instance);
		}

		/* to do what if producer/consumer alread has value */
		if (data->role == VT_ROLE_PRODUCER) {
			if (instance->producer &&
			    instance->producer != session) {
				videotunnel_instance_put(instance);
				pr_err("Connect to vt %d err, already has producer", id);
				return -EINVAL;
			}
			instance->producer = session;
		} else if (data->role == VT_ROLE_CONSUMER) {
			if (instance->consumer &&
			    instance->consumer != session) {
				videotunnel_instance_put(instance);
				pr_err("Connect to vt %d err, already has consumer", id);
				return -EINVAL;
			}
			instance->consumer = session;
		}
		session->cid = videotunnel_get_connected_id();
		session->role = data->role;
		break;
	}
	case VT_CTRL_DISCONNECT:
	{
		if (!instance || session->role != data->role)
			return -EINVAL;

		if (data->role == VT_ROLE_PRODUCER) {
			if (!instance->producer)
				return -EINVAL;
			if (instance->producer != session)
				return -EINVAL;
			videotunnel_session_trim(session, instance);
			instance->producer = NULL;
		} else if (data->role == VT_ROLE_CONSUMER) {
			if (!instance->consumer)
				return -EINVAL;
			if (instance->consumer != session)
				return -EINVAL;
			videotunnel_session_trim(session, instance);
			instance->consumer = NULL;
		}

		videotunnel_instance_put(instance);
		session->cid = -1;
		break;
	}
	default:
		pr_err("unknown videotunnel cmd:%d\n", data->cmd);
		return -EINVAL;
	}

	return 0;
}

static int videotunnel_buffer_add(struct videotunnel_session *session,
				  struct videotunnel_buffer *buffer)
{
	struct rb_node **p = &session->buffers.rb_node;
	struct rb_node *parent = NULL;
	struct videotunnel_buffer *entry;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct videotunnel_buffer, node);

		if (buffer->item.buffer_fd < entry->item.buffer_fd) {
			p = &(*p)->rb_left;
		} else if (buffer->item.buffer_fd > entry->item.buffer_fd) {
			p = &(*p)->rb_right;
		} else {
			videotunnel_debug(VT_DEBUG_BUFFERS,
					  "%s: buffer already found.", __func__);
			return -EEXIST;
		}
	}

	rb_link_node(&buffer->node, parent, p);
	rb_insert_color(&buffer->node, &session->buffers);

	return 0;
}

static struct videotunnel_buffer *videotunnel_buffer_get(struct rb_root *root,
							 int key)
{
	struct videotunnel_buffer *buffer = NULL;
	struct rb_node *n = NULL;

	for (n = rb_first(root); n; n = rb_next(n)) {
		buffer = rb_entry(n, struct videotunnel_buffer, node);
		if (buffer->item.buffer_fd == key)
			break;
	}

	return buffer;
}

static int videotunnel_has_buffer(struct videotunnel_instance *instance,
				  enum vt_role_e role)
{
	int ret = 0;

	if (role == VT_ROLE_PRODUCER)
		ret = !kfifo_is_empty(&instance->fifo_to_producer);
	else
		ret = !kfifo_is_empty(&instance->fifo_to_consumer);

	return ret;
}

static int videotunnel_queue_buffer(struct vt_buffer_data *data,
				    struct videotunnel_session *session)
{
	struct videotunnel_dev *dev = session->dev;
	struct videotunnel_instance *instance =
		idr_find(&dev->instance_idr, data->tunnel_id);
	struct vt_buffer_item *item;
	struct videotunnel_buffer *buffer = NULL;
	int ret = 0;
	int i;

	if (!instance || !instance->producer)
		return -EINVAL;
	if (instance->producer && instance->producer != session)
		return -EINVAL;

	videotunnel_debug(VT_DEBUG_BUFFERS,
			  "vt queuebuffer size=%d\n", data->buffer_size);
	for (i = 0; i < data->buffer_size; i++) {
		item = &data->buffers[i];
		buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
		if (!buffer)
			return -ENOMEM;
		buffer->file_buffer = fget(item->buffer_fd);
		videotunnel_debug(VT_DEBUG_BUFFERS,
				  "vt queuebuffer fget file=%p, buffer=%p\n",
				  buffer->file_buffer, buffer);
		if (!buffer->file_buffer) {
			ret = -EBADF;
			goto err_fget;
		}
		buffer->buffer_fd_pro = item->buffer_fd;
		buffer->buffer_fd_con = -1;
		buffer->session_pro = session;
		buffer->cid_pro = session->cid;
		buffer->item = *item;
		buffer->item.buffer_status = VT_BUFFER_QUEUE;

		mutex_lock(&session->lock);
		videotunnel_buffer_add(session, buffer);
		mutex_unlock(&session->lock);

		mutex_lock(&instance->lock);
		kfifo_put(&instance->fifo_to_consumer, buffer);
		mutex_unlock(&instance->lock);
	}

	if (instance->consumer && data->buffer_size > 0)
		wake_up_interruptible(&instance->wait_consumer);

	return 0;

err_fget:
	kfree(buffer);
	return ret;
}

static int videotunnel_dequeue_buffer(struct vt_buffer_data *data,
				      struct videotunnel_session *session)
{
	struct videotunnel_dev *dev = session->dev;
	struct videotunnel_instance *instance =
		idr_find(&dev->instance_idr, data->tunnel_id);
	struct videotunnel_buffer *buffer = NULL;
	int ret = -1;

	if (!instance || !instance->producer)
		return -EINVAL;
	if (instance->producer && instance->producer != session)
		return -EINVAL;

	/* empty need wait */
	if (kfifo_is_empty(&instance->fifo_to_producer)) {
		ret = wait_event_interruptible_timeout(instance->wait_producer,
						       videotunnel_has_buffer(instance,
									      VT_ROLE_PRODUCER),
						       msecs_to_jiffies(VIDEO_TUNNEL_MAX_WAIT_MS));
		/* timeout */
		if (ret == 0)
			return -EAGAIN;
	}

	mutex_lock(&instance->lock);
	ret = kfifo_get(&instance->fifo_to_producer, &buffer);
	mutex_unlock(&instance->lock);
	if (!ret || !buffer) {
		pr_err("dequeue buffer got null buffer");
		return -EAGAIN;
	}

	/* it's previous connect buffer */
	if (buffer->cid_pro != session->cid) {
		mutex_lock(&session->lock);
		if (buffer->file_buffer)
			fput(buffer->file_buffer);
		if (buffer->file_fence)
			aml_sync_put_fence(buffer->file_fence);

		rb_erase(&buffer->node, &session->buffers);
		mutex_unlock(&session->lock);

		kfree(buffer);
		return -EAGAIN;
	}

	if (buffer->file_fence) {
		ret = aml_sync_wait_fence(buffer->file_fence, 3000);
		if (ret < 0)
			pr_err("dequeue buffer wait fence timeout");

		aml_sync_put_fence(buffer->file_fence);
	}

	/* remove the buffer from producer session rb tree*/
	mutex_lock(&session->lock);
	rb_erase(&buffer->node, &instance->producer->buffers);
	mutex_unlock(&session->lock);

	buffer->item.buffer_fd = buffer->buffer_fd_pro;
	buffer->item.tunnel_id = instance->id;
	buffer->item.buffer_status = VT_BUFFER_DEQUEUE;

	/* return the buffer */
	data->buffer_size = 1;
	data->buffers[0] = buffer->item;

	/* free the videotunnel buffer*/
	kfree(buffer);

	return 0;
}

static int videotunnel_acquire_buffer(struct vt_buffer_data *data,
				      struct videotunnel_session *session)
{
	struct videotunnel_dev *dev = session->dev;
	struct videotunnel_instance *instance =
		idr_find(&dev->instance_idr, data->tunnel_id);
	struct videotunnel_buffer *buffer = NULL;
	int fd, ret = -1;

	if (!instance || !instance->consumer)
		return -EINVAL;
	if (instance->consumer && instance->consumer != session)
		return -EINVAL;

	videotunnel_debug(VT_DEBUG_BUFFERS,
			  "vt acquirebuffer is empty=%d\n",
			  kfifo_is_empty(&instance->fifo_to_consumer));
	/* empty need wait */
	if (kfifo_is_empty(&instance->fifo_to_consumer)) {
		ret = wait_event_interruptible_timeout(instance->wait_consumer,
						       videotunnel_has_buffer(instance,
									      VT_ROLE_CONSUMER),
						       msecs_to_jiffies(VIDEO_TUNNEL_MAX_WAIT_MS));

		/* timeout */
		if (ret == 0)
			return -EAGAIN;
	}

	mutex_lock(&instance->lock);
	ret = kfifo_get(&instance->fifo_to_consumer, &buffer);
	mutex_unlock(&instance->lock);
	if (!ret || !buffer) {
		pr_err("dequeue buffer got null buffer");
		return -EAGAIN;
	}

	/* get the fd in consumer */
	if (buffer->buffer_fd_con <= 0) {
		fd = get_unused_fd_flags(O_CLOEXEC);
		if (fd < 0) {
			/* back to producer */
			pr_err("videotunnel install fd error\n");
			buffer->item.buffer_status = VT_BUFFER_RELEASE;
			mutex_lock(&instance->lock);
			kfifo_put(&instance->fifo_to_producer, buffer);
			mutex_unlock(&instance->lock);
			ret = -ENOMEM;
		}

		fd_install(fd, buffer->file_buffer);
		buffer->buffer_fd_con = fd;
		videotunnel_debug(VT_DEBUG_BUFFERS,
				  "vt acquirebuffer install buffer fd=%d\n", fd);
	}

	/* remove the buffer from producer session rb tree*/
	if (instance->producer) {
		mutex_lock(&instance->producer->lock);
		rb_erase(&buffer->node, &instance->producer->buffers);
		mutex_unlock(&instance->producer->lock);
	}
	/* insert it the consumer session's rb tree */
	mutex_lock(&session->lock);
	videotunnel_buffer_add(session, buffer);
	mutex_unlock(&session->lock);

	buffer->item.buffer_fd = buffer->buffer_fd_con;
	buffer->item.tunnel_id = instance->id;
	buffer->item.buffer_status = VT_BUFFER_ACQUIRE;

	/* return the buffer */
	data->buffer_size = 1;
	data->buffers[0] = buffer->item;
	return 0;
}

static int videotunnel_release_buffer(struct vt_buffer_data *data,
				      struct videotunnel_session *session)
{
	struct videotunnel_dev *dev = session->dev;
	struct videotunnel_instance *instance =
		idr_find(&dev->instance_idr, data->tunnel_id);
	struct vt_buffer_item *item;
	struct videotunnel_buffer *buffer;
	int i;

	if (!instance || !instance->consumer)
		return -EINVAL;
	if (instance->consumer && instance->consumer != session)
		return -EINVAL;

	videotunnel_debug(VT_DEBUG_BUFFERS,
			  "vt releasebuffer buffer_size:%d\n", data->buffer_size);
	for (i = 0; i < data->buffer_size; i++) {
		item = &data->buffers[i];
		/* find the buffer in consumer rb tree */
		buffer = videotunnel_buffer_get(&session->buffers,
						item->buffer_fd);

		if (!buffer)
			return -EINVAL;

		if (item->fence_fd > 0)
			buffer->file_fence = aml_sync_get_fence(item->fence_fd);

		if (!buffer->file_fence)
			videotunnel_debug(VT_DEBUG_BUFFERS,
					  "vt releasebuffer fence file is null");

		/* close the fd in consumer side */
		videotunnel_close_fd(session, buffer->buffer_fd_con);
		buffer->item.buffer_fd = buffer->buffer_fd_pro;
		buffer->item.buffer_status = VT_BUFFER_RELEASE;

		/* erase node from consumer rb tree */
		mutex_lock(&session->lock);
		rb_erase(&buffer->node, &session->buffers);
		mutex_unlock(&session->lock);

		/* todo if producer has disconnect */
		if (!instance->producer) {
			videotunnel_debug(VT_DEBUG_BUFFERS,
					  "vt releasebuffer buffer, no producer\n");
			kfree(buffer);
		} else {
			/* insert it the producer session's rb tree */
			videotunnel_debug(VT_DEBUG_BUFFERS,
					  "vt releasebuffer producer:%p\n",
				instance->producer);

			if (buffer->session_pro &&
			    buffer->session_pro != instance->producer) {
				videotunnel_debug(VT_DEBUG_BUFFERS,
						  "vt releasebuffer buffer, producer valid\n");
				kfree(buffer);
				continue;
			}

			mutex_lock(&instance->producer->lock);
			videotunnel_buffer_add(instance->producer, buffer);
			mutex_unlock(&instance->producer->lock);

			mutex_lock(&instance->lock);
			kfifo_put(&instance->fifo_to_producer, buffer);
			mutex_unlock(&instance->lock);
		}
	}
	if (instance->producer && data->buffer_size > 0)
		wake_up_interruptible(&instance->wait_producer);

	return 0;
}

static unsigned int videotunnel_ioctl_dir(unsigned int cmd)
{
	switch (cmd) {
	case VT_IOC_ALLOC_ID:
	case VT_IOC_DEQUEUE_BUFFER:
	case VT_IOC_ACQUIRE_BUFFER:
		return _IOC_READ;
	case VT_IOC_FREE_ID:
	case VT_IOC_CTRL:
		return _IOC_WRITE;
	default:
		return _IOC_DIR(cmd);
	}
}

static long videotunnel_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long arg)
{
	int ret = 0;
	union videotunnel_ioctl_arg data;
	struct videotunnel_session *session = filp->private_data;
	unsigned int dir = videotunnel_ioctl_dir(cmd);
	struct videotunnel_dev *dev = session->dev;
	struct videotunnel_instance *instance = NULL;

	if (_IOC_SIZE(cmd) > sizeof(data))
		return -EINVAL;

	if (copy_from_user(&data, (void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	switch (cmd) {
	case VT_IOC_ALLOC_ID: {
		char name[64];

		instance = videotunnel_instance_create(session->dev);
		if (IS_ERR(instance))
			return PTR_ERR(instance);

		mutex_lock(&dev->instance_lock);
		ret = idr_alloc(&dev->instance_idr, instance, 0, 0, GFP_KERNEL);
		instance->id = ret;
		mutex_unlock(&dev->instance_lock);
		if (ret < 0) {
			videotunnel_instance_put(instance);
			return ret;
		}

		snprintf(name, 64, "instance-%d", instance->id);
		instance->debug_root =
			debugfs_create_file(name, 0664, dev->debug_root,
					    instance, &debug_instance_fops);

		mutex_lock(&session->lock);
		list_add_tail(&session->instances_head, &instance->entry);
		mutex_unlock(&session->lock);

		data.alloc_data.tunnel_id = instance->id;
		break;
	}
	case VT_IOC_FREE_ID: {
		instance = idr_find(&dev->instance_idr,
				    data.alloc_data.tunnel_id);
		/* to do free id operation check */
		if (!instance) {
			pr_err("destroy unknown videotunnel instance:%d\n",
			       data.alloc_data.tunnel_id);
			ret = -EINVAL;
		} else {
			ret = videotunnel_instance_put(instance);
		}
		break;
	}
	case VT_IOC_CTRL:
		ret = videotunnel_ctrl_process(&data.ctrl_data, session);
		break;
	case VT_IOC_QUEUE_BUFFER:
		ret = videotunnel_queue_buffer(&data.buffer_data, session);
		break;
	case VT_IOC_DEQUEUE_BUFFER:
		ret = videotunnel_dequeue_buffer(&data.buffer_data, session);
		break;
	case VT_IOC_RELEASE_BUFFER:
		ret = videotunnel_release_buffer(&data.buffer_data, session);
		break;
	case VT_IOC_ACQUIRE_BUFFER:
		ret = videotunnel_acquire_buffer(&data.buffer_data, session);
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

static const struct file_operations videotunnel_fops = {
	.owner = THIS_MODULE,
	.open = videotunnel_open,
	.release = videotunnel_release,
	.unlocked_ioctl = videotunnel_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = videotunnel_ioctl,
#endif
};

static int videotunnel_probe(struct platform_device *pdev)
{
	int ret;

	vdev = kzalloc(sizeof(*vdev), GFP_KERNEL);
	if (!vdev)
		return -ENOMEM;

	vdev->dev_name = DEVICE_NAME;
	vdev->mdev.minor = MISC_DYNAMIC_MINOR;
	vdev->mdev.name = DEVICE_NAME;
	vdev->mdev.fops = &videotunnel_fops;

	ret = misc_register(&vdev->mdev);
	if (ret) {
		pr_err("videotunnel: misc_register fail.\n");
		goto failed_alloc_dev;
	}

	mutex_init(&vdev->instance_lock);
	idr_init(&vdev->instance_idr);
	vdev->instances = RB_ROOT;
	init_rwsem(&vdev->session_lock);
	vdev->sessions = RB_ROOT;

	ret = kfifo_alloc(&vdev->cmd_queue, VIDEOTUNNEL_CMD_FIFO_SIZE,
			  GFP_KERNEL);
	if (ret) {
		pr_err("videotunnel: kfifo_alloc failed.\n");
		goto failed_misc_register;
	}

	init_waitqueue_head(&vdev->cmd_wait);

	mutex_init(&debugfs_mutex);
	vdev->debug_root = debugfs_create_dir("videotunnel", NULL);
	if (!vdev->debug_root)
		pr_err("videotunnel: failed to create debugfs root directory.\n");

	return 0;

failed_misc_register:
	misc_deregister(&vdev->mdev);
failed_alloc_dev:
	kfree(vdev);

	return ret;
}

static int videotunnel_remove(struct platform_device *pdev)
{
	kfifo_free(&vdev->cmd_queue);
	idr_destroy(&vdev->instance_idr);
	debugfs_remove_recursive(vdev->debug_root);
	misc_deregister(&vdev->mdev);
	kfree(vdev);

	return 0;
}

static const struct of_device_id meson_videotunnel_match[] = {
	{.compatible = "amlogic, meson_videotunnel"},
	{},
};

static struct platform_driver meson_videotunnel_driver = {
	.driver = {
		.name = "meson_videotunnel_driver",
		.owner = THIS_MODULE,
		.of_match_table = meson_videotunnel_match,
	},
	.probe = videotunnel_probe,
	.remove = videotunnel_remove,
};

int __init meson_videotunnel_init(void)
{
	pr_info("videotunnel init\n");

	if (platform_driver_register(&meson_videotunnel_driver)) {
		pr_err("failed to register videotunnel\n");
		return -ENODEV;
	}

	return 0;
}

void __exit meson_videotunnel_exit(void)
{
	platform_driver_unregister(&meson_videotunnel_driver);
}

#ifndef MODULE
module_init(meson_videotunnel_init);
module_exit(meson_videotunnel_exit);
#endif
