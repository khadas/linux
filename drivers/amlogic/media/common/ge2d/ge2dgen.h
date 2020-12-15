/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _GE2DGEN_H_
#define _GE2DGEN_H_

void ge2dgen_src(struct ge2d_context_s *wq,
		 unsigned int canvas_addr,
		unsigned int format,
		unsigned long *phy_addr,
		unsigned int *stride);

void ge2dgen_post_release_src1buf(struct ge2d_context_s *wq,
				  unsigned int buffer);

void ge2dgen_post_release_src1canvas(struct ge2d_context_s *wq);

void ge2dgen_post_release_src2buf(struct ge2d_context_s *wq,
				  unsigned int buffer);

void ge2dgen_post_release_src2canvas(struct ge2d_context_s *wq);

void ge2dgen_src2(struct ge2d_context_s *wq,
		  unsigned int canvas_addr,
		unsigned int format,
		unsigned long *phy_addr,
		unsigned int *stride);

void ge2dgen_src2_clip(struct ge2d_context_s *wq,
		       int x, int y, int w, int h);
void ge2dgen_antiflicker(struct ge2d_context_s *wq, unsigned long enable);
void ge2dgen_rendering_dir(struct ge2d_context_s *wq,
			   int src1_xrev,
			   int src1_yrev,
			   int dst_xrev,
			   int dst_yrev,
			   int dst_xy_swap);

void ge2dgen_dst(struct ge2d_context_s *wq,
		 unsigned int canvas_addr,
		unsigned int format,
		unsigned long *phy_addr,
		unsigned int *stride);

void ge2dgen_src_clip(struct ge2d_context_s *wq,
		      int x, int y, int w, int h);

void ge2dgen_src_key(struct ge2d_context_s *wq,
		     int en, int key, int keymask, int keymode);

void ge2dgent_src_gbalpha(struct ge2d_context_s *wq,
			  unsigned char alpha1, unsigned char alpha2);

void ge2dgen_src_color(struct ge2d_context_s *wq,
		       unsigned int color);

void ge2dgent_rendering_dir(struct ge2d_context_s *wq,
			    int src_x_dir, int src_y_dir,
			    int dst_x_dir, int dst_y_dir);

void ge2dgen_dst_clip(struct ge2d_context_s *wq,
		      int x, int y, int w, int h, int mode);

void ge2dgent_src2_clip(struct ge2d_context_s *wq,
			int x, int y, int w, int h);

void ge2dgen_cb(struct ge2d_context_s *wq,
		int (*cmd_cb)(unsigned int), unsigned int param);

void ge2dgen_const_color(struct ge2d_context_s *wq,
			 unsigned int color);
void ge2dgen_disable_matrix(struct ge2d_context_s *wq);
#endif

