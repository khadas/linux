/*
 *
 * Remote Driver
 *
 * Copyright (C) 2013 Amlogic Corporation
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <linux/amlogic/scpi_protocol.h>


/*#include <mach/pinmux.h>*/
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_platform.h>
#include "remote_main.h"
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/pm.h>
#include <linux/of_address.h>
#undef NEW_BOARD_LEARNING_MODE
#define IR_CONTROL_DECODER_MODE     (3<<7)
#define IR_CONTROL_SKIP_HEADER      (1<<7)
#define IR_CONTROL_RESET            (1<<0)
#define KEY_RELEASE_DELAY    200

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static struct early_suspend early_suspend;
#endif

static bool key_pointer_switch = true;
static unsigned int FN_KEY_SCANCODE = 0x3ff;
static unsigned int OK_KEY_SCANCODE = 0x3ff;
type_printk input_dbg;
static DEFINE_MUTEX(remote_enable_mutex);
static DEFINE_MUTEX(remote_file_mutex);
static void remote_tasklet(unsigned long);
static int remote_enable;
static int NEC_REMOTE_IRQ_NO = -1;
unsigned long g_remote_ao_offset;
DECLARE_TASKLET_DISABLED(tasklet, remote_tasklet, 0);
/*static int repeat_flag;*/
static struct remote *gp_remote;
char *remote_log_buf;
/* use 20 map for this driver*/
static __u16 key_map[20][512];

#ifdef REMOTE_FIQ
static  irqreturn_t (*remote_bridge_sw_isr[])(int irq, void *dev_id) = {
	remote_null_bridge_isr,
	remote_null_bridge_isr,
	remote_null_bridge_isr,
	remote_null_bridge_isr,
	remote_null_bridge_isr,
	remote_null_bridge_isr,
	remote_null_bridge_isr,
	remote_null_bridge_isr,
	remote_null_bridge_isr,
	remote_null_bridge_isr,
	remote_null_bridge_isr,
	remote_null_bridge_isr,
	remote_null_bridge_isr,
	remote_null_bridge_isr,
	remote_null_bridge_isr,
	remote_bridge_isr,
	remote_bridge_isr,
};
#endif
static  int (*remote_report_key[])(struct remote *remote_data) = {
	remote_hw_report_key,
	remote_duokan_report_key,
	remote_hw_report_null_key,
	remote_hw_report_null_key,
	remote_hw_report_null_key,
	remote_hw_report_null_key,
	remote_hw_report_null_key,
	remote_hw_report_null_key,
	remote_rc6_report_key,
	remote_hw_report_null_key,
	remote_hw_report_null_key,
	remote_hw_report_null_key,
	remote_hw_report_null_key,
	remote_hw_report_null_key,
	remote_hw_nec_rca_2in1_report_key,
	remote_hw_nec_toshiba_2in1_report_key,
	remote_hw_nec_rcmm_2in1_report_key,
	remote_sw_report_key
};

static  void (*remote_report_release_key[])(struct remote *remote_data) = {
	remote_nec_report_release_key,
	remote_duokan_report_release_key,
	remote_null_report_release_key,
	remote_null_report_release_key,
	remote_null_report_release_key,
	remote_null_report_release_key,
	remote_null_report_release_key,
	remote_null_report_release_key,
	remote_rc6_report_release_key,
	remote_null_report_release_key,
	remote_null_report_release_key,
	remote_null_report_release_key,
	remote_null_report_release_key,
	remote_null_report_release_key,
	remote_nec_rca_2in1_report_release_key,
	remote_nec_toshiba_2in1_report_release_key,
	remote_nec_rcmm_2in1_report_release_key,
	remote_sw_report_release_key
};
static __u16 mouse_map[20][6];
int remote_printk(const char *fmt, ...)
{
	va_list args;
	int r;

	if (gp_remote->debug_enable == 0)
		return 0;
	pr_info("remote: ");
	va_start(args, fmt);
	r = vprintk(fmt, args);
	va_end(args);
	return r;
}

