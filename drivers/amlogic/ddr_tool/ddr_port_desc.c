// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/arm-smccc.h>
#include <linux/slab.h>
#include "ddr_port.h"

/*
 * NOTE:
 * Port "DEVICE" is total name for small bandwidth device.
 * There are many small bandwidth devices such as nand/arb/parser
 * connected to dmc under port "device", for better configure of
 * these devices, re-number them with start ID of 32
 *
 * EXAMPLE:
 *
 *            DMC CONTROLLER
 *                   |
 *   ---------------------------------
 *   |    |    | ..... |       | ... |
 *  arm  mali vpu    device   vdec  hevc
 *                     |
 *         ------------------------
 *         |    |     | ... |     |
 *       emmc  ge2d  usb  audio  spicc
 */
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static struct ddr_port_desc ddr_port_desc_m8b[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "MALI0"         },
	{ .port_id =  2, .port_name = "MALI1"         },
	{ .port_id =  3, .port_name = "HEVC"          },
	{ .port_id =  8, .port_name = "VPU0"          },
	{ .port_id =  9, .port_name = "VPU1"          },
	{ .port_id = 10, .port_name = "VPU2"          },
	{ .port_id = 11, .port_name = "VDEC"          },
	{ .port_id = 12, .port_name = "HCODEC"        },
	{ .port_id = 14, .port_name = "AUDIO"         },
	{ .port_id = 15, .port_name = "DEVICE"        },
	/* start of each device */
	{ .port_id = 33, .port_name = "NAND"          },
	{ .port_id = 34, .port_name = "BLKMV"         },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "USB0"          },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "GE2D SOURCE1"  },
	{ .port_id = 39, .port_name = "GE2D SOURCE2"  },
	{ .port_id = 40, .port_name = "GE2D DEST"     },
	{ .port_id = 41, .port_name = "PARSER"        },
	{ .port_id = 42, .port_name = "SANA"          },
	{ .port_id = 43, .port_name = "SDIO2"         },
	{ .port_id = 44, .port_name = "SPICC"         },
	{ .port_id = 45, .port_name = "ETHERNET"      }
};

static struct ddr_port_desc ddr_port_desc_gxbb[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "MALI0"         },
	{ .port_id =  2, .port_name = "MALI1"         },
	{ .port_id =  3, .port_name = "HDCP"          },
	{ .port_id =  4, .port_name = "HEVC"          },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id =  8, .port_name = "VPU0 READ"     },
	{ .port_id =  9, .port_name = "VPU1 READ"     },
	{ .port_id = 10, .port_name = "VPU2 READ"     },
	{ .port_id = 11, .port_name = "VPU0 WRITE"    },
	{ .port_id = 12, .port_name = "VPU1 WRITE"    },
	{ .port_id = 13, .port_name = "VDEC"          },
	{ .port_id = 14, .port_name = "HCODEC"        },
	{ .port_id = 15, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 32, .port_name = "SD_EMMC_A"     },
	{ .port_id = 33, .port_name = "USB0"          },
	{ .port_id = 34, .port_name = "BLKMV"         },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "AUDIO OUT"     },
	{ .port_id = 39, .port_name = "AUDIO IN"      },
	{ .port_id = 40, .port_name = "AIU"           },
	{ .port_id = 41, .port_name = "PARSER"        },
	{ .port_id = 42, .port_name = "AO CPU"        },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC"         },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 46, .port_name = "SANA"          }
};

static struct ddr_port_desc ddr_port_desc_gxl[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "MALI0"         },
	{ .port_id =  2, .port_name = "MALI1"         },
	{ .port_id =  3, .port_name = "HDCP"          },
	{ .port_id =  4, .port_name = "HEVC"          },
	{ .port_id =  5, .port_name = "TEST"          },
	{ .port_id =  6, .port_name = "USB3.0"        },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id =  8, .port_name = "VPU0 READ"     },
	{ .port_id =  9, .port_name = "VPU1 READ"     },
	{ .port_id = 10, .port_name = "VPU2 READ"     },
	{ .port_id = 11, .port_name = "VPU0 WRITE"    },
	{ .port_id = 12, .port_name = "VPU1 WRITE"    },
	{ .port_id = 13, .port_name = "VDEC"          },
	{ .port_id = 14, .port_name = "HCODEC"        },
	{ .port_id = 15, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 32, .port_name = "SD_EMMC_A"     },
	{ .port_id = 33, .port_name = "USB0"          },
	{ .port_id = 34, .port_name = "DMA"           },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "AUDIO OUT"     },
	{ .port_id = 39, .port_name = "AUDIO IN"      },
	{ .port_id = 40, .port_name = "AIU"           },
	{ .port_id = 41, .port_name = "PARSER"        },
	{ .port_id = 42, .port_name = "AO CPU"        },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC"         },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 46, .port_name = "SANA"          }
};

static struct ddr_port_desc ddr_port_desc_gxm[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "MALI"          },
	{ .port_id =  2, .port_name = "H265ENC"       },
	{ .port_id =  3, .port_name = "HDCP"          },
	{ .port_id =  4, .port_name = "HEVC"          },
	{ .port_id =  5, .port_name = "TEST"          },
	{ .port_id =  6, .port_name = "USB3.0"        },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id =  8, .port_name = "VPU0 READ"     },
	{ .port_id =  9, .port_name = "VPU1 READ"     },
	{ .port_id = 10, .port_name = "VPU2 READ"     },
	{ .port_id = 11, .port_name = "VPU0 WRITE"    },
	{ .port_id = 12, .port_name = "VPU1 WRITE"    },
	{ .port_id = 13, .port_name = "VDEC"          },
	{ .port_id = 14, .port_name = "HCODEC"        },
	{ .port_id = 15, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 32, .port_name = "SD_EMMC_A"     },
	{ .port_id = 33, .port_name = "USB0"          },
	{ .port_id = 34, .port_name = "DMA"           },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "AUDIO OUT"     },
	{ .port_id = 39, .port_name = "AUDIO IN"      },
	{ .port_id = 40, .port_name = "AIU"           },
	{ .port_id = 41, .port_name = "PARSER"        },
	{ .port_id = 42, .port_name = "AO CPU"        },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC"         },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 46, .port_name = "SANA"          }
};

