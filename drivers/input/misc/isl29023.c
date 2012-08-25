/*
 * drivers/input/misc/isl29023.c
 *
 * Ambient light sensorr driver for ISL29023
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
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio-bank.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include "isl29023.h"
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/wakelock.h>
#include <linux/earlysuspend.h>
#include <linux/suspend.h>
#endif

#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>


/* ktj */
//----------------------------------------------------------------------------------
//  #define rdebug(fmt,arg...) printk(KERN_CRIT "___[isl29023] " fmt "\n",## arg)
    #define rdebug(fmt,arg...)
//----------------------------------------------------------------------------------

#define  USE_ALS_IRQ		/* ktj, use als irq */				

#define GPIO_ALS_INT    S5PV210_GPH0(3)  

static int debug_enable = 0;
struct isl29023_data *this_als_ir_data;

void set_debug_lightsensor(int enable)
{
    if (enable)
        debug_enable = 1;
    else
        debug_enable = 0;
}
EXPORT_SYMBOL(set_debug_lightsensor);



/*
 * driver private data
 */
struct isl29023_data {
	struct input_dev *idev;
	struct i2c_client       *client;
	struct work_struct      wq;
	struct workqueue_struct *working_queue;
	int enabled;
	int suspened;//Shanghai ewada
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend	power;
#endif
}the_data;

static int isl29023_read_reg(struct isl29023_data *als_ir_data, uint8_t reg,
		   uint8_t *value)
{
	int error = 0;
	int i = 0;
	uint8_t dest_buffer;

	if (!value) {
		pr_err("%s: invalid value pointer\n", __func__);
		return -EINVAL;
	}
	do {
		dest_buffer = reg;
		error = i2c_master_send(als_ir_data->client, &dest_buffer, 1);
		if (error == 1) {
			error = i2c_master_recv(als_ir_data->client,
				&dest_buffer, LD_ISL29023_ALLOWED_R_BYTES);
		}
		if (error != LD_ISL29023_ALLOWED_R_BYTES) {
			pr_err("%s: read[%i] failed: %d\n", __func__, i, error);
			msleep_interruptible(LD_ISL29023_I2C_RETRY_DELAY);
		}
	} while ((error != LD_ISL29023_ALLOWED_R_BYTES) &&
			((++i) < LD_ISL29023_MAX_RW_RETRIES));

	if (error == LD_ISL29023_ALLOWED_R_BYTES) {
		error = 0;
		*value = dest_buffer;
	}

	return error;
}

