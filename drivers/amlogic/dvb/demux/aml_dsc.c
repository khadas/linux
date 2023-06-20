// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/fcntl.h>
#include <asm/irq.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/pinctrl/consumer.h>
#include <linux/reset.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/clk.h>
#include <linux/dvb/ca.h>
#include <linux/dvb/dmx.h>
#include <linux/version.h>
#include <linux/compat.h>
#include "aml_dvb.h"
#include "dmx_log.h"

//#include "sc2_demux/dvb_reg.h"
#include "aml_dsc.h"
#include "sc2_demux/sc2_control.h"

#define DSC_CHANNEL_NUM 8

#define DSC_STATE_FREE      0
#define DSC_STATE_READY     3
#define DSC_STATE_GO        4

#define DSC_COUNT         8

struct dsc_channel {
	struct aml_dsc *dsc;

	int state;
	unsigned int id;
	enum ca_sc2_dsc_type dsc_type;
	int index;
	int index00;

	int sid;
	int pid;
	char loop;
	enum ca_sc2_algo_type algo;
	struct dsc_channel *next;
};

#define MAX_DSC_PID_TABLE_NUM		(64)

static struct dsc_pid_table dsc_tsn_pid_table[MAX_DSC_PID_TABLE_NUM];
static struct dsc_pid_table dsc_tsd_pid_table[MAX_DSC_PID_TABLE_NUM];
static struct dsc_pid_table dsc_tse_pid_table[MAX_DSC_PID_TABLE_NUM];

