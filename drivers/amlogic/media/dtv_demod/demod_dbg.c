// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/dma-mapping.h>
#include <linux/amlogic/aml_dtvdemod.h>
#if IS_ENABLED(CONFIG_AMLOGIC_DMC_DEV_ACCESS)
#include <linux/amlogic/dmc_dev_access.h>
#endif
#include "dvbs.h"
#include "demod_func.h"
#include "amlfrontend.h"
#include "demod_dbg.h"
#include "dvbt_func.h"
#include "isdbt_func.h"
#include "dvbs_diseqc.h"
#include "aml_demod.h"

static unsigned int dtmb_mode;
static unsigned int atsc_mode_para;

static unsigned long demod_dmc_id;
static unsigned int demod_ddr_addr;
static unsigned int demod_ddr_size;

MODULE_PARM_DESC(testbus_addr, "\n\t\t testbus_addr");
static unsigned int testbus_addr = 0x1000;
module_param(testbus_addr, int, 0644);

MODULE_PARM_DESC(testbus_width, "\n\t\t testbus_width");
static unsigned int testbus_width = 9;
module_param(testbus_width, int, 0644);

MODULE_PARM_DESC(testbus_vld, "\n\t\t testbus_vld");
static unsigned int testbus_vld = 0x100000;
module_param(testbus_vld, int, 0644);

MODULE_PARM_DESC(testbus_read_only, "\n\t\t testbus_read_only");
static unsigned int testbus_read_only;
module_param(testbus_read_only, int, 0644);

MODULE_PARM_DESC(testbus_test_mode, "\n\t\t testbus_test_mode");
static unsigned char testbus_test_mode;
module_param(testbus_test_mode, byte, 0644);

static void get_chip_name(struct amldtvdemod_device_s *devp, char *str)
{
	switch (devp->data->hw_ver) {
	case DTVDEMOD_HW_ORG:
		strcpy(str, "DTVDEMOD_HW_ORG");
		break;

	case DTVDEMOD_HW_TXLX:
		strcpy(str, "DTVDEMOD_HW_TXLX");
		break;

	case DTVDEMOD_HW_SM1:
		strcpy(str, "DTVDEMOD_HW_SM1");
		break;

	case DTVDEMOD_HW_TL1:
		strcpy(str, "DTVDEMOD_HW_TL1");
		break;

	case DTVDEMOD_HW_TM2:
		strcpy(str, "DTVDEMOD_HW_TM2");
		break;

	case DTVDEMOD_HW_TM2_B:
		strcpy(str, "DTVDEMOD_HW_TM2_B");
		break;

	case DTVDEMOD_HW_T5:
		strcpy(str, "DTVDEMOD_HW_T5");
		break;

	case DTVDEMOD_HW_T5D:
		strcpy(str, "DTVDEMOD_HW_T5D");
		break;

	case DTVDEMOD_HW_T5D_B:
		strcpy(str, "DTVDEMOD_HW_T5D_B");
		break;

	case DTVDEMOD_HW_S4:
		strcpy(str, "DTVDEMOD_HW_S4");
		break;

	case DTVDEMOD_HW_T3:
		strcpy(str, "DTVDEMOD_HW_T3");
		break;

	case DTVDEMOD_HW_S4D:
		strcpy(str, "DTVDEMOD_HW_S4D");
		break;

	case DTVDEMOD_HW_T5W:
		strcpy(str, "DTVDEMOD_HW_T5W");
		break;

	default:
		strcpy(str, "UNKNOWN");
		break;
	}
}

static void seq_dump_regs(struct seq_file *seq)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();
	struct aml_dtvdemod *demod = NULL;
	unsigned int reg_start, polling_en = 1;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		seq_puts(seq, "demod top start\n");
		for (reg_start = 0; reg_start <= 0xc; reg_start += 4)
			seq_printf(seq, "[0x%x]=0x%x\n", reg_start, demod_top_read_reg(reg_start));

		if (devp->data->hw_ver >= DTVDEMOD_HW_T5D) {
			seq_printf(seq, "[0x10]=0x%x\n", demod_top_read_reg(DEMOD_TOP_CFG_REG_4));
			demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);
		}

		seq_puts(seq, "demod front start\n");
		for (reg_start = 0x20; reg_start <= 0x68; reg_start++)
			seq_printf(seq, "[0x%x]=0x%x\n", reg_start, front_read_reg(reg_start));
	}

	list_for_each_entry(demod, &devp->demod_list, list) {
		seq_printf(seq, "demod regs [id %d]:\n", demod->id);
		switch (demod->last_delsys) {
		case SYS_ATSC:
		case SYS_ATSCMH:
			if (is_meson_txlx_cpu()) {
				for (reg_start = 0; reg_start <= 0xfff; reg_start++)
					seq_printf(seq, "[0x%x] = 0x%x\n",
						   reg_start, atsc_read_reg(reg_start));
			} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
				for (reg_start = 0x0; reg_start <= 0xff; reg_start++)
					seq_printf(seq, "[0x%x] = 0x%x\n",
						   reg_start, atsc_read_reg_v4(reg_start));
			}
			break;

		case SYS_DVBC_ANNEX_A:
		case SYS_DVBC_ANNEX_C:
		case SYS_DVBC_ANNEX_B:
			seq_puts(seq, "dvbc/j83b start\n");
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
				for (reg_start = 0; reg_start <= 0xff; reg_start++)
					seq_printf(seq, "[0x%x] = 0x%x\n",
						   reg_start, qam_read_reg(demod, reg_start));
			}
			break;

		case SYS_DVBT:
		case SYS_DVBT2:
			polling_en = devp->demod_thread;
			devp->demod_thread = 0;
			demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x182);

			if (devp->data->hw_ver >= DTVDEMOD_HW_T5D) {
				for (reg_start = 0x0; reg_start <= 0xf4; reg_start++)
					seq_printf(seq, "[0x%x] = 0x%x\n",
						   reg_start, dvbt_t2_rdb(reg_start));

				for (reg_start = 0x538; reg_start <= 0xfff; reg_start++)
					seq_printf(seq, "[0x%x] = 0x%x\n",
						   reg_start, dvbt_t2_rdb(reg_start));

				/* TODO: flash weak signal during dump the following regs */
				//for (reg_start = 0x1500; reg_start <= 0x3776; reg_start++)
					//seq_printf(seq, "[0x%x] = 0x%x\n",
						   //reg_start, dvbt_t2_rdb(reg_start));
			}

			if (demod->last_delsys == SYS_DVBT2 && demod_is_t5d_cpu(devp))
				demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);

			devp->demod_thread = polling_en;
			break;

		case SYS_DTMB:
			for (reg_start = 0x0; reg_start <= 0xff; reg_start++)
				seq_printf(seq, "[0x%x] = 0x%x\n",
					   reg_start, dtmb_read_reg(reg_start));
			break;

		case SYS_ISDBT:
			for (reg_start = 0x0; reg_start <= 0xff; reg_start++)
				seq_printf(seq, "[0x%x] = 0x%x\n",
					   reg_start, dvbt_isdbt_rd_reg_new(reg_start));
			break;

		case SYS_DVBS:
		case SYS_DVBS2:
			for (reg_start = 0x0; reg_start <= 0xfbf; reg_start++)
				seq_printf(seq, "[0x%x] = 0x%x\n",
					   reg_start, dvbs_rd_byte(reg_start));
			break;

		default:
			seq_puts(seq, "current mode is unknown\n");
			break;
		}
	}

	seq_puts(seq, "\n");
}

