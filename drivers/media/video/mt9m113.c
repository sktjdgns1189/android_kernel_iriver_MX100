/*
 * Driver for mt9m113 CMOS Image Sensor from Micron
 *
 * Copyright (C) 2008, Robert Jarzmik <robert.jarzmik@free.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/videodev2.h>
#include <linux/videodev2_samsung.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/log2.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include <media/v4l2-common.h>
#include <media/v4l2-chip-ident.h>
#include <media/soc_camera.h>

/*
 * mt9m113 and mt9m112 i2c address is 0x78 (depending on SAddr pin)
 * The platform has to define i2c_board_info and call i2c_register_board_info()
 */

/* mt9m113: Sensor register addresses */
#define MT9M113_CHIP_VERSION		0x000
#define MT9M113_ROW_START		0x001
#define MT9M113_COLUMN_START		0x002
#define MT9M113_WINDOW_HEIGHT		0x003
#define MT9M113_WINDOW_WIDTH		0x004
#define MT9M113_HORIZONTAL_BLANKING_B	0x005
#define MT9M113_VERTICAL_BLANKING_B	0x006
#define MT9M113_HORIZONTAL_BLANKING_A	0x007
#define MT9M113_VERTICAL_BLANKING_A	0x008
#define MT9M113_SHUTTER_WIDTH		0x009
#define MT9M113_ROW_SPEED		0x00a
#define MT9M113_EXTRA_DELAY		0x00b
#define MT9M113_SHUTTER_DELAY		0x00c
#define MT9M113_RESET			0x00d
#define MT9M113_READ_MODE_B		0x020
#define MT9M113_READ_MODE_A		0x021
#define MT9M113_FLASH_CONTROL		0x023
#define MT9M113_GREEN1_GAIN		0x02b
#define MT9M113_BLUE_GAIN		0x02c
#define MT9M113_RED_GAIN		0x02d
#define MT9M113_GREEN2_GAIN		0x02e
#define MT9M113_GLOBAL_GAIN		0x02f
#define MT9M113_CONTEXT_CONTROL		0x0c8
#define MT9M113_PAGE_MAP		0x0f0
#define MT9M113_BYTE_WISE_ADDR		0x0f1

#define MT9M113_RESET_SYNC_CHANGES	(1 << 15)
#define MT9M113_RESET_RESTART_BAD_FRAME	(1 << 9)
#define MT9M113_RESET_SHOW_BAD_FRAMES	(1 << 8)
#define MT9M113_RESET_RESET_SOC		(1 << 5)
#define MT9M113_RESET_OUTPUT_DISABLE	(1 << 4)
#define MT9M113_RESET_CHIP_ENABLE	(1 << 3)
#define MT9M113_RESET_ANALOG_STANDBY	(1 << 2)
#define MT9M113_RESET_RESTART_FRAME	(1 << 1)
#define MT9M113_RESET_RESET_MODE	(1 << 0)

#define MT9M113_RMB_MIRROR_COLS		(1 << 1)
#define MT9M113_RMB_MIRROR_ROWS		(1 << 0)
#define MT9M113_CTXT_CTRL_RESTART	(1 << 15)
#define MT9M113_CTXT_CTRL_DEFECTCOR_B	(1 << 12)
#define MT9M113_CTXT_CTRL_RESIZE_B	(1 << 10)
#define MT9M113_CTXT_CTRL_CTRL2_B	(1 << 9)
#define MT9M113_CTXT_CTRL_GAMMA_B	(1 << 8)
#define MT9M113_CTXT_CTRL_XENON_EN	(1 << 7)
#define MT9M113_CTXT_CTRL_READ_MODE_B	(1 << 3)
#define MT9M113_CTXT_CTRL_LED_FLASH_EN	(1 << 2)
#define MT9M113_CTXT_CTRL_VBLANK_SEL_B	(1 << 1)
#define MT9M113_CTXT_CTRL_HBLANK_SEL_B	(1 << 0)
/*
 * mt9m113: Colorpipe register addresses (0x100..0x1ff)
 */
#define MT9M113_OPER_MODE_CTRL		0x106
#define MT9M113_OUTPUT_FORMAT_CTRL	0x108
#define MT9M113_REDUCER_XZOOM_B		0x1a0
#define MT9M113_REDUCER_XSIZE_B		0x1a1
#define MT9M113_REDUCER_YZOOM_B		0x1a3
#define MT9M113_REDUCER_YSIZE_B		0x1a4
#define MT9M113_REDUCER_XZOOM_A		0x1a6
#define MT9M113_REDUCER_XSIZE_A		0x1a7
#define MT9M113_REDUCER_YZOOM_A		0x1a9
#define MT9M113_REDUCER_YSIZE_A		0x1aa

#define MT9M113_OUTPUT_FORMAT_CTRL2_A	0x13a
#define MT9M113_OUTPUT_FORMAT_CTRL2_B	0x19b

#define MT9M113_OPMODE_AUTOEXPO_EN	(1 << 14)
#define MT9M113_OPMODE_AUTOWHITEBAL_EN	(1 << 1)

#define MT9M113_OUTFMT_PROCESSED_BAYER	(1 << 14)
#define MT9M113_OUTFMT_BYPASS_IFP	(1 << 10)
#define MT9M113_OUTFMT_INV_PIX_CLOCK	(1 << 9)
#define MT9M113_OUTFMT_RGB		(1 << 8)
#define MT9M113_OUTFMT_RGB565		(0x0 << 6)
#define MT9M113_OUTFMT_RGB555		(0x1 << 6)
#define MT9M113_OUTFMT_RGB444x		(0x2 << 6)
#define MT9M113_OUTFMT_RGBx444		(0x3 << 6)
#define MT9M113_OUTFMT_TST_RAMP_OFF	(0x0 << 4)
#define MT9M113_OUTFMT_TST_RAMP_COL	(0x1 << 4)
#define MT9M113_OUTFMT_TST_RAMP_ROW	(0x2 << 4)
#define MT9M113_OUTFMT_TST_RAMP_FRAME	(0x3 << 4)
#define MT9M113_OUTFMT_SHIFT_3_UP	(1 << 3)
#define MT9M113_OUTFMT_AVG_CHROMA	(1 << 2)
#define MT9M113_OUTFMT_SWAP_YCbCr_C_Y	(1 << 1)
#define MT9M113_OUTFMT_SWAP_RGB_EVEN	(1 << 1)
#define MT9M113_OUTFMT_SWAP_YCbCr_Cb_Cr	(1 << 0)
/*
 * mt9m113: Camera control register addresses (0x200..0x2ff not implemented)
 */

#define reg_read(reg) mt9m113_reg_read(client, MT9M113_##reg)
#define reg_write(reg, val) mt9m113_reg_write(client, MT9M113_##reg, (val))
#define reg_set(reg, val) mt9m113_reg_set(client, MT9M113_##reg, (val))
#define reg_clear(reg, val) mt9m113_reg_clear(client, MT9M113_##reg, (val))

#define MT9M113_MIN_DARK_ROWS	8
#define MT9M113_MIN_DARK_COLS	24
#define MT9M113_MAX_HEIGHT	1024
#define MT9M113_MAX_WIDTH	1280

#define COL_FMT(_name, _depth, _fourcc, _colorspace) \
	{ .name = _name, .depth = _depth, .fourcc = _fourcc, \
	.colorspace = _colorspace }
#define RGB_FMT(_name, _depth, _fourcc) \
	COL_FMT(_name, _depth, _fourcc, V4L2_COLORSPACE_SRGB)
#define JPG_FMT(_name, _depth, _fourcc) \
	COL_FMT(_name, _depth, _fourcc, V4L2_COLORSPACE_JPEG)

static const struct soc_camera_data_format mt9m113_colour_formats[] = {
	JPG_FMT("CbYCrY 16 bit", 16, V4L2_PIX_FMT_UYVY),
	JPG_FMT("CrYCbY 16 bit", 16, V4L2_PIX_FMT_VYUY),
	JPG_FMT("YCbYCr 16 bit", 16, V4L2_PIX_FMT_YUYV),
	JPG_FMT("YCrYCb 16 bit", 16, V4L2_PIX_FMT_YVYU),
	RGB_FMT("RGB 565", 16, V4L2_PIX_FMT_RGB565),
	RGB_FMT("RGB 555", 16, V4L2_PIX_FMT_RGB555),
	RGB_FMT("Bayer (sRGB) 10 bit", 10, V4L2_PIX_FMT_SBGGR16),
	RGB_FMT("Bayer (sRGB) 8 bit", 8, V4L2_PIX_FMT_SBGGR8),
};

/*
 * mt9m111: Camera Bright
 */
#define MT9M113_BRIGHT_REDUCE2     1
#define MT9M113_BRIGHT_REDUCE1_5   2
#define MT9M113_BRIGHT_REDUCE1     3
#define MT9M113_BRIGHT_REDUCE0_5   4
#define MT9M113_BRIGHT_0           5
#define MT9M113_BRIGHT_INCREASE0_5 6
#define MT9M113_BRIGHT_INCREASE1   7
#define MT9M113_BRIGHT_INCREASE1_5 8
#define MT9M113_BRIGHT_INCREASE2   9
/*
 * mt9m111: Camera Capture Resolution
 */
#define MT9M113_MANUAL_CAPTURERESOLUTION_1280X1024 (1280*1024)
#define MT9M113_MANUAL_CAPTURERESOLUTION_1280X960  (1280*960)
#define MT9M113_MANUAL_CAPTURERESOLUTION_800X600   (800*600)
#define MT9M113_MANUAL_CAPTURERESOLUTION_640X512   (640*512)
#define MT9M113_MANUAL_CAPTURERESOLUTION_640X480   (640*480)
#define MT9M113_MANUAL_CAPTURERESOLUTION_352X288   (352*288)
/*
 * mt9m111: Camera Effect
 */
#define MT9M113_EFFECT_NONE     0
#define MT9M113_EFFECT_GRAY     1
#define MT9M113_EFFECT_NEGATIVE 2
#define MT9M113_EFFECT_SEPIA    3
#define MT9M113_EFFECT_SOLARIZE 4
/*
 * mt9m111: Camera SNAPSHOT/Movie Mode
 */
#define MT9M113_SHAPSHOT_MODE 1
#define MT9M113_MOVIE_MODE    2
/*
 * mt9m111: Camera Reflect
 */
#define MT9M113_REFLECT_NORMAL      1
#define MT9M113_REFLECT_MIRROR      2
#define MT9M113_REFLECT_FLIP        3
#define MT9M113_REFLECT_MIRROR_FLIP 4
/*
 * mt9p111: Camera White Balance
 */
#define MT9M113_WB_AUTO           1
#define MT9M113_WB_INCANDESCENT_A 2
#define MT9M113_WB_FLUORESCENT    3
#define MT9M113_WB_DAYLIGHT       4
#define MT9M113_WB_CLOUDY         5

struct mt9m113_reg_cfg
{
	u16 addr;
	u16 val;
};

struct mt9m113_reg_cfg mt9m113_pre_init_regs[]=
{
	
	{0x001c, 0x0001},	// Disable the firmware  
	{0x0018, 0x6008},	// power up imaging system  
	{0x0018, 0x6009},	// go back to  standby state
	{0x001c, 0x0000},	// Reset mcu for next 
	
};


struct mt9m113_reg_cfg mt9m113_init_regs0[]=
{
	
	{0x0016, 0x00FF},	// CLOCKS_CONTROL  
	{0x0018, 0x0028},	// STANDBY_CONTROL 
	
};

struct mt9m113_reg_cfg mt9m113_init_regs1[]=
{
	//[FD SETUP]								   
	{0x098C, 0x222D},	// MCU_ADDRESS			   
	{0x0990, 0x0067},	// MCU_DATA_0			   
	{0x098C, 0xA408},	// MCU_ADDRESS			   
	{0x0990, 0x0013},	// MCU_DATA_0			   
	{0x098C, 0xA409},	// MCU_ADDRESS			   
	{0x0990, 0x0015},	// MCU_DATA_0			   
	{0x098C, 0xA40A},	// MCU_ADDRESS			   
	{0x0990, 0x0017},	// MCU_DATA_0			   
	{0x098C, 0xA40B},	// MCU_ADDRESS			   
	{0x0990, 0x0019},	// MCU_DATA_0			   
	{0x098C, 0x2411},	// MCU_ADDRESS			   
	{0x0990, 0x0067},	// MCU_DATA_0			   
	{0x098C, 0x2413},	// MCU_ADDRESS			   
	{0x0990, 0x007C},	// MCU_DATA_0			   
	{0x098C, 0x2415},	// MCU_ADDRESS			   
	{0x0990, 0x0067},	// MCU_DATA_0			   
	{0x098C, 0x2417},	// MCU_ADDRESS			   
	{0x0990, 0x007C},	// MCU_DATA_0			   
	//[PLL on timing wizard]					   
	{0x0016, 0x00FF},	// CLOCKS_CONTROL		   
	{0x0018, 0x0008},	// STANDBY_CONTROL		   
	{0x001A, 0x0210},	// RESET_AND_MISC_CONTROL  

	//DELAY= 3	// 3ms
	
};

struct mt9m113_reg_cfg mt9m113_init_regs2[]=
{
	
	{0x0014, 0x2145},	// PLL_CONTROL	 
	{0x0014, 0x2145},	// PLL_CONTROL	 
	{0x0014, 0x2145},	// PLL_CONTROL	 
	{0x0010, 0x0114},	// PLL_DIVIDERS  
	{0x0012, 0x1FF1},	// PLL_P_DIVIDERS
	{0x0014, 0x2545},	// PLL_CONTROL	 
	{0x0014, 0x2547},	// PLL_CONTROL	 
	{0x0014, 0x3447},	// PLL_CONTROL	 
	//sleep 1ms
};


struct mt9m113_reg_cfg mt9m113_init_regs3[]=
{

	{0x0014, 0x3047},	// PLL_CONTROL			 
	{0x0014, 0x3046},	// PLL_CONTROL			 
	{0x001A, 0x0210},	// RESET_AND_MISC_CONTROL
	{0x0018, 0x0008},	// STANDBY_CONTROL		 
	{0x321C, 0x0003},	// OFIFO_CONTROL_STATUS  
	{0x098C, 0x2703},	// MCU_ADDRESS			 
	{0x0990, 0x0280},	// MCU_DATA_0			 
	{0x098C, 0x2705},	// MCU_ADDRESS			 
	{0x0990, 0x0200},	// MCU_DATA_0			 
	{0x098C, 0x2707},	// MCU_ADDRESS			 
	{0x0990, 0x0500},	// MCU_DATA_0			 
	{0x098C, 0x2709},	// MCU_ADDRESS			 
	{0x0990, 0x0400},	// MCU_DATA_0			 
	{0x098C, 0x270D},	// MCU_ADDRESS			 
	{0x0990, 0x0000},	// MCU_DATA_0			 
	{0x098C, 0x270F},	// MCU_ADDRESS			 
	{0x0990, 0x0000},	// MCU_DATA_0			 
	{0x098C, 0x2711},	// MCU_ADDRESS			 
	{0x0990, 0x040D},	// MCU_DATA_0			 
	{0x098C, 0x2713},	// MCU_ADDRESS			 
	{0x0990, 0x050D},	// MCU_DATA_0			 
	{0x098C, 0x2715},	// MCU_ADDRESS			 
	{0x0990, 0x2111},	// MCU_DATA_0			 
	{0x098C, 0x2717},	// MCU_ADDRESS			 
	{0x0990, 0x046C},	// MCU_DATA_0			 
	{0x098C, 0x2719},	// MCU_ADDRESS			 
	{0x0990, 0x00AC},	// MCU_DATA_0			 
	{0x098C, 0x271B},	// MCU_ADDRESS			 
	{0x0990, 0x01F1},	// MCU_DATA_0			 
	{0x098C, 0x271D},	// MCU_ADDRESS			 
	{0x0990, 0x013F},	// MCU_DATA_0			 
	{0x098C, 0x271F},	// MCU_ADDRESS			 
	{0x0990, 0x029E},	// MCU_DATA_0			 
	{0x098C, 0x2721},	// MCU_ADDRESS			 
	{0x0990, 0x0722},	// MCU_DATA_0			 
	{0x098C, 0x2723},	// MCU_ADDRESS			 
	{0x0990, 0x0004},	// MCU_DATA_0			 
	{0x098C, 0x2725},	// MCU_ADDRESS			 
	{0x0990, 0x0004},	// MCU_DATA_0			 
	{0x098C, 0x2727},	// MCU_ADDRESS			 
	{0x0990, 0x040B},	// MCU_DATA_0			 
	{0x098C, 0x2729},	// MCU_ADDRESS			 
	{0x0990, 0x050B},	// MCU_DATA_0			 
	{0x098C, 0x272B},	// MCU_ADDRESS			 
	{0x0990, 0x2111},	// MCU_DATA_0			 
	{0x098C, 0x272D},	// MCU_ADDRESS			 
	{0x0990, 0x0024},	// MCU_DATA_0			 
	{0x098C, 0x272F},	// MCU_ADDRESS			 
	{0x0990, 0x004C},	// MCU_DATA_0			 
	{0x098C, 0x2731},	// MCU_ADDRESS			 
	{0x0990, 0x00F9},	// MCU_DATA_0			 
	{0x098C, 0x2733},	// MCU_ADDRESS			 
	{0x0990, 0x00A7},	// MCU_DATA_0			 
	{0x098C, 0x2735},	// MCU_ADDRESS			 
	{0x0990, 0x0461},	// MCU_DATA_0			 
	{0x098C, 0x2737},	// MCU_ADDRESS			 
	{0x0990, 0x0722},	// MCU_DATA_0			 
	{0x098C, 0x2739},	// MCU_ADDRESS			 
	{0x0990, 0x0000},	// MCU_DATA_0			 
	{0x098C, 0x273B},	// MCU_ADDRESS			 
	{0x0990, 0x027F},	// MCU_DATA_0			 
	{0x098C, 0x273D},	// MCU_ADDRESS			 
	{0x0990, 0x0000},	// MCU_DATA_0			 
	{0x098C, 0x273F},	// MCU_ADDRESS			 
	{0x0990, 0x01FF},	// MCU_DATA_0			 
	{0x098C, 0x2747},	// MCU_ADDRESS			 
	{0x0990, 0x0000},	// MCU_DATA_0			 
	{0x098C, 0x2749},	// MCU_ADDRESS			 
	{0x0990, 0x04FF},	// MCU_DATA_0			 
	{0x098C, 0x274B},	// MCU_ADDRESS			 
	{0x0990, 0x0000},	// MCU_DATA_0			 
	{0x098C, 0x274D},	// MCU_ADDRESS			 
	{0x0990, 0x03FF},	// MCU_DATA_0			 
	{0x098C, 0x222D},	// MCU_ADDRESS			 
	{0x0990, 0x0089},	// MCU_DATA_0			 
	{0x098C, 0xA404},	// MCU_ADDRESS			 
	{0x0990, 0x0010},	// MCU_DATA_0			 
	{0x098C, 0xA408},	// MCU_ADDRESS			 
	{0x0990, 0x0021},	// MCU_DATA_0			 
	{0x098C, 0xA409},	// MCU_ADDRESS			 
	{0x0990, 0x0023},	// MCU_DATA_0			 
	{0x098C, 0xA40A},	// MCU_ADDRESS			 
	{0x0990, 0x0028},	// MCU_DATA_0			 
	{0x098C, 0xA40B},	// MCU_ADDRESS			 
	{0x0990, 0x002A},	// MCU_DATA_0			 
	{0x098C, 0x2411},	// MCU_ADDRESS			 
	{0x0990, 0x0089},	// MCU_DATA_0			 
	{0x098C, 0x2413},	// MCU_ADDRESS			 
	{0x0990, 0x00A4},	// MCU_DATA_0			 
	{0x098C, 0x2415},	// MCU_ADDRESS			 
	{0x0990, 0x0089},	// MCU_DATA_0			 
	{0x098C, 0x2417},	// MCU_ADDRESS			 
	{0x0990, 0x00A4},	// MCU_DATA_0			 
	{0x098C, 0xA40D},	// MCU_ADDRESS			 
	{0x0990, 0x0002},	// MCU_DATA_0			 
	{0x098C, 0xA40E},	// MCU_ADDRESS			 
	{0x0990, 0x0003},	// MCU_DATA_0			 
	{0x098C, 0xA410},	// MCU_ADDRESS			 
	{0x0990, 0x000A},	// MCU_DATA_0	
};

