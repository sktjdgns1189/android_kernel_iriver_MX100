/*
 * BMA150 accelerometer driver
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
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

/* ktj */
//----------------------------------------------------------------------------------
//#define rdebug(fmt,arg...) printk(KERN_CRIT "_[bma150] " fmt "\n",## arg)
#define rdebug(fmt,arg...)

//#define DISP_ACC_VALUE
/* Shanghai ewada */
#define NEED_CALIBRATION

#define USE_IOCTL

#ifdef USE_IOCTL
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#endif
//----------------------------------------------------------------------------------

#define BMA150_VERSION "1.0.0"

#define BMA150_NAME "bma150"

/* for debugging */
#define DEBUG 0
#define TRACE_FUNC() pr_debug(BMA150_NAME ": <trace> %s()\n", __FUNCTION__)

/*
 * Default parameters
 */
#define BMA150_DEFAULT_DELAY            100
#define BMA150_MAX_DELAY                2000

/*
 * Registers
 */
#define BMA150_CHIP_ID_REG              0x00
#define BMA150_CHIP_ID                  0x02

#define BMA150_AL_VERSION_REG           0x01
#define BMA150_AL_VERSION_MASK          0x0f
#define BMA150_AL_VERSION_SHIFT         0

#define BMA150_ML_VERSION_REG           0x01
#define BMA150_ML_VERSION_MASK          0xf0
#define BMA150_ML_VERSION_SHIFT         4

#define BMA150_ACC_REG                  0x02

#define BMA150_SOFT_RESET_REG           0x0a
#define BMA150_SOFT_RESET_MASK          0x02
#define BMA150_SOFT_RESET_SHIFT         1

#define BMA150_SLEEP_REG                0x0a
#define BMA150_SLEEP_MASK               0x01
#define BMA150_SLEEP_SHIFT              0

#define BMA150_RANGE_REG                0x14
#define BMA150_RANGE_MASK               0x18
#define BMA150_RANGE_SHIFT              3
#define BMA150_RANGE_2G                 0
#define BMA150_RANGE_4G                 1
#define BMA150_RANGE_8G                 2

#define BMA150_BANDWIDTH_REG            0x14
#define BMA150_BANDWIDTH_MASK           0x07
#define BMA150_BANDWIDTH_SHIFT          0
#define BMA150_BANDWIDTH_25HZ           0
#define BMA150_BANDWIDTH_50HZ           1
#define BMA150_BANDWIDTH_100HZ          2
#define BMA150_BANDWIDTH_190HZ          3
#define BMA150_BANDWIDTH_375HZ          4
#define BMA150_BANDWIDTH_750HZ          5
#define BMA150_BANDWIDTH_1500HZ         6

#define BMA150_SHADOW_DIS_REG           0x15
#define BMA150_SHADOW_DIS_MASK          0x08
#define BMA150_SHADOW_DIS_SHIFT         3

#define BMA150_WAKE_UP_PAUSE_REG        0x15
#define BMA150_WAKE_UP_PAUSE_MASK       0x06
#define BMA150_WAKE_UP_PAUSE_SHIFT      1

#define BMA150_WAKE_UP_REG              0x15
#define BMA150_WAKE_UP_MASK             0x01
#define BMA150_WAKE_UP_SHIFT            0

/*
 * Acceleration measurement
 */
#define BMA150_RESOLUTION               256

/* ABS axes parameter range [um/s^2] (for input event) */
#define GRAVITY_EARTH                   9806550
#define ABSMIN_2G                       (-GRAVITY_EARTH * 2)
#define ABSMAX_2G                       (GRAVITY_EARTH * 2)

struct acceleration {
	int x;
	int y;
	int z;
};

/*
 * Output data rate
 */
struct bma150_odr {
        unsigned long delay;            /* min delay (msec) in the range of ODR */
        u8 odr;                         /* bandwidth register value */
};

static const struct bma150_odr bma150_odr_table[] = {
	{1,  BMA150_BANDWIDTH_1500HZ},
	{2,  BMA150_BANDWIDTH_750HZ},
	{3,  BMA150_BANDWIDTH_375HZ},
	{6,  BMA150_BANDWIDTH_190HZ},
	{10, BMA150_BANDWIDTH_100HZ},
	{20, BMA150_BANDWIDTH_50HZ},
	{40, BMA150_BANDWIDTH_25HZ},
};

/*
 * Transformation matrix for chip mounting position
 */
static const int bma150_position_map[][3][3] = {
	{{ 0, -1,  0}, { 1,  0,  0}, { 0,  0,  1}}, /* top/upper-left */
	{{ 1,  0,  0}, { 0,  1,  0}, { 0,  0,  1}}, /* top/upper-right */
	{{ 0,  1,  0}, {-1,  0,  0}, { 0,  0,  1}}, /* top/lower-right */
	{{-1,  0,  0}, { 0, -1,  0}, { 0,  0,  1}}, /* top/lower-left */
	{{ 0,  1,  0}, { 1,  0,  0}, { 0,  0, -1}}, /* bottom/upper-right */
	{{-1,  0,  0}, { 0,  1,  0}, { 0,  0, -1}}, /* bottom/upper-left */
	{{ 0, -1,  0}, {-1,  0,  0}, { 0,  0, -1}}, /* bottom/lower-left */
	{{ 1,  0,  0}, { 0, -1,  0}, { 0,  0, -1}}, /* bottom/lower-right */
};

/*
 * driver private data
 */
