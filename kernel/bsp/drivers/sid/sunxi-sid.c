/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 *
 * Copyright(c) 2014-2016 Allwinnertech Co., Ltd.
 *         http://www.allwinnertech.com
 *
 * Author: sunny <sunny@allwinnertech.com>
 * Author: superm <superm@allwinnertech.com>
 * Author: Matteo <duanmintao@allwinnertech.com>
 *
 * Allwinner sunxi soc chip version and chip id manager.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/err.h>
#include <sunxi-smc.h>

#include <linux/module.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>

#include <sunxi-sid.h>
#include <linux/acpi.h>

#define SID_DBG(fmt, arg...) pr_debug("%s()%d - "fmt, __func__, __LINE__, ##arg)
#define SID_WARN(fmt, arg...) pr_warn("%s()%d - "fmt, __func__, __LINE__, ##arg)
#define SID_ERR(fmt, arg...) pr_err("%s()%d - "fmt, __func__, __LINE__, ##arg)

#if IS_ENABLED(CONFIG_ARCH_SUN50I)
#define SUNXI_SECURITY_SUPPORT	1
#endif

#define SUNXI_VER_MAX_NUM	8
struct soc_ver_map {
	u32 id;
	u32 rev[SUNXI_VER_MAX_NUM];
};

#define SUNXI_SOC_ID_INDEX		1
#define SUNXI_SECURITY_ENABLE_INDEX	2
struct soc_ver_reg {
	s8 compatile[48];
	u32 offset;
	u32 mask;
	u32 shift;
	struct soc_ver_map ver_map;
};

#define SUNXI_SOC_ID_IN_SID

#if IS_ENABLED(CONFIG_ARCH_SUN50IW6)
#define TYPE_SB (0b001)
#define TYPE_NB (0b010)
#define TYPE_FB (0b011)
#else
#define TYPE_SB (0b001)
#define TYPE_NB (0b011)
#define TYPE_FB (0b111)
#endif

struct efuse_crypt {
	sunxi_efuse_key_info_t key_store;
	struct work_struct work;
	struct completion work_end;
	struct mutex mutex;
	unsigned char *temp_data;
	unsigned int cmd;
	unsigned int ret;
};

typedef struct efuse_crypt efuse_cry_st;
typedef struct efuse_crypt *efuse_cry_pt;

#define SECBLK_READ							_IO('V', 20)
#define SECBLK_WRITE						_IO('V', 21)
#define SECBLK_IOCTL						_IO('V', 22)

struct secblc_op_t {
	int item;
	unsigned char *buf;
	unsigned int len;
};

static unsigned int sunxi_soc_chipid[4];
static unsigned int sunxi_soc_ftzone[4];
static unsigned int sunxi_serial[4];
static int sunxi_soc_secure;
static unsigned int sunxi_soc_bin;
static unsigned int sunxi_soc_ver;
static unsigned int sunxi_platform_id;
static unsigned int sunxi_soc_rotpk_status;
static unsigned int sunxi_sid_nonsec_max_offset;
static unsigned int sunxi_sid_nonsec_max_len;

u32 chip_ver_platform_process(u32 val)
{
#if IS_ENABLED(CONFIG_ARCH_SUN8IW20) || IS_ENABLED(CONFIG_ARCH_SUN20IW1)
	/* c_version is 101 */
	if (val == 0x5) {
		return 3;
	}
#endif
	return val;
}


static s32 sid_get_vir_base(struct device_node **pnode, void __iomem **base,
		s8 *compatible)
{
	*pnode = of_find_compatible_node(NULL, NULL, compatible);
	if (IS_ERR_OR_NULL(*pnode)) {
		SID_ERR("Failed to find \"%s\" in dts.\n", compatible);
		return -ENXIO;
	}

	*base = of_iomap(*pnode, 0); /* reg[0] must be accessible. */
	if (*base == NULL) {
		SID_ERR("Unable to remap IO\n");
		return -ENXIO;
	}
	SID_DBG("Base addr of \"%s\" is %p\n", compatible, *base);
	return 0;
}

