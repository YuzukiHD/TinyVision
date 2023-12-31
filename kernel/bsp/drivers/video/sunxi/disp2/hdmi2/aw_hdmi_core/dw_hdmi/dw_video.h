/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * Allwinner SoCs hdmi2.0 driver.
 *
 * Copyright (C) 2016 Allwinner.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#ifndef _DW_VIDEO_H_
#define _DW_VIDEO_H_

#include "dw_dev.h"
#include "dw_edid.h"

/*****************************************************************************
 *                                                                           *
 *                          Video Sampler Registers                 *
 *                                                                           *
 *****************************************************************************/

/* Video Input Mapping and Internal Data Enable Configuration Register */
#define TX_INVID0  0x00000800
#define TX_INVID0_VIDEO_MAPPING_MASK  0x0000001F /* Video Input mapping (color space/color depth): 0x01: RGB 4:4:4/8 bits 0x03: RGB 4:4:4/10 bits 0x05: RGB 4:4:4/12 bits 0x07: RGB 4:4:4/16 bits 0x09: YCbCr 4:4:4 or 4:2:0/8 bits 0x0B: YCbCr 4:4:4 or 4:2:0/10 bits 0x0D: YCbCr 4:4:4 or 4:2:0/12 bits 0x0F: YCbCr 4:4:4 or 4:2:0/16 bits 0x16: YCbCr 4:2:2/8 bits 0x14: YCbCr 4:2:2/10 bits 0x12: YCbCr 4:2:2/12 bits 0x17: YCbCr 4:4:4 (IPI)/8 bits 0x18: YCbCr 4:4:4 (IPI)/10 bits 0x19: YCbCr 4:4:4 (IPI)/12 bits 0x1A: YCbCr 4:4:4 (IPI)/16 bits 0x1B: YCbCr 4:2:2 (IPI)/12 bits 0x1C: YCbCr 4:2:0 (IPI)/8 bits 0x1D: YCbCr 4:2:0 (IPI)/10 bits 0x1E: YCbCr 4:2:0 (IPI)/12 bits 0x1F: YCbCr 4:2:0 (IPI)/16 bits */
#define TX_INVID0_INTERNAL_DE_GENERATOR_MASK  0x00000080 /* Internal data enable (DE) generator enable */

/* Video Input Stuffing Enable Register */
#define TX_INSTUFFING  0x00000804
#define TX_INSTUFFING_GYDATA_STUFFING_MASK  0x00000001 /* - 0b: When the dataen signal is low, the value in the gydata[15:0] output is the one sampled from the corresponding input data */
#define TX_INSTUFFING_RCRDATA_STUFFING_MASK  0x00000002 /* - 0b: When the dataen signal is low, the value in the rcrdata[15:0] output is the one sampled from the corresponding input data */
#define TX_INSTUFFING_BCBDATA_STUFFING_MASK  0x00000004 /* - 0b: When the dataen signal is low, the value in the bcbdata[15:0] output is the one sampled from the corresponding input data */

/* Video Input gy Data Channel Stuffing Register 0 */
#define TX_GYDATA0  0x00000808
#define TX_GYDATA0_GYDATA_MASK  0x000000FF /* This register defines the value of gydata[7:0] when TX_INSTUFFING[0] (gydata_stuffing) is set to 1b */

/* Video Input gy Data Channel Stuffing Register 1 */
#define TX_GYDATA1  0x0000080C
#define TX_GYDATA1_GYDATA_MASK  0x000000FF /* This register defines the value of gydata[15:8] when TX_INSTUFFING[0] (gydata_stuffing) is set to 1b */

/* Video Input rcr Data Channel Stuffing Register 0 */
#define TX_RCRDATA0  0x00000810
#define TX_RCRDATA0_RCRDATA_MASK  0x000000FF /* This register defines the value of rcrydata[7:0] when TX_INSTUFFING[1] (rcrdata_stuffing) is set to 1b */

