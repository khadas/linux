#ifndef H264_DPB_H_
#define H264_DPB_H_

#define ERROR_CHECK

#define OUTPUT_BUFFER_IN_C

#define PRINT_FLAG_ERROR              0x0
#define PRINT_FLAG_DPB                0X0001
#define PRINT_FLAG_DPB_DETAIL         0x0002
#define PRINT_FLAG_DUMP_DPB           0x0004
#define PRINT_FLAG_UCODE_EVT          0x0008
#define PRINT_FLAG_VDEC_STATUS        0x0010
#define PRINT_FLAG_VDEC_DETAIL        0x0020
#define PRINT_FLAG_UCODE_DBG          0x0040
#define PRINT_FLAG_TIME_STAMP         0x0080
#define PRINT_FLAG_RUN_SCHEDULE       0x0100
#define PRINT_FLAG_DEBUG_POC          0x0200
#define PRINT_FLAG_VDEC_DATA          0x0400
#define RRINT_FLAG_RPM                0x0800
#define DISABLE_ERROR_HANDLE          0x10000
#define OUTPUT_CURRENT_BUF            0x20000
#define ONLY_RESET_AT_START           0x40000
#define CLEAR_INIT_FLAG_REG           0x80000
#define FORCE_NO_SLICE                0x100000
#define REINIT_DPB_TEST               0x200000


#define MVC_EXTENSION_ENABLE 0
#define PRINTREFLIST  0

#define MAX_LIST_SIZE 33

#define FALSE 0

#define H264_SLICE_HEAD_DONE         0x01
#define H264_PIC_DATA_DONE          0x02
/*#define H264_SPS_DONE               0x03*/
/*#define H264_PPS_DONE               0x04*/
/*#define H264_SLICE_DATA_DONE        0x05*/
/*#define H264_DATA_END               0x06*/

#define H264_CONFIG_REQUEST         0x11
#define H264_DATA_REQUEST           0x12

#define H264_DECODE_BUFEMPTY        0x20
#define H264_DECODE_TIMEOUT         0x21
#define H264_SEARCH_BUFEMPTY        0x22

#define H264_FIND_NEXT_PIC_NAL              0x50
#define H264_FIND_NEXT_DVEL_NAL             0x51
#define H264_AUX_DATA_READY					0x52

    /* 0x8x, search state*/
#define H264_STATE_SEARCH_AFTER_SPS  0x80
#define H264_STATE_SEARCH_AFTER_PPS  0x81
#define H264_STATE_PARSE_SLICE_HEAD  0x82
#define H264_STATE_SEARCH_HEAD       0x83
  /**/
#define H264_ACTION_SEARCH_HEAD     0xf0
#define H264_ACTION_DECODE_SLICE    0xf1
#define H264_ACTION_CONFIG_DONE     0xf2
#define H264_ACTION_DECODE_NEWPIC   0xf3
#define H264_ACTION_DECODE_START    0xff

#define RPM_BEGIN			0x0
#define RPM_END				0x400

#define val(s) (s[0]|(s[1]<<16))