static s32  sid_get_phy_base(struct device_node **pnode, phys_addr_t **base,
		s8 *compatible)
{
	struct resource res = {0};
	int ret;
	*pnode = of_find_compatible_node(*pnode, NULL, compatible);
	if (IS_ERR_OR_NULL(*pnode)) {
		SID_ERR("Failed to find \"%s\" in dts.\n", compatible);
		return -ENXIO;
	}

	ret = of_address_to_resource(*pnode, 0, &res);
	if (ret) {
		SID_ERR("ret:%d Failed to get \"%s\"  base address\n", ret, compatible);
		return -ENXIO;
	}
	*base = (phys_addr_t *)res.start;
	SID_DBG("Base addr of \"%s\" is %p\n", compatible, (void *)*base);
	return 0;
}

static s32 sid_get_base(struct device_node **pnode,
		void __iomem **base, s8 *compatible, u32 sec)
{
	if (sec == 1)
		return sid_get_phy_base(pnode,
			(phys_addr_t **)base, compatible);
	else
		return sid_get_vir_base(pnode, base, compatible);
}

static void sid_put_base(struct device_node *pnode, void __iomem *base, u32 sec)
{
	SID_DBG("base = %p, Sec = %d\n", base, sec);
	if ((sec == 0) && (base != NULL))
		iounmap(base);
	if (pnode)
		of_node_put(pnode);
}

static u32 sid_readl(void __iomem *base, u32 sec)
{
	if (sec == 0)
		return readl(base);
	else
		return sunxi_smc_readl((phys_addr_t)base);
}

int get_key_map_info(s8 *name, u8 *compatile, u32 *offset, u32 *max_size)
{
	struct device_node *child_pnode;
	struct device_node *pnode = of_find_compatible_node(NULL, NULL, compatile);
	if (IS_ERR_OR_NULL(pnode)) {
		SID_ERR("Failed to find \"%s\" in dts.\n", compatile);
		return -ENXIO;
	}
	child_pnode = of_get_child_by_name(pnode, name);
	if (IS_ERR_OR_NULL(child_pnode)) {
		SID_ERR("Failed to find \"%s\" in dts.\n", name);
		return -ENXIO;
	}
	of_property_read_u32(child_pnode, "offset", offset);
	of_property_read_u32(child_pnode, "size", max_size);
	return 0;
}


static u32 sid_read_key(s8 *key_name, u32 *key_buf, u32 key_size, u32 sec)
{
	u32 i, offset = 0, max_size = 0;
	void __iomem *baseaddr = NULL;
	struct device_node *dev_node = NULL;

	if (sid_get_base(&dev_node, &baseaddr, EFUSE_SID_BASE, sec))
		return 0;

	get_key_map_info(key_name, EFUSE_SID_BASE, &offset, &max_size);
	SID_DBG("key_name:%s offset:0x%x max_size:0x%x\n", key_name, offset, max_size);
	if (key_size > max_size) {
		key_size = max_size;
	}
	for (i = 0; i < key_size; i += 4) {
		key_buf[i/4] = sid_readl(baseaddr + offset + i, sec);
	}

	sid_put_base(dev_node, baseaddr, sec);
	return 0;
}


static u32 sid_rd_bits(s8 *name, u32 offset, u32 shift, u32 mask, u32 sec)
{
	u32 value = 0;
	void __iomem *baseaddr = NULL;
	struct device_node *dev_node = NULL;

#ifdef SID_REG_READ
	return __sid_reg_read_key(offset);
#else
	if (sid_get_base(&dev_node, &baseaddr, name, sec))
		return 0;

	value = sid_readl(baseaddr + offset, sec);

	value = (value >> shift) & mask;
	SID_DBG("Read \"%s\" + %#x, shift %#x, mask %#x, return %#x, Sec %d\n",
			name, offset, shift, mask, value, sec);

	sid_put_base(dev_node, baseaddr, sec);
	return value;
#endif
}