static int remote_mouse_event(struct input_dev *dev, unsigned int scancode,
			      unsigned int type, bool flag)
{

	__u16 mouse_code = REL_X;
	__s32 mouse_value = 0;
	static unsigned int repeat_count;
	__s32 move_accelerate[] = {0, 2, 2, 4, 4, 6, 8, 10, 12, 14, 16, 18};
	unsigned int i;

	if (flag)
		return 1;
	for (i = 0; i < ARRAY_SIZE(mouse_map[gp_remote->map_num]); i++)
		if (mouse_map[gp_remote->map_num][i] == scancode)
			break;

	if (i >= ARRAY_SIZE(mouse_map[gp_remote->map_num]))
		return -1;
	switch (type) {
	case 1:     /*press*/
		repeat_count = 0;
		break;
	case 2:     /*repeat*/
		if (repeat_count >= ARRAY_SIZE(move_accelerate) - 1)
			repeat_count = ARRAY_SIZE(move_accelerate) - 1;
		else
			repeat_count++;
	}
	switch (i) {
	case 0:
		mouse_code = REL_X;
		mouse_value = -(1 + move_accelerate[repeat_count]);
		break;
	case 1:
		mouse_code = REL_X;
		mouse_value = 1 + move_accelerate[repeat_count];
		break;
	case 2:
		mouse_code = REL_Y;
		mouse_value = -(1 + move_accelerate[repeat_count]);
		break;
	case 3:
		mouse_code = REL_Y;
		mouse_value = 1 + move_accelerate[repeat_count];
		break;
	case 4:     /*up*/
		mouse_code = REL_WHEEL;
		mouse_value = 0x1;
		break;
	case 5:
		mouse_code = REL_WHEEL;
		mouse_value = 0xffffffff;
		break;

	}
	if (type) {
		input_event(dev, EV_REL, mouse_code, mouse_value);
		input_sync(dev);
		switch (mouse_code) {
		case REL_X:
		case REL_Y:
			input_dbg("mouse be %s moved %d.\n",
				mouse_code == REL_X ? "horizontal" :
							"vertical",
							mouse_value);
			break;
		case REL_WHEEL:
			input_dbg("mouse wheel move %s .\n",
				mouse_value == 0x1 ? "up" : "down");
			break;
		}

	}
	return 0;
}

void remote_send_key(struct input_dev *dev, unsigned int scancode,
		     unsigned int type, int event)
{
	if (scancode == FN_KEY_SCANCODE && type == 1) {
		/* switch from key to pointer*/
		if (key_pointer_switch) {
			key_pointer_switch = false;
			gp_remote->repeat_enable = 1;
			gp_remote->input->rep[REP_DELAY] =
				gp_remote->repeat_delay[gp_remote->map_num];
			gp_remote->input->rep[REP_PERIOD] =
				gp_remote->repeat_peroid[gp_remote->map_num];
		}
		/* switch from pointer to key*/
		else {
			key_pointer_switch = true;
			gp_remote->repeat_enable = 0;
			gp_remote->input->rep[REP_DELAY] = 0xffffffff;
			gp_remote->input->rep[REP_PERIOD] = 0xffffffff;
		}
	}

	if (scancode == OK_KEY_SCANCODE && key_pointer_switch == false) {
		input_event(dev, EV_KEY, BTN_MOUSE, type);
		input_sync(dev);

		return;
	}

	if (remote_mouse_event(dev, scancode, type, key_pointer_switch)) {
		if (scancode > ARRAY_SIZE(key_map[gp_remote->map_num])) {
			input_dbg("scancode is 0x%04x, out of key mapping.\n",
				scancode);
			return;
		}
		if ((key_map[gp_remote->map_num][scancode] >= KEY_MAX)
			|| (key_map[gp_remote->map_num][scancode] ==
				KEY_RESERVED)){
			input_dbg("scancode is 0x%04x,invalid key is 0x%04x.\n",
					scancode,
					key_map[gp_remote->map_num][scancode]);
			return;
		}

		if (type == 2 &&
			key_map[gp_remote->map_num][scancode] == 0x0074)
			return;
		else {
			input_event(dev, EV_KEY,
					key_map[gp_remote->map_num][scancode],
					type);
			input_sync(dev);
		}

		switch (type) {
		case 0:
			input_dbg("release ircode = 0x%02x,",
					scancode);
			input_dbg("scancode = 0x%04x, maptable = %d,code:0x%08x\n\n",
					key_map[gp_remote->map_num][scancode],
					gp_remote->map_num,
					gp_remote->cur_lsbkeycode);
			break;
		case 1:
			input_dbg("press ircode = 0x%02x,",
					scancode);
			input_dbg("scancode = 0x%04x,maptable = %d,code:0x%08x\n\n",
					key_map[gp_remote->map_num][scancode],
					gp_remote->map_num,
					gp_remote->cur_lsbkeycode);
			break;
		case 2:
			input_dbg("repeat ircode = 0x%02x,",
					scancode);
			input_dbg("scancode = 0x%04x, maptable = %d,code:0x%08x\n\n",
					key_map[gp_remote->map_num][scancode],
					gp_remote->map_num,
					gp_remote->cur_lsbkeycode);
			break;
		}
		if (gp_remote->sleep && scancode == 0x1a &&
		    key_map[gp_remote->map_num][scancode] == 0x0074) {
			input_dbg(" set 0x4853ffff\n");
		}
	}
}