static void seq_dump_status(struct seq_file *seq)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();
	struct aml_dtvdemod *demod = NULL;
	int snr, lock_status, agc_if_gain[3];
	unsigned int ser;
	int strength = 0;
	char chip_name[30];
	struct aml_demod_sts demod_sts;
	unsigned int polling_en = 1;

	seq_printf(seq, "version:%s\n", DTVDEMOD_VER);
	seq_printf(seq, "AMLDTVDEMOD_VER:%s\nAMLDTVDEMOD_T2_FW_VER:%s\n",
		AMLDTVDEMOD_VER, AMLDTVDEMOD_T2_FW_VER);
	get_chip_name(devp, chip_name);
	seq_printf(seq, "chip: %d, %s\n", devp->data->hw_ver, chip_name);
	seq_printf(seq, "demod_thread: %d\n", devp->demod_thread);

	list_for_each_entry(demod, &devp->demod_list, list) {
		seq_printf(seq, "demod [id %d]:\n", demod->id);
		strength = tuner_get_ch_power(&demod->frontend);
		seq_printf(seq, "cur delsys: %s\n", dtvdemod_get_cur_delsys(demod->last_delsys));
		seq_printf(seq, "freq: %d\n", demod->freq);

		switch (demod->last_delsys) {
		case SYS_DVBC_ANNEX_A:
		case SYS_DVBC_ANNEX_C:
		case SYS_DVBC_ANNEX_B:
			dvbc_status(demod, &demod_sts, seq);
			break;

		case SYS_DTMB:
			dtmb_information(seq);
			if (strength <= -56) {
				dtmb_read_agc(DTMB_D9_IF_GAIN, &agc_if_gain[0]);
				strength = dtmb_get_power_strength(agc_if_gain[0]);
			}
			break;

		case SYS_ISDBT:
			break;

		case SYS_ATSC:
		case SYS_ATSCMH:
			if (demod->atsc_mode != VSB_8)
				return;

			snr = atsc_read_snr();
			seq_printf(seq, "snr: %d\n", snr);

			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
				lock_status = dtvdemod_get_atsc_lock_sts(demod);
			else
				lock_status = atsc_read_reg(0x0980);

			seq_printf(seq, "lock: %d\n", lock_status);

			ser = atsc_read_ser();
			seq_printf(seq, "ser: %d\n", ser);
			break;

		case SYS_DVBT:
			dvbt_info(demod, seq);
			break;

		case SYS_DVBT2:
			polling_en = devp->demod_thread;
			devp->demod_thread = 0;
			demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x182);
			dvbt2_info(seq);

			if (demod_is_t5d_cpu(devp))
				demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);

			devp->demod_thread = polling_en;
			break;

		case SYS_DVBS:
		case SYS_DVBS2:
			seq_printf(seq, "lock: %d.\n", (dvbs_rd_byte(0x160) >> 3) & 0x1);
			dvbs_check_status(seq);
			break;

		default:
			break;
		}

		seq_printf(seq, "tuner strength : %d, 0x%x\n", strength, strength);
	}
}

static int demod_dump_info_show(struct seq_file *seq, void *v)
{
	seq_dump_status(seq);
	seq_dump_regs(seq);

	return 0;
}

static int demod_version_info_show(struct seq_file *seq, void *v)
{
	seq_printf(seq, "AMLDTVDEMOD_VER:%s\nAMLDTVDEMOD_T2_FW_VER:%s\n",
		AMLDTVDEMOD_VER, AMLDTVDEMOD_T2_FW_VER);
	return 0;
}

#define DEFINE_SHOW_DEMOD(__name) \
static int __name ## _open(struct inode *inode, struct file *file)	\
{ \
	return single_open(file, __name ## _show, inode->i_private);	\
} \
									\
static const struct file_operations __name ## _fops = {			\
	.owner = THIS_MODULE,		\
	.open = __name ## _open,	\
	.read = seq_read,		\
	.llseek = seq_lseek,		\
	.release = single_release,	\
}

/* cat /sys/kernel/debug/demod/dump_info */
DEFINE_SHOW_DEMOD(demod_dump_info);
/* cat /sys/kernel/debug/demod/version_info */
DEFINE_SHOW_DEMOD(demod_version_info);

static struct demod_debugfs_files_t demod_debug_files[] = {
	{"dump_info", S_IFREG | 0644, &demod_dump_info_fops},
	{"version_info", S_IFREG | 0644, &demod_version_info_fops},
};

static void dtv_dmd_parse_param(char *buf_orig, char **parm)
{
	char *ps, *token;
	char delim1[3] = " ";
	char delim2[2] = "\n";
	unsigned int n = 0;

	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}

static void wait_capture(int cap_cur_addr, int depth_MB, int start)
{
	int readfirst;
	int tmp;
	int time_out;
	int last = 0x1000000;

	time_out = 0;
	tmp = depth_MB << 20;

	/* 10 seconds time out */
	while (tmp && (time_out < 500)) {
		time_out = time_out + 1;
		usleep_range(1000, 2000);
		readfirst = front_read_reg(cap_cur_addr);

		if ((last - readfirst) > 0)
			tmp = 0;
		else
			last = readfirst;

		usleep_range(10000, 20000);
	}
}

unsigned int capture_adc_data_mass(void)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();
	int testbus_addr, width, vld;
	unsigned int tb_start, tb_depth;
	unsigned int start_addr = 0;

	/* capture point: common fe */
	testbus_addr = 0x1000;
	tb_depth = 10;
	/* sample bit width */
	width = 9;
	vld = 0x100000;

	/* enable arb */
	front_write_bits(0x39, 1, 30, 1);
	/* enable mask p2 */
	front_write_bits(0x3a, 1, 10, 1);
	/* Stop cap */
	front_write_bits(0x3a, 1, 12, 1);
	/* testbus addr */
	front_write_bits(0x39, testbus_addr, 0, 16);

	/* disable test_mode */
	front_write_bits(0x3a, 0, 13, 1);
	start_addr = devp->mem_start;

	/* 5M for DTMB working use */
	start_addr += (5 * 1024 * 1024);
	front_write_reg(0x3c, start_addr);
	front_write_reg(0x3d, start_addr + ((CAP_SIZE + 2) << 20));
	tb_depth = CAP_SIZE + 2;
	/* set testbus width */
	front_write_bits(0x3a, width, 0, 5);
	/* collect by system clk */
	front_write_reg(0x3b, vld);
	/* disable mask p2 */
	front_write_bits(0x3a, 0, 10, 1);
	front_write_bits(0x39, 0, 31, 1);
	front_write_bits(0x39, 1, 31, 1);
	/* go tb */
	front_write_bits(0x3a, 0, 12, 1);
	wait_capture(0x3f, tb_depth, start_addr);
	/* stop tb */
	/* front_write_bits(0x3a, 1, 12, 1); */
	tb_start = front_read_reg(0x3f);

	return tb_start;
}

int write_usb_mass(struct dtvdemod_capture_s *cap, unsigned int read_start)
{
	int count;
	unsigned int i, memsize, y, block_size;
	int times;
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buf = NULL;
	char *parm;
	mm_segment_t old_fs = get_fs();

	parm = cap->cap_dev_name;

	memsize = CAP_SIZE << 20;
	set_fs(KERNEL_DS);
	filp = filp_open(parm, O_RDWR | O_CREAT, 0666);
	if (IS_ERR(filp))
		return -1;

	buf =	phys_to_virt(read_start);
	times = memsize / PER_WR;
	block_size = PER_WR;

	for (i = 0; i < cap->cap_size; i++) {
		while (front_read_reg(0x3f) <= (read_start + memsize * 3 / 4))
			usleep_range(5000, 5001);

		for (y = 0; y < times; y++) {
			count = vfs_write(filp, buf + y * PER_WR, block_size, &pos);
			if (count <= 0)
				pr_info("block_size is %d, 0x%lx, count is %d\n", block_size,
					y * PER_WR, count);
			usleep_range(5000, 5001);
		}
	}

	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);

	/* stop tb */
	front_write_bits(0x3a, 1, 12, 1);
	return 0;
}

