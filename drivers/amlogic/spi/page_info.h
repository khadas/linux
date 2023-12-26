/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _PAGE_INFO_H_
#define _PAGE_INFO_H_

#include <linux/mtd/spinand.h>
#include <linux/mtd/mtd.h>
#include <linux/amlogic/aml_pageinfo.h>
#include "nfc.h"

//#define __PXP_DEBUG__

#define PAGEINFO_QUICK_INIT
#define MAX_BYTES_IN_BOOTINFO	(512)

#ifdef __PXP_DEBUG__
#define NFC_PRINT(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)

#define DUMP_BUFFER(buffer, size, loop, actual)		\
{							\
	unsigned char *info;				\
	unsigned int _loop = loop;			\
	unsigned int i, j, each_size = (size) / _loop;	\
	for (i = 0; i < _loop; i++) {			\
		info = (unsigned char *)((unsigned long)(buffer) + i * each_size);	\
		for (j = 0; j < (actual); j++, info++) {\
			pr_cont(" ");			\
			pr_cont("%02x", *info);		\
		}					\
		pr_cont("\n");				\
	}						\
}
#else
#define NFC_PRINT(fmt, ...)
#define DUMP_BUFFER(buffer, size, loop, actual)
#endif

#define MAX_CLK_PROVIDER	13

#define PAGE_ERROR_MODE(mod, num)	((uint32_t)((((mod) << 16) | (num))))
#define ERR_PAGE_INFO	PAGE_ERROR_MODE(1, 1)

#define MAX_PAGEINFO_COUNT	21

extern struct boot_info *page_info;
extern unsigned short pageinfo_located[MAX_PAGEINFO_COUNT];

struct boot_info {
#define BOOTINFO_MAGIC       "BOOTINFO"
#define BOOTINFO_INFO_VER    1
	char magic[8];
	unsigned char version;		/* need to greater than or equal to 2 */
	unsigned char reserved[3];	/* reserve zero */
	struct {
		/* from SPI NAND datasheet */
		unsigned int page_size;
		/* bit0~3: planes_per_lun bit4~7: plane_shift */
		unsigned char planes_per_lun;
		/* bit0~3: bus_width bit4~7: cache_plane_shift */
		/* 0, 1, 2, 3 buswidth, 2: 4 wires; 3: 8 wires */
		unsigned char bus_width;
	} dev_cfg0;

	/* the checksum of the whole boot_info struct  */
	unsigned int checksum;

	struct {
		/* frequency index, which is used to select the frequency and
		 * timing mode. we provide a array of fixed clock providers in
		 * ROM, such as the definition of clk_provider[] in nfc.c. only
		 * if a worse board used. otherwise these items should be prior
		 * of using. we considered some common frequency and timing
		 * points.
		 * 0xFF: it follows the old method to select the frequency
		 *       which means EFUSE. EFUSE can bring four type [16MHZ,
		 *       20MHZ, 41MHZ, 83MHZ] and the adjust index is also from
		 *       EFUSE bits.
		 * !0xFF: bit6-0 indicates the index of the array clk_provider[].
		 *        if bit7 of frequency index is 0, we will use the fixed
		 *	  timing parameters such as core divider, spi clock
		 *        divider form the fixed value defined in clk_provider[];
		 *        but if bit7 of frequency index is 1, we use these timing
		 *        parameters form page info.
		 */
		unsigned char frequency_index;
		/* core clock divider */
		unsigned char core_div;
		/* SPI clock divider */
		unsigned char bus_cycle;
		/* bit6-7: mode select
		 * spi nor/nand only support mode0 and mode3
		 */
		/* bit5-0: rx adjust index, to adjust the sampling point */
		unsigned char mode_rx_adj;
		/* delay value for each line */
		unsigned int lines_delay[2];
		/* decide the ECC/ran mode of bl2 pages, satisfy */
		/* only used by spi nand at now */
		unsigned int n2m_cmd;
		/* bit0 :disable the built-in ECC of SPI nand */
		unsigned char device_ecc_disable;
	} host_cfg;

