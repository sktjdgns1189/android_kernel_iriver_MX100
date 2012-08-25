/* Source for:
 * Cypress TrueTouch(TM) Standard Product I2C touchscreen driver.
 * drivers/input/touchscreen/cyttsp-i2c.c
 *
 * Copyright (C) 2009, 2010 Cypress Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Cypress reserves the right to make changes without further notice
 * to the materials described herein. Cypress does not assume any
 * liability arising out of the application described herein.
 *
 * Contact Cypress Semiconductor at www.cypress.com
 *
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
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif /* CONFIG_HAS_EARLYSUSPEND */

#define CYTTSP_DECLARE_GLOBALS

#include <linux/cyttsp.h>

uint32_t cyttsp_tsdebug1;
module_param_named(tsdebug1, cyttsp_tsdebug1, uint, 0664);

/* CY TTSP I2C Driver private data */
struct cyttsp {
	struct i2c_client *client;
	struct input_dev *input;
	struct work_struct work;
	struct timer_list timer;
	struct mutex mutex;
	char phys[32];
	struct cyttsp_platform_data *platform_data;
	u8 num_prev_st_touch;
	u16 active_track[CYTTSP_NUM_TRACK_ID];
	u16 prev_st_touch[CYTTSP_NUM_ST_TOUCH_ID];
	u16 prev_mt_touch[CYTTSP_NUM_MT_TOUCH_ID];
	u16 prev_mt_pos[CYTTSP_NUM_MT_TOUCH_ID][2];
	atomic_t irq_enabled;
	struct early_suspend early_suspend;
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cyttsp_early_suspend(struct early_suspend *handler);
static void cyttsp_late_resume(struct early_suspend *handler);
#endif /* CONFIG_HAS_EARLYSUSPEND */

/* ****************************************************************************
 * Prototypes for static functions
 * ************************************************************************** */
static void cyttsp_xy_worker(struct work_struct *work);
static irqreturn_t cyttsp_irq(int irq, void *handle);
static int cyttsp_inlist(u16 prev_track[], u8 curr_track_id, u8 *prev_loc, u8 num_touches);
static int cyttsp_next_avail_inlist(u16 curr_track[], u8 *new_loc, u8 num_touches);
static int cyttsp_putbl(struct cyttsp *ts, int show, int show_status, int show_version, int show_cid);
static int __devinit cyttsp_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int __devexit cyttsp_remove(struct i2c_client *client);
static int cyttsp_resume(struct i2c_client *client);
static int cyttsp_suspend(struct i2c_client *client, pm_message_t message);

/* Static variables */
static struct cyttsp_gen3_xydata_t g_xy_data;
static struct cyttsp_bootloader_data_t g_bl_data;
static struct cyttsp_sysinfo_data_t g_sysinfo_data;
static struct cyttsp_gen3_xydata_t g_wake_data;
static const struct i2c_device_id cyttsp_id[] = {
	{ CYTTSP_I2C_NAME, 0 },  { }
};

MODULE_DEVICE_TABLE(i2c, cyttsp_id);

static struct i2c_driver cyttsp_driver = {
	.driver = {
		.name = CYTTSP_I2C_NAME,
		.owner = THIS_MODULE,
	},
	.probe = cyttsp_probe,
	.remove = __devexit_p(cyttsp_remove),
	.suspend = cyttsp_suspend,
	.resume = cyttsp_resume,
	.id_table = cyttsp_id,
};

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard touchscreen driver");
MODULE_AUTHOR("Cypress");

static ssize_t cyttsp_irq_status(struct device *dev,
                                 struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = container_of(dev,
	                                         struct i2c_client, dev);
	struct cyttsp *ts = i2c_get_clientdata(client);
	return sprintf(buf, "%u\n", atomic_read(&ts->irq_enabled));
}

static ssize_t cyttsp_irq_enable(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf, size_t size)
{
	struct i2c_client *client = container_of(dev,
	                                         struct i2c_client, dev);
	struct cyttsp *ts = i2c_get_clientdata(client);
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
			msg = cyttsp_read_msg(ts);
			if (msg == NULL)
				pr_err("%s: Cannot read message\n", __func__);
*/
			enable_irq(ts->client->irq);
		}
		err = size;
		break;
	default:
		pr_info("cyttsp_irq_enable failed -> irq_enabled = %d\n",
		atomic_read(&ts->irq_enabled));
		err = -EINVAL;
		break;
	}

	return err;
}





static inline __s32 i2c_smbus_read_i2c_block_data2(struct i2c_client *client, __u8 command,
                                                  __u8 length, __u8 *values)
{
int i;

#if 1  // 현재 아래걸로 동작됨. 
	struct i2c_msg xfer[2];
	int ret;

	/* write register */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len =  1;
	xfer[0].buf = &command;

	/* read data */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD; // jhlim-touch | I2C_M_DIBCOM_MODE;
	xfer[1].len = length * sizeof(u8);
	xfer[1].buf = values;

	ret = i2c_transfer(client->adapter, xfer, 2);

	#if 0
	for(i=0;i<length;i++ ) {
		printk("%d\n",values[i]);
	}
	#endif
	
	if (ret < 0) {

		printk("%s: returns %d\n", __func__, ret);
		return -EINVAL;
	}
	return ret;
#else


#if 1
	int i,j;
	unsigned char buf;

	i2c_master_send(client, &command, 1);
	#if  1
	if (length > 32)
		length = 32;

	for (i = 0; i < length; i++) {
		j = i2c_smbus_read_byte(client);
		if (j < 0) {
			return -1;
		}
		values[i] = j;
	}
		
	return length;	
	#else
	if (length > 32)
		length = 32;

	return i2c_master_recv(client, values, length);
	#endif

#else
	int i,j;
	
	if (length > 32)
		length = 32;

	for (i = 0; i < length; i++) {
		j = i2c_smbus_read_byte(client);
		if (j < 0) {
			return -1;
		}
		values[i] = j;
	}
		
	return length;
#endif	
#endif
}


static DEVICE_ATTR(irq_enable, 0777, cyttsp_irq_status, cyttsp_irq_enable);

/* The cyttsp_xy_worker function reads the XY coordinates and sends them to
 * the input layer.  It is scheduled from the interrupt (or timer).
 */

static int cyttsp_read3(struct i2c_client *i2c,u8 reg)
{
	unsigned char buf;

	i2c_master_send(i2c, &reg, 1);
	
	i2c_master_recv(i2c, &buf, 1);


	return buf;
}



