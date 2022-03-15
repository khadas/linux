// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define INFO_PREFIX "video_rdma"
#define pr_fmt(fmt) "rdma: " fmt

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/dma-mapping.h>

#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/ctype.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/clk.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include "rdma.h"
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/registers/register_map.h>
#include <linux/amlogic/media/rdma/rdma_mgr.h>

#define Wr(adr, val) WRITE_VCBUS_REG(adr, val)
#define Rd(adr)     READ_VCBUS_REG(adr)
#define Wr_reg_bits(adr, val, start, len) \
			WRITE_VCBUS_REG_BITS(adr, val, start, len)

#define RDMA_NUM  5
static int second_rdma_feature;
int vsync_rdma_handle[RDMA_NUM];
static int irq_count[RDMA_NUM];
static int enable[RDMA_NUM];
static int cur_enable[RDMA_NUM];
static int pre_enable_[RDMA_NUM];
static int debug_flag[RDMA_NUM];
static int vsync_cfg_count[RDMA_NUM];
static u32 force_rdma_config[RDMA_NUM];
static bool first_config[RDMA_NUM];
static bool rdma_done[RDMA_NUM];
static u32 cur_vsync_handle_id;
static int ex_vsync_rdma_enable;
static u32 rdma_reset;

static DEFINE_SPINLOCK(lock);
static void vsync_rdma_irq(void *arg);
static void vsync_rdma_vpp1_irq(void *arg);
static void vsync_rdma_vpp2_irq(void *arg);
static void pre_vsync_rdma_irq(void *arg);
static void line_n_int_rdma_irq(void *arg);
static void vsync_rdma_read_irq(void *arg);
static void ex_vsync_rdma_irq(void *arg);

struct rdma_op_s vsync_rdma_op = {
	vsync_rdma_irq,
	NULL
};

struct rdma_op_s vsync_rdma_vpp1_op = {
	vsync_rdma_vpp1_irq,
	NULL
};

struct rdma_op_s vsync_rdma_vpp2_op = {
	vsync_rdma_vpp2_irq,
	NULL
};

struct rdma_op_s pre_vsync_rdma_op = {
	pre_vsync_rdma_irq,
	NULL
};

struct rdma_op_s line_n_int_rdma_op = {
	line_n_int_rdma_irq,
	NULL
};

struct rdma_op_s vsync_rdma_read_op = {
	vsync_rdma_read_irq,
	NULL
};

struct rdma_op_s ex_vsync_rdma_op = {
	ex_vsync_rdma_irq,
	NULL
};

int get_ex_vsync_rdma_enable(void)
{
	return ex_vsync_rdma_enable;
}

void set_ex_vsync_rdma_enable(int enable)
{
	ex_vsync_rdma_enable = enable;
}

static void set_rdma_trigger_line(void)
{
	int trigger_line;

	switch (aml_read_vcbus(VPU_VIU_VENC_MUX_CTRL) & 0x3) {
	case 0:
		trigger_line = aml_read_vcbus(ENCL_VIDEO_VAVON_ELINE)
			- aml_read_vcbus(ENCL_VIDEO_VSO_BLINE);
		break;
	case 1:
		if ((aml_read_vcbus(ENCI_VIDEO_MODE) & 1) == 0)
			trigger_line = 260; /* 480i */
		else
			trigger_line = 310; /* 576i */
		break;
	case 2:
		if (aml_read_vcbus(ENCP_VIDEO_MODE) & (1 << 12))
			trigger_line = aml_read_vcbus(ENCP_DE_V_END_EVEN);
		else
			trigger_line = aml_read_vcbus(ENCP_VIDEO_VAVON_ELINE)
				- aml_read_vcbus(ENCP_VIDEO_VSO_BLINE);
		break;
	case 3:
		trigger_line = aml_read_vcbus(ENCT_VIDEO_VAVON_ELINE)
			- aml_read_vcbus(ENCT_VIDEO_VSO_BLINE);
		break;
	}
	aml_write_vcbus(VPP_INT_LINE_NUM, trigger_line);
}

void set_force_rdma_config(void)
{
	rdma_reset = 1;
}
EXPORT_SYMBOL(set_force_rdma_config);

