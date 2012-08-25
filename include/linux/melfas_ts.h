#ifndef __MELFAS_TS_H__
#define __MELFAS_TS_H__

#include <linux/input.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/delay.h>

#define MELFAS_TTSP_NAME	"melfas_ts"
#define MELFAS_7000_I2C_NAME		"melfas_ts_7000-i2c"
#define MELFAS_8000_I2C_NAME		"melfas_ts_8000-i2c"


//#define DEBUG_MELFAS_TS

#define DEBUG_TOUCH_NOISE

#ifdef CONFIG_MX100_WS
#define ENABLE_TOUCH_KEY_LED  /* JHLIM 2010.12.15 */

//#define ENABLE_TOUCH_NG_DEBUG  /* 2011.04.20  */

#ifdef ENABLE_TOUCH_NG_DEBUG
//#undef ENABLE_TOUCH_KEY_LED
//#define ENABLE_PM_LED_DEBUG  /* JHLIM 2011.04.14 */
#endif


#define ENABLE_TOUCH_PM_POWERONOFF    /* JHLIM 2011.01.28 */
#define ENABLE_TOUCH_SINGLE_WORKQUEUE    /* JHLIM 2011.01.30 */
#endif

/* see kernel.h for pr_xxx def'ns */
#define melfas_ts_info(f, a...)			printk("%s:" f,  __func__ , ## a)
#define melfas_ts_error(f, a...)		printk("%s:" f,  __func__ , ## a)
#define melfas_ts_alert(f, a...)		printk("%s:" f,  __func__ , ## a)

#ifdef DEBUG_MELFAS_TS
#define melfas_ts_debug(f, a...)	printk("%s:" f,  __func__ , ## a)
#else
#define melfas_ts_debug(f, a...)
#endif

//#define	TOUCHSCREEN_TIMEOUT	(msecs_to_jiffies(1000))
#define	TOUCHSCREEN_TIMEOUT	(msecs_to_jiffies(28))

#define REVERSE_X(flags)		((flags) & REVERSE_X_FLAG)
#define REVERSE_Y(flags)		((flags) & REVERSE_Y_FLAG)
#define FLIP_XY(x, y)			{ \
						u16 tmp; \
						tmp = (x); \
						(x) = (y); \
						(y) = tmp; \
					}
#define INVERT_X(x, xmax)		((xmax) - (x))
#define INVERT_Y(y, ymax)		((ymax) - (y))
#define SET_HSTMODE(reg, mode)		((reg) & (mode))
#define GET_HSTMODE(reg)		((reg & 0x70) >> 4)
#define GET_BOOTLOADERMODE(reg)		((reg & 0x10) >> 4)


#define MF_USING_TIMER

// MELFAS register define.

#define MF_DEVICE_STATUS (0)
#define MF_OPERATING_MODE (0x1)
#define MF_SENSITIVITY_CONTROL (0x2)
#define MF_FILTER_LEVEL (0x3)
#define MF_XY_SIZE_UPPER (0xA)
#define MF_X_SIZE_LOWER (0xB)
#define MF_Y_SIZE_LOWER (0xc)
#define MF_INPUT_INFO (0x10)
#define MF_TS_POSITION_ENABLE (0x11)

#define MF_XY_POS0_UPPER (0x12)
#define MF_X_POS0_LOWER (0x13)
#define MF_Y_POS0_LOWER (0x14)

#define MF_XY_POS1_UPPER (0x15)
#define MF_X_POS1_LOWER (0x16)
#define MF_Y_POS1_LOWER (0x17)

#define MF_XY_POS2_UPPER (0x18)
#define MF_X_POS2_LOWER (0x19)
#define MF_Y_POS2_LOWER (0x1A)

#define MF_XY_POS3_UPPER (0x1B)
#define MF_X_POS3_LOWER (0x1C)
#define MF_Y_POS3_LOWER (0x1D)

#define MF_XY_POS4_UPPER (0x1E)
#define MF_X_POS4_LOWER (0x1F)
#define MF_Y_POS4_LOWER (0x20)


#define MF_Z_POS0_LOWER (0x21)
#define MF_Z_POS1_LOWER (0x22)
#define MF_Z_POS2_LOWER (0x23)
#define MF_Z_POS3_LOWER (0x24)
#define MF_Z_POS4_LOWER (0x25)

#define MF_TSP_REV	(0x40)
#define MF_HW_REV	 (0x41)
#define MF_COMP_GROUP (0x42)
#define MF_FW_VER (0x43)



#define USE_MX100_LCD_RES  // 480 * 800 => 600 * 1024 ·Î »ç¿ë.

#define TOUCH_KEY_ENABLE

//#define USE_8000_TOUCH

#endif
