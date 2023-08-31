// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Linux Headers */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/ctype.h>
#include <linux/poll.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/dma-contiguous.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/of_device.h>
/* media module used media/registers/cpu_version.h since kernel 5.4 */
#include <linux/amlogic/media/registers/cpu_version.h>
/* Local Headers */
#include "osd.h"
#include "osd_io.h"
#include "osd_reg.h"
#include "osd_rdma.h"
#include "osd_hw.h"
#include "osd_backup.h"
#include "osd_log.h"
#include <linux/amlogic/media/registers/register_map.h>
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
#include <linux/amlogic/media/rdma/rdma_mgr.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
#include <linux/amlogic/media/amvecm/ve.h>
#endif
#endif

#define RDMA_TABLE_INTERNAL_COUNT 512
#define RDMA_TEMP_TBL_SIZE        (8 * RDMA_TABLE_INTERNAL_COUNT)
#define VPP_NUM 3
static DEFINE_SPINLOCK(rdma_lock_vpp0);
static DEFINE_SPINLOCK(rdma_lock_vpp1);
static DEFINE_SPINLOCK(rdma_lock_vpp2);
static struct rdma_table_item *rdma_table[VPP_NUM];
static struct device *osd_rdma_dev;
static void *osd_rdma_table_virt[VPP_NUM];
static dma_addr_t osd_rdma_table_phy[VPP_NUM];
static ulong table_paddr[VPP_NUM];
static void *table_vaddr[VPP_NUM];
static u32 rdma_enable[VPP_NUM];
static u32 item_count[VPP_NUM];
static u32 rdma_debug;
static u32 rdma_hdr_delay;
static bool osd_rdma_init_flag;
#define OSD_RDMA_UPDATE_RETRY_COUNT 10
static unsigned int debug_rdma_status[VPP_NUM];
static unsigned int rdma_irq_count[VPP_NUM];
static unsigned int rdma_lost_count[VPP_NUM];
static unsigned int dump_reg_trigger;
static unsigned int rdma_recovery_count[VPP_NUM];
#ifdef OSD_RDMA_ISR
static unsigned int second_rdma_irq;
#endif
static unsigned int vsync_irq_count[VPP_NUM];

static bool osd_rdma_done[VPP_NUM];
static int osd_rdma_handle[VPP_NUM] = {-1, -1, -1};
static struct rdma_table_item *rdma_temp_tbl[VPP_NUM];
static int support_64bit_addr = 1;
static struct rdma_warn_array recovery_table[WARN_TABLE];
static struct rdma_warn_array recovery_not_hit_table[WARN_TABLE];
static uint num_reject = 2;
static int rdma_reject_cnt[2];
static int rdma_done_line[VPP_NUM];
module_param_array(rdma_reject_cnt, uint, &num_reject, 0664);
MODULE_PARM_DESC(rdma_reject_cnt, "\n rdma_reject_cnt\n");

void *memcpy(void *dest, const void *src, size_t len);

static inline void spin_lock_irqsave_vpp(u32 vpp_index, unsigned long *flags)
{
	switch (vpp_index) {
	case VPU_VPP0:
		spin_lock_irqsave(&rdma_lock_vpp0, *flags);
		break;
	case VPU_VPP1:
		spin_lock_irqsave(&rdma_lock_vpp1, *flags);
		break;
	case VPU_VPP2:
		spin_lock_irqsave(&rdma_lock_vpp2, *flags);
		break;
	}
}

static inline void spin_unlock_irqrestore_vpp(u32 vpp_index, unsigned long flags)
{
	switch (vpp_index) {
	case VPU_VPP0:
		spin_unlock_irqrestore(&rdma_lock_vpp0, flags);
		break;
	case VPU_VPP1:
		spin_unlock_irqrestore(&rdma_lock_vpp1, flags);
		break;
	case VPU_VPP2:
		spin_unlock_irqrestore(&rdma_lock_vpp2, flags);
		break;
	}
}

static void rdma_start_end_addr_update(u32 vpp_index, ulong table_paddr)
{
	u32 start_addr = 0, start_addr_msb = 0;
	u32 end_addr = 0, end_addr_msb = 0;

	switch (vpp_index) {
	case VPU_VPP0:
		start_addr = START_ADDR;
		start_addr_msb = START_ADDR_MSB;
		end_addr = END_ADDR;
		end_addr_msb = END_ADDR_MSB;
		break;
	case VPU_VPP1:
		start_addr = START_ADDR_VPP1;
		start_addr_msb = START_ADDR_MSB_VPP1;
		end_addr = END_ADDR_VPP1;
		end_addr_msb = END_ADDR_MSB_VPP1;
		break;
	case VPU_VPP2:
		start_addr = START_ADDR_VPP2;
		start_addr_msb = START_ADDR_MSB_VPP2;
		end_addr = END_ADDR_VPP2;
		end_addr_msb = END_ADDR_MSB_VPP2;
		break;
	}
	if (support_64bit_addr) {
	#ifdef CONFIG_ARM64
		osd_reg_write(start_addr,
			      table_paddr & 0xffffffff);
		osd_reg_write(start_addr_msb,
			      (table_paddr >> 32) & 0xffffffff);

		osd_reg_write(end_addr,
			      (table_paddr - 1) & 0xffffffff);
		osd_reg_write(end_addr_msb,
			      ((table_paddr - 1) >> 32) & 0xffffffff);
	#else
		osd_reg_write(start_addr,
			      table_paddr & 0xffffffff);
		osd_reg_write(start_addr_msb, 0);

		osd_reg_write(end_addr,
			      (table_paddr - 1) & 0xffffffff);
		osd_reg_write(end_addr_msb, 0);
	#endif
	} else {
		osd_reg_write(start_addr,
			      table_paddr & 0xffffffff);
		osd_reg_write(end_addr,
			      (table_paddr - 1) & 0xffffffff);
	}
}

static void rdma_end_addr_update(u32 vpp_index, ulong table_paddr, u32 count)
{
	u32 end_addr = 0, end_addr_msb = 0;

	switch (vpp_index) {
	case VPU_VPP0:
		end_addr = END_ADDR;
		end_addr_msb = END_ADDR_MSB;
		break;
	case VPU_VPP1:
		end_addr = END_ADDR_VPP1;
		end_addr_msb = END_ADDR_MSB_VPP1;
		break;
	case VPU_VPP2:
		end_addr = END_ADDR_VPP2;
		end_addr_msb = END_ADDR_MSB_VPP2;
		break;
	}
	if (support_64bit_addr) {
	#ifdef CONFIG_ARM64
		osd_reg_write(end_addr,
			      (table_paddr +
			      count * 8 - 1) & 0xffffffff);
		osd_reg_write(end_addr_msb,
			      ((table_paddr +
			      count * 8 - 1) >> 32) & 0xffffffff);
	#else
		osd_reg_write(end_addr,
			      (table_paddr +
			      count * 8 - 1) & 0xffffffff);
		osd_reg_write(end_addr_msb, 0);
	#endif
	} else {
		osd_reg_write(end_addr,
			      (table_paddr +
			      count * 8 - 1) & 0xffffffff);
	}
}

static ulong rdma_end_addr_get(u32 vpp_index)
{
	u32 end_addr = 0, end_addr_msb = 0;
	ulong rdma_end_addr = 0;

	switch (vpp_index) {
	case VPU_VPP0:
		end_addr = END_ADDR;
		end_addr_msb = END_ADDR_MSB;
		break;
	case VPU_VPP1:
		end_addr = END_ADDR_VPP1;
		end_addr_msb = END_ADDR_MSB_VPP1;
		break;
	case VPU_VPP2:
		end_addr = END_ADDR_VPP2;
		end_addr_msb = END_ADDR_MSB_VPP2;
		break;
	}

	if (support_64bit_addr) {
	#ifdef CONFIG_ARM64
		rdma_end_addr = osd_reg_read(end_addr_msb);
		rdma_end_addr = (rdma_end_addr & 0xffffffff) << 32;
		rdma_end_addr |= osd_reg_read(end_addr);
		rdma_end_addr++;
	#else
		rdma_end_addr = osd_reg_read(end_addr);
		rdma_end_addr++;
	#endif
	} else {
		rdma_end_addr = osd_reg_read(end_addr) + 1;
	}
	return rdma_end_addr;
}

static u32 rdma_current_table_addr_get(u32 vpp_index)
{
	u32 current_table_addr = 0;

	switch (vpp_index) {
	case VPU_VPP0:
		current_table_addr = OSD_RDMA_FLAG_REG;
		break;
	case VPU_VPP1:
		current_table_addr = OSD_RDMA_FLAG_REG_VPP1;
		break;
	case VPU_VPP2:
		current_table_addr = OSD_RDMA_FLAG_REG_VPP2;
		break;
	}
	return current_table_addr;
}

static int osd_rdma_init(void);
u32 osd_rdma_flag_reg[VPP_NUM];
u32 rdma_detect_reg;

void osd_rdma_flag_init(void)
{
	if (osd_dev_hw.display_type == S5_DISPLAY) {
		/* no OSD2 for S5 */
		osd_rdma_flag_reg[VPP0] = S5_VIU_OSD1_TCOLOR_AG3;
		osd_rdma_flag_reg[VPP1] = S5_VIU_OSD1_TCOLOR_AG2;
		osd_rdma_flag_reg[VPP2] = S5_VIU_OSD1_TCOLOR_AG1;
		rdma_detect_reg = S5_VIU_OSD1_TCOLOR_AG0;
	} else {
		osd_rdma_flag_reg[VPP0] = VIU_OSD2_TCOLOR_AG3;
		osd_rdma_flag_reg[VPP1] = VIU_OSD2_TCOLOR_AG2;
		osd_rdma_flag_reg[VPP2] = VIU_OSD2_TCOLOR_AG1;
		rdma_detect_reg = VIU_OSD2_TCOLOR_AG0;
	}
}

static u32 osd_rdma_status_is_reject(u32 vpp_index)
{
	u32 ret = 0;

	switch (vpp_index) {
	case 0:
		ret = OSD_RDMA_STATUS_IS_REJECT;
		break;
	case 1:
		ret = OSD_RDMA_VPP1_STATUS_IS_REJECT;
		break;
	case 2:
		ret = OSD_RDMA_VPP2_STATUS_IS_REJECT;
		break;
	}
	return ret;
}

static u32 osd_rdma_status_mark_tbl_done(u32 vpp_index)
{
	u32 ret = 0;

	switch (vpp_index) {
	case 0:
		ret = OSD_RDMA_STATUS_MARK_TBL_DONE;
		break;
	case 1:
		ret = OSD_RDMA_VPP1_STATUS_MARK_TBL_DONE;
		break;
	case 2:
		ret = OSD_RDMA_VPP2_STATUS_MARK_TBL_DONE;
		break;
	}
	return ret;
}

static void osd_rdma_status_clear_reject(u32 vpp_index)
{
	switch (vpp_index) {
	case 0:
		OSD_RDMA_STATUS_CLEAR_REJECT;
		break;
	case 1:
		OSD_RDMA_VPP1_STATUS_CLEAR_REJECT;
		break;
	case 2:
		OSD_RDMA_VPP2_STATUS_CLEAR_REJECT;
		break;
	}
}

