/*
 * Copyright (C) 2016 Allwinnertech
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOS.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/delay.h>
#include <linux/clk/sunxi.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/syscore_ops.h>
#include <linux/module.h>

#include "clk-sunxi.h"
#include "clk-factors.h"
#include "clk-periph.h"
#include "clk-cpu.h"
#include "clk-sun8iw19.h"
#include "clk-sun8iw19_tbl.c"

#define FACTOR_SIZEOF(name) (sizeof(factor_pll##name##_tbl)/ \
			     sizeof(struct sunxi_clk_factor_freq))

#define FACTOR_SEARCH(name) (sunxi_clk_com_ftr_sr( \
		&sunxi_clk_factor_pll_##name, factor, \
		factor_pll##name##_tbl, index, \
		FACTOR_SIZEOF(name)))

#ifndef CONFIG_EVB_PLATFORM
	#define LOCKBIT(x) 31
#else
	#define LOCKBIT(x) x
#endif

DEFINE_SPINLOCK(clk_lock);
void __iomem *sunxi_clk_base;
void __iomem *sunxi_clk_cpus_base;
void __iomem *sunxi_clk_rtc_base;
int sunxi_clk_maxreg = SUNXI_CLK_MAX_REG;
int cpus_clk_maxreg = CPUS_CLK_MAX_REG;

/*                                ns  nw  ks  kw  ms  mw  ps  pw  d1s d1w d2s d2w {frac   out mode}   en-s    sdmss   sdmsw   sdmpat          sdmval*/
SUNXI_CLK_FACTORS(pll_cpu,        8,  8,  0,  0,  0,  2,  16, 2,  0,  0,  0,  0,    0,    0,  0,      31,     0,      0,      0,              0);
SUNXI_CLK_FACTORS(pll_ddr0,       8,  8,  0,  0,  0,  0,  0,  0,  1,  1,  0,  1,    0,    0,  0,      31,     24,     1,      PLL_DDR0PAT,    0xd1303333);
SUNXI_CLK_FACTORS(pll_periph0,    8,  8,  0,  0,  0,  0,  0,  0,  1,  1,  0,  1,    0,    0,  0,      31,     0,      0,      PLL_PERI0PAT0,  0xd1303333);
SUNXI_CLK_FACTORS(pll_uni,        8,  8,  0,  0,  0,  0,  0,  0,  1,  1,  0,  1,    0,    0,  0,      31,     24,     1,      PLL_UNIPAT0,    0xd1303333);
SUNXI_CLK_FACTORS(pll_video0,     8,  8,  0,  0,  0,  0,  0,  0,  1,  1,  0,  0,    0,    0,  0,      31,     0,      0,      PLL_VIDEO0PAT0, 0xd1303333);
SUNXI_CLK_FACTORS(pll_audio,      8,  8,  0,  0,  0,  0,  16, 6,  1,  1,  0,  1,    0,    0,  0,      31,     24,     1,      PLL_AUDIOPAT0,  0xd1303333);
SUNXI_CLK_FACTORS(pll_csi,        8,  8,  0,  0,  0,  0,  0,  0,  1,  1,  0,  1,    0,    0,  0,      31,     0,      0,      PLL_CSIPAT0,    0xd1303333);


static int get_factors_pll_cpu(u32 rate, u32 parent_rate,
		struct clk_factors_value *factor)
{

	int index;
	u64 tmp_rate;

	if (!factor)
		return -1;
	tmp_rate = rate > pllcpu_max ? pllcpu_max : rate;
	do_div(tmp_rate, 1000000);
	index = tmp_rate;

	if (FACTOR_SEARCH(cpu))
		return -1;

	return 0;
}

static int get_factors_pll_ddr0(u32 rate, u32 parent_rate,
		struct clk_factors_value *factor)
{
	int index;
	u64 tmp_rate;

	if (!factor)
		return -1;

	tmp_rate = rate > pllddr0_max ? pllddr0_max : rate;
	do_div(tmp_rate, 1000000);
	index = tmp_rate;

	if (FACTOR_SEARCH(ddr0))
		return -1;

	return 0;
}


static int get_factors_pll_periph0(u32 rate, u32 parent_rate,
		struct clk_factors_value *factor)
{
	int index;
	u64 tmp_rate;

	if (!factor)
		return -1;

	tmp_rate = rate > pllperiph0_max ? pllperiph0_max : rate;
	do_div(tmp_rate, 1000000);
	index = tmp_rate;

	if (FACTOR_SEARCH(periph0))
		return -1;

	return 0;
}

static int get_factors_pll_uni(u32 rate, u32 parent_rate,
		struct clk_factors_value *factor)
{
	int index;
	u64 tmp_rate;

	if (!factor)
		return -1;

	tmp_rate = rate > pllperiph0_max ? pllperiph0_max : rate;
	do_div(tmp_rate, 1000000);
	index = tmp_rate;

	if (FACTOR_SEARCH(periph0))
		return -1;

	return 0;
}

static int get_factors_pll_video0(u32 rate, u32 parent_rate,
		struct clk_factors_value *factor)
{
	u64 tmp_rate;
	int index;

	if (!factor)
		return -1;

	tmp_rate = rate > pllvideo0_max ? pllvideo0_max : rate;
	do_div(tmp_rate, 1000000);
	index = tmp_rate;

	if (FACTOR_SEARCH(video0))
		return -1;

	return 0;
}

static int get_factors_pll_csi(u32 rate, u32 parent_rate,
		struct clk_factors_value *factor)
{
	u64 tmp_rate;
	int index;

	if (!factor)
		return -1;

	tmp_rate = rate > pllddr0_max ? pllddr0_max : rate;
	do_div(tmp_rate, 1000000);
	index = tmp_rate;

	if (FACTOR_SEARCH(ddr0))
		return -1;

	return 0;
}
/*    pll_cpux: 24*N/M/P (P=2^factorp)  */
static unsigned long calc_rate_pll_cpu(u32 parent_rate,
		struct clk_factors_value *factor)
{
	u64 tmp_rate = (parent_rate ? parent_rate : 24000000);

	tmp_rate = tmp_rate * (factor->factorn + 1);
	do_div(tmp_rate, (factor->factorm + 1) * (1 << factor->factorp));
	return (unsigned long)tmp_rate;
}