/* Video Input rcr Data Channel Stuffing Register 1 */
#define TX_RCRDATA1  0x00000814
#define TX_RCRDATA1_RCRDATA_MASK  0x000000FF /* This register defines the value of rcrydata[15:8] when TX_INSTUFFING[1] (rcrdata_stuffing) is set to 1b */

/* Video Input bcb Data Channel Stuffing Register 0 */
#define TX_BCBDATA0  0x00000818
#define TX_BCBDATA0_BCBDATA_MASK  0x000000FF /* This register defines the value of bcbdata[7:0] when TX_INSTUFFING[2] (bcbdata_stuffing) is set to 1b */

/* Video Input bcb Data Channel Stuffing Register 1 */
#define TX_BCBDATA1  0x0000081C
#define TX_BCBDATA1_BCBDATA_MASK  0x000000FF /* This register defines the value of bcbdata[15:8] when TX_INSTUFFING[2] (bcbdata_stuffing) is set to 1b */



/*****************************************************************************
 *                                                                           *
 *                         Video Packetizer Registers                        *
 *                                                                           *
 *****************************************************************************/

/* Video Packetizer Packing Phase Status Register */
#define VP_STATUS  0x00002000
#define VP_STATUS_PACKING_PHASE_MASK  0x0000000F /* Read only register that holds the "packing phase" output of the Video Packetizer block */

/* Video Packetizer Pixel Repetition and Color Depth Register */
#define VP_PR_CD  0x00002004
#define VP_PR_CD_DESIRED_PR_FACTOR_MASK  0x0000000F /* Desired pixel repetition factor configuration */
#define VP_PR_CD_COLOR_DEPTH_MASK  0x000000F0 /* The Color depth configuration is described as the following, with the action stated corresponding to color_depth[3:0]: - 0000b: 24 bits per pixel video (8 bits per component) */

/* Video Packetizer Stuffing and Default Packing Phase Register */
#define VP_STUFF  0x00002008
#define VP_STUFF_PR_STUFFING_MASK  0x00000001 /* Pixel repeater stuffing control */
#define VP_STUFF_PP_STUFFING_MASK  0x00000002 /* Pixel packing stuffing control */
#define VP_STUFF_YCC422_STUFFING_MASK  0x00000004 /* YCC 422 remap stuffing control */
#define VP_STUFF_ICX_GOTO_P0_ST_MASK  0x00000008 /* Reserved */
#define VP_STUFF_IFIX_PP_TO_LAST_MASK  0x00000010 /* Reserved */
#define VP_STUFF_IDEFAULT_PHASE_MASK  0x00000020 /* Controls the default phase packing machine used according to HDMI 1 */

/* Video Packetizer YCC422 Remapping Register */
#define VP_REMAP  0x0000200C
#define VP_REMAP_YCC422_SIZE_MASK  0x00000003 /* YCC 422 remap input video size ycc422_size[1:0] 00b: YCC 422 16-bit input video (8 bits per component) 01b: YCC 422 20-bit input video (10 bits per component) 10b: YCC 422 24-bit input video (12 bits per component) 11b: Reserved */

/* Video Packetizer Output, Bypass and Enable Configuration Register */
#define VP_CONF  0x00002010
#define VP_CONF_OUTPUT_SELECTOR_MASK  0x00000003 /* Video Packetizer output selection output_selector[1:0] 00b: Data from pixel packing block 01b: Data from YCC 422 remap block 10b: Data from 8-bit bypass block 11b: Data from 8-bit bypass block */
#define VP_CONF_BYPASS_SELECT_MASK  0x00000004 /* bypass_select 0b: Data from pixel repeater block 1b: Data from input of Video Packetizer block */
#define VP_CONF_YCC422_EN_MASK  0x00000008 /* YCC 422 select enable */
#define VP_CONF_PR_EN_MASK  0x00000010 /* Pixel repeater enable */
#define VP_CONF_PP_EN_MASK  0x00000020 /* Pixel packing enable */
#define VP_CONF_BYPASS_EN_MASK  0x00000040 /* Bypass enable */