struct bma150_data {
	atomic_t enable;                /* attribute value */
	atomic_t delay;                 /* attribute value */
	atomic_t position;              /* attribute value */
	struct acceleration last;       /* last measured data */
	struct mutex enable_mutex;
	struct mutex data_mutex;
	struct i2c_client *client;
	struct input_dev *input;
	struct delayed_work work;
#if DEBUG
	int suspend;
#endif
};

struct bma150_data *this_bma150;    /* ktj */

#define delay_to_jiffies(d) ((d)?msecs_to_jiffies(d):1)
#define actual_delay(d)     (jiffies_to_msecs(delay_to_jiffies(d)))

/* register access functions */
#define bma150_read_bits(p,r) \
	((i2c_smbus_read_byte_data((p)->client, r##_REG) & r##_MASK) >> r##_SHIFT)

#define bma150_update_bits(p,r,v) \
        i2c_smbus_write_byte_data((p)->client, r##_REG, \
                ((i2c_smbus_read_byte_data((p)->client,r##_REG) & ~r##_MASK) | ((v) << r##_SHIFT)))

/*
 * Device dependant operations
 */
static int bma150_power_up(struct bma150_data *bma150)
{
    rdebug("%s called", __func__);

	bma150_update_bits(bma150, BMA150_SLEEP, 0);
	/* wait 1ms for wake-up time from sleep to operational mode */
	msleep(1);

	return 0;
}

static int bma150_power_down(struct bma150_data *bma150)
{
    rdebug("%s called", __func__);

	bma150_update_bits(bma150, BMA150_SLEEP, 1);

	return 0;
}

static int bma150_hw_init(struct bma150_data *bma150)
{
	/* reset hardware */
	bma150_power_up(bma150);
	i2c_smbus_write_byte_data(bma150->client,
				  BMA150_SOFT_RESET_REG, BMA150_SOFT_RESET_MASK);

	/* wait 10us after soft_reset to start I2C transaction */
	msleep(1);

	bma150_update_bits(bma150, BMA150_RANGE, BMA150_RANGE_2G);

	bma150_power_down(bma150);

	return 0;
}

static int bma150_get_enable(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma150_data *bma150 = i2c_get_clientdata(client);

	return atomic_read(&bma150->enable);
}

static void bma150_set_enable(struct device *dev, int enable)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma150_data *bma150 = i2c_get_clientdata(client);
	int delay = atomic_read(&bma150->delay);

	mutex_lock(&bma150->enable_mutex);

	if (enable) {                   /* enable if state will be changed */
		if (!atomic_cmpxchg(&bma150->enable, 0, 1)) {
			bma150_power_up(bma150);
			schedule_delayed_work(&bma150->work,
					      delay_to_jiffies(delay) + 1);
		}
	} else {                        /* disable if state will be changed */
		if (atomic_cmpxchg(&bma150->enable, 1, 0)) {
			cancel_delayed_work_sync(&bma150->work);
			bma150_power_down(bma150);
		}
	}
	atomic_set(&bma150->enable, enable);

	mutex_unlock(&bma150->enable_mutex);
}

static int bma150_get_delay(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma150_data *bma150 = i2c_get_clientdata(client);

	return atomic_read(&bma150->delay);
}

static void bma150_set_delay(struct device *dev, int delay)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma150_data *bma150 = i2c_get_clientdata(client);
	u8 odr;
	int i;

	/* determine optimum ODR */
	for (i = 1; (i < ARRAY_SIZE(bma150_odr_table)) &&
		     (actual_delay(delay) >= bma150_odr_table[i].delay); i++)
		;
	odr = bma150_odr_table[i-1].odr;
	atomic_set(&bma150->delay, delay);

	mutex_lock(&bma150->enable_mutex);

	if (bma150_get_enable(dev)) {
		cancel_delayed_work_sync(&bma150->work);
		bma150_update_bits(bma150, BMA150_BANDWIDTH, odr);
		schedule_delayed_work(&bma150->work,
				      delay_to_jiffies(delay) + 1);
	} else {
		bma150_power_up(bma150);
		bma150_update_bits(bma150, BMA150_BANDWIDTH, odr);
	#ifndef USE_IOCTL /* ktj, fix issue after wakeup */
		bma150_power_down(bma150);
	#endif
	}

	mutex_unlock(&bma150->enable_mutex);

    rdebug("%s bma150->delay=%d delay=%d", __func__, bma150->delay, delay);
}

static int bma150_get_position(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma150_data *bma150 = i2c_get_clientdata(client);

	return atomic_read(&bma150->position);
}

static void bma150_set_position(struct device *dev, int position)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma150_data *bma150 = i2c_get_clientdata(client);

	atomic_set(&bma150->position, position);
}

static int bma150_measure(struct bma150_data *bma150, struct acceleration *accel)
{
	struct i2c_client *client = bma150->client;
	u8 buf[6];
	int raw[3], data[3];
	int pos = atomic_read(&bma150->position);
	long long g;
	int i, j;

	/* read acceleration data */
	if (i2c_smbus_read_i2c_block_data(client, BMA150_ACC_REG, 6, buf) != 6) {
		dev_err(&client->dev,
			"I2C block read error: addr=0x%02x, len=%d\n",
			BMA150_ACC_REG, 6);
		for (i = 0; i < 3; i++) {
			raw[i] = 0;
		}
	} else {
		for (i = 0; i < 3; i++) {
			raw[i] = (*(s16 *)&buf[i*2]) >> 6;
		}
	}

	/* for X, Y, Z axis */
	for (i = 0; i < 3; i++) {
		/* coordinate transformation */
		data[i] = 0;
		for (j = 0; j < 3; j++) {
			data[i] += raw[j] * bma150_position_map[pos][i][j];
		}
		/* normalization */
		g = (long long)data[i] * GRAVITY_EARTH / BMA150_RESOLUTION;
		data[i] = g;
	}

	dev_dbg(&client->dev, "raw(%5d,%5d,%5d) => norm(%8d,%8d,%8d)\n",
		raw[0], raw[1], raw[2], data[0], data[1], data[2]);

#ifdef DISP_ACC_VALUE
    rdebug("_[bma150] raw(%5d,%5d,%5d) => norm(%8d,%8d,%8d)", raw[0], raw[1], raw[2], data[0], data[1], data[2]);
#endif

    /* ktj fix direction, 2011-1-26 */
	accel->x = raw[0];
	accel->y = raw[1] * -1;
	accel->z = raw[2] * -1;

	return 0;
}

static void bma150_work_func(struct work_struct *work)
{
	struct bma150_data *bma150 = container_of((struct delayed_work *)work,
						  struct bma150_data, work);
	struct acceleration accel;
	unsigned long delay = delay_to_jiffies(atomic_read(&bma150->delay));

//  rdebug("%s called", __func__);

	bma150_measure(bma150, &accel);

	input_report_abs(bma150->input, ABS_X, accel.x);
	input_report_abs(bma150->input, ABS_Y, accel.y);
	input_report_abs(bma150->input, ABS_Z, accel.z);
	input_sync(bma150->input);

	mutex_lock(&bma150->data_mutex);
	bma150->last = accel;
	mutex_unlock(&bma150->data_mutex);

	schedule_delayed_work(&bma150->work, delay);
}

/*
 * Input device interface
 */
static int bma150_input_init(struct bma150_data *bma150)
{
	struct input_dev *dev;
	int err;

	dev = input_allocate_device();
	if (!dev) {
		return -ENOMEM;
	}
	dev->name = "accelerometer";
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_ABS, ABS_MISC);
	input_set_abs_params(dev, ABS_X, ABSMIN_2G, ABSMAX_2G, 0, 0);
	input_set_abs_params(dev, ABS_Y, ABSMIN_2G, ABSMAX_2G, 0, 0);
	input_set_abs_params(dev, ABS_Z, ABSMIN_2G, ABSMAX_2G, 0, 0);
	input_set_drvdata(dev, bma150);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	bma150->input = dev;

	return 0;
}

static void bma150_input_fini(struct bma150_data *bma150)
{
	struct input_dev *dev = bma150->input;

	input_unregister_device(dev);
	input_free_device(dev);
}

/*
 * sysfs device attributes
 */
static ssize_t bma150_enable_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bma150_get_enable(dev));
}

static ssize_t bma150_enable_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	unsigned long enable = simple_strtoul(buf, NULL, 10);

	if ((enable == 0) || (enable == 1)) {
		bma150_set_enable(dev, enable);
	}

	return count;
}

