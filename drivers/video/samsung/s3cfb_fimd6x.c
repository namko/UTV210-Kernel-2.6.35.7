/* linux/drivers/video/samsung/s3cfb_fimd6x.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * Register interface file for Samsung Display Controller (FIMD) driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fb.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <mach/map.h>
#include <plat/clock.h>
#include <plat/fb.h>
#include <plat/regs-fb.h>

#include "s3cfb.h"

//SSCR xuhui 110130
#define S3C_FB_GAMMA_TBLINX	0 // 0: 1.00 1: 0.40 2: 0.50 3: 0.67 4: 1.50 5: 2.00 6: 2.50

static int gamma_tbl[6][64] = {
/*0.40*/
	{
		0, 194, 256, 301, 338, 369, 397, 423, 
		446, 467, 487, 506, 524, 541, 558, 573,
		588, 603, 617, 630, 643, 656, 668, 680,
		692, 703, 714, 725, 736, 746, 756, 766,
		776, 786, 795, 804, 813, 822, 831, 840,
		849, 857, 865, 873, 881, 889, 897, 905,
		913, 920, 928, 935, 942, 950, 957, 964,
		971, 978, 984, 991, 998, 1005, 1011, 1018, 
		//1024
	},

/*0.50*/
	{
		0, 128, 181, 222, 256, 286, 314, 339,
		362, 384, 405, 425, 443, 462, 479, 496,
		512, 528, 543, 558, 572, 587, 600, 614,
		627, 640, 653, 665, 677, 689, 701, 713,
		724, 735, 746, 757, 768, 779, 789, 799,
		810, 820, 830, 839, 849, 859, 868, 878,
		887, 896, 905, 914, 923, 932, 941, 949,
		958, 966, 975, 983, 991, 1000, 1008, 1016,
		//1024
	},

/*0.67*/
	{
		0, 64, 102, 133, 161, 187, 211, 234, 
		256, 277, 297, 317, 335, 354, 372, 389,
		406, 423, 440, 456, 472, 487, 502, 518, 
		533, 547, 562, 576, 590, 604, 618, 632,
		645, 658, 672, 685, 698, 711, 723, 736, 
		749, 761, 773, 786, 798, 810, 822, 834,
		845, 857, 869, 880, 892, 903, 914, 926,
		937, 948, 959, 970, 981, 992, 1003, 1013,
		//1024
	},

/*1.50*/
	{
		0, 2, 6, 10, 16, 22, 29, 37,
		45, 54, 63, 73, 83, 94, 105,116,
		128, 140, 153, 166, 179, 192, 206, 221,
		235, 250, 265, 281, 296, 312, 329, 345,
		362, 379, 397, 414, 432, 450, 468, 487, 
		506, 525, 544, 564, 584, 604, 624, 644,
		665, 686, 707, 728, 750, 772, 794, 816,
		838, 861, 883, 906, 930, 953, 976, 1000,
		//1024
	},

/*2.00*/
	{
		0, 0, 1, 2, 4, 6, 9, 12,
		16, 20, 25, 30, 36, 42, 49,56,
		64, 72, 81, 90, 100, 110, 121, 132,
		144, 156, 169, 182, 196, 210, 225, 240,
		256, 272, 289, 306, 324, 342, 361, 380,
		400, 420, 441, 462, 484, 506, 529, 552, 
		576, 600, 625, 650, 676, 702, 729, 756, 
		784, 812, 841, 870, 900, 930, 961, 992,
		//1024	
	},

/*2.50*/
	{
		0, 0, 0, 0, 1, 2, 3, 4,
		6, 8, 10, 13, 16, 19, 23, 27,
		32, 37, 43, 49, 56, 63, 71, 79,
		88, 98, 108, 118, 130, 142, 154, 167,
		181, 195, 211, 226, 243, 260, 278, 297,
		316, 336, 357, 379, 401, 425, 448, 473,
		499, 525, 552, 580, 609, 639, 670, 701,
		733, 767, 801, 836, 871, 908, 946, 984,
		//1024
	}
};