int _vsync_rdma_config(int rdma_type)
{
	int iret = 0;
	int enable_ = cur_enable[rdma_type] & 0xf;
	unsigned long flags;

	if (vsync_rdma_handle[rdma_type] <= 0)
		return -1;

	/* first frame not use rdma */
	if (!first_config[rdma_type]) {
		cur_enable[rdma_type] = enable[rdma_type];
		pre_enable_[rdma_type] = enable_;
		first_config[rdma_type] = true;
		rdma_done[rdma_type] = false;
		return 0;
	}

	if (rdma_type == EX_VSYNC_RDMA) {
		spin_lock_irqsave(&lock, flags);
		force_rdma_config[rdma_type] = 1;
	}

	/* if rdma mode changed, reset rdma */
	if (pre_enable_[rdma_type] != enable_) {
		rdma_clear(vsync_rdma_handle[rdma_type]);
		force_rdma_config[rdma_type] = 1;
	}

	if (force_rdma_config[rdma_type])
		rdma_done[rdma_type] = true;

	if (enable_ == 1) {
		if (rdma_done[rdma_type])
			iret = rdma_watchdog_setting(0);
		else
			iret = rdma_watchdog_setting(1);
	} else {
		/* not vsync mode */
		iret = rdma_watchdog_setting(0);
		force_rdma_config[rdma_type] = 1;
	}
	rdma_done[rdma_type] = false;
	if (iret)
		force_rdma_config[rdma_type] = 1;

	iret = 0;
	if (force_rdma_config[rdma_type] || rdma_reset) {
		if (enable_ == 1) {
			if (has_multi_vpp) {
				if (rdma_type == VSYNC_RDMA) {
					iret = rdma_config(vsync_rdma_handle[rdma_type],
							   RDMA_TRIGGER_VSYNC_INPUT);
				} else if (rdma_type == VSYNC_RDMA_VPP1) {
					iret = rdma_config(vsync_rdma_handle[rdma_type],
							   RDMA_TRIGGER_VPP1_VSYNC_INPUT);
				} else if (rdma_type == VSYNC_RDMA_VPP2) {
					iret = rdma_config(vsync_rdma_handle[rdma_type],
							  RDMA_TRIGGER_VPP2_VSYNC_INPUT);
				} else if (rdma_type == PRE_VSYNC_RDMA) {
					iret = rdma_config(vsync_rdma_handle[rdma_type],
							  RDMA_TRIGGER_PRE_VSYNC_INPUT);
				} else if (rdma_type == EX_VSYNC_RDMA) {
					iret = rdma_config(vsync_rdma_handle[rdma_type],
						RDMA_TRIGGER_VSYNC_INPUT |
						RDMA_TRIGGER_OMIT_LOCK);
				}
			} else {
				if (rdma_type == VSYNC_RDMA) {
					iret = rdma_config(vsync_rdma_handle[rdma_type],
							   RDMA_TRIGGER_VSYNC_INPUT);
				} else if (rdma_type == LINE_N_INT_RDMA) {
					set_rdma_trigger_line();
					iret = rdma_config(vsync_rdma_handle[rdma_type],
							   RDMA_TRIGGER_LINE_INPUT);
				} else if (rdma_type == VSYNC_RDMA_READ) {
					iret = rdma_config(vsync_rdma_handle[rdma_type],
							   RDMA_TRIGGER_VSYNC_INPUT |
							   RDMA_READ_MASK);
				} else if (rdma_type == EX_VSYNC_RDMA) {
					iret = rdma_config(vsync_rdma_handle[rdma_type],
						RDMA_TRIGGER_VSYNC_INPUT |
						RDMA_TRIGGER_OMIT_LOCK);
				}
			}
			if (iret)
				vsync_cfg_count[rdma_type]++;
		} else if (enable_ == 2) {
			/*manually in cur vsync*/
			rdma_config(vsync_rdma_handle[rdma_type],
				    RDMA_TRIGGER_MANUAL);
		} else if (enable_ == 3) {
			;
		} else if (enable_ == 4) {
			rdma_config(vsync_rdma_handle[rdma_type],
				    RDMA_TRIGGER_DEBUG1); /*for debug*/
		} else if (enable_ == 5) {
			rdma_config(vsync_rdma_handle[rdma_type],
				    RDMA_TRIGGER_DEBUG2); /*for debug*/
		} else if (enable_ == 6) {
			;
		}
		if (!iret) {
			force_rdma_config[rdma_type] = 1;
			iret = 0;
		} else {
			force_rdma_config[rdma_type] = 0;
			iret = 0;
			if (rdma_reset) {
				rdma_reset = 0;
				iret = 1;
			}
		}
	}
	pre_enable_[rdma_type] = enable_;
	cur_enable[rdma_type] = enable[rdma_type];
	if (rdma_type == EX_VSYNC_RDMA)
		spin_unlock_irqrestore(&lock, flags);
	return iret;
}

int vsync_rdma_config(void)
{
	int ret;

	ret = _vsync_rdma_config(cur_vsync_handle_id);
	if (cur_vsync_handle_id == EX_VSYNC_RDMA &&
		 vsync_rdma_handle[cur_vsync_handle_id] > 0)
		rdma_buffer_unlock(vsync_rdma_handle[cur_vsync_handle_id]);

	if (!has_multi_vpp) {
		ret = _vsync_rdma_config(VSYNC_RDMA_READ);
		if (second_rdma_feature &&
		    is_meson_g12b_revb())
			ret = _vsync_rdma_config(LINE_N_INT_RDMA);
	}
	return ret;
}
EXPORT_SYMBOL(vsync_rdma_config);

int vsync_rdma_vpp1_config(void)
{
	return _vsync_rdma_config(VSYNC_RDMA_VPP1);
}
EXPORT_SYMBOL(vsync_rdma_vpp1_config);

int vsync_rdma_vpp2_config(void)
{
	return _vsync_rdma_config(VSYNC_RDMA_VPP2);
}
EXPORT_SYMBOL(vsync_rdma_vpp2_config);

int pre_vsync_rdma_config(void)
{
	return _vsync_rdma_config(PRE_VSYNC_RDMA);
}
EXPORT_SYMBOL(pre_vsync_rdma_config);
void _vsync_rdma_config_pre(int rdma_type)
{
	int enable_ = cur_enable[rdma_type] & 0xf;

	if (vsync_rdma_handle[rdma_type] == 0)
		return;
	if (enable_ == 3)/*manually in next vsync*/
		rdma_config(vsync_rdma_handle[rdma_type], 0);
	else if (enable_ == 6)
		rdma_config(vsync_rdma_handle[rdma_type], 0x101); /*for debug*/
}

