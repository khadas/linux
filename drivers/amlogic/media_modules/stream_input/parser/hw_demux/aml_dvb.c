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
 * AMLOGIC DVB driver.
 */
//move to define in Makefile
//#define ENABLE_DEMUX_DRIVER

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
#include <linux/of_gpio.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/clk.h>
#include <linux/of_irq.h>
#include <linux/compat.h>

#include "c_stb_define.h"
#include "c_stb_regs_define.h"
#include "aml_dvb.h"
#include "aml_dvb_reg.h"

#include "aml_demod_gt.h"
#include "../../../common/media_clock/switch/amports_gate.h"

#define pr_dbg(args...)\
	do {\
		if (debug_dvb)\
			printk(args);\
	} while (0)
#define pr_error(fmt, args...) printk("DVB: " fmt, ## args)
#define pr_inf(fmt, args...)   printk(fmt, ## args)

MODULE_PARM_DESC(debug_dvb, "\n\t\t Enable dvb debug information");
static int debug_dvb;
module_param(debug_dvb, int, 0644);

#define CARD_NAME "amlogic-dvb-demux"

#define DVB_VERSION "V2.02"


//echo 0xff646180 0x40 > /sys/kernel/debug/aml_reg/paddr
//echo 0xff634590 0x1 > /sys/kernel/debug/aml_reg/paddr
#define TSINB_DEGLITCH0 0xff646180
#define TSINB_DEGLITCH1 0xff634590

DVB_DEFINE_MOD_OPT_ADAPTER_NR(adapter_nr);

MODULE_PARM_DESC(dsc_max, "max number of dsc");
static int dsc_max = DSC_DEV_COUNT;
module_param(dsc_max, int, 0644);

static struct aml_dvb aml_dvb_device;
static struct class aml_stb_class;

static int dmx_reset_all_flag = 0;
#if 0
static struct reset_control *aml_dvb_demux_reset_ctl;
static struct reset_control *aml_dvb_afifo_reset_ctl;
static struct reset_control *aml_dvb_ahbarb0_reset_ctl;
static struct reset_control *aml_dvb_uparsertop_reset_ctl;
#else
/*no used reset ctl,need use clk in 4.9 kernel*/
static struct clk *aml_dvb_demux_clk;
static struct clk *aml_dvb_afifo_clk;
static struct clk *aml_dvb_ahbarb0_clk;
static struct clk *aml_dvb_uparsertop_clk;
#endif

static int aml_tsdemux_reset(void);
static int aml_tsdemux_set_reset_flag(void);
static int aml_tsdemux_request_irq(irq_handler_t handler, void *data);
static int aml_tsdemux_free_irq(void);
static int aml_tsdemux_set_vid(int vpid);
static int aml_tsdemux_set_aid(int apid);
static int aml_tsdemux_set_sid(int spid);
static int aml_tsdemux_set_pcrid(int pcrpid);
static int aml_tsdemux_set_skipbyte(int skipbyte);
static int aml_tsdemux_set_demux(int id);
static unsigned long aml_tsdemux_hwdmx_spin_lock(unsigned long flags);
static int aml_tsdemux_hwdmx_spin_unlock(unsigned long flags);

static struct tsdemux_ops aml_tsdemux_ops = {
	.reset = aml_tsdemux_reset,
	.set_reset_flag = aml_tsdemux_set_reset_flag,
	.request_irq = aml_tsdemux_request_irq,
	.free_irq = aml_tsdemux_free_irq,
	.set_vid = aml_tsdemux_set_vid,
	.set_aid = aml_tsdemux_set_aid,
	.set_sid = aml_tsdemux_set_sid,
	.set_pcrid = aml_tsdemux_set_pcrid,
	.set_skipbyte = aml_tsdemux_set_skipbyte,
	.set_demux = aml_tsdemux_set_demux,
	.hw_dmx_lock = aml_tsdemux_hwdmx_spin_lock,
	.hw_dmx_unlock = aml_tsdemux_hwdmx_spin_unlock
};

long aml_stb_get_base(int id)
{
	int newbase = 0;
	if (MESON_CPU_MAJOR_ID_TXL < get_cpu_type()
		&& MESON_CPU_MAJOR_ID_GXLX != get_cpu_type()) {
		newbase = 1;
	}

	switch (id) {
	case ID_STB_CBUS_BASE:
		return (newbase) ? 0x1800 : 0x1600;
	case ID_SMARTCARD_REG_BASE:
		return (newbase) ? 0x9400 : 0x2110;
	case ID_ASYNC_FIFO_REG_BASE:
		return (newbase) ? 0x2800 : 0x2310;
	case ID_ASYNC_FIFO1_REG_BASE:
		return 0x9800;
	case ID_ASYNC_FIFO2_REG_BASE:
		return (newbase) ? 0x2400 : 0x2314;
	case ID_RESET_BASE:
		return (newbase) ? 0x0400 : 0x1100;
	case ID_PARSER_SUB_START_PTR_BASE:
		return (newbase) ? 0x3800 : 0x2900;
	default:
		return 0;
	}
	return 0;
}
static void aml_dvb_dmx_release(struct aml_dvb *advb, struct aml_dmx *dmx)
{
	int i;
	pr_inf("[dmx_kpi] %s Enter.\n", __func__);
	dvb_net_release(&dmx->dvb_net);
	aml_dmx_hw_deinit(dmx);
	dmx->demux.dmx.close(&dmx->demux.dmx);
	dmx->demux.dmx.remove_frontend(&dmx->demux.dmx, &dmx->mem_fe);

	for (i = 0; i < DMX_DEV_COUNT; i++)
		dmx->demux.dmx.remove_frontend(&dmx->demux.dmx, &dmx->hw_fe[i]);

	dvb_dmxdev_release(&dmx->dmxdev);
	dvb_dmx_release(&dmx->demux);
	pr_inf("[dmx_kpi] %s Exit.\n", __func__);
}

static int aml_dvb_dmx_init(struct aml_dvb *advb, struct aml_dmx *dmx, int id)
{
	int i, ret;
	struct device_node *node_dmx = NULL;
	char buf[32];

	switch (id) {
	case 0:
		dmx->dmx_irq = INT_DEMUX;
		break;
	case 1:
		dmx->dmx_irq = INT_DEMUX_1;
		break;
	case 2:
		dmx->dmx_irq = INT_DEMUX_2;
		break;
	}
	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "dmx");
	node_dmx = of_parse_phandle(advb->pdev->dev.of_node, buf, 0);
	if (node_dmx) {
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "demux%d_irq", id);
		ret = of_irq_get_byname(node_dmx, buf);
		if (ret > 0)
			dmx->dmx_irq = ret;
		printk("get irq num demux%d_irq:%d\n",id,dmx->dmx_irq);
	} else {
		printk("get default demux%d_irq:%d\n",id,dmx->dmx_irq);
	}

	dmx->source = -1;
	dmx->dump_ts_select = 0;
	dmx->dvr_irq = -1;

	dmx->demux.dmx.capabilities =
	    (DMX_TS_FILTERING | DMX_SECTION_FILTERING |
	     DMX_MEMORY_BASED_FILTERING);
	dmx->demux.filternum = dmx->demux.feednum = FILTER_COUNT;
	dmx->demux.priv = advb;
	dmx->demux.start_feed = aml_dmx_hw_start_feed;
	dmx->demux.stop_feed = aml_dmx_hw_stop_feed;
	dmx->demux.write_to_decoder = NULL;
	ret = dvb_dmx_init(&dmx->demux);
	if (ret < 0) {
		pr_error("dvb_dmx failed: error %d\n", ret);
		goto error_dmx_init;
	}

	dmx->dmxdev.filternum = dmx->demux.feednum;
	dmx->dmxdev.demux = &dmx->demux.dmx;
	dmx->dmxdev.capabilities = 0;
	ret = dvb_dmxdev_init(&dmx->dmxdev, &advb->dvb_adapter);
	if (ret < 0) {
		pr_error("dvb_dmxdev_init failed: error %d\n", ret);
		goto error_dmxdev_init;
	}

	for (i = 0; i < DMX_DEV_COUNT; i++) {
		int source = i + DMX_FRONTEND_0;

		dmx->hw_fe[i].source = source;
		ret =
		    dmx->demux.dmx.add_frontend(&dmx->demux.dmx,
						&dmx->hw_fe[i]);
		if (ret < 0) {
			pr_error("adding hw_frontend to dmx failed: error %d",
				 ret);
			dmx->hw_fe[i].source = 0;
			goto error_add_hw_fe;
		}
	}

	dmx->mem_fe.source = DMX_MEMORY_FE;
	ret = dmx->demux.dmx.add_frontend(&dmx->demux.dmx, &dmx->mem_fe);
	if (ret < 0) {
		pr_error("adding mem_frontend to dmx failed: error %d", ret);
		goto error_add_mem_fe;
	}
	ret = dmx->demux.dmx.connect_frontend(&dmx->demux.dmx, &dmx->hw_fe[1]);
	if (ret < 0) {
		pr_error("connect frontend failed: error %d", ret);
		goto error_connect_fe;
	}

	dmx->id = id;
	dmx->aud_chan = -1;
	dmx->vid_chan = -1;
	dmx->sub_chan = -1;
	dmx->pcr_chan = -1;

	/*smallsec*/
	dmx->smallsec.enable = 0;
	dmx->smallsec.bufsize = SS_BUFSIZE_DEF;
	dmx->smallsec.dmx = dmx;

	/*input timeout*/
	dmx->timeout.enable = 1;
	dmx->timeout.timeout = DTO_TIMEOUT_DEF;
	dmx->timeout.ch_disable = DTO_CHDIS_VAS;
	dmx->timeout.match = 1;
	dmx->timeout.trigger = 0;
	dmx->timeout.dmx = dmx;

	/*CRC monitor*/
	dmx->crc_check_count = 0;
	dmx->crc_check_time = 0;

	ret = aml_dmx_hw_init(dmx);
	if (ret < 0) {
		pr_error("demux hw init error %d", ret);
		dmx->id = -1;
		goto error_dmx_hw_init;
	}

	dvb_net_init(&advb->dvb_adapter, &dmx->dvb_net, &dmx->demux.dmx);

	return 0;
error_dmx_hw_init:
error_connect_fe:
	dmx->demux.dmx.remove_frontend(&dmx->demux.dmx, &dmx->mem_fe);
error_add_mem_fe:
error_add_hw_fe:
	for (i = 0; i < DMX_DEV_COUNT; i++) {
		if (dmx->hw_fe[i].source)
			dmx->demux.dmx.remove_frontend(&dmx->demux.dmx,
						       &dmx->hw_fe[i]);
	}
	dvb_dmxdev_release(&dmx->dmxdev);
error_dmxdev_init:
	dvb_dmx_release(&dmx->demux);
error_dmx_init:
	return ret;
}

struct aml_dvb *aml_get_dvb_device(void)
{
	return &aml_dvb_device;
}
EXPORT_SYMBOL(aml_get_dvb_device);

struct dvb_adapter *aml_get_dvb_adapter(void)
{
	return &aml_dvb_device.dvb_adapter;
}
EXPORT_SYMBOL(aml_get_dvb_adapter);

static int dvb_dsc_open(struct inode *inode, struct file *file)
{
	int err;

	err = dvb_generic_open(inode, file);
	if (err < 0)
		return err;

	return 0;
}

static void dsc_channel_alloc(struct aml_dsc *dsc, int id, unsigned int pid)
{
	struct aml_dsc_channel *ch = &dsc->channel[id];

	ch->used  = 1;
	ch->work_mode = -1;
	ch->id    = id;
	ch->pid   = pid;
	ch->set = 0;
	ch->dsc   = dsc;
	ch->mode = -1;

	dsc_set_pid(ch, ch->pid);
}

static void dsc_channel_free(struct aml_dsc_channel *ch)
{
	if (!ch->used)
		return;

	ch->used = 0;
	dsc_set_pid(ch, 0x1fff);
	dsc_release();

	ch->pid   = 0x1fff;
	ch->set = 0;
	ch->work_mode = -1;
	ch->mode  = -1;
}

static void dsc_reset(struct aml_dsc *dsc)
{
	int i;

	for (i = 0; i < DSC_COUNT; i++)
		dsc_channel_free(&dsc->channel[i]);
}

