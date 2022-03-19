// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>

#include "resourcemanage.h"
#define RESMAM_ENABLE_JSON  (1)

#ifdef RESMAM_ENABLE_JSON
#include "cjson.h"
#endif

#define DEVICE_NAME "amresource_mgr"
#define DEVICE_CLASS_NAME "resource_mgr"

#define QUERY_SECURE_BUFFER (1)
#define QUERY_NO_SECURE_BUFFER (0)
#define RESMAN_VERSION         (3)
#define SINGLE_SIZE           (64)
#define EXT_MAX_SIZE     (64 * 1024)
#define STR_MAX_SIZE     (32)


/*resman.json is the common SDK resource json file*/
/*resmanext.json is project customized json file*/
#define EXTCONFIGNAME "/vendor/etc/resman.json"
#define EXTCONFIGNAMELIX "/etc/resman.json"
#define EXTCONFIGCUSTOMNAME "/vendor/etc/resmanext.json"
#define EXTCONFIGCUSTOMNAMELIX "/etc/resmanext.json"
#define COMMOMCONFIG  (1)
#define CUSTOMCONFIG  (2)

struct {
	int id;
	char *name;
} resources_map[] = {
	{RESMAN_ID_VFM_DEFAULT, "vfm_default"},
	{RESMAN_ID_AMVIDEO, "amvideo"},
	{RESMAN_ID_PIPVIDEO, "videopip"},
	{RESMAN_ID_SEC_TVP, "tvp"},
	{RESMAN_ID_TSPARSER, "tsparser"},
	{RESMAN_ID_CODEC_MM, "codec_mm"},
	{RESMAN_ID_ADC_PLL, "adc_pll"},
	{RESMAN_ID_DECODER, "decoder"},
	{RESMAN_ID_HWC, "hwc"},
	{RESMAN_ID_DMX, "dmx"},
	{RESMAN_ID_DI, "di"},
};

enum RESMAN_STATUS {
	RESMAN_STATUS_INITED,
	RESMAN_STATUS_ACQUIRED,
	RESMAN_STATUS_PREEMPTED
};

/**
 * struct resman_event - event list node
 * @list: list entry
 * @type: event type, see RESMAN_EVENT
 */
struct resman_event {
	struct list_head list;
	__u32 type;
};

/**
 * struct resman_node - allocated resource node
 * @slist: session list that own same resource
 * @rlist: resource list that own by same session
 * @pending_release: pending session for release
 * @session_ptr: session owner
 * @resource_ptr: resource owner
 * @s: node data
 */
struct resman_node {
	struct list_head slist;
	struct list_head rlist;
	struct completion pending_release;
	void *session_ptr;
	void *resource_ptr;
	union {
		struct {
			char type;
		} toggle;
		struct {
			__u32 score;
			bool secure;
		} codec_mm;
		struct {
			__u32 use;
		} capacity;
	} s;
};

/**
 * struct resman_session - session data
 * @id: session id
 * @list: sessions list
 * @resources: allocated resource list
 * @events: events list
 * @app_name: name of app
 * @status: session status, see RESMAN_STATUS
 * @comm: proc comm name
 * @pid: thread id
 * @tgid: pid
 * @lock: lock of session
 * @wq_event: wait queue for poll
 * @app_type: type of session, see RESMAN_APP
 */
struct resman_session {
	int id;
	struct list_head list;
	struct list_head resources;
	struct list_head events;
	char app_name[32];
	int status;
	char comm[TASK_COMM_LEN];
	pid_t pid;
	pid_t tgid;
	struct mutex lock; /*session lock*/
	wait_queue_head_t wq_event;
	int app_type;
	int prio;
};

/**
 * struct resman_resource - resource data
 * @list: resource list
 * @sessions: list of session that acquired this resource
 *            the first one always the oldest one
 * @lock: resource lock
 * @wq_release: wait queue for release
 * @wq_acquire: wait queue for acquire
 * @pending_acquire: pending session for acquire
 * @id: resource id
 * @name: name of resource
 * @type: resource tye, see RESMAN_TYPE
 * @acquire: hook of acquire
 * @release: hook of release
 * @value: value of resource
 * @d: resource data
 *
 */
struct resman_resource {
	struct list_head list;
	struct list_head sessions;
	struct mutex lock; /*resource lock*/
	wait_queue_head_t wq_release;
	wait_queue_head_t wq_acquire;
	atomic_t pending_acquire;
	int id;
	char name[32];
	__u32 type;
	bool (*acquire)(struct resman_session *sess,
			struct resman_resource *resource,
			struct resman_node *node,
			bool preempt,
			__u32 timeout,
			char *arg);
	void (*release)(struct resman_session *sess,
			struct resman_resource *resource,
			struct resman_node *node);
	int value;
	union {
		/**
		 * @avail: max available value for counter
		 */
		struct {
			__u32 avail;
		} counter;
		/**
		 * @total: total score
		 * @uhd: how much score per uhd instance
		 * @fhd: how much score per fhd instance
		 * @secure: max secure decoder count
		 */
		struct {
			__u32 total;
			__u32 uhd;
			__u32 fhd;
			__u32 secure;
			atomic_t counter_secure;
			atomic_t counter_nonsecure;
		} codec_mm;
		/**
		 * @total: max available capacity size
		 */
		struct {
			__u32 total;
		} capacity;
	} d;
};

static int resman_debug = 1;
static int preempt_timeout_ms = 2500;
static struct list_head sessions_head;
static struct list_head resources_head;
static DEFINE_MUTEX(sessions_lock);
static DEFINE_MUTEX(resource_lock);
static dev_t resman_devno;
static int sess_id = 1;
static struct cdev *resman_cdev;
static struct class *resman_class;
static char *resman_configs;
static bool extloaded;

module_param(resman_debug, int, 0644);
module_param(preempt_timeout_ms, int, 0644);

