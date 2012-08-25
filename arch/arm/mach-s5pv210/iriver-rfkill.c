/*
 * Copyright (C) 2008 Google, Inc.
 * Author: Nick Pelly <npelly@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* Control bluetooth power for iriver platform */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/rfkill.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/wakelock.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <mach/gpio.h>
#include <mach/hardware.h>
#include <plat/gpio-cfg.h>
#include <plat/irqs.h>
#include <linux/io.h>
#include <mach/dma.h>
#include <plat/regs-otg.h>
#include <mach/regs-gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio-bank.h>

#ifdef CONFIG_S5P_LPAUDIO
#include <mach/cpuidle.h>
#endif /* CONFIG_S5P_LPAUDIO */

/*----------------------------------------------------------------------------------*/
//  #define rdebug(fmt,arg...) printk(KERN_CRIT "___[iriver-rfkill] " fmt ,## arg)
    #define rdebug(fmt,arg...)
/*----------------------------------------------------------------------------------*/


#define IRQ_BT_HOST_WAKE      IRQ_EINT10

static struct wake_lock rfkill_wake_lock;
static  struct hrtimer bt_lpm_timer;
static ktime_t bt_lpm_delay;

#ifndef	GPIO_LEVEL_LOW
#define GPIO_LEVEL_LOW		0
#define GPIO_LEVEL_HIGH		1
#endif

#define GPIO_BT_WAKE          	S5PV210_GPJ0(2)
#define GPIO_BT_nRST            S5PV210_GPJ0(6)
#define GPIO_WLAN_BT_EN         S5PV210_GPJ2(3)
#define GPIO_WLAN_nRST          S5PV210_GPJ0(5)

#define GPIO_BT_RXD             S5PV210_GPA0(0)
#define GPIO_BT_RXD_AF          2
#define GPIO_BT_TXD             S5PV210_GPA0(1)
#define GPIO_BT_TXD_AF          2
#define GPIO_BT_CTS             S5PV210_GPA0(2)
#define GPIO_BT_CTS_AF          2
#define GPIO_BT_RTS             S5PV210_GPA0(3)
#define GPIO_BT_RTS_AF          2

#define GPIO_BT_HOST_WAKE       S5PV210_GPH1(2)
#define GPIO_BT_HOST_WAKE_AF    0xF

#ifdef CONFIG_MX100_REV_ES
#define GPIO_WLAN_BT_CLK_EN     S5PV210_GPJ0(1)
#endif

void rfkill_switch_all(enum rfkill_type type, enum rfkill_user_states state);