void s3cfb_check_line_count(struct s3cfb_global *ctrl)
{
	int timeout = 30 * 5300;
	int i = 0;

	do {
		if (!(readl(ctrl->regs + S3C_VIDCON1) & 0x7ff0000))
			break;
		i++;
	} while (i < timeout);

	if (i == timeout) {
		dev_err(ctrl->dev, "line count mismatch\n");
		s3cfb_display_on(ctrl);
	}
}

int s3cfb_set_output(struct s3cfb_global *ctrl)
{
	u32 cfg;

	cfg = readl(ctrl->regs + S3C_VIDCON0);
	cfg &= ~S3C_VIDCON0_VIDOUT_MASK;

	if (ctrl->output == OUTPUT_RGB)
		cfg |= S3C_VIDCON0_VIDOUT_RGB;
	else if (ctrl->output == OUTPUT_ITU)
		cfg |= S3C_VIDCON0_VIDOUT_ITU;
	else if (ctrl->output == OUTPUT_I80LDI0)
		cfg |= S3C_VIDCON0_VIDOUT_I80LDI0;
	else if (ctrl->output == OUTPUT_I80LDI1)
		cfg |= S3C_VIDCON0_VIDOUT_I80LDI1;
	else if (ctrl->output == OUTPUT_WB_RGB)
		cfg |= S3C_VIDCON0_VIDOUT_WB_RGB;
	else if (ctrl->output == OUTPUT_WB_I80LDI0)
		cfg |= S3C_VIDCON0_VIDOUT_WB_I80LDI0;
	else if (ctrl->output == OUTPUT_WB_I80LDI1)
		cfg |= S3C_VIDCON0_VIDOUT_WB_I80LDI1;
	else {
		dev_err(ctrl->dev, "invalid output type: %d\n", ctrl->output);
		return -EINVAL;
	}

	writel(cfg, ctrl->regs + S3C_VIDCON0);

	cfg = readl(ctrl->regs + S3C_VIDCON2);
	cfg &= ~(S3C_VIDCON2_WB_MASK | S3C_VIDCON2_TVFORMATSEL_MASK | \
					S3C_VIDCON2_TVFORMATSEL_YUV_MASK);

	if (ctrl->output == OUTPUT_RGB)
		cfg |= S3C_VIDCON2_WB_DISABLE;
	else if (ctrl->output == OUTPUT_ITU)
		cfg |= S3C_VIDCON2_WB_DISABLE;
	else if (ctrl->output == OUTPUT_I80LDI0)
		cfg |= S3C_VIDCON2_WB_DISABLE;
	else if (ctrl->output == OUTPUT_I80LDI1)
		cfg |= S3C_VIDCON2_WB_DISABLE;
	else if (ctrl->output == OUTPUT_WB_RGB)
		cfg |= (S3C_VIDCON2_WB_ENABLE | S3C_VIDCON2_TVFORMATSEL_SW | \
					S3C_VIDCON2_TVFORMATSEL_YUV444);
	else if (ctrl->output == OUTPUT_WB_I80LDI0)
		cfg |= (S3C_VIDCON2_WB_ENABLE | S3C_VIDCON2_TVFORMATSEL_SW | \
					S3C_VIDCON2_TVFORMATSEL_YUV444);
	else if (ctrl->output == OUTPUT_WB_I80LDI1)
		cfg |= (S3C_VIDCON2_WB_ENABLE | S3C_VIDCON2_TVFORMATSEL_SW | \
					S3C_VIDCON2_TVFORMATSEL_YUV444);
	else {
		dev_err(ctrl->dev, "invalid output type: %d\n", ctrl->output);
		return -EINVAL;
	}

	writel(cfg, ctrl->regs + S3C_VIDCON2);

	return 0;
}

int s3cfb_set_display_mode(struct s3cfb_global *ctrl)
{
	u32 cfg;

	cfg = readl(ctrl->regs + S3C_VIDCON0);
	cfg &= ~S3C_VIDCON0_PNRMODE_MASK;
	cfg |= (ctrl->rgb_mode << S3C_VIDCON0_PNRMODE_SHIFT);
	writel(cfg, ctrl->regs + S3C_VIDCON0);

	return 0;
}

