
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/clk.h>


#include <linux/delay.h>
#include <linux/io.h>
#include <mach/hardware.h>
#include <asm/delay.h>
#include <asm/irq.h>

#include <asm/gpio.h>
#include <plat/gpio-cfg.h>
//#include <plat/regs-gpio.h>
//#include <plat/regs-clock.h>
#include <mach/regs-clock.h>

#include <mach/gpio-bank.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
// Debug message enable flag
//#define	DEBUG_MSG			
//#define	DEBUG_PM_MSG

#include "mx100_keypad.h"
#include "mx100_keycode.h"
#include "mx100_keypad_sysfs.h"

#define DEVICE_NAME "mx100-keypad"

mx100_keypad_t	mx100_keypad;



#ifdef ENABLE_CROSSKEY_ON_CAMERA_KEY

int g_cross_key_mode = 0;

#endif

static void				generate_keycode				(unsigned short prev_key, unsigned short cur_key, int *key_table);
//static void 			mx100_poweroff_timer_run		(void);
//static void 			mx100_poweroff_timer_handler	(unsigned long data);

static int				mx100_keypad_get_data			(void);
static void				mx100_keypad_port_init			(void);
static void 			mx100_keypad_control			(void);

static void				mx100_rd_timer_handler			(unsigned long data);

static int              mx100_keypad_open              (struct input_dev *dev);
static void             mx100_keypad_close             (struct input_dev *dev);

static void             mx100_keypad_release_device    (struct device *dev);
static int              mx100_keypad_resume            (struct platform_device *dev);
static int              mx100_keypad_suspend           (struct platform_device *dev, pm_message_t state);

static void				mx100_keypad_config			(unsigned char state);

static int __devinit    mx100_keypad_probe				(struct platform_device *pdev);
static int __devexit    mx100_keypad_remove			(struct platform_device *pdev);

static int __init       mx100_keypad_init				(void);
static void __exit      mx100_keypad_exit				(void);

static struct platform_driver mx100_platform_device_driver = {
		.probe          = mx100_keypad_probe,
		.remove         = mx100_keypad_remove,
		.suspend        = mx100_keypad_suspend,
		.resume         = mx100_keypad_resume,
		.driver		= {
			.owner	= THIS_MODULE,
			.name	= DEVICE_NAME,
		},
};

static struct platform_device mx100_platform_device = {
        .name           = DEVICE_NAME,
        .id             = -1,
        .num_resources  = 0,
        .dev    = {
                .release	= mx100_keypad_release_device,
        },
};
#if 0  /* JHLIM  remove warning  */
static void mx100_poweroff_timer_run(void)
{
	init_timer(&mx100_keypad.poweroff_timer);
	mx100_keypad.poweroff_timer.function = mx100_poweroff_timer_handler;

	switch(mx100_keypad.poweroff_time)	{
		default	:
			mx100_keypad.poweroff_time = 0;
		case	0:
			mx100_keypad.poweroff_timer.expires = jiffies + PERIOD_1SEC;
			break;
		case	1:
			mx100_keypad.poweroff_timer.expires = jiffies + PERIOD_3SEC;
			break;
		case	2:
			mx100_keypad.poweroff_timer.expires = jiffies + PERIOD_5SEC;
			break;
	}
	add_timer(&mx100_keypad.poweroff_timer);
}
#endif

#if 0 /* remove warning */
static void mx100_poweroff_timer_handler(unsigned long data)
{
	POWER_OFF_ENABLE();
	
 	//gpio_set_value(POWER_LED_PORT, 0);	
	gpio_set_value(POWER_CONTROL_PORT, 0);		
	
}
#endif

#ifdef INSTEAD_VOL_MENU
extern int g_headset_connected;
#endif