#if defined(CONFIG_ARM64) && !defined(__clang__)
static inline void osd_rdma_mem_cpy(struct rdma_table_item *dst,
				    struct rdma_table_item *src, u32 len)
{
	asm volatile
		("	stp x5, x6, [sp, #-16]!\n"
		"	cmp %2,#8\n"
		"	bne 1f\n"
		"	ldr x5, [%0]\n"
		"	str x5, [%1]\n"
		"	b 2f\n"
		"1:     ldp x5, x6, [%0]\n"
		"	stp x5, x6, [%1]\n"
		"2:     nop\n"
		"	ldp x5, x6, [sp], #16\n"
		:
		: "r" (src), "r" (dst), "r" (len)
		: "x5", "x6");
}
#else
inline void osd_rdma_mem_cpy(struct rdma_table_item *dst,
			struct rdma_table_item *src, u32 len)
{
	memcpy(dst, src, len);
}
#endif

static int get_rdma_stat(struct rdma_warn_array *warn_array,
	u32 vpp_index)
{
	int i;

	for (i = 0; i < WARN_TABLE; i++) {
		if (warn_array[i].addr) {
			pr_info("table[%d]:addr=%x,count=%d, cur_line=%d, begin_line=%d\n",
				i,
				warn_array[i].addr,
				warn_array[i].count,
				warn_array[i].cur_line,
				warn_array[i].cur_begin_line);
		} else {
			break;
		}
	}
	return i;
}

int get_rdma_recovery_stat(u32 vpp_index)
{
	pr_info("%s:\n", __func__);
	return get_rdma_stat(recovery_table, vpp_index);
}

int get_rdma_not_hit_recovery_stat(u32 vpp_index)
{
	pr_info("%s:\n", __func__);
	return get_rdma_stat(recovery_not_hit_table, vpp_index);
}

static void update_warn_table(struct rdma_warn_array *warn_array,
	u32 addr, u32 vpp_index)
{
	int i;

	if ((addr == AMDV_CORE2A_SWAP_CTRL1 ||
	     addr == AMDV_CORE2A_SWAP_CTRL2 ||
	     addr == VPU_MAFBC_IRQ_CLEAR ||
	     addr == VPU_MAFBC1_IRQ_CLEAR ||
	     addr == VPU_MAFBC2_IRQ_CLEAR ||
	     addr == VPU_MAFBC_COMMAND ||
	     addr == VPU_MAFBC1_COMMAND ||
	     addr == VPU_MAFBC2_COMMAND))
		return;

	for (i = 0; i < WARN_TABLE; i++) {
		if (!warn_array[i].addr) {
			/* find empty table, update new addr */
			warn_array[i].addr = addr;
			warn_array[i].count++;
			warn_array[i].cur_line = get_encp_line(vpp_index);
			warn_array[i].cur_begin_line =
				get_cur_begin_line(vpp_index);
			break;
		} else if (warn_array[i].addr == addr) {
			/* same addr, update count */
			warn_array[i].count++;
			warn_array[i].cur_line = get_encp_line(vpp_index);
			warn_array[i].cur_begin_line =
				get_cur_begin_line(vpp_index);
			break;
		}
	}
}
static inline void reset_rdma_table(u32 vpp_index)
{
	struct rdma_table_item request_item;
	unsigned long flags = 0;
	u32 old_count;
	ulong end_addr;
	int i, j = 0, k = 0, trace_num = 0;
	struct rdma_table_item reset_item[VPP_NUM][2] = {
		{
		{
			.addr = OSD_RDMA_FLAG_REG,
			.val = OSD_RDMA_STATUS_MARK_TBL_RST,
		},
		{
			.addr = OSD_RDMA_FLAG_REG,
			.val = OSD_RDMA_STATUS_MARK_TBL_DONE,
		}
		},
		{
		{
			.addr = OSD_RDMA_FLAG_REG_VPP1,
			.val = OSD_RDMA_STATUS_MARK_TBL_RST,
		},
		{
			.addr = OSD_RDMA_FLAG_REG_VPP1,
			.val = OSD_RDMA_STATUS_MARK_TBL_DONE,
		}
		},
		{
		{
			.addr = OSD_RDMA_FLAG_REG_VPP2,
			.val = OSD_RDMA_STATUS_MARK_TBL_RST,
		},
		{
			.addr = OSD_RDMA_FLAG_REG_VPP2,
			.val = OSD_RDMA_STATUS_MARK_TBL_DONE,
		}
		},
	};

	if (osd_hw.rdma_trace_enable)
		trace_num = osd_hw.rdma_trace_num;
	else
		trace_num = 0;
	spin_lock_irqsave_vpp(vpp_index, &flags);
	if (!osd_rdma_status_is_reject(vpp_index)) {
		u32 val, mask;
		int iret;

		if ((item_count[vpp_index] * (sizeof(struct rdma_table_item))) >
			RDMA_TEMP_TBL_SIZE) {
			pr_info("more memory: allocate(%x), expect(%zu)\n",
				(unsigned int)RDMA_TEMP_TBL_SIZE,
				sizeof(struct rdma_table_item) *
				item_count[vpp_index]);
			WARN_ON(1);
		}
		memset(rdma_temp_tbl[vpp_index], 0,
		       (sizeof(struct rdma_table_item) * item_count[vpp_index]));
		end_addr = rdma_end_addr_get(vpp_index);
		if (end_addr > table_paddr[vpp_index])
			old_count = (end_addr - table_paddr[vpp_index]) >> 3;
		else
			old_count = 0;
		rdma_end_addr_update(vpp_index, table_paddr[vpp_index], 0);
		for (i = (int)(item_count[vpp_index] - 1);
			i >= 0; i--) {
			if (!rdma_temp_tbl[vpp_index])
				break;
			if (rdma_table[vpp_index][i].addr ==
				rdma_current_table_addr_get(vpp_index))
				continue;
			if (rdma_table[vpp_index][i].addr ==
				VPP_MISC)
				continue;
			iret = get_recovery_item(rdma_table[vpp_index][i].addr,
						 &val, &mask);
			if (!iret) {
				request_item.addr =
					rdma_table[vpp_index][i].addr;
				request_item.val = val;
				osd_rdma_mem_cpy(&rdma_temp_tbl[vpp_index][j],
						 &request_item, 8);
				j++;

				for (k = 0; k < trace_num; k++) {
					if (osd_hw.rdma_trace_reg[k] & 0x10000)
						pr_info("recovery -- 0x%04x:0x%08x, mask:0x%08x, org_val:0x%x, old_count=%d, item_count=%d, j=%d\n",
							rdma_table[vpp_index][i].addr,
							val, mask,
							osd_reg_read(rdma_table[vpp_index][i].addr),
							old_count,
							item_count[vpp_index],
							j);
				}
				update_warn_table(recovery_table,
					rdma_table[vpp_index][i].addr,
					vpp_index);

				rdma_recovery_count[vpp_index]++;
			} else if (iret < 0 && i >= old_count) {
				request_item.addr =
					rdma_table[vpp_index][i].addr;
				request_item.val =
					rdma_table[vpp_index][i].val;
				osd_rdma_mem_cpy(&rdma_temp_tbl[vpp_index][j],
						&request_item, 8);
				j++;
				for (k = 0; k < trace_num; k++) {
					if (osd_hw.rdma_trace_reg[k] & 0x10000) {
						pr_info("recovery -- 0x%04x:0x%08x, mask:0x%08x\n",
							rdma_table[vpp_index][i].addr,
							rdma_table[vpp_index][i].val,
							mask);
						pr_info("recovery -- i:%d,item_count:%d,old_count:%d\n",
							i,
							item_count[vpp_index],
							old_count);
					}
				}
				rdma_recovery_count[vpp_index]++;
			} else if (iret < 0) {
				/* record not recovery reg */
				update_warn_table(recovery_not_hit_table,
					rdma_table[vpp_index][i].addr,
					vpp_index);
			}
		}
		for (i = 0; i < j; i++) {
			osd_rdma_mem_cpy
				(&rdma_table[vpp_index][1 + i],
				&rdma_temp_tbl[vpp_index][j - i - 1], 8);
			update_recovery_item
				(rdma_temp_tbl[vpp_index][j - i - 1].addr,
				rdma_temp_tbl[vpp_index][j - i - 1].val);
		}
		item_count[vpp_index] = j + 2;
		osd_rdma_mem_cpy(rdma_table[vpp_index], &reset_item[vpp_index][0], 8);
		osd_rdma_mem_cpy(&rdma_table[vpp_index][item_count[vpp_index] - 1],
				 &reset_item[vpp_index][1], 8);
		rdma_end_addr_update(vpp_index, table_paddr[vpp_index],
				     item_count[vpp_index]);
	}
	spin_unlock_irqrestore_vpp(vpp_index, flags);
}

