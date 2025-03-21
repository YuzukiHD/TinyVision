/*
 * linux/drivers/net/ethernet/allwinner/sunxi_gmac.c
 *
 * Copyright © 2016-2018, fuzhaoke
 *		Author: fuzhaoke <fuzhaoke@allwinnertech.com>
 *
 * This file is provided under a dual BSD/GPL license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 */
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/mii.h>
#include <linux/gpio.h>
#include <linux/crc32.h>
#include <linux/skbuff.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/crypto.h>
#include <crypto/algapi.h>
#include <crypto/hash.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include <linux/regulator/consumer.h>
#include <linux/of_net.h>
#include <linux/of_gpio.h>
#include <linux/io.h>
#include <linux/sunxi-sid.h>
#include <linux/sunxi-gpio.h>
#include "sunxi-gmac.h"
#ifdef CONFIG_RTL8363NB_VB
#include <smi.h>
#include <rtk8363.h>
#include <rtk_types.h>
#include <port.h>
#include <rtk_switch.h>
#include <rtk_error.h>
#include <rtl8367c_asicdrv_port.h>
#endif

#define SUNXI_GMAC_VERSION "1.0.0"

#define DMA_DESC_RX	256
#define DMA_DESC_TX	256
#define BUDGET		(dma_desc_rx / 4)
#define TX_THRESH	(dma_desc_tx / 4)

#define HASH_TABLE_SIZE	64
#define MAX_BUF_SZ	(SZ_2K - 1)

#define POWER_CHAN_NUM	3

#undef PKT_DEBUG
#undef DESC_PRINT

/*PHY ID REG*/
#define PHYID1_REG        2
#define PHYID2_REG        3
/* IP101 */
#define PHYID1_IP101      0x0243
#define PHYID2_IP101      0x0C54
#define IP101GR_PAGE_CTRL_REG		20

#define circ_cnt(head, tail, size) (((head) > (tail)) ? \
					((head) - (tail)) : \
					((head) - (tail)) & ((size) - 1))

#define circ_space(head, tail, size) circ_cnt((tail), ((head) + 1), (size))

#define circ_inc(n, s) (((n) + 1) % (s))

#define GETH_MAC_ADDRESS "00:00:00:00:00:00"
static char *mac_str = GETH_MAC_ADDRESS;
module_param(mac_str, charp, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mac_str, "MAC Address String.(xx:xx:xx:xx:xx:xx)");

static int rxmode = 1;
module_param(rxmode, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(rxmode, "DMA threshold control value");

static int txmode = 1;
module_param(txmode, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(txmode, "DMA threshold control value");

static int pause = 0x400;
module_param(pause, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(pause, "Flow Control Pause Time");

#define TX_TIMEO	5000
static int watchdog = TX_TIMEO;
module_param(watchdog, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(watchdog, "Transmit timeout in milliseconds");

static int dma_desc_rx = DMA_DESC_RX;
module_param(dma_desc_rx, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(watchdog, "The number of receive's descriptors");

static int dma_desc_tx = DMA_DESC_TX;
module_param(dma_desc_tx, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(watchdog, "The number of transmit's descriptors");

/* - 0: Flow Off
 * - 1: Rx Flow
 * - 2: Tx Flow
 * - 3: Rx & Tx Flow
 */
static int flow_ctrl;
module_param(flow_ctrl, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(flow_ctrl, "Flow control [0: off, 1: rx, 2: tx, 3: both]");

/*static unsigned long tx_delay;
module_param(tx_delay, ulong, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(tx_delay, "Adjust transmit clock delay, value: 0~7");

static unsigned long rx_delay;
module_param(rx_delay, ulong, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(rx_delay, "Adjust receive clock delay, value: 0~31");*/

/* whether using ephy_clk */
/* static int g_use_ephy_clk;
static int g_phy_addr; */

struct geth_priv {
	struct dma_desc *dma_tx;
	struct sk_buff **tx_sk;
	unsigned int tx_clean;
	unsigned int tx_dirty;
	dma_addr_t dma_tx_phy;

	unsigned long buf_sz;

	struct dma_desc *dma_rx;
	struct sk_buff **rx_sk;
	unsigned int rx_clean;
	unsigned int rx_dirty;
	dma_addr_t dma_rx_phy;

	struct net_device *ndev;
	struct device *dev;
	struct napi_struct napi;

	struct geth_extra_stats xstats;

	struct mii_bus *mii;
	int link;
	int speed;
	int duplex;
#define INT_PHY 0
#define EXT_PHY 1
	int phy_ext;
	int phy_interface;

	void __iomem *base;
	void __iomem *base_phy;
	struct clk *geth_clk;
	struct clk *ephy_clk;
	struct pinctrl *pinctrl;

	struct regulator *gmac_power[POWER_CHAN_NUM];
	bool is_suspend;
	int phyrst;
	u8  rst_active_low;
	/* definition spinlock */
	spinlock_t lock;
	spinlock_t tx_lock;

	/* whether using ephy_clk */
	int use_ephy_clk;
	int phy_addr;

	/* adjust transmit clock delay, value: 0~7 */
	/* adjust receive clock delay, value: 0~31 */
	unsigned int tx_delay;
	unsigned int rx_delay;

	/* resume work */
	struct work_struct eth_work;

	/* phy az mode */
	bool disable_az_mode;
};

#ifdef CONFIG_RTL8363NB_VB
rtk_port_mac_ability_t mac_cfg;
rtk_mode_ext_t mode;
struct net_device *ndev = NULL;
struct geth_priv *priv;
#endif
#define LOOPBACK_DEBUG
static u64 geth_dma_mask = DMA_BIT_MASK(32);
static struct mii_reg_dump mii_reg;

void sunxi_udelay(int n)
{
	udelay(n);
}

static int geth_stop(struct net_device *ndev);
static int geth_open(struct net_device *ndev);
static void geth_tx_complete(struct geth_priv *priv);
static void geth_rx_refill(struct net_device *ndev);

#ifdef CONFIG_GETH_ATTRS
static ssize_t adjust_bgs_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int value = 0;
	u32 efuse_value;
	struct net_device *ndev = to_net_dev(dev);
	struct geth_priv *priv = netdev_priv(ndev);

	if (priv->phy_ext == INT_PHY) {
		value = readl(priv->base_phy) >> 28;
		if (sunxi_efuse_read(EFUSE_OEM_NAME, &efuse_value) != 0)
			pr_err("get PHY efuse fail!\n");
		else
#if defined(CONFIG_ARCH_SUN50IW2)
			value = value - ((efuse_value >> 24) & 0x0F);
#else
			pr_warn("miss config come from efuse!\n");
#endif
	}

	return sprintf(buf, "bgs: %d\n", value);
}

static ssize_t adjust_bgs_write(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned int out = 0;
	struct net_device *ndev = to_net_dev(dev);
	struct geth_priv *priv = netdev_priv(ndev);
	u32 clk_value = readl(priv->base_phy);
	u32 efuse_value;

	out = simple_strtoul(buf, NULL, 10);

	if (priv->phy_ext == INT_PHY) {
		clk_value &= ~(0xF << 28);
		if (sunxi_efuse_read(EFUSE_OEM_NAME, &efuse_value) != 0)
			pr_err("get PHY efuse fail!\n");
		else
#if defined(CONFIG_ARCH_SUN50IW2)
			clk_value |= (((efuse_value >> 24) & 0x0F) + out) << 28;
#else
			pr_warn("miss config come from efuse!\n");
#endif
	}

	writel(clk_value, priv->base_phy);

	return count;
}

static struct device_attribute adjust_reg[] = {
	__ATTR(adjust_bgs, 0664, adjust_bgs_show, adjust_bgs_write),
};

static int geth_create_attrs(struct net_device *ndev)
{
	int j, ret;

	for (j = 0; j < ARRAY_SIZE(adjust_reg); j++) {
		ret = device_create_file(&ndev->dev, &adjust_reg[j]);
		if (ret)
			goto sysfs_failed;
	}
	goto succeed;

sysfs_failed:
	while (j--)
		device_remove_file(&ndev->dev, &adjust_reg[j]);
succeed:
	return ret;
}
#endif

#ifdef DEBUG
static void desc_print(struct dma_desc *desc, int size)
{
#ifdef DESC_PRINT
	int i;

	for (i = 0; i < size; i++) {
		u32 *x = (u32 *)(desc + i);

		pr_info("\t%d [0x%08lx]: %08x %08x %08x %08x\n",
			i, (unsigned long)(&desc[i]),
			x[0], x[1], x[2], x[3]);
	}
	pr_info("\n");
#endif
}
#endif

static ssize_t extra_tx_stats_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct geth_priv *priv = netdev_priv(ndev);

	if (!dev) {
		pr_err("Argment is invalid\n");
		return 0;
	}

	if (!ndev) {
		pr_err("Net device is null\n");
		return 0;
	}

	return sprintf(buf, "tx_underflow: %lu\ntx_carrier: %lu\n"
			"tx_losscarrier: %lu\nvlan_tag: %lu\n"
			"tx_deferred: %lu\ntx_vlan: %lu\n"
			"tx_jabber: %lu\ntx_frame_flushed: %lu\n"
			"tx_payload_error: %lu\ntx_ip_header_error: %lu\n\n",
			priv->xstats.tx_underflow, priv->xstats.tx_carrier,
			priv->xstats.tx_losscarrier, priv->xstats.vlan_tag,
			priv->xstats.tx_deferred, priv->xstats.tx_vlan,
			priv->xstats.tx_jabber, priv->xstats.tx_frame_flushed,
			priv->xstats.tx_payload_error, priv->xstats.tx_ip_header_error);
}
static DEVICE_ATTR(extra_tx_stats, 0444, extra_tx_stats_show, NULL);

static ssize_t extra_rx_stats_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct geth_priv *priv = netdev_priv(ndev);

	if (!dev) {
		pr_err("Argment is invalid\n");
		return 0;
	}

	if (!ndev) {
		pr_err("Net device is null\n");
		return 0;
	}

	return sprintf(buf, "rx_desc: %lu\nsa_filter_fail: %lu\n"
			"overflow_error: %lu\nipc_csum_error: %lu\n"
			"rx_collision: %lu\nrx_crc: %lu\n"
			"dribbling_bit: %lu\nrx_length: %lu\n"
			"rx_mii: %lu\nrx_multicast: %lu\n"
			"rx_gmac_overflow: %lu\nrx_watchdog: %lu\n"
			"da_rx_filter_fail: %lu\nsa_rx_filter_fail: %lu\n"
			"rx_missed_cntr: %lu\nrx_overflow_cntr: %lu\n"
			"rx_vlan: %lu\n\n",
			priv->xstats.rx_desc, priv->xstats.sa_filter_fail,
			priv->xstats.overflow_error, priv->xstats.ipc_csum_error,
			priv->xstats.rx_collision, priv->xstats.rx_crc,
			priv->xstats.dribbling_bit, priv->xstats.rx_length,
			priv->xstats.rx_mii, priv->xstats.rx_multicast,
			priv->xstats.rx_gmac_overflow, priv->xstats.rx_length,
			priv->xstats.da_rx_filter_fail, priv->xstats.sa_rx_filter_fail,
			priv->xstats.rx_missed_cntr, priv->xstats.rx_overflow_cntr,
			priv->xstats.rx_vlan);
}
static DEVICE_ATTR(extra_rx_stats, 0444, extra_rx_stats_show, NULL);

static ssize_t gphy_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = dev_get_drvdata(dev);

	if (!dev) {
		pr_err("Argment is invalid\n");
		return 0;
	}

	if (!ndev) {
		pr_err("Net device is null\n");
		return 0;
	}

	return sprintf(buf, "Usage:\necho [0/1/2/3/4] > gphy_test\n"
			"0 - Normal Mode\n"
			"1 - Transmit Jitter Test\n"
			"2 - Transmit Jitter Test(MASTER mode)\n"
			"3 - Transmit Jitter Test(SLAVE mode)\n"
			"4 - Transmit Distortion Test\n\n");
}

static ssize_t gphy_test_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct geth_priv *priv = netdev_priv(ndev);
	u16 value = 0;
	int ret = 0;
	u16 data = 0;

	if (!dev) {
		pr_err("Argument is invalid\n");
		return count;
	}

	if (!ndev) {
		pr_err("Net device is null\n");
		return count;
	}

	data = sunxi_mdio_read(priv->base, priv->phy_addr, MII_CTRL1000);

	ret = kstrtou16(buf, 0, &value);
	if (ret)
		return ret;

	if (value >= 0 && value <= 4) {
		data &= ~(0x7 << 13);
		data |= value << 13;
		sunxi_mdio_write(priv->base, priv->phy_addr, MII_CTRL1000, data);
		pr_info("Set MII_CTRL1000(0x09) Reg: 0x%x\n", data);
	} else {
		pr_info("unknown value (%d)\n", value);
	}

	return count;
}

static DEVICE_ATTR(gphy_test, 0664, gphy_test_show, gphy_test_store);

static ssize_t mii_read_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct net_device *ndev;
	struct geth_priv *priv;

	if (!dev) {
		pr_err("Argment is invalid\n");
		return 0;
	}

	ndev = dev_get_drvdata(dev);
	if (!ndev) {
		pr_err("Net device is null\n");
		return 0;
	}

	priv = netdev_priv(ndev);
	if (!priv) {
		pr_err("geth_priv is null\n");
		return 0;
	}

	if (!netif_running(ndev)) {
		pr_warn("eth is down!\n");
		return 0;
	}

	mii_reg.value = sunxi_mdio_read(priv->base, mii_reg.addr, mii_reg.reg);
	return sprintf(buf, "ADDR[0x%02x]:REG[0x%02x] = 0x%04x\n",
		       mii_reg.addr, mii_reg.reg, mii_reg.value);
}

static ssize_t mii_read_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct net_device *ndev;
	struct geth_priv *priv;
	int ret = 0;
	u16 reg, addr;
	char *ptr;

	ptr = (char *)buf;

	if (!dev) {
		pr_err("Argment is invalid\n");
		return count;
	}

	ndev = dev_get_drvdata(dev);
	if (!ndev) {
		pr_err("Net device is null\n");
		return count;
	}

	priv = netdev_priv(ndev);
	if (!priv) {
		pr_err("geth_priv is null\n");
		return count;
	}

	if (!netif_running(ndev)) {
		pr_warn("eth is down!\n");
		return count;
	}

	ret = sunxi_parse_read_str(ptr, &addr, &reg);
	if (ret)
		return ret;

	mii_reg.addr = addr;
	mii_reg.reg = reg;

	return count;
}

