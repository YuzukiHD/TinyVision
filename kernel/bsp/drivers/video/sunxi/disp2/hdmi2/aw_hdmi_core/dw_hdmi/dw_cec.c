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
#include <linux/delay.h>
#include "dw_access.h"
#include "dw_mc.h"
#include "dw_cec.h"

extern u32 bHdmi_drv_enable;

/**
 * Read locked status
 * @param dev: address and device information
 * @return LOCKED status
 */
static int _dw_cec_get_lock(dw_hdmi_dev_t *dev)
{
	return (dev_read(dev, CEC_LOCK) & CEC_LOCK_LOCKED_BUFFER_MASK) != 0;
}

/**
 * Enable interrupts
 * @param dev:  address and device information
 * @param mask: interrupt mask to enable
 * @return error code
 */
static void _dw_cec_interrupt_enable(dw_hdmi_dev_t *dev, unsigned char mask)
{
	dev_write(dev, CEC_MASK, dev_read(dev, CEC_MASK) & ~mask);
}

/**
 * Disable interrupts
 * @param dev:  address and device information
 * @param mask: interrupt mask to disable
 * @return error code
 */
static void _dw_cec_interrupt_disable(dw_hdmi_dev_t *dev, unsigned char mask)
{
	dev_write(dev, CEC_MASK, dev_read(dev, CEC_MASK) | mask);
}

/**
 * Clear interrupts
 * @param dev:  address and device information
 * @param mask: interrupt mask to clear
 * @return error code
 */
static void _dw_cec_interrupt_clear(dw_hdmi_dev_t *dev, unsigned char mask)
{
	dev_write(dev, CEC_MASK, mask);
}

/**
 * Read interrupts
 * @param dev:  address and device information
 * @param mask: interrupt mask to read
 * @return INT content
 */
static int _dw_cec_interrupt_state_read(dw_hdmi_dev_t *dev, unsigned char mask)
{
	return dev_read(dev, IH_CEC_STAT0) & mask;
}

static int _dw_cec_interrupt_state_clear(dw_hdmi_dev_t *dev, unsigned char mask)
{
	/* write 1 to clear */
	dev_write(dev, IH_CEC_STAT0, mask);
	return 0;
}

/**
 * Read send status
 * @param dev:  address and device information
 * @return SEND status
 */
static int _dw_cec_get_send_status(dw_hdmi_dev_t *dev)
{
	return (dev_read(dev, CEC_CTRL) & CEC_CTRL_SEND_MASK) != 0;
}

/**
 * Set send status
 * @param dev: address and device information
 * @return error code
 */
static int _dw_cec_set_send_status(dw_hdmi_dev_t *dev)
{
	dev_write(dev, CEC_CTRL, dev_read(dev, CEC_CTRL) | CEC_CTRL_SEND_MASK);
	return 0;
}

/**
 * Configure standby mode
 * @param dev:    address and device information
 * @param enable: if true enable standby mode
 * @return error code
 */
static int _dw_cec_set_standby_mode(dw_hdmi_dev_t *dev, int enable)
{
	if (enable)
		dev_write(dev, CEC_CTRL,
			(dev_read(dev, CEC_CTRL) | CEC_CTRL_STANDBY_MASK));
	else
		dev_write(dev, CEC_CTRL,
			(dev_read(dev, CEC_CTRL) & ~CEC_CTRL_STANDBY_MASK));
	return 0;
}

/**
 * Configure signal free time
 * @param dev:  address and device information
 * @param time: time between attempts [nr of frames]
 * @return error code
 */
static int _dw_cec_set_signal_free_time(dw_hdmi_dev_t *dev, int time)
{
	unsigned char data;

	data = dev_read(dev, CEC_CTRL) & (~(CEC_CTRL_FRAME_TYP_MASK));
	if (time == 3)
		;                                  /* 0 */
	else if (time == 5)
		data |= FRAME_TYP0;                /* 1 */
	else if (time == 7)
		data |= FRAME_TYP1 | FRAME_TYP0;   /* 2 */
	else {
		hdmi_err("%s: invalid param time: %d\n", __func__, time);
		return -1;
	}
	dev_write(dev, CEC_CTRL, data);
	return 0;
}