static void disable_remote_irq(void)
{
	if (gp_remote->work_mode >= DECODEMODE_MAX)
		disable_irq(NEC_REMOTE_IRQ_NO);
}

static void enable_remote_irq(void)
{
	if (gp_remote->work_mode >= DECODEMODE_MAX)
		enable_irq(NEC_REMOTE_IRQ_NO);

}

void remote_reprot_key(struct remote *remote_data)
{
	remote_report_key[remote_data->work_mode](remote_data);
}

static void remote_release_timer_sr(unsigned long data)
{
	struct remote *remote_data = (struct remote *)data;
	/*key report release use timer interrupt*/
	remote_data->key_release_report =
		remote_report_release_key[remote_data->work_mode];
	remote_data->key_release_report(remote_data);
}

static irqreturn_t remote_interrupt(int irq, void *dev_id)
{
	gp_remote->jiffies_irq = jiffies;

	tasklet_schedule(&tasklet);
	return IRQ_HANDLED;
}
#ifdef REMOTE_FIQ
static void remote_fiq_interrupt(void)
{
	remote_reprot_key(gp_remote);
}
#endif

static void remote_tasklet(unsigned long data)
{
	struct remote *remote_data = (struct remote *)data;
	remote_reprot_key(remote_data);
}

static ssize_t remote_log_buffer_show(struct device *dev,
				      struct device_attribute *attr,
					char *buf)
{
	int ret = 0;
	ret = sprintf(buf, "%s\n", remote_log_buf);
	remote_log_buf[0] = '\0';
	return ret;
}

static ssize_t remote_enable_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", remote_enable);
}
static ssize_t remote_enable_store(struct device *dev,
				   struct device_attribute *attr,
					const char *buf, size_t count)
{
	int state;

	if (sscanf(buf, "%u", &state) != 1)
		return -EINVAL;

	if ((state != 1) && (state != 0))
		return -EINVAL;

	mutex_lock(&remote_enable_mutex);
	if (state != remote_enable) {
		if (state)
			enable_remote_irq();
		else
			disable_remote_irq();
		remote_enable = state;
	}
	mutex_unlock(&remote_enable_mutex);

	return strnlen(buf, count);
}


static ssize_t remote_powerkey_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	unsigned int val = 0;
	unsigned int ret;

	ret = scpi_get_usr_data(SCPI_CL_POWER, &val, 1);
	if (IS_ERR_VALUE(ret)) {
		pr_info("scpi_get_usr_data error: val =0x%x\n", val);
		return 0;
	}
	return sprintf(buf, "powerkey = 0x%x\n", val);
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, remote_enable_show,
		   remote_enable_store);
static DEVICE_ATTR(log_buffer, S_IRUGO , remote_log_buffer_show, NULL);
static DEVICE_ATTR(powerkey, S_IRUGO , remote_powerkey_show, NULL);

