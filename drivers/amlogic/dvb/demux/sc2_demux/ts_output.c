// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/of.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/dvb/dmx.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>

#include "sc2_control.h"
#include "ts_output.h"
#include "mem_desc.h"
#include "../../aucpu/aml_aucpu.h"
#include "../aml_dvb.h"
#include "../dmx_log.h"
#include <linux/fs.h>
#define KERNEL_ATRACE_TAG KERNEL_ATRACE_TAG_DEMUX
#include <trace/events/meson_atrace.h>

#define MAX_TS_PID_NUM              1024
#define MAX_SID_NUM                 64
#define MAX_REMAP_NUM               32
#define MAX_ES_NUM                  64
#define MAX_OUT_ELEM_NUM            128
#define MAX_PCR_NUM                 16

#define MAX_READ_BUF_LEN			(64 * 1024)
#define MAX_DVR_READ_BUF_LEN		(2 * 1024 * 1024)

#define MAX_FEED_NUM				32
/*protect cb_list/ts output*/
static struct mutex *ts_output_mutex;
static struct mutex es_output_mutex;

struct es_params_t {
	struct dmx_non_sec_es_header header;
	char last_last_header[16];
	char last_header[16];
	u8 have_header;
	u8 have_send_header;
	unsigned long data_start;
	unsigned int data_len;
	int es_overflow;
	int es_error_cn;
	u32 header_wp;
	int has_splice;
	unsigned int have_sent_len;
	u32 dirty_len;
};

struct ts_out {
	struct out_elem *pout;
	struct es_params_t *es_params;
	struct ts_out *pnext;
};

struct ts_out_task {
	int running;
	wait_queue_head_t wait_queue;
	struct task_struct *out_task;
	u16 flush_time_ms;
	struct timer_list out_timer;
	struct ts_out *ts_out_list;
};

struct pid_entry {
	u8 used;
	u16 id;
	u16 pid;
	u16 pid_mask;
	u8 dmx_id;
	u8 ref;
	struct out_elem *pout;
	struct pid_entry *pnext;
};

struct cb_entry {
	u8 id;
	u8 format;
	u8 ref;
	u8 demux_id;
	ts_output_cb cb;
	void *udata[MAX_FEED_NUM];
	struct cb_entry *next;
};

struct dump_file {
	loff_t file_pos;
	struct file *file_fp;
};

struct out_elem {
	u8 used;
	u8 sid;
	u8 enable;
	u8 ref;
	u8 dmx_id;
	enum output_format format;
	enum content_type type;
	int media_type;

	struct pid_entry *pid_list;
	struct es_entry *es_pes;
	struct chan_id *pchan;
	struct chan_id *pchan1;
	struct pid_entry *pcrpid_list;

	struct cb_entry *cb_sec_list;
	struct cb_entry *cb_ts_list;
	u16 flush_time_ms;
	int running;
	u8 output_mode;

	s32 aucpu_handle;
	u8 aucpu_start;
	unsigned long aucpu_mem_phy;
	unsigned long aucpu_mem;
	unsigned int aucpu_mem_size;
	unsigned int aucpu_read_offset;
	__u64 newest_pts;

	/*pts/dts for aucpu*/
	s32 aucpu_pts_handle;
	u8 aucpu_pts_start;
	unsigned long aucpu_pts_mem_phy;
	unsigned long aucpu_pts_mem;
	unsigned int aucpu_pts_mem_size;
	unsigned int aucpu_pts_r_offset;
	unsigned int aucpu_newest_pts_r_offset;

	struct dump_file dump_file;
	u8 use_external_mem;
	unsigned int decoder_rp_offset;
	char name[32];
	/*get es header mutex with get newest pts*/
	struct mutex pts_mutex;
	u8 ts_dump;
	int temi_index;
};

struct sid_entry {
	int used;
	int pid_entry_begin;
	int pid_entry_num;
};

struct remap_entry {
	int status;
	u8 stream_id;
	int remap_flag;
	int pid_entry;
	int pid;
	int pid_new;
};

struct es_entry {
	u8 used;
	u8 buff_id;
	u8 id;
	u8 dmx_id;
	int status;		//-1:off;
	int pid;
	struct out_elem *pout;
};

struct pcr_entry {
	u8 turn_on;
	u8 stream_id;
	int pcr_pid;
	int sid;
	struct out_elem *pout;
	int ref;
	int type;
	int pcr_total;
	int temi_total;
};

static struct pid_entry *pid_table;
static struct sid_entry sid_table[MAX_SID_NUM];
static struct remap_entry remap_table[MAX_REMAP_NUM];
static struct es_entry es_table[MAX_ES_NUM];
static struct out_elem *out_elem_table;
static struct pcr_entry pcr_table[MAX_PCR_NUM];
static struct ts_out_task ts_out_task_tmp;
static struct ts_out_task es_out_task_tmp;
static int timer_wake_up;
static int timer_es_wake_up;