void s3c_setup_uart_cfg_gpio(unsigned char port)
{
	switch(port)
	{
	case 0:
		s3c_gpio_cfgpin(GPIO_BT_RXD, S3C_GPIO_SFN(GPIO_BT_RXD_AF));
		s3c_gpio_setpull(GPIO_BT_RXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_BT_TXD, S3C_GPIO_SFN(GPIO_BT_TXD_AF));
		s3c_gpio_setpull(GPIO_BT_TXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_BT_CTS, S3C_GPIO_SFN(GPIO_BT_CTS_AF));
		s3c_gpio_setpull(GPIO_BT_CTS, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_BT_RTS, S3C_GPIO_SFN(GPIO_BT_RTS_AF));
		s3c_gpio_setpull(GPIO_BT_RTS, S3C_GPIO_PULL_NONE);

        	s3c_gpio_cfgpin(GPIO_BT_WAKE, S3C_GPIO_SFN(0x1));
        	s3c_gpio_setpull(GPIO_BT_WAKE, S3C_GPIO_PULL_NONE);
        	s3c_gpio_cfgpin(GPIO_BT_HOST_WAKE, S3C_GPIO_SFN(0xF));
   		s3c_gpio_setpull(GPIO_BT_HOST_WAKE, S3C_GPIO_PULL_NONE);

#if 0		
		s3c_gpio_slp_cfgpin(GPIO_BT_RXD, S3C_GPIO_SLP_PREV);
		s3c_gpio_slp_setpull_updown(GPIO_BT_RXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_slp_cfgpin(GPIO_BT_TXD, S3C_GPIO_SLP_PREV);
		s3c_gpio_slp_setpull_updown(GPIO_BT_TXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_slp_cfgpin(GPIO_BT_CTS, S3C_GPIO_SLP_PREV);
		s3c_gpio_slp_setpull_updown(GPIO_BT_CTS, S3C_GPIO_PULL_NONE);
		s3c_gpio_slp_cfgpin(GPIO_BT_RTS, S3C_GPIO_SLP_PREV);
		s3c_gpio_slp_setpull_updown(GPIO_BT_RTS, S3C_GPIO_PULL_NONE);
#endif		
		break;
    }
}

static struct rfkill *bt_rfk;
static const char bt_name[] = "bcm4329";

static bool lpaudio_lock = 0;
static bool bt_init_complete = 0;

static unsigned int bt_uart_off_table[][4] = {
	{GPIO_BT_RXD, 1, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_BT_TXD, 1, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_BT_CTS, 1, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_BT_RTS, 1, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
};

void bt_config_gpio_table(int array_size, int (*gpio_table)[4])
{
	u32 i, gpio;

	for (i = 0; i < array_size; i++) {
		gpio = gpio_table[i][0];
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(gpio_table[i][1]));
		s3c_gpio_setpull(gpio, gpio_table[i][3]);
		if (gpio_table[i][2] != GPIO_LEVEL_NONE)
			gpio_set_value(gpio, gpio_table[i][2]);
	}
}

void bt_uart_rts_ctrl(int flag)
{
	unsigned int gpa0_bt_con;
	unsigned int gpa0_bt_pud;
	unsigned int gpa0_bt_dat;
	
	if(flag)
	{
		// BT RTS Set to HIGH
		s3c_gpio_cfgpin(S5PV210_GPA0(3), S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(S5PV210_GPA0(3), S3C_GPIO_PULL_NONE);
		gpa0_bt_dat = __raw_readl(S5PV210_GPA0DAT);
		gpa0_bt_dat |= (1 << 3);
		__raw_writel(gpa0_bt_dat, S5PV210_GPA0DAT);

		gpa0_bt_con = __raw_readl(S5PV210_GPA0CONPDN);
		gpa0_bt_con |= (1 << 6);
		gpa0_bt_con &= ~(1 << 7);
		__raw_writel(gpa0_bt_con, S5PV210_GPA0CONPDN);

		gpa0_bt_pud = __raw_readl(S5PV210_GPA0PUDPDN);
		gpa0_bt_pud &= ~(1 << 7 | 1 << 6);
		__raw_writel(gpa0_bt_pud, S5PV210_GPA0PUDPDN);
	}
	else
	{
		// BT RTS Set to LOW
		s3c_gpio_cfgpin(S5PV210_GPA0(3), S3C_GPIO_OUTPUT);
		gpa0_bt_dat = __raw_readl(S5PV210_GPA0DAT);
		gpa0_bt_dat &= ~(1 << 3);
		__raw_writel(gpa0_bt_dat, S5PV210_GPA0DAT);

		s3c_gpio_cfgpin(S5PV210_GPA0(3), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPA0(3), S3C_GPIO_PULL_NONE);
	}
}
EXPORT_SYMBOL(bt_uart_rts_ctrl);

static int bluetooth_set_power(void *data, enum rfkill_user_states state)
{
	int ret = 0; 
	int irq;
	irq = IRQ_BT_HOST_WAKE;	
	switch (state) {

		case RFKILL_USER_STATE_UNBLOCKED:
			printk("BT Device Powering ON \n");			

        #ifdef CONFIG_MX100_REV_ES
			if (gpio_is_valid(GPIO_WLAN_BT_CLK_EN))
			{
				ret = gpio_request(GPIO_WLAN_BT_CLK_EN, "GPJ0");
				if (ret < 0) {
					printk("Failed to request GPIO_WLAN_BT_CLK_EN!\n");
					return ret;
				}
				gpio_direction_output(GPIO_WLAN_BT_CLK_EN, GPIO_LEVEL_HIGH);
			}

            s3c_gpio_setpull(GPIO_WLAN_BT_CLK_EN, S3C_GPIO_PULL_NONE);
            gpio_set_value(GPIO_WLAN_BT_CLK_EN, GPIO_LEVEL_HIGH);
        
   			s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_CLK_EN, S3C_GPIO_SLP_OUT1);  
			s3c_gpio_slp_setpull_updown(GPIO_WLAN_BT_CLK_EN, S3C_GPIO_PULL_NONE);
        #endif

			s3c_setup_uart_cfg_gpio(0);

			if (gpio_is_valid(GPIO_WLAN_BT_EN))
			{
				ret = gpio_request(GPIO_WLAN_BT_EN, "GPJ2");
				if (ret < 0) {
					printk("Failed to request GPIO_WLAN_BT_EN!\n");
					return ret;
				}
				gpio_direction_output(GPIO_WLAN_BT_EN, GPIO_LEVEL_HIGH);
			}

			if (gpio_is_valid(GPIO_BT_nRST))
			{
				ret = gpio_request(GPIO_BT_nRST, "GPJ0");
				if (ret < 0) {
					gpio_free(GPIO_WLAN_BT_EN);
					printk("Failed to request GPIO_BT_nRST\n");
					return ret;			
				}
				gpio_direction_output(GPIO_BT_nRST, GPIO_LEVEL_LOW);
			}
			rdebug("GPIO_BT_nRST = %d\n", gpio_get_value(GPIO_BT_nRST));

			/* Set GPIO_BT_WLAN_REG_ON high */ 
			s3c_gpio_setpull(GPIO_WLAN_BT_EN, S3C_GPIO_PULL_NONE);
			gpio_set_value(GPIO_WLAN_BT_EN, GPIO_LEVEL_HIGH);

			s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_SLP_OUT1);  
			s3c_gpio_slp_setpull_updown(GPIO_WLAN_BT_EN, S3C_GPIO_PULL_NONE);

			rdebug("GPIO_WLAN_BT_EN = %d\n", gpio_get_value(GPIO_WLAN_BT_EN));		
			/*FIXME sleep should be enabled disabled since the device is not booting 
			 * 			if its enabled*/
			msleep(100);  // 100msec, delay  between reg_on & rst. (bcm4329 powerup sequence)

			/* Set GPIO_BT_nRST high */
			s3c_gpio_setpull(GPIO_BT_nRST, S3C_GPIO_PULL_NONE);
			gpio_set_value(GPIO_BT_nRST, GPIO_LEVEL_HIGH);

			s3c_gpio_slp_cfgpin(GPIO_BT_nRST, S3C_GPIO_SLP_OUT1);
			s3c_gpio_slp_setpull_updown(GPIO_BT_nRST, S3C_GPIO_PULL_NONE);

			rdebug("GPIO_BT_nRST = %d\n", gpio_get_value(GPIO_BT_nRST));

			gpio_free(GPIO_BT_nRST);
			gpio_free(GPIO_WLAN_BT_EN);
        #ifdef CONFIG_MX100_REV_ES
            gpio_free(GPIO_WLAN_BT_CLK_EN);
        #endif
			ret = enable_irq_wake(irq);
                	if (ret < 0)
                        	pr_err("[BT] set wakeup src failed\n");
                	enable_irq(irq);
			break;

		case RFKILL_USER_STATE_SOFT_BLOCKED:
			printk("BT Device Powering OFF \n");
//			s3c_reset_uart_cfg_gpio(0);

			s3c_gpio_setpull(GPIO_BT_nRST, S3C_GPIO_PULL_NONE);
			gpio_set_value(GPIO_BT_nRST, GPIO_LEVEL_LOW);

			s3c_gpio_slp_cfgpin(GPIO_BT_nRST, S3C_GPIO_SLP_OUT0);
			s3c_gpio_slp_setpull_updown(GPIO_BT_nRST, S3C_GPIO_PULL_NONE);

			rdebug("GPIO_BT_nRST = %d\n",gpio_get_value(GPIO_BT_nRST));

			if(gpio_get_value(GPIO_WLAN_nRST) == 0)
			{		
				s3c_gpio_setpull(GPIO_WLAN_BT_EN, S3C_GPIO_PULL_NONE);
				gpio_set_value(GPIO_WLAN_BT_EN, GPIO_LEVEL_LOW);

				s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_SLP_OUT0);
				s3c_gpio_slp_setpull_updown(GPIO_WLAN_BT_EN, S3C_GPIO_PULL_NONE);

				rdebug("GPIO_WLAN_BT_EN = %d\n", gpio_get_value(GPIO_WLAN_BT_EN));

            #ifdef CONFIG_MX100_REV_ES
                s3c_gpio_setpull(GPIO_WLAN_BT_CLK_EN, S3C_GPIO_PULL_NONE);
                gpio_set_value(GPIO_WLAN_BT_CLK_EN, GPIO_LEVEL_LOW);
    
       			s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_CLK_EN, S3C_GPIO_SLP_OUT0);  
    			s3c_gpio_slp_setpull_updown(GPIO_WLAN_BT_CLK_EN, S3C_GPIO_PULL_NONE);
            #endif
			}

			bt_config_gpio_table(ARRAY_SIZE(bt_uart_off_table), bt_uart_off_table);

			gpio_free(GPIO_BT_nRST);
			gpio_free(GPIO_WLAN_BT_EN);
        #ifdef CONFIG_MX100_REV_ES
			gpio_free(GPIO_WLAN_BT_CLK_EN);
        #endif
                	ret = disable_irq_wake(irq);
                	if (ret < 0)
                        	pr_err("[BT] unset wakeup src failed\n");
                	disable_irq(irq);
			break;

		default:
			rdebug("Bad bluetooth rfkill state %d\n", state);
	}

	return 0;
}


