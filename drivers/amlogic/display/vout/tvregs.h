/*
 * drivers/amlogic/display/vout/tvregs.h
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
#include "vout_reg.h"

#define CONFIG_CVBS_PERFORMANCE_COMPATIBLITY_SUPPORT	1
#define VIDEO_CLOCK_HD_25	0x00101529
#define VIDEO_CLOCK_SD_25	0x00500a6c
#define VIDEO_CLOCK_HD_24	0x00140863
#define VIDEO_CLOCK_SD_24	0x0050042d

static const struct reg_s tvreg_vclk_sd[] = {
	{HHI_VID_PLL_CNTL, VIDEO_CLOCK_SD_24}, /* SD.24 */
	{HHI_VID_PLL_CNTL, VIDEO_CLOCK_SD_25}, /* SD,25 */
};

static const  struct reg_s tvreg_vclk_hd[] = {
	{HHI_VID_PLL_CNTL, VIDEO_CLOCK_HD_24}, /* HD,24 */
	{HHI_VID_PLL_CNTL, VIDEO_CLOCK_HD_25}, /* HD,25 */
};

#define MREG_END_MARKER 0xffff

#ifdef CONFIG_CVBS_PERFORMANCE_COMPATIBLITY_SUPPORT
static const struct reg_s tvregs_576cvbs_china_sarft_m8[] = {
	{MREG_END_MARKER,		0      }
};

static const struct reg_s tvregs_576cvbs_china_telecom_m8[] = {
	{ENCI_SYNC_ADJ,				0x8060	},
	{ENCI_VIDEO_SAT,              0xfe	},
	{VENC_VDAC_DAC0_FILT_CTRL1,   0xf850	},
	{MREG_END_MARKER,		0		}
};

static const struct reg_s tvregs_576cvbs_china_mobile_m8[] = {
	{ENCI_SYNC_ADJ,				0x8060	},
	{ENCI_VIDEO_SAT,              0xfe	},
	{VENC_VDAC_DAC0_FILT_CTRL1,   0xf850	},
	{MREG_END_MARKER,		0       }
};

static const struct reg_s *tvregs_576cvbs_performance_m8[] = {
	tvregs_576cvbs_china_sarft_m8,
	tvregs_576cvbs_china_telecom_m8,
	tvregs_576cvbs_china_mobile_m8
};

static const struct reg_s tvregs_576cvbs_china_sarft_m8m2[] = {
	{ENCI_YC_DELAY,				0x343  },
	{MREG_END_MARKER,		0      }
};

static const struct reg_s tvregs_576cvbs_china_telecom_m8m2[] = {
	{ENCI_YC_DELAY,				0x343   },
	{ENCI_SYNC_ADJ,				0x8080	},
	{ENCI_VIDEO_SAT,              0xfd	},
	{VENC_VDAC_DAC0_FILT_CTRL1,   0xf850	},
	{MREG_END_MARKER,		0		}
};

static const struct reg_s tvregs_576cvbs_china_mobile_m8m2[] = {
	{ENCI_YC_DELAY,				0x343   },
	{ENCI_SYNC_ADJ,				0x8080	},
	{ENCI_VIDEO_SAT,              0xfd	},
	{VENC_VDAC_DAC0_FILT_CTRL1,   0xf850	},
	{MREG_END_MARKER,		0       }
};

static const struct reg_s *tvregs_576cvbs_performance_m8m2[] = {
	tvregs_576cvbs_china_sarft_m8m2,
	tvregs_576cvbs_china_telecom_m8m2,
	tvregs_576cvbs_china_mobile_m8m2
};

static const struct reg_s tvregs_576cvbs_china_sarft_m8b[] = {
	{ENCI_YC_DELAY,				0x343  },
	{MREG_END_MARKER,		0      }
};

static const struct reg_s tvregs_576cvbs_china_telecom_m8b[] = {
	{ENCI_YC_DELAY,				0x343   },
	{ENCI_SYNC_ADJ,				0x8080	},
	{ENCI_VIDEO_SAT,              0xfd	},
	{VENC_VDAC_DAC0_FILT_CTRL1,   0xf850	},
	{MREG_END_MARKER,		0		}
};

static const struct reg_s tvregs_576cvbs_china_mobile_m8b[] = {
	{ENCI_YC_DELAY,				0x343   },
	{ENCI_SYNC_ADJ,				0x8080	},
	{ENCI_VIDEO_SAT,              0xfd	},
	{VENC_VDAC_DAC0_FILT_CTRL1,   0xf850	},
	{MREG_END_MARKER,		0       }
};

static const struct reg_s *tvregs_576cvbs_performance_m8b[] = {
	tvregs_576cvbs_china_sarft_m8b,
	tvregs_576cvbs_china_telecom_m8b,
	tvregs_576cvbs_china_mobile_m8b
};

static const struct reg_s tvregs_576cvbs_china_sarft_gxbb[] = {
	{VENC_VDAC_DAC0_GAINCTRL,	0x9  },
	{MREG_END_MARKER,			0    }
};

static const struct reg_s tvregs_576cvbs_china_telecom_gxbb[] = {
	{VENC_VDAC_DAC0_GAINCTRL,		0x9		},
	{ENCI_SYNC_ADJ,					0x8060	},
	{ENCI_VIDEO_SAT,				0x2		},
	{VENC_VDAC_DAC0_FILT_CTRL1,		0xf94e	},
	{MREG_END_MARKER,				0		}
};

static const struct reg_s tvregs_576cvbs_china_mobile_gxbb[] = {
	{VENC_VDAC_DAC0_GAINCTRL,		0x9		},
	{ENCI_SYNC_ADJ,					0x8060	},
	{ENCI_VIDEO_SAT,				0x2		},
	{VENC_VDAC_DAC0_FILT_CTRL1,		0xf94e	},
	{MREG_END_MARKER,				0		}
};

static const struct reg_s *tvregs_576cvbs_performance_gxbb[] = {
	tvregs_576cvbs_china_sarft_gxbb,
	tvregs_576cvbs_china_telecom_gxbb,
	tvregs_576cvbs_china_mobile_gxbb
};

#endif

