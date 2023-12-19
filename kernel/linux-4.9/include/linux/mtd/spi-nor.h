/*
 * Copyright (C) 2014 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __LINUX_MTD_SPI_NOR_H
#define __LINUX_MTD_SPI_NOR_H

#include <linux/bitops.h>
#include <linux/mtd/cfi.h>
#include <linux/mtd/mtd.h>
#include <linux/spi/spi-mem.h>

/*
 * Manufacturer IDs
 *
 * The first byte returned from the flash after sending opcode SPINOR_OP_RDID.
 * Sometimes these are the same as CFI IDs, but sometimes they aren't.
 */
#define SNOR_MFR_ATMEL		CFI_MFR_ATMEL
#define SNOR_MFR_GIGADEVICE	0xc8
#define SNOR_MFR_INTEL		CFI_MFR_INTEL
#define SNOR_MFR_ST		CFI_MFR_ST /* ST Micro <--> Micron */
#define SNOR_MFR_MICRON		CFI_MFR_MICRON /* ST Micro <--> Micron */
#define SNOR_MFR_MACRONIX	CFI_MFR_MACRONIX
#define SNOR_MFR_SPANSION	CFI_MFR_AMD
#define SNOR_MFR_SST		CFI_MFR_SST
#define SNOR_MFR_ESMT		CFI_MFR_EON
#define SNOR_MFR_GFX		CFI_MFR_EON
#define SNOR_MFR_WINBOND	0xef /* Also used by some Spansion */
#define SNOR_MFR_ADESTO		0x1f /* Also used by some Spansion0 */
#define SNOR_MFR_XMC		0x20
#define SNOR_MFR_XTX		0x0b
#define SNOR_MFR_PUYA		0x85
#define SNOR_MFR_ZETTA		0xba
#define SNOR_MFR_BOYA		0x68
#define SNOR_MFR_EON		CFI_MFR_EON
#define SNOR_MFR_FM             0xa1
#define SNOR_MFR_MXIC		0xc2

/*
 * GFX Device IDs
 *
 * Manufacturer ID is equal to EON,so define Device id to distinguish.
 */
#define GM_64A_ID  	0x4017
#define GM_128A_ID	 0x4018

/*
 * Note on opcode nomenclature: some opcodes have a format like
 * SPINOR_OP_FUNCTION{4,}_x_y_z. The numbers x, y, and z stand for the number
 * of I/O lines used for the opcode, address, and data (respectively). The
 * FUNCTION has an optional suffix of '4', to represent an opcode which
 * requires a 4-byte (32-bit) address.
 */

/* Flash opcodes. */
#define SPINOR_OP_WREN		0x06	/* Write enable */
#define SPINOR_OP_RDSR		0x05	/* Read status register */
#define SPINOR_OP_WRSR		0x01	/* Write status register 1 byte */
#define SPINOR_OP_RDSR2		0x35	/* Read status register 2 */
#define SPINOR_OP_WRSR2		0x31	/* Write status register 2 */
#define SPINOR_OP_RDSR3		0x15	/* Read status register 3 */
#define SPINOR_OP_WRSR3		0x11	/* Write status register 3 */
#define SPINOR_OP_READ		0x03	/* Read data bytes (low frequency) */
#define SPINOR_OP_READ_FAST	0x0b	/* Read data bytes (high frequency) */
#define SPINOR_OP_READ_1_1_2	0x3b	/* Read data bytes (Dual Output SPI) */
#define SPINOR_OP_READ_1_2_2	0xbb	/* Read data bytes (Dual I/O SPI) */
#define SPINOR_OP_READ_1_1_4	0x6b	/* Read data bytes (Quad Output SPI) */
#define SPINOR_OP_READ_1_4_4	0xeb	/* Read data bytes (Quad I/O SPI) */
#define SPINOR_OP_READ_1_1_8	0x8b	/* Read data bytes (Octal Output SPI) */
#define SPINOR_OP_READ_1_8_8	0xcb	/* Read data bytes (Octal I/O SPI) */
#define SPINOR_OP_READ_UID	0x4b	/* Read the UID of NOR */
#define SPINOR_OP_PP		0x02	/* Page program (up to 256 bytes) */
#define SPINOR_OP_PP_1_1_4	0x32	/* Quad page program */
#define SPINOR_OP_PP_1_4_4	0x38	/* Quad page program */
#define SPINOR_OP_PP_1_1_8	0x82	/* Octal page program */
#define SPINOR_OP_PP_1_8_8	0xc2	/* Octal page program */
#define SPINOR_OP_BE_4K		0x20	/* Erase 4KiB block */
#define SPINOR_OP_BE_4K_PMC	0xd7	/* Erase 4KiB block on PMC chips */
#define SPINOR_OP_BE_32K	0x52	/* Erase 32KiB block */
#define SPINOR_OP_CHIP_ERASE	0xc7	/* Erase whole flash chip */
#define SPINOR_OP_SE		0xd8	/* Sector erase (usually 64KiB) */
#define SPINOR_OP_RDID		0x9f	/* Read JEDEC ID */
#define SPINOR_OP_RDSFDP	0x5a	/* Read SFDP */
#define SPINOR_OP_RDCR		0x35	/* Read configuration register */
#define SPINOR_OP_WRCR		0x31	/* Write configuration register */
#define SPINOR_OP_RDFSR		0x70	/* Read flag status register */
#define SPINOR_OP_DIESEL	0xc2	/* Software Die Select */
#define SPINOR_OP_RESTEN	0x66	/* Reset enable */
#define SPINOR_OP_RESET  	0x99	/* Reset device */
#define SPINOR_OP_CLFSR		0x50	/* Clear flag status register */
#define SPINOR_OP_WREAR		0xc5	/* Write Extended Address Register */

