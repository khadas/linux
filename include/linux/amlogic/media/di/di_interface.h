/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DI_INTERLACE_H__
#define __DI_INTERLACE_H__

/**********************************************************
 * get di driver's version:
 *	DI_DRV_OLD_DEINTERLACE	: old deinterlace
 *	DI_DRV_MULTI		: di_multi
 **********************************************************/
#define DI_DRV_DEINTERLACE	(0)
#define DI_DRV_MULTI		(1)

unsigned int dil_get_diffver_flag(void);

/**********************************************************
 * di new interface
 **********************************************************/
enum di_work_mode {
	WORK_MODE_PRE = 0,
	WORK_MODE_POST,
	WORK_MODE_PRE_POST,
	/*copy for decoder 1/4 * 1/4 */
	/*  */
	WORK_MODE_S4_DCOPY,
	WORK_MODE_PRE_VPP_LINK,
	WORK_MODE_MAX
};

enum di_buffer_mode {
	BUFFER_MODE_ALLOC_BUF = 0, /*di alloc output buffer*/
	BUFFER_MODE_USE_BUF,
	/* BUFFER_MODE_USE_BUF
	 * di not alloc output buffer, need caller alloc output buffer
	 ***************************/
	BUFFER_MODE_MAX
};

/**********************************************************
 * If flag is DI_BUFFERFLAG_ENDOFFRAME,
 * di_buffer is invalid,
 * only notify di that there will be no more input
 *********************************************************/
#define DI_BUFFERFLAG_ENDOFFRAME 0x00000001

struct ins_mng_s {	/*ary add*/
	unsigned int	code;
	unsigned int	ch;
	unsigned int	index;

	unsigned int	type;
	struct page	*pages;
};

struct di_buffer {
	/*header*/
	struct ins_mng_s mng;
	void *private_data;
	u32 size;
	unsigned long phy_addr;
	struct vframe_s *vf;
	void *caller_data; /*from di_init_parm.caller_data*/
	u32 flag;
	unsigned int crcout;
	unsigned int nrcrcout;
};

enum DI_FLAG {
	DI_FLAG_P               = 0x1,
	DI_FLAG_I               = 0x2,
	DI_FLAG_EOS		= 0x4,
	DI_FLAG_BUF_BY_PASS	= 0x8,
	DI_FLAG_MAX = 0x7FFFFFFF,
};

enum DI_ERRORTYPE {
	DI_ERR_NONE = 0,
	DI_ERR_UNDEFINED = 0x80000001,
	DI_ERR_REG_NO_IDLE_CH,
	DI_ERR_INDEX_OVERFLOW,
	DI_ERR_INDEX_NOT_ACTIVE,
	DI_ERR_IN_NO_SPACE,
	DI_ERR_UNSUPPORT,
	DI_ERR_MAX = 0x7FFFFFFF,
};

struct di_operations_s {
	/*The input data has been processed and returned to the caller*/
	enum DI_ERRORTYPE (*empty_input_done)(struct di_buffer *buf);
	/* The output buffer has filled in the data and returned it
	 * to the caller.
	 */
	enum DI_ERRORTYPE (*fill_output_done)(struct di_buffer *buf);
};

enum di_output_format {
	DI_OUTPUT_422 = 0,
	DI_OUTPUT_NV12 = 1,
	DI_OUTPUT_NV21 = 2,	/* ref to DIM_OUT_FORMAT_FIX_MASK */
	DI_OUTPUT_BYPASS	= 0x10000000, /*22-06-24*/
	DI_OUTPUT_TVP		= 0x20000000, /*21-03-02*/
	DI_OUTPUT_LINEAR	= 0x40000000,
	/*1:di output must linear, 0: determined by di,may be linear or block*/
	DI_OUTPUT_BY_DI_DEFINE	= 0x80000000,
	/* when use di's local buffer, can use this option,
	 * when use this option, blow define is no use
	 *	DI_OUTPUT_422
	 *	DI_OUTPUT_NV12
	 *	DI_OUTPUT_NV21
	 *	DI_OUTPUT_LINEAR
	 */

	DI_OUTPUT_MAX = 0x7FFFFFFF,
};

#define DIM_OUT_FORMAT_FIX_MASK		(0xffff)

struct di_init_parm {
	enum di_work_mode work_mode;
	enum di_buffer_mode buffer_mode;
	struct di_operations_s ops;
	void *caller_data;
	enum di_output_format output_format;
};

struct di_status {
	unsigned int status;
};

struct composer_dst {
	unsigned int h;
};

