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
 * AMLOGIC demux driver.
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
#include <linux/delay.h>
#include <asm/cacheflush.h>
#include <linux/dma-mapping.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/vmalloc.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include "../../amports/streambuf.h"
#include "c_stb_define.h"
#include "c_stb_regs_define.h"
#include "aml_dvb.h"
#include "aml_dvb_reg.h"
#include <linux/sched/signal.h>


#define ENABLE_SEC_BUFF_WATCHDOG
#define USE_AHB_MODE
#define PR_ERROR_SPEED_LIMIT

#define pr_dbg_flag(_f, _args...)\
	do {\
		if (debug_dmx&(_f))\
			printk(_args);\
	} while (0)
#define pr_dbg_irq_flag(_f, _args...)\
	do {\
		if (debug_irq&(_f))\
			printk(_args);\
	} while (0)
#define pr_dbg(args...)	pr_dbg_flag(0x1, args)
#define pr_dbg_irq(args...)pr_dbg_irq_flag(0x1, args)
#define pr_dbg_irq_dvr(args...)pr_dbg_irq_flag(0x2, args)
#define pr_dbg_sf(args...) pr_dbg_flag(0x4, args)
#define pr_dbg_irq_sf(args...) pr_dbg_irq_flag(0x4, args)
#define pr_dbg_ss(args...) pr_dbg_flag(0x8, args)
#define pr_dbg_irq_ss(args...) pr_dbg_irq_flag(0x8, args)
#define pr_dbg_irq_pes(args...) pr_dbg_irq_flag(0x10, args)
#define pr_dbg_irq_sub(args...) pr_dbg_irq_flag(0x20, args)

