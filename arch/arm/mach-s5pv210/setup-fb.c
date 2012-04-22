/* linux/arch/arm/mach-s5pv210/setup-fb.c
 *
 * Copyright (c) 2009 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * Base FIMD controller configuration
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <plat/clock.h>
#include <plat/gpio-cfg.h>
#include <plat/fb.h>
#include <mach/regs-clock.h>
#include <mach/regs-gpio.h>
#include <linux/io.h>
#include <mach/map.h>
#include <mach/power-domain.h>
#include <mach/gpio-bank.h>


struct platform_device; /* don't need the contents */

void s3cfb_cfg_gpio(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF0(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF0(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF1(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF1(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF2(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF2(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 4; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF3(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF3(i), S3C_GPIO_PULL_NONE);
	}

	/* mDNIe SEL: why we shall write 0x2 ? */
	writel(0x2, S5P_MDNIE_SEL);

	/* drive strength to max */
	writel(0xffffffff, S5PV210_GPF0_BASE + 0xc);
	writel(0xffffffff, S5PV210_GPF1_BASE + 0xc);
	writel(0xffffffff, S5PV210_GPF2_BASE + 0xc);
	writel(0x000000ff, S5PV210_GPF3_BASE + 0xc);
}

int s3cfb_clk_on(struct platform_device *pdev, struct clk **s3cfb_clk)
{
	struct clk *sclk = NULL;
	struct clk *mout_mpll = NULL;
	u32 rate = 0;

	sclk = clk_get(&pdev->dev, "sclk_fimd");
	if (IS_ERR(sclk)) {
		dev_err(&pdev->dev, "failed to get sclk for fimd\n");
		goto err_clk1;
	}

	mout_mpll = clk_get(&pdev->dev, "mout_mpll");
	if (IS_ERR(mout_mpll)) {
		dev_err(&pdev->dev, "failed to get mout_mpll\n");
		goto err_clk2;
	}

	clk_set_parent(sclk, mout_mpll);

	if (!rate)
		rate = 166750000;

	clk_set_rate(sclk, rate);
	dev_dbg(&pdev->dev, "set fimd sclk rate to %d\n", rate);

	clk_put(mout_mpll);

	clk_enable(sclk);

	*s3cfb_clk = sclk;

	return 0;

err_clk2:
	clk_put(sclk);

err_clk1:
	return -EINVAL;
}

int s3cfb_clk_off(struct platform_device *pdev, struct clk **clk)
{
	clk_disable(*clk);
	clk_put(*clk);

	*clk = NULL;

	return 0;
}

void s3cfb_get_clk_name(char *clk_name)
{
	strcpy(clk_name, "sclk_fimd");
}

void s3cfb_lateresume_cfg_gpio(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < 8; i++) {
		s3c_gpio_setpull_pd(S5PV210_GPF0(i), 0);
	}
	for (i = 0; i < 8; i++) {
		s3c_gpio_setpull_pd(S5PV210_GPF1(i), 0);
	}
	for (i = 0; i < 8; i++) {
		s3c_gpio_setpull_pd(S5PV210_GPF2(i), 0);
	}
	for (i = 0; i < 4; i++) {
		s3c_gpio_setpull_pd(S5PV210_GPF3(i), 0);
	}
}

void s3cfb_earlysuspend_cfg_gpio(struct platform_device *pdev)
{
	int i;
	for (i = 0; i < 8; i++) {	
		s3c_gpio_setpin_pd(S5PV210_GPF0(i), 2);
		s3c_gpio_setpull_pd(S5PV210_GPF0(i), 1);
	}
	for (i = 0; i < 8; i++) {
		s3c_gpio_setpin_pd(S5PV210_GPF1(i), 2);
		s3c_gpio_setpull_pd(S5PV210_GPF1(i), 1);
	}
	for (i = 0; i < 8; i++) {
		s3c_gpio_setpin_pd(S5PV210_GPF2(i), 2);
		s3c_gpio_setpull_pd(S5PV210_GPF2(i), 1);
	}
	for (i = 0; i < 4; i++) {
		s3c_gpio_setpin_pd(S5PV210_GPF3(i), 2);
		s3c_gpio_setpull_pd(S5PV210_GPF3(i), 1);
	}	
}

int s3cfb_backlight_onoff(struct platform_device *pdev, int onoff)
{
    int err;

    // mg3100: GPH2[4] LCD backlight
    unsigned int nGPIO = S5PV210_GPH2(4);
	err = gpio_request(nGPIO, "backlight-en");

	if (err) {
		printk(KERN_ERR "failed to request GPH2[4] for backlight control\n");
		return err;
	}

    if (onoff)
        gpio_direction_output(nGPIO, 1);
    else
        gpio_direction_output(nGPIO, 0);

	mdelay(10);
    gpio_free(nGPIO);

	return 0;
}

static int s3cfb_lcd_onoff(struct platform_device *pdev, int onoff)
{
    int err;

    // namko: Use GPH1[6] and GPH1[7] to control LCD power (including backlight)
    unsigned int nGPIOs[] = {S5PV210_GPH1(6), S5PV210_GPH1(7)};

	if ((err = gpio_request(nGPIOs[0], "lcd-backlight-en"))) {
		printk(KERN_ERR "failed to request GPH1[6] for lcd control\n");
		return err;
	}

	if ((err = gpio_request(nGPIOs[1], "lcd-backlight-en"))) {
		printk(KERN_ERR "failed to request GPH1[7] for lcd control\n");
        gpio_free(nGPIOs[0]);
		return err;
	}

    if (onoff) {
        gpio_direction_output(nGPIOs[0], 1);
        gpio_direction_output(nGPIOs[1], 0);
    } else {
        gpio_direction_output(nGPIOs[0], 0);
        gpio_direction_output(nGPIOs[1], 1);
    }

	mdelay(20);
    gpio_free(nGPIOs[1]);
    gpio_free(nGPIOs[0]);

	return 0;
}

int s3cfb_backlight_on(struct platform_device *pdev)
{
    return s3cfb_backlight_onoff(pdev, 1);
}

int s3cfb_backlight_off(struct platform_device *pdev)
{
    return s3cfb_backlight_onoff(pdev, 0);
}

int s3cfb_lcd_on(struct platform_device *pdev)
{
	return s3cfb_lcd_onoff(pdev, 1);
}

int s3cfb_lcd_off(struct platform_device *pdev)
{
	return s3cfb_lcd_onoff(pdev, 0);
}

int s3cfb_reset_lcd(struct platform_device *pdev)
{
    s3cfb_lcd_on(pdev);
	mdelay(200);
    s3cfb_lcd_off(pdev);
    s3cfb_lcd_on(pdev);
	return 0;
}