static void	generate_keycode(unsigned short prev_key, unsigned short cur_key, int *key_table)
{
	unsigned short 	press_key, release_key, i;
	
	press_key	= (cur_key ^ prev_key) & cur_key;
	release_key	= (cur_key ^ prev_key) & prev_key;
	
	i = 0;
	while(press_key)	{
		if(press_key & 0x01)	{
			if(mx100_keypad.hold_state)	{
				
				if(key_table[i] == KEY_POWER)	{
				//		input_report_key(mx100_keypad.driver, key_table[i], KEY_PRESS);					
				}
				else	{	
				//	input_report_switch(mx100_keypad.driver, SW_KEY_HOLD, ON);
				}
				
				
			}
			else			
			{
				#ifdef ENABLE_CROSSKEY_ON_CAMERA_KEY
				if(key_table[i] == KEY_CAMERA) {
					
					if(g_cross_key_mode==1) g_cross_key_mode = 0;
					else g_cross_key_mode = 	1;
					
					if(g_cross_key_mode == 1) {
						printk("\nCross Key Mode\n");
					} else {
						printk("\nNormal Key Mode\n");
					}
					
				} else {
					if(g_cross_key_mode  == 1) {
						input_report_key(mx100_keypad.driver, MX100_Keycode_Shift[i], KEY_PRESS);
						#if defined(DEBUG_MSG)
						printk("PRESS KEY :%d %d %s",i,MX100_Keycode_Shift[i], (char *)MX100_KeyMapStr_Shift[i ]);
						#endif

					} else {
						input_report_key(mx100_keypad.driver, key_table[i], KEY_PRESS);
						#if defined(DEBUG_MSG)
						printk("PRESS KEY :%d %d %s",i, key_table[i], (char *)MX100_KeyMapStr[i ]);							
						#endif
					}
				}
				#else

				#ifdef INSTEAD_VOL_MENU
				if(1) {
					input_report_key(mx100_keypad.driver, MX100_Keycode2[i], KEY_PRESS);
				} else 
				#endif
				{
					input_report_key(mx100_keypad.driver, key_table[i], KEY_PRESS);
				}

				#if defined(DEBUG_MSG)
				printk("PRESS KEY :%d %d %s",i, key_table[i], (char *)MX100_KeyMapStr[i ]);							
				#endif
				
				#endif
			
			}

			#ifdef ENABLE_POWER_LONG_KEY
			// POWER OFF PRESS
			if(key_table[i] == KEY_POWER)	mx100_poweroff_timer_run();
			#endif
		}
		i++;	press_key >>= 1;
	}
	
	i = 0;
	while(release_key)	{
		if(release_key & 0x01)	{
			if(mx100_keypad.hold_state)	{
//				if(key_table[i] == KEY_POWER)	input_report_key(mx100_keypad.driver, key_table[i], KEY_RELEASE);
	//			else							input_report_switch(mx100_keypad.driver, SW_KEY_HOLD, OFF);
			}
			else	
			{
				#ifdef ENABLE_CROSSKEY_ON_CAMERA_KEY
				if(g_cross_key_mode==1) {
					input_report_key(mx100_keypad.driver, MX100_Keycode_Shift[i], KEY_RELEASE);
					#if defined(DEBUG_MSG)
					printk("RELEASE KEY :%d %d %s",i,MX100_Keycode_Shift[i], (char *)MX100_KeyMapStr_Shift[i ]);
					#endif
					
				} else {
					input_report_key(mx100_keypad.driver, key_table[i], KEY_RELEASE);
					#if defined(DEBUG_MSG)
					printk("RELEASE KEY :%d %d %s",i, key_table[i], (char *)MX100_KeyMapStr[i ]);
					#endif

				}

				#else

				#ifdef INSTEAD_VOL_MENU
				if(1) {
					input_report_key(mx100_keypad.driver, MX100_Keycode2[i], KEY_RELEASE);
				} else 
				#endif
				{
					input_report_key(mx100_keypad.driver, key_table[i], KEY_RELEASE);
				}

				#if defined(DEBUG_MSG)
				printk("RELEASE KEY :%d %d %s",i, key_table[i], (char *)MX100_KeyMapStr[i ]);
				#endif

				#endif
			}

			#ifdef ENABLE_POWER_LONG_KEY
			// POWER OFF (LONG PRESS)	
			if(key_table[i] == KEY_POWER)		del_timer_sync(&mx100_keypad.poweroff_timer);
			#endif
			
		}
		i++;	release_key >>= 1;
	}
}

#if defined(DEBUG_MSG)
	static void debug_keycode_printf(unsigned short prev_key, unsigned short cur_key, char **key_table)
	{
		unsigned short 	press_key, release_key, i;
		
		press_key	= (cur_key ^ prev_key) & cur_key;
		release_key	= (cur_key ^ prev_key) & prev_key;
		
		i = 0;
		while(press_key)	{
				if(press_key & 0x01)	printk("PRESS KEY :%d %s",i, (char *)key_table[i ]);
			i++;		
			press_key >>= 1;
		}
		
		i = 0;
		while(release_key)	{
				if(release_key & 0x01)	printk("RELEASE KEY :%d %s",i,(char *)key_table[i]);
			i++;					release_key >>= 1;
		}
	}
#endif