void vsync_rdma_config_pre(void)
{
	if (cur_vsync_handle_id == EX_VSYNC_RDMA &&
		 vsync_rdma_handle[cur_vsync_handle_id] > 0)
		rdma_buffer_lock(vsync_rdma_handle[cur_vsync_handle_id]);
	_vsync_rdma_config_pre(cur_vsync_handle_id);
	if (!has_multi_vpp) {
		_vsync_rdma_config_pre(VSYNC_RDMA_READ);
		if (second_rdma_feature &&
		    is_meson_g12b_revb())
			_vsync_rdma_config_pre(LINE_N_INT_RDMA);
	}
}
EXPORT_SYMBOL(vsync_rdma_config_pre);

void vsync_rdma_vpp1_config_pre(void)
{
	_vsync_rdma_config_pre(VSYNC_RDMA_VPP1);
}
EXPORT_SYMBOL(vsync_rdma_vpp1_config_pre);

void vsync_rdma_vpp2_config_pre(void)
{
	_vsync_rdma_config_pre(VSYNC_RDMA_VPP2);
}
EXPORT_SYMBOL(vsync_rdma_vpp2_config_pre);

void pre_vsync_rdma_config_pre(void)
{
	_vsync_rdma_config_pre(PRE_VSYNC_RDMA);
}
EXPORT_SYMBOL(pre_vsync_rdma_config_pre);
static void vsync_rdma_irq(void *arg)
{
	int iret;
	int enable_ = cur_enable[VSYNC_RDMA] & 0xf;

	if (enable_ == 1) {
		/*triggered by next vsync*/
		iret = rdma_config(vsync_rdma_handle[VSYNC_RDMA],
				   RDMA_TRIGGER_VSYNC_INPUT);
		if (iret)
			vsync_cfg_count[VSYNC_RDMA]++;
	} else {
		iret = rdma_config(vsync_rdma_handle[VSYNC_RDMA], 0);
	}
	pre_enable_[VSYNC_RDMA] = enable_;

	if (!iret || enable_ != 1)
		force_rdma_config[VSYNC_RDMA] = 1;
	else
		force_rdma_config[VSYNC_RDMA] = 0;
	rdma_done[VSYNC_RDMA] = true;
	irq_count[VSYNC_RDMA]++;
}

static void vsync_rdma_vpp1_irq(void *arg)
{
	int iret;
	int enable_ = cur_enable[VSYNC_RDMA_VPP1] & 0xf;

	if (enable_ == 1) {
		/*triggered by next vsync*/
		iret = rdma_config(vsync_rdma_handle[VSYNC_RDMA_VPP1],
				   RDMA_TRIGGER_VPP1_VSYNC_INPUT);
		if (iret)
			vsync_cfg_count[VSYNC_RDMA_VPP1]++;
	} else {
		iret = rdma_config(vsync_rdma_handle[VSYNC_RDMA_VPP1], 0);
	}
	pre_enable_[VSYNC_RDMA_VPP1] = enable_;

	if (!iret || enable_ != 1)
		force_rdma_config[VSYNC_RDMA_VPP1] = 1;
	else
		force_rdma_config[VSYNC_RDMA_VPP1] = 0;
	rdma_done[VSYNC_RDMA_VPP1] = true;
	irq_count[VSYNC_RDMA_VPP1]++;
}

static void vsync_rdma_vpp2_irq(void *arg)
{
	int iret;
	int enable_ = cur_enable[VSYNC_RDMA_VPP2] & 0xf;

	if (enable_ == 1) {
		/*triggered by next vsync*/
		iret = rdma_config(vsync_rdma_handle[VSYNC_RDMA_VPP2],
				   RDMA_TRIGGER_VPP2_VSYNC_INPUT);
		if (iret)
			vsync_cfg_count[VSYNC_RDMA_VPP2]++;
	} else {
		iret = rdma_config(vsync_rdma_handle[VSYNC_RDMA_VPP2], 0);
	}
	pre_enable_[VSYNC_RDMA_VPP2] = enable_;

	if (!iret || enable_ != 1)
		force_rdma_config[VSYNC_RDMA_VPP2] = 1;
	else
		force_rdma_config[VSYNC_RDMA_VPP2] = 0;
	rdma_done[VSYNC_RDMA_VPP2] = true;
	irq_count[VSYNC_RDMA_VPP2]++;
}

static void pre_vsync_rdma_irq(void *arg)
{
	int iret;
	int enable_ = cur_enable[PRE_VSYNC_RDMA] & 0xf;

	if (enable_ == 1) {
		iret = rdma_config(vsync_rdma_handle[PRE_VSYNC_RDMA],
				   RDMA_TRIGGER_PRE_VSYNC_INPUT);
		if (iret)
			vsync_cfg_count[PRE_VSYNC_RDMA]++;
	} else {
		iret = rdma_config(vsync_rdma_handle[PRE_VSYNC_RDMA], 0);
	}
	pre_enable_[PRE_VSYNC_RDMA] = enable_;
	if (!iret || enable_ != 1)
		force_rdma_config[PRE_VSYNC_RDMA] = 1;
	else
		force_rdma_config[PRE_VSYNC_RDMA] = 0;
	rdma_done[PRE_VSYNC_RDMA] = true;
	irq_count[PRE_VSYNC_RDMA]++;
}