#define FRAME_IN_DPB	24
#define DPB_OFFSET		0x100
#define MMCO_OFFSET		0x200
union param {
#if 0
#define H_TIME_STAMP_START	0X00
#define H_TIME_STAMP_END	0X17
#define PTS_ZERO_0		0X18
#define PTS_ZERO_1		0X19
#endif
#define FIXED_FRAME_RATE_FLAG                   0X21

#define OFFSET_DELIMITER_LO                     0x2f
#define OFFSET_DELIMITER_HI                     0x30


#define SLICE_IPONLY_BREAK						0X5C
#define PREV_MAX_REFERENCE_FRAME_NUM					0X5D
#define EOS								0X5E
#define FRAME_PACKING_TYPE						0X5F
#define OLD_POC_PAR_1							0X60
#define OLD_POC_PAR_2							0X61
#define PREV_MBX							0X62
#define PREV_MBY							0X63
#define ERROR_SKIP_MB_NUM						0X64
#define ERROR_MB_STATUS							0X65
#define L0_PIC0_STATUS							0X66
#define TIMEOUT_COUNTER							0X67
#define BUFFER_SIZE							0X68
#define BUFFER_SIZE_HI							0X69
#define CROPPING_LEFT_RIGHT						0X6A
#define CROPPING_TOP_BOTTOM						0X6B
#define POC_SELECT_NEED_SWAP						0X6C
#define POC_SELECT_SWAP							0X6D
#define MAX_BUFFER_FRAME						0X6E

#define NON_CONFORMING_STREAM						0X70
#define RECOVERY_POINT							0X71
#define POST_CANVAS							0X72
#define POST_CANVAS_H							0X73
#define SKIP_PIC_COUNT							0X74
#define TARGET_NUM_SCALING_LIST						0X75
#define FF_POST_ONE_FRAME						0X76
#define PREVIOUS_BIT_CNT						0X77
#define MB_NOT_SHIFT_COUNT						0X78
#define PIC_STATUS							0X79
#define FRAME_COUNTER							0X7A
#define NEW_SLICE_TYPE							0X7B
#define NEW_PICTURE_STRUCTURE						0X7C
#define NEW_FRAME_NUM							0X7D
#define NEW_IDR_PIC_ID							0X7E
#define IDR_PIC_ID							0X7F

/* h264 LOCAL */
#define NAL_UNIT_TYPE							0X80
#define NAL_REF_IDC							0X81
#define SLICE_TYPE							0X82
#define LOG2_MAX_FRAME_NUM						0X83
#define FRAME_MBS_ONLY_FLAG						0X84
#define PIC_ORDER_CNT_TYPE						0X85
#define LOG2_MAX_PIC_ORDER_CNT_LSB					0X86
#define PIC_ORDER_PRESENT_FLAG						0X87
#define REDUNDANT_PIC_CNT_PRESENT_FLAG					0X88
#define PIC_INIT_QP_MINUS26						0X89
#define DEBLOCKING_FILTER_CONTROL_PRESENT_FLAG				0X8A
#define NUM_SLICE_GROUPS_MINUS1						0X8B
#define MODE_8X8_FLAGS							0X8C
#define ENTROPY_CODING_MODE_FLAG					0X8D
#define SLICE_QUANT							0X8E
#define TOTAL_MB_HEIGHT							0X8F
#define PICTURE_STRUCTURE						0X90
#define TOP_INTRA_TYPE							0X91
#define RV_AI_STATUS							0X92
#define AI_READ_START							0X93
#define AI_WRITE_START							0X94
#define AI_CUR_BUFFER							0X95
#define AI_DMA_BUFFER							0X96
#define AI_READ_OFFSET							0X97
#define AI_WRITE_OFFSET							0X98
#define AI_WRITE_OFFSET_SAVE						0X99
#define RV_AI_BUFF_START						0X9A
#define I_PIC_MB_COUNT							0X9B
#define AI_WR_DCAC_DMA_CTRL						0X9C
#define SLICE_MB_COUNT							0X9D
#define PICTYPE								0X9E
#define SLICE_GROUP_MAP_TYPE						0X9F
#define MB_TYPE								0XA0
#define MB_AFF_ADDED_DMA						0XA1
#define PREVIOUS_MB_TYPE						0XA2
#define WEIGHTED_PRED_FLAG						0XA3
#define WEIGHTED_BIPRED_IDC						0XA4
/* bit 3:2 - PICTURE_STRUCTURE
 * bit 1 - MB_ADAPTIVE_FRAME_FIELD_FLAG
 * bit 0 - FRAME_MBS_ONLY_FLAG
 */
#define MBFF_INFO							0XA5
#define TOP_INTRA_TYPE_TOP						0XA6

#define RV_AI_BUFF_INC							0xa7

#define DEFAULT_MB_INFO_LO						0xa8

/* 0 -- no need to read
 * 1 -- need to wait Left
 * 2 -- need to read Intra
 * 3 -- need to read back MV
 */
#define NEED_READ_TOP_INFO						0xa9
/* 0 -- idle
 * 1 -- wait Left
 * 2 -- reading top Intra
 * 3 -- reading back MV
 */
#define READ_TOP_INFO_STATE						0xaa
#define DCAC_MBX							0xab
#define TOP_MB_INFO_OFFSET						0xac
#define TOP_MB_INFO_RD_IDX						0xad
#define TOP_MB_INFO_WR_IDX						0xae

#define VLD_NO_WAIT     0
#define VLD_WAIT_BUFFER 1
#define VLD_WAIT_HOST   2
#define VLD_WAIT_GAP	3

#define VLD_WAITING							0xaf

#define MB_X_NUM							0xb0
/* #define MB_WIDTH							0xb1 */
#define MB_HEIGHT							0xb2
#define MBX								0xb3
#define TOTAL_MBY							0xb4
#define INTR_MSK_SAVE							0xb5

/* #define has_time_stamp						0xb6 */
#define NEED_DISABLE_PPE						0xb6
#define IS_NEW_PICTURE							0XB7
#define PREV_NAL_REF_IDC						0XB8
#define PREV_NAL_UNIT_TYPE						0XB9
#define FRAME_MB_COUNT							0XBA
#define SLICE_GROUP_UCODE						0XBB
#define SLICE_GROUP_CHANGE_RATE						0XBC
#define SLICE_GROUP_CHANGE_CYCLE_LEN					0XBD
#define DELAY_LENGTH							0XBE
#define PICTURE_STRUCT							0XBF
/* #define pre_picture_struct						0xc0 */
#define DCAC_PREVIOUS_MB_TYPE						0xc1

#define TIME_STAMP							0XC2
#define H_TIME_STAMP							0XC3
#define VPTS_MAP_ADDR							0XC4
#define H_VPTS_MAP_ADDR							0XC5

#define MAX_DPB_SIZE							0XC6
#define PIC_INSERT_FLAG							0XC7

#define TIME_STAMP_START						0XC8
#define TIME_STAMP_END							0XDF

#define OFFSET_FOR_NON_REF_PIC						0XE0
#define OFFSET_FOR_TOP_TO_BOTTOM_FIELD					0XE2
#define MAX_REFERENCE_FRAME_NUM						0XE4
#define FRAME_NUM_GAP_ALLOWED						0XE5
#define NUM_REF_FRAMES_IN_PIC_ORDER_CNT_CYCLE				0XE6
#define PROFILE_IDC_MMCO						0XE7
#define LEVEL_IDC_MMCO							0XE8
#define FRAME_SIZE_IN_MB						0XE9
#define DELTA_PIC_ORDER_ALWAYS_ZERO_FLAG				0XEA
#define PPS_NUM_REF_IDX_L0_ACTIVE_MINUS1				0XEB
#define PPS_NUM_REF_IDX_L1_ACTIVE_MINUS1				0XEC
#define CURRENT_SPS_ID							0XED
#define CURRENT_PPS_ID							0XEE
/* bit 0 - sequence parameter set may change
 * bit 1 - picture parameter set may change
 * bit 2 - new dpb just inited
 * bit 3 - IDR picture not decoded yet
 * bit 5:4 - 0: mb level code loaded 1: picture
 * level code loaded 2: slice level code loaded
 */
#define DECODE_STATUS							0XEF
#define FIRST_MB_IN_SLICE						0XF0
#define PREV_MB_WIDTH							0XF1
#define PREV_FRAME_SIZE_IN_MB						0XF2
#define MAX_REFERENCE_FRAME_NUM_IN_MEM					0XF3
/* bit 0 - aspect_ratio_info_present_flag
 * bit 1 - timing_info_present_flag
 * bit 2 - nal_hrd_parameters_present_flag
 * bit 3 - vcl_hrd_parameters_present_flag
 * bit 4 - pic_struct_present_flag
 * bit 5 - bitstream_restriction_flag
 */
#define VUI_STATUS							0XF4
#define ASPECT_RATIO_IDC						0XF5
#define ASPECT_RATIO_SAR_WIDTH						0XF6
#define ASPECT_RATIO_SAR_HEIGHT						0XF7
#define NUM_UNITS_IN_TICK						0XF8
#define TIME_SCALE							0XFA
#define CURRENT_PIC_INFO						0XFC
#define DPB_BUFFER_INFO							0XFD
#define	REFERENCE_POOL_INFO						0XFE
#define REFERENCE_LIST_INFO						0XFF
	struct{
		unsigned short data[RPM_END-RPM_BEGIN];
	} l;
	struct{
		unsigned short dump[DPB_OFFSET];
		unsigned short dpb_base[FRAME_IN_DPB<<3];

