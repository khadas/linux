/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2018 Amlogic or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/
#define pr_fmt(fmt) "AM_ADAP: " fmt

#include "system_am_adap.h"
#include <linux/irqreturn.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/ioport.h>
#include <linux/of_platform.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/dma-contiguous.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of_fdt.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/kfifo.h>
#include <linux/completion.h>
#include <linux/jiffies.h>


#define AM_ADAPTER_NAME "amlogic, isp-adapter"
#define BOUNDRY 16

struct am_adap *g_adap = NULL;
struct am_adap_info para;

struct kfifo adapt_fifo;

/*we allocte from CMA*/
static uint8_t *isp_cma_mem = NULL;
static struct page *cma_pages = NULL;
static resource_size_t buffer_start;

#define DDR_BUF_SIZE 4
static resource_size_t ddr_buf[DDR_BUF_SIZE];

#define DOL_BUF_SIZE 6
static resource_size_t dol_buf[DOL_BUF_SIZE];

#define DEFAULT_ADAPTER_BUFFER_SIZE 24

static int dump_width;
static int dump_height;
static int dump_flag;
static int dump_cur_flag;
static int dump_buf_index;
static int irq_count;
static int cur_buf_index;
static int current_flag;
static int control_flag;
static int wbuf_index;

static int fte1_index;
static int fte0_index;
static int buffer_index;

static int dump_dol_frame;
static int fte_state;

static struct completion wakeupdump;
static unsigned int data_process_para;
static unsigned int frontend1_flag;

module_param(data_process_para, uint, 0664);
MODULE_PARM_DESC(data_process_para, "\n control inject or dump data parameter from adapter\n");

static int ceil_upper(int val, int mod)
{
	int ret = 0;
	if ((val == 0) || (mod == 0)) {
		pr_info("input a invalid value.\n");
		return 0;
	} else {
		if ((val % mod) == 0) {
			ret = (val/mod);
		} else {
			ret = ((val/mod) + 1);
		}
	}
	return ret;
}

static void parse_param(char *buf_orig, char **parm, int num)
{
	char *ps, *token;
	unsigned int n = 0;
	char delim1[2] = " ";

	ps = buf_orig;

	token = strsep(&ps, delim1);
	while (token != NULL) {
		if (*token != '\0') {
			parm[n++] = token;
		}
		if (n > num - 1) {
			printk("string element larger than array.\n");
			break;
		}
		token = strsep(&ps, delim1);
	}
}

static long getulfromstr(char* input)
{
	long out = 0;
	if (!input)
		return -1;
	if (kstrtoul(input, 10, &out) < 0) {
		return -1;
	}
	return out;
}

int write_index_to_file(char *path, char *buf, int index, int size)
{
	int ret = 0;
	struct file *fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	int nwrite = 0;
	int offset = 0;
	char file_name[150];

	sprintf(file_name, "%s%s%d%s", path, "adapt_img_", index, ".raw");
	/* change to KERNEL_DS address limit */
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	/* open file to write */
	fp = filp_open(file_name, O_WRONLY|O_CREAT, 0640);
	if (!fp) {
	   printk("%s: open file error\n", __FUNCTION__);
	   ret = -1;
	   goto exit;
	}

	pos=(unsigned long)offset;

	/* Write buf to file */
	nwrite=vfs_write(fp, buf, size, &pos);
	offset +=nwrite;

	if (fp) {
		filp_close(fp, NULL);
	}

exit:
	set_fs(old_fs);
	return ret;
}

static inline void update_wr_reg_bits(unsigned int reg,
				adap_io_type_t io_type, unsigned int mask,
				unsigned int val)
{
	unsigned int tmp, orig;
	void __iomem *base = NULL;
	switch (io_type) {
		case FRONTEND_IO:
			base = g_adap->base_addr + FRONTEND_BASE;
			break;
		case RD_IO:
			base = g_adap->base_addr + RD_BASE;
			break;
		case PIXEL_IO:
			base = g_adap->base_addr + PIXEL_BASE;
			break;
		case ALIGN_IO:
			base = g_adap->base_addr + ALIGN_BASE;
			break;
		case MISC_IO:
			base = g_adap->base_addr + MISC_BASE;
			break;
		default:
			pr_err("adapter error io type.\n");
			base = NULL;
			break;
	}
	if (base !=  NULL) {
		orig = readl(base + reg);
		tmp = orig & ~mask;
		tmp |= val & mask;
		writel(tmp, base + reg);
	}
}

static inline void adap_wr_reg_bits(unsigned int adr,
	    adap_io_type_t io_type, unsigned int val,
		unsigned int start, unsigned int len)
{
	update_wr_reg_bits(adr, io_type,
		           ((1<<len)-1)<<start, val<<start);
}

static inline void mipi_adap_reg_wr(int addr, adap_io_type_t io_type, uint32_t val)
{
	void __iomem *base_reg_addr = NULL;
	void __iomem *reg_addr = NULL;
	switch (io_type) {
		case FRONTEND_IO:
			base_reg_addr = g_adap->base_addr + FRONTEND_BASE;
			break;
		case RD_IO:
			base_reg_addr = g_adap->base_addr + RD_BASE;
			break;
		case PIXEL_IO:
			base_reg_addr = g_adap->base_addr + PIXEL_BASE;
			break;
		case ALIGN_IO:
			base_reg_addr = g_adap->base_addr + ALIGN_BASE;
			break;
		case MISC_IO:
			base_reg_addr = g_adap->base_addr + MISC_BASE;
			break;
		default:
			pr_err("adapter error io type.\n");
			base_reg_addr = NULL;
			break;
	}
	if (base_reg_addr != NULL) {
		reg_addr = base_reg_addr + addr;
		writel(val, reg_addr);
	} else
		pr_err("mipi adapter write register failed.\n");

}

static inline void mipi_adap_reg_rd(int addr, adap_io_type_t io_type, uint32_t *val)
{
	void __iomem *base_reg_addr = NULL;
	void __iomem *reg_addr = NULL;
	switch (io_type) {
		case FRONTEND_IO:
			base_reg_addr = g_adap->base_addr + FRONTEND_BASE;
			break;
		case RD_IO:
			base_reg_addr = g_adap->base_addr + RD_BASE;
			break;
		case PIXEL_IO:
			base_reg_addr = g_adap->base_addr + PIXEL_BASE;
			break;
		case ALIGN_IO:
			base_reg_addr = g_adap->base_addr + ALIGN_BASE;
			break;
		case MISC_IO:
			base_reg_addr = g_adap->base_addr + MISC_BASE;
			break;
		default:
			pr_err("%s, adapter error io type.\n", __func__);
			base_reg_addr = NULL;
			break;
	}
	if (base_reg_addr != NULL) {
		reg_addr = base_reg_addr + addr;
		*val = readl(reg_addr);
	} else
		pr_err("mipi adapter read register failed.\n");

}

