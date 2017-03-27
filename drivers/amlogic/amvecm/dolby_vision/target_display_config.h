/**
* This product contains one or more programs protected under international
* and U.S. copyright laws as unpublished works.  They are confidential and
* proprietary to Dolby Laboratories.  Their reproduction or disclosure, in
* whole or in part, or the production of derivative works therefrom without
* the express permission of Dolby Laboratories is prohibited.
*
*             Copyright 2016 by Dolby Laboratories.
*                           All rights reserved.
*
* @brief Target Display Config.
* @file target_display_config.h
*
*/

#ifndef TARGET_DISPLAY_CONFIG_H_
#define TARGET_DISPLAY_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#define PQ2G_LUT_SIZE (4 + 1024 * 4 + 16 * 3)
#define GM_LUT_HDR_SIZE  (13 + 2*9)
#define LUT_DIM          17
#define GM_LUT_SIZE      (3 * LUT_DIM * LUT_DIM * LUT_DIM * 2)
#define BACLIGHT_LUT_SIZE 4096
#define TUNING_LUT_ENTRIES 14

#define TUNINGMODE_FORCE_ABSOLUTE        0x1
#define TUNINGMODE_EXTLEVEL1_DISABLE     0x2
#define TUNINGMODE_EXTLEVEL2_DISABLE     0x4
#define TUNINGMODE_EXTLEVEL4_DISABLE     0x8
#define TUNINGMODE_EXTLEVEL5_DISABLE     0x10
#define TUNINGMODE_EL_FORCEDDISABLE      0x20

/*! @brief Output CSC configuration.
*/
# pragma pack(push, 1)
struct TgtOutCscCfg {
	int32_t   lms2RgbMat[3][3]; /**<@brief  LMS to RGB matrix */
	int32_t   lms2RgbMatScale;  /**<@brief  LMS 2 RGB matrix scale */
	uint8_t   whitePoint[3];    /**<@brief  White point */
	uint8_t   whitePointScale;  /**<@brief  White point scale */
	int32_t   reserved[3];
};
#pragma pack(pop)

/*! @brief Global dimming configuration.
*/
# pragma pack(push, 1)
struct TgtGDCfg {
	int32_t   gdEnable;             /**<@brief  Global Dimming enabled */
	uint32_t  gdWMin;               /**<@brief  Global Dimming window min */
	uint32_t  gdWMax;               /**<@brief  Global Dimming window max */
	uint32_t  gdWMm;                /**<@brief  Global Dimming window max's min */
	uint32_t  gdWDynRngSqrt;        /**<@brief  Only used in 2.8.6  */
	uint32_t  gdWeightMean;         /**<@brief  Only used in 2.8.6  */
	uint32_t  gdWeightStd;          /**<@brief  Only used in 2.8.6  */
	uint32_t  gdDelayMilliSec_hdmi; /**<@brief  Back light delay for HDMI case */
	int32_t   gdRgb2YuvExt;         /**<@brief  Global Dimming  */
	int32_t   gdM33Rgb2Yuv[3][3];   /**<@brief  RGB to YUV matrix */
	int32_t   gdM33Rgb2YuvScale2P;  /**<@brief  RGB to YUV scale  */
	int32_t   gdRgb2YuvOffExt;      /**<@brief  Hardcoded to 1  */
	int32_t   gdV3Rgb2YuvOff[3];    /**<@brief  RGB to YUV offset  */
	uint32_t  gdUpBound;            /**<@brief  Only used in 2.8.6  */
	uint32_t  gdLowBound;           /**<@brief  Only used in 2.8.6  */
	uint32_t  lastMaxPq;            /**<@brief  Only used in 2.8.6  */
	uint16_t  gdWMinPq;             /**<@brief  Global Dimming window min in PQ */
	uint16_t  gdWMaxPq;             /**<@brief  Global Dimming window max in PQ */
	uint16_t  gdWMmPq;              /**<@brief  Global Dimming window max's min in PQ */
	uint16_t  gdTriggerPeriod;      /**<@brief  Global Dimming will be triggered only if time elapses more than gdTriggerPeriod frames */
	uint32_t  gdTriggerLinThresh;   /**<@brief  Global Dimming will be triggered only if tmax changes more than gdTriggerLinThresh nits */
	uint32_t  gdDelayMilliSec_ott;  /**<@brief  Back light delay for OTT case */
	uint32_t  reserved[9];
};
#pragma pack(pop)

/*! @defgroup general Enumerations and data structures
 *
 * @{
 */
