/*
 * xr806/main.c
 *
 * Copyright (c) 2022
 * Allwinner Technology Co., Ltd. <www.allwinner.com>
 * laumy <liumingyuan@allwinner.com>
 *
 * Main code init for Xr806 drivers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#include "os_intf.h"

#if PLATFORM_LINUX
#include <linux/init.h>
#include <linux/module.h>
#endif

#include "xradio.h"
#include "os_net.h"
#include "hwio.h"
#include "txrx.h"
#include "up_cmd.h"
#include "debug.h"
#include "low_cmd.h"
#include "xr_version.h"
#include "data_test.h"

MODULE_AUTHOR("XRadioTech");
MODULE_DESCRIPTION("XRadioTech WLAN driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("xr806");

static struct xradio_priv *priv;

static void xradio_reset_driver(void);

static int xradio_core_init(void)
{
	int ret = 0;
	int i = 0;
	xradio_dbg(XRADIO_DBG_ALWY, "xradio wlan version:%s\n", XRADIO_VERSION);

	xradio_dbg(XRADIO_DBG_ALWY, "add wlan interface dev\n");

	priv = (struct xradio_priv *)xradio_k_zmalloc(sizeof(struct xradio_priv));
	if (!priv) {
		xradio_dbg(XRADIO_DBG_ERROR, "xradio priv malloc failed.\n");
		return -1;
	}

	if (xradio_add_net_dev(priv, XR_STA_IF, 0, "wlan0") != 0)
		goto error;

	if (xradio_debug_init_common(priv))
		goto error1;

	xradio_dbg(XRADIO_DBG_ALWY, "xradio queue init\n");
	for (i = 0; i < XR_MAX; i++) {
		if (xradio_queue_init(&priv->tx_queue[i], i,
				(i == XR_DATA ? XRWL_DATA_QUEUE_SZ : XRWL_CMD_QUEUE_SZ),
				2 * HZ, xradio_free_skb)) {
			for (; i > 0; i--)
				xradio_queue_deinit(&priv->tx_queue[i - 1]);
			goto error2;
		}
	}

	xradio_dbg(XRADIO_DBG_ALWY, "hardware io init\n");
	ret = xradio_hwio_init(priv);
	if (ret)
		goto error3;

	xradio_dbg(XRADIO_DBG_ALWY, "register tx and rx task\n");
	ret = xradio_register_trans(priv);
	if (ret)
		goto error4;

	xradio_dbg(XRADIO_DBG_ALWY, "up cmd init\n");
	ret = xradio_up_cmd_init(priv);
	if (ret)
		goto error5;

	xradio_dbg(XRADIO_DBG_ALWY, "low cmd init\n");
	ret = xradio_low_cmd_init(priv);
	if (ret)
		goto error6;

	ret = xraido_low_cmd_dev_hand_way();
	if (ret)
		goto error7;

	xradio_dbg(XRADIO_DBG_ALWY, "xradio core init Suceess\n");
#if DATA_TEST
	xradio_data_test_start(priv);
#endif

	/*xradio_keep_alive(xradio_reset_driver);*/

	return 0;
error7:
	xradio_low_cmd_deinit(priv);
error6:
	xradio_up_cmd_deinit(priv);
error5:
	xradio_unregister_trans(priv);
error4:
	xradio_hwio_deinit(priv);
error3:
	for (i = 0; i < XR_MAX; i++)
		xradio_queue_deinit(&priv->tx_queue[i]);
error2:
	xradio_debug_deinit_common(priv);
error1:
	xradio_remove_net_dev(priv);
error:
	xradio_k_free(priv);
	priv = NULL;
	xradio_dbg(XRADIO_DBG_ERROR, "xradio core init Failed\n");
	return ret;
}

static void xradio_core_deinit(void)
{
	int i;
#if DATA_TEST
	xradio_data_test_stop(priv);
#endif
	if (!priv)
		return;

	xradio_dbg(XRADIO_DBG_ALWY, "low cmd deinit\n");
	xradio_low_cmd_deinit(priv);

	xradio_dbg(XRADIO_DBG_ALWY, "up cmd deinit\n");
	xradio_up_cmd_deinit(priv);

	xradio_dbg(XRADIO_DBG_ALWY, "unregister tx rx transmit\n");
	xradio_unregister_trans(priv);

	xradio_dbg(XRADIO_DBG_ALWY, "xradio queue deinit\n");
	for (i = 0; i < XR_MAX; i++)
		xradio_queue_deinit(&priv->tx_queue[i]);

	xradio_dbg(XRADIO_DBG_ALWY, "hardware io deinit\n");
	xradio_hwio_deinit(priv);

	xradio_debug_deinit_common(priv);

	xradio_dbg(XRADIO_DBG_ALWY, "xradio wlan interface dev remove\n");

	xradio_remove_net_dev(priv);

	xradio_k_free(priv);

	priv = NULL;
}

static void xradio_reset_driver(void)
{
	xradio_dbg(XRADIO_DBG_ALWY, "hardware io deinit\n");
	xradio_core_deinit();

	msleep(500);

	xradio_dbg(XRADIO_DBG_ALWY, "hardware io init\n");
	xradio_core_init();
}

#if PLATFORM_LINUX
static int __init xradio_init(void)
{
	return xradio_core_init();
}

static void __exit xradio_exit(void)
{
	xradio_core_deinit();
}

module_init(xradio_init);
module_exit(xradio_exit);
#endif