static void	mx100_keypad_port_init(void)
{
//int err;
	
//mx100evm1 keypad port.
//GPH2[5] : HOME
//GPH2[6] : MENU
//GPH2[7] :BACK
//GPH3[0] : VOL+
//GPH3[4] : VOL-
//
//	gpio_set_value(GPIO_BACK_KEY_IN, 0);

#ifdef CONFIG_MX100_EVM
	gpio_request(GPIO_BACK_KEY_IN, "GPH0");
        gpio_direction_input(GPIO_BACK_KEY_IN);
        s3c_gpio_setpull(GPIO_BACK_KEY_IN, S3C_GPIO_PULL_UP);	

//	gpio_set_value(GPIO_MENU_KEY_IN, 0);
	gpio_request(GPIO_MENU_KEY_IN, "GPH0");
        gpio_direction_input(GPIO_MENU_KEY_IN);
        s3c_gpio_setpull(GPIO_MENU_KEY_IN, S3C_GPIO_PULL_UP);	

//	gpio_set_value(GPIO_HOME_KEY_IN, 0);
	gpio_request(GPIO_HOME_KEY_IN, "GPH0");
        gpio_direction_input(GPIO_HOME_KEY_IN);
        s3c_gpio_setpull(GPIO_HOME_KEY_IN, S3C_GPIO_PULL_UP);	

//	gpio_set_value(GPIO_VOLPLUS_KEY_IN, 0);
	gpio_request(GPIO_VOLPLUS_KEY_IN, "GPH0");
        gpio_direction_input(GPIO_VOLPLUS_KEY_IN);
        s3c_gpio_setpull(GPIO_VOLPLUS_KEY_IN, S3C_GPIO_PULL_UP);	

//	gpio_set_value(GPIO_VOLMINUS_KEY_IN, 0);
	gpio_request(GPIO_VOLMINUS_KEY_IN, "GPH0");
        gpio_direction_input(GPIO_VOLMINUS_KEY_IN);
        s3c_gpio_setpull(GPIO_VOLMINUS_KEY_IN, S3C_GPIO_PULL_UP);	

	#ifdef ENABLE_KEY_CAMERA
	gpio_request(GPIO_CAM_HALF_SHUTTOR_KEY_IN, "GPH0");
        s3c_gpio_setpull(GPIO_CAM_HALF_SHUTTOR_KEY_IN, S3C_GPIO_PULL_UP);	
        gpio_direction_output(GPIO_CAM_HALF_SHUTTOR_KEY_IN,1);
        gpio_direction_input(GPIO_CAM_HALF_SHUTTOR_KEY_IN);
	#endif

#elif defined(CONFIG_MX100_WS)
	gpio_request(GPIO_VOLPLUS_KEY_IN, "GPH0");
        gpio_direction_input(GPIO_VOLPLUS_KEY_IN);
        s3c_gpio_setpull(GPIO_VOLPLUS_KEY_IN, S3C_GPIO_PULL_UP);	

	gpio_request(GPIO_VOLMINUS_KEY_IN, "GPH0");
        gpio_direction_input(GPIO_VOLMINUS_KEY_IN);
        s3c_gpio_setpull(GPIO_VOLMINUS_KEY_IN, S3C_GPIO_PULL_UP);		
#endif
	
}

static int	mx100_keypad_get_data(void)
{
	int		key_data = 0;
//	int gph2dat,gph3dat,gph0dat;
	
#ifdef CONFIG_MX100_EVM

	key_data |= gpio_get_value(GPIO_BACK_KEY_IN);   // BACK
	key_data |= gpio_get_value(GPIO_MENU_KEY_IN) << 1 ;  // MENU
	key_data |= gpio_get_value(GPIO_HOME_KEY_IN) << 2;  // HOME
	key_data |= gpio_get_value(GPIO_VOLPLUS_KEY_IN) << 3;  // VOL+
	key_data |= gpio_get_value(GPIO_POWER_KEY_IN)	 << 4;  // POWER
	key_data |= gpio_get_value(GPIO_VOLMINUS_KEY_IN) << 5;  // VOL-
	#ifdef ENABLE_KEY_CAMERA
	key_data |= gpio_get_value(GPIO_CAM_HALF_SHUTTOR_KEY_IN)	 << 6;  // CAMERA
	#endif
#elif defined(CONFIG_MX100_WS)
	key_data |= gpio_get_value(GPIO_VOLPLUS_KEY_IN);  // VOL+
	key_data |= gpio_get_value(GPIO_POWER_KEY_IN)	 << 1;  // POWER
	key_data |= gpio_get_value(GPIO_VOLMINUS_KEY_IN) << 2;  // VOL-
#endif	
	key_data = ~key_data;

	return	key_data;
}