static struct ddr_port_desc ddr_port_desc_gxlx[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "MALI0"         },
	{ .port_id =  2, .port_name = "MALI1"         },
	{ .port_id =  3, .port_name = "HDCP"          },
	{ .port_id =  4, .port_name = "HEVC"          },
	{ .port_id =  5, .port_name = "H265ENC/TEST"  },
	{ .port_id =  6, .port_name = "USB3.0"        },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id =  8, .port_name = "VPU0 READ"     },
	{ .port_id =  9, .port_name = "VPU1 READ"     },
	{ .port_id = 10, .port_name = "VPU2 READ"     },
	{ .port_id = 11, .port_name = "VPU0 WRITE"    },
	{ .port_id = 12, .port_name = "VPU1 WRITE"    },
	{ .port_id = 13, .port_name = "VDEC"          },
	{ .port_id = 14, .port_name = "HCODEC"        },
	{ .port_id = 15, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 32, .port_name = "SD_EMMC_A"     },
	{ .port_id = 33, .port_name = "USB0"          },
	{ .port_id = 34, .port_name = "DMA"           },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "AUDIO OUT"     },
	{ .port_id = 39, .port_name = "AUDIO IN"      },
	{ .port_id = 40, .port_name = "AIU"           },
	{ .port_id = 41, .port_name = "PARSER"        },
	{ .port_id = 42, .port_name = "AO CPU"        },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC"         },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 46, .port_name = "SANA"          }
};

static struct ddr_port_desc ddr_port_desc_axg[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "PCIE-A"        },
	{ .port_id =  2, .port_name = "PCIE-B"        },
	{ .port_id =  3, .port_name = "GE2D"          },
	{ .port_id =  4, .port_name = "AUDIO"         },
	{ .port_id =  5, .port_name = "TEST"          },
	{ .port_id =  6, .port_name = "USB3.0"        },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id =  8, .port_name = "VPU0 READ"     },
	/* start of each device */
	{ .port_id = 34, .port_name = "DMA"           },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 42, .port_name = "AO CPU"        },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC0"        },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 46, .port_name = "SPICC1"        }
};

static struct ddr_port_desc ddr_port_desc_txl[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "MALI0"         },
	{ .port_id =  2, .port_name = "MALI1"         },
	{ .port_id =  3, .port_name = "HDCP"          },
	{ .port_id =  4, .port_name = "HEVC"          },
	{ .port_id =  5, .port_name = "TEST"          },
	{ .port_id =  6, .port_name = "USB3.0"        },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id =  8, .port_name = "VPU0 READ"     },
	{ .port_id =  9, .port_name = "VPU1 READ"     },
	{ .port_id = 10, .port_name = "VPU2 READ"     },
	{ .port_id = 11, .port_name = "VPU0 WRITE"    },
	{ .port_id = 12, .port_name = "VPU1 WRITE"    },
	{ .port_id = 13, .port_name = "VDEC"          },
	{ .port_id = 14, .port_name = "HCODEC"        },
	{ .port_id = 15, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 32, .port_name = "SD_EMMC_A"     },
	{ .port_id = 33, .port_name = "USB0"          },
	{ .port_id = 34, .port_name = "DMA"           },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "AUDIO OUT"     },
	{ .port_id = 39, .port_name = "AUDIO IN"      },
	{ .port_id = 40, .port_name = "AIU"           },
	{ .port_id = 41, .port_name = "PARSER"        },
	{ .port_id = 42, .port_name = "AO CPU"        },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC"         },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 46, .port_name = "SANA"          },
	{ .port_id = 47, .port_name = "DEMOD"         }
};

static struct ddr_port_desc ddr_port_desc_txlx[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "MALI0"         },
	{ .port_id =  2, .port_name = "MALI1"         },
	{ .port_id =  3, .port_name = "HDCP"          },
	{ .port_id =  4, .port_name = "HEVC"          },
	{ .port_id =  5, .port_name = "TEST"          },
	{ .port_id =  6, .port_name = "USB3.0"        },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id =  8, .port_name = "VPU0 READ"     },
	{ .port_id =  9, .port_name = "VPU1 READ"     },
	{ .port_id = 10, .port_name = "VPU2 READ"     },
	{ .port_id = 11, .port_name = "VPU0 WRITE"    },
	{ .port_id = 12, .port_name = "VPU1 WRITE"    },
	{ .port_id = 13, .port_name = "VDEC"          },
	{ .port_id = 14, .port_name = "HCODEC"        },
	{ .port_id = 15, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 32, .port_name = "SPICC1"        },
	{ .port_id = 33, .port_name = "USB0"          },
	{ .port_id = 34, .port_name = "DMA"           },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "AUDIO OUT"     },
	{ .port_id = 39, .port_name = "AUDIO IN"      },
	{ .port_id = 40, .port_name = "AIU"           },
	{ .port_id = 41, .port_name = "PARSER"        },
	{ .port_id = 42, .port_name = "AO CPU"        },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC"         },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 46, .port_name = "SANA"          },
	{ .port_id = 47, .port_name = "DEMOD"         }
};

static struct ddr_port_desc ddr_port_desc_txhd[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "MALI0"         },
	{ .port_id =  2, .port_name = "MALI1"         },
	{ .port_id =  4, .port_name = "HEVC"          },
	{ .port_id =  5, .port_name = "TEST"          },
	{ .port_id =  6, .port_name = "USB3.0"        },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id =  8, .port_name = "VPU0 READ"     },
	{ .port_id =  9, .port_name = "VPU1 READ"     },
	{ .port_id = 10, .port_name = "VPU2 READ"     },
	{ .port_id = 11, .port_name = "VPU0 WRITE"    },
	{ .port_id = 12, .port_name = "VPU1 WRITE"    },
	{ .port_id = 13, .port_name = "VDEC"          },
	{ .port_id = 14, .port_name = "HCODEC"        },
	{ .port_id = 15, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 33, .port_name = "USB0"          },
	{ .port_id = 34, .port_name = "DMA"           },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "AUDIO OUT"     },
	{ .port_id = 39, .port_name = "AUDIO IN"      },
	{ .port_id = 40, .port_name = "AIU"           },
	{ .port_id = 41, .port_name = "PARSER"        },
	{ .port_id = 42, .port_name = "AO CPU"        },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC"         },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 46, .port_name = "SANA"          },
	{ .port_id = 47, .port_name = "DEMOD"         }
};

