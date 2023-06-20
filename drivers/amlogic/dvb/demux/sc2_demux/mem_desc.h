/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _MEM_DESC_H_
#define _MEM_DESC_H_

#ifndef u8
#define u8 unsigned char
#endif

union mem_desc {
	u64 data;
	struct {
		u64 byte_length:27;	// 26:0
		u64 irq:1;	// 27 not used
		u64 eoc:1;	// 28 not used
		u64 loop:1;	// 29
		u64 error:1;	// 30 not used
		u64 owner:1;	// 31 not used
		u64 address:32;	// 63:32
	} bits;
};

struct dump_input_file {
	loff_t file_pos;
	struct file *file_fp;
};

struct chan_id {
	u8 used;
	u8 id;
	u8 is_es;
	u8 mode;
	u8 enable;
	unsigned long mem;
	unsigned long mem_phy;
	unsigned int mem_size;
	int sec_level;
	union mem_desc *memdescs;
	unsigned int memdescs_phy;
	unsigned int r_offset;
	unsigned int pts_newest_r_offset;
	unsigned long memdescs_map;
	unsigned int last_w_addr;

	/*just for DVR sec direct mem*/
	unsigned int sec_mem;
	unsigned int sec_size;

	struct dump_input_file dump_file;
};

enum bufferid_mode {
	INPUT_MODE,
	OUTPUT_MODE
};

 /**/ struct bufferid_attr {
	u8 is_es;
	enum bufferid_mode mode;
	u8 req_id;
};

/**
 * chan init
 * \retval 0: success
 * \retval -1:fail.
 */
int SC2_bufferid_init(void);

/**
 * alloc chan
 * \param attr
 * \param pchan,if ts/es, return this channel
 * \param pchan1, if es, return this channel for pcr
 * \retval success: 0
 * \retval -1:fail.
 */
int SC2_bufferid_alloc(struct bufferid_attr *attr,
		       struct chan_id **pchan, struct chan_id **pchan1);

/**
 * dealloc chan & free mem
 * \param pchan:struct chan_id handle
 * \retval success: 0
 * \retval -1:fail.
 */
int SC2_bufferid_dealloc(struct chan_id *pchan);

/**
 * chan mem
 * \param pchan:struct chan_id handle
 * \param mem_size:used memory
 * \param sec_level: memory security level
 * \retval success: 0
 * \retval -1:fail.
 */
int SC2_bufferid_set_mem(struct chan_id *pchan,
			 unsigned int mem_size, int sec_level);

/**
 * chan mem
 * \param pchan:struct chan_id handle
 * \param sec_mem:direct memory
 * \param sec_size: memory size
 * \retval success: 0
 * \retval -1:fail.
 */
int SC2_bufferid_set_sec_mem(struct chan_id *pchan,
			 unsigned int sec_mem, unsigned int sec_size);

/**
 * set enable
 * \param pchan:struct chan_id handle
 * \param enable: 1/0
 * \param sid: stream id
 * \param pid: packet id
 * \retval success: 0
 * \retval -1:fail.
 */
int SC2_bufferid_set_enable(struct chan_id *pchan, int enable, int sid, int pid);

/**
 * recv data
 * \param pchan:struct chan_id handle
 * \retval 0: no data
 * \retval 1: recv data, it can call SC2_bufferid_read
 */
int SC2_bufferid_recv_data(struct chan_id *pchan);

/**
 * chan read
 * \param pchan:struct chan_id handle
 * \param pread: if is secure will return physical addr, otherwise virtual addr
 * \param plen:data size addr
 * \param is_sec: 1 is secure, 0 is normal
 * \retval >=0:read cnt.
 * \retval -1:fail.
 */
int SC2_bufferid_read(struct chan_id *pchan, char **pread, unsigned int len,
		int is_sec);

/**
 * write to channel
 * \param pchan:struct chan_id handle
 * \param buf: data addr
 * \param buf_phys: data phys addr
 * \param  count: write size
 * \param  isphybuf: isphybuf
 * \param  pack_len: 188 or 192
 * \retval -1:fail
 * \retval written size
 */
int SC2_bufferid_write(struct chan_id *pchan, const char *buf, char *buf_phys,
		       unsigned int count, int isphybuf, int pack_len);

/**
 * write to channel
 * \param pchan:struct chan_id handle
 * \param buf: data addr
 * \param buf_phys: data phys addr
 * \param  count: write size
 * \param  isphybuf: isphybuf
 * \param  pack_len: 188 or 192
 * \retval -1:fail
 * \retval written size
 */
int SC2_bufferid_non_block_write(struct chan_id *pchan, const char *buf, char *buf_phys,
		       unsigned int count, int isphybuf, int pack_len);

/**
 * check wrint done
 * \param pchan:struct chan_id handle
 * \retval 1:done, 0:not done
 */
int SC2_bufferid_non_block_write_status(struct chan_id *pchan);

/**
 * free channel
 * \param pchan:struct chan_id handle
 * \retval 0:success
 */
int SC2_bufferid_non_block_write_free(struct chan_id *pchan);

int SC2_bufferid_write_empty(struct chan_id *pchan, int pid);

unsigned int SC2_bufferid_get_free_size(struct chan_id *pchan);
unsigned int SC2_bufferid_get_wp_offset(struct chan_id *pchan);
unsigned int SC2_bufferid_get_data_len(struct chan_id *pchan);
int SC2_bufferid_read_header_again(struct chan_id *pchan, char **pread);
int SC2_bufferid_read_newest_pts(struct chan_id *pchan, char **pread);
/**
 * SC2_bufferid_move_read_rp
 * \param pchan:struct chan_id handle
 * \param len:length
 * \param flag: 0:rewind; 1:forward
 * \retval 0:success.
 * \retval -1:fail.
 */
int SC2_bufferid_move_read_rp(struct chan_id *pchan, unsigned int len, int flag);

int _alloc_buff(unsigned int len, int sec_level,
		unsigned long *vir_mem, unsigned long *phy_mem);
void _free_buff(unsigned long buf, unsigned int len, int sec_level);

int cache_status_info(char *buf);
int cache_clear(void);
int cache_adjust(int cache0_count, int cache1_count);

int dmc_mem_set_size(int sec_level, unsigned int mem_size);
int dmc_mem_dump_info(char *buf);
#endif