/* 4-byte address opcodes - used on Spansion and some Macronix flashes. */
#define SPINOR_OP_READ4		0x13	/* Read data bytes (low frequency) */
#define SPINOR_OP_READ4_FAST	0x0c	/* Read data bytes (high frequency) */
#define SPINOR_OP_READ4_1_1_2	0x3c	/* Read data bytes (Dual SPI) */
#define SPINOR_OP_READ4_1_2_2	0xbc	/* Read data bytes (Dual I/O SPI) */
#define SPINOR_OP_READ4_1_1_4	0x6c	/* Read data bytes (Quad SPI) */
#define SPINOR_OP_READ4_1_4_4	0xec	/* Read data bytes (Quad I/O SPI) */
#define SPINOR_OP_READ4_1_1_8	0x7c	/* Read data bytes (Octal Output SPI) */
#define SPINOR_OP_READ4_1_8_8	0xcc	/* Read data bytes (Octal I/O SPI) */
#define SPINOR_OP_PP_4B		0x12	/* Page program (up to 256 bytes) */
#define SPINOR_OP_PP_1_1_4_4B	0x34	/* Quad page program */
#define SPINOR_OP_PP_1_4_4_4B	0x3e	/* Quad page program */
#define SPINOR_OP_PP_1_1_8_4B	0x84    /* OCTAL page program */
#define SPINOR_OP_PP_1_8_8_4B	0x8e    /* OCTAL page program */
#define SPINOR_OP_BE_4K_4B	0x21	/* Erase 4KiB block */
#define SPINOR_OP_BE_32K_4B	0x5c	/* Erase 32KiB block */
#define SPINOR_OP_SE_4B		0xdc	/* Sector erase (usually 64KiB) */

/* Double Transfer Rate opcodes - defined in JEDEC JESD216B. */
#define SPINOR_OP_READ_1_1_1_DTR	0x0d
#define SPINOR_OP_READ_1_2_2_DTR	0xbd
#define SPINOR_OP_READ_1_4_4_DTR	0xed

#define SPINOR_OP_READ_1_1_1_DTR_4B	0x0e
#define SPINOR_OP_READ_1_2_2_DTR_4B	0xbe
#define SPINOR_OP_READ_1_4_4_DTR_4B	0xee

/* Used for SST flashes only. */
#define SPINOR_OP_BP		0x02	/* Byte program */
#define SPINOR_OP_WRDI		0x04	/* Write disable */
#define SPINOR_OP_AAI_WP	0xad	/* Auto address increment word program */

/* Used for S3AN flashes only */
#define SPINOR_OP_XSE		0x50	/* Sector erase */
#define SPINOR_OP_XPP		0x82	/* Page program */
#define SPINOR_OP_XRDSR		0xd7	/* Read status register */

#define XSR_PAGESIZE		BIT(0)	/* Page size in Po2 or Linear */
#define XSR_RDY			BIT(7)	/* Ready */

/* Used for Macronix and Winbond flashes. */
#define SPINOR_OP_EN4B		0xb7	/* Enter 4-byte mode */
#define SPINOR_OP_EX4B		0xe9	/* Exit 4-byte mode */

/* Used for Spansion flashes only. */
#define SPINOR_OP_BRWR		0x17	/* Bank register write */
#define SPINOR_OP_CLSR		0x30	/* Clear status register 1 */

/* Used for Micron flashes only. */
#define SPINOR_OP_RD_EVCR      0x65    /* Read EVCR register */
#define SPINOR_OP_WD_EVCR      0x61    /* Write EVCR register */

