/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2002-2016, Audio Video coding Standard Workgroup of China
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Audio Video coding Standard Workgroup of China
 *    nor the names of its contributors maybe
 *    used to endorse or promote products
 *    derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */




/*
 * File name: global.h
 * Function:  global definitions for for AVS decoder.
 *
 */

#ifndef _GLOBAL_H_
#define _GLOBAL_H_

/* #include <stdio.h>                              //!< for FILE */
/* #include <stdlib.h> */

#define AML
#define SANITY_CHECK
#undef NO_DISPLAY

/* #include "define.h" */
#define RD      "19.2"
#define VERSION "19.2"

#define RESERVED_PROFILE_ID      0x24
#define BASELINE_PICTURE_PROFILE 18
#define BASELINE_PROFILE         32  /* 0x20 */
#define BASELINE10_PROFILE       34  /* 0x22 */


#define SCENE_PROFILE            48  /* 0x21 */
#define SCENE10_PROFILE          50  /* 0x23 */

#define TRACE                    0 /* !< 0:Trace off 1:Trace on */


/* Type definitions and file operation for Windows/Linux
 * All file operations for windows are replaced with native (FILE *) operations
 * Falei LUO (falei.luo@vipl.ict.ac.cn)
 * */

#define _FILE_OFFSET_BITS 64       /* for 64 bit fseeko */
#define fseek fseeko

#define int16 int16_t
#define int64 int64_t

/* ////////////////// bug fix ///////////////////////////// */
#define ALFSliceFix                        1
#define WRITENBIT_FIX                      1
#define FIX_PROFILE_LEVEL_DPB_RPS_1        1
#define FIX_PROFILE_LEVEL_DPB_RPS_2        1
#define FIX_RPS_PICTURE_REMOVE             1   /* flluo@pku.edu.cn */
#define Mv_Clip                            1   /* yuquanhe@hisilicon.com */
#define REMOVE_UNUSED                      1   /* yuquanhe@hisilicon.com */
#define SAO_Height_Fix                     1   /* yuquanhe@hisilicon.com */
#define B_BACKGROUND_Fix                   1   /* yuquanhe@hisilicon.com */
#define Check_Bitstream                    1   /* yuquanhe@hisilicon.com */
#define Wq_param_Clip                      1   /* yuquanhe@hisilicon.com */
    /* luofalei flluo@pku.edu.cn , wlq15@mails.tsinghua.edu.cn ,
    Longfei.Wang@mediatek.com */
#define RD1501_FIX_BG                      1
    /* yuquanhe@hisilicon.com ; he-yuan.lin@mstarsemi.com */
#define Mv_Rang                            1
     /* Longfei.Wang@mediatek.com ;fred.chiu@mediatek.com
     jie1222.chen@samsung.com */
#define RD160_FIX_BG                       1
     /* Y_K_Tu@novatek.com.tw, he-yuan.lin@mstarsemi.com,
     victor.huang@montage-tech.com M4041 */
#define RD1601_FIX_BG                      1
#define SEQ_CHANGE_CHECKER                 1    /* he-yuan.lin@mstarsemi.com */
#define M4140_END_OF_SLICE_CHECKER         1    /* he-yuan.lin@mstarsemi.com */
     /* wlq15@mails.tsinghua.edu.cn */
#define Mv_check_bug                       1
#define SAO_ASSERTION_FIX                  1    /* fred.chiu@mediatek.com */
#define FIELD_HORI_MV_NO_SCALE_FIX         1    /* fred.chiu@mediatek.com */
#define RD170_FIX_BG                       1
#define FIX_CHROMA_FIELD_MV_BK_DIST        1
#define FIX_LUMA_FIELD_MV_BK_DIST          1
#define FIX_CHROMA_FIELD_MV_CLIP           1
#if 1
#define FIX_FLUSH_DPB_BY_LF                1    /* fred.chiu@mediatek.com */
#define FIX_SEQ_END_FLUSH_DPB_BY_LF        1   /* fred.chiu@mediatek.com */
#else
#define FIX_FLUSH_DPB_BY_LF                0    /* fred.chiu@mediatek.com */
#define FIX_SEQ_END_FLUSH_DPB_BY_LF        0   /* fred.chiu@mediatek.com */
#endif
#define RD191_FIX_BUG                      1  /* yuquanhe@hsilicon.com */
#define SYM_MV_SCALE_FIX                   1/* peisong.chen@broadcom.com */
#define BUG_10BIT_REFINEQP                 0 /* wangzhenyu */



#if RD191_FIX_BUG
#endif

/************************
 * AVS2 macros start
 **************************/

#define INTERLACE_CODING                   1
#if INTERLACE_CODING  /* M3531: MV scaling compensation */
/* Luma component */
#define HALF_PIXEL_COMPENSATION            1 /* common functions definition */
#define HALF_PIXEL_COMPENSATION_PMV        1 /* spacial MV prediction */
#define HALF_PIXEL_COMPENSATION_DIRECT     1 /* B direct mode */
  /* MV derivation method 1, weighted P_skip mode */
#define HALF_PIXEL_COMPENSATION_M1         1
  /* M1 related with mv-scaling function */
#define HALF_PIXEL_COMPENSATION_M1_FUCTION 1
#define HALF_PIXEL_COMPENSATION_MVD        1 /* MV scaling from FW->BW */
/* Chroma components */
  /* chroma MV is scaled with luma MV for 4:2:0 format */
#define HALF_PIXEL_CHROMA                  1
  /* half pixel compensation for p skip/direct */
#define HALF_PIXEL_PSKIP                   1
#define INTERLACE_CODING_FIX               1 /* HLS fix */
#define OUTPUT_INTERLACE_MERGED_PIC        1

#endif
/*
 *******************************
AVS2 10bit/12bit profile
 ********************************
 */

#define DBFIX_10bit              1

#define BUG_10bit              1

/*
 ***************************************
AVS2 HIGH LEVEL SYNTAX
 ***************************************
 */
#define AVS2_HDR_HLS             1
 /* AVS2 HDR technology //yuquanhe@hisilicon.com */
#define AVS2_HDR_Tec                       1
#if AVS2_HDR_Tec
#define HDR_CHROMA_DELTA_QP                1 /* M3905 */
#define HDR_ADPTIVE_UV_DELTA                  1
#endif
/*
 *************************************
AVS2 S2
 *************************************
 */
#define AVS2_S2_FASTMODEDECISION 1
#define RD1510_FIX_BG            1      /* 20160714, flluo@pku.edu.cn */


/* ////////////////// prediction techniques ///////////////////////////// */
#define LAM_2Level_TU            0.8


#define DIRECTION                4
#define DS_FORWARD               4
#define DS_BACKWARD              2
#define DS_SYM                   3
#define DS_BID                   1

#define MH_PSKIP_NUM             4
#define NUM_OFFSET               0
#define BID_P_FST                1
#define BID_P_SND                2
#define FW_P_FST                 3
#define FW_P_SND                 4
#define WPM_NUM                  3
      /* M3330 changes it to 2, the original value is 3 */
#define MAX_MVP_CAND_NUM         2