static int isl29023_write_reg(struct isl29023_data *als_ir_data,
							uint8_t reg,
							uint8_t value)
{
	uint8_t buf[LD_ISL29023_ALLOWED_W_BYTES] = { reg, value };
	int bytes;
	int i = 0;

	do {
		bytes = i2c_master_send(als_ir_data->client, buf,
					LD_ISL29023_ALLOWED_W_BYTES);

		if (bytes != LD_ISL29023_ALLOWED_W_BYTES) {
			pr_err("%s: write %d failed: %d\n", __func__, i, bytes);
			msleep_interruptible(LD_ISL29023_I2C_RETRY_DELAY);
		}
	} while ((bytes != (LD_ISL29023_ALLOWED_W_BYTES))
		 && ((++i) < LD_ISL29023_MAX_RW_RETRIES));

	if (bytes != LD_ISL29023_ALLOWED_W_BYTES) {
		pr_err("%s: i2c_master_send error\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static int ld_isl29023_init_registers(struct isl29023_data *data)
{
	u8 low_lsb,low_msb, high_lsb, high_msb;
    uint8_t command1, command2;

    low_msb  = 0x4E; low_lsb  = 0x20;   // 20000 
    high_msb = 0x9C; high_lsb = 0x40;   // 40000
    
    //command1 = ISL29023_OPERATION_MODE_ALS_CONTINUE | ISL29023_INT_PERSIST_4;
    /*Shanghai ewada modify to open isl29023 just at enabled*/
    command1 = 0;
    command2 = ISL29023_RESOLUTION_16BIT | ISL29023_LUX_RANGE4;

	isl29023_write_reg(data, ISL29023_COMMAND_II, command2);
	isl29023_write_reg(data, ISL29023_ALSIR_LT_LSB,	low_lsb);
	isl29023_write_reg(data, ISL29023_ALSIR_LT_MSB,	low_msb);
	isl29023_write_reg(data, ISL29023_ALSIR_HT_LSB,	high_lsb);
	isl29023_write_reg(data, ISL29023_ALSIR_HT_MSB,	high_msb);

	isl29023_write_reg(data, ISL29023_COMMAND_I, command1);     // start

#if 0 /* ktj test */
    int i;
	u8 reg_val;

    for (i=0; i<8; i++)
    {
	    isl29023_read_reg(data, i, &reg_val);
	    rdebug("addr 0x%x = 0x%x ", i, reg_val);
	}
#endif	
	
	return 0;
}

#if 0 // for test
static int light_conut = 0;
static int test_lux = 50;
#endif

#define MAX_LEVEL 8
#define  MAX_LUX 64000
static unsigned int LUX_TO_LEVEL[MAX_LEVEL][2] = {
						 //light's value      report value     hal value    android value
                                                  {10, 			1}, 		//40            40
                                                  {48, 			5},		//160           40
                                                  {188, 		9},		//260           220
                                                  {520, 		13},		//390           220
                                                  {1880, 		17},		//540           430
                                                  {4500, 		21},		//750		430
                                                  {6000, 		25},		//1280		800
                                                  {MAX_LUX, 		33}		//10240         9600
                                                  };

static int isl29023_read_adj_als(struct isl29023_data *isl)
{
	int lens_adj_lux = -1;
	unsigned int als_read_data = 0;
	unsigned char als_lower;
	unsigned char als_upper;

	int i;
	isl29023_read_reg(isl, ISL29023_ALSIR_DT_LSB, &als_lower);
	isl29023_read_reg(isl, ISL29023_ALSIR_DT_MSB, &als_upper);

	als_read_data = (als_upper << 8);
	als_read_data |= als_lower;
    
	lens_adj_lux = 64000 * als_read_data / 65536;

	for(i = 0; i < MAX_LEVEL -1; i++){

              if(lens_adj_lux >= LUX_TO_LEVEL[i][0])
                                continue;
                        else
                                break;
	}
        lens_adj_lux = LUX_TO_LEVEL[i][1];


    // ktj, tuned range
    	//lens_adj_lux = lens_adj_lux / 5;

#if 0 // for test
    light_conut++;
    if ( light_conut % 5 == 0)
        test_lux--;
    
    if (test_lux == 0)
        test_lux = 50;        
#endif


//	return test_lux;
	return lens_adj_lux;
}

static void isl29023_report_input(struct isl29023_data *isl)
{
        //int ret = 0;
        int lux_val;
        u8 als_prox_int;
        static int lasttime_lux_val;//2011.3.21. add  for not to report repeat input events when same value

        isl29023_read_reg(isl, ISL29023_COMMAND_I, &als_prox_int);
        //rdebug("als_prox_int = 0x%x", als_prox_int);

        if (als_prox_int & ISL29023_ALS_FLAG_MASK) 
        {
            lux_val = isl29023_read_adj_als(isl);
            //rdebug("lux_val = %d", lux_val);
#if 1
            if (lux_val >= 0) {
                rdebug("lux_val = %d, isl->idev=0x%x", lux_val,isl->idev);
                //2011.3.21. add for not to report repeat input events when same value                        
                if (lux_val==lasttime_lux_val)                                                                                     
                    return;
                //2011.3.21  add end
                
                input_report_abs(isl->idev, ABS_MISC, lux_val);
                input_sync(isl->idev);

			//input_event(isl->idev, EV_LED, LED_MISC, lux_val);
			//input_sync(isl->idev);
            }
#endif
            lasttime_lux_val=lux_val;//2011.3.21. add  for not to report repeat input events when same value
        }
}

#ifdef USE_ALS_IRQ
void isl29023_enable_irq(struct isl29023_data *als_ir_data)
{
	enable_irq(als_ir_data->client->irq);
}

void isl29023_disable_irq(struct isl29023_data *als_ir_data)
{
	disable_irq_nosync(als_ir_data->client->irq);
}
#endif

void ld_isl29023_work_queue(struct work_struct *work)
{
	struct isl29023_data *als_ir_data = container_of(work, struct isl29023_data, wq);

	isl29023_report_input(als_ir_data);
#ifdef USE_ALS_IRQ
	isl29023_enable_irq(als_ir_data);
#endif
}

#ifdef USE_ALS_IRQ
irqreturn_t ld_isl29023_irq_handler(int irq, void *dev)
{
	struct isl29023_data *als_ir_data = dev;

	disable_irq_nosync(irq); 
	queue_work(als_ir_data->working_queue, &als_ir_data->wq);
	
	return IRQ_HANDLED;
}
#endif

static int isl29023_enable(struct isl29023_data *data)
{
    int rc= -EIO;
    rdebug("%s\n", __func__);
	
    if (data->enabled && (!data->suspened)) {
        rdebug("%s: already enabled\n", __func__);
        return 0;
    }
    //Shanghai ewada
	data->suspened = 0;
	uint8_t command1 = ISL29023_OPERATION_MODE_ALS_CONTINUE | ISL29023_INT_PERSIST_4;
	//2011.3.24  add "rc=" to fix ioctl error
    rc=isl29023_write_reg(this_als_ir_data, ISL29023_COMMAND_I, command1);     // start
	
    data->enabled = 1;
#ifdef USE_ALS_IRQ
    isl29023_enable_irq(data);
#endif

    return rc;
}

static int isl29023_disable(struct isl29023_data *data)
{
    int rc = -EIO;

    rdebug("%s\n", __func__);
    if (!data->enabled) {
        rdebug("%s: already disabled\n", __func__);
        return 0;
    }
    //Shanghai ewada
    //2011.3.24  add "rc=" to fix ioctl error
	rc=isl29023_write_reg(this_als_ir_data, ISL29023_COMMAND_I, 0x00);     // stop
#ifdef USE_ALS_IRQ
    isl29023_disable_irq(data);
#endif
    data->enabled = 0;
    return rc;
}

/***** Isl29023_fops functions ***************************************/
static int Isl29023_open(struct inode *inode, struct file *file)
{
//  rdebug("%s called", __func__);
    return 0;
}

static int Isl29023_close(struct inode *inode, struct file *file)
{
//  rdebug("%s called", __func__);
    return 0;
}

static ssize_t Isl29023_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{	
    return 0;
}

static ssize_t Isl29023_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    return 0;
}

//static long isl29023_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
static int Isl29023_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
    int val = 0;
         
    rdebug("%s cmd %d\n", __func__, _IOC_NR(cmd));
    switch (cmd) {
        case ISL29023_IOCTL_ENABLE:
            if (get_user(val, (unsigned long __user *)arg))
                return -EFAULT;
            if (val)
            {
                rdebug("%s isl29023_enable\n", __func__);
                return isl29023_enable(this_als_ir_data);
            }		
            else
            {
                rdebug("%s isl29023_disable\n", __func__);
                return isl29023_disable(this_als_ir_data);
            }
            break;
        case ISL29023_IOCTL_GET_ENABLED:
            return put_user(this_als_ir_data->enabled, (unsigned long __user *)arg);
            break;
        default:
            pr_err("%s: invalid cmd %d\n", __func__, _IOC_NR(cmd));
            return -EINVAL;
    }
}


static const struct file_operations Isl29023_fops = {
	.owner      = THIS_MODULE,
	.read       = Isl29023_read,
	.write      = Isl29023_write,
	.open       = Isl29023_open,
	.release    = Isl29023_close,
	.ioctl      = Isl29023_ioctl,
};

static struct miscdevice Isl29023_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "Isl29023_dev",
	.fops = &Isl29023_fops,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ld_isl29023_early_suspend(struct early_suspend *h)
{
    rdebug("%s called\n", __func__);
	printk("[KENEL] %s, %d \n", __FUNCTION__, __LINE__);
	//Shanghai ewada
//	if (this_als_ir_data->enabled) 
	{
		this_als_ir_data->suspened = 1;
//		wmb();
#ifdef USE_ALS_IRQ
    	isl29023_disable_irq(this_als_ir_data);
#endif
		isl29023_write_reg(this_als_ir_data, ISL29023_COMMAND_I, 0x00);     // stop

	}

    return;
}

static void ld_isl29023_late_resume	(struct early_suspend *h)
{
    uint8_t command1 = ISL29023_OPERATION_MODE_ALS_CONTINUE | ISL29023_INT_PERSIST_4;

    //Shanghai ewada
//	if (this_als_ir_data->enabled && this_als_ir_data->suspened) 
	{
		this_als_ir_data->suspened = 0;
//		wmb();
		isl29023_write_reg(this_als_ir_data, ISL29023_COMMAND_I, command1);     // start
#ifdef USE_ALS_IRQ
    	isl29023_enable_irq(this_als_ir_data);
#endif
	}

    rdebug("%s called\n", __func__);

    return;
}
#endif

static int ld_isl29023_probe(struct i2c_client *client,   const struct i2c_device_id *id)
{
        struct isl29023_data *als_ir_data;
//        struct input_dev *input_dev;
        int error = 0;

        rdebug("%s called\n", __func__);

        /* setup i2c client */
        if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
            error = -ENODEV;
            goto error_1;
        }

        /* setup private data */
        als_ir_data = kzalloc(sizeof(struct isl29023_data), GFP_KERNEL);
        if (als_ir_data == NULL) {
            error = -ENOMEM;
            goto error_0;
        }

        i2c_set_clientdata(client, als_ir_data);
        als_ir_data->client = client;

        /* Declare input device */
        als_ir_data->idev  = input_allocate_device();
        if (!als_ir_data->idev) {
            pr_err("%s: could not allocate input device\n", __func__);
            error = -ENOMEM;
	     goto error_0;
        }

	 rdebug("james debug als_ir_data->idev = 0x%x" , als_ir_data->idev);
	 input_set_capability(als_ir_data->idev, EV_ABS, ABS_MISC);
        //set_bit(EV_ABS, als_ir_data->idev->evbit);
        input_set_abs_params(als_ir_data->idev, ABS_DISTANCE, 0, 1, 0, 0);
        input_set_drvdata(als_ir_data->idev, als_ir_data);

        als_ir_data->idev->name = "lightsensor";

        error = input_register_device(als_ir_data->idev);
        if (error < 0) {
            pr_err("%s: could not register input device\n", __func__);
            goto err_unregister_input_device;
        }
    
    /* queueÀÇ »ý¼º */
	als_ir_data->working_queue = create_singlethread_workqueue("als_wq");
	if (!als_ir_data->working_queue) {
		rdebug("%s: Cannot create work queue\n", __func__);
		error = -ENOMEM;
		goto error_1;
	}
	INIT_WORK(&als_ir_data->wq, ld_isl29023_work_queue);

    /* setup irq */
	s3c_gpio_cfgpin(GPIO_ALS_INT, S3C_GPIO_SFN(0));
	s3c_gpio_setpull(GPIO_ALS_INT, S3C_GPIO_PULL_UP);

#ifdef USE_ALS_IRQ
	error = request_irq(    als_ir_data->client->irq, 
	                        ld_isl29023_irq_handler, 
	                        IRQF_TRIGGER_FALLING,
					        LD_ISL29023_NAME, 
					        als_ir_data );

	if (error != 0) {
		rdebug("%s: irq request failed: %d\n", __func__, error);
		error = -ENODEV;
		goto error_1;
	}
	rdebug("ALS_IRQ request_irq = 0x%x" , als_ir_data->client->irq);
	isl29023_disable_irq(als_ir_data);/*ewada for irq error*/
#endif

    als_ir_data->enabled = 0;
	als_ir_data->suspened = 0;
	
	/* detect and init hardware */
	ld_isl29023_init_registers(als_ir_data);

#ifdef CONFIG_HAS_EARLYSUSPEND
    als_ir_data->power.suspend  = ld_isl29023_early_suspend;
    als_ir_data->power.resume   = ld_isl29023_late_resume;
    als_ir_data->power.level    = EARLY_SUSPEND_LEVEL_DISABLE_FB;
   	register_early_suspend(&als_ir_data->power);	
#endif
    this_als_ir_data = als_ir_data;

    error = misc_register(&Isl29023_device);
    if (error) {
        rdebug("%s: Isl29023 misc device register failed: %\n", __func__, error);
        goto error_0;
    }
    else
        rdebug("%s Isl29023 misc device registered", __func__);

    rdebug("%s success", __func__);

	return 0;
    
err_unregister_input_device:
    input_unregister_device(als_ir_data->idev);
    goto error_0;
error_1:
	kfree(als_ir_data);
error_0:
	return error;
}

