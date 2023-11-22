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
#include <sunxi-log.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>
#include "aw_hdmi_define.h"
#include "aw_hdmi_drv.h"
#include "aw_hdmi_dev.h"

aw_device_t aw_dev_hdmi;

#if IS_ENABLED(CONFIG_AW_HDMI2_CEC_USER)
static wait_queue_head_t cec_poll_queue;
struct cec_private cec_priv;
DEFINE_MUTEX(file_ops_lock);
DEFINE_MUTEX(cec_ioctl_lock);
#endif

extern struct aw_hdmi_driver *g_hdmi_drv;

#if IS_ENABLED(CONFIG_AW_HDMI2_HDCP22_SUNXI)
extern char *gHdcp_esm_fw_vir_addr;
extern u32 gHdcp_esm_fw_size;
#endif

static u8 reg_region;

/* use for HDMI address range out of range detection */
#define AW_HDMI_REGISTER_RANGE 0X100000 

/**
 * @short of_device_id structure
 */
static const struct of_device_id dw_hdmi_tx[] = {
	{ .compatible =	"allwinner,sunxi-hdmi" },
	{ }
};
MODULE_DEVICE_TABLE(of, dw_hdmi_tx);

/**
 * @short Platform driver structure
 */
static struct platform_driver __refdata dw_hdmi_pdrv = {
	.remove = aw_hdmi_drv_remove,
	.probe  = aw_hdmi_drv_probe,
	.driver = {
		.name = "allwinner,sunxi-hdmi",
		.owner = THIS_MODULE,
		.of_match_table = dw_hdmi_tx,
	},
};

static int __parse_dump_str(const char *buf, size_t size,
				unsigned long *start, unsigned long *end)
{
	char *ptr = NULL;
	char *ptr2 = (char *)buf;
	int ret = 0, times = 0;

	/* Support single address mode, some time it haven't ',' */
next:

	/* Default dump only one register(*start =*end).
	If ptr is not NULL, we will cover the default value of end. */
	if (times == 1)
		*start = *end;

	if (!ptr2 || (ptr2 - buf) >= size)
		goto out;

	ptr = ptr2;
	ptr2 = strnchr(ptr, size - (ptr - buf), ',');
	if (ptr2) {
		*ptr2 = '\0';
		ptr2++;
	}

	ptr = strim(ptr);
	if (!strlen(ptr))
		goto next;

	ret = kstrtoul(ptr, 16, end);
	if (!ret) {
		times++;
		goto next;
	} else {
		hdmi_wrn("String syntax errors: %s\n", ptr);
	}

out:
	return ret;
}

static ssize_t hdmi_hpd_mask_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", aw_hdmi_drv_get_hpd_mask());
}

static ssize_t hdmi_hpd_mask_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err;
	unsigned long val;

	if (count < 1)
		return -EINVAL;

	err = kstrtoul(buf, 16, &val);
	if (err) {
		hdmi_err("%s: parse buf error: %s\n", __func__, buf);
		return err;
	}

	aw_hdmi_drv_set_hpd_mask(val);
	hdmi_inf("set hpd mask: 0x%x\n", (u32)val);

	return count;
}

static DEVICE_ATTR(hpd_mask, 0664,
		hdmi_hpd_mask_show, hdmi_hpd_mask_store);

static ssize_t hdmi_edid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 *edid = NULL;
	u8 *edid_ext = NULL;

	/* get edid base block0 */
	edid = (u8 *)g_hdmi_drv->hdmi_core->mode.edid;
	if (edid == NULL) {
		hdmi_err("%s: edid base point is null!\n", __func__);
		return 0;
	}
	memcpy(buf, edid, 0x80);

	/* get edid extension block */
	edid_ext = (u8 *)g_hdmi_drv->hdmi_core->mode.edid_ext;
	if (edid_ext != NULL) {
		memcpy(buf + 0x80, edid_ext,
			0x80 * ((struct edid *)edid)->extensions);
	}

	if (edid && (!edid_ext))
		return 0x80;
	if (edid && edid_ext)
		return 0x80 * (1 + ((struct edid *)edid)->extensions);
	else
		return 0;
}

static ssize_t hdmi_edid_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	g_hdmi_drv->hdmi_core->edid_ops.set_test_data(buf, count);
	return count;
}

static DEVICE_ATTR(edid, 0664,
		hdmi_edid_show, hdmi_edid_store);

static ssize_t hdmi_edid_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	bool mode = g_hdmi_drv->hdmi_core->edid_ops.get_test_mode();
	return sprintf(buf, "edid test mode: %d.\n", mode);
}

static ssize_t hdmi_edid_test_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	bool state = false;
	if (strncmp(buf, "0", 1) == 0)
		state = false;
	else
		state = true;

	g_hdmi_drv->hdmi_core->edid_ops.set_test_mode(state);

	return count;
}

static DEVICE_ATTR(edid_test, 0664,
		hdmi_edid_test_show, hdmi_edid_test_store);

static ssize_t hdmi_test_reg_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t n = 0;
	int i = 0;

	if (!reg_region) {
		n += sprintf(buf + n, "echo [region_number] > /sys/class/hdmi/hdmi/attr/reg_dump\n");
		n += sprintf(buf + n, "cat /sys/class/hdmi/hdmi/attr/reg_dump\n");
		n += sprintf(buf + n, "region_number:\n");
		n += sprintf(buf + n, "1: Identification Register\n");
		n += sprintf(buf + n, "2: Interrupt Register\n");
		n += sprintf(buf + n, "3: Video Sampler Register\n");
		n += sprintf(buf + n, "4: Video Packetiser Register\n");
		n += sprintf(buf + n, "5: Frame Composer Register\n");
		n += sprintf(buf + n, "6: Audio Sampler Register\n");
		n += sprintf(buf + n, "7: Audio Packertiser Register\n");
		n += sprintf(buf + n, "8: Audio Sample GP Register\n");
		n += sprintf(buf + n, "9: Main Controller Register\n");
		n += sprintf(buf + n, "10: Color Space Conventer Register\n");
		n += sprintf(buf + n, "11: HDCP Register\n");
		n += sprintf(buf + n, "12: HDCP2.2 Register\n");
		n += sprintf(buf + n, "13: CEC Register\n");
		n += sprintf(buf + n, "14: EDID Register\n");
	}

	if (reg_region == 1) {
		n += sprintf(buf + n, "Identification Register:\n");
		n += sprintf(buf + n, "0x0000-0x0006:");
		for (i = 0; i < 7; i++) {
			n += sprintf(buf + n, " 0x%02x", aw_hdmi_drv_read((0x0 + i) * 4));
		}
		n += sprintf(buf + n, "\n");
	}

	if (reg_region == 2) {
		n += sprintf(buf + n, "Interrupt Register:\n");
		n += sprintf(buf + n, "0x0100-0x100f:");
		for (i = 0; i < 0x100; i++) {
			if ((i % 16) == 0 && i != 0) {
				n += sprintf(buf + n, "\n0x%04x-0x%04x:",
						0x100 + i, 0x100 + i + 0x0f);
			}

			n += sprintf(buf + n, " 0x%02x", aw_hdmi_drv_read((0x100 + i) * 4));
		}
		n += sprintf(buf + n, "\n");
	}

	if (reg_region == 3) {
		n += sprintf(buf + n, "Video Sampler Register:\n");
		n += sprintf(buf + n, "0x0200-0x0208:");
		for (i = 0; i < 8; i++) {
			n += sprintf(buf + n, " 0x%02x", aw_hdmi_drv_read((0x200 + i) * 4));
		}
		n += sprintf(buf + n, "\n");
	}

	if (reg_region == 4) {
		n += sprintf(buf + n, "Video Packetiser Register:\n");
		n += sprintf(buf + n, "0x0800-0x0807:");
		for (i = 0; i < 8; i++) {
			n += sprintf(buf + n, " 0x%02x", aw_hdmi_drv_read((0x800 + i) * 4));
		}
		n += sprintf(buf + n, "\n");
	}

	if (reg_region == 5) {
		n += sprintf(buf + n, "Frame Composer Register:\n");
		n += sprintf(buf + n, "0x1000-0x100f:");
		for (i = 0; i < 0x21c; i++) {
			if ((i % 16) == 0 && i != 0) {
				n += sprintf(buf + n, "\n0x%04x-0x%04x:",
					0x1000 + i, 0x1000 + i + 0x0f);
			}

			n += sprintf(buf + n, " 0x%02x", aw_hdmi_drv_read((0x1000 + i) * 4));
		}
		n += sprintf(buf + n, "\n");
	}

	if (reg_region == 6) {
		n += sprintf(buf + n, "Audio Sampler Register:\n");
		n += sprintf(buf + n, "0x3100-0x3104:");
		for (i = 0; i < 5; i++) {
			n += sprintf(buf + n, " 0x%02x", aw_hdmi_drv_read((0x3100 + i) * 4));
		}
		n += sprintf(buf + n, "\n");
	}

	if (reg_region == 7) {

		n += sprintf(buf + n, "Audio Packetiser Register:\n");
		n += sprintf(buf + n, "0x3200-0x3207:");
		for (i = 0; i < 8; i++) {
			n += sprintf(buf + n, " 0x%02x", aw_hdmi_drv_read((0x3200 + i) * 4));
		}
		n += sprintf(buf + n, "\n");
	}

	if (reg_region == 8) {
		n += sprintf(buf + n, "Audio Sample GP Register:\n");
		n += sprintf(buf + n, "0x3500-0x3507:");
		for (i = 0; i < 8; i++) {
			n += sprintf(buf + n, " 0x%02x", aw_hdmi_drv_read((0x3500 + i) * 4));
		}
		n += sprintf(buf + n, "\n");
	}

	if (reg_region == 9) {
		n += sprintf(buf + n, "Main Controller Register:\n");
		n += sprintf(buf + n, "0x4001-0x400b:");
		for (i = 0; i < 0xb; i++) {
			n += sprintf(buf + n, " 0x%02x", aw_hdmi_drv_read((0x4001 + i) * 4));
		}
		n += sprintf(buf + n, "\n");
	}

	if (reg_region == 10) {
		n += sprintf(buf + n, "Color Space Conventer Register:\n");
		n += sprintf(buf + n, "0x4100-0x410f:");
		for (i = 0; i < 0x1e; i++) {
			if ((i % 16) == 0 && i != 0) {
				n += sprintf(buf + n, "\n0x%04x-0x%04x:",
					0x4100 + i, 0x4100 + i + 0x0f);
			}

			n += sprintf(buf + n, " 0x%02x", aw_hdmi_drv_read((0x4100 + i) * 4));
		}
		n += sprintf(buf + n, "\n");
	}

	if (reg_region == 11) {
		n += sprintf(buf + n, "HDCP Register:\n");
		n += sprintf(buf + n, "0x5000-0x500f:");
		for (i = 0; i < 0x20; i++) {
			if ((i % 16) == 0 && i != 0) {
				n += sprintf(buf + n, "\n0x%04x-0x%04x:",
					0x5000 + i, 0x5000 + i + 0x0f);
			}

			n += sprintf(buf + n, " 0x%02x", aw_hdmi_drv_read((0x5000 + i) * 4));
		}
		n += sprintf(buf + n, "\n");
	}

	if (reg_region == 12) {
		n += sprintf(buf + n, "HDCP2.2 Register:\n");
		n += sprintf(buf + n, "0x7900-0x790e:");
		for (i = 0; i < 0x0f; i++) {
			n += sprintf(buf + n, " 0x%02x", aw_hdmi_drv_read((0x7900 + i) * 4));
		}
		n += sprintf(buf + n, "\n");
	}

	if (reg_region == 13) {
		n += sprintf(buf + n, "CEC Register:\n");
		n += sprintf(buf + n, "0x7d00-0x7d0f:");
		for (i = 0; i < 0x14; i++) {
			if ((i % 16) == 0 && i != 0) {
				n += sprintf(buf + n, "\n0x%04x-0x%04x:",
					0x7d00 + i, 0x7d00 + i + 0x0f);
			}

			n += sprintf(buf + n, " 0x%02x", aw_hdmi_drv_read((0x7d00 + i) * 4));
		}
		n += sprintf(buf + n, "\n");
	}

	if (reg_region == 14) {
		n += sprintf(buf + n, "EDID Register:\n");
		n += sprintf(buf + n, "0x7e00-0x7e0f:");
		for (i = 0; i < 0x32; i++) {
			if ((i % 16) == 0 && i != 0) {
				n += sprintf(buf + n, "\n0x%04x-0x%04x:",
					0x7e00 + i, 0x7e00 + i + 0x0f);
			}

			n += sprintf(buf + n, " 0x%02x", aw_hdmi_drv_read((0x7e00 + i) * 4));
		}
		n += sprintf(buf + n, "\n");
	}

	reg_region = 0;
	return n;
}

