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
	{ENCI_YC_DELAY,				0x343},
	{ENCI_VIDEO_SAT,			0x9	 },
	{MREG_END_MARKER,			0    }
};

static const struct reg_s tvregs_576cvbs_china_telecom_gxbb[] = {
	{VENC_VDAC_DAC0_GAINCTRL,	0x9		},
	{ENCI_YC_DELAY,				0x343   },
	{ENCI_SYNC_ADJ,				0x8080	},
	{ENCI_VIDEO_SAT,			0xfd	},
	{VENC_VDAC_DAC0_FILT_CTRL1,	0xf850	},
	{MREG_END_MARKER,			0		}
};

static const struct reg_s tvregs_576cvbs_china_mobile_gxbb[] = {
	{VENC_VDAC_DAC0_GAINCTRL,	0x9		},
	{ENCI_YC_DELAY,				0x343   },
	{ENCI_SYNC_ADJ,				0x8080	},
	{ENCI_VIDEO_SAT,			0xfd	},
	{VENC_VDAC_DAC0_FILT_CTRL1,	0xf850	},
	{MREG_END_MARKER,			0		}
};

static const struct reg_s *tvregs_576cvbs_performance_gxbb[] = {
	tvregs_576cvbs_china_sarft_gxbb,
	tvregs_576cvbs_china_telecom_gxbb,
	tvregs_576cvbs_china_mobile_gxbb
};

static const struct reg_s tvregs_576cvbs_china_sarft_gxtvbb[] = {
	{ENCI_YC_DELAY,				0x333	},
	{ENCI_VIDEO_SAT,			0xfb	},
	{VENC_VDAC_DAC0_FILT_CTRL1,	0xff42	},
	{ENCI_SYNC_ADJ,				0x8c00	},
	{MREG_END_MARKER,			0		}
};

static const struct reg_s tvregs_576cvbs_china_telecom_gxtvbb[] = {
	{ENCI_YC_DELAY,				0x333	},
	{ENCI_VIDEO_SAT,			0xfb	},
	{VENC_VDAC_DAC0_FILT_CTRL1,	0xff42	},
	{ENCI_SYNC_ADJ,				0x8c00	},
	{MREG_END_MARKER,			0		}
};

static const struct reg_s tvregs_576cvbs_china_mobile_gxtvbb[] = {
	{ENCI_YC_DELAY,				0x333	},
	{ENCI_VIDEO_SAT,			0xfb	},
	{VENC_VDAC_DAC0_FILT_CTRL1,	0xff42	},
	{ENCI_SYNC_ADJ,				0x8c00	},
	{MREG_END_MARKER,			0		}
};

static const struct reg_s *tvregs_576cvbs_performance_gxtvbb[] = {
	tvregs_576cvbs_china_sarft_gxtvbb,
	tvregs_576cvbs_china_telecom_gxtvbb,
	tvregs_576cvbs_china_mobile_gxtvbb
};

static const struct reg_s tvregs_576cvbs_china_sarft_905x[] = {
	{VENC_VDAC_DAC0_GAINCTRL,	0x9		},
	{ENCI_YC_DELAY,				0x343	},
	{ENCI_SYNC_ADJ,				0x0		},
	{ENCI_VIDEO_SAT,			0x9		},
	{VENC_VDAC_DAC0_FILT_CTRL1,	0xfc48	},
	{MREG_END_MARKER,			0		}
};

static const struct reg_s tvregs_576cvbs_china_telecom_905x[] = {
	{VENC_VDAC_DAC0_GAINCTRL,	0x9		},
	{ENCI_YC_DELAY,				0x343   },
	{ENCI_SYNC_ADJ,				0x8080	},
	{ENCI_VIDEO_SAT,			0xfd	},
	{VENC_VDAC_DAC0_FILT_CTRL1,	0xf752	},
	{MREG_END_MARKER,			0		}
};

static const struct reg_s tvregs_576cvbs_china_mobile_905x[] = {
	{VENC_VDAC_DAC0_GAINCTRL,	0x9		},
	{ENCI_YC_DELAY,				0x343   },
	{ENCI_SYNC_ADJ,				0x8080	},
	{ENCI_VIDEO_SAT,			0xfd	},
	{VENC_VDAC_DAC0_FILT_CTRL1,	0xf752	},
	{MREG_END_MARKER,			0		}
};

static const struct reg_s *tvregs_576cvbs_performance_905x[] = {
	tvregs_576cvbs_china_sarft_905x,
	tvregs_576cvbs_china_telecom_905x,
	tvregs_576cvbs_china_mobile_905x
};