static int update_table_item(u32 vpp_index, u32 addr, u32 val, u8 irq_mode)
{
	unsigned long flags = 0;
	int retry_count = OSD_RDMA_UPDATE_RETRY_COUNT;
	struct rdma_table_item request_item;
	int reject1 = 0, reject2 = 0, ret = 0;
	ulong paddr;
	static int pace_logging[VPP_NUM];

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (item_count[vpp_index] > 500 || rdma_reset_tigger_flag) {
//#else
//	if (item_count[vpp_index] > 500) {
#endif
		int i;
		struct rdma_table_item reset_item[VPP_NUM][2] = {
			{
			{
				.addr = OSD_RDMA_FLAG_REG,
				.val = OSD_RDMA_STATUS_MARK_TBL_RST,
			},
			{
				.addr = OSD_RDMA_FLAG_REG,
				.val = OSD_RDMA_STATUS_MARK_TBL_DONE,
			}
			},
			{
			{
				.addr = OSD_RDMA_FLAG_REG_VPP1,
				.val = OSD_RDMA_VPP1_STATUS_MARK_TBL_RST,
			},
			{
				.addr = OSD_RDMA_FLAG_REG_VPP1,
				.val = OSD_RDMA_VPP1_STATUS_MARK_TBL_DONE,
			}
			},
			{
			{
				.addr = OSD_RDMA_FLAG_REG_VPP2,
				.val = OSD_RDMA_VPP2_STATUS_MARK_TBL_RST,
			},
			{
				.addr = OSD_RDMA_FLAG_REG_VPP2,
				.val = OSD_RDMA_VPP2_STATUS_MARK_TBL_DONE,
			}
			},
		};

		/* rdma table is full */
		if (!(pace_logging[vpp_index]++ % 50))
			pr_info("%s overflow!vsync_cnt=%d, rdma_cnt=%d\n",
				__func__, vsync_irq_count[vpp_index], rdma_irq_count[vpp_index]);
		/* update rdma table */
		for (i = 1; i < item_count[vpp_index] - 1; i++)
			osd_reg_write(rdma_table[vpp_index][i].addr, rdma_table[vpp_index][i].val);

		osd_reg_write(addr, val);
		update_recovery_item(addr, val);

		item_count[vpp_index] = 2;
		osd_rdma_mem_cpy(rdma_table[vpp_index], &reset_item[vpp_index][0], 8);
		osd_rdma_mem_cpy(&rdma_table[vpp_index][item_count[vpp_index] - 1],
				 &reset_item[vpp_index][1], 8);
		rdma_end_addr_update(vpp_index, table_paddr[vpp_index],
				    item_count[vpp_index]);
		return -1;
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	}
#endif

	/* pr_debug("%02dth, ctrl: 0x%x, status: 0x%x, auto:0x%x, flag:0x%x\n",
	 *	item_count, osd_reg_read(RDMA_CTRL),
	 *	osd_reg_read(RDMA_STATUS),
	 *	osd_reg_read(RDMA_ACCESS_AUTO),
	 *	osd_reg_read(OSD_RDMA_FLAG_REG));
	 */
retry:
	if (0 == (retry_count--)) {
		pr_debug("OSD RDMA stuck: 0x%x = 0x%x, status: 0x%x\n",
			 addr, val, osd_reg_read(RDMA_STATUS));
		pr_debug("::retry count: %d-%d, count: %d, flag: 0x%x\n",
			 reject1, reject2, item_count[vpp_index],
			 osd_reg_read(osd_rdma_flag_reg[vpp_index]));
		spin_lock_irqsave_vpp(vpp_index, &flags);
		request_item.addr = osd_rdma_flag_reg[vpp_index];
		request_item.val = osd_rdma_status_mark_tbl_done(vpp_index);
		osd_rdma_mem_cpy
			(&rdma_table[vpp_index][item_count[vpp_index]],
			&request_item, 8);
		request_item.addr = addr;
		request_item.val = val;
		update_backup_reg(addr, val);
		update_recovery_item(addr, val);
		osd_rdma_mem_cpy
			(&rdma_table[vpp_index][item_count[vpp_index] - 1],
			&request_item, 8);
		item_count[vpp_index]++;
		spin_unlock_irqrestore_vpp(vpp_index, flags);
		return -1;
	}

	if (osd_rdma_status_is_reject(vpp_index) && (irq_mode)) {
		/* should not be here. Using the wrong write function
		 * or rdma isr is block
		 */
		pr_info("update reg but rdma running, mode: %d\n",
			irq_mode);
		return -2;
	}

	if (osd_rdma_status_is_reject(vpp_index) && !irq_mode) {
		/* should not be here. Using the wrong write function
		 * or rdma isr is block
		 */
		reject1++;
		rdma_reject_cnt[0] = reject1;
		pr_debug("update reg but rdma running, mode: %d,",
			 irq_mode);
		pr_debug("retry count:%d (%d), flag: 0x%x, status: 0x%x\n",
			 retry_count, reject1,
			 osd_reg_read(osd_rdma_flag_reg[vpp_index]),
			 osd_reg_read(RDMA_STATUS));
		goto retry;
	}

	/*atom_lock_start:*/
	spin_lock_irqsave_vpp(vpp_index, &flags);
	request_item.addr = osd_rdma_flag_reg[vpp_index];
	request_item.val = osd_rdma_status_mark_tbl_done(vpp_index);
	osd_rdma_mem_cpy(&rdma_table[vpp_index][item_count[vpp_index]], &request_item, 8);
	request_item.addr = addr;
	request_item.val = val;
	update_backup_reg(addr, val);
	update_recovery_item(addr, val);
	osd_rdma_mem_cpy(&rdma_table[vpp_index][item_count[vpp_index] - 1], &request_item, 8);
	item_count[vpp_index]++;
	paddr = table_paddr[vpp_index] + item_count[vpp_index] * 8;
	if (!osd_rdma_status_is_reject(vpp_index)) {
		rdma_end_addr_update(vpp_index, paddr, 0);
	} else if (!irq_mode) {
		reject2++;
		rdma_reject_cnt[1] = reject1;
		pr_debug("need update ---, but rdma running,");
		pr_debug("retry count:%d (%d), flag: 0x%x, status: 0x%x\n",
			 retry_count, reject2,
			 osd_reg_read(osd_rdma_flag_reg[vpp_index]),
			 osd_reg_read(RDMA_STATUS));
		item_count[vpp_index]--;
		spin_unlock_irqrestore_vpp(vpp_index, flags);
		goto retry;
	} else {
		ret = -3;
	}
	/*atom_lock_end:*/
	spin_unlock_irqrestore_vpp(vpp_index, flags);
	return ret;
}

static inline u32 is_rdma_reg(u32 addr)
{
	u32 rdma_en = 1;

	if (addr >= VIU2_OSD1_CTRL_STAT && addr <= VIU2_OSD1_BLK3_CFG_W4)
		rdma_en = 0;
	else
		rdma_en = 1;
	return rdma_en;
}

static inline u32 read_reg_internal(u32 vpp_index, u32 addr)
{
	int  i;
	u32 val = 0;
	u32 rdma_en = 0;

	if (!is_rdma_reg(addr))
		rdma_en = 0;
	else
		rdma_en = rdma_enable[vpp_index];

	if (rdma_en) {
		for (i = (int)(item_count[vpp_index] - 1);
			i >= 0; i--) {
			if (addr == rdma_table[vpp_index][i].addr) {
				val = rdma_table[vpp_index][i].val;
				break;
			}
		}
		if (i >= 0)
			return val;
	}
	return osd_reg_read(addr);
}

static inline int wrtie_reg_internal(u32 vpp_index, u32 addr, u32 val)
{
	struct rdma_table_item request_item;
	u32 rdma_en = 0;

	if (!is_rdma_reg(addr)) {
		/* need write at vsync, update table here */
		viu2_osd_reg_set(addr, val);
		return 0;
	}
	rdma_en = rdma_enable[vpp_index];

	if (!rdma_en) {
		osd_reg_write(addr, val);
		return 0;
	}

	if (item_count[vpp_index] > 500) {
		/* rdma table is full */
		pr_info("%s overflow!\n", __func__);
		return -1;
	}
	/* TODO remove the Done write operation to save the time */
	request_item.addr = osd_rdma_flag_reg[vpp_index];
	request_item.val = osd_rdma_status_mark_tbl_done(vpp_index);
	/* afbc start before afbc reset will cause afbc decode error */
	if (addr == VIU_SW_RESET) {
		int i = 0;

		for (i = 0; i < item_count[vpp_index]; i++) {
			if (rdma_table[vpp_index][i].addr == VPU_MAFBC_COMMAND) {
				rdma_table[vpp_index][i].addr = VIU_OSD1_TEST_RDDATA;
				rdma_table[vpp_index][i].val = 0x0;
			}
		}
	}
	osd_rdma_mem_cpy
		(&rdma_table[vpp_index][item_count[vpp_index]],
		&request_item, 8);
	request_item.addr = addr;
	request_item.val = val;
	update_backup_reg(addr, val);
	update_recovery_item(addr, val);
	osd_rdma_mem_cpy
		(&rdma_table[vpp_index][item_count[vpp_index] - 1],
		&request_item, 8);
	item_count[vpp_index]++;
	return 0;
}

static u32 _VSYNCOSD_RD_MPEG_REG(u32 vpp_index, u32 addr)
{
	int  i;
	bool find = false;
	u32 val = 0;
	unsigned long flags = 0;
	u32 rdma_en = 0;

	if (!is_rdma_reg(addr)) {
		/* need write at vsync, update table here */
		val = viu2_osd_reg_read(addr);
		return val;
	}
	rdma_en = rdma_enable[vpp_index];

	if (rdma_en) {
		spin_lock_irqsave_vpp(vpp_index, &flags);
		/* 1st, read from rdma table */
		for (i = (int)(item_count[vpp_index] - 1);
			i >= 0; i--) {
			if (addr == rdma_table[vpp_index][i].addr) {
				val = rdma_table[vpp_index][i].val;
				break;
			}
		}
		if (i >= 0)
			find = true;
		else if (get_backup_reg(addr, &val) == 0)
			find = true;
		 /* 2nd, read from backup reg */
		spin_unlock_irqrestore_vpp(vpp_index, flags);
		if (find)
			return val;
	}
	/* 3rd, read from osd reg */
	return osd_reg_read(addr);
}

u32 VSYNCOSD_RD_MPEG_REG(u32 addr)
{
	return _VSYNCOSD_RD_MPEG_REG(VPP0, addr);
}
EXPORT_SYMBOL(VSYNCOSD_RD_MPEG_REG);

u32 VSYNCOSD_RD_MPEG_REG_VPP1(u32 addr)
{
	return _VSYNCOSD_RD_MPEG_REG(VPP1, addr);
}

u32 VSYNCOSD_RD_MPEG_REG_VPP2(u32 addr)
{
	return _VSYNCOSD_RD_MPEG_REG(VPP2, addr);
}

static int _VSYNCOSD_WR_MPEG_REG(u32 vpp_index, u32 addr, u32 val)
{
	int ret = 0, k = 0;
	u32 rdma_en = 0, trace_num = 0;

	if (!is_rdma_reg(addr)) {
		/* need write at vsync, update table here */
		viu2_osd_reg_set(addr, val);
		return ret;
	}
	rdma_en = rdma_enable[vpp_index];

	if (rdma_en)
		ret = update_table_item(vpp_index, addr, val, 0);
	else
		osd_reg_write(addr, val);
	if (osd_hw.rdma_trace_enable)
		trace_num = osd_hw.rdma_trace_num;
	else
		trace_num = 0;
	for (k = 0; k < trace_num; k++) {
		if (addr == (osd_hw.rdma_trace_reg[k] & 0xffff))
			pr_info("(%s), %04x=0x%08x, rdma_en=%d, ret=%d\n",
				__func__,
				addr, val,
				rdma_en, ret);
	}
	return ret;
}

int VSYNCOSD_WR_MPEG_REG(u32 addr, u32 val)
{
	return _VSYNCOSD_WR_MPEG_REG(VPP0, addr, val);
}
EXPORT_SYMBOL(VSYNCOSD_WR_MPEG_REG);

int VSYNCOSD_WR_MPEG_REG_VPP1(u32 addr, u32 val)
{
	return _VSYNCOSD_WR_MPEG_REG(VPP1, addr, val);
}

int VSYNCOSD_WR_MPEG_REG_VPP2(u32 addr, u32 val)
{
	return _VSYNCOSD_WR_MPEG_REG(VPP2, addr, val);
}

static int _VSYNCOSD_WR_MPEG_REG_BITS(u32 vpp_index, u32 addr, u32 val, u32 start, u32 len)
{
	u32 read_val = 0;
	u32 write_val = 0;
	int ret = 0, k = 0;
	u32 rdma_en = 0, trace_num = 0;

	if (!is_rdma_reg(addr)) {
		/* need write at vsync, update table here */
		viu2_osd_reg_set_bits(addr, val, start, len);
		return ret;
	}
	rdma_en = rdma_enable[vpp_index];

	if (rdma_en) {
		read_val = _VSYNCOSD_RD_MPEG_REG(vpp_index, addr);
		write_val = (read_val & ~(((1L << (len)) - 1) << (start)))
			    | ((unsigned int)(val) << (start));
		ret = update_table_item(vpp_index, addr, write_val, 0);
	} else {
		osd_reg_set_bits(addr, val, start, len);
	}
	if (osd_hw.rdma_trace_enable)
		trace_num = osd_hw.rdma_trace_num;
	else
		trace_num = 0;
	for (k = 0; k < trace_num; k++) {
		if (addr == (osd_hw.rdma_trace_reg[k] & 0xffff))
			pr_info("(%s), vpp:%d addr:%04x val:0x%08x start:%d len:%d, rdma_en=%d, ret=%d write_val:0x%x\n",
				__func__, vpp_index,
				addr, val, start, len,
				rdma_en, ret, write_val);
	}
	return ret;
}

