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
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/dvb/dmx.h>
#include <linux/amlogic/tee.h>
#include <linux/amlogic/tee_demux.h>
#include <linux/amlogic/cpu_version.h>
#include <media/dvb_frontend.h>

#include "aml_dvb.h"
#include "dmx_log.h"
#include "sc2_demux/ts_output.h"
#include "sc2_demux/frontend.h"
#include "sc2_demux/dvb_reg.h"

#define dprint_i(fmt, args...)  \
	dprintk(LOG_ERROR, debug_dvb, fmt, ## args)
#define dprint(fmt, args...)   \
	dprintk(LOG_ERROR, debug_dvb, fmt, ## args)
#define pr_dbg(fmt, args...)   \
	dprintk(LOG_DBG, debug_dvb, "dvb:" fmt, ## args)

MODULE_PARM_DESC(debug_dvb, "\n\t\t Enable demux debug information");
static int debug_dvb = 1;
module_param(debug_dvb, int, 0644);

#define CARD_NAME "amlogic-dvb"
#define DVB_VERSION "V2.02"

DVB_DEFINE_MOD_OPT_ADAPTER_NR(adapter_nr);

static struct aml_dvb aml_dvb_device;
static int dmx_dev_num;
static int tsn_in;
static int tsn_out;
static int print_stc;
static int print_dmx;
static int tso_src = -1;
static int cpu_type;
static int minor_type;

#define MAX_DMX_DEV_NUM      32
#define DEFAULT_DMX_DEV_NUM  3

int is_security_dmx;

static void demux_config_pipeline(int cfg_demod_tsn, int cfg_tsn_out)
{
	u32 value = 0;
	int ret = 0;

	value = cfg_demod_tsn;
	value += cfg_tsn_out << 1;

	if (cpu_type == MESON_CPU_MAJOR_ID_T5W)
		ret = tee_write_reg_bits(0xff610320, value, 0, 2);
	else
		ret = tee_write_reg_bits(0xfe440320, value, 0, 2);

	if (ret != 0)
		dprint("tee_write_reg_bits value:%d, ret:%d\n", value, ret);
}

ssize_t get_pcr_show(struct class *class,
		     struct class_attribute *attr, char *buf)
{
	struct aml_dvb *dvb = aml_get_dvb_device();
	int r, total = 0;
	int i;
	u64 value;
	unsigned int base;
	int ret = 0;

	for (i = 0; i < 4; i++) {
		value = 0;
		base = 0;
		if (print_stc) {
			ret = dmx_get_stc(&dvb->dmx[print_dmx].dmx, i,
				&value, &base);
			if (ret != 0)
				continue;
			r = sprintf(buf, "dmx:%d num%d stc:0x%llx base:%d\n",
				print_dmx, i, value, base);
		} else {
			ret = dmx_get_pcr(&dvb->dmx[print_dmx].dmx, i, &value);
			if (ret != 0)
				continue;
			r = sprintf(buf, "dmx:%d num%d pcr:0x%llx\n",
				print_dmx, i, value);
		}

		buf += r;
		total += r;
	}

	return total;
}

ssize_t get_pcr_store(struct class *class,
			 struct class_attribute *attr,
			 const char *buf, size_t count)
{
	if (!strncmp(buf, "dmx", 3))
		print_dmx = buf[4] - 0x30;

	if (!strncmp(&buf[6], "stc", 3))
		print_stc = buf[10] - 0x30;

	dprint("dmx=%d stc=%d\n", print_dmx, print_stc);
	return count;
}

ssize_t dmx_setting_show(struct class *class, struct class_attribute *attr,
			 char *buf)
{
	int r, total = 0;
	int i;
	struct aml_dvb *dvb = aml_get_dvb_device();

	for (i = 0; i < dmx_dev_num; i++) {
		r = sprintf(buf, "dmx%d input:%s ", i,
			    dvb->dmx[i].source == INPUT_DEMOD ? "input_demod" :
			    (dvb->dmx[i].source == INPUT_LOCAL ?
			     "input_local" : "input_local_sec"));
		buf += r;
		total += r;
		if (dvb->dmx[i].hw_source >= 0 &&
			dvb->dmx[i].hw_source < FRONTEND_TS0) {
			r = sprintf(buf, "DMA_%d ", dvb->dmx[i].hw_source);
		} else if (dvb->dmx[i].hw_source >= FRONTEND_TS0 &&
			dvb->dmx[i].hw_source < DMA_0_1) {
			r = sprintf(buf, "FRONTEND_TS%d ",
					dvb->dmx[i].hw_source - FRONTEND_TS0);
		} else if (dvb->dmx[i].hw_source >= DMA_0_1 &&
			dvb->dmx[i].hw_source < FRONTEND_TS0_1) {
			r = sprintf(buf, "DMA_%d_1 ", dvb->dmx[i].hw_source - DMA_0_1);
		} else if (dvb->dmx[i].hw_source >= FRONTEND_TS0_1) {
			r = sprintf(buf, "FRONTEND_TS%d_1 ",
					dvb->dmx[i].hw_source - FRONTEND_TS0_1);
		}

		buf += r;
		total += r;

		r = sprintf(buf, "sid:0x%0x\n", dvb->dmx[i].sid);
		buf += r;
		total += r;
	}

	return total;
}

ssize_t dsc_setting_show(struct class *class, struct class_attribute *attr,
			 char *buf)
{
	int total = 0;

	total = dsc_dump_info(buf);

	return total;
}

unsigned int get_dmx_version(void)
{
	unsigned int value;

	value = (unsigned int)aml_read_self(0x2c04);
	return value >> 16;
}

static ssize_t get_chip_version(char *buf)
{
	int total = 0;
	unsigned int version = get_dmx_version();

	if (cpu_type == MESON_CPU_MAJOR_ID_SC2)
		total = sprintf(buf, "sc2-%x\n", minor_type);
	else if (cpu_type == MESON_CPU_MAJOR_ID_S4)
		total = sprintf(buf, "s4-%x\n", minor_type);
	else if (cpu_type == MESON_CPU_MAJOR_ID_S4D)
		total = sprintf(buf, "s4d-%x\n", minor_type);
	else if (cpu_type == MESON_CPU_MAJOR_ID_T3)
		total = sprintf(buf, "t3-%x\n", minor_type);
	else if (cpu_type == MESON_CPU_MAJOR_ID_T7)
		total = sprintf(buf, "t7-%x\n", minor_type);
	else if (cpu_type == MESON_CPU_MAJOR_ID_T5W)
		total = sprintf(buf, "t5w-%x\n", minor_type);
	else
		total = sprintf(buf, "chip:%x-%x, dmx:%d\n", cpu_type, minor_type, version);
	dprint("version:%s", buf);

	return total;
}

ssize_t dmx_ver_show(struct class *class, struct class_attribute *attr,
			 char *buf)
{
	return get_chip_version(buf);
}

int demux_get_stc(int demux_device_index, int index,
		  u64 *stc, unsigned int *base)
{
	struct aml_dvb *dvb = aml_get_dvb_device();

	if (demux_device_index >= DMX_DEV_COUNT || demux_device_index < 0)
		return -1;

	dmx_get_stc(&dvb->dmx[demux_device_index].dmx, index, stc, base);

	return 0;
}
EXPORT_SYMBOL(demux_get_stc);

int demux_get_pcr(int demux_device_index, int index, u64 *pcr)
{
	struct aml_dvb *dvb = aml_get_dvb_device();

	if (demux_device_index >= DMX_DEV_COUNT || demux_device_index < 0)
		return -1;

	return dmx_get_pcr(&dvb->dmx[demux_device_index].dmx, index, pcr);
}
EXPORT_SYMBOL(demux_get_pcr);

ssize_t tsn_source_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	int r, total = 0;
	u32 value = 0;
	int ret = 0;

	if (tsn_in == INPUT_DEMOD)
		r = sprintf(buf, "tsn_source:demod\n");
	else if (tsn_in == INPUT_LOCAL)
		r = sprintf(buf, "tsn_source:local\n");
	else if (tsn_in == INPUT_LOCAL_SEC)
		r = sprintf(buf, "tsn_source:local_sec\n");
	else
		return 0;

	buf += r;
	total += r;

	if (cpu_type == MESON_CPU_MAJOR_ID_T5W)
		ret = tee_read_reg_bits(0xff610320, &value, 0, 2);
	else
		ret = tee_read_reg_bits(0xfe440320, &value, 0, 2);
	if (ret != 0)
		dprint("tee_read_reg_bits value:%d, ret:%d\n", value, ret);

	return total;
}

ssize_t tsn_source_store(struct class *class,
			 struct class_attribute *attr,
			 const char *buf, size_t count)
{
	struct aml_dvb *advb = aml_get_dvb_device();
	int tsn_in_reg = 0;

	if (!strncmp(buf, "demod", 5))
		tsn_in = INPUT_DEMOD;
	else if (!strncmp(buf, "local", 5))
		tsn_in = INPUT_LOCAL;
	else if (!strncmp(buf, "local_sec", 9))
		tsn_in = INPUT_LOCAL_SEC;
	else
		return count;

	if (mutex_lock_interruptible(&advb->mutex))
		return -ERESTARTSYS;

	if (tsn_in == INPUT_DEMOD)
		tsn_in_reg = 1;

//	pr_dbg("tsn_in:%d, tsn_out:%d\n", tsn_in_reg, tsn_out);
	advb->dsc_pipeline = tsn_in_reg;
	//set demod/local
	demux_config_pipeline(tsn_in_reg, tsn_out);

	mutex_unlock(&advb->mutex);
	return count;
}

int tsn_set_double_out(int flag)
{
	int tsn_in_reg = 0;

	if (tsn_out == flag)
		return 0;

	tsn_out = flag;

	if (tsn_in == INPUT_DEMOD)
		tsn_in_reg = 1;

	demux_config_pipeline(tsn_in_reg, tsn_out);
	return 0;
}

ssize_t tso_source_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	int r, total = 0;

	if (tso_src == -1)
		r = sprintf(buf, "tso_source:undefine\n");
	else
		r = sprintf(buf, "tso_source:ts%d\n", tso_src);

	buf += r;
	total += r;
	return total;
}

ssize_t tso_source_store(struct class *class,
			 struct class_attribute *attr,
			 const char *buf, size_t count)
{
	struct aml_dvb *advb = aml_get_dvb_device();

	if (mutex_lock_interruptible(&advb->mutex))
		return -ERESTARTSYS;

	if (!strncmp("ts0", buf, 3))
		tso_src = 0;
	else if (!strncmp("ts1", buf, 3))
		tso_src = 1;
	else if (!strncmp("ts2", buf, 3))
		tso_src = 2;
	else if (!strncmp("ts3", buf, 3))
		tso_src = 3;
	else if (!strncmp("close", buf, 5))
		tso_src = 0xff;

	tso_set(tso_src);

	mutex_unlock(&advb->mutex);
	return count;
}

ssize_t tsn_loop_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	struct aml_dvb *advb = aml_get_dvb_device();
	int r, total = 0;
	u32 val = 0;
	int ret = 0;

	if (cpu_type == MESON_CPU_MAJOR_ID_T5W)
		ret = tee_read_reg_bits(0xff610320, &val, 2, 1);
	else
		ret = tee_read_reg_bits(0xfe440320, &val, 2, 1);
	r = sprintf(buf, "loop:%d, reg val:%d, ret:%d\n", advb->loop_tsn,
		val, ret);

	buf += r;
	total += r;
	return total;
}