static int get_dsc_key_work_mode(enum ca_cw_type cw_type)
{
	int work_mode = DVBCSA_MODE;

	switch (cw_type) {
	case CA_CW_DVB_CSA_EVEN:
	case CA_CW_DVB_CSA_ODD:
		work_mode = DVBCSA_MODE;
		break;
	case CA_CW_AES_EVEN:
	case CA_CW_AES_ODD:
	case CA_CW_AES_ODD_IV:
	case CA_CW_AES_EVEN_IV:
	case CA_CW_DES_EVEN:
	case CA_CW_DES_ODD:
	case CA_CW_SM4_EVEN:
	case CA_CW_SM4_ODD:
	case CA_CW_SM4_ODD_IV:
	case CA_CW_SM4_EVEN_IV:
		work_mode = CIPLUS_MODE;
	default:
		break;
	}
	return work_mode;
}

/* Check if there are channels run in previous mode(aes/dvbcsa)
 * in dsc0/ciplus
 */
static void dsc_ciplus_switch_check(struct aml_dsc_channel *ch,
			enum ca_cw_type cw_type)
{
	struct aml_dsc *dsc = ch->dsc;
	int work_mode = 0;
	struct aml_dsc_channel *pch = NULL;
	int i;

	work_mode = get_dsc_key_work_mode(cw_type);
	if (dsc->work_mode == work_mode)
		return;

	dsc->work_mode = work_mode;

	for (i = 0; i < DSC_COUNT; i++) {
		pch = &dsc->channel[i];
		if (pch->work_mode != work_mode && pch->work_mode != -1) {
			pr_error("Dsc work mode changed,");
			pr_error("but there are still some channels");
			pr_error("run in different mode\n");
			pr_error("mod_pre[%d] -> mod[%d] ch[%d]\n",
				pch->work_mode, work_mode, i);
		}
	}
}

static int dsc_set_cw(struct aml_dsc *dsc, struct ca_descr_ex *d)
{
	struct aml_dsc_channel *ch;

	if (d->index >= DSC_COUNT)
		return -EINVAL;

	ch = &dsc->channel[d->index];

	switch (d->type) {
	case CA_CW_DVB_CSA_EVEN:
	case CA_CW_AES_EVEN:
	case CA_CW_DES_EVEN:
	case CA_CW_SM4_EVEN:
		memcpy(ch->even, d->cw, DSC_KEY_SIZE_MAX);
		break;
	case CA_CW_DVB_CSA_ODD:
	case CA_CW_AES_ODD:
	case CA_CW_DES_ODD:
	case CA_CW_SM4_ODD:
		memcpy(ch->odd, d->cw, DSC_KEY_SIZE_MAX);
		break;
	case CA_CW_AES_EVEN_IV:
	case CA_CW_SM4_EVEN_IV:
		memcpy(ch->even_iv, d->cw, DSC_KEY_SIZE_MAX);
		break;
	case CA_CW_AES_ODD_IV:
	case CA_CW_SM4_ODD_IV:
		memcpy(ch->odd_iv, d->cw, DSC_KEY_SIZE_MAX);
		break;
	default:
		break;
	}

	ch->set |= (1 << d->type) | (d->flags << 24);

	if (d->mode == CA_DSC_IDSA) {
		ch->mode = IDSA_MODE;
	}

	/*do key set*/
	dsc_set_key(ch, d->flags, d->type, d->cw);
	dsc_ciplus_switch_check(ch, d->type);

	return 0;
}

static int dvb_dsc_do_ioctl(struct file *file, unsigned int cmd,
			  void *parg)
{
	struct dvb_device *dvbdev = file->private_data;
	struct aml_dsc *dsc = dvbdev->priv;
	struct aml_dvb *dvb = dsc->dvb;
	struct aml_dsc_channel *ch;
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&dvb->slock, flags);

	switch (cmd) {
	case CA_RESET:
		dsc_reset(dsc);
		break;
	case CA_GET_CAP: {
		struct ca_caps *cap = parg;

		cap->slot_num   = 1;
		cap->slot_type  = CA_DESCR;
		cap->descr_num  = DSC_COUNT;
		cap->descr_type = 0;
		break;
	}
	case CA_GET_SLOT_INFO: {
		struct ca_slot_info *slot = parg;

		slot->num   = 1;
		slot->type  = CA_DESCR;
		slot->flags = 0;
		break;
	}
	case CA_GET_DESCR_INFO: {
		struct ca_descr_info *descr = parg;

		descr->num  = DSC_COUNT;
		descr->type = 0;
		break;
	}
	case CA_SET_DESCR: {
		struct ca_descr    *d = parg;
		struct ca_descr_ex  dex;

		dex.index = d->index;
		dex.type  = d->parity ? CA_CW_DVB_CSA_ODD : CA_CW_DVB_CSA_EVEN;
		dex.mode = -1;
		dex.flags = 0;
		memcpy(dex.cw, d->cw, sizeof(d->cw));

		ret = dsc_set_cw(dsc, &dex);
		break;
	}
	case CA_SET_PID: {
		struct ca_pid *pi = parg;
		int i;

		if (pi->index == -1) {
			for (i = 0; i < DSC_COUNT; i++) {
				ch = &dsc->channel[i];

				if (ch->used && (ch->pid == pi->pid)) {
					dsc_channel_free(ch);
					break;
				}
			}
		} else if ((pi->index >= 0) && (pi->index < DSC_COUNT)) {
			ch = &dsc->channel[pi->index];

			if (pi->pid < 0x1fff) {
				if (!ch->used) {
					dsc_channel_alloc(dsc,
					pi->index, pi->pid);
				}
			} else {
				if (ch->used)
					dsc_channel_free(ch);
			}
		} else {
			ret = -EINVAL;
		}
		break;
	}
	case CA_SET_DESCR_EX: {
		struct ca_descr_ex *d = parg;

		ret = dsc_set_cw(dsc, d);
		break;
	}
	default:
		ret = -EINVAL;
		break;
	}

	spin_unlock_irqrestore(&dvb->slock, flags);

	return ret;
}

static int dvb_dsc_usercopy(struct file *file,
		     unsigned int cmd, unsigned long arg,
		     int (*func)(struct file *file,
		     unsigned int cmd, void *arg))
{
	char    sbuf[128];
	void    *mbuf = NULL;
	void    *parg = NULL;
	int     err  = -EINVAL;

	/*  Copy arguments into temp kernel buffer  */
	switch (_IOC_DIR(cmd)) {
	case _IOC_NONE:
		/*
		 * For this command, the pointer is actually an integer
		 * argument.
		 */
		parg = (void *) arg;
		break;
	case _IOC_READ: /* some v4l ioctls are marked wrong ... */
	case _IOC_WRITE:
	case (_IOC_WRITE | _IOC_READ):
		if (_IOC_SIZE(cmd) <= sizeof(sbuf)) {
			parg = sbuf;
		} else {
			/* too big to allocate from stack */
			mbuf = kmalloc(_IOC_SIZE(cmd), GFP_KERNEL);
			if (mbuf == NULL)
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

static long dvb_dsc_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	return dvb_dsc_usercopy(file, cmd, arg, dvb_dsc_do_ioctl);
}

static int dvb_dsc_release(struct inode *inode, struct file *file)
{
	struct dvb_device *dvbdev = file->private_data;
	struct aml_dsc *dsc = dvbdev->priv;
	struct aml_dvb *dvb = dsc->dvb;
	unsigned long flags;

	spin_lock_irqsave(&dvb->slock, flags);

	dsc_reset(dsc);

	spin_unlock_irqrestore(&dvb->slock, flags);

	dvb_generic_release(inode, file);

	return 0;
}

#ifdef CONFIG_COMPAT
static long dvb_dsc_compat_ioctl(struct file *filp,
			unsigned int cmd, unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = dvb_dsc_ioctl(filp, cmd, args);
	return ret;
}
#endif


static const struct file_operations dvb_dsc_fops = {
	.owner = THIS_MODULE,
	.read = NULL,
	.write = NULL,
	.unlocked_ioctl = dvb_dsc_ioctl,
	.open = dvb_dsc_open,
	.release = dvb_dsc_release,
	.poll = NULL,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= dvb_dsc_compat_ioctl,
#endif
};

static struct dvb_device dvbdev_dsc = {
	.priv = NULL,
	.users = 1,
	.readers = 1,
	.writers = 1,
	.fops = &dvb_dsc_fops,
};

static int aml_dvb_asyncfifo_init(struct aml_dvb *advb,
				  struct aml_asyncfifo *asyncfifo, int id)
{
	char buf[32];
	int ret = 0;
	struct device_node *node_dmx = NULL;

	if (id == 0)
		asyncfifo->asyncfifo_irq = INT_ASYNC_FIFO_FLUSH;
	else if(id == 2)
		asyncfifo->asyncfifo_irq = INT_ASYNC_FIFO3_FLUSH;
	else
		asyncfifo->asyncfifo_irq = INT_ASYNC_FIFO2_FLUSH;

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "dmx");
	node_dmx = of_parse_phandle(advb->pdev->dev.of_node, buf, 0);
	if (node_dmx) {
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "dvr%d_irq", id);
		ret = of_irq_get_byname(node_dmx, buf);
		if (ret > 0)
			asyncfifo->asyncfifo_irq = ret;
		printk("get async%d_irq:%d\n",id,asyncfifo->asyncfifo_irq);
	} else {
		printk("get default async%d_irq:%d\n",id,asyncfifo->asyncfifo_irq);
	}
	asyncfifo->dvb = advb;
	asyncfifo->id = id;
	asyncfifo->init = 0;
	asyncfifo->flush_size = 256 * 1024;
	asyncfifo->secure_enable = 0;
	asyncfifo->blk.addr = 0;
	asyncfifo->blk.len = 0;
	asyncfifo->stored_pages = 0;

	return aml_asyncfifo_hw_init(asyncfifo);
}
static void aml_dvb_asyncfifo_release(struct aml_dvb *advb,
				      struct aml_asyncfifo *asyncfifo)
{
	aml_asyncfifo_hw_deinit(asyncfifo);
}

static int aml_dvb_dsc_init(struct aml_dvb *advb,
				  struct aml_dsc *dsc, int id)
{
	int i;

	for (i = 0; i < DSC_COUNT; i++) {
		dsc->channel[i].id    = i;
		dsc->channel[i].used  = 0;
		dsc->channel[i].set = 0;
		dsc->channel[i].pid   = 0x1fff;
		dsc->channel[i].dsc   = dsc;
	}
	dsc->dvb = advb;
	dsc->id = id;
	dsc->source = -1;
	dsc->dst = -1;

	/*Register descrambler device */
	return dvb_register_device(&advb->dvb_adapter, &dsc->dev,
				  &dvbdev_dsc, dsc, DVB_DEVICE_CA, 0);
}
static void aml_dvb_dsc_release(struct aml_dvb *advb,
				      struct aml_dsc *dsc)
{
	if (dsc->dev)
		dvb_unregister_device(dsc->dev);
	dsc->dev = NULL;
}


/*Show the STB input source*/
static ssize_t source_show(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	ssize_t ret = 0;
	char *src;

	switch (dvb->stb_source) {
	case AM_TS_SRC_TS0:
	case AM_TS_SRC_S_TS0:
		src = "ts0";
		break;
	case AM_TS_SRC_TS1:
	case AM_TS_SRC_S_TS1:
		src = "ts1";
		break;
	case AM_TS_SRC_TS2:
	case AM_TS_SRC_S_TS2:
		src = "ts2";
		break;
	case AM_TS_SRC_TS3:
		src = "ts3";
		break;
	case AM_TS_SRC_HIU:
		src = "hiu";
		break;
	case AM_TS_SRC_HIU1:
		src = "hiu1";
		break;
	case AM_TS_SRC_DMX0:
		src = "dmx0";
		break;
	case AM_TS_SRC_DMX1:
		src = "dmx1";
		break;
	case AM_TS_SRC_DMX2:
		src = "dmx2";
		break;
	default:
		src = "disable";
		break;
	}

	ret = sprintf(buf, "%s\n", src);
	return ret;
}

