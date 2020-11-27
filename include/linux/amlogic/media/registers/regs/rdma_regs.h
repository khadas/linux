/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef RDMA_REGS_HEADER_
#define RDMA_REGS_HEADER_

/* cbus reset ctrl */
#define RESETCTRL_RESET4  0x0004

#ifndef RDMA_AHB_START_ADDR_MAN
#define RDMA_AHB_START_ADDR_MAN     0x1100
#define RDMA_AHB_END_ADDR_MAN       0x1101
#define RDMA_AHB_START_ADDR_1       0x1102
#define RDMA_AHB_END_ADDR_1         0x1103
#define RDMA_AHB_START_ADDR_2       0x1104
#define RDMA_AHB_END_ADDR_2         0x1105
#define RDMA_AHB_START_ADDR_3       0x1106
#define RDMA_AHB_END_ADDR_3         0x1107

#define RDMA_AHB_START_ADDR_4       0x1108
#define RDMA_AHB_END_ADDR_4         0x1109
#define RDMA_AHB_START_ADDR_5       0x110a
#define RDMA_AHB_END_ADDR_5         0x110b
#define RDMA_AHB_START_ADDR_6       0x110c
#define RDMA_AHB_END_ADDR_6         0x110d
#define RDMA_AHB_START_ADDR_7       0x110e
#define RDMA_AHB_END_ADDR_7         0x110f

#define RDMA_ACCESS_AUTO            0x1110
#define RDMA_ACCESS_AUTO2           0x1111
#define RDMA_ACCESS_AUTO3           0x1112
#define RDMA_ACCESS_MAN             0x1113
#define RDMA_CTRL                   0x1114
#define RDMA_STATUS                 0x1115
#define RDMA_STATUS2                0x1116
#define RDMA_STATUS3                0x1117

#define RDMA_AUTO_SRC1_SEL          0x1123
#define RDMA_AUTO_SRC2_SEL          0x1124
#define RDMA_AUTO_SRC3_SEL          0x1125
#define RDMA_AUTO_SRC4_SEL          0x1126
#define RDMA_AUTO_SRC5_SEL          0x1127
#define RDMA_AUTO_SRC6_SEL          0x1128
#define RDMA_AUTO_SRC7_SEL          0x1129

#define RDMA_AHB_START_ADDR_MAN_MSB                0x1130
#define RDMA_AHB_END_ADDR_MAN_MSB                  0x1131
#define RDMA_AHB_START_ADDR_1_MSB                  0x1132
#define RDMA_AHB_END_ADDR_1_MSB                    0x1133
#define RDMA_AHB_START_ADDR_2_MSB                  0x1134
#define RDMA_AHB_END_ADDR_2_MSB                    0x1135
#define RDMA_AHB_START_ADDR_3_MSB                  0x1136
#define RDMA_AHB_END_ADDR_3_MSB                    0x1137
#define RDMA_AHB_START_ADDR_4_MSB                  0x1138
#define RDMA_AHB_END_ADDR_4_MSB                    0x1139
#define RDMA_AHB_START_ADDR_5_MSB                  0x113a
#define RDMA_AHB_END_ADDR_5_MSB                    0x113b
#define RDMA_AHB_START_ADDR_6_MSB                  0x113c
#define RDMA_AHB_END_ADDR_6_MSB                    0x113d
#define RDMA_AHB_START_ADDR_7_MSB                  0x113e
#define RDMA_AHB_END_ADDR_7_MSB                    0x113f
#endif

#endif