static void bt_host_wake_work_func(struct work_struct *ignored)
{
        int irq;
        irq = IRQ_BT_HOST_WAKE;
        wake_lock_timeout(&rfkill_wake_lock, 5*HZ);

/*  ktj remove useless code
	if(bt_init_complete && !lpaudio_lock)
	{
		s5p_set_lpaudio_lock(1);
		lpaudio_lock = 1;
	}
*/	

	enable_irq(IRQ_BT_HOST_WAKE);
}
static DECLARE_WORK(bt_host_wake_work, bt_host_wake_work_func);



irqreturn_t bt_host_wake_irq_handler(int irq, void *dev_id)
{
        
        disable_irq_nosync(irq);
        schedule_work(&bt_host_wake_work);
        return IRQ_HANDLED;
}


static enum hrtimer_restart bt_enter_lpm(struct hrtimer *timer)
{
        gpio_set_value(GPIO_BT_WAKE, 1);
        return HRTIMER_NORESTART;
}

void bcm_bt_uart_wake_peer(struct uart_port *port)
{
        if (!bt_lpm_timer.function)
    {
                return;
        }
        hrtimer_try_to_cancel(&bt_lpm_timer);
        gpio_set_value(GPIO_BT_WAKE, 0);
        hrtimer_start(&bt_lpm_timer, bt_lpm_delay, HRTIMER_MODE_REL);
}