ssize_t tsn_loop_store(struct class *class,
			 struct class_attribute *attr,
			 const char *buf, size_t count)
{
	struct aml_dvb *advb = aml_get_dvb_device();
	long val = 0;
	int loop = 0;

	if (kstrtol(buf, 0, &val) == 0) {
		loop = (int)val;
		if (loop != 1 && loop != 0) {
			dprint("tsn err loop:%d\n", loop);
			return count;
		}
		dprint("tsn loop :0x%0x\n", loop);
	} else {
		dprint("%s parameter fail\n", buf);
		return count;
	}

	if (mutex_lock_interruptible(&advb->mutex))
		return -ERESTARTSYS;

	set_dvb_loop_tsn(loop);

	mutex_unlock(&advb->mutex);
	return count;
}

static int lock_err_status;

ssize_t dmx_mutex_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	int r, total = 0;

	r = sprintf(buf, "lock_err_status:%d\n", lock_err_status);

	buf += r;
	total += r;
	return total;
}

ssize_t dmx_mutex_store(struct class *class,
			 struct class_attribute *attr,
			 const char *buf, size_t count)
{
	struct aml_dvb *advb = aml_get_dvb_device();

	if (!strncmp(buf, "lock", 4)) {
		if (mutex_lock_interruptible(&advb->mutex))
			lock_err_status++;
		else
			;//pr_dbg("dmx_mutex lock\n");
	} else if (!strncmp(buf, "unlock", 6)) {
		mutex_unlock(&advb->mutex);
		//pr_dbg("dmx_mutex unlock\n");
	} else {
		dprint("err parameters %s\n", buf);
	}
	return count;
}

