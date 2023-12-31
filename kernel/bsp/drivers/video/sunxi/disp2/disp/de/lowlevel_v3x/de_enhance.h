/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * Allwinner SoCs display driver.
 *
 * Copyright (C) 2016 Allwinner.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

/******************************************************************************
 *  All Winner Tech, All Right Reserved. 2014-2015 Copyright (c)
 *
 *  File name   :   de_enhance.h
 *
 *  Description :   display engine 2.0 enhance basic function declaration
 *
 *  History     :   2014/04/02  vito cheng  v0.1  Initial version
 *                  2014/04/29  vito cheng  v0.2  Add disp_enhance_config_data
 *                                                struct delcaration
 ******************************************************************************/
#ifndef __DE_ENHANCE_H__
#define __DE_ENHANCE_H__

#include "../include.h"
#include "de_fce_type.h"
#include "de_peak_type.h"
#include "de_peak2d_type.h"
#include "de_lti_type.h"
#include "de_dns_type.h"
#include "de_fcc_type.h"
#include "de_bls_type.h"

#define MODE_NUM 4
#define PARA_NUM 6
#define FORMAT_NUM 2
#define ENHANCE_MODE_NUM 3

#define ENHANCE_MIN_OVL_WIDTH  32
#define ENHANCE_MIN_OVL_HEIGHT 32
#define ENHANCE_MIN_BLD_WIDTH 32
#define ENHANCE_MIN_BLD_HEIGHT 4

#define FCE_OFST  0x70000	/* FCE offset based on RTMX */
#define PEAK_OFST 0x70800	/* PEAKING offset based on RTMX */
#define LTI_OFST  0x71000	/* LTI offset based on RTMX */
#define BLS_OFST  0x71800	/* BLS offset based on RTMX */
#define FCC_OFST  0x72000	/* FCC offset based on RTMX */
#define DNS_OFST  0x80000	/* DNS offset based on RTMX */
#define PEAK2D_OFST  0x30	/* PEAK2D offset based on VSU */

/* vep bypass mask */
#define USER_BYPASS 0x00000001
#define USER_BYPASS_MASK 0xfffffffe
#define SIZE_BYPASS 0x00000002
#define SIZE_BYPASS_MASK 0xfffffffd
#define LAYER_BYPASS 0x00000004
#define LAYER_BYPASS_MASK 0xfffffffb

#define DE_CLIP(x, low, high) max(low, min(x, high))

struct disp_enhance_layer_info {
	/* layer framebuffer size width/height */
	struct disp_rectsz fb_size;
	/* layer framebuffer crop retangle, don't care w/h at all */
	struct disp_rect fb_crop;
	/* layer enable */
	unsigned int en;
	/* layer format */
	unsigned int format;
};

struct disp_enhance_chn_info {
	/* overlay size (scale in size) */
	/* CAUTION: overlay size can be the output size of */
	/* video overlay/ de-interlace overlay/ FBD overlay */
	struct disp_rectsz ovl_size;
	/* bld size (scale out size) */
	struct disp_rectsz bld_size;
	/* layer info */
	struct disp_enhance_layer_info layer_info[LAYER_MAX_NUM_PER_CHN];
};

/* parameters of vep module */
struct vep_para {
	enum disp_manager_dirty_flags flags;
	struct __fce_config_data fce_para;
	struct __lti_config_data lti_para;
	struct __peak_config_data peak_para;
	struct __bls_config_data bls_para;
	struct __fcc_config_data fcc_para;
	struct __dns_config_data dns_para;
	struct __peak2d_config_data peak2d_para;
	unsigned int demo_enable;
	unsigned int bypass;
	unsigned int dev_type;
	unsigned int fmt;
};

/* system configuration of vep */
struct vep_config_info {
	unsigned int device_num;
	unsigned int vep_num[DE_NUM];
	unsigned int dns_exist[DE_NUM][VI_CHN_NUM];
	unsigned int peak2d_exist[DE_NUM][VI_CHN_NUM];
};