int VSYNCOSD_WR_MPEG_REG_BITS(u32 addr, u32 val, u32 start, u32 len)
{
	return _VSYNCOSD_WR_MPEG_REG_BITS(VPP0, addr, val, start, len);
}
EXPORT_SYMBOL(VSYNCOSD_WR_MPEG_REG_BITS);

int VSYNCOSD_WR_MPEG_REG_BITS_VPP1(u32 addr, u32 val, u32 start, u32 len)
{
	return _VSYNCOSD_WR_MPEG_REG_BITS(VPP1, addr, val, start, len);
}

int VSYNCOSD_WR_MPEG_REG_BITS_VPP2(u32 addr, u32 val, u32 start, u32 len)
{
	return _VSYNCOSD_WR_MPEG_REG_BITS(VPP2, addr, val, start, len);
}

static int _VSYNCOSD_SET_MPEG_REG_MASK(u32 vpp_index, u32 addr, u32 _mask)
{
	u32 read_val = 0;
	u32 write_val = 0;
	int ret = 0, k = 0;
	u32 rdma_en = 0, trace_num = 0;

	if (!is_rdma_reg(addr)) {
		/* need write at vsync, update table here */
		viu2_osd_reg_set_mask(addr, _mask);
		return ret;
	}
	rdma_en = rdma_enable[vpp_index];

	if (rdma_en) {
		read_val = _VSYNCOSD_RD_MPEG_REG(vpp_index, addr);
		write_val = read_val | _mask;
		ret = update_table_item(vpp_index, addr, write_val, 0);
	} else {
		osd_reg_set_mask(addr, _mask);
	}
	if (osd_hw.rdma_trace_enable)
		trace_num = osd_hw.rdma_trace_num;
	else
		trace_num = 0;
	for (k = 0; k < trace_num; k++) {
		if (addr == (osd_hw.rdma_trace_reg[k] & 0xffff))
			pr_info("(%s) %04x=0x%08x->0x%08x, mask=0x%08x, rdma_en=%d, ret=%d\n",
				__func__,
				addr, read_val, write_val,
				_mask, rdma_en, ret);
	}
	return ret;
}

int VSYNCOSD_SET_MPEG_REG_MASK(u32 addr, u32 _mask)
{
	return _VSYNCOSD_SET_MPEG_REG_MASK(VPP0, addr, _mask);
}
EXPORT_SYMBOL(VSYNCOSD_SET_MPEG_REG_MASK);

int VSYNCOSD_SET_MPEG_REG_MASK_VPP1(u32 addr, u32 _mask)
{
	return _VSYNCOSD_SET_MPEG_REG_MASK(VPP1, addr, _mask);
}

int VSYNCOSD_SET_MPEG_REG_MASK_VPP2(u32 addr, u32 _mask)
{
	return _VSYNCOSD_SET_MPEG_REG_MASK(VPP2, addr, _mask);
}

static int _VSYNCOSD_CLR_MPEG_REG_MASK(u32 vpp_index, u32 addr, u32 _mask)
{
	u32 read_val = 0;
	u32 write_val = 0;
	int ret = 0, k = 0;
	u32 rdma_en = 0, trace_num = 0;

	if (!is_rdma_reg(addr)) {
		/* need write at vsync, update table here */
		viu2_osd_reg_clr_mask(addr, _mask);
		return ret;
	}
	rdma_en = rdma_enable[vpp_index];

	if (rdma_en) {
		read_val = _VSYNCOSD_RD_MPEG_REG(vpp_index, addr);
		write_val = read_val & (~_mask);
		ret = update_table_item(vpp_index, addr, write_val, 0);
	} else {
		osd_reg_clr_mask(addr, _mask);
	}
	if (osd_hw.rdma_trace_enable)
		trace_num = osd_hw.rdma_trace_num;
	else
		trace_num = 0;
	for (k = 0; k < trace_num; k++) {
		if (addr == (osd_hw.rdma_trace_reg[k] & 0xffff))
			pr_info("(%s) %04x=0x%08x->0x%08x, mask=0x%08x, rdma_en=%d, ret=%d\n",
				__func__,
				addr, read_val, write_val,
				_mask, rdma_en, ret);
	}
	return ret;
}

int VSYNCOSD_CLR_MPEG_REG_MASK(u32 addr, u32 _mask)
{
	return _VSYNCOSD_CLR_MPEG_REG_MASK(VPP0, addr, _mask);
}
EXPORT_SYMBOL(VSYNCOSD_CLR_MPEG_REG_MASK);

int VSYNCOSD_CLR_MPEG_REG_MASK_VPP1(u32 addr, u32 _mask)
{
	return _VSYNCOSD_CLR_MPEG_REG_MASK(VPP1, addr, _mask);
}

int VSYNCOSD_CLR_MPEG_REG_MASK_VPP2(u32 addr, u32 _mask)
{
	return _VSYNCOSD_CLR_MPEG_REG_MASK(VPP2, addr, _mask);
}

int _VSYNCOSD_IRQ_WR_MPEG_REG(u32 vpp_index, u32 addr, u32 val)
{
	int ret = 0, k = 0;
	u32 rdma_en = 0, trace_num = 0;

	if (!is_rdma_reg(addr)) {
		/* need write at vsync, update table here */
		viu2_osd_reg_set(addr, val);
		return ret;
	}
	rdma_en = rdma_enable[vpp_index];

	if (rdma_en)
		ret = update_table_item(vpp_index, addr, val, 1);
	else
		osd_reg_write(addr, val);
	if (osd_hw.rdma_trace_enable)
		trace_num = osd_hw.rdma_trace_num;
	else
		trace_num = 0;
	for (k = 0; k < trace_num; k++) {
		if (addr == (osd_hw.rdma_trace_reg[k] & 0xffff))
			pr_info("(%s), %04x=0x%08x, rdma_en=%d, ret=%d\n",
				__func__,
				addr, val,
				rdma_en, ret);
	}
	return ret;
}

int VSYNCOSD_IRQ_WR_MPEG_REG(u32 addr, u32 val)
{
	return _VSYNCOSD_IRQ_WR_MPEG_REG(VPP0, addr, val);
}
EXPORT_SYMBOL(VSYNCOSD_IRQ_WR_MPEG_REG);

int VSYNCOSD_IRQ_WR_MPEG_REG_VPP1(u32 addr, u32 val)
{
	return _VSYNCOSD_IRQ_WR_MPEG_REG(VPP1, addr, val);
}

int VSYNCOSD_IRQ_WR_MPEG_REG_VPP2(u32 addr, u32 val)
{
	return _VSYNCOSD_IRQ_WR_MPEG_REG(VPP2, addr, val);
}

/* number lines before vsync for reset */
static unsigned int reset_line;
module_param(reset_line, uint, 0664);
MODULE_PARM_DESC(reset_line, "reset_line");

static unsigned int disable_osd_rdma_reset;
module_param(disable_osd_rdma_reset, uint, 0664);
MODULE_PARM_DESC(disable_osd_rdma_reset, "disable_osd_rdma_reset");

int get_rdma_irq_done_line(u32 vpp_index)
{
	return rdma_done_line[VIU1];
}

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
static int osd_reset_rdma_handle = -1;

void set_reset_rdma_trigger_line(void)
{
	int trigger_line;

	switch (aml_read_vcbus(VPU_VIU_VENC_MUX_CTRL) & 0x3) {
	case 0:
		trigger_line = aml_read_vcbus(ENCL_VIDEO_VAVON_ELINE)
			- aml_read_vcbus(ENCL_VIDEO_VSO_BLINE) - reset_line;
		break;
	case 1:
		if ((aml_read_vcbus(ENCI_VIDEO_MODE) & 1) == 0)
			trigger_line = 260; /* 480i */
		else
			trigger_line = 310; /* 576i */
		break;
	case 2:
		if (aml_read_vcbus(ENCP_VIDEO_MODE) & (1 << 12))
			trigger_line = aml_read_vcbus(ENCP_DE_V_END_EVEN);
		else
			trigger_line = aml_read_vcbus(ENCP_VIDEO_VAVON_ELINE)
				- aml_read_vcbus(ENCP_VIDEO_VSO_BLINE)
				- reset_line;
		break;
	case 3:
		trigger_line = aml_read_vcbus(ENCT_VIDEO_VAVON_ELINE)
			- aml_read_vcbus(ENCT_VIDEO_VSO_BLINE) - reset_line;
		break;
	}
	aml_write_vcbus(VPP_INT_LINE_NUM, trigger_line);
}

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
struct hdr_osd_reg_s hdr_osd_shadow_reg = {
	0x00000001, /* VIU_OSD1_MATRIX_CTRL 0x1a90 */
	0x00ba0273, /* VIU_OSD1_MATRIX_COEF00_01 0x1a91 */
	0x003f1f9a, /* VIU_OSD1_MATRIX_COEF02_10 0x1a92 */
	0x1ea801c0, /* VIU_OSD1_MATRIX_COEF11_12 0x1a93 */
	0x01c01e6a, /* VIU_OSD1_MATRIX_COEF20_21 0x1a94 */
	0x00000000, /* VIU_OSD1_MATRIX_COLMOD_COEF42 0x1a95 */
	0x00400200, /* VIU_OSD1_MATRIX_OFFSET0_1 0x1a96 */
	0x00000200, /* VIU_OSD1_MATRIX_PRE_OFFSET2 0x1a97 */
	0x00000000, /* VIU_OSD1_MATRIX_PRE_OFFSET0_1 0x1a98 */
	0x00000000, /* VIU_OSD1_MATRIX_PRE_OFFSET2 0x1a99 */
	0x1fd80000, /* VIU_OSD1_MATRIX_COEF22_30 0x1a9d */
	0x00000000, /* VIU_OSD1_MATRIX_COEF31_32 0x1a9e */
	0x00000000, /* VIU_OSD1_MATRIX_COEF40_41 0x1a9f */
	0x00000000, /* VIU_OSD1_EOTF_CTL 0x1ad4 */
	0x08000000, /* VIU_OSD1_EOTF_COEF00_01 0x1ad5 */
	0x00000000, /* VIU_OSD1_EOTF_COEF02_10 0x1ad6 */
	0x08000000, /* VIU_OSD1_EOTF_COEF11_12 0x1ad7 */
	0x00000000, /* VIU_OSD1_EOTF_COEF20_21 0x1ad8 */
	0x08000001, /* VIU_OSD1_EOTF_COEF22_RS 0x1ad9 */
	0x0,		/* VIU_OSD1_EOTF_3X3_OFST_0 0x1aa0 */
	0x0,		/* VIU_OSD1_EOTF_3X3_OFST_1 0x1aa1 */
	0x01c00000, /* VIU_OSD1_OETF_CTL 0x1adc */
	{
		/* eotf table */
		{ /* r map */
			0x0000, 0x0200, 0x0400, 0x0600, 0x0800, 0x0a00,
			0x0c00, 0x0e00, 0x1000, 0x1200, 0x1400, 0x1600,
			0x1800, 0x1a00, 0x1c00, 0x1e00, 0x2000, 0x2200,
			0x2400, 0x2600, 0x2800, 0x2a00, 0x2c00, 0x2e00,
			0x3000, 0x3200, 0x3400, 0x3600, 0x3800, 0x3a00,
			0x3c00, 0x3e00, 0x4000
		},
		{ /* g map */
			0x0000, 0x0200, 0x0400, 0x0600, 0x0800, 0x0a00,
			0x0c00, 0x0e00, 0x1000, 0x1200, 0x1400, 0x1600,
			0x1800, 0x1a00, 0x1c00, 0x1e00, 0x2000, 0x2200,
			0x2400, 0x2600, 0x2800, 0x2a00, 0x2c00, 0x2e00,
			0x3000, 0x3200, 0x3400, 0x3600, 0x3800, 0x3a00,
			0x3c00, 0x3e00, 0x4000
		},
		{ /* b map */
			0x0000, 0x0200, 0x0400, 0x0600, 0x0800, 0x0a00,
			0x0c00, 0x0e00, 0x1000, 0x1200, 0x1400, 0x1600,
			0x1800, 0x1a00, 0x1c00, 0x1e00, 0x2000, 0x2200,
			0x2400, 0x2600, 0x2800, 0x2a00, 0x2c00, 0x2e00,
			0x3000, 0x3200, 0x3400, 0x3600, 0x3800, 0x3a00,
			0x3c00, 0x3e00, 0x4000
		},
		/* oetf table */
		{ /* or map */
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000
		},
		{ /* og map */
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000
		},
		{ /* ob map */
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000
		}
	},
	-1
};