static ssize_t bma150_delay_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bma150_get_delay(dev));
}

static ssize_t bma150_delay_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned long delay = simple_strtoul(buf, NULL, 10);

	if (delay > BMA150_MAX_DELAY) {
		delay = BMA150_MAX_DELAY;
	}

	bma150_set_delay(dev, delay);

	return count;
}

static ssize_t bma150_position_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bma150_get_position(dev));
}

static ssize_t bma150_position_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	unsigned long position;

	position = simple_strtoul(buf, NULL,10);
	if ((position >= 0) && (position <= 7)) {
		bma150_set_position(dev, position);
	}

	return count;
}

static ssize_t bma150_wake_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	static atomic_t serial = ATOMIC_INIT(0);

	input_report_abs(input, ABS_MISC, atomic_inc_return(&serial));

	return count;
}

static ssize_t bma150_data_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bma150_data *bma150 = input_get_drvdata(input);
	struct acceleration accel;

	mutex_lock(&bma150->data_mutex);
	accel = bma150->last;
	mutex_unlock(&bma150->data_mutex);

	return sprintf(buf, "%d %d %d\n", accel.x, accel.y, accel.z);
}

#if DEBUG
static ssize_t bma150_debug_reg_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bma150_data *bma150 = input_get_drvdata(input);
	struct i2c_client *client = bma150->client;
	ssize_t count = 0;
	u8 reg[0x16];
	int i;

	i2c_smbus_read_i2c_block_data(client, 0x00, 0x16, reg);
	for (i = 0; i < 0x16; i++) {
		count += sprintf(&buf[count], "%02x: %d\n", i, reg[i]);
	}

	return count;
}

static int bma150_suspend(struct i2c_client *client, pm_message_t mesg);
static int bma150_resume(struct i2c_client *client);

static ssize_t bma150_debug_suspend_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bma150_data *bma150 = input_get_drvdata(input);

	return sprintf(buf, "%d\n", bma150->suspend);
}

static ssize_t bma150_debug_suspend_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bma150_data *bma150 = input_get_drvdata(input);
	struct i2c_client *client = bma150->client;
	unsigned long suspend = simple_strtoul(buf, NULL, 10);

	if (suspend) {
		pm_message_t msg;
		bma150_suspend(client, msg);
	} else {
		bma150_resume(client);
	}

	return count;
}
#endif /* DEBUG */

static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
		   bma150_enable_show, bma150_enable_store);
static DEVICE_ATTR(delay, S_IRUGO|S_IWUSR|S_IWGRP,
		   bma150_delay_show, bma150_delay_store);