static int hardware_init(struct platform_device *pdev)
{
	struct pinctrl *p;
	p = devm_pinctrl_get_select_default(&pdev->dev);
	if (IS_ERR(p)) {
		input_dbg("hardware init fail, %ld\n", PTR_ERR(p));
		return -1;
	}
	set_remote_mode(DECODEMODE_NEC);

	gp_remote->jiffies_old = jiffies;
	gp_remote->jiffies_new = jiffies;
	gp_remote->keystate = RC_KEY_STATE_UP;

	return request_irq(NEC_REMOTE_IRQ_NO, remote_interrupt, IRQF_SHARED,
				"keypad",
				(void *)remote_interrupt);
}
static int work_mode_config(unsigned int cur_mode)
{

#ifdef REMOTE_FIQ
	struct irq_desc *desc = irq_to_desc(NEC_REMOTE_IRQ_NO);
#endif
	int ret;
	input_dbg("cur_mode = %d\n", cur_mode);
	if (cur_mode > DECODEMODE_MAX) {
		/*g_remote_ao_offset = P_AO_IR_DEC_LDR_ACTIVE;//change to old
		ir decode*/
		input_dbg("Error!!! you are setting one invalid mode\n");
		return -1;
	}
	set_remote_mode(cur_mode);
	set_remote_init(gp_remote);
	if (cur_mode == gp_remote->save_mode)
		return 0;
	if ((cur_mode <= DECODEMODE_MAX)  &&
		(gp_remote->save_mode > DECODEMODE_MAX)) {
#ifdef REMOTE_FIQ
		unregister_fiq_bridge_handle(&gp_remote->fiq_handle_item);
		free_fiq(NEC_REMOTE_IRQ_NO, &remote_fiq_interrupt);
#endif
		ret = request_irq(NEC_REMOTE_IRQ_NO, remote_interrupt,
					IRQF_SHARED, "keypad",
				  (void *)remote_interrupt);
		if (ret < 0) {
			input_dbg("request_irq failed, ret=%d.\n", ret);
			return ret;
		}
	} else if ((cur_mode > DECODEMODE_MAX)  &&
		   (gp_remote->save_mode < DECODEMODE_MAX)) {
		free_irq(NEC_REMOTE_IRQ_NO, remote_interrupt);
#ifdef REMOTE_FIQ
		gp_remote->fiq_handle_item.handle =
		remote_bridge_sw_isr[gp_remote->work_mode];
		gp_remote->fiq_handle_item.key = (u32) gp_remote;
		gp_remote->fiq_handle_item.name = "remote_bridge";
		register_fiq_bridge_handle(&gp_remote->fiq_handle_item);
		desc->depth++;
		request_fiq(NEC_REMOTE_IRQ_NO, remote_fiq_interrupt);
#endif
	} else
		input_dbg("do nothing\n");
	gp_remote->save_mode = cur_mode;
	return 0;
}

static int remote_config_open(struct inode *inode, struct file *file)
{
	file->private_data = gp_remote;
	return 0;
}