struct hdr_osd_reg_s hdr_osd_display_reg = {
	0x00000001, /* VIU_OSD1_MATRIX_CTRL 0x1a90 */
	0x00ba0273, /* VIU_OSD1_MATRIX_COEF00_01 0x1a91 */
	0x003f1f9a, /* VIU_OSD1_MATRIX_COEF02_10 0x1a92 */
	0x1ea801c0, /* VIU_OSD1_MATRIX_COEF11_12 0x1a93 */
	0x01c01e6a, /* VIU_OSD1_MATRIX_COEF20_21 0x1a94 */
	0x00000000, /* VIU_OSD1_MATRIX_COLMOD_COEF42 0x1a95 */
	0x00400200, /* VIU_OSD1_MATRIX_OFFSET0_1 0x1a96 */
	0x00000200, /* VIU_OSD1_MATRIX_PRE_OFFSET2 0x1a97 */
	0x00000000, /* VIU_OSD1_MATRIX_PRE_OFFSET0_1 0x1a98 */
	0x00000000, /* VIU_OSD1_MATRIX_PRE_OFFSET2 0x1a99 */
	0x1fd80000, /* VIU_OSD1_MATRIX_COEF22_30 0x1a9d */
	0x00000000, /* VIU_OSD1_MATRIX_COEF31_32 0x1a9e */
	0x00000000, /* VIU_OSD1_MATRIX_COEF40_41 0x1a9f */
	0x00000000, /* VIU_OSD1_EOTF_CTL 0x1ad4 */
	0x08000000, /* VIU_OSD1_EOTF_COEF00_01 0x1ad5 */
	0x00000000, /* VIU_OSD1_EOTF_COEF02_10 0x1ad6 */
	0x08000000, /* VIU_OSD1_EOTF_COEF11_12 0x1ad7 */
	0x00000000, /* VIU_OSD1_EOTF_COEF20_21 0x1ad8 */
	0x08000001, /* VIU_OSD1_EOTF_COEF22_RS 0x1ad9 */
	0x0,		/* VIU_OSD1_EOTF_3X3_OFST_0 0x1aa0 */
	0x0,		/* VIU_OSD1_EOTF_3X3_OFST_1 0x1aa1 */
	0x01c00000, /* VIU_OSD1_OETF_CTL 0x1adc */
	{
		/* eotf table */
		{ /* r map */
			0x0000, 0x0200, 0x0400, 0x0600, 0x0800, 0x0a00,
			0x0c00, 0x0e00, 0x1000, 0x1200, 0x1400, 0x1600,
			0x1800, 0x1a00, 0x1c00, 0x1e00, 0x2000, 0x2200,
			0x2400, 0x2600, 0x2800, 0x2a00, 0x2c00, 0x2e00,
			0x3000, 0x3200, 0x3400, 0x3600, 0x3800, 0x3a00,
			0x3c00, 0x3e00, 0x4000
		},
		{ /* g map */
			0x0000, 0x0200, 0x0400, 0x0600, 0x0800, 0x0a00,
			0x0c00, 0x0e00, 0x1000, 0x1200, 0x1400, 0x1600,
			0x1800, 0x1a00, 0x1c00, 0x1e00, 0x2000, 0x2200,
			0x2400, 0x2600, 0x2800, 0x2a00, 0x2c00, 0x2e00,
			0x3000, 0x3200, 0x3400, 0x3600, 0x3800, 0x3a00,
			0x3c00, 0x3e00, 0x4000
		},
		{ /* b map */
			0x0000, 0x0200, 0x0400, 0x0600, 0x0800, 0x0a00,
			0x0c00, 0x0e00, 0x1000, 0x1200, 0x1400, 0x1600,
			0x1800, 0x1a00, 0x1c00, 0x1e00, 0x2000, 0x2200,
			0x2400, 0x2600, 0x2800, 0x2a00, 0x2c00, 0x2e00,
			0x3000, 0x3200, 0x3400, 0x3600, 0x3800, 0x3a00,
			0x3c00, 0x3e00, 0x4000
		},
		/* oetf table */
		{ /* or map */
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000
		},
		{ /* og map */
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000
		},
		{ /* ob map */
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000
		}
	},
	-1
};

static void hdr_restore_osd_csc(void)
{
	u32 i = 0;
	u32 addr_port;
	u32 data_port;
	struct hdr_osd_lut_s *lut = &hdr_osd_shadow_reg.lut_val;

	if (osd_reset_rdma_handle == -1 ||
	    disable_osd_rdma_reset)
		return;
	/* check osd matrix enable status */
	if (hdr_osd_shadow_reg.viu_osd1_matrix_ctrl & 0x00000001) {
		/* osd matrix, VPP_MATRIX_0 */
		rdma_write_reg
			(osd_reset_rdma_handle,
			 VIU_OSD1_MATRIX_PRE_OFFSET0_1,
			 hdr_osd_shadow_reg.viu_osd1_matrix_pre_offset0_1);
		rdma_write_reg
			(osd_reset_rdma_handle,
			VIU_OSD1_MATRIX_PRE_OFFSET2,
			hdr_osd_shadow_reg.viu_osd1_matrix_pre_offset2);
		rdma_write_reg
			(osd_reset_rdma_handle,
			VIU_OSD1_MATRIX_COEF00_01,
			hdr_osd_shadow_reg.viu_osd1_matrix_coef00_01);
		rdma_write_reg
			(osd_reset_rdma_handle,
			VIU_OSD1_MATRIX_COEF02_10,
			hdr_osd_shadow_reg.viu_osd1_matrix_coef02_10);
		rdma_write_reg
			(osd_reset_rdma_handle,
			VIU_OSD1_MATRIX_COEF11_12,
			hdr_osd_shadow_reg.viu_osd1_matrix_coef11_12);
		rdma_write_reg
			(osd_reset_rdma_handle,
			VIU_OSD1_MATRIX_COEF20_21,
			hdr_osd_shadow_reg.viu_osd1_matrix_coef20_21);
		rdma_write_reg
			(osd_reset_rdma_handle,
			VIU_OSD1_MATRIX_COEF22_30,
			hdr_osd_shadow_reg.viu_osd1_matrix_coef22_30);
		rdma_write_reg
			(osd_reset_rdma_handle,
			VIU_OSD1_MATRIX_COEF31_32,
			hdr_osd_shadow_reg.viu_osd1_matrix_coef31_32);
		rdma_write_reg
			(osd_reset_rdma_handle,
			VIU_OSD1_MATRIX_COEF40_41,
			hdr_osd_shadow_reg.viu_osd1_matrix_coef40_41);
		rdma_write_reg
			(osd_reset_rdma_handle,
			VIU_OSD1_MATRIX_COLMOD_COEF42,
			hdr_osd_shadow_reg.viu_osd1_matrix_colmod_coef42);
		rdma_write_reg
			(osd_reset_rdma_handle,
			VIU_OSD1_MATRIX_OFFSET0_1,
			hdr_osd_shadow_reg.viu_osd1_matrix_offset0_1);
		rdma_write_reg
			(osd_reset_rdma_handle,
			VIU_OSD1_MATRIX_OFFSET2,
			hdr_osd_shadow_reg.viu_osd1_matrix_offset2);
		rdma_write_reg
			(osd_reset_rdma_handle,
			VIU_OSD1_MATRIX_CTRL,
			hdr_osd_shadow_reg.viu_osd1_matrix_ctrl);
	}
	/* restore eotf lut */
	if ((hdr_osd_shadow_reg.viu_osd1_eotf_ctl & 0x80000000) != 0) {
		addr_port = VIU_OSD1_EOTF_LUT_ADDR_PORT;
		data_port = VIU_OSD1_EOTF_LUT_DATA_PORT;
		rdma_write_reg
			(osd_reset_rdma_handle,
			addr_port, 0);
		for (i = 0; i < 16; i++)
			rdma_write_reg
				(osd_reset_rdma_handle,
				data_port,
				lut->r_map[i * 2]
				| (lut->r_map[i * 2 + 1] << 16));
		rdma_write_reg
			(osd_reset_rdma_handle,
			data_port,
			lut->r_map[EOTF_LUT_SIZE - 1]
			| (lut->g_map[0] << 16));
		for (i = 0; i < 16; i++)
			rdma_write_reg
				(osd_reset_rdma_handle,
				data_port,
				lut->g_map[i * 2 + 1]
				| (lut->b_map[i * 2 + 2] << 16));
		for (i = 0; i < 16; i++)
			rdma_write_reg
				(osd_reset_rdma_handle,
				data_port,
				lut->b_map[i * 2]
				| (lut->b_map[i * 2 + 1] << 16));
		rdma_write_reg
			(osd_reset_rdma_handle,
			data_port, lut->b_map[EOTF_LUT_SIZE - 1]);

		/* load eotf matrix */
		rdma_write_reg
			(osd_reset_rdma_handle,
			VIU_OSD1_EOTF_COEF00_01,
			hdr_osd_shadow_reg.viu_osd1_eotf_coef00_01);
		rdma_write_reg
			(osd_reset_rdma_handle,
			VIU_OSD1_EOTF_COEF02_10,
			hdr_osd_shadow_reg.viu_osd1_eotf_coef02_10);
		rdma_write_reg
			(osd_reset_rdma_handle,
			VIU_OSD1_EOTF_COEF11_12,
			hdr_osd_shadow_reg.viu_osd1_eotf_coef11_12);
		rdma_write_reg
			(osd_reset_rdma_handle,
			VIU_OSD1_EOTF_COEF20_21,
			hdr_osd_shadow_reg.viu_osd1_eotf_coef20_21);
		rdma_write_reg
			(osd_reset_rdma_handle,
			VIU_OSD1_EOTF_COEF22_RS,
			hdr_osd_shadow_reg.viu_osd1_eotf_coef22_rs);
		rdma_write_reg
			(osd_reset_rdma_handle,
			VIU_OSD1_EOTF_CTL,
			hdr_osd_shadow_reg.viu_osd1_eotf_ctl);
	}
	/* restore oetf lut */
	if ((hdr_osd_shadow_reg.viu_osd1_oetf_ctl & 0xe0000000) != 0) {
		addr_port = VIU_OSD1_OETF_LUT_ADDR_PORT;
		data_port = VIU_OSD1_OETF_LUT_DATA_PORT;
		for (i = 0; i < 20; i++) {
			rdma_write_reg
				(osd_reset_rdma_handle,
				addr_port, i);
			rdma_write_reg
				(osd_reset_rdma_handle,
				data_port,
				lut->or_map[i * 2]
				| (lut->or_map[i * 2 + 1] << 16));
		}
		rdma_write_reg
			(osd_reset_rdma_handle,
			 addr_port, 20);
		rdma_write_reg
			(osd_reset_rdma_handle,
			data_port,
			lut->or_map[41 - 1]
			| (lut->og_map[0] << 16));
		for (i = 0; i < 20; i++) {
			rdma_write_reg
				(osd_reset_rdma_handle,
				addr_port, 21 + i);
			rdma_write_reg
				(osd_reset_rdma_handle,
				data_port,
				lut->og_map[i * 2 + 1]
				| (lut->og_map[i * 2 + 2] << 16));
		}
		for (i = 0; i < 20; i++) {
			rdma_write_reg
				(osd_reset_rdma_handle,
				 addr_port, 41 + i);
			rdma_write_reg
				(osd_reset_rdma_handle,
				 data_port,
				 lut->ob_map[i * 2] |
				 (lut->ob_map[i * 2 + 1] << 16));
		}
		rdma_write_reg
			(osd_reset_rdma_handle,
			addr_port, 61);
		rdma_write_reg
			(osd_reset_rdma_handle,
			data_port,
			lut->ob_map[41 - 1]);
		rdma_write_reg
			(osd_reset_rdma_handle,
			VIU_OSD1_OETF_CTL,
			hdr_osd_shadow_reg.viu_osd1_oetf_ctl);
	}
}
#endif

