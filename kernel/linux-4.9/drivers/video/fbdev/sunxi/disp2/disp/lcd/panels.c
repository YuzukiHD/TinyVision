/*
 * Allwinner SoCs display driver.
 *
 * Copyright (C) 2016 Allwinner.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include "panels.h"

struct sunxi_lcd_drv g_lcd_drv;

struct __lcd_panel *panel_array[] = {
#ifdef CONFIG_EINK_PANEL_USED
	&default_eink,
#endif
	&default_panel,
#ifdef CONFIG_LCD_SUPPORT_LT070ME05000
	&lt070me05000_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_WTQ05027D01
	&wtq05027d01_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_T27P06
	&t27p06_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_DX0960BE40A1
	&dx0960be40a1_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_TFT720X1280
	&tft720x1280_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_S6D7AA0X01
	&S6D7AA0X01_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_GG1P4062UTSW
	&gg1p4062utsw_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_LS029B3SX02
	&ls029b3sx02_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_HE0801A068
	&he0801a068_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_INET_DSI_PANEL
	&inet_dsi_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_LQ101R1SX03
	&lq101r1sx03_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_TM_DSI_PANEL
	&tm_dsi_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_ILI9881C
	&ili9881c_dsi_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_FD055HD003S
	&fd055hd003s_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_FRD450H40014
	&frd450h40014_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_H245QBN02
	&h245qbn02_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_ILI9341
	&ili9341_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_LH219WQ1
	&lh219wq1_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_ST7789V
	&st7789v_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_ST7796S
	&st7796s_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_ST7701S
	&st7701s_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_ST7701S_G5
	&st7701s_g5_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_T30P106
	&t30p106_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_TO20T20000
	&to20t20000_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_S2003T46G
	&s2003t46g_panel,
#endif

#ifdef CONFIG_LCD_SUPPORT_WILLIAMLCD
	&WilliamLcd_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_WTL096601G03
	&wtl096601g03_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_RT13QV005D
	&rt13qv005d_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_ST7789V_CPU
	&st7789v_cpu_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_KD101NA5
	&kd101na5_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_KD070D57
	&kd070d57_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_ZS080NI4003E7H3H_A
	&zs080ni4003e7h3h_a_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_TV096WXM_NH0
	&TV096WXM_NH0_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_BP101WX1_206
	&bp101wx1_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_K101IM2QA04
	&k101im2qa04_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_T050K589
	&t050k589_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_KD080D24
	&kd080d24_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_JD9161Z_MIPI
	&jd9161z_panel,
#endif
#ifdef CONFIG_LCD_SUPPORT_ICN6202
       &icn6202_panel,
#endif

#ifdef CONFIG_LCD_SUPPORT_ICN6202_2LANE
       &icn6202_2lane_panel,
#endif

#ifdef CONFIG_LCD_SUPPORT_NT35510_MIPI
	&nt35510_panel,
#endif

	&super_lcd_panel,
	NULL,
};

void lcd_set_panel_funs(void)
{
	int i;

	for (i = 0; panel_array[i] != NULL; i++) {
		sunxi_lcd_set_panel_funs(panel_array[i]->name,
					 &panel_array[i]->func);
	}
}

int lcd_init(void)
{
	sunxi_disp_get_source_ops(&g_lcd_drv.src_ops);

	return 0;
}