/* Used for individual lock*/
#define SPINOR_OP_IBLK          0x36    /* Lock individual block */
#define SPINOR_OP_UIBLK         0x39    /* Unlock individual block */
#define SPINOR_OP_RDBLK         0x3d    /* Read block lock */
#define SPINOR_OP_GBLK          0x7e    /* Lock global block */
#define SPINOR_OP_UGBLK         0x98    /* Unlock global block */
#define SR_WPS_EN_WINBOND	BIT(2)
#define SR_WPS_EN_FM		BIT(3)
#define SR_WPS_EN_XTX		BIT(4)

/* Used for MXIC individual lock*/
#define SPINOR_OP_MXICWBLK	 0xe1    /* Lock MXIC DPB individual block */
#define SPINOR_OP_MXICRBLK	 0xe0    /* Unlock MXIC DPB individual block */

/* Used for MXIC nor flash*/
#define SPINOR_OP_USPB	 0xe4    /* Unused SPB lock */
#define SPINOR_OP_RDSCUR	 0x2b    /* read security regisetr */
#define SR_WPSEL	 BIT(7)    /* WPSEL bit */

/* Used for secutity rigister */
#define SR_OP_PROGRAM		 0x42    /* common sr write op */
#define SR_OP_ERASE		 0x44    /* common sr erase op */
#define SR_OP_READ		 0x48    /* common sr read op */
#define LB_OFS			 2      /* LB bit offset */


/* use for EON flashes*/
#define SPINOR_OP_WREN_VSR      0x50    /* Write enable for Volatile Status Register */
#define SPINOR_OP_RDCR_EON	0x09	/* Read EON configuration register */
#define SPINOR_OP_EXIT_OTP	0x04	/* exit otp mode*/
#define SPINOR_OP_ENTER_OTP	0x3a	/* enter otp mode*/
#define SR_OTP_WXDIS_EN_EON	BIT(6)  /* status register WXDIS*/

/* Used for XTX flashes only */
#define SPINOR_OP_READ_UID_XTX	0x5a	/* Read the UID of NOR */

/* Status Register bits. */
#define SR_WIP			BIT(0)	/* Write in progress */
#define SR_WEL			BIT(1)	/* Write enable latch */
/* meaning of other SR_* bits may differ between vendors */
#define SR_BP0			BIT(2)	/* Block protect 0 */
#define SR_BP1			BIT(3)	/* Block protect 1 */
#define SR_BP2			BIT(4)	/* Block protect 2 */
#define SR_BP3			BIT(5)	/* Block protect 3 */
#define SR_BP4			BIT(6)	/* Block protect 4 */
#define SR_TB			BIT(5)	/* Top/Bottom protect */
#define SR_SRWD			BIT(7)	/* SR write protect */
/* Spansion/Cypress specific status bits */
#define SR_E_ERR		BIT(5)
#define SR_P_ERR		BIT(6)

#define SR_QUAD_EN_MX		BIT(6)	/* Macronix Quad I/O */
#define CR_TB_MX		BIT(3)  /* Macronix Top/Bottom protect */
#define SCUR_WPSEL_MX           BIT(7)  /* Macronix WPSEL bit */
#define OTP_SR_TB_EON		BIT(3)	/* Eon Top/Bottom protect */
#define SR2_CMP_GD		BIT(6)  /* Gigadevice CMP bit */

/* Enhanced Volatile Configuration Register bits */
#define EVCR_QUAD_EN_MICRON	BIT(7)	/* Micron Quad I/O */

/* Flag Status Register bits */
#define FSR_READY		BIT(7)
#define FSR_E_ERR		BIT(5)	/* Erase operation status */
#define FSR_P_ERR		BIT(4)	/* Program operation status */
#define FSR_PT_ERR		BIT(1)	/* Protection error bit */

/* Configuration Register bits. */
#define CR_QUAD_EN_GD		BIT(1)  /* Gd Quad I/O */
#define CR_QUAD_EN_EON		BIT(1)  /* Gd Quad I/O */

/* Configuration Register bits. */
#define CR_QUAD_EN_SPAN		BIT(1)	/* Spansion Quad I/O */

/* Status Register 2 bits. */
#define SR2_QUAD_EN_BIT7	BIT(7)

/* Supported SPI protocols */
#define SNOR_PROTO_INST_MASK	GENMASK(23, 16)
#define SNOR_PROTO_INST_SHIFT	16
#define SNOR_PROTO_INST(_nbits)	\
	((((unsigned long)(_nbits)) << SNOR_PROTO_INST_SHIFT) & \
	 SNOR_PROTO_INST_MASK)

#define SNOR_PROTO_ADDR_MASK	GENMASK(15, 8)
#define SNOR_PROTO_ADDR_SHIFT	8
#define SNOR_PROTO_ADDR(_nbits)	\
	((((unsigned long)(_nbits)) << SNOR_PROTO_ADDR_SHIFT) & \
	 SNOR_PROTO_ADDR_MASK)

