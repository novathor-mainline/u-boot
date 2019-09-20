// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2019 Stephan Gerhold
 *
 * Based on Linux ux500 MUSB glue driver:
 * Copyright (C) 2010 ST-Ericsson AB
 * Mian Yousaf Kaukab <mian.yousaf.kaukab@stericsson.com>
 *
 * Based on omap2430.c
 */

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <generic-phy.h>
#include <linux/usb/musb.h>
#include "linux-compat.h"
#include "musb_core.h"
#include "musb_uboot.h"

static struct musb_hdrc_config ux500_musb_hdrc_config = {
	.multipoint	= true,
	.dyn_fifo	= true,
	.num_eps	= 16,
	.ram_bits	= 16,
};

struct ux500_glue {
	struct musb_host_data mdata;
	struct device dev;
	struct clk clk;
	struct phy phy;
	bool enabled;
};
#define to_ux500_glue(d)	container_of(d, struct ux500_glue, dev)

static int ux500_musb_enable(struct musb *musb)
{
	struct ux500_glue *glue = to_ux500_glue(musb->controller);
	int ret;

	if (glue->enabled)
		return 0;

	ret = generic_phy_power_on(&glue->phy);
	if (ret) {
		printf("%s: failed to power on USB PHY\n", __func__);
		return ret;
	}

	glue->enabled = true;
	return 0;
}

static void ux500_musb_disable(struct musb *musb)
{
	struct ux500_glue *glue = to_ux500_glue(musb->controller);
	int ret;

	if (!glue->enabled)
		return;

	ret = generic_phy_power_off(&glue->phy);
	if (ret) {
		printf("%s: failed to power off USB PHY\n", __func__);
		return;
	}

	glue->enabled = false;
}

static int ux500_musb_init(struct musb *musb)
{
	struct ux500_glue *glue = to_ux500_glue(musb->controller);
	int ret;

#ifdef CONFIG_CLK
	ret = clk_enable(&glue->clk);
	if (ret) {
		printf("%s: failed to enable clock\n", __func__);
		return ret;
	}
#endif

	ret = generic_phy_init(&glue->phy);
	if (ret) {
		printf("%s: failed to init USB PHY\n", __func__);
		goto err_clk;
	}

	return 0;

err_clk:
#ifdef CONFIG_CLK
	clk_disable(&glue->clk);
#endif
	return ret;
}

static int ux500_musb_exit(struct musb *musb)
{
	struct ux500_glue *glue = to_ux500_glue(musb->controller);
	int ret;

	ret = generic_phy_exit(&glue->phy);
	if (ret) {
		printf("%s: failed to exit USB PHY\n", __func__);
		return ret;
	}

#ifdef CONFIG_CLK
	ret = clk_disable(&glue->clk);
	if (ret) {
		printf("%s: failed to disable clock\n", __func__);
		return ret;
	}
#endif

	return 0;
}

static const struct musb_platform_ops ux500_musb_ops = {
	.init		= ux500_musb_init,
	.exit		= ux500_musb_exit,
	.enable		= ux500_musb_enable,
	.disable	= ux500_musb_disable,
};

static int ux500_musb_probe(struct udevice *dev)
{
#ifdef CONFIG_USB_MUSB_HOST
	struct usb_bus_priv *priv = dev_get_uclass_priv(dev);
#endif
	struct ux500_glue *glue = dev_get_priv(dev);
	struct musb_host_data *host = &glue->mdata;
	struct musb_hdrc_platform_data pdata;
	void *base = dev_read_addr_ptr(dev);
	int ret;

	if (!base)
		return -EINVAL;

#ifdef CONFIG_CLK
	ret = clk_get_by_index(dev, 0, &glue->clk);
	if (ret) {
		printf("%s: failed to get clock\n", __func__);
		return ret;
	}
#endif

	ret = generic_phy_get_by_name(dev, "usb", &glue->phy);
	if (ret) {
		printf("%s: failed to get USB PHY\n", __func__);
		return ret;
	}

	memset(&pdata, 0, sizeof(pdata));
	pdata.platform_ops = &ux500_musb_ops;
	pdata.config = &ux500_musb_hdrc_config;

#ifdef CONFIG_USB_MUSB_HOST
	priv->desc_before_addr = true;
	pdata.mode = MUSB_HOST;
#else
	pdata.mode = MUSB_PERIPHERAL;
#endif

	host->host = musb_init_controller(&pdata, &glue->dev, base);
	if (!host->host)
		return -EIO;

	return 0;
}

static int ux500_musb_remove(struct udevice *dev)
{
	struct ux500_glue *glue = dev_get_priv(dev);
	struct musb_host_data *host = &glue->mdata;

	musb_stop(host->host);

	return 0;
}

static const struct udevice_id ux500_musb_ids[] = {
	{ .compatible = "stericsson,db8500-musb" },
	{ }
};

U_BOOT_DRIVER(ux500_musb) = {
	.name	= "ux500-musb",
#ifdef CONFIG_USB_MUSB_HOST
	.id		= UCLASS_USB,
#else
	.id		= UCLASS_USB_GADGET_GENERIC,
#endif
	.of_match = ux500_musb_ids,
	.probe = ux500_musb_probe,
	.remove = ux500_musb_remove,
#ifdef CONFIG_USB_MUSB_HOST
	.ops = &musb_usb_ops,
#endif
	.platdata_auto_alloc_size = sizeof(struct usb_platdata),
	.priv_auto_alloc_size = sizeof(struct ux500_glue),
};
