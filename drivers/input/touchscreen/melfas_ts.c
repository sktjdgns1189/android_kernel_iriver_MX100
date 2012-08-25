/* 
 *  melfas_ts.c - driver for MELFAS Touch device.
 *
 *  Copyright (C) 2010 Jaehwan Lim <jaehwan.lim@iriver.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 */


#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/byteorder/generic.h>
#include <linux/bitops.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio.h>



#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif /* CONFIG_HAS_EARLYSUSPEND */

#include <linux/melfas_ts.h>

#ifdef CONFIG_MX100_IOCTL
#include <mach/mx100_ioctl.h>
#endif

#define TS_MAX_X_COORD 1024
#define TS_MAX_Y_COORD 600

#define TS_READ_START_ADDR 0x10

#ifdef CONFIG_MX100_EVM
#define TS_TOUCH_NUMBER_MAX		5
#define TS_READ_REGS_LEN 7
#elif defined(CONFIG_MX100_WS)
#define TS_TOUCH_NUMBER_MAX		5
/* 2011.04.14 multi touch 3=> 5 */
#define TS_READ_REGS_LEN 23

#ifdef CONFIG_MX100_REV_ES
#define TOUCH_REV_ES
#endif

#ifdef TOUCH_REV_ES
int MX100_TouchKeycodeAllow[] = {
		KEY_SEARCH,
		KEY_MENU,
		KEY_HOME,		
		KEY_BACK
};
#else

int MX100_TouchKeycodeAllow[] = {
		KEY_HOME,		
		KEY_MENU,
		KEY_BACK,
		KEY_SEARCH
};
#endif

#endif

#define PRESS_KEY	1
#define RELEASE_KEY	0

uint32_t melfas_ts_tsdebug1;
module_param_named(tsdebug1, melfas_ts_tsdebug1, uint, 0664);
int mx100_melfasts_key_led(int onoff);

struct melfas_ts {
	struct i2c_client *client;
	struct input_dev *input;
	struct work_struct work;
	#ifdef ENABLE_TOUCH_SINGLE_WORKQUEUE
	struct workqueue_struct *work_queue_eventd;
	#endif
	struct delayed_work delayedwork_release_key;
	struct timer_list release_key_timer;

	int last_press_key;
	
	int 		ts_input_state[TS_TOUCH_NUMBER_MAX];
	int 		ts_x[TS_TOUCH_NUMBER_MAX];
	int 		ts_y[TS_TOUCH_NUMBER_MAX];
	uint8_t 	ts_z[TS_TOUCH_NUMBER_MAX];
	