static ssize_t adapt_frame_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("adapt-read.\n");
	uint8_t buf1[100];

	return sprintf(buf1,"dump flag:%d", 0);
}


static ssize_t adapt_frame_write(struct device *dev,
	struct device_attribute *attr, char const *buf, size_t size)
{
	long val = 0;
	ssize_t ret = size;
	char *virt_buf = NULL;
	int depth;
	uint32_t stride;
	uint32_t frame_size;
	char *buf_orig, *parm[10] = {NULL};
	unsigned int frame_index = 0;
	resource_size_t dump_buf_addr;

	if (!buf)
		return ret;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return ret;

	parse_param(buf_orig, (char **)&parm, sizeof(parm)/sizeof(parm[0]));

	if (!parm[0]) {
		ret = -EINVAL;
		goto Err;
	}

	if (!parm[1] || (kstrtoul(parm[1], 10, &val) < 0)) {
		ret = -EINVAL;
		goto Err;
	} else {
		dump_cur_flag = val;
	}

	if (!parm[2] || (kstrtoul(parm[2], 10, &val) < 0)) {
		ret = -EINVAL;
		goto Err;
	} else {
		cur_buf_index = val;
		if (cur_buf_index >= DDR_BUF_SIZE) {
			pr_info("dump current index is invalid.\n");
			ret = -EINVAL;
			goto Err;
		}
	}

	depth = am_adap_get_depth();
	stride = ((dump_width * depth)/8);
	stride = ((stride + (BOUNDRY - 1)) & (~(BOUNDRY - 1)));
	frame_index = ((data_process_para) & (0xfffffff));
	frame_size = stride * dump_height;
	pr_info("dump width = %d, height = %d, size = %d\n",
		dump_width, dump_height, frame_size);

	if (dump_cur_flag) {
		dump_buf_addr = ddr_buf[cur_buf_index];
		pr_info("dump current buffer index %d.\n", cur_buf_index);
		if (dump_buf_addr)
			virt_buf = phys_to_virt(dump_buf_addr);
		write_index_to_file(parm[0], virt_buf, cur_buf_index, frame_size);
		dump_cur_flag = 0;
		current_flag = 0;
	} else if (frame_index > 0) {
		pr_info("dump the buf_index = %d\n", dump_buf_index);
		dump_buf_addr = ddr_buf[dump_buf_index];
		if (dump_buf_addr)
			virt_buf = phys_to_virt(dump_buf_addr);
		write_index_to_file(parm[0], virt_buf, dump_buf_index, frame_size);
		dump_flag = 0;
	} else {
		pr_info("No match condition to dump file.\n");
	}

Err:
	kfree(buf_orig);
	return ret;

}

static DEVICE_ATTR(adapt_frame, S_IRUGO | S_IWUSR, adapt_frame_read, adapt_frame_write);

static ssize_t dol_frame_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("adapt-read.\n");
	uint8_t buf1[100];

	return sprintf(buf1,"dump flag:%d", 0);
}

static ssize_t dol_frame_write(struct device *dev,
	struct device_attribute *attr, char const *buf, size_t size)
{
	ssize_t ret = size;
	char *virt_buf = NULL;
	int depth;
	uint32_t stride;
	uint32_t frame_size;
	char *buf_orig, *parm[10] = {NULL};
	resource_size_t dump_buf_addr;

	if (!buf)
		return ret;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return ret;

	parse_param(buf_orig, (char **)&parm, sizeof(parm)/sizeof(parm[0]));

	if (!parm[0]) {
		ret = -EINVAL;
		goto Err;
	}

	depth = am_adap_get_depth();
	stride = ((dump_width * depth)/8);
	stride = ((stride + (BOUNDRY - 1)) & (~(BOUNDRY - 1)));
	frame_size = stride * dump_height;
	pr_info("dump width = %d, height = %d, size = %d\n",
		dump_width, dump_height, frame_size);

	dump_dol_frame = 1;

	if (!wait_for_completion_timeout(&wakeupdump, msecs_to_jiffies(100))) {
		pr_err("wait for same frame timeout.\n");
		dump_dol_frame = 0;
		return ret;
	}

	dump_buf_addr = dol_buf[(buffer_index - 1) % 2];
	pr_info("dump ft0/ft1 buffer index %d.\n", buffer_index);
	if (dump_buf_addr)
		virt_buf = phys_to_virt(dump_buf_addr);
	write_index_to_file(parm[0], virt_buf, 0, frame_size);

	dump_buf_addr = dol_buf[(buffer_index - 1)% 2 + 2];
	if (dump_buf_addr)
		virt_buf = phys_to_virt(dump_buf_addr);
	write_index_to_file(parm[0], virt_buf, 1, frame_size);

	dump_dol_frame = 0;
	if (buffer_index % 2 == 1) {
		adap_wr_reg_bits(CSI2_DDR_START_PIX, FRONTEND_IO, dol_buf[0], 0, 32);
		adap_wr_reg_bits(CSI2_DDR_START_PIX + FTE1_OFFSET, FRONTEND_IO, dol_buf[2], 0, 32);
	} else {
		adap_wr_reg_bits(CSI2_DDR_START_PIX_ALT, FRONTEND_IO, dol_buf[1], 0, 32);
		adap_wr_reg_bits(CSI2_DDR_START_PIX_ALT + FTE1_OFFSET, FRONTEND_IO, dol_buf[3], 0, 32);
	}

Err:
	kfree(buf_orig);
	return ret;

}

static DEVICE_ATTR(dol_frame, S_IRUGO | S_IWUSR, dol_frame_read, dol_frame_write);

static int write_data_to_buf(char *path, char *buf, int size)
{
	int ret = 0;
	struct file *fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	unsigned int r_size = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	fp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_info("read error.\n");
		return -1;
	}
	r_size = vfs_read(fp, buf, size, &pos);
	pr_info("read r_size = %u, size = %u\n", r_size, size);

	vfs_fsync(fp, 0);
	filp_close(fp, NULL);
	set_fs(old_fs);

	return ret;
}

static const char *adapt_inject_usage_str = {
	"Usage:\n"
	"echo <src_path> <width> <height> <bit_depth> > /sys/devices/platform/ff650000.isp-adapter/inject_frame\n"
};

static ssize_t err_note(void) {
	uint8_t buf1[128];
	return sprintf(buf1, "%s\n", adapt_inject_usage_str);
}

static ssize_t inject_frame_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", adapt_inject_usage_str);
}


