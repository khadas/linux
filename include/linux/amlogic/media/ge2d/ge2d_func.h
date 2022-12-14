/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _GE2D_FUNC_H_
#define _GE2D_FUNC_H_

#define BLENDOP_ADD           0    /* Cd = Cs*Fs+Cd*Fd */
#define BLENDOP_SUB           1    /* Cd = Cs*Fs-Cd*Fd */
#define BLENDOP_REVERSE_SUB   2    /* Cd = Cd*Fd-Cs*Fs */
#define BLENDOP_MIN           3    /* Cd = Min(Cd*Fd,Cs*Fs) */
#define BLENDOP_MAX           4    /* Cd = Max(Cd*Fd,Cs*Fs) */
#define BLENDOP_LOGIC         5    /* Cd = Cs op Cd */
#define BLENDOP_LOGIC_CLEAR       (BLENDOP_LOGIC + 0)
#define BLENDOP_LOGIC_COPY        (BLENDOP_LOGIC + 1)
#define BLENDOP_LOGIC_NOOP        (BLENDOP_LOGIC + 2)
#define BLENDOP_LOGIC_SET         (BLENDOP_LOGIC + 3)
#define BLENDOP_LOGIC_COPY_INVERT (BLENDOP_LOGIC + 4)
#define BLENDOP_LOGIC_INVERT      (BLENDOP_LOGIC + 5)
#define BLENDOP_LOGIC_AND_REVERSE (BLENDOP_LOGIC + 6)
#define BLENDOP_LOGIC_OR_REVERSE  (BLENDOP_LOGIC + 7)
#define BLENDOP_LOGIC_AND         (BLENDOP_LOGIC + 8)
#define BLENDOP_LOGIC_OR          (BLENDOP_LOGIC + 9)
#define BLENDOP_LOGIC_NAND        (BLENDOP_LOGIC + 10)
#define BLENDOP_LOGIC_NOR         (BLENDOP_LOGIC + 11)
#define BLENDOP_LOGIC_XOR         (BLENDOP_LOGIC + 12)
#define BLENDOP_LOGIC_EQUIV       (BLENDOP_LOGIC + 13)
#define BLENDOP_LOGIC_AND_INVERT  (BLENDOP_LOGIC + 14)
#define BLENDOP_LOGIC_OR_INVERT   (BLENDOP_LOGIC + 15)

static inline unsigned int blendop(unsigned int color_blending_mode,
				   unsigned int color_blending_src_factor,
				   unsigned int color_blending_dst_factor,
				   unsigned int alpha_blending_mode,
				   unsigned int alpha_blending_src_factor,
				   unsigned int alpha_blending_dst_factor)
{
	return (color_blending_mode << 24) |
	       (color_blending_src_factor << 20) |
	       (color_blending_dst_factor << 16) |
	       (alpha_blending_mode << 8) |
	       (alpha_blending_src_factor << 4) |
	       (alpha_blending_dst_factor << 0);
}

/* GE2D bitblt functions */
void bitblt(struct ge2d_context_s *wq,
	    int src_x, int src_y, int w, int h,
	    int dst_x, int dst_y);

void bitblt_noblk(struct ge2d_context_s *wq,
		  int src_x, int src_y, int w, int h,
		  int dst_x, int dst_y);

void bitblt_noalpha(struct ge2d_context_s *wq,
		    int src_x, int src_y, int w, int h,
		    int dst_x, int dst_y);

void bitblt_noalpha_noblk(struct ge2d_context_s *wq,
			  int src_x, int src_y, int w, int h,
			  int dst_x, int dst_y);
void bitblt_enqueue(struct ge2d_context_s *wq,
		    int src_x, int src_y, int w, int h,
		    int dst_x, int dst_y);
void bitblt_noalpha_enqueue(struct ge2d_context_s *wq,
			    int src_x, int src_y, int w, int h,
			    int dst_x, int dst_y);

/* GE2D stretchblt functions */
void stretchblt(struct ge2d_context_s *wq,
		int src_x, int src_y, int src_w, int src_h,
		int dst_x, int dst_y, int dst_w, int dst_h);

void stretchblt_noblk(struct ge2d_context_s *wq,
		      int src_x, int src_y, int src_w, int src_h,
		      int dst_x, int dst_y, int dst_w, int dst_h);

void stretchblt_noalpha(struct ge2d_context_s *wq,
			int src_x, int src_y, int src_w, int src_h,
			int dst_x, int dst_y, int dst_w, int dst_h);

void stretchblt_noalpha_noblk(struct ge2d_context_s *wq,
			      int src_x, int src_y, int src_w, int src_h,
			      int dst_x, int dst_y, int dst_w, int dst_h);
void stretchblt_enqueue(struct ge2d_context_s *wq,
			int src_x, int src_y, int src_w, int src_h,
			int dst_x, int dst_y, int dst_w, int dst_h);

void stretchblt_noalpha_enqueue(struct ge2d_context_s *wq,
				int src_x, int src_y, int src_w, int src_h,
				int dst_x, int dst_y, int dst_w, int dst_h);

/* GE2D fillrect functions */
void fillrect(struct ge2d_context_s *wq,
	      int x, int y, int w, int h, unsigned int color);

void fillrect_noblk(struct ge2d_context_s *wq,
		    int x, int y, int w, int h, unsigned int color);
void fillrect_enqueue(struct ge2d_context_s *wq,
		      int x, int y, int w, int h, unsigned int color);

/* GE2D blend functions */
void blend(struct ge2d_context_s *wq,
	   int src_x, int src_y, int src_w, int src_h,
	   int src2_x, int src2_y, int src2_w, int src2_h,
	   int dst_x, int dst_y, int dst_w, int dst_h,
	   int op);

void blend_noblk(struct ge2d_context_s *wq,
		 int src_x, int src_y, int src_w, int src_h,
		 int src2_x, int src2_y, int src2_w, int src2_h,
		 int dst_x, int dst_y, int dst_w, int dst_h,
		 int op, int enqueue);

void blend_noalpha(struct ge2d_context_s *wq,
		   int src_x, int src_y, int src_w, int src_h,
		   int src2_x, int src2_y, int src2_w, int src2_h,
		   int dst_x, int dst_y, int dst_w, int dst_h,
		   int op);

void blend_noalpha_noblk(struct ge2d_context_s *wq,
			 int src_x, int src_y, int src_w, int src_h,
			 int src2_x, int src2_y, int src2_w, int src2_h,
			 int dst_x, int dst_y, int dst_w, int dst_h,
			 int op, int enqueue);
void blend_enqueue(struct ge2d_context_s *wq,
		   int src_x, int src_y, int src_w, int src_h,
		   int src2_x, int src2_y, int src2_w, int src2_h,
		   int dst_x, int dst_y, int dst_w, int dst_h,
		   int op);
void blend_noalpha_enqueue(struct ge2d_context_s *wq,
			   int src_x, int src_y, int src_w, int src_h,
			   int src2_x, int src2_y, int src2_w, int src2_h,
			   int dst_x, int dst_y, int dst_w, int dst_h,
			   int op);
int post_queue_to_process(struct ge2d_context_s *wq, int block);

#endif
