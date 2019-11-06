/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2011-2018 ARM or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#include "acamera_command_api.h"
#include "acamera_firmware_settings.h"
// ------------ 3A & iridix
static uint8_t _calibration_evtolux_probability_enable[] = {1};

static uint8_t _calibration_awb_avg_coef[] = {7};

static uint8_t _calibration_iridix_avg_coef[] = {30};

static uint16_t _calibration_ccm_one_gain_threshold[] = {1408};

static uint8_t _calibration_iridix_strength_maximum[] = {255};

static uint16_t _calibration_iridix_min_max_str[] = {0};

static uint32_t _calibration_iridix_ev_lim_full_str[] = {2557570};

static uint32_t _calibration_iridix_ev_lim_no_str[] = {3750000, 3574729}; //3574729

static uint8_t _calibration_ae_correction[] = {128, 128, 128, 128, 128, 108, 98, 88, 78, 78, 78, 55};

static uint32_t _calibration_ae_exposure_correction[] = {6710, 15739, 15778, 23282, 56186, 500325, 632161, 1190074, 1406400, 2382765, 3295034, 5491142}; //500,157778,500325,632161,1406400,6046465 //23282 - Max Lab Exposure

// ------------Noise reduction ----------------------//
static uint16_t _calibration_sinter_strength[][2] = {
    {0 * 256, 35}, //30
    {1 * 256, 43}, //30
    {2 * 256, 53}, //45
    {3 * 256, 65}, //55
    {4 * 256, 65}, //73
    {5 * 256, 70}, //74
    {6 * 256, 78}, //74
    {7 * 256, 82}  //82

    // {0*256 , 110}, //118
    // {1*256 , 113}, //123
    // {2*256 , 125}, //130
    // {3*256 , 133}, //133
    // {4*256 , 134}, //134
    // {5*256 , 133}, //133
    // {6*256 , 135}  //135
};
// ------------Noise reduction ----------------------//
static uint16_t _calibration_sinter_strength_MC_contrast[][2] = {
    {0 * 256, 0}};

static uint16_t _calibration_sinter_strength1[][2] = {

    {0 * 256, 155}, //155
    {1 * 256, 155}, //155
    {2 * 256, 125}, //155
    {3 * 256, 115}, //155
    {4 * 256, 115}, //255 4 int
    {5 * 256, 115}, //255 4 int
    {6 * 256, 85},  //255 4 int
    {7 * 256, 85}}; //255 4 int

static uint16_t _calibration_sinter_thresh1[][2] = {
    {0 * 256, 0},
    {1 * 256, 0},
    {2 * 256, 2},
    {3 * 256, 3},
    {4 * 256, 4},
    {5 * 256, 4},
    {6 * 256, 5}};

static uint16_t _calibration_sinter_thresh4[][2] = {
    {0 * 256, 0},
    {1 * 256, 0},
    {2 * 256, 0},
    {3 * 256, 5},
    {4 * 256, 64},
    {5 * 256, 64},
    {6 * 256, 128}};

static uint16_t _calibration_sinter_intConfig[][2] = {
    {0 * 256, 10},
    {1 * 256, 10},
    {2 * 256, 8},
    {3 * 256, 8},
    {4 * 256, 7},
    {5 * 256, 5},
    {6 * 256, 4}};

static uint8_t _calibration_sinter_radial_lut[] = {0, 0, 0, 0, 0, 0, 1, 3, 4, 6, 7, 9, 10, 12, 13, 15, 16, 18, 19, 21, 22, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24};

static uint16_t _calibration_sinter_radial_params[] = {
    0,        // rm_enable
    1920 / 2, // rm_centre_x
    1080 / 2, // rm_centre_y
    1770      // rm_off_centre_mult: round((2^31)/((rm_centre_x^2)+(rm_centre_y^2)))
};

static uint16_t _calibration_sinter_sad[][2] = {
    {0, 8},
    {1 * 256, 8},
    {2 * 256, 5},
    {3 * 256, 5},
    {4 * 256, 9},
    {5 * 256, 11},
    {6 * 256, 13}};
// ------------ Sharpening and demosaic
static uint16_t _calibration_sharp_alt_d[][2] = {
    {0 * 256, 28},
    {1 * 256, 28},
    {2 * 256, 26},
    {3 * 256, 26},
    {4 * 256, 29},
    {5 * 256, 15},
    {6 * 256, 5},
    {7 * 256, 0}
    //    {0 * 256, 30},
    //    {1 * 256, 20},
    //    {2 * 256, 19},
    //    {3 * 256, 18},
    //    {4 * 256, 18},
    //    {5 * 256, 14},
    //    {6 * 256, 19}
};

static uint16_t _calibration_sharp_alt_ud[][2] = {
    {0 * 256, 40}, //12
    {1 * 256, 25},
    {2 * 256, 10},
    {3 * 256, 8},
    {4 * 256, 8},
    {5 * 256, 2},
    {6 * 256, 0},
    {7 * 256, 0}
    //    {0 * 256, 34},
    //    {1 * 256, 31},
    //    {2 * 256, 27},
    //    {3 * 256, 23},
    //    {4 * 256, 12},
    //    {5 * 256, 9},
    //    {6 * 256, 4}
};


static uint16_t _calibration_sharp_alt_du[][2] = {
    {0 * 256, 70},
    {1 * 256, 70},
    {2 * 256, 65},
    {3 * 256, 55},
    {4 * 256, 29},
    {5 * 256, 17},
    {6 * 256, 5},
    {7 * 256, 0}
    //    {0 * 256, 80},
    //    {1 * 256, 47},
    //    {2 * 256, 45},
    //    {3 * 256, 35},
    //    {4 * 256, 34},
    //    {5 * 256, 15},
    //    {6 * 256, 7}
};

static uint16_t _calibration_sharpen_fr[][2] = {
    {0 * 256, 42},
    {1 * 256, 27},
    {2 * 256, 20},
    {3 * 256, 10},
    {4 * 256, 8},
    {5 * 256, 2},
    {6 * 256, 1},
};

static uint16_t _calibration_demosaic_np_offset[][2] = {
    {0 * 256, 1},
    {1 * 256, 1},
    {2 * 256, 1},
    {3 * 256, 3},
    {4 * 256, 18},
    {5 * 256, 18},
    {6 * 256, 15}};


static uint16_t _calibration_mesh_shading_strength[][2] = {
    {0 * 256, 4096}};

static uint16_t _calibration_saturation_strength[][2] = {
    {0 * 256, 128},
    {1 * 256, 128},
    {2 * 256, 128},
    {3 * 256, 118},
    {4 * 256, 105},
    {5 * 256, 90},
    {6 * 256 - 1, 90},
    {6 * 256, 128},
    {7 * 256, 128},

};

