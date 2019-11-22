/*
 * include/linux/amlogic/media/registers/regs/parser_regs.h
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

#ifndef PARSER_REGS_HEADER_
#define PARSER_REGS_HEADER_

/*
 * pay attention : the regs range has
 * changed to 0x38xx in txlx, it was
 * converted automatically based on
 * the platform at init.
 * #define PARSER_CONTROL 0x3860
 * parser A
 */
#define PARSER_CONTROL                    0x2960
#define PARSER_FETCH_ADDR                 0x2961
#define PARSER_FETCH_CMD                  0x2962
#define PARSER_FETCH_STOP_ADDR            0x2963
#define PARSER_FETCH_LEVEL                0x2964
#define PARSER_CONFIG                     0x2965
#define PFIFO_WR_PTR                      0x2966
#define PFIFO_RD_PTR                      0x2967
#define PFIFO_DATA                        0x2968
#define PARSER_SEARCH_PATTERN             0x2969
#define PARSER_SEARCH_MASK                0x296a
#define PARSER_INT_ENABLE                 0x296b
#define PARSER_INT_STATUS                 0x296c
#define PARSER_SCR_CTL                    0x296d
#define PARSER_SCR                        0x296e
#define PARSER_PARAMETER                  0x296f
#define PARSER_INSERT_DATA                0x2970
#define VAS_STREAM_ID                     0x2971
#define VIDEO_DTS                         0x2972
#define VIDEO_PTS                         0x2973
#define VIDEO_PTS_DTS_WR_PTR              0x2974
#define AUDIO_PTS                         0x2975
#define AUDIO_PTS_WR_PTR                  0x2976
#define PARSER_ES_CONTROL                 0x2977
#define PFIFO_MONITOR                     0x2978
#define PARSER_VIDEO_START_PTR            0x2980
#define PARSER_VIDEO_END_PTR              0x2981
#define PARSER_VIDEO_WP                   0x2982
#define PARSER_VIDEO_RP                   0x2983
#define PARSER_VIDEO_HOLE                 0x2984
#define PARSER_AUDIO_START_PTR            0x2985
#define PARSER_AUDIO_END_PTR              0x2986
#define PARSER_AUDIO_WP                   0x2987
#define PARSER_AUDIO_RP                   0x2988
#define PARSER_AUDIO_HOLE                 0x2989
#define PARSER_SUB_START_PTR              0x298a
#define PARSER_SUB_END_PTR                0x298b
#define PARSER_SUB_WP                     0x298c
#define PARSER_SUB_RP                     0x298d
#define PARSER_SUB_HOLE                   0x298e
#define PARSER_FETCH_INFO                 0x298f
#define PARSER_STATUS                     0x2990
#define PARSER_AV_WRAP_COUNT              0x2991
#define WRRSP_PARSER                      0x2992
#define PARSER_VIDEO2_START_PTR           0x2993
#define PARSER_VIDEO2_END_PTR             0x2994
#define PARSER_VIDEO2_WP                  0x2995
#define PARSER_VIDEO2_RP                  0x2996
#define PARSER_VIDEO2_HOLE                0x2997
#define PARSER_AV2_WRAP_COUNT             0x2998

/*
 * pay attention : the regs range has
 * changed to 0x34xx in SM1, it was
 * converted automatically based on
 * the platform at init.
 * #define PARSER_B_CONTROL 0x3460
 * parser B
 */

#define PARSER_B_CONTROL                  0x2560
#define PARSER_B_FETCH_ADDR               0x2561
#define PARSER_B_FETCH_CMD                0x2562
#define PARSER_B_FETCH_STOP_ADDR          0x2563
#define PARSER_B_FETCH_LEVEL              0x2564
#define PARSER_B_CONFIG                   0x2565
#define PARSER_B_PFIFO_WR_PTR             0x2566
#define PARSER_B_PFIFO_RD_PTR             0x2567
#define PARSER_B_PFIFO_DATA               0x2568
#define PARSER_B_SEARCH_PATTERN           0x2569
#define PARSER_B_SEARCH_MASK              0x256a
#define PARSER_B_INT_ENABLE               0x256b
#define PARSER_B_INT_STATUS               0x256c
#define PARSER_B_SCR_CTL                  0x256d
#define PARSER_B_SCR                      0x256e
#define PARSER_B_PARAMETER                0x256f
#define PARSER_B_INSERT_DATA              0x2570
#define PARSER_B_VAS_STREAM_ID            0x2571
#define PARSER_B_VIDEO_DTS                0x2572
#define PARSER_B_VIDEO_PTS                0x2573
#define PARSER_B_VIDEO_PTS_DTS_WR_PTR     0x2574
#define PARSER_B_AUDIO_PTS                0x2575
#define PARSER_B_AUDIO_PTS_WR_PTR         0x2576
#define PARSER_B_ES_CONTROL               0x2577
#define PARSER_B_PFIFO_MONITOR            0x2578
#define PARSER_B_VIDEO_START_PTR          0x2580
#define PARSER_B_VIDEO_END_PTR            0x2581
#define PARSER_B_VIDEO_WP                 0x2582
#define PARSER_B_VIDEO_RP                 0x2583
#define PARSER_B_VIDEO_HOLE               0x2584
#define PARSER_B_AUDIO_START_PTR          0x2585
#define PARSER_B_AUDIO_END_PTR            0x2586
#define PARSER_B_AUDIO_WP                 0x2587
#define PARSER_B_AUDIO_RP                 0x2588
#define PARSER_B_AUDIO_HOLE               0x2589
#define PARSER_B_SUB_START_PTR            0x258a
#define PARSER_B_SUB_END_PTR              0x258b
#define PARSER_B_SUB_WP                   0x258c
#define PARSER_B_SUB_RP                   0x258d
#define PARSER_B_SUB_HOLE                 0x258e
#define PARSER_B_FETCH_INFO               0x258f
#define PARSER_B_STATUS                   0x2590
#define PARSER_B_AV_WRAP_COUNT            0x2591
#define PARSER_B_WRRSP_PARSER             0x2592
#define PARSER_B_VIDEO2_START_PTR         0x2593
#define PARSER_B_VIDEO2_END_PTR           0x2594
#define PARSER_B_VIDEO2_WP                0x2595
#define PARSER_B_VIDEO2_RP                0x2596
#define PARSER_B_VIDEO2_HOLE              0x2597
#define PARSER_B_AV2_WRAP_COUNT           0x2598

#endif