		unsigned short dpb_max_buffer_frame;
		unsigned short actual_dpb_size;

		unsigned short colocated_buf_status;

		unsigned short num_forward_short_term_reference_pic;
		unsigned short num_short_term_reference_pic;
		unsigned short num_reference_pic;

		unsigned short current_dpb_index;
		unsigned short current_decoded_frame_num;
		unsigned short current_reference_frame_num;

		unsigned short l0_size;
		unsigned short l1_size;

		/* [6:5] : nal_ref_idc */
		/* [4:0] : nal_unit_type */
		unsigned short NAL_info_mmco;

		/* [1:0] : 00 - top field, 01 - bottom field,
		   10 - frame, 11 - mbaff frame */
		unsigned short picture_structure_mmco;

		unsigned short frame_num;
		unsigned short pic_order_cnt_lsb;

		unsigned short num_ref_idx_l0_active_minus1;
		unsigned short num_ref_idx_l1_active_minus1;

		unsigned short PrevPicOrderCntLsb;
		unsigned short PreviousFrameNum;

		/* 32 bits variables */
		unsigned short delta_pic_order_cnt_bottom[2];
		unsigned short	delta_pic_order_cnt_0[2];
		unsigned short delta_pic_order_cnt_1[2];

		unsigned short PrevPicOrderCntMsb[2];
		unsigned short PrevFrameNumOffset[2];