static ssize_t clear_av_store(struct class *class,
				struct class_attribute *attr, const char *buf,
				size_t size)
{
	if (!strncmp("1", buf, 1)) {
		aml_tsdemux_set_vid(0x1fff);
		aml_tsdemux_set_aid(0x1fff);
		aml_tsdemux_set_sid(0x1fff);
		aml_tsdemux_set_pcrid(0x1fff);
	}

	return size;
}

static int stb_check_source(const char *buf)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	int ret = 0;
	char *src;

	switch (dvb->stb_source) {
	case AM_TS_SRC_TS0:
	case AM_TS_SRC_S_TS0:
		src = "ts0";
		break;
	case AM_TS_SRC_TS1:
	case AM_TS_SRC_S_TS1:
		src = "ts1";
		break;
	case AM_TS_SRC_TS2:
	case AM_TS_SRC_S_TS2:
		src = "ts2";
		break;
	case AM_TS_SRC_TS3:
		src = "ts3";
		break;
	case AM_TS_SRC_HIU:
		src = "hiu";
		break;
	case AM_TS_SRC_HIU1:
		src = "hiu1";
		break;
	case AM_TS_SRC_DMX0:
		src = "dmx0";
		break;
	case AM_TS_SRC_DMX1:
		src = "dmx1";
		break;
	case AM_TS_SRC_DMX2:
		src = "dmx2";
		break;
	default:
		src = "disable";
		break;
	}
	pr_error("stb_check_source set buf:%s, src:%s\n", buf, src);
	ret = strcmp(buf,src);
	return ret;
}

/*Set the STB input source*/
static ssize_t source_store(struct class *class,
				struct class_attribute *attr, const char *buf,
				size_t size)
{
	dmx_source_t src = -1;
	if (stb_check_source(buf) == 0) {
		pr_error("stb_store_source same source \n");
		return size;
	}
	if (!strncmp("ts0", buf, 3))
		src = DMX_SOURCE_FRONT0;
	else if (!strncmp("ts1", buf, 3))
		src = DMX_SOURCE_FRONT1;
	else if (!strncmp("ts2", buf, 3))
		src = DMX_SOURCE_FRONT2;
	else if (!strncmp("ts3", buf, 3))
		src = DMX_SOURCE_FRONT3;
	else if (!strncmp("hiu1", buf, 4))
		src = DMX_SOURCE_DVR1;
	else if (!strncmp("hiu", buf, 3))
		src = DMX_SOURCE_DVR0;
	else if (!strncmp("dmx0", buf, 4))
		src = DMX_SOURCE_FRONT0 + 100;
	else if (!strncmp("dmx1", buf, 4))
		src = DMX_SOURCE_FRONT1 + 100;
	else if (!strncmp("dmx2", buf, 4))
		src = DMX_SOURCE_FRONT2 + 100;
	if (src != -1)
		aml_stb_hw_set_source(&aml_dvb_device, src);
	return size;
}

static ssize_t demux_reset_all_flag_show(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	char *src;

	if (dmx_reset_all_flag)
		src = "1";
	else
		src = "0";
	ret = sprintf(buf, "%s\n", src);
	return ret;
}
static ssize_t demux_reset_all_flag_store(struct class *class,
				struct class_attribute *attr, const char *buf,
				size_t size)
{
	if (!strncmp("0", buf, 1))
	    dmx_reset_all_flag = 0;
	else if (!strncmp("1", buf, 1))
	    dmx_reset_all_flag = 1;

	return size;
}
#define CASE_PREFIX

/*Show the descrambler's input source*/
#define DSC_SOURCE_FUNC_DECL(i)  \
static ssize_t dsc##i##_source_show(struct class *class,  \
				struct class_attribute *attr, char *buf)\
{\
	struct aml_dvb *dvb = &aml_dvb_device;\
	struct aml_dsc *dsc = &dvb->dsc[i];\
	ssize_t ret = 0;\
	char *src, *dst;\
	switch (dsc->source) {\
	CASE_PREFIX case AM_TS_SRC_DMX0:\
		src = "dmx0";\
	break;\
	CASE_PREFIX case AM_TS_SRC_DMX1:\
		src = "dmx1";\
	break;\
	CASE_PREFIX case AM_TS_SRC_DMX2:\
		src = "dmx2";\
	break;\
	CASE_PREFIX default :\
		src = "bypass";\
	break;\
	} \
	switch (dsc->dst) {\
	CASE_PREFIX case AM_TS_SRC_DMX0:\
		dst = "dmx0";\
	break;\
	CASE_PREFIX case AM_TS_SRC_DMX1:\
		dst = "dmx1";\
	break;\
	CASE_PREFIX case AM_TS_SRC_DMX2:\
		dst = "dmx2";\
	break;\
	CASE_PREFIX default :\
		dst = "bypass";\
	break;\
	} \
	ret = sprintf(buf, "%s-%s\n", src, dst);\
	return ret;\
} \
static ssize_t dsc##i##_source_store(struct class *class,  \
		struct class_attribute *attr, const char *buf, size_t size)\
{\
	dmx_source_t src = -1, dst = -1;\
	\
	if (!strncmp("dmx0", buf, 4)) {\
		src = DMX_SOURCE_FRONT0 + 100;\
	} else if (!strncmp("dmx1", buf, 4)) {\
		src = DMX_SOURCE_FRONT1 + 100;\
	} else if (!strncmp("dmx2", buf, 4)) {\
		src = DMX_SOURCE_FRONT2 + 100;\
	} \
	if (buf[4] == '-') {\
		if (!strncmp("dmx0", buf+5, 4)) {\
			dst = DMX_SOURCE_FRONT0 + 100;\
		} else if (!strncmp("dmx1", buf+5, 4)) {\
			dst = DMX_SOURCE_FRONT1 + 100;\
		} else if (!strncmp("dmx2", buf+5, 4)) {\
			dst = DMX_SOURCE_FRONT2 + 100;\
		} \
	} \
	else \
		dst = src; \
	aml_dsc_hw_set_source(&aml_dvb_device.dsc[i], src, dst);\
	return size;\
}

/*Show free descramblers count*/
#define DSC_FREE_FUNC_DECL(i)  \
static ssize_t dsc##i##_free_dscs_show(struct class *class, \
				  struct class_attribute *attr, char *buf) \
{ \
	struct aml_dvb *dvb = &aml_dvb_device; \
	int fid, count; \
	ssize_t ret = 0; \
	unsigned long flags;\
\
	spin_lock_irqsave(&dvb->slock, flags); \
	count = 0; \
	for (fid = 0; fid < DSC_COUNT; fid++) { \
		if (!dvb->dsc[i].channel[fid].used) \
			count++; \
	} \
	spin_unlock_irqrestore(&dvb->slock, flags); \
\
	ret = sprintf(buf, "%d\n", count); \
	return ret; \
}

#if DSC_DEV_COUNT > 0
	DSC_SOURCE_FUNC_DECL(0)
	DSC_FREE_FUNC_DECL(0)
#endif
#if DSC_DEV_COUNT > 1
	DSC_SOURCE_FUNC_DECL(1)
	DSC_FREE_FUNC_DECL(1)
#endif

/*Show the TS output source*/
static ssize_t tso_source_show(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	ssize_t ret = 0;
	char *src;

	switch (dvb->tso_source) {
	case AM_TS_SRC_TS0:
	case AM_TS_SRC_S_TS0:
		src = "ts0";
		break;
	case AM_TS_SRC_TS1:
	case AM_TS_SRC_S_TS1:
		src = "ts1";
		break;
	case AM_TS_SRC_TS2:
	case AM_TS_SRC_S_TS2:
		src = "ts2";
		break;
	case AM_TS_SRC_TS3:
		src = "ts3";
		break;
	case AM_TS_SRC_HIU:
		src = "hiu";
		break;
	case AM_TS_SRC_DMX0:
		src = "dmx0";
		break;
	case AM_TS_SRC_DMX1:
		src = "dmx1";
		break;
	case AM_TS_SRC_DMX2:
		src = "dmx2";
		break;
	default:
		src = "default";
		break;
	}

	ret = sprintf(buf, "%s\n", src);
	return ret;
}

/*Set the TS output source*/
static ssize_t tso_source_store(struct class *class,
				struct class_attribute *attr, const char *buf,
				size_t size)
{
	dmx_source_t src = -1;

	if (!strncmp("ts0", buf, 3))
		src = DMX_SOURCE_FRONT0;
	else if (!strncmp("ts1", buf, 3))
		src = DMX_SOURCE_FRONT1;
	else if (!strncmp("ts2", buf, 3))
		src = DMX_SOURCE_FRONT2;
	else if (!strncmp("ts3", buf, 3))
		src = DMX_SOURCE_FRONT3;
	else if (!strncmp("hiu", buf, 3))
		src = DMX_SOURCE_DVR0;
	else if (!strncmp("dmx0", buf, 4))
		src = DMX_SOURCE_FRONT0 + 100;
	else if (!strncmp("dmx1", buf, 4))
		src = DMX_SOURCE_FRONT1 + 100;
	else if (!strncmp("dmx2", buf, 4))
		src = DMX_SOURCE_FRONT2 + 100;

	aml_tso_hw_set_source(&aml_dvb_device, src);

	return size;
}

/*Show PCR*/
#define DEMUX_PCR_FUNC_DECL(i)  \
static ssize_t demux##i##_pcr_show(struct class *class,  \
				struct class_attribute *attr, char *buf)\
{\
	int f = 0;\
	if (i == 0)\
		f = READ_MPEG_REG(PCR_DEMUX);\
	else if (i == 1)\
		f = READ_MPEG_REG(PCR_DEMUX_2);\
	else if (i == 2)\
		f = READ_MPEG_REG(PCR_DEMUX_3);\
	return sprintf(buf, "%08x\n", f);\
}

static int dmx_check_source(int i, const char *buf)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	struct aml_dmx *dmx = &dvb->dmx[i];
	ssize_t ret = 0;
	char *src;
	switch (dmx->source) {
	 case AM_TS_SRC_TS0:
	 case AM_TS_SRC_S_TS0:
		src = "ts0";
	break;
	 case AM_TS_SRC_TS1:
	 case AM_TS_SRC_S_TS1:
		src = "ts1";
	break;
	 case AM_TS_SRC_TS2:
	 case AM_TS_SRC_S_TS2:
		src = "ts2";
	break;
	 case AM_TS_SRC_TS3:
		src = "ts3";
	break;
	 case AM_TS_SRC_DMX0:
		src = "dmx0";
	break;
	 case AM_TS_SRC_DMX1:
		src = "dmx1";
	break;
	 case AM_TS_SRC_DMX2:
		src = "dmx2";
	break;
	 case AM_TS_SRC_HIU:
		src = "hiu";
	break;
	 case AM_TS_SRC_HIU1:
		src = "hiu1";
	break;
	 default :
		src = "";
	break;
	}
	pr_error("dmx_check_source:set[%s]src[%s]dmx[%d]dmx->source:%d\n", buf, src, i, dmx->source);
	ret = strcmp(buf, src);
	return ret;
}