#define DMH_MODE_NUM             5     /* Number of DMH mode */
#define TH_ME                    0     /* Threshold of ME */

#define MV_SCALE                 1

/* ///// reference picture management // */
#define FIX_MAX_REF              1     /* Falei LUO, flluo@pku.edu.cn */
#if FIX_MAX_REF
      /* maximum number of reference frame for each frame */
#define MAXREF                   7
#define MAXGOP                   32
#endif

/* #define REF_MAXBUFFER            7 */
/* more bufferes for displaying and background */
/* #define REF_MAXBUFFER            15 */
#if 1
#define REF_MAXBUFFER            23
#define REF_BUFFER  16
#else
#if RD170_FIX_BG
#define REF_MAXBUFFER            16
#else
#define REF_MAXBUFFER            7
#endif
#endif

#ifdef TO_PORTING
    /* block-composed background reference, fangdong@mail.ustc.edu.cn */
#define BCBR                     1
#else
#define BCBR    0
#endif
/* one more buffer for background when background_picture_output_flag is 0*/
#define AVS2_MAX_BUFFER_NUM               (REF_MAXBUFFER + 1)

/* /////////////////Adaptive Loop Filter////////////////////////// */
#define NUM_ALF_COEFF_CTX        1
#define NUM_ALF_LCU_CTX          4

#define LAMBDA_SCALE_LUMA       (1.0)
#define LAMBDA_SCALE_CHROMA     (1.0)



/* ////////////////// entropy coding ///////////////////////////// */
   /* M3090: Make sure rs1 will not overflow for 8-bit unsign char */
#define NUN_VALUE_BOUND          254
#define Encoder_BYPASS_Final     1    /* M3484 */
#define Decoder_Bypass_Annex     0    /* M3484 */
#define Decoder_Final_Annex      0    /* M3540 */


/* ////////////////// coefficient coding ///// */
     /* M3035 size of an coefficient group, 4x4 */
#define CG_SIZE                  16

#define SWAP(x, y) {\
	(y) = (y) ^ (x);\
	(x) = (y) ^ (x);\
	(y) = (x) ^ (y);\
}

/* ////////////////// encoder optimization /////// */
#define TH                                 2

#define M3624MDLOG                             /* reserved */

#define TDRDO                               1  /* M3528 */
/* #define FIX_TDRDO_BG  1  // flluo@pku.edu.cn, 20160318// */
#define RATECONTROL                         1  /* M3580 M3627 M3689 */
#define AQPO                                1  /* M3623 */
#define AQPOM3694                           0
#define AQPOM4063                           1
#define AQPOM3762                           1
#define BGQPO                               1  /* M4061 */
#if BGQPO
#define LONGREFERENCE                       32
#endif

/* #define REPORT */
/* ////////////////// Quantization   /////////////////////////////////////// */
 /* Adaptive frequency weighting quantization */
#define FREQUENCY_WEIGHTING_QUANTIZATION    1
#if FREQUENCY_WEIGHTING_QUANTIZATION
#define CHROMA_DELTA_QP                     1
#define AWQ_WEIGHTING                       1
#define AWQ_LARGE_BLOCK_ENABLE              1
#define COUNT_BIT_OVERHEAD                  0
#define AWQ_LARGE_BLOCK_EXT_MAPPING         1
#endif

#define QuantClip                           1
#define QuantMatrixClipFix                  1  /* 20160418, fllu@pku.edu.cn */

#define WQ_MATRIX_FCD                       1
#if !WQ_MATRIX_FCD
#define WQ_FLATBASE_INBIT  7
#else
#define WQ_FLATBASE_INBIT  6
#endif


#define REFINED_QP                          1


/* ////////////////// delta QP ///// */
      /* M3122: the minimum dQP unit is Macro block */
#define MB_DQP                    1
      /* M3122: 1 represents left prediction
      and 0 represents previous prediction */
#define LEFT_PREDICTION           1


/* //////////////////////SAO///////// */
#define NUM_BO_OFFSET             32
#define MAX_NUM_SAO_CLASSES       32
#define NUM_SAO_BO_CLASSES_LOG2   5
#define NUM_SAO_BO_CLASSES_IN_BIT 5
#define MAX_DOUBLE                (1.7e + 308)
#define NUM_SAO_EO_TYPES_LOG2     2
#define NUM_SAO_BO_CLASSES        (1<<NUM_SAO_BO_CLASSES_LOG2)
#define SAO_RATE_THR              0.75
#define SAO_RATE_CHROMA_THR       1
#define SAO_SHIFT_PIX_NUM         4

#define SAO_PARA_CROSS_SLICE      1
#define SAO_MULSLICE_FTR_FIX      1

/* /////////////////// Transform ///////////////////// */
#define SEC_TR_SIZE               4
   /* apply secT to greater than or equal to 8x8 block, */
#define SEC_TR_MIN_BITSIZE        3

#define BUGFIXED_COMBINED_ST_BD   1

/* /////////////////// Scalable ///////////////////// */
#define M3480_TEMPORAL_SCALABLE   1
#define TEMPORAL_MAXLEVEL         8
#define TEMPORAL_MAXLEVEL_BIT     3




/*
 *************************************
 * AVS2 macros end
 *
 *************************************
 */

#define CHROMA                    1
#define LUMA_8x8                  2
#define NUM_BLOCK_TYPES           8

#if (!defined clamp)
     /* !< clamp a to the range of [b;c] */
#define clamp(a, b, c) ((a) < (b) ? (b) : ((a) > (c) ? (c) : (a)))
#endif

	/* POC200301 moved from defines.h */
#define LOG2_MAX_FRAME_NUM_MINUS4    4
	/* !< bytes for one frame */
#define MAX_CODED_FRAME_SIZE         15000000

/* ----------------------- */
/* FLAGS and DEFINES for new chroma intra prediction, Dzung Hoang */
/* Threshold values to zero out quantized transform coefficients. */
/* Recommend that _CHROMA_COEFF_COST_ be low to improve chroma quality */
#define _LUMA_COEFF_COST_         4 /* !< threshold for luma coeffs */
  /* !< Number of pixels padded around the reference frame (>=4) */
#define IMG_PAD_SIZE              64

#define OUTSTRING_SIZE            255

	/* !< abs macro, faster than procedure */
#define absm(A) ((A) < (0) ? (-(A)) : (A))
    /* !< used for start value for some variables */
#define MAX_VALUE                999999

#define Clip1(a)     ((a) > 255 ? 255:((a) < 0 ? 0 : (a)))
#define Clip3(min, max, val) (((val) < (min)) ?\
		(min) : (((val) > (max)) ? (max) : (val)))

/* --------------------------------------------- */

/* block size of block transformed by AVS */
#define PSKIPDIRECT               0
#define P2NX2N                    1
#define P2NXN                     2
#define PNX2N                     3
#define PHOR_UP                   4
#define PHOR_DOWN                 5
#define PVER_LEFT                 6
#define PVER_RIGHT                7
#define PNXN                      8
#define I8MB                      9
#define I16MB                     10
#define IBLOCK                    11
#define InNxNMB                   12
#define INxnNMB                   13
#define MAXMODE                   14   /* add yuqh 20130824 */
#define  LAMBDA_ACCURACY_BITS     16
#define  LAMBDA_FACTOR(lambda) ((int)((double)(1 << LAMBDA_ACCURACY_BITS)\
		* lambda + 0.5))