static DEVICE_ATTR(position, S_IRUGO|S_IWUSR,
		   bma150_position_show, bma150_position_store);
static DEVICE_ATTR(wake, S_IWUSR|S_IWGRP,
		   NULL, bma150_wake_store);
static DEVICE_ATTR(data, S_IRUGO,
		   bma150_data_show, NULL);

#if DEBUG
static DEVICE_ATTR(debug_reg, S_IRUGO,
		   bma150_debug_reg_show, NULL);
static DEVICE_ATTR(debug_suspend, S_IRUGO|S_IWUSR,
		   bma150_debug_suspend_show, bma150_debug_suspend_store);
#endif /* DEBUG */

static struct attribute *bma150_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_delay.attr,
	&dev_attr_position.attr,
	&dev_attr_wake.attr,
	&dev_attr_data.attr,
#if DEBUG
	&dev_attr_debug_reg.attr,
	&dev_attr_debug_suspend.attr,
#endif /* DEBUG */
	NULL
};

static struct attribute_group bma150_attribute_group = {
	.attrs = bma150_attributes
};

/*
 * I2C client
 */
static int bma150_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	int id;

	id = i2c_smbus_read_byte_data(client, BMA150_CHIP_ID_REG);
	if (id != BMA150_CHIP_ID)
		return -ENODEV;

	return 0;
}

#ifdef USE_IOCTL /* ktj */

#define BMA150_IOC_MAGIC 'B'

#define BMA150_INIT				    _IO(BMA150_IOC_MAGIC,0)
#define BMA150_DEINIT				_IO(BMA150_IOC_MAGIC,1)
#define BMA150_READ_ACCEL_XYZ		_IOWR(BMA150_IOC_MAGIC,10,short)
//Shanghai ewada:for calibration
#define BMA150_CALIBRATION          _IOWR(BMA150_IOC_MAGIC,48,short)

typedef struct {
    int x;
    int y;
    int z;
} smb380acc_t;

/* Shanghai ewada */
#ifdef NEED_CALIBRATION
static int smb380_calibrate(smb380acc_t orientation, int *tries);
#endif

static int bma150_open(struct inode *inode, struct file *file)
{
//  rdebug("%s called", __func__);
    return 0;
}

static int bma150_close(struct inode *inode, struct file *file)
{
//  rdebug("%s called", __func__);
    return 0;
}

static ssize_t bma150_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{	
    return 0;
}

static ssize_t bma150_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    return 0;
}

static int bma150_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	smb380acc_t accel;
    struct acceleration accels;
    unsigned char data[6];//Shanghai ewada
    int temp;//Shanghai ewada

//  rdebug("%s called cmd=%d", __func__, cmd);

	switch(cmd)
	{

	case BMA150_INIT:
        rdebug("IOCTL_BMA_ACC_INIT");
    	bma150_power_up(this_bma150);
        break;
        
	case BMA150_DEINIT:
        rdebug("IOCTL_BMA_ACC_DEINIT");
		bma150_power_down(this_bma150);
        break;

	case BMA150_READ_ACCEL_XYZ:
//	    bma150_measure(this_bma150, &accel);
	    bma150_measure(this_bma150, &accels);

        accel.x = accels.x; 
        accel.y = accels.y; 
        accel.z = accels.z; 

//      rdebug( "acc data x = %d  /  y =  %d  /  z = %d", accel.x, accel.y, accel.z );
        rdebug( "acc data x = %d  /  y =  %d  /  z = %d", accels.x, accels.y, accels.z );

		if( copy_to_user( (smb380acc_t*)arg, &accel, sizeof(smb380acc_t) ) )
//		if( copy_to_user( (&acceleration*)arg, &accel, sizeof(acceleration) ) )
		{
			err = -EFAULT;
		}   
        break;
#ifdef NEED_CALIBRATION /* Shanghai ewada */
    /* offset calibration routine */
    case BMA150_CALIBRATION:
        if(copy_from_user((smb380acc_t*)data,(smb380acc_t*)arg,6)!=0)
        {
            rdebug("copy_from_user error\n");
            return -EFAULT;
        }
        /* iteration time 10 */
        temp = 10;
        printk("Starting Gsensor calibration...\n");
        err = smb380_calibrate(*(smb380acc_t*)data, &temp);
        printk("Calibration done\n");
        break;
#endif
    default:
        break;        
    }

    return err;
}

static const struct file_operations bma150_fops = {
	.owner      = THIS_MODULE,
	.read       = bma150_read,
	.write      = bma150_write,
	.open       = bma150_open,
	.release    = bma150_close,
	.ioctl      = bma150_ioctl,
};

static struct miscdevice bma150_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "bma150_dev",
	.fops = &bma150_fops,
};
#endif // USE_IOCTL

#ifdef NEED_CALIBRATION

#define BMA150_EE_W_SHIFT		4
#define BMA150_EE_W_LEN			1
#define BMA150_EE_W_MASK		0x10
#define BMA150_EE_W_REG			0x0a

#define BMA150_OFFS_GAIN_X_REG		0x16
#define BMA150_OFFS_GAIN_Y_REG		0x17
#define BMA150_OFFS_GAIN_Z_REG		0x18
#define BMA150_OFFS_GAIN_T_REG		0x19
#define BMA150_OFFSET_X_REG		0x1a
#define BMA150_OFFSET_Y_REG		0x1b
#define BMA150_OFFSET_Z_REG		0x1c
#define BMA150_OFFSET_T_REG		0x1d