static DEVICE_ATTR(mii_read, 0664, mii_read_show, mii_read_store);

static ssize_t mii_write_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct net_device *ndev;
	struct geth_priv *priv;
	u16 bef_val, aft_val;

	if (!dev) {
		pr_err("Argment is invalid\n");
		return 0;
	}

	ndev = dev_get_drvdata(dev);
	if (!ndev) {
		pr_err("Net device is null\n");
		return 0;
	}

	priv = netdev_priv(ndev);
	if (!priv) {
		pr_err("geth_priv is null\n");
		return 0;
	}

	if (!netif_running(ndev)) {
		pr_warn("eth is down!\n");
		return 0;
	}

	bef_val = sunxi_mdio_read(priv->base, mii_reg.addr, mii_reg.reg);
	sunxi_mdio_write(priv->base, mii_reg.addr, mii_reg.reg, mii_reg.value);
	aft_val = sunxi_mdio_read(priv->base, mii_reg.addr, mii_reg.reg);
	return sprintf(buf, "before ADDR[0x%02x]:REG[0x%02x] = 0x%04x\n"
			    "after  ADDR[0x%02x]:REG[0x%02x] = 0x%04x\n",
			    mii_reg.addr, mii_reg.reg, bef_val,
			    mii_reg.addr, mii_reg.reg, aft_val);
}

static ssize_t mii_write_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct net_device *ndev;
	struct geth_priv *priv;
	int ret = 0;
	u16 reg, addr, val;
	char *ptr;

	ptr = (char *)buf;

	if (!dev) {
		pr_err("Argment is invalid\n");
		return count;
	}

	ndev = dev_get_drvdata(dev);
	if (!ndev) {
		pr_err("Net device is null\n");
		return count;
	}

	priv = netdev_priv(ndev);
	if (!priv) {
		pr_err("geth_priv is null\n");
		return count;
	}

	if (!netif_running(ndev)) {
		pr_warn("eth is down!\n");
		return count;
	}

	ret = sunxi_parse_write_str(ptr, &addr, &reg, &val);
	if (ret)
		return ret;

	mii_reg.reg = reg;
	mii_reg.addr = addr;
	mii_reg.value = val;

	return count;
}

static DEVICE_ATTR(mii_write, 0664, mii_write_show, mii_write_store);

static ssize_t mii_reg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = NULL;
	struct geth_priv *priv = NULL;

	if (dev == NULL) {
		pr_err("Argment is invalid\n");
		return 0;
	}

	ndev = dev_get_drvdata(dev);
	if (ndev == NULL) {
		pr_err("Net device is null\n");
		return 0;
	}

	priv = netdev_priv(ndev);
	if (priv == NULL) {
		pr_err("geth_priv is null\n");
		return 0;
	}

	if (!netif_running(ndev)) {
		pr_warn("eth is down!\n");
		return 0;
	}

	return sprintf(buf,
		"Current MII Registers:\n"
		"BMCR[0x%02x] = 0x%04x,\t\tBMSR[0x%02x] = 0x%04x,\t\tPHYSID1[0x%02x] = 0x%04x\n"
		"PHYSID2[0x%02x] = 0x%04x,\t\tADVERTISE[0x%02x] = 0x%04x,\tLPA[0x%02x] = 0x%04x\n"
		"EXPANSION[0x%02x] = 0x%04x,\tCTRL1000[0x%02x] = 0x%04x,\tSTAT1000[0x%02x] = 0x%04x\n",
		MII_BMCR, sunxi_mdio_read(priv->base, priv->phy_addr, MII_BMCR),
		MII_BMSR, sunxi_mdio_read(priv->base, priv->phy_addr, MII_BMSR),
		MII_PHYSID1, sunxi_mdio_read(priv->base, priv->phy_addr, MII_PHYSID1),
		MII_PHYSID2, sunxi_mdio_read(priv->base, priv->phy_addr, MII_PHYSID2),
		MII_ADVERTISE, sunxi_mdio_read(priv->base, priv->phy_addr, MII_ADVERTISE),
		MII_LPA, sunxi_mdio_read(priv->base, priv->phy_addr, MII_LPA),
		MII_EXPANSION, sunxi_mdio_read(priv->base, priv->phy_addr, MII_EXPANSION),
		MII_CTRL1000, sunxi_mdio_read(priv->base, priv->phy_addr, MII_CTRL1000),
		MII_STAT1000, sunxi_mdio_read(priv->base, priv->phy_addr, MII_STAT1000));
}
static DEVICE_ATTR(mii_reg, 0444, mii_reg_show, NULL);

static ssize_t loopback_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "Usage:\necho [0/1/2] > loopback_test\n"
			"0 - Normal Mode\n"
			"1 - Mac loopback test mode\n"
			"2 - Phy loopback test mode\n");
}

