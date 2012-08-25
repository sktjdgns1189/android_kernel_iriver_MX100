/*
 *	mx100 headset jack device detection driver.
 *
 *	Copyright (C) 2011 Iriver, Inc.
 *
 *	Authors:
 *		JH Lim <jaehwan.lim@iriver.com>
 *     2011.03.17 4pole headset support, sendkey support.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; version 2 of the License.
 */

#include <linux/module.h>
#include <linux/sysdev.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/timer.h>
#include <linux/wakelock.h>

#include <mach/hardware.h>
#include <mach/gpio-mx100.h>
#include <mach/gpio-bank.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <asm/gpio.h>
#include <plat/gpio-cfg.h>

#include <linux/io.h>

#include <plat/irqs.h>
#include <asm/mach-types.h>

#include <sound/wm8993.h>

#include <mach/mx100_jack.h>


#ifdef CONFIG_MX100_IOCTL
#include <mach/mx100_ioctl.h>
#endif


#define IRQ_HEADSET_JACK			IRQ_EINT(19)

static struct delayed_work g_delayedwork_headset_detect;

int is_headset_connected(void);

int g_4pole_sendkey_ready = -1;

#ifdef SUPPORT_EXT_4POLE_HEADSET
//#define USING_EXT_MIC_DETECT_TIMER

#define KEYCODE_HEADSETHOOK 248

int HeadsetKeyAllow[] = {
		KEYCODE_HEADSETHOOK
};

static int g_4pole_headset_connected = -1;

//static int g_ext_mic_press_time = -1;
static int g_ext_mic_release_time = -1;



#ifndef USING_EXT_MIC_DETECT_TIMER
#define IRQ_EXT_MIC_DETECT			IRQ_EINT_GROUP(19, 5)
static struct delayed_work g_delayedwork_ext_mic_detect;
#endif
static struct input_dev	 *g_headset_hook_input;
#endif


#ifdef ENABLE_ANDROID_SWITCH_H2W
struct class *jack_class;
EXPORT_SYMBOL(jack_class);
static struct device *jack_selector_fs;			
EXPORT_SYMBOL(jack_selector_fs);

static unsigned int current_jack_type_status = MX100_JACK_NO_DEVICE;

short int get_headset_status(void)
{
//	MX100_JACKDEV_DBG(" headset_status %d", current_jack_type_status);
	return current_jack_type_status;
}
void reset_4pole_sendkey_ready(void)
{
	g_4pole_sendkey_ready = 0;
}

EXPORT_SYMBOL(get_headset_status);


static ssize_t select_jack_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	MX100_DBG(DBG_FILTER_MX100_JACK,"operate nothing\n");

	return 0;
}
static ssize_t select_jack_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
//	int value = 0;
	//int state = gpio_get_value(det_jack->gpio) ^ det_jack->low_active;	

	MX100_DBG(DBG_FILTER_MX100_JACK,"buf = %s", buf);
	MX100_DBG(DBG_FILTER_MX100_JACK,"buf size = %d", sizeof(buf));
	MX100_DBG(DBG_FILTER_MX100_JACK,"buf size = %d", strlen(buf));

	// not implement.
	
	return size;

}
static ssize_t print_switch_name(struct switch_dev *sdev, char *buf);
static ssize_t print_switch_state(struct switch_dev *sdev, char *buf);

struct switch_dev switch_jack_detection = {
		.name = "h2w",
//		.print_name = print_switch_name,
		.print_state = print_switch_state		
};

static ssize_t print_switch_name(struct switch_dev *sdev, char *buf)
{
	MX100_DBG(DBG_FILTER_MX100_JACK,"%s %d\n", "h2w",current_jack_type_status);
	return sprintf(buf, "%s\n", "h2w");
}
static int g_ready_headset_observer = 0;

static ssize_t print_switch_state(struct switch_dev *sdev, char *buf)
{
	MX100_DBG(DBG_FILTER_MX100_JACK,"print_switch_state: %d \n",current_jack_type_status);
	if(g_ready_headset_observer==0) {
		if(current_jack_type_status == MX100_HEADSET_4_POLE_DEVICE) {
			//wm8993_route_set_playback_path(HP4P);
		} else if(current_jack_type_status == MX100_HEADSET_3_POLE_DEVICE) {
			//wm8993_route_set_playback_path(HP3P);
		} else  {
//			wm8993_route_set_playback_path(SPK);
		}
		g_ready_headset_observer = 1;
//		switch_set_state(&switch_jack_detection, 0);			
		switch_set_state(&switch_jack_detection, current_jack_type_status);			
	}
	
	
	return -1;
}

