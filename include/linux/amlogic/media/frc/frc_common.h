/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __FRC_COMMON_H__
#define __FRC_COMMON_H__

#ifndef MAX
#define MAX(a, b) ({ \
			typeof(a) _a = a; \
			typeof(b) _b = b; \
			_a > _b ? _a : _b; \
		})
#endif // MAX

#ifndef MIN
#define MIN(a, b) ({ \
			typeof(a) _a = a; \
			typeof(b) _b = b; \
			_a < _b ? _a : _b; \
		})
#endif // MIN

#ifndef ABS
#define ABS(a)	({ \
			int _a = (int)a;\
			((_a) > 0 ? (_a) : -(_a));\
		})
#endif // ABS

#ifndef DIV
#define DIV(a, b) ({ \
			int _a = (int)a; \
			int _b = (int)b; \
			_b == 0 ? 0 : _a / _b; \
		})
#endif

#define BIT_0		0x00000001
#define BIT_1		0x00000002
#define BIT_2		0x00000004
#define BIT_3		0x00000008
#define BIT_4		0x00000010
#define BIT_5		0x00000020
#define BIT_6		0x00000040
#define BIT_7		0x00000080
#define BIT_8		0x00000100
#define BIT_9		0x00000200
#define BIT_10		0x00000400
#define BIT_11		0x00000800
#define BIT_12		0x00001000
#define BIT_13		0x00002000
#define BIT_14		0x00004000
#define BIT_15		0x00008000
#define BIT_16		0x00010000
#define BIT_17		0x00020000
#define BIT_18		0x00040000
#define BIT_19		0x00080000
#define BIT_20		0x00100000
#define BIT_21		0x00200000
#define BIT_22		0x00400000
#define BIT_23		0x00800000
#define BIT_24		0x01000000
#define BIT_25		0x02000000
#define BIT_26		0x04000000
#define BIT_27		0x08000000
#define BIT_28		0x10000000
#define BIT_29		0x20000000
#define BIT_30		0x40000000
#define BIT_31		0x80000000

#define FRC_DS_11	0
#define FRC_DS_12	1
#define FRC_DS_14	2

enum dbg_level {
	dbg_frc = 0,
	dbg_sts = 5,
	dbg_bbd = 100,
	dbg_film = 200,
	dbg_logo = 300,
	dbg_me = 400,
	dbg_mc = 500,
	dbg_scene = 600,
	dbg_vp = 700,
	dbg_glb = 800,
};

enum efrc_event {
	FRC_EVENT_NO_EVENT		= 0x00000000,
	FRC_EVENT_VF_CHG_TO_NO		= 0x00000001,
	FRC_EVENT_VF_CHG_TO_HAVE	= 0x00000002,
	FRC_EVENT_VF_IS_GAME		= 0x00000004,
	FRC_EVENT_VF_CHG_IN_SIZE	= 0x00000008,
	FRC_EVENT_VOUT_CHG		= 0x00000010,
};

enum eFRC_POS {
	FRC_POS_BEFORE_POSTBLEND = 0,
	FRC_POS_AFTER_POSTBLEND = 1,
};

enum efrc_memc_level {
	FRC_MEMC_LOW      = 4,
	FRC_MEMC_MID      = 7,
	FRC_MEMC_HIGH     = 10,
};

enum efrc_memc_dbg_type {
	MEMC_DBG_BBD_FINAL_LINE   = 0x01,
	MEMC_DBG_VP_CTRL          = 0x02,
	MEMC_DBG_LOGO_CTRL        = 0x03,
	MEMC_DBG_IPLOGO_CTRL      = 0x04,
	MEMC_DBG_MELOGO_CTRL      = 0x05,
	MEMC_DBG_SENCE_CHG_DETECT = 0x06,
	MEMC_DBG_FB_CTRL          = 0x07,
	MEMC_DBG_ME_CTRL          = 0x08,
	MEMC_DBG_SEARCH_RANG      = 0x09,
	MEMC_DBG_PIXEL_LPF        = 0x0A,
	MEMC_DBG_ME_RULE          = 0x0B,
	MEMC_DBG_FILM_CTRL        = 0x0C,
	MEMC_DBG_GLB_CTRL	  = 0x0D,
};

//-----------------------------------------------------------frc top cfg
enum frc_ratio_mode_type {
	FRC_RATIO_1_2 = 0,
	FRC_RATIO_2_3,
	FRC_RATIO_2_5,
	FRC_RATIO_5_6,
	FRC_RATIO_5_12,
	FRC_RATIO_2_9,
	FRC_RATIO_1_1
};

enum en_film_mode {
	EN_VIDEO=0,
	EN_FILM22,
	EN_FILM32,
	EN_FILM3223,
	EN_FILM2224,
	EN_FILM32322,
	EN_FILM44,
	EN_FILM21111,
	EN_FILM23322,
	EN_FILM2111,
	EN_FILM22224,
	EN_FILM33,
	EN_FILM334,
	EN_FILM55,
	EN_FILM64,
	EN_FILM66,
	EN_FILM87,
	EN_FILM212,
	EN_FILM1123,
	EN_FILM7231313,
	EN_FILM31,
	EN_FILM6446,
	EN_FILM64644,
	EN_FILM1010,
	EN_FILM128,
	EN_FILM_MAX = 0xFF,
};