static void line_n_int_rdma_irq(void *arg)
{
	int iret;
	int enable_ = cur_enable[LINE_N_INT_RDMA] & 0xf;

	if (enable_ == 1) {
		/*triggered by next vsync*/
		//set_rdma_trigger_line();
		iret = rdma_config(vsync_rdma_handle[LINE_N_INT_RDMA],
				   RDMA_TRIGGER_LINE_INPUT);
		if (iret)
			vsync_cfg_count[LINE_N_INT_RDMA]++;
	} else {
		iret = rdma_config(vsync_rdma_handle[LINE_N_INT_RDMA], 0);
	}
	pre_enable_[LINE_N_INT_RDMA] = enable_;

	if (!iret || enable_ != 1)
		force_rdma_config[LINE_N_INT_RDMA] = 1;
	else
		force_rdma_config[LINE_N_INT_RDMA] = 0;
	rdma_done[LINE_N_INT_RDMA] = true;
	irq_count[LINE_N_INT_RDMA]++;
}

static void vsync_rdma_read_irq(void *arg)
{
	int iret;
	int enable_ = cur_enable[VSYNC_RDMA_READ] & 0xf;

	if (enable_ == 1) {
		/*triggered by next vsync*/
		iret = rdma_config(vsync_rdma_handle[VSYNC_RDMA_READ],
				   RDMA_TRIGGER_VSYNC_INPUT | RDMA_READ_MASK);
		if (iret)
			vsync_cfg_count[VSYNC_RDMA_READ]++;
	} else {
		iret = rdma_config(vsync_rdma_handle[VSYNC_RDMA_READ], 0);
	}
	pre_enable_[VSYNC_RDMA_READ] = enable_;

	if (!iret || enable_ != 1)
		force_rdma_config[VSYNC_RDMA_READ] = 1;
	else
		force_rdma_config[VSYNC_RDMA_READ] = 0;
	rdma_done[VSYNC_RDMA_READ] = true;
	irq_count[VSYNC_RDMA_READ]++;
}

static void ex_vsync_rdma_irq(void *arg)
{
	unsigned long flags;
	int iret;
	int enable_ = cur_enable[EX_VSYNC_RDMA] & 0xf;

	spin_lock_irqsave(&lock, flags);
	if (enable_ == 1) {
		/*triggered by next vsync*/
		iret = rdma_config(vsync_rdma_handle[EX_VSYNC_RDMA],
			RDMA_TRIGGER_VSYNC_INPUT);
		if (iret)
			vsync_cfg_count[EX_VSYNC_RDMA]++;
	} else {
		iret = rdma_config(vsync_rdma_handle[EX_VSYNC_RDMA], 0);
	}
	pre_enable_[EX_VSYNC_RDMA] = enable_;

	if (!iret || enable_ != 1)
		force_rdma_config[EX_VSYNC_RDMA] = 1;
	else
		force_rdma_config[EX_VSYNC_RDMA] = 0;
	rdma_done[EX_VSYNC_RDMA] = true;
	spin_unlock_irqrestore(&lock, flags);
	irq_count[EX_VSYNC_RDMA]++;
}

/* add a register addr to read list
 * success: return index, this index can be used in read-back table
 *   fail: return -1
 */
s32 VSYNC_ADD_RD_REG(u32 adr)
{
	int enable_ = cur_enable[VSYNC_RDMA_READ] & 0xf;
	int handle = vsync_rdma_handle[VSYNC_RDMA_READ];

	if (enable_ != 0 && handle > 0)
		return rdma_add_read_reg(handle, adr);

	pr_info("%s: VSYNC_RDMA_READ is diabled\n", __func__);
	return -1;
}
EXPORT_SYMBOL(VSYNC_ADD_RD_REG);

/* get read-back addr, this func should be invoked everytime before getting vals
 * success: return start addr of read-back
 *   fail: return NULL
 */
u32 *VSYNC_GET_RD_BACK_ADDR(void)
{
	int enable_ = cur_enable[VSYNC_RDMA_READ] & 0xf;
	int handle = vsync_rdma_handle[VSYNC_RDMA_READ];

	if (enable_ != 0 && handle > 0)
		return rdma_get_read_back_addr(handle);

	return NULL;
}
EXPORT_SYMBOL(VSYNC_GET_RD_BACK_ADDR);

u32 VSYNC_RD_MPEG_REG(u32 adr)
{
	int enable_ = cur_enable[cur_vsync_handle_id] & 0xf;

	u32 read_val = Rd(adr);

	if (enable_ != 0 && vsync_rdma_handle[cur_vsync_handle_id] > 0)
		read_val = rdma_read_reg(vsync_rdma_handle[cur_vsync_handle_id], adr);

	return read_val;
}
EXPORT_SYMBOL(VSYNC_RD_MPEG_REG);

u32 VSYNC_RD_MPEG_REG_VPP1(u32 adr)
{
	int enable_ = cur_enable[VSYNC_RDMA_VPP1] & 0xf;

	u32 read_val = Rd(adr);

	if (enable_ != 0 && vsync_rdma_handle[VSYNC_RDMA_VPP1] > 0)
		read_val = rdma_read_reg(vsync_rdma_handle[VSYNC_RDMA_VPP1], adr);

	return read_val;
}
EXPORT_SYMBOL(VSYNC_RD_MPEG_REG_VPP1);

u32 VSYNC_RD_MPEG_REG_VPP2(u32 adr)
{
	int enable_ = cur_enable[VSYNC_RDMA_VPP2] & 0xf;

	u32 read_val = Rd(adr);

	if (enable_ != 0 && vsync_rdma_handle[VSYNC_RDMA_VPP2] > 0)
		read_val = rdma_read_reg(vsync_rdma_handle[VSYNC_RDMA_VPP2], adr);

	return read_val;
}
EXPORT_SYMBOL(VSYNC_RD_MPEG_REG_VPP2);