void refresh_h2w(void)
{
	switch_set_state(&switch_jack_detection, 0);			
	switch_set_state(&switch_jack_detection, current_jack_type_status);			
}

struct switch_dev switch_sendend = {
		.name = "send_end",
};

static DEVICE_ATTR(select_jack, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, select_jack_show, select_jack_store);

#endif

/*Shanghai ewada  modify for fm path from 0 to -1 */
int g_headset_connected = -1;
bool		g_btvout;

//static int lowpower = 0;

extern bool get_hdmi_onoff_status(void);
//---------------------------------------------------------------------------------
//---------------------------------------------------------------------------------
irqreturn_t	headset_detect_isr(int irq, void *dev_id)
{
	disable_irq_nosync(IRQ_HEADSET_JACK);

	//	disable_irq(IRQ_HEADSET_JACK);

	schedule_delayed_work(&g_delayedwork_headset_detect, msecs_to_jiffies(0));

//	printk("Headset IRQ occured OK!\n");

	return	IRQ_HANDLED;
}

int is_headset_connected(void)
{

#ifdef CONFIG_MX100_EVM
 if(gpio_get_value(GPIO_EARJACK_IN) ) return 0;
 else return 1;
#elif defined(CONFIG_MX100_WS)
 if(gpio_get_value(GPIO_EARJACK_IN) ) return 1;
 else return 0;
#endif

}

//---------------------------------------------------------------------------------
//---------------------------------------------------------------------------------

int g_frist_headset_connected = -1;

void	mx100_headset_detect(struct work_struct *dwork)
{
//	int err; 

	g_frist_headset_connected = is_headset_connected();

	#ifdef SUPPORT_EXT_4POLE_HEADSET
	if(g_frist_headset_connected==1) {
		g_4pole_sendkey_ready = 0;
		wm8993_enable_extmic_bias(1);   // on connect 4pole headset, enable ext bias.
	} else {
		g_4pole_sendkey_ready = 0;
		wm8993_enable_extmic_bias(0);
	}
	#endif

	msleep(300);
	
	if ((g_headset_connected != is_headset_connected()))
	{		
		if(is_headset_connected()==0 && g_frist_headset_connected==0) {
			g_headset_connected = 0;

			#ifdef ENABLE_ANDROID_SWITCH_H2W	
			current_jack_type_status = MX100_JACK_NO_DEVICE;
			if(dwork)  {
				switch_set_state(&switch_jack_detection, current_jack_type_status);
			}
			#endif
			

			#ifdef SUPPORT_EXT_4POLE_HEADSET

			if(g_4pole_headset_connected==1) {
				g_4pole_headset_connected = 0;
				mx100_printk("4Pole Headset DisConnected!\n");			
			} else {
				mx100_printk("Headset DisConnected!\n");
			}
			wm8993_enable_extmic_bias(0);			
			#endif

//			if(!dwork) {
				wm8993_route_set_h2w_path(H2W_SPEAKER);
//				set_autio_codec_path(PATH_H2W,H2W_SPEAKER);
//			}

			if(g_ready_headset_observer==0 && dwork) {
			//	wm8993_route_set_playback_path(SPK);
			}
			
		}
		else if(is_headset_connected()==1 && g_frist_headset_connected==1) {
			g_headset_connected = 1;

			#ifdef SUPPORT_EXT_4POLE_HEADSET
			if(is_4pole_headset_connected()==1) {
				#ifdef ENABLE_ANDROID_SWITCH_H2W		
				current_jack_type_status = MX100_HEADSET_4_POLE_DEVICE;
				if(dwork)  {
					switch_set_state(&switch_jack_detection, current_jack_type_status);	
				}
				#endif
				mx100_printk("4 pole Headset Connected!\n");
				g_4pole_headset_connected = 1;

				if(g_ready_headset_observer==0 && dwork) {
				//	wm8993_route_set_playback_path(HP4P);
				}				
			} else
			#endif
			
			{
				#ifdef ENABLE_ANDROID_SWITCH_H2W	
				current_jack_type_status = MX100_HEADSET_3_POLE_DEVICE;
				if(dwork)  {
					switch_set_state(&switch_jack_detection, current_jack_type_status);	
				}
				#endif
				mx100_printk("3 pole Headset Connected!\n");

				if(g_ready_headset_observer==0 && dwork) {
					//wm8993_route_set_playback_path(HP3P);
				}	

				#ifdef SUPPORT_EXT_4POLE_HEADSET
				g_4pole_headset_connected = 0;
				#endif
				wm8993_enable_extmic_bias(0);
			}

//			if(!dwork) {
				wm8993_route_set_h2w_path(H2W_HEADSET);
			//	set_autio_codec_path(PATH_H2W,H2W_HEADSET);
//			}
		} else {

		}

	}
	
//	if(!g_btvout) wm8991_set_outpath(g_current_out);
	if(dwork) {
		enable_irq(IRQ_HEADSET_JACK);
	}
}