// ----------- Frame stitching motion
static uint16_t _calibration_stitching_lm_np[][2] = {
    {0, 540},
    {3 * 256, 1458},
    {4 * 256, 1458},
    {5 * 256, 3000}};

static uint16_t _calibration_stitching_lm_mov_mult[][2] = {
    {0, 128},
    {2 * 256 - 128, 20},
    {5 * 256, 8},
};
static uint16_t _calibration_stitching_lm_med_noise_intensity_thresh[][2] = {
    {0, 32},
    {6 * 256, 32},
    {8 * 256, 4095},
};
static uint16_t _calibration_stitching_ms_np[][2] = {
    {0, 3680},
    {1 * 256, 3680},
    {2 * 256, 2680}};

static uint16_t _calibration_stitching_ms_mov_mult[][2] = {
    {0, 128},
    {1 * 256, 128},
    {2 * 256, 100}};

static uint16_t _calibration_dp_slope[][2] = {
    {0 * 256, 170},
    {1 * 256, 170},
    {2 * 256, 170},
    {3 * 256, 1800},
    {4 * 256, 1911},
    {5 * 256, 2200},
    {6 * 256, 2400},
};


static uint16_t _calibration_dp_threshold[][2] = {
    {0 * 256, 4095},
    {1 * 256, 312},
    {2 * 256, 302},
    {3 * 256, 110},
    {4 * 256, 95},
    {5 * 256, 85},
    {6 * 256, 70},
};

static uint16_t _calibration_AWB_bg_max_gain[][2] = {
    {0 * 256, 100},
    {1 * 256, 100},
    {7 * 256, 200},
};

static uint32_t _calibration_cmos_control[] = {
    0,   // enable antiflicker
    50,  // antiflicker frequency
    0,   // manual integration time
    0,   // manual sensor analog gain
    0,   // manual sensor digital gain
    0,   // manual isp digital gain
    0,   // manual max integration time
    0,   // max integration time
    150, // max sensor AG
    0,   // max sensor DG
    112, // max isp DG
    255, // max exposure ratio
    0,   // integration time.
    0,   // sensor analog gain. log2 fixed - 5 bits
    0,   // sensor digital gain. log2 fixed - 5 bits
    0,   // isp digital gain. log2 fixed - 5 bits
    0,   // analog_gain_last_priority
    4    // analog_gain_reserve
};


static uint32_t _calibration_status_info[] = {
    0xFFFFFFFF, // sys.total_gain_log2
    0xFFFFFFFF, // sys.expsoure_log2
    0xFFFFFFFF, // awb.mix_light_contrast
    0xFFFFFFFF, // af.cur_lens_pos
    0xFFFFFFFF  // af.cur_focus_value
};

static uint32_t _calibration_iridix8_strength_dk_enh_control[] = {
    20,      // dark_prc
    95,      // bright_prc
    800,     // min_dk: minimum dark enhancement
    2000,    // max_dk: maximum dark enhancement
    8,       // pD_cut_min: minimum intensity cut for dark regions in which dk_enh will be applied
    20,      // pD_cut_max: maximum intensity cut for dark regions in which dk_enh will be applied
    30 << 8, // dark contrast min
    50 << 8, // dark contrast max
    0,       // min_str: iridix strength in percentage //26
    40,      // max_str: iridix strength in percentage: 50 = 1x gain. 100 = 2x gain                //50
    40,      // dark_prc_gain_target: target in histogram (percentage) for dark_prc after iridix is applied
    30 << 8, // contrast_min: clip factor of strength for LDR scenes.
    40 << 8, // contrast_max: clip factor of strength for HDR scenes.
    20,      // max iridix gain
    0        // print debug
};

static uint32_t _calibration_ae_control[] = {
    30,  // AE convergance
    236, // LDR AE target -> this should match the 18% grey of teh output gamma
    0,   // AE tail weight
    0,   // WDR mode only: Max percentage of clipped pixels for long exposure: WDR mode only: 256 = 100% clipped pixels
    0,   // WDR mode only: Time filter for exposure ratio
    100, // control for clipping: bright percentage of pixels that should be below hi_target_prc
    99,  // control for clipping: highlights percentage (hi_target_prc): target for tail of histogram
    1,   // 1:0 enable | disable iridix global gain.
    10,  // AE tolerance
};

static uint16_t _calibration_ae_control_HDR_target[][2] = {
    {0 * 256, 139}, // HDR AE target should not be higher than LDR target
    {1 * 256, 139},
    {2 * 256, 139},
    {3 * 256, 187},
    {4 * 256, 236},
    {5 * 256, 236},
    {6 * 256, 236},
    {7 * 256, 236},
};

static uint8_t _calibration_pf_radial_lut[] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};

static uint16_t _calibration_pf_radial_params[] = {
    1920 / 2, // rm_centre_x
    1080 / 2, // rm_centre_y
    1770      // rm_off_centre_mult: round((2^31)/((540^2)+(960^2)))
};

static uint32_t _calibration_auto_level_control[] = {
    1,  // black_percentage
    99, // white_percentage
    0,  // auto_black_min
    50, // auto_black_max
    75, // auto_white_prc
    15, // avg_coeff
    1   // enable_auto_level
};


static uint16_t _calibration_exposure_ratio_adjustment[][2] = {
    //contrast, adjustment
    {1 * 256, 256},
    {16 * 256, 256},
    {32 * 256, 256},
    {64 * 256, 256}};


static uint16_t _calibration_cnr_uv_delta12_slope[][2] = {
    {0 * 256, 1500}, //3800
    {1 * 256, 2000},
    {2 * 256, 2100},
    {3 * 256, 2100},
    {4 * 256, 3500},
    {5 * 256, 4100},
    {6 * 256, 5900},
    {7 * 256, 6100},
};


static uint16_t _calibration_fs_mc_off[] = {
    8 * 256, // gain_log2 threshold. if gain is higher than the current gain_log2. mc off mode will be enabed.
};
static int16_t _AWB_colour_preference[] = {7500, 6000, 4700, 2800};

static uint32_t _calibration_awb_mix_light_parameters[] = {
    1,    // 1 = enable, 0 = disable
    500,  //lux low boundary for mix light lux range : range = {500: inf}
    3000, // lux high boundary for mix light range : range = {500: inf}
    2000, // contrast threshold for mix light: range = {200:2000}
    330,  //BG threshold {255:400}
    5,    // BG weight
    260,  // rgHigh_LUT_max
    252,  // rgHigh_LUT_min
    0     // print debug
};

static uint16_t _calibration_rgb2yuv_conversion[] = {76, 150, 29, 0x8025, 0x8049, 111, 157, 0x8083, 0x8019, 0, 512, 512};