	struct timer_list timer;
	struct mutex mutex;
	atomic_t irq_enabled;
	int isirq;
	struct early_suspend early_suspend;
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void melfas_ts_early_suspend(struct early_suspend *handler);
static void melfas_ts_late_resume(struct early_suspend *handler);
#endif /* CONFIG_HAS_EARLYSUSPEND */

static int MELFAS_TS_WRITE(unsigned char reg,unsigned char reg_value);
static int MELFAS_TS_READ(unsigned char reg);

/* ****************************************************************************
 * Prototypes for static functions
 * ************************************************************************** */
static void melfas_ts_xy_worker(struct work_struct *work);
static irqreturn_t melfas_ts_irq(int irq, void *handle);
static int __devinit melfas_ts_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int __devexit melfas_ts_remove(struct i2c_client *client);
static int melfas_ts_resume(struct i2c_client *client);
static int melfas_ts_suspend(struct i2c_client *client, pm_message_t message);
void melfas_ts_enable(struct melfas_ts *ts,int enable);

/* Static variables */

#ifdef CONFIG_MX100_IOCTL
int mx100_melfasts_print_log(char *log);
#endif

static const struct i2c_device_id melfas_ts_7000_id[] = {
	{ MELFAS_7000_I2C_NAME, 0 },  { }
};

static const struct i2c_device_id melfas_ts_8000_id[] = {
	{ MELFAS_8000_I2C_NAME, 0 },  { }
};

MODULE_DEVICE_TABLE(i2c, melfas_ts_7000_id);

MODULE_DEVICE_TABLE(i2c, melfas_ts_8000_id);

static struct i2c_driver melfas_ts_7000_driver = {
	.driver = {
		.name = MELFAS_7000_I2C_NAME,
		.owner = THIS_MODULE,
	},
	.probe = melfas_ts_probe,
	.remove = __devexit_p(melfas_ts_remove),
#if 1 // ktj
	.suspend = NULL,
	.resume = NULL,
#else
	.suspend = melfas_ts_suspend,
	.resume = melfas_ts_resume,
#endif
	.id_table = melfas_ts_7000_id,
};


static struct i2c_driver melfas_ts_8000_driver = {
	.driver = {
		.name = MELFAS_8000_I2C_NAME,
		.owner = THIS_MODULE,
	},
	.probe = melfas_ts_probe,
	.remove = __devexit_p(melfas_ts_remove),
#if 1 // ktj
	.suspend = NULL,
	.resume = NULL,
#else
	.suspend = melfas_ts_suspend,
	.resume = melfas_ts_resume,
#endif
	.id_table = melfas_ts_8000_id,
};

static struct i2c_device_id * g_which_i2c_device_id = 0;

#ifdef ENABLE_TOUCH_KEY_LED

#define	PERIOD_5SEC			(5*HZ)		// 5sec
struct timer_list	g_touch_key_led_timer;

int melfasts_key_led_onoff(int onoff)
{
	#ifdef TOUCH_REV_ES

	gpio_set_value(GPIO_TOUCH_KEY_LED0,onoff);
	gpio_set_value(GPIO_TOUCH_KEY_LED1,onoff);
	gpio_set_value(GPIO_TOUCH_KEY_LED2,onoff);
	gpio_set_value(GPIO_FRONT_LED_CTL_OUT,onoff);
	#else
	gpio_set_value(GPIO_FRONT_LED_CTL_OUT,onoff);
	#endif
	return 0;
}

void melfasts_key_led_refresh(void)
{
//	MX100_DBG(DBG_FILTER_TOUCH_KEY,"led timer on triggered\n");

	melfasts_key_led_onoff(1);
	
	mod_timer(&g_touch_key_led_timer, jiffies + PERIOD_5SEC);
}

void melfasts_key_led_timer_handler(unsigned long data)
{
	MX100_DBG(DBG_FILTER_TOUCH_KEY,"led timer off triggered\n");
	melfasts_key_led_onoff(0);
}


void melfasts_key_led_timer_run(void)
{
	init_timer(&g_touch_key_led_timer);
	g_touch_key_led_timer.function = melfasts_key_led_timer_handler;

	g_touch_key_led_timer.expires = jiffies + PERIOD_5SEC;

	add_timer(&g_touch_key_led_timer);
}
#endif	

void get_resume_value(void);
void get_suspend_value(void);


int pm_debug_led_ctl(int value)
{
	int i;
	
	#ifdef TOUCH_REV_ES
	if(value & 1) {
		gpio_set_value(GPIO_TOUCH_KEY_LED0,1);
	} else {
		gpio_set_value(GPIO_TOUCH_KEY_LED0,0);
	}

	if(value>>1 & 1) {
		gpio_set_value(GPIO_TOUCH_KEY_LED1,1);
	} else {
		gpio_set_value(GPIO_TOUCH_KEY_LED1,0);
	}

	if(value>>2 & 1) {
		gpio_set_value(GPIO_TOUCH_KEY_LED2,1);
	} else {
		gpio_set_value(GPIO_TOUCH_KEY_LED2,0);
	}

	if(value>>3 & 1) {
		gpio_set_value(GPIO_FRONT_LED_CTL_OUT,1);
	} else {
		gpio_set_value(GPIO_FRONT_LED_CTL_OUT,0);
	}
	#else
	gpio_set_value(GPIO_FRONT_LED_CTL_OUT,onoff);
	#endif
	return 0;
}


void init_pm_debug_led_ctl(void)
{
	int i;

	for(i=0;i<4;i++) {
		pm_debug_led_ctl(1<< i);
		msleep(100);
	}
	for(i=0;i<4;i++) {
		pm_debug_led_ctl(15>> i);
		msleep(100);
	}

	pm_debug_led_ctl(0x0);
	msleep(500);
	pm_debug_led_ctl(0xf);
	msleep(500);
	pm_debug_led_ctl(0x0);

}


#ifdef CONFIG_MX100_IOCTL
int mx100_melfasts_reg_write(int addr,int data)
{
	return MELFAS_TS_WRITE(addr,data);
}

int mx100_melfasts_reg_read(int addr)
{
	return MELFAS_TS_READ(addr);
}

int mx100_melfasts_key_led(int onoff)
{
	#ifdef ENABLE_TOUCH_KEY_LED
	melfasts_key_led_onoff(onoff);
	#endif
	
	return 0;
}


static int g_log_start_time=0;
static int g_log_enable = 0;
	

static struct ts_kernel_file_io g_touch_log;

int mx100_melfasts_log_start(char *logfile)
{
	struct timeval tv;

	#ifdef DEBUG_TOUCH_NOISE
	k_close(&g_touch_log);

	klog_open(&g_touch_log,logfile, 1024 *128);

	do_gettimeofday(&tv);
	g_log_start_time = tv.tv_sec;

	mx100_melfasts_print_log("mx100 touch log start\n");

	g_log_enable = 1;
	#endif	
	return 0;
}

int mx100_melfasts_print_log(char *logMsg)
{
	#ifdef DEBUG_TOUCH_NOISE

	char szlog[128];
	struct timeval tv;
	int write_size;
	
	if(g_log_enable==1) {

		do_gettimeofday(&tv);

		sprintf(szlog,"%06d %s",(int)tv.tv_sec-g_log_start_time,logMsg);

		mx100_printk(szlog);

		write_size=klog_printf(&g_touch_log,szlog);

		if(write_size == 0) {
			 g_touch_log.file_pos = 0;
//			mx100_melfasts_log_stop();
		}
	}
	#endif
	return 0;
}
int mx100_melfasts_log_stop(void)
{
	#ifdef DEBUG_TOUCH_NOISE
	k_close(&g_touch_log);
	g_log_enable = 0;
	#endif
	return 0;
}

#endif
static  struct i2c_client *g_melfas_i2c_client;

static int MELFAS_TS_READ(unsigned char reg)
{
	int ret = 0;
	uint8_t buf[TS_READ_REGS_LEN];

	if(!g_melfas_i2c_client) {
		printk(KERN_ERR "g_melfas_i2c_client is null\n");

		return -1;
	}
	buf[0] = reg;
	
	ret = i2c_master_send(g_melfas_i2c_client, buf, 1);
	if (ret < 0)  {
		printk(KERN_ERR "melfas_ts_work_func: i2c_master_send() failed\n");
		return -1;
	}
	ret = i2c_master_recv(g_melfas_i2c_client, buf, 1);
	if (ret < 0) {
		printk(KERN_ERR "melfas_ts_work_func: i2c_master_recv() failed\n");
		return -1;
	} 


	return buf[0];

}

static int MELFAS_TS_WRITE(unsigned char reg,unsigned char reg_value)
{
	int ret = 0;
	uint8_t buf[TS_READ_REGS_LEN];

	if(!g_melfas_i2c_client) {
		printk(KERN_ERR "g_melfas_i2c_client is null\n");

		return -1;
	}
	buf[0] = reg;
	
	ret = i2c_master_send(g_melfas_i2c_client, buf, 1);
	if (ret < 0) 
		printk(KERN_ERR "melfas_ts_work_func: i2c_master_send() failed\n");

	return buf[0];
}


void set_xy_cord(int width_res,int height_res)
{

//	int xy_size_upper = 0;
//	int x_size_lower,y_size_lower = 0;

	int ret = 0;
	uint8_t buf[TS_READ_REGS_LEN];

	if(!g_melfas_i2c_client) {
		printk(KERN_ERR "g_melfas_i2c_client is null\n");

		return ;
	}
	buf[0] = 0x0a;
	#if 0
	buf[1] = 0x42;
	buf[2] = 0x58;
	buf[3] = 0x00;
	#else
	buf[1] = 0x24;
	buf[2] = 0x00;
	buf[3] = 0x58;

	#endif
	ret = i2c_master_send(g_melfas_i2c_client, buf, 4);
	if (ret < 0) 
		printk(KERN_ERR "melfas_ts_work_func: i2c_master_send() failed\n");
}


static ssize_t melfas_ts_irq_status(struct device *dev,
                                 struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = container_of(dev,
	                                         struct i2c_client, dev);
	struct melfas_ts *ts = i2c_get_clientdata(client);
	return sprintf(buf, "%u\n", atomic_read(&ts->irq_enabled));
}

static ssize_t melfas_ts_irq_enable(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf, size_t size)
{
	struct i2c_client *client = container_of(dev,
	                                         struct i2c_client, dev);
	struct melfas_ts *ts = i2c_get_clientdata(client);
	int err = 0;
	unsigned long value;
/*
	struct qtm_obj_message *msg;
*/

	if (size > 2)
		return -EINVAL;

	err = strict_strtoul(buf, 10, &value);
	if (err != 0)
		return err;

	switch (value) {
	case 0:
		if (atomic_cmpxchg(&ts->irq_enabled, 1, 0)) {
			pr_info("touch irq disabled!\n");
			disable_irq_nosync(ts->client->irq);
		}
		err = size;
		break;
	case 1:
		if (!atomic_cmpxchg(&ts->irq_enabled, 0, 1)) {
			pr_info("touch irq enabled!\n");
/*
			msg = melfas_ts_read_msg(ts);
			if (msg == NULL)
				pr_err("%s: Cannot read message\n", __func__);
*/
			enable_irq(ts->client->irq);
		}
		err = size;
		break;
	default:
		pr_info("melfas_ts_irq_enable failed -> irq_enabled = %d\n",
		atomic_read(&ts->irq_enabled));
		err = -EINVAL;
		break;
	}

	return err;
}




static DEVICE_ATTR(irq_enable, 0777, melfas_ts_irq_status, melfas_ts_irq_enable);


/* 2011.06.21 fix missing touch key release key issue  */

#define CORRECT_RELEASE_KEY_INTERVAL (msecs_to_jiffies(200))

void refresh_release_touch_key(struct melfas_ts *ts)
{
	mod_timer(&ts->release_key_timer, jiffies + CORRECT_RELEASE_KEY_INTERVAL);
}

void release_touch_key_handler(unsigned long data)
{
	struct melfas_ts *ts = (struct melfas_ts *) data;

	if(ts->last_press_key !=0) {
		input_report_key(ts->input, ts->last_press_key, RELEASE_KEY);	
		input_sync(ts->input);
		ts->last_press_key = 0;
		MX100_DBG(DBG_FILTER_TOUCH_KEY,"touch key release2 :%d\n",ts->last_press_key);
	}
}


void init_release_touch_key(struct melfas_ts *ts)
{
	init_timer(&ts->release_key_timer)
		;
	ts->release_key_timer.function = release_touch_key_handler;
	ts->release_key_timer.expires = jiffies + CORRECT_RELEASE_KEY_INTERVAL;
	ts->release_key_timer.data = (unsigned long)ts;
	
	add_timer(&ts->release_key_timer);
}


#define	TOUCH_PRESS				1
#define	TOUCH_RELEASE			0

#ifdef CONFIG_MX100_WS
//added 2011.06.15
void __inline process_multi_touch(struct melfas_ts *ts,int ts_input_number) 
{
	int i;
	
	for(i=0;i<TS_TOUCH_NUMBER_MAX	;i++) {

		if(ts_input_number > 0)	{
			if(ts->ts_input_state[i]) {
				input_report_abs(ts->input, ABS_MT_POSITION_X, ts->ts_x[i]);
				input_report_abs(ts->input, ABS_MT_POSITION_Y, ts->ts_y[i]);
				input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, 1);   // press               
				input_report_abs(ts->input, ABS_MT_WIDTH_MAJOR, 20);			
				input_mt_sync(ts->input);

				MX100_DBG(DBG_FILTER_TOUCH_XY,"down %d  x: %4d,  y: %4d\n", i, ts->ts_x[i], ts->ts_y[i]);
			}					
		}
		else {
			if(ts->ts_x[i]!=-1 && ts->ts_y[i]!=-1) {
				input_report_abs(ts->input, ABS_MT_POSITION_X,ts->ts_x[i]);
				input_report_abs(ts->input, ABS_MT_POSITION_Y, ts->ts_y[i]);
				input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, 0);   // release             
				input_report_abs(ts->input, ABS_MT_WIDTH_MAJOR, 20);
				input_mt_sync(ts->input);
				
				MX100_DBG(DBG_FILTER_TOUCH_XY,"  up %d  x: %4d,  y: %4d\n\n",i,ts->ts_x[i],ts->ts_y[i]);
				ts->ts_x[i] = -1;
				ts->ts_y[i] = -1;
			}
		}
	}
	
	input_sync(ts->input);
}		
void melfas_ts_xy_worker(struct work_struct *work)
{
	struct melfas_ts *ts = container_of(work,struct melfas_ts,work);
	uint8_t buf[TS_READ_REGS_LEN];
	int ret = 0;


	//printk(KERN_ERR "%p test ============= xp\n",ts->client);
		
	#if 1
	buf[0] = TS_READ_START_ADDR;
	ret = i2c_master_send(ts->client, buf, 1);
	if (ret < 0) {
		printk(KERN_ERR "melfas_ts_work_func: i2c_master_send() failed\n");
	}

	ret = i2c_master_recv(ts->client, buf, TS_READ_REGS_LEN);

	if (ret < 0) {
		printk(KERN_ERR "melfas_ts_work_func: i2c_master_recv() failed\n");
		//melfas_ts_enable(ts,true);  /* added 2011.06.24  */
	} else
	#endif
	{
		int i;
		int tk_press,tk_index,tk_code,tk_type;

		uint8_t ts_input_number;
		uint8_t ts_input_position;


		uint8_t *buf_point;

		ts_input_number   = buf[0] & 0x0F;					// Number of fingers
		ts_input_position = buf[1] & 0x1F;					// TS Position

		for (i = 0; i < TS_TOUCH_NUMBER_MAX; i++)			// Get points
		{
			ts->ts_input_state[i] = ( ts_input_position >> i ) & 0x01;
			if (!ts->ts_input_state[i])
				continue;
			
			buf_point = &buf[2 + i*3];
			ts->ts_x[i] = ((uint16_t)(buf_point[0]&0xF0) << 4)  + (uint16_t)buf_point[1];		// High 4 bits | Low 8 bits
			ts->ts_y[i] = ((uint16_t)(buf_point[0]&0x0F) << 8)  + (uint16_t)buf_point[2];		// High 4 bits | Low 8 bits
			ts->ts_z[i] = (uint16_t)buf[17 + i];

			if(ts->ts_input_state[i]) {
			}	
		}

		#ifdef DEBUG_TOUCH_NOISE
		if(g_log_enable==1 || g_mx100_system_log==1) {
			char szlog[128];

			int write_len=0;

			write_len+= sprintf(&szlog[write_len],"%02x %2x (%03d,%03d), (%03d,%03d)",
			buf[0],
			buf[1],
			ts->ts_x[0],
			ts->ts_y[0],
			ts->ts_input_state[1] ? ts->ts_x[1] : 0,
			ts->ts_input_state[1] ? ts->ts_y[1] : 0);

			write_len+= sprintf(&szlog[write_len],"\n");

			if(g_mx100_system_log==1) {
				printk(szlog);
			} else {
				mx100_melfasts_print_log(szlog);
			}
		}
		#endif

		process_multi_touch(ts,ts_input_number);

		tk_type = buf[22] & 0x40;
		if(tk_type !=0) 
		 {
			tk_press = buf[22] & 0x8;
			tk_index = (buf[22] & 0x7) - 1;
 
			if(tk_index >=0 && tk_index < 4) {
				tk_code = MX100_TouchKeycodeAllow[tk_index];


				if(tk_press==0) {
					MX100_DBG(DBG_FILTER_TOUCH_KEY,"touch key press :%d %d\n",tk_index,tk_code);

					input_report_key(ts->input, tk_code, PRESS_KEY);	
					ts->last_press_key = tk_code;
					refresh_release_touch_key(ts);
				} else {
					MX100_DBG(DBG_FILTER_TOUCH_KEY,"touch key release :%d\n",tk_index);
					input_report_key(ts->input, tk_code, RELEASE_KEY);	
					ts->last_press_key = 0;
				}


			}

			input_sync(ts->input);
		} 	


	}

	#ifdef ENABLE_TOUCH_KEY_LED
	melfasts_key_led_refresh();
	#endif
	
//exit_xy_worker:
	 {
		if(ts->client->irq == 0) {
			/* restart event timer */
			mod_timer(&ts->timer, jiffies + TOUCHSCREEN_TIMEOUT);
		}
		else {
			/* re-enable the interrupt after processing */
			enable_irq(ts->client->irq);
		}
	}
	return;
}

