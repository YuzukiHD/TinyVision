// SPDX-License-Identifier: (GPL-2.0+ or MIT)
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * Copyright (C) 2023 rengaomin@allwinnertech.com
 */

#ifndef _DT_BINDINGS_CLK_SUN8IW21_H_
#define _DT_BINDINGS_CLK_SUN8IW21_H_

#define CLK_PLL_CPU		0
#define CLK_PLL_DDR		1
#define CLK_PLL_PERI_PARENT	2
#define CLK_PLL_PERI_2X		3
#define CLK_PLL_PERI_DIV3	4
#define CLK_PLL_PERI_800M	5
#define CLK_PLL_PERI_480M	6
#define CLK_PLL_PERI_600M	7
#define CLK_PLL_PERI_400M	8
#define CLK_PLL_PERI_300M	9
#define CLK_PLL_PERI_200M	10
#define CLK_PLL_PERI_160M	11
#define CLK_PLL_PERI_150M	12
#define CLK_PLL_VIDEO_4X	13
#define CLK_PLL_VIDEO_2X	14
#define CLK_PLL_VIDEO_1X	15
#define CLK_PLL_CSI_4X		16
#define CLK_PLL_AUDIO_DIV2	17
#define CLK_PLL_NPU_4X		18
#define CLK_CPU			19
#define CLK_CPU_AXI		20
#define CLK_CPU_APB		21
#define CLK_BUS_CPU		22
#define CLK_AHB			23
#define CLK_APB0		24
#define CLK_APB1		25
#define CLK_DE			26
#define CLK_BUS_DE		27
#define CLK_G2D			28
#define CLK_BUS_G2D		29
#define CLK_CE			30
#define CLK_CE_SYS		31
#define CLK_BUS_CE		32
#define CLK_VE			33
#define CLK_BUS_VE		34
#define CLK_NPU			35
#define CLK_BUS_NPU		36
#define CLK_DMA			37
#define CLK_MSGBOX1		38
#define CLK_MSGBOX0		39
#define CLK_SPINLOCK		40
#define CLK_HSTIMER		41
#define CLK_AVS			42
#define CLK_DBGSYS		43
#define CLK_PWM			44
#define CLK_IOMMU		45
#define CLK_DRAM		46
#define CLK_MBUS_NPU_GATE	47
#define CLK_MBUS_VID_IN_GATE	48
#define CLK_MBUS_VID_OUT_GATE	49
#define CLK_MBUS_G2D		50
#define CLK_MBUS_ISP		51
#define CLK_MBUS_CSI		52
#define CLK_MBUS_CE_GATE	53
#define CLK_MBUS_VE		54
#define CLK_MBUS_DMA		55
#define CLK_BUS_DRAM		56
#define CLK_SMHC0		57
#define CLK_SMHC1		58
#define CLK_SMHC2		59
#define CLK_BUS_SMHC2		60
#define CLK_BUS_SMHC1		61
#define CLK_BUS_SMHC0		62
#define CLK_UART3		63
#define CLK_UART2		64
#define CLK_UART1		65
#define CLK_UART0		66
#define CLK_TWI4		67
#define CLK_TWI3		68
#define CLK_TWI2		69
#define CLK_TWI1		70
#define CLK_TWI0		71
#define CLK_SPI0		72
#define CLK_SPI1		73
#define CLK_SPI2		74
#define CLK_SPI3		75
#define CLK_SPIF		76
#define CLK_BUS_SPIF		77
#define CLK_BUS_SPI3		78
#define CLK_BUS_SPI2		79
#define CLK_BUS_SPI1		80
#define CLK_BUS_SPI0		81
#define CLK_GMAC_25M		82
#define CLK_GMAC_25M_CLK_SRC	83
#define CLK_GMAC		84
#define CLK_GPADC		85
#define CLK_THS			86
#define CLK_I2S0		87
#define CLK_I2S1		88
#define CLK_BUS_I2S1		89
#define CLK_BUS_I2S0		90
#define CLK_DMIC		91
#define CLK_BUS_DMIC		92
#define CLK_AUDIO_CODEC_DAC	93
#define CLK_AUDIO_CODEC_ADC	94
#define CLK_AUDIO_CODEC		95
#define CLK_USB			96
#define CLK_USBOTG0		97
#define CLK_USBEHCI0		98
#define CLK_USBOHCI0		99
#define CLK_DPSS_TOP		100
#define CLK_DSI			101
#define CLK_BUS_DSI		102
#define CLK_TCONLCD		103
#define CLK_BUS_TCONLCD		104
#define CLK_CSI			105
#define CLK_CSI_MASTER0		106
#define CLK_CSI_MASTER1		107
#define CLK_CSI_MASTER2		108
#define CLK_BUS_CSI		109
#define CLK_WIEGAND		110
#define CLK_E907_GATING_RS	111
#define CLK_E907		112
#define CLK_RISCV_CFG		113
#define CLK_CPUS_HCLK_GATE	114
#define CLK_VID_OUT_AHB_GATE	115
#define CLK_VID_IN_AHB_GATE	116
#define CLK_RES_DCAP_24M	117
#define CLK_GPADC_24M		118
#define CLK_WIEGAND_24M		119
#define CLK_USB_24M		120
#define CLK_GPADC_SEL		121

#define CLK_MAX_NO		(CLK_GPADC_SEL + 1)

#endif /* _DT_BINDINGS_CLK_SUN8IW21_H_ */
