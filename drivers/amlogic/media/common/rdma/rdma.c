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

#define RDMA_NUM  3
static int second_rdma_feature;
static int vsync_rdma_handle[RDMA_NUM];
static int irq_count[RDMA_NUM];
static int enable[RDMA_NUM];
static int cur_enable[RDMA_NUM];
static int pre_enable_[RDMA_NUM];
static int debug_flag[RDMA_NUM];
static int vsync_cfg_count[RDMA_NUM];
static u32 force_rdma_config[RDMA_NUM];
static bool first_config[RDMA_NUM];
static bool rdma_done[RDMA_NUM];

static void vsync_rdma_irq(void *arg);
static void vsync_rdma_vpp1_irq(void *arg);
static void vsync_rdma_vpp2_irq(void *arg);
static void line_n_int_rdma_irq(void *arg);
static void vsync_rdma_read_irq(void *arg);

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

struct rdma_op_s line_n_int_rdma_op = {
	line_n_int_rdma_irq,
	NULL
};

struct rdma_op_s vsync_rdma_read_op = {
	vsync_rdma_read_irq,
	NULL
};

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

void _vsync_rdma_config(int rdma_type)
{
	int iret = 0;
	int enable_ = cur_enable[rdma_type] & 0xf;

	if (vsync_rdma_handle[rdma_type] <= 0)
		return;

	/* first frame not use rdma */
	if (!first_config[rdma_type]) {
		cur_enable[rdma_type] = enable[rdma_type];
		pre_enable_[rdma_type] = enable_;
		first_config[rdma_type] = true;
		rdma_done[rdma_type] = false;
		return;
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
	if (force_rdma_config[rdma_type]) {
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
		if (!iret)
			force_rdma_config[rdma_type] = 1;
		else
			force_rdma_config[rdma_type] = 0;
	}
	pre_enable_[rdma_type] = enable_;
	cur_enable[rdma_type] = enable[rdma_type];
}

void vsync_rdma_config(void)
{
	_vsync_rdma_config(VSYNC_RDMA);
	if (!has_multi_vpp) {
		_vsync_rdma_config(VSYNC_RDMA_READ);
		if (second_rdma_feature &&
		    is_meson_g12b_revb())
			_vsync_rdma_config(LINE_N_INT_RDMA);
	}
}
EXPORT_SYMBOL(vsync_rdma_config);

void vsync_rdma_vpp1_config(void)
{
	_vsync_rdma_config(VSYNC_RDMA_VPP1);
}
EXPORT_SYMBOL(vsync_rdma_vpp1_config);

void vsync_rdma_vpp2_config(void)
{
	_vsync_rdma_config(VSYNC_RDMA_VPP2);
}
EXPORT_SYMBOL(vsync_rdma_vpp2_config);

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
	_vsync_rdma_config_pre(VSYNC_RDMA);
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
	int enable_ = cur_enable[VSYNC_RDMA] & 0xf;

	u32 read_val = Rd(adr);

	if (enable_ != 0 && vsync_rdma_handle[VSYNC_RDMA] > 0)
		read_val = rdma_read_reg(vsync_rdma_handle[VSYNC_RDMA], adr);

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

int VSYNC_WR_MPEG_REG(u32 adr, u32 val)
{
	int enable_ = cur_enable[VSYNC_RDMA] & 0xf;

	if (enable_ != 0 && vsync_rdma_handle[VSYNC_RDMA] > 0) {
		rdma_write_reg(vsync_rdma_handle[VSYNC_RDMA], adr, val);
	} else {
		Wr(adr, val);
		if (debug_flag[VSYNC_RDMA] & 1)
			pr_info("VSYNC_WR(%x)<=%x\n", adr, val);
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
			pr_info("VSYNC_VPP1_WR(%x)<=%x\n", adr, val);
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
			pr_info("VSYNC_VPP2_WR(%x)<=%x\n", adr, val);
	}
	return 0;
}
EXPORT_SYMBOL(VSYNC_WR_MPEG_REG_VPP2);

int VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
	int enable_ = cur_enable[VSYNC_RDMA] & 0xf;

	if (enable_ != 0 && vsync_rdma_handle[VSYNC_RDMA] > 0) {
		rdma_write_reg_bits(vsync_rdma_handle[VSYNC_RDMA],
				    adr, val, start, len);
	} else {
		u32 read_val = Rd(adr);
		u32 write_val = (read_val &
				 ~(((1L << (len)) - 1) << (start)))
			| ((unsigned int)(val) << (start));
		Wr(adr, write_val);
		if (debug_flag[VSYNC_RDMA] & 1)
			pr_info("VSYNC_WR(%x)<=%x\n", adr, write_val);
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
			pr_info("VSYNC_VPP1_WR(%x)<=%x\n", adr, write_val);
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
			pr_info("VSYNC_VPP2_WR(%x)<=%x\n", adr, write_val);
	}
	return 0;
}
EXPORT_SYMBOL(VSYNC_WR_MPEG_REG_BITS_VPP2);

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
				pr_info("VSYNC_WR(%x)<=%x\n", adr, val);
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
	int enable_ = cur_enable[VSYNC_RDMA] & 0xf;

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