#define SNOR_PROTO_DATA_MASK	GENMASK(7, 0)
#define SNOR_PROTO_DATA_SHIFT	0
#define SNOR_PROTO_DATA(_nbits)	\
	((((unsigned long)(_nbits)) << SNOR_PROTO_DATA_SHIFT) & \
	 SNOR_PROTO_DATA_MASK)

#define SNOR_PROTO_IS_DTR	BIT(24)	/* Double Transfer Rate */

#define SNOR_PROTO_STR(_inst_nbits, _addr_nbits, _data_nbits)	\
	(SNOR_PROTO_INST(_inst_nbits) |				\
	 SNOR_PROTO_ADDR(_addr_nbits) |				\
	 SNOR_PROTO_DATA(_data_nbits))
#define SNOR_PROTO_DTR(_inst_nbits, _addr_nbits, _data_nbits)	\
	(SNOR_PROTO_IS_DTR |					\
	 SNOR_PROTO_STR(_inst_nbits, _addr_nbits, _data_nbits))

enum spi_nor_protocol {
	SNOR_PROTO_1_1_1 = SNOR_PROTO_STR(1, 1, 1),
	SNOR_PROTO_1_1_2 = SNOR_PROTO_STR(1, 1, 2),
	SNOR_PROTO_1_1_4 = SNOR_PROTO_STR(1, 1, 4),
	SNOR_PROTO_1_1_8 = SNOR_PROTO_STR(1, 1, 8),
	SNOR_PROTO_1_2_2 = SNOR_PROTO_STR(1, 2, 2),
	SNOR_PROTO_1_4_4 = SNOR_PROTO_STR(1, 4, 4),
	SNOR_PROTO_1_8_8 = SNOR_PROTO_STR(1, 8, 8),
	SNOR_PROTO_2_2_2 = SNOR_PROTO_STR(2, 2, 2),
	SNOR_PROTO_4_4_4 = SNOR_PROTO_STR(4, 4, 4),
	SNOR_PROTO_8_8_8 = SNOR_PROTO_STR(8, 8, 8),

	SNOR_PROTO_1_1_1_DTR = SNOR_PROTO_DTR(1, 1, 1),
	SNOR_PROTO_1_2_2_DTR = SNOR_PROTO_DTR(1, 2, 2),
	SNOR_PROTO_1_4_4_DTR = SNOR_PROTO_DTR(1, 4, 4),
	SNOR_PROTO_1_8_8_DTR = SNOR_PROTO_DTR(1, 8, 8),
};

static inline bool spi_nor_protocol_is_dtr(enum spi_nor_protocol proto)
{
	return !!(proto & SNOR_PROTO_IS_DTR);
}

static inline u8 spi_nor_get_protocol_inst_nbits(enum spi_nor_protocol proto)
{
	return ((unsigned long)(proto & SNOR_PROTO_INST_MASK)) >>
		SNOR_PROTO_INST_SHIFT;
}

static inline u8 spi_nor_get_protocol_addr_nbits(enum spi_nor_protocol proto)
{
	return ((unsigned long)(proto & SNOR_PROTO_ADDR_MASK)) >>
		SNOR_PROTO_ADDR_SHIFT;
}

static inline u8 spi_nor_get_protocol_data_nbits(enum spi_nor_protocol proto)
{
	return ((unsigned long)(proto & SNOR_PROTO_DATA_MASK)) >>
		SNOR_PROTO_DATA_SHIFT;
}

static inline u8 spi_nor_get_protocol_width(enum spi_nor_protocol proto)
{
	return spi_nor_get_protocol_data_nbits(proto);
}

/*For Spinor Debugfs*/
#define OCTAL_MODE	(8)
#define QUAD_MODE	(4)
#define DUAL_MODE	(2)
#define SINGLE_MODE	(1)
struct spinordbg_data {
	char param[32];
	char value[32];
	char status[512];
};

enum read_mode {
	SPI_NOR_NORMAL = 0,
	SPI_NOR_FAST,
	SPI_NOR_DUAL,
	SPI_NOR_QUAD,
	SPI_NOR_OCTAL,
};

#define SPI_NOR_MAX_CMD_SIZE	8
enum spi_nor_ops {
	SPI_NOR_OPS_READ = 0,
	SPI_NOR_OPS_WRITE,
	SPI_NOR_OPS_ERASE,
	SPI_NOR_OPS_LOCK,
	SPI_NOR_OPS_UNLOCK,
};

enum spi_nor_option_flags {
	SNOR_F_USE_FSR		= BIT(0),
	SNOR_F_HAS_SR_TB	= BIT(1),
	SNOR_F_NO_OP_CHIP_ERASE	= BIT(2),
	SNOR_F_READY_XSR_RDY	= BIT(3),
	SNOR_F_USE_CLSR		= BIT(4),
	SNOR_F_BROKEN_RESET	= BIT(5),
	SNOR_F_4B_OPCODES	= BIT(6),
	SNOR_F_HAS_4BAIT	= BIT(7),
	SNOR_F_HAS_LOCK		= BIT(8),
	SNOR_F_INDIVIDUAL_LOCK	= BIT(9),
	SNOR_F_HAS_LOCK_HANDLE	= BIT(10),
};