#define BMA150_EEP_OFFSET   0x20

#define BMA150_OFFSET_X_LSB_SHIFT	6
#define BMA150_OFFSET_X_LSB_LEN	2
#define BMA150_OFFSET_X_LSB_MASK	0xC0
#define BMA150_OFFSET_X_LSB_REG	BMA150_OFFS_GAIN_X_REG

#define BMA150_GAIN_X_SHIFT			0
#define BMA150_GAIN_X_LEN			6
#define BMA150_GAIN_X_MASK			0x3f
#define BMA150_GAIN_X_REG			BMA150_OFFS_GAIN_X_REG


#define BMA150_OFFSET_Y_LSB_SHIFT	6
#define BMA150_OFFSET_Y_LSB_LEN	2
#define BMA150_OFFSET_Y_LSB_MASK	0xC0
#define BMA150_OFFSET_Y_LSB_REG	BMA150_OFFS_GAIN_Y_REG

#define BMA150_GAIN_Y_SHIFT			0
#define BMA150_GAIN_Y_LEN			6
#define BMA150_GAIN_Y_MASK			0x3f
#define BMA150_GAIN_Y_REG			BMA150_OFFS_GAIN_Y_REG


#define BMA150_OFFSET_Z_LSB_SHIFT	6
#define BMA150_OFFSET_Z_LSB_LEN	2
#define BMA150_OFFSET_Z_LSB_MASK	0xC0
#define BMA150_OFFSET_Z_LSB_REG	BMA150_OFFS_GAIN_Z_REG

#define BMA150_GAIN_Z_SHIFT			0
#define BMA150_GAIN_Z_LEN			6
#define BMA150_GAIN_Z_MASK			0x3f
#define BMA150_GAIN_Z_REG			BMA150_OFFS_GAIN_Z_REG

#define BMA150_OFFSET_T_LSB_SHIFT	6
#define BMA150_OFFSET_T_LSB_LEN	    2
#define BMA150_OFFSET_T_LSB_MASK	0xC0
#define BMA150_OFFSET_T_LSB_REG	    BMA150_OFFS_GAIN_T_REG

#define BMA150_GAIN_T_SHIFT			0
#define BMA150_GAIN_T_LEN			6
#define BMA150_GAIN_T_MASK			0x3f
#define BMA150_GAIN_T_REG			BMA150_OFFS_GAIN_T_REG

#define BMA150_OFFSET_X_MSB_SHIFT	0
#define BMA150_OFFSET_X_MSB_LEN	8
#define BMA150_OFFSET_X_MSB_MASK	0xFF
#define BMA150_OFFSET_X_MSB_REG	BMA150_OFFSET_X_REG


#define BMA150_OFFSET_Y_MSB_SHIFT	0
#define BMA150_OFFSET_Y_MSB_LEN	8
#define BMA150_OFFSET_Y_MSB_MASK	0xFF
#define BMA150_OFFSET_Y_MSB_REG	BMA150_OFFSET_Y_REG

#define BMA150_OFFSET_Z_MSB_SHIFT	0
#define BMA150_OFFSET_Z_MSB_LEN	8
#define BMA150_OFFSET_Z_MSB_MASK	0xFF
#define BMA150_OFFSET_Z_MSB_REG	BMA150_OFFSET_Z_REG

#define BMA150_OFFSET_T_MSB_SHIFT	0
#define BMA150_OFFSET_T_MSB_LEN	8
#define BMA150_OFFSET_T_MSB_MASK	0xFF
#define BMA150_OFFSET_T_MSB_REG	BMA150_OFFSET_T_REG

