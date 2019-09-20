// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2019 Stephan Gerhold
 *
 * Registers taken from internal u-boot MCDE driver:
 * Copyright (C) ST-Ericsson SA 2010
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <video.h>
#include <asm/io.h>
#include <asm/system.h>

#define MCDE_REG2VAL(__reg, __fld, __val) \
	(((__val) & __reg##_##__fld##_MASK) >> __reg##_##__fld##_SHIFT)

#define MCDE_EXTSRC0A0				0x00000200
#define MCDE_EXTSRC0CONF			0x0000020C
#define MCDE_EXTSRC0CONF_BPP_SHIFT		8
#define MCDE_EXTSRC0CONF_BPP_MASK		0x00000F00
#define MCDE_OVL0CONF				0x00000404
#define MCDE_OVL0CONF_PPL_SHIFT			0
#define MCDE_OVL0CONF_PPL_MASK			0x000007FF
#define MCDE_OVL0CONF_LPF_SHIFT			16
#define MCDE_OVL0CONF_LPF_MASK			0x07FF0000

enum mcde_bpp {
	MCDE_EXTSRC0CONF_BPP_1BPP_PAL,
	MCDE_EXTSRC0CONF_BPP_2BPP_PAL,
	MCDE_EXTSRC0CONF_BPP_4BPP_PAL,
	MCDE_EXTSRC0CONF_BPP_8BPP_PAL,
	MCDE_EXTSRC0CONF_BPP_RGB444,
	MCDE_EXTSRC0CONF_BPP_ARGB4444,
	MCDE_EXTSRC0CONF_BPP_IRGB1555,
	MCDE_EXTSRC0CONF_BPP_RGB565,
	MCDE_EXTSRC0CONF_BPP_RGB888,
	MCDE_EXTSRC0CONF_BPP_XRGB8888,
	MCDE_EXTSRC0CONF_BPP_ARGB8888,
	MCDE_EXTSRC0CONF_BPP_YCBCR422
};

static int mcde_simple_probe(struct udevice *dev)
{
	struct video_uc_platdata *plat = dev_get_uclass_platdata(dev);
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);
	enum mcde_bpp bpp;
	fdt_addr_t base;
	unsigned int val;

	base = dev_read_addr(dev);
	if (base == FDT_ADDR_T_NONE)
		return -EINVAL;

	plat->base = readl(base + MCDE_EXTSRC0A0);
	if (!plat->base)
		return -ENODEV;

	val = readl(base + MCDE_OVL0CONF);
	uc_priv->xsize = MCDE_REG2VAL(MCDE_OVL0CONF, PPL, val);
	uc_priv->ysize = MCDE_REG2VAL(MCDE_OVL0CONF, LPF, val);
	uc_priv->rot = 0;

	val = readl(base + MCDE_EXTSRC0CONF);
	bpp = (enum mcde_bpp)MCDE_REG2VAL(MCDE_EXTSRC0CONF, BPP, val);
	switch (bpp) {
	case MCDE_EXTSRC0CONF_BPP_RGB565:
		uc_priv->bpix = VIDEO_BPP16;
		break;
	case MCDE_EXTSRC0CONF_BPP_XRGB8888:
	case MCDE_EXTSRC0CONF_BPP_ARGB8888:
		uc_priv->bpix = VIDEO_BPP32;
		break;
	default:
		printf("unsupported format: %d\n", bpp);
		return -EINVAL;
	}

	plat->size = uc_priv->xsize * uc_priv->ysize * VNBYTES(uc_priv->bpix);
	debug("MCDE base: 0x%lx, xsize: %d, ysize: %d, bpp: %d\n",
	      plat->base, uc_priv->xsize, uc_priv->ysize, VNBITS(uc_priv->bpix));

	video_set_flush_dcache(dev, true);
	return 0;
}

static const struct udevice_id mcde_simple_ids[] = {
	{ .compatible = "ste,mcde" },
	{ }
};

U_BOOT_DRIVER(mcde_simple) = {
	.name	= "mcde_simple",
	.id	= UCLASS_VIDEO,
	.of_match = mcde_simple_ids,
	.probe	= mcde_simple_probe,
};
