// SPDX-License-Identifier:	GPL-2.0+
/*****************************************************************************/
#define _HW_BUILD_C_
/*****************************************************************************/

#include "mbr.h"
#include "phy.h"

/*****************************************************************************/

extern int PHY_VirtualPageRead(unsigned int nDieNum, unsigned int nBlkNum,
		unsigned int nPage, uint64 SectBitmap,
		void *pBuf, void *pSpare);
extern int PHY_VirtualPageWrite(unsigned int nDieNum, unsigned int nBlkNum,
		unsigned int nPage, uint64 SectBitmap,
		void *pBuf, void *pSpare);
extern int PHY_VirtualBlockErase(unsigned int nDieNum, unsigned int nBlkNum);
extern int BlockCheck(unsigned short nDieNum, unsigned short nBlkNum);
extern struct _nand_phy_partition *
build_phy_partition(struct _nand_info *nand_info, uint32 logic_size,
		uint16 flag);

unsigned int calc_crc32(void *buffer, unsigned int length);
int write_mbr(struct _nand_info *nand_info);
int write_factory_block_table(struct _nand_info *nand_info);
int build_all_phy_partition(struct _nand_info *nand_info);
void print_nand_info(struct _nand_info *nand_info);
unsigned short read_new_bad_block_table(struct _nand_info *nand_info);
int write_new_block_table(struct _nand_info *nand_info);
struct _nand_phy_partition *
get_head_phy_partition_from_nand_info(struct _nand_info *nand_info);
int write_no_use_block(struct _nand_info *nand_info);
void set_cache_level(struct _nand_info *nand_info, unsigned short cache_level);
void set_capacity_level(struct _nand_info *nand_info,
		unsigned short capacity_level);
int print_factory_block_table(struct _nand_info *nand_info);
int test_mbr(uchar *data);
void debug_read_chip(struct _nand_info *nand_info);
/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :
 *****************************************************************************/