#define SMB380_GET_BITSLICE(regvar, bitname)\
			(regvar & bitname##_MASK) >> bitname##_SHIFT

#define SMB380_SET_BITSLICE(regvar, bitname, val)\
		  (regvar & ~bitname##_MASK) | ((val<<bitname##_SHIFT)&bitname##_MASK)
/**	set smb380s range 
 \param range 
 \return  result of bus communication function
 
 \see SMB380_RANGE_2G		
 \see SMB380_RANGE_4G			
 \see SMB380_RANGE_8G			
*/
int smb380_set_range(char range) 
{			
   int comres = 0;
   unsigned char data;

   if (range < 3) {	
	 	comres = i2c_smbus_read_i2c_block_data(this_bma150->client, BMA150_RANGE_REG, 1, &data);
	 	data = SMB380_SET_BITSLICE(data, BMA150_RANGE, range);		  	
        comres += i2c_smbus_write_i2c_block_data(this_bma150->client, BMA150_RANGE_REG, 1, &data);
   }
   return comres;

}

/** set SMB380 internal filter bandwidth
   \param bw bandwidth (see bandwidth constants)
   \return result of bus communication function
   \see #define SMB380_BW_25HZ, SMB380_BW_50HZ, SMB380_BW_100HZ, SMB380_BW_190HZ, SMB380_BW_375HZ, SMB380_BW_750HZ, SMB380_BW_1500HZ
   \see smb380_get_bandwidth()
*/
int smb380_set_bandwidth(char bw) 
{
	int comres = 0;
	unsigned char data;

	if (bw<8) {
      comres = i2c_smbus_read_i2c_block_data(this_bma150->client, BMA150_BANDWIDTH_REG, 1, &data);
	  data = SMB380_SET_BITSLICE(data, BMA150_BANDWIDTH, bw);
	  comres += i2c_smbus_write_i2c_block_data(this_bma150->client, BMA150_BANDWIDTH_REG, 1, &data);
	}
    return comres;


}

/** write offset data to SMB380 image
   \param eew 0 = lock EEPROM 1 = unlock EEPROM 
   \return result of bus communication function
*/
int smb380_set_ee_w(unsigned char eew)
{
    unsigned char data;
    int comres;

    comres = i2c_smbus_read_i2c_block_data(this_bma150->client, BMA150_EE_W_REG, 1, &data);
	data = SMB380_SET_BITSLICE(data, BMA150_EE_W, eew);
	comres += i2c_smbus_write_i2c_block_data(this_bma150->client, BMA150_EE_W_REG, 1, &data);
	return comres;
}

/** read out offset data from 
   \param xyz select axis x=0, y=1, z=2
   \param *offset pointer to offset value (offset is in offset binary representation
   \return result of bus communication function
   \note use smb380_set_ee_w() function to enable access to offset registers 
*/
int smb380_get_offset(unsigned char xyz, unsigned short *offset) 
{
    int comres;
    unsigned char data;
    comres = i2c_smbus_read_i2c_block_data(this_bma150->client, BMA150_OFFSET_X_LSB_REG+xyz, 1, &data);
    *offset = data;
    data = SMB380_GET_BITSLICE(data, BMA150_OFFSET_X_LSB);
    comres += i2c_smbus_read_i2c_block_data(this_bma150->client, BMA150_OFFSET_X_MSB_REG+xyz, 1, &data);
    *offset |= (data<<2);

    return comres;
}

/** write offset data to SMB380 image
   \param xyz select axis x=0, y=1, z=2
   \param offset value to write (offset is in offset binary representation
   \return result of bus communication function
   \note use smb380_set_ee_w() function to enable access to offset registers 
*/
int smb380_set_offset(unsigned char xyz, unsigned short offset) 
{
   int comres;
   unsigned char data;

   comres = i2c_smbus_read_i2c_block_data(this_bma150->client, (BMA150_OFFSET_X_LSB_REG+xyz), 1, &data);
   data = SMB380_SET_BITSLICE(data, BMA150_OFFSET_X_LSB, offset);
   comres += i2c_smbus_write_i2c_block_data(this_bma150->client, (BMA150_OFFSET_X_LSB_REG+xyz), 1, &data);
   data = (offset&0x3ff)>>2;
   comres += i2c_smbus_write_i2c_block_data(this_bma150->client, (BMA150_OFFSET_X_MSB_REG+xyz), 1, &data);
   return comres;
}

/** write offset data to SMB380 image
   \param xyz select axis x=0, y=1, z=2
   \param offset value to write to eeprom(offset is in offset binary representation
   \return result of bus communication function
   \note use smb380_set_ee_w() function to enable access to offset registers in EEPROM space
*/
int smb380_set_offset_eeprom(unsigned char xyz, unsigned short offset) 
{

   int comres;
   unsigned char data;
  
   comres = i2c_smbus_read_i2c_block_data(this_bma150->client, (BMA150_OFFSET_X_LSB_REG+xyz), 1, &data);
   data = SMB380_SET_BITSLICE(data, BMA150_OFFSET_X_LSB, offset);
   comres += i2c_smbus_write_i2c_block_data(this_bma150->client, (BMA150_EEP_OFFSET + BMA150_OFFSET_X_LSB_REG + xyz), 1, &data);   
   bma150_set_delay(&this_bma150->client->dev,34);
   data = (offset&0x3ff)>>2;
   comres += i2c_smbus_write_i2c_block_data(this_bma150->client, (BMA150_EEP_OFFSET + BMA150_OFFSET_X_MSB_REG+xyz), 1, &data);
   bma150_set_delay(&this_bma150->client->dev, 34);
   return comres;
}

/** calculates new offset in respect to acceleration data and old offset register values
  \param orientation pass orientation one axis needs to be absolute 1 the others need to be 0
  \param *offset_x takes the old offset value and modifies it to the new calculated one
  \param *offset_y takes the old offset value and modifies it to the new calculated one
  \param *offset_z takes the old offset value and modifies it to the new calculated one
 */
static int smb380_calc_new_offset(smb380acc_t orientation, smb380acc_t accel, 
                                             unsigned short *offset_x, unsigned short *offset_y, unsigned short *offset_z)
{
   short old_offset_x, old_offset_y, old_offset_z;
   short new_offset_x, new_offset_y, new_offset_z;   

   unsigned char  calibrated =0;

   old_offset_x = *offset_x;
   old_offset_y = *offset_y;
   old_offset_z = *offset_z;
   
   accel.x = accel.x - (orientation.x * 256);   
   accel.y = accel.y - (orientation.y * 256);
   accel.z = accel.z - (orientation.z * 256);
   	                                
   if ((accel.x > 4) | (accel.x < -4)) {           /* does x axis need calibration? */
	     
	 if ((accel.x <8) && accel.x > 0)              /* check for values less than quantisation of offset register */
	 	new_offset_x= old_offset_x -1;           
	 else if ((accel.x >-8) && (accel.x < 0))    
	   new_offset_x= old_offset_x +1;
     else 
       new_offset_x = old_offset_x - (accel.x/8);  /* calculate new offset due to formula */
     if (new_offset_x <0) 						/* check for register boundary */
	   new_offset_x =0;							/* <0 ? */
     else if (new_offset_x>1023)
	   new_offset_x=1023;                       /* >1023 ? */
     *offset_x = new_offset_x;     
	 calibrated = 1;
   }

   if ((accel.y > 4) | (accel.y<-4)) {            /* does y axis need calibration? */
		 if ((accel.y <8) && accel.y > 0)             /* check for values less than quantisation of offset register */
	 	new_offset_y= old_offset_y -1;
	 else if ((accel.y >-8) && accel.y < 0)
	   new_offset_y= old_offset_y +1;	    
     else 
       new_offset_y = old_offset_y - accel.y/8;  /* calculate new offset due to formula */
     
	 if (new_offset_y <0) 						/* check for register boundary */
	   new_offset_y =0;							/* <0 ? */
     else if (new_offset_y>1023)
       new_offset_y=1023;                       /* >1023 ? */
   
     *offset_y = new_offset_y;
     calibrated = 1;
   }

   if ((accel.z > 4) | (accel.z<-4)) {            /* does z axis need calibration? */
	   	 if ((accel.z <8) && accel.z > 0)             /* check for values less than quantisation of offset register */  
	 	new_offset_z= old_offset_z -1;
	 else if ((accel.z >-8) && accel.z < 0)
	   new_offset_z= old_offset_z +1;	     
     else 
       new_offset_z = old_offset_z - (accel.z/8);/* calculate new offset due to formula */
 
     if (new_offset_z <0) 						/* check for register boundary */
	   new_offset_z =0;							/* <0 ? */
     else if (new_offset_z>1023)
	   new_offset_z=1023;

	 *offset_z = new_offset_z;
	 calibrated = 1;
  }	   
  return calibrated;
}

/** reads out acceleration data and averages them, measures min and max
  \param orientation pass orientation one axis needs to be absolute 1 the others need to be 0
  \param num_avg numer of samples for averaging
  \param *min returns the minimum measured value
  \param *max returns the maximum measured value
  \param *average returns the average value
 */
static int smb380_read_accel_avg(int num_avg, smb380acc_t *min, smb380acc_t *max, smb380acc_t *avg )
{
   long x_avg=0, y_avg=0, z_avg=0;   
   int i;
   struct acceleration accel;		                /* read accel data */

   x_avg = 0; y_avg=0; z_avg=0;                  
   max->x = -512; max->y =-512; max->z = -512;
   min->x = 512;  min->y = 512; min->z = 512; 
     

	 for (i=0; i<num_avg; i++) {
	    bma150_measure(this_bma150, &accel);
		if (accel.x>max->x)
			max->x = accel.x;
		if (accel.x<min->x) 
			min->x=accel.x;

		if (accel.y>max->y)
			max->y = accel.y;
		if (accel.y<min->y) 
			min->y=accel.y;

		if (accel.z>max->z)
			max->z = accel.z;
		if (accel.z<min->z) 
			min->z=accel.z;
		
		x_avg+= accel.x;
		y_avg+= accel.y;
		z_avg+= accel.z;

		bma150_set_delay(&this_bma150->client->dev, 10);
     }
	 avg->x = x_avg /= num_avg;                             /* calculate averages, min and max values */
	 avg->y = y_avg /= num_avg;
	 avg->z = z_avg /= num_avg;
	 return 0;
}

/** verifies the accerleration values to be good enough for calibration calculations
 \param min takes the minimum measured value
  \param max takes the maximum measured value
  \param takes returns the average value
  \return 1: min,max values are in range, 0: not in range
*/
static int smb380_verify_min_max(smb380acc_t min, smb380acc_t max, smb380acc_t avg) 
{
	short dx, dy, dz;
	int ver_ok=1;
	
	dx =  max.x - min.x;    /* calc delta max-min */
	dy =  max.y - min.y;
	dz =  max.z - min.z;


	if (dx> 10 || dx<-10) 
	  ver_ok = 0;
	if (dy> 10 || dy<-10) 
	  ver_ok = 0;
	if (dz> 10 || dz<-10) 
	  ver_ok = 0;

	return ver_ok;
}	

/** overall calibration process. This function takes care about all other functions 
  \param orientation input for orientation [0;0;1] for measuring the device in horizontal surface up
  \param *tries takes the number of wanted iteration steps, this pointer returns the number of calculated steps after this routine has finished
  \return 1: calibration passed 2: did not pass within N steps 
*/  
static int smb380_calibrate(smb380acc_t orientation, int *tries)
{
	unsigned short offset_x, offset_y, offset_z;
	unsigned short old_offset_x, old_offset_y, old_offset_z;
	int need_calibration=0, min_max_ok=0;	
	int ltries;

	smb380acc_t min,max,avg;

	smb380_set_range(BMA150_RANGE_2G);
	smb380_set_bandwidth(BMA150_BANDWIDTH_25HZ);

	smb380_set_ee_w(1);  
	
	smb380_get_offset(0, &offset_x);
	smb380_get_offset(1, &offset_y);
	smb380_get_offset(2, &offset_z);

	old_offset_x = offset_x;
	old_offset_y = offset_y;
	old_offset_z = offset_z;
	ltries = *tries;

	do {

		smb380_read_accel_avg(10, &min, &max, &avg);  /* read acceleration data min, max, avg */

		min_max_ok = smb380_verify_min_max(min, max, avg);

		/* check if calibration is needed */
		if (min_max_ok)
			need_calibration = smb380_calc_new_offset(orientation, avg, &offset_x, &offset_y, &offset_z);		
		  
		if (*tries==0) /*number of maximum tries reached? */
			break;

		if (need_calibration) {   
			/* when needed calibration is updated in image */
			(*tries)--;
			smb380_set_offset(0, offset_x); 
   		 	smb380_set_offset(1, offset_y);
 	  	 	smb380_set_offset(2, offset_z);
			bma150_set_delay(&this_bma150->client->dev, 20);
   		}
    } while (need_calibration || !min_max_ok);

	        		
    if (*tries>0 && *tries < ltries) {

		if (old_offset_x!= offset_x) 
			smb380_set_offset_eeprom(0, offset_x);

		if (old_offset_y!= offset_y) 
	   		smb380_set_offset_eeprom(1,offset_y);

		if (old_offset_z!= offset_z) 
	   		smb380_set_offset_eeprom(2, offset_z);
	}	
		
	smb380_set_ee_w(0);	    
    bma150_set_delay(&this_bma150->client->dev, 20);
	*tries = ltries - *tries;

	return !need_calibration;
}
#endif //NEED_CALIBRATION

static int bma150_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct bma150_data *bma150;
	int err;

    rdebug("%s called", __func__);

	/* setup private data */
	bma150 = kzalloc(sizeof(struct bma150_data), GFP_KERNEL);
	if (!bma150) {
		err = -ENOMEM;
		goto error_0;
	}
	mutex_init(&bma150->enable_mutex);
	mutex_init(&bma150->data_mutex);

	/* setup i2c client */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto error_1;
	}
	i2c_set_clientdata(client, bma150);
	bma150->client = client;

	/* detect and init hardware */
	if ((err = bma150_detect(client, NULL))) {
		goto error_1;
	}
	dev_info(&client->dev, "%s found\n", id->name);
	dev_info(&client->dev, "al_version=%d, ml_version=%d\n",
		 bma150_read_bits(bma150, BMA150_AL_VERSION),
		 bma150_read_bits(bma150, BMA150_ML_VERSION));

    bma150_hw_init(bma150);
	bma150_set_delay(&client->dev, BMA150_DEFAULT_DELAY);
	bma150_set_position(&client->dev, CONFIG_INPUT_BMA150_POSITION);

	/* setup driver interfaces */
	INIT_DELAYED_WORK(&bma150->work, bma150_work_func);

//bma150_set_enable(&client->dev, 1); // ktj, 임시로 동작하게 수정함

	err = bma150_input_init(bma150);
	if (err < 0) {
		goto error_1;
	}

	err = sysfs_create_group(&bma150->input->dev.kobj, &bma150_attribute_group);
	if (err < 0) {
		goto error_2;
	}

#ifdef USE_IOCTL
  	err = misc_register(&bma150_device);
	if (err) {
		printk(KERN_ERR "bma150 misc device register failed\n");
		goto error_2;
	}
    else
	    printk(KERN_INFO "ld070 misc device registered ok\n");

    this_bma150 = bma150;
#endif

    rdebug("bma150_probe success");

	return 0;

error_2:
	bma150_input_fini(bma150);
error_1:
	kfree(bma150);
error_0:
	return err;
}

