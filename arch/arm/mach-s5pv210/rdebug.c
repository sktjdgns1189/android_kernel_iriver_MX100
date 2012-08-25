/*
 * rdebug.c
 *
 * Control Debug driver
 *
 * Copyright (C) 2010 iriver, Inc.
 * ktj
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/uaccess.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio-bank.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>


typedef struct
{
	u32 reg;
	u32 data;
}lcddata_t;

typedef struct
{
	u8 reg;
	u8 value;
}regdata_t;

#define RDBG_IOC_MAGIC             'R'

#define RDBG_BATTERY			    _IOW(RDBG_IOC_MAGIC, 0, int)
#define RDBG_BACKLIGHT		        _IOW(RDBG_IOC_MAGIC, 1, int)
#define RDBG_LIGHTSENSOR	        _IOW(RDBG_IOC_MAGIC, 2, int)
#define RDBG_LCD                    _IOW(RDBG_IOC_MAGIC, 3, lcddata_t)
#define RDBG_SET_BL                 _IOW(RDBG_IOC_MAGIC, 5, int)
#define RDBG_PMIC_READ              _IOWR(RDBG_IOC_MAGIC, 6, regdata_t)
#define RDBG_PMIC_WRITE             _IOW(RDBG_IOC_MAGIC, 7, regdata_t)
#define RDBG_GET_SERIAL             _IOR(RDBG_IOC_MAGIC, 8, unsigned int)

/*----------------------------------------------------------------------------------*/
//  #define rdebug(fmt,arg...) printk(KERN_CRIT "_[rdbg] " fmt ,## arg)
    #define rdebug(fmt,arg...)
/*----------------------------------------------------------------------------------*/


extern void set_debug_battery(int enable);
extern void set_debug_backlight(int enable);
extern void set_debug_lightsensor(int enable);
extern void set_backlight_period(int period);

extern void set_reg(u32 reg, u32 data);  
extern void get_reg(u32 reg);  
extern void set_gammalut(u32 mode);
extern void get_gammalut(u32 mode);

extern void pmic_write_reg(unsigned char reg, unsigned char value);
extern unsigned char pmic_read_reg(unsigned char reg);

extern unsigned int get_device_serial(void);

static int rdbg_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int rdbg_close(struct inode *inode, struct file *file)
{
    return 0;
}

static int rdbg_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    int value;
	void __user *argp = (void __user *)arg;   
    lcddata_t lcddata;
    regdata_t regdata; 
    u8 pmic_value;      
    unsigned int serial;      

	switch(cmd)
	{
	case RDBG_GET_SERIAL:
        serial = get_device_serial;
        // printk("___serial = %08X \n", serial);
        if (copy_to_user(argp, (void*) &serial, sizeof(unsigned int)))
            ret = -1;
        break;
	
	case RDBG_PMIC_READ:
		if(copy_from_user((void*) &regdata, argp, sizeof(regdata_t)))
			ret = -1;
        pmic_value = pmic_read_reg(regdata.reg);
        regdata.value = pmic_value;
        if (copy_to_user(argp, (void*) &regdata, sizeof(regdata)))
            ret = -1;
        break;

	case RDBG_PMIC_WRITE:
		if(copy_from_user((void*) &regdata, argp, sizeof(regdata_t)))
			ret = -1;
        pmic_write_reg(regdata.reg, regdata.value);
        break;

	case RDBG_BATTERY:
		if(copy_from_user((void*) &value, argp, sizeof(int)))
			ret = -1;
    #if defined(CONFIG_BATTERY_MAX17040) /* fix build error on evm */
        set_debug_battery(value);
    #endif
        break;
        
	case RDBG_BACKLIGHT:
		if(copy_from_user((void*) &value, argp, sizeof(int)))
			ret = -1;
        set_debug_backlight(value);
        break;

	case RDBG_LIGHTSENSOR:
		if(copy_from_user((void*) &value, argp, sizeof(int)))
			ret = -1;
        set_debug_lightsensor(value);
        break;

	case RDBG_LCD:
		if(copy_from_user((void*) &lcddata, argp, sizeof(lcddata_t)))
			ret = -1;

        printk("set lcd reg = %d  data = 0x%8x \n", lcddata.reg, lcddata.data);    

        if (lcddata.reg == 100000)
        {
            get_reg(lcddata.data);  
        }    
        else if (lcddata.reg == 200000)
        {
            set_gammalut(lcddata.data);
        }    
        else if (lcddata.reg == 200001)
        {
            get_gammalut(lcddata.data);
        }    
        else 
        {
            set_reg(lcddata.reg, lcddata.data);  
        }
        break;

	case RDBG_SET_BL:
		if(copy_from_user((void*) &value, argp, sizeof(int)))
			ret = -1;
        set_backlight_period(value);
        break;
    }

	return ret;
}

static const struct file_operations rdbg_fops = {
	.owner      = THIS_MODULE,
	.open       = rdbg_open,
	.release    = rdbg_close,
	.ioctl      = rdbg_ioctl,
};

static struct miscdevice rdbg_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "rdbg",
	.fops = &rdbg_fops,
};

/*
 * Module init and exit
 */
static int __init rdbg_init(void)
{
    rdebug("%s called", __func__);

	return misc_register(&rdbg_device);
}
module_init(rdbg_init);

static void __exit rdbg_exit(void)
{

}
module_exit(rdbg_exit);

MODULE_DESCRIPTION("Debug driver");
MODULE_AUTHOR("iriver/ktj");
MODULE_LICENSE("GPL");
