/*
 * Bluetooth built-in chip control
 *
 * Copyright (c) 2008 Dmitry Baryshkov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/rfkill.h>
#include <mach/bcm_bt.h>
#include <mach/gpio-mx100.h>
#include <mach/irqs.h>
#include <linux/interrupt.h>
#include <linux/wakelock.h>
#include <plat/gpio-cfg.h>
#include <linux/hrtimer.h>

#define GPIO_BT_WAKE          	S5PV210_GPJ0(2)
#define GPIO_BT_nRST            S5PV210_GPJ0(6)
#define GPIO_WLAN_BT_EN         S5PV210_GPJ2(3)

#ifdef CONFIG_MX100_REV_ES
#define GPIO_WLAN_BT_CLK_EN     S5PV210_GPJ0(1)
#endif

/* GPA0 */
#define GPIO_BT_RXD                     S5PV210_GPA0(0)
#define GPIO_BT_RXD_AF                  2

#define GPIO_BT_TXD                     S5PV210_GPA0(1)
#define GPIO_BT_TXD_AF                  2

#define GPIO_BT_CTS                     S5PV210_GPA0(2)
#define GPIO_BT_CTS_AF                  2

#define GPIO_BT_RTS                     S5PV210_GPA0(3)
#define GPIO_BT_RTS_AF                  2

#define GPIO_BT_HOST_WAKE     S5PV210_GPH1(2)

#define IRQ_BT_HOST_WAKE     IRQ_EINT10
static struct wake_lock rfkill_wake_lock;
static void bcm_uart_cfg(int onoff)
{
	s3c_gpio_cfgpin(GPIO_BT_RXD, S3C_GPIO_SFN(GPIO_BT_RXD_AF));
        s3c_gpio_setpull(GPIO_BT_RXD, S3C_GPIO_PULL_NONE);
        s3c_gpio_cfgpin(GPIO_BT_TXD, S3C_GPIO_SFN(GPIO_BT_TXD_AF));
        s3c_gpio_setpull(GPIO_BT_TXD, S3C_GPIO_PULL_NONE);
        s3c_gpio_cfgpin(GPIO_BT_CTS, S3C_GPIO_SFN(GPIO_BT_CTS_AF));
        s3c_gpio_setpull(GPIO_BT_CTS, S3C_GPIO_PULL_NONE);
        s3c_gpio_cfgpin(GPIO_BT_RTS, S3C_GPIO_SFN(GPIO_BT_RTS_AF));
        s3c_gpio_setpull(GPIO_BT_RTS, S3C_GPIO_PULL_NONE);
}

static void bcm_bt_cfg(int onoff)
{
	s3c_gpio_cfgpin(GPIO_BT_WAKE, S3C_GPIO_SFN(0x1));
	s3c_gpio_setpull(GPIO_BT_WAKE, S3C_GPIO_PULL_NONE);

	s3c_gpio_cfgpin(GPIO_BT_nRST, S3C_GPIO_SFN(0x1));
	s3c_gpio_setpull(GPIO_BT_nRST, S3C_GPIO_PULL_NONE);

	s3c_gpio_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_SFN(0x1));
	s3c_gpio_setpull(GPIO_WLAN_BT_EN, S3C_GPIO_PULL_NONE);

	s3c_gpio_cfgpin(GPIO_BT_HOST_WAKE, S3C_GPIO_SFN(0xF));
    s3c_gpio_setpull(GPIO_BT_HOST_WAKE, S3C_GPIO_PULL_DOWN);
}

static void bcm_bt_on(struct bcm_bt_data  *data)
{

	printk("%s () line= %d \n", __FUNCTION__, __LINE__);
	bcm_bt_cfg(1);
	bcm_uart_cfg(1);
#ifdef CONFIG_MX100_REV_ES
       s3c_gpio_cfgpin(GPIO_WLAN_BT_CLK_EN, S3C_GPIO_OUTPUT);
       s3c_gpio_setpull(GPIO_WLAN_BT_CLK_EN, S3C_GPIO_PULL_NONE);
       gpio_set_value(GPIO_WLAN_BT_CLK_EN, GPIO_LEVEL_HIGH);
#endif
	gpio_set_value(GPIO_WLAN_BT_EN, GPIO_LEVEL_HIGH);
	gpio_set_value(GPIO_BT_nRST, GPIO_LEVEL_LOW);
	msleep(100);
	gpio_set_value(GPIO_BT_nRST, GPIO_LEVEL_HIGH);
}