ssize_t hdmi_test_reg_dump_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	char *end;

	reg_region = (u8)simple_strtoull(buf, &end, 0);

	return count;
}

static DEVICE_ATTR(reg_dump, 0664,
		hdmi_test_reg_dump_show, hdmi_test_reg_dump_store);

static ssize_t hdmi_test_reg_read_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n",
		"echo [0x(address offset), 0x(count)] > read");
}

ssize_t hdmi_test_reg_read_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long start_reg = 0;
	unsigned long read_count = 0;
	u32 i;
	u8 *separator;
	u32 data = 0;

	separator = strchr(buf, ',');
	if (separator != NULL) {
		if (__parse_dump_str(buf, count, &start_reg, &read_count))
			hdmi_err("%s: parse buf error: %s\n", __func__, buf);

		hdmi_inf("start_reg=0x%lx  read_count=%ld\n", start_reg, read_count);
		for (i = 0; i < read_count; i++) {
			hdmi_inf("hdmi_addr_offset: 0x%lx = 0x%x\n", start_reg,
					aw_hdmi_drv_read(start_reg * 4));

			start_reg++;
		}
	} else {
		separator = strchr(buf, ' ');
		if (separator != NULL) {
			start_reg = simple_strtoul(buf, NULL, 0);
			read_count = simple_strtoul(separator + 1, NULL, 0);
			for (i = 0; i < read_count; i += 4) {
				data = (u8)aw_hdmi_drv_read((start_reg + i) * 4);
				data |= ((u8)aw_hdmi_drv_read((start_reg + i + 1) * 4)) << 8;
				data |= ((u8)aw_hdmi_drv_read((start_reg + i + 2) * 4)) << 16;
				data |= ((u8)aw_hdmi_drv_read((start_reg + i + 3) * 4)) << 24;
				if ((i % 16) == 0)
					printk(KERN_CONT "\n0x%08lx: 0x%08x",
							(start_reg + i), data);
				else
					printk(KERN_CONT " 0x%08x", data);
			}
		} else {
			start_reg = simple_strtoul(buf, NULL, 0);
			hdmi_inf("hdmi_addr_offset: 0x%lx = 0x%x\n", start_reg,
					aw_hdmi_drv_read(start_reg * 4));
		}
	}
	hdmi_inf("\n");

	return count;
}

static DEVICE_ATTR(read, 0664,
		hdmi_test_reg_read_show, hdmi_test_reg_read_store);

static ssize_t hdmi_test_reg_write_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", "echo [0x(address offset), 0x(value)] > write");
}

ssize_t hdmi_test_reg_write_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long reg_addr = 0;
	unsigned long value = 0;
	u8 *separator1 = NULL;
	u8 *separator2 = NULL;

	separator1 = strchr(buf, ',');
	separator2 = strchr(buf, ' ');
	if (separator1 != NULL) {
		if (__parse_dump_str(buf, count, &reg_addr, &value))
			hdmi_err("%s: parse buf error: %s\n", __func__, buf);

		hdmi_inf("reg_addr=0x%lx  write_value=0x%lx\n", reg_addr, value);

		if (reg_addr * 4 < 0 || (reg_addr * 4 + 0x4 > AW_HDMI_REGISTER_RANGE)) {
			hdmi_err("%s: register address is out of range!\n", __func__);
			return -EINVAL;
		}

		aw_hdmi_drv_write((reg_addr * 4), value);

		mdelay(1);
		hdmi_inf("after write,reg(%lx)=%x\n", reg_addr,
				aw_hdmi_drv_read(reg_addr * 4));
	} else if (separator2 != NULL) {
		reg_addr = simple_strtoul(buf, NULL, 0);
		value = simple_strtoul(separator2 + 1, NULL, 0);
		hdmi_inf("reg_addr=0x%lx  write_value=0x%lx\n", reg_addr, value);
		aw_hdmi_drv_write((reg_addr * 4), value);

		if (reg_addr * 4 < 0 || (reg_addr * 4 + 0x4 > AW_HDMI_REGISTER_RANGE)) {
			hdmi_err("%s: register address is out of range!\n", __func__);
			return -EINVAL;
		}

		mdelay(1);
		hdmi_inf("after write,red(%lx)=%x\n", reg_addr,
				aw_hdmi_drv_read(reg_addr * 4));
	} else {
		hdmi_err("%s: error input: %s\n", __func__, buf);
		return -EINVAL;
	}

	return count;
}

static DEVICE_ATTR(write, 0664,
		hdmi_test_reg_write_show, hdmi_test_reg_write_store);

static ssize_t phy_write_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	hdmi_inf("OPMODE_PLLCFG-0x06\n");
	hdmi_inf("CKSYMTXCTRL-0x09\n");
	hdmi_inf("PLLCURRCTRL-0x10\n");
	hdmi_inf("VLEVCTRL-0x0E\n");
	hdmi_inf("PLLGMPCTRL-0x15\n");
	hdmi_inf("TXTERM-0x19\n");

	return sprintf(buf, "%s\n", "echo [0x(address offset), 0x(value)] > phy_write");
}

static ssize_t phy_write_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long reg_addr = 0;
	u32 value = 0;
	struct aw_hdmi_core_s *core = NULL;

	core = g_hdmi_drv->hdmi_core;
	if (__parse_dump_str(buf, count, (unsigned long *)&reg_addr, (unsigned long *)&value))
		hdmi_err("%s: parse buf error: %s\n", __func__, buf);

	hdmi_inf("reg_addr=0x%lx  write_value=0x%x\n", reg_addr, value);

	core->phy_ops.phy_write((u8)reg_addr, value);
	return count;
}

static DEVICE_ATTR(phy_write, 0664,
		phy_write_show, phy_write_store);

static ssize_t phy_read_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	hdmi_inf("OPMODE_PLLCFG-0x06\n");
	hdmi_inf("CKSYMTXCTRL-0x09\n");
	hdmi_inf("PLLCURRCTRL-0x10\n");
	hdmi_inf("VLEVCTRL-0x0E\n");
	hdmi_inf("PLLGMPCTRL-0x15\n");
	hdmi_inf("TXTERM-0x19\n");

	return sprintf(buf, "%s\n", "echo [0x(address offset), 0x(count)] > phy_read");
}