int get_soc_ver_regs(u8 *name, u8 *compatile, struct soc_ver_reg *reg)
{
	struct device_node *child_pnode;
	struct device_node *pnode = of_find_compatible_node(NULL, NULL, compatile);
	if (IS_ERR_OR_NULL(pnode)) {
		SID_ERR("Failed to find \"%s\" in dts.\n", SRAM_CTRL_BASE);
		return -ENXIO;
	}
	child_pnode = of_get_child_by_name(pnode, name);
	if (IS_ERR_OR_NULL(child_pnode)) {
		SID_ERR("Failed to find \"%s\" in dts.\n", name);
		return -ENXIO;
	}

	of_property_read_u32(child_pnode, "offset", &reg->offset);
	of_property_read_u32(child_pnode, "shift", &reg->shift);
	of_property_read_u32(child_pnode, "mask", &reg->mask);
	of_property_read_u32(child_pnode, "ver_a", &reg->ver_map.rev[0]);
	of_property_read_u32(child_pnode, "ver_b", &reg->ver_map.rev[1]);
	return 0;
}

void sid_rd_ver_reg(u32 id)
{
	s32 i = 0;
	u32 ver = 0;
	static struct soc_ver_reg reg = {0};
	get_soc_ver_regs("soc_ver", SRAM_CTRL_BASE, &reg);
	ver = sid_rd_bits(SRAM_CTRL_BASE, reg.offset,
		reg.shift, reg.mask, 0);
	if (ver >= SUNXI_VER_MAX_NUM/2)
		SID_WARN("ver >= %d, soc ver:%d\n", SUNXI_VER_MAX_NUM/2, ver);
	ver = chip_ver_platform_process(ver);
	sunxi_soc_ver = reg.ver_map.rev[0] + ver;

	SID_DBG("%d-%d: soc_ver %#x\n", i, ver, sunxi_soc_ver);
}

void sid_soc_ver_from_reg_init(void)
{
	static s32 init_flag;
	void __iomem *platform_id_vir;
	unsigned int val = 0;
	static struct soc_ver_reg reg = {0};
	struct device_node *dev_node = NULL;
	void __iomem *baseaddr = NULL;
	ulong platform_id_phy = 0;

	if (sid_get_base(&dev_node, &baseaddr, SRAM_CTRL_BASE, 1)) {
		pr_err("sid_get_base fail \n");
		return;
	}
	get_soc_ver_regs("soc_ver", SRAM_CTRL_BASE, &reg);
	platform_id_phy = (ulong)(baseaddr) + reg.offset;
	platform_id_vir = ioremap(platform_id_phy, 4);

	val = readl(platform_id_vir);
	writel(val | (1 << 15), platform_id_vir);
	val = readl(platform_id_vir);

	sunxi_platform_id = (val & 0xFFFF0007);
	SID_DBG("sunxi_platform_id: %x\n", sunxi_platform_id);
	iounmap(platform_id_vir);
	init_flag = 1;
}

unsigned int sunxi_get_soc_ver_from_reg(void)
{
	sid_soc_ver_from_reg_init();
	return sunxi_platform_id;
}
EXPORT_SYMBOL(sunxi_get_soc_ver_from_reg);

static s32 sid_rd_soc_ver_from_sid(void)
{
	u32 id = 0;
	static struct soc_ver_reg reg = {0};
	get_soc_ver_regs("soc_id", SRAM_CTRL_BASE, &reg);
	id = sid_rd_bits(EFUSE_SID_BASE, reg.offset, reg.shift, reg.mask, 0);
	sid_rd_ver_reg(id);

	return 0;
}

static void sid_soc_ver_init(void)
{
	static s32 init_flag;

	if (init_flag == 1) {
		SID_DBG("It's already inited.\n");
		return;
	}

	sid_rd_soc_ver_from_sid();

	SID_DBG("The SoC version: %#x\n", sunxi_soc_ver);
	init_flag = 1;
}

