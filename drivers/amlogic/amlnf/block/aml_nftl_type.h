/*


*/

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/notifier.h>
#include <linux/time.h>
#include <linux/errno.h>
#include <linux/rbtree.h>
#include <linux/sched.h>


#define aml_nftl_dbg	printk

#define	FACTORY_BAD_BLOCK_ERROR		2
#define BYTES_PER_SECTOR		512
#define SHIFT_PER_SECTOR		9
#define BYTES_OF_USER_PER_PAGE		16
#define MIN_BYTES_OF_USER_PER_PAGE	16

/* #pragma pack(1) */

/* nand page */
struct _nand_page {
	ushort Page_NO;
	ushort blkNO_in_chip;
	/*
	unmapp: page_status=0xff
	valid mapping: page_status=1 discard: page_status=0
	*/
	unchar page_status;
};

/* nand discard page */
struct _nand_discard_page {
	uint timestamp;
	/*
	unmapp: page_status=0xff
	valid mapping: page_status=1 discard: page_status=0
	*/
	unchar page_status;
};

/* param for physical interface */
struct _physic_op_par {
	struct _nand_page phy_page;
	ushort page_bitmap;
	unchar *main_data_addr;
	unchar *spare_data_addr;
};

struct _nftl_cfg {
	ushort nftl_use_cache;
	ushort nftl_support_gc_read_reclaim;
	ushort nftl_support_wear_leveling;
	ushort nftl_need_erase;
	ushort nftl_part_reserved_block_ratio;
	ushort nftl_part_adjust_block_num;
	ushort nftl_min_free_block_num;
	ushort nftl_gc_threshold_free_block_num;
	ushort nftl_min_free_block;
	ushort nftl_gc_threshold_ratio_numerator;
	ushort nftl_gc_threshold_ratio_denominator;
	ushort nftl_max_cache_write_num;
};
/* #pragma pack() */