#else
void melfas_ts_xy_worker(struct work_struct *work)
{
	struct melfas_ts *ts = container_of(work,struct melfas_ts,work);
	uint8_t buf[TS_READ_REGS_LEN];
	int ret = 0;


	//printk(KERN_ERR "%p test ============= xp\n",ts->client);
		
	#if 1
	buf[0] = TS_READ_START_ADDR;
	ret = i2c_master_send(ts->client, buf, 1);
	if (ret < 0) {
	//	printk(KERN_ERR "melfas_ts_work_func: i2c_master_send() failed\n");
	}

	ret = i2c_master_recv(ts->client, buf, TS_READ_REGS_LEN);
	if (ret < 0) {
		printk(KERN_ERR "melfas_ts_work_func: i2c_master_recv() failed\n");
	} else
	#endif
	{
		int ts->ts_x,ts->ts_y; //,ts_x1,ts_y1,ts_x2,ts_y2;
		int xy_upper;
		uint8_t ts_input_type = buf[0] & 0x7;
		uint8_t ts->ts_z = buf[4];
		uint8_t tk_input_type = (buf[0] & 0x18) >> 3;
		int tk_press,tk_index,tk_code,tk_type;

		{
			xy_upper = buf[2];
		
			ts->ts_x = ((uint16_t)((xy_upper>>4) & 0x0f) << 8) | buf[3];		
			ts->ts_y = ((uint16_t)(xy_upper & 0x0f) << 8) | buf[4];


	//		printk(KERN_ERR "melfas_ts_work_func: %x %x x: %d, y: %d, z: %d, tk: %d\r",buf[0],buf[1], ts->ts_x, ts->ts_y, ts->ts_z, tk_posi);
			if (ts_input_type) {
				// printk(KERN_ERR "melfas_ts_work_func:  x: %d, y: %d, z: %d\r", ts->ts_x, ts->ts_y, ts->ts_z);
	
				input_report_abs(ts->input, ABS_MT_POSITION_X/*ABS_X*/, ts->ts_x);
				input_report_abs(ts->input, ABS_MT_POSITION_Y/*ABS_Y*/, ts->ts_y);
				input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, 1);
			}

			input_report_abs(ts->input, ABS_PRESSURE, ts->ts_z);
			input_report_abs(ts->input, BTN_TOUCH, 1);
			input_mt_sync(ts->input);
			input_sync(ts->input);
		}	
	}
	
