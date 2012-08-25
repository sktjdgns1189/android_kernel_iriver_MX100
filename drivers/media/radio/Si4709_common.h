#ifndef _COMMON_H
#define _COMMON_H

#include <linux/kernel.h>
#include <linux/types.h>

#define IRIVER_RADIO
#define IRIVER_FM_TEST          /* ktj, fm test, use only for testing */
//#define SI4702_VOLUME           /* ktj, use si4702 internal volume *//*Shanghai ewada disable 20110124*/
 					            /*  JHLIM enable for fm volume tuning. 2011.02.18 */

#define SI4702_VOLUME_CODEC  /* JHLIM fm volume tuning.   */

#define IRIVER_FM_MODIFY_CLK    /* ljh */

#undef  USE_FM_IRQ

//#define Si4709_DEBUG        /* enable debug message */

#define error(fmt,arg...) printk(KERN_CRIT fmt "\n",## arg)

#ifdef Si4709_DEBUG
#define debug(fmt,arg...) printk(KERN_CRIT "___[si4709] " fmt "\n",## arg)
#else
#define debug(fmt,arg...)
#endif

#define GPIO_FM_RST         S5PV210_GPJ1(2)  
#define FM_RESET			GPIO_FM_RST
#define GPIO_LEVEL_HIGH     1
#define GPIO_LEVEL_LOW      0

extern void s3c_rtc_clkout(int en);

/*VNVS:28-OCT'09---- For testing FM tune and seek operation status*/
#define TEST_FM   

#define YES  1
#define NO  0

#endif