static u8 *demod_vmap(ulong addr, u32 size)
{
	u8 *vaddr = NULL;
	struct page **pages = NULL;
	u32 i, npages, offset = 0;
	ulong phys, page_start;
	/*pgprot_t pgprot = pgprot_noncached(PAGE_KERNEL);*/
	pgprot_t pgprot = PAGE_KERNEL;

	if (!PageHighMem(phys_to_page(addr)))
		return phys_to_virt(addr);

	offset = offset_in_page(addr);
	page_start = addr - offset;
	npages = DIV_ROUND_UP(size + offset, PAGE_SIZE);

	pages = kmalloc_array(npages, sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return NULL;

	for (i = 0; i < npages; i++) {
		phys = page_start + i * PAGE_SIZE;
		pages[i] = pfn_to_page(phys >> PAGE_SHIFT);
	}

	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	if (!vaddr) {
		pr_err("the phy(%lx) vmaped fail, size: %d\n",
		       page_start, npages << PAGE_SHIFT);
		kfree(pages);
		return NULL;
	}

	kfree(pages);
	pr_info("[demod vmap] %s, pa(%lx) to va(%p), size: %d\n",
		__func__, page_start, vaddr, npages << PAGE_SHIFT);

	return vaddr + offset;
}

static void demod_unmap_phyaddr(u8 *vaddr)
{
	void *addr = (void *)(PAGE_MASK & (ulong)vaddr);

	if (is_vmalloc_or_module_addr(vaddr)) {
		vunmap(addr);
		PR_INFO("%s:%p\n", __func__, vaddr);
	}
}

static void demod_dma_flush(void *vaddr, int size, enum dma_data_direction dir)
{
	ulong phy_addr;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (unlikely(!devp)) {
		PR_ERR("%s:devp is NULL\n", __func__);
		return;
	}

	if (is_vmalloc_or_module_addr(vaddr)) {
		phy_addr = page_to_phys(vmalloc_to_page(vaddr))
			+ offset_in_page(vaddr);
		if (phy_addr && PageHighMem(phys_to_page(phy_addr))) {
			dma_sync_single_for_device(&devp->this_pdev->dev,
						   phy_addr, size, dir);
		}
		return;
	}
}

static int read_memory_to_file(char *path, unsigned int start_addr,
				unsigned int size)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buf = NULL;
	mm_segment_t old_fs = get_fs();

	if (!start_addr) {
		PR_ERR("%s: start addr is NULL\n", __func__);
		return -1;
	}

	if (unlikely(!path)) {
		PR_ERR("%s:capture path is NULL\n", __func__);
		return -1;
	}

	PR_INFO("capture data path:%s,start addr:0x%x, size:%dM\n ",
		path, start_addr, size / SZ_1M);

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR | O_CREAT, 0666);

	buf = demod_vmap(start_addr, size);
	if (!buf) {
		PR_ERR("%s:buf is NULL\n", __func__);
		return -1;
	}

	demod_dma_flush(buf, size, DMA_FROM_DEVICE);
	vfs_write(filp, buf, size, &pos);
	demod_unmap_phyaddr(buf);
	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);

	return 0;
}

unsigned int capture_adc_data_once(char *path, unsigned int capture_mode,
		unsigned int test_mode)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();
	int addr = 0, width = 0, vld = 0;
	unsigned int tb_start = 0, tb_depth = 0;
	unsigned int start_addr = 0;
	unsigned int top_saved = 0, polling_en = 0;
	//struct dvb_frontend *fe;
	struct aml_dtvdemod *demod = NULL, *tmp = NULL;
	unsigned int offset = 0, size = 0;

	if (unlikely(!devp)) {
		PR_ERR("%s:devp is NULL\n", __func__);
		return -1;
	}

	list_for_each_entry(tmp, &devp->demod_list, list) {
		if (tmp->id == 0) {
			demod = tmp;
			//fe = &demod->frontend;
			break;
		}
	}

	if (unlikely(!demod)) {
		PR_ERR("%s: demod is NULL\n", __func__);
		return -1;
	}

	PR_INFO("%s:[path]%s,[cap_mode]%d\n", __func__, path, capture_mode);
	switch (capture_mode) {
	case 0: /* common fe */
		if (devp->data->hw_ver == DTVDEMOD_HW_S4D ||
			devp->data->hw_ver == DTVDEMOD_HW_S4) {
			addr = 0x101b;
			//tb_depth = 10;
			/* sample bit width */
			width = 19;
			vld = 0x100000;
		} else {
			addr = 0x1000;
			//tb_depth = 10;
			/* sample bit width */
			width = 9;
			vld = 0x100000;
		}
		break;

	case 3: /* ts stream */
		addr = 0x3000;
		//tb_depth = 100;
		/* sample bit width */
		width = 7;
		vld = 0x10000;
		break;

	case 4: /* T/T2 */
		addr = 0x1000;
		//tb_depth = 10;
		/* sample bit width */
		width = 11;
		vld = 0x100000;
		break;

	case 5: /* S/S2 */
		if (devp->data->hw_ver == DTVDEMOD_HW_S4D ||
			devp->data->hw_ver == DTVDEMOD_HW_S4) {
			addr = 0x101b;
			//tb_depth = 10;
			/* sample bit width */
			width = 19;
			vld = 0x100000;
		} else {
			addr = 0x1012;
			//tb_depth = 10;
			/* sample bit width */
			width = 19;
			vld = 0x100000;
		}
		break;

	case 6: /* diseqc in */
		addr = 0x1000;
		//tb_depth = 10;
		/* sample bit width */
		width = 9;
		vld = 0x100000;
		break;

	case 7: /* user define */
		addr = testbus_addr;
		//tb_depth = 10;
		/* sample bit width */
		width = testbus_width;
		vld = testbus_vld;
		break;

	default: /* common fe */
		addr = 0x1000;
		//tb_depth = 10;
		/* sample bit width */
		width = 9;
		vld = 0x100000;
		break;
	}

	PR_INFO("%s: testbus addr:0x%x, width:%d, vld:0x%x, read_only:%d.\n",
			__func__, addr, width, vld, testbus_read_only);

	if (devp->data->hw_ver >= DTVDEMOD_HW_T5D) {
		polling_en = devp->demod_thread;
		devp->demod_thread = 0;
		top_saved = demod_top_read_reg(DEMOD_TOP_CFG_REG_4);
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);
	}
	msleep(3000);
	/* enable arb */
	front_write_bits(0x39, 1, 30, 1);
	/* enable mask p2 */
	front_write_bits(0x3a, 1, 10, 1);
	/* Stop cap */
	front_write_bits(0x3a, 1, 12, 1);
	/* testbus addr */
	front_write_bits(0x39, addr, 0, 16);

	/* disable test_mode */
	if (test_mode || testbus_test_mode)
		front_write_bits(0x3a, 1, 13, 1);
	else
		front_write_bits(0x3a, 0, 13, 1);

	if (!demod_ddr_addr)
		start_addr = devp->mem_start;
	else
		start_addr = demod_ddr_addr;

	switch (demod->last_delsys) {
	case SYS_DTMB:
	case SYS_ISDBT:
		offset = 5 * 1024 * 1024;
		break;

	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
	case SYS_DVBT:
	case SYS_DVBS:
	case SYS_DVBS2:
		offset = 0 * 1024 * 1024;
		break;

	case SYS_DVBT2:
		offset = 32 * 1024 * 1024;
		break;

	default:
		offset = 32 * 1024 * 1024;
		PR_ERR("%s: unknown delivery system\n", __func__);
		break;
	}

	if (!demod_ddr_size)
		size = devp->mem_size - offset;
	else
		size = demod_ddr_size;

	PR_INFO("%s: capture_mode:%d, test_mode:%d, start_addr:0x%x, offset:%dM, size:%dM.\n",
			__func__, capture_mode, test_mode || testbus_test_mode,
			start_addr, offset / SZ_1M, size / SZ_1M);

	start_addr += offset;
	front_write_reg(0x3c, start_addr);
	front_write_reg(0x3d, start_addr + size);
	tb_depth = size / SZ_1M;
	/* set testbus width */
	front_write_bits(0x3a, width, 0, 5);

	/* sel adc 10bit */
	if (width == 9)
		demod_top_write_bits(DEMOD_TOP_REGC, 0, 8, 1);
	else if (width == 11)/* sel adc 12bit */
		demod_top_write_bits(DEMOD_TOP_REGC, 1, 8, 1);

	/* collect by system clk */
	front_write_reg(0x3b, vld);
	/* disable mask p2 */
	if (!testbus_read_only) {
		front_write_bits(0x3a, 0, 10, 1);
		front_write_bits(0x39, 0, 31, 1);
		front_write_bits(0x39, 1, 31, 1);

		/* go tb */
		front_write_bits(0x3a, 0, 12, 1);
		wait_capture(0x3f, tb_depth, start_addr);
	}

	/* stop tb */
	front_write_bits(0x3a, 1, 12, 1);
	tb_start = front_read_reg(0x3f);

	if (devp->data->hw_ver >= DTVDEMOD_HW_T5D) {
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, top_saved);
		devp->demod_thread = polling_en;
	}

	if (!testbus_read_only)
		read_memory_to_file(path, tb_start, size);
	else
		read_memory_to_file(path, start_addr, size);

	return 0;
}