static struct ddr_port_desc ddr_port_desc_a1[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "DSPA"          },
	{ .port_id =  2, .port_name = "DSPB"          },
	{ .port_id =  4, .port_name = "DEVICE"        },
	{ .port_id =  5, .port_name = "USB_AHB"       },
	{ .port_id =  6, .port_name = "PROD_I2C"      },
	{ .port_id =  7, .port_name = "USB HOST"      },
	/* start of each device */
	{ .port_id = 33, .port_name = "SPICC"         },
	{ .port_id = 35, .port_name = "SD_EMMC_A"     },
	{ .port_id = 37, .port_name = "AUDIO"         },
	{ .port_id = 38, .port_name = "DMA"           }
};

static struct ddr_port_desc ddr_port_desc_c1[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "DSPA"          },
	{ .port_id =  2, .port_name = "DSPB"          },
	{ .port_id =  3, .port_name = "WAVE"          },
	{ .port_id =  4, .port_name = "GDC"           },
	{ .port_id =  5, .port_name = "GE2D"          },
	{ .port_id =  6, .port_name = "ISP"           },
	{ .port_id =  7, .port_name = "NNA"           },
	{ .port_id =  8, .port_name = "JPEG"          },
	{ .port_id =  9, .port_name = "ETHERNET"      },
	{ .port_id = 10, .port_name = "SPICC_A"       },
	{ .port_id = 11, .port_name = "SPICC_B"       },
	{ .port_id = 12, .port_name = "SD_EMMCA"      },
	{ .port_id = 13, .port_name = "SD_EMMCB"      },
	{ .port_id = 14, .port_name = "AUDIO"         },
	{ .port_id = 15, .port_name = "DMA"           },
	{ .port_id = 16, .port_name = "NAND_EMMCC"    },
	{ .port_id = 17, .port_name = "USB_AHB"       },
	{ .port_id = 18, .port_name = "USB_AXI"       },
	{ .port_id = 19, .port_name = "I2C"           },
	{ .port_id = 20, .port_name = "DMC_TEST"      }
};
#endif

static struct ddr_port_desc ddr_port_desc_tl1[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "MALI0"         },
	{ .port_id =  3, .port_name = "HDCP_HDMI"     },
	{ .port_id =  4, .port_name = "HEVC FRONT"    },
	{ .port_id =  5, .port_name = "TEST"          },
	{ .port_id =  6, .port_name = "USB3.0"        },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id =  8, .port_name = "HEVC BACK"     },
	{ .port_id = 16, .port_name = "VPU0 READ"     },
	{ .port_id = 17, .port_name = "VPU1 READ"     },
	{ .port_id = 18, .port_name = "VPU2 READ"     },
	{ .port_id = 19, .port_name = "VPU0 WRITE"    },
	{ .port_id = 20, .port_name = "VPU1 WRITE"    },
	{ .port_id = 21, .port_name = "VDEC"          },
	{ .port_id = 22, .port_name = "HCODEC"        },
	{ .port_id = 23, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 32, .port_name = "SPICC1"        },
	{ .port_id = 33, .port_name = "USB0"          },
	{ .port_id = 34, .port_name = "DMA"           },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "AUDIO"         },
	{ .port_id = 39, .port_name = "AIFIFO"        },
	{ .port_id = 41, .port_name = "PARSER"        },
	{ .port_id = 42, .port_name = "AO CPU"        },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC2"        },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 46, .port_name = "SANA"          },
	{ .port_id = 47, .port_name = "DEMOD"         }
};

static struct ddr_port_desc ddr_port_desc_g12a[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "MALI"          },
	{ .port_id =  2, .port_name = "PCIE"          },
	{ .port_id =  3, .port_name = "HDCP"          },
	{ .port_id =  4, .port_name = "HEVC FRONT"    },
	{ .port_id =  5, .port_name = "TEST"          },
	{ .port_id =  6, .port_name = "USB3.0"        },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id =  8, .port_name = "HEVC BACK"     },
	{ .port_id =  9, .port_name = "H265ENC"       },
	{ .port_id = 16, .port_name = "VPU0 READ"     },
	{ .port_id = 17, .port_name = "VPU1 READ"     },
	{ .port_id = 18, .port_name = "VPU2 READ"     },
	{ .port_id = 19, .port_name = "VPU0 WRITE"    },
	{ .port_id = 20, .port_name = "VPU1 WRITE"    },
	{ .port_id = 21, .port_name = "VDEC"          },
	{ .port_id = 22, .port_name = "HCODEC"        },
	{ .port_id = 23, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 32, .port_name = "SPICC1"        },
	{ .port_id = 33, .port_name = "USB0"          },
	{ .port_id = 34, .port_name = "DMA"           },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "AUDIO"         },
	{ .port_id = 39, .port_name = "AIFIFO"        },
	{ .port_id = 41, .port_name = "PARSER"        },
	{ .port_id = 42, .port_name = "AO CPU"        },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC2"        },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 46, .port_name = "SANA"          }
};

static struct ddr_port_desc ddr_port_desc_g12b[] __initdata = {
	{ .port_id =  0, .port_name = "ARM-ADDR-E"    },
	{ .port_id =  1, .port_name = "MALI"          },
	{ .port_id =  2, .port_name = "PCIE"          },
	{ .port_id =  3, .port_name = "HDCP"          },
	{ .port_id =  4, .port_name = "HEVC FRONT"    },
	{ .port_id =  5, .port_name = "TEST"          },
	{ .port_id =  6, .port_name = "USB3.0"        },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id =  8, .port_name = "HEVC BACK"     },
	{ .port_id =  9, .port_name = "H265ENC"       },
	{ .port_id = 10, .port_name = "NNA"           },
	{ .port_id = 11, .port_name = "GDC"           },
	{ .port_id = 12, .port_name = "MIPI ISP"      },
	{ .port_id = 13, .port_name = "ARM-ADDR-F"    },
	{ .port_id = 16, .port_name = "VPU0 READ"     },
	{ .port_id = 17, .port_name = "VPU1 READ"     },
	{ .port_id = 18, .port_name = "VPU2 READ"     },
	{ .port_id = 19, .port_name = "VPU0 WRITE"    },
	{ .port_id = 20, .port_name = "VPU1 WRITE"    },
	{ .port_id = 21, .port_name = "VDEC"          },
	{ .port_id = 22, .port_name = "HCODEC"        },
	{ .port_id = 23, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 32, .port_name = "SPICC1"        },
	{ .port_id = 33, .port_name = "USB0"          },
	{ .port_id = 34, .port_name = "DMA"           },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "AUDIO"         },
	{ .port_id = 39, .port_name = "AIFIFO"        },
	{ .port_id = 40, .port_name = "SD_EMMC_A"     },
	{ .port_id = 41, .port_name = "PARSER"        },
	{ .port_id = 42, .port_name = "AO CPU"        },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC2"        },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 46, .port_name = "SANA"          }
};