ssize_t dmc_mem_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	return dmc_mem_dump_info(buf);
}

ssize_t dmc_mem_store(struct class *class,
			 struct class_attribute *attr,
			 const char *buf, size_t count)
{
	struct aml_dvb *advb = aml_get_dvb_device();
	int sec_level = 0;
	unsigned int size = 0;

	dprint("input: %s\n", buf);
	if (sscanf(buf, "%d %d\n", &sec_level, &size) == 2) {
		dprint("sec_level :0x%0x\n", sec_level);
		dprint("size :0x%0x\n", size);
	} else {
		dprint("%s parameter fail, like sec_level(1~7) mem_size\n",
			buf);
		return count;
	}

	if (mutex_lock_interruptible(&advb->mutex))
		return -ERESTARTSYS;

	dmc_mem_set_size(sec_level, size);

	mutex_unlock(&advb->mutex);
	return count;
}

ssize_t demod_out_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	int r, total = 0;
	u32 val = 0;
	int ret = 0;

	if (cpu_type == MESON_CPU_MAJOR_ID_T5W)
		ret = tee_read_reg_bits(0xff610320, &val, 3, 1);
	else
		ret = tee_read_reg_bits(0xfe440320, &val, 3, 1);

	r = sprintf(buf, "demod out:%d, ret:%d\n", val, ret);
	buf += r;
	total += r;
	return total;
}

