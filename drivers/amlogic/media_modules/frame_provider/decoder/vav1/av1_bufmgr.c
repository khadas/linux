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
 */
#ifndef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#else
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/amlogic/media/canvas/canvas.h>

#undef pr_info
#define pr_info pr_cont

#define __COMPARE(context, p1, p2) comp(p1, p2)
#define __SHORTSORT(lo, hi, width, comp, context) \
  shortsort(lo, hi, width, comp)
#define CUTOFF 8            /* testing shows that this is good value */
#define STKSIZ (8*sizeof(void *) - 2)

#undef swap
static void swap(char *a, char *b, size_t width)
{
	char tmp;

	if (a != b)
	/* Do the swap one character at a time to avoid potential
	*   alignment problems.
	*/
	while (width--) {
		tmp = *a;
		*a++ = *b;
		*b++ = tmp;
	}
}

static void shortsort(char *lo, char *hi, size_t width,
  int (*comp)(const void *, const void *))
{
	char *p, *max;

	/* Note: in assertions below, i and j are alway inside original
	*   bound of array to sort.
	*/
	while (hi > lo) {
		/* A[i] <= A[j] for i <= j, j > hi */
		max = lo;
		for (p = lo + width; p <= hi; p += width) {
			/* A[i] <= A[max] for lo <= i < p */
			if (__COMPARE(context, p, max) > 0)
				max = p;
				/* A[i] <= A[max] for lo <= i <= p */
		}
		/* A[i] <= A[max] for lo <= i <= hi */
		swap(max, hi, width);

		/* A[i] <= A[hi] for i <= hi, so A[i] <= A[j] for i <= j,
		*   j >= hi
		*/
		hi -= width;

		/* A[i] <= A[j] for i <= j, j > hi, loop top condition
		*   established
		*/
	}
}

static void qsort(void *base, size_t num, size_t width,
  int (*comp)(const void *, const void *))
{
  char *lo, *hi;              /* ends of sub-array currently sorting */
  char *mid;                  /* points to middle of subarray */
  char *loguy, *higuy;        /* traveling pointers for partition step */
  size_t size;                /* size of the sub-array */
  char *lostk[STKSIZ], *histk[STKSIZ];
  int stkptr;

/*  stack for saving sub-array to be
 *          processed
 */
#if 0
  /* validation section */
  _VALIDATE_RETURN_VOID(base != NULL || num == 0, EINVAL);
  _VALIDATE_RETURN_VOID(width > 0, EINVAL);
  _VALIDATE_RETURN_VOID(comp != NULL, EINVAL);
#endif
  if (num < 2)
    return;                 /* nothing to do */

  stkptr = 0;                 /* initialize stack */
  lo = (char *)base;
  hi = (char *)base + width * (num - 1);      /* initialize limits */

  /* this entry point is for pseudo-recursion calling: setting
   * lo and hi and jumping to here is like recursion, but stkptr is
   * preserved, locals aren't, so we preserve stuff on the stack
   */
recurse:

  size = (hi - lo) / width + 1;        /* number of el's to sort */

  /* below a certain size, it is faster to use a O(n^2) sorting method */
  if (size <= CUTOFF) {
    __SHORTSORT(lo, hi, width, comp, context);
  } else {
    /* First we pick a partitioning element.  The efficiency of
     * the algorithm demands that we find one that is approximately
     * the median of the values, but also that we select one fast.
     * We choose the median of the first, middle, and last
     * elements, to avoid bad performance in the face of already
     * sorted data, or data that is made up of multiple sorted
     * runs appended together.  Testing shows that a
     * median-of-three algorithm provides better performance than
     * simply picking the middle element for the latter case.
     */

    mid = lo + (size / 2) * width;      /* find middle element */

    /* Sort the first, middle, last elements into order */
    if (__COMPARE(context, lo, mid) > 0)
      swap(lo, mid, width);
    if (__COMPARE(context, lo, hi) > 0)
      swap(lo, hi, width);
    if (__COMPARE(context, mid, hi) > 0)
      swap(mid, hi, width);

    /* We now wish to partition the array into three pieces, one
     * consisting of elements <= partition element, one of elements
     * equal to the partition element, and one of elements > than
     * it. This is done below; comments indicate conditions
     * established at every step.
     */

    loguy = lo;
    higuy = hi;

    /* Note that higuy decreases and loguy increases on every
     *   iteration, so loop must terminate.
     */
    for (;;) {
      /* lo <= loguy < hi, lo < higuy <= hi,
       *   A[i] <= A[mid] for lo <= i <= loguy,
       *   A[i] > A[mid] for higuy <= i < hi,
       *   A[hi] >= A[mid]
       */

      /* The doubled loop is to avoid calling comp(mid,mid),
       *   since some existing comparison funcs don't work
       *   when passed the same value for both pointers.
       */

      if (mid > loguy) {
        do  {
          loguy += width;
        } while (loguy < mid &&
          __COMPARE(context, loguy, mid) <= 0);
      }
      if (mid <= loguy) {
        do  {
          loguy += width;
        } while (loguy <= hi &&
          __COMPARE(context, loguy, mid) <= 0);
      }

      /* lo < loguy <= hi+1, A[i] <= A[mid] for
       *   lo <= i < loguy,
       *   either loguy > hi or A[loguy] > A[mid]
       */

      do  {
        higuy -= width;
      } while (higuy > mid &&
          __COMPARE(context, higuy, mid) > 0);

      /* lo <= higuy < hi, A[i] > A[mid] for higuy < i < hi,
       *   either higuy == lo or A[higuy] <= A[mid]
       */

      if (higuy < loguy)
        break;

      /* if loguy > hi or higuy == lo, then we would have
       *   exited, so A[loguy] > A[mid], A[higuy] <= A[mid],
       *   loguy <= hi, higuy > lo
       */

      swap(loguy, higuy, width);

      /* If the partition element was moved, follow it.
       *   Only need to check for mid == higuy, since before
       *   the swap, A[loguy] > A[mid] implies loguy != mid.
       */

      if (mid == higuy)
        mid = loguy;

      /* A[loguy] <= A[mid], A[higuy] > A[mid]; so condition
       *   at top of loop is re-established
       */
    }

    /*     A[i] <= A[mid] for lo <= i < loguy,
     *       A[i] > A[mid] for higuy < i < hi,
     *       A[hi] >= A[mid]
     *       higuy < loguy
     *   implying:
     *       higuy == loguy-1
     *       or higuy == hi - 1, loguy == hi + 1, A[hi] == A[mid]
     */

    /* Find adjacent elements equal to the partition element.  The
     *   doubled loop is to avoid calling comp(mid,mid), since some
     *   existing comparison funcs don't work when passed the same
     *   value for both pointers.
     */

    higuy += width;
    if (mid < higuy) {
      do  {
        higuy -= width;
      } while (higuy > mid &&
        __COMPARE(context, higuy, mid) == 0);
    }
    if (mid >= higuy) {
      do  {
        higuy -= width;
      } while (higuy > lo &&
        __COMPARE(context, higuy, mid) == 0);
    }

    /* OK, now we have the following:
     *      higuy < loguy
     *      lo <= higuy <= hi
     *      A[i]  <= A[mid] for lo <= i <= higuy
     *      A[i]  == A[mid] for higuy < i < loguy
     *      A[i]  >  A[mid] for loguy <= i < hi
     *      A[hi] >= A[mid]
     */

    /* We've finished the partition, now we want to sort the
     *   subarrays [lo, higuy] and [loguy, hi].
     *   We do the smaller one first to minimize stack usage.
     *   We only sort arrays of length 2 or more.
     */

    if (higuy - lo >= hi - loguy) {
      if (lo < higuy) {
        lostk[stkptr] = lo;
        histk[stkptr] = higuy;
        ++stkptr;
      }                    /* save big recursion for later */

      if (loguy < hi) {
        lo = loguy;
        goto recurse;          /* do small recursion */
      }
    } else {
      if (loguy < hi) {
        lostk[stkptr] = loguy;
        histk[stkptr] = hi;
        ++stkptr;    /* save big recursion for later */
      }

      if (lo < higuy) {
        hi = higuy;
        goto recurse;          /* do small recursion */
      }
    }
  }

  /* We have sorted the array, except for any pending sorts on the stack.
   *   Check if there are any, and do them.
   */

  --stkptr;
  if (stkptr >= 0) {
    lo = lostk[stkptr];
    hi = histk[stkptr];
    goto recurse;           /* pop subarray from stack */
  } else
    return;                 /* all subarrays done */
}

#endif

#include "av1_global.h"
int aom_realloc_frame_buffer(AV1_COMMON *cm, PIC_BUFFER_CONFIG *pic,
  int width, int height, unsigned int order_hint);
void dump_params(AV1Decoder *pbi, union param_u *params);

#define assert(a)
#define IMPLIES(a)

int new_compressed_data_count = 0;

static int valid_ref_frame_size(int ref_width, int ref_height,
                                       int this_width, int this_height) {
  return 2 * this_width >= ref_width && 2 * this_height >= ref_height &&
         this_width <= 16 * ref_width && this_height <= 16 * ref_height;
}

#ifdef SUPPORT_SCALE_FACTOR
// Note: Expect val to be in q4 precision
static inline int scaled_x(int val, const struct scale_factors *sf) {
  const int off =
      (sf->x_scale_fp - (1 << REF_SCALE_SHIFT)) * (1 << (SUBPEL_BITS - 1));
  const int64_t tval = (int64_t)val * sf->x_scale_fp + off;
  return (int)ROUND_POWER_OF_TWO_SIGNED_64(tval,
                                           REF_SCALE_SHIFT - SCALE_EXTRA_BITS);
}

// Note: Expect val to be in q4 precision
static inline int scaled_y(int val, const struct scale_factors *sf) {
  const int off =
      (sf->y_scale_fp - (1 << REF_SCALE_SHIFT)) * (1 << (SUBPEL_BITS - 1));
  const int64_t tval = (int64_t)val * sf->y_scale_fp + off;
  return (int)ROUND_POWER_OF_TWO_SIGNED_64(tval,
                                           REF_SCALE_SHIFT - SCALE_EXTRA_BITS);
}

// Note: Expect val to be in q4 precision
static int unscaled_value(int val, const struct scale_factors *sf) {
  (void)sf;
  return val << SCALE_EXTRA_BITS;
}

static int get_fixed_point_scale_factor(int other_size, int this_size) {
  // Calculate scaling factor once for each reference frame
  // and use fixed point scaling factors in decoding and encoding routines.
  // Hardware implementations can calculate scale factor in device driver
  // and use multiplication and shifting on hardware instead of division.
  return ((other_size << REF_SCALE_SHIFT) + this_size / 2) / this_size;
}

// Given the fixed point scale, calculate coarse point scale.
static int fixed_point_scale_to_coarse_point_scale(int scale_fp) {
  return ROUND_POWER_OF_TWO(scale_fp, REF_SCALE_SHIFT - SCALE_SUBPEL_BITS);
}


void av1_setup_scale_factors_for_frame(struct scale_factors *sf, int other_w,
                                       int other_h, int this_w, int this_h) {
  if (!valid_ref_frame_size(other_w, other_h, this_w, this_h)) {
    sf->x_scale_fp = REF_INVALID_SCALE;
    sf->y_scale_fp = REF_INVALID_SCALE;
    return;
  }

  sf->x_scale_fp = get_fixed_point_scale_factor(other_w, this_w);
  sf->y_scale_fp = get_fixed_point_scale_factor(other_h, this_h);

  sf->x_step_q4 = fixed_point_scale_to_coarse_point_scale(sf->x_scale_fp);
  sf->y_step_q4 = fixed_point_scale_to_coarse_point_scale(sf->y_scale_fp);

  if (av1_is_scaled(sf)) {
    sf->scale_value_x = scaled_x;
    sf->scale_value_y = scaled_y;
  } else {
    sf->scale_value_x = unscaled_value;
    sf->scale_value_y = unscaled_value;
  }
#ifdef ORI_CODE
  // AV1 convolve functions
  // Special case convolve functions should produce the same result as
  // av1_convolve_2d.
  // subpel_x_qn == 0 && subpel_y_qn == 0
  sf->convolve[0][0][0] = av1_convolve_2d_copy_sr;
  // subpel_x_qn == 0
  sf->convolve[0][1][0] = av1_convolve_y_sr;
  // subpel_y_qn == 0
  sf->convolve[1][0][0] = av1_convolve_x_sr;
  // subpel_x_qn != 0 && subpel_y_qn != 0
  sf->convolve[1][1][0] = av1_convolve_2d_sr;
  // subpel_x_qn == 0 && subpel_y_qn == 0
  sf->convolve[0][0][1] = av1_dist_wtd_convolve_2d_copy;
  // subpel_x_qn == 0
  sf->convolve[0][1][1] = av1_dist_wtd_convolve_y;
  // subpel_y_qn == 0
  sf->convolve[1][0][1] = av1_dist_wtd_convolve_x;
  // subpel_x_qn != 0 && subpel_y_qn != 0
  sf->convolve[1][1][1] = av1_dist_wtd_convolve_2d;
  // AV1 High BD convolve functions
  // Special case convolve functions should produce the same result as
  // av1_highbd_convolve_2d.
  // subpel_x_qn == 0 && subpel_y_qn == 0
  sf->highbd_convolve[0][0][0] = av1_highbd_convolve_2d_copy_sr;
  // subpel_x_qn == 0
  sf->highbd_convolve[0][1][0] = av1_highbd_convolve_y_sr;
  // subpel_y_qn == 0
  sf->highbd_convolve[1][0][0] = av1_highbd_convolve_x_sr;
  // subpel_x_qn != 0 && subpel_y_qn != 0
  sf->highbd_convolve[1][1][0] = av1_highbd_convolve_2d_sr;
  // subpel_x_qn == 0 && subpel_y_qn == 0
  sf->highbd_convolve[0][0][1] = av1_highbd_dist_wtd_convolve_2d_copy;
  // subpel_x_qn == 0
  sf->highbd_convolve[0][1][1] = av1_highbd_dist_wtd_convolve_y;
  // subpel_y_qn == 0
  sf->highbd_convolve[1][0][1] = av1_highbd_dist_wtd_convolve_x;
  // subpel_x_qn != 0 && subpel_y_qn != 0
  sf->highbd_convolve[1][1][1] = av1_highbd_dist_wtd_convolve_2d;
#endif
}
#endif

static RefCntBuffer *assign_cur_frame_new_fb(AV1_COMMON *const cm) {
  // Release the previously-used frame-buffer
  int new_fb_idx;
  if (cm->cur_frame != NULL) {
    --cm->cur_frame->ref_count;
    cm->cur_frame = NULL;
  }

  // Assign a new framebuffer
  new_fb_idx = get_free_frame_buffer(cm);
  if (new_fb_idx == INVALID_IDX) return NULL;

  cm->cur_frame = &cm->buffer_pool->frame_bufs[new_fb_idx];
  cm->cur_frame->buf.buf_8bit_valid = 0;
#ifdef AML
  cm->cur_frame->buf.index = new_fb_idx;
#endif
#ifdef ORI_CODE
  av1_zero(cm->cur_frame->interp_filter_selected);
#endif
  return cm->cur_frame;
}

// Modify 'lhs_ptr' to reference the buffer at 'rhs_ptr', and update the ref
// counts accordingly.
static void assign_frame_buffer_p(RefCntBuffer **lhs_ptr,
                                       RefCntBuffer *rhs_ptr) {
  RefCntBuffer *const old_ptr = *lhs_ptr;
  if (old_ptr != NULL) {
    assert(old_ptr->ref_count > 0);
    // One less reference to the buffer at 'old_ptr', so decrease ref count.
    --old_ptr->ref_count;
  }

  *lhs_ptr = rhs_ptr;
  // One more reference to the buffer at 'rhs_ptr', so increase ref count.
  ++rhs_ptr->ref_count;
}

AV1Decoder *av1_decoder_create(BufferPool *const pool, AV1_COMMON *cm) {
  int i;

#ifndef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
  AV1Decoder *pbi = (AV1Decoder *)malloc(sizeof(*pbi));
#else
  AV1Decoder *pbi = (AV1Decoder *)vmalloc(sizeof(AV1Decoder));
#endif
  if (!pbi) return NULL;
  memset(pbi, 0, sizeof(*pbi));

  // The jmp_buf is valid only for the duration of the function that calls
  // setjmp(). Therefore, this function must reset the 'setjmp' field to 0
  // before it returns.

  pbi->common = cm;
  cm->error.setjmp = 1;

#ifdef ORI_CODE
  memset(cm->fc, 0, sizeof(*cm->fc));
  memset(cm->default_frame_context, 0, sizeof(*cm->default_frame_context));
#endif
  pbi->need_resync = 1;

  // Initialize the references to not point to any frame buffers.
  for (i = 0; i < REF_FRAMES; i++) {
    cm->ref_frame_map[i] = NULL;
    cm->next_ref_frame_map[i] = NULL;
#ifdef AML
    cm->next_used_ref_frame_map[i] = NULL;
#endif
  }

  cm->current_frame.frame_number = 0;
  pbi->decoding_first_frame = 1;
  pbi->common->buffer_pool = pool;

  cm->seq_params.bit_depth = AOM_BITS_8;

#ifdef ORI_CODE
  cm->alloc_mi = dec_alloc_mi;
  cm->free_mi = dec_free_mi;
  cm->setup_mi = dec_setup_mi;

  av1_loop_filter_init(cm);

  av1_qm_init(cm);
  av1_loop_restoration_precal();
#if CONFIG_ACCOUNTING
  pbi->acct_enabled = 1;
  aom_accounting_init(&pbi->accounting);
#endif
#endif
  cm->error.setjmp = 0;

#ifdef ORI_CODE
  aom_get_worker_interface()->init(&pbi->lf_worker);
  pbi->lf_worker.thread_name = "aom lf worker";
#endif

  return pbi;
}