	struct {
		/* unusable now */
		unsigned char high_speed_mode;
		/* bit3-0: command lanes bit7-4: address lanes */
		unsigned char ca_lanes;
		/* chip deselect time that needs from datasheet, variable */
		unsigned char cs_deselect_time;
		/* dummy clock needs by spi nor octal mode */
		unsigned char dummy_cycles;
		/* it is not used in ROM, instead used in BL2(core).
		 * because we have quick initialization in BL2(core) which
		 * reuse the page info reading in ROM stage, we don't want
		 * to read it from SPI storage again.
		 * bit31-24 : BBT page number bit23-0 : BBT start page
		 */
		unsigned int bbt_start_page;
		/* from SPI NAND datasheet */
		unsigned int block_size;

#ifdef XOR_BBT_SCAN_SUPPORT_BOOTINFO
#define	MAX_F_BAD_BLOCK_NUM	64
#else
#define	MAX_F_BAD_BLOCK_NUM	16
#endif
		unsigned short bbt[MAX_F_BAD_BLOCK_NUM];
		/* when enable_bbt == 1, the bad block skipping mechanism is
		 * enabled and bbt[] array is used.
		 */
		unsigned char enable_bbt;
#ifdef XOR_BBT_SCAN_SUPPORT_BOOTINFO
		/* decide usb update or gang programmer */
		unsigned char is_gang_programer;
		/*bit15-0 xor bbt start block */
		/*bit23-16 xor bbt block number */
		/*bit31-24 xor bbt pages */
		unsigned int xor_bbt_start_block;
		/* bit15-0  total blocks in chip */
		unsigned int block_num_in_chip;
#endif
	} dev_cfg1;

	struct {
		/* layout_method[bit0-3]: decide the layout of bootloader
		 * 0: the old method that we already supported.
		 *    bootloader is discrete, [bl2 + fip] or [1stblob + bl2e/x +
		 *    {ddrfip} + devfip]; and one page info is at the head
		 *    of bl2 or 1stblob. 1024 pages size of BL2 or 1stblob
		 *    is wasting too much space, and discreteness of bootloader
		 *    cause a more difficult design.
		 * 1: no discreteness of bootloader. one page info is at the head
		 *    of each bootloader. we can store each u-boot.bin as a whole
		 *    in nand array.
		 * 2: no discreteness of bootloader. the difference between
		 *    layout_method 1 and 2 is that we separate the page info from
		 *    each bootloader and each page info is stored in one blocks.
		 *    so the MTD bootloader is like MTD normal that we don't need
		 *    to register two nand device or handle the different interfaces.
		 *    but obviously, the copy number of page info is equal to the
		 *    copy number of bootloader, and bootloaders are separate by each
		 *    page info.
		 * 3: no discreteness of bootloader. the difference between
		 *    layout_method 2 and 3 is that bootloaders are not separate
		 *    by each page info and the copy number of page info is
		 *    maximum 4, which does not follow the copy number of bootloaders.
		 * bit7-4: boot copies
		 */
		unsigned char layout_method;
		unsigned char reserved[3];	/* reserve zero */
		/* which must be block align. in layout_method 1, the boot size
		 * is equal to <the size of each bootloader adds one page zie>
		 * in layout_method 2 and 3, the boot size is equal to the size
		 * of each bootloader,
		 */
		unsigned int boot_size;
	} boot_layout;
};

/* *NOTE* Change the setting for different SOCs */
// 2 << 20 : code1code0(10b')
// 1 << 19 : rand enable
// 1 << 17 : read, not write
// 7 << 14 : ECC mode which is from 0 to 7; Maybe different setting here,
//	   : such as 2 for AXG serials, 7 for g12a.
//	   : 0->none
//	   : 1->BCH8/512
//	   : 2->BCH8/1024
//	   : 3->BCH24/1024
//	   : 4->BCH30/1024
//	   : 5->BCH40/1024
//	   : 6->BCH50/1024
//	   : 7->BCH60/1024
// 1 << 13 : short mode enable
// 48 << 6 : ECC page size
// 1 << 0  : ECC page number
#define DEFAULT_ECC_MODE		\
(					\
	(2 << 20) |			\
	(0 << 19) |			\
	(1 << 17) |			\
	(1 << 14) |			\
	(0 << 13) |			\
	(0 << 6) |			\
	(1 << 0)			\
)