static uint16_t _calibration_ae_zone_wght_hor[] = {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16};
static uint16_t _calibration_ae_zone_wght_ver[] = {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16};

static uint16_t _calibration_awb_zone_wght_hor[] = {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16};
static uint16_t _calibration_awb_zone_wght_ver[] = {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16};

static uint32_t _scaler_h_filter[] = {
    0x27f70200, 0x0002f727, 0x29f70200, 0x0002f824, 0x2cf70200, 0x0002f821, 0x2ef70200, 0x0002f91e, 0x30f70200, 0x0002f91c, 0x33f70200, 0x0001fa19, 0x35f70200, 0x0001fa17, 0x37f70200, 0x0001fb14, 0x39f70200, 0x0001fc11, 0x3af80200, 0x0001fc0f, 0x3bf90200, 0x0001fd0c, 0x3cfa0200, 0x0001fd0a, 0x3efb0100, 0x0000fe08, 0x3efc0100, 0x0000ff06, 0x3ffd0100, 0x0000ff04, 0x40fe0000, 0x00000002, 0x40000000, 0x00000000, 0x40020000, 0x000000fe, 0x3f04ff00, 0x000001fd, 0x3e06ff00, 0x000001fc, 0x3e08fe00, 0x000001fb, 0x3c0afd01, 0x000002fa, 0x3b0cfd01, 0x000002f9, 0x3a0ffc01, 0x000002f8, 0x3911fc01, 0x000002f7, 0x3714fb01, 0x000002f7, 0x3517fa01, 0x000002f7, 0x3319fa01, 0x000002f7, 0x301cf902, 0x000002f7, 0x2e1ef902, 0x000002f7, 0x2c21f802, 0x000002f7, 0x2924f802, 0x000002f7,
    0x25fbfd05, 0x00fdfb26, 0x27fcfc05, 0x00fefa24, 0x28fdfc05, 0x00fef923, 0x29fefb05, 0x00fff921, 0x29fffb06, 0x00fff820, 0x2a00fa06, 0x0000f81e, 0x2b01fa06, 0x0000f71d, 0x2b02f906, 0x0001f71c, 0x2c03f906, 0x0001f71a, 0x2d04f806, 0x0002f619, 0x2d06f806, 0x0002f617, 0x2d07f806, 0x0003f615, 0x2f08f705, 0x0003f614, 0x2f09f705, 0x0004f612, 0x2f0bf705, 0x0004f610, 0x2f0df605, 0x0004f60f, 0x2f0df605, 0x0005f60e, 0x2f0ff604, 0x0005f60d, 0x2f10f604, 0x0005f70b, 0x2f12f604, 0x0005f709, 0x2f14f603, 0x0005f708, 0x2d15f603, 0x0006f807, 0x2d17f602, 0x0006f806, 0x2d19f602, 0x0006f804, 0x2c1af701, 0x0006f903, 0x2b1cf701, 0x0006f902, 0x2b1df700, 0x0006fa01, 0x2a1ef800, 0x0006fa00, 0x2920f8ff, 0x0006fbff, 0x2921f9ff, 0x0005fbfe, 0x2823f9fe, 0x0005fcfd, 0x2724fafe, 0x0005fcfc,
    0x1e0afafc, 0x00fa0a1e, 0x1e0bfafc, 0x00fa091e, 0x1e0cfbfb, 0x00fa091d, 0x1f0cfbfb, 0x00fa081d, 0x200dfbfb, 0x00f9071d, 0x200efbfb, 0x00f9071c, 0x200efcfb, 0x00f9061c, 0x210ffcfa, 0x00f9051c, 0x2110fcfa, 0x00f9051b, 0x2111fdfa, 0x00f9041a, 0x2211fdfa, 0x00f9031a, 0x2212fefa, 0x00f90318, 0x2213fefa, 0x00f90218, 0x2213fff9, 0x00f90218, 0x2215fff9, 0x00f90117, 0x2215fff9, 0x00f90117, 0x221600f9, 0x00f90016, 0x221701f9, 0x00f9ff15, 0x221701f9, 0x00f9ff15, 0x221802f9, 0x00f9ff13, 0x221802f9, 0x00fafe13, 0x221803f9, 0x00fafe12, 0x221a03f9, 0x00fafd11, 0x211a04f9, 0x00fafd11, 0x211b05f9, 0x00fafc10, 0x211c05f9, 0x00fafc0f, 0x201c06f9, 0x00fbfc0e, 0x201c07f9, 0x00fbfb0e, 0x201d07f9, 0x00fbfb0d, 0x1f1d08fa, 0x00fbfb0c, 0x1e1d09fa, 0x00fbfb0c, 0x1e1e09fa, 0x00fcfa0b,
    0x0e0b0602, 0x00060b0e, 0x0e0b0702, 0x00060b0d, 0x0e0b0702, 0x00060b0d, 0x0e0b0702, 0x00060b0d, 0x0e0c0702, 0x00060a0d, 0x0e0c0702, 0x00060a0d, 0x0e0c0703, 0x00050a0d, 0x0e0c0703, 0x00050a0d, 0x0e0c0703, 0x00050a0d, 0x0e0c0803, 0x0005090d, 0x0e0c0803, 0x0005090d, 0x0e0c0803, 0x0005090d, 0x0e0c0803, 0x0005090d, 0x0e0c0804, 0x0004090d, 0x0e0c0804, 0x0004090d, 0x0e0c0904, 0x0004090c, 0x0e0c0904, 0x0004090c, 0x0e0c0904, 0x0004090c, 0x0e0d0904, 0x0004080c, 0x0e0d0904, 0x0004080c, 0x0e0d0905, 0x0003080c, 0x0e0d0905, 0x0003080c, 0x0e0d0905, 0x0003080c, 0x0e0d0905, 0x0003080c, 0x0e0d0a05, 0x0003070c, 0x0e0d0a05, 0x0003070c, 0x0e0d0a05, 0x0003070c, 0x0e0d0a06, 0x0002070c, 0x0e0d0a06, 0x0002070c, 0x0e0d0b06, 0x0002070b, 0x0e0d0b06, 0x0002070b, 0x0e0d0b06, 0x0002070b};

