/* linux/drivers/video/samsung/s3cfb_fimd6x.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
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

	vclk = PICOS2KHZ(ctrl->fb[pdata->default_win]->var.pixclock) * 1000;

	if (vclk > maxclk) {
		dev_info(ctrl->dev, "vclk(%d) should be smaller than %d\n",
			vclk, maxclk);
		/* vclk = maxclk; */
	}

	div = src_clk / vclk;

#if 0 // ktj disable to change VCLK
	if (src_clk % vclk)
		div++;
#endif

	printk(KERN_INFO "FIMD vclk = %d div = %d\n", vclk, div);

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
		start_addr = fix->smem_start + ((var->xres_virtual *
				var->yoffset + var->xoffset) *
				(var->bits_per_pixel / 8));

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

#if defined(CONFIG_MX100)
int s3cfb_set_vidcon3(struct s3cfb_global *ctrl, u32 reg)
{
    /* VIDCON3 0xF800_000C */
	u32 cfg;

	writel(reg, ctrl->regs + 0x0C);

	cfg = readl(ctrl->regs + 0x1C0);
    printk("read reg = 0x%8x \n", cfg);

	return 0;
}

int s3cfb_set_colorgaincon(struct s3cfb_global *ctrl, u32 reg)
{
	u32 cfg;

	writel(reg, ctrl->regs + 0x1C0);

	cfg = readl(ctrl->regs + 0x1C0);
    printk("read reg = 0x%8x \n", cfg);


	return 0;
}

int s3cfb_set_huecoef00(struct s3cfb_global *ctrl, u32 reg)
{
	u32 cfg;
	writel(reg, ctrl->regs + 0x1EC);

	cfg = readl(ctrl->regs + 0x1C0);
    printk("read reg = 0x%8x \n", cfg);

	return 0;
}

int s3cfb_set_huecoef01(struct s3cfb_global *ctrl, u32 reg)
{
	u32 cfg;
	writel(reg, ctrl->regs + 0x1F0);

	cfg = readl(ctrl->regs + 0x1C0);
    printk("read reg = 0x%8x \n", cfg);

	return 0;
}

int s3cfb_set_huecoef10(struct s3cfb_global *ctrl, u32 reg)
{
	u32 cfg;
	writel(reg, ctrl->regs + 0x1F4);

	cfg = readl(ctrl->regs + 0x1C0);
    printk("read reg = 0x%8x \n", cfg);

	return 0;
}

int s3cfb_set_huecoef11(struct s3cfb_global *ctrl, u32 reg)
{
	u32 cfg;
	writel(reg, ctrl->regs + 0x1F8);

	cfg = readl(ctrl->regs + 0x1C0);
    printk("read reg = 0x%8x \n", cfg);

	return 0;
}

int s3cfb_set_hueoffset(struct s3cfb_global *ctrl, u32 reg)
{
	u32 cfg;
	writel(reg, ctrl->regs + 0x1FC);

	cfg = readl(ctrl->regs + 0x1C0);
    printk("read reg = 0x%8x \n", cfg);

	return 0;
}

#endif

// GAMMALUT_01_00, 0xF800_037C

// 0.4 ~ 2.5, 0.1 step [0 ~ 21]