unsigned int clear_ddr_bus_data(struct aml_dtvdemod *demod)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();
	int testbus_addr = 0, width = 0, vld = 0;
	unsigned int tb_start = 0, tb_depth = 0;
	unsigned int start_addr = 0;
	unsigned int top_saved = 0, polling_en = 0;
	unsigned int offset = 0, size = 0;

	testbus_addr = 0x1000;
	width = 9;
	vld = 0x100000;

	if (devp->data->hw_ver >= DTVDEMOD_HW_T5D) {
		polling_en = devp->demod_thread;
		devp->demod_thread = 0;
		top_saved = demod_top_read_reg(DEMOD_TOP_CFG_REG_4);
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);
	}
	/* enable arb */
	front_write_bits(0x39, 1, 30, 1);
	/* enable mask p2 */
	front_write_bits(0x3a, 1, 10, 1);
	/* Stop cap */
	front_write_bits(0x3a, 1, 12, 1);
	/* testbus addr */
	front_write_bits(0x39, testbus_addr, 0, 16);

	/* enable test_mode */
	front_write_bits(0x3a, 1, 13, 1);
	start_addr = devp->mem_start;

	offset = 0 * 1024 * 1024;

	size = (1 * 1024) - offset;
	start_addr += offset;
	front_write_reg(0x3c, start_addr);
	front_write_reg(0x3d, start_addr + size);
	tb_depth = size / SZ_1M;
	/* set testbus width */
	front_write_bits(0x3a, width, 0, 5);

	/* sel adc 10bit */
	demod_top_write_bits(DEMOD_TOP_REGC, 0, 8, 1);

	/* collect by system clk */
	front_write_reg(0x3b, vld);
	/* disable mask p2 */
	front_write_bits(0x3a, 0, 10, 1);
	front_write_bits(0x39, 0, 31, 1);
	front_write_bits(0x39, 1, 31, 1);
	/* go tb */
	front_write_bits(0x3a, 0, 12, 1);
	wait_capture(0x3f, tb_depth, start_addr);
	/* stop tb */
	front_write_bits(0x3a, 1, 12, 1);
	tb_start = front_read_reg(0x3f);

	if (devp->data->hw_ver >= DTVDEMOD_HW_T5D) {
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, top_saved);
		devp->demod_thread = polling_en;
	}

	PR_DBGL("%s: clear done.\n", __func__);

	return 0;
}

static void dbg_ic_cfg_addr(struct amldtvdemod_device_s *devp)
{
	char *name_reg[ES_MAP_ADDR_NUM] = {
		"demod",
		"iohiu",
		"aobus",
		"reset",
	};
	struct ss_reg_phy *preg = &devp->reg_p[0];
	struct ss_reg_vt *regv = &devp->reg_v[0];
	int i = 0;

	PR_INFO("demod top :0x%x\n", devp->data->regoff.off_demod_top);
	PR_INFO("dvbc      :0x%x\n", devp->data->regoff.off_dvbc);
	PR_INFO("dtmb      :0x%x\n", devp->data->regoff.off_dtmb);
	PR_INFO("dvbt/isdbt:0x%x\n", devp->data->regoff.off_dvbt_isdbt);
	PR_INFO("isdbt     :0x%x\n", devp->data->regoff.off_isdbt);
	PR_INFO("atsc      :0x%x\n", devp->data->regoff.off_atsc);
	PR_INFO("front     :0x%x\n", devp->data->regoff.off_front);
	PR_INFO("dvbt/t2   :0x%x\n", devp->data->regoff.off_dvbt_t2);

	for (i = 0; i < ES_MAP_ADDR_NUM; i++)
		PR_INFO("%s: phy_addr=0x%x size=0x%x vir_addr=0x%p.\n",
			name_reg[i], preg[i].phy_addr, preg[i].size, regv[i].v);
}

static void info_show(void)
{
	int snr, lock_status, agc_if_gain[3];
	unsigned int ser;
	int strength = 0;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();
	struct dtv_frontend_properties *c = NULL;
	struct aml_dtvdemod *demod = NULL;
	unsigned int debug_mode = aml_demod_debug;
	char chip_name[30];
	struct aml_demod_sts demod_sts;
	u32 ber;
	int fw_ver = 0;

	PR_INFO("DTV DEMOD state:\n");
	PR_INFO("demod_thread: %d.\n", devp->demod_thread);
	get_chip_name(devp, chip_name);
	PR_INFO("hw version chip: %d, %s.\n", devp->data->hw_ver, chip_name);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T3))
		fw_ver = dvbt_t2_rdb(0x48);
	PR_INFO("version: %s-%s, T2 FW ver: V%d.%s.\n", AMLDTVDEMOD_VER,
		DTVDEMOD_VER, fw_ver, AMLDTVDEMOD_T2_FW_VER);

	dbg_ic_cfg_addr(devp);

	PR_INFO("agc_pin_direction: %d.\n", devp->agc_direction);

	PR_INFO("iq_swap: %d.\n", dvbs_get_iq_swap());

	aml_diseqc_status(&devp->diseqc);

	list_for_each_entry(demod, &devp->demod_list, list) {
		strength = tuner_get_ch_power(&demod->frontend);

		c = &demod->frontend.dtv_property_cache;

		PR_INFO("demod [id %d]: 0x%p\n", demod->id, demod);
		PR_INFO("current delsys: %s.\n", dtvdemod_get_cur_delsys(demod->last_delsys));
		PR_INFO("delsys:%d, freq:%d, symbol_rate:%d, bw:%d, modulation:%d, invert:%d.\n",
				c->delivery_system, c->frequency, c->symbol_rate,
				c->bandwidth_hz, c->modulation, c->inversion);

		switch (demod->last_delsys) {
		case SYS_DVBC_ANNEX_A:
		case SYS_DVBC_ANNEX_C:
		case SYS_DVBC_ANNEX_B:
			dvbc_status(demod, &demod_sts, NULL);
			break;

		case SYS_DTMB:
			dtmb_information(NULL);
			if (strength <= -56) {
				dtmb_read_agc(DTMB_D9_IF_GAIN, &agc_if_gain[0]);
				strength = dtmb_get_power_strength(agc_if_gain[0]);
			}
			break;

		case SYS_ISDBT:
			break;

		case SYS_ATSC:
		case SYS_ATSCMH:
			if (demod->atsc_mode != VSB_8)
				return;

			snr = atsc_read_snr();
			PR_INFO("snr: %d.\n", snr);

			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
				lock_status = dtvdemod_get_atsc_lock_sts(demod);
			else
				lock_status = atsc_read_reg(0x0980);

			PR_INFO("lock: %d.\n", lock_status);

			ser = atsc_read_ser();
			PR_INFO("ser: %d.\n", ser);

			PR_INFO("atsc rst done: %d.\n", demod->atsc_rst_done);
			break;

		case SYS_DVBT:
			aml_demod_debug |= DBG_DVBT;
			dvbt_info(demod, NULL);
			aml_demod_debug = debug_mode;
			break;

		case SYS_DVBT2:
			aml_demod_debug |= DBG_DVBT;
			devp->demod_thread = 0;
			demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x182);
			dvbt2_info(NULL);
			if (demod_is_t5d_cpu(devp))
				demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);

			devp->demod_thread = 1;
			aml_demod_debug = debug_mode;
			break;

		case SYS_DVBS:
		case SYS_DVBS2:
			PR_INFO("lock: %d.\n", (dvbs_rd_byte(0x160) >> 3) & 0x1);
			dvbs_check_status(NULL);
			break;

		default:
			break;
		}

		PR_INFO("tuner strength: %d, 0x%x.\n", strength, strength);

		if (!aml_dtvdm_read_ber(&demod->frontend, &ber))
			PR_INFO("ber=%d\n", ber);
	}
}