void cyttsp_xy_worker(struct work_struct *work)
{
	struct cyttsp *ts = container_of(work,struct cyttsp,work);
	u8 id, tilt, reverse_x, reverse_y;
	u8 i, loc;
	u8 prev_touches;
	u8 curr_touches;
	u16 temp_track[CYTTSP_NUM_MT_TOUCH_ID];
	u16 send_track[CYTTSP_NUM_MT_TOUCH_ID];
	u16 curr_track[CYTTSP_NUM_TRACK_ID];
	u16 curr_st_touch[CYTTSP_NUM_ST_TOUCH_ID];
	u16 curr_mt_touch[CYTTSP_NUM_MT_TOUCH_ID];
	u16 curr_mt_pos[CYTTSP_NUM_TRACK_ID][2];	/* if NOT CYTTSP_USE_TRACKING_ID then only uses CYTTSP_NUM_MT_TOUCH_ID positions */
	u8 curr_mt_z[CYTTSP_NUM_TRACK_ID];			/* if NOT CYTTSP_USE_TRACKING_ID then only uses CYTTSP_NUM_MT_TOUCH_ID positions */
	u8 curr_tool_width;
	u16 st_x1, st_y1;
	u8 st_z1;
	u16 st_x2, st_y2;
	u8 st_z2;
	s32 retval;
	int button_key;

#if 1
	button_key = cyttsp_read3(ts->client,0x1B);
	if (button_key < 0){
		goto exit_xy_worker;
	}
//	g_button_touch = 0;
	
	if (button_key & 0x40)    // 0x80>>touch    0x40>> key  
	{
		if (button_key == 0x42)
		{
		//	g_button_touch = 2;
			//input_report_key(tcc_private->poll_dev->input, KEY_MENU, CYTTSP_TOUCH);
			//input_sync(tcc_private->poll_dev->input);

			cyttsp_debug(" right key \n");
		}	
		else if (button_key == 0x41) 
		{
			//g_button_touch = 1;
			//input_report_key(tcc_private->poll_dev->input, KEY_BACK, CYTTSP_TOUCH);
			//input_sync(tcc_private->poll_dev->input);

			cyttsp_debug(" left key \n");
		}
		
	}
	else
#endif
		///////////////////////////////////
	{
		cyttsp_xdebug("TTSP worker start 1:\n");

		/* get event data from CYTTSP device */
		i = CYTTSP_NUM_RETRY;
		do {
			retval = i2c_smbus_read_i2c_block_data(ts->client, CYTTSP_REG_BASE,
				sizeof(struct cyttsp_gen3_xydata_t), (u8 *)&g_xy_data);



					#if 0
					g_xy_data.x1 =  g_xy_data.x1 * 4 /5;
					g_xy_data.x2 =  g_xy_data.x2 * 4 /5;
					g_xy_data.x3 =  g_xy_data.x3 * 4 /5;
					g_xy_data.x4 =  g_xy_data.x4 * 4 /5;
					
					g_xy_data.y1 =  g_xy_data.y1 * 25 /32;
					g_xy_data.y2 =  g_xy_data.y2 * 25 /32;
					g_xy_data.y3 =  g_xy_data.y3 * 25 /32;
					g_xy_data.y4 =  g_xy_data.y4 * 25 /32;
					#else
					#if 0
					printk("X1:%03d Y1:%03d X2:%03d  Y2:%03d X3:%03d Y3%03d  X4:%03d  Y4:%03d\n", \
							g_xy_data.x1, g_xy_data.y1, 
							g_xy_data.x2, g_xy_data.y2,
							g_xy_data.x3, g_xy_data.y3,
							g_xy_data.x4, g_xy_data.y4);
					#endif

					#endif

				
		}		
		while ((retval < CYTTSP_OPERATIONAL) && --i);

		/* return immediately on failure to read device on the i2c bus */
		if (retval < CYTTSP_OPERATIONAL) {
			goto exit_xy_worker;
		}

	cyttsp_xdebug("TTSP worker start 2:\n");
		
		/* Get the current number of touches and return if there are no touches */
		if ((GET_BOOTLOADERMODE(g_xy_data.tt_mode)==1) ||
			(GET_HSTMODE(g_xy_data.hst_mode) != CYTTSP_OPERATIONAL)) {
			u8 host_reg, tries;
			/* the TTSP device has suffered spurious reset or mode switch */
			cyttsp_debug("Spurious error in operational mode (tt_mode=0x%02X  hst_mode=0x%02X)\n", \
				g_xy_data.tt_mode, g_xy_data.hst_mode);
			cyttsp_debug("Resetting TTSP Device; Terminating active tracks\n");
			/* terminate all active tracks */
			curr_touches = CYTTSP_NOTOUCH; 
			/* reset the TTSP part and take it back out of Bootloader mode */
			host_reg = CYTTSP_SOFT_RESET_MODE;
			retval = i2c_smbus_write_i2c_block_data(ts->client, CYTTSP_REG_BASE, 
				sizeof(host_reg), &host_reg);
			tries = 0;
			do {
				mdelay(1000);

				/* set arg2 to non-0 to activate */
				retval = cyttsp_putbl(ts, 1, false, false, false);
			}
			while (!(retval < CYTTSP_OPERATIONAL) &&
				!GET_BOOTLOADERMODE(g_bl_data.bl_status) && 
				!(g_bl_data.bl_file == CYTTSP_OPERATE_MODE + CYTTSP_LOW_POWER_MODE) &&
				tries++ < 10);
			/* switch back to operational mode */
			if (GET_BOOTLOADERMODE(g_bl_data.bl_status)) {
				static u8 bl_cmd[] = {
					CYTTSP_BL_FILE0, CYTTSP_BL_CMD, CYTTSP_BL_EXIT,
					CYTTSP_BL_KEY0, CYTTSP_BL_KEY1, CYTTSP_BL_KEY2,
					CYTTSP_BL_KEY3, CYTTSP_BL_KEY4, CYTTSP_BL_KEY5,
					CYTTSP_BL_KEY6, CYTTSP_BL_KEY7};
				retval = i2c_smbus_write_i2c_block_data(ts->client, CYTTSP_REG_BASE, sizeof(bl_cmd), bl_cmd);
				tries = 0;
				do {
					mdelay(1000);
					cyttsp_putbl(ts, 1, false, false, false);
				}
				while (GET_BOOTLOADERMODE(g_bl_data.bl_status) && 
					tries++ < 10);
			}
			if (!(retval < CYTTSP_OPERATIONAL)) {
				host_reg = CYTTSP_OPERATE_MODE/* + CYTTSP_LOW_POWER_MODE*/;
				retval = i2c_smbus_write_i2c_block_data(ts->client, CYTTSP_REG_BASE, 
					sizeof(host_reg), &host_reg);
				/* wait for TTSP Device to complete switch to Operational mode */
				mdelay(1000);
			}
		}
		else if ((curr_touches = GET_NUM_TOUCHES(g_xy_data.tt_stat)) > CYTTSP_NUM_MT_TOUCH_ID) {
			/* if the number of fingers on the touch surface is more than the maximum
			 * then there will be no new track information even for the orginal
			 * touches. Therefore, ignore this touch event.
			 */
			 goto exit_xy_worker;
		}
		else if (IS_LARGE_AREA(g_xy_data.tt_stat)==1) {
			/* terminate all active tracks */
			curr_touches = CYTTSP_NOTOUCH;
			cyttsp_debug("Large object detected. Terminating active tracks\n");
		}

		/* set tool size */
		curr_tool_width = CYTTSP_SMALL_TOOL_WIDTH;

		/* translate Gen2 interface data into comparable Gen3 data */
		if(ts->platform_data->gen == CYTTSP_GEN2) {
			struct cyttsp_gen2_xydata_t *pxy_gen2_data;
			pxy_gen2_data = (struct cyttsp_gen2_xydata_t *)(&g_xy_data);

			/* use test data? */
			cyttsp_testdat(&g_xy_data, &tt_gen2_testray, sizeof(struct cyttsp_gen3_xydata_t));

			if (pxy_gen2_data->evnt_idx == CYTTSP_GEN2_NOTOUCH) {
				curr_touches = 0;
			}
			else if (curr_touches == CYTTSP_GEN2_GHOST) {
				curr_touches = 0;
			}
			else if (curr_touches == CYTTSP_GEN2_2TOUCH) {
				g_xy_data.touch12_id = 0x12;	/* stuff artificial track ID1 and ID2 */
				g_xy_data.z1 = CYTTSP_MAXZ;
				g_xy_data.z2 = CYTTSP_MAXZ;
				curr_touches--;			/* 2 touches */
			}
			else if (curr_touches == CYTTSP_GEN2_1TOUCH) {
				g_xy_data.touch12_id = 0x12;	/* stuff artificial track ID1 and ID2 */
				g_xy_data.z1 = CYTTSP_MAXZ;
				g_xy_data.z2 = CYTTSP_NOTOUCH;
				if (pxy_gen2_data->evnt_idx == CYTTSP_GEN2_TOUCH2) {
					/* push touch 2 data into touch1 (first finger up; second finger down) */
					g_xy_data.touch12_id = 0x20;	/* stuff artificial track ID1 for touch 2 info */
					g_xy_data.x1 = g_xy_data.x2;	/* stuff touch 1 with touch 2 coordinate data */
					g_xy_data.y1 = g_xy_data.y2;
				}
			}
			else {
				curr_touches = 0;
			}
		}
		else {
			/* use test data? */
			cyttsp_testdat(&g_xy_data, &tt_gen3_testray, sizeof(struct cyttsp_gen3_xydata_t));
		}
		
		/* clear current active track ID array and count previous touches */
		for (id = 0, prev_touches = CYTTSP_NOTOUCH; id < CYTTSP_NUM_TRACK_ID; id++) {
			curr_track[id] = CYTTSP_NOTOUCH;
			prev_touches += ts->active_track[id];
		}
			
		/* send no events if there were no previous touches and no new touches */
		if ((prev_touches == CYTTSP_NOTOUCH) && 
			((curr_touches == CYTTSP_NOTOUCH) || (curr_touches > CYTTSP_NUM_MT_TOUCH_ID))) {
			goto exit_xy_worker;
		}

		cyttsp_debug("prev=%d  curr=%d\n", prev_touches, curr_touches);

		/* clear current single touches array */
		for (id = 0; id < CYTTSP_NUM_ST_TOUCH_ID; id++) {
			curr_st_touch[id] = CYTTSP_IGNORE_TOUCH;
		}

		/* clear single touch positions */
		st_x1 = CYTTSP_NOTOUCH;
		st_y1 = CYTTSP_NOTOUCH;
		st_z1 = CYTTSP_NOTOUCH;
		st_x2 = CYTTSP_NOTOUCH;
		st_y2 = CYTTSP_NOTOUCH;
		st_z2 = CYTTSP_NOTOUCH;

		/* clear current multi-touches array and multi-touch positions/z */
		for (id = 0; id < CYTTSP_NUM_MT_TOUCH_ID; id++) {
			curr_mt_touch[id] = CYTTSP_IGNORE_TOUCH;
		}

		if (ts->platform_data->use_trk_id) {
			for (id = 0; id < CYTTSP_NUM_MT_TOUCH_ID; id++) {
				curr_mt_pos[id][CYTTSP_XPOS] = 0;
				curr_mt_pos[id][CYTTSP_YPOS] = 0;
				curr_mt_z[id] = 0;
			}
		}
		else {
			for (id = 0; id < CYTTSP_NUM_TRACK_ID; id++) {
				curr_mt_pos[id][CYTTSP_XPOS] = 0;
				curr_mt_pos[id][CYTTSP_YPOS] = 0;
				curr_mt_z[id] = 0;
			}
		}

		/* Determine if display is tilted */
		if (FLIP_DATA(ts->platform_data->flags)) {
			tilt = true;
		}
		else {
			tilt = false;
		}
		/* Check for switch in origin */
		if (REVERSE_X(ts->platform_data->flags)) {
			reverse_x = true;
		}
		else {
			reverse_x = false;
		}
		if (REVERSE_Y(ts->platform_data->flags)) {
			reverse_y = true;
		}
		else {
			reverse_y = false;
		}

		if (curr_touches) {
			struct cyttsp_gen2_xydata_t *pxy_gen2_data;
			struct cyttsp_gen3_xydata_t *pxy_gen3_data;
			switch (ts->platform_data->gen) {
				case CYTTSP_GEN2: {
					pxy_gen2_data = (struct cyttsp_gen2_xydata_t *)(&g_xy_data);
					cyttsp_xdebug("TTSP Gen2 report:\n");
					cyttsp_xdebug("%02X %02X %02X  %04X %04X %02X  %02X  %04X %04X %02X\n", \
							pxy_gen2_data->hst_mode, pxy_gen2_data->tt_mode, pxy_gen2_data->tt_stat, \
							pxy_gen2_data->x1, pxy_gen2_data->y1, pxy_gen2_data->z1, \
							pxy_gen2_data->evnt_idx, \
							pxy_gen2_data->x2, pxy_gen2_data->y2, pxy_gen2_data->tt_undef1);
					cyttsp_xdebug("%02X %02X %02X\n", \
							pxy_gen2_data->gest_cnt, pxy_gen2_data->gest_id, pxy_gen2_data->gest_set);
					break;
				}
				case CYTTSP_GEN3:
				default: {
					pxy_gen3_data = (struct cyttsp_gen3_xydata_t *)(&g_xy_data);
					cyttsp_xdebug("TTSP Gen3 report:\n");
					cyttsp_xdebug("%02X %02X %02X  %04X %04X %02X  %02X  %04X %04X %02X\n", \
							pxy_gen3_data->hst_mode, pxy_gen3_data->tt_mode, pxy_gen3_data->tt_stat, \
							pxy_gen3_data->x1, pxy_gen3_data->y1, pxy_gen3_data->z1, \
							pxy_gen3_data->touch12_id, \
							pxy_gen3_data->x2, pxy_gen3_data->y2, pxy_gen3_data->z2);
					cyttsp_xdebug("%02X %02X %02X  %04X %04X %02X  %02X  %04X %04X %02X\n", \
							pxy_gen3_data->gest_cnt, pxy_gen3_data->gest_id, pxy_gen3_data->gest_set, \
							pxy_gen3_data->x3, pxy_gen3_data->y3, pxy_gen3_data->z3, \
							pxy_gen3_data->touch34_id, \
							pxy_gen3_data->x4, pxy_gen3_data->y4, pxy_gen3_data->z4);
					break;
				}
			}
		}

		/* process the touches */
		switch (curr_touches) {
			case 4: {
				g_xy_data.x4 = be16_to_cpu(g_xy_data.x4);
				g_xy_data.y4 = be16_to_cpu(g_xy_data.y4);
				if (tilt) { 
					FLIP_XY(g_xy_data.x4, g_xy_data.y4);
				}
				if (reverse_x) {
					g_xy_data.x4 = INVERT_X(g_xy_data.x4, ts->platform_data->maxx);
				}
				if (reverse_y) {
					g_xy_data.y4 = INVERT_X(g_xy_data.y4, ts->platform_data->maxy);
				}
				id = GET_TOUCH4_ID(g_xy_data.touch34_id);
				if (ts->platform_data->use_trk_id) {
					curr_mt_pos[CYTTSP_MT_TOUCH4_IDX][CYTTSP_XPOS] = g_xy_data.x4;
					curr_mt_pos[CYTTSP_MT_TOUCH4_IDX][CYTTSP_YPOS] = g_xy_data.y4;
					curr_mt_z[CYTTSP_MT_TOUCH4_IDX] = g_xy_data.z4;
				}
				else {
					curr_mt_pos[id][CYTTSP_XPOS] = g_xy_data.x4;
					curr_mt_pos[id][CYTTSP_YPOS] = g_xy_data.y4;
					curr_mt_z[id] = g_xy_data.z4;
				}
				curr_mt_touch[CYTTSP_MT_TOUCH4_IDX] = id;
				curr_track[id] = CYTTSP_TOUCH;
				if (ts->prev_st_touch[CYTTSP_ST_FINGER1_IDX] < CYTTSP_NUM_TRACK_ID) {
					if (ts->prev_st_touch[CYTTSP_ST_FINGER1_IDX] == id) {
						st_x1 = g_xy_data.x4;
						st_y1 = g_xy_data.y4;
						st_z1 = g_xy_data.z4;
						curr_st_touch[CYTTSP_ST_FINGER1_IDX] = id;
					}
					else if (ts->prev_st_touch[CYTTSP_ST_FINGER2_IDX] == id) {
						st_x2 = g_xy_data.x4;
						st_y2 = g_xy_data.y4;
						st_z2 = g_xy_data.z4;
						curr_st_touch[CYTTSP_ST_FINGER2_IDX] = id;
					}
				}
				cyttsp_xdebug("4th XYZ:% 3d,% 3d,% 3d  ID:% 2d\n\n", \
					g_xy_data.x4, g_xy_data.y4, g_xy_data.z4, (g_xy_data.touch34_id & 0x0F));
				/* do not break */
			}
			case 3: {
				g_xy_data.x3 = be16_to_cpu(g_xy_data.x3);
				g_xy_data.y3 = be16_to_cpu(g_xy_data.y3);
				if (tilt) { 
					FLIP_XY(g_xy_data.x3, g_xy_data.y3);
				}
				if (reverse_x) {
					g_xy_data.x3 = INVERT_X(g_xy_data.x3, ts->platform_data->maxx);
				}
				if (reverse_y) {
					g_xy_data.y3 = INVERT_X(g_xy_data.y3, ts->platform_data->maxy);
				}
				id = GET_TOUCH3_ID(g_xy_data.touch34_id);
				if (ts->platform_data->use_trk_id) {
					curr_mt_pos[CYTTSP_MT_TOUCH3_IDX][CYTTSP_XPOS] = g_xy_data.x3;
					curr_mt_pos[CYTTSP_MT_TOUCH3_IDX][CYTTSP_YPOS] = g_xy_data.y3;
					curr_mt_z[CYTTSP_MT_TOUCH3_IDX] = g_xy_data.z3;
				}
				else {
					curr_mt_pos[id][CYTTSP_XPOS] = g_xy_data.x3;
					curr_mt_pos[id][CYTTSP_YPOS] = g_xy_data.y3;
					curr_mt_z[id] = g_xy_data.z3;
				}
				curr_mt_touch[CYTTSP_MT_TOUCH3_IDX] = id;
				curr_track[id] = CYTTSP_TOUCH;
				if (ts->prev_st_touch[CYTTSP_ST_FINGER1_IDX] < CYTTSP_NUM_TRACK_ID) {
					if (ts->prev_st_touch[CYTTSP_ST_FINGER1_IDX] == id) {
						st_x1 = g_xy_data.x3;
						st_y1 = g_xy_data.y3;
						st_z1 = g_xy_data.z3;
						curr_st_touch[CYTTSP_ST_FINGER1_IDX] = id;
					}
					else if (ts->prev_st_touch[CYTTSP_ST_FINGER2_IDX] == id) {
						st_x2 = g_xy_data.x3;
						st_y2 = g_xy_data.y3;
						st_z2 = g_xy_data.z3;
						curr_st_touch[CYTTSP_ST_FINGER2_IDX] = id;
					}
				}
				cyttsp_xdebug("3rd XYZ:% 3d,% 3d,% 3d  ID:% 2d\n", \
					g_xy_data.x3, g_xy_data.y3, g_xy_data.z3, ((g_xy_data.touch34_id >> 4) & 0x0F));
				/* do not break */
			}
			case 2: {
				g_xy_data.x2 = be16_to_cpu(g_xy_data.x2);
				g_xy_data.y2 = be16_to_cpu(g_xy_data.y2);
				if (tilt) {
					FLIP_XY(g_xy_data.x2, g_xy_data.y2);
				}
				if (reverse_x) {
					g_xy_data.x2 = INVERT_X(g_xy_data.x2, ts->platform_data->maxx);
				}
				if (reverse_y) {
					g_xy_data.y2 = INVERT_X(g_xy_data.y2, ts->platform_data->maxy);
				}
				id = GET_TOUCH2_ID(g_xy_data.touch12_id);
				if (ts->platform_data->use_trk_id) {
					curr_mt_pos[CYTTSP_MT_TOUCH2_IDX][CYTTSP_XPOS] = g_xy_data.x2;
					curr_mt_pos[CYTTSP_MT_TOUCH2_IDX][CYTTSP_YPOS] = g_xy_data.y2;
					curr_mt_z[CYTTSP_MT_TOUCH2_IDX] = g_xy_data.z2;
				}
				else {
					curr_mt_pos[id][CYTTSP_XPOS] = g_xy_data.x2;
					curr_mt_pos[id][CYTTSP_YPOS] = g_xy_data.y2;
					curr_mt_z[id] = g_xy_data.z2;
				}
				curr_mt_touch[CYTTSP_MT_TOUCH2_IDX] = id;
				curr_track[id] = CYTTSP_TOUCH;
				if (ts->prev_st_touch[CYTTSP_ST_FINGER1_IDX] < CYTTSP_NUM_TRACK_ID) {
					if (ts->prev_st_touch[CYTTSP_ST_FINGER1_IDX] == id) {
						st_x1 = g_xy_data.x2;
						st_y1 = g_xy_data.y2;
						st_z1 = g_xy_data.z2;
						curr_st_touch[CYTTSP_ST_FINGER1_IDX] = id;
					}
					else if (ts->prev_st_touch[CYTTSP_ST_FINGER2_IDX] == id) {
						st_x2 = g_xy_data.x2;
						st_y2 = g_xy_data.y2;
						st_z2 = g_xy_data.z2;
						curr_st_touch[CYTTSP_ST_FINGER2_IDX] = id;
					}
				}
				cyttsp_xdebug("2nd XYZ:% 3d,% 3d,% 3d  ID:% 2d\n", \
					g_xy_data.x2, g_xy_data.y2, g_xy_data.z2, (g_xy_data.touch12_id & 0x0F));
				/* do not break */
			}
			case 1:	{
				g_xy_data.x1 = be16_to_cpu(g_xy_data.x1);
				g_xy_data.y1 = be16_to_cpu(g_xy_data.y1);
				if (tilt) {
					FLIP_XY(g_xy_data.x1, g_xy_data.y1);
				}
				if (reverse_x) {
					g_xy_data.x1 = INVERT_X(g_xy_data.x1, ts->platform_data->maxx);
				}
				if (reverse_y) {
					g_xy_data.y1 = INVERT_X(g_xy_data.y1, ts->platform_data->maxy);
				}
				id = GET_TOUCH1_ID(g_xy_data.touch12_id);
				if (ts->platform_data->use_trk_id) {
					curr_mt_pos[CYTTSP_MT_TOUCH1_IDX][CYTTSP_XPOS] = g_xy_data.x1;
					curr_mt_pos[CYTTSP_MT_TOUCH1_IDX][CYTTSP_YPOS] = g_xy_data.y1;
					curr_mt_z[CYTTSP_MT_TOUCH1_IDX] = g_xy_data.z1;
				}
				else {
					curr_mt_pos[id][CYTTSP_XPOS] = g_xy_data.x1;
					curr_mt_pos[id][CYTTSP_YPOS] = g_xy_data.y1;
					curr_mt_z[id] = g_xy_data.z1;
				}
				curr_mt_touch[CYTTSP_MT_TOUCH1_IDX] = id;
				curr_track[id] = CYTTSP_TOUCH;
				if (ts->prev_st_touch[CYTTSP_ST_FINGER1_IDX] < CYTTSP_NUM_TRACK_ID) {
					if (ts->prev_st_touch[CYTTSP_ST_FINGER1_IDX] == id) {
						st_x1 = g_xy_data.x1;
						st_y1 = g_xy_data.y1;
						st_z1 = g_xy_data.z1;
						curr_st_touch[CYTTSP_ST_FINGER1_IDX] = id;
					}
					else if (ts->prev_st_touch[CYTTSP_ST_FINGER2_IDX] == id) {
						st_x2 = g_xy_data.x1;
						st_y2 = g_xy_data.y1;
						st_z2 = g_xy_data.z1;
						curr_st_touch[CYTTSP_ST_FINGER2_IDX] = id;
					}
				}
				cyttsp_xdebug("1st XYZ:% 3d,% 3d,% 3d  ID:% 2d\n", \
					g_xy_data.x1, g_xy_data.y1, g_xy_data.z1, ((g_xy_data.touch12_id >> 4) & 0x0F));
				break;
			}
			case 0:
			default:{
				break;
			}
		}

		/* handle Single Touch signals */
		if (ts->platform_data->use_st) {
			cyttsp_xdebug("ST STEP 0 - ST1 ID=%d  ST2 ID=%d\n", \
				curr_st_touch[CYTTSP_ST_FINGER1_IDX], curr_st_touch[CYTTSP_ST_FINGER2_IDX]);
			if (curr_st_touch[CYTTSP_ST_FINGER1_IDX] > CYTTSP_NUM_TRACK_ID) {
				/* reassign finger 1 and 2 positions to new tracks */
				if (curr_touches > 0) {
					/* reassign st finger1 */
					if (ts->platform_data->use_trk_id) {
						id = CYTTSP_MT_TOUCH1_IDX;
						curr_st_touch[CYTTSP_ST_FINGER1_IDX] = curr_mt_touch[id];
					}
					else {
						id = GET_TOUCH1_ID(g_xy_data.touch12_id);
						curr_st_touch[CYTTSP_ST_FINGER1_IDX] = id;
					}
					st_x1 = curr_mt_pos[id][CYTTSP_XPOS];
					st_y1 = curr_mt_pos[id][CYTTSP_YPOS];
					st_z1 = curr_mt_z[id];
					cyttsp_xdebug("ST STEP 1 - ST1 ID=%3d\n", curr_st_touch[CYTTSP_ST_FINGER1_IDX]);
					if (curr_touches > 1) {
						if (curr_st_touch[CYTTSP_ST_FINGER2_IDX] > CYTTSP_NUM_TRACK_ID) {
							/* reassign st finger2 */
							if (curr_touches > 1) {
								if (ts->platform_data->use_trk_id) {
									id = CYTTSP_MT_TOUCH2_IDX;
									curr_st_touch[CYTTSP_ST_FINGER2_IDX] = curr_mt_touch[id];
								}
								else {
									id = GET_TOUCH2_ID(g_xy_data.touch12_id);
									curr_st_touch[CYTTSP_ST_FINGER2_IDX] = id;
								}
								st_x2 = curr_mt_pos[id][CYTTSP_XPOS];
								st_y2 = curr_mt_pos[id][CYTTSP_YPOS];
								st_z2 = curr_mt_z[id];
								cyttsp_xdebug("ST STEP 2 - ST2 ID=%3d\n", curr_st_touch[CYTTSP_ST_FINGER2_IDX]);
							}
						}
					}
				}
			}
			else if (curr_st_touch[CYTTSP_ST_FINGER2_IDX] > CYTTSP_NUM_TRACK_ID) {
				if (curr_touches > 1) {
					/* reassign st finger2 */
					if (ts->platform_data->use_trk_id) {
						id = CYTTSP_MT_TOUCH2_IDX;
						curr_st_touch[CYTTSP_ST_FINGER2_IDX] = curr_mt_touch[id];	/* reassign st finger2 */
					}
					else {
						id = GET_TOUCH2_ID(g_xy_data.touch12_id);
						curr_st_touch[CYTTSP_ST_FINGER2_IDX] = id;			/* reassign st finger2 */
					}
					st_x2 = curr_mt_pos[id][CYTTSP_XPOS];
					st_y2 = curr_mt_pos[id][CYTTSP_YPOS];
					st_z2 = curr_mt_z[id];
					cyttsp_xdebug("ST STEP 3 - ST2 ID=%3d\n", curr_st_touch[CYTTSP_ST_FINGER2_IDX]);
				}
			}
			/* if the first touch is missing and there is a second touch, 
			 * then set the first touch to second touch and terminate second touch
			 */
			if ((curr_st_touch[CYTTSP_ST_FINGER1_IDX] > CYTTSP_NUM_TRACK_ID) && 
			    (curr_st_touch[CYTTSP_ST_FINGER2_IDX] < CYTTSP_NUM_TRACK_ID)) {
				st_x1 = st_x2;
				st_y1 = st_y2;
				st_z1 = st_z2;
				curr_st_touch[CYTTSP_ST_FINGER1_IDX] = curr_st_touch[CYTTSP_ST_FINGER2_IDX];
				curr_st_touch[CYTTSP_ST_FINGER2_IDX] = CYTTSP_IGNORE_TOUCH;
			}
			/* if the second touch ends up equal to the first touch, then just report a single touch */
			if (curr_st_touch[CYTTSP_ST_FINGER1_IDX] == curr_st_touch[CYTTSP_ST_FINGER2_IDX]) {
				curr_st_touch[CYTTSP_ST_FINGER2_IDX] = CYTTSP_IGNORE_TOUCH;
			}

			
			/* set Single Touch current event signals */
			if (curr_st_touch[CYTTSP_ST_FINGER1_IDX] < CYTTSP_NUM_TRACK_ID) {
				#ifdef USE_MX100_LCD_RES


				FLIP_XY (st_x1,st_y1);

				st_y1 =  st_y1 * 5 /4;
				st_x1 =  st_x1 * 32 /25;
				st_x1 = 1024 - st_x1 ;

				#endif
				
				input_report_abs(ts->input, ABS_X, st_x1);
				input_report_abs(ts->input, ABS_Y, st_y1);
				input_report_abs(ts->input, ABS_PRESSURE, st_z1);
				input_report_key(ts->input, BTN_TOUCH, CYTTSP_TOUCH);
				input_report_abs(ts->input, ABS_TOOL_WIDTH, curr_tool_width);
				cyttsp_debug("ST ->  F1:%3d  X:%3d  Y:%3d  Z:%3d  \n", \
					curr_st_touch[CYTTSP_ST_FINGER1_IDX], st_x1, st_y1, st_z1);
				if (curr_st_touch[CYTTSP_ST_FINGER2_IDX] < CYTTSP_NUM_TRACK_ID) {
					#ifdef USE_MX100_LCD_RES
			
					FLIP_XY (st_x1,st_y1);
					st_y2 =  st_y2 * 5 /4;
					st_x2 =  st_x2 * 32 /25;
					st_x2= 1024 - st_x2 ;	
					#endif
					
					input_report_key(ts->input, BTN_2, CYTTSP_TOUCH);
					input_report_abs(ts->input, ABS_HAT0X, st_x2);
					input_report_abs(ts->input, ABS_HAT0Y, st_y2);
					cyttsp_debug("ST ->  F2:%3d  X:%3d  Y:%3d  Z:%3d  \n", \
						curr_st_touch[CYTTSP_ST_FINGER2_IDX], st_x2, st_y2, st_z2);
				}
				else {
					input_report_key(ts->input, BTN_2, CYTTSP_NOTOUCH);
				}
			}
			else {
				input_report_abs(ts->input, ABS_PRESSURE, CYTTSP_NOTOUCH);
				input_report_key(ts->input, BTN_TOUCH, CYTTSP_NOTOUCH);
				input_report_key(ts->input, BTN_2, CYTTSP_NOTOUCH);
			}
			/* update platform data for the current single touch information */
			ts->prev_st_touch[CYTTSP_ST_FINGER1_IDX] = curr_st_touch[CYTTSP_ST_FINGER1_IDX];
			ts->prev_st_touch[CYTTSP_ST_FINGER2_IDX] = curr_st_touch[CYTTSP_ST_FINGER2_IDX];

		}

		/* handle Multi-touch signals */
		if (ts->platform_data->use_mt) {
			if (ts->platform_data->use_trk_id) {
				/* terminate any previous touch where the track is missing from the current event */
				for (id = 0; id < CYTTSP_NUM_TRACK_ID; id++) {
					if ((ts->active_track[id] != CYTTSP_NOTOUCH) && (curr_track[id] == CYTTSP_NOTOUCH)) {
						input_report_abs(ts->input, ABS_MT_TRACKING_ID, id);
						input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, CYTTSP_NOTOUCH);
						input_report_abs(ts->input, ABS_MT_WIDTH_MAJOR, curr_tool_width);
						input_report_abs(ts->input, ABS_MT_POSITION_X, curr_mt_pos[id][CYTTSP_XPOS]);
						input_report_abs(ts->input, ABS_MT_POSITION_Y, curr_mt_pos[id][CYTTSP_YPOS]);
						CYTTSP_MT_SYNC(ts->input);
					}
				}
				/* set Multi-Touch current event signals */
				for (id = 0; id < CYTTSP_NUM_MT_TOUCH_ID; id++) {
					if (curr_mt_touch[id] < CYTTSP_NUM_TRACK_ID) {
						input_report_abs(ts->input, ABS_MT_TRACKING_ID, curr_mt_touch[id]);
						input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, curr_mt_z[id]);
						input_report_abs(ts->input, ABS_MT_WIDTH_MAJOR, curr_tool_width);
						input_report_abs(ts->input, ABS_MT_POSITION_X, 
							curr_mt_pos[id][CYTTSP_XPOS]);
						input_report_abs(ts->input, ABS_MT_POSITION_Y, 
							curr_mt_pos[id][CYTTSP_YPOS]);
						CYTTSP_MT_SYNC(ts->input);
					}
				}
			}
			else {
				/* set temporary track array elements to voids */
				for (id = 0; id < CYTTSP_NUM_MT_TOUCH_ID; id++) {
					temp_track[id] = CYTTSP_IGNORE_TOUCH;
					send_track[id] = CYTTSP_IGNORE_TOUCH;
				}

				/* get what is currently active */
				for (i = 0, id = 0; id < CYTTSP_NUM_TRACK_ID && i < CYTTSP_NUM_MT_TOUCH_ID; id++) {
					if (curr_track[id] == CYTTSP_TOUCH) {
						temp_track[i] = id;
						i++; /* only increment counter if track found */
					}
				}
				cyttsp_xdebug("T1: t0=%d, t1=%d, t2=%d, t3=%d\n", \
					temp_track[0], temp_track[1], temp_track[2], temp_track[3]);
				cyttsp_xdebug("T1: p0=%d, p1=%d, p2=%d, p3=%d\n", \
					ts->prev_mt_touch[0], ts->prev_mt_touch[1], \
					ts->prev_mt_touch[2], ts->prev_mt_touch[3]);

				/* pack in still active previous touches */
				for (id = 0, prev_touches = 0; id < CYTTSP_NUM_MT_TOUCH_ID; id++) {
					if (temp_track[id] < CYTTSP_NUM_TRACK_ID) {
						if (cyttsp_inlist(ts->prev_mt_touch, temp_track[id], &loc,
							 CYTTSP_NUM_MT_TOUCH_ID)) {
							loc &= CYTTSP_NUM_MT_TOUCH_ID-1;
							send_track[loc] = temp_track[id];
							prev_touches++;
							cyttsp_xdebug("in list s[%d]=%d t[%d]=%d, loc=%d p=%d\n", \
								loc, send_track[loc], \
								id, temp_track[id], loc, prev_touches);
						}
						else {
							cyttsp_xdebug("is not in list s[%d]=%d t[%d]=%d loc=%d\n", \
								id, send_track[id], id, temp_track[id], loc);
						}
					}
				}
				cyttsp_xdebug("S1: s0=%d, s1=%d, s2=%d, s3=%d p=%d\n", \
					send_track[0], send_track[1], send_track[2], send_track[3], \
					prev_touches);

				/* pack in new touches */
				for (id = 0; id < CYTTSP_NUM_MT_TOUCH_ID; id++) {
					if (temp_track[id] < CYTTSP_NUM_TRACK_ID) {
						if (!cyttsp_inlist(send_track, temp_track[id], &loc, CYTTSP_NUM_MT_TOUCH_ID)) {
							cyttsp_xdebug("not in list t[%d]=%d, loc=%d\n", \
								id, temp_track[id], loc);
							if (cyttsp_next_avail_inlist(send_track, &loc, CYTTSP_NUM_MT_TOUCH_ID)) {
								loc &= CYTTSP_NUM_MT_TOUCH_ID-1;
								send_track[loc] = temp_track[id];
								cyttsp_xdebug("put in list s[%d]=%d t[%d]=%d\n", 
									loc, send_track[loc], id, temp_track[id]);
							}
						}
						else {
							cyttsp_xdebug("is in list s[%d]=%d t[%d]=%d loc=%d\n", \
								id, send_track[id], id, temp_track[id], loc);
						}
					}
				}
				cyttsp_xdebug("S2: s0=%d, s1=%d, s2=%d, s3=%d\n", \
					send_track[0], send_track[1], send_track[2], send_track[3]);

				/* synchronize motion event signals for each current touch */
				for (id = 0; id < CYTTSP_NUM_MT_TOUCH_ID; id++) {
					/* z will either be 0 (NOTOUCH) or some pressure (TOUCH) */
					cyttsp_xdebug("MT0 prev[%d]=%d temp[%d]=%d send[%d]=%d\n", \
						id, ts->prev_mt_touch[id], \
						id, temp_track[id], \
						id, send_track[id]);
					if (send_track[id] < CYTTSP_NUM_TRACK_ID) {
						input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, curr_mt_z[send_track[id]]);	
						input_report_abs(ts->input, ABS_MT_WIDTH_MAJOR, curr_tool_width);
						input_report_abs(ts->input, ABS_MT_POSITION_X, curr_mt_pos[send_track[id]][CYTTSP_XPOS]);
						input_report_abs(ts->input, ABS_MT_POSITION_Y, curr_mt_pos[send_track[id]][CYTTSP_YPOS]);
						CYTTSP_MT_SYNC(ts->input);
						cyttsp_debug("MT1->TID:%2d X:%3d Y:%3d Z:%3d act touch-sent\n", \
							send_track[id], curr_mt_pos[send_track[id]][CYTTSP_XPOS], \
							curr_mt_pos[send_track[id]][CYTTSP_YPOS],  curr_mt_z[send_track[id]]);
					}
					else if (ts->prev_mt_touch[id] < CYTTSP_NUM_TRACK_ID) {
						input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, CYTTSP_NOTOUCH);	/* void out this touch */
						input_report_abs(ts->input, ABS_MT_WIDTH_MAJOR, curr_tool_width);
						input_report_abs(ts->input, ABS_MT_POSITION_X, ts->prev_mt_pos[ts->prev_mt_touch[id]][CYTTSP_XPOS]);
						input_report_abs(ts->input, ABS_MT_POSITION_Y, ts->prev_mt_pos[ts->prev_mt_touch[id]][CYTTSP_YPOS]);
						CYTTSP_MT_SYNC(ts->input);
						cyttsp_debug("MT2->TID:%2d X:%3d Y:%3d Z:%3d lift off-sent\n", \
							ts->prev_mt_touch[id], \
							ts->prev_mt_pos[ts->prev_mt_touch[id]][CYTTSP_XPOS], \
							ts->prev_mt_pos[ts->prev_mt_touch[id]][CYTTSP_YPOS], \
							CYTTSP_NOTOUCH);
					}
					else {
						/* do not stuff any signals for this previously and currently void touches */
						cyttsp_xdebug("MT3-> send[%d]=%d - No touch - NOT sent\n", \
								id, send_track[id]);
					}
				}

				/* save current posted tracks to previous track memory */
				for (id = 0; id < CYTTSP_NUM_MT_TOUCH_ID; id++) {
					ts->prev_mt_touch[id] = send_track[id];
					ts->prev_mt_pos[send_track[id]][CYTTSP_XPOS] = curr_mt_pos[send_track[id]][CYTTSP_XPOS];
					ts->prev_mt_pos[send_track[id]][CYTTSP_YPOS] = curr_mt_pos[send_track[id]][CYTTSP_YPOS];
					cyttsp_xdebug("MT4->TID:%2d X:%3d Y:%3d Z:%3d save for previous\n", \
						send_track[id], \
						ts->prev_mt_pos[send_track[id]][CYTTSP_XPOS], \
						ts->prev_mt_pos[send_track[id]][CYTTSP_YPOS], \
						CYTTSP_NOTOUCH);
				}
			}
		}

		/* handle gestures */
		if (ts->platform_data->use_gestures) {
			if (g_xy_data.gest_id) {
				input_report_key(ts->input, BTN_3, CYTTSP_TOUCH);
				input_report_abs(ts->input, ABS_HAT1X, g_xy_data.gest_id);
				input_report_abs(ts->input, ABS_HAT2Y, g_xy_data.gest_cnt);
			}
		}

		/* signal the view motion event */
		input_sync(ts->input);

		/* update platform data for the current multi-touch information */
		for (id = 0; id < CYTTSP_NUM_TRACK_ID; id++) {
			ts->active_track[id] = curr_track[id];
		}
	}