#define dprintk(level, fmt, arg...)					\
	do {								\
		if (resman_debug >= (level))					\
			pr_info("resman: " fmt, ## arg);	\
	} while (0)

/**
 * resman_parser_kv() - parse key value pair
 *
 * @str: input string, must rw
 * @k: key name
 * @v: value to store
 *
 * parse key value pair like "foo:1"
 *
 * Return: true for success of false
 */
static inline bool resman_parser_kv(char *str, const char *k, __u32 *v)
{
	bool ret = false;
	char *sk, *sv;

	sk = strsep(&str, ":");
	sv = str;
	if (k && v && sk && sv && !strcmp(sk, k) && !kstrtou32(sv, 0, v))
		ret = true;
	if (!ret)
		dprintk(2, "parse %s failed %s\n", k, str);
	return ret;
}

static struct resman_resource *resman_find_resource_by_id(int id)
{
	struct resman_resource *ret = NULL;
	struct list_head *pos, *tmp;

	list_for_each_safe(pos, tmp, &resources_head) {
		ret = list_entry(pos, struct resman_resource, list);
		if (ret->id == id)
			return ret;
	}
	return NULL;
}

static struct resman_resource *resman_find_resource_by_name(const char *name)
{
	struct resman_resource *ret = NULL;
	struct list_head *pos, *tmp;

	list_for_each_safe(pos, tmp, &resources_head) {
		ret = list_entry(pos, struct resman_resource, list);
		if (!strcmp(name, ret->name))
			return ret;
	}
	return NULL;
}

static struct resman_node *resman_find_node_by_resource_id
		(struct resman_session *sess, int id)
{
	struct resman_node *ret = NULL;
	struct resman_resource *resource = NULL;
	struct list_head *pos, *tmp;

	list_for_each_safe(pos, tmp, &sess->resources) {
		ret = list_entry(pos, struct resman_node, rlist);
		resource = (struct resman_resource *)ret->resource_ptr;
		if (resource->id == id)
			return ret;
	}
	return NULL;
}

static int resman_count_codec_mm_session(bool secure)
{
	struct resman_resource *resource;
	int count = 0;

	resource = resman_find_resource_by_id(RESMAN_ID_CODEC_MM);
	if (!resource)
		return 0;

	if (secure)
		count = atomic_read(&resource->d.codec_mm.counter_secure);
	else
		count = atomic_read(&resource->d.codec_mm.counter_nonsecure);

	return count;
}

static bool resman_acquire_resource(struct resman_session *sess,
				    struct resman_resource *resource,
				    bool preempt,
				    __u32 timeout,
				    const char *arg_ro)
{
	bool ret = false;
	struct resman_node *node;
	char *arg_rw = NULL;

	if (arg_ro)
		arg_rw = kstrdup(arg_ro, GFP_KERNEL);

	node = kzalloc(sizeof(*node), GFP_KERNEL);

	if (atomic_read(&resource->pending_acquire) > 0)
		wait_event_interruptible(resource->wq_acquire,
				atomic_read(&resource->pending_acquire) == 0);

	mutex_lock(&resource->lock);
	dprintk(1, "%d acquire [%s], preempt %s\n",
		sess->id,
		resource->name,
		preempt ? "yes" : "no");
	if (resource->acquire(sess, resource, node, preempt, timeout, arg_rw)) {
		list_add_tail(&node->rlist, &sess->resources);
		list_add_tail(&node->slist, &resource->sessions);
		init_completion(&node->pending_release);
		node->session_ptr = (void *)sess;
		node->resource_ptr = (void *)resource;
		ret = true;
		sess->status = RESMAN_STATUS_ACQUIRED;
		dprintk(1, "%d acquire [%s] success\n",
			sess->id,
			resource->name);
	} else {
		dprintk(1, "%d acquire [%s] failed\n",
			sess->id, resource->name);
	}
	mutex_unlock(&resource->lock);
	kfree(arg_rw);
	return ret;
}

static void resman_release_resource(struct resman_node *node)
{
	struct resman_session *sess = node->session_ptr;
	struct resman_resource *resource = node->resource_ptr;

	if (!mutex_trylock(&resource->lock)) {
		while (!completion_done(&node->pending_release))
			complete(&node->pending_release);
		mutex_lock(&resource->lock);
	}

	resource->release(sess, resource, node);
	list_del(&node->rlist);
	list_del(&node->slist);
	dprintk(1, "%d release [%s]\n",
		sess->id,
		resource->name);
	wake_up_interruptible_all(&resource->wq_release);
	mutex_unlock(&resource->lock);
	kfree(node);
}

static struct resman_session *resman_open_session(void)
{
	struct resman_session *sess = NULL;

	sess = kzalloc(sizeof(*sess), GFP_KERNEL);
	if (!sess)
		return NULL;

	INIT_LIST_HEAD(&sess->resources);
	INIT_LIST_HEAD(&sess->events);
	mutex_init(&sess->lock);
	init_waitqueue_head(&sess->wq_event);
	strcpy(sess->comm, current->comm);
	sess->tgid = current->tgid;
	sess->pid = current->pid;
	sess->status = RESMAN_STATUS_INITED;
	sess->id = sess_id++;
	mutex_lock(&sessions_lock);
	list_add_tail(&sess->list, &sessions_head);
	mutex_unlock(&sessions_lock);
	return sess;
}

static void resman_close_session(struct resman_session *sess)
{
	struct list_head *pos, *tmp;
	struct resman_node *node;
	struct resman_event *event;

	mutex_lock(&sess->lock);
	wake_up_interruptible_all(&sess->wq_event);
	list_for_each_safe(pos, tmp, &sess->resources) {
		node = list_entry(pos, struct resman_node, rlist);
		resman_release_resource(node);
	}
	list_for_each_safe(pos, tmp, &sess->events) {
		event = list_entry(pos, struct resman_event, list);
		list_del(&event->list);
		kfree(event);
	}
	mutex_unlock(&sess->lock);
	mutex_lock(&sessions_lock);
	list_del(&sess->list);
	mutex_unlock(&sessions_lock);
	kfree(sess);
}

static void resman_send_event(struct resman_session *sess, __u32 type)
{
	struct resman_event *event;

	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (!event)
		return;

	event->type = type;
	list_add_tail(&event->list, &sess->events);
}

static bool resman_preempt_session(struct resman_session *curr,
				   struct resman_node *node)
{
	bool ret = false;
	bool need_unlock = true;
	struct resman_session *sess;

	if (!node)
		goto beach;

	sess = node->session_ptr;
	if (sess->status != RESMAN_STATUS_ACQUIRED)
		goto beach;

	if (!mutex_trylock(&sess->lock)) {
		/*other session may waiting to release this resource*/
		if (wait_for_completion_interruptible_timeout(&node->pending_release,
				usecs_to_jiffies(10000)) <= 0)
			goto beach;
		need_unlock = false;
	}
	dprintk(1, "%d preempting %d %s\n",
		curr->id,
		sess->id,
		sess->app_name);
	sess->status = RESMAN_STATUS_PREEMPTED;
	resman_send_event(sess, RESMAN_EVENT_PREEMPT);
	wake_up_interruptible(&sess->wq_event);
	if (need_unlock)
		mutex_unlock(&sess->lock);
	ret = true;
beach:
	return ret;
}

/**
 * resman_counter_preempt() - preempt for counter type
 *
 * @curr: current session data
 * @resource: resource data
 * Return: true for success or false
 */
static bool resman_counter_preempt(struct resman_session *curr,
				   struct resman_resource *resource)
{
	bool ret = false;
	struct resman_node *node, *selected = NULL;
	struct resman_session *sess;
	struct list_head *pos, *tmp;
	int lowestprio = curr->prio;

	list_for_each_safe(pos, tmp, &resource->sessions) {
		node = list_entry(pos, struct resman_node, slist);
		sess = node->session_ptr;
		if (sess->status == RESMAN_STATUS_PREEMPTED && sess->prio >= curr->prio) {
			selected = node;
			dprintk(2, "preempt prio%d\n",  sess->prio);
			break;
		}
	}
	/*find the lowest prio*/
	if (!selected) {
		list_for_each_safe(pos, tmp, &resource->sessions) {
			node = list_entry(pos, struct resman_node, slist);
			sess = node->session_ptr;
			if (sess->prio > lowestprio) {
				selected = node;
				lowestprio = sess->prio;
			}
		}
	}
	/*find the oldest same prio*/
	if (!selected) {
		list_for_each_safe(pos, tmp, &resource->sessions) {
			node = list_entry(pos, struct resman_node, slist);
			sess = node->session_ptr;
			if (sess->prio == lowestprio) {
				selected = node;
				break;
			}
		}
	}
	if (!selected)
		dprintk(2, "fail to find node to preempt\n");
	else
		dprintk(2, "preempt node prio:%d\n", lowestprio);

	if (resman_preempt_session(curr, selected))
		ret = true;

	return ret;
}

/**
 * resman_counter_acquire() - acquire counter resource
 *
 * @sess: session data
 * @resource: resource data
 * @node: node data
 * @preempt: preempt or not
 * @timeout: time out
 * @arg: arg rw
 *
 * acquire a counter resource,
 * Return: true for success or false
 */
static bool resman_counter_acquire(struct resman_session *sess,
				   struct resman_resource *resource,
				   struct resman_node *node,
				   bool preempt,
				   __u32 timeout,
				   char *arg)
{
	bool ret = false;
	long remain;

	if (resource->value >= resource->d.counter.avail) {
		if (preempt) {
			if (!timeout)
				timeout = preempt_timeout_ms;
			if (!resman_counter_preempt(sess, resource))
				timeout = 0;
		}
		if (timeout) {
			atomic_inc(&resource->pending_acquire);
			mutex_unlock(&resource->lock);
			dprintk(1, "%d acquire wait\n", sess->id);
			remain = wait_event_interruptible_timeout(resource->wq_release,
					(resource->value <
						resource->d.counter.avail),
					msecs_to_jiffies(timeout));
			dprintk(1, "%d acquire wait return %ld\n",
				sess->id,
				remain);
			mutex_lock(&resource->lock);
			atomic_dec(&resource->pending_acquire);
			wake_up_interruptible_all(&resource->wq_acquire);
		}
	}
	if (resource->value < resource->d.counter.avail) {
		resource->value++;
		ret = true;
	}
	return ret;
}

/**
 * resman_counter_release() - release counter resource
 *
 * @sess: session data
 * @resource: resource data
 * @node: node data
 */
static void resman_counter_release(struct resman_session *sess,
				   struct resman_resource *resource,
				   struct resman_node *node)
{
	if (resource->value > 0)
		resource->value--;
}

/**
 * resman_toggle_acquire() - acquire toggle resource
 *
 * @sess: session data
 * @resource: resource data
 * @node: node data
 * @preempt: preempt or not
 * @timeout: time out
 * @arg: arg rw
 * Return: true for success or false
 */
static bool resman_toggle_acquire(struct resman_session *sess,
				  struct resman_resource *resource,
				  struct resman_node *node,
				  bool preempt,
				  __u32 timeout,
				  char *arg)
{
	bool ret = false;

	if (arg) {
		switch (*arg) {
		case '+':
			if (resource->value >= 0) {
				resource->value++;
				node->s.toggle.type = '+';
				ret = true;
			}
			break;
		case '-':
			if (resource->value <= 0) {
				resource->value--;
				node->s.toggle.type = '-';
				ret = true;
			}
			break;
		default:
			break;
		}
	}
	return ret;
}

/**
 * resman_toggle_release() - resman_toggle_release
 *
 * @sess: session data
 * @resource: resource data
 * @node: node data
 */
static void resman_toggle_release(struct resman_session *sess,
				  struct resman_resource *resource,
				  struct resman_node *node)
{
	switch (node->s.toggle.type) {
	case '+':
		if (resource->value > 0)
			resource->value--;
		break;
	case '-':
		if (resource->value < 0)
			resource->value++;
		break;
	default:
		break;
	}
	node->s.toggle.type = 0;
}

/**
 * resman_tvp_preempt() - resman_tvp_preempt
 *
 * @curr: current session data
 * Return: true for success or false
 */
static bool resman_tvp_preempt(struct resman_session *curr)
{
	bool ret = false;
	struct resman_resource *resource;
	struct resman_node *node, *selected = NULL;
	struct resman_session *sess;
	struct list_head *pos, *tmp;
	int lowestprio = curr->prio;

	resource = resman_find_resource_by_id(RESMAN_ID_CODEC_MM);
	if (!resource)
		return false;

	/*find the lowest non-secure prio*/
	list_for_each_safe(pos, tmp, &resource->sessions) {
		node = list_entry(pos, struct resman_node, slist);
		sess = node->session_ptr;
		if (!node->s.codec_mm.secure && sess->prio > lowestprio)
			lowestprio = sess->prio;
	}

	/*find the oldest non-secure decoder instance*/
	list_for_each_safe(pos, tmp, &resource->sessions) {
		node = list_entry(pos, struct resman_node, slist);
		sess = node->session_ptr;
		if (!node->s.codec_mm.secure && sess->prio >= lowestprio) {
			selected = node;
			break;
		}
	}

	if (resman_preempt_session(curr, selected))
		ret = true;

	return ret;
}

/**
 * resman_tvp_acquire() - resman_tvp_acquire
 *
 * @sess: session data
 * @resource: resource data
 * @node: node data
 * @preempt: preempt or not
 * @timeout: time out
 * @arg: arg rw
 * Return: true for success or false
 */
static bool resman_tvp_acquire(struct resman_session *sess,
			       struct resman_resource *resource,
			       struct resman_node *node,
			       bool preempt,
			       __u32 timeout,
			       char *arg)
{
	bool ret = false;
	bool again = false;
	long remain;
	int size = 0;
	int flags = 1;
	int count;

	if (!resman_parser_kv(arg, "size", (__u32 *)&size)) {
		if (!strcmp(arg, "uhd"))
			flags = 2;
	}

	do {
		count = resman_count_codec_mm_session(false);
		if (!codec_mm_enable_tvp(size, flags)) {
			resource->value++;
			ret = true;
			break;
		}
		/*If TVP alloc failed, reclaim non-secure decoder instance*/
		if (!preempt || !count)
			break;

		if (!timeout)
			timeout = preempt_timeout_ms;

		if (!resman_tvp_preempt(sess)) {
			again = false;
			timeout = 0;
		} else {
			again = true;
		}
		if (timeout) {
			atomic_inc(&resource->pending_acquire);
			mutex_unlock(&resource->lock);
			dprintk(1, "%d acquire wait\n", sess->id);
			remain = wait_event_interruptible_timeout(resource->wq_release,
					(resman_count_codec_mm_session(false)
							< count),
					msecs_to_jiffies(timeout));
			dprintk(1, "%d acquire wait return %ld\n",
				sess->id,
				remain);
			mutex_lock(&resource->lock);
			atomic_dec(&resource->pending_acquire);
			wake_up_interruptible_all(&resource->wq_acquire);
		}
	} while (again);

	return ret;
}

/**
 * resman_tvp_release() - resman_tvp_release
 *
 * @sess: session data
 * @resource: resource data
 * @node: node data
 */
static void resman_tvp_release(struct resman_session *sess,
			       struct resman_resource *resource,
			       struct resman_node *node)
{
	if (resource->value > 0) {
		codec_mm_disable_tvp();
		resource->value--;
	}
}

/**
 * resman_codec_mm_preempt() - resman_codec_mm_preempt
 *
 * @curr: current session data
 * @resource: resource data
 * @score: acquired score
 * @secure: secure or not
 * Return: true for success or false
 */
static bool resman_codec_mm_preempt(struct resman_session *curr,
				    struct resman_resource *resource,
				    __u32 score,
				    bool secure)
{
	bool ret = false;
	int secure_count;
	struct resman_node *node, *selected;
	struct resman_session *sess;
	struct list_head *pos, *tmp;
	__u32 new_score;
	int lowestprio = curr->prio;


	secure_count = atomic_read(&resource->d.codec_mm.counter_secure);

	/*Reclaim secure instance if exceed limitation*/
	list_for_each_safe(pos, tmp, &resource->sessions) {
		if (!secure || secure_count < resource->d.codec_mm.secure)
			break;
		node = list_entry(pos, struct resman_node, slist);
		sess = node->session_ptr;
		if (node->s.codec_mm.secure) {
			if (resman_preempt_session(curr, node))
				ret = true;
			secure_count--;
		}
	}

	do {
		selected = NULL;
		/*Calculate the available score*/
		new_score = resource->value;
		list_for_each_safe(pos, tmp, &resource->sessions) {
			node = list_entry(pos, struct resman_node, slist);
			sess = node->session_ptr;
			if (sess->status == RESMAN_STATUS_PREEMPTED)
				new_score -= node->s.codec_mm.score;
		}
		if (new_score + score <= resource->d.codec_mm.total)
			break;
		/*find the lowest prio*/
		list_for_each_safe(pos, tmp, &resource->sessions) {
			node = list_entry(pos, struct resman_node, slist);
			sess = node->session_ptr;
			if (sess->status == RESMAN_STATUS_ACQUIRED && sess->prio > lowestprio) {
				selected = node;
				lowestprio = sess->prio;
			}
		}
		/*find the oldest same prio*/
		if (!selected) {
			list_for_each_safe(pos, tmp, &resource->sessions) {
				node = list_entry(pos, struct resman_node, slist);
				sess = node->session_ptr;
				if (sess->status == RESMAN_STATUS_ACQUIRED &&
					sess->prio == lowestprio) {
					dprintk(2, "preempt prio:%d\n",  sess->prio);
					selected = node;
					break;
				}
			}
		}
		if (!selected)
			dprintk(2, "fail to find node to preempt\n");
		else
			dprintk(2, "preempt node prio:%d\n", lowestprio);

		if (resman_preempt_session(curr, selected)) {
			new_score -= selected->s.codec_mm.score;
			lowestprio = curr->prio;
			/*if not enough need preempt more*/
			ret = true;
		}
	} while (selected &&
		(new_score + score > resource->d.codec_mm.total));

	return ret;
}

static bool resman_codec_mm_enough(struct resman_resource *resource,
				   __u32 score,
				   bool secure)
{
	bool enough = true;

	if (!secure && (codec_mm_get_free_size() >> 20 < score)) {
		enough = false;
		dprintk(2, "free size 0x%x\n", codec_mm_get_free_size());
	} else if (secure && ((codec_mm_get_tvp_free_size() + codec_mm_get_free_size()) >> 20)
			< score) {
		enough = false;
		dprintk(2, "free size 0x%x\n", codec_mm_get_tvp_free_size() +
			+ codec_mm_get_free_size());
	} else if (resource->value + score > resource->d.codec_mm.total)
		enough = false;

	return enough;
}

/**
 * resman_codec_mm_acquire() - resman_codec_mm_acquire
 *
 * @sess: session data
 * @resource: resource data
 * @node: node data
 * @preempt: preempt or not
 * @timeout: time out
 * @arg: arg rw
 * Return: true for success or false
 */
static bool resman_codec_mm_acquire(struct resman_session *sess,
				    struct resman_resource *resource,
				    struct resman_node *node,
				    bool preempt,
				    __u32 timeout,
				    char *arg)
{
	bool ret = false;
	long remain;
	__u32 score = resource->d.codec_mm.fhd;
	bool secure = false;
	char *opt;

	while ((opt = strsep(&arg, ","))) {
		if (!strncmp(opt, "size", 4)) {
			if (!resman_parser_kv(opt, "size", &score))
				dprintk(1, "parser size error\n");
		}
		else if (!strcmp(opt, "single"))
		/*single mode, 64M codec mm size is enough*/
			score = SINGLE_SIZE;
		else if (!strcmp(opt, "uhd"))
			score = resource->d.codec_mm.uhd;
		else if (!strcmp(opt, "secure"))
			secure = true;
	}
	dprintk(1, "%d score %d secure %s\n",
		sess->id,
		score,
		secure ? "yes" : "no");

	if (!resman_codec_mm_enough(resource, score, secure)) {
		if (preempt) {
			if (!timeout)
				timeout = preempt_timeout_ms;
			if (!resman_codec_mm_preempt(sess, resource,
						     score, secure))
				timeout = 0;
		}
		if (timeout) {
			atomic_inc(&resource->pending_acquire);
			mutex_unlock(&resource->lock);
			dprintk(1, "%d acquire wait\n", sess->id);
			remain = wait_event_interruptible_timeout(resource->wq_release,
					resman_codec_mm_enough(resource,
							       score,
							       secure),
					msecs_to_jiffies(timeout));
			dprintk(1, "%d acquire wait return %ld\n",
				sess->id,
				remain);
			mutex_lock(&resource->lock);
			atomic_dec(&resource->pending_acquire);
			wake_up_interruptible_all(&resource->wq_acquire);
		}
	}

	if (resman_codec_mm_enough(resource, score, secure)) {
		node->s.codec_mm.score = score;
		node->s.codec_mm.secure = secure;
		resource->value += score;
		if (secure)
			atomic_inc(&resource->d.codec_mm.counter_secure);
		else
			atomic_inc(&resource->d.codec_mm.counter_nonsecure);
		ret = true;
	}
	return ret;
}

/**
 * resman_codec_mm_release() - resman_codec_mm_release
 *
 * @sess: session data
 * @resource: resource data
 * @node: node data
 */
static void resman_codec_mm_release(struct resman_session *sess,
				    struct resman_resource *resource,
				    struct resman_node *node)
{
	if (node->s.codec_mm.secure)
		atomic_dec(&resource->d.codec_mm.counter_secure);
	else
		atomic_dec(&resource->d.codec_mm.counter_nonsecure);
	if (resource->value >= node->s.codec_mm.score)
		resource->value -= node->s.codec_mm.score;
}

/**
 * void resman_codec_mm_probe() - probe codec mm parameters
 *
 * @resource: resorce context
 */
static void resman_codec_mm_probe(struct resman_resource *resource)
{
	int total_bytes;
	int tvp_fhd, tvp_uhd;

	codec_mm_get_default_tvp_size(&tvp_fhd, &tvp_uhd);
	total_bytes = codec_mm_get_total_size();
	if (total_bytes >= tvp_fhd + tvp_uhd)
		resource->d.codec_mm.secure = 2;
	else
		resource->d.codec_mm.secure = 1;
	resource->d.codec_mm.total = total_bytes >> 20;
	resource->d.codec_mm.uhd = tvp_uhd >> 20;
	resource->d.codec_mm.fhd = tvp_fhd >> 20;
}

static bool resman_capacity_enough(struct resman_resource *resource,
				   __u32 size)
{
	bool enough = true;

	if ((resource->value + size) > resource->d.capacity.total)
		enough = false;
	dprintk(2, "[%s] value 0x%x score 0x%x total 0x%x\n",
			resource->name, resource->value,
			size, resource->d.capacity.total);
	return enough;
}

/**
 * resman_capacity_preempt() - resman_capacity_preempt
 *
 * @curr: current session data
 * @resource: resource data
 * @size: need resman to release resource size
 * Return: true for success or false
 */

static bool resman_capacity_preempt(struct resman_session *curr,
				    struct resman_resource *resource,
				    __u32 size)
{
	bool ret = false;
	struct resman_node *node, *selected;
	struct resman_session *sess;
	struct list_head *pos, *tmp;
	__u32 used_size;
	int lowestprio = curr->prio;

	do {
		selected = NULL;
		/*Calculate the available size*/
		used_size = resource->value;
		list_for_each_safe(pos, tmp, &resource->sessions) {
			node = list_entry(pos, struct resman_node, slist);
			sess = node->session_ptr;
			if (sess->status == RESMAN_STATUS_PREEMPTED)
				used_size -= node->s.capacity.use;
		}
		dprintk(2, "used_size 0x%x size 0x%x\n", used_size, size);
		if ((used_size + size) <= resource->d.capacity.total)
			break;

		/*find the lowest prio*/
		list_for_each_safe(pos, tmp, &resource->sessions) {
			node = list_entry(pos, struct resman_node, slist);
			sess = node->session_ptr;
			if (sess->status == RESMAN_STATUS_ACQUIRED && sess->prio > lowestprio) {
				selected = node;
				lowestprio = sess->prio;
			}
		}
		/*find the oldest same prio*/
		if (!selected) {
			list_for_each_safe(pos, tmp, &resource->sessions) {
				node = list_entry(pos, struct resman_node, slist);
				sess = node->session_ptr;
				if (sess->status == RESMAN_STATUS_ACQUIRED &&
					sess->prio == lowestprio) {
					selected = node;
					break;
				}
			}
		}
		if (!selected)
			dprintk(2, "fail to find node to preempt\n");
		else
			dprintk(2, "preempt node prio:%d\n", lowestprio);

		if (resman_preempt_session(curr, selected)) {
			used_size -= selected->s.capacity.use;
			lowestprio = curr->prio;
			/*if not enough need preempt more*/
			ret = true;
		}
	} while (selected &&
		(used_size + size > resource->d.capacity.total));

	return ret;
}

/**
 * resman_capacity_acquire() - resman_capacity_acquire
 *
 * @sess: session data
 * @resource: resource data
 * @node: node data
 * @preempt: preempt or not
 * @timeout: time out
 * @arg: arg rw
 * Return: true for success or false
 */
static bool resman_capacity_acquire(struct resman_session *sess,
				    struct resman_resource *resource,
				    struct resman_node *node,
				    bool preempt,
				    __u32 timeout,
				    char *arg)
{
	bool ret = false;
	long remain;
	__u32 size = 0;
	char *opt;

	while ((opt = strsep(&arg, ","))) {
		if (!strncmp(opt, "size", 4))
			if (!resman_parser_kv(opt, "size", &size))
				return ret;
	}
	if (size <= 0) {
		dprintk(0, "%d size %d parameter format:size:X!\n", sess->id, size);
		return ret;
	}

	if (!resman_capacity_enough(resource, size)) {
		if (preempt) {
			if (!timeout)
				timeout = preempt_timeout_ms;
			if (!resman_capacity_preempt(sess, resource, size))
				timeout = 0;
		}
		if (timeout) {
			atomic_inc(&resource->pending_acquire);
			mutex_unlock(&resource->lock);
			dprintk(1, "%d acquire wait\n", sess->id);
			remain =
			wait_event_interruptible_timeout(resource->wq_release,
				resman_capacity_enough(resource, size),
				msecs_to_jiffies(timeout));
			dprintk(1, "%d acquire wait return %ld\n",
				sess->id, remain);
			mutex_lock(&resource->lock);
			atomic_dec(&resource->pending_acquire);
			wake_up_interruptible_all(&resource->wq_acquire);
		}
	}

	if (resman_capacity_enough(resource, size)) {
		node->s.capacity.use = size;
		resource->value += size;
		ret = true;
	}
	return ret;
}

/**
 * resman_codec_mm_release() - resman_codec_mm_release
 *
 * @sess: session data
 * @resource: resource data
 * @node: node data
 */
static void resman_capacity_release(struct resman_session *sess,
				    struct resman_resource *resource,
				    struct resman_node *node)
{
	if (resource->value >= node->s.capacity.use)
		resource->value -= node->s.capacity.use;
}

static bool resman_create_resource(const char *name,
				   __u32 type,
				   char *arg)
{
	struct resman_resource *resource = NULL, *restmp = NULL;
	char *opt;
	int i;
	char r0 = 0;
	char r1 = 0;
	char r2 = 0;
	char r3 = 0;

	resource = kzalloc(sizeof(*resource), GFP_KERNEL);
	if (!resource || !name)
		goto error;

	resource->id = -1;
	for (i = 0; i < ARRAY_SIZE(resources_map) && resources_map[i].name;
			i++) {
		if (!strcmp(resources_map[i].name, name)) {
			restmp = resman_find_resource_by_id(resources_map[i].id);
			if (restmp) {
				dprintk(3, "notice: [%s] exist\n", name);
				kfree(resource);
				resource = restmp;
			} else {
				resource->id = resources_map[i].id;
				strncpy(resource->name, resources_map[i].name,
				sizeof(resource->name));
				resource->name[sizeof(resource->name) - 1] = 0;
			}
			break;
		}
	}
	if (i >= ARRAY_SIZE(resources_map)) {
		if (strlen(name) < 4) {
			dprintk(0, "At least 4 letters are required for resource name\n");
			goto error;
		}
		r0 = name[0];
		r1 = name[1];
		r2 = name[2];
		r3 = name[3];
		resource->id = (r0 << 24) | (r1 << 16) | (r2 << 8) | r3;
		restmp = resman_find_resource_by_id(resource->id);
		if (restmp) {
			dprintk(3, "notice: [%s] exist\n", name);
			kfree(resource);
			resource = restmp;
		} else {
			if (strlen(name) < 32)
				strncpy(resource->name, name, strlen(name));
			dprintk(3, "ext resource id[%d] name %s\n",
				resource->id, resource->name);
		}
	}
	if (resource->id < 0) {
		dprintk(0, "%s error, [%s] invalid\n", __func__, name);
		goto error;
	}

	switch (type) {
	default:
		dprintk(0, "%s error, unkonw type\n", __func__);
		goto error;
	case RESMAN_TYPE_COUNTER:
		resource->acquire = resman_counter_acquire;
		resource->release = resman_counter_release;

		if (!resman_parser_kv(arg, "avail",
				      &resource->d.counter.avail)) {
			goto error;
		}
		break;
	case RESMAN_TYPE_TOGGLE:
		resource->acquire = resman_toggle_acquire;
		resource->release = resman_toggle_release;
		break;
	case RESMAN_TYPE_TVP:
		resource->acquire = resman_tvp_acquire;
		resource->release = resman_tvp_release;
		break;
	case RESMAN_TYPE_CODEC_MM:
		resource->acquire = resman_codec_mm_acquire;
		resource->release = resman_codec_mm_release;

		opt = strsep(&arg, ",");
		if (!resman_parser_kv(opt, "total",
				      &resource->d.codec_mm.total)) {
			goto error;
		}
		if (resource->d.codec_mm.total) {
			opt = strsep(&arg, ",");
			if (!resman_parser_kv(opt, "uhd",
					      &resource->d.codec_mm.uhd)) {
				goto error;
			}
			opt = strsep(&arg, ",");
			if (!resman_parser_kv(opt, "fhd",
					      &resource->d.codec_mm.fhd)) {
				goto error;
			}
			opt = strsep(&arg, ",");
			if (!resman_parser_kv(opt, "secure",
					      &resource->d.codec_mm.secure)) {
				goto error;
			}
		} else {
			resman_codec_mm_probe(resource);
		}
		atomic_set(&resource->d.codec_mm.counter_secure, 0);
		atomic_set(&resource->d.codec_mm.counter_nonsecure, 0);
		break;
	case RESMAN_TYPE_CAPACITY_SIZE:
		resource->acquire = resman_capacity_acquire;
		resource->release = resman_capacity_release;

		opt = strsep(&arg, ",");
		if (!resman_parser_kv(opt, "total",
				&resource->d.capacity.total)) {
			goto error;
		}
		break;
	}
	resource->type = type;
	if (!restmp) {//resource exist just need update the parameter
		INIT_LIST_HEAD(&resource->sessions);
		mutex_init(&resource->lock);
		list_add_tail(&resource->list, &resources_head);
		init_waitqueue_head(&resource->wq_release);
		init_waitqueue_head(&resource->wq_acquire);
		atomic_set(&resource->pending_acquire, 0);
	}
	return true;
error:
	kfree(resource);
	return false;
}

static void resman_destroy_resource(struct resman_resource *resource)
{
	if (list_empty(&resource->sessions)) {
		list_del(&resource->list);
		kfree(resource);
	}
}

static void all_resource_uninit(void)
{
	struct resman_resource *resource;
	struct resman_node *node;
	struct list_head *pos, *tmp;
	struct list_head *pos1, *tmp1;

	mutex_lock(&resource_lock);
	kfree(resman_configs);
	resman_configs = NULL;
	list_for_each_safe(pos, tmp, &resources_head) {
		resource = list_entry(pos, struct resman_resource, list);

		list_for_each_safe(pos1, tmp1, &resource->sessions) {
			node = list_entry(pos, struct resman_node, slist);
			resman_release_resource(node);
		}
		resman_destroy_resource(resource);
	}
	mutex_unlock(&resource_lock);
}

#ifdef RESMAM_ENABLE_JSON
/**
 * resman_config_from_json() - parse resource config
 *
 * @buf: config string
 *
 * Parse config
 * The config is a json file that contains various of resource config arrays
 * Every resource config contains resource name and type and optional args
 *
 * config_string
 *     {   "resman_config": [
 *         {
 *          "name": "resname1",
 *          "type": type1,
 *          "args": "args1"
 *         },
 *         {
 *          "name": "resname2",
 *          "type": type1,
 *          "args": "args1"
 *         }
 *      ]
 *     }
 *
 * Return: true for success or false
 */

static bool resman_config_from_json(char *buf)
{
	bool ret = true;
	char *configs = kstrdup(buf, GFP_KERNEL);
	char *newconfig = NULL;
	struct cjson *root = NULL, *arrs_node = NULL;
	struct cjson *node = NULL, *name_node = NULL, *type_node = NULL, *args_node = NULL;

	if (!configs | !buf) {
		dprintk(0, "read %s\n", buf);
		return false;
	}

	if (configs[strlen(configs) - 1] == '\n')
		configs[strlen(configs) - 1] = 0;

	root = cjson_parse(configs);
	if (root == 0) {
		dprintk(0, "json null or has error, please check the json file:[ %s]\n", configs);
		ret = false;
		goto error;
	}
	arrs_node = cjson_getobjectitem(root, "resman_config");
	if (arrs_node == 0) {
		ret = false;
		goto error;
	}
	mutex_lock(&resource_lock);

	if (arrs_node->type == cjson_array) {
		node = arrs_node->child;
		while (node) {
			name_node = cjson_getobjectitem(node, "name");
			if (!name_node) {
				ret = false;
				mutex_unlock(&resource_lock);
				goto error;
			}
			dprintk(3, "%s:%s\n", name_node->string, name_node->valuestring);
			type_node = cjson_getobjectitem(node, "type");
			if (!type_node) {
				ret = false;
				mutex_unlock(&resource_lock);
				goto error;
			}
			dprintk(3, "type:%d\n", type_node->valueint);
			args_node = cjson_getobjectitem(node, "args");
			if (args_node)
				dprintk(3, "args:%s\n", args_node->valuestring);
			else
				dprintk(3, "args = NULL");
			if (!resman_create_resource(name_node->valuestring, type_node->valueint,
					args_node ? args_node->valuestring : NULL)) {
				ret = false;
				break;
			}
			node = node->next;
		}
	}

	mutex_unlock(&resource_lock);
	if (!ret) {
		dprintk(0, "%s parse config failed\n%s\n", __func__, buf);
		all_resource_uninit();
	} else if (resman_configs) {
		newconfig = kzalloc(strlen(resman_configs) + strlen(buf) + 1,
					GFP_KERNEL);
		if (newconfig) {
			memcpy(newconfig, resman_configs,
				strlen(resman_configs));
			memcpy(newconfig + strlen(resman_configs),
				buf, strlen(buf));
			kfree(resman_configs);
			resman_configs = newconfig;
		}
	} else {
		resman_configs = kstrdup(buf, GFP_KERNEL);
	}

error:
	cjson_delete(root);
	kfree(configs);
	return ret;
}
#else
/**
 * resman_parser_config() - parse resource config
 *
 * @buf: config string
 *
 * Parse config
 * The config is a string that contains various of resource config split by ';'
 * Every resource config contains resource name and key-value pairs split by ','
 *
 * config_string
 *     resource_config[];
 *         {
 *             resource_name,
 *             type: type_value,
 *             [key0: int_value0, key1:int_value1, ...]
 *         }
 * eg: "resource0,type:1;resource1,type:1,key0:1,key1:2;"
 *
 * Return: true for success or false
 */

static bool resman_parser_config(const char *buf)
{
	bool ret = true;
	char *config, *cur;
	char *name, *type_str, *arg;
	__u32 type;
	char *configs = kstrdup(buf, GFP_KERNEL);
	char *newconfig = NULL;

	if (!configs | !buf)
		return false;

	cur = configs;
	if (configs[strlen(configs) - 1] == '\n')
		configs[strlen(configs) - 1] = 0;

	mutex_lock(&resource_lock);
	while ((config = strsep(&cur, ";"))) {
		name = strsep(&config, ",");
		if (!name) {
			ret = false;
			break;
		}
		if (!strlen(name))
			break;
		type_str = strsep(&config, ",");
		if (!type_str) {
			ret = false;
			break;
		}
		;
		if (!resman_parser_kv(type_str, "type", &type)) {
			ret = false;
			break;
		}
		arg = config;
		if (!resman_create_resource(name, type, arg)) {
			ret = false;
			break;
		}
	}

	mutex_unlock(&resource_lock);
	if (!ret) {
		dprintk(0, "%s parse config failed\n%s\n", __func__, buf);
		all_resource_uninit();
	} else if (resman_configs) {
		newconfig = kzalloc(strlen(resman_configs) + strlen(buf) + 1,
					GFP_KERNEL);
		if (newconfig) {
			memcpy(newconfig, resman_configs,
				strlen(resman_configs));
			memcpy(newconfig + strlen(resman_configs),
				buf, strlen(buf));
			kfree(resman_configs);
			resman_configs = newconfig;
		}
	} else {
		resman_configs = kstrdup(buf, GFP_KERNEL);
	}
	kfree(configs);
	return ret;
}

#endif

static bool ext_resource_init(u32 conftype)
{
	bool ret = false;
	struct file *extfile = NULL;
	struct kstat stat;
	char *extfilename = (conftype == CUSTOMCONFIG) ? EXTCONFIGCUSTOMNAME : EXTCONFIGNAME;
	char *extconfig = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	unsigned int flen = 0, readlen = 0;
	int error;

	old_fs = get_fs();
	extfile = filp_open(extfilename, O_RDONLY, 0);

	if (IS_ERR_OR_NULL(extfile)) {
		/*First access the Android path , if fail access Linux path*/
		dprintk(2, "There(%s) is no ext config or read failed\n", extfilename);
		extfilename = (conftype == CUSTOMCONFIG) ?
						EXTCONFIGCUSTOMNAMELIX : EXTCONFIGNAMELIX;
		extfile = filp_open(extfilename, O_RDONLY, 0);
		if (IS_ERR_OR_NULL(extfile)) {
			dprintk(2, "There(%s) is no ext config or read failed\n", extfilename);
			return false;
		}
	}

	set_fs(KERNEL_DS);

	error = vfs_stat(extfilename, &stat);
	if (error) {
		dprintk(0, "vfs_stat fail %s\n", extfilename);
		ret = false;
		goto error;
	}
	if (flen > EXT_MAX_SIZE)
		dprintk(0, "extconfig file is too large%lld\n", stat.size);
	else
		dprintk(3, "%s file size %lld\n", __func__, stat.size);
	flen = (stat.size > EXT_MAX_SIZE) ? EXT_MAX_SIZE : stat.size;
	extconfig = kzalloc(flen + 8, GFP_KERNEL);
	if (extconfig) {
		readlen = vfs_read(extfile, extconfig,
					flen, &pos);
		if (readlen < flen)
			dprintk(0, "size %d read %d\n", flen, readlen);
#ifdef RESMAM_ENABLE_JSON
		ret = resman_config_from_json(extconfig);
#else
		resman_parser_config(extconfig);
#endif
		kfree(extconfig);
	}

error:
	filp_close(extfile, NULL);
	set_fs(old_fs);
	return ret;
}

static void all_resource_init(void)
{
#ifdef RESMAM_ENABLE_JSON
/*
 * default resman_config json
 * {
 *	   "resman_config": [
 *		   {
 *			   "name": "vfm_default",
 *			   "type": 1,
 *			   "args": "avail:1"
 *		   },
 *		   {
 *			   "name": "amvideo",
 *			   "type": 1,
 *			   "args": "avail:1"
 *		   },
 *		   {
 *			   "name": "videopip",
 *			   "type": 1,
 *			   "args": "avail:1"
 *		   },
 *		   {
 *			   "name": "tvp",
 *			   "type": 3
 *		   },
 *		   {
 *			   "name": "tsparser",
 *			   "type": 1,
 *			   "args": "avail:1"
 *		   },
 *		   {
 *			   "name": "codec_mm",
 *			   "type": 4,
 *			   "args": "total:0"
 *		   },
 *		   {
 *			   "name": "adc_pll",
 *			   "type": 1,
 *			   "args": "avail:1"
 *		   },
 *		   {
 *			   "name": "decoder",
 *			   "type": 1,
 *			   "args": "avail:9"
 *		   }
 *	   ]
 * }
 */
	char *default_configs = NULL;
	int len = 0, i = 0;
	char c[8][100] = {
	"{\"resman_config\":[{\"name\":\"vfm_default\",\"type\":1,\"args\":\"avail:1\"},",
	"{\"name\": \"amvideo\",\"type\": 1,\"args\": \"avail:1\"},",
	"{\"name\": \"videopip\",\"type\": 1,\"args\": \"avail:1\"},",
	"{\"name\": \"tvp\",\"type\": 3},",
	"{\"name\": \"tsparser\",\"type\": 1,\"args\": \"avail:1\"},",
	"{\"name\": \"codec_mm\",\"type\": 4,\"args\": \"total:0\"},",
	"{\"name\": \"adc_pll\",\"type\": 1,\"args\": \"avail:1\"},",
	"{\"name\": \"decoder\",\"type\": 1,\"args\": \"avail:9\"}]}"};

	len = strlen(c[0]) + strlen(c[1]) + strlen(c[2]) + strlen(c[3]) +
			strlen(c[4]) + strlen(c[5]) + strlen(c[6]) + strlen(c[7]);
	default_configs = kzalloc(len + 1, GFP_KERNEL);
	len = 0;
	if (default_configs) {
		for (i = 0; i < 8; i++) {
			memcpy(default_configs + len, c[i], strlen(c[i]));
			len +=  strlen(c[i]);
		}
	}
#else
	char *default_configs =
		"vfm_default,type:1,avail:1;"
		"amvideo,type:1,avail:1;"
		"videopip,type:1,avail:1;"
		"tvp,type:3;"
		"tsparser,type:1,avail:1;"
		"codec_mm,type:4,total:0;"
		"adc_pll,type:1,avail:1;"
		"decoder,type:1,avail:9;"
		"dmx,type:1,avail:4;"
		"di,type:1,avail:4;"
		"hwc,type:1,avail:9;";
#endif
	INIT_LIST_HEAD(&sessions_head);
	INIT_LIST_HEAD(&resources_head);
#ifdef RESMAM_ENABLE_JSON
	resman_config_from_json(default_configs);
	kfree(default_configs);
#else
	resman_parser_config(default_configs);
#endif
}

static long resman_ioctl_acquire(struct resman_session *sess,
				 unsigned long para)
{
	long r = 0;
	struct resman_resource *resource;
	struct resman_para __user *argp = (void __user *)para;
	struct resman_para resman;
	int selec_res;
	bool preempt = false;
	__u32 timeout = 0;
	char *arg = NULL;

	if (copy_from_user((void *)&resman, argp, sizeof(resman))) {
		/*Compat with old userspace lib*/
		selec_res = (int)para;
	} else {
		selec_res = resman.k;
		preempt = !!resman.v.acquire.preempt;
		timeout = resman.v.acquire.timeout;
		arg = resman.v.acquire.arg;
	}

	resource = resman_find_resource_by_id(selec_res);
	if (!resource)
		return -EINVAL;
	mutex_lock(&sess->lock);
	if (resman_find_node_by_resource_id(sess, selec_res)) {
		r = 0;
		goto beach;
	}

	if (!resman_acquire_resource(sess, resource, preempt, timeout, arg))
		r = -EBUSY;

beach:
	mutex_unlock(&sess->lock);
	return r;
}

static int resman_estimate_tvp_available(int size)
{
	int free_size = codec_mm_get_free_size();
	int tvp_fhd, tvp_uhd;
	int mode = 0;

	free_size += codec_mm_get_tvp_free_size();
	if (size <= 0) {
		codec_mm_get_default_tvp_size(&tvp_fhd, &tvp_uhd);
		if (free_size >= tvp_uhd)
			mode = 2;
		else if (free_size > tvp_fhd)
			mode = 1;
	} else {
		if (free_size >= size)
			mode = 2;
	}
	return mode;
}

static int resman_codec_mm_available(int issecure)
{
	int avail = 0;

	dprintk(2, "issecure %d\n", issecure);
	if (issecure == QUERY_NO_SECURE_BUFFER)
		avail = codec_mm_get_free_size() >> 20;
	if (issecure == QUERY_SECURE_BUFFER) {
		avail = (codec_mm_get_free_size() +
			codec_mm_get_tvp_free_size()) >> 20;
	}
	dprintk(2, "avail %d\n", avail);

	return avail;
}

static long resman_ioctl_query(struct resman_session *sess, unsigned long para)
{
	long r = 0;
	int selec_res = 0;
	struct resman_para __user *argp = (void __user *)para;
	char usage[32];
	struct resman_para resman;
	struct resman_resource *resource;

	memset(usage, 0, sizeof(usage));

	if (copy_from_user((void *)&resman, argp, sizeof(resman))) {
		return -EINVAL;
	}
	selec_res = resman.k;
	resource = resman_find_resource_by_id(selec_res);
	if (resource) {
		memcpy(resman.v.query.name,
			resource->name, sizeof(resman.v.query.name));
		resman.v.query.type = resource->type;

	switch (resource->type) {
	case RESMAN_TYPE_COUNTER:
		resman.v.query.value = resource->value;
		resman.v.query.avail =
			resource->d.counter.avail;
		break;
	case RESMAN_TYPE_CODEC_MM:
		resman.v.query.avail =
			resman_codec_mm_available(resman.v.query.value);
		resman.v.query.value = codec_mm_get_total_size() >> 20;
		break;
	case RESMAN_TYPE_CAPACITY_SIZE:
		resman.v.query.value = resource->value;
		resman.v.query.avail =
			(resource->d.capacity.total > resource->value) ?
			(resource->d.capacity.total - resource->value) : 0;
		break;
	case RESMAN_TYPE_TVP:
		resman.v.query.avail =
			resman_estimate_tvp_available(resman.v.query.value);
		break;
	default:
		break;
	}

	if (copy_to_user((void *)argp,
			&resman, sizeof(resman)))
		r = -EFAULT;
	} else {
		r = -EINVAL;
	}
	return r;
}

static long resman_ioctl_release(struct resman_session *sess,
				 unsigned long para)
{
	long r = 0;
	int selec_res = (int)para;
	struct resman_resource *resource;
	struct resman_node  *node;
	dprintk(2, "%d appname:%s, type=%d\n",
			sess->id,
			sess->app_name,
			sess->app_type);
	resource = resman_find_resource_by_id(selec_res);
	if (!resource)
		return -EINVAL;

	mutex_lock(&sess->lock);
	node = resman_find_node_by_resource_id(sess, selec_res);
	if (node)
		resman_release_resource(node);
	mutex_unlock(&sess->lock);
	return r;
}

static long resman_ioctl_setappinfo(struct resman_session *sess,
				    unsigned long para)
{
	long r = 0;
	struct app_info __user *argp = (void __user *)para;
	struct app_info appinfo;

	if (copy_from_user((void *)&appinfo, argp, sizeof(struct app_info)) ||
		!sess) {
		r = -EINVAL;
	} else {
		strncpy(sess->app_name, appinfo.app_name,
			sizeof(sess->app_name));
		sess->app_name[sizeof(sess->app_name) - 1] = '\0';
		sess->app_type = appinfo.app_type;
		sess->prio = appinfo.prio;
		dprintk(1, "%d appname:%s, type = %d prio = %d\n",
			sess->id,
			sess->app_name,
			sess->app_type,
			sess->prio);
	}
	return r;
}

static long resman_ioctl_checksupportres(struct resman_session *sess,
					 unsigned long para)
{
	long r = 0;
	struct resman_para __user *argp = (void __user *)para;
	struct resman_para resman;

	memset(&resman, 0, sizeof(resman));

	if (copy_from_user((void *)&resman, argp, sizeof(resman))) {
		r = -EINVAL;
	} else {
		if (resman.k > sizeof(resman.v.support.name) - 1)
			return -EINVAL;

		if (resman_find_resource_by_name(resman.v.support.name))
			return 0;
		r = -EINVAL;
	}

	return r;
}

static long resman_ioctl_load_res(struct resman_session *sess,
				 unsigned long para)
{
	long r = 0;
	struct res_item __user *argp = (void __user *)para;
	struct res_item item;
	int arglen = 0;
	char *itemarg = NULL;

	mutex_lock(&resource_lock);
	item.name[sizeof(item.name) - 1] = '\0';
	item.arg[sizeof(item.arg) - 1] = '\0';
	if (copy_from_user((void *)&item, argp, sizeof(item))) {
		dprintk(1, "load res FAIL!!\n");
	} else {
		item.name[sizeof(item.name) - 1] = '\0';
		item.arg[sizeof(item.arg) - 1] = '\0';
		if (strlen(item.name) > 0 && strlen(item.name) < STR_MAX_SIZE) {
			if (strlen(item.arg) > 0 && strlen(item.arg) < STR_MAX_SIZE)
				arglen = strlen(item.arg);
			if (arglen > 0 && arglen < STR_MAX_SIZE)
				itemarg = item.arg;
			if (!resman_create_resource(item.name, item.type, itemarg))
				r = -1;
		}
	}
	mutex_unlock(&resource_lock);
	return r;
}

static long resman_ioctl_release_all(struct resman_session *sess,
				     unsigned long para)
{
	long r = 0;
	struct resman_node  *node;
	struct list_head *pos, *tmp;

	mutex_lock(&sess->lock);
	list_for_each_safe(pos, tmp, &sess->resources) {
		node = list_entry(pos, struct resman_node, rlist);
		resman_release_resource(node);
	}
	mutex_unlock(&sess->lock);
	return r;
}

#define APPEND_ATTR_BUF(format, args...) { \
	size += snprintf(buf + size, PAGE_SIZE - size, format, args); \
}