ssize_t phy_read_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long start_reg = 0;
	u32 value = 0;
	unsigned long read_count = 0;
	u32 i;
	struct aw_hdmi_core_s *core = g_hdmi_drv->hdmi_core;

	if (__parse_dump_str(buf, count, (unsigned long *)&start_reg, &read_count))
		hdmi_err("%s: parse buf error: %s\n", __func__, buf);

	hdmi_inf("start_reg=0x%lx  read_count=%ld\n", start_reg, read_count);

	for (i = 0; i < read_count; i++) {
		core->phy_ops.phy_read((u8)start_reg, &value);
		hdmi_inf("hdmi_addr_offset: 0x%lx = 0x%x\n", start_reg, (u32)value);
		start_reg++;
	}
	hdmi_inf("\n");

	return count;
}

static DEVICE_ATTR(phy_read, 0664,
		phy_read_show, phy_read_store);

extern u16 i2c_min_ss_scl_low_time;
extern u16 i2c_min_ss_scl_high_time;
static ssize_t hdmi_set_ddc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t n = 0;

	n += sprintf(buf + n, "low:%d high:%d\n", i2c_min_ss_scl_low_time,
		i2c_min_ss_scl_high_time);
	n += sprintf(buf + n, "%s\n",
		"echo [low_time, high_time] > set_ddc");

	return n;
}

ssize_t hdmi_set_ddc_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	char *end;

	i2c_min_ss_scl_low_time = (u16)simple_strtoull(buf, &end, 0);

	if ((*end != ' ') && (*end != ',')) {
		hdmi_err("%s: error separator:%c\n", __func__, *end);
		return count;
	}

	i2c_min_ss_scl_high_time = (u16)simple_strtoull(end + 1, &end, 0);

	hdmi_inf("low:%d  high:%d\n", i2c_min_ss_scl_low_time,
			i2c_min_ss_scl_high_time);
	return count;
}

static DEVICE_ATTR(set_ddc, 0664,
		hdmi_set_ddc_show, hdmi_set_ddc_store);

#ifndef SUPPORT_ONLY_HDMI14
static ssize_t scdc_read_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", "echo [0x(address offset), 0x(count)] > scdc_read");
}

ssize_t scdc_read_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u8 start_reg = 0;
	u8 value = 0;
	unsigned long read_count = 0;
	u32 i;
	struct aw_hdmi_core_s *core = g_hdmi_drv->hdmi_core;

	if (__parse_dump_str(buf, count, (unsigned long *)&start_reg, &read_count))
		hdmi_err("%s: parse buf error: %s\n", __func__, buf);

	hdmi_inf("start_reg=0x%x  read_count=%ld\n", (u32)start_reg, read_count);
	for (i = 0; i < read_count; i++) {
		core->dev_ops.scdc_read(start_reg, 1, &value);
		hdmi_inf("hdmi_addr_offset: 0x%x = 0x%x\n", (u32)start_reg, value);
		start_reg++;
	}
	hdmi_inf("\n");

	return count;
}

static DEVICE_ATTR(scdc_read, 0664,
		scdc_read_show, scdc_read_store);

static ssize_t scdc_write_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", "echo [0x(address offset), 0x(value)] > scdc_write");
}

static ssize_t scdc_write_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u8 reg_addr = 0;
	u8 value = 0;
	struct aw_hdmi_core_s *core = NULL;

	core = g_hdmi_drv->hdmi_core;

	if (__parse_dump_str(buf, count, (unsigned long *)&reg_addr, (unsigned long *)&value))
		hdmi_err("%s: parse buf error: %s\n", __func__, buf);

	hdmi_inf("reg_addr=0x%x  write_value=0x%x\n", reg_addr, value);
	core->dev_ops.scdc_write(reg_addr, 1, &value);
	return count;
}

static DEVICE_ATTR(scdc_write, 0664,
		scdc_write_show, scdc_write_store);
#endif

static ssize_t hdmi_debug_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t n = 0;

	n += sprintf(buf + n, "Current debug=%d\n\n", gHdmi_log_level);

	n += sprintf(buf + n, "hdmi log debug level:\n");
	n += sprintf(buf + n, "debug = 1, print video log\n");
	n += sprintf(buf + n, "debug = 2, print edid log\n");
	n += sprintf(buf + n, "debug = 3, print audio log\n");
	n += sprintf(buf + n, "debug = 4, print video+edid+audio log\n");
	n += sprintf(buf + n, "debug = 5, print cec log\n");
	n += sprintf(buf + n, "debug = 6, print hdcp log\n");
	n += sprintf(buf + n, "debug = 7, print all of the logs above\n");
	n += sprintf(buf + n, "debug = 8, print all of the logs above and trace log\n");

	return n;
}

static ssize_t hdmi_debug_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (count < 1)
		return -EINVAL;

	if (strncmp(buf, "9", 1) == 0)
		gHdmi_log_level = 9;
	else if (strncmp(buf, "8", 1) == 0)
		gHdmi_log_level = 8;
	else if (strncmp(buf, "7", 1) == 0)
		gHdmi_log_level = 7;
	else if (strncmp(buf, "6", 1) == 0)
		gHdmi_log_level = 6;
	else if (strncmp(buf, "5", 1) == 0)
			gHdmi_log_level = 5;
	else if (strncmp(buf, "4", 1) == 0)
		gHdmi_log_level = 4;
	else if (strncmp(buf, "3", 1) == 0)
		gHdmi_log_level = 3;
	else if (strncmp(buf, "2", 1) == 0)
		gHdmi_log_level = 2;
	else if (strncmp(buf, "1", 1) == 0)
		gHdmi_log_level = 1;
	else if (strncmp(buf, "0", 1) == 0)
		gHdmi_log_level = 0;
	else
		hdmi_err("%s: invalid input: %s\n", __func__, buf);

	hdmi_inf("set hdmi debug level: %d\n", gHdmi_log_level);
	return count;
}

static DEVICE_ATTR(debug, 0664,
		hdmi_debug_show, hdmi_debug_store);

#if IS_ENABLED(CONFIG_AW_HDMI2_CEC_SUNXI)
#if (!IS_ENABLED(CONFIG_AW_HDMI2_CEC_USER))
static ssize_t hdmi_one_touch_play_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t hdmi_one_touch_play_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (count < 1)
		return -EINVAL;

	if (strncmp(buf, "1", 1) == 0)
		aw_cec_local_feature_one_touch_play();
	else
		hdmi_err("%s: invalid input: %s\n", __func__, buf);

	return count;
}

static DEVICE_ATTR(one_touch_play, 0664,
		 hdmi_one_touch_play_show, hdmi_one_touch_play_store);

static ssize_t hdmi_cec_local_standby_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	ret = sprintf(buf, "%d\n", (unsigned int)aw_cec_get_local_standby_mask());

	return ret;
}

static ssize_t hdmi_cec_local_standby_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (count < 1)
		return -EINVAL;

	if (strncmp(buf, "1", 1) == 0)
		aw_cec_set_local_standby_mask(true);
	else if (strncmp(buf, "1", 0) == 0)
		aw_cec_set_local_standby_mask(false);
	else
		hdmi_err("%s: invalid input: %s\n", __func__, buf);

	return count;
}

static DEVICE_ATTR(cec_local_standby_mode, 0664,
		hdmi_cec_local_standby_show, hdmi_cec_local_standby_store);
#endif

static ssize_t hdmi_cec_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t n = 0;

	n += aw_cec_drv_dump_info(buf + n);

	return n;
}

static DEVICE_ATTR(cec_dump, 0664,
		hdmi_cec_dump_show, NULL);

static ssize_t hdmi_cec_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", g_hdmi_drv->aw_dts.cec_support);
}

static ssize_t hdmi_cec_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (count < 1)
		return -EINVAL;

	if (strncmp(buf, "1", 1) == 0) {
		g_hdmi_drv->aw_dts.cec_support = 1;
		aw_cec_drv_enable();
	} else {
		g_hdmi_drv->aw_dts.cec_support = 0;
		aw_cec_drv_disable();
	}

	return count;
}

static DEVICE_ATTR(cec_enable, 0664,
		hdmi_cec_enable_show, hdmi_cec_enable_store);
#endif

#if IS_ENABLED(CONFIG_AW_HDMI2_HDCP_SUNXI)
static ssize_t hdmi_hdcp_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char hdcp_type = (char)aw_hdmi_drv_get_hdcp_type(g_hdmi_drv->hdmi_core);

	mutex_lock(&g_hdmi_drv->aw_mutex.lock_hdcp);
	memcpy(buf, &hdcp_type, 1);
	mutex_unlock(&g_hdmi_drv->aw_mutex.lock_hdcp);

	return 1;
}

static ssize_t hdmi_hdcp_type_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static DEVICE_ATTR(hdcp_type, 0664,
		hdmi_hdcp_type_show, hdmi_hdcp_type_store);

static ssize_t hdmi_hdcp_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n",
		g_hdmi_drv->hdmi_core->mode.pHdcp.hdcp_on);
}