static u32 gammalut[][65] =
{
    // 0.4
{
  0,
 49,
 64,
 75,
 84,
 92,
 99,
106,
111,
117,
122,
127,
131,
135,
139,
143,
147,
151,
154,
157,
161,
164,
167,
170,
173,
176,
179,
181,
184,
187,
189,
192,
194,
196,
199,
201,
203,
206,
208,
210,
212,
214,
216,
218,
220,
222,
224,
226,
228,
230,
232,
234,
236,
237,
239,
241,
243,
244,
246,
248,
249,
251,
253,
254,
256

},

// 0.50
{
  0,
 32,
 45,
 55,
 64,
 72,
 78,
 85,
 91,
 96,
101,
106,
111,
115,
120,
124,
128,
132,
136,
139,
143,
147,
150,
153,
157,
160,
163,
166,
169,
172,
175,
178,
181,
184,
187,
189,
192,
195,
197,
200,
202,
205,
207,
210,
212,
215,
217,
219,
222,
224,
226,
229,
231,
233,
235,
237,
239,
242,
244,
246,
248,
250,
252,
254,
256,
},

// 0.60
{
  0,
 21,
 32,
 41,
 49,
 55,
 62,
 68,
 74,
 79,
 84,
 89,
 94,
 98,
103,
107,
111,
116,
120,
124,
127,
131,
135,
139,
142,
146,
149,
153,
156,
159,
162,
166,
169,
172,
175,
178,
181,
184,
187,
190,
193,
196,
199,
202,
204,
207,
210,
213,
215,
218,
221,
223,
226,
229,
231,
234,
236,
239,
241,
244,
246,
249,
251,
254,
256
},

/*
// 0.67
{
  0,
 16,
 25,
 33,
 40,
 46,
 52,
 58,
 64,
 69,
 74,
 79,
 83,
 88,
 92,
 97,
101,
105,
109,
113,
117,
121,
125,
129,
133,
136,
140,
144,
147,
151,
154,
158,
161,
164,
168,
171,
174,
177,
181,
184,
187,
190,
193,
196,
199,
202,
205,
208,
211,
214,
217,
220,
223,
226,
228,
231,
234,
237,
240,
242,
245,
248,
251,
253,
256
},
*/
// 0.7
{
0
,14
,23
,30
,37
,43
,49
,54
,60
,65
,70
,75
,79
,84
,88
,93
,97
,101
,105
,109
,113
,117
,121
,125
,129
,133
,136
,140
,144
,147
,151
,154
,158
,161
,164
,168
,171
,174
,178
,181
,184
,187
,191
,194
,197
,200
,203
,206
,209
,212
,215
,218
,221
,224
,227
,230
,233
,236
,239
,242
,245
,248
,250
,253
,256
    
},

// 0.80
{
0
,9
,16
,22
,28
,33
,39
,44
,49
,53
,58
,63
,67
,72
,76
,80
,84
,89
,93
,97
,101
,105
,109
,113
,117
,121
,125
,128
,132
,136
,140
,143
,147
,151
,154
,158
,162
,165
,169
,172
,176
,179
,183
,186
,190
,193
,197
,200
,203
,207
,210
,213
,217
,220
,223
,227
,230
,233
,237
,240
,243
,246
,250
,253
,256
    
},

// 0.9
{
0
,6
,11
,16
,21
,26
,30
,35
,39
,44
,48
,52
,57
,61
,65
,69
,74
,78
,82
,86
,90
,94
,98
,102
,106
,110
,114
,118
,122
,126
,129
,133
,137
,141
,145
,149
,153
,156
,160
,164
,168
,171
,175
,179
,183
,186
,190
,194
,198
,201
,205
,209
,212
,216
,220
,223
,227
,231
,234
,238
,242
,245
,249
,252
,256
    
},

// 1.00 [3]
{
  0,
  4,
  8,
 12,
 16,
 20,
 24,
 28,
 32,
 36,
 40,
 44,
 48,
 52,
 56,
 60,
 64,
 68, 
 72,
 76,
 80,
 84,
 88,
 92,
 96,
100,
104,
108,
112,
116,
120,
124,
128,
132,
136,
140,
144,
148,
152,
156,
160,
164,
168,
172,
176,
180,
184,
188,
192,
196,
200,
204,
208,
212,
216,
220,
224,
228,
232,
236,
240,
244,
248,
252,
256
},

// 1.1
{
0
,3
,6
,9
,12
,15
,19
,22
,26
,30
,33
,37
,41
,44
,48
,52
,56
,60
,63
,67
,71
,75
,79
,83
,87
,91
,95
,99
,103
,107
,111
,115
,119
,124
,128
,132
,136
,140
,144
,148
,153
,157
,161
,165
,170
,174
,178
,182
,187
,191
,195
,199
,204
,208
,212
,217
,221
,225
,230
,234
,238
,243
,247
,252
,256
    
},

// 1.2
{
0
,2
,4
,7
,9
,12
,15
,18
,21
,24
,28
,31
,34
,38
,41
,45
,49
,52
,56
,60
,63
,67
,71
,75
,79
,83
,87
,91
,95
,99
,103
,107
,111
,116
,120
,124
,128
,133
,137
,141
,146
,150
,154
,159
,163
,168
,172
,177
,181
,186
,190
,195
,200
,204
,209
,213
,218
,223
,227
,232
,237
,242
,246
,251
,256
    
},

// 1.3
{
0
,1
,3
,5
,7
,9
,12
,14
,17
,20
,23
,26
,29
,32
,35
,39
,42
,46
,49
,53
,56
,60
,64
,68
,72
,75
,79
,83
,87
,91
,96
,100
,104
,108
,112
,117
,121
,126
,130
,134
,139
,143
,148
,153
,157
,162
,167
,171
,176
,181
,186
,191
,195
,200
,205
,210
,215
,220
,225
,230
,235
,241
,246
,251
,256
    
},

// 1.4
{
0
,1
,2
,4
,5
,7
,9
,12
,14
,16
,19
,22
,25
,27
,30
,34
,37
,40
,43
,47
,50
,54
,57
,61
,65
,69
,73
,76
,80
,85
,89
,93
,97
,101
,106
,110
,114
,119
,123
,128
,133
,137
,142
,147
,152
,156
,161
,166
,171
,176
,181
,186
,191
,197
,202
,207
,212
,218
,223
,228
,234
,239
,245
,250
,256
    
},

// 1.50 [4]
{
  0,
  1,
  1,
  3,
  4,
  6,
  7,
  9,
 11,
 14,
 16,
 18,
 21,
 23,
 26,
 29,
 32,
 35,
 38,
 41,
 45,
 48,
 52,
 55,
 59,
 63,
 66,
 70,
 74,
 78,
 82,
 86,
 91,
 95,
 99,
104,
108,
113,
117,
122,
126,
131,
136,
141,
146,
151,
156,
161,
166,
172,
177,
182,
187,
193,
198,
204,
210,
215,
221,
227,
232,
238,
244,
250,
256
},

// 1.6
{
0
,0
,1
,2
,3
,4
,6
,7
,9
,11
,13
,15
,18
,20
,22
,25
,28
,31
,34
,37
,40
,43
,46
,50
,53
,57
,61
,64
,68
,72
,76
,80
,84
,89
,93
,97
,102
,107
,111
,116
,121
,126
,130
,135
,141
,146
,151
,156
,162
,167
,172
,178
,184
,189
,195
,201
,207
,213
,219
,225
,231
,237
,243
,250
,256
    
},

// 1.7
{
0
,0
,1
,1
,2
,3
,5
,6
,7
,9
,11
,13
,15
,17
,19
,22
,24
,27
,30
,32
,35
,39
,42
,45
,48
,52
,55
,59
,63
,67
,71
,75
,79
,83
,87
,92
,96
,101
,106
,110
,115
,120
,125
,130
,135
,141
,146
,151
,157
,163
,168
,174
,180
,186
,192
,198
,204
,210
,217
,223
,229
,236
,243
,249
,256
    
},

// 1.8
{
0
,0
,1
,1
,2
,3
,4
,5
,6
,7
,9
,11
,13
,15
,17
,19
,21
,24
,26
,29
,32
,34
,37
,41
,44
,47
,51
,54
,58
,62
,65
,69
,74
,78
,82
,86
,91
,95
,100
,105
,110
,115
,120
,125
,130
,136
,141
,147
,153
,158
,164
,170
,176
,182
,189
,195
,201
,208
,214
,221
,228
,235
,242
,249
,256
    
},

// 1.9
{
0
,0
,0
,1
,1
,2
,3
,4
,5
,6
,8
,9
,11
,12
,14
,16
,18
,21
,23
,25
,28
,31
,34
,37
,40
,43
,46
,50
,53
,57
,61
,65
,69
,73
,77
,81
,86
,90
,95
,100
,105
,110
,115
,120
,126
,131
,137
,142
,148
,154
,160
,166
,173
,179
,185
,192
,199
,205
,212
,219
,226
,234
,241
,248
,256
    
},

// 2.00 [5]
{
  0,
  0,
  0,
  1,
  1,
  2,
  2,
  3,
  4,
  5,
  6,
  8,
  9,
 11,
 12,
 14,
 16,
 18,
 20,
 23,
 25,
 28,
 30,
 33,
 36,
 39,
 42,
 46,
 49,
 53,
 56,
 60,
 64,
 68,
 72,
 77,
 81,
 86,
 90,
 95,
100,
105,
110,
116,
121,
127,
132,
138,
144,
150,
156,
163,
169,
176,
182,
189,
196,
203,
210,
218,
225,
233,
240,
248,
256
},    

// 2.1
{
0
,0
,0
,0
,1
,1
,2
,2
,3
,4
,5
,6
,8
,9
,11
,12
,14
,16
,18
,20
,22
,25
,27
,30
,33
,36
,39
,42
,45
,49
,52
,56
,60
,64
,68
,72
,76
,81
,86
,90
,95
,100
,106
,111
,117
,122
,128
,134
,140
,146
,152
,159
,166
,172
,179
,186
,193
,201
,208
,216
,224
,231
,239
,248
,256
    
},

// 2.2
{
0
,0
,0
,0
,1
,1
,1
,2
,3
,3
,4
,5
,6
,8
,9
,11
,12
,14
,16
,18
,20
,22
,24
,27
,30
,32
,35
,38
,42
,45
,48
,52
,56
,60
,64
,68
,72
,77
,81
,86
,91
,96
,101
,107
,112
,118
,124
,130
,136
,142
,149
,155
,162
,169
,176
,183
,191
,198
,206
,214
,222
,230
,239
,247
,256
    
},

// 2.3
{
0
,0
,0
,0
,0
,1
,1
,2
,2
,3
,4
,4
,5
,7
,8
,9
,11
,12
,14
,16
,18
,20
,22
,24
,27
,29
,32
,35
,38
,41
,45
,48
,52
,56
,60
,64
,68
,73
,77
,82
,87
,92
,97
,103
,108
,114
,120
,126
,132
,139
,145
,152
,159
,166
,173
,181
,188
,196
,204
,212
,221
,229
,238
,247
,256
    
},

// 2.4
{
0
,0
,0
,0
,0
,1
,1
,1
,2
,2
,3
,4
,5
,6
,7
,8
,9
,11
,12
,14
,16
,18
,20
,22
,24
,27
,29
,32
,35
,38
,42
,45
,49
,52
,56
,60
,64
,69
,73
,78
,83
,88
,93
,99
,104
,110
,116
,122
,128
,135
,142
,148
,156
,163
,170
,178
,186
,194
,202
,211
,219
,228
,237
,247
,256
    
},



// 2.50 [6]
{
  0,
  0,
  0,
  0,
  0,
  0,
  1,
  1,
  1,
  2,
  2,
  3,
  4,
  5,
  6,
  7,
  8,
  9,
 11,
 12,
 14,
 16,
 18,
 20,
 22,
 24,
 27,
 30,
 32,
 35,
 39,
 42,
 45,
 49,
 53,
 57,
 61,
 65,
 70,
 74,
 79,
 84,
 89,
 95,
100,
106,
112,
118,
125,
131,
138,
145,
152,
160,
167,
175,
183,
192,
200,
209,
218,
227,
236,
246,
256
}
    
};


