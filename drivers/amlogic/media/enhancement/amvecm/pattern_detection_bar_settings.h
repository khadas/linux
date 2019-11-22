/*
 * drivers/amlogic/media/enhancement/amvecm/pattern_detection_bar_settings.h
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

#ifndef __PATTERN_DETECTION_COLORBAR_SETTINGS__
#define __PATTERN_DETECTION_COLORBAR_SETTINGS__

static struct setting_regs_s colorbar_cvd2_settings[] = {
	/* color bar ATV NTSC */
	{
		1,
		{
			{REG_TYPE_APB,	0xb5,	    0xff,	0xcc        },
		}
	},
	/* color bar ATV PAL */
	{
		1,
		{
			{REG_TYPE_APB,	0xb5,	    0xff,	0xcc        },
		}
	},
	/* color bar ATV SECAM */
	{
		1,
		{
			{REG_TYPE_APB,	0xb5,	    0xff,	0xcc        },
		}
	},
	/* color bar AV NTSC */
	{
		1,
		{
			{REG_TYPE_APB,	0xb5,	    0xff,	0xc4        },
		}
	},
	/* color bar AV PAL */
	{
		1,
		{
			{REG_TYPE_APB,	0xb5,	    0xff,	0xc4        },
		}
	},
	/* color bar AV SECAM */
	{
		1,
		{
			{REG_TYPE_APB,	0xb5,	    0xff,	0xc4        },
		}
	},
	/* mixed color bar ATV NTSC */
	{
		1,
		{
			{REG_TYPE_APB,	0xb5,	    0xff,	0xcc        },
		}
	},
	/* mixed color bar ATV PAL */
	{
		1,
		{
			{REG_TYPE_APB,	0xb5,	    0xff,	0xcc        },
		}
	},
	/* mixed color bar  ATV SECAM */
	{
		1,
		{
			{REG_TYPE_APB,	0xb5,	    0xff,	0xcc        },
		}
	},
	/* mixed color bar  AV NTSC */
	{
		1,
		{
			{REG_TYPE_APB,	0xb5,	    0xff,	0xc4        },
		}
	},
	/* mixed color bar  AV PAL */
	{
		1,
		{
			{REG_TYPE_APB,	0xb5,	    0xff,	0xc4        },
		}
	},
	/* mixed color bar  AV SECAM */
	{
		1,
		{
			{REG_TYPE_APB,	0xb5,	    0xff,	0xc4        },
		}
	},
	/* default ATV NTSC */
	{
		1,
		{
			{REG_TYPE_APB,	0xb5,	    0xff,	0xffffffff },
		}
	},
	/* default ATV PAL */
	{
		1,
		{
			{REG_TYPE_APB,	0xb5,	    0xff,	0xffffffff },
		}
	},
	/* default ATV SECAM */
	{
		1,
		{
			{REG_TYPE_APB,	0xb5,	    0xff,	0xffffffff },
		}
	},
	/* default AV NTSC */
	{
		1,
		{
			{REG_TYPE_APB,	0xb5,	    0xff,	0xffffffff },
		}
	},
	/* default AV PAL */
	{
		1,
		{
			{REG_TYPE_APB,	0xb5,	    0xff,	0xffffffff },
		}
	},
	/* default AV SECAM */
	{
		1,
		{
			{REG_TYPE_APB,	0xb5,	    0xff,	0xffffffff },
		}
	}
};