struct mt9m113_reg_cfg 	mt9m113_autoflick[]=
{

	//[auto flicker]													  
	{0x098C, 0xA11E},	// MCU_ADDRESS									  
	{0x0990, 0x0001},	// MCU_DATA_0									  
	{0x098C, 0xA404},	// MCU_ADDRESS									  
	{0x0990, 0x0000},	// MCU_DATA_0	
};

struct mt9m113_reg_cfg	mt9m113_autofast[]=
{

	//[ae fast] 														  
	{0x098C, 0xA109},	// MCU_ADDRESS									  
	{0x0990, 0x0004},	// MCU_DATA_0									  
	{0x098C, 0xA10A},	// MCU_ADDRESS									  
	{0x0990, 0x0001},	// MCU_DATA_0									  
	{0x098C, 0xA112},	// MCU_ADDRESS									  
	{0x0990, 0x000A},	// MCU_DATA_0
};

struct mt9m113_reg_cfg mt9m113_ll1[]=
{
	//[LL1] 															  
	{0x098C, 0xAB20},	// MCU_ADDRESS									  
	{0x0990, 0x0043},	// MCU_DATA_0									  
	{0x098C, 0xAB21},	// MCU_ADDRESS									  
	{0x0990, 0x0020},	// MCU_DATA_0									  
	{0x098C, 0xAB22},	// MCU_ADDRESS									  
	{0x0990, 0x0007},	// MCU_DATA_0									  
	{0x098C, 0xAB23},	// MCU_ADDRESS									  
	{0x0990, 0x0000},	// MCU_DATA_0	
};

struct mt9m113_reg_cfg	mt9m113_ll2[]=
{

	//[LL2] 															  
	{0x098C, 0xAB24},	// MCU_ADDRESS									  
	{0x0990, 0x000C},	// MCU_DATA_0									  
	{0x098C, 0xAB25},	// MCU_ADDRESS									  
	{0x0990, 0x002A},	// MCU_DATA_0									  
	{0x098C, 0xAB26},	// MCU_ADDRESS									  
	{0x0990, 0x0000},	// MCU_DATA_0									  
	{0x098C, 0xAB27},	// MCU_ADDRESS									  
	{0x0990, 0x0005},	// MCU_DATA_0	
};

struct mt9m113_reg_cfg	mt9m113_fw_pipe[] =
{
	//[FW_PIPE] 														  
	{0x098C, 0xAB2D},	// MCU_ADDRESS									  
	{0x0990, 0x002A},	// MCU_DATA_0									  
	{0x098C, 0xAB2F},	// MCU_ADDRESS									  
	{0x0990, 0x002E},	// MCU_DATA_0									  
	{0x098C, 0x2B28},	// MCU_ADDRESS									  
	{0x0990, 0x157C},	// MCU_DATA_0									  
	{0x098C, 0x2B2A},	// MCU_ADDRESS									  
	{0x0990, 0x32C8},	// MCU_DATA_0									  
	{0x098C, 0x2B38},	// MCU_ADDRESS									  
	{0x0990, 0x1F40},	// MCU_DATA_0									  
	{0x098C, 0x2B3A},	// MCU_ADDRESS									  
	{0x0990, 0x3A98},	// MCU_DATA_0									  
	{0x098C, 0x2257},	// MCU_ADDRESS									  
	{0x0990, 0x2710},	// MCU_DATA_0									  
	{0x098C, 0x2250},	// MCU_ADDRESS									  
	{0x0990, 0x1B58},	// MCU_DATA_0									  
	{0x098C, 0x2252},	// MCU_ADDRESS									  
	{0x0990, 0x32C8},	// MCU_DATA_0	
};

struct mt9m113_reg_cfg	mt9m113_lens_correction_4th[]=
{

	//[Lens Correction_4th] 											  
	{0x3658, 0x0310},	// P_RD_P0Q0									  
	{0x365A, 0xB9AD},	// P_RD_P0Q1									  
	{0x365C, 0x5090},	// P_RD_P0Q2									  
	{0x365E, 0x16CD},	// P_RD_P0Q3									  
	{0x3660, 0xCA8F},	// P_RD_P0Q4									  
	{0x3680, 0x56EA},	// P_RD_P1Q0									  
	{0x3682, 0xB0AA},	// P_RD_P1Q1									  
	{0x3684, 0x2FEC},	// P_RD_P1Q2									  
	{0x3686, 0x19ED},	// P_RD_P1Q3									  
	{0x3688, 0xB86E},	// P_RD_P1Q4									  
	{0x36A8, 0x20B0},	// P_RD_P2Q0									  
	{0x36AA, 0xB74E},	// P_RD_P2Q1									  
	{0x36AC, 0xCFD1},	// P_RD_P2Q2									  
	{0x36AE, 0xDF2E},	// P_RD_P2Q3									  
	{0x36B0, 0x14B3},	// P_RD_P2Q4									  
	{0x36D0, 0x39AD},	// P_RD_P3Q0									  
	{0x36D2, 0xE3ED},	// P_RD_P3Q1									  
	{0x36D4, 0xE2ED},	// P_RD_P3Q2									  
	{0x36D6, 0xCBEB},	// P_RD_P3Q3									  
	{0x36D8, 0x7B0E},	// P_RD_P3Q4									  
	{0x36F8, 0x980F},	// P_RD_P4Q0									  
	{0x36FA, 0x34AF},	// P_RD_P4Q1									  
	{0x36FC, 0x42B3},	// P_RD_P4Q2									  
	{0x36FE, 0x2B91},	// P_RD_P4Q3									  
	{0x3700, 0x81B5},	// P_RD_P4Q4									  
	{0x364E, 0x0370},	// P_GR_P0Q0									  
	{0x3650, 0x2B2E},	// P_GR_P0Q1									  
	{0x3652, 0x6B70},	// P_GR_P0Q2									  
	{0x3654, 0xFF4A},	// P_GR_P0Q3									  
	{0x3656, 0x9CF0},	// P_GR_P0Q4									  
	{0x3676, 0x87AB},	// P_GR_P1Q0									  
	{0x3678, 0x03AD},	// P_GR_P1Q1									  
	{0x367A, 0x474A},	// P_GR_P1Q2									  
	{0x367C, 0xBF4B},	// P_GR_P1Q3									  
	{0x367E, 0x8D2B},	// P_GR_P1Q4									  
	{0x369E, 0x0DF0},	// P_GR_P2Q0									  
	{0x36A0, 0x930E},	// P_GR_P2Q1									  
	{0x36A2, 0x8FD2},	// P_GR_P2Q2									  
	{0x36A4, 0xDB0E},	// P_GR_P2Q3									  
	{0x36A6, 0x21D3},	// P_GR_P2Q4									  
	{0x36C6, 0x370E},	// P_GR_P3Q0									  
	{0x36C8, 0x984E},	// P_GR_P3Q1									  
	{0x36CA, 0xF60E},	// P_GR_P3Q2									  
	{0x36CC, 0x438D},	// P_GR_P3Q3									  
	{0x36CE, 0x3B8E},	// P_GR_P3Q4									  
	{0x36EE, 0x81F0},	// P_GR_P4Q0									  
	{0x36F0, 0x7B30},	// P_GR_P4Q1									  
	{0x36F2, 0x5773},	// P_GR_P4Q2									  
	{0x36F4, 0x5CAE},	// P_GR_P4Q3									  
	{0x36F6, 0xF474},	// P_GR_P4Q4									  
	{0x3662, 0x0410},	// P_BL_P0Q0									  
	{0x3664, 0x2FEE},	// P_BL_P0Q1									  
	{0x3666, 0x3730},	// P_BL_P0Q2									  
	{0x3668, 0xFE8D},	// P_BL_P0Q3									  
	{0x366A, 0xE56F},	// P_BL_P0Q4									  
	{0x368A, 0x99EC},	// P_BL_P1Q0									  
	{0x368C, 0xD68B},	// P_BL_P1Q1									  
	{0x368E, 0xC6AD},	// P_BL_P1Q2									  
	{0x3690, 0x608B},	// P_BL_P1Q3									  
	{0x3692, 0x44ED},	// P_BL_P1Q4									  
	{0x36B2, 0x542F},	// P_BL_P2Q0									  
	{0x36B4, 0xC80D},	// P_BL_P2Q1									  
	{0x36B6, 0xA151},	// P_BL_P2Q2									  
	{0x36B8, 0xE9CE},	// P_BL_P2Q3									  
	{0x36BA, 0x6B72},	// P_BL_P2Q4									  
	{0x36DA, 0x4069},	// P_BL_P3Q0									  
	{0x36DC, 0xBE4E},	// P_BL_P3Q1									  
	{0x36DE, 0x6ECE},	// P_BL_P3Q2									  
	{0x36E0, 0x3D0E},	// P_BL_P3Q3									  
	{0x36E2, 0xA410},	// P_BL_P3Q4									  
	{0x3702, 0xEC8A},	// P_BL_P4Q0									  
	{0x3704, 0x2E50},	// P_BL_P4Q1									  
	{0x3706, 0x68D2},	// P_BL_P4Q2									  
	{0x3708, 0x43F1},	// P_BL_P4Q3									  
	{0x370A, 0x81B4},	// P_BL_P4Q4									  
	{0x366C, 0x0210},	// P_GB_P0Q0									  
	{0x366E, 0xD0ED},	// P_GB_P0Q1									  
	{0x3670, 0x4FD0},	// P_GB_P0Q2									  
	{0x3672, 0x52EB},	// P_GB_P0Q3									  
	{0x3674, 0x8BB0},	// P_GB_P0Q4									  
	{0x3694, 0xA86C},	// P_GB_P1Q0									  
	{0x3696, 0x494D},	// P_GB_P1Q1									  
	{0x3698, 0xE86B},	// P_GB_P1Q2									  
	{0x369A, 0xADAC},	// P_GB_P1Q3									  
	{0x369C, 0xF02A},	// P_GB_P1Q4									  
	{0x36BC, 0x12B0},	// P_GB_P2Q0									  
	{0x36BE, 0xCE8E},	// P_GB_P2Q1									  
	{0x36C0, 0x83B2},	// P_GB_P2Q2									  
	{0x36C2, 0x946E},	// P_GB_P2Q3									  
	{0x36C4, 0x1F73},	// P_GB_P2Q4									  
	{0x36E4, 0x544D},	// P_GB_P3Q0									  
	{0x36E6, 0xFA4E},	// P_GB_P3Q1									  
	{0x36E8, 0x3F8B},	// P_GB_P3Q2									  
	{0x36EA, 0x630E},	// P_GB_P3Q3									  
	{0x36EC, 0xAA4E},	// P_GB_P3Q4									  
	{0x370C, 0x8B90},	// P_GB_P4Q0									  
	{0x370E, 0x4330},	// P_GB_P4Q1									  
	{0x3710, 0x4573},	// P_GB_P4Q2									  
	{0x3712, 0x6B90},	// P_GB_P4Q3									  
	{0x3714, 0xECF4},	// P_GB_P4Q4									  
	{0x3644, 0x0284},	// POLY_ORIGIN_C								  
	{0x3642, 0x01F4},	// POLY_ORIGIN_R								  
	{0x3210, 0x01B8},	// COLOR_PIPELINE_CONTROL	
};