exit_xy_worker:
	if(cyttsp_disable_touch) {
		cyttsp_debug("Not enabling touch\n");
	}
	else {
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

static int cyttsp_inlist(u16 prev_track[], u8 curr_track_id, u8 *prev_loc, u8 num_touches)
{
	u8 id =0;

	*prev_loc = CYTTSP_IGNORE_TOUCH;

		cyttsp_xdebug("IN p[%d]=%d c=%d n=%d loc=%d\n", \
			id, prev_track[id], curr_track_id, num_touches, *prev_loc);
	for (id = 0, *prev_loc = CYTTSP_IGNORE_TOUCH;
		(id < num_touches); id++) {
		cyttsp_xdebug("p[%d]=%d c=%d n=%d loc=%d\n", \
			id, prev_track[id], curr_track_id, num_touches, *prev_loc);
		if (prev_track[id] == curr_track_id) {
			*prev_loc = id;
			break;
		}
	}
		cyttsp_xdebug("OUT p[%d]=%d c=%d n=%d loc=%d\n", \
			id, prev_track[id], curr_track_id, num_touches, *prev_loc);

	return ((*prev_loc < CYTTSP_NUM_TRACK_ID) ? true : false);
}

static int cyttsp_next_avail_inlist(u16 curr_track[], u8 *new_loc, u8 num_touches)
{
	u8 id;

	for (id = 0, *new_loc = CYTTSP_IGNORE_TOUCH;
		(id < num_touches); id++) {
		if (curr_track[id] > CYTTSP_NUM_TRACK_ID) {
			*new_loc = id;
			break;
		}
	}

	return ((*new_loc < CYTTSP_NUM_TRACK_ID) ? true : false);
}

/* Timer function used as dummy interrupt driver */
static void cyttsp_timer(unsigned long handle)
{
	struct cyttsp *ts = (struct cyttsp *) handle;

	cyttsp_xdebug("TTSP Device timer event\n");

	/* schedule motion signal handling */
	schedule_work(&ts->work);

	return;
}



/* ************************************************************************
 * ISR function. This function is general, initialized in drivers init
 * function
 * ************************************************************************ */
static irqreturn_t cyttsp_irq(int irq, void *handle)
{
	struct cyttsp *ts = (struct cyttsp *) handle;

	cyttsp_xdebug("%s: Got IRQ\n", CYTTSP_I2C_NAME);

	/* disable further interrupts until this interrupt is processed */
	disable_irq_nosync(ts->client->irq);

	/* schedule motion signal handling */
	schedule_work(&ts->work);
	return IRQ_HANDLED;
}

/* ************************************************************************
 * Probe initialization functions
 * ************************************************************************ */
static int cyttsp_putbl(struct cyttsp *ts, int show, int show_status, int show_version, int show_cid)
{
	int retval = CYTTSP_OPERATIONAL;

	int num_bytes = (show_status * 3) + (show_version * 6) + (show_cid * 3);

	if (show_cid) {
		num_bytes = sizeof(struct cyttsp_bootloader_data_t);
	}
	else if (show_version) {
		num_bytes = sizeof(struct cyttsp_bootloader_data_t) - 3;
	}
	else {
		num_bytes = sizeof(struct cyttsp_bootloader_data_t) - 9;
	}

	if (show) {
		retval = i2c_smbus_read_i2c_block_data(ts->client, CYTTSP_REG_BASE,
			num_bytes, (u8 *)&g_bl_data);
		if (show_status) {
			cyttsp_debug("BL%d: f=%02X s=%02X err=%02X bl=%02X%02X bld=%02X%02X\n", \
				show, \
				g_bl_data.bl_file, g_bl_data.bl_status, g_bl_data.bl_error, \
				g_bl_data.blver_hi, g_bl_data.blver_lo, \
				g_bl_data.bld_blver_hi, g_bl_data.bld_blver_lo);
		}
		if (show_version) {
			cyttsp_debug("BL%d: ttspver=0x%02X%02X appid=0x%02X%02X appver=0x%02X%02X\n", \
				show, \
				g_bl_data.ttspver_hi, g_bl_data.ttspver_lo, \
				g_bl_data.appid_hi, g_bl_data.appid_lo, \
				g_bl_data.appver_hi, g_bl_data.appver_lo);
		}
		if (show_cid) {
			cyttsp_debug("BL%d: cid=0x%02X%02X%02X\n", \
				show, \
				g_bl_data.cid_0, g_bl_data.cid_1, g_bl_data.cid_2);
		}
		mdelay(CYTTSP_DELAY_DFLT);
	}

	return retval;
}

#ifdef CYTTSP_INCLUDE_LOAD_FILE
#define CYTTSP_MAX_I2C_LEN	256
#define CYTTSP_MAX_TRY		10
#define CYTTSP_BL_PAGE_SIZE	16
#define CYTTSP_BL_NUM_PAGES	5
static int cyttsp_i2c_write_block_data(struct i2c_client *client, u8 command,
			       u8 length, const u8 *values)
{
	int retval = CYTTSP_OPERATIONAL;

	u8 dataray[CYTTSP_MAX_I2C_LEN];
	u8 try;
	dataray[0] = command;
	if (length) {
		memcpy(&dataray[1], values, length);
	}

	try = CYTTSP_MAX_TRY;
	do {
		retval = i2c_master_send(client, dataray, length+1);
		mdelay(CYTTSP_DELAY_DFLT*2);
	}
	while ((retval != length+1) && try--);

	return retval;
}

static int cyttsp_i2c_write_block_data_chunks(struct cyttsp *ts, u8 command,
			       u8 length, const u8 *values)
{
	int retval = CYTTSP_OPERATIONAL;
	int block = 1;

	u8 dataray[CYTTSP_MAX_I2C_LEN];

	/* first page already includes the bl page offset */
	retval = i2c_smbus_write_i2c_block_data(ts->client, CYTTSP_REG_BASE, 
		CYTTSP_BL_PAGE_SIZE+1, values);
	mdelay(10);
	values += CYTTSP_BL_PAGE_SIZE+1;
	length -= CYTTSP_BL_PAGE_SIZE+1;

	/* rem blocks require bl page offset stuffing */
	while (length && (block < CYTTSP_BL_NUM_PAGES) && !(retval < CYTTSP_OPERATIONAL)) {
		dataray[0] = CYTTSP_BL_PAGE_SIZE*block;
		memcpy(&dataray[1], values, 
			length >= CYTTSP_BL_PAGE_SIZE ? CYTTSP_BL_PAGE_SIZE : length);
		retval = i2c_smbus_write_i2c_block_data(ts->client, CYTTSP_REG_BASE, 
			length >= CYTTSP_BL_PAGE_SIZE ? CYTTSP_BL_PAGE_SIZE+1 : length+1, dataray);
		mdelay(10);
		values += CYTTSP_BL_PAGE_SIZE;
		length = length >= CYTTSP_BL_PAGE_SIZE ? length - CYTTSP_BL_PAGE_SIZE : 0;
		block++;
	}

	return retval;
}

static int cyttsp_bootload_app(struct cyttsp *ts)
{
	int retval = CYTTSP_OPERATIONAL;
	int i, tries;
	u8 host_reg;

	cyttsp_debug("load new firmware \n");
	/* reset TTSP Device back to bootloader mode */
	host_reg = CYTTSP_SOFT_RESET_MODE;
	retval = i2c_smbus_write_i2c_block_data(ts->client, CYTTSP_REG_BASE, 
		sizeof(host_reg), &host_reg);
	/* wait for TTSP Device to complete reset back to bootloader */
//	mdelay(CYTTSP_DELAY_DFLT);
	mdelay(1000);
	cyttsp_putbl(ts,3, true, true, true);
	cyttsp_debug("load file -- tts_ver=0x%02X%02X  app_id=0x%02X%02X  app_ver=0x%02X%02X\n", \
		cyttsp_fw_tts_verh, cyttsp_fw_tts_verl, \
		cyttsp_fw_app_idh, cyttsp_fw_app_idl, \
		cyttsp_fw_app_verh, cyttsp_fw_app_verl);

	/* download new TTSP Application to the Bootloader
	 *
	 */
	if (!(retval < CYTTSP_OPERATIONAL)) {
		i = 0;
		/* send bootload initiation command */
		if (cyttsp_fw[i].Command == CYTTSP_BL_INIT_LOAD) {
			g_bl_data.bl_file = 0;
			g_bl_data.bl_status = 0;
			g_bl_data.bl_error = 0;
			retval = i2c_smbus_write_i2c_block_data(ts->client, CYTTSP_REG_BASE, 
				cyttsp_fw[i].Length, cyttsp_fw[i].Block);
			/* delay to allow bootloader to get ready for block writes */
			i++;
			tries = 0;
			cyttsp_debug("wait init f=%02X, s=%02X, e=%02X t=%d\n",g_bl_data.bl_file,
				g_bl_data.bl_status, g_bl_data.bl_error, tries);
			do {
				mdelay(1000);
				cyttsp_putbl(ts,4, true, false, false);
			}
			while (g_bl_data.bl_status != 0x10 && 
				g_bl_data.bl_status != 0x11 && 
				tries++ < 10);
			/* send bootload firmware load blocks - 
			 * kernel limits transfers to I2C_SMBUS_BLOCK_MAX(32) bytes
			 */
			if (!(retval < CYTTSP_OPERATIONAL)) {
				while (cyttsp_fw[i].Command == CYTTSP_BL_WRITE_BLK) {
					retval = cyttsp_i2c_write_block_data_chunks(ts,
						CYTTSP_REG_BASE, 
						cyttsp_fw[i].Length, cyttsp_fw[i].Block);
//					if (cyttsp_fw[i].Address & 0x01) {
//						mdelay(CYTTSP_DELAY_DNLOAD);
//					}
//					else {
//						mdelay(CYTTSP_DELAY_DNLOAD);
//					}
					/* bootloader requires delay after odd block addresses */
					mdelay(100);
					cyttsp_debug("BL DNLD Rec=% 3d Len=% 3d Addr=%04X\n", 
						cyttsp_fw[i].Record, cyttsp_fw[i].Length, 
						cyttsp_fw[i].Address);
					i++;
					if (retval < CYTTSP_OPERATIONAL) {
						cyttsp_debug("BL fail Rec=%3d retval=%d\n",cyttsp_fw[i-1].Record, retval);
						break;
					}
					else {
						/* reset TTSP I2C counter */
						retval = cyttsp_i2c_write_block_data(ts->client,
							CYTTSP_REG_BASE, 
							0, NULL);
						mdelay(10);
						/* set arg2 to non-0 to activate */
						cyttsp_putbl(ts,5, true, false, false);
					}
				}
				if (!(retval < CYTTSP_OPERATIONAL)) {
					while (i < cyttsp_fw_records) {
						retval = i2c_smbus_write_i2c_block_data(ts->client, CYTTSP_REG_BASE, 
							cyttsp_fw[i].Length, cyttsp_fw[i].Block);
						i++;
						tries = 0;
						cyttsp_debug("wait init f=%02X, s=%02X, e=%02X t=%d\n",g_bl_data.bl_file,
							g_bl_data.bl_status, g_bl_data.bl_error, tries);
						do {
							mdelay(1000);
							cyttsp_putbl(ts,6, true, false, false);
						}
						while (g_bl_data.bl_status != 0x10 && 
							g_bl_data.bl_status != 0x11 && 
							tries++ < 10);
						cyttsp_putbl(ts,7, true, false, false);
						if (retval < CYTTSP_OPERATIONAL) {
							break;
						}
					}
				}
			}
		}
	}

	/* Do we need to reset TTSP Device back to bootloader mode?? */
	/*
	*/
	host_reg = CYTTSP_SOFT_RESET_MODE;
	retval = i2c_smbus_write_i2c_block_data(ts->client, CYTTSP_REG_BASE, 
		sizeof(host_reg), &host_reg);
	/* wait for TTSP Device to complete reset back to bootloader */
	/*
	*/
	mdelay(1000);

	/* set arg2 to non-0 to activate */
	retval = cyttsp_putbl(ts, 8, true, true, true);

	return retval;
}
#else 
static int cyttsp_bootload_app(struct cyttsp *ts)
{
	cyttsp_debug("no-load new firmware \n");
	return CYTTSP_OPERATIONAL;
}
#endif /* CYTTSP_INCLUDE_LOAD_FILE */


static int cyttsp_power_on(struct cyttsp *ts)
{
	int retval = CYTTSP_OPERATIONAL;
	u8 host_reg;
	int tries;
	static u8 bl_cmd[] = {
		CYTTSP_BL_FILE0, CYTTSP_BL_CMD, CYTTSP_BL_EXIT,
		CYTTSP_BL_KEY0, CYTTSP_BL_KEY1, CYTTSP_BL_KEY2,
		CYTTSP_BL_KEY3, CYTTSP_BL_KEY4, CYTTSP_BL_KEY5,
		CYTTSP_BL_KEY6, CYTTSP_BL_KEY7};

	cyttsp_debug("Power up \n");

	/* check if the TTSP device has a bootloader installed */
	host_reg = CYTTSP_SOFT_RESET_MODE;
	retval = i2c_smbus_write_i2c_block_data(ts->client, CYTTSP_REG_BASE, 
		sizeof(host_reg), &host_reg);
	tries = 0;
	do {
		mdelay(1000);

		/* set arg2 to non-0 to activate */
		retval = cyttsp_putbl(ts, 1, true, true, true);
		cyttsp_info("BL%d: f=%02X s=%02X err=%02X bl=%02X%02X bld=%02X%02X R=%d\n", \
			101, \
			g_bl_data.bl_file, g_bl_data.bl_status, g_bl_data.bl_error, \
			g_bl_data.blver_hi, g_bl_data.blver_lo, \
			g_bl_data.bld_blver_hi, g_bl_data.bld_blver_lo,
			retval);
		cyttsp_info("BL%d: tver=%02X%02X a_id=%02X%02X aver=%02X%02X\n", \
			102, \
			g_bl_data.ttspver_hi, g_bl_data.ttspver_lo, \
			g_bl_data.appid_hi, g_bl_data.appid_lo, \
			g_bl_data.appver_hi, g_bl_data.appver_lo);
		cyttsp_info("BL%d: c_id=%02X%02X%02X\n", \
			103, \
			g_bl_data.cid_0, g_bl_data.cid_1, g_bl_data.cid_2);
	}
	while (!(retval < CYTTSP_OPERATIONAL) &&
		!GET_BOOTLOADERMODE(g_bl_data.bl_status) && 
		!(g_bl_data.bl_file == CYTTSP_OPERATE_MODE + CYTTSP_LOW_POWER_MODE) &&
		tries++ < 10);

	/* is bootloader missing? */
	if (!(retval < CYTTSP_OPERATIONAL)) {
		cyttsp_xdebug("Retval=%d  Check if bootloader is missing...\n", retval);
		if (!GET_BOOTLOADERMODE(g_bl_data.bl_status)) {
			/* skip all bootloader and sys info and go straight to operational mode */
			if (!(retval < CYTTSP_OPERATIONAL)) {
				cyttsp_xdebug("Bootloader is missing (retval = %d)\n", retval);
				host_reg = CYTTSP_OPERATE_MODE/* + CYTTSP_LOW_POWER_MODE*/;
				retval = i2c_smbus_write_i2c_block_data(ts->client, CYTTSP_REG_BASE, 
					sizeof(host_reg), &host_reg);
				/* wait for TTSP Device to complete switch to Operational mode */
				mdelay(1000);
				goto bypass;
			}
		}
	}


	/* take TTSP out of bootloader mode; go to TrueTouch operational mode */
	if (!(retval < CYTTSP_OPERATIONAL)) {
		cyttsp_xdebug1("exit bootloader; go operational\n");
		retval = i2c_smbus_write_i2c_block_data(ts->client, CYTTSP_REG_BASE, 
			sizeof(bl_cmd), bl_cmd);
		tries = 0;
		do {
			mdelay(1000);
			cyttsp_putbl(ts,4, true, false, false);
			cyttsp_info("BL%d: f=%02X s=%02X err=%02X bl=%02X%02X bld=%02X%02X\n", \
				104, \
				g_bl_data.bl_file, g_bl_data.bl_status, g_bl_data.bl_error, \
				g_bl_data.blver_hi, g_bl_data.blver_lo, \
				g_bl_data.bld_blver_hi, g_bl_data.bld_blver_lo);
		}
		while (GET_BOOTLOADERMODE(g_bl_data.bl_status) && 
			tries++ < 10);
	}



	if (!(retval < CYTTSP_OPERATIONAL) &&
		cyttsp_app_load()) {
		mdelay(1000);
		if (CYTTSP_DIFF(g_bl_data.ttspver_hi, cyttsp_tts_verh())  ||
			CYTTSP_DIFF(g_bl_data.ttspver_lo, cyttsp_tts_verl())  ||
			CYTTSP_DIFF(g_bl_data.appid_hi, cyttsp_app_idh())  ||
			CYTTSP_DIFF(g_bl_data.appid_lo, cyttsp_app_idl())  ||
			CYTTSP_DIFF(g_bl_data.appver_hi, cyttsp_app_verh())  ||
			CYTTSP_DIFF(g_bl_data.appver_lo, cyttsp_app_verl())  ||
			CYTTSP_DIFF(g_bl_data.cid_0, cyttsp_cid_0())  ||
			CYTTSP_DIFF(g_bl_data.cid_1, cyttsp_cid_1())  ||
			CYTTSP_DIFF(g_bl_data.cid_2, cyttsp_cid_2())  ||
			cyttsp_force_fw_load()) {
			cyttsp_debug("blttsp=0x%02X%02X flttsp=0x%02X%02X force=%d\n", \
				g_bl_data.ttspver_hi, g_bl_data.ttspver_lo, \
				cyttsp_tts_verh(), cyttsp_tts_verl(), cyttsp_force_fw_load());
			cyttsp_debug("blappid=0x%02X%02X flappid=0x%02X%02X\n", \
				g_bl_data.appid_hi, g_bl_data.appid_lo, \
				cyttsp_app_idh(), cyttsp_app_idl());
			cyttsp_debug("blappver=0x%02X%02X flappver=0x%02X%02X\n", \
				g_bl_data.appver_hi, g_bl_data.appver_lo, \
				cyttsp_app_verh(), cyttsp_app_verl());
			cyttsp_debug("blcid=0x%02X%02X%02X flcid=0x%02X%02X%02X\n", \
				g_bl_data.cid_0, g_bl_data.cid_1, g_bl_data.cid_2, \
				cyttsp_cid_0(), cyttsp_cid_1(), cyttsp_cid_2());
			/* enter bootloader to load new app into TTSP Device */
			retval = cyttsp_bootload_app(ts);
			/* take TTSP device out of bootloader mode; switch back to TrueTouch operational mode */
			if (!(retval < CYTTSP_OPERATIONAL)) {
				retval = i2c_smbus_write_i2c_block_data(ts->client, CYTTSP_REG_BASE, 
					sizeof(bl_cmd), bl_cmd);
				/* wait for TTSP Device to complete switch to Operational mode */
				mdelay(1000);
			}
		}
	}

bypass:
	/* switch to System Information mode to read versions and set interval registers */
	if (!(retval < CYTTSP_OPERATIONAL)) {
		cyttsp_debug("switch to sysinfo mode \n");
		host_reg = CYTTSP_SYSINFO_MODE;
		retval = i2c_smbus_write_i2c_block_data(ts->client, CYTTSP_REG_BASE, 
			sizeof(host_reg), &host_reg);
		/* wait for TTSP Device to complete switch to SysInfo mode */
		mdelay(1000);
		if (!(retval < CYTTSP_OPERATIONAL)) {
			retval = i2c_smbus_read_i2c_block_data(ts->client, CYTTSP_REG_BASE, 
				sizeof(struct cyttsp_sysinfo_data_t), (u8 *)&g_sysinfo_data);
			cyttsp_debug("SI2: hst_mode=0x%02X mfg_cmd=0x%02X mfg_stat=0x%02X\n", \
				g_sysinfo_data.hst_mode, g_sysinfo_data.mfg_cmd, \
				g_sysinfo_data.mfg_stat);
			cyttsp_debug("SI2: bl_ver=0x%02X%02X\n", \
				g_sysinfo_data.bl_verh, g_sysinfo_data.bl_verl);
			cyttsp_debug("SI2: sysinfo act_int=0x%02X tch_tmout=0x%02X lp_int=0x%02X\n", \
				g_sysinfo_data.act_intrvl, g_sysinfo_data.tch_tmout, \
				g_sysinfo_data.lp_intrvl);
			cyttsp_info("SI%d: tver=%02X%02X a_id=%02X%02X aver=%02X%02X\n", \
				102, \
				g_sysinfo_data.tts_verh, g_sysinfo_data.tts_verl, \
				g_sysinfo_data.app_idh, g_sysinfo_data.app_idl, \
				g_sysinfo_data.app_verh, g_sysinfo_data.app_verl);
			cyttsp_info("SI%d: c_id=%02X%02X%02X\n", \
				103, \
				g_sysinfo_data.cid[0], g_sysinfo_data.cid[1], g_sysinfo_data.cid[2]);
			if (!(retval < CYTTSP_OPERATIONAL) &&
				(CYTTSP_DIFF(ts->platform_data->act_intrvl, CYTTSP_ACT_INTRVL_DFLT)  ||
				CYTTSP_DIFF(ts->platform_data->tch_tmout, CYTTSP_TCH_TMOUT_DFLT) ||
				CYTTSP_DIFF(ts->platform_data->lp_intrvl, CYTTSP_LP_INTRVL_DFLT))) {
				if (!(retval < CYTTSP_OPERATIONAL)) {
					u8 intrvl_ray[sizeof(ts->platform_data->act_intrvl) + 
						sizeof(ts->platform_data->tch_tmout) + 
						sizeof(ts->platform_data->lp_intrvl)];
					u8 i = 0;

					intrvl_ray[i++] = ts->platform_data->act_intrvl;
					intrvl_ray[i++] = ts->platform_data->tch_tmout;
					intrvl_ray[i++] = ts->platform_data->lp_intrvl;

					cyttsp_debug("SI2: platinfo act_intrvl=0x%02X tch_tmout=0x%02X lp_intrvl=0x%02X\n", \
						ts->platform_data->act_intrvl, ts->platform_data->tch_tmout, \
						ts->platform_data->lp_intrvl);
					// set intrvl registers
					retval = i2c_smbus_write_i2c_block_data(ts->client, CYTTSP_REG_ACT_INTRVL, 
						sizeof(intrvl_ray), intrvl_ray);
					mdelay(CYTTSP_DELAY_SYSINFO);
				}
			}
		}
		/* switch back to Operational mode */
		cyttsp_debug("switch back to operational mode \n");
		if (!(retval < CYTTSP_OPERATIONAL)) {
			host_reg = CYTTSP_OPERATE_MODE/* + CYTTSP_LOW_POWER_MODE*/;
			retval = i2c_smbus_write_i2c_block_data(ts->client, CYTTSP_REG_BASE, 
				sizeof(host_reg), &host_reg);
			/* wait for TTSP Device to complete switch to Operational mode */
			mdelay(1000);
		}
	}
	/* init gesture setup; 
	 * this is required even if not using gestures
	 * in order to set the active distance */
	if (!(retval < CYTTSP_OPERATIONAL)) {
		u8 gesture_setup;
		cyttsp_debug("init gesture setup \n");
		gesture_setup = ts->platform_data->gest_set;
		retval = i2c_smbus_write_i2c_block_data(ts->client, CYTTSP_REG_GEST_SET, 
			sizeof(gesture_setup), &gesture_setup);
		mdelay(CYTTSP_DELAY_DFLT);
	}

	if (!(retval < CYTTSP_OPERATIONAL)) {
		ts->platform_data->power_state = CYTTSP_ACTIVE_STATE;
	}
	else {
		ts->platform_data->power_state = CYTTSP_IDLE_STATE;
	}
	cyttsp_debug("Retval=%d Power state is %s\n", retval, (ts->platform_data->power_state == CYTTSP_ACTIVE_STATE) ? "ACTIVE" : "IDLE");

	return retval;
}

/* cyttsp_initialize: Driver Initialization. This function takes
 * care of the following tasks:
 * 1. Create and register an input device with input layer
 * 2. Take CYTTSP device out of bootloader mode; go operational
 * 3. Start any timers/Work queues.  */
static int cyttsp_initialize(struct i2c_client *client, struct cyttsp *ts)
{
	struct input_dev *input_device;
	int error = 0;
	int retval = CYTTSP_OPERATIONAL;
	u8 id;

	/* Create the input device and register it. */
	input_device = input_allocate_device();
	if (!input_device) {
		error = -ENOMEM;
		cyttsp_xdebug1("err input allocate device\n");
		goto error_free_device;
	}

	if (!client) {
		error = ~ENODEV;
		cyttsp_xdebug1("err client is Null\n");
		goto error_free_device;
	}

	if (!ts) {
		error = ~ENODEV;
		cyttsp_xdebug1("err context is Null\n");
		goto error_free_device;
	}

	ts->input = input_device;
	input_device->name = CYTTSP_I2C_NAME;
	input_device->phys = ts->phys;
	input_device->dev.parent = &client->dev;

	/* init the touch structures */
	ts->num_prev_st_touch = CYTTSP_NOTOUCH;
	for (id = 0; id < CYTTSP_NUM_TRACK_ID; id++) {
		ts->active_track[id] = CYTTSP_NOTOUCH;
	}
	for (id = 0; id < CYTTSP_NUM_MT_TOUCH_ID; id++) {
		ts->prev_mt_touch[id] = CYTTSP_IGNORE_TOUCH;
		ts->prev_mt_pos[id][CYTTSP_XPOS] = 0;
		ts->prev_mt_pos[id][CYTTSP_YPOS] = 0;
	}
	for (id = 0; id < CYTTSP_NUM_ST_TOUCH_ID; id++) {
		ts->prev_st_touch[id] = CYTTSP_IGNORE_TOUCH;
	}

	set_bit(EV_SYN, input_device->evbit);
	set_bit(EV_KEY, input_device->evbit);
	set_bit(EV_ABS, input_device->evbit);
	set_bit(BTN_TOUCH, input_device->keybit);
	set_bit(BTN_2, input_device->keybit);
	if (ts->platform_data->use_gestures) {
		set_bit(BTN_3, input_device->keybit);
	}

	input_set_abs_params(input_device, ABS_X, 0, ts->platform_data->maxx, 0, 0);
	input_set_abs_params(input_device, ABS_Y, 0, ts->platform_data->maxy, 0, 0);
	input_set_abs_params(input_device, ABS_TOOL_WIDTH, 0, CYTTSP_LARGE_TOOL_WIDTH, 0 ,0);
	input_set_abs_params(input_device, ABS_PRESSURE, 0, CYTTSP_MAXZ, 0, 0);
	input_set_abs_params(input_device, ABS_HAT0X, 0, ts->platform_data->maxx, 0, 0);
	input_set_abs_params(input_device, ABS_HAT0Y, 0, ts->platform_data->maxy, 0, 0);
	if (ts->platform_data->use_gestures) {
		input_set_abs_params(input_device, ABS_HAT1X, 0, CYTTSP_MAXZ, 0, 0);
		input_set_abs_params(input_device, ABS_HAT1Y, 0, CYTTSP_MAXZ, 0, 0);
	}
	if (ts->platform_data->use_mt) {
		input_set_abs_params(input_device, ABS_MT_POSITION_X, 0, ts->platform_data->maxx, 0, 0);
		input_set_abs_params(input_device, ABS_MT_POSITION_Y, 0, ts->platform_data->maxy, 0, 0);
		input_set_abs_params(input_device, ABS_MT_TOUCH_MAJOR, 0, CYTTSP_MAXZ, 0, 0);
		input_set_abs_params(input_device, ABS_MT_WIDTH_MAJOR, 0, CYTTSP_LARGE_TOOL_WIDTH, 0, 0);
		if (ts->platform_data->use_trk_id) {
			input_set_abs_params(input_device, ABS_MT_TRACKING_ID, 0, CYTTSP_NUM_TRACK_ID, 0, 0);
		}
	}
	cyttsp_info("%s: Register input device\n", CYTTSP_I2C_NAME);

	error = input_register_device(input_device);
	if (error) {
		cyttsp_alert("%s: Failed to register input device\n", CYTTSP_I2C_NAME);
		retval = error;
		goto error_free_device;
	}

	/* Prepare our worker structure prior to setting up the timer/ISR */
	INIT_WORK(&ts->work,cyttsp_xy_worker);

	/* Power on the chip and make sure that I/Os are set as specified
	 * in the platform 
	 */
	 #if 0  //jhlim
	retval = cyttsp_power_on(ts);
	if (retval < 0) {
		goto error_free_device;
	}
	#endif
	
	/* Timer or Interrupt setup */
	if(ts->client->irq == 0) {
		cyttsp_info("Setting up timer\n");
		setup_timer(&ts->timer, cyttsp_timer, (unsigned long) ts);
		mod_timer(&ts->timer, jiffies + TOUCHSCREEN_TIMEOUT);
	}
	else {
		cyttsp_info("Setting up interrupt\n");
		/* request_irq() will also call enable_irq() */
		error = request_irq (client->irq,cyttsp_irq,IRQF_TRIGGER_FALLING,
			client->dev.driver->name,ts);
		if (error) {
			cyttsp_alert("error: could not request irq\n");
			retval = error;
			goto error_free_irq;
		}
	}

	atomic_set(&ts->irq_enabled, 1);
	retval = device_create_file(&ts->client->dev, &dev_attr_irq_enable);
	if (retval < CYTTSP_OPERATIONAL) {
		cyttsp_alert("File device creation failed: %d\n", retval);
		retval = -ENODEV;
		goto error_free_irq;
	}

	cyttsp_info("%s: Successful registration\n", CYTTSP_I2C_NAME);
	goto success;

error_free_irq:
	cyttsp_alert("Error: Failed to register IRQ handler\n");
	free_irq(client->irq,ts);

error_free_device:
	if (input_device) {
		input_free_device(input_device);
	}

success:
	return retval;
}

/* I2C driver probe function */
static int __devinit cyttsp_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct cyttsp *ts;
	int error;
	int retval = CYTTSP_OPERATIONAL;

	cyttsp_info("Start Probe\n");

	/* allocate and clear memory */
	ts = kzalloc (sizeof(struct cyttsp),GFP_KERNEL);
	if (ts == NULL) {
		cyttsp_xdebug1("err kzalloc for cyttsp\n");
		retval = -ENOMEM;
	}

	if (!(retval < CYTTSP_OPERATIONAL)) {
		/* register driver_data */
		ts->client = client;
		ts->platform_data = client->dev.platform_data;
		i2c_set_clientdata(client,ts);

		error = cyttsp_initialize(client, ts);
		if (error) {
			cyttsp_xdebug1("err cyttsp_initialize\n");
			if (ts != NULL) {
				/* deallocate memory */
				kfree(ts);	
			}
/*
			i2c_del_driver(&cyttsp_driver);
*/
			retval = -ENODEV;
		}
		else {
			cyttsp_openlog();
		}
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	if (!(retval < CYTTSP_OPERATIONAL)) {
		ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
		ts->early_suspend.suspend = cyttsp_early_suspend;
		ts->early_suspend.resume = cyttsp_late_resume;
		register_early_suspend(&ts->early_suspend);
	}
#endif /* CONFIG_HAS_EARLYSUSPEND */

	cyttsp_info("Start Probe %s\n", (retval < CYTTSP_OPERATIONAL) ? "FAIL" : "PASS");

	return retval;
}

/* Function to manage power-on resume */
static int cyttsp_resume(struct i2c_client *client)
{
	struct cyttsp *ts;
	u8 wake_mode = CYTTSP_OPERATIONAL;
	int retval = CYTTSP_OPERATIONAL;

	cyttsp_debug("Wake Up\n");

	ts = (struct cyttsp *) i2c_get_clientdata(client);
	if (ts->platform_data->use_sleep && (ts->platform_data->power_state != CYTTSP_ACTIVE_STATE)) {
		retval = i2c_smbus_write_i2c_block_data(ts->client, CYTTSP_REG_BASE,
			sizeof(wake_mode), &wake_mode);
		if (!(retval < CYTTSP_OPERATIONAL)) {
			retval = i2c_smbus_read_i2c_block_data(ts->client, CYTTSP_REG_BASE,
				sizeof(struct cyttsp_wake_data_t), (u8 *)&g_wake_data);
		}
	}

	if (!(retval < CYTTSP_OPERATIONAL) && 
		(GET_HSTMODE(g_wake_data.hst_mode) == CYTTSP_OPERATIONAL)) {
		ts->platform_data->power_state = CYTTSP_ACTIVE_STATE;

		/* re-enable the interrupt after resuming */
		enable_irq(ts->client->irq);
	}
	else {
		retval = -ENODEV;
	}

	cyttsp_debug("Wake Up %s\n", (retval < CYTTSP_OPERATIONAL) ? "FAIL" : "PASS" );

	return retval;
}


/* Function to manage low power suspend */
static int cyttsp_suspend(struct i2c_client *client, pm_message_t message)
{
	struct cyttsp *ts; 
	u8 sleep_mode = CYTTSP_OPERATIONAL;
	int retval = CYTTSP_OPERATIONAL;

	cyttsp_debug("Enter Sleep\n");

	ts = (struct cyttsp *) i2c_get_clientdata(client); 

	/* disable worker */
	disable_irq_nosync(ts->client->irq);
	retval = cancel_work_sync(&ts->work);

	if (!(retval < CYTTSP_OPERATIONAL)) {
		if (ts->platform_data->use_sleep && 
			(ts->platform_data->power_state == CYTTSP_ACTIVE_STATE)) {
			if (ts->platform_data->use_sleep & CYTTSP_USE_DEEP_SLEEP_SEL) {
				sleep_mode = CYTTSP_DEEP_SLEEP_MODE;
			}
			else {
				sleep_mode = CYTTSP_LOW_POWER_MODE;
			}
			retval = i2c_smbus_write_i2c_block_data(ts->client, CYTTSP_REG_BASE, 
				sizeof(sleep_mode), &sleep_mode);
		}
	}

	if (!(retval < CYTTSP_OPERATIONAL)) {
		if (sleep_mode == CYTTSP_DEEP_SLEEP_MODE) {
			ts->platform_data->power_state = CYTTSP_SLEEP_STATE;
		}
		else if (sleep_mode == CYTTSP_LOW_POWER_MODE) {
			ts->platform_data->power_state = CYTTSP_LOW_PWR_STATE;
		}
	}

	cyttsp_debug("Sleep Power state is %s\n", \
		(ts->platform_data->power_state == CYTTSP_ACTIVE_STATE) ? "ACTIVE" : \
		((ts->platform_data->power_state == CYTTSP_SLEEP_STATE) ? "SLEEP" : "LOW POWER"));

	return retval;
}

/* registered in driver struct */
static int __devexit cyttsp_remove(struct i2c_client *client)
{
	struct cyttsp *ts;
	int err;

	cyttsp_alert("Unregister\n");

	/* clientdata registered on probe */
	ts = i2c_get_clientdata(client);
	device_remove_file(&ts->client->dev, &dev_attr_irq_enable);

	/* Start cleaning up by removing any delayed work and the timer */
	if (cancel_delayed_work((struct delayed_work *)&ts->work)<0) {
		cyttsp_alert("error: could not remove work from workqueue\n");
	}

	/* free up timer or irq */
    if(ts->client->irq == 0) {	
		err = del_timer(&ts->timer);
		if (err < 0) {
			cyttsp_alert("error: failed to delete timer\n");
		}
	}
	else {
		free_irq(client->irq,ts);
	}

	/* housekeeping */
	if (ts != NULL) {
		kfree(ts);
	}

	cyttsp_alert("Leaving\n");

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cyttsp_early_suspend(struct early_suspend *handler)
{
	struct cyttsp *ts;

	ts = container_of(handler, struct cyttsp, early_suspend);
	cyttsp_suspend(ts->client, PMSG_SUSPEND);
}

static void cyttsp_late_resume(struct early_suspend *handler)
{
	struct cyttsp *ts;

	ts = container_of(handler, struct cyttsp, early_suspend);
	cyttsp_resume(ts->client);
}
#endif  /* CONFIG_HAS_EARLYSUSPEND */

static int cyttsp_init(void)
{
	int ret;

	cyttsp_info("Cypress TrueTouch(R) Standard Product I2C Touchscreen Driver (Built %s @ %s)\n",__DATE__,__TIME__);
	ret = i2c_add_driver(&cyttsp_driver);

	return ret;
}

static void cyttsp_exit(void)
{
	return i2c_del_driver(&cyttsp_driver);
}

module_init(cyttsp_init);
module_exit(cyttsp_exit);