ssize_t demod_out_store(struct class *class,
			 struct class_attribute *attr,
			 const char *buf, size_t count)
{
	unsigned int version = get_dmx_version();
	unsigned int value;
	int ret;

	if (version < 5) {
		dprint("can't set demod out version:%d\n", version);
		return count;
	}
	if (buf[0] == '0') {
		value = 0;
		ret = tee_write_reg_bits(0xfe440320, value, 3, 1);
		pr_dbg("value:0x%0x, ret:%d\n", value, ret);
	} else if (buf[0] == '1') {
		value = 1;
		ret = tee_write_reg_bits(0xfe440320, value, 3, 1);
		pr_dbg("value:0x%0x, ret:%d\n", value, ret);
	}
	return count;
}

static CLASS_ATTR_RW(ts_setting);
static CLASS_ATTR_RW(get_pcr);
static CLASS_ATTR_RO(dmx_setting);
static CLASS_ATTR_RO(dsc_setting);
static CLASS_ATTR_RO(dmx_ver);

static CLASS_ATTR_RW(tsn_source);
static CLASS_ATTR_RW(tso_source);
static CLASS_ATTR_RW(tsn_loop);
static CLASS_ATTR_RW(dmc_mem);
static CLASS_ATTR_RW(dmx_mutex);
static CLASS_ATTR_RW(demod_out);

static struct attribute *aml_stb_class_attrs[] = {
	&class_attr_ts_setting.attr,
	&class_attr_get_pcr.attr,
	&class_attr_dmx_setting.attr,
	&class_attr_dsc_setting.attr,
	&class_attr_tsn_source.attr,
	&class_attr_tso_source.attr,
	&class_attr_tsn_loop.attr,
	&class_attr_dmc_mem.attr,
	&class_attr_dmx_ver.attr,
	&class_attr_dmx_mutex.attr,
	&class_attr_demod_out.attr,
	NULL
};

ATTRIBUTE_GROUPS(aml_stb_class);

static struct class aml_stb_class = {
	.name = "stb",
	.class_groups = aml_stb_class_groups,
};

int dmx_get_dev_num(struct platform_device *pdev)
{
	char buf[32];
	u32 dmxdev = 0;
	int ret = 0;

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "dmxdev_num");
	ret = of_property_read_u32(pdev->dev.of_node, buf, &dmxdev);
	if (!ret)
		dprint("%s: 0x%x\n", buf, dmxdev);
	else
		dmxdev = DEFAULT_DMX_DEV_NUM;

	return dmxdev;
}

int dmx_get_tsn_flag(struct platform_device *pdev, int *tsn_in, int *tsn_out)
{
	char buf[32];
	u32 source = 0;
	int ret = 0;
	const char *str;

	*tsn_out = 0;

	source = 0;
	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tsn_from");
	ret = of_property_read_string(pdev->dev.of_node, buf, &str);
	if (!ret) {
		dprint("%s:%s\n", buf, str);
		if (!strcmp(str, "demod"))
			source = INPUT_DEMOD;
		else if (!strcmp(str, "local"))
			source = INPUT_LOCAL;
		else
			source = INPUT_LOCAL_SEC;
	} else {
		source = INPUT_DEMOD;
	}

	*tsn_in = source;
	return 0;
}