struct mt9m113_reg_cfg	mt9m113_gamma_5_045_125[]=
{
#if 0
	//[Gamma_5_0.45_1.25]												  
	{0x098C, 0xAB04},	// MCU_ADDRESS									  
	{0x0990, 0x0028},	// MCU_DATA_0									  
	{0x098C, 0xAB06},	// MCU_ADDRESS									  
	{0x0990, 0x0003},	// MCU_DATA_0									  
	{0x098C, 0xAB3C},	// MCU_ADDRESS									  
	{0x0990, 0x0000},	// MCU_DATA_0									  
	{0x098C, 0xAB3D},	// MCU_ADDRESS									  
	{0x0990, 0x000A},	// MCU_DATA_0									  
	{0x098C, 0xAB3E},	// MCU_ADDRESS									  
	{0x0990, 0x001D},	// MCU_DATA_0									  
	{0x098C, 0xAB3F},	// MCU_ADDRESS									  
	{0x0990, 0x0037},	// MCU_DATA_0									  
	{0x098C, 0xAB40},	// MCU_ADDRESS									  
	{0x0990, 0x0058},	// MCU_DATA_0									  
	{0x098C, 0xAB41},	// MCU_ADDRESS									  
	{0x0990, 0x0071},	// MCU_DATA_0									  
	{0x098C, 0xAB42},	// MCU_ADDRESS									  
	{0x0990, 0x0086},	// MCU_DATA_0									  
	{0x098C, 0xAB43},	// MCU_ADDRESS									  
	{0x0990, 0x0098},	// MCU_DATA_0									  
	{0x098C, 0xAB44},	// MCU_ADDRESS									  
	{0x0990, 0x00A7},	// MCU_DATA_0									  
	{0x098C, 0xAB45},	// MCU_ADDRESS									  
	{0x0990, 0x00B5},	// MCU_DATA_0									  
	{0x098C, 0xAB46},	// MCU_ADDRESS									  
	{0x0990, 0x00C0},	// MCU_DATA_0									  
	{0x098C, 0xAB47},	// MCU_ADDRESS									  
	{0x0990, 0x00CB},	// MCU_DATA_0									  
	{0x098C, 0xAB48},	// MCU_ADDRESS									  
	{0x0990, 0x00D4},	// MCU_DATA_0									  
	{0x098C, 0xAB49},	// MCU_ADDRESS									  
	{0x0990, 0x00DD},	// MCU_DATA_0									  
	{0x098C, 0xAB4A},	// MCU_ADDRESS									  
	{0x0990, 0x00E4},	// MCU_DATA_0									  
	{0x098C, 0xAB4B},	// MCU_ADDRESS									  
	{0x0990, 0x00EC},	// MCU_DATA_0		
#endif

#if 0
	/* based on A settings */
	{0x098C, 0xAB04},	// MCU_ADDRESS									  
	{0x0990, 0x0028},	// MCU_DATA_0									  
	{0x098C, 0xAB06},	// MCU_ADDRESS									  
	{0x0990, 0x0003},	// MCU_DATA_0									  
	{0x098C, 0xAB3C},	// MCU_ADDRESS									  
	{0x0990, 0x0000},	// MCU_DATA_0									  
	{0x098C, 0xAB3D},	// MCU_ADDRESS									  
	{0x0990, 0x0003},	// MCU_DATA_0									  
	{0x098C, 0xAB3E},	// MCU_ADDRESS									  
	{0x0990, 0x000B},	// MCU_DATA_0									  
	{0x098C, 0xAB3F},	// MCU_ADDRESS									  
	{0x0990, 0x0021},	// MCU_DATA_0									  
	{0x098C, 0xAB40},	// MCU_ADDRESS									  
	{0x0990, 0x0044},	// MCU_DATA_0									  
	{0x098C, 0xAB41},	// MCU_ADDRESS									  
	{0x0990, 0x005E},	// MCU_DATA_0									  
	{0x098C, 0xAB42},	// MCU_ADDRESS									  
	{0x0990, 0x0075},	// MCU_DATA_0									  
	{0x098C, 0xAB43},	// MCU_ADDRESS									  
	{0x0990, 0x008A},	// MCU_DATA_0									  
	{0x098C, 0xAB44},	// MCU_ADDRESS									  
	{0x0990, 0x009C},	// MCU_DATA_0									  
	{0x098C, 0xAB45},	// MCU_ADDRESS									  
	{0x0990, 0x00AC},	// MCU_DATA_0									  
	{0x098C, 0xAB46},	// MCU_ADDRESS									  
	{0x0990, 0x00B9},	// MCU_DATA_0									  
	{0x098C, 0xAB47},	// MCU_ADDRESS									  
	{0x0990, 0x00C5},	// MCU_DATA_0									  
	{0x098C, 0xAB48},	// MCU_ADDRESS									  
	{0x0990, 0x00CF},	// MCU_DATA_0									  
	{0x098C, 0xAB49},	// MCU_ADDRESS									  
	{0x0990, 0x00D9},	// MCU_DATA_0									  
	{0x098C, 0xAB4A},	// MCU_ADDRESS									  
	{0x0990, 0x00E2},	// MCU_DATA_0									  
	{0x098C, 0xAB4B},	// MCU_ADDRESS									  
	{0x0990, 0x00EA},	// MCU_DATA_0		
#else
	/* based on B settings */
	{0x098C, 0xAB04},	// MCU_ADDRESS									  
	{0x0990, 0x0028},	// MCU_DATA_0									  
	{0x098C, 0xAB06},	// MCU_ADDRESS									  
	{0x0990, 0x0003},	// MCU_DATA_0									  
	{0x098C, 0xAB3C},	// MCU_ADDRESS									  
	{0x0990, 0x0000},	// MCU_DATA_0									  
	{0x098C, 0xAB3D},	// MCU_ADDRESS									  
	{0x0990, 0x0005},	// MCU_DATA_0									  
	{0x098C, 0xAB3E},	// MCU_ADDRESS									  
	{0x0990, 0x000F},	// MCU_DATA_0									  
	{0x098C, 0xAB3F},	// MCU_ADDRESS									  
	{0x0990, 0x0029},	// MCU_DATA_0									  
	{0x098C, 0xAB40},	// MCU_ADDRESS									  
	{0x0990, 0x004A},	// MCU_DATA_0									  
	{0x098C, 0xAB41},	// MCU_ADDRESS									  
	{0x0990, 0x0063},	// MCU_DATA_0									  
	{0x098C, 0xAB42},	// MCU_ADDRESS									  
	{0x0990, 0x0078},	// MCU_DATA_0									  
	{0x098C, 0xAB43},	// MCU_ADDRESS									  
	{0x0990, 0x008B},	// MCU_DATA_0									  
	{0x098C, 0xAB44},	// MCU_ADDRESS									  
	{0x0990, 0x009C},	// MCU_DATA_0									  
	{0x098C, 0xAB45},	// MCU_ADDRESS									  
	{0x0990, 0x00AA},	// MCU_DATA_0									  
	{0x098C, 0xAB46},	// MCU_ADDRESS									  
	{0x0990, 0x00B7},	// MCU_DATA_0									  
	{0x098C, 0xAB47},	// MCU_ADDRESS									  
	{0x0990, 0x00C3},	// MCU_DATA_0									  
	{0x098C, 0xAB48},	// MCU_ADDRESS									  
	{0x0990, 0x00CD},	// MCU_DATA_0									  
	{0x098C, 0xAB49},	// MCU_ADDRESS									  
	{0x0990, 0x00D7},	// MCU_DATA_0									  
	{0x098C, 0xAB4A},	// MCU_ADDRESS									  
	{0x0990, 0x00E0},	// MCU_DATA_0									  
	{0x098C, 0xAB4B},	// MCU_ADDRESS									  
	{0x0990, 0x00E9},	// MCU_DATA_0		
#endif
	{0x098C, 0xAB4C},	// MCU_ADDRESS									  
	{0x0990, 0x00F3},	// MCU_DATA_0									  
	{0x098C, 0xAB4D},	// MCU_ADDRESS									  
	{0x0990, 0x00F9},	// MCU_DATA_0									  
	{0x098C, 0xAB4E},	// MCU_ADDRESS									  
	{0x0990, 0x00FF},	// MCU_DATA_0									  
	{0x098C, 0xAB4F},	// MCU_ADDRESS									  
	{0x0990, 0x0000},	// MCU_DATA_0									  
	{0x098C, 0xAB50},	// MCU_ADDRESS									  
	{0x0990, 0x0008},	// MCU_DATA_0									  
	{0x098C, 0xAB51},	// MCU_ADDRESS									  
	{0x0990, 0x0015},	// MCU_DATA_0									  
	{0x098C, 0xAB52},	// MCU_ADDRESS									  
	{0x0990, 0x0029},	// MCU_DATA_0									  
	{0x098C, 0xAB53},	// MCU_ADDRESS									  
	{0x0990, 0x0044},	// MCU_DATA_0									  
	{0x098C, 0xAB54},	// MCU_ADDRESS									  
	{0x0990, 0x0059},	// MCU_DATA_0									  
	{0x098C, 0xAB55},	// MCU_ADDRESS									  
	{0x0990, 0x006C},	// MCU_DATA_0									  
	{0x098C, 0xAB56},	// MCU_ADDRESS									  
	{0x0990, 0x007C},	// MCU_DATA_0									  
	{0x098C, 0xAB57},	// MCU_ADDRESS									  
	{0x0990, 0x008B},	// MCU_DATA_0									  
	{0x098C, 0xAB58},	// MCU_ADDRESS									  
	{0x0990, 0x0099},	// MCU_DATA_0									  
	{0x098C, 0xAB59},	// MCU_ADDRESS									  
	{0x0990, 0x00A7},	// MCU_DATA_0									  
	{0x098C, 0xAB5A},	// MCU_ADDRESS									  
	{0x0990, 0x00B3},	// MCU_DATA_0									  
	{0x098C, 0xAB5B},	// MCU_ADDRESS									  
	{0x0990, 0x00BF},	// MCU_DATA_0									  
	{0x098C, 0xAB5C},	// MCU_ADDRESS									  
	{0x0990, 0x00CB},	// MCU_DATA_0									  
	{0x098C, 0xAB5D},	// MCU_ADDRESS									  
	{0x0990, 0x00D6},	// MCU_DATA_0									  
	{0x098C, 0xAB5E},	// MCU_ADDRESS									  
	{0x0990, 0x00E1},	// MCU_DATA_0									  
	{0x098C, 0xAB5F},	// MCU_ADDRESS									  
	{0x0990, 0x00EB},	// MCU_DATA_0									  
	{0x098C, 0xAB60},	// MCU_ADDRESS									  
	{0x0990, 0x00F5},	// MCU_DATA_0									  
	{0x098C, 0xAB61},	// MCU_ADDRESS									  
	{0x0990, 0x00FF},	// MCU_DATA_0
};

struct mt9m113_reg_cfg	mt9m113_ae_target2[]=
{

	//[AE TARGET_2] 													  
	{0x098C, 0xA24F},	// MCU_ADDRESS									  
	{0x0990, 0x004B},	// MCU_DATA_0									  
	{0x098C, 0xA207},	// MCU_ADDRESS									  
	{0x0990, 0x000A},	// MCU_DATA_0									  
	//[AWB and CCMs_3rd]												  
	{0x098C, 0x2306},	// MCU_ADDRESS									  
	{0x0990, 0x00FC},	// MCU_DATA_0									  
	{0x098C, 0x2308},	// MCU_ADDRESS									  
	{0x0990, 0xFFAB},	// MCU_DATA_0									  
	{0x098C, 0x230A},	// MCU_ADDRESS									  
	{0x0990, 0x005D},	// MCU_DATA_0									  
	{0x098C, 0x230C},	// MCU_ADDRESS									  
	{0x0990, 0xFFC3},	// MCU_DATA_0									  
	{0x098C, 0x230E},	// MCU_ADDRESS									  
	{0x0990, 0x0151},	// MCU_DATA_0									  
	{0x098C, 0x2310},	// MCU_ADDRESS									  
	{0x0990, 0xFFF7},	// MCU_DATA_0									  
	{0x098C, 0x2312},	// MCU_ADDRESS									  
	{0x0990, 0x0014},	// MCU_DATA_0									  
	{0x098C, 0x2314},	// MCU_ADDRESS									  
	{0x0990, 0xFEF7},	// MCU_DATA_0									  
	{0x098C, 0x2316},	// MCU_ADDRESS									  
	{0x0990, 0x0205},	// MCU_DATA_0									  
	{0x098C, 0x2318},	// MCU_ADDRESS									  
	{0x0990, 0x0026},	// MCU_DATA_0									  
	{0x098C, 0x231A},	// MCU_ADDRESS									  
	{0x0990, 0x003E},	// MCU_DATA_0									  
	{0x098C, 0x231C},	// MCU_ADDRESS									  
	{0x0990, 0x0083},	// MCU_DATA_0									  
	{0x098C, 0x231E},	// MCU_ADDRESS									  
	{0x0990, 0xFFCA},	// MCU_DATA_0									  
	{0x098C, 0x2320},	// MCU_ADDRESS									  
	{0x0990, 0xFFB1},	// MCU_DATA_0									  
	{0x098C, 0x2322},	// MCU_ADDRESS									  
	{0x0990, 0x002F},	// MCU_DATA_0									  
	{0x098C, 0x2324},	// MCU_ADDRESS									  
	{0x0990, 0x003B},	// MCU_DATA_0									  
	{0x098C, 0x2326},	// MCU_ADDRESS									  
	{0x0990, 0xFF8E},	// MCU_DATA_0									  
	{0x098C, 0x2328},	// MCU_ADDRESS									  
	{0x0990, 0x002C},	// MCU_DATA_0									  
	{0x098C, 0x232A},	// MCU_ADDRESS									  
	{0x0990, 0x0092},	// MCU_DATA_0									  
	{0x098C, 0x232C},	// MCU_ADDRESS									  
	{0x0990, 0xFF32},	// MCU_DATA_0									  
	{0x098C, 0x232E},	// MCU_ADDRESS									  
	{0x0990, 0x000C},	// MCU_DATA_0									  
	{0x098C, 0x2330},	// MCU_ADDRESS									  
	{0x0990, 0xFFE9},	// MCU_DATA_0									  
	{0x098C, 0xA348},	// MCU_ADDRESS									  
	{0x0990, 0x0008},	// MCU_DATA_0									  
	{0x098C, 0xA349},	// MCU_ADDRESS									  
	{0x0990, 0x0002},	// MCU_DATA_0									  
	{0x098C, 0xA34A},	// MCU_ADDRESS									  
	{0x0990, 0x0059},	// MCU_DATA_0									  
	{0x098C, 0xA34B},	// MCU_ADDRESS									  
	{0x0990, 0x00C6},	// MCU_DATA_0									  
	{0x098C, 0xA351},	// MCU_ADDRESS									  
	{0x0990, 0x0000},	// MCU_DATA_0									  
	{0x098C, 0xA352},	// MCU_ADDRESS									  
	{0x0990, 0x007F},	// MCU_DATA_0									  
	{0x098C, 0xA354},	// MCU_ADDRESS									  
	{0x0990, 0x0043},	// MCU_DATA_0									  
	{0x098C, 0xA355},	// MCU_ADDRESS									  
	{0x0990, 0x0002},	// MCU_DATA_0									  
	{0x098C, 0xA35D},	// MCU_ADDRESS									  
	{0x0990, 0x0078},	// MCU_DATA_0									  
	{0x098C, 0xA35E},	// MCU_ADDRESS									  
	{0x0990, 0x0086},	// MCU_DATA_0									  
	{0x098C, 0xA35F},	// MCU_ADDRESS									  
	{0x0990, 0x007E},	// MCU_DATA_0									  
	{0x098C, 0xA360},	// MCU_ADDRESS									  
	{0x0990, 0x0082},	// MCU_DATA_0									  
	{0x098C, 0xA302},	// MCU_ADDRESS									  
	{0x0990, 0x0000},	// MCU_DATA_0									  
	{0x098C, 0xA303},	// MCU_ADDRESS									  
	{0x0990, 0x00EF},	// MCU_DATA_0									  
	{0x098C, 0xA363},	// MCU_ADDRESS									  
	{0x0990, 0x00CA},	// MCU_DATA_0									  
	{0x098C, 0xA364},	// MCU_ADDRESS									  
	{0x0990, 0x00F6},	// MCU_DATA_0									  
	{0x098C, 0xA366},	// MCU_ADDRESS									  
	{0x0990, 0x009A},	// MCU_DATA_0									  
	{0x098C, 0xA367},	// MCU_ADDRESS									  
	{0x0990, 0x0096},	// MCU_DATA_0									  
	{0x098C, 0xA368},	// MCU_ADDRESS									  
	{0x0990, 0x0098},	// MCU_DATA_0									  
	{0x098C, 0xA369},	// MCU_ADDRESS									  
	{0x0990, 0x0080},	// MCU_DATA_0									  
	{0x098C, 0xA36A},	// MCU_ADDRESS									  
	{0x0990, 0x007E},	// MCU_DATA_0									  
	{0x098C, 0xA36B},	// MCU_ADDRESS									  
	{0x0990, 0x0082},	// MCU_DATA_0									  
	{0x098C, 0xA36D},	// MCU_ADDRESS									  
	{0x0990, 0x0000},	// MCU_DATA_0									  
	{0x098C, 0xA36E},	// MCU_ADDRESS									  
	{0x0990, 0x0018},	// MCU_DATA_0
};

struct mt9m113_reg_cfg mt9m113_picfine[]=
{

	//[noise reduction] 												  
	{0x098C, 0xAB2C},	// MCU_ADDRESS									  
	{0x0990, 0x0006},	// MCU_DATA_0									  
	{0x098C, 0xAB2D},	// MCU_ADDRESS									  
	{0x0990, 0x000E},	// MCU_DATA_0									  
	{0x098C, 0xAB2E},	// MCU_ADDRESS									  
	{0x0990, 0x0006},	// MCU_DATA_0									  
	{0x098C, 0xAB2F},	// MCU_ADDRESS									  
	{0x0990, 0x0006},	// MCU_DATA_0									  
	{0x098C, 0xAB30},	// MCU_ADDRESS									  
	{0x0990, 0x001E},	// MCU_DATA_0									  
	{0x098C, 0xAB31},	// MCU_ADDRESS									  
	{0x0990, 0x0015},	// MCU_DATA_0									  
	{0x098C, 0xAB32},	// MCU_ADDRESS									  
	{0x0990, 0x001E},	// MCU_DATA_0									  
	{0x098C, 0xAB33},	// MCU_ADDRESS									  
	{0x0990, 0x001E},	// MCU_DATA_0									  
	{0x098C, 0xAB34},	// MCU_ADDRESS									  
	{0x0990, 0x0030},	// MCU_DATA_0									  
	{0x098C, 0xAB35},	// MCU_ADDRESS									  
	{0x0990, 0x004D},	// MCU_DATA_0									  
	//[ADD] 															  
	{0x098C, 0xA20E},	// MCU_ADDRESS									  
	{0x0990, 0x0080},	// MCU_DATA_0									  
	{0x098C, 0xA20D},	// MCU_ADDRESS									  
	{0x0990, 0x001A},	// MCU_DATA_0									  
	{0x098C, 0xA11D},	// MCU_ADDRESS									  
	{0x0990, 0x0002},	// MCU_DATA_0									  
	{0x098C, 0xA24A},	// MCU_ADDRESS									  
	{0x0990, 0x0040},	// MCU_DATA_0									  
	{0x098C, 0x275F},	// MCU_ADDRESS									  
	{0x0990, 0x0596},	// MCU_DATA_0									  
	{0x098C, 0x2761},	// MCU_ADDRESS									  
	{0x0990, 0x0094},	// MCU_DATA_0									  
	{0x33F4, 0x011B},	// KERNEL_CONFIG								  
	//MFD																  
	{0x098C, 0xA11E},	// MCU_ADDRESS									  
	{0x0990, 0x0002},	// MCU_DATA_0									  
	{0x098C, 0xA404},	// MCU_ADDRESS									  
	{0x0990, 0x0080},	// MCU_DATA_0									  
	//AE window size													  
	{0x098C, 0xA202},	// MCU_ADDRESS									  
	{0x0990, 0x0022},	// MCU_DATA_0									  
	{0x098C, 0xA203},	// MCU_ADDRESS									  
	{0x0990, 0x00BB},	// MCU_DATA_0									  
	// CONTEXT_B (1280*960) 											  
	{0x098C, 0x2747},	// MCU_ADDRESS [MODE_CROP_X0_B] 				  
	{0x0990, 0x0000},	// MCU_DATA_0									  
	{0x098C, 0x2749},	// MCU_ADDRESS [MODE_CROP_X1_B] 				  
	{0x0990, 0x04FF},	// MCU_DATA_0									  
	{0x098C, 0x274B},	// MCU_ADDRESS [MODE_CROP_Y0_B] 				  
	{0x0990, 0x0000},	// MCU_DATA_0									  
	{0x098C, 0x274D},	// MCU_ADDRESS [MODE_CROP_Y1_B] 				  
	{0x0990, 0x03FF},	// MCU_DATA_0									  
	{0x098C, 0x2707},	// MCU_ADDRESS [MODE_OUTPUT_WIDTH_B]			  
	{0x0990, 0x0500},	// MCU_DATA_0									  
	{0x098C, 0x2709},	// MCU_ADDRESS [MODE_OUTPUT_HEIGHT_B]			  
	{0x0990, 0x03C0},	// MCU_DATA_0									  
	//[REFRESH] 														  
	{0x098C, 0xA103},	// MCU_ADDRESS									  
	{0x0990, 0x0006},	// MCU_DATA_0

