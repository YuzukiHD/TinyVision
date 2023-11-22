/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 *
 * Allwinner SoCs hdmi2.0 driver.
 *
 * Copyright (C) 2016 Allwinner.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef _AW_HDMI_CORE_H_
#define _AW_HDMI_CORE_H_

#include <video/sunxi_display2.h>
#include <video/drv_hdmi.h>

#include "dw_hdmi/dw_i2cm.h"
#include "dw_hdmi/dw_mc.h"
#include "dw_hdmi/dw_video.h"
#include "dw_hdmi/dw_audio.h"
#include "dw_hdmi/dw_fc.h"
#include "dw_hdmi/dw_phy.h"
#include "dw_hdmi/dw_scdc.h"
#include "dw_hdmi/dw_edid.h"
#include "dw_hdmi/dw_hdcp.h"
#include "dw_hdmi/dw_access.h"
#include "dw_hdmi/dw_dev.h"
#include "dw_hdmi/dw_hdcp22_tx.h"
#include "dw_hdmi/dw_cec.h"
#include "dw_hdmi/phy_aw.h"
#include "dw_hdmi/phy_inno.h"
#include "dw_hdmi/phy_snps.h"

#define	DISP_CONFIG_UPDATE_NULL			0x0
#define	DISP_CONFIG_UPDATE_MODE		    0x1
#define	DISP_CONFIG_UPDATE_FORMAT		0x2
#define	DISP_CONFIG_UPDATE_BITS			0x4
#define	DISP_CONFIG_UPDATE_EOTF		    0x8
#define	DISP_CONFIG_UPDATE_CS			0x10
#define	DISP_CONFIG_UPDATE_DVI			0x20
#define	DISP_CONFIG_UPDATE_RANGE		0x40
#define	DISP_CONFIG_UPDATE_SCAN		    0x80
#define	DISP_CONFIG_UPDATE_RATIO		0x100

#define AW_CEC_BUF_SIZE 			    100
#define AW_CEC_MSG_SIZE 			    80