static ssize_t loopback_test_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct net_device *ndev = NULL;
	struct geth_priv *priv = NULL;
	u16 value = 0;
	int ret = 0;
	u16 data = 0;

	if (dev == NULL) {
		pr_err("Argment is invalid\n");
		return count;
	}

	ndev = dev_get_drvdata(dev);
	if (ndev == NULL) {
		pr_err("Net device is null\n");
		return count;
	}

	priv = netdev_priv(ndev);
	if (priv == NULL) {
		pr_err("geth_priv is null\n");
		return count;
	}

	if (!netif_running(ndev)) {
		pr_warn("eth is down!\n");
		return count;
	}

	ret = kstrtou16(buf, 0, &value);
	if (ret)
		return ret;

	if (value == 0) { /* normal mode */
		/* clear mac loopback */
		sunxi_mac_loopback(priv->base, 0);

		/* clear phy loopback */
		data = sunxi_mdio_read(priv->base, priv->phy_addr, MII_BMCR);
		sunxi_mdio_write(priv->base, priv->phy_addr, MII_BMCR, data & ~BMCR_LOOPBACK);
	} else if (value == 1) { /* mac loopback test mode */
		data = sunxi_mdio_read(priv->base, priv->phy_addr, MII_BMCR);
		sunxi_mdio_write(priv->base, priv->phy_addr, MII_BMCR, data & ~BMCR_LOOPBACK);

		sunxi_mac_loopback(priv->base, 1);
	} else if (value == 2) { /* phy loopback test mode */
		sunxi_mac_loopback(priv->base, 0);

		data = sunxi_mdio_read(priv->base, priv->phy_addr, MII_BMCR);
		sunxi_mdio_write(priv->base, priv->phy_addr, MII_BMCR, data | BMCR_LOOPBACK);
	} else {
		pr_err("Undefined value (%d)\n", value);
	}

	return count;
}
static DEVICE_ATTR(loopback_test, 0664, loopback_test_show, loopback_test_store);

static int geth_power_on(struct geth_priv *priv)
{
	int value;
	int i;

	value = readl(priv->base_phy);
	if (priv->phy_ext == INT_PHY) {
		value |= (1 << 15);
		value &= ~(1 << 16);
		value |= (3 << 17);
	} else {
		value &= ~(1 << 15);

		for (i = 0; i < POWER_CHAN_NUM; i++) {
			if (IS_ERR_OR_NULL(priv->gmac_power[i]))
				continue;
			if (regulator_enable(priv->gmac_power[i]) != 0) {
				pr_err("gmac-power%d enable error\n", i);
				return -EINVAL;
			}
		}
	}

	writel(value, priv->base_phy);

	return 0;
}

static void geth_power_off(struct geth_priv *priv)
{
	int value;
	int i;

	if (priv->phy_ext == INT_PHY) {
		value = readl(priv->base_phy);
		value |= (1 << 16);
		writel(value, priv->base_phy);
	} else {
		for (i = 0; i < POWER_CHAN_NUM; i++) {
			if (IS_ERR_OR_NULL(priv->gmac_power[i]))
				continue;
			regulator_disable(priv->gmac_power[i]);
		}
	}
}

#ifdef CONFIG_RTL8363NB_VB
/* rtl8363nb_vb switch init. */
static int rtl8363nb_vb_init(void)
{
	pr_info("%s->%d rtk8363nb_vb init=====\n", __func__, __LINE__);

	if (rtk_switch_init() != RT_ERR_OK) {
		pr_info("rtk switch init failed!\n");
		return -1;
	}
	mode = MODE_EXT_RGMII;
	mac_cfg.forcemode = MAC_FORCE;
	mac_cfg.speed = SPD_1000M;
	mac_cfg.duplex = FULL_DUPLEX;
	mac_cfg.link = PORT_LINKUP;
	mac_cfg.nway = DISABLED;
	mac_cfg.txpause = ENABLED;
	mac_cfg.rxpause = ENABLED;

	if (rtk_port_macForceLinkExt_set(EXT_PORT0, mode, &mac_cfg) != RT_ERR_OK) {
		pr_info("macForceLinkExt set failed!\n");
		return -1;
	}

	rtk_port_rgmiiDelayExt_set(EXT_PORT0, 1, 0);
	rtk_port_phyEnableAll_set(ENABLED);
	return 0;
}

/* rtl8363 switch mdc/mdio interface operations */
int rtk_mdio_read(u32 len, u8 phy_adr, u8 reg, u32 *value)
{

	struct geth_priv *priv = netdev_priv(ndev);

	*value = sunxi_mdio_read(priv->base, 0, reg);

	return 0;
}

int rtk_mdio_write(u32 len, u8 phy_adr, u8 reg, u32 data)
{

	struct geth_priv *priv = netdev_priv(ndev);
	sunxi_mdio_write(priv->base, 0, reg, data);

	return 0;
}
/* rtl8363 switch PHY interface operations */
static int rtk_phy_read(struct mii_bus *bus, int phyaddr, int phyreg)
{
	struct net_device *ndev = bus->priv;
	struct geth_priv *priv = netdev_priv(ndev);

	return (int)smi_read(phyreg, NULL);
}

static int rtk_phy_write(struct mii_bus *bus, int phyaddr,
			   int phyreg, u16 data)
{
	struct net_device *ndev = bus->priv;
	struct geth_priv *priv = netdev_priv(ndev);

	smi_write(phyreg, data);

	return 0;
}
#endif

/* PHY interface operations */
static int geth_mdio_read(struct mii_bus *bus, int phyaddr, int phyreg)
{
	struct net_device *ndev = bus->priv;
	struct geth_priv *priv = netdev_priv(ndev);

	return (int)sunxi_mdio_read(priv->base,  phyaddr, phyreg);
}

static int geth_mdio_write(struct mii_bus *bus, int phyaddr,
			   int phyreg, u16 data)
{
	struct net_device *ndev = bus->priv;
	struct geth_priv *priv = netdev_priv(ndev);

	sunxi_mdio_write(priv->base, phyaddr, phyreg, data);

	return 0;
}

static int geth_mdio_reset(struct mii_bus *bus)
{
	struct net_device *ndev = bus->priv;
	struct geth_priv *priv = netdev_priv(ndev);

	return sunxi_mdio_reset(priv->base);
}

static void geth_adjust_link(struct net_device *ndev)
{
	struct geth_priv *priv = netdev_priv(ndev);
	struct phy_device *phydev = ndev->phydev;
	unsigned long flags;
	int new_state = 0;

	if (!phydev)
		return;

	spin_lock_irqsave(&priv->lock, flags);
 #ifdef CONFIG_RTL8363NB_VB
         priv->speed = 1000;
         priv->duplex = 1;
         //pr_info("%s->%d ===> speed: %d duplex: %d!\n", __func__, __LINE__, priv->speed, priv->duplex);
         sunxi_set_link_mode(priv->base, 1, 1000);
         phy_print_status(phydev);
#else
	if (phydev->link) {
		/* Now we make sure that we can be in full duplex mode.
		 * If not, we operate in half-duplex mode.
		 */
		if (phydev->duplex != priv->duplex) {
			new_state = 1;
			priv->duplex = phydev->duplex;
		}
		/* Flow Control operation */
		if (phydev->pause)
			sunxi_flow_ctrl(priv->base, phydev->duplex,
					flow_ctrl, pause);

		if (phydev->speed != priv->speed) {
			new_state = 1;
			priv->speed = phydev->speed;
		}

		if (priv->link == 0) {
			new_state = 1;
			priv->link = phydev->link;
		}

		if (new_state)
			sunxi_set_link_mode(priv->base, priv->duplex, priv->speed);

#ifdef LOOPBACK_DEBUG
		phydev->state = PHY_FORCING;
#endif

	} else if (priv->link != phydev->link) {
		new_state = 1;
		priv->link = 0;
		priv->speed = 0;
		priv->duplex = -1;
	}

	if (new_state)
		phy_print_status(phydev);
#endif
	spin_unlock_irqrestore(&priv->lock, flags);
}

static __maybe_unused void ip101gr_write_reg(struct net_device *dev, u32 page, u32 reg, u16 val)
{
	struct phy_device *phydev = dev->phydev;

	phy_write(phydev, IP101GR_PAGE_CTRL_REG, page);  /* switch to the specified page */
	phy_write(phydev, reg, val);
	phy_write(phydev, IP101GR_PAGE_CTRL_REG, 0);  /* restore to page 0 */
}