static void reset_frame_buffers(AV1Decoder *const pbi);

void av1_bufmgr_ctx_reset(AV1Decoder *pbi, BufferPool *const pool, AV1_COMMON *cm)
{
	u32 save_w, save_h;

	if (!pbi || !pool || !cm)
		return;

	reset_frame_buffers(pbi);
	memset(pbi, 0, sizeof(*pbi));
	/*save w,h for resolution change after seek */
	save_w = cm->width;
	save_h = cm->height;
	memset(cm, 0, sizeof(*cm));

	cm->current_frame.frame_number	= 0;
	cm->seq_params.bit_depth	= AOM_BITS_8;
	cm->error.setjmp = 0;
	cm->width = save_w;
	cm->height = save_h;

	pbi->bufmgr_proc_count		= 0;
	pbi->need_resync		= 1;
	pbi->decoding_first_frame	= 1;
	pbi->num_output_frames		= 0;
	pbi->common			= cm;
	pbi->common->buffer_pool	= pool;
}

int release_fb_cb(void *cb_priv, aom_codec_frame_buffer_t *fb) {
#if 0
  InternalFrameBuffer *const int_fb = (InternalFrameBuffer *)fb->priv;
  (void)cb_priv;
  if (int_fb) int_fb->in_use = 0;
#endif
  return 0;
}

static void decrease_ref_count(AV1Decoder *pbi, RefCntBuffer *const buf,
                                      BufferPool *const pool) {
  if (buf != NULL) {
    --buf->ref_count;
    // Reference counts should never become negative. If this assertion fails,
    // there is a bug in our reference count management.
    assert(buf->ref_count >= 0);
    // A worker may only get a free framebuffer index when calling get_free_fb.
    // But the raw frame buffer is not set up until we finish decoding header.
    // So if any error happens during decoding header, frame_bufs[idx] will not
    // have a valid raw frame buffer.
    if (buf->ref_count == 0
#ifdef ORI_CODE
     && buf->raw_frame_buffer.data
#endif
     ) {
#ifdef AML
      av1_release_buf(pbi, buf);
#endif
      release_fb_cb(pool->cb_priv, &buf->raw_frame_buffer);
      buf->raw_frame_buffer.data = NULL;
      buf->raw_frame_buffer.size = 0;
      buf->raw_frame_buffer.priv = NULL;
    }
  }
}

void clear_frame_buf_ref_count(AV1Decoder *pbi)
{
	int i;

	for (i = 0; i < pbi->num_output_frames; i++) {
		decrease_ref_count(pbi, pbi->output_frames[i],
			pbi->common->buffer_pool);
	}
	pbi->num_output_frames = 0;
}

static void swap_frame_buffers(AV1Decoder *pbi, int frame_decoded) {
  int ref_index = 0, mask;
  AV1_COMMON *const cm = pbi->common;
  BufferPool *const pool = cm->buffer_pool;
  unsigned long flags;

  if (frame_decoded) {
    int check_on_show_existing_frame;
    lock_buffer_pool(pool, flags);

    // In ext-tile decoding, the camera frame header is only decoded once. So,
    // we don't release the references here.
    if (!pbi->camera_frame_header_ready) {
      // If we are not holding reference buffers in cm->next_ref_frame_map,
      // assert that the following two for loops are no-ops.
      assert(IMPLIES(!pbi->hold_ref_buf,
                     cm->current_frame.refresh_frame_flags == 0));
      assert(IMPLIES(!pbi->hold_ref_buf,
                     cm->show_existing_frame && !pbi->reset_decoder_state));

      // The following two for loops need to release the reference stored in
      // cm->ref_frame_map[ref_index] before transferring the reference stored
      // in cm->next_ref_frame_map[ref_index] to cm->ref_frame_map[ref_index].
      for (mask = cm->current_frame.refresh_frame_flags; mask; mask >>= 1) {
        decrease_ref_count(pbi, cm->ref_frame_map[ref_index], pool);
        cm->ref_frame_map[ref_index] = cm->next_ref_frame_map[ref_index];
        cm->next_ref_frame_map[ref_index] = NULL;
        ++ref_index;
      }

      check_on_show_existing_frame =
          !cm->show_existing_frame || pbi->reset_decoder_state;
      for (; ref_index < REF_FRAMES && check_on_show_existing_frame;
           ++ref_index) {
        decrease_ref_count(pbi, cm->ref_frame_map[ref_index], pool);
        cm->ref_frame_map[ref_index] = cm->next_ref_frame_map[ref_index];
        cm->next_ref_frame_map[ref_index] = NULL;
      }
    }

    if (cm->show_existing_frame || cm->show_frame) {
      if (pbi->output_all_layers) {
        // Append this frame to the output queue
        if (pbi->num_output_frames >= MAX_NUM_SPATIAL_LAYERS) {
          // We can't store the new frame anywhere, so drop it and return an
          // error
          cm->cur_frame->buf.corrupted = 1;
          decrease_ref_count(pbi, cm->cur_frame, pool);
          cm->error.error_code = AOM_CODEC_UNSUP_BITSTREAM;
        } else {
          pbi->output_frames[pbi->num_output_frames] = cm->cur_frame;
          pbi->num_output_frames++;
        }
      } else {
        // Replace any existing output frame
        assert(pbi->num_output_frames == 0 || pbi->num_output_frames == 1);
        if (pbi->num_output_frames > 0) {
          decrease_ref_count(pbi, pbi->output_frames[0], pool);
        }
	if (cm->cur_frame) {
          pbi->output_frames[0] = cm->cur_frame;
          pbi->num_output_frames = 1;
	}
      }
    } else {
      decrease_ref_count(pbi, cm->cur_frame, pool);
    }

    unlock_buffer_pool(pool, flags);
  } else {
    // The code here assumes we are not holding reference buffers in
    // cm->next_ref_frame_map. If this assertion fails, we are leaking the
    // frame buffer references in cm->next_ref_frame_map.
    assert(IMPLIES(!pbi->camera_frame_header_ready, !pbi->hold_ref_buf));
    // Nothing was decoded, so just drop this frame buffer
    lock_buffer_pool(pool, flags);
    decrease_ref_count(pbi, cm->cur_frame, pool);
    unlock_buffer_pool(pool, flags);
  }
  cm->cur_frame = NULL;

  if (!pbi->camera_frame_header_ready) {
    pbi->hold_ref_buf = 0;

    // Invalidate these references until the next frame starts.
    for (ref_index = 0; ref_index < INTER_REFS_PER_FRAME; ref_index++) {
      cm->remapped_ref_idx[ref_index] = INVALID_IDX;
    }
  }
}

void aom_internal_error(struct aom_internal_error_info *info,
                        aom_codec_err_t error, const char *fmt, ...) {
  va_list ap;

  info->error_code = error;
  info->has_detail = 0;

  if (fmt) {
    size_t sz = sizeof(info->detail);

    info->has_detail = 1;
    va_start(ap, fmt);
    vsnprintf(info->detail, sz - 1, fmt, ap);
    va_end(ap);
    info->detail[sz - 1] = '\0';
  }
#ifdef ORI_CODE
  if (info->setjmp) longjmp(info->jmp, info->error_code);
#endif
}

#ifdef ORI_CODE
void av1_zero_unused_internal_frame_buffers(InternalFrameBufferList *list) {
  int i;

  assert(list != NULL);

  for (i = 0; i < list->num_internal_frame_buffers; ++i) {
    if (list->int_fb[i].data && !list->int_fb[i].in_use)
      memset(list->int_fb[i].data, 0, list->int_fb[i].size);
  }
}
#endif

// Release the references to the frame buffers in cm->ref_frame_map and reset
// all elements of cm->ref_frame_map to NULL.
static void reset_ref_frame_map(AV1Decoder *const pbi) {
  AV1_COMMON *const cm = pbi->common;
  BufferPool *const pool = cm->buffer_pool;
  int i;

  for (i = 0; i < REF_FRAMES; i++) {
    decrease_ref_count(pbi, cm->ref_frame_map[i], pool);
    cm->ref_frame_map[i] = NULL;
#ifdef AML
    cm->next_used_ref_frame_map[i] = NULL;
#endif
  }
}

// Generate next_ref_frame_map.
static void generate_next_ref_frame_map(AV1Decoder *const pbi) {
  AV1_COMMON *const cm = pbi->common;
  BufferPool *const pool = cm->buffer_pool;
  unsigned long flags;
  int ref_index = 0;
  int mask;

  lock_buffer_pool(pool, flags);
  // cm->next_ref_frame_map holds references to frame buffers. After storing a
  // frame buffer index in cm->next_ref_frame_map, we need to increase the
  // frame buffer's ref_count.
  for (mask = cm->current_frame.refresh_frame_flags; mask; mask >>= 1) {
    if (mask & 1) {
      cm->next_ref_frame_map[ref_index] = cm->cur_frame;
    } else {
      cm->next_ref_frame_map[ref_index] = cm->ref_frame_map[ref_index];
    }
    if (cm->next_ref_frame_map[ref_index] != NULL)
      ++cm->next_ref_frame_map[ref_index]->ref_count;
    ++ref_index;
  }

  for (; ref_index < REF_FRAMES; ++ref_index) {
    cm->next_ref_frame_map[ref_index] = cm->ref_frame_map[ref_index];
    if (cm->next_ref_frame_map[ref_index] != NULL)
      ++cm->next_ref_frame_map[ref_index]->ref_count;
  }
  unlock_buffer_pool(pool, flags);
  pbi->hold_ref_buf = 1;
}

// If the refresh_frame_flags bitmask is set, update reference frame id values
// and mark frames as valid for reference.
static void update_ref_frame_id(AV1_COMMON *const cm, int frame_id) {
  int i;
  int refresh_frame_flags = cm->current_frame.refresh_frame_flags;
  assert(cm->seq_params.frame_id_numbers_present_flag);
  for (i = 0; i < REF_FRAMES; i++) {
    if ((refresh_frame_flags >> i) & 1) {
      cm->ref_frame_id[i] = frame_id;
      cm->valid_for_referencing[i] = 1;
    }
  }
}

static void show_existing_frame_reset(AV1Decoder *const pbi,
                                      int existing_frame_idx) {
  AV1_COMMON *const cm = pbi->common;
  int i;
  assert(cm->show_existing_frame);

  cm->current_frame.frame_type = KEY_FRAME;

  cm->current_frame.refresh_frame_flags = (1 << REF_FRAMES) - 1;

  for (i = 0; i < INTER_REFS_PER_FRAME; ++i) {
    cm->remapped_ref_idx[i] = INVALID_IDX;
  }

  if (pbi->need_resync) {
    reset_ref_frame_map(pbi);
    pbi->need_resync = 0;
  }

  // Note that the displayed frame must be valid for referencing in order to
  // have been selected.
  if (cm->seq_params.frame_id_numbers_present_flag) {
    cm->current_frame_id = cm->ref_frame_id[existing_frame_idx];
    update_ref_frame_id(cm, cm->current_frame_id);
  }

  cm->refresh_frame_context = REFRESH_FRAME_CONTEXT_DISABLED;

  generate_next_ref_frame_map(pbi);

#ifdef ORI_CODE
  // Reload the adapted CDFs from when we originally coded this keyframe
  *cm->fc = cm->next_ref_frame_map[existing_frame_idx]->frame_context;
#endif
}

static void reset_frame_buffers(AV1Decoder *const pbi) {
  AV1_COMMON *const cm = pbi->common;
  RefCntBuffer *const frame_bufs = cm->buffer_pool->frame_bufs;
  int i;
  unsigned long flags;

  // We have not stored any references to frame buffers in
  // cm->next_ref_frame_map, so we can directly reset it to all NULL.
  for (i = 0; i < REF_FRAMES; ++i) {
    cm->next_ref_frame_map[i] = NULL;
  }

  lock_buffer_pool(cm->buffer_pool, flags);
  reset_ref_frame_map(pbi);
  assert(cm->cur_frame->ref_count == 1);
  for (i = 0; i < FRAME_BUFFERS; ++i) {
    // Reset all unreferenced frame buffers. We can also reset cm->cur_frame
    // because we are the sole owner of cm->cur_frame.
    if (frame_bufs[i].ref_count > 0 && &frame_bufs[i] != cm->cur_frame) {
      continue;
    }
    frame_bufs[i].order_hint = 0;
    av1_zero(frame_bufs[i].ref_order_hints);
  }
#ifdef ORI_CODE
  av1_zero_unused_internal_frame_buffers(&cm->buffer_pool->int_frame_buffers);
#endif
  unlock_buffer_pool(cm->buffer_pool, flags);
}

static int frame_is_intra_only(const AV1_COMMON *const cm) {
  return cm->current_frame.frame_type == KEY_FRAME ||
      cm->current_frame.frame_type == INTRA_ONLY_FRAME;
}

static int frame_is_sframe(const AV1_COMMON *cm) {
  return cm->current_frame.frame_type == S_FRAME;
}

// These functions take a reference frame label between LAST_FRAME and
// EXTREF_FRAME inclusive.  Note that this is different to the indexing
// previously used by the frame_refs[] array.
static int get_ref_frame_map_idx(const AV1_COMMON *const cm,
                                        const MV_REFERENCE_FRAME ref_frame) {
  return (ref_frame >= LAST_FRAME && ref_frame <= EXTREF_FRAME)
             ? cm->remapped_ref_idx[ref_frame - LAST_FRAME]
             : INVALID_IDX;
}

static RefCntBuffer *get_ref_frame_buf(
    const AV1_COMMON *const cm, const MV_REFERENCE_FRAME ref_frame) {
  const int map_idx = get_ref_frame_map_idx(cm, ref_frame);
  return (map_idx != INVALID_IDX) ? cm->ref_frame_map[map_idx] : NULL;
}
#ifdef SUPPORT_SCALE_FACTOR
static struct scale_factors *get_ref_scale_factors(
    AV1_COMMON *const cm, const MV_REFERENCE_FRAME ref_frame) {
  const int map_idx = get_ref_frame_map_idx(cm, ref_frame);
  return (map_idx != INVALID_IDX) ? &cm->ref_scale_factors[map_idx] : NULL;
}
#endif
static RefCntBuffer *get_primary_ref_frame_buf(
    const AV1_COMMON *const cm) {
  int map_idx;
  if (cm->primary_ref_frame == PRIMARY_REF_NONE) return NULL;
  map_idx = get_ref_frame_map_idx(cm, cm->primary_ref_frame + 1);
  return (map_idx != INVALID_IDX) ? cm->ref_frame_map[map_idx] : NULL;
}

static int get_relative_dist(const OrderHintInfo *oh, int a, int b) {
  int bits;
  int m;
  int diff;
  if (!oh->enable_order_hint) return 0;

  bits = oh->order_hint_bits_minus_1 + 1;

  assert(bits >= 1);
  assert(a >= 0 && a < (1 << bits));
  assert(b >= 0 && b < (1 << bits));

  diff = a - b;
  m = 1 << (bits - 1);
  diff = (diff & (m - 1)) - (diff & m);
  return diff;
}


void av1_read_frame_size(union param_u *params, int num_bits_width,
                         int num_bits_height, int *width, int *height, int* dec_width) {
  *width = params->p.frame_width;
  *height = params->p.frame_height;//aom_rb_read_literal(rb, num_bits_height) + 1;
#ifdef AML
  *dec_width = params->p.dec_frame_width;
#endif
}

static REFERENCE_MODE read_frame_reference_mode(
    const AV1_COMMON *cm, union param_u *params) {
  if (frame_is_intra_only(cm)) {
    return SINGLE_REFERENCE;
  } else {
    return params->p.reference_mode ? REFERENCE_MODE_SELECT : SINGLE_REFERENCE;
  }
}

static inline int calc_mi_size(int len) {
  // len is in mi units. Align to a multiple of SBs.
  return ALIGN_POWER_OF_TWO(len, MAX_MIB_SIZE_LOG2);
}

void av1_set_mb_mi(AV1_COMMON *cm, int width, int height) {
  // Ensure that the decoded width and height are both multiples of
  // 8 luma pixels (note: this may only be a multiple of 4 chroma pixels if
  // subsampling is used).
  // This simplifies the implementation of various experiments,
  // eg. cdef, which operates on units of 8x8 luma pixels.
  const int aligned_width = ALIGN_POWER_OF_TWO(width, 3);
  const int aligned_height = ALIGN_POWER_OF_TWO(height, 3);
  av1_print2(AV1_DEBUG_BUFMGR_DETAIL, " [PICTURE] av1_set_mb_mi (%d X %d)\n", width, height);

  cm->mi_cols = aligned_width >> MI_SIZE_LOG2;
  cm->mi_rows = aligned_height >> MI_SIZE_LOG2;
  cm->mi_stride = calc_mi_size(cm->mi_cols);

  cm->mb_cols = (cm->mi_cols + 2) >> 2;
  cm->mb_rows = (cm->mi_rows + 2) >> 2;
  cm->MBs = cm->mb_rows * cm->mb_cols;

#if CONFIG_LPF_MASK
  alloc_loop_filter_mask(cm);
#endif
}

int av1_alloc_context_buffers(AV1_COMMON *cm, int width, int height) {
#ifdef ORI_CODE
  int new_mi_size;
#endif
  av1_set_mb_mi(cm, width, height);
#ifdef ORI_CODE
  new_mi_size = cm->mi_stride * calc_mi_size(cm->mi_rows);
  if (cm->mi_alloc_size < new_mi_size) {
    cm->free_mi(cm);
    if (cm->alloc_mi(cm, new_mi_size)) goto fail;
  }
#endif
  return 0;

#ifdef ORI_CODE
fail:
#endif
  // clear the mi_* values to force a realloc on resync
  av1_set_mb_mi(cm, 0, 0);
#ifdef ORI_CODE
  av1_free_context_buffers(cm);
#endif
  return 1;
}

