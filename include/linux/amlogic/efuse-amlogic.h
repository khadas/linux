#ifndef __EFUSE_AMLOGIC_H
#define __EFUSE_AMLOGIC_H

#ifdef CONFIG_ARM64
struct efusekey_info {
	char keyname[32];
	unsigned int offset;
	unsigned int size;
};

extern int efusekeynum;

int efuse_getinfo(char *item, struct efusekey_info *info);
ssize_t efuse_user_attr_show(char *name, char *buf);
ssize_t efuse_user_attr_store(char *name, const char *buf, size_t count);
#endif
#endif