		unsigned short frame_pic_order_cnt[2];
		unsigned short top_field_pic_order_cnt[2];
		unsigned short bottom_field_pic_order_cnt[2];

		unsigned short colocated_mv_addr_start[2];
		unsigned short colocated_mv_addr_end[2];
		unsigned short colocated_mv_wr_addr[2];
	} dpb;
	struct {
		unsigned short dump[MMCO_OFFSET];

		/* array base address for offset_for_ref_frame */
		unsigned short offset_for_ref_frame_base[128];

		/* 0 - Index in DPB
		 * 1 - Picture Flag
		 *  [    2] : 0 - short term reference,
		 *            1 - long term reference
		 *  [    1] : bottom field
		 *  [    0] : top field
		 * 2 - Picture Number (short term or long term) low 16 bits
		 * 3 - Picture Number (short term or long term) high 16 bits
		 */
		unsigned short	reference_base[128];

		/* command and parameter, until command is 3 */
		unsigned short l0_reorder_cmd[66];
		unsigned short l1_reorder_cmd[66];

		/* command and parameter, until command is 0 */
		unsigned short mmco_cmd[44];

		unsigned short l0_base[40];
		unsigned short l1_base[40];
	} mmco;
	struct {
		/* from ucode lmem, do not change this struct */
	} p;
};


struct StorablePicture;
struct VideoParameters;
struct DecodedPictureBuffer;

/* New enum for field processing */
enum PictureStructure {
	FRAME,
	TOP_FIELD,
	BOTTOM_FIELD
};

#define I_Slice                               2
#define P_Slice                               5
#define B_Slice                               6
#define P_Slice_0                             0
#define B_Slice_1                             1
#define I_Slice_7                             7

enum SliceType {
	P_SLICE = 0,
	B_SLICE = 1,
	I_SLICE = 2,
	SP_SLICE = 3,
	SI_SLICE = 4,
	NUM_SLICE_TYPES = 5
};

struct SPSParameters {
	int pic_order_cnt_type;
	int log2_max_pic_order_cnt_lsb_minus4;
	int num_ref_frames_in_pic_order_cnt_cycle;
	short offset_for_ref_frame[128];
	short offset_for_non_ref_pic;
	short offset_for_top_to_bottom_field;

	/**/
	int frame_mbs_only_flag;
	int num_ref_frames;
	int max_dpb_size;

	int log2_max_frame_num_minus4;
};

#define DEC_REF_PIC_MARKING_BUFFER_NUM_MAX   45
struct DecRefPicMarking_s {
	int memory_management_control_operation;
	int difference_of_pic_nums_minus1;
	int long_term_pic_num;
	int long_term_frame_idx;
	int max_long_term_frame_idx_plus1;
	struct DecRefPicMarking_s *Next;
};