//exit_xy_worker:
	 {
		if(ts->client->irq == 0) {
			/* restart event timer */
			mod_timer(&ts->timer, jiffies + TOUCHSCREEN_TIMEOUT);
		}
		else {
			/* re-enable the interrupt after processing */
			enable_irq(ts->client->irq);
		}
	}
	return;
}

#endif


/* Timer function used as dummy interrupt driver */
static void melfas_ts_timer(unsigned long handle)
{
	struct melfas_ts *ts = (struct melfas_ts *) handle;

//	melfas_ts_debug("TTSP Device timer event\n");

	/* schedule motion signal handling */
	schedule_work(&ts->work);

	return;
}

/* ************************************************************************
 * ISR function. This function is general, initialized in drivers init
 * function
 * ************************************************************************ */
static irqreturn_t melfas_ts_irq(int irq, void *handle)
{
	struct melfas_ts *ts = (struct melfas_ts *) handle;

	#ifdef DEBUG_TOUCH_NOISE
	if(g_log_enable==1) {
		//mx100_printk("melfas ts irq triggered.\n");
	}	
	#endif

	#ifdef ENABLE_TOUCH_SINGLE_WORKQUEUE
//	disable_irq(ts->client->irq);
	disable_irq_nosync(ts->client->irq);	
	queue_work(ts->work_queue_eventd, &ts->work);
	#else
	/* disable further interrupts until this interrupt is processed */
	disable_irq_nosync(ts->client->irq);

	/* schedule motion signal handling */
	schedule_work(&ts->work);
	#endif
	return IRQ_HANDLED;
}