int nand_info_init_v1(struct _nand_info *nand_info, uchar chip,
		uint16 start_block, uchar *mbr_data)
{
	unsigned int nDieNum, nBlkNum, nPage;
	unsigned char spare[BYTES_OF_USER_PER_PAGE];
	uint64 SectBitmap;
	/*unsigned int ret;*/
	unsigned int bytes_per_page;

	bytes_per_page = nand_info->SectorNumsPerPage;
	bytes_per_page <<= 9;
	nand_info->temp_page_buf = MALLOC(bytes_per_page);
	if (nand_info->temp_page_buf == NULL) {
		PRINT("[NE]%s:malloc fail for temp_page_buf\n", __func__);
	}

	MEMSET(spare, 0xff, BYTES_OF_USER_PER_PAGE);

	MEMSET(nand_info->mbr_data, 0xff, sizeof(PARTITION_MBR));

	nand_info->mini_free_block_first_reserved = MIN_FREE_BLOCK_NUM;
	nand_info->mini_free_block_reserved = MIN_FREE_BLOCK;

	// get mbr data
	nand_info->mbr_block_addr.Chip_NO = chip;
	nand_info->mbr_block_addr.Block_NO = start_block;

	nand_info->new_bad_page_addr = 0xffff;

	if (mbr_data != NULL)
		nand_info->FirstBuild = 1;
	else
		nand_info->FirstBuild = 0;

	while (1) {
		////////////////////////////////////////////////////////////////////////////////////////////
		// get mbr table
		nDieNum = nand_info->mbr_block_addr.Chip_NO;
		nBlkNum = nand_info->mbr_block_addr.Block_NO;
		nPage = 0;
		SectBitmap = nand_info->FullBitmap;
		if (!BlockCheck(nDieNum, nBlkNum)) {
			if (nand_info->FirstBuild == 1) {
				MEMCPY(nand_info->mbr_data, mbr_data,
						sizeof(PARTITION_MBR));
				write_mbr(nand_info);
				break;
			} else {
				PHY_VirtualPageRead(
						nDieNum, nBlkNum, nPage, SectBitmap,
						nand_info->temp_page_buf, spare);
				if ((spare[1] == 0xaa) && (spare[2] == 0xaa)) {
					MEMCPY(nand_info->mbr_data,
							nand_info->temp_page_buf,
							sizeof(PARTITION_MBR));
					break;
				} else if ((spare[1] == 0xaa) &&
						(spare[2] == 0xdd)) {
					PRINT("[NE]no mbr_data table %d "
							"!!!!!!!\n",
							nBlkNum);
					return NFTL_FAILURE;
				} else {
					PRINT("[NE]read mbr_data table error2 "
							"%d %x %x\n",
							nBlkNum, spare[1], spare[2]);
				}
			}
		}
		nand_info->mbr_block_addr.Block_NO++;
		if (nand_info->mbr_block_addr.Block_NO ==
				nand_info->BlkPerChip) {
			PRINT("[NE]can not find mbr table !!\n");
			return NFTL_FAILURE;
		}
	}
	/*
	   if(*(uint32 *)nand_info->mbr_data == calc_crc32((uint32 *)nand_info->mbr_data + 1,sizeof(PARTITION_MBR) - 4)) {
	   NFTL_DBG("[ND]get mbr ok!\n");
	   } else {
	   NFTL_ERR("[NE]mbr read error!\n");
	   }

	 */
	nand_info->no_used_block_addr.Chip_NO =
		nand_info->mbr_block_addr.Chip_NO;
	nand_info->no_used_block_addr.Block_NO =
		nand_info->mbr_block_addr.Block_NO + 1;
	////////////////////////////////////////////////////////////////////////////////////////////
	// get factory_bad_block table
	nand_info->bad_block_addr.Chip_NO = nand_info->mbr_block_addr.Chip_NO;
	nand_info->bad_block_addr.Block_NO =
		nand_info->mbr_block_addr.Block_NO + 1;
	while (1) {
		nDieNum = nand_info->bad_block_addr.Chip_NO;
		nBlkNum = nand_info->bad_block_addr.Block_NO;
		nPage = 0;
		SectBitmap = nand_info->FullBitmap;
		if (!BlockCheck(nDieNum, nBlkNum)) {
			if (nand_info->FirstBuild == 1) {
				break;
			} else {
				PHY_VirtualPageRead(
						nDieNum, nBlkNum, nPage, SectBitmap,
						nand_info->temp_page_buf, spare);
				if ((spare[1] == 0xaa) && (spare[2] == 0xbb)) {
					PRINT("[ND]ok  get factory_bad_block "
							"factory_bad_block table!\n");
					MEMCPY(nand_info->factory_bad_block,
							nand_info->temp_page_buf,
							FACTORY_BAD_BLOCK_SIZE);
					print_factory_block_table(nand_info);
					break;
				} else if ((spare[1] == 0xaa) &&
						(spare[2] == 0xdd)) {
					PRINT("[NE]no factory_bad_block "
							"table!!!!!!!\n");
					return NFTL_FAILURE;
				} else {
					PRINT("[NE]read factory_bad_block "
							"table error2\n");
				}
			}
		}
		nand_info->bad_block_addr.Block_NO++;
		if (nand_info->bad_block_addr.Block_NO ==
				nand_info->BlkPerChip) {
			PRINT("[NE]can not find factory_bad_block table !!\n");
			return NFTL_FAILURE;
		}
	}
	nand_info->no_used_block_addr.Chip_NO =
		nand_info->bad_block_addr.Chip_NO;
	nand_info->no_used_block_addr.Block_NO =
		nand_info->bad_block_addr.Block_NO + 1;
	////////////////////////////////////////////////////////////////////////////////////////////
	// get new_bad_block table
	nand_info->new_bad_block_addr.Chip_NO =
		nand_info->bad_block_addr.Chip_NO;
	nand_info->new_bad_block_addr.Block_NO =
		nand_info->bad_block_addr.Block_NO + 1;
	while (1) {
		nDieNum = nand_info->new_bad_block_addr.Chip_NO;
		nBlkNum = nand_info->new_bad_block_addr.Block_NO;
		nPage = 0;
		SectBitmap = nand_info->FullBitmap;
		if (!BlockCheck(nDieNum, nBlkNum)) {
			if (nand_info->FirstBuild == 1) {
				if (PHY_VirtualBlockErase(nDieNum, nBlkNum) !=
						0) {
					PRINT("[NE]init new_bad_block table "
							"error!!\n");
					return NFTL_FAILURE;
				}
				break;
			} else {
				nand_info->new_bad_page_addr =
					read_new_bad_block_table(nand_info);
				break;
			}
		}
		nand_info->new_bad_block_addr.Block_NO++;
		if (nand_info->new_bad_block_addr.Block_NO ==
				nand_info->BlkPerChip) {
			PRINT("[NE]can not find new_bad_block table!!\n");
			return NFTL_FAILURE;
		}
	}
	nand_info->no_used_block_addr.Chip_NO =
		nand_info->new_bad_block_addr.Chip_NO;
	nand_info->no_used_block_addr.Block_NO =
		nand_info->new_bad_block_addr.Block_NO + 1;
	////////////////////////////////////////////////////////////////////////////////////////////
	// get no use block
	nand_info->no_used_block_addr.Chip_NO =
		nand_info->new_bad_block_addr.Chip_NO;
	nand_info->no_used_block_addr.Block_NO =
		nand_info->new_bad_block_addr.Block_NO + 1;
	while (1) {
		nDieNum = nand_info->no_used_block_addr.Chip_NO;
		nBlkNum = nand_info->no_used_block_addr.Block_NO;
		nPage = 0;
		SectBitmap = nand_info->FullBitmap;
		if (!BlockCheck(nDieNum, nBlkNum)) {
			if (nand_info->FirstBuild == 1) {
				write_no_use_block(nand_info);
				break;
			} else {
				PHY_VirtualPageRead(
						nDieNum, nBlkNum, nPage, SectBitmap,
						nand_info->temp_page_buf, spare);
				if ((spare[1] == 0xaa) && (spare[2] == 0xdd)) {
					break;
				} else {
					PRINT("[NE]can not find no use block "
							"%d!\n",
							nBlkNum);
					return NFTL_FAILURE;
				}
			}
		}
		nand_info->no_used_block_addr.Block_NO++;
		if (nand_info->no_used_block_addr.Block_NO ==
				nand_info->BlkPerChip) {
			PRINT("[NE]can not find no_used_block!!\n");
			return NFTL_FAILURE;
		}
	}

	nand_info->no_used_block_addr.Block_NO =
		nand_info->no_used_block_addr.Block_NO + 1;

	////////////////////////////////////////////////////////////////////////////////////////////

	nand_info->phy_partition_head = NULL;

	PRINT_DBG("[ND]build_all_phy_partition start!\n");
	if (build_all_phy_partition(nand_info) != 0) {
		PRINT("[NE]%s(): build_all_phy_partition() fail!\n");
		return NFTL_FAILURE;
	}

	if (nand_info->FirstBuild == 1) {
		write_factory_block_table(nand_info);
	}

	print_nand_info(nand_info);

	return 0;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :
 *****************************************************************************/
int test_mbr(uchar *data)
{
	PARTITION_MBR *mbr_data = (PARTITION_MBR *)data;

	mbr_data->PartCount = 2;
	mbr_data->array[0].classname[0] = 'p';
	mbr_data->array[0].classname[1] = 'a';
	mbr_data->array[0].classname[2] = 'r';
	mbr_data->array[0].classname[3] = 't';
	mbr_data->array[0].classname[4] = '0';
	mbr_data->array[0].classname[5] = '\0';

	mbr_data->array[0].addr = 0;
	mbr_data->array[0].len = 327680;
	mbr_data->array[0].user_type = 0x8000;

	mbr_data->array[1].classname[0] = 'p';
	mbr_data->array[1].classname[1] = 'a';
	mbr_data->array[1].classname[2] = 'r';
	mbr_data->array[1].classname[3] = 't';
	mbr_data->array[1].classname[4] = '1';
	mbr_data->array[1].classname[5] = '\0';

	mbr_data->array[1].addr = 327680;
	mbr_data->array[1].len = 0;
	mbr_data->array[1].user_type = 0;
	return 0;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :
 *****************************************************************************/
int write_mbr(struct _nand_info *nand_info)
{
	int ret;
	unsigned int nDieNum, nBlkNum, nPage;
	unsigned char spare[BYTES_OF_USER_PER_PAGE];
	uint64 SectBitmap;
	unsigned char *buf;

	MEMSET(spare, 0xff, BYTES_OF_USER_PER_PAGE);

	nDieNum = nand_info->mbr_block_addr.Chip_NO;
	nBlkNum = nand_info->mbr_block_addr.Block_NO;
	nPage = 0;
	SectBitmap = nand_info->FullBitmap;
	buf = nand_info->temp_page_buf;
	spare[1] = 0xaa;
	spare[2] = 0xaa;

	ret = PHY_VirtualBlockErase(nDieNum, nBlkNum);
	if (ret != 0) {
		PRINT("[NE]mbr_block_addr erase error?!\n");
	}
	MEMCPY(buf, nand_info->mbr_data, sizeof(PARTITION_MBR));

	PRINT_DBG("[ND]write_mbr!\n");
	ret = PHY_VirtualPageWrite(nDieNum, nBlkNum, nPage, SectBitmap, buf,
			spare);
	if (ret != 0) {
		PRINT("[NE]mbr_block_addr write error?!\n");
	}

	return 0;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :
 *****************************************************************************/
int write_factory_block_table(struct _nand_info *nand_info)
{
	int ret, i;
	unsigned int nDieNum, nBlkNum;
	unsigned char spare[BYTES_OF_USER_PER_PAGE];
	uint64 SectBitmap;
	unsigned char *buf;

	MEMSET(spare, 0xff, BYTES_OF_USER_PER_PAGE);
	spare[1] = 0xaa;
	spare[2] = 0xbb;
	nDieNum  = nand_info->bad_block_addr.Chip_NO;
	nBlkNum  = nand_info->bad_block_addr.Block_NO;
	/*nPage = 0;*/
	SectBitmap = nand_info->FullBitmap;
	buf = nand_info->temp_page_buf;

	ret = PHY_VirtualBlockErase(nDieNum, nBlkNum);
	if (ret != 0) {
		PRINT("[NE]bad_block_addr erase error?!\n");
	}

	MEMCPY(buf, nand_info->factory_bad_block, FACTORY_BAD_BLOCK_SIZE);

	PRINT("[NE]write_factory_block_table!\n");
	for (i = 0; i < nand_info->PageNumsPerBlk; i++) {
		ret = PHY_VirtualPageWrite(nDieNum, nBlkNum, i, SectBitmap, buf,
				spare);
		if (ret != 0) {
			PRINT("[NE]bad_block_addr write error %d %d?!\n",
					nBlkNum, i);
		}
	}

	nDieNum = FACTORY_BAD_BLOCK_SIZE >> 2;
	for (i = 0; i < nDieNum; i++) {
		if (nand_info->factory_bad_block[i].Chip_NO != 0xffff) {
			PRINT("[ND]factory bad block:%d %d!\n",
					nand_info->factory_bad_block[i].Chip_NO,
					nand_info->factory_bad_block[i].Block_NO);
		} else {
			break;
		}
	}

	return 0;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :
 *****************************************************************************/
int print_factory_block_table(struct _nand_info *nand_info)
{
	int i, nDieNum;

	nDieNum = FACTORY_BAD_BLOCK_SIZE >> 2;
	for (i = 0; i < nDieNum; i++) {
		if (nand_info->factory_bad_block[i].Chip_NO != 0xffff) {
			PRINT_DBG("[ND]factory bad block:%d %d!\n",
					nand_info->factory_bad_block[i].Chip_NO,
					nand_info->factory_bad_block[i].Block_NO);
		} else {
			break;
		}
	}
	return 0;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :
 *****************************************************************************/
int write_new_block_table(struct _nand_info *nand_info)
{
	int ret;
	unsigned int nDieNum, nBlkNum, nPage;
	unsigned char spare[BYTES_OF_USER_PER_PAGE];
	uint64 SectBitmap;
	unsigned char *buf;

	MEMSET(spare, 0xff, BYTES_OF_USER_PER_PAGE);
	spare[1] = 0xaa;
	spare[2] = 0xcc;
	nDieNum = nand_info->new_bad_block_addr.Chip_NO;
	nBlkNum = nand_info->new_bad_block_addr.Block_NO;
	SectBitmap = nand_info->FullBitmap;
	buf = nand_info->temp_page_buf;

	nand_info->new_bad_page_addr++;
	if (nand_info->new_bad_page_addr == nand_info->PageNumsPerBlk) {
		ret = PHY_VirtualBlockErase(nDieNum, nBlkNum);
		if (ret != 0) {
			PRINT("[NE]new_bad_block_addr erase error?!\n");
		}
		nand_info->new_bad_page_addr = 0;
	}

	nPage = nand_info->new_bad_page_addr;

	MEMCPY(buf, nand_info->new_bad_block, PHY_PARTITION_BAD_BLOCK_SIZE);

	PRINT("[NE]write_new_bad_block_table %d,%d!\n", nBlkNum, nPage);
	ret = PHY_VirtualPageWrite(nDieNum, nBlkNum, nPage, SectBitmap, buf,
			spare);
	if (ret != 0) {
		PRINT("[NE]bad_block_addr write error?!\n");
	}

	return 0;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :
 *****************************************************************************/
int write_no_use_block(struct _nand_info *nand_info)
{
	int ret;
	unsigned int nDieNum, nBlkNum, nPage;
	unsigned char spare[BYTES_OF_USER_PER_PAGE];
	uint64 SectBitmap;
	unsigned char *buf;

	MEMSET(spare, 0xff, BYTES_OF_USER_PER_PAGE);
	spare[1] = 0xaa;
	spare[2] = 0xdd;
	nDieNum = nand_info->no_used_block_addr.Chip_NO;
	nBlkNum = nand_info->no_used_block_addr.Block_NO;
	nPage = 0;
	SectBitmap = nand_info->FullBitmap;
	buf = nand_info->temp_page_buf;

	ret = PHY_VirtualBlockErase(nDieNum, nBlkNum);
	if (ret != 0) {
		PRINT("[NE]write_no_use_block erase error?!\n");
	}

	PRINT_DBG("[ND]write_no_use_block!\n");
	ret = PHY_VirtualPageWrite(nDieNum, nBlkNum, nPage, SectBitmap, buf,
			spare);
	if (ret != 0) {
		PRINT("[NE]write_no_use_block write error?!\n");
		return NFTL_FAILURE;
	}

	return 0;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :
 *****************************************************************************/
unsigned short read_new_bad_block_table(struct _nand_info *nand_info)
{
	unsigned short num, i;
	unsigned int nDieNum, nBlkNum;
	unsigned char spare[BYTES_OF_USER_PER_PAGE];
	uint64 SectBitmap;
	/*unsigned char* buf;*/

	nDieNum = nand_info->new_bad_block_addr.Chip_NO;
	nBlkNum = nand_info->new_bad_block_addr.Block_NO;
	SectBitmap = nand_info->FullBitmap;
	/*buf = nand_info->temp_page_buf;*/

	num = 0xffff;

	for (i = 0; i < nand_info->PageNumsPerBlk; i++) {
		MEMSET(spare, 0xff, BYTES_OF_USER_PER_PAGE);
		PHY_VirtualPageRead(nDieNum, nBlkNum, i, SectBitmap,
				nand_info->temp_page_buf, spare);
		if ((spare[1] == 0xaa) && (spare[2] == 0xcc) &&
				(spare[0] == 0xff)) {
			PRINT_DBG("[ND]ok get a new bad table!\n");
			MEMCPY(nand_info->new_bad_block,
					nand_info->temp_page_buf,
					PHY_PARTITION_BAD_BLOCK_SIZE);
			num = i;
		} else if ((spare[1] == 0xff) && (spare[2] == 0xff) &&
				(spare[0] == 0xff)) {
			PRINT("[ND]new bad block last first use page:%d\n", i);
			break;
		} else {
			PRINT("[NE]read new_bad_block table error:%d!\n", i);
		}
	}

	nDieNum = PHY_PARTITION_BAD_BLOCK_SIZE >> 2;

	for (i = 0; i < nDieNum; i++) {
		if (nand_info->new_bad_block[i].Chip_NO != 0xffff) {
			PRINT("[ND]new bad block:%d %d!\n",
					nand_info->new_bad_block[i].Chip_NO,
					nand_info->new_bad_block[i].Block_NO);
		} else {
			break;
		}
	}

	return num;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :
 *****************************************************************************/
void print_nand_info(struct _nand_info *nand_info)
{
	PRINT_DBG("[ND]nand_info->type :%d\n", nand_info->type);
	PRINT_DBG("[ND]nand_info->SectorNumsPerPage :%d\n",
			nand_info->SectorNumsPerPage);
	PRINT_DBG("[ND]nand_info->BytesUserData :%d\n",
			nand_info->BytesUserData);
	PRINT_DBG("[ND]nand_info->PageNumsPerBlk :%d\n",
			nand_info->PageNumsPerBlk);
	PRINT_DBG("[ND]nand_info->BlkPerChip :%d\n", nand_info->BlkPerChip);
	PRINT_DBG("[ND]nand_info->FirstBuild :%d\n", nand_info->FirstBuild);
	PRINT_DBG("[ND]nand_info->FullBitmap :%d\n", nand_info->FullBitmap);
	PRINT_DBG("[ND]nand_info->bad_block_addr.Chip_NO :%d\n",
			nand_info->bad_block_addr.Chip_NO);
	PRINT_DBG("[ND]nand_info->bad_block_addr.Block_NO :%d\n",
			nand_info->bad_block_addr.Block_NO);
	PRINT_DBG("[ND]nand_info->mbr_block_addr.Chip_NO :%d\n",
			nand_info->mbr_block_addr.Chip_NO);
	PRINT_DBG("[ND]nand_info->mbr_block_addr.Block_NO :%d\n",
			nand_info->mbr_block_addr.Block_NO);
	PRINT_DBG("[ND]nand_info->no_used_block_addr.Chip_NO :%d\n",
			nand_info->no_used_block_addr.Chip_NO);
	PRINT_DBG("[ND]nand_info->no_used_block_addr.Block_NO :%d\n",
			nand_info->no_used_block_addr.Block_NO);
	PRINT_DBG("[ND]nand_info->new_bad_block_addr.Chip_NO :%d\n",
			nand_info->new_bad_block_addr.Chip_NO);
	PRINT_DBG("[ND]nand_info->new_bad_block_addr.Block_NO :%d\n",
			nand_info->new_bad_block_addr.Block_NO);
	PRINT_DBG("[ND]nand_info->new_bad_page_addr :%d\n",
			nand_info->new_bad_page_addr);
	PRINT_DBG("[ND]nand_info->partition_nums :%d\n",
			nand_info->partition_nums);
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :
 *****************************************************************************/
unsigned int calc_crc32(void *buffer, unsigned int length)
{
	unsigned int i, j;
	struct _NAND_CRC32_DATA nand_crc32 = {0};

	unsigned int CRC32 = 0xffffffff; //设置初始值
	nand_crc32.CRC = 0;

	for (i = 0; i < 256; ++i) {//用++i以提高效率
		nand_crc32.CRC = i;
		for (j = 0; j < 8; ++j) {
			//这个循环实际上就是用"计算法"来求取CRC的校验码
			if (nand_crc32.CRC & 1)
				nand_crc32.CRC =
					(nand_crc32.CRC >> 1) ^ 0xEDB88320;
			else // 0xEDB88320就是CRC-32多项表达式的值
				nand_crc32.CRC >>= 1;
		}
		nand_crc32.CRC_32_Tbl[i] = nand_crc32.CRC;
	}

	CRC32 = 0xffffffff; //设置初始值
	for (i = 0; i < length; ++i) {
		CRC32 = nand_crc32.CRC_32_Tbl[(CRC32 ^
				((unsigned char *)buffer)[i]) &
			0xff] ^
			(CRC32 >> 8);
	}
	// return CRC32;
	return CRC32 ^ 0xffffffff;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :
 *****************************************************************************/
int build_all_phy_partition(struct _nand_info *nand_info)
{
	PARTITION_MBR *mbr;
	unsigned int part_cnt, part_type, i, m;

	mbr = (PARTITION_MBR *)nand_info->mbr_data;

	MEMSET(nand_info->partition, 0xff, sizeof(nand_info->partition));

	PRINT_DBG("[ND]nand_info->SectorNumsPerPage :0x%x\n",
			nand_info->SectorNumsPerPage);
	PRINT_DBG("[ND]nand_info->PageNumsPerBlk :0x%x\n",
			nand_info->PageNumsPerBlk);
	PRINT_DBG("[ND]nand_info->BlkPerChip :0x%x\n", nand_info->BlkPerChip);
	PRINT_DBG("[ND]nand_info->ChipNum :0x%x\n", nand_info->ChipNum);

	nand_info->partition_nums = 0;
	for (i = 0; i < MAX_PARTITION; i++) {
		nand_info->partition[i].size = 0;
		nand_info->partition[i].cross_talk = 0;
		// nand_info->partition[i].offset = 0;
		m = 0;
		for (part_cnt = 0; part_cnt < mbr->PartCount &&
				part_cnt < ND_MAX_PARTITION_COUNT;
				part_cnt++) {
			part_type = mbr->array[part_cnt].user_type & 0x0000ff00;

			if ((part_type & FTL_PARTITION_TYPE) != 0) {
				part_type >>= 8;
				if ((part_type & 0x0f) == i) {
					if ((part_type & 0x40) != 0) {
						nand_info->partition[i]
							.cross_talk = 0;
					}
					PRINT_DBG(
							"[ND]get phy_partition %s size "
							":0x%x\n",
							mbr->array[part_cnt].classname,
							mbr->array[part_cnt].len);
					nand_info->partition[i]
						.nand_disk[m]
						.size = mbr->array[part_cnt].len;
					nand_info->partition[i]
						.nand_disk[m]
						.type = 0;
					MEMCPY(nand_info->partition[i]
							.nand_disk[m]
							.name,
							mbr->array[part_cnt].classname,
							PARTITION_NAME_SIZE);
					if (nand_info->partition[i].size !=
							0xffffffff) {
						if (mbr->array[part_cnt].len ==
								0) {
							nand_info->partition[i]
								.size = 0xffffffff;
						} else {
							nand_info->partition[i]
								.size +=
								nand_info
								->partition[i]
								.nand_disk[m]
								.size;
						}
					}
					m++;
				}
			}
		}
		if (m != 0) {
			nand_info->partition_nums++;
		}
	}

	for (part_cnt = 0;
			part_cnt < mbr->PartCount && part_cnt < ND_MAX_PARTITION_COUNT;
			part_cnt++) {
		PRINT_DBG("[ND]partition: %d,%d,%d\n", mbr->PartCount, part_cnt,
				mbr->array[part_cnt].len);
		if (mbr->array[part_cnt].len == 0) {
			part_type = mbr->array[part_cnt].user_type & 0x0000ff00;
			PRINT_DBG("[ND]user_type:%d\n", part_type);
			part_type >>= 8;
			if ((part_type & 0x40) != 0) {
				nand_info->partition[nand_info->partition_nums]
					.cross_talk = 0;
			}
			PRINT_DBG("[ND]get last phy_partition\n");
			nand_info->partition[nand_info->partition_nums].size =
				0xffffffff;
			nand_info->partition[nand_info->partition_nums]
				.nand_disk[0]
				.size = 0xffffffff;
			nand_info->partition[nand_info->partition_nums]
				.nand_disk[0]
				.type = 0;
			MEMCPY(nand_info->partition[nand_info->partition_nums]
					.nand_disk[0]
					.name,
					mbr->array[part_cnt].classname,
					PARTITION_NAME_SIZE);
			nand_info->partition[nand_info->partition_nums]
				.nand_disk[1]
				.type = 0xffffffff;
			nand_info->partition_nums++;
			break;
		}
	}

	for (i = 0; i < nand_info->partition_nums; i++) {
		nand_info->partition[i].cross_talk = 0;
		if (build_phy_partition(nand_info, nand_info->partition[i].size,
					nand_info->partition[i].cross_talk) ==
				NULL) {
			PRINT("[NE]build_phy_partition %d error!\n", i);
			return NFTL_FAILURE;
		}
	}

	PRINT_DBG("[ND]build %d phy_partition !\n", nand_info->partition_nums);
	return 0;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :
 *****************************************************************************/
	struct _nand_phy_partition *
get_head_phy_partition_from_nand_info(struct _nand_info *nand_info)
{
	return nand_info->phy_partition_head;
}
/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :
 *****************************************************************************/
void set_cache_level(struct _nand_info *nand_info, unsigned short cache_level)
{
	nand_info->cache_level = cache_level;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :
 *****************************************************************************/
void set_capacity_level(struct _nand_info *nand_info,
		unsigned short capacity_level)
{
	if (capacity_level != 0)
		nand_info->capacity_level = 1;
	else
		nand_info->capacity_level = 0;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :
 *****************************************************************************/
void set_read_reclaim_interval(struct _nand_info *nand_info,
		unsigned int read_reclaim_interval)
{
	nand_info->read_claim_interval = read_reclaim_interval;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :
 *****************************************************************************/
unsigned short debug_read_block(struct _nand_info *nand_info,
		unsigned int nDieNum, unsigned int nBlkNum)
{
	unsigned int i;

	unsigned char spare[BYTES_OF_USER_PER_PAGE];

	for (i = 0; i < nand_info->PageNumsPerBlk; i++) {
		MEMSET(spare, 0xff, BYTES_OF_USER_PER_PAGE);
		PHY_VirtualPageRead(nDieNum, nBlkNum, i, nand_info->FullBitmap,
				nand_info->temp_page_buf, spare);
		PRINT("[ND]block:%d page:%d spare: %x %x %x %x %x %x %x %x %x "
				"%x\n",
				nBlkNum, i, spare[0], spare[1], spare[2], spare[3],
				spare[4], spare[5], spare[6], spare[7], spare[8],
				spare[9]);
	}

	return 0;
}
/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :
 *****************************************************************************/
void debug_read_chip(struct _nand_info *nand_info)
{
	unsigned int i, j;

	for (i = 0; i < nand_info->ChipNum; i++) {
		PRINT("[ND]=============chip %d============================\n",
				i);
		for (j = 0; j < nand_info->BlkPerChip; j++) {
			debug_read_block(nand_info, i, j);
		}
	}
	PRINT("[ND]=================end========================\n");
}
