/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_AUDIO_DDR_MANAGER_H_
#define __AML_AUDIO_DDR_MANAGER_H_

#include <linux/device.h>
#include <linux/interrupt.h>
#include <sound/soc.h>
#include "audio_io.h"

#define MEMIF_INT_ADDR_FINISH       BIT(0)
#define MEMIF_INT_ADDR_INT          BIT(1)
#define MEMIF_INT_COUNT_REPEAT      BIT(2)
#define MEMIF_INT_COUNT_ONCE        BIT(3)
#define MEMIF_INT_FIFO_ZERO         BIT(4)
#define MEMIF_INT_FIFO_DEPTH        BIT(5)
#define MEMIF_INT_MASK              GENMASK(7, 0)

#define TODDR_FIFO_CNT              GENMASK(19, 8)
#define FRDDR_FIFO_CNT              GENMASK(17, 8)

#define FIFO_BURST                  8
#define FIFO_DEPTH_32K              0x8000
#define FIFO_DEPTH_2K               0x800
#define FIFO_DEPTH_1K               0x400
#define FIFO_DEPTH_512              0x200

#define DDRMAX 5

enum ddr_num {
	DDR_A,
	DDR_B,
	DDR_C,
	DDR_D,
	DDR_E,
	DDR_MAX,
};

enum ddr_types {
	LJ_8BITS,
	LJ_16BITS,
	RJ_16BITS,
	LJ_32BITS,
	RJ_32BITS,
};

/*
 * from tl1, add new source FRATV, FRHDMIRX, LOOPBACK_B, SPDIFIN_LB, VAD
 */
enum toddr_src {
	TODDR_INVAL = -1,
	TDMIN_A = 0,
	TDMIN_B = 1,
	TDMIN_C = 2,
	SPDIFIN = 3,
	PDMIN = 4,
	FRATV = 5, /* NONE for axg, g12a, g12b */
	TDMIN_LB = 6,
	LOOPBACK_A = 7,
	FRHDMIRX = 8, /* from tl1 chipset*/
	LOOPBACK_B = 9,
	SPDIFIN_LB = 10,
	EARCRX_DMAC = 11,/* from sm1 chipset */
	FRHDMIRX_PAO = 12, /* tm2 */
	RESAMPLEA = 13,   /* t5 */
	RESAMPLEB = 14,
	VAD = 15,
	PDMIN_B = 16,
	TDMINB_LB = 17,
	TDMIN_D = 18,
	TODDR_SRC_MAX = 19
};

enum resample_idx {
	RESAMPLE_A,
	RESAMPLE_B
};

enum resample_src {
	ASRC_TODDR_A,
	ASRC_TODDR_B,
	ASRC_TODDR_C,
	ASRC_TODDR_D,      /* from tl1 chipset */
	ASRC_LOOPBACK_A,
	ASRC_LOOPBACK_B,
};

enum frddr_dest {
	TDMOUT_A,
	TDMOUT_B,
	TDMOUT_C,
	SPDIFOUT_A,
	SPDIFOUT_B,
	EARCTX_DMAC,
	TDMOUT_D,
	FRDDR_MAX
};

enum status_sel {
	CURRENT_DDR_ADDR,
	NEXT_FINISH_ADDR,
	COUNT_CURRENT_DDR_ACK,
	COUNT_NEXT_FINISH_DDR_ACK,
	VAD_WAKEUP_ADDR,   /* from tl1, vad */
	VAD_FS_ADDR,
	VAD_FIFO_CNT,
};

struct toddr_fmt {
	unsigned int type;
	unsigned int msb;
	unsigned int lsb;
	unsigned int endian;
	unsigned int bit_depth;
	unsigned int ch_num;
	unsigned int rate;
};

struct toddr_src_conf {
	char name[32];
	unsigned int val;
	unsigned int reg;
	unsigned int shift;
	unsigned int mask;
};

struct frddr_src_conf {
	char name[32];
	unsigned int val;
	unsigned int reg;
	unsigned int shift;
	unsigned int mask;
};