u32 PRE_VSYNC_RD_MPEG_REG(u32 adr)
{
	int enable_ = cur_enable[PRE_VSYNC_RDMA] & 0xf;
	u32 read_val = Rd(adr);

	if (enable_ != 0 && vsync_rdma_handle[PRE_VSYNC_RDMA] > 0)
		read_val = rdma_read_reg(vsync_rdma_handle[PRE_VSYNC_RDMA], adr);
	return read_val;
}
EXPORT_SYMBOL(PRE_VSYNC_RD_MPEG_REG);
int VSYNC_WR_MPEG_REG(u32 adr, u32 val)
{
	int enable_ = cur_enable[cur_vsync_handle_id] & 0xf;

	if (enable_ != 0 && vsync_rdma_handle[cur_vsync_handle_id] > 0) {
		rdma_write_reg(vsync_rdma_handle[cur_vsync_handle_id], adr, val);
	} else {
		Wr(adr, val);
		if (debug_flag[cur_vsync_handle_id] & 1)
			pr_info("VSYNC_WR(%x)=%x\n", adr, val);
	}
	return 0;
}
EXPORT_SYMBOL(VSYNC_WR_MPEG_REG);

int VSYNC_WR_MPEG_REG_VPP1(u32 adr, u32 val)
{
	int enable_ = cur_enable[VSYNC_RDMA_VPP1] & 0xf;

	if (enable_ != 0 && vsync_rdma_handle[VSYNC_RDMA_VPP1] > 0) {
		rdma_write_reg(vsync_rdma_handle[VSYNC_RDMA_VPP1], adr, val);
	} else {
		Wr(adr, val);
		if (debug_flag[VSYNC_RDMA_VPP1] & 1)
			pr_info("VSYNC_VPP1_WR(%x)=%x\n", adr, val);
	}
	return 0;
}
EXPORT_SYMBOL(VSYNC_WR_MPEG_REG_VPP1);

int VSYNC_WR_MPEG_REG_VPP2(u32 adr, u32 val)
{
	int enable_ = cur_enable[VSYNC_RDMA_VPP2] & 0xf;

	if (enable_ != 0 && vsync_rdma_handle[VSYNC_RDMA_VPP2] > 0) {
		rdma_write_reg(vsync_rdma_handle[VSYNC_RDMA_VPP2], adr, val);
	} else {
		Wr(adr, val);
		if (debug_flag[VSYNC_RDMA_VPP2] & 1)
			pr_info("VSYNC_VPP2_WR(%x)=%x\n", adr, val);
	}
	return 0;
}
EXPORT_SYMBOL(VSYNC_WR_MPEG_REG_VPP2);
int PRE_VSYNC_WR_MPEG_REG(u32 adr, u32 val)
{
	int enable_ = cur_enable[PRE_VSYNC_RDMA] & 0xf;

	if (enable_ != 0 && vsync_rdma_handle[PRE_VSYNC_RDMA] > 0) {
		rdma_write_reg(vsync_rdma_handle[PRE_VSYNC_RDMA], adr, val);
	} else {
		Wr(adr, val);
		if (debug_flag[PRE_VSYNC_RDMA] & 1)
			pr_info("PRE_VSYNC_RDMA_WR(%x)=%x\n", adr, val);
	}
	return 0;
}
EXPORT_SYMBOL(PRE_VSYNC_WR_MPEG_REG);

int VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
	int enable_ = cur_enable[cur_vsync_handle_id] & 0xf;

	if (enable_ != 0 && vsync_rdma_handle[cur_vsync_handle_id] > 0) {
		rdma_write_reg_bits(vsync_rdma_handle[cur_vsync_handle_id],
				    adr, val, start, len);
	} else {
		u32 read_val = Rd(adr);
		u32 write_val = (read_val &
				 ~(((1L << (len)) - 1) << (start)))
			| ((unsigned int)(val) << (start));
		Wr(adr, write_val);
		if (debug_flag[cur_vsync_handle_id] & 1)
			pr_info("VSYNC_WR(%x)=%x\n", adr, write_val);
	}
	return 0;
}
EXPORT_SYMBOL(VSYNC_WR_MPEG_REG_BITS);

int VSYNC_WR_MPEG_REG_BITS_VPP1(u32 adr, u32 val, u32 start, u32 len)
{
	int enable_ = cur_enable[VSYNC_RDMA_VPP1] & 0xf;

	if (enable_ != 0 && vsync_rdma_handle[VSYNC_RDMA_VPP1] > 0) {
		rdma_write_reg_bits(vsync_rdma_handle[VSYNC_RDMA_VPP1],
				    adr, val, start, len);
	} else {
		u32 read_val = Rd(adr);
		u32 write_val = (read_val &
				 ~(((1L << (len)) - 1) << (start)))
			| ((unsigned int)(val) << (start));
		Wr(adr, write_val);
		if (debug_flag[VSYNC_RDMA_VPP1] & 1)
			pr_info("VSYNC_VPP1_WR(%x)=%x\n", adr, write_val);
	}
	return 0;
}
EXPORT_SYMBOL(VSYNC_WR_MPEG_REG_BITS_VPP1);