static struct ddr_port_desc ddr_port_desc_sm1[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "MALI"          },
	{ .port_id =  2, .port_name = "PCIE"          },
	{ .port_id =  3, .port_name = "HDCP"          },
	{ .port_id =  4, .port_name = "HEVC FRONT"    },
	{ .port_id =  5, .port_name = "TEST"          },
	{ .port_id =  6, .port_name = "USB3.0"        },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id =  8, .port_name = "HEVC BACK"     },
	{ .port_id =  9, .port_name = "H265ENC"       },
	{ .port_id = 10, .port_name = "NNA"           },
	{ .port_id = 16, .port_name = "VPU0 READ"     },
	{ .port_id = 17, .port_name = "VPU1 READ"     },
	{ .port_id = 18, .port_name = "VPU2 READ"     },
	{ .port_id = 19, .port_name = "VPU0 WRITE"    },
	{ .port_id = 20, .port_name = "VPU1 WRITE"    },
	{ .port_id = 21, .port_name = "VDEC"          },
	{ .port_id = 22, .port_name = "HCODEC"        },
	{ .port_id = 23, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 32, .port_name = "SPICC1"        },
	{ .port_id = 33, .port_name = "USB0"          },
	{ .port_id = 34, .port_name = "DMA"           },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "AUDIO"         },
	{ .port_id = 39, .port_name = "AIFIFO"        },
	{ .port_id = 41, .port_name = "PARSER"        },
	{ .port_id = 42, .port_name = "AO CPU"        },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC2"        },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 46, .port_name = "SANA"          }
};

static struct ddr_port_desc ddr_port_desc_tm2[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "MALI"          },
	{ .port_id =  2, .port_name = "PCIE"          },
	{ .port_id =  3, .port_name = "HDCP"          },
	{ .port_id =  4, .port_name = "HEVC FRONT"    },
	{ .port_id =  5, .port_name = "TEST"          },
	{ .port_id =  6, .port_name = "USB3.0"        },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id =  8, .port_name = "HEVC BACK"     },
	{ .port_id =  9, .port_name = "DSPA"          },
	{ .port_id = 10, .port_name = "DSPB"          },
	{ .port_id = 11, .port_name = "NNA"           },
	{ .port_id = 12, .port_name = "PCIE1"         },
	{ .port_id = 16, .port_name = "VPU0 READ"     },
	{ .port_id = 17, .port_name = "VPU1 READ"     },
	{ .port_id = 18, .port_name = "VPU2 READ"     },
	{ .port_id = 19, .port_name = "VPU0 WRITE"    },
	{ .port_id = 20, .port_name = "VPU1 WRITE"    },
	{ .port_id = 21, .port_name = "VDEC"          },
	{ .port_id = 22, .port_name = "HCODEC"        },
	{ .port_id = 23, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 32, .port_name = "SPICC1"        },
	{ .port_id = 33, .port_name = "USB0"          },
	{ .port_id = 34, .port_name = "DMA"           },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "AUDIO"         },
	{ .port_id = 39, .port_name = "AIFIFO"        },
	{ .port_id = 40, .port_name = "PARSER1"       },
	{ .port_id = 41, .port_name = "PARSER"        },
	{ .port_id = 42, .port_name = "AO CPU"        },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC2"        },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 46, .port_name = "SANA"          },
	{ .port_id = 47, .port_name = "DEMODE"        }
};

static struct ddr_port_desc ddr_port_desc_t5[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "MALI"          },
	{ .port_id =  2, .port_name = "PCIE"          },
	{ .port_id =  3, .port_name = "HDCP"          },
	{ .port_id =  4, .port_name = "HEVC"          },
	{ .port_id =  5, .port_name = "TEST"          },
	{ .port_id =  6, .port_name = "USB3.0"        },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id = 16, .port_name = "VPU0 READ"     },
	{ .port_id = 17, .port_name = "VPU1 READ"     },
	{ .port_id = 18, .port_name = "VPU2 READ"     },
	{ .port_id = 19, .port_name = "VPU0 WRITE"    },
	{ .port_id = 20, .port_name = "VPU1 WRITE"    },
	{ .port_id = 21, .port_name = "VDEC"          },
	{ .port_id = 23, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 33, .port_name = "USB0"          },
	{ .port_id = 34, .port_name = "DMA"           },
	{ .port_id = 35, .port_name = "ARB0"          },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "AUDIO"         },
	{ .port_id = 39, .port_name = "AIFIFO"        },
	{ .port_id = 40, .port_name = "PARSER1"       },
	{ .port_id = 41, .port_name = "PARSER"        },
	{ .port_id = 42, .port_name = "AO CPU"        },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC"         },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 46, .port_name = "SANA"          },
	{ .port_id = 47, .port_name = "DEMODE"        }
};

