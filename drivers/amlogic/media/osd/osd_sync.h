/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _OSD_SYNC_H_
#define _OSD_SYNC_H_

enum {
	GLES_COMPOSE_MODE = 0,
	DIRECT_COMPOSE_MODE = 1,
	GE2D_COMPOSE_MODE = 2,
};

#define FB_SYNC_REQUEST_MAGIC            0x54376812
#define FB_SYNC_REQUEST_RENDER_MAGIC_V1  0x55386816
#define FB_SYNC_REQUEST_RENDER_MAGIC_V2  0x55386817

#define AFBC_EN                BIT(31)
#define TILED_HEADER_EN        BIT(18)
#define SUPER_BLOCK_ASPECT     BIT(16)
#define BLOCK_SPLIT            BIT(9)
#define YUV_TRANSFORM          BIT(8)

/* Blend modes, settable per layer */
enum {
	BLEND_MODE_INVALID = 0,
	/* colorOut = colorSrc */
	BLEND_MODE_NONE = 1,
	/* colorOut = colorSrc + colorDst * (1 - alphaSrc) */
	BLEND_MODE_PREMULTIPLIED = 2,
	/* colorOut = colorSrc * alphaSrc + colorDst * (1 - alphaSrc) */
	BLEND_MODE_COVERAGE = 3,
};

struct sync_req_old_s {
	unsigned int xoffset;
	unsigned int yoffset;
	int in_fen_fd;
	int out_fen_fd;
};

struct sync_req_s {
	int magic;
	int len;
	unsigned int xoffset;
	unsigned int yoffset;
	int in_fen_fd;
	int out_fen_fd;
	int format;
	int reserved[3];
};

struct sync_req_render_s {
	int magic;
	int len;
	unsigned int    xoffset;
	unsigned int    yoffset;
	int             in_fen_fd;
	int             out_fen_fd;
	int             width;
	int             height;
	int             format;
	int             shared_fd;
	unsigned int    op;
	unsigned int    type; /*direct render or ge2d*/
	unsigned int    dst_x;
	unsigned int    dst_y;
	unsigned int    dst_w;
	unsigned int    dst_h;
	int				byte_stride;
	int				pxiel_stride;
	int  afbc_inter_format;
	unsigned int  zorder;
	unsigned int  blend_mode;
	unsigned char  plane_alpha;
	unsigned char  dim_layer;
	unsigned int  dim_color;
	int  fb_width;
	int  fb_height;
	int  secure_enable;
};

struct fb_sync_request_s {
	union {
		struct sync_req_old_s sync_req_old;
		struct sync_req_s sync_req;
		struct sync_req_render_s sync_req_render;
	};
};

struct display_flip_info_s {
	unsigned int  background_w;
	unsigned int  background_h;
	unsigned int  fullscreen_w;
	unsigned int  fullscreen_h;
	unsigned int  position_x;
	unsigned int  position_y;
	unsigned int  position_w;
	unsigned int  position_h;
};

struct do_hwc_cmd_s {
	int out_fen_fd;
	unsigned char hdr_mode;
	struct display_flip_info_s disp_info;
};
#endif