int s3cfb_display_on(struct s3cfb_global *ctrl)
{
	u32 cfg;

	cfg = readl(ctrl->regs + S3C_VIDCON0);
	cfg |= (S3C_VIDCON0_ENVID_ENABLE | S3C_VIDCON0_ENVID_F_ENABLE);
	writel(cfg, ctrl->regs + S3C_VIDCON0);

	dev_dbg(ctrl->dev, "global display is on\n");

	return 0;
}

int s3cfb_display_off(struct s3cfb_global *ctrl)
{
	u32 cfg;

	cfg = readl(ctrl->regs + S3C_VIDCON0);
	cfg &= ~S3C_VIDCON0_ENVID_ENABLE;
	writel(cfg, ctrl->regs + S3C_VIDCON0);

	cfg &= ~S3C_VIDCON0_ENVID_F_ENABLE;
	writel(cfg, ctrl->regs + S3C_VIDCON0);

	dev_dbg(ctrl->dev, "global display is off\n");

	return 0;
}

int s3cfb_frame_off(struct s3cfb_global *ctrl)
{
	u32 cfg;

	cfg = readl(ctrl->regs + S3C_VIDCON0);
	cfg &= ~S3C_VIDCON0_ENVID_F_ENABLE;
	writel(cfg, ctrl->regs + S3C_VIDCON0);

	dev_dbg(ctrl->dev, "current frame display is off\n");

	return 0;
}

int s3cfb_set_clock(struct s3cfb_global *ctrl)
{
	struct s3c_platform_fb *pdata = to_fb_plat(ctrl->dev);
	u32 cfg, maxclk, src_clk, vclk, div;

	maxclk = 86 * 1000000;

	/* fixed clock source: hclk */
	cfg = readl(ctrl->regs + S3C_VIDCON0);
	cfg &= ~(S3C_VIDCON0_CLKSEL_MASK | S3C_VIDCON0_CLKVALUP_MASK |
		S3C_VIDCON0_VCLKEN_MASK | S3C_VIDCON0_CLKDIR_MASK);
	cfg |= (S3C_VIDCON0_CLKVALUP_ALWAYS | S3C_VIDCON0_VCLKEN_NORMAL |
		S3C_VIDCON0_CLKDIR_DIVIDED);


	if (strcmp(pdata->clk_name, "sclk_fimd") == 0) {
		cfg |= S3C_VIDCON0_CLKSEL_SCLK;
		src_clk = clk_get_rate(ctrl->clock);
		printk(KERN_INFO "FIMD src sclk = %d\n", src_clk);
	} else {
		cfg |= S3C_VIDCON0_CLKSEL_HCLK;
		src_clk = ctrl->clock->parent->rate;
		printk(KERN_INFO "FIMD src hclk = %d\n", src_clk);
	}

	vclk = ctrl->fb[pdata->default_win]->var.pixclock;

	if (vclk > maxclk) {
		dev_info(ctrl->dev, "vclk(%d) should be smaller than %d\n",
			vclk, maxclk);
		/* vclk = maxclk; */
	}

	div = src_clk / vclk;
	if (src_clk % vclk)
		div++;

	if ((src_clk/div) > maxclk)
		dev_info(ctrl->dev, "vclk(%d) should be smaller than %d Hz\n",
			src_clk/div, maxclk);

	cfg |= S3C_VIDCON0_CLKVAL_F(div - 1);
	writel(cfg, ctrl->regs + S3C_VIDCON0);

	dev_dbg(ctrl->dev, "parent clock: %d, vclk: %d, vclk div: %d\n",
			src_clk, vclk, div);

	return 0;
}

int s3cfb_set_polarity(struct s3cfb_global *ctrl)
{
	struct s3cfb_lcd_polarity *pol;
	u32 cfg;

	pol = &ctrl->lcd->polarity;
	cfg = 0;

	if (pol == NULL)
		{
			printk("pol is null in s3cfb_set_polarity\n");
			return 0;
		}

	if (pol->rise_vclk)
		cfg |= S3C_VIDCON1_IVCLK_RISING_EDGE;

	if (pol->inv_hsync)
		cfg |= S3C_VIDCON1_IHSYNC_INVERT;

	if (pol->inv_vsync)
		cfg |= S3C_VIDCON1_IVSYNC_INVERT;

	if (pol->inv_vden)
		cfg |= S3C_VIDCON1_IVDEN_INVERT;

	writel(cfg, ctrl->regs + S3C_VIDCON1);

	return 0;
}

