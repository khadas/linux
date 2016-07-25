#ifndef __JPEG_ENC_H_
#define __JPEG_ENC_H_

#define JPEGENC_DEVINFO_M8 "AML-M8"
#define JPEGENC_DEVINFO_G9 "AML-G9"
#define JPEGENC_DEVINFO_GXBB "AML-GXBB"
#define JPEGENC_DEVINFO_GXTVBB "AML-GXTVBB"
#define JPEGENC_DEVINFO_GXL "AML-GXL"

/* M8: 2550/10 = 255M GX: 2000/10 = 200M */
#define JPEGENC_HDEC_L0()   WRITE_HHI_REG(HHI_VDEC_CLK_CNTL, \
			 (2 << 25) | (1 << 16) | (1 << 24) | \
			 (0xffff & READ_HHI_REG(HHI_VDEC_CLK_CNTL)))
/* M8: 2550/8 = 319M GX: 2000/8 = 250M */
#define JPEGENC_HDEC_L1()   WRITE_HHI_REG(HHI_VDEC_CLK_CNTL, \
			 (0 << 25) | (1 << 16) | (1 << 24) | \
			 (0xffff & READ_HHI_REG(HHI_VDEC_CLK_CNTL)))
/* M8: 2550/7 = 364M GX: 2000/7 = 285M */
#define JPEGENC_HDEC_L2()   WRITE_HHI_REG(HHI_VDEC_CLK_CNTL, \
			 (3 << 25) | (0 << 16) | (1 << 24) | \
			 (0xffff & READ_HHI_REG(HHI_VDEC_CLK_CNTL)))
/* M8: 2550/6 = 425M GX: 2000/6 = 333M */
#define JPEGENC_HDEC_L3()   WRITE_HHI_REG(HHI_VDEC_CLK_CNTL, \
			 (1 << 25) | (1 << 16) | (1 << 24) | \
			 (0xffff & READ_HHI_REG(HHI_VDEC_CLK_CNTL)))
/* M8: 2550/5 = 510M GX: 2000/5 = 400M */
#define JPEGENC_HDEC_L4()   WRITE_HHI_REG(HHI_VDEC_CLK_CNTL, \
			 (2 << 25) | (0 << 16) | (1 << 24) | \
			 (0xffff & READ_HHI_REG(HHI_VDEC_CLK_CNTL)))
/* M8: 2550/4 = 638M GX: 2000/4 = 500M */
#define JPEGENC_HDEC_L5()   WRITE_HHI_REG(HHI_VDEC_CLK_CNTL, \
			 (0 << 25) | (0 << 16) | (1 << 24) | \
			 (0xffff & READ_HHI_REG(HHI_VDEC_CLK_CNTL)))
/* M8: 2550/3 = 850M GX: 2000/3 = 667M */
#define JPEGENC_HDEC_L6()   WRITE_HHI_REG(HHI_VDEC_CLK_CNTL, \
			 (1 << 25) | (0 << 16) | (1 << 24) | \
			 (0xffff & READ_HHI_REG(HHI_VDEC_CLK_CNTL)))

#define jpegenc_clock_enable(level) \
	do { \
		if (level == 0)  \
			JPEGENC_HDEC_L0(); \
		else if (level == 1)  \
			JPEGENC_HDEC_L1(); \
		else if (level == 2)  \
			JPEGENC_HDEC_L2(); \
		else if (level == 3)  \
			JPEGENC_HDEC_L3(); \
		else if (level == 4)  \
			JPEGENC_HDEC_L4(); \
		else if (level == 5)  \
			JPEGENC_HDEC_L5(); \
		else if (level == 6)  \
			JPEGENC_HDEC_L6(); \
		WRITE_VREG_BITS(DOS_GCLK_EN0, 0x7fff, 12, 15); \
	} while (0)

#define jpegenc_clock_disable() \
	do { \
		WRITE_VREG_BITS(DOS_GCLK_EN0, 0, 12, 15); \
		WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  0, 24, 1); \
	} while (0)

#define JPEGENC_IOC_MAGIC  'J'

#define JPEGENC_IOC_GET_DEVINFO	_IOW(JPEGENC_IOC_MAGIC, 0xf0, u32)

#define JPEGENC_IOC_GET_BUFFINFO	_IOW(JPEGENC_IOC_MAGIC, 0x00, u32)
#define JPEGENC_IOC_CONFIG_INIT	_IOW(JPEGENC_IOC_MAGIC, 0x01, u32)
#define JPEGENC_IOC_NEW_CMD	_IOW(JPEGENC_IOC_MAGIC, 0x02, u32)
#define JPEGENC_IOC_GET_STAGE	_IOW(JPEGENC_IOC_MAGIC, 0x03, u32)
#define JPEGENC_IOC_GET_OUTPUT_SIZE	_IOW(JPEGENC_IOC_MAGIC, 0x04, u32)
#define JPEGENC_IOC_SET_EXT_QUANT_TABLE	_IOW(JPEGENC_IOC_MAGIC, 0x05, u32)

#define DCTSIZE2	    64

#define JPEGENC_FLUSH_FLAG_INPUT			0x1
#define JPEGENC_FLUSH_FLAG_OUTPUT		0x2