#ifndef USE_SCALED_WIDTH_FROM_UCODE
static void calculate_scaled_size_helper(int *dim, int denom) {
  if (denom != SCALE_NUMERATOR) {
    // We need to ensure the constraint in "Appendix A" of the spec:
    // * FrameWidth is greater than or equal to 16
    // * FrameHeight is greater than or equal to 16
    // For this, we clamp the downscaled dimension to at least 16. One
    // exception: if original dimension itself was < 16, then we keep the
    // downscaled dimension to be same as the original, to ensure that resizing
    // is valid.
    const int min_dim = AOMMIN(16, *dim);
    // Use this version if we need *dim to be even
    // *width = (*width * SCALE_NUMERATOR + denom) / (2 * denom);
    // *width <<= 1;
    *dim = (*dim * SCALE_NUMERATOR + denom / 2) / (denom);
    *dim = AOMMAX(*dim, min_dim);
  }
}
#ifdef ORI_CODE
void av1_calculate_scaled_size(int *width, int *height, int resize_denom) {
  calculate_scaled_size_helper(width, resize_denom);
  calculate_scaled_size_helper(height, resize_denom);
}
#endif
void av1_calculate_scaled_superres_size(int *width, int *height,
                                        int superres_denom) {
  (void)height;
  calculate_scaled_size_helper(width, superres_denom);
}
#endif

static void setup_superres(AV1_COMMON *const cm, union param_u *params,
                           int *width, int *height) {
#ifdef USE_SCALED_WIDTH_FROM_UCODE
  cm->superres_upscaled_width = params->p.frame_width_scaled;
  cm->superres_upscaled_height = params->p.frame_height;


  *width = params->p.dec_frame_width;
  *height = params->p.frame_height;
  av1_print2(AV1_DEBUG_BUFMGR_DETAIL, " [PICTURE] set decoding size to (%d X %d) scaled size to (%d X %d)\n",
	*width, *height,
	cm->superres_upscaled_width,
	cm->superres_upscaled_height);
#else
  cm->superres_upscaled_width = *width;
  cm->superres_upscaled_height = *height;

  const SequenceHeader *const seq_params = &cm->seq_params;
  if (!seq_params->enable_superres) return;

  //if (aom_rb_read_bit(-1, defmark, rb)) {
  if (params->p.superres_scale_denominator != SCALE_NUMERATOR) {
#ifdef ORI_CODE
    cm->superres_scale_denominator =
        (uint8_t)aom_rb_read_literal(-1, defmark, rb, SUPERRES_SCALE_BITS);
    cm->superres_scale_denominator += SUPERRES_SCALE_DENOMINATOR_MIN;
#else
    cm->superres_scale_denominator = params->p.superres_scale_denominator;
#endif
    // Don't edit cm->width or cm->height directly, or the buffers won't get
    // resized correctly
    av1_calculate_scaled_superres_size(width, height,
                                       cm->superres_scale_denominator);
  } else {
    // 1:1 scaling - ie. no scaling, scale not provided
    cm->superres_scale_denominator = SCALE_NUMERATOR;
  }
/*!USE_SCALED_WIDTH_FROM_UCODE*/
#endif
}

static void resize_context_buffers(AV1_COMMON *cm, int width, int height) {
#if CONFIG_SIZE_LIMIT
  if (width > DECODE_WIDTH_LIMIT || height > DECODE_HEIGHT_LIMIT)
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Dimensions of %dx%d beyond allowed size of %dx%d.",
                       width, height, DECODE_WIDTH_LIMIT, DECODE_HEIGHT_LIMIT);
#endif
  if (cm->width != width || cm->height != height) {
    const int new_mi_rows =
        ALIGN_POWER_OF_TWO(height, MI_SIZE_LOG2) >> MI_SIZE_LOG2;
    const int new_mi_cols =
        ALIGN_POWER_OF_TWO(width, MI_SIZE_LOG2) >> MI_SIZE_LOG2;

    // Allocations in av1_alloc_context_buffers() depend on individual
    // dimensions as well as the overall size.
    if (new_mi_cols > cm->mi_cols || new_mi_rows > cm->mi_rows) {
      if (av1_alloc_context_buffers(cm, width, height)) {
        // The cm->mi_* values have been cleared and any existing context
        // buffers have been freed. Clear cm->width and cm->height to be
        // consistent and to force a realloc next time.
        cm->width = 0;
        cm->height = 0;
        aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                           "Failed to allocate context buffers");
      }
    } else {
      av1_set_mb_mi(cm, width, height);
    }
#ifdef ORI_CODE
    av1_init_context_buffers(cm);
#endif
    cm->width = width;
    cm->height = height;
  }

#ifdef ORI_CODE
  ensure_mv_buffer(cm->cur_frame, cm);
#endif
  cm->cur_frame->width = cm->width;
  cm->cur_frame->height = cm->height;
}

static void setup_buffer_pool(AV1_COMMON *cm) {
  BufferPool *const pool = cm->buffer_pool;
  const SequenceHeader *const seq_params = &cm->seq_params;
  unsigned long flags;

  lock_buffer_pool(pool, flags);
  if (aom_realloc_frame_buffer(cm, &cm->cur_frame->buf,
    cm->width, cm->height, cm->cur_frame->order_hint)) {
    unlock_buffer_pool(pool, flags);
    aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate frame buffer");
  }
  unlock_buffer_pool(pool, flags);

  cm->cur_frame->buf.bit_depth = (unsigned int)seq_params->bit_depth;
  cm->cur_frame->buf.color_primaries = seq_params->color_primaries;
  cm->cur_frame->buf.transfer_characteristics =
      seq_params->transfer_characteristics;
  cm->cur_frame->buf.matrix_coefficients = seq_params->matrix_coefficients;
  cm->cur_frame->buf.monochrome = seq_params->monochrome;
  cm->cur_frame->buf.chroma_sample_position =
      seq_params->chroma_sample_position;
  cm->cur_frame->buf.color_range = seq_params->color_range;
  cm->cur_frame->buf.render_width = cm->render_width;
  cm->cur_frame->buf.render_height = cm->render_height;
}

static void setup_frame_size(AV1_COMMON *cm, int frame_size_override_flag, union param_u *params) {
  const SequenceHeader *const seq_params = &cm->seq_params;
  int width, height, dec_width;

  if (frame_size_override_flag) {
    int num_bits_width = seq_params->num_bits_width;
    int num_bits_height = seq_params->num_bits_height;
    av1_read_frame_size(params, num_bits_width, num_bits_height, &width, &height, &dec_width);
#ifdef AML
    cm->dec_width = dec_width;
#endif
    if (width > seq_params->max_frame_width ||
        height > seq_params->max_frame_height) {
      aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                         "Frame dimensions are larger than the maximum values");
    }
  } else {
    width = seq_params->max_frame_width;
    height = seq_params->max_frame_height;
#ifdef AML
    cm->dec_width = dec_width = params->p.dec_frame_width;
#endif
  }
  setup_superres(cm, params, &width, &height);
  resize_context_buffers(cm, width, height);
#ifdef ORI_CODE
  setup_render_size(cm, params);
#endif
  setup_buffer_pool(cm);
}

static int valid_ref_frame_img_fmt(aom_bit_depth_t ref_bit_depth,
                                          int ref_xss, int ref_yss,
                                          aom_bit_depth_t this_bit_depth,
                                          int this_xss, int this_yss) {
  return ref_bit_depth == this_bit_depth && ref_xss == this_xss &&
         ref_yss == this_yss;
}

static void setup_frame_size_with_refs(AV1_COMMON *cm, union param_u *params) {
  int width, height, dec_width;
  int found = 0;
  int has_valid_ref_frame = 0;
  int i;
  SequenceHeader *seq_params;
  for (i = LAST_FRAME; i <= ALTREF_FRAME; ++i) {
    /*if (aom_rb_read_bit(rb)) {*/
    if (params->p.valid_ref_frame_bits & (1<<i)) {
      const RefCntBuffer *const ref_buf = get_ref_frame_buf(cm, i);
      // This will never be NULL in a normal stream, as streams are required to
      // have a shown keyframe before any inter frames, which would refresh all
      // the reference buffers. However, it might be null if we're starting in
      // the middle of a stream, and static analysis will error if we don't do
      // a null check here.
      if (ref_buf == NULL) {
        aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                           "Invalid condition: invalid reference buffer");
      } else {
        const PIC_BUFFER_CONFIG *const buf = &ref_buf->buf;
        width = buf->y_crop_width;
        height = buf->y_crop_height;
        cm->render_width = buf->render_width;
        cm->render_height = buf->render_height;
        setup_superres(cm, params, &width, &height);
        resize_context_buffers(cm, width, height);
        found = 1;
        break;
      }
    }
  }

  seq_params = &cm->seq_params;
  if (!found) {
    int num_bits_width = seq_params->num_bits_width;
    int num_bits_height = seq_params->num_bits_height;

    av1_read_frame_size(params, num_bits_width, num_bits_height, &width, &height, &dec_width);
#ifdef AML
    cm->dec_width = dec_width;
#endif
    setup_superres(cm, params, &width, &height);
    resize_context_buffers(cm, width, height);
#ifdef ORI_CODE
    setup_render_size(cm, rb);
#endif
  }

  if (width <= 0 || height <= 0)
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Invalid frame size");

  // Check to make sure at least one of frames that this frame references
  // has valid dimensions.
  for (i = LAST_FRAME; i <= ALTREF_FRAME; ++i) {
    const RefCntBuffer *const ref_frame = get_ref_frame_buf(cm, i);
    if (ref_frame != NULL) {
      has_valid_ref_frame |=
        valid_ref_frame_size(ref_frame->buf.y_crop_width,
                             ref_frame->buf.y_crop_height, width, height);
    }
  }
  if (!has_valid_ref_frame)
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Referenced frame has invalid size");
  for (i = LAST_FRAME; i <= ALTREF_FRAME; ++i) {
    const RefCntBuffer *const ref_frame = get_ref_frame_buf(cm, i);
    if (ref_frame != NULL) {
      if (!valid_ref_frame_img_fmt(
            ref_frame->buf.bit_depth, ref_frame->buf.subsampling_x,
            ref_frame->buf.subsampling_y, seq_params->bit_depth,
            seq_params->subsampling_x, seq_params->subsampling_y))
      aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                         "Referenced frame has incompatible color format");
    }
  }
  setup_buffer_pool(cm);
}

typedef struct {
  int map_idx;        // frame map index
  RefCntBuffer *buf;  // frame buffer
  int sort_idx;       // index based on the offset to be used for sorting
} REF_FRAME_INFO;

// Compares the sort_idx fields. If they are equal, then compares the map_idx
// fields to break the tie. This ensures a stable sort.
static int compare_ref_frame_info(const void *arg_a, const void *arg_b) {
  const REF_FRAME_INFO *info_a = (REF_FRAME_INFO *)arg_a;
  const REF_FRAME_INFO *info_b = (REF_FRAME_INFO *)arg_b;

  const int sort_idx_diff = info_a->sort_idx - info_b->sort_idx;
  if (sort_idx_diff != 0) return sort_idx_diff;
  return info_a->map_idx - info_b->map_idx;
}


/*
for av1_setup_motion_field()
*/
static int motion_field_projection(AV1_COMMON *cm,
                                   MV_REFERENCE_FRAME start_frame, int dir) {
#ifdef ORI_CODE
  TPL_MV_REF *tpl_mvs_base = cm->tpl_mvs;
  int ref_offset[REF_FRAMES] = { 0 };
#endif
  MV_REFERENCE_FRAME rf;
  const RefCntBuffer *const start_frame_buf =
      get_ref_frame_buf(cm, start_frame);
  int start_frame_order_hint;
  unsigned int const *ref_order_hints;
  int cur_order_hint;
  int start_to_current_frame_offset;

#ifdef AML
  int i;
  //av1_print2(AV1_DEBUG_BUFMGR_DETAIL, "$$$$$$$$$$$%s:cm->mv_ref_id_index = %d, start_frame=%d\n", __func__, cm->mv_ref_id_index, start_frame);
  cm->mv_ref_id[cm->mv_ref_id_index] = start_frame;
  for (i = 0; i < REF_FRAMES; i++) {
      cm->mv_ref_offset[cm->mv_ref_id_index][i]=0;
  }
  cm->mv_cal_tpl_mvs[cm->mv_ref_id_index]=0;
  cm->mv_ref_id_index++;
#endif
  if (start_frame_buf == NULL) return 0;

  if (start_frame_buf->frame_type == KEY_FRAME ||
      start_frame_buf->frame_type == INTRA_ONLY_FRAME)
    return 0;

  if (start_frame_buf->mi_rows != cm->mi_rows ||
      start_frame_buf->mi_cols != cm->mi_cols)
    return 0;

  start_frame_order_hint = start_frame_buf->order_hint;
  ref_order_hints =
      &start_frame_buf->ref_order_hints[0];
  cur_order_hint = cm->cur_frame->order_hint;
  start_to_current_frame_offset = get_relative_dist(
      &cm->seq_params.order_hint_info, start_frame_order_hint, cur_order_hint);

  for (rf = LAST_FRAME; rf <= INTER_REFS_PER_FRAME; ++rf) {
    cm->mv_ref_offset[cm->mv_ref_id_index-1][rf] = get_relative_dist(&cm->seq_params.order_hint_info,
                                       start_frame_order_hint,
                                       ref_order_hints[rf - LAST_FRAME]);
  }
#ifdef AML
  cm->mv_cal_tpl_mvs[cm->mv_ref_id_index-1]=1;
#endif
  if (dir == 2) start_to_current_frame_offset = -start_to_current_frame_offset;
#ifdef ORI_CODE
  MV_REF *mv_ref_base = start_frame_buf->mvs;
  const int mvs_rows = (cm->mi_rows + 1) >> 1;
  const int mvs_cols = (cm->mi_cols + 1) >> 1;

  for (int blk_row = 0; blk_row < mvs_rows; ++blk_row) {
    for (int blk_col = 0; blk_col < mvs_cols; ++blk_col) {
      MV_REF *mv_ref = &mv_ref_base[blk_row * mvs_cols + blk_col];
      MV fwd_mv = mv_ref->mv.as_mv;

      if (mv_ref->ref_frame > INTRA_FRAME) {
        int_mv this_mv;
        int mi_r, mi_c;
        const int ref_frame_offset = ref_offset[mv_ref->ref_frame];

        int pos_valid =
            abs(ref_frame_offset) <= MAX_FRAME_DISTANCE &&
            ref_frame_offset > 0 &&
            abs(start_to_current_frame_offset) <= MAX_FRAME_DISTANCE;

        if (pos_valid) {
          get_mv_projection(&this_mv.as_mv, fwd_mv,
                            start_to_current_frame_offset, ref_frame_offset);
          pos_valid = get_block_position(cm, &mi_r, &mi_c, blk_row, blk_col,
                                         this_mv.as_mv, dir >> 1);
        }

        if (pos_valid) {
          const int mi_offset = mi_r * (cm->mi_stride >> 1) + mi_c;

          tpl_mvs_base[mi_offset].mfmv0.as_mv.row = fwd_mv.row;
          tpl_mvs_base[mi_offset].mfmv0.as_mv.col = fwd_mv.col;
          tpl_mvs_base[mi_offset].ref_frame_offset = ref_frame_offset;
        }
      }
    }
  }
#endif
  return 1;
}

#ifdef AML
static int setup_motion_field_debug_count = 0;
#endif
void av1_setup_motion_field(AV1_COMMON *cm) {
  const OrderHintInfo *const order_hint_info = &cm->seq_params.order_hint_info;
  int ref_frame;
  int size;
  int cur_order_hint;
  const RefCntBuffer *ref_buf[INTER_REFS_PER_FRAME];
  int ref_order_hint[INTER_REFS_PER_FRAME];
  int ref_stamp;
  memset(cm->ref_frame_side, 0, sizeof(cm->ref_frame_side));
  if (!order_hint_info->enable_order_hint) return;
#ifdef ORI_CODE
  TPL_MV_REF *tpl_mvs_base = cm->tpl_mvs;
#endif
  size = ((cm->mi_rows + MAX_MIB_SIZE) >> 1) * (cm->mi_stride >> 1);
#ifdef ORI_CODE
  for (int idx = 0; idx < size; ++idx) {
    tpl_mvs_base[idx].mfmv0.as_int = INVALID_MV;
    tpl_mvs_base[idx].ref_frame_offset = 0;
  }
#endif
  cur_order_hint = cm->cur_frame->order_hint;

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ref_frame++) {
    const int ref_idx = ref_frame - LAST_FRAME;
    const RefCntBuffer *const buf = get_ref_frame_buf(cm, ref_frame);
    int order_hint = 0;

    if (buf != NULL) order_hint = buf->order_hint;

    ref_buf[ref_idx] = buf;
    ref_order_hint[ref_idx] = order_hint;

    if (get_relative_dist(order_hint_info, order_hint, cur_order_hint) > 0)
      cm->ref_frame_side[ref_frame] = 1;
    else if (order_hint == cur_order_hint)
      cm->ref_frame_side[ref_frame] = -1;
  }
  ref_stamp = MFMV_STACK_SIZE - 1;