static ssize_t usage_show(struct class *class,
			  struct class_attribute *attr, char *buf)
{
	struct list_head *pos1, *tmp1;
	struct list_head *pos2, *tmp2;
	struct resman_resource *resource;
	struct resman_session *sess;
	ssize_t size = 0;
	int i, j;

	mutex_lock(&resource_lock);
	j = 0;
	list_for_each_safe(pos1, tmp1, &resources_head) {
		resource = list_entry(pos1, struct resman_resource, list);
		for (i = 0; i < j; i++)
			APPEND_ATTR_BUF("%c ", '|')
		APPEND_ATTR_BUF("%s %d\n", resource->name, resource->value)
		j++;
	}
	for (i = 0; i < j; i++)
		APPEND_ATTR_BUF("%c ", '|')
	APPEND_ATTR_BUF("%-32s", "name")
	APPEND_ATTR_BUF("%-10s", "status")
	APPEND_ATTR_BUF("%-16s\n", "proc/PID/TID")

	mutex_lock(&sessions_lock);
	list_for_each_safe(pos2, tmp2, &sessions_head) {
		sess = list_entry(pos2, struct resman_session, list);
		list_for_each_safe(pos1, tmp1, &resources_head) {
			resource = list_entry(pos1,
					      struct resman_resource, list);
			if (resman_find_node_by_resource_id(sess, resource->id))
				APPEND_ATTR_BUF("%-2c", 'x')
			else
				APPEND_ATTR_BUF("%-2c", ' ')
		}
		APPEND_ATTR_BUF("%-32s", sess->app_name)
		switch (sess->status) {
		case RESMAN_STATUS_INITED:
			APPEND_ATTR_BUF("%-10s", "inited")
			break;
		case RESMAN_STATUS_ACQUIRED:
			APPEND_ATTR_BUF("%-10s", "acquired")
			break;
		case RESMAN_STATUS_PREEMPTED:
			APPEND_ATTR_BUF("%-10s", "preempted")
			break;
		default:
			APPEND_ATTR_BUF("%-10s", "unknown")
			break;
		}

		APPEND_ATTR_BUF("%s/%d/%d\n",
				sess->comm,
				sess->tgid,
				sess->pid)
	}
	mutex_unlock(&sessions_lock);
	mutex_unlock(&resource_lock);
	APPEND_ATTR_BUF("%s", "\n")
	return size;
}