/**
 * struct spi_nor_erase_type - Structure to describe a SPI NOR erase type
 * @size:		the size of the sector/block erased by the erase type.
 *			JEDEC JESD216B imposes erase sizes to be a power of 2.
 * @size_shift:		@size is a power of 2, the shift is stored in
 *			@size_shift.
 * @size_mask:		the size mask based on @size_shift.
 * @opcode:		the SPI command op code to erase the sector/block.
 * @idx:		Erase Type index as sorted in the Basic Flash Parameter
 *			Table. It will be used to synchronize the supported
 *			Erase Types with the ones identified in the SFDP
 *			optional tables.
 */
struct spi_nor_erase_type {
	u32	size;
	u32	size_shift;
	u32	size_mask;
	u8	opcode;
	u8	idx;
};

/**
 * struct spi_nor_erase_command - Used for non-uniform erases
 * The structure is used to describe a list of erase commands to be executed
 * once we validate that the erase can be performed. The elements in the list
 * are run-length encoded.
 * @list:		for inclusion into the list of erase commands.
 * @count:		how many times the same erase command should be
 *			consecutively used.
 * @size:		the size of the sector/block erased by the command.
 * @opcode:		the SPI command op code to erase the sector/block.
 */
struct spi_nor_erase_command {
	struct list_head	list;
	u32			count;
	u32			size;
	u8			opcode;
};

/**
 * struct spi_nor_erase_region - Structure to describe a SPI NOR erase region
 * @offset:		the offset in the data array of erase region start.
 *			LSB bits are used as a bitmask encoding flags to
 *			determine if this region is overlaid, if this region is
 *			the last in the SPI NOR flash memory and to indicate
 *			all the supported erase commands inside this region.
 *			The erase types are sorted in ascending order with the
 *			smallest Erase Type size being at BIT(0).
 * @size:		the size of the region in bytes.
 */
struct spi_nor_erase_region {
	u64		offset;
	u64		size;
};

#define SNOR_ERASE_TYPE_MAX	4
#define SNOR_ERASE_TYPE_MASK	GENMASK_ULL(SNOR_ERASE_TYPE_MAX - 1, 0)

#define SNOR_LAST_REGION	BIT(4)
#define SNOR_OVERLAID_REGION	BIT(5)

#define SNOR_ERASE_FLAGS_MAX	6
#define SNOR_ERASE_FLAGS_MASK	GENMASK_ULL(SNOR_ERASE_FLAGS_MAX - 1, 0)

/**
 * struct spi_nor_erase_map - Structure to describe the SPI NOR erase map
 * @regions:		array of erase regions. The regions are consecutive in
 *			address space. Walking through the regions is done
 *			incrementally.
 * @uniform_region:	a pre-allocated erase region for SPI NOR with a uniform
 *			sector size (legacy implementation).
 * @erase_type:		an array of erase types shared by all the regions.
 *			The erase types are sorted in ascending order, with the
 *			smallest Erase Type size being the first member in the
 *			erase_type array.
 * @uniform_erase_type:	bitmask encoding erase types that can erase the
 *			entire memory. This member is completed at init by
 *			uniform and non-uniform SPI NOR flash memories if they
 *			support at least one erase type that can erase the
 *			entire memory.
 */
struct spi_nor_erase_map {
	struct spi_nor_erase_region	*regions;
	struct spi_nor_erase_region	uniform_region;
	struct spi_nor_erase_type	erase_type[SNOR_ERASE_TYPE_MAX];
	u8				uniform_erase_type;
};

/**
 * struct spi_nor_hwcaps - Structure for describing the hardware capabilies
 * supported by the SPI controller (bus master).
 * @mask:		the bitmask listing all the supported hw capabilies
 */
struct spi_nor_hwcaps {
	u32	mask;
};

/*
 *(Fast) Read capabilities.
 * MUST be ordered by priority: the higher bit position, the higher priority.
 * As a matter of performances, it is relevant to use Octal SPI protocols first,
 * then Quad SPI protocols before Dual SPI protocols, Fast Read and lastly
 * (Slow) Read.
 */
#define SNOR_HWCAPS_READ_MASK		GENMASK(14, 0)
#define SNOR_HWCAPS_READ		BIT(0)
#define SNOR_HWCAPS_READ_FAST		BIT(1)
#define SNOR_HWCAPS_READ_1_1_1_DTR	BIT(2)