static ssize_t hdmi_hdcp_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (count < 1)
		return -EINVAL;

	mutex_lock(&g_hdmi_drv->aw_mutex.lock_hdcp);
	if (strncmp(buf, "1", 1) == 0) {
		aw_hdmi_drv_set_hdcp_state(AW_HDCP_ING);
		aw_hdmi_drv_hdcp_enable(g_hdmi_drv->hdmi_core, 1);
	} else {
		aw_hdmi_drv_hdcp_enable(g_hdmi_drv->hdmi_core, 0);
		aw_hdmi_drv_set_hdcp_state(AW_HDCP_DISABLE);
	}
	mutex_unlock(&g_hdmi_drv->aw_mutex.lock_hdcp);

	return count;
}

static DEVICE_ATTR(hdcp_enable, 0664,
		hdmi_hdcp_enable_show, hdmi_hdcp_enable_store);

static ssize_t hdcp_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u32 count = sizeof(u8);
	u8 statue = aw_hdmi_drv_get_hdcp_state();

	mutex_lock(&g_hdmi_drv->aw_mutex.lock_hdcp);
	memcpy(buf, &statue, count);
	mutex_unlock(&g_hdmi_drv->aw_mutex.lock_hdcp);

	return count;
}

static ssize_t hdcp_status_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (count < 1)
		return -EINVAL;

	return count;
}

static DEVICE_ATTR(hdcp_status, 0664,
		hdcp_status_show, hdcp_status_store);

static ssize_t hdcp_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	ret += sprintf(buf + ret, "\n");
	ret += g_hdmi_drv->hdmi_core->hdcp_ops.hdcp_config_dump(buf + ret);

	return ret;
}

static DEVICE_ATTR(hdcp_dump, 0664,
		hdcp_dump_show, NULL);


#if IS_ENABLED(CONFIG_AW_HDMI2_HDCP22_SUNXI)
static char *esm_addr;
static ssize_t esm_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t n = 0;

	n += sprintf(buf+n, "addr=%p, size=0x%04x\n",
		gHdcp_esm_fw_vir_addr, gHdcp_esm_fw_size);
	esm_addr = gHdcp_esm_fw_vir_addr;

	return n;
}

static ssize_t esm_dump_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (esm_addr == 0 || gHdcp_esm_fw_size == 0) {
		hdmi_err("%s: esm address or fize is zero!!!\n", __func__);
		return -1;
	}

	memcpy(esm_addr, buf, count);
	esm_addr = esm_addr + count;
	hdmi_inf("esm_addr=%p, count=0x%04x\n", esm_addr, (unsigned int)count);
	return count;
}

static DEVICE_ATTR(esm_dump, 0664,
		esm_dump_show, esm_dump_store);

static ssize_t hpi_write_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", "echo [0x(address offset), 0x(value)] > hpi_write");
}

static ssize_t hpi_write_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 reg_addr = 0;
	u32 value = 0;
	struct aw_hdmi_core_s *core = NULL;

	core = g_hdmi_drv->hdmi_core;

	if (__parse_dump_str(buf, count, (unsigned long *)&reg_addr, (unsigned long *)&value))
		hdmi_err("%s: parse buf error: %s\n", __func__, buf);

	hdmi_inf("reg_addr=0x%x  write_value=0x%x\n", reg_addr, value);

	if (reg_addr % 4 || reg_addr < 0 || reg_addr + 0x4 > AW_HDMI_REGISTER_RANGE) {
		hdmi_err("%s: register address is out of range\n", __func__);
		return -EINVAL;
	}

	*((u32 *)(core->mode.pHdcp.esm_hpi_base + reg_addr)) = (u32)value;
	return count;
}

static DEVICE_ATTR(hpi_write, 0664,
		hpi_write_show, hpi_write_store);

static ssize_t hpi_read_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", "echo [0x(address offset)] > hpi_read");
}

ssize_t hpi_read_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 start_reg = 0;
	u32 value = 0;
	char *end;
	struct aw_hdmi_core_s *core = g_hdmi_drv->hdmi_core;

	start_reg = (u32)simple_strtoull(buf, &end, 0);

	if ((*end != ' ') && (*end != ',')) {
		hdmi_err("%s: error separator:%c\n", __func__, *end);
		return count;
	}

	hdmi_inf("start_reg = 0x%x\n", (u32)start_reg);
	value = *((u32 *)(core->mode.pHdcp.esm_hpi_base + start_reg));
	hdmi_inf("hdmi_addr_offset: 0x%x = 0x%x\n", (u32)start_reg, value);

	return count;
}

static DEVICE_ATTR(hpi_read, 0664,
		hpi_read_show, hpi_read_store);
#endif
#endif

#if IS_ENABLED(CONFIG_HDMI2_FREQ_SPREAD_SPECTRUM)
extern u32 freq_ss_amp;
static ssize_t freq_spread_spectrum_amp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t n = 0;

	n += sprintf(buf + n, "frequency spread spectrum amp:\n");
	n += sprintf(buf + n, "2/4/6/../30\n\n");

	n += sprintf(buf + n, "echo [amp] > /sys/clas/hdmi/hdmi/attr/ss_amp\n\n");

	n += sprintf(buf + n, "NOTE: if echo 0 > /sys/clas/hdmi/hdmi/attr/ss_amp,\n");
	n += sprintf(buf + n, "disable sprum spectrum\n\n");

	n += sprintf(buf + n, "The current amp is:%d\n", ss_amp);

	return n;
}

static ssize_t freq_spread_spectrum_amp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	char *end;
	struct disp_video_timings *video_info;

	freq_ss_amp = (u32)simple_strtoull(buf, &end, 0);

	_aw_disp_hdmi_get_video_timming_info(&video_info);

	_aw_freq_spread_sprectrum_enabled(video_info->pixel_clk);

	return count;
}

static DEVICE_ATTR(ss_amp, 0664,
		freq_spread_spectrum_amp_show, freq_spread_spectrum_amp_store);

extern bool freq_ss_old;
static ssize_t freq_spread_spectrum_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t n = 0;

	n += sprintf(buf + n, "Use the old frequency spread spectrum:\n");
	n += sprintf(buf + n, "echo old > /sys/clas/hdmi/hdmi/attr/ss_version\n\n");

	n += sprintf(buf + n, "Use the new frequency spread spectrum:\n");
	n += sprintf(buf + n, "echo new > /sys/clas/hdmi/hdmi/attr/ss_version\n\n");

	n += sprintf(buf + n, "The current version of frequency spread spectrum  is:%s\n",
		freq_ss_old ? "old" : "new");

	return n;
}

static ssize_t freq_spread_spectrum_version_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct disp_video_timings *video_info;

	if (strncmp(buf, "old", 1) == 0) {
		hdmi_inf("set old version freq_spread_spectrum\n");
		freq_ss_old = 1;
	} else if (strncmp(buf, "new", 1) == 0) {
		hdmi_inf("set new version freq_spread_spectrum\n");
		freq_ss_old = 0;
	} else {
		hdmi_err("%s: invalid input: %s\n", __func__, buf);
		return count;
	}

	_aw_disp_hdmi_get_video_timming_info(&video_info);

	_aw_freq_spread_sprectrum_enabled(video_info->pixel_clk);

	return count;
}

static DEVICE_ATTR(ss_version, 0664,
		freq_spread_spectrum_version_show, freq_spread_spectrum_version_store);
#endif

struct hdmi_debug_video_mode {
	int hdmi_mode;/* vic */
	char *name;
};

static struct hdmi_debug_video_mode debug_video_mode[] = {
	{HDMI_VIC_720x480I_4_3, "480I"},
	{HDMI_VIC_720x480I_16_9, "480I"},
	{HDMI_VIC_720x576I_4_3,	"576I"},
	{HDMI_VIC_720x576I_16_9, "576I"},
	{HDMI_VIC_720x480P60_4_3, "720x480P"},
	{HDMI_VIC_720x480P60_16_9, "720x480P"},
	{HDMI_VIC_640x480P60, "640x480P"},
	{HDMI_VIC_720x576P_4_3,	"576P"},
	{HDMI_VIC_720x576P_16_9, "576P"},
	{HDMI_VIC_1280x720P24, "720P24"},
	{HDMI_VIC_1280x720P25, "720P25"},
	{HDMI_VIC_1280x720P30, "720P30"},
	{HDMI_VIC_1280x720P50, "720P50"},
	{HDMI_VIC_1280x720P60, "720P60"},
	{HDMI_VIC_1920x1080I50,	"1080I50"},
	{HDMI_VIC_1920x1080I60,	"1080I60"},
	{HDMI_VIC_1920x1080P24,	"1080P24"},
	{HDMI_VIC_1920x1080P50,	"1080P50"},
	{HDMI_VIC_1920x1080P60,	"1080P60"},
	{HDMI_VIC_1920x1080P25,	"1080P25"},
	{HDMI_VIC_1920x1080P30,	"1080P30"},
	{HDMI_VIC_3840x2160P30,	"2160P30"},
	{HDMI_VIC_3840x2160P25,	"2160PP25"},
	{HDMI_VIC_3840x2160P24,	"2160P24"},
	{HDMI_VIC_4096x2160P24,	"4096x2160P24"},
	{HDMI_VIC_4096x2160P25,	"4096x2160P25"},
	{HDMI_VIC_4096x2160P30, "4096x2160P30"},
	{HDMI_VIC_3840x2160P50, "2160P50"},
	{HDMI_VIC_4096x2160P50, "4096x2160P50"},
	{HDMI_VIC_3840x2160P60, "2160P60"},
	{HDMI_VIC_4096x2160P60, "4096x2160P60"},
};

static char *hdmi_vic_name[] = {
	"2160P30",
	"2160P25",
	"2160P24",
	"4096x2160P24",
};

