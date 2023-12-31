/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */

#ifndef __AW_SPINAND_H
#define __AW_SPINAND_H

#include <linux/mutex.h>
#include <linux/spi/spi.h>

#define AW_OOB_SIZE_PER_PHY_PAGE (16)
/**
 * In order to fix for nftl nand, make they has the same address
 * for saving crc16
 */
#define AW_CRC16_OOB_OFFSET (12)

#define SBROM_TOC0_HEAD_SPACE  (0x80)
#define STORAGE_BUFFER_SIZE    (256)
#define STAMP_VALUE             0x5F0A6C39
#define MAX_ID_LEN 8

/* ecc status */
#define ECC_GOOD	(0 << 4)
#define ECC_LIMIT	(1 << 4)
#define ECC_ERR		(2 << 4)

#define SECBLK_READ		_IO('V', 20)
#define SECBLK_WRITE		_IO('V', 21)
#define SECBLK_IOCTL		_IO('V', 22)

#define BLKBURNBOOT0		_IO('v', 127)
#define BLKBURNBOOT1		_IO('v', 128)

struct aw_spinand_ecc;
struct aw_spinand_info;
struct aw_spinand_phy_info;
struct aw_spinand_chip_ops;

struct aw_spinand_chip {
	struct aw_spinand_chip_ops *ops;
	struct aw_spinand_ecc *ecc;
	struct aw_spinand_cache *cache;
	struct aw_spinand_info *info;
	struct aw_spinand_bbt *bbt;
	struct spi_device *spi;
	unsigned int rx_bit;
	unsigned int tx_bit;
	unsigned int freq;
	void *priv;
};

struct aw_spinand_chip_request {
	unsigned int block;
	unsigned int page;
	unsigned int pageoff;
	unsigned int ooblen;
	unsigned int datalen;
	void *databuf;
	void *oobbuf;

	unsigned int oobleft;
	unsigned int dataleft;
#define AW_SPINAND_MTD_OPS_RAW (2)
	int mode;
#define AW_SPINAND_MTD_REQ_PANIC (1)
	int type;
};

struct aw_spinand_chip_ops {
	int (*get_block_lock)(struct aw_spinand_chip *chip, u8 *reg_val);
	int (*set_block_lock)(struct aw_spinand_chip *chip, u8 reg_val);
	int (*get_otp)(struct aw_spinand_chip *chip, u8 *reg_val);
	int (*set_otp)(struct aw_spinand_chip *chip, u8 reg_val);
	int (*get_driver_level)(struct aw_spinand_chip *chip, u8 *reg_val);
	int (*set_driver_level)(struct aw_spinand_chip *chip, u8 reg_val);
	int (*reset)(struct aw_spinand_chip *chip);
	int (*read_status)(struct aw_spinand_chip *chip, u8 *status);
	int (*read_id)(struct aw_spinand_chip *chip, void *id, int len,
			int dummy);
	int (*read_reg)(struct aw_spinand_chip *chip, u8 cmd, u8 reg, u8 *val);
	int (*write_reg)(struct aw_spinand_chip *chip, u8 cmd, u8 reg, u8 val);
	int (*is_bad)(struct aw_spinand_chip *chip,
			struct aw_spinand_chip_request *req);
	int (*mark_bad)(struct aw_spinand_chip *chip,
			struct aw_spinand_chip_request *req);
	int (*erase_block)(struct aw_spinand_chip *chip,
			struct aw_spinand_chip_request *req);
	int (*write_page)(struct aw_spinand_chip *chip,
			struct aw_spinand_chip_request *req);
	int (*read_page)(struct aw_spinand_chip *chip,
			struct aw_spinand_chip_request *req);
	int (*phy_is_bad)(struct aw_spinand_chip *chip,
			struct aw_spinand_chip_request *req);
	int (*phy_mark_bad)(struct aw_spinand_chip *chip,
			struct aw_spinand_chip_request *req);
	int (*phy_erase_block)(struct aw_spinand_chip *chip,
			struct aw_spinand_chip_request *req);
	int (*phy_write_page)(struct aw_spinand_chip *chip,
			struct aw_spinand_chip_request *req);
	int (*phy_read_page)(struct aw_spinand_chip *chip,
			struct aw_spinand_chip_request *req);
	int (*phy_copy_block)(struct aw_spinand_chip *chip,
			unsigned int from_blk, unsigned int to_blk);
};