static int ld_isl29023_remove(struct i2c_client *client)
{
	struct isl29023_data *als_ir_data = i2c_get_clientdata(client);

	free_irq(als_ir_data->client->irq, als_ir_data);

	if (als_ir_data->working_queue)
		destroy_workqueue(als_ir_data->working_queue);

	kfree(als_ir_data);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&als_ir_data->power);
#endif

	return 0;
}

#ifndef CONFIG_HAS_EARLYSUSPEND

static int ld_isl29023_suspend(struct i2c_client *client, pm_message_t mesg)
{
    rdebug("%s called\n", __func__);
	/*should not enter if have early suspend*/
	//Shanghai ewada
	if (this_als_ir_data->enabled) {
#ifdef USE_ALS_IRQ
    	isl29023_disable_irq(this_als_ir_data);
#endif
		isl29023_write_reg(this_als_ir_data, ISL29023_COMMAND_I, 0x00);     // stop
	}
	return 0;
}

static int ld_isl29023_resume(struct i2c_client *client)
{
    rdebug("%s called\n", __func__);
	//Shanghai ewada
	uint8_t command1 = ISL29023_OPERATION_MODE_ALS_CONTINUE | ISL29023_INT_PERSIST_4;

	if (this_als_ir_data->enabled) {
    	isl29023_write_reg(this_als_ir_data, ISL29023_COMMAND_I, command1);     // start
#ifdef USE_ALS_IRQ
    	isl29023_enable_irq(this_als_ir_data);
#endif
	}
	return 0;
}
#endif

static const struct i2c_device_id isl29023_id[] = {
	{LD_ISL29023_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, isl29023_id);

static struct i2c_driver ld_isl29023_i2c_driver = {
	.driver = {
		   .name = LD_ISL29023_NAME,
		   .owner = THIS_MODULE,
	},
	.probe      = ld_isl29023_probe,
	.remove     = ld_isl29023_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend    = ld_isl29023_suspend,
	.resume     = ld_isl29023_resume,
#endif
	.id_table   = isl29023_id,
};

/*
 * Module init and exit
 */
static int __init ld_isl29023_init(void)
{
	return i2c_add_driver(&ld_isl29023_i2c_driver);
} 
module_init(ld_isl29023_init);

static void __exit ld_isl29023_exit(void)
{
	i2c_del_driver(&ld_isl29023_i2c_driver);
}
module_exit(ld_isl29023_exit);

MODULE_DESCRIPTION("ALS driver for ISL29023");
MODULE_AUTHOR("iriver/ktj");
MODULE_LICENSE("GPL");