	//DELAY= 300	// 300ms	
};

struct mt9m113_reg_cfg	mt9m113_conterxb[]=
{

	// Context B														  
	{0x098C, 0xA115},	// MCU_ADDRESS [SEQ_CAP_MODE]					  
	{0x0990, 0x0002},	// MCU_DATA_0									  
	{0x098C, 0xA103},	// MCU_ADDRESS [SEQ_CMD]						  
	{0x0990, 0x0002},	// MCU_DATA_0									  
	//DELAY= 10															  
};										  
																		  
	// End Initialize													  
struct mt9m113_reg_cfg	mt9m113_capture_1280_1024[]=
{
																	  
	//[Capture_Resoluton_1280*1024]										  
	{0x098C, 0x2747},	// MCU_ADDRESS [MODE_CROP_X0_B] 				  
	{0x0990, 0x0000},	// MCU_DATA_0									  
	{0x098C, 0x2749},	// MCU_ADDRESS [MODE_CROP_X1_B] 				  
	{0x0990, 0x04FF},	// MCU_DATA_0									  
	{0x098C, 0x274B},	// MCU_ADDRESS [MODE_CROP_Y0_B] 				  
	{0x0990, 0x0000},	// MCU_DATA_0									  
	{0x098C, 0x274D},	// MCU_ADDRESS [MODE_CROP_Y1_B] 				  
	{0x0990, 0x03FF},	// MCU_DATA_0									  
	{0x098C, 0x2707},	// MCU_ADDRESS [MODE_OUTPUT_WIDTH_B]			  
	{0x0990, 0x0500},	// MCU_DATA_0									  
	{0x098C, 0x2709},	// MCU_ADDRESS [MODE_OUTPUT_HEIGHT_B]			  
	{0x0990, 0x0400},	// MCU_DATA_0									  
	{0x098C, 0xA103},	// MCU_ADDRESS [SEQ_CMD]						  
	{0x0990, 0x0005},	// MCU_DATA_0									  
	//DELAY= 200	// 200ms		
};

struct mt9m113_reg_cfg	mt9m113_preview_1280_960[]=
{
	//[Preview_Resoluton_1280*960]										  
	{0x098C, 0x2747},	// MCU_ADDRESS [MODE_CROP_X0_B] 				  
	{0x0990, 0x0000},	// MCU_DATA_0									  
	{0x098C, 0x2749},	// MCU_ADDRESS [MODE_CROP_X1_B] 				  
	{0x0990, 0x04FF},	// MCU_DATA_0									  
	{0x098C, 0x274B},	// MCU_ADDRESS [MODE_CROP_Y0_B] 				  
	{0x0990, 0x0000},	// MCU_DATA_0									  
	{0x098C, 0x274D},	// MCU_ADDRESS [MODE_CROP_Y1_B] 				  
	{0x0990, 0x03FF},	// MCU_DATA_0									  
	{0x098C, 0x2707},	// MCU_ADDRESS [MODE_OUTPUT_WIDTH_B]			  
	{0x0990, 0x0500},	// MCU_DATA_0									  
	{0x098C, 0x2709},	// MCU_ADDRESS [MODE_OUTPUT_HEIGHT_B]			  
	{0x0990, 0x03C0},	// MCU_DATA_0									  
	{0x098C, 0xA103},	// MCU_ADDRESS [SEQ_CMD]						  
	{0x0990, 0x0005},	// MCU_DATA_0									  
	//DELAY= 200	// 200ms
};

struct mt9m113_reg_cfg	mt9m113_ae_awb[]=
{

	// AE,AWB Enable													  
	{0x098C, 0xA102},	// MCU_ADDRESS [SEQ_MODE]						  
	{0x0990, 0x0001},	// MCU_DATA_0									  
	{0x098C, 0xA102},	// MCU_ADDRESS [SEQ_MODE]						  
	{0x0990, 0x0005},	// MCU_DATA_0
};

struct mt9m113_reg_cfg mt9m111_brightness_reduce2[]=
{
	{0x098C, 0xA24F}, 
	{0x0990, 0x0014}, 
	{0x098C, 0xA207}, 
	{0x0990, 0x0005}, 
	{0x098C, 0xA109}, 
	{0x0990, 0x0015}, 
	{0x098C, 0xA10A}, 
	{0x0990, 0x0002}, 
};

struct mt9m113_reg_cfg mt9m111_brightness_reduce1_5[]=
{
	{0x098C, 0xA24F}, 
	{0x0990, 0x001E}, 
	{0x098C, 0xA207}, 
	{0x0990, 0x0005}, 
	{0x098C, 0xA109}, 
	{0x0990, 0x0015}, 
	{0x098C, 0xA10A}, 
	{0x0990, 0x0002}, 
};

struct mt9m113_reg_cfg mt9m111_brightness_reduce1[]=
{
	{0x098C, 0xA24F}, 
	{0x0990, 0x0028}, 
	{0x098C, 0xA207}, 
	{0x0990, 0x0005}, 
	{0x098C, 0xA109}, 
	{0x0990, 0x0010}, 
	{0x098C, 0xA10A}, 
	{0x0990, 0x0002}, 
};

struct mt9m113_reg_cfg mt9m111_brightness_reduce0_5[]=
{
	{0x098C, 0xA24F}, 
	{0x0990, 0x0032}, 
	{0x098C, 0xA207}, 
	{0x0990, 0x0006}, 
	{0x098C, 0xA109}, 
	{0x0990, 0x0010}, 
	{0x098C, 0xA10A}, 
	{0x0990, 0x0002}, 
};

struct mt9m113_reg_cfg mt9m111_brightness_0[]=
{
	{0x098C, 0xA24F}, 
	{0x0990, 0x004B}, 
	{0x098C, 0xA207}, 
	{0x0990, 0x000A}, 
	{0x098C, 0xA109}, 
	{0x0990, 0x0004}, 
	{0x098C, 0xA10A}, 
	{0x0990, 0x0001}, 
};

struct mt9m113_reg_cfg mt9m111_brightness_increase0_5[]=
{
	{0x098C, 0xA24F}, 
	{0x0990, 0x005F}, 
	{0x098C, 0xA207}, 
	{0x0990, 0x000A}, 
	{0x098C, 0xA109}, 
	{0x0990, 0x0004}, 
	{0x098C, 0xA10A}, 
	{0x0990, 0x0001}, 
};

struct mt9m113_reg_cfg mt9m111_brightness_increase1[]=
{
	{0x098C, 0xA24F}, 
	{0x0990, 0x0073}, 
	{0x098C, 0xA207}, 
	{0x0990, 0x000A}, 
	{0x098C, 0xA109}, 
	{0x0990, 0x0004}, 
	{0x098C, 0xA10A}, 
	{0x0990, 0x0001}, 
};

struct mt9m113_reg_cfg mt9m111_brightness_increase1_5[]=
{
	{0x098C, 0xA24F}, 
	{0x0990, 0x0087}, 
	{0x098C, 0xA207}, 
	{0x0990, 0x000A}, 
	{0x098C, 0xA109}, 
	{0x0990, 0x0004}, 
	{0x098C, 0xA10A}, 
	{0x0990, 0x0001}, 
};

struct mt9m113_reg_cfg mt9m111_brightness_increase2[]=
{
	{0x098C, 0xA24F}, 
	{0x0990, 0x009B}, 
	{0x098C, 0xA207}, 
	{0x0990, 0x000A}, 
	{0x098C, 0xA109}, 
	{0x0990, 0x0004}, 
	{0x098C, 0xA10A}, 
	{0x0990, 0x0001}, 
};

struct mt9m113_reg_cfg mt9m113_manual_captureresolution_1280x1024[]=
{
	{0x098C, 0x2747}, // MCU_ADDRESS [MODE_CROP_X0_B]
	{0x0990, 0x0000}, // MCU_DATA_0
	{0x098C, 0x2749}, // MCU_ADDRESS [MODE_CROP_X1_B]
	{0x0990, 0x04FF}, // MCU_DATA_0
	{0x098C, 0x274B}, // MCU_ADDRESS [MODE_CROP_Y0_B]
	{0x0990, 0x0000}, // MCU_DATA_0
	{0x098C, 0x274D}, // MCU_ADDRESS [MODE_CROP_Y1_B]
	{0x0990, 0x03FF}, // MCU_DATA_0
	{0x098C, 0x2707}, // MCU_ADDRESS [MODE_OUTPUT_WIDTH_B]
	{0x0990, 0x0500}, // MCU_DATA_0
	{0x098C, 0x2709}, // MCU_ADDRESS [MODE_OUTPUT_HEIGHT_B]
	{0x0990, 0x0400}, // MCU_DATA_0
	{0x098C, 0xA103}, // MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0005}, // MCU_DATA_0
};

struct mt9m113_reg_cfg mt9m113_manual_captureresolution_1280x960[]=
{
	{0x098C, 0x2747}, // MCU_ADDRESS [MODE_CROP_X0_B]
	{0x0990, 0x0000}, // MCU_DATA_0
	{0x098C, 0x2749}, // MCU_ADDRESS [MODE_CROP_X1_B]
	{0x0990, 0x04FF}, // MCU_DATA_0
	{0x098C, 0x274B}, // MCU_ADDRESS [MODE_CROP_Y0_B]
	{0x0990, 0x0000}, // MCU_DATA_0
	{0x098C, 0x274D}, // MCU_ADDRESS [MODE_CROP_Y1_B]
	{0x0990, 0x03FF}, // MCU_DATA_0
	{0x098C, 0x2707}, // MCU_ADDRESS [MODE_OUTPUT_WIDTH_B]
	{0x0990, 0x0500}, // MCU_DATA_0
	{0x098C, 0x2709}, // MCU_ADDRESS [MODE_OUTPUT_HEIGHT_B]
	{0x0990, 0x03C0}, // MCU_DATA_0
	{0x098C, 0xA103}, // MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0005}, // MCU_DATA_0
};

struct mt9m113_reg_cfg mt9m113_manual_captureresolution_800x600[]=
{
	{0x098C, 0x2747}, // MCU_ADDRESS [MODE_CROP_X0_B]
	{0x0990, 0x0000}, // MCU_DATA_0
	{0x098C, 0x2749}, // MCU_ADDRESS [MODE_CROP_X1_B]
	{0x0990, 0x04FF}, // MCU_DATA_0
	{0x098C, 0x274B}, // MCU_ADDRESS [MODE_CROP_Y0_B]
	{0x0990, 0x0000}, // MCU_DATA_0
	{0x098C, 0x274D}, // MCU_ADDRESS [MODE_CROP_Y1_B]
	{0x0990, 0x03FF}, // MCU_DATA_0
	{0x098C, 0x2707}, // MCU_ADDRESS [MODE_OUTPUT_WIDTH_B]
	{0x0990, 0x0320}, // MCU_DATA_0
	{0x098C, 0x2709}, // MCU_ADDRESS [MODE_OUTPUT_HEIGHT_B]
	{0x0990, 0x0258}, // MCU_DATA_0
	{0x098C, 0xA103}, // MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0005}, // MCU_DATA_0
};

struct mt9m113_reg_cfg mt9m113_manual_captureresolution_640x512[]=
{
	{0x098C, 0x2747}, // MCU_ADDRESS [MODE_CROP_X0_B]
	{0x0990, 0x0000}, // MCU_DATA_0
	{0x098C, 0x2749}, // MCU_ADDRESS [MODE_CROP_X1_B]
	{0x0990, 0x04FF}, // MCU_DATA_0
	{0x098C, 0x274B}, // MCU_ADDRESS [MODE_CROP_Y0_B]
	{0x0990, 0x0000}, // MCU_DATA_0
	{0x098C, 0x274D}, // MCU_ADDRESS [MODE_CROP_Y1_B]
	{0x0990, 0x03FF}, // MCU_DATA_0
	{0x098C, 0x2707}, // MCU_ADDRESS [MODE_OUTPUT_WIDTH_B]
	{0x0990, 0x0280}, // MCU_DATA_0
	{0x098C, 0x2709}, // MCU_ADDRESS [MODE_OUTPUT_HEIGHT_B]
	{0x0990, 0x0200}, // MCU_DATA_0
	{0x098C, 0xA103}, // MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0005}, // MCU_DATA_0
};

struct mt9m113_reg_cfg mt9m113_manual_captureresolution_640x480[]=
{
	{0x098C, 0x2747}, // MCU_ADDRESS [MODE_CROP_X0_B]
	{0x0990, 0x0000}, // MCU_DATA_0
	{0x098C, 0x2749}, // MCU_ADDRESS [MODE_CROP_X1_B]
	{0x0990, 0x04FF}, // MCU_DATA_0
	{0x098C, 0x274B}, // MCU_ADDRESS [MODE_CROP_Y0_B]
	{0x0990, 0x0000}, // MCU_DATA_0
	{0x098C, 0x274D}, // MCU_ADDRESS [MODE_CROP_Y1_B]
	{0x0990, 0x03FF}, // MCU_DATA_0
	{0x098C, 0x2707}, // MCU_ADDRESS [MODE_OUTPUT_WIDTH_B]
	{0x0990, 0x0280}, // MCU_DATA_0
	{0x098C, 0x2709}, // MCU_ADDRESS [MODE_OUTPUT_HEIGHT_B]
	{0x0990, 0x01E0}, // MCU_DATA_0
	{0x098C, 0xA103}, // MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0005}, // MCU_DATA_0
};

struct mt9m113_reg_cfg mt9m113_manual_captureresolution_352x288[]=
{
	{0x098C, 0x2747}, // MCU_ADDRESS [MODE_CROP_X0_B]
	{0x0990, 0x0000}, // MCU_DATA_0
	{0x098C, 0x2749}, // MCU_ADDRESS [MODE_CROP_X1_B]
	{0x0990, 0x04FF}, // MCU_DATA_0
	{0x098C, 0x274B}, // MCU_ADDRESS [MODE_CROP_Y0_B]
	{0x0990, 0x0000}, // MCU_DATA_0
	{0x098C, 0x274D}, // MCU_ADDRESS [MODE_CROP_Y1_B]
	{0x0990, 0x03FF}, // MCU_DATA_0
	{0x098C, 0x2707}, // MCU_ADDRESS [MODE_OUTPUT_WIDTH_B]
	{0x0990, 0x0160}, // MCU_DATA_0
	{0x098C, 0x2709}, // MCU_ADDRESS [MODE_OUTPUT_HEIGHT_B]
	{0x0990, 0x0120}, // MCU_DATA_0
	{0x098C, 0xA103}, // MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0005}, // MCU_DATA_0
};

struct mt9m113_reg_cfg mt9m113_effect_reflect_wb[]=
{
	{0x098C, 0xA102}, // MCU_ADDRESS [SEQ_MODE]
	{0x0990, 0x0001}, // MCU_DATA_0
	{0x098C, 0xA102}, // MCU_ADDRESS [SEQ_MODE]
	{0x0990, 0x0005}, // MCU_DATA_0
	{0x098C, 0xA102}, // MCU_ADDRESS [SEQ_MODE]
	{0x0990, 0x000D}, // MCU_DATA_0
};

struct mt9m113_reg_cfg mt9m113_effect_none_1[]=
{
	{0x098C, 0x2759}, // MCU_ADDRESS [MODE_SPEC_EFFECTS_A]
	{0x0990, 0x6440}, // MCU_DATA_0
	{0x098C, 0x275B}, // MCU_ADDRESS [MODE_SPEC_EFFECTS_B]
	{0x0990, 0x6440}, // MCU_DATA_0
	{0x098C, 0xA103}, // MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0005}, // MCU_DATA_0
};

struct mt9m113_reg_cfg mt9m113_effect_gray_1[]=
{
	{0x098C, 0x2759}, // MCU_ADDRESS [MODE_SPEC_EFFECTS_A]
	{0x0990, 0x6441}, // MCU_DATA_0
	{0x098C, 0x275B}, // MCU_ADDRESS [MODE_SPEC_EFFECTS_B]
	{0x0990, 0x6441}, // MCU_DATA_0
	{0x098C, 0xA103}, // MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0005}, // MCU_DATA_0
};

struct mt9m113_reg_cfg mt9m113_effect_sepia[]=
{
	{0x098C, 0x2759}, // MCU_ADDRESS [MODE_SPEC_EFFECTS_A]
	{0x0990, 0x6442}, // MCU_DATA_0
	{0x098C, 0x275B}, // MCU_ADDRESS [MODE_SPEC_EFFECTS_B]
	{0x0990, 0x6442}, // MCU_DATA_0
	{0x098C, 0xA103}, // MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0005}, // MCU_DATA_0
};

struct mt9m113_reg_cfg mt9m113_effect_negative_1[]=
{
	{0x098C, 0x2759}, // MCU_ADDRESS [MODE_SPEC_EFFECTS_A]
	{0x0990, 0x6443}, // MCU_DATA_0
	{0x098C, 0x275B}, // MCU_ADDRESS [MODE_SPEC_EFFECTS_B]
	{0x0990, 0x6443}, // MCU_DATA_0
	{0x098C, 0xA103}, // MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0005}, // MCU_DATA_0
};

struct mt9m113_reg_cfg mt9m113_effect_solarize_1[]=
{
	{0x098C, 0x2759}, // MCU_ADDRESS [MODE_SPEC_EFFECTS_A]
	{0x0990, 0x6444}, // MCU_DATA_0
	{0x098C, 0x275B}, // MCU_ADDRESS [MODE_SPEC_EFFECTS_B]
	{0x0990, 0x6444}, // MCU_DATA_0
	{0x098C, 0xA103}, // MCU_ADDRESS [SEQ_CMD]
	{0x0990, 0x0005}, // MCU_DATA_0
};

