/*
 * cdma_manaer:CDMA module power management.
 * 
 * Shanghai ewada
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include <mach/gpio-mx100.h>

#include <plat/gpio-cfg.h>
#include <linux/wakelock.h>
#include <linux/irq.h>
#include <plat/irqs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <mach/regs-gpio.h>
/* GPA0 *//*Serial 1*/
#define GPIO_CDMADATA_RXD                     S5PV210_GPA0(4)
#define GPIO_CDMADATA_RXD_AF                  2

#define GPIO_CDMADATA_TXD                     S5PV210_GPA0(5)
#define GPIO_CDMADATA_TXD_AF                  2

#define GPIO_CDMADATA_CTS                     S5PV210_GPA0(6)
#define GPIO_CDMADATA_CTS_AF                  2

#define GPIO_CDMADATA_RTS                     S5PV210_GPA0(7)
#define GPIO_CDMADATA_RTS_AF                  2

/* GPA1 *//*Serial 3*/
#define GPIO_CDMACTRL_RXD                     S5PV210_GPA1(2)
#define GPIO_CDMACTRL_RXD_AF                  2

#define GPIO_CDMACTRL_TXD                     S5PV210_GPA1(3)
#define GPIO_CDMACTRL_TXD_AF                  2

#define IRQ_CDMA_HOST_WAKE      IRQ_EINT(21)
#define GPIO_CDMA_HOST_WAKE       S5PV210_GPH2(5)


static struct wake_lock cdma_wake_lock;

extern int mx100_cdma_power(int status);
extern int mx100_cdma_sleep(int status);

static unsigned int ril_wakeup;

void cdma_uart_port_config(unsigned char port);

void cdma_uart_wake_peer(struct uart_port *port)
{
	if(gpio_get_value(GPIO_CDMA_SLEEP_CTL_OUT) == 0)
	{
		printk("cdma sleep, wakeup\n");
		gpio_set_value(GPIO_CDMA_SLEEP_CTL_OUT,1);
	}
}


static int ril_get_wakeup(void)
{
	return ril_wakeup;
}

static void ril_set_wakeup(int enable)
{
	ril_wakeup = enable;
}

static ssize_t ril_wakeup_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	sprintf(buf, "%d\n", ril_get_wakeup());
	
	printk("ril_wakeup_show wakeup=%s\n", buf);
	//printk("ril_wakeup_show wakeup=%d\n", ril_get_wakeup());
	
	return strlen(buf);
}

static ssize_t ril_wakeup_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int wakeup = simple_strtoul(buf, NULL, 10);
	
	printk("ril_wakeup_store wakeup=%s\n", buf);

	if ((wakeup == 0) || (wakeup == 1)) {
		ril_set_wakeup(wakeup);
	}

	return count;
}

static DEVICE_ATTR(wakeup, 0666,\
		   ril_wakeup_show, ril_wakeup_store);

/*
 * status:0:power on;1:sleep
 *
 */
 
