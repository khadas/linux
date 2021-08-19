/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef DI_REGS_HEADER_
#define DI_REGS_HEADER_

#define DI_POST_CTRL                    0x1701
#define DI_POST_SIZE                    0x1702
#define DI_IF1_GEN_REG                  0x17e8
#define VD1_IF0_GEN_REG2                0x1a6d
#define VD2_IF0_GEN_REG2                0x1a8d
#define VD1_IF0_GEN_REG3                0x1aa7
#define DI_IF1_GEN_REG3                 0x20a7
#define DI_IF2_GEN_REG3                 0x2022
#define DI_IF0_GEN_REG3                 0x2042
/* DI HDR */
#define DI_HDR_IN_HSIZE                 0x376e
#define DI_HDR_IN_VSIZE                 0x376f
#ifdef HIS_CODE
#define DI_HDR2_CTRL                    0x3800
#define DI_HDR2_CLK_GATE                0x3881
#define DI_HDR2_MATRIXI_COEF00_01       0x3882
#define DI_HDR2_MATRIXI_COEF02_10       0x3883
#define DI_HDR2_MATRIXI_COEF11_12       0x3884
#define DI_HDR2_MATRIXI_COEF20_21       0x3885
#define DI_HDR2_MATRIXI_COEF22          0x3886
#define DI_HDR2_MATRIXI_COEF30_31       0x3887
#define DI_HDR2_MATRIXI_COEF32_40       0x3888
#define DI_HDR2_MATRIXI_COEF41_42       0x3889
#define DI_HDR2_MATRIXI_OFFSET0_1       0x388A
#define DI_HDR2_MATRIXI_OFFSET2         0x388B
#define DI_HDR2_MATRIXI_PRE_OFFSET0_1   0x388C
#define DI_HDR2_MATRIXI_PRE_OFFSET2     0x388D
#define DI_HDR2_MATRIXO_COEF00_01       0x388E
#define DI_HDR2_MATRIXO_COEF02_10       0x388F
#define DI_HDR2_MATRIXO_COEF11_12       0x3890
#define DI_HDR2_MATRIXO_COEF20_21       0x3891
#define DI_HDR2_MATRIXO_COEF22          0x3892
#define DI_HDR2_MATRIXO_COEF30_31       0x3893
#define DI_HDR2_MATRIXO_COEF32_40       0x3894
#define DI_HDR2_MATRIXO_COEF41_42       0x3895
#define DI_HDR2_MATRIXO_OFFSET0_1       0x3896
#define DI_HDR2_MATRIXO_OFFSET2         0x3897
#define DI_HDR2_MATRIXO_PRE_OFFSET0_1   0x3898
#define DI_HDR2_MATRIXO_PRE_OFFSET2     0x3899
#define DI_HDR2_MATRIXI_CLIP            0x389A
#define DI_HDR2_MATRIXO_CLIP            0x389B
#define DI_HDR2_CGAIN_OFFT              0x389C
#define DI_EOTF_LUT_ADDR_PORT           0x389E
#define DI_EOTF_LUT_DATA_PORT           0x389F
#define DI_OETF_LUT_ADDR_PORT           0x38A0
#define DI_OETF_LUT_DATA_PORT           0x38A1
#define DI_CGAIN_LUT_ADDR_PORT          0x38A2
#define DI_CGAIN_LUT_DATA_PORT          0x38A3
#define DI_HDR2_CGAIN_COEF0             0x38A4
#define DI_HDR2_CGAIN_COEF1             0x38A5
#define DI_OGAIN_LUT_ADDR_PORT          0x38A6
#define DI_OGAIN_LUT_DATA_PORT          0x38A7
#define DI_HDR2_ADPS_CTRL               0x38A8
#define DI_HDR2_ADPS_ALPHA0             0x38A9
#define DI_HDR2_ADPS_ALPHA1             0x38AA
#define DI_HDR2_ADPS_BETA0              0x38AB
#define DI_HDR2_ADPS_BETA1              0x38AC
#define DI_HDR2_ADPS_BETA2              0x38AD
#define DI_HDR2_ADPS_COEF0              0x38AE
#define DI_HDR2_ADPS_COEF1              0x38AF
#define DI_HDR2_GMUT_CTRL               0x38B0
#define DI_HDR2_GMUT_COEF0              0x38B1
#define DI_HDR2_GMUT_COEF1              0x38B2
#define DI_HDR2_GMUT_COEF2              0x38B3
#define DI_HDR2_GMUT_COEF3              0x38B4
#define DI_HDR2_GMUT_COEF4              0x38B5
#define DI_HDR2_PIPE_CTRL1              0x38B6
#define DI_HDR2_PIPE_CTRL2              0x38B7
#define DI_HDR2_PIPE_CTRL3              0x38B8
#define DI_HDR2_PROC_WIN1               0x38B9
#define DI_HDR2_PROC_WIN2               0x38BA
#define DI_HDR2_MATRIXI_EN_CTRL         0x38BB
#define DI_HDR2_MATRIXO_EN_CTRL         0x38BC
#else
/* cp from vpp_hdr_regs.h for hdr */
#define DI_HDR2_CTRL                      0x3770
#define DI_HDR2_CLK_GATE                  0x3771
#define DI_HDR2_MATRIXI_COEF00_01         0x3772
#define DI_HDR2_MATRIXI_COEF02_10         0x3773
#define DI_HDR2_MATRIXI_COEF11_12         0x3774
#define DI_HDR2_MATRIXI_COEF20_21         0x3775
#define DI_HDR2_MATRIXI_COEF22            0x3776
#define DI_HDR2_MATRIXI_COEF30_31         0x3777
#define DI_HDR2_MATRIXI_COEF32_40         0x3778
#define DI_HDR2_MATRIXI_COEF41_42         0x3779
#define DI_HDR2_MATRIXI_OFFSET0_1         0x377a
#define DI_HDR2_MATRIXI_OFFSET2           0x377b
#define DI_HDR2_MATRIXI_PRE_OFFSET0_1     0x377c
#define DI_HDR2_MATRIXI_PRE_OFFSET2       0x377d
#define DI_HDR2_MATRIXO_COEF00_01         0x377e
#define DI_HDR2_MATRIXO_COEF02_10         0x377f
#define DI_HDR2_MATRIXO_COEF11_12         0x3780
#define DI_HDR2_MATRIXO_COEF20_21         0x3781
#define DI_HDR2_MATRIXO_COEF22            0x3782
#define DI_HDR2_MATRIXO_COEF30_31         0x3783
#define DI_HDR2_MATRIXO_COEF32_40         0x3784
#define DI_HDR2_MATRIXO_COEF41_42         0x3785
#define DI_HDR2_MATRIXO_OFFSET0_1         0x3786
#define DI_HDR2_MATRIXO_OFFSET2           0x3787
#define DI_HDR2_MATRIXO_PRE_OFFSET0_1     0x3788
#define DI_HDR2_MATRIXO_PRE_OFFSET2       0x3789
#define DI_HDR2_MATRIXI_CLIP              0x378a
#define DI_HDR2_MATRIXO_CLIP              0x378b
#define DI_HDR2_CGAIN_OFFT                0x378c
#define DI_EOTF_LUT_ADDR_PORT             0x378e
#define DI_EOTF_LUT_DATA_PORT             0x378f
#define DI_OETF_LUT_ADDR_PORT             0x3790
#define DI_OETF_LUT_DATA_PORT             0x3791
#define DI_CGAIN_LUT_ADDR_PORT            0x3792
#define DI_CGAIN_LUT_DATA_PORT            0x3793
#define DI_HDR2_CGAIN_COEF0               0x3794
#define DI_HDR2_CGAIN_COEF1               0x3795
#define DI_OGAIN_LUT_ADDR_PORT            0x3796
#define DI_OGAIN_LUT_DATA_PORT            0x3797
#define DI_HDR2_ADPS_CTRL                 0x3798
#define DI_HDR2_ADPS_ALPHA0               0x3799
#define DI_HDR2_ADPS_ALPHA1               0x379a
#define DI_HDR2_ADPS_BETA0                0x379b
#define DI_HDR2_ADPS_BETA1                0x379c
#define DI_HDR2_ADPS_BETA2                0x379d
#define DI_HDR2_ADPS_COEF0                0x379e
#define DI_HDR2_ADPS_COEF1                0x379f
#define DI_HDR2_GMUT_CTRL                 0x37a0
#define DI_HDR2_GMUT_COEF0                0x37a1
#define DI_HDR2_GMUT_COEF1                0x37a2
#define DI_HDR2_GMUT_COEF2                0x37a3
#define DI_HDR2_GMUT_COEF3                0x37a4
#define DI_HDR2_GMUT_COEF4                0x37a5
#define DI_HDR2_PIPE_CTRL1                0x37a6
#define DI_HDR2_PIPE_CTRL2                0x37a7
#define DI_HDR2_PIPE_CTRL3                0x37a8
#define DI_HDR2_PROC_WIN1                 0x37a9
#define DI_HDR2_PROC_WIN2                 0x37aa
#define DI_HDR2_MATRIXI_EN_CTRL           0x37ab
#define DI_HDR2_MATRIXO_EN_CTRL           0x37ac
#define DI_HDR2_HIST_CTRL                 0x37ad
#define DI_HDR2_HIST_H_START_END          0x37ae
#define DI_HDR2_HIST_V_START_END          0x37af

#endif
#endif