struct mt9m113_reg_cfg mt9m113_shapshot_mode[]=
{
	// Variable 7.5fps ~ 14.7fps
	{0x098C,  0xA20C}, // MCU_ADDRESS [AE_MAX_INDEX]
	{0x0990,  0x0010}, // MCU_DATA_0
	{0x098C,  0xA214}, // MCU_ADDRESS [RESERVED_AE_14]
	{0x0990,  0x0020}, // MCU_DATA_0
};

struct mt9m113_reg_cfg mt9m113_movie_mode[]=
{
	// Fixed 14.7fps
	{0x098C,  0xA20C}, // MCU_ADDRESS [AE_MAX_INDEX]
	{0x0990,  0x0008}, // MCU_DATA_0
	{0x098C,  0xA214}, // MCU_ADDRESS [RESERVED_AE_14]
	{0x0990,  0x0022}, // MCU_DATA_0
};

struct mt9m113_reg_cfg mt9m113_reflect_normal[]=
{
	{0x098C,  0x2717}, // MCU_ADDRESS [MODE_SENSOR_READ_MODE_A]
	{0x0990,  0x046C}, // MCU_DATA_0
	{0x098C,  0x272D}, // MCU_ADDRESS [MODE_SENSOR_READ_MODE_B]
	{0x0990,  0x0024}, // MCU_DATA_0
	{0x098C,  0xA103}, // MCU_ADDRESS [SEQ_CMD]
	{0x0990,  0x0006}, // MCU_DATA_0
};

struct mt9m113_reg_cfg mt9m113_reflect_mirror[]=
{
	{0x098C,  0x2717}, // MCU_ADDRESS [MODE_SENSOR_READ_MODE_A]
	{0x0990,  0x046D}, // MCU_DATA_0
	{0x098C,  0x272D}, // MCU_ADDRESS [MODE_SENSOR_READ_MODE_B]
	{0x0990,  0x0025}, // MCU_DATA_0
	{0x098C,  0xA103}, // MCU_ADDRESS [SEQ_CMD]
	{0x0990,  0x0006}, // MCU_DATA_0
};

struct mt9m113_reg_cfg mt9m113_reflect_flip[]=
{
	{0x098C,  0x2717}, // MCU_ADDRESS [MODE_SENSOR_READ_MODE_A]
	{0x0990,  0x046E}, // MCU_DATA_0
	{0x098C,  0x272D}, // MCU_ADDRESS [MODE_SENSOR_READ_MODE_B]
	{0x0990,  0x0026}, // MCU_DATA_0
	{0x098C,  0xA103}, // MCU_ADDRESS [SEQ_CMD]
	{0x0990,  0x0006}, // MCU_DATA_0
};

struct mt9m113_reg_cfg mt9m113_reflect_mirror_flip[]=
{
	{0x098C,  0x2717}, // MCU_ADDRESS [MODE_SENSOR_READ_MODE_A]
	{0x0990,  0x046F}, // MCU_DATA_0
	{0x098C,  0x272D}, // MCU_ADDRESS [MODE_SENSOR_READ_MODE_B]
	{0x0990,  0x0027}, // MCU_DATA_0
	{0x098C,  0xA103}, // MCU_ADDRESS [SEQ_CMD]
	{0x0990,  0x0006}, // MCU_DATA_0
};

struct mt9m113_reg_cfg mt9m113_wb_auto[]=
{
	{0x098C,  0xA351}, // MCU_ADDRESS	Need to match with initialization
	{0x0990,  0x0000}, // AWB_CCM_POSITION_MIN
	{0x098C,  0xA352}, // MCU_ADDRESS
	{0x0990,  0x007F}, // AWB_CCM_POSITION_MAX
	{0x098C,  0xA34A}, // MCU_ADDRESS
	{0x0990,  0x0059}, // AWB_GAIN_MIN
	{0x098C,  0xA34B}, // MCU_ADDRESS
	{0x0990,  0x00A6}, // AWB_GAIN_MAX
	{0x098C,  0xA34C}, // MCU_ADDRESS
	{0x0990,  0x0059}, // AWB_GAINMIN_B
	{0x098C,  0xA34D}, // MCU_ADDRESS
	{0x0990,  0x00A6}, // AWB_GAINMAX_B
	{0x098C,  0xA102}, // MCU_ADDRESS [SEQ_MODE]
	{0x0990,  0x0005}, // MCU_DATA_0
};

struct mt9m113_reg_cfg mt9m113_wb_cloudy[]=
{
	{0x098C,  0xA351}, // CCMposition Min			
	{0x0990,  0x007F}, // MCU_DATA_0			        
	{0x098C,  0xA352}, // CCMposition Max			
	{0x0990,  0x007F}, // MCU_DATA_0			        
	{0x098C,  0xA353}, // CCMposition			    
	{0x0990,  0x007F}, // MCU_DATA_0	                
	{0x098C,  0xA34A}, // GainMin_R			        
	{0x0990,  0x00AF}, // MCU_DATA_0A4 -> A6		    
	{0x098C,  0xA34B}, // GainMax_R			        
	{0x0990,  0x00AF}, // MCU_DATA_0 A4 -> A6	    
	{0x098C,  0xA34C}, // GainMin_B			        
	{0x0990,  0x0079}, // MCU_DATA_0 			    
	{0x098C,  0xA34D}, // GainMax_B			        
	{0x0990,  0x0079}, // MCU_DATA_0	                
	{0x098C,  0xA102}, // MCU_ADDRESS [SEQ_MODE]    
	{0x0990,  0x0005}, // MCU_DATA_0                 
};   

struct mt9m113_reg_cfg mt9m113_wb_daylight[]=
{
	{0x098C,  0xA351}, // CCMposition Min			
	{0x0990,  0x007F}, // MCU_DATA_0			        
	{0x098C,  0xA352}, // CCMposition Max			
	{0x0990,  0x007F}, // MCU_DATA_0			        
	{0x098C,  0xA353}, // CCMposition			    
	{0x0990,  0x007F}, // MCU_DATA_0	                
	{0x098C,  0xA34A}, // GainMin_R			        
	{0x0990,  0x00A6}, // MCU_DATA_0A4 -> A6		    
	{0x098C,  0xA34B}, // GainMax_R			        
	{0x0990,  0x00A6}, // MCU_DATA_0 A4 -> A6	    
	{0x098C,  0xA34C}, // GainMin_B			        
	{0x0990,  0x007C}, // MCU_DATA_0 			    
	{0x098C,  0xA34D}, // GainMax_B			        
	{0x0990,  0x007C}, // MCU_DATA_0	                
	{0x098C,  0xA102}, // MCU_ADDRESS [SEQ_MODE]    
	{0x0990,  0x0005}, // MCU_DATA_0                 
};   

struct mt9m113_reg_cfg mt9m113_wb_fluorescent[]=
{
	{0x098C,  0xA351}, // CCMposition Min			
	{0x0990,  0x004E}, // MCU_DATA_0			        
	{0x098C,  0xA352}, // CCMposition Max			
	{0x0990,  0x004E}, // MCU_DATA_0			        
	{0x098C,  0xA353}, // CCMposition			    
	{0x0990,  0x004E}, // MCU_DATA_0	                
	{0x098C,  0xA34A}, // GainMin_R			        
	{0x0990,  0x009A}, // MCU_DATA_0A4 -> A6		    
	{0x098C,  0xA34B}, // GainMax_R			        
	{0x0990,  0x009A}, // MCU_DATA_0 A4 -> A6	    
	{0x098C,  0xA34C}, // GainMin_B			        
	{0x0990,  0x0084}, // MCU_DATA_0 			    
	{0x098C,  0xA34D}, // GainMax_B			        
	{0x0990,  0x0084}, // MCU_DATA_0	                
	{0x098C,  0xA102}, // MCU_ADDRESS [SEQ_MODE]    
	{0x0990,  0x0005}, // MCU_DATA_0                 
};   

struct mt9m113_reg_cfg mt9m113_wb_incandescent_a[]=
{
	{0x098C,  0xA351}, // CCMposition Min			
	{0x0990,  0x0003}, // MCU_DATA_0			        
	{0x098C,  0xA352}, // CCMposition Max			
	{0x0990,  0x0003}, // MCU_DATA_0			        
	{0x098C,  0xA353}, // CCMposition			    
	{0x0990,  0x0003}, // MCU_DATA_0	                
	{0x098C,  0xA34A}, // GainMin_R			        
	{0x0990,  0x00AD}, // MCU_DATA_0A4 -> A6		    
	{0x098C,  0xA34B}, // GainMax_R			        
	{0x0990,  0x00AD}, // MCU_DATA_0 A4 -> A6	    
	{0x098C,  0xA34C}, // GainMin_B			        
	{0x0990,  0x0081}, // MCU_DATA_0 			    
	{0x098C,  0xA34D}, // GainMax_B			        
	{0x0990,  0x0081}, // MCU_DATA_0	                
	{0x098C,  0xA102}, // MCU_ADDRESS [SEQ_MODE]    
	{0x0990,  0x0005}, // MCU_DATA_0                 
};   

/* Camera functional setting values configured by user concept */
struct mt9m113_userset {
	signed int exposure_bias;	/* V4L2_CID_EXPOSURE */
	unsigned int ae_lock;
	unsigned int awb_lock;
	unsigned int auto_wb;	/* V4L2_CID_AUTO_WHITE_BALANCE */
	unsigned int manual_wb;	/* V4L2_CID_WHITE_BALANCE_PRESET */
	unsigned int wb_temp;	/* V4L2_CID_WHITE_BALANCE_TEMPERATURE */
	unsigned int effect;	/* Color FX (AKA Color tone) */
	unsigned int contrast;	/* V4L2_CID_CONTRAST */
	unsigned int saturation;	/* V4L2_CID_SATURATION */
	unsigned int sharpness;		/* V4L2_CID_SHARPNESS */
	unsigned int glamour;
};



/*
 * S5K4BA register structure : 2bytes address, 2bytes value
 * retry on write failure up-to 5 times
 */

enum mt9m113_context {
	HIGHPOWER = 0,
	LOWPOWER,
};

struct mt9m113 {
	struct v4l2_subdev subdev;
	int model;	/* V4L2_IDENT_MT9M11x* codes from v4l2-chip-ident.h */
	enum mt9m113_context context;
	struct v4l2_rect rect;
	u32 pixfmt;
	unsigned int gain;
	unsigned char autoexposure;
	unsigned char datawidth;
	unsigned int powered:1;
	unsigned int hflip:1;
	unsigned int vflip:1;
	unsigned int swap_rgb_even_odd:1;
	unsigned int swap_rgb_red_blue:1;
	unsigned int swap_yuv_y_chromas:1;
	unsigned int swap_yuv_cb_cr:1;
	unsigned int autowhitebalance:1;
	struct mt9m113_userset userset;

	struct v4l2_pix_format pix;
	struct v4l2_fract timeperframe;
	int freq;	/* MCLK in KHz */
	int is_mipi;
	int isize;
	int ver;
	int fps;
};

extern int mt9m113_sensor_power(int on);

static inline struct mt9m113 *to_mt9m113_sd(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct mt9m113, subdev);
}


static struct mt9m113 *to_mt9m113(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client), struct mt9m113, subdev);
}

static int reg_page_map_set(struct i2c_client *client, const u16 reg)
{
	int ret;
	u16 page;
	static int lastpage = -1;	/* PageMap cache value */

	page = (reg >> 8);
	if (page == lastpage)
		return 0;
	if (page > 2)
		return -EINVAL;

	ret = i2c_smbus_write_word_data(client, MT9M113_PAGE_MAP, swab16(page));
	if (!ret)
		lastpage = page;
	return ret;
}

static int mt9m113_reg_read(struct i2c_client *client, const u16 reg)
{
#if 0
	int ret;

	ret = reg_page_map_set(client, reg);
	if (!ret)
		ret = swab16(i2c_smbus_read_word_data(client, reg & 0xff));

	dev_dbg(&client->dev, "read  reg.%03x -> %04x\n", reg, ret);
	return ret;
#endif
	__be16 buffer =0;
	int rc, val;
	u8 regaddr[2];
	regaddr[0]= reg >> 8;
	regaddr[1]= reg & 0xff;
	

	rc = i2c_master_send(client, regaddr, 2);
	if (rc != 2)
	{
		//printk( "i2c i/o error:  read send rc == %d (should be 2)\n", rc);
		//printk("reg=%x  %x %x \n", reg, regaddr[0], regaddr[1]);
	}

	//msleep(10);//Shanghai ewada mask for old I2C test code

	rc = i2c_master_recv(client, (char *)&buffer, 2);
	if (rc != 2)
	{
		//printk("i2c i/o error  in mt9m113_reg_read: rc == %d (should be 2)\n", rc);
	}

	val = be16_to_cpu(buffer);

	//printk("mt9v011: read 0x%04x = 0x%04x :0x%04x\n", reg, val, buffer );

	return val;
}

static int mt9m113_reg_write(struct i2c_client *client, const u16 reg,
			     const u16 data)
{
#if 0
	int ret;

	ret = reg_page_map_set(client, reg);
	if (!ret)
		ret = i2c_smbus_write_word_data(client, reg & 0xff,
						swab16(data));
	dev_dbg(&client->dev, "write reg.%03x = %04x -> %d\n", reg, data, ret);
	return ret;
#endif

	int rc; 
	int num = 4;
	u8 cmdbuf[4];
	cmdbuf[0]= reg >> 8;
	cmdbuf[1]= reg & 0xff;

	cmdbuf[2] = data >> 8;
	cmdbuf[3] = data & 0xff;

	rc = i2c_master_send(client, cmdbuf, num);
	if (rc != num)
	{
		printk("i2c i/o error in reg write : rc == %d (should be %d)\n", rc, num);
		printk("reg%x = %x \n", reg, data);
		printk("%02x :%02x :%02x :%02x", cmdbuf[0], cmdbuf[1], cmdbuf[2], cmdbuf[3]);
	}

	

	return !(num==rc);
}

static int mt9m113_reg_write_array(struct i2c_client *client, struct mt9m113_reg_cfg 	*p_array, int sizes)
{
	int i;
	struct mt9m113_reg_cfg 	*p_reg = p_array;
	for (i = 0; i < sizes; i++, p_reg++)
	{
		mt9m113_reg_write(client, p_reg->addr, p_reg->val);
	}
	
	return 0;
}

static int mt9m113_reg_set(struct i2c_client *client, const u16 reg,
			   const u16 data)
{
	int ret;

	ret = mt9m113_reg_read(client, reg);
	if (ret >= 0)
		ret = mt9m113_reg_write(client, reg, ret | data);
	return ret;
}

static int mt9m113_reg_clear(struct i2c_client *client, const u16 reg,
			     const u16 data)
{
	int ret;

	ret = mt9m113_reg_read(client, reg);
	return mt9m113_reg_write(client, reg, ret & ~data);
}

static int mt9m113_set_context(struct i2c_client *client,
			       enum mt9m113_context ctxt)
{
	int valB = MT9M113_CTXT_CTRL_RESTART | MT9M113_CTXT_CTRL_DEFECTCOR_B
		| MT9M113_CTXT_CTRL_RESIZE_B | MT9M113_CTXT_CTRL_CTRL2_B
		| MT9M113_CTXT_CTRL_GAMMA_B | MT9M113_CTXT_CTRL_READ_MODE_B
		| MT9M113_CTXT_CTRL_VBLANK_SEL_B
		| MT9M113_CTXT_CTRL_HBLANK_SEL_B;
	int valA = MT9M113_CTXT_CTRL_RESTART;

	if (ctxt == HIGHPOWER)
		return reg_write(CONTEXT_CONTROL, valB);
	else
		return reg_write(CONTEXT_CONTROL, valA);
}

static int mt9m113_setup_rect(struct i2c_client *client,
			      struct v4l2_rect *rect)
{
	struct mt9m113 *mt9m113 = to_mt9m113(client);
	int ret, is_raw_format;
	int width = rect->width;
	int height = rect->height;

	if (mt9m113->pixfmt == V4L2_PIX_FMT_SBGGR8 ||
	    mt9m113->pixfmt == V4L2_PIX_FMT_SBGGR16)
		is_raw_format = 1;
	else
		is_raw_format = 0;

	ret = reg_write(COLUMN_START, rect->left);
	if (!ret)
		ret = reg_write(ROW_START, rect->top);

	if (is_raw_format) {
		if (!ret)
			ret = reg_write(WINDOW_WIDTH, width);
		if (!ret)
			ret = reg_write(WINDOW_HEIGHT, height);
	} else {
		if (!ret)
			ret = reg_write(REDUCER_XZOOM_B, MT9M113_MAX_WIDTH);
		if (!ret)
			ret = reg_write(REDUCER_YZOOM_B, MT9M113_MAX_HEIGHT);
		if (!ret)
			ret = reg_write(REDUCER_XSIZE_B, width);
		if (!ret)
			ret = reg_write(REDUCER_YSIZE_B, height);
		if (!ret)
			ret = reg_write(REDUCER_XZOOM_A, MT9M113_MAX_WIDTH);
		if (!ret)
			ret = reg_write(REDUCER_YZOOM_A, MT9M113_MAX_HEIGHT);
		if (!ret)
			ret = reg_write(REDUCER_XSIZE_A, width);
		if (!ret)
			ret = reg_write(REDUCER_YSIZE_A, height);
	}

	return ret;
}

static int mt9m113_setup_pixfmt(struct i2c_client *client, u16 outfmt)
{
	int ret;

	ret = reg_write(OUTPUT_FORMAT_CTRL2_A, outfmt);
	if (!ret)
		ret = reg_write(OUTPUT_FORMAT_CTRL2_B, outfmt);
	return ret;
}