#define SNOR_HWCAPS_READ_DUAL		GENMASK(6, 3)
#define SNOR_HWCAPS_READ_1_1_2		BIT(3)
#define SNOR_HWCAPS_READ_1_2_2		BIT(4)
#define SNOR_HWCAPS_READ_2_2_2		BIT(5)
#define SNOR_HWCAPS_READ_1_2_2_DTR	BIT(6)

#define SNOR_HWCAPS_READ_QUAD		GENMASK(10, 7)
#define SNOR_HWCAPS_READ_1_1_4		BIT(7)
#define SNOR_HWCAPS_READ_1_4_4		BIT(8)
#define SNOR_HWCAPS_READ_4_4_4		BIT(9)
#define SNOR_HWCAPS_READ_1_4_4_DTR	BIT(10)

#define SNOR_HWCAPS_READ_OCTAL		GENMASK(14, 11)
#define SNOR_HWCAPS_READ_1_1_8		BIT(11)
#define SNOR_HWCAPS_READ_1_8_8		BIT(12)
#define SNOR_HWCAPS_READ_8_8_8		BIT(13)
#define SNOR_HWCAPS_READ_1_8_8_DTR	BIT(14)

/*
 * Page Program capabilities.
 * MUST be ordered by priority: the higher bit position, the higher priority.
 * Like (Fast) Read capabilities, Octal/Quad SPI protocols are preferred to the
 * legacy SPI 1-1-1 protocol.
 * Note that Dual Page Programs are not supported because there is no existing
 * JEDEC/SFDP standard to define them. Also at this moment no SPI flash memory
 * implements such commands.
 */
#define SNOR_HWCAPS_PP_MASK	GENMASK(22, 16)
#define SNOR_HWCAPS_PP		BIT(16)

#define SNOR_HWCAPS_PP_QUAD	GENMASK(19, 17)
#define SNOR_HWCAPS_PP_1_1_4	BIT(17)
#define SNOR_HWCAPS_PP_1_4_4	BIT(18)
#define SNOR_HWCAPS_PP_4_4_4	BIT(19)

#define SNOR_HWCAPS_PP_OCTAL	GENMASK(22, 20)
#define SNOR_HWCAPS_PP_1_1_8	BIT(20)
#define SNOR_HWCAPS_PP_1_8_8	BIT(21)
#define SNOR_HWCAPS_PP_8_8_8	BIT(22)

#define SNOR_HWCAPS_X_X_X	(SNOR_HWCAPS_READ_2_2_2 |	\
				 SNOR_HWCAPS_READ_4_4_4 |	\
				 SNOR_HWCAPS_READ_8_8_8 |	\
				 SNOR_HWCAPS_PP_4_4_4 |		\
				 SNOR_HWCAPS_PP_8_8_8)

#define SNOR_HWCAPS_DTR		(SNOR_HWCAPS_READ_1_1_1_DTR |	\
				 SNOR_HWCAPS_READ_1_2_2_DTR |	\
				 SNOR_HWCAPS_READ_1_4_4_DTR |	\
				 SNOR_HWCAPS_READ_1_8_8_DTR)

#define SNOR_HWCAPS_ALL		(SNOR_HWCAPS_READ_MASK |	\
				 SNOR_HWCAPS_PP_MASK)

struct spi_nor_read_command {
	u8			num_mode_clocks;
	u8			num_wait_states;
	u8			opcode;
	enum spi_nor_protocol	proto;
};

struct spi_nor_pp_command {
	u8			opcode;
	enum spi_nor_protocol	proto;
};

enum spi_nor_read_command_index {
	SNOR_CMD_READ,
	SNOR_CMD_READ_FAST,
	SNOR_CMD_READ_1_1_1_DTR,

	/* Dual SPI */
	SNOR_CMD_READ_1_1_2,
	SNOR_CMD_READ_1_2_2,
	SNOR_CMD_READ_2_2_2,
	SNOR_CMD_READ_1_2_2_DTR,

	/* Quad SPI */
	SNOR_CMD_READ_1_1_4,
	SNOR_CMD_READ_1_4_4,
	SNOR_CMD_READ_4_4_4,
	SNOR_CMD_READ_1_4_4_DTR,

	/* Octal SPI */
	SNOR_CMD_READ_1_1_8,
	SNOR_CMD_READ_1_8_8,
	SNOR_CMD_READ_8_8_8,
	SNOR_CMD_READ_1_8_8_DTR,

	SNOR_CMD_READ_MAX
};

enum spi_nor_pp_command_index {
	SNOR_CMD_PP,

	/* Quad SPI */
	SNOR_CMD_PP_1_1_4,
	SNOR_CMD_PP_1_4_4,
	SNOR_CMD_PP_4_4_4,