void enable_rdma_log(int flag)
{
	if (flag) {
		debug_flag[VSYNC_RDMA] |= 0x1;
		if (has_multi_vpp) {
			debug_flag[VSYNC_RDMA_VPP1] |= 0x1;
			debug_flag[VSYNC_RDMA_VPP1] |= 0x1;
		} else {
			debug_flag[LINE_N_INT_RDMA] |= 0x1;
			debug_flag[VSYNC_RDMA_READ] |= 0x1;
		}
	} else {
		debug_flag[VSYNC_RDMA] &= (~0x1);
		if (has_multi_vpp) {
			debug_flag[VSYNC_RDMA_VPP1] &= (~0x1);
			debug_flag[VSYNC_RDMA_VPP1] &= (~0x1);
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
	if (has_multi_vpp) {
		enable[VSYNC_RDMA_VPP1] = enable_flag;
		enable[VSYNC_RDMA_VPP2] = enable_flag;
	} else {
		enable[LINE_N_INT_RDMA] = enable_flag;
		enable[VSYNC_RDMA_READ] = enable_flag;
	}
}
EXPORT_SYMBOL(enable_rdma);

struct rdma_op_s *get_rdma_ops(int rdma_type)
{
	if (has_multi_vpp) {
		if (rdma_type == VSYNC_RDMA)
			return &vsync_rdma_op;
		else if (rdma_type == VSYNC_RDMA_VPP1)
			return &vsync_rdma_vpp1_op;
		else if (rdma_type == VSYNC_RDMA_VPP2)
			return &vsync_rdma_vpp2_op;
		else
			return NULL;
	} else {
		if (rdma_type == VSYNC_RDMA)
			return &vsync_rdma_op;
		else if (rdma_type == LINE_N_INT_RDMA)
			return &line_n_int_rdma_op;
		else if (rdma_type == VSYNC_RDMA_READ)
			return &vsync_rdma_read_op;
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
	return snprintf(buf, PAGE_SIZE, "%d %d\n", enable[0], enable[1]);
}

static ssize_t enable_store(struct class *class,
			    struct class_attribute *attr,
			    const char *buf, size_t count)
{
	if (likely(parse_para(buf, 2, enable) == 2))
		pr_info("enalbe: %d, %d\n", enable[0], enable[1]);
	else
		pr_err("set enable error\n");
	return count;
}

static ssize_t show_irq_count(struct class *class,
			      struct class_attribute *attr,
			      char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d %d\n", irq_count[0], irq_count[1]);
}

static ssize_t store_irq_count(struct class *class,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	if (likely(parse_para(buf, 2, irq_count) == 2))
		pr_info("enalbe: %d, %d\n", irq_count[0], irq_count[1]);
	else
		pr_err("set irq_count error\n");
	return count;
}

static ssize_t show_debug_flag(struct class *class,
			       struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d %d\n",
			debug_flag[0], debug_flag[1]);
}

static ssize_t store_debug_flag(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	if (likely(parse_para(buf, 2, debug_flag) == 2))
		pr_info("debug_flag: %d, %d\n", debug_flag[0], debug_flag[1]);
	else
		pr_err("set debug_flag error\n");
	return count;
}

static ssize_t show_vsync_cfg_count(struct class *class,
				    struct class_attribute *attr,
				    char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d %d\n",
			vsync_cfg_count[0], vsync_cfg_count[1]);
}

static ssize_t store_vsync_cfg_count(struct class *class,
				     struct class_attribute *attr,
				     const char *buf, size_t count)
{
	if (likely(parse_para(buf, 2, vsync_cfg_count) == 2))
		pr_info("vsync_cfg_count: %d, %d\n",
			vsync_cfg_count[0], vsync_cfg_count[1]);
	else
		pr_err("set vsync_cfg_count error\n");
	return count;
}

static ssize_t show_force_rdma_config(struct class *class,
				      struct class_attribute *attr,
				      char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d %d\n",
			force_rdma_config[0], force_rdma_config[1]);
}

static ssize_t store_force_rdma_config(struct class *class,
				       struct class_attribute *attr,
				       const char *buf, size_t count)
{
	if (likely(parse_para(buf, 2, force_rdma_config) == 2))
		pr_info("force_rdma_config: %d, %d\n",
			force_rdma_config[0], force_rdma_config[1]);
	else
		pr_err("set force_rdma_config error\n");
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

	cur_enable[VSYNC_RDMA] = 0;
	enable[VSYNC_RDMA] = 1;
	force_rdma_config[VSYNC_RDMA] = 1;

	if (has_multi_vpp) {
		cur_enable[VSYNC_RDMA_VPP1] = 0;
		enable[VSYNC_RDMA_VPP1] = 1;
		force_rdma_config[VSYNC_RDMA_VPP1] = 1;

		cur_enable[VSYNC_RDMA_VPP1] = 0;
		enable[VSYNC_RDMA_VPP1] = 1;
		force_rdma_config[VSYNC_RDMA_VPP1] = 1;
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

