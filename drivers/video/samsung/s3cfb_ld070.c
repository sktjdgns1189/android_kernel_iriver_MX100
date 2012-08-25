/*
 * LD070 LCD Panel Driver
 *
 * Copyright (c) 2010 iriver
 * Author: ktj
 *
 * Derived from drivers/video/omap/lcd-apollon.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <linux/wait.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>

#include <plat/gpio-cfg.h>
#include <plat/regs-fb.h>

#include "s3cfb.h"

#define USE_IOCTL   /* ioctl for icmd, ktj */
#ifdef USE_IOCTL
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#endif

/* ktj */
//----------------------------------------------------------------------------------
//#define debug(fmt,arg...) printk(KERN_CRIT "___[s3cfb_ld070] " fmt "\n",## arg)
#define debug(fmt,arg...)
//----------------------------------------------------------------------------------

#define SLEEPMSEC		0x1000
#define ENDDEF			0x2000
#define	DEFMASK			0xFF00
#define COMMAND_ONLY	0xFE
#define DATA_ONLY		0xFF

static struct spi_device *g_spi;


#define CABC_OFF    0x30
#define CABC_UI     0x70
#define CABC_PIC    0xB0
#define CABC_MOV    0xF0

//#define CABC_MODE   CABC_OFF
#define CABC_MODE   CABC_MOV  // LGD issue

const unsigned short LD070_DISPLAY_ON[] = {

    0x00, 0x21,
    0x00, 0xA5,
    0x04, CABC_MODE,
    0x08, 0x40,
    0x38, 0x5F,
    0x3C, 0xA4,
    0x10, 0x00,         // R4
    0x34, 0x00,
    0x08, 0x43,
    0x28, 0x28,
    0x40, 0x41,
    0x00, 0xAD,
	ENDDEF, 0x0000
};

const unsigned short LD070_CABC_OFF[] = {
    0x04, CABC_OFF,
	ENDDEF, 0x0000
};

const unsigned short LD070_CABC_UI[] = {
    0x04, CABC_UI,
	ENDDEF, 0x0000
};

const unsigned short LD070_CABC_PIC[] = {
    0x04, CABC_PIC,
	ENDDEF, 0x0000
};

const unsigned short LD070_CABC_MOV[] = {
    0x04, CABC_MOV,
	ENDDEF, 0x0000
};


/* 
const unsigned short SEQ_DISPLAY_ON[] = {
	0x14, 0x03,

	ENDDEF, 0x0000
};

const unsigned short SEQ_DISPLAY_OFF[] = {
	0x14, 0x00,

	ENDDEF, 0x0000
};

const unsigned short SEQ_STANDBY_ON[] = {
	0x1D, 0xA1,
	SLEEPMSEC, 200,

	ENDDEF, 0x0000
};

const unsigned short SEQ_STANDBY_OFF[] = {
	0x1D, 0xA0,
	SLEEPMSEC, 250,

	ENDDEF, 0x0000
};

const unsigned short SEQ_SETTING[] = {
	0x31, 0x08, // panel setting 
	0x32, 0x14,
	0x30, 0x02,
	0x27, 0x03, // 0x27, 0x01 
	0x12, 0x08,
	0x13, 0x08,
	0x15, 0x10, // 0x15, 0x00 
	0x16, 0x00, // 24bit line and 16M color 

	0xef, 0xd0, // pentile key setting 
	DATA_ONLY, 0xe8,

	0x39, 0x44,
	0x40, 0x00, 
	0x41, 0x3f, 
	0x42, 0x2b, 
	0x43, 0x1f, 
	0x44, 0x24, 
	0x45, 0x1b, 
	0x46, 0x29, 
	0x50, 0x00, 
	0x51, 0x00, 
	0x52, 0x00, 
	0x53, 0x1b, 
	0x54, 0x22, 
	0x55, 0x1b, 
	0x56, 0x2a, 
	0x60, 0x00, 
	0x61, 0x3f, 
	0x62, 0x25, 
	0x63, 0x1c, 
	0x64, 0x21, 
	0x65, 0x18, 
	0x66, 0x3e, 

	0x17, 0x22,	//Boosting Freq
	0x18, 0x33,	//power AMP Medium
	0x19, 0x03,	//Gamma Amp Medium
	0x1a, 0x01,	//Power Boosting 
	0x22, 0xa4,	//Vinternal = 0.65*VCI
	0x23, 0x00,	//VLOUT1 Setting = 0.98*VCI
	0x26, 0xa0,	//Display Condition LTPS signal generation : Reference= DOTCLK

	0x1d, 0xa0,
	SLEEPMSEC, 300,

	0x14, 0x03,

	ENDDEF, 0x0000
};
*/

