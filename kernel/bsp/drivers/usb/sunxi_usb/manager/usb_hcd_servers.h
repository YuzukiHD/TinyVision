/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * (C) Copyright 2010-2015
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * javen, 2011-4-14, create this file
 *
 * usb host contoller driver. service function set.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#ifndef __USB_HCD_SERVERS_H__
#define __USB_HCD_SERVERS_H__

extern int sunxi_usb_disable_ehci(__u32 usbc_no);
extern int sunxi_usb_enable_ehci(__u32 usbc_no);
extern int sunxi_usb_disable_ohci(__u32 usbc_no);
extern int sunxi_usb_enable_ohci(__u32 usbc_no);

int sunxi_usb_disable_hcd(__u32 usbc_no);
int sunxi_usb_enable_hcd(__u32 usbc_no);

#endif /* __USB_HCD_SERVERS_H__ */
