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

#ifndef _AML_VCODEC_DEC_PM_H_
#define _AML_VCODEC_DEC_PM_H_

#include "aml_vcodec_drv.h"

int aml_vcodec_init_dec_pm(struct aml_vcodec_dev *dev);
void aml_vcodec_release_dec_pm(struct aml_vcodec_dev *dev);

void aml_vcodec_dec_pw_on(struct aml_vcodec_pm *pm);
void aml_vcodec_dec_pw_off(struct aml_vcodec_pm *pm);
void aml_vcodec_dec_clock_on(struct aml_vcodec_pm *pm);
void aml_vcodec_dec_clock_off(struct aml_vcodec_pm *pm);

#endif /* _AML_VCODEC_DEC_PM_H_ */