static int mt9m113_setfmt_bayer8(struct i2c_client *client)
{
	return mt9m113_setup_pixfmt(client, MT9M113_OUTFMT_PROCESSED_BAYER);
}

static int mt9m113_setfmt_bayer10(struct i2c_client *client)
{
	return mt9m113_setup_pixfmt(client, MT9M113_OUTFMT_BYPASS_IFP);
}

static int mt9m113_setfmt_rgb565(struct i2c_client *client)
{
	struct mt9m113 *mt9m113 = to_mt9m113(client);
	int val = 0;

	if (mt9m113->swap_rgb_red_blue)
		val |= MT9M113_OUTFMT_SWAP_YCbCr_Cb_Cr;
	if (mt9m113->swap_rgb_even_odd)
		val |= MT9M113_OUTFMT_SWAP_RGB_EVEN;
	val |= MT9M113_OUTFMT_RGB | MT9M113_OUTFMT_RGB565;

	return mt9m113_setup_pixfmt(client, val);
}

static int mt9m113_setfmt_rgb555(struct i2c_client *client)
{
	struct mt9m113 *mt9m113 = to_mt9m113(client);
	int val = 0;

	if (mt9m113->swap_rgb_red_blue)
		val |= MT9M113_OUTFMT_SWAP_YCbCr_Cb_Cr;
	if (mt9m113->swap_rgb_even_odd)
		val |= MT9M113_OUTFMT_SWAP_RGB_EVEN;
	val |= MT9M113_OUTFMT_RGB | MT9M113_OUTFMT_RGB555;

	return mt9m113_setup_pixfmt(client, val);
}

static int mt9m113_setfmt_yuv(struct i2c_client *client)
{
	struct mt9m113 *mt9m113 = to_mt9m113(client);
	int val = 0;

	if (mt9m113->swap_yuv_cb_cr)
		val |= MT9M113_OUTFMT_SWAP_YCbCr_Cb_Cr;
	if (mt9m113->swap_yuv_y_chromas)
		val |= MT9M113_OUTFMT_SWAP_YCbCr_C_Y;

	return mt9m113_setup_pixfmt(client, val);
}

static int mt9m113_leave_stanby(struct i2c_client *client)
{
	struct mt9m113 *mt9m113 = to_mt9m113(client);
	int ret;
	//int i;
	//int val;
#if 0
	struct mt9m113 *mt9m113 = to_mt9m113(client);
	int ret;

	ret = reg_set(RESET, MT9M113_RESET_CHIP_ENABLE);
	if (!ret)
		mt9m113->powered = 1;
#endif
	mt9m113_reg_write(client, 0x0018,  0x4008);
	msleep(10);
	mt9m113_reg_write(client, 0x0010,  0x0340);

	return ret;
}

static int mt9m113_enable(struct i2c_client *client)
{
	struct mt9m113 *mt9m113 = to_mt9m113(client);
	int ret = 0;
#if 0
	struct mt9m113 *mt9m113 = to_mt9m113(client);
	int ret;

	ret = reg_set(RESET, MT9M113_RESET_CHIP_ENABLE);
	if (!ret)
		mt9m113->powered = 1;
#endif
	
	return ret;
}

static int mt9m113_reset(struct i2c_client *client)
{
	int ret;

	ret = reg_set(RESET, MT9M113_RESET_RESET_MODE);
	if (!ret)
		ret = reg_set(RESET, MT9M113_RESET_RESET_SOC);
	if (!ret)
		ret = reg_clear(RESET, MT9M113_RESET_RESET_MODE
				| MT9M113_RESET_RESET_SOC);

	return ret;
}

static unsigned long mt9m113_query_bus_param(struct soc_camera_device *icd)
{
	struct soc_camera_link *icl = to_soc_camera_link(icd);
	unsigned long flags = SOCAM_MASTER | SOCAM_PCLK_SAMPLE_RISING |
		SOCAM_HSYNC_ACTIVE_HIGH | SOCAM_VSYNC_ACTIVE_HIGH |
		SOCAM_DATA_ACTIVE_HIGH | SOCAM_DATAWIDTH_8;
	printk("..........%s line=%d \n", __FUNCTION__, __LINE__);

	return soc_camera_apply_sensor_flags(icl, flags);
}

static int mt9m113_set_bus_param(struct soc_camera_device *icd, unsigned long f)
{
	return 0;
}

static int mt9m113_make_rect(struct i2c_client *client,
			     struct v4l2_rect *rect)
{
	struct mt9m113 *mt9m113 = to_mt9m113(client);

	if (mt9m113->pixfmt == V4L2_PIX_FMT_SBGGR8 ||
	    mt9m113->pixfmt == V4L2_PIX_FMT_SBGGR16) {
		/* Bayer format - even size lengths */
		rect->width	= ALIGN(rect->width, 2);
		rect->height	= ALIGN(rect->height, 2);
		/* Let the user play with the starting pixel */
	}
	printk("..........%s line=%d \n", __FUNCTION__, __LINE__);
	/* FIXME: the datasheet doesn't specify minimum sizes */
	soc_camera_limit_side(&rect->left, &rect->width,
		     MT9M113_MIN_DARK_COLS, 2, MT9M113_MAX_WIDTH);

	soc_camera_limit_side(&rect->top, &rect->height,
		     MT9M113_MIN_DARK_ROWS, 2, MT9M113_MAX_HEIGHT);

	return mt9m113_setup_rect(client, rect);
}

static int mt9m113_s_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	struct v4l2_rect rect = a->c;
	struct i2c_client *client = sd->priv;
	struct mt9m113 *mt9m113 = to_mt9m113(client);
	int ret;

	dev_dbg(&client->dev, "%s left=%d, top=%d, width=%d, height=%d\n",
		__func__, rect.left, rect.top, rect.width, rect.height);
	printk("..........%s line=%d \n", __FUNCTION__, __LINE__);

	ret = mt9m113_make_rect(client, &rect);
	if (!ret)
		mt9m113->rect = rect;
	return ret;
}

static int mt9m113_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	struct i2c_client *client = sd->priv;
	struct mt9m113 *mt9m113 = to_mt9m113(client);

	a->c	= mt9m113->rect;
	a->type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int mt9m113_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	a->bounds.left			= MT9M113_MIN_DARK_COLS;
	a->bounds.top			= MT9M113_MIN_DARK_ROWS;
	a->bounds.width			= MT9M113_MAX_WIDTH;
	a->bounds.height		= MT9M113_MAX_HEIGHT;
	a->defrect			= a->bounds;
	a->type				= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	return 0;
}

static int mt9m113_g_fmt(struct v4l2_subdev *sd, struct v4l2_format *f)
{
	struct i2c_client *client = sd->priv;
	struct mt9m113 *mt9m113 = to_mt9m113(client);
	struct v4l2_pix_format *pix = &f->fmt.pix;

	pix->width		= mt9m113->rect.width;
	pix->height		= mt9m113->rect.height;
	pix->pixelformat	= mt9m113->pixfmt;
	pix->field		= V4L2_FIELD_NONE;
	pix->colorspace		= V4L2_COLORSPACE_SRGB;

	return 0;
}

static int mt9m113_set_pixfmt(struct i2c_client *client, u32 pixfmt)
{
	struct mt9m113 *mt9m113 = to_mt9m113(client);
	int ret;

	switch (pixfmt) {
	case V4L2_PIX_FMT_SBGGR8:
		ret = mt9m113_setfmt_bayer8(client);
		break;
	case V4L2_PIX_FMT_SBGGR16:
		ret = mt9m113_setfmt_bayer10(client);
		break;
	case V4L2_PIX_FMT_RGB555:
		ret = mt9m113_setfmt_rgb555(client);
		break;
	case V4L2_PIX_FMT_RGB565:
		ret = mt9m113_setfmt_rgb565(client);
		break;
	case V4L2_PIX_FMT_UYVY:
		mt9m113->swap_yuv_y_chromas = 0;
		mt9m113->swap_yuv_cb_cr = 0;
		ret = mt9m113_setfmt_yuv(client);
		break;
	case V4L2_PIX_FMT_VYUY:
		mt9m113->swap_yuv_y_chromas = 0;
		mt9m113->swap_yuv_cb_cr = 1;
		ret = mt9m113_setfmt_yuv(client);
		break;
	case V4L2_PIX_FMT_YUYV:
		mt9m113->swap_yuv_y_chromas = 1;
		mt9m113->swap_yuv_cb_cr = 0;
		ret = mt9m113_setfmt_yuv(client);
		break;
	case V4L2_PIX_FMT_YVYU:
		mt9m113->swap_yuv_y_chromas = 1;
		mt9m113->swap_yuv_cb_cr = 1;
		ret = mt9m113_setfmt_yuv(client);
		break;
	default:
		dev_err(&client->dev, "Pixel format not handled : %x\n",
			pixfmt);
		ret = -EINVAL;
	}

	if (!ret)
		mt9m113->pixfmt = pixfmt;

	return ret;
}

static int mt9m113_s_fmt(struct v4l2_subdev *sd, struct v4l2_format *f)
{
	struct i2c_client *client = sd->priv;
	struct mt9m113 *mt9m113 = to_mt9m113(client);
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_rect rect = {
		.left	= mt9m113->rect.left,
		.top	= mt9m113->rect.top,
		.width	= pix->width,
		.height	= pix->height,
	};
	int ret;

	dev_dbg(&client->dev,
		"%s fmt=%x left=%d, top=%d, width=%d, height=%d\n", __func__,
		pix->pixelformat, rect.left, rect.top, rect.width, rect.height);
	
	printk("..........%s line=%d \n", __FUNCTION__, __LINE__);

	printk(" width=%d, height=%d\n",  rect.width, rect.height);
return 0;
	ret = mt9m113_make_rect(client, &rect);
	if (!ret)
		ret = mt9m113_set_pixfmt(client, pix->pixelformat);
	if (!ret)
		mt9m113->rect = rect;
	return ret;
}

static int mt9m113_try_fmt(struct v4l2_subdev *sd, struct v4l2_format *f)
{
	struct v4l2_pix_format *pix = &f->fmt.pix;
	bool bayer = pix->pixelformat == V4L2_PIX_FMT_SBGGR8 ||
		pix->pixelformat == V4L2_PIX_FMT_SBGGR16;

	/*
	 * With Bayer format enforce even side lengths, but let the user play
	 * with the starting pixel
	 */
	 printk("..........%s line=%d \n", __FUNCTION__, __LINE__);

	if (pix->height > MT9M113_MAX_HEIGHT)
		pix->height = MT9M113_MAX_HEIGHT;
	else if (pix->height < 2)
		pix->height = 2;
	else if (bayer)
		pix->height = ALIGN(pix->height, 2);

	if (pix->width > MT9M113_MAX_WIDTH)
		pix->width = MT9M113_MAX_WIDTH;
	else if (pix->width < 2)
		pix->width = 2;
	else if (bayer)
		pix->width = ALIGN(pix->width, 2);

	return 0;
}

static int mt9m113_g_chip_ident(struct v4l2_subdev *sd,
				struct v4l2_dbg_chip_ident *id)
{
	struct i2c_client *client = sd->priv;
	struct mt9m113 *mt9m113 = to_mt9m113(client);

	if (id->match.type != V4L2_CHIP_MATCH_I2C_ADDR)
		return -EINVAL;

	if (id->match.addr != client->addr)
		return -ENODEV;