/* Define Quantization table:   Max two tables */
#define QUANT_SEL_COMP0     0
#define QUANT_SEL_COMP1     1
#define QUANT_SEL_COMP2     1

/* Define Huffman table selection: Max two tables per DC/AC */
#define DC_HUFF_SEL_COMP0   0
#define DC_HUFF_SEL_COMP1   1
#define DC_HUFF_SEL_COMP2   1
#define AC_HUFF_SEL_COMP0   0
#define AC_HUFF_SEL_COMP1   1
#define AC_HUFF_SEL_COMP2   1

/* DCT interrupt select:0=Disable intr;
    1=Intr at end of each 8x8 block of DCT input;
    2=Intr at end of each MCU of DCT input;
    3=Intr at end of a scan of DCT input;
    4=Intr at end of each 8x8 block of DCT output;
    5=Intr at end of each MCU of DCT output;
    6=Intr at end of a scan of DCT output; */
#define JDCT_INTR_SEL       0

/* 0=Mark last coeff at the end of an 8x8 block,
    1=Mark last coeff at the end of an MCU
    2=Mark last coeff at the end of a scan */
#define JDCT_LASTCOEFF_SEL  1

enum jpegenc_mem_type_e {
	JPEGENC_LOCAL_BUFF = 0,
	JPEGENC_CANVAS_BUFF,
	JPEGENC_PHYSICAL_BUFF,
	JPEGENC_MAX_BUFF_TYPE
};

enum jpegenc_frame_fmt_e {
	JPEGENC_FMT_YUV422_SINGLE = 0,
	JPEGENC_FMT_YUV444_SINGLE,
	JPEGENC_FMT_NV21,
	JPEGENC_FMT_NV12,
	JPEGENC_FMT_YUV420,
	JPEGENC_FMT_YUV444_PLANE,
	JPEGENC_FMT_RGB888,
	JPEGENC_FMT_RGB888_PLANE,
	JPEGENC_FMT_RGB565,
	JPEGENC_FMT_RGBA8888,
	JPEGENC_FMT_YUV422_12BIT,
	JPEGENC_FMT_YUV444_10BIT,
	JPEGENC_FMT_YUV422_10BIT,
	JPEGENC_MAX_FRAME_FMT
};

struct Jpegenc_Buff_s {
	u32 buf_start;
	u32 buf_size;
};

struct Jpegenc_BuffInfo_s {
	u32 lev_id;
	u32 max_width;
	u32 max_height;
	u32 min_buffsize;
	struct Jpegenc_Buff_s input;
	struct Jpegenc_Buff_s assit;
	struct Jpegenc_Buff_s bitstream;
};

struct jpegenc_request_s {
	u32 src;
	u32 encoder_width;
	u32 encoder_height;
	u32 framesize;
	u32 jpeg_quality;
	u32 QuantTable_id;
	u32 flush_flag;
	enum jpegenc_mem_type_e type;
	enum jpegenc_frame_fmt_e input_fmt;
	enum jpegenc_frame_fmt_e output_fmt;
};

struct jpegenc_meminfo_s {
	u32 buf_start;
	u32 buf_size;
	u8 cur_buf_lev;

#ifdef CONFIG_CMA
	ulong cma_pool_size;
#endif
	struct Jpegenc_Buff_s reserve_mem;
	struct Jpegenc_BuffInfo_s *bufspec;
};

struct jpegenc_wq_s {
	u32 hw_status;
	u32 headbytes;
	u32 output_size;

	u32 buf_start;
	u32 buf_size;

	u32 InputBuffStart;
	u32 InputBuffEnd;

	u32 AssitStart;
	u32 AssitEnd;

	u32 BitstreamStart;
	u32 BitstreamEnd;
	void __iomem *AssitstreamStartVirtAddr;

	u32 max_width;
	u32 max_height;

	struct jpegenc_request_s cmd;
	atomic_t ready;
	wait_queue_head_t complete;
#ifdef CONFIG_CMA
	struct page *venc_pages;
#endif
};

struct jpegenc_manager_s {
	u32 encode_hw_status;
	s32 irq_num;

	bool irq_requested;
	bool process_irq;
	bool inited;
	bool use_reserve;
	u8 opened;

	spinlock_t sem_lock;
	struct platform_device *this_pdev;
	struct jpegenc_meminfo_s mem;
	struct jpegenc_wq_s wq;
	struct tasklet_struct tasklet;
};

/********************************************
 *  AV Scratch Register Re-Define
********************************************/
#define JPEGENC_ENCODER_STATUS		HCODEC_HENC_SCRATCH_0
#define JPEGENC_BITSTREAM_OFFSET	HCODEC_HENC_SCRATCH_1

/*********************************************
 * ENCODER_STATUS define
********************************************/
#define JPEGENC_ENCODER_IDLE		0
#define JPEGENC_ENCODER_START		1
/* #define JPEGENC_ENCODER_SOS_HEADER          2 */
#define JPEGENC_ENCODER_MCU		3
#define JPEGENC_ENCODER_DONE		4

extern bool jpegenc_on(void);
#endif
