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
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*
* Description:
*/
#ifndef _VDEC_DRV_BASE_
#define _VDEC_DRV_BASE_

#include "aml_vcodec_drv.h"

#include "vdec_drv_if.h"

struct vdec_common_if {
	/**
	 * (*init)() - initialize decode driver
	 * @ctx     : [in] aml v4l2 context
	 * @h_vdec  : [out] driver handle
	 */
	int (*init)(struct aml_vcodec_ctx *ctx, unsigned long *h_vdec);

	int (*probe)(unsigned long h_vdec,
		struct aml_vcodec_mem *bs, void *out);

	/**
	 * (*decode)() - trigger decode
	 * @h_vdec  : [in] driver handle
	 * @bs      : [in] input bitstream
	 * @fb      : [in] frame buffer to store decoded frame
	 * @res_chg : [out] resolution change happen
	 */
	int (*decode)(unsigned long h_vdec, struct aml_vcodec_mem *bs,
		unsigned long int pts, bool *res_chg);

	/**
	 * (*get_param)() - get driver's parameter
	 * @h_vdec : [in] driver handle
	 * @type   : [in] input parameter type
	 * @out    : [out] buffer to store query result
	 */
	int (*get_param)(unsigned long h_vdec,
		enum vdec_get_param_type type, void *out);

	/**
	 * (*deinit)() - deinitialize driver.
	 * @h_vdec : [in] driver handle to be deinit
	 */
	void (*deinit)(unsigned long h_vdec);
};

#endif