/*Show the STB input source*/
#define DEMUX_SOURCE_FUNC_DECL(i)  \
static ssize_t demux##i##_source_show(struct class *class,  \
				struct class_attribute *attr, char *buf)\
{\
	struct aml_dvb *dvb = &aml_dvb_device;\
	struct aml_dmx *dmx = &dvb->dmx[i];\
	ssize_t ret = 0;\
	char *src;\
	switch (dmx->source) {\
	CASE_PREFIX case AM_TS_SRC_TS0:\
	CASE_PREFIX case AM_TS_SRC_S_TS0:\
		src = "ts0";\
	break;\
	CASE_PREFIX case AM_TS_SRC_TS1:\
	CASE_PREFIX case AM_TS_SRC_S_TS1:\
		src = "ts1";\
	break;\
	CASE_PREFIX case AM_TS_SRC_TS2:\
	CASE_PREFIX case AM_TS_SRC_S_TS2:\
		src = "ts2";\
	break;\
	CASE_PREFIX case AM_TS_SRC_TS3:\
		src = "ts3";\
	break;\
	CASE_PREFIX case AM_TS_SRC_DMX0:\
		src = "dmx0";\
	break;\
	CASE_PREFIX case AM_TS_SRC_DMX1:\
		src = "dmx1";\
	break;\
	CASE_PREFIX case AM_TS_SRC_DMX2:\
		src = "dmx2";\
	break;\
	CASE_PREFIX case AM_TS_SRC_HIU:\
		src = "hiu";\
	break;\
	CASE_PREFIX case AM_TS_SRC_HIU1:\
		src = "hiu1";\
	break;\
	CASE_PREFIX default :\
		src = "";\
	break;\
	} \
	ret = sprintf(buf, "%s\n", src);\
	return ret;\
} \
static ssize_t demux##i##_source_store(struct class *class,  \
		struct class_attribute *attr, const char *buf, size_t size)\
{\
	dmx_source_t src = -1;\
	if (dmx_check_source(i, buf) == 0) {\
		pr_error("dmx[%d] source is same [%s]\n", i, buf);\
		return size;\
	}\
	if (!strncmp("ts0", buf, 3)) {\
		src = DMX_SOURCE_FRONT0;\
	} else if (!strncmp("ts1", buf, 3)) {\
		src = DMX_SOURCE_FRONT1;\
	} else if (!strncmp("ts2", buf, 3)) {\
		src = DMX_SOURCE_FRONT2;\
	} else if (!strncmp("ts3", buf, 3)) {\
		src = DMX_SOURCE_FRONT3;\
	} else if (!strncmp("hiu1", buf, 4)) {\
		src = DMX_SOURCE_DVR1;\
	} else if (!strncmp("hiu", buf, 3)) {\
		src = DMX_SOURCE_DVR0;\
	} else if (!strncmp("dmx0", buf, 4)) {\
		src = DMX_SOURCE_FRONT0_OFFSET;\
	} else if (!strncmp("dmx1", buf, 4)) {\
		src = DMX_SOURCE_FRONT1_OFFSET;\
	} else if (!strncmp("dmx2", buf, 4)) {\
		src = DMX_SOURCE_FRONT2_OFFSET;\
	} \
	if (src != -1) {\
		aml_dmx_hw_set_source(aml_dvb_device.dmx[i].dmxdev.demux, src);\
	} \
	return size;\
}

/*Show free filters count*/
#define DEMUX_FREE_FILTERS_FUNC_DECL(i)  \
static ssize_t demux##i##_free_filters_show(struct class *class,  \
				struct class_attribute *attr, char *buf)\
{\
	struct aml_dvb *dvb = &aml_dvb_device;\
	struct dvb_demux *dmx = &dvb->dmx[i].demux;\
	int fid, count;\
	ssize_t ret = 0;\
	if (mutex_lock_interruptible(&dmx->mutex)) \
		return -ERESTARTSYS; \
	count = 0;\
	for (fid = 0; fid < dmx->filternum; fid++) {\
		if (!dmx->filter[fid].state != DMX_STATE_FREE)\
			count++;\
	} \
	mutex_unlock(&dmx->mutex);\
	ret = sprintf(buf, "%d\n", count);\
	return ret;\
}

/*Show dmx dev open count*/
#define DEMUX_DEV_USERS_FUNC_DECL(i)  \
static ssize_t demux##i##_dev_users_show(struct class *class,  \
				struct class_attribute *attr, char *buf)\
{\
	struct aml_dvb *dvb = &aml_dvb_device;\
	struct dvb_demux *dmx = &dvb->dmx[i].demux;\
	int count;\
	ssize_t ret = 0;\
	if (mutex_lock_interruptible(&dmx->mutex)) \
		return -ERESTARTSYS; \
	count = dvb->dmx[i].dmxdev.dvbdev->users -1;\
	mutex_unlock(&dmx->mutex);\
	ret = sprintf(buf, "%d\n", count);\
	return ret;\
}


static ssize_t demux_state_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	int i = 0, r = 0, j = 0;
	struct dvb_demux *dmx;
	struct aml_dmx *dmx1;
	int fid, count;
	ssize_t ret = 0;
	char *str = NULL;

	for (i = 0; i < 3; i++) {
		r = sprintf(buf, "#####dmx%d#######\n", i);
		buf += r;
		ret += r;
		dmx = &dvb->dmx[i].demux;
		if (mutex_lock_interruptible(&dmx->mutex))
			return -ERESTARTSYS;

		r = sprintf(buf, "filter:\n");
		buf += r;
		ret += r;

		count = 0;
		for (fid = 0; fid < dmx->filternum; fid++) {
			if (!dmx->filter[fid].state != DMX_STATE_FREE)
				count++;
			else {
				r = sprintf(buf, "fid:%d, pid:0x%0x, state:%d\n", fid, dmx->filter[fid].feed->pid,
					dmx->filter[fid].state);
				buf += r;
				ret += r;
			}
		}
		r = sprintf(buf, "used filter:%d, free filter:%d\n", (dmx->filternum - count), count);
		buf += r;
		ret += r;

		r = sprintf(buf, "file users:%d\n", dvb->dmx[i].dmxdev.dvbdev->users);
		buf += r;
		ret += r;

		r = sprintf(buf, "chan:\n");
		buf += r;
		ret += r;

		dmx1 = &dvb->dmx[i];
		count = 0;
		for (j = 0; j < CHANNEL_COUNT; j++) {
			if (dmx1->channel[j].used) {
				if (dmx1->channel[j].type == DMX_TYPE_TS) {
					if (dmx1->channel[j].pes_type == DMX_PES_VIDEO) {
						str = "video";
					} else if (dmx1->channel[j].pes_type == DMX_PES_AUDIO){
						str = "audio";
					} else if (dmx1->channel[j].pes_type == DMX_PES_SUBTITLE) {
						str = "sub";
					} else if (dmx1->channel[j].pes_type == DMX_PES_TELETEXT) {
						str = "ttx";
					} else if (dmx1->channel[j].pes_type == DMX_PES_TELETEXT) {
						str = "other";
					}
				} else {
					str = "sec";
				}
				count ++;
				r = sprintf(buf, "id:%d, type:%s, pid:0x%0x\n", j, str, dmx1->channel[j].pid);
				buf += r;
				ret += r;
			}
		}
		r = sprintf(buf, "used chan:%d, free chan:%d\n", count, (CHANNEL_COUNT - count));
		buf += r;
		ret += r;
		mutex_unlock(&dmx->mutex);
	}
	return ret;
}

/*Show filter users count*/
#define DEMUX_FILTER_USERS_FUNC_DECL(i)  \
static ssize_t demux##i##_filter_users_show(struct class *class,  \
				struct class_attribute *attr, char *buf)\
{\
	struct aml_dvb *dvb = &aml_dvb_device;\
	struct aml_dmx *dmx = &dvb->dmx[i];\
	int dmxdevfid, count;\
	ssize_t ret = 0;\
	unsigned long flags;\
	spin_lock_irqsave(&dvb->slock, flags);\
	count = 0;\
	for (dmxdevfid = 0; dmxdevfid < dmx->dmxdev.filternum; dmxdevfid++) {\
		if (dmx->dmxdev.filter[dmxdevfid].state >= \
			DMXDEV_STATE_ALLOCATED)\
			count++;\
	} \
	if (count > dmx->demux_filter_user) {\
		count = dmx->demux_filter_user;\
	} else{\
		dmx->demux_filter_user = count;\
	} \
	spin_unlock_irqrestore(&dvb->slock, flags);\
	ret = sprintf(buf, "%d\n", count);\
	return ret;\
} \
static ssize_t demux##i##_filter_users_store(struct class *class,  \
		struct class_attribute *attr, const char *buf, size_t size)\
{\
	struct aml_dvb *dvb = &aml_dvb_device;\
	struct aml_dmx *dmx = &dvb->dmx[i];\
	unsigned long filter_used;\
	unsigned long flags;/*char *endp;*/\
	/*filter_used = simple_strtol(buf, &endp, 0);*/\
	int ret = kstrtol(buf, 0, &filter_used);\
	spin_lock_irqsave(&dvb->slock, flags);\
	if (ret == 0 && filter_used) {\
		if (dmx->demux_filter_user < FILTER_COUNT)\
			dmx->demux_filter_user++;\
	} else {\
		if (dmx->demux_filter_user > 0)\
			dmx->demux_filter_user--;\
	} \
	spin_unlock_irqrestore(&dvb->slock, flags);\
	return size;\
}

/*Show ts header*/
#define DEMUX_TS_HEADER_FUNC_DECL(i)  \
static ssize_t demux##i##_ts_header_show(struct class *class,  \
				struct class_attribute *attr, char *buf)\
{\
	int hdr = 0;\
	if (i == 0)\
		hdr = READ_MPEG_REG(TS_HEAD_1);\
	else if (i == 1)\
		hdr = READ_MPEG_REG(TS_HEAD_1_2);\
	else if (i == 2)\
		hdr = READ_MPEG_REG(TS_HEAD_1_3);\
	return sprintf(buf, "%08x\n", hdr);\
}

/*Show channel activity*/
#define DEMUX_CHANNEL_ACTIVITY_FUNC_DECL(i)  \
static ssize_t demux##i##_channel_activity_show(struct class *class,  \
				struct class_attribute *attr, char *buf)\
{\
	int f = 0;\
	if (i == 0)\
		f = READ_MPEG_REG(DEMUX_CHANNEL_ACTIVITY);\
	else if (i == 1)\
		f = READ_MPEG_REG(DEMUX_CHANNEL_ACTIVITY_2);\
	else if (i == 2)\
		f = READ_MPEG_REG(DEMUX_CHANNEL_ACTIVITY_3);\
	return sprintf(buf, "%08x\n", f);\
}

#define DEMUX_RESET_FUNC_DECL(i)  \
static ssize_t demux##i##_reset_store(struct class *class,  \
				struct class_attribute *attr, \
				const char *buf, size_t size)\
{\
	if (!strncmp("1", buf, 1)) { \
		struct aml_dvb *dvb = &aml_dvb_device; \
		pr_inf("Reset demux["#i"], call dmx_reset_dmx_hw\n"); \
		dmx_reset_dmx_id_hw_ex(dvb, i, 0); \
	} \
	return size; \
}

/*DVR record mode*/
#define DVR_MODE_FUNC_DECL(i)  \
static ssize_t dvr##i##_mode_show(struct class *class,  \
				struct class_attribute *attr, char *buf)\
{\
	struct aml_dvb *dvb = &aml_dvb_device;\
	struct aml_dmx *dmx = &dvb->dmx[i];\
	ssize_t ret = 0;\
	char *mode;\
	if (dmx->dump_ts_select) {\
		mode = "ts";\
	} else {\
		mode = "pid";\
	} \
	ret = sprintf(buf, "%s\n", mode);\
	return ret;\
} \
static ssize_t dvr##i##_mode_store(struct class *class,  \
		struct class_attribute *attr, const char *buf, size_t size)\
{\
	struct aml_dvb *dvb = &aml_dvb_device;\
	struct aml_dmx *dmx = &dvb->dmx[i];\
	int dump_ts_select = -1;\
	\
	if (!strncmp("pid", buf, 3) && dmx->dump_ts_select) {\
		dump_ts_select = 0;\
	} else if (!strncmp("ts", buf, 2) && !dmx->dump_ts_select) {\
		dump_ts_select = 1;\
	} \
	if (dump_ts_select != -1) {\
		aml_dmx_hw_set_dump_ts_select(\
			aml_dvb_device.dmx[i].dmxdev.demux, dump_ts_select);\
	} \
	return size;\
}

#if DMX_DEV_COUNT > 0
	DEMUX_PCR_FUNC_DECL(0)
	DEMUX_SOURCE_FUNC_DECL(0)
	DEMUX_FREE_FILTERS_FUNC_DECL(0)
	DEMUX_FILTER_USERS_FUNC_DECL(0)
	DEMUX_DEV_USERS_FUNC_DECL(0)
	DVR_MODE_FUNC_DECL(0)
	DEMUX_TS_HEADER_FUNC_DECL(0)
	DEMUX_CHANNEL_ACTIVITY_FUNC_DECL(0)
	DEMUX_RESET_FUNC_DECL(0)
#endif
#if DMX_DEV_COUNT > 1
	DEMUX_PCR_FUNC_DECL(1)
	DEMUX_SOURCE_FUNC_DECL(1)
	DEMUX_FREE_FILTERS_FUNC_DECL(1)
	DEMUX_FILTER_USERS_FUNC_DECL(1)
	DEMUX_DEV_USERS_FUNC_DECL(1)
	DVR_MODE_FUNC_DECL(1)
	DEMUX_TS_HEADER_FUNC_DECL(1)
	DEMUX_CHANNEL_ACTIVITY_FUNC_DECL(1)
	DEMUX_RESET_FUNC_DECL(1)