typedef enum {
	CEC_OPCODE_FEATURE_ABORT                 = 0x00,
	CEC_OPCODE_IMAGE_VIEW_ON                 = 0x04,
	CEC_OPCODE_TUNER_STEP_INCREMENT          = 0x05,
	CEC_OPCODE_TUNER_STEP_DECREMENT          = 0x06,
	CEC_OPCODE_TUNER_DEVICE_STATUS           = 0x07,
	CEC_OPCODE_GIVE_TUNER_DEVICE_STATUS      = 0x08,
	CEC_OPCODE_RECORD_ON                     = 0x09,
	CEC_OPCODE_RECORD_STATUS                 = 0x0A,
	CEC_OPCODE_RECORD_OFF                    = 0x0B,
	CEC_OPCODE_TEXT_VIEW_ON                  = 0x0D,
	CEC_OPCODE_RECORD_TV_SCREEN              = 0x0F,
	CEC_OPCODE_GIVE_DECK_STATUS              = 0x1A,
	CEC_OPCODE_DECK_STATUS                   = 0x1B,
	CEC_OPCODE_SET_MENU_LANGUAGE             = 0x32,
	CEC_OPCODE_CLEAR_ANALOGUE_TIMER          = 0x33,
	CEC_OPCODE_SET_ANALOGUE_TIMER            = 0x34,
	CEC_OPCODE_TIMER_STATUS                  = 0x35,
	CEC_OPCODE_STANDBY                       = 0x36,
	CEC_OPCODE_PLAY                          = 0x41,
	CEC_OPCODE_DECK_CONTROL                  = 0x42,
	CEC_OPCODE_TIMER_CLEARED_STATUS          = 0x43,
	CEC_OPCODE_USER_CONTROL_PRESSED          = 0x44,
	CEC_OPCODE_USER_CONTROL_RELEASE          = 0x45,
	CEC_OPCODE_GIVE_OSD_NAME                 = 0x46,
	CEC_OPCODE_SET_OSD_NAME                  = 0x47,
	CEC_OPCODE_SET_OSD_STRING                = 0x64,
	CEC_OPCODE_SET_TIMER_PROGRAM_TITLE       = 0x67,
	CEC_OPCODE_SYSTEM_AUDIO_MODE_REQUEST     = 0x70,
	CEC_OPCODE_GIVE_AUDIO_STATUS             = 0x71,
	CEC_OPCODE_SET_SYSTEM_AUDIO_MODE         = 0x72,
	CEC_OPCODE_GIVE_SYSTEM_AUDIO_MODE_STATUS = 0x7D,
	CEC_OPCODE_SYSTEM_AUDIO_MODE_STATUS      = 0x7E,
	CEC_OPCODE_ROUTING_CHANGE                = 0x80,
	CEC_OPCODE_ROUTING_INFORMATION           = 0x81,
	CEC_OPCODE_ACTIVE_SOURCE                 = 0x82,
	CEC_OPCODE_GIVE_PHYSICAL_ADDRESS         = 0x83,
	CEC_OPCODE_REPORT_PHYSICAL_ADDRESS       = 0x84,
	CEC_OPCODE_REQUEST_ACTIVE_SOURCE         = 0x85,
	CEC_OPCODE_SET_STREAM_PATH               = 0x86,
	CEC_OPCODE_DEVICE_VENDOR_ID              = 0x87,
	CEC_OPCODE_VENDOR_COMMAND                = 0x89,
	CEC_OPCODE_VENDOR_REMOTE_BUTTON_DOWN     = 0x8A,
	CEC_OPCODE_VENDOR_REMOTE_BUTTON_UP       = 0x8B,
	CEC_OPCODE_GIVE_DEVICE_VENDOR_ID         = 0x8C,
	CEC_OPCODE_MENU_REQUEST                  = 0x8D,
	CEC_OPCODE_MENU_STATUS                   = 0x8E,
	CEC_OPCODE_GIVE_DEVICE_POWER_STATUS      = 0x8F,
	CEC_OPCODE_REPORT_POWER_STATUS           = 0x90,
	CEC_OPCODE_GET_MENU_LANGUAGE             = 0x91,
	CEC_OPCODE_SET_DIGITAL_TIMER             = 0x97,
	CEC_OPCODE_CLEAR_DIGITAL_TIMER           = 0x99,
	CEC_OPCODE_SET_AUDIO_RATE                = 0x9A,
	CEC_OPCODE_INACTIVE_SOURCE               = 0x9D,
	CEC_OPCODE_CEC_VERSION                   = 0x9E,
	CEC_OPCODE_GET_CEC_VERSION               = 0x9F,
	CEC_OPCODE_VENDOR_COMMAND_WITH_ID        = 0xA0,
	CEC_OPCODE_CLEAR_EXTERNAL_TIMER          = 0xA1,
	CEC_OPCODE_SET_EXTERNAL_TIMER            = 0xA2,
	CEC_OPCODE_SELECT_ANALOGUE_SERVICE       = 0x92,
	CEC_OPCODE_SELECT_DIGITAL_SERVICE        = 0x93,
	CEC_OPCODE_ABORT                         = 0xFF,
} cec_opcode_t;

enum HDMI_VIC {
    HDMI_VIC_640x480P60 = 1,
    HDMI_VIC_720x480P60_4_3,
    HDMI_VIC_720x480P60_16_9,
    HDMI_VIC_1280x720P60,
    HDMI_VIC_1920x1080I60,
    HDMI_VIC_720x480I_4_3,
    HDMI_VIC_720x480I_16_9,
    HDMI_VIC_720x240P_4_3,
    HDMI_VIC_720x240P_16_9,
    HDMI_VIC_1920x1080P60 = 16,
    HDMI_VIC_720x576P_4_3,
    HDMI_VIC_720x576P_16_9,
    HDMI_VIC_1280x720P50,
    HDMI_VIC_1920x1080I50,
    HDMI_VIC_720x576I_4_3,
    HDMI_VIC_720x576I_16_9,
    HDMI_VIC_1920x1080P50 = 31,
    HDMI_VIC_1920x1080P24,
    HDMI_VIC_1920x1080P25,
    HDMI_VIC_1920x1080P30,
    HDMI_VIC_1280x720P24 = 60,
    HDMI_VIC_1280x720P25,
    HDMI_VIC_1280x720P30,
    HDMI_VIC_3840x2160P24 = 93,
    HDMI_VIC_3840x2160P25,
    HDMI_VIC_3840x2160P30,
    HDMI_VIC_3840x2160P50,
    HDMI_VIC_3840x2160P60,
    HDMI_VIC_4096x2160P24,
    HDMI_VIC_4096x2160P25,
    HDMI_VIC_4096x2160P30,
    HDMI_VIC_4096x2160P50,
    HDMI_VIC_4096x2160P60,