#define REORDERING_COMMAND_MAX_SIZE    33
struct Slice {
	int first_mb_in_slice;
	int mode_8x8_flags;
	int picture_structure_mmco;

	int frame_num;
	int idr_flag;
	int toppoc;
	int bottompoc;
	int framepoc;
	int pic_order_cnt_lsb;
	int PicOrderCntMsb;
	unsigned char field_pic_flag;
	unsigned char bottom_field_flag;
	int ThisPOC;
	int nal_reference_idc;
	int AbsFrameNum;
	int delta_pic_order_cnt_bottom;
	int delta_pic_order_cnt[2];

	/**/
	char listXsize[6];
	struct StorablePicture *listX[6][MAX_LIST_SIZE * 2];

	/**/
	enum PictureStructure structure;
	int long_term_reference_flag;
	int no_output_of_prior_pics_flag;
	int adaptive_ref_pic_buffering_flag;

	struct VideoParameters *p_Vid;
	struct DecodedPictureBuffer *p_Dpb;
	int num_ref_idx_active[2];    /* number of available list references */

	/*modification*/
	int slice_type;    /* slice type */
	int ref_pic_list_reordering_flag[2];
	int modification_of_pic_nums_idc[2][REORDERING_COMMAND_MAX_SIZE];
	int abs_diff_pic_num_minus1[2][REORDERING_COMMAND_MAX_SIZE];
	int long_term_pic_idx[2][REORDERING_COMMAND_MAX_SIZE];
	/**/
	unsigned char dec_ref_pic_marking_buffer_valid;
	struct DecRefPicMarking_s
		dec_ref_pic_marking_buffer[DEC_REF_PIC_MARKING_BUFFER_NUM_MAX];
};

struct OldSliceParams {
	unsigned field_pic_flag;
	unsigned frame_num;
	int      nal_ref_idc;
	unsigned pic_oder_cnt_lsb;
	int      delta_pic_oder_cnt_bottom;
	int      delta_pic_order_cnt[2];
	unsigned char     bottom_field_flag;
	unsigned char     idr_flag;
	int      idr_pic_id;
	int      pps_id;
#if (MVC_EXTENSION_ENABLE)
	int      view_id;
	int      inter_view_flag;
	int      anchor_pic_flag;
#endif
	int      layer_id;
};

struct VideoParameters {
	int PrevPicOrderCntMsb;
	int PrevPicOrderCntLsb;
	unsigned char last_has_mmco_5;
	unsigned char last_pic_bottom_field;
	int ThisPOC;
	int PreviousFrameNum;
	int FrameNumOffset;
	int PreviousFrameNumOffset;
	int max_frame_num;
	unsigned int pre_frame_num;
	int ExpectedDeltaPerPicOrderCntCycle;
	int PicOrderCntCycleCnt;
	int FrameNumInPicOrderCntCycle;
	int ExpectedPicOrderCnt;

	/**/
	struct SPSParameters *active_sps;
	struct Slice **ppSliceList;
	int iSliceNumOfCurrPic;
	int conceal_mode;
	int earlier_missing_poc;
	int pocs_in_dpb[100];

	struct OldSliceParams old_slice;
	/**/
	struct StorablePicture *dec_picture;
	struct StorablePicture *no_reference_picture;

	/*modification*/
	int non_conforming_stream;
	int recovery_point;
};

static inline int imin(int a, int b)
{
	return ((a) < (b)) ? (a) : (b);
}

static inline int imax(int a, int b)
{
	return ((a) > (b)) ? (a) : (b);
}

#define MAX_PIC_BUF_NUM 128
#define MAX_NUM_SLICES 50

struct StorablePicture {
/**/
	int width;
	int height;

	int y_canvas_index;
	int u_canvas_index;
	int v_canvas_index;
/**/
	int index;
	unsigned char is_used;

	enum PictureStructure structure;

	int         poc;
	int         top_poc;
	int         bottom_poc;
	int         frame_poc;
	unsigned int  frame_num;
	unsigned int  recovery_frame;

	int         pic_num;
	int         buf_spec_num;
	int         colocated_buf_index;
	int         long_term_pic_num;
	int         long_term_frame_idx;