#endif
#if DMX_DEV_COUNT > 2
	DEMUX_PCR_FUNC_DECL(2)
	DEMUX_SOURCE_FUNC_DECL(2)
	DEMUX_FREE_FILTERS_FUNC_DECL(2)
	DEMUX_FILTER_USERS_FUNC_DECL(2)
	DEMUX_DEV_USERS_FUNC_DECL(2)
	DVR_MODE_FUNC_DECL(2)
	DEMUX_TS_HEADER_FUNC_DECL(2)
	DEMUX_CHANNEL_ACTIVITY_FUNC_DECL(2)
	DEMUX_RESET_FUNC_DECL(2)
#endif

/*Show the async fifo source*/
#define ASYNCFIFO_SOURCE_FUNC_DECL(i)  \
static ssize_t asyncfifo##i##_source_show(struct class *class,  \
				struct class_attribute *attr, char *buf)\
{\
	struct aml_dvb *dvb = &aml_dvb_device;\
	struct aml_asyncfifo *afifo = &dvb->asyncfifo[i];\
	ssize_t ret = 0;\
	char *src;\
	if (dvb->async_fifo_total_count <= i)\
		return ret;\
	switch (afifo->source) {\
	CASE_PREFIX case AM_DMX_0:\
		src = "dmx0";\
		break;\
	CASE_PREFIX case AM_DMX_1:\
		src = "dmx1";\
		break;   \
	CASE_PREFIX case AM_DMX_2:\
		src = "dmx2";\
		break;\
	CASE_PREFIX default :\
		src = "";\
		break;\
	} \
	ret = sprintf(buf, "%s\n", src);\
	return ret;\
} \
static ssize_t asyncfifo##i##_source_store(struct class *class,  \
		struct class_attribute *attr, const char *buf, size_t size)\
{\
	enum aml_dmx_id_t src = -1;\
	\
	if (aml_dvb_device.async_fifo_total_count <= i)\
		return 0;\
	if (!strncmp("dmx0", buf, 4)) {\
		src = AM_DMX_0;\
	} else if (!strncmp("dmx1", buf, 4)) {\
		src = AM_DMX_1;\
	} else if (!strncmp("dmx2", buf, 4)) {\
		src = AM_DMX_2;\
	} \
	if (src != -1) {\
		aml_asyncfifo_hw_set_source(&aml_dvb_device.asyncfifo[i], src);\
	} \
	return size;\
}

#if ASYNCFIFO_COUNT > 0
ASYNCFIFO_SOURCE_FUNC_DECL(0)
#endif
#if ASYNCFIFO_COUNT > 1
	ASYNCFIFO_SOURCE_FUNC_DECL(1)
#endif

#if ASYNCFIFO_COUNT > 2
	ASYNCFIFO_SOURCE_FUNC_DECL(2)
#endif

/*Show the async fifo flush size*/
#define ASYNCFIFO_FLUSHSIZE_FUNC_DECL(i)  \
static ssize_t asyncfifo##i##_flush_size_show(struct class *class,  \
				struct class_attribute *attr, char *buf)\
{\
	struct aml_dvb *dvb = &aml_dvb_device;\
	struct aml_asyncfifo *afifo = &dvb->asyncfifo[i];\
	ssize_t ret = 0;\
	if (dvb->async_fifo_total_count <= i)\
		return ret;\
	ret = sprintf(buf, "%d\n", afifo->flush_size);\
	return ret;\
} \
static ssize_t asyncfifo##i##_flush_size_store(struct class *class,  \
					struct class_attribute *attr, \
					const char *buf, size_t size)\
{\
	struct aml_dvb *dvb = &aml_dvb_device;\
	struct aml_asyncfifo *afifo = &dvb->asyncfifo[i];\
	/*int fsize = simple_strtol(buf, NULL, 10);*/\
	int fsize = 0;\
	long value;\
	int ret =0;\
	if (dvb->async_fifo_total_count <= i)\
		return (size_t)0;\
	ret = kstrtol(buf, 0, &value);\
	if (ret == 0)\
		fsize = value;\
	if (fsize != afifo->flush_size) {\
		afifo->flush_size = fsize;\
	aml_asyncfifo_hw_reset(&aml_dvb_device.asyncfifo[i]);\
	} \
	return size;\
}

#if ASYNCFIFO_COUNT > 0
ASYNCFIFO_FLUSHSIZE_FUNC_DECL(0)
#endif

#if ASYNCFIFO_COUNT > 1
	ASYNCFIFO_FLUSHSIZE_FUNC_DECL(1)
#endif

#if ASYNCFIFO_COUNT > 2
	ASYNCFIFO_FLUSHSIZE_FUNC_DECL(2)
#endif

/*Show the async fifo secure buffer addr*/
#define ASYNCFIFO_SECUREADDR_FUNC_DECL(i)  \
static ssize_t asyncfifo##i##_secure_addr_show(struct class *class,  \
				struct class_attribute *attr, char *buf)\
{\
	struct aml_dvb *dvb = &aml_dvb_device;\
	struct aml_asyncfifo *afifo = &dvb->asyncfifo[i];\
	ssize_t ret = 0;\
	if (dvb->async_fifo_total_count <= i)\
		return ret;\
	ret = sprintf(buf, "0x%x\n", afifo->blk.addr);\
	return ret;\
} \
static ssize_t asyncfifo##i##_secure_addr_store(struct class *class,  \
					struct class_attribute *attr, \
const char *buf, size_t size)\
{\
	struct aml_dvb *dvb = &aml_dvb_device;\
	struct aml_asyncfifo *afifo = &dvb->asyncfifo[i];\
	unsigned long value;\
	int ret=0;\
	if (dvb->async_fifo_total_count <= i)\
		return (size_t)0;\
	ret = kstrtol(buf, 0, &value);\
	if (ret == 0 && value != afifo->blk.addr) {\
		afifo->blk.addr = value;\
		aml_asyncfifo_hw_reset(&aml_dvb_device.asyncfifo[i]);\
	} \
	return size;\
}

#if ASYNCFIFO_COUNT > 0
	ASYNCFIFO_SECUREADDR_FUNC_DECL(0)
#endif

#if ASYNCFIFO_COUNT > 1
	ASYNCFIFO_SECUREADDR_FUNC_DECL(1)
#endif

#if ASYNCFIFO_COUNT > 2
	ASYNCFIFO_SECUREADDR_FUNC_DECL(2)
#endif

/*Show the async fifo secure buffer size*/
#define ASYNCFIFO_SECUREADDR_SIZE_FUNC_DECL(i)  \
static ssize_t asyncfifo##i##_secure_addr_size_show(struct class *class,  \
				struct class_attribute *attr, char *buf)\
{\
	struct aml_dvb *dvb = &aml_dvb_device;\
	struct aml_asyncfifo *afifo = &dvb->asyncfifo[i];\
	ssize_t ret = 0;\
	if (dvb->async_fifo_total_count <= i)\
		return ret;\
	ret = sprintf(buf, "0x%x\n", afifo->blk.len);\
	return ret;\
} \
static ssize_t asyncfifo##i##_secure_addr_size_store(struct class *class,  \
					struct class_attribute *attr, \
const char *buf, size_t size)\
{\
	struct aml_dvb *dvb = &aml_dvb_device;\
	struct aml_asyncfifo *afifo = &dvb->asyncfifo[i];\
	unsigned long value;\
	int ret=0;\
	if (dvb->async_fifo_total_count <= i)\
		return (size_t)0;\
	ret = kstrtol(buf, 0, &value);\
	if (ret == 0 && value != afifo->blk.len) {\
		afifo->blk.len = value;\
		aml_asyncfifo_hw_reset(&aml_dvb_device.asyncfifo[i]);\
	} \
	return size;\
}

#if ASYNCFIFO_COUNT > 0
	ASYNCFIFO_SECUREADDR_SIZE_FUNC_DECL(0)
#endif

#if ASYNCFIFO_COUNT > 1
	ASYNCFIFO_SECUREADDR_SIZE_FUNC_DECL(1)
#endif

#if ASYNCFIFO_COUNT > 2
	ASYNCFIFO_SECUREADDR_SIZE_FUNC_DECL(2)
#endif


/*Show the async fifo secure enable*/
#define ASYNCFIFO_SECURENABLE_FUNC_DECL(i)  \
static ssize_t asyncfifo##i##_secure_enable_show(struct class *class,  \
				struct class_attribute *attr, char *buf)\
{\
	struct aml_dvb *dvb = &aml_dvb_device;\
	struct aml_asyncfifo *afifo = &dvb->asyncfifo[i];\
	ssize_t ret = 0;\
	if (dvb->async_fifo_total_count <= i)\
		return ret;\
	ret = sprintf(buf, "%d\n", afifo->secure_enable);\
	return ret;\
} \
static ssize_t asyncfifo##i##_secure_enable_store(struct class *class,  \
					struct class_attribute *attr, \
					const char *buf, size_t size)\
{\
	struct aml_dvb *dvb = &aml_dvb_device;\
	struct aml_asyncfifo *afifo = &dvb->asyncfifo[i];\
	int enable = 0;\
	long value;\
	int ret=0;\
	if (dvb->async_fifo_total_count <= i)\
		return (size_t)0;\
	ret = kstrtol(buf, 0, &value);\
	if (ret == 0)\
		enable = value;\
	if (enable != afifo->secure_enable) {\
		afifo->secure_enable = enable;\
	aml_asyncfifo_hw_reset(&aml_dvb_device.asyncfifo[i]);\
	} \
	return size;\
}

#if ASYNCFIFO_COUNT > 0
ASYNCFIFO_SECURENABLE_FUNC_DECL(0)
#endif

#if ASYNCFIFO_COUNT > 1
	ASYNCFIFO_SECURENABLE_FUNC_DECL(1)
#endif

#if ASYNCFIFO_COUNT > 2
	ASYNCFIFO_SECURENABLE_FUNC_DECL(2)
#endif

/*Reset the Demux*/
static ssize_t demux_reset_store(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t size)
{
	if (!strncmp("1", buf, 1)) {
		struct aml_dvb *dvb = &aml_dvb_device;
		unsigned long flags;

		spin_lock_irqsave(&dvb->slock, flags);
		pr_dbg("Reset demux, call dmx_reset_hw\n");
		dmx_reset_hw_ex(dvb, 0);
		spin_unlock_irqrestore(&dvb->slock, flags);
	}

	return size;
}

/*Show the Video PTS value*/
static ssize_t video_pts_show(struct class *class,
				    struct class_attribute *attr, char *buf)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	ssize_t ret = 0;

	ret = sprintf(buf, "%u\n", aml_dmx_get_video_pts(dvb));

	return ret;
}

/*Show the Audio PTS value*/
static ssize_t audio_pts_show(struct class *class,
				    struct class_attribute *attr, char *buf)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	ssize_t ret = 0;

	ret = sprintf(buf, "%u\n", aml_dmx_get_audio_pts(dvb));

	return ret;
}

/*Show the Video PTS bit32 value*/
static ssize_t video_pts_bit32_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	ssize_t ret = 0;

	ret = sprintf(buf, "%u\n", aml_dmx_get_video_pts_bit32(dvb));

	return ret;
}

/*Show the Audio PTS bit32 value*/
static ssize_t audio_pts_bit32_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	ssize_t ret = 0;

	ret = sprintf(buf, "%u\n", aml_dmx_get_audio_pts_bit32(dvb));

	return ret;
}

/*Show the 33bit Video PTS value*/
static ssize_t video_pts_u64_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	ssize_t ret = 0;

	u64 pts_val = aml_dmx_get_video_pts(dvb);
	pts_val &= 0x00000000FFFFFFFF;

	if (aml_dmx_get_video_pts_bit32(dvb)) {
		pts_val = pts_val | (1LL<<32);
	}

	ret = sprintf(buf, "%llu\n", pts_val);

	return ret;
}

