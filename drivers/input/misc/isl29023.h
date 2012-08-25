/*
 * drivers/input/misc/isl29023.h
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

#ifndef _LINUX_ISL29023_H__
#define _LINUX_ISL29023_H__

#include <linux/types.h>
/*
#define	MANUAL		    0
#define	AUTOMATIC	    1
#define	MANUAL_SENSOR	2
*/

#define LD_ISL29023_NAME        "isl29023_als_ir"
#define FOPS_ISL29023_NAME      "isl29023"

#define LD_ISL29023_ALLOWED_R_BYTES     1
#define LD_ISL29023_ALLOWED_W_BYTES     2
#define LD_ISL29023_MAX_RW_RETRIES      5
#define LD_ISL29023_I2C_RETRY_DELAY     10

#define ISL29023_COMMAND_I	    0x00
#define ISL29023_COMMAND_II	    0x01
#define ISL29023_ALSIR_DT_LSB	0x02
#define ISL29023_ALSIR_DT_MSB	0x03
#define ISL29023_ALSIR_LT_LSB   0x04
#define ISL29023_ALSIR_LT_MSB   0x05
#define ISL29023_ALSIR_HT_LSB   0x06
#define ISL29023_ALSIR_HT_MSB   0x07

//#define ISL29023_HIGH_LUX_RANGE	2000
//#define ISL29023_LOW_LUX_RANGE	125

#define ISL29023_OPERATION_MODE_ALS_ONCE		0x20    
#define ISL29023_OPERATION_MODE_IR_ONCE		    0x40    
#define ISL29023_OPERATION_MODE_ALS_CONTINUE	0xA0    
#define ISL29023_OPERATION_MODE_IR_CONTINUE	    0xC0    

#define ISL29023_INT_PERSIST_1	0x00    
#define ISL29023_INT_PERSIST_4	0x01    
#define ISL29023_INT_PERSIST_8	0x02    
#define ISL29023_INT_PERSIST_16	0x03    


#define ISL29023_RESOLUTION_16BIT	    0x00
#define ISL29023_RESOLUTION_12BIT	    0x40
#define ISL29023_RESOLUTION_8BIT	    0x80
#define ISL29023_RESOLUTION_4BIT	    0xC0

#define ISL29023_LUX_RANGE1	    0x00
#define ISL29023_LUX_RANGE2	    0x01
#define ISL29023_LUX_RANGE3	    0x02
#define ISL29023_LUX_RANGE4	    0x03

#define ISL29023_ALS_FLAG_MASK		0x04

/*
#ifdef __KERNEL__
struct isl29023_platform_data {
	int  (*init)(void);
	void (*exit)(void);
	int  (*power_on)(void);
	int  (*power_off)(void);

	u8	command1;
	u8	command2;
	u16	als_lower_threshold;
	u16	als_higher_threshold;
	u32	lens_percent_t;
	u16	irq;
} __attribute__ ((packed));
#endif

#define ISL29023_IO			0xA3
#define ISL29023_IOCTL_GET_ENABLE	    _IOR(ISL29023_IO, 0x00, char)
#define ISL29023_IOCTL_SET_ENABLE	    _IOW(ISL29023_IO, 0x01, char)
#define ISL29023_IOCTL_GET_INT_LINE     _IOR(ISL29023_IO, 0x02, char)
*/

#define ISL29023_IOCTL_MAGIC 'l'
 #define ISL29023_IOCTL_GET_ENABLED \
                 _IOR(ISL29023_IOCTL_MAGIC, 1, int *)
 #define ISL29023_IOCTL_ENABLE \
                 _IOW(ISL29023_IOCTL_MAGIC, 2, int *)

#endif	/* _LINUX_ISL29030_H__ */