static int geth_phy_init(struct net_device *ndev)
{
	int value;
	struct mii_bus *new_bus;
	struct geth_priv *priv = netdev_priv(ndev);
	struct phy_device *phydev = ndev->phydev;

	if (priv->is_suspend && phydev)
		goto resume;

	/* Fixup the phy interface type */
	if (priv->phy_ext == INT_PHY) {
		priv->phy_interface = PHY_INTERFACE_MODE_MII;
	} else {
		/* If config gpio to reset the phy device, we should reset it */
		if (gpio_is_valid(priv->phyrst)) {
			gpio_direction_output(priv->phyrst,
					priv->rst_active_low);
			msleep(50);
			gpio_direction_output(priv->phyrst,
					!priv->rst_active_low);
			msleep(50);
		}
	}

	new_bus = mdiobus_alloc();
	if (!new_bus) {
		netdev_err(ndev, "Failed to alloc new mdio bus\n");
		return -ENOMEM;
	}

	new_bus->name = dev_name(priv->dev);
#ifdef CONFIG_RTL8363NB_VB
	/*rtl8363 switch can not use kernel's phy interface, neeed to redefine hook function.*/
	new_bus->read = &rtk_phy_read;
	new_bus->write = &rtk_phy_write;
#if 0
	//read reg 0x1b00
	sunxi_mdio_write(priv->base, priv->phy_addr, 31, 0x000E);
	sunxi_mdio_write(priv->base, priv->phy_addr, 23, 0x1b00);
	sunxi_mdio_write(priv->base, priv->phy_addr, 21, 0x1);
	sunxi_mdio_read(priv->base, priv->phy_addr, 25);
	pr_info("%s->%d ===> reg 0x1b00 = %x!\n", __func__, __LINE__,\
			sunxi_mdio_read(priv->base, priv->phy_addr, 25));
#endif
#else
	new_bus->read = &geth_mdio_read;
	new_bus->write = &geth_mdio_write;
	new_bus->reset = &geth_mdio_reset;
#endif
	snprintf(new_bus->id, MII_BUS_ID_SIZE, "%s-%x", new_bus->name, 0);

	new_bus->parent = priv->dev;
	new_bus->priv = ndev;

	if (mdiobus_register(new_bus)) {
		pr_err("%s: Cannot register as MDIO bus\n", new_bus->name);
		goto reg_fail;
	}

	priv->mii = new_bus;

	{
		int addr;

		for (addr = 0; addr < PHY_MAX_ADDR; addr++) {
			struct phy_device *phydev_tmp = mdiobus_get_phy(new_bus, addr);

			if (phydev_tmp && (phydev_tmp->phy_id != 0x00)) {
				phydev = phydev_tmp;
				priv->phy_addr = addr;
				break;
			}
		}
	}


#ifdef CONFIG_RTL8363NB_VB
	phydev->autoneg = AUTONEG_DISABLE;
#endif

	if (!phydev) {
		netdev_err(ndev, "No PHY found!\n");
		goto err;
	}

	phy_write(phydev, MII_BMCR, BMCR_RESET);
	while (BMCR_RESET & phy_read(phydev, MII_BMCR))
		msleep(30);

	value = phy_read(phydev, MII_BMCR);
	phy_write(phydev, MII_BMCR, (value & ~BMCR_PDOWN));

	phydev->irq = PHY_POLL;

	value = phy_connect_direct(ndev, phydev, &geth_adjust_link, priv->phy_interface);
	if (value) {
		netdev_err(ndev, "Could not attach to PHY\n");
		goto err;
	} else {
		netdev_info(ndev, "%s: Type(%d) PHY ID %08x at %d IRQ %s (%s)\n",
			    ndev->name, phydev->interface, phydev->phy_id,
			    phydev->mdio.addr, "poll", dev_name(&phydev->mdio.dev));
	}

	phydev->supported &= PHY_GBIT_FEATURES;
	phydev->advertising = phydev->supported;

	/* Fix PHY eye diagram */
#define PHY_EXT_READ		0x1e
#define PHY_EXT_WRITE		0x1f
#define PHY_ADDR		0x00
#define PHY_EYE_DIAGRAM_REG	0x2057
#define PHY_EYE_DIAGRAM_VALUE	0x2b4f
	sunxi_mdio_write(priv->base, PHY_ADDR, PHY_EXT_READ, PHY_EYE_DIAGRAM_REG);
	sunxi_mdio_write(priv->base, PHY_ADDR, PHY_EXT_WRITE, PHY_EYE_DIAGRAM_VALUE);

resume:
	if (priv->phy_ext == INT_PHY) {
		/* EPHY Initial */
		phy_write(phydev, 0x1f, 0x0100); /* switch to page 1 */
		phy_write(phydev, 0x12, 0x4824); /* Disable APS */
		phy_write(phydev, 0x1f, 0x0200); /* switchto page 2 */
		phy_write(phydev, 0x18, 0x0000); /* PHYAFE TRX optimization */
		phy_write(phydev, 0x1f, 0x0600); /* switchto page 6 */
		phy_write(phydev, 0x14, 0x708F); /* PHYAFE TX optimization */
		phy_write(phydev, 0x19, 0x0000);
		phy_write(phydev, 0x13, 0xf000); /* PHYAFE RX optimization */
		phy_write(phydev, 0x15, 0x1530);
		phy_write(phydev, 0x1f, 0x0800); /* switch to page 8 */
		phy_write(phydev, 0x18, 0x00bc); /* PHYAFE TRX optimization */
		phy_write(phydev, 0x1f, 0x0100); /* switchto page 1 */
		/* reg 0x17 bit3,set 0 to disable iEEE */
		phy_write(phydev, 0x17, phy_read(phydev, 0x17) & (~(1<<3)));
		phy_write(phydev, 0x1f, 0x0000); /* switch to page 0 */

	} else {
		if (priv->disable_az_mode) {
			/* check phy id */
			if ((phy_read(phydev, PHYID1_REG) == PHYID1_IP101)
				&& (phy_read(phydev, PHYID2_REG) == PHYID2_IP101)) {

				/*disable az-mode for ip101 phy, the phy can speed up the linking process.*/
				ip101gr_write_reg(ndev, 0, 13, 0x0007);
				ip101gr_write_reg(ndev, 0, 14, 0x003c);
				ip101gr_write_reg(ndev, 0, 13, 0x4007);
				ip101gr_write_reg(ndev, 0, 14, 0x0000);
				ip101gr_write_reg(ndev, 0, 0,  0x3300);

			}

		}

	}

	if (priv->is_suspend)
		phy_init_hw(phydev);

	return 0;

err:
	mdiobus_unregister(new_bus);
reg_fail:
	mdiobus_free(new_bus);

	return -EINVAL;
}

static int geth_phy_release(struct net_device *ndev)
{
	struct geth_priv *priv = netdev_priv(ndev);
	struct phy_device *phydev = ndev->phydev;
	int value = 0;

	/* Stop and disconnect the PHY */
	if (phydev)
		phy_stop(phydev);

	priv->link = PHY_DOWN;
	priv->speed = 0;
	priv->duplex = -1;

	if (priv->is_suspend)
		return 0;

	if (phydev) {
		value = phy_read(phydev, MII_BMCR);
		phy_write(phydev, MII_BMCR, (value | BMCR_PDOWN));
		phy_disconnect(phydev);
		ndev->phydev = NULL;
	}

	if (priv->mii) {
		mdiobus_unregister(priv->mii);
		priv->mii->priv = NULL;
		mdiobus_free(priv->mii);
		priv->mii = NULL;
	}

	return 0;
}

static void geth_rx_refill(struct net_device *ndev)
{
	struct geth_priv *priv = netdev_priv(ndev);
	struct dma_desc *desc;
	struct sk_buff *sk = NULL;
	dma_addr_t paddr;

	while (circ_space(priv->rx_clean, priv->rx_dirty, dma_desc_rx) > 0) {
		int entry = priv->rx_clean;

		/* Find the dirty's desc and clean it */
		desc = priv->dma_rx + entry;

		if (priv->rx_sk[entry] == NULL) {
			sk = netdev_alloc_skb_ip_align(ndev, priv->buf_sz);

			if (unlikely(sk == NULL))
				break;

			priv->rx_sk[entry] = sk;
			paddr = dma_map_single(priv->dev, sk->data,
					       priv->buf_sz, DMA_FROM_DEVICE);
			desc_buf_set(desc, paddr, priv->buf_sz);
		}

		/* sync memery */
		wmb();
		desc_set_own(desc);
		priv->rx_clean = circ_inc(priv->rx_clean, dma_desc_rx);
	}
}

/* geth_dma_desc_init - initialize the RX/TX descriptor list
 * @ndev: net device structure
 * Description: initialize the list for dma.
 */
static int geth_dma_desc_init(struct net_device *ndev)
{
	struct geth_priv *priv = netdev_priv(ndev);
	unsigned int buf_sz;

	priv->rx_sk = kzalloc(sizeof(struct sk_buff *) * dma_desc_rx,
				GFP_KERNEL);
	if (!priv->rx_sk)
		return -ENOMEM;

	priv->tx_sk = kzalloc(sizeof(struct sk_buff *) * dma_desc_tx,
				GFP_KERNEL);
	if (!priv->tx_sk)
		goto tx_sk_err;

	/* Set the size of buffer depend on the MTU & max buf size */
	buf_sz = MAX_BUF_SZ;

	priv->dma_tx = dma_alloc_coherent(priv->dev,
					dma_desc_tx *
					sizeof(struct dma_desc),
					&priv->dma_tx_phy,
					GFP_KERNEL);
	if (!priv->dma_tx)
		goto dma_tx_err;

	priv->dma_rx = dma_alloc_coherent(priv->dev,
					dma_desc_rx *
					sizeof(struct dma_desc),
					&priv->dma_rx_phy,
					GFP_KERNEL);
	if (!priv->dma_rx)
		goto dma_rx_err;

	priv->buf_sz = buf_sz;

	return 0;

dma_rx_err:
	dma_free_coherent(priv->dev, dma_desc_rx * sizeof(struct dma_desc),
			  priv->dma_tx, priv->dma_tx_phy);
dma_tx_err:
	kfree(priv->tx_sk);
tx_sk_err:
	kfree(priv->rx_sk);

	return -ENOMEM;
}

static void geth_free_rx_sk(struct geth_priv *priv)
{
	int i;

	for (i = 0; i < dma_desc_rx; i++) {
		if (priv->rx_sk[i] != NULL) {
			struct dma_desc *desc = priv->dma_rx + i;

			dma_unmap_single(priv->dev, (u32)desc_buf_get_addr(desc),
					 desc_buf_get_len(desc),
					 DMA_FROM_DEVICE);
			dev_kfree_skb_any(priv->rx_sk[i]);
			priv->rx_sk[i] = NULL;
		}
	}
}

static void geth_free_tx_sk(struct geth_priv *priv)
{
	int i;

	for (i = 0; i < dma_desc_tx; i++) {
		if (priv->tx_sk[i] != NULL) {
			struct dma_desc *desc = priv->dma_tx + i;

			if (desc_buf_get_addr(desc))
				dma_unmap_single(priv->dev, (u32)desc_buf_get_addr(desc),
						 desc_buf_get_len(desc),
						 DMA_TO_DEVICE);
			dev_kfree_skb_any(priv->tx_sk[i]);
			priv->tx_sk[i] = NULL;
		}
	}
}

static void geth_free_dma_desc(struct geth_priv *priv)
{
	/* Free the region of consistent memory previously allocated for the DMA */
	dma_free_coherent(priv->dev, dma_desc_tx * sizeof(struct dma_desc),
			  priv->dma_tx, priv->dma_tx_phy);
	dma_free_coherent(priv->dev, dma_desc_rx * sizeof(struct dma_desc),
			  priv->dma_rx, priv->dma_rx_phy);

	kfree(priv->rx_sk);
	kfree(priv->tx_sk);
}

#ifdef CONFIG_PM
static void geth_select_rst_state(int gpio, unsigned int gpio_func)
{
	char pin_name[SUNXI_PIN_NAME_MAX_LEN];
	unsigned long config_set;

	if (!gpio_is_valid(gpio))
		return ;

	sunxi_gpio_to_name(gpio, pin_name);
	config_set = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_FUNC, gpio_func);
	if (gpio < SUNXI_PL_BASE)
		pin_config_set(SUNXI_PINCTRL, pin_name, config_set);
	else
		pin_config_set(SUNXI_R_PINCTRL, pin_name, config_set);
}