#ifdef SUPPORT_EXT_4POLE_HEADSET

int is_4pole_headset_connected(void)
{
	int ret;

	 if(gpio_get_value(GPIO_EXT_MIC_SENDKEY_IN) ) {

		ret = 0;
	 }
	 else {

		ret = 1;
	 }

	return ret;	 
}

static int g_first_ext_mic_detect;

void ext_mic_detect_handler(void)
{
	int ext_mic_connected = 0;

	g_first_ext_mic_detect =   is_4pole_headset_connected();

	msleep(50);
	
	ext_mic_connected = is_4pole_headset_connected();

	if(ext_mic_connected!= g_first_ext_mic_detect)   return;

	if(g_4pole_sendkey_ready == 0) {
		g_4pole_sendkey_ready=1;
		return;
	}

	if(g_4pole_headset_connected==0) return;

//	MX100_DBG(DBG_FILTER_MX100_JACK,"\nEXT MIC %d %d %d\n",ext_mic_connected,g_4pole_sendkey_ready,g_4pole_headset_connected);

	if(ext_mic_connected == 1 ) {
		g_ext_mic_release_time=jiffies_to_msecs(jiffies);

		if(g_4pole_sendkey_ready==1)  {

			#ifdef SUPPORT_HEADSET_HOOK_KEY
				input_report_key(g_headset_hook_input ,KEYCODE_HEADSETHOOK, 0);
			    	input_sync(g_headset_hook_input);
				MX100_DBG(DBG_FILTER_MX100_JACK,"Send Key Released!\n");
				MX100_LOG("Send Key Released!\n");
			#endif
		} else {

		}

	} else {
		if(g_4pole_sendkey_ready == 1) {
			#ifdef SUPPORT_HEADSET_HOOK_KEY
				input_report_key(g_headset_hook_input ,KEYCODE_HEADSETHOOK, 1);
				input_sync(g_headset_hook_input);
				MX100_DBG(DBG_FILTER_MX100_JACK,"Send Key Pressed!\n");
				MX100_LOG("Send Key Pressed!\n");
			#endif		
		}
	}

}


#ifdef USING_EXT_MIC_DETECT_TIMER
#define	PERIOD_20MS			(HZ/50)		// 20ms

struct timer_list	g_ext_mic_detect_timer;




void ext_mic_detect_timer_handler(unsigned long data)
{
	ext_mic_detect_handler();
	mod_timer(&g_ext_mic_detect_timer, jiffies + PERIOD_20MS);	
}


void ext_mic_detect_timer_run(void)
{
	init_timer(&g_ext_mic_detect_timer);
	g_ext_mic_detect_timer.function = ext_mic_detect_timer_handler;

	g_ext_mic_detect_timer.expires = jiffies + PERIOD_20MS;

	add_timer(&g_ext_mic_detect_timer);
}
#else

irqreturn_t 	ext_mic_detect_isr(int irq, void *dev_id)
{
	
	disable_irq_nosync(irq);

	schedule_delayed_work(&g_delayedwork_ext_mic_detect, msecs_to_jiffies(0));

	return	IRQ_HANDLED;
}


void mx100_ext_mic_detect(struct work_struct *dwork)
{
	
	s3c_gpio_cfgpin(GPIO_EXT_MIC_SENDKEY_IN, S3C_GPIO_SFN(0x0));
	
	ext_mic_detect_handler();

	s3c_gpio_cfgpin(GPIO_EXT_MIC_SENDKEY_IN, S3C_GPIO_SFN(0xf));

	if(dwork) {
		enable_irq(IRQ_EXT_MIC_DETECT);
	}
}
#endif