static int bma150_remove(struct i2c_client *client)
{
	struct bma150_data *bma150 = i2c_get_clientdata(client);

	bma150_set_enable(&client->dev, 0);

	sysfs_remove_group(&bma150->input->dev.kobj, &bma150_attribute_group);
	bma150_input_fini(bma150);
	kfree(bma150);

	return 0;
}

static int bma150_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct bma150_data *bma150 = i2c_get_clientdata(client);

	return 0;//FIXME,sleep/wakeup issue
    rdebug("%s called", __func__);
	TRACE_FUNC();

	mutex_lock(&bma150->enable_mutex);

#ifdef USE_IOCTL /* ktj, fix issue after wakeup */
		bma150_power_down(bma150);
#else
	if (bma150_get_enable(&client->dev)) {
		cancel_delayed_work_sync(&bma150->work);
		bma150_power_down(bma150);
	}
#endif

#if DEBUG
	bma150->suspend = 1;
#endif

	mutex_unlock(&bma150->enable_mutex);

	return 0;
}

static int bma150_resume(struct i2c_client *client)
{
	struct bma150_data *bma150 = i2c_get_clientdata(client);
	int delay = atomic_read(&bma150->delay);

	return 0; //FIXME, sleep/wakeup issue
    rdebug("%s called", __func__);
	TRACE_FUNC();

	bma150_hw_init(bma150);
	bma150_set_delay(&client->dev, delay);

	mutex_lock(&bma150->enable_mutex);

	if (bma150_get_enable(&client->dev)) {
		bma150_power_up(bma150);
		schedule_delayed_work(&bma150->work,
				      delay_to_jiffies(delay) + 1);
	}
	
#if DEBUG
	bma150->suspend = 0;
#endif

	mutex_unlock(&bma150->enable_mutex);

	return 0;
}

static const struct i2c_device_id bma150_id[] = {
	{BMA150_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, bma150_id);

struct i2c_driver bma150_driver ={
	.driver = {
		.name = "bma150",
		.owner = THIS_MODULE,
	},
	.probe = bma150_probe,
	.remove = bma150_remove,
	.suspend = bma150_suspend,
	.resume = bma150_resume,
	.id_table = bma150_id,
};

/*
 * Module init and exit
 */
static int __init bma150_init(void)
{
    rdebug("%s called", __func__);
	return i2c_add_driver(&bma150_driver);
}
module_init(bma150_init);

static void __exit bma150_exit(void)
{
	i2c_del_driver(&bma150_driver);
}
module_exit(bma150_exit);

MODULE_DESCRIPTION("BMA150 accelerometer driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(BMA150_VERSION);