static struct setting_regs_s colorbar_vpp_settings[] = {
	/* colorbar ATV */
	{0}, {0}, {0},
	/*
	 *{
	 *	2,
	 *	{
	 *	{REG_TYPE_VCBUS, SHARP0_NR_TI_DNLP_BLEND, 0xc, 0x4},
	 *	{REG_TYPE_VCBUS, SHARP0_HCTI_BST_CORE, 0xff000000, 0x4000000},
	 *	}
	 *},
	 */
	/* colorbar AV NTSC */
	{0},
	/*
	 *{
	 *	2,
	 *	{
	 *	{REG_TYPE_VCBUS, SHARP0_NR_TI_DNLP_BLEND, 0xc, 0x4},
	 *	{REG_TYPE_VCBUS, SHARP0_HCTI_BST_CORE, 0xff000000, 0x4000000},
	 *	}
	 *},
	 */
	/* colorbar AV PAL */
	{0},
	/*
	 *{
	 *	2,
	 *	{
	 *	{REG_TYPE_VCBUS, SHARP0_NR_TI_DNLP_BLEND, 0xc, 0x4},
	 *	{REG_TYPE_VCBUS, SHARP0_HCTI_BST_CORE, 0xff000000, 0x4000000},
	 *	}
	 *},
	 */
	/* colorbar AV SECAM */
	{0},
	/*
	 *{
	 *	2,
	 *	{
	 *	{REG_TYPE_VCBUS, SHARP0_NR_TI_DNLP_BLEND, 0xc, 0x4},
	 *	{REG_TYPE_VCBUS, SHARP0_HCTI_BST_CORE, 0xff000000, 0x4000000},
	 *	}
	 *},
	 */
	/* mixed color bar ATV */
	{0}, {0}, {0},
	/*
	 *{
	 *	2,
	 *	{
	 *	{REG_TYPE_VCBUS, SHARP0_NR_TI_DNLP_BLEND, 0xc, 0x4},
	 *	{REG_TYPE_VCBUS, SHARP0_HCTI_BST_CORE, 0xff000000, 0x4000000},
	 *	}
	 *},
	 */
	/* mixed color bar  AV NTSC */
	{0},
	/*
	 *{
	 *	2,
	 *	{
	 *	{REG_TYPE_VCBUS, SHARP0_NR_TI_DNLP_BLEND, 0xc, 0x4},
	 *	{REG_TYPE_VCBUS, SHARP0_HCTI_BST_CORE, 0xff000000, 0x4000000},
	 *	}
	 *},
	 */
	/* mixed color bar  AV PAL */
	{0},
	/*
	 *{
	 *	2,
	 *	{
	 *	{REG_TYPE_VCBUS, SHARP0_NR_TI_DNLP_BLEND, 0xc, 0x4},
	 *	{REG_TYPE_VCBUS, SHARP0_HCTI_BST_CORE, 0xff000000, 0x4000000},
	 *	}
	 *},
	 */
	/* mixed color bar  AV SECAM */
	{0},
	/*
	 *{
	 *	2,
	 *	{
	 *	{REG_TYPE_VCBUS, SHARP0_NR_TI_DNLP_BLEND, 0xc, 0x4},
	 *	{REG_TYPE_VCBUS, SHARP0_HCTI_BST_CORE, 0xff000000, 0x4000000},
	 *	}
	 *},
	 */
	/* default ATV */
	{0}, {0}, {0},
	/*
	 *{
	 *	2,
	 *	{
	 *	{REG_TYPE_VCBUS, SHARP0_NR_TI_DNLP_BLEND, 0xc, 0x4},
	 *	{REG_TYPE_VCBUS, SHARP0_HCTI_BST_CORE, 0xff000000, 0x4000000},
	 *	}
	 *},
	 */
	/* default  AV NTSC */
	{0},
	/*
	 *{
	 *	2,
	 *	{
	 *	{REG_TYPE_VCBUS, SHARP0_NR_TI_DNLP_BLEND, 0xc, 0x4},
	 *	{REG_TYPE_VCBUS, SHARP0_HCTI_BST_CORE, 0xff000000, 0x4000000},
	 *	}
	 *},
	 */
	/* default AV PAL */
	{0},
	/*
	 *{
	 *	2,
	 *	{
	 *	{REG_TYPE_VCBUS, SHARP0_NR_TI_DNLP_BLEND, 0xc, 0x4},
	 *	{REG_TYPE_VCBUS, SHARP0_HCTI_BST_CORE, 0xff000000, 0x4000000},
	 *	}
	 *},
	 */
	/* default  AV SECAM */
	{0},
	/*
	 *{
	 *	2,
	 *	{
	 *	{REG_TYPE_VCBUS, SHARP0_NR_TI_DNLP_BLEND, 0xc, 0x4},
	 *	{REG_TYPE_VCBUS, SHARP0_HCTI_BST_CORE, 0xff000000, 0x4000000},
	 *	}
	 *},
	 */
};
#endif

