// SPDX-License-Identifier: GPL-2.0-only
/*
 * Generic GPIO driver for logic cells found in the Nomadik SoC
 * Copyright (C) 2019 Stephan Gerhold
 *
 * Based on Linux kernel driver (pinctrl-nomadik.c):
 *
 * Copyright (C) 2008,2009 STMicroelectronics
 * Copyright (C) 2009 Alessandro Rubini <rubini@unipv.it>
 *   Rewritten based on work by Prafulla WADASKAR <prafulla.wadaskar@st.com>
 * Copyright (C) 2011-2013 Linus Walleij <linus.walleij@linaro.org>
 */

#include <common.h>
#include <dm.h>
#include <asm/gpio.h>
#include <asm/io.h>

#define GPIO_BLOCK_SHIFT	5
#define NMK_GPIO_PER_CHIP	(1 << GPIO_BLOCK_SHIFT)

/* Register in the logic block */
#define NMK_GPIO_DAT	0x00
#define NMK_GPIO_DATS	0x04
#define NMK_GPIO_DATC	0x08
#define NMK_GPIO_PDIS	0x0c
#define NMK_GPIO_DIR	0x10
#define NMK_GPIO_DIRS	0x14
#define NMK_GPIO_DIRC	0x18
#define NMK_GPIO_SLPC	0x1c
#define NMK_GPIO_AFSLA	0x20
#define NMK_GPIO_AFSLB	0x24
#define NMK_GPIO_LOWEMI	0x28

struct nmk_gpio {
	phys_addr_t addr;
};

static int nmk_gpio_get_value(struct udevice *dev, unsigned offset)
{
	struct nmk_gpio *priv = dev_get_priv(dev);

	return !!(readl(priv->addr + NMK_GPIO_DAT) & BIT(offset));
}

static int nmk_gpio_set_value(struct udevice *dev, unsigned offset, int value)
{
	struct nmk_gpio *priv = dev_get_priv(dev);

	if (value)
		writel(BIT(offset), priv->addr + NMK_GPIO_DATS);
	else
		writel(BIT(offset), priv->addr + NMK_GPIO_DATC);

	return 0;
}

static int nmk_gpio_get_function(struct udevice *dev, unsigned offset)
{
	struct nmk_gpio *priv = dev_get_priv(dev);

	if (readl(priv->addr + NMK_GPIO_AFSLA) & BIT(offset) ||
	    readl(priv->addr + NMK_GPIO_AFSLB) & BIT(offset))
		return GPIOF_FUNC;

	if (readl(priv->addr + NMK_GPIO_DIR) & BIT(offset))
		return GPIOF_OUTPUT;
	else
		return GPIOF_INPUT;
}

static int nmk_gpio_direction_input(struct udevice *dev, unsigned offset)
{
	struct nmk_gpio *priv = dev_get_priv(dev);

	writel(BIT(offset), priv->addr + NMK_GPIO_DIRC);

	return 0;
}

static int nmk_gpio_direction_output(struct udevice *dev, unsigned offset,
					 int value)
{
	struct nmk_gpio *priv = dev_get_priv(dev);

	writel(BIT(offset), priv->addr + NMK_GPIO_DIRS);
	return nmk_gpio_set_value(dev, offset, value);
}

static const struct dm_gpio_ops nmk_gpio_ops = {
	.direction_input	= nmk_gpio_direction_input,
	.direction_output	= nmk_gpio_direction_output,
	.get_value		= nmk_gpio_get_value,
	.set_value		= nmk_gpio_set_value,
	.get_function		= nmk_gpio_get_function,
};

static int nmk_gpio_probe(struct udevice *dev)
{
	struct nmk_gpio *priv = dev_get_priv(dev);
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	char buf[20];
	u32 bank;
	int ret;

	priv->addr = dev_read_addr(dev);
	if (priv->addr == FDT_ADDR_T_NONE)
		return -EINVAL;

	ret = dev_read_u32(dev, "gpio-bank", &bank);
	if (ret < 0) {
		printf("nmk_gpio(%s): Failed to read gpio-bank\n", dev->name);
		return ret;
	}

	sprintf(buf, "nmk%u-gpio", bank);
	uc_priv->bank_name = strdup(buf);
	if (!uc_priv->bank_name)
		return -ENOMEM;

	uc_priv->gpio_count = NMK_GPIO_PER_CHIP;

	return 0;
}

static const struct udevice_id nmk_gpio_ids[] = {
	{ .compatible = "st,nomadik-gpio" },
	{ }
};

U_BOOT_DRIVER(gpio_nmk) = {
	.name	= "gpio_nmk",
	.id	= UCLASS_GPIO,
	.of_match = nmk_gpio_ids,
	.probe	= nmk_gpio_probe,
	.ops	= &nmk_gpio_ops,
	.priv_auto_alloc_size = sizeof(struct nmk_gpio),
};
