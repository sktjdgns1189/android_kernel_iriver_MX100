/*
 * linux/drivers/video/backlight/pwm_bl.c
 *
 * simple PWM based backlight control, board code has to setup
 * 1) pin configuration so PWM waveforms can output
 * 2) platform_data being correctly configured
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/pwm_backlight.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#define GPIO_LED_PWM        S5PV210_GPD0(0)     

#if defined(CONFIG_MX100) /* ktj */
//----------------------------------------------------------------------------------
//  #define rdebug(fmt,arg...) printk(KERN_CRIT "_[pwm_bl] " fmt ,## arg)
    #define rdebug(fmt,arg...)
//----------------------------------------------------------------------------------

/* fs = 25KHz ts = 40000 */
static unsigned int period_tbl[] = {
    0,
 2200,
 2400,
 2600,
 2800,
 3000,
 3200,
 3400,
 3600,
 3800,
 4000,
 4200,
 4400,
 4600,
 4800,
 5000,
 5200,
 5400,
 5600,
 5800,
 6000,
 6092,
 6179,
 6266,
 6353,
 6440,
 6527,
 6614,
 6701,
 6788,
 6875,
 7008,
 7141,
 7274,
 7407,
 7540,
 7673,
 7806,
 7939,
 8072,
 8205,
 8338,
 8471,
 8604,
 8737,
 8870,
 9003,
 9136,
 9269,
 9402,
 9535,
 9668,
 9801,
 9934,
10067,
10200,
10333,
10466,
10599,
10732,
10865,
10998,
11131,
11264,
11397,
11530,
11663,
11796,
11929,
12062,
12195,
12328,
12461,
12594,
12727,
12860,
12993,
13126,
13259,
13392,
13525,
13658,
13791,
13924,
14057,
14190,
14323,
14456,
14589,
14722,
14855,
14988,
15121,
15254,
15387,
15520,
15653,
15786,
15919,
16052,
16185,
16318,
16451,
16584,
16717,
16850,
16983,
17116,
17249,
17382,
17515,
17648,
17781,
17914,
18047,
18180,
18313,
18446,
18579,
18712,
18845,
18978,
19111,
19244,
19377,
19510,
19643,
19776,
19909,
20042,
20175,
20308,
20441,
20574,
20707,
20840,
20973,
21106,
21239,
21372,
21505,
21638,
21771,
21904,
22037,
22170,
22303,
22436,
22569,
22702,
22835,
22968,
23101,
23234,
23367,
23500,
23633,
23766,
23899,
24032,
24165,
24298,
24431,
24564,
24697,
24830,
24963,
25096,
25229,
25362,
25495,
25628,
25761,
25894,
26027,
26160,
26293,
26426,
26559,
26692,
26825,
26958,
27091,
27224,
27357,
27490,
27623,
27756,
27889,
28022,
28155,
28288,
28421,
28554,
28687,
28820,
28953,
29086,
29219,
29352,
29485,
29618,
29751,
29884,
30017,
30150,
30283,
30416,
30549,
30682,
30815,
30948,
31081,
31214,
31347,
31480,
31613,
31746,
31879,
32012,
32145,
32278,
32411,
32544,
32677,
32810,
32943,
33076,
33209,
33342,
33475,
33608,
33741,
33874,
34007,
34140,
34273,
34406,
34539,
34672,
34805,
34938,
35071,
35204,
35337,
35470,
35603,
35736,
35869,
36002,
36135,
36268,
36401,
36534,
36667,
36800
};


static struct backlight_device *backlight;
static int debug_enable = 0;

void set_debug_backlight(int enable)
{
    if (enable)
        debug_enable = 1;
    else
        debug_enable = 0;
}
EXPORT_SYMBOL(set_debug_backlight);
#endif /* CONFIG_MX100 */


struct pwm_bl_data {
	struct pwm_device	*pwm;
	unsigned int		period;
	int			(*notify)(int brightness);
};

static int pwm_backlight_update_status(struct backlight_device *bl)
{
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);
	int brightness = bl->props.brightness;
	int max = bl->props.max_brightness;

	if (bl->props.power != FB_BLANK_UNBLANK)
		brightness = 0;

	if (bl->props.fb_blank != FB_BLANK_UNBLANK)
		brightness = 0;

	if (pb->notify)
		brightness = pb->notify(brightness);

#if defined(CONFIG_MX100) /* ktj */
#if defined(CONFIG_MX100_EVM)
    {
        /* 
            inuput is duty_ns 
            brightness input range : 30 ~ 255, 21 steps    
        */
        int period;
        int period_min = 24000;
        
        period = pb->period - (brightness * pb->period / max);
        period = period * period_min / 40000;
        
	    if (brightness == 0) {
	    	pwm_config(pb->pwm, period, pb->period);
	    	pwm_disable(pb->pwm);
	
	    } else {
	    	pwm_config(pb->pwm, period, pb->period);
	    	pwm_enable(pb->pwm);
        }
	    rdebug("brightness=%3d duty_ns=%5d period_ns=%5d \n\n", brightness, period, pb->period);
    }