static int geth_select_gpio_state(struct pinctrl *pctrl, char *name)
{
	int ret = 0;
	struct pinctrl_state *pctrl_state = NULL;

	pctrl_state = pinctrl_lookup_state(pctrl, name);
	if (IS_ERR(pctrl_state)) {
		pr_err("gmac pinctrl_lookup_state(%s) failed! return %p\n",
						name, pctrl_state);
		return -EINVAL;
	}

	ret = pinctrl_select_state(pctrl, pctrl_state);
	if (ret < 0)
		pr_err("gmac pinctrl_select_state(%s) failed! return %d\n",
						name, ret);

	return ret;
}

static int geth_suspend(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct geth_priv *priv = netdev_priv(ndev);

	cancel_work_sync(&priv->eth_work);

	if (!ndev || !netif_running(ndev))
		return 0;

	priv->is_suspend = true;

	spin_lock(&priv->lock);
	netif_device_detach(ndev);
	spin_unlock(&priv->lock);

	geth_stop(ndev);

	if (priv->phy_ext == EXT_PHY)
		geth_select_gpio_state(priv->pinctrl, PINCTRL_STATE_SLEEP);

	if (gpio_is_valid(priv->phyrst)) {
		geth_select_rst_state(priv->phyrst, 7);
	}

	return 0;
}

static void geth_resume_work(struct work_struct *work)
{
	struct geth_priv *priv = container_of(work, struct geth_priv, eth_work);
	struct net_device *ndev = priv->ndev;
	int ret = 0;

	if (!netif_running(ndev))
		return;

	if (priv->phy_ext == EXT_PHY)
		geth_select_gpio_state(priv->pinctrl, PINCTRL_STATE_DEFAULT);

	spin_lock(&priv->lock);
	netif_device_attach(ndev);
	spin_unlock(&priv->lock);

#if defined(CONFIG_SUNXI_EPHY)
	if (!ephy_is_enable()) {
		pr_info("[geth_resume] ephy is not enable, waiting...\n");
		msleep(2000);
		if (!ephy_is_enable()) {
			netdev_err(ndev, "Wait for ephy resume timeout.\n");
			return;
		}
	}
#endif

	ret = geth_open(ndev);
	if (!ret)
		priv->is_suspend = false;
}

static void geth_resume(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct geth_priv *priv = netdev_priv(ndev);

	schedule_work(&priv->eth_work);
}

static int geth_freeze(struct device *dev)
{
	return 0;
}

static int geth_restore(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops geth_pm_ops = {
	.complete = geth_resume,
	.prepare = geth_suspend,
	.suspend = NULL,
	.resume = NULL,
	.freeze = geth_freeze,
	.restore = geth_restore,
};
#else
static const struct dev_pm_ops geth_pm_ops;
#endif /* CONFIG_PM */

#define sunxi_get_soc_chipid(x) {}
static void geth_chip_hwaddr(u8 *addr)
{
#define MD5_SIZE	16
#define CHIP_SIZE	16

	struct crypto_ahash *tfm;
	struct ahash_request *req;
	struct scatterlist sg;
	u8 result[MD5_SIZE];
	u8 chipid[CHIP_SIZE];
	int i = 0;
	int ret = -1;

	memset(chipid, 0, sizeof(chipid));
	memset(result, 0, sizeof(result));

	sunxi_get_soc_chipid((u8 *)chipid);

	tfm = crypto_alloc_ahash("md5", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm)) {
		pr_err("Failed to alloc md5\n");
		return;
	}

	req = ahash_request_alloc(tfm, GFP_KERNEL);
	if (!req)
		goto out;

	ahash_request_set_callback(req, 0, NULL, NULL);

	ret = crypto_ahash_init(req);
	if (ret) {
		pr_err("crypto_ahash_init() failed\n");
		goto out;
	}

	sg_init_one(&sg, chipid, sizeof(chipid));
	ahash_request_set_crypt(req, &sg, result, sizeof(chipid));
	ret = crypto_ahash_update(req);
	if (ret) {
		pr_err("crypto_ahash_update() failed for id\n");
		goto out;
	}

	ret = crypto_ahash_final(req);
	if (ret) {
		pr_err("crypto_ahash_final() failed for result\n");
		goto out;
	}

	ahash_request_free(req);

	/* Choose md5 result's [0][2][4][6][8][10] byte as mac address */
	for (i = 0; i < ETH_ALEN; i++)
		addr[i] = result[2 * i];
	addr[0] &= 0xfe; /* clear multicast bit */
	addr[0] |= 0x02; /* set local assignment bit (IEEE802) */

out:
	crypto_free_ahash(tfm);
}

static void geth_check_addr(struct net_device *ndev, unsigned char *mac)
{
	int i;
	char *p = mac;

	if (!is_valid_ether_addr(ndev->dev_addr)) {
		for (i = 0; i < ETH_ALEN; i++, p++)
			ndev->dev_addr[i] = simple_strtoul(p, &p, 16);

		if (!is_valid_ether_addr(ndev->dev_addr))
			geth_chip_hwaddr(ndev->dev_addr);

		if (!is_valid_ether_addr(ndev->dev_addr)) {
			random_ether_addr(ndev->dev_addr);
			pr_warn("%s: Use random mac address\n", ndev->name);
		}
	}
}

static void geth_clk_enable(struct geth_priv *priv)
{
	int phy_interface = 0;
	u32 clk_value;
	u32 efuse_value;

	if (clk_prepare_enable(priv->geth_clk))
		pr_err("try to enable geth_clk failed!\n");

	if (((priv->phy_ext == INT_PHY) || priv->use_ephy_clk)
			&& !IS_ERR_OR_NULL(priv->ephy_clk)) {
		if (clk_prepare_enable(priv->ephy_clk))
			pr_err("try to enable ephy_clk failed!\n");
	}

	phy_interface = priv->phy_interface;

	clk_value = readl(priv->base_phy);
	if (phy_interface == PHY_INTERFACE_MODE_RGMII)
		clk_value |= 0x00000004;
	else
		clk_value &= (~0x00000004);

	clk_value &= (~0x00002003);
	if (phy_interface == PHY_INTERFACE_MODE_RGMII
			|| phy_interface == PHY_INTERFACE_MODE_GMII)
		clk_value |= 0x00000002;
	else if (phy_interface == PHY_INTERFACE_MODE_RMII)
		clk_value |= 0x00002001;

	if (priv->phy_ext == INT_PHY) {
		if (0 != sunxi_efuse_read(EFUSE_OEM_NAME, &efuse_value))
			pr_err("get PHY efuse fail!\n");
		else
#if defined(CONFIG_ARCH_SUN50IW2)
			clk_value |= (((efuse_value >> 24) & 0x0F) + 3) << 28;
#else
			pr_warn("miss config come from efuse!\n");
#endif
	}

	/* Adjust Tx/Rx clock delay */
	clk_value &= ~(0x07 << 10);
	clk_value |= ((priv->tx_delay & 0x07) << 10);
	clk_value &= ~(0x1F << 5);
	clk_value |= ((priv->rx_delay & 0x1F) << 5);

	writel(clk_value, priv->base_phy);
}

static void geth_clk_disable(struct geth_priv *priv)
{
	if (((priv->phy_ext == INT_PHY) || priv->use_ephy_clk)
			&& !IS_ERR_OR_NULL(priv->ephy_clk))
		clk_disable_unprepare(priv->ephy_clk);

	clk_disable_unprepare(priv->geth_clk);
}

static void geth_tx_err(struct geth_priv *priv)
{
	netif_stop_queue(priv->ndev);

	sunxi_stop_tx(priv->base);

	geth_free_tx_sk(priv);
	memset(priv->dma_tx, 0, dma_desc_tx * sizeof(struct dma_desc));
	desc_init_chain(priv->dma_tx, (unsigned long)priv->dma_tx_phy, dma_desc_tx);
	priv->tx_dirty = 0;
	priv->tx_clean = 0;
	sunxi_start_tx(priv->base, priv->dma_tx_phy);

	priv->ndev->stats.tx_errors++;
	netif_wake_queue(priv->ndev);
}

static inline void geth_schedule(struct geth_priv *priv)
{
	if (likely(napi_schedule_prep(&priv->napi))) {
		sunxi_int_disable(priv->base);
		__napi_schedule(&priv->napi);
	}
}

static irqreturn_t geth_interrupt(int irq, void *dev_id)
{
	struct net_device *ndev = (struct net_device *)dev_id;
	struct geth_priv *priv = netdev_priv(ndev);
	int status;

	if (unlikely(!ndev)) {
		pr_err("%s: invalid ndev pointer\n", __func__);
		return IRQ_NONE;
	}

	status = sunxi_int_status(priv->base, (void *)(&priv->xstats));

	if (likely(status == handle_tx_rx))
		geth_schedule(priv);
	else if (unlikely(status == tx_hard_error_bump_tc))
		netdev_info(ndev, "Do nothing for bump tc\n");
	else if (unlikely(status == tx_hard_error))
		geth_tx_err(priv);
	else
		netdev_info(ndev, "Do nothing.....\n");

	return IRQ_HANDLED;
}

static int geth_open(struct net_device *ndev)
{
	struct geth_priv *priv = netdev_priv(ndev);
	int ret = 0;

	ret = geth_power_on(priv);
	if (ret) {
		netdev_err(ndev, "Power on is failed\n");
		ret = -EINVAL;
	}

	geth_clk_enable(priv);

	netif_carrier_off(ndev);

	ret = geth_phy_init(ndev);
	if (ret)
		goto err;

	ret = sunxi_mac_reset((void *)priv->base, &sunxi_udelay, 10000);
	if (ret) {
		netdev_err(ndev, "Initialize hardware error\n");
		goto desc_err;
	}

	sunxi_mac_init(priv->base, txmode, rxmode);
	sunxi_set_umac(priv->base, ndev->dev_addr, 0);

	if (!priv->is_suspend) {
		ret = geth_dma_desc_init(ndev);
		if (ret) {
			ret = -EINVAL;
			goto desc_err;
		}
	}

	memset(priv->dma_tx, 0, dma_desc_tx * sizeof(struct dma_desc));
	memset(priv->dma_rx, 0, dma_desc_rx * sizeof(struct dma_desc));

	desc_init_chain(priv->dma_rx, (unsigned long)priv->dma_rx_phy, dma_desc_rx);
	desc_init_chain(priv->dma_tx, (unsigned long)priv->dma_tx_phy, dma_desc_tx);

	priv->rx_clean = 0;
	priv->rx_dirty = 0;
	priv->tx_clean = 0;
	priv->tx_dirty = 0;
	geth_rx_refill(ndev);

	/* Extra statistics */
	memset(&priv->xstats, 0, sizeof(struct geth_extra_stats));

	if (ndev->phydev)
		phy_start(ndev->phydev);

	sunxi_start_rx(priv->base, (unsigned long)((struct dma_desc *)
		       priv->dma_rx_phy + priv->rx_dirty));
	sunxi_start_tx(priv->base, (unsigned long)((struct dma_desc *)
		       priv->dma_tx_phy + priv->tx_clean));

	napi_enable(&priv->napi);
	netif_start_queue(ndev);

	/* Enable the Rx/Tx */
	sunxi_mac_enable(priv->base);

	return 0;

desc_err:
	geth_phy_release(ndev);
err:
	geth_clk_disable(priv);
	if (priv->is_suspend)
		napi_enable(&priv->napi);

	geth_power_off(priv);

	return ret;
}