static ssize_t config_show(struct class *class,
			   struct class_attribute *attr,
			   char *buf)
{
	ssize_t size = 0;
	if (resman_configs)
		APPEND_ATTR_BUF("%s\n", resman_configs)
	return size;
}

static ssize_t config_store(struct class *class,
			    struct class_attribute *attr,
			    const char *buf, size_t size)
{
	all_resource_uninit();
#ifdef RESMAM_ENABLE_JSON
	resman_config_from_json((char *)buf);
#else
	resman_parser_config(buf);
#endif
	return size;
}

static ssize_t extconfig_show(struct class *class,
			   struct class_attribute *attr,
			   char *buf)
{
	ssize_t size = 0;

	if (resman_configs)
		dprintk(0, "%s\n", resman_configs);
	return size;
}

static ssize_t extconfig_store(struct class *class,
			    struct class_attribute *attr,
			    const char *buf, size_t size)
{
	int r;
	int val;

	if (extloaded && resman_debug != 4) {
		dprintk(0, "extconfig has already loaded\n");
		return size;
	}

	r = kstrtoint(buf, 10, &val);
	if (r < 0) {
		dprintk(0, "extcofig format has error\n");
		return -EINVAL;
	}
	/*First access common config resman.json, then access custom config resmanext.json*/
	if (val == 1 && ext_resource_init(COMMOMCONFIG)) {
		extloaded = true;
		dprintk(3, "resman.json loading successful\n");
	}
	if (val == 1 && ext_resource_init(CUSTOMCONFIG)) {
		extloaded = true;
		dprintk(3, "resmanext.json loading successful\n");
	}

	return size;
}