struct aml_dvb *aml_get_dvb_device(void)
{
	return &aml_dvb_device;
}
EXPORT_SYMBOL(aml_get_dvb_device);

struct device *aml_get_device(void)
{
	return aml_dvb_device.dev;
}

struct dvb_adapter *aml_get_dvb_adapter(void)
{
	struct device *dev = aml_get_device();

	return aml_dvb_get_adapter(dev);
}
EXPORT_SYMBOL(aml_get_dvb_adapter);

static int aml_dvb_remove(struct platform_device *pdev)
{
	struct aml_dvb *advb;
	struct dvb_adapter *padapter;
	int i;

	advb = &aml_dvb_device;
	padapter = aml_dvb_get_adapter(advb->dev);

	for (i = 0; i < dmx_dev_num; i++) {
		if (advb->tsp[i])
			swdmx_ts_parser_free(advb->tsp[i]);
		if (advb->swdmx[i])
			swdmx_demux_free(advb->swdmx[i]);
	}

	for (i = 0; i < dmx_dev_num; i++) {
		if (advb->dmx[i].init) {
			dmx_destroy(&advb->dmx[i]);
			advb->dmx[i].id = -1;
		}
	}

	for (i = 0; i < dmx_dev_num; i++) {
		dsc_release(&advb->dsc[i]);
		advb->dsc[i].id = -1;
	}

	ts_output_destroy();

	mutex_destroy(&advb->mutex);
	aml_dvb_put_adapter(padapter);
	class_unregister(&aml_stb_class);
	dmx_unregist_dmx_class();

	return 0;
}

static int get_first_valid_ts(struct aml_dvb *advb)
{
	int i = 0;

	for (i = 0; i < FE_DEV_COUNT; i++) {
		if (advb->ts[i].ts_sid != -1)
			return i;
	}
	return 0;
}

int get_demux_feature(int support_feature)
{
	unsigned int version = get_dmx_version();

	if (support_feature == SUPPORT_ES_HEADER_NEED_AUCPU) {
		if (cpu_type == MESON_CPU_MAJOR_ID_SC2 ||
			cpu_type == MESON_CPU_MAJOR_ID_S4 ||
			cpu_type == MESON_CPU_MAJOR_ID_T7)
			return 1;
		else
			if (version >= 3)
				return 0;
			else
				return 1;
	} else if (support_feature == SUPPORT_TSD) {
		if (cpu_type == MESON_CPU_MAJOR_ID_SC2)
			return 1;
		else if ((cpu_type == MESON_CPU_MAJOR_ID_T7 && minor_type == 0xc) ||
				(cpu_type == MESON_CPU_MAJOR_ID_S5 && minor_type == 0xa))
			return 0;
		else
			return 0;
	} else if (support_feature == SUPPORT_PSCP) {
		if (version >= 4)
			return 1;
		return 0;
	} else if (support_feature == SUPPORT_TEMI) {
		if (version >= 4)
			return 1;
		else
			return 0;
	} else if (support_feature == SUPPORT_PES_HEADER) {
		return 0;
	} else {
		return 0;
	}
}

