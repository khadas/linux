// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>

int __init aml_dvb_extern_init(void)
{
	return 0;
}

void __exit aml_dvb_extern_exit(void)
{
}

fs_initcall(aml_dvb_extern_init);
module_exit(aml_dvb_extern_exit);

MODULE_AUTHOR("Amlogic");
MODULE_DESCRIPTION("Amlogic dvb extern driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("V1.00");