void s3c_setup_cdmauart_cfg_gpio(unsigned char port)
{
	if (port == 1)
	{
		s3c_gpio_cfgpin(GPIO_CDMADATA_RXD, S3C_GPIO_SFN(GPIO_CDMADATA_RXD_AF));
        	s3c_gpio_setpull(GPIO_CDMADATA_RXD, S3C_GPIO_PULL_UP);
		s3c_gpio_slp_cfgpin(GPIO_CDMADATA_RXD, 		S3C_GPIO_SLP_INPUT);  
		s3c_gpio_slp_setpull_updown(GPIO_CDMADATA_RXD, S3C_GPIO_PULL_UP);
	
		s3c_gpio_cfgpin(GPIO_CDMADATA_TXD, S3C_GPIO_SFN(GPIO_CDMADATA_TXD_AF));
        	s3c_gpio_setpull(GPIO_CDMADATA_TXD, S3C_GPIO_PULL_UP);

		//s3c_gpio_slp_cfgpin(GPIO_CDMADATA_TXD, S3C_GPIO_SLP_OUT1);  
		//s3c_gpio_slp_setpull_updown(GPIO_CDMADATA_TXD, S3C_GPIO_PULL_NONE);

		s3c_gpio_cfgpin(GPIO_CDMADATA_CTS, S3C_GPIO_SFN(GPIO_CDMADATA_CTS_AF));
		s3c_gpio_setpull(GPIO_CDMADATA_CTS, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_CDMADATA_RTS, S3C_GPIO_SFN(GPIO_CDMADATA_RTS_AF));
		s3c_gpio_setpull(GPIO_CDMADATA_RTS, S3C_GPIO_PULL_NONE);
	}
	else if (port == 3)
	{
		s3c_gpio_cfgpin(GPIO_CDMACTRL_RXD, S3C_GPIO_SFN(GPIO_CDMACTRL_RXD_AF));
		s3c_gpio_setpull(GPIO_CDMACTRL_RXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_slp_cfgpin(GPIO_CDMACTRL_RXD, 		S3C_GPIO_SLP_INPUT);  
		s3c_gpio_slp_setpull_updown(GPIO_CDMACTRL_RXD, S3C_GPIO_PULL_UP);
		s3c_gpio_cfgpin(GPIO_CDMACTRL_TXD, S3C_GPIO_SFN(GPIO_CDMACTRL_TXD_AF));
		s3c_gpio_setpull(GPIO_CDMACTRL_TXD, S3C_GPIO_PULL_UP);
    	}
}
/* added by jhlim 2011.06.21 request from andrew.park */
void cdma_uart_port_config(unsigned char port)
{
	if (port == 1)
	{

	}
	else if (port == 3)
	{
		s3c_gpio_cfgpin(GPIO_CDMACTRL_RXD, S3C_GPIO_SFN(GPIO_CDMACTRL_RXD_AF));
		s3c_gpio_setpull(GPIO_CDMACTRL_RXD, S3C_GPIO_PULL_UP);
		s3c_gpio_cfgpin(GPIO_CDMACTRL_TXD, S3C_GPIO_SFN(GPIO_CDMACTRL_TXD_AF));
		s3c_gpio_setpull(GPIO_CDMACTRL_TXD, S3C_GPIO_PULL_UP);
    	}
}
void uart3_wakeup(void)
{
		unsigned int gpa0_tx_dat = 0;
		//AP uart3 TX Set to Low
		s3c_gpio_cfgpin(S5PV210_GPA1(3), S3C_GPIO_OUTPUT);
		gpa0_tx_dat = __raw_readl(S5PV210_GPA1DAT);
		gpa0_tx_dat &= ~(1 << 3);
		__raw_writel(gpa0_tx_dat, S5PV210_GPA1DAT);

		s3c_gpio_cfgpin(S5PV210_GPA1(3), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPA1(3), S3C_GPIO_PULL_NONE);
}

static int cdma_manager_resume(struct platform_device *dev) {

	printk("cdma resume, check ready pin signal is %d\n",gpio_get_value(GPIO_CDMA_READY_SIGNAL_IN));
	printk("__%s start \n", __func__);
	
	s3c_gpio_setpull(GPIO_CDMA_SLEEP_CTL_OUT, S3C_GPIO_PULL_UP);
	//mx100_cdma_sleep(1); //not used now
	//ril_set_wakeup(1);
	//msleep(100);
	//uart3_wakeup();
	cdma_uart_port_config(3);
	
	printk("__%s done \n", __func__);

	return 0;
}

static int cdma_manager_suspend(struct platform_device *dev, pm_message_t state) {

	printk("cdma suspend,disable cdma power\n");
	mx100_cdma_sleep(0);//not used now
	//ril_set_wakeup(0); 
	
	//s3c_setup_cdmauart_cfg_gpio(1);
	//s3c_setup_cdmauart_cfg_gpio(3);
        
   	s3c_gpio_slp_cfgpin(GPIO_CDMA_RESET_OUT, S3C_GPIO_SLP_OUT1);  
	s3c_gpio_slp_setpull_updown(GPIO_CDMA_RESET_OUT, S3C_GPIO_PULL_NONE);

	printk("__%s done \n", __func__);
	msleep(100);		
	return 0;
}

static void cdma_host_wake_work_func(struct work_struct *ignored)
{
        int irq;
        irq = IRQ_CDMA_HOST_WAKE;
        printk("cdma wakeup host\n");
        wake_lock_timeout(&cdma_wake_lock, 5*HZ);//Is it necessary?
        mx100_cdma_sleep(1); 
        printk("end cdma_host_wake_work_func\n");//
		enable_irq(IRQ_CDMA_HOST_WAKE);
}

static DECLARE_WORK(cdma_host_wake_work, cdma_host_wake_work_func);



irqreturn_t cdma_host_wake_irq_handler(int irq, void *dev_id)
{
        
        disable_irq_nosync(irq);
        schedule_work(&cdma_host_wake_work);
        return IRQ_HANDLED;
}

static int cdma_manager_probe(struct platform_device *dev)
{
/*	if (gpio_is_valid(GPIO_CDMA_PWR_ON_OUT)) {
		if (gpio_request(GPIO_CDMA_PWR_ON_OUT, "GPH2")) 
			printk(KERN_ERR "Failed to request GPIO_CDMA_PWR_ON_OUT!\n");
		gpio_direction_output(GPIO_CDMA_PWR_ON_OUT, GPIO_LEVEL_LOW);
	}
	s3c_gpio_setpull(GPIO_CDMA_PWR_ON_OUT, S3C_GPIO_PULL_DOWN);

	if (gpio_is_valid(GPIO_CDMA_RESET_OUT)) {
		if (gpio_request(GPIO_CDMA_RESET_OUT, "GPH2")) 
			printk(KERN_ERR "Failed to request GPIO_CDMA_RESET_OUT!\n");
		gpio_direction_output(GPIO_CDMA_RESET_OUT, GPIO_LEVEL_LOW);
	}
	s3c_gpio_setpull(GPIO_CDMA_RESET_OUT, S3C_GPIO_PULL_NONE);

	if (gpio_is_valid(GPIO_CDMA_READY_SIGNAL_IN)) {
		if (gpio_request(GPIO_CDMA_READY_SIGNAL_IN, "GPH2")) 
			printk(KERN_ERR "Failed to request GPIO_CDMA_READY_SIGNAL_IN!\n");
		
		gpio_direction_input(GPIO_CDMA_READY_SIGNAL_IN);
	}
	s3c_gpio_setpull(GPIO_CDMA_READY_SIGNAL_IN, S3C_GPIO_PULL_DOWN);

	if (gpio_is_valid(GPIO_CDMA_SLEEP_CTL_OUT)) {
		if (gpio_request(GPIO_CDMA_SLEEP_CTL_OUT, "GPH2")) 
			printk(KERN_ERR "Failed to request GPIO_CDMA_SLEEP_CTL_OUT!\n");
		
		gpio_direction_output(GPIO_CDMA_SLEEP_CTL_OUT,1);
	}
	s3c_gpio_setpull(GPIO_CDMA_SLEEP_CTL_OUT, S3C_GPIO_PULL_UP);*/
	int ret = 0;
	int irq;
	ril_wakeup = 1;
	
	ret = device_create_file(&(dev->dev), &dev_attr_wakeup);
	if (ret < 0) {
		printk("failed to add sysfs entries\n");
		return -1;
	}
	
//[yoon 20110504]changed cdma power squency : iriver_init_gpio() -> cdma_manager_probe()
#if 1
	mx100_cdma_power(0); 
#endif

	
    s3c_gpio_cfgpin(GPIO_CDMA_HOST_WAKE, S3C_GPIO_SFN(0xF));
   	s3c_gpio_setpull(GPIO_CDMA_HOST_WAKE, S3C_GPIO_PULL_NONE);

	cdma_uart_port_config(3);

    irq = IRQ_CDMA_HOST_WAKE;
    wake_lock_init(&cdma_wake_lock, WAKE_LOCK_SUSPEND, "cdma_host_wake");
    ret = request_irq(irq, cdma_host_wake_irq_handler,IRQF_TRIGGER_RISING,
                        "cdma_host_wake_irq_handler", NULL);

    if (ret < 0) {
                pr_err("[CDMA] Request_irq failed\n");
                return ret;
    }
    disable_irq(irq);
    ret = enable_irq_wake(irq);
    if (ret < 0)
        pr_err("[CDMA] set wakeup src failed\n");
    enable_irq(irq);

	return 0;
}

static int __devexit cdma_manager_remove(struct platform_device *dev)
{
	device_remove_file(&(dev->dev), &dev_attr_wakeup);
	return 0;
}

static struct platform_driver cdma_manager_driver = {
	.probe = cdma_manager_probe,
	.remove = __devexit_p(cdma_manager_remove),
	
	.suspend = cdma_manager_suspend,
	.resume	= cdma_manager_resume,

	.driver = {
		.name = "cdma_manager",
		.owner = THIS_MODULE,
	},
};

static struct platform_device *cdma_dev = NULL;
static int __init cdma_manager_init(void)
{
	
	cdma_dev = platform_device_alloc("cdma_manager", -1);

	if (NULL != cdma_dev)
		platform_device_add(cdma_dev);     /*Shanghai ewada modify for warnings*/
	platform_driver_register(&cdma_manager_driver);

	return 0;
}

static void __exit cdma_manager_exit(void)
{
	platform_driver_unregister(&cdma_manager_driver);
	platform_device_del(cdma_dev);
}

late_initcall(cdma_manager_init);
module_exit(cdma_manager_exit);