static int aml_dvb_probe(struct platform_device *pdev)
{
	struct aml_dvb *advb;
	int i, ret = 0;
	int tsn_in_reg = 0;
	int valid_ts = 0;
	char buf[255];
	struct dvb_adapter *padater;
	int hw_source = 0;

	dprint("probe amlogic dvb driver [%s].\n", DVB_VERSION);
	memset(&buf, 0, sizeof(buf));
	cpu_type = get_cpu_type();
	minor_type = get_meson_cpu_version(MESON_CPU_VERSION_LVL_MINOR);

	advb = &aml_dvb_device;
	memset(advb, 0, sizeof(aml_dvb_device));

	advb->dev = &pdev->dev;
	advb->pdev = pdev;

	padater = aml_dvb_get_adapter(advb->dev);
//	ret = dvb_register_adapter(padater, CARD_NAME, THIS_MODULE,
//				   advb->dev, adapter_nr);
//	if (ret < 0)
//		return ret;

	mutex_init(&advb->mutex);

	ret = tee_demux_get(TEE_DMX_GET_SECURITY_ENABLE,
			NULL, 0, &is_security_dmx, sizeof(is_security_dmx));

	ret = init_demux_addr(pdev);
	if (ret != 0)
		return ret;

	get_chip_version(buf);

	frontend_probe(pdev);
	dmx_dev_num = dmx_get_dev_num(pdev);

	dmx_get_tsn_flag(pdev, &tsn_in, &tsn_out);

	if (tsn_in == INPUT_DEMOD)
		tsn_in_reg = 1;

	pr_dbg("tsn_in:%d, tsn_out:%d\n", tsn_in_reg, tsn_out);
	advb->dsc_pipeline = tsn_in_reg;
	//set demod/local
	demux_config_pipeline(tsn_in_reg, tsn_out);

	dmx_init_hw();

	valid_ts = get_first_valid_ts(advb);

	if (tsn_in == INPUT_DEMOD)
		hw_source = FRONTEND_TS0 + valid_ts;
	else
		hw_source = DMA_0;

	//create dmx dev
	for (i = 0; i < dmx_dev_num; i++) {
		advb->swdmx[i] = swdmx_demux_new();
		if (!advb->swdmx[i])
			goto INIT_ERR;

		advb->tsp[i] = swdmx_ts_parser_new();
		if (!advb->tsp[i])
			goto INIT_ERR;

		swdmx_ts_parser_add_ts_packet_cb(advb->tsp[i],
						 swdmx_demux_ts_packet_cb,
						 advb->swdmx[i]);

		advb->dmx[i].id = i;
		advb->dmx[i].pmutex = &advb->mutex;
		advb->dmx[i].swdmx = advb->swdmx[i];
		advb->dmx[i].tsp = advb->tsp[i];
		advb->dmx[i].source = tsn_in;
		advb->dmx[i].ts_index = valid_ts;
		if (tsn_in == INPUT_DEMOD)
			advb->dmx[i].sid = advb->ts[advb->dmx[i].ts_index].ts_sid;
		else
			advb->dmx[i].sid = -1;
		advb->dmx[i].hw_source = hw_source;
		ret = dmx_init(&advb->dmx[i], padater);
		if (ret)
			goto INIT_ERR;

		advb->dsc[i].mutex = advb->mutex;
//              advb->dsc[i].slock = advb->slock;
		advb->dsc[i].id = i;
		advb->dsc[i].sid = advb->dmx[i].sid;

		ret = dsc_init(&advb->dsc[i], padater);
		if (ret)
			goto INIT_ERR;
	}
	frontend_config_ts_sid();

	class_register(&aml_stb_class);
	dmx_regist_dmx_class();

	dprint("probe dvb done, ret:%d, is_security_dmx:%d\n",
			ret, is_security_dmx);

	return 0;

INIT_ERR:
	aml_dvb_remove(pdev);

	return -1;
}

void set_dvb_loop_tsn(int flag)
{
	struct aml_dvb *advb;
	int ret = 0;

	advb = &aml_dvb_device;

	if (advb->loop_tsn == flag)
		return;

	// set loop_tsn in tee
	advb->loop_tsn = flag;
	if (cpu_type == MESON_CPU_MAJOR_ID_T5W)
		ret = tee_write_reg_bits(0xff610320, (u32)flag, 2, 1);
	else
		ret = tee_write_reg_bits(0xfe440320, (u32)flag, 2, 1);
}

int get_dvb_loop_tsn(void)
{
	struct aml_dvb *advb;

	advb = &aml_dvb_device;

	return advb->loop_tsn;
}

#ifdef CONFIG_OF
static const struct of_device_id aml_dvb_dt_match[] = {
	{
	 .compatible = "amlogic sc2, dvb-demux",
	 },
	{}
};
#endif /*CONFIG_OF */

struct platform_driver aml_dvb_driver = {
	.probe = aml_dvb_probe,
	.remove = aml_dvb_remove,
	.suspend = NULL,
	.resume = NULL,
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
	dprint("aml dvb init\n");
	return platform_driver_register(&aml_dvb_driver);
}

static void __exit aml_dvb_exit(void)
{
	dprint("aml dvb exit\n");
	platform_driver_unregister(&aml_dvb_driver);
}

module_init(aml_dvb_init);
module_exit(aml_dvb_exit);

MODULE_DESCRIPTION("driver for the AMLogic DVB card");
MODULE_AUTHOR("AMLOGIC");
MODULE_LICENSE("GPL");
