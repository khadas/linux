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
static uint8_t _calibration_evtolux_probability_enable[] = {0};

static uint8_t _calibration_awb_avg_coef[] = {15};

static uint8_t _calibration_iridix_avg_coef[] = {15};

static uint16_t _calibration_ccm_one_gain_threshold[] = {7*256};

static uint8_t _calibration_iridix_strength_maximum[] = {255};

static uint16_t _calibration_iridix_min_max_str[] = {0};

static uint32_t _calibration_iridix_ev_lim_full_str[] = {1500000};

static uint32_t _calibration_iridix_ev_lim_no_str[] = {7000000, 3000000}; //3574729

static uint8_t _calibration_ae_correction[] = {128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128};

static uint32_t _calibration_ae_exposure_correction[] = {6710, 15739, 15778, 23282, 56186, 205000, 400000, 600000, 900000, 1400000, 1800000, 2200000}; //500,157778,500325,632161,1406400,6046465 //23282 - Max Lab Exposure

// ------------Noise reduction ----------------------//
static uint16_t _calibration_sinter_strength[][2] = {
    {0 * 256, 30}, //30
    {1 * 256, 30}, //30
    {2 * 256, 30}, //45
    {3 * 256, 30}, //55
    {4 * 256, 30}, //73
    {5 * 256, 30}, //74
    {6 * 256, 30}, //74
    {7 * 256, 30}  //82
};
// ------------Noise reduction ----------------------//
static uint16_t _calibration_sinter_strength_MC_contrast[][2] = {
    {0 * 256, 0}};

static uint16_t _calibration_sinter_strength1[][2] = {
    {0 * 256, 200},  //155
    {1 * 256, 200},  //155
    {2 * 256, 200},  //155
    {3 * 256, 200},  //155
    {4 * 256, 200},  //255 4 int
    {5 * 256, 200},  //255 4 int
    {6 * 256, 200},  //255 4 int
    {7 * 256, 200}}; //255 4 int

static uint16_t _calibration_sinter_thresh1[][2] = {
    {0 * 256, 20},
    {1 * 256, 20},
    {2 * 256, 20},
    {3 * 256, 20},
    {4 * 256, 20},
    {5 * 256, 20},
    {6 * 256, 20}};

static uint16_t _calibration_sinter_thresh4[][2] = {
    {0 * 256, 130},
    {1 * 256, 130},
    {2 * 256, 130},
    {3 * 256, 130},
    {4 * 256, 130},
    {5 * 256, 130},
    {6 * 256, 130}};

static uint16_t _calibration_sinter_intConfig[][2] = {
    {0 * 256, 0},
    {1 * 256, 0},
    {2 * 256, 0},
    {3 * 256, 0},
    {4 * 256, 0},
    {5 * 256, 0},
    {6 * 256, 0}};

static uint8_t _calibration_sinter_radial_lut[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28};

static uint16_t _calibration_sinter_radial_params[] = {
    0,    // rm_enable
    960, // rm_centre_x
    540, // rm_centre_y
    1770   // rm_off_centre_mult: round((2^31)/((rm_centre_x^2)+(rm_centre_y^2)))
};

static uint16_t _calibration_sinter_sad[][2] = {
    {0, 50},
    {1 * 256, 50},
    {2 * 256, 50},
    {3 * 256, 50},
    {4 * 256, 50},
    {5 * 256, 50},
    {6 * 256, 50}};
// ------------ Sharpening and demosaic
static uint16_t _calibration_sharp_alt_d[][2] = {
    {0 * 256, 39},
    {1 * 256, 39},
    {2 * 256, 39},
    {3 * 256, 39},
    {4 * 256, 39},
    {5 * 256, 39},
    {6 * 256, 39},
    {7 * 256, 39}};

static uint16_t _calibration_sharp_alt_ud[][2] = {
    {0 * 256, 20},
    {1 * 256, 20},
    {2 * 256, 20},
    {3 * 256, 20},
    {4 * 256, 20},
    {5 * 256, 20},
    {6 * 256, 20},
    {7 * 256, 20}};

static uint16_t _calibration_sharp_alt_du[][2] = {
    {0 * 256, 255},
    {1 * 256, 255},
    {2 * 256, 255},
    {3 * 256, 255},
    {4 * 256, 255},
    {5 * 256, 255},
    {6 * 256, 255},
    {7 * 256, 255}};

static uint16_t _calibration_sharpen_fr[][2] = {
    {0 * 256, 150},
    {1 * 256, 150},
    {2 * 256, 150},
    {3 * 256, 150},
    {4 * 256, 150},
    {5 * 256, 150},
    {6 * 256, 150},
    {7 * 256, 150},};

static uint16_t _calibration_demosaic_np_offset[][2] = {
    {0 * 256, 1},
    {1 * 256, 1},
    {2 * 256, 1},
    {3 * 256, 3},
    {4 * 256, 5},
    {5 * 256, 5},
    {6 * 256, 5}};


static uint16_t _calibration_mesh_shading_strength[][2] = {
    {0 * 256, 4096},
    {1 * 256, 3072},
    {2 * 256, 2048},
    {3 * 256, 1024},
    {4 * 256, 512},
    {5 * 256, 256},
    {6 * 256, 128}};