#ifdef PR_ERROR_SPEED_LIMIT
static u32 last_pr_error_time;
#define pr_error(fmt, _args...)\
	do {\
		u32 diff = jiffies_to_msecs(jiffies - last_pr_error_time);\
		if (!last_pr_error_time || diff > 50) {\
			pr_err("DVB:" fmt, ## _args);\
			last_pr_error_time = jiffies;\
		} \
	} while (0)
#else
#define pr_error(fmt, args...) pr_err("DVB: " fmt, ## args)
#endif

#define pr_inf(fmt, args...)  printk("DVB: " fmt, ## args)

#define dump(b, l) \
	do { \
		int i; \
		printk("dump: "); \
		for (i = 0; i < (l); i++) {\
			if (!(i&0xf)) \
				printk("\n\t"); \
			printk("%02x ", *(((unsigned char *)(b))+i)); \
		} \
		printk("\n"); \
	} while (0)

MODULE_PARM_DESC(debug_dmx, "\n\t\t Enable demux debug information");
static int debug_dmx;
module_param(debug_dmx, int, 0644);

MODULE_PARM_DESC(debug_irq, "\n\t\t Enable demux IRQ debug information");
static int debug_irq;
module_param(debug_irq, int, 0644);

MODULE_PARM_DESC(disable_dsc, "\n\t\t Disable discrambler");
static int disable_dsc;
module_param(disable_dsc, int, 0644);

MODULE_PARM_DESC(enable_sec_monitor, "\n\t\t Enable sec monitor default is enable");
static int enable_sec_monitor = 2;
module_param(enable_sec_monitor, int, 0644);
/*For old version kernel */
#ifndef MESON_CPU_MAJOR_ID_GXL
#define MESON_CPU_MAJOR_ID_GXL	0x21
#endif

static int npidtypes = CHANNEL_COUNT;
#define MOD_PARAM_DECLARE_CHANPIDS_TYPES(_dmx) \
MODULE_PARM_DESC(debug_dmx##_dmx##_chanpids_types, "\n\t\t pids types of dmx channels"); \
static short debug_dmx##_dmx##_chanpids_types[CHANNEL_COUNT] = \
					{[0 ... (CHANNEL_COUNT - 1)] = -1}; \
module_param_array(debug_dmx##_dmx##_chanpids_types, short, &npidtypes, 0444)

MOD_PARAM_DECLARE_CHANPIDS_TYPES(0);
MOD_PARAM_DECLARE_CHANPIDS_TYPES(1);
MOD_PARAM_DECLARE_CHANPIDS_TYPES(2);

#define set_debug_dmx_chanpids_types(_dmx, _idx, _type)\
	do { \
		if ((_dmx) == 0) \
			debug_dmx0_chanpids_types[(_idx)] = (_type); \
		else if ((_dmx) == 1) \
			debug_dmx1_chanpids_types[(_idx)] = (_type); \
		else if ((_dmx) == 2) \
			debug_dmx2_chanpids_types[(_idx)] = (_type); \
	} while (0)


static int npids = CHANNEL_COUNT;
#define MOD_PARAM_DECLARE_CHANPIDS(_dmx) \
MODULE_PARM_DESC(debug_dmx##_dmx##_chanpids, "\n\t\t pids of dmx channels"); \
static short debug_dmx##_dmx##_chanpids[CHANNEL_COUNT] = \
					{[0 ... (CHANNEL_COUNT - 1)] = -1}; \
module_param_array(debug_dmx##_dmx##_chanpids, short, &npids, 0444)

#define CIPLUS_OUTPUT_AUTO 8
static int ciplus_out_sel = CIPLUS_OUTPUT_AUTO;
static int ciplus_out_auto_mode = 1;
static u32 ciplus = 0;
#define CIPLUS_OUT_SEL    28
#define CIPLUS_IN_SEL     26

MOD_PARAM_DECLARE_CHANPIDS(0);
MOD_PARAM_DECLARE_CHANPIDS(1);
MOD_PARAM_DECLARE_CHANPIDS(2);

#define set_debug_dmx_chanpids(_dmx, _idx, _pid)\
	do { \
		if ((_dmx) == 0) \
			debug_dmx0_chanpids[(_idx)] = (_pid); \
		else if ((_dmx) == 1) \
			debug_dmx1_chanpids[(_idx)] = (_pid); \
		else if ((_dmx) == 2) \
			debug_dmx2_chanpids[(_idx)] = (_pid); \
		if (_pid == -1) \
			set_debug_dmx_chanpids_types(_dmx, _idx, -1); \
	} while (0)

MODULE_PARM_DESC(debug_sf_user, "\n\t\t only for sf mode check");
static int debug_sf_user;
module_param(debug_sf_user, int, 0444);

MODULE_PARM_DESC(force_sec_sf, "\n\t\t force sf mode for sec filter");
static int force_sec_sf;
module_param(force_sec_sf, int, 0644);

MODULE_PARM_DESC(force_pes_sf, "\n\t\t force sf mode for pes filter");
static int force_pes_sf;
module_param(force_pes_sf, int, 0644);

MODULE_PARM_DESC(use_of_sop, "\n\t\t Enable use of sop input");
static int use_of_sop;
module_param(use_of_sop, int, 0644);

/*
  As the default value of unused channel's PID_TYPE is 0x7,
  if we use PID_TYPE(RECORDER_STREAM:0x7) for recording channel,
  the data with the pid which assigned in unused channel's setting will be captured also.
  To avoid the high bitrate(exists) of this pid's data flood the buffers in the data path,
  which will causes the record data corruption, and bad picture decoded.
*/
MODULE_PARM_DESC(g_chan_def_pid, "\n\t\t default pid for unused channel");
static int g_chan_def_pid = 0x1FFE;
module_param(g_chan_def_pid, int, 0644);


static u32 old_stb_top_config;
static u32 old_fec_input_control;
static int have_old_stb_top_config = 1;
static int have_old_fec_input_control = 1;
static long pes_off_pre[DMX_DEV_COUNT];

static void
dmx_write_reg(int r, u32 v)
{
	u32 oldv, mask;

	if (disable_dsc) {
		if (r == STB_TOP_CONFIG) {
			if (have_old_stb_top_config) {
				oldv = old_stb_top_config;
				have_old_stb_top_config = 0;
			} else {
				oldv = READ_MPEG_REG(STB_TOP_CONFIG);
			}

			mask = (1<<7)|(1<<15)|(3<<26)|(7<<28);
			v    &= ~mask;
			v    |= (oldv & mask);
		} else if (r == FEC_INPUT_CONTROL) {
			if (have_old_fec_input_control) {
				oldv = old_fec_input_control;
				have_old_fec_input_control = 0;
			} else {
				oldv = READ_MPEG_REG(FEC_INPUT_CONTROL);
			}

			mask = (1<<15);
			v   &= ~mask;
			v   |= (oldv & mask);
		} else if ((r == RESET1_REGISTER) || (r == RESET3_REGISTER)) {
			if (!have_old_stb_top_config) {
				have_old_stb_top_config = 1;
				old_stb_top_config =
					READ_MPEG_REG(STB_TOP_CONFIG);
			}
			if (!have_old_fec_input_control) {
				have_old_fec_input_control = 1;
				old_fec_input_control =
					READ_MPEG_REG(FEC_INPUT_CONTROL);
			}
		} else if ((r == TS_PL_PID_INDEX) || (r == TS_PL_PID_DATA)
					|| (r == COMM_DESC_KEY0)
					|| (r == COMM_DESC_KEY1)
					|| (r == COMM_DESC_KEY_RW)
					|| (r == CIPLUS_KEY0)
					|| (r == CIPLUS_KEY1)
					|| (r == CIPLUS_KEY2)
					|| (r == CIPLUS_KEY3)
					|| (r == CIPLUS_KEY_WR)
					|| (r == CIPLUS_CONFIG)
					|| (r == CIPLUS_ENDIAN)) {
			return;
		}
	}
	WRITE_MPEG_REG(r, v);
}

#undef WRITE_MPEG_REG
#define WRITE_MPEG_REG(r, v) dmx_write_reg(r, v)

#define DMX_READ_REG(i, r)\
	((i)?((i == 1)?READ_MPEG_REG(r##_2) :\
	READ_MPEG_REG(r##_3)) : READ_MPEG_REG(r))

#define DMX_WRITE_REG(i, r, d)\
	do {\
		if (i == 1) {\
			WRITE_MPEG_REG(r##_2, d);\
		} else if (i == 2) {\
			WRITE_MPEG_REG(r##_3, d);\
		} \
		else {\
			WRITE_MPEG_REG(r, d);\
		} \
	} while (0)

#define READ_PERI_REG			READ_CBUS_REG
#define WRITE_PERI_REG			WRITE_CBUS_REG

#define READ_ASYNC_FIFO_REG(i, r)\
	((i) ? ((i-1)?READ_PERI_REG(ASYNC_FIFO1_##r):\
	READ_PERI_REG(ASYNC_FIFO2_##r)) : READ_PERI_REG(ASYNC_FIFO_##r))

#define WRITE_ASYNC_FIFO_REG(i, r, d)\
	do {\
		if (i == 2) {\
			WRITE_PERI_REG(ASYNC_FIFO1_##r, d);\
		} else if (i == 0) {\
			WRITE_PERI_REG(ASYNC_FIFO_##r, d);\
		} else {\
			WRITE_PERI_REG(ASYNC_FIFO2_##r, d);\
		} \
	} while (0)

#define CLEAR_ASYNC_FIFO_REG_MASK(i, reg, mask) \
	WRITE_ASYNC_FIFO_REG(i, reg, \
	(READ_ASYNC_FIFO_REG(i, reg)&(~(mask))))

#define DVR_FEED(f) \
	((f) && ((f)->type == DMX_TYPE_TS) &&	\
	(((f)->ts_type & (TS_PACKET | TS_DEMUX)) == TS_PACKET))

#define MOD_PARAM_DECLARE_CHANREC(_dmx) \
MODULE_PARM_DESC(dmx##_dmx##_chanrec_enable, \
	       "\n\t\t record by channel, one time use in the beginning"); \
static int dmx##_dmx##_chanrec_enable; \
module_param(dmx##_dmx##_chanrec_enable, int, 0644); \
MODULE_PARM_DESC(dmx##_dmx##_chanrec, "\n\t\t record channels bits"); \
static int dmx##_dmx##_chanrec; \
module_param(dmx##_dmx##_chanrec, int, 0644)

MOD_PARAM_DECLARE_CHANREC(0);
MOD_PARAM_DECLARE_CHANREC(1);
MOD_PARAM_DECLARE_CHANREC(2);

#define MOD_PARAM_DECLARE_CHANPROC(_dmx) \
MODULE_PARM_DESC(dmx##_dmx##_chanproc_enable, "channel further processing"); \
static int dmx##_dmx##_chanproc_enable; \
module_param(dmx##_dmx##_chanproc_enable, int, 0644); \
MODULE_PARM_DESC(dmx##_dmx##_chanproc, "further process channels bits"); \
static int dmx##_dmx##_chanproc; \
module_param(dmx##_dmx##_chanproc, int, 0644)

MOD_PARAM_DECLARE_CHANPROC(0);
MOD_PARAM_DECLARE_CHANPROC(1);
MOD_PARAM_DECLARE_CHANPROC(2);

#define DMX_CH_OP_CHANREC  0
#define DMX_CH_OP_CHANPROC 1

static inline int _setbit(int v, int b) { return v|(1<<b); }
static inline int _clrbit(int v, int b) { return v&~(1<<b); }
static inline int _set(int v, int b) { return b; }

static int dsc_set_csa_key(struct aml_dsc_channel *ch, int flags,
			enum ca_cw_type type, u8 *key);
static int dsc_set_aes_des_sm4_key(struct aml_dsc_channel *ch, int flags,
			enum ca_cw_type type, u8 *key);
static void aml_ci_plus_disable(void);
static void am_ci_plus_set_output(struct aml_dsc_channel *ch);
static int set_subtitle_pes_buffer(struct aml_dmx *dmx);

static void dmxn_op_chan(int dmx, int ch, int(*op)(int, int), int ch_op)
{
	int enable_0, enable_1, enable_2;
	int *set_0, *set_1, *set_2;
	int reg;

	if (ch_op == DMX_CH_OP_CHANREC) {
		enable_0 = dmx0_chanrec_enable;
		enable_1 = dmx1_chanrec_enable;
		enable_2 = dmx2_chanrec_enable;
		set_0 = &dmx0_chanrec;
		set_1 = &dmx1_chanrec;
		set_2 = &dmx2_chanrec;
		reg = DEMUX_CHAN_RECORD_EN;
	} else if (ch_op == DMX_CH_OP_CHANPROC) {
		enable_0 = dmx0_chanproc_enable;
		enable_1 = dmx1_chanproc_enable;
		enable_2 = dmx2_chanproc_enable;
		set_0 = &dmx0_chanproc;
		set_1 = &dmx1_chanproc;
		set_2 = &dmx2_chanproc;
		reg = DEMUX_CHAN_PROCESS_EN;
	} else {
		return;
	}
	if (dmx == 0) {
		if (enable_0) {
			*set_0 = op(*set_0, ch);
			WRITE_MPEG_REG(reg+DEMUX_1_OFFSET, *set_0);
		}
	} else if (dmx == 1) {
		if (enable_1) {
			*set_1 = op(*set_1, ch);
			WRITE_MPEG_REG(reg+DEMUX_2_OFFSET, *set_1);
		}
	} else if (dmx == 2) {
		if (enable_2) {
			*set_2 = op(*set_2, ch);
			WRITE_MPEG_REG(reg+DEMUX_3_OFFSET, *set_2);
		}
	}
}
#define dmx_add_recchan(_dmx, _chid) \
	do { \
		pr_dbg("dmx[%d]_add_recchan[%d]\n", _dmx, _chid); \
		dmxn_op_chan(_dmx, _chid, _setbit, DMX_CH_OP_CHANREC); \
	} while (0)
#define dmx_rm_recchan(_dmx, _chid) \
	do { \
		pr_dbg("dmx[%d]_rm_recchan[%ld]\n", _dmx, _chid); \
		dmxn_op_chan(_dmx, _chid, _clrbit, DMX_CH_OP_CHANREC); \
	} while (0)
#define dmx_set_recchan(_dmx, _chs) \
	do { \
		pr_dbg("dmx[%d]_set_recchan[%d]\n", _dmx, _chs); \
		dmxn_op_chan(_dmx, _chs, _set, DMX_CH_OP_CHANREC); \
	} while (0)

#define dmx_add_procchan(_dmx, _chid) \
	do { \
		pr_dbg("dmx[%d]_add_procchan[%d]\n", _dmx, _chid); \
		dmxn_op_chan(_dmx, _chid, _setbit, DMX_CH_OP_CHANPROC); \
	} while (0)
#define dmx_rm_procchan(_dmx, _chid) \
	do { \
		pr_dbg("dmx[%d]_rm_procchan[%ld]\n", _dmx, _chid); \
		dmxn_op_chan(_dmx, _chid, _clrbit, DMX_CH_OP_CHANPROC); \
	} while (0)
#define dmx_set_procchan(_dmx, _chs) \
	do { \
		pr_dbg("dmx[%d]_set_procchan[%d]\n", _dmx, _chs); \
		dmxn_op_chan(_dmx, _chs, _set, DMX_CH_OP_CHANPROC); \
	} while (0)

#define NO_SUB
#define SUB_BUF_DMX
#define SUB_PARSER

#ifndef SUB_BUF_DMX
#undef SUB_PARSER
#endif

#define SUB_BUF_SHARED
#define PES_BUF_SHARED

#define SYS_CHAN_COUNT    (4)
#define SEC_GRP_LEN_0     (0xc)
#define SEC_GRP_LEN_1     (0xc)
#define SEC_GRP_LEN_2     (0xc)
#define SEC_GRP_LEN_3     (0xc)
#define LARGE_SEC_BUFF_MASK  0xFFFFFFFF
#define LARGE_SEC_BUFF_COUNT 32
#define WATCHDOG_TIMER    250
#define ASYNCFIFO_BUFFER_SIZE_DEFAULT (512*1024)

#define DEMUX_INT_MASK\
			((0<<(AUDIO_SPLICING_POINT))    |\
			(0<<(VIDEO_SPLICING_POINT))     |\
			(1<<(OTHER_PES_READY))          |\
			(1<<(PCR_READY))                |\
			(1<<(SUB_PES_READY))            |\
			(1<<(SECTION_BUFFER_READY))     |\
			(0<<(OM_CMD_READ_PENDING))      |\
			(1<<(TS_ERROR_PIN))             |\
			(1<<(NEW_PDTS_READY))           |\
			(0<<(DUPLICATED_PACKET))        |\
			(0<<(DIS_CONTINUITY_PACKET)))

#define TS_SRC_MAX 3

/*Reset the demux device*/
#define RESET_DEMUX2      (1<<15)
#define RESET_DEMUX1      (1<<14)
#define RESET_DEMUX0      (1<<13)
#define RESET_S2P1        (1<<12)
#define RESET_S2P0        (1<<11)
#define RESET_DES         (1<<10)
#define RESET_TOP         (1<<9)

static int dmx_remove_feed(struct aml_dmx *dmx, struct dvb_demux_feed *feed);
static void reset_async_fifos(struct aml_dvb *dvb);
static int dmx_add_feed(struct aml_dmx *dmx, struct dvb_demux_feed *feed);
static int dmx_smallsec_set(struct aml_smallsec *ss, int enable, int bufsize,
				int force);
static int dmx_timeout_set(struct aml_dmxtimeout *dto, int enable,
				int timeout, int ch_dis, int nomatch,
				int force);

/*Audio & Video PTS value*/
static u32 video_pts = 0;
static u32 audio_pts = 0;
static u32 video_pts_bit32 = 0;
static u32 audio_pts_bit32 = 0;
static u32 first_video_pts = 0;
static u32 first_audio_pts = 0;
static int demux_skipbyte;
static int tsfile_clkdiv = 5;
static int asyncfifo_buf_len = ASYNCFIFO_BUFFER_SIZE_DEFAULT;

#define SF_DMX_ID 2
#define SF_AFIFO_ID 1

#define sf_dmx_sf(_dmx) \
	(((_dmx)->id == SF_DMX_ID) \
	&& ((struct aml_dvb *)(_dmx)->demux.priv)->swfilter.user)
#define sf_afifo_sf(_afifo) \
	(((_afifo)->id == SF_AFIFO_ID) && (_afifo)->dvb->swfilter.user)
#define dmx_get_dev(dmx) (((struct aml_dvb *)((dmx)->demux.priv))->dev)
#define asyncfifo_get_dev(afifo) ((afifo)->dvb->dev)


int dmx_phyreg_access(unsigned int reg, unsigned int writeval,
		   unsigned int *readval)
{
	void __iomem *vaddr;

	reg = round_down(reg, 0x3);
	vaddr = ioremap(reg, 0x4);
	if (!vaddr)
		return -ENOMEM;

	if (readval)
		*readval = readl_relaxed(vaddr);
	else
		writel_relaxed(writeval, vaddr);
	iounmap(vaddr);
	return 0;
}

/*Section buffer watchdog*/
static void section_buffer_watchdog_func(struct timer_list * timer)
{
	struct aml_dvb *dvb = from_timer(dvb,timer,watchdog_timer);
	struct aml_dmx *dmx;
	u32 section_busy32 = 0, om_cmd_status32 = 0,
	    demux_channel_activity32 = 0;
	u16 demux_int_status1 = 0;
	u32 device_no = 0;
	u32 filter_number = 0;
	u32 i = 0;
	unsigned long flags;

	spin_lock_irqsave(&dvb->slock, flags);

	for (device_no = 0; device_no < DMX_DEV_COUNT; device_no++) {

		dmx = &dvb->dmx[device_no];

		if (dvb->dmx_watchdog_disable[device_no])
			continue;

		if (!dmx->init)
			continue;

		om_cmd_status32 =
		    DMX_READ_REG(device_no, OM_CMD_STATUS);
		demux_channel_activity32 =
		    DMX_READ_REG(device_no, DEMUX_CHANNEL_ACTIVITY);
		section_busy32 =
			DMX_READ_REG(device_no, SEC_BUFF_BUSY);

		if (om_cmd_status32 & 0x8fc2) {
			/* bit 15:12 -- om_cmd_count (read only) */
			/* bit  11:9 -- overflow_count */
			/* bit  11:9 --       om_cmd_wr_ptr(read only) */
			/* bit   8:6 -- om_overwrite_count */
			/* bit   8:6 --       om_cmd_rd_ptr(read only) */
			/* bit   5:3 -- type_stb_om_w_rd(read only) */
			/* bit     2 -- unit_start_stb_om_w_rd(read only) */
			/* bit     1 -- om_cmd_overflow(read only) */
			/* bit     0 -- om_cmd_pending(read) */
			/* bit     0 -- om_cmd_read_finished(write) */
			/*BUG: If the recoder is running, return */
			if (!dmx->record) {
				/* OM status is wrong */
				dmx->om_status_error_count++;
				pr_dbg("demux om status \n"
				"%04x\t%03x\t%03x\t%03x\t%01x\t%01x\t"
				"%x\t%x\tdmx%d:status:0x%xerr_cnt:%d-%d\n",
				(om_cmd_status32 >> 12) & 0xf,
				(om_cmd_status32 >> 9) & 0x7,
				(om_cmd_status32 >> 6) & 0x7,
				(om_cmd_status32 >> 3) & 0x7,
				(om_cmd_status32 >> 2) & 0x1,
				(om_cmd_status32 >> 1) & 0x1,
				demux_channel_activity32, section_busy32,
				dmx->id, om_cmd_status32, dmx->om_status_error_count, enable_sec_monitor);
				if (enable_sec_monitor &&
						dmx->om_status_error_count > enable_sec_monitor) {
					/*Reset the demux */
					dmx_reset_dmx_hw_ex_unlock(dvb, dmx, 0);
					/* Reset the error count */
					dmx->om_status_error_count = 0;
					goto end;
				}
			}
		} else {
			/* OM status is correct, reset the error count */
			dmx->om_status_error_count = 0;
		}
		section_busy32 =
			DMX_READ_REG(device_no, SEC_BUFF_BUSY);
		if (LARGE_SEC_BUFF_MASK ==
				(section_busy32 & LARGE_SEC_BUFF_MASK)) {
			/*All the largest section buffers occupied,
			 * clear buffers
			 */
			DMX_WRITE_REG(device_no,
					SEC_BUFF_READY, section_busy32);
		} else {
			for (i = 0; i < SEC_BUF_COUNT; i++) {
				if (!(section_busy32 & (1 << i)))
					continue;
				DMX_WRITE_REG(device_no, SEC_BUFF_NUMBER, i);
				filter_number =	DMX_READ_REG(device_no,
							SEC_BUFF_NUMBER);
				filter_number >>= 8;
				if ((filter_number >= FILTER_COUNT)
					/* >=31, do not handle this case */
					|| ((filter_number < FILTER_COUNT)
					&& dmx->filter[filter_number].used))
					section_busy32 &= ~(1 << i);
			}
			if (section_busy32 & (dmx->smallsec.enable ?
						0x7FFFFFFF :
						LARGE_SEC_BUFF_MASK)) {
				/*Clear invalid buffers */
				DMX_WRITE_REG(device_no,
						SEC_BUFF_READY,
						section_busy32);
				pr_error("clear invalid buffer 0x%x\n",
						section_busy32);
			}
		}
		demux_int_status1 =
			DMX_READ_REG(device_no, STB_INT_STATUS) & 0xfff7;
		if (demux_int_status1 & (1 << TS_ERROR_PIN)) {
			DMX_WRITE_REG(device_no,
				STB_INT_STATUS,
				(1 << TS_ERROR_PIN));
		}
	}

end:
	spin_unlock_irqrestore(&dvb->slock, flags);
#ifdef ENABLE_SEC_BUFF_WATCHDOG
	mod_timer(&dvb->watchdog_timer,
		  jiffies + msecs_to_jiffies(WATCHDOG_TIMER));
#endif
}

static inline int sec_filter_match(struct aml_dmx *dmx, struct aml_filter *f,
				   u8 *p)
{
	int b;
	u8 neq = 0;

	if (!f->used || !dmx->channel[f->chan_id].used)
		return 0;

	for (b = 0; b < FILTER_LEN; b++) {
		u8 xor = p[b] ^ f->value[b];

		if (xor & f->maskandmode[b])
			return 0;

		if (xor & f->maskandnotmode[b])
			neq = 1;
	}

	if (f->neq && !neq)
		return 0;

	return 1;
}

static void trigger_crc_monitor(struct aml_dmx *dmx)
{
	if (!dmx->crc_check_time) {
		dmx->crc_check_time = jiffies;
		dmx->crc_check_count = 0;
	}

	if (dmx->crc_check_count > 100) {
		if (jiffies_to_msecs(jiffies - dmx->crc_check_time) <= 1000) {
			struct aml_dvb *dvb = (struct aml_dvb *)dmx->demux.priv;

			pr_error("Too many crc fail (%d crc fail in %d ms)!\n",
				dmx->crc_check_count,
				jiffies_to_msecs(jiffies - dmx->crc_check_time)
			);
			dmx_reset_dmx_hw_ex_unlock(dvb, dmx, 0);
		}
		dmx->crc_check_time = 0;
	}

	dmx->crc_check_count++;
}
static int section_crc(struct aml_dmx *dmx, struct aml_filter *f, u8 *p)
{
	int sec_len = (((p[1] & 0xF) << 8) | p[2]) + 3;
	struct dvb_demux_feed *feed = dmx->channel[f->chan_id].feed;

	if (feed->feed.sec.check_crc) {
		struct dvb_demux *demux = feed->demux;
		struct dmx_section_feed *sec = &feed->feed.sec;
		int section_syntax_indicator;

		section_syntax_indicator = ((p[1] & 0x80) != 0);
		sec->seclen = sec_len;
		sec->crc_val = ~0;
		if (demux->check_crc32(feed, p, sec_len)) {
			pr_error("section CRC check failed! pid[%d]\n", feed->pid);
			trigger_crc_monitor(dmx);
			return 0;
		}
	}

	return 1;
}

static void section_notify(struct aml_dmx *dmx, struct aml_filter *f, u8 *p)
{
	int sec_len = (((p[1] & 0xF) << 8) | p[2]) + 3;
	struct dvb_demux_feed *feed = dmx->channel[f->chan_id].feed;

	if (feed && feed->cb.sec)
		feed->cb.sec(p, sec_len, NULL, 0, f->filter,0);
}

static void hardware_match_section(struct aml_dmx *dmx,
						u16 sec_num, u16 buf_num)
{
	u8 *p = (u8 *) dmx->sec_buf[buf_num].addr;
	struct aml_filter *f;
	int chid, i;
	int need_crc = 1;

	if (sec_num >= FILTER_COUNT) {
		pr_dbg("sec_num invalid: %d\n", sec_num);
		return;
	}

	dma_sync_single_for_cpu(dmx_get_dev(dmx),
				dmx->sec_pages_map + (buf_num << 0x0c),
				(1 << 0x0c), DMA_FROM_DEVICE);

	f = &dmx->filter[sec_num];
	chid = f->chan_id;

	dmx->sec_cnt[SEC_CNT_HW]++;

	for (i = 0; i < FILTER_COUNT; i++) {
		f = &dmx->filter[i];
		if (f->chan_id != chid)
			continue;
		if (sec_filter_match(dmx, f, p)) {
			if (need_crc) {
				dmx->sec_cnt_match[SEC_CNT_HW]++;
				if (!section_crc(dmx, f, p)) {
					dmx->sec_cnt_crc_fail[SEC_CNT_HW]++;
					return;
				}
				need_crc = 0;
			}
			section_notify(dmx, f, p);
		}
	}
}

static void software_match_section(struct aml_dmx *dmx, u16 buf_num)
{
	u8 *p = (u8 *) dmx->sec_buf[buf_num].addr;
	struct aml_filter *f, *fmatch = NULL;
	int i, fid = -1;

	dma_sync_single_for_cpu(dmx_get_dev(dmx),
				dmx->sec_pages_map + (buf_num << 0x0c),
				(1 << 0x0c), DMA_FROM_DEVICE);

	dmx->sec_cnt[SEC_CNT_SW]++;

	for (i = 0; i < FILTER_COUNT; i++) {
		f = &dmx->filter[i];

		if (sec_filter_match(dmx, f, p)) {
			pr_dbg("[software match]filter %d match, pid %d\n",
			       i, dmx->channel[f->chan_id].pid);
			if (!fmatch) {
				fmatch = f;
				fid = i;
			} else {
				pr_error("[sw match]Muli-filter match this\n"
					"section, will skip this section\n");
				return;
			}
		}
	}

	if (fmatch) {
		pr_dbg("[software match]dispatch\n"
			"section to filter %d pid %d\n",
			fid, dmx->channel[fmatch->chan_id].pid);
		dmx->sec_cnt_match[SEC_CNT_SW]++;
		if (section_crc(dmx, fmatch, p))
			section_notify(dmx, fmatch, p);
		else
			dmx->sec_cnt_crc_fail[SEC_CNT_SW]++;
	} else {
		pr_dbg("[software match]this section do not\n"
			"match any filter!!!\n");
	}
}


static int _rbuf_write(struct dvb_ringbuffer *buf, const u8 *src, size_t len)
{
	ssize_t free;

	if (!len)
		return 0;
	if (!buf->data)
		return 0;

	free = dvb_ringbuffer_free(buf);
	if (len > free) {
		pr_error("sf: buffer overflow\n");
		return -EOVERFLOW;
	}

	return dvb_ringbuffer_write(buf, src, len);
}

static int _rbuf_filter_pkts(struct dvb_ringbuffer *rb,
			u8 *wrapbuf,
			void (*swfilter_packets)(struct dvb_demux *demux,
						const u8 *buf,
						size_t count),
			struct dvb_demux *demux)
{
	ssize_t len1 = 0;
	ssize_t len2 = 0;
	size_t off;
	size_t count;
	size_t size;

	if (debug_irq & 0x4)
		dump(&rb->data[rb->pread], (debug_irq & 0xFFF00) >> 8);

	/*
	 *   rb|====--------===[0x47]====|
	 *   ^             ^
	 *   wr            rd
	 */

	len1 = rb->pwrite - rb->pread;
	if (len1 < 0) {
		len1 = rb->size - rb->pread;
		len2 = rb->pwrite;
	}

	for (off = 0; off < len1; off++) {
		if (rb->data[rb->pread + off] == 0x47)
			break;
	}

	if (off)
		pr_dbg_irq_sf("off ->|%zd\n", off);

	len1 -= off;
	rb->pread = (rb->pread + off) % rb->size;

	count = len1 / 188;
	if (count) {
		pr_dbg_irq_sf("pkt >> 1[%zd<->%zd]\n", rb->pread, rb->pwrite);
		swfilter_packets(demux, rb->data + rb->pread, count);

		size = count * 188;
		len1 -= size;
		rb->pread += size;
	}

	if (len2 && len1 && ((len1 + len2) > 188)) {
		pr_dbg_irq_sf("pkt >> 2[%zd<->%zd]\n", rb->pread, rb->pwrite);
		size = 188 - len1;
		memcpy(wrapbuf, rb->data + rb->pread, len1);
		memcpy(wrapbuf + len1, rb->data, size);
		swfilter_packets(demux, wrapbuf, 1);
		rb->pread = size;
		len2 -= size;
	}

	if (len2) {
		pr_dbg_irq_sf("pkt >> 3[%zd<->%zd]\n", rb->pread, rb->pwrite);
		count = len2 / 188;
		if (count) {
			swfilter_packets(demux, rb->data + rb->pread, count);
			rb->pread += count * 188;
		}
	}
	return 0;
}

static void smallsection_match_section(struct aml_dmx *dmx, u8 *p, u16 sec_num)
{
	struct aml_filter *f;
	int chid, i;
	int need_crc = 1;

	if (sec_num >= FILTER_COUNT) {
		pr_dbg("sec_num invalid: %d\n", sec_num);
		return;
	}

	f = &dmx->filter[sec_num];
	chid = f->chan_id;

	dmx->sec_cnt[SEC_CNT_SS]++;

	for (i = 0; i < FILTER_COUNT; i++) {
		f = &dmx->filter[i];
		if (f->chan_id != chid)
			continue;
		if (sec_filter_match(dmx, f, p)) {
			if (need_crc) {
				dmx->sec_cnt_match[SEC_CNT_SS]++;
				if (!section_crc(dmx, f, p)) {
					dmx->sec_cnt_crc_fail[SEC_CNT_SS]++;
					return;
				}
				need_crc = 0;
			}
			section_notify(dmx, f, p);
		}
	}

}
static void process_smallsection(struct aml_dmx *dmx)
{

	u32 v, wr, rd;
	u32 data32;
	struct aml_smallsec *ss = &dmx->smallsec;

	v = DMX_READ_REG(dmx->id, DEMUX_SMALL_SEC_CTL);
	wr = (v >> 8) & 0xff;
	rd = (v >> 16) & 0xff;

	if (rd != wr) {
		int n1 = wr - rd,
		    n2 = 0,
		    max = (ss->bufsize>>8);
		int i;
		u8 *p;
		int sec_len;

		pr_dbg_irq_ss("secbuf[31] ctrl:0x%x\n", v);

		if (n1 < 0) {
			n1 = max - rd;
			n2 = wr;
		}
		if (n1) {
			pr_dbg_irq_ss("n1:%d\n", n1);
			dma_sync_single_for_cpu(dmx_get_dev(dmx),
						ss->buf_map+(rd<<8),
						n1<<8,
						DMA_FROM_DEVICE);
			for (i = 0; i < n1; i++) {
				p = (u8 *)ss->buf+((rd+i)<<8);
				sec_len = (((p[1] & 0xF) << 8) | p[2]) + 3;
				smallsection_match_section(dmx, p,
							*(p+sec_len+1));
			}
		}
		if (n2) {
			pr_dbg_irq_ss("n2:%d\n", n2);
			dma_sync_single_for_cpu(dmx_get_dev(dmx),
						ss->buf_map,
						n2<<8,
						DMA_FROM_DEVICE);
			for (i = 0; i < n2; i++) {
				p = (u8 *)ss->buf+(i<<8);
				sec_len = (((p[1] & 0xF) << 8) | p[2]) + 3;
				smallsection_match_section(dmx, p,
							*(p+sec_len+1));
			}
		}

		rd = wr;
		data32 = (DMX_READ_REG(dmx->id,	DEMUX_SMALL_SEC_CTL)
				& 0xff00ffff)
				| (rd << 16);
		DMX_WRITE_REG(dmx->id, DEMUX_SMALL_SEC_CTL, data32);
	}
}


static void process_section(struct aml_dmx *dmx)
{
	u32 ready, i, sec_busy;
	u16 sec_num;

	ready = DMX_READ_REG(dmx->id, SEC_BUFF_READY);
	if (ready) {
		if ((ready & (1<<31)) && dmx->smallsec.enable) {
			u32 v, wr, rd;

			v = DMX_READ_REG(dmx->id, DEMUX_SMALL_SEC_CTL);
			wr = (v >> 8) & 0xff;
			rd = (v >> 16) & 0xff;
			if ((wr < rd) && (5 > (rd - wr)))
				pr_error("warning: small ss buf [w%dr%d]\n",
					wr, rd);
			pr_dbg_irq_ss("ss>%x\n",
				DMX_READ_REG(dmx->id, DEMUX_SMALL_SEC_CTL));
			process_smallsection(dmx);
			DMX_WRITE_REG(dmx->id, SEC_BUFF_READY, (1<<31));
			return;
		}

		for (i = 0; i < SEC_BUF_COUNT; i++) {
			if (!(ready & (1 << i)))
				continue;

			/* get section busy */
			sec_busy = DMX_READ_REG(dmx->id, SEC_BUFF_BUSY);
			/* get filter number */
			DMX_WRITE_REG(dmx->id, SEC_BUFF_NUMBER, i);
			sec_num = (DMX_READ_REG(dmx->id, SEC_BUFF_NUMBER) >> 8);

			/*
			 * sec_buf_watchdog_count dispatch:
			 * byte0 -- always busy=0 's watchdog count
			 * byte1 -- always busy=1 & filter_num=31 's
			 * watchdog count
			 */

			/* sec_busy is not set, check busy=0 watchdog count */
			if (!(sec_busy & (1 << i))) {
				/* clear other wd count	of this buffer */
				dmx->sec_buf_watchdog_count[i] &= 0x000000ff;
				dmx->sec_buf_watchdog_count[i] += 0x1;
				pr_dbg("bit%d ready=1, busy=0,\n"
					"sec_num=%d for %d times\n",
					i, sec_num,
					dmx->sec_buf_watchdog_count[i]);
				if (dmx->sec_buf_watchdog_count[i] >= 5) {
					pr_dbg("busy=0 reach the max count,\n"
						"try software match.\n");
					software_match_section(dmx, i);
					dmx->sec_buf_watchdog_count[i] = 0;
					DMX_WRITE_REG(dmx->id, SEC_BUFF_READY,
							(1 << i));
				}
				continue;
			}

			/* filter_num == 31 && busy == 1,check watchdog count */
			if (sec_num >= FILTER_COUNT) {
				/* clear other wd count	of this buffer */
				dmx->sec_buf_watchdog_count[i] &= 0x0000ff00;
				dmx->sec_buf_watchdog_count[i] += 0x100;
				pr_dbg("bit%d ready=1,busy=1,\n"
					"sec_num=%d for %d times\n",
					i, sec_num,
					dmx->sec_buf_watchdog_count[i] >> 8);
				if (dmx->sec_buf_watchdog_count[i] >= 0x500) {
					pr_dbg("busy=1&filter_num=31\n"
						" reach the max count, clear\n"
						" the buf ready & busy!\n");
					software_match_section(dmx, i);
					dmx->sec_buf_watchdog_count[i] = 0;
					DMX_WRITE_REG(dmx->id,
						      SEC_BUFF_READY,
						      (1 << i));
					DMX_WRITE_REG(dmx->id,
						      SEC_BUFF_BUSY,
						      (1 << i));
				}
				continue;
			}

			/* now, ready & busy are both set and
			 * filter number is valid
			 */
			if (dmx->sec_buf_watchdog_count[i] != 0)
				dmx->sec_buf_watchdog_count[i] = 0;

			/* process this section */
			hardware_match_section(dmx, sec_num, i);

			/* clear the ready & busy bit */
			DMX_WRITE_REG(dmx->id, SEC_BUFF_READY, (1 << i));
			DMX_WRITE_REG(dmx->id, SEC_BUFF_BUSY, (1 << i));
		}
	}
}

#ifdef NO_SUB
static void process_sub(struct aml_dmx *dmx)
{

	u32 rd_ptr = 0;
	u32 wr_ptr = READ_MPEG_REG(PARSER_SUB_WP);
	u32 start_ptr = READ_MPEG_REG(PARSER_SUB_START_PTR);
	u32 end_ptr = READ_MPEG_REG(PARSER_SUB_END_PTR);
	u32 buffer1 = 0, buffer2 = 0;
	u8 *buffer1_virt = 0, *buffer2_virt = 0;
	u32 len1 = 0, len2 = 0;

	if (!dmx->sub_buf_base_virt)
		return;

	rd_ptr = READ_MPEG_REG(PARSER_SUB_RP);
	if (!rd_ptr)
		return;
	if (rd_ptr > wr_ptr) {
		len1 = end_ptr - rd_ptr + 8;
		buffer1 = rd_ptr;

		len2 = wr_ptr - start_ptr;
		buffer2 = start_ptr;

		rd_ptr = start_ptr + len2;
	} else if (rd_ptr < wr_ptr) {
		len1 = wr_ptr - rd_ptr;
		buffer1 = rd_ptr;
		rd_ptr += len1;
		len2 = 0;
	} else if (rd_ptr == wr_ptr) {
		pr_dbg("sub no data\n");
	}

	if (buffer1 && len1)
#ifdef SUB_BUF_DMX
		buffer1_virt = (void *)dmx->sub_pages + (buffer1 - start_ptr);
#else
		buffer1_virt = (void *)dmx->sub_buf_base_virt + (buffer1 - start_ptr);
#endif

	if (buffer2 && len2)
#ifdef SUB_BUF_DMX
		buffer2_virt = (void *)dmx->sub_pages + (buffer2 - start_ptr);
#else
		buffer2_virt = (void *)dmx->sub_buf_base_virt + (buffer2 - start_ptr);
#endif

	pr_dbg_irq_sub("sub: rd_ptr:%x buf1:%x len1:%d buf2:%x len2:%d\n",
		rd_ptr, buffer1, len1, buffer2, len2);
	pr_dbg_irq_sub("sub: buf1_virt:%p buf2_virt:%p\n",
		buffer1_virt, buffer2_virt);

	if (len1)
		dma_sync_single_for_cpu(dmx_get_dev(dmx),
					(dma_addr_t) buffer1, len1,
					DMA_FROM_DEVICE);
	if (len2)
		dma_sync_single_for_cpu(dmx_get_dev(dmx),
					(dma_addr_t) buffer2, len2,
					DMA_FROM_DEVICE);

	if (dmx->channel[2].used) {
		if (dmx->channel[2].feed && dmx->channel[2].feed->cb.ts &&
			((buffer1_virt != NULL && len1 !=0 ) || (buffer2_virt != NULL && len2 != 0)))
		{
			dmx->channel[2].feed->cb.ts(buffer1_virt, len1,
						buffer2_virt, len2,
						&dmx->channel[2].feed->feed.ts,0);
		}
	}
	WRITE_MPEG_REG(PARSER_SUB_RP, rd_ptr);
}
#endif

static void process_pes(struct aml_dmx *dmx)
{
	long off, off_pre = pes_off_pre[dmx->id];
	u8 *buffer1 = 0, *buffer2 = 0;
	u8 *buffer1_phys = 0, *buffer2_phys = 0;
	u32 len1 = 0, len2 = 0;
	int i = 1;

	off = (DMX_READ_REG(dmx->id, OTHER_WR_PTR) << 3);

	pr_dbg_irq_pes("[%d]WR:0x%x PES WR:0x%x\n", dmx->id,
			DMX_READ_REG(dmx->id, OTHER_WR_PTR),
			DMX_READ_REG(dmx->id, OB_PES_WR_PTR));
	buffer1 = (u8 *)(dmx->pes_pages + off_pre);
	pr_dbg_irq_pes("[%d]PES WR[%02x %02x %02x %02x %02x %02x %02x %02x",
		dmx->id,
		buffer1[0], buffer1[1], buffer1[2], buffer1[3],
		buffer1[4], buffer1[5], buffer1[6], buffer1[7]);
	pr_dbg_irq_pes(" %02x %02x %02x %02x %02x %02x %02x %02x]\n",
			buffer1[8], buffer1[9], buffer1[10], buffer1[11],
			buffer1[12], buffer1[13], buffer1[14], buffer1[15]);

	if (off > off_pre) {
		len1 = off-off_pre;
		buffer1 = (unsigned char *)(dmx->pes_pages + off_pre);
	} else if (off < off_pre) {
		len1 = dmx->pes_buf_len-off_pre;
		buffer1 = (unsigned char *)(dmx->pes_pages + off_pre);
		len2 = off;
		buffer2 = (unsigned char *)dmx->pes_pages;
	} else if (off == off_pre) {
		pr_dbg("pes no data\n");
	}
	pes_off_pre[dmx->id] = off;
	if (len1) {
		buffer1_phys = (unsigned char *)virt_to_phys(buffer1);
		dma_sync_single_for_cpu(dmx_get_dev(dmx),
			(dma_addr_t)buffer1_phys, len1, DMA_FROM_DEVICE);
	}
	if (len2) {
		buffer2_phys = (unsigned char *)virt_to_phys(buffer2);
		dma_sync_single_for_cpu(dmx_get_dev(dmx),
			(dma_addr_t)buffer2_phys, len2, DMA_FROM_DEVICE);
	}
	if (len1 || len2) {
		struct aml_channel *ch;

		for (i = 0; i < CHANNEL_COUNT; i++) {
			ch = &dmx->channel[i];
			if (ch->used && ch->feed
				&& (ch->feed->type == DMX_TYPE_TS)) {
				if (ch->feed->ts_type & TS_PAYLOAD_ONLY) {
					ch->feed->cb.ts(buffer1,
						len1, buffer2, len2,
						&ch->feed->feed.ts,0);
				}
			}
		}
	}
}

static void process_om_read(struct aml_dmx *dmx)
{
	unsigned int i;
	unsigned short om_cmd_status_data_0 = 0;
	unsigned short om_cmd_status_data_1 = 0;
	unsigned short om_cmd_data_out = 0;

	om_cmd_status_data_0 = DMX_READ_REG(dmx->id, OM_CMD_STATUS);
	om_cmd_status_data_1 = DMX_READ_REG(dmx->id, OM_CMD_DATA);

	if (om_cmd_status_data_0 & 1) {
		DMX_WRITE_REG(dmx->id, OM_DATA_RD_ADDR,
			(1 << 15) | ((om_cmd_status_data_1 & 0xff) << 2));
		for (i = 0; i < (((om_cmd_status_data_1 >> 7) & 0x1fc) >> 1);
		     i++) {
			om_cmd_data_out = DMX_READ_REG(dmx->id, OM_DATA_RD);
		}

		om_cmd_data_out = DMX_READ_REG(dmx->id, OM_DATA_RD_ADDR);
		DMX_WRITE_REG(dmx->id, OM_DATA_RD_ADDR, 0);
		DMX_WRITE_REG(dmx->id, OM_CMD_STATUS, 1);
	}
}

static void dmx_irq_bh_handler(unsigned long arg)
{
	struct aml_dmx *dmx = (struct aml_dmx *)arg;
	process_smallsection(dmx);
}

static irqreturn_t dmx_irq_handler(int irq_number, void *para)
{
	struct aml_dmx *dmx = (struct aml_dmx *)para;
	struct aml_dvb *dvb = aml_get_dvb_device();
	u32 status;
	unsigned long flags;

	spin_lock_irqsave(&dvb->slock, flags);
	status = DMX_READ_REG(dmx->id, STB_INT_STATUS);
	if (!status)
		goto irq_handled;

	pr_dbg_irq("demux %d irq status: 0x%08x\n", dmx->id, status);

	if (status & (1 << SECTION_BUFFER_READY))
		process_section(dmx);
#ifdef NO_SUB
	if (status & (1 << SUB_PES_READY)) {
		/*If the subtitle is set by tsdemux,
		 *do not parser in demux driver.
		 */
		if (dmx->sub_chan == -1)
			process_sub(dmx);
	}
#endif
	if (status & (1 << OTHER_PES_READY))
		process_pes(dmx);
	if (status & (1 << OM_CMD_READ_PENDING))
		process_om_read(dmx);
	if (status & (1 << TS_ERROR_PIN))
		pr_error("TS_ERROR_PIN\n");

	if (status & (1 << NEW_PDTS_READY)) {
		u32 pdts_status = DMX_READ_REG(dmx->id, STB_PTS_DTS_STATUS);

		if (pdts_status & (1 << VIDEO_PTS_READY)) {
			video_pts = DMX_READ_REG(dmx->id, VIDEO_PTS_DEMUX);
			video_pts_bit32 =
				(pdts_status & (1 << VIDEO_PTS_BIT32)) ? 1 : 0;
			if (!first_video_pts
			    || 0 > (int)(video_pts - first_video_pts))
				first_video_pts = video_pts;
		}

		if (pdts_status & (1 << AUDIO_PTS_READY)) {
			audio_pts = DMX_READ_REG(dmx->id, AUDIO_PTS_DEMUX);
			audio_pts_bit32 =
				(pdts_status & (1 << AUDIO_PTS_BIT32)) ? 1 : 0;
			if (!first_audio_pts
			    || 0 > (int)(audio_pts - first_audio_pts))
				first_audio_pts = audio_pts;
		}
	}

	if (dmx->irq_handler)
		dmx->irq_handler(dmx->dmx_irq, (void *)(long)dmx->id);

	DMX_WRITE_REG(dmx->id, STB_INT_STATUS, status);

	{
		if (!dmx->int_check_time) {
			dmx->int_check_time = jiffies;
			dmx->int_check_count = 0;
		}

		if (jiffies_to_msecs(jiffies - dmx->int_check_time) >= 100
		    || dmx->int_check_count > 1000) {
			if (dmx->int_check_count > 1000) {
				struct aml_dvb *dvb =
				    (struct aml_dvb *)dmx->demux.priv;
				pr_error("Too many irq (%d irq in %d ms)!\n",
					dmx->int_check_count,
					jiffies_to_msecs(jiffies -
						      dmx->int_check_time));
				if (dmx->fe && !dmx->in_tune)
					DMX_WRITE_REG(dmx->id, STB_INT_MASK, 0);
				dmx_reset_hw_ex(dvb, 0);
			}
			dmx->int_check_time = 0;
		}

		dmx->int_check_count++;

		if (dmx->in_tune) {
			dmx->error_check++;
			if (dmx->error_check > 200)
				DMX_WRITE_REG(dmx->id, STB_INT_MASK, 0);
		}
	}

irq_handled:
	spin_unlock_irqrestore(&dvb->slock, flags);
	return IRQ_HANDLED;
}

static inline int dmx_get_order(unsigned long size)
{
	int order;

	order = -1;
	do {
		size >>= 1;
		order++;
	} while (size);

	return order;
}

static inline int dmx_get_afifo_size(struct aml_asyncfifo *afifo)
{
	return afifo->secure_enable && afifo->blk.len ? afifo->blk.len : asyncfifo_buf_len;
}

static void dvr_process_channel(struct aml_asyncfifo *afifo,
				struct aml_channel *channel,
				u32 total, u32 size,
				struct aml_swfilter *sf)
{
	int cnt;
	int ret = 0;
	struct aml_dvr_block blk;

	if (afifo->buf_read > afifo->buf_toggle) {
		cnt = total - afifo->buf_read;
		if (!(afifo->secure_enable && afifo->blk.addr)) {
		dma_sync_single_for_cpu(asyncfifo_get_dev(afifo),
				afifo->pages_map+afifo->buf_read*size,
				cnt*size,
				DMA_FROM_DEVICE);
		if (sf)
			ret = _rbuf_write(&sf->rbuf,
					(u8 *)afifo->pages+afifo->buf_read*size,
					cnt*size);
		else
			channel->dvr_feed->cb.ts(
					(u8 *)afifo->pages+afifo->buf_read*size,
					cnt*size, NULL, 0,
					&channel->dvr_feed->feed.ts,0);
		} else {
			blk.addr = afifo->blk.addr+afifo->buf_read*size;
			blk.len = cnt*size;
			if (sf)
				ret = _rbuf_write(&sf->rbuf,
					(u8 *)afifo->pages+afifo->buf_read*size,
					cnt*size);
			else {
				channel->dvr_feed->cb.ts(
					(u8 *)&blk,
					sizeof(struct aml_dvr_block),
					NULL, 0,
					&channel->dvr_feed->feed.ts,0);
			}
		}
		afifo->buf_read = 0;
	}

	if (afifo->buf_toggle > afifo->buf_read) {
		cnt = afifo->buf_toggle - afifo->buf_read;
		if (!(afifo->secure_enable && afifo->blk.addr)) {
		dma_sync_single_for_cpu(asyncfifo_get_dev(afifo),
				afifo->pages_map+afifo->buf_read*size,
				cnt*size,
				DMA_FROM_DEVICE);
		if (sf) {
			if (ret >= 0)
				ret = _rbuf_write(&sf->rbuf,
					(u8 *)afifo->pages+afifo->buf_read*size,
					cnt*size);
			} else {
			channel->dvr_feed->cb.ts(
					(u8 *)afifo->pages+afifo->buf_read*size,
					cnt*size, NULL, 0,
				&channel->dvr_feed->feed.ts,0);
			}
		} else {
			blk.addr = afifo->blk.addr+afifo->buf_read*size;
			blk.len = cnt*size;
			if (sf)
				ret = _rbuf_write(&sf->rbuf,
					(u8 *)afifo->pages+afifo->buf_read*size,
					cnt*size);
			else {
				channel->dvr_feed->cb.ts(
					(u8 *)&blk,
					sizeof(struct aml_dvr_block),
					NULL, 0,
					&channel->dvr_feed->feed.ts,0);
			}
		}
		afifo->buf_read = afifo->buf_toggle;
	}

	if (sf && ret > 0) {
		_rbuf_filter_pkts(&sf->rbuf, sf->wrapbuf,
				dvb_dmx_swfilter_packets,
				channel->dvr_feed->demux);
	} else if (sf && ret <= 0)
		pr_error("sf rbuf write error[%d]\n", ret);
	else
		pr_dbg_irq_dvr("write data to dvr\n");
}

static uint32_t last_afifo_time = 0;
static void dvr_irq_bh_handler(unsigned long arg)
{
	struct aml_asyncfifo *afifo = (struct aml_asyncfifo *)arg;
	struct aml_dvb *dvb = afifo->dvb;
	struct aml_dmx *dmx;
	u32 size, total;
	int i, factor;
	unsigned long flags;

	pr_dbg_irq_dvr("async fifo %d irq, interval:%d ms, %d data\n", afifo->id,
			jiffies_to_msecs(jiffies - last_afifo_time), afifo->flush_size);

	if (dvb)
		spin_lock_irqsave(&dvb->slock, flags);

	if (dvb && afifo->source >= AM_DMX_0 && afifo->source < AM_DMX_MAX) {
		dmx = &dvb->dmx[afifo->source];
		if (dmx->init && dmx->record) {
			struct aml_swfilter *sf = &dvb->swfilter;
			int issf = 0;

			total = afifo->buf_len / afifo->flush_size;
			factor = dmx_get_order(total);
			size = afifo->buf_len >> factor;

			if (sf->user && (sf->afifo == afifo))
				issf = 1;

			for (i = 0; i < CHANNEL_COUNT; i++) {
				if (dmx->channel[i].used
						&& dmx->channel[i].dvr_feed) {
					dvr_process_channel(afifo,
							&dmx->channel[i],
							total,
							size,
							issf?sf:NULL);
				break;
				}
			}

		}
	}
	if (dvb)
		spin_unlock_irqrestore(&dvb->slock, flags);
	last_afifo_time = jiffies;
}

static irqreturn_t dvr_irq_handler(int irq_number, void *para)
{
	struct aml_asyncfifo *afifo = (struct aml_asyncfifo *)para;
	int factor = dmx_get_order(afifo->buf_len / afifo->flush_size);

	afifo->buf_toggle++;
	afifo->buf_toggle %= (1 << factor);
	tasklet_schedule(&afifo->asyncfifo_tasklet);
	return IRQ_HANDLED;
}

/*Enable the STB*/
static void stb_enable(struct aml_dvb *dvb)
{
	int out_src, des_in, en_des, fec_clk, hiu, dec_clk_en;
	int src, tso_src, i;
	u32 fec_s0, fec_s1,fec_s2;
	u32 invert0, invert1, invert2;
	u32 data;

	switch (dvb->stb_source) {
	case AM_TS_SRC_DMX0:
		src = dvb->dmx[0].source;
		break;
	case AM_TS_SRC_DMX1:
		src = dvb->dmx[1].source;
		break;
	case AM_TS_SRC_DMX2:
		src = dvb->dmx[2].source;
		break;
	default:
		src = dvb->stb_source;
		break;
	}

	switch (src) {
	case AM_TS_SRC_TS0:
		fec_clk = tsfile_clkdiv;
		hiu = 0;
		break;
	case AM_TS_SRC_TS1:
		fec_clk = tsfile_clkdiv;
		hiu = 0;
		break;
	case AM_TS_SRC_TS2:
		fec_clk = tsfile_clkdiv;
		hiu = 0;
		break;
	case AM_TS_SRC_TS3:
		fec_clk = tsfile_clkdiv;
		hiu = 0;
		break;
	case AM_TS_SRC_S_TS0:
		fec_clk = tsfile_clkdiv;
		hiu = 0;
		break;
	case AM_TS_SRC_S_TS1:
		fec_clk = tsfile_clkdiv;
		hiu = 0;
		break;
	case AM_TS_SRC_S_TS2:
		fec_clk = tsfile_clkdiv;
		hiu = 0;
		break;
	case AM_TS_SRC_HIU:
		fec_clk = tsfile_clkdiv;
		hiu = 1;
		break;
	case AM_TS_SRC_HIU1:
		fec_clk = tsfile_clkdiv;
		hiu = 1;
		break;
	default:
		fec_clk = 0;
		hiu = 0;
		break;
	}

	switch (dvb->dsc[0].source) {
	case AM_TS_SRC_DMX0:
		des_in = 0;
		en_des = 1;
		dec_clk_en = 1;
		break;
	case AM_TS_SRC_DMX1:
		des_in = 1;
		en_des = 1;
		dec_clk_en = 1;
		break;
	case AM_TS_SRC_DMX2:
		des_in = 2;
		en_des = 1;
		dec_clk_en = 1;
		break;
	default:
		des_in = 0;
		en_des = 0;
		dec_clk_en = 0;
		break;
	}
	switch (dvb->tso_source) {
	case AM_TS_SRC_DMX0:
		tso_src = dvb->dmx[0].source;
		break;
	case AM_TS_SRC_DMX1:
		tso_src = dvb->dmx[1].source;
		break;
	case AM_TS_SRC_DMX2:
		tso_src = dvb->dmx[2].source;
		break;
	default:
		tso_src = dvb->tso_source;
		break;
	}

	switch (tso_src) {
	case AM_TS_SRC_TS0:
		out_src = 0;
		break;
	case AM_TS_SRC_TS1:
		out_src = 1;
		break;
	case AM_TS_SRC_TS2:
		out_src = 2;
		break;
	case AM_TS_SRC_TS3:
		out_src = 3;
		break;
	case AM_TS_SRC_S_TS0:
		out_src = 6;
		break;
	case AM_TS_SRC_S_TS1:
		out_src = 5;
		break;
	case AM_TS_SRC_S_TS2:
		out_src = 4;
		break;
	case AM_TS_SRC_HIU:
		out_src = 7;
		break;
	default:
		out_src = 0;
		break;
	}

	pr_dbg("[stb]src: %d, dsc1in: %d, tso: %d\n", src, des_in, out_src);

	fec_s0 = 0;
	fec_s1 = 0;
	fec_s2 = 0;
	invert0 = 0;
	invert1 = 0;
	invert2 = 0;

	for (i = 0; i < dvb->ts_in_total_count; i++) {
		if (dvb->ts[i].s2p_id == 0)
			fec_s0 = i;
		else if (dvb->ts[i].s2p_id == 1)
			fec_s1 = i;
		else if (dvb->ts[i].s2p_id == 2)
			fec_s2 = i;
	}

	invert0 = dvb->s2p[0].invert;
	invert1 = dvb->s2p[1].invert;

	WRITE_MPEG_REG(STB_TOP_CONFIG,
		       (invert1 << INVERT_S2P1_FEC_CLK) |
		       (fec_s1 << S2P1_FEC_SERIAL_SEL) |
		       (out_src << TS_OUTPUT_SOURCE) |
		       (des_in << DES_INPUT_SEL) |
		       (en_des << ENABLE_DES_PL) |
		       (dec_clk_en << ENABLE_DES_PL_CLK) |
		       (invert0 << INVERT_S2P0_FEC_CLK) |
		       (fec_s0 << S2P0_FEC_SERIAL_SEL)|
		       (ciplus));
	ciplus = 0;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_TL1) {
		invert2 = dvb->s2p[2].invert;

		WRITE_MPEG_REG(STB_S2P2_CONFIG,
		       (invert2 << INVERT_S2P2_FEC_CLK) |
		       (fec_s2 << S2P2_FEC_SERIAL_SEL));
	}

	if (dvb->reset_flag)
		hiu = 0;
	/* invert ts out clk,add ci model need add this*/
	if (dvb->ts_out_invert) {
		printk("ts out invert ---\r\n");
		data = READ_MPEG_REG(TS_TOP_CONFIG);
		data |= 1 << TS_OUT_CLK_INVERT;
		WRITE_MPEG_REG(TS_TOP_CONFIG, data);
	}

	if (src == AM_TS_SRC_HIU1) {
		WRITE_MPEG_REG(TS_HIU1_CONFIG,
			       (demux_skipbyte << FILE_M2TS_SKIP_BYTES_HIU1) |
			       (hiu << TS_HIU_ENABLE_HIU1) |
			       (fec_clk << FEC_CLK_DIV_HIU1) |
			       (0xBB << TS_PACKAGE_LENGTH_SUB_1_HIU1) |
				   (0x47 << FEC_SYNC_BYTE_HIU1));
	} else {
		/* invert ts out clk  end */
		WRITE_MPEG_REG(TS_FILE_CONFIG,
			       (demux_skipbyte << 16) |
			       (6 << DES_OUT_DLY) |
			       (3 << TRANSPORT_SCRAMBLING_CONTROL_ODD) |
			       (3 << TRANSPORT_SCRAMBLING_CONTROL_ODD_2) |
			       (hiu << TS_HIU_ENABLE) | (fec_clk << FEC_FILE_CLK_DIV));
	}
}

int dsc_set_pid(struct aml_dsc_channel *ch, int pid)
{
	struct aml_dsc *dsc = ch->dsc;
	int is_dsc2 = (dsc->id == 1) ? 1 : 0;
	u32 data;

	WRITE_MPEG_REG(TS_PL_PID_INDEX,
			((ch->id & 0x0f) >> 1)+(is_dsc2 ? 4 : 0));
	data = READ_MPEG_REG(TS_PL_PID_DATA);
	if (ch->id & 1) {
		data &= 0xFFFF0000;
		data |= pid & 0x1fff;
		if (!ch->used)
			data |= 1 << PID_MATCH_DISABLE_LOW;
	} else {
		data &= 0xFFFF;
		data |= (pid & 0x1fff) << 16;
		if (!ch->used)
			data |= 1 << PID_MATCH_DISABLE_HIGH;
	}
	WRITE_MPEG_REG(TS_PL_PID_INDEX,
			((ch->id & 0x0f) >> 1)+(is_dsc2 ? 4 : 0));
	WRITE_MPEG_REG(TS_PL_PID_DATA, data);
	WRITE_MPEG_REG(TS_PL_PID_INDEX, 0);

	if (ch->used)
		pr_dbg("set DSC %d ch %d PID %d\n", dsc->id, ch->id, pid);
	else
		pr_dbg("disable DSC %d ch %d\n", dsc->id, ch->id);
	return 0;
}

int dsc_get_pid(struct aml_dsc_channel *ch, int *pid)
{
	struct aml_dsc *dsc = ch->dsc;
	int is_dsc2 = (dsc->id == 1) ? 1 : 0;
	u32 data;

	WRITE_MPEG_REG(TS_PL_PID_INDEX,
			((ch->id & 0x0f) >> 1)+(is_dsc2 ? 4 : 0));
	data = READ_MPEG_REG(TS_PL_PID_DATA);
	if (ch->id & 1) {
		*pid = data & 0x1fff;
	} else {
		*pid = (data >> 16) & 0x1fff;
	}
	return 0;
}

int dsc_set_key(struct aml_dsc_channel *ch, int flags, enum ca_cw_type type,
			u8 *key)
{
	int ret = -1;

	switch (type) {
	case CA_CW_DVB_CSA_EVEN:
	case CA_CW_DVB_CSA_ODD:
		aml_ci_plus_disable();
		ret = dsc_set_csa_key(ch, flags, type, key);
		if (ret != 0)
			goto END;
		/* Different with old mode, do change */
		if (ch->work_mode == CIPLUS_MODE || ch->work_mode == -1) {
			if (ch->work_mode == -1)
				pr_inf("dsc[%d:%d] enable\n",
					ch->dsc->id, ch->id);
			else
				pr_inf("dsc[%d:%d] enable (from ciplus)\n",
					ch->dsc->id, ch->id);
			ch->mode = ECB_MODE;
			ch->work_mode = DVBCSA_MODE;
		}
		break;
	case CA_CW_AES_EVEN:
	case CA_CW_AES_ODD:
	case CA_CW_AES_EVEN_IV:
	case CA_CW_AES_ODD_IV:
	case CA_CW_DES_EVEN:
	case CA_CW_DES_ODD:
	case CA_CW_SM4_EVEN:
	case CA_CW_SM4_ODD:
	case CA_CW_SM4_EVEN_IV:
	case CA_CW_SM4_ODD_IV:
		ret = dsc_set_aes_des_sm4_key(ch, flags, type, key);
		if (ret != 0)
			goto END;
		am_ci_plus_set_output(ch);
		/* Different with old mode, do change */
		if (ch->work_mode == DVBCSA_MODE || ch->work_mode == -1) {
			if (ch->work_mode == -1)
				pr_inf("dsc[%d:%d] ciplus enable\n",
					ch->dsc->id, ch->id);
			else
				pr_inf("dsc[%d:%d] ciplus enable (from dsc)\n",
					ch->dsc->id, ch->id);
			ch->work_mode = CIPLUS_MODE;
		}
		break;
	default:
		break;
	}
END:
	return ret;
}

int dsc_set_keys(struct aml_dsc_channel *ch)
{
	int types = ch->set & 0xFFFFFF;
	int flag = (ch->set >> 24) & 0xFF;
	int i;
	u8  *k;
	int ret = 0;

	for (i = 0; i < CA_CW_TYPE_MAX; i++) {
		if (types & (1 << i)) {
			k = NULL;
			switch (i) {
			case CA_CW_DVB_CSA_EVEN:
			case CA_CW_AES_EVEN:
			case CA_CW_DES_EVEN:
			case CA_CW_SM4_EVEN:
				k = ch->even;
				break;
			case CA_CW_DVB_CSA_ODD:
			case CA_CW_AES_ODD:
			case CA_CW_DES_ODD:
			case CA_CW_SM4_ODD:
				k = ch->odd;
				break;
			case CA_CW_AES_EVEN_IV:
			case CA_CW_SM4_EVEN_IV:
				k = ch->even_iv;
				break;
			case CA_CW_AES_ODD_IV:
			case CA_CW_SM4_ODD_IV:
				k = ch->odd_iv;
				break;
			}
			if (k)
				ret = dsc_set_key(ch, flag,
					i,
					k);
		}
	}
	return 0;
}

static int dsc_set_csa_key(struct aml_dsc_channel *ch, int flags,
			enum ca_cw_type type, u8 *key)
{
	struct aml_dsc *dsc = ch->dsc;
	int is_dsc2 = (dsc->id == 1) ? 1 : 0;
	u16 k0, k1, k2, k3;
	u32 key0, key1;
	int reg;

	if (flags & DSC_FROM_KL) {
		k0 = k1 = k2 = k3 = 0;
		/*dummy write to check if kl not working*/
		key0 = key1 = 0;
		WRITE_MPEG_REG(COMM_DESC_KEY0, key0);
		WRITE_MPEG_REG(COMM_DESC_KEY1, key1);

	/*tdes? :*/
		if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXBB) {
			WRITE_MPEG_REG(COMM_DESC_KEY_RW,
				((1 << 5)) |
				((ch->id + type * DSC_COUNT)+
					(is_dsc2 ? 16 : 0)));
		}
		if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXL ||
			get_cpu_type() == MESON_CPU_MAJOR_ID_GXM) {
			pr_info("do kl..\n");
			WRITE_MPEG_REG(COMM_DESC_KEY_RW,
				(type ? (1 << 6) : (1 << 5)) | (1<<7) |
				((ch->id + type * DSC_COUNT)+
				 (is_dsc2 ? 16 : 0)));
		}
		reg = (type ? (1 << 6) : (1 << 5)) |
				((ch->id + type * DSC_COUNT)+
				 (is_dsc2 ? 16 : 0));
	} else {
		k0 = (key[0] << 8) | key[1];
		k1 = (key[2] << 8) | key[3];
		k2 = (key[4] << 8) | key[5];
		k3 = (key[6] << 8) | key[7];

		key0 = (k0 << 16) | k1;
		key1 = (k2 << 16) | k3;
		WRITE_MPEG_REG(COMM_DESC_KEY0, key0);
		WRITE_MPEG_REG(COMM_DESC_KEY1, key1);

		reg = (ch->id + type * DSC_COUNT)+(is_dsc2 ? 16 : 0);
		WRITE_MPEG_REG(COMM_DESC_KEY_RW, reg);
	}

	return 0;
}

/************************* AES DESC************************************/
#define ENABLE_DEC_PL     7
#define ENABLE_DES_PL_CLK 15

#define KEY_WR_AES_IV_B 5
#define KEY_WR_AES_IV_A 4
#define KEY_WR_AES_B    3
#define KEY_WR_AES_A    2
#define KEY_WR_DES_B    1
#define KEY_WR_DES_A    0

#define IDSA_MODE_BIT	31
#define SM4_MODE	30
#define DES2_KEY_ENDIAN 25
#define DES2_IN_ENDIAN  21
#define DES2_CFG	6
#define DES2_EN		5
#define CNTL_ENABLE     3
#define AES_CBC_DISABLE 2
#define AES_EN          1
#define DES_EN          0

#define AES_IV_ENDIAN	28
#define AES_MSG_OUT_ENDIAN 24
#define AES_MSG_IN_ENDIAN  20
#define AES_KEY_ENDIAN  16
#define DES_MSG_OUT_ENDIAN 8
#define DES_MSG_IN_ENDIAN  4
#define DES_KEY_ENDIAN  0

#define ALGO_AES		0
#define ALGO_SM4		1
#define ALGO_DES		2

/*
 * param:
 * key:
 *	16bytes IV key
 * type:
 *	AM_DSC_KEY_TYPE_AES_ODD    IV odd key
 *	AM_DSC_KEY_TYPE_AES_EVEN  IV even key
 */
void aml_ci_plus_set_iv(struct aml_dsc_channel *ch, enum ca_cw_type type,
			u8 *key)
{
	unsigned int k0, k1, k2, k3;

	k3 = (key[0] << 24) | (key[1] << 16) | (key[2] << 8) | key[3];
	k2 = (key[4] << 24) | (key[5] << 16) | (key[6] << 8) | key[7];
	k1 = (key[8] << 24) | (key[9] << 16) | (key[10] << 8) | key[11];
	k0 = (key[12] << 24) | (key[13] << 16) | (key[14] << 8) | key[15];

	if (type == CA_CW_AES_EVEN_IV ||
		type == CA_CW_SM4_EVEN_IV) {
		WRITE_MPEG_REG(CIPLUS_KEY0, k0);
		WRITE_MPEG_REG(CIPLUS_KEY1, k1);
		WRITE_MPEG_REG(CIPLUS_KEY2, k2);
		WRITE_MPEG_REG(CIPLUS_KEY3, k3);
		WRITE_MPEG_REG(CIPLUS_KEY_WR,
			(ch->id << 9) | (1<<KEY_WR_AES_IV_A));
	} else if (type == CA_CW_AES_ODD_IV ||
			   type == CA_CW_SM4_ODD_IV) {
		WRITE_MPEG_REG(CIPLUS_KEY0, k0);
		WRITE_MPEG_REG(CIPLUS_KEY1, k1);
		WRITE_MPEG_REG(CIPLUS_KEY2, k2);
		WRITE_MPEG_REG(CIPLUS_KEY3, k3);
		WRITE_MPEG_REG(CIPLUS_KEY_WR,
			(ch->id << 9) | (1<<KEY_WR_AES_IV_B));
	}
}

/*
 * Param:
 * key_endian
 *	S905D  7 for kl    0 for set key directly
 * mode
 *  0 for ebc
 *  1 for cbc
 */
static void aml_ci_plus_config(int key_endian, int mode, int algo)
{
	unsigned int data;
	unsigned int idsa_mode = 0;
	unsigned int sm4_mode = 0;
	unsigned int cbc_disable = 0;
	unsigned int des_enable = 0;
	unsigned int aes_enable = 0;
	unsigned int des2_key_endian = 0;
	unsigned int des2_in_endian = 0;
	unsigned int des2_cfg = 0;
	unsigned int des2_enable = 0;

	pr_dbg("%s mode:%d,alog:%d\n",__FUNCTION__,mode,algo);

	if (get_cpu_type() < MESON_CPU_MAJOR_ID_SM1) {
		WRITE_MPEG_REG(CIPLUS_ENDIAN,
				(15 << AES_MSG_OUT_ENDIAN)
				| (15 << AES_MSG_IN_ENDIAN)
				| (key_endian << AES_KEY_ENDIAN)
				|
				(15 << DES_MSG_OUT_ENDIAN)
				| (15 << DES_MSG_IN_ENDIAN)
				| (key_endian << DES_KEY_ENDIAN)
				);
	} else if (algo == ALGO_DES){
		WRITE_MPEG_REG(CIPLUS_ENDIAN,
				(15 << AES_IV_ENDIAN)
				| (7 << AES_MSG_OUT_ENDIAN)
				| (15 << AES_MSG_IN_ENDIAN)
				| (15 << AES_KEY_ENDIAN)
				);
		pr_inf("CIPLUS_ENDIAN is 0x%x\n", READ_MPEG_REG(CIPLUS_ENDIAN));
	} else {
		WRITE_MPEG_REG(CIPLUS_ENDIAN, 0);
	}

	data = READ_MPEG_REG(CIPLUS_ENDIAN);

	if (algo == ALGO_SM4) {
		sm4_mode = 1;
	} else if (algo ==  ALGO_AES){
		aes_enable = 1;
	} else {
		if (get_cpu_type() < MESON_CPU_MAJOR_ID_SM1) {
			des_enable = 1;
		} else {
			des2_key_endian = 8;
			des2_in_endian = 8;
			des2_cfg = 2;
			des2_enable = 1;
			aes_enable = 1;
		}
	}

	if (mode == IDSA_MODE) {
		idsa_mode = 1;
		cbc_disable = 0;
	} else if (mode == CBC_MODE) {
		cbc_disable = 0;
	} else {
		cbc_disable = 1;
	}
	pr_dbg("idsa_mode:%d sm4_mode:%d cbc_disable:%d aes_enable:%d des_enable:%d\n", \
		idsa_mode,sm4_mode,cbc_disable,aes_enable,des_enable);

	data =  (idsa_mode << IDSA_MODE_BIT) |
			(sm4_mode << SM4_MODE ) |
			(des2_key_endian << DES2_KEY_ENDIAN) |
			(des2_in_endian << DES2_IN_ENDIAN) |
			(des2_cfg << DES2_CFG) |
			(des2_enable << DES2_EN) |
			(cbc_disable << AES_CBC_DISABLE) |
			(1 << CNTL_ENABLE) |
			(aes_enable << AES_EN) |
			(des_enable << DES_EN);

	WRITE_MPEG_REG(CIPLUS_CONFIG, data);
	data = READ_MPEG_REG(CIPLUS_CONFIG);
	pr_dbg("CIPLUS_CONFIG is 0x%x\n",data);
}

static void set_fec_core_sel (struct aml_dvb *dvb)
{
	int i;

	for (i = 0; i < DMX_DEV_COUNT; i ++) {
		int set = 0;
		u32 ctrl = DMX_READ_REG(i, FEC_INPUT_CONTROL);

		if ((dvb->dsc[0].dst != -1) && (dvb->dsc[0].dst - AM_TS_SRC_DMX0 == i)) {
			set = 1;
		} else if ((dvb->dsc[1].dst != -1) && (dvb->dsc[1].dst - AM_TS_SRC_DMX0 == i)) {
			set = 1;
		} else {
			u32 cfg = READ_MPEG_REG(CIPLUS_CONFIG);

			if (cfg & (1 << CNTL_ENABLE)) {
				if (!ciplus_out_auto_mode) {
					if (ciplus_out_sel & (1 << i))
						set = 1;
				}
			}
		}

		if (set) {
			ctrl |= (1 << FEC_CORE_SEL);
		} else {
			ctrl &= ~(1 << FEC_CORE_SEL);
		}

		DMX_WRITE_REG(i, FEC_INPUT_CONTROL, ctrl);
	}
}

/*
 * Set output to demux set.
 */
static void am_ci_plus_set_output(struct aml_dsc_channel *ch)
{
	struct aml_dsc *dsc = ch->dsc;
	struct aml_dvb *dvb = dsc->dvb;
	u32 data;
	u32 in = 0, out = 0;
	int set = 0;

	if (dsc->id != 0) {
		pr_error("Ciplus set output can only work at dsc0 device\n");
		return;
	}

	switch (dsc->source) {
	case  AM_TS_SRC_DMX0:
		in = 0;
		break;
	case  AM_TS_SRC_DMX1:
		in = 1;
		break;
	case  AM_TS_SRC_DMX2:
		in = 2;
		break;
	default:
		break;
	}

	if (ciplus_out_auto_mode == 1) {
		switch (dsc->dst) {
		case  AM_TS_SRC_DMX0:
			out = 1;
			break;
		case  AM_TS_SRC_DMX1:
			out = 2;
			break;
		case  AM_TS_SRC_DMX2:
			out = 4;
			break;
		default:
			break;
		}
		set = 1;
		ciplus_out_sel = out;
	} else if (ciplus_out_sel >= 0 && ciplus_out_sel <= 7) {
		set = 1;
		out = ciplus_out_sel;
	} else {
		pr_error("dsc ciplus out config is invalid\n");
	}

	if (set) {
		/* Set ciplus input source ,
		 * output set 0 means no output. ---> need confirm.
		 * if output set 0 still affects dsc output, we need to disable
		 * ciplus module.
		 */
		data = READ_MPEG_REG(STB_TOP_CONFIG);
		data &= ~(3<<CIPLUS_IN_SEL);
		data |= in << CIPLUS_IN_SEL;
		data &= ~(7<<CIPLUS_OUT_SEL);
		data |= out << CIPLUS_OUT_SEL;
		WRITE_MPEG_REG(STB_TOP_CONFIG, data);
		pr_inf("dsc ciplus in[%x] out[%x] %s\n", in, out,
			(ciplus_out_auto_mode) ? "" : "force");

		set_fec_core_sel(dvb);
	}
}

static void aml_ci_plus_disable(void)
{
	u32 data = 0;

	WRITE_MPEG_REG(CIPLUS_CONFIG, 0);

	data = READ_MPEG_REG(STB_TOP_CONFIG);
	WRITE_MPEG_REG(STB_TOP_CONFIG, data &
			~((3 << CIPLUS_IN_SEL) | (7 << CIPLUS_OUT_SEL)));
}

static int dsc_set_aes_des_sm4_key(struct aml_dsc_channel *ch, int flags,
			enum ca_cw_type type, u8 *key)
{
	unsigned int k0, k1, k2, k3;
	int iv = 0, aes = 0, des = 0;
	int ab_iv = 0, ab_aes = 0, ab_des = 0;
	int from_kl = flags & CA_CW_FROM_KL;
	int algo = 0;

	if (!from_kl) {
		if (get_cpu_type() < MESON_CPU_MAJOR_ID_SM1) {
		k3 = (key[0] << 24) | (key[1] << 16) | (key[2] << 8) | key[3];
		k2 = (key[4] << 24) | (key[5] << 16) | (key[6] << 8) | key[7];
		k1 = (key[8] << 24) | (key[9] << 16) | (key[10] << 8) | key[11];
		k0 = (key[12] << 24) | (key[13] << 16)
			| (key[14] << 8) | key[15];
		} else {
		k0 = (key[0]) | (key[1] << 8) | (key[2] << 16) | (key[3] << 24);
		k1 = (key[4]) | (key[5] << 8) | (key[6] << 16) | (key[7] << 24);
		k2 = (key[8]) | (key[9] << 8) | (key[10] << 16)| (key[11] << 24);
		k3 = (key[12])| (key[13] << 8)| (key[14] << 16)| (key[15] << 24);
		}
	} else
		k0 = k1 = k2 = k3 = 0;

	switch (type) {
	case CA_CW_AES_EVEN:
	case CA_CW_SM4_EVEN:
		ab_aes = (from_kl) ? 0x2 : 0x1;
		if (ch->mode == -1)
			ch->mode = ECB_MODE;
		aes = 1;
		if (type == CA_CW_AES_EVEN)
			algo = ALGO_AES;
		else
			algo = ALGO_SM4;
		break;
	case CA_CW_AES_ODD:
	case CA_CW_SM4_ODD:
		ab_aes = (from_kl) ? 0x1 : 0x2;
		if (ch->mode == -1)
			ch->mode = ECB_MODE;
		aes = 1;
		if (type == CA_CW_AES_ODD)
			algo = ALGO_AES;
		else
			algo = ALGO_SM4;
		break;
	case CA_CW_AES_EVEN_IV:
	case CA_CW_SM4_EVEN_IV:
		ab_iv = 0x1;
		if (ch->mode == -1)
			ch->mode = CBC_MODE;
		iv = 1;
		if (type == CA_CW_AES_EVEN_IV)
			algo = ALGO_AES;
		else
			algo = ALGO_SM4;
		break;
	case CA_CW_AES_ODD_IV:
	case CA_CW_SM4_ODD_IV:
		ab_iv = 0x2;
		if (ch->mode == -1)
			ch->mode = CBC_MODE;
		iv = 1;
		if (type == CA_CW_AES_ODD_IV)
			algo = ALGO_AES;
		else
			algo = ALGO_SM4;
		break;
	case CA_CW_DES_EVEN:
		if (get_cpu_type() < MESON_CPU_MAJOR_ID_SM1) {
			ab_des = 0x1;
		} else {
			ab_aes = 0x1;
		}
		ch->mode = ECB_MODE;
		des = 1;
		algo = ALGO_DES;
		break;
	case CA_CW_DES_ODD:
		if (get_cpu_type() < MESON_CPU_MAJOR_ID_SM1) {
			ab_des = 0x2;
		} else {
			ab_aes = 0x2;
		}
		ch->mode = ECB_MODE;
		algo = ALGO_DES;
		des = 1;
		break;
	default:
		break;
	}

	/* Set endian and cbc/ecb mode */
	if (from_kl)
		aml_ci_plus_config(7, ch->mode, algo);
	else
		aml_ci_plus_config(0, ch->mode, algo);

	/* Write keys to work */
	if (iv || aes) {
		WRITE_MPEG_REG(CIPLUS_KEY0, k0);
		WRITE_MPEG_REG(CIPLUS_KEY1, k1);
		WRITE_MPEG_REG(CIPLUS_KEY2, k2);
		WRITE_MPEG_REG(CIPLUS_KEY3, k3);
	} else {/*des*/
		WRITE_MPEG_REG(CIPLUS_KEY0, k2);
		WRITE_MPEG_REG(CIPLUS_KEY1, k3);
		WRITE_MPEG_REG(CIPLUS_KEY2, 0);
		WRITE_MPEG_REG(CIPLUS_KEY3, 0);
	}
	WRITE_MPEG_REG(CIPLUS_KEY_WR,
		(ch->id << 9) |
				/* bit[11:9] the key of index,
					need match PID index*/
		((from_kl && des) ? (1 << 8) : 0) |
				/* bit[8] des key use cw[127:64]*/
		(0 << 7) |      /* bit[7] aes iv use cw*/
		((from_kl && (aes || des)) ? (1 << 6) : 0) |
				/* bit[6] aes/des key use cw*/
				/* bit[5] write AES IV B value*/
		(ab_iv << 4) |  /* bit[4] write AES IV A value*/
				/* bit[3] write AES B key*/
		(ab_aes << 2) | /* bit[2] write AES A key*/
				/* bit[1] write DES B key*/
		(ab_des));      /* bit[0] write DES A key*/

	return 0;
}

void dsc_release(void)
{
}
/************************* AES DESC************************************/
void set_ciplus_input_source(struct aml_dsc *dsc)
{
	u32 data;
	u32 in = 0;

	if (dsc->id != 0) {
		pr_error("Ciplus set output can only work at dsc0 device\n");
		return;
	}

	switch (dsc->source) {
	case  AM_TS_SRC_DMX0:
		in = 0;
		break;
	case  AM_TS_SRC_DMX1:
		in = 1;
		break;
	case  AM_TS_SRC_DMX2:
		in = 2;
		break;
	default:
		break;
	}

	if (ciplus_out_auto_mode == 1) {
		/* Set ciplus input source */
		data = READ_MPEG_REG(STB_TOP_CONFIG);
		data &= ~(3<<CIPLUS_IN_SEL);
		data |= in << CIPLUS_IN_SEL;
		WRITE_MPEG_REG(STB_TOP_CONFIG, data);
		pr_inf("dsc ciplus in[%x]\n", in);
	}
}

int dsc_enable(struct aml_dsc *dsc, int enable)
{
	if (dsc->id == 0) {
		WRITE_MPEG_REG(STB_TOP_CONFIG,
			READ_MPEG_REG(STB_TOP_CONFIG) &
				~((0x11 << DES_INPUT_SEL)|
				(1 << ENABLE_DES_PL)|
				(1 << ENABLE_DES_PL_CLK)));
	} else if (dsc->id == 1) {
		WRITE_MPEG_REG(COMM_DESC_2_CTL, 0);
	}
	return 0;
}

/*Set section buffer*/
static int dmx_alloc_sec_buffer(struct aml_dmx *dmx)
{
	unsigned long base;
	unsigned long grp_addr[SEC_BUF_GRP_COUNT];
	int grp_len[SEC_BUF_GRP_COUNT];
	int i;

	if (dmx->sec_pages)
		return 0;

	grp_len[0] = (1 << SEC_GRP_LEN_0) * 8;
	grp_len[1] = (1 << SEC_GRP_LEN_1) * 8;
	grp_len[2] = (1 << SEC_GRP_LEN_2) * 8;
	grp_len[3] = (1 << SEC_GRP_LEN_3) * 8;

	dmx->sec_total_len = grp_len[0] + grp_len[1] + grp_len[2] + grp_len[3];
	dmx->sec_pages =
	    __get_free_pages(GFP_KERNEL, get_order(dmx->sec_total_len));
	if (!dmx->sec_pages) {
		pr_error("cannot allocate section buffer %d bytes %d order\n",
			 dmx->sec_total_len, get_order(dmx->sec_total_len));
		return -1;
	}
	dmx->sec_pages_map =
	    dma_map_single(dmx_get_dev(dmx), (void *)dmx->sec_pages,
					 dmx->sec_total_len, DMA_FROM_DEVICE);

	grp_addr[0] = dmx->sec_pages_map;
	grp_addr[1] = grp_addr[0] + grp_len[0];
	grp_addr[2] = grp_addr[1] + grp_len[1];
	grp_addr[3] = grp_addr[2] + grp_len[2];

	dmx->sec_buf[0].addr = dmx->sec_pages;
	dmx->sec_buf[0].len = grp_len[0] / 8;

	for (i = 1; i < SEC_BUF_COUNT; i++) {
		dmx->sec_buf[i].addr =
		    dmx->sec_buf[i - 1].addr + dmx->sec_buf[i - 1].len;
		dmx->sec_buf[i].len = grp_len[i / 8] / 8;
	}

	base = grp_addr[0] & 0xFFFF0000;
	DMX_WRITE_REG(dmx->id, SEC_BUFF_BASE, base >> 16);
	DMX_WRITE_REG(dmx->id, SEC_BUFF_01_START,
		      (((grp_addr[0] - base) >> 8) << 16) |
		       ((grp_addr[1] - base) >> 8));
	DMX_WRITE_REG(dmx->id, SEC_BUFF_23_START,
		      (((grp_addr[2] - base) >> 8) << 16) |
		       ((grp_addr[3] - base) >> 8));
	DMX_WRITE_REG(dmx->id, SEC_BUFF_SIZE,
			SEC_GRP_LEN_0 |
			(SEC_GRP_LEN_1 << 4) |
			(SEC_GRP_LEN_2 << 8) |
			(SEC_GRP_LEN_3 << 12));

	return 0;
}

#ifdef NO_SUB
/*Set subtitle buffer*/
static int dmx_alloc_sub_buffer(struct aml_dvb *dvb, struct aml_dmx *dmx)
{
#ifdef SUB_BUF_DMX
	unsigned long addr;

	if (dmx->sub_pages)
		return 0;

	/*check if use shared buf*/
	if (dvb->sub_pages) {
		dmx->sub_pages = dvb->sub_pages;
		dmx->sub_buf_len = dvb->sub_buf_len;
		dmx->sub_pages_map = dvb->sub_pages_map;
		goto end_alloc;
	}

	dmx->sub_buf_len = 64 * 1024;
	dmx->sub_pages =
	    __get_free_pages(GFP_KERNEL, get_order(dmx->sub_buf_len));
	if (!dmx->sub_pages) {
		pr_error("cannot allocate subtitle buffer\n");
		return -1;
	}
	dmx->sub_pages_map =
	    dma_map_single(dmx_get_dev(dmx), (void *)dmx->sub_pages,
					dmx->sub_buf_len, DMA_FROM_DEVICE);

end_alloc:
	addr = virt_to_phys((void *)dmx->sub_pages);
#ifndef SUB_PARSER
	DMX_WRITE_REG(dmx->id, SB_START, addr >> 12);
	DMX_WRITE_REG(dmx->id, SB_LAST_ADDR, (dmx->sub_buf_len >> 3) - 1);
#endif
	if (dmx->sub_pages != dvb->sub_pages) {
		pr_dbg("sub buff: (%d) %lx %x\n",
			dmx->id, addr, dmx->sub_buf_len);
	}
#endif
	return 0;
}
#ifdef SUB_BUF_SHARED
static int dmx_alloc_sub_buffer_shared(struct aml_dvb *dvb)
{
#ifdef SUB_BUF_DMX
	if (dvb->sub_pages)
		return 0;

	dvb->sub_buf_len = 64 * 1024;
	dvb->sub_pages =
	    __get_free_pages(GFP_KERNEL, get_order(dvb->sub_buf_len));
	if (!dvb->sub_pages) {
		pr_error("cannot allocate subtitle buffer\n");
		return -1;
	}
	dvb->sub_pages_map =
	    dma_map_single(dvb->dev, (void *)dvb->sub_pages,
					dvb->sub_buf_len, DMA_FROM_DEVICE);

	pr_dbg("sub buff shared: %lx %x\n",
		(unsigned long)virt_to_phys((void *)dvb->sub_pages),
		dvb->sub_buf_len);
#endif
	return 0;
}
#endif
#endif /*NO_SUB */

/*Set PES buffer*/
static int dmx_alloc_pes_buffer(struct aml_dvb *dvb, struct aml_dmx *dmx)
{
	unsigned long addr;

	if (dmx->pes_pages)
		return 0;

	/*check if use shared buf*/
	if (dvb->pes_pages) {
		dmx->pes_pages = dvb->pes_pages;
		dmx->pes_buf_len = dvb->pes_buf_len;
		dmx->pes_pages_map = dvb->pes_pages_map;
		goto end_alloc;
	}

	dmx->pes_buf_len = 64 * 1024;
	dmx->pes_pages =
	    __get_free_pages(GFP_KERNEL, get_order(dmx->pes_buf_len));
	if (!dmx->pes_pages) {
		pr_error("cannot allocate pes buffer\n");
		return -1;
	}
	dmx->pes_pages_map =
	    dma_map_single(dmx_get_dev(dmx), (void *)dmx->pes_pages,
					dmx->pes_buf_len, DMA_FROM_DEVICE);
end_alloc:
	addr = virt_to_phys((void *)dmx->pes_pages);
	DMX_WRITE_REG(dmx->id, OB_START, addr >> 12);
	DMX_WRITE_REG(dmx->id, OB_LAST_ADDR, (dmx->pes_buf_len >> 3) - 1);

	if (dmx->pes_pages != dvb->pes_pages) {
		pr_dbg("pes buff: (%d) %lx %x\n",
			dmx->id, addr, dmx->pes_buf_len);
	}
	return 0;
}
#ifdef PES_BUF_SHARED
static int dmx_alloc_pes_buffer_shared(struct aml_dvb *dvb)
{
	if (dvb->pes_pages)
		return 0;

	dvb->pes_buf_len = 64 * 1024;
	dvb->pes_pages =
	    __get_free_pages(GFP_KERNEL, get_order(dvb->pes_buf_len));
	if (!dvb->pes_pages) {
		pr_error("cannot allocate pes buffer\n");
		return -1;
	}
	dvb->pes_pages_map =
	    dma_map_single(dvb->dev, (void *)dvb->pes_pages,
					dvb->pes_buf_len, DMA_FROM_DEVICE);

	pr_dbg("pes buff shared: %lx %x\n",
		(unsigned long)virt_to_phys((void *)dvb->pes_pages),
		dvb->pes_buf_len);
	return 0;
}
#endif

/*Allocate ASYNC FIFO Buffer*/
static unsigned long asyncfifo_alloc_buffer(struct aml_asyncfifo *afifo, int len)
{
	if (!afifo->stored_pages) {
		afifo->stored_pages = __get_free_pages(GFP_KERNEL, get_order(len));
	}

	if (!afifo->stored_pages) {
		pr_error("cannot allocate async fifo buffer\n");
		return 0;
	}
	return afifo->stored_pages;
}
static void asyncfifo_free_buffer(unsigned long buf, int len)
{
}

static int asyncfifo_set_buffer(struct aml_asyncfifo *afifo,
					int len, unsigned long buf)
{
	if (afifo->pages)
		return -1;

	afifo->buf_toggle = 0;
	afifo->buf_read   = 0;
	afifo->buf_len = dmx_get_afifo_size(afifo);
	pr_dbg("async fifo %d buf %lu buf size %d, flush size %d, secure_enable %d, blk.addr %u\n",
			afifo->id, buf, afifo->buf_len, afifo->flush_size, afifo->secure_enable, afifo->blk.addr);

	if ((afifo->flush_size <= 0)
			|| (afifo->flush_size > (len>>1))) {
		afifo->flush_size = len>>1;
	} else if (afifo->flush_size < 128) {
		afifo->flush_size = 128;
	} else {
		int fsize;

		for (fsize = 128; fsize < (len>>1); fsize <<= 1) {
			if (fsize >= afifo->flush_size)
				break;
		}

		afifo->flush_size = fsize;
	}

	afifo->pages = buf;
	if (!afifo->pages)
		return -1;

	afifo->pages_map = dma_map_single(asyncfifo_get_dev(afifo),
			(void *)afifo->pages, len, DMA_FROM_DEVICE);

	return 0;
}
static void asyncfifo_put_buffer(struct aml_asyncfifo *afifo)
{
	if (afifo->pages) {
		dma_unmap_single(asyncfifo_get_dev(afifo),
			afifo->pages_map, asyncfifo_buf_len, DMA_FROM_DEVICE);
		asyncfifo_free_buffer(afifo->pages, asyncfifo_buf_len);
		afifo->pages_map = 0;
		afifo->pages = 0;
	}
}

int async_fifo_init(struct aml_asyncfifo *afifo, int initirq,
			int buf_len, unsigned long buf)
{
	int ret = 0;
	int irq;

	if (afifo->init)
		return -1;

	afifo->source  = AM_DMX_MAX;
	afifo->pages = 0;
	afifo->buf_toggle = 0;
	afifo->buf_read = 0;
	afifo->buf_len = 0;

	if (afifo->asyncfifo_irq == -1) {
		pr_error("no irq for ASYNC_FIFO%d\n", afifo->id);
		/*Do not return error*/
		return -1;
	}

	tasklet_init(&afifo->asyncfifo_tasklet,
			dvr_irq_bh_handler, (unsigned long)afifo);
	if (initirq)
		irq = request_irq(afifo->asyncfifo_irq,	dvr_irq_handler,
				IRQF_SHARED|IRQF_TRIGGER_RISING,
				"dvr irq", afifo);
	else
		enable_irq(afifo->asyncfifo_irq);

	/*alloc buffer*/
	ret = asyncfifo_set_buffer(afifo, buf_len, buf);

	afifo->init = 1;

	return ret;
}

int async_fifo_deinit(struct aml_asyncfifo *afifo, int freeirq)
{
	struct aml_dvb *dvb = afifo->dvb;
	unsigned long flags;

	if (!afifo->init)
		return 0;

	spin_lock_irqsave(&dvb->slock, flags);
	CLEAR_ASYNC_FIFO_REG_MASK(afifo->id, REG1, 1 << ASYNC_FIFO_FLUSH_EN);
	CLEAR_ASYNC_FIFO_REG_MASK(afifo->id, REG2, 1 << ASYNC_FIFO_FILL_EN);
	spin_unlock_irqrestore(&dvb->slock, flags);

	asyncfifo_put_buffer(afifo);

	afifo->source  = AM_DMX_MAX;
	afifo->buf_toggle = 0;
	afifo->buf_read = 0;
	afifo->buf_len = 0;

	if (afifo->asyncfifo_irq != -1) {
		if (freeirq)
			free_irq(afifo->asyncfifo_irq, afifo);
		else
			disable_irq(afifo->asyncfifo_irq);
	}
	tasklet_kill(&afifo->asyncfifo_tasklet);

	afifo->init = 0;

	return 0;
}

static int _dmx_smallsec_enable(struct aml_smallsec *ss, int bufsize)
{
	if (!ss->buf) {

		ss->buf = __get_free_pages(GFP_KERNEL,
					get_order(bufsize));
		if (!ss->buf) {
			pr_error("cannot allocate smallsec buffer\n"
				"%d bytes %d order\n",
				 bufsize, get_order(bufsize));
			return -1;
		}
		ss->buf_map = dma_map_single(dmx_get_dev(ss->dmx),
						(void *)ss->buf,
						 bufsize, DMA_FROM_DEVICE);
	}

	DMX_WRITE_REG(ss->dmx->id, DEMUX_SMALL_SEC_ADDR,
				ss->buf_map);
	DMX_WRITE_REG(ss->dmx->id, DEMUX_SMALL_SEC_CTL,
				((((bufsize>>8)-1)&0xff)<<24) |
				(1<<1) |/*enable reset the wr ptr*/
				(1<<0));

	ss->bufsize = bufsize;
	ss->enable = 1;

	pr_inf("demux%d smallsec buf start: %lx, size: %d\n",
		ss->dmx->id, ss->buf, ss->bufsize);
	return 0;
}

static int _dmx_smallsec_disable(struct aml_smallsec *ss)
{
	DMX_WRITE_REG(ss->dmx->id, DEMUX_SMALL_SEC_CTL, 0);
	if (ss->buf) {
		dma_unmap_single(dmx_get_dev(ss->dmx), ss->buf_map,
				ss->bufsize, DMA_FROM_DEVICE);
		free_pages(ss->buf, get_order(ss->bufsize));
		ss->buf = 0;
		ss->buf_map = 0;
	}
	ss->enable = 0;
	pr_inf("demux%d smallsec buf disable\n", ss->dmx->id);
	return 0;
}

static int dmx_smallsec_set(struct aml_smallsec *ss, int enable, int bufsize,
				int force)
{
	if (!enable) {/*disable*/

		if (ss->enable || force)
			_dmx_smallsec_disable(ss);

	} else {/*enable*/

		if (bufsize < 0)
			bufsize = SS_BUFSIZE_DEF;
		else if (!bufsize)
			bufsize = ss->bufsize;
		else {
			/*unit:FF max:FF00*/
			bufsize &= ~0xFF;
			bufsize &= 0x1FF00;
		}

		if ((ss->enable && (bufsize != ss->bufsize)) || force)
			_dmx_smallsec_disable(ss);

		if (!ss->enable)
			_dmx_smallsec_enable(ss, bufsize);
	}

	return 0;
}

static int _dmx_timeout_enable(struct aml_dmxtimeout *dto, int timeout,
						int ch_dis, int match)
{

	DMX_WRITE_REG(dto->dmx->id, DEMUX_INPUT_TIMEOUT_C, ch_dis);
	DMX_WRITE_REG(dto->dmx->id, DEMUX_INPUT_TIMEOUT,
				((!!match)<<31) |
				(timeout&0x7fffffff));

	dto->ch_disable = ch_dis;
	dto->match = match;
	dto->timeout = timeout;
	dto->trigger = 0;
	dto->enable = 1;

	pr_inf("demux%d timeout enable:timeout(%d),ch(0x%x),match(%d)\n",
		dto->dmx->id, dto->timeout, dto->ch_disable, dto->match);

	return 0;
}
static int _dmx_timeout_disable(struct aml_dmxtimeout *dto)
{

	DMX_WRITE_REG(dto->dmx->id, DEMUX_INPUT_TIMEOUT, 0);
	dto->enable = 0;
	dto->trigger = 0;
	pr_inf("demux%d timeout disable\n", dto->dmx->id);

	return 0;
}

static int dmx_timeout_set(struct aml_dmxtimeout *dto, int enable,
				int timeout, int ch_dis, int match,
				int force)
{

	if (!enable) {/*disable*/

		if (dto->enable || force)
			_dmx_timeout_disable(dto);

	} else {/*enable*/

		if (timeout < 0) {
			timeout = DTO_TIMEOUT_DEF;
			ch_dis = DTO_CHDIS_VAS;
			match = dto->match;
		} else if (!timeout) {
			timeout = dto->timeout;
			ch_dis = dto->ch_disable;
			match = dto->match;
		}

		if ((dto->enable && (timeout != dto->timeout))
			|| force)
			_dmx_timeout_disable(dto);

		if (!dto->enable)
			_dmx_timeout_enable(dto, timeout, ch_dis, match);
	}

	return 0;
}

/*Initialize the registers*/
static int dmx_init(struct aml_dmx *dmx)
{
	struct aml_dvb *dvb = (struct aml_dvb *)dmx->demux.priv;
	int irq;
	int ret = 0;
	char buf[32];
	u32 value = 0;

	if (dmx->init)
		return 0;

	pr_dbg("[dmx_kpi] %s Enter\n", __func__);

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "asyncfifo_buf_len");
	ret = of_property_read_u32(dvb->pdev->dev.of_node, buf, &value);
	if (!ret) {
		pr_inf("%s: 0x%x\n", buf, value);
		asyncfifo_buf_len = value;
	}
	/*Register irq handlers */
	if (dmx->dmx_irq != -1) {
		pr_dbg("request irq\n");
		tasklet_init(&dmx->dmx_tasklet,
				dmx_irq_bh_handler,
				(unsigned long)dmx);
		irq = request_irq(dmx->dmx_irq,	dmx_irq_handler,
				IRQF_SHARED|IRQF_TRIGGER_RISING,
				"dmx irq", dmx);
	}

	/*Allocate buffer */
	if (dmx_alloc_sec_buffer(dmx) < 0)
		return -1;
#ifdef NO_SUB
#ifdef SUB_BUF_SHARED
	if (dmx_alloc_sub_buffer_shared(dvb) < 0)
		return -1;
#endif
	if (dmx_alloc_sub_buffer(dvb, dmx) < 0)
		return -1;
#endif
#ifdef PES_BUF_SHARED
	if (dmx_alloc_pes_buffer_shared(dvb) < 0)
		return -1;
#endif
	if (dmx_alloc_pes_buffer(dvb, dmx) < 0)
		return -1;
	/*Reset the hardware */
	if (!dvb->dmx_init) {
		timer_setup(&dvb->watchdog_timer, section_buffer_watchdog_func,0);
#ifdef ENABLE_SEC_BUFF_WATCHDOG
		mod_timer(&dvb->watchdog_timer,jiffies + msecs_to_jiffies(WATCHDOG_TIMER));
#endif
		dmx_reset_hw(dvb);
	}

	dvb->dmx_init++;

	memset(dmx->sec_buf_watchdog_count, 0,
	       sizeof(dmx->sec_buf_watchdog_count));

	dmx->om_status_error_count = 0;
	dmx->init = 1;
	pr_dbg("[dmx_kpi] %s Exit\n", __func__);
	return 0;
}

/*Release the resource*/
static int dmx_deinit(struct aml_dmx *dmx)
{
	struct aml_dvb *dvb = (struct aml_dvb *)dmx->demux.priv;
	pr_dbg("[dmx_kpi] %s Enter\n", __func__);
	if (!dmx->init)
		return 0;

	DMX_WRITE_REG(dmx->id, DEMUX_CONTROL, 0);

	dvb->dmx_init--;

	/*Reset the hardware */
	if (!dvb->dmx_init) {
		dmx_reset_hw(dvb);
#ifdef ENABLE_SEC_BUFF_WATCHDOG
		del_timer_sync(&dvb->watchdog_timer);
#endif
	}

	if (dmx->sec_pages) {
		dma_unmap_single(dmx_get_dev(dmx), dmx->sec_pages_map,
				dmx->sec_total_len, DMA_FROM_DEVICE);
		free_pages(dmx->sec_pages, get_order(dmx->sec_total_len));
		dmx->sec_pages = 0;
		dmx->sec_pages_map = 0;
	}
#ifdef NO_SUB
#ifdef SUB_BUF_DMX
#ifdef SUB_BUF_SHARED
	if (dvb->sub_pages) {
		dma_unmap_single(dvb->dev, dvb->sub_pages_map,
				dvb->sub_buf_len, DMA_FROM_DEVICE);
		free_pages(dvb->sub_pages, get_order(dvb->sub_buf_len));
		dvb->sub_pages = 0;
	}
	dmx->sub_pages = 0;
#else
	if (dmx->sub_pages) {
		dma_unmap_single(dmx_get_dev(dmx), dmx->sub_pages_map,
				dmx->sub_buf_len, DMA_FROM_DEVICE);
		free_pages(dmx->sub_pages, get_order(dmx->sub_buf_len));
		dmx->sub_pages = 0;
	}
#endif
#endif
#endif
#ifdef PES_BUF_SHARED
	if (dvb->pes_pages) {
		dma_unmap_single(dvb->dev, dvb->pes_pages_map,
				dvb->pes_buf_len, DMA_FROM_DEVICE);
		free_pages(dvb->pes_pages, get_order(dvb->pes_buf_len));
		dvb->pes_pages = 0;
	}
	dmx->pes_pages = 0;
#else
	if (dmx->pes_pages) {
		dma_unmap_single(dmx_get_dev(dmx), dmx->pes_pages_map,
				dmx->pes_buf_len, DMA_FROM_DEVICE);
		free_pages(dmx->pes_pages, get_order(dmx->pes_buf_len));
		dmx->pes_pages = 0;
	}
#endif
	if (dmx->dmx_irq != -1) {
		free_irq(dmx->dmx_irq, dmx);
		tasklet_kill(&dmx->dmx_tasklet);
	}

	dmx->init = 0;
	pr_dbg("[dmx_kpi] %s Exit\n", __func__);
	return 0;
}

/*Check the record flag*/
static int dmx_get_record_flag(struct aml_dmx *dmx)
{
	int i, linked = 0, record_flag = 0;
	struct aml_dvb *dvb = (struct aml_dvb *)dmx->demux.priv;

	/*Check whether a async fifo connected to this dmx */
	for (i = 0; i < dvb->async_fifo_total_count; i++) {
		if (!dvb->asyncfifo[i].init)
			continue;
		if ((dvb->asyncfifo[i].source == dmx->id)) {
			linked = 1;
			break;
		}
	}

	for (i = 0; i < CHANNEL_COUNT; i++) {
		if (dmx->channel[i].used && dmx->channel[i].dvr_feed) {
			if (!dmx->record) {
				pr_error("dmx_get_record_flag set record dmx->id: %d\n", dmx->id);
				dmx->record = 1;

				if (linked) {
					/*A new record will start,
					 *   must reset the async fifos for
					 * linking the right demux
					 */
					reset_async_fifos(dvb);
				}
			}
			if (linked)
				record_flag = 1;
			goto find_done;
		}
	}

	if (dmx->record) {
		pr_error("dmx_get_record_flag clear record dmx->id: %d\n", dmx->id);
		dmx->record = 0;
		if (linked) {
			/*A record will stop, reset the async fifos
			 *for linking the right demux
			 */
			reset_async_fifos(dvb);
		}
	}

find_done:
	return record_flag;
}

static void dmx_cascade_set(int cur_dmx, int source) {
	int fec_sel_demux = 0;
	int data;

	switch (source) {
		case AM_TS_SRC_DMX0:
		case AM_TS_SRC_DMX1:
		case AM_TS_SRC_DMX2:
			fec_sel_demux = source -AM_TS_SRC_DMX0;
			break;
		default:
			fec_sel_demux = cur_dmx;
			break;
	}

	data = READ_MPEG_REG(TS_TOP_CONFIG1);
	data &= ~(0x3 << (cur_dmx*2));
	data |= (fec_sel_demux << (cur_dmx*2));
	WRITE_MPEG_REG(TS_TOP_CONFIG1,data);

	pr_dbg("%s id:%d, source:%d data:0x%0x\n",__FUNCTION__,cur_dmx,fec_sel_demux,data);
}

/*Enable the demux device*/
static int dmx_enable(struct aml_dmx *dmx)
{
	struct aml_dvb *dvb = (struct aml_dvb *)dmx->demux.priv;
	int fec_sel, hi_bsf, fec_ctrl, record;
	int fec_core_sel = 0;
	int set_stb = 0, fec_s = 0;
	int s2p_id;
	u32 invert0 = 0, invert1 = 0, invert2 = 0, fec_s0 = 0, fec_s1 = 0, fec_s2 = 0;
	u32 use_sop = 0;
	int i = 0;

	record = dmx_get_record_flag(dmx);
	if (use_of_sop == 1) {
		use_sop = 1;
		pr_inf("dmx use of sop input\r\n");
	}
	switch (dmx->source) {
	case AM_TS_SRC_TS0:
		fec_sel = 0;
		fec_ctrl = dvb->ts[0].control;
		record = record ? 1 : 0;
		break;
	case AM_TS_SRC_TS1:
		fec_sel = 1;
		fec_ctrl = dvb->ts[1].control;
		record = record ? 1 : 0;
		break;
	case AM_TS_SRC_TS2:
		fec_sel = 2;
		fec_ctrl = dvb->ts[2].control;
		record = record ? 1 : 0;
		break;
	case AM_TS_SRC_TS3:
		fec_sel = 3;
		fec_ctrl = dvb->ts[3].control;
		record = record ? 1 : 0;
		break;
	case AM_TS_SRC_S_TS0:
	case AM_TS_SRC_S_TS1:
	case AM_TS_SRC_S_TS2:
		s2p_id = 0;
		fec_ctrl = 0;
		if (dmx->source == AM_TS_SRC_S_TS0) {
			s2p_id = 0;
		} else if (dmx->source == AM_TS_SRC_S_TS1) {
			s2p_id = 1;
		} else if (dmx->source == AM_TS_SRC_S_TS2) {
			s2p_id = 2;
		}
		for (i = 0; i < dvb->s2p_total_count; i++) {
			if (dvb->ts[i].s2p_id == s2p_id) {
				fec_ctrl = dvb->ts[i].control;
			}
		}
		fec_sel = 6 - s2p_id;
		record = record ? 1 : 0;
		set_stb = 1;
		fec_s = dmx->source - AM_TS_SRC_S_TS0;
		break;
	case AM_TS_SRC_HIU:
		fec_sel = 7;
		fec_ctrl = 0;
		break;
	case AM_TS_SRC_HIU1:
		fec_sel = 8;
		fec_ctrl = 0;
		break;
	case AM_TS_SRC_DMX0:
	case AM_TS_SRC_DMX1:
	case AM_TS_SRC_DMX2:
		fec_sel = -1;
		fec_ctrl = 0;
		record = record ? 1 : 0;
		break;
	default:
		fec_sel = 0;
		fec_ctrl = 0;
		record = 0;
		break;
	}

	if (dmx->channel[0].used || dmx->channel[1].used) {
		hi_bsf = 1;
		if (fec_sel == 8) {
			hi_bsf = 2; /*hi_bsf select hiu1*/
		}
	}else {
		hi_bsf = 0;
	}
	if ((dvb->dsc[0].dst != -1)
	    && ((dvb->dsc[0].dst - AM_TS_SRC_DMX0) == dmx->id))
		fec_core_sel = 1;

	if ((dvb->dsc[1].dst != -1)
	    && ((dvb->dsc[1].dst - AM_TS_SRC_DMX0) == dmx->id))	{
		int des_in, des_out, en_des = 0;

		switch (dvb->dsc[1].source) {
		case AM_TS_SRC_DMX0:
			des_in = 0;
			en_des = 1;
			break;
		case AM_TS_SRC_DMX1:
			des_in = 1;
			en_des = 1;
			break;
		case AM_TS_SRC_DMX2:
			des_in = 2;
			en_des = 1;
			break;
		default:
			des_in = 0;
			en_des = 0;
			break;
		}

		switch (dvb->dsc[1].dst) {
		case AM_TS_SRC_DMX0:
			des_out = 1;
			break;
		case AM_TS_SRC_DMX1:
			des_out = 2;
			break;
		case AM_TS_SRC_DMX2:
			des_out = 4;
			break;
		default:
			des_out = 0;
			break;
		}

		if (!des_out)
			en_des = 0;

		WRITE_MPEG_REG(COMM_DESC_2_CTL,
				(6 << 8) |/*des_out_dly_2*/
				((!!en_des) << 6) |/* des_pl_clk_2*/
				((!!en_des) << 5) |/* des_pl_2*/
				(des_out << 2) |/*use_des_2*/
				(des_in)/*des_i_sel_2*/
				);
		fec_core_sel = 1;
		pr_dbg("dsc2 ctrl: 0x%x\n", READ_MPEG_REG(COMM_DESC_2_CTL));
	}

	pr_dbg("[dmx-%d]src: %d, rec: %d, hi_bsf: %d, dsc: %d\n",
	       dmx->id, dmx->source, record, hi_bsf, fec_core_sel);

	if (dmx->chan_count) {
		if (set_stb) {
			u32 v = READ_MPEG_REG(STB_TOP_CONFIG);
			int i;

			for (i = 0; i < dvb->ts_in_total_count; i++) {
				if (dvb->ts[i].s2p_id == 0)
					fec_s0 = i;
				else if (dvb->ts[i].s2p_id == 1)
					fec_s1 = i;
				else if (dvb->ts[i].s2p_id == 2)
					fec_s2 = i;
			}

			invert0 = dvb->s2p[0].invert;
			invert1 = dvb->s2p[1].invert;

			v &= ~((0x3 << S2P0_FEC_SERIAL_SEL) |
			       (0x1f << INVERT_S2P0_FEC_CLK) |
			       (0x3 << S2P1_FEC_SERIAL_SEL) |
			       (0x1f << INVERT_S2P1_FEC_CLK));

			v |= (fec_s0 << S2P0_FEC_SERIAL_SEL) |
			    (invert0 << INVERT_S2P0_FEC_CLK) |
			    (fec_s1 << S2P1_FEC_SERIAL_SEL) |
			    (invert1 << INVERT_S2P1_FEC_CLK);
			WRITE_MPEG_REG(STB_TOP_CONFIG, v);

			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_TL1) {
			    invert2 = dvb->s2p[2].invert;

			//add s2p2 config
			v = READ_MPEG_REG(STB_S2P2_CONFIG);
			v &= ~((0x3 << S2P2_FEC_SERIAL_SEL) |
			       (0x1f << INVERT_S2P2_FEC_CLK));
			    v |= (fec_s2 << S2P2_FEC_SERIAL_SEL) |
				   (invert2 << INVERT_S2P2_FEC_CLK);
			    WRITE_MPEG_REG(STB_S2P2_CONFIG, v);
			}
		}

		/*Initialize the registers */
		DMX_WRITE_REG(dmx->id, STB_INT_MASK, DEMUX_INT_MASK);
		DMX_WRITE_REG(dmx->id, DEMUX_MEM_REQ_EN,
#ifdef USE_AHB_MODE
			      (1 << SECTION_AHB_DMA_EN) |
			      (0 << SUB_AHB_DMA_EN) |
			      (1 << OTHER_PES_AHB_DMA_EN) |
#endif
			      (1 << SECTION_PACKET) |
			      (1 << VIDEO_PACKET) |
			      (1 << AUDIO_PACKET) |
			      (1 << SUB_PACKET) |
			      (1 << SCR_ONLY_PACKET) |
				(1 << OTHER_PES_PACKET));
		DMX_WRITE_REG(dmx->id, PES_STRONG_SYNC, 0x1234);
		DMX_WRITE_REG(dmx->id, DEMUX_ENDIAN,
			      (1<<SEPERATE_ENDIAN) |
			      (0<<OTHER_PES_ENDIAN) |
			      (7<<SCR_ENDIAN) |
			      (7<<SUB_ENDIAN) |
			      (7<<AUDIO_ENDIAN) |
			      (7<<VIDEO_ENDIAN) |
			      (7 << OTHER_ENDIAN) |
			      (7 << BYPASS_ENDIAN) | (0 << SECTION_ENDIAN));
		if (fec_sel != 8) {
			DMX_WRITE_REG(dmx->id, TS_HIU_CTL,
			   (hi_bsf << USE_HI_BSF_INTERFACE));
		} else {
			DMX_WRITE_REG(dmx->id, TS_HIU_CTL,
				  (1 << PDTS_WR_SEL) |
			   (hi_bsf << USE_HI_BSF_INTERFACE));
		}

		if (!fec_core_sel) {
			u32 cfg = READ_MPEG_REG(CIPLUS_CONFIG);

			if (cfg & (1 << CNTL_ENABLE)) {
				if (!ciplus_out_auto_mode) {
					int mask = 1 << dmx->id;

					if (ciplus_out_sel & mask)
						fec_core_sel = 1;
				}
			}
		}

		if (fec_sel == -1) {
			dmx_cascade_set(dmx->id,dmx->source);
			DMX_WRITE_REG(dmx->id, FEC_INPUT_CONTROL,
			      (fec_core_sel << FEC_CORE_SEL) |
			      (0 << FEC_SEL) | (fec_ctrl << 0));
		} else {
			dmx_cascade_set(dmx->id,dmx->source);
			if (fec_sel != 8) {
				DMX_WRITE_REG(dmx->id, FEC_INPUT_CONTROL,
				      (fec_core_sel << FEC_CORE_SEL) |
				      (fec_sel << FEC_SEL) | (fec_ctrl << 0));
			} else {
				DMX_WRITE_REG(dmx->id, FEC_INPUT_CONTROL,
				      (fec_core_sel << FEC_CORE_SEL) |
				      (1 << FEC_SEL_3BIT) | (fec_ctrl << 0));
			}
		}
		DMX_WRITE_REG(dmx->id, STB_OM_CTL,
			      (0x40 << MAX_OM_DMA_COUNT) |
			      (0x7f << LAST_OM_ADDR));

		/*RECORDER_STREAM depends on video2*/
		/*VIDEO_STREAM_ID: video2_stream_id (bit[31:16])*/
		/*DEMUX_CONTROL:
		  bit[25] video2_en
		  bit[24:22] video2_type*/
		#define VIDEO2_FOR_RECORDER_STREAM (1 << 25 | 7 << 22)

		DMX_WRITE_REG(dmx->id, VIDEO_STREAM_ID,
				((record) ? 0xFFFF0000 : 0));

		DMX_WRITE_REG(dmx->id, DEMUX_CONTROL,
			      (0 << BYPASS_USE_RECODER_PATH) |
			      (0 << INSERT_AUDIO_PES_STRONG_SYNC) |
			      (0 << INSERT_VIDEO_PES_STRONG_SYNC) |
			      (0 << OTHER_INT_AT_PES_BEGINING) |
			      (0 << DISCARD_AV_PACKAGE) |
			      ((!!dmx->dump_ts_select) << TS_RECORDER_SELECT) |
			      (record << TS_RECORDER_ENABLE) |
			      (1 << KEEP_DUPLICATE_PACKAGE) |
			      (1 << SECTION_END_WITH_TABLE_ID) |
			      (1 << ENABLE_FREE_CLK_FEC_DATA_VALID) |
			      (1 << ENABLE_FREE_CLK_STB_REG) |
			      (1 << STB_DEMUX_ENABLE) |
			      (use_sop << NOT_USE_OF_SOP_INPUT) |
			      ((record)? VIDEO2_FOR_RECORDER_STREAM : 0));
		pr_dbg("dmx control[%#x]\n",
			DMX_READ_REG(dmx->id, DEMUX_CONTROL));
	} else {
		DMX_WRITE_REG(dmx->id, STB_INT_MASK, 0);
		/* if disable FEC_INPUT_CONTROL, background and unattended record will fail */
		DMX_WRITE_REG(dmx->id, DEMUX_CONTROL, 0);
		//dmx not used, but it can cascade for other dmx
		if ((dmx->source == AM_TS_SRC_DMX0 ||
			dmx->source == AM_TS_SRC_DMX1 ||
			dmx->source == AM_TS_SRC_DMX2 ) &&
			(dmx->id != dmx->source-AM_TS_SRC_DMX0))
			dmx_cascade_set(dmx->id,dmx->source);
	}
	return 0;
}

static int dmx_set_misc(struct aml_dmx *dmx, int hi_bsf, int en_dsc)
{
	if (hi_bsf >= 0) {
		DMX_WRITE_REG(dmx->id, TS_HIU_CTL,
					hi_bsf ?
					(DMX_READ_REG(dmx->id, TS_HIU_CTL) |
					(1 << USE_HI_BSF_INTERFACE))
					:
					(DMX_READ_REG(dmx->id, TS_HIU_CTL) &
					(~(1 << USE_HI_BSF_INTERFACE))));
	}

	if (en_dsc >= 0) {
		DMX_WRITE_REG(dmx->id, FEC_INPUT_CONTROL,
				en_dsc ?
				(DMX_READ_REG(dmx->id, FEC_INPUT_CONTROL) |
				(1 << FEC_CORE_SEL))
				:
				(DMX_READ_REG(dmx->id, FEC_INPUT_CONTROL) &
				(~(1 << FEC_CORE_SEL))));
	}

	return 0;
}

static int dmx_set_misc_id(struct aml_dvb *dvb, int id, int hi_bsf, int en_dsc)
{
	return dmx_set_misc(&dvb->dmx[id], hi_bsf, en_dsc);
}

/*Get the channel's ID by its PID*/
static int dmx_get_chan(struct aml_dmx *dmx, int pid)
{
	int id;

	for (id = 0; id < CHANNEL_COUNT; id++) {
		if (dmx->channel[id].used && dmx->channel[id].pid == pid)
			return id;
	}

	return -1;
}

/*Get the channel's target*/
static u32 dmx_get_chan_target(struct aml_dmx *dmx, int cid)
{
	u32 type;

	if (!dmx->channel[cid].used)
		return (0x7 << PID_TYPE) | g_chan_def_pid;

	if (dmx->channel[cid].type == DMX_TYPE_SEC) {
		type = SECTION_PACKET;
	} else {
		switch (dmx->channel[cid].pes_type) {
		case DMX_PES_AUDIO:
			type = AUDIO_PACKET;
			break;
		case DMX_PES_VIDEO:
			type = VIDEO_PACKET;
			break;
		case DMX_PES_SUBTITLE:
		case DMX_PES_TELETEXT:
			type = SUB_PACKET;
			break;
		case DMX_PES_PCR:
			type = SCR_ONLY_PACKET;
			break;
		case DMX_PES_AUDIO3:
			type = OTHER_PES_PACKET;
			break;
		default:
			type = RECORDER_STREAM;
			break;
		}
	}
	dmx->channel[cid].pkt_type = type;

	pr_dbg("chan target: %x %x\n", type, dmx->channel[cid].pid);
	return (type << PID_TYPE) | dmx->channel[cid].pid;
}

/*Get the advance value of the channel*/
static inline u32 dmx_get_chan_advance(struct aml_dmx *dmx, int cid)
{
	return 0;
}

/*Set the channel registers*/
static int dmx_set_chan_regs(struct aml_dmx *dmx, int cid)
{
	u32 data, addr, advance, max;

	pr_dbg("set channel (id:%d PID:0x%x) registers\n", cid,
	       dmx->channel[cid].pid);

	while (DMX_READ_REG(dmx->id, FM_WR_ADDR) & 0x8000)
		udelay(1);

	if (cid & 1) {
		data =
		    (dmx_get_chan_target(dmx, cid - 1) << 16) |
		    dmx_get_chan_target(dmx, cid);
		advance =
		    (dmx_get_chan_advance(dmx, cid) << 8) |
		    dmx_get_chan_advance(dmx, cid - 1);

		if (dmx->channel[cid - 1].used)
			set_debug_dmx_chanpids_types(dmx->id, cid - 1,
				dmx->channel[cid - 1].pkt_type);
	} else {
		data =
		    (dmx_get_chan_target(dmx, cid) << 16) |
		    dmx_get_chan_target(dmx, cid + 1);
		advance =
		    (dmx_get_chan_advance(dmx, cid + 1) << 8) |
		    dmx_get_chan_advance(dmx, cid);

		if (dmx->channel[cid + 1].used)
			set_debug_dmx_chanpids_types(dmx->id, cid + 1,
				dmx->channel[cid + 1].pkt_type);
	}
	addr = cid >> 1;
	DMX_WRITE_REG(dmx->id, FM_WR_DATA, data);
	DMX_WRITE_REG(dmx->id, FM_WR_ADDR, (advance << 16) | 0x8000 | addr);

	pr_dbg("write fm %x:%x\n", (advance << 16) | 0x8000 | addr, data);

	for (max = CHANNEL_COUNT - 1; max > 0; max--) {
		if (dmx->channel[max].used)
			break;
	}

	data = DMX_READ_REG(dmx->id, MAX_FM_COMP_ADDR) & 0xF0;
	DMX_WRITE_REG(dmx->id, MAX_FM_COMP_ADDR, data | (max >> 1));

	pr_dbg("write fm comp %x\n", data | (max >> 1));

	if (DMX_READ_REG(dmx->id, OM_CMD_STATUS) & 0x8e00) {
		pr_error("warning: send cmd %x\n",
			 DMX_READ_REG(dmx->id, OM_CMD_STATUS));
	}

	if (cid == 0) {
		video_pts = 0;
		first_video_pts = 0;
	}
	else if (cid == 1) {
		audio_pts = 0;
		first_audio_pts = 0;
	}

	if (dmx->channel[cid].used)
		set_debug_dmx_chanpids_types(dmx->id, cid,
			dmx->channel[cid].pkt_type);
	return 0;
}

/*Get the filter target*/
static int dmx_get_filter_target(struct aml_dmx *dmx, int fid, u32 *target,
				 u8 *advance)
{
	struct dmx_section_filter *filter;
	struct aml_filter *f;
	int i, cid, neq_bytes;

	fid = fid & 0xFFFF;
	f = &dmx->filter[fid];

	if (!f->used) {
		target[0] = 0x1fff;
		advance[0] = 0;
		for (i = 1; i < FILTER_LEN; i++) {
			target[i] = 0x9fff;
			advance[i] = 0;
		}
		return 0;
	}

	cid = f->chan_id;
	filter = f->filter;

	neq_bytes = 0;
	if (filter->filter_mode[0] != 0xFF) {
		neq_bytes = 2;
	} else {
		for (i = 3; i < FILTER_LEN; i++) {
			if (filter->filter_mode[i] != 0xFF)
				neq_bytes++;
		}
	}

	f->neq = 0;

	for (i = 0; i < FILTER_LEN; i++) {
		u8 value = filter->filter_value[i];
		u8 mask = filter->filter_mask[i];
		u8 mode = filter->filter_mode[i];
		u8 mb, mb1, nb, v, t, adv = 0;

		if (!i) {
			mb = 1;
			mb1 = 1;
			v = 0;
			if ((mode == 0xFF) && mask) {
				t = mask & 0xF0;
				if (t) {
					mb1 = 0;
					adv |= t^0xF0;
				}
				v |= (value & 0xF0) | adv;

				t = mask & 0x0F;
				if (t) {
					mb  = 0;
					adv |= t^0x0F;
				}
				v |= (value & 0x0F) | adv;
			}

			target[i] = (mb << SECTION_FIRSTBYTE_MASKLOW) |
			    (mb1 << SECTION_FIRSTBYTE_MASKHIGH) |
			    (0 << SECTION_FIRSTBYTE_DISABLE_PID_CHECK) |
			    (cid << SECTION_FIRSTBYTE_PID_INDEX) | v;
			advance[i] = adv;
		} else {
			if (i < 3) {
				value = 0;
				mask = 0;
				mode = 0xff;
			}
			mb = 1;
			nb = 0;
			v = 0;

			if ((i >= 3) && mask) {
				if (mode == 0xFF) {
					mb = 0;
					nb = 0;
					adv = mask ^ 0xFF;
					v = value | adv;
				} else {
					if (neq_bytes == 1) {
						mb = 0;
						nb = 1;
						adv = mask ^ 0xFF;
						v = value & ~adv;
					}
				}
			}
			target[i] = (mb << SECTION_RESTBYTE_MASK) |
			    (nb << SECTION_RESTBYTE_MASK_EQ) |
			    (0 << SECTION_RESTBYTE_DISABLE_PID_CHECK) |
			    (cid << SECTION_RESTBYTE_PID_INDEX) | v;
			advance[i] = adv;
		}

		f->value[i] = value;
		f->maskandmode[i] = mask & mode;
		f->maskandnotmode[i] = mask & ~mode;

		if (f->maskandnotmode[i])
			f->neq = 1;
	}

	return 0;
}

/*Set the filter registers*/
static int dmx_set_filter_regs(struct aml_dmx *dmx, int fid)
{
	u32 t1[FILTER_LEN], t2[FILTER_LEN];
	u8 advance1[FILTER_LEN], advance2[FILTER_LEN];
	u32 addr, data, max, adv;
	int i;

	pr_dbg("set filter (id:%d) registers\n", fid);

	if (fid & 1) {
		dmx_get_filter_target(dmx, fid - 1, t1, advance1);
		dmx_get_filter_target(dmx, fid, t2, advance2);
	} else {
		dmx_get_filter_target(dmx, fid, t1, advance1);
		dmx_get_filter_target(dmx, fid + 1, t2, advance2);
	}

	for (i = 0; i < FILTER_LEN; i++) {
		while (DMX_READ_REG(dmx->id, FM_WR_ADDR) & 0x8000)
			udelay(1);

		data = (t1[i] << 16) | t2[i];
		addr = (fid >> 1) | ((i + 1) << 4);
		adv = (advance1[i] << 8) | advance2[i];

		DMX_WRITE_REG(dmx->id, FM_WR_DATA, data);
		DMX_WRITE_REG(dmx->id, FM_WR_ADDR, (adv << 16) | 0x8000 | addr);

		pr_dbg("write fm %x:%x\n", (adv << 16) | 0x8000 | addr, data);
	}

	for (max = FILTER_COUNT - 1; max > 0; max--) {
		if (dmx->filter[max].used)
			break;
	}

	data = DMX_READ_REG(dmx->id, MAX_FM_COMP_ADDR) & 0xF;
	DMX_WRITE_REG(dmx->id, MAX_FM_COMP_ADDR, data | ((max >> 1) << 4));

	pr_dbg("write fm comp %x\n", data | ((max >> 1) << 4));

	if (DMX_READ_REG(dmx->id, OM_CMD_STATUS) & 0x8e00) {
		pr_error("error send cmd %x\n",
			 DMX_READ_REG(dmx->id, OM_CMD_STATUS));
	}

	return 0;
}

/*Clear the filter's buffer*/
static void dmx_clear_filter_buffer(struct aml_dmx *dmx, int fid)
{
	u32 section_busy32 = DMX_READ_REG(dmx->id, SEC_BUFF_READY);
	u32 filter_number;
	int i;

	if (!section_busy32)
		return;

	for (i = 0; i < SEC_BUF_COUNT; i++) {
		if (section_busy32 & (1 << i)) {
			DMX_WRITE_REG(dmx->id, SEC_BUFF_NUMBER, i);
			filter_number =
			    (DMX_READ_REG(dmx->id, SEC_BUFF_NUMBER) >> 8);
			if (filter_number != fid)
				section_busy32 &= ~(1 << i);
		}
	}

	if (section_busy32)
		DMX_WRITE_REG(dmx->id, SEC_BUFF_READY, section_busy32);
}

static void async_fifo_disable(struct aml_asyncfifo *afifo)
{
	pr_inf("AF(%d) disable asyncfifo\n", afifo->id);
	CLEAR_ASYNC_FIFO_REG_MASK(afifo->id, REG1, 1 << ASYNC_FIFO_FLUSH_EN);
	CLEAR_ASYNC_FIFO_REG_MASK(afifo->id, REG2, 1 << ASYNC_FIFO_FILL_EN);
	if (READ_ASYNC_FIFO_REG(afifo->id, REG2) & (1 << ASYNC_FIFO_FILL_EN)
		|| READ_ASYNC_FIFO_REG(afifo->id, REG1)
			& (1 << ASYNC_FIFO_FLUSH_EN)) {
		pr_error("disable failed\n");
	} else
		pr_inf("disable ok\n");
	afifo->buf_toggle = 0;
	afifo->buf_read = 0;
}

static void async_fifo_set_regs(struct aml_asyncfifo *afifo, int source_val)
{
	u32 start_addr = (afifo->secure_enable && afifo->blk.addr)?
			afifo->blk.addr : virt_to_phys((void *)afifo->pages);
	u32 size = afifo->buf_len;
	u32 flush_size = afifo->flush_size;
	int factor = dmx_get_order(size / flush_size);
	u32 old_size, new_size, old_factor, new_factor;
	int old_src, old_en;

	old_en  = READ_ASYNC_FIFO_REG(afifo->id, REG2)
			& (1 << ASYNC_FIFO_FILL_EN);
	old_src =
		(READ_ASYNC_FIFO_REG(afifo->id, REG2) >> ASYNC_FIFO_SOURCE_LSB)
		& 3;

	new_size = (size >> 7) & 0x7fff;
	old_size =
		(READ_ASYNC_FIFO_REG(afifo->id, REG1)
			>> ASYNC_FIFO_FLUSH_CNT_LSB)
		& 0x7fff;

	old_factor =
		(READ_ASYNC_FIFO_REG(afifo->id, REG3)
			>> ASYNC_FLUSH_SIZE_IRQ_LSB)
		& 0x7fff;
	new_factor = ((size >> (factor + 7)) - 1) & 0x7fff;

	pr_inf("AF(%d) [%s] src:0x%x->0x%x size:0x%x->0x%x factor:0x%x->0x%x\n",
		afifo->id,
		old_en? "on" : "off",
		old_src, source_val,
		old_size, new_size,
		old_factor, new_factor);

	if (old_en
		&& (old_src == source_val)
		&& (new_size == old_size)
		&& (old_factor == new_factor))
		return;

	if (old_en) {
		if ((old_size == new_size)
			&& (old_factor == new_factor)) {
			/*only source changed, do not reset all*/
			/* Connect the DEMUX to ASYNC_FIFO */
			WRITE_ASYNC_FIFO_REG(afifo->id, REG2,
				(READ_ASYNC_FIFO_REG(afifo->id, REG2)
				& ~(0x3 << ASYNC_FIFO_SOURCE_LSB))
				| (source_val << ASYNC_FIFO_SOURCE_LSB));
			return;
		} else {
			/*Dynamic change setting is not supported,
			 *should disable it first.
			 *Data discontinue will be here.
			 */
			async_fifo_disable(afifo);
		}
	}

	pr_inf("ASYNC FIFO id=%d, link to DMX%d, start_addr %x, buf_size %d,"
		"source value 0x%x, factor %d\n",
		afifo->id, afifo->source, start_addr, size, source_val, factor);

	/* Destination address */
	WRITE_ASYNC_FIFO_REG(afifo->id, REG0, start_addr);

	/* Setup flush parameters */
	WRITE_ASYNC_FIFO_REG(afifo->id, REG1,
			(0 << ASYNC_FIFO_TO_HIU) |
			(0 << ASYNC_FIFO_FLUSH) |
			/* don't flush the path */
			(1 << ASYNC_FIFO_RESET) |
			/* reset the path */
			(1 << ASYNC_FIFO_WRAP_EN) |
			/* wrap enable */
			(0 << ASYNC_FIFO_FLUSH_EN) |
			(((size >> 7) & 0x7fff) << ASYNC_FIFO_FLUSH_CNT_LSB));
			/* number of 128-byte blocks to flush */

	/* clear the reset signal */
	WRITE_ASYNC_FIFO_REG(afifo->id, REG1,
		     READ_ASYNC_FIFO_REG(afifo->id,
					REG1) & ~(1 << ASYNC_FIFO_RESET));
	/* Enable flush */
	WRITE_ASYNC_FIFO_REG(afifo->id, REG1,
		     READ_ASYNC_FIFO_REG(afifo->id,
				REG1) | (1 << ASYNC_FIFO_FLUSH_EN));

	/*Setup Fill parameters */
	WRITE_ASYNC_FIFO_REG(afifo->id, REG2,
			     (1 << ASYNC_FIFO_ENDIAN_LSB) |
			     (0 << ASYNC_FIFO_FILL_EN) |
			     (0 << ASYNC_FIFO_FILL_CNT_LSB));
	WRITE_ASYNC_FIFO_REG(afifo->id, REG2,
			READ_ASYNC_FIFO_REG(afifo->id, REG2) |
				(1 << ASYNC_FIFO_FILL_EN));/*Enable fill path*/

	/* generate flush interrupt */
	WRITE_ASYNC_FIFO_REG(afifo->id, REG3,
			(READ_ASYNC_FIFO_REG(afifo->id, REG3) & 0xffff0000) |
				((((size >> (factor + 7)) - 1) & 0x7fff) <<
					ASYNC_FLUSH_SIZE_IRQ_LSB));

	/* Connect the STB DEMUX to ASYNC_FIFO */
	WRITE_ASYNC_FIFO_REG(afifo->id, REG2,
			READ_ASYNC_FIFO_REG(afifo->id, REG2) |
			(source_val << ASYNC_FIFO_SOURCE_LSB));
}

/*Reset the ASYNC FIFOS when a ASYNC FIFO connect to a different DMX*/
static void reset_async_fifos(struct aml_dvb *dvb)
{
	struct aml_asyncfifo *low_dmx_fifo = NULL;
	struct aml_asyncfifo *high_dmx_fifo = NULL;
	struct aml_asyncfifo *highest_dmx_fifo = NULL;
	int i, j;
	int record_enable;

	struct aml_asyncfifo *afifo = NULL;

	for (j = 0; j < DMX_DEV_COUNT; j++) {
		if (!dvb->dmx[j].init)
			continue;

		record_enable = 0;
		for (i = 0; i < dvb->async_fifo_total_count; i++) {
			afifo = &dvb->asyncfifo[i];

			if (!afifo->init)
				continue;

			if (!dvb->dmx[j].record
				|| !(dvb->dmx[j].id == afifo->source))
				continue;

			/*This dmx is linked to the async fifo,
			 *Enable the TS_RECORDER_ENABLE
			 */
			record_enable = 1;
			if (!low_dmx_fifo) {
				low_dmx_fifo = afifo;
			} else if (low_dmx_fifo->source >
				   afifo->source) {
				if (!high_dmx_fifo)
					high_dmx_fifo = low_dmx_fifo;
				else {
					highest_dmx_fifo = high_dmx_fifo;
					high_dmx_fifo = low_dmx_fifo;
				}
				low_dmx_fifo = afifo;
			} else if (low_dmx_fifo->source < afifo->source) {
				if (!high_dmx_fifo)
					high_dmx_fifo = afifo;
				else {
					if (high_dmx_fifo->source >
						afifo->source) {
						highest_dmx_fifo =
							high_dmx_fifo;
						high_dmx_fifo = afifo;
					} else {
						highest_dmx_fifo = afifo;
					}
				}
			}
			break;
		}

		pr_inf("Set DMX%d TS_RECORDER_ENABLE to %d\n", dvb->dmx[j].id,
			record_enable ? 1 : 0);

		if (record_enable) {
			int old_en =
				DMX_READ_REG(dvb->dmx[j].id, DEMUX_CONTROL)
				& (1 << TS_RECORDER_ENABLE);

			if (!old_en) {
				DMX_WRITE_REG(dvb->dmx[j].id, DEMUX_CONTROL,
					DMX_READ_REG(dvb->dmx[j].id,
						DEMUX_CONTROL)
					| (1 << TS_RECORDER_ENABLE));
			}
		} else {
			int old_en =
				DMX_READ_REG(dvb->dmx[j].id, DEMUX_CONTROL)
				& (1 << TS_RECORDER_ENABLE);

			if (old_en) {
				DMX_WRITE_REG(dvb->dmx[j].id, DEMUX_CONTROL,
					DMX_READ_REG(dvb->dmx[j].id,
						DEMUX_CONTROL)
					& (~(1 << TS_RECORDER_ENABLE)));
			}
		}
	}
	pr_inf("reset ASYNC FIFOs\n");
	for (i = 0; i < dvb->async_fifo_total_count; i++) {
		int old;

		afifo = &dvb->asyncfifo[i];

		if (!afifo->init)
			continue;

		old = READ_ASYNC_FIFO_REG(afifo->id, REG2)
			& (1 << ASYNC_FIFO_FILL_EN);

		if (old
			&& (afifo != low_dmx_fifo)
			&& (afifo != high_dmx_fifo)
			&& (afifo != highest_dmx_fifo))
			async_fifo_disable(afifo);
	}

	/*Set the async fifo regs */
	if (low_dmx_fifo) {
		async_fifo_set_regs(low_dmx_fifo, 0x3);

		if (high_dmx_fifo) {
			async_fifo_set_regs(high_dmx_fifo, 0x2);

			if (highest_dmx_fifo)
				async_fifo_set_regs(highest_dmx_fifo, 0x0);
		}
	}
}

/*Reset the demux device*/
void dmx_reset_hw(struct aml_dvb *dvb)
{
	dmx_reset_hw_ex(dvb, 1);
}

/*Reset the demux device*/
void dmx_reset_hw_ex(struct aml_dvb *dvb, int reset_irq)
{
	int id, times;
	u32 pcr_num[DMX_DEV_COUNT];
	u32 pcr_reg[DMX_DEV_COUNT];

	pr_dbg("[dmx_kpi] demux reset begin\n");

	memset(&pcr_reg, 0, sizeof(pcr_reg));
	memset(&pcr_num, 0, sizeof(pcr_num));

	for (id = 0; id < DMX_DEV_COUNT; id++) {
		if (!dvb->dmx[id].init)
			continue;
		pcr_reg[id] = DMX_READ_REG(id, PCR90K_CTL);
		pcr_num[id] = DMX_READ_REG(id, ASSIGN_PID_NUMBER);
		pr_dbg("reset demux, pcr_regs[%d]:0x%x, pcr_num[%d]:0x%x\n", id, pcr_reg[id], id, pcr_num[id]);
		if (reset_irq) {
			if (dvb->dmx[id].dmx_irq != -1)
				disable_irq(dvb->dmx[id].dmx_irq);
			if (dvb->dmx[id].dvr_irq != -1)
				disable_irq(dvb->dmx[id].dvr_irq);
		}
	}
#ifdef ENABLE_SEC_BUFF_WATCHDOG
	if (reset_irq)
		del_timer_sync(&dvb->watchdog_timer);
#endif
	/*RESET_TOP will clear the dsc pid , save all dsc pid that setting in TA*/
	for (id = 0; id < DSC_DEV_COUNT; id++) {
		struct aml_dsc *dsc = &dvb->dsc[id];
		int n;

		for (n = 0; n < DSC_COUNT; n++) {
			struct aml_dsc_channel *ch = &dsc->channel[n];
			{
				ch->id = n;
				dsc_get_pid(ch,&ch->pid);
			}
		}
	}

	WRITE_MPEG_REG(RESET1_REGISTER, RESET_DEMUXSTB);

	for (id = 0; id < DMX_DEV_COUNT; id++) {
		times = 0;
		while (times++ < 1000000) {
			if (!(DMX_READ_REG(id, OM_CMD_STATUS) & 0x01))
				break;
		}
	}

	WRITE_MPEG_REG(STB_TOP_CONFIG, 0);
	WRITE_MPEG_REG(STB_S2P2_CONFIG, 0);

	for (id = 0; id < DMX_DEV_COUNT; id++) {
		u32 version, data;

		if (!dvb->dmx[id].init)
			continue;

		if (reset_irq) {
			if (dvb->dmx[id].dmx_irq != -1)
				enable_irq(dvb->dmx[id].dmx_irq);
			if (dvb->dmx[id].dvr_irq != -1)
				enable_irq(dvb->dmx[id].dvr_irq);
		}
		DMX_WRITE_REG(id, DEMUX_CONTROL, 0x0000);
		version = DMX_READ_REG(id, STB_VERSION);
		DMX_WRITE_REG(id, STB_TEST_REG, version);
		pr_dbg("STB %d hardware version : %d\n", id, version);
		DMX_WRITE_REG(id, STB_TEST_REG, 0x5550);
		data = DMX_READ_REG(id, STB_TEST_REG);
		if (data != 0x5550)
			pr_error("STB %d register access failed\n", id);
		DMX_WRITE_REG(id, STB_TEST_REG, 0xaaa0);
		data = DMX_READ_REG(id, STB_TEST_REG);
		if (data != 0xaaa0)
			pr_error("STB %d register access failed\n", id);
		DMX_WRITE_REG(id, MAX_FM_COMP_ADDR, 0x0000);
		DMX_WRITE_REG(id, STB_INT_MASK, 0);
		DMX_WRITE_REG(id, STB_INT_STATUS, 0xffff);
		DMX_WRITE_REG(id, FEC_INPUT_CONTROL, 0);
	}

	stb_enable(dvb);

	for (id = 0; id < DMX_DEV_COUNT; id++) {
		struct aml_dmx *dmx = &dvb->dmx[id];
		int n;
		unsigned long addr;
		unsigned long base;
		unsigned long grp_addr[SEC_BUF_GRP_COUNT];
		int grp_len[SEC_BUF_GRP_COUNT];

		if (!dvb->dmx[id].init)
			continue;

		if (dmx->sec_pages) {
			grp_len[0] = (1 << SEC_GRP_LEN_0) * 8;
			grp_len[1] = (1 << SEC_GRP_LEN_1) * 8;
			grp_len[2] = (1 << SEC_GRP_LEN_2) * 8;
			grp_len[3] = (1 << SEC_GRP_LEN_3) * 8;

			grp_addr[0] = virt_to_phys((void *)dmx->sec_pages);
			grp_addr[1] = grp_addr[0] + grp_len[0];
			grp_addr[2] = grp_addr[1] + grp_len[1];
			grp_addr[3] = grp_addr[2] + grp_len[2];

			base = grp_addr[0] & 0xFFFF0000;
			DMX_WRITE_REG(dmx->id, SEC_BUFF_BASE, base >> 16);
			DMX_WRITE_REG(dmx->id, SEC_BUFF_01_START,
					(((grp_addr[0] - base) >> 8) << 16) |
					 ((grp_addr[1] - base) >> 8));
			DMX_WRITE_REG(dmx->id, SEC_BUFF_23_START,
					(((grp_addr[2] - base) >> 8) << 16) |
					 ((grp_addr[3] - base) >> 8));
			DMX_WRITE_REG(dmx->id, SEC_BUFF_SIZE,
					SEC_GRP_LEN_0 |
					(SEC_GRP_LEN_1 << 4) |
					(SEC_GRP_LEN_2 << 8) |
					(SEC_GRP_LEN_3 << 12));
		}
#ifdef NO_SUB
#ifndef SUB_PARSER
		if (dmx->sub_pages) {
			addr = virt_to_phys((void *)dmx->sub_pages);
			DMX_WRITE_REG(dmx->id, SB_START, addr >> 12);
			DMX_WRITE_REG(dmx->id, SB_LAST_ADDR,
				      (dmx->sub_buf_len >> 3) - 1);
		}
#endif
#endif
		if (dmx->pes_pages) {
			addr = virt_to_phys((void *)dmx->pes_pages);
			DMX_WRITE_REG(dmx->id, OB_START, addr >> 12);
			DMX_WRITE_REG(dmx->id, OB_LAST_ADDR,
				      (dmx->pes_buf_len >> 3) - 1);
		}

		for (n = 0; n < CHANNEL_COUNT; n++) {
			{
#ifdef NO_SUB
#ifdef SUB_PARSER
				/*
				  check if subtitle channel was running,
				  the parser will be used in amstream also,
				  take care of the buff ptr.
				*/
				u32 v = dmx_get_chan_target(dmx, n);
				if (v != 0xFFFF &&
					(v & (0x7 << PID_TYPE))
						== (SUB_PACKET << PID_TYPE))
					set_subtitle_pes_buffer(dmx);
#endif
#endif
				{
					u32 v = dmx_get_chan_target(dmx, n);
					if (v != 0xFFFF &&
						(v & (0x7 << PID_TYPE))
						==
						(OTHER_PES_PACKET << PID_TYPE))
						pes_off_pre[dmx->id] = 0;
				}
				dmx_set_chan_regs(dmx, n);
			}
		}

		for (n = 0; n < FILTER_COUNT; n++) {
			struct aml_filter *filter = &dmx->filter[n];

			if (filter->used)
				dmx_set_filter_regs(dmx, n);
		}
		dmx_enable(&dvb->dmx[id]);
		dmx_smallsec_set(&dmx->smallsec,
				dmx->smallsec.enable,
				dmx->smallsec.bufsize,
				1);

		dmx_timeout_set(&dmx->timeout,
				dmx->timeout.enable,
				dmx->timeout.timeout,
				dmx->timeout.ch_disable,
				dmx->timeout.match,
				1);
		DMX_WRITE_REG(id, ASSIGN_PID_NUMBER,  pcr_num[id]);
		DMX_WRITE_REG(id, PCR90K_CTL,  pcr_reg[id]);
	}

	for (id = 0; id < DSC_DEV_COUNT; id++) {
		struct aml_dsc *dsc = &dvb->dsc[id];
		int n;

		for (n = 0; n < DSC_COUNT; n++) {
			int flag = 0;
			struct aml_dsc_channel *ch = &dsc->channel[n];
			{
				ch->work_mode = -1;
				//if ta setting pid, used will 0
				if (ch->pid != 0x1fff && !ch->used) {
					flag = 1;
					ch->used = 1;
				}
				dsc_set_pid(ch, ch->pid);
				if (flag)
					ch->used = 0;
				dsc_set_keys(ch);
			}
		}
	}
#ifdef ENABLE_SEC_BUFF_WATCHDOG
	if (reset_irq) {
		mod_timer(&dvb->watchdog_timer,
			  jiffies + msecs_to_jiffies(WATCHDOG_TIMER));
	}
#endif

	pr_dbg("[dmx_kpi] demux reset end\n");
}

/*Reset the individual demux*/
void dmx_reset_dmx_hw_ex_unlock(struct aml_dvb *dvb, struct aml_dmx *dmx,
				int reset_irq)
{
	int id;
	u32 pcr_num = 0;
	u32 pcr_regs = 0;
	{
		if (!dmx->init)
			return;
		pcr_regs = DMX_READ_REG(dmx->id, PCR90K_CTL);
		pcr_num = DMX_READ_REG(dmx->id, ASSIGN_PID_NUMBER);
		pr_dbg("reset demux, pcr_regs:0x%x, pcr_num:0x%x\n", pcr_regs, pcr_num);
	}
	{
		if (!dmx->init)
			return;
		if (reset_irq) {
			if (dmx->dmx_irq != -1)
				disable_irq(dmx->dmx_irq);
			if (dmx->dvr_irq != -1)
				disable_irq(dmx->dvr_irq);
		}
	}
#ifdef ENABLE_SEC_BUFF_WATCHDOG
	if (reset_irq) {
		dvb->dmx_watchdog_disable[dmx->id] = 1;
	}
#endif

	for (id = 0; id < DSC_DEV_COUNT; id++)
	{
		struct aml_dsc *dsc = &dvb->dsc[id];
		int n;

		for (n = 0; n < DSC_COUNT; n++)
		{
			struct aml_dsc_channel *ch = &dsc->channel[n];
			{
				ch->id = n;
				dsc_get_pid(ch, &ch->pid);
			}
		}
	}

	pr_error("dmx_reset_dmx_hw_ex_unlock into\n");
	WRITE_MPEG_REG(RESET3_REGISTER,
		       (dmx->id) ? ((dmx->id ==
				     1) ? RESET_DEMUX1 : RESET_DEMUX2) :
		       RESET_DEMUX0);
	WRITE_MPEG_REG(RESET3_REGISTER, RESET_DES);

	{
		int times;

		times = 0;
		while (times++ < 1000000) {
			if (!(DMX_READ_REG(dmx->id, OM_CMD_STATUS) & 0x01))
				break;
		}
	}
	{
		u32 version, data;

		if (!dmx->init)
			return;

		if (reset_irq) {
			if (dmx->dmx_irq != -1)
				enable_irq(dmx->dmx_irq);
			if (dmx->dvr_irq != -1)
				enable_irq(dmx->dvr_irq);
		}
		DMX_WRITE_REG(dmx->id, DEMUX_CONTROL, 0x0000);
		version = DMX_READ_REG(dmx->id, STB_VERSION);
		DMX_WRITE_REG(dmx->id, STB_TEST_REG, version);
		pr_dbg("STB %d hardware version : %d\n", dmx->id, version);
		DMX_WRITE_REG(dmx->id, STB_TEST_REG, 0x5550);
		data = DMX_READ_REG(dmx->id, STB_TEST_REG);
		if (data != 0x5550)
			pr_error("STB %d register access failed\n", dmx->id);
		DMX_WRITE_REG(dmx->id, STB_TEST_REG, 0xaaa0);
		data = DMX_READ_REG(dmx->id, STB_TEST_REG);
		if (data != 0xaaa0)
			pr_error("STB %d register access failed\n", dmx->id);
		DMX_WRITE_REG(dmx->id, MAX_FM_COMP_ADDR, 0x0000);
		DMX_WRITE_REG(dmx->id, STB_INT_MASK, 0);
		DMX_WRITE_REG(dmx->id, STB_INT_STATUS, 0xffff);
		DMX_WRITE_REG(dmx->id, FEC_INPUT_CONTROL, 0);
	}

	stb_enable(dvb);

	{
		int n;
		unsigned long addr;
		unsigned long base;
		unsigned long grp_addr[SEC_BUF_GRP_COUNT];
		int grp_len[SEC_BUF_GRP_COUNT];

		if (!dmx->init)
			return;

		if (dmx->sec_pages) {
			grp_len[0] = (1 << SEC_GRP_LEN_0) * 8;
			grp_len[1] = (1 << SEC_GRP_LEN_1) * 8;
			grp_len[2] = (1 << SEC_GRP_LEN_2) * 8;
			grp_len[3] = (1 << SEC_GRP_LEN_3) * 8;

			grp_addr[0] = virt_to_phys((void *)dmx->sec_pages);
			grp_addr[1] = grp_addr[0] + grp_len[0];
			grp_addr[2] = grp_addr[1] + grp_len[1];
			grp_addr[3] = grp_addr[2] + grp_len[2];

			base = grp_addr[0] & 0xFFFF0000;
			DMX_WRITE_REG(dmx->id, SEC_BUFF_BASE, base >> 16);
			DMX_WRITE_REG(dmx->id, SEC_BUFF_01_START,
					(((grp_addr[0] - base) >> 8) << 16) |
					 ((grp_addr[1] - base) >> 8));
			DMX_WRITE_REG(dmx->id, SEC_BUFF_23_START,
					(((grp_addr[2] - base) >> 8) << 16) |
					 ((grp_addr[3] - base) >> 8));
			DMX_WRITE_REG(dmx->id, SEC_BUFF_SIZE,
					SEC_GRP_LEN_0 |
					(SEC_GRP_LEN_1 << 4) |
					(SEC_GRP_LEN_2 << 8) |
					(SEC_GRP_LEN_3 << 12));
		}
#ifdef NO_SUB
#ifndef SUB_PARSER
		if (dmx->sub_pages) {
			addr = virt_to_phys((void *)dmx->sub_pages);
			DMX_WRITE_REG(dmx->id, SB_START, addr >> 12);
			DMX_WRITE_REG(dmx->id, SB_LAST_ADDR,
				      (dmx->sub_buf_len >> 3) - 1);
		}
#endif
#endif
		if (dmx->pes_pages) {
			addr = virt_to_phys((void *)dmx->pes_pages);
			DMX_WRITE_REG(dmx->id, OB_START, addr >> 12);
			DMX_WRITE_REG(dmx->id, OB_LAST_ADDR,
				      (dmx->pes_buf_len >> 3) - 1);
		}

		for (n = 0; n < CHANNEL_COUNT; n++) {
			{
#ifdef NO_SUB
#ifdef SUB_PARSER
				/*
				  check if subtitle channel was running,
				  the parser will be used in amstream also,
				  take care of the buff ptr.
				*/
				u32 v = dmx_get_chan_target(dmx, n);
				if (v != 0xFFFF &&
					(v & (0x7 << PID_TYPE))
						== (SUB_PACKET << PID_TYPE))
					set_subtitle_pes_buffer(dmx);
#endif
#endif
				{
					u32 v = dmx_get_chan_target(dmx, n);
					if (v != 0xFFFF &&
						(v & (0x7 << PID_TYPE))
						==
						(OTHER_PES_PACKET << PID_TYPE))
						pes_off_pre[dmx->id] = 0;
				}
				dmx_set_chan_regs(dmx, n);
			}
		}

		for (n = 0; n < FILTER_COUNT; n++) {
			struct aml_filter *filter = &dmx->filter[n];

			if (filter->used)
				dmx_set_filter_regs(dmx, n);
		}

		for (n = 0; n < SEC_CNT_MAX; n++) {
			dmx->sec_cnt[n] = 0;
			dmx->sec_cnt_match[n] = 0;
			dmx->sec_cnt_crc_fail[n] = 0;
		}
		dmx_enable(dmx);
		dmx_smallsec_set(&dmx->smallsec,
				dmx->smallsec.enable,
				dmx->smallsec.bufsize,
				1);

		dmx_timeout_set(&dmx->timeout,
				dmx->timeout.enable,
				dmx->timeout.timeout,
				dmx->timeout.ch_disable,
				dmx->timeout.match,
				1);
	}

	{
		int id;

		for (id = 0; id < DSC_DEV_COUNT; id++) {
			struct aml_dsc *dsc = &dvb->dsc[id];
			int n;

			for (n = 0; n < DSC_COUNT; n++) {
				int flag = 0;
				struct aml_dsc_channel *ch = &dsc->channel[n];
				/*if(ch->used)*/ {
				ch->id = n;
				ch->work_mode = -1;
				if (ch->pid != 0x1fff && !ch->used) {
					flag = 1;
					ch->used = 1;
				}
				dsc_set_pid(ch, ch->pid);
				if (flag)
					ch->used = 0;
				dsc_set_keys(ch);
				}
			}
		}
	}
#ifdef ENABLE_SEC_BUFF_WATCHDOG
	if (reset_irq) {
		dvb->dmx_watchdog_disable[dmx->id] = 0;
	}
#endif
	{
		DMX_WRITE_REG(dmx->id, ASSIGN_PID_NUMBER,  pcr_num);
		DMX_WRITE_REG(dmx->id, PCR90K_CTL,  pcr_regs);
	}
}

void dmx_reset_dmx_id_hw_ex_unlock(struct aml_dvb *dvb, int id, int reset_irq)
{
	dmx_reset_dmx_hw_ex_unlock(dvb, &dvb->dmx[id], reset_irq);
}

void dmx_reset_dmx_hw_ex(struct aml_dvb *dvb, struct aml_dmx *dmx,
			 int reset_irq)
{
	unsigned long flags;
	spin_lock_irqsave(&dvb->slock, flags);
	dmx_reset_dmx_hw_ex_unlock(dvb, dmx, reset_irq);
	spin_unlock_irqrestore(&dvb->slock, flags);
}

void dmx_reset_dmx_id_hw_ex(struct aml_dvb *dvb, int id, int reset_irq)
{
	unsigned long flags;

	spin_lock_irqsave(&dvb->slock, flags);
	dmx_reset_dmx_id_hw_ex_unlock(dvb, id, reset_irq);
	spin_unlock_irqrestore(&dvb->slock, flags);
}

void dmx_reset_dmx_hw(struct aml_dvb *dvb, int id)
{
	dmx_reset_dmx_id_hw_ex(dvb, id, 1);
}

static int set_subtitle_pes_buffer(struct aml_dmx *dmx)
{
#ifdef SUB_PARSER
	if (dmx->sub_chan == -1) {
	unsigned long addr = virt_to_phys((void *)dmx->sub_pages);
	WRITE_MPEG_REG(PARSER_SUB_RP, addr);
	WRITE_MPEG_REG(PARSER_SUB_START_PTR, addr);
	WRITE_MPEG_REG(PARSER_SUB_END_PTR, addr + dmx->sub_buf_len - 8);
	pr_inf("set sub buff: (%d) %lx %x\n", dmx->id, addr, dmx->sub_buf_len);
	}
#endif
	return 0;
}

int dmx_get_sub_buffer(unsigned long *base, unsigned long *virt)
{
#ifndef SUB_BUF_DMX
	unsigned long s = READ_MPEG_REG(PARSER_SUB_START_PTR);
	if (base)
		*base = s;
	if (virt)
		*virt = (unsigned long)codec_mm_phys_to_virt(s);
#endif
	return 0;
}

int dmx_init_sub_buffer(struct aml_dmx *dmx, unsigned long base, unsigned long virt)
{
#ifndef SUB_BUF_DMX
	dmx->sub_buf_base = base;
	pr_inf("sub buf base: 0x%lx\n", dmx->sub_buf_base);

	dmx->sub_buf_base_virt = (u8 *)virt;
	pr_inf("sub buf base virt: 0x%p\n", dmx->sub_buf_base_virt);
#endif
	return 0;
}

static int check_dvr_for_raw_channel(struct aml_dmx *dmx, int ch)
{
	switch (ch) {
		case 0:
		case 1:  return 1;
		case 2:  return dmx->sub_chan != -1 ? 1 : 0;
		case 3:  return dmx->pcr_chan != -1 ? 1 : 0;
		default: return 0;
	}
	return 0;
}

/*Allocate a new channel*/
int dmx_alloc_chan(struct aml_dmx *dmx, int type, int pes_type, int pid)
{
	int id = -1;
	int ret;

	if (type == DMX_TYPE_TS) {
		switch (pes_type) {
		case DMX_PES_VIDEO:
			if (!dmx->channel[0].used)
				id = 0;
			break;
		case DMX_PES_AUDIO:
			if (!dmx->channel[1].used)
				id = 1;
			break;
		case DMX_PES_SUBTITLE:
		case DMX_PES_TELETEXT:
			if (!dmx->channel[2].used)
				id = 2;
			set_subtitle_pes_buffer(dmx);
			break;
		case DMX_PES_PCR:
			if (!dmx->channel[3].used)
				id = 3;
			break;
		case DMX_PES_AUDIO3:
			pes_off_pre[dmx->id] = 0;
			{
				int i;

				for (i = SYS_CHAN_COUNT;
						i < CHANNEL_COUNT; i++) {
					if (!dmx->channel[i].used) {
						id = i;
						break;
					}
				}
			}
			break;
		case DMX_PES_OTHER:
			{
				int i;

				for (i = SYS_CHAN_COUNT;
						i < CHANNEL_COUNT; i++) {
					if (!dmx->channel[i].used) {
						id = i;
						break;
					}
				}
			}
			break;
		default:
			break;
		}
	} else {
		int i;

		for (i = SYS_CHAN_COUNT; i < CHANNEL_COUNT; i++) {
			if (!dmx->channel[i].used) {
				id = i;
				break;
			}
		}
	}

	if (id == -1) {
		pr_error("too many channels\n");
		return -1;
	}

	pr_dbg("allocate channel(id:%d-%d PID:0x%x)\n", dmx->id, id, pid);

	if (check_dvr_for_raw_channel(dmx, id)) {
		ret = dmx_get_chan(dmx, pid);
		if (ret >= 0 && DVR_FEED(dmx->channel[ret].feed)) {
			pr_dbg("raw ch fix: dmx:%d: ch[%d(dvr)] -> ch[%d]\n",
				dmx->id, ret, id);
			dmx_remove_feed(dmx, dmx->channel[ret].feed);
			dmx->channel[id].dvr_feed = dmx->channel[ret].feed;
			dmx->channel[id].dvr_feed->priv = (void *)(long)id;
		} else {
			dmx->channel[id].dvr_feed = NULL;
		}
	}

	dmx->channel[id].type = type;
	dmx->channel[id].pes_type = pes_type;
	dmx->channel[id].pid = pid;
	dmx->channel[id].used = 1;
	dmx->channel[id].filter_count = 0;

	dmx_set_chan_regs(dmx, id);

	set_debug_dmx_chanpids(dmx->id, id, pid);

	dmx->chan_count++;
	dmx_enable(dmx);

	return id;
}

/*Free a channel*/
void dmx_free_chan(struct aml_dmx *dmx, int cid)
{
	pr_dbg("free channel(id:%d-%d PID:0x%x)\n", dmx->id, cid, dmx->channel[cid].pid);

	dmx->channel[cid].used = 0;
	dmx->channel[cid].pid = 0x1fff;
	dmx_set_chan_regs(dmx, cid);

	if (cid == 2) {
		u32 parser_sub_start_ptr;

		parser_sub_start_ptr = READ_MPEG_REG(PARSER_SUB_START_PTR);
		WRITE_MPEG_REG(PARSER_SUB_RP, parser_sub_start_ptr);
		WRITE_MPEG_REG(PARSER_SUB_WP, parser_sub_start_ptr);
	}

	set_debug_dmx_chanpids(dmx->id, cid, -1);
	dmx->chan_count--;
	dmx_enable(dmx);

	/*Special pes type channel, check its dvr feed */
	if (check_dvr_for_raw_channel(dmx, cid)
			&& dmx->channel[cid].dvr_feed) {
		/*start the dvr feed */
		pr_dbg("raw ch fix: dmx:%d: ch[%d] -> ch[(dvr)]\n",
			dmx->id, cid);
		dmx_add_feed(dmx, dmx->channel[cid].dvr_feed);
		dmx->channel[cid].dvr_feed = NULL;
	}
}

/*Add a section*/
static int dmx_chan_add_filter(struct aml_dmx *dmx, int cid,
			       struct dvb_demux_filter *filter)
{
	int id = -1;
	int i;

	for (i = 0; i < FILTER_COUNT; i++) {
		if (!dmx->filter[i].used) {
			id = i;
			break;
		}
	}

	if (id == -1) {
		pr_error("too many filters\n");
		return -1;
	}

	pr_dbg("channel(id:%d PID:0x%x) add filter(id:%d)\n", cid,
	       filter->feed->pid, id);

	dmx->filter[id].chan_id = cid;
	dmx->filter[id].used = 1;
	dmx->filter[id].filter = (struct dmx_section_filter *)filter;
	dmx->channel[cid].filter_count++;

	dmx_set_filter_regs(dmx, id);

	return id;
}

static void dmx_remove_filter(struct aml_dmx *dmx, int cid, int fid)
{
	pr_dbg("channel(id:%d PID:0x%x) remove filter(id:%d)\n", cid,
	       dmx->channel[cid].pid, fid);

	dmx->filter[fid].used = 0;
	dmx->channel[cid].filter_count--;

	dmx_set_filter_regs(dmx, fid);
	dmx_clear_filter_buffer(dmx, fid);
}

static int sf_add_feed(struct aml_dmx *src_dmx, struct dvb_demux_feed *feed)
{
	int ret = 0;

	struct aml_dvb *dvb = (struct aml_dvb *)src_dmx->demux.priv;
	struct aml_swfilter *sf = &dvb->swfilter;

	pr_dbg_sf("sf add pid[%d]\n", feed->pid);

	/*init sf */
	if (!sf->user) {
		void *mem;

		mem = vmalloc(SF_BUFFER_SIZE);
		if (!mem) {
			ret = -ENOMEM;
			goto fail;
		}
		dvb_ringbuffer_init(&sf->rbuf, mem, SF_BUFFER_SIZE);

		sf->dmx = &dvb->dmx[SF_DMX_ID];
		sf->afifo = &dvb->asyncfifo[SF_AFIFO_ID];

		sf->dmx->source = src_dmx->source;
		sf->afifo->source = sf->dmx->id;
		sf->track_dmx = src_dmx->id;

		pr_dbg_sf("init sf mode.\n");

	} else if (sf->dmx->source != src_dmx->source) {
		pr_error(" pid=%d[src:%d] already used with sfdmx%d[src:%d]\n",
			 feed->pid, src_dmx->source, sf->dmx->id,
			 sf->dmx->source);
		ret = -EBUSY;
		goto fail;
	}

	/*setup feed */
	ret = dmx_get_chan(sf->dmx, feed->pid);
	if (ret >= 0) {
		pr_error(" pid=%d[dmx:%d] already used [dmx:%d].\n",
			 feed->pid, src_dmx->id,
			 ((struct aml_dmx *)sf->dmx->channel[ret].feed->
			 demux)->id);
		ret = -EBUSY;
		goto fail;
	}
	ret =
	     dmx_alloc_chan(sf->dmx, DMX_TYPE_TS, DMX_PES_OTHER,
			    feed->pid);
	if (ret < 0) {
		pr_error(" %s: alloc chan error, ret=%d\n", __func__, ret);
		ret = -EBUSY;
		goto fail;
	}
	sf->dmx->channel[ret].feed = feed;
	feed->priv = (void *)(long)ret;

	sf->dmx->channel[ret].dvr_feed = feed;

	sf->user++;
	debug_sf_user = sf->user;
	dmx_enable(sf->dmx);

	return 0;

fail:
	feed->priv = (void *)-1;
	return ret;
}

static int sf_remove_feed(struct aml_dmx *src_dmx, struct dvb_demux_feed *feed)
{
	int ret;

	struct aml_dvb *dvb = (struct aml_dvb *)src_dmx->demux.priv;
	struct aml_swfilter *sf = &dvb->swfilter;

	if (!sf->user || (sf->dmx->source != src_dmx->source))
		return 0;

	/*add fail, no need to remove*/
	if (((long)feed->priv) < 0)
		return 0;

	ret = dmx_get_chan(sf->dmx, feed->pid);
	if (ret < 0)
		return 0;

	pr_dbg_sf("sf remove pid[%d]\n", feed->pid);

	dmx_free_chan(sf->dmx, (long)feed->priv);

	sf->dmx->channel[ret].feed = NULL;
	sf->dmx->channel[ret].dvr_feed = NULL;

	sf->user--;
	debug_sf_user = sf->user;

	if (!sf->user) {
		sf->dmx->source = -1;
		sf->afifo->source = AM_DMX_MAX;
		sf->track_dmx = -1;

		if (sf->rbuf.data) {
			void *mem = sf->rbuf.data;

			sf->rbuf.data = NULL;
			vfree(mem);
		}
		pr_dbg_sf("exit sf mode.\n");
	}

	return 0;
}

static int sf_feed_sf(struct aml_dmx *dmx, struct dvb_demux_feed *feed,
		      int add_not_remove)
{
	int sf = 0;

	if (sf_dmx_sf(dmx)) {
		pr_error("%s: demux %d is in sf mode\n", __func__, dmx->id);
		return -EINVAL;
	}

	switch (feed->type) {
	case DMX_TYPE_TS:{
			struct dmxdev_filter *dmxdevfilter =
							 feed->feed.ts.priv;
			if (!DVR_FEED(feed)) {
				if (dmxdevfilter->params.pes.
				    flags & DMX_USE_SWFILTER)
					sf = 1;
				if (force_pes_sf)
					sf = 1;
			}
		}
		break;

	case DMX_TYPE_SEC:{
			struct dvb_demux_filter *filter;

			for (filter = feed->filter; filter;
			     filter = filter->next) {
				struct dmxdev_filter *dmxdevfilter =
				    filter->filter.priv;
				if (dmxdevfilter->params.sec.
				    flags & DMX_USE_SWFILTER)
					sf = 1;
				if (add_not_remove)
					filter->hw_handle = (u16)-1;
			}
			if (force_sec_sf)
				sf = 1;
		}
		break;
	}

	return sf ? 0 : 1;
}

static int sf_check_feed(struct aml_dmx *dmx, struct dvb_demux_feed *feed,
			 int add_not_remove)
{
	int ret = 0;

	ret = sf_feed_sf(dmx, feed, add_not_remove);
	if (ret)
		return ret;

	pr_dbg_sf("%s [pid:%d] %s\n",
		  (feed->type == DMX_TYPE_TS) ? "DMX_TYPE_TS" : "DMX_TYPE_SEC",
		  feed->pid, add_not_remove ? "-> sf mode" : "sf mode ->");

	if (add_not_remove)
		ret = sf_add_feed(dmx, feed);
	else
		ret = sf_remove_feed(dmx, feed);

	if (ret < 0) {
		pr_error("sf %s feed fail[%d]\n",
			 add_not_remove ? "add" : "remove", ret);
	}
	return ret;
}

static int dmx_add_feed(struct aml_dmx *dmx, struct dvb_demux_feed *feed)
{
	int id, ret = 0;
	struct dvb_demux_filter *filter;
	struct dvb_demux_feed *dfeed = NULL;
	int sf_ret = 0;		/*<0:error, =0:sf_on, >0:sf_off */

	sf_ret = sf_check_feed(dmx, feed, 1/*SF_FEED_OP_ADD */);
	if (sf_ret < 0)
		return sf_ret;

	switch (feed->type) {
	case DMX_TYPE_TS:
		pr_dbg("add feed ts: pid:%d-0x%x, (%p)\n",
			dmx->id, feed->pid, feed);
		ret = dmx_get_chan(dmx, feed->pid);
		if (ret >= 0) {
			if (DVR_FEED(dmx->channel[ret].feed)) {
				if (DVR_FEED(feed)) {
					/*dvr feed already work */
					pr_error("PID %d already used(DVR)\n",
						 feed->pid);
					ret = -EBUSY;
					goto fail;
				}
				if (sf_ret) {
					/*if sf_on, we do not reset the
					 *previous dvr feed, just load the pes
					 *feed on the sf, a diffrent data path.
					 */
					dfeed = dmx->channel[ret].feed;
					dmx_remove_feed(dmx, dfeed);
				}
			} else {
				if (DVR_FEED(feed)
				    && (!dmx->channel[ret].dvr_feed)) {
					/*just store the dvr_feed */
					dmx->channel[ret].dvr_feed = feed;
					feed->priv = (void *)(long)ret;
					if (!dmx->record)
						dmx_enable(dmx);
					dmx_add_recchan(dmx->id, ret);
					return 0;
				}
				{
					pr_error("PID %d already used\n",
						 feed->pid);
					ret = -EBUSY;
					goto fail;
				}
			}
		}

		if (sf_ret) {	/*not sf feed. */
			ret =
			     dmx_alloc_chan(dmx, feed->type,
						feed->pes_type, feed->pid);
			if (ret < 0) {
				pr_dbg("%s: alloc chan error, ret=%d\n",
				       __func__, ret);
				ret = -EBUSY;
				goto fail;
			}
			dmx->channel[ret].feed = feed;
			feed->priv = (void *)(long)ret;
			dmx->channel[ret].dvr_feed = NULL;
		}
		/*dvr */
		if (DVR_FEED(feed)) {
			dmx->channel[ret].dvr_feed = feed;
			feed->priv = (void *)(long)ret;
			if (!dmx->record)
				dmx_enable(dmx);
			dmx_add_recchan(dmx->id, ret);
		} else if (dfeed && sf_ret) {
			dmx->channel[ret].dvr_feed = dfeed;
			dfeed->priv = (void *)(long)ret;
			if (!dmx->record)
				dmx_enable(dmx);
			dmx_add_recchan(dmx->id, ret);
		}

		break;
	case DMX_TYPE_SEC:
		pr_dbg("add feed sec: pid:%d-0x%x, (%p)\n",
			dmx->id, feed->pid, feed);
		ret = dmx_get_chan(dmx, feed->pid);
		if (ret >= 0) {
			if (DVR_FEED(dmx->channel[ret].feed)) {
				if (sf_ret) {
					/*if sf_on, we do not reset the
					 *previous dvr feed, just load the pes
					 *feed on the sf,a diffrent data path.
					 */
					dfeed = dmx->channel[ret].feed;
					dmx_remove_feed(dmx, dfeed);
				}
			} else {
				pr_error("PID %d already used\n", feed->pid);
				ret = -EBUSY;
				goto fail;
			}
		}
		if (sf_ret) {	/*not sf feed. */
			id = dmx_alloc_chan(dmx, feed->type,
				feed->pes_type, feed->pid);
			if (id < 0) {
				pr_dbg("%s: alloc chan error, ret=%d\n",
				       __func__, id);
				ret = -EBUSY;
				goto fail;
			}
			for (filter = feed->filter; filter;
				filter = filter->next) {
				ret = dmx_chan_add_filter(dmx, id, filter);
				if (ret >= 0)
					filter->hw_handle = ret;
				else
					filter->hw_handle = (u16)-1;
			}
			dmx->channel[id].feed = feed;
			feed->priv = (void *)(long)id;
			dmx->channel[id].dvr_feed = NULL;

			if (dfeed) {
				dmx->channel[id].dvr_feed = dfeed;
				dfeed->priv = (void *)(long)id;
				if (!dmx->record)
					dmx_enable(dmx);
				dmx_add_recchan(dmx->id, id);
			}
		}
		break;
	default:
		return -EINVAL;
	}

	dmx->feed_count++;

	return 0;

fail:
	feed->priv = (void *)-1;
	return ret;
}

static int dmx_remove_feed(struct aml_dmx *dmx, struct dvb_demux_feed *feed)
{
	struct dvb_demux_filter *filter;
	struct dvb_demux_feed *dfeed = NULL;

	int sf_ret = 0;		/*<0:error, =0:sf_on, >0:sf_off */

	/*add fail, no need to remove*/
	if (((long)feed->priv) < 0)
		return 0;

	sf_ret = sf_check_feed(dmx, feed, 0/*SF_FEED_OP_RM */);
	if (sf_ret <= 0)
		return sf_ret;

	switch (feed->type) {
	case DMX_TYPE_TS:
		pr_dbg("rm feed ts: pid:%d-0x%x, %p\n",
			dmx->id, feed->pid, feed);
		if (dmx->channel[(long)feed->priv].feed ==
		    dmx->channel[(long)feed->priv].dvr_feed) {
			dmx_rm_recchan(dmx->id, (long)feed->priv);
			dmx_free_chan(dmx, (long)feed->priv);
		} else {
			if (feed == dmx->channel[(long)feed->priv].feed) {
				dfeed = dmx->channel[(long)feed->priv].dvr_feed;
				dmx_rm_recchan(dmx->id, (long)feed->priv);
				dmx_free_chan(dmx, (long)feed->priv);
				if (dfeed) {
					/*start the dvr feed */
					dmx_add_feed(dmx, dfeed);
				}
			} else if (feed ==
				   dmx->channel[(long)feed->priv].dvr_feed) {
				/*just remove the dvr_feed */
				dmx->channel[(long)feed->priv].dvr_feed = NULL;
				dmx_rm_recchan(dmx->id,	(long)feed->priv);
				if (dmx->record)
					dmx_enable(dmx);
			} else {
				/*This must never happen */
				pr_error("%s: unknown feed\n", __func__);
				return -EINVAL;
			}
		}

		break;
	case DMX_TYPE_SEC:
		pr_dbg("rm feed sec: pid:%d-0x%x, %p\n",
			dmx->id, feed->pid, feed);
		for (filter = feed->filter; filter; filter = filter->next) {
			if (filter->hw_handle != (u16)-1)
				dmx_remove_filter(dmx, (long)feed->priv,
						  (int)filter->hw_handle);
		}

		dfeed = dmx->channel[(long)feed->priv].dvr_feed;
		dmx_rm_recchan(dmx->id, (long)feed->priv);
		dmx_free_chan(dmx, (long)feed->priv);
		if (dfeed) {
			/*start the dvr feed */
			dmx_add_feed(dmx, dfeed);
		}
		break;
	default:
		return -EINVAL;
	}

	dmx->feed_count--;
	return 0;
}

int aml_dmx_hw_init(struct aml_dmx *dmx)
{
	int ret;
	ret = dmx_init(dmx);

	return ret;
}

int aml_dmx_hw_deinit(struct aml_dmx *dmx)
{
	struct aml_dvb *dvb = (struct aml_dvb *)dmx->demux.priv;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&dvb->slock, flags);
	ret = dmx_deinit(dmx);
	spin_unlock_irqrestore(&dvb->slock, flags);

	return ret;
}


int aml_asyncfifo_hw_init(struct aml_asyncfifo *afifo)
{
	int ret;

	int len = asyncfifo_buf_len;
	unsigned long buf = asyncfifo_alloc_buffer(afifo, len);

	if (!buf)
		return -1;

	WRITE_MPEG_REG(RESET6_REGISTER, (1<<11)|(1<<12));
	ret = async_fifo_init(afifo, 1, len, buf);

	if (ret < 0)
		asyncfifo_free_buffer(buf, len);

	return ret;
}

int aml_asyncfifo_hw_deinit(struct aml_asyncfifo *afifo)
{
	int ret;
	ret = async_fifo_deinit(afifo, 1);

	return ret;
}

int aml_asyncfifo_hw_reset(struct aml_asyncfifo *afifo)
{
	struct aml_dvb *dvb = afifo->dvb;
	unsigned long flags;
	int ret, src = -1;

	unsigned long buf = 0;
	int len = asyncfifo_buf_len;
	buf = asyncfifo_alloc_buffer(afifo, len);
	if (!buf)
		return -1;

	if (afifo->init) {
		src = afifo->source;
		async_fifo_deinit(afifo, 0);
	}

	spin_lock_irqsave(&dvb->slock, flags);
	ret = async_fifo_init(afifo, 0, len, buf);
	/* restore the source */
	if (src != -1)
		afifo->source = src;

	if ((ret == 0) && afifo->dvb)
		reset_async_fifos(afifo->dvb);

	spin_unlock_irqrestore(&dvb->slock, flags);

	if (ret < 0)
		asyncfifo_free_buffer(buf, len);

	return ret;
}

int aml_dmx_hw_start_feed(struct dvb_demux_feed *dvbdmxfeed)
{
	struct aml_dmx *dmx = (struct aml_dmx *)dvbdmxfeed->demux;
	struct aml_dvb *dvb = (struct aml_dvb *)dmx->demux.priv;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&dvb->slock, flags);
	ret = dmx_add_feed(dmx, dvbdmxfeed);
	spin_unlock_irqrestore(&dvb->slock, flags);

	/*handle errors silently*/
	if (ret != 0)
		ret = 0;

	return ret;
}

int aml_dmx_hw_stop_feed(struct dvb_demux_feed *dvbdmxfeed)
{
	struct aml_dmx *dmx = (struct aml_dmx *)dvbdmxfeed->demux;
	struct aml_dvb *dvb = (struct aml_dvb *)dmx->demux.priv;
	unsigned long flags;

	spin_lock_irqsave(&dvb->slock, flags);
	dmx_remove_feed(dmx, dvbdmxfeed);
	spin_unlock_irqrestore(&dvb->slock, flags);

	return 0;
}

int sf_dmx_track_source(struct aml_dmx *dmx)
{
	struct aml_dvb *dvb = (struct aml_dvb *)dmx->demux.priv;
	struct aml_swfilter *sf = &dvb->swfilter;

	if (sf->user && (dmx->id == sf->track_dmx)) {
		pr_dbg_sf("tracking dmx src [%d -> %d]\n",
			  sf->dmx->source, dmx->source);
		sf->dmx->source = dmx->source;
		dmx_reset_dmx_hw_ex_unlock(dvb, sf->dmx, 0);
	}
	return 0;
}

int aml_dmx_hw_set_source(struct dmx_demux *demux, dmx_source_t src)
{
	struct aml_dmx *dmx = (struct aml_dmx *)demux;
	struct aml_dvb *dvb = (struct aml_dvb *)dmx->demux.priv;
	int ret = 0;
	int hw_src;
	unsigned long flags;

	if (sf_dmx_sf(dmx)) {
		pr_error("%s: demux %d is in sf mode\n", __func__, dmx->id);
		return -EINVAL;
	}

	spin_lock_irqsave(&dvb->slock, flags);

	hw_src = dmx->source;

	switch (src) {
	case DMX_SOURCE_FRONT0:
		hw_src =
		    (dvb->ts[0].mode ==
		     AM_TS_SERIAL) ? (dvb->ts[0].s2p_id+AM_TS_SRC_S_TS0) : AM_TS_SRC_TS0;
		break;
	case DMX_SOURCE_FRONT1:
		hw_src =
		    (dvb->ts[1].mode ==
		     AM_TS_SERIAL) ? (dvb->ts[1].s2p_id+AM_TS_SRC_S_TS0) : AM_TS_SRC_TS1;
		break;
	case DMX_SOURCE_FRONT2:
		hw_src =
		    (dvb->ts[2].mode ==
		     AM_TS_SERIAL) ? (dvb->ts[2].s2p_id+AM_TS_SRC_S_TS0) : AM_TS_SRC_TS2;
		break;
	case DMX_SOURCE_FRONT3:
		hw_src =
			(dvb->ts[3].mode ==
			 AM_TS_SERIAL) ? (dvb->ts[3].s2p_id+AM_TS_SRC_S_TS0) : AM_TS_SRC_TS3;
		break;
	case DMX_SOURCE_DVR0:
		hw_src = AM_TS_SRC_HIU;
		break;
	case DMX_SOURCE_DVR1:
		hw_src = AM_TS_SRC_HIU1;
		break;
	case DMX_SOURCE_FRONT0_OFFSET:
		hw_src = AM_TS_SRC_DMX0;
		break;
	case DMX_SOURCE_FRONT1_OFFSET:
		hw_src = AM_TS_SRC_DMX1;
		break;
	case DMX_SOURCE_FRONT2_OFFSET:
		hw_src = AM_TS_SRC_DMX2;
		break;
	default:
		pr_error("illegal demux source %d\n", src);
		ret = -EINVAL;
		break;
	}

	if (hw_src != dmx->source) {
		dmx->source = hw_src;
		dmx_reset_dmx_hw_ex_unlock(dvb, dmx, 0);
		sf_dmx_track_source(dmx);
	}

	spin_unlock_irqrestore(&dvb->slock, flags);

	return ret;
}

#define IS_SRC_DMX(_src) ((_src) >= AM_TS_SRC_DMX0 && (_src) <= AM_TS_SRC_DMX2)

int aml_stb_hw_set_source(struct aml_dvb *dvb, dmx_source_t src)
{
	unsigned long flags;
	int hw_src;
	int ret;

	ret = 0;
	spin_lock_irqsave(&dvb->slock, flags);

	hw_src = dvb->stb_source;

	switch (src) {
	case DMX_SOURCE_FRONT0:
		hw_src =
		    (dvb->ts[0].mode ==
		     AM_TS_SERIAL) ? (dvb->ts[0].s2p_id+AM_TS_SRC_S_TS0) : AM_TS_SRC_TS0;
		break;
	case DMX_SOURCE_FRONT1:
		hw_src =
		    (dvb->ts[1].mode ==
		     AM_TS_SERIAL) ? (dvb->ts[1].s2p_id+AM_TS_SRC_S_TS0) : AM_TS_SRC_TS1;
		break;
	case DMX_SOURCE_FRONT2:
		hw_src =
		    (dvb->ts[2].mode ==
		     AM_TS_SERIAL) ? (dvb->ts[2].s2p_id+AM_TS_SRC_S_TS0) : AM_TS_SRC_TS2;
		break;
	case DMX_SOURCE_FRONT3:
		hw_src =
		    (dvb->ts[3].mode ==
		     AM_TS_SERIAL) ? (dvb->ts[3].s2p_id+AM_TS_SRC_S_TS0) : AM_TS_SRC_TS3;
		break;
	case DMX_SOURCE_DVR0:
		hw_src = AM_TS_SRC_HIU;
		break;
	case DMX_SOURCE_DVR1:
		hw_src = AM_TS_SRC_HIU1;
		break;
	case DMX_SOURCE_FRONT0_OFFSET:
		hw_src = AM_TS_SRC_DMX0;
		break;
	case DMX_SOURCE_FRONT1_OFFSET:
		hw_src = AM_TS_SRC_DMX1;
		break;
	case DMX_SOURCE_FRONT2_OFFSET:
		hw_src = AM_TS_SRC_DMX2;
		break;
	default:
		pr_error("illegal demux source %d\n", src);
		ret = -EINVAL;
		break;
	}

	if (dvb->stb_source != hw_src) {
		int old_source = dvb->stb_source;

		dvb->stb_source = hw_src;

		if (IS_SRC_DMX(old_source)) {
			dmx_set_misc_id(dvb,
				(old_source - AM_TS_SRC_DMX0), 0, -1);
		} else {
			/*which dmx for av-play is unknown,
			 *can't avoid reset-all
			 */
			dmx_reset_hw_ex(dvb, 0);
		}

		if (IS_SRC_DMX(dvb->stb_source)) {
			dmx_set_misc_id(dvb,
				(dvb->stb_source - AM_TS_SRC_DMX0), 1, -1);
		} else {
			/*which dmx for av-play is unknown,
			 *can't avoid reset-all
			 */
			dmx_reset_hw_ex(dvb, 0);
		}
	}

	spin_unlock_irqrestore(&dvb->slock, flags);

	return ret;
}



int aml_dsc_hw_set_source(struct aml_dsc *dsc,
			dmx_source_t src, dmx_source_t dst)
{
	struct aml_dvb *dvb = dsc->dvb;
	int ret = 0;
	unsigned long flags;
	int hw_src = -1, hw_dst = -1, org_src = -1, org_dst = -1;
	int src_reset = 0, dst_reset = 0;

	spin_lock_irqsave(&dvb->slock, flags);

	hw_src = dsc->source;
	hw_dst = dsc->dst;

	switch (src) {
	case DMX_SOURCE_FRONT0_OFFSET:
		hw_src = AM_TS_SRC_DMX0;
		break;
	case DMX_SOURCE_FRONT1_OFFSET:
		hw_src = AM_TS_SRC_DMX1;
		break;
	case DMX_SOURCE_FRONT2_OFFSET:
		hw_src = AM_TS_SRC_DMX2;
		break;
	default:
		hw_src = -1;
		break;
	}
	switch (dst) {
	case DMX_SOURCE_FRONT0_OFFSET:
		hw_dst = AM_TS_SRC_DMX0;
		break;
	case DMX_SOURCE_FRONT1_OFFSET:
		hw_dst = AM_TS_SRC_DMX1;
		break;
	case DMX_SOURCE_FRONT2_OFFSET:
		hw_dst = AM_TS_SRC_DMX2;
		break;
	default:
		hw_dst = -1;
		break;
	}

	if (hw_src != dsc->source) {
		org_src = dsc->source;
		dsc->source = hw_src;
		src_reset = 1;
	}
	if (hw_dst != dsc->dst) {
		org_dst = dsc->dst;
		dsc->dst = hw_dst;
		dst_reset = 1;
	}

	if (src_reset) {
		pr_inf("dsc%d source changed: %d -> %d\n",
			dsc->id, org_src, hw_src);
		if (org_src != -1) {
			pr_inf("reset dmx%d\n", (org_src - AM_TS_SRC_DMX0));
			dmx_reset_dmx_id_hw_ex_unlock(dvb,
					(org_src - AM_TS_SRC_DMX0), 0);
		}
		if (hw_src != -1) {
			pr_inf("reset dmx%d\n", (hw_src - AM_TS_SRC_DMX0));
			dmx_reset_dmx_id_hw_ex_unlock(dvb,
					(hw_src - AM_TS_SRC_DMX0), 0);
		} else
			dsc_enable(dsc, 0);
	}
	if (dst_reset) {
		pr_inf("dsc%d dest changed: %d -> %d\n",
			dsc->id, org_dst, hw_dst);
		if (((!src_reset) && (org_dst != -1)) ||
			(src_reset && (org_dst != -1) &&
			(org_dst != org_src) && (org_dst != hw_src))) {
			pr_inf("reset dmx%d\n", (org_dst - AM_TS_SRC_DMX0));
			dmx_reset_dmx_id_hw_ex_unlock(dvb,
					(org_dst - AM_TS_SRC_DMX0), 0);
		}
		if (((!src_reset) && (hw_dst != -1)) ||
			(src_reset && (hw_dst != -1)
			&& (hw_dst != org_src) && (hw_dst != hw_src))) {
			pr_inf("reset dmx%d\n", (hw_dst - AM_TS_SRC_DMX0));
			dmx_reset_dmx_id_hw_ex_unlock(dvb,
					(hw_dst - AM_TS_SRC_DMX0), 0);
		}
		if (hw_dst == -1)
			dsc_enable(dsc, 0);
	}
	if (src_reset && dst_reset) {
		set_ciplus_input_source(dsc);
	}

	spin_unlock_irqrestore(&dvb->slock, flags);

	return ret;
}

int aml_tso_hw_set_source(struct aml_dvb *dvb, dmx_source_t src)
{
	int ret = 0;
	unsigned long flags;
	int hw_src;

	spin_lock_irqsave(&dvb->slock, flags);

	hw_src = dvb->tso_source;

	switch (src) {
	case DMX_SOURCE_FRONT0:
		hw_src = (dvb->ts[0].mode == AM_TS_SERIAL)
		    ? (dvb->ts[0].s2p_id+AM_TS_SRC_S_TS0) : AM_TS_SRC_TS0;
		break;
	case DMX_SOURCE_FRONT1:
		hw_src = (dvb->ts[1].mode == AM_TS_SERIAL)
		    ? (dvb->ts[1].s2p_id+AM_TS_SRC_S_TS0) : AM_TS_SRC_TS1;
		break;
	case DMX_SOURCE_FRONT2:
		hw_src = (dvb->ts[2].mode == AM_TS_SERIAL)
		    ? (dvb->ts[2].s2p_id+AM_TS_SRC_S_TS0) : AM_TS_SRC_TS2;
		break;
	case DMX_SOURCE_FRONT3:
		hw_src = (dvb->ts[3].mode == AM_TS_SERIAL)
		    ? (dvb->ts[3].s2p_id+AM_TS_SRC_S_TS0) : AM_TS_SRC_TS3;
		break;
	case DMX_SOURCE_DVR0:
		hw_src = AM_TS_SRC_HIU;
		break;
	case DMX_SOURCE_FRONT0 + 100:
		hw_src = AM_TS_SRC_DMX0;
		break;
	case DMX_SOURCE_FRONT1 + 100:
		hw_src = AM_TS_SRC_DMX1;
		break;
	case DMX_SOURCE_FRONT2 + 100:
		hw_src = AM_TS_SRC_DMX2;
		break;
	default:
		hw_src = -1;
		ret = -EINVAL;
		break;
	}

	if (hw_src != dvb->tso_source) {
		dvb->tso_source = hw_src;
		stb_enable(dvb);
	}

	spin_unlock_irqrestore(&dvb->slock, flags);

	return ret;
}

int aml_asyncfifo_hw_set_source(struct aml_asyncfifo *afifo,
				enum aml_dmx_id_t src)
{
	struct aml_dvb *dvb = afifo->dvb;
	int ret = -1;
	unsigned long flags;

	if (sf_afifo_sf(afifo)) {
		pr_error("%s: afifo %d is in sf mode\n", __func__, afifo->id);
		return -EINVAL;
	}

	spin_lock_irqsave(&dvb->slock, flags);

	pr_dbg("asyncfifo %d set source %d->%d",
						afifo->id, afifo->source, src);
	switch (src) {
	case AM_DMX_0:
	case AM_DMX_1:
	case AM_DMX_2:
		if (afifo->source != src) {
			afifo->source = src;
			ret = 0;
		}
		break;
	default:
		pr_error("illegal async fifo source %d\n", src);
		ret = -EINVAL;
		break;
	}

	if (ret == 0 && afifo->dvb)
		reset_async_fifos(afifo->dvb);

	spin_unlock_irqrestore(&dvb->slock, flags);

	return ret;
}

int aml_dmx_hw_set_dump_ts_select(struct dmx_demux *demux, int dump_ts_select)
{
	struct aml_dmx *dmx = (struct aml_dmx *)demux;
	struct aml_dvb *dvb = (struct aml_dvb *)dmx->demux.priv;
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&dvb->slock, flags);
	dump_ts_select = !!dump_ts_select;
	if (dmx->dump_ts_select != dump_ts_select) {
		dmx->dump_ts_select = dump_ts_select;
		dmx_reset_dmx_hw_ex_unlock(dvb, dmx, 0);
	}
	spin_unlock_irqrestore(&dvb->slock, flags);

	return ret;
}

u32 aml_dmx_get_video_pts(struct aml_dvb *dvb)
{
	unsigned long flags;
	u32 pts;

	spin_lock_irqsave(&dvb->slock, flags);
	pts = video_pts;
	spin_unlock_irqrestore(&dvb->slock, flags);

	return pts;
}

u32 aml_dmx_get_audio_pts(struct aml_dvb *dvb)
{
	unsigned long flags;
	u32 pts;

	spin_lock_irqsave(&dvb->slock, flags);
	pts = audio_pts;
	spin_unlock_irqrestore(&dvb->slock, flags);

	return pts;
}

u32 aml_dmx_get_video_pts_bit32(struct aml_dvb *dvb)
{
	unsigned long flags;
	u32 bit32;

	spin_lock_irqsave(&dvb->slock, flags);
	bit32 = video_pts_bit32;
	spin_unlock_irqrestore(&dvb->slock, flags);

	return bit32;
}

u32 aml_dmx_get_audio_pts_bit32(struct aml_dvb *dvb)
{
	unsigned long flags;
	u32 bit32;

	spin_lock_irqsave(&dvb->slock, flags);
	bit32 = audio_pts_bit32;
	spin_unlock_irqrestore(&dvb->slock, flags);

	return bit32;
}
u32 aml_dmx_get_first_video_pts(struct aml_dvb *dvb)
{
	unsigned long flags;
	u32 pts;

	spin_lock_irqsave(&dvb->slock, flags);
	pts = first_video_pts;
	spin_unlock_irqrestore(&dvb->slock, flags);

	return pts;
}

u32 aml_dmx_get_first_audio_pts(struct aml_dvb *dvb)
{
	unsigned long flags;
	u32 pts;

	spin_lock_irqsave(&dvb->slock, flags);
	pts = first_audio_pts;
	spin_unlock_irqrestore(&dvb->slock, flags);

	return pts;
}

int aml_dmx_set_skipbyte(struct aml_dvb *dvb, int skipbyte)
{
	if (demux_skipbyte != skipbyte) {
		pr_dbg("set skip byte %d\n", skipbyte);
		demux_skipbyte = skipbyte;
		dmx_reset_hw_ex(dvb, 0);
	}

	return 0;
}

int aml_dmx_set_demux(struct aml_dvb *dvb, int id)
{
	aml_stb_hw_set_source(dvb, DMX_SOURCE_DVR0);
	if (id < DMX_DEV_COUNT) {
		struct aml_dmx *dmx = &dvb->dmx[id];

		aml_dmx_hw_set_source((struct dmx_demux *)dmx,
							DMX_SOURCE_DVR0);
	}

	return 0;
}

int _set_tsfile_clkdiv(struct aml_dvb *dvb, int clkdiv)
{
	if (tsfile_clkdiv != clkdiv) {
		pr_dbg("set ts file clock div %d\n", clkdiv);
		tsfile_clkdiv = clkdiv;
		dmx_reset_hw(dvb);
	}

	return 0;
}

static ssize_t tsfile_clkdiv_store(struct class *class,
				     struct class_attribute *attr,
				     const char *buf, size_t size)
{
	long div;

	if (kstrtol(buf, 0, &div) == 0)
		_set_tsfile_clkdiv(aml_get_dvb_device(), (int)div);
	return size;
}

static ssize_t tsfile_clkdiv_show(struct class *class,
				     struct class_attribute *attr, char *buf)
{
	ssize_t ret;

	ret = sprintf(buf, "%d\n", tsfile_clkdiv);
	return ret;
}


static int dmx_id;

static ssize_t dmx_smallsec_show(struct class *class,
				     struct class_attribute *attr, char *buf)
{
	ssize_t ret;
	struct aml_dvb *dvb = aml_get_dvb_device();

	ret = sprintf(buf, "%d:%d\n", dvb->dmx[dmx_id].smallsec.enable,
					dvb->dmx[dmx_id].smallsec.bufsize);
	return ret;
}
static ssize_t dmx_smallsec_store(struct class *class,
				     struct class_attribute *attr,
				     const char *buf, size_t size)
{
	int i, e, s = 0, f = 0;
	struct aml_dvb *dvb = aml_get_dvb_device();

	i = sscanf(buf, "%d:%i:%d", &e, &s, &f);
	if (i <= 0)
		return size;

	dmx_smallsec_set(&dvb->dmx[dmx_id].smallsec, e, s, f);
	return size;
}

static ssize_t dmx_timeout_show(struct class *class,
				     struct class_attribute *attr, char *buf)
{
	ssize_t ret;
	struct aml_dvb *dvb = aml_get_dvb_device();

	ret = sprintf(buf, "%d:%d:0x%x:%d:%d\n",
				dvb->dmx[dmx_id].timeout.enable,
				dvb->dmx[dmx_id].timeout.timeout,
				dvb->dmx[dmx_id].timeout.ch_disable,
				dvb->dmx[dmx_id].timeout.match,
		(DMX_READ_REG(dmx_id, STB_INT_STATUS)&(1<<INPUT_TIME_OUT)) ?
			1 : 0);
	DMX_WRITE_REG(dmx_id, STB_INT_STATUS, (1<<INPUT_TIME_OUT));
	return ret;
}
static ssize_t dmx_timeout_store(struct class *class,
				     struct class_attribute *attr,
				     const char *buf, size_t size)
{
	int i, e, t = 0, c = 0, m = 0, f = 0;
	struct aml_dvb *dvb = aml_get_dvb_device();

	i = sscanf(buf, "%d:%i:%i:%d:%d", &e, &t, &c, &m, &f);
	if (i <= 0)
		return size;

	dmx_timeout_set(&dvb->dmx[dmx_id].timeout, e, t, c, m, f);
	return size;
}


#define DEMUX_SCAMBLE_FUNC_DECL(i)  \
static ssize_t demux##i##_scramble_show(struct class *class,  \
struct class_attribute *attr, char *buf)\
{\
	int data = 0;\
	int aflag = 0;\
	int vflag = 0;\
	ssize_t ret = 0;\
	data = DMX_READ_REG(i, DEMUX_SCRAMBLING_STATE);\
	if ((data & 0x01) == 0x01) \
		vflag = 1;\
	if ((data & 0x02) == 0x02) \
		aflag = 1;\
	ret = sprintf(buf, "%d %d\n", vflag, aflag);\
	return ret;\
}

#if DMX_DEV_COUNT > 0
DEMUX_SCAMBLE_FUNC_DECL(0)
#endif
#if DMX_DEV_COUNT > 1
DEMUX_SCAMBLE_FUNC_DECL(1)
#endif
#if DMX_DEV_COUNT > 2
DEMUX_SCAMBLE_FUNC_DECL(2)
#endif
static ssize_t ciplus_output_ctrl_show(struct class *class,
					 struct class_attribute *attr,
					 char *buf)
{
	int ret;
	char *out = "none";

	pr_inf("output demux use 3 bit to indicate.\n");
	pr_inf("1bit:demux0 2bit:demux1 3bit:demux2\n");

	switch (ciplus_out_sel) {
	case 1:
		out = "dmx0";
		break;
	case 2:
		out = "dmx1";
		break;
	case 4:
		out = "dmx2";
		break;
	default:
		break;
	}

	ret = sprintf(buf, "%s 0x%x %s\n",
		out,
		ciplus_out_sel,
		(ciplus_out_auto_mode) ? "" : "(force)");
	return ret;
}

static ssize_t ciplus_output_ctrl_store(struct class *class,
					  struct class_attribute *attr,
					  const char *buf, size_t size)
{
	struct aml_dvb *dvb = aml_get_dvb_device();
	int i, tmp;
	u32 top_cfg, ci_cfg;

	i = kstrtoint(buf, -1, &tmp);
	if (tmp > 8 || tmp < 0)
		pr_error("Invalid output set\n");
	else if (tmp == 8) {
		ciplus_out_auto_mode = 1;
		ciplus_out_sel = -1;
		pr_error("Auto set output mode enable\n");
	} else {
		ciplus_out_auto_mode = 0;
		ciplus_out_sel = tmp;
		pr_error("Auto set output mode disable\n");
	}

	top_cfg = READ_MPEG_REG(STB_TOP_CONFIG);
	ci_cfg  = READ_MPEG_REG(CIPLUS_CONFIG);

	if (ci_cfg & (1 << CNTL_ENABLE)) {
		int out = 0;

		if (ciplus_out_auto_mode) {
			if (dvb->dsc[0].source != -1)
				out = 1 << (dvb->dsc[0].source - AM_TS_SRC_DMX0);
		} else {
			out = ciplus_out_sel;
		}

		top_cfg &=  ~(7<<CIPLUS_OUT_SEL);
		top_cfg |= (out<<CIPLUS_OUT_SEL);
		WRITE_MPEG_REG(STB_TOP_CONFIG, top_cfg);

		set_fec_core_sel(dvb);
	}

	return size;
}
static ssize_t reset_fec_input_ctrl_show(struct class *class,
					 struct class_attribute *attr,
					 char *buf)
{
	return 0;
}

static ssize_t reset_fec_input_ctrl_store(struct class *class,
					  struct class_attribute *attr,
					  const char *buf, size_t size)
{
	u32 v;

	v = READ_MPEG_REG(FEC_INPUT_CONTROL);
	v &= ~(1<<11);
	WRITE_MPEG_REG(FEC_INPUT_CONTROL, v);

	pr_dbg("reset FEC_INPUT_CONTROL to %x\n", v);

	return size;
}
static ssize_t register_addr_show(struct class *class,
					struct class_attribute *attr,
					char *buf);
static ssize_t register_addr_store(struct class *class,
					 struct class_attribute *attr,
					 const char *buf, size_t size);
static ssize_t dmx_id_show(struct class *class,
				  struct class_attribute *attr, char *buf);
static ssize_t dmx_id_store(struct class *class,
				   struct class_attribute *attr,
				   const char *buf, size_t size);
static ssize_t register_value_show(struct class *class,
					 struct class_attribute *attr,
					 char *buf);
static ssize_t register_value_store(struct class *class,
					  struct class_attribute *attr,
					  const char *buf, size_t size);
static ssize_t dmx_sec_statistics_show(struct class *class,
					 struct class_attribute *attr,
					 char *buf);
static int reg_addr;

static CLASS_ATTR_RW(dmx_id);
static CLASS_ATTR_RW(register_addr);
static CLASS_ATTR_RW(register_value);
static CLASS_ATTR_RW(tsfile_clkdiv);

#define DEMUX_SCAMBLE_ATTR_DECL(i)\
	CLASS_ATTR_RO(demux##i##_scramble);
#if DMX_DEV_COUNT > 0
DEMUX_SCAMBLE_ATTR_DECL(0);
#endif
#if DMX_DEV_COUNT > 1
DEMUX_SCAMBLE_ATTR_DECL(1);
#endif
#if DMX_DEV_COUNT > 2
DEMUX_SCAMBLE_ATTR_DECL(2);
#endif

static CLASS_ATTR_RW(dmx_smallsec);
static CLASS_ATTR_RW(dmx_timeout);
static CLASS_ATTR_RW(reset_fec_input_ctrl);
static CLASS_ATTR_RW(ciplus_output_ctrl);
static CLASS_ATTR_RO(dmx_sec_statistics);

#define DMX_ATTR(name) &class_attr_##name.attr

static struct attribute *aml_dmx_class_attrs[] = {
	DMX_ATTR(dmx_id),
	DMX_ATTR(register_addr),
	DMX_ATTR(register_value),
	DMX_ATTR(tsfile_clkdiv),
	DMX_ATTR(dmx_smallsec),
	DMX_ATTR(dmx_timeout),
	DMX_ATTR(reset_fec_input_ctrl),
	DMX_ATTR(ciplus_output_ctrl),
	DMX_ATTR(dmx_sec_statistics),
#define DEMUX_SCRAMBLE(i) \
	DMX_ATTR(demux##i##_scramble)
#if DMX_DEV_COUNT > 0
	DEMUX_SCRAMBLE(0),
#endif
#if DMX_DEV_COUNT > 1
	DEMUX_SCRAMBLE(1),
#endif
#if DMX_DEV_COUNT > 2
	DEMUX_SCRAMBLE(2),
#endif
	NULL,
};

ATTRIBUTE_GROUPS(aml_dmx_class);

static struct class aml_dmx_class = {
	.name = "dmx",
	.class_groups = aml_dmx_class_groups,
};

static ssize_t dmx_id_show(struct class *class,
				  struct class_attribute *attr, char *buf)
{
	int ret;

	ret = sprintf(buf, "%d\n", dmx_id);
	return ret;
}

static ssize_t dmx_id_store(struct class *class,
				   struct class_attribute *attr,
				   const char *buf, size_t size)
{
	int id = 0;
	long value = 0;

	if (kstrtol(buf, 0, &value) == 0)
		id = (int)value;

	if (id < 0 || id > 2)
		pr_dbg("dmx id must 0 ~2\n");
	else
		dmx_id = id;

	return size;
}

static ssize_t register_addr_show(struct class *class,
					struct class_attribute *attr,
					 char *buf)
{
	int ret;

	ret = sprintf(buf, "%x\n", reg_addr);
	return ret;
}

static ssize_t register_addr_store(struct class *class,
					 struct class_attribute *attr,
					 const char *buf, size_t size)
{
	int addr = 0;
	long value = 0;

	if (kstrtol(buf, 0, &value) == 0)
		addr = (int)value;
	reg_addr = addr;
	return size;
}

static ssize_t register_value_show(struct class *class,
					 struct class_attribute *attr,
					 char *buf)
{
	int ret, value;

	value = READ_MPEG_REG(reg_addr);
	ret = sprintf(buf, "%x\n", value);
	return ret;
}

static ssize_t register_value_store(struct class *class,
					  struct class_attribute *attr,
					  const char *buf, size_t size)
{
	int value = 0;
	long val = 0;

	if (kstrtol(buf, 0, &val) == 0)
		value = (int)val;
	WRITE_MPEG_REG(reg_addr, value);
	return size;
}

static ssize_t dmx_sec_statistics_show(struct class *class,
					 struct class_attribute *attr,
					 char *buf)
{
	ssize_t ret;
	char tmp[128];
	struct aml_dvb *dvb = aml_get_dvb_device();

	ret = sprintf(tmp, "[hw]%#lx:%#lx:%#lx\n[sw]%#lx:%#lx:%#lx\n",
			dvb->dmx[dmx_id].sec_cnt[SEC_CNT_HW],
			dvb->dmx[dmx_id].sec_cnt_match[SEC_CNT_HW],
			dvb->dmx[dmx_id].sec_cnt_crc_fail[SEC_CNT_HW],
			dvb->dmx[dmx_id].sec_cnt[SEC_CNT_SW],
			dvb->dmx[dmx_id].sec_cnt_match[SEC_CNT_SW],
			dvb->dmx[dmx_id].sec_cnt_crc_fail[SEC_CNT_SW]);
	ret = sprintf(buf, "%s[ss]%#lx:%#lx:%#lx\n",
			tmp,
			dvb->dmx[dmx_id].sec_cnt[SEC_CNT_SS],
			dvb->dmx[dmx_id].sec_cnt_match[SEC_CNT_SS],
			dvb->dmx[dmx_id].sec_cnt_crc_fail[SEC_CNT_SS]);
	return ret;
}

int aml_regist_dmx_class(void)
{

	if (class_register(&aml_dmx_class) < 0)
		pr_error("register class error\n");

	return 0;
}

int aml_unregist_dmx_class(void)
{

	class_unregister(&aml_dmx_class);
	return 0;
}

static struct mconfig parser_configs[] = {
	MC_PU32("video_pts", &video_pts),
	MC_PU32("audio_pts", &audio_pts),
	MC_PU32("video_pts_bit32", &video_pts_bit32),
	MC_PU32("audio_pts_bit32", &audio_pts_bit32),
	MC_PU32("first_video_pts", &first_video_pts),
	MC_PU32("first_audio_pts", &first_audio_pts),
};

void aml_register_parser_mconfig(void)
{
	REG_PATH_CONFIGS("media.parser", parser_configs);
}

