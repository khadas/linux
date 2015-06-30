/*
 * drivers/amlogic/display/vout2/tvregs.h
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


#ifndef _TVREGS_H_
#define _TVREGS_H_

/* Amlogic Headers */
#include <linux/amlogic/vout/vinfo.h>

/* Local Headers */
#include <vout/vout_reg.h>

#define MREG_END_MARKER 0xffff
static const struct reg_s tvregs_720p_clk[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_720p_enc[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_720p_50hz_clk[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_720p_50hz_enc[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_480i_clk[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_480i_enc[] = {
	{MREG_END_MARKER,              0      }
};

static const struct reg_s tvregs_480cvbs_clk[] = {
	{MREG_END_MARKER,              0      }
};

static const struct reg_s tvregs_480cvbs_enc[] = {
	/* {VENC_VDAC_SETTING,            0xff,  }, */
	{ENCI_CFILT_CTRL,              0x12,},
	{ENCI_CFILT_CTRL2,              0x12,},
	{VENC_DVI_SETTING,             0,     },
	{ENCI_VIDEO_MODE,              0,     },
	{ENCI_VIDEO_MODE_ADV,          0,     },
	{ENCI_SYNC_HSO_BEGIN,          5,     },
	{ENCI_SYNC_HSO_END,            129,   },
	{ENCI_SYNC_VSO_EVNLN,          0x0003 },
	{ENCI_SYNC_VSO_ODDLN,          0x0104 },
	{ENCI_MACV_MAX_AMP,            0x810b },
	{VENC_VIDEO_PROG_MODE,         0xf0   },
	{ENCI_VIDEO_MODE,              0x08   },
	{ENCI_VIDEO_MODE_ADV,          0x26,  },
	{ENCI_VIDEO_SCH,               0x20,  },
	{ENCI_SYNC_MODE,               0x07,  },
	{ENCI_YC_DELAY,                0x333, },
	{ENCI_VFIFO2VD_PIXEL_START,    0xf3,  },
	{ENCI_VFIFO2VD_PIXEL_END,      0x0693,},
	{ENCI_VFIFO2VD_LINE_TOP_START, 0x12,  },
	{ENCI_VFIFO2VD_LINE_TOP_END,   0x102, },
	{ENCI_VFIFO2VD_LINE_BOT_START, 0x13,  },
	{ENCI_VFIFO2VD_LINE_BOT_END,   0x103, },
	{VENC_SYNC_ROUTE,              0,     },
	{ENCI_DBG_PX_RST,              0,     },
	{VENC_INTCTRL,                 0x2,   },
	{ENCI_VFIFO2VD_CTL,            0x4e01,},
	{VENC_VDAC_SETTING,          0,     },
	{VENC_UPSAMPLE_CTRL0,          0x0061,},
	{VENC_UPSAMPLE_CTRL1,          0x4061,},
	{VENC_UPSAMPLE_CTRL2,          0x5061,},
	{VENC_VDAC_DACSEL0,            0x0000,},
	{VENC_VDAC_DACSEL1,            0x0000,},
	{VENC_VDAC_DACSEL2,            0x0000,},
	{VENC_VDAC_DACSEL3,            0x0000,},
	{VENC_VDAC_DACSEL4,            0x0000,},
	{VENC_VDAC_DACSEL5,            0x0000,},
	{VENC_VDAC_FIFO_CTRL,          0x2000,},
	{ENCI_DACSEL_0,                0x0011 },
	{ENCI_DACSEL_1,                0x11   },
	{ENCI_VIDEO_EN,                1,     },
	/* {HHI_VDAC_CNTL0,               0x650001   }, */
	/* {HHI_VDAC_CNTL1,               0x1        }, */
	{ENCI_VIDEO_SAT,               0x7        },
	{VENC_VDAC_DAC0_FILT_CTRL0,    0x1        },
	{VENC_VDAC_DAC0_FILT_CTRL1,    0xfc48     },
	{MREG_END_MARKER,              0      }
};

static const struct reg_s tvregs_480p_clk[] = {
	{MREG_END_MARKER,              0      }
};

static const struct reg_s tvregs_480p_enc[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_576i_clk[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_576i_enc[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_576cvbs_enc[] = {
	/* {VENC_VDAC_SETTING,          0xff,  }, */
	/* {VCLK_SD}, */
	{ENCI_CFILT_CTRL,                 0x12,    },
	{ENCI_CFILT_CTRL2,                 0x12,    },
	{VENC_DVI_SETTING,                0,         },
	{ENCI_VIDEO_MODE,                 0,         },
	{ENCI_VIDEO_MODE_ADV,             0,         },
	{ENCI_SYNC_HSO_BEGIN,             3,         },
	{ENCI_SYNC_HSO_END,               129,       },
	{ENCI_SYNC_VSO_EVNLN,             0x0003     },
	{ENCI_SYNC_VSO_ODDLN,             0x0104     },
	{ENCI_MACV_MAX_AMP,               0x8107     },
	{VENC_VIDEO_PROG_MODE,            0xff       },
	{ENCI_VIDEO_MODE,                 0x13       },
	{ENCI_VIDEO_MODE_ADV,             0x26,      },
	{ENCI_VIDEO_SCH,                  0x28,      },
	{ENCI_SYNC_MODE,                  0x07,      },
	{ENCI_YC_DELAY,                   0x333,     },
	{ENCI_VFIFO2VD_PIXEL_START,       0x010b     },
	{ENCI_VFIFO2VD_PIXEL_END,         0x06ab     },
	{ENCI_VFIFO2VD_LINE_TOP_START,    0x0016     },
	{ENCI_VFIFO2VD_LINE_TOP_END,      0x0136     },
	{ENCI_VFIFO2VD_LINE_BOT_START,    0x0017     },
	{ENCI_VFIFO2VD_LINE_BOT_END,      0x0137     },
	{VENC_SYNC_ROUTE,                 0,         },
	{ENCI_DBG_PX_RST,                 0,         },
	{VENC_INTCTRL,                    0x2,       },
	{ENCI_VFIFO2VD_CTL,               0x4e01,    },
	{VENC_VDAC_SETTING,          0,     },
	{VENC_UPSAMPLE_CTRL0,             0x0061,    },
	{VENC_UPSAMPLE_CTRL1,             0x4061,    },
	{VENC_UPSAMPLE_CTRL2,             0x5061,    },
	{VENC_VDAC_DACSEL0,               0x0000,    },
	{VENC_VDAC_DACSEL1,               0x0000,    },
	{VENC_VDAC_DACSEL2,               0x0000,    },
	{VENC_VDAC_DACSEL3,               0x0000,    },
	{VENC_VDAC_DACSEL4,               0x0000,    },
	{VENC_VDAC_DACSEL5,               0x0000,    },
	{VENC_VDAC_FIFO_CTRL,             0x2000,    },
	{ENCI_DACSEL_0,                   0x0011     },
	{ENCI_DACSEL_1,                   0x11       },
	{ENCI_VIDEO_EN,                   1,         },
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_576p_clk[] = {
	{MREG_END_MARKER,            0          }
};

static const struct reg_s tvregs_576p_enc[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_1080i_clk[] = {
	{MREG_END_MARKER,            0          }
};

static const struct reg_s tvregs_1080i_enc[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_1080i_50hz_clk[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_1080i_50hz_enc[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_1080p_clk[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_1080p_enc[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_1080p_50hz_clk[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_1080p_50hz_enc[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_1080p_24hz_enc[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_4k2k_30hz_enc[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_4k2k_25hz_enc[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_4k2k_24hz_enc[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_4k2k_smpte_enc[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_vga_640x480_enc[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_svga_800x600_enc[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_xga_1024x768_enc[] = {
	{MREG_END_MARKER,            0      }
};

/* Using tvmode as index */
static struct tvregs_set_t tvregsTab2[] = {
	{TVMODE_480I, tvregs_480i_clk, tvregs_480i_enc},
	{TVMODE_480I_RPT, tvregs_480i_clk, tvregs_480i_enc},
	{TVMODE_480CVBS, tvregs_480cvbs_clk, tvregs_480cvbs_enc},
	{TVMODE_480P, tvregs_480p_clk, tvregs_480p_enc},
	{TVMODE_480P_RPT, tvregs_480p_clk, tvregs_480p_enc},
	{TVMODE_576I, tvregs_576i_clk, tvregs_576i_enc},
	{TVMODE_576I_RPT, tvregs_576i_clk, tvregs_576i_enc},
	{TVMODE_576CVBS, NULL, tvregs_576cvbs_enc},
	{TVMODE_576P, tvregs_576p_clk, tvregs_576p_enc},
	{TVMODE_576P_RPT, tvregs_576p_clk, tvregs_576p_enc},
	{TVMODE_720P, tvregs_720p_clk, tvregs_720p_enc},
	{TVMODE_1080I, tvregs_1080i_clk, tvregs_1080i_enc},
	{TVMODE_1080P, tvregs_1080p_clk, tvregs_1080p_enc},
	{TVMODE_720P_50HZ, tvregs_720p_50hz_clk, tvregs_720p_50hz_enc},
	{TVMODE_1080I_50HZ, tvregs_1080i_50hz_clk, tvregs_1080i_50hz_enc},
	{TVMODE_1080P_50HZ, tvregs_1080p_50hz_clk, tvregs_1080p_50hz_enc},
	{TVMODE_1080P_24HZ, NULL, tvregs_1080p_24hz_enc},
	{TVMODE_4K2K_30HZ, NULL, tvregs_4k2k_30hz_enc},
	{TVMODE_4K2K_25HZ, NULL, tvregs_4k2k_25hz_enc},
	{TVMODE_4K2K_24HZ, NULL, tvregs_4k2k_24hz_enc},
	{TVMODE_4K2K_SMPTE, NULL, tvregs_4k2k_smpte_enc},
	{TVMODE_VGA, NULL, tvregs_vga_640x480_enc},
	{TVMODE_SVGA, NULL, tvregs_svga_800x600_enc},
	{TVMODE_XGA, NULL, tvregs_xga_1024x768_enc},
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	{TVMODE_480P_59HZ, tvregs_480p_clk, tvregs_480p_enc},
	{TVMODE_720P_59HZ , tvregs_720p_clk, tvregs_720p_enc},
	{TVMODE_1080I_59HZ, tvregs_1080i_clk, tvregs_1080i_enc},
	{TVMODE_1080P_59HZ, tvregs_1080p_clk, tvregs_1080p_enc},
	{TVMODE_1080P_23HZ, NULL, tvregs_1080p_24hz_enc},
	{TVMODE_4K2K_29HZ, NULL, tvregs_4k2k_30hz_enc},
	{TVMODE_4K2K_23HZ, NULL, tvregs_4k2k_24hz_enc},
#endif
};

static const struct tvinfo_s tvinfoTab2[] = {
	{
		.tvmode = TVMODE_480I,
		.xres = 720,
		.yres = 480,
		.id = "480i60hz"
	},
	{
		.tvmode = TVMODE_480I_RPT,
		.xres = 720,
		.yres = 480,
		.id = "480i_rpt"
	},
	{
		.tvmode = TVMODE_480CVBS,
		.xres = 720,
		.yres = 480,
		.id = "480cvbs"
	},
	{
		.tvmode = TVMODE_480P,
		.xres = 720,
		.yres = 480,
		.id = "480p60hz"
	},
	{
		.tvmode = TVMODE_480P_RPT,
		.xres = 720,
		.yres = 480,
		.id = "480p_rpt"
	},
	{
		.tvmode = TVMODE_576I,
		.xres = 720,
		.yres = 576,
		.id = "576i50hz"
	},
	{
		.tvmode = TVMODE_576I_RPT,
		.xres = 720,
		.yres = 576,
		.id = "576i_rpt"
	},
	{
		.tvmode = TVMODE_576CVBS,
		.xres = 720,
		.yres = 576,
		.id = "576cvbs"
	},
	{
		.tvmode = TVMODE_576P,
		.xres = 720,
		.yres = 576,
		.id = "576p50hz"
	},
	{
		.tvmode = TVMODE_576P_RPT,
		.xres = 720,
		.yres = 576,
		.id = "576p_prt"
	},
	{
		.tvmode = TVMODE_720P,
		.xres = 1280,
		.yres = 720,
		.id = "720p60hz"
	},
	{
		.tvmode = TVMODE_1080I,
		.xres = 1920,
		.yres = 1080,
		.id = "1080i60hz"
	},
	{
		.tvmode = TVMODE_1080P,
		.xres = 1920,
		.yres = 1080,
		.id = "1080p60hz"
	},
	{
		.tvmode = TVMODE_720P_50HZ,
		.xres = 1280,
		.yres = 720,
		.id = "720p50hz"
	},
	{
		.tvmode = TVMODE_1080I_50HZ,
		.xres = 1920,
		.yres = 1080,
		.id = "1080i50hz"
	},
	{
		.tvmode = TVMODE_1080P_50HZ,
		.xres = 1920,
		.yres = 1080,
		.id = "1080p50hz"
	},
	{
		.tvmode = TVMODE_1080P_24HZ,
		.xres = 1920,
		.yres = 1080,
		.id = "1080p24hz"
	},
	{
		.tvmode = TVMODE_4K2K_30HZ,
		.xres = 3840,
		.yres = 2160,
		.id = "2160p30hz"
	},
	{
		.tvmode = TVMODE_4K2K_25HZ,
		.xres = 3840,
		.yres = 2160,
		.id = "2160p25hz"
	},
	{
		.tvmode = TVMODE_4K2K_24HZ,
		.xres = 3840,
		.yres = 2160,
		.id = "2160p24hz"
	},
	{
		.tvmode = TVMODE_4K2K_SMPTE,
		.xres = 4096,
		.yres = 2160,
		.id = "smpte24hz"
	},
	{
		.tvmode = TVMODE_VGA,
		.xres = 640,
		.yres = 480,
		.id = "vga"
	},
	{
		.tvmode = TVMODE_SVGA,
		.xres = 800,
		.yres = 600,
		.id = "svga"
	},
	{
		.tvmode = TVMODE_XGA,
		.xres = 1024,
		.yres = 768,
		.id = "xga"
	},
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	{
		.tvmode = TVMODE_480P_59HZ,
		.xres = 720,
		.yres =	480,
		.id = "480p59hz"
	},
	{
		.tvmode = TVMODE_720P_59HZ,
		.xres = 1280,
		.yres = 720,
		.id = "720p59hz"
	},
	{
		.tvmode = TVMODE_1080I_59HZ,
		.xres = 1920,
		.yres = 1080,
		.id = "1080i59hz"
	},
	{
		.tvmode = TVMODE_1080P_59HZ,
		.xres = 1920,
		.yres = 1080,
		.id = "1080p59hz"
	},
	{
		.tvmode = TVMODE_1080P_23HZ,
		.xres = 1920,
		.yres = 1080,
		.id = "1080p23hz"
	},
	{
		.tvmode = TVMODE_4K2K_29HZ,
		.xres = 3840,
		.yres = 2160,
		.id = "4k2k29hz"
	},
	{
		.tvmode = TVMODE_4K2K_23HZ,
		.xres = 3840,
		.yres = 2160,
		.id = "4k2k23hz"
	},
#endif
};

#endif /* TVREGS_H */

