/*
 *  max17040_battery.c
 *  fuel-gauge systems for lithium-ion (Li+) batteries
 *
 *  Copyright (C) 2009 Samsung Electronics
 *  Minkyu Kang <mk7.kang@samsung.com>
 *
 *  Copyright (C) 2010 iriver Co.,ltd.
 *  ktj
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/max17040_battery.h>
#include <linux/switch.h> // ktj usb_switch

#include <asm/mach/irq.h>
#include <asm/io.h>
#include <mach/gpio.h>
#include <mach/regs-gpio.h>

#include <plat/gpio-cfg.h>

#define FIX_BATTERY_STATUS_BUG  /* JHLIM 2011.07.15 */


#define MAX17040_VCELL_MSB	0x02
#define MAX17040_VCELL_LSB	0x03
#define MAX17040_SOC_MSB	0x04
#define MAX17040_SOC_LSB	0x05
#define MAX17040_MODE_MSB	0x06
#define MAX17040_MODE_LSB	0x07
#define MAX17040_VER_MSB	0x08
#define MAX17040_VER_LSB	0x09
#define MAX17040_RCOMP_MSB	0x0C
#define MAX17040_RCOMP_LSB	0x0D
#define MAX17040_CMD_MSB	0xFE
#define MAX17040_CMD_LSB	0xFF

#define MAX17040_DELAY		msecs_to_jiffies(1000)
#define MAX17040_BATTERY_FULL	99


#if defined(CONFIG_MX100)
//----------------------------------------------------------------------------------
//  #define rdebug(fmt,arg...) printk(KERN_CRIT "_[battery] " fmt ,## arg)
    #define rdebug(fmt,arg...)
//----------------------------------------------------------------------------------

#undef  USE_LEVEL_VCELL         /* only for test, calculate level by vcell, not define level by soc value */ 

#if defined(CONFIG_MX100_REV_ES)
#define USE_ADAPTOR             /* process adaptor online and charging, need to be fixed ws board */
#endif

#ifdef USE_LEVEL_VCELL
#define MAX_VCELL   4180000     /* can be reached at 4184000 */  
#define LOW_VCELL   3400000     /* 3450000 */
#endif

static struct wake_lock vbus_wake_lock;

static u32 elapsed_sec = 0;

static int debug_enable = 0;


// ktj usb_switch
struct switch_dev 		indicator_dev;
void UsbIndicator(u8 state)
{
//  printk("__%s state = %d \n", __func__, state);
	switch_set_state(&indicator_dev, state);
}


void set_debug_battery(int enable)
{
    if (enable)
        debug_enable = 1;
    else
        debug_enable = 0;
}
EXPORT_SYMBOL(set_debug_battery);
//----------------------------------------------------------------------------------
#endif


struct max17040_chip {
	struct i2c_client		*client;
	struct delayed_work		work;
	struct power_supply		battery;
	struct power_supply		ac;
	struct power_supply		usb;
	struct max17040_platform_data	*pdata;

	/* State Of Connect */
	int online;
	/* battery voltage */
	int vcell;
	/* battery capacity */
	int soc;
	/* State Of Charge */
	int status;
	/* usb online */
	int usb_online;
};
static bool isconnected;

void isUSBconnected(bool usb_connect)
{
#if 0
    if (gpio_get_value(GPIO_CHARGER_ONLINE) == 0) // connected usb or adaptor
	    isconnected = usb_connect;
    else
	    isconnected = 0;
#else
    isconnected = usb_connect;
#endif
}
//EXPORT_SYMBOL(isUSBconnected);

/*Shanghai ewada for usb status*/
#if defined(CONFIG_MX100)
extern struct class *iriver_class;
static ssize_t show_usb_charge_attr(struct device *dev,
				 struct device_attribute *attr, char *sysfsbuf)
{
	return snprintf(sysfsbuf, PAGE_SIZE, "%d\n", gpio_get_value(GPIO_CHARGER_ONLINE)?0:1);
}

static CLASS_ATTR(usbonline, S_IRUGO, 
	show_usb_charge_attr, NULL);