/* algorithm variable */
struct vep_alg_var {
	unsigned int frame_cnt;
};

/* peak function declaration */
int de_peak_update_regs(unsigned int sel, unsigned int chno);
int de_peak_init(unsigned int sel, unsigned int chno, uintptr_t reg_base);
int de_peak_double_init(unsigned int sel, unsigned int chno,
			uintptr_t reg_base);
int de_peak_exit(unsigned int sel, unsigned int chno);
int de_peak_double_exit(unsigned int sel, unsigned int chno);
int de_peak_enable(unsigned int sel, unsigned int chno, unsigned int en);
int de_peak_set_size(unsigned int sel, unsigned int chno, unsigned int width,
			   unsigned int height);
int de_peak_set_window(unsigned int sel, unsigned int chno,
			     unsigned int win_enable, struct de_rect window);
int de_peak_init_para(unsigned int sel, unsigned int chno);
int de_peak_info2para(unsigned int sel, unsigned int chno,
		      unsigned int fmt, unsigned int dev_type,
		      struct __peak_config_data *para, unsigned int bypass);

/* /LTI function declaration */
int de_lti_update_regs(unsigned int sel, unsigned int chno);
int de_lti_init(unsigned int sel, unsigned int chno, uintptr_t reg_base);
int de_lti_double_init(unsigned int sel, unsigned int chno, uintptr_t reg_base);
int de_lti_exit(unsigned int sel, unsigned int chno);
int de_lti_double_exit(unsigned int sel, unsigned int chno);
int de_lti_enable(unsigned int sel, unsigned int chno, unsigned int en);
int de_lti_set_size(unsigned int sel, unsigned int chno, unsigned int width,
			  unsigned int height);
int de_lti_set_window(unsigned int sel, unsigned int chno,
			    unsigned int win_enable, struct de_rect window);
int de_lti_init_para(unsigned int sel, unsigned int chno);
int de_lti_info2para(unsigned int sel, unsigned int chno,
		     unsigned int fmt, unsigned int dev_type,
		     struct __lti_config_data *para, unsigned int bypass);

/* FCE function declaration */
int de_fce_update_regs(unsigned int sel, unsigned int chno);
int de_fce_init(unsigned int sel, unsigned int chno, uintptr_t reg_base);
int de_fce_double_init(unsigned int sel, unsigned int chno, uintptr_t reg_base);
int de_fce_exit(unsigned int sel, unsigned int chno);
int de_fce_double_exit(unsigned int sel, unsigned int chno);
int de_fce_enable(unsigned int sel, unsigned int chno, unsigned int en);
int de_fce_set_size(unsigned int sel, unsigned int chno, unsigned int width,
		    unsigned int height);
int de_fce_set_window(unsigned int sel, unsigned int chno,
		      unsigned int win_enable, struct de_rect window);
int de_fce_init_para(unsigned int sel, unsigned int chno);
int de_fce_info2para(unsigned int sel, unsigned int chno,
		     unsigned int fmt, unsigned int dev_type,
		     struct __fce_config_data *para, unsigned int bypass);
int de_hist_tasklet(unsigned int screen_id, unsigned int chno,
		    unsigned int frame_cnt);
int de_ce_tasklet(unsigned int screen_id, unsigned int chno,
		  unsigned int frame_cnt);

/* BLS function declaration */
int de_bls_update_regs(unsigned int sel, unsigned int chno);
int de_bls_init(unsigned int sel, unsigned int chno, uintptr_t reg_base);
int de_bls_double_init(unsigned int sel, unsigned int chno, uintptr_t reg_base);
int de_bls_enable(unsigned int sel, unsigned int chno, unsigned int en);
int de_bls_exit(unsigned int sel, unsigned int chno);
int de_bls_double_exit(unsigned int sel, unsigned int chno);
int de_bls_set_size(unsigned int sel, unsigned int chno, unsigned int width,
		    unsigned int height);
int de_bls_set_window(unsigned int sel, unsigned int chno,
		      unsigned int win_enable, struct de_rect window);
