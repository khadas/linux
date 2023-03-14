/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2020 Amlogic or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#ifndef __AML_ADAPTER_H__
#define __AML_ADAPTER_H__

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/clk-provider.h>
#include <linux/interrupt.h>

#include <media/v4l2-async.h>
#include <media/v4l2-subdev.h>
#include <media/media-entity.h>

#include "aml_common.h"

#define HDR_LOOPBACK_MODE 0

#define ADAP_DDR_BUFF_CNT 4
#define ADAP_DOL_BUFF_CNT 2
#define ADAP_ALIGN(data, val) ((data + val - 1) & (~(val - 1)))

enum {
	MODE_MIPI_RAW_SDR_DDR = 0,
	MODE_MIPI_RAW_SDR_DIRCT,
	MODE_MIPI_RAW_HDR_DDR_DDR,
	MODE_MIPI_RAW_HDR_DDR_DIRCT,
	MODE_MIPI_YUV_SDR_DDR,
	MODE_MIPI_RGB_SDR_DDR,
	MODE_MIPI_YUV_SDR_DIRCT,
	MODE_MIPI_RGB_SDR_DIRCT
};

enum {
	AML_ADAP_PAD_SINK = 0,
	AML_ADAP_PAD_SRC,
	AML_ADAP_PAD_MAX,
};

enum {
	AML_ADAP_STREAM_TOF,
	AML_ADAP_STREAM_MAX
};

enum {
	ADAP_PATH0 = 0,
	ADAP_PATH1,
	ADAP_PATH_MAX
};

enum {
	ADAP_DDR_MODE = 0,
	ADAP_DIR_MODE,
	ADAP_DOL_MODE,
	ADAP_MODE_MAX
};

#define ADAP_YUV422_8BIT  0x1e
#define ADAP_YUV422_10BIT 0x1f
#define ADAP_RGB444       0x20
#define ADAP_RGB555       0x21
#define ADAP_RGB565       0x22
#define ADAP_RGB666       0x23
#define ADAP_RGB888       0x24
#define ADAP_RAW6         0x28
#define ADAP_RAW7         0x29
#define ADAP_RAW8         0x2a
#define ADAP_RAW10        0x2b
#define ADAP_RAW12        0x2c
#define ADAP_RAW14        0x2d

enum {
	ADAP_DOL_NONE = 0,
	ADAP_DOL_VC,
	ADAP_DOL_LINEINFO,
	ADAP_DOL_MAX
};
/*
struct adap_regval {
	u32 reg;
	u32 val;
};*/

struct adap_exp_offset {
	int long_offset;
	int short_offset;
	int offset_x;
	int offset_y;
};

typedef struct {
   int fe_sel             ;//add for sel 7 mipi_isp_top
   int fe_cfg_ddr_max_bytes_other;
   int fe_dec_ctrl0       ;
   int fe_dec_ctrl1       ;
   int fe_dec_ctrl2       ;
   int fe_dec_ctrl3       ;
   int fe_dec_ctrl4       ;

   int fe_work_mode       ;
   int fe_mem_x_start     ;
   int fe_mem_x_end       ;
   int fe_mem_y_start     ;
   int fe_mem_y_end       ;
   int fe_isp_x_start     ;
   int fe_isp_x_end       ;
   int fe_isp_y_start     ;
   int fe_isp_y_end       ;
   int fe_mem_ping_addr   ;
   int fe_mem_pong_addr   ;
   int fe_mem_other_addr  ;
   int fe_mem_line_stride ;
   int fe_mem_line_minbyte;
   int fe_int_mask        ;
} Fe_param;

typedef struct {
   int rd_sel             ;//add for sel 7 mipi_isp_top
   int rd_work_mode       ;
   int rd_mem_ping_addr   ;
   int rd_mem_pong_addr   ;
   int rd_mem_line_stride ;
   int rd_mem_line_size   ;
   int rd_mem_line_number ;
} Rd_param;

typedef struct {
   int pixel_sel             ;//add for sel 7 mipi_isp_top
   int pixel_work_mode       ;
   int pixel_data_type       ;
   int pixel_isp_x_start     ;
   int pixel_isp_x_end       ;
   int pixel_line_size       ;
   int pixel_pixel_number    ;
} Pixel_param;

typedef struct {
   int alig_sel             ;//add for sel 7 mipi_isp_top
   int alig_work_mode       ;
   int alig_hsize           ;
   int alig_vsize           ;
} Alig_param;