	unsigned char  is_long_term;
	int         used_for_reference;
	int         is_output;
#if 1
	/* rain */
	int         pre_output;
#endif
	int         non_existing;
	int         separate_colour_plane_flag;

	short       max_slice_id;

	int         size_x, size_y, size_x_cr, size_y_cr;
	int         size_x_m1, size_y_m1, size_x_cr_m1, size_y_cr_m1;
	int         coded_frame;
	int         mb_aff_frame_flag;
	unsigned    PicWidthInMbs;
	unsigned    PicSizeInMbs;
	int         iLumaPadY, iLumaPadX;
	int         iChromaPadY, iChromaPadX;

	/* for mb aff, if frame for referencing the top field */
	struct StorablePicture *top_field;
	/* for mb aff, if frame for referencing the bottom field */
	struct StorablePicture *bottom_field;
	/* for mb aff, if field for referencing the combined frame */
	struct StorablePicture *frame;

	int         slice_type;
	int         idr_flag;
	int         no_output_of_prior_pics_flag;
	int         long_term_reference_flag;
	int         adaptive_ref_pic_buffering_flag;

	int         chroma_format_idc;
	int         frame_mbs_only_flag;
	int         frame_cropping_flag;
	int         frame_crop_left_offset;
	int         frame_crop_right_offset;
	int         frame_crop_top_offset;
	int         frame_crop_bottom_offset;
	int         qp;
	int         chroma_qp_offset[2];
	int         slice_qp_delta;
	/* stores the memory management control operations */
	struct DecRefPicMarking_s *dec_ref_pic_marking_buffer;

	/* picture error concealment */
	/*indicates if this is a concealed picture */
	int         concealed_pic;

	/* variables for tone mapping */
	int         seiHasTone_mapping;
	int         tone_mapping_model_id;
	int         tonemapped_bit_depth;
	/* imgpel*     tone_mapping_lut; tone mapping look up table */

	int         proc_flag;
#if (MVC_EXTENSION_ENABLE)
	int         view_id;
	int         inter_view_flag;
	int         anchor_pic_flag;
#endif
	int         iLumaStride;
	int         iChromaStride;
	int         iLumaExpandedHeight;
	int         iChromaExpandedHeight;
	/* imgpel **cur_imgY; for more efficient get_block_luma */
	int no_ref;
	int iCodingType;

	char listXsize[MAX_NUM_SLICES][2];
	struct StorablePicture **listX[MAX_NUM_SLICES][2];
	int         layer_id;

	int offset_delimiter_lo;
	int offset_delimiter_hi;

	u32         pts;
	u64         pts64;
};

struct FrameStore {
	/* rain */
	int      buf_spec_num;
	/* rain */
	int      colocated_buf_index;

	/* 0=empty; 1=top; 2=bottom; 3=both fields (or frame) */
	int       is_used;
	/* 0=not used for ref; 1=top used; 2=bottom used;
	 * 3=both fields (or frame) used */
	int       is_reference;
	/* 0=not used for ref; 1=top used; 2=bottom used;
	 * 3=both fields (or frame) used */
	int       is_long_term;
	/* original marking by nal_ref_idc: 0=not used for ref; 1=top used;
	 * 2=bottom used; 3=both fields (or frame) used */
	int       is_orig_reference;

	int       is_non_existent;

	unsigned  frame_num;
	unsigned  recovery_frame;

	int       frame_num_wrap;
	int       long_term_frame_idx;
	int       is_output;
#if 1
	/* rain */
	int         pre_output;
	/* index in gFrameStore */
	int       index;
#define OTHER_DATA		0
#define I_DATA			1
#define NO_DATA		0xff
	unsigned char data_flag;
#endif
	int       poc;

	/* picture error concealment */
	int concealment_reference;

	struct StorablePicture *frame;
	struct StorablePicture *top_field;
	struct StorablePicture *bottom_field;

#if (MVC_EXTENSION_ENABLE)
	int       view_id;
	int       inter_view_flag[2];
	int       anchor_pic_flag[2];
#endif
	int       layer_id;

	u32       pts;
	u64       pts64;

};