struct ddr_chipinfo {
	/* INT and Start address is same or separated */
	bool int_start_same_addr;
	/* force finished */
	bool force_finished;
	/* same source */
	bool same_src_fn;
	/* insert channel number */
	bool insert_chnum;

	/* ddr bus in urgent */
	bool ugt;

	/* source sel switch to ctrl1
	 * for toddr, 0: source sel is controlled by ctrl0
	 *            1: source sel is controlled by ctrl1
	 * for frddr, 0: source sel is controlled by ctrl0
	 *            1: source sel is controlled by ctrl2
	 */
	bool src_sel_ctrl;

	/*
	 * resample source sel switch
	 * resample : from ctrl0 to ctrl3
	 * toddr : from ctrl0 to ctrl1
	 */
	bool asrc_src_sel_ctrl;
	/* spdif in 32bit, only support left justified */
	bool asrc_only_left_j;

	/* toddr number max
	 * 0: default, 3 toddr, axg, g12a, g12b
	 * 4: 4 toddr, tl1
	 */
	int toddr_num;
	int frddr_num;
	unsigned int fifo_depth;

	/* power detect or VAD
	 * 0: disabled
	 * 1: power detect
	 * 2: vad
	 */
	int wakeup;

	bool chnum_sync;

	bool burst_finished_flag;

	struct toddr_src_conf *to_srcs;
	struct toddr_src_conf *fr_srcs;
	bool use_arb;
};

struct toddr {
	struct device *dev;
	unsigned int resample: 1;
	unsigned int ext_signed: 1;
	unsigned int reg_base;

	struct toddr_fmt fmt;
	unsigned int start_addr;
	unsigned int end_addr;
	unsigned int threshold;

	enum toddr_src src;
	unsigned int fifo_id;

	int is_lb; /* check whether for loopback */
	int irq;
	bool in_use: 1;
	struct aml_audio_controller *actrl;
	struct ddr_chipinfo *chipinfo;
	char toddr_name[32];
};

enum status {
	DISABLED,
	READY,    /* controls has set enable, but ddr is not in running */
	RUNNING,
};

struct toddr_attach {
	bool enable;
	enum resample_idx id;
	int status;
	/* which module should be attached,
	 * check which toddr in use should be attached
	 */
	enum toddr_src attach_module;
	int resample_version;
	struct mutex lock; /* device lock */
};

struct frddr_attach {
	bool enable;
	int status;
	/* which module for attach ,
	 * check which frddr in use should be added
	 */
	enum frddr_dest attach_module;
};

struct frddr {
	struct device *dev;
	enum frddr_dest dest;

	/* dest for same source, whether enable */
	enum frddr_dest ss_dest;
	bool ss_en;
	enum frddr_dest ss2_dest;
	bool ss2_en;

	struct aml_audio_controller *actrl;
	unsigned int reg_base;
	enum ddr_num fifo_id;

	unsigned int channels;
	unsigned int rate;
	unsigned int msb;
	unsigned int type;

	int irq;
	bool in_use;
	struct ddr_chipinfo *chipinfo;

	bool reserved;
	char frddr_name[32];
};

struct ddr_info {
	unsigned int toddr_addr;
	unsigned int frddr_addr;
};

/* to ddrs */
struct toddr *fetch_toddr_by_src(int toddr_src);
struct toddr *aml_audio_register_toddr(struct device *dev,
		irq_handler_t handler, void *data);
int aml_audio_unregister_toddr(struct device *dev, void *data);
void audio_toddr_irq_enable(struct toddr *to, bool en);
int aml_toddr_set_buf(struct toddr *to, unsigned int start,
			unsigned int end);
int aml_toddr_set_buf_startaddr(struct toddr *to, unsigned int start);
int aml_toddr_set_buf_endaddr(struct toddr *to,	unsigned int end);