int s3cfb_set_timing(struct s3cfb_global *ctrl)
{
	struct s3cfb_lcd_timing *time;
	u32 cfg;

	time = &ctrl->lcd->timing;
	cfg = 0;

	cfg |= S3C_VIDTCON0_VBPDE(time->v_bpe - 1);
	cfg |= S3C_VIDTCON0_VBPD(time->v_bp - 1);
	cfg |= S3C_VIDTCON0_VFPD(time->v_fp - 1);
	cfg |= S3C_VIDTCON0_VSPW(time->v_sw - 1);

	writel(cfg, ctrl->regs + S3C_VIDTCON0);

	cfg = 0;

	cfg |= S3C_VIDTCON1_VFPDE(time->v_fpe - 1);
	cfg |= S3C_VIDTCON1_HBPD(time->h_bp - 1);
	cfg |= S3C_VIDTCON1_HFPD(time->h_fp - 1);
	cfg |= S3C_VIDTCON1_HSPW(time->h_sw - 1);

	writel(cfg, ctrl->regs + S3C_VIDTCON1);

	return 0;
}

int s3cfb_set_lcd_size(struct s3cfb_global *ctrl)
{
	u32 cfg = 0;

	cfg |= S3C_VIDTCON2_HOZVAL(ctrl->lcd->width - 1);
	cfg |= S3C_VIDTCON2_LINEVAL(ctrl->lcd->height - 1);

	writel(cfg, ctrl->regs + S3C_VIDTCON2);

	return 0;
}

int s3cfb_set_global_interrupt(struct s3cfb_global *ctrl, int enable)
{
	u32 cfg = 0;

	cfg = readl(ctrl->regs + S3C_VIDINTCON0);
	cfg &= ~(S3C_VIDINTCON0_INTFRMEN_ENABLE | S3C_VIDINTCON0_INT_ENABLE);

	if (enable) {
		dev_dbg(ctrl->dev, "video interrupt is on\n");
		cfg |= (S3C_VIDINTCON0_INTFRMEN_ENABLE |
			S3C_VIDINTCON0_INT_ENABLE);
	} else {
		dev_dbg(ctrl->dev, "video interrupt is off\n");
		cfg |= (S3C_VIDINTCON0_INTFRMEN_DISABLE |
			S3C_VIDINTCON0_INT_DISABLE);
	}

	writel(cfg, ctrl->regs + S3C_VIDINTCON0);

	return 0;
}

int s3cfb_set_vsync_interrupt(struct s3cfb_global *ctrl, int enable)
{
	u32 cfg = 0;

	cfg = readl(ctrl->regs + S3C_VIDINTCON0);
	cfg &= ~S3C_VIDINTCON0_FRAMESEL0_MASK;

	if (enable) {
		dev_dbg(ctrl->dev, "vsync interrupt is on\n");
		cfg |= S3C_VIDINTCON0_FRAMESEL0_VSYNC;
	} else {
		dev_dbg(ctrl->dev, "vsync interrupt is off\n");
		cfg &= ~S3C_VIDINTCON0_FRAMESEL0_VSYNC;
	}

	writel(cfg, ctrl->regs + S3C_VIDINTCON0);

	return 0;
}

int s3cfb_get_vsync_interrupt(struct s3cfb_global *ctrl)
{
	u32 cfg = 0;

	cfg = readl(ctrl->regs + S3C_VIDINTCON0);
	cfg &= S3C_VIDINTCON0_FRAMESEL0_VSYNC;

	if (cfg & S3C_VIDINTCON0_FRAMESEL0_VSYNC) {
		dev_dbg(ctrl->dev, "vsync interrupt is on\n");
		return 1;
	} else {
		dev_dbg(ctrl->dev, "vsync interrupt is off\n");
		return 0;
	}
}