/*Show the 33bit Audio PTS value*/
static ssize_t audio_pts_u64_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	ssize_t ret = 0;

	u64 pts_val = aml_dmx_get_audio_pts(dvb);
	pts_val &= 0x00000000FFFFFFFF;

	if (aml_dmx_get_audio_pts_bit32(dvb)) {
		pts_val = pts_val | (1LL<<32);
	}

	ret = sprintf(buf, "%llu\n", pts_val);

	return ret;
}

/*Show the First Video PTS value*/
static ssize_t first_video_pts_show(struct class *class,
					  struct class_attribute *attr,
					  char *buf)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	ssize_t ret = 0;

	ret = sprintf(buf, "%u\n", aml_dmx_get_first_video_pts(dvb));

	return ret;
}

/*Show the First Audio PTS value*/
static ssize_t first_audio_pts_show(struct class *class,
					  struct class_attribute *attr,
					  char *buf)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	ssize_t ret = 0;

	ret = sprintf(buf, "%u\n", aml_dmx_get_first_audio_pts(dvb));

	return ret;
}

static ssize_t hw_setting_show(struct class *class,
				   struct class_attribute *attr, char *buf)
{
	int r, total = 0;
	int i;
	struct aml_dvb *dvb = &aml_dvb_device;
	int invert, ctrl;

	for (i = 0; i < dvb->ts_in_total_count; i++) {
		struct aml_ts_input *ts = &dvb->ts[i];

		if (ts->s2p_id != -1)
			invert = dvb->s2p[ts->s2p_id].invert;
		else
			invert = 0;

		ctrl = ts->control;

		r = sprintf(buf, "ts%d %s control: 0x%x invert: 0x%x\n", i,
			    ts->mode == AM_TS_DISABLE ? "disable" :
				(ts->mode == AM_TS_SERIAL ? "serial" :
				"parallel"), ctrl, invert);
		buf += r;
		total += r;
	}

	return total;
}

static ssize_t hw_setting_store(struct class *class,
				    struct class_attribute *attr,
				    const char *buf, size_t count)
{
	int id, ctrl, invert, r, mode;
	char mname[32];
	char pname[32];
	unsigned long flags;
	struct aml_ts_input *ts;
	struct aml_dvb *dvb = &aml_dvb_device;

	r = sscanf(buf, "%d %s %x %x", &id, mname, &ctrl, &invert);
	if (r != 4)
		return -EINVAL;

	if (id < 0 || id >= dvb->ts_in_total_count)
		return -EINVAL;

	if ((mname[0] == 's') || (mname[0] == 'S')) {
		sprintf(pname, "s_ts%d", id);
		mode = AM_TS_SERIAL;
	} else if ((mname[0] == 'p') || (mname[0] == 'P')) {
		sprintf(pname, "p_ts%d", id);
		mode = AM_TS_PARALLEL;
	} else
		mode = AM_TS_DISABLE;

	spin_lock_irqsave(&dvb->slock, flags);

	ts = &dvb->ts[id];

	if ((mode == AM_TS_SERIAL) && (ts->mode != AM_TS_SERIAL)) {
		int i;
		int scnt = 0;

		for (i = 0; i < dvb->ts_in_total_count; i++) {
			if (dvb->ts[i].s2p_id != -1)
				scnt++;
		}

		if (scnt >= dvb->s2p_total_count)
			pr_error("no free s2p\n");
		else
			ts->s2p_id = scnt;
	}

	if ((mode != AM_TS_SERIAL) || (ts->s2p_id != -1)) {
		if (ts->pinctrl) {
			devm_pinctrl_put(ts->pinctrl);
			ts->pinctrl = NULL;
		}

		ts->pinctrl = devm_pinctrl_get_select(&dvb->pdev->dev, pname);
/*              if(IS_ERR_VALUE(ts->pinctrl))*/
/*                      ts->pinctrl = NULL;*/
		ts->mode = mode;
		ts->control = ctrl;

		if (mode == AM_TS_SERIAL)
			dvb->s2p[ts->s2p_id].invert = invert;
		else
			ts->s2p_id = -1;
	}

	spin_unlock_irqrestore(&dvb->slock, flags);

	return count;
}