static void dump_regs(struct aml_dtvdemod *demod)
{
	unsigned int reg_start, polling_en = 1;
	enum fe_delivery_system delsys = demod->last_delsys;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		pr_info("demod top start\n");
		for (reg_start = 0; reg_start <= 0xc; reg_start += 4)
			pr_info("[0x%x]=0x%x\n", reg_start, demod_top_read_reg(reg_start));
		pr_info("demod top end\n\n");

		pr_info("demod front start\n");
		for (reg_start = 0x20; reg_start <= 0x68; reg_start++)
			pr_info("[0x%x]=0x%x\n", reg_start, front_read_reg(reg_start));
		pr_info("demod front end\n\n");
	}

	switch (delsys) {
	case SYS_ATSC:
	case SYS_ATSCMH:
		pr_info("atsc start\n");
		if (is_meson_txlx_cpu()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
			for (reg_start = 0; reg_start <= 0xfff; reg_start++)
				pr_info("[0x%x] = 0x%x\n", reg_start, atsc_read_reg(reg_start));
#endif
		} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			for (reg_start = 0; reg_start <= 0xff; reg_start++)
				pr_info("[0x%x] = 0x%x\n", reg_start, atsc_read_reg_v4(reg_start));
		}
		pr_info("atsc end\n");
		break;

	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
	case SYS_DVBC_ANNEX_B:
		pr_info("dvbc/j83b start\n");
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			for (reg_start = 0; reg_start <= 0xff; reg_start++)
				pr_info("[0x%x] = 0x%x\n", reg_start,
						qam_read_reg(demod, reg_start));
		}
		pr_info("dvbc/j83b end\n");
		break;

	case SYS_DVBT:
	case SYS_DVBT2:
		pr_info("dvbt/t2 start\n");
		if (demod->last_delsys == SYS_DVBT2) {
			polling_en = devp->demod_thread;
			devp->demod_thread = 0;
			demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x182);
		}

		if (devp->data->hw_ver >= DTVDEMOD_HW_T5D) {
			for (reg_start = 0x0; reg_start <= 0xf4; reg_start++)
				pr_info("[0x%x] = 0x%x\n", reg_start, dvbt_t2_rdb(reg_start));

			for (reg_start = 0x538; reg_start <= 0xfff; reg_start++)
				pr_info("[0x%x] = 0x%x\n", reg_start, dvbt_t2_rdb(reg_start));

			for (reg_start = 0x1500; reg_start <= 0x3776; reg_start++)
				pr_info("[0x%x] = 0x%x\n", reg_start, dvbt_t2_rdb(reg_start));
		}

		if (demod->last_delsys == SYS_DVBT2) {
			if (demod_is_t5d_cpu(devp))
				demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);

			devp->demod_thread = polling_en;
		}
		pr_info("dvbt/t2 end\n");
		break;

	case SYS_DTMB:
		pr_info("dtmb start\n");
		for (reg_start = 0x0; reg_start <= 0xff; reg_start++)
			pr_info("[0x%x] = 0x%x\n", reg_start, dtmb_read_reg(reg_start));

		pr_info("dtmb end\n");
		break;

	case SYS_ISDBT:
		pr_info("isdbt start\n");
		for (reg_start = 0x0; reg_start <= 0xff; reg_start++)
			pr_info("[0x%x] = 0x%x\n", reg_start, dvbt_isdbt_rd_reg_new(reg_start));

		pr_info("isdbt end\n");
		break;

	case SYS_DVBS:
	case SYS_DVBS2:
		pr_info("dvbs start\n");
		for (reg_start = 0x0; reg_start <= 0xfbf; reg_start++)
			pr_info("[0x%x] = 0x%x\n", reg_start, dvbs_rd_byte(reg_start));

		pr_info("dvbs end\n");
		break;

	default:
		break;
	}
}