static const struct reg_s tvregs_576cvbs_china_sarft_905l[] = {
	{VENC_VDAC_DAC0_GAINCTRL,	0x9		},
	{ENCI_YC_DELAY,				0x343	},
	{ENCI_SYNC_ADJ,				0x0		},
	{ENCI_VIDEO_SAT,			0x9		},
	{VENC_VDAC_DAC0_FILT_CTRL1,	0xfc48	},
	{MREG_END_MARKER,			0		}
};

static const struct reg_s tvregs_576cvbs_china_telecom_905l[] = {
	{VENC_VDAC_DAC0_GAINCTRL,	0x9		},
	{ENCI_YC_DELAY,				0x343   },
	{ENCI_SYNC_ADJ,				0x8080	},
	{ENCI_VIDEO_SAT,			0xfd	},
	{VENC_VDAC_DAC0_FILT_CTRL1,	0xf850	},
	{MREG_END_MARKER,			0		}
};

static const struct reg_s tvregs_576cvbs_china_mobile_905l[] = {
	{VENC_VDAC_DAC0_GAINCTRL,	0x9		},
	{ENCI_YC_DELAY,				0x343   },
	{ENCI_SYNC_ADJ,				0x8080	},
	{ENCI_VIDEO_SAT,			0xfd	},
	{VENC_VDAC_DAC0_FILT_CTRL1,	0xf850	},
	{MREG_END_MARKER,			0		}
};

static const struct reg_s *tvregs_576cvbs_performance_905l[] = {
	tvregs_576cvbs_china_sarft_905l,
	tvregs_576cvbs_china_telecom_905l,
	tvregs_576cvbs_china_mobile_905l
};

static const struct reg_s tvregs_576cvbs_china_sarft_txl[] = {
	{ENCI_YC_DELAY,				0x333	},
	{ENCI_VIDEO_SAT,			0xf4	},
	{VENC_VDAC_DAC0_FILT_CTRL1,	0xfc48	},
	{ENCI_SYNC_ADJ,				0x8c00	},
	{MREG_END_MARKER,			0		}
};

static const struct reg_s tvregs_576cvbs_china_telecom_txl[] = {
	{ENCI_YC_DELAY,				0x333	},
	{ENCI_VIDEO_SAT,			0xf4	},
	{VENC_VDAC_DAC0_FILT_CTRL1,	0xfc48	},
	{ENCI_SYNC_ADJ,				0x8c00	},
	{MREG_END_MARKER,			0		}
};

static const struct reg_s tvregs_576cvbs_china_mobile_txl[] = {
	{ENCI_YC_DELAY,				0x333	},
	{ENCI_VIDEO_SAT,			0xf4	},
	{VENC_VDAC_DAC0_FILT_CTRL1,	0xfc48	},
	{ENCI_SYNC_ADJ,				0x8c00	},
	{MREG_END_MARKER,			0		}
};

static const struct reg_s *tvregs_576cvbs_performance_txl[] = {
	tvregs_576cvbs_china_sarft_txl,
	tvregs_576cvbs_china_telecom_txl,
	tvregs_576cvbs_china_mobile_txl
};

#endif

/* ALL REGISTERS WITH HDMITX MODE MOVE TO HDMITX DRIVER */
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
	{ENCI_VIDEO_EN,                1,     },
	{ENCI_VIDEO_SAT,               0x12   },
	{VENC_VDAC_DAC0_FILT_CTRL0,    0x1    },
	{VENC_VDAC_DAC0_FILT_CTRL1,    0xfc48 },
	{ENCI_MACV_N0,                 0x0    },
	{ENCI_SYNC_ADJ,                0x9c00 },
	{ENCI_VIDEO_CONT,              0x3    },
	{MREG_END_MARKER,              0      }
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
	{ENCI_VIDEO_EN,                   1,         },
	{ENCI_VIDEO_SAT,                  0x7        },
	{VENC_VDAC_DAC0_FILT_CTRL0,       0x1        },
	{VENC_VDAC_DAC0_FILT_CTRL1,       0xfc48     },
	{ENCI_MACV_N0,                    0x0        },
	{MREG_END_MARKER,                 0          }
};

static const struct reg_s tvregs_pal_m_enc[] = {
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
	{ENCI_VIDEO_MODE,              0x2a   },
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
	{ENCI_VIDEO_EN,                1,     },
	{ENCI_VIDEO_SAT,               0x12   },
	{VENC_VDAC_DAC0_FILT_CTRL0,    0x1    },
	{VENC_VDAC_DAC0_FILT_CTRL1,    0xfc48 },
	{ENCI_MACV_N0,                 0x0    },
	{ENCI_SYNC_ADJ,                0x9c00 },
	{ENCI_VIDEO_CONT,              0x3    },
	{MREG_END_MARKER,              0      }
};