static void osd_reset_rdma_func(u32 reset_bit)
{
	if (disable_osd_rdma_reset == 0 && reset_bit) {
		rdma_write_reg(osd_reset_rdma_handle,
			       VIU_SW_RESET, 1);
		rdma_write_reg(osd_reset_rdma_handle,
			       VIU_SW_RESET, 0);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
		if (osd_hw.osd_meson_dev.osd_ver <= OSD_NORMAL) {
			if (rdma_hdr_delay == 0 ||
			    hdr_osd_reg.shadow_mode == 0)
				memcpy(&hdr_osd_shadow_reg, &hdr_osd_reg,
				       sizeof(struct hdr_osd_reg_s));
			hdr_restore_osd_csc();
		}
		/* Todo: what about g12a */
#endif
		set_reset_rdma_trigger_line();
		rdma_config(osd_reset_rdma_handle, 1 << 6);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
		if (hdr_osd_reg.shadow_mode == -1) {
			if (rdma_hdr_delay == 1) {
				memcpy(&hdr_osd_shadow_reg,
				       &hdr_osd_reg,
				       sizeof(struct hdr_osd_reg_s));
			} else if (rdma_hdr_delay == 2) {
				memcpy(&hdr_osd_shadow_reg,
				       &hdr_osd_display_reg,
				       sizeof(struct hdr_osd_reg_s));
				memcpy(&hdr_osd_display_reg,
				       &hdr_osd_reg,
				       sizeof(struct hdr_osd_reg_s));
			}
		} else {
			if (hdr_osd_reg.shadow_mode == 1) {
				memcpy(&hdr_osd_shadow_reg,
				       &hdr_osd_reg,
				       sizeof(struct hdr_osd_reg_s));
			} else if (hdr_osd_reg.shadow_mode == 2) {
				memcpy(&hdr_osd_shadow_reg,
				       &hdr_osd_display_reg,
				       sizeof(struct hdr_osd_reg_s));
				memcpy(&hdr_osd_display_reg,
				       &hdr_osd_reg,
				       sizeof(struct hdr_osd_reg_s));
			}
		}
#endif
	} else {
		rdma_clear(osd_reset_rdma_handle);
	}
}

static void osd_rdma_irq(void *arg)
{
	u32 rdma_status;

	if (osd_rdma_handle[0] == -1)
		return;
	rdma_done_line[VIU1] = get_encp_line(VIU1);

	rdma_status = osd_reg_read(RDMA_STATUS);
	debug_rdma_status[VIU1] = rdma_status;
	OSD_RDMA_STATUS_CLEAR_REJECT;
	osd_update_vsync_hit();
	reset_rdma_table(VIU1);
	osd_update_scan_mode();
	osd_update_3d_mode();
	osd_hw_reset(VIU1);
	rdma_irq_count[VIU1]++;
	osd_rdma_done[VIU1] = true;
	{
		/*This is a memory barrier*/
		wmb();
	}
}

static void osd_rdma_vpp1_irq(void *arg)
{
	u32 rdma_status;

	if (osd_rdma_handle[1] == -1)
		return;

	rdma_status = osd_reg_read(RDMA_STATUS);
	debug_rdma_status[VIU2] = rdma_status;
	OSD_RDMA_VPP1_STATUS_CLEAR_REJECT;
	osd_update_vsync_hit_viu2();
	reset_rdma_table(VIU2);
	//osd_update_scan_mode();
	osd_hw_reset(VIU2);
	rdma_irq_count[VIU2]++;
	osd_rdma_done[VIU2] = true;
	{
		/*This is a memory barrier*/
		wmb();
	}
}

static void osd_rdma_vpp2_irq(void *arg)
{
	u32 rdma_status;

	if (osd_rdma_handle[2] == -1)
		return;
	rdma_status = osd_reg_read(RDMA_STATUS);
	debug_rdma_status[VIU3] = rdma_status;
	OSD_RDMA_VPP2_STATUS_CLEAR_REJECT;
	osd_update_vsync_hit_viu3();
	reset_rdma_table(VIU3);
	//osd_update_scan_mode();
	osd_hw_reset(VIU3);
	rdma_irq_count[VIU3]++;
	osd_rdma_done[VIU3] = true;
	{
		/*This is a memory barrier*/
		wmb();
	}
}

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static void osd_reset_rdma_irq(void *arg)
{
}

static struct rdma_op_s osd_reset_rdma_op = {
	osd_reset_rdma_irq,
	NULL
};
#endif

static struct rdma_op_s osd_rdma_op = {
	osd_rdma_irq,
	NULL
};

static struct rdma_op_s osd_rdma_vpp1_op = {
	osd_rdma_vpp1_irq,
	NULL
};

static struct rdma_op_s osd_rdma_vpp2_op = {
	osd_rdma_vpp2_irq,
	NULL
};
#endif

static int start_osd_rdma(char channel, u32 vpp_index)
{
#ifndef CONFIG_AMLOGIC_MEDIA_RDMA
	char intr_bit = 8 * channel;
	char rw_bit = 4 + channel;
	char inc_bit = channel;
	u32 data32;

	data32  = 0;
	data32 |= 1 << 7; /* [31: 6] Rsrv. */
	data32 |= 1 << 6; /* [31: 6] Rsrv. */
	data32 |= 3 << 4;
	/* [ 5: 4] ctrl_ahb_wr_burst_size. 0=16; 1=24; 2=32; 3=48. */
	data32 |= 3 << 2;
	/* [ 3: 2] ctrl_ahb_rd_burst_size. 0=16; 1=24; 2=32; 3=48. */
	data32 |= 0 << 1;
	/* [    1] ctrl_sw_reset.*/
	data32 |= 0 << 0;
	/* [    0] ctrl_free_clk_enable.*/
	osd_reg_write(RDMA_CTRL, data32);

	data32  = osd_reg_read(RDMA_ACCESS_AUTO);
	/*
	 * [23: 16] interrupt inputs enable mask for auto-start
	 * 1: vsync int bit 0
	 */
	data32 |= 0x1 << intr_bit;
	/* [    6] ctrl_cbus_write_1. 1=Register write; 0=Register read. */
	data32 |= 1 << rw_bit;
	/*
	 * [    2] ctrl_cbus_addr_incr_1.
	 * 1=Incremental register access; 0=Non-incremental.
	 */
	data32 &= ~(1 << inc_bit);
	osd_reg_write(RDMA_ACCESS_AUTO, data32);
#else
	switch (vpp_index) {
	case VPU_VPP0:
		rdma_config(channel,
			    RDMA_TRIGGER_VSYNC_INPUT |
			    RDMA_AUTO_START_MASK);
		break;
	case VPU_VPP1:
		rdma_config(channel,
			    RDMA_TRIGGER_VPP1_VSYNC_INPUT |
			    RDMA_AUTO_START_MASK);
		break;
	case VPU_VPP2:
		rdma_config(channel,
			    RDMA_TRIGGER_VPP2_VSYNC_INPUT |
			    RDMA_AUTO_START_MASK);
	break;
	}
	osd_hw.line_n_rdma = 0;
#endif
	return 1;
}

static int stop_rdma(char channel)
{
#ifndef CONFIG_AMLOGIC_MEDIA_RDMA
	char intr_bit = 8 * channel;
	u32 data32 = 0x0;

	data32  = osd_reg_read(RDMA_ACCESS_AUTO);
	data32 &= ~(0x1 <<
		    intr_bit);
	/* [23: 16] interrupt inputs enable mask
	 * for auto-start 1: vsync int bit 0
	 */
	osd_reg_write(RDMA_ACCESS_AUTO, data32);
#else
	rdma_clear(channel);
	if (osd_reset_rdma_handle != -1)
		rdma_clear(osd_reset_rdma_handle);
#endif
	return 0;
}

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
static void osd_rdma_config(u32 vpp_index)
{
	switch (vpp_index) {
	case 0:
		rdma_config(osd_rdma_handle[vpp_index],
			    RDMA_TRIGGER_VSYNC_INPUT |
			    RDMA_AUTO_START_MASK);
		break;
	case 1:
		rdma_config(osd_rdma_handle[vpp_index],
			    RDMA_TRIGGER_VPP1_VSYNC_INPUT |
			    RDMA_AUTO_START_MASK);
		break;
	case 2:
		rdma_config(osd_rdma_handle[vpp_index],
			    RDMA_TRIGGER_VPP2_VSYNC_INPUT |
			    RDMA_AUTO_START_MASK);
		break;
	}
}
#endif