static ssize_t ver_show(struct class *class,
			   struct class_attribute *attr,
			   char *buf)
{
	ssize_t size = 0;

	APPEND_ATTR_BUF("%d\n", RESMAN_VERSION)
	return RESMAN_VERSION;
}

static ssize_t res_show(struct class *class,
			   struct class_attribute *attr,
			   char *buf)
{
	ssize_t size = 0;
	struct list_head *pos1, *tmp1;
	struct resman_resource *resource;

	mutex_lock(&resource_lock);
	APPEND_ATTR_BUF("%-32s", "name")
	APPEND_ATTR_BUF("%-10s\n", "avail")
	list_for_each_safe(pos1, tmp1, &resources_head) {
		resource = list_entry(pos1, struct resman_resource, list);
		APPEND_ATTR_BUF("%-32s", resource->name)
		if (resource->type == RESMAN_TYPE_COUNTER)
			APPEND_ATTR_BUF("%d", resource->d.counter.avail)
		else if (resource->type == RESMAN_TYPE_CODEC_MM)
			APPEND_ATTR_BUF("total%d uhd%d fhd%d secure%d, counts%d countnon%d",
				resource->d.codec_mm.total, resource->d.codec_mm.uhd,
				resource->d.codec_mm.fhd, resource->d.codec_mm.secure,
				resource->d.codec_mm.counter_secure.counter,
				resource->d.codec_mm.counter_nonsecure.counter)
		else if (resource->type == RESMAN_TYPE_CAPACITY_SIZE)
			APPEND_ATTR_BUF("%d", resource->d.capacity.total)
		APPEND_ATTR_BUF("%s", "\n")
	}
	mutex_unlock(&resource_lock);
	return size;
}