static void sid_chipid_init(void)
{
	u32 type = 0, offset = 0, max_size;
	static s32 init_flag;
	static struct soc_ver_reg reg = {0};

	if (init_flag == 1) {
		SID_DBG("It's already inited.\n");
		return;
	}
	sid_read_key("chipid", sunxi_soc_chipid, 16, sunxi_soc_is_secure());

	sunxi_serial[0] = sunxi_soc_chipid[3];
	sunxi_serial[1] = sunxi_soc_chipid[2];
	sunxi_serial[2] = (sunxi_soc_chipid[1] >> 16) & 0x0FFFF;

	get_key_map_info("chipid", EFUSE_SID_BASE, &offset, &max_size);
	get_soc_ver_regs("soc_bin", SRAM_CTRL_BASE, &reg);

	type = sid_rd_bits(EFUSE_SID_BASE, reg.offset + offset, reg.shift,
		reg.mask, sunxi_soc_is_secure());

	switch (type) {
	case 0b000001:
		sunxi_soc_bin = 1;
		break;
	case 0b000011:
		sunxi_soc_bin = 2;
		break;
	case 0b000111:
		sunxi_soc_bin = 3;
		break;
	default:
		break;
	}
	SID_DBG("soc bin: %d\n", sunxi_soc_bin);

	init_flag = 1;
}

void sid_ft_zone_init(void)
{
	static s32 init_flag;
	if (init_flag == 1) {
		SID_DBG("It's already inited.\n");
		return;
	}
	sid_read_key(EFUSE_FT_ZONE_NAME, sunxi_soc_ftzone, 0x10, sunxi_soc_is_secure());

	init_flag = 1;

}

void sid_rd_soc_secure_status(void)
{
	static s32 init_flag;
	void __iomem *base = NULL;
	struct device_node *node = NULL;
	u32 offset = 0, max_size;

	if (init_flag == 1) {
		SID_DBG("It's already inited.\n");
		return;
	}

	if (sid_get_base(&node, &base, EFUSE_SID_BASE, 1))
		return;

	get_key_map_info("secure_status", EFUSE_SID_BASE, &offset, &max_size);

	sunxi_soc_secure = ((sunxi_smc_readl((phys_addr_t)(base + offset))) & 0x1);

	sid_put_base(node, base, 1);
	init_flag = 1;
}

void sid_rotpk_status_init(void)
{
	static s32 init_flag;
	if (init_flag == 1) {
		SID_DBG("It's already inited.\n");
		return;
	}
	sid_read_key(EFUSE_ROTPK_NAME, &sunxi_soc_rotpk_status, 4, sunxi_soc_is_secure());

	init_flag = 1;

}

//#define CONFIG_SUNXI_SOC_NAME "sun50iw10"
s32 sunxi_get_platform(s8 *buf, s32 size)
{
	return snprintf(buf, size, "%s", CONFIG_AW_SOC_NAME);
}
EXPORT_SYMBOL(sunxi_get_platform);


/**
 * get module_param:
 * argc[0]---dst buf
 * argc[1]---the sid offset
 * argc[2]---len(btye)
 */
int sunxi_get_module_param_from_sid(u32 *dst, u32 offset, u32 len)
{
	void __iomem *baseaddr = NULL;
	struct device_node *dev_node = NULL;
	int i;

	if (dst == NULL) {
		pr_err("the dst buf is NULL\n");
		return -1;
	}

	if (len & 0x3) {
		pr_err("the len must be word algin\n");
		return -2;
	}

	if (sid_get_base(&dev_node, &baseaddr, EFUSE_SID_BASE, 0)) {
		pr_err("sid_get_base fail \n");
		return 0;
	}

	SID_DBG("baseaddr: 0x%p offset:0x%x len(word):0x%x\n", baseaddr, offset, len);

	for (i = 0; i < (len >> 2); i++) {
		dst[i] = sid_readl(baseaddr + 0x200 + offset + (i * 4), 0);
	}

	sid_put_base(dev_node, baseaddr, 0);

	return 0;
}
EXPORT_SYMBOL_GPL(sunxi_get_module_param_from_sid);

/**
 * soc markid:
 */
unsigned int sunxi_get_soc_markid(void)
{
	unsigned int val;

	sid_chipid_init();
	val = sunxi_soc_chipid[0] & 0xffff;
	return val;
}
EXPORT_SYMBOL(sunxi_get_soc_markid);

/**
 * soc chipid:
 */
int sunxi_get_soc_chipid(u8 *chipid)
{
	sid_chipid_init();
	memcpy(chipid, sunxi_soc_chipid, 16);
	return 0;
}
EXPORT_SYMBOL(sunxi_get_soc_chipid);