#endif

#if 1 // ktj
static int battery_soc_level = 50;
void set_battery_level(int level)
{
    battery_soc_level = level;        
}

int get_battery_level(void)
{
    return battery_soc_level;    
}
#endif

#ifdef USE_ADAPTOR
static int charger_online(void)
{
    int ret = 1;

    ret = gpio_get_value(GPIO_CHARGER_ONLINE);

    if (ret == 1)
        ret = 0;
    else
        ret = 1;

    if (ret && isconnected)
        ret = 0;  

    rdebug("%s %d \n", __func__, ret);

    return ret;
}

static int is_connected_gpio(void)
{
    if (gpio_get_value(GPIO_CHARGER_ONLINE))
        return 0;
    
    return 1;
}

/*
Charger Status Output. 
Active-low, open-drain output pulls low when the battery is in fast-charge or
prequal. Otherwise, CHG is high impedance.

0 : charging
1 : not charging
*/
static int charger_done(void)
{
    int ret = 0;

    ret = gpio_get_value(GPIO_CHARGER_STATUS);
    
    rdebug("%s %d \n", __func__, ret);

    return ret;
}
#endif

#if defined(CONFIG_INPUT_BMA150_SENSOR)
	extern int bma150_sensor_get_air_temp(void);
#endif

#if defined(CONFIG_MX100) /* ktj */
void s3c_cable_check_status(int flag)
{
    if (flag == 0)
        isUSBconnected(0);    
    else
        isUSBconnected(1);
}
EXPORT_SYMBOL(s3c_cable_check_status);
#endif

static int max17040_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct max17040_chip *chip = container_of(psy,
				struct max17040_chip, battery);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chip->status;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->online;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = chip->vcell;
		if(psp  == POWER_SUPPLY_PROP_PRESENT)
			val->intval = 1; /* You must never run Odrioid1 without Battery. */
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		#ifdef FIX_BATTERY_STATUS_BUG
		if(chip->status == POWER_SUPPLY_STATUS_FULL) {
			val->intval = 100;
		} else {
			val->intval = chip->soc;
		}
		#else
		val->intval = chip->soc;
		#endif
		break;
	 case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	 case POWER_SUPPLY_PROP_HEALTH:
	 	if(chip->vcell  < 2850)
			val->intval = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
		else
		 	 val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		#if defined(CONFIG_INPUT_BMA150_SENSOR)
			val->intval = bma150_sensor_get_air_temp();
		#else
			val->intval = 365;
		#endif
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int usb_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	int ret = 0;
	struct max17040_chip *chip = container_of(psy,
				struct max17040_chip, usb);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		//TODO:
		val->intval =  chip->usb_online;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int adapter_get_property(struct power_supply *psy,
			enum power_supply_property psp,
			union power_supply_propval *val)
{
#ifdef USE_ADAPTOR
	struct max17040_chip *chip = container_of(psy, struct max17040_chip, ac);
#endif
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
		//TODO:
#ifdef USE_ADAPTOR
//		if (chip->pdata->charger_online)
//			val->intval = chip->pdata->charger_online();

    	val->intval = charger_online();

//      rdebug("%s val->intval = %d\n", __func__, val->intval);
#else
		val->intval = 0;
#endif
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}
static int max17040_write_reg(struct i2c_client *client, int reg, u8 value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

#ifdef CONFIG_MX100_IOCTL /* LJH */
int mx100_max17040_reg_write(int addr,int data)
{
/*
	if(!g_max17040_i2c_client) return 0;
	return max17040_write_reg(g_max17040_i2c_client,addr,data);	
*/
    return 0;
}

int mx100_max17040_reg_read(int addr)
{
/*    
	if(!g_max17040_i2c_client) return 0;
	return max17040_read_reg(g_max17040_i2c_client,addr);	
*/
    return 0;
}
#endif

static int max17040_read_reg(struct i2c_client *client, int reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);

#if 0
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
#endif

	return ret;
}

static void max17040_reset(struct i2c_client *client)
{
	max17040_write_reg(client, MAX17040_CMD_MSB, 0x54);
	max17040_write_reg(client, MAX17040_CMD_LSB, 0x00);
}

