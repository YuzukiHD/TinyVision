/*
 * drivers/usb/sunxi_usb/usbc/usbc_i.h
 * (C) Copyright 2010-2015
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * daniel, 2009.09.15
 *
 * usb common ops.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#ifndef __USBC_I_H__
#define __USBC_I_H__

#include "../include/sunxi_usb_config.h"

#define  USBC_MAX_OPEN_NUM    1

void __iomem *get_otgc_vbase(void);

/* record USB common info */
typedef struct __fifo_info {
	void __iomem *port0_fifo_addr;
	__u32 port0_fifo_size;

	void __iomem *port1_fifo_addr;
	__u32 port1_fifo_size;

	void __iomem *port2_fifo_addr;
	__u32 port2_fifo_size;
} __fifo_info_t;

/* record current USB port's all hardware info */
typedef struct __usbc_otg {
	__u32 port_num;
	void __iomem *base_addr;	/* usb base address */

	__u32 used;			/* is used or not */
	__u32 no;			/* index in manager table */
} __usbc_otg_t;


/* PHYS EFUSE offest */
#define EFUSE_OFFSET					0x28		//esuse offset
#define SUNXI_USB_PHY_EFUSE_ADJUST		0x10		//bit4
#define SUNXI_USB_PHY_EFUSE_MODE		0x20000		//bit17 unvalid, don't distinguish
#define SUNXI_USB_PHY_EFUSE_RES			0x1E0		//bit8-5
#define SUNXI_USB_PHY_EFUSE_COM			0x1E00		//bit12-9
#define SUNXI_USB_PHY_EFUSE_USB0TX		0x1E00		//bit12-9
#define SUNXI_USB_PHY_EFUSE_USB1TX		0xE000000	//bit25-27 unvalid, don't have USB1

/* PHY RANGE: bit field */
#define PHY_RANGE_TRAN_MASK			0x3C0		//bit9:6, trancevie_data
#define PHY_RANGE_PREE_MASK			0x30		//bit5:4, preemphasis_data
#define PHY_RANGE_RESI_MASK			0xF		//bit3:0, resistance_data

#endif /* __USBC_I_H__ */