/**
 * Write transmission buffer
 * @param dev:  address and device information
 * @param buf:  data to transmit
 * @param size: data length [byte]
 * @return error code or bytes configured
 */
static int _dw_cec_config_tx_buffer(dw_hdmi_dev_t *dev, char *buf, unsigned size)
{
	unsigned i;

	if (buf == NULL) {
		hdmi_err("%s: param buf is null!!!\n", __func__);
		return -1;
	}

	if (size > CEC_TX_DATA_SIZE) {
		hdmi_err("%s: cec config tx buffer size invalid!!!\n", __func__);
		return -1;
	}

	dev_write(dev, CEC_TX_CNT, size);
	for (i = 0; i < size; i++)
		dev_write(dev, CEC_TX_DATA + (i * 4), *buf++);

	return size;
}

/**
 * Read reception buffer
 * @param dev:  address and device information
 * @param buf:  buffer to hold receive data
 * @param size: maximum data length [byte]
 * @return error code or received bytes
 */
static int _dw_cec_config_rx_buffer(dw_hdmi_dev_t *dev, char *buf, unsigned size)
{
	unsigned i;
	unsigned char cnt;

	cnt = dev_read(dev, CEC_RX_CNT);   /* mask 7-5? */
	cnt = (cnt > size) ? size : cnt;
	for (i = 0; i < size; i++)
		*buf++ = dev_read(dev, CEC_RX_DATA + (i * 4));

	if (cnt > CEC_RX_DATA_SIZE) {
		hdmi_err("%s: cec read wrong byte count: %d\n", __func__, cnt);
		return -1;
	}
	return cnt;
}

/**
 * Wait for message (quit after a given period)
 * @param dev:     address and device information
 * @param buf:     buffer to hold data
 * @param size:    maximum data length [byte]
 * @param timeout: time out period [ms]
 * @return error code or received bytes
 */
static int _dw_cec_message_rx(dw_hdmi_dev_t *dev,
		char *buf, unsigned size, int timeout)
{
	int count = -1;
	unsigned char mask = 0x0;

	if (_dw_cec_get_lock(dev) == 0) {
		return count;
	}

	count = _dw_cec_config_rx_buffer(dev, buf, size);

	mask  = CEC_MASK_EOM_MASK;
	mask |= CEC_MASK_ERROR_FLOW_MASK;
	_dw_cec_interrupt_state_clear(dev, mask);

	/* set locked status */
	dev_write(dev, CEC_LOCK,
		(dev_read(dev, CEC_LOCK) & (~CEC_LOCK_LOCKED_BUFFER_MASK)));
	return count;
}

/**
 * Send a message (retry in case of failure)
 * @param dev:   address and device information
 * @param buf:   data to send
 * @param size:  data length [byte]
 * @param retry: maximum number of retries
 * @return error code or transmitted bytes
 */
