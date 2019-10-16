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
#define MCDE_CHNL0SYNCHMOD			0x00000608
#define MCDE_CHNL0SYNCHMOD_SRC_SYNCH_SHIFT	0
#define MCDE_CHNL0SYNCHMOD_SRC_SYNCH_MASK	0x00000003
#define MCDE_CHNL0SYNCHSW			0x0000060C
#define MCDE_CHNL0SYNCHSW_SW_TRIG		1
#define MCDE_CRA0				0x00000800
#define MCDE_CRA0_FLOEN				1

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

enum mcde_src_synch {
	MCDE_CHNL0SYNCHMOD_SRC_SYNCH_HARDWARE,
	MCDE_CHNL0SYNCHMOD_SRC_SYNCH_NO_SYNCH,
	MCDE_CHNL0SYNCHMOD_SRC_SYNCH_SOFTWARE
};

struct mcde_simple_priv {
	fdt_addr_t base;
	enum mcde_src_synch src_synch;
};

static int mcde_simple_probe(struct udevice *dev)
{
	struct mcde_simple_priv *priv = dev_get_priv(dev);
	struct video_uc_platdata *plat = dev_get_uclass_platdata(dev);
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);
	enum mcde_bpp bpp;
	unsigned int val;

	priv->base = dev_read_addr(dev);
	if (priv->base == FDT_ADDR_T_NONE)
		return -EINVAL;

	plat->base = readl(priv->base + MCDE_EXTSRC0A0);
	if (!plat->base)
		return -ENODEV;

	val = readl(priv->base + MCDE_OVL0CONF);
	uc_priv->xsize = MCDE_REG2VAL(MCDE_OVL0CONF, PPL, val);
	uc_priv->ysize = MCDE_REG2VAL(MCDE_OVL0CONF, LPF, val);
	uc_priv->rot = 0;

	val = readl(priv->base + MCDE_EXTSRC0CONF);
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

	val = readl(priv->base + MCDE_CHNL0SYNCHMOD);
	priv->src_synch = (enum mcde_src_synch) MCDE_REG2VAL(MCDE_CHNL0SYNCHMOD,
							     SRC_SYNCH, val);

	plat->size = uc_priv->xsize * uc_priv->ysize * VNBYTES(uc_priv->bpix);
	debug("MCDE base: 0x%lx, xsize: %d, ysize: %d, bpp: %d\n",
	      plat->base, uc_priv->xsize, uc_priv->ysize, VNBITS(uc_priv->bpix));

	video_set_flush_dcache(dev, true);
	return 0;
}

void mcde_simple_sync(struct udevice *dev, bool force)
{
	struct mcde_simple_priv *priv = dev_get_priv(dev);
	unsigned int val;

	if (priv->src_synch != MCDE_CHNL0SYNCHMOD_SRC_SYNCH_SOFTWARE)
		return;

	/* Enable flow */
	val = readl(priv->base + MCDE_CRA0);
	val |= MCDE_CRA0_FLOEN;
	writel(val, priv->base + MCDE_CRA0);

	/* Trigger a software sync */
	writel(MCDE_CHNL0SYNCHSW_SW_TRIG, priv->base + MCDE_CHNL0SYNCHSW);

	/* Disable flow */
	val = readl(priv->base + MCDE_CRA0);
	val &= ~MCDE_CRA0_FLOEN;
	writel(val, priv->base + MCDE_CRA0);

	/* Wait for completion */
	while (readl(priv->base + MCDE_CRA0) & MCDE_CRA0_FLOEN) {}
}

static struct video_ops mcde_simple_ops = {
	.sync = mcde_simple_sync,
};

static const struct udevice_id mcde_simple_ids[] = {
	{ .compatible = "ste,mcde" },
	{ }
};

U_BOOT_DRIVER(mcde_simple) = {
	.name	= "mcde_simple",
	.id	= UCLASS_VIDEO,
	.ops	= &mcde_simple_ops,
	.of_match = mcde_simple_ids,
	.probe	= mcde_simple_probe,
	.priv_auto_alloc_size = sizeof(struct mcde_simple_priv),
};