#undef APPEND_ATTR_BUF

/* ------------------------------------------------------------------
 * File operations for the device
 * ------------------------------------------------------------------
 */
int resman_open(struct inode *inode, struct file *filp)
{
	struct resman_session *sess;

	sess = resman_open_session();
	if (!sess)
		return -ENOMEM;

	filp->private_data = sess;
	dprintk(2, "%d open\n",
		sess->id);
	return 0;
}

ssize_t resman_read(struct file *filp, char *buf, size_t len, loff_t *off)
{
	ssize_t ret = -EFAULT;
	struct resman_session *sess;
	struct resman_event *event;

	sess = filp->private_data;
	if (len < sizeof(__u32))
		return -EINVAL;
	mutex_lock(&sess->lock);
	event = list_first_entry_or_null(&sess->events,
					 struct resman_event, list);
	if (event) {
		if (!copy_to_user(buf, &event->type, sizeof(event->type))) {
			ret = sizeof(event->type);
			list_del(&event->list);
			kfree(event);
		}
	}
	mutex_unlock(&sess->lock);
	return ret;
}

unsigned int resman_poll(struct file *filp, struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct resman_session *sess;

	sess = filp->private_data;
	poll_wait(filp, &sess->wq_event, wait);
	if (!list_empty(&sess->events))
		mask |= POLL_IN | POLLRDNORM;

	return mask;
}