typedef struct {
   int dma_src_addr          ;
   int dma_dst_addr       ;
   int dma_type           ; //1 write 0 read
   int dma_len            ;
} Dma_Param;

struct adapter_dev_param {
	int path;
	int mode;
	u32 width;
	u32 height;
	int format;
	u32 hsize_mipi;
	u32 pixel_bit;
	int dol_type;
	Fe_param fe_param;
	Rd_param rd_param;
	Pixel_param pixel_param;
	Alig_param alig_param;
	struct adap_exp_offset offset;

	struct aml_buffer ddr_buf[2];
	struct aml_buffer rsvd_buf;
	struct aml_buffer *cur_buf;
	struct aml_buffer *done_buf;
	struct list_head free_list;
	struct spinlock ddr_lock;
};

struct adapter_global_info {
	int mode;
	int devno;
	u32 task_status;
	u32 user;
	struct task_struct *task;
	struct aml_buffer *done_buf;
	struct completion g_cmpt;
	struct completion complete;
	spinlock_t list_lock;
	struct list_head done_list;
};

struct adapter_dev_ops {
	int (*hw_init)(void *a_dev);
	void (*hw_reset)(void *a_dev);
	int (*hw_start)(void *a_dev);
	void (*hw_stop)(void *a_dev);
	int (*hw_irq_handler)(void *a_dev);
	int (*hw_fe_set_fmt)(struct aml_video *video, struct aml_format *fmt);
	int (*hw_fe_cfg_buf)(struct aml_video *video, struct aml_buffer *buff);
	int (*hw_rd_set_fmt)(struct aml_video *video, struct aml_format *fmt);
	int (*hw_rd_cfg_buf)(struct aml_video *video, struct aml_buffer *buff);
	void (*hw_fe_enable)(struct aml_video *video);
	void (*hw_fe_disable)(struct aml_video *video);
	void (*hw_rd_enable)(struct aml_video *video);
	void (*hw_rd_disable)(struct aml_video *video);
	int (*hw_interrupt_status)(void *a_dev);
	u64 (*hw_timestamp)(void *a_dev);
	int (*hw_wdr_cfg_buf)(void *a_dev);
	void (*hw_irq_en)(void *a_dev);
	void (*hw_irq_dis)(void *a_dev);
	void (*hw_offline_mode)(void *a_dev);
};

struct adapter_dev_t {
	u32 index;
	u32 version;
	char *bus_info;
	struct device *dev;
	struct platform_device *pdev;

	struct clk *wrmif_clk;
    struct clk *adap_clk;
    struct clk *vapb_clk;
	int irq;
	spinlock_t irq_lock;
	struct tasklet_struct irq_tasklet;
	void __iomem *adap;
	void __iomem *wrmif;
	u32 enWDRMode;
	u32 wstatus;

	struct v4l2_subdev sd;
	struct media_pad pads[AML_ADAP_PAD_MAX];
	struct v4l2_mbus_framefmt pfmt[AML_ADAP_PAD_MAX];
	unsigned int fmt_cnt;
	const struct aml_format *formats;
	struct v4l2_device *v4l2_dev;
	struct media_pipeline pipe;
	struct aml_subdev subdev;
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_ctrl *wdr;

	struct adapter_dev_param param;
	const struct adapter_dev_ops *ops;

	struct aml_video video[AML_ADAP_STREAM_MAX];
};

int aml_adap_subdev_init(void *c_dev);
void aml_adap_subdev_deinit(void *c_dev);
int aml_adap_video_register(struct adapter_dev_t *adap_dev);
void aml_adap_video_unregister(struct adapter_dev_t *adap_dev);
int aml_adap_subdev_register(struct adapter_dev_t *adap_dev);
void aml_adap_subdev_unregister(struct adapter_dev_t *adap_dev);

extern const struct adapter_dev_ops adap_dev_hw_ops;

struct adapter_dev_t *adap_get_dev(int index);
int write_data_to_buf(char *buf, int size);

struct adapter_global_info *aml_adap_global_get_info(void);
int aml_adap_global_init(void);
int aml_adap_global_create_thread(void);
int aml_adap_global_done_completion(void);
void aml_adap_global_destroy_thread(void);
void aml_adap_global_mode(int mode);
void aml_adap_global_devno(int devno);
int aml_adap_global_get_vdev(void);

#endif /* __AML_ADAPTER_H__ */