static struct ddr_port_desc ddr_port_desc_t7[] __initdata = {
	/* CCI */
	{ .port_id =  1, .port_name = "ARM_A53"       },
	{ .port_id =  2, .port_name = "ARM_A73"       },
	{ .port_id =  3, .port_name = "MALI0"         },
	{ .port_id =  4, .port_name = "MALI1"         },
	{ .port_id =  5, .port_name = "CCI"           },
	/* NIC1 */
	{ .port_id =  8, .port_name = "NNA0"          },
	{ .port_id =  9, .port_name = "NNA1"          },
	{ .port_id = 10, .port_name = "NNA2"          },
	{ .port_id = 11, .port_name = "NNA3"          },
	/* NIC2 */
	{ .port_id = 16, .port_name = "HDMI_RX"       },
	{ .port_id = 17, .port_name = "USB2"          },
	{ .port_id = 18, .port_name = "USB3"          },
	{ .port_id = 19, .port_name = "PCIE"          },
	{ .port_id = 24, .port_name = "SPICC0"        },
	{ .port_id = 25, .port_name = "SPICC1"        },
	{ .port_id = 26, .port_name = "SDEMMCA"       },
	{ .port_id = 27, .port_name = "SDEMMCB"       },
	{ .port_id = 28, .port_name = "SDEMMCC"       },
	{ .port_id = 29, .port_name = "SPICC2"        },
	{ .port_id = 34, .port_name = "ETHERNET"      },
	{ .port_id = 35, .port_name = "AFIFO"         },
	{ .port_id = 36, .port_name = "AUDMA"         },
	{ .port_id = 37, .port_name = "SPICC3"        },
	{ .port_id = 38, .port_name = "SPICC4"        },
	{ .port_id = 39, .port_name = "SPICC5"        },
	/* NIC3 */
	{ .port_id = 48, .port_name = "GE2D"          },
	{ .port_id = 49, .port_name = "GDC"           },
	{ .port_id = 50, .port_name = "DEWARP"        },
	{ .port_id = 51, .port_name = "VDEC"          },
	{ .port_id = 52, .port_name = "HEVC_F"        },
	{ .port_id = 53, .port_name = "HEVC_B"        },
	{ .port_id = 54, .port_name = "HCODEC"        },
	{ .port_id = 55, .port_name = "WAVE521"       },
	/* NIC4 */
	{ .port_id = 63, .port_name = "DEMUX"         },
	{ .port_id = 64, .port_name = "ISP0"          },
	{ .port_id = 65, .port_name = "ISP1"          },
	{ .port_id = 66, .port_name = "ISP2"          },
	{ .port_id = 67, .port_name = "ISP3"          },
	{ .port_id = 68, .port_name = "DSPA"          },
	{ .port_id = 69, .port_name = "DSPB"          },
	{ .port_id = 70, .port_name = "AOCPU"         },
	{ .port_id = 71, .port_name = "JTAG"          },
	{ .port_id = 72, .port_name = "AUDIO"         },
	{ .port_id = 73, .port_name = "DMA"           },
	/* NIC5 */
	{ .port_id = 80, .port_name = "VPU0"          },
	{ .port_id = 81, .port_name = "VPU1"          },
	{ .port_id = 82, .port_name = "VPU2"          }
};

static struct ddr_port_desc ddr_port_desc_t3[] __initdata = {
	{ .port_id =  2, .port_name = "ARM_A55"       },
	{ .port_id =  4, .port_name = "MALI"          },
	/* NoC_dev */
	{ .port_id = 16, .port_name = "HDMI_RX"       },
	{ .port_id = 17, .port_name = "USB2"          },
	{ .port_id = 18, .port_name = "USB3"          },
	{ .port_id = 19, .port_name = "PCIE"          },
	{ .port_id = 20, .port_name = "TVFE"          },
	{ .port_id = 21, .port_name = "USB2_DEVICE"   },
	{ .port_id = 27, .port_name = "SDEMMCB"       },
	{ .port_id = 28, .port_name = "SDEMMCC"       },
	{ .port_id = 34, .port_name = "ETHERNET"      },
	{ .port_id = 35, .port_name = "AFIFO"         },
	{ .port_id = 36, .port_name = "AUDMA"         },
	{ .port_id = 37, .port_name = "SPICC0"        },
	{ .port_id = 38, .port_name = "SPICC1"        },
	{ .port_id = 39, .port_name = "SPICC2"        },
	/* NoC_dos */
	{ .port_id = 48, .port_name = "GE2D"          },
	{ .port_id = 51, .port_name = "VDEC"          },
	{ .port_id = 52, .port_name = "HEVC_F"        },
	{ .port_id = 53, .port_name = "HEVC_B"        },
	{ .port_id = 54, .port_name = "HCODEC"        },
	/* NoC_sys */
	{ .port_id = 63, .port_name = "DEMUX"         },
	{ .port_id = 64, .port_name = "NNA"           },
	{ .port_id = 68, .port_name = "DSP"           },
	{ .port_id = 70, .port_name = "AOCPU"         },
	{ .port_id = 71, .port_name = "JTAG"          },
	{ .port_id = 72, .port_name = "AUDIO"         },
	{ .port_id = 73, .port_name = "DMA"           },
	{ .port_id = 74, .port_name = "DEMOD"         },
	/* NoC_vpu */
	{ .port_id = 80, .port_name = "VPU0"          },
	{ .port_id = 81, .port_name = "VPU1"          },
	{ .port_id = 82, .port_name = "VPU2"          },
	{ .port_id = 83, .port_name = "FRC0"          },
	{ .port_id = 84, .port_name = "FRC1"          },
	{ .port_id = 85, .port_name = "FRC2"          }
};

static struct ddr_port_desc ddr_port_desc_s4[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "MALI"          },
	{ .port_id =  3, .port_name = "HDCP"          },
	{ .port_id =  4, .port_name = "HEVC"          },
	{ .port_id =  5, .port_name = "TEST"          },
	{ .port_id =  6, .port_name = "USB3.0"        },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id = 11, .port_name = "DEMUX"         },
	{ .port_id = 16, .port_name = "VPU0 READ"     },
	{ .port_id = 17, .port_name = "VPU1 READ"     },
	{ .port_id = 18, .port_name = "VPU2 READ"     },
	{ .port_id = 19, .port_name = "VPU0 WRITE"    },
	{ .port_id = 20, .port_name = "VPU1 WRITE"    },
	{ .port_id = 21, .port_name = "VDEC"          },
	{ .port_id = 23, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 33, .port_name = "USB0"          },
	{ .port_id = 34, .port_name = "DMA"           },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "AUDIO"         },
	{ .port_id = 39, .port_name = "AIFIFO"        },
	{ .port_id = 40, .port_name = "SD_EMMC_A"     },
	{ .port_id = 42, .port_name = "AUDMA"         },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC2"        },
	{ .port_id = 45, .port_name = "ETHERNET"      },
	{ .port_id = 47, .port_name = "DEMOD"         }
};