#define  WEIGHTED_COST(factor, bits) (((factor) * (bits))\
		>> LAMBDA_ACCURACY_BITS)
#define  MV_COST(f, s, cx, cy, px, py) (WEIGHTED_COST(f, mvbits[((cx) << (s))\
		- px] +	mvbits[((cy) << (s)) - py]))
#define  REF_COST(f, ref)              (WEIGHTED_COST(f, refbits[(ref)]))

#define  BWD_IDX(ref)                 (((ref) < 2) ? 1 - (ref) : (ref))
#define  REF_COST_FWD(f, ref) (WEIGHTED_COST(f,\
		((img->num_ref_pic_active_fwd_minus1 == 0) ?\
			0 : refbits[(ref)])))
#define  REF_COST_BWD(f, ef) (WEIGHTED_COST(f,\
		((img->num_ref_pic_active_bwd_minus1 == 0) ?\
			0 : BWD_IDX(refbits[ref]))))

#define IS_INTRA(MB) ((MB)->cuType == I8MB ||\
	(MB)->cuType == I16MB ||\
	(MB)->cuType == InNxNMB || (MB)->cuType == INxnNMB)
#define IS_INTER(MB) ((MB)->cuType != I8MB &&\
	(MB)->cuType != I16MB && (MB)->cuType != InNxNMB\
	&& (MB)->cuType != INxnNMB)
#define IS_INTERMV(MB) ((MB)->cuType != I8MB &&\
	(MB)->cuType != I16MB && (MB)->cuType != InNxNMB &&\
	(MB)->cuType != INxnNMB && (MB)->cuType != 0)


#define IS_DIRECT(MB) ((MB)->cuType == PSKIPDIRECT && (img->type == B_IMG))
#define IS_P_SKIP(MB) ((MB)->cuType == PSKIPDIRECT &&\
	(((img->type == F_IMG)) || ((img->type == P_IMG))))
#define IS_P8x8(MB)  ((MB)->cuType == PNXN)

/* Quantization parameter range */
#define MIN_QP                       0
#define MAX_QP                       63
#define SHIFT_QP                     11

/* Picture types */
#define INTRA_IMG                    0   /* !< I frame */
#define INTER_IMG                    1   /* !< P frame */
#define B_IMG                        2   /* !< B frame */
#define I_IMG                        0   /* !< I frame */
#define P_IMG                        1   /* !< P frame */
#define F_IMG                        4  /* !< F frame */

#define BACKGROUND_IMG               3

#define BP_IMG                       5


/* Direct Mode types */
#define MIN_CU_SIZE                  8
#define MIN_BLOCK_SIZE               4
#define MIN_CU_SIZE_IN_BIT           3
#define MIN_BLOCK_SIZE_IN_BIT        2
#define BLOCK_MULTIPLE              (MIN_CU_SIZE/(MIN_BLOCK_SIZE))
#define MAX_CU_SIZE                  64
#define MAX_CU_SIZE_IN_BIT           6
#define B4X4_IN_BIT                  2
#define B8X8_IN_BIT                  3
#define B16X16_IN_BIT                4
#define B32X32_IN_BIT                5
#define B64X64_IN_BIT                6
    /* !< # luma intra prediction modes */
#define NUM_INTRA_PMODE              33
    /* number of luma modes for full RD search */
#define NUM_MODE_FULL_RD             9
    /* !< #chroma intra prediction modes */
#define NUM_INTRA_PMODE_CHROMA       5

/* luma intra prediction modes */

#define DC_PRED                      0
#define PLANE_PRED                   1
#define BI_PRED                      2
#define VERT_PRED                    12
#define HOR_PRED                     24


/* chroma intra prediction modes */
#define DM_PRED_C                    0
#define DC_PRED_C                    1
#define HOR_PRED_C                   2
#define VERT_PRED_C                  3
#define BI_PRED_C                    4

#define EOS                          1         /* !< End Of Sequence */
	/* !< Start Of Picture */
#define SOP                          2

#define DECODING_OK                  0
#define SEARCH_SYNC                  1
#define DECODE_MB                    1

#ifndef max
  /* !< Macro returning max value */
#define max(a, b)                   ((a) > (b) ? (a) : (b))
  /* !< Macro returning min value */
#define min(a, b)                   ((a) < (b) ? (a) : (b))
#endif


#define XY_MIN_PMV                   1
#if XY_MIN_PMV
#define MVPRED_xy_MIN                0
#else
#define MVPRED_MEDIAN                0
#endif
#define MVPRED_L                     1
#define MVPRED_U                     2
#define MVPRED_UR                    3

#define DUAL                         4
#define FORWARD                      0
#define BACKWARD                     1
#define SYM                          2
#define BID                          3
#define INTRA                        -1

#define BUF_CYCLE                    5

#define ROI_M3264                    1      /* ROI Information Encoding */

#define PicExtensionData             1


#define REF_OUTPUT                   1  /* M3337 */


/* MV scaling 14 bit */
#define MULTI                        16384
#define HALF_MULTI                   8192
#define OFFSET                       14
/* end of MV scaling */
 /* store the middle pixel's mv in a motion information unit */
#define MV_DECIMATION_FACTOR         4

/* BUGFIX_AVAILABILITY_INTRA */
#define NEIGHBOR_INTRA_LEFT                 0
#define NEIGHBOR_INTRA_UP                   1
#define NEIGHBOR_INTRA_UP_RIGHT             2
#define NEIGHBOR_INTRA_UP_LEFT              3
#define NEIGHBOR_INTRA_LEFT_DOWN            4
/* end of BUGFIX_AVAILABILITY_INTRA */

/* end #include "define.h" */

/*#include "commonStructures.h"*/

/*typedef uint16_t byte;*/    /* !< byte type definition */
#define byte uint16_t
#define pel_t byte

enum BitCountType_e {
	BITS_HEADER,
	BITS_TOTAL_MB,
	BITS_MB_MODE,
	BITS_INTER_MB,
	BITS_CBP_MB,
	BITS_CBP01_MB,
	BITS_COEFF_Y_MB,
	BITS_COEFF_UV_MB,
	BITS_DELTA_QUANT_MB,
	BITS_SAO_MB,
	MAX_BITCOUNTER_MB
};


enum SAOEOClasses {
/* EO Groups, the assignments depended on
how you implement the edgeType calculation */
	SAO_CLASS_EO_FULL_VALLEY = 0,
	SAO_CLASS_EO_HALF_VALLEY = 1,
	SAO_CLASS_EO_PLAIN       = 2,
	SAO_CLASS_EO_HALF_PEAK   = 3,
	SAO_CLASS_EO_FULL_PEAK   = 4,
	SAO_CLASS_BO             = 5,
	NUM_SAO_EO_CLASSES = SAO_CLASS_BO,
	NUM_SAO_OFFSET
};

