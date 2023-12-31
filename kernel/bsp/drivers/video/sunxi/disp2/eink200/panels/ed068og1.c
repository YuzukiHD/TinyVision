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
/*
 * sysconfig.fex
[eink]
eink_panel_name     = "default_eink"
eink_scan_mode	    = 0
eink_width          = 1440
eink_height         = 1080
eink_bits	    = 4
eink_data_len	    = 16
eink_lsl	    = 8
eink_lbl	    = 8
eink_lel	    = 84
eink_fsl	    = 2
eink_fbl	    = 4
eink_fel	    = 5
eink_lgonl	    = 180
eink_gdck_sta	    = 8
eink_gdoe_start_line= 2
eink_fresh_hz       = 85
eink_gray_level	    = 32
eink_wav_path	    = "/system/default_4_16.awf"
;eink_power	    = "vcc-lcd"
;eink_pin_power	    = "vcc-pd"

eink_gpio_0         = port:PD00<1><0><default><1>
eink_gpio_1         = port:PD01<1><0><default><1>
eink_gpio_2         = port:PD02<1><0><default><1>
eink_gpio_3         = port:PD03<1><0><default><1>
eink_gpio_4         = port:PD04<1><0><default><1>
eink_gpio_5         = port:PD05<1><0><default><1>

;eink_pin0	    = port:PD00<5><0><default><default>
;eink_pin1           = port:PD01<5><0><default><default>
;eink_pin2           = port:PD02<5><0><default><default>
;eink_pin3           = port:PD03<5><0><default><default>
;eink_pin4           = port:PD04<5><0><default><default>
;eink_pin5           = port:PD05<5><0><default><default>
eink_pin6           = port:PD06<5><0><default><default>
eink_pin7           = port:PD07<5><0><default><default>
eink_pin8           = port:PD08<5><0><default><default>
eink_pin9           = port:PD09<5><0><default><default>
eink_pin10          = port:PD10<5><0><default><default>
eink_pin11          = port:PD11<5><0><default><default>
eink_pin12          = port:PD12<5><0><default><default>
eink_pin13          = port:PD13<5><0><default><default>
eink_pin14          = port:PD14<5><0><default><default>
eink_pin15          = port:PD15<5><0><default><default>
eink_pinoeh         = port:PD16<5><0><default><default>
eink_pinleh         = port:PD17<5><0><default><default>
eink_pinckh         = port:PD18<5><0><default><default>
eink_pinsth         = port:PD19<5><0><default><default>
eink_pinckv         = port:PD20<5><0><default><default>
eink_pinmod         = port:PD21<5><0><default><default>
eink_pinstv         = port:PD22<5><0><default><default>

[eink_suspend]
;eink_pin0           = port:PD00<7><0><default><default>
;eink_pin1           = port:PD01<7><0><default><default>
;eink_pin2           = port:PD02<7><0><default><default>
;eink_pin3           = port:PD03<7><0><default><default>
;eink_pin4           = port:PD04<7><0><default><default>
;eink_pin5           = port:PD05<7><0><default><default>
eink_pin6           = port:PD06<7><0><default><default>
eink_pin7           = port:PD07<7><0><default><default>
eink_pin8           = port:PD08<7><0><default><default>
eink_pin9           = port:PD09<7><0><default><default>
eink_pin10          = port:PD10<7><0><default><default>
eink_pin11          = port:PD11<7><0><default><default>
eink_pin12          = port:PD12<7><0><default><default>
eink_pin13          = port:PD13<7><0><default><default>
eink_pin14          = port:PD14<7><0><default><default>
eink_pin15          = port:PD15<7><0><default><default>
eink_pinoeh         = port:PD16<7><0><default><default>
eink_pinleh         = port:PD17<7><0><default><default>
eink_pinckh         = port:PD18<7><0><default><default>
eink_pinsth         = port:PD19<7><0><default><default>
eink_pinckv         = port:PD20<7><0><default><default>
eink_pinmod         = port:PD21<7><0><default><default>
eink_pinstv         = port:PD22<7><0><default><default>
 *
 */

#include "default_eink.h"

static void EINK_power_on(void);
static void EINK_power_off(void);

s32 EINK_open_flow(void)
{
	EINK_INFO_MSG("\n");
	EINK_OPEN_FUNC(EINK_power_on, 2);
	return 0;
}

s32 EINK_close_flow(void)
{
	EINK_INFO_MSG("\n");
	EINK_CLOSE_FUNC(EINK_power_off, 2);
	return 0;
}

static void EINK_power_on(void)
{
	EINK_INFO_MSG("EINK_power_on!\n");
	/* pwr3 pb2 */
	panel_gpio_set_value(0, 1);
	mdelay(1);
	/* pwr0 pb0 */
	panel_gpio_set_value(1, 1);
	mdelay(1);
	/* pb1 */
	panel_gpio_set_value(2, 1);
	mdelay(1);
	/* pwr2 pd6 */
	panel_gpio_set_value(3, 1);
	mdelay(1);
	/* pwr com pd7 */
	panel_gpio_set_value(4, 1);
	mdelay(1);

	panel_gpio_set_value(5, 1);
	mdelay(2);

	panel_pin_cfg(1);
}

static void EINK_power_off(void)
{
	EINK_INFO_MSG("EINK_power_off!\n");
	panel_pin_cfg(0);

#if 1
	panel_gpio_set_value(5, 0);
	mdelay(1);
#endif
	panel_gpio_set_value(4, 0);
	mdelay(1);

	panel_gpio_set_value(3, 0);
	mdelay(1);

	panel_gpio_set_value(2, 0);
	mdelay(1);

	panel_gpio_set_value(1, 0);
	mdelay(1);

	panel_gpio_set_value(0, 0);
	mdelay(2);
}

struct __eink_panel default_eink = {
	/* panel driver name, must mach the eink_panel_name in sys_config.fex */
	.name = "default_eink",
	.func = {
		 .cfg_open_flow = EINK_open_flow,
		 .cfg_close_flow = EINK_close_flow,
		}
	,
};