static int _dw_cec_message_tx(dw_hdmi_dev_t *dev, char *buf, unsigned size,
		 unsigned retry)
{
	int timeout = 10;
	u8 status = 0;
	int i;
	unsigned char mask = 0x0;

	if (size <= 0) {
		hdmi_err("%s: cec message tx size %d is unvalid!!!\n", __func__, size);
		return -1;
	}

	if (_dw_cec_config_tx_buffer(dev, buf, size) != (int)size) {
		hdmi_err("%s: cec config tx buffer size unmatch set size %d!!!\n", __func__, size);
		return -1;
	}

	for (i = 0; i < (int)retry; i++) {

		if (!bHdmi_drv_enable) {
			hdmi_err("%s: cec message tx failed when hdmi enable mask not set!\n", __func__);
			return -1;
		}

		if (_dw_cec_set_signal_free_time(dev, i ? 3 : 5)) {
			hdmi_err("%s: cec config signal free time failed!!!\n", __func__);
			break;
		}

		mask |= CEC_MASK_DONE_MASK;
		mask |= CEC_MASK_NACK_MASK;
		mask |= CEC_MASK_ARB_LOST_MASK;
		mask |= CEC_MASK_ERROR_INITIATOR_MASK;
		_dw_cec_interrupt_enable(dev, mask);

		_dw_cec_set_send_status(dev);

		mdelay(1);

		timeout = 10;
		while (_dw_cec_get_send_status(dev) != 0 && timeout) {
			mdelay(20);
			timeout--;
		}

		if (timeout == 0) {
			hdmi_err("%s: cec config error, can not send cec message\n", __func__);
			return -1;
		}

		mdelay(1);
		status = dev_read(dev, IH_CEC_STAT0);
		dev_write(dev, IH_CEC_STAT0, status);

		if (status  & CEC_MASK_DONE_MASK) {
			return 0;
		} else if (status & CEC_MASK_NACK_MASK) {
			CEC_INF("cec message send nack!\n");
			return 1;
		} else if (status & CEC_MASK_ARB_LOST_MASK) {
			CEC_INF("cec message send arb lost. re-try %d\n", i);
			continue;
		} else if (status & CEC_MASK_ERROR_INITIATOR_MASK) {
			CEC_INF("cec message send error initiator. re-try %d\n", i);
			continue;
		} else if (status & CEC_MASK_ERROR_FLOW_MASK) {
			CEC_INF("cec message send error flow. re-try %d\n", i);
			continue;
		}
	}

	return -1;
}

int dw_cec_send_poll_message(dw_hdmi_dev_t *dev, char src)
{
	char buf[1];
	int timeout = 40;
	unsigned char mask = 0x0;

	buf[0] = (src << 4) | src;

	if (_dw_cec_config_tx_buffer(dev, buf, 1) != 1) {
		hdmi_err("%s: cec send polling message failed!\n", __func__);
		goto exit_ng;
	}

	_dw_cec_set_signal_free_time(dev, 5);

	mask |= CEC_MASK_DONE_MASK;
	mask |= CEC_MASK_NACK_MASK;
	mask |= CEC_MASK_ARB_LOST_MASK;
	mask |= CEC_MASK_ERROR_INITIATOR_MASK;
	_dw_cec_interrupt_enable(dev, mask);

	_dw_cec_set_send_status(dev);

	udelay(100);
	while (timeout) {
		if ((!_dw_cec_get_send_status(dev)) &&
				_dw_cec_interrupt_state_read(dev, CEC_MASK_NACK_MASK)) {
			_dw_cec_interrupt_state_clear(dev, CEC_MASK_NACK_MASK);
			goto exit_ok;
		}
		msleep(10);
		timeout--;
	}
	hdmi_err("%s: cec send polling message timeout!!!\n", __func__);

exit_ng:
	return -1;
exit_ok:
	return 0;
}

int dw_cec_set_logic_addr(dw_hdmi_dev_t *dev, unsigned addr, int enable)
{
	unsigned int regs;

	if (addr > CEC_BROADCAST_ADDR) {
		hdmi_err("%s: set logic address %d invalid!!!\n", __func__, addr);
		return -1;
	}

	/* clear cec hw logic address */
	dev_write(dev, CEC_ADDR_L, 0x00);
	dev_write(dev, CEC_ADDR_H, 0x00);

	if (addr == CEC_BROADCAST_ADDR) {
		if (enable) {
			dev_write(dev, CEC_ADDR_H, 0x80);
			dev_write(dev, CEC_ADDR_L, 0x00);
			return 0;
		} else {
			hdmi_err("%s: cannot de-allocate broadcast logical address\n", __func__);
			return -1;
		}
	}

	regs = (addr > 7) ? CEC_ADDR_H : CEC_ADDR_L;
	addr = (addr > 7) ? (addr - 8) : addr;
	if (enable) {
		dev_write(dev, CEC_ADDR_H, 0x00);
		dev_write(dev, regs, dev_read(dev, regs) |  (1 << addr));
	} else
		dev_write(dev, regs, dev_read(dev, regs) & ~(1 << addr));
	return 0;
}