/*! @brief Target configuration.
*/
# pragma pack(push, 1)
struct TargetDisplayConfig {
	uint16_t gain;          /**<@brief  Gain */
	uint16_t offset;        /**<@brief  Offset */
	uint16_t gamma;         /**<@brief  Gamma */
	uint16_t eotf;          /**<@brief  Target EOTF */
	uint16_t bitDepth;      /**<@brief  Bit Depth */
	uint16_t rangeSpec;     /**<@brief  Target Range */
	uint16_t diagSize;      /**<@brief  Diagonal Size */
	uint16_t maxPq;         /**<@brief  Target max luminnace in PQ */
	uint16_t minPq;         /**<@brief  Target min luminnace in PQ */
	uint16_t mSWeight;           /**<@brief  Multi-scale Weight */
	uint16_t mSEdgeWeight;       /**<@brief  Multi-scale Edge Weight */
	int16_t  minPQBias;          /**<@brief  Tone Mapping Min bias */
	int16_t  midPQBias;          /**<@brief  Tone Mapping Mid bias */
	int16_t  maxPQBias;          /**<@brief  Tone Mapping Max bias */
	int16_t  trimSlopeBias;      /**<@brief  Trim Slope bias */
	int16_t  trimOffsetBias;     /**<@brief  Trim Offset bias */
	int16_t  trimPowerBias;      /**<@brief  Trim Power bias */
	int16_t  msWeightBias;       /**<@brief  Multi-scale weight bias */
	int16_t  brightness;         /**<@brief  Brighness */
	int16_t  contrast;           /**<@brief  Contrast */
	int16_t  chromaWeightBias;   /**<@brief  Tone mapping chroma weight bias */
	int16_t  saturationGainBias; /**<@brief  Tone mapping saturation gain bias */
	uint16_t chromaWeight;       /**<@brief  Chroma Weight */
	uint16_t saturationGain;     /**<@brief  Saturation Gain */
	uint16_t crossTalk;          /**<@brief  Crosstalk */
	uint16_t tuningMode;         /**<@brief  Tuning Mode */
	int16_t  reserved0;
	int16_t  dbgExecParamsPrintPeriod;
	int16_t  dbgDmMdPrintPeriod;
	int16_t  dbgDmCfgPrintPeriod;
	uint16_t maxPq_dupli;
	uint16_t minPq_dupli;
	int32_t  keyWeight;
	int32_t  intensityVectorWeight; /**<@brief  Intensity vector weight */
	int32_t  chromaVectorWeight;    /**<@brief  Chroma vector weight */
	int16_t  chip_fpga_lowcomplex;  /**<@brief  Chip uses low complexity implementation */
	int16_t  midPQBiasLut[TUNING_LUT_ENTRIES];           /**<@brief  Mid PQ Bias Lut */
	int16_t  saturationGainBiasLut[TUNING_LUT_ENTRIES];  /**<@brief  Saturation Gain Bias Lut */
	int16_t  chromaWeightBiasLut[TUNING_LUT_ENTRIES];    /**<@brief  Chroma Weight Bias Lut */
	int16_t  slopeBiasLut[TUNING_LUT_ENTRIES];           /**<@brief  Slope Bias Lut */
	int16_t  offsetBiasLut[TUNING_LUT_ENTRIES];          /**<@brief  Offset Bias Lut */
	int16_t  backlightBiasLut[TUNING_LUT_ENTRIES];       /**<@brief  Backlight Bias Lut */
	struct TgtGDCfg gdConfig;       /**<@brief  Global Dimming configuration */
	int32_t  min_lin;          /**<@brief  Inverse Chroma vector weight */
	int32_t  max_lin;          /**<@brief  Target min luminnace in linear scale */
	int16_t  backlight_scaler; /**<@brief  Backlight Scaler */
	int32_t  min_lin_dupli;    /**<@brief  Duplicate of min_lin, only needed for 2.8.6 */
	int32_t  max_lin_dupli;    /**<@brief  Duplicate of max_lin, only needed for 2.8.6 */
	struct TgtOutCscCfg ocscConfig; /**<@brief  Output CSC configuration */
	int16_t  reserved00;
	int16_t  brightnessPreservation; /**<@brief  Brightness preservation */
	int32_t  iintensityVectorWeight; /**<@brief  Inverse intensity vector weight */
	int32_t  ichromaVectorWeight;    /**<@brief  Inverse Chroma vector weight */
	int16_t  isaturationGainBias;    /**<@brief  Inverse saturation gain bias */
	int16_t  chip_12b_ocsc;          /**<@brief  Chip uses 12 bit output CSC */
	int16_t  chip_512_tonecurve;     /**<@brief  Chip uses 512 entry tone curve */
	int16_t  chip_nolvl5;            /**<@brief  Chip doesn't support AOI */
	int16_t  padding[8];
};
#pragma pack(pop)

/*! @brief PQ config main data structure.
*/
struct pq_config_s {
	unsigned char default_gm_lut[GM_LUT_HDR_SIZE + GM_LUT_SIZE];
	unsigned char gd_gm_lut_min[GM_LUT_HDR_SIZE + GM_LUT_SIZE];
	unsigned char gd_gm_lut_max[GM_LUT_HDR_SIZE + GM_LUT_SIZE];
	unsigned char pq2gamma[sizeof(int32_t)*PQ2G_LUT_SIZE];
	unsigned char backlight_lut[BACLIGHT_LUT_SIZE];
	struct TargetDisplayConfig target_display_config;
};

/*!
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* TARGET_DISPLAY_CONFIG_H_ */