static void bcm_bt_off(struct bcm_bt_data  *data)
{
	printk("%s () line= %d \n", __FUNCTION__, __LINE__);
	gpio_set_value(GPIO_BT_nRST, GPIO_LEVEL_LOW);

}

static int bcm_bt_set_block(void *data, bool blocked)
{
	pr_info("BT_RADIO going: %s\n", blocked ? "off" : "on");

	if (!blocked) {
		pr_info("BCM_BT: going ON\n");
		bcm_bt_on(data);
				/*
		 * 50msec, delay after bt rst
		 * (bcm4329 powerup sequence)
		 */
		msleep(50);
	} else {
		pr_info("BCM_BT: going OFF\n");
		bcm_bt_off(data);
	}
	return 0;
}

static const struct rfkill_ops bcm_bt_rfkill_ops = {
	.set_block = bcm_bt_set_block,
};


static int bcm_bt_probe(struct platform_device *dev)
{
	int rc;
	struct rfkill *rfk;
	struct bcm_bt_data *data = dev->dev.platform_data;
  
	rfk = rfkill_alloc("bcm-bt", &dev->dev, RFKILL_TYPE_BLUETOOTH, &bcm_bt_rfkill_ops, data);
	if (!rfk) {
		rc = -ENOMEM;
		goto err_rfk_alloc;
	}
	rfkill_init_sw_state(rfk, 0);
	rc = rfkill_register(rfk);
	if (rc)
		goto err_rfkill;
	rfkill_set_sw_state(rfk, 1);
	bcm_bt_set_block(NULL, 1); //close bt power when boot. saving power.
	//rfkill_set_led_trigger_name(rfk, "bcm-bt");
	//platform_set_drvdata(dev, rfk);
	return 0;

err_rfkill:
	if (rfk)
		rfkill_destroy(rfk);
	rfk = NULL;
err_rfk_alloc:
	bcm_bt_off(data);
	return rc;
}

static int __devexit bcm_bt_remove(struct platform_device *dev)
{
	struct bcm_bt_data *data = dev->dev.platform_data;
	struct rfkill *rfk = platform_get_drvdata(dev);
	platform_set_drvdata(dev, NULL);
	if (rfk)
	{
		rfkill_unregister(rfk);
		rfkill_destroy(rfk);
	}
	rfk = NULL;
	bcm_bt_off(data);
	return 0;
}

static struct platform_driver bcm_bt_driver = {
	.probe = bcm_bt_probe,
	.remove = __devexit_p(bcm_bt_remove),
	.driver = {
		.name = "bcm-bt",
		.owner = THIS_MODULE,
	},
};

static  struct hrtimer bt_lpm_timer;
static ktime_t bt_lpm_delay;
irqreturn_t bt_host_wake_irq_handler(int irq, void *dev_id)
{
	pr_debug("[BT] bt_host_wake_irq_handler start\n");
/*
	if (gpio_get_value(GPIO_BT_HOST_WAKE))
		wake_lock(&rfkill_wake_lock);
	else
		wake_lock_timeout(&rfkill_wake_lock, HZ);
*/
	wake_lock_timeout(&rfkill_wake_lock, 5*HZ);
	return IRQ_HANDLED;
}
static enum hrtimer_restart bt_enter_lpm(struct hrtimer *timer)
{
	gpio_set_value(GPIO_BT_WAKE, 0);
	return HRTIMER_NORESTART;
}

