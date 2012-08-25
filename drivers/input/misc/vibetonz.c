/*
 * drivers/input/misc/vibetonz.c
 *
 * Haptic driver for ISA1200
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <asm/io.h>
#include <mach/hardware.h>
#include <plat/gpio-cfg.h>
#include <linux/delay.h>
#include <linux/timed_output.h>
#include <linux/pwm.h>


//----------------------------------------------------------------------------------
//#define rdebug(fmt,arg...) printk(KERN_CRIT "___[vibtonz] " fmt "\n",## arg)
#define rdebug(fmt,arg...)
//----------------------------------------------------------------------------------

#define GPIO_HAPTIC_LEN         S5PV210_GPJ3(2)  
#define GPIO_HAPTIC_HEN         S5PV210_GPJ3(3)  
#define GPIO_LEVEL_HIGH         1
#define GPIO_LEVEL_LOW          0

#define VIBETONZ_PWM	        2           /* pwm_id */
#define VIBRATOR_PERIOD	        22321
#define VIBRATOR_DUTY_HALF	    VIBRATOR_PERIOD / 2

#define VIBRATOR_DUTY	        21000
//#define VIBRATOR_DUTY	        22200   

static struct hrtimer timer;
static int max_timeout = 5000;
static int vibrator_value = 0;
struct pwm_device	*vib_pwm;


static int set_vibetonz(int timeout)
{
	if(!timeout) {
		rdebug("DISABLE");

		pwm_config(vib_pwm, VIBRATOR_DUTY_HALF, VIBRATOR_PERIOD);
		pwm_enable(vib_pwm);

//      mdelay(1);

#if 1
//	    gpio_direction_output(GPIO_HAPTIC_HEN, GPIO_LEVEL_LOW);

		gpio_set_value(GPIO_HAPTIC_HEN, GPIO_LEVEL_LOW);
		gpio_direction_input(GPIO_HAPTIC_HEN);
		s3c_gpio_setpull(GPIO_HAPTIC_HEN,S3C_GPIO_PULL_DOWN);
	    
	    
#else
	    if (gpio_is_valid(GPIO_HAPTIC_HEN)) {
		    if (gpio_request(GPIO_HAPTIC_HEN, "GPJ3")) 
			    printk(KERN_ERR "Failed to request GPIO_HAPTIC_HEN!\n");

		    gpio_direction_output(GPIO_HAPTIC_HEN, GPIO_LEVEL_LOW);
	    }
	    gpio_free(GPIO_HAPTIC_HEN);	
#endif

		pwm_disable(vib_pwm);
	}
	else {
		rdebug("ENABLE");

		pwm_config(vib_pwm, VIBRATOR_DUTY_HALF, VIBRATOR_PERIOD);
		pwm_enable(vib_pwm);
        
//      mdelay(1);

#if 1
//	    gpio_direction_output(GPIO_HAPTIC_HEN, GPIO_LEVEL_HIGH);

		gpio_direction_output(GPIO_HAPTIC_HEN, GPIO_LEVEL_LOW);
//		mdelay(1);
		gpio_set_value(GPIO_HAPTIC_HEN, GPIO_LEVEL_HIGH);
	    
#else
	    if (gpio_is_valid(GPIO_HAPTIC_HEN)) {
		    if (gpio_request(GPIO_HAPTIC_HEN, "GPJ3")) 
			    printk(KERN_ERR "Failed to request GPIO_HAPTIC_HEN!\n");

		    gpio_direction_output(GPIO_HAPTIC_HEN, GPIO_LEVEL_HIGH);
	    }
	    gpio_free(GPIO_HAPTIC_HEN);	
#endif

		pwm_config(vib_pwm, VIBRATOR_DUTY, VIBRATOR_PERIOD);
		pwm_enable(vib_pwm);
	}

	vibrator_value = timeout;
	
	return 0;
}

static enum hrtimer_restart vibetonz_timer_func(struct hrtimer *timer)
{
//	rdebug("%s called", __func__);
	set_vibetonz(0);
	return HRTIMER_NORESTART;
}

static int get_time_for_vibetonz(struct timed_output_dev *dev)
{
	int remaining;

	if (hrtimer_active(&timer)) {
		ktime_t r = hrtimer_get_remaining(&timer);
		remaining = r.tv.sec * 1000 + r.tv.nsec / 1000000;
	} else
		remaining = 0;

	if (vibrator_value ==-1)
		remaining = -1;

	rdebug("%s : remaining = %d", remaining);

	return remaining;
}

#define VIBE_DISABLE_SOC_LEVEL      1
extern int get_battery_level(void);
  
static void enable_vibetonz_from_user(struct timed_output_dev *dev,int value)
{
	rdebug("%s : time = %d msec",__func__,value);

    /* ktj, disable vibe when low battery level */
    if (get_battery_level() <= VIBE_DISABLE_SOC_LEVEL)
    {    
    	rdebug("%s : No vibe for low battery level = %d",__func__, get_battery_level() );
        return;
    }

	hrtimer_cancel(&timer);
	
	set_vibetonz(value);
	vibrator_value = value;

	if (value > 0) 
	{
		if (value > max_timeout)
			value = max_timeout;

		hrtimer_start(&timer,
						ktime_set(value / 1000, (value % 1000) * 1000000),
						HRTIMER_MODE_REL);
		vibrator_value = 0;
	}
}

static struct timed_output_dev timed_output_vt = {
	.name     = "vibrator",
	.get_time = get_time_for_vibetonz,
	.enable   = enable_vibetonz_from_user,
};

void vibetonz_start(void)
{
	int ret = 0;

	rdebug("%s called", __func__);

	/* hrtimer settings */
	hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timer.function = vibetonz_timer_func;

    vib_pwm = pwm_request(VIBETONZ_PWM, "vibtonz");
	
	/* timed_output_device settings */
	ret = timed_output_dev_register(&timed_output_vt);
	if(ret)
		printk(KERN_ERR "[___vibtonz] timed_output_dev_register is fail \n");	
}


void vibetonz_end(void)
{
	printk("[VIBETONZ] %s \n",__func__);
}