struct SAOstatdata {
	int32_t diff[MAX_NUM_SAO_CLASSES];
	int32_t  count[MAX_NUM_SAO_CLASSES];
};

struct CopyRight_s {
	int32_t extension_id;
	int32_t copyright_flag;
	int32_t copyright_id;
	int32_t original_or_copy;
	int32_t reserved;
	int32_t copyright_number;
};

struct CameraParamters_s {
	int32_t reserved;
	int32_t camera_id;
	int32_t height_of_image_device;
	int32_t focal_length;
	int32_t f_number;
	int32_t vertical_angle_of_view;
	int32_t camera_position_x;
	int32_t camera_position_y;
	int32_t camera_position_z;
	int32_t camera_direction_x;
	int32_t camera_direction_y;
	int32_t camera_direction_z;
	int32_t image_plane_vertical_x;
	int32_t image_plane_vertical_y;
	int32_t image_plane_vertical_z;
};

/* ! SNRParameters */
struct SNRParameters_s {
	double snr_y;               /* !< current Y SNR */
	double snr_u;               /* !< current U SNR */
	double snr_v;               /* !< current V SNR */
	double snr_y1;              /* !< SNR Y(dB) first frame */
	double snr_u1;              /* !< SNR U(dB) first frame */
	double snr_v1;              /* !< SNR V(dB) first frame */
	double snr_ya;              /* !< Average SNR Y(dB) remaining frames */
	double snr_ua;              /* !< Average SNR U(dB) remaining frames */
	double snr_va;              /* !< Average SNR V(dB) remaining frames */
#if INTERLACE_CODING
	double i_snr_ya;               /* !< current Y SNR */
	double i_snr_ua;               /* !< current U SNR */
	double i_snr_va;               /* !< current V SNR */
#endif
};

/* signal to noise ratio parameters */

/* ! codingUnit */
struct codingUnit {
	uint32_t        ui_MbBitSize;
	int32_t                 uiBitSize;            /* size of MB */
	/* !< number of current syntax element */
	int32_t                 currSEnr;
	int32_t                 slice_nr;
	int32_t                 delta_quant;          /* !< for rate control */
	int32_t                 delta_qp;
	int32_t                 qp;
	int32_t                 bitcounter[MAX_BITCOUNTER_MB];
	struct codingUnit
	*mb_available[3][3]; /*!< pointer to neighboring MBs
		in a 3x3 window of current MB, which is located at [1][1] \n
		NULL pointer identifies neighboring MBs which are unavailable */
	/* some storage of codingUnit syntax elements for global access */
	int32_t                 cuType;
	int32_t                 weighted_skipmode;

	int32_t                 md_directskip_mode;

	int32_t                 trans_size;
	int
	/* !< indices correspond to [forw,backw][block_y][block_x][x,y, dmh] */
	mvd[2][BLOCK_MULTIPLE][BLOCK_MULTIPLE][3];

	int32_t  intra_pred_modes[BLOCK_MULTIPLE * BLOCK_MULTIPLE];
	int32_t  real_intra_pred_modes[BLOCK_MULTIPLE * BLOCK_MULTIPLE];
	int32_t  l_ipred_mode;
	int32_t  cbp, cbp_blk;
	uint32_t cbp_bits;

	int32_t                 b8mode[4];
	int32_t                 b8pdir[4];
      /* !< chroma intra prediction mode */
	int32_t                 c_ipred_mode;

   /* !< pointer to neighboring MB (AEC) */
	struct codingUnit   *mb_available_up;
	 /* !< pointer to neighboring MB (AEC) */
	struct codingUnit   *mb_available_left;
	int32_t                 mbAddrA, mbAddrB, mbAddrC, mbAddrD;
       /* !<added by mz, 2008.04 */
	int32_t                 slice_set_index;
     /* added by mz, 2008.04 */
	int32_t                 slice_header_flag;
	int32_t                 sliceqp;         /* added by mz, 2008.04 */
#if MB_DQP
	int32_t                 previouse_qp;
	int32_t                 left_cu_qp;
#endif
	int32_t                 block_available_up;
	int32_t                 block_available_left;

};


/* image parameters */
struct syntaxelement;
struct slice;
struct alfdatapart;
struct SAOBlkParam_s {
	int32_t modeIdc; /* NEW, MERGE, OFF */
	/* NEW: EO_0, EO_90, EO_135, EO_45, BO. MERGE: left, above */
	int32_t typeIdc;
	int32_t startBand; /* BO: starting band index */
	int32_t startBand2;
	int32_t deltaband;
	int32_t offset[MAX_NUM_SAO_CLASSES];
};
struct ALFParam_s {
	int32_t alf_flag;
	int32_t num_coeff;
	int32_t filters_per_group;
	int32_t componentID;
	int32_t filterPattern[16]; /* *filterPattern; */
	int32_t coeffmulti[16][9]; /* **coeffmulti; */
};

enum ALFComponentID {
	ALF_Y = 0,
	ALF_Cb,
	ALF_Cr,
	NUM_ALF_COMPONENT
};
struct ALF_APS_s {
	int32_t usedflag;
	int32_t cur_number;
	int32_t max_number;
	struct ALFParam_s alf_par[NUM_ALF_COMPONENT];
};


/* ------------------------------------------------------
 * frame data
 */
struct avs2_frame_s {
	int32_t imgcoi_ref;
	byte * *referenceFrame[3];
	int32_t **refbuf;
	int32_t ***mvbuf;
#if 0
	double saorate[NUM_SAO_COMPONENTS];
#endif
	byte ***ref;

	int32_t imgtr_fwRefDistance;
	int32_t refered_by_others;
	int32_t is_output;
	int32_t to_prepare_disp;
#if M3480_TEMPORAL_SCALABLE
	/* temporal level setted in configure file */
	int32_t temporal_id;
#endif
	byte **oneForthRefY;
#if FIX_MAX_REF
	int32_t ref_poc[MAXREF];
#else
	int32_t ref_poc[4];
#endif
#ifdef AML
	int32_t index;
	int32_t mmu_alloc_flag;
	int32_t lcu_size_log2;
	/*uint32_t header_adr;*/
	uint32_t mc_y_adr;
	uint32_t mc_u_v_adr;
	uint32_t mc_canvas_y;
	uint32_t mc_canvas_u_v;
	uint32_t mpred_mv_wr_start_addr;
	uint8_t bg_flag;
	/**/
	unsigned long header_adr;
	int buf_size;
	int lcu_total;
	int comp_body_size;
	uint32_t dw_y_adr;
	uint32_t dw_u_v_adr;
	int y_canvas_index;
	int uv_canvas_index;
	struct canvas_config_s canvas_config[2];
	int double_write_mode;
	int bit_depth;
	unsigned long cma_alloc_addr;
	int BUF_index;
	int pic_w;
	int pic_h;
	int stream_offset;
	u32 pts;
	u64 pts64;
	/**/
	int vf_ref;
	int decode_idx;
	int slice_type;
	int32_t imgtr_fwRefDistance_bak;
	int32_t error_mark;
	int32_t decoded_lcu;
#endif
#ifndef MV_USE_FIXED_BUF
	int mv_buf_index;
#endif
};


