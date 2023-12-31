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

/*******************************************************************************
 *  All Winner Tech, All Right Reserved. 2014-2016 Copyright (c)
 *
 *  File name   :   de_dns_type.h
 *
 *  Description :   display engine 3.0 dns struct declaration
 *
 *  History     :   2016-3-3 zzz  v0.1  Initial version
 ******************************************************************************/

#ifndef __DE_DNS_TYPE_H__
#define __DE_DNS_TYPE_H__

#include "de_rtmx.h"


union DNS_CTL {
	u32 dwval;
	struct {
		u32 en:1;
		u32 winsz_sel:1;
		u32 res0:2;
		u32 chr_en:1;
		u32 res1:26;
		u32 win_en:1;
	} bits;
};

union DNS_SIZE {
	u32 dwval;
	struct {
		u32 width:13;
		u32 res0:3;
		u32 height:13;
		u32 res1:3;
	} bits;
};

union DNS_WIN0 {
	u32 dwval;
	struct {
		u32 win_left:13;
		u32 res0:3;
		u32 win_top:13;
		u32 res1:3;
	} bits;
};

union DNS_WIN1 {
	u32 dwval;
	struct {
		u32 win_right:13;
		u32 res0:3;
		u32 win_bot:13;
		u32 res1:3;
	} bits;
};

union DNS_LFT_PARA0 {
	u32 dwval;
	struct {
		u32 lsig:3;
		u32 res0:5;
		u32 lsig2:8;
		u32 lsig3:8;
		u32 ldir_rsig_gain2:8;
	} bits;
};

union DNS_LFT_PARA1 {
	u32 dwval;
	struct {
		u32 ldir_cen:8;
		u32 ldir_rsig_gain:8;
		u32 ldir_thrlow:8;
		u32 ldir_thrhigh:8;
	} bits;
};

union DNS_LFT_PARA2 {
	u32 dwval;
	struct {
		u32 lbben:1;
		u32 res0:7;
		u32 lbgain:8;
		u32 lbyst:4;
		u32 lbxst:4;
		u32 lbw:4;
		u32 lbh:4;
	} bits;
};

union DNS_LFT_PARA3 {
	u32 dwval;
	struct {
		u32 lsig_cen:8;
		u32 res0:24;
	} bits;
};

union DNS_CFT_PARA0 {
	u32 dwval;
	struct {
		u32 csig:3;
		u32 res0:5;
		u32 csig2:8;
		u32 csig3:8;
		u32 cdir_rsig_gain2:8;
	} bits;
};

union DNS_CFT_PARA1 {
	u32 dwval;
	struct {
		u32 cdir_cen:8;
		u32 cdir_rsig_gain:8;
		u32 cdir_thrlow:8;
		u32 cdir_thrhigh:8;
	} bits;
};

union DNS_CFT_PARA2 {
	u32 dwval;
	struct {
		u32 cbben:1;
		u32 res0:7;
		u32 cbgain:8;
		u32 cbyst:4;
		u32 cbxst:4;
		u32 cbw:4;
		u32 cbh:4;
	} bits;
};

union DNS_CFT_PARA3 {
	u32 dwval;
	struct {
		u32 csig_cen:8;
		u32 res0:24;
	} bits;
};

union IQA_CTL {
	u32 dwval;
	struct {
		u32 res0:1;
		u32 res1:15;
		u32 max:10;
		u32 res2:6;
	} bits;
};

union IQA_SUM {
	u32 dwval;
	struct {
		u32 sum;
	} bits;
};

union IQA_STA0 {
	u32 dwval;
	struct {
		u32 sta0;
	} bits;
};

union IQA_BLKDT_PARA0 {
	u32 dwval;
	struct {
		u32 dt_thrlow:8;
		u32 dt_thrhigh:8;
		u32 res0:16;
	} bits;
};

union IQA_BLKDT_SUM {
	u32 dwval;
	struct {
		u32 dt_blksum:27;
		u32 res0:5;
	} bits;
};

union IQA_BLKDT_NUM {
	u32 dwval;
	struct {
		u32 dt_blknum:19;
		u32 res0:13;
	} bits;
};


struct __dns_reg_t {
	union DNS_CTL          ctrl;                /* 0x0000 */
	union DNS_SIZE         size;                /* 0x0004 */
	union DNS_WIN0         win0;                /* 0x0008 */
	union DNS_WIN1         win1;                /* 0x000c */
	union DNS_LFT_PARA0    lpara0;              /* 0x0010 */
	union DNS_LFT_PARA1    lpara1;              /* 0x0014 */
	union DNS_LFT_PARA2    lpara2;              /* 0x0018 */
	union DNS_LFT_PARA3    lpara3;              /* 0x001c */
};

struct __iqa_reg_t {
	union IQA_CTL          iqa_ctl;             /* 0x0100 */
	union IQA_SUM          iqa_sum;             /* 0x0104 */
	union IQA_STA0         iqa_sta[13];         /* 0x0108~0x0138 */
	union IQA_BLKDT_PARA0  blk_dt_para;         /* 0x013c */
	union IQA_BLKDT_SUM    blk_dt_sum;          /* 0x0140 */
	union IQA_BLKDT_NUM    blk_dt_num;          /* 0x0144 */
};

struct __dns_para_t {
	int w;
	int h;

	int win_en;
	int win_left;
	int win_top;
	int win_right;
	int win_bot;

	int en;
	int autolvl;
	int winsz_sel;
	int sig;
	int sig2;
	int sig3;
	int rsig_cen;
	int dir_center;
	int dir_thrhigh;
	int dir_thrlow;

	int dir_rsig_gain;
	int dir_rsig_gain2;

	int bgain;
	int xst;
	int yst;

};

struct __dns_config_data {
	unsigned int mod_en; /* return mod en info */

	/* parameter */
	unsigned int level; /* user level */
	unsigned int inw;
	unsigned int inh;
	struct disp_rect croprect;
};

#endif