/* #define DPB_SIZE_MAX     16 */
#define DPB_SIZE_MAX     32
struct DecodedPictureBuffer {
	struct VideoParameters *p_Vid;
	/* InputParameters *p_Inp; ??? */
	struct FrameStore  *fs[DPB_SIZE_MAX];
	struct FrameStore  *fs_ref[DPB_SIZE_MAX];
	struct FrameStore  *fs_ltref[DPB_SIZE_MAX];
	/* inter-layer reference (for multi-layered codecs) */
	struct FrameStore  *fs_ilref[DPB_SIZE_MAX];
	/**/
	struct FrameStore *fs_list0[DPB_SIZE_MAX];
	struct FrameStore *fs_list1[DPB_SIZE_MAX];
	struct FrameStore *fs_listlt[DPB_SIZE_MAX];

	/**/
	unsigned      size;
	unsigned      used_size;
	unsigned      ref_frames_in_buffer;
	unsigned      ltref_frames_in_buffer;
	int           last_output_poc;
#if (MVC_EXTENSION_ENABLE)
	int           last_output_view_id;
#endif
	int           max_long_term_pic_idx;


	int           init_done;
	int           num_ref_frames;

	struct FrameStore   *last_picture;
	unsigned     used_size_il;
	int          layer_id;

	/* DPB related function; */
};

struct h264_dpb_stru {
	struct vdec_s *vdec;
	int decoder_index;

	union param dpb_param;

	int decode_idx;
	int buf_num;
	int curr_POC;
	int reorder_pic_num;
	/**/
	unsigned int max_reference_size;

	unsigned int colocated_buf_map;
	unsigned int colocated_buf_count;
	unsigned int colocated_mv_addr_start;
	unsigned int colocated_mv_addr_end;
	unsigned int colocated_buf_size;

	struct DecodedPictureBuffer mDPB;
	struct Slice mSlice;
	struct VideoParameters mVideo;
	struct SPSParameters mSPS;

	struct StorablePicture m_PIC[MAX_PIC_BUF_NUM];
	struct FrameStore mFrameStore[DPB_SIZE_MAX];

	/*vui*/
	unsigned int vui_status;
	unsigned int num_units_in_tick;
	unsigned int time_scale;
	unsigned int fixed_frame_rate_flag;
	unsigned int aspect_ratio_idc;
	unsigned int aspect_ratio_sar_width;
	unsigned int aspect_ratio_sar_height;
};


extern unsigned int h264_debug_flag;
extern unsigned int h264_debug_mask;

int dpb_print(int indext, int debug_flag, const char *fmt, ...);

int dpb_print_cont(int index, int debug_flag, const char *fmt, ...);

unsigned char dpb_is_debug(int index, int debug_flag);

int prepare_display_buf(struct vdec_s *vdec, struct FrameStore *frame);

int release_buf_spec_num(struct vdec_s *vdec, int buf_spec_num);

void set_frame_output_flag(struct h264_dpb_stru *p_H264_Dpb, int index);

int is_there_unused_frame_from_dpb(struct DecodedPictureBuffer *p_Dpb);

int h264_slice_header_process(struct h264_dpb_stru *p_H264_Dpb);

void dpb_init_global(struct h264_dpb_stru *p_H264_Dpb,
	int id, int actual_dpb_size, int max_reference_size);

void init_colocate_buf(struct h264_dpb_stru *p_H264_Dpb, int count);

int release_colocate_buf(struct h264_dpb_stru *p_H264_Dpb, int index);

int get_free_buf_idx(struct vdec_s *vdec);

void store_picture_in_dpb(struct h264_dpb_stru *p_H264_Dpb,
			struct StorablePicture *p, unsigned char data_flag);

int remove_picture(struct h264_dpb_stru *p_H264_Dpb,
			struct StorablePicture *pic);

void bufmgr_post(struct h264_dpb_stru *p_H264_Dpb);

int get_long_term_flag_by_buf_spec_num(struct h264_dpb_stru *p_H264_Dpb,
	int buf_spec_num);

void bufmgr_h264_remove_unused_frame(struct h264_dpb_stru *p_H264_Dpb);

void flush_dpb(struct h264_dpb_stru *p_H264_Dpb);
#endif