static char *hdmi_audio_code_name[] = {
	"LPCM",
	"AC-3",
	"MPEG1",
	"MP3",
	"MPEG2",
	"AAC",
	"DTS",
	"ATRAC",
	"OneBitAudio",
	"DolbyDigital+",
	"DTS-HD",
	"MAT",
	"DST",
	"WMAPro",
};
static char *debug_get_video_name(int hdmi_mode)
{
	int i = 0;
	int size = sizeof(debug_video_mode)/sizeof(struct hdmi_debug_video_mode);

	for (i = 0; i < size; i++) {
		if (debug_video_mode[i].hdmi_mode == hdmi_mode)
			return debug_video_mode[i].name;
	}

	return NULL;
}
static ssize_t hdmi_sink_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t n = 0;
	int i = 0;
	sink_edid_t *sink_cap =  g_hdmi_drv->hdmi_core->mode.sink_cap;

	if (!sink_cap) {
		n += sprintf(buf+n, "%s\n", "Do not read edid from sink");
		return n;
	}

	/* Video Data Block */
	n += sprintf(buf+n, "\n\n%s", "Video Mode:");
	for (i = 0; i < sink_cap->edid_mSvdIndex; i++) {
		if (sink_cap->edid_mSvd[i].mLimitedToYcc420
		|| sink_cap->edid_mSvd[i].mYcc420)
			continue;

		n += sprintf(buf+n, "  %s",
			debug_get_video_name(
			(int)sink_cap->edid_mSvd[i].mCode));


		if (sink_cap->edid_mSvd[i].mNative)
			n += sprintf(buf+n, "%s", "(native)");
	}


	for (i = 0; i < sink_cap->edid_mHdmivsdb.mHdmiVicCount; i++) {
		if (sink_cap->edid_mHdmivsdb.mHdmiVic[i] <= 0x4) {
			n += sprintf(buf+n, "%s",
			hdmi_vic_name[
			sink_cap->edid_mHdmivsdb.mHdmiVic[i]-1]);
		}
	}


	/* YCC420 VDB */
	n += sprintf(buf+n, "\n\n%s", "Only Support YUV420:");
	for (i = 0; i < sink_cap->edid_mSvdIndex; i++) {
		if (sink_cap->edid_mSvd[i].mLimitedToYcc420) {
			n += sprintf(buf+n, "  %s",
			debug_get_video_name(
			(int)sink_cap->edid_mSvd[i].mCode));
		}
	}

	/* YCC420 CMDB */
	n += sprintf(buf+n, "\n\n%s", "Also Support YUV420:");
	for (i = 0; i < sink_cap->edid_mSvdIndex; i++) {
		if (sink_cap->edid_mSvd[i].mYcc420) {
			n += sprintf(buf+n, "  %s",
			debug_get_video_name(
			(int)sink_cap->edid_mSvd[i].mCode));
		}
	}


	/* video pixel format */
	n += sprintf(buf+n, "\n\n%s", "Pixel Format: RGB");
	if (sink_cap->edid_mYcc444Support)
		n += sprintf(buf+n, "  %s", "YUV444");
	if (sink_cap->edid_mYcc422Support)
		n += sprintf(buf+n, "  %s", "YUV422");
	/* deepcolor */
	n += sprintf(buf+n, "\n\n%s", "Deep Color:");
	if (sink_cap->edid_mHdmivsdb.mDeepColor30) {
		n += sprintf(buf+n, "  %s", "RGB444_30bit");
		if (sink_cap->edid_mHdmivsdb.mDeepColorY444)
			n += sprintf(buf+n, "  %s", "YUV444_30bit");
	}

	if (sink_cap->edid_mHdmivsdb.mDeepColor36) {
		n += sprintf(buf+n, "  %s", "RGB444_36bit");
		if (sink_cap->edid_mHdmivsdb.mDeepColorY444)
			n += sprintf(buf+n, "  %s", "YUV444_36bit");
	}
	if (sink_cap->edid_mHdmivsdb.mDeepColor48) {
		n += sprintf(buf+n, "  %s", "RGB444_48bit");
		if (sink_cap->edid_mHdmivsdb.mDeepColorY444)
			n += sprintf(buf+n, "  %s", "YUV444_48bit");
	}

	if (sink_cap->edid_mHdmiForumvsdb.mDC_30bit_420)
		n += sprintf(buf+n, "  %s", "YUV420_30bit");
	if (sink_cap->edid_mHdmiForumvsdb.mDC_36bit_420)
		n += sprintf(buf+n, "  %s", "YUV420_36bit");
	if (sink_cap->edid_mHdmiForumvsdb.mDC_48bit_420)
		n += sprintf(buf+n, "  %s", "YUV420_48bit");

	/* 3D format */
	if (sink_cap->edid_mHdmivsdb.m3dPresent) {
		n += sprintf(buf+n, "\n\n%s", "3D Mode:");
		for (i = 0; i < 16; i++) {
			if (sink_cap->edid_mHdmivsdb.mVideo3dStruct[i][0] == 1
			&& i < sink_cap->edid_mSvdIndex) {
				n += sprintf(buf+n, "  %s_FP",
				debug_get_video_name(
				(int)sink_cap->edid_mSvd[i].mCode));
			}
			if (sink_cap->edid_mHdmivsdb.mVideo3dStruct[i][6] == 1
			&& i < sink_cap->edid_mSvdIndex) {
				n += sprintf(buf+n, "  %s_SBS",
				debug_get_video_name(
				(int)sink_cap->edid_mSvd[i].mCode));
			}
			if (sink_cap->edid_mHdmivsdb.mVideo3dStruct[i][8] == 1
			&& i < sink_cap->edid_mSvdIndex) {
				n += sprintf(buf+n, "  %s_TAB",
				debug_get_video_name(
				(int)sink_cap->edid_mSvd[i].mCode));
			}
		}
	}

	/* TMDS clk rate */
	if (sink_cap->edid_mHdmiForumvsdb.mValid) {
		n += sprintf(buf+n, "\n\n%s", "MaxTmdsCharRate:");
		n += sprintf(buf+n, "  %d",
			sink_cap->edid_mHdmiForumvsdb.mMaxTmdsCharRate);
	}
	/* audio */
	n += sprintf(buf+n, "\n\n%s", "Basic Audio Support:");
	n += sprintf(buf+n, "  %s",
		sink_cap->edid_mBasicAudioSupport ? "YES" : "NO");
	if (sink_cap->edid_mBasicAudioSupport) {
		n += sprintf(buf+n, "\n\n%s", "Audio Code:");
		for (i = 0; i < sink_cap->edid_mSadIndex; i++) {
			n += sprintf(buf+n, "  %s",
			hdmi_audio_code_name[sink_cap->edid_mSad[i].mFormat-1]);
		}
	}

	/* hdcp */
	n += sprintf(buf+n, "\n\n%s", "HDCP Tpye:");

	n += sprintf(buf+n, "%c", '\n');

	return n;

}

static DEVICE_ATTR(hdmi_sink, 0664,
		hdmi_sink_show, NULL);

static char *pixel_format_name[] = {
	"RGB",
	"YUV422",
	"YUV444",
	"YUV420",
};

static char *colorimetry_name[] = {
	"NULL",
	"ITU601",
	"ITU709",
	"XV_YCC601",
	"XV_YCC709",
	"S_YCC601",
	"ADOBE_YCC601",
	"ADOBE_RGB",
	"BT2020_Yc_Cbc_Crc",
	"BT2020_Y_CB_CR",
};

static ssize_t hdmi_rxsense_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u32 rxsense = g_hdmi_drv->hdmi_core->phy_ops.phy_get_rxsense();
	return sprintf(buf, "%u\n", rxsense);
}

static DEVICE_ATTR(rxsense, 0664,
		hdmi_rxsense_show, NULL);

static ssize_t hdmi_source_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t n = 0;
	u32 temp = 0;;

	/* phy info */
	n += sprintf(buf + n, "[phy info]\n");

	n += sprintf(buf + n, " - hpd: %d\n",
		g_hdmi_drv->hdmi_core->phy_ops.get_hpd());

	n += sprintf(buf + n, " - rxsense: %d\n",
		g_hdmi_drv->hdmi_core->phy_ops.phy_get_rxsense());

	temp = g_hdmi_drv->hdmi_core->phy_ops.phy_get_pll_lock();
	n += sprintf(buf + n, " - state: %s\n", temp == 1 ? "lock" : "unlock");

	temp = g_hdmi_drv->hdmi_core->phy_ops.phy_get_power();
	n += sprintf(buf + n, " - power: %s\n", temp == 1 ? "on" : "off");

	/* video info */
	n += sprintf(buf + n, "\n[video info]\n");

	temp = g_hdmi_drv->hdmi_core->video_ops.get_vic_code();
	n += sprintf(buf + n, " - video format: %s\n", debug_get_video_name((int)temp));

	temp = g_hdmi_drv->hdmi_core->video_ops.get_color_space();
	n += sprintf(buf + n, " - color format: %s\n", pixel_format_name[temp]);

	n += sprintf(buf + n, " - color depth: %d\n",
		g_hdmi_drv->hdmi_core->video_ops.get_color_depth());

	temp = g_hdmi_drv->hdmi_core->video_ops.get_tmds_mode();
	n += sprintf(buf + n, " - tmds mode: %s\n", temp == 1 ? "hdmi" : "dvi");