static ssize_t attr_store(struct class *cls, struct class_attribute *attr,
		const char *buf, size_t count)
{
	char *buf_orig, *parm[47] = {NULL};
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();
	struct aml_dtvdemod *demod = NULL, *tmp = NULL;
	struct isdbt_tmcc_info tmcc_info;
	unsigned int capture_start = 0;
	unsigned int val = 0;
	unsigned int addr = 0;
	unsigned int i;
	static struct dvb_frontend *fe;
	struct demod_config config;
	struct dtv_property tvp;
	unsigned int delay;
	enum fe_status sts;
	unsigned int cap_mode = 0, test_mode = 0;

	if (!buf)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	dtv_dmd_parse_param(buf_orig, (char **)&parm);

	list_for_each_entry(tmp, &devp->demod_list, list) {
		if (tmp->id == 0) {
			demod = tmp;
			fe = &demod->frontend;
			break;
		}
	}

	if (!demod) {
		fe = aml_dtvdm_attach(&config);
		if (!fe) {
			pr_err("delsys, fe is NULL\n");
		} else {
			list_for_each_entry(tmp, &devp->demod_list, list) {
				if (tmp->id == 0) {
					demod = tmp;
					fe = &demod->frontend;
					break;
				}
			}
		}
	}

	if (!demod || !parm[0])
		goto fail_exec_cmd;

	if (!strcmp(parm[0], "symb_rate")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val) == 0))
			demod->symbol_rate_manu = val;
	} else if (!strcmp(parm[0], "symb_rate_en")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val) == 0))
			demod->symb_rate_en = val;
	} else if (!strcmp(parm[0], "capture_mass")) {
		if (parm[1] && (strlen(parm[1]) < CAP_NAME_LEN)) /* path/name of ssd */
			strcpy(devp->capture_para.cap_dev_name, parm[1]);
		else
			goto fail_exec_cmd;

		if (parm[2] && (kstrtouint(parm[2], 10, &val) == 0)) /* capture size */
			devp->capture_para.cap_size = val;
		else
			goto fail_exec_cmd;

		capture_start = capture_adc_data_mass();
		write_usb_mass(&devp->capture_para, capture_start);

	} else if (!strcmp(parm[0], "capture_once")) {
		if (!parm[1]) {
			PR_ERR("%s:capture_once, no path para\n", __func__);
			goto fail_exec_cmd;
		}

		if (parm[2] && kstrtouint(parm[2], 10, &cap_mode) == 0) {
			if (parm[3] && kstrtouint(parm[3], 10, &test_mode) == 0)
				capture_adc_data_once(parm[1], cap_mode, test_mode);
			else
				capture_adc_data_once(parm[1], cap_mode, 0);
		} else {
			capture_adc_data_once(parm[1], 0, 0);
		}
	} else if (!strcmp(parm[0], "state")) {
		info_show();
	} else if (!strcmp(parm[0], "delsys")) {
		tvp.cmd = DTV_DELIVERY_SYSTEM;

		/* ref include/uapi/linux/dvb/frontend.h */
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0) {
			tvp.u.data = val;
			fe->dtv_property_cache.delivery_system = val;
		} else {
			tvp.u.data = SYS_DTMB;
			fe->dtv_property_cache.delivery_system = SYS_DTMB;
		}

		/* ref include/uapi/linux/dvb/frontend.h */
		if (parm[2] && (kstrtouint(parm[2], 10, &val)) == 0)
			fe->dtv_property_cache.frequency = val;
		else
			fe->dtv_property_cache.frequency = 88000000;

		if (parm[3] && (kstrtouint(parm[3], 10, &val)) == 0)
			fe->dtv_property_cache.symbol_rate = val;
		else
			fe->dtv_property_cache.symbol_rate = 0;

		if (parm[4] && (kstrtouint(parm[4], 10, &val)) == 0)
			fe->dtv_property_cache.bandwidth_hz = val;
		else
			fe->dtv_property_cache.bandwidth_hz = 8000000;

		if (parm[5] && (kstrtouint(parm[5], 10, &val)) == 0)
			fe->dtv_property_cache.modulation = val;
		else
			fe->dtv_property_cache.modulation = QAM_AUTO;

		if (fe->dtv_property_cache.delivery_system == SYS_DVBS ||
			fe->dtv_property_cache.delivery_system == SYS_DVBS2)
			fe->dtv_property_cache.frequency = fe->dtv_property_cache.frequency / 1000;

		if (fe->ops.init && demod->last_delsys == SYS_UNDEFINED)
			fe->ops.init(fe);
		if (fe->ops.set_property)
			fe->ops.set_property(fe, &tvp);
		if (fe->ops.tune)
			fe->ops.tune(fe, true, 0, &delay, &sts);
	} else if (!strcmp(parm[0], "tune")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			fe->ops.tune(fe, val, 0, &delay, &sts);
	} else if (!strcmp(parm[0], "retune")) {
		if (fe->ops.tune)
			fe->ops.tune(fe, true, 0, &delay, &sts);
	} else if (!strcmp(parm[0], "stop_wr")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			devp->stop_reg_wr = val;
	} else if (!strcmp(parm[0], "timeout_atsc")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			demod->timeout_atsc_ms = val;
	} else if (!strcmp(parm[0], "timeout_dvbt")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			demod->timeout_dvbt_ms = val;
	} else if (!strcmp(parm[0], "timeout_dvbs")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			demod->timeout_dvbs_ms = val;
	} else if (!strcmp(parm[0], "timeout_dvbc")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			demod->timeout_dvbc_ms = val;
	} else if (!strcmp(parm[0], "dump_reg")) {
		dump_regs(demod);
	} else if (!strcmp(parm[0], "get_plp")) {
		dtvdemod_get_plp_dbg();
	} else if (!strcmp(parm[0], "set_plp")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			dtvdemod_set_plpid(val);
	} else if (!strcmp(parm[0], "lnb_en")) {
	} else if (!strcmp(parm[0], "lnb_sel")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			aml_diseqc_set_voltage(fe, (enum fe_sec_voltage)val);
	} else if (!strcmp(parm[0], "diseqc_reg")) {
		demod_dump_reg_diseqc();
	} else if (!strcmp(parm[0], "diseqc_dbg")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			aml_diseqc_dbg_en(val);
	} else if (!strcmp(parm[0], "diseqc_sts")) {
		aml_diseqc_status(&devp->diseqc);
	} else if (!strcmp(parm[0], "diseqc_cmd")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			diseqc_cmd_bypass = val;
		PR_INFO("diseqc_cmd_bypass %d\n", val);
	} else if (!strcmp(parm[0], "diseqc_burston")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			sendburst_on = val;
		PR_INFO("sendburst_on %d\n", val);
	} else if (!strcmp(parm[0], "diseqc_burstsa")) {
		aml_diseqc_toneburst_sa();
	} else if (!strcmp(parm[0], "diseqc_burstsb")) {
		aml_diseqc_toneburst_sb();
	} else if (!strcmp(parm[0], "diseqc_toneon")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0) {
			aml_diseqc_tone_on(&devp->diseqc, val);
			PR_INFO("continuous_tone %d\n", val);
		}
	} else if (!strcmp(parm[0], "monitor")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			devp->print_on = val;
	} else if (!strcmp(parm[0], "strength_limit")) {
		if (parm[1] && (kstrtoint(parm[1], 10, &devp->tuner_strength_limit)) == 0)
			;
	} else if (!strcmp(parm[0], "debug_on")) {
		if (parm[1] && (kstrtoint(parm[1], 10, &devp->debug_on)) == 0)
			;
	} else if (!strcmp(parm[0], "dvbc_sel")) {
		if (parm[1] && (kstrtoint(parm[1], 10, &demod->dvbc_sel)) == 0)
			;
	} else if (!strcmp(parm[0], "dvbsw")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			if (parm[2] && (kstrtouint(parm[2], 16, &val)) == 0) {
				dvbs_wr_byte(addr, val);
				PR_INFO("dvbs wr addr:0x%x, val:0x%x\n", addr, val);
			}
		}
	} else if (!strcmp(parm[0], "dvbsr")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			val = dvbs_rd_byte(addr);
			PR_INFO("dvds rd addr:0x%x, val:0x%x\n", addr, val);
		}
	} else if (!strcmp(parm[0], "dvbsd")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			if (parm[2] && (kstrtouint(parm[2], 16, &val)) == 0) {
				for (i = addr; i < (addr + val); i++)
					PR_INFO("dvds rd addr:0x%x, val:0x%x\n",
						i, dvbs_rd_byte(i));
			}
		}
	} else if (!strcmp(parm[0], "dvbtr")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			val = dvbt_t2_rdb(addr);
			PR_INFO("dvdt rd addr:0x%x, val:0x%x\n", addr, val);
		}
	} else if (!strcmp(parm[0], "dvbtw")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			if (parm[2] && (kstrtouint(parm[2], 16, &val)) == 0) {
				dvbt_t2_wrb(addr, val);
				PR_INFO("dvbt wr addr:0x%x, val:0x%x\n", addr, val);
			}
		}
	} else if (!strcmp(parm[0], "topr")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			val = demod_top_read_reg(addr);
			PR_INFO("top rd addr:0x%x, val:0x%x\n", addr, val);
		}
	} else if (!strcmp(parm[0], "topw")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			if (parm[2] && (kstrtouint(parm[2], 16, &val)) == 0) {
				demod_top_write_reg(addr, val);
				PR_INFO("top wr addr:0x%x, val:0x%x\n", addr, val);
			}
		}
	} else if (!strcmp(parm[0], "frontr")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			val = front_read_reg(addr);
			PR_INFO("front rd addr:0x%x, val:0x%x\n", addr, val);
		}
	} else if (!strcmp(parm[0], "frontw")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			if (parm[2] && (kstrtouint(parm[2], 16, &val)) == 0) {
				front_write_reg(addr, val);
				PR_INFO("front wr addr:0x%x, val:0x%x\n", addr, val);
			}
		}
	} else if (!strcmp(parm[0], "atscr")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			val = atsc_read_reg_v4(addr);
			PR_INFO("atsc rd addr:0x%x, val:0x%x\n", addr, val);
		}
	} else if (!strcmp(parm[0], "atscw")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			if (parm[2] && (kstrtouint(parm[2], 16, &val)) == 0) {
				atsc_write_reg_v4(addr, val);
				PR_INFO("atsc wr addr:0x%x, val:0x%x\n", addr, val);
			}
		}
	} else if (!strcmp(parm[0], "dvbcr")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			val = qam_read_reg(demod, addr);
			PR_INFO("dvbc rd addr:0x%x, val:0x%x\n", addr, val);
		}
	} else if (!strcmp(parm[0], "dvbcw")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			if (parm[2] && (kstrtouint(parm[2], 16, &val)) == 0) {
				qam_write_reg(demod, addr, val);
				PR_INFO("dvbc wr addr:0x%x, val:0x%x\n", addr, val);
			}
		}
	} else if (!strcmp(parm[0], "dtmbr")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			val = dtmb_read_reg(addr);
			PR_INFO("dtmb rd addr:0x%x, val:0x%x\n", addr, val);
		}
	} else if (!strcmp(parm[0], "dtmbw")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			if (parm[2] && (kstrtouint(parm[2], 16, &val)) == 0) {
				dtmb_write_reg(addr, val);
				PR_INFO("dtmb wr addr:0x%x, val:0x%x\n", addr, val);
			}
		}
	} else if (!strcmp(parm[0], "isdbtr")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			val = dvbt_isdbt_rd_reg_new(addr);
			PR_INFO("isdbt rd addr:0x%x, val:0x%x\n", addr, val);
		}
	} else if (!strcmp(parm[0], "isdbtw")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr)) == 0) {
			if (parm[2] && (kstrtouint(parm[2], 16, &val)) == 0) {
				dvbt_isdbt_wr_reg_new(addr, val);
				PR_INFO("isdbt wr addr:0x%x, val:0x%x\n", addr, val);
			}
		}
	} else if (!strcmp(parm[0], "demod_thread")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &val)) == 0)
			devp->demod_thread = val;
	} else if (!strcmp(parm[0], "demod_id")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &val)) == 0)
			demod_id = val;
	} else if (!strcmp(parm[0], "blind_stop")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &val)) == 0)
			devp->blind_scan_stop = val;
		PR_INFO("set blind scan to %d\n", devp->blind_scan_stop);
	} else if (!strcmp(parm[0], "cr_val")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &val)) == 0)
			devp->atsc_cr_step_size_dbg = val;
		PR_INFO("set atsc cr val to 0x%x\n", devp->atsc_cr_step_size_dbg);
	} else if (!strcmp(parm[0], "ci_mode")) {
		if (demod->demod_status.delsys == SYS_DVBC_ANNEX_A) {
			if (parm[1] && (kstrtouint(parm[1], 16, &val)) == 0) {
				if (val == 0) {
					qam_write_bits(demod, 0x11, 0x90, 24, 8);
					demod->ci_mode = 0;
				} else if (val == 1) {
					qam_write_bits(demod, 0x11, 0x00, 24, 8);
					demod->ci_mode = 1;
				}
				PR_INFO("ic card set mode to %d\n", val);
			}
		} else {
			PR_INFO("not dvbc mode,nothing to do\n");
		}
	} else if (!strcmp(parm[0], "blind_min")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			devp->blind_debug_min_frc = val;
		PR_INFO("set blind frec min to %d\n", devp->blind_debug_min_frc);
	} else if (!strcmp(parm[0], "blind_max")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			devp->blind_debug_max_frc = val;
		PR_INFO("set blind frec max to %d\n", devp->blind_debug_max_frc);
	} else if (!strcmp(parm[0], "timeout_ddr_leave")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &val)) == 0)
			demod->timeout_ddr_leave = val;
	} else if (!strcmp(parm[0], "register_dmc")) {
		demod_dmc_notifier();
	} else if (!strcmp(parm[0], "tmcc")) {
		isdbt_get_tmcc_info(&tmcc_info);
	} else {
		PR_INFO("invalid command: %s.\n", parm[0]);
	}