struct ImageParameters_s {
	struct codingUnit    *mb_data;
	int32_t number;                                 /* <! frame number */
	int32_t numIPFrames;

	int32_t type;
	int32_t typeb;
	int32_t typeb_before;

	int32_t qp; /* <! quant for the current frame */
	int32_t current_mb_nr; /* bitstream order */
	int32_t current_slice_nr;
	int32_t tr;   /* <! temporal reference, 8 bit, */

	int32_t width;                   /* !< Number of pels */
	int32_t width_cr;                /* !< Number of pels chroma */
	int32_t height;                  /* !< Number of lines */
	int32_t height_cr;               /* !< Number of lines  chroma */
	int32_t PicWidthInMbs;
	int32_t PicSizeInMbs;
	int32_t block8_x, block8_y;
	int32_t   subblock_x;
	int32_t   subblock_y;

	int32_t num_of_references;
    /* <! Bug Fix: correct picture size for outputted reconstructed pictures */
	int32_t auto_crop_right;
	int32_t auto_crop_bottom;
	int32_t buf_cycle;
	int32_t picture_structure;
	 /* <! pointer to current Slice data struct */
	struct slice       *currentSlice;

	int32_t **predBlock;             /* !< current best prediction mode */
	int32_t **predBlockTmp;
	/* !< the diff pixel values between orginal image and prediction */
	int32_t **resiY;
	/* !< Array containing square values,used for snr computation */
	int32_t *quad;

	/* //location of current MB////// */
	int32_t mb_y;                    /* !< current MB vertical */
	int32_t mb_x;                    /* !< current MB horizontal */
	int32_t pix_y;                   /* !< current pixel vertical */
	int32_t pix_x;                   /* !< current pixel horizontal */
	int32_t pix_c_y;                 /* !< current pixel chroma vertical */
	int32_t pix_c_x; /* !< current pixel chroma horizontal */

	int32_t imgtr_next_P;

	int32_t imgcoi_next_ref;

    /* !< GH ipredmode[90][74];prediction mode for inter frames */
    /* fix from ver 4.1 */
	int32_t **ipredmode;
	int32_t **rec_ipredmode;


	/* //////////////decoder////////////////////////// */
	int32_t max_mb_nr;
	int32_t **intra_block;

	int32_t block_y;
	int32_t block_x;
    /* <! final 4x4 block. Extended to 16x16 for AVS */
	int32_t resiUV[2][MAX_CU_SIZE][MAX_CU_SIZE];

	int32_t **fw_refFrArr;                          /* <! [72][88]; */
	int32_t **bw_refFrArr;                          /* <! [72][88]; */

	int32_t random_access_decodable_flag;

	int32_t seq_header_indicate;
	int32_t B_discard_flag;

	/* B pictures */
	uint32_t pic_distance;

	uint32_t coding_order;

	uint32_t PrevPicDistanceLsb;
	int32_t CurrPicDistanceMsb;

	int32_t PicHeightInMbs;

	int32_t types;

	int32_t new_sequence_flag;
	int32_t sequence_end_flag;            /* <! rm52k_r2 */

	int32_t current_slice_set_index;          /* <! added by mz, 2008.04 */
	int32_t current_slice_header_flag;        /* <! added by mz, 2008.04 */
	int32_t slice_set_qp[64];             /* <! added by mz, 2008.04 */


	int32_t inter_amp_enable;

	/* ////////////////////////encoder////////////////////////// */

	/* int32_t nb_references;     //!< replaced by "num_of_references" */

	int32_t framerate;

	int32_t ***predBlockY;        /* !< all 9 prediction modes */
     /* !< new chroma 8x8 intra prediction modes */
	int32_t ****predBlockUV;

	int32_t **Coeff_all;/* qyu 0821 */

	struct syntaxelement   *MB_SyntaxElements; /* !< by oliver 0612 */

	/* B pictures */

	int32_t b_frame_to_code;
	int32_t num_ref_pic_active_fwd_minus1;
	int32_t num_ref_pic_active_bwd_minus1;
	int32_t mv_range_flag;

	uint32_t frame_num;   /* frame_num for this frame */
	int32_t slice_offset;
	/* the following are sent in the slice header */
	int32_t NoResidueDirect;
	int32_t coded_mb_nr;
	int32_t progressive_frame;
	int32_t tc_reserve_bit;
	 /* the last MB no in current slice.      Yulj 2004.07.15 */
	int32_t mb_no_currSliceLastMB;
	int32_t Seqheader_flag;     /* Added by cjw, 20070327 */
	int32_t EncodeEnd_flag;         /* Carmen, 2007/12/19 */

	uint16_t bbv_delay;

	int32_t tmp_fwBSkipMv[DIRECTION + 1][2];
	int32_t tmp_bwBSkipMv[DIRECTION + 1][2];

	int32_t tmp_pref_fst[MH_PSKIP_NUM + NUM_OFFSET + 1];
	int32_t tmp_pref_snd[MH_PSKIP_NUM + NUM_OFFSET + 1];
	int32_t tmp_fstPSkipMv[MH_PSKIP_NUM + NUM_OFFSET + 1][3];
	int32_t tmp_sndPSkipMv[MH_PSKIP_NUM + NUM_OFFSET + 1][3];
#if BCBR
byte *org_ref_y;
byte *org_ref_u;
byte *org_ref_v;
int32_t  *BLCUidx;
int32_t  *DQPList;
int32_t  iNumCUsInFrame;

byte *org_ref2_y;
byte *org_ref2_u;
byte *org_ref2_v;
int32_t  ref2Num;
#endif
/* //////////////SAO parameter////////////////// */
double        *cur_saorate;
#if 0
int32_t            slice_sao_on[NUM_SAO_COMPONENTS];
#endif
int32_t            pic_alf_on[NUM_ALF_COMPONENT];
struct alfdatapart   *dp_ALF;

#if INTERLACE_CODING
int32_t is_field_sequence;
int32_t is_top_field;
#endif


};



/* ! struct for context management */
struct BiContextType_s {
	uint8_t MPS;   /* 1 bit */
	uint32_t  LG_PMPS; /* 10 bits */
	uint8_t cycno;  /* 2 bits */
};

/***********************************************************************
 * D a t a    t y p e s   f o r  A E C
 ************************************************************************/



struct pix_pos {
	int32_t available;   /* ABCD */
	int32_t mb_addr;    /* MB position */
	int32_t x;
	int32_t y;
	int32_t pos_x;     /* 4x4 x-pos */
	int32_t pos_y;
};



struct STDOUT_DATA_s {
	int32_t type;
	int32_t typeb;

	int32_t   framenum;
	int32_t   tr;
	int32_t   qp;
	double snr_y;
	double snr_u;
	double snr_v;
	int32_t   tmp_time;
	int32_t   picture_structure;
	int32_t   curr_frame_bits;
	int32_t   emulate_bits;

