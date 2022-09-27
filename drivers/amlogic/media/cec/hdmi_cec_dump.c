// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/pwm.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include "hdmi_aocec_api.h"
#include "hdmi_ao_cec.h"

#define DEVICE_NAME "aocec"

static const char * const cec_reg_name1[] = {
	"CEC_TX_MSG_LENGTH",
	"CEC_TX_MSG_CMD",
	"CEC_TX_WRITE_BUF",
	"CEC_TX_CLEAR_BUF",
	"CEC_RX_MSG_CMD",
	"CEC_RX_CLEAR_BUF",
	"CEC_LOGICAL_ADDR0",
	"CEC_LOGICAL_ADDR1",
	"CEC_LOGICAL_ADDR2",
	"CEC_LOGICAL_ADDR3",
	"CEC_LOGICAL_ADDR4",
	"CEC_CLOCK_DIV_H",
	"CEC_CLOCK_DIV_L"
};

static const char * const cec_reg_name2[] = {
	"CEC_RX_MSG_LENGTH",
	"CEC_RX_MSG_STATUS",
	"CEC_RX_NUM_MSG",
	"CEC_TX_MSG_STATUS",
	"CEC_TX_NUM_MSG"
};

static const char * const ceca_reg_name3[] = {
	"STAT_0_0",
	"STAT_0_1",
	"STAT_0_2",
	"STAT_0_3",
	"STAT_1_0",
	"STAT_1_1",
	"STAT_1_2"
};