static ssize_t inject_frame_write(struct device *dev,
	struct device_attribute *attr, char const *buf, size_t size)
{
	char *buf_orig, *parm[100] = {NULL};
	long stride = 0;
	long frame_width = 0;
	long frame_height = 0;
	long bit_depth = 0;
	char *virt_buf = NULL;
	uint32_t file_size;
	ssize_t ret = size;

	if (!buf)
		return ret;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return ret;

	parse_param(buf_orig, (char **)&parm, sizeof(parm)/sizeof(parm[0]));

	if (!parm[0]) {
		ret = -EINVAL;
		pr_err("error---path--->:%s\n", adapt_inject_usage_str);
		goto Err;
	}

	pr_info("file_path = %s\n", parm[0]);

	frame_width = getulfromstr(parm[1]);
	frame_height = getulfromstr(parm[2]);
	bit_depth = getulfromstr(parm[3]);
	if (frame_width < 0 || frame_height < 0 || bit_depth < 0) {
		ret = -EINVAL;
		goto Err;
	}

	stride = (frame_width * bit_depth)/8;
	stride = ((stride + (BOUNDRY - 1)) & (~(BOUNDRY - 1)));
	if (ddr_buf[DDR_BUF_SIZE - 1] != 0)
		virt_buf = phys_to_virt(ddr_buf[DDR_BUF_SIZE - 1]);
	file_size = stride * frame_height;
	pr_info("inject frame width = %ld, height = %ld, bitdepth = %ld, size = %d\n",
		frame_width, frame_height,
		bit_depth, file_size);
	write_data_to_buf(parm[0], virt_buf, file_size);

Err:
	if (ret < 0)
		err_note();
	kfree(buf_orig);
	return ret;
}

static DEVICE_ATTR(inject_frame, S_IRUGO | S_IWUSR, inject_frame_read, inject_frame_write);

int am_adap_parse_dt(struct device_node *node)
{
	int rtn = -1;
	int irq = -1;
	int ret = 0;
	struct resource rs;
	struct am_adap *t_adap = NULL;

	if (node == NULL) {
		pr_err("%s: Error input param\n", __func__);
		return -1;
	}

	rtn = of_device_is_compatible(node, AM_ADAPTER_NAME);
	if (rtn == 0) {
		pr_err("%s: Error match compatible\n", __func__);
		return -1;
	}

	t_adap = kzalloc(sizeof(*t_adap), GFP_KERNEL);
	if (t_adap == NULL) {
		pr_err("%s: Failed to alloc adapter\n", __func__);
		return -1;
	}

	t_adap->of_node = node;

	rtn = of_address_to_resource(node, 0, &rs);
	if (rtn != 0) {
		pr_err("%s:Error get adap reg resource\n", __func__);
		goto reg_error;
	}

	pr_info("%s: rs idx info: name: %s\n", __func__, rs.name);
	if (strcmp(rs.name, "adapter") == 0) {
		t_adap->reg = rs;
		t_adap->base_addr =
				ioremap_nocache(t_adap->reg.start, resource_size(&t_adap->reg));
	}

	irq = irq_of_parse_and_map(node, 0);
	if (irq <= 0) {
		pr_err("%s:Error get adap irq\n", __func__);
		goto irq_error;
	}

	t_adap->rd_irq = irq;
	pr_info("%s:rs info: irq: %d\n", __func__, t_adap->rd_irq);

	t_adap->p_dev = of_find_device_by_node(node);
	ret = of_reserved_mem_device_init(&(t_adap->p_dev->dev));
	if (ret != 0) {
		pr_err("adapt reserved mem device init failed.\n");
		return ret;
	}

	ret = of_property_read_u32(t_adap->p_dev->dev.of_node, "mem_alloc",
		&(t_adap->adap_buf_size));
	pr_info("adapter alloc %dM memory\n", t_adap->adap_buf_size);
	if (ret != 0) {
		pr_err("failed to get adap-buf-size from dts, use default value\n");
		t_adap->adap_buf_size = DEFAULT_ADAPTER_BUFFER_SIZE;
	}

	device_create_file(&(t_adap->p_dev->dev), &dev_attr_adapt_frame);
	device_create_file(&(t_adap->p_dev->dev), &dev_attr_inject_frame);
	device_create_file(&(t_adap->p_dev->dev), &dev_attr_dol_frame);

	g_adap = t_adap;

	return 0;

irq_error:
	iounmap(t_adap->base_addr);
	t_adap->base_addr = NULL;


reg_error:
	if (t_adap != NULL)
		kfree(t_adap);
	return -1;
}

void am_adap_deinit_parse_dt(void)
{
	struct am_adap *t_adap = NULL;

	t_adap = g_adap;

	if (t_adap == NULL || t_adap->p_dev == NULL ||
				t_adap->base_addr == NULL) {
		pr_err("Error input param\n");
		return;
	}

	device_remove_file(&(t_adap->p_dev->dev), &dev_attr_adapt_frame);
	device_remove_file(&(t_adap->p_dev->dev), &dev_attr_inject_frame);
	device_remove_file(&(t_adap->p_dev->dev), &dev_attr_dol_frame);

	iounmap(t_adap->base_addr);
	t_adap->base_addr = NULL;

	kfree(t_adap);
	t_adap = NULL;
	g_adap = NULL;

	pr_info("Success deinit parse adap module\n");
}


void am_adap_set_info(struct am_adap_info *info)
{
	int inject_data_flag;
	int dump_data_flag;

	memset(&para, 0, sizeof(struct am_adap_info));
	memcpy(&para, info, sizeof(struct am_adap_info));
	frontend1_flag = ((data_process_para >> 30) & 0x1);
	inject_data_flag = ((data_process_para >> 29) & 0x1);
	dump_data_flag = ((data_process_para >> 28) & 0x1);

	pr_info("inject_data_flag = %x, dump_data_flag = %x\n",
		 inject_data_flag, dump_data_flag);

	if ((inject_data_flag) || (dump_data_flag)) {
		para.mode = DDR_MODE;
	}
	dump_width = para.img.width;
	dump_height = para.img.height;
}

int get_fte1_flag(void)
{
	frontend1_flag = (data_process_para >> 30) & 0x1;
	return (data_process_para >> 30) & 0x1;
}

int am_adap_get_depth(void)
{
	int depth = 0;
	switch (para.fmt) {
		case AM_RAW6:
			depth = 6;
			break;
		case AM_RAW7:
			depth = 7;
			break;
		case AM_RAW8:
			depth = 8;
			break;
		case AM_RAW10:
			depth = 10;
			break;
		case AM_RAW12:
			depth = 12;
			break;
		case AM_RAW14:
			depth = 14;
			break;
		default:
			pr_err("Not supported data format.\n");
			break;
	}
	return depth;
}