static int geth_stop(struct net_device *ndev)
{
	struct geth_priv *priv = netdev_priv(ndev);

	netif_stop_queue(ndev);
	napi_disable(&priv->napi);

	netif_carrier_off(ndev);

	/* Release PHY resources */
	geth_phy_release(ndev);

	/* Disable Rx/Tx */
	sunxi_mac_disable(priv->base);

	geth_clk_disable(priv);
	geth_power_off(priv);

	netif_tx_lock_bh(ndev);
	/* Release the DMA TX/RX socket buffers */
	geth_free_rx_sk(priv);
	geth_free_tx_sk(priv);
	netif_tx_unlock_bh(ndev);

	/* Ensure that hareware have been stopped */
	if (!priv->is_suspend)
		geth_free_dma_desc(priv);

	return 0;
}

static void geth_tx_complete(struct geth_priv *priv)
{
	unsigned int entry = 0;
	struct sk_buff *skb = NULL;
	struct dma_desc *desc = NULL;
	int tx_stat;

	spin_lock(&priv->tx_lock);
	while (circ_cnt(priv->tx_dirty, priv->tx_clean, dma_desc_tx) > 0) {
		entry = priv->tx_clean;
		desc = priv->dma_tx + entry;

		/* Check if the descriptor is owned by the DMA. */
		if (desc_get_own(desc))
			break;

		/* Verify tx error by looking at the last segment */
		if (desc_get_tx_ls(desc)) {
			tx_stat = desc_get_tx_status(desc, (void *)(&priv->xstats));

			if (likely(!tx_stat))
				priv->ndev->stats.tx_packets++;
			else
				priv->ndev->stats.tx_errors++;
		}

		dma_unmap_single(priv->dev, (u32)desc_buf_get_addr(desc),
				 desc_buf_get_len(desc), DMA_TO_DEVICE);

		skb = priv->tx_sk[entry];
		priv->tx_sk[entry] = NULL;
		desc_init(desc);

		/* Find next dirty desc */
		priv->tx_clean = circ_inc(entry, dma_desc_tx);

		if (unlikely(skb == NULL))
			continue;

		dev_kfree_skb(skb);
	}

	if (unlikely(netif_queue_stopped(priv->ndev)) &&
	    circ_space(priv->tx_dirty, priv->tx_clean, dma_desc_tx) >
	    TX_THRESH) {
		netif_wake_queue(priv->ndev);
	}
	spin_unlock(&priv->tx_lock);
}

static netdev_tx_t geth_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct geth_priv  *priv = netdev_priv(ndev);
	unsigned int entry;
	struct dma_desc *desc, *first;
	unsigned int len, tmp_len = 0;
	int i, csum_insert;
	int nfrags = skb_shinfo(skb)->nr_frags;
	dma_addr_t paddr;

	spin_lock(&priv->tx_lock);
	if (unlikely(circ_space(priv->tx_dirty, priv->tx_clean,
	    dma_desc_tx) < (nfrags + 1))) {
		if (!netif_queue_stopped(ndev)) {
			netdev_err(ndev, "%s: BUG! Tx Ring full when queue awake\n", __func__);
			netif_stop_queue(ndev);
		}
		spin_unlock(&priv->tx_lock);

		return NETDEV_TX_BUSY;
	}

	csum_insert = (skb->ip_summed == CHECKSUM_PARTIAL);
	entry = priv->tx_dirty;
	first = priv->dma_tx + entry;
	desc = priv->dma_tx + entry;

	len = skb_headlen(skb);
	priv->tx_sk[entry] = skb;

#ifdef PKT_DEBUG
	pr_info("======TX PKT DATA: ============\n");
	/* dump the packet */
	print_hex_dump(KERN_DEBUG, "skb->data: ", DUMP_PREFIX_NONE,
		       16, 1, skb->data, 64, true);
#endif

	/* Every desc max size is 2K */
	while (len != 0) {
		desc = priv->dma_tx + entry;
		tmp_len = ((len > MAX_BUF_SZ) ?  MAX_BUF_SZ : len);

		paddr = dma_map_single(priv->dev, skb->data, tmp_len, DMA_TO_DEVICE);
		if (dma_mapping_error(priv->dev, paddr)) {
			dev_kfree_skb(skb);
			return -EIO;
		}
		desc_buf_set(desc, paddr, tmp_len);
		/* Don't set the first's own bit, here */
		if (first != desc) {
			priv->tx_sk[entry] = NULL;
			desc_set_own(desc);
		}

		entry = circ_inc(entry, dma_desc_tx);
		len -= tmp_len;
	}

	for (i = 0; i < nfrags; i++) {
		const skb_frag_t *frag = &skb_shinfo(skb)->frags[i];

		len = skb_frag_size(frag);
		desc = priv->dma_tx + entry;
		paddr = skb_frag_dma_map(priv->dev, frag, 0, len, DMA_TO_DEVICE);
		if (dma_mapping_error(priv->dev, paddr)) {
			dev_kfree_skb(skb);
			return -EIO;
		}

		desc_buf_set(desc, paddr, len);
		desc_set_own(desc);
		priv->tx_sk[entry] = NULL;
		entry = circ_inc(entry, dma_desc_tx);
	}

	ndev->stats.tx_bytes += skb->len;
	priv->tx_dirty = entry;
	desc_tx_close(first, desc, csum_insert);

	desc_set_own(first);
	spin_unlock(&priv->tx_lock);

	if (circ_space(priv->tx_dirty, priv->tx_clean, dma_desc_tx) <=
			(MAX_SKB_FRAGS + 1)) {
		netif_stop_queue(ndev);
		if (circ_space(priv->tx_dirty, priv->tx_clean, dma_desc_tx) >
				TX_THRESH)
			netif_wake_queue(ndev);
	}

#ifdef DEBUG
	pr_info("=======TX Descriptor DMA: 0x%08llx\n", priv->dma_tx_phy);
	pr_info("Tx pointor: dirty: %d, clean: %d\n", priv->tx_dirty, priv->tx_clean);
	desc_print(priv->dma_tx, dma_desc_tx);
#endif
	sunxi_tx_poll(priv->base);
	geth_tx_complete(priv);

	return NETDEV_TX_OK;
}

static int geth_rx(struct geth_priv *priv, int limit)
{
	unsigned int rxcount = 0;
	unsigned int entry;
	struct dma_desc *desc;
	struct sk_buff *skb;
	int status;
	int frame_len;

	while (rxcount < limit) {
		entry = priv->rx_dirty;
		desc = priv->dma_rx + entry;

		if (desc_get_own(desc))
			break;

		rxcount++;
		priv->rx_dirty = circ_inc(priv->rx_dirty, dma_desc_rx);

		/* Get length & status from hardware */
		frame_len = desc_rx_frame_len(desc);
		status = desc_get_rx_status(desc, (void *)(&priv->xstats));

		netdev_dbg(priv->ndev, "Rx frame size %d, status: %d\n",
			   frame_len, status);

		skb = priv->rx_sk[entry];
		if (unlikely(!skb)) {
			netdev_err(priv->ndev, "Skb is null\n");
			priv->ndev->stats.rx_dropped++;
			break;
		}

#ifdef PKT_DEBUG
		pr_info("======RX PKT DATA: ============\n");
		/* dump the packet */
		print_hex_dump(KERN_DEBUG, "skb->data: ", DUMP_PREFIX_NONE,
				16, 1, skb->data, 64, true);
#endif

		if (status == discard_frame) {
			netdev_dbg(priv->ndev, "Get error pkt\n");
			priv->ndev->stats.rx_errors++;
			continue;
		}

		if (unlikely(status != llc_snap))
			frame_len -= ETH_FCS_LEN;

		priv->rx_sk[entry] = NULL;

		skb_put(skb, frame_len);
		dma_unmap_single(priv->dev, (u32)desc_buf_get_addr(desc),
				 desc_buf_get_len(desc), DMA_FROM_DEVICE);

		skb->protocol = eth_type_trans(skb, priv->ndev);

		skb->ip_summed = CHECKSUM_UNNECESSARY;
		napi_gro_receive(&priv->napi, skb);

		priv->ndev->stats.rx_packets++;
		priv->ndev->stats.rx_bytes += frame_len;
	}

#ifdef DEBUG
	if (rxcount > 0) {
		pr_info("======RX Descriptor DMA: 0x%08llx=\n", priv->dma_rx_phy);
		pr_info("RX pointor: dirty: %d, clean: %d\n", priv->rx_dirty, priv->rx_clean);
		desc_print(priv->dma_rx, dma_desc_rx);
	}
#endif
	geth_rx_refill(priv->ndev);

	return rxcount;
}

static int geth_poll(struct napi_struct *napi, int budget)
{
	struct geth_priv *priv = container_of(napi, struct geth_priv, napi);
	int work_done = 0;

	geth_tx_complete(priv);
	work_done = geth_rx(priv, budget);

	if (work_done < budget) {
		napi_complete(napi);
		sunxi_int_enable(priv->base);
	}

	return work_done;
}

