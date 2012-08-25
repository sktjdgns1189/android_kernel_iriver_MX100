/*
 * gps-gpio.c
 *
 * GPS GPIO driver
 *
 * Copyright (C) 2010 iriver, Inc.
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
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/uaccess.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio-bank.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>


#define GPSGPIO_IOC_MAGIC 'G'

#define IOCTL_GPS_PWRON				    _IO(GPSGPIO_IOC_MAGIC,0)
#define IOCTL_GPS_PWROFF				_IO(GPSGPIO_IOC_MAGIC,1)

/*----------------------------------------------------------------------------------*/
//  #define rdebug(fmt,arg...) printk(KERN_CRIT "___[gps-gpio] " fmt "\n",## arg)
    #define rdebug(fmt,arg...)
/*----------------------------------------------------------------------------------*/


static int Device_Open = 0;

/* Shanghai ewada */
static int is_device_need_suspend = 0;


static int gpsgpio_open(struct inode *inode, struct file *file)
{
	if (Device_Open)
		return -EBUSY;

	Device_Open++;
    return 0;
}

static int gpsgpio_close(struct inode *inode, struct file *file)
{
	Device_Open = 0;
    return 0;
}

#define JANGED_GPS
#if defined(JANGED_GPS)
static int function_status = 0;
int device_poweronoff (int value)
{
	if (function_status)
		return 0;

	function_status = 1;
	
	if (value)
	{
    	rdebug("device_poweron");

/* ktj_gps
	    if (gpio_is_valid(GPIO_GPS_PWR_ENABLE_OUT)) 
	    {
	    	if (gpio_request(GPIO_GPS_PWR_ENABLE_OUT, "GPJ0"))
	    	{
	    		printk(KERN_ERR "Failed to request GPIO_GPS_PWR_ENABLE_OUT\n");
				printk (KERN_ERR "GPS GPIO Power On Failure!\n");
				function_status = 0;
				return -1;

	    	}
	    	gpio_direction_output(GPIO_GPS_PWR_ENABLE_OUT, GPIO_LEVEL_HIGH);
	    }
	    s3c_gpio_setpull(GPIO_GPS_PWR_ENABLE_OUT, S3C_GPIO_PULL_NONE);
*/

/*
#ifdef CONFIG_MX100_REV_TP
		gpio_set_value(GPIO_GPS_DEBUG_SELECT_OUT,   GPIO_LEVEL_HIGH);   // set gps
//      msleep(10);
        msleep(100);
#endif
*/

		gpio_set_value(GPIO_GPS_PWR_ENABLE_OUT,     GPIO_LEVEL_HIGH);

		function_status = 0;

		return 0;
	}
	else
	{
	    rdebug("device_poweroff");

		gpio_set_value(GPIO_GPS_PWR_ENABLE_OUT,     GPIO_LEVEL_LOW);

/*
#ifdef CONFIG_MX100_REV_TP
		gpio_set_value(GPIO_GPS_DEBUG_SELECT_OUT,   GPIO_LEVEL_LOW);    // set debug
//      msleep(10);
        msleep(100);
#endif
*/

		gpio_free (GPIO_GPS_PWR_ENABLE_OUT);

		function_status = 0;
		return 0;
	}

}

static int gpsgpio_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int nretVal = 0;

	switch(cmd)
	{

	case IOCTL_GPS_PWRON:
        nretVal = device_poweronoff(1);
        if(!nretVal)		//janged add
			is_device_need_suspend = 1;		
			
        break;
        
	case IOCTL_GPS_PWROFF:
        nretVal = device_poweronoff(0);
		is_device_need_suspend = 0;
        break;
    }

	return nretVal;

}
#else
int device_poweron ()
{
    rdebug("device_poweron");

/* 
    if (gpio_is_valid(GPIO_GPS_PWR_ENABLE_OUT)) 
    {
    	if (gpio_request(GPIO_GPS_PWR_ENABLE_OUT, "GPJ0"))
    	{
    		printk(KERN_ERR "Failed to request GPIO_GPS_PWR_ENABLE_OUT\n");
    		goto ERROR_VAL;
    	}
    	gpio_direction_output(GPIO_GPS_PWR_ENABLE_OUT, GPIO_LEVEL_HIGH);
    }
    s3c_gpio_setpull(GPIO_GPS_PWR_ENABLE_OUT, S3C_GPIO_PULL_NONE);
*/

	gpio_set_value(GPIO_GPS_PWR_ENABLE_OUT, GPIO_LEVEL_HIGH);

	return 0;

/*	
ERROR_VAL: 
	printk (KERN_ERR "GPS GPIO Power On Failure!\n");

	return -1;
*/
}