void enable_line_n_rdma(void)
{
	unsigned long flags = 0;

	osd_log_info("%s\n", __func__);
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	rdma_clear(OSD_RDMA_CHANNEL_INDEX);
#endif
	spin_lock_irqsave_vpp(0, &flags);
	OSD_RDMA_STATUS_CLEAR_REJECT;
	if (support_64bit_addr) {
		#ifdef CONFIG_ARM64
		osd_reg_write(START_ADDR,
			      table_paddr[0] & 0xffffffff);
		osd_reg_write(START_ADDR_MSB,
			      (table_paddr[0] >> 32) & 0xffffffff);

		osd_reg_write(END_ADDR,
			      (table_paddr[0] - 1) & 0xffffffff);
		osd_reg_write(END_ADDR_MSB,
			      ((table_paddr[0] - 1) >> 32) & 0xffffffff);
		#else
		osd_reg_write(START_ADDR,
			      table_paddr[0] & 0xffffffff);
		osd_reg_write(START_ADDR_MSB, 0);
		osd_reg_write(END_ADDR,
			      (table_paddr[0] - 1) & 0xffffffff);
		osd_reg_write(END_ADDR_MSB, 0);
		#endif
	} else {
		osd_reg_write(START_ADDR,
			      table_paddr[0] & 0xffffffff);
		osd_reg_write(END_ADDR,
			      (table_paddr[0] - 1) & 0xffffffff);
	}
	item_count[0] = 0;
	spin_unlock_irqrestore_vpp(0, flags);
	reset_rdma_table(0);
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	rdma_config(OSD_RDMA_CHANNEL_INDEX,
		    RDMA_TRIGGER_LINE_INPUT |
		    RDMA_AUTO_START_MASK);
#endif
}

void enable_vsync_rdma(u32 vpp_index)
{
	unsigned long flags = 0;

	osd_log_info("%s\n", __func__);
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	rdma_clear(osd_rdma_handle[vpp_index]);
#endif
	spin_lock_irqsave_vpp(vpp_index, &flags);
	OSD_RDMA_STATUS_CLEAR_REJECT;
	if (support_64bit_addr) {
		#ifdef CONFIG_ARM64
		osd_reg_write(START_ADDR,
			      table_paddr[vpp_index] & 0xffffffff);
		osd_reg_write(START_ADDR_MSB,
			      (table_paddr[vpp_index] >> 32) & 0xffffffff);

		osd_reg_write(END_ADDR,
			      (table_paddr[vpp_index] - 1) & 0xffffffff);
		osd_reg_write(END_ADDR_MSB,
			      ((table_paddr[vpp_index] - 1) >> 32) & 0xffffffff);
		#else
		osd_reg_write(START_ADDR,
			      table_paddr[vpp_index] & 0xffffffff);
		osd_reg_write(START_ADDR_MSB, 0);
		osd_reg_write(END_ADDR,
			      (table_paddr[vpp_index] - 1) & 0xffffffff);
		osd_reg_write(END_ADDR_MSB, 0);
		#endif
	} else {
		osd_reg_write(START_ADDR,
			      table_paddr[vpp_index] & 0xffffffff);
		osd_reg_write(END_ADDR,
			      (table_paddr[vpp_index] - 1) & 0xffffffff);
	}
	item_count[vpp_index] = 0;
	spin_unlock_irqrestore_vpp(vpp_index, flags);
	reset_rdma_table(vpp_index);
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	osd_rdma_config(vpp_index);
#endif
}

void osd_rdma_interrupt_done_clear(u32 vpp_index)
{
	vsync_irq_count[vpp_index]++;

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (osd_rdma_done[vpp_index])
		rdma_watchdog_setting(0);
	else
		rdma_watchdog_setting(1);
#endif
	osd_rdma_done[vpp_index] = false;

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (rdma_reset_tigger_flag) {
		u32 rdma_status;

		rdma_status =
			osd_reg_read(RDMA_STATUS);
		pr_info("osd rdma restart! 0x%x\n",
			rdma_status);
		osd_rdma_enable(vpp_index, 0);
		osd_rdma_enable(vpp_index, 2);
		rdma_reset_tigger_flag = 0;
	}
#endif
}

int read_rdma_table(u32 vpp_index)
{
	int rdma_count = 0;
	int i, reg;

	if (rdma_debug) {
		for (rdma_count = 0; rdma_count < item_count[vpp_index] + 1; rdma_count++)
			pr_info("rdma_table addr is 0x%x, value is 0x%x\n",
				rdma_table[vpp_index][rdma_count].addr,
				rdma_table[vpp_index][rdma_count].val);
		reg = 0x1100;
		pr_info("RDMA relative registers-----------------\n");
		for (i = 0 ; i < 24 ; i++)
			pr_info("[0x%x]== 0x%x\n",
				reg + i, osd_reg_read(reg + i));
	}
	return 0;
}
EXPORT_SYMBOL(read_rdma_table);

int osd_rdma_enable(u32 vpp_index, u32 enable)
{
	int ret = 0;
	unsigned long flags = 0;

	if ((enable && rdma_enable[vpp_index]) ||
	    (!enable && !rdma_enable[vpp_index]))
		return 0;

	ret = osd_rdma_init();
	if (ret != 0)
		return -1;
	rdma_enable[vpp_index] = enable;
	if (enable) {
		spin_lock_irqsave_vpp(vpp_index, &flags);
		osd_rdma_status_clear_reject(vpp_index);
		rdma_start_end_addr_update(vpp_index, table_paddr[vpp_index]);
		item_count[vpp_index] = 0;
		spin_unlock_irqrestore_vpp(vpp_index, flags);
		reset_rdma_table(vpp_index);
		start_osd_rdma(osd_rdma_handle[vpp_index], vpp_index);
	} else {
		stop_rdma(osd_rdma_handle[vpp_index]);
	}

	return 1;
}
EXPORT_SYMBOL(osd_rdma_enable);