static long remote_config_ioctl(struct file *filp, unsigned int cmd,
				unsigned long args)
{
	struct remote *remote = (struct remote *)filp->private_data;
	void __user *argp = (void __user *)args;
	unsigned int val, i;
	unsigned int ret;

	if (args)
		ret = copy_from_user(&val, argp, sizeof(unsigned long));
	mutex_lock(&remote_file_mutex);
	switch (cmd) {
	case REMOTE_IOC_INFCODE_CONFIG:
		remote->map_num = val;
		break;
	case REMOTE_IOC_RESET_KEY_MAPPING:
		for (i = 0; i < ARRAY_SIZE(key_map[remote->map_num]); i++)
			key_map[remote->map_num][i] = KEY_RESERVED;
		for (i = 0; i < ARRAY_SIZE(mouse_map[remote->map_num]); i++)
			mouse_map[remote->map_num][i] = 0xffff;
		break;
	case REMOTE_IOC_SET_REPEAT_KEY_MAPPING:
		if ((val >> 16) >= ARRAY_SIZE(remote->key_repeat_map)) {
			mutex_unlock(&remote_file_mutex);
			return -1;
		}
		remote->key_repeat_map[remote->map_num][val >> 16] =
							val & 0xffff;
		break;
	case REMOTE_IOC_SET_KEY_MAPPING:
		if ((val >> 16) >= ARRAY_SIZE(key_map[remote->map_num])) {
			mutex_unlock(&remote_file_mutex);
			return -1;
		}
		key_map[remote->map_num][val >> 16] = val & 0xffff;
		break;
	case REMOTE_IOC_SET_MOUSE_MAPPING:
		if ((val >> 16) >= ARRAY_SIZE(mouse_map[remote->map_num])) {
			mutex_unlock(&remote_file_mutex);
			return -1;
		}
		mouse_map[remote->map_num][val >> 16] = val & 0xff;
		break;
	case REMOTE_IOC_SET_RELT_DELAY:
		ret = copy_from_user(&remote->relt_delay[remote->map_num],
					argp, sizeof(long));
		break;
	case REMOTE_IOC_SET_REPEAT_DELAY:
		ret = copy_from_user(&remote->repeat_delay[remote->map_num],
					argp,
					sizeof(long));
		break;
	case REMOTE_IOC_SET_REPEAT_PERIOD:
		ret = copy_from_user(&remote->repeat_peroid[remote->map_num],
					argp,
					sizeof(long));
		break;
	case REMOTE_IOC_SET_REPEAT_ENABLE:
		ret = copy_from_user(&remote->repeat_enable, argp,
					sizeof(long));
		break;
	case REMOTE_IOC_SET_DEBUG_ENABLE:
		ret = copy_from_user(&remote->debug_enable, argp,
							sizeof(long));
		break;
	case REMOTE_IOC_SET_MODE:
		ret = copy_from_user(&remote->work_mode, argp, sizeof(long));
		break;
	case REMOTE_IOC_SET_BIT_COUNT:
		ret = copy_from_user(&remote->bit_count, argp, sizeof(long));
		break;
	case REMOTE_IOC_SET_CUSTOMCODE:
		ret = copy_from_user(&remote->custom_code[remote->map_num],
							argp, sizeof(long));
		break;
	case REMOTE_IOC_SET_REG_BASE_GEN:
		am_remote_write_reg(OPERATION_CTRL_REG0, val);
		break;
	case REMOTE_IOC_SET_REG_CONTROL:
		am_remote_write_reg(DURATION_REG1_AND_STATUS, val);
		break;
	case REMOTE_IOC_SET_REG_LEADER_ACT:
		am_remote_write_reg(LDR_ACTIVE, val);
		break;
	case REMOTE_IOC_SET_REG_LEADER_IDLE:
		am_remote_write_reg(LDR_IDLE, val);
		break;
	case REMOTE_IOC_SET_REG_REPEAT_LEADER:
		am_remote_write_reg(LDR_REPEAT, val);
		break;
	case REMOTE_IOC_SET_REG_BIT0_TIME:
		am_remote_write_reg(DURATION_REG0, val);
		break;
	case REMOTE_IOC_SET_RELEASE_DELAY:
		ret = copy_from_user(&remote->release_delay[remote->map_num],
					argp,
				sizeof(long));
		break;
	/*SW*/
	case REMOTE_IOC_SET_TW_LEADER_ACT:
		remote->time_window[0] = val & 0xffff;
		remote->time_window[1] = (val >> 16) & 0xffff;
		break;
	case REMOTE_IOC_SET_TW_BIT0_TIME:
		remote->time_window[2] = val & 0xffff;
		remote->time_window[3] = (val >> 16) & 0xffff;
		break;
	case REMOTE_IOC_SET_TW_BIT1_TIME:
		remote->time_window[4] = val & 0xffff;
		remote->time_window[5] = (val >> 16) & 0xffff;
		break;
	case REMOTE_IOC_SET_TW_REPEATE_LEADER:
		remote->time_window[6] = val & 0xffff;
		remote->time_window[7] = (val >> 16) & 0xffff;
		break;
	case REMOTE_IOC_SET_TW_BIT2_TIME:
		remote->time_window[8] = val & 0xffff;
		remote->time_window[9] = (val >> 16) & 0xffff;
		break;
	case REMOTE_IOC_SET_TW_BIT3_TIME:
		remote->time_window[10] = val & 0xffff;
		remote->time_window[11] = (val >> 16) & 0xffff;
		break;
	case REMOTE_IOC_SET_FN_KEY_SCANCODE:
		FN_KEY_SCANCODE = val;
		break;
	case REMOTE_IOC_SET_OK_KEY_SCANCODE:
		OK_KEY_SCANCODE = val;
		break;
	}
	/*output result*/
	switch (cmd) {
	case REMOTE_IOC_SET_REPEAT_ENABLE:
		if (remote->repeat_enable) {
			remote->input->rep[REP_DELAY] =
				remote->repeat_delay[remote->map_num];
			remote->input->rep[REP_PERIOD] =
				remote->repeat_peroid[remote->map_num];
		} else {
			remote->input->rep[REP_DELAY] = 0xffffffff;
			remote->input->rep[REP_PERIOD] = 0xffffffff;
		}
		break;
	case REMOTE_IOC_SET_MODE:
		work_mode_config(remote->work_mode);
		break;
	case REMOTE_IOC_GET_REG_BASE_GEN:
	case REMOTE_IOC_GET_REG_CONTROL:
	case REMOTE_IOC_GET_REG_LEADER_ACT:
	case REMOTE_IOC_GET_REG_LEADER_IDLE:
	case REMOTE_IOC_GET_REG_REPEAT_LEADER:
	case REMOTE_IOC_GET_REG_BIT0_TIME:
	case REMOTE_IOC_GET_REG_FRAME_DATA:
	case REMOTE_IOC_GET_REG_FRAME_STATUS:
	case REMOTE_IOC_GET_TW_LEADER_ACT:
	case REMOTE_IOC_GET_TW_BIT0_TIME:
	case REMOTE_IOC_GET_TW_BIT1_TIME:
	case REMOTE_IOC_GET_TW_REPEATE_LEADER:
		ret = copy_to_user(argp, &val, sizeof(long));
		break;
	case REMOTE_IOC_GET_POWERKEY:
		ret = scpi_get_usr_data(SCPI_CL_POWER, &val, 1);
		if (IS_ERR_VALUE(ret)) {
			pr_info("scpi_get_usr_data SCPI_CL_POWER error\n");
			mutex_unlock(&remote_file_mutex);
			return -1;
		}
		ret = copy_to_user(argp, &val, sizeof(long));
		break;
	}
	mutex_unlock(&remote_file_mutex);
	return 0;
}