#ifdef AML
  cm->mv_ref_id_index = 0;
  av1_print2(AV1_DEBUG_BUFMGR_DETAIL, "%s(%d) mi_cols %d mi_rows %d\n",
      __func__, setup_motion_field_debug_count++,
      cm->mi_cols,
      cm->mi_rows
      );
#endif
  if (ref_buf[LAST_FRAME - LAST_FRAME] != NULL) {
    const int alt_of_lst_order_hint =
        ref_buf[LAST_FRAME - LAST_FRAME]
            ->ref_order_hints[ALTREF_FRAME - LAST_FRAME];

    const int is_lst_overlay =
        (alt_of_lst_order_hint == ref_order_hint[GOLDEN_FRAME - LAST_FRAME]);
    if (!is_lst_overlay) motion_field_projection(cm, LAST_FRAME, 2);
    --ref_stamp;
  }

  if (get_relative_dist(order_hint_info,
                        ref_order_hint[BWDREF_FRAME - LAST_FRAME],
                        cur_order_hint) > 0) {
    if (motion_field_projection(cm, BWDREF_FRAME, 0)) --ref_stamp;
  }

  if (get_relative_dist(order_hint_info,
                        ref_order_hint[ALTREF2_FRAME - LAST_FRAME],
                        cur_order_hint) > 0) {
    if (motion_field_projection(cm, ALTREF2_FRAME, 0)) --ref_stamp;
  }

  if (get_relative_dist(order_hint_info,
                        ref_order_hint[ALTREF_FRAME - LAST_FRAME],
                        cur_order_hint) > 0 &&
      ref_stamp >= 0)
    if (motion_field_projection(cm, ALTREF_FRAME, 0)) --ref_stamp;

  if (ref_stamp >= 0) motion_field_projection(cm, LAST2_FRAME, 2);
}


static void set_ref_frame_info(int *remapped_ref_idx, int frame_idx,
                               REF_FRAME_INFO *ref_info) {
  assert(frame_idx >= 0 && frame_idx < INTER_REFS_PER_FRAME);

  remapped_ref_idx[frame_idx] = ref_info->map_idx;
  av1_print2(AV1_DEBUG_BUFMGR_DETAIL, "+++++++++++++%s:remapped_ref_idx[%d]=0x%x\n", __func__, frame_idx, ref_info->map_idx);
}


void av1_set_frame_refs(AV1_COMMON *const cm, int *remapped_ref_idx,
                        int lst_map_idx, int gld_map_idx) {
  int lst_frame_sort_idx = -1;
  int gld_frame_sort_idx = -1;
  int i;
  //assert(cm->seq_params.order_hint_info.enable_order_hint);
  //assert(cm->seq_params.order_hint_info.order_hint_bits_minus_1 >= 0);
  const int cur_order_hint = (int)cm->current_frame.order_hint;
  const int cur_frame_sort_idx =
      1 << cm->seq_params.order_hint_info.order_hint_bits_minus_1;

  REF_FRAME_INFO ref_frame_info[REF_FRAMES];
  int ref_flag_list[INTER_REFS_PER_FRAME] = { 0, 0, 0, 0, 0, 0, 0 };
  int bwd_start_idx;
  int bwd_end_idx;
  int fwd_start_idx, fwd_end_idx;
  int ref_idx;
  static const MV_REFERENCE_FRAME ref_frame_list[INTER_REFS_PER_FRAME - 2] = {
    LAST2_FRAME, LAST3_FRAME, BWDREF_FRAME, ALTREF2_FRAME, ALTREF_FRAME
  };

  for (i = 0; i < REF_FRAMES; ++i) {
    const int map_idx = i;
    RefCntBuffer *buf;
    int offset;

    ref_frame_info[i].map_idx = map_idx;
    ref_frame_info[i].sort_idx = -1;

    buf = cm->ref_frame_map[map_idx];
    ref_frame_info[i].buf = buf;

    if (buf == NULL) continue;
    // If this assertion fails, there is a reference leak.
    assert(buf->ref_count > 0);

    offset = (int)buf->order_hint;
    ref_frame_info[i].sort_idx =
        (offset == -1) ? -1
                       : cur_frame_sort_idx +
                             get_relative_dist(&cm->seq_params.order_hint_info,
                                               offset, cur_order_hint);
    assert(ref_frame_info[i].sort_idx >= -1);

    if (map_idx == lst_map_idx) lst_frame_sort_idx = ref_frame_info[i].sort_idx;
    if (map_idx == gld_map_idx) gld_frame_sort_idx = ref_frame_info[i].sort_idx;
  }

  // Confirm both LAST_FRAME and GOLDEN_FRAME are valid forward reference
  // frames.
  if (lst_frame_sort_idx == -1 || lst_frame_sort_idx >= cur_frame_sort_idx) {
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Inter frame requests a look-ahead frame as LAST");
  }
  if (gld_frame_sort_idx == -1 || gld_frame_sort_idx >= cur_frame_sort_idx) {
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Inter frame requests a look-ahead frame as GOLDEN");
  }

  // Sort ref frames based on their frame_offset values.
  qsort(ref_frame_info, REF_FRAMES, sizeof(REF_FRAME_INFO),
        compare_ref_frame_info);

  // Identify forward and backward reference frames.
  // Forward  reference: offset < order_hint
  // Backward reference: offset >= order_hint
  fwd_start_idx = 0;
  fwd_end_idx = REF_FRAMES - 1;

  for (i = 0; i < REF_FRAMES; i++) {
    if (ref_frame_info[i].sort_idx == -1) {
      fwd_start_idx++;
      continue;
    }

    if (ref_frame_info[i].sort_idx >= cur_frame_sort_idx) {
      fwd_end_idx = i - 1;
      break;
    }
  }

  bwd_start_idx = fwd_end_idx + 1;
  bwd_end_idx = REF_FRAMES - 1;

  // === Backward Reference Frames ===

  // == ALTREF_FRAME ==
  if (bwd_start_idx <= bwd_end_idx) {
    set_ref_frame_info(remapped_ref_idx, ALTREF_FRAME - LAST_FRAME,
                       &ref_frame_info[bwd_end_idx]);
    ref_flag_list[ALTREF_FRAME - LAST_FRAME] = 1;
    bwd_end_idx--;
  }

  // == BWDREF_FRAME ==
  if (bwd_start_idx <= bwd_end_idx) {
    set_ref_frame_info(remapped_ref_idx, BWDREF_FRAME - LAST_FRAME,
                       &ref_frame_info[bwd_start_idx]);
    ref_flag_list[BWDREF_FRAME - LAST_FRAME] = 1;
    bwd_start_idx++;
  }

  // == ALTREF2_FRAME ==
  if (bwd_start_idx <= bwd_end_idx) {
    set_ref_frame_info(remapped_ref_idx, ALTREF2_FRAME - LAST_FRAME,
                       &ref_frame_info[bwd_start_idx]);
    ref_flag_list[ALTREF2_FRAME - LAST_FRAME] = 1;
  }

  // === Forward Reference Frames ===

  for (i = fwd_start_idx; i <= fwd_end_idx; ++i) {
    // == LAST_FRAME ==
    if (ref_frame_info[i].map_idx == lst_map_idx) {
      set_ref_frame_info(remapped_ref_idx, LAST_FRAME - LAST_FRAME,
                         &ref_frame_info[i]);
      ref_flag_list[LAST_FRAME - LAST_FRAME] = 1;
    }

    // == GOLDEN_FRAME ==
    if (ref_frame_info[i].map_idx == gld_map_idx) {
      set_ref_frame_info(remapped_ref_idx, GOLDEN_FRAME - LAST_FRAME,
                         &ref_frame_info[i]);
      ref_flag_list[GOLDEN_FRAME - LAST_FRAME] = 1;
    }
  }

  assert(ref_flag_list[LAST_FRAME - LAST_FRAME] == 1 &&
         ref_flag_list[GOLDEN_FRAME - LAST_FRAME] == 1);

  // == LAST2_FRAME ==
  // == LAST3_FRAME ==
  // == BWDREF_FRAME ==
  // == ALTREF2_FRAME ==
  // == ALTREF_FRAME ==

  // Set up the reference frames in the anti-chronological order.
  for (ref_idx = 0; ref_idx < (INTER_REFS_PER_FRAME - 2); ref_idx++) {
    const MV_REFERENCE_FRAME ref_frame = ref_frame_list[ref_idx];

    if (ref_flag_list[ref_frame - LAST_FRAME] == 1) continue;

    while (fwd_start_idx <= fwd_end_idx &&
           (ref_frame_info[fwd_end_idx].map_idx == lst_map_idx ||
            ref_frame_info[fwd_end_idx].map_idx == gld_map_idx)) {
      fwd_end_idx--;
    }
    if (fwd_start_idx > fwd_end_idx) break;

    set_ref_frame_info(remapped_ref_idx, ref_frame - LAST_FRAME,
                       &ref_frame_info[fwd_end_idx]);
    ref_flag_list[ref_frame - LAST_FRAME] = 1;

    fwd_end_idx--;
  }

  // Assign all the remaining frame(s), if any, to the earliest reference frame.
  for (; ref_idx < (INTER_REFS_PER_FRAME - 2); ref_idx++) {
    const MV_REFERENCE_FRAME ref_frame = ref_frame_list[ref_idx];
    if (ref_flag_list[ref_frame - LAST_FRAME] == 1) continue;
    set_ref_frame_info(remapped_ref_idx, ref_frame - LAST_FRAME,
                       &ref_frame_info[fwd_start_idx]);
    ref_flag_list[ref_frame - LAST_FRAME] = 1;
  }

  for (i = 0; i < INTER_REFS_PER_FRAME; i++) {
    assert(ref_flag_list[i] == 1);
  }
}

void av1_setup_frame_buf_refs(AV1_COMMON *cm) {
  MV_REFERENCE_FRAME ref_frame;
  cm->cur_frame->order_hint = cm->current_frame.order_hint;

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    const RefCntBuffer *const buf = get_ref_frame_buf(cm, ref_frame);
    if (buf != NULL)
      cm->cur_frame->ref_order_hints[ref_frame - LAST_FRAME] = buf->order_hint;
  }
}

void av1_setup_frame_sign_bias(AV1_COMMON *cm) {
  MV_REFERENCE_FRAME ref_frame;
  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    const RefCntBuffer *const buf = get_ref_frame_buf(cm, ref_frame);
    if (cm->seq_params.order_hint_info.enable_order_hint && buf != NULL) {
      const int ref_order_hint = buf->order_hint;
      cm->ref_frame_sign_bias[ref_frame] =
          (get_relative_dist(&cm->seq_params.order_hint_info, ref_order_hint,
                             (int)cm->current_frame.order_hint) <= 0)
              ? 0
              : 1;
    } else {
      cm->ref_frame_sign_bias[ref_frame] = 0;
    }
  }
}


void av1_setup_skip_mode_allowed(AV1_COMMON *cm)
{
	const OrderHintInfo *const order_hint_info = &cm->seq_params.order_hint_info;
	SkipModeInfo *const skip_mode_info = &cm->current_frame.skip_mode_info;
	int i;
	int cur_order_hint;
	int ref_order_hints[2] = { -1, INT_MAX };
	int ref_idx[2] = { INVALID_IDX, INVALID_IDX };

	skip_mode_info->skip_mode_allowed = 0;
	skip_mode_info->ref_frame_idx_0 = INVALID_IDX;
	skip_mode_info->ref_frame_idx_1 = INVALID_IDX;
	av1_print2(AV1_DEBUG_BUFMGR_DETAIL, "av1_setup_skip_mode_allowed %d %d %d\n", order_hint_info->enable_order_hint,
		frame_is_intra_only(cm),
		cm->current_frame.reference_mode);
	if (!order_hint_info->enable_order_hint || frame_is_intra_only(cm) ||
		cm->current_frame.reference_mode == SINGLE_REFERENCE)
		return;

	cur_order_hint = cm->current_frame.order_hint;

	// Identify the nearest forward and backward references.
	for (i = 0; i < INTER_REFS_PER_FRAME; ++i) {
		const RefCntBuffer *const buf = get_ref_frame_buf(cm, LAST_FRAME + i);
		int ref_order_hint;
		if (buf == NULL) continue;

		ref_order_hint = buf->order_hint;
		if (get_relative_dist(order_hint_info, ref_order_hint, cur_order_hint) < 0) {
			// Forward reference
			if (ref_order_hints[0] == -1 ||
				get_relative_dist(order_hint_info, ref_order_hint,
				ref_order_hints[0]) > 0) {
				ref_order_hints[0] = ref_order_hint;
				ref_idx[0] = i;
			}
		} else if (get_relative_dist(order_hint_info, ref_order_hint,
		cur_order_hint) > 0) {
			// Backward reference
			if (ref_order_hints[1] == INT_MAX ||
				get_relative_dist(order_hint_info, ref_order_hint,
				ref_order_hints[1]) < 0) {
				ref_order_hints[1] = ref_order_hint;
				ref_idx[1] = i;
			}
		}
	}

	if (ref_idx[0] != INVALID_IDX && ref_idx[1] != INVALID_IDX) {
		// == Bi-directional prediction ==
		skip_mode_info->skip_mode_allowed = 1;
		skip_mode_info->ref_frame_idx_0 = AOMMIN(ref_idx[0], ref_idx[1]);
		skip_mode_info->ref_frame_idx_1 = AOMMAX(ref_idx[0], ref_idx[1]);
	} else if (ref_idx[0] != INVALID_IDX && ref_idx[1] == INVALID_IDX) {
		// == Forward prediction only ==
		// Identify the second nearest forward reference.
		ref_order_hints[1] = -1;
		for (i = 0; i < INTER_REFS_PER_FRAME; ++i) {
			const RefCntBuffer *const buf = get_ref_frame_buf(cm, LAST_FRAME + i);
			int ref_order_hint;
			if (buf == NULL) continue;

			ref_order_hint = buf->order_hint;
			if ((ref_order_hints[0] != -1 &&
			get_relative_dist(order_hint_info, ref_order_hint, ref_order_hints[0]) < 0) &&
			(ref_order_hints[1] == -1 ||
			get_relative_dist(order_hint_info, ref_order_hint, ref_order_hints[1]) > 0)) {
				// Second closest forward reference
				ref_order_hints[1] = ref_order_hint;
				ref_idx[1] = i;
			}
		}
		if (ref_order_hints[1] != -1) {
			skip_mode_info->skip_mode_allowed = 1;
			skip_mode_info->ref_frame_idx_0 = AOMMIN(ref_idx[0], ref_idx[1]);
			skip_mode_info->ref_frame_idx_1 = AOMMAX(ref_idx[0], ref_idx[1]);
		}
	}
	av1_print2(AV1_DEBUG_BUFMGR_DETAIL,
		"skip_mode_info: skip_mode_allowed 0x%x 0x%x 0x%x\n",
	cm->current_frame.skip_mode_info.skip_mode_allowed,
	cm->current_frame.skip_mode_info.ref_frame_idx_0,
	cm->current_frame.skip_mode_info.ref_frame_idx_1);
}

static inline int frame_might_allow_ref_frame_mvs(const AV1_COMMON *cm) {
  return !cm->error_resilient_mode &&
    cm->seq_params.order_hint_info.enable_ref_frame_mvs &&
    cm->seq_params.order_hint_info.enable_order_hint &&
    !frame_is_intra_only(cm);
}

#ifdef ORI_CODE
/*
* segmentation
*/
static const int seg_feature_data_signed[SEG_LVL_MAX] = {
  1, 1, 1, 1, 1, 0, 0, 0
};

static const int seg_feature_data_max[SEG_LVL_MAX] = { MAXQ,
                                                       MAX_LOOP_FILTER,
                                                       MAX_LOOP_FILTER,
                                                       MAX_LOOP_FILTER,
                                                       MAX_LOOP_FILTER,
                                                       7,
                                                       0,
                                                       0 };


static inline void segfeatures_copy(struct segmentation *dst,
                                    const struct segmentation *src) {
  int i, j;
  for (i = 0; i < MAX_SEGMENTS; i++) {
    dst->feature_mask[i] = src->feature_mask[i];
    for (j = 0; j < SEG_LVL_MAX; j++) {
      dst->feature_data[i][j] = src->feature_data[i][j];
    }
  }
  dst->segid_preskip = src->segid_preskip;
  dst->last_active_segid = src->last_active_segid;
}

static void av1_clearall_segfeatures(struct segmentation *seg) {
  av1_zero(seg->feature_data);
  av1_zero(seg->feature_mask);
}

static void av1_enable_segfeature(struct segmentation *seg, int segment_id,
    int feature_id) {
  seg->feature_mask[segment_id] |= 1 << feature_id;
}

void av1_calculate_segdata(struct segmentation *seg) {
  seg->segid_preskip = 0;
  seg->last_active_segid = 0;
  for (int i = 0; i < MAX_SEGMENTS; i++) {
    for (int j = 0; j < SEG_LVL_MAX; j++) {
      if (seg->feature_mask[i] & (1 << j)) {
        seg->segid_preskip |= (j >= SEG_LVL_REF_FRAME);
        seg->last_active_segid = i;
      }
    }
  }
}

static int av1_seg_feature_data_max(int feature_id) {
  return seg_feature_data_max[feature_id];
}

static int av1_is_segfeature_signed(int feature_id) {
  return seg_feature_data_signed[feature_id];
}

static void av1_set_segdata(struct segmentation *seg, int segment_id,
                     int feature_id, int seg_data) {
  if (seg_data < 0) {
    assert(seg_feature_data_signed[feature_id]);
    assert(-seg_data <= seg_feature_data_max[feature_id]);
  } else {
    assert(seg_data <= seg_feature_data_max[feature_id]);
  }

  seg->feature_data[segment_id][feature_id] = seg_data;
}

