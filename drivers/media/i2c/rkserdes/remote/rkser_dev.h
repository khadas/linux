/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 */

#ifndef __RKSER_DEV_H__
#define __RKSER_DEV_H__

#include <linux/version.h>
#include <linux/i2c.h>

/* Serializer State */
enum {
	RKSER_STATE_DEINIT = 0,
	RKSER_STATE_INIT,
};

struct rkser_dev;

/* rkser ops */
struct rkser_ops {
	int (*ser_module_init)(struct rkser_dev *rk_ser);
	int (*ser_module_deinit)(struct rkser_dev *rk_ser);
};

/* rk serializer device */
struct rkser_dev {
	struct i2c_client	*client; /* serializer i2c client */

	int			ser_state; /* serializer state */

	void			*ser_data; /* serializer private data */

	const struct rkser_ops	*ser_ops; /* serializer operation function */
};

/* serializer type define for sensor */
typedef struct rkser_dev	serializer_t;

/* serializer initialization completed */
static inline bool rkser_is_inited(struct rkser_dev *rkser_dev)
{
	if (rkser_dev && (rkser_dev->ser_state == RKSER_STATE_INIT))
		return true;

	return false;
}

/* camera get serializer by phandle */
static inline struct rkser_dev *rkcam_get_ser_by_phandle(struct device *cam_dev)
{
	struct i2c_client *ser_client = NULL;
	struct device_node *ser_node = NULL;
	struct rkser_dev *serializer = NULL;

	/* camera get remote serializer node */
	ser_node = of_parse_phandle(cam_dev->of_node, "cam-remote-ser", 0);
	if (!IS_ERR_OR_NULL(ser_node)) {
		dev_info(cam_dev, "remote serializer node: %pOF\n", ser_node);

		ser_client = of_find_i2c_device_by_node(ser_node);
		of_node_put(ser_node);
		if (!IS_ERR_OR_NULL(ser_client)) {
			serializer = i2c_get_clientdata(ser_client);
			if (!IS_ERR_OR_NULL(serializer))
				return serializer;
			else
				return NULL;
		} else {
			dev_err(cam_dev, "camera find remote serializer client error\n");

			return NULL;
		}
	} else {
		dev_warn(cam_dev, "cam-remote-ser node isn't exist\n");
		return NULL;
	}
}

#endif /* __RKSER_DEV_H__ */