static void max17040_get_vcell(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);
	u8 msb;
	u8 lsb;

	msb = max17040_read_reg(client, MAX17040_VCELL_MSB);
	lsb = max17040_read_reg(client, MAX17040_VCELL_LSB);

	chip->vcell = (msb << 4) + (lsb >> 4);
	chip->vcell = (chip->vcell * 125) * 10;
}

static int low_battery_power_off = 0;
extern int get_request_poweroff(void); // ktj, pm.c

static void max17040_get_soc(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);
	unsigned char SOC1, SOC2;


    if (low_battery_power_off == 1)
    {
        chip->soc = 0;
        if (debug_enable)
            printk("_[battery] Low battery power off chip->soc = %d \n\n", chip->soc);
        return;
    }    

	SOC1 = max17040_read_reg(client, MAX17040_SOC_MSB);
	SOC2 = max17040_read_reg(client, MAX17040_SOC_LSB);

	chip->soc = (SOC1 >= 99) ? 100 : SOC1;

//  if (SOC1 == 0 && SOC2 >= 128)
//      chip->soc = 1;    

#ifdef USE_LEVEL_VCELL /* ktj */
    if (chip->vcell <= LOW_VCELL)
        chip->soc = 0;
    else if (chip->vcell >= MAX_VCELL)
        chip->soc = 100;
    else
    {
        chip->soc = (chip->vcell - LOW_VCELL) * 100 / (MAX_VCELL - LOW_VCELL);
        if (chip->soc == 0 && chip->vcell > LOW_VCELL)
            chip->soc = 1;    
    }
#endif


	#ifdef FIX_BATTERY_STATUS_BUG
	if(chip->soc > 96) {
		chip->soc = 100;
	}
	#endif


    /* ktj process pmic lowbat interrupt */
    if (get_request_poweroff())
        chip->soc = 0;    
    set_battery_level(chip->soc);

    if (chip->soc == 0)
        low_battery_power_off = 1;    

    if (debug_enable)
        printk("_[battery] vcell = %d level = %d SOC1 = %d SOC2 = %d elapsed_sec = %ld \n\n", chip->vcell, chip->soc, SOC1, SOC2, elapsed_sec++); /* ktj */

	return;
}

static void max17040_get_version(struct i2c_client *client)
{
	u8 msb;
	u8 lsb;

	msb = max17040_read_reg(client, MAX17040_VER_MSB);
	lsb = max17040_read_reg(client, MAX17040_VER_LSB);

	dev_info(&client->dev, "MAX17040 Fuel-Gauge Ver %d%d\n", msb, lsb);
}

static void max17040_get_online(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);

#ifdef USE_ADAPTOR
//	if (chip->pdata->charger_online)
//		chip->online = chip->pdata->charger_online();
//	else
//		chip->online = 0;

	chip->online = charger_online();

//  rdebug("%s chip->online = %d\n", __func__, chip->online);

#else
		chip->online = 1;
#endif
}

static void max17040_get_status(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);

#ifdef USE_ADAPTOR

	chip->usb_online = isconnected;
	if (is_connected_gpio() == 0)
	    chip->usb_online = 0;    
	

	if (chip->online) 
	{
        UsbIndicator(0); // ktj usb_switch
        //printk("__%s Connected adpator \n\n", __func__);

		if(charger_done())
			chip->status = POWER_SUPPLY_STATUS_FULL;
		else 
		{
			if(chip->soc > MAX17040_BATTERY_FULL)
				chip->status = POWER_SUPPLY_STATUS_FULL;
			else
				chip->status = POWER_SUPPLY_STATUS_CHARGING;
		}
	}
	else if (chip->usb_online) 
	{
        UsbIndicator(1); // ktj usb_switch
        //printk("__%s Connected usb \n\n", __func__);

		if(charger_done())
			chip->status = POWER_SUPPLY_STATUS_FULL;
		else 
		{
			if(chip->soc > MAX17040_BATTERY_FULL)
				chip->status = POWER_SUPPLY_STATUS_FULL;
			else
				chip->status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		}
    }
	else 
	{
        UsbIndicator(0); // ktj usb_switch
        //printk("__%s Disconnected \n\n", __func__);

		chip->status = POWER_SUPPLY_STATUS_DISCHARGING;
	}
	