unsigned int dw_cec_get_logic_addr(dw_hdmi_dev_t *dev)
{
	unsigned int reg, addr = 16, i;

	reg = dev_read(dev, CEC_ADDR_H);
	reg = (reg << 8) | dev_read(dev, CEC_ADDR_L);

	for (i = 0; i < 16; i++) {
		if ((reg >> i) & 0x01)
			addr = i;
	}

	return addr;
}

int dw_cec_receive_frame(dw_hdmi_dev_t *dev, char *buf, unsigned size)
{
	if (dev == NULL) {
		hdmi_err("%s: param dev is null!!!\n", __func__);
		return -1;
	}

	if (buf == NULL) {
		hdmi_err("%s: param buf is null!!!\n", __func__);
		return -1;
	}

	if (size > CEC_TX_DATA_SIZE) {
		hdmi_err("%s: cec receive frame size (%d) too large!!!\n", __func__, size);
		return -1;
	}

	return _dw_cec_message_rx(dev, buf, size, 100);
}

int dw_cec_send_frame(dw_hdmi_dev_t *dev, char *buf, unsigned size)
{
	if (dev == NULL) {
		hdmi_err("%s: param dev is null!!!\n", __func__);
		return -1;
	}

	if (buf == NULL) {
		hdmi_err("%s: param buf is null!!!\n", __func__);
		return -1;
	}

	if (size >= CEC_TX_DATA_SIZE) {
		hdmi_err("%s: cec send frame size (%d) too large!!!\n", __func__, size);
		return -1;
	}

	return _dw_cec_message_tx(dev, buf, size, 5);
}

int dw_cec_enable(dw_hdmi_dev_t *dev)
{
	unsigned char mask = 0x0;
	_dw_cec_set_standby_mode(dev, 0);
	dw_mc_irq_unmute_source(dev, IRQ_CEC);
	_dw_cec_interrupt_disable(dev, CEC_MASK_WAKEUP_MASK);
	_dw_cec_interrupt_clear(dev, CEC_MASK_WAKEUP_MASK);

	/* clear cec wakeup control */
	dev_write(dev, CEC_WAKEUPCTRL, 0);

	mask |= CEC_MASK_DONE_MASK;
	mask |= CEC_MASK_EOM_MASK;
	mask |= CEC_MASK_NACK_MASK;
	mask |= CEC_MASK_ARB_LOST_MASK;
	mask |= CEC_MASK_ERROR_FLOW_MASK;
	mask |= CEC_MASK_ERROR_INITIATOR_MASK;
	_dw_cec_interrupt_enable(dev, mask);

	return 0;
}

int dw_cec_disable(dw_hdmi_dev_t *dev, int wakeup)
{
	unsigned char mask = 0x0;

	if (dev == NULL) {
		hdmi_err("%s: param dev is null!!!\n", __func__);
		return -1;
	}

	mask  = CEC_MASK_DONE_MASK;
	mask |= CEC_MASK_EOM_MASK;
	mask |= CEC_MASK_NACK_MASK;
	mask |= CEC_MASK_ARB_LOST_MASK;
	mask |= CEC_MASK_ERROR_FLOW_MASK;
	mask |= CEC_MASK_ERROR_INITIATOR_MASK;
	_dw_cec_interrupt_disable(dev, mask);

	if (wakeup) {
		_dw_cec_interrupt_clear(dev, CEC_MASK_WAKEUP_MASK);
		_dw_cec_interrupt_enable(dev, CEC_MASK_WAKEUP_MASK);
		_dw_cec_set_standby_mode(dev, 1);
	} else {
		dw_cec_set_logic_addr(dev, CEC_BROADCAST_ADDR, 1);
	}
	return 0;
}
