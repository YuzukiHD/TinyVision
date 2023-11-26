/* SPDX-License-Identifier: (GPL-2.0+ or MIT) */
/*
 * Copyright (C) 2020 huangzhenwei@allwinnertech.com
 * Copyright (C) 2023 Andras Szemzo <szemzo.andras@gmail.com.com>
 */

#ifndef _DT_BINDINGS_CLK_SUN8I_V85X_CCU_H_
#define _DT_BINDINGS_CLK_SUN8I_V85X_CCU_H_

#define CLK_PLL_CPU		0
#define CLK_PLL_DDR		1
#define CLK_PLL_PERIPH_4X	2
#define CLK_PLL_PERIPH_2X	3
#define CLK_PLL_PERIPH_800M	4
#define CLK_PLL_PERIPH_480M	5
#define CLK_PLL_PERIPH_600M	6
#define CLK_PLL_PERIPH_400M	7
#define CLK_PLL_PERIPH_300M	8
#define CLK_PLL_PERIPH_200M	9
#define CLK_PLL_PERIPH_160M	10
#define CLK_PLL_PERIPH_150M	11
#define CLK_PLL_PERIPH		12
#define CLK_PLL_VIDEO_4X	13
#define CLK_PLL_CSI_4X		14

#define CLK_PLL_AUDIO		15
#define CLK_PLL_AUDIO_DIV2	16
#define CLK_PLL_AUDIO_DIV5	17
#define CLK_PLL_AUDIO_4X	18
#define CLK_PLL_AUDIO_1X	19


#define CLK_PLL_NPU_4X		20
#define CLK_CPU			21
#define CLK_CPU_AXI		22
#define CLK_CPU_APB		23
#define CLK_AHB			24
#define CLK_APB0		25
#define CLK_APB1		26
#define CLK_MBUS		27
#define CLK_DE			28
#define CLK_BUS_DE		29
#define CLK_G2D			30
#define CLK_BUS_G2D		31
#define CLK_CE			32
#define CLK_BUS_CE		33
#define CLK_VE			34
#define CLK_BUS_VE		35
#define CLK_NPU			36
#define CLK_BUS_NPU		37
#define CLK_BUS_DMA		38
#define CLK_BUS_MSGBOX0		39
#define CLK_BUS_MSGBOX1		40
#define CLK_BUS_SPINLOCK	41
#define CLK_BUS_HSTIMER		42
#define CLK_AVS			43
#define CLK_BUS_DBG		44
#define CLK_BUS_PWM		45
#define CLK_BUS_IOMMU		46
#define CLK_DRAM		47
#define CLK_MBUS_DMA		48
#define CLK_MBUS_VE		49
#define CLK_MBUS_CE		50
#define CLK_MBUS_CSI		51
#define CLK_MBUS_ISP		52
#define CLK_MBUS_G2D		53
#define CLK_BUS_DRAM		54
#define CLK_MMC0		55
#define CLK_MMC1		56
#define CLK_MMC2		57
#define CLK_BUS_MMC0		58
#define CLK_BUS_MMC1		59
#define CLK_BUS_MMC2		60
#define CLK_BUS_UART0		61
#define CLK_BUS_UART1		62
#define CLK_BUS_UART2		63
#define CLK_BUS_UART3		64
#define CLK_BUS_I2C0		65
#define CLK_BUS_I2C1		66
#define CLK_BUS_I2C2		67
#define CLK_BUS_I2C3		68
#define CLK_BUS_I2C4		69
#define CLK_SPI0		70
#define CLK_SPI1		71
#define CLK_SPI2		72
#define CLK_SPI3		73
#define CLK_BUS_SPI0		74
#define CLK_BUS_SPI1		75
#define CLK_BUS_SPI2		76
#define CLK_BUS_SPI3		77
#define CLK_EMAC_25M		78
#define CLK_BUS_EMAC		79
#define CLK_BUS_GPADC		80
#define CLK_BUS_THS		81
#define CLK_I2S			82
#define CLK_BUS_I2S		83
#define CLK_DMIC		84
#define CLK_BUS_DMIC		85
#define CLK_AUDIO_DAC		86
#define CLK_AUDIO_ADC		87
#define CLK_BUS_AUDIO		88
#define CLK_USB_OHCI		89
#define CLK_BUS_OHCI		90
#define CLK_BUS_EHCI		91
#define CLK_BUS_OTG		92
#define CLK_MIPI_DSI		93
#define CLK_BUS_MIPI_DSI	94
#define CLK_TCON_LCD		95
#define CLK_BUS_TCON_LCD	96
#define CLK_CSI_TOP		97
#define CLK_CSI_MCLK0		98
#define CLK_CSI_MCLK1		99
#define CLK_CSI_MCLK2		100
#define CLK_BUS_CSI		101
#define CLK_BUS_WIEGAND		102
#define CLK_RISCV		103
#define CLK_RISCV_AXI		104
#define CLK_BUS_RISCV		105
#define CLK_FANOUT_24M		106
#define CLK_FANOUT_12M		107
#define CLK_FANOUT_16M		108
#define CLK_FANOUT_25M		109
#define CLK_FANOUT_27M		110
#define CLK_FANOUT_PCLK		111
#define CLK_FANOUT0		112
#define CLK_FANOUT1		113
#define CLK_FANOUT2		114

#endif /* _DT_BINDINGS_CLK_SUN8I_V85X_CCU_H_ */