int VSYNC_WR_MPEG_REG_BITS_VPP2(u32 adr, u32 val, u32 start, u32 len)
{
	int enable_ = cur_enable[VSYNC_RDMA_VPP2] & 0xf;

	if (enable_ != 0 && vsync_rdma_handle[VSYNC_RDMA_VPP2] > 0) {
		rdma_write_reg_bits(vsync_rdma_handle[VSYNC_RDMA_VPP2],
				    adr, val, start, len);
	} else {
		u32 read_val = Rd(adr);
		u32 write_val = (read_val &
				 ~(((1L << (len)) - 1) << (start)))
			| ((unsigned int)(val) << (start));
		Wr(adr, write_val);
		if (debug_flag[VSYNC_RDMA_VPP2] & 1)
			pr_info("VSYNC_VPP2_WR(%x)=%x\n", adr, write_val);
	}
	return 0;
}
EXPORT_SYMBOL(VSYNC_WR_MPEG_REG_BITS_VPP2);
int PRE_VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
	int enable_ = cur_enable[PRE_VSYNC_RDMA] & 0xf;

	if (enable_ != 0 && vsync_rdma_handle[PRE_VSYNC_RDMA] > 0) {
		rdma_write_reg_bits(vsync_rdma_handle[PRE_VSYNC_RDMA],
				    adr, val, start, len);
	} else {
		u32 read_val = Rd(adr);
		u32 write_val = (read_val &
				 ~(((1L << (len)) - 1) << (start)))
			| ((unsigned int)(val) << (start));
		Wr(adr, write_val);
		if (debug_flag[PRE_VSYNC_RDMA] & 1)
			pr_info("PRE_VSYNC_VPP2_WR(%x)=%x\n", adr, write_val);
	}
	return 0;
}
EXPORT_SYMBOL(PRE_VSYNC_WR_MPEG_REG_BITS);

u32 _VSYNC_RD_MPEG_REG(u32 adr)
{
	u32 read_val = 0;

	if (second_rdma_feature && is_meson_g12b_revb()) {
		int enable_ = cur_enable[LINE_N_INT_RDMA] & 0xf;

		read_val = Rd(adr);

		if (enable_ != 0 &&
		    vsync_rdma_handle[LINE_N_INT_RDMA] > 0)
			read_val = rdma_read_reg
				(vsync_rdma_handle[LINE_N_INT_RDMA], adr);
	} else {
		read_val = VSYNC_RD_MPEG_REG(adr);
	}
	return read_val;
}
EXPORT_SYMBOL(_VSYNC_RD_MPEG_REG);

int _VSYNC_WR_MPEG_REG(u32 adr, u32 val)
{
	if (second_rdma_feature && is_meson_g12b_revb()) {
		int enable_ = cur_enable[LINE_N_INT_RDMA] & 0xf;

		if (enable_ != 0 &&
		    vsync_rdma_handle[LINE_N_INT_RDMA] > 0) {
			rdma_write_reg
				(vsync_rdma_handle[LINE_N_INT_RDMA], adr, val);
		} else {
			Wr(adr, val);
			if (debug_flag[LINE_N_INT_RDMA] & 1)
				pr_info("VSYNC_WR(%x)=%x\n", adr, val);
		}
	} else {
		VSYNC_WR_MPEG_REG(adr, val);
	}
	return 0;
}
EXPORT_SYMBOL(_VSYNC_WR_MPEG_REG);

int _VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
	if (second_rdma_feature && is_meson_g12b_revb()) {
		int enable_ = cur_enable[LINE_N_INT_RDMA] & 0xf;

		if (enable_ != 0 &&
		    vsync_rdma_handle[LINE_N_INT_RDMA] > 0) {
			rdma_write_reg_bits
				(vsync_rdma_handle[LINE_N_INT_RDMA],
				adr, val, start, len);
		} else {
			u32 read_val = Rd(adr);
			u32 write_val = (read_val &
					 ~(((1L << (len)) - 1) << (start)))
				| ((unsigned int)(val) << (start));
			Wr(adr, write_val);
			if (debug_flag[LINE_N_INT_RDMA] & 1)
				pr_info("VSYNC_WR(%x)<=%x\n", adr, write_val);
		}
	} else {
		VSYNC_WR_MPEG_REG_BITS(adr, val, start, len);
	}
	return 0;
}
EXPORT_SYMBOL(_VSYNC_WR_MPEG_REG_BITS);

bool is_vsync_rdma_enable(void)
{
	bool ret;
	int enable_ = cur_enable[cur_vsync_handle_id] & 0xf;

	ret = (enable_ != 0);
	return ret;
}
EXPORT_SYMBOL(is_vsync_rdma_enable);

bool is_vsync_vpp1_rdma_enable(void)
{
	bool ret;
	int enable_ = cur_enable[VSYNC_RDMA_VPP1] & 0xf;

	ret = (enable_ != 0);
	return ret;
}
EXPORT_SYMBOL(is_vsync_vpp1_rdma_enable);

bool is_vsync_vpp2_rdma_enable(void)
{
	bool ret;
	int enable_ = cur_enable[VSYNC_RDMA_VPP2] & 0xf;

	ret = (enable_ != 0);
	return ret;
}
EXPORT_SYMBOL(is_vsync_vpp2_rdma_enable);

bool is_pre_vsync_rdma_enable(void)
{
	bool ret;
	int enable_ = cur_enable[PRE_VSYNC_RDMA] & 0xf;

	ret = (enable_ != 0);
	return ret;
}
EXPORT_SYMBOL(is_pre_vsync_rdma_enable);
void enable_rdma_log(int flag)
{
	if (flag) {
		debug_flag[VSYNC_RDMA] |= 0x1;
		debug_flag[EX_VSYNC_RDMA] |= 0x1;
		if (has_multi_vpp) {
			debug_flag[VSYNC_RDMA_VPP1] |= 0x1;
			debug_flag[VSYNC_RDMA_VPP2] |= 0x1;
			debug_flag[PRE_VSYNC_RDMA] |= 0x1;
		} else {
			debug_flag[LINE_N_INT_RDMA] |= 0x1;
			debug_flag[VSYNC_RDMA_READ] |= 0x1;
		}
	} else {
		debug_flag[VSYNC_RDMA] &= (~0x1);
		debug_flag[EX_VSYNC_RDMA] &= (~0x1);
		if (has_multi_vpp) {
			debug_flag[VSYNC_RDMA_VPP1] &= (~0x1);
			debug_flag[VSYNC_RDMA_VPP2] &= (~0x1);
			debug_flag[PRE_VSYNC_RDMA] &= (~0x1);
		} else {
			debug_flag[LINE_N_INT_RDMA] &= (~0x1);
			debug_flag[VSYNC_RDMA_READ] &= (~0x1);
		}
	}
}
EXPORT_SYMBOL(enable_rdma_log);