static uint16_t _calibration_saturation_strength[][2] = {
    {0 * 256, 128},
    {1 * 256, 128},
    {2 * 256, 128},
    {3 * 256, 128},
    {4 * 256, 128},
    {5 * 256, 110},
    {6 * 256, 110},
    {7 * 256, 105},
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
    {0 * 256, 2306},
    {1 * 256, 2306},
    {2 * 256, 2306},
    {3 * 256, 2306},
    {4 * 256, 2306},
    {5 * 256, 2306},
    {6 * 256, 2306},
    {7 * 256, 2306}};

static uint16_t _calibration_dp_threshold[][2] = {
    {0 * 256, 65},
    {1 * 256, 65},
    {2 * 256, 65},
    {3 * 256, 65},
    {4 * 256, 65},
    {5 * 256, 65},
    {6 * 256, 65},
    {7 * 256, 65}};

static uint16_t _calibration_AWB_bg_max_gain[][2] = {
    {0 * 256, 100},
    {1 * 256, 100},
    {7 * 256, 200},
};

// *** NOTE: to add/remove items in partition luts, please also update SYSTEM_EXPOSURE_PARTITION_VALUE_COUNT.
static uint16_t _calibration_cmos_exposure_partition_luts[][10] = {
    // {integration time, gain }
    // value: for integration time - milliseconds, for gains - multiplier.
    //		  Zero value means maximum.
    // lut partitions_balanced
    {
        10, 2,
        30, 4,
        60, 6,
        100, 8,
        0, 0,
    },

    // lut partition_int_priority
    {
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
    },
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
    126, // max sensor AG
    0,   // max sensor DG
    45,  // max isp DG
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
    25,      // dark_prc
    95,      // bright_prc
    400,     // min_dk: minimum dark enhancement
    2000,    // max_dk: maximum dark enhancement
    8,       // pD_cut_min: minimum intensity cut for dark regions in which dk_enh will be applied
    20,      // pD_cut_max: maximum intensity cut for dark regions in which dk_enh will be applied
    30 << 8, // dark contrast min
    50 << 8, // dark contrast max
    0,       // min_str: iridix strength in percentage //26
    50,      // max_str: iridix strength in percentage: 50 = 1x gain. 100 = 2x gain                //50
    40,      // dark_prc_gain_target: target in histogram (percentage) for dark_prc after iridix is applied
    30 << 8, // contrast_min: clip factor of strength for LDR scenes.
    40 << 8, // contrast_max: clip factor of strength for HDR scenes.
    25,      // max iridix gain
    0        // print debug
};

static uint32_t _calibration_ae_control[] = {
    15,  // AE convergance
    236, // LDR AE target -> this should match the 18% grey of teh output gamma
    0,   // AE tail weight
    0,   // WDR mode only: Max percentage of clipped pixels for long exposure: WDR mode only: 256 = 100% clipped pixels
    0,   // WDR mode only: Time filter for exposure ratio
    100, // control for clipping: bright percentage of pixels that should be below hi_target_prc
    99,  // control for clipping: highlights percentage (hi_target_prc): target for tail of histogram
    0,   // 1:0 enable | disable iridix global gain.
    10,  // AE tolerance
};

static uint16_t _calibration_ae_control_HDR_target[][2] = {
    {0 * 256, 236}, // HDR AE target should not be higher than LDR target
    {1 * 256, 236},
    {2 * 256, 236},
    {3 * 256, 236},
    {4 * 256, 236},
    {5 * 256, 236},
    {6 * 256, 236},
    {7 * 256, 236},
};

static uint8_t _calibration_pf_radial_lut[] = {0, 0, 0, 0, 100, 180, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};

static uint16_t _calibration_pf_radial_params[] = {
    1920, // rm_centre_x
    1080, // rm_centre_y
    442   // rm_off_centre_mult: round((2^31)/((540^2)+(960^2)))
};

static uint32_t _calibration_auto_level_control[] = {
    1,  // black_percentage
    99, // white_percentage
    20,  // auto_black_min
    50, // auto_black_max
    90, // auto_white_prc
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
    {0 * 256, 6500},
    {1 * 256, 6500},
    {2 * 256, 6500},
    {3 * 256, 6500},
    {4 * 256, 6500},
    {5 * 256, 6500},
    {6 * 256, 6500},
    {7 * 256, 6500},
};


static uint16_t _calibration_fs_mc_off[] = {
    8 * 256, // gain_log2 threshold. if gain is higher than the current gain_log2. mc off mode will be enabed.
};
static int16_t _AWB_colour_preference[] = {7500, 6000, 4700, 2800};

static uint32_t _calibration_awb_mix_light_parameters[] = {
    1,    // 1 = enable, 0 = disable
    900,  //lux low boundary for mix light lux range : range = {500: inf}
    2500, // lux high boundary for mix light range : range = {500: inf}
    800,  // contrast threshold for mix light: range = {200:2000}
    330,  //BG threshold {255:400}
    5,    // BG weight
    419,  // rgHigh_LUT_max
    252,  // rgHigh_LUT_min
    0     // print debug
};

