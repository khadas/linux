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
	{ENCI_VIDEO_EN,                   1,         },
	{ENCI_VIDEO_SAT,                  0x7        },
	{VENC_VDAC_DAC0_FILT_CTRL0,       0x1        },
	{VENC_VDAC_DAC0_FILT_CTRL1,       0xfc48     },
	{ENCI_MACV_N0,                    0x0        },
	{MREG_END_MARKER,                 0          }
};

static const struct reg_s tvregs_576p_clk[] = {
	{MREG_END_MARKER,            0          }
};

static const struct reg_s tvregs_576p_enc[] = {
	{MREG_END_MARKER,            0          }
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

static const struct reg_s tvregs_1080p_30hz_enc[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_1080p_25hz_enc[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_1080p_50hz_clk[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_1080p_25hz_clk[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_1080p_30hz_clk[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_1080p_50hz_enc[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_1080p_24hz_clk[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_1080p_24hz_enc[] = {
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_4k2k_30hz_enc[] = {
	{MREG_END_MARKER,            0      },
};

static const struct reg_s tvregs_4k2k_25hz_enc[] = {
	{MREG_END_MARKER,            0      },
};

static const struct reg_s tvregs_4k2k_24hz_enc[] = {
	{MREG_END_MARKER,            0      },
};

static const struct reg_s tvregs_4k2k_smpte_enc[] = {
	{MREG_END_MARKER,            0      },
};

static const struct reg_s tvregs_4k2k_smpte_25hz_enc[] = {
	{MREG_END_MARKER,            0      },
};

static const struct reg_s tvregs_4k2k_smpte_30hz_enc[] = {
	{MREG_END_MARKER,            0      },
};

static const struct reg_s tvregs_4k2k_smpte_50hz_enc[] = {
	{MREG_END_MARKER,            0      },
};

static const struct reg_s tvregs_4k2k_smpte_60hz_enc[] = {
	{MREG_END_MARKER,            0      },
};

static const struct reg_s tvregs_vga_640x480_clk[] = { /* 25.17mhz 800 *525 */
	{MREG_END_MARKER,            0      },
};

static const struct reg_s tvregs_vga_640x480_enc[] = { /* 25.17mhz 800 *525 */
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_svga_800x600_clk[] = { /* 39.5mhz 1056 *628 */
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_svga_800x600_enc[] = { /* 39.5mhz 1056 *628 */
	{MREG_END_MARKER,            0      }
};

static const struct reg_s tvregs_xga_1024x768_clk[] = {
	{MREG_END_MARKER,            0      }
};
static const struct reg_s tvregs_xga_1024x768_enc[] = {
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
	{TVMODE_1080P_30HZ, tvregs_1080p_30hz_clk, tvregs_1080p_30hz_enc},
	{TVMODE_1080P_50HZ, tvregs_1080p_50hz_clk, tvregs_1080p_50hz_enc},
	{TVMODE_1080P_25HZ, tvregs_1080p_25hz_clk, tvregs_1080p_25hz_enc},
	{TVMODE_1080P_24HZ, tvregs_1080p_24hz_clk, tvregs_1080p_24hz_enc},
	{TVMODE_4K2K_30HZ, NULL, tvregs_4k2k_30hz_enc},
	{TVMODE_4K2K_25HZ, NULL, tvregs_4k2k_25hz_enc},
	{TVMODE_4K2K_24HZ, NULL, tvregs_4k2k_24hz_enc},
	{TVMODE_4K2K_SMPTE, NULL, tvregs_4k2k_smpte_enc},
	{TVMODE_4K2K_SMPTE_25HZ, NULL, tvregs_4k2k_smpte_25hz_enc},
	{TVMODE_4K2K_SMPTE_30HZ, NULL, tvregs_4k2k_smpte_30hz_enc},
	{TVMODE_4K2K_SMPTE_50HZ, NULL, tvregs_4k2k_smpte_50hz_enc},
	{TVMODE_4K2K_SMPTE_60HZ, NULL, tvregs_4k2k_smpte_60hz_enc},
	{TVMODE_VGA, tvregs_vga_640x480_clk, tvregs_vga_640x480_enc},
	{TVMODE_SVGA, tvregs_svga_800x600_clk, tvregs_svga_800x600_enc},
	{TVMODE_XGA, tvregs_xga_1024x768_clk, tvregs_xga_1024x768_enc},
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