#define dprint(fmt, args...) \
	dprintk(LOG_ERROR, debug_ts_output, "ts_output:" fmt, ## args)
#define dprintk_info(fmt, args...) \
	dprintk(LOG_ERROR, debug_ts_output, fmt, ## args)
#define pr_dbg(fmt, args...) \
	dprintk(LOG_DBG, debug_ts_output, "ts_output:" fmt, ## args)
#define pr_sec_dbg(fmt, args...) \
	dprintk(LOG_DBG, debug_section, "ts_output:" fmt, ## args)

MODULE_PARM_DESC(debug_ts_output, "\n\t\t Enable demux debug information");
static int debug_ts_output;
module_param(debug_ts_output, int, 0644);

MODULE_PARM_DESC(drop_dup, "\n\t\t drop duplicate packet");
static int drop_dup;
module_param(drop_dup, int, 0644);

MODULE_PARM_DESC(dump_video_es, "\n\t\t dump video es packet");
static int dump_video_es;
module_param(dump_video_es, int, 0644);

MODULE_PARM_DESC(dump_audio_es, "\n\t\t dump audio es packet");
static int dump_audio_es;
module_param(dump_audio_es, int, 0644);

MODULE_PARM_DESC(dump_dvr_ts, "\n\t\t dump dvr ts packet");
static int dump_dvr_ts;
module_param(dump_dvr_ts, int, 0644);

MODULE_PARM_DESC(es_count_one_time, "\n\t\t handle es count one time");
static int es_count_one_time = 10;
module_param(es_count_one_time, int, 0644);

MODULE_PARM_DESC(dump_pes, "\n\t\t dump pes packet");
static int dump_pes;
module_param(dump_pes, int, 0644);

MODULE_PARM_DESC(dump_section, "\n\t\t dump section packet");
static int dump_section;
module_param(dump_section, int, 0644);

MODULE_PARM_DESC(debug_section, "\n\t\t debug section");
static int debug_section;
module_param(debug_section, int, 0644);

MODULE_PARM_DESC(audio_es_len_limit, "\n\t\t debug section");
static int audio_es_len_limit = (40 * 1024);
module_param(audio_es_len_limit, int, 0644);

MODULE_PARM_DESC(video_es_splice, "\n\t\t video es splice");
static int video_es_splice;
module_param(video_es_splice, int, 0644);

MODULE_PARM_DESC(audio_es_splice, "\n\t\t audio es splice");
static int audio_es_splice;
module_param(audio_es_splice, int, 0644);

MODULE_PARM_DESC(ts_output_max_pid_num_per_sid, "\n\t\t max pid num per sid in si_table");
static int ts_output_max_pid_num_per_sid = 32;
module_param(ts_output_max_pid_num_per_sid, int, 0644);

MODULE_PARM_DESC(debug_es_len, "\n\t\t print es 5 bytes");
static int debug_es_len;
module_param(debug_es_len, int, 0644);

#define VIDEOES_DUMP_FILE   "/data/video_dump"
#define AUDIOES_DUMP_FILE   "/data/audio_dump"
#define DVR_DUMP_FILE       "/data/dvr_dump"
#define PES_DUMP_FILE		"/data/pes_dump"
#define TS_DUMP_FILE		"/data/ts_dump"
#define SECTION_DUMP_FILE	"/data/section_dump"

#define READ_CACHE_SIZE      (188)
#define INVALID_DECODE_RP	(0xFFFFFFFF)

static int out_flush_time = 10;
static int out_es_flush_time = 10;

static int _handle_es(struct out_elem *pout, struct es_params_t *es_params);
static int start_aucpu_non_es(struct out_elem *pout);
static int aucpu_bufferid_read(struct out_elem *pout,
			       char **pread, unsigned int len, int is_pts);
static void enforce_flush_cache(char *addr, unsigned int len);

static void dump_file_open(char *path, struct dump_file *dump_file_fp,
	int sid, int pid, int is_ts)
{
	int i = 0;
	char whole_path[255];
	struct file *file_fp;

	if (dump_file_fp->file_fp)
		return;

	//find new file name
	while (i < 999) {
		if (is_ts == 1)
			snprintf((char *)&whole_path, sizeof(whole_path),
			"%s_%03d.ts", path, i);
		else if (is_ts == 2)
			snprintf((char *)&whole_path, sizeof(whole_path),
			"%s_0x%0x_%03d.ts", path, sid, i);
		else if (is_ts == 3)
			snprintf((char *)&whole_path, sizeof(whole_path),
			"%s_0x%0x_0x%0x_%03d.ts", path, sid, pid, i);
		else
			snprintf((char *)&whole_path, sizeof(whole_path),
			"%s_0x%0x_0x%0x_%03d.es", path, sid, pid, i);

		file_fp = filp_open(whole_path, O_RDONLY, 0666);
		if (IS_ERR(file_fp))
			break;
		filp_close(file_fp, current->files);
		i++;
	}
	dump_file_fp->file_fp = filp_open(whole_path,
		O_CREAT | O_RDWR | O_APPEND, 0666);
	if (IS_ERR(dump_file_fp->file_fp)) {
		pr_err("create video dump [%s] file failed [%d]\n",
			whole_path, (int)PTR_ERR(dump_file_fp->file_fp));
		dump_file_fp->file_fp = NULL;
	} else {
		dprint("create dump [%s] success\n", whole_path);
	}
}

static void dump_file_write(char *buf,
		size_t count, struct dump_file *dump_file_fp)
{
	mm_segment_t old_fs;

	if (!dump_file_fp->file_fp) {
		pr_err("Failed to write video dump file fp is null\n");
		return;
	}
	if (count == 0)
		return;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (count != vfs_write(dump_file_fp->file_fp, buf, count,
			&dump_file_fp->file_pos))
		pr_err("Failed to write dump file\n");

	set_fs(old_fs);
}

static void dump_file_close(struct dump_file *dump_file_fp)
{
	if (dump_file_fp->file_fp) {
		vfs_fsync(dump_file_fp->file_fp, 0);
		filp_close(dump_file_fp->file_fp, current->files);
		dump_file_fp->file_fp = NULL;
	}
}

struct out_elem *_find_free_elem(void)
{
	int i = 0;

	for (i = 0; i < MAX_OUT_ELEM_NUM; i++) {
		struct out_elem *pout = &out_elem_table[i];

		if (!pout->used)
			return pout;
	}
	return NULL;
}

static int s_cur_sid_pid_index;

static int _init_sid_pid(int sid)
{
	struct sid_entry *psid = NULL;

	if (sid >= MAX_SID_NUM)
		return -1;
	psid = &sid_table[sid];
	if (psid->used)
		return 0;
	if (s_cur_sid_pid_index + ts_output_max_pid_num_per_sid > 1024)
		return -1;
	psid->used = 1;
	psid->pid_entry_begin = s_cur_sid_pid_index;
	psid->pid_entry_num = ts_output_max_pid_num_per_sid;
	tsout_config_sid_table(sid,
				       psid->pid_entry_begin / 4,
				       psid->pid_entry_num / 4);
	s_cur_sid_pid_index += ts_output_max_pid_num_per_sid;
	return 0;
}

static struct pid_entry *_malloc_pid_entry_slot(int sid, int pid)
{
	int i = 0;
	int start = 0;
	int end = 0;
	struct pid_entry *pid_slot;
	int j = 0;
	int jump = 0;
	int row_start = 0;

	if (sid >= MAX_SID_NUM) {
		pr_dbg("%s error sid:%d\n", __func__, sid);
		return NULL;
	}
	if (_init_sid_pid(sid) != 0) {
		pr_dbg("%s error init sid:%d\n", __func__, sid);
		return NULL;
	}

	start = sid_table[sid].pid_entry_begin;
	end = sid_table[sid].pid_entry_begin + sid_table[sid].pid_entry_num;

	for (i = start; i < end; i++) {
		if (i >= MAX_TS_PID_NUM) {
			pr_dbg("err sid:%d,pid start:%d, num:%d\n", sid,
			       sid_table[sid].pid_entry_begin,
			       sid_table[sid].pid_entry_num);
			return NULL;
		}

		pid_slot = &pid_table[i];

		if (!pid_slot->used) {
			/*check same pid, not at same row(there are 4 pid) */
			row_start = i / 4 * 4;
			for (j = row_start; j < row_start + 4; j++) {
				if (pid_table[j].used &&
				    pid_table[j].pid == pid) {
					jump = 1;
					break;
				}
			}
			if (jump) {
				pr_dbg("sid:%d at pos:%d, find pid:%d\n",
				       sid, j, pid);
				jump = 0;
				continue;
			}
			pr_dbg("sid:%d start:%d, end:%d, pid_entry:%d\n", sid,
			       start, end, pid_slot->id);
			return pid_slot;
		}
	}
	pr_dbg("err sid:%d,pid start:%d, num:%d\n", sid,
	       sid_table[sid].pid_entry_begin, sid_table[sid].pid_entry_num);

	return NULL;
}

static int _free_pid_entry_slot(struct pid_entry *pid_slot)
{
	if (pid_slot) {
		pid_slot->pid = -1;
		pid_slot->pid_mask = 0;
		pid_slot->used = 0;
	}
	return 0;
}

static struct es_entry *_malloc_es_entry_slot(void)
{
	int i = 0;

	for (i = 0; i < MAX_ES_NUM; i++) {
		struct es_entry *p = &es_table[i];

		if (p->used == 0) {
			p->used = 1;
			return p;
		}
	}

	return NULL;
}

static int _free_es_entry_slot(struct es_entry *es)
{
	if (es) {
		es->pid = 0;
		es->status = -1;
		es->used = 0;
		es->buff_id = 0;
	}
	return 0;
}

static int _check_timer_wakeup(void)
{
	if (timer_wake_up) {
		timer_wake_up = 0;
		return 1;
	}
	return 0;
}

static void _timer_ts_out_func(struct timer_list *timer)
{
//    dprint("wakeup ts_out_timer\n");
	if (ts_out_task_tmp.ts_out_list) {
		timer_wake_up = 1;
		wake_up_interruptible(&ts_out_task_tmp.wait_queue);
		mod_timer(&ts_out_task_tmp.out_timer,
		  jiffies + msecs_to_jiffies(out_flush_time));
	}
}

static int _check_timer_es_wakeup(void)
{
	if (timer_es_wake_up) {
		timer_es_wake_up = 0;
		return 1;
	}
	return 0;
}

static void _timer_es_out_func(struct timer_list *timer)
{
//  dprint("wakeup es_out_timer\n");

	if (es_out_task_tmp.ts_out_list) {
		timer_es_wake_up = 1;
		wake_up_interruptible(&es_out_task_tmp.wait_queue);
		mod_timer(&es_out_task_tmp.out_timer,
		  jiffies + msecs_to_jiffies(out_es_flush_time));
	}
}

static int out_sec_cb_list(struct out_elem *pout, char *buf, int size)
{
	int w_size = 0;
	int last_w_size = -1;
	struct cb_entry *ptmp = NULL;

	if (pout->running != TASK_RUNNING)
		return 0;

	ptmp = pout->cb_sec_list;
	while (ptmp && ptmp->cb) {
		w_size = ptmp->cb(pout, buf, size, ptmp->udata[0], 0, 0);
		if (last_w_size != -1 && w_size != last_w_size) {
			dprint("ref:%d ", pout->ref);
			dprint("add pid:%d cache for filter ",
			       pout->pid_list->pid);
			dprint("w:%d,last_w:%d\n", w_size, last_w_size);
		}
		last_w_size = w_size;
		ptmp = ptmp->next;
	}

	return w_size;
}

static int out_ts_cb_list(struct out_elem *pout, char *buf, int size,
	int req_len, int *req_ret)
{
	int w_size = 0;
	struct cb_entry *ptmp = NULL;

	if (pout->running != TASK_RUNNING)
		return 0;

	ptmp = pout->cb_ts_list;
	while (ptmp && ptmp->cb) {
		w_size = ptmp->cb(pout, buf, size, ptmp->udata[0],
				req_len, req_ret);
		if (req_ret && *req_ret == 1)
			return 0;
		ptmp = ptmp->next;
	}

	return w_size;
}

static int section_process(struct out_elem *pout)
{
	int ret = 0;
	int len = 0, w_size;
	char *pread;
	int remain_len = 0;

	if (pout->pchan->sec_level)
		start_aucpu_non_es(pout);

	len = MAX_READ_BUF_LEN;
	if (pout->pchan->sec_level)
		ret = aucpu_bufferid_read(pout, &pread, len, 0);
	else
		ret = SC2_bufferid_read(pout->pchan, &pread, len, 0);
	if (ret != 0) {
		if (pout->cb_sec_list) {
			w_size = out_sec_cb_list(pout, pread, ret);
			ATRACE_COUNTER(pout->name, w_size);
			pr_sec_dbg("%s pid:0x%0x send:%d, w:%d wwwwww\n", __func__,
			       pout->es_pes->pid, ret, w_size);
			if (dump_section == 0xFFFFFFFF)
				dump_file_open(SECTION_DUMP_FILE,
						&pout->dump_file,
						pout->sid,
						pout->es_pes->pid, 3);
			if (pout->dump_file.file_fp && dump_section == 0)
				dump_file_close(&pout->dump_file);
			if (pout->dump_file.file_fp && w_size)
				dump_file_write(pread, w_size, &pout->dump_file);

			remain_len = ret - w_size;
			if (remain_len) {
				if (pout->pchan->sec_level)
					pout->aucpu_read_offset =
						(pout->aucpu_read_offset +
						pout->aucpu_mem_size - remain_len)
						% pout->aucpu_mem_size;
				else
					SC2_bufferid_move_read_rp(pout->pchan, remain_len, 0);
			}
		}
		if (pout->cb_ts_list)
			out_ts_cb_list(pout, pread, ret, 0, 0);
	}
	return 0;
}

static void write_sec_ts_data(struct out_elem *pout, char *buf, int size)
{
	struct dmx_sec_ts_data sec_ts_data;

	sec_ts_data.buf_start = pout->pchan->mem_phy;
	sec_ts_data.buf_end = sec_ts_data.buf_start + pout->pchan->mem_size;
	sec_ts_data.data_start = (unsigned long)buf;
	sec_ts_data.data_end = (unsigned long)buf + size;

	out_ts_cb_list(pout, (char *)&sec_ts_data,
		       sizeof(struct dmx_sec_ts_data), 0, 0);
}

static int dvr_process(struct out_elem *pout)
{
	int ret = 0;
	int len = 0;
	char *pread;
	int flag = 0;
	int overflow = 0;

	if (pout->pchan->sec_level)
		flag = 1;

	len = MAX_DVR_READ_BUF_LEN;
	ret = SC2_bufferid_read(pout->pchan, &pread, len, flag);
	if (ret != 0) {
		if (flag == 0) {
			if (pout->cb_ts_list)
				out_ts_cb_list(pout, pread, ret, ret, &overflow);
			if (pout->ts_dump) {
				if (!pout->dump_file.file_fp)
					dump_file_open(TS_DUMP_FILE,
							&pout->dump_file, pout->sid, 0, 2);
				dump_file_write(pread, ret, &pout->dump_file);
			} else {
				if (dump_dvr_ts == 1) {
					dump_file_open(DVR_DUMP_FILE, &pout->dump_file,
						0, 0, 1);
					dump_file_write(pread, ret, &pout->dump_file);
				} else {
					dump_file_close(&pout->dump_file);
				}
			}
		} else if (pout->cb_ts_list && flag == 1) {
			if (dump_dvr_ts == 1) {
				dump_file_open(DVR_DUMP_FILE, &pout->dump_file,
					0, 0, 1);
				enforce_flush_cache(pread, ret);
				dump_file_write(pread - pout->pchan->mem_phy +
					pout->pchan->mem, ret,
					&pout->dump_file);
			} else {
				dump_file_close(&pout->dump_file);
			}
			write_sec_ts_data(pout, pread, ret);
		}
	}

	return 0;
}

static int temi_process(struct out_elem *pout)
{
	int ret = 0;
	char *pread = NULL;
	char *pts_dts = NULL;
	char temi[204] = {0};
	int header_len = 16;
	int payload = 188;
	int offset = 0;
	struct dmx_temi_data temi_data;

	while (header_len) {
		ret = SC2_bufferid_read(pout->pchan, &pts_dts, header_len, 0);
		if (ret != 0) {
			memcpy((char *)(temi + offset), pts_dts, ret);
			header_len -= ret;
			offset += ret;
		} else {
			break;
		}
	}

	if (header_len == 0) {
		temi_data.pts_dts_flag = temi[2] & 0xF;
		temi_data.dts = temi[3] & 0x1;
		temi_data.dts <<= 32;
		temi_data.dts |= ((__u64)temi[11]) << 24
						| ((__u64)temi[10]) << 16
						| ((__u64)temi[9]) << 8
						| ((__u64)temi[8]);
		temi_data.dts &= 0x1FFFFFFFF;

		temi_data.pts = temi[3] >> 1 & 0x1;
		temi_data.pts <<= 32;
		temi_data.pts |= ((__u64)temi[15]) << 24
						| ((__u64)temi[14]) << 16
						| ((__u64)temi[13]) << 8
						| ((__u64)temi[12]);

		temi_data.pts &= 0x1FFFFFFFF;

		while (payload) {
			ret = SC2_bufferid_read(pout->pchan,
						&pread, payload, 0);
			if (ret != 0) {
				memcpy((char *)(temi + offset), pread, ret);
				payload -= ret;
				offset += ret;
			} else {
				break;
			}
		}

		if (payload == 0) {
			memcpy(temi_data.temi, temi + 16, 188);
			out_ts_cb_list(pout, (char *)&temi_data,
								sizeof(temi_data), 0, 0);
			return 0;
		}
	}

	return -1;
}

static int find_pes_pts(char *buf, int size)
{
	int i = 0;
	int pts_dts_flags = 0;

	pr_dbg("%s size:%d\n", __func__, size);
	if (size >= 4)
		pr_dbg("sub 0x%0x 0x%0x 0x%0x 0x%0x\n", buf[0], buf[1], buf[2], buf[3]);
	for (i = 0; i < size - 4; i++) {
		if (buf[i] == 0 &&
			buf[i + 1] == 0 &&
			buf[i + 2] == 1 &&
			buf[i + 3] == 0xBD)	{
			if (i + 8 < size) {
				pts_dts_flags = ((buf[i + 7] >> 6) & 3);
				if ((pts_dts_flags & 2) && i + 14 < size) {
					u64 pts;

					pts = (((u64)buf[i + 9] & 0xe) << 29)
						| (buf[i + 10] << 22)
						| ((buf[i + 11] & 0xfe) << 14)
						| (buf[i + 12] << 7)
						| ((buf[i + 13] & 0xfe) >> 1);
					pr_dbg("%s pts:0x%lx\n", __func__, (unsigned long)pts);
				}
			}
		}
	}
	return 0;
}

static int _task_out_func(void *data)
{
	int timeout = 0;
	int ret = 0;
	int len = 0;
	struct ts_out *ptmp;
	char *pread = NULL;

	while (ts_out_task_tmp.running == TASK_RUNNING) {
		timeout =
		    wait_event_interruptible(ts_out_task_tmp.wait_queue,
						     _check_timer_wakeup());

		if (ts_out_task_tmp.running != TASK_RUNNING)
			break;

		mutex_lock(ts_output_mutex);

		ptmp = ts_out_task_tmp.ts_out_list;
		while (ptmp) {
			if (!ptmp->pout->enable) {
				ptmp = ptmp->pnext;
				continue;
			}
			if (ptmp->pout->format == SECTION_FORMAT) {
				section_process(ptmp->pout);
			} else if (ptmp->pout->format == DVR_FORMAT) {
				dvr_process(ptmp->pout);
			} else if (ptmp->pout->format == TEMI_FORMAT) {
				temi_process(ptmp->pout);
			} else {
				len = MAX_READ_BUF_LEN;
				if (ptmp->pout->pchan->sec_level) {
					start_aucpu_non_es(ptmp->pout);
					ret = aucpu_bufferid_read(ptmp->pout,
						&pread, len, 0);
				} else {
					ret = SC2_bufferid_read(ptmp->pout->pchan,
						&pread, len, 0);
				}

				if (ret != 0) {
					if (((dump_pes & 0xFFFF)  == ptmp->pout->es_pes->pid &&
						((dump_pes >> 16) & 0xFFFF) == ptmp->pout->sid) ||
							dump_pes == 0xFFFFFFFF)
						dump_file_open(PES_DUMP_FILE,
								&ptmp->pout->dump_file,
								ptmp->pout->sid,
								ptmp->pout->es_pes->pid, 0);
					if (ptmp->pout->dump_file.file_fp && dump_pes == 0)
						dump_file_close(&ptmp->pout->dump_file);
					if (ptmp->pout->dump_file.file_fp)
						dump_file_write(pread, ret, &ptmp->pout->dump_file);

					find_pes_pts(pread, ret);
					out_ts_cb_list(ptmp->pout, pread,
										ret, 0, 0);
				}
			}
			ptmp = ptmp->pnext;
		}
		mutex_unlock(ts_output_mutex);
	}
	ts_out_task_tmp.running = TASK_IDLE;
	return 0;
}

static int _task_es_out_func(void *data)
{
	int timeout = 0;
	struct ts_out *ptmp;
	int ret = 0;
	int count = 0;

	while (es_out_task_tmp.running == TASK_RUNNING) {
		timeout =
		    wait_event_interruptible(es_out_task_tmp.wait_queue,
						     _check_timer_es_wakeup());

		if (es_out_task_tmp.running != TASK_RUNNING)
			break;

		mutex_lock(&es_output_mutex);

		ptmp = es_out_task_tmp.ts_out_list;
		while (ptmp) {
			if (!ptmp->pout->enable) {
				ptmp = ptmp->pnext;
				continue;
			}
			if (ptmp->pout->format == ES_FORMAT) {
//				pr_dbg("get %s data\n",
//				       ptmp->pout->type == AUDIO_TYPE ?
//				       "audio" : "video");
				do {
					ret =
					    _handle_es(ptmp->pout,
						       ptmp->es_params);
					count++;
				} while (ret == 0 && count <= es_count_one_time);
//				pr_dbg("get %s data done\n",
//				       ptmp->pout->type == AUDIO_TYPE ?
//				       "audio" : "video");
			}
			count = 0;
			ptmp = ptmp->pnext;
		}
		mutex_unlock(&es_output_mutex);
	}
	es_out_task_tmp.running = TASK_IDLE;
	return 0;
}

//> 0: have dirty data
//0: success
//-1: exit
//-2: pid not same
static int get_non_sec_es_header(struct out_elem *pout, char *last_header,
				 char *cur_header,
				 struct dmx_non_sec_es_header *pheader)
{
	char *pts_dts = NULL;
	int ret = 0;
	int header_len = 0;
	int offset = 0;
	int pid = 0;
	unsigned int cur_es_bytes = 0;
	unsigned int last_es_bytes = 0;
	int count = 0;

//      pr_dbg("%s enter\n", __func__);
//	pr_dbg("%s pid:0x%0x\n", __func__, pout->es_pes->pid);

	header_len = 16;
	offset = 0;

	if (!pout->aucpu_pts_start &&
		 pout->aucpu_pts_handle >= 0) {
		if (wdma_get_active(pout->pchan1->id)) {
			ret = aml_aucpu_strm_start(pout->aucpu_pts_handle);
			if (ret >= 0) {
				pr_dbg("aucpu pts start success\n");
				pout->aucpu_pts_start = 1;
			} else {
				pr_dbg("aucpu start fail ret:%d\n",
					   ret);
				return -1;
			}
		} else {
			return -1;
		}
	}

	while (header_len) {
		if (pout->aucpu_pts_start)
			ret = aucpu_bufferid_read(pout, &pts_dts,
					header_len, 1);
		else
			ret = SC2_bufferid_read(pout->pchan1,
					&pts_dts, header_len, 0);
		if (ret != 0) {
			memcpy((char *)(cur_header + offset), pts_dts, ret);
			header_len -= ret;
			offset += ret;
		} else {
			return -3;
		}
		if (pout->running == TASK_DEAD || !pout->enable)
			return -1;
	}
	pid = (cur_header[1] & 0x1f) << 8 | cur_header[0];
	if (pout->es_pes->pid != pid) {
		dprint("%s pid diff req pid %d, ret pid:%d\n",
		       __func__, pout->es_pes->pid, pid);
		return -2;
	}
	count = pout->pchan1->last_w_addr >= pout->pchan1->r_offset ?
		(pout->pchan1->last_w_addr - pout->pchan1->r_offset) / 16 :
		(pout->pchan1->mem_size - pout->pchan1->r_offset +
		 pout->pchan1->last_w_addr) / 16;
	pr_dbg("r:0x%0x, w:0x%0x, count:%d\n",
			pout->pchan1->r_offset,
			pout->pchan1->last_w_addr, count);

	pr_dbg("cur header: 0x%0x 0x%0x 0x%0x 0x%0x\n",
	       *(unsigned int *)cur_header,
	       *((unsigned int *)cur_header + 1),
	       *((unsigned int *)cur_header + 2),
	       *((unsigned int *)cur_header + 3));

	pr_dbg("last header: 0x%0x 0x%0x 0x%0x 0x%0x\n",
	       *(unsigned int *)last_header,
	       *((unsigned int *)last_header + 1),
	       *((unsigned int *)last_header + 2),
	       *((unsigned int *)last_header + 3));

	cur_es_bytes = cur_header[7] << 24 |
	    cur_header[6] << 16 | cur_header[5] << 8 | cur_header[4];

	//when start, es_bytes not equal 0, there are dirty data
	if (last_header[0] == 0xff && last_header[1] == 0xff) {
		pr_dbg("%s dirty data:%d\n", __func__, cur_es_bytes);
		if (cur_es_bytes != 0)
			return cur_es_bytes;
	}

	pheader->pts_dts_flag = last_header[2] & 0xF;
	pheader->dts = last_header[3] & 0x1;
	pheader->dts <<= 32;
	pheader->dts |= ((__u64)last_header[11]) << 24
	    | ((__u64)last_header[10]) << 16
	    | ((__u64)last_header[9]) << 8
	    | ((__u64)last_header[8]);
	pheader->dts &= 0x1FFFFFFFF;

	pheader->pts = last_header[3] >> 1 & 0x1;
	pheader->pts <<= 32;
	pheader->pts |= ((__u64)last_header[15]) << 24
	    | ((__u64)last_header[14]) << 16
	    | ((__u64)last_header[13]) << 8
	    | ((__u64)last_header[12]);

	pheader->pts &= 0x1FFFFFFFF;
	last_es_bytes = last_header[7] << 24
	    | last_header[6] << 16 | last_header[5] << 8 | last_header[4];
	if (cur_es_bytes < last_es_bytes)
		pheader->len = 0xffffffff - last_es_bytes + cur_es_bytes + 1;
	else
		pheader->len = cur_es_bytes - last_es_bytes;

//	pr_dbg("sid:0x%0x pid:0x%0x len:%d,cur_es:0x%0x, last_es:0x%0x\n",
//	       pout->sid, pout->es_pes->pid,
//		   pheader->len, cur_es_bytes, last_es_bytes);
//	dprint("%s pid:0x%0x\n", __func__, pout->es_pes->pid);

	return 0;
}

static int re_get_non_sec_es_header(struct out_elem *pout, char *last_header,
				 char *cur_header,
				 struct dmx_non_sec_es_header *pheader)
{
	unsigned int cur_es_bytes = 0;
	unsigned int last_es_bytes = 0;

	cur_es_bytes = cur_header[7] << 24 |
		cur_header[6] << 16 | cur_header[5] << 8 | cur_header[4];

	pheader->pts_dts_flag = last_header[2] & 0xF;
	pheader->dts = last_header[3] & 0x1;
	pheader->dts <<= 32;
	pheader->dts |= ((__u64)last_header[11]) << 24
			| ((__u64)last_header[10]) << 16
			| ((__u64)last_header[9]) << 8
			| ((__u64)last_header[8]);
	pheader->dts &= 0x1FFFFFFFF;

	pheader->pts = last_header[3] >> 1 & 0x1;
	pheader->pts <<= 32;
	pheader->pts |= ((__u64)last_header[15]) << 24
			| ((__u64)last_header[14]) << 16
			| ((__u64)last_header[13]) << 8
			| ((__u64)last_header[12]);

	pheader->pts &= 0x1FFFFFFFF;
	last_es_bytes = last_header[7] << 24
			| last_header[6] << 16 | last_header[5] << 8 | last_header[4];
	if (cur_es_bytes < last_es_bytes)
		pheader->len = 0xffffffff - last_es_bytes + cur_es_bytes + 1;
	else
		pheader->len = cur_es_bytes - last_es_bytes;

	dprint("sid:0x%0x pid:0x%0x len:%d,cur_es:0x%0x, last_es:0x%0x\n",
		   pout->sid, pout->es_pes->pid,
		   pheader->len, cur_es_bytes, last_es_bytes);

	return 0;
}

#ifdef CHECK_AUD_ES
int find_audio_es_type(char *es_buf, int length)
{
	char *p;
	static int count;
	int match = 0;

	if (length < 2)
		return -1;

	p = es_buf;

	if ((p[0] == 0x0b && p[1] == 0x77) ||
		(p[0] == 0x77 && p[1] == 0x0b) ||
		(p[0] == 0xff && (p[1] & 0xe0) == 0xe0) ||
		(((p[0] & 0xff) == 0xff) && (p[1] & 0xf6) == 0xf0) ||
		(p[0] == 0x56 && ((p[1] & 0xe0) == 0xe0))) {
		match = 1;
	}

	if (match) {
		count++;
		if (count > 30) {
			pr_dbg("0x%0x 0x%0x\n", p[0], p[1]);
			count = 0;
		}
		return 0;
	}
	pr_dbg("es error 0x%0x, 0x%0x\n", p[0], p[1]);
	return -1;
}
#endif

static int write_es_data(struct out_elem *pout, struct es_params_t *es_params)
{
	int ret;
	char *ptmp;
	int len;
	struct dmx_non_sec_es_header header;
	int h_len = sizeof(struct dmx_non_sec_es_header);
	int es_len = 0;

	if (es_params->have_header == 0)
		return -1;

	es_params->header.len -= es_params->have_sent_len;
	es_params->have_sent_len = 0;

	if (es_params->have_send_header == 0) {
		memset(&header, 0, h_len);
		if (es_params->has_splice == 0) {
			memcpy(&header, &es_params->header, h_len);
			ATRACE_COUNTER(pout->name, es_params->header.pts);
			pr_dbg("%s pid:0x%0x sid:0x%0x flag:%d, pts:0x%lx, dts:0x%lx, len:%d\n",
		       pout->type == AUDIO_TYPE ? "audio" : "video",
			   pout->es_pes->pid,
			   pout->sid,
		       header.pts_dts_flag,
		       (unsigned long)header.pts,
		       (unsigned long)header.dts,
		       header.len);
		} else {
			header.len = es_params->header.len;
		pr_dbg("%s pid:0x%0x sid:0x%0x flag:%d, pts:0x%lx, dts:0x%lx, len:%d\n",
		   pout->type == AUDIO_TYPE ? "last splice audio" : "last splice video",
		   pout->es_pes->pid,
		   pout->sid,
		   header.pts_dts_flag,
		   (unsigned long)header.pts,
		   (unsigned long)header.dts,
		   header.len);
		}
		if (!(es_params->header.pts_dts_flag & 0x4) ||
			(pout->type == AUDIO_TYPE &&
			 es_params->header.len < audio_es_len_limit))
			out_ts_cb_list(pout, (char *)&header,
				h_len,
				(h_len + header.len),
				&es_params->es_overflow);
		es_params->have_send_header = 1;
	}

	es_len = es_params->header.len;
	len = es_len - es_params->data_len;
	ret = SC2_bufferid_read(pout->pchan, &ptmp, len, 0);
	if (ret) {
		if (!es_params->es_overflow)
			if (!(es_params->header.pts_dts_flag & 0x4) ||
				(pout->type == AUDIO_TYPE &&
				 es_params->header.len < audio_es_len_limit)) {
#ifdef CHECK_AUD_ES
				if (pout->type == AUDIO_TYPE && es_params->has_splice == 0)
					find_audio_es_type(ptmp, ret);
#endif
				out_ts_cb_list(pout, ptmp, ret, 0, 0);
			} else {
				; //do nothing
			}
		else
			pr_dbg("audio data lost\n");
		if (((dump_audio_es & 0xFFFF)  == pout->es_pes->pid &&
			((dump_audio_es >> 16) & 0xFFFF) == pout->sid) ||
			dump_audio_es == 0xFFFFFFFF)
			dump_file_open(AUDIOES_DUMP_FILE, &pout->dump_file,
				pout->sid, pout->es_pes->pid, 0);
		if (pout->dump_file.file_fp && dump_audio_es == 0)
			dump_file_close(&pout->dump_file);
		if (pout->dump_file.file_fp)
			dump_file_write(ptmp, ret, &pout->dump_file);

		es_params->data_len += ret;

		if (ret != len) {
			pr_dbg("%s total len:%d, remain:%d\n",
				   pout->type == AUDIO_TYPE ? "audio" : "video",
				   es_len,
				   es_len - es_params->data_len);
			if (pout->pchan->r_offset != 0)
				return -1;
			/*loop back ,read one time*/
			len = es_params->header.len - es_params->data_len;
			ret = SC2_bufferid_read(pout->pchan, &ptmp, len, 0);
			if (ret) {
				if (!es_params->es_overflow)
					if (!(es_params->header.pts_dts_flag & 0x4) ||
						(pout->type == AUDIO_TYPE &&
						 es_params->header.len < audio_es_len_limit))
						out_ts_cb_list(pout, ptmp, ret, 0, 0);
					else
						; //do nothing
				else
					pr_dbg("audio data lost\n");
				if (pout->dump_file.file_fp)
					dump_file_write(ptmp,
							ret, &pout->dump_file);
				es_params->data_len += ret;
				if (ret != len) {
					pr_dbg("%s total len:%d, remain:%d\n",
						   pout->type == AUDIO_TYPE ?
						   "audio" : "video",
						   es_len,
						   es_len - es_params->data_len);
					return -1;
				} else {
					return 0;
				}
			}
		} else {
			return 0;
		}
	}
	return -1;
}

static int clean_es_data(struct out_elem *pout, struct chan_id *pchan,
			 unsigned int len)
{
	int ret;
	char *ptmp;

	while (len) {
		ret = SC2_bufferid_read(pout->pchan, &ptmp, len, 0);
		if (ret != 0) {
			len -= ret;
		} else {
			dprint("%s ret:%d\n", __func__, len);
			return -1;
		}
		if (pout->running == TASK_DEAD || !pout->enable)
			return -1;
	}
	return 0;
}

static int start_aucpu_non_es(struct out_elem *pout)
{
	int ret;
	if (!pout || !pout->pchan)
		return -1;
	if (!pout->aucpu_start &&
		pout->aucpu_handle >= 0 &&
		wdma_get_active(pout->pchan->id)) {
		ret = aml_aucpu_strm_start(pout->aucpu_handle);
		if (ret >= 0) {
			pr_dbg("%s aucpu start success\n", __func__);
			pout->aucpu_start = 1;
		} else {
			pr_dbg("aucpu start fail ret:%d\n", ret);
			return -1;
		}
	}
	return 0;
}

static int create_aucpu_pts(struct out_elem *pout)
{
	struct aml_aucpu_strm_buf src;
	struct aml_aucpu_strm_buf dst;
	struct aml_aucpu_inst_config cfg;
	int ret;

	if (!pout || !pout->pchan1)
		return -1;

	if (pout->aucpu_pts_handle < 0 && pout->pchan1->sec_level) {
		src.phy_start = pout->pchan1->mem_phy;
		src.buf_size = pout->pchan1->mem_size;
		src.buf_flags = 0;

		pr_dbg("%s src aucpu pts phy:0x%lx, size:0x%0x\n",
		       __func__, src.phy_start, src.buf_size);

		pout->aucpu_pts_mem_size = pout->pchan1->mem_size;
		ret =
		    _alloc_buff(pout->aucpu_pts_mem_size, 0,
					&pout->aucpu_pts_mem,
					&pout->aucpu_pts_mem_phy);
		if (ret != 0) {
			dprint("aucpu pts alloc buf fail\n");
			return -1;
		}
		pr_dbg("%s dst aucpu pts mem:0x%lx, phy:0x%lx\n",
		       __func__, pout->aucpu_pts_mem, pout->aucpu_pts_mem_phy);

		dst.phy_start = pout->aucpu_pts_mem_phy;
		dst.buf_size = pout->aucpu_pts_mem_size;
		dst.buf_flags = 0;
		cfg.media_type = MEDIA_PTS_PACK;
		cfg.dma_chn_id = (pout->es_pes->pid << 8) |
			pout->pchan1->id;
		cfg.config_flags = 0;
		pout->aucpu_pts_handle = aml_aucpu_strm_create(&src,
				&dst, &cfg);
		if (pout->aucpu_pts_handle < 0) {
			_free_buff(pout->aucpu_pts_mem_phy,
				pout->aucpu_pts_mem_size, 0);
			pout->aucpu_pts_mem = 0;

			dprint("%s create aucpu pts fail, ret:%d\n",
			       __func__, pout->aucpu_pts_handle);
			return -1;
		}
		dprint("%s create aucpu pts inst success\n", __func__);
		pout->aucpu_pts_r_offset = 0;
		pout->aucpu_pts_start = 0;
		pout->aucpu_newest_pts_r_offset = 0;
	}
	return 0;
}

static int create_aucpu_inst(struct out_elem *pout)
{
	struct aml_aucpu_strm_buf src;
	struct aml_aucpu_strm_buf dst;
	struct aml_aucpu_inst_config cfg;
	int ret;

	if (!pout)
		return -1;

	if (pout->type == NONE_TYPE)
		return -1;

	if (pout->type == VIDEO_TYPE) {
		return create_aucpu_pts(pout);
	} else if (pout->type == AUDIO_TYPE) {
		ret = create_aucpu_pts(pout);
		if (ret != 0)
			return ret;
		if (!pout->pchan->sec_level)
			return 0;
	}
	/*now except the video, others will pass by aucpu */
	if (pout->aucpu_handle < 0) {
		src.phy_start = pout->pchan->mem_phy;
		src.buf_size = pout->pchan->mem_size;
		src.buf_flags = 0;

		pr_dbg("%s src aucpu phy:0x%lx, size:0x%0x\n",
		       __func__, src.phy_start, src.buf_size);

		pout->aucpu_mem_size = pout->pchan->mem_size;
		ret =
		    _alloc_buff(pout->aucpu_mem_size, 0, &pout->aucpu_mem,
				&pout->aucpu_mem_phy);
		if (ret != 0) {
			dprint("aucpu mem alloc fail\n");
			return -1;
		}
		pr_dbg("%s dst aucpu mem:0x%lx, phy:0x%lx\n",
		       __func__, pout->aucpu_mem, pout->aucpu_mem_phy);

		dst.phy_start = pout->aucpu_mem_phy;
		dst.buf_size = pout->aucpu_mem_size;
		dst.buf_flags = 0;
		cfg.media_type = pout->media_type;	/*MEDIA_MPX; */
		cfg.dma_chn_id = pout->pchan->id;
		cfg.config_flags = 0;
		pout->aucpu_handle = aml_aucpu_strm_create(&src, &dst, &cfg);
		if (pout->aucpu_handle < 0) {
			_free_buff(pout->aucpu_mem_phy,
				pout->aucpu_mem_size, 0);
			pout->aucpu_mem = 0;
			dprint("%s create aucpu fail, ret:%d\n",
			       __func__, pout->aucpu_handle);
		} else {
			dprint("%s create aucpu inst success\n", __func__);
		}
		pout->aucpu_read_offset = 0;
		pout->aucpu_start = 0;
	}
	return 0;
}

static unsigned int aucpu_read_pts_process(struct out_elem *pout,
				       unsigned int w_offset,
				       char **pread, unsigned int len, int is_pts)
{
	unsigned int buf_len = len;
	unsigned int data_len = 0;

	if (is_pts == 2) {
		int w_offset_align = 0;
		int pts_mem_offset;

		w_offset_align = w_offset / 16 * 16;
		if (w_offset_align == 0)
			pts_mem_offset = pout->aucpu_pts_mem_size - 16;
		else
			pts_mem_offset = w_offset_align - 16;
		dma_sync_single_for_cpu(aml_get_device(),
					(dma_addr_t)(pout->aucpu_pts_mem_phy + pts_mem_offset),
					16, DMA_FROM_DEVICE);
		*pread = (char *)(pout->aucpu_pts_mem + pts_mem_offset);
		pout->aucpu_newest_pts_r_offset = w_offset_align;
//		dprint("%s type:%d pts w:0x%0x, new pts r:0x%0x\n", __func__, pout->type,
//			   (u32)w_offset, (u32)(pout->aucpu_newest_pts_r_offset));
		return 16;
	}
	pr_dbg("%s type:%d pts w:0x%0x, r:0x%0x\n", __func__, pout->type,
	       (u32)w_offset, (u32)(pout->aucpu_pts_r_offset));
	if (w_offset > pout->aucpu_pts_r_offset) {
		data_len = min((w_offset - pout->aucpu_pts_r_offset), buf_len);
		if (data_len < 16)
			return 0;
		dma_sync_single_for_cpu(aml_get_device(),
					(dma_addr_t)(pout->aucpu_pts_mem_phy +
						      pout->aucpu_pts_r_offset),
					data_len, DMA_FROM_DEVICE);
		*pread = (char *)(pout->aucpu_pts_mem +
				pout->aucpu_pts_r_offset);
		pout->aucpu_pts_r_offset += data_len;
	} else {
		unsigned int part1_len = 0;

		part1_len = pout->aucpu_pts_mem_size - pout->aucpu_pts_r_offset;
		if (part1_len == 0) {
			data_len = min(w_offset, buf_len);
			if (data_len < 16)
				return 0;
			pout->aucpu_pts_r_offset = 0;
			*pread = (char *)(pout->aucpu_pts_mem +
				pout->aucpu_pts_r_offset);
			dma_sync_single_for_cpu(aml_get_device(), (dma_addr_t)
						(pout->aucpu_pts_mem_phy +
						 pout->aucpu_pts_r_offset),
						data_len, DMA_FROM_DEVICE);
			pout->aucpu_pts_r_offset += data_len;
			return data_len;
		}
		data_len = min(part1_len, buf_len);
		if (data_len < 16)
			return 0;

		*pread = (char *)(pout->aucpu_pts_mem +
				pout->aucpu_pts_r_offset);
		if (data_len < part1_len) {
			dma_sync_single_for_cpu(aml_get_device(), (dma_addr_t)
						(pout->aucpu_pts_mem_phy +
						 pout->aucpu_pts_r_offset),
						data_len, DMA_FROM_DEVICE);
			pout->aucpu_pts_r_offset += data_len;
		} else {
			data_len = part1_len;
			dma_sync_single_for_cpu(aml_get_device(), (dma_addr_t)
						(pout->aucpu_pts_mem_phy +
						 pout->aucpu_pts_r_offset),
						data_len, DMA_FROM_DEVICE);
			pout->aucpu_pts_r_offset = 0;
		}
	}
	if (len != data_len)
		pr_dbg("%s pts request:%d, ret:%d\n", __func__, len, data_len);
	return data_len;
}

static unsigned int aucpu_read_process(struct out_elem *pout,
				       unsigned int w_offset,
				       char **pread, unsigned int len,
					   int is_pts)
{
	unsigned int buf_len = len;
	unsigned int data_len = 0;

	if (is_pts)
		return aucpu_read_pts_process(pout, w_offset, pread, len, is_pts);

	pr_dbg("%s w:0x%0x, r:0x%0x\n", __func__,
	       (u32)w_offset, (u32)(pout->aucpu_read_offset));
	if (w_offset > pout->aucpu_read_offset) {
		data_len = min((w_offset - pout->aucpu_read_offset), buf_len);
		dma_sync_single_for_cpu(aml_get_device(),
					(dma_addr_t)(pout->aucpu_mem_phy +
						      pout->aucpu_read_offset),
					data_len, DMA_FROM_DEVICE);
		*pread = (char *)(pout->aucpu_mem + pout->aucpu_read_offset);
		pout->aucpu_read_offset += data_len;
	} else {
		unsigned int part1_len = 0;

		part1_len = pout->aucpu_mem_size - pout->aucpu_read_offset;
		if (part1_len == 0) {
			data_len = min(w_offset, buf_len);
			pout->aucpu_read_offset = 0;
			*pread = (char *)(pout->aucpu_mem +
					pout->aucpu_read_offset);
			dma_sync_single_for_cpu(aml_get_device(), (dma_addr_t)
						(pout->aucpu_mem_phy +
						 pout->aucpu_read_offset),
						data_len, DMA_FROM_DEVICE);
			pout->aucpu_read_offset += data_len;
			return data_len;
		}
		data_len = min(part1_len, buf_len);
		*pread = (char *)(pout->aucpu_mem + pout->aucpu_read_offset);
		if (data_len < part1_len) {
			dma_sync_single_for_cpu(aml_get_device(), (dma_addr_t)
						(pout->aucpu_mem_phy +
						 pout->aucpu_read_offset),
						data_len, DMA_FROM_DEVICE);
			pout->aucpu_read_offset += data_len;
		} else {
			data_len = part1_len;
			dma_sync_single_for_cpu(aml_get_device(), (dma_addr_t)
						(pout->aucpu_mem_phy +
						 pout->aucpu_read_offset),
						data_len, DMA_FROM_DEVICE);
			pout->aucpu_read_offset = 0;
		}
	}
	if (len != data_len)
		pr_dbg("%s request:%d, ret:%d\n", __func__, len, data_len);
	return data_len;
}

static int aucpu_bufferid_read(struct out_elem *pout,
			       char **pread, unsigned int len, int is_pts)
{
	struct aml_aucpu_buf_upd upd;
	unsigned int w_offset = 0;
	s32 handle;
	unsigned long mem_phy;
	unsigned int r_offset;

	if (is_pts) {
		handle = pout->aucpu_pts_handle;
		mem_phy = pout->aucpu_pts_mem_phy;
		r_offset = pout->aucpu_pts_r_offset;
	} else {
		handle = pout->aucpu_handle;
		mem_phy = pout->aucpu_mem_phy;
		r_offset = pout->aucpu_read_offset;
	}

	if (aml_aucpu_strm_get_dst(handle, &upd)
	    >= 0) {
		w_offset = upd.phy_cur_ptr - mem_phy;
		if (r_offset != w_offset)
			return aucpu_read_process(pout,
					w_offset, pread, len, is_pts);
	}
	return 0;
}

static int aucpu_bufferid_read_newest_pts(struct out_elem *pout,
			       char **pread)
{
	struct aml_aucpu_buf_upd upd;
	unsigned int w_offset = 0;
	s32 handle;
	unsigned long mem_phy;
	unsigned int r_offset;

	handle = pout->aucpu_pts_handle;
	mem_phy = pout->aucpu_pts_mem_phy;
	r_offset = pout->aucpu_newest_pts_r_offset;
	if (aml_aucpu_strm_get_dst(handle, &upd)
	    >= 0) {
		w_offset = upd.phy_cur_ptr - mem_phy;
		if (r_offset != w_offset)
			return aucpu_read_process(pout,
					w_offset, pread, 16, 2);
	}
	return 0;
}

static int write_aucpu_es_data(struct out_elem *pout,
	struct es_params_t *es_params, unsigned int isdirty)
{
	int ret;
	char *ptmp;
	struct dmx_non_sec_es_header header;
	int h_len = sizeof(struct dmx_non_sec_es_header);
	int es_len = 0;
	int len = 0;

//	pr_dbg("%s chan id:%d, isdirty:%d\n", __func__,
//	       pout->pchan->id, isdirty);

	if (es_params->have_header == 0)
		return -1;

	if (!pout->aucpu_start &&
		pout->format == ES_FORMAT &&
		pout->type == AUDIO_TYPE && pout->aucpu_handle >= 0) {
		if (wdma_get_active(pout->pchan->id)) {
			ret = aml_aucpu_strm_start(pout->aucpu_handle);
			if (ret >= 0) {
				pr_dbg("%s aucpu start success\n", __func__);
				pout->aucpu_start = 1;
			} else {
				pr_dbg("aucpu start fail ret:%d\n",
					   ret);
				return -1;
			}
		} else {
			return -1;
		}
	}

	es_params->header.len -= es_params->have_sent_len;
	es_params->have_sent_len = 0;

	if (es_params->have_send_header == 0) {
		memset(&header, 0, h_len);
		if (es_params->has_splice) {
			header.len = es_params->header.len;
			pr_dbg("%s pid:0x%0x sid:0x%0x flag:%d, pts:0x%lx, dts:0x%lx, len:%d\n",
		   pout->type == AUDIO_TYPE ? "last splice audio" : "last splice video",
		   pout->es_pes->pid,
		   pout->sid,
		   header.pts_dts_flag,
		   (unsigned long)header.pts,
		   (unsigned long)header.dts,
		   header.len);
		} else {
			memcpy(&header, &es_params->header, h_len);
			ATRACE_COUNTER(pout->name, es_params->header.pts);
			pr_dbg("%s pid:0x%0x sid:0x%0x flag:%d, pts:0x%lx, dts:0x%lx, len:%d\n",
		       pout->type == AUDIO_TYPE ? "audio" : "video",
			   pout->es_pes->pid,
			   pout->sid,
		       header.pts_dts_flag,
		       (unsigned long)header.pts,
		       (unsigned long)header.dts,
		       header.len);
		}

		if (!(es_params->header.pts_dts_flag & 0x4) ||
			(pout->type == AUDIO_TYPE &&
			 es_params->header.len < audio_es_len_limit))
			out_ts_cb_list(pout, (char *)&es_params->header,
				h_len,
				(h_len + header.len),
				&es_params->es_overflow);
		es_params->have_send_header = 1;
	}

	es_len = es_params->header.len;
	len = es_len - es_params->data_len;
	ret = aucpu_bufferid_read(pout, &ptmp, len, 0);
	if (ret) {
		if (!es_params->es_overflow)
			if (!(es_params->header.pts_dts_flag & 0x4) ||
				(pout->type == AUDIO_TYPE &&
				 es_params->header.len < audio_es_len_limit)) {
#ifdef CHECK_AUD_ES
				find_audio_es_type(ptmp, ret);
#endif
				out_ts_cb_list(pout, ptmp, ret, 0, 0);
			} else {
				; //do nothing
			}
		else
			pr_dbg("audio data lost\n");
		if (((dump_audio_es & 0xFFFF)  == pout->es_pes->pid &&
			((dump_audio_es >> 16) & 0xFFFF) == pout->sid) ||
			dump_audio_es == 0xFFFFFFFF)
			dump_file_open(AUDIOES_DUMP_FILE, &pout->dump_file,
				pout->sid, pout->es_pes->pid, 0);

		if (pout->dump_file.file_fp && dump_audio_es == 0)
			dump_file_close(&pout->dump_file);

		if (pout->dump_file.file_fp)
			dump_file_write(ptmp, ret, &pout->dump_file);

		es_params->data_len += ret;

		if (ret != len) {
			pr_dbg("%s total len:%d, remain:%d\n",
				   pout->type == AUDIO_TYPE ? "audio" : "video",
				   es_len,
				   es_len - es_params->data_len);
			return -1;
		} else {
			return 0;
		}
	}
	return -1;
}

static int write_aucpu_sec_es_data(struct out_elem *pout,
				   struct es_params_t *es_params)
{
	unsigned int len = es_params->header.len;
	struct dmx_sec_es_data sec_es_data;
	char *ptmp;
	int ret;

	if (es_params->header.len == 0)
		return -1;

	if (!pout->aucpu_start &&
		pout->format == ES_FORMAT &&
		pout->type == AUDIO_TYPE && pout->aucpu_handle >= 0) {
		if (wdma_get_active(pout->pchan->id)) {
			ret = aml_aucpu_strm_start(pout->aucpu_handle);
			if (ret >= 0) {
				pr_dbg("%s aucpu start success\n", __func__);
				pout->aucpu_start = 1;
			} else {
				pr_dbg("aucpu start fail ret:%d\n",
					   ret);
				return -1;
			}
		} else {
			return -1;
		}
	}

	es_params->header.len -= es_params->have_sent_len;
	es_params->have_sent_len = 0;

	len = es_params->header.len - es_params->data_len;
	ret = aucpu_bufferid_read(pout, &ptmp, len, 0);
	if (es_params->data_start == 0)
		es_params->data_start = (unsigned long)ptmp;

	es_params->data_len += ret;
	if (ret != len)
		return -1;

	/*s4/s4d, es header will add scb & pscp*/
	if ((es_params->header.pts_dts_flag & 0x4)) {
		pr_dbg("detect pscp is 1, will discard the es\n");
		es_params->data_start = 0;
		es_params->data_len = 0;
		return 0;
	}

	memset(&sec_es_data, 0, sizeof(struct dmx_sec_es_data));
	if (es_params->has_splice == 0) {
		sec_es_data.pts_dts_flag = es_params->header.pts_dts_flag;
		sec_es_data.dts = es_params->header.dts;
		sec_es_data.pts = es_params->header.pts;
	}
	sec_es_data.buf_start = pout->pchan->mem;
	sec_es_data.buf_end = pout->pchan->mem + pout->pchan->mem_size;
	sec_es_data.data_start = es_params->data_start;
	sec_es_data.data_end = (unsigned long)ptmp + len;

	if (es_params->has_splice == 0)
		pr_dbg("%s pid:0x%0x sid:0x%0x flag:%d, pts:0x%lx, dts:0x%lx, offset:0x%lx\n",
			pout->type == AUDIO_TYPE ? "audio" : "video",
		   pout->es_pes->pid,
		   pout->sid,
	       sec_es_data.pts_dts_flag, (unsigned long)sec_es_data.pts,
	       (unsigned long)sec_es_data.dts,
	       (unsigned long)(sec_es_data.data_start - sec_es_data.buf_start));
	else
		pr_dbg("last splice %s pid:0x%0x sid:0x%0x flag:%d, pts:0x%lx, dts:0x%lx, offset:0x%lx\n",
			pout->type == AUDIO_TYPE ? "audio" : "video",
		   pout->es_pes->pid,
		   pout->sid,
	       sec_es_data.pts_dts_flag, (unsigned long)sec_es_data.pts,
	       (unsigned long)sec_es_data.dts,
	       (unsigned long)(sec_es_data.data_start - sec_es_data.buf_start));

	out_ts_cb_list(pout, (char *)&sec_es_data,
		       sizeof(struct dmx_sec_es_data), 0, 0);

	es_params->data_start = 0;
	es_params->data_len = 0;
	return 0;
}

static int start_aucpu(struct out_elem *pout)
{
	int ret;

	if (pout->aucpu_handle < 0)
		return -1;

	if (!pout->aucpu_start &&
		pout->format == ES_FORMAT &&
		pout->type == AUDIO_TYPE && pout->aucpu_handle >= 0) {
		if (wdma_get_active(pout->pchan->id)) {
			ret = aml_aucpu_strm_start(pout->aucpu_handle);
			if (ret >= 0) {
				pr_dbg("%s aucpu start success\n", __func__);
				pout->aucpu_start = 1;
			} else {
				pr_dbg("aucpu start fail ret:%d\n",
					   ret);
				return -1;
			}
		}
	}

	return 0;
}

static int clean_aucpu_data(struct out_elem *pout, unsigned int len)
{
	int ret;
	char *ptmp;
	static int times;

	if (pout->aucpu_handle < 0)
		return len;

	if (!pout->aucpu_start &&
		pout->format == ES_FORMAT &&
		pout->type == AUDIO_TYPE && pout->aucpu_handle >= 0) {
		if (wdma_get_active(pout->pchan->id)) {
			ret = aml_aucpu_strm_start(pout->aucpu_handle);
			if (ret >= 0) {
				pr_dbg("%s aucpu start success\n", __func__);
				pout->aucpu_start = 1;
			} else {
				pr_dbg("aucpu start fail ret:%d\n",
					   ret);
				return len;
			}
		}
	}

	while (len) {
		ret = aucpu_bufferid_read(pout, &ptmp, len, 0);
		if (ret != 0) {
			len -= ret;
		} else {
			times++;
			dprint("%s ret:%d times:%d\n", __func__, len, times);
			return len;
		}
		if (pout->running == TASK_DEAD || !pout->enable)
			return len;
	}
	times = 0;
	return 0;
}

static void enforce_flush_cache(char *addr, unsigned int len)
{
	if (len == 0)
		return;

	dma_sync_single_for_cpu(aml_get_device(),
			(dma_addr_t)(addr),
			len, DMA_FROM_DEVICE);
}

static int write_sec_video_es_data(struct out_elem *pout,
				   struct es_params_t *es_params)
{
	unsigned int len = es_params->header.len;
	struct dmx_sec_es_data sec_es_data;
	char *ptmp;
	int ret;
	int flag = 0;

//	pr_dbg("%s pid:0x%0x enter\n", __func__, pout->es_pes->pid);
	if (es_params->header.len == 0)
		return -1;

	es_params->header.len -= es_params->have_sent_len;
	es_params->have_sent_len = 0;

	if (pout->pchan->sec_level)
		flag = 1;

	len = es_params->header.len - es_params->data_len;
	ret = SC2_bufferid_read(pout->pchan, &ptmp, len, flag);
	if (ret == 0)
		return -1;

	if (es_params->data_start == 0)
		es_params->data_start = (unsigned long)ptmp;

	es_params->data_len += ret;

	if (((dump_video_es & 0xFFFF)  == pout->es_pes->pid &&
		((dump_video_es >> 16) & 0xFFFF) == pout->sid) ||
		dump_video_es == 0XFFFFFFFF)
		dump_file_open(VIDEOES_DUMP_FILE, &pout->dump_file,
			pout->sid, pout->es_pes->pid, 0);

	if (pout->dump_file.file_fp && dump_video_es == 0)
		dump_file_close(&pout->dump_file);

	if (pout->dump_file.file_fp) {
		if (flag) {
			enforce_flush_cache(ptmp, ret);
			dump_file_write(ptmp - pout->pchan->mem_phy +
				pout->pchan->mem, ret,
				&pout->dump_file);
		} else {
			dump_file_write(ptmp, ret, &pout->dump_file);
		}
	}

	if (ret != len) {
//		if (pout->pchan->r_offset != 0)
//			return -1;

		/*if loop back , read one time */
		len = es_params->header.len - es_params->data_len;
		ret = SC2_bufferid_read(pout->pchan, &ptmp, len, flag);
		if (ret != 0) {
			es_params->data_len += ret;

			if (pout->dump_file.file_fp) {
				if (flag) {
					enforce_flush_cache(ptmp, ret);
					dump_file_write(ptmp - pout->pchan->mem_phy +
						pout->pchan->mem, ret,
						&pout->dump_file);
				} else {
					dump_file_write(ptmp, ret,
						&pout->dump_file);
				}
			}
			if (ret != len)
				dprint("get es data error2, req:%d, actual:%d\n",
					es_params->header.len, es_params->data_len);
		} else {
			len = es_params->data_len;
			dprint("get es data error1, req:%d, actual:%d\n",
				es_params->header.len, es_params->data_len);
		}
	}
	memset(&sec_es_data, 0, sizeof(struct dmx_sec_es_data));
	if (es_params->has_splice == 0) {
		sec_es_data.pts_dts_flag = es_params->header.pts_dts_flag;
		sec_es_data.dts = es_params->header.dts;
		sec_es_data.pts = es_params->header.pts;
		ATRACE_COUNTER(pout->name, sec_es_data.pts);
	}
	sec_es_data.buf_start = pout->pchan->mem_phy;
	sec_es_data.buf_end = pout->pchan->mem_phy + pout->pchan->mem_size;
	if (flag) {
		sec_es_data.data_start = es_params->data_start;
		sec_es_data.data_end = (unsigned long)ptmp + len;
	} else {
		sec_es_data.data_start = es_params->data_start -
			pout->pchan->mem + pout->pchan->mem_phy;
		sec_es_data.data_end = (unsigned long)ptmp -
			pout->pchan->mem + pout->pchan->mem_phy + len;
	}

//	if (sec_es_data.data_end > sec_es_data.data_start)
//		pr_dbg("video data start:0x%x, end:0x%x len:0x%x\n",
//			sec_es_data.data_start, sec_es_data.data_end,
//			(sec_es_data.data_end - sec_es_data.data_start));
//	else
//		pr_dbg("video data start:0x%x,data end:0x%x\n",
//				sec_es_data.data_start, sec_es_data.data_end);

	if (es_params->has_splice == 0) {
		pr_dbg("video pid:0x%0x sid:0x%0x flag:%d, pts:0x%lx, dts:0x%lx, offset:0x%lx\n",
			pout->es_pes->pid,
			pout->sid,
			sec_es_data.pts_dts_flag,
			(unsigned long)sec_es_data.pts,
			(unsigned long)sec_es_data.dts,
			(unsigned long)(sec_es_data.data_start -
				sec_es_data.buf_start));
			if (len >= debug_es_len && flag == 0 && debug_es_len) {
				int tt = 0;

				for (tt = 0; tt < debug_es_len; tt++)
					dprint("0x%0x ", ptmp[tt]);
				dprint("\n");
			}
	} else {
		pr_dbg("last splice video pid:0x%0x sid:0x%0x flag:%d, pts:0x%lx, dts:0x%lx, offset:0x%lx\n",
			pout->es_pes->pid,
			pout->sid,
			sec_es_data.pts_dts_flag,
			(unsigned long)sec_es_data.pts,
			(unsigned long)sec_es_data.dts,
			(unsigned long)(sec_es_data.data_start -
				sec_es_data.buf_start));
	}
	out_ts_cb_list(pout, (char *)&sec_es_data,
			sizeof(struct dmx_sec_es_data), 0, 0);

	es_params->data_start = 0;
	es_params->data_len = 0;
	return 0;
}

static int transfer_header(struct dmx_non_sec_es_header *pheader, char *cur_header)
{
	pheader->pts_dts_flag = cur_header[2] & 0xF;
	pheader->dts = cur_header[3] & 0x1;
	pheader->dts <<= 32;
	pheader->dts |= ((__u64)cur_header[11]) << 24
	    | ((__u64)cur_header[10]) << 16
	    | ((__u64)cur_header[9]) << 8
	    | ((__u64)cur_header[8]);
	pheader->dts &= 0x1FFFFFFFF;

	pheader->pts = cur_header[3] >> 1 & 0x1;
	pheader->pts <<= 32;
	pheader->pts |= ((__u64)cur_header[15]) << 24
	    | ((__u64)cur_header[14]) << 16
	    | ((__u64)cur_header[13]) << 8
	    | ((__u64)cur_header[12]);

	pheader->pts &= 0x1FFFFFFFF;
	return 0;
}

static int _find_es_header(struct out_elem *pout)
{
	int count = 0;
	unsigned int h_len = 0;

	//wait header write to es header buffer
	usleep_range(20, 30);
	do {
		if (pout->aucpu_pts_handle >= 0) {
			unsigned int w_offset;

			w_offset = SC2_bufferid_get_wp_offset(pout->pchan1);
			if (w_offset != pout->aucpu_pts_r_offset)
				return 1;
		} else {
			h_len = SC2_bufferid_get_data_len(pout->pchan1);
			if (h_len != 0)
				return 1;
		}
		count++;
	} while (count < 4);

	return 0;
}

static int _handle_es_splice(struct out_elem *pout, struct es_params_t *es_params)
{
	struct dmx_non_sec_es_header *pheader = &es_params->header;
	char *plast_header;
	unsigned int len = 0;
	unsigned int d_len = 0;
	unsigned int h_len = 0;
	struct dmx_sec_es_data sec_es_data;
	char *ptmp;
	int ret;
	int flag = 0;

	plast_header = (char *)&es_params->last_header;

	if (plast_header[0] == 0xff && plast_header[1] == 0xff)
		return -1;
	if (plast_header[2] & 0x4 && pout->type == AUDIO_TYPE)
		return -1;

	/*read data length first*/
	if (pout->aucpu_handle >= 0) {
		unsigned int w_offset;

		w_offset = SC2_bufferid_get_wp_offset(pout->pchan);
		if (w_offset >= pout->aucpu_read_offset)
			d_len = w_offset - pout->aucpu_read_offset;
		else
			d_len = pout->aucpu_mem_size - pout->aucpu_read_offset + w_offset;
	} else {
		d_len = SC2_bufferid_get_data_len(pout->pchan);
	}
	if (d_len == 0) {
//		dprint("pengcc no es data\n");
		return -1;
	}

	/*read es header length*/
	mutex_lock(&pout->pts_mutex);
	if (_find_es_header(pout)) {
		pr_dbg("find es header\n");
		mutex_unlock(&pout->pts_mutex);
		return -1;
	}
	mutex_unlock(&pout->pts_mutex);

	/*read es data*/
	if (pout->aucpu_handle >= 0) {
		ret = aucpu_bufferid_read(pout, &ptmp, d_len, 0);
	} else {
		if (pout->pchan->sec_level)
			flag = 1;

		ret = SC2_bufferid_read(pout->pchan, &ptmp, d_len, flag);
	}
	if (ret == 0) {
//		dprint("pengcc get no es data\n");
		return -1;
	}

	/*dump es data*/
	if (pout->type == VIDEO_TYPE) {
		if (((dump_video_es & 0xFFFF)  == pout->es_pes->pid &&
			((dump_video_es >> 16) & 0xFFFF) == pout->sid) ||
			dump_video_es == 0XFFFFFFFF)
			dump_file_open(VIDEOES_DUMP_FILE, &pout->dump_file,
				pout->sid, pout->es_pes->pid, 0);

		if (pout->dump_file.file_fp && dump_video_es == 0)
			dump_file_close(&pout->dump_file);

		if (pout->dump_file.file_fp) {
			if (flag) {
				enforce_flush_cache(ptmp, ret);
				dump_file_write(ptmp - pout->pchan->mem_phy +
					pout->pchan->mem, ret,
					&pout->dump_file);
			} else {
				dump_file_write(ptmp, ret, &pout->dump_file);
			}
		}
	} else {
		if (((dump_audio_es & 0xFFFF)  == pout->es_pes->pid &&
			((dump_audio_es >> 16) & 0xFFFF) == pout->sid) ||
			dump_audio_es == 0xFFFFFFFF)
			dump_file_open(AUDIOES_DUMP_FILE, &pout->dump_file,
				pout->sid, pout->es_pes->pid, 0);

		if (pout->dump_file.file_fp && dump_audio_es == 0)
			dump_file_close(&pout->dump_file);

		if (pout->dump_file.file_fp)
			dump_file_write(ptmp, ret, &pout->dump_file);
	}

	/*send data to dvb-core */
	len = ret;

	if (pout->output_mode) {
		memset(&sec_es_data, 0, sizeof(struct dmx_sec_es_data));
		if (es_params->have_sent_len == 0) {
			transfer_header(pheader, plast_header);

			sec_es_data.pts_dts_flag = es_params->header.pts_dts_flag;
			sec_es_data.dts = es_params->header.dts;
			sec_es_data.pts = es_params->header.pts;
			if (len >= debug_es_len && flag == 0 && debug_es_len) {
				int tt = 0;

				for (tt = 0; tt < debug_es_len; tt++)
					dprint("0x%0x ", ptmp[tt]);
				dprint("\n");
			}
		}
		sec_es_data.buf_start = pout->pchan->mem_phy;
		sec_es_data.buf_end = pout->pchan->mem_phy + pout->pchan->mem_size;
		if (flag) {
			sec_es_data.data_start = (unsigned long)ptmp;
			sec_es_data.data_end = (unsigned long)ptmp + len;
		} else {
			sec_es_data.data_start = (unsigned long)ptmp -
				pout->pchan->mem + pout->pchan->mem_phy;
			sec_es_data.data_end = (unsigned long)ptmp -
				pout->pchan->mem + pout->pchan->mem_phy + len;
		}
		es_params->have_sent_len += len;
		es_params->has_splice++;

		pr_dbg("%d %s pid:0x%0x sid:0x%0x flag:%d, pts:0x%lx, dts:0x%lx, offset:0x%lx\n",
			es_params->has_splice,
			pout->type == VIDEO_TYPE ? "splice video" : "splice audio",
			pout->es_pes->pid,
		pout->sid,
		sec_es_data.pts_dts_flag,
		(unsigned long)sec_es_data.pts,
		(unsigned long)sec_es_data.dts,
		(unsigned long)(sec_es_data.data_start -
			sec_es_data.buf_start));

		out_ts_cb_list(pout, (char *)&sec_es_data,
				sizeof(struct dmx_sec_es_data), 0, 0);
		return 0;
	}
	h_len = sizeof(struct dmx_non_sec_es_header);
	if (es_params->has_splice == 0) {
		transfer_header(pheader, plast_header);
		es_params->header.len = len;
	} else {
		memset((char *)&es_params->header, 0, h_len);
		es_params->header.len = len;
	}
	es_params->have_sent_len += len;
	es_params->has_splice++;

	pr_dbg("%d %s pid:0x%0x sid:0x%0x flag:%d, pts:0x%lx, dts:0x%lx, len:%d\n",
		   es_params->has_splice,
		   pout->type == AUDIO_TYPE ? "splice audio" : "splice video",
		   pout->es_pes->pid,
		   pout->sid,
		   es_params->header.pts_dts_flag,
		   (unsigned long)es_params->header.pts,
		   (unsigned long)es_params->header.dts,
		   es_params->header.len);

	out_ts_cb_list(pout, (char *)&es_params->header,
		h_len,
		(h_len + len),
		&es_params->es_overflow);
	if (!es_params->es_overflow)
		out_ts_cb_list(pout, ptmp, ret, 0, 0);
	return 0;
}

static int _handle_es(struct out_elem *pout, struct es_params_t *es_params)
{
	int ret = 0;
	unsigned int dirty_len = 0;
	char cur_header[16];
	char *pcur_header;
	char *plast_header;
	struct dmx_non_sec_es_header *pheader = &es_params->header;
	unsigned int len = 0;
	char *pheader_again;

	memset(&cur_header, 0, sizeof(cur_header));
	pcur_header = (char *)&cur_header;
	plast_header = (char *)&es_params->last_header;

//      pr_dbg("enter es %s\n", pout->type ? "audio" : "video");
//      pr_dbg("%s enter,line:%d\n", __func__, __LINE__);

	if (pout->running != TASK_RUNNING)
		return -1;

	if (es_params->have_header == 0) {
		mutex_lock(&pout->pts_mutex);
		//dirty len need clean first.
		if (pout->pchan->sec_level &&
			pout->type != VIDEO_TYPE &&
			es_params->dirty_len) {
			ret = clean_aucpu_data(pout, es_params->dirty_len);
			es_params->dirty_len = ret;
			if (ret != 0) {
				mutex_unlock(&pout->pts_mutex);
				return 0;
			}
		}
		ret =
		    get_non_sec_es_header(pout, plast_header, pcur_header,
					  pheader);
		mutex_unlock(&pout->pts_mutex);
		if (ret < 0) {
			if (ret == -3) {
				if (pout->type == VIDEO_TYPE && video_es_splice) {
					if (pout->output_mode || pout->pchan->sec_level)
						_handle_es_splice(pout, es_params);
				} else if (pout->type == AUDIO_TYPE && audio_es_splice) {
					_handle_es_splice(pout, es_params);
				}
			}
			return -1;
		} else if (ret > 0) {
			dirty_len = ret;
			if (pout->pchan->sec_level &&
					pout->type != VIDEO_TYPE) {
				start_aucpu(pout);
				es_params->dirty_len = dirty_len;
			} else {
				ret = clean_es_data(pout,
						pout->pchan, dirty_len);
			}
			memcpy(&es_params->last_last_header,
				&es_params->last_header, 16);
			memcpy(&es_params->last_header, pcur_header,
					sizeof(es_params->last_header));
			if (pout->type == VIDEO_TYPE)
				dprint("video: clean dirty len:0x%0x\n", dirty_len);
			else
				dprint("audio: record dirty len:0x%0x\n", dirty_len);
			return 0;
		}
		if (pheader->len == 0) {
			memcpy(&es_params->last_last_header,
				&es_params->last_header, 16);
			memcpy(&es_params->last_header, pcur_header,
					sizeof(es_params->last_header));
			if (get_demux_feature(SUPPORT_PSCP)) {
				if (!(pheader->pts_dts_flag & 0x4) &&
					!(pheader->pts_dts_flag & 0x8))
					return 0;

				if (pout->output_mode) {
					if (pout->type == VIDEO_TYPE || pout->type == AUDIO_TYPE) {
						struct dmx_sec_es_data sec_es_data;

						memset(&sec_es_data, 0,
							sizeof(struct dmx_sec_es_data));
						sec_es_data.pts_dts_flag =
							es_params->header.pts_dts_flag & 0xC;
						out_ts_cb_list(pout, (char *)&sec_es_data,
							sizeof(struct dmx_sec_es_data), 0, 0);
					}
				} else {
					if (pout->type == VIDEO_TYPE || pout->type == AUDIO_TYPE) {
						struct dmx_non_sec_es_header es_data_header;

						memset(&es_data_header, 0,
							sizeof(struct dmx_non_sec_es_header));
						es_data_header.pts_dts_flag =
							es_params->header.pts_dts_flag & 0xC;
						out_ts_cb_list(pout, (char *)&es_data_header,
							sizeof(struct dmx_non_sec_es_header), 0, 0);
					}
				}
			}
			return 0;
		}
		if (pheader->len <= es_params->have_sent_len) {
			es_params->have_header = 0;
			es_params->have_send_header = 0;
			es_params->data_len = 0;
			es_params->data_start = 0;
			es_params->es_overflow = 0;
			es_params->have_sent_len = 0;
			es_params->has_splice = 0;
			memcpy(&es_params->last_last_header,
				&es_params->last_header, 16);
			memcpy(&es_params->last_header, pcur_header,
				sizeof(es_params->last_header));
			return 0;
		}
		if (pout->aucpu_handle >= 0) {
			unsigned int w_offset;

			w_offset = SC2_bufferid_get_wp_offset(pout->pchan);
			if (w_offset >= pout->aucpu_read_offset)
				len = w_offset - pout->aucpu_read_offset;
			else
				len = pout->aucpu_mem_size - pout->aucpu_read_offset + w_offset;
		} else {
			len = SC2_bufferid_get_data_len(pout->pchan);
		}
		if (pheader->len - es_params->have_sent_len > len) {
			dprint("compute len:%d, es len:%d\n", pheader->len, len);
			//re read header data again
			if (pout->aucpu_pts_start) {
				goto error_handle;
			} else {
				SC2_bufferid_read_header_again(pout->pchan1, &pheader_again);
				if (memcmp(pcur_header, pheader_again, 0x10) != 0) {
					dprint("cur header: 0x%0x 0x%0x 0x%0x 0x%0x\n",
				       *(unsigned int *)pcur_header,
				       *((unsigned int *)pcur_header + 1),
				       *((unsigned int *)pcur_header + 2),
				       *((unsigned int *)pcur_header + 3));
					dprint("read again header: 0x%0x 0x%0x 0x%0x 0x%0x\n",
					   *(unsigned int *)pheader_again,
					   *((unsigned int *)pheader_again + 1),
					   *((unsigned int *)pheader_again + 2),
					   *((unsigned int *)pheader_again + 3));
					memcpy(pcur_header, pheader_again, 0x10);
					re_get_non_sec_es_header(pout, plast_header, pcur_header,
					  pheader);
					if (pheader->len - es_params->have_sent_len > len) {
						dprint("two compute len:%d, es len:%d\n",
							pheader->len, len);
						goto error_handle;
					}
				} else {
					goto error_handle;
				}
			}
		}
		memcpy(&es_params->last_last_header,
			&es_params->last_header, 16);
		memcpy(&es_params->last_header, pcur_header,
				sizeof(es_params->last_header));
		es_params->have_header = 1;
	}

	if (pout->output_mode || pout->pchan->sec_level) {
		if (es_params->have_header == 0)
			return -1;

		if (pout->type == VIDEO_TYPE) {
			ret = write_sec_video_es_data(pout, es_params);
		} else {
			//to do for use aucpu to handle non-video data
			if (pout->output_mode) {
				ret =
				    write_aucpu_sec_es_data(pout, es_params);
			} else {
				ret = write_aucpu_es_data(pout, es_params, 0);
			}
		}
		if (ret == 0) {
			es_params->have_header = 0;
			es_params->have_send_header = 0;
			es_params->data_len = 0;
			es_params->data_start = 0;
			es_params->es_overflow = 0;
			es_params->have_sent_len = 0;
			es_params->has_splice = 0;
			return 0;
		} else {
			return -1;
		}
	} else {
		if (es_params->have_header) {
			ret = write_es_data(pout, es_params);
			if (ret == 0) {
				es_params->have_header = 0;
				es_params->have_send_header = 0;
				es_params->data_len = 0;
				es_params->data_start = 0;
				es_params->es_overflow = 0;
				es_params->have_sent_len = 0;
				es_params->has_splice = 0;
				return 0;
			} else {
				return -1;
			}
		}
	}
	return -1;
error_handle:
	memcpy(&es_params->last_last_header,
		&es_params->last_header, 16);
	memcpy(&es_params->last_header, pcur_header,
			sizeof(es_params->last_header));

	es_params->header_wp =
		SC2_bufferid_get_wp_offset(pout->pchan1);
	es_params->es_error_cn++;
	dprint("error: es len: 0x%0x, err count:%d\n",
		pheader->len, es_params->es_error_cn);
	return 0;
}

static void add_ts_out_list(struct ts_out_task *head, struct ts_out *ts_out_tmp)
{
	struct ts_out *cur_tmp = NULL;

	cur_tmp = head->ts_out_list;
	while (cur_tmp) {
		if (!cur_tmp->pnext)
			break;
		cur_tmp = cur_tmp->pnext;
	}
	if (cur_tmp)
		cur_tmp->pnext = ts_out_tmp;
	else
		head->ts_out_list = ts_out_tmp;
}

static void remove_ts_out_list(struct out_elem *pout, struct ts_out_task *head)
{
	struct ts_out *ts_out_pre = NULL;
	struct ts_out *cur_tmp = NULL;

	cur_tmp = head->ts_out_list;
	ts_out_pre = cur_tmp;
	while (cur_tmp) {
		if (cur_tmp->pout == pout) {
			if (cur_tmp == head->ts_out_list)
				head->ts_out_list = cur_tmp->pnext;
			else
				ts_out_pre->pnext = cur_tmp->pnext;

			kfree(cur_tmp->es_params);
			kfree(cur_tmp);
			break;
		}
		ts_out_pre = cur_tmp;
		cur_tmp = cur_tmp->pnext;
	}
}

/**
 * ts_output_init
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_init(void)
{
	int i = 0;
	int times = 0;
	struct aml_dvb *advb = aml_get_dvb_device();

	do {
	} while (!tsout_get_ready() && times++ < 20);

	memset(&sid_table, 0, sizeof(sid_table));

	pid_table = vmalloc(sizeof(*pid_table) * MAX_TS_PID_NUM);
	if (!pid_table) {
		dprint("%s malloc fail\n", __func__);
		return -1;
	}

	memset(pid_table, 0, sizeof(struct pid_entry) * MAX_TS_PID_NUM);
	for (i = 0; i < MAX_TS_PID_NUM; i++) {
		struct pid_entry *pid_slot = &pid_table[i];

		pid_slot->id = i;
	}

	memset(&es_table, 0, sizeof(es_table));
	for (i = 0; i < MAX_ES_NUM; i++) {
		struct es_entry *es_slot = &es_table[i];

		es_slot->id = i;
	}

	memset(&remap_table, 0, sizeof(remap_table));
	for (i = 0; i < MAX_REMAP_NUM; i++) {
		struct remap_entry *remap_slot = &remap_table[i];

		remap_slot->pid_entry = i;
	}

	out_elem_table = vmalloc(sizeof(*out_elem_table)
				 * MAX_OUT_ELEM_NUM);
	if (!out_elem_table) {
		dprint("%s malloc fail\n", __func__);
		vfree(pid_table);
		return -1;
	}
	memset(out_elem_table, 0, sizeof(struct out_elem) * MAX_OUT_ELEM_NUM);
//      memset(&out_elem_table, 0, sizeof(out_elem_table));
	memset(&pcr_table, 0, sizeof(pcr_table));

	ts_out_task_tmp.running = TASK_RUNNING;
	ts_out_task_tmp.flush_time_ms = out_flush_time;
	ts_out_task_tmp.ts_out_list = NULL;
	ts_output_mutex = &advb->mutex;

	init_waitqueue_head(&ts_out_task_tmp.wait_queue);
	timer_setup(&ts_out_task_tmp.out_timer, _timer_ts_out_func, 0);
	add_timer(&ts_out_task_tmp.out_timer);

	ts_out_task_tmp.out_task =
	    kthread_run(_task_out_func, (void *)NULL, "ts_out_task");
	if (!ts_out_task_tmp.out_task)
		dprint("create ts_out_task fail\n");

	es_out_task_tmp.running = TASK_RUNNING;
	es_out_task_tmp.flush_time_ms = out_es_flush_time;
	mutex_init(&es_output_mutex);
	es_out_task_tmp.ts_out_list = NULL;

	init_waitqueue_head(&es_out_task_tmp.wait_queue);
	timer_setup(&es_out_task_tmp.out_timer, _timer_es_out_func, 0);
	add_timer(&es_out_task_tmp.out_timer);

	es_out_task_tmp.out_task =
	    kthread_run(_task_es_out_func, (void *)NULL, "es_out_task");
	if (!es_out_task_tmp.out_task)
		dprint("create es_out_task fail\n");

	return 0;
}

int ts_output_sid_debug(void)
{
	int i = 0;
	int dmxdev_num;
	struct sid_entry *psid = NULL;

	memset(&sid_table, 0, sizeof(sid_table));
	dmxdev_num = 32;
	/*for every dmx dev, it will use 2 sids,
	 * one is demod, another is local
	 */
	ts_output_max_pid_num_per_sid =
	    MAX_TS_PID_NUM / (2 * dmxdev_num * 4) * 4;

	for (i = 0; i < dmxdev_num; i++) {
		psid = &sid_table[i];
		psid->used = 1;
		psid->pid_entry_begin = ts_output_max_pid_num_per_sid * 2 * i;
		psid->pid_entry_num = ts_output_max_pid_num_per_sid;
		pr_dbg("%s sid:%d,pid start:%d, len:%d\n",
		       __func__, i, psid->pid_entry_begin, psid->pid_entry_num);
		tsout_config_sid_table(i,
				       psid->pid_entry_begin / 4,
				       psid->pid_entry_num / 4);

		psid = &sid_table[i + 32];
		psid->used = 1;
		psid->pid_entry_begin =
		    ts_output_max_pid_num_per_sid * (2 * i + 1);
		psid->pid_entry_num = ts_output_max_pid_num_per_sid;

		pr_dbg("%s sid:%d, pid start:%d, len:%d\n", __func__,
		       i + 32, psid->pid_entry_begin, psid->pid_entry_num);
		tsout_config_sid_table(i + 32,
				       psid->pid_entry_begin / 4,
				       psid->pid_entry_num / 4);
	}

	return 0;
}

int ts_output_destroy(void)
{
	if (out_elem_table)
		vfree(out_elem_table);

	out_elem_table = NULL;

	if (pid_table)
		vfree(pid_table);

	pid_table = NULL;
	return 0;
}

static struct remap_entry *ts_output_find_remap_entry(int sid, int pid)
{
	struct remap_entry *remap_slot = NULL;
	int i;

	for (i = 0; i < MAX_REMAP_NUM; i++) {
		if (remap_table[i].status == 1 &&
		    remap_table[i].stream_id == sid &&
		    remap_table[i].pid == pid) {
			remap_slot = &remap_table[i];
			break;
		}
	}

	return remap_slot;
}

static struct remap_entry *ts_output_allocate_remap_entry(void)
{
	struct remap_entry *remap_slot = NULL;
	int i;

	for (i = 0; i < MAX_REMAP_NUM; i++) {
		if (remap_table[i].status == 0) {
			remap_slot = &remap_table[i];
			break;
		}
	}

	return remap_slot;
}

static void ts_output_free_remap_entry(int sid, int pid)
{
	struct remap_entry *remap_slot = ts_output_find_remap_entry(sid, pid);

	if (remap_slot) {
		remap_slot->status = 0;
		remap_slot->stream_id = 0;
		remap_slot->remap_flag = 0;
		remap_slot->pid = 0;
		remap_slot->pid_new = 0;
	}
}

/**
 * remap pid
 * \param sid: stream id
 * \param pid: original pid
 * \param new_pid: replace pid
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_remap_pid(int sid, int pid, int new_pid)
{
	struct remap_entry *remap_slot = NULL;

	remap_slot = ts_output_find_remap_entry(sid, pid);
	if (!remap_slot) {
		if (new_pid < 0)
			return 0;

		remap_slot = ts_output_allocate_remap_entry();
		if (!remap_slot) {
			dprint("%s out of remap entry\n", __func__);
			return -1;
		}
	}

	remap_slot->status = 1;
	remap_slot->stream_id = sid;
	remap_slot->remap_flag = 1;
	remap_slot->pid = pid;
	remap_slot->pid_new = new_pid;

	pr_dbg("%s id : %d, pid : %d, new pid : %d\n",
			__func__, remap_slot->pid_entry,
			remap_slot->pid, remap_slot->pid_new);

	tsout_config_remap_table(remap_slot->pid_entry, sid, pid, new_pid);

	if (new_pid < 0)
		ts_output_free_remap_entry(sid, pid);

	return 0;
}

/**
 * set pid pcr
 * \param sid: stream id
 * \param pcr_num
 * \param pcrpid
 * \retval 0:success.
 * \retval -1:fail.
 * \note:pcrpid == -1, it will close
 */
int ts_output_set_pcr(int sid, int pcr_num, int pcrpid)
{
	pr_dbg("%s pcr_num:%d,pid:%d,ref=%d\n", __func__, pcr_num, pcrpid, pcr_table[pcr_num].ref);
	if (pcr_num >= MAX_PCR_NUM) {
		dprint("%s num:%d invalid\n", __func__, pcr_num);
		return -1;
	}

	if (pcrpid == -1) {
		if (pcr_table[pcr_num].ref <= 0) {
			pcr_table[pcr_num].turn_on = 0;
			pcr_table[pcr_num].stream_id = -1;
			pcr_table[pcr_num].pcr_pid = -1;
			pcr_table[pcr_num].sid = -1;
			tsout_config_pcr_table(pcr_num, -1, sid);
		}
	} else {
		pcr_table[pcr_num].turn_on = 1;
		pcr_table[pcr_num].stream_id = sid;
		pcr_table[pcr_num].pcr_pid = pcrpid;
		pcr_table[pcr_num].sid = sid;
		tsout_config_pcr_table(pcr_num, pcrpid, sid);
	}
	return 0;
}

/**
 * get pcr value
 * \param pcr_num
 * \param pcr:pcr value
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_get_pcr(int pcr_num, uint64_t *pcr)
{
	if (pcr_num >= MAX_PCR_NUM) {
		dprint("%s num:%d invalid\n", __func__, pcr_num);
		return -1;
	}
	if (!pcr_table[pcr_num].turn_on) {
		dprint("%s num:%d close\n", __func__, pcr_num);
		return -1;
	}
	tsout_config_get_pcr(pcr_num, pcr);
	return 0;
}

struct out_elem *ts_output_find_same_section_pid(int sid, int pid)
{
	int i = 0;

	for (i = 0; i < MAX_OUT_ELEM_NUM; i++) {
		struct out_elem *pout = &out_elem_table[i];

		if (pout->used &&
		    pout->sid == sid &&
		    pout->format == SECTION_FORMAT &&
			pout->es_pes->pid == pid) {
			return pout;
		}
	}
	return NULL;
}

struct out_elem *ts_output_find_dvr(int sid)
{
	int i = 0;

	for (i = 0; i < MAX_OUT_ELEM_NUM; i++) {
		struct out_elem *pout = &out_elem_table[i];

		if (pout->used &&
		    pout->sid == sid && pout->format == DVR_FORMAT) {
			return pout;
		}
	}
	return NULL;
}

/**
 * open one output pipeline
 * \param dmx_id:demux id.
 * \param sid:stream id.
 * \param format:output format.
 * \param type:input content type.
 * \param media_type:aucpu support format
 * \param output_mode:1 will output raw mode,just for ES.
 * \retval return out_elem.
 * \retval NULL:fail.
 */
struct out_elem *ts_output_open(int sid, u8 dmx_id, u8 format,
				enum content_type type, int media_type,
				int output_mode)
{
	struct bufferid_attr attr;
	int ret = 0;
	struct out_elem *pout;
	struct ts_out *ts_out_tmp = NULL;

	pr_dbg("%s sid:%d, format:%d, type:%d ", __func__, sid, format, type);
	pr_dbg("media_type:%d, output_mode:%d\n", media_type, output_mode);

	if (sid >= MAX_SID_NUM) {
		dprint("%s sid:%d fail\n", __func__, sid);
		return NULL;
	}
	pout = _find_free_elem();
	if (!pout) {
		dprint("%s find free elem sid:%d fail\n", __func__, sid);
		return NULL;
	}
	pout->dmx_id = dmx_id;
	pout->format = format;
	pout->sid = sid;
	pout->pid_list = NULL;
	pout->output_mode = output_mode;
	pout->type = type;
	pout->media_type = media_type;
	pout->ref = 0;
	pout->newest_pts = 0;
	pout->decoder_rp_offset = INVALID_DECODE_RP;
	memset(&attr, 0, sizeof(struct bufferid_attr));
	attr.mode = OUTPUT_MODE;

	if (format == ES_FORMAT) {
		attr.is_es = 1;
		ret = SC2_bufferid_alloc(&attr, &pout->pchan, &pout->pchan1);
		if (ret != 0) {
			dprint("%s sid:%d SC2_bufferid_alloc fail\n",
			       __func__, sid);
			return NULL;
		}
		pout->enable = 0;
		pout->aucpu_handle = -1;
		pout->aucpu_start = 0;
		pout->aucpu_pts_handle = -1;
		pout->aucpu_pts_start = 0;
	} else if (format == CLONE_FORMAT) {
		ret = SC2_bufferid_alloc(&attr, &pout->pchan, NULL);
		if (ret != 0) {
			dprint("%s sid:%d SC2_bufferid_alloc fail\n",
				   __func__, sid);
			return NULL;
		}
		pout->enable = 0;
		pout->aucpu_handle = -1;
		pout->aucpu_start = 0;
		pout->aucpu_pts_handle = -1;
		pout->aucpu_pts_start = 0;
		pout->running = TASK_RUNNING;
		pout->used = 1;
		pr_dbg("%s ts clone success\n", __func__);
		return pout;
	} else {
		if (get_dmx_version() >= 5 && format == PES_FORMAT) {
			attr.is_es = 1;
			ret = SC2_bufferid_alloc(&attr, &pout->pchan, &pout->pchan1);
		} else {
			ret = SC2_bufferid_alloc(&attr, &pout->pchan, NULL);
		}
		if (ret != 0) {
			dprint("%s sid:%d SC2_bufferid_alloc fail\n",
			       __func__, sid);
			return NULL;
		}
		pout->aucpu_handle = -1;
		pout->aucpu_start = 0;
		pout->aucpu_pts_handle = -1;
		pout->aucpu_pts_start = 0;
		pout->enable = 0;
	}

	ts_out_tmp = kmalloc(sizeof(*ts_out_tmp), GFP_KERNEL);
	if (!ts_out_tmp) {
		dprint("ts out list fail\n");
		SC2_bufferid_dealloc(pout->pchan);
		return NULL;
	}

	if (format == ES_FORMAT) {
		ts_out_tmp->es_params =
			kmalloc(sizeof(struct es_params_t), GFP_KERNEL);
		if (!ts_out_tmp->es_params) {
			dprint("ts out es_params fail\n");
			SC2_bufferid_dealloc(pout->pchan);
			kfree(ts_out_tmp);
			return NULL;
		}
		memset(ts_out_tmp->es_params, 0,
				sizeof(struct es_params_t));
		ts_out_tmp->es_params->last_header[0] = 0xff;
		ts_out_tmp->es_params->last_header[1] = 0xff;
		mutex_init(&pout->pts_mutex);
	} else {
		ts_out_tmp->es_params = NULL;
	}
	ts_out_tmp->pout = pout;
	ts_out_tmp->pnext = NULL;

	if (format == ES_FORMAT) {
		mutex_lock(&es_output_mutex);
		add_ts_out_list(&es_out_task_tmp, ts_out_tmp);
		mutex_unlock(&es_output_mutex);
	} else {
		add_ts_out_list(&ts_out_task_tmp, ts_out_tmp);
	}
	pout->running = TASK_RUNNING;
	pout->used = 1;
	pr_dbg("%s exit\n", __func__);

	return pout;
}

/**
 * close openned index
 * \param pout
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_close(struct out_elem *pout)
{
	if (pout->ref)
		return -1;

	pr_dbg("%s enter, line:%d\n", __func__, __LINE__);

	pout->running = TASK_DEAD;

	if (pout->format == ES_FORMAT) {
		if (pout->dump_file.file_fp)
			dump_file_close(&pout->dump_file);

		mutex_lock(&es_output_mutex);
		remove_ts_out_list(pout, &es_out_task_tmp);
		mutex_unlock(&es_output_mutex);
		mutex_destroy(&pout->pts_mutex);
	} else if (pout->format != CLONE_FORMAT) {
		if (pout->format == DVR_FORMAT && pout->dump_file.file_fp)
			dump_file_close(&pout->dump_file);
		remove_ts_out_list(pout, &ts_out_task_tmp);
	}

	if (pout->aucpu_handle >= 0) {
		s32 ret;

		if (pout->aucpu_start) {
			ret = aml_aucpu_strm_stop(pout->aucpu_handle);
			if (ret >= 0)
				pr_dbg("aml_aucpu_strm_stop success\n");
			else
				pr_dbg("aucpu_stop fail ret:%d\n", ret);
			pout->aucpu_start = 0;
		}
		ret = aml_aucpu_strm_remove(pout->aucpu_handle);
		if (ret >= 0)
			pr_dbg("aucpu_strm_remove success\n");
		else
			pr_dbg("aucpu_strm_remove fail ret:%d\n", ret);
		pout->aucpu_handle = -1;

		_free_buff(pout->aucpu_mem_phy,
				pout->aucpu_mem_size, 0);
		pout->aucpu_mem = 0;
	}

	if (pout->aucpu_pts_handle >= 0) {
		s32 ret;

		if (pout->aucpu_pts_start) {
			ret = aml_aucpu_strm_stop(pout->aucpu_pts_handle);
			if (ret >= 0)
				pr_dbg("aml_aucpu_strm_stop pts success\n");
			else
				pr_dbg("aucpu_stop fail ret:%d\n", ret);
			pout->aucpu_pts_start = 0;
		}
		ret = aml_aucpu_strm_remove(pout->aucpu_pts_handle);
		if (ret >= 0)
			pr_dbg("aucpu_strm_remove pts success\n");
		else
			pr_dbg("aucpu_strm_remove pts fail ret:%d\n", ret);
		pout->aucpu_pts_handle = -1;

		_free_buff(pout->aucpu_pts_mem_phy,
				pout->aucpu_pts_mem_size, 0);
		pout->aucpu_pts_mem = 0;
	}

	if (pout->pchan) {
		SC2_bufferid_set_enable(pout->pchan, 0, pout->sid, 0x1fff);
		SC2_bufferid_dealloc(pout->pchan);
		pout->pchan = NULL;
	}
	if (pout->pchan1) {
		SC2_bufferid_set_enable(pout->pchan1, 0, pout->sid, 0x1fff);
		SC2_bufferid_dealloc(pout->pchan1);
		pout->pchan1 = NULL;
	}
	pout->use_external_mem = 0;
	pout->ts_dump = 0;
	pout->used = 0;
	if (pout->format == TEMI_FORMAT) {
		pcr_table[pout->temi_index].pout = NULL;
		pout->temi_index = -1;
	}

	pr_dbg("%s exit, line:%d\n", __func__, __LINE__);
	return 0;
}

int ts_output_alloc_pcr_temi_entry(int pid, int sid, int type)
{
	int index = 0;

	for (index = 0; index < MAX_PCR_NUM; index++)
		if (pcr_table[index].turn_on == 0)
			break;

	if (index == MAX_PCR_NUM)
		return index;

	pcr_table[index].turn_on = 1;
	pcr_table[index].type = type;

	switch (type) {
	case PCR_TYPE:
		pcr_table[index].pcr_total = 1;
		pcr_table[index].ref = 1;
	break;
	case TEMI_TYPE:
		pcr_table[index].temi_total = 1;
		pcr_table[index].ref = 1;
	break;
	case PCR_TEMI_TYPE:
		pcr_table[index].pcr_total = 1;
		pcr_table[index].temi_total = 1;
		pcr_table[index].ref = 2;
	break;
	default:
		dprint("%s type error type=%d\n", __func__, type);
	break;
	}

	return 0;
}

int ts_output_free_pcr_temi_entry(int index, int type)
{
	if (index < 0 || index >= MAX_PCR_NUM)
		return -1;

	pcr_table[index].ref -= 1;
	if (type == PCR_TYPE)
		pcr_table[index].pcr_total -= 1;
	else if (type == TEMI_TYPE)
		pcr_table[index].temi_total -= 1;

	if (pcr_table[index].ref <= 0) {
		pcr_table[index].turn_on = 0;
		pcr_table[index].type = 0;
	}

	return 0;
}

static int ts_output_set_temi(int index, int pid, int sid, void *pout)
{
	struct out_elem *p = NULL;

	if (index < 0 || index >= MAX_PCR_NUM)
		return -1;

	p = pout;

	if (pid == -1) {
		if (pcr_table[index].ref <= 0) {
			pcr_table[index].pcr_pid = -1;
			pcr_table[index].stream_id = -1;
			pcr_table[index].sid = -1;
			pcr_table[index].pout = NULL;
			tsout_config_temi_table(index, -1, -1, -1, -1);
		}
	} else {
		pcr_table[index].pcr_pid = pid;
		pcr_table[index].stream_id = sid;
		pcr_table[index].sid = sid;
		pcr_table[index].pout = pout;
		tsout_config_temi_table(index, pid, sid, p->pchan->id, 0);
	}

	return 0;
}

void *ts_output_find_temi_pcr(int sid, int pid, int *pcr_index, int *temi_index)
{
	int index = 0;

	for (index = 0; index < MAX_PCR_NUM; index++) {
		if (pcr_table[index].turn_on == 1 && pid == pcr_table[index].pcr_pid &&
			sid == pcr_table[index].sid) {
			if (pcr_index) {
				pcr_table[index].ref += 1;
				pcr_table[index].pcr_total += 1;
				if (pcr_table[index].type == TEMI_TYPE)
					pcr_table[index].type = PCR_TEMI_TYPE;
				*pcr_index = index;
			}

			if (temi_index) {
				pcr_table[index].ref += 1;
				pcr_table[index].temi_total += 1;
				if (pcr_table[index].type == PCR_TYPE)
					pcr_table[index].type = PCR_TEMI_TYPE;
				*temi_index = index;
			}

			if (pcr_index && temi_index)
				pcr_table[index].type = PCR_TEMI_TYPE;

			return pcr_table[index].pout;
		}
	}

	if (pcr_index)
		*pcr_index = index;

	if (temi_index)
		*temi_index = index;

	return NULL;
}

int ts_output_add_temi_pid(struct out_elem *pout, int pid, int dmx_id,
						int *cb_id, int *index, int type)
{
	int ret = 0;

	if (!pout)
		return -1;

	if (*index >= MAX_PCR_NUM) {
		*index = ts_output_alloc_pcr_temi_entry(pid, pout->sid, type);
		if (*index >= MAX_PCR_NUM)
			return -1;
	}

	if (pout->pchan)
		SC2_bufferid_set_enable(pout->pchan, 1, pout->sid, pid);

	if (cb_id)
		*cb_id = dmx_id;

	ret = ts_output_set_temi(*index, pid, pout->sid, pout);
	if (ret != 0) {
		dprint("set temi status failed\n");
		ts_output_free_pcr_temi_entry(*index, TEMI_TYPE);
		return -1;
	}

	pout->enable = 1;
	pout->temi_index = *index;

	return 0;
}

int ts_output_add_remove_temi_pid(struct out_elem *pout, int index)
{
	int ret = 0;

	ret = ts_output_set_temi(index, -1, -1, NULL);

	ts_output_free_pcr_temi_entry(index, TEMI_TYPE);

	if (pout->ref <= 1)
		pout->enable = 0;

	return ret;
}

/**
 * add pid in stream
 * \param pout
 * \param pid:
 * \param pid_mask:0,matched all bits; 0x1FFF matched any PID
 * \param dmx_id: dmx_id
 * \param cb_id:same pid ref
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_add_pid(struct out_elem *pout, int pid, int pid_mask, int dmx_id,
		      int *cb_id)
{
	struct pid_entry *pid_slot = NULL;
	struct es_entry *es_pes = NULL;
	int ret = 0;

	if (!pout)
		return -1;

	if (cb_id)
		*cb_id = 0;

	if (pout->pchan)
		SC2_bufferid_set_enable(pout->pchan, 1, pout->sid, pid);
	if (pout->pchan1)
		SC2_bufferid_set_enable(pout->pchan1, 1, pout->sid, pid);

	pr_dbg("%s pout:0x%lx pid:%d, pid_mask:%d\n",
	       __func__, (unsigned long)pout, pid, pid_mask);
	if (pout->format == ES_FORMAT ||
	    pout->format == PES_FORMAT || pout->format == SECTION_FORMAT) {
		es_pes = _malloc_es_entry_slot();
		if (!es_pes) {
			dprint("get es entry slot error\n");
			return -1;
		}
		if (!pout->pchan) {
			dprint("get pout->pchan error\n");
			_free_es_entry_slot(es_pes);
			return -1;
		}
		es_pes->buff_id = pout->pchan->id;
		es_pes->pid = pid;
		es_pes->status = pout->format;
		es_pes->dmx_id = dmx_id;
		es_pes->pout = pout;
		pout->es_pes = es_pes;

		/*before pid filter enable */
		if (pout->pchan->sec_level || (pout->pchan1 && pout->pchan1->sec_level)) {
			ret = create_aucpu_inst(pout);
			if (ret != 0) {
				_free_es_entry_slot(es_pes);
				return -1;
			}
		}
		if (pout->type == VIDEO_TYPE) {
			if (((dump_video_es & 0xFFFF) == pout->es_pes->pid &&
				((dump_video_es >> 16) & 0xFFFF) == pout->sid) ||
				dump_video_es == 0XFFFFFFFF)
				dump_file_open(VIDEOES_DUMP_FILE,
						 &pout->dump_file,	pout->sid,
						 pout->es_pes->pid,	0);
		}
		if (pout->type == AUDIO_TYPE) {
			if (((dump_audio_es & 0xFFFF) == pout->es_pes->pid &&
				((dump_audio_es >> 16) & 0xFFFF) ==	pout->sid) ||
			   dump_audio_es == 0xFFFFFFFF)
				dump_file_open(AUDIOES_DUMP_FILE,
						&pout->dump_file, pout->sid,
						pout->es_pes->pid, 0);
		}
		tsout_config_es_table(es_pes->buff_id, es_pes->pid,
				      pout->sid, 1, !drop_dup, pout->format);
		switch (pout->format) {
		case ES_FORMAT:
		case PES_FORMAT:
			snprintf(pout->name, sizeof(pout->name),
				"demux_%s_%s_%x_%x",
				pout->type == AUDIO_TYPE ? "audio" : "video",
				pout->format == ES_FORMAT ? "es" : "pes",
				pout->sid,
				pout->es_pes->pid
			);
			ATRACE_COUNTER(pout->name, 0);
			break;
		case SECTION_FORMAT:
			snprintf(pout->name, sizeof(pout->name),
				"demux_section_%x_%x",
				pout->sid,
				pout->es_pes->pid
			);
			ATRACE_COUNTER(pout->name, 0);
			break;
		default:
			break;
		}
	} else if (pout->format == CLONE_FORMAT) {
		pid_slot = _malloc_pid_entry_slot(pout->sid, 0x1fff);
		if (!pid_slot) {
			pr_dbg("malloc pid entry fail\n");
			return -1;
		}
		if (!pout->pchan) {
			dprint("get pout->pchan NULL error\n");
			_free_pid_entry_slot(pid_slot);
			return -1;
		}

		pid_slot->pid = 0x1fff;
		pid_slot->pid_mask = 0;
		pid_slot->used = 1;
		pid_slot->dmx_id = dmx_id;
		pid_slot->ref = 1;
		pid_slot->pout = pout;

		pid_slot->pnext = pout->pid_list;
		pout->pid_list = pid_slot;
		pr_dbg("sid:%d, pid:0x%0x, mask:0x%0x\n",
				pout->sid, pid_slot->pid, pid_slot->pid_mask);
		tsout_config_ts_table(pid_slot->pid, pid_slot->pid_mask,
				pid_slot->id, pout->pchan->id, pout->sid, 0);

		pid_slot = _malloc_pid_entry_slot(pout->sid, 0x1ffe);
		if (!pid_slot) {
			pr_dbg("malloc pid entry fail\n");
			return -1;
		}
		pid_slot->pid = 0x1ffe;
		pid_slot->pid_mask = 0x1fff;
		pid_slot->used = 1;
		pid_slot->dmx_id = dmx_id;
		pid_slot->ref = 1;
		pid_slot->pout = pout;

		pid_slot->pnext = pout->pid_list;
		pout->pid_list = pid_slot;
		pr_dbg("sid:%d, pid:0x%0x, mask:0x%0x\n",
				pout->sid, pid_slot->pid, pid_slot->pid_mask);
		tsout_config_ts_table(pid_slot->pid, pid_slot->pid_mask,
				pid_slot->id, pout->pchan->id, pout->sid, 0);
	} else  {
		if (pid == 0x1fff && pid_mask == 0x1fff)
			pout->ts_dump = 1;
		if (cb_id)
			*cb_id = dmx_id;
		pid_slot = pout->pid_list;
		while (pid_slot) {
			if (pid_slot->pid == pid) {
				pid_slot->ref++;
				if (cb_id)
					*cb_id = dmx_id;
				if (pout->enable == 0)
					pr_err("!!!!!pout enable is 0, this is danger error!!!!!!\n");
				return 0;
			}
			pid_slot = pid_slot->pnext;
		}

		pid_slot = _malloc_pid_entry_slot(pout->sid, pid);
		if (!pid_slot) {
			pr_dbg("malloc pid entry fail\n");
			return -1;
		}
		if (!pout->pchan) {
			dprint("get pout->pchan NULL error\n");
			_free_pid_entry_slot(pid_slot);
			return -1;
		}
		pid_slot->pid = pid;
		pid_slot->pid_mask = pid_mask;
		pid_slot->used = 1;
		pid_slot->dmx_id = dmx_id;
		pid_slot->ref = 1;
		pid_slot->pout = pout;

		pid_slot->pnext = pout->pid_list;
		pout->pid_list = pid_slot;
		pr_dbg("sid:%d, pid:0x%0x, mask:0x%0x\n",
				pout->sid, pid_slot->pid, pid_slot->pid_mask);
		tsout_config_ts_table(pid_slot->pid, pid_slot->pid_mask,
				pid_slot->id, pout->pchan->id, pout->sid, pout->pchan->sec_level);
	}
	pout->enable = 1;
	return 0;
}

/**
 * remove pid in stream
 * \param pout
 * \param pid
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_remove_pid(struct out_elem *pout, int pid)
{
	struct pid_entry *cur_pid;
	struct pid_entry *prev_pid;

	pr_dbg("%s pout:0x%lx\n", __func__, (unsigned long)pout);
	if (pout->format == ES_FORMAT ||
		pout->format == PES_FORMAT || pout->format == SECTION_FORMAT) {
		if (pout->format != pout->es_pes->status) {
			pr_err("rm pid error pout fmt:%d es_pes fmt:%d\r\n",
			pout->format,
			pout->es_pes->status);
			return 0;
		} else if (pout->ref <= 1) {
			tsout_config_es_table(pout->es_pes->buff_id, -1,
					pout->sid, 1, !drop_dup, pout->format);
			_free_es_entry_slot(pout->es_pes);
//          pout->es_pes = NULL;
		} else {
			return 0;
		}
	} else if (pout->format == DVR_FORMAT) {
		cur_pid = pout->pid_list;
		prev_pid = cur_pid;
		while (cur_pid) {
			if (cur_pid->pid == pid) {
				if (cur_pid->ref > 1) {
					cur_pid->ref--;
					return 0;
				} else if (cur_pid->ref == 1) {
					if (cur_pid == pout->pid_list)
						pout->pid_list = cur_pid->pnext;
					else
						prev_pid->pnext =
						    cur_pid->pnext;
					break;
				}
			}
			prev_pid = cur_pid;
			cur_pid = cur_pid->pnext;
		}
		if (cur_pid) {
			tsout_config_ts_table(-1, cur_pid->pid_mask,
					      cur_pid->id, pout->pchan->id,
					      pout->sid, pout->pchan->sec_level);
			_free_pid_entry_slot(cur_pid);
		}
		if (pout->pid_list)
			return 0;
	} else if (pout->format == CLONE_FORMAT) {
		cur_pid = pout->pid_list;
		while (cur_pid) {
			if (cur_pid) {
				tsout_config_ts_table(-1, cur_pid->pid_mask,
							  cur_pid->id, pout->pchan->id,
							  pout->sid, pout->pchan->sec_level);
				_free_pid_entry_slot(cur_pid);
			}
			cur_pid = cur_pid->pnext;
		}
	}
	pout->enable = 0;
	pr_dbg("%s line:%d\n", __func__, __LINE__);
	return 0;
}

int ts_output_set_mem(struct out_elem *pout, int memsize,
	int sec_level, int pts_memsize, int pts_level)
{
	int ret = 0;
	pr_dbg("%s mem size:0x%0x, pts_memsize:0x%0x, sec_level:%d\n",
		__func__, memsize, pts_memsize, sec_level);

	if (pout && pout->pchan) {
		ret = SC2_bufferid_set_mem(pout->pchan, memsize, sec_level);
		if (ret != 0)
			return -1;
	}
	if (pout && pout->pchan1) {
		ret = SC2_bufferid_set_mem(pout->pchan1,
				pts_memsize, pts_level);
		if (ret != 0)
			return -1;
	}
	return 0;
}

int ts_output_set_sec_mem(struct out_elem *pout,
	unsigned int buf, unsigned int size)
{
	pr_dbg("%s size:0x%0x\n", __func__, size);

	if (pout && pout->pchan) {
		SC2_bufferid_set_sec_mem(pout->pchan, buf, size);
		pout->use_external_mem = 1;
	}
	return 0;
}

int ts_output_get_newest_pts(struct out_elem *pout,
			   __u64 *newest_pts)
{
	char *pts_dts = NULL;
	int ret = 0;
	int pid = 0;
	char newest_header[16];
	__u64 newest_pts_tmp = 0;

	*newest_pts = pout->newest_pts;
	memset(&newest_header, 0, sizeof(newest_header));
	if (pout->type == VIDEO_TYPE || pout->type == AUDIO_TYPE) {
		mutex_lock(&pout->pts_mutex);
		if (!pout->aucpu_pts_start &&
			 pout->aucpu_pts_handle >= 0) {
			if (wdma_get_active(pout->pchan1->id)) {
				ret = aml_aucpu_strm_start(pout->aucpu_pts_handle);
				if (ret >= 0) {
					pr_dbg("aucpu pts start success\n");
					pout->aucpu_pts_start = 1;
				} else {
					pr_dbg("aucpu start fail ret:%d\n",
						   ret);
					mutex_unlock(&pout->pts_mutex);
					return -1;
				}
			} else {
				mutex_unlock(&pout->pts_mutex);
				return -1;
			}
		}

		if (pout->aucpu_pts_start)
			ret = aucpu_bufferid_read_newest_pts(pout, &pts_dts);
		else
			ret = SC2_bufferid_read_newest_pts(pout->pchan1, &pts_dts);
		if (ret != 0) {
			memcpy((char *)newest_header, pts_dts, 16);
		} else {
			mutex_unlock(&pout->pts_mutex);
			return -3;
		}

		pid = (newest_header[1] & 0x1f) << 8 | newest_header[0];
		if (pout->es_pes->pid != pid) {
			dprint("%s pid diff req pid %d, ret pid:%d\n",
				   __func__, pout->es_pes->pid, pid);
			mutex_unlock(&pout->pts_mutex);
			return -2;
		}
		if (newest_header[2] & 0xc) {
			dprint("%s scrambled es, invalid\n", __func__);
			mutex_unlock(&pout->pts_mutex);
			return -2;
		}
		newest_pts_tmp = newest_header[3] >> 1 & 0x1;
		newest_pts_tmp <<= 32;
		newest_pts_tmp |= ((__u64)newest_header[15]) << 24
			| ((__u64)newest_header[14]) << 16
			| ((__u64)newest_header[13]) << 8
			| ((__u64)newest_header[12]);

		newest_pts_tmp &= 0x1FFFFFFFF;

		if (newest_header[2] & 0x2) {
			pout->newest_pts = newest_pts_tmp;
			*newest_pts = newest_pts_tmp;
			pr_dbg("%s pts:0x%lx\n", __func__, (unsigned long)newest_pts_tmp);
		}

		mutex_unlock(&pout->pts_mutex);
	}
	return 0;
}

int ts_output_get_mem_info(struct out_elem *pout,
			   unsigned int *total_size,
			   unsigned int *buf_phy_start,
			   unsigned int *free_size, unsigned int *wp_offset,
			   __u64 *newest_pts)
{
	*total_size = pout->pchan->mem_size;
	*buf_phy_start = pout->pchan->mem_phy;
	*wp_offset = SC2_bufferid_get_wp_offset(pout->pchan);
	if (pout->aucpu_start) {
		unsigned int now_w = 0;
		unsigned int mem_size = pout->aucpu_mem_size;

		now_w = SC2_bufferid_get_wp_offset(pout->pchan);
		if (now_w >= pout->aucpu_read_offset)
			*free_size = mem_size - (now_w - pout->aucpu_read_offset);
		else
			*free_size = pout->aucpu_read_offset - now_w;
	} else {
		if (pout->decoder_rp_offset == INVALID_DECODE_RP) {
			*free_size = SC2_bufferid_get_free_size(pout->pchan);
		} else {
			unsigned int w = 0;
			unsigned int total = 0;

//			pr_dbg("decoder rp:0x%0x\n", pout->decoder_rp_offset);
			w = SC2_bufferid_get_wp_offset(pout->pchan);
			total = pout->pchan->mem_size;
			if (w > pout->decoder_rp_offset)
				*free_size = total - w + pout->decoder_rp_offset;
			else
				*free_size = pout->decoder_rp_offset - w;
		}
	}
	if (newest_pts && pout->format == ES_FORMAT)
		ts_output_get_newest_pts(pout, newest_pts);
	return 0;
}

/**
 * reset index pipeline, clear the buf
 * \param pout
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_reset(struct out_elem *pout)
{
	return 0;
}

/**
 * set callback for getting data
 * \param pout
 * \param cb
 * \param udata:private data
 * \param cb_id:cb_id
 * \param format:format
 * \param is_sec: is section callback
 * \param demux_id:dmx id
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_add_cb(struct out_elem *pout, ts_output_cb cb, void *udata,
		     u8 cb_id, u8 format, bool is_sec, u8 demux_id)
{
	struct cb_entry *tmp_cb = NULL;

	if (format == DVR_FORMAT) {
		tmp_cb = pout->cb_ts_list;
		while (tmp_cb) {
			if (tmp_cb->id == cb_id &&
				tmp_cb->format == DVR_FORMAT) {
				tmp_cb->ref++;
				if (tmp_cb->ref < MAX_FEED_NUM)
					tmp_cb->udata[tmp_cb->ref] = udata;
				return 0;
			}
			tmp_cb = tmp_cb->next;
		}
	}

	tmp_cb = vmalloc(sizeof(*tmp_cb));
	if (!tmp_cb) {
		dprint("%s malloc fail\n", __func__);
		return -1;
	}
	tmp_cb->cb = cb;
	tmp_cb->udata[0] = udata;
	tmp_cb->next = NULL;
	tmp_cb->format = format;
	tmp_cb->ref = 0;
	tmp_cb->id = cb_id;
	tmp_cb->demux_id = demux_id;

	if (is_sec) {
		tmp_cb->next = pout->cb_sec_list;
		pout->cb_sec_list = tmp_cb;
	} else {
		if (pout->type == VIDEO_TYPE || pout->type == AUDIO_TYPE)
			mutex_lock(&es_output_mutex);

		tmp_cb->next = pout->cb_ts_list;
		pout->cb_ts_list = tmp_cb;

		if (pout->type == VIDEO_TYPE || pout->type == AUDIO_TYPE)
			mutex_unlock(&es_output_mutex);
	}

	pout->ref++;

	if ((pout->type == VIDEO_TYPE || pout->type == AUDIO_TYPE) && ES_FORMAT == format)
		mod_timer(&es_out_task_tmp.out_timer,
		  jiffies + msecs_to_jiffies(out_es_flush_time));
	else
		mod_timer(&ts_out_task_tmp.out_timer,
		  jiffies + msecs_to_jiffies(out_flush_time));
	return 0;
}

static void remove_udata(struct cb_entry *tmp_cb, void *udata)
{
	int i, j;

	/*remove the free feed*/
	for (i = 0; i <= tmp_cb->ref && i < MAX_FEED_NUM; i++) {
		if (tmp_cb->udata[i] == udata) {
			for (j = i; j < tmp_cb->ref && j < (MAX_FEED_NUM - 1); j++)
				tmp_cb->udata[j] = tmp_cb->udata[j + 1];
		}
	}
}