#else

	chip->usb_online = isconnected;
	if(chip->soc > 101) {
		chip->status = POWER_SUPPLY_STATUS_UNKNOWN;
		return;
	}

	if(chip->usb_online)
		chip->status = POWER_SUPPLY_STATUS_CHARGING;
	else
		chip->status = POWER_SUPPLY_STATUS_DISCHARGING;

	if (chip->soc > MAX17040_BATTERY_FULL)
		chip->status = POWER_SUPPLY_STATUS_FULL;

#endif
}

static void max17040_work(struct work_struct *work)
{
	struct max17040_chip *chip;
	int old_usb_online, old_online, old_vcell, old_soc;

	chip = container_of(work, struct max17040_chip, work.work);

//	old_online = chip->usb_online;
	old_online = chip->online;
	old_usb_online = chip->usb_online;

	old_vcell = chip->vcell;
//	old_soc = old_soc;
	old_soc = chip->soc; /* ktj */

	max17040_get_online(chip->client);
	max17040_get_vcell(chip->client);
	max17040_get_soc(chip->client);
	max17040_get_status(chip->client);

	if((old_vcell != chip->vcell) || (old_soc != chip->soc))
		power_supply_changed(&chip->battery);


#ifdef USE_ADAPTOR
    #if 1 /* ktj fixed */     /*Shanghai ewada for usb unstable*/
	if((old_online != chip->online) && (!chip->usb_online)) 
	{
        if (chip->online) {
    		wake_lock(&vbus_wake_lock);
			rdebug("wake_lock - adaptor\n");
		} else {
    		/* give userspace some time to see the uevent and update
	    	 * LED state or whatnot...
		     */
		    wake_lock_timeout(&vbus_wake_lock, HZ / 2);
            rdebug("wake_lock_timeout - adaptor\n");
        }

		power_supply_changed(&chip->ac);
	}
    #else
	if(old_online != chip->online)
		power_supply_changed(&chip->ac);
    #endif
#endif

#if 1 /* ktj */
	if(old_usb_online != chip->usb_online)
    {   
        if (chip->usb_online) {
    		wake_lock(&vbus_wake_lock);
			rdebug("wake_lock \n");
		} else {
    		/* give userspace some time to see the uevent and update
	    	 * LED state or whatnot...
		     */
		    wake_lock_timeout(&vbus_wake_lock, HZ / 2);
            rdebug("wake_lock_timeout \n");
        }

		power_supply_changed(&chip->usb);
    }
#else
	if(old_usb_online != chip->usb_online)
		power_supply_changed(&chip->usb);
#endif

	schedule_delayed_work(&chip->work, MAX17040_DELAY);
}

static enum power_supply_property max17040_battery_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_STATUS,
	/*POWER_SUPPLY_PROP_ONLINE,*/
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TEMP,
};

static enum power_supply_property adapter_get_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property usb_get_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};


static int __devinit max17040_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct max17040_chip *chip;
	int ret;//, i;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->pdata = client->dev.platform_data;

	i2c_set_clientdata(client, chip);

	chip->battery.name		= "battery";
	chip->battery.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->battery.get_property	= max17040_get_property;
	chip->battery.properties	= max17040_battery_props;
	chip->battery.num_properties	= ARRAY_SIZE(max17040_battery_props);
	chip->battery.external_power_changed = NULL;

	chip->ac.name		= "ac";
	chip->ac.type		= POWER_SUPPLY_TYPE_MAINS;
	chip->ac.get_property	= adapter_get_property;
	chip->ac.properties	= adapter_get_props;
	chip->ac.num_properties	= ARRAY_SIZE(adapter_get_props);
	chip->ac.external_power_changed = NULL;

	chip->usb.name		= "usb";
	chip->usb.type		= POWER_SUPPLY_TYPE_USB;
	chip->usb.get_property	= usb_get_property;
	chip->usb.properties	= usb_get_props;
	chip->usb.num_properties	= ARRAY_SIZE(usb_get_props);
	chip->usb.external_power_changed = NULL;

	/*Shanghai ewada add for usb status*/