static struct ddr_port_desc ddr_port_desc_sc2[] __initdata = {
	{ .port_id =  0, .port_name = "ARM"           },
	{ .port_id =  1, .port_name = "MALI"          },
	{ .port_id =  2, .port_name = "PCIE"          },
	{ .port_id =  3, .port_name = "HDCP"          },
	{ .port_id =  4, .port_name = "HEVC FRONT"    },
	{ .port_id =  5, .port_name = "TEST"          },
	{ .port_id =  6, .port_name = "USB3.0"        },
	{ .port_id =  7, .port_name = "DEVICE"        },
	{ .port_id =  8, .port_name = "HEVC BACK"     },
	{ .port_id =  9, .port_name = "H265ENC"       },
	{ .port_id = 11, .port_name = "DEMUX"         },
	{ .port_id = 12, .port_name = "DSPA"          },
	{ .port_id = 16, .port_name = "VPU0 READ"     },
	{ .port_id = 17, .port_name = "VPU1 READ"     },
	{ .port_id = 18, .port_name = "VPU2 READ"     },
	{ .port_id = 19, .port_name = "VPU0 WRITE"    },
	{ .port_id = 20, .port_name = "VPU1 WRITE"    },
	{ .port_id = 21, .port_name = "VDEC"          },
	{ .port_id = 22, .port_name = "HCODEC"        },
	{ .port_id = 23, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 32, .port_name = "SPICC1"        },
	{ .port_id = 33, .port_name = "USB0"          },
	{ .port_id = 34, .port_name = "DMA"           },
	{ .port_id = 36, .port_name = "SD_EMMC_B"     },
	{ .port_id = 37, .port_name = "USB1"          },
	{ .port_id = 38, .port_name = "AUDIO"         },
	{ .port_id = 39, .port_name = "AIFIFO"        },
	{ .port_id = 40, .port_name = "SD_EMMC_A"     },
	{ .port_id = 42, .port_name = "AUDMA"         },
	{ .port_id = 43, .port_name = "SD_EMMC_C"     },
	{ .port_id = 44, .port_name = "SPICC2"        },
	{ .port_id = 45, .port_name = "ETHERNET"      }
};

static struct ddr_port_desc ddr_port_desc_p1[] __initdata = {
	/* TOP*/
	{ .port_id =  2, .port_name = "ARM_A76/A55"   },
	{ .port_id =  5, .port_name = "SMMU"          },
	/* NIC_NNA_0 */
	{ .port_id =  8, .port_name = "NNA_A"         },
	{ .port_id =  9, .port_name = "NNA_B"         },
	{ .port_id = 10, .port_name = "NNA_C"         },
	/* NIC_NNA_1 */
	{ .port_id = 11, .port_name = "NNA_D"         },
	{ .port_id = 12, .port_name = "NNA_E"         },
	{ .port_id = 13, .port_name = "NNA_F"         },
	/* NIC_ISP */
	{ .port_id = 16, .port_name = "ISP_A"         },
	{ .port_id = 17, .port_name = "ISP_B"         },
	{ .port_id = 18, .port_name = "ISP_C"         },
	{ .port_id = 19, .port_name = "ISP_D"         },
	{ .port_id = 20, .port_name = "ISP_E"         },
	{ .port_id = 21, .port_name = "ISP_F"         },
	{ .port_id = 22, .port_name = "ISP_G"         },
	/* NIC_FDLE*/
	{ .port_id = 32, .port_name = "MOP_A"         },
	{ .port_id = 33, .port_name = "MOP_B"         },
	{ .port_id = 34, .port_name = "DEP_A"         },
	{ .port_id = 35, .port_name = "DEP_B"         },
	/* NIC_VFE*/
	{ .port_id = 40, .port_name = "VFE"           },
	{ .port_id = 41, .port_name = "GE2D"          },
	{ .port_id = 42, .port_name = "DEWARP_A"      },
	{ .port_id = 43, .port_name = "DEWARP_B"      },
	{ .port_id = 44, .port_name = "DEWARP_C"      },
	/* NIC_SYS*/
	{ .port_id = 48, .port_name = "USB3_A"        },
	{ .port_id = 49, .port_name = "USB3_B"        },
	{ .port_id = 50, .port_name = "USB3_C"        },
	{ .port_id = 51, .port_name = "PCIE"          },
	{ .port_id = 67, .port_name = "M4"            },
	{ .port_id = 68, .port_name = "DSP_A"         },
	{ .port_id = 69, .port_name = "DSP_B"         },
	{ .port_id = 70, .port_name = "AO_CPU"        },
	{ .port_id = 71, .port_name = "JTAG"          },
	{ .port_id = 72, .port_name = "SPICC0"        },
	{ .port_id = 73, .port_name = "SPICC1"        },
	{ .port_id = 74, .port_name = "SDEMMCA"       },
	{ .port_id = 75, .port_name = "SDEMMCB"       },
	{ .port_id = 76, .port_name = "SDEMMCC"       },
	{ .port_id = 77, .port_name = "SPICC2"        },
	{ .port_id = 82, .port_name = "ETH"           },
	{ .port_id = 83, .port_name = "AIFIFO"        },
	{ .port_id = 84, .port_name = "AUDMA"         },
	{ .port_id = 85, .port_name = "SPICC3"        },
	{ .port_id = 86, .port_name = "SPICC4"        },
	{ .port_id = 87, .port_name = "SPICC5"        },
	{ .port_id = 88, .port_name = "AUDIO"         },
	{ .port_id = 96, .port_name = "EMMC"          },
	{ .port_id = 97, .port_name = "DMA"           }
};

static struct ddr_port_desc ddr_port_desc_t5w[] __initdata = {
	{ .port_id =  0, .port_name = "ARM_A55"       },
	{ .port_id =  1, .port_name = "MALI"          },
	{ .port_id =  3, .port_name = "HDMI"          },
	{ .port_id =  4, .port_name = "HEVC"          },
	{ .port_id =  6, .port_name = "USB"           },
	{ .port_id =  7, .port_name = "DEVICE0"       },
	{ .port_id =  8, .port_name = "TVFE"          },
	{ .port_id =  9, .port_name = "DEMUX"         },
	{ .port_id = 10, .port_name = "DEVICE1"       },
	{ .port_id = 16, .port_name = "VPU0 READ"     },
	{ .port_id = 17, .port_name = "VPU1 READ"     },
	{ .port_id = 18, .port_name = "VPU2 READ"     },
	{ .port_id = 19, .port_name = "VPU0 WRITE"    },
	{ .port_id = 20, .port_name = "VPU1 WRITE"    },
	{ .port_id = 21, .port_name = "VDEC"          },
	{ .port_id = 23, .port_name = "GE2D"          },
	/* start of each device */
	{ .port_id = 32, .port_name = "USB0"          },
	{ .port_id = 33, .port_name = "DMA"           },
	{ .port_id = 34, .port_name = "USB1"          },
	{ .port_id = 35, .port_name = "AUCPU"         },
	{ .port_id = 36, .port_name = "AO2DDR"        },
	{ .port_id = 37, .port_name = "ETH"           },
	{ .port_id = 38, .port_name = "SANA"          },
	{ .port_id = 39, .port_name = "DEMOD"         },
	{ .port_id = 40, .port_name = "SPICC1"        },
	{ .port_id = 41, .port_name = "ARBTODDR"      },
	{ .port_id = 42, .port_name = "SD_EMMC_B"     },
	{ .port_id = 43, .port_name = "AUDIO"         },
	{ .port_id = 44, .port_name = "AIFIFO"        },
	{ .port_id = 45, .port_name = "SPICC2"        },
	{ .port_id = 46, .port_name = "SD_EMMC_C"     },
	{ .port_id = 46, .port_name = "SPICC0"        }
};