static inline int clamp(int value, int low, int high) {
  return value < low ? low : (value > high ? high : value);
}

static void setup_segmentation(AV1_COMMON *const cm,
                               union param_u *params) {
  struct segmentation *const seg = &cm->seg;

  seg->update_map = 0;
  seg->update_data = 0;
  seg->temporal_update = 0;

  seg->enabled = params->p.seg_enabled; //aom_rb_read_bit(-1, defmark, rb);
  if (!seg->enabled) {
    if (cm->cur_frame->seg_map)
      memset(cm->cur_frame->seg_map, 0, (cm->mi_rows * cm->mi_cols));

    memset(seg, 0, sizeof(*seg));
    segfeatures_copy(&cm->cur_frame->seg, seg);
    return;
  }
  if (cm->seg.enabled && cm->prev_frame &&
      (cm->mi_rows == cm->prev_frame->mi_rows) &&
      (cm->mi_cols == cm->prev_frame->mi_cols)) {
    cm->last_frame_seg_map = cm->prev_frame->seg_map;
  } else {
    cm->last_frame_seg_map = NULL;
  }
  // Read update flags
  if (cm->primary_ref_frame == PRIMARY_REF_NONE) {
    // These frames can't use previous frames, so must signal map + features
    seg->update_map = 1;
    seg->temporal_update = 0;
    seg->update_data = 1;
  } else {
    seg->update_map = params->p.seg_update_map; // aom_rb_read_bit(-1, defmark, rb);
    if (seg->update_map) {
      seg->temporal_update = params->p.seg_temporal_update; //aom_rb_read_bit(-1, defmark, rb);
    } else {
      seg->temporal_update = 0;
    }
    seg->update_data = params->p.seg_update_data; //aom_rb_read_bit(-1, defmark, rb);
  }

  // Segmentation data update
  if (seg->update_data) {
    av1_clearall_segfeatures(seg);

    for (int i = 0; i < MAX_SEGMENTS; i++) {
      for (int j = 0; j < SEG_LVL_MAX; j++) {
        int data = 0;
        const int feature_enabled = params->p.seg_feature_enabled ;//aom_rb_read_bit(-1, defmark, rb);
        if (feature_enabled) {
          av1_enable_segfeature(seg, i, j);

          const int data_max = av1_seg_feature_data_max(j);
          const int data_min = -data_max;
          /*
          const int ubits = get_unsigned_bits(data_max);

          if (av1_is_segfeature_signed(j)) {
            data = aom_rb_read_inv_signed_literal(-1, defmark, rb, ubits);
          } else {
            data = aom_rb_read_literal(-1, defmark, rb, ubits);
          }*/
          data = params->p.seg_data;
          data = clamp(data, data_min, data_max);
        }
        av1_set_segdata(seg, i, j, data);
      }
    }
    av1_calculate_segdata(seg);
  } else if (cm->prev_frame) {
    segfeatures_copy(seg, &cm->prev_frame->seg);
  }
  segfeatures_copy(&cm->cur_frame->seg, seg);
}
#endif

/**/


int av1_decode_frame_headers_and_setup(AV1Decoder *pbi, int trailing_bits_present, union param_u *params)
{
  AV1_COMMON *const cm = pbi->common;
  /*
  read_uncompressed_header()
  */
  const SequenceHeader *const seq_params = &cm->seq_params;
  CurrentFrame *const current_frame = &cm->current_frame;
  //MACROBLOCKD *const xd = &pbi->mb;
  BufferPool *const pool = cm->buffer_pool;
  RefCntBuffer *const frame_bufs = pool->frame_bufs;
  int i;
  int frame_size_override_flag;
  unsigned long flags;

  if (!pbi->sequence_header_ready) {
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "No sequence header");
  }
  cm->last_frame_type = current_frame->frame_type;

  if (seq_params->reduced_still_picture_hdr) {
    cm->show_existing_frame = 0;
    cm->show_frame = 1;
    current_frame->frame_type = KEY_FRAME;
    if (pbi->sequence_header_changed) {
      // This is the start of a new coded video sequence.
      pbi->sequence_header_changed = 0;
      pbi->decoding_first_frame = 1;
      reset_frame_buffers(pbi);
    }
    cm->error_resilient_mode = 1;
  } else {
    cm->show_existing_frame = params->p.show_existing_frame;
    pbi->reset_decoder_state = 0;
    if (cm->show_existing_frame) {
      int existing_frame_idx;
      RefCntBuffer *frame_to_show;
      if (pbi->sequence_header_changed) {
        aom_internal_error(
            &cm->error, AOM_CODEC_CORRUPT_FRAME,
            "New sequence header starts with a show_existing_frame.");
      }
      // Show an existing frame directly.
      existing_frame_idx = params->p.existing_frame_idx; //aom_rb_read_literal(rb, 3);
      frame_to_show = cm->ref_frame_map[existing_frame_idx];
      if (frame_to_show == NULL) {
          aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                           "Buffer does not contain a decoded frame");
          return 0;
      }
      if (seq_params->decoder_model_info_present_flag &&
          cm->timing_info.equal_picture_interval == 0) {
        cm->frame_presentation_time = params->p.frame_presentation_time;
        //read_temporal_point_info(cm);
      }
      if (seq_params->frame_id_numbers_present_flag) {
        //int frame_id_length = seq_params->frame_id_length;
        int display_frame_id = params->p.display_frame_id; //aom_rb_read_literal(rb, frame_id_length);
        /* Compare display_frame_id with ref_frame_id and check valid for
         * referencing */
        if (display_frame_id != cm->ref_frame_id[existing_frame_idx] ||
            cm->valid_for_referencing[existing_frame_idx] == 0)
          aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                             "Reference buffer frame ID mismatch");
      }
      lock_buffer_pool(pool, flags);
      assert(frame_to_show->ref_count > 0);
      // cm->cur_frame should be the buffer referenced by the return value
      // of the get_free_fb() call in av1_receive_compressed_data(), and
      // generate_next_ref_frame_map() has not been called, so ref_count
      // should still be 1.
      assert(cm->cur_frame->ref_count == 1);
      // assign_frame_buffer_p() decrements ref_count directly rather than
      // call decrease_ref_count(). If cm->cur_frame->raw_frame_buffer has
      // already been allocated, it will not be released by
      // assign_frame_buffer_p()!
      assert(!cm->cur_frame->raw_frame_buffer.data);
      assign_frame_buffer_p(&cm->cur_frame, frame_to_show);
      pbi->reset_decoder_state = frame_to_show->frame_type == KEY_FRAME;
      unlock_buffer_pool(pool, flags);

#ifdef ORI_CODE
      cm->lf.filter_level[0] = 0;
      cm->lf.filter_level[1] = 0;
#endif
      cm->show_frame = 1;

      // Section 6.8.2: It is a requirement of bitstream conformance that when
      // show_existing_frame is used to show a previous frame, that the value
      // of showable_frame for the previous frame was equal to 1.
      if (!frame_to_show->showable_frame) {
        aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                           "Buffer does not contain a showable frame");
      }
      // Section 6.8.2: It is a requirement of bitstream conformance that when
      // show_existing_frame is used to show a previous frame with
      // RefFrameType[ frame_to_show_map_idx ] equal to KEY_FRAME, that the
      // frame is output via the show_existing_frame mechanism at most once.
      if (pbi->reset_decoder_state) frame_to_show->showable_frame = 0;

#ifdef ORI_CODE
      cm->film_grain_params = frame_to_show->film_grain_params;
#endif
      if (pbi->reset_decoder_state) {
        show_existing_frame_reset(pbi, existing_frame_idx);
      } else {
        current_frame->refresh_frame_flags = 0;
      }

      return 0;
    }

    current_frame->frame_type = (FRAME_TYPE)params->p.frame_type; //aom_rb_read_literal(rb, 2);
    if (pbi->sequence_header_changed) {
      if (current_frame->frame_type == KEY_FRAME) {
        // This is the start of a new coded video sequence.
        pbi->sequence_header_changed = 0;
        pbi->decoding_first_frame = 1;
        reset_frame_buffers(pbi);
      } else {
        aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                           "Sequence header has changed without a keyframe.");
      }
    }
    cm->show_frame = params->p.show_frame; //aom_rb_read_bit(rb);
    if (seq_params->still_picture &&
        (current_frame->frame_type != KEY_FRAME || !cm->show_frame)) {
      aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                         "Still pictures must be coded as shown keyframes");
    }
    cm->showable_frame = current_frame->frame_type != KEY_FRAME;
    if (cm->show_frame) {
      if (seq_params->decoder_model_info_present_flag &&
          cm->timing_info.equal_picture_interval == 0)
        cm->frame_presentation_time = params->p.frame_presentation_time;
        //read_temporal_point_info(cm);
    } else {
      // See if this frame can be used as show_existing_frame in future
      cm->showable_frame = params->p.showable_frame;//aom_rb_read_bit(rb);
    }
    cm->cur_frame->show_frame = cm->show_frame;
    cm->cur_frame->showable_frame = cm->showable_frame;
    cm->error_resilient_mode =
        frame_is_sframe(cm) ||
                (current_frame->frame_type == KEY_FRAME && cm->show_frame)
            ? 1
            : params->p.error_resilient_mode; //aom_rb_read_bit(rb);
  }

#ifdef ORI_CODE
  cm->disable_cdf_update = aom_rb_read_bit(rb);
  if (seq_params->force_screen_content_tools == 2) {
    cm->allow_screen_content_tools = aom_rb_read_bit(rb);
  } else {
    cm->allow_screen_content_tools = seq_params->force_screen_content_tools;
  }

  if (cm->allow_screen_content_tools) {
    if (seq_params->force_integer_mv == 2) {
      cm->cur_frame_force_integer_mv = aom_rb_read_bit(rb);
    } else {
      cm->cur_frame_force_integer_mv = seq_params->force_integer_mv;
    }
  } else {
    cm->cur_frame_force_integer_mv = 0;
  }
#endif

  frame_size_override_flag = 0;
  cm->allow_intrabc = 0;
  cm->primary_ref_frame = PRIMARY_REF_NONE;

  if (!seq_params->reduced_still_picture_hdr) {
    if (seq_params->frame_id_numbers_present_flag) {
      int frame_id_length = seq_params->frame_id_length;
      int diff_len = seq_params->delta_frame_id_length;
      int prev_frame_id = 0;
      int have_prev_frame_id =
          !pbi->decoding_first_frame &&
          !(current_frame->frame_type == KEY_FRAME && cm->show_frame);
      if (have_prev_frame_id) {
        prev_frame_id = cm->current_frame_id;
      }
      cm->current_frame_id = params->p.current_frame_id; //aom_rb_read_literal(rb, frame_id_length);

      if (have_prev_frame_id) {
        int diff_frame_id;
        if (cm->current_frame_id > prev_frame_id) {
          diff_frame_id = cm->current_frame_id - prev_frame_id;
        } else {
          diff_frame_id =
              (1 << frame_id_length) + cm->current_frame_id - prev_frame_id;
        }
        /* Check current_frame_id for conformance */
        if (prev_frame_id == cm->current_frame_id ||
            diff_frame_id >= (1 << (frame_id_length - 1))) {
          aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                             "Invalid value of current_frame_id");
        }
      }
      /* Check if some frames need to be marked as not valid for referencing */
      for (i = 0; i < REF_FRAMES; i++) {
        if (current_frame->frame_type == KEY_FRAME && cm->show_frame) {
          cm->valid_for_referencing[i] = 0;
        } else if (cm->current_frame_id - (1 << diff_len) > 0) {
          if (cm->ref_frame_id[i] > cm->current_frame_id ||
              cm->ref_frame_id[i] < cm->current_frame_id - (1 << diff_len))
            cm->valid_for_referencing[i] = 0;
        } else {
          if (cm->ref_frame_id[i] > cm->current_frame_id &&
              cm->ref_frame_id[i] < (1 << frame_id_length) +
                                        cm->current_frame_id - (1 << diff_len))
            cm->valid_for_referencing[i] = 0;
        }
      }
    }

    frame_size_override_flag = frame_is_sframe(cm) ? 1 : params->p.frame_size_override_flag; //aom_rb_read_bit(rb);

    current_frame->order_hint = params->p.order_hint; /*aom_rb_read_literal(
        rb, seq_params->order_hint_info.order_hint_bits_minus_1 + 1);*/
    current_frame->frame_number = current_frame->order_hint;

    if (!cm->error_resilient_mode && !frame_is_intra_only(cm)) {
      cm->primary_ref_frame = params->p.primary_ref_frame;//aom_rb_read_literal(rb, PRIMARY_REF_BITS);
    }
  }

  if (seq_params->decoder_model_info_present_flag) {
    cm->buffer_removal_time_present = params->p.buffer_removal_time_present; //aom_rb_read_bit(rb);
    if (cm->buffer_removal_time_present) {
      int op_num;
      for (op_num = 0;
           op_num < seq_params->operating_points_cnt_minus_1 + 1; op_num++) {
        if (cm->op_params[op_num].decoder_model_param_present_flag) {
          if ((((seq_params->operating_point_idc[op_num] >>
                 cm->temporal_layer_id) &
                0x1) &&
               ((seq_params->operating_point_idc[op_num] >>
                 (cm->spatial_layer_id + 8)) &
                0x1)) ||
              seq_params->operating_point_idc[op_num] == 0) {
            cm->op_frame_timing[op_num].buffer_removal_time =
                params->p.op_frame_timing[op_num];
                /*aom_rb_read_unsigned_literal(
                    rb, cm->buffer_model.buffer_removal_time_length);*/
          } else {
            cm->op_frame_timing[op_num].buffer_removal_time = 0;
          }
        } else {
          cm->op_frame_timing[op_num].buffer_removal_time = 0;
        }
      }
    }
  }
  if (current_frame->frame_type == KEY_FRAME) {
    if (!cm->show_frame) {  // unshown keyframe (forward keyframe)
      current_frame->refresh_frame_flags = params->p.refresh_frame_flags; //aom_rb_read_literal(rb, REF_FRAMES);
    } else {  // shown keyframe
      current_frame->refresh_frame_flags = (1 << REF_FRAMES) - 1;
    }

    for (i = 0; i < INTER_REFS_PER_FRAME; ++i) {
      cm->remapped_ref_idx[i] = INVALID_IDX;
    }
    if (pbi->need_resync) {
      reset_ref_frame_map(pbi);
      pbi->need_resync = 0;
    }
  } else {
    if (current_frame->frame_type == INTRA_ONLY_FRAME) {
      current_frame->refresh_frame_flags = params->p.refresh_frame_flags; //aom_rb_read_literal(rb, REF_FRAMES);
      if (current_frame->refresh_frame_flags == 0xFF) {
        aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                           "Intra only frames cannot have refresh flags 0xFF");
      }
      if (pbi->need_resync) {
        reset_ref_frame_map(pbi);
        pbi->need_resync = 0;
      }
    } else if (pbi->need_resync != 1) { /* Skip if need resync */
      current_frame->refresh_frame_flags =
          frame_is_sframe(cm) ? 0xFF : params->p.refresh_frame_flags; //aom_rb_read_literal(rb, REF_FRAMES);
    }
  }

  if (!frame_is_intra_only(cm) || current_frame->refresh_frame_flags != 0xFF) {
    // Read all ref frame order hints if error_resilient_mode == 1
    if (cm->error_resilient_mode &&
        seq_params->order_hint_info.enable_order_hint) {
      int ref_idx;
      for (ref_idx = 0; ref_idx < REF_FRAMES; ref_idx++) {
        // Read order hint from bit stream
        unsigned int order_hint = params->p.ref_order_hint[ref_idx];/*aom_rb_read_literal(
            rb, seq_params->order_hint_info.order_hint_bits_minus_1 + 1);*/
        // Get buffer
        RefCntBuffer *buf = cm->ref_frame_map[ref_idx];
        int buf_idx;
        if (buf == NULL || order_hint != buf->order_hint) {
          if (buf != NULL) {
            lock_buffer_pool(pool, flags);
            decrease_ref_count(pbi, buf, pool);
            unlock_buffer_pool(pool, flags);
          }
          // If no corresponding buffer exists, allocate a new buffer with all
          // pixels set to neutral grey.
          buf_idx =  get_free_frame_buffer(cm);
          if (buf_idx == INVALID_IDX) {
            aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                               "Unable to find free frame buffer");
          }
          buf = &frame_bufs[buf_idx];
          lock_buffer_pool(pool, flags);
          if (aom_realloc_frame_buffer(cm, &buf->buf, seq_params->max_frame_width,
                  seq_params->max_frame_height, buf->order_hint)) {
            decrease_ref_count(pbi, buf, pool);
            unlock_buffer_pool(pool, flags);
            aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                               "Failed to allocate frame buffer");
          }
          unlock_buffer_pool(pool, flags);
#ifdef ORI_CODE
          set_planes_to_neutral_grey(seq_params, &buf->buf, 0);
#endif
          cm->ref_frame_map[ref_idx] = buf;
          buf->order_hint = order_hint;
        }
      }
    }
  }

  if (current_frame->frame_type == KEY_FRAME) {
    setup_frame_size(cm, frame_size_override_flag, params);
#ifdef ORI_CODE
    if (cm->allow_screen_content_tools && !av1_superres_scaled(cm))
      cm->allow_intrabc = aom_rb_read_bit(rb);
#endif
    cm->allow_ref_frame_mvs = 0;
    cm->prev_frame = NULL;
  } else {
    cm->allow_ref_frame_mvs = 0;

    if (current_frame->frame_type == INTRA_ONLY_FRAME) {
#ifdef ORI_CODE
      cm->cur_frame->film_grain_params_present =
          seq_params->film_grain_params_present;
#endif
      setup_frame_size(cm, frame_size_override_flag, params);
#ifdef ORI_CODE
      if (cm->allow_screen_content_tools && !av1_superres_scaled(cm))
        cm->allow_intrabc = aom_rb_read_bit(rb);
#endif
    } else if (pbi->need_resync != 1) { /* Skip if need resync */
      int frame_refs_short_signaling = 0;
      // Frame refs short signaling is off when error resilient mode is on.
      if (seq_params->order_hint_info.enable_order_hint)
        frame_refs_short_signaling = params->p.frame_refs_short_signaling;//aom_rb_read_bit(rb);

      if (frame_refs_short_signaling) {
        // == LAST_FRAME ==
        const int lst_ref = params->p.lst_ref; //aom_rb_read_literal(rb, REF_FRAMES_LOG2);
        const RefCntBuffer *const lst_buf = cm->ref_frame_map[lst_ref];

        // == GOLDEN_FRAME ==
        const int gld_ref = params->p.gld_ref; //aom_rb_read_literal(rb, REF_FRAMES_LOG2);
        const RefCntBuffer *const gld_buf = cm->ref_frame_map[gld_ref];

        // Most of the time, streams start with a keyframe. In that case,
        // ref_frame_map will have been filled in at that point and will not
        // contain any NULLs. However, streams are explicitly allowed to start
        // with an intra-only frame, so long as they don't then signal a
        // reference to a slot that hasn't been set yet. That's what we are
        // checking here.
        if (lst_buf == NULL)
          aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                             "Inter frame requests nonexistent reference");
        if (gld_buf == NULL)
          aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                             "Inter frame requests nonexistent reference");

        av1_set_frame_refs(cm, cm->remapped_ref_idx, lst_ref, gld_ref);
      }

      for (i = 0; i < INTER_REFS_PER_FRAME; ++i) {
        int ref = 0;
        if (!frame_refs_short_signaling) {
          ref = params->p.remapped_ref_idx[i];//aom_rb_read_literal(rb, REF_FRAMES_LOG2);

          // Most of the time, streams start with a keyframe. In that case,
          // ref_frame_map will have been filled in at that point and will not
          // contain any NULLs. However, streams are explicitly allowed to start
          // with an intra-only frame, so long as they don't then signal a
          // reference to a slot that hasn't been set yet. That's what we are
          // checking here.
          if (cm->ref_frame_map[ref] == NULL)
            aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                               "Inter frame requests nonexistent reference");
          cm->remapped_ref_idx[i] = ref;
        } else {
          ref = cm->remapped_ref_idx[i];
        }

        cm->ref_frame_sign_bias[LAST_FRAME + i] = 0;

        if (seq_params->frame_id_numbers_present_flag) {
          int frame_id_length = seq_params->frame_id_length;
          //int diff_len = seq_params->delta_frame_id_length;
          int delta_frame_id_minus_1 = params->p.delta_frame_id_minus_1[i];//aom_rb_read_literal(rb, diff_len);
          int ref_frame_id =
              ((cm->current_frame_id - (delta_frame_id_minus_1 + 1) +
                (1 << frame_id_length)) %
               (1 << frame_id_length));
          // Compare values derived from delta_frame_id_minus_1 and
          // refresh_frame_flags. Also, check valid for referencing
          if (ref_frame_id != cm->ref_frame_id[ref] ||
              cm->valid_for_referencing[ref] == 0)
            aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                               "Reference buffer frame ID mismatch");
        }
      }

      if (!cm->error_resilient_mode && frame_size_override_flag) {
        setup_frame_size_with_refs(cm, params);
      } else {
        setup_frame_size(cm, frame_size_override_flag, params);
      }