struct frc_holdline_s {
	u32 me_hold_line;//me_hold_line
	u32 mc_hold_line;//mc_hold_line
	u32 inp_hold_line;
	u32 reg_post_dly_vofst;//fixed
	u32 reg_mc_dly_vofst0;//fixed
};

struct frc_top_type_s {
	/*input*/
	u32       hsize;
	u32       vsize;
	u32       vfp;//line num before vsync,VIDEO_VSO_BLINE
	u32       vfb;//line num before de   ,VIDEO_VAVON_BLINE
	u32       frc_fb_num;//buffer num for frc loop
	enum frc_ratio_mode_type frc_ratio_mode;//frc_ratio_mode = frame rate of input : frame rate of output
	enum en_film_mode    film_mode;//film_mode
	u32       film_hwfw_sel;//0:hw 1:fw
	u32       is_me1mc4;//1: me:mc=1/4, 0 : me:mc=1/2, default 0
	u32       me_loss_en;//default 0
	u32       mc_loss_en;//default 0
	u32       frc_prot_mode;//0:memc prefetch acorrding mode frame 1:memc prefetch 1 frame
	u32       force_en;    // for debug
	u32       in_out_ratio;  // for debug

	/*output*/
	u32 out_hsize;
	u32 out_vsize;
	/* frc_other*/
	u8  frc_memc_level;
	u8  frc_memc_level_1; // default 0, 1:fullback, 2:24P film
	u8  frc_other_reserved;
	u16 frc_other_info;
	u32 frc_reserved;

};

struct frc_fw_alg_ctrl_s {
	u8 frc_algctrl_u8vendor;  // vendor information
	u8 frc_algctrl_u8mcfb;
	u8 frc_algctrl_u8param3;
	u8 frc_algctrl_u8param4;
	u8 frc_algctrl_u8param5;
	u8 frc_algctrl_u8param6;
	u8 frc_algctrl_u8param7;
	u8 frc_algctrl_u8param8;
	u16 frc_algctrl_u16param1;
	u16 frc_algctrl_u16param2;
	u32 frc_algctrl_u32film;
	u32 frc_algctrl_u32param2;
};

#define MONITOR_REG_MAX	6
#define DBG_REG_BUFF	4096

struct frc_fw_data_s {
	/*scene*/
	void (*scene_detect_input)(struct frc_fw_data_s *fw_data);
	void (*scene_detect_output)(struct frc_fw_data_s *fw_data);

	/*bbd*/
	void (*bbd_ctrl)(struct frc_fw_data_s *fw_data);

	/*logo*/
	void (*iplogo_ctrl)(struct frc_fw_data_s *fw_data);
	void (*melogo_ctrl)(struct frc_fw_data_s *fw_data);

	/*film detect*/
	void (*film_detect_ctrl)(struct frc_fw_data_s *fw_data);

	/*me*/
	void (*me_ctrl)(struct frc_fw_data_s *fw_data);

	/*mc*/
	void (*mc_ctrl)(struct frc_fw_data_s *fw_data);
	void (*vp_ctrl)(struct frc_fw_data_s *fw_data);

	/*frc top type config*/
	struct frc_top_type_s frc_top_type;
	struct frc_holdline_s holdline_parm;
	void (*frc_input_cfg)(struct frc_fw_data_s *fw_data);
	void (*frc_memc_level)(struct frc_fw_data_s *fw_data);
	ssize_t (*frc_alg_dbg_show)(struct frc_fw_data_s *fw_data,
					enum efrc_memc_dbg_type dbg_type, char *buf);
	ssize_t (*frc_alg_dbg_stor)(struct frc_fw_data_s *fw_data,
					enum efrc_memc_dbg_type dbg_type, char *buf, size_t count);
	u8 frc_alg_ver[32];
	void (*frc_fw_reinit)(void);
	/*below interfaces in alg which fae can to adjust it **/
	struct frc_fw_alg_ctrl_s  frc_fw_alg_ctrl;
	void (*frc_fw_ctrl_if)(struct frc_fw_data_s *fw_data);
};

extern int frc_dbg_en;
extern int frc_kerdrv_ver;
void config_phs_regs(enum frc_ratio_mode_type frc_ratio_mode,
	enum en_film_mode film_mode);


#define pr_frc(level, fmt, arg...)			\
	do {						\
		if ((frc_dbg_en >= (level) && frc_dbg_en < 3) || frc_dbg_en == level)	\
			pr_info("frc: " fmt, ## arg);	\
	} while (0)

#define PR_ERR(fmt, args ...)		pr_info("frc_Err: " fmt, ##args)
#define PR_FRC(fmt, args ...)		pr_info("frc: " fmt, ##args)

#endif