/**
 * remove callback for getting data
 * \param pout
 * \param cb
 * \param udata:private data
 * \param cb_id:dmx_id
 * \param is_sec: is section callback
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_remove_cb(struct out_elem *pout, ts_output_cb cb, void *udata,
			u8 cb_id, bool is_sec)
{
	struct cb_entry *tmp_cb = NULL;
	struct cb_entry *pre_cb = NULL;

	pr_dbg("%s enter\n", __func__);

	if (pout->format == DVR_FORMAT) {
		tmp_cb = pout->cb_ts_list;
		while (tmp_cb) {
			if (tmp_cb->id == cb_id &&
				tmp_cb->format == DVR_FORMAT) {
				if (tmp_cb->ref == 0) {
					if (tmp_cb == pout->cb_ts_list)
						pout->cb_ts_list = tmp_cb->next;
					else
						pre_cb->next = tmp_cb->next;
					vfree(tmp_cb);
					pout->ref--;
					pr_dbg("%s exit, line:%d\n", __func__, __LINE__);
					return 0;
				}
				remove_udata(tmp_cb, udata);
				tmp_cb->ref--;
				pr_dbg("%s exit, line:%d\n", __func__, __LINE__);
				return 0;
			}
			pre_cb = tmp_cb;
			tmp_cb = tmp_cb->next;
		}
		pr_dbg("%s exit, line:%d\n", __func__, __LINE__);
		return 0;
	}
	if (is_sec) {
		tmp_cb = pout->cb_sec_list;
		while (tmp_cb) {
			if (tmp_cb->cb == cb && tmp_cb->udata[0] == udata) {
				if (tmp_cb == pout->cb_sec_list)
					pout->cb_sec_list = tmp_cb->next;
				else
					pre_cb->next = tmp_cb->next;

				vfree(tmp_cb);
				pout->ref--;
				pr_dbg("%s exit, line:%d\n", __func__, __LINE__);
				return 0;
			}
			pre_cb = tmp_cb;
			tmp_cb = tmp_cb->next;
		}
	} else {
		if (pout->type == VIDEO_TYPE || pout->type == AUDIO_TYPE)
			mutex_lock(&es_output_mutex);

		tmp_cb = pout->cb_ts_list;
		while (tmp_cb) {
			if (tmp_cb->cb == cb && tmp_cb->udata[0] == udata) {
				if (tmp_cb == pout->cb_ts_list)
					pout->cb_ts_list = tmp_cb->next;
				else
					pre_cb->next = tmp_cb->next;

				vfree(tmp_cb);
				pout->ref--;
				if (pout->type == VIDEO_TYPE ||
				    pout->type == AUDIO_TYPE)
					mutex_unlock(&es_output_mutex);
				pr_dbg("%s exit, line:%d\n", __func__, __LINE__);
				return 0;
			}
			pre_cb = tmp_cb;
			tmp_cb = tmp_cb->next;
		}
		if (pout->type == VIDEO_TYPE || pout->type == AUDIO_TYPE)
			mutex_unlock(&es_output_mutex);
	}
	pr_dbg("%s exit, line:%d\n", __func__, __LINE__);
	return 0;
}

int ts_output_dump_info(char *buf)
{
	int i = 0;
	int count = 0;
	int r, total = 0;

	r = sprintf(buf, "********dvr********\n");
	buf += r;
	total += r;

	for (i = 0; i < MAX_OUT_ELEM_NUM; i++) {
		struct out_elem *pout = &out_elem_table[i];
		unsigned int total_size = 0;
		unsigned int buf_phy_start = 0;
		unsigned int free_size = 0;
		unsigned int wp_offset = 0;
		struct pid_entry *pid_list;
		struct cb_entry *tmp_cb = NULL;

		if (pout->used && pout->format == DVR_FORMAT) {
			tmp_cb = pout->cb_ts_list;

			r = sprintf(buf, "%d sid:0x%0x ref:%d ",
				    count, pout->sid, pout->ref);
			buf += r;
			total += r;

			r = sprintf(buf, "dmx_id ");
			buf += r;
			total += r;

			while (tmp_cb) {
				r = sprintf(buf, "%d ", tmp_cb->demux_id);
				buf += r;
				total += r;

				tmp_cb = tmp_cb->next;
			}

			ts_output_get_mem_info(pout,
					       &total_size,
					       &buf_phy_start,
					       &free_size, &wp_offset, NULL);
			r = sprintf(buf,
				    "mem total:0x%0x, buf_base:0x%0x, ",
				    total_size, buf_phy_start);
			buf += r;
			total += r;

			r = sprintf(buf,
				    "free size:0x%0x, rp:0x%0x, wp:0x%0x, ",
				    free_size, pout->pchan->r_offset,
					wp_offset);
			buf += r;
			total += r;

			if (pout->use_external_mem == 1)
				r = sprintf(buf, "mem mode:secure\n");
			else
				r = sprintf(buf, "mem mode:normal\n");
			buf += r;
			total += r;

			pid_list = pout->pid_list;
			r = sprintf(buf, "    pid:");
			buf += r;
			total += r;

			while (pid_list) {
				r = sprintf(buf, "0x%0x ", pid_list->pid);
				buf += r;
				total += r;
				pid_list = pid_list->pnext;
			}
			r = sprintf(buf, "\n");
			buf += r;
			total += r;

			count++;
		}
	}

	r = sprintf(buf, "********PES********\n");
	buf += r;
	total += r;

	count = 0;
	for (i = 0; i < MAX_ES_NUM; i++) {
		struct es_entry *es_slot = &es_table[i];
		unsigned int total_size = 0;
		unsigned int buf_phy_start = 0;
		unsigned int free_size = 0;
		unsigned int wp_offset = 0;

		if (es_slot->used && es_slot->status == PES_FORMAT) {
			if (!es_slot->pout) {
				dprint("%s line:%d\n", __func__, __LINE__);
				continue;
			}
			r = sprintf(buf, "%d dmx_id:%d sid:0x%0x type:%d,",
				count, es_slot->dmx_id,
				es_slot->pout->sid, es_slot->pout->type);
			buf += r;
			total += r;

			r = sprintf(buf, "pid:0x%0x ", es_slot->pid);
			buf += r;
			total += r;

			ts_output_get_mem_info(es_slot->pout,
					       &total_size,
					       &buf_phy_start,
					       &free_size, &wp_offset, NULL);
			r = sprintf(buf,
				    "mem total:0x%0x, buf_base:0x%0x, ",
				    total_size, buf_phy_start);
			buf += r;
			total += r;

			if (es_slot->pout->aucpu_start)
				r = sprintf(buf,
				    "free size:0x%0x, rp:0x%0x, wp:0x%0x, ",
				    free_size, es_slot->pout->aucpu_read_offset,
					wp_offset);
			else
				r = sprintf(buf,
					"free size:0x%0x, rp:0x%0x, wp:0x%0x, ",
					free_size, es_slot->pout->pchan->r_offset,
					wp_offset);
			buf += r;
			total += r;

			if (aml_aucpu_strm_get_load_firmware_status() != 0) {
				r = sprintf(buf, "aucpu:no load\n");
			} else {
				if (es_slot->pout->aucpu_start)
					r = sprintf(buf, "aucpu:using\n");
				else
					r = sprintf(buf, "aucpu:no\n");
			}

			buf += r;
			total += r;

			count++;
		}
	}

	r = sprintf(buf, "********ES********\n");
	buf += r;
	total += r;
	count = 0;

	for (i = 0; i < MAX_ES_NUM; i++) {
		struct es_entry *es_slot = &es_table[i];
		unsigned int total_size = 0;
		unsigned int buf_phy_start = 0;
		unsigned int free_size = 0;
		unsigned int wp_offset = 0;

		if (es_slot->used && es_slot->status == ES_FORMAT) {
			if (!es_slot->pout) {
				dprint("%s line:%d\n", __func__, __LINE__);
				continue;
			}
			r = sprintf(buf, "%d dmx_id:%d sid:0x%0x type:%s",
				count, es_slot->dmx_id, es_slot->pout->sid,
				    (es_slot->pout->type == AUDIO_TYPE) ?
				    "aud" : "vid");
			buf += r;
			total += r;

			r = sprintf(buf, " pid:0x%0x ", es_slot->pid);
			buf += r;
			total += r;

			ts_output_get_mem_info(es_slot->pout,
					       &total_size,
					       &buf_phy_start,
					       &free_size, &wp_offset, NULL);

			r = sprintf(buf,
				    "mem total:0x%0x, buf_base:0x%0x, ",
				    total_size, buf_phy_start);
			buf += r;
			total += r;

			if (es_slot->pout->aucpu_start)
				r = sprintf(buf,
				    "free size:0x%0x, rp:0x%0x, wp:0x%0x, ",
				    free_size, es_slot->pout->aucpu_read_offset,
					wp_offset);
			else
				r = sprintf(buf,
					"free size:0x%0x, rp:0x%0x, wp:0x%0x, ",
					free_size, es_slot->pout->pchan->r_offset,
					wp_offset);
			buf += r;
			total += r;

			if (es_slot->pout->aucpu_pts_start)
				r = sprintf(buf,
				    "h rp:0x%0x, h wp:0x%0x, ",
				    es_slot->pout->aucpu_pts_r_offset,
					SC2_bufferid_get_wp_offset(es_slot->pout->pchan1));
			else
				r = sprintf(buf,
					"h rp:0x%0x, h wp:0x%0x, ",
					es_slot->pout->pchan1->r_offset,
					SC2_bufferid_get_wp_offset(es_slot->pout->pchan1));
			buf += r;
			total += r;

			if (es_slot->pout->aucpu_pts_start)
				r = sprintf(buf, "h mode:aucpu, ");
			else
				r = sprintf(buf, "h mode:normal, ");
			buf += r;
			total += r;

			r = sprintf(buf,
				    "sec_level:0x%0x, ",
				    es_slot->pout->pchan->sec_level);
			buf += r;
			total += r;

			if (aml_aucpu_strm_get_load_firmware_status() != 0) {
				r = sprintf(buf, "aucpu:no load\n");
			} else {
				if (es_slot->pout->aucpu_start)
					r = sprintf(buf, "aucpu:using\n");
				else
					r = sprintf(buf, "aucpu:no\n");
			}
			buf += r;
			total += r;

			count++;
		}
	}
	r = sprintf(buf, "********section********\n");
	buf += r;
	total += r;
	count = 0;

	for (i = 0; i < MAX_ES_NUM; i++) {
		struct es_entry *es_slot = &es_table[i];
		unsigned int total_size = 0;
		unsigned int buf_phy_start = 0;
		unsigned int free_size = 0;
		unsigned int wp_offset = 0;
		struct cb_entry *tmp_cb = NULL;
		struct out_elem *pout = NULL;

		if (es_slot->used && es_slot->status == SECTION_FORMAT) {
			if (!es_slot->pout) {
				dprint("%s line:%d\n", __func__, __LINE__);
				continue;
			}
			pout = es_slot->pout;
			tmp_cb = pout->cb_sec_list;

			r = sprintf(buf, "%d dmx_id:", count);
			buf += r;
			total += r;

			while (tmp_cb) {
				r = sprintf(buf, "%d ", tmp_cb->demux_id);
				buf += r;
				total += r;

				tmp_cb = tmp_cb->next;
			}

			r = sprintf(buf, "sid:0x%0x ", es_slot->pout->sid);
			buf += r;
			total += r;

			r = sprintf(buf, "pid:0x%0x ref:%d ",
				    es_slot->pid, es_slot->pout->ref);
			buf += r;
			total += r;

			ts_output_get_mem_info(es_slot->pout,
					       &total_size,
					       &buf_phy_start,
					       &free_size, &wp_offset, NULL);

			r = sprintf(buf,
				    "mem total:0x%0x, buf_base:0x%0x, ",
				    total_size, buf_phy_start);
			buf += r;
			total += r;

			r = sprintf(buf,
				    "free size:0x%0x, wp:0x%0x, ",
				    free_size, wp_offset);
			buf += r;
			total += r;

			if (aml_aucpu_strm_get_load_firmware_status() != 0) {
				r = sprintf(buf, "aucpu:no load\n");
			} else {
				if (es_slot->pout->aucpu_start)
					r = sprintf(buf, "aucpu:using\n");
				else
					r = sprintf(buf, "aucpu:no\n");
			}
			buf += r;
			total += r;

			count++;
		}
	}

	r = sprintf(buf, "********PCR********\n");
	buf += r;
	total += r;
	count = 0;

	for (i = 0; i < MAX_PCR_NUM; i++) {
		if (pcr_table[i].turn_on == 0 || pcr_table[i].pcr_total <= 0)
			continue;

		r = sprintf(buf, "%d sid:0x%0x pcr pid:0x%0x ref=%d\n", count,
			pcr_table[i].stream_id, pcr_table[i].pcr_pid, pcr_table[i].pcr_total);
		buf += r;
		total += r;

		count++;
	}

	r = sprintf(buf, "********TEMI********\n");
	buf += r;
	total += r;
	count = 0;

	for (i = 0; i < MAX_PCR_NUM; i++) {
		unsigned int total_size = 0;
		unsigned int buf_phy_start = 0;
		unsigned int free_size = 0;
		unsigned int wp_offset = 0;

		if (pcr_table[i].turn_on == 0 || pcr_table[i].temi_total <= 0)
			continue;

		if (!pcr_table[i].pout) {
			dprint("%s line:%d\n", __func__, __LINE__);
			continue;
		}
		r = sprintf(buf, "%d sid:0x%0x temi pid:0x%0x, ref=%d ", count,
			pcr_table[i].stream_id, pcr_table[i].pcr_pid, pcr_table[i].temi_total);

		buf += r;
		total += r;

		ts_output_get_mem_info(pcr_table[i].pout,
					       &total_size,
					       &buf_phy_start,
					       &free_size, &wp_offset, NULL);

		r = sprintf(buf,
				    "mem total:0x%0x, buf_base:0x%0x, ",
				    total_size, buf_phy_start);
		buf += r;
		total += r;

		r = sprintf(buf,
					"free size:0x%0x, rp:0x%0x, wp:0x%0x\n",
					free_size, pcr_table[i].pout->pchan->r_offset,
					wp_offset);

		buf += r;
		total += r;

		count++;
	}

	return total;
}

static void update_dvr_sid(struct out_elem *pout, int sid, int dmx_no)
{
	struct pid_entry *head_pid_slot = NULL;
	struct pid_entry *prev_pid_slot = NULL;
	struct pid_entry *new_pid_slot = NULL;
	struct pid_entry *pid_slot = NULL;

	pout->sid = sid;
	pid_slot = pout->pid_list;
	while (pid_slot) {
		dprint("change dmx id:%d, dvr dmx filter sid:0x%0x, pid:0x%0x\n",
				dmx_no, pout->sid, pid_slot->pid);
		/*free slot*/
		tsout_config_ts_table(-1, pid_slot->pid_mask,
		      pid_slot->id, pout->pchan->id, pout->sid, pout->pchan->sec_level);

		/*malloc slot and */
		new_pid_slot = _malloc_pid_entry_slot(pout->sid, pid_slot->pid);
		if (!new_pid_slot) {
			pr_dbg("malloc pid entry fail\n");
			_free_pid_entry_slot(pid_slot);
			pid_slot = pid_slot->pnext;
			return;
		}
		new_pid_slot->pid = pid_slot->pid;
		new_pid_slot->pid_mask = pid_slot->pid_mask;
		new_pid_slot->used = 1;
		new_pid_slot->dmx_id = pid_slot->dmx_id;
		new_pid_slot->ref = pid_slot->ref;
		new_pid_slot->pout = pout;
		new_pid_slot->pnext = NULL;

		if (!head_pid_slot)
			head_pid_slot = new_pid_slot;
		else
			prev_pid_slot->pnext = new_pid_slot;

		prev_pid_slot = new_pid_slot;
		tsout_config_ts_table(new_pid_slot->pid,
			new_pid_slot->pid_mask,
			new_pid_slot->id,
			pout->pchan->id,
			pout->sid,
			pout->pchan->sec_level);

		_free_pid_entry_slot(pid_slot);
		pid_slot = pid_slot->pnext;
	}
	pout->pid_list = head_pid_slot;
}