	/* Octal SPI */
	SNOR_CMD_PP_1_1_8,
	SNOR_CMD_PP_1_8_8,
	SNOR_CMD_PP_8_8_8,

	SNOR_CMD_PP_MAX
};

/* Forward declaration that will be used in 'struct spi_nor_flash_parameter' */
struct spi_nor;

/**
 * struct spi_nor_locking_ops - SPI NOR locking methods
 * @lock:	lock a region of the SPI NOR.
 * @unlock:	unlock a region of the SPI NOR.
 * @is_locked:	check if a region of the SPI NOR is completely locked
 */
struct spi_nor_locking_ops {
	int (*lock)(struct spi_nor *nor, loff_t ofs, uint64_t len);
	int (*unlock)(struct spi_nor *nor, loff_t ofs, uint64_t len);
	int (*is_locked)(struct spi_nor *nor, loff_t ofs, uint64_t len);
};

/**
 * struct spi_nor_flash_parameter - SPI NOR flash parameters and settings.
 * Includes legacy flash parameters and settings that can be overwritten
 * by the spi_nor_fixups hooks, or dynamically when parsing the JESD216
 * Serial Flash Discoverable Parameters (SFDP) tables.
 *
 * @size:		the flash memory density in bytes.
 * @page_size:		the page size of the SPI NOR flash memory.
 * @hwcaps:		describes the read and page program hardware
 *			capabilities.
 * @reads:		read capabilities ordered by priority: the higher index
 *                      in the array, the higher priority.
 * @page_programs:	page program capabilities ordered by priority: the
 *                      higher index in the array, the higher priority.
 * @erase_map:		the erase map parsed from the SFDP Sector Map Parameter
 *                      Table.
 * @quad_enable:	enables SPI NOR quad mode.
 * @set_4byte:		puts the SPI NOR in 4 byte addressing mode.
 * @convert_addr:	converts an absolute address into something the flash
 *                      will understand. Particularly useful when pagesize is
 *                      not a power-of-2.
 * @setup:              configures the SPI NOR memory. Useful for SPI NOR
 *                      flashes that have peculiarities to the SPI NOR standard
 *                      e.g. different opcodes, specific address calculation,
 *                      page size, etc.
 * @locking_ops:	SPI NOR locking methods.
 */
struct spi_nor_flash_parameter {
	u64				size;
	u32				page_size;

	struct spi_nor_hwcaps		hwcaps;
	struct spi_nor_read_command	reads[SNOR_CMD_READ_MAX];
	struct spi_nor_pp_command	page_programs[SNOR_CMD_PP_MAX];

	struct spi_nor_erase_map        erase_map;

	int (*quad_enable)(struct spi_nor *nor);
	int (*set_4byte)(struct spi_nor *nor, bool enable);
	u32 (*convert_addr)(struct spi_nor *nor, u32 addr);
	int (*setup)(struct spi_nor *nor, const struct spi_nor_hwcaps *hwcaps);

	const struct spi_nor_locking_ops *locking_ops;
};

/**
 * struct flash_info - Forward declaration of a structure used internally by
 *		       spi_nor_scan()
 */
struct flash_info;

/**
 * struct spi_nor - Structure for defining a the SPI NOR layer
 * @mtd:		point to a mtd_info structure
 * @lock:		the lock for the read/write/erase/lock/unlock operations
 * @dev:		point to a spi device, or a spi nor controller device.
 * @page_size:		the page size of the SPI NOR
 * @addr_width:		number of address bytes
 * @erase_opcode:	the opcode for erasing a sector
 * @read_opcode:	the read opcode
 * @read_dummy:		the dummy needed by the read operation
 * @program_opcode:	the program opcode
 * @flash_read:		the mode of the read
 * @sst_write_second:	used by the SST write operation
 * @flags:		flag options for the current SPI-NOR (SNOR_F_*)
 * @cmd_buf:		used by the write_reg
 * @prepare:		[OPTIONAL] do some preparations for the
 *			read/write/erase/lock/unlock operations
 * @unprepare:		[OPTIONAL] do some post work after the
 *			read/write/erase/lock/unlock operations
 * @read_reg:		[DRIVER-SPECIFIC] read out the register
 * @write_reg:		[DRIVER-SPECIFIC] write data to the register
 * @read:		[DRIVER-SPECIFIC] read data from the SPI NOR
 * @write:		[DRIVER-SPECIFIC] write data to the SPI NOR
 * @erase:		[DRIVER-SPECIFIC] erase a sector of the SPI NOR
 *			at the offset @offs; if not provided by the driver,
 *			spi-nor will send the erase opcode via write_reg()
 * @flash_lock:		[FLASH-SPECIFIC] lock a region of the SPI NOR
 * @flash_unlock:	[FLASH-SPECIFIC] unlock a region of the SPI NOR
 * @flash_is_locked:	[FLASH-SPECIFIC] check if a region of the SPI NOR is
 *			completely locked
 * @priv:		the private data
 */