int am_disable_irq(void)
{
	//disable irq mask
	mipi_adap_reg_wr(CSI2_INTERRUPT_CTRL_STAT, FRONTEND_IO, 0xffffffff);
	mipi_adap_reg_wr(MIPI_ADAPT_IRQ_MASK0, ALIGN_IO, 0xffffffff);

	return 0;
}

/*
 *========================AM ADAPTER FRONTEND INTERFACE========================
 */

void am_adap_frontend_start(void)
{
	int width = para.img.width;
	int depth, val;
	depth = am_adap_get_depth();
	if (!depth)
		pr_err("is not supported data format.");
	adap_wr_reg_bits(CSI2_GEN_CTRL0, FRONTEND_IO, 1, 0, 1);

	if (frontend1_flag) {
		adap_wr_reg_bits(CSI2_GEN_CTRL0 + FTE1_OFFSET, FRONTEND_IO, 1, 0, 1);
	}

	val = ceil_upper((width * depth), (8 * 16));
	pr_info("frontend : width = %d, val = %d\n", width, val);
	adap_wr_reg_bits(CSI2_DDR_STRIDE_PIX, FRONTEND_IO, val, 4, 28);

	if (frontend1_flag) {
		adap_wr_reg_bits(CSI2_DDR_STRIDE_PIX + FTE1_OFFSET, FRONTEND_IO, val, 4, 28);
	}
}

int am_adap_frontend_init(void)
{
	int long_exp_offset = para.offset.long_offset;
	int short_exp_offset = para.offset.short_offset;
	mipi_adap_reg_wr(CSI2_CLK_RESET, FRONTEND_IO, 0x7);//release from reset
	mipi_adap_reg_wr(CSI2_CLK_RESET, FRONTEND_IO, 0x6);//enable frontend module clock and disable auto clock gating
	mipi_adap_reg_wr(CSI2_CLK_RESET, FRONTEND_IO, 0x6);

	if (frontend1_flag) {
		mipi_adap_reg_wr(CSI2_CLK_RESET + FTE1_OFFSET, FRONTEND_IO, 0x7);//release from reset
		mipi_adap_reg_wr(CSI2_CLK_RESET + FTE1_OFFSET, FRONTEND_IO, 0x6);//enable frontend module clock and disable auto clock gating
		mipi_adap_reg_wr(CSI2_CLK_RESET + FTE1_OFFSET, FRONTEND_IO, 0x6);
	}

	if (para.mode == DIR_MODE) {
		if (para.path == PATH0)
			mipi_adap_reg_wr(CSI2_GEN_CTRL0, FRONTEND_IO, 0x001f0001);//bit[0] 1:enable virtual channel 0
	} else if (para.mode == DDR_MODE) {
		if (para.path == PATH0)
			mipi_adap_reg_wr(CSI2_GEN_CTRL0, FRONTEND_IO, 0x001f001f);
	} else if (para.mode == DOL_MODE) {
		mipi_adap_reg_wr(CSI2_GEN_CTRL0, FRONTEND_IO, 0x001f10a3);

		if (frontend1_flag) {
				mipi_adap_reg_wr(CSI2_GEN_CTRL0 + FTE1_OFFSET, FRONTEND_IO, 0x001f10a1);
		}
	} else {
		pr_err("%s, Not supported Mode.\n", __func__);
	}

	//applicable only to Raw data, direct MEM path
	mipi_adap_reg_wr(CSI2_X_START_END_MEM, FRONTEND_IO, 0xffff0000);
	mipi_adap_reg_wr(CSI2_Y_START_END_MEM, FRONTEND_IO, 0xffff0000);

	if (frontend1_flag) {
		mipi_adap_reg_wr(CSI2_X_START_END_MEM + FTE1_OFFSET, FRONTEND_IO, 0xffff0000);
		mipi_adap_reg_wr(CSI2_Y_START_END_MEM + FTE1_OFFSET, FRONTEND_IO, 0xffff0000);
	}
	if (para.mode == DOL_MODE) {
		if (para.type == DOL_VC) {                          //for ov
			mipi_adap_reg_wr(CSI2_VC_MODE, FRONTEND_IO, 0x11220040);
/*
			mipi_adap_reg_wr(CSI2_VC_MODE + FTE1_OFFSET, FRONTEND_IO, 0x11220040);
*/
		} else if (para.type == DOL_LINEINFO) {             //for sony
			mipi_adap_reg_wr(CSI2_VC_MODE, FRONTEND_IO, 0x11110052);
			mipi_adap_reg_wr(CSI2_VC_MODE2_MATCH_MASK_L, FRONTEND_IO, 0x6f6f6f6f);
			mipi_adap_reg_wr(CSI2_VC_MODE2_MATCH_MASK_H, FRONTEND_IO, 0xffffffcc);
			mipi_adap_reg_wr(CSI2_VC_MODE2_MATCH_TO_VC_L, FRONTEND_IO, 0x90909090);
			mipi_adap_reg_wr(CSI2_VC_MODE2_MATCH_TO_VC_H, FRONTEND_IO, 0x55);
			mipi_adap_reg_wr(CSI2_VC_MODE2_MATCH_TO_IGNORE_L, FRONTEND_IO, 0x80808080);
			mipi_adap_reg_wr(CSI2_VC_MODE2_MATCH_TO_IGNORE_H, FRONTEND_IO, 0x0);

			adap_wr_reg_bits(CSI2_X_START_END_MEM, FRONTEND_IO, 0xc, 0, 16);
			adap_wr_reg_bits(CSI2_X_START_END_MEM, FRONTEND_IO,
						0xc + para.img.width - 1, 16, 16);
			adap_wr_reg_bits(CSI2_Y_START_END_MEM, FRONTEND_IO,
						long_exp_offset, 0, 16);
			adap_wr_reg_bits(CSI2_Y_START_END_MEM, FRONTEND_IO,
						long_exp_offset + para.img.height - 1, 16, 16);
			//set short exposure offset
			adap_wr_reg_bits(CSI2_X_START_END_ISP, FRONTEND_IO, 0xc, 0, 16);
			adap_wr_reg_bits(CSI2_X_START_END_ISP, FRONTEND_IO,
						0xc + para.img.width - 1, 16, 16);
			adap_wr_reg_bits(CSI2_Y_START_END_ISP, FRONTEND_IO,
						short_exp_offset, 0, 16);
			adap_wr_reg_bits(CSI2_Y_START_END_ISP, FRONTEND_IO,
						short_exp_offset + para.img.height - 1, 16, 16);




			if (frontend1_flag) {
				mipi_adap_reg_wr(CSI2_VC_MODE + FTE1_OFFSET, FRONTEND_IO, 0x11110046);   //ft1 vc_mode
				mipi_adap_reg_wr(CSI2_VC_MODE2_MATCH_MASK_L + FTE1_OFFSET, FRONTEND_IO, 0x6f6f6f6f);
				mipi_adap_reg_wr(CSI2_VC_MODE2_MATCH_MASK_H + FTE1_OFFSET, FRONTEND_IO, 0xffffffcc);
				mipi_adap_reg_wr(CSI2_VC_MODE2_MATCH_TO_VC_L + FTE1_OFFSET, FRONTEND_IO, 0x90909090);

				mipi_adap_reg_wr(CSI2_VC_MODE2_MATCH_TO_VC_H + FTE1_OFFSET, FRONTEND_IO, 0x55);
				mipi_adap_reg_wr(CSI2_VC_MODE2_MATCH_TO_IGNORE_L + FTE1_OFFSET, FRONTEND_IO, 0x80808080);
				mipi_adap_reg_wr(CSI2_VC_MODE2_MATCH_TO_IGNORE_H + FTE1_OFFSET, FRONTEND_IO, 0x0);


				adap_wr_reg_bits(CSI2_X_START_END_MEM + FTE1_OFFSET, FRONTEND_IO, 12, 0, 16);
				adap_wr_reg_bits(CSI2_X_START_END_MEM + FTE1_OFFSET, FRONTEND_IO,
							12 + para.img.width - 1, 16, 16);
				adap_wr_reg_bits(CSI2_Y_START_END_MEM + FTE1_OFFSET, FRONTEND_IO,
							short_exp_offset, 0, 16);
				adap_wr_reg_bits(CSI2_Y_START_END_MEM + FTE1_OFFSET, FRONTEND_IO,
							short_exp_offset + para.img.height - 1, 16, 16);
				//set short exposure offset
				adap_wr_reg_bits(CSI2_X_START_END_ISP + FTE1_OFFSET, FRONTEND_IO, 12, 0, 16);
				adap_wr_reg_bits(CSI2_X_START_END_ISP + FTE1_OFFSET, FRONTEND_IO,
							12 + para.img.width - 1, 16, 16);
				adap_wr_reg_bits(CSI2_Y_START_END_ISP + FTE1_OFFSET, FRONTEND_IO,
							long_exp_offset, 0, 16);
				adap_wr_reg_bits(CSI2_Y_START_END_ISP + FTE1_OFFSET, FRONTEND_IO,
							long_exp_offset + para.img.height - 1, 16, 16);
			}
		} else {
			pr_err("Not support DOL type\n");
		}
	}

	if (para.mode == DIR_MODE) {
		mipi_adap_reg_wr(CSI2_VC_MODE, FRONTEND_IO, 0x110000);
	}

	if (para.mode == DDR_MODE) {
		//config ddr_buf[0] address
		adap_wr_reg_bits(CSI2_DDR_START_PIX, FRONTEND_IO, ddr_buf[wbuf_index], 0, 32);
	} else if (para.mode == DOL_MODE) {
		adap_wr_reg_bits(CSI2_DDR_START_PIX, FRONTEND_IO, dol_buf[0], 0, 32);
		adap_wr_reg_bits(CSI2_DDR_START_PIX_ALT, FRONTEND_IO, dol_buf[1], 0, 32);

		if (frontend1_flag) {
				adap_wr_reg_bits(CSI2_DDR_START_PIX + FTE1_OFFSET, FRONTEND_IO, dol_buf[2], 0, 32);
				adap_wr_reg_bits(CSI2_DDR_START_PIX_ALT + FTE1_OFFSET, FRONTEND_IO, dol_buf[3], 0, 32);
		}
	}

	//enable vs_rise_isp interrupt & enable ddr_wdone interrupt
	mipi_adap_reg_wr(CSI2_INTERRUPT_CTRL_STAT, FRONTEND_IO, 0x5);

	if (frontend1_flag) {
		mipi_adap_reg_wr(CSI2_INTERRUPT_CTRL_STAT + FTE1_OFFSET, FRONTEND_IO, 0x5);
	}
	return 0;
}