struct aw_spinand_info {
	const char *(*model)(struct aw_spinand_chip *chip);
	const char *(*manufacture)(struct aw_spinand_chip *chip);
	void (*nandid)(struct aw_spinand_chip *chip, unsigned char *id, int cnt);
	unsigned int (*die_cnt)(struct aw_spinand_chip *chip);
	unsigned int (*oob_size)(struct aw_spinand_chip *chip);
	unsigned int (*sector_size)(struct aw_spinand_chip *chip);
	unsigned int (*page_size)(struct aw_spinand_chip *chip);
	unsigned int (*block_size)(struct aw_spinand_chip *chip);
	unsigned int (*phy_oob_size)(struct aw_spinand_chip *chip);
	unsigned int (*phy_page_size)(struct aw_spinand_chip *chip);
	unsigned int (*phy_block_size)(struct aw_spinand_chip *chip);
	unsigned int (*total_size)(struct aw_spinand_chip *chip);
	int (*operation_opt)(struct aw_spinand_chip *chip);
	int (*max_erase_times)(struct aw_spinand_chip *chip);

	/* private data */
	struct aw_spinand_phy_info *phy_info;
};

typedef struct {
	__u8        ChipCnt;                 /* the count of the total nand flash chips are currently connecting on the CE pin */
	__u8        ConnectMode;             /* the rb connect  mode */
	__u8        BankCntPerChip;          /* the count of the banks in one nand chip, multiple banks can support Inter-Leave */
	__u8        DieCntPerChip;           /* the count of the dies in one nand chip, block management is based on Die */
	__u8        PlaneCntPerDie;          /* the count of planes in one die, multiple planes can support multi-plane operation */
	__u8        SectorCntPerPage;        /* the count of sectors in one single physic page, one sector is 0.5k */
	__u16       ChipConnectInfo;         /* chip connect information, bit == 1 means there is a chip connecting on the CE pin */
	__u32       PageCntPerPhyBlk;        /* the count of physic pages in one physic block */
	__u32       BlkCntPerDie;            /* the count of the physic blocks in one die, include valid block and invalid block */
	__u32       OperationOpt;            /* the mask of the operation types which current nand flash can support support */
	__u32       FrequencePar;            /* the parameter of the hardware access clock, based on 'MHz' */
	__u32       SpiMode;                 /* spi nand mode, 0:mode 0, 3:mode 3 */
	__u8        NandChipId[8];           /* the nand chip id of current connecting nand chip */
	__u32       pagewithbadflag;         /* bad block flag was written at the first byte of spare area of this page */
	__u32       MultiPlaneBlockOffset;   /* the value of the block number offset between the two plane block */
	__u32       MaxEraseTimes;           /* the max erase times of a physic block */
	__u32       MaxEccBits;              /* the max ecc bits that nand support */
	__u32       EccLimitBits;            /* the ecc limit flag for tne nand */
	__u32       uboot_start_block;
	__u32       uboot_next_block;
	__u32       logic_start_block;
	__u32       nand_specialinfo_page;
	__u32       nand_specialinfo_offset;
	__u32       physic_block_reserved;
	__u32	    sample_mode;
	__u32	    sample_delay;
	__u32       Reserved[4];
} boot_spinand_para_t;