static int remote_config_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static const struct file_operations remote_fops = {
	.owner = THIS_MODULE,
	.open = remote_config_open,
#ifdef CONFIG_COMPAT
	.compat_ioctl =	remote_config_ioctl,
#endif
	.unlocked_ioctl = remote_config_ioctl,
	.release = remote_config_release,
};

static int register_remote_dev(struct remote *remote)
{
	int ret = 0;
	strcpy(remote->config_name, "amremote");
	ret = register_chrdev(0, remote->config_name, &remote_fops);
	if (ret <= 0) {
		input_dbg("register char dev tv error\r\n");
		return ret;
	}
	remote->config_major = ret;
	input_dbg("remote config major:%d\r\n", ret);
	remote->config_class = class_create(THIS_MODULE, remote->config_name);
	remote->config_dev = device_create(remote->config_class, NULL,
					   MKDEV(remote->config_major, 0),
						NULL, remote->config_name);
	return ret;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void remote_early_suspend(struct early_suspend *handler)
{
	input_dbg("remote_early_suspend, set sleep 1\n");
	gp_remote->sleep = 1;
	return;
}
#endif

static const struct of_device_id remote_dt_match[] = {
	{
		.compatible     = "amlogic, aml_remote",
	},
	{},
};


static int remote_probe(struct platform_device *pdev)
{
	struct remote *remote;
	struct input_dev *input_dev;
	int ao_offset;
	struct resource *res_irq;
	int i, ret;

	/*aml_set_reg32_mask(P_AO_RTI_PIN_MUX_REG, (1 << 0));*/
	if (!pdev->dev.of_node) {
		pr_err("remote: pdev->dev.of_node == NULL!\n");
		return -1;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "remote_ao_offset",
					&ao_offset);
	if (ret) { /*new decoder*/
		pr_err("remote: cant find match item for remote_ao_offset\n");
		return -1;
	}
	/*ao_offset = P_AO_IR_DEC_LDR_ACTIVE;*/
	/*ao_offset = P_AO_MF_IR_DEC_LDR_ACTIVE;*/
	g_remote_ao_offset = ao_offset;
	pr_info("remote: platform_data g_remote_ao_offset=%x\n", ao_offset);
	/*get irq number.*/
	res_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	NEC_REMOTE_IRQ_NO = res_irq->start;
	pr_info("remote: platform_data irq =%d\n", NEC_REMOTE_IRQ_NO);
	remote_enable = 1;
	remote = kzalloc(sizeof(struct remote), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!remote || !input_dev) {
		kfree(remote);
		input_free_device(input_dev);
		return -ENOMEM;
	}
	gp_remote = remote;
	remote->debug_enable = 1;
	remote->ig_custom_enable = 1;
	gp_remote->remote_send_key = remote_send_key;
	input_dbg = remote_printk;
	platform_set_drvdata(pdev, remote);
	remote->map_num = 0;
	remote->work_mode = remote->save_mode = DECODEMODE_NEC;
	remote->input = input_dev;
	remote->release_fdelay = KEY_RELEASE_DELAY;
	remote->custom_code[remote->map_num] = 0xfb04;
	for (i = 1; i < 20; i++)
		remote->custom_code[i] = 0xffff;
	remote->bit_count = 32;
	remote->last_jiffies = 0xffffffff;
	remote->last_pulse_width = 0;
	remote->step = REMOTE_STATUS_WAIT;
	remote->sleep = 0;
	/*init logic0 logic1  time window*/
	for (i = 0; i < 18; i++)
		remote->time_window[i] = 0x1;
	/* Disable the interrupt for the MPUIO keyboard
	init the default key map table ,and mouse map table.
	note KEY_RESERVED==0*/
	memset(key_map, 0x00, sizeof(key_map));
	memset(mouse_map, 0xff, sizeof(mouse_map));
	remote->repeat_delay[remote->map_num]  = 250;
	remote->repeat_peroid[remote->map_num]  = 33;
	/* get the irq and init timer */
	input_dbg("set drvdata completed\r\n");
	tasklet_enable(&tasklet);
	tasklet.data = (unsigned long)remote;
	setup_timer(&remote->timer, remote_release_timer_sr, 0);
	/*read status & frame register to abandon last key from uboot*/
	am_remote_read_reg(DURATION_REG1_AND_STATUS);
	am_remote_read_reg(FRAME_BODY);
	/*reset IR decode*/
	am_remote_write_reg(OPERATION_CTRL_REG1, 0x1f01);
#ifdef CONFIG_HAS_EARLYSUSPEND
	early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING + 1;
	early_suspend.suspend = remote_early_suspend;
	early_suspend.resume = NULL;
	early_suspend.param = gp_remote;
	register_early_suspend(&early_suspend);
#endif

	ret = device_create_file(&pdev->dev, &dev_attr_enable);
	if (ret < 0)
		goto err1;
	ret = device_create_file(&pdev->dev, &dev_attr_log_buffer);
	if (ret < 0) {
		device_remove_file(&pdev->dev, &dev_attr_enable);
		goto err1;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_powerkey);
	if (ret < 0) {
		device_remove_file(&pdev->dev, &dev_attr_enable);
		device_remove_file(&pdev->dev, &dev_attr_log_buffer);
		goto err1;
	}

	input_dbg("device_create_file completed \r\n");
	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REL)
						| BIT_MASK(EV_ABS);
	input_dev->keybit[BIT_WORD(BTN_MOUSE)] = BIT_MASK(BTN_LEFT) |
				BIT_MASK(BTN_RIGHT) | BIT_MASK(BTN_MIDDLE);
	input_dev->relbit[0] = BIT_MASK(REL_X) | BIT_MASK(REL_Y) | BIT_MASK
						(REL_WHEEL);
	input_dev->keybit[BIT_WORD(BTN_MOUSE)] |= BIT_MASK(BTN_SIDE)
						| BIT_MASK(BTN_EXTRA);
	for (i = 0; i < KEY_MAX; i++)
		set_bit(i, input_dev->keybit);
	input_dev->name = "aml_keypad";
	input_dev->phys = "keypad/input0";
	input_dev->dev.parent = &pdev->dev;
	input_dev->id.bustype = BUS_ISA;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0100;
	remote->repeat_enable = 0;
	input_dev->rep[REP_DELAY] = 0xffffffff;
	input_dev->rep[REP_PERIOD] = 0xffffffff;
	input_dev->keycodesize = sizeof(unsigned short);
	input_dev->keycodemax = 0x1ff;
	ret = input_register_device(remote->input);
	if (ret < 0) {
		input_dbg("Unable to register keypad input device\n");
		goto err2;
	}
	input_dbg("input_register_device completed \r\n");


	if (hardware_init(pdev))
		goto err3;
	register_remote_dev(gp_remote);
	remote_log_buf = (char *)__get_free_pages(GFP_KERNEL,
					REMOTE_LOG_BUF_ORDER);
	remote_log_buf[0] = '\0';
	input_dbg("physical address:0x%x\n",
		(unsigned int)virt_to_phys(remote_log_buf));
	return 0;

err3:
	input_unregister_device(remote->input);
	input_dev = NULL;
err2:
	device_remove_file(&pdev->dev, &dev_attr_enable);
	device_remove_file(&pdev->dev, &dev_attr_log_buffer);
err1:
	kfree(remote);
	input_free_device(input_dev);
	return -EINVAL;
}
static int remote_remove(struct platform_device *pdev)
{
	struct remote *remote = platform_get_drvdata(pdev);
	/* disable keypad interrupt handling */
	input_dbg("remove remote\r\n");
	tasklet_disable(&tasklet);
	tasklet_kill(&tasklet);
	/* unregister everything */
	input_unregister_device(remote->input);
	free_pages((unsigned long)remote_log_buf, REMOTE_LOG_BUF_ORDER);
	device_remove_file(&pdev->dev, &dev_attr_enable);
	device_remove_file(&pdev->dev, &dev_attr_log_buffer);
	if (gp_remote->work_mode >= DECODEMODE_MAX) {
#ifdef REMOTE_FIQ
		free_fiq(NEC_REMOTE_IRQ_NO, remote_fiq_interrupt);
		free_irq(BRIDGE_IRQ, gp_remote);
#endif
	} else
		free_irq(NEC_REMOTE_IRQ_NO, remote_interrupt);
	input_free_device(remote->input);

	unregister_chrdev(remote->config_major, remote->config_name);
	if (remote->config_class) {
		if (remote->config_dev)
			device_destroy(remote->config_class,
			 MKDEV(remote->config_major, 0));
		class_destroy(remote->config_class);
	}

	kfree(remote);
	gp_remote = NULL;
	return 0;
}