#ifndef SUPPORT_ONLY_HDMI14
	temp = g_hdmi_drv->hdmi_core->video_ops.get_scramble();
	n += sprintf(buf + n, " - scramble: %s\n", temp == 1 ? "on": "off");
#endif

	temp = g_hdmi_drv->hdmi_core->video_ops.get_avmute();
	n += sprintf(buf + n, " - avmute: %s\n", temp == 1 ? "on" : "off");

	temp = g_hdmi_drv->hdmi_core->video_ops.get_pixel_repetion();
	n += sprintf(buf + n, " - pixel repetion: %s\n", temp == 1 ? "on" : "off");

	temp = g_hdmi_drv->hdmi_core->video_ops.get_color_metry();
	n += sprintf(buf + n, " - color metry: %s\n", colorimetry_name[temp]);

	/* auido info */
	n += sprintf(buf + n, "\n[audio info]\n");

	n += sprintf(buf + n, " - layout: %d\n",
		g_hdmi_drv->hdmi_core->audio_ops.get_layout());

	n += sprintf(buf + n, " - channel count: %d\n",
		g_hdmi_drv->hdmi_core->audio_ops.get_channel_count());

	n += sprintf(buf + n, " - sample freq: %d\n",
		g_hdmi_drv->hdmi_core->audio_ops.get_sample_freq());

	n += sprintf(buf + n, " - sample size: %d\n",
		g_hdmi_drv->hdmi_core->audio_ops.get_sample_size());

	n += sprintf(buf + n, " - acr n: %d\n",
		g_hdmi_drv->hdmi_core->audio_ops.get_acr_n());

	n += sprintf(buf + n, "\n");
	return n;
}

static DEVICE_ATTR(hdmi_source, 0664,
		hdmi_source_show, NULL);

static ssize_t hdmi_avmute_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n%s\n",
		"echo [value] > avmute",
		"-----value =1:avmute on; =0:avmute off");
}

static ssize_t hdmi_avmute_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (count < 1)
		return -EINVAL;

	if (strncmp(buf, "1", 1) == 0)
		g_hdmi_drv->hdmi_core->video_ops.set_avmute(1);
	else
		g_hdmi_drv->hdmi_core->video_ops.set_avmute(0);

	return count;
}

static DEVICE_ATTR(avmute, 0664,
		hdmi_avmute_show, hdmi_avmute_store);

static ssize_t phy_power_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n%s\n",
		 "echo [value] > phy_power",
		"-----value =1:phy power on; =0:phy power off");
}

static ssize_t phy_power_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (count < 1)
		return -EINVAL;

	if (strncmp(buf, "1", 1) == 0)
		g_hdmi_drv->hdmi_core->phy_ops.phy_set_power(1);
	else
		g_hdmi_drv->hdmi_core->phy_ops.phy_set_power(0);

	return count;
}

static DEVICE_ATTR(phy_power, 0664,
		phy_power_show, phy_power_store);

static ssize_t dvi_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t n = 0;

	n += sprintf(buf + n, "%s\n%s\n",
			 "echo [value] > dvi_mode",
			"-----value =0:HDMI mode; =1:DVI mode");
	return n;
}

static ssize_t dvi_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (count < 1)
		return -EINVAL;

	if (strncmp(buf, "1", 1) == 0)
		g_hdmi_drv->hdmi_core->video_ops.set_tmds_mode(1);
	else
		g_hdmi_drv->hdmi_core->video_ops.set_tmds_mode(0);

	return count;
}

static DEVICE_ATTR(dvi_mode, 0664,
		dvi_mode_show, dvi_mode_store);

#if IS_ENABLED(CONFIG_AW_HDMI2_LOG_BUFFER)
static ssize_t hdmi_log_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t n = 0;
	int i;
	char *addr = aw_hdmi_log_get_address();
	unsigned int start = aw_hdmi_log_get_start_index();
	unsigned int max_size = aw_hdmi_log_get_max_size();
	unsigned int used_size = aw_hdmi_log_get_used_size();

	printk("%s start.\n", __func__);
	if (used_size < max_size) {
		for (i = 0; i < used_size; i++)
			printk(KERN_CONT "%c", addr[i]);
	} else {
		for (i = start; i < max_size; i++)
			printk(KERN_CONT "%c", addr[i]);
		for (i = 0; i < start; i++)
			printk(KERN_CONT "%c", addr[i]);
	}
	printk("%s finish!\n", __func__);
	aw_hdmi_log_put_address();

	n += sprintf(buf + n, "hdmi_log enable: %d\n", aw_hdmi_log_get_enable());
	return n;
}

static ssize_t hdmi_log_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (count < 1)
		return -EINVAL;

	if (strncmp(buf, "1", 1) == 0)
		aw_hdmi_log_set_enable(true);
	else if (strncmp(buf, "0", 1) == 0)
		aw_hdmi_log_set_enable(false);
	else
		hdmi_err("%s: echo 0/1 > hdmi_log\n", __func__);

	return count;
}

static DEVICE_ATTR(hdmi_log, 0664,
		hdmi_log_show, hdmi_log_store);
#endif

static struct attribute *hdmi_attributes[] = {
	&dev_attr_reg_dump.attr,
	&dev_attr_read.attr,
	&dev_attr_write.attr,
	&dev_attr_phy_write.attr,
	&dev_attr_phy_read.attr,
	&dev_attr_set_ddc.attr,
#ifndef SUPPORT_ONLY_HDMI14
	&dev_attr_scdc_read.attr,
	&dev_attr_scdc_write.attr,
#endif
	&dev_attr_debug.attr,
	&dev_attr_hpd_mask.attr,
	&dev_attr_edid.attr,
	&dev_attr_edid_test.attr,

	&dev_attr_hdmi_sink.attr,
	&dev_attr_hdmi_source.attr,
	&dev_attr_rxsense.attr,
	&dev_attr_avmute.attr,
	&dev_attr_phy_power.attr,
	&dev_attr_dvi_mode.attr,

#if IS_ENABLED(CONFIG_AW_HDMI2_HDCP_SUNXI)
	&dev_attr_hdcp_status.attr,
	&dev_attr_hdcp_type.attr,
	&dev_attr_hdcp_enable.attr,
	&dev_attr_hdcp_dump.attr,
#if IS_ENABLED(CONFIG_AW_HDMI2_HDCP22_SUNXI)
	&dev_attr_esm_dump.attr,
	&dev_attr_hpi_read.attr,
	&dev_attr_hpi_write.attr,
#endif
#endif

#if IS_ENABLED(CONFIG_AW_HDMI2_CEC_SUNXI)
#if (!IS_ENABLED(CONFIG_AW_HDMI2_CEC_USER))
	&dev_attr_one_touch_play.attr,
	&dev_attr_cec_local_standby_mode.attr,
#endif
	&dev_attr_cec_dump.attr,
	&dev_attr_cec_enable.attr,
#endif

#if IS_ENABLED(CONFIG_HDMI2_FREQ_SPREAD_SPECTRUM)
	&dev_attr_ss_amp.attr,
	&dev_attr_ss_version.attr,
#endif

#if IS_ENABLED(CONFIG_AW_HDMI2_LOG_BUFFER)
	&dev_attr_hdmi_log.attr,
#endif
	NULL
};

static struct attribute_group hdmi_attribute_group = {
	.name = "attr",
	.attrs = hdmi_attributes
};

#if IS_ENABLED(CONFIG_AW_HDMI2_CEC_USER)

static int _aw_cec_dev_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &cec_priv;
	return 0;
}

static int _aw_cec_dev_release(struct inode *inode, struct file *filp)
{
	return 0;
}

unsigned int _aw_cec_dev_poll(struct file *filp, struct poll_table_struct *wait)
{
	unsigned int mask = 0;

	mutex_lock(&file_ops_lock);
	poll_wait(filp, &cec_poll_queue, wait);
	if (aw_cec_user_msg_poll(filp))
		mask |= POLLIN | POLLRDNORM;
	mutex_unlock(&file_ops_lock);

	return mask;
}

