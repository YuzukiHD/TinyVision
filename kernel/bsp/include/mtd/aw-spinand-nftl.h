/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */

#ifndef __AW_SPINAND_NFTL_H__
#define __AW_SPINAND_NFTL_H__

#include <sunxi-boot.h>

#define AW_NFTL_OOB_LEN (16)

enum AW_NFTL_ECC_STATUS {
	AW_NFTL_ECC_ERR = -2,
	AW_NFTL_ECC_LIMIT = 10
};


unsigned int spinand_nftl_get_super_page_size(int type);
unsigned int spinand_nftl_get_super_block_size(int type);
unsigned int spinand_nftl_get_single_page_size(int type);
unsigned int spinand_nftl_get_single_block_size(int type);
unsigned int spinand_nftl_get_chip_size(int type);
unsigned int spinand_nftl_get_die_size(int type);
unsigned int spinand_nftl_get_die_cnt(void);
unsigned int spinand_nftl_get_chip_cnt(void);
unsigned int spinand_nftl_get_max_erase_times(void);
unsigned int spinand_nftl_get_multi_plane_flag(void);
unsigned int spinand_nftl_get_operation_opt(void);
void spinand_nftl_get_chip_id(unsigned char *id);

int spinand_nftl_read_super_page(unsigned short dienum, unsigned short blocknum,
		unsigned short pagenum, unsigned short sectorbitmap,
		void *rmbuf, void *rspare);

int spinand_nftl_write_super_page(unsigned short dienum, unsigned short blocknum,
		unsigned short pagenum, unsigned short sectorbitmap,
		void *wmbuf, void *wspare);
int spinand_nftl_erase_super_block(unsigned short dienum, unsigned short blocknum);
int spinand_nftl_super_badblock_check(unsigned short dienum, unsigned short blocknum);
int spinand_nftl_super_badblock_mark(unsigned short dienum, unsigned short blocknum);

int spinand_nftl_read_single_page(unsigned short dienum, unsigned short blocknum,
		unsigned short pagenum, unsigned short sectorbitmap,
		void *rmbuf, void *rspare);

int spinand_nftl_write_single_page(unsigned short dienum, unsigned short blocknum,
		unsigned short pagenum, unsigned short sectorbitmap,
		void *wmbuf, void *wspare);
int spinand_nftl_erase_single_block(unsigned short dienum, unsigned short blocknum);

int spinand_nftl_single_block_copy(unsigned int from_chip,
		unsigned int from_block, unsigned int to_chip,
		unsigned int to_block);
int spinand_nftl_single_badblock_check(unsigned short chipnum, unsigned short blocknum);
int spinand_nftl_single_badblock_mark(unsigned short chipnum, unsigned short blocknum);

#endif /*AW_SPINAND_NFTL_H*/