static int dump_cec_reg_show(struct seq_file *s, void *p)
{
	unsigned int reg32;
	int i = 0;
	unsigned char reg;

	seq_puts(s, "\n--------CEC registers--------\n");

	if (cec_dev->plat_data->chip_id == CEC_CHIP_A1) {
		seq_printf(s, "[%2x]:%2x\n", 0xe4, read_periphs(0xe4));
		seq_printf(s, "[%2x]:%2x\n", 0xe8, read_periphs(0xe8));
		seq_printf(s, "[%2x]:%2x\n", 0xec, read_periphs(0xec));
		seq_printf(s, "[%2x]:%2x\n", 0xf0, read_periphs(0xf0));
	} else if (cec_dev->plat_data->chip_id == CEC_CHIP_SC2 ||
		   cec_dev->plat_data->chip_id == CEC_CHIP_S4) {
		seq_printf(s, "[%2x]:%2x\n", 0x22, read_clock(0x22));
		seq_printf(s, "[%2x]:%2x\n", 0x23, read_clock(0x23));
		seq_printf(s, "[%2x]:%2x\n", 0x24, read_clock(0x24));
		seq_printf(s, "[%2x]:%2x\n", 0x25, read_clock(0x25));
	}

	if (ee_cec == CEC_B) {
		reg32 = read_ao(AO_CECB_CLK_CNTL_REG0);
		seq_printf(s, "AO_CECB_CLK_CNTL_REG0:0x%08x\n", reg32);
		reg32 = read_ao(AO_CECB_CLK_CNTL_REG1);
		seq_printf(s, "AO_CECB_CLK_CNTL_REG1:0x%08x\n", reg32);
		reg32 = read_ao(AO_CECB_GEN_CNTL);
		seq_printf(s, "AO_CECB_GEN_CNTL:0x%08x\n", reg32);
		reg32 = read_ao(AO_CECB_RW_REG);
		seq_printf(s, "AO_CECB_RW_REG: 0x%08x\n", reg32);
		reg32 = read_ao(AO_CECB_INTR_MASKN);
		seq_printf(s, "AO_CECB_INTR_MASKN:0x%08x\n", reg32);
		reg32 = read_ao(AO_CECB_INTR_STAT);
		seq_printf(s, "AO_CECB_INTR_STAT: 0x%08x\n", reg32);

		seq_puts(s, "CEC MODULE REGS:\n");
		seq_printf(s, "CEC_CTRL = 0x%02x\n",
			hdmirx_cec_read(DWC_CEC_CTRL));
		if (cec_dev->plat_data->cecb_ver >= CECB_VER_2)
			seq_printf(s, "CEC_CTRL2 = 0x%02x\n",
				hdmirx_cec_read(DWC_CEC_CTRL2));
		seq_printf(s, "CEC_MASK = 0x%02x\n",
			hdmirx_cec_read(DWC_CEC_MASK));
		seq_printf(s, "CEC_ADDR_L = 0x%02x\n",
			hdmirx_cec_read(DWC_CEC_ADDR_L));
		seq_printf(s, "CEC_ADDR_H = 0x%02x\n",
			hdmirx_cec_read(DWC_CEC_ADDR_H));
		seq_printf(s, "CEC_TX_CNT = 0x%02x\n",
			hdmirx_cec_read(DWC_CEC_TX_CNT));
		seq_printf(s, "CEC_RX_CNT = 0x%02x\n",
			hdmirx_cec_read(DWC_CEC_RX_CNT));
		if (cec_dev->plat_data->cecb_ver >= CECB_VER_2)
			seq_printf(s, "CEC_STAT0 = 0x%02x\n",
				hdmirx_cec_read(DWC_CEC_STAT0));
		seq_printf(s, "CEC_LOCK	= 0x%02x\n",
			hdmirx_cec_read(DWC_CEC_LOCK));
		seq_printf(s, "CEC_WKUPCTRL = 0x%02x\n",
			hdmirx_cec_read(DWC_CEC_WKUPCTRL));

		seq_printf(s, "%s", "RX buffer:");
		for (i = 0; i < 16; i++) {
			reg = hdmirx_cec_read(DWC_CEC_RX_DATA0 + i * 4) & 0xff;
			seq_printf(s, " %02x", reg);
		}
		seq_puts(s, "\n");

		seq_printf(s, "%s", "TX buffer:");
		for (i = 0; i < 16; i++) {
			reg = hdmirx_cec_read(DWC_CEC_TX_DATA0 + i * 4) & 0xff;
			seq_printf(s, " %02x", reg);
		}
		seq_puts(s, "\n");
	} else {
		reg32 = read_ao(AO_CEC_CLK_CNTL_REG0);
		seq_printf(s, "AO_CEC_CLK_CNTL_REG0: 0x%08x\n", reg32);
		reg32 = read_ao(AO_CEC_CLK_CNTL_REG1);
		seq_printf(s, "AO_CEC_CLK_CNTL_REG1:0x%08x\n", reg32);
		reg32 = read_ao(AO_CEC_GEN_CNTL);
		seq_printf(s, "AO_CEC_GEN_CNTL:	0x%08x\n", reg32);
		reg32 = read_ao(AO_CEC_RW_REG);
		seq_printf(s, "AO_CEC_RW_REG:	0x%08x\n", reg32);
		reg32 = read_ao(AO_CEC_INTR_MASKN);
		seq_printf(s, "AO_CEC_INTR_MASKN:0x%08x\n",	reg32);
		reg32 = read_ao(AO_CEC_INTR_STAT);
		seq_printf(s, "AO_CEC_INTR_STAT: 0x%08x\n", reg32);
		seq_puts(s, "CEC MODULE REGS:\n");
		for (i = 0; i < ARRAY_SIZE(cec_reg_name1); i++) {
			seq_printf(s, "%s:%2x\n",
				cec_reg_name1[i], aocec_rd_reg(i + 0x10));
		}

		for (i = 0; i < ARRAY_SIZE(cec_reg_name2); i++) {
			seq_printf(s, "%s:%2x\n",
				cec_reg_name2[i], aocec_rd_reg(i + 0x90));
		}

		if (cec_dev->plat_data->ceca_sts_reg) {
			for (i = 0; i < ARRAY_SIZE(ceca_reg_name3); i++) {
				seq_printf(s, "%s:%2x\n",
				 ceca_reg_name3[i], aocec_rd_reg(i + 0xA0));
			}
		}
	}

	seq_puts(s, "\n");
	return 0;
}

static int dump_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_cec_reg_show, inode->i_private);
}

static const struct file_operations dump_reg_fops = {
	.open		= dump_reg_open,
	.read		= seq_read,
	.release	= single_release,
};

