// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) "sunxi-spinand-phy: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mtd/aw-spinand.h>
#include <linux/mutex.h>
#include <linux/of.h>

#include "physic.h"
#include "../sunxi-spinand.h"

/**
 * aw_spinand_chip_update_cfg() - Update the configuration register
 * @chip: spinand chip structure
 *
 * Return: 0 on success, a negative error code otherwise.
 */
int aw_spinand_chip_update_cfg(struct aw_spinand_chip *chip)
{
	int ret;
	struct aw_spinand_chip_ops *ops = chip->ops;
	struct aw_spinand_info *info = chip->info;
	u8 reg;

	reg = 0;
	if (!strcmp(info->manufacture(chip), "SkyHigh")) {
		ret = ops->set_block_lock(chip, 0x7e);
		if (ret)
			goto err;
		ret = ops->set_block_lock(chip, 0x02);
		if (ret)
			goto err;
	} else {
		ret = ops->set_block_lock(chip, reg);
		if (ret)
			goto err;
	}
	ret = ops->get_block_lock(chip, &reg);
	if (ret)
		goto err;
	pr_info("block lock register: 0x%02x\n", reg);

	ret = ops->get_otp(chip, &reg);
	if (ret) {
		pr_err("get otp register failed: %d\n", ret);
		goto err;
	}
	/* FS35ND01G ECC_EN not on register 0xB0, but on 0x90 */
	if (!strcmp(info->manufacture(chip), "Foresee")) {
		ret = ops->write_reg(chip, SPI_NAND_SETSR, FORESEE_REG_ECC_CFG,
				CFG_ECC_ENABLE);
		if (ret) {
			pr_err("enable ecc for foresee failed: %d\n", ret);
			goto err;
		}
	} else {
		reg |= CFG_ECC_ENABLE;
	}
	if (!strcmp(info->manufacture(chip), "Winbond"))
		reg |= CFG_BUF_MODE;
	if (info->operation_opt(chip) & SPINAND_QUAD_READ ||
			info->operation_opt(chip) & SPINAND_QUAD_PROGRAM)
		reg |= CFG_QUAD_ENABLE;
	if (info->operation_opt(chip) & SPINAND_QUAD_NO_NEED_ENABLE)
		reg &= ~CFG_QUAD_ENABLE;
	ret = ops->set_otp(chip, reg);
	if (ret) {
		pr_err("set otp register failed: val %d, ret %d\n", reg, ret);
		goto err;
	}
	ret = ops->get_otp(chip, &reg);
	if (ret) {
		pr_err("get updated otp register failed: %d\n", ret);
		goto err;
	}
	pr_info("feature register: 0x%02x\n", reg);

	return 0;
err:
	pr_err("update config register failed\n");
	return ret;
}
EXPORT_SYMBOL(aw_spinand_chip_update_cfg);

static void aw_spinand_chip_clean(struct aw_spinand_chip *chip)
{
	aw_spinand_chip_cache_exit(chip);
	aw_spinand_chip_bbt_exit(chip);
}

int aw_spinand_fill_phy_info(struct aw_spinand_chip *chip, void *data)
{
	struct aw_spinand *spinand = get_aw_spinand();
	struct aw_spinand_info *info = chip->info;
	struct aw_spinand_phy_info *pinfo = info->phy_info;
	struct device_node *node = chip->spi->dev.of_node;
	boot_spinand_para_t *boot_info = data;
	int ret;
	unsigned int max_hz;

	ret = of_property_read_u32(node, "spi-max-frequency", &max_hz);
	if (ret < 0)
		pr_err("get spi-max-frequency from node of spi-nand failed\n");

	ret = of_property_read_u32(node, "sample_mode",
				&spinand->right_sample_mode);
	if (ret) {
		pr_err("Failed to get sample mode\n");
		spinand->right_sample_mode = AW_SAMP_MODE_DL_DEFAULT;
	}
	ret = of_property_read_u32(node, "sample_delay",
				&spinand->right_sample_delay);
	if (ret) {
		pr_err("Failed to get sample delay\n");
		spinand->right_sample_delay = AW_SAMP_MODE_DL_DEFAULT;
	}

	/* nand information */
	boot_info->ChipCnt = 1;
	boot_info->ConnectMode = 1;
	boot_info->BankCntPerChip = 1;
	boot_info->DieCntPerChip = pinfo->DieCntPerChip;
	boot_info->PlaneCntPerDie = 2;
	boot_info->SectorCntPerPage = pinfo->SectCntPerPage;
	boot_info->ChipConnectInfo = 1;
	boot_info->PageCntPerPhyBlk = pinfo->PageCntPerBlk;
	boot_info->BlkCntPerDie = pinfo->BlkCntPerDie;
	boot_info->OperationOpt = pinfo->OperationOpt;
	boot_info->FrequencePar = max_hz / 1000 / 1000;
	boot_info->SpiMode = 0;
	info->nandid(chip, boot_info->NandChipId, 8);
	boot_info->pagewithbadflag = pinfo->BadBlockFlag;
	boot_info->MultiPlaneBlockOffset = 1;
	boot_info->MaxEraseTimes = pinfo->MaxEraseTimes;
	/* there is no metter what max ecc bits is */
	boot_info->MaxEccBits = 4;
	boot_info->EccLimitBits = 4;

	boot_info->sample_mode = spinand->right_sample_mode;
	boot_info->sample_delay = spinand->right_sample_delay;

	return 0;
}