//static uint16_t _calibration_rgb2yuv_conversion[] = {76, 150, 29, 0x8025, 0x8049, 111, 157, 0x8083, 0x8019, 0, 512, 512};
static uint16_t _calibration_rgb2yuv_conversion[] = {85, 168, 31, 0x802b, 0x8053, 124, 176, 0x8095, 0x801d, 0x8022, 511, 511};

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
    {0 * 256, 80},
    {1 * 256, 90},
    {2 * 256, 150},
    {3 * 256, 150},
    {4 * 256, 150},
    {5 * 256, 150},
    {6 * 256, 150},
    {7 * 256, 150}};

static uint32_t _calibration_af_lms[] = {
    70 << 6,                       // Down_FarEnd
    70 << 6,                       // Hor_FarEnd
    70 << 6,                       // Up_FarEnd
    112 << 6,                      // Down_Infinity
    112 << 6,                      // Hor_Infinity
    112 << 6,                      // Up_Infinity
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


static int32_t _calibration_gamma_threshold[] = {1, 3357408, 4357408};

// CALIBRATION_GAMMA_EV1
static uint16_t _calibration_gamma_ev1[] =
    /*sRGB highcontrast{0, 150, 261, 359, 452, 541, 623, 702, 781, 859, 937, 1014, 1087, 1158, 1224, 1288, 1348, 1407, 1464, 1519, 1572, 1625, 1676, 1727, 1775, 1823, 1869, 1913, 1956, 1999, 2041, 2082, 2123, 2162, 2201, 2238, 2276, 2312, 2348, 2383, 2417, 2451, 2485, 2516, 2549, 2580, 2611, 2641, 2671, 2701, 2730, 2759, 2787, 2816, 2843, 2871, 2897, 2923, 2950, 2975, 3000, 3025, 3048, 3071, 3095, 3118, 3139, 3161, 3182, 3204, 3224, 3244, 3263, 3283, 3302, 3322, 3340, 3358, 3377, 3394, 3412, 3429, 3447, 3464, 3481, 3497, 3514, 3530, 3546, 3562, 3579, 3594, 3610, 3625, 3641, 3656, 3671, 3686, 3701, 3716, 3731, 3745, 3759, 3774, 3788, 3802, 3816, 3830, 3843, 3857, 3871, 3884, 3898, 3911, 3924, 3936, 3949, 3962, 3974, 3987, 4000, 4011, 4024, 4036, 4048, 4060, 4072, 4083, 4095}; */
    /*sRGB 65{0,192,318,419,511,596,675,749,820,887,950,1012,1070,1126,1180,1231,1282,1332,1380,1428,1475,1521,1568,1614,1660,1706,1751,1796,1842,1890,1938,1988,2037,2085,2133,2180,2228,2273,2319,2363,2406,2447,2489,2528,2566,2603,2638,2671,2703,2734,2762,2790,2818,2845,2871,2897,2921,2946,2970,2993,3016,3038,3060,3081,3103,3123,3143,3163,3183,3203,3222,3241,3259,3278,3296,3315,3333,3351,3369,3386,3403,3420,3438,3455,3472,3489,3506,3522,3539,3555,3572,3588,3604,3620,3635,3651,3666,3681,3696,3712,3726,3741,3755,3770,3784,3798,3813,3827,3840,3854,3868,3881,3895,3908,3921,3934,3947,3960,3972,3985,3998,4010,4023,4035,4048,4060,4071,4083,4095}; */
    //{0,347,539,679,794,894,982,1062,1136,1204,1268,1329,1386,1441,1493,1543,1591,1638,1683,1726,1768,1809,1849,1888,1926,1963,1999,2034,2068,2102,2135,2168,2200,2231,2262,2292,2322,2351,2380,2408,2436,2463,2491,2517,2544,2570,2596,2621,2646,2671,2695,2719,2743,2767,2790,2814,2836,2859,2882,2904,2926,2948,2969,2990,3012,3033,3053,3074,3094,3115,3135,3155,3174,3194,3213,3233,3252,3271,3290,3308,3327,3345,3364,3382,3400,3418,3436,3453,3471,3488,3506,3523,3540,3557,3574,3591,3607,3624,3640,3657,3673,3689,3705,3721,3737,3753,3769,3785,3800,3816,3831,3846,3862,3877,3892,3907,3922,3937,3951,3966,3981,3995,4010,4024,4039,4053,4067,4081,4095};
    //{0,256,380,479,565,642,712,778,840,898,954,1007,1059,1108,1156,1203,1248,1292,1335,1377,1418,1458,1497,1535,1573,1610,1647,1683,1718,1753,1787,1821,1854,1887,1920,1952,1984,2015,2046,2075,2106,2136,2165,2195,2224,2252,2281,2309,2337,2365,2392,2419,2447,2473,2500,2526,2552,2578,2604,2630,2655,2680,2705,2730,2755,2779,2804,2828,2852,2876,2900,2923,2947,2970,2993,3016,3039,3062,3085,3107,3130,3152,3174,3196,3218,3240,3262,3284,3305,3326,3348,3369,3390,3411,3432,3453,3474,3494,3515,3535,3556,3576,3596,3616,3636,3656,3676,3696,3716,3735,3755,3774,3794,3813,3832,3851,3870,3889,3908,3927,3946,3965,3984,4002,4021,4039,4058,4076,4095};
    {0,144,289,426,541,641,730,812,887,957,1024,1086,1146,1203,1257,1310,1360,1409,1456,1502,1547,1590,1632,1673,1714,1753,1791,1829,1866,1902,1938,1972,2007,2040,2073,2106,2138,2169,2200,2231,2261,2291,2321,2350,2378,2406,2434,2462,2489,2516,2543,2569,2596,2621,2647,2672,2697,2722,2747,2771,2795,2819,2843,2866,2889,2913,2935,2958,2981,3003,3025,3047,3069,3090,3112,3133,3154,3175,3196,3217,3237,3258,3278,3298,3318,3338,3358,3378,3397,3417,3436,3455,3474,3493,3512,3530,3549,3567,3586,3604,3622,3640,3658,3676,3694,3712,3729,3747,3764,3782,3799,3816,3833,3850,3867,3884,3900,3917,3934,3950,3967,3983,3999,4016,4032,4048,4064,4080,4095};

// CALIBRATION_GAMMA_EV2
static uint16_t _calibration_gamma_ev2[] =
    /*sRGB highcontrast{0, 150, 261, 359, 452, 541, 623, 702, 781, 859, 937, 1014, 1087, 1158, 1224, 1288, 1348, 1407, 1464, 1519, 1572, 1625, 1676, 1727, 1775, 1823, 1869, 1913, 1956, 1999, 2041, 2082, 2123, 2162, 2201, 2238, 2276, 2312, 2348, 2383, 2417, 2451, 2485, 2516, 2549, 2580, 2611, 2641, 2671, 2701, 2730, 2759, 2787, 2816, 2843, 2871, 2897, 2923, 2950, 2975, 3000, 3025, 3048, 3071, 3095, 3118, 3139, 3161, 3182, 3204, 3224, 3244, 3263, 3283, 3302, 3322, 3340, 3358, 3377, 3394, 3412, 3429, 3447, 3464, 3481, 3497, 3514, 3530, 3546, 3562, 3579, 3594, 3610, 3625, 3641, 3656, 3671, 3686, 3701, 3716, 3731, 3745, 3759, 3774, 3788, 3802, 3816, 3830, 3843, 3857, 3871, 3884, 3898, 3911, 3924, 3936, 3949, 3962, 3974, 3987, 4000, 4011, 4024, 4036, 4048, 4060, 4072, 4083, 4095}; */
    /*sRGB 65{0,192,318,419,511,596,675,749,820,887,950,1012,1070,1126,1180,1231,1282,1332,1380,1428,1475,1521,1568,1614,1660,1706,1751,1796,1842,1890,1938,1988,2037,2085,2133,2180,2228,2273,2319,2363,2406,2447,2489,2528,2566,2603,2638,2671,2703,2734,2762,2790,2818,2845,2871,2897,2921,2946,2970,2993,3016,3038,3060,3081,3103,3123,3143,3163,3183,3203,3222,3241,3259,3278,3296,3315,3333,3351,3369,3386,3403,3420,3438,3455,3472,3489,3506,3522,3539,3555,3572,3588,3604,3620,3635,3651,3666,3681,3696,3712,3726,3741,3755,3770,3784,3798,3813,3827,3840,3854,3868,3881,3895,3908,3921,3934,3947,3960,3972,3985,3998,4010,4023,4035,4048,4060,4071,4083,4095}; */
    //{0,347,539,679,794,894,982,1062,1136,1204,1268,1329,1386,1441,1493,1543,1591,1638,1683,1726,1768,1809,1849,1888,1926,1963,1999,2034,2068,2102,2135,2168,2200,2231,2262,2292,2322,2351,2380,2408,2436,2463,2491,2517,2544,2570,2596,2621,2646,2671,2695,2719,2743,2767,2790,2814,2836,2859,2882,2904,2926,2948,2969,2990,3012,3033,3053,3074,3094,3115,3135,3155,3174,3194,3213,3233,3252,3271,3290,3308,3327,3345,3364,3382,3400,3418,3436,3453,3471,3488,3506,3523,3540,3557,3574,3591,3607,3624,3640,3657,3673,3689,3705,3721,3737,3753,3769,3785,3800,3816,3831,3846,3862,3877,3892,3907,3922,3937,3951,3966,3981,3995,4010,4024,4039,4053,4067,4081,4095};
    //{0,256,380,479,565,642,712,778,840,898,954,1007,1059,1108,1156,1203,1248,1292,1335,1377,1418,1458,1497,1535,1573,1610,1647,1683,1718,1753,1787,1821,1854,1887,1920,1952,1984,2015,2046,2075,2106,2136,2165,2195,2224,2252,2281,2309,2337,2365,2392,2419,2447,2473,2500,2526,2552,2578,2604,2630,2655,2680,2705,2730,2755,2779,2804,2828,2852,2876,2900,2923,2947,2970,2993,3016,3039,3062,3085,3107,3130,3152,3174,3196,3218,3240,3262,3284,3305,3326,3348,3369,3390,3411,3432,3453,3474,3494,3515,3535,3556,3576,3596,3616,3636,3656,3676,3696,3716,3735,3755,3774,3794,3813,3832,3851,3870,3889,3908,3927,3946,3965,3984,4002,4021,4039,4058,4076,4095};
    {0,144,289,426,541,641,730,812,887,957,1024,1086,1146,1203,1257,1310,1360,1409,1456,1502,1547,1590,1632,1673,1714,1753,1791,1829,1866,1902,1938,1972,2007,2040,2073,2106,2138,2169,2200,2231,2261,2291,2321,2350,2378,2406,2434,2462,2489,2516,2543,2569,2596,2621,2647,2672,2697,2722,2747,2771,2795,2819,2843,2866,2889,2913,2935,2958,2981,3003,3025,3047,3069,3090,3112,3133,3154,3175,3196,3217,3237,3258,3278,3298,3318,3338,3358,3378,3397,3417,3436,3455,3474,3493,3512,3530,3549,3567,3586,3604,3622,3640,3658,3676,3694,3712,3729,3747,3764,3782,3799,3816,3833,3850,3867,3884,3900,3917,3934,3950,3967,3983,3999,4016,4032,4048,4064,4080,4095};

static uint32_t _calibration_custom_settings_context[][4] = {
    //stop sequence - address is 0x0000
    //mesh shading
    { 0x1abfc, 0x1f1fe445L, 0xffffffff,4},//shading mesh scale 0x1f1fe445L, 0x1f1fe449L
    //Demosaic
    { 0x1ae7c, 0x64bec8c8L, 0xffffffff,4 }, // UU Slope/VA Slope/AA Slope/VH Slope
    { 0x1ae84, 0x006e006eL, 0xfff0fff,4 }, //AA Thresh/VH Thresh
    { 0x1ae88, 0x00fa006eL, 0xfff0fff,4 }, //UU Thresh/VA Thresh
    { 0x1aec0, 0x080896L, 0xffffff,4 }, //grey det thresh/lg det thresh/UU SH Slope
    { 0x1aec8, 0x10688000L, 0xffffffff,4 }, //grey det slope/lg det slope
    { 0x1aeb4, 0x1f40L, 0x1fff,2 }, //min_d_strength
    { 0x1aeb8, 0x1f40L, 0x1fff,2 }, //min_ud_strength
    { 0x1aecc, 0x7080708L, 0x1fff1fff,4 }, //max_ud_strength/max_d_strength
    //CNR
    { 0x1b0f0, 0xcccL, 0xfff,2 }, //global_offset
    { 0x1b0f4, 0x333L, 0x0,2 }, //global_slope
    { 0x1b0fc, 0x578L, 0xfff,2 }, //uv_seg1_offset
    { 0x1b100, 0xea60L, 0x0,2 }, //uv_seg1_slope
    { 0x1b13c, 0xff00L, 0x0,2 }, //uv_var1_slope
    { 0x1b148, 0xff00L, 0x0,2 }, //uv_var2_slope
    //sinter noise pofile
    { 0x19368, 0x1e00L, 0xff07,2 }, //0x19368[0]: use LUT, 1=use LUT data, 0 = use exposure mask provided by Frame stitching or threshold
    { 0x19364, 0xff00ff00L, 0x00ff00ff,4 }, //Strength 0/Strength 1/Strength 2/Strength 4
    { 0x1937c, 0x00000000L, 0x0,4 }, //noise level 3/noise level 2/noise level 1/noise level 0
    { 0x19370, 0x0L, 0x0000ffff,2 }, //Thresh1 [15:0]
    { 0x19374, 0x0L, 0x0000ffff,2 }, //Thresh2 [15:0]
    { 0x19378, 0x0L, 0x0000ffff,2 }, //Thresh3 [15:0]
    //temper
    {0x1aa24, 0x001L, 0xff0f,2 }, //Delta/Recursion Limit
    {0x1aa28, 0x00L, 0x1f,1 }, //global offset [7:0]/Black Reflect/use_exp_mask/use LUT
    //temper Noise Profile
    { 0x1aa30, 0x3fe2L, 0x0000ffff,2 }, //Thresh1 [15:0]
    { 0x1aa34, 0x8000L, 0x0000ffff,2 }, //Thresh2 [15:0]
    { 0x1aa38, 0xc000L, 0x0000ffff,2 }, //Thresh3 [15:0]
    { 0x1aa3c, 0x00000000L, 0xffffffff,4 }, //noise level 3/noise level 2/noise level 1/noise level 0

    {0x0000, 0x0000, 0x0000, 0x0000}};

static uint32_t _calibration_defog_control[] = {
    0, //defog enable
    3, //defog detect mode
    600000, //acc fog value threshhold
    80, //hist fog idx threshhold
    10, //hist fog pec threshhold_1
    20, //hist fog pec threshhold_2
    1024, //ratio delta
    950, //max rng
    50, //min rng
    10, //black percentage
    995, //white percentage
    15, //avg_coeff
};

// { GAIN_LOG2_ID*256, lum_thresh, sad_amp, uu_sh_slope, uu_sh_thresh, luma_thresh_low_d,
// luma_thresh_low_ud, luma_slope_low_d, luma_slope_low_ud, luma_thresh_high_d,
// luma_thresh_high_ud, luma_slope_high_d, luma_slope_high_ud }
static uint32_t _calibration_demosaic_rgb_extension_control[][13] = {
    {0 * 256, 96, 0, 150, 0, 50, 100, 7000, 7000, 4000, 4000, 8000, 8000},
    {1 * 256, 96, 0, 150, 0, 50, 100, 7000, 7000, 4000, 4000, 8000, 8000},
    {2 * 256, 96, 0, 150, 0, 50, 100, 7000, 7000, 4000, 4000, 8000, 8000},
    {3 * 256, 96, 0, 150, 0, 50, 100, 7000, 7000, 4000, 4000, 8000, 8000},
    {4 * 256, 96, 0, 150, 0, 50, 100, 7000, 7000, 4000, 4000, 8000, 8000},
    {5 * 256, 96, 0, 150, 0, 50, 100, 7000, 7000, 4000, 4000, 8000, 8000},
    {6 * 256, 96, 0, 150, 0, 50, 100, 7000, 7000, 4000, 4000, 8000, 8000},
    {7 * 256, 96, 0, 150, 0, 50, 100, 7000, 7000, 4000, 4000, 8000, 8000},
};

// { GAIN_LOG2_ID*256, alpha_undershoot, luma_thresh_low, luma_slope_low, luma_thresh_high, luma_slope_high }
static uint32_t _calibration_fr_sharpen_extension_control[][6] = {
    {0 * 256, 10, 350, 1000, 1000, 1700},
    {1 * 256, 10, 350, 1000, 1000, 1700},
    {2 * 256, 10, 350, 1000, 1000, 1700},
    {3 * 256, 10, 350, 1000, 1000, 1700},
    {4 * 256, 10, 350, 1000, 1000, 1700},
    {5 * 256, 10, 350, 1000, 1000, 1700},
    {6 * 256, 10, 350, 1000, 1000, 1700},
    {7 * 256, 10, 350, 1000, 1000, 1700},
};

// { GAIN_LOG2_ID*256, alpha_undershoot, luma_thresh_low, luma_slope_low, luma_thresh_high, luma_slope_high }
static uint32_t _calibration_ds_sharpen_extension_control[][6] = {
    {0 * 256, 10, 400, 1000, 900, 1700},
    {1 * 256, 10, 400, 1000, 900, 1700},
    {2 * 256, 10, 400, 1000, 900, 1700},
    {3 * 256, 10, 400, 1000, 900, 1700},
    {4 * 256, 10, 400, 1000, 900, 1700},
    {5 * 256, 10, 400, 1000, 900, 1700},
    {6 * 256, 10, 400, 1000, 900, 1700},
    {7 * 256, 10, 400, 1000, 900, 1700},
};

// { GAIN_LOG2_ID*256, delta_factor, umean1_thd, umean1_offset, umean1_slope, umean2_thd, umean2_offset, umean2_slope,
// vmean1_thd, vmean1_offset, vmean1_slope, vmean2_thd, vmean2_offset, vmean2_slope, uv_delta1_thd, uv_delta1_offset,
// uv_delta1_slope, uv_delta2_thd, uv_delta2_offset, uv_delta2_slope }
static uint32_t _calibration_cnr_extension_control[][20] = {
    {0 * 256, 1000, 0, 200, 57000, 0, 200, 57000, 0, 62, 59316, 0, 62, 59316, 0, 800, 1000, 0, 800, 1000},
    {1 * 256, 1000, 0, 200, 57000, 0, 200, 57000, 0, 62, 59316, 0, 62, 59316, 0, 800, 1000, 0, 800, 1000},
    {2 * 256, 1000, 0, 200, 57000, 0, 200, 57000, 0, 62, 59316, 0, 62, 59316, 0, 800, 1000, 0, 800, 1000},
    {3 * 256, 1000, 0, 200, 57000, 0, 200, 57000, 0, 62, 59316, 0, 62, 59316, 0, 800, 1000, 0, 800, 1000},
    {4 * 256, 1000, 0, 200, 57000, 0, 200, 57000, 0, 62, 59316, 0, 62, 59316, 0, 800, 1000, 0, 800, 1000},
    {5 * 256, 1000, 0, 200, 57000, 0, 200, 57000, 0, 62, 59316, 0, 62, 59316, 0, 800, 1000, 0, 800, 1000},
    {6 * 256, 1000, 0, 200, 57000, 0, 200, 57000, 0, 62, 59316, 0, 62, 59316, 0, 800, 1000, 0, 800, 1000},
    {7 * 256, 1000, 0, 200, 57000, 0, 200, 57000, 0, 62, 59316, 0, 62, 59316, 0, 800, 1000, 0, 800, 1000},
};

// { GAIN_LOG2_ID*256, svariance, bright_pr, contrast, white_level }
static uint32_t _calibration_iridix_extension_control[][5] = {
    {0 * 256, 14, 220, 180, 809600},
    {1 * 256, 14, 220, 180, 809600},
    {2 * 256, 14, 220, 180, 809600},
    {3 * 256, 14, 220, 180, 809600},
    {4 * 256, 14, 220, 180, 809600},
    {5 * 256, 14, 220, 180, 809600},
    {6 * 256, 14, 220, 180, 809600},
    {7 * 256, 14, 220, 180, 809600},
};

// { GAIN_LOG2_ID*256, black_level_in, black_level_out }
static uint32_t _calibration_sqrt_extension_control[][3] = {
    {0 * 256, 65792, 4112},
    {1 * 256, 65792, 4112},
    {2 * 256, 65792, 4112},
    {3 * 256, 65792, 4112},
    {4 * 256, 65792, 4112},
    {5 * 256, 65792, 4112},
    {6 * 256, 65792, 4112},
    {7 * 256, 65792, 4112},
};

// { GAIN_LOG2_ID*256, black_level_in, black_level_out }
static uint32_t _calibration_square_be_extension_control[][3] = {
    {0 * 256, 4112, 68792},
    {1 * 256, 4112, 68792},
    {2 * 256, 4112, 68792},
    {3 * 256, 4112, 68792},
    {4 * 256, 4112, 68792},
    {5 * 256, 4112, 68792},
    {6 * 256, 4112, 68792},
    {7 * 256, 4112, 68792},
};

// { GAIN_LOG2_ID*256, dpdev threshold }
static uint16_t _calibration_dp_devthreshold[][2] = {
    {0 * 256, 32768},
    {1 * 256, 32768},
    {2 * 256, 32768},
    {3 * 256, 32768},
    {4 * 256, 32768},
    {5 * 256, 32768},
    {6 * 256, 32768},
    {7 * 256, 32768},
};

// { GAIN_LOG2_ID*256, hue strength,luma strength,sat strength,saturation strength,purple strength }
static uint16_t _calibration_pf_correction[][6] = {
    {0 * 256, 1024, 1024, 100, 10, 2048},
    {1 * 256, 1024, 1024, 100, 10, 2048},
    {2 * 256, 1024, 1024, 100, 10, 2048},
    {3 * 256, 1024, 1024, 100, 10, 2048},
    {4 * 256, 1024, 1024, 100, 10, 2048},
    {5 * 256, 1024, 1024, 100, 10, 2048},
    {6 * 256, 1024, 1024, 100, 10, 2048},
    {7 * 256, 1024, 1024, 100, 10, 2048},
};

// { GAIN_LOG2_ID*256, fc slope, alias slop, alias thresh }
static uint16_t _calibration_fc_correction[][4] = {
    {0 * 256, 150, 85, 0},
    {1 * 256, 150, 85, 0},
    {2 * 256, 150, 85, 0},
    {3 * 256, 150, 85, 0},
    {4 * 256, 150, 85, 0},
    {5 * 256, 150, 85, 0},
    {6 * 256, 150, 85, 0},
    {7 * 256, 150, 85, 0},
};

static LookupTable calibration_gamma_threshold = {.ptr = _calibration_gamma_threshold, .rows = 1, .cols = sizeof( _calibration_gamma_threshold ) / sizeof( _calibration_gamma_threshold[0] ), .width = sizeof( _calibration_gamma_threshold[0] )};
static LookupTable calibration_gamma_ev1 = {.ptr = _calibration_gamma_ev1, .rows = 1, .cols = sizeof( _calibration_gamma_ev1 ) / sizeof( _calibration_gamma_ev1[0] ), .width = sizeof( _calibration_gamma_ev1[0] )};
static LookupTable calibration_gamma_ev2 = {.ptr = _calibration_gamma_ev2, .rows = 1, .cols = sizeof( _calibration_gamma_ev2 ) / sizeof( _calibration_gamma_ev2[0] ), .width = sizeof( _calibration_gamma_ev2[0] )};

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
static LookupTable calibration_cmos_exposure_partition_luts = {.ptr = _calibration_cmos_exposure_partition_luts, .rows = sizeof( _calibration_cmos_exposure_partition_luts ) / sizeof( _calibration_cmos_exposure_partition_luts[0] ), .cols = 10, .width = sizeof( _calibration_cmos_exposure_partition_luts[0][0] )};
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
static LookupTable calibration_custom_settings_context = {.ptr = _calibration_custom_settings_context, .rows = sizeof( _calibration_custom_settings_context ) / sizeof( _calibration_custom_settings_context[0] ), .cols = 4, .width = sizeof( _calibration_custom_settings_context[0][0] )};
static LookupTable calibration_defog_control = {.ptr = _calibration_defog_control, .rows = 1, .cols = sizeof(_calibration_defog_control) / sizeof(_calibration_defog_control[0]), .width = sizeof(_calibration_defog_control[0])};
static LookupTable calibration_demosaic_rgb_extension_control = {.ptr = _calibration_demosaic_rgb_extension_control, .rows = sizeof(_calibration_demosaic_rgb_extension_control) / sizeof(_calibration_demosaic_rgb_extension_control[0]), .cols = 13, .width = sizeof(_calibration_demosaic_rgb_extension_control[0][0])};
static LookupTable calibration_fr_sharpen_extension_control = {.ptr = _calibration_fr_sharpen_extension_control, .rows = sizeof(_calibration_fr_sharpen_extension_control) / sizeof(_calibration_fr_sharpen_extension_control[0]), .cols = 6, .width = sizeof(_calibration_fr_sharpen_extension_control[0][0])};
static LookupTable calibration_ds_sharpen_extension_control = {.ptr = _calibration_ds_sharpen_extension_control, .rows = sizeof(_calibration_ds_sharpen_extension_control) / sizeof(_calibration_ds_sharpen_extension_control[0]), .cols = 6, .width = sizeof(_calibration_ds_sharpen_extension_control[0][0])};
static LookupTable calibration_cnr_extension_control = {.ptr = _calibration_cnr_extension_control, .rows = sizeof(_calibration_cnr_extension_control) / sizeof(_calibration_cnr_extension_control[0]), .cols = 20, .width = sizeof(_calibration_cnr_extension_control[0][0])};
static LookupTable calibration_iridix_extension_control = {.ptr = _calibration_iridix_extension_control, .rows = sizeof(_calibration_iridix_extension_control) / sizeof(_calibration_iridix_extension_control[0]), .cols = 5, .width = sizeof(_calibration_iridix_extension_control[0][0])};
static LookupTable calibration_sqrt_extension_control = {.ptr = _calibration_sqrt_extension_control, .rows = sizeof(_calibration_sqrt_extension_control) / sizeof(_calibration_sqrt_extension_control[0]), .cols = 3, .width = sizeof(_calibration_sqrt_extension_control[0][0])};
static LookupTable calibration_square_be_extension_control = {.ptr = _calibration_square_be_extension_control, .rows = sizeof(_calibration_square_be_extension_control) / sizeof(_calibration_square_be_extension_control[0]), .cols = 3, .width = sizeof(_calibration_square_be_extension_control[0][0])};
static LookupTable calibration_dp_devthreshold = {.ptr = _calibration_dp_devthreshold, .rows = sizeof(_calibration_dp_devthreshold) / sizeof(_calibration_dp_devthreshold[0]), .cols = 2, .width = sizeof(_calibration_dp_devthreshold[0][0])};
static LookupTable calibration_pf_correction = {.ptr = _calibration_pf_correction, .rows = sizeof(_calibration_pf_correction) / sizeof(_calibration_pf_correction[0]), .cols = 6, .width = sizeof(_calibration_pf_correction[0][0])};
static LookupTable calibration_fc_correction = {.ptr = _calibration_fc_correction, .rows = sizeof(_calibration_fc_correction) / sizeof(_calibration_fc_correction[0]), .cols = 4, .width = sizeof(_calibration_fc_correction[0][0])};

uint32_t get_calibrations_dynamic_linear_ov13858( ACameraCalibrations *c )
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
        c->calibrations[CALIBRATION_CMOS_EXPOSURE_PARTITION_LUTS] = &calibration_cmos_exposure_partition_luts;
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
        c->calibrations[CALIBRATION_GAMMA_EV1] = &calibration_gamma_ev1;
        c->calibrations[CALIBRATION_GAMMA_EV2] = &calibration_gamma_ev2;
        c->calibrations[CALIBRATION_GAMMA_THRESHOLD] = &calibration_gamma_threshold;
        c->calibrations[CALIBRATION_CUSTOM_SETTINGS_CONTEXT] = &calibration_custom_settings_context;
        c->calibrations[CALIBRATION_DEFOG_CONTROL] = &calibration_defog_control;
        c->calibrations[CALIBRATION_DEMOSAIC_RGB_EXT_CONTROL] = &calibration_demosaic_rgb_extension_control;
        c->calibrations[CALIBRATION_FR_SHARPEN_EXT_CONTROL] = &calibration_fr_sharpen_extension_control;
        c->calibrations[CALIBRATION_DS_SHARPEN_EXT_CONTROL] = &calibration_ds_sharpen_extension_control;
        c->calibrations[CALIBRATION_CNR_EXT_CONTROL] = &calibration_cnr_extension_control;
        c->calibrations[CALIBRATION_IRIDIX_EXT_CONTROL] = &calibration_iridix_extension_control;
        c->calibrations[CALIBRATION_SQRT_EXT_CONTROL] = &calibration_sqrt_extension_control;
        c->calibrations[CALIBRATION_SQUARE_BE_EXT_CONTROL] = &calibration_square_be_extension_control;
        c->calibrations[CALIBRATION_DP_DEVTHRESHOLD] = &calibration_dp_devthreshold;
        c->calibrations[CALIBRATION_PF_CORRECTION] = &calibration_pf_correction;
        c->calibrations[CALIBRATION_FC_CORRECTION] = &calibration_fc_correction;
    } else {
        result = -1;
    }
    return result;
}