static long _aw_cec_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	unsigned long *p_arg = (unsigned long *)arg;
	struct cec_msg msg;
	struct cec_msg_tx *tx_msg = NULL;
	unsigned int phy_addr;
	unsigned char log_addr;
	int ret = 0;

	mutex_lock(&file_ops_lock);
	switch (cmd) {
	case AW_IOCTL_CEC_S_PHYS_ADDR:
		hdmi_wrn("AW_IOCTL_CEC_S_PHYS_ADDR not supported yet!!!\n");
		break;

	case AW_IOCTL_CEC_G_PHYS_ADDR:
		phy_addr = (unsigned int)g_hdmi_drv->hdmi_core->cec_ops.cec_get_pa();
		if (phy_addr <= 0) {
			hdmi_err("cec ioctl get physical address failed!!!\n");
			mutex_unlock(&file_ops_lock);
			return (long)phy_addr;
		}

		if (copy_to_user((void __user *)p_arg[0], &phy_addr,
								sizeof(unsigned int))) {
			hdmi_err("%s: copy to user fail when get physical address!!!\n", __func__);
			goto err_exit;
		}
		mutex_unlock(&file_ops_lock);
		return (long)phy_addr;

	case AW_IOCTL_CEC_S_LOG_ADDR:
		if (copy_from_user((void *)&log_addr, (void __user *)p_arg[0],
						sizeof(unsigned char))) {
			hdmi_err("%s: copy from user fail when set logical address!!!\n", __func__);
			goto err_exit;
		}

		if (log_addr < 0 || (log_addr > 0x0f)) {
			hdmi_err("%s: invalid cec logcal address:0x%x\n", __func__, log_addr);
			goto err_exit;
		}
		aw_cec_set_tx_la(log_addr);
		ret = g_hdmi_drv->hdmi_core->cec_ops.cec_set_la(log_addr);
		if (ret != 0) {
			hdmi_err("%s: ioctl set logical address:0x%x failed!!!\n", __func__, log_addr);
			goto err_exit;
		}
		break;

	case AW_IOCTL_CEC_G_LOG_ADDR:
		log_addr = g_hdmi_drv->hdmi_core->cec_ops.cec_get_la();
		if (copy_to_user((void __user *)p_arg[0], &log_addr,
							sizeof(unsigned char))) {
			hdmi_err("%s: copy to user fail when get logical address!!!\n", __func__);
			goto err_exit;
		}
		mutex_unlock(&file_ops_lock);
		return (long)log_addr;

	case AW_IOCTL_CEC_TRANSMIT_MSG:
		tx_msg = vmalloc(sizeof(struct cec_msg_tx));
		if (!tx_msg) {
			hdmi_err("%s: vmalloc tx_msg failed!!!\n", __func__);
			goto err_exit;
		}

		ret = copy_from_user((void *)&tx_msg->msg, (void __user *)p_arg[0],
					sizeof(struct cec_msg));
		if (ret) {
			hdmi_err("%s: copy from user fail when transmit message!!!\n", __func__);
			goto err_exit;
		}

		if (filp->f_flags & O_NONBLOCK)
			ret = aw_cec_user_ioctl_send(tx_msg, AW_CEC_SEND_NON_BLOCK);
		else
			ret = aw_cec_user_ioctl_send(tx_msg, AW_CEC_SEND_BLOCK);
		if (ret == -1) {
			hdmi_err("%s: aw cec ioctl send msg failed!!!\n", __func__);
			vfree(tx_msg);
			goto err_exit;
		}

		ret = copy_to_user((void __user *)p_arg[0], &tx_msg->msg,
				sizeof(struct cec_msg));
		if (ret) {
			hdmi_err("%s: copy to user fail when cec transmit!!!\n",
					__func__);
			aw_cec_user_tx_list_delete(tx_msg);
			vfree(tx_msg);
			goto err_exit;
		}

		vfree(tx_msg);
		break;

	case AW_IOCTL_CEC_RECEIVE_MSG:
		ret = aw_cec_user_ioctl_receive(&msg);
		if (ret < 0) {
			hdmi_wrn("current not received cec message!\n");
			goto err_exit;
		}

		if (copy_to_user((void __user *)p_arg[0], &msg,
					sizeof(struct cec_msg))) {
			hdmi_err("%s: copy to user fail when cec receive!!!\n",
					__func__);
			goto err_exit;
		}

		break;

	default:
		hdmi_err("%s: cmd %d invalid\n", __func__, cmd);
		break;
	}

	mutex_unlock(&file_ops_lock);
	return 0;

err_exit:
	mutex_unlock(&file_ops_lock);
	return -1;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long _aw_cec_dev_compat_ioctl(struct file *file, unsigned int cmd,
					unsigned long arg)
{
	compat_uptr_t arg32[3] = {0};
	unsigned long arg64[3] = {0};

	if (copy_from_user((void *)arg32, (void __user *)arg,
				3 * sizeof(compat_uptr_t))) {
		hdmi_err("%s: copy from user fail when cec compat ioctl!!!\n", __func__);
		return -EFAULT;
	}
	arg64[0] = (unsigned long)arg32[0];
	arg64[1] = (unsigned long)arg32[1];
	arg64[2] = (unsigned long)arg32[2];

	return _aw_cec_dev_ioctl(file, cmd, (unsigned long)arg64);
}
#endif

static long _aw_cec_dev_unlock_ioctl(struct file *file, unsigned int cmd,
			      unsigned long arg)
{
	unsigned long arg64[3] = {0};

	if (copy_from_user((void *)arg64, (void __user *)arg,
				3 * sizeof(unsigned long))) {
		hdmi_err("%s: copy from user fail when cec unlock ioctl!!!\n", __func__);
		return -EFAULT;
	}

	return _aw_cec_dev_ioctl(file, cmd, (unsigned long)arg64);
}

static const struct file_operations aw_cec_fops = {
	.owner		= THIS_MODULE,
	.open		= _aw_cec_dev_open,
	.release	= _aw_cec_dev_release,
	.poll		= _aw_cec_dev_poll,
	.unlocked_ioctl	= _aw_cec_dev_unlock_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl	= _aw_cec_dev_compat_ioctl,
#endif
};

#endif

static int _aw_hdmi_dev_open(struct inode *inode, struct file *filp)
{
	struct file_ops *fops;

	fops = kmalloc(sizeof(struct file_ops), GFP_KERNEL | __GFP_ZERO);
	if (!fops) {
		hdmi_err("%s: memory allocated for hdmi fops failed!\n", __func__);
		return -EINVAL;
	}

	filp->private_data = fops;

	return 0;
}

static int _aw_hdmi_dev_release(struct inode *inode, struct file *filp)
{
	struct file_ops *fops = (struct file_ops *)filp->private_data;

	kfree(fops);
	return 0;
}

static ssize_t _aw_hdmi_dev_read(struct file *file, char __user *buf,
						size_t count,
						loff_t *ppos)
{
	return -EINVAL;
}

static ssize_t _aw_hdmi_dev_write(struct file *file, const char __user *buf,
						size_t count,
						loff_t *ppos)
{
	return -EINVAL;
}

static int _aw_hdmi_dev_mmap(struct file *filp, struct vm_area_struct *vma)
{
	return 0;
}

static long _aw_hdmi_dev_ioctl_inner(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct file_ops *fops = (struct file_ops *)filp->private_data;
#if IS_ENABLED(CONFIG_AW_HDMI2_HDCP_SUNXI) || IS_ENABLED(CONFIG_AW_HDMI2_LOG_BUFFER)
	unsigned long *p_arg = (unsigned long *)arg;
#endif
#if IS_ENABLED(CONFIG_AW_HDMI2_HDCP_SUNXI)
	struct hdmi_hdcp_info hdcp_info;
#if IS_ENABLED(CONFIG_AW_HDMI2_HDCP22_SUNXI)
	/* for hdcp keys */
	unsigned int key_size;
#endif
#endif

	if (!fops) {
		hdmi_err("%s: param fops is null!!!\n", __func__);
		return -EINVAL;
	}

	switch (cmd) {
	case AW_IOCTL_HDMI_NULL:
		fops->ioctl_cmd = AW_IOCTL_HDMI_NULL;
		break;

#if IS_ENABLED(CONFIG_AW_HDMI2_HDCP_SUNXI)
#if IS_ENABLED(CONFIG_AW_HDMI2_HDCP22_SUNXI)
	case AW_IOCTL_HDMI_HDCP22_LOAD_FW:
		fops->ioctl_cmd = AW_IOCTL_HDMI_HDCP22_LOAD_FW;
		if (p_arg[1] > gHdcp_esm_fw_size) {
			hdmi_err("%s: hdcp22 firmware is too big! arg_size:%lu esm_size:%d\n",
				__func__, p_arg[1], gHdcp_esm_fw_size);
			return -EINVAL;
		}

		key_size = p_arg[1];
		memset(gHdcp_esm_fw_vir_addr, 0, gHdcp_esm_fw_size);
		if (copy_from_user((void *)gHdcp_esm_fw_vir_addr,
					(void __user *)p_arg[0], key_size)) {
			hdmi_err("%s: copy from user fail when hdcp load firmware!!!\n", __func__);
			return -EINVAL;
		}
		hdmi_inf("ioctl hdcp22 load firmware has commpleted!\n");
		break;
#endif

	case AW_IOCTL_HDMI_HDCP_ENABLE:
		aw_hdmi_drv_set_hdcp_state(AW_HDCP_ING);
		aw_hdmi_drv_hdcp_enable(g_hdmi_drv->hdmi_core, 1);
		break;

	case AW_IOCTL_HDMI_HDCP_DISABLE:
		aw_hdmi_drv_hdcp_enable(g_hdmi_drv->hdmi_core, 0);
		aw_hdmi_drv_set_hdcp_state(AW_HDCP_DISABLE);
		break;

	case AW_IOCTL_HDMI_HDCP_INFO:
		hdcp_info.hdcp_type = aw_hdmi_drv_get_hdcp_type(g_hdmi_drv->hdmi_core);
		hdcp_info.hdcp_status = (unsigned int)aw_hdmi_drv_get_hdcp_state();

		if (copy_to_user((void __user *)p_arg[0], (void *)&hdcp_info,
								sizeof(struct hdmi_hdcp_info))) {
			hdmi_err("%s: copy to user fail when get hdcp info!!!\n", __func__);
			return -EINVAL;
		}

		break;
#endif

#if IS_ENABLED(CONFIG_AW_HDMI2_LOG_BUFFER)
	case AW_IOCTL_HDMI_GET_LOG_SIZE:
		{
			unsigned int size = aw_hdmi_log_get_max_size();

			if (copy_to_user((void __user *)p_arg[0], (void *)&size, sizeof(unsigned int))) {
				hdmi_err("%s: copy to user fail when get hdmi log size!\n", __func__);
				return -EINVAL;
			}
			break;
		}

	case AW_IOCTL_HDMI_GET_LOG:
		{
			char *addr = aw_hdmi_log_get_address();
			unsigned int start = aw_hdmi_log_get_start_index();
			unsigned int max_size = aw_hdmi_log_get_max_size();
			unsigned int used_size = aw_hdmi_log_get_used_size();

			if (used_size < max_size) {
				if (copy_to_user((void __user *)p_arg[0], (void *)addr, used_size)) {
					aw_hdmi_log_put_address();
					hdmi_err("%s: copy to user fail when get hdmi log!\n", __func__);
					return -EINVAL;
				}
			} else {
				if (copy_to_user((void __user *)p_arg[0], (void *)(addr + start),
							max_size - start)) {
					aw_hdmi_log_put_address();
					hdmi_err("%s: copy to user fail when get hdmi log!\n", __func__);
					return -EINVAL;
				}
				if (copy_to_user((void __user *)(p_arg[0] + (max_size - start)),
							(void *)addr, start)) {
					aw_hdmi_log_put_address();
					hdmi_err("%s: copy to user fail when get hdmi log!\n", __func__);
					return -EINVAL;
				}
			}
			aw_hdmi_log_put_address();
			break;
		}
#endif

	default:
		hdmi_err("%s: cmd %d invalid\n", __func__, cmd);
		return -EINVAL;
	}

	return 0;
}