#if defined(CONFIG_MX100)
	//device_create_file(&client->dev, &dev_attr_);
	class_create_file(iriver_class, &class_attr_usbonline);
#endif

	ret = power_supply_register(&client->dev, &chip->battery);
	if (ret)
		goto err_battery_failed;

	ret = power_supply_register(&client->dev, &chip->ac);
	if(ret)
		goto err_ac_failed;

	ret = power_supply_register(&client->dev, &chip->usb);
	if(ret)
		goto err_usb_failed;

	max17040_get_version(client);

	INIT_DELAYED_WORK_DEFERRABLE(&chip->work, max17040_work);
	schedule_delayed_work(&chip->work, MAX17040_DELAY);

	return 0;

err_usb_failed:
	power_supply_unregister(&chip->ac);
err_ac_failed:
	power_supply_unregister(&chip->battery);
err_battery_failed:
	dev_err(&client->dev, "failed: power supply register\n");
	i2c_set_clientdata(client, NULL);
	kfree(chip);

	return ret;
}

static int __devexit max17040_remove(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);

	power_supply_unregister(&chip->usb);
	power_supply_unregister(&chip->ac);
	power_supply_unregister(&chip->battery);
	cancel_delayed_work(&chip->work);
	i2c_set_clientdata(client, NULL);
	kfree(chip);
	return 0;
}

#if defined(CONFIG_PM)

static int max17040_suspend(struct i2c_client *client,
		pm_message_t state)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);

	cancel_delayed_work(&chip->work);
    rdebug("%s\n", __func__);
	return 0;
}

static int max17040_resume(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);

	schedule_delayed_work(&chip->work, MAX17040_DELAY);
    rdebug("%s\n", __func__);
	return 0;
}

#else

#define max17040_suspend NULL
#define max17040_resume NULL

#endif /* CONFIG_PM */

static const struct i2c_device_id max17040_id[] = {
	{ "max17040", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17040_id);

static struct i2c_driver max17040_i2c_driver = {
	.driver	= {
		.name	= "max17040",
	},
	.probe		= max17040_probe,
	.remove		= __devexit_p(max17040_remove),
	.suspend	= max17040_suspend,
	.resume		= max17040_resume,
	.id_table	= max17040_id,
};

#if 1 // ktj usb_switch
#define DRIVER_NAME  "usb_mass_storage"
static ssize_t print_switch_name(struct switch_dev *sdev, char *buf)
{
	return sprintf(buf, "%s\n", DRIVER_NAME);
}

static ssize_t print_switch_state(struct switch_dev *sdev, char *buf)
{
    bool connect;
    
    if (is_connected_gpio() && isconnected)
        connect = 1;
    else
        connect = 0;

	return sprintf(buf, "%s\n", (connect ? "online" : "offline"));
//	return sprintf(buf, "%s\n", (isconnected ? "online" : "offline"));
}
#endif

static int __init max17040_init(void)
{
#if 1 // ktj usb_switch
	indicator_dev.name = DRIVER_NAME;
	indicator_dev.print_name = print_switch_name;
	indicator_dev.print_state = print_switch_state;
	switch_dev_register(&indicator_dev);
#endif

	wake_lock_init(&vbus_wake_lock, WAKE_LOCK_SUSPEND, "vbus_present"); /* ktj */

	return i2c_add_driver(&max17040_i2c_driver);
}
module_init(max17040_init);

static void __exit max17040_exit(void)
{
	i2c_del_driver(&max17040_i2c_driver);
}
module_exit(max17040_exit);

MODULE_AUTHOR("Minkyu Kang <mk7.kang@samsung.com>");
MODULE_DESCRIPTION("MAX17040 Fuel Gauge");
MODULE_LICENSE("GPL");
