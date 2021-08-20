/*
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#include <linux/module.h>

#include "aml_vcodec_drv.h"
#include "aml_vcodec_util.h"

void aml_vcodec_set_curr_ctx(struct aml_vcodec_dev *dev,
	struct aml_vcodec_ctx *ctx)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->irqlock, flags);
	dev->curr_ctx = ctx;
	spin_unlock_irqrestore(&dev->irqlock, flags);
}
EXPORT_SYMBOL(aml_vcodec_set_curr_ctx);

struct aml_vcodec_ctx *aml_vcodec_get_curr_ctx(struct aml_vcodec_dev *dev)
{
	unsigned long flags;
	struct aml_vcodec_ctx *ctx;

	spin_lock_irqsave(&dev->irqlock, flags);
	ctx = dev->curr_ctx;
	spin_unlock_irqrestore(&dev->irqlock, flags);
	return ctx;
}
EXPORT_SYMBOL(aml_vcodec_get_curr_ctx);