static int geth_change_mtu(struct net_device *ndev, int new_mtu)
{
	int max_mtu;

	if (netif_running(ndev)) {
		pr_err("%s: must be stopped to change its MTU\n", ndev->name);
		return -EBUSY;
	}

	max_mtu = SKB_MAX_HEAD(NET_SKB_PAD + NET_IP_ALIGN);

	if ((new_mtu < 46) || (new_mtu > max_mtu)) {
		pr_err("%s: invalid MTU, max MTU is: %d\n", ndev->name, max_mtu);
		return -EINVAL;
	}

	ndev->mtu = new_mtu;
	netdev_update_features(ndev);

	return 0;
}

static netdev_features_t geth_fix_features(struct net_device *ndev,
					   netdev_features_t features)
{
	return features;
}

static void geth_set_rx_mode(struct net_device *ndev)
{
	struct geth_priv *priv = netdev_priv(ndev);
	unsigned int value = 0;

	pr_debug("%s: # mcasts %d, # unicast %d\n",
		 __func__, netdev_mc_count(ndev), netdev_uc_count(ndev));

	spin_lock(&priv->lock);
	if (ndev->flags & IFF_PROMISC) {
		value = GETH_FRAME_FILTER_PR;
	} else if ((netdev_mc_count(ndev) > HASH_TABLE_SIZE) ||
		   (ndev->flags & IFF_ALLMULTI)) {
		value = GETH_FRAME_FILTER_PM;	/* pass all multi */
		sunxi_hash_filter(priv->base, ~0UL, ~0UL);
	} else if (!netdev_mc_empty(ndev)) {
		u32 mc_filter[2];
		struct netdev_hw_addr *ha;

		/* Hash filter for multicast */
		value = GETH_FRAME_FILTER_HMC;

		memset(mc_filter, 0, sizeof(mc_filter));
		netdev_for_each_mc_addr(ha, ndev) {
			/* The upper 6 bits of the calculated CRC are used to
			 *  index the contens of the hash table
			 */
			int bit_nr = bitrev32(~crc32_le(~0, ha->addr, 6)) >> 26;
			/* The most significant bit determines the register to
			 * use (H/L) while the other 5 bits determine the bit
			 * within the register.
			 */
			mc_filter[bit_nr >> 5] |= 1 << (bit_nr & 31);
		}
		sunxi_hash_filter(priv->base, mc_filter[0], mc_filter[1]);
	}

	/* Handle multiple unicast addresses (perfect filtering)*/
	if (netdev_uc_count(ndev) > 16) {
		/* Switch to promiscuous mode is more than 8 addrs are required */
		value |= GETH_FRAME_FILTER_PR;
	} else {
		int reg = 1;
		struct netdev_hw_addr *ha;

		netdev_for_each_uc_addr(ha, ndev) {
			sunxi_set_umac(priv->base, ha->addr, reg);
			reg++;
		}
	}

#ifdef FRAME_FILTER_DEBUG
	/* Enable Receive all mode (to debug filtering_fail errors) */
	value |= GETH_FRAME_FILTER_RA;
#endif
	sunxi_set_filter(priv->base, value);
	spin_unlock(&priv->lock);
}

static void geth_tx_timeout(struct net_device *ndev)
{
	struct geth_priv *priv = netdev_priv(ndev);

	geth_tx_err(priv);
}

static int geth_ioctl(struct net_device *ndev, struct ifreq *rq, int cmd)
{
	if (!netif_running(ndev))
		return -EINVAL;

	if (!ndev->phydev)
		return -EINVAL;

	return phy_mii_ioctl(ndev->phydev, rq, cmd);
}

/* Configuration changes (passed on by ifconfig) */
static int geth_config(struct net_device *ndev, struct ifmap *map)
{
	if (ndev->flags & IFF_UP)	/* can't act on a running interface */
		return -EBUSY;

	/* Don't allow changing the I/O address */
	if (map->base_addr != ndev->base_addr) {
		pr_warn("%s: can't change I/O address\n", ndev->name);
		return -EOPNOTSUPP;
	}

	/* Don't allow changing the IRQ */
	if (map->irq != ndev->irq) {
		pr_warn("%s: can't change IRQ number %d\n", ndev->name, ndev->irq);
		return -EOPNOTSUPP;
	}

	return 0;
}

static int geth_set_mac_address(struct net_device *ndev, void *p)
{
	struct geth_priv *priv = netdev_priv(ndev);
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	memcpy(ndev->dev_addr, addr->sa_data, ndev->addr_len);
	sunxi_set_umac(priv->base, ndev->dev_addr, 0);

	return 0;
}

