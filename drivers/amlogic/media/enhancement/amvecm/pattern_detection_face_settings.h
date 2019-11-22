/*
 * drivers/amlogic/media/enhancement/amvecm/pattern_detection_face_settings.h
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

#ifndef __PATTERN_DETECTION_FACE_SETTINGS__
#define __PATTERN_DETECTION_FACE_SETTINGS__

static struct setting_regs_s face_cvd2_settings[] = {
	/* face AV NTSC */
	{
		4,
		{
			{REG_TYPE_APB,	0x16f,	    0xffffffff,	0xa00833da  },
			{REG_TYPE_APB,	0x194,	    0xffffffff,	0x40100160  },
			{REG_TYPE_APB,	0x195,	    0xffffffff,	0x00000050  },
			{REG_TYPE_APB,	0x196,	    0xffffffff,	0x00000000  },
		}
	},
	/* face AV PAL */
	{
		4,
		{
			{REG_TYPE_APB,	0x16f,	    0xffffffff,	0x800833da  },
			{REG_TYPE_APB,	0x194,	    0xffffffff,	0x4011a293  },
			{REG_TYPE_APB,	0x195,	    0xffffffff,	0x34f2038a  },
			{REG_TYPE_APB,	0x196,	    0xffffffff,	0xfe0df9e8  },
		}
	},
	/* default AV NTSC */
	{
		4,
		{
			{REG_TYPE_APB,	0x16f,	    0xffffffff,	0xffffffff  },
			{REG_TYPE_APB,	0x194,	    0xffffffff,	0xffffffff  },
			{REG_TYPE_APB,	0x195,	    0xffffffff,	0xffffffff  },
			{REG_TYPE_APB,	0x196,	    0xffffffff,	0xffffffff  },
		}
	},
	/* default AV PAL */
	{
		4,
		{
			{REG_TYPE_APB,	0x16f,	    0xffffffff,	0xffffffff  },
			{REG_TYPE_APB,	0x194,	    0xffffffff,	0xffffffff  },
			{REG_TYPE_APB,	0x195,	    0xffffffff,	0xffffffff  },
			{REG_TYPE_APB,	0x196,	    0xffffffff,	0xffffffff  },
		}
	}
};

#endif