#ifdef ORI_CODE
      if (cm->cur_frame_force_integer_mv) {
        cm->allow_high_precision_mv = 0;
      } else {
        cm->allow_high_precision_mv = aom_rb_read_bit(rb);
      }
      cm->interp_filter = read_frame_interp_filter(rb);
      cm->switchable_motion_mode = aom_rb_read_bit(rb);
#endif
    }

    cm->prev_frame = get_primary_ref_frame_buf(cm);
    if (cm->primary_ref_frame != PRIMARY_REF_NONE &&
        get_primary_ref_frame_buf(cm) == NULL) {
      aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                         "Reference frame containing this frame's initial "
                         "frame context is unavailable.");
    }
#if 0
    av1_print2(AV1_DEBUG_BUFMGR_DETAIL, "%d,%d,%d,%d\n",cm->error_resilient_mode,
      cm->seq_params.order_hint_info.enable_ref_frame_mvs,
      cm->seq_params.order_hint_info.enable_order_hint,frame_is_intra_only(cm));

    printf("frame_might_allow_ref_frame_mvs()=>%d, current_frame->frame_type=%d, pbi->need_resync=%d, params->p.allow_ref_frame_mvs=%d\n",
        frame_might_allow_ref_frame_mvs(cm), current_frame->frame_type, pbi->need_resync,
        params->p.allow_ref_frame_mvs);
#endif
    if (!(current_frame->frame_type == INTRA_ONLY_FRAME) &&
        pbi->need_resync != 1) {
      if (frame_might_allow_ref_frame_mvs(cm))
        cm->allow_ref_frame_mvs = params->p.allow_ref_frame_mvs; //aom_rb_read_bit(-1, "<allow_ref_frame_mvs>", rb);
      else
        cm->allow_ref_frame_mvs = 0;

#ifdef SUPPORT_SCALE_FACTOR
      for (i = LAST_FRAME; i <= ALTREF_FRAME; ++i) {
        const RefCntBuffer *const ref_buf = get_ref_frame_buf(cm, i);
        struct scale_factors *const ref_scale_factors =
            get_ref_scale_factors(cm, i);
       if (ref_buf != NULL) {
#ifdef AML
        av1_setup_scale_factors_for_frame(
            ref_scale_factors, ref_buf->buf.y_crop_width,
            ref_buf->buf.y_crop_height, cm->dec_width, cm->height);
#else
        av1_setup_scale_factors_for_frame(
            ref_scale_factors, ref_buf->buf.y_crop_width,
            ref_buf->buf.y_crop_height, cm->width, cm->height);
#endif
      }
       if (ref_scale_factors) {
        if ((!av1_is_valid_scale(ref_scale_factors)))
          aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                             "Reference frame has invalid dimensions");
       }
      }
#endif
    }
  }

  av1_setup_frame_buf_refs(cm);

  av1_setup_frame_sign_bias(cm);

  cm->cur_frame->frame_type = current_frame->frame_type;

  if (seq_params->frame_id_numbers_present_flag) {
    update_ref_frame_id(cm, cm->current_frame_id);
  }
#ifdef ORI_CODE
  const int might_bwd_adapt =
      !(seq_params->reduced_still_picture_hdr) && !(cm->disable_cdf_update);
  if (might_bwd_adapt) {
    cm->refresh_frame_context = aom_rb_read_bit(rb)
                                    ? REFRESH_FRAME_CONTEXT_DISABLED
                                    : REFRESH_FRAME_CONTEXT_BACKWARD;
  } else {
    cm->refresh_frame_context = REFRESH_FRAME_CONTEXT_DISABLED;
  }
#endif

  cm->cur_frame->buf.bit_depth = seq_params->bit_depth;
  cm->cur_frame->buf.color_primaries = seq_params->color_primaries;
  cm->cur_frame->buf.transfer_characteristics =
      seq_params->transfer_characteristics;
  cm->cur_frame->buf.matrix_coefficients = seq_params->matrix_coefficients;
  cm->cur_frame->buf.monochrome = seq_params->monochrome;
  cm->cur_frame->buf.chroma_sample_position =
      seq_params->chroma_sample_position;
  cm->cur_frame->buf.color_range = seq_params->color_range;
  cm->cur_frame->buf.render_width = cm->render_width;
  cm->cur_frame->buf.render_height = cm->render_height;

  if (pbi->need_resync) {
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Keyframe / intra-only frame required to reset decoder"
                       " state");
  }

  generate_next_ref_frame_map(pbi);

#ifdef ORI_CODE
  if (cm->allow_intrabc) {
    // Set parameters corresponding to no filtering.
    struct loopfilter *lf = &cm->lf;
    lf->filter_level[0] = 0;
    lf->filter_level[1] = 0;
    cm->cdef_info.cdef_bits = 0;
    cm->cdef_info.cdef_strengths[0] = 0;
    cm->cdef_info.nb_cdef_strengths = 1;
    cm->cdef_info.cdef_uv_strengths[0] = 0;
    cm->rst_info[0].frame_restoration_type = RESTORE_NONE;
    cm->rst_info[1].frame_restoration_type = RESTORE_NONE;
    cm->rst_info[2].frame_restoration_type = RESTORE_NONE;
  }

  read_tile_info(pbi, rb);
  if (!av1_is_min_tile_width_satisfied(cm)) {
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Minimum tile width requirement not satisfied");
  }

  setup_quantization(cm, rb);
  xd->bd = (int)seq_params->bit_depth;

  if (cm->num_allocated_above_context_planes < av1_num_planes(cm) ||
      cm->num_allocated_above_context_mi_col < cm->mi_cols ||
      cm->num_allocated_above_contexts < cm->tile_rows) {
    av1_free_above_context_buffers(cm, cm->num_allocated_above_contexts);
    if (av1_alloc_above_context_buffers(cm, cm->tile_rows))
      aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                         "Failed to allocate context buffers");
  }

  if (cm->primary_ref_frame == PRIMARY_REF_NONE) {
    av1_setup_past_independence(cm);
  }

  setup_segmentation(cm, params);

  cm->delta_q_info.delta_q_res = 1;
  cm->delta_q_info.delta_lf_res = 1;
  cm->delta_q_info.delta_lf_present_flag = 0;
  cm->delta_q_info.delta_lf_multi = 0;
  cm->delta_q_info.delta_q_present_flag =
      cm->base_qindex > 0 ? aom_rb_read_bit(-1, defmark, rb) : 0;
  if (cm->delta_q_info.delta_q_present_flag) {
    xd->current_qindex = cm->base_qindex;
    cm->delta_q_info.delta_q_res = 1 << aom_rb_read_literal(-1, defmark, rb, 2);
    if (!cm->allow_intrabc)
      cm->delta_q_info.delta_lf_present_flag = aom_rb_read_bit(-1, defmark, rb);
    if (cm->delta_q_info.delta_lf_present_flag) {
      cm->delta_q_info.delta_lf_res = 1 << aom_rb_read_literal(-1, defmark, rb, 2);
      cm->delta_q_info.delta_lf_multi = aom_rb_read_bit(-1, defmark, rb);
      av1_reset_loop_filter_delta(xd, av1_num_planes(cm));
    }
  }

  xd->cur_frame_force_integer_mv = cm->cur_frame_force_integer_mv;

  for (int i = 0; i < MAX_SEGMENTS; ++i) {
    const int qindex = av1_get_qindex(&cm->seg, i, cm->base_qindex);
    xd->lossless[i] = qindex == 0 && cm->y_dc_delta_q == 0 &&
                      cm->u_dc_delta_q == 0 && cm->u_ac_delta_q == 0 &&
                      cm->v_dc_delta_q == 0 && cm->v_ac_delta_q == 0;
    xd->qindex[i] = qindex;
  }
  cm->coded_lossless = is_coded_lossless(cm, xd);
  cm->all_lossless = cm->coded_lossless && !av1_superres_scaled(cm);
  setup_segmentation_dequant(cm, xd);
  if (cm->coded_lossless) {
    cm->lf.filter_level[0] = 0;
    cm->lf.filter_level[1] = 0;
  }
  if (cm->coded_lossless || !seq_params->enable_cdef) {
    cm->cdef_info.cdef_bits = 0;
    cm->cdef_info.cdef_strengths[0] = 0;
    cm->cdef_info.cdef_uv_strengths[0] = 0;
  }
  if (cm->all_lossless || !seq_params->enable_restoration) {
    cm->rst_info[0].frame_restoration_type = RESTORE_NONE;
    cm->rst_info[1].frame_restoration_type = RESTORE_NONE;
    cm->rst_info[2].frame_restoration_type = RESTORE_NONE;
  }
  setup_loopfilter(cm, rb);

  if (!cm->coded_lossless && seq_params->enable_cdef) {
    setup_cdef(cm, rb);
  }
  if (!cm->all_lossless && seq_params->enable_restoration) {
    decode_restoration_mode(cm, rb);
  }

  cm->tx_mode = read_tx_mode(cm, rb);
#endif

  current_frame->reference_mode = read_frame_reference_mode(cm, params);

#ifdef ORI_CODE
  if (current_frame->reference_mode != SINGLE_REFERENCE)
    setup_compound_reference_mode(cm);


#endif

  av1_setup_skip_mode_allowed(cm);

  /*
    the point that ucode send send_bufmgr_info
    and wait bufmgr code to return is_skip_mode_allowed
  */

  /*
  read_uncompressed_header() end
  */

  av1_setup_motion_field(cm);
#ifdef AML
  cm->cur_frame->mi_cols = cm->mi_cols;
  cm->cur_frame->mi_rows = cm->mi_rows;
  cm->cur_frame->dec_width = cm->dec_width;

  /*
  superres_post_decode(AV1Decoder *pbi) =>
    av1_superres_upscale(cm, pool); =>
      aom_realloc_frame_buffer(
            frame_to_show, cm->superres_upscaled_width,
            cm->superres_upscaled_height, seq_params->subsampling_x,
            seq_params->subsampling_y, seq_params->use_highbitdepth,
            AOM_BORDER_IN_PIXELS, cm->byte_alignment, fb, cb, cb_priv)
  */
  aom_realloc_frame_buffer(cm, &cm->cur_frame->buf,
    cm->superres_upscaled_width, cm->superres_upscaled_height,
    cm->cur_frame->order_hint);
#endif
  return 0;
}

static int are_seq_headers_consistent(const SequenceHeader *seq_params_old,
                                      const SequenceHeader *seq_params_new) {
  return !memcmp(seq_params_old, seq_params_new, sizeof(SequenceHeader));
}

aom_codec_err_t aom_get_num_layers_from_operating_point_idc(
    int operating_point_idc, unsigned int *number_spatial_layers,
    unsigned int *number_temporal_layers) {
  // derive number of spatial/temporal layers from operating_point_idc

  if (!number_spatial_layers || !number_temporal_layers)
    return AOM_CODEC_INVALID_PARAM;

  if (operating_point_idc == 0) {
    *number_temporal_layers = 1;
    *number_spatial_layers = 1;
  } else {
    int j;
    *number_spatial_layers = 0;
    *number_temporal_layers = 0;
    for (j = 0; j < MAX_NUM_SPATIAL_LAYERS; j++) {
      *number_spatial_layers +=
          (operating_point_idc >> (j + MAX_NUM_TEMPORAL_LAYERS)) & 0x1;
    }
    for (j = 0; j < MAX_NUM_TEMPORAL_LAYERS; j++) {
      *number_temporal_layers += (operating_point_idc >> j) & 0x1;
    }
  }

  return AOM_CODEC_OK;
}

void av1_read_sequence_header(AV1_COMMON *cm, union param_u *params,
                              SequenceHeader *seq_params) {
#ifdef ORI_CODE
  const int num_bits_width = aom_rb_read_literal(-1, "<num_bits_width>", rb, 4) + 1;
  const int num_bits_height = aom_rb_read_literal(-1, "<num_bits_height>", rb, 4) + 1;
  const int max_frame_width = aom_rb_read_literal(-1, "<max_frame_width>", rb, num_bits_width) + 1;
  const int max_frame_height = aom_rb_read_literal(-1, "<max_frame_height>", rb, num_bits_height) + 1;

  seq_params->num_bits_width = num_bits_width;
  seq_params->num_bits_height = num_bits_height;
#endif
  seq_params->max_frame_width = params->p.max_frame_width; //max_frame_width;
  seq_params->max_frame_height = params->p.max_frame_height; //max_frame_height;

  if (seq_params->reduced_still_picture_hdr) {
    seq_params->frame_id_numbers_present_flag = 0;
  } else {
    seq_params->frame_id_numbers_present_flag = params->p.frame_id_numbers_present_flag; //aom_rb_read_bit(-1, "<frame_id_numbers_present_flag>", rb);
  }
  if (seq_params->frame_id_numbers_present_flag) {
    // We must always have delta_frame_id_length < frame_id_length,
    // in order for a frame to be referenced with a unique delta.
    // Avoid wasting bits by using a coding that enforces this restriction.
#ifdef ORI_CODE
    seq_params->delta_frame_id_length = aom_rb_read_literal(-1, "<delta_frame_id_length>", rb, 4) + 2;
    seq_params->frame_id_length = params->p.frame_id_length  + aom_rb_read_literal(-1, "<frame_id_length>", rb, 3) + seq_params->delta_frame_id_length + 1;
#else
    seq_params->delta_frame_id_length = params->p.delta_frame_id_length;
    seq_params->frame_id_length = params->p.frame_id_length  + seq_params->delta_frame_id_length + 1;
#endif
    if (seq_params->frame_id_length > 16)
      aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                         "Invalid frame_id_length");
  }
#ifdef ORI_CODE
  setup_sb_size(seq_params, rb);
  seq_params->enable_filter_intra = aom_rb_read_bit(-1, "<enable_filter_intra>", rb);
  seq_params->enable_intra_edge_filter = aom_rb_read_bit(-1, "<enable_intra_edge_filter>", rb);