int ts_output_update_filter(int dmx_no, int sid)
{
	int i = 0;

	if (get_dvb_loop_tsn()) {
		sid = sid >= 32 ? sid : (sid + 32);
		pr_dbg("%s tsn out loop, sid:%d\n", __func__, sid);
	}

	/*update dvr filter*/
	for (i = 0; i < MAX_OUT_ELEM_NUM; i++) {
		struct out_elem *pout = &out_elem_table[i];
		u8 flag = 0;

		if (pout->used && pout->format == DVR_FORMAT) {
			struct cb_entry *tmp_cb = pout->cb_ts_list;

			while (tmp_cb) {
				if (tmp_cb->demux_id == dmx_no) {
					flag = 1;
					break;
				}
				tmp_cb = tmp_cb->next;
			}
			if (flag)
				update_dvr_sid(pout, sid, dmx_no);
		}
	}
	/*update es table filter*/
	for (i = 0; i < MAX_ES_NUM; i++) {
		struct es_entry *es_slot = &es_table[i];
		struct out_elem *pout = NULL;
		u8 flag = 0;

		if (es_slot->used) {
			struct cb_entry *tmp_cb = NULL;
			pout = es_slot->pout;

			if (es_slot->status == ES_FORMAT || es_slot->status == PES_FORMAT)
				tmp_cb = pout->cb_ts_list;
			else if (es_slot->status == SECTION_FORMAT)
				tmp_cb = pout->cb_sec_list;

			while (tmp_cb) {
				if (tmp_cb->demux_id == dmx_no) {
					flag = 1;
					break;
				}
				tmp_cb = tmp_cb->next;
			}

			if (flag) {
				pout->sid = sid;
				dprint("change dmx id:%d, filter sid:0x%0x, pid:0x%0x\n",
					dmx_no, pout->sid, es_slot->pid);
				tsout_config_es_table(es_slot->buff_id, es_slot->pid,
				      pout->sid, 0, !drop_dup, pout->format);
			}
		}
	}
	return 0;
}