int de_bls_init_para(unsigned int sel, unsigned int chno);
int de_bls_info2para(unsigned int sel, unsigned int chno,
		     unsigned int fmt, unsigned int dev_type,
		     struct __bls_config_data *para, unsigned int bypass);

/* FCC function declaration */
int de_fcc_update_regs(unsigned int sel, unsigned int chno);
int de_fcc_init(unsigned int sel, unsigned int chno, uintptr_t reg_base);
int de_fcc_double_init(unsigned int sel, unsigned int chno, uintptr_t reg_base);
int de_fcc_exit(unsigned int sel, unsigned int chno);
int de_fcc_double_exit(unsigned int sel, unsigned int chno);
int de_fcc_enable(unsigned int sel, unsigned int chno, unsigned int en);
int de_fcc_set_size(unsigned int sel, unsigned int chno, unsigned int width,
			  unsigned int height);
int de_fcc_set_window(unsigned int sel, unsigned int chno, unsigned int win_en,
		      struct de_rect window);
int de_fcc_init_para(unsigned int sel, unsigned int chno);
int de_fcc_info2para(unsigned int sel, unsigned int chno, unsigned int fmt,
		     unsigned int dev_type, struct __fcc_config_data *para,
		     unsigned int bypass);

/* PEAK2D function declaration */
int de_peak2d_update_regs(unsigned int sel, unsigned int chno);
int de_peak2d_init(unsigned int sel, unsigned int chno, uintptr_t reg_base);
int de_peak2d_double_init(unsigned int sel, unsigned int chno,
			  uintptr_t reg_base);
int de_peak2d_exit(unsigned int sel, unsigned int chno);
int de_peak2d_double_exit(unsigned int sel, unsigned int chno);
int de_peak2d_enable(unsigned int sel, unsigned int chno, unsigned int en);
int de_peak2d_init_para(unsigned int sel, unsigned int chno);
int de_peak2d_info2para(unsigned int sel, unsigned int chno,
			unsigned int fmt, unsigned int dev_type,
			struct __peak2d_config_data *para, unsigned int bypass);

/* DNS function declaration */
int de_dns_update_regs(unsigned int sel, unsigned int chno);
int de_dns_init(unsigned int sel, unsigned int chno, uintptr_t reg_base);
int de_dns_double_init(unsigned int sel, unsigned int chno, uintptr_t reg_base);
int de_dns_exit(unsigned int sel, unsigned int chno);
int de_dns_double_exit(unsigned int sel, unsigned int chno);
int de_dns_enable(unsigned int sel, unsigned int chno, unsigned int en);
int de_dns_set_size(unsigned int sel, unsigned int chno, unsigned int width,
			  unsigned int height);
int de_dns_set_window(unsigned int sel, unsigned int chno,
			    unsigned int win_enable, struct de_rect window);
int de_dns_init_para(unsigned int sel, unsigned int chno);
int de_dns_info2para(unsigned int sel, unsigned int chno,
			   unsigned int fmt, unsigned int dev_type,
			   struct __dns_config_data *para, unsigned int bypass);
int de_dns_tasklet(unsigned int screen_id, unsigned int chno,
			unsigned int frame_cnt);

/* ENHANCE function declaration */
int de_enhance_update_regs(unsigned int screen_id);
int de_enhance_init(struct disp_bsp_init_para *para);
int de_enhance_double_init(struct disp_bsp_init_para *para);
int de_enhance_exit(void);
int de_enhance_double_exit(void);
int de_enhance_layer_apply(unsigned int screen_id,
			   struct disp_enhance_chn_info *info);
int de_enhance_apply(unsigned int screen_id,
			   struct disp_enhance_config *config);
int de_enhance_sync(unsigned int screen_id);
int de_enhance_tasklet(unsigned int screen_id);

/* module public function declaration */
void de_set_bits(unsigned int *reg_addr, unsigned int bits_val,
			unsigned int shift, unsigned int width);

#endif