	uint32_t DecMD5Value[4];
#if RD1501_FIX_BG
int32_t background_picture_output_flag;/* Longfei.Wang@mediatek.com */
#endif
#if RD160_FIX_BG
int32_t picture_reorder_delay;
#endif
int8_t str_reference_list[128];  /* reference list information */
};

/**********************************************************************
 * C O N T E X T S   F O R   T M L   S Y N T A X   E L E M E N T S
 **********************************************************************
 */
#define NUM_CuType_CTX              (11 + 10)
#define NUM_B8_TYPE_CTX              9
#define NUM_MVD_CTX                 15
#define NUM_PMV_IDX_CTX             10
#define NUM_REF_NO_CTX               6
#define NUM_DELTA_QP_CTX             4
#define NUM_INTER_DIR_CTX           18
#define NUM_INTER_DIR_DHP_CTX           3
#define NUM_B8_TYPE_DHP_CTX             1
#define NUM_AMP_CTX                  2
#define NUM_C_INTRA_MODE_CTX         4
#define NUM_CBP_CTX                  4
#define NUM_BCBP_CTX                 4
#define NUM_MAP_CTX                 17
#define NUM_LAST_CTX                17

#define NUM_INTRA_MODE_CTX           7

#define NUM_ABS_CTX                  5
#define NUM_TU_CTX                   3
#define NUM_SPLIT_CTX                8  /* CU depth */
#if BCBR
#define NUM_BGBLCOK_CTX              1
#endif

#define NUM_BRP_CTX                  8


#define NUM_LAST_CG_CTX_LUMA        12
#define NUM_LAST_CG_CTX_CHROMA       6
#define NUM_SIGCG_CTX_LUMA           2
#define NUM_SIGCG_CTX_CHROMA         1
#define NUM_LAST_POS_CTX_LUMA   56
#define NUM_LAST_POS_CTX_CHROMA 16
#define NUM_LAST_CG_CTX (NUM_LAST_CG_CTX_LUMA + NUM_LAST_CG_CTX_CHROMA)
#define NUM_SIGCG_CTX (NUM_SIGCG_CTX_LUMA + NUM_SIGCG_CTX_CHROMA)
#define NUM_LAST_POS_CTX (NUM_LAST_POS_CTX_LUMA + NUM_LAST_POS_CTX_CHROMA)
#define NUM_SAO_MERGE_FLAG_CTX                   3
#define NUM_SAO_MODE_CTX                         1
#define NUM_SAO_OFFSET_CTX                       2
#define NUM_INTER_DIR_MIN_CTX         2

/*end #include "commonStructures.h"*/

/*#include "commonVariables.h"*/

/*
extern struct CameraParamters_s *camera;
extern struct SNRParameters_s *snr;
extern struct ImageParameters_s *img;
 */

/* avs2_frame_t *fref[REF_MAXBUFFER]; */


#define ET_SIZE 300      /* !< size of error text buffer */


/* ------------------------------------------------------
 * common data
 */
struct Video_Com_data_s {
	int32_t   Bframe_ctr;

	/* FILE *p_log;                     //!< SNR file */
	/* FILE *p_trace;                   //!< Trace file */

	int32_t   tot_time;

	/* Tsinghua for picture_distance  200701 */
	int32_t   picture_distance;

	/* M3178 PKU Reference Manage */
	int32_t   coding_order;
	/* !< current encoding/decoding frame pointer */
	struct avs2_frame_s *f_rec;
	int32_t   seq_header;
    /* !< Array for reference frames of each block */
	int32_t    **refFrArr;
	int32_t    **p_snd_refFrArr;

	byte  ***currentFrame; /* [yuv][height][width] */
#ifdef AML
	struct avs2_frame_s *cur_pic; /*either f_rec or m_bg*/
#endif
	byte   **backgroundReferenceFrame[3];
	byte  ***background_ref;


	int32_t  total_frames;

	/* mv_range, 20071009 */
	int32_t  Min_V_MV;
	int32_t  Max_V_MV;
	int32_t  Min_H_MV;
	int32_t  Max_H_MV;
	/* !< buffer for error message for exit with error(void) */
	int8_t errortext[ET_SIZE];
	int8_t str_list_reference[128];


};
/* extern Video_Com_data *hc; */


/*end #include "commonVariables.h"*/
/* #define USE_PARAM_TXT */
/*
#if FIX_CHROMA_FIELD_MV_BK_DIST
int8_t bk_img_is_top_field;
#endif
*/
/* void write_GB_frame(FILE *p_dec); */

#if !FIX_MAX_REF
#define MAXREF    4
#define MAXGOP    32
#endif

struct StatBits {
	int32_t   curr_frame_bits;
	int32_t   prev_frame_bits;
	int32_t   emulate_bits;
	int32_t   prev_emulate_bits;
	int32_t   last_unit_bits;
	int32_t   bitrate;
	int32_t   total_bitrate[1000];
	int32_t   coded_pic_num;
	int32_t   time_s;
};

struct reference_management {
	int32_t poc;
	int32_t qp_offset;
	int32_t num_of_ref;
	int32_t referd_by_others;
	int32_t ref_pic[MAXREF];
	int32_t predict;
	int32_t deltaRPS;
	int32_t num_to_remove;
	int32_t remove_pic[MAXREF];
};


/* ------------------------------------------------------
 * dec data
 */
struct Video_Dec_data_s {
	byte **background_frame[3];
	int32_t background_reference_enable;

	int32_t background_picture_flag;
	int32_t background_picture_output_flag;
	int32_t background_picture_enable;

	int32_t background_number;

#if BCBR
	int32_t bcbr_enable;
#endif

	int32_t demulate_enable;
	int32_t currentbitoffset;

	int32_t aspect_ratio_information;
	int32_t frame_rate_code;
	int32_t bit_rate_lower;
	int32_t bit_rate_upper;
	int32_t  marker_bit;

	int32_t video_format;
	int32_t color_description;
	int32_t color_primaries;
	int32_t transfer_characteristics;
	int32_t matrix_coefficients;

