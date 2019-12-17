/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DEFENDKEY_H
#define __DEFENDKEY_H

#define CMD_SECURE_CHECK    _IO('d', 0x01)
#define CMD_DECRYPT_DTB     _IO('d', 0x02)

#define GET_SHARE_MEM_INPUT_BASE    (0x82000020)
#define AML_DATA_PROCESS            (0x820000FF)
#define AML_D_P_UPGRADE_CHECK       (0x80)
#define AML_D_P_IMG_DECRYPT         (0x40)
#define AML_D_Q_IMG_SIG_HDR_SIZE    (0x100)

#define DEFENDKEY_LIMIT_ADDR	    (0x0F000000)

#define NSTATE_INIT      (1)
#define NSTATE_UPDATE    (2)
#define NSTATE_FINAL     (4)
#define NSTATE_ALL       (7)

struct aml_defendkey_dev {
	struct platform_device *pdev;
	struct class           cls;
	struct cdev            cdev;
	dev_t                  devno;
	void __iomem	       *reg_base;
};

struct defendkey_mem {
	unsigned long base;
	unsigned long size;
};

enum e_defendkey_type {
	E_UPGRADE_CHECK = 0,
	E_DECRYPT_DTB   = 1,
};

struct aml_defendkey_type {
	enum e_defendkey_type decrypt_dtb;
	int                   status;
};

enum ret_defendkey {
	RET_FAIL    = 0,
	RET_SUCCESS = 1,
	RET_ERROR   = -1,
};

int aml_is_secure_set(void __iomem *reg_base);
unsigned long aml_sec_boot_check(unsigned long type,
				 unsigned long buffer,
				 unsigned long length,
				 unsigned long option);
#endif