#endif

  if (seq_params->reduced_still_picture_hdr) {
    seq_params->enable_interintra_compound = 0;
    seq_params->enable_masked_compound = 0;
    seq_params->enable_warped_motion = 0;
    seq_params->enable_dual_filter = 0;
    seq_params->order_hint_info.enable_order_hint = 0;
    seq_params->order_hint_info.enable_dist_wtd_comp = 0;
    seq_params->order_hint_info.enable_ref_frame_mvs = 0;
    seq_params->force_screen_content_tools = 2;  // SELECT_SCREEN_CONTENT_TOOLS
    seq_params->force_integer_mv = 2;            // SELECT_INTEGER_MV
    seq_params->order_hint_info.order_hint_bits_minus_1 = -1;
  } else {
#ifdef ORI_CODE
    seq_params->enable_interintra_compound = aom_rb_read_bit(-1, "<enable_interintra_compound>", rb);
    seq_params->enable_masked_compound = aom_rb_read_bit(-1, "<enable_masked_compound>", rb);
    seq_params->enable_warped_motion = aom_rb_read_bit(-1, "<enable_warped_motion>", rb);
    seq_params->enable_dual_filter = aom_rb_read_bit(-1, "<enable_dual_filter>", rb);
#endif
    seq_params->order_hint_info.enable_order_hint = params->p.enable_order_hint; //aom_rb_read_bit(-1, "<order_hint_info.enable_order_hint>", rb);
    seq_params->order_hint_info.enable_dist_wtd_comp =
        seq_params->order_hint_info.enable_order_hint ? params->p.enable_dist_wtd_comp : 0; //aom_rb_read_bit(-1, "<order_hint_info.enable_dist_wtd_comp>", rb) : 0;
    seq_params->order_hint_info.enable_ref_frame_mvs =
        seq_params->order_hint_info.enable_order_hint ? params->p.enable_ref_frame_mvs : 0; //aom_rb_read_bit(-1, "<order_hint_info.enable_ref_frame_mvs>", rb) : 0;

#ifdef ORI_CODE
    if (aom_rb_read_bit(-1, defmark, rb)) {
      seq_params->force_screen_content_tools =
          2;  // SELECT_SCREEN_CONTENT_TOOLS
    } else {
      seq_params->force_screen_content_tools = aom_rb_read_bit(-1, defmark, rb);
    }

    if (seq_params->force_screen_content_tools > 0) {
      if (aom_rb_read_bit(-1, defmark, rb)) {
        seq_params->force_integer_mv = 2;  // SELECT_INTEGER_MV
      } else {
        seq_params->force_integer_mv = aom_rb_read_bit(-1, defmark, rb);
      }
    } else {
      seq_params->force_integer_mv = 2;  // SELECT_INTEGER_MV
    }
#endif
    seq_params->order_hint_info.order_hint_bits_minus_1 =
        seq_params->order_hint_info.enable_order_hint
            ? params->p.order_hint_bits_minus_1 /*aom_rb_read_literal(-1, "<order_hint_info.order_hint_bits_minus_1>", rb, 3)*/
            : -1;
  }
  seq_params->enable_superres = params->p.enable_superres; //aom_rb_read_bit(-1, defmark, rb);

#ifdef ORI_CODE
  seq_params->enable_cdef = aom_rb_read_bit(-1, defmark, rb);
  seq_params->enable_restoration = aom_rb_read_bit(-1, defmark, rb);
#endif
}

#ifdef ORI_CODE
void av1_read_op_parameters_info(AV1_COMMON *const cm,
                                 struct aom_read_bit_buffer *rb, int op_num) {
  // The cm->op_params array has MAX_NUM_OPERATING_POINTS + 1 elements.
  if (op_num > MAX_NUM_OPERATING_POINTS) {
    aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                       "AV1 does not support %d decoder model operating points",
                       op_num + 1);
  }

  cm->op_params[op_num].decoder_buffer_delay = aom_rb_read_unsigned_literal(-1, defmark,
      rb, cm->buffer_model.encoder_decoder_buffer_delay_length);

  cm->op_params[op_num].encoder_buffer_delay = aom_rb_read_unsigned_literal(-1, defmark,
      rb, cm->buffer_model.encoder_decoder_buffer_delay_length);

  cm->op_params[op_num].low_delay_mode_flag = aom_rb_read_bit(-1, defmark, rb);
}
#endif

static int is_valid_seq_level_idx(AV1_LEVEL seq_level_idx) {
	return seq_level_idx < SEQ_LEVELS || seq_level_idx == SEQ_LEVEL_MAX;
}

static uint32_t read_sequence_header_obu(AV1Decoder *pbi,
                                         union param_u *params) {
  AV1_COMMON *const cm = pbi->common;
  int i;
  int operating_point;
  // Verify rb has been configured to report errors.
  //assert(rb->error_handler);

  // Use a local variable to store the information as we decode. At the end,
  // if no errors have occurred, cm->seq_params is updated.
  SequenceHeader sh = cm->seq_params;
  SequenceHeader *const seq_params = &sh;

  seq_params->profile = params->p.profile; //av1_read_profile(rb);
  if (seq_params->profile > CONFIG_MAX_DECODE_PROFILE) {
    cm->error.error_code = AOM_CODEC_UNSUP_BITSTREAM;
    return 0;
  }

  // Still picture or not
  seq_params->still_picture = params->p.still_picture; //aom_rb_read_bit(-1, "<still_picture>", rb);
  seq_params->reduced_still_picture_hdr = params->p.reduced_still_picture_hdr; //aom_rb_read_bit(-1, "<reduced_still_picture_hdr>", rb);
  // Video must have reduced_still_picture_hdr = 0
  if (!seq_params->still_picture && seq_params->reduced_still_picture_hdr) {
    cm->error.error_code = AOM_CODEC_UNSUP_BITSTREAM;
    return 0;
  }

  if (seq_params->reduced_still_picture_hdr) {
    cm->timing_info_present = 0;
    seq_params->decoder_model_info_present_flag = 0;
    seq_params->display_model_info_present_flag = 0;
    seq_params->operating_points_cnt_minus_1 = 0;
    seq_params->operating_point_idc[0] = 0;
    //if (!read_bitstream_level(0, "<seq_level_idx>", &seq_params->seq_level_idx[0], rb)) {
    if (!is_valid_seq_level_idx(params->p.seq_level_idx[0])) {
      cm->error.error_code = AOM_CODEC_UNSUP_BITSTREAM;
      return 0;
    }
    seq_params->tier[0] = 0;
    cm->op_params[0].decoder_model_param_present_flag = 0;
    cm->op_params[0].display_model_param_present_flag = 0;
  } else {
    cm->timing_info_present = params->p.timing_info_present; //aom_rb_read_bit(-1, "<timing_info_present>", rb);  // timing_info_present_flag
    if (cm->timing_info_present) {
#ifdef ORI_CODE
      av1_read_timing_info_header(cm, rb);
#endif
      seq_params->decoder_model_info_present_flag = params->p.decoder_model_info_present_flag; //aom_rb_read_bit(-1, "<decoder_model_info_present_flag>", rb);
#ifdef ORI_CODE
      if (seq_params->decoder_model_info_present_flag)
        av1_read_decoder_model_info(cm, rb);
#endif
    } else {
      seq_params->decoder_model_info_present_flag = 0;
    }
#ifdef ORI_CODE
    seq_params->display_model_info_present_flag = aom_rb_read_bit(-1, "<display_model_info_present_flag>", rb);
#endif
    seq_params->operating_points_cnt_minus_1 = params->p.operating_points_cnt_minus_1;
        //aom_rb_read_literal(-1, "<operating_points_cnt_minus_1>", rb, OP_POINTS_CNT_MINUS_1_BITS);
    for (i = 0; i < seq_params->operating_points_cnt_minus_1 + 1; i++) {
      seq_params->operating_point_idc[i] = params->p.operating_point_idc[i];
          //aom_rb_read_literal(i, "<operating_point_idc>", rb, OP_POINTS_IDC_BITS);
      //if (!read_bitstream_level(i, "<seq_level_idx>", &seq_params->seq_level_idx[i], rb)) {
      if (!is_valid_seq_level_idx(params->p.seq_level_idx[i])) {
        cm->error.error_code = AOM_CODEC_UNSUP_BITSTREAM;
        return 0;
      }
      // This is the seq_level_idx[i] > 7 check in the spec. seq_level_idx 7
      // is equivalent to level 3.3.
#ifdef ORI_CODE
      if (seq_params->seq_level_idx[i] >= SEQ_LEVEL_4_0)
        seq_params->tier[i] = aom_rb_read_bit(i, "<tier>", rb);
      else
        seq_params->tier[i] = 0;
#endif
      if (seq_params->decoder_model_info_present_flag) {
        cm->op_params[i].decoder_model_param_present_flag = params->p.decoder_model_param_present_flag[i]; //aom_rb_read_bit(-1, defmark, rb);
#ifdef ORI_CODE
        if (cm->op_params[i].decoder_model_param_present_flag)
          av1_read_op_parameters_info(cm, rb, i);
#endif
      } else {
        cm->op_params[i].decoder_model_param_present_flag = 0;
      }
#ifdef ORI_CODE
      if (cm->timing_info_present &&
          (cm->timing_info.equal_picture_interval ||
           cm->op_params[i].decoder_model_param_present_flag)) {
        cm->op_params[i].bitrate = av1_max_level_bitrate(
            seq_params->profile, seq_params->seq_level_idx[i],
            seq_params->tier[i]);
        // Level with seq_level_idx = 31 returns a high "dummy" bitrate to pass
        // the check
        if (cm->op_params[i].bitrate == 0)
          aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                             "AV1 does not support this combination of "
                             "profile, level, and tier.");
        // Buffer size in bits/s is bitrate in bits/s * 1 s
        cm->op_params[i].buffer_size = cm->op_params[i].bitrate;
      }
#endif
      if (cm->timing_info_present && cm->timing_info.equal_picture_interval &&
          !cm->op_params[i].decoder_model_param_present_flag) {
        // When the decoder_model_parameters are not sent for this op, set
        // the default ones that can be used with the resource availability mode
        cm->op_params[i].decoder_buffer_delay = 70000;
        cm->op_params[i].encoder_buffer_delay = 20000;
        cm->op_params[i].low_delay_mode_flag = 0;
      }

#ifdef ORI_CODE
      if (seq_params->display_model_info_present_flag) {
        cm->op_params[i].display_model_param_present_flag = aom_rb_read_bit(-1, defmark, rb);
        if (cm->op_params[i].display_model_param_present_flag) {
          cm->op_params[i].initial_display_delay =
              aom_rb_read_literal(-1, defmark, rb, 4) + 1;
          if (cm->op_params[i].initial_display_delay > 10)
            aom_internal_error(
                &cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                "AV1 does not support more than 10 decoded frames delay");
        } else {
          cm->op_params[i].initial_display_delay = 10;
        }
      } else {
        cm->op_params[i].display_model_param_present_flag = 0;
        cm->op_params[i].initial_display_delay = 10;
      }
#endif
    }
  }
  // This decoder supports all levels.  Choose operating point provided by
  // external means
  operating_point = pbi->operating_point;
  if (operating_point < 0 ||
      operating_point > seq_params->operating_points_cnt_minus_1)
    operating_point = 0;
  pbi->current_operating_point =
      seq_params->operating_point_idc[operating_point];
  if (aom_get_num_layers_from_operating_point_idc(
          pbi->current_operating_point, &cm->number_spatial_layers,
          &cm->number_temporal_layers) != AOM_CODEC_OK) {
    cm->error.error_code = AOM_CODEC_ERROR;
    return 0;
  }

  av1_read_sequence_header(cm, params, seq_params);
#ifdef ORI_CODE
  av1_read_color_config(rb, pbi->allow_lowbitdepth, seq_params, &cm->error);
  if (!(seq_params->subsampling_x == 0 && seq_params->subsampling_y == 0) &&
      !(seq_params->subsampling_x == 1 && seq_params->subsampling_y == 1) &&
      !(seq_params->subsampling_x == 1 && seq_params->subsampling_y == 0)) {
    aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                       "Only 4:4:4, 4:2:2 and 4:2:0 are currently supported, "
                       "%d %d subsampling is not supported.\n",
                       seq_params->subsampling_x, seq_params->subsampling_y);
  }
  seq_params->film_grain_params_present = aom_rb_read_bit(-1, "<film_grain_params_present>", rb);

  if (av1_check_trailing_bits(pbi, rb) != 0) {
    // cm->error.error_code is already set.
    return 0;
  }
#endif

  // If a sequence header has been decoded before, we check if the new
  // one is consistent with the old one.
  if (pbi->sequence_header_ready) {
    if (!are_seq_headers_consistent(&cm->seq_params, seq_params))
      pbi->sequence_header_changed = 1;
  }

  cm->seq_params = *seq_params;
  pbi->sequence_header_ready = 1;
  return 0;

}

int aom_decode_frame_from_obus(AV1Decoder *pbi, union param_u *params, int obu_type)
{
  AV1_COMMON *const cm = pbi->common;
  ObuHeader obu_header;
 int frame_decoding_finished = 0;
  uint32_t frame_header_size = 0;

    //struct aom_read_bit_buffer rb;
    size_t payload_size = 0;
    size_t decoded_payload_size = 0;
    size_t obu_payload_offset = 0;
    //size_t bytes_read = 0;

  memset(&obu_header, 0, sizeof(obu_header));
#ifdef ORI_CODE
  pbi->seen_frame_header = 0;
#else
  /* set in the test.c*/
#endif

  obu_header.type = obu_type;
  pbi->cur_obu_type = obu_header.type;
  if (av1_is_debug(AOM_DEBUG_PRINT_LIST_INFO))
    dump_params(pbi, params);
  switch (obu_header.type) {
    case OBU_SEQUENCE_HEADER:
        decoded_payload_size = read_sequence_header_obu(pbi, params);
        if (cm->error.error_code != AOM_CODEC_OK) return -1;
        break;

    case OBU_FRAME_HEADER:
    case OBU_REDUNDANT_FRAME_HEADER:
    case OBU_FRAME:
        if (obu_header.type == OBU_REDUNDANT_FRAME_HEADER) {
          if (!pbi->seen_frame_header) {
            cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
            return -1;
          }
        } else {
          // OBU_FRAME_HEADER or OBU_FRAME.
          if (pbi->seen_frame_header) {
            cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
            return -1;
          }
        }
        // Only decode first frame header received
        if (!pbi->seen_frame_header ||
            (cm->large_scale_tile && !pbi->camera_frame_header_ready)) {
          frame_header_size = av1_decode_frame_headers_and_setup(
              pbi, /*&rb, data, p_data_end,*/obu_header.type != OBU_FRAME, params);
          pbi->seen_frame_header = 1;
          if (!pbi->ext_tile_debug && cm->large_scale_tile)
            pbi->camera_frame_header_ready = 1;
        } else {
          // TODO(wtc): Verify that the frame_header_obu is identical to the
          // original frame_header_obu. For now just skip frame_header_size
          // bytes in the bit buffer.
          if (frame_header_size > payload_size) {
            cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
            return -1;
          }
          assert(rb.bit_offset == 0);
#ifdef ORI_CODE
          rb.bit_offset = 8 * frame_header_size;
#endif
        }

        decoded_payload_size = frame_header_size;
        pbi->frame_header_size = frame_header_size;

        if (cm->show_existing_frame) {
          if (obu_header.type == OBU_FRAME) {
            cm->error.error_code = AOM_CODEC_UNSUP_BITSTREAM;
            return -1;
          }
          frame_decoding_finished = 1;
          pbi->seen_frame_header = 0;
          break;
        }

        // In large scale tile coding, decode the common camera frame header
        // before any tile list OBU.
        if (!pbi->ext_tile_debug && pbi->camera_frame_header_ready) {
          frame_decoding_finished = 1;
          // Skip the rest of the frame data.
          decoded_payload_size = payload_size;
          // Update data_end.
#ifdef ORI_CODE
          *p_data_end = data_end;
#endif
          break;
        }
#if 0 //def AML
        frame_decoding_finished = 1;
#endif
        if (obu_header.type != OBU_FRAME) break;
        obu_payload_offset = frame_header_size;
        // Byte align the reader before reading the tile group.
        // byte_alignment() has set cm->error.error_code if it returns -1.
#ifdef ORI_CODE
        if (byte_alignment(cm, &rb)) return -1;
        AOM_FALLTHROUGH_INTENDED;  // fall through to read tile group.
#endif
  default:
    break;
      }
	return frame_decoding_finished;
}

int get_buffer_index(AV1Decoder *pbi, RefCntBuffer *buffer)
{
	AV1_COMMON *const cm = pbi->common;
	int i = -1;

	if (buffer) {
		for (i = 0; i < FRAME_BUFFERS; i++) {
			RefCntBuffer *buf =
				&cm->buffer_pool->frame_bufs[i];
			if (buf == buffer) {
				break;
			}
		}
	}
	return i;
}

void dump_buffer(RefCntBuffer *buf)
{
	int i;
	pr_info("ref_count %d, vf_ref %d, order_hint %d, w/h(%d,%d) showable_frame %d frame_type %d canvas(%d,%d) w/h(%d,%d) mi_c/r(%d,%d) header 0x%x ref_deltas(",
	buf->ref_count, buf->buf.vf_ref, buf->order_hint, buf->width, buf->height, buf->showable_frame, buf->frame_type,
	buf->buf.mc_canvas_y, buf->buf.mc_canvas_u_v,
	buf->buf.y_crop_width, buf->buf.y_crop_height,
	buf->mi_cols, buf->mi_rows,
	buf->buf.header_adr);
	for (i = 0; i < REF_FRAMES; i++)
		pr_info("%d,", buf->ref_deltas[i]);
	pr_info("), ref_order_hints(");

	for (i = 0; i < INTER_REFS_PER_FRAME; i++)
		pr_info("%d ", buf->ref_order_hints[i]);
	pr_info(")");
}