/*
 *========================AM ADAPTER READER INTERFACE==========================
 */

void am_adap_reader_start(void)
{
	int height = para.img.height;
	int width = para.img.width;
	int val, depth;
	depth = am_adap_get_depth();
	val = ceil_upper((width * depth), (8 * 16));
	pr_info("reader : width = %d, val = %d\n", width, val);
	adap_wr_reg_bits(MIPI_ADAPT_DDR_RD0_CNTL1, RD_IO, height, 16, 13);
	adap_wr_reg_bits(MIPI_ADAPT_DDR_RD0_CNTL1, RD_IO, val, 0, 10);
	if (para.mode == DOL_MODE) {
		adap_wr_reg_bits(MIPI_ADAPT_DDR_RD1_CNTL1, RD_IO, height, 16, 13);
		adap_wr_reg_bits(MIPI_ADAPT_DDR_RD1_CNTL1, RD_IO, val, 0, 10);
	}
	adap_wr_reg_bits(MIPI_ADAPT_DDR_RD0_CNTL0, RD_IO, 1, 0, 1);
}

int am_adap_reader_init(void)
{
	if (para.mode == DIR_MODE) {
		mipi_adap_reg_wr(MIPI_ADAPT_DDR_RD0_CNTL1, RD_IO, 0x02d00078);
		mipi_adap_reg_wr(MIPI_ADAPT_DDR_RD0_CNTL0, RD_IO, 0xb5000005);
	} else if (para.mode == DDR_MODE) {
		mipi_adap_reg_wr(MIPI_ADAPT_DDR_RD0_CNTL1, RD_IO, 0x02d00078);
		adap_wr_reg_bits(MIPI_ADAPT_DDR_RD0_CNTL2, RD_IO, ddr_buf[wbuf_index], 0, 32);//ddr mode config frame address
		mipi_adap_reg_wr(MIPI_ADAPT_DDR_RD0_CNTL0, RD_IO, 0x70000001);
	} else if (para.mode == DOL_MODE) {
		mipi_adap_reg_wr(MIPI_ADAPT_DDR_RD0_CNTL1, RD_IO, 0x04380096);
		adap_wr_reg_bits(MIPI_ADAPT_DDR_RD0_CNTL2, RD_IO, dol_buf[0], 0, 32);
		adap_wr_reg_bits(MIPI_ADAPT_DDR_RD0_CNTL3, RD_IO, dol_buf[1], 0, 32);
		mipi_adap_reg_wr(MIPI_ADAPT_DDR_RD0_CNTL0, RD_IO, 0xb5800001);
		mipi_adap_reg_wr(MIPI_ADAPT_DDR_RD1_CNTL1, RD_IO, 0x04380096);
		adap_wr_reg_bits(MIPI_ADAPT_DDR_RD1_CNTL2, RD_IO, dol_buf[0], 0, 32);
		adap_wr_reg_bits(MIPI_ADAPT_DDR_RD1_CNTL3, RD_IO, dol_buf[1], 0, 32);
		mipi_adap_reg_wr(MIPI_ADAPT_DDR_RD1_CNTL0, RD_IO, 0xf1c10005);
	} else {
		pr_err("%s, Not supported Mode.\n", __func__);
	}

	return 0;
}


