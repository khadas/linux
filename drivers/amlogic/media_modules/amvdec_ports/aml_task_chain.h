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
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#ifndef AML_TASK_CHAIN_H
#define AML_TASK_CHAIN_H

#include <linux/amlogic/media/vfm/vframe.h>

enum task_type_e {
	TASK_TYPE_DEC,
	TASK_TYPE_VPP,
	TASK_TYPE_V4L_SINK,
	TASK_TYPE_GE2D,
	TASK_TYPE_MAX
};

enum task_dir_e {
	TASK_DIR_SUBMIT,
	TASK_DIR_RECYCLE,
	TASK_DIR_MAX
};

struct task_chain_s;

/*
 * struct task_ops_s - interface of the task item.
 * @type	: type of task ops involves dec, vpp, v4l sink etc.
 * @get_vframe	: get the video frame from caller's fifo.
 * @put_vframe	: put the video frame to caller's fifo.
 * @fill_buffer	: submit the buffer into next process module.
 */
struct task_ops_s {
	enum task_type_e type;
	void	(*get_vframe) (void *caller, struct vframe_s **vf);
	void	(*put_vframe) (void *caller, struct vframe_s *vf);
	void	(*fill_buffer) (void *v4l_ctx, void *fb_ctx);
};

/*
 * struct task_item_s - items of the task chain.
 * @node	: list node of the specific task item.
 * @ref		: reference count of item be used by others.
 * @name	: name of task item, map with task type.
 * @is_active	: indicate this item whether is active.
 * @vframe[3]	: store the vframes that get from caller.
 * @task	: the context of the task chain.
 * @caller	: it's the handle, meght it's dec, vpp or v4l-sink etc.
 * @ops		: sets of interface which attach from task item.
 */
struct task_item_s {
	struct list_head	node;
	struct kref		ref;
	const u8		*name;
	bool			is_active;
	void			*vframe[3];
	struct task_chain_s	*task;
	void			*caller;
	struct task_ops_s	*ops;
};

/*
 * struct task_chain_s - the manager struct of the task chain.
 * @list_item	: all task items be attached are store in the list.
 * @node	: will register to the task chain pool.
 * @ref		: reference count of task chain be used by others.
 * @slock	: used for list item write and read safely.
 * @id		: it's vb index to be a mark used for task chain.
 * @ctx		: the context of the v4l driver.
 * @obj		: the object managed by task chain.
 * @direction	: direction incluse 2 flows submit & recycle.
 * @cur_type	: the latest item type before a new item be attached.
 * @map		: the map store the pipeline information.
 * @attach	: attach a new item to task chain.
 * @submit	: submit the finish item to next item module.
 * @recycle	: if item's date was consumed will be recycled to item.
 */
struct task_chain_s {
	struct list_head	list_item;
	struct list_head	node;
	struct kref		ref;
	spinlock_t		slock;
	int			id;
	void			*ctx;
	void			*obj;
	enum task_dir_e		direction;
	enum task_type_e	cur_type;
	u8			map[2][8];

	void	(*attach) (struct task_chain_s *, struct task_ops_s *, void *);
	void	(*submit) (struct task_chain_s *, enum task_type_e);
	void	(*recycle) (struct task_chain_s *, enum task_type_e);
};


int task_chain_init(struct task_chain_s **task_out,
		     void *v4l_ctx,
		     void *obj,
		     int vb_idx);
void task_order_attach(struct task_chain_s *task,
		       struct task_ops_s *ops,
		       void *caller);
void task_chain_clean(struct task_chain_s *task);
void task_chain_release(struct task_chain_s *task);
void task_chain_show(struct task_chain_s *task);
void task_chain_update_object(struct task_chain_s *task, void *obj);

#endif //AML_TASK_CHAIN_H