static CLASS_ATTR_RW(hw_setting);
static CLASS_ATTR_RW(source);
static CLASS_ATTR_RW(demux_reset_all_flag);
static CLASS_ATTR_RW(tso_source);
#define DEMUX_SOURCE_ATTR_PCR(i)\
	static CLASS_ATTR_RO(demux##i##_pcr)
#define DEMUX_SOURCE_ATTR_DECL(i)\
	static CLASS_ATTR_RW(demux##i##_source)
#define DEMUX_FREE_FILTERS_ATTR_DECL(i)\
	static CLASS_ATTR_RO(demux##i##_free_filters)
#define DEMUX_FILTER_USERS_ATTR_DECL(i)\
	static CLASS_ATTR_RW(demux##i##_filter_users)
#define DEMUX_DEV_USERS_ATTR_DECL(i)\
	static CLASS_ATTR_RO(demux##i##_dev_users)
#define DVR_MODE_ATTR_DECL(i)\
	static CLASS_ATTR_RW(dvr##i##_mode)
#define DEMUX_TS_HEADER_ATTR_DECL(i)\
	static CLASS_ATTR_RO(demux##i##_ts_header)
#define DEMUX_CHANNEL_ACTIVITY_ATTR_DECL(i)\
	static CLASS_ATTR_RO(demux##i##_channel_activity)
#define DMX_RESET_ATTR_DECL(i)\
	static CLASS_ATTR_WO(demux##i##_reset)

#if DMX_DEV_COUNT > 0
	DEMUX_SOURCE_ATTR_PCR(0);
	DEMUX_SOURCE_ATTR_DECL(0);
	DEMUX_FREE_FILTERS_ATTR_DECL(0);
	DEMUX_FILTER_USERS_ATTR_DECL(0);
	DEMUX_DEV_USERS_ATTR_DECL(0);
	DVR_MODE_ATTR_DECL(0);
	DEMUX_TS_HEADER_ATTR_DECL(0);
	DEMUX_CHANNEL_ACTIVITY_ATTR_DECL(0);
	DMX_RESET_ATTR_DECL(0);
#endif
#if DMX_DEV_COUNT > 1
	DEMUX_SOURCE_ATTR_PCR(1);
	DEMUX_SOURCE_ATTR_DECL(1);
	DEMUX_FREE_FILTERS_ATTR_DECL(1);
	DEMUX_FILTER_USERS_ATTR_DECL(1);
	DEMUX_DEV_USERS_ATTR_DECL(1);
	DVR_MODE_ATTR_DECL(1);
	DEMUX_TS_HEADER_ATTR_DECL(1);
	DEMUX_CHANNEL_ACTIVITY_ATTR_DECL(1);
	DMX_RESET_ATTR_DECL(1);
#endif
#if DMX_DEV_COUNT > 2
	DEMUX_SOURCE_ATTR_PCR(2);
	DEMUX_SOURCE_ATTR_DECL(2);
	DEMUX_FREE_FILTERS_ATTR_DECL(2);
	DEMUX_FILTER_USERS_ATTR_DECL(2);
	DEMUX_DEV_USERS_ATTR_DECL(2);
	DVR_MODE_ATTR_DECL(2);
	DEMUX_TS_HEADER_ATTR_DECL(2);
	DEMUX_CHANNEL_ACTIVITY_ATTR_DECL(2);
	DMX_RESET_ATTR_DECL(2);
#endif

#define ASYNCFIFO_SOURCE_ATTR_DECL(i)\
	static CLASS_ATTR_RW(asyncfifo##i##_source)
#define ASYNCFIFO_FLUSHSIZE_ATTR_DECL(i)\
	static CLASS_ATTR_RW(asyncfifo##i##_flush_size)
#define ASYNCFIFO_SECUREADDR_ATTR_DECL(i)\
	static CLASS_ATTR_RW(asyncfifo##i##_secure_addr)
#define ASYNCFIFO_SECUREADDR_SIZE_ATTR_DECL(i)\
	static CLASS_ATTR_RW(asyncfifo##i##_secure_addr_size)
#define ASYNCFIFO_SECURENABLE_ATTR_DECL(i)\
	static CLASS_ATTR_RW(asyncfifo##i##_secure_enable)

#if ASYNCFIFO_COUNT > 0
	ASYNCFIFO_SOURCE_ATTR_DECL(0);
	ASYNCFIFO_FLUSHSIZE_ATTR_DECL(0);
	ASYNCFIFO_SECUREADDR_ATTR_DECL(0);
	ASYNCFIFO_SECUREADDR_SIZE_ATTR_DECL(0);
	ASYNCFIFO_SECURENABLE_ATTR_DECL(0);
#endif
#if ASYNCFIFO_COUNT > 1
	ASYNCFIFO_SOURCE_ATTR_DECL(1);
	ASYNCFIFO_FLUSHSIZE_ATTR_DECL(1);
	ASYNCFIFO_SECUREADDR_ATTR_DECL(1);
	ASYNCFIFO_SECUREADDR_SIZE_ATTR_DECL(1);
	ASYNCFIFO_SECURENABLE_ATTR_DECL(1);
#endif

#if ASYNCFIFO_COUNT > 2
	ASYNCFIFO_SOURCE_ATTR_DECL(2);
	ASYNCFIFO_FLUSHSIZE_ATTR_DECL(2);
	ASYNCFIFO_SECUREADDR_ATTR_DECL(2);
	ASYNCFIFO_SECUREADDR_SIZE_ATTR_DECL(2);
	ASYNCFIFO_SECURENABLE_ATTR_DECL(2);
#endif

	static CLASS_ATTR_WO(demux_reset);
	static CLASS_ATTR_RO(video_pts);
	static CLASS_ATTR_RO(audio_pts);
	static CLASS_ATTR_RO(video_pts_bit32);
	static CLASS_ATTR_RO(audio_pts_bit32);
	static CLASS_ATTR_RO(video_pts_u64);
	static CLASS_ATTR_RO(audio_pts_u64);
	static CLASS_ATTR_RO(first_video_pts);
	static CLASS_ATTR_RO(first_audio_pts);
	static CLASS_ATTR_WO(clear_av);
	static CLASS_ATTR_RO(demux_state);

#define DSC_SOURCE_ATTR_DECL(i)\
	static CLASS_ATTR_RW(dsc##i##_source)
#define DSC_FREE_ATTR_DECL(i) \
	static CLASS_ATTR_RO(dsc##i##_free_dscs)

#if DSC_DEV_COUNT > 0
	DSC_SOURCE_ATTR_DECL(0);
	DSC_FREE_ATTR_DECL(0);
#endif
#if DSC_DEV_COUNT > 1
	DSC_SOURCE_ATTR_DECL(1);
	DSC_FREE_ATTR_DECL(1);
#endif

#define STB_ATTR(name) &class_attr_##name.attr

static struct attribute *aml_stb_class_attrs[] = {
	STB_ATTR(hw_setting),
	STB_ATTR(source),
	STB_ATTR(demux_reset_all_flag),
	STB_ATTR(tso_source),
	STB_ATTR(demux_reset),
	STB_ATTR(video_pts),
	STB_ATTR(audio_pts),
	STB_ATTR(video_pts_bit32),
	STB_ATTR(audio_pts_bit32),
	STB_ATTR(video_pts_u64),
	STB_ATTR(audio_pts_u64),
	STB_ATTR(first_video_pts),
	STB_ATTR(first_audio_pts),
	STB_ATTR(clear_av),
	STB_ATTR(demux_state),
#define DEMUX_PCR(i) \
	STB_ATTR(demux##i##_pcr)
	DEMUX_PCR(0),
	DEMUX_PCR(1),
	DEMUX_PCR(2),
#define DEMUX_SOURCE(i) \
	STB_ATTR(demux##i##_source)
	DEMUX_SOURCE(0),
	DEMUX_SOURCE(1),
	DEMUX_SOURCE(2),
#define DEMUX_FREE_FILTER(i) \
	STB_ATTR(demux##i##_free_filters)
	DEMUX_FREE_FILTER(0),
	DEMUX_FREE_FILTER(1),
	DEMUX_FREE_FILTER(2),
#define DEMUX_FILTER_USERS(i) \
	STB_ATTR(demux##i##_filter_users)
	DEMUX_FILTER_USERS(0),
	DEMUX_FILTER_USERS(1),
	DEMUX_FILTER_USERS(2),
#define DEMUX_DEV_USERS(i) \
	STB_ATTR(demux##i##_dev_users)
	DEMUX_DEV_USERS(0),
	DEMUX_DEV_USERS(1),
	DEMUX_DEV_USERS(2),
#define DEMUX_TS_HEADER(i) \
	STB_ATTR(demux##i##_ts_header)
	DEMUX_TS_HEADER(0),
	DEMUX_TS_HEADER(1),
	DEMUX_TS_HEADER(2),
#define DEMUX_CHANNEL_ACTIVITY_ATTR(i) \
	STB_ATTR(demux##i##_channel_activity)
	DEMUX_CHANNEL_ACTIVITY_ATTR(0),
	DEMUX_CHANNEL_ACTIVITY_ATTR(1),
	DEMUX_CHANNEL_ACTIVITY_ATTR(2),
#define DEMUX_RESET(i)	\
	STB_ATTR(demux##i##_reset)
	DEMUX_RESET(0),
	DEMUX_RESET(1),
	DEMUX_RESET(2),
#define DVR_MODE(i)\
	STB_ATTR(dvr##i##_mode)
	DVR_MODE(0),
	DVR_MODE(1),
	DVR_MODE(2),
#define ASYNCFIFO_SOURCE(i) \
	STB_ATTR(asyncfifo##i##_source)
#if ASYNCFIFO_COUNT > 0
	ASYNCFIFO_SOURCE(0),
#endif
#if ASYNCFIFO_COUNT > 1
	ASYNCFIFO_SOURCE(1),
#endif
#if ASYNCFIFO_COUNT > 2
	ASYNCFIFO_SOURCE(2),
#endif
#define ASYNCFIFO_FLUSH_SIZE(i) \
	STB_ATTR(asyncfifo##i##_flush_size)
#if ASYNCFIFO_COUNT > 0
	ASYNCFIFO_FLUSH_SIZE(0),
#endif
#if ASYNCFIFO_COUNT > 1
	ASYNCFIFO_FLUSH_SIZE(1),
#endif
#if ASYNCFIFO_COUNT > 2
	ASYNCFIFO_FLUSH_SIZE(2),
#endif

#define ASYNCFIFO_SECURE_ADDR(i) \
	STB_ATTR(asyncfifo##i##_secure_addr)
#if ASYNCFIFO_COUNT > 0
	ASYNCFIFO_SECURE_ADDR(0),
#endif
#if ASYNCFIFO_COUNT > 1
	ASYNCFIFO_SECURE_ADDR(1),
#endif
#if ASYNCFIFO_COUNT > 2
	ASYNCFIFO_SECURE_ADDR(2),
#endif

#define ASYNCFIFO_SECURE_ADDR_SIZE(i) \
	STB_ATTR(asyncfifo##i##_secure_addr_size)
#if ASYNCFIFO_COUNT > 0
	ASYNCFIFO_SECURE_ADDR_SIZE(0),
#endif
#if ASYNCFIFO_COUNT > 1
	ASYNCFIFO_SECURE_ADDR_SIZE(1),
#endif
#if ASYNCFIFO_COUNT > 2
	ASYNCFIFO_SECURE_ADDR_SIZE(2),
#endif

#define ASYNCFIFO_SECURE_ENABLE(i)\
	STB_ATTR(asyncfifo##i##_secure_enable)
#if ASYNCFIFO_COUNT > 0
	ASYNCFIFO_SECURE_ENABLE(0),
#endif
#if ASYNCFIFO_COUNT > 1
	ASYNCFIFO_SECURE_ENABLE(1),
#endif
#if ASYNCFIFO_COUNT > 2
	ASYNCFIFO_SECURE_ENABLE(2),
#endif
#define DSC_SOURCE(i) \
	STB_ATTR(dsc##i##_source)
#if DSC_DEV_COUNT > 0
	DSC_SOURCE(0),
#endif
#if DSC_DEV_COUNT > 1
	DSC_SOURCE(1),
#endif
#define DSC_FREE_DSCS(i) \
	STB_ATTR(dsc##i##_free_dscs)
#if DSC_DEV_COUNT > 0
	DSC_FREE_DSCS(0),
#endif
#if DSC_DEV_COUNT > 1
	DSC_FREE_DSCS(1),
#endif
	NULL
};


ATTRIBUTE_GROUPS(aml_stb_class);

static struct class aml_stb_class = {
	.name = "stb",
	.class_groups = aml_stb_class_groups,
};

/*
 *extern int aml_regist_dmx_class(void);
 *extern int aml_unregist_dmx_class(void);
 */
/*
 *void afifo_reset(int v)
 *{
 *	if (v)
 *		reset_control_assert(aml_dvb_afifo_reset_ctl);
 *	else
 *		reset_control_deassert(aml_dvb_afifo_reset_ctl);
 *}
 */

static int aml_dvb_probe(struct platform_device *pdev)
{
	struct aml_dvb *advb;
	int i, ret = 0;
	struct devio_aml_platform_data *pd_dvb;

	pr_inf("probe amlogic dvb driver [%s]\n", DVB_VERSION);

	if (get_cpu_type() < MESON_CPU_MAJOR_ID_G12A)
	{
		aml_dvb_demux_clk =
			devm_clk_get(&pdev->dev, "demux");
		if (IS_ERR_OR_NULL(aml_dvb_demux_clk)) {
			dev_err(&pdev->dev, "get demux clk fail\n");
			return -1;
		}
		clk_prepare_enable(aml_dvb_demux_clk);

		aml_dvb_afifo_clk =
			devm_clk_get(&pdev->dev, "asyncfifo");
		if (IS_ERR_OR_NULL(aml_dvb_afifo_clk)) {
			dev_err(&pdev->dev, "get asyncfifo clk fail\n");
			return -1;
		}
		clk_prepare_enable(aml_dvb_afifo_clk);

		aml_dvb_ahbarb0_clk =
			devm_clk_get(&pdev->dev, "ahbarb0");
		if (IS_ERR_OR_NULL(aml_dvb_ahbarb0_clk)) {
			dev_err(&pdev->dev, "get ahbarb0 clk fail\n");
			return -1;
		}
		clk_prepare_enable(aml_dvb_ahbarb0_clk);

		aml_dvb_uparsertop_clk =
			devm_clk_get(&pdev->dev, "uparsertop");
		if (IS_ERR_OR_NULL(aml_dvb_uparsertop_clk)) {
			dev_err(&pdev->dev, "get uparsertop clk fail\n");
			return -1;
		}
		clk_prepare_enable(aml_dvb_uparsertop_clk);
	}
	else
	{
		amports_switch_gate("demux", 1);
		amports_switch_gate("ahbarb0", 1);
		amports_switch_gate("parser_top", 1);
		if (get_cpu_type() == MESON_CPU_MAJOR_ID_TL1)
		{
			aml_dvb_afifo_clk =
				devm_clk_get(&pdev->dev, "asyncfifo");
			if (IS_ERR_OR_NULL(aml_dvb_afifo_clk))
				dev_err(&pdev->dev, "get asyncfifo clk fail\n");
			else
				clk_prepare_enable(aml_dvb_afifo_clk);
		}
	}

	advb = &aml_dvb_device;
	memset(advb, 0, sizeof(aml_dvb_device));

	spin_lock_init(&advb->slock);

	advb->dev = &pdev->dev;
	advb->pdev = pdev;
	advb->stb_source = -1;
	advb->tso_source = -1;

	if (get_cpu_type() < MESON_CPU_MAJOR_ID_TL1) {
		advb->ts_in_total_count = 3;
		advb->s2p_total_count = 2;
		advb->async_fifo_total_count = 2;
	} else {
		advb->ts_in_total_count = 4;
		advb->s2p_total_count = 3;
		advb->async_fifo_total_count = 3;
	}

	for (i = 0; i < DMX_DEV_COUNT; i++) {
		advb->dmx[i].dmx_irq = -1;
		advb->dmx[i].dvr_irq = -1;
	}

#ifdef CONFIG_OF
	if (pdev->dev.of_node) {
		int s2p_id = 0;
		char buf[32];
		const char *str;
		u32 value;

		for (i = 0; i < advb->ts_in_total_count; i++) {

			advb->ts[i].mode = AM_TS_DISABLE;
			advb->ts[i].s2p_id = -1;
			advb->ts[i].pinctrl = NULL;
			memset(buf, 0, 32);
			snprintf(buf, sizeof(buf), "ts%d", i);
			ret =
			    of_property_read_string(pdev->dev.of_node, buf,
						    &str);
			if (!ret) {
				if (!strcmp(str, "serial")) {
					pr_inf("%s: serial\n", buf);

					if (s2p_id >= advb->s2p_total_count)
						pr_error("no free s2p\n");
					else {
						snprintf(buf, sizeof(buf),
							 "s_ts%d", i);
						advb->ts[i].mode = AM_TS_SERIAL;
						advb->ts[i].pinctrl =
						    devm_pinctrl_get_select
						    (&pdev->dev, buf);
						advb->ts[i].s2p_id = s2p_id;

						s2p_id++;
					}
				} else if (!strcmp(str, "parallel")) {
					pr_inf("%s: parallel\n", buf);
					memset(buf, 0, 32);
					snprintf(buf, sizeof(buf), "p_ts%d", i);
					advb->ts[i].mode = AM_TS_PARALLEL;
					advb->ts[i].pinctrl =
					    devm_pinctrl_get_select(&pdev->dev,
								    buf);
				} else {
					advb->ts[i].mode = AM_TS_DISABLE;
					advb->ts[i].pinctrl = NULL;
				}

				/* if(IS_ERR_VALUE(advb->ts[i].pinctrl)) */
				/* advb->ts[i].pinctrl = NULL; */
			}
			memset(buf, 0, 32);
			snprintf(buf, sizeof(buf), "ts%d_control", i);
			ret =
			    of_property_read_u32(pdev->dev.of_node, buf,
						 &value);
			if (!ret) {
				pr_inf("%s: 0x%x\n", buf, value);
				advb->ts[i].control = value;
			} else {
				pr_inf("read error:%s: 0x%x\n", buf, value);
			}

			if (advb->ts[i].s2p_id != -1) {
				memset(buf, 0, 32);
				snprintf(buf, sizeof(buf), "ts%d_invert", i);
				ret =
				    of_property_read_u32(pdev->dev.of_node, buf,
							 &value);
				if (!ret) {
					pr_inf("%s: 0x%x\n", buf, value);
					advb->s2p[advb->ts[i].s2p_id].invert =
					    value;
				}
			}
		}
		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "ts_out_invert");
		ret =
			of_property_read_u32(pdev->dev.of_node, buf,
				&value);
		if (!ret) {
			pr_inf("%s: 0x%x\n", buf, value);
				advb->ts_out_invert = value;
		}
		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "tsin_deglitch");
		ret =
			of_property_read_u32(pdev->dev.of_node, buf,
				&value);
		if (!ret) {
			pr_inf("%s: 0x%x\n", buf, value);
			if (value) {
			   pr_inf("TSINB_DEGLITCH0 is set %s: 0x%x\n", buf, value);
			   dmx_phyreg_access(TSINB_DEGLITCH0, 0x40, NULL);
			   dmx_phyreg_access(TSINB_DEGLITCH1, 0x1, NULL);
			}
		}
	}
#endif

	pd_dvb = (struct devio_aml_platform_data *)advb->dev->platform_data;

	ret =
	    dvb_register_adapter(&advb->dvb_adapter, CARD_NAME, THIS_MODULE,
				 advb->dev, adapter_nr);
	if (ret < 0)
		return ret;

	for (i = 0; i < DMX_DEV_COUNT; i++)
		advb->dmx[i].id = -1;

	for (i = 0; i<DSC_DEV_COUNT; i++)
		advb->dsc[i].id = -1;

	for (i = 0; i < advb->async_fifo_total_count; i++)
		advb->asyncfifo[i].id = -1;

	advb->dvb_adapter.priv = advb;
	dev_set_drvdata(advb->dev, advb);

	for (i = 0; i < DSC_DEV_COUNT; i++) {
		ret = aml_dvb_dsc_init(advb, &advb->dsc[i], i);
		if (ret < 0)
			goto error;
	}

	for (i = 0; i < DMX_DEV_COUNT; i++) {
		ret = aml_dvb_dmx_init(advb, &advb->dmx[i], i);
		if (ret < 0)
			goto error;
	}

	/*Init the async fifos */
	for (i = 0; i < advb->async_fifo_total_count; i++) {
		ret = aml_dvb_asyncfifo_init(advb, &advb->asyncfifo[i], i);
		if (ret < 0)
			goto error;
	}

	aml_regist_dmx_class();

	if (class_register(&aml_stb_class) < 0) {
		pr_error("dvb register class error\n");
		goto error;
	}

	aml_register_parser_mconfig();
#ifdef ENABLE_DEMUX_DRIVER
	tsdemux_set_ops(&aml_tsdemux_ops);
#else
	tsdemux_set_ops(NULL);
#endif

#if (defined CONFIG_AMLOGIC_DVB_EXTERN ||\
		defined CONFIG_AMLOGIC_DVB_EXTERN_MODULE)
	ret = dvb_extern_register_frontend(&advb->dvb_adapter);
	if (ret) {
		pr_error("aml register dvb frontend failed\n");
		goto error;
	}
#endif

	return 0;

error:
	for (i = 0; i < advb->async_fifo_total_count; i++) {
		if (advb->asyncfifo[i].id != -1)
			aml_dvb_asyncfifo_release(advb, &advb->asyncfifo[i]);
	}

	for (i = 0; i < DMX_DEV_COUNT; i++) {
		if (advb->dmx[i].id != -1)
			aml_dvb_dmx_release(advb, &advb->dmx[i]);
	}

	for (i = 0; i < DSC_DEV_COUNT; i++) {
		if (advb->dsc[i].id != -1)
			aml_dvb_dsc_release(advb, &advb->dsc[i]);
	}

	dvb_unregister_adapter(&advb->dvb_adapter);

	return ret;
}

static int aml_dvb_remove(struct platform_device *pdev)
{
	struct aml_dvb *advb = (struct aml_dvb *)dev_get_drvdata(&pdev->dev);
	int i;

	pr_inf("[dmx_kpi] %s Enter.\n", __func__);

#if (defined CONFIG_AMLOGIC_DVB_EXTERN ||\
		defined CONFIG_AMLOGIC_DVB_EXTERN_MODULE)
	dvb_extern_unregister_frontend();
#endif

	tsdemux_set_ops(NULL);

	aml_unregist_dmx_class();
	class_unregister(&aml_stb_class);

	for (i = 0; i < advb->async_fifo_total_count; i++) {
		if (advb->asyncfifo[i].id != -1)
			aml_dvb_asyncfifo_release(advb, &advb->asyncfifo[i]);
	}

	for (i = 0; i < DMX_DEV_COUNT; i++) {
		pr_error("remove demx %d, id is %d\n",i,advb->dmx[i].id);
		if (advb->dmx[i].id != -1)
			aml_dvb_dmx_release(advb, &advb->dmx[i]);
	}

	for (i = 0; i < DSC_DEV_COUNT; i++) {
		if (advb->dsc[i].id != -1)
			aml_dvb_dsc_release(advb, &advb->dsc[i]);
	}
	dvb_unregister_adapter(&advb->dvb_adapter);

	for (i = 0; i < advb->ts_in_total_count; i++) {
		if (advb->ts[i].pinctrl && !IS_ERR_VALUE(advb->ts[i].pinctrl))
			devm_pinctrl_put(advb->ts[i].pinctrl);
	}

	/*switch_mod_gate_by_name("demux", 0); */
#if 0
	reset_control_assert(aml_dvb_uparsertop_reset_ctl);
	reset_control_assert(aml_dvb_ahbarb0_reset_ctl);
	reset_control_assert(aml_dvb_afifo_reset_ctl);
	reset_control_assert(aml_dvb_demux_reset_ctl);
#else
#if 1
	if (get_cpu_type() < MESON_CPU_MAJOR_ID_G12A)
	{
		clk_disable_unprepare(aml_dvb_uparsertop_clk);
		clk_disable_unprepare(aml_dvb_ahbarb0_clk);
		clk_disable_unprepare(aml_dvb_afifo_clk);
		clk_disable_unprepare(aml_dvb_demux_clk);
	}
	else
	{
		amports_switch_gate("demux", 0);
		amports_switch_gate("ahbarb0", 0);
		amports_switch_gate("parser_top", 0);

		if (!IS_ERR_OR_NULL(aml_dvb_afifo_clk)) {
			clk_disable_unprepare(aml_dvb_afifo_clk);
		}
	}
#endif
#endif

	pr_inf("[dmx_kpi] %s Exit.\n", __func__);
	return 0;
}

static int aml_dvb_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int aml_dvb_resume(struct platform_device *dev)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	int i;

	for (i = 0; i < DMX_DEV_COUNT; i++)
		dmx_reset_dmx_id_hw_ex(dvb, i, 0);

	pr_inf("dvb resume\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id aml_dvb_dt_match[] = {
	{
	 .compatible = "amlogic, dvb-demux",
	 },
	{},
};
#endif /*CONFIG_OF */

static struct platform_driver aml_dvb_driver = {
	.probe = aml_dvb_probe,
	.remove = aml_dvb_remove,
	.suspend = aml_dvb_suspend,
	.resume = aml_dvb_resume,
	.driver = {
		   .name = "amlogic-dvb-demux",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
	   .of_match_table = aml_dvb_dt_match,
#endif
		}
};

static int __init aml_dvb_init(void)
{
	return platform_driver_register(&aml_dvb_driver);
}

static void __exit aml_dvb_exit(void)
{
	pr_inf("aml dvb exit\n");
	platform_driver_unregister(&aml_dvb_driver);
}

/*Get the STB source demux*/
static struct aml_dmx *get_stb_dmx(void)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	struct aml_dmx *dmx = NULL;
	int i;

	switch (dvb->stb_source) {
	case AM_TS_SRC_DMX0:
		dmx = &dvb->dmx[0];
		break;
	case AM_TS_SRC_DMX1:
		dmx = &dvb->dmx[1];
		break;
	case AM_TS_SRC_DMX2:
		dmx = &dvb->dmx[2];
		break;
	default:
		for (i = 0; i < DMX_DEV_COUNT; i++) {
			dmx = &dvb->dmx[i];
			if (dmx->source == dvb->stb_source)
				return dmx;
		}
		break;
	}

	return dmx;
}

static int aml_tsdemux_reset(void)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	unsigned long flags;
	pr_inf("[dmx_kpi] %s Enter\n", __func__);

	spin_lock_irqsave(&dvb->slock, flags);
	if (dvb->reset_flag) {
		struct aml_dmx *dmx = get_stb_dmx();

		dvb->reset_flag = 0;
		if (dmx) {
			if (dmx_reset_all_flag)
				dmx_reset_hw_ex(dvb, 0);
			else
				dmx_reset_dmx_hw_ex_unlock(dvb, dmx, 0);
		}
	}
	spin_unlock_irqrestore(&dvb->slock, flags);
	pr_inf("[dmx_kpi] %s Exit\n", __func__);
	return 0;
}

static int aml_tsdemux_set_reset_flag(void)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	unsigned long flags;

	spin_lock_irqsave(&dvb->slock, flags);
	dvb->reset_flag = 1;
	spin_unlock_irqrestore(&dvb->slock, flags);

	return 0;

}

/*Add the amstream irq handler*/
static int aml_tsdemux_request_irq(irq_handler_t handler, void *data)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	struct aml_dmx *dmx;
	unsigned long flags;

	spin_lock_irqsave(&dvb->slock, flags);

	dmx = get_stb_dmx();
	if (dmx) {
		dmx->irq_handler = handler;
		dmx->irq_data = data;
	}

	spin_unlock_irqrestore(&dvb->slock, flags);

	return 0;
}

/*Free the amstream irq handler*/
static int aml_tsdemux_free_irq(void)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	struct aml_dmx *dmx;
	unsigned long flags;

	spin_lock_irqsave(&dvb->slock, flags);

	dmx = get_stb_dmx();
	if (dmx) {
		dmx->irq_handler = NULL;
		dmx->irq_data = NULL;
	}

	spin_unlock_irqrestore(&dvb->slock, flags);

	return 0;
}

/*Reset the video PID*/
static int aml_tsdemux_set_vid(int vpid)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	struct aml_dmx *dmx;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&dvb->slock, flags);
	dmx = get_stb_dmx();
	if (dmx) {
		if (dmx->vid_chan != -1) {
			dmx_free_chan(dmx, dmx->vid_chan);
			dmx->vid_chan = -1;
		}

		if ((vpid >= 0) && (vpid < 0x1FFF)) {
			dmx->vid_chan =
			    dmx_alloc_chan(dmx, DMX_TYPE_TS,
						DMX_PES_VIDEO, vpid);
			if (dmx->vid_chan == -1)
				ret = -1;
		}
	}

	spin_unlock_irqrestore(&dvb->slock, flags);

	return ret;
}

/*Reset the audio PID*/
static int aml_tsdemux_set_aid(int apid)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	struct aml_dmx *dmx;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&dvb->slock, flags);
	dmx = get_stb_dmx();
	if (dmx) {
		if (dmx->aud_chan != -1) {
			dmx_free_chan(dmx, dmx->aud_chan);
			dmx->aud_chan = -1;
		}

		if ((apid >= 0) && (apid < 0x1FFF)) {
			dmx->aud_chan =
			    dmx_alloc_chan(dmx, DMX_TYPE_TS,
						DMX_PES_AUDIO, apid);
			if (dmx->aud_chan == -1)
				ret = -1;
		}
	}

	spin_unlock_irqrestore(&dvb->slock, flags);

	return ret;
}

/*Reset the subtitle PID*/
static int aml_tsdemux_set_sid(int spid)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	struct aml_dmx *dmx;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&dvb->slock, flags);

	dmx = get_stb_dmx();
	if (dmx) {
		if (dmx->sub_chan != -1) {
			dmx_free_chan(dmx, dmx->sub_chan);
			dmx->sub_chan = -1;
		}

		if ((spid >= 0) && (spid < 0x1FFF)) {
			dmx->sub_chan = 3;
			dmx->sub_chan =
			    dmx_alloc_chan(dmx, DMX_TYPE_TS,
						DMX_PES_SUBTITLE, spid);
			if (dmx->sub_chan == -1)
				ret = -1;
		}
	}

	spin_unlock_irqrestore(&dvb->slock, flags);

	return ret;
}

static int aml_tsdemux_set_pcrid(int pcrpid)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	struct aml_dmx *dmx;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&dvb->slock, flags);

	dmx = get_stb_dmx();
	if (dmx) {
		if (dmx->pcr_chan != -1) {
			dmx_free_chan(dmx, dmx->pcr_chan);
			dmx->pcr_chan = -1;
		}

		if ((pcrpid >= 0) && (pcrpid < 0x1FFF)) {
			dmx->pcr_chan =
			    dmx_alloc_chan(dmx, DMX_TYPE_TS,
						DMX_PES_PCR, pcrpid);
			if (dmx->pcr_chan == -1)
				ret = -1;
		}
	}

	spin_unlock_irqrestore(&dvb->slock, flags);

	return ret;
}

static int aml_tsdemux_set_skipbyte(int skipbyte)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	unsigned long flags;

	spin_lock_irqsave(&dvb->slock, flags);
	aml_dmx_set_skipbyte(dvb, skipbyte);
	spin_unlock_irqrestore(&dvb->slock, flags);

	return 0;
}

static int aml_tsdemux_set_demux(int id)
{
	struct aml_dvb *dvb = &aml_dvb_device;

	aml_dmx_set_demux(dvb, id);
	return 0;
}

static unsigned long aml_tsdemux_hwdmx_spin_lock(unsigned long flags)
{
	struct aml_dvb *dvb = &aml_dvb_device;

	spin_lock_irqsave(&dvb->slock, flags);
	return flags;
}

static int aml_tsdemux_hwdmx_spin_unlock(unsigned long flags)
{
	struct aml_dvb *dvb = &aml_dvb_device;

	spin_unlock_irqrestore(&dvb->slock, flags);
	return 0;
}

module_init(aml_dvb_init);
module_exit(aml_dvb_exit);

MODULE_DESCRIPTION("driver for the AMLogic DVB card");
MODULE_AUTHOR("AMLOGIC");
MODULE_LICENSE("GPL");