#ifdef CONFIG_FB_S3C_TRACE_UNDERRUN
int s3cfb_set_fifo_interrupt(struct s3cfb_global *ctrl, int enable)
{
	u32 cfg = 0;

	cfg = readl(ctrl->regs + S3C_VIDINTCON0);

	cfg &= ~(S3C_VIDINTCON0_FIFOSEL_MASK | S3C_VIDINTCON0_FIFOLEVEL_MASK);
	cfg |= (S3C_VIDINTCON0_FIFOSEL_ALL | S3C_VIDINTCON0_FIFOLEVEL_EMPTY);

	if (enable) {
		dev_dbg(ctrl->dev, "fifo interrupt is on\n");
		cfg |= (S3C_VIDINTCON0_INTFIFO_ENABLE |
			S3C_VIDINTCON0_INT_ENABLE);
	} else {
		dev_dbg(ctrl->dev, "fifo interrupt is off\n");
		cfg &= ~(S3C_VIDINTCON0_INTFIFO_ENABLE |
			S3C_VIDINTCON0_INT_ENABLE);
	}

	writel(cfg, ctrl->regs + S3C_VIDINTCON0);

	return 0;
}
#endif

int s3cfb_clear_interrupt(struct s3cfb_global *ctrl)
{
	u32 cfg = 0;

	cfg = readl(ctrl->regs + S3C_VIDINTCON1);

	if (cfg & S3C_VIDINTCON1_INTFIFOPEND)
		dev_info(ctrl->dev, "fifo underrun occur\n");

	cfg |= (S3C_VIDINTCON1_INTVPPEND | S3C_VIDINTCON1_INTI80PEND |
		S3C_VIDINTCON1_INTFRMPEND | S3C_VIDINTCON1_INTFIFOPEND);

	writel(cfg, ctrl->regs + S3C_VIDINTCON1);

	return 0;
}

int s3cfb_channel_localpath_on(struct s3cfb_global *ctrl, int id)
{
	struct s3c_platform_fb *pdata = to_fb_plat(ctrl->dev);
	u32 cfg;

	if (pdata->hw_ver == 0x62) {
		cfg = readl(ctrl->regs + S3C_WINSHMAP);
		cfg |= S3C_WINSHMAP_LOCAL_ENABLE(id);
		writel(cfg, ctrl->regs + S3C_WINSHMAP);
	}

	dev_dbg(ctrl->dev, "[fb%d] local path enabled\n", id);

	return 0;
}

int s3cfb_channel_localpath_off(struct s3cfb_global *ctrl, int id)
{
	struct s3c_platform_fb *pdata = to_fb_plat(ctrl->dev);
	u32 cfg;

	if (pdata->hw_ver == 0x62) {
		cfg = readl(ctrl->regs + S3C_WINSHMAP);
		cfg &= ~S3C_WINSHMAP_LOCAL_DISABLE(id);
		writel(cfg, ctrl->regs + S3C_WINSHMAP);
	}

	dev_dbg(ctrl->dev, "[fb%d] local path disabled\n", id);

	return 0;
}

int s3cfb_window_on(struct s3cfb_global *ctrl, int id)
{
	struct s3c_platform_fb *pdata = to_fb_plat(ctrl->dev);
	u32 cfg;

	cfg = readl(ctrl->regs + S3C_WINCON(id));
	cfg |= S3C_WINCON_ENWIN_ENABLE;
	writel(cfg, ctrl->regs + S3C_WINCON(id));

	if (pdata->hw_ver == 0x62) {
		cfg = readl(ctrl->regs + S3C_WINSHMAP);
		cfg |= S3C_WINSHMAP_CH_ENABLE(id);
		writel(cfg, ctrl->regs + S3C_WINSHMAP);
	}

	dev_dbg(ctrl->dev, "[fb%d] turn on\n", id);

	return 0;
}

int s3cfb_window_off(struct s3cfb_global *ctrl, int id)
{
	struct s3c_platform_fb *pdata = to_fb_plat(ctrl->dev);
	u32 cfg;

	cfg = readl(ctrl->regs + S3C_WINCON(id));
	cfg &= ~(S3C_WINCON_ENWIN_ENABLE | S3C_WINCON_DATAPATH_MASK);
	cfg |= S3C_WINCON_DATAPATH_DMA;
	writel(cfg, ctrl->regs + S3C_WINCON(id));

	if (pdata->hw_ver == 0x62) {
		cfg = readl(ctrl->regs + S3C_WINSHMAP);
		cfg &= ~S3C_WINSHMAP_CH_DISABLE(id);
		writel(cfg, ctrl->regs + S3C_WINSHMAP);
	}

	dev_dbg(ctrl->dev, "[fb%d] turn off\n", id);

	return 0;
}