int ts_output_set_dump_timer(int flag)
{
	mod_timer(&ts_out_task_tmp.out_timer,
	  jiffies + msecs_to_jiffies(out_flush_time));

	return 0;
}

int ts_output_set_decode_info(int sid, struct decoder_mem_info *info)
{
	int i = 0;
	unsigned int total_size = 0;
	unsigned int buf_phy_start = 0;
	unsigned int free_size = 0;
	unsigned int wp_offset = 0;
	struct out_elem *pout;
	struct es_entry *es_slot;

	for (i = 0; i < MAX_ES_NUM; i++) {
		es_slot = &es_table[i];
		pout = es_slot->pout;

		if (es_slot->used &&
			es_slot->status == ES_FORMAT &&
			pout->type == VIDEO_TYPE &&
			pout->sid == sid) {
			ts_output_get_mem_info(pout,
					       &total_size,
					       &buf_phy_start,
					       &free_size, &wp_offset, NULL);
			if (info->rp_phy >= buf_phy_start &&
				info->rp_phy <= (buf_phy_start + total_size)) {
				pout->decoder_rp_offset = info->rp_phy - buf_phy_start;
				return 0;
			}
		}
	}

	return 0;
}

static int aucpu_get_aud_free_size(struct out_elem *pout)
{
	struct aml_aucpu_buf_upd upd;
	unsigned int w_offset = 0;
	int free_size = -1;
	unsigned int total_size = 0;

	if (aml_aucpu_strm_get_dst(pout->aucpu_handle, &upd) >= 0) {
		w_offset = upd.phy_cur_ptr - pout->aucpu_mem_phy;
		total_size = pout->aucpu_mem_size;
		if (w_offset >= pout->aucpu_read_offset)
			free_size = total_size - w_offset + pout->aucpu_read_offset;
		else
			free_size = pout->aucpu_read_offset - w_offset;
	}
	return free_size;
}