/*    pll_ddr: 24*N/D1/D2    */
static unsigned long calc_rate_pll_ddr(u32 parent_rate,
		struct clk_factors_value *factor)
{
	u64 tmp_rate = (parent_rate ? parent_rate : 24000000);

	tmp_rate = tmp_rate * (factor->factorn + 1);
	do_div(tmp_rate, (factor->factord1 + 1) * (factor->factord2 + 1));
	return (unsigned long)tmp_rate;
}

/*    pll_periph0/pll_periph1/pll_uni: 24*N/D1/D2/2    */
static unsigned long calc_rate_pll_periph(u32 parent_rate,
		struct clk_factors_value *factor)
{
	u64 tmp_rate = (parent_rate ? parent_rate : 24000000);

	tmp_rate = tmp_rate * (factor->factorn + 1);
	do_div(tmp_rate, 2 * (factor->factord1 + 1) * (factor->factord2 + 1));
	return (unsigned long)tmp_rate;
}

/*    pll_video0: 24*N/D1/4    */
static unsigned long calc_rate_video(u32 parent_rate,
		struct clk_factors_value *factor)
{
	u64 tmp_rate = (parent_rate ? parent_rate : 24000000);

	tmp_rate = tmp_rate * (factor->factorn + 1);
	do_div(tmp_rate, 4 * (factor->factord1 + 1));
	return (unsigned long)tmp_rate;
}

/*    pll_media: 24*N/D1/D2    */
static unsigned long calc_rate_media(u32 parent_rate,
		struct clk_factors_value *factor)
{
	u64 tmp_rate = (parent_rate ? parent_rate : 24000000);

	tmp_rate = tmp_rate * (factor->factorn + 1);
	do_div(tmp_rate, (factor->factord1 + 1) * (factor->factord2 + 1));
	return (unsigned long)tmp_rate;
}

/*
 *    pll_audio: 24*N/D1/D2/P
 *
 *    NOTE: pll_audiox4 = 24*N/D1/2
 *          pll_audiox2 = 24*N/D1/4
 *
 *    pll_audiox4=2*pll_audiox2=4*pll_audio only when D2*P=8
 */
static unsigned long calc_rate_audio(u32 parent_rate,
		struct clk_factors_value *factor)
{
	u64 tmp_rate = (parent_rate ? parent_rate : 24000000);
	if ((factor->factorn == 32) &&
			(factor->factorp == 17) &&
			(factor->factord1 == 0) &&
			(factor->factord2 == 1))
		return 22579200;
	else if ((factor->factorn == 39) &&
			(factor->factorp == 19) &&
			(factor->factord1 == 0) &&
			(factor->factord2 == 1))
		return 24576000;
	else {
		tmp_rate = tmp_rate * (factor->factorn + 1);
		do_div(tmp_rate, ((factor->factorp + 1) *
				(factor->factord1 + 1) *
				(factor->factord2 + 1)));
		return (unsigned long)tmp_rate;
	}
}

static int get_factors_pll_audio(u32 rate, u32 parent_rate,
		struct clk_factors_value *factor)
{
	if (rate == 22579200) {
		factor->factorn = 32;
		factor->factorp = 17;
		factor->factord1 = 0;
		factor->factord2 = 1;
		sunxi_clk_factor_pll_audio.sdmval = 0xC001BCD3;
	} else if (rate == 24576000) {
		factor->factorn = 39;
		factor->factorp = 19;
		factor->factord1 = 0;
		factor->factord2 = 1;
		sunxi_clk_factor_pll_audio.sdmval = 0xC001EB85;
	} else
		return -1;

	return 0;
}

static const char *hosc_parents[] = {"hosc"};
struct factor_init_data sunxi_factos[] = {
	/* name         parent        parent_num, flags                                      reg          lock_reg     lock_bit     pll_lock_ctrl_reg lock_en_bit lock_mode           config                         get_factors               calc_rate              priv_ops*/
	{"pll_cpu",     hosc_parents, 1,          CLK_GET_RATE_NOCACHE|CLK_NO_DISABLE, PLL_CPU,     PLL_CPU,     LOCKBIT(28), PLL_CPU,     29,          PLL_LOCK_NEW_MODE, &sunxi_clk_factor_pll_cpu,     &get_factors_pll_cpu,     &calc_rate_pll_cpu,    (struct clk_ops *)NULL},
	{"pll_ddr0",    hosc_parents, 1,          CLK_GET_RATE_NOCACHE|CLK_NO_DISABLE, PLL_DDR0,    PLL_DDR0,    LOCKBIT(28), PLL_DDR0,    29,          PLL_LOCK_NEW_MODE, &sunxi_clk_factor_pll_ddr0,    &get_factors_pll_ddr0,    &calc_rate_pll_ddr,    (struct clk_ops *)NULL},
	{"pll_periph0", hosc_parents, 1,          CLK_NO_DISABLE,         PLL_PERIPH0, PLL_PERIPH0, LOCKBIT(28), PLL_PERIPH0, 29,          PLL_LOCK_NEW_MODE, &sunxi_clk_factor_pll_periph0, &get_factors_pll_periph0, &calc_rate_pll_periph, (struct clk_ops *)NULL},
	{"pll_uni",     hosc_parents, 1,          CLK_NO_DISABLE,         PLL_UNI, 	   PLL_UNI,     LOCKBIT(28), PLL_UNI,     29,          PLL_LOCK_NEW_MODE, &sunxi_clk_factor_pll_uni,     &get_factors_pll_uni,     &calc_rate_pll_periph, (struct clk_ops *)NULL},
	{"pll_video0",  hosc_parents, 1,          CLK_NO_DISABLE,         PLL_VIDEO0,  PLL_VIDEO0,  LOCKBIT(28), PLL_VIDEO0,  29,          PLL_LOCK_NEW_MODE, &sunxi_clk_factor_pll_video0,  &get_factors_pll_video0,  &calc_rate_video,      (struct clk_ops *)NULL},
	{"pll_audio",   hosc_parents, 1,          CLK_NO_DISABLE,         PLL_AUDIO,   PLL_AUDIO,   LOCKBIT(28), PLL_AUDIO,   29,          PLL_LOCK_NEW_MODE, &sunxi_clk_factor_pll_audio,   &get_factors_pll_audio,   &calc_rate_audio,      (struct clk_ops *)NULL},
	{"pll_csi",     hosc_parents, 1,          CLK_NO_DISABLE,         PLL_CSI,     PLL_CSI,     LOCKBIT(28), PLL_CSI,     29,          PLL_LOCK_NEW_MODE, &sunxi_clk_factor_pll_csi,     &get_factors_pll_csi,     &calc_rate_media,       (struct clk_ops *)NULL},
};