int s3cfb_win_map_on(struct s3cfb_global *ctrl, int id, int color)
{
	u32 cfg = 0;

	cfg |= S3C_WINMAP_ENABLE;
	cfg |= S3C_WINMAP_COLOR(color);
	writel(cfg, ctrl->regs + S3C_WINMAP(id));

	dev_dbg(ctrl->dev, "[fb%d] win map on : 0x%08x\n", id, color);

	return 0;
}

int s3cfb_win_map_off(struct s3cfb_global *ctrl, int id)
{
	writel(0, ctrl->regs + S3C_WINMAP(id));

	dev_dbg(ctrl->dev, "[fb%d] win map off\n", id);

	return 0;
}

int s3cfb_set_window_control(struct s3cfb_global *ctrl, int id)
{
	struct s3c_platform_fb *pdata = to_fb_plat(ctrl->dev);
	struct fb_info *fb = ctrl->fb[id];
	struct fb_var_screeninfo *var = &fb->var;
	struct s3cfb_window *win = fb->par;
	u32 cfg;

	cfg = readl(ctrl->regs + S3C_WINCON(id));

	cfg &= ~(S3C_WINCON_BITSWP_ENABLE | S3C_WINCON_BYTESWP_ENABLE |
		S3C_WINCON_HAWSWP_ENABLE | S3C_WINCON_WSWP_ENABLE |
		S3C_WINCON_BURSTLEN_MASK | S3C_WINCON_BPPMODE_MASK |
		S3C_WINCON_INRGB_MASK | S3C_WINCON_DATAPATH_MASK);

	if (win->path != DATA_PATH_DMA) {
		dev_dbg(ctrl->dev, "[fb%d] data path: fifo\n", id);

		cfg |= S3C_WINCON_DATAPATH_LOCAL;

		if (win->path == DATA_PATH_FIFO) {
			cfg |= S3C_WINCON_INRGB_RGB;
			cfg |= S3C_WINCON_BPPMODE_24BPP_888;
		} else if (win->path == DATA_PATH_IPC) {
			cfg |= S3C_WINCON_INRGB_YUV;
			cfg |= S3C_WINCON_BPPMODE_24BPP_888;
		}

		if (id == 1) {
			cfg &= ~(S3C_WINCON1_LOCALSEL_MASK |
				S3C_WINCON1_VP_ENABLE);

			if (win->local_channel == 0) {
				cfg |= S3C_WINCON1_LOCALSEL_FIMC1;
			} else {
				cfg |= (S3C_WINCON1_LOCALSEL_VP |
					S3C_WINCON1_VP_ENABLE);
			}
		}
	} else {
		dev_dbg(ctrl->dev, "[fb%d] data path: dma\n", id);

		cfg |= S3C_WINCON_DATAPATH_DMA;

		if (fb->var.bits_per_pixel == 16 && pdata->swap & FB_SWAP_HWORD)
			cfg |= S3C_WINCON_HAWSWP_ENABLE;

		if (fb->var.bits_per_pixel == 32 && pdata->swap & FB_SWAP_WORD)
			cfg |= S3C_WINCON_WSWP_ENABLE;

		/* dma burst */
		if (win->dma_burst == 4)
			cfg |= S3C_WINCON_BURSTLEN_4WORD;
		else if (win->dma_burst == 8)
			cfg |= S3C_WINCON_BURSTLEN_8WORD;
		else
			cfg |= S3C_WINCON_BURSTLEN_16WORD;

		/* bpp mode set */
		switch (fb->var.bits_per_pixel) {
		case 16:
			if (var->transp.length == 1) {
				dev_dbg(ctrl->dev,
					"[fb%d] bpp mode: A1-R5-G5-B5\n", id);
				cfg |= S3C_WINCON_BPPMODE_16BPP_A555;
			} else if (var->transp.length == 4) {
				dev_dbg(ctrl->dev,
					"[fb%d] bpp mode: A4-R4-G4-B4\n", id);
				cfg |= S3C_WINCON_BPPMODE_16BPP_A444;
			} else {
				dev_dbg(ctrl->dev,
					"[fb%d] bpp mode: R5-G6-B5\n", id);
				cfg |= S3C_WINCON_BPPMODE_16BPP_565;
			}
			break;

		case 24: /* packed 24 bpp: nothing to do for 6.x fimd */
			break;

		case 32:
			if (var->transp.length == 0) {
				dev_dbg(ctrl->dev,
					"[fb%d] bpp mode: R8-G8-B8\n", id);
				cfg |= S3C_WINCON_BPPMODE_24BPP_888;
			} else {
				dev_dbg(ctrl->dev,
					"[fb%d] bpp mode: A8-R8-G8-B8\n", id);
				cfg |= S3C_WINCON_BPPMODE_32BPP;
			}
			break;
		}
	}

	writel(cfg, ctrl->regs + S3C_WINCON(id));

	return 0;
}