fail_exec_cmd:
	kfree(buf_orig);

	return count;
}

static ssize_t attr_show(struct class *cls,
				 struct class_attribute *attr, char *buf)
{
	/* struct amldtvdemod_device_s *devp = dev_get_drvdata(dev); */
	ssize_t len = 0;

	len += sprintf(buf + len, "\nDTV Demod Usage:\n");
	len += sprintf(buf + len, "echo [CMD] > /sys/class/dtvdemod/attr\n");
	len += sprintf(buf + len, "CMD:\n");
	len += sprintf(buf + len, "\tsymb_rate [val]\n");
	len += sprintf(buf + len, "\tsymb_rate_en [val]\n");
	len += sprintf(buf + len, "\tstate\n");
	len += sprintf(buf + len, "\tdelsys [deliver_system] [frequency_hz] [symbol_rate_bps] [bw_hz] [modulation]\n");
	len += sprintf(buf + len, "\ttune [val]\n");
	len += sprintf(buf + len, "\tfrontw|topw|dvbsw|atscw|dvbcw|dvbtw [addr] [val]\n");
	len += sprintf(buf + len, "\tfrontr|topr|dvbsr|atscr|dvbcr|dvbtr [addr]\n");
	len += sprintf(buf + len, "\tdvbsd [addr] [len]\n");
	len += sprintf(buf + len, "\tlnb_en [val]\n");
	len += sprintf(buf + len, "\tlnb_sel [val]\n");
	len += sprintf(buf + len, "\tdiseqc_reg\n");
	len += sprintf(buf + len, "\tdiseqc_dbg [val]\n");
	len += sprintf(buf + len, "\tdiseqc_sts\n");
	len += sprintf(buf + len, "\tdiseqc_cmd [val]\n");
	len += sprintf(buf + len, "\tdiseqc_burston [val]\n");
	len += sprintf(buf + len, "\tdiseqc_burstsa\n");
	len += sprintf(buf + len, "\tdiseqc_burstsb\n");
	len += sprintf(buf + len, "\tdiseqc_toneon [val]\n");
	len += sprintf(buf + len, "\tcapture_once /data/hcap_XXX.bin [mode]\n");
	len += sprintf(buf + len, "\t\tmode: 0 - others adc(default); 3 - ts, 4 - t/t2 adc; 5 - s/s2 adc.\n");

	return len;
}

static ssize_t dtmb_para_show(struct class *cls,
			      struct class_attribute *attr, char *buf)
{
	int snr = 0, lock_status = 0, bch = 0, agc_if_gain[3] = { 0 };
	int strength = 0;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();
	struct aml_dtvdemod *demod = NULL, *tmp = NULL;

	list_for_each_entry(tmp, &devp->demod_list, list) {
		if (tmp->id == 0) {
			demod = tmp;
			break;
		}
	}

	if (!demod)
		return 0;

	if (dtmb_mode == DTMB_READ_STRENGTH) {
		strength = tuner_get_ch_power(&demod->frontend);
		if (strength <= -56) {
			dtmb_read_agc(DTMB_D9_IF_GAIN, &agc_if_gain[0]);
			strength = dtmb_get_power_strength(agc_if_gain[0]);
		}
		return sprintf(buf, "strength is %d\n", strength);
	} else if (dtmb_mode == DTMB_READ_SNR) {
		/*snr = dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR) & 0x3fff;*/
		snr = dtmb_reg_r_che_snr();
		snr = convert_snr(snr);
		return sprintf(buf, "snr is %d\n", snr);
	} else if (dtmb_mode == DTMB_READ_LOCK) {
		lock_status = dtmb_reg_r_fec_lock();
		return sprintf(buf, "lock_status is %d\n", lock_status);
	} else if (dtmb_mode == DTMB_READ_BCH) {
		bch = dtmb_reg_r_bch();
		return sprintf(buf, "bch is %d\n", bch);
	} else {
		return sprintf(buf, "dtmb_para_show can't match mode\n");
	}

	return 0;
}

static ssize_t dtmb_para_store(struct class *cls, struct class_attribute *attr,
		const char *buf, size_t count)
{
	if (buf[0] == '0')
		dtmb_mode = DTMB_READ_STRENGTH;
	else if (buf[0] == '1')
		dtmb_mode = DTMB_READ_SNR;
	else if (buf[0] == '2')
		dtmb_mode = DTMB_READ_LOCK;
	else if (buf[0] == '3')
		dtmb_mode = DTMB_READ_BCH;
	else
		PR_INFO("invalid command.\n");

	return count;
}

static ssize_t atsc_para_show(struct class *cls,
		struct class_attribute *attr, char *buf)
{
	int snr, lock_status;
	unsigned int ser, ck;
	int strength = 0;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();
	struct aml_dtvdemod *demod = NULL, *tmp = NULL;

	list_for_each_entry(tmp, &devp->demod_list, list) {
		if (tmp->id == 0) {
			demod = tmp;
			break;
		}
	}

	if (!demod)
		return 0;