int osd_rdma_reset_and_flush(u32 output_index, u32 reset_bit)
{
	int i, ret = 0;
	unsigned long flags = 0;
	u32 reset_reg_mask;
	u32 base;
	u32 addr;
	u32 value;

	spin_lock_irqsave_vpp(output_index, &flags);
	reset_reg_mask = reset_bit;
	reset_reg_mask &= ~HW_RESET_OSD1_REGS;
	if (disable_osd_rdma_reset != 0) {
		reset_reg_mask = 0;
		reset_bit = 0;
	}

	if (reset_reg_mask) {
		wrtie_reg_internal(output_index, VIU_SW_RESET,
				   reset_reg_mask);
		wrtie_reg_internal(output_index, VIU_SW_RESET, 0);
	}

	/* same bit, but gxm only reset hardware, not top reg*/
	if (osd_hw.osd_meson_dev.cpu_id >= __MESON_CPU_MAJOR_ID_GXM)
		reset_bit &= ~HW_RESET_AFBCD_REGS;

	i = 0;
	base = hw_osd_reg_array[OSD1].osd_ctrl_stat;
	while ((reset_bit & HW_RESET_OSD1_REGS) &&
	       (i < OSD_REG_BACKUP_COUNT)) {
		addr = osd_reg_backup[i];
		wrtie_reg_internal(output_index, addr, osd_backup[addr - base]);
		i++;
	}
	i = 0;
	base = OSD1_AFBCD_ENABLE;
	while ((reset_bit & HW_RESET_AFBCD_REGS) &&
	       (i < OSD_AFBC_REG_BACKUP_COUNT)) {
		addr = osd_afbc_reg_backup[i];
		value = osd_afbc_backup[addr - base];
		if (addr == OSD1_AFBCD_ENABLE)
			value |=  0x100;
		wrtie_reg_internal(output_index, addr, value);
		i++;
	}

	if (osd_hw.osd_meson_dev.afbc_type == MALI_AFBC &&
	    !osd_dev_hw.multi_afbc_core &&
	    reset_bit & HW_RESET_MALI_AFBCD_REGS) {
		/* restore mali afbcd regs */
		if (osd_hw.afbc_regs_backup) {
			int i;
			u32 addr;
			u32 value;
			u32 base = VPU_MAFBC_IRQ_MASK;

			for (i = 0; i < MALI_AFBC_REG_BACKUP_COUNT;
				i++) {
				addr = mali_afbc_reg_backup[i];
				value = mali_afbc_backup[addr - base];
				wrtie_reg_internal(output_index, addr, value);
			}
		}
	}
	if (osd_hw.osd_meson_dev.afbc_type == MALI_AFBC &&
	    osd_dev_hw.multi_afbc_core) {
		u32 afbc_reset;

		reset_bit &= ~HW_RESET_MALI_AFBCD_ARB;
		afbc_reset = HW_RESET_MALI_AFBCD_REGS;
		afbc_reset &= ~HW_RESET_MALI_AFBCD_ARB;
		if (reset_bit & afbc_reset) {
			/* restore mali afbcd regs */
			if (osd_hw.afbc_regs_backup) {
				int i;
				u32 addr;
				u32 value;
				u32 base = VPU_MAFBC_IRQ_MASK;

				for (i = 0; i < MALI_AFBC_REG_T7_BACKUP_COUNT;
					i++) {
					addr = mali_afbc_reg_t7_backup[i];
					value = mali_afbc_t7_backup[addr - base];
					wrtie_reg_internal(output_index, addr, value);
				}
			}
		}
		afbc_reset = HW_RESET_MALI_AFBCD1_REGS;
		afbc_reset &= ~HW_RESET_MALI_AFBCD_ARB;
		if (reset_bit & afbc_reset) {
			/* restore mali afbcd regs */
			if (osd_hw.afbc_regs_backup) {
				int i;
				u32 addr;
				u32 value;
				u32 base = VPU_MAFBC1_IRQ_MASK;

				for (i = 0; i < MALI_AFBC1_REG_T7_BACKUP_COUNT;
					i++) {
					addr = mali_afbc1_reg_t7_backup[i];
					value = mali_afbc1_t7_backup[addr - base];
					wrtie_reg_internal(output_index, addr, value);
				}
			}
		}
		afbc_reset = HW_RESET_MALI_AFBCD2_REGS;
		afbc_reset &= ~HW_RESET_MALI_AFBCD_ARB;
		if (reset_bit & afbc_reset) {
			/* restore mali afbcd regs */
			if (osd_hw.afbc_regs_backup) {
				int i;
				u32 addr;
				u32 value;
				u32 base = VPU_MAFBC2_IRQ_MASK;

				for (i = 0; i < MALI_AFBC2_REG_T7_BACKUP_COUNT;
					i++) {
					addr = mali_afbc2_reg_t7_backup[i];
					value = mali_afbc2_t7_backup[addr - base];
					wrtie_reg_internal(output_index, addr, value);
				}
			}
		}
	}

	if (osd_hw.osd_meson_dev.afbc_type == MALI_AFBC &&
	    osd_hw.osd_meson_dev.osd_ver == OSD_HIGH_ONE &&
	    !osd_dev_hw.multi_afbc_core)
		wrtie_reg_internal(output_index, VPU_MAFBC_COMMAND, 1);

	if (osd_hw.osd_meson_dev.afbc_type == MALI_AFBC &&
	    osd_hw.osd_meson_dev.osd_ver == OSD_HIGH_ONE &&
	    osd_dev_hw.multi_afbc_core) {
		struct hw_osd_reg_s *osd_reg;
		u32 osd_count = osd_hw.osd_meson_dev.osd_count;
		int afbc0_started = 0;

		for (i = 0; i < osd_count; i++) {
			u32 hw_index;

			if (get_output_device_id(i) != output_index ||
			    !osd_hw.osd_afbcd[i].enable)
				continue;

			hw_index = to_osd_hw_index(i);

			/* for osd_dev_hw.multi_afbc_core,
			 * OSD1+OSD2 uses afbc, OSD3 uses afbc1, OSD4 uses afbc2.
			 */
			if (hw_index == OSD2 && afbc0_started)
				continue;

			osd_reg = &hw_osd_reg_array[i];

			wrtie_reg_internal(output_index,
					   osd_reg->vpu_mafbc_command, 1);
			osd_log_dbg2(MODULE_BASE,
				     "%s, AFBC osd%d start command\n",
				     __func__, i);
			if (hw_index == OSD1)
				afbc0_started = 1;
		}
	}

	if (item_count[output_index] < 500) {
		rdma_end_addr_update(output_index,
				     table_paddr[output_index],
				     item_count[output_index]);
	} else {
		pr_info("%s item overflow %d\n",
			__func__, item_count[output_index]);
		ret = -1;
	}
	if (dump_reg_trigger > 0) {
		for (i = 0; i < item_count[output_index]; i++)
			pr_info("dump rdma reg[%d]:0x%x, data:0x%x\n",
				i, rdma_table[output_index][i].addr,
				rdma_table[output_index][i].val);
		dump_reg_trigger--;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (osd_reset_rdma_handle != -1)
		osd_reset_rdma_func(reset_bit &
		HW_RESET_OSD1_REGS);
#endif

	spin_unlock_irqrestore_vpp(output_index, flags);
	return ret;
}
EXPORT_SYMBOL(osd_rdma_reset_and_flush);

static void osd_rdma_release(struct device *dev)
{
	int i, vpp_num;

	vpp_num = osd_hw.vpp_num;
	for (i = 0; i < vpp_num; i++)
		dma_free_coherent(osd_rdma_dev,
				  PAGE_SIZE,
				  osd_rdma_table_virt[i],
				  (dma_addr_t)&osd_rdma_table_phy[i]);
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (osd_reset_rdma_handle != -1) {
		rdma_unregister(osd_reset_rdma_handle);
		osd_reset_rdma_handle = -1;
	}

	for (i = 0; i < vpp_num; i++) {
		if (osd_rdma_handle[i] != -1) {
			rdma_unregister(osd_rdma_handle[i]);
			osd_rdma_handle[i] = -1;
		}
	}
#endif
}

#ifdef OSD_RDMA_ISR
static irqreturn_t osd_rdma_isr(int irq, void *dev_id)
{
	u32 rdma_status;

	rdma_status = osd_reg_read(RDMA_STATUS);
	debug_rdma_status = rdma_status;
	if (rdma_status & (1 << (24 + OSD_RDMA_CHANNEL_INDEX))) {
		OSD_RDMA_STATUS_CLEAR_REJECT;
		reset_rdma_table();
		osd_update_scan_mode();
		osd_update_3d_mode();
		osd_mali_afbc_start();
		osd_update_vsync_hit();
		osd_hw_reset(VIU1);
		rdma_irq_count++;
		{
			/*This is a memory barrier*/
			wmb();
		}
		osd_reg_write(RDMA_CTRL,
			      1 << (24 + OSD_RDMA_CHANNEL_INDEX));
	} else {
		rdma_lost_count++;
	}
	rdma_status = osd_reg_read(RDMA_STATUS);
	if (rdma_status & 0xf7000000) {
		if (!second_rdma_irq)
			pr_info("osd rdma irq as first function call, status: 0x%x\n",
				rdma_status);
		pr_info("osd rdma miss done isr, status: 0x%x\n", rdma_status);
		osd_reg_write(RDMA_CTRL, rdma_status & 0xf7000000);
	}
	return IRQ_HANDLED;
}
#endif

static int osd_rdma_init(void)
{
	int ret = -1, i;
	u32 vpp_num = osd_hw.vpp_num;

	if (osd_rdma_init_flag)
		return 0;
	osd_rdma_dev = kzalloc(sizeof(*osd_rdma_dev), GFP_KERNEL);
	if (!osd_rdma_dev) {
		/* osd_log_err("osd rdma init error!\n"); */
		return -1;
	}
	for (i = 0; i < vpp_num; i++) {
		rdma_temp_tbl[i] = kmalloc(RDMA_TEMP_TBL_SIZE, GFP_KERNEL);
		if (!rdma_temp_tbl[i]) {
			/* osd_log_err("osd rdma alloc temp_tbl error!\n"); */
			goto error2;
		}
	}
	osd_rdma_dev->release = osd_rdma_release;
	dev_set_name(osd_rdma_dev, "osd-rdma-dev");
	dev_set_drvdata(osd_rdma_dev, osd_rdma_dev);
	ret = device_register(osd_rdma_dev);
	if (ret) {
		osd_log_err("register rdma dev error\n");
		goto error1;
	}
#ifdef OSD_RDMA_ISR
	second_rdma_irq = 0;
#endif
	dump_reg_trigger = 0;

	osd_rdma_dev->coherent_dma_mask = DMA_BIT_MASK(32);
	osd_rdma_dev->dma_mask = &osd_rdma_dev->coherent_dma_mask;

	of_dma_configure(osd_rdma_dev, osd_rdma_dev->of_node, true);
	for (i = 0; i < vpp_num; i++) {
		osd_rdma_table_virt[i] =
			dma_alloc_coherent(osd_rdma_dev, PAGE_SIZE,
					   &osd_rdma_table_phy[i], GFP_KERNEL);

		if (!osd_rdma_table_virt[i]) {
			osd_log_err("osd rdma dma alloc failed!\n");
			goto error2;
		}

		table_vaddr[i] = osd_rdma_table_virt[i];
		table_paddr[i] = osd_rdma_table_phy[i];
		rdma_table[i] = (struct rdma_table_item *)table_vaddr[i];
		if (!rdma_table[i]) {
			osd_log_err("%s: failed to remap rmda_table_addr\n", __func__);
			goto error2;
		}
	}
#ifdef OSD_RDMA_ISR
	if (rdma_mgr_irq_request) {
		second_rdma_irq = 1;
		pr_info("osd rdma request irq as second interrupt function!\n");
	}
	if (request_irq(INT_RDMA, &osd_rdma_isr,
			IRQF_SHARED, "osd_rdma", (void *)"osd_rdma")) {
		osd_log_err("can't request irq for rdma\n");
		goto error2;
	}
#endif
	osd_rdma_init_flag = true;
	osd_reg_write(OSD_RDMA_FLAG_REG, 0x0);
	osd_reg_write(OSD_RDMA_FLAG_REG_VPP1, 0x0);
	osd_reg_write(OSD_RDMA_FLAG_REG_VPP2, 0x0);

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	if (osd_hw.osd_meson_dev.cpu_id >= __MESON_CPU_MAJOR_ID_GXL &&
	    osd_hw.osd_meson_dev.cpu_id <= __MESON_CPU_MAJOR_ID_TXL) {
		osd_reset_rdma_op.arg = osd_rdma_dev;
		osd_reset_rdma_handle = rdma_register(&osd_reset_rdma_op,
						      NULL, PAGE_SIZE);
		pr_info("%s:osd reset rdma handle = %d.\n", __func__,
			osd_reset_rdma_handle);
	}
#endif
	osd_rdma_op.arg = osd_rdma_dev;
	osd_rdma_handle[0] = rdma_register(&osd_rdma_op,
					NULL, PAGE_SIZE);
	pr_info("%s:osd rdma handle[0] = %d.\n", __func__,
		osd_rdma_handle[0]);
	if (osd_hw.osd_meson_dev.has_vpp1 &&
	   osd_hw.display_dev_cnt == 2) {
		/* vpp1 used then register rdma channel */
		osd_rdma_handle[1] = rdma_register(&osd_rdma_vpp1_op,
						NULL, PAGE_SIZE);
		pr_info("%s:osd rdma handle[1] = %d.\n", __func__,
			osd_rdma_handle[1]);
	}
	if (osd_hw.osd_meson_dev.has_vpp2 &&
	   osd_hw.display_dev_cnt == 3) {
		/* vpp2 used then register rdma channel */
		osd_rdma_handle[2] = rdma_register(&osd_rdma_vpp2_op,
						NULL, PAGE_SIZE);
		pr_info("%s:osd rdma handle[2] = %d.\n", __func__,
			osd_rdma_handle[2]);
	}

#else
	osd_rdma_handle[0] = 3; /* use channel 3 as default */
#endif

	if (osd_hw.osd_meson_dev.cpu_id == __MESON_CPU_MAJOR_ID_T7)
		support_64bit_addr =  1;
	else
		support_64bit_addr =  0;
	return 0;

error2:
	device_unregister(osd_rdma_dev);
error1:
	kfree(osd_rdma_dev);
	osd_rdma_dev = NULL;
	for (i = 0; i < vpp_num; i++) {
		kfree(rdma_temp_tbl[i]);
		rdma_temp_tbl[i] = NULL;
	}
	return -1;
}

int osd_rdma_uninit(void)
{
	int i;

	if (osd_rdma_init_flag) {
		device_unregister(osd_rdma_dev);
		kfree(osd_rdma_dev);
		osd_rdma_dev = NULL;
		for (i = 0; i < VPP_NUM; i++) {
			kfree(rdma_temp_tbl[i]);
			rdma_temp_tbl[i] = NULL;
		}
		osd_rdma_init_flag = false;
	}
	return 0;
}
EXPORT_SYMBOL(osd_rdma_uninit);

static int param_vpp_num = VPP_NUM;
module_param_array(item_count, uint, &param_vpp_num, 0664);
MODULE_PARM_DESC(item_count, "\n item_count\n");
module_param_array(table_paddr, ulong, &param_vpp_num, 0664);
MODULE_PARM_DESC(table_paddr, "\n table_paddr\n");
module_param_array(debug_rdma_status, uint, &param_vpp_num, 0664);
MODULE_PARM_DESC(debug_rdma_status, "\n debug_rdma_status\n");
module_param_array(rdma_irq_count, uint, &param_vpp_num, 0664);
MODULE_PARM_DESC(rdma_irq_count, "\n rdma_irq_count\n");
module_param_array(rdma_lost_count, uint, &param_vpp_num, 0664);
MODULE_PARM_DESC(rdma_lost_count, "\n rdma_lost_count\n");
module_param_array(rdma_recovery_count, uint, &param_vpp_num, 0664);
MODULE_PARM_DESC(rdma_recovery_count, "\n rdma_recovery_count\n");
module_param_array(vsync_irq_count, uint, &param_vpp_num, 0664);
MODULE_PARM_DESC(vsync_irq_count, "\n vsync_irq_count\n");

MODULE_PARM_DESC(rdma_debug, "\n rdma_debug\n");
module_param(rdma_debug, uint, 0664);
MODULE_PARM_DESC(dump_reg_trigger, "\n dump_reg_trigger\n");
module_param(dump_reg_trigger, uint, 0664);
MODULE_PARM_DESC(rdma_hdr_delay, "\n rdma_hdr_delay\n");
module_param(rdma_hdr_delay, uint, 0664);