static long _aw_hdmi_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	unsigned long arg64[3] = {0};

	if (copy_from_user((void *)arg64, (void __user *)arg,
						3 * sizeof(unsigned long))) {
		hdmi_err("%s: copy from user fail when hdmi ioctl!!!\n", __func__);
		return -EFAULT;
	}

	return _aw_hdmi_dev_ioctl_inner(filp, cmd, (unsigned long)arg64);
}

#if IS_ENABLED(CONFIG_COMPAT)
static long _aw_hdmi_dev_compat_ioctl(struct file *filp, unsigned int cmd,
						unsigned long arg)
{
	compat_uptr_t arg32[3] = {0};
	unsigned long arg64[3] = {0};

	if (!arg)
		return _aw_hdmi_dev_ioctl_inner(filp, cmd, 0);

	if (copy_from_user((void *)arg32, (void __user *)arg,
						3 * sizeof(compat_uptr_t))) {
		hdmi_err("%s: copy from user fail when hdmi compat ioctl!!!\n", __func__);
		return -EFAULT;
	}

	arg64[0] = (unsigned long)arg32[0];
	arg64[1] = (unsigned long)arg32[1];
	arg64[2] = (unsigned long)arg32[2];
	return _aw_hdmi_dev_ioctl_inner(filp, cmd, (unsigned long)arg64);
}
#endif

static const struct file_operations aw_hdmi_fops = {
	.owner		    = THIS_MODULE,
	.open		    = _aw_hdmi_dev_open,
	.release	    = _aw_hdmi_dev_release,
	.write		    = _aw_hdmi_dev_write,
	.read		    = _aw_hdmi_dev_read,
	.mmap		    = _aw_hdmi_dev_mmap,
	.unlocked_ioctl	= _aw_hdmi_dev_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl	= _aw_hdmi_dev_compat_ioctl,
#endif
};

#if IS_ENABLED(CONFIG_AW_HDMI2_CEC_USER)
static int aw_cec_module_init(void)
{
	/* Create and add a character device */
	/* corely for device number */
	alloc_chrdev_region(&aw_dev_hdmi.cec_devid, 0, 1, "cec");

	aw_dev_hdmi.cec_cdev = cdev_alloc();
	if (!aw_dev_hdmi.cec_cdev) {
		hdmi_err("%s: device register cdev_alloc failed!!!\n", __func__);
		return -1;
	}

	cdev_init(aw_dev_hdmi.cec_cdev, &aw_cec_fops);
	aw_dev_hdmi.cec_cdev->owner = THIS_MODULE;

	if (cdev_add(aw_dev_hdmi.cec_cdev, aw_dev_hdmi.cec_devid, 1)) {
		hdmi_err("%s: device register cdev_add failed!!!\n", __func__);
		return -1;
	}

	/* Create a path: sys/class/hdmi */
	aw_dev_hdmi.cec_class = class_create(THIS_MODULE, "cec");
	if (IS_ERR(aw_dev_hdmi.cec_class)) {
		hdmi_err("%s: cec class_create failed\n", __func__);
		return -1;
	}

	aw_dev_hdmi.cec_device = device_create(aw_dev_hdmi.cec_class, NULL, aw_dev_hdmi.cec_devid, NULL, "cec");
	if (!aw_dev_hdmi.cec_device) {
		hdmi_err("%s: device register device_create failed!!!\n", __func__);
		return -1;
	}

	init_waitqueue_head(&cec_poll_queue);

	aw_cec_user_driver_init();

	return 0;
}

static void aw_cec_module_exit(void)
{
	class_destroy(aw_dev_hdmi.cec_class);

	cdev_del(aw_dev_hdmi.cec_cdev);
}
#endif

static int __init aw_hdmi_module_init(void)
{
	int ret = 0;

	/* Create and add a character device */
	alloc_chrdev_region(&aw_dev_hdmi.hdmi_devid, 0, 1, "hdmi");/* corely for device number */
	aw_dev_hdmi.hdmi_cdev = cdev_alloc();
	if (!aw_dev_hdmi.hdmi_cdev) {
		hdmi_err("%s: device register cdev_alloc failed!!!\n", __func__);
		return -1;
	}

	cdev_init(aw_dev_hdmi.hdmi_cdev, &aw_hdmi_fops);
	aw_dev_hdmi.hdmi_cdev->owner = THIS_MODULE;

	/* Create a path: /proc/device/hdmi */
	if (cdev_add(aw_dev_hdmi.hdmi_cdev, aw_dev_hdmi.hdmi_devid, 1)) {
		hdmi_err("%s: device register cdev_add failed!!!\n", __func__);
		return -1;
	}

	/* Create a path: sys/class/hdmi */
	aw_dev_hdmi.hdmi_class = class_create(THIS_MODULE, "hdmi");
	if (IS_ERR(aw_dev_hdmi.hdmi_class)) {
		hdmi_err("%s: device register class_create failed!!!\n", __func__);
		return -1;
	}

	/* Create a path "sys/class/hdmi/hdmi" */
	aw_dev_hdmi.hdmi_device = device_create(aw_dev_hdmi.hdmi_class, NULL, aw_dev_hdmi.hdmi_devid, NULL, "hdmi");
	if (!aw_dev_hdmi.hdmi_device) {
		hdmi_err("%s: device register device_create failed!!!\n", __func__);
		return -1;
	}

	/* Create a path: sys/class/hdmi/hdmi/attr */
	ret = sysfs_create_group(&aw_dev_hdmi.hdmi_device->kobj, &hdmi_attribute_group);
	if (ret) {
		hdmi_err("%s: device register sysfs_create_group failed!!!\n", __func__);
		return -1;
	}

	ret = platform_driver_register(&dw_hdmi_pdrv);
	if (ret != 0) {
		hdmi_err("%s: device register platform_driver_register failed!!!\n", __func__);
		return -1;
	}

	hdmi_inf("hdmi module init end.\n");

#if IS_ENABLED(CONFIG_AW_HDMI2_CEC_USER)
	if (aw_cec_module_init() != 0) {
		hdmi_err("%s: hdmi cec module init failed!!!\n", __func__);
		return -1;
	}
	hdmi_inf("hdmi cec module init end.\n");
#endif

	return ret;
}

static void __exit aw_hdmi_module_exit(void)
{
#if IS_ENABLED(CONFIG_AW_HDMI2_CEC_SUNXI)
#if IS_ENABLED(CONFIG_AW_HDMI2_CEC_USER)
	aw_cec_module_exit();
#endif
#endif

	aw_hdmi_drv_remove(g_hdmi_drv->pdev);

#if IS_ENABLED(CONFIG_AW_HDMI2_HDCP22_SUNXI)
	dma_free_coherent(g_hdmi_drv->parent_dev,
				HDCP22_DATA_SIZE,
				&g_hdmi_drv->hdmi_core->mode.pHdcp.esm_data_phy_addr,
				GFP_KERNEL | __GFP_ZERO);
	dma_free_coherent(g_hdmi_drv->parent_dev,
					HDCP22_FIRMWARE_SIZE,
					&g_hdmi_drv->hdmi_core->mode.pHdcp.esm_firm_phy_addr,
					GFP_KERNEL | __GFP_ZERO);
#endif
	aw_hdmi_core_exit(g_hdmi_drv->hdmi_core);

	kfree(g_hdmi_drv);

	platform_driver_unregister(&dw_hdmi_pdrv);

	sysfs_remove_group(&aw_dev_hdmi.hdmi_device->kobj, &hdmi_attribute_group);

	device_destroy(aw_dev_hdmi.hdmi_class, aw_dev_hdmi.hdmi_devid);

	class_destroy(aw_dev_hdmi.hdmi_class);

	cdev_del(aw_dev_hdmi.hdmi_cdev);

#if IS_ENABLED(CONFIG_AW_HDMI2_LOG_BUFFER)
	aw_hdmi_log_exit();
#endif
}

late_initcall(aw_hdmi_module_init);
module_exit(aw_hdmi_module_exit);

MODULE_LICENSE("GPL v2");
MODULE_IMPORT_NS(DMA_BUF);
MODULE_AUTHOR("liangxianhui");
MODULE_DESCRIPTION("aw hdmi tx module driver");
MODULE_VERSION("2.0");