static const struct reg_s tvregs_720p_clk[] = {
	/* {ENCP_VIDEO_EN,              0,     }, */
	/* {ENCI_VIDEO_EN,              0,     }, */
	/* {VENC_VDAC_SETTING,          0xff,  }, */
	{HHI_VID_CLK_CNTL,           0x0,},
	{HHI_VID_PLL_CNTL2,          0x814d3928},
	{HHI_VID_PLL_CNTL3,          0x6b425012},
	{HHI_VID_PLL_CNTL4,          0x110},
	{HHI_VID_PLL_CNTL,           0x0001043e,},
	{HHI_VID_DIVIDER_CNTL,       0x00010843,},
	{HHI_VID_CLK_DIV,            0x100},
	{HHI_VID_CLK_CNTL,           0x80000,},
	{HHI_VID_CLK_CNTL,           0x88001,},
	{HHI_VID_CLK_CNTL,           0x80003,},
	{HHI_VIID_CLK_DIV,           0x00000101,},
	{HHI_HDMI_AFC_CNTL,          0x8c0000c3},
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_720p_enc[] = {
	{ENCP_VIDEO_FILT_CTRL,       0x0052,},
	{VENC_DVI_SETTING,           0x2029,},
	{ENCP_VIDEO_MODE,            0x4040,},
	{ENCP_VIDEO_MODE_ADV,        0x0019,},
	{ENCP_VIDEO_YFP1_HTIME,      648,   },
	{ENCP_VIDEO_YFP2_HTIME,      3207,  },
	{ENCP_VIDEO_MAX_PXCNT,       3299,  },
	{ENCP_VIDEO_HSPULS_BEGIN,    80,    },
	{ENCP_VIDEO_HSPULS_END,      240,   },
	{ENCP_VIDEO_HSPULS_SWITCH,   80,    },
	{ENCP_VIDEO_VSPULS_BEGIN,    688,   },
	{ENCP_VIDEO_VSPULS_END,      3248,  },
	{ENCP_VIDEO_VSPULS_BLINE,    4,     },
	{ENCP_VIDEO_VSPULS_ELINE,    8,     },
	{ENCP_VIDEO_EQPULS_BLINE,    4,     },
	{ENCP_VIDEO_EQPULS_ELINE,    8,     },
	{ENCP_VIDEO_HAVON_BEGIN,     648,   },
	{ENCP_VIDEO_HAVON_END,       3207,  },
	{ENCP_VIDEO_VAVON_BLINE,     29,    },
	{ENCP_VIDEO_VAVON_ELINE,     748,   },
	{ENCP_VIDEO_HSO_BEGIN,       256    },
	{ENCP_VIDEO_HSO_END,         168,   },
	{ENCP_VIDEO_VSO_BEGIN,       168,   },
	{ENCP_VIDEO_VSO_END,         256,   },
	{ENCP_VIDEO_VSO_BLINE,       0,     },
	{ENCP_VIDEO_VSO_ELINE,       5,     },
	{ENCP_VIDEO_MAX_LNCNT,       749,   },
	{VENC_VIDEO_PROG_MODE,       0x100, },
	{VENC_SYNC_ROUTE,            0,     },
	{VENC_INTCTRL,               0x200, },
	{ENCP_VFIFO2VD_CTL,               0,     },
	{VENC_VDAC_SETTING,          0,     },
	{VENC_UPSAMPLE_CTRL0,        0x9061,},
	{VENC_UPSAMPLE_CTRL1,        0xa061,},
	{VENC_UPSAMPLE_CTRL2,        0xb061,},
	{VENC_VDAC_DACSEL0,          0x0001,},
	{VENC_VDAC_DACSEL1,          0x0001,},
	{VENC_VDAC_DACSEL2,          0x0001,},
	{VENC_VDAC_DACSEL3,          0x0001,},
	{VENC_VDAC_DACSEL4,          0x0001,},
	{VENC_VDAC_DACSEL5,          0x0001,},
	{VPU_VIU_VENC_MUX_CTRL,      0x000a,},
	{VENC_VDAC_FIFO_CTRL,        0x1000,},
	{ENCP_DACSEL_0,              0x3102,},
	{ENCP_DACSEL_1,              0x0054,},
	{ENCP_VIDEO_EN,              1,     },
	{ENCI_VIDEO_EN,              0,     },
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_720p_50hz_clk[] = {
	/* {ENCP_VIDEO_EN,              0,     }, */
	/* {ENCI_VIDEO_EN,              0,     }, */
	/* {VENC_VDAC_SETTING,          0xff,  }, */
	{HHI_VID_CLK_CNTL,           0x0,},
	{HHI_VID_PLL_CNTL2,          0x814d3928},
	{HHI_VID_PLL_CNTL3,          0x6b425012},
	{HHI_VID_PLL_CNTL4,          0x110},
	{HHI_VID_PLL_CNTL,           0x0001043e,},
	{HHI_VID_DIVIDER_CNTL,       0x00010843,},
	{HHI_VID_CLK_DIV,            0x100},
	{HHI_VID_CLK_CNTL,           0x80000,},
	{HHI_VID_CLK_CNTL,           0x88001,},
	{HHI_VID_CLK_CNTL,           0x80003,},
	{HHI_VIID_CLK_DIV,           0x00000101,},
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_720p_50hz_enc[] = {
	{ENCP_VIDEO_FILT_CTRL,       0x0052,},
	{VENC_DVI_SETTING,           0x202d,},
	{ENCP_VIDEO_MAX_PXCNT,       3959,  },
	{ENCP_VIDEO_MAX_LNCNT,       749,   },

	/* analog vidoe position in horizontal */
	{ENCP_VIDEO_HSPULS_BEGIN,    80,    },
	{ENCP_VIDEO_HSPULS_END,      240,   },
	{ENCP_VIDEO_HSPULS_SWITCH,   80,    },

	/* DE position in horizontal */
	{ENCP_VIDEO_HAVON_BEGIN,     648,   },
	{ENCP_VIDEO_HAVON_END,       3207,  },

	/* ditital hsync positon in horizontal */
	{ENCP_VIDEO_HSO_BEGIN,       128 ,},
	{ENCP_VIDEO_HSO_END,         208 , },

	/* vsync horizontal timing */
	{ENCP_VIDEO_VSPULS_BEGIN,    688,   },
	{ENCP_VIDEO_VSPULS_END,      3248,  },

	/* vertical timing settings */
	{ENCP_VIDEO_VSPULS_BLINE,    4,     },
	{ENCP_VIDEO_VSPULS_ELINE,    8,     },
	{ENCP_VIDEO_EQPULS_BLINE,    4,     },
	{ENCP_VIDEO_EQPULS_ELINE,    8,     },

	/* DE position in vertical */
	{ENCP_VIDEO_VAVON_BLINE,     29,    },
	{ENCP_VIDEO_VAVON_ELINE,     748,   },

	/* adjust the vsync start point and end point */
	{ENCP_VIDEO_VSO_BEGIN,       128,},  /* 168, */
	{ENCP_VIDEO_VSO_END,         128, },  /* 256 */

	/* adjust the vsync start line and end line */
	{ENCP_VIDEO_VSO_BLINE,       0,     },
	{ENCP_VIDEO_VSO_ELINE,       5,     },

	/* filter & misc settings */
	{ENCP_VIDEO_YFP1_HTIME,      648,   },
	{ENCP_VIDEO_YFP2_HTIME,      3207,  },


	{VENC_VIDEO_PROG_MODE,       0x100, },
	/* Enable Hsync and equalization pulse switch in center */
	{ENCP_VIDEO_MODE,            0x4040,},
	/* bit6:swap PbPr; bit4:YPBPR gain as HDTV type; */
	/* bit3:Data input from VFIFO;bit[2}0]:repreat pixel a time */
	{ENCP_VIDEO_MODE_ADV,        0x0019,},

	/* Video input Synchronization mode */
	/* bit[7:0] -- 4:Slave mode, 7:Master mode */
	/* bit[15:6] -- adjust the vsync vertical position */
	{ENCP_VIDEO_SYNC_MODE,       0x407,  },
	/* Y/Cb/Cr delay */
	{ENCP_VIDEO_YC_DLY,          0,     },
	{VENC_SYNC_ROUTE,            0,     },
	{VENC_INTCTRL,               0x200, },
	{ENCP_VFIFO2VD_CTL,               0,     },
	{VENC_VDAC_SETTING,          0,     },
	{VPU_VIU_VENC_MUX_CTRL,      0x000a,},
	{VENC_VDAC_FIFO_CTRL,        0x1000,},
	{ENCP_DACSEL_0,              0x3102,},
	{ENCP_DACSEL_1,              0x0054,},
	{VENC_VDAC_DACSEL0,          0x0001,},
	{VENC_VDAC_DACSEL1,          0x0001,},
	{VENC_VDAC_DACSEL2,          0x0001,},
	{VENC_VDAC_DACSEL3,          0x0001,},
	{VENC_VDAC_DACSEL4,          0x0001,},
	{VENC_VDAC_DACSEL5,          0x0001,},
	{ENCP_VIDEO_EN,              1,     },
	{ENCI_VIDEO_EN,              0,     },
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_480i_clk[] = {
	{HHI_VID_CLK_CNTL,           0x0,       },
	{HHI_VID_PLL_CNTL,           0x2001042d,},
	{HHI_VID_PLL_CNTL2,          0x814d3928,},
	{HHI_VID_PLL_CNTL3,          0x6b425012,    },
	{HHI_VID_PLL_CNTL4,          0x110},
	{HHI_VID_PLL_CNTL,           0x0001042d,},

	{HHI_VID_DIVIDER_CNTL,       0x00011943,},
	{HHI_VID_CLK_DIV,            0x100},
	{HHI_VID_CLK_CNTL,           0x80000,},
	{HHI_VID_CLK_CNTL,           0x88001,},
	{HHI_VID_CLK_CNTL,           0x80003,},
	{HHI_VIID_CLK_DIV,           0x00000101,},
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_480i_enc[] = {
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
	{VPU_VIU_VENC_MUX_CTRL,        0x0005,},
	{VENC_VDAC_FIFO_CTRL,          0x2000,},
	{ENCI_DACSEL_0,                0x0011 },
	{ENCI_DACSEL_1,                0x87   },
	{ENCP_VIDEO_EN,                0,     },
	{ENCI_VIDEO_EN,                1,     },
	{MREG_END_MARKER,              0      }
};

static const struct reg_s tvregs_480cvbs_clk[] = {
	{HHI_VID_CLK_CNTL,           0x0,       },
	{HHI_VID_PLL_CNTL,           0x2001042d,},
	{HHI_VID_PLL_CNTL2,          0x814d3928,},
	{HHI_VID_PLL_CNTL3,          0x6b425012,    },
	{HHI_VID_PLL_CNTL4,          0x110},
	{HHI_VID_PLL_CNTL,           0x0001042d,},

	{HHI_VID_DIVIDER_CNTL,       0x00011943,},
	{HHI_VID_CLK_DIV,            0x100},
	{HHI_VID_CLK_CNTL,           0x80000,},
	{HHI_VID_CLK_CNTL,           0x88001,},
	{HHI_VID_CLK_CNTL,           0x80003,},
	{HHI_VIID_CLK_DIV,           0x00000101,},
	{MREG_END_MARKER,              0      }
};

static const struct reg_s tvregs_480cvbs_enc[] = {
	{ENCI_CFILT_CTRL,              0x12,  },
	{ENCI_CFILT_CTRL2,             0x12,  },
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
	{ENCI_VFIFO2VD_PIXEL_START,    0xe3,  },
	{ENCI_VFIFO2VD_PIXEL_END,      0x0683,},
	{ENCI_VFIFO2VD_LINE_TOP_START, 0x12,  },
	{ENCI_VFIFO2VD_LINE_TOP_END,   0x102, },
	{ENCI_VFIFO2VD_LINE_BOT_START, 0x13,  },
	{ENCI_VFIFO2VD_LINE_BOT_END,   0x103, },
	{VENC_SYNC_ROUTE,              0,     },
	{ENCI_DBG_PX_RST,              0,     },
	{VENC_INTCTRL,                 0x2,   },
	{ENCI_VFIFO2VD_CTL,            0x4e01,},
	{VENC_VDAC_SETTING,            0,     },
	{VENC_UPSAMPLE_CTRL0,          0x0061,},
	{VENC_UPSAMPLE_CTRL1,          0x4061,},
	{VENC_UPSAMPLE_CTRL2,          0x5061,},
	{VENC_VDAC_DACSEL0,            0x0000,},
	{VENC_VDAC_DACSEL1,            0x0000,},
	{VENC_VDAC_DACSEL2,            0x0000,},
	{VENC_VDAC_DACSEL3,            0x0000,},
	{VENC_VDAC_DACSEL4,            0x0000,},
	{VENC_VDAC_DACSEL5,            0x0000,},
	{VPU_VIU_VENC_MUX_CTRL,        0x0005,},
	{VENC_VDAC_FIFO_CTRL,          0x2000,},
	{ENCI_DACSEL_0,                0x0011 },
	{ENCI_DACSEL_1,                0x11   },
	{ENCP_VIDEO_EN,                0,     },
	{ENCI_VIDEO_EN,                1,     },
	{ENCI_VIDEO_SAT,               0x12   },
	{VENC_VDAC_DAC0_FILT_CTRL0,    0x1    },
	{VENC_VDAC_DAC0_FILT_CTRL1,    0xfc48 },
	{ENCI_MACV_N0,                 0x0    },
	{ENCI_SYNC_ADJ,                0x9c00 },
	{ENCI_VIDEO_CONT,              0x3    },
	{MREG_END_MARKER,              0      }
};

static const struct reg_s tvregs_480p_clk[] = {
	/* {ENCP_VIDEO_EN,              0,     }, */
	/* {ENCI_VIDEO_EN,              0,     }, */
	{VENC_VDAC_SETTING,          0xff,  },
	{HHI_VID_CLK_CNTL,           0x0,       },
	{HHI_VID_PLL_CNTL,           0x2001042d,},
	{HHI_VID_PLL_CNTL2,          0x814d3928,},
	{HHI_VID_PLL_CNTL3,          0x6b425012,    },
	{HHI_VID_PLL_CNTL4,          0x110},
	{HHI_VID_PLL_CNTL,           0x0001042d,},

	{HHI_VID_DIVIDER_CNTL,       0x00011943,},
	{HHI_VID_CLK_DIV,            0x100},
	{HHI_VID_CLK_CNTL,           0x80000,},
	{HHI_VID_CLK_CNTL,           0x88001,},
	{HHI_VID_CLK_CNTL,           0x80003,},
	{HHI_VIID_CLK_DIV,           0x00000101,},
	/* {HHI_VID_CLK_DIV,            0x01000100,}, */
	{MREG_END_MARKER,              0      }
};

static const struct reg_s tvregs_480p_enc[] = {
	{ENCP_VIDEO_FILT_CTRL,       0x2052,},
	{VENC_DVI_SETTING,           0x21,  },
	{ENCP_VIDEO_MODE,            0x4000,},
	{ENCP_VIDEO_MODE_ADV,        9,     },
	{ENCP_VIDEO_YFP1_HTIME,      244,   },
	{ENCP_VIDEO_YFP2_HTIME,      1630,  },
	{ENCP_VIDEO_YC_DLY,          0,     },
	{ENCP_VIDEO_MAX_PXCNT,       1715,  },
	{ENCP_VIDEO_MAX_LNCNT,       524,   },
	{ENCP_VIDEO_HSPULS_BEGIN,    0x22,  },
	{ENCP_VIDEO_HSPULS_END,      0xa0,  },
	{ENCP_VIDEO_HSPULS_SWITCH,   88,    },
	{ENCP_VIDEO_VSPULS_BEGIN,    0,     },
	{ENCP_VIDEO_VSPULS_END,      1589   },
	{ENCP_VIDEO_VSPULS_BLINE,    0,     },
	{ENCP_VIDEO_VSPULS_ELINE,    5,     },
	{ENCP_VIDEO_HAVON_BEGIN,     249,   },
	{ENCP_VIDEO_HAVON_END,       1689,  },
	{ENCP_VIDEO_VAVON_BLINE,     42,    },
	{ENCP_VIDEO_VAVON_ELINE,     521,   },
	{ENCP_VIDEO_SYNC_MODE,       0x07,  },
	{VENC_VIDEO_PROG_MODE,       0x0,   },
	{VENC_VIDEO_EXSRC,           0x0,   },
	{ENCP_VIDEO_HSO_BEGIN,       0x3,   },
	{ENCP_VIDEO_HSO_END,         0x5,   },
	{ENCP_VIDEO_VSO_BEGIN,       0x3,   },
	{ENCP_VIDEO_VSO_END,         0x5,   },
	/* added by JZD. Switch Panel to 480p first time */
	/* movie video flicks if not set this to 0 */
	{ENCP_VIDEO_VSO_BLINE,       0,     },
	{ENCP_VIDEO_SY_VAL,          8,     },
	{ENCP_VIDEO_SY2_VAL,         0x1d8, },
	{VENC_SYNC_ROUTE,            0,     },
	{VENC_INTCTRL,               0x200, },
	{ENCP_VFIFO2VD_CTL,               0,     },
	{VENC_VDAC_SETTING,          0,     },
	{VENC_UPSAMPLE_CTRL0,        0x9061,},
	{VENC_UPSAMPLE_CTRL1,        0xa061,},
	{VENC_UPSAMPLE_CTRL2,        0xb061,},
	{VENC_VDAC_DACSEL0,          0xf003,},
	{VENC_VDAC_DACSEL1,          0xf003,},
	{VENC_VDAC_DACSEL2,          0xf003,},
	{VENC_VDAC_DACSEL3,          0xf003,},
	{VENC_VDAC_DACSEL4,          0xf003,},
	{VENC_VDAC_DACSEL5,          0xf003,},
	{VPU_VIU_VENC_MUX_CTRL,      0x000a,},
	{VENC_VDAC_FIFO_CTRL,        0x1000,},
	{ENCP_DACSEL_0,              0x3102,},
	{ENCP_DACSEL_1,              0x0054,},
	{ENCI_VIDEO_EN,              0      },
	{ENCP_VIDEO_EN,              1      },
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_576i_clk[] = {
	{HHI_VID_CLK_CNTL,           0x0,       },
	{HHI_VID_PLL_CNTL,           0x2001042d,},
	{HHI_VID_PLL_CNTL2,          0x814d3928,},
	{HHI_VID_PLL_CNTL3,          0x6b425012,    },
	{HHI_VID_PLL_CNTL4,          0x110},
	{HHI_VID_PLL_CNTL,           0x0001042d,},

	{HHI_VID_DIVIDER_CNTL,       0x00011943,},
	{HHI_VID_CLK_DIV,            0x100},
	{HHI_VID_CLK_CNTL,           0x80000,},
	{HHI_VID_CLK_CNTL,           0x88001,},
	{HHI_VID_CLK_CNTL,           0x80003,},
	{HHI_VIID_CLK_DIV,           0x00000101,},
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_576i_enc[] = {
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
	{VPU_VIU_VENC_MUX_CTRL,           0x0005,    },
	{VENC_VDAC_FIFO_CTRL,             0x2000,    },
	{ENCI_DACSEL_0,                   0x0011     },
	{ENCI_DACSEL_1,                   0x87       },
	{ENCP_VIDEO_EN,                   0,         },
	{ENCI_VIDEO_EN,                   1,         },
	{MREG_END_MARKER,                 0          }
};

static const struct reg_s tvregs_576cvbs_clk[] = {
	{HHI_VID_CLK_CNTL,           0x0,       },
	{HHI_VID_PLL_CNTL,           0x2001042d,},
	{HHI_VID_PLL_CNTL2,          0x814d3928,},
	{HHI_VID_PLL_CNTL3,          0x6b425012,    },
	{HHI_VID_PLL_CNTL4,          0x110},
	{HHI_VID_PLL_CNTL,           0x0001042d,},

	{HHI_VID_DIVIDER_CNTL,       0x00011943,},
	{HHI_VID_CLK_DIV,            0x100},
	{HHI_VID_CLK_CNTL,           0x80000,},
	{HHI_VID_CLK_CNTL,           0x88001,},
	{HHI_VID_CLK_CNTL,           0x80003,},
	{HHI_VIID_CLK_DIV,           0x00000101,},
	{MREG_END_MARKER,                 0          }
};

static const struct reg_s tvregs_576cvbs_enc[] = {
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
	{ENCI_VFIFO2VD_PIXEL_START,       0x0fb	   },
	{ENCI_VFIFO2VD_PIXEL_END,         0x069b     },
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
	{VPU_VIU_VENC_MUX_CTRL,           0x0005,    },
	{VENC_VDAC_FIFO_CTRL,             0x2000,    },
	{ENCI_DACSEL_0,                   0x0011     },
	{ENCI_DACSEL_1,                   0x11       },
	{ENCP_VIDEO_EN,                   0,         },
	{ENCI_VIDEO_EN,                   1,         },
	{ENCI_VIDEO_SAT,                  0x7        },
	{VENC_VDAC_DAC0_FILT_CTRL0,       0x1        },
	{VENC_VDAC_DAC0_FILT_CTRL1,       0xfc48     },
	{ENCI_MACV_N0,                    0x0        },
	{MREG_END_MARKER,                 0          }
};

static const struct reg_s tvregs_576p_clk[] = {
	{HHI_VID_CLK_CNTL,           0x0,       },
	{HHI_VID_PLL_CNTL,           0x2001042d,},
	{HHI_VID_PLL_CNTL2,          0x814d3928,},
	{HHI_VID_PLL_CNTL3,          0x6b425012,    },
	{HHI_VID_PLL_CNTL4,          0x110},
	{HHI_VID_PLL_CNTL,           0x0001042d,},

	{HHI_VID_DIVIDER_CNTL,       0x00011943,},
	{HHI_VID_CLK_DIV,            0x100},
	{HHI_VID_CLK_CNTL,           0x80000,},
	{HHI_VID_CLK_CNTL,           0x88001,},
	{HHI_VID_CLK_CNTL,           0x80003,},
	{HHI_VIID_CLK_DIV,           0x00000101,},
	{HHI_HDMI_AFC_CNTL,          0x8c0000c3,},
	{MREG_END_MARKER,            0          }
};

static const struct reg_s tvregs_576p_enc[] = {
	{ENCP_VIDEO_FILT_CTRL,       0x52,      },
	{VENC_DVI_SETTING,           0x21,      },
	{ENCP_VIDEO_MODE,            0x4000,    },
	{ENCP_VIDEO_MODE_ADV,        9,         },
	{ENCP_VIDEO_YFP1_HTIME,      235,       },
	{ENCP_VIDEO_YFP2_HTIME,      1674,      },
	{ENCP_VIDEO_YC_DLY,          0xf,       },
	{ENCP_VIDEO_MAX_PXCNT,       1727,      },
	{ENCP_VIDEO_MAX_LNCNT,       624,       },
	{ENCP_VIDEO_HSPULS_BEGIN,    0,         },
	{ENCP_VIDEO_HSPULS_END,      0x80,      },
	{ENCP_VIDEO_HSPULS_SWITCH,   88,        },
	{ENCP_VIDEO_VSPULS_BEGIN,    0,         },
	{ENCP_VIDEO_VSPULS_END,      1599       },
	{ENCP_VIDEO_VSPULS_BLINE,    0,         },
	{ENCP_VIDEO_VSPULS_ELINE,    4,         },
	{ENCP_VIDEO_HAVON_BEGIN,     235,       },
	{ENCP_VIDEO_HAVON_END,       1674,      },
	{ENCP_VIDEO_VAVON_BLINE,     44,        },
	{ENCP_VIDEO_VAVON_ELINE,     619,       },
	{ENCP_VIDEO_SYNC_MODE,       0x07,      },
	{VENC_VIDEO_PROG_MODE,       0x0,       },
	{VENC_VIDEO_EXSRC,           0x0,       },
	{ENCP_VIDEO_HSO_BEGIN,       0x80,      },
	{ENCP_VIDEO_HSO_END,         0x0,       },
	{ENCP_VIDEO_VSO_BEGIN,       0x0,       },
	{ENCP_VIDEO_VSO_END,         0x5,       },
	{ENCP_VIDEO_VSO_BLINE,       0,         },
	{ENCP_VIDEO_SY_VAL,          8,         },
	{ENCP_VIDEO_SY2_VAL,         0x1d8,     },
	{VENC_SYNC_ROUTE,            0,         },
	{VENC_INTCTRL,               0x200,     },
	{ENCP_VFIFO2VD_CTL,               0,         },
	{VENC_VDAC_SETTING,          0,     },
	{VENC_UPSAMPLE_CTRL0,        0x9061,    },
	{VENC_UPSAMPLE_CTRL1,        0xa061,    },
	{VENC_UPSAMPLE_CTRL2,        0xb061,    },
	{VENC_VDAC_DACSEL0,          0xf003,    },
	{VENC_VDAC_DACSEL1,          0xf003,    },
	{VENC_VDAC_DACSEL2,          0xf003,    },
	{VENC_VDAC_DACSEL3,          0xf003,    },
	{VENC_VDAC_DACSEL4,          0xf003,    },
	{VENC_VDAC_DACSEL5,          0xf003,    },
	{VPU_VIU_VENC_MUX_CTRL,      0x000a,    },
	{VENC_VDAC_FIFO_CTRL,        0x1000,    },
	{ENCP_DACSEL_0,              0x3102,    },
	{ENCP_DACSEL_1,              0x0054,    },
	{ENCI_VIDEO_EN,              0          },
	{ENCP_VIDEO_EN,              1          },
	{MREG_END_MARKER,            0          }
};

static const struct reg_s tvregs_1080i_clk[] = {
	{HHI_VID_CLK_CNTL,           0x0,},
	{HHI_VID_PLL_CNTL2,          0x814d3928},
	{HHI_VID_PLL_CNTL3,          0x6b425012},
	{HHI_VID_PLL_CNTL4,          0x110},
	{HHI_VID_PLL_CNTL,           0x0001043e,},
	{HHI_VID_DIVIDER_CNTL,       0x00010843,},
	{HHI_VID_CLK_DIV,            0x100},
	{HHI_VID_CLK_CNTL,           0x80000,},
	{HHI_VID_CLK_CNTL,           0x88001,},
	{HHI_VID_CLK_CNTL,           0x80003,},
	{HHI_VIID_CLK_DIV,           0x00000101,},
	{MREG_END_MARKER,            0          }
};

static const struct reg_s tvregs_1080i_enc[] = {
	{ENCP_VIDEO_FILT_CTRL,       0x0052,},
	{VENC_DVI_SETTING,           0x2029,},
	{ENCP_VIDEO_MAX_PXCNT,       4399,  },
	{ENCP_VIDEO_MAX_LNCNT,       1124,  },
	{ENCP_VIDEO_HSPULS_BEGIN,    88,    },
	{ENCP_VIDEO_HSPULS_END,      264,   },
	{ENCP_VIDEO_HSPULS_SWITCH,   88,    },
	{ENCP_VIDEO_HAVON_BEGIN,     516,   },
	{ENCP_VIDEO_HAVON_END,       4355,  },
	{ENCP_VIDEO_HSO_BEGIN,       264,   },
	{ENCP_VIDEO_HSO_END,         176,   },
	{ENCP_VIDEO_EQPULS_BEGIN,    2288,  },
	{ENCP_VIDEO_EQPULS_END,      2464,  },
	{ENCP_VIDEO_VSPULS_BEGIN,    440,   },
	{ENCP_VIDEO_VSPULS_END,      2200,  },
	{ENCP_VIDEO_VSPULS_BLINE,    0,     },
	{ENCP_VIDEO_VSPULS_ELINE,    4,     },
	{ENCP_VIDEO_EQPULS_BLINE,    0,     },
	{ENCP_VIDEO_EQPULS_ELINE,    4,     },
	{ENCP_VIDEO_VAVON_BLINE,     20,    },
	{ENCP_VIDEO_VAVON_ELINE,     559,   },
	{ENCP_VIDEO_VSO_BEGIN,       88,    },
	{ENCP_VIDEO_VSO_END,         88,    },
	{ENCP_VIDEO_VSO_BLINE,       0,     },
	{ENCP_VIDEO_VSO_ELINE,       5,     },
	{ENCP_VIDEO_YFP1_HTIME,      516,   },
	{ENCP_VIDEO_YFP2_HTIME,      4355,  },
	{VENC_VIDEO_PROG_MODE,       0x100, },
	{ENCP_VIDEO_OFLD_VOAV_OFST,  0x11   },
	{ENCP_VIDEO_MODE,            0x5ffc,},
	{ENCP_VIDEO_MODE_ADV,        0x0019,},
	{ENCP_VIDEO_SYNC_MODE,       0x207, },
	{VENC_SYNC_ROUTE,            0,     },
	{VENC_INTCTRL,               0x200, },
	{ENCP_VFIFO2VD_CTL,               0,     },
	{VENC_VDAC_FIFO_CTRL,        0x1000,},
	{VENC_VDAC_SETTING,          0,     },
	{ENCP_DACSEL_0,              0x3102,},
	{ENCP_DACSEL_1,              0x0054,},
	{VENC_VDAC_DACSEL0,          0x0001,},
	{VENC_VDAC_DACSEL1,          0x0001,},
	{VENC_VDAC_DACSEL2,          0x0001,},
	{VENC_VDAC_DACSEL3,          0x0001,},
	{VENC_VDAC_DACSEL4,          0x0001,},
	{VENC_VDAC_DACSEL5,          0x0001,},
	{ENCI_VIDEO_EN,              0,     },
	{ENCP_VIDEO_EN,              1,     },
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_1080i_50hz_clk[] = {
	{HHI_VID_CLK_CNTL,           0x0,},
	{HHI_VID_PLL_CNTL2,          0x814d3928},
	{HHI_VID_PLL_CNTL3,          0x6b425012},
	{HHI_VID_PLL_CNTL4,          0x110},
	{HHI_VID_PLL_CNTL,           0x0001043e,},
	{HHI_VID_DIVIDER_CNTL,       0x00010843,},
	{HHI_VID_CLK_DIV,            0x100},
	{HHI_VID_CLK_CNTL,           0x80000,},
	{HHI_VID_CLK_CNTL,           0x88001,},
	{HHI_VID_CLK_CNTL,           0x80003,},
	{HHI_VIID_CLK_DIV,           0x00000101,},
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_1080i_50hz_enc[] = {
	{ENCP_VIDEO_FILT_CTRL,       0x0052,},
	{VENC_DVI_SETTING,           0x202d,},
	{ENCP_VIDEO_MAX_PXCNT,       5279,  },
	{ENCP_VIDEO_MAX_LNCNT,       1124,  },

	/* analog vidoe position in horizontal */
	{ENCP_VIDEO_HSPULS_BEGIN,    88,    },
	{ENCP_VIDEO_HSPULS_END,      264,   },
	{ENCP_VIDEO_HSPULS_SWITCH,   88,    },

	/* DE position in horizontal */
	{ENCP_VIDEO_HAVON_BEGIN,     526,   },
	{ENCP_VIDEO_HAVON_END,       4365,  },

	/* ditital hsync positon in horizontal */
	{ENCP_VIDEO_HSO_BEGIN,       142,   },
	{ENCP_VIDEO_HSO_END,         230,   },

	/* vsync horizontal timing */
	{ENCP_VIDEO_EQPULS_BEGIN,    2728,  },
	{ENCP_VIDEO_EQPULS_END,      2904,  },
	{ENCP_VIDEO_VSPULS_BEGIN,    440,   },
	{ENCP_VIDEO_VSPULS_END,      2200,  },

	{ENCP_VIDEO_VSPULS_BLINE,    0,     },
	{ENCP_VIDEO_VSPULS_ELINE,    4,     },
	{ENCP_VIDEO_EQPULS_BLINE,    0,     },
	{ENCP_VIDEO_EQPULS_ELINE,    4,     },

	/* DE position in vertical */
	{ENCP_VIDEO_VAVON_BLINE,     20,    },
	{ENCP_VIDEO_VAVON_ELINE,     559,   },

	/* adjust vsync start point and end point */
	{ENCP_VIDEO_VSO_BEGIN,       142,    },
	{ENCP_VIDEO_VSO_END,         142,    },

	/* adjust the vsync start line and end line */
	{ENCP_VIDEO_VSO_BLINE,       0,     },
	{ENCP_VIDEO_VSO_ELINE,       5,     },

	/* filter & misc settings */
	{ENCP_VIDEO_YFP1_HTIME,      526,   },
	{ENCP_VIDEO_YFP2_HTIME,      4365,  },

	/* Select clk108 as DAC clock, progressive mode */
	{VENC_VIDEO_PROG_MODE,       0x100, },
	{ENCP_VIDEO_OFLD_VOAV_OFST,  0x11   },
	/* bit[15:12]: Odd field VSO  offset begin, */
	/* bit[11:8]: Odd field VSO  offset end, */
	/* bit[7:4]: Odd field VAVON offset begin, */
	/* bit[3:0]: Odd field VAVON offset end, */

	/* Enable Hsync and equalization pulse switch in center */
	{ENCP_VIDEO_MODE,            0x5ffc,},
	{ENCP_VIDEO_MODE_ADV,        0x0019,},
	/* bit6:swap PbPr; bit4:YPBPR gain as HDTV type; */
	/* bit3:Data input from VFIFO;bit[2}0]:repreat pixel a time */
	{ENCP_VIDEO_SYNC_MODE,       0x7, },
	/* bit[15:8] -- adjust the vsync vertical position */
	{VENC_SYNC_ROUTE,            0,     },
	{VENC_INTCTRL,               0x200, },
	{ENCP_VFIFO2VD_CTL,               0,     },
	{VENC_VDAC_FIFO_CTRL,        0x1000,},
	{VENC_VDAC_SETTING,          0,     },
	{ENCP_DACSEL_0,              0x3102,},
	{ENCP_DACSEL_1,              0x0054,},
	{VENC_VDAC_DACSEL0,          0x0001,},
	{VENC_VDAC_DACSEL1,          0x0001,},
	{VENC_VDAC_DACSEL2,          0x0001,},
	{VENC_VDAC_DACSEL3,          0x0001,},
	{VENC_VDAC_DACSEL4,          0x0001,},
	{VENC_VDAC_DACSEL5,          0x0001,},
	{ENCI_VIDEO_EN,              0,     },
	{ENCP_VIDEO_EN,              1,     },
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_1080p_clk[] = {
	{HHI_VID_CLK_CNTL,           0x0,},
	{HHI_VID_PLL_CNTL2,          0x814d3928},
	{HHI_VID_PLL_CNTL3,          0x6b425012},
	{HHI_VID_PLL_CNTL4,          0x110},
	{HHI_VID_PLL_CNTL,           0x0001043e,},
	{HHI_VID_DIVIDER_CNTL,       0x00010843,},
	{HHI_VID_CLK_DIV,            0x100},
	{HHI_VID_CLK_CNTL,           0x80000,},
	{HHI_VID_CLK_CNTL,           0x88001,},
	{HHI_VID_CLK_CNTL,           0x80003,},
	{HHI_VIID_CLK_DIV,           0x00000101,},
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_1080p_enc[] = {
	{ENCP_VIDEO_FILT_CTRL,       0x1052,},
	{VENC_DVI_SETTING,           0x0001,},
	{ENCP_VIDEO_MODE,            0x4040,},
	{ENCP_VIDEO_MODE_ADV,        0x0018,},
	{ENCP_VIDEO_YFP1_HTIME,      140,   },
	{ENCP_VIDEO_YFP2_HTIME,      2060,  },
	{ENCP_VIDEO_MAX_PXCNT,       2199,  },
	{ENCP_VIDEO_HSPULS_BEGIN,    2156,  },/* 1980 */
	{ENCP_VIDEO_HSPULS_END,      44,    },
	{ENCP_VIDEO_HSPULS_SWITCH,   44,    },
	{ENCP_VIDEO_VSPULS_BEGIN,    140,   },
	{ENCP_VIDEO_VSPULS_END,      2059,  },
	{ENCP_VIDEO_VSPULS_BLINE,    0,     },
	{ENCP_VIDEO_VSPULS_ELINE,    4,     },/* 35 */
	{ENCP_VIDEO_HAVON_BEGIN,     148,   },
	{ENCP_VIDEO_HAVON_END,       2067,  },
	{ENCP_VIDEO_VAVON_BLINE,     41,    },
	{ENCP_VIDEO_VAVON_ELINE,     1120,  },
	{ENCP_VIDEO_HSO_BEGIN,       44,    },
	{ENCP_VIDEO_HSO_END,         2156,  },
	{ENCP_VIDEO_VSO_BEGIN,       2100,  },
	{ENCP_VIDEO_VSO_END,         2164,  },
	{ENCP_VIDEO_VSO_BLINE,       0,     },
	{ENCP_VIDEO_VSO_ELINE,       5,     },
	{ENCP_VIDEO_MAX_LNCNT,       1124,  },
	/*
	 * New Add. If not set, when system boots up,
	 * switch panel to HDMI 1080P, nothing on TV.
	 */
	{VPU_VIU_VENC_MUX_CTRL,      0x000a,},
	{VENC_VIDEO_PROG_MODE,       0x100, },
	{VENC_SYNC_ROUTE,            0,     },
	{VENC_INTCTRL,               0x200, },
	{ENCP_VFIFO2VD_CTL,               0,     },
	{VENC_VDAC_SETTING,          0,     },
	{VENC_VDAC_DACSEL0,          0x0001,},
	{VENC_VDAC_DACSEL1,          0x0001,},
	{VENC_VDAC_DACSEL2,          0x0001,},
	{VENC_VDAC_DACSEL3,          0x0001,},
	{VENC_VDAC_DACSEL4,          0x0001,},
	{VENC_VDAC_DACSEL5,          0x0001,},
	{VENC_VDAC_FIFO_CTRL,        0x1000,},
	{ENCP_DACSEL_0,              0x3102,},
	{ENCP_DACSEL_1,              0x0054,},
	{ENCI_VIDEO_EN,              0,     },
	{ENCP_VIDEO_EN,              1,     },
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_1080p_50hz_clk[] = {
	{HHI_VID_CLK_CNTL,           0x0,},
	{HHI_VID_PLL_CNTL2,          0x814d3928},
	{HHI_VID_PLL_CNTL3,          0x6b425012},
	{HHI_VID_PLL_CNTL4,          0x110},
	{HHI_VID_PLL_CNTL,           0x0001043e,},
	{HHI_VID_DIVIDER_CNTL,       0x00010843,},
	{HHI_VID_CLK_DIV,            0x100},
	{HHI_VID_CLK_CNTL,           0x80000,},
	{HHI_VID_CLK_CNTL,           0x88001,},
	{HHI_VID_CLK_CNTL,           0x80003,},
	{HHI_VIID_CLK_DIV,           0x00000101,},
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_1080p_50hz_enc[] = {
	{ENCP_VIDEO_FILT_CTRL,       0x1052,},

	/* bit 13    1          (delayed prog_vs) */
	/* bit 5:4:  2          (pixel[0]) */
	/* bit 3:    1          invert vsync or not */
	/* bit 2:    1          invert hsync or not */
	/* bit1:     1          (select viu sync) */
	/* bit0:     1          (progressive) */
	{VENC_DVI_SETTING,           0x000d,},
	{ENCP_VIDEO_MAX_PXCNT,       2639,  },
	{ENCP_VIDEO_MAX_LNCNT,       1124,  },
	/* horizontal timing settings */
	{ENCP_VIDEO_HSPULS_BEGIN,    44,  },/* 1980 */
	{ENCP_VIDEO_HSPULS_END,      132,    },
	{ENCP_VIDEO_HSPULS_SWITCH,   44,    },

	/* DE position in horizontal */
	{ENCP_VIDEO_HAVON_BEGIN,     271,   },
	{ENCP_VIDEO_HAVON_END,       2190,  },

	/* ditital hsync positon in horizontal */
	{ENCP_VIDEO_HSO_BEGIN,       79 ,    },
	{ENCP_VIDEO_HSO_END,         123,  },

	/* vsync horizontal timing */
	{ENCP_VIDEO_VSPULS_BEGIN,    220,   },
	{ENCP_VIDEO_VSPULS_END,      2140,  },

	/* vertical timing settings */
	{ENCP_VIDEO_VSPULS_BLINE,    0,     },
	{ENCP_VIDEO_VSPULS_ELINE,    4,     },/* 35 */
	{ENCP_VIDEO_EQPULS_BLINE,    0,     },
	{ENCP_VIDEO_EQPULS_ELINE,    4,     },/* 35 */
	{ENCP_VIDEO_VAVON_BLINE,     41,    },
	{ENCP_VIDEO_VAVON_ELINE,     1120,  },

	/* adjust the hsync & vsync start point and end point */
	{ENCP_VIDEO_VSO_BEGIN,       79,  },
	{ENCP_VIDEO_VSO_END,         79,  },

	/* adjust the vsync start line and end line */
	{ENCP_VIDEO_VSO_BLINE,       0,     },
	{ENCP_VIDEO_VSO_ELINE,       5,     },

	{ENCP_VIDEO_YFP1_HTIME,      271,   },
	{ENCP_VIDEO_YFP2_HTIME,      2190,  },
	{VENC_VIDEO_PROG_MODE,       0x100, },
	{ENCP_VIDEO_MODE,            0x4040,},
	{ENCP_VIDEO_MODE_ADV,        0x0018,},

	/* bit[15:8] -- adjust the vsync vertical position */
	{ENCP_VIDEO_SYNC_MODE,       0x7, },
	/* Y/Cb/Cr delay */
	{ENCP_VIDEO_YC_DLY,          0,     },
	/* enable sync on B */
	{ENCP_VIDEO_RGB_CTRL, 2,},
	{VENC_SYNC_ROUTE,            0,     },
	{VENC_INTCTRL,               0x200, },
	{ENCP_VFIFO2VD_CTL,               0,     },
	{VENC_VDAC_FIFO_CTRL,        0x1000,},
	{VENC_VDAC_SETTING,          0,     },
	{VPU_VIU_VENC_MUX_CTRL,      0x000a,},
	{ENCP_DACSEL_0,              0x3102,},
	{ENCP_DACSEL_1,              0x0054,},
	{VENC_VDAC_DACSEL0,          0x0001,},
	{VENC_VDAC_DACSEL1,          0x0001,},
	{VENC_VDAC_DACSEL2,          0x0001,},
	{VENC_VDAC_DACSEL3,          0x0001,},
	{VENC_VDAC_DACSEL4,          0x0001,},
	{VENC_VDAC_DACSEL5,          0x0001,},
	{ENCI_VIDEO_EN,              0,     },
	{ENCP_VIDEO_EN,              1,     },
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_1080p_24hz_clk[] = {
	{HHI_VID_CLK_CNTL,           0x0,},
	{HHI_VID_PLL_CNTL2,          0x814d3928},
	{HHI_VID_PLL_CNTL3,          0x6b425012},
	{HHI_VID_PLL_CNTL4,          0x110},
	{HHI_VID_PLL_CNTL,           0x0001043e,},
	{HHI_VID_DIVIDER_CNTL,       0x00010843,},
	{HHI_VID_CLK_DIV,            0x100},
	{HHI_VID_CLK_CNTL,           0x80000,},
	{HHI_VID_CLK_CNTL,           0x88001,},
	{HHI_VID_CLK_CNTL,           0x80003,},
	{HHI_VIID_CLK_DIV,           0x00000101,},
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_1080p_24hz_enc[] = {
	{ENCP_VIDEO_FILT_CTRL,       0x1052,},
	/* bit 13    1          (delayed prog_vs) */
	/* bit 5:4:  2          (pixel[0]) */
	/* bit 3:    1          invert vsync or not */
	/* bit 2:    1          invert hsync or not */
	/* bit1:     1          (select viu sync) */
	/* bit0:     1          (progressive) */
	{VENC_DVI_SETTING,           0x000d,},
	{ENCP_VIDEO_MAX_PXCNT,       2749,  },
	{ENCP_VIDEO_MAX_LNCNT,       1124,  },
	/* horizontal timing settings */
	{ENCP_VIDEO_HSPULS_BEGIN,    44,  },/* 1980 */
	{ENCP_VIDEO_HSPULS_END,      132,    },
	{ENCP_VIDEO_HSPULS_SWITCH,   44,    },

	/* DE position in horizontal */
	{ENCP_VIDEO_HAVON_BEGIN,     271,   },
	{ENCP_VIDEO_HAVON_END,       2190,  },

	/* ditital hsync positon in horizontal */
	{ENCP_VIDEO_HSO_BEGIN,       79 ,    },
	{ENCP_VIDEO_HSO_END,         123,  },

	/* vsync horizontal timing */
	{ENCP_VIDEO_VSPULS_BEGIN,    220,   },
	{ENCP_VIDEO_VSPULS_END,      2140,  },

	/* vertical timing settings */
	{ENCP_VIDEO_VSPULS_BLINE,    0,     },
	{ENCP_VIDEO_VSPULS_ELINE,    4,     },/* 35 */
	{ENCP_VIDEO_EQPULS_BLINE,    0,     },
	{ENCP_VIDEO_EQPULS_ELINE,    4,     },/* 35 */
	{ENCP_VIDEO_VAVON_BLINE,     41,    },
	{ENCP_VIDEO_VAVON_ELINE,     1120,  },

	/* adjust the hsync & vsync start point and end point */
	{ENCP_VIDEO_VSO_BEGIN,       79,  },
	{ENCP_VIDEO_VSO_END,         79,  },

	/* adjust the vsync start line and end line */
	{ENCP_VIDEO_VSO_BLINE,       0,     },
	{ENCP_VIDEO_VSO_ELINE,       5,     },

	{ENCP_VIDEO_YFP1_HTIME,      271,   },
	{ENCP_VIDEO_YFP2_HTIME,      2190,  },
	{VENC_VIDEO_PROG_MODE,       0x100, },
	{ENCP_VIDEO_MODE,            0x4040,},
	{ENCP_VIDEO_MODE_ADV,        0x0018,},

	/* bit[15:8] -- adjust the vsync vertical position */
	{ENCP_VIDEO_SYNC_MODE,       0x7, },

	/* Y/Cb/Cr delay */
	{ENCP_VIDEO_YC_DLY,          0,     },
	/* enable sync on B */
	{ENCP_VIDEO_RGB_CTRL, 2,},
	{VENC_SYNC_ROUTE,            0,     },
	{VENC_INTCTRL,               0x200, },
	{ENCP_VFIFO2VD_CTL,               0,     },
	{VENC_VDAC_FIFO_CTRL,        0x1000,},
	{VENC_VDAC_SETTING,          0,     },
	{VPU_VIU_VENC_MUX_CTRL,      0x000a,},
	{ENCP_DACSEL_0,              0x3102,},
	{ENCP_DACSEL_1,              0x0054,},
	{VENC_VDAC_DACSEL0,          0x0001,},
	{VENC_VDAC_DACSEL1,          0x0001,},
	{VENC_VDAC_DACSEL2,          0x0001,},
	{VENC_VDAC_DACSEL3,          0x0001,},
	{VENC_VDAC_DACSEL4,          0x0001,},
	{VENC_VDAC_DACSEL5,          0x0001,},
	{ENCI_VIDEO_EN,              0,     },
	{ENCP_VIDEO_EN,              1,     },
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_4k2k_30hz_enc[] = {
	{ENCP_VIDEO_EN,              0,     },
	{ENCI_VIDEO_EN,              0,     },
	/* Enable Hsync and equalization pulse switch in center */
	/* bit[14] cfg_de_v = 1 */
	{ENCP_VIDEO_MODE,             0x4040},
	/* Sampling rate: 1 */
	{ENCP_VIDEO_MODE_ADV,         0x0008},
	{ENCP_VIDEO_YFP1_HTIME,       140},
	{ENCP_VIDEO_YFP2_HTIME,       140 + 3840},

	{ENCP_VIDEO_MAX_PXCNT,        3840 + 560 - 1},
	{ENCP_VIDEO_HSPULS_BEGIN,     2156 + 1920},
	{ENCP_VIDEO_HSPULS_END,       44},
	{ENCP_VIDEO_HSPULS_SWITCH,    44},
	{ENCP_VIDEO_VSPULS_BEGIN,     140},
	{ENCP_VIDEO_VSPULS_END,       2059 + 1920},
	{ENCP_VIDEO_VSPULS_BLINE,     0},
	{ENCP_VIDEO_VSPULS_ELINE,     4},

	{ENCP_VIDEO_HAVON_BEGIN,      148},
	{ENCP_VIDEO_HAVON_END,        3987},
	{ENCP_VIDEO_VAVON_BLINE,      89},
	{ENCP_VIDEO_VAVON_ELINE,      2248},

	{ENCP_VIDEO_HSO_BEGIN,	    44},
	{ENCP_VIDEO_HSO_END,		    2156 + 1920},
	{ENCP_VIDEO_VSO_BEGIN,	    2100 + 1920},
	{ENCP_VIDEO_VSO_END,		    2164 + 1920},

	{ENCP_VIDEO_VSO_BLINE,        51},
	{ENCP_VIDEO_VSO_ELINE,        53},
	{ENCP_VIDEO_MAX_LNCNT,        2249},

	/* bypass filter */
	{ENCP_VIDEO_FILT_CTRL,        0x1000},
	{ENCP_VIDEO_EN,              1,     },
	{ENCI_VIDEO_EN,              0,     },
	{MREG_END_MARKER,            0      },
};

static const struct reg_s tvregs_4k2k_25hz_enc[] = {
	/* {ENCP_VIDEO_EN,              0,     }, */
	/* {ENCI_VIDEO_EN,              0,     }, */
	/* Enable Hsync and equalization pulse switch in center */
	/* bit[14] cfg_de_v = 1 */
	{ENCP_VIDEO_MODE,             0x4040},
	/* Sampling rate: 1 */
	{ENCP_VIDEO_MODE_ADV,         0x0008},
	{ENCP_VIDEO_YFP1_HTIME,       140},
	{ENCP_VIDEO_YFP2_HTIME,       140 + 3840},

	{ENCP_VIDEO_MAX_PXCNT,        3840 + 1440 - 1},
	{ENCP_VIDEO_HSPULS_BEGIN,     2156 + 1920},
	{ENCP_VIDEO_HSPULS_END,       44},
	{ENCP_VIDEO_HSPULS_SWITCH,    44},
	{ENCP_VIDEO_VSPULS_BEGIN,     140},
	{ENCP_VIDEO_VSPULS_END,       2059 + 1920},
	{ENCP_VIDEO_VSPULS_BLINE,     0},
	{ENCP_VIDEO_VSPULS_ELINE,     4},

	{ENCP_VIDEO_HAVON_BEGIN,      148},
	{ENCP_VIDEO_HAVON_END,        3987},
	{ENCP_VIDEO_VAVON_BLINE,      89},
	{ENCP_VIDEO_VAVON_ELINE,      2248},

	{ENCP_VIDEO_HSO_BEGIN,	    44},
	{ENCP_VIDEO_HSO_END,		    2156 + 1920},
	{ENCP_VIDEO_VSO_BEGIN,	    2100 + 1920},
	{ENCP_VIDEO_VSO_END,		    2164 + 1920},

	{ENCP_VIDEO_VSO_BLINE,        51},
	{ENCP_VIDEO_VSO_ELINE,        53},
	{ENCP_VIDEO_MAX_LNCNT,        2249},

	/* bypass filter */
	{ENCP_VIDEO_FILT_CTRL,        0x1000},
	{ENCP_VIDEO_EN,              1,     },
	{ENCI_VIDEO_EN,              0,     },
	{MREG_END_MARKER,            0      },
};

static const struct reg_s tvregs_4k2k_24hz_enc[] = {
	/* {ENCP_VIDEO_EN,              0,     }, */
	/* {ENCI_VIDEO_EN,              0,     }, */
	/* Enable Hsync and equalization pulse switch in center */
	/* bit[14] cfg_de_v = 1 */
	{ENCP_VIDEO_MODE,             0x4040},
	/* Sampling rate: 1 */
	{ENCP_VIDEO_MODE_ADV,         0x0008},
	{ENCP_VIDEO_YFP1_HTIME,       140},
	{ENCP_VIDEO_YFP2_HTIME,       140 + 3840},

	{ENCP_VIDEO_MAX_PXCNT,        3840 + 1660 - 1},
	{ENCP_VIDEO_HSPULS_BEGIN,     2156 + 1920},
	{ENCP_VIDEO_HSPULS_END,       44},
	{ENCP_VIDEO_HSPULS_SWITCH,    44},
	{ENCP_VIDEO_VSPULS_BEGIN,     140},
	{ENCP_VIDEO_VSPULS_END,       2059 + 1920},
	{ENCP_VIDEO_VSPULS_BLINE,     0},
	{ENCP_VIDEO_VSPULS_ELINE,     4},

	{ENCP_VIDEO_HAVON_BEGIN,      148},
	{ENCP_VIDEO_HAVON_END,        3987},
	{ENCP_VIDEO_VAVON_BLINE,      89},
	{ENCP_VIDEO_VAVON_ELINE,      2248},

	{ENCP_VIDEO_HSO_BEGIN,	    44},
	{ENCP_VIDEO_HSO_END,		    2156 + 1920},
	{ENCP_VIDEO_VSO_BEGIN,	    2100 + 1920},
	{ENCP_VIDEO_VSO_END,		    2164 + 1920},

	{ENCP_VIDEO_VSO_BLINE,        51},
	{ENCP_VIDEO_VSO_ELINE,        53},
	{ENCP_VIDEO_MAX_LNCNT,        2249},

	/* bypass filter */
	{ENCP_VIDEO_FILT_CTRL,        0x1000},
	{ENCP_VIDEO_EN,              1,     },
	{ENCI_VIDEO_EN,              0,     },
	{MREG_END_MARKER,            0      },
};

static const struct reg_s tvregs_4k2k_smpte_enc[] = {
	{ENCP_VIDEO_MODE,             0x4040},
	{ENCP_VIDEO_MODE_ADV,         0x0008},
	{ENCP_VIDEO_YFP1_HTIME,       140},
	{ENCP_VIDEO_YFP2_HTIME,       140 + 3840 + 256},

	{ENCP_VIDEO_MAX_PXCNT,        4096 + 1404 - 1},
	{ENCP_VIDEO_HSPULS_BEGIN,     2156 + 1920},
	{ENCP_VIDEO_HSPULS_END,       44},
	{ENCP_VIDEO_HSPULS_SWITCH,    44},
	{ENCP_VIDEO_VSPULS_BEGIN,     140},
	{ENCP_VIDEO_VSPULS_END,       2059 + 1920},
	{ENCP_VIDEO_VSPULS_BLINE,     0},
	{ENCP_VIDEO_VSPULS_ELINE,     4},

	{ENCP_VIDEO_HAVON_BEGIN,      148},
	{ENCP_VIDEO_HAVON_END,        3987 + 256},
	{ENCP_VIDEO_VAVON_BLINE,      89},
	{ENCP_VIDEO_VAVON_ELINE,      2248},

	{ENCP_VIDEO_HSO_BEGIN,	    44},
	{ENCP_VIDEO_HSO_END,		    2156 + 1920 + 256},
	{ENCP_VIDEO_VSO_BEGIN,	    2100 + 1920 + 256},
	{ENCP_VIDEO_VSO_END,		    2164 + 1920 + 256},

	{ENCP_VIDEO_VSO_BLINE,        51},
	{ENCP_VIDEO_VSO_ELINE,        53},
	{ENCP_VIDEO_MAX_LNCNT,        2249},

	{ENCP_VIDEO_FILT_CTRL,        0x1000},
	{ENCP_VIDEO_EN,              1,     },
	{ENCI_VIDEO_EN,              0,     },
	{MREG_END_MARKER,            0      },
};

static const struct reg_s tvregs_vga_640x480_clk[] = { /* 25.17mhz 800 *525 */
	{HHI_VID_CLK_CNTL,           0x0,       },
	{HHI_VID_PLL_CNTL,           0x2001042d,},
	{HHI_VID_PLL_CNTL2,          0x814d3928,},
	{HHI_VID_PLL_CNTL3,          0x6b425012,    },
	{HHI_VID_PLL_CNTL4,          0x110},
	{HHI_VID_PLL_CNTL,           0x0001042a,},/* 50 */

	{HHI_VID_DIVIDER_CNTL,       0x00011943,},
	{HHI_VID_CLK_DIV,            0x100},
	{HHI_VID_CLK_CNTL,           0x80000,},
	{HHI_VID_CLK_CNTL,           0x88001,},
	{HHI_VID_CLK_CNTL,           0x80003,},
	{HHI_VIID_CLK_DIV,           0x00000101,},
	{MREG_END_MARKER,            0      },
};

static const struct reg_s tvregs_vga_640x480_enc[] = { /* 25.17mhz 800 *525 */
	{ENCP_VIDEO_FILT_CTRL,       0x1052,},
	/* {HHI_VID_CLK_DIV,            0x01000100,}, */
	{ENCP_VIDEO_FILT_CTRL,       0x2052,},
	{VENC_DVI_SETTING,           0x21,  },
	{ENCP_VIDEO_MODE,            0,     },
	{ENCP_VIDEO_MODE_ADV,        0x009,     },
	{ENCP_VIDEO_YFP1_HTIME,      244,   },
	{ENCP_VIDEO_YFP2_HTIME,      1630,  },
	{ENCP_VIDEO_YC_DLY,          0,     },
	{ENCP_VIDEO_MAX_PXCNT,       1599,  },
	{ENCP_VIDEO_MAX_LNCNT,       525,   },
	{ENCP_VIDEO_HSPULS_BEGIN,    0x60,  },
	{ENCP_VIDEO_HSPULS_END,      0xa0,  },
	{ENCP_VIDEO_HSPULS_SWITCH,   88,    },
	{ENCP_VIDEO_VSPULS_BEGIN,    0,     },
	{ENCP_VIDEO_VSPULS_END,      1589   },
	{ENCP_VIDEO_VSPULS_BLINE,    0,     },
	{ENCP_VIDEO_VSPULS_ELINE,    5,     },
	{ENCP_VIDEO_HAVON_BEGIN,     153,   },
	{ENCP_VIDEO_HAVON_END,       1433,  },
	{ENCP_VIDEO_VAVON_BLINE,     59,    },
	{ENCP_VIDEO_VAVON_ELINE,     540,   },
	{ENCP_VIDEO_SYNC_MODE,       0x07,  },
	{VENC_VIDEO_PROG_MODE,       0x100,   },
	{VENC_VIDEO_EXSRC,           0x0,   },
	{ENCP_VIDEO_HSO_BEGIN,       0x3,   },
	{ENCP_VIDEO_HSO_END,         0x5,   },
	{ENCP_VIDEO_VSO_BEGIN,       0x3,   },
	{ENCP_VIDEO_VSO_END,         0x5,   },
	{ENCP_VIDEO_VSO_BLINE,       0,     },
	{ENCP_VIDEO_SY_VAL,          8,     },
	{ENCP_VIDEO_SY2_VAL,         0x1d8, },
	{VENC_SYNC_ROUTE,            0,     },
	{VENC_INTCTRL,               0x200, },
	{ENCP_VFIFO2VD_CTL,               0,     },
	{VENC_VDAC_SETTING,          0,     },
	{ENCP_VIDEO_RGB_CTRL,		 0,},
	{VENC_UPSAMPLE_CTRL0,        0xc061,},
	{VENC_UPSAMPLE_CTRL1,        0xd061,},
	{VENC_UPSAMPLE_CTRL2,        0xe061,},
	{VENC_VDAC_DACSEL0,          0xf003,},
	{VENC_VDAC_DACSEL1,          0xf003,},
	{VENC_VDAC_DACSEL2,          0xf003,},
	{VENC_VDAC_DACSEL3,          0xf003,},
	{VENC_VDAC_DACSEL4,          0xf003,},
	{VENC_VDAC_DACSEL5,          0xf003,},
	{VPU_VIU_VENC_MUX_CTRL,      0x000a,},
	{VENC_VDAC_FIFO_CTRL,        0x1fc0,},
	{ENCP_DACSEL_0,              0x0543,},
	{ENCP_DACSEL_1,              0x0000,},
	{ENCI_VIDEO_EN,              0      },
	{ENCP_VIDEO_EN,              1      },
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_svga_800x600_clk[] = { /* 39.5mhz 1056 *628 */
	{HHI_VID_CLK_CNTL,           0x0,},
	{HHI_VID_PLL_CNTL2,          0x814d3928},
	{HHI_VID_PLL_CNTL3,          0x6b425012},
	{HHI_VID_PLL_CNTL4,          0x110},
	{HHI_VID_PLL_CNTL,           0x00010422,},/* 79 */
	{HHI_VID_DIVIDER_CNTL,       0x00010843,},
	{HHI_VID_CLK_DIV,            0x100},
	{HHI_VID_CLK_CNTL,           0x80000,},
	{HHI_VID_CLK_CNTL,           0x88001,},
	{HHI_VID_CLK_CNTL,           0x80003,},
	{HHI_VIID_CLK_DIV,           0x00000101,},
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_svga_800x600_enc[] = { /* 39.5mhz 1056 *628 */
	{ENCP_VIDEO_FILT_CTRL,       0x0052,},
	{VENC_DVI_SETTING,           0x2029,},
	{ENCP_VIDEO_MODE,            0x0040,},
	{ENCP_VIDEO_MODE_ADV,        0x0019,},
	{ENCP_VIDEO_YFP1_HTIME,      500,   },
	{ENCP_VIDEO_YFP2_HTIME,      2112,  },
	{ENCP_VIDEO_MAX_PXCNT,       2111,  },
	{ENCP_VIDEO_MAX_LNCNT,       628,   },/* 628 */
	{ENCP_VIDEO_HSPULS_BEGIN,    0,    },
	{ENCP_VIDEO_HSPULS_END,      230,   },
	{ENCP_VIDEO_HSPULS_SWITCH,   80,    },
	{ENCP_VIDEO_VSPULS_BEGIN,    0x58,   },
	{ENCP_VIDEO_VSPULS_END,      0x80,  },
	{ENCP_VIDEO_VSPULS_BLINE,    0,     },
	{ENCP_VIDEO_VSPULS_ELINE,    5,     },
	{ENCP_VIDEO_EQPULS_BLINE,    0,     },
	{ENCP_VIDEO_EQPULS_ELINE,    5,     },
	{ENCP_VIDEO_HAVON_BEGIN,     267,   },/* 59 */
	{ENCP_VIDEO_HAVON_END,       1866,  },/* 1659 */
	{ENCP_VIDEO_VAVON_BLINE,    59,    },/* 59 */
	{ENCP_VIDEO_VAVON_ELINE,     658,   },/* 659 */
	{ENCP_VIDEO_HSO_BEGIN,       0,    },
	{ENCP_VIDEO_HSO_END,         260,   },
	{ENCP_VIDEO_VSO_BEGIN,       0,   },
	{ENCP_VIDEO_VSO_END,         2200,   },
	{ENCP_VIDEO_VSO_BLINE,       0,     },
	{ENCP_VIDEO_VSO_ELINE,       5,     },
	{VENC_VIDEO_PROG_MODE,       0x100, },
	{VENC_VIDEO_EXSRC,           0x0,   },
	{ENCP_VIDEO_HSO_BEGIN,       0x3,   },
	{ENCP_VIDEO_HSO_END,         0x5,   },
	{ENCP_VIDEO_VSO_BEGIN,       0x3,   },
	{ENCP_VIDEO_VSO_END,         0x5,   },
	{ENCP_VIDEO_VSO_BLINE,       0,     },
	{ENCP_VIDEO_SY_VAL,          8,     },
	{ENCP_VIDEO_SY2_VAL,         0x1d8, },
	{VENC_SYNC_ROUTE,            0,     },
	{VENC_INTCTRL,               0x200, },
	{ENCP_VFIFO2VD_CTL,               0,     },
	{VENC_VDAC_SETTING,          0,     },
	{ENCP_VIDEO_RGB_CTRL,		 0,},
	{VENC_UPSAMPLE_CTRL0,        0xc061,},
	{VENC_UPSAMPLE_CTRL1,        0xd061,},
	{VENC_UPSAMPLE_CTRL2,        0xe061,},
	{VENC_VDAC_DACSEL0,          0xf003,},
	{VENC_VDAC_DACSEL1,          0xf003,},
	{VENC_VDAC_DACSEL2,          0xf003,},
	{VENC_VDAC_DACSEL3,          0xf003,},
	{VENC_VDAC_DACSEL4,          0xf003,},
	{VENC_VDAC_DACSEL5,          0xf003,},
	{VPU_VIU_VENC_MUX_CTRL,      0x000a,},
	{VENC_VDAC_FIFO_CTRL,        0x1fc0,},
	{ENCP_DACSEL_0,              0x0543,},
	{ENCP_DACSEL_1,              0x0000,},
	{ENCI_VIDEO_EN,              0      },
	{ENCP_VIDEO_EN,              1      },
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_xga_1024x768_clk[] = {
	{ENCP_VIDEO_EN,              0,     },
	{ENCI_VIDEO_EN,              0,     },
	{VENC_VDAC_SETTING,          0xff,  },
	{HHI_VID_CLK_CNTL,           0x0,},
	{HHI_VID_PLL_CNTL2,          0x814d3928},
	{HHI_VID_PLL_CNTL3,          0x6b425012},
	{HHI_VID_PLL_CNTL4,          0x110},
	{HHI_VID_PLL_CNTL,           0x00010436,},
	{HHI_VID_DIVIDER_CNTL,       0x00010843,},
	{HHI_VID_CLK_DIV,            0x100},
	{HHI_VID_CLK_CNTL,           0x80000,},
	{HHI_VID_CLK_CNTL,           0x88001,},
	{HHI_VID_CLK_CNTL,           0x80003,},
	{HHI_VIID_CLK_DIV,           0x00000101,},
	{MREG_END_MARKER,            0      }
};
static const struct reg_s tvregs_xga_1024x768_enc[] = {
	{ENCP_VIDEO_FILT_CTRL,       0x0052,},
	{VENC_DVI_SETTING,           0x2029,},
	{ENCP_VIDEO_MODE,            0x0040,},
	{ENCP_VIDEO_MODE_ADV,        0x0009,},
	{ENCP_VIDEO_YFP1_HTIME,      500,   },
	{ENCP_VIDEO_YFP2_HTIME,      2500,  },
	{ENCP_VIDEO_MAX_PXCNT,       2691,  },
	{ENCP_VIDEO_MAX_LNCNT,       806,   },
	{ENCP_VIDEO_HSPULS_BEGIN,    0,    },
	{ENCP_VIDEO_HSPULS_END,      230,   },
	{ENCP_VIDEO_HSPULS_SWITCH,   80,    },
	{ENCP_VIDEO_VSPULS_BEGIN,    0x22,   },
	{ENCP_VIDEO_VSPULS_END,      0xa0,  },
	{ENCP_VIDEO_VSPULS_BLINE,    0,     },
	{ENCP_VIDEO_VSPULS_ELINE,    5,     },
	{ENCP_VIDEO_EQPULS_BLINE,    0,     },
	{ENCP_VIDEO_EQPULS_ELINE,    5,     },
	{ENCP_VIDEO_HAVON_BEGIN,     315,   },
	{ENCP_VIDEO_HAVON_END,       2362,  },
	{ENCP_VIDEO_VAVON_BLINE,     59,    },
	{ENCP_VIDEO_VAVON_ELINE,     827,   },/* 827 */
	{ENCP_VIDEO_HSO_BEGIN,       0,    },
	{ENCP_VIDEO_HSO_END,         260,   },
	{ENCP_VIDEO_VSO_BEGIN,       0,   },
	{ENCP_VIDEO_VSO_END,         2200,   },
	{ENCP_VIDEO_VSO_BLINE,       0,     },
	{ENCP_VIDEO_VSO_ELINE,       5,     },
	{VENC_VIDEO_PROG_MODE,       0x100, },
	{VENC_SYNC_ROUTE,            0,     },
	{VENC_INTCTRL,               0x200, },
	{ENCP_VFIFO2VD_CTL,               0,     },
	{VENC_VDAC_SETTING,          0,     },

	{ENCP_VIDEO_RGB_CTRL,		 0,},
	{VENC_UPSAMPLE_CTRL0,        0xc061,},
	{VENC_UPSAMPLE_CTRL1,        0xd061,},
	{VENC_UPSAMPLE_CTRL2,        0xe061,},
	{VENC_VDAC_DACSEL0,          0xf003,},
	{VENC_VDAC_DACSEL1,          0xf003,},
	{VENC_VDAC_DACSEL2,          0xf003,},
	{VENC_VDAC_DACSEL3,          0xf003,},
	{VENC_VDAC_DACSEL4,          0xf003,},
	{VENC_VDAC_DACSEL5,          0xf003,},
	{VPU_VIU_VENC_MUX_CTRL,      0x000a,},
	{VENC_VDAC_FIFO_CTRL,        0x1fc0,},
	{ENCP_DACSEL_0,              0x0543,},
	{ENCP_DACSEL_1,              0x0000,},
	{ENCI_VIDEO_EN,              0      },
	{ENCP_VIDEO_EN,              1      },
	{MREG_END_MARKER,            0      }
};

/* Using tvmode as index */
static struct tvregs_set_t tvregsTab[] = {
	{TVMODE_480I, tvregs_480i_clk, tvregs_480i_enc},
	{TVMODE_480I_RPT, tvregs_480i_clk, tvregs_480i_enc},
	{TVMODE_480CVBS, tvregs_480cvbs_clk, tvregs_480cvbs_enc},
	{TVMODE_480P, tvregs_480p_clk, tvregs_480p_enc},
	{TVMODE_480P_RPT, tvregs_480p_clk, tvregs_480p_enc},
	{TVMODE_576I, tvregs_576i_clk, tvregs_576i_enc},
	{TVMODE_576I_RPT, tvregs_576i_clk, tvregs_576i_enc},
	{TVMODE_576CVBS, tvregs_576cvbs_clk, tvregs_576cvbs_enc},
	{TVMODE_576P, tvregs_576p_clk, tvregs_576p_enc},
	{TVMODE_576P_RPT, tvregs_576p_clk, tvregs_576p_enc},
	{TVMODE_720P, tvregs_720p_clk, tvregs_720p_enc},
	{TVMODE_1080I, tvregs_1080i_clk, tvregs_1080i_enc},
	{TVMODE_1080P, tvregs_1080p_clk, tvregs_1080p_enc},
	{TVMODE_720P_50HZ, tvregs_720p_50hz_clk, tvregs_720p_50hz_enc},
	{TVMODE_1080I_50HZ, tvregs_1080i_50hz_clk, tvregs_1080i_50hz_enc},
	{TVMODE_1080P_50HZ, tvregs_1080p_50hz_clk, tvregs_1080p_50hz_enc},
	{TVMODE_1080P_24HZ, tvregs_1080p_24hz_clk, tvregs_1080p_24hz_enc},
	{TVMODE_4K2K_30HZ, NULL, tvregs_4k2k_30hz_enc},
	{TVMODE_4K2K_25HZ, NULL, tvregs_4k2k_25hz_enc},
	{TVMODE_4K2K_24HZ, NULL, tvregs_4k2k_24hz_enc},
	{TVMODE_4K2K_SMPTE, NULL, tvregs_4k2k_smpte_enc},
	{TVMODE_VGA, tvregs_vga_640x480_clk, tvregs_vga_640x480_enc},
	{TVMODE_SVGA, tvregs_svga_800x600_clk, tvregs_svga_800x600_enc},
	{TVMODE_XGA, tvregs_xga_1024x768_clk, tvregs_xga_1024x768_enc},
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	{TVMODE_480P_59HZ, tvregs_480p_clk, tvregs_480p_enc},
	{TVMODE_720P_59HZ , tvregs_720p_clk, tvregs_720p_enc},
	{TVMODE_1080I_59HZ, tvregs_1080i_clk, tvregs_1080i_enc},
	{TVMODE_1080P_59HZ, tvregs_1080p_clk, tvregs_1080p_enc},
	{TVMODE_1080P_23HZ, tvregs_1080p_24hz_clk, tvregs_1080p_24hz_enc},
	{TVMODE_4K2K_29HZ, NULL, tvregs_4k2k_30hz_enc},
	{TVMODE_4K2K_23HZ, NULL, tvregs_4k2k_24hz_enc},
#endif
};

static const struct tvinfo_s tvinfoTab[] = {
	{
		.tvmode = TVMODE_480I,
		.xres =  720,
		.yres =  480,
		.id = "480i60hz"
	},
	{
		.tvmode = TVMODE_480I_RPT,
		.xres =  720,
		.yres =  480,
		.id = "480i_rpt"
	},
	{
		.tvmode = TVMODE_480CVBS,
		.xres =  720,
		.yres =  480,
		.id = "480cvbs"
	},
	{
		.tvmode = TVMODE_480P,
		.xres =  720,
		.yres =  480,
		.id = "480p60hz"
	},
	{
		.tvmode = TVMODE_480P_RPT,
		.xres =  720,
		.yres =  480,
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
		.id = "576i_rpt"},
	{
		.tvmode = TVMODE_576CVBS,
		.xres = 720,
		.yres =  576,
		.id = "576cvbs"
	},
	{
		.tvmode = TVMODE_576P,
		.xres = 720,
		.yres =  576,
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