static int aw_spinand_chip_init_last(struct aw_spinand_chip *chip)
{
	int ret;
	struct aw_spinand_info *info = chip->info;
	struct device_node *node = chip->spi->dev.of_node;
	unsigned int val;

	/* initialize from spinand information */
	if (info->operation_opt(chip) & SPINAND_QUAD_PROGRAM)
		chip->tx_bit = SPI_NBITS_QUAD;
	else
		chip->tx_bit = SPI_NBITS_SINGLE;

	if (info->operation_opt(chip) & SPINAND_QUAD_READ)
		chip->rx_bit = SPI_NBITS_QUAD;
	else if (info->operation_opt(chip) & SPINAND_DUAL_READ)
		chip->rx_bit = SPI_NBITS_DUAL;
	else
		chip->rx_bit = SPI_NBITS_SINGLE;

	/* re-initialize from device tree */
	ret = of_property_read_u32(node, "spi-rx-bus-width", &val);
	if (!ret && val < chip->rx_bit) {
		pr_info("%s reset rx bit width to %u\n",
				info->model(chip), val);
		chip->rx_bit = val;
	}

	ret = of_property_read_u32(node, "spi-tx-bus-width", &val);
	if (!ret && val < chip->tx_bit) {
		pr_info("%s reset tx bit width to %u\n",
				info->model(chip), val);
		chip->tx_bit = val;
	}

	/* update spinand register */
	ret = aw_spinand_chip_update_cfg(chip);
	if (ret)
		return ret;

	/* do read/write cache init */
	ret = aw_spinand_chip_cache_init(chip);
	if (ret)
		return ret;

	/* do bad block table init */
	ret = aw_spinand_chip_bbt_init(chip);
	if (ret)
		return ret;

	return 0;
}

static int aw_spinand_chip_preinit(struct spi_device *spi,
		struct aw_spinand_chip *chip)
{
	int ret;

	chip->spi = spi;

	ret = aw_spinand_chip_ecc_init(chip);
	if (unlikely(ret))
		return ret;

	ret = aw_spinand_chip_ops_init(chip);
	if (unlikely(ret))
		return ret;

	return 0;
}


int aw_spinand_chip_init(struct spi_device *spi, struct aw_spinand_chip *chip)
{
	int ret;

	pr_info("AW SPINand Phy Layer Version: %x.%x %x\n",
			AW_SPINAND_PHY_VER_MAIN, AW_SPINAND_PHY_VER_SUB,
			AW_SPINAND_PHY_VER_DATE);

	ret = aw_spinand_chip_preinit(spi, chip);
	if (unlikely(ret))
		return ret;

	ret = aw_spinand_chip_detect(chip);
	if (ret)
		return ret;

	ret = aw_spinand_chip_init_last(chip);
	if (ret)
		goto err;

	pr_info("sunxi physic nand init end\n");
	return 0;
err:
	aw_spinand_chip_clean(chip);
	return ret;
}
EXPORT_SYMBOL(aw_spinand_chip_init);

void aw_spinand_chip_exit(struct aw_spinand_chip *chip)
{
	aw_spinand_chip_clean(chip);
}
EXPORT_SYMBOL(aw_spinand_chip_exit);

MODULE_AUTHOR("liaoweixiong <liaoweixiong@allwinnertech.com>");
MODULE_DESCRIPTION("Commond physic layer for Allwinner's spinand driver");