	id->ident	= mt9m113->model;
	id->revision	= 0;

	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int mt9m113_g_register(struct v4l2_subdev *sd,
			      struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = sd->priv;
	int val;
	printk("..........%s line=%d \n", __FUNCTION__, __LINE__);

	if (reg->match.type != V4L2_CHIP_MATCH_I2C_ADDR || reg->reg > 0x2ff)
		return -EINVAL;
	if (reg->match.addr != client->addr)
		return -ENODEV;

	val = mt9m113_reg_read(client, reg->reg);
	reg->size = 2;
	reg->val = (u64)val;

	if (reg->val > 0xffff)
		return -EIO;

	return 0;
}

static int mt9m113_s_register(struct v4l2_subdev *sd,
			      struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = sd->priv;

	if (reg->match.type != V4L2_CHIP_MATCH_I2C_ADDR || reg->reg > 0x2ff)
		return -EINVAL;

	if (reg->match.addr != client->addr)
		return -ENODEV;

	if (mt9m113_reg_write(client, reg->reg, reg->val) < 0)
		return -EIO;

	return 0;
}
#endif

static const struct v4l2_queryctrl mt9m113_controls[] = {
	{
		.id		= V4L2_CID_VFLIP,
		.type		= V4L2_CTRL_TYPE_BOOLEAN,
		.name		= "Flip Verticaly",
		.minimum	= 0,
		.maximum	= 1,
		.step		= 1,
		.default_value	= 0,
	}, {
		.id		= V4L2_CID_HFLIP,
		.type		= V4L2_CTRL_TYPE_BOOLEAN,
		.name		= "Flip Horizontaly",
		.minimum	= 0,
		.maximum	= 1,
		.step		= 1,
		.default_value	= 0,
	}, {	/* gain = 1/32*val (=>gain=1 if val==32) */
		.id		= V4L2_CID_GAIN,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Gain",
		.minimum	= 0,
		.maximum	= 63 * 2 * 2,
		.step		= 1,
		.default_value	= 32,
		.flags		= V4L2_CTRL_FLAG_SLIDER,
	}, {
		.id		= V4L2_CID_EXPOSURE_AUTO,
		.type		= V4L2_CTRL_TYPE_BOOLEAN,
		.name		= "Auto Exposure",
		.minimum	= 0,
		.maximum	= 1,
		.step		= 1,
		.default_value	= 1,
	}
};

static int mt9m113_resume(struct soc_camera_device *icd);
static int mt9m113_suspend(struct soc_camera_device *icd, pm_message_t state);

static struct soc_camera_ops mt9m113_ops = {
	.suspend		= mt9m113_suspend,
	.resume			= mt9m113_resume,
	.query_bus_param	= mt9m113_query_bus_param,
	.set_bus_param		= mt9m113_set_bus_param,
	.controls		= mt9m113_controls,
	.num_controls		= ARRAY_SIZE(mt9m113_controls),
};

static int mt9m113_set_flip(struct i2c_client *client, int flip, int mask)
{
	struct mt9m113 *mt9m113 = to_mt9m113(client);
	int ret;

	if (mt9m113->context == HIGHPOWER) {
		if (flip)
			ret = reg_set(READ_MODE_B, mask);
		else
			ret = reg_clear(READ_MODE_B, mask);
	} else {
		if (flip)
			ret = reg_set(READ_MODE_A, mask);
		else
			ret = reg_clear(READ_MODE_A, mask);
	}

	return ret;
}

static int mt9m113_get_global_gain(struct i2c_client *client)
{
	int data;

	data = reg_read(GLOBAL_GAIN);
	if (data >= 0)
		return (data & 0x2f) * (1 << ((data >> 10) & 1)) *
			(1 << ((data >> 9) & 1));
	return data;
}

static int mt9m113_set_global_gain(struct i2c_client *client, int gain)
{
	struct mt9m113 *mt9m113 = to_mt9m113(client);
	u16 val;

	if (gain > 63 * 2 * 2)
		return -EINVAL;

	mt9m113->gain = gain;
	if ((gain >= 64 * 2) && (gain < 63 * 2 * 2))
		val = (1 << 10) | (1 << 9) | (gain / 4);
	else if ((gain >= 64) && (gain < 64 * 2))
		val = (1 << 9) | (gain / 2);
	else
		val = gain;

	return reg_write(GLOBAL_GAIN, val);
}

static int mt9m113_set_autoexposure(struct i2c_client *client, int on)
{
	struct mt9m113 *mt9m113 = to_mt9m113(client);
	int ret;

	if (on)
		ret = reg_set(OPER_MODE_CTRL, MT9M113_OPMODE_AUTOEXPO_EN);
	else
		ret = reg_clear(OPER_MODE_CTRL, MT9M113_OPMODE_AUTOEXPO_EN);

	if (!ret)
		mt9m113->autoexposure = on;

	return ret;
}

static int mt9m113_set_autowhitebalance(struct i2c_client *client, int on)
{
	struct mt9m113 *mt9m113 = to_mt9m113(client);
	int ret;

	if (on)
		ret = reg_set(OPER_MODE_CTRL, MT9M113_OPMODE_AUTOWHITEBAL_EN);
	else
		ret = reg_clear(OPER_MODE_CTRL, MT9M113_OPMODE_AUTOWHITEBAL_EN);

	if (!ret)
		mt9m113->autowhitebalance = on;

	return ret;
}

static int mt9m113_set_bright(struct i2c_client *client, int bright)
{
	int size;

	switch (bright)
	{
	case MT9M113_BRIGHT_REDUCE2:
		size = (sizeof(mt9m111_brightness_reduce2) / sizeof(mt9m111_brightness_reduce2[0]));
		mt9m113_reg_write_array(client, mt9m111_brightness_reduce2, size);
		break;
	case MT9M113_BRIGHT_REDUCE1_5:
		size = (sizeof(mt9m111_brightness_reduce1_5) / sizeof(mt9m111_brightness_reduce1_5[0]));
		mt9m113_reg_write_array(client, mt9m111_brightness_reduce1_5, size);
		break;
	case MT9M113_BRIGHT_REDUCE1:
		size = (sizeof(mt9m111_brightness_reduce1) / sizeof(mt9m111_brightness_reduce1[0]));
		mt9m113_reg_write_array(client, mt9m111_brightness_reduce1, size);
		break;
	case MT9M113_BRIGHT_REDUCE0_5:
		size = (sizeof(mt9m111_brightness_reduce0_5) / sizeof(mt9m111_brightness_reduce0_5[0]));
		mt9m113_reg_write_array(client, mt9m111_brightness_reduce0_5, size);
		break;
	case MT9M113_BRIGHT_0:
		size = (sizeof(mt9m111_brightness_0) / sizeof(mt9m111_brightness_0[0]));
		mt9m113_reg_write_array(client, mt9m111_brightness_0, size);
		break;
	case MT9M113_BRIGHT_INCREASE0_5:
		size = (sizeof(mt9m111_brightness_increase0_5) / sizeof(mt9m111_brightness_increase0_5[0]));
		mt9m113_reg_write_array(client, mt9m111_brightness_increase0_5, size);
		break;
	case MT9M113_BRIGHT_INCREASE1:
		size = (sizeof(mt9m111_brightness_increase1) / sizeof(mt9m111_brightness_increase1[0]));
		mt9m113_reg_write_array(client, mt9m111_brightness_increase1, size);
		break;
	case MT9M113_BRIGHT_INCREASE1_5:
		size = (sizeof(mt9m111_brightness_increase1_5) / sizeof(mt9m111_brightness_increase1_5[0]));
		mt9m113_reg_write_array(client, mt9m111_brightness_increase1_5, size);
		break;
	case MT9M113_BRIGHT_INCREASE2:
		size = (sizeof(mt9m111_brightness_increase2) / sizeof(mt9m111_brightness_increase2[0]));
		mt9m113_reg_write_array(client, mt9m111_brightness_increase2, size);
		break;
	}

	return 0;
}

//extern void fimc_cap_set_cam_source(int width, int height, int cap);

static int mt9m113_set_capture_resolution(struct i2c_client *client, int captureresolution)
{
	int size;

	switch (captureresolution)
	{
	case MT9M113_MANUAL_CAPTURERESOLUTION_1280X1024:
		size = (sizeof(mt9m113_manual_captureresolution_1280x1024) / sizeof(mt9m113_manual_captureresolution_1280x1024[0]));
		mt9m113_reg_write_array(client, mt9m113_manual_captureresolution_1280x1024, size);		
	//	fimc_cap_set_cam_source(1280, 1024, 1);
		break;
	case MT9M113_MANUAL_CAPTURERESOLUTION_1280X960:
		size = (sizeof(mt9m113_manual_captureresolution_1280x960) / sizeof(mt9m113_manual_captureresolution_1280x960[0]));
		mt9m113_reg_write_array(client, mt9m113_manual_captureresolution_1280x960, size);
	//	fimc_cap_set_cam_source(1280, 960, 1);
		break;
	case MT9M113_MANUAL_CAPTURERESOLUTION_800X600:
		size = (sizeof(mt9m113_manual_captureresolution_800x600) / sizeof(mt9m113_manual_captureresolution_800x600[0]));
		mt9m113_reg_write_array(client, mt9m113_manual_captureresolution_800x600, size);
	//	fimc_cap_set_cam_source(800, 600, 1);

		break;
	case MT9M113_MANUAL_CAPTURERESOLUTION_640X512:
		size = (sizeof(mt9m113_manual_captureresolution_640x512) / sizeof(mt9m113_manual_captureresolution_640x512[0]));
		mt9m113_reg_write_array(client, mt9m113_manual_captureresolution_640x512, size);
		break;
	case MT9M113_MANUAL_CAPTURERESOLUTION_640X480:
		size = (sizeof(mt9m113_manual_captureresolution_640x480) / sizeof(mt9m113_manual_captureresolution_640x480[0]));
		mt9m113_reg_write_array(client, mt9m113_manual_captureresolution_640x480, size);
		break;
	case MT9M113_MANUAL_CAPTURERESOLUTION_352X288:
		size = (sizeof(mt9m113_manual_captureresolution_352x288) / sizeof(mt9m113_manual_captureresolution_352x288[0]));
		mt9m113_reg_write_array(client, mt9m113_manual_captureresolution_352x288, size);
		break;
	}
	mdelay(200);

	return 0;
}

static int mt9p113_set_preview_mode(struct i2c_client *client, int capture)
{
	int size = 0;
	int i;
	int err = 0;

	if (capture == 0)
	{
		size = (sizeof(mt9m113_preview_1280_960) / sizeof(mt9m113_preview_1280_960[0]));
		for (i = 0; i < size; i++) 
		{
			err = mt9m113_reg_write(client, mt9m113_preview_1280_960[i].addr, mt9m113_preview_1280_960[i].val);
			if (err)
				v4l_info(client, "%s: register set failed\n", __func__);
		}
		msleep(200);
		size = (sizeof(mt9m113_ae_awb) / sizeof(mt9m113_ae_awb[0]));
		for (i = 0; i < size; i++)
		{
			err = mt9m113_reg_write(client, mt9m113_ae_awb[i].addr, mt9m113_ae_awb[i].val);
			if (err)
				v4l_info(client, "%s: register set failed\n", __func__);
		}
		msleep(200);
	}
	else 
	{
		
	}
		
	return 0;

}


static int mt9m113_set_effect(struct i2c_client *client, int effect)
{
	int size;

	switch (effect)
	{
	case MT9M113_EFFECT_NONE:
		size = (sizeof(mt9m113_effect_none_1) / sizeof(mt9m113_effect_none_1[0]));
		mt9m113_reg_write_array(client, mt9m113_effect_none_1, size);
		mdelay(300);
		size = (sizeof(mt9m113_effect_reflect_wb) / sizeof(mt9m113_effect_reflect_wb[0]));
		mt9m113_reg_write_array(client, mt9m113_effect_reflect_wb, size);
		break;
	case MT9M113_EFFECT_GRAY:
		size = (sizeof(mt9m113_effect_gray_1) / sizeof(mt9m113_effect_gray_1[0]));
		mt9m113_reg_write_array(client, mt9m113_effect_gray_1, size);
		mdelay(300);
		size = (sizeof(mt9m113_effect_reflect_wb) / sizeof(mt9m113_effect_reflect_wb[0]));
		mt9m113_reg_write_array(client, mt9m113_effect_reflect_wb, size);
		break;
	case MT9M113_EFFECT_SEPIA:
		size = (sizeof(mt9m113_effect_sepia) / sizeof(mt9m113_effect_sepia[0]));
		mt9m113_reg_write_array(client, mt9m113_effect_sepia, size);
		break;
	case MT9M113_EFFECT_NEGATIVE:
		size = (sizeof(mt9m113_effect_negative_1) / sizeof(mt9m113_effect_negative_1[0]));
		mt9m113_reg_write_array(client, mt9m113_effect_negative_1, size);
		mdelay(300);
		size = (sizeof(mt9m113_effect_reflect_wb) / sizeof(mt9m113_effect_reflect_wb[0]));
		mt9m113_reg_write_array(client, mt9m113_effect_reflect_wb, size);
		break;
	case MT9M113_EFFECT_SOLARIZE:
		size = (sizeof(mt9m113_effect_solarize_1) / sizeof(mt9m113_effect_solarize_1[0]));
		mt9m113_reg_write_array(client, mt9m113_effect_solarize_1, size);
		mdelay(300);
		size = (sizeof(mt9m113_effect_reflect_wb) / sizeof(mt9m113_effect_reflect_wb[0]));
		mt9m113_reg_write_array(client, mt9m113_effect_reflect_wb, size);
		break;
	}
	mdelay(200);

	return 0;
}

static int mt9m113_set_movie(struct i2c_client *client, int movie)
{
	int size;

	switch (movie)
	{
	case MT9M113_SHAPSHOT_MODE:
		size = (sizeof(mt9m113_shapshot_mode) / sizeof(mt9m113_shapshot_mode[0]));
		mt9m113_reg_write_array(client, mt9m113_shapshot_mode, size);
		break;
	case MT9M113_MOVIE_MODE:
		size = (sizeof(mt9m113_movie_mode) / sizeof(mt9m113_movie_mode[0]));
		mt9m113_reg_write_array(client, mt9m113_movie_mode, size);
		break;
	}
	mdelay(200);

	return 0;
}

static int mt9m113_set_reflect(struct i2c_client *client, int reflect)
{
	int size;

	switch (reflect)
	{
	case MT9M113_REFLECT_NORMAL:
		size = (sizeof(mt9m113_reflect_normal) / sizeof(mt9m113_reflect_normal[0]));
		mt9m113_reg_write_array(client, mt9m113_reflect_normal, size);
		mdelay(300);
		size = (sizeof(mt9m113_effect_reflect_wb) / sizeof(mt9m113_effect_reflect_wb[0]));
		mt9m113_reg_write_array(client, mt9m113_effect_reflect_wb, size);
		break;
	case MT9M113_REFLECT_MIRROR:
		size = (sizeof(mt9m113_reflect_mirror) / sizeof(mt9m113_reflect_mirror[0]));
		mt9m113_reg_write_array(client, mt9m113_reflect_mirror, size);
		mdelay(300);
		size = (sizeof(mt9m113_effect_reflect_wb) / sizeof(mt9m113_effect_reflect_wb[0]));
		mt9m113_reg_write_array(client, mt9m113_effect_reflect_wb, size);
		break;
	case MT9M113_REFLECT_FLIP:
		size = (sizeof(mt9m113_reflect_flip) / sizeof(mt9m113_reflect_flip[0]));
		mt9m113_reg_write_array(client, mt9m113_reflect_flip, size);
		mdelay(300);
		size = (sizeof(mt9m113_effect_reflect_wb) / sizeof(mt9m113_effect_reflect_wb[0]));
		mt9m113_reg_write_array(client, mt9m113_effect_reflect_wb, size);
		break;
	case MT9M113_REFLECT_MIRROR_FLIP:
		size = (sizeof(mt9m113_reflect_mirror_flip) / sizeof(mt9m113_reflect_mirror_flip[0]));
		mt9m113_reg_write_array(client, mt9m113_reflect_mirror_flip, size);
		mdelay(300);
		size = (sizeof(mt9m113_effect_reflect_wb) / sizeof(mt9m113_effect_reflect_wb[0]));
		mt9m113_reg_write_array(client, mt9m113_effect_reflect_wb, size);
		break;
	}
	mdelay(200);

	return 0;
}

static int mt9m113_set_wb(struct i2c_client *client, int whitebalance)
{
	int size;

	switch (whitebalance)
	{
	case MT9M113_WB_AUTO:
		size = (sizeof(mt9m113_wb_auto) / sizeof(mt9m113_wb_auto[0]));
		mt9m113_reg_write_array(client, mt9m113_wb_auto, size);
		mdelay(300);
		size = (sizeof(mt9m113_effect_reflect_wb) / sizeof(mt9m113_effect_reflect_wb[0]));
		mt9m113_reg_write_array(client, mt9m113_effect_reflect_wb, size);
		break;
	case MT9M113_WB_CLOUDY:
		size = (sizeof(mt9m113_wb_cloudy) / sizeof(mt9m113_wb_cloudy[0]));
		mt9m113_reg_write_array(client, mt9m113_wb_cloudy, size);
		mdelay(300);
		size = (sizeof(mt9m113_effect_reflect_wb) / sizeof(mt9m113_effect_reflect_wb[0]));
		mt9m113_reg_write_array(client, mt9m113_effect_reflect_wb, size);
		break;
	case MT9M113_WB_DAYLIGHT:
		size = (sizeof(mt9m113_wb_daylight) / sizeof(mt9m113_wb_daylight[0]));
		mt9m113_reg_write_array(client, mt9m113_wb_daylight, size);
		mdelay(300);
		size = (sizeof(mt9m113_effect_reflect_wb) / sizeof(mt9m113_effect_reflect_wb[0]));
		mt9m113_reg_write_array(client, mt9m113_effect_reflect_wb, size);
		break;
	case MT9M113_WB_FLUORESCENT:
		size = (sizeof(mt9m113_wb_fluorescent) / sizeof(mt9m113_wb_fluorescent[0]));
		mt9m113_reg_write_array(client, mt9m113_wb_fluorescent, size);
		mdelay(300);
		size = (sizeof(mt9m113_effect_reflect_wb) / sizeof(mt9m113_effect_reflect_wb[0]));
		mt9m113_reg_write_array(client, mt9m113_effect_reflect_wb, size);
		break;
	case MT9M113_WB_INCANDESCENT_A:
		size = (sizeof(mt9m113_wb_incandescent_a) / sizeof(mt9m113_wb_incandescent_a[0]));
		mt9m113_reg_write_array(client, mt9m113_wb_incandescent_a, size);
		mdelay(300);
		size = (sizeof(mt9m113_effect_reflect_wb) / sizeof(mt9m113_effect_reflect_wb[0]));
		mt9m113_reg_write_array(client, mt9m113_effect_reflect_wb, size);
		break;
	}
	mdelay(200);

	return 0;
}


static int mt9m113_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = sd->priv;
	struct mt9m113 *mt9m113 = to_mt9m113(client);
	int data;
printk("..........%s line=%d V4L2_CID_ =%d ctrl->id=%d\n", __FUNCTION__, __LINE__, V4L2_CID_BASE, ctrl->id);

return 0;
	switch (ctrl->id) {
	case V4L2_CID_VFLIP:
		if (mt9m113->context == HIGHPOWER)
			data = reg_read(READ_MODE_B);
		else
			data = reg_read(READ_MODE_A);

		if (data < 0)
			return -EIO;
		ctrl->value = !!(data & MT9M113_RMB_MIRROR_ROWS);
		break;
	case V4L2_CID_HFLIP:
		if (mt9m113->context == HIGHPOWER)
			data = reg_read(READ_MODE_B);
		else
			data = reg_read(READ_MODE_A);

		if (data < 0)
			return -EIO;
		ctrl->value = !!(data & MT9M113_RMB_MIRROR_COLS);
		break;
	case V4L2_CID_GAIN:
		data = mt9m113_get_global_gain(client);
		if (data < 0)
			return data;
		ctrl->value = data;
		break;
	case V4L2_CID_EXPOSURE_AUTO:
		ctrl->value = mt9m113->autoexposure;
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
		ctrl->value = mt9m113->autowhitebalance;
		break;
	}
	return 0;
}

static int mt9m113_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = sd->priv;
	struct mt9m113 *mt9m113 = to_mt9m113(client);
	int ret;
	int value;	

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE_AUTO:
		ret =  mt9m113_set_autoexposure(client, ctrl->value);
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
	case V4L2_CID_WHITE_BALANCE_PRESET:
		//ret =  mt9m113_set_autowhitebalance(client, ctrl->value);
		if (V4L2_CID_WHITE_BALANCE_PRESET == ctrl->id)
		{
			switch(ctrl->value)
			{
			case 0:
				value = MT9M113_WB_INCANDESCENT_A;
				break;
			case 1:
				value = MT9M113_WB_FLUORESCENT;
				break;
			case 2:
				value = MT9M113_WB_DAYLIGHT;
				break;
			case 3:
				value = MT9M113_WB_CLOUDY;
				break;
			}
		}
		else
		{
			value = MT9M113_WB_AUTO;
		}
		ret =  mt9m113_set_wb(client, ctrl->value);
		break;
	case V4L2_CID_EXPOSURE:
		switch (ctrl->value)
		{
		case -4:
			value = MT9M113_BRIGHT_REDUCE2;
			break;
		case -3:
			value = MT9M113_BRIGHT_REDUCE1_5;
			break;
		case -2:
			value = MT9M113_BRIGHT_REDUCE1;
			break;
		case -1:
			value = MT9M113_BRIGHT_REDUCE0_5;
			break;
		case 0:
			value = MT9M113_BRIGHT_0;
			break;
		case 1:
			value = MT9M113_BRIGHT_INCREASE0_5;
			break;
		case 2:
			value = MT9M113_BRIGHT_INCREASE1;
			break;
		case 3:
			value = MT9M113_BRIGHT_INCREASE1_5;
			break;
		case 4:
			value = MT9M113_BRIGHT_INCREASE2;
			break;
		}
		ret =  mt9m113_set_bright(client, ctrl->value);
		break;
	case V4L2_CID_CAPTURE:
		ret =  mt9m113_set_capture_resolution(client, ctrl->value);
		break;
	case V4L2_CID_COLORFX:
		ret =  mt9m113_set_effect(client, ctrl->value);
		break;
	//case V4L2_CID_PREVIEW:
	case V4L2_CID_CAM_PREVIEW_ONOFF:
		ret = mt9p113_set_preview_mode(client, ctrl->value);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int mt9m113_suspend(struct soc_camera_device *icd, pm_message_t state)
{
	struct i2c_client *client = to_i2c_client(to_soc_camera_control(icd));
	struct mt9m113 *mt9m113 = to_mt9m113(client);

	mt9m113->gain = mt9m113_get_global_gain(client);

	return 0;
}

static int mt9m113_restore_state(struct i2c_client *client)
{
	struct mt9m113 *mt9m113 = to_mt9m113(client);

	mt9m113_set_context(client, mt9m113->context);
	mt9m113_set_pixfmt(client, mt9m113->pixfmt);
	mt9m113_setup_rect(client, &mt9m113->rect);
	mt9m113_set_flip(client, mt9m113->hflip, MT9M113_RMB_MIRROR_COLS);
	mt9m113_set_flip(client, mt9m113->vflip, MT9M113_RMB_MIRROR_ROWS);
	mt9m113_set_global_gain(client, mt9m113->gain);
	mt9m113_set_autoexposure(client, mt9m113->autoexposure);
	mt9m113_set_autowhitebalance(client, mt9m113->autowhitebalance);
	return 0;
}

static int mt9m113_resume(struct soc_camera_device *icd)
{
	struct i2c_client *client = to_i2c_client(to_soc_camera_control(icd));
	struct mt9m113 *mt9m113 = to_mt9m113(client);
	int ret = 0;

	if (mt9m113->powered) {
		ret = mt9m113_enable(client);
		if (!ret)
			ret = mt9m113_reset(client);
		if (!ret)
			ret = mt9m113_restore_state(client);
	}
	return ret;
}