static uint32_t _scaler_v_filter[] = {
    0x00400000, 0x00000000, 0x0240fe00, 0x00000000, 0x0340fd01, 0x000000ff, 0x053ffc01, 0x000000ff, 0x073ffb01, 0x000000fe, 0x093efa01, 0x000000fe, 0x0c3cf901, 0x000000fe, 0x0e3bf901, 0x000000fd, 0x1138f801, 0x000001fd, 0x1337f801, 0x000001fc, 0x1635f801, 0x000001fb, 0x1932f801, 0x000001fb, 0x1b31f801, 0x000001fa, 0x1e2ef801, 0x000001fa, 0x212cf801, 0x000001f9, 0x2429f801, 0x000001f9, 0x2626f901, 0x000001f9, 0x2924f901, 0x000001f8, 0x2c21f901, 0x000001f8, 0x2e1efa01, 0x000001f8, 0x311bfa01, 0x000001f8, 0x3219fb01, 0x000001f8, 0x3516fb01, 0x000001f8, 0x3713fc01, 0x000001f8, 0x3811fd01, 0x000001f8, 0x3b0efd00, 0x000001f9, 0x3c0cfe00, 0x000001f9, 0x3e09fe00, 0x000001fa, 0x3f07fe00, 0x000001fb, 0x3f05ff00, 0x000001fc, 0x4003ff00, 0x000001fd, 0x40020000, 0x000000fe,
    0x2526fbfd, 0x0005fdfb, 0x2724fafe, 0x0005fcfc, 0x2823f9fe, 0x0005fcfd, 0x2921f9ff, 0x0005fbfe, 0x2920f8ff, 0x0006fbff, 0x2a1ef800, 0x0006fa00, 0x2b1df700, 0x0006fa01, 0x2b1cf701, 0x0006f902, 0x2c1af701, 0x0006f903, 0x2d19f602, 0x0006f804, 0x2d17f602, 0x0006f806, 0x2d15f603, 0x0006f807, 0x2f14f603, 0x0005f708, 0x2f12f604, 0x0005f709, 0x2f10f604, 0x0005f70b, 0x2f0ff604, 0x0005f60d, 0x2f0ef605, 0x0005f60d, 0x2f0df605, 0x0004f60f, 0x2f0bf705, 0x0004f610, 0x2f09f705, 0x0004f612, 0x2f08f705, 0x0003f614, 0x2d07f806, 0x0003f615, 0x2d06f806, 0x0002f617, 0x2d04f806, 0x0002f619, 0x2c03f906, 0x0001f71a, 0x2b02f906, 0x0001f71c, 0x2b01fa06, 0x0000f71d, 0x2a00fa06, 0x0000f81e, 0x29fffb06, 0x00fff820, 0x29fefb05, 0x00fff921, 0x28fdfc05, 0x00fef923, 0x27fcfc05, 0x00fefa24,
    0x1e1e0afa, 0x00fcfa0a, 0x1e1e09fa, 0x00fcfa0b, 0x1e1d09fa, 0x00fbfb0c, 0x1f1d08fa, 0x00fbfb0c, 0x201d07f9, 0x00fbfb0d, 0x201c07f9, 0x00fbfb0e, 0x201c06f9, 0x00fbfc0e, 0x211c05f9, 0x00fafc0f, 0x211b05f9, 0x00fafc10, 0x211a04f9, 0x00fafd11, 0x221a03f9, 0x00fafd11, 0x221803f9, 0x00fafe12, 0x221802f9, 0x00fafe13, 0x221802f9, 0x00f9ff13, 0x221701f9, 0x00f9ff15, 0x221701f9, 0x00f9ff15, 0x221600f9, 0x00f90016, 0x2215fff9, 0x00f90117, 0x2215fff9, 0x00f90117, 0x2213fff9, 0x00f90218, 0x2213fefa, 0x00f90218, 0x2212fefa, 0x00f90318, 0x2211fdfa, 0x00f9031a, 0x2111fdfa, 0x00f9041a, 0x2110fcfa, 0x00f9051b, 0x210ffcfa, 0x00f9051c, 0x200efcfb, 0x00f9061c, 0x200efbfb, 0x00f9071c, 0x200dfbfb, 0x00f9071d, 0x1f0cfbfb, 0x00fa081d, 0x1e0cfbfb, 0x00fa091d, 0x1e0bfafc, 0x00fa091e,
    0x0e0e0b06, 0x0002060b, 0x0e0d0b06, 0x0002070b, 0x0e0d0b06, 0x0002070b, 0x0e0d0b06, 0x0002070b, 0x0e0d0a06, 0x0002070c, 0x0e0d0a06, 0x0002070c, 0x0e0d0a05, 0x0003070c, 0x0e0d0a05, 0x0003070c, 0x0e0d0a05, 0x0003070c, 0x0e0d0905, 0x0003080c, 0x0e0d0905, 0x0003080c, 0x0e0d0905, 0x0003080c, 0x0e0d0905, 0x0003080c, 0x0e0d0904, 0x0004080c, 0x0e0d0904, 0x0004080c, 0x0e0c0904, 0x0004090c, 0x0e0c0904, 0x0004090c, 0x0e0c0904, 0x0004090c, 0x0e0c0804, 0x0004090d, 0x0e0c0804, 0x0004090d, 0x0e0c0803, 0x0005090d, 0x0e0c0803, 0x0005090d, 0x0e0c0803, 0x0005090d, 0x0e0c0803, 0x0005090d, 0x0e0c0703, 0x00050a0d, 0x0e0c0703, 0x00050a0d, 0x0e0c0703, 0x00050a0d, 0x0e0c0702, 0x00060a0d, 0x0e0c0702, 0x00060a0d, 0x0e0b0702, 0x00060b0d, 0x0e0b0702, 0x00060b0d, 0x0e0b0702, 0x00060b0d};

static uint16_t _calibration_sharpen_ds1[][2] = {
    {0 * 256, 70},
    {1 * 256, 70},
    {2 * 256, 70},
    {3 * 256, 70},
    {4 * 256, 70},
    {5 * 256, 50},
    {6 * 256, 40},
    {7 * 256, 25},
    {8 * 256, 10}};
static uint16_t _calibration_temper_strength[][2] = {
    {0 * 256, 120},
    {1 * 256, 120},
    {2 * 256, 120},
    {3 * 256, 120},
    {4 * 256, 140},
    {5 * 256, 145},
    {6 * 256, 195},
    {7 * 256, 195},
};

static uint32_t _calibration_af_lms[] = {
    70 << 6,  // Down_FarEnd
    70 << 6,  // Hor_FarEnd
    70 << 6,  // Up_FarEnd
    112 << 6, // Down_Infinity
    112 << 6, // Hor_Infinity
    112 << 6, // Up_Infinity
    // 50<<6, // Down_FarEnd
    // 50<<6, // Hor_FarEnd
    // 50<<6, // Up_FarEnd
    // 167<<6, // Down_Infinity
    // 167<<6, // Hor_Infinity
    // 167<<6, // Up_Infinity
    832 << 6,                      // Down_Macro
    832 << 6,                      // Hor_Macro
    832 << 6,                      // Up_Macro
    915 << 6,                      // Down_NearEnd
    915 << 6,                      // Hor_NearEnd
    915 << 6,                      // Up_NearEnd
    11,                            // step_num
    6,                             // skip_frames_init
    2,                             // skip_frames_move
    30,                            // dynamic_range_th
    2 << ( LOG2_GAIN_SHIFT - 2 ),  // spot_tolerance
    1 << ( LOG2_GAIN_SHIFT - 1 ),  // exit_th
    16 << ( LOG2_GAIN_SHIFT - 4 ), // caf_trigger_th
    4 << ( LOG2_GAIN_SHIFT - 4 ),  // caf_stable_th
    0,                             // print_debug
};