typedef struct {
	unsigned int ChannelCnt;
	/* count of total nand chips are currently connecting on the CE pin */
	unsigned int ChipCnt;
	/* chip connect info, bit=1 means one chip connecting on the CE pin */
	unsigned int ChipConnectInfo;
	unsigned int RbCnt;
	/* connect info of all rb  chips are connected */
	unsigned int RbConnectInfo;
	unsigned int RbConnectMode;	/*rb connect mode */
	/* count of banks in one nand chip, multi banks can support Inter-Leave */
	unsigned int BankCntPerChip;
	/* count of dies in one nand chip, block management is based on Die */
	unsigned int DieCntPerChip;
	/* count of planes in one die, >1 can support multi-plane operation */
	unsigned int PlaneCntPerDie;
	/* count of sectors in one single physic page, one sector is 0.5k */
	unsigned int SectorCntPerPage;
	/* count of physic pages in one physic block */
	unsigned int PageCntPerPhyBlk;
	/* count of physic blocks in one die, include valid and invalid blocks */
	unsigned int BlkCntPerDie;
	/* mask of operation types which current nand flash can support support */
	unsigned int OperationOpt;
	/* parameter of hardware access clock, based on 'MHz' */
	unsigned int FrequencePar;
	/* Ecc Mode for nand chip, 0: bch-16, 1:bch-28, 2:bch_32 */
	unsigned int EccMode;
	/* nand chip id of current connecting nand chip */
	unsigned char NandChipId[8];
	/* ratio of valid physical blocks, based on 1024 */
	unsigned int ValidBlkRatio;
	unsigned int good_block_ratio; /* good block ratio get from hwscan */
	unsigned int ReadRetryType; /* read retry type */
	unsigned int DDRType;
	unsigned int Reserved[32];
} boot_nand_para_t;

typedef struct _normal_gpio_cfg {
	unsigned char port;
	unsigned char port_num;
	char mul_sel;
	char pull;
	char drv_level;
	char data;
	unsigned char reserved[2];
} normal_gpio_cfg;

/******************************************************************************/
/*                              head of Boot0                                 */
/******************************************************************************/
typedef struct _boot0_private_head_t {
	unsigned int prvt_head_size;
	char prvt_head_vsn[4];        /* the version of boot0_private_head_t */
	unsigned int dram_para[32];   /* Original values is arbitrary */
	int uart_port;
	normal_gpio_cfg uart_ctrl[2];
	int enable_jtag;              /* 1 : enable,  0 : disable */
	normal_gpio_cfg jtag_gpio[5];
	normal_gpio_cfg storage_gpio[32];
	char storage_data[512 - sizeof(normal_gpio_cfg) * 32];
}
boot0_private_head_t;

typedef struct standard_Boot_file_head {
	unsigned int jump_instruction;  /* one intruction jumping to real code */
	unsigned char magic[8];         /* ="eGON.BT0" or "eGON.BT1",  not C-style string */
	unsigned int check_sum;         /* generated by PC */
	unsigned int length;            /* generated by PC */
	unsigned int pub_head_size;     /* size of boot_file_head_t */
	unsigned char pub_head_vsn[4];  /* version of boot_file_head_t */
	unsigned char file_head_vsn[4]; /* version of boot0_file_head_t or boot1_file_head_t */
	unsigned char Boot_vsn[4];      /* Boot version */
	unsigned char eGON_vsn[4];      /* eGON version */
	unsigned char platform[8];      /* platform information */
} standard_boot_file_head_t;

typedef struct _boot0_file_head_t {
	standard_boot_file_head_t boot_head;
	boot0_private_head_t prvt_head;
} boot0_file_head_t;

typedef struct _boot_core_para_t {
	unsigned int user_set_clock;
	unsigned int user_set_core_vol;
	unsigned int vol_threshold;
} boot_core_para_t;

/******************************************************************************/
/*                                   head of Boot1                            */
/******************************************************************************/
typedef struct _boot1_private_head_t {
	unsigned int dram_para[32];
	int run_clock;		/* Mhz */
	int run_core_vol;	/* mV */
	int uart_port;
	normal_gpio_cfg uart_gpio[2];
	int twi_port;
	normal_gpio_cfg twi_gpio[2];
	int work_mode;
	int storage_type;	/* 0nand   1sdcard    2: spinor */
	normal_gpio_cfg nand_gpio[32];
	char nand_spare_data[256];
	normal_gpio_cfg sdcard_gpio[32];
	char sdcard_spare_data[256];
	int reserved[6];
} boot1_private_head_t;