long resman_ioctl(struct file *filp, unsigned int cmd, unsigned long para)
{
	long retval = 0;
	struct resman_session *sess;

	if (_IOC_TYPE(cmd) != RESMAN_IOC_MAGIC) {
		dprintk(0, "[%s] error cmd!\n", __func__);
		return -EINVAL;
	}
	sess = filp->private_data;
	if (!sess)
		return -EINVAL;
	dprintk(3, "%d ioctl cmd:%x\n", sess->id, cmd);
	switch (cmd) {
	case RESMAN_IOC_ACQUIRE_RES:
		retval = resman_ioctl_acquire(sess, para);
		break;
	case RESMAN_IOC_QUERY_RES:
		retval = resman_ioctl_query(sess, para);
		break;
	case RESMAN_IOC_RELEASE_RES:
		retval = resman_ioctl_release(sess, para);
		break;
	case RESMAN_IOC_SETAPPINFO:
		retval = resman_ioctl_setappinfo(sess, para);
		break;
	case RESMAN_IOC_SUPPORT_RES:
		retval = resman_ioctl_checksupportres(sess, para);
		break;
	case RESMAN_IOC_RELEASE_ALL:
		retval = resman_ioctl_release_all(sess, para);
		break;
	case RESMAN_IOC_LOAD_RES:
		retval = resman_ioctl_load_res(sess, para);
		break;
	default:
		retval = -EINVAL;
		break;
	}
	return retval;
}

