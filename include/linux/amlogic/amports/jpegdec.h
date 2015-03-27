#ifndef JPEGDEC_H
#define JPEGDEC_H

#define JPEGDEC_IOC_MAGIC  'J'

#define JPEGDEC_IOC_INFOCONFIG	_IOW(JPEGDEC_IOC_MAGIC, 0x00, unsigned int)
#define JPEGDEC_IOC_DECCONFIG	_IOW(JPEGDEC_IOC_MAGIC, 0x01, unsigned int)
#define JPEGDEC_IOC_INFO _IOW(JPEGDEC_IOC_MAGIC, 0x02, unsigned int)
#define JPEGDEC_IOC_STAT _IOW(JPEGDEC_IOC_MAGIC, 0x03, unsigned int)
#define JPEGDEC_G_MEM_INFO _IOW(JPEGDEC_IOC_MAGIC, 0x04, unsigned int)

#define JPEGDEC_OPT_THUMBNAIL_ONLY		0x01
#define JPEGDEC_OPT_THUMBNAIL_PREFERED	0x02
#define JPEGDEC_OPT_FULLRANGE			0x04
#define JPEGDEC_OPT_SRC_CROP			0x08

#define JPEGINFO_TYPE_PROGRESSIVE		0x01
#define JPEGINFO_TYPE_MULTISCAN			0x02
#define JPEGINFO_TYPE_GRAYSCALE			0x04

#define JPEGDEC_STAT_WAIT_DATA			0x01
#define JPEGDEC_STAT_WAIT_INFOCONFIG	0x02
#define JPEGDEC_STAT_WAIT_DECCONFIG		0x04
#define JPEGDEC_STAT_ERROR				0x08
#define JPEGDEC_STAT_UNSUPPORT			0x10
#define JPEGDEC_STAT_INFO_READY			0x20
#define JPEGDEC_STAT_DONE				0x40
enum jpegdec_angle_e {
	CLKWISE_0 = 0,
	CLKWISE_90 = 1,
	CLKWISE_180 = 2,
	CLKWISE_270 = 3,
};

struct jpegdec_config_s {

	unsigned long addr_y;

	unsigned long addr_u;

	unsigned long addr_v;

	unsigned canvas_width;

	unsigned opt;

	unsigned src_crop_x;

	unsigned src_crop_y;

	unsigned src_crop_w;

	unsigned src_crop_h;

	unsigned dec_x;

	unsigned dec_y;

	unsigned dec_w;

	unsigned dec_h;

	jpegdec_angle_t angle;
};

struct jpegdec_info_s {

	unsigned width;

	unsigned height;

	unsigned comp_num;

	unsigned type;
};

struct jpegdec_mem_info_s {

	jpegdec_angle_t angle;

	unsigned dec_w;

	unsigned dec_h;

	unsigned canv_addr;

	unsigned canv_len;
};

#endif				/* JPEGDEC_H */