void dump_ref_buffer_info(AV1Decoder *pbi, int i)
{
	AV1_COMMON *const cm = pbi->common;
	pr_info("remapped_ref_idx %d, ref_frame_sign_bias %d, ref_frame_id %d, valid_for_referencing %d ref_frame_side %d ref_frame_map idx %d, next_ref_frame_map idx %d",
		cm->remapped_ref_idx[i],
		cm->ref_frame_sign_bias[i],
		cm->ref_frame_id[i],
		cm->valid_for_referencing[i],
		cm->ref_frame_side[i],
		get_buffer_index(pbi, cm->ref_frame_map[i]),
		get_buffer_index(pbi, cm->next_ref_frame_map[i]));
}

void dump_mv_refs(AV1Decoder *pbi)
{
  int i, j;
  AV1_COMMON *const cm = pbi->common;
  for (i = 0; i < cm->mv_ref_id_index; i++) {
    pr_info("%d: ref_id %d cal_tpl_mvs %d mv_ref_offset: ",
      i, cm->mv_ref_id[i], cm->mv_cal_tpl_mvs[i]);
    for (j = 0; j < REF_FRAMES; j++)
        pr_info("%d ", cm->mv_ref_offset[i][j]);
    pr_info("\n");
  }
}

void dump_ref_spec_bufs(AV1Decoder *pbi)
{
  int i;
  AV1_COMMON *const cm = pbi->common;
  for (i = 0; i < INTER_REFS_PER_FRAME; ++i) {
    PIC_BUFFER_CONFIG *pic_config = av1_get_ref_frame_spec_buf(cm, LAST_FRAME + i);
    if (pic_config == NULL) continue;
    pr_info("%d: index %d order_hint %d header 0x%x dw_header 0x%x canvas(%d,%d) mv_wr_start 0x%x lcu_total %d\n",
      i, pic_config->index,
      pic_config->order_hint,
      pic_config->header_adr,
#ifdef AOM_AV1_MMU_DW
      pic_config->header_dw_adr,
#else
      0,
#endif
      pic_config->mc_canvas_y,
      pic_config->mc_canvas_u_v,
      pic_config->mpred_mv_wr_start_addr,
      pic_config->lcu_total
      );
  }
}

#ifdef SUPPORT_SCALE_FACTOR
void dump_scale_factors(AV1Decoder *pbi)
{
  int i;
  AV1_COMMON *const cm = pbi->common;
  for (i = LAST_FRAME; i <= ALTREF_FRAME; ++i) {
    struct scale_factors *const sf =
        get_ref_scale_factors(cm, i);
    if (sf)
      pr_info("%d: is_scaled %d x_scale_fp %d, y_scale_fp %d\n",
        i, av1_is_scaled(sf),
        sf->x_scale_fp, sf->y_scale_fp);
    else
      pr_info("%d: sf null\n", i);
  }
}

#endif

void dump_buffer_status(AV1Decoder *pbi)
{
	int i;
	AV1_COMMON *const cm = pbi->common;
	BufferPool *const pool = cm->buffer_pool;
	unsigned long flags;

	lock_buffer_pool(pool, flags);

	pr_info("%s: pbi %p cm %p cur_frame %p\n", __func__, pbi, cm, cm->cur_frame);

	pr_info("Buffer Pool:\n");
	for (i = 0; i < FRAME_BUFFERS; i++) {
		RefCntBuffer *buf =
			&cm->buffer_pool->frame_bufs[i];
		pr_info("%d: ", i);
		if (buf)
			dump_buffer(buf);
		pr_info("\n");
	}

	if (cm->prev_frame) {
		pr_info("prev_frame (%d): ",
			get_buffer_index(pbi, cm->prev_frame));
		dump_buffer(cm->prev_frame);
		pr_info("\n");
	}
	if (cm->cur_frame) {
		pr_info("cur_frame (%d): ",
			get_buffer_index(pbi, cm->cur_frame));
		dump_buffer(cm->cur_frame);
		pr_info("\n");
	}
	pr_info("REF_FRAMES Info(ref buf is ref_frame_map[remapped_ref_idx[i-1]], i=1~7):\n");
	for (i = 0; i < REF_FRAMES; i++) {
		pr_info("%d: ", i);
		dump_ref_buffer_info(pbi, i);
		pr_info("\n");
	}
	pr_info("Ref Spec Buffers:\n");
	dump_ref_spec_bufs(pbi);

	pr_info("MV refs:\n");
	dump_mv_refs(pbi);

#ifdef SUPPORT_SCALE_FACTOR
	pr_info("Scale factors:\n");
	dump_scale_factors(pbi);
#endif
	unlock_buffer_pool(pool, flags);
}


struct param_dump_item_s {
	unsigned int size;
	char* name;
	unsigned int adr_off;
} param_dump_items[] = {
	{1, "profile",                                   (unsigned long)&(((union param_u *)0)->p.profile                        )},
	{1, "still_picture",                             (unsigned long)&(((union param_u *)0)->p.still_picture                  )},
	{1, "reduced_still_picture_hdr",                 (unsigned long)&(((union param_u *)0)->p.reduced_still_picture_hdr      )},
	{1, "decoder_model_info_present_flag",           (unsigned long)&(((union param_u *)0)->p.decoder_model_info_present_flag)},
	{1, "max_frame_width",                           (unsigned long)&(((union param_u *)0)->p.max_frame_width                )},
	{1, "max_frame_height",                          (unsigned long)&(((union param_u *)0)->p.max_frame_height               )},
	{1, "frame_id_numbers_present_flag",             (unsigned long)&(((union param_u *)0)->p.frame_id_numbers_present_flag  )},
	{1, "delta_frame_id_length",                     (unsigned long)&(((union param_u *)0)->p.delta_frame_id_length          )},
	{1, "frame_id_length",                           (unsigned long)&(((union param_u *)0)->p.frame_id_length                )},
	{1, "order_hint_bits_minus_1",                   (unsigned long)&(((union param_u *)0)->p.order_hint_bits_minus_1        )},
	{1, "enable_order_hint",                         (unsigned long)&(((union param_u *)0)->p.enable_order_hint              )},
	{1, "enable_dist_wtd_comp",                      (unsigned long)&(((union param_u *)0)->p.enable_dist_wtd_comp           )},
	{1, "enable_ref_frame_mvs",                      (unsigned long)&(((union param_u *)0)->p.enable_ref_frame_mvs           )},
	{1, "enable_superres",                           (unsigned long)&(((union param_u *)0)->p.enable_superres                )},
	{1, "superres_scale_denominator",                (unsigned long)&(((union param_u *)0)->p.superres_scale_denominator     )},
	{1, "show_existing_frame",                       (unsigned long)&(((union param_u *)0)->p.show_existing_frame            )},
	{1, "frame_type",                                (unsigned long)&(((union param_u *)0)->p.frame_type                     )},
	{1, "show_frame",                                (unsigned long)&(((union param_u *)0)->p.show_frame                     )},
	{1, "e.r.r.o.r_resilient_mode",                  (unsigned long)&(((union param_u *)0)->p.error_resilient_mode           )},
	{1, "refresh_frame_flags",                       (unsigned long)&(((union param_u *)0)->p.refresh_frame_flags            )},
	{1, "showable_frame",                            (unsigned long)&(((union param_u *)0)->p.showable_frame                 )},
	{1, "current_frame_id",                          (unsigned long)&(((union param_u *)0)->p.current_frame_id               )},
	{1, "frame_size_override_flag",                  (unsigned long)&(((union param_u *)0)->p.frame_size_override_flag       )},
	{1, "order_hint",                                (unsigned long)&(((union param_u *)0)->p.order_hint                     )},
	{1, "primary_ref_frame",                         (unsigned long)&(((union param_u *)0)->p.primary_ref_frame              )},
	{1, "frame_refs_short_signaling",                (unsigned long)&(((union param_u *)0)->p.frame_refs_short_signaling     )},
	{1, "frame_width",                               (unsigned long)&(((union param_u *)0)->p.frame_width                    )},
	{1, "dec_frame_width",                           (unsigned long)&(((union param_u *)0)->p.dec_frame_width                )},
	{1, "frame_width_scaled",                        (unsigned long)&(((union param_u *)0)->p.frame_width_scaled             )},
	{1, "frame_height",                              (unsigned long)&(((union param_u *)0)->p.frame_height                   )},
	{1, "reference_mode",                            (unsigned long)&(((union param_u *)0)->p.reference_mode                 )},
	{1, "update_parameters",                         (unsigned long)&(((union param_u *)0)->p.update_parameters              )},
	{1, "film_grain_params_ref_idx",                 (unsigned long)&(((union param_u *)0)->p.film_grain_params_ref_idx      )},
	{1, "allow_ref_frame_mvs",                       (unsigned long)&(((union param_u *)0)->p.allow_ref_frame_mvs            )},
	{1, "lst_ref",                                   (unsigned long)&(((union param_u *)0)->p.lst_ref                        )},
	{1, "gld_ref",                                   (unsigned long)&(((union param_u *)0)->p.gld_ref                        )},
	{INTER_REFS_PER_FRAME, "remapped_ref_idx",       (unsigned long)&(((union param_u *)0)->p.remapped_ref_idx[0]            )},
	{INTER_REFS_PER_FRAME, "delta_frame_id_minus_1", (unsigned long)&(((union param_u *)0)->p.delta_frame_id_minus_1[0]      )},
	{REF_FRAMES, "ref_order_hint",                   (unsigned long)&(((union param_u *)0)->p.ref_order_hint[0]              )},
};

void dump_params(AV1Decoder *pbi, union param_u *params)
{
	int i, j;
	unsigned char *start_adr = (unsigned char*)params;

	pr_info("============ params:\n");
	for (i = 0; i < sizeof(param_dump_items) / sizeof(param_dump_items[0]); i++) {
		for (j = 0; j < param_dump_items[i].size; j++) {
			if (param_dump_items[i].size > 1)
				pr_info("%s(%d): 0x%x\n",
				param_dump_items[i].name, j,
				*((unsigned short*)(start_adr + param_dump_items[i].adr_off + j * 2)));
			else
				pr_info("%s: 0x%x\n", param_dump_items[i].name,
				*((unsigned short*)(start_adr + param_dump_items[i].adr_off + j * 2)));
		}
	}
}

/*static void raw_write_image(AV1Decoder *pbi, PIC_BUFFER_CONFIG *sd)
{
  printf("$$$$$$$ output image\n");
}*/

/*
  return 0, need decoding data
  1, decoding done
  -1, decoding error

*/
int av1_bufmgr_process(AV1Decoder *pbi, union param_u *params,
  unsigned char new_compressed_data, int obu_type)
{
  AV1_COMMON *const cm = pbi->common;
  int j;
  // Release any pending output frames from the previous decoder_decode call.
  // We need to do this even if the decoder is being flushed or the input
  // arguments are invalid.
  BufferPool *const pool = cm->buffer_pool;
  int frame_decoded;
  av1_print2(AV1_DEBUG_BUFMGR_DETAIL, "%s: pbi %p cm %p cur_frame %p\n", __func__, pbi, cm, cm->cur_frame);
  av1_print2(AV1_DEBUG_BUFMGR_DETAIL, "%s: new_compressed_data= %d\n", __func__, new_compressed_data);
  for (j = 0; j < pbi->num_output_frames; j++) {
    decrease_ref_count(pbi, pbi->output_frames[j], pool);
  }
  pbi->num_output_frames = 0;
  //
  if (new_compressed_data) {
    if (assign_cur_frame_new_fb(cm) == NULL) {
      cm->error.error_code = AOM_CODEC_MEM_ERROR;
      return -1;
    }
    pbi->seen_frame_header = 0;
    av1_print2(AV1_DEBUG_BUFMGR_DETAIL, "New_compressed_data (%d)\n", new_compressed_data_count++);

  }

  frame_decoded =
      aom_decode_frame_from_obus(pbi, params, obu_type);

  if (pbi->cur_obu_type == OBU_FRAME_HEADER ||
          pbi->cur_obu_type == OBU_REDUNDANT_FRAME_HEADER ||
          pbi->cur_obu_type == OBU_FRAME) {
      if (av1_is_debug(AOM_DEBUG_PRINT_LIST_INFO)) {
        pr_info("after bufmgr (frame_decoded %d seen_frame_header %d): ",
          frame_decoded, pbi->seen_frame_header);
        dump_buffer_status(pbi);
      }
  }
  av1_print2(AV1_DEBUG_BUFMGR_DETAIL, "%s: pbi %p cm %p cur_frame %p\n", __func__, pbi, cm, cm->cur_frame);

  return frame_decoded;

}

int av1_get_raw_frame(AV1Decoder *pbi, size_t index, PIC_BUFFER_CONFIG **sd) {
  if (index >= pbi->num_output_frames) return -1;
  *sd = &pbi->output_frames[index]->buf;
  //*grain_params = &pbi->output_frames[index]->film_grain_params;
  //aom_clear_system_state();
  return 0;
}

int av1_bufmgr_postproc(AV1Decoder *pbi, unsigned char frame_decoded)
{
    PIC_BUFFER_CONFIG *sd = NULL;
    int index;
#if 0
    if (frame_decoded) {
      printf("before swap_frame_buffers: ");
      dump_buffer_status(pbi);
    }
#endif
    swap_frame_buffers(pbi, frame_decoded);
    if (frame_decoded) {
      if (av1_is_debug(AOM_DEBUG_PRINT_LIST_INFO)) {
        pr_info("after swap_frame_buffers: ");
        dump_buffer_status(pbi);
      }
    }
    if (frame_decoded) {
      pbi->decoding_first_frame = 0;
    }


    for (index = 0;;index++) {
      if (av1_get_raw_frame(pbi, index, &sd) < 0)
          break;
      if (sd)
	      av1_raw_write_image(pbi, sd);
    }
    return 0;
}

int aom_realloc_frame_buffer(AV1_COMMON *cm, PIC_BUFFER_CONFIG *pic,
  int width, int height, unsigned int order_hint)
{
  av1_print2(AV1_DEBUG_BUFMGR_DETAIL, "%s, index 0x%x, width 0x%x, height 0x%x order_hint 0x%x\n",
    __func__, pic->index, width, height, order_hint);
  pic->y_crop_width = width;
  pic->y_crop_height = height;
  pic->order_hint = order_hint;
  return 0;
}


unsigned char av1_frame_is_inter(const AV1_COMMON *const cm) {
  unsigned char is_inter = cm->cur_frame && (cm->cur_frame->frame_type != KEY_FRAME)
     && (cm->current_frame.frame_type != INTRA_ONLY_FRAME);
  return is_inter;
}

PIC_BUFFER_CONFIG *av1_get_ref_frame_spec_buf(
    const AV1_COMMON *const cm, const MV_REFERENCE_FRAME ref_frame) {
  RefCntBuffer *buf = get_ref_frame_buf(cm, ref_frame);
  if (buf) {
    buf->buf.order_hint = buf->order_hint;
    return &(buf->buf);
  }
  return NULL;
}

struct scale_factors *av1_get_ref_scale_factors(
  AV1_COMMON *const cm, const MV_REFERENCE_FRAME ref_frame)
{
  return get_ref_scale_factors(cm, ref_frame);
}

void av1_set_next_ref_frame_map(AV1Decoder *pbi) {
  int ref_index = 0;
  int mask;
  AV1_COMMON *const cm = pbi->common;
  int check_on_show_existing_frame;
  av1_print2(AV1_DEBUG_BUFMGR_DETAIL, "%s, %d, mask 0x%x, show_existing_frame %d, reset_decoder_state %d\n",
    __func__, pbi->camera_frame_header_ready,
    cm->current_frame.refresh_frame_flags,
    cm->show_existing_frame,
    pbi->reset_decoder_state
    );
  if (!pbi->camera_frame_header_ready) {
    for (mask = cm->current_frame.refresh_frame_flags; mask; mask >>= 1) {
      cm->next_used_ref_frame_map[ref_index] = cm->next_ref_frame_map[ref_index];
      ++ref_index;
    }

    check_on_show_existing_frame =
        !cm->show_existing_frame || pbi->reset_decoder_state;
    for (; ref_index < REF_FRAMES && check_on_show_existing_frame;
         ++ref_index) {
      cm->next_used_ref_frame_map[ref_index] = cm->next_ref_frame_map[ref_index];
    }
  }
}

unsigned int av1_get_next_used_ref_info(
    const AV1_COMMON *const cm, int i) {
  /*
  i = 0~1 orde_hint map
  i = 2~10 size map[i-2]
  */
  unsigned int info = 0;
  int j;
  if (i < 2) {
    /*next_used_ref_frame_map has 8 items*/
    for (j = 0; j < 4; j++) {
      RefCntBuffer *buf =
        cm->next_used_ref_frame_map[(i * 4) + j];
      if (buf)
        info |= ((buf->buf.order_hint & 0xff)
          << (j * 8));
    }
  } else if (i < 10) {
    RefCntBuffer *buf =
      cm->next_used_ref_frame_map[i-2];
    if (buf)
      info = (buf->buf.y_crop_width << 16) | (buf->buf.y_crop_height & 0xffff);
  } else {
    for (j = 0; j < 4; j++) {
      RefCntBuffer *buf =
        cm->next_used_ref_frame_map[((i - 10) * 4) + j];
      if (buf)
        info |= ((buf->buf.index & 0xff)
          << (j * 8));
    }
  }
  return info;
}

RefCntBuffer *av1_get_primary_ref_frame_buf(
  const AV1_COMMON *const cm)
{
  return get_primary_ref_frame_buf(cm);
}