static struct ddr_port_desc ddr_port_desc_a5[] __initdata = {
	{ .port_id =  0, .port_name = "ARM_A53"      },
	{ .port_id =  3, .port_name = "NNA"          },
	{ .port_id =  4, .port_name = "DSP"          },
	{ .port_id =  5, .port_name = "TEST"         },
	{ .port_id =  6, .port_name = "DEVICE1"      },
	{ .port_id =  7, .port_name = "DEVICE0"      },

	/* start of each device0 */
	{ .port_id = 32, .port_name = "SPICC1"       },
	{ .port_id = 33, .port_name = "ETH"	     },
	{ .port_id = 34, .port_name = "SPICC0"       },

	/* start of each device1 */
	{ .port_id = 40, .port_name = "SDIO_A"       },
	{ .port_id = 41, .port_name = "SEC_TOP"      },
	{ .port_id = 42, .port_name = "EMMC"         },
	{ .port_id = 43, .port_name = "USB"          },
	{ .port_id = 44, .port_name = "AUDIO"        }
};

static struct ddr_port_desc ddr_port_desc_s5[] __initdata = {
	/* TOP*/
	{ .port_id =  2, .port_name = "ARM_A76/A55"   },
	{ .port_id =  4, .port_name = "MALI"          },
	/* NIC_NPU */
	{ .port_id =  8, .port_name = "ADLA_4T_0"     },
	{ .port_id =  9, .port_name = "ADLA_4T_1"     },
	/* NIC_SYS */
	{ .port_id = 16, .port_name = "AOCPU"         },
	{ .port_id = 24, .port_name = "SPICC0"        },
	{ .port_id = 25, .port_name = "SPICC1"        },
	{ .port_id = 26, .port_name = "SPICC2"        },
	{ .port_id = 28, .port_name = "AUDMA"         },
	{ .port_id = 29, .port_name = "ETHERNET"      },
	{ .port_id = 33, .port_name = "AUDIO"         },
	{ .port_id = 35, .port_name = "SDEMMCA"       },
	{ .port_id = 36, .port_name = "SDEMMCB"       },
	{ .port_id = 37, .port_name = "SDEMMCC"       },
	{ .port_id = 38, .port_name = "DMA"           },
	{ .port_id = 54, .port_name = "DEMUX"         },
	{ .port_id = 56, .port_name = "USB3_A"        },
	{ .port_id = 57, .port_name = "USB3_B"        },
	{ .port_id = 58, .port_name = "USB2"          },
	{ .port_id = 59, .port_name = "PCIE0"         },
	{ .port_id = 60, .port_name = "PCIE1"         },
	/* NIC_VPU */
	{ .port_id = 64, .port_name = "VPU0"          },
	{ .port_id = 66, .port_name = "VPU1"          },
	{ .port_id = 68, .port_name = "VPU2"          },
	{ .port_id = 70, .port_name = "VPU3"          },
	/* NIC_DOS*/
	{ .port_id = 72, .port_name = "HEVC0_F"       },
	{ .port_id = 80, .port_name = "HEVC1_B0"      },
	{ .port_id = 81, .port_name = "HEVC2_B1"      },
	{ .port_id = 88, .port_name = "VDEC"          },
	{ .port_id = 96, .port_name = "HCODEC"        },
	/* NIC_VGE*/
	{ .port_id = 104, .port_name = "GE2D"          },
	{ .port_id = 106, .port_name = "VIDEO_CMPR"    },
	{ .port_id = 108, .port_name = "VC9000E"       }
};

static struct ddr_port_desc ddr_port_desc_a4_prot[] __initdata = {
	{ .port_id = 2, .port_name = "ARM_A53"        },
	{ .port_id = 19, .port_name = "VOUT"          },
	{ .port_id = 40, .port_name = "AOCPU"         },
	{ .port_id = 41, .port_name = "AUDIO_VAD"     },
	{ .port_id = 71, .port_name = "JTAG"          },
	{ .port_id = 72, .port_name = "SPICC0"        },
	{ .port_id = 73, .port_name = "SPICC1"        },
	{ .port_id = 74, .port_name = "ETH"           },
	{ .port_id = 80, .port_name = "EMMC"          },
	{ .port_id = 81, .port_name = "SDIO_A"        },
	{ .port_id = 82, .port_name = "SECU_TOP"      },
	{ .port_id = 83, .port_name = "USB_X2H"       },
	{ .port_id = 84, .port_name = "USB2DRD"	      },
	{ .port_id = 85, .port_name = "AUDIO"         }
};

static struct ddr_port_desc ddr_port_desc_a4_mon[] __initdata = {
	{ .port_id = 0, .port_name = "ARM_A53"        },
	{ .port_id = 2, .port_name = "VOUT"           },
	{ .port_id = 4, .port_name = "DSP"            },
	{ .port_id = 6, .port_name = "DEVICE1"        },
	{ .port_id = 7, .port_name = "DEVICE0"        },
	/* start of each device0 */
	{ .port_id = 32, .port_name = "SPICC1"        },
	{ .port_id = 33, .port_name = "ETH"           },
	{ .port_id = 34, .port_name = "SPICC0"        },
	/* start of each device1 */
	{ .port_id = 40, .port_name = "SDIO_A"        },
	{ .port_id = 41, .port_name = "SEC_TOP"       },
	{ .port_id = 42, .port_name = "EMMC"          },
	{ .port_id = 43, .port_name = "USB"           },
	{ .port_id = 44, .port_name = "AUDIO"         },
	{ .port_id = 45, .port_name = "USB_X2H"       },
	/* start of each dsp */
	{ .port_id = 48, .port_name = "AUDIO_VAD"     },
	{ .port_id = 49, .port_name = "AOCPU"         }
};