	int32_t progressive_sequence;
#if INTERLACE_CODING
int32_t is_field_sequence;
#endif
int32_t low_delay;
int32_t horizontal_size;
int32_t vertical_size;
int32_t sample_precision;
int32_t video_range;

int32_t display_horizontal_size;
int32_t display_vertical_size;
int32_t TD_mode;
int32_t view_packing_mode;
int32_t view_reverse;

int32_t b_pmvr_enabled;
int32_t dhp_enabled;
int32_t b_dmh_enabled;
int32_t b_mhpskip_enabled;
int32_t wsm_enabled;
int32_t b_secT_enabled;

int32_t tmp_time;
int32_t FrameNum;
int32_t eos;
int32_t pre_img_type;
int32_t pre_img_types;
/* int32_t pre_str_vec; */
int32_t pre_img_tr;
int32_t pre_img_qp;
int32_t pre_tmp_time;
int32_t RefPicExist;   /* 20071224 */
int32_t BgRefPicExist;
int32_t dec_ref_num;                /* ref order */

/* video edit code */ /* M1956 by Grandview 2006.12.12 */
int32_t vec_flag;

/* Copyright_extension(void) header */
int32_t copyright_flag;
int32_t copyright_identifier;
int32_t original_or_copy;
int64_t copyright_number_1;
int64_t copyright_number_2;
int64_t copyright_number_3;
/* Camera_parameters_extension */
int32_t camera_id;
int32_t height_of_image_device;
int32_t focal_length;
int32_t f_number;
int32_t vertical_angle_of_view;
int32_t camera_position_x_upper;
int32_t camera_position_x_lower;
int32_t camera_position_y_upper;
int32_t camera_position_y_lower;
int32_t camera_position_z_upper;
int32_t camera_position_z_lower;
int32_t camera_direction_x;
int32_t camera_direction_y;
int32_t camera_direction_z;
int32_t image_plane_vertical_x;
int32_t image_plane_vertical_y;
int32_t image_plane_vertical_z;

#if AVS2_HDR_HLS
/* mastering_display_and_content_metadata_extension(void) header */
int32_t display_primaries_x0;
int32_t display_primaries_y0;
int32_t display_primaries_x1;
int32_t display_primaries_y1;
int32_t display_primaries_x2;
int32_t display_primaries_y2;
int32_t white_point_x;
int32_t white_point_y;
int32_t max_display_mastering_luminance;
int32_t min_display_mastering_luminance;
int32_t maximum_content_light_level;
int32_t maximum_frame_average_light_level;
#endif

/* I_pictures_header(void) */
int32_t top_field_first;
int32_t repeat_first_field;
int32_t progressive_frame;
#if INTERLACE_CODING
int32_t is_top_field;
#endif
/* int32_t fixed_picture_qp;   //qyu 0927 */
int32_t picture_qp;
int32_t fixed_picture_qp;
int32_t time_code_flag;
int32_t time_code;
int32_t loop_filter_disable;
int32_t loop_filter_parameter_flag;
/* int32_t alpha_offset; */
/* int32_t beta_offset; */

/* Pb_picture_header(void) */
int32_t picture_coding_type;

/*picture_display_extension(void)*/
int32_t frame_centre_horizontal_offset[4];
int32_t frame_centre_vertical_offset[4];

/* slice_header(void) */
int32_t img_width;
int32_t slice_vertical_position;
int32_t slice_vertical_position_extension;
int32_t fixed_slice_qp;
int32_t slice_qp;
int32_t slice_horizontal_positon;       /* added by mz, 2008.04 */
int32_t slice_horizontal_positon_extension;

int32_t StartCodePosition;
int32_t background_pred_flag;


/* Reference Manage */
int32_t displaydelay;
int32_t picture_reorder_delay;
#if M3480_TEMPORAL_SCALABLE
int32_t temporal_id_exist_flag;
#endif

int32_t gop_size;
struct reference_management decod_RPS[MAXGOP];
struct reference_management curr_RPS;
int32_t last_output;
int32_t trtmp;
#if M3480_TEMPORAL_SCALABLE
int32_t cur_layer;
#endif

/* Adaptive frequency weighting quantization */
#if FREQUENCY_WEIGHTING_QUANTIZATION
int32_t weight_quant_enable_flag;
int32_t load_seq_weight_quant_data_flag;

int32_t pic_weight_quant_enable_flag;
int32_t pic_weight_quant_data_index;
int32_t weighting_quant_param;
int32_t weighting_quant_model;
int16_t quant_param_undetail[6];      /* M2148 2007-09 */
int16_t quant_param_detail[6];        /* M2148 2007-09 */
int32_t WeightQuantEnable;              /* M2148 2007-09 */
int32_t mb_adapt_wq_disable;            /* M2331 2008-04 */
int32_t mb_wq_mode;                     /* M2331 2008-04 */
#if CHROMA_DELTA_QP
int32_t chroma_quant_param_disable;
int32_t chroma_quant_param_delta_u;
int32_t chroma_quant_param_delta_v;
#endif

int32_t b_pre_dec_intra_img;
int32_t pre_dec_img_type;
int32_t CurrentSceneModel;
#endif

int32_t curr_IDRcoi;
int32_t curr_IDRtr;
int32_t next_IDRtr;
int32_t next_IDRcoi;
int32_t end_SeqTr;

#if MB_DQP
int32_t lastQP;
/* FILE * testQP; */
#endif

};
/* extern Video_Dec_data *hd; */

struct DecodingEnvironment_s {
	uint32_t    Dbuffer;
	int32_t             Dbits_to_go;
	uint8_t            *Dcodestrm;
	int32_t             *Dcodestrm_len;
};

/* added at rm52k version */

struct inp_par;



/* ! Slice */
struct slice {
	int32_t                 picture_id;
	int32_t                 qp;
	int32_t                 picture_type; /* !< picture type */
	int32_t                 start_mb_nr;
	 /* !< number of different partitions */
	int32_t                 max_part_nr;

	/* added by lzhang */
	/* !< pointer to struct of context models for use in AEC */
	struct SyntaxInfoContexts_s  *syn_ctx;
};

struct alfdatapart {
	struct Bitstream_s           *bitstream;
	struct DecodingEnvironment_s de_AEC;
	struct SyntaxInfoContexts_s  *syn_ctx;
};
/* static int32_t alfParAllcoated = 0; */

/* input parameters from configuration file */
struct inp_par {
	int32_t   buf_cycle;                 /* <! Frame buffer size */
	int32_t   ref_pic_order;             /* <! ref order */
	int32_t   output_dec_pic;            /* <! output_dec_pic */
	int32_t   profile_id;
	int32_t   level_id;
	int32_t   chroma_format;
	int32_t   g_uiMaxSizeInBit;
	int32_t   alpha_c_offset;
	int32_t   beta_offset;
	int32_t   useNSQT;
#if MB_DQP
	int32_t   useDQP;
#endif
	int32_t   useSDIP;
	int32_t sao_enable;
#if M3480_TEMPORAL_SCALABLE
	int32_t temporal_id_exist_flag;
#endif
	int32_t alf_enable;

	int32_t crossSliceLoopFilter;

	int32_t   sample_bit_depth;  /* sample bit depth */
  /* decoded file bit depth (assuming output_bit_depth is
  less or equal to sample_bit_depth) */
	int32_t   output_bit_depth;


	int32_t MD5Enable;

#if OUTPUT_INTERLACE_MERGED_PIC
	int32_t output_interlace_merged_picture;
#endif

};

/* extern struct inp_par *input; */

struct outdata_s {
#if RD170_FIX_BG
	struct STDOUT_DATA_s stdoutdata[REF_MAXBUFFER];
#else
	struct STDOUT_DATA_s stdoutdata[8];
#endif
	int32_t         buffer_num;
};
/* outdata outprint; */

#define PAYLOAD_TYPE_IDERP 8

struct Bitstream_s *AllocBitstream(void);
void FreeBitstream(void);
#if TRACE
void tracebits2(const int8_t *trace_str, int32_t len, int32_t info);
#endif

/* int32_t   direct_mv[45][80][4][4][3]; // only to verify result */