static int dump_cec_status_show(struct seq_file *s, void *p)
{
	struct hdmi_port_info *port;
	unsigned char i = 0;
	unsigned int tmp = 0;

	seq_puts(s, "\n--------CEC status--------\n");
	seq_printf(s, "driver date:%s\n", CEC_DRIVER_VERSION);
	seq_printf(s, "ee_cec:%d\n", ee_cec);
	seq_printf(s, "cec_num:%d\n", cec_dev->cec_num);
	seq_printf(s, "dev_type:%d\n", (unsigned int)cec_dev->dev_type);
	seq_printf(s, "wk_logic_addr:0x%x\n", cec_dev->wakeup_data.wk_logic_addr);
	seq_printf(s, "wk_phy_addr:0x%x\n", cec_dev->wakeup_data.wk_phy_addr);
	seq_printf(s, "wk_port_id:0x%x\n", cec_dev->wakeup_data.wk_port_id);
	seq_printf(s, "wakeup_reason:0x%x\n", cec_dev->wakeup_reason);
	seq_printf(s, "phy_addr:0x%x\n", cec_dev->phy_addr);
	seq_printf(s, "cec_version:0x%x\n", cec_dev->cec_info.cec_version);
	seq_printf(s, "hal_ctl:0x%x\n", cec_dev->cec_info.hal_ctl);
	seq_printf(s, "menu_lang:0x%x\n", cec_dev->cec_info.menu_lang);
	seq_printf(s, "menu_status:0x%x\n", cec_dev->cec_info.menu_status);
	seq_printf(s, "open_count:%d\n", cec_dev->cec_info.open_count.counter);
	seq_printf(s, "vendor_id:0x%x\n", cec_dev->cec_info.vendor_id);
	seq_printf(s, "port_num:0x%x\n", cec_dev->port_num);
	seq_printf(s, "output:0x%x\n", cec_dev->output);
	seq_printf(s, "arc_port:0x%x\n", cec_dev->arc_port);
	seq_printf(s, "hal_flag:0x%x\n", cec_dev->hal_flag);
	seq_printf(s, "hpd_state:0x%x\n", get_hpd_state());
	seq_printf(s, "cec_config:0x%x\n", cec_config(0, 0));
	seq_printf(s, "log_addr:0x%x\n", cec_dev->cec_info.log_addr);

	seq_printf(s, "id:0x%x\n", cec_dev->plat_data->chip_id);
	seq_printf(s, "ceca_ver:0x%x\n", cec_dev->plat_data->ceca_ver);
	seq_printf(s, "cecb_ver:0x%x\n", cec_dev->plat_data->cecb_ver);
	seq_printf(s, "share_io:0x%x\n", cec_dev->plat_data->share_io);
	seq_printf(s, "line_bit:0x%x\n", cec_dev->plat_data->line_bit);
	seq_printf(s, "ee_to_ao:0x%x\n", cec_dev->plat_data->ee_to_ao);
	seq_printf(s, "irq_ceca:0x%x\n", cec_dev->irq_ceca);
	seq_printf(s, "irq_cecb:0x%x\n", cec_dev->irq_cecb);
	seq_printf(s, "framework_on:0x%x\n", cec_dev->framework_on);
	seq_printf(s, "store msg_num:0x%x\n", cec_dev->msg_num);
	seq_printf(s, "store msg_idx:0x%x\n", cec_dev->msg_idx);

	seq_printf(s, "cec_msg_dbg_en: %d\n", cec_msg_dbg_en);
	seq_printf(s, "port_seq: %x\n", cec_dev->port_seq);
	seq_printf(s, "cec_log_en: %x\n", cec_dev->cec_log_en);

	port = kcalloc(cec_dev->port_num, sizeof(*port), GFP_KERNEL);
	if (port) {
		init_cec_port_info(port, cec_dev);
		for (i = 0; i < cec_dev->port_num; i++) {
			/* port_id: 1/2/3 means HDMIRX1/2/3, 0 means HDMITX port */
			seq_printf(s, "port_id: %d, ",
					port[i].port_id);
			seq_printf(s, "port_type: %s, ",
					port[i].type == HDMI_INPUT ?
					"hdmirx" : "hdmitx");
			seq_printf(s, "physical_address: %x, ",
					port[i].physical_address);
			seq_printf(s, "cec_supported: %s, ",
					port[i].cec_supported ? "true" : "false");
			seq_printf(s, "arc_supported: %s\n",
					port[i].arc_supported ? "true" : "false");
		}
		kfree(port);
	}

#if (defined(CONFIG_AMLOGIC_HDMITX) || defined(CONFIG_AMLOGIC_HDMITX21))
	tmp |= (get_hpd_state() << 4);
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_HDMI
	tmp |= (hdmirx_get_connect_info() & 0xF);
#endif
	seq_printf(s, "hdmitx/rx connect status: 0x%x\n", tmp);

	if (cec_dev->cec_num > ENABLE_ONE_CEC) {
		seq_printf(s, "B addrL 0x%x\n", hdmirx_cec_read(DWC_CEC_ADDR_L));
		seq_printf(s, "B addrH 0x%x\n", hdmirx_cec_read(DWC_CEC_ADDR_H));

		seq_printf(s, "A addr0 0x%x\n", aocec_rd_reg(CEC_LOGICAL_ADDR0));
		seq_printf(s, "A addr1 0x%x\n", aocec_rd_reg(CEC_LOGICAL_ADDR1));
		/*seq_printf(s, "addr2 0x%x\n", aocec_rd_reg(CEC_LOGICAL_ADDR2));*/
		/*seq_printf(s, "addr3 0x%x\n", aocec_rd_reg(CEC_LOGICAL_ADDR3));*/
		/*seq_printf(s, "addr4 0x%x\n", aocec_rd_reg(CEC_LOGICAL_ADDR4));*/
	} else {
		if (ee_cec == CEC_B) {
			seq_printf(s, "addrL 0x%x\n",
				hdmirx_cec_read(DWC_CEC_ADDR_L));
			seq_printf(s, "addrH 0x%x\n",
				hdmirx_cec_read(DWC_CEC_ADDR_H));
		} else {
			seq_printf(s, "addr0 0x%x\n",
				aocec_rd_reg(CEC_LOGICAL_ADDR0));
			seq_printf(s, "addr1 0x%x\n",
				aocec_rd_reg(CEC_LOGICAL_ADDR1));
			seq_printf(s, "addr2 0x%x\n",
				aocec_rd_reg(CEC_LOGICAL_ADDR2));
			seq_printf(s, "addr3 0x%x\n",
				aocec_rd_reg(CEC_LOGICAL_ADDR3));
			seq_printf(s, "addr4 0x%x\n",
				aocec_rd_reg(CEC_LOGICAL_ADDR4));
		}
	}
	seq_printf(s, "addr_enable:0x%x\n", cec_dev->cec_info.addr_enable);
	seq_printf(s, "chk_sig_free_time: %d\n", cec_dev->chk_sig_free_time);
	seq_printf(s, "sw_chk_bus: %d\n", cec_dev->sw_chk_bus);

	seq_puts(s, "\n");
	return 0;
}

static int dump_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_cec_status_show, inode->i_private);
}

static const struct file_operations dump_status_fops = {
	.open		= dump_status_open,
	.read		= seq_read,
	.release	= single_release,
};

struct cec_dbg_files_s {
	const char *name;
	const umode_t mode;
	const struct file_operations *fops;
};

static struct cec_dbg_files_s cec_dbg_files[] = {
	{"cec_reg", S_IFREG | 0444, &dump_reg_fops},
	{"cec_status", S_IFREG | 0444, &dump_status_fops},
};

static struct dentry *cec_dbg_fs;
void cec_debug_fs_init(void)
{
	struct dentry *entry;
	int i;

	if (cec_dbg_fs)
		return;

	cec_dbg_fs = debugfs_create_dir(DEVICE_NAME, NULL);
	if (!cec_dbg_fs) {
		pr_err("can't create %s debugfs dir\n", DEVICE_NAME);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(cec_dbg_files); i++) {
		entry = debugfs_create_file(cec_dbg_files[i].name,
			cec_dbg_files[i].mode,
			cec_dbg_fs, NULL,
			cec_dbg_files[i].fops);
		if (!entry)
			pr_err("debugfs create file %s failed\n",
				cec_dbg_files[i].name);
	}
}