void enable_rdma(int enable_flag)
{
	enable[VSYNC_RDMA] = enable_flag;
	enable[EX_VSYNC_RDMA] = enable_flag;
	if (has_multi_vpp) {
		enable[VSYNC_RDMA_VPP1] = enable_flag;
		enable[VSYNC_RDMA_VPP2] = enable_flag;
		enable[PRE_VSYNC_RDMA] = enable_flag;
	} else {
		enable[LINE_N_INT_RDMA] = enable_flag;
		enable[VSYNC_RDMA_READ] = enable_flag;
	}
}
EXPORT_SYMBOL(enable_rdma);

int set_vsync_rdma_id(u8 id)
{
	if (!ex_vsync_rdma_enable) {
		cur_vsync_handle_id = VSYNC_RDMA;
		return (id != cur_vsync_handle_id) ? -1 : 0;
	}

	if (id != VSYNC_RDMA && id != EX_VSYNC_RDMA)
		return -1;
	cur_vsync_handle_id = id;
	return 0;
}
EXPORT_SYMBOL(set_vsync_rdma_id);

struct rdma_op_s *get_rdma_ops(int rdma_type)
{
	if (has_multi_vpp) {
		if (rdma_type == VSYNC_RDMA)
			return &vsync_rdma_op;
		else if (rdma_type == VSYNC_RDMA_VPP1)
			return &vsync_rdma_vpp1_op;
		else if (rdma_type == VSYNC_RDMA_VPP2)
			return &vsync_rdma_vpp2_op;
		else if (rdma_type == PRE_VSYNC_RDMA)
			return &pre_vsync_rdma_op;
		else if (rdma_type == EX_VSYNC_RDMA)
			return &ex_vsync_rdma_op;
		else
			return NULL;
	} else {
		if (rdma_type == VSYNC_RDMA)
			return &vsync_rdma_op;
		else if (rdma_type == LINE_N_INT_RDMA)
			return &line_n_int_rdma_op;
		else if (rdma_type == VSYNC_RDMA_READ)
			return &vsync_rdma_read_op;
		else if (rdma_type == EX_VSYNC_RDMA)
			return &ex_vsync_rdma_op;
		else
			return NULL;
	}
}

void set_rdma_handle(int rdma_type, int handle)
{
	vsync_rdma_handle[rdma_type] = handle;
	pr_info("%s video rdma handle = %d.\n", __func__,
		vsync_rdma_handle[rdma_type]);
}

int get_rdma_handle(int rdma_type)
{
	return vsync_rdma_handle[rdma_type];
}

u32 is_line_n_rdma_enable(void)
{
	return second_rdma_feature;
}

static int parse_para(const char *para, int para_num, int *result)
{
	char *token = NULL;
	char *params, *params_base;
	int *out = result;
	int len = 0, count = 0;
	int res = 0;
	int ret = 0;

	if (!para)
		return 0;

	params = kstrdup(para, GFP_KERNEL);
	params_base = params;
	token = params;
	len = strlen(token);
	do {
		token = strsep(&params, " ");
		while (token && (isspace(*token) ||
				 !isgraph(*token)) && len) {
			token++;
			len--;
		}
		if (len == 0)
			break;
		ret = kstrtoint(token, 0, &res);
		if (ret < 0)
			break;
		len = strlen(token);
		*out++ = res;
		count++;
	} while ((token) && (count < para_num) && (len > 0));

	kfree(params_base);
	return count;
}

static ssize_t show_second_rdma_feature(struct class *class,
					struct class_attribute *attr,
					char *buf)
{
	return snprintf(buf, 40, "%d\n", second_rdma_feature);
}

static ssize_t store_second_rdma_feature(struct class *class,
					 struct class_attribute *attr,
					 const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	pr_info("second_rdma_feature: %d->%d\n", second_rdma_feature, res);
	second_rdma_feature = res;

	return count;
}

static ssize_t enable_show(struct class *class,
			   struct class_attribute *attr,
			   char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d %d %d %d\n",
			enable[0], enable[1],
			enable[2], enable[3]);
}

static ssize_t enable_store(struct class *class,
			    struct class_attribute *attr,
			    const char *buf, size_t count)
{
	int i = 0;

	if (likely(parse_para(buf, RDMA_NUM, enable) == RDMA_NUM)) {
		for (i = 0; i < RDMA_NUM; i++)
			pr_info("enalbe[%d]: %d\n", i, enable[i]);
	} else {
		pr_err("set enable error\n");
	}
	return count;
}

static ssize_t show_irq_count(struct class *class,
			      struct class_attribute *attr,
			      char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d %d %d %d\n",
			irq_count[0], irq_count[1],
			irq_count[2], irq_count[3]);
}