#define I_PICTURE_START_CODE    0xB3
#define PB_PICTURE_START_CODE   0xB6
#define SLICE_START_CODE_MIN    0x00
#define SLICE_START_CODE_MAX    0x8F
#define USER_DATA_START_CODE    0xB2
#define SEQUENCE_HEADER_CODE    0xB0
#define EXTENSION_START_CODE    0xB5
#define SEQUENCE_END_CODE       0xB1
#define VIDEO_EDIT_CODE         0xB7


#define SEQUENCE_DISPLAY_EXTENSION_ID            2
#define COPYRIGHT_EXTENSION_ID                   4
#define CAMERAPARAMETERS_EXTENSION_ID            11
#define PICTURE_DISPLAY_EXTENSION_ID             7
#if M3480_TEMPORAL_SCALABLE
#define TEMPORAL_SCALABLE_EXTENSION_ID           3
#endif

#if ROI_M3264
#if RD1501_FIX_BG
#define LOCATION_DATA_EXTENSION_ID               12
#else
#define LOCATION_DATA_EXTENSION_ID               15
#endif
#endif

#if AVS2_HDR_HLS
#define MASTERING_DISPLAY_AND_CONTENT_METADATA_EXTENSION     10
#endif

void malloc_slice(void);
void free_slice(void);


void read_ipred_modes(void);

int32_t  AEC_startcode_follows(int32_t eos_bit);

/* extern uint32_t max_value_s; */

/*ComAdaptiveLoopFilter.h*/
#define ALF_MAX_NUM_COEF       9
#define NO_VAR_BINS            16


#define RPM_BEGIN                                              0x100
#define ALF_BEGIN                                              0x180
#define RPM_END                                                0x280

union param_u {
	struct {
		uint16_t data[RPM_END - RPM_BEGIN];
	} l;
	struct {
		/*sequence*/
		uint16_t profile_id;
		uint16_t level_id;
		uint16_t progressive_sequence;
		uint16_t is_field_sequence;
		uint16_t horizontal_size;
		uint16_t vertical_size;
		uint16_t chroma_format;
		uint16_t sample_precision;
		uint16_t encoding_precision;
		uint16_t aspect_ratio_information;
		uint16_t frame_rate_code;
		uint16_t bit_rate_lower;
		uint16_t bit_rate_upper;
		uint16_t low_delay;
		uint16_t temporal_id_exist_flag;
		uint16_t g_uiMaxSizeInBit;

#define BACKGROUND_PICTURE_DISABLE_BIT         11
#define B_MHPSKIP_ENABLED_BIT                  10
#define DHP_ENABLED_BIT                         9
#define WSM_ENABLED_BIT                        8
#define INTER_AMP_ENABLE_BIT                   7
#define USENSQT_BIT                            6
#define USESDIP_BIT                            5
#define B_SECT_ENABLED_BIT                     4
#define SAO_ENABLE_BIT                         3
#define ALF_ENABLE_BIT                         2
#define B_PMVR_ENABLED_BIT                     1
#define CROSSSLICELOOPFILTER_BIT               0
		uint16_t avs2_seq_flags;

		uint16_t num_of_RPS;
		uint16_t picture_reorder_delay;
		/*PIC*/
		uint16_t time_code_flag;
		uint16_t time_code;
		uint16_t background_picture_flag;
		uint16_t background_picture_output_flag;
		uint16_t coding_order;
		uint16_t cur_layer;
		uint16_t displaydelay; /*???*/
		uint16_t predict;     /*???*/
		uint16_t RPS_idx;      /*???*/
		uint16_t referd_by_others_cur;
		uint16_t num_of_ref_cur;
		uint16_t ref_pic_cur[8];
		uint16_t num_to_remove_cur;
		uint16_t remove_pic_cur[8];
		uint16_t progressive_frame;
		uint16_t picture_structure;
		uint16_t top_field_first;
		uint16_t repeat_first_field;
		uint16_t is_top_field;

		uint16_t picture_coding_type;
		uint16_t background_pred_flag;
		uint16_t background_reference_enable;
		uint16_t random_access_decodable_flag;
		uint16_t lcu_size;
		uint16_t alpha_c_offset;
		uint16_t beta_offset;
		uint16_t chroma_quant_param_delta_cb;
		uint16_t chroma_quant_param_delta_cr;
		uint16_t loop_filter_disable;

		uint16_t video_signal_type;
		uint16_t color_description;
		uint16_t display_primaries_x[3];
		uint16_t display_primaries_y[3];
		uint16_t white_point_x;
		uint16_t white_point_y;
		uint16_t max_display_mastering_luminance;
		uint16_t min_display_mastering_luminance;
		uint16_t max_content_light_level;
		uint16_t max_picture_average_light_level;
	} p;
	struct {
		uint16_t padding[ALF_BEGIN - RPM_BEGIN];
		uint16_t picture_alf_enable_Y;
		uint16_t picture_alf_enable_Cb;
		uint16_t picture_alf_enable_Cr;
		uint16_t alf_filters_num_m_1;
		uint16_t region_distance[16];
		uint16_t alf_cb_coeffmulti[9];
		uint16_t alf_cr_coeffmulti[9];
		uint16_t alf_y_coeffmulti[16][9];
	} alf;
};


struct avs2_decoder {
	uint8_t init_hw_flag;
	struct inp_par   input;
	struct ImageParameters_s  img;
	struct Video_Com_data_s  hc;
	struct Video_Dec_data_s  hd;
	union param_u param;
	struct avs2_frame_s frm_pool[AVS2_MAX_BUFFER_NUM];
	struct avs2_frame_s *fref[REF_MAXBUFFER];
#ifdef AML
	/*used for background
	when background_picture_output_flag is 0*/
	struct avs2_frame_s *m_bg;
	/*current background picture, ether m_bg or fref[..]*/
	struct avs2_frame_s *f_bg;
#endif
	struct outdata_s outprint;
	uint32_t cm_header_start;
	struct ALFParam_s m_alfPictureParam[NUM_ALF_COMPONENT];
#ifdef FIX_CHROMA_FIELD_MV_BK_DIST
	int8_t bk_img_is_top_field;
#endif
#ifdef AML
	int32_t lcu_size;
	int32_t lcu_size_log2;
	int32_t lcu_x_num;
	int32_t lcu_y_num;
	int32_t lcu_total;
	int32_t ref_maxbuffer;
	int32_t to_prepare_disp_count;
	int8_t bufmgr_error_flag;
#endif
};


extern void write_frame(struct avs2_decoder *avs2_dec, int32_t pos);
extern void init_frame_t(struct avs2_frame_s *currfref);
extern void report_frame(struct avs2_decoder *avs2_dec,
	struct outdata_s *data, int32_t pos);

extern int avs2_post_process(struct avs2_decoder *avs2_dec);
extern void avs2_prepare_header(struct avs2_decoder *avs2_dec,
	int32_t start_code);
extern int32_t avs2_process_header(struct avs2_decoder *avs2_dec);

extern void init_avs2_decoder(struct avs2_decoder *avs2_dec);

extern int32_t avs2_init_global_buffers(struct avs2_decoder *avs2_dec);

extern bool is_avs2_print_param(void);
extern bool is_avs2_print_bufmgr_detail(void);
#endif