static const struct reg_s tvregs_pal_n_enc[] = {
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
	{ENCI_VIDEO_MODE,                 0x3b       },
	{ENCI_VIDEO_MODE_ADV,             0x26,      },
	{ENCI_VIDEO_SCH,                  0x28,      },
	{ENCI_SYNC_MODE,                  0x07,      },
	{ENCI_YC_DELAY,                   0x333,     },
	{ENCI_VFIFO2VD_PIXEL_START,       0x0fb	     },
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
	{ENCI_VIDEO_EN,                   1,         },
	{ENCI_VIDEO_SAT,                  0x7        },
	{VENC_VDAC_DAC0_FILT_CTRL0,       0x1        },
	{VENC_VDAC_DAC0_FILT_CTRL1,       0xfc48     },
	{ENCI_MACV_N0,                    0x0        },
	{MREG_END_MARKER,                 0          }
};

static const struct reg_s tvregs_clk_DUMMY[] = {
	{MREG_END_MARKER,            0          }
};

static const struct reg_s tvregs_enc_DUMMY[] = {
	{MREG_END_MARKER,            0          }
};

/* Using tvmode as index */
static struct tvregs_set_t tvregsTab[] = {
	{TVMODE_480I, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_480I_RPT, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_480CVBS, tvregs_480cvbs_clk, tvregs_480cvbs_enc},
	{TVMODE_NTSC_M, tvregs_480cvbs_clk, tvregs_480cvbs_enc},
	{TVMODE_480P, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_480P_RPT, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_576I, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_576I_RPT, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_576CVBS, tvregs_576cvbs_clk, tvregs_576cvbs_enc},
	{TVMODE_PAL_M, tvregs_576cvbs_clk, tvregs_pal_m_enc},
	{TVMODE_PAL_N, tvregs_576cvbs_clk, tvregs_pal_n_enc},
	{TVMODE_576P, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_576P_RPT, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_720P, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_1080I, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_1080P, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_720P_50HZ, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_1080I_50HZ, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_1080P_30HZ, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_1080P_50HZ, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_1080P_25HZ, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_1080P_24HZ, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_4K2K_30HZ, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_4K2K_25HZ, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_4K2K_24HZ, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_4K2K_SMPTE, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_4K2K_SMPTE_25HZ, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_4K2K_SMPTE_30HZ, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_4K2K_SMPTE_50HZ, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_4K2K_SMPTE_60HZ, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_640x480p60hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_800x480p60hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_800x600p60hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_852x480p60hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_854x480p60hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_1024x600p60hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_1024x768p60hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_1152x864p75hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_1280x600p60hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_1280x768p60hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_1280x800p60hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_1280x960p60hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_1280x1024p60hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_1360x768p60hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_1366x768p60hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_1400x1050p60hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_1440x900p60hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_1600x900p60hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_1600x1200p60hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_1680x1050p60hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_1920x1200p60hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_2160x1200p90hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_2560x1080p60hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_2560x1440p60hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_2560x1600p60hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_3440x1440p60hz, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_VGA, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_SVGA, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
	{TVMODE_XGA, tvregs_clk_DUMMY, tvregs_enc_DUMMY},
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
		.tvmode = TVMODE_NTSC_M,
		.xres =  720,
		.yres =  480,
		.id = "ntsc_m"
	},
	{
		.tvmode = TVMODE_PAL_M,
		.xres = 720,
		.yres =  480,
		.id = "pal_m"
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
		.tvmode = TVMODE_PAL_N,
		.xres = 720,
		.yres =  576,
		.id = "pal_n"
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
		.tvmode = TVMODE_1080P_30HZ,
		.xres = 1920,
		.yres = 1080,
		.id = "1080p30hz"
	},
	{
		.tvmode = TVMODE_1080P_25HZ,
		.xres = 1920,
		.yres = 1080,
		.id = "1080p25hz"
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
		.tvmode = TVMODE_4K2K_SMPTE_25HZ,
		.xres = 4096,
		.yres = 2160,
		.id = "smpte25hz"
	},
	{
		.tvmode = TVMODE_4K2K_SMPTE_30HZ,
		.xres = 4096,
		.yres = 2160,
		.id = "smpte30hz"
	},
	{
		.tvmode = TVMODE_4K2K_SMPTE_50HZ,
		.xres = 4096,
		.yres = 2160,
		.id = "smpte50hz"
	},
	{
		.tvmode = TVMODE_4K2K_SMPTE_60HZ,
		.xres = 4096,
		.yres = 2160,
		.id = "smpte60hz"
	},
	{
		.tvmode = TVMODE_640x480p60hz,
		.xres = 640,
		.yres = 480,
		.id = "640x480p60hz"
	},
	{
		.tvmode = TVMODE_800x480p60hz,
		.xres = 800,
		.yres = 480,
		.id = "800x480p60hz"
	},
	{
		.tvmode = TVMODE_800x600p60hz,
		.xres = 800,
		.yres = 600,
		.id = "800x600p60hz"
	},
	{
		.tvmode = TVMODE_852x480p60hz,
		.xres = 852,
		.yres = 480,
		.id = "852x480p60hz"
	},
	{
		.tvmode = TVMODE_854x480p60hz,
		.xres = 854,
		.yres = 480,
		.id = "854x480p60hz"
	},
	{
		.tvmode = TVMODE_1024x600p60hz,
		.xres = 1024,
		.yres = 600,
		.id = "1024x600p60hz"
	},
	{
		.tvmode = TVMODE_1024x768p60hz,
		.xres = 1024,
		.yres = 768,
		.id = "1024x768p60hz"
	},
	{
		.tvmode = TVMODE_1152x864p75hz,
		.xres = 1152,
		.yres = 864,
		.id = "1152x864p75hz"
	},
	{
		.tvmode = TVMODE_1280x600p60hz,
		.xres = 1280,
		.yres = 600,
		.id = "1280x600p60hz"
	},
	{
		.tvmode = TVMODE_1280x768p60hz,
		.xres = 1280,
		.yres = 768,
		.id = "1280x768p60hz"
	},
	{
		.tvmode = TVMODE_1280x800p60hz,
		.xres = 1280,
		.yres = 800,
		.id = "1280x800p60hz"
	},
	{
		.tvmode = TVMODE_1280x960p60hz,
		.xres = 1280,
		.yres = 960,
		.id = "1280x960p60hz"
	},
	{
		.tvmode = TVMODE_1280x1024p60hz,
		.xres = 1280,
		.yres = 1024,
		.id = "1280x1024p60hz"
	},
	{
		.tvmode = TVMODE_1360x768p60hz,
		.xres = 1360,
		.yres = 768,
		.id = "1360x768p60hz"
	},
	{
		.tvmode = TVMODE_1366x768p60hz,
		.xres = 1366,
		.yres = 768,
		.id = "1366x768p60hz"
	},
	{
		.tvmode = TVMODE_1400x1050p60hz,
		.xres = 1400,
		.yres = 1050,
		.id = "1400x1050p60hz"
	},
	{
		.tvmode = TVMODE_1440x900p60hz,
		.xres = 1440,
		.yres = 900,
		.id = "1440x900p60hz"
	},
	{
		.tvmode = TVMODE_1600x900p60hz,
		.xres = 1600,
		.yres = 900,
		.id = "1600x900p60hz"
	},
	{
		.tvmode = TVMODE_1600x1200p60hz,
		.xres = 1600,
		.yres = 1200,
		.id = "1600x1200p60hz"
	},
	{
		.tvmode = TVMODE_1680x1050p60hz,
		.xres = 1680,
		.yres = 1050,
		.id = "1680x1050p60hz"
	},
	{
		.tvmode = TVMODE_1920x1200p60hz,
		.xres = 1920,
		.yres = 1200,
		.id = "1920x1200p60hz"
	},
	{
		.tvmode = TVMODE_2160x1200p90hz,
		.xres = 2160,
		.yres = 1200,
		.id = "2160x1200p90hz"
	},
	{
		.tvmode = TVMODE_2560x1080p60hz,
		.xres = 2560,
		.yres = 1080,
		.id = "2560x1080p60hz"
	},
	{
		.tvmode = TVMODE_2560x1440p60hz,
		.xres = 2560,
		.yres = 1440,
		.id = "2560x1440p60hz"
	},
	{
		.tvmode = TVMODE_2560x1600p60hz,
		.xres = 2560,
		.yres = 1600,
		.id = "2560x1600p60hz"
	},
	{
		.tvmode = TVMODE_3440x1440p60hz,
		.xres = 3440,
		.yres = 1440,
		.id = "3440x1440p60hz"
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
};

#endif /* TVREGS_H */