    HDMI_VIC_2560x1440P60 = 0x201,
    HDMI_VIC_1440x2560P70 = 0x202,
    HDMI_VIC_1080x1920P60 = 0x203,
};

#define HDMI1080P_24_3D_FP  (HDMI_VIC_1920x1080P24 + 0x80)
#define HDMI720P_50_3D_FP   (HDMI_VIC_1280x720P50 + 0x80)
#define HDMI720P_60_3D_FP   (HDMI_VIC_1280x720P60 + 0x80)

struct disp_hdmi_mode {
    enum disp_tv_mode mode;
    int  hdmi_mode; /* vic code */
};

struct aw_blacklist_edid {
    u8  mft_id[2]; /* EDID manufacture id */
    u8  stib[13];  /* EDID standard timing information blocks */
    u8  checksum;
};

struct aw_blacklist_issue {
    u32 tv_mode;
    u32 issue_type;
};

struct aw_sink_blacklist {
    struct aw_blacklist_edid  sink;
    struct aw_blacklist_issue issue[10];
};

struct hdmi_mode {
    dw_video_param_t      pVideo;
    dw_audio_param_t      pAudio;
    dw_hdcp_param_t       pHdcp;
    dw_product_param_t    pProduct;

    int               edid_done;
    struct edid	      *edid;
    u8		          *edid_ext; /* edid extenssion raw data */
    sink_edid_t	      *sink_cap;
};

#if IS_ENABLED(CONFIG_AW_HDMI2_CEC_SUNXI)
enum aw_cec_trans_type_e {
    AW_CEC_SEND_NON_BLOCK = 0,
    AW_CEC_SEND_BLOCK     = 1,
};



#if IS_ENABLED(CONFIG_AW_HDMI2_CEC_USER)
struct cec_msg {
    uint32_t len;       /* Length in bytes of the message */
    uint32_t timeout;   /* The timeout (in ms) for waiting for a reply */
    uint32_t sequence;  /* Seq num for reply tracing */
    uint32_t tx_status;
    uint32_t rx_status;
    uint8_t  msg[32];
};

struct cec_msg_tx {
    struct cec_msg msg;
    unsigned char trans_type;
    struct list_head list;
};

struct cec_msg_rx {
    struct cec_msg msg;
    struct list_head list;
};

struct cec_private {
    u32 i;
};
#else
enum aw_cec_power_status_e {
    AW_CEC_POWER_ON      = 0,
    AW_CEC_STANDBY       = 1,
    AW_CEC_STANDBY_TO_ON = 2,
    AW_CEC_ON_TO_STANDBY = 3,
};

enum cec_logical_addr_e {
    CEC_LA_TV_DEV     = 0,
    CEC_LA_RD_DEV_1   = 1,
    CEC_LA_RD_DEV_2   = 2,
    CEC_LA_TN_DEV_1   = 3,
    CEC_LA_PB_DEV_1   = 4,
    CEC_LA_AUDIO_SYS  = 5,
    CEC_LA_TN_DEV_2   = 6,
    CEC_LA_TN_DEV_3   = 7,
    CEC_LA_PB_DEV_2   = 8,
    CEC_LA_RD_DEV_3   = 9,
    CEC_LA_TN_DEV_4   = 10,
    CEC_LA_PB_DEV_3   = 11,
    CEC_LA_RESERVED_1 = 12,
    CEC_LA_RESERVED_2 = 13,
    CEC_LA_SPECIFIC   = 14,
    CEC_LA_BROADCAST  = 15,
};

#endif /* CONFIG_AW_HDMI2_CEC_USER */
#endif /* CONFIG_AW_HDMI2_CEC_SUNXI */

struct aw_device_ops {
    s32 (*dev_smooth_enable)(void);
    s32 (*dev_tv_mode_check)(u32 mode);