static int bt_lpm_init(void)
{
        int ret;
    	int irq;
        ret = gpio_request(GPIO_BT_WAKE, "gpio_bt_wake");
        if (ret) {
                printk(KERN_ERR "Failed to request gpio_bt_wake control\n");
                return 0;
        }

        gpio_direction_output(GPIO_BT_WAKE, GPIO_LEVEL_LOW);

        hrtimer_init(&bt_lpm_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        bt_lpm_delay = ktime_set(1, 0); /* 1 sec */
        bt_lpm_timer.function = bt_enter_lpm;
                /* Initialize wake locks */
        wake_lock_init(&rfkill_wake_lock, WAKE_LOCK_SUSPEND, "bt_host_wake");

                /* BT Host Wake IRQ */
        irq = IRQ_BT_HOST_WAKE;

        ret = request_irq(irq, bt_host_wake_irq_handler,IRQF_TRIGGER_FALLING,
                        "bt_host_wake_irq_handler", NULL);

        if (ret < 0) {
                pr_err("[BT] Request_irq failed\n");
                return ret;
        }
        disable_irq(irq);

        return 0;
}

static int bt_rfkill_set_block(void *data, bool blocked)
{
	int ret =0;
	
	ret = bluetooth_set_power(data, blocked? RFKILL_USER_STATE_SOFT_BLOCKED : RFKILL_USER_STATE_UNBLOCKED);
		
	return ret;
}

static const struct rfkill_ops bt_rfkill_ops = {
	.set_block = bt_rfkill_set_block,
};

static int __init iriver_rfkill_probe(struct platform_device *pdev)
{
	int rc = 0;
	int irq,ret;


	bt_rfk = rfkill_alloc(bt_name, &pdev->dev, RFKILL_TYPE_BLUETOOTH, &bt_rfkill_ops, NULL);
	if (!bt_rfk)
		return -ENOMEM;

	rfkill_init_sw_state(bt_rfk, 0);


	rc = rfkill_register(bt_rfk);
	if (rc)
	{
		printk ("***********ERROR IN REGISTERING THE RFKILL***********\n");
		rfkill_destroy(bt_rfk);
	}

	rfkill_set_sw_state(bt_rfk, 1);
	bluetooth_set_power(NULL, RFKILL_USER_STATE_SOFT_BLOCKED);

	return rc;
}

static struct platform_driver iriver_device_rfkill = {
	.probe = iriver_rfkill_probe,
	.driver = {
		.name = "bt_rfkill",
		.owner = THIS_MODULE,
	},
};


static int __init iriver_rfkill_init(void)
{
	int rc = 0;

	rc = platform_driver_register(&iriver_device_rfkill);
	bt_lpm_init();
	bt_init_complete = 1;
	return rc;
}

module_init(iriver_rfkill_init);
MODULE_DESCRIPTION("iriver rfkill");
MODULE_LICENSE("GPL");