/* Video Packetizer Interrupt Mask Register */
#define VP_MASK  0x0000201C
#define VP_MASK_OINTEMPTYBYP_MASK  0x00000001 /* Mask bit for Video Packetizer 8-bit bypass FIFO empty */
#define VP_MASK_OINTFULLBYP_MASK  0x00000002 /* Mask bit for Video Packetizer 8-bit bypass FIFO full */
#define VP_MASK_OINTEMPTYREMAP_MASK  0x00000004 /* Mask bit for Video Packetizer pixel YCC 422 re-mapper FIFO empty */
#define VP_MASK_OINTFULLREMAP_MASK  0x00000008 /* Mask bit for Video Packetizer pixel YCC 422 re-mapper FIFO full */
#define VP_MASK_OINTEMPTYPP_MASK  0x00000010 /* Mask bit for Video Packetizer pixel packing FIFO empty */
#define VP_MASK_OINTFULLPP_MASK  0x00000020 /* Mask bit for Video Packetizer pixel packing FIFO full */
#define VP_MASK_OINTEMPTYREPET_MASK  0x00000040 /* Mask bit for Video Packetizer pixel repeater FIFO empty */
#define VP_MASK_OINTFULLREPET_MASK  0x00000080 /* Mask bit for Video Packetizer pixel repeater FIFO full */

/*****************************************************************************
 *                                                                           *
 *                      Color Space Converter Registers                      *
 *                                                                           *
 *****************************************************************************/

/* Color Space Converter Interpolation and Decimation Configuration Register */
#define CSC_CFG  0x00010400
#define CSC_CFG_DECMODE_MASK  0x00000003 /* Chroma decimation configuration: decmode[1:0] | Chroma Decimation 00 | decimation disabled 01 | Hd (z) =1 10 | Hd(z)=1/ 4 + 1/2z^(-1 )+1/4 z^(-2) 11 | Hd(z)x2^(11)= -5+12z^(-2) - 22z^(-4)+39z^(-8) +109z^(-10) -204z^(-12)+648z^(-14) + 1024z^(-15) +648z^(-16) -204z^(-18) +109z^(-20)- 65z^(-22) +39z^(-24) -22z^(-26) +12z^(-28)-5z^(-30) */
#define CSC_CFG_SPARE_1_MASK  0x0000000C /* This is a spare register with no associated functionality */
#define CSC_CFG_INTMODE_MASK  0x00000030 /* Chroma interpolation configuration: intmode[1:0] | Chroma Interpolation 00 | interpolation disabled 01 | Hu (z) =1 + z^(-1) 10 | Hu(z)=1/ 2 + z^(-11)+1/2 z^(-2) 11 | interpolation disabled */
#define CSC_CFG_SPARE_2_MASK  0x00000040 /* This is a spare register with no associated functionality */
#define CSC_CFG_CSC_LIMIT_MASK  0x00000080 /* When set (1'b1), the range limitation values defined in registers csc_mat_uplim and csc_mat_dnlim are applied to the output of the Color Space Conversion matrix */

/* Color Space Converter Scale and Deep Color Configuration Register */
#define CSC_SCALE  0x00010404
#define CSC_SCALE_CSCSCALE_MASK  0x00000003 /* Defines the cscscale[1:0] scale factor to apply to all coefficients in Color Space Conversion */
#define CSC_SCALE_SPARE_MASK  0x0000000C /* The is a spare register with no associated functionality */
#define CSC_SCALE_CSC_COLOR_DEPTH_MASK  0x000000F0 /* Color space converter color depth configuration: csc_colordepth[3:0] | Action 0000 | 24 bit per pixel video (8 bit per component) */

/* Color Space Converter Matrix A1 Coefficient Register MSB Notes: - The coefficients used in the CSC matrix use only 15 bits for the internal computations */
#define CSC_COEF_A1_MSB  0x00010408
#define CSC_COEF_A1_MSB_CSC_COEF_A1_MSB_MASK  0x000000FF /* Color Space Converter Matrix A1 Coefficient Register MSB */