#define dprint_i(fmt, args...)   \
	dprintk(LOG_ERROR, debug_dsc, fmt, ## args)
#define dprint(fmt, args...)   \
	dprintk(LOG_ERROR, debug_dsc, "dsc:" fmt, ## args)
#define pr_dbg(fmt, args...)   \
	dprintk(LOG_DBG, debug_dsc, "dsc:" fmt, ## args)

MODULE_PARM_DESC(debug_dsc, "\n\t\t Enable dsc information");
static int debug_dsc;
module_param(debug_dsc, int, 0644);

static int s_init_flag;
static unsigned int global_ch_id;

static void _init_per_table(struct dsc_pid_table *head, int n)
{
	int i = 0;

	memset(head, 0, sizeof(struct dsc_pid_table) * n);

	for (i = 0; i < n; i++) {
		head->id = i;
		head++;
	}
}

static void _init_dsc_table(void)
{
	if (s_init_flag == 0) {
		_init_per_table(dsc_tsn_pid_table, MAX_DSC_PID_TABLE_NUM);
		_init_per_table(dsc_tsd_pid_table, MAX_DSC_PID_TABLE_NUM);
		_init_per_table(dsc_tse_pid_table, MAX_DSC_PID_TABLE_NUM);
		s_init_flag = 1;
	}
}

static struct dsc_pid_table *_get_dsc_pid_table(int index, int dsc_type)
{
	struct dsc_pid_table *table;

	if (index >= MAX_DSC_PID_TABLE_NUM || index < 0)
		return NULL;

	if (dsc_type == CA_DSC_COMMON_TYPE)
		table = dsc_tsn_pid_table;
	else if (dsc_type == CA_DSC_TSD_TYPE)
		table = dsc_tsd_pid_table;
	else
		table = dsc_tse_pid_table;

	return &table[index];
}

static int _malloc_dsc_table_index(int dsc_type)
{
	int i = 0;
	struct dsc_pid_table *table;

	if (dsc_type == CA_DSC_COMMON_TYPE)
		table = dsc_tsn_pid_table;
	else if (dsc_type == CA_DSC_TSD_TYPE)
		table = dsc_tsd_pid_table;
	else
		table = dsc_tse_pid_table;

	for (i = 0; i < MAX_DSC_PID_TABLE_NUM; i++) {
		if (table[i].used == 0) {
			table[i].used = 1;
			table[i].valid = 0;
			break;
		}
	}
	if (i == MAX_DSC_PID_TABLE_NUM) {
		dprint("%s fail\n", __func__);
		return -1;
	}
	return i;
}

static int _free_dsc_table_index(int index, int dsc_type)
{
	struct dsc_pid_table *table;
	struct dsc_pid_table *ptmp = NULL;
	int id = 0;

	pr_dbg("%s enter\n", __func__);

	if (index >= MAX_DSC_PID_TABLE_NUM)
		return -1;

	if (dsc_type == CA_DSC_COMMON_TYPE)
		table = dsc_tsn_pid_table;
	else if (dsc_type == CA_DSC_TSD_TYPE)
		table = dsc_tsd_pid_table;
	else
		table = dsc_tse_pid_table;

	ptmp = &table[index];
	id = ptmp->id;
	memset(ptmp, 0, sizeof(struct dsc_pid_table));
	ptmp->id = id;

	dsc_config_pid_table(ptmp, dsc_type);

	pr_dbg("%s exit\n", __func__);

	return 0;
}

static int _add_chan_to_list(struct aml_dsc *dsc, struct dsc_channel *chan)
{
	if (dsc->dsc_channels)
		chan->next = dsc->dsc_channels;

	dsc->dsc_channels = chan;
	return 0;
}

static int _remove_chan_from_list(struct aml_dsc *dsc, struct dsc_channel *chan)
{
	struct dsc_channel *ptmp = dsc->dsc_channels;
	struct dsc_channel *pretmp = dsc->dsc_channels;

	pr_dbg("%s\n", __func__);
	while (ptmp) {
		if (ptmp == chan) {
			if (ptmp == dsc->dsc_channels)
				dsc->dsc_channels = dsc->dsc_channels->next;
			else
				pretmp->next = chan->next;
		}
		pretmp = ptmp;
		ptmp = ptmp->next;
	}
	return 0;
}

static struct dsc_channel *_get_chan_from_list(struct aml_dsc *dsc, int id)
{
	struct dsc_channel *ptmp = dsc->dsc_channels;

	pr_dbg("%s\n", __func__);
	while (ptmp) {
		if (ptmp->id == id)
			return ptmp;
		ptmp = ptmp->next;
	}
	return NULL;
}

static int _dsc_chan_alloc(struct aml_dsc *dsc,
			   unsigned int pid, int algo, int dsc_type,
			   unsigned int *ca_index, char loop)
{
	char index = 0;

	struct dsc_channel *ch = vmalloc(sizeof(*ch));

	pr_dbg("%s pid:0x%0x, algo:%d, dsc_type:%d\n",
	       __func__, pid, algo, dsc_type);

	if (!ch) {
		dprint("%s vmalloc fail\n", __func__);
		return -ENOMEM;
	}
	memset(ch, 0, sizeof(struct dsc_channel));
	ch->dsc = dsc;

	ch->sid = dsc->sid;

	if (loop && ch->sid >= 32)
		dprint("warn: double descramble sid should small than 32\n");

	if (loop)
		ch->sid = ch->sid >= 32 ? ch->sid : (ch->sid + 32);

	ch->loop = loop;
	ch->pid = pid;
	ch->dsc_type = dsc_type;
	ch->algo = algo;

	index = _malloc_dsc_table_index(dsc_type);
	if (index == -1) {
		dprint("%s _malloc_dsc_table_index fail\n", __func__);
		vfree(ch);
		return -1;
	}
	ch->state = DSC_STATE_READY;
	ch->index = index;
	ch->index00 = -1;
	ch->next = NULL;
	ch->id = global_ch_id;

	_add_chan_to_list(dsc, ch);
	*ca_index = global_ch_id;

	global_ch_id++;
	return 0;
}

static void _dsc_chan_free(struct dsc_channel *ch)
{
	struct aml_dsc *dsc = (struct aml_dsc *)ch->dsc;

	pr_dbg("%s enter\n", __func__);

	if (ch->state == DSC_STATE_FREE)
		return;

	_remove_chan_from_list(dsc, ch);

	_free_dsc_table_index(ch->index, ch->dsc_type);
	if (ch->index00 != -1) {
		_free_dsc_table_index(ch->index00, ch->dsc_type);
		ch->index00 = -1;
	}

	vfree(ch);
	pr_dbg("%s exit\n", __func__);
}

static int _dsc_chan_set_key(struct dsc_channel *ch,
			     enum ca_sc2_key_type parity, u32 key_index)
{
	struct dsc_pid_table *ptmp = NULL;
	struct dsc_pid_table *ptmp1 = NULL;
	int handle_has_iv = 0;
	int type_has_iv = 0;
	u32 kte;
#define KTE_MAX_NUM				256
#define IV_FLAG_OFFSET    31
#define HANDLE_TO_KTE(h) ((h) & (KTE_MAX_NUM - 1))

	/*
	 * handle definition
	 | Name      | Bits   | Notes                    |
	 | --------- | ------ | ------------------------ |
	 | IV flag   | 31     | 0: not IV 1: is IV       |
	 | RFU       | [20:8] | Reserved for future      |
	 | key table | [7:0]  | the real key table entry |
	 */
	pr_dbg("%s parity:%d, handle:%#x\n", __func__, parity, key_index);
	kte = HANDLE_TO_KTE(key_index);
	handle_has_iv = key_index >> IV_FLAG_OFFSET;
	if (parity == CA_KEY_ODD_IV_TYPE ||
	    parity == CA_KEY_EVEN_IV_TYPE || parity == CA_KEY_00_IV_TYPE)
		type_has_iv = 1;

	if (handle_has_iv != type_has_iv) {
		pr_dbg("%s parity:%d, handle:%#x, failed: not match\n",
		       __func__, parity, key_index);
		return -1;
	}

	if (parity == CA_KEY_00_TYPE || parity == CA_KEY_00_IV_TYPE) {
		if (ch->index00 == -1) {
			ch->index00 = _malloc_dsc_table_index(ch->dsc_type);
			if (ch->index00 == -1) {
				dprint("%s _malloc_dsc_table_index fail\n",
				       __func__);
				return -1;
			}
		}
		ptmp = _get_dsc_pid_table(ch->index00, ch->dsc_type);
		if (!ptmp) {
			dprint("%s _get_dsc_pid_table fail\n", __func__);
			return -1;
		}
		ptmp->scb00 = 1;
		ptmp1 = _get_dsc_pid_table(ch->index, ch->dsc_type);
		if (ptmp1 && ptmp1->scb_user == 1) {
			ptmp->scb_as_is = ptmp1->scb_as_is;
			ptmp->scb_out = ptmp1->scb_out;
		} else {
			ptmp->scb_as_is = 0;
			ptmp->scb_out = 0;
		}
	} else {
		ptmp = _get_dsc_pid_table(ch->index, ch->dsc_type);
		if (!ptmp) {
			dprint("%s _get_dsc_pid_table fail\n", __func__);
			return -1;
		}
		ptmp->scb00 = 0;
		if (ptmp->scb_user == 0) {
			ptmp->scb_as_is = 0;
			ptmp->scb_out = 0;
			/*tse, need set scb */
			if (ch->dsc_type == CA_DSC_TSE_TYPE) {
				if (parity == CA_KEY_EVEN_TYPE)
					ptmp->scb_out = 0x2;
				if (parity == CA_KEY_ODD_TYPE)
					ptmp->scb_out = 0x3;
			} else {
				ptmp->scb_as_is = 0;
				ptmp->scb_out = 0;
			}
		}
	}
	ptmp->valid = 1;
	ptmp->algo = ch->algo;
	ptmp->sid = ch->sid;
	ptmp->pid = ch->pid;

	if (parity == CA_KEY_EVEN_TYPE)
		ptmp->kte_even_00 = kte;
	else if (parity == CA_KEY_EVEN_IV_TYPE)
		ptmp->even_00_iv = kte;
	else if (parity == CA_KEY_ODD_TYPE)
		ptmp->kte_odd = kte;
	else if (parity == CA_KEY_ODD_IV_TYPE)
		ptmp->odd_iv = kte;
	else if (parity == CA_KEY_00_IV_TYPE)
		ptmp->even_00_iv = kte;
	else
		ptmp->kte_even_00 = kte;

	if (ptmp->algo != (CA_ALGO_UNKNOWN + 1))
		dsc_config_pid_table(ptmp, ch->dsc_type);
	return 0;
}

static int _dsc_chan_set_scb(struct dsc_channel *ch, u8 scb_out, u8 scb_as_is)
{
	struct dsc_pid_table *ptmp = NULL;

	ptmp = _get_dsc_pid_table(ch->index, ch->dsc_type);
	if (!ptmp) {
		dprint("%s _get_dsc_pid_table fail\n", __func__);
		return -1;
	}
	ptmp->scb_as_is = scb_as_is;
	ptmp->scb_out = scb_out;
	ptmp->scb_user = 1;
	if (ptmp->valid)
		dsc_config_pid_table(ptmp, ch->dsc_type);

	if (ch->index00 != -1) {
		ptmp = _get_dsc_pid_table(ch->index00, ch->dsc_type);
		if (!ptmp) {
			dprint("%s _get_dsc_pid_table fail\n", __func__);
			return -1;
		}
		ptmp->scb_as_is = scb_as_is;
		ptmp->scb_out = scb_out;
		if (ptmp->valid)
			dsc_config_pid_table(ptmp, ch->dsc_type);
	}
	return 0;
}

static int _dsc_chan_set_algo(struct dsc_channel *ch,
		enum ca_sc2_algo_type algo)
{
	struct dsc_pid_table *ptmp = NULL;

	ptmp = _get_dsc_pid_table(ch->index, ch->dsc_type);
	if (!ptmp) {
		dprint("%s _get_dsc_pid_table fail\n", __func__);
		return -1;
	}
	ptmp->algo = algo;
	if (ptmp->valid)
		dsc_config_pid_table(ptmp, ch->dsc_type);
	return 0;
}

static int _dsc_chan_set_sid(struct dsc_channel *ch,
		int sid)
{
	struct dsc_pid_table *ptmp = NULL;

	ch->sid = sid;
	if (ch->loop && ch->sid >= 32)
		dprint("warn: double descramble sid should small than 32\n");

	if (ch->loop)
		ch->sid = ch->sid >= 32 ? ch->sid : (ch->sid + 32);

	ptmp = _get_dsc_pid_table(ch->index, ch->dsc_type);
	if (!ptmp) {
		dprint("%s _get_dsc_pid_table fail\n", __func__);
		return -1;
	}
	ptmp->sid = ch->sid;
	if (ptmp->valid)
		dsc_config_pid_table(ptmp, ch->dsc_type);

	if (ch->index00 != -1) {
		ptmp = _get_dsc_pid_table(ch->index00, ch->dsc_type);
		if (!ptmp) {
			dprint("%s _get_dsc_pid_table fail\n", __func__);
			return -1;
		}
		ptmp->sid = sid;
		if (ptmp->valid)
			dsc_config_pid_table(ptmp, ch->dsc_type);
	}

	return 0;
}

static void _dsc_reset(struct aml_dsc *dsc)
{
	dprint("dsc_reset not support\n");
}

static int _dvb_dsc_open(struct inode *inode, struct file *file)
{
	int err;

	err = dvb_generic_open(inode, file);
	if (err < 0)
		return err;

	return 0;
}

static int handle_desc_ext(struct aml_dsc *dsc, struct ca_sc2_descr_ex *d)
{
	int ret = -EINVAL;

	switch (d->cmd) {
	case CA_ALLOC:{
//              pr_dbg("%s CA_ALLOC\n", __func__);
			if (d->params.alloc_params.algo > CA_ALGO_UNKNOWN) {
				ret = -EINVAL;
				break;
			}
			if (d->params.alloc_params.loop != 1 &&
				d->params.alloc_params.loop != 0) {
				dprint("alloc_params.loop:%d err\n",
					d->params.alloc_params.loop);
				ret = -EINVAL;
				break;
			}
			if (!get_demux_feature(SUPPORT_TSD) &&
				d->params.alloc_params.dsc_type == CA_DSC_TSD_TYPE) {
				dprint("not sc2, tsd have removed\n");
				ret = -EINVAL;
				break;
			}

			ret = _dsc_chan_alloc(dsc,
					      d->params.alloc_params.pid & 0x1FFF,
					      d->params.alloc_params.algo + 1,
					      d->params.alloc_params.dsc_type,
					      &d->params.alloc_params.ca_index,
					      d->params.alloc_params.loop);
			pr_dbg("%s CA_ALLOC:%d, loop:%d\n", __func__,
			       d->params.alloc_params.ca_index,
			       d->params.alloc_params.loop);
		}
		break;
	case CA_FREE:{
			struct dsc_channel *ch;

			pr_dbg("%s CA_FREE:%d\n", __func__,
			       d->params.free_params.ca_index);
			ch = _get_chan_from_list(dsc,
						 d->params.free_params.ca_index);
			if (ch)
				_dsc_chan_free(ch);

			ret = 0;
		}
		break;
	case CA_KEY:{
			struct dsc_channel *ch;

//              pr_dbg("%s CA_KEY ca_index:%d\n", __func__,
//                      d->params.alloc_params.ca_index);
			ch = _get_chan_from_list(dsc,
						 d->params.key_params.ca_index);
			if (ch) {
				pr_dbg("%s KEY parity:%d, index:%d\n",
				       __func__,
				       d->params.key_params.parity,
				       d->params.key_params.key_index);
				ret = _dsc_chan_set_key(ch,
							d->params.key_params.parity,
							d->params.key_params.key_index);
			}
		}
		break;
	case CA_GET_STATUS:{
			int dsc_type = d->params.key_params.ca_index;

			if (dsc_type > CA_DSC_TSE_TYPE) {
				d->params.key_params.key_index = 0xffffffff;
				ret = -1;
			} else {
				d->params.key_params.key_index =
						dsc_get_status(dsc_type);
				ret = 0;
			}
		}
		break;
	case CA_SET_SCB:{
			struct dsc_channel *ch;

			ch = _get_chan_from_list(dsc,
						 d->params.scb_params.ca_index);
			if (ch) {
				pr_dbg("%s scb:%d, scb_as_is:%d\n",
				       __func__,
				       d->params.scb_params.ca_scb,
				       d->params.scb_params.ca_scb_as_is);
				ret = _dsc_chan_set_scb(ch,
							d->params.scb_params.ca_scb,
							d->params.scb_params.ca_scb_as_is);
			}
		}
		break;
	case CA_SET_ALGO:{
			struct dsc_channel *ch;

			ch = _get_chan_from_list(dsc,
					 d->params.algo_params.ca_index);
			if (ch) {
				pr_dbg("%s algo:%d\n",
				       __func__,
				       d->params.algo_params.algo);
				ret = _dsc_chan_set_algo(ch,
						d->params.algo_params.algo + 1);
			}
		}
		break;
	default:
		break;
	}
	return ret;
}

static int _dvb_dsc_do_ioctl(struct file *file, unsigned int cmd, void *parg)
{
	struct dvb_device *dvbdev = file->private_data;
	struct aml_dsc *dsc = dvbdev->priv;
	int ret = -EINVAL;

	if (mutex_lock_interruptible(&dsc->mutex))
		return -ERESTARTSYS;

	switch (cmd) {
	case CA_RESET:
		_dsc_reset(dsc);
		break;
	case CA_GET_CAP:{
			struct ca_caps *cap = parg;

			cap->slot_num = 1;
			cap->slot_type = CA_DESCR;
			cap->descr_num = DSC_CHANNEL_NUM;
			cap->descr_type = 0;
			break;
		}
	case CA_GET_SLOT_INFO:{
			struct ca_slot_info *slot = parg;

			slot->num = 1;
			slot->type = CA_DESCR;
			slot->flags = 0;
			break;
		}
	case CA_GET_DESCR_INFO:{
			struct ca_descr_info *descr = parg;

			descr->num = DSC_CHANNEL_NUM;
			descr->type = 0;
			break;
		}
	case CA_SC2_SET_DESCR_EX:{
			ret =
			    handle_desc_ext(dsc,
					    (struct ca_sc2_descr_ex *)parg);
			break;
		}
	}

	mutex_unlock(&dsc->mutex);

	return ret;
}

static int _dvb_dsc_usercopy(struct file *file,
			     unsigned int cmd, unsigned long arg,
			     int (*func)(struct file *file,
					  unsigned int cmd, void *arg))
{
	char sbuf[128];
	void *mbuf = NULL;
	void *parg = NULL;
	int err = -EINVAL;

	/*  Copy arguments into temp kernel buffer  */
	switch (_IOC_DIR(cmd)) {
	case _IOC_NONE:
		/*
		 * For this command, the pointer is actually an integer
		 * argument.
		 */
		parg = (void *)arg;
		break;
	case _IOC_READ:	/* some v4l ioctls are marked wrong ... */
	case _IOC_WRITE:
	case (_IOC_WRITE | _IOC_READ):
		if (_IOC_SIZE(cmd) <= sizeof(sbuf)) {
			parg = sbuf;
		} else {
			/* too big to allocate from stack */
			mbuf = kmalloc(_IOC_SIZE(cmd), GFP_KERNEL);
			if (!mbuf)
				return -ENOMEM;
			parg = mbuf;
		}

		err = -EFAULT;
		if (copy_from_user(parg, (void __user *)arg, _IOC_SIZE(cmd)))
			goto out;
		break;
	}

	/* call driver */
	err = func(file, cmd, parg);
	if (err == -ENOIOCTLCMD)
		err = -ENOTTY;

	if (err < 0)
		goto out;

	/*  Copy results into user buffer  */
	switch (_IOC_DIR(cmd)) {
	case _IOC_READ:
	case (_IOC_WRITE | _IOC_READ):
		if (copy_to_user((void __user *)arg, parg, _IOC_SIZE(cmd)))
			err = -EFAULT;
		break;
	}

out:
	kfree(mbuf);
	return err;
}

static long _dvb_dsc_ioctl(struct file *file, unsigned int cmd,
			   unsigned long arg)
{
	return _dvb_dsc_usercopy(file, cmd, arg, _dvb_dsc_do_ioctl);
}

static int _dvb_dsc_release(struct inode *inode, struct file *file)
{
	struct dvb_device *dvbdev = file->private_data;
	struct aml_dsc *dsc = dvbdev->priv;
	struct dsc_channel *ptmp = dsc->dsc_channels;
	struct dsc_channel *ch;

	if (mutex_lock_interruptible(&dsc->mutex))
		return -ERESTARTSYS;

	while (ptmp) {
		ch = ptmp;
		ptmp = ptmp->next;
		if (ch)
			_dsc_chan_free(ch);
	}

	mutex_unlock(&dsc->mutex);

	dvb_generic_release(inode, file);

	return 0;
}

#ifdef CONFIG_COMPAT
static long _dvb_dsc_compat_ioctl(struct file *filp,
				  unsigned int cmd, unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = _dvb_dsc_ioctl(filp, cmd, args);
	return ret;
}
#endif

static const struct file_operations dvb_dsc_fops = {
	.owner = THIS_MODULE,
	.read = NULL,
	.write = NULL,
	.unlocked_ioctl = _dvb_dsc_ioctl,
	.open = _dvb_dsc_open,
	.release = _dvb_dsc_release,
	.poll = NULL,
#ifdef CONFIG_COMPAT
	.compat_ioctl = _dvb_dsc_compat_ioctl,
#endif
};

static struct dvb_device dvbdev_dsc = {
	.priv = NULL,
	.users = 1,
	.readers = 1,
	.writers = 1,
	.fops = &dvb_dsc_fops,
};

int dsc_init(struct aml_dsc *dsc, struct dvb_adapter *dvb_adapter)
{
	_init_dsc_table();

	/*Register descrambler device */
	return dvb_register_device(dvb_adapter, &dsc->dev,
				   &dvbdev_dsc, dsc, DVB_DEVICE_CA, 0);
}

void dsc_release(struct aml_dsc *dsc)
{
	if (dsc->dev) {
		dvb_unregister_device(dsc->dev);
		dsc->dev = NULL;
	}
}

int dsc_set_sid(int id, int sid)
{
	struct aml_dvb *advb = aml_get_dvb_device();
	struct aml_dsc *dsc;
	struct dsc_channel *chans;

	advb->dsc[id].sid = sid;

	dsc = &advb->dsc[id];
	if (dsc->dev) {
		chans = dsc->dsc_channels;
		while (chans) {
			_dsc_chan_set_sid(chans, sid);
			chans = chans->next;
		}
	}

	return 0;
}

static char *get_algo_str(int algo)
{
	char *str;

	switch (algo) {
	case CA_ALGO_AES_ECB_CLR_END:
	case CA_ALGO_AES_ECB_CLR_FRONT:
		str = "aes_ecb";
		break;
	case CA_ALGO_AES_CBC_CLR_END:
	case CA_ALGO_AES_CBC_IDSA:
		str = "aes_cbc";
		break;
	case CA_ALGO_CSA2:
		str = "csa2";
		break;
	case CA_ALGO_DES_SCTE41:
	case CA_ALGO_DES_SCTE52:
		str = "des";
		break;
	case CA_ALGO_TDES_ECB_CLR_END:
		str = "tdes";
		break;
	case CA_ALGO_CPCM_LSA_MDI_CBC:
	case CA_ALGO_CPCM_LSA_MDD_CBC:
		str = "cpcm";
		break;
	case CA_ALGO_CSA3:
		str = "csa3";
		break;
	case CA_ALGO_ASA:
	case CA_ALGO_ASA_LIGHT:
		str = "asa";
		break;
	case CA_ALGO_S17_ECB_CLR_END:
	case CA_ALGO_S17_ECB_CTS:
		str = "s17";
		break;
	default:
		str = "none";
		break;
	}
	return str;
}

int dsc_dump_info(char *buf)
{
	int r, total = 0;
	int i = 0;
	struct aml_dvb *dvb = aml_get_dvb_device();
	struct aml_dsc *dsc;
	struct dsc_channel *chans;
	struct dsc_pid_table *ptmp;
	int have_show_n = 0;
	int have_show_d = 0;
	int have_show_e = 0;
	unsigned int status;
	char kte = 0;
	int pid = 0;
	char sid = 0;
	char err = 0;

	r = sprintf(buf, "\n pay attention: dsc connected to %s\n\n",
			dvb->dsc_pipeline ? "demod" : "local");
	buf += r;
	total += r;

	for (i = 0; i < DSC_DEV_COUNT; i++) {
		dsc = &dvb->dsc[i];
		if (!dsc->dev)
			continue;

		if (mutex_lock_interruptible(&dsc->mutex))
			return -ERESTARTSYS;

		r = sprintf(buf, "dsc%d sid:0x%0x\n", dsc->id, dsc->sid);
		buf += r;
		total += r;

		/*add dsc status*/
		chans = dsc->dsc_channels;
		while (chans) {
			status = dsc_get_status(chans->dsc_type);
			kte = status & 0xff;
			pid = (status >> 8) & 0x1FFF;
			sid = (status >> 24) & 0x3F;
			err = (status >> 30) & 0x3;
			r = 0;
			if (chans->dsc_type == 0 && have_show_n == 0) {
				r = sprintf(buf, "tsn status: kte:%d, ", kte);
				buf += r;
				total += r;

			r = sprintf(buf, "pid:0x%0x, sid:0x%0x, err:%d\n",
					pid, sid, err);
				buf += r;
				total += r;

				have_show_n = 1;
			} else if (chans->dsc_type == 1 && have_show_d == 0) {
				r = sprintf(buf, "tsd status: kte:%d, ", kte);
				buf += r;
				total += r;

			r = sprintf(buf, "pid:0x%0x, sid:0x%0x, err:%d\n",
					pid, sid, err);
				buf += r;
				total += r;

				have_show_d = 1;
			} else if (chans->dsc_type == 2 && have_show_e == 0) {
				r = sprintf(buf, "tse status: kte:%d, ", kte);
				buf += r;
				total += r;

			r = sprintf(buf, "pid:0x%0x, sid:0x%0x, err:%d\n",
					pid, sid, err);
				buf += r;
				total += r;

				have_show_e = 1;
			}
			chans = chans->next;
		}

		chans = dsc->dsc_channels;
		while (chans) {
			r = sprintf(buf, " chan_id:%d module:ts%s ",
				    chans->id, (chans->dsc_type == 0 ? "n" :
						(chans->dsc_type ==
						 1 ? "d" : "e")));
			buf += r;
			total += r;

			r = sprintf(buf, "pid:0x%0x algo:%s ",
				    chans->pid, get_algo_str(chans->algo - 1));
			buf += r;
			total += r;

			r = sprintf(buf, "slot:%d, 00_slot:%d, loop:%d\n",
				    chans->index, chans->index00, chans->loop);
			buf += r;
			total += r;
			if (chans->index != -1) {
				ptmp =
				    _get_dsc_pid_table(chans->index,
						       chans->dsc_type);
				if (ptmp) {
					r = sprintf(buf,
					    "  slot:%d, even:%d, even iv:%d, odd:%d odd iv:%d\n",
					    chans->index, ptmp->kte_even_00,
					    ptmp->even_00_iv, ptmp->kte_odd,
					    ptmp->odd_iv);
					buf += r;
					total += r;
				}
			}
			if (chans->index00 != -1) {
				ptmp =
				    _get_dsc_pid_table(chans->index00,
						       chans->dsc_type);
				if (ptmp) {
					r = sprintf(buf, "  slot:%d, 00:%d, 00 iv:%d\n",
					    chans->index, ptmp->kte_even_00,
					    ptmp->even_00_iv);
					buf += r;
					total += r;
				}
			}
			chans = chans->next;
		}
		mutex_unlock(&dvb->dsc[i].mutex);
	}
	return total;
}