static int g_use_old_8000_ts = 0;

static int __init ts8000_setting(char *ts8000)
{
	if(ts8000) {
		if(strcmp(ts8000,"on")==0) {
			g_use_old_8000_ts = 1;
		}  else {
			g_use_old_8000_ts = 0;
		}
	}
	return 1; /*JHLIM remove warning */
}
__setup("old8000ts=", ts8000_setting);

static int melfas_ts_initialize(struct i2c_client *client, struct melfas_ts *ts)
{
	int 	key, code;
	struct input_dev *input_device;
	int error = 0;
//	u8 id;
	int retval = 0;

	/* Create the input device and register it. */
	input_device = input_allocate_device();
	if (!input_device) {
		error = -ENOMEM;
		melfas_ts_debug("err input allocate device\n");
		goto error_free_device;
	}

	if (!client) {
		error = ~ENODEV;
		melfas_ts_debug("err client is Null\n");
		goto error_free_device;
	}

	if (!ts) {
		error = ~ENODEV;
		melfas_ts_debug("err context is Null\n");
		goto error_free_device;
	}

	ts->input = input_device;
	input_device->name = MELFAS_TTSP_NAME;
	input_device->dev.parent = &client->dev;

	/* init the touch structures */


	set_bit(EV_SYN, input_device->evbit);
	set_bit(EV_KEY, input_device->evbit);
	set_bit(EV_ABS, input_device->evbit);
	set_bit(BTN_TOUCH, input_device->keybit);
	set_bit(BTN_2, input_device->keybit);

	#ifdef CONFIG_MX100_WS
	for(key = 0; key < sizeof(MX100_TouchKeycodeAllow) / 4; key++){
		code = MX100_TouchKeycodeAllow[key];
		if(code<=0)
			continue;
		set_bit(code & KEY_MAX, input_device->keybit);
	}

	input_device->keycode = MX100_TouchKeycodeAllow;
	#endif
	/* For single touch */
	input_set_abs_params(ts->input, ABS_X, 0, TS_MAX_X_COORD-1, 0, 0);
	input_set_abs_params(ts->input, ABS_Y, 0, TS_MAX_Y_COORD-1, 0, 0);


	/* For multi touch */
	input_set_abs_params(ts->input, ABS_MT_TOUCH_MAJOR,  0, 255, 2, 0);
	input_set_abs_params(ts->input, ABS_MT_POSITION_X,     0, TS_MAX_X_COORD-1, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_POSITION_Y,     0, TS_MAX_Y_COORD-1, 0, 0);

	#if 0
	input_set_abs_params(ts->input, ABS_MT_POSITION_X, 0, TS_MAX_X_COORD-1, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_POSITION_Y, 0, TS_MAX_Y_COORD-1, 0, 0);

	input_set_abs_params(ts->input, ABS_MT_TOUCH_MAJOR, 0, 255, 2, 0);
	input_set_abs_params(ts->input, ABS_MT_WIDTH_MAJOR, 0,  15, 2, 0);
	#endif	
//	input_set_abs_params(ts->input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
//	input_set_abs_params(ts->input, ABS_PRESSURE, 0, 255, 0, 0);
	
	melfas_ts_info("%s: Register input device\n", MELFAS_7000_I2C_NAME);

	error = input_register_device(input_device);
	if (error) {
		melfas_ts_alert("%s: Failed to register input device\n", MELFAS_7000_I2C_NAME);
		retval = error;
		goto error_free_device;
	}

	#ifdef ENABLE_TOUCH_SINGLE_WORKQUEUE
	ts->work_queue_eventd = create_singlethread_workqueue("touch_eventd");
	if (!ts->work_queue_eventd) {
		melfas_ts_info(" create_singlethread_workqueue error!!!");

		return -ENOMEM;
	}
	#endif

	/* Prepare our worker structure prior to setting up the timer/ISR */
	INIT_WORK(&ts->work,melfas_ts_xy_worker);

	/* Power on the chip and make sure that I/Os are set as specified
	 * in the platform 
	 */
	 
	
	/* Timer or Interrupt setup */
	if(ts->client->irq == 0) {
		melfas_ts_info("Setting up timer\n");
		setup_timer(&ts->timer, melfas_ts_timer, (unsigned long) ts);
		mod_timer(&ts->timer, jiffies + TOUCHSCREEN_TIMEOUT);
	}
	else {
		melfas_ts_info("Setting up interrupt\n");
		/* request_irq() will also call enable_irq() */

		if((int)melfas_ts_7000_id==(int)g_which_i2c_device_id) {
			error = request_irq (client->irq,melfas_ts_irq,IRQF_TRIGGER_FALLING,
				client->dev.driver->name,ts);
		} else {
			// 2011.02.27 fix melfas ts TP issue
			if(g_use_old_8000_ts==1) {
				printk(KERN_ERR "selectd OLD 8000 ts\n");
				error = request_irq (client->irq,melfas_ts_irq,IRQF_TRIGGER_LOW,
					client->dev.driver->name,ts);
			} else {
				printk(KERN_ERR "selectd New 8000 ts\n");

				error = request_irq (client->irq,melfas_ts_irq, IRQF_TRIGGER_FALLING,
				client->dev.driver->name,ts); 
			}
		}
		if (error) {
			melfas_ts_alert("error: could not request irq\n");
			retval = error;
			goto error_free_irq;
		}
	}

	atomic_set(&ts->irq_enabled, 1);
	retval = device_create_file(&ts->client->dev, &dev_attr_irq_enable);

	if (retval < 0) {
		melfas_ts_alert("File device creation failed: %d\n", retval);
		retval = -ENODEV;
		goto error_free_irq;
	}

	init_release_touch_key(ts);

	#ifdef ENABLE_TOUCH_NG_DEBUG
	get_suspend_value();
	#endif//	melfas_ts_info("%s: Successful registration\n", MELFAS_7000_I2C_NAME);

	goto success;

error_free_irq:
	melfas_ts_alert("Error: Failed to register IRQ handler\n");
	free_irq(client->irq,ts);

error_free_device:
	if (input_device) {
		input_free_device(input_device);
	}

success:
	return retval;
}