    int (*dev_get_blacklist_issue)(u32 mode);
    s32 (*dev_tv_mode_get)(void);

    int (*dev_config)(void);

    int (*dev_standby)(void);
    int (*dev_close)(void);
    void (*dev_resistor_calibration)(u32 reg, u32 data);

#ifndef SUPPORT_ONLY_HDMI14
    int (*scdc_read)(u8 address, u8 size, u8 *data);
    int (*scdc_write)(u8 address, u8 size, u8 *data);
#endif

    s32 (*dev_tv_mode_update_dtd)(u32 mode);
};

struct aw_video_ops {
    u32 (*get_color_space)(void);
    u32 (*get_color_depth)(void);
    s32 (*get_color_format)(void);
    u32 (*get_color_metry)(void);
    u8 (*get_color_range)(void);
    void (*set_tmds_mode)(u8 enable);
    u32 (*get_tmds_mode)(void);

    void (*set_avmute)(u8 enable);
    u32 (*get_avmute)(void);

    u32 (*get_scramble)(void);
    u32 (*get_pixel_repetion)(void);
    void (*set_drm_up)(dw_fc_drm_pb_t *pb);
    u32 (*get_vic_code)(void);
};

struct aw_audio_ops {
    u32 (*get_layout)(void);
    u32 (*get_channel_count)(void);
    u32 (*get_sample_freq)(void);
    u32 (*get_sample_size)(void);
    u32 (*get_acr_n)(void);
    int (*audio_config)(void);
};

struct aw_phy_ops {
    int (*phy_write)(u8 addr, u32 data);
    int (*phy_read)(u8 addr, u32 *value);
    u32 (*phy_get_rxsense)(void);
    u32 (*phy_get_pll_lock)(void);
    u32 (*phy_get_power)(void);
    void (*phy_set_power)(u8 enable);
    void (*phy_reset)(void);
    int (*phy_resume)(void);
    void (*set_hpd)(u8 enable);
    u8 (*get_hpd)(void);
};

struct aw_hdcp_ops {
    void (*hdcp_close)(void);
    void (*hdcp_configure)(void);
    void (*hdcp_disconfigure)(void);
    int (*hdcp_get_status)(void);
    u32 (*hdcp_get_avmute)(void);
    int (*hdcp_get_type)(void);
    ssize_t (*hdcp_config_dump)(char *buf);
};

struct aw_edid_ops {
    void (*main_release)(void);
    void (*main_read)(void);
    void (*set_prefered_video)(void);
    void (*correct_hw_config)(void);
    bool (*get_test_mode)(void);
    void (*set_test_mode)(bool en);
    void (*set_test_data)(const unsigned char *data, unsigned int size);
};

struct aw_cec_ops {
    s32 (*cec_receive)(unsigned char *msg, unsigned size);
    s32 (*cec_send)(unsigned char *msg, unsigned size);
    s32 (*cec_send_poll)(char src);
    int (*cec_enable)(void);
    int (*cec_disable)(void);
    unsigned char (*cec_get_la)(void);
    int (*cec_set_la)(unsigned char addr);
    u16 (*cec_get_pa)(void);
};

/**
 * aw hdmi core
 */
struct aw_hdmi_core_s {
    int   blacklist_sink;
    int   hdmi_tx_phy;
    uintptr_t reg_base;

    struct hdmi_mode    mode;
    struct disp_device_config   config;

    struct device_access  acs_ops;
    struct aw_device_ops dev_ops;
    struct aw_video_ops   video_ops;
    struct aw_audio_ops   audio_ops;
    struct aw_phy_ops     phy_ops;
    struct aw_edid_ops    edid_ops;
    struct aw_hdcp_ops    hdcp_ops;
    struct aw_cec_ops     cec_ops;

    dw_hdmi_dev_t      hdmi_tx;
};

int aw_hdmi_core_init(struct aw_hdmi_core_s *core, int phy, dw_hdcp_param_t *hdcp);

void aw_hdmi_core_exit(struct aw_hdmi_core_s *core);

extern unsigned int sunxi_get_soc_ver(void);
extern unsigned int sunxi_get_soc_markid(void);

#endif /* _AW_HDMI_CORE_H_ */