/* Color Space Converter Matrix A1 Coefficient Register LSB Notes: - The coefficients used in the CSC matrix use only 15 bits for the internal computations */
#define CSC_COEF_A1_LSB  0x0001040C
#define CSC_COEF_A1_LSB_CSC_COEF_A1_LSB_MASK  0x000000FF /* Color Space Converter Matrix A1 Coefficient Register LSB */

/* Color Space Converter Matrix A2 Coefficient Register MSB Color Space Conversion A2 coefficient */
#define CSC_COEF_A2_MSB  0x00010410
#define CSC_COEF_A2_MSB_CSC_COEF_A2_MSB_MASK  0x000000FF /* Color Space Converter Matrix A2 Coefficient Register MSB */

/* Color Space Converter Matrix A2 Coefficient Register LSB Color Space Conversion A2 coefficient */
#define CSC_COEF_A2_LSB  0x00010414
#define CSC_COEF_A2_LSB_CSC_COEF_A2_LSB_MASK  0x000000FF /* Color Space Converter Matrix A2 Coefficient Register LSB */

/* Color Space Converter Matrix A3 Coefficient Register MSB Color Space Conversion A3 coefficient */
#define CSC_COEF_A3_MSB  0x00010418
#define CSC_COEF_A3_MSB_CSC_COEF_A3_MSB_MASK  0x000000FF /* Color Space Converter Matrix A3 Coefficient Register MSB */

/* Color Space Converter Matrix A3 Coefficient Register LSB Color Space Conversion A3 coefficient */
#define CSC_COEF_A3_LSB  0x0001041C
#define CSC_COEF_A3_LSB_CSC_COEF_A3_LSB_MASK  0x000000FF /* Color Space Converter Matrix A3 Coefficient Register LSB */

/* Color Space Converter Matrix A4 Coefficient Register MSB Color Space Conversion A4 coefficient */
#define CSC_COEF_A4_MSB  0x00010420
#define CSC_COEF_A4_MSB_CSC_COEF_A4_MSB_MASK  0x000000FF /* Color Space Converter Matrix A4 Coefficient Register MSB */

/* Color Space Converter Matrix A4 Coefficient Register LSB Color Space Conversion A4 coefficient */
#define CSC_COEF_A4_LSB  0x00010424
#define CSC_COEF_A4_LSB_CSC_COEF_A4_LSB_MASK  0x000000FF /* Color Space Converter Matrix A4 Coefficient Register LSB */

/* Color Space Converter Matrix B1 Coefficient Register MSB Color Space Conversion B1 coefficient */
#define CSC_COEF_B1_MSB  0x00010428
#define CSC_COEF_B1_MSB_CSC_COEF_B1_MSB_MASK  0x000000FF /* Color Space Converter Matrix B1 Coefficient Register MSB */

/* Color Space Converter Matrix B1 Coefficient Register LSB Color Space Conversion B1 coefficient */
#define CSC_COEF_B1_LSB  0x0001042C
#define CSC_COEF_B1_LSB_CSC_COEF_B1_LSB_MASK  0x000000FF /* Color Space Converter Matrix B1 Coefficient Register LSB */

/* Color Space Converter Matrix B2 Coefficient Register MSB Color Space Conversion B2 coefficient */
#define CSC_COEF_B2_MSB  0x00010430
#define CSC_COEF_B2_MSB_CSC_COEF_B2_MSB_MASK  0x000000FF /* Color Space Converter Matrix B2 Coefficient Register MSB */

/* Color Space Converter Matrix B2 Coefficient Register LSB Color Space Conversion B2 coefficient */
#define CSC_COEF_B2_LSB  0x00010434
#define CSC_COEF_B2_LSB_CSC_COEF_B2_LSB_MASK  0x000000FF /* Color Space Converter Matrix B2 Coefficient Register LSB */

/* Color Space Converter Matrix B3 Coefficient Register MSB Color Space Conversion B3 coefficient */
#define CSC_COEF_B3_MSB  0x00010438
#define CSC_COEF_B3_MSB_CSC_COEF_B3_MSB_MASK  0x000000FF /* Color Space Converter Matrix B3 Coefficient Register MSB */