struct spi_nor {
	struct mtd_info		mtd;
	struct mutex		lock;
	struct device		*dev;
	struct spi_mem		*spimem;
	u8			*bouncebuf;
	size_t			bouncebuf_size;
	const struct flash_info	*info;
	u32			page_size;
	u8			addr_width;
	u32			n_banks;
	u8			erase_opcode;
	u8			read_opcode;
	u8			read_dummy;
	u8			mode;
	u8			program_opcode;
	enum spi_nor_protocol	read_proto;
	enum spi_nor_protocol	write_proto;
	enum spi_nor_protocol	reg_proto;
	enum read_mode		flash_read;
	bool			sst_write_second;
	u32			flags;
	u8			cmd_buf[SPI_NOR_MAX_CMD_SIZE];

	int (*prepare)(struct spi_nor *nor, enum spi_nor_ops ops);
	void (*unprepare)(struct spi_nor *nor, enum spi_nor_ops ops);
	int (*read_reg)(struct spi_nor *nor, u8 opcode, u8 *buf, int len);
	int (*write_reg)(struct spi_nor *nor, u8 opcode, u8 *buf, int len);

	ssize_t (*read)(struct spi_nor *nor, loff_t from,
			size_t len, u_char *read_buf);
	ssize_t (*write)(struct spi_nor *nor, loff_t to,
			size_t len, const u_char *write_buf);
	int (*erase)(struct spi_nor *nor, loff_t offs);

	int (*clear_sr_bp)(struct spi_nor *nor);

	int (*flash_lock)(struct spi_nor *nor, loff_t ofs, uint64_t len);
	int (*flash_unlock)(struct spi_nor *nor, loff_t ofs, uint64_t len);
	int (*flash_is_locked)(struct spi_nor *nor, loff_t ofs, uint64_t len);
	int (*flash_select_bank)(struct spi_nor *nor, loff_t ofs, u32 ids);
	struct spi_nor_flash_parameter params;

	void *priv;
};

static u64 __maybe_unused
spi_nor_region_is_last(const struct spi_nor_erase_region *region)
{
	return region->offset & SNOR_LAST_REGION;
}

static u64 __maybe_unused
spi_nor_region_end(const struct spi_nor_erase_region *region)
{
	return (region->offset & ~SNOR_ERASE_FLAGS_MASK) + region->size;
}

static void __maybe_unused
spi_nor_region_mark_end(struct spi_nor_erase_region *region)
{
	region->offset |= SNOR_LAST_REGION;
}

static void __maybe_unused
spi_nor_region_mark_overlay(struct spi_nor_erase_region *region)
{
	region->offset |= SNOR_OVERLAID_REGION;
}

static bool __maybe_unused spi_nor_has_uniform_erase(const struct spi_nor *nor)
{
	return !!nor->params.erase_map.uniform_erase_type;
}

static inline void spi_nor_set_flash_node(struct spi_nor *nor,
					  struct device_node *np)
{
	mtd_set_of_node(&nor->mtd, np);
}

static inline struct device_node *spi_nor_get_flash_node(struct spi_nor *nor)
{
	return mtd_get_of_node(&nor->mtd);
}

/**
 * spi_nor_scan() - scan the SPI NOR
 * @nor:	the spi_nor structure
 * @name:	the chip type name
 * @mode:	the read mode supported by the driver
 *
 * The drivers can use this fuction to scan the SPI NOR.
 * In the scanning, it will try to get all the necessary information to
 * fill the mtd_info{} and the spi_nor{}.
 *
 * The chip type name can be provided through the @name parameter.
 *
 * Return: 0 for success, others for failure.
 */
int spi_nor_scan(struct spi_nor *nor, const char *name, enum read_mode mode);
struct spi_nor *get_spinor(void);

/**
 * spi_nor_restore_addr_mode() - restore the status of SPI NOR
 * @nor:	the spi_nor structure
 */

struct nor_protection {
    unsigned int boundary; /* protected addr [0, boundary) */
    int bp:8;
    int flag:24;
#define SET_TB BIT(0)
#define SET_CMP BIT(1)
};

void spi_nor_restore(struct spi_nor *nor);
int spinor_debug_init(void);

#ifdef CONFIG_SPI_FLASH_SR
/* Support the operation of safety register */
int security_regiser_read_data(struct mtd_info *mtd,
					loff_t addr, loff_t len, u_char *buf);
int security_regiser_write_data(struct mtd_info *mtd,
					loff_t addr, loff_t len, u_char *buf);
int security_regiser_is_locked(struct mtd_info *mtd, u8 sr_num);
int security_register_lock(struct mtd_info *mtd, u8 sr_num);
#endif

#endif