/**
 * soc chipid serial:
 */
int sunxi_get_serial(u8 *serial)
{
	sid_chipid_init();
	memcpy(serial, sunxi_serial, 16);
	return 0;
}
EXPORT_SYMBOL(sunxi_get_serial);

/**
 * soc chipid str:
 */
int sunxi_get_soc_chipid_str(char *serial)
{
	size_t size;

	sid_chipid_init();
#if IS_ENABLED(CONFIG_ARCH_SUN50IW9) || IS_ENABLED(CONFIG_ARCH_SUN50IW10) || IS_ENABLED(CONFIG_ARCH_SUN55IW3) || IS_ENABLED(CONFIG_ARCH_SUN8IW21)
	size = sprintf(serial, "%08x", sunxi_soc_chipid[0] & 0xffff);
#else
	size = sprintf(serial, "%08x", sunxi_soc_chipid[0] & 0x0ff);
#endif
	return size;
}
EXPORT_SYMBOL(sunxi_get_soc_chipid_str);

/**
 * soc ft zone str:
 */
int sunxi_get_soc_ft_zone_str(char *serial)
{
	size_t size;

	sid_ft_zone_init();
	size = sprintf(serial, "%08x", (sunxi_soc_ftzone[0] & 0xff000000) >> 24);
	return size;
}
EXPORT_SYMBOL(sunxi_get_soc_ft_zone_str);

/**
 * soc rotpk status str:
 */
int sunxi_get_soc_rotpk_status_str(char *status)
{
	size_t size;

	sid_rotpk_status_init();
	size = sprintf(status, "%d", (sunxi_soc_rotpk_status & 0x3) >> 1);
	return size;
}
EXPORT_SYMBOL(sunxi_get_soc_rotpk_status_str);

/**
 * soc chipid:
 */
int sunxi_soc_is_secure(void)
{
	sid_rd_soc_secure_status();
	return sunxi_soc_secure;
}
EXPORT_SYMBOL(sunxi_soc_is_secure);

/**
 * get sunxi soc bin
 *
 * return: the bin of sunxi soc, like that:
 * 0 : fail
 * 1 : slow
 * 2 : normal
 * 3 : fast
 */
unsigned int sunxi_get_soc_bin(void)
{
	sid_chipid_init();
	return sunxi_soc_bin;
}
EXPORT_SYMBOL(sunxi_get_soc_bin);

unsigned int sunxi_get_soc_ver(void)
{
	sid_soc_ver_init();
	return sunxi_soc_ver;
}
EXPORT_SYMBOL(sunxi_get_soc_ver);

unsigned int sunxi_get_platform_id(void)
{
	unsigned int ret = sunxi_get_soc_ver_from_reg();
	if (ret)
		return ret;
	return sunxi_get_soc_ver();
}
EXPORT_SYMBOL(sunxi_get_platform_id);

s32 sunxi_efuse_readn(s8 *key_name, void *buf, u32 n)
{
	char name[32] = {0};

	if ((key_name == NULL) || (*(s8 *)key_name == 0)
			|| (n == 0) || (buf == NULL)) {
		SID_ERR("Invalid parameter. name: %p, read_buf: %p, size: %d\n",
		key_name, buf, n);
		return -EINVAL;
	}
	WARN_ON(n < 4);

	strncpy(name, key_name, strlen(key_name) - 1);
	sid_read_key(name, buf, n, sunxi_soc_is_secure());
	return 0;
}
EXPORT_SYMBOL(sunxi_efuse_readn);

#define SEC_BLK_SIZE						(4096)

#define SUNXI_EFUSE_READ	_IO('V', 1)
#define SUNXI_EFUSE_WRITE	_IO('V', 2)

static efuse_cry_pt nfcr;
static sunxi_efuse_key_info_t temp_key;