/* Color Space Converter Matrix B3 Coefficient Register LSB Color Space Conversion B3 coefficient */
#define CSC_COEF_B3_LSB  0x0001043C
#define CSC_COEF_B3_LSB_CSC_COEF_B3_LSB_MASK  0x000000FF /* Color Space Converter Matrix B3 Coefficient Register LSB */

/* Color Space Converter Matrix B4 Coefficient Register MSB Color Space Conversion B4 coefficient */
#define CSC_COEF_B4_MSB  0x00010440
#define CSC_COEF_B4_MSB_CSC_COEF_B4_MSB_MASK  0x000000FF /* Color Space Converter Matrix B4 Coefficient Register MSB */

/* Color Space Converter Matrix B4 Coefficient Register LSB Color Space Conversion B4 coefficient */
#define CSC_COEF_B4_LSB  0x00010444
#define CSC_COEF_B4_LSB_CSC_COEF_B4_LSB_MASK  0x000000FF /* Color Space Converter Matrix B4 Coefficient Register LSB */

/* Color Space Converter Matrix C1 Coefficient Register MSB Color Space Conversion C1 coefficient */
#define CSC_COEF_C1_MSB  0x00010448
#define CSC_COEF_C1_MSB_CSC_COEF_C1_MSB_MASK  0x000000FF /* Color Space Converter Matrix C1 Coefficient Register MSB */

/* Color Space Converter Matrix C1 Coefficient Register LSB Color Space Conversion C1 coefficient */
#define CSC_COEF_C1_LSB  0x0001044C
#define CSC_COEF_C1_LSB_CSC_COEF_C1_LSB_MASK  0x000000FF /* Color Space Converter Matrix C1 Coefficient Register LSB */

/* Color Space Converter Matrix C2 Coefficient Register MSB Color Space Conversion C2 coefficient */
#define CSC_COEF_C2_MSB  0x00010450
#define CSC_COEF_C2_MSB_CSC_COEF_C2_MSB_MASK  0x000000FF /* Color Space Converter Matrix C2 Coefficient Register MSB */

/* Color Space Converter Matrix C2 Coefficient Register LSB Color Space Conversion C2 coefficient */
#define CSC_COEF_C2_LSB  0x00010454
#define CSC_COEF_C2_LSB_CSC_COEF_C2_LSB_MASK  0x000000FF /* Color Space Converter Matrix C2 Coefficient Register LSB */

/* Color Space Converter Matrix C3 Coefficient Register MSB Color Space Conversion C3 coefficient */
#define CSC_COEF_C3_MSB  0x00010458
#define CSC_COEF_C3_MSB_CSC_COEF_C3_MSB_MASK  0x000000FF /* Color Space Converter Matrix C3 Coefficient Register MSB */

/* Color Space Converter Matrix C3 Coefficient Register LSB Color Space Conversion C3 coefficient */
#define CSC_COEF_C3_LSB  0x0001045C
#define CSC_COEF_C3_LSB_CSC_COEF_C3_LSB_MASK  0x000000FF /* Color Space Converter Matrix C3 Coefficient Register LSB */

/* Color Space Converter Matrix C4 Coefficient Register MSB Color Space Conversion C4 coefficient */
#define CSC_COEF_C4_MSB  0x00010460
#define CSC_COEF_C4_MSB_CSC_COEF_C4_MSB_MASK  0x000000FF /* Color Space Converter Matrix C4 Coefficient Register MSB */

/* Color Space Converter Matrix C4 Coefficient Register LSB Color Space Conversion C4 coefficient */
#define CSC_COEF_C4_LSB  0x00010464
#define CSC_COEF_C4_LSB_CSC_COEF_C4_LSB_MASK  0x000000FF /* Color Space Converter Matrix C4 Coefficient Register LSB */