typedef struct _Boot_file_head {
	unsigned int jump_instruction; /* one intruction jumping to real code */
	unsigned char magic[8];        /* ="u-boot" */
	unsigned int check_sum;        /* generated by PC */
	unsigned int align_size;       /* align size in byte */
	unsigned int length;           /* the size of all file */
	unsigned int uboot_length;     /* the size of uboot */
	unsigned char version[8];      /* uboot version */
	unsigned char platform[8];     /* platform information */
	int reserved[1];               /* stamp space, 16bytes align */
} boot_file_head_t;

typedef struct _boot1_file_head_t {
	boot_file_head_t boot_head;
	boot1_private_head_t prvt_head;
} boot1_file_head_t;

typedef struct sbrom_toc0_config {
	unsigned char config_vsn[4];
	unsigned int dram_para[32];
	int uart_port;
	normal_gpio_cfg uart_ctrl[2];
	int enable_jtag;
	normal_gpio_cfg jtag_gpio[5];
	normal_gpio_cfg storage_gpio[50];
	char storage_data[384];
	unsigned int secure_dram_mbytes;
	unsigned int drm_start_mbytes;
	unsigned int drm_size_mbytes;
	unsigned int res[8];
} sbrom_toc0_config_t;

typedef struct {
	u8 name[8];
	u32 magic;
	u32 check_sum;

	u32 serial_num;
	u32 status;

	u32 items_nr;
	u32 length;
	u8 platform[4];
	u32 reserved[2];
	u32 end;

} toc0_private_head_t;


int addr_to_req(struct aw_spinand_chip *chip, struct aw_spinand_chip_request *req,
		unsigned int addr);
int aw_spinand_chip_init(struct spi_device *spi, struct aw_spinand_chip *chip);
void aw_spinand_chip_exit(struct aw_spinand_chip *chip);

#define aw_spinand_hexdump(level, prefix, buf, len)			\
	print_hex_dump(level, prefix, DUMP_PREFIX_OFFSET, 16, 1,	\
			buf, len, true)
#define aw_spinand_reqdump(func, note, req)				\
	do {								\
		func("%s(%d): %s\n", __func__, __LINE__, note);		\
		func("\tblock: %u\n", (req)->block);			\
		func("\tpage: %u\n", (req)->page);			\
		func("\tpageoff: %u\n", (req)->pageoff);		\
		if ((req)->databuf)					\
			func("\tdatabuf: 0x%p\n", (req)->databuf);	\
		else							\
			func("\tdatabuf: NULL\n");			\
		func("\tdatalen: %u\n", (req)->datalen);		\
		func("\tdataleft: %u\n", (req)->dataleft);		\
		if ((req)->oobbuf)					\
			func("\toobbuf: 0x%p\n", (req)->oobbuf);	\
		else							\
			func("\toobbuf: NULL\n");			\
		func("\tooblen: %u\n", (req)->ooblen);			\
		func("\toobleft: %u\n", (req)->oobleft);		\
		func("\n");						\
	} while (0)

#if IS_ENABLED(CONFIG_AW_SPINAND_SECURE_STORAGE)
struct aw_spinand_sec_sto {
	/* the follow three must be initialized by the caller before read/write */
	unsigned int startblk;
	unsigned int endblk;
	struct aw_spinand_chip *chip;

	unsigned int blk[2];
	int init_end;
};

int aw_spinand_secure_storage_read(struct aw_spinand_sec_sto *sec_sto,
		int item, char *buf, unsigned int len);
int aw_spinand_secure_storage_write(struct aw_spinand_sec_sto *sec_sto,
		int item, char *buf, unsigned int len);
int aw_spinand_mtd_read_secure_storage(struct mtd_info *mtd,
		int item, void *buf, unsigned int len);
int aw_spinand_mtd_write_secure_storage(struct mtd_info *mtd,
		int item, void *buf, unsigned int len);
#endif

int aw_spinand_mtd_download_boot0(struct mtd_info *mtd,
		unsigned int len, void *buf);
int aw_spinand_mtd_download_uboot(struct mtd_info *mtd,
		unsigned int len, void *buf);
struct mtd_info *get_aw_spinand_mtd(void);
int spinand_mtd_upload_bootpackage(struct mtd_info *mtd, loff_t from,
		unsigned int len, void *buf);
int aw_spinand_chip_update_cfg(struct aw_spinand_chip *chip);

#endif