static int sunxi_efuse_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int sunxi_efuse_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long sunxi_efuse_ioctl(struct file *file, unsigned int ioctl_num,
			      unsigned long ioctl_param)
{
	int err = 0;

	mutex_lock(&nfcr->mutex);
	if (copy_from_user(&nfcr->key_store, (void __user *)ioctl_param,
			   sizeof(nfcr->key_store))) {
		err = -EFAULT;
		goto _out;
	}

	/* check user param */
	if (nfcr->key_store.offset > EFUSE_MAX_ADDR_SIZE) {
		err = -EFAULT;
		goto _out;
	}

	if (nfcr->key_store.len > EFUSE_RW_MAX_LEN) {
		err = -EFAULT;
		goto _out;
	}

	if ((nfcr->key_store.offset + nfcr->key_store.len) > EFUSE_MAX_ADDR_SIZE) {
		err = -EFAULT;
		goto _out;
	}

	nfcr->cmd = ioctl_num;

#ifdef SUNXI_SID_DBG
	pr_err("sunxi_efuse_ioctl: ioctl_num=%d\n", ioctl_num);
	pr_err("nfcr = %p\n", nfcr);
	pr_err("name = %s\n", nfcr->key_store.name);
	pr_err("len = %d\n", nfcr->key_store.len);
#endif

	switch (ioctl_num) {
	case SUNXI_EFUSE_READ:
		schedule_work(&nfcr->work);
		wait_for_completion(&nfcr->work_end);
		/* sunxi_dump(nfcr->temp_data, 50); */
		err = nfcr->ret;
		if (!err) {
			if ((nfcr->key_store.offset >= 0) &&
					(nfcr->key_store.offset < sunxi_sid_nonsec_max_offset)) {
				if (copy_to_user(
					(void __user *)(phys_addr_t)
					    nfcr->key_store.key_data,
					(nfcr->temp_data),
					nfcr->key_store.len)) {
					err = -EFAULT;
					pr_err("copy_to_user: err:%d\n", err);
					goto _out;
				}
			} else {
				if (copy_to_user(
					(void __user *)(phys_addr_t)
					    nfcr->key_store.key_data,
					(nfcr->temp_data + nfcr->key_store.offset),
					nfcr->key_store.len)) {
					err = -EFAULT;
					pr_err("copy_to_user: err:%d\n", err);
					goto _out;
				}

			}
		}
		break;
	case SUNXI_EFUSE_WRITE:
		if (copy_from_user(nfcr->temp_data,
				   (void __user *)(phys_addr_t)
					   nfcr->key_store.key_data,
				   nfcr->key_store.len)) {
			err = -EFAULT;
			pr_err("copy_from_user: err:%d\n", err);
			goto _out;
		}
		snprintf(temp_key.name, sizeof(temp_key.name), "%s", nfcr->key_store.name);
		temp_key.len = nfcr->key_store.len;
		temp_key.offset = nfcr->key_store.offset;
		temp_key.key_data =
			virt_to_phys((const volatile void *)nfcr->temp_data);
		schedule_work(&nfcr->work);
		wait_for_completion(&nfcr->work_end);
		err = nfcr->ret;
		break;
	default:
		pr_err("sunxi_efuse_ioctl: un supported cmd:%d\n", ioctl_num);
		break;
	}
_out:
	memset(nfcr->temp_data, 0, SEC_BLK_SIZE);
	memset(&nfcr->key_store, 0, sizeof(nfcr->key_store));
	nfcr->cmd = -1;
	mutex_unlock(&nfcr->mutex);
	return err;
}

static const struct file_operations sunxi_efuse_ops = {
	.owner = THIS_MODULE,
	.open = sunxi_efuse_open,
	.release = sunxi_efuse_release,
	.unlocked_ioctl = sunxi_efuse_ioctl,
	.compat_ioctl = sunxi_efuse_ioctl,
};

struct miscdevice sunxi_efuse_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sid_efuse",
	.fops = &sunxi_efuse_ops,
};