/* I2C driver probe function */
static int __devinit melfas_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct melfas_ts *ts;
	int error;
	int retval = 0;
	int ret;
	uint8_t buf = 0x40;

	/* allocate and clear memory */
	ts = kzalloc (sizeof(struct melfas_ts),GFP_KERNEL);
	if (ts == NULL) {
		melfas_ts_debug("err kzalloc for melfas_ts\n");
		retval = -ENOMEM;
	}

	if (!(retval < 0)) {
		/* register driver_data */
		ts->client = client;

		
//		ts->platform_data = client->dev.platform_data;
		i2c_set_clientdata(client,ts);

		g_melfas_i2c_client =client;

		ret = i2c_master_send(ts->client, &buf, 1);
		
		if (ret < 0)  {
	//		printk(KERN_ERR "melfas_ts_probe: i2c_master_send() failed\n");
			retval = -EIO;
			goto err_detect_failed;
		}

		/* detect melfas ts */
		g_which_i2c_device_id =(struct i2c_device_id *) id;

		set_xy_cord(600,1024);

		error = melfas_ts_initialize(client, ts);
		if (error) {
			melfas_ts_debug("err melfas_ts_initialize\n");
			if (ts != NULL) {
				/* deallocate memory */
				kfree(ts);	
			}
/*
			i2c_del_driver(&melfas_ts_7000_driver);
*/
			retval = -ENODEV;
		}
		else {

		}
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	if (!(retval < 0)) {
		ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
		ts->early_suspend.suspend = melfas_ts_early_suspend;
		ts->early_suspend.resume = melfas_ts_late_resume;
		register_early_suspend(&ts->early_suspend);
	}
#endif /* CONFIG_HAS_EARLYSUSPEND */

//	melfas_ts_info("Start Probe %s\n", (retval < 0) ? "FAIL" : "PASS");

	return retval;
err_detect_failed:
	printk(KERN_ERR "melfas-ts: err_detect failed\n");
	kfree(ts);
	return retval;
}

#ifdef ENABLE_TOUCH_NG_DEBUG
#include <linux/io.h>

unsigned int get_reg_value(unsigned int addr)
{
    unsigned int virtual_addr,virtual_addr2;
    unsigned int physical_addr;
    unsigned int reg_value ;
	physical_addr =  addr >> 12;
	physical_addr =  physical_addr <<12;

	virtual_addr = (unsigned long)ioremap(physical_addr,0x1000);  // 0x1000  ·Î ¶³¾îÁö°Ô

	if(virtual_addr) {
		virtual_addr2 = virtual_addr + (addr % 0x1000);
		reg_value =  readl(virtual_addr2);	
		iounmap((void __iomem *)virtual_addr);
	}

	return reg_value;
}

//GPIO_TOUCH_INT_IN  GPH1(0)
//#define GPIO_TOUCH_ENABLE_OUT			S5PV210_GPJ2(5)

static unsigned int g_Suspend_TOUCH_INT_CON;
static unsigned int g_Suspend_TOUCH_INT_PUD;

static unsigned int g_Suspend_TOUCH_EN_CON;
static unsigned int g_Suspend_TOUCH_EN_PUD;

static unsigned int g_Resume_TOUCH_INT_CON;
static unsigned int g_Resume_TOUCH_INT_PUD;

static unsigned int g_Resume_TOUCH_EN_CON;
static unsigned int g_Resume_TOUCH_EN_PUD;

void get_suspend_value(void)
{
g_Suspend_TOUCH_INT_CON = get_reg_value(0xE0200C20) & 0xf;
g_Suspend_TOUCH_INT_PUD = get_reg_value(0xE0200C28) & 0x3;
g_Suspend_TOUCH_EN_CON = get_reg_value(0xE0200280) & 0xF00000;;
g_Suspend_TOUCH_EN_PUD = get_reg_value(0xE0200288) & 0xC00;
printk("TOUCH suspend %x %x %x %x\n",
g_Suspend_TOUCH_INT_CON, 
g_Suspend_TOUCH_INT_PUD ,
g_Suspend_TOUCH_EN_CON ,	
g_Suspend_TOUCH_EN_PUD );


}
void get_resume_value(void)
{
g_Resume_TOUCH_INT_CON = get_reg_value(0xE0200C20) & 0xf;
g_Resume_TOUCH_INT_PUD = get_reg_value(0xE0200C28) & 0x3;
g_Resume_TOUCH_EN_CON = get_reg_value(0xE0200280) & 0xF00000;;
g_Resume_TOUCH_EN_PUD = get_reg_value(0xE0200288) & 0xC00;

printk("TOUCH resume %x %x %x %x\n",
g_Resume_TOUCH_INT_CON, 
g_Resume_TOUCH_INT_PUD ,
g_Resume_TOUCH_EN_CON ,	
g_Resume_TOUCH_EN_PUD );

}