int s3cfb_set_reg(struct s3cfb_global *ctrl, u32 reg, u32 data)
{
	u32 cfg;

	writel(data, ctrl->regs + reg);
/*        
    printk("write reg = 0x%x \n", data);

    msleep(5);

   	cfg = readl(ctrl->regs + reg);
        
    printk("read reg = 0x%x \n", cfg);
*/
	return 0;
}

int s3cfb_get_reg(struct s3cfb_global *ctrl, u32 reg)
{
	u32 cfg;

   	cfg = readl(ctrl->regs + reg);
        
    printk("read reg = 0x%x \n", cfg);

	return 0;
}

int s3cfb_set_gammalut(struct s3cfb_global *ctrl, u32 mode)
{
	int i;
	u32 cfg, lut_x, lut_y;
	u32 addr = 0x37c;
	
	for (i=0; i<32; i++)
	{
	    lut_x = gammalut[mode][(i*2)+1] << 18;   

	    lut_y = gammalut[mode][i*2] << 2;        
	    
	    cfg = lut_x | lut_y; 
	    
	    writel(cfg, ctrl->regs + addr);

//      printk("write reg : addr = 0x%04x  data = 0x%08x \n", addr, cfg);

        addr = addr + 4;
    }

    i = 64;

    cfg = gammalut[mode][i] << 2;   // lut_y

    writel(cfg, ctrl->regs + addr);

//  printk("write reg : addr = 0x%04x  data = 0x%08x \n", addr, cfg);

/*
    msleep(5);

    printk("--------------------------------------------\n");

	addr = 0x37c;
	
	for (i=0; i<33; i++)
    {
    	cfg = readl(ctrl->regs + addr);

        printk("read reg : addr = 0x%04x  data = 0x%08x \n", addr, cfg);

        addr = addr + 4;
    }
*/

	return 0;
}

int s3cfb_get_gammalut(struct s3cfb_global *ctrl, u32 mode)
{
	int i;
	u32 cfg, lut_x, lut_y, addr;
	
	addr = 0x37c;

	for (i=0; i<32; i++)
	{
    	cfg = readl(ctrl->regs + addr);
        
        lut_x = cfg >> 18;
    
        lut_y = (cfg >> 2) & 0x3ff;
        
        printk("addr = 0x%04x lut_y = %d lut_x = %d \n", addr, lut_y, lut_x);

        addr = addr + 4;
    }

  	cfg = readl(ctrl->regs + addr);

    lut_y = (cfg >> 2) & 0x3ff;

    printk("addr = 0x%04x lut_y = %d \n", addr, lut_y);

	return 0;
}