int ts_output_check_flow_control(int sid, int percentage)
{
	int i = 0;
	struct es_entry *es_slot;
	unsigned int total_size = 0;
	unsigned int buf_phy_start = 0;
	unsigned int free_size = 0;
	unsigned int wp_offset = 0;
	unsigned int buff_len = 0;
	unsigned int level = 0;
	struct out_elem *pout;
	struct cb_entry *ptmp = NULL;

	for (i = 0; i < MAX_ES_NUM; i++) {
		es_slot = &es_table[i];
		pout = es_slot->pout;

		if (es_slot->used &&
			es_slot->status == ES_FORMAT &&
			pout->sid == sid) {
			ts_output_get_mem_info(pout,
				&total_size,
				&buf_phy_start,
				&free_size, &wp_offset, NULL);
			level = (unsigned long)total_size * percentage / 100;

			if (pout->type == VIDEO_TYPE) {
				if (pout->decoder_rp_offset == INVALID_DECODE_RP)
					continue;
				if (pout->decoder_rp_offset <= wp_offset) {
					buff_len = wp_offset - pout->decoder_rp_offset;
					if (buff_len >= level) {
						pr_dbg("%s v 1 buf:0x%0x, level:0x%0x\n",
							__func__, buff_len, level);
						return -1;
					}
				} else {
					buff_len = wp_offset - pout->decoder_rp_offset + total_size;
					if (buff_len >= level) {
						pr_dbg("%s v 2 buf:0x%0x, level:0x%0x\n",
							__func__, buff_len, level);
						return -1;
					}
				}
			} else if (pout->type == AUDIO_TYPE) {
				if (pout->aucpu_start) {
					free_size = aucpu_get_aud_free_size(pout);
					if (free_size == -1)
						continue;
					total_size = pout->aucpu_mem_size;
					pr_dbg("aucpu flow control free_size:0x%0x\n", free_size);
				}
				if ((total_size - free_size) >= level) {
					pr_dbg("%s a 1 free:0x%0x, level:0x%0x\n",
						__func__, free_size, level);
					return -2;
				}
				ptmp = pout->cb_ts_list;
				while (ptmp && ptmp->cb) {
					if (check_dmx_filter_buff(ptmp->udata[0],
						(total_size - free_size)) != 0) {
						pr_dbg("%s a 2 total_size:0x%0x, free:0x%0x\n",
							__func__, total_size, free_size);
						return -3;
					}
					ptmp = ptmp->next;
				}
			}
		}
	}
	return 0;
}