void bcm_bt_uart_wake_peer(struct uart_port *port)
{
	if (!bt_lpm_timer.function)
    {
		return;
	}
	hrtimer_try_to_cancel(&bt_lpm_timer);
	gpio_set_value(GPIO_BT_WAKE, 1);
	//printk("bt_wake value is %d\n",gpio_get_value(GPIO_BT_WAKE));
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
	bt_lpm_delay = ktime_set(1, 0);	/* 1 sec */
	bt_lpm_timer.function = bt_enter_lpm;

		/* Initialize wake locks */
	wake_lock_init(&rfkill_wake_lock, WAKE_LOCK_SUSPEND, "bt_host_wake");

		/* BT Host Wake IRQ */
	irq = IRQ_BT_HOST_WAKE;

	ret = request_irq(irq, bt_host_wake_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"bt_host_wake_irq_handler", NULL);

	if (ret < 0) {
		pr_err("[BT] Request_irq failed\n");
		return ret;
	}
	set_irq_wake(irq, 1);

	return 0;
}
#if 0
static struct rfkill *bt_sleep;
static struct wake_lock rfkill_wake_lock;
//static struct wake_lock bt_wake_lock;
static void bt_host_wake_work_func(struct work_struct *ignored)
{
	        wake_lock_timeout(&rfkill_wake_lock, 5*HZ);
	        enable_irq(IRQ_EINT10);
}
static DECLARE_WORK(bt_host_wake_work, bt_host_wake_work_func);
irqreturn_t bt_host_wake_irq_handler(int irq, void *dev_id)
{
	        disable_irq_nosync(IRQ_EINT10);
	        schedule_work(&bt_host_wake_work);
	               
	        return IRQ_HANDLED;
}
#endif
#if 0
static int bluetooth_set_sleep(void *data,  bool blocked)
{      
		pr_info("bluetooth_set_sleep : %s\n", blocked ? "off" : "on");
		if (!blocked) {//state=1
                printk("wake up bt chip \n");
                gpio_set_value(GPIO_BT_WAKE, GPIO_LEVEL_HIGH);
              //wake_lock(&bt_wake_lock);
		}
        else {//state=0
			    printk("allow bt sleep \n");
                gpio_set_value(GPIO_BT_WAKE, GPIO_LEVEL_LOW);
               // wake_unlock(&bt_wake_lock);
	    }       
        return 0;
}

static const struct rfkill_ops bcm_bt_rfkill_sleep_ops = {
	.set_block = bluetooth_set_sleep,
};
#endif
#if 0
static int __init bcm_btsleep_probe(struct platform_device *pdev)
{
        int rc = 0;
		rc = request_irq(IRQ_EINT10, bt_host_wake_irq_handler, IRQF_TRIGGER_FALLING, "bt_host_wake_irq_handler", NULL);
	        if(rc < 0)
	                printk("[BT] Request_irq failed \n");
		set_irq_wake(IRQ_EINT10, 1);
	wake_lock_init(&rfkill_wake_lock, WAKE_LOCK_SUSPEND, "board-rfkill");
	//wake_lock_init(&bt_wake_lock, WAKE_LOCK_SUSPEND, "bcm-bt");
    	bt_sleep = rfkill_alloc("bcm-bt", &pdev->dev, RFKILL_TYPE_BLUETOOTH, &bcm_bt_rfkill_sleep_ops, NULL);
        if (!bt_sleep)
                return -ENOMEM;
        rc = rfkill_register(bt_sleep);
        if (rc)
                	rfkill_destroy(bt_sleep);
        return rc;
}

static struct platform_driver bcm_device_btsleep = {
        .probe = bcm_btsleep_probe,
        .driver = {
                .name = "bcm-bt-sleep",
                .owner = THIS_MODULE,
        },
};
#endif
static int __init bcm_bt_init(void)
{
	 int rc =0;
	 rc = platform_driver_register(&bcm_bt_driver);
	 #if 0
	 platform_driver_register(&bcm_device_btsleep);
	 #endif
	 bt_lpm_init();
	 return rc;
}

static void __exit bcm_bt_exit(void)
{
	platform_driver_unregister(&bcm_bt_driver);
	#if 0
	platform_driver_unregister(&bcm_device_btsleep);
	#endif
	
}
late_initcall(bcm_bt_init);
module_exit(bcm_bt_exit);