#endif
static int mx100_jack_probe(struct platform_device *pdev)
{
	int ret;
//	struct mx100_jack_platform_data *pdata = pdev->dev.platform_data;
	int err;

	if (gpio_is_valid(GPIO_EARJACK_IN))
	{

		err = gpio_request( GPIO_EARJACK_IN, "GPIO_EARJACK_IN");
		if(err)
		{
			MX100_DBG(DBG_FILTER_MX100_JACK, "failed to request GPIO_EARJACK_IN\n");
			return err;
		}
		gpio_direction_input(GPIO_EARJACK_IN);

		#ifdef CONFIG_MX100_EVM
		s3c_gpio_setpull(GPIO_EARJACK_IN, S3C_GPIO_PULL_UP);
		#elif defined(CONFIG_MX100_WS)
		s3c_gpio_setpull(GPIO_EARJACK_IN, S3C_GPIO_PULL_NONE);
		#endif		
		msleep(100);
		
		gpio_free(GPIO_EARJACK_IN);
	}

	else {
		MX100_DBG(DBG_FILTER_MX100_JACK,"\nDEBUG -> IRQ_HEADSET_JACK gpio is invalid!!!\n\n");
	}

	#ifdef CONFIG_MX100_EVM
	err = request_irq(IRQ_HEADSET_JACK, headset_detect_isr,IRQF_DISABLED | IRQ_TYPE_EDGE_FALLING, "hpjack-irq", NULL);
	#elif defined(CONFIG_MX100_WS)
	err = request_irq(IRQ_HEADSET_JACK, headset_detect_isr,IRQF_DISABLED | IRQ_TYPE_EDGE_BOTH, "hpjack-irq", NULL);
	#endif		
	

	if(err) MX100_DBG(DBG_FILTER_MX100_JACK,"\nDEBUG -> IRQ_HEADSET_JACK request error!!!\n\n");

	INIT_DELAYED_WORK(&g_delayedwork_headset_detect, mx100_headset_detect);




	#ifdef SUPPORT_HEADSET_HOOK_KEY

	g_headset_hook_input= input_allocate_device();

	if (!g_headset_hook_input)
	{
		printk(KERN_ERR "mx100 ext jack: Failed to allocate input device.\n");
	} else {
		g_headset_hook_input->name = "mx100 ext jack";
		set_bit(EV_SYN, g_headset_hook_input->evbit);
		set_bit(EV_KEY, g_headset_hook_input->evbit);
		set_bit(KEYCODE_HEADSETHOOK, g_headset_hook_input->keybit);
		g_headset_hook_input->keycode = HeadsetKeyAllow;

		ret = input_register_device(g_headset_hook_input);
		if (ret < 0)
		{
			printk(KERN_ERR "mx100_ext jack: Failed to register driver\n");
		}
	}

	#endif

	#ifdef SUPPORT_EXT_4POLE_HEADSET

	if (gpio_is_valid(GPIO_EXT_MIC_SENDKEY_IN))
	{
		err = gpio_request( GPIO_EXT_MIC_SENDKEY_IN, "GPIO_EXT_MIC_SENDKEY_IN");
		if(err)
		{
			MX100_DBG(DBG_FILTER_MX100_JACK, "failed to request GPIO_EXT_MIC_SENDKEY_IN \n");
			return err;
		}
		gpio_direction_input(GPIO_EXT_MIC_SENDKEY_IN);


		s3c_gpio_setpull(GPIO_EXT_MIC_SENDKEY_IN, S3C_GPIO_PULL_NONE);

		msleep(100);
		
		gpio_free(GPIO_EXT_MIC_SENDKEY_IN);
	}


	#ifdef USING_EXT_MIC_DETECT_TIMER

	ext_mic_detect_timer_run();
	
	#else

	set_irq_type(IRQ_EXT_MIC_DETECT, IRQ_TYPE_EDGE_BOTH); 
	err = request_irq(IRQ_EXT_MIC_DETECT, ext_mic_detect_isr, IRQF_DISABLED, "ext_mic-irq", NULL);
	s3c_gpio_cfgpin(GPIO_EXT_MIC_SENDKEY_IN, S3C_GPIO_SFN(0xf));  //JHLIM  move here. 2011.04.04  : LCD On Off hang
	
	INIT_DELAYED_WORK(&g_delayedwork_ext_mic_detect, mx100_ext_mic_detect);
	#endif

	#endif

	#ifdef ENABLE_ANDROID_SWITCH_H2W
	current_jack_type_status = MX100_JACK_NO_DEVICE;
	
	ret = switch_dev_register(&switch_jack_detection);
	if (ret < 0) 
	{
		MX100_DBG(DBG_FILTER_MX100_JACK, "SEC JACK: Failed to register switch device\n");
	//	goto err_switch_dev_register;
	}

	ret = switch_dev_register(&switch_sendend);
	if (ret < 0)
	{
		MX100_DBG(DBG_FILTER_MX100_JACK, "SEC JACK: Failed to register switch sendend device\n");
		//goto err_switch_dev_register;
	}

	//Create JACK Device file in Sysfs
	jack_class = class_create(THIS_MODULE, "jack");
	if(IS_ERR(jack_class))
	{
		MX100_DBG(DBG_FILTER_MX100_JACK, "Failed to create class(sec_jack)\n");
	}

	jack_selector_fs = device_create(jack_class, NULL, 0, NULL, "jack_selector");
	if (IS_ERR(jack_selector_fs))
		MX100_DBG(DBG_FILTER_MX100_JACK,"Failed to create device(sec_jack)!= %ld\n", IS_ERR(jack_selector_fs));

	if (device_create_file(jack_selector_fs, &dev_attr_select_jack) < 0)
		MX100_DBG(DBG_FILTER_MX100_JACK, "Failed to create device file(%s)!\n", dev_attr_select_jack.attr.name);	

	#endif


//	mx100_headset_detect(0);  remove 2011.05.17 for fix mic source bug.
	#if 0
	if(is_headset_connected() ) {
		wm8993_setpath_headset_play();
	} else {
		wm8993_setpath_speaker_play();
	}
	#endif
	#if 0
	if(get_hdmi_onoff_status()) {
		g_btvout = true;
		wm8991_set_outpath(WM8991_NONE);
	} else {
		g_btvout = false;
		wm8991_set_outpath(g_current_out);
	}
	#endif

	return 0;	
}