/*
 * The 2KB head of the buffer may be overwritten
 * by NAND DMA because of BL2_SIZE misaligment with nand page.
 * so add 2KB!!!
 */
//#define NFC_PAGE0_SIZE	(512)
//#define NFC_PAGE0_BUF	(BL2_NAND_PAGE_BUFFER + 0x800)
//#define NFC_INFO_BUF	(NFC_PAGE0_BUF + NFC_PAGE0_SIZE)

#define LANE_INVALID(x, y, z)	((x) > 3 || (y) > 3 || (z) > 3)
#define MODE_INVALID(x) (__extension__ ({			\
	__typeof__(x) _x = (x);					\
	(_x != 0 || _x != 3);					\
}))
#define STEPS_INVALID(x) (__extension__ ({			\
	__typeof__(x) _x = (x);					\
	(_x == 0 || _x > 16);					\
}))
#define BUS_CYCLE_INVALID(x) (__extension__ ({			\
	__typeof__(x) _x = (x);					\
	(_x == 1 || _x > 31);					\
}))
#define FREQUENCY_INDEX_INVALID(x) (__extension__ ({		\
	__typeof__(x) _x = (x);					\
	(_x != 0xFF && (_x & 0x7F) >= MAX_CLK_PROVIDER);	\
}))
#define PAGE_SIZE_INVALID(x) (__extension__ ({			\
	__typeof__(x) _x = (x);					\
	(_x != 0x800 && _x != 0x1000 && _x != 0x2000);		\
}))
#define BLOCK_SIZE_INVALID(x) (__extension__ ({			\
	__typeof__(x) _x = (x);					\
	(_x != 0x20000 && _x != 0x40000 && _x != 0x80000);	\
}))
#define PAGEC_PLANE_SHIFT_INVALID(x) (__extension__ ({		\
	__typeof__(x) _x = (x);					\
	(_x != 0 && _x != 12 && _x != 13 && _x != 14);		\
}))
#define BLOCK_PLANE_SHIFT_INVALID(x) (__extension__ ({		\
	__typeof__(x) _x = (x);					\
	(_x != 0 && _x != 6 && _x != 7 && _x != 8);		\
}))
#define PLANE_NUMBER_INVALID(x) (__extension__ ({		\
	__typeof__(x) _x = (x);					\
	(_x != 1 && _x != 2);					\
}))

void page_info_set_addr(unsigned long pi_addr);
unsigned char page_info_get_data_lanes_mode(void);
unsigned char page_info_get_cmd_lanes_mode(void);
unsigned char page_info_get_addr_lanes_mode(void);
unsigned char page_info_get_frequency_index(void);
unsigned char page_info_get_adj_index(void);
unsigned char page_info_get_work_mode(void);
unsigned char page_info_get_line_delay1(void);
unsigned char page_info_get_line_delay2(void);
unsigned char page_info_get_core_div(void);
unsigned char page_info_get_bus_cycle(void);
unsigned char page_info_get_device_ecc_disable(void);
unsigned char page_info_get_host_ecc_disable(void);
unsigned int page_info_get_n2m_command(void);
unsigned int page_info_get_page_size(void);
unsigned char page_info_get_planes(void);
unsigned char page_info_get_plane_shift(void);
unsigned char page_info_get_cache_plane_shift(void);
unsigned char page_info_get_layout_method(void);
unsigned char page_info_get_cs_deselect_time(void);
unsigned char page_info_get_dummy_cycles(void);
unsigned int page_info_get_block_size(void);
unsigned int page_info_get_boot_size(void);
unsigned int page_info_get_pages_in_block(void);
unsigned char page_info_get_enable_bbt(void);
unsigned short *page_info_get_bbt(void);
unsigned int page_info_get_pages_in_boot(void);
void page_info_initialize(unsigned int default_n2m,
	unsigned char bus_width, unsigned char ca);
int page_info_pre_init(u8 *boot_info, int version);
#endif