/*
 *========================AM ADAPTER PIXEL INTERFACE===========================
 */

void am_adap_pixel_start(void)
{
	int fmt = para.fmt;
	int width = para.img.width;
	adap_wr_reg_bits(MIPI_ADAPT_PIXEL0_CNTL0, PIXEL_IO, fmt, 13, 3);
	adap_wr_reg_bits(MIPI_ADAPT_PIXEL0_CNTL0, PIXEL_IO, width, 0, 13);
	if (para.mode == DOL_MODE) {
		adap_wr_reg_bits(MIPI_ADAPT_PIXEL1_CNTL0, PIXEL_IO, fmt, 13, 3);
		adap_wr_reg_bits(MIPI_ADAPT_PIXEL1_CNTL0, PIXEL_IO, width, 0, 13);
	}
	adap_wr_reg_bits(MIPI_ADAPT_PIXEL0_CNTL1, PIXEL_IO, 1, 31, 1);
}


int am_adap_pixel_init(void)
{
	if (para.mode == DIR_MODE) {
		//default width 1280
		mipi_adap_reg_wr(MIPI_ADAPT_PIXEL0_CNTL0, PIXEL_IO, 0x8000a500);
		mipi_adap_reg_wr(MIPI_ADAPT_PIXEL0_CNTL1, PIXEL_IO, 0x80000808);
	} else if (para.mode == DDR_MODE) {
		mipi_adap_reg_wr(MIPI_ADAPT_PIXEL0_CNTL0, PIXEL_IO, 0x0000a500);
		mipi_adap_reg_wr(MIPI_ADAPT_PIXEL0_CNTL1, PIXEL_IO, 0x80000008);
	} else if (para.mode == DOL_MODE) {
		mipi_adap_reg_wr(MIPI_ADAPT_PIXEL0_CNTL0, PIXEL_IO, 0x80008780);
		mipi_adap_reg_wr(MIPI_ADAPT_PIXEL0_CNTL1, PIXEL_IO, 0x80000008);
		mipi_adap_reg_wr(MIPI_ADAPT_PIXEL1_CNTL0, PIXEL_IO, 0x00008780);
		mipi_adap_reg_wr(MIPI_ADAPT_PIXEL1_CNTL1, PIXEL_IO, 0x80000008);
	} else {
		pr_err("%s, Not supported Mode.\n", __func__);
	}

	return 0;
}


/*
 *========================AM ADAPTER ALIGNMENT INTERFACE=======================
 */

void am_adap_alig_start(void)
{
	int width, height, alig_width, alig_height, val;
	width = para.img.width;
	height = para.img.height;
	alig_width = width + 40; //hblank > 32 cycles
	alig_height = height + 60; //vblank > 48 lines
	val = width + 35; // width < val < alig_width
	adap_wr_reg_bits(MIPI_ADAPT_ALIG_CNTL0, ALIGN_IO, alig_width, 0, 13);
	adap_wr_reg_bits(MIPI_ADAPT_ALIG_CNTL0, ALIGN_IO, alig_height, 16, 13);
	adap_wr_reg_bits(MIPI_ADAPT_ALIG_CNTL1, ALIGN_IO, width, 16, 13);
	adap_wr_reg_bits(MIPI_ADAPT_ALIG_CNTL2, ALIGN_IO, height, 16, 13);
	adap_wr_reg_bits(MIPI_ADAPT_ALIG_CNTL8, ALIGN_IO, val, 16, 13);
	adap_wr_reg_bits(MIPI_ADAPT_ALIG_CNTL8, ALIGN_IO, 1, 31, 1);
}


int am_adap_alig_init(void)
{
	if (para.mode == DOL_MODE) {
		mipi_adap_reg_wr(MIPI_ADAPT_ALIG_CNTL0, ALIGN_IO, 0x078a043a);//associate width and height
		mipi_adap_reg_wr(MIPI_ADAPT_ALIG_CNTL1, ALIGN_IO, 0x07800000);//associate width
		mipi_adap_reg_wr(MIPI_ADAPT_ALIG_CNTL2, ALIGN_IO, 0x04380000);//associate height
	} else {
		//default width 1280, height 720
		mipi_adap_reg_wr(MIPI_ADAPT_ALIG_CNTL0, ALIGN_IO, 0x02f80528);//associate width and height
		mipi_adap_reg_wr(MIPI_ADAPT_ALIG_CNTL1, ALIGN_IO, 0x05000000);//associate width
		mipi_adap_reg_wr(MIPI_ADAPT_ALIG_CNTL2, ALIGN_IO, 0x02d00000);//associate height
	}
	if (para.mode == DIR_MODE) {
		mipi_adap_reg_wr(MIPI_ADAPT_ALIG_CNTL6, ALIGN_IO, 0x00fff011);
		mipi_adap_reg_wr(MIPI_ADAPT_ALIG_CNTL7, ALIGN_IO, 0xc350c000);
		mipi_adap_reg_wr(MIPI_ADAPT_ALIG_CNTL8, ALIGN_IO, 0x85231020);
	} else if (para.mode == DDR_MODE) {
		mipi_adap_reg_wr(MIPI_ADAPT_ALIG_CNTL6, ALIGN_IO, 0x00fff001);
		mipi_adap_reg_wr(MIPI_ADAPT_ALIG_CNTL7, ALIGN_IO, 0x0);
		mipi_adap_reg_wr(MIPI_ADAPT_ALIG_CNTL8, ALIGN_IO, 0x80000020);
	} else if (para.mode == DOL_MODE) {
		mipi_adap_reg_wr(MIPI_ADAPT_ALIG_CNTL6, ALIGN_IO, 0x00ff541d);
		mipi_adap_reg_wr(MIPI_ADAPT_ALIG_CNTL7, ALIGN_IO, 0xffffe000);
		mipi_adap_reg_wr(MIPI_ADAPT_ALIG_CNTL8, ALIGN_IO, 0x87881020);
	} else {
		pr_err("Not supported mode.\n");
	}
	mipi_adap_reg_wr(MIPI_ADAPT_IRQ_MASK0, ALIGN_IO, 0x01082000);

	return 0;
}

/*
 *========================AM ADAPTER INTERFACE==========================
 */