int s3cfb_set_buffer_address(struct s3cfb_global *ctrl, int id)
{
	struct fb_fix_screeninfo *fix = &ctrl->fb[id]->fix;
	struct fb_var_screeninfo *var = &ctrl->fb[id]->var;
	struct s3c_platform_fb *pdata = to_fb_plat(ctrl->dev);
	dma_addr_t start_addr = 0, end_addr = 0;
	u32 shw;

	if (fix->smem_start) {
		start_addr = fix->smem_start + (var->xres_virtual *
				(var->bits_per_pixel / 8) * var->yoffset);

		end_addr = start_addr + fix->line_length * var->yres;
	}

	if (pdata->hw_ver == 0x62) {
		shw = readl(ctrl->regs + S3C_WINSHMAP);
		shw |= S3C_WINSHMAP_PROTECT(id);
		writel(shw, ctrl->regs + S3C_WINSHMAP);
	}

	writel(start_addr, ctrl->regs + S3C_VIDADDR_START0(id));
	writel(end_addr, ctrl->regs + S3C_VIDADDR_END0(id));

	if (pdata->hw_ver == 0x62) {
		shw = readl(ctrl->regs + S3C_WINSHMAP);
		shw &= ~(S3C_WINSHMAP_PROTECT(id));
		writel(shw, ctrl->regs + S3C_WINSHMAP);
	}

	dev_dbg(ctrl->dev, "[fb%d] start_addr: 0x%08x, end_addr: 0x%08x\n",
		id, start_addr, end_addr);

	return 0;
}

int s3cfb_set_alpha_blending(struct s3cfb_global *ctrl, int id)
{
	struct s3cfb_window *win = ctrl->fb[id]->par;
	struct s3cfb_alpha *alpha = &win->alpha;
	u32 avalue = 0, cfg;

	if (id == 0) {
		dev_err(ctrl->dev, "[fb%d] does not support alpha blending\n",
			id);
		return -EINVAL;
	}

	cfg = readl(ctrl->regs + S3C_WINCON(id));
	cfg &= ~(S3C_WINCON_BLD_MASK | S3C_WINCON_ALPHA_SEL_MASK);

	if (alpha->mode == PIXEL_BLENDING) {
		dev_dbg(ctrl->dev, "[fb%d] alpha mode: pixel blending\n", id);

		/* fixing to DATA[31:24] for alpha value */
		cfg |= (S3C_WINCON_BLD_PIXEL | S3C_WINCON_ALPHA1_SEL);
	} else {
		dev_dbg(ctrl->dev, "[fb%d] alpha mode: plane %d blending\n",
			id, alpha->channel);

		cfg |= S3C_WINCON_BLD_PLANE;

		if (alpha->channel == 0) {
			cfg |= S3C_WINCON_ALPHA0_SEL;
			avalue = (alpha->value << S3C_VIDOSD_ALPHA0_SHIFT);
		} else {
			cfg |= S3C_WINCON_ALPHA1_SEL;
			avalue = (alpha->value << S3C_VIDOSD_ALPHA1_SHIFT);
		}
	}

	writel(cfg, ctrl->regs + S3C_WINCON(id));
	writel(avalue, ctrl->regs + S3C_VIDOSD_C(id));

	return 0;
}