static uint16_t _calibration_af_zone_wght_hor[] = {0, 0, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0};
static uint16_t _calibration_af_zone_wght_ver[] = {0, 0, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0};

static LookupTable calibration_fs_mc_off = {.ptr = _calibration_fs_mc_off, .rows = 1, .cols = sizeof( _calibration_fs_mc_off ) / sizeof( _calibration_fs_mc_off[0] ), .width = sizeof( _calibration_fs_mc_off[0] )};
static LookupTable calibration_exposure_ratio_adjustment = {.ptr = _calibration_exposure_ratio_adjustment, .rows = sizeof( _calibration_exposure_ratio_adjustment ) / sizeof( _calibration_exposure_ratio_adjustment[0] ), .cols = 2, .width = sizeof( _calibration_exposure_ratio_adjustment[0][0] )};
static LookupTable AWB_colour_preference = {.ptr = _AWB_colour_preference, .rows = 1, .cols = sizeof( _AWB_colour_preference ) / sizeof( _AWB_colour_preference[0] ), .width = sizeof( _AWB_colour_preference[0] )};
static LookupTable calibration_awb_mix_light_parameters = {.ptr = _calibration_awb_mix_light_parameters, .rows = 1, .cols = sizeof( _calibration_awb_mix_light_parameters ) / sizeof( _calibration_awb_mix_light_parameters[0] ), .width = sizeof( _calibration_awb_mix_light_parameters[0] )};
static LookupTable calibration_sinter_strength_MC_contrast = {.ptr = _calibration_sinter_strength_MC_contrast, .rows = sizeof( _calibration_sinter_strength_MC_contrast ) / sizeof( _calibration_sinter_strength_MC_contrast[0] ), .cols = 2, .width = sizeof( _calibration_sinter_strength_MC_contrast[0][0] )};
static LookupTable calibration_pf_radial_lut = {.ptr = _calibration_pf_radial_lut, .rows = 1, .cols = sizeof( _calibration_pf_radial_lut ) / sizeof( _calibration_pf_radial_lut[0] ), .width = sizeof( _calibration_pf_radial_lut[0] )};
static LookupTable calibration_pf_radial_params = {.ptr = _calibration_pf_radial_params, .rows = 1, .cols = sizeof( _calibration_pf_radial_params ) / sizeof( _calibration_pf_radial_params[0] ), .width = sizeof( _calibration_pf_radial_params[0] )};
static LookupTable calibration_sinter_radial_lut = {.ptr = _calibration_sinter_radial_lut, .rows = 1, .cols = sizeof( _calibration_sinter_radial_lut ) / sizeof( _calibration_sinter_radial_lut[0] ), .width = sizeof( _calibration_sinter_radial_lut[0] )};
static LookupTable calibration_sinter_radial_params = {.ptr = _calibration_sinter_radial_params, .rows = 1, .cols = sizeof( _calibration_sinter_radial_params ) / sizeof( _calibration_sinter_radial_params[0] ), .width = sizeof( _calibration_sinter_radial_params[0] )};
static LookupTable calibration_AWB_bg_max_gain = {.ptr = _calibration_AWB_bg_max_gain, .rows = sizeof( _calibration_AWB_bg_max_gain ) / sizeof( _calibration_AWB_bg_max_gain[0] ), .cols = 2, .width = sizeof( _calibration_AWB_bg_max_gain[0][0] )};
static LookupTable calibration_iridix8_strength_dk_enh_control = {.ptr = _calibration_iridix8_strength_dk_enh_control, .rows = 1, .cols = sizeof( _calibration_iridix8_strength_dk_enh_control ) / sizeof( _calibration_iridix8_strength_dk_enh_control[0] ), .width = sizeof( _calibration_iridix8_strength_dk_enh_control[0] )};
static LookupTable calibration_auto_level_control = {.ptr = _calibration_auto_level_control, .rows = 1, .cols = sizeof( _calibration_auto_level_control ) / sizeof( _calibration_auto_level_control[0] ), .width = sizeof( _calibration_auto_level_control[0] )};
static LookupTable calibration_dp_threshold = {.ptr = _calibration_dp_threshold, .rows = sizeof( _calibration_dp_threshold ) / sizeof( _calibration_dp_threshold[0] ), .cols = 2, .width = sizeof( _calibration_dp_threshold[0][0] )};
static LookupTable calibration_stitching_lm_np = {.ptr = _calibration_stitching_lm_np, .rows = sizeof( _calibration_stitching_lm_np ) / sizeof( _calibration_stitching_lm_np[0] ), .cols = 2, .width = sizeof( _calibration_stitching_lm_np[0][0] )};
static LookupTable calibration_stitching_lm_med_noise_intensity_thresh = {.ptr = _calibration_stitching_lm_med_noise_intensity_thresh, .rows = sizeof( _calibration_stitching_lm_med_noise_intensity_thresh ) / sizeof( _calibration_stitching_lm_med_noise_intensity_thresh[0] ), .cols = 2, .width = sizeof( _calibration_stitching_lm_med_noise_intensity_thresh[0][0] )};
static LookupTable calibration_stitching_lm_mov_mult = {.ptr = _calibration_stitching_lm_mov_mult, .rows = sizeof( _calibration_stitching_lm_mov_mult ) / sizeof( _calibration_stitching_lm_mov_mult[0] ), .cols = 2, .width = sizeof( _calibration_stitching_lm_mov_mult[0][0] )};
static LookupTable calibration_stitching_ms_np = {.ptr = _calibration_stitching_ms_np, .rows = sizeof( _calibration_stitching_ms_np ) / sizeof( _calibration_stitching_ms_np[0] ), .cols = 2, .width = sizeof( _calibration_stitching_ms_np[0][0] )};
static LookupTable calibration_stitching_ms_mov_mult = {.ptr = _calibration_stitching_ms_mov_mult, .rows = sizeof( _calibration_stitching_ms_mov_mult ) / sizeof( _calibration_stitching_ms_mov_mult[0] ), .cols = 2, .width = sizeof( _calibration_stitching_ms_mov_mult[0][0] )};
static LookupTable calibration_evtolux_probability_enable = {.ptr = _calibration_evtolux_probability_enable, .rows = 1, .cols = sizeof( _calibration_evtolux_probability_enable ) / sizeof( _calibration_evtolux_probability_enable[0] ), .width = sizeof( _calibration_evtolux_probability_enable[0] )};
static LookupTable calibration_awb_avg_coef = {.ptr = _calibration_awb_avg_coef, .rows = 1, .cols = sizeof( _calibration_awb_avg_coef ) / sizeof( _calibration_awb_avg_coef[0] ), .width = sizeof( _calibration_awb_avg_coef[0] )};
static LookupTable calibration_iridix_avg_coef = {.ptr = _calibration_iridix_avg_coef, .rows = 1, .cols = sizeof( _calibration_iridix_avg_coef ) / sizeof( _calibration_iridix_avg_coef[0] ), .width = sizeof( _calibration_iridix_avg_coef[0] )};
static LookupTable calibration_iridix_strength_maximum = {.ptr = _calibration_iridix_strength_maximum, .rows = 1, .cols = sizeof( _calibration_iridix_strength_maximum ) / sizeof( _calibration_iridix_strength_maximum[0] ), .width = sizeof( _calibration_iridix_strength_maximum[0] )};
static LookupTable calibration_iridix_min_max_str = {.ptr = _calibration_iridix_min_max_str, .rows = 1, .cols = sizeof( _calibration_iridix_min_max_str ) / sizeof( _calibration_iridix_min_max_str[0] ), .width = sizeof( _calibration_iridix_min_max_str[0] )};
static LookupTable calibration_iridix_ev_lim_full_str = {.ptr = _calibration_iridix_ev_lim_full_str, .rows = 1, .cols = sizeof( _calibration_iridix_ev_lim_full_str ) / sizeof( _calibration_iridix_ev_lim_full_str[0] ), .width = sizeof( _calibration_iridix_ev_lim_full_str[0] )};
static LookupTable calibration_iridix_ev_lim_no_str = {.ptr = _calibration_iridix_ev_lim_no_str, .rows = 1, .cols = sizeof( _calibration_iridix_ev_lim_no_str ) / sizeof( _calibration_iridix_ev_lim_no_str[0] ), .width = sizeof( _calibration_iridix_ev_lim_no_str[0] )};
static LookupTable calibration_ae_correction = {.ptr = _calibration_ae_correction, .rows = 1, .cols = sizeof( _calibration_ae_correction ) / sizeof( _calibration_ae_correction[0] ), .width = sizeof( _calibration_ae_correction[0] )};
static LookupTable calibration_ae_exposure_correction = {.ptr = _calibration_ae_exposure_correction, .rows = 1, .cols = sizeof( _calibration_ae_exposure_correction ) / sizeof( _calibration_ae_exposure_correction[0] ), .width = sizeof( _calibration_ae_exposure_correction[0] )};
static LookupTable calibration_sinter_strength = {.ptr = _calibration_sinter_strength, .rows = sizeof( _calibration_sinter_strength ) / sizeof( _calibration_sinter_strength[0] ), .cols = 2, .width = sizeof( _calibration_sinter_strength[0][0] )};
static LookupTable calibration_sinter_strength1 = {.ptr = _calibration_sinter_strength1, .rows = sizeof( _calibration_sinter_strength1 ) / sizeof( _calibration_sinter_strength1[0] ), .cols = 2, .width = sizeof( _calibration_sinter_strength1[0][0] )};
static LookupTable calibration_sinter_thresh1 = {.ptr = _calibration_sinter_thresh1, .rows = sizeof( _calibration_sinter_thresh1 ) / sizeof( _calibration_sinter_thresh1[0] ), .cols = 2, .width = sizeof( _calibration_sinter_thresh1[0][0] )};
static LookupTable calibration_sinter_thresh4 = {.ptr = _calibration_sinter_thresh4, .rows = sizeof( _calibration_sinter_thresh4 ) / sizeof( _calibration_sinter_thresh4[0] ), .cols = 2, .width = sizeof( _calibration_sinter_thresh4[0][0] )};
static LookupTable calibration_sinter_intConfig = {.ptr = _calibration_sinter_intConfig, .rows = sizeof( _calibration_sinter_intConfig ) / sizeof( _calibration_sinter_intConfig[0] ), .cols = 2, .width = sizeof( _calibration_sinter_intConfig[0][0] )};
static LookupTable calibration_sharp_alt_d = {.ptr = _calibration_sharp_alt_d, .rows = sizeof( _calibration_sharp_alt_d ) / sizeof( _calibration_sharp_alt_d[0] ), .cols = 2, .width = sizeof( _calibration_sharp_alt_d[0][0] )};
static LookupTable calibration_sharp_alt_ud = {.ptr = _calibration_sharp_alt_ud, .rows = sizeof( _calibration_sharp_alt_ud ) / sizeof( _calibration_sharp_alt_ud[0] ), .cols = 2, .width = sizeof( _calibration_sharp_alt_ud[0][0] )};
static LookupTable calibration_sharp_alt_du = {.ptr = _calibration_sharp_alt_du, .rows = sizeof( _calibration_sharp_alt_du ) / sizeof( _calibration_sharp_alt_du[0] ), .cols = 2, .width = sizeof( _calibration_sharp_alt_du[0][0] )};
static LookupTable calibration_sharpen_fr = {.ptr = _calibration_sharpen_fr, .rows = sizeof( _calibration_sharpen_fr ) / sizeof( _calibration_sharpen_fr[0] ), .cols = 2, .width = sizeof( _calibration_sharpen_fr[0][0] )};
static LookupTable calibration_demosaic_np_offset = {.ptr = _calibration_demosaic_np_offset, .rows = sizeof( _calibration_demosaic_np_offset ) / sizeof( _calibration_demosaic_np_offset[0] ), .cols = 2, .width = sizeof( _calibration_demosaic_np_offset[0][0] )};
static LookupTable calibration_mesh_shading_strength = {.ptr = _calibration_mesh_shading_strength, .rows = sizeof( _calibration_mesh_shading_strength ) / sizeof( _calibration_mesh_shading_strength[0] ), .cols = 2, .width = sizeof( _calibration_mesh_shading_strength[0][0] )};
static LookupTable calibration_saturation_strength = {.ptr = _calibration_saturation_strength, .rows = sizeof( _calibration_saturation_strength ) / sizeof( _calibration_saturation_strength[0] ), .cols = 2, .width = sizeof( _calibration_saturation_strength[0][0] )};
static LookupTable calibration_ccm_one_gain_threshold = {.ptr = _calibration_ccm_one_gain_threshold, .cols = sizeof( _calibration_ccm_one_gain_threshold ) / sizeof( _calibration_ccm_one_gain_threshold[0] ), .rows = 1, .width = sizeof( _calibration_ccm_one_gain_threshold[0] )};
static LookupTable calibration_cmos_control = {.ptr = _calibration_cmos_control, .rows = 1, .cols = sizeof( _calibration_cmos_control ) / sizeof( _calibration_cmos_control[0] ), .width = sizeof( _calibration_cmos_control[0] )};
static LookupTable calibration_status_info = {.ptr = _calibration_status_info, .rows = 1, .cols = sizeof( _calibration_status_info ) / sizeof( _calibration_status_info[0] ), .width = sizeof( _calibration_status_info[0] )};
static LookupTable calibration_ae_control = {.ptr = _calibration_ae_control, .rows = 1, .cols = sizeof( _calibration_ae_control ) / sizeof( _calibration_ae_control[0] ), .width = sizeof( _calibration_ae_control[0] )};
static LookupTable calibration_ae_control_HDR_target = {.ptr = _calibration_ae_control_HDR_target, .rows = sizeof( _calibration_ae_control_HDR_target ) / sizeof( _calibration_ae_control_HDR_target[0] ), .cols = 2, .width = sizeof( _calibration_ae_control_HDR_target[0][0] )};
static LookupTable calibration_rgb2yuv_conversion = {.ptr = _calibration_rgb2yuv_conversion, .rows = 1, .cols = sizeof( _calibration_rgb2yuv_conversion ) / sizeof( _calibration_rgb2yuv_conversion[0] ), .width = sizeof( _calibration_rgb2yuv_conversion[0] )};
static LookupTable calibration_calibration_af_lms = {.ptr = _calibration_af_lms, .rows = 1, .cols = sizeof( _calibration_af_lms ) / sizeof( _calibration_af_lms[0] ), .width = sizeof( _calibration_af_lms[0] )};
static LookupTable calibration_calibration_af_zone_wght_hor = {.ptr = _calibration_af_zone_wght_hor, .rows = 1, .cols = sizeof( _calibration_af_zone_wght_hor ) / sizeof( _calibration_af_zone_wght_hor[0] ), .width = sizeof( _calibration_af_zone_wght_hor[0] )};
static LookupTable calibration_calibration_af_zone_wght_ver = {.ptr = _calibration_af_zone_wght_ver, .rows = 1, .cols = sizeof( _calibration_af_zone_wght_ver ) / sizeof( _calibration_af_zone_wght_ver[0] ), .width = sizeof( _calibration_af_zone_wght_ver[0] )};
static LookupTable calibration_calibration_ae_zone_wght_hor = {.ptr = _calibration_ae_zone_wght_hor, .rows = 1, .cols = sizeof( _calibration_ae_zone_wght_hor ) / sizeof( _calibration_ae_zone_wght_hor[0] ), .width = sizeof( _calibration_ae_zone_wght_hor[0] )};
static LookupTable calibration_calibration_ae_zone_wght_ver = {.ptr = _calibration_ae_zone_wght_ver, .rows = 1, .cols = sizeof( _calibration_ae_zone_wght_ver ) / sizeof( _calibration_ae_zone_wght_ver[0] ), .width = sizeof( _calibration_ae_zone_wght_ver[0] )};
static LookupTable calibration_calibration_awb_zone_wght_hor = {.ptr = _calibration_awb_zone_wght_hor, .rows = 1, .cols = sizeof( _calibration_awb_zone_wght_hor ) / sizeof( _calibration_awb_zone_wght_hor[0] ), .width = sizeof( _calibration_awb_zone_wght_hor[0] )};
static LookupTable calibration_calibration_awb_zone_wght_ver = {.ptr = _calibration_awb_zone_wght_ver, .rows = 1, .cols = sizeof( _calibration_awb_zone_wght_ver ) / sizeof( _calibration_awb_zone_wght_ver[0] ), .width = sizeof( _calibration_awb_zone_wght_ver[0] )};
static LookupTable calibration_dp_slope = {.ptr = _calibration_dp_slope, .rows = sizeof( _calibration_dp_slope ) / sizeof( _calibration_dp_slope[0] ), .cols = 2, .width = sizeof( _calibration_dp_slope[0][0] )};
static LookupTable calibration_cnr_uv_delta12_slope = {.ptr = _calibration_cnr_uv_delta12_slope, .rows = sizeof( _calibration_cnr_uv_delta12_slope ) / sizeof( _calibration_cnr_uv_delta12_slope[0] ), .cols = 2, .width = sizeof( _calibration_cnr_uv_delta12_slope[0][0] )};
static LookupTable calibration_sinter_sad = {.ptr = _calibration_sinter_sad, .rows = sizeof( _calibration_sinter_sad ) / sizeof( _calibration_sinter_sad[0] ), .cols = 2, .width = sizeof( _calibration_sinter_sad[0][0] )};
static LookupTable calibration_scaler_h_filter = {.ptr = _scaler_h_filter, .rows = 1, .cols = sizeof( _scaler_h_filter ) / sizeof( _scaler_h_filter[0] ), .width = sizeof( _scaler_h_filter[0] )};
static LookupTable calibration_scaler_v_filter = {.ptr = _scaler_v_filter, .rows = 1, .cols = sizeof( _scaler_v_filter ) / sizeof( _scaler_v_filter[0] ), .width = sizeof( _scaler_v_filter[0] )};
static LookupTable calibration_sharpen_ds1 = {.ptr = _calibration_sharpen_ds1, .rows = sizeof( _calibration_sharpen_ds1 ) / sizeof( _calibration_sharpen_ds1[0] ), .cols = 2, .width = sizeof( _calibration_sharpen_ds1[0][0] )};
static LookupTable calibration_temper_strength = {.ptr = _calibration_temper_strength, .rows = sizeof( _calibration_temper_strength ) / sizeof( _calibration_temper_strength[0] ), .cols = 2, .width = sizeof( _calibration_temper_strength[0][0] )};