static int mx100_jack_remove(struct platform_device *pdev)
{
	#ifdef SUPPORT_EXT_4POLE_HEADSET
	input_unregister_device(g_headset_hook_input);
	
	#ifndef USING_EXT_MIC_DETECT_TIMER
	free_irq(IRQ_EXT_MIC_DETECT, 0);
	#endif

	#endif

	return 0;
}

#ifdef CONFIG_PM
static int mx100_jack_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret;
	
	MX100_DBG(DBG_FILTER_PM, "suspend mx100_jack\n");

	disable_irq(IRQ_HEADSET_JACK);

	ret = cancel_delayed_work_sync(&g_delayedwork_headset_detect);

	#ifdef SUPPORT_EXT_4POLE_HEADSET
	s3c_gpio_cfgpin(GPIO_EXT_MIC_SENDKEY_IN, S3C_GPIO_SFN(0x0));
	disable_irq(IRQ_EXT_MIC_DETECT);
	g_4pole_sendkey_ready = 0;

	ret = cancel_delayed_work_sync(&g_delayedwork_ext_mic_detect);
	#endif


	return 0;
}
static int mx100_jack_resume(struct platform_device *pdev)
{
	MX100_DBG(DBG_FILTER_PM, "resumed mx100_jack\n");


	set_irq_type(IRQ_HEADSET_JACK, IRQ_TYPE_EDGE_BOTH);  /* added 2011.06.22  */

	enable_irq(IRQ_HEADSET_JACK);

	#ifdef SUPPORT_EXT_4POLE_HEADSET
	g_4pole_sendkey_ready = 0;
	set_irq_type(IRQ_EXT_MIC_DETECT, IRQ_TYPE_EDGE_BOTH);  //BUG fix: when goto suspend and exit, it changed.!!!

	enable_irq(IRQ_EXT_MIC_DETECT);
	s3c_gpio_cfgpin(GPIO_EXT_MIC_SENDKEY_IN, S3C_GPIO_SFN(0xf));
	#endif
	
	return 0;
}

#else
#define mx100_jack_resume		NULL
#define mx100_jack_suspend	NULL
#endif

static struct platform_driver mx100_jack_driver = {
	.probe		= mx100_jack_probe,
	.remove		= mx100_jack_remove,
	.suspend		= mx100_jack_suspend,
	.resume		= mx100_jack_resume,
	.driver		= {
		.name		= "mx100_jack",
		.owner		= THIS_MODULE,
	},
};


static char banner[] __initdata = KERN_INFO "mx100 jack Driver, (c) 2010 Iriver INC\n";

static int __init mx100_jack_init(void)
{
	printk(banner);
	return platform_driver_register(&mx100_jack_driver);
}

static void __exit mx100_jack_exit(void)
{
	platform_driver_unregister(&mx100_jack_driver);
}


module_init(mx100_jack_init);
module_exit(mx100_jack_exit);

MODULE_AUTHOR("JH Lim <jaehwan.lim@iriver.com>");
MODULE_DESCRIPTION("mx100 jack detection driver");
MODULE_LICENSE("GPL");