static const char *cpu_parents[] = {"hosc", "losc", "iosc", "pll_cpu", "pll_periph0"};
static const char *axi_parents[] = {"cpu"};
static const char *psi_parents[] = {"hosc", "losc", "iosc", "pll_periph0"};
static const char *ahb1_parents[] = {"psi"};
static const char *ahb3_parents[] = {"hosc", "losc", "psi", "pll_periph0"};
static const char *mbus_parents[] = {"pll_ddr0"};
static const char *de_parents[] = {"pll_uni", "pll_unix2", "pll_periph0x2"};
static const char *isp_parents[] = {"pll_uni", "pll_unix2", "pll_periph0", "pll_periph0x2", "pll_video0x4", "pll_csi"};
static const char *ce_parents[] = {"hosc", "pll_periph0x2"};
static const char *ve_parents[] = {"pll_uni", "pll_unix2", "pll_periph0", "pll_video0x4"};
static const char *ahb1mod_parents[] = {"ahb1"};
static const char *ahb3mod_parents[] = {"ahb3"};
static const char *apb1mod_parents[] = {"apb1"};
static const char *apb2mod_parents[] = {"apb2"};
static const char *nna_parents[] = {"pll_periph0", "pll_periph0x2", "pll_uni", "pll_unix2", "pll_video0x4", "pll_cpu", "pll_csi"};
static const char *sdram_parents[] = {"pll_ddr0", "pll_periph0x2"};
static const char *smhc_parents[] = {"hosc", "pll_periph0x2", "pll_unix2"};
static const char *spi_parents[] = {"hosc", "pll_periph0", "pll_uni", "pll_periph0x2", "pll_unix2"};
static const char *ephy_25m_parents[] = {"pll_periph0div25m"};
static const char *audio_parents[] = {"pll_audio", "pll_audiox2", "pll_audiox4"};
static const char *usbohci_12m_parents[] = {"osc48md4", "hoscd2", "losc"};
static const char *mipi_dphy_parents[] = {"pll_video0", "pll_video0x4"};
static const char *mipi_host_parents[] = {"pll_periph0", "pll_periph0x2", "hosc"};
static const char *tcon_lcd_parents[] = {"pll_video0", "pll_video0x4"};
static const char *csi_top_parents[] = {"pll_uni", "pll_unix2", "pll_periph0", "pll_periph0x2", "pll_video0x4", "pll_csi"};
static const char *csi_master_parents[] = {"hosc", "pll_uni", "pll_unix2", "pll_periph0", "pll_periph0x2", "pll_video0", "pll_csi"};
static const char *dspo_parents[] = {"pll_video0", "pll_video0x4", "pll_periph0", "pll_periph0x2", "pll_uni", "pll_unix2", "pll_csi"};
static const char *cpurcpus_parents[] = {"hosc", "losc", "iosc", "cpurcpus_pll"};
static const char *cpurahbs_parents[] = {"cpurcpus"};
static const char *cpurapbs1_parents[] = {"cpurahbs"};
static const char *cpurapbs2_parents[] = {"hosc", "losc", "iosc", "pll_periph0"};
static const char *cpurdev_parents[] = {"losc", "hosc"};
static const char *cpurpio_parents[] = {"cpurapbs1"};
static const char *stwi_parents[] = {"cpurapbs1"};
static const char *losc_parents[] = {"losc"};
static const char *spwm_parents[] = {"hosc"};
static const char *dcxo_parents[] = {"hosc"};
static const char *cpurapbs2_pll_parents[] = {"pll_periph0"};
static const char *cpurcpus_pll_parents[] = {"pll_periph0"};

struct sunxi_clk_comgate com_gates[] = {
{"nand",    0, 0x3, BUS_GATE_SHARE|RST_GATE_SHARE|MBUS_GATE_SHARE, 0},
{"csi",     0, 0x1ff, BUS_GATE_SHARE|RST_GATE_SHARE|MBUS_GATE_SHARE, 0},
{"mipi",    0, 0x3, BUS_GATE_SHARE|RST_GATE_SHARE|MBUS_GATE_SHARE, 0},
{"codec",   0, 0x3, BUS_GATE_SHARE|RST_GATE_SHARE, 0},
};

