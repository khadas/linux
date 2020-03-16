/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __GKI_MODULE_AMLOGIC_H
#define __GKI_MODULE_AMLOGIC_H

#ifdef MODULE

#undef __setup
#undef __setup_param
#undef early_param

#define GKI_MODULE_SETUP_MAGIC1 0x014589cd
#define GKI_MODULE_SETUP_MAGIC2 0x2367abef

struct gki_module_setup_struct {
	/* must be first */
	int magic1;
	int magic2;

	char *str;
	void *fn;
	int early;
};

#define __setup_gki_module(str, fn, early)			\
	struct gki_module_setup_struct __gki_setup_##fn =        \
		   {GKI_MODULE_SETUP_MAGIC1, GKI_MODULE_SETUP_MAGIC2,    \
		   str, fn, early};                                     \
	EXPORT_SYMBOL(__gki_setup_##fn)

#define __setup(str, fn)						\
		__setup_gki_module(str, fn, 0)

#define early_param(str, fn)						\
		__setup_gki_module(str, fn, 1)

static inline unsigned long gki_symbol_value(const struct kernel_symbol *sym)
{
#ifdef CONFIG_HAVE_ARCH_PREL32_RELOCATIONS
	const int *off = &sym->value_offset;

	return (unsigned long)((unsigned long)off + *off);
#else
	return sym->value;
#endif
}

void __module_init_hook(struct module *m);

#define module_init_hook(initfn)      \
	int init_module(void) \
	{       \
		__module_init_hook(THIS_MODULE); \
		return initfn();     \
	}

#undef early_initcall
#undef core_initcall
#undef core_initcall_sync
#undef postcore_initcall
#undef postcore_initcall_sync
#undef arch_initcall
#undef subsys_initcall
#undef subsys_initcall_sync
#undef fs_initcall
#undef fs_initcall_sync
#undef rootfs_initcall
#undef device_initcall
#undef device_initcall_sync
#undef late_initcall
#undef late_initcall_sync
#undef console_initcall
#undef security_initcall

#define early_initcall(fn)		module_init_hook(fn)
#define core_initcall(fn)		module_init_hook(fn)
#define core_initcall_sync(fn)		module_init_hook(fn)
#define postcore_initcall(fn)		module_init_hook(fn)
#define postcore_initcall_sync(fn)	module_init_hook(fn)
#define arch_initcall(fn)		module_init_hook(fn)
#define subsys_initcall(fn)		module_init_hook(fn)
#define subsys_initcall_sync(fn)	module_init_hook(fn)
#define fs_initcall(fn)			module_init_hook(fn)
#define fs_initcall_sync(fn)		module_init_hook(fn)
#define rootfs_initcall(fn)		module_init_hook(fn)
#define device_initcall(fn)		module_init_hook(fn)
#define device_initcall_sync(fn)	module_init_hook(fn)
#define late_initcall(fn)		module_init_hook(fn)
#define late_initcall_sync(fn)		module_init_hook(fn)
#define console_initcall(fn)		module_init_hook(fn)
#define security_initcall(fn)		module_init_hook(fn)

#undef module_init
#define module_init(fn)			module_init_hook(fn)

#endif //MODULE

#endif //__GKI_MODULE_AMLOGIC_H