int device_poweroff ()
{
    rdebug("device_poweroff");

	gpio_set_value(GPIO_GPS_PWR_ENABLE_OUT, GPIO_LEVEL_LOW);
//	gpio_free (GPIO_GPS_PWR_ENABLE_OUT);    // no need to free gpio

	return 0;
}

static int gpsgpio_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int nretVal = 0;

	switch(cmd)
	{

	case IOCTL_GPS_PWRON:
        nretVal = device_poweron();
        if(!nretVal)		//janged add
			is_device_need_suspend = 1;
			
        break;
        
	case IOCTL_GPS_PWROFF:
        nretVal = device_poweroff();
		is_device_need_suspend = 0;
        break;
    }

	return nretVal;
}

#endif

static const struct file_operations gpsgpio_fops = {
	.owner      = THIS_MODULE,
	.open       = gpsgpio_open,
	.release    = gpsgpio_close,
	.ioctl      = gpsgpio_ioctl,
};

static struct miscdevice gpsgpio_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gpsgpio",
	.fops = &gpsgpio_fops,
};

/*Shanghai ewada*/
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static struct early_suspend gpssuspend;
#endif


#if defined(JANGED_GPS)
static int gps_resume(struct early_suspend *h) 
{
    if (is_device_need_suspend)
		device_poweronoff(1);
	return 0;
}

static int gps_suspend(struct early_suspend *h) 
{

	printk("[KENEL] %s, %d \n", __FUNCTION__, __LINE__);
	if (is_device_need_suspend)
		device_poweronoff(0);

	return 0;
}

#else
static int gps_resume(struct early_suspend *h) 
{
    if (is_device_need_suspend)
		device_poweron();
	return 0;
}

static int gps_suspend(struct early_suspend *h) 
{
	if (is_device_need_suspend)
		device_poweroff();

	return 0;
}
#endif

/*
 * Module init and exit
 */
static int __init gpsgpio_init(void)
{
    rdebug("%s called", __func__);


// ktj add
    // init, gps power-off
    if (gpio_is_valid(GPIO_GPS_PWR_ENABLE_OUT)) 
    {
    	if (gpio_request(GPIO_GPS_PWR_ENABLE_OUT, "GPJ0"))
    		printk(KERN_ERR "Failed to request GPIO_GPS_PWR_ENABLE_OUT\n");
    	gpio_direction_output(GPIO_GPS_PWR_ENABLE_OUT, GPIO_LEVEL_LOW);
    }
    s3c_gpio_setpull(GPIO_GPS_PWR_ENABLE_OUT, S3C_GPIO_PULL_NONE);
// ktj end


/*Shanghai ewada*/
#ifdef CONFIG_HAS_EARLYSUSPEND
	#if 1	//janged for sleep wake up
	gpssuspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	gpssuspend.suspend = gps_suspend;
	gpssuspend.resume = gps_resume;
	register_early_suspend(&gpssuspend);
	#endif
#endif /* CONFIG_HAS_EARLYSUSPEND */

	return misc_register(&gpsgpio_device);
}
module_init(gpsgpio_init);

static void __exit gpsgpio_exit(void)
{
/*Shanghai ewada*/
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&gpssuspend);
#endif
}
module_exit(gpsgpio_exit);

MODULE_DESCRIPTION("GPS GPIO driver");
MODULE_AUTHOR("iriver/ktj");
MODULE_LICENSE("GPL");