#elif defined(CONFIG_MX100_WS)
    {
        /* pb->period : 40000nsec, 25KHz */
        int period = 0;
        
    #if 1 /* use period_tbl */

	    if (brightness == 0) {
	    	pwm_config(pb->pwm, 0, pb->period);
	    	pwm_disable(pb->pwm);

	        gpio_request(GPIO_LED_PWM, "GPD0");
	        gpio_direction_output(GPIO_LED_PWM, 0);
	        gpio_free(GPIO_LED_PWM);
	    } else {
            period = period_tbl[brightness];
	    	pwm_config(pb->pwm, period, pb->period);
	    	pwm_enable(pb->pwm);
	    }
    #else
	    if (brightness == 0) {
	    	pwm_config(pb->pwm, 0, pb->period);
	    	pwm_disable(pb->pwm);
	    } else {
	        period = (brightness * pb->period / max) * 92 / 100;  /* max brightness duty is 92% */
	    	pwm_config(pb->pwm, period, pb->period);
	    	pwm_enable(pb->pwm);
	    }
	#endif
	    
	    if (debug_enable)
    	    printk("_[backlight] brightness=%3d period=%5d max_period=%5d duty=%d \n\n", 
	            brightness, period, pb->period, (period * 100 / pb->period));
    }
#endif
#else
	if (brightness == 0) {
		pwm_config(pb->pwm, 0, pb->period);
		pwm_disable(pb->pwm);
	} else {
		pwm_config(pb->pwm, brightness * pb->period / max, pb->period);
		pwm_enable(pb->pwm);
	}
#endif

	return 0;
}

static int pwm_backlight_get_brightness(struct backlight_device *bl)
{
	return bl->props.brightness;
}

static struct backlight_ops pwm_backlight_ops = {
	.update_status	= pwm_backlight_update_status,
	.get_brightness	= pwm_backlight_get_brightness,
};

static int pwm_backlight_probe(struct platform_device *pdev)
{
	struct platform_pwm_backlight_data *data = pdev->dev.platform_data;
	struct backlight_device *bl;
	struct pwm_bl_data *pb;
	int ret;

	if (!data) {
		dev_err(&pdev->dev, "failed to find platform data\n");
		return -EINVAL;
	}

	if (data->init) {
		ret = data->init(&pdev->dev);
		if (ret < 0)
			return ret;
	}

	pb = kzalloc(sizeof(*pb), GFP_KERNEL);
	if (!pb) {
		dev_err(&pdev->dev, "no memory for state\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

#if 0 /* ktj 2011-1-25 */
	pb->period = data->pwm_period_ns / 3;
#else
	pb->period = data->pwm_period_ns;
#endif
	pb->notify = data->notify;

	pb->pwm = pwm_request(data->pwm_id, "backlight");
	if (IS_ERR(pb->pwm)) {
		dev_err(&pdev->dev, "unable to request PWM for backlight\n");
		ret = PTR_ERR(pb->pwm);
		goto err_pwm;
	} else
		dev_dbg(&pdev->dev, "got pwm for backlight\n");

	bl = backlight_device_register(dev_name(&pdev->dev), &pdev->dev,
			pb, &pwm_backlight_ops);
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bl);
		goto err_bl;
	}

	bl->props.max_brightness = data->max_brightness;
	bl->props.brightness = data->dft_brightness;
	backlight_update_status(bl);

	platform_set_drvdata(pdev, bl);

#if defined(CONFIG_MX100)   
    backlight = bl; /* ktj */ 
#endif

	return 0;

err_bl:
	pwm_free(pb->pwm);
err_pwm:
	kfree(pb);
err_alloc:
	if (data->exit)
		data->exit(&pdev->dev);
	return ret;
}

static int pwm_backlight_remove(struct platform_device *pdev)
{
	struct platform_pwm_backlight_data *data = pdev->dev.platform_data;
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);

	backlight_device_unregister(bl);
	pwm_config(pb->pwm, 0, pb->period);
	pwm_disable(pb->pwm);
	pwm_free(pb->pwm);
	kfree(pb);
	if (data->exit)
		data->exit(&pdev->dev);
	return 0;
}

#ifdef CONFIG_PM
static int pwm_backlight_suspend(struct platform_device *pdev,
				 pm_message_t state)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);

	if (pb->notify)
		pb->notify(0);
	pwm_config(pb->pwm, 0, pb->period);
	pwm_disable(pb->pwm);
	return 0;
}

static int pwm_backlight_resume(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);

	backlight_update_status(bl);
	return 0;
}
#else
#define pwm_backlight_suspend	NULL
#define pwm_backlight_resume	NULL
#endif

#if defined(CONFIG_MX100) /* ktj */
void set_backlight_period(int period)
{
    int duty;
	struct pwm_bl_data *pb = dev_get_drvdata(&backlight->dev);
 
   	pwm_config(pb->pwm, period, pb->period);
  	pwm_enable(pb->pwm);
  	
  	duty = period * 1000 / pb->period;
  	printk("___setbl period = %d max_period = %d duty = %d\n", period, pb->period, duty);  
}
EXPORT_SYMBOL(set_backlight_period);
#endif

static struct platform_driver pwm_backlight_driver = {
	.driver		= {
		.name	= "pwm-backlight",
		.owner	= THIS_MODULE,
	},
	.probe		= pwm_backlight_probe,
	.remove		= pwm_backlight_remove,
	.suspend	= pwm_backlight_suspend,
	.resume		= pwm_backlight_resume,
};

static int __init pwm_backlight_init(void)
{
	return platform_driver_register(&pwm_backlight_driver);
}
module_init(pwm_backlight_init);

static void __exit pwm_backlight_exit(void)
{
	platform_driver_unregister(&pwm_backlight_driver);
}
module_exit(pwm_backlight_exit);

MODULE_DESCRIPTION("PWM based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:pwm-backlight");