static struct s3cfb_lcd ld070 = {
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
	.freq   = 51,   // pixclk 22125          
//	.freq   = 60,   // pixclk 18806                
//	.freq   = 70,   // pixclk 16119                

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
//		.h_fp   = 160,            // Horizontal Front Porch (16~216)
		.h_fp   =  25,            // Horizontal Front Porch
		.h_bp   = 160,            // Horizontal Back Porch  (160)
//		.h_sw   =  30,            // HSYNC Low Width        (1~140)
		.h_sw   =   5,            // HSYNC Low Width    
		.v_fp   =  12,            // Vertical Front Porch   (1~127)
		.v_fpe  =   2,            // 1 or 2, OdroidT is 2
		.v_bp   =  23,            // Vertical Back Porch    (23)
		.v_bpe  =   2,            // 1 or 2, OdroidT is 2
//		.v_sw   =  10,            // VSYNC Low Width        (1~10)
		.v_sw   =   3,            // VSYNC Low Width    
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

static int ld070_spi_write_driver(int addr, int data)
{
	u16 buf[1];
	struct spi_message msg;

	struct spi_transfer xfer = {
		.len	= 2,
		.tx_buf	= buf,
	};

    buf[0] = 0;
	buf[0] = (addr << 8) | data;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	return spi_sync(g_spi, &msg);
}


static void ld070_spi_write(unsigned char address, unsigned char command)
{
    ld070_spi_write_driver(address, command);
}

static void ld070_panel_send_sequence(const unsigned short *wbuf)
{
	int i = 0;

    debug("%s called", __func__);

	while ((wbuf[i] & DEFMASK) != ENDDEF) {
		if ((wbuf[i] & DEFMASK) != SLEEPMSEC)
			ld070_spi_write(wbuf[i], wbuf[i+1]);
		else
			msleep(wbuf[i+1]);
			//mdelay(wbuf[i+1]);
		i += 2;
	}

}

void ld070_ldi_init(void)
{
    debug("%s called", __func__);


	ld070_panel_send_sequence(LD070_DISPLAY_ON);

	mdelay(100); /* ktj fix flashing on lcd on, 2011-1-26 */

//  ld070_panel_send_sequence(LD070_CABC_MOV); // ktj fix half lcd issue
}

void ld070_ldi_enable(void)
{
    debug("%s called", __func__);
}

void ld070_ldi_disable(void)
{
    debug("%s called", __func__);
}

void ld070_ldi_cabc_set(int mode)
{
    printk("%s called mode = 0x%x\n", __func__, mode);

    if (mode == 0)
        ld070_panel_send_sequence(LD070_CABC_OFF);
    else if (mode == 1)
        ld070_panel_send_sequence(LD070_CABC_UI);
    else if (mode == 2)
        ld070_panel_send_sequence(LD070_CABC_PIC);
    else if (mode == 3)
        ld070_panel_send_sequence(LD070_CABC_MOV);
}

void s3cfb_set_lcd_info(struct s3cfb_global *ctrl)
{
    debug("%s called", __func__);

//	ld070.init_ldi = NULL;
	ld070.init_ldi = ld070_ldi_init;	/* for s3cfb_late_resume, ktj */

	ctrl->lcd = &ld070;
}


#ifdef USE_IOCTL /* ktj */
#define LD070_IOC_MAGIC 'B'
#define LD070_SET_CABCMODE				_IOWR(LD070_IOC_MAGIC,0, unsigned char)

static int ld070_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int ld070_close(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t ld070_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{	
    return 0;
}

static ssize_t ld070_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    return 0;
}

static int ld070_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned char data[6];

    debug("%s called cmd=%d", __func__, cmd);

	switch(cmd)
	{

	case LD070_SET_CABCMODE:
		if(copy_from_user(data,(unsigned char*)arg,1)!=0)
		{
			debug(KERN_INFO "copy_from_user error\n");
			return -EFAULT;
		}
		ld070_ldi_cabc_set(*data);
		break;
    
    default:
        break;        
    }

    return 0;
}

static const struct file_operations ld070_fops = {
	.owner = THIS_MODULE,
	.read = ld070_read,
	.write = ld070_write,
	.open = ld070_open,
	.release = ld070_close,
	.ioctl = ld070_ioctl,
};

static struct miscdevice ld070_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ld070",
	.fops = &ld070_fops,
};
#endif // USE_IOCTL


static int __init ld070_probe(struct spi_device *spi)
{
	int ret;

    debug("%s called", __func__);

	spi->bits_per_word = 16;
	ret = spi_setup(spi);

	g_spi = spi;

/* ktj, disable to avoid flashing, 2011-1-26 
	ld070_ldi_init();
	ld070_ldi_enable();
*/

    ld070_panel_send_sequence(LD070_CABC_MOV); // ktj fix half lcd issue

#ifdef USE_IOCTL
  	int err = 0;
	err = misc_register(&ld070_device);
	if (err) {
		printk(KERN_ERR "ld070 misc device register failed\n");
		//goto exit_kfree;
	}
    else
	    printk(KERN_INFO "ld070 misc device registered ok\n");
#endif

	if (ret < 0)
		return 0;

	return ret;
}

#ifdef CONFIG_PM
int ld070_suspend(struct platform_device *pdev, pm_message_t state)
{
    debug("%s called", __func__);
	ld070_ldi_disable();
	return 0;
}

int ld070_resume(struct platform_device *pdev, pm_message_t state)
{
    debug("%s called", __func__);
	ld070_ldi_init();
	ld070_ldi_enable();

	return 0;
}
#endif

static struct spi_driver ld070_driver = {
	.driver = {
		.name	= "ld070",
		.owner	= THIS_MODULE,
	},
	.probe		= ld070_probe,
	.remove		= __exit_p(ld070_remove),
	.suspend	= NULL,
	.resume		= NULL,
};

static int __init ld070_init(void)
{
    debug("%s called", __func__);
	return spi_register_driver(&ld070_driver);
}

static void __exit ld070_exit(void)
{
    debug("%s called", __func__);
	spi_unregister_driver(&ld070_driver);
}

module_init(ld070_init);
module_exit(ld070_exit);