int ts_output_get_wp(struct out_elem *pout, unsigned int *wp)
{
	if (!pout) {
		*wp = 0;
		return -1;
	}
	*wp = SC2_bufferid_get_wp_offset(pout->pchan);
	return 0;
}

int ts_output_get_meminfo(struct out_elem *pout, unsigned int *size,
	unsigned long *mem, unsigned long *mem_phy)
{
	if (!pout) {
		*mem = 0;
		*mem_phy = 0;
		return -1;
	}
	*size = pout->pchan->mem_size;
	*mem = pout->pchan->mem;
	*mem_phy = pout->pchan->mem_phy;
	return 0;
}

int ts_output_dump_clone_info(char *buf)
{
	int i = 0;
	int count = 0;
	int r, total = 0;

	r = sprintf(buf, "********ts clone ********\n");
	buf += r;
	total += r;

	for (i = 0; i < MAX_OUT_ELEM_NUM; i++) {
		struct out_elem *pout = &out_elem_table[i];
		unsigned int total_size = 0;
		unsigned int buf_phy_start = 0;
		unsigned int free_size = 0;
		unsigned int wp_offset = 0;

		if (pout->used && pout->format == CLONE_FORMAT) {
			r = sprintf(buf, "%d ts clone sid:0x%0x ",
					count, pout->sid);
			buf += r;
			total += r;

			ts_output_get_mem_info(pout,
					       &total_size,
					       &buf_phy_start,
					       &free_size, &wp_offset, NULL);
			r = sprintf(buf,
				    "mem total:0x%0x, buf_base:0x%0x, ",
				    total_size, buf_phy_start);
			buf += r;
			total += r;

			r = sprintf(buf,
				    "free size:0x%0x, wp:0x%0x\n", free_size, wp_offset);
			buf += r;
			total += r;

			count++;
		}
	}
	return total;
}
