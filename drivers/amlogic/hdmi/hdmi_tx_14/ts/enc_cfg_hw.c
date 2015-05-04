/*
 * drivers/amlogic/hdmi/hdmi_tx_14/ts/enc_cfg_hw.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/amlogic/vout/vinfo.h>
#include <linux/amlogic/vout/enc_clk_config.h>
#include <linux/amlogic/hdmi_tx/hdmi_info_global.h>
#include <linux/amlogic/hdmi_tx/hdmi_tx_module.h>
#include <linux/reset.h>
#include "mach_reg.h"

#include "mach_reg.h"
#include <linux/amlogic/vout/vinfo.h>

#if 0
static const reg_t tvregs_1080p[] = {
	{P_ENCP_VIDEO_EN,              0,     },
	{P_ENCI_VIDEO_EN,              0,     },

	{P_VENC_DVI_SETTING,           0x0001,},
	{P_ENCP_VIDEO_MODE,            0x4040,},
	{P_ENCP_VIDEO_MODE_ADV,        0x0018,},
	{P_ENCP_VIDEO_YFP1_HTIME,      140,   },
	{P_ENCP_VIDEO_YFP2_HTIME,      2060,  },
	{P_ENCP_VIDEO_MAX_PXCNT,       2199,  },
	{P_ENCP_VIDEO_HSPULS_BEGIN,    2156,  },/* 1980 */
	{P_ENCP_VIDEO_HSPULS_END,      44,    },
	{P_ENCP_VIDEO_HSPULS_SWITCH,   44,    },
	{P_ENCP_VIDEO_VSPULS_BEGIN,    140,   },
	{P_ENCP_VIDEO_VSPULS_END,      2059,  },
	{P_ENCP_VIDEO_VSPULS_BLINE,    0,     },
	{P_ENCP_VIDEO_VSPULS_ELINE,    4,     },/* 35 */
	{P_ENCP_VIDEO_HAVON_BEGIN,     148,   },
	{P_ENCP_VIDEO_HAVON_END,       2067,  },
	{P_ENCP_VIDEO_VAVON_BLINE,     41,    },
	{P_ENCP_VIDEO_VAVON_ELINE,     1120,  },
	{P_ENCP_VIDEO_HSO_BEGIN,       44,    },
	{P_ENCP_VIDEO_HSO_END,         2156,  },
	{P_ENCP_VIDEO_VSO_BEGIN,       2100,  },
	{P_ENCP_VIDEO_VSO_END,         2164,  },
	{P_ENCP_VIDEO_VSO_BLINE,       0,     },
	{P_ENCP_VIDEO_VSO_ELINE,       5,     },
	{P_ENCP_VIDEO_MAX_LNCNT,       1124,  },
	{P_VPU_VIU_VENC_MUX_CTRL,      0x000a,},
	{P_VENC_VIDEO_PROG_MODE,       0x100, },
	{P_ENCI_VIDEO_EN,              0,     },
	{P_ENCP_VIDEO_EN,              1,     },
	{MREG_END_MARKER,            0      }
};

static const reg_t tvregs_1080p_50hz[] = {
	{P_ENCP_VIDEO_EN,              0,     },
	{P_ENCI_VIDEO_EN,              0,     },

    /* bit 13    1          (delayed prog_vs) */
    /* bit 5:4:  2          (pixel[0]) */
    /* bit 3:    1          invert vsync or not */
    /* bit 2:    1          invert hsync or not */
    /* bit1:     1          (select viu sync) */
    /* bit0:     1          (progressive) */
	{P_VENC_DVI_SETTING,           0x000d,},
	{P_ENCP_VIDEO_MAX_PXCNT,       2639,  },
	{P_ENCP_VIDEO_MAX_LNCNT,       1124,  },
    /* horizontal timing settings */
	{P_ENCP_VIDEO_HSPULS_BEGIN,    44,  },/* 1980 */
	{P_ENCP_VIDEO_HSPULS_END,      132,    },
	{P_ENCP_VIDEO_HSPULS_SWITCH,   44,    },

    /* DE position in horizontal */
	{P_ENCP_VIDEO_HAVON_BEGIN,     271,   },
	{P_ENCP_VIDEO_HAVON_END,       2190,  },

    /* ditital hsync positon in horizontal */
	{P_ENCP_VIDEO_HSO_BEGIN,       79 ,    },
	{P_ENCP_VIDEO_HSO_END,         123,  },

    /* vsync horizontal timing */
	{P_ENCP_VIDEO_VSPULS_BEGIN,    220,   },
	{P_ENCP_VIDEO_VSPULS_END,      2140,  },

    /* vertical timing settings */
	{P_ENCP_VIDEO_VSPULS_BLINE,    0,     },
	{P_ENCP_VIDEO_VSPULS_ELINE,    4,     },/* 35 */
	{P_ENCP_VIDEO_EQPULS_BLINE,    0,     },
	{P_ENCP_VIDEO_EQPULS_ELINE,    4,     },/* 35 */
	{P_ENCP_VIDEO_VAVON_BLINE,     41,    },
	{P_ENCP_VIDEO_VAVON_ELINE,     1120,  },

    /* adjust the hsync & vsync start point and end point */
	{P_ENCP_VIDEO_VSO_BEGIN,       79,  },
	{P_ENCP_VIDEO_VSO_END,         79,  },

    /* adjust the vsync start line and end line */
	{P_ENCP_VIDEO_VSO_BLINE,       0,     },
	{P_ENCP_VIDEO_VSO_ELINE,       5,     },

	{P_ENCP_VIDEO_YFP1_HTIME,      271,   },
	{P_ENCP_VIDEO_YFP2_HTIME,      2190,  },
	{P_VENC_VIDEO_PROG_MODE,       0x100, },
	{P_ENCP_VIDEO_MODE,            0x4040,},
	{P_ENCP_VIDEO_MODE_ADV,        0x0018,},

	{P_VPU_VIU_VENC_MUX_CTRL,      0x000a,},
	{P_ENCI_VIDEO_EN,              0,     },
	{P_ENCP_VIDEO_EN,              1,     },
	{MREG_END_MARKER,            0      }
};
/* Using tvmode as index */
static struct tvregs_set_t tvregsTab[] = {
	{TVMODE_1080P, tvregs_1080p},
	{TVMODE_1080P_50HZ, tvregs_1080p_50hz},
};

static const tvinfo_t tvinfoTab[] = {
	{.tvmode = TVMODE_1080P, .xres = 1920, .yres = 1080, .id = "1080p"},
	{.tvmode = TVMODE_1080P_50HZ, .xres = 1920, .yres = 1080,
		.id = "1080p50hz"},
};

static inline void setreg(const reg_t *r)
{
	hd_write_reg(r->reg, r->val);
	/* printk("[0x%x] = 0x%x\n", r->reg, r->val); */
}

void set_vmode_enc_hw(enum hdmi_vic vic)
{
	const reg_t *s;

	if (vic == HDMI_1080p60)
		s = tvregsTab[0].reg_setting;
	if (vic == HDMI_1080p50)
		s = tvregsTab[0].reg_setting;

	while (MREG_END_MARKER != s->reg)
		setreg(s++);
	pr_info("%s[%d]\n", __func__, __LINE__);
}
#endif
