/*
 * drivers/input/misc/isa1200.c
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

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/types.h>

#include <plat/gpio-cfg.h>
#include <mach/gpio.h>


//----------------------------------------------------------------------------------
//#define rdebug(fmt,arg...) printk(KERN_CRIT "___[isa1200] " fmt "\n",## arg)
#define rdebug(fmt,arg...)
//----------------------------------------------------------------------------------

#define GPIO_HAPTIC_LEN         S5PV210_GPJ3(2)  
#define GPIO_HAPTIC_HEN         S5PV210_GPJ3(3)  

#define GPIO_LEVEL_HIGH         1
#define GPIO_LEVEL_LOW          0


// Haptic Driver's Control Register Address Definition
#define HCTRL0 0x30
#define HCTRL1 0x31
#define HCTRL2 0x32
#define HCTRL3 0x33
#define HCTRL4 0x34
#define HCTRL5 0x35
#define HCTRL6 0x36
#define HCTRL7 0x37
#define HCTRL8 0x38
#define HCTRL9 0x39
#define HCTRLA 0x3A
#define HCTRLB 0x3B
#define HCTRLC 0x3C
#define HCTRLD 0x3D

#define ISA1200_NAME    "isa1200"

extern void vibetonz_start(void);
extern void vibetonz_end(void);


struct isa1200_data {
	atomic_t enable;                /* attribute value */
	struct i2c_client *client;
};

static int isa1200_detect(struct i2c_client *client)
{
    int i;
	int data;

#if 0

	gpio_direction_output(GPIO_HAPTIC_LEN, GPIO_LEVEL_HIGH);
	gpio_direction_output(GPIO_HAPTIC_HEN, GPIO_LEVEL_HIGH);

    mdelay(5);  // for charge on

	gpio_set_value(GPIO_HAPTIC_HEN, GPIO_LEVEL_LOW);	

    mdelay(1);
    
#else
	if (gpio_is_valid(GPIO_HAPTIC_LEN)) {
		if (gpio_request(GPIO_HAPTIC_LEN, "GPJ3")) 
			printk(KERN_ERR "Failed to request GPIO_HAPTIC_LEN!\n");

		gpio_direction_output(GPIO_HAPTIC_LEN, GPIO_LEVEL_HIGH);
	}
//	gpio_free(GPIO_HAPTIC_LEN);	

	if (gpio_is_valid(GPIO_HAPTIC_HEN)) {
		if (gpio_request(GPIO_HAPTIC_HEN, "GPJ3")) 
			printk(KERN_ERR "Failed to request GPIO_HAPTIC_HEN!\n");

		gpio_direction_output(GPIO_HAPTIC_HEN, GPIO_LEVEL_HIGH);
	}
    mdelay(5);  // for charge on
	gpio_set_value(GPIO_HAPTIC_HEN, GPIO_LEVEL_LOW);	
//	gpio_free(GPIO_HAPTIC_HEN);	
#endif

//---------------------------------------------------------------------------//


	i2c_smbus_write_byte_data(client, 0x31, 0x40);
	i2c_smbus_write_byte_data(client, 0x32, 0x00);
	i2c_smbus_write_byte_data(client, 0x30, 0x89);


#if 0
    i=0;
   	data = i2c_smbus_read_byte_data(client, i);
    rdebug("address 0x%02x = 0x%02x", i, data);
    
    for (i=0x30 ; i<0x3F; i++)
    {
    	data = i2c_smbus_read_byte_data(client, i);
        rdebug("address 0x%02x = 0x%02x", i, data);
    }    
#endif

/*
	data = i2c_smbus_read_byte_data(client, HCTRL0);
    rdebug("HCTRL0 = 0x%2x", data);

	data = i2c_smbus_read_byte_data(client, HCTRL1);
    rdebug("HCTRL1 = 0x%2x", data);

	data = i2c_smbus_read_byte_data(client, HCTRL3);
    rdebug("HCTRL3 = 0x%2x", data);
*/

	return 0;
}

static int isa1200_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct isa1200_data *isa1200;
	int err;

    rdebug("%s called", __func__);

	/* setup private data */
	isa1200 = kzalloc(sizeof(struct isa1200_data), GFP_KERNEL);
	if (!isa1200) {
		err = -ENOMEM;
		goto error_0;
	}

	/* setup i2c client */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto error_1;
	}
	i2c_set_clientdata(client, isa1200);
	isa1200->client = client;

	/* detect and init hardware */
	if ((err = isa1200_detect(client))) {
		goto error_1;
	}
	dev_info(&client->dev, "%s found\n", id->name);

    vibetonz_start();

    rdebug("%s success", __func__);

	return 0;
    

error_1:
	kfree(isa1200);
error_0:
	return err;
}

static int isa1200_remove(struct i2c_client *client)
{
	return 0;
}

static int isa1200_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}

static int isa1200_resume(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id isa1200_id[] = {
	{ISA1200_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, isa1200_id);

static struct i2c_driver isa1200_i2c_driver = {
	.driver = {
		   .name = ISA1200_NAME,
		   .owner = THIS_MODULE,
	},
	.probe      = isa1200_probe,
	.remove     = isa1200_remove,
	.suspend    = isa1200_suspend,
	.resume     = isa1200_resume,
	.id_table   = isa1200_id,
};

/*
 * Module init and exit
 */
static int __init isa1200_init(void)
{
	return i2c_add_driver(&isa1200_i2c_driver);
} 
module_init(isa1200_init);

static void __exit isa1200_exit(void)
{
	i2c_del_driver(&isa1200_i2c_driver);
}
module_exit(isa1200_exit);

MODULE_DESCRIPTION("Motor driver for ISA1200");
MODULE_AUTHOR("iriver/ktj");
MODULE_LICENSE("GPL");