static ssize_t store_irq_count(struct class *class,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	int i = 0;

	if (likely(parse_para(buf, RDMA_NUM, irq_count) == RDMA_NUM)) {
		for (i = 0; i < RDMA_NUM; i++)
			pr_info("enalbe[%d]: %d\n", i, irq_count[i]);
	} else {
		pr_err("set irq_count error\n");
	}
	return count;
}

static ssize_t show_debug_flag(struct class *class,
			       struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d %d, %d, %d\n",
			debug_flag[0], debug_flag[1],
			debug_flag[2], debug_flag[3]);
}

static ssize_t store_debug_flag(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	int i = 0;

	if (likely(parse_para(buf, RDMA_NUM, debug_flag) == RDMA_NUM)) {
		for (i = 0; i < RDMA_NUM; i++)
			pr_info("debug_flag[%d]: %d\n", i, debug_flag[i]);
	} else {
		pr_err("set debug_flag error\n");
	}
	return count;
}

static ssize_t show_vsync_cfg_count(struct class *class,
				    struct class_attribute *attr,
				    char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d %d, %d, %d\n",
			vsync_cfg_count[0], vsync_cfg_count[1],
			vsync_cfg_count[2], vsync_cfg_count[3]);
}

static ssize_t store_vsync_cfg_count(struct class *class,
				     struct class_attribute *attr,
				     const char *buf, size_t count)
{
	int i = 0;

	if (likely(parse_para(buf, RDMA_NUM, vsync_cfg_count) == RDMA_NUM)) {
		for (i = 0; i < RDMA_NUM; i++)
			pr_info("vsync_cfg_count[%d]: %d\n",
				i, vsync_cfg_count[i]);
	} else {
		pr_err("set vsync_cfg_count error\n");
	}
	return count;
}

static ssize_t show_force_rdma_config(struct class *class,
				      struct class_attribute *attr,
				      char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d %d, %d, %d\n",
			force_rdma_config[0], force_rdma_config[1],
			force_rdma_config[2], force_rdma_config[3]);
}

static ssize_t store_force_rdma_config(struct class *class,
				       struct class_attribute *attr,
				       const char *buf, size_t count)
{
	int i = 0;

	if (likely(parse_para(buf, RDMA_NUM, force_rdma_config) == RDMA_NUM)) {
		for (i = 0; i < RDMA_NUM; i++)
			pr_info("force_rdma_config[%d]: %d\n",
				i, force_rdma_config[i]);
	} else {
		pr_err("set force_rdma_config error\n");
	}
	return count;
}

static struct class_attribute rdma_attrs[] = {
	__ATTR(second_rdma_feature, 0664,
	       show_second_rdma_feature, store_second_rdma_feature),
	__ATTR(enable, 0664,
	       enable_show, enable_store),
	__ATTR(irq_count, 0664,
	       show_irq_count, store_irq_count),
	__ATTR(debug_flag, 0664,
	       show_debug_flag, store_debug_flag),
	__ATTR(vsync_cfg_count, 0664,
	       show_vsync_cfg_count, store_vsync_cfg_count),
	__ATTR(force_rdma_config, 0664,
	       show_force_rdma_config, store_force_rdma_config),
};

static struct class *rdma_class;
static int create_rdma_class(void)
{
	int i;

	rdma_class = class_create(THIS_MODULE, "rdma");
	if (IS_ERR_OR_NULL(rdma_class)) {
		pr_err("create rdma_class failed\n");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(rdma_attrs); i++) {
		if (class_create_file(rdma_class,
				      &rdma_attrs[i])) {
			pr_err("create rdma attribute %s failed\n",
			       rdma_attrs[i].attr.name);
		}
	}
	return 0;
}

static int remove_rdma_class(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rdma_attrs); i++)
		class_remove_file(rdma_class, &rdma_attrs[i]);

	class_destroy(rdma_class);
	rdma_class = NULL;
	return 0;
}

int rdma_init(void)

{
	second_rdma_feature = 0;

	ex_vsync_rdma_enable = 1;
	cur_vsync_handle_id = VSYNC_RDMA;

	cur_enable[VSYNC_RDMA] = 0;
	enable[VSYNC_RDMA] = 1;
	force_rdma_config[VSYNC_RDMA] = 1;

	cur_enable[EX_VSYNC_RDMA] = 0;
	enable[EX_VSYNC_RDMA] = 1;
	force_rdma_config[EX_VSYNC_RDMA] = 1;

	if (has_multi_vpp) {
		cur_enable[VSYNC_RDMA_VPP1] = 0;
		enable[VSYNC_RDMA_VPP1] = 1;
		force_rdma_config[VSYNC_RDMA_VPP1] = 1;

		cur_enable[VSYNC_RDMA_VPP2] = 0;
		enable[VSYNC_RDMA_VPP2] = 1;
		force_rdma_config[VSYNC_RDMA_VPP2] = 1;
		cur_enable[PRE_VSYNC_RDMA] = 0;
		enable[PRE_VSYNC_RDMA] = 1;
		force_rdma_config[PRE_VSYNC_RDMA] = 1;
	} else {
		cur_enable[VSYNC_RDMA_READ] = 0;
		enable[VSYNC_RDMA_READ] = 1;
		force_rdma_config[VSYNC_RDMA_READ] = 1;

		if (second_rdma_feature) {
			cur_enable[LINE_N_INT_RDMA] = 0;
			enable[LINE_N_INT_RDMA] = 1;
			force_rdma_config[LINE_N_INT_RDMA] = 1;
		}
	}
	create_rdma_class();
	return 0;
}

void rdma_exit(void)
{
	remove_rdma_class();
}