int geth_set_features(struct net_device *ndev, netdev_features_t features)
{
	struct geth_priv *priv = netdev_priv(ndev);

	if (features & NETIF_F_LOOPBACK && netif_running(ndev))
		sunxi_mac_loopback(priv->base, 1);
	else
		sunxi_mac_loopback(priv->base, 0);

	return 0;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
/* Polling receive - used by NETCONSOLE and other diagnostic tools
 * to allow network I/O with interrupts disabled.
 */
static void geth_poll_controller(struct net_device *dev)
{
	disable_irq(dev->irq);
	geth_interrupt(dev->irq, dev);
	enable_irq(dev->irq);
}
#endif

static const struct net_device_ops geth_netdev_ops = {
	.ndo_init = NULL,
	.ndo_open = geth_open,
	.ndo_start_xmit = geth_xmit,
	.ndo_stop = geth_stop,
	.ndo_change_mtu = geth_change_mtu,
	.ndo_fix_features = geth_fix_features,
	.ndo_set_rx_mode = geth_set_rx_mode,
	.ndo_tx_timeout = geth_tx_timeout,
	.ndo_do_ioctl = geth_ioctl,
	.ndo_set_config = geth_config,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller = geth_poll_controller,
#endif
	.ndo_set_mac_address = geth_set_mac_address,
	.ndo_set_features = geth_set_features,
};

static int geth_check_if_running(struct net_device *ndev)
{
	if (!netif_running(ndev))
		return -EBUSY;
	return 0;
}

static int geth_get_sset_count(struct net_device *netdev, int sset)
{
	int len;

	switch (sset) {
	case ETH_SS_STATS:
		len = 0;
		return len;
	default:
		return -EOPNOTSUPP;
	}
}

static int geth_ethtool_getsettings(struct net_device *ndev,
				    struct ethtool_cmd *cmd)
{
	struct geth_priv *priv = netdev_priv(ndev);
	struct phy_device *phy = ndev->phydev;
	int rc;

	if (phy == NULL) {
		netdev_err(ndev, "%s: %s: PHY is not registered\n",
		       __func__, ndev->name);
		return -ENODEV;
	}

	if (!netif_running(ndev)) {
		pr_err("%s: interface is disabled: we cannot track "
		       "link speed / duplex setting\n", ndev->name);
		return -EBUSY;
	}

	cmd->transceiver = XCVR_INTERNAL;
	spin_lock_irq(&priv->lock);
	rc = phy_ethtool_gset(phy, cmd);
	spin_unlock_irq(&priv->lock);

	return rc;
}

static int geth_ethtool_setsettings(struct net_device *ndev,
				    struct ethtool_cmd *cmd)
{
	struct geth_priv *priv = netdev_priv(ndev);
	struct phy_device *phy = ndev->phydev;
	int rc;

	spin_lock(&priv->lock);
	rc = phy_ethtool_sset(phy, cmd);
	spin_unlock(&priv->lock);

	return rc;
}

static void geth_ethtool_getdrvinfo(struct net_device *ndev,
				    struct ethtool_drvinfo *info)
{
	strlcpy(info->driver, "sunxi_geth", sizeof(info->driver));

#define DRV_MODULE_VERSION "SUNXI Gbgit driver V1.1"

	strcpy(info->version, DRV_MODULE_VERSION);
	info->fw_version[0] = '\0';
}

static const struct ethtool_ops geth_ethtool_ops = {
	.begin = geth_check_if_running,
	.get_settings = geth_ethtool_getsettings,
	.set_settings = geth_ethtool_setsettings,
	.get_link = ethtool_op_get_link,
	.get_pauseparam = NULL,
	.set_pauseparam = NULL,
	.get_ethtool_stats = NULL,
	.get_strings = NULL,
	.get_wol = NULL,
	.set_wol = NULL,
	.get_sset_count = geth_get_sset_count,
	.get_drvinfo = geth_ethtool_getdrvinfo,
};

/* config hardware resource */
static int geth_hw_init(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct geth_priv *priv = netdev_priv(ndev);
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;
	struct resource *res;
	u32 value;
	struct gpio_config cfg;
	const char *gmac_power;
	char power[20];
	int i;

#ifdef CONFIG_SUNXI_EXT_PHY
	priv->phy_ext = EXT_PHY;
#else
	priv->phy_ext = INT_PHY;
#endif

	/* config memery resource */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (unlikely(!res)) {
		pr_err("%s: ERROR: get gmac memory failed", __func__);
		return -ENODEV;
	}

	priv->base = devm_ioremap_resource(&pdev->dev, res);
	if (!priv->base) {
		pr_err("%s: ERROR: gmac memory mapping failed", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (unlikely(!res)) {
		pr_err("%s: ERROR: get phy memory failed", __func__);
		ret = -ENODEV;
		goto mem_err;
	}

	priv->base_phy = devm_ioremap_resource(&pdev->dev, res);
	if (unlikely(!priv->base_phy)) {
		pr_err("%s: ERROR: phy memory mapping failed", __func__);
		ret = -ENOMEM;
		goto mem_err;
	}

	/* config IRQ */
	ndev->irq = platform_get_irq_byname(pdev, "gmacirq");
	if (ndev->irq == -ENXIO) {
		pr_err("%s: ERROR: MAC IRQ not found\n", __func__);
		ret = -ENXIO;
		goto irq_err;
	}

	ret = request_irq(ndev->irq, geth_interrupt, IRQF_SHARED, dev_name(&pdev->dev), ndev);
	if (unlikely(ret < 0)) {
		pr_err("Could not request irq %d, error: %d\n", ndev->irq, ret);
		goto irq_err;
	}

	/* config clock */
	priv->geth_clk = of_clk_get_by_name(np, "gmac");
	if (unlikely(!priv->geth_clk || IS_ERR(priv->geth_clk))) {
		pr_err("Get gmac clock failed!\n");
		ret = -EINVAL;
		goto clk_err;
	}

	if (INT_PHY == priv->phy_ext) {
		priv->ephy_clk = of_clk_get_by_name(np, "ephy");
		if (unlikely(IS_ERR_OR_NULL(priv->ephy_clk))) {
			pr_err("Get ephy clock failed!\n");
			ret = -EINVAL;
			goto clk_err;
		}
	} else {
		if (!of_property_read_u32(np, "use_ephy25m", &(priv->use_ephy_clk))
				&& priv->use_ephy_clk) {
			priv->ephy_clk = of_clk_get_by_name(np, "ephy");
			if (unlikely(IS_ERR_OR_NULL(priv->ephy_clk))) {
				pr_err("Get ephy clk failed!\n");
				ret = -EINVAL;
				goto clk_err;
			}
		}
	}

	/* config power regulator */
	if (EXT_PHY == priv->phy_ext) {
		for (i = 0; i < POWER_CHAN_NUM; i++) {
			snprintf(power, 15, "gmac-power%d", i);
			ret = of_property_read_string(np, power, &gmac_power);
			if (ret) {
				priv->gmac_power[i] = NULL;
				pr_info("gmac-power%d: NULL\n", i);
				continue;
			}
			priv->gmac_power[i] = regulator_get(NULL, gmac_power);
			if (IS_ERR(priv->gmac_power[i])) {
				pr_err("gmac-power%d get error!\n", i);
				ret = -EINVAL;
				goto clk_err;
			}
		}
	}
	/* config other parameters */
	priv->phy_interface = of_get_phy_mode(np);
	if (priv->phy_interface != PHY_INTERFACE_MODE_MII &&
	    priv->phy_interface != PHY_INTERFACE_MODE_RGMII &&
	    priv->phy_interface != PHY_INTERFACE_MODE_RMII) {
		pr_err("Not support phy type!\n");
		priv->phy_interface = PHY_INTERFACE_MODE_MII;
	}

	if (!of_property_read_u32(np, "tx-delay", &value))
		priv->tx_delay = value;

	if (!of_property_read_u32(np, "rx-delay", &value))
		priv->rx_delay = value;

	/*
	 * get az-mode state from dts
	 * 1.az-mode of phy will open in defualt, the linking process takes 3 seconds
	 * 2.if you want to speed up the linking process, can close az-mode of phy in dts
	 * */
	priv->disable_az_mode = of_property_read_bool(np, "disable-az-mode");

	/* config pinctrl */
	if (EXT_PHY == priv->phy_ext) {
		priv->phyrst = of_get_named_gpio_flags(np, "phy-rst", 0, (enum of_gpio_flags *)&cfg);
		priv->rst_active_low = (cfg.data == OF_GPIO_ACTIVE_LOW) ? 1 : 0;

		if (gpio_is_valid(priv->phyrst)) {
			if (gpio_request(priv->phyrst, "phy-rst") < 0) {
				pr_err("gmac gpio request fail!\n");
				ret = -EINVAL;
				goto pin_err;
			}
		}

		priv->pinctrl = devm_pinctrl_get_select_default(&pdev->dev);
		if (IS_ERR_OR_NULL(priv->pinctrl)) {
			pr_err("gmac pinctrl error!\n");
			priv->pinctrl = NULL;
			ret = -EINVAL;
			goto pin_err;
		}
	}

	return 0;

pin_err:
	if (EXT_PHY == priv->phy_ext) {
		for (i = 0; i < POWER_CHAN_NUM; i++) {
			if (IS_ERR_OR_NULL(priv->gmac_power[i]))
				continue;
			regulator_put(priv->gmac_power[i]);
		}
	}
clk_err:
	free_irq(ndev->irq, ndev);
irq_err:
	devm_iounmap(&pdev->dev, priv->base_phy);
mem_err:
	devm_iounmap(&pdev->dev, priv->base);

	return ret;
}

static void geth_hw_release(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct geth_priv *priv = netdev_priv(ndev);
	int i;

	devm_iounmap(&pdev->dev, (priv->base_phy));
	devm_iounmap(&pdev->dev, priv->base);
	free_irq(ndev->irq, ndev);
	if (priv->geth_clk)
		clk_put(priv->geth_clk);

	if (EXT_PHY == priv->phy_ext) {
		for (i = 0; i < POWER_CHAN_NUM; i++) {
			if (IS_ERR_OR_NULL(priv->gmac_power[i]))
				continue;
			regulator_put(priv->gmac_power[i]);
		}

		if (!IS_ERR_OR_NULL(priv->pinctrl))
			devm_pinctrl_put(priv->pinctrl);

		if (gpio_is_valid(priv->phyrst))
			gpio_free(priv->phyrst);
	}

	if (!IS_ERR_OR_NULL(priv->ephy_clk))
		clk_put(priv->ephy_clk);
}

/**
 * geth_probe
 * @pdev: platform device pointer
 * Description: the driver is initialized through platform_device.
 */
static int geth_probe(struct platform_device *pdev)
{
	int ret = 0;
#ifdef CONFIG_RTL8363NB_VB
	//use net_device and geth_priv as global variable.
#else
	struct net_device *ndev = NULL;
	struct geth_priv *priv;
#endif
	pr_info("sunxi gmac driver's version: %s\n", SUNXI_GMAC_VERSION);

#ifdef CONFIG_OF
	pdev->dev.dma_mask = &geth_dma_mask;
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
#endif

	ndev = alloc_etherdev(sizeof(struct geth_priv));
	if (!ndev) {
		dev_err(&pdev->dev, "could not allocate device.\n");
		return -ENOMEM;
	}
	SET_NETDEV_DEV(ndev, &pdev->dev);

	priv = netdev_priv(ndev);
	platform_set_drvdata(pdev, ndev);

	/* Must set private data to pdev, before call it */
	ret = geth_hw_init(pdev);
	if (0 != ret) {
		pr_err("geth_hw_init fail!\n");
		goto hw_err;
	}

#ifdef CONFIG_RTL8363NB_VB
	rtl8363nb_vb_init();
#endif

	/* setup the netdevice, fill the field of netdevice */
	ether_setup(ndev);
	ndev->netdev_ops = &geth_netdev_ops;
	netdev_set_default_ethtool_ops(ndev, &geth_ethtool_ops);
	ndev->base_addr = (unsigned long)priv->base;

	priv->ndev = ndev;
	priv->dev = &pdev->dev;

	/* TODO: support the VLAN frames */
	ndev->hw_features = NETIF_F_SG | NETIF_F_HIGHDMA | NETIF_F_IP_CSUM |
				NETIF_F_IPV6_CSUM | NETIF_F_RXCSUM;

	ndev->features |= ndev->hw_features;
	ndev->hw_features |= NETIF_F_LOOPBACK;
	ndev->priv_flags |= IFF_UNICAST_FLT;

	ndev->watchdog_timeo = msecs_to_jiffies(watchdog);

	netif_napi_add(ndev, &priv->napi, geth_poll,  BUDGET);

	spin_lock_init(&priv->lock);
	spin_lock_init(&priv->tx_lock);

	/* The last val is mdc clock ratio */
	sunxi_geth_register((void *)ndev->base_addr, HW_VERSION, 0x03);

	ret = register_netdev(ndev);
	if (ret) {
		netif_napi_del(&priv->napi);
		pr_err("Error: Register %s failed\n", ndev->name);
		goto reg_err;
	}

	/* Before open the device, the mac address should be set */
	geth_check_addr(ndev, mac_str);

#ifdef CONFIG_GETH_ATTRS
	geth_create_attrs(ndev);
#endif
	device_create_file(&pdev->dev, &dev_attr_gphy_test);
	device_create_file(&pdev->dev, &dev_attr_mii_reg);
	device_create_file(&pdev->dev, &dev_attr_mii_read);
	device_create_file(&pdev->dev, &dev_attr_mii_write);
	device_create_file(&pdev->dev, &dev_attr_loopback_test);
	device_create_file(&pdev->dev, &dev_attr_extra_tx_stats);
	device_create_file(&pdev->dev, &dev_attr_extra_rx_stats);

	device_enable_async_suspend(&pdev->dev);

#ifdef CONFIG_PM
	INIT_WORK(&priv->eth_work, geth_resume_work);
#endif

	return 0;

reg_err:
	geth_hw_release(pdev);
hw_err:
	platform_set_drvdata(pdev, NULL);
	free_netdev(ndev);

	return ret;
}

static int geth_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct geth_priv *priv = netdev_priv(ndev);

	device_remove_file(&pdev->dev, &dev_attr_gphy_test);
	device_remove_file(&pdev->dev, &dev_attr_mii_read);
	device_remove_file(&pdev->dev, &dev_attr_mii_write);
	device_remove_file(&pdev->dev, &dev_attr_extra_tx_stats);
	device_remove_file(&pdev->dev, &dev_attr_extra_rx_stats);

	netif_napi_del(&priv->napi);
	unregister_netdev(ndev);
	geth_hw_release(pdev);
	platform_set_drvdata(pdev, NULL);
	free_netdev(ndev);

	return 0;
}

static const struct of_device_id geth_of_match[] = {
	{.compatible = "allwinner,sunxi-gmac",},
	{},
};
MODULE_DEVICE_TABLE(of, geth_of_match);

static struct platform_driver geth_driver = {
	.probe	= geth_probe,
	.remove = geth_remove,
	.driver = {
		   .name = "sunxi-gmac",
		   .owner = THIS_MODULE,
		   .pm = &geth_pm_ops,
		   .of_match_table = geth_of_match,
	},
};
module_platform_driver(geth_driver);

#ifndef MODULE
static int __init set_mac_addr(char *str)
{
	char *p = str;

	if (str && strlen(str))
		memcpy(mac_str, p, 18);

	return 0;
}
__setup("mac_addr=", set_mac_addr);
#endif

MODULE_DESCRIPTION("Allwinner Gigabit Ethernet driver");
MODULE_AUTHOR("fuzhaoke <fuzhaoke@allwinnertech.com>");
MODULE_LICENSE("Dual BSD/GPL");