/* Color Space Converter Matrix Output Up Limit Register MSB For more details, refer to the HDMI 1 */
#define CSC_LIMIT_UP_MSB  0x00010468
#define CSC_LIMIT_UP_MSB_CSC_LIMIT_UP_MSB_MASK  0x000000FF /* Color Space Converter Matrix Output Upper Limit Register MSB */

/* Color Space Converter Matrix output Up Limit Register LSB For more details, refer to the HDMI 1 */
#define CSC_LIMIT_UP_LSB  0x0001046C
#define CSC_LIMIT_UP_LSB_CSC_LIMIT_UP_LSB_MASK  0x000000FF /* Color Space Converter Matrix Output Upper Limit Register LSB */

/* Color Space Converter Matrix output Down Limit Register MSB For more details, refer to the HDMI 1 */
#define CSC_LIMIT_DN_MSB  0x00010470
#define CSC_LIMIT_DN_MSB_CSC_LIMIT_DN_MSB_MASK  0x000000FF /* Color Space Converter Matrix output Down Limit Register MSB */

/* Color Space Converter Matrix output Down Limit Register LSB For more details, refer to the HDMI 1 */
#define CSC_LIMIT_DN_LSB  0x00010474
#define CSC_LIMIT_DN_LSB_CSC_LIMIT_DN_LSB_MASK  0x000000FF /* Color Space Converter Matrix Output Down Limit Register LSB */


int dw_vp_get_cea_vic(int hdmi_vic_code);

void dw_vp_set_support_ycc420(dw_dtd_t *paramsDtd, dw_edid_block_svd_t *paramsSvd);

int dw_vp_get_hdmi_vic(int cea_code);

/**
 * This methotaEnablePolarity
 * to put the state of the strucutre to default
 * @param params pointer to the video parameters structure
 */
void dw_vp_param_reset(dw_hdmi_dev_t *dev, dw_video_param_t *params);


/**
 * @param params pointer to the video parameters structure
 * @return Video PixelClock in [0.01 MHz]
 */
u32 dw_vp_get_pixel_clk(dw_hdmi_dev_t *dev, dw_video_param_t *params);

/**
 * @param params pointer to the video parameters structure
 * @return TMDS Clock in [0.01 MHz]
 */
u16 videoParams_GetTmdsClock(dw_hdmi_dev_t *dev, dw_video_param_t *params);

/**
 * @param params pointer to the video parameters structure
 * @return Ration clock x 100 (dw_hdmi_dev_t *dev,
 * should be multiplied by x 0.01 afterwards)
 */
unsigned dw_vp_get_ratio_clk(dw_hdmi_dev_t *dev, dw_video_param_t *params);

void dw_vp_update_csc_coefficients(dw_hdmi_dev_t *dev, dw_video_param_t *params);

u8 videoParams_IsLimitedToYcc420(dw_hdmi_dev_t *dev, dw_video_param_t *params);

char *dw_vp_get_color_format_string(dw_color_format_t encoding);


/**
 * Configures the video blocks to do any video processing and to
 * transmit the video set up required by the user, allowing to
 * force video pixels (from the DEBUG pixels) to be transmitted
 * rather than the video stream being received.
 * @param baseAddr Base Address of module
 * @param params VideoParams
 * @return true if successful
 */
int dw_video_config(dw_hdmi_dev_t *dev, dw_video_param_t *params);

/**
 * A test only method that is used for a test module
 * @param baseAddr Base Address of module
 * @param params VideoParams
 * @param dataEnablePolarity
 * @return true if successful
 */
int video_VideoGenerator(dw_hdmi_dev_t *dev, dw_video_param_t *params,
			 u8 dataEnablePolarity);

u8 dw_vp_get_pixel_repet_factor(dw_hdmi_dev_t *dev);
u8 dw_vp_get_color_depth(dw_hdmi_dev_t *dev);

#ifndef SUPPORT_ONLY_HDMI14
void dw_video_set_scramble(dw_hdmi_dev_t *dev, u8 enable);
#endif

#endif	/* _DW_VIDEO_H_ */