void check_suspend_resume_value(void)
{
	int checkvalue = 0;

	if(g_Suspend_TOUCH_INT_CON!= g_Resume_TOUCH_INT_CON) {
		checkvalue |= 1;
	}
	if(g_Suspend_TOUCH_INT_PUD != g_Resume_TOUCH_INT_PUD ) {
		checkvalue |= 2;
	}

	if(g_Suspend_TOUCH_EN_CON != g_Resume_TOUCH_EN_CON ) {
		checkvalue += 4;
	}

	if(g_Suspend_TOUCH_EN_PUD !=g_Resume_TOUCH_EN_PUD ) {
		checkvalue += 8;		
	}

	if(checkvalue > 0) {
		int i;
		printk("touch suspend resume error!!!!: %X",checkvalue);

		for(i=0;i<20;i++) {
			pm_debug_led_ctl(checkvalue);
			msleep(300);
			pm_debug_led_ctl(0);
			msleep(300);			
		}
	}
}
#endif

void melfas_ts_enable(struct melfas_ts *ts,int enable)
{
int ret;
uint8_t buf = 0x40;
int retry_count=0;
#if 0
	if(enable==1) {
	    	gpio_set_value(GPIO_TOUCH_ENABLE_OUT,0);
		msleep(30); // ktj_sleep
	    	gpio_set_value(GPIO_TOUCH_ENABLE_OUT,1);
		printk("checkou touch enable pin status =%d\n",gpio_get_value(GPIO_TOUCH_ENABLE_OUT));
	} else {
		printk("melfas_ts ce disabled\n");
    		gpio_set_value(GPIO_TOUCH_ENABLE_OUT,0);
	}
#else
	if(enable==1) {
	retry:
//	000 = Low level 001 = High level 
//	010 = Falling edge triggered 
//	011 = Rising edge triggered 
//	100 = Both edge triggered

		// 2011.06.10 : add port re-setting.

		if(g_use_old_8000_ts==1) {
			set_irq_type(GPIO_TOUCH_INT_IN, IRQ_TYPE_LEVEL_LOW); 
		} else {
			set_irq_type(GPIO_TOUCH_INT_IN, IRQ_TYPE_EDGE_FALLING); 
		}
		
		s3c_gpio_setpull(GPIO_TOUCH_ENABLE_OUT, S3C_GPIO_PULL_DOWN);	
	 	s3c_gpio_setpull(GPIO_TOUCH_INT_IN, S3C_GPIO_PULL_NONE);
		
		gpio_direction_output(GPIO_TOUCH_ENABLE_OUT,0);
		msleep(50); 
	    	gpio_direction_output(GPIO_TOUCH_ENABLE_OUT,1);
		msleep(50); 

		printk("checkou touch enable pin status =%d\n",gpio_get_value(GPIO_TOUCH_ENABLE_OUT));

		ret = i2c_master_send(ts->client, &buf, 1);
		
		if (ret < 0 && retry_count++ < 3)  {
			goto retry;
		} 
		
		if(retry_count == 3) {
			printk("melfas_ts enable failed\n");
		}
	
	} else {
		printk("melfas_ts ce disabled\n");
    		gpio_direction_output(GPIO_TOUCH_ENABLE_OUT,0);
	}

#endif
}

/* Function to manage power-on resume */
static int melfas_ts_resume(struct i2c_client *client)
{
	struct melfas_ts *ts = i2c_get_clientdata(client);
//	u8 wake_mode = 0;
	int retval = 0;

	
	melfas_ts_debug("Wake Up\n");

#ifdef ENABLE_TOUCH_PM_POWERONOFF
	melfas_ts_enable(ts,true);
#else
    msleep(100); /* ktj_pm, to avoid abnormal ts after wakeup */
	ret = i2c_smbus_write_byte_data(client, 0x01, 0b001); /* resume & reset */
	
	if (ret < 0) {
		printk(KERN_ERR "melfas_ts_resume: i2c_smbus_write_byte_data failed\n");
	}
#endif
	
	enable_irq(ts->client->irq);

	#ifdef ENABLE_TOUCH_NG_DEBUG
	get_resume_value();
        check_suspend_resume_value();	
	#endif

printk("__%s done \n", __func__);

	return retval;
}


/* Function to manage low power suspend */
static int melfas_ts_suspend(struct i2c_client *client, pm_message_t message)
{
	int ret;
	struct melfas_ts *ts = i2c_get_clientdata(client);

	melfas_ts_debug("Suspend\n");

	disable_irq(client->irq);
//	disable_irq_nosync(client->irq);	

	#ifdef ENABLE_TOUCH_NG_DEBUG
//	get_suspend_value();
	#endif

	#ifdef ENABLE_TOUCH_SINGLE_WORKQUEUE
	flush_workqueue(ts->work_queue_eventd);	
	#endif
	
	ret = cancel_work_sync(&ts->work);
#if 0 // ktj disable for sleep issue
	if (ret) /* if work was pending disable-count is now 2 */ 
		enable_irq(client->irq);
#endif 

	#ifdef ENABLE_TOUCH_PM_POWERONOFF
		melfas_ts_enable(ts,false);
	#else
	ret = i2c_smbus_write_byte_data(client, 0x01, 0x00); /* deep sleep */
	if (ret < 0)
		printk(KERN_ERR "melfas_ts_suspend: i2c_smbus_write_byte_data failed\n");
	#endif


	
	return 0;
}