static int mt9m113_init(struct i2c_client *client)
{
	struct mt9m113 *mt9m113 = to_mt9m113(client);
	int ret;
	printk("..........%s line=%d \n", __FUNCTION__, __LINE__);

	return 0;

	mt9m113->context = HIGHPOWER;
	ret = mt9m113_enable(client);
	if (!ret)
		ret = mt9m113_reset(client);
	if (!ret)
		ret = mt9m113_set_context(client, mt9m113->context);
	if (!ret)
		ret = mt9m113_set_autoexposure(client, mt9m113->autoexposure);
	if (ret)
		dev_err(&client->dev, "mt9m11x init failed: %d\n", ret);
	return ret;
}

/*
 * Interface active, can use i2c. If it fails, it can indeed mean, that
 * this wasn't our capture interface, so, we wait for the right one
 */
static int mt9m113_video_probe(struct soc_camera_device *icd,
			       struct i2c_client *client)
{
	struct mt9m113 *mt9m113 = to_mt9m113(client);
	s32 data;
	int ret;

	/*
	 * We must have a parent by now. And it cannot be a wrong one.
	 * So this entire test is completely redundant.
	 */
	if (!icd->dev.parent ||
	    to_soc_camera_host(icd->dev.parent)->nr != icd->iface)
		return -ENODEV;

	mt9m113->autoexposure = 1;
	mt9m113->autowhitebalance = 1;

	mt9m113->swap_rgb_even_odd = 1;
	mt9m113->swap_rgb_red_blue = 1;

	ret = mt9m113_init(client);
	if (ret)
		goto ei2c;

	data = reg_read(CHIP_VERSION);

	switch (data) {
#if 0
	case 0x143a: /* MT9P111 */
		mt9m113->model = V4L2_IDENT_MT9P111;
		break;
	case 0x148c: /* MT9M112 */
		mt9m113->model = V4L2_IDENT_MT9M112;
		break;
#endif
	case 0x2880:
		mt9m113->model = V4L2_IDENT_MT9P111;
		break;
	default:
		ret = -ENODEV;
		dev_err(&client->dev,
			"No MT9M11x chip detected, register read %x\n", data);
		goto ei2c;
	}

	icd->formats = mt9m113_colour_formats;
	icd->num_formats = ARRAY_SIZE(mt9m113_colour_formats);

	dev_info(&client->dev, "Detected a MT9M11x chip ID %x\n", data);

ei2c:
	return ret;
}

//fix mt9p111 and mt9m113 camera output issue:
extern void fimc_rot_flip_patch(u32 rot, u32 flip);
extern void fimc_cap_rot_flip_patch(u32 rot, u32 flip);

#include <plat/gpio-cfg.h>
/*Shanghai ewada camera power*/
static void  mt9m113_5M_Standby(void);
static void  mt9m113_5M_Standby(void)
{
	int stanby_5M = S5PV210_GPH3(3);   /*active high, work to set to low*/
	
	int err = gpio_request(stanby_5M, "cam stanby_5M");
	if (err) printk("mt9p111_stanby_5M get gpio error \n");

	gpio_direction_output(stanby_5M, 1);
	gpio_free(stanby_5M);
	msleep(30);
}


static int mt9m113_sub_dev_init(struct v4l2_subdev *sd, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int  i;
	int data;
	int size;
	int err;
	unsigned short addr = client->addr;

	v4l_info(client, "%s: camera initialization start\n", __func__);

	printk(" ................. mt9m113_sub_dev_init client name=%s addr=%x, 5M add: 0x%x \n", client->name, client->addr, 0x3D);
	client->addr = 0x3D;
#if 0
	for (i = 0; i < S5K4BA_INIT_REGS; i++) {
		err = s5k4ba_i2c_write(sd, s5k4ba_init_reg[i], \
					sizeof(s5k4ba_init_reg[i]));
		if (err < 0)
			v4l_info(client, "%s: register set failed\n", \
			__func__);
	}

	if (err < 0) {
		v4l_err(client, "%s: camera initialization failed\n", \
			__func__);
		return -EIO;	/* FIXME */
	}
#endif

#if 1	
	size = (sizeof(mt9m113_pre_init_regs) / sizeof(mt9m113_pre_init_regs[0]));
	for (i = 0; i < size; i++) 
	{
		err = mt9m113_reg_write(client, mt9m113_pre_init_regs[i].addr, mt9m113_pre_init_regs[i].val);
		if (err)
			v4l_info(client, "%s: register set failed\n", __func__);
	}
	msleep(5);
	mt9m113_5M_Standby();
	client->addr = addr;
#endif 
	//mt9m113_sensor_power(1);
	//rotate 90 degree clockwise, y flip
	//fimc_rot_flip_patch(90, 0x2);
	fimc_cap_rot_flip_patch(90, 0x2);
    
	for (i = 0; i < 10; i++)
	{
		data = reg_read(CHIP_VERSION);
		if (data ==0x2480)
		{
			break;
		}
		msleep(10);
		printk(" ................ 111111111 CHIP_VERSION=%x \n", data);
	}
	printk(" .................CHIP_VERSION=%x %d \n", data, data);

	size = (sizeof(mt9m113_init_regs0) / sizeof(mt9m113_init_regs0[0]));
	for (i = 0; i < size; i++) 
	{
		err = mt9m113_reg_write(client, mt9m113_init_regs0[i].addr, mt9m113_init_regs0[i].val);
		if (err)
			v4l_info(client, "%s: register set failed\n", __func__);
	}
	msleep(50);
	
	size = (sizeof(mt9m113_init_regs1) / sizeof(mt9m113_init_regs1[0]));
	for (i = 0; i < size; i++) 
	{
		err = mt9m113_reg_write(client, mt9m113_init_regs1[i].addr, mt9m113_init_regs1[i].val);
		if (err)
			v4l_info(client, "%s: register set failed\n", __func__);
	}	
	msleep(3);
	
	size = (sizeof(mt9m113_init_regs2) / sizeof(mt9m113_init_regs2[0]));
	for (i = 0; i < size; i++) 
	{
		err = mt9m113_reg_write(client, mt9m113_init_regs2[i].addr, mt9m113_init_regs2[i].val);
		if (err)
			v4l_info(client, "%s: register set failed\n", __func__);
	}
	msleep(1);
		

	size = (sizeof(mt9m113_init_regs3) / sizeof(mt9m113_init_regs3[0]));
	for (i = 0; i < size; i++) 
	{
		err = mt9m113_reg_write(client, mt9m113_init_regs3[i].addr, mt9m113_init_regs3[i].val);
		if (err)
			v4l_info(client, "%s: register set failed\n", __func__);
	}
	size = (sizeof(mt9m113_autoflick) / sizeof(mt9m113_autoflick[0]));
	for (i = 0; i < size; i++) 
	{
		err = mt9m113_reg_write(client, mt9m113_autoflick[i].addr, mt9m113_autoflick[i].val);
		if (err)
			v4l_info(client, "%s: register set failed\n", __func__);
	}
	size = (sizeof(mt9m113_autofast) / sizeof(mt9m113_autofast[0]));
	for (i = 0; i < size; i++) 
	{
		err = mt9m113_reg_write(client, mt9m113_autofast[i].addr, mt9m113_autofast[i].val);
		if (err)
			v4l_info(client, "%s: register set failed\n", __func__);
	}
	size = (sizeof(mt9m113_ll1) / sizeof(mt9m113_ll1[0]));
	for (i = 0; i < size; i++) 
	{
		err = mt9m113_reg_write(client, mt9m113_ll1[i].addr, mt9m113_ll1[i].val);
		if (err)
			v4l_info(client, "%s: register set failed\n", __func__);
	}
	size = (sizeof(mt9m113_ll2) / sizeof(mt9m113_ll2[0]));
	for (i = 0; i < size; i++) 
	{
		err = mt9m113_reg_write(client, mt9m113_ll2[i].addr, mt9m113_ll2[i].val);
		if (err)
			v4l_info(client, "%s: register set failed\n", __func__);
	}
	size = (sizeof(mt9m113_fw_pipe) / sizeof(mt9m113_fw_pipe[0]));
	for (i = 0; i < size; i++) 
	{
		err = mt9m113_reg_write(client, mt9m113_fw_pipe[i].addr, mt9m113_fw_pipe[i].val);
		if (err)
			v4l_info(client, "%s: register set failed\n", __func__);
	}
	size = (sizeof(mt9m113_lens_correction_4th) / sizeof(mt9m113_lens_correction_4th[0]));
	for (i = 0; i < size; i++) 
	{
		err = mt9m113_reg_write(client, mt9m113_lens_correction_4th[i].addr, mt9m113_lens_correction_4th[i].val);
		if (err)
			v4l_info(client, "%s: register set failed\n", __func__);
	}
	size = (sizeof(mt9m113_gamma_5_045_125) / sizeof(mt9m113_gamma_5_045_125[0]));
	for (i = 0; i < size; i++) 
	{
		err = mt9m113_reg_write(client, mt9m113_gamma_5_045_125[i].addr, mt9m113_gamma_5_045_125[i].val);
		if (err)
			v4l_info(client, "%s: register set failed\n", __func__);
	}
	size = (sizeof(mt9m113_ae_target2) / sizeof(mt9m113_ae_target2[0]));
	for (i = 0; i < size; i++) 
	{
		err = mt9m113_reg_write(client, mt9m113_ae_target2[i].addr, mt9m113_ae_target2[i].val);
		if (err)
			v4l_info(client, "%s: register set failed\n", __func__);
	}
	size = (sizeof(mt9m113_picfine) / sizeof(mt9m113_picfine[0]));
	for (i = 0; i < size; i++) 
	{
		err = mt9m113_reg_write(client, mt9m113_picfine[i].addr, mt9m113_picfine[i].val);
		if (err)
			v4l_info(client, "%s: register set failed\n", __func__);
	}
	msleep(300);

	size = (sizeof(mt9m113_conterxb) / sizeof(mt9m113_conterxb[0]));
	for (i = 0; i < size; i++) 
	{
		err = mt9m113_reg_write(client, mt9m113_conterxb[i].addr, mt9m113_conterxb[i].val);
		if (err)
			v4l_info(client, "%s: register set failed\n", __func__);
	}
	msleep(10);
	
	size = (sizeof(mt9m113_ae_awb) / sizeof(mt9m113_ae_awb[0]));
	for (i = 0; i < size; i++) 
	{
		err = mt9m113_reg_write(client, mt9m113_ae_awb[i].addr, mt9m113_ae_awb[i].val);
		if (err)
			v4l_info(client, "%s: register set failed\n", __func__);
	}
	msleep(200);
	//end of initialize

	size = (sizeof(mt9m113_preview_1280_960) / sizeof(mt9m113_preview_1280_960[0]));
	for (i = 0; i < size; i++) 
	{
		err = mt9m113_reg_write(client, mt9m113_preview_1280_960[i].addr, mt9m113_preview_1280_960[i].val);
		if (err)
			v4l_info(client, "%s: register set failed\n", __func__);
	}
	msleep(200);
	
	size = (sizeof(mt9m113_ae_awb) / sizeof(mt9m113_ae_awb[0]));
	for (i = 0; i < size; i++) 
	{
		err = mt9m113_reg_write(client, mt9m113_ae_awb[i].addr, mt9m113_ae_awb[i].val);
		if (err)
			v4l_info(client, "%s: register set failed\n", __func__);
	}
	
	printk(" .................%s finished \n", __FUNCTION__);
	return 0;
}

static int mt9m113_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;

	dev_dbg(&client->dev, "%s\n", __func__);
	printk( "%s param->type=%d \n", __func__,param->type);

	return err;
}

static int mt9m113_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;

	dev_dbg(&client->dev, "%s: numerator %d, denominator: %d\n", \
		__func__, param->parm.capture.timeperframe.numerator, \
		param->parm.capture.timeperframe.denominator);

	printk("%s: numerator %d, denominator: %d\n", \
		__func__, param->parm.capture.timeperframe.numerator, \
		param->parm.capture.timeperframe.denominator);
	printk( "%s param->type=%d \n", __func__,param->type);
	return err;
}


static int mt9m113_s_config(struct v4l2_subdev *sd, int irq, void *platform_data)
{
	printk(" .................mt9m113_s_config \n");
	return 0;
}

static struct v4l2_subdev_core_ops mt9m113_subdev_core_ops = {
	.init = mt9m113_sub_dev_init,
	.s_config = mt9m113_s_config,
	.g_ctrl		= mt9m113_g_ctrl,
	.s_ctrl		= mt9m113_s_ctrl,
	.g_chip_ident	= mt9m113_g_chip_ident,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= mt9m113_g_register,
	.s_register	= mt9m113_s_register,
#endif
};

static struct v4l2_subdev_video_ops mt9m113_subdev_video_ops = {
	.s_fmt		= mt9m113_s_fmt,
	.g_fmt		= mt9m113_g_fmt,
	.try_fmt	= mt9m113_try_fmt,
	.s_crop		= mt9m113_s_crop,
	.g_crop		= mt9m113_g_crop,
	.cropcap	= mt9m113_cropcap,
	.g_parm    = mt9m113_g_parm,
	.s_parm    = mt9m113_s_parm,
};

static struct v4l2_subdev_ops mt9m113_subdev_ops = {
	.core	= &mt9m113_subdev_core_ops,
	.video	= &mt9m113_subdev_video_ops,
};

#ifdef CONFIG_MX100_IOCTL

static struct i2c_client *g_mt9m113_i2c_client;

int MT9M113_WRITE(unsigned int reg, unsigned int val)
{
	if(!g_mt9m113_i2c_client) return -1;
	
	return mt9m113_reg_write(g_mt9m113_i2c_client,reg,val);
}


int MT9M113_READ(unsigned int reg)
{
	if(!g_mt9m113_i2c_client) return -1;

	return mt9m113_reg_read(g_mt9m113_i2c_client,reg);
}

#endif


static int mt9m113_probe(struct i2c_client *client,
			 const struct i2c_device_id *did)
{
	struct mt9m113 *mt9m113;
	struct soc_camera_device *icd = client->dev.platform_data;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct soc_camera_link *icl;
	int ret;


	printk(" ................. mt9m113_probe client name=%s addr=%x \n", client->name, client->addr);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		
		printk("I2C-Adapter doesn't support I2C_FUNC_SMBUS_WORD\n");
		return -EIO;
	}

	#ifdef CONFIG_MX100_IOCTL
	g_mt9m113_i2c_client = client;
	#endif

	//client->flags |=I2C_M_IGNORE_NAK;
#if 0	
	mt9m113_sensor_power(1);

	reg_set(RESET, MT9M113_RESET_CHIP_ENABLE);

	mt9m113_reset(client);
	
	data = reg_read(CHIP_VERSION);

	printk(" .................CHIP_VERSION=%x \n", data);

	mt9m113_sensor_power(0);
#endif
	//struct s5k4ba_state *state;
	struct v4l2_subdev *sd;

#if 0
	state = kzalloc(sizeof(struct s5k4ba_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;
#endif
	sd = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL); //&state->sd;
	strcpy(sd->name, "mt9m113");

	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &mt9m113_subdev_ops);

	dev_info(&client->dev, "mt0p111 has been probed\n");
	return 0;
	
	
	if (!icd) {
		dev_err(&client->dev, "MT9M11x: missing soc-camera data!\n");
		return -EINVAL;
	}

	icl = to_soc_camera_link(icd);
	if (!icl) {
		dev_err(&client->dev, "MT9M11x driver needs platform data\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_warn(&adapter->dev,
			 "I2C-Adapter doesn't support I2C_FUNC_SMBUS_WORD\n");
		return -EIO;
	}

	mt9m113 = kzalloc(sizeof(struct mt9m113), GFP_KERNEL);
	if (!mt9m113)
		return -ENOMEM;

	v4l2_i2c_subdev_init(&mt9m113->subdev, client, &mt9m113_subdev_ops);

	/* Second stage probe - when a capture adapter is there */
	icd->ops		= &mt9m113_ops;
	icd->y_skip_top		= 0;

	mt9m113->rect.left	= MT9M113_MIN_DARK_COLS;
	mt9m113->rect.top	= MT9M113_MIN_DARK_ROWS;
	mt9m113->rect.width	= MT9M113_MAX_WIDTH;
	mt9m113->rect.height	= MT9M113_MAX_HEIGHT;

	ret = mt9m113_video_probe(icd, client);
	if (ret) {
		icd->ops = NULL;
		i2c_set_clientdata(client, NULL);
		kfree(mt9m113);
	}

	return ret;
}

static int mt9m113_remove(struct i2c_client *client)
{
	struct mt9m113 *mt9m113 = to_mt9m113(client);
	struct soc_camera_device *icd = client->dev.platform_data;

	icd->ops = NULL;
	i2c_set_clientdata(client, NULL);
	client->driver = NULL;
	kfree(mt9m113);

	return 0;
}

static const struct i2c_device_id mt9m113_id[] = {
	{ "mt9m113", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mt9m113_id);

static struct i2c_driver mt9m113_i2c_driver = {
	.driver = {
		.name = "mt9m113",
	},
	.probe		= mt9m113_probe,
	.remove		= mt9m113_remove,
	.id_table	= mt9m113_id,
};

static int __init mt9m113_mod_init(void)
{
	printk(" \n\n................. mt9m113_mod_init \n");
	return i2c_add_driver(&mt9m113_i2c_driver);
}

static void __exit mt9m113_mod_exit(void)
{
	i2c_del_driver(&mt9m113_i2c_driver);
}

//module_init(mt9m113_mod_init);
late_initcall(mt9m113_mod_init);

module_exit(mt9m113_mod_exit);

MODULE_DESCRIPTION("Micron MT9P111/MT9M112 Camera driver");
MODULE_AUTHOR("Robert Jarzmik");
MODULE_LICENSE("GPL");