	if (demod->atsc_mode == QAM_64 || demod->atsc_mode == QAM_256 ||
		demod->atsc_mode == QAM_AUTO) {
		if (atsc_mode_para == ATSC_READ_STRENGTH) {
			strength = tuner_get_ch_power(&demod->frontend);
			return sprintf(buf, "strength is %d\n", strength);
		} else if (atsc_mode_para == ATSC_READ_SER) {
			ser = (unsigned int)dvbc_get_per(demod);
			return sprintf(buf, "ser is %d\n", ser);
		} else if (atsc_mode_para == ATSC_READ_FREQ) {
			return sprintf(buf, "freq is %d\n", demod->freq);
		} else {
			return sprintf(buf, "atsc_para_shows can't match mode\n");
		}
	} else if (demod->atsc_mode == VSB_8) {
		if (atsc_mode_para == ATSC_READ_STRENGTH) {
			strength = tuner_get_ch_power(&demod->frontend);
			return sprintf(buf, "strength is %d\n", strength);
		} else if (atsc_mode_para == ATSC_READ_SNR) {
			snr = atsc_read_snr();
			return sprintf(buf, "snr is %d\n", snr);
		} else if (atsc_mode_para == ATSC_READ_LOCK) {
			lock_status = atsc_read_reg(0x0980);
			return sprintf(buf, "lock_status is %x\n", lock_status);
		} else if (atsc_mode_para == ATSC_READ_SER) {
			ser = atsc_read_ser();
			return sprintf(buf, "ser is %d\n", ser);
		} else if (atsc_mode_para == ATSC_READ_FREQ) {
			return sprintf(buf, "freq is %d\n", demod->freq);
		} else if (atsc_mode_para == ATSC_READ_CK) {
			ck = atsc_read_ck();
			return sprintf(buf, "ck=0x%x lock=%d\n", ck, demod->last_status);
		} else {
			return sprintf(buf, "atsc_para_shows can't match mode\n");
		}
	} else {
		return sprintf(buf, "atsc_para_shows can't match mode.\n");
	}

	return 0;
}

static ssize_t atsc_para_store(struct class *cls, struct class_attribute *attr,
		const char *buf, size_t count)
{
	if (buf[0] == '0')
		atsc_mode_para = ATSC_READ_STRENGTH;
	else if (buf[0] == '1')
		atsc_mode_para = ATSC_READ_SNR;
	else if (buf[0] == '2')
		atsc_mode_para = ATSC_READ_LOCK;
	else if (buf[0] == '3')
		atsc_mode_para = ATSC_READ_SER;
	else if (buf[0] == '4')
		atsc_mode_para = ATSC_READ_FREQ;
	else if (buf[0] == '5')
		atsc_mode_para = ATSC_READ_CK;
	else
		PR_INFO("invalid command.\n");

	return count;
}

static ssize_t diseq_cmd_store(struct class *cla, struct class_attribute *attr,
		const char *bu, size_t count)
{
	int tmpbuf[20] = {};
	int i;
	int cnt;
	struct dvb_diseqc_master_cmd cmd;
	/*int ret;*/
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();
	struct aml_dtvdemod *demod = NULL, *tmp = NULL;
	struct dvb_frontend *fe;

	if (unlikely(!devp)) {
		PR_ERR("%s:devp is NULL\n", __func__);
		return -1;
	}

	list_for_each_entry(tmp, &devp->demod_list, list) {
		if (tmp->id == 0) {
			demod = tmp;
			fe = &demod->frontend;
			break;
		}
	}

	if (unlikely(!demod)) {
		PR_ERR("%s: demod is NULL\n", __func__);
		return -1;
	}

	cnt = sscanf(bu, "%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",
		    &tmpbuf[0], &tmpbuf[1], &tmpbuf[2], &tmpbuf[3],
		    &tmpbuf[4], &tmpbuf[5], &tmpbuf[6], &tmpbuf[7],
		    &tmpbuf[8], &tmpbuf[9], &tmpbuf[10], &tmpbuf[11],
		    &tmpbuf[12], &tmpbuf[13], &tmpbuf[14], &tmpbuf[15]);
	if (cnt < 0)
		return -EINVAL;
	if (cnt > 6)
		cnt = 6;

	for (i = 0; i < cnt; i++) {
		cmd.msg[i] = (char)tmpbuf[i];
		PR_INFO(" 0x%x\n", cmd.msg[i]);
	}
	cmd.msg_len = cnt;
	/* send diseqc msg */
	aml_diseqc_send_master_cmd(fe, &cmd);

	return count;
}

static ssize_t diseq_cmd_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "diseq_cmd debug interface\n");
}

static CLASS_ATTR_RW(diseq_cmd);
static CLASS_ATTR_RW(attr);
static CLASS_ATTR_RW(dtmb_para);
static CLASS_ATTR_RW(atsc_para);

int dtvdemod_create_class_files(struct class *clsp)
{
	int ret = 0;

	ret = class_create_file(clsp, &class_attr_dtmb_para);
	ret |= class_create_file(clsp, &class_attr_atsc_para);
	ret |= class_create_file(clsp, &class_attr_attr);
	ret |= class_create_file(clsp, &class_attr_diseq_cmd);

	return ret;
}

void dtvdemod_remove_class_files(struct class *clsp)
{
	class_remove_file(clsp, &class_attr_dtmb_para);
	class_remove_file(clsp, &class_attr_atsc_para);
	class_remove_file(clsp, &class_attr_attr);
	class_remove_file(clsp, &class_attr_diseq_cmd);
}

void aml_demod_dbg_init(void)
{
	struct dentry *root_entry = dtvdemod_get_dev()->demod_root;
	struct dentry *entry;
	unsigned int i;

	root_entry = debugfs_create_dir("demod", NULL);
	if (!root_entry) {
		PR_INFO("Can't create debugfs dir frontend.\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(demod_debug_files); i++) {
		entry = debugfs_create_file(demod_debug_files[i].name,
			demod_debug_files[i].mode,
			root_entry, NULL,
			demod_debug_files[i].fops);
		if (!entry)
			PR_INFO("Can't create debugfs seq file.\n");
	}
}

void aml_demod_dbg_exit(void)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();
	struct dentry *root_entry;

	if (unlikely(!devp)) {
		PR_ERR("%s:devp is NULL\n", __func__);
		return;
	}

	root_entry = devp->demod_root;

	if (root_entry)
		debugfs_remove_recursive(root_entry);
}

#if IS_ENABLED(CONFIG_AMLOGIC_DMC_DEV_ACCESS)
static int demod_dmc_dev_access_notify(struct notifier_block *nb, unsigned long id, void *data)
{
	struct dmc_dev_access_data *dmc = (struct dmc_dev_access_data *)data;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();
	struct aml_dtvdemod *demod = NULL, *tmp = NULL;

	if (unlikely(!devp)) {
		PR_ERR("\n\n\n%s: devp is NULL, demod is not load!!!\n", __func__);
		PR_ERR("%s: devp is NULL, demod is not load!!!\n", __func__);
		PR_ERR("%s: devp is NULL, demod is not load!!!\n\n\n", __func__);

		return 0;
	}

	list_for_each_entry(tmp, &devp->demod_list, list) {
		if (tmp->id == 0) {
			demod = tmp;
			break;
		}
	}

	if (!demod || demod->last_delsys == SYS_UNDEFINED) {
		PR_ERR("\n\n\n%s: demod is not work, please scan and channel!!!\n", __func__);
		PR_ERR("%s: demod is not work, please scan and channel!!!\n", __func__);
		PR_ERR("%s: demod is not work, please scan and channel!!!\n\n\n", __func__);

		return 0;
	}

	if (demod_dmc_id == id) {
		PR_ERR("%s: id (%ld) == demod_dmc_id (%ld).\n", __func__, id, demod_dmc_id);

		demod_ddr_addr = dmc->addr;
		demod_ddr_size = dmc->size;

		capture_adc_data_once("/data/hdcp_dump.bin", 3, 0);

		demod_ddr_addr = 0;
		demod_ddr_size = 0;
	}

	return 0;
}

static struct notifier_block demod_dmc_dev_access_nb = {
	.notifier_call = demod_dmc_dev_access_notify,
};
#endif

void demod_dmc_notifier(void)
{
	demod_dmc_id = 0;

#if IS_ENABLED(CONFIG_AMLOGIC_DMC_DEV_ACCESS)
	if (is_meson_t5w_cpu())
		demod_dmc_id = register_dmc_dev_access_notifier("DEVICE0",
				&demod_dmc_dev_access_nb);
	else if (is_meson_t3_cpu())
		demod_dmc_id = register_dmc_dev_access_notifier("DEMOD",
				&demod_dmc_dev_access_nb);
	else
		demod_dmc_id = register_dmc_dev_access_notifier("DEVICE",
				&demod_dmc_dev_access_nb);
#endif
}