/*
SUNXI_CLK_PERIPH(name,           mux_reg,         mux_sft, mux_wid,      div_reg,            div_msft,  div_mwid,   div_nsft,   div_nwid,   gate_flag,  en_reg,          rst_reg,         bus_gate_reg,  drm_gate_reg,  en_sft,     rst_sft,    bus_gate_sft,   dram_gate_sft, lock,      com_gate,         com_gate_off)
*/
SUNXI_CLK_PERIPH(cpu,            CPU_CFG,         24,      3,            0,                  0,         0,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(axi,            0,               0,       0,            CPU_CFG,            0,         2,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpuapb,         0,               0,       0,            CPU_CFG,            8,         2,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(psi,            PSI_CFG,         24,      2,            PSI_CFG,            0,         2,          8,          2,          0,          0,               PSI_GATE,        PSI_GATE,      0,             0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(ahb1,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(ahb2,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(ahb3,           AHB3_CFG,        24,      2,            AHB3_CFG,           0,         2,          8,          2,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(apb1,           APB1_CFG,        24,      2,            APB1_CFG,           0,         2,          8,          2,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(apb2,           APB2_CFG,        24,      2,            APB2_CFG,           0,         2,          8,          2,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(mbus,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               MBUS_CFG,        0,             0,             0,          30,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(de,             DE_CFG,          24,      2,            DE_CFG,             0,         5,          0,          0,          0,          DE_CFG,          DE_GATE,         DE_GATE,       0,             31,         16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(g2d,            G2D_CFG,         24,      2,            G2D_CFG,            0,         5,          0,          0,          0,          G2D_CFG,         G2D_GATE,        G2D_GATE,      MBUS_GATE,     31,         16,         0,              10,            &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(ce,             CE_CFG,          24,      1,            CE_CFG,             0,         4,          8,          2,          0,          CE_CFG,          CE_GATE,         CE_GATE,       MBUS_GATE,     31,         16,         0,              2,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(ve,             VE_CFG,          24,      2,            VE_CFG,             0,         4,          0,          0,          0,          VE_CFG,          VE_GATE,         VE_GATE,       MBUS_GATE,     31,         16,         0,              1,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(eise,           EISE_CFG,        24,      2,            EISE_CFG,           0,         5,          0,          0,          0,          EISE_CFG,        EISE_GATE,       EISE_GATE,     MBUS_GATE,     31,         16,         0,              23,            &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(nna,            NNA_CFG,         24,      2,            NNA_CFG,            0,         5,          0,          0,          0,          NNA_CFG,         NNA_GATE,        NNA_GATE,      0,             31,         16,          0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(nna_rst,        0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               NNA_CFG,         0,             0,             0,           30,         0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(dma,            0,                0,      0,            0,                  0,         0,          0,          0,          0,          0,               DMA_GATE,        DMA_GATE,      MBUS_GATE,     0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(hstimer,        0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               HSTIMER_GATE,    HSTIMER_GATE,     0,          0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(avs,            0,               0,       0,            0,                  0,         0,          0,          0,          0,          AVS_CFG,         0,               0,                0,          31,         0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(dbgsys,         0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               DBGSYS_GATE,     DBGSYS_GATE,      0,          0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(pwm,            0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               PWM_GATE,        PWM_GATE,         0,          0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(iommu,          0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,               IOMMU_GATE,       0,          0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(sdram,          DRAM_CFG,        24,      2,            DRAM_CFG,           0,         2,          0,          0,          0,          0,               DRAM_GATE,       DRAM_GATE,        0,          0,          16,         0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(sdmmc0_mod,     SMHC0_CFG,       24,      2,            SMHC0_CFG,          0,         4,          8,          2,          0,          SMHC0_CFG,       0,              0,                 0,          31,         0,          0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(sdmmc0_rst,     0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               SMHC_GATE,      0,                 0,          0,          16,         0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(sdmmc0_bus,     0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,              SMHC_GATE,         0,          0,          0,          0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(sdmmc1_mod,     SMHC1_CFG,       24,      2,            SMHC1_CFG,          0,         4,          8,          2,          0,          SMHC1_CFG,       0,              0,                 0,          31,         0,          0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(sdmmc1_rst,     0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               SMHC_GATE,      0,                 0,          0,          17,         0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(sdmmc1_bus,     0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,              SMHC_GATE,         0,          0,          0,          1,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(sdmmc2_mod,     SMHC2_CFG,       24,      2,            SMHC2_CFG,          0,         4,          8,          2,          0,          SMHC2_CFG,       0,              0,                 0,          31,         0,          0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(sdmmc2_rst,     0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               SMHC_GATE,      0,                 0,          0,          18,         0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(sdmmc2_bus,     0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,              SMHC_GATE,         0,          0,          0,          2,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(uart0,          0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               UART_GATE,      UART_GATE,          0,         0,          16,         0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(uart1,          0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               UART_GATE,      UART_GATE,          0,         0,          17,         1,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(uart2,          0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               UART_GATE,      UART_GATE,          0,         0,          18,         2,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(uart3,          0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               UART_GATE,      UART_GATE,          0,         0,          19,         3,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(twi0,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               TWI_GATE,       TWI_GATE,           0,         0,          16,         0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(twi1,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               TWI_GATE,       TWI_GATE,           0,         0,          17,         1,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(twi2,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               TWI_GATE,       TWI_GATE,           0,         0,          18,         2,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(twi3,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               TWI_GATE,       TWI_GATE,           0,         0,          19,         3,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(spi0,           SPI0_CFG,        24,      3,            SPI0_CFG,           0,         4,          8,          2,          0,          SPI0_CFG,        SPI_GATE,       SPI_GATE,          0,          31,         16,         0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(spi1,           SPI1_CFG,        24,      3,            SPI1_CFG,           0,         4,          8,          2,          0,          SPI1_CFG,        SPI_GATE,       SPI_GATE,          0,          31,         17,         1,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(spi2,           SPI2_CFG,        24,      3,            SPI2_CFG,           0,         4,          8,          2,          0,          SPI2_CFG,        SPI_GATE,       SPI_GATE,          0,          31,         18,         2,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(ephy_25m,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          EPHY_25M_CFG,    0,              EPHY_25M_CFG,      0,          31,         0,          30,            0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(gmac,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               GMAC_GATE,      GMAC_GATE,         0,          0,          16,         0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(gpadc,          0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               GPADC_GATE,     GPADC_GATE,        0,          0,          16,         0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(ths,            0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               THS_GATE,        THS_GATE,         0,          0,          16,         0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(i2s0,           I2S0_CFG,        24,      2,            I2S0_CFG,           0,         0,          8,          2,          0,          I2S0_CFG,        I2S_GATE,        I2S_GATE,         0,          31,         16,         0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(i2s1,           I2S1_CFG,        24,      2,            I2S1_CFG,           0,         0,          8,          2,          0,          I2S1_CFG,        I2S_GATE,        I2S_GATE,         0,          31,         17,         1,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(codec_1x,       CODEC_1X_CFG,    24,      2,            CODEC_1X_CFG,       0,         4,          0,          0,          0,          CODEC_1X_CFG,    CODEC_GATE,      CODEC_GATE,       0,          31,         16,         0,             0,             &clk_lock, &com_gates[3],    0);
SUNXI_CLK_PERIPH(codec_4x,       CODEC_4X_CFG,    24,      2,            CODEC_4X_CFG,       0,         4,          0,          0,          0,          CODEC_4X_CFG,    CODEC_GATE,      CODEC_GATE,       0,          31,         16,         0,             0,             &clk_lock, &com_gates[3],    1);
SUNXI_CLK_PERIPH(usbphy0,        0,               0,       0,            0,                  0,         0,          0,          0,          0,          USB0_CFG,        USB0_CFG,        0,                0,          29,         30,         0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(usbohci0,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          USB0_CFG,        USB_GATE,        USB_GATE,         0,          31,         16,         0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(usbohci0_12m,   USB0_CFG,        24,      2,            0,                  0,         0,          0,          0,          0,          0,               0,               0,                0,          0,          0,          0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(usbehci0,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               USB_GATE,        USB_GATE,         0,          0,          20,         4,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(usbotg,         0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               USB_GATE,        USB_GATE,         0,          0,          24,         8,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(mipi_dphy0,     MIPI_DPHY0_CFG,  24,      2,            MIPI_DPHY0_CFG,     0,         4,          8,          2,          0,          MIPI_DPHY0_CFG,  MIPI_GATE,       MIPI_GATE,        0,          31,         16,         0,            0,              &clk_lock, &com_gates[2],    0);
SUNXI_CLK_PERIPH(mipi_host0,     MIPI_HOST0_CFG,  24,      2,            MIPI_HOST0_CFG,     0,         4,          0,          0,          0,          MIPI_HOST0_CFG,  MIPI_GATE,       MIPI_GATE,        0,          31,         16,         0,            0,              &clk_lock, &com_gates[2],    1);
SUNXI_CLK_PERIPH(display_top,    0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               DISPLAY_TOP_GATE, DISPLAY_TOP_GATE, 0,         0,          16,         0,            0,              &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(tcon_lcd,       TCON_LCD_CFG,    24,      3,            0,                  0,         0,          0,          0,          0,          TCON_LCD_CFG,    TCON_LCD_GATE,   TCON_LCD_GATE,    0,          31,         16,         0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(csi_top,        CSI_TOP_CFG,     24,      3,            CSI_TOP_CFG,        0,         5,          0,          0,          0,          CSI_TOP_CFG,     CSI_GATE,        CSI_GATE,      	MBUS_GATE,  31,         16,         0,             8,             &clk_lock, &com_gates[1],    0);
SUNXI_CLK_PERIPH(csi_master0,    CSI_MASTER0_CFG, 24,      3,            CSI_MASTER0_CFG,    0,         5,          0,          0,          0,          CSI_MASTER0_CFG, 0,               0,                0,          31,         16,         0,             8,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(csi_master1,    CSI_MASTER1_CFG, 24,      3,            CSI_MASTER1_CFG,    0,         5,          0,          0,          0,          CSI_MASTER1_CFG, 0,               0,                0,          31,         16,         0,             8,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(isp,            ISP_CFG,         24,      3,            ISP_CFG,            0,         5,          0,          0,          0,          ISP_CFG,         0,               0,             	MBUS_GATE,  31,         0,          0,             9,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(dspo,           DSPO_CFG,        24,      3,            DSPO_CFG,           0,         5,          8,          2,          0,          DSPO_CFG,        DSPO_GATE,       DSPO_GATE,        0,          31,         16,         0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(pio,            0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurcpus_pll,   0,               0,       0,            CPUS_CFG,           0,         5,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurcpus,       CPUS_CFG,        24,      2,            CPUS_CFG,           0,         0,          8,          2,          0,          0,               0,               0,             0,             0,          0,          0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurahbs,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurapbs1,      0,               0,       0,            CPUS_APBS1_CFG,     0,         2,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurapbs2_pll,  0,               0,       0,            CPUS_APBS2_CFG,     0,         5,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurapbs2,      CPUS_APBS2_CFG,  24,      2,            CPUS_APBS2_CFG,     0,         0,          8,          2,          0,          0,               0,               0,             0,             0,          0,          0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurowc,        CPUS_OWC_CFG,    24,      1,            CPUS_OWC_CFG,       0,         5,          8,          2,          0,          CPUS_OWC_CFG,    CPUS_OWC_GATE,   CPUS_OWC_GATE, 0,             31,         16,         0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(losc_out,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,               LOSC_OUT_GATE, 0,             0,          0,          0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurpio,        0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(spwm,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CPUS_PWM_GATE,   CPUS_PWM_GATE, 0,             0,          16,         0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(stwi,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CPUS_TWI_GATE,   CPUS_TWI_GATE, 0,             0,          16,         0,             0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(dcxo_out,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          DCXO_OUT_CFG,    0,               0,             0,             31,         0,          0,             0,             &clk_lock, NULL,             0);

struct periph_init_data sunxi_periphs_init[] = {
	{"cpu",            CLK_GET_RATE_NOCACHE | CLK_CPU_CHANGE_STABLE, cpu_parents,            ARRAY_SIZE(cpu_parents),            &sunxi_clk_periph_cpu              },
	{"axi",            0,                    axi_parents,            ARRAY_SIZE(axi_parents),            &sunxi_clk_periph_axi              },
	{"cpuapb",         0,                    axi_parents,            ARRAY_SIZE(axi_parents),            &sunxi_clk_periph_cpuapb           },
	{"psi",            0,                    psi_parents,            ARRAY_SIZE(psi_parents),            &sunxi_clk_periph_psi              },
	{"ahb1",           0,                    ahb1_parents,           ARRAY_SIZE(ahb1_parents),           &sunxi_clk_periph_ahb1             },
	{"ahb2",           0,                    ahb1_parents,           ARRAY_SIZE(ahb1_parents),           &sunxi_clk_periph_ahb2             },
	{"ahb3",           0,                    ahb3_parents,           ARRAY_SIZE(ahb3_parents),           &sunxi_clk_periph_ahb3             },
	{"apb1",           0,                    ahb3_parents,           ARRAY_SIZE(ahb3_parents),           &sunxi_clk_periph_apb1             },
	{"apb2",           0,                    ahb3_parents,           ARRAY_SIZE(ahb3_parents),           &sunxi_clk_periph_apb2             },
	{"mbus",           0,                    mbus_parents,           ARRAY_SIZE(mbus_parents),           &sunxi_clk_periph_mbus             },
	{"de",             0,                    de_parents,             ARRAY_SIZE(de_parents),             &sunxi_clk_periph_de               },
	{"g2d",            0,                    de_parents,             ARRAY_SIZE(de_parents),             &sunxi_clk_periph_g2d              },
	{"ce",             0,                    ce_parents,             ARRAY_SIZE(ce_parents),             &sunxi_clk_periph_ce               },
	{"ve",             0,                    ve_parents,             ARRAY_SIZE(ve_parents),             &sunxi_clk_periph_ve               },
	{"eise",           0,                    ve_parents,             ARRAY_SIZE(ve_parents),             &sunxi_clk_periph_eise             },
	{"nna",            0,                    nna_parents,            ARRAY_SIZE(nna_parents),            &sunxi_clk_periph_nna              },
	{"nna_rst",        0,                    nna_parents,            ARRAY_SIZE(nna_parents),            &sunxi_clk_periph_nna_rst          },
	{"dma",            0,                    ahb1mod_parents,        ARRAY_SIZE(ahb1mod_parents),        &sunxi_clk_periph_dma              },
	{"hstimer",        0,                    ahb1mod_parents,        ARRAY_SIZE(ahb1mod_parents),        &sunxi_clk_periph_hstimer          },
	{"avs",            0,                    hosc_parents,           ARRAY_SIZE(hosc_parents),           &sunxi_clk_periph_avs              },
	{"dbgsys",         0,                    hosc_parents,           ARRAY_SIZE(hosc_parents),           &sunxi_clk_periph_dbgsys           },
	{"pwm",            0,                    apb1mod_parents,        ARRAY_SIZE(apb1mod_parents),        &sunxi_clk_periph_pwm              },
	{"iommu",          0,                    ahb1mod_parents,        ARRAY_SIZE(ahb1mod_parents),        &sunxi_clk_periph_iommu            },
	{"sdram",          0,                    sdram_parents,          ARRAY_SIZE(sdram_parents),          &sunxi_clk_periph_sdram            },
	{"sdmmc0_mod",     0,                    smhc_parents,           ARRAY_SIZE(smhc_parents),           &sunxi_clk_periph_sdmmc0_mod       },
	{"sdmmc0_rst",     0,                    smhc_parents,           ARRAY_SIZE(smhc_parents),           &sunxi_clk_periph_sdmmc0_rst       },
	{"sdmmc0_bus",     0,                    smhc_parents,           ARRAY_SIZE(smhc_parents),           &sunxi_clk_periph_sdmmc0_bus       },
	{"sdmmc1_mod",     0,                    smhc_parents,           ARRAY_SIZE(smhc_parents),           &sunxi_clk_periph_sdmmc1_mod       },
	{"sdmmc1_rst",     0,                    smhc_parents,           ARRAY_SIZE(smhc_parents),           &sunxi_clk_periph_sdmmc1_rst       },
	{"sdmmc1_bus",     0,                    smhc_parents,           ARRAY_SIZE(smhc_parents),           &sunxi_clk_periph_sdmmc1_bus       },
	{"sdmmc2_mod",     0,                    smhc_parents,           ARRAY_SIZE(smhc_parents),           &sunxi_clk_periph_sdmmc2_mod       },
	{"sdmmc2_rst",     0,                    smhc_parents,           ARRAY_SIZE(smhc_parents),           &sunxi_clk_periph_sdmmc2_rst       },
	{"sdmmc2_bus",     0,                    smhc_parents,           ARRAY_SIZE(smhc_parents),           &sunxi_clk_periph_sdmmc2_bus       },
	{"uart0",          0,                    apb2mod_parents,        ARRAY_SIZE(apb2mod_parents),        &sunxi_clk_periph_uart0            },
	{"uart1",          0,                    apb2mod_parents,        ARRAY_SIZE(apb2mod_parents),        &sunxi_clk_periph_uart1            },
	{"uart2",          0,                    apb2mod_parents,        ARRAY_SIZE(apb2mod_parents),        &sunxi_clk_periph_uart2            },
	{"uart3",          0,                    apb2mod_parents,        ARRAY_SIZE(apb2mod_parents),        &sunxi_clk_periph_uart3            },
	{"twi0",           0,                    apb2mod_parents,        ARRAY_SIZE(apb2mod_parents),        &sunxi_clk_periph_twi0             },
	{"twi1",           0,                    apb2mod_parents,        ARRAY_SIZE(apb2mod_parents),        &sunxi_clk_periph_twi1             },
	{"twi2",           0,                    apb2mod_parents,        ARRAY_SIZE(apb2mod_parents),        &sunxi_clk_periph_twi2             },
	{"twi3",           0,                    apb2mod_parents,        ARRAY_SIZE(apb2mod_parents),        &sunxi_clk_periph_twi3             },
	{"spi0",           0,                    spi_parents,            ARRAY_SIZE(spi_parents),            &sunxi_clk_periph_spi0             },
	{"spi1",           0,                    spi_parents,            ARRAY_SIZE(spi_parents),            &sunxi_clk_periph_spi1             },
	{"spi2",           0,                    spi_parents,            ARRAY_SIZE(spi_parents),            &sunxi_clk_periph_spi2             },
	{"ephy_25m",       0,                    ephy_25m_parents,       ARRAY_SIZE(ephy_25m_parents),       &sunxi_clk_periph_ephy_25m         },
	{"gmac",           0,                    ahb3mod_parents,        ARRAY_SIZE(ahb3mod_parents),        &sunxi_clk_periph_gmac             },
	{"gpadc",          0,                    hosc_parents,           ARRAY_SIZE(hosc_parents),           &sunxi_clk_periph_gpadc            },
	{"ths",            0,                    hosc_parents,           ARRAY_SIZE(hosc_parents),          &sunxi_clk_periph_ths              },
	{"i2s0",           0,                    audio_parents,          ARRAY_SIZE(audio_parents),          &sunxi_clk_periph_i2s0             },
	{"i2s1",           0,                    audio_parents,          ARRAY_SIZE(audio_parents),          &sunxi_clk_periph_i2s1             },
	{"codec_1x",       0,                    audio_parents,          ARRAY_SIZE(audio_parents),          &sunxi_clk_periph_codec_1x         },
	{"codec_4x",       0,                    audio_parents,          ARRAY_SIZE(audio_parents),          &sunxi_clk_periph_codec_4x         },
	{"usbphy0",        0,                    hosc_parents,           ARRAY_SIZE(hosc_parents),           &sunxi_clk_periph_usbphy0          },
	{"usbohci0",       0,                    ahb3mod_parents,        ARRAY_SIZE(ahb3mod_parents),        &sunxi_clk_periph_usbohci0         },
	{"usbohci0_12m",   0,                    usbohci_12m_parents,    ARRAY_SIZE(usbohci_12m_parents),    &sunxi_clk_periph_usbohci0_12m     },
	{"usbehci0",       0,                    ahb3mod_parents,        ARRAY_SIZE(ahb3mod_parents),        &sunxi_clk_periph_usbehci0         },
	{"usbotg",         0,                    ahb3mod_parents,        ARRAY_SIZE(ahb3mod_parents),        &sunxi_clk_periph_usbotg           },
	{"mipi_dphy0",     0,                    mipi_dphy_parents,      ARRAY_SIZE(mipi_dphy_parents),      &sunxi_clk_periph_mipi_dphy0       },
	{"mipi_host0",     0,                    mipi_host_parents,      ARRAY_SIZE(mipi_host_parents),      &sunxi_clk_periph_mipi_host0       },
	{"display_top",    0,                    ahb3mod_parents,        ARRAY_SIZE(ahb3mod_parents),        &sunxi_clk_periph_display_top      },
	{"tcon_lcd",       0,                    tcon_lcd_parents,       ARRAY_SIZE(tcon_lcd_parents),       &sunxi_clk_periph_tcon_lcd         },
	{"csi_top",        0,                    csi_top_parents,        ARRAY_SIZE(csi_top_parents),        &sunxi_clk_periph_csi_top          },
	{"csi_master0",    0,                    csi_master_parents,     ARRAY_SIZE(csi_master_parents),     &sunxi_clk_periph_csi_master0      },
	{"csi_master1",    0,                    csi_master_parents,     ARRAY_SIZE(csi_master_parents),     &sunxi_clk_periph_csi_master1      },
	{"isp",            0,                    isp_parents,            ARRAY_SIZE(isp_parents),            &sunxi_clk_periph_isp              },
	{"dspo",           0,                    dspo_parents,           ARRAY_SIZE(dspo_parents),           &sunxi_clk_periph_dspo             },
	{"pio",            0,                    apb1mod_parents,        ARRAY_SIZE(apb1mod_parents),        &sunxi_clk_periph_pio              },
};

struct periph_init_data sunxi_periphs_cpus_init[] = {
	{"cpurcpus_pll",    CLK_GET_RATE_NOCACHE|CLK_READONLY,  cpurcpus_pll_parents,   ARRAY_SIZE(cpurcpus_pll_parents),   &sunxi_clk_periph_cpurcpus_pll  },
	{"cpurcpus",        CLK_GET_RATE_NOCACHE|CLK_READONLY,  cpurcpus_parents,       ARRAY_SIZE(cpurcpus_parents),       &sunxi_clk_periph_cpurcpus      },
	{"cpurahbs",        CLK_GET_RATE_NOCACHE|CLK_READONLY,  cpurahbs_parents,       ARRAY_SIZE(cpurahbs_parents),       &sunxi_clk_periph_cpurahbs      },
	{"cpurapbs1",       CLK_GET_RATE_NOCACHE|CLK_READONLY,  cpurapbs1_parents,      ARRAY_SIZE(cpurapbs1_parents),      &sunxi_clk_periph_cpurapbs1     },
	{"cpurapbs2_pll",   CLK_GET_RATE_NOCACHE|CLK_READONLY,  cpurapbs2_pll_parents,  ARRAY_SIZE(cpurapbs2_pll_parents),  &sunxi_clk_periph_cpurapbs2_pll },
	{"cpurapbs2",       CLK_GET_RATE_NOCACHE|CLK_READONLY,  cpurapbs2_parents,      ARRAY_SIZE(cpurapbs2_parents),      &sunxi_clk_periph_cpurapbs2     },
	{"cpurowc",         CLK_GET_RATE_NOCACHE,               cpurdev_parents,        ARRAY_SIZE(cpurdev_parents),        &sunxi_clk_periph_cpurowc       },
	{"losc_out",        0,                                  losc_parents,           ARRAY_SIZE(losc_parents),           &sunxi_clk_periph_losc_out      },
	{"cpurpio",         CLK_GET_RATE_NOCACHE|CLK_READONLY,  cpurpio_parents,        ARRAY_SIZE(cpurpio_parents),        &sunxi_clk_periph_cpurpio       },
	{"spwm",            0,                                  spwm_parents,           ARRAY_SIZE(spwm_parents),           &sunxi_clk_periph_spwm          },
	{"stwi",            0,                                  stwi_parents,           ARRAY_SIZE(stwi_parents),           &sunxi_clk_periph_stwi          },
	{"dcxo_out",        0,                                  dcxo_parents,           ARRAY_SIZE(dcxo_parents),           &sunxi_clk_periph_dcxo_out      },
};

/* dcxo private operations
 * dcxo control register(0x07000160),
 * if bit_31 == 0, dcxo 24M output for peripheral is enabled
 * if bit_31 == 1, dcxo 24M output for peripheral is disabled
 * we could enable dcxo 24M output for some peripheral devices such as
 * wifi modoule
 */
int dcxo_out_priv_enable(struct clk_hw *hw)
{
	unsigned long reg;

	if (sunxi_clk_periph_dcxo_out.gate.enable) {
		reg = readl(sunxi_clk_periph_dcxo_out.gate.enable);
		reg = SET_BITS(31, 1, reg, 0);
		writel(reg, sunxi_clk_periph_dcxo_out.gate.enable);
	}

	return 0;
}

void dcxo_out_priv_disable(struct clk_hw *hw)
{
	unsigned long reg;

	if (sunxi_clk_periph_dcxo_out.gate.enable) {
		reg = readl(sunxi_clk_periph_dcxo_out.gate.enable);
		reg = SET_BITS(31, 1, reg, 1);
		writel(reg, sunxi_clk_periph_dcxo_out.gate.enable);
	}
}

int dcxo_out_priv_is_enabled(struct clk_hw *hw)
{
	unsigned long reg;

	if (sunxi_clk_periph_dcxo_out.gate.enable) {
		reg = readl(sunxi_clk_periph_dcxo_out.gate.enable);
		if (GET_BITS(sunxi_clk_periph_dcxo_out.gate.enb_shift, 1, reg))
			return 0;
		else
			return 1;
	}

	return 0;
}

void set_dcxo_out_priv_ops(struct clk_ops *priv_ops)
{
	priv_ops->enable = dcxo_out_priv_enable;
	priv_ops->disable = dcxo_out_priv_disable;
	priv_ops->is_enabled = dcxo_out_priv_is_enabled;
}

struct clk_ops dcxo_out_priv_ops;
void sunxi_set_clk_priv_ops(char *clk_name, struct clk_ops  *clk_priv_ops,
	void (*set_priv_ops)(struct clk_ops *priv_ops))
{
	int i = 0;
	sunxi_clk_get_periph_ops(clk_priv_ops);
	set_priv_ops(clk_priv_ops);
	for (i = 0; i < (ARRAY_SIZE(sunxi_periphs_cpus_init)); i++) {
		if (!strcmp(sunxi_periphs_cpus_init[i].name, clk_name))
			sunxi_periphs_cpus_init[i].periph->priv_clkops =
								clk_priv_ops;
	}
}
/*
 * sunxi_clk_get_factor_by_name() - Get factor clk init config
 */
struct factor_init_data *sunxi_clk_get_factor_by_name(const char *name)
{
	struct factor_init_data *factor;
	int i;

	/* get pll clk init config */
	for (i = 0; i < ARRAY_SIZE(sunxi_factos); i++) {
		factor = &sunxi_factos[i];
		if (strcmp(name, factor->name))
			continue;
		return factor;
	}

	return NULL;
}

struct periph_init_data *sunxi_clk_get_periph_rtc_by_name(const char *name)
{
	return NULL;
}
/*
 * sunxi_clk_get_periph_by_name() - Get periph clk init config
 */
struct periph_init_data *sunxi_clk_get_periph_by_name(const char *name)
{
	struct periph_init_data *perpih;
	int i;

	for (i = 0; i < ARRAY_SIZE(sunxi_periphs_init); i++) {
		perpih = &sunxi_periphs_init[i];
		if (strcmp(name, perpih->name))
			continue;
		return perpih;
	}

	return NULL;
}

/*
 * sunxi_clk_get_periph_cpus_by_name() - Get periph clk init config
 */
struct periph_init_data *sunxi_clk_get_periph_cpus_by_name(const char *name)
{
	struct periph_init_data *perpih;
	int i;

	for (i = 0; i < ARRAY_SIZE(sunxi_periphs_cpus_init); i++) {
		perpih = &sunxi_periphs_cpus_init[i];
		if (strcmp(name, perpih->name))
			continue;
		return perpih;
	}

	return NULL;
}
struct periph_init_data *sunxi_cpus_clk_get_periph_by_name(const char *name)
{
	struct periph_init_data *perpih;
	int i;

	for (i = 0; i < ARRAY_SIZE(sunxi_periphs_cpus_init); i++) {
		perpih = &sunxi_periphs_cpus_init[i];
		if (strcmp(name, perpih->name))
			continue;
		return perpih;
	}

	return NULL;
}

#ifdef CONFIG_PM_SLEEP
static int sunxi_clk_suspend(void)
{
	struct sunxi_factor_clk_reg_cache *factor_clk_reg;
	struct sunxi_periph_clk_reg_cache *periph_clk_reg;

	list_for_each_entry(factor_clk_reg, &clk_factor_reg_cache_list, node)
		sunxi_factor_clk_save(factor_clk_reg);

	list_for_each_entry(periph_clk_reg, &clk_periph_reg_cache_list, node)
		sunxi_periph_clk_save(periph_clk_reg);

	return 0;
}
static void sunxi_clk_resume(void)
{
	struct sunxi_factor_clk_reg_cache *factor_clk_reg;
	struct sunxi_periph_clk_reg_cache *periph_clk_reg;

	list_for_each_entry(factor_clk_reg, &clk_factor_reg_cache_list, node)
		sunxi_factor_clk_restore(factor_clk_reg);

	list_for_each_entry(periph_clk_reg, &clk_periph_reg_cache_list, node)
		sunxi_periph_clk_restore(periph_clk_reg);
}

static struct syscore_ops sunxi_clk_syscore_ops = {
	.suspend = sunxi_clk_suspend,
	.resume = sunxi_clk_resume,
};
#endif

void __init sunxi_cpu_clocks_init(struct device_node *node)
{
	struct clk *clk;
	const char *clk_name = node->name;
	struct periph_init_data *periph;

	if (of_property_read_string(node, "clock-output-names", &clk_name)) {
		pr_err("%s:get clock-output-names failed in %s node\n",
						__func__, node->full_name);
		return;
	}

	periph = sunxi_clk_get_periph_by_name(clk_name);
	if (!periph) {
		pr_err("clk %s not found in %s\n", clk_name, __func__);
		return;
	}

	clk = sunxi_clk_register_cpu(periph, sunxi_clk_base, cpu_parents[4],
					cpu_parents[3]);
	if (!IS_ERR(clk)) {
		clk_register_clkdev(clk, clk_name, NULL);
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
	}
}

void __init sunxi_clocks_init(struct device_node *node)
{
	int err;
	sunxi_clk_base = of_iomap(node, 0);
	sunxi_clk_cpus_base = of_iomap(node, 1);
	sunxi_clk_periph_losc_out.gate.bus = of_iomap(node, 2) + LOSC_OUT_GATE;
	sunxi_clk_periph_dcxo_out.gate.enable = of_iomap(node, 2) +
								DCXO_OUT_CFG;
	sunxi_set_clk_priv_ops("dcxo_out", &dcxo_out_priv_ops,
						set_dcxo_out_priv_ops);
	/*do some initialize arguments here*/
	sunxi_clk_factor_initlimits();
#ifdef CONFIG_PM_SLEEP
	register_syscore_ops(&sunxi_clk_syscore_ops);
#endif
	err = sunxi_parse_sdm_info(node);
	if (err) {
		pr_info("sunxi_parse_sdm_info failed: %d\n", err);
		return;
	}
}

MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0.1");
MODULE_AUTHOR("rgm <rengaomin@allwinnertech.com>");