static int remote_resume(struct platform_device *pdev)
{
	struct remote *remote = platform_get_drvdata(pdev);
	input_dbg("remote_resume To do remote resume\n");
	input_dbg("remote_resume make sure read frame enable ir interrupt\n");
	am_remote_read_reg(DURATION_REG1_AND_STATUS);
	am_remote_read_reg(FRAME_BODY);
	remote_restore_regs(remote->work_mode); /*restore remote regs*/
	if (is_meson_m8m2_cpu()) {
#define  AO_RTI_STATUS_REG2 ((0x00 << 10) | (0x02 << 2))
		if (aml_read_aobus(AO_RTI_STATUS_REG2) == 0x1234abcd) {
			/*aml_write_reg32(P_AO_RTC_ADDR0,
			(aml_read_reg32(P_AO_RTC_ADDR0) | (0x0000f000)));*/
			aml_write_aobus(AO_RTI_STATUS_REG2, 0);
		}
	} else {
		if (get_resume_method() == REMOTE_WAKEUP) {
			input_dbg("remote_wakeup\n");
		}
		if (get_resume_method() == ETH_PHY_WAKEUP) {
			input_dbg("ethernet_wakeup\n");
			input_event(gp_remote->input, EV_KEY, KEY_POWER, 1);
			input_sync(gp_remote->input);
			input_event(gp_remote->input, EV_KEY, KEY_POWER, 0);
			input_sync(gp_remote->input);
		}
	}

	gp_remote->sleep = 0;
	input_dbg("to clear irq ...\n");
	disable_irq(NEC_REMOTE_IRQ_NO);
	udelay(1000);
	enable_irq(NEC_REMOTE_IRQ_NO);

	return 0;
}

static int remote_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct remote *remote = platform_get_drvdata(pdev);
	input_dbg("remote_suspend, set sleep 1\n");
	remote_save_regs(remote->work_mode); /*save remote regs*/
	gp_remote->sleep = 1;
	return 0;
}


static struct platform_driver remote_driver = {
	.probe = remote_probe,
	.remove = remote_remove,
	.suspend = remote_suspend,
	.resume = remote_resume,
	.driver = {
		.name = "meson-remote",
		.of_match_table = remote_dt_match,
	},
};

static int __init remote_init(void)
{
	pr_info("remote: Driver init\n");
	return platform_driver_register(&remote_driver);
}

static void __exit remote_exit(void)
{
	input_dbg("remote: exit\n");
	platform_driver_unregister(&remote_driver);
}

module_init(remote_init);
module_exit(remote_exit);

MODULE_AUTHOR("Beijing-platform");
MODULE_DESCRIPTION("Remote Driver");
MODULE_LICENSE("GPL");