/**
 * @brief  di_create_instance  creat di instance
 *
 * @param[in]  parm    Pointer of parm structure
 *
 * @return      di index for success, or fail type if < 0
 */
int di_create_instance(struct di_init_parm parm);

/**
 * @brief  di_set_parameter  set parameter to di for init
 *
 * @param[in]  index   instance index
 *
 * @return      0 for success, or fail type if < 0
 */

int di_destroy_instance(int index);

/**
 * @brief  di_empty_input_buffer  send input date to di
 *
 * @param[in]  index   instance index
 * @param[in]  buffer  Pointer of buffer structure
 *
 * @return      Success or fail type
 */
enum DI_ERRORTYPE di_empty_input_buffer(int index, struct di_buffer *buffer);

/**
 * @brief  di_fill_output_buffer  send output buffer to di
 *
 * @param[in]  index   instance index
 * @param[in]  buffer  Pointer of buffer structure
 * @ 2019-12-30:discuss with jintao
 * @ when pre + post + local mode,
 * @	use this function put post buf to di
 * @return      Success or fail type
 */

enum DI_ERRORTYPE di_fill_output_buffer(int index, struct di_buffer *buffer);

/**
 * @brief  di_get_state  Get the state of di by instance index
 *
 * @param[in]   index   instance index
 * @param[out]  status  Pointer of di status structure
 *
 * @return      0 for success, or fail type if < 0
 */
int di_get_state(int index, struct di_status *status);

/**
 * @brief  di_write  use di pre buffer to do post
 *
 * @param[in]  buffer        di pre output buffer
 * @param[in]  composer_dst  Pointer of dst buffer and composer para structure
 *
 * @return      0 for success, or fail type if < 0
 */
int di_write(struct di_buffer *buffer, struct composer_dst *dst);

/**
 * @brief  di_release_keep_buf  release buf after instance destroy
 *
 * @param[in]  buffer        di output buffer
 *
 * @return      0 for success, or fail type if < 0
 */
int di_release_keep_buf(struct di_buffer *buffer);

/**
 * @brief  di_get_output_buffer_num  get output buffer num
 *
 * @param[in]  index   instance index
 * @param[in]  buffer  Pointer of buffer structure
 *
 * @return      number or fail type
 */
int di_get_output_buffer_num(int index);

/**
 * @brief  di_get_input_buffer_num  get input buffer num
 *
 * @param[in]  index   instance index
 * @param[in]  buffer  Pointer of buffer structure
 *
 * @return      number or fail type
 */
int di_get_input_buffer_num(int index);

/**************************************
 * pre-vpp link define
 **************************************/
enum EPVPP_DISPLAY_MODE {
	EPVPP_DISPLAY_MODE_BYPASS = 0,
	EPVPP_DISPLAY_MODE_NR,
};

enum EPVPP_ERROR {
	EPVPP_ERROR_DI_NOT_REG = 0x80000001,
	EPVPP_ERROR_VPP_OFF,	/*ref to pvpp_sw*/
	EPVPP_ERROR_DI_OFF,
	EPVPP_ERROR_VFM_NOT_ACT,
};

enum EPVPP_STATS {
	EPVPP_REG_BY_DI		= 0x00000001,
	EPVPP_REG_BY_VPP	= 0x00000002,/*ref to pvpp_sw*/
	EPVPP_VFM_ACT		= 0x00000010,
};

struct di_win_s {
	unsigned int x_size;
	unsigned int y_size;
	unsigned int x_st;
	unsigned int y_st;
	unsigned int x_end;
	unsigned int y_end;
};

struct pvpp_dis_para_in_s {
	enum EPVPP_DISPLAY_MODE dmode;
	bool unreg_bypass; //for unreg bypass: set 1; other, set 0;
	struct di_win_s win;
};

int pvpp_display(struct vframe_s *vfm,
			    struct pvpp_dis_para_in_s *in_para,
			    void *out_para);

int pvpp_check_vf(struct vframe_s *vfm);
int pvpp_check_act(void);

int pvpp_sw(bool on);

/************************************************
 * di_api_get_plink_instance_id
 *	only for pre-vpp link
 *	get current pre-vpp link instance_id
 ************************************************/
u32 di_api_get_plink_instance_id(void);

void di_disable_prelink_notify(bool async);
void di_prelink_state_changed_notify(void);

/*********************************************************
 * @brief  di_s_bypass_ch  set channel bypass
 *
 * @param[in]  index   instance index
 * @param[in]  on  1:bypass enable; 0: bypass disable
 *
 * @return      number or fail type
 *********************************************************/
int di_s_bypass_ch(int index, bool on);

#endif	/*__DI_INTERLACE_H__*/