#ifdef CONFIG_COMPAT
static long resman_compat_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	long ret = 0;

	ret = resman_ioctl(file, cmd, (ulong)compat_ptr(arg));
	return ret;
}
#endif

int resman_close(struct inode *inode, struct file *filp)
{
	struct resman_session *sess;

	sess = filp->private_data;
	if (sess) {
		dprintk(2, "%d close, proc:%s/%d/%d\n",
			sess->id,
			sess->comm,
			sess->tgid,
			sess->pid);
		resman_close_session(sess);
	}
	filp->private_data = NULL;
	return 0;
}

const struct file_operations resman_fops = {
	.owner = THIS_MODULE,
	.open = resman_open,
	.read = resman_read,
	.poll = resman_poll,
	.release = resman_close,
	.unlocked_ioctl = resman_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = resman_compat_ioctl,
#endif
};

/* -----------------------------------------------------------------
 * Initialization and module stuff
 * -----------------------------------------------------------------
 */
static struct class_attribute resman_class_attrs[] = {
	__ATTR_RO(usage),
	__ATTR_RW(config),
	__ATTR_RO(ver),
	__ATTR_RW(extconfig),
	__ATTR_RO(res),
	__ATTR_NULL
};

static void remove_resmansub_attrs(struct class *class)
{
	int i = 0;

	for (i = 0; resman_class_attrs[i].attr.name; i++)
		class_remove_file(class, &resman_class_attrs[i]);
}

static void create_resmansub_attrs(struct class *class)
{
	int i = 0;

	for (i = 0; resman_class_attrs[i].attr.name; i++) {
		if (class_create_file(class, &resman_class_attrs[i]) < 0)
			break;
	}
}

int __init resman_init(void)
{
	int result;
	struct device *resman_dev;

	result = alloc_chrdev_region(&resman_devno, 0, 1, DEVICE_NAME);

	if (result < 0) {
		dprintk(0, "failed to allocate amresource_mgr dev region\n");
		result = -ENODEV;
		return result;
	}

	resman_class = class_create(THIS_MODULE, DEVICE_CLASS_NAME);

	if (IS_ERR(resman_class)) {
		result = PTR_ERR(resman_class);
		goto fail1;
	}
	create_resmansub_attrs(resman_class);

	resman_cdev = kmalloc(sizeof(*resman_cdev), GFP_KERNEL);
	if (!resman_cdev) {
		result = -ENOMEM;
		goto fail2;
	}
	cdev_init(resman_cdev, &resman_fops);
	resman_cdev->owner = THIS_MODULE;
	result = cdev_add(resman_cdev, resman_devno, 1);
	if (result) {
		dprintk(0, "failed to add cdev\n");
		goto fail3;
	}
	resman_dev = device_create(resman_class,
				  NULL, MKDEV(MAJOR(resman_devno), 0),
				  NULL, DEVICE_NAME);

	if (IS_ERR(resman_dev)) {
		pr_err("Can't create %s device\n", DEVICE_NAME);
		result = PTR_ERR(resman_dev);
		goto fail4;
	}
	all_resource_init();
	dprintk(1, "%s init success\n", DEVICE_NAME);

	return 0;
fail4:
	cdev_del(resman_cdev);
fail3:
	kfree(resman_cdev);
fail2:
	remove_resmansub_attrs(resman_class);
	class_destroy(resman_class);
fail1:
	unregister_chrdev_region(resman_devno, 1);
	return result;
}

void __exit resman_exit(void)
{
	all_resource_uninit();
	kfree(resman_configs);
	cdev_del(resman_cdev);
	kfree(resman_cdev);
	device_destroy(resman_class, MKDEV(MAJOR(resman_devno), 0));
	remove_resmansub_attrs(resman_class);
	class_destroy(resman_class);
	unregister_chrdev_region(resman_devno, 1);
	dprintk(1, "uninstall sourmanage module\n");
}

MODULE_LICENSE("GPL");