int aml_toddr_set_intrpt(struct toddr *to, unsigned int intrpt);
unsigned int aml_toddr_get_position(struct toddr *to);
unsigned int aml_toddr_get_addr(struct toddr *to, enum status_sel sel);
void aml_toddr_select_src(struct toddr *to, enum toddr_src);
void aml_toddr_enable(struct toddr *to, bool enable);
void aml_toddr_set_fifos(struct toddr *to, unsigned int threshold);
void aml_toddr_force_finish(struct toddr *to);
void aml_toddr_set_format(struct toddr *to, struct toddr_fmt *fmt);
int toddr_src_get_reg(struct toddr *to, enum toddr_src src);

unsigned int aml_toddr_get_status(struct toddr *to);
unsigned int aml_toddr_get_fifo_cnt(struct toddr *to);
void aml_toddr_ack_irq(struct toddr *to, int status);

void aml_toddr_insert_chanum(struct toddr *to);
unsigned int aml_toddr_read(struct toddr *to);
void aml_toddr_write(struct toddr *to, unsigned int val);
unsigned int aml_toddr_read1(struct toddr *to);
void aml_toddr_write1(struct toddr *to, unsigned int val);
unsigned int aml_toddr_read_status2(struct toddr *to);
bool aml_toddr_burst_finished(struct toddr *to);

void toddr_vad_enable(bool enable);
void toddr_vad_set_buf(unsigned int start, unsigned int end);
void toddr_vad_set_intrpt(unsigned int intrpt);
void toddr_vad_select_src(enum toddr_src src);
void toddr_vad_set_fifos(unsigned int thresh);
void toddr_vad_set_format(struct toddr_fmt *fmt);
unsigned int toddr_vad_get_status(void);
unsigned int toddr_vad_get_status2(struct toddr *to);

/* resample */
void aml_set_resample(enum resample_idx id,
		bool enable, enum toddr_src resample_module);
/* power detect */
void aml_pwrdet_enable(bool enable, int pwrdet_module);
/* Voice Activity Detection */
void aml_set_vad(bool enable, int module);

/* from ddrs */
struct frddr *fetch_frddr_by_src(int frddr_src);

struct frddr *aml_audio_register_frddr(struct device *dev,
		irq_handler_t handler, void *data, bool rvd_dst);
int aml_audio_unregister_frddr(struct device *dev, void *data);
int aml_frddr_set_buf(struct frddr *fr, unsigned int start,
			unsigned int end);
int aml_frddr_set_intrpt(struct frddr *fr, unsigned int intrpt);
unsigned int aml_frddr_get_position(struct frddr *fr);
void aml_frddr_enable(struct frddr *fr, bool enable);
void aml_frddr_select_dst(struct frddr *fr, enum frddr_dest);
void aml_frddr_select_dst_ss(struct frddr *fr,
	enum frddr_dest dst, int sel, bool enable);

int aml_check_sharebuffer_valid(struct frddr *fr, int ss_sel);

void aml_frddr_set_fifos(struct frddr *fr,
		unsigned int depth, unsigned int threshold);
unsigned int aml_frddr_get_fifo_id(struct frddr *fr);
void aml_frddr_set_format(struct frddr *fr,
	unsigned int chnum,
	unsigned int rate,
	unsigned int msb,
	unsigned int frddr_type);

void aml_frddr_reset(struct frddr *fr, int offset);

/* audio eq drc */
void aml_set_aed(bool enable, int aed_module);
void aml_aed_top_enable(struct frddr *fr, bool enable);

void frddr_init_without_mngr(unsigned int frddr_index, unsigned int src0_sel);
void frddr_deinit_without_mngr(unsigned int frddr_index);

enum toddr_src toddr_src_get(void);
const char *toddr_src_get_str(enum toddr_src idx);
int frddr_src_get(void);
const char *frddr_src_get_str(int idx);

int card_add_ddr_kcontrols(struct snd_soc_card *card);

void pm_audio_set_suspend(bool is_suspend);
bool pm_audio_is_suspend(void);

void aml_frddr_check(struct frddr *fr);
void aml_aed_set_frddr_reserved(void);
void get_toddr_bits_config(enum toddr_src src,
	int bit_depth, int *msb, int *lsb);
int aml_check_and_release_sharebuffer(struct frddr *fr, enum frddr_dest ss_sel);

#endif