static void sunxi_efuse_work(struct work_struct *data)
{
	efuse_cry_pt fcpt = container_of(data, struct efuse_crypt, work);
	void __iomem *baseaddr = NULL;
	struct device_node *dev_node = NULL;
	u32 *reg_val;
	int i;

	switch (fcpt->cmd) {
	case SUNXI_EFUSE_READ:
		dev_node = of_find_compatible_node(NULL, NULL, EFUSE_SID_BASE);
		if (IS_ERR_OR_NULL(dev_node)) {
			SID_ERR("Failed to find \"%s\" in dts.\n", EFUSE_SID_BASE);
			fcpt->ret = -1;
			break;
		}

		/* max non_secure key offset from efuse mapping */
		of_property_read_u32(dev_node, "non-secure-maxoffset", &sunxi_sid_nonsec_max_offset);
		/* max non_secure key len(byte) from efuse mapping */
		of_property_read_u32(dev_node, "non-secure-maxlen", &sunxi_sid_nonsec_max_len);
		if ((nfcr->key_store.offset >= 0) &&
				(nfcr->key_store.offset < sunxi_sid_nonsec_max_offset)) {
			if (nfcr->key_store.len <= 0 ||
					nfcr->key_store.len > sunxi_sid_nonsec_max_len) {
				fcpt->ret = -1;
				break;
			}

			if (sid_get_base(&dev_node, &baseaddr, EFUSE_SID_BASE, 0)) {
				fcpt->ret = -1;
				break;
			}

			reg_val = kzalloc(sizeof(*reg_val) * (nfcr->key_store.len >> 2), GFP_KERNEL);

			for (i = 0; i < (nfcr->key_store.len >> 2); i++)
				reg_val[i] = readl(baseaddr + 0x200 + nfcr->key_store.offset + (i * 4));
			memcpy(fcpt->temp_data, reg_val, sizeof(*reg_val) * (nfcr->key_store.len >> 2));

			sid_put_base(dev_node, baseaddr, 0);
			kfree(reg_val);
			fcpt->ret = 0;
		}
#if IS_ENABLED(CONFIG_AW_SMC)
		else {
			fcpt->ret =
				(((nfcr->key_store.offset +
				   nfcr->key_store.len) >
				  (arm_svc_efuse_read(
					  virt_to_phys(
						  (const volatile void *)
							  fcpt->key_store.name),
					  virt_to_phys(
						  (const volatile void *)
							  fcpt->temp_data)))) ?
					 -1 :
					 0);
		}
		break;
	case SUNXI_EFUSE_WRITE:
		fcpt->ret = arm_svc_efuse_write(
			virt_to_phys((const volatile void *)&temp_key));
		break;
#endif
	default:
		fcpt->ret = -1;
		pr_err("sunxi_efuse_work: un supported cmd:%d\n", fcpt->cmd);
		break;
	}

	if ((fcpt->cmd == SUNXI_EFUSE_READ) || (fcpt->cmd == SUNXI_EFUSE_WRITE))
		complete(&fcpt->work_end);
}

static void sunxi_efuse_exit(void)
{
	pr_debug("sunxi efuse driver exit\n");

	misc_deregister(&sunxi_efuse_device);
	kfree(nfcr->temp_data);
	kfree(nfcr);
}

static int sunxi_efuse_init(void)
{
	int ret;

	ret = misc_register(&sunxi_efuse_device);
	if (ret) {
		pr_err("%s: cannot deregister miscdev.(return value-%d)\n",
		       __func__, ret);
		return ret;
	}

	nfcr = kzalloc(sizeof(*nfcr), GFP_KERNEL);
	if (!nfcr)
		return -ENOMEM;

	nfcr->temp_data = kzalloc(SEC_BLK_SIZE, GFP_KERNEL);
	if (!nfcr->temp_data) {
		kfree(nfcr);
		return -ENOMEM;
	}

	INIT_WORK(&nfcr->work, sunxi_efuse_work);
	init_completion(&nfcr->work_end);
	mutex_init(&nfcr->mutex);

	return 0;
}

static int __init sunxi_sid_init(void)
{
	SID_WARN("insmod ok\n");
	return sunxi_efuse_init();
}

static void __exit sunxi_sid_exit(void)
{
	sunxi_efuse_exit();
	SID_WARN("rmmod ok\n");
}

module_init(sunxi_sid_init);
module_exit(sunxi_sid_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("weidonghui<weidonghui@allwinnertech.com>");
MODULE_DESCRIPTION("sunxi sid.");
MODULE_VERSION("1.0.1");