#if 0 //janged
extern int get_start_sleep_state(void);
static void mx100_keypad_control(void)
{
	static	unsigned short	prev_keypad_data = 0, cur_keypad_data = 0;

	// key data process
	cur_keypad_data = mx100_keypad_get_data();

	if(prev_keypad_data ==0 ) 
		prev_keypad_data = cur_keypad_data;

	if(prev_keypad_data != cur_keypad_data) {
		if(!get_start_sleep_state()) {
			generate_keycode(prev_keypad_data, cur_keypad_data, &MX100_Keycode[0]);
			prev_keypad_data = cur_keypad_data;
			input_sync(mx100_keypad.driver);
		} else {
			printk("[KERNEL] keypad ignored by sleep state ++++++++++++\n");
		}
	}
}
#else
static void mx100_keypad_control(void)
{
	static	unsigned short	prev_keypad_data = 0, cur_keypad_data = 0;

	// key data process
	cur_keypad_data = mx100_keypad_get_data();

	if(prev_keypad_data ==0 ) 
	    prev_keypad_data = cur_keypad_data;

	if(prev_keypad_data != cur_keypad_data)	{
		generate_keycode(prev_keypad_data, cur_keypad_data, &MX100_Keycode[0]);
		prev_keypad_data = cur_keypad_data;
//		input_sync(mx100_keypad.driver);
	}
}
#endif


static void	mx100_rd_timer_handler(unsigned long data)
{
    unsigned long flags;

    local_irq_save(flags);

	if(mx100_keypad.wakeup_delay > KEYPAD_WAKEUP_DELAY)	mx100_keypad_control();	
	else													mx100_keypad.wakeup_delay++;
		
	// Kernel Timer restart
	switch(mx100_keypad.sampling_rate)	{
		default	:
			mx100_keypad.sampling_rate = 0;
		case	0:
			mod_timer(&mx100_keypad.rd_timer,jiffies + PERIOD_10MS);
			break;
		case	1:
			mod_timer(&mx100_keypad.rd_timer,jiffies + PERIOD_20MS);
			break;
		case	2:
			mod_timer(&mx100_keypad.rd_timer,jiffies + PERIOD_50MS);
			break;
	}

    local_irq_restore(flags);
}

static int	mx100_keypad_open(struct input_dev *dev)
{
	#if	defined(DEBUG_MSG)
		printk("%s\n", __FUNCTION__);
	#endif
	
	return	0;
}

static void	mx100_keypad_close(struct input_dev *dev)
{
	#if	defined(DEBUG_MSG)
		printk("%s\n", __FUNCTION__);
	#endif
}

static void	mx100_keypad_release_device(struct device *dev)
{
	#if	defined(DEBUG_MSG)
		printk("%s\n", __FUNCTION__);
	#endif
}

/* ktj process_wkup_src */
extern void set_request_screenon(int value);
extern int get_request_screenon(void);

static int	mx100_keypad_resume(struct platform_device *dev)
{
	#if	defined(DEBUG_PM_MSG)
		printk("%s\n", __FUNCTION__);
	#endif

printk("__%s start \n", __func__);
	
	mx100_keypad_config(KEYPAD_STATE_RESUME);

#if 1 /* ktj process_wkup_src */
    if (get_request_screenon()) 
    { 
	    input_report_key(mx100_keypad.driver, KEY_POWER, KEY_PRESS);					
    	udelay(5);
    	input_report_key(mx100_keypad.driver, KEY_POWER, KEY_RELEASE);					
		set_request_screenon(0);
//	    input_sync(mx100_keypad.driver);
	}
#else	
    input_report_key(mx100_keypad.driver, KEY_POWER, KEY_PRESS);		//janged			
    input_report_key(mx100_keypad.driver, KEY_POWER, KEY_RELEASE);					
	input_sync(mx100_keypad.driver);
#endif
	
	mx100_keypad.wakeup_delay = 0;

printk("__%s done \n", __func__);

    return  0;
}

static int	mx100_keypad_suspend(struct platform_device *dev, pm_message_t state)
{
	#if	defined(DEBUG_PM_MSG)
		printk("%s\n", __FUNCTION__);
	#endif

	del_timer_sync(&mx100_keypad.rd_timer);
	
    return  0;
}

static void	mx100_keypad_config(unsigned char state)
{
	if(state == KEYPAD_STATE_BOOT)	{
		mx100_keypad_port_init();	// GPH2, GPH3 All Input, All Pull up

		if(gpio_request(GPIO_POWER_KEY_IN, POWER_CONTROL_STR))	{
			printk("%s(%s) : request port error!\n", __FUNCTION__, POWER_CONTROL_STR);
		}

		s3c_gpio_setpull(GPIO_POWER_KEY_IN, S3C_GPIO_PULL_UP);	
	       	gpio_direction_output(GPIO_POWER_KEY_IN,1);
	        gpio_direction_input(GPIO_POWER_KEY_IN);
	}

	/* Scan timer init */
	init_timer(&mx100_keypad.rd_timer);

	mx100_keypad.rd_timer.function = mx100_rd_timer_handler;
	mx100_keypad.rd_timer.expires = jiffies + (HZ/100);

	add_timer(&mx100_keypad.rd_timer);
}

