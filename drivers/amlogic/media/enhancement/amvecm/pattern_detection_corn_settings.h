/*
 * drivers/amlogic/media/enhancement/amvecm/pattern_detection_corn_settings.h
 *
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
 */

#ifndef __PATTERN_DETECTION_CORN_SETTINGS__
#define __PATTERN_DETECTION_CORN_SETTINGS__

static struct setting_regs_s corn_cvd2_settings[] = {
	/* corn AV NTSC */
	{
		1,
		{
			{REG_TYPE_APB,	0x153,	    0xffffffff,	0x50502020  },
		}
	},
	/* corn AV PAL */
	{
		1,
		{
			{REG_TYPE_APB,	0x153,	    0xffffffff,	0x18182020  },
		}
	},
	/* default AV NTSC */
	{
		1,
		{
			{REG_TYPE_APB,	0x153,	    0xffffffff,	0xffffffff  },
		}
	},
	/* default AV PAL */
	{
		1,
		{
			{REG_TYPE_APB,	0x153,	    0xffffffff,	0xffffffff  },
		}
	},
};

#endif

