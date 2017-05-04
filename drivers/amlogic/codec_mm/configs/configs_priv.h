
#ifndef AMLOGIC_MEDIA_CONFIG_HEADER_PRIV__
#define AMLOGIC_MEDIA_CONFIG_HEADER_PRIV__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
int configs_config_system_init(void);
int configs_inc_node_ref_locked(
	struct mconfig_node *node);
int configs_dec_node_ref_locked(
	struct mconfig_node *node);
int config_dump(void *buf, int size);
int configs_config_setstr(const char *buf);

#endif

