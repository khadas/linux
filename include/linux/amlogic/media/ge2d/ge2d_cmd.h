/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _GE2D_CMD_H_
#define _GE2D_CMD_H_

#define GE2D_GET_CAP                        0x470b
#define GE2D_BLEND_NOALPHA_NOBLOCK          0x470a
#define GE2D_BLEND_NOALPHA                  0x4709
#define	GE2D_STRETCHBLIT_NOALPHA_NOBLOCK    0x4708
#define	GE2D_BLIT_NOALPHA_NOBLOCK           0x4707
#define	GE2D_BLEND_NOBLOCK                  0x4706
#define	GE2D_BLIT_NOBLOCK                   0x4705
#define	GE2D_STRETCHBLIT_NOBLOCK            0x4704
#define	GE2D_FILLRECTANGLE_NOBLOCK          0x4703

#define	GE2D_STRETCHBLIT_NOALPHA            0x4702
#define	GE2D_BLIT_NOALPHA                   0x4701
#define	GE2D_BLEND                          0x4700
#define	GE2D_BLIT                           0x46ff
#define	GE2D_STRETCHBLIT                    0x46fe
#define	GE2D_FILLRECTANGLE                  0x46fd
#define	GE2D_SRCCOLORKEY_OLD                0x46fc
#define	GE2D_SET_COEF                       0x46fb
#define	GE2D_CONFIG_EX_OLD                  0x46fa
#define	GE2D_CONFIG_OLD                     0x46f9
#define	GE2D_ANTIFLICKER_ENABLE             0x46f8

#endif
