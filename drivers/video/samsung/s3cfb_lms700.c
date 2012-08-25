/* linux/drivers/video/samsung/s3cfb_lms700.c
 *
 * MX100 LMS700(SMD) 7" SWVGA Display Panel Support
 *
 * iriver, Copyright (c) 2010 iriver
 * http://www.iriver.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "s3cfb.h"

//
// refer s3cfb.h
//

static struct s3cfb_lcd lms700 = {
/*
 * struct s3cfb_lcd
 * @width:		horizontal resolution
 * @height:		vertical resolution
 * @bpp:		bits per pixel
 * @freq:		vframe frequency
 * @timing:		timing values
 * @polarity:		polarity settings
 * @init_ldi:		pointer to LDI init function
 *
*/
	.width  = 1024,
	.height = 600,
	.bpp    = 32,
	.freq   = 70,             

/*
 * struct s3cfb_lcd_timing
 * @h_fp:	horizontal front porch
 * @h_bp:	horizontal back porch
 * @h_sw:	horizontal sync width
 * @v_fp:	vertical front porch
 * @v_fpe:	vertical front porch for even field
 * @v_bp:	vertical back porch
 * @v_bpe:	vertical back porch for even field
*/
	.timing = {
		.h_fp   = 36,             // Horizontal Front Porch
		.h_bp   = 60,             // Horizontal Back Porch
		.h_sw   = 30,             // HSYNC Low Width
		.v_fp   = 10,             // Vertical Front Porch
		.v_fpe  = 2,              // 1 or 2, OdroidT is 2
		.v_bp   = 11,             // Vertical Back Porch
		.v_bpe  = 2,              // 1 or 2, OdroidT is 2
		.v_sw   = 10,             // VSYNC Low Width
	},

/*
 * struct s3cfb_lcd_polarity
 * @rise_vclk:	if 1, video data is fetched at rising edge
 * @inv_hsync:	if HSYNC polarity is inversed
 * @inv_vsync:	if VSYNC polarity is inversed
 * @inv_vden:	if VDEN polarity is inversed
*/
    // same with odroid-t
	.polarity = {
		.rise_vclk  = 0,
		.inv_hsync  = 1,
		.inv_vsync  = 1,
		.inv_vden   = 0,
	},
};

/* name should be fixed as 's3cfb_set_lcd_info' */
void s3cfb_set_lcd_info(struct s3cfb_global *ctrl)
{
	lms700.init_ldi = NULL;
	ctrl->lcd = &lms700;
}