uint32_t get_calibrations_dynamic_linear_dummy( ACameraCalibrations *c )
{
    uint32_t result = 0;
    if ( c != 0 ) {
        c->calibrations[CALIBRATION_STITCHING_LM_MED_NOISE_INTENSITY] = &calibration_stitching_lm_med_noise_intensity_thresh;
        c->calibrations[CALIBRATION_EXPOSURE_RATIO_ADJUSTMENT] = &calibration_exposure_ratio_adjustment;
        c->calibrations[CALIBRATION_SINTER_STRENGTH_MC_CONTRAST] = &calibration_sinter_strength_MC_contrast;
        c->calibrations[AWB_COLOUR_PREFERENCE] = &AWB_colour_preference;
        c->calibrations[CALIBRATION_AWB_MIX_LIGHT_PARAMETERS] = &calibration_awb_mix_light_parameters;
        c->calibrations[CALIBRATION_PF_RADIAL_LUT] = &calibration_pf_radial_lut;
        c->calibrations[CALIBRATION_PF_RADIAL_PARAMS] = &calibration_pf_radial_params;
        c->calibrations[CALIBRATION_SINTER_RADIAL_LUT] = &calibration_sinter_radial_lut;
        c->calibrations[CALIBRATION_SINTER_RADIAL_PARAMS] = &calibration_sinter_radial_params;
        c->calibrations[CALIBRATION_AWB_BG_MAX_GAIN] = &calibration_AWB_bg_max_gain;
        c->calibrations[CALIBRATION_IRIDIX8_STRENGTH_DK_ENH_CONTROL] = &calibration_iridix8_strength_dk_enh_control;
        c->calibrations[CALIBRATION_CMOS_CONTROL] = &calibration_cmos_control;
        c->calibrations[CALIBRATION_STATUS_INFO] = &calibration_status_info;
        c->calibrations[CALIBRATION_AUTO_LEVEL_CONTROL] = &calibration_auto_level_control;
        c->calibrations[CALIBRATION_DP_SLOPE] = &calibration_dp_slope;
        c->calibrations[CALIBRATION_DP_THRESHOLD] = &calibration_dp_threshold;
        c->calibrations[CALIBRATION_STITCHING_LM_MOV_MULT] = &calibration_stitching_lm_mov_mult;
        c->calibrations[CALIBRATION_STITCHING_LM_NP] = &calibration_stitching_lm_np;
        c->calibrations[CALIBRATION_STITCHING_MS_MOV_MULT] = &calibration_stitching_ms_mov_mult;
        c->calibrations[CALIBRATION_STITCHING_MS_NP] = &calibration_stitching_ms_np;
        c->calibrations[CALIBRATION_EVTOLUX_PROBABILITY_ENABLE] = &calibration_evtolux_probability_enable;
        c->calibrations[CALIBRATION_AWB_AVG_COEF] = &calibration_awb_avg_coef;
        c->calibrations[CALIBRATION_IRIDIX_AVG_COEF] = &calibration_iridix_avg_coef;
        c->calibrations[CALIBRATION_IRIDIX_STRENGTH_MAXIMUM] = &calibration_iridix_strength_maximum;
        c->calibrations[CALIBRATION_IRIDIX_MIN_MAX_STR] = &calibration_iridix_min_max_str;
        c->calibrations[CALIBRATION_IRIDIX_EV_LIM_FULL_STR] = &calibration_iridix_ev_lim_full_str;
        c->calibrations[CALIBRATION_IRIDIX_EV_LIM_NO_STR] = &calibration_iridix_ev_lim_no_str;
        c->calibrations[CALIBRATION_AE_CORRECTION] = &calibration_ae_correction;
        c->calibrations[CALIBRATION_AE_EXPOSURE_CORRECTION] = &calibration_ae_exposure_correction;
        c->calibrations[CALIBRATION_SINTER_STRENGTH] = &calibration_sinter_strength;
        c->calibrations[CALIBRATION_SINTER_STRENGTH1] = &calibration_sinter_strength1;
        c->calibrations[CALIBRATION_SINTER_THRESH1] = &calibration_sinter_thresh1;
        c->calibrations[CALIBRATION_SINTER_THRESH4] = &calibration_sinter_thresh4;
        c->calibrations[CALIBRATION_SINTER_INTCONFIG] = &calibration_sinter_intConfig;
        c->calibrations[CALIBRATION_SHARP_ALT_D] = &calibration_sharp_alt_d;
        c->calibrations[CALIBRATION_SHARP_ALT_UD] = &calibration_sharp_alt_ud;
        c->calibrations[CALIBRATION_SHARP_ALT_DU] = &calibration_sharp_alt_du;
        c->calibrations[CALIBRATION_SHARPEN_FR] = &calibration_sharpen_fr;
        c->calibrations[CALIBRATION_DEMOSAIC_NP_OFFSET] = &calibration_demosaic_np_offset;
        c->calibrations[CALIBRATION_MESH_SHADING_STRENGTH] = &calibration_mesh_shading_strength;
        c->calibrations[CALIBRATION_SATURATION_STRENGTH] = &calibration_saturation_strength;
        c->calibrations[CALIBRATION_CCM_ONE_GAIN_THRESHOLD] = &calibration_ccm_one_gain_threshold;
        c->calibrations[CALIBRATION_AE_CONTROL] = &calibration_ae_control;
        c->calibrations[CALIBRATION_AE_CONTROL_HDR_TARGET] = &calibration_ae_control_HDR_target;
        c->calibrations[CALIBRATION_RGB2YUV_CONVERSION] = &calibration_rgb2yuv_conversion;
        c->calibrations[CALIBRATION_AF_LMS] = &calibration_calibration_af_lms;
        c->calibrations[CALIBRATION_AF_ZONE_WGHT_HOR] = &calibration_calibration_af_zone_wght_hor;
        c->calibrations[CALIBRATION_AF_ZONE_WGHT_VER] = &calibration_calibration_af_zone_wght_ver;
        c->calibrations[CALIBRATION_AE_ZONE_WGHT_HOR] = &calibration_calibration_ae_zone_wght_hor;
        c->calibrations[CALIBRATION_AE_ZONE_WGHT_VER] = &calibration_calibration_ae_zone_wght_ver;
        c->calibrations[CALIBRATION_AWB_ZONE_WGHT_HOR] = &calibration_calibration_awb_zone_wght_hor;
        c->calibrations[CALIBRATION_AWB_ZONE_WGHT_VER] = &calibration_calibration_awb_zone_wght_ver;
        c->calibrations[CALIBRATION_CNR_UV_DELTA12_SLOPE] = &calibration_cnr_uv_delta12_slope;
        c->calibrations[CALIBRATION_FS_MC_OFF] = &calibration_fs_mc_off;
        c->calibrations[CALIBRATION_SINTER_SAD] = &calibration_sinter_sad;
        c->calibrations[CALIBRATION_SCALER_H_FILTER] = &calibration_scaler_h_filter;
        c->calibrations[CALIBRATION_SCALER_V_FILTER] = &calibration_scaler_v_filter;
        c->calibrations[CALIBRATION_SHARPEN_DS1] = &calibration_sharpen_ds1;
        c->calibrations[CALIBRATION_TEMPER_STRENGTH] = &calibration_temper_strength;
    } else {
        result = -1;
    }
    return result;
}