int s3cfb_set_window_position(struct s3cfb_global *ctrl, int id)
{
	struct fb_var_screeninfo *var = &ctrl->fb[id]->var;
	struct s3cfb_window *win = ctrl->fb[id]->par;
	u32 cfg, shw;

	shw = readl(ctrl->regs + S3C_WINSHMAP);
	shw |= S3C_WINSHMAP_PROTECT(id);
	writel(shw, ctrl->regs + S3C_WINSHMAP);

	cfg = S3C_VIDOSD_LEFT_X(win->x) | S3C_VIDOSD_TOP_Y(win->y);
	writel(cfg, ctrl->regs + S3C_VIDOSD_A(id));

	cfg = S3C_VIDOSD_RIGHT_X(win->x + var->xres - 1) |
		S3C_VIDOSD_BOTTOM_Y(win->y + var->yres - 1);

	writel(cfg, ctrl->regs + S3C_VIDOSD_B(id));

	shw = readl(ctrl->regs + S3C_WINSHMAP);
	shw &= ~(S3C_WINSHMAP_PROTECT(id));
	writel(shw, ctrl->regs + S3C_WINSHMAP);

	dev_dbg(ctrl->dev, "[fb%d] offset: (%d, %d, %d, %d)\n", id,
		win->x, win->y, win->x + var->xres - 1, win->y + var->yres - 1);

	return 0;
}

int s3cfb_set_window_size(struct s3cfb_global *ctrl, int id)
{
	struct fb_var_screeninfo *var = &ctrl->fb[id]->var;
	u32 cfg;

	if (id > 2)
		return 0;

	cfg = S3C_VIDOSD_SIZE(var->xres * var->yres);

	if (id == 0)
		writel(cfg, ctrl->regs + S3C_VIDOSD_C(id));
	else
		writel(cfg, ctrl->regs + S3C_VIDOSD_D(id));

	dev_dbg(ctrl->dev, "[fb%d] resolution: %d x %d\n", id,
		var->xres, var->yres);

	return 0;
}

int s3cfb_set_buffer_size(struct s3cfb_global *ctrl, int id)
{
	struct fb_var_screeninfo *var = &ctrl->fb[id]->var;
	u32 offset = (var->xres_virtual - var->xres) * var->bits_per_pixel / 8;
	u32 cfg = 0;

	cfg = S3C_VIDADDR_PAGEWIDTH(var->xres * var->bits_per_pixel / 8);
	cfg |= S3C_VIDADDR_OFFSIZE(offset);

	writel(cfg, ctrl->regs + S3C_VIDADDR_SIZE(id));

	return 0;
}

int s3cfb_set_chroma_key(struct s3cfb_global *ctrl, int id)
{
	struct s3cfb_window *win = ctrl->fb[id]->par;
	struct s3cfb_chroma *chroma = &win->chroma;
	u32 cfg = 0;

	if (id == 0) {
		dev_err(ctrl->dev, "[fb%d] does not support chroma key\n", id);
		return -EINVAL;
	}

	cfg = (S3C_KEYCON0_KEYBLEN_DISABLE | S3C_KEYCON0_DIRCON_MATCH_FG);

	if (chroma->enabled)
		cfg |= S3C_KEYCON0_KEY_ENABLE;

	writel(cfg, ctrl->regs + S3C_KEYCON(id));

	cfg = S3C_KEYCON1_COLVAL(chroma->key);
	writel(cfg, ctrl->regs + S3C_KEYVAL(id));

	dev_dbg(ctrl->dev, "[fb%d] chroma key: 0x%08x, %s\n", id, cfg,
		chroma->enabled ? "enabled" : "disabled");

	return 0;
}

//SSCR xuhui 110130
/**
 * s3c_fb_set_gamma() - set gamma values.
 */
void s3cfb_set_gamma(struct s3cfb_global *ctrl)
{
#if S3C_FB_GAMMA_TBLINX
	void __iomem *regs = ctrl->regs;
	int idx;
	u32 gamma_val;

	for(idx = 0; idx < 64; idx += 2)
	{
		gamma_val = (gamma_tbl[S3C_FB_GAMMA_TBLINX - 1][idx]  & 0x7FC) | ((gamma_tbl[S3C_FB_GAMMA_TBLINX - 1][idx + 1] << 16) & 0x7FC0000);
		writel(gamma_val, regs + S3C_GAMMALUT + (idx * 2));
	}	
#endif
}