static int __devinit    mx100_keypad_probe(struct platform_device *pdev)
{
    int 	key, code, rc;

	// struct init
	memset(&mx100_keypad, 0x00, sizeof(mx100_keypad_t));
	
	// create sys_fs
	if((rc = mx100_keypad_sysfs_create(pdev)))	{
		printk("%s : sysfs_create_group fail!!\n", __FUNCTION__);
		return	rc;
	}

	mx100_keypad.driver = input_allocate_device();
	
    if(!(mx100_keypad.driver))	{
		printk("ERROR! : %s input_allocate_device() error!!! no memory!!\n", __FUNCTION__);
		mx100_keypad_sysfs_remove(pdev);
		return -ENOMEM;
    }

	set_bit(EV_KEY, mx100_keypad.driver->evbit);
//	set_bit(EV_REP, mx100_keypad.driver->evbit);	// Repeat Key

	set_bit(EV_SW, mx100_keypad.driver->evbit);

	set_bit(SW_HDMI 			& SW_MAX, mx100_keypad.driver->swbit);
	set_bit(SW_KEY_HOLD			& SW_MAX, mx100_keypad.driver->swbit);
	set_bit(SW_SCREEN_ROTATE 	& SW_MAX, mx100_keypad.driver->swbit);


	for(key = 0; key < sizeof(MX100_KeycodeAllow) / 4; key++){
		code = MX100_KeycodeAllow[key];
		if(code<=0)
			continue;
		set_bit(code & KEY_MAX, mx100_keypad.driver->keybit);
	}

	mx100_keypad.driver->name 	= DEVICE_NAME;
	mx100_keypad.driver->phys 	= DEVICE_NAME "/input0";
    	mx100_keypad.driver->open 	= mx100_keypad_open;
   	mx100_keypad.driver->close	= mx100_keypad_close;
	
	mx100_keypad.driver->id.bustype	= BUS_HOST;
	mx100_keypad.driver->id.vendor 	= 0x16B4;
	mx100_keypad.driver->id.product 	= 0x0701;
	mx100_keypad.driver->id.version 	= 0x0001;

	mx100_keypad.driver->keycode = MX100_KeycodeAllow;
	
	if(input_register_device(mx100_keypad.driver))	{
		printk("mx100 keypad input register device fail!!\n");

		mx100_keypad_sysfs_remove(pdev);
		input_free_device(mx100_keypad.driver);	return	-ENODEV;
	}

	mx100_keypad_config(KEYPAD_STATE_BOOT);

	printk("--------------------------------------------------------\n");
	printk("mx100 Keypad driver initialized\n");
	printk("--------------------------------------------------------\n");

    return 0;
}

static int __devexit    mx100_keypad_remove(struct platform_device *pdev)
{
	mx100_keypad_sysfs_remove(pdev);

	input_unregister_device(mx100_keypad.driver);
	
	del_timer_sync(&mx100_keypad.rd_timer);

	//gpio_free(POWER_LED_PORT);	
	#if 1  // jhlim
	gpio_free(GPIO_POWER_KEY_IN);
	#endif
	
	#if	defined(DEBUG_MSG)
		printk("%s\n", __FUNCTION__);
	#endif
	
	return  0;
}

static int __init	mx100_keypad_init(void)
{
	int ret = platform_driver_register(&mx100_platform_device_driver);
	
	#if	defined(DEBUG_MSG)
		printk("%s\n", __FUNCTION__);
	#endif
	
	if(!ret)        {
		ret = platform_device_register(&mx100_platform_device);
		
		#if	defined(DEBUG_MSG)
			printk("platform_driver_register %d \n", ret);
		#endif
		
		if(ret)	platform_driver_unregister(&mx100_platform_device_driver);
	}
	return ret;
}

static void __exit	mx100_keypad_exit(void)
{
	#if	defined(DEBUG_MSG)
		printk("%s\n",__FUNCTION__);
	#endif
	
	platform_device_unregister(&mx100_platform_device);
	platform_driver_unregister(&mx100_platform_device_driver);
}

module_init(mx100_keypad_init);
module_exit(mx100_keypad_exit);

MODULE_AUTHOR("iriver");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Keypad interface for mx100.");
