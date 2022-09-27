/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _HDMI_AOCEC_API_H_
#define _HDMI_AOCEC_API_H_
#include "hdmi_tx_cec_20.h"
#include "hdmi_ao_cec.h"

extern int cec_msg_dbg_en;
extern struct cec_msg_last *last_cec_msg;
extern struct ao_cec_dev *cec_dev;
extern struct hrtimer start_bit_check;
extern bool ceca_err_flag;
extern bool ee_cec;
extern unsigned char msg_log_buf[128];

#define MAX_LOG_ADDR_CNT 1

/* application operation */
void cec_ap_clear_logical_addr(void);
void cec_ap_add_logical_addr(u32 l_addr);
void cec_ap_rm_logical_addr(u32 addr);
void cec_ap_set_dev_type(u32 type);

/* hw related */

unsigned int aocec_rd_reg(unsigned long addr);
void aocec_wr_reg(unsigned long addr, unsigned long data);
unsigned int hdmirx_cec_read(unsigned int reg);
void hdmirx_cec_write(unsigned int reg, unsigned int value);
inline void hdmirx_set_bits_dwc(u32 reg, u32 bits,
				       u32 start, u32 len);
unsigned int read_ao(unsigned int addr);
void write_ao(unsigned int addr, unsigned int data);
void cec_clear_saved_logic_addr(void);

void ceca_rx_buf_clear(void);
int ceca_trigger_tx(const unsigned char *msg, int len);
unsigned int ao_ceca_intr_stat(void);
void ceca_hw_reset(void);

int cecb_trigger_tx(const unsigned char *msg, unsigned char len, unsigned char sig_free);
void cecb_check_irq_enable(void);
int cecb_irq_stat(void);
inline void cecb_clear_irq(unsigned int flags);

void cec_set_clk(struct device *dev);
void cec_hw_init(void);
void cec_pre_init(void);
void cec_hw_reset(unsigned int cec_sel);

void cec_irq_enable(bool enable);
void cec_logicaddr_set(int l_add);
void cec_logicaddr_add(unsigned int cec_sel, unsigned int l_add);
void cec_logicaddr_remove(unsigned int cec_sel, unsigned int l_add);
void cec_clear_all_logical_addr(unsigned int cec_sel);
void cec_restore_logical_addr(unsigned int cec_sel, unsigned int addr_en);

void cec_enable_arc_pin(bool enable);

enum hrtimer_restart cec_line_check(struct hrtimer *timer);
int check_conflict(void);
void cec_ip_share_io(u32 share, u32 cec_ip);

/* msg related */
void cec_give_version(unsigned int dest);
void cec_report_physical_address_smp(void);
void cec_device_vendor_id(void);
void cec_give_deck_status(unsigned int dest);
void cec_menu_status_smp(int dest, int status);
void cec_inactive_source(int dest);
void cec_set_osd_name(int dest);
void cec_active_source_smp(void);
void cec_request_active_source(void);
void cec_set_stream_path(unsigned char *msg);
void cec_report_power_status(int dest, int status);

/* common logic interface */
bool need_nack_repeat_msg(const unsigned char *msg, int len, int t);
inline bool is_poll_message(unsigned char header);
inline bool is_get_cec_ver_msg(const unsigned char *msg, int len);

unsigned int cec_config(unsigned int value, bool wr_flag);
unsigned int cec_config2_phyaddr(unsigned int value, bool wr_flag);
unsigned int cec_config2_logaddr(unsigned int value, bool wr_flag);
unsigned int cec_config2_devtype(unsigned int value, bool wr_flag);

void cec_key_report(int suspend);
const char *cec_tx_ret_str(int ret);
void init_cec_port_info(struct hdmi_port_info *port,
			       struct ao_cec_dev *cec_dev);
void cec_status(void);
unsigned int cec_get_cur_phy_addr(void);

void cec_save_mail_box(void);
void cec_get_wakeup_reason(void);
unsigned int cec_get_wk_port_id(unsigned int phy_addr);
void cec_clear_wakeup_reason(void);
void cec_get_wakeup_data(void);
int dump_cec_reg(char *b);
int dump_cec_status(char *buf);
int cec_set_uevent(enum cec_event_type type, unsigned int val);
int cec_set_dev_info(uint8_t dev_idx);
int hdmirx_get_connect_info(void);
int hdmirx_set_cec_cfg(u32 cfg);
bool cec_message_op(unsigned char *msg, unsigned char len);
void cec_debug_fs_init(void);
unsigned int read_clock(unsigned int addr);
unsigned int read_periphs(unsigned int addr);
inline unsigned int get_pin_status(void);

#define CEC_ERR(format, args...)				\
	do {	\
		if (cec_dev->dbg_dev)				\
			pr_info("cec: " format, ##args);	\
	} while (0)

#define CEC_INFO(format, args...)				\
	do {	\
		if (cec_msg_dbg_en && cec_dev->dbg_dev)		\
			pr_info("cec: " format, ##args);	\
	} while (0)

#define dprintk(level, fmt, arg...) \
	do { \
		if (cec_msg_dbg_en >= (level) && cec_dev->dbg_dev) \
			pr_info("cec: " fmt, ## arg); \
	} while (0)

/* print tx/rx status */
#define CEC_PRINT(fmt, args...) \
	do { \
		if (cec_msg_dbg_en || cec_dev->cec_log_en) \
			pr_info("cec: " fmt, ##args); \
	} while (0)

#endif