static int get_next_wr_buf_index(int inject_flag)
{
	int index = 0;
	wbuf_index = wbuf_index + 1;

	if (inject_flag) {
		index = wbuf_index % (DDR_BUF_SIZE - 1);
	} else {
		if (dump_flag) {
			index = wbuf_index % DDR_BUF_SIZE;
			if (index == dump_buf_index) {
				wbuf_index = wbuf_index + 1;
				index = wbuf_index % DDR_BUF_SIZE;
			}
		} else if (current_flag){
			index = wbuf_index % DDR_BUF_SIZE;
			if (index == cur_buf_index) {
				wbuf_index = wbuf_index + 1;
				index = wbuf_index % DDR_BUF_SIZE;
			}
		} else {
			index = wbuf_index % DDR_BUF_SIZE;
		}
	}

	return index;
}

static resource_size_t read_buf;
static int next_buf_index;
static irqreturn_t adpapter_isr(int irq, void *para)
{
	uint32_t data = 0;
	resource_size_t val = 0;
	int kfifo_ret = 0;
	int inject_data_flag = ((data_process_para >> 29) & 0x1);
	int frame_index = ((data_process_para) & (0xfffffff));

	mipi_adap_reg_rd(MIPI_ADAPT_IRQ_PENDING0, ALIGN_IO, &data);

	if (data & (1 << 19)) {
		adap_wr_reg_bits(MIPI_ADAPT_IRQ_PENDING0, ALIGN_IO, 1, 19, 1); //clear write done irq
		if ((dump_cur_flag) && (next_buf_index == cur_buf_index)) {
			current_flag = 1;
		}
		if (!control_flag) {
			if (!kfifo_is_full(&adapt_fifo)) {
				kfifo_in(&adapt_fifo, &ddr_buf[next_buf_index], sizeof(resource_size_t));
				irq_count = irq_count + 1;
				if (irq_count == frame_index) {
					dump_buf_index = next_buf_index;
					dump_flag = 1;
				}
			} else {
				pr_info("adapt fifo is full .\n");
			}

			next_buf_index = get_next_wr_buf_index(inject_data_flag);
			val = ddr_buf[next_buf_index];
			adap_wr_reg_bits(CSI2_DDR_START_PIX, FRONTEND_IO, val, 0, 32);
		}
		if ((!control_flag) && (kfifo_len(&adapt_fifo) > 0)) {
			adap_wr_reg_bits(MIPI_ADAPT_DDR_RD0_CNTL0, RD_IO, 1, 31, 1);
			kfifo_ret = kfifo_out(&adapt_fifo, &val, sizeof(val));
			read_buf = val;
			control_flag = 1;
		}

	}

	if (data & (1 << 13)) {
		adap_wr_reg_bits(MIPI_ADAPT_IRQ_PENDING0, ALIGN_IO, 1, 13, 1);
		if (inject_data_flag) {
			adap_wr_reg_bits(MIPI_ADAPT_DDR_RD0_CNTL2, RD_IO, ddr_buf[DDR_BUF_SIZE - 1], 0, 32);
		} else {
			adap_wr_reg_bits(MIPI_ADAPT_DDR_RD0_CNTL2, RD_IO, read_buf, 0, 32);
		}
		control_flag = 0;
	}

	return IRQ_HANDLED;
}

static irqreturn_t dol_isr(int irq, void *para)
{
	uint32_t pending0 = 0;

	mipi_adap_reg_rd(MIPI_ADAPT_IRQ_PENDING0, MISC_IO, &pending0);

	if (pending0 & (1 << 19)) {
		adap_wr_reg_bits(MIPI_ADAPT_IRQ_PENDING0, MISC_IO, 1, 19, 1); //clear write done irq
		fte0_index ++;
	}

	if (frontend1_flag) {
		if (pending0 & (1 << 24)) {
			adap_wr_reg_bits(MIPI_ADAPT_IRQ_PENDING0, MISC_IO, 1, 24, 1); //clear write done irq
			fte1_index ++;
		}
	}

	if (dump_dol_frame) {   //replace ping/pong buffer
		pr_info("frontend0 index:%d, frontend1 index:%d\n",fte0_index, fte1_index);
		if (fte0_index == fte1_index) {
			if (fte0_index % 2 == 1) {
				adap_wr_reg_bits(CSI2_DDR_START_PIX, FRONTEND_IO, dol_buf[4], 0, 32);
				adap_wr_reg_bits(CSI2_DDR_START_PIX + FTE1_OFFSET, FRONTEND_IO, dol_buf[5], 0, 32);
			} else {
				adap_wr_reg_bits(CSI2_DDR_START_PIX_ALT, FRONTEND_IO, dol_buf[4], 0, 32);
				adap_wr_reg_bits(CSI2_DDR_START_PIX_ALT + FTE1_OFFSET, FRONTEND_IO, dol_buf[5], 0, 32);
			}
			buffer_index = fte0_index;
			dump_dol_frame = 0;
			complete(&wakeupdump);
		} else if (fte0_index > fte1_index){
			if (fte0_index % 2 == 1) {
				adap_wr_reg_bits(CSI2_DDR_START_PIX, FRONTEND_IO, dol_buf[4], 0, 32);
			} else {
				adap_wr_reg_bits(CSI2_DDR_START_PIX_ALT, FRONTEND_IO, dol_buf[4], 0, 32);
			}
		} else {
			if (fte1_index % 2 == 1) {
				adap_wr_reg_bits(CSI2_DDR_START_PIX + FTE1_OFFSET, FRONTEND_IO, dol_buf[5], 0, 32);
			} else {
				adap_wr_reg_bits(CSI2_DDR_START_PIX_ALT + FTE1_OFFSET, FRONTEND_IO, dol_buf[5], 0, 32);
			}
		}
	}
	return IRQ_HANDLED;
}

int am_adap_alloc_mem(void)
{

	if (para.mode == DDR_MODE) {
		cma_pages = dma_alloc_from_contiguous(
				  &(g_adap->p_dev->dev),
				  (g_adap->adap_buf_size * SZ_1M) >> PAGE_SHIFT, 0);
		if (cma_pages) {
			buffer_start = page_to_phys(cma_pages);
		} else {
			pr_err("alloc cma pages failed.\n");
			return 0;
		}
		isp_cma_mem = phys_to_virt(buffer_start);
	} else if (para.mode == DOL_MODE) {
		cma_pages = dma_alloc_from_contiguous(
				  &(g_adap->p_dev->dev),
				  (g_adap->adap_buf_size * SZ_1M) >> PAGE_SHIFT, 0);
		if (cma_pages) {
			buffer_start = page_to_phys(cma_pages);
		} else {
			pr_err("alloc dol cma pages failed.\n");
			return 0;
		}
	}
	return 0;
}