static struct ddr_port_desc *chip_ddr_port;
static unsigned int chip_ddr_port_num __initdata;

int __init ddr_find_port_desc(int cpu_type, struct ddr_port_desc **desc)
{
	return ddr_find_port_desc_type(cpu_type, desc, 0);
}

/* type 0:dmc_monitor 1:ddr_bandwidth */
int __init ddr_find_port_desc_type(int cpu_type, struct ddr_port_desc **desc, int type)
{
	int desc_size = -EINVAL;

	switch (cpu_type) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	case DMC_TYPE_M8B:
		*desc = ddr_port_desc_m8b;
		desc_size = ARRAY_SIZE(ddr_port_desc_m8b);
		break;

	case DMC_TYPE_GXBB:
		*desc = ddr_port_desc_gxbb;
		desc_size = ARRAY_SIZE(ddr_port_desc_gxbb);
		break;

	case DMC_TYPE_GXL:
		*desc = ddr_port_desc_gxl;
		desc_size = ARRAY_SIZE(ddr_port_desc_gxl);
		break;

	case DMC_TYPE_GXM:
		*desc = ddr_port_desc_gxm;
		desc_size = ARRAY_SIZE(ddr_port_desc_gxm);
		break;

	case DMC_TYPE_TXL:
		*desc = ddr_port_desc_txl;
		desc_size = ARRAY_SIZE(ddr_port_desc_txl);
		break;

	case DMC_TYPE_TXLX:
		*desc = ddr_port_desc_txlx;
		desc_size = ARRAY_SIZE(ddr_port_desc_txlx);
		break;

	case DMC_TYPE_AXG:
		*desc = ddr_port_desc_axg;
		desc_size = ARRAY_SIZE(ddr_port_desc_axg);
		break;

	case DMC_TYPE_GXLX:
		*desc = ddr_port_desc_gxlx;
		desc_size = ARRAY_SIZE(ddr_port_desc_gxlx);
		break;

	case DMC_TYPE_TXHD:
		*desc = ddr_port_desc_txhd;
		desc_size = ARRAY_SIZE(ddr_port_desc_txhd);
		break;

	case DMC_TYPE_A1:
		*desc = ddr_port_desc_a1;
		desc_size = ARRAY_SIZE(ddr_port_desc_a1);
		break;

	case DMC_TYPE_C1:
		*desc = ddr_port_desc_c1;
		desc_size = ARRAY_SIZE(ddr_port_desc_c1);
		break;
#endif

	case DMC_TYPE_G12A:
		*desc = ddr_port_desc_g12a;
		desc_size = ARRAY_SIZE(ddr_port_desc_g12a);
		break;

	case DMC_TYPE_G12B:
		*desc = ddr_port_desc_g12b;
		desc_size = ARRAY_SIZE(ddr_port_desc_g12b);
		break;

	case DMC_TYPE_SM1:
		*desc = ddr_port_desc_sm1;
		desc_size = ARRAY_SIZE(ddr_port_desc_sm1);
		break;

	case DMC_TYPE_TM2:
		*desc = ddr_port_desc_tm2;
		desc_size = ARRAY_SIZE(ddr_port_desc_tm2);
		break;

	case DMC_TYPE_T5:
	case DMC_TYPE_T5D:
		*desc = ddr_port_desc_t5;
		desc_size = ARRAY_SIZE(ddr_port_desc_t5);
		break;

	case DMC_TYPE_T7:
		*desc = ddr_port_desc_t7;
		desc_size = ARRAY_SIZE(ddr_port_desc_t7);
		break;

	case DMC_TYPE_T3:
		*desc = ddr_port_desc_t3;
		desc_size = ARRAY_SIZE(ddr_port_desc_t3);
		break;

	case DMC_TYPE_S4:
		*desc = ddr_port_desc_s4;
		desc_size = ARRAY_SIZE(ddr_port_desc_s4);
		break;

	case DMC_TYPE_SC2:
		*desc = ddr_port_desc_sc2;
		desc_size = ARRAY_SIZE(ddr_port_desc_sc2);
		break;

	case DMC_TYPE_P1:
		*desc = ddr_port_desc_p1;
		desc_size = ARRAY_SIZE(ddr_port_desc_p1);
		break;

	case DMC_TYPE_T5W:
		*desc = ddr_port_desc_t5w;
		desc_size = ARRAY_SIZE(ddr_port_desc_t5w);
		break;

	case DMC_TYPE_A5:
		*desc = ddr_port_desc_a5;
		desc_size = ARRAY_SIZE(ddr_port_desc_a5);
		break;

	case DMC_TYPE_S5:
		*desc = ddr_port_desc_s5;
		desc_size = ARRAY_SIZE(ddr_port_desc_s5);
		break;

	case DMC_TYPE_A4:
		if (type) {
			*desc = ddr_port_desc_a4_mon;
			desc_size = ARRAY_SIZE(ddr_port_desc_a4_mon);
		} else {
			*desc = ddr_port_desc_a4_prot;
			desc_size = ARRAY_SIZE(ddr_port_desc_a4_prot);
		}
		break;

	case DMC_TYPE_TL1:
		*desc = ddr_port_desc_tl1;
		desc_size = ARRAY_SIZE(ddr_port_desc_tl1);
		break;

	default:
		return -EINVAL;
	}

	/* using once */
	chip_ddr_port = kcalloc(desc_size, sizeof(*chip_ddr_port), GFP_KERNEL);
	if (!chip_ddr_port)
		return -ENOMEM;
	memcpy(chip_ddr_port, *desc, sizeof(struct ddr_port_desc) * desc_size);
	chip_ddr_port_num = desc_size;
	*desc = chip_ddr_port;

	return desc_size;
}

unsigned long dmc_rw(unsigned long addr, unsigned long value, int rw)
{
	struct arm_smccc_res smccc;

	arm_smccc_smc(DMC_MON_RW, addr, value, rw, 0, 0, 0, 0, &smccc);
	return smccc.a0;
}