/* registered in driver struct */
static int __devexit melfas_ts_remove(struct i2c_client *client)
{
	struct melfas_ts *ts;
	int err;

	melfas_ts_alert("Unregister\n");

	/* clientdata registered on probe */
	ts = i2c_get_clientdata(client);
	device_remove_file(&ts->client->dev, &dev_attr_irq_enable);

	/* Start cleaning up by removing any delayed work and the timer */
	if (cancel_delayed_work((struct delayed_work *)&ts->work)<0) {
		melfas_ts_alert("error: could not remove work from workqueue\n");
	}

	/* free up timer or irq */
    if(ts->client->irq == 0) {	
		err = del_timer(&ts->timer);
		if (err < 0) {
			melfas_ts_alert("error: failed to delete timer\n");
		}
	}
	else {
		free_irq(client->irq,ts);
	}

	/* housekeeping */
	if (ts != NULL) {
		kfree(ts);
	}

//	melfas_ts_alert("Leaving\n");

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void melfas_ts_early_suspend(struct early_suspend *handler)
{
	struct melfas_ts *ts;
	printk("[KENEL] %s, %d \n", __FUNCTION__, __LINE__);
	#ifdef ENABLE_TOUCH_KEY_LED
	melfasts_key_led_onoff(0);
	#endif

	ts = container_of(handler, struct melfas_ts, early_suspend);
	melfas_ts_suspend(ts->client, PMSG_SUSPEND);
}

static void melfas_ts_late_resume(struct early_suspend *handler)
{
	struct melfas_ts *ts;

	#ifdef ENABLE_TOUCH_KEY_LED
	melfasts_key_led_refresh();
	#endif

	ts = container_of(handler, struct melfas_ts, early_suspend);

	melfas_ts_debug("late resume \n");
	
	melfas_ts_resume(ts->client);
}
#endif  /* CONFIG_HAS_EARLYSUSPEND */

extern char  g_mx100_hw_name[8];

int __init melfas_ts_init(void)
{

	int ret;


	#if defined(ENABLE_TOUCH_KEY_LED) || defined(ENABLE_PM_LED_DEBUG)

	#ifdef CONFIG_MX100_REV_ES
	gpio_request(GPIO_TOUCH_KEY_LED0, "GPIO_TOUCH_KEY_LED0");
        gpio_direction_output(GPIO_TOUCH_KEY_LED0,0);
	s3c_gpio_setpull(GPIO_TOUCH_KEY_LED0, S3C_GPIO_PULL_DOWN);	

	gpio_request(GPIO_TOUCH_KEY_LED1, "GPIO_TOUCH_KEY_LED1");
        gpio_direction_output(GPIO_TOUCH_KEY_LED1,0);
	s3c_gpio_setpull(GPIO_TOUCH_KEY_LED1, S3C_GPIO_PULL_DOWN);	

	gpio_request(GPIO_TOUCH_KEY_LED2, "GPIO_TOUCH_KEY_LED2");
        gpio_direction_output(GPIO_TOUCH_KEY_LED2,0);
	s3c_gpio_setpull(GPIO_TOUCH_KEY_LED2, S3C_GPIO_PULL_DOWN);	

	gpio_request(GPIO_FRONT_LED_CTL_OUT, "GPIO_TOUCH_KEY_LED3");
        gpio_direction_output(GPIO_FRONT_LED_CTL_OUT,0);
	s3c_gpio_setpull(GPIO_FRONT_LED_CTL_OUT, S3C_GPIO_PULL_DOWN);	

	#else
	gpio_request(GPIO_FRONT_LED_CTL_OUT, "GPIO_FRONT_LED_CTL_OUT");
        gpio_direction_output(GPIO_FRONT_LED_CTL_OUT,0);
	s3c_gpio_setpull(GPIO_FRONT_LED_CTL_OUT, S3C_GPIO_PULL_DOWN);	
	#endif

	
	#endif

	#ifdef ENABLE_TOUCH_KEY_LED
	melfasts_key_led_timer_run();
	#endif

	#ifdef CONFIG_MX100_WS	
	gpio_request(GPIO_TOUCH_ENABLE_OUT, "GPIO_TOUCH_ENABLE_OUT");
	s3c_gpio_setpull(GPIO_TOUCH_ENABLE_OUT, S3C_GPIO_PULL_DOWN);	
	msleep(50);
        gpio_direction_output(GPIO_TOUCH_ENABLE_OUT,0);
	msleep(50);
        gpio_direction_output(GPIO_TOUCH_ENABLE_OUT,1);

	// 2011.04.17 
   	 s3c_gpio_setpull(GPIO_TOUCH_INT_IN, S3C_GPIO_PULL_NONE);
		
	msleep(30);
	#endif

//	mx100_melfasts_key_led(1);
	if(strcmp(g_mx100_hw_name,"LPP")==0) {   /* 2011.05.11  LPP use only 8000 tsp */
		ret = i2c_add_driver(&melfas_ts_8000_driver);
	} else {
		ret = i2c_add_driver(&melfas_ts_7000_driver);
		if(g_which_i2c_device_id == 0) {
			i2c_del_driver(&melfas_ts_7000_driver);
			ret = i2c_add_driver(&melfas_ts_8000_driver);
		}
	}
	if(g_which_i2c_device_id==0) {
		melfas_ts_info("\nMELFAS TouchScreen Driver probe faild !!!!\n");
	} else {
		melfas_ts_info("\nMELFAS TouchScreen(%s) Driver (Built %s @ %s)\n",g_which_i2c_device_id->name,__DATE__,__TIME__);
	}
	#ifdef ENABLE_PM_LED_DEBUG
	init_pm_debug_led_ctl();
	#endif
//	mx100_melfasts_log_start("touch.txt");

	return ret;
}

void __exit melfas_ts_exit(void)
{
	struct melfas_ts *ts = i2c_get_clientdata(g_melfas_i2c_client);

	#ifdef ENABLE_TOUCH_SINGLE_WORKQUEUE
	if (ts->work_queue_eventd)
		destroy_workqueue(ts->work_queue_eventd);
	#endif
	
	return i2c_del_driver(&melfas_ts_7000_driver);
}


module_init(melfas_ts_init);
module_exit(melfas_ts_exit);

MODULE_AUTHOR("jaehwan <jaehwan.lim@iriver.com>");
MODULE_DESCRIPTION("Iriver Melfas touch driver");