int am_adap_free_mem(void)
{
	if (para.mode == DDR_MODE) {
		if (cma_pages) {
			dma_release_from_contiguous(
				 &(g_adap->p_dev->dev),
				 cma_pages,
				 (g_adap->adap_buf_size * SZ_1M) >> PAGE_SHIFT);
			cma_pages = NULL;
			buffer_start = 0;
			pr_info("release alloc CMA buffer.\n");
		}
	} else if (para.mode == DOL_MODE) {
		if (cma_pages) {
			dma_release_from_contiguous(
				 &(g_adap->p_dev->dev),
				 cma_pages,
				 (g_adap->adap_buf_size * SZ_1M) >> PAGE_SHIFT);
			cma_pages = NULL;
			buffer_start = 0;
			pr_info("release alloc dol CMA buffer.\n");
		}
	}
	return 0;
}

int am_adap_init(void)
{
	int ret = 0;
	int depth;
	int i;
	int kfifo_ret = 0;
	resource_size_t temp_buf;
	char *buf = NULL;
	uint32_t stride;
	int buf_cnt;
	control_flag = 0;
	wbuf_index = 0;
	dump_flag = 0;
	dump_cur_flag = 0;
	dump_buf_index = 0;
	next_buf_index = 0;
	irq_count = 0;
	cur_buf_index = 0;
	current_flag = 0;

	if (frontend1_flag) {
		fte0_index = 0;
		fte1_index = 0;
		dump_dol_frame = 0;
		fte_state = FTE_DONE;
		init_completion(&wakeupdump);
	}
	if (cma_pages) {
		am_adap_free_mem();
		cma_pages = NULL;
	}

	if ((para.mode == DDR_MODE) ||
		(para.mode == DOL_MODE)) {
		am_adap_alloc_mem();
		depth = am_adap_get_depth();
		if ((cma_pages) && (para.mode == DDR_MODE)) {
			//note important : ddr_buf[0] and ddr_buf[1] address should alignment 16 byte
			stride = (para.img.width * depth)/8;
			stride = ((stride + (BOUNDRY - 1)) & (~(BOUNDRY - 1)));
			ddr_buf[0] = buffer_start;
			ddr_buf[0] = (ddr_buf[0] + (PAGE_SIZE - 1)) & (~(PAGE_SIZE - 1));
			temp_buf = ddr_buf[0];
			buf = phys_to_virt(ddr_buf[0]);
			memset(buf, 0x0, (stride * para.img.height));
			for (i = 1; i < DDR_BUF_SIZE; i++) {
				ddr_buf[i] = temp_buf + (stride * (para.img.height));
				ddr_buf[i] = (ddr_buf[i] + (PAGE_SIZE - 1)) & (~(PAGE_SIZE - 1));
				temp_buf = ddr_buf[i];
				buf = phys_to_virt(ddr_buf[i]);
				memset(buf, 0x0, (stride * para.img.height));
			}
		} else if ((cma_pages) && (para.mode == DOL_MODE)) {
			dol_buf[0] = buffer_start;
			dol_buf[0] = (dol_buf[0] + (PAGE_SIZE - 1)) & (~(PAGE_SIZE - 1));
			temp_buf = dol_buf[0];
			if (frontend1_flag)
				buf_cnt = DOL_BUF_SIZE;
			else
				buf_cnt = 2;
			for (i = 1; i < buf_cnt; i++) {
				dol_buf[i] = temp_buf + ((para.img.width) * (para.img.height) * depth)/8;
				dol_buf[i] = (dol_buf[i] + (PAGE_SIZE - 1)) & (~(PAGE_SIZE - 1));
				temp_buf = dol_buf[i];
			}
		}
	}

	if (para.mode == DOL_MODE) {
		//adap_wr_reg_bits(MIPI_ADAPT_IRQ_PENDING0, MISC_IO, 1, 24, 1);
		//adap_wr_reg_bits(MIPI_ADAPT_IRQ_PENDING0, MISC_IO, 1, 19, 1);
		ret = request_irq(g_adap->rd_irq, &dol_isr, IRQF_SHARED | IRQF_TRIGGER_HIGH,
		"adapter-irq", (void *)g_adap);
		pr_err("adapter dol irq = %d, ret = %d\n", g_adap->rd_irq, ret);
	}

	if (para.mode == DDR_MODE) {
		ret = request_irq(g_adap->rd_irq, &adpapter_isr, IRQF_SHARED | IRQF_TRIGGER_HIGH,
		"adapter-irq", (void *)g_adap);
		pr_info("adapter irq = %d, ret = %d\n", g_adap->rd_irq, ret);
	}

	if (para.mode == DDR_MODE) {
		kfifo_ret = kfifo_alloc(&adapt_fifo, PAGE_SIZE, GFP_KERNEL);
		if (kfifo_ret) {
			pr_info("alloc adapter fifo failed.\n");
			return kfifo_ret;
		}
	}

	//default setting : 720p & RAW12
	am_adap_frontend_init();
	am_adap_reader_init();
	am_adap_pixel_init();
	am_adap_alig_init();

	return 0;
}

int am_adap_start(int idx)
{
	am_adap_alig_start();
	am_adap_pixel_start();
	am_adap_reader_start();
	am_adap_frontend_start();

	return 0;
}

int am_adap_reset(void)
{
	mipi_adap_reg_wr(CSI2_CLK_RESET, FRONTEND_IO, 0x7);
	mipi_adap_reg_wr(CSI2_CLK_RESET, FRONTEND_IO, 0x6);
	mipi_adap_reg_wr(CSI2_CLK_RESET, FRONTEND_IO, 0x6);
	mipi_adap_reg_wr(CSI2_GEN_CTRL0, FRONTEND_IO, 0x001f0000);
	mipi_adap_reg_wr(MIPI_OTHER_CNTL0, ALIGN_IO, 0xf0000000);
	mipi_adap_reg_wr(MIPI_OTHER_CNTL0, ALIGN_IO, 0x00000000);

	return 0;
}

int am_adap_deinit(void)
{
	if (para.mode == DDR_MODE) {
		am_adap_free_mem();
		am_disable_irq();
		kfifo_free(&adapt_fifo);
	} else if (para.mode == DOL_MODE) {
		am_adap_free_mem();
	}
	am_adap_reset();
	control_flag = 0;
	wbuf_index = 0;
	dump_flag = 0;
	dump_cur_flag = 0;
	dump_buf_index = 0;
	next_buf_index = 0;
	irq_count = 0;
	cur_buf_index = 0;
	current_flag = 0;

	if (frontend1_flag) {
		fte0_index = 0;
		fte1_index = 0;
		dump_dol_frame = 0;
		fte_state = 0;
	}
	return 0;
}

