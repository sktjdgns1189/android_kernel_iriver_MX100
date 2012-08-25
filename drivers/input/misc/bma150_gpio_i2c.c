#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <linux/irq.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/wait.h>
#include <linux/stat.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio.h>
/* ktj add for 2.6.32 */
#include <mach/gpio-bank.h>
#include <mach/regs-gpio.h>

//[*]----------------------------------------------------------------------------------------------[*]
#include "bma150_gpio_i2c.h"

//[*]----------------------------------------------------------------------------------------------[*]
#define GPIO_I2C_SDA            S5PV210_GPJ2(6)  
#define GPIO_I2C_SCL		    S5PV210_GPJ2(7)  
#define	GPIO_SDA_PIN		    6
#define	GPIO_CLK_PIN		    7
#define GPIO_LEVEL_HIGH         1
#define GPIO_LEVEL_LOW          0

//[*]----------------------------------------------------------------------------------------------[*]
#define	GPIO_CON_INPUT			0x0
#define	GPIO_CON_OUTPUT			0x1

#define	DELAY_TIME				10	// us value
#define	PORT_CHANGE_DELAY_TIME	10

//[*]----------------------------------------------------------------------------------------------[*]
#define	HIGH					1
#define	LOW						0

//[*]----------------------------------------------------------------------------------------------[*]
//#define	DEBUG_GPIO_I2C			// debug enable flag
#define	DEBUG_MSG(x)			printk(x)

//[*]----------------------------------------------------------------------------------------------[*]
//	static function prototype
//[*]----------------------------------------------------------------------------------------------[*]
static	void			gpio_i2c_sda_port_control(unsigned char inout);
static	void			gpio_i2c_clk_port_control(unsigned char inout);

static	unsigned char	gpio_i2c_get_sda			(void);
static	void			gpio_i2c_set_sda			(unsigned char hi_lo);
static	void			gpio_i2c_set_clk			(unsigned char hi_lo);
                                        	
static	void			gpio_i2c_start			(void);
static	void			gpio_i2c_stop			(void);
                                        	
static	void			gpio_i2c_send_ack		(void);
static	void			gpio_i2c_send_noack		(void);
static	unsigned char	gpio_i2c_chk_ack			(void);
                		                        	
static	void 			gpio_i2c_byte_write		(unsigned char wdata);
static	void			gpio_i2c_byte_read		(unsigned char *rdata);
		        		
//[*]----------------------------------------------------------------------------------------------[*]
//	extern function prototype
//[*]----------------------------------------------------------------------------------------------[*]
int 					sensor_read				(unsigned char st_reg, unsigned char *rdata, unsigned char rsize);
int 					sensor_write				(unsigned char reg, unsigned char data);
void 					sensor_port_init			(void);

//[*]----------------------------------------------------------------------------------------------[*]
static	void			gpio_i2c_sda_port_control	(unsigned char inout)
{
	if (inout)
	    s3c_gpio_cfgpin(GPIO_I2C_SDA, S3C_GPIO_SFN(1));
	else
	    s3c_gpio_cfgpin(GPIO_I2C_SDA, S3C_GPIO_SFN(0));
}

//[*]----------------------------------------------------------------------------------------------[*]
static	void			gpio_i2c_clk_port_control	(unsigned char inout)
{
	if (inout)
	    s3c_gpio_cfgpin(GPIO_I2C_SCL, S3C_GPIO_SFN(1));
	else
	    s3c_gpio_cfgpin(GPIO_I2C_SCL, S3C_GPIO_SFN(0));
}

//[*]----------------------------------------------------------------------------------------------[*]
static	unsigned char	gpio_i2c_get_sda		(void)
{
    unsigned int tmp;

	tmp = __raw_readl(S5PV210_GPJ2DAT);    

    return tmp & (1 << GPIO_SDA_PIN) ? 1 : 0;
//	return	GPIO_I2C_SDA_DAT_PORT & (HIGH << GPIO_SDA_PIN) ? 1 : 0;
}

//[*]----------------------------------------------------------------------------------------------[*]
static	void			gpio_i2c_set_sda		(unsigned char hi_lo)
{
#if 1
    unsigned int tmp;

	tmp = __raw_readl(S5PV210_GPJ2DAT);
    
    if(hi_lo)
        tmp |= 1 << GPIO_SDA_PIN;
    else
        tmp &= ~(1 << GPIO_SDA_PIN);
    
	__raw_writel(tmp,S5PV210_GPJ2DAT);

	gpio_i2c_sda_port_control(GPIO_CON_OUTPUT);
	udelay(PORT_CHANGE_DELAY_TIME);
#else
	if(hi_lo)	{
		gpio_i2c_sda_port_control(GPIO_CON_INPUT);
		udelay(PORT_CHANGE_DELAY_TIME);
	}
	else		{
		__raw_writel(tmp,S5PV210_GPJ2DAT);
		gpio_i2c_sda_port_control(GPIO_CON_OUTPUT);
		udelay(PORT_CHANGE_DELAY_TIME);
	}
#endif
}

//[*]----------------------------------------------------------------------------------------------[*]
static	void			gpio_i2c_set_clk		(unsigned char hi_lo)
{
#if 1
    unsigned int tmp;

	tmp = __raw_readl(S5PV210_GPJ2DAT);
    
    if(hi_lo)
        tmp |= 1 << GPIO_CLK_PIN;
    else
        tmp &= ~(1 << GPIO_CLK_PIN);
    
	__raw_writel(tmp,S5PV210_GPJ2DAT);

	gpio_i2c_clk_port_control(GPIO_CON_OUTPUT);
	udelay(PORT_CHANGE_DELAY_TIME);
#else
	if(hi_lo)	{
		gpio_i2c_clk_port_control(GPIO_CON_INPUT);
		udelay(PORT_CHANGE_DELAY_TIME);
	}
	else		{
		GPIO_I2C_CLK_DAT_PORT &= ~(HIGH << GPIO_CLK_PIN);
		gpio_i2c_clk_port_control(GPIO_CON_OUTPUT);
		udelay(PORT_CHANGE_DELAY_TIME);
	}
#endif
}

//[*]----------------------------------------------------------------------------------------------[*]
static	void			gpio_i2c_start			(void)
{
	// Setup SDA, CLK output High
	gpio_i2c_set_sda(HIGH);
	gpio_i2c_set_clk(HIGH);
	
	udelay(DELAY_TIME);

	// SDA low before CLK low
	gpio_i2c_set_sda(LOW);	udelay(DELAY_TIME);
	gpio_i2c_set_clk(LOW);	udelay(DELAY_TIME);
}

//[*]----------------------------------------------------------------------------------------------[*]
static	void			gpio_i2c_stop			(void)
{
	// Setup SDA, CLK output low
	gpio_i2c_set_sda(LOW);
	gpio_i2c_set_clk(LOW);
	
	udelay(DELAY_TIME);
	
	// SDA high after CLK high
	gpio_i2c_set_clk(HIGH);	udelay(DELAY_TIME);
	gpio_i2c_set_sda(HIGH);	udelay(DELAY_TIME);
}

//[*]----------------------------------------------------------------------------------------------[*]
static	void			gpio_i2c_send_ack		(void)
{
	// SDA Low
	gpio_i2c_set_sda(LOW);	udelay(DELAY_TIME);
	gpio_i2c_set_clk(HIGH);	udelay(DELAY_TIME);
	gpio_i2c_set_clk(LOW);	udelay(DELAY_TIME);
}

//[*]----------------------------------------------------------------------------------------------[*]
static	void			gpio_i2c_send_noack		(void)
{
	// SDA High
	gpio_i2c_set_sda(HIGH);	udelay(DELAY_TIME);
	gpio_i2c_set_clk(HIGH);	udelay(DELAY_TIME);
	gpio_i2c_set_clk(LOW);	udelay(DELAY_TIME);
}

//[*]----------------------------------------------------------------------------------------------[*]
static	unsigned char	gpio_i2c_chk_ack		(void)
{
	unsigned char	count = 0, ret = 0;

	gpio_i2c_set_sda(LOW);		udelay(DELAY_TIME);
	gpio_i2c_set_clk(HIGH);		udelay(DELAY_TIME);

	gpio_i2c_sda_port_control(GPIO_CON_INPUT);
	udelay(PORT_CHANGE_DELAY_TIME);

	while(gpio_i2c_get_sda())	{
		if(count++ > 100)	{	ret = 1;	break;	}
		else					udelay(DELAY_TIME);	
	}

	gpio_i2c_set_clk(LOW);		udelay(DELAY_TIME);
	
	#if defined(DEBUG_GPIO_I2C)
		if(ret)		DEBUG_MSG(("%s (%d): no ack!!\n",__FUNCTION__, ret));
		else		DEBUG_MSG(("%s (%d): ack !! \n" ,__FUNCTION__, ret));
	#endif

	return	ret;
}

//[*]----------------------------------------------------------------------------------------------[*]
static	void 			gpio_i2c_byte_write		(unsigned char wdata)
{
	unsigned char	cnt, mask;
	
	for(cnt = 0, mask = 0x80; cnt < 8; cnt++, mask >>= 1)	{
		if(wdata & mask)		gpio_i2c_set_sda(HIGH);
		else					gpio_i2c_set_sda(LOW);
			
		gpio_i2c_set_clk(HIGH);		udelay(DELAY_TIME);
		gpio_i2c_set_clk(LOW);		udelay(DELAY_TIME);
	}
}

//[*]----------------------------------------------------------------------------------------------[*]
static	void			gpio_i2c_byte_read		(unsigned char *rdata)
{
	unsigned char	cnt, mask;

	gpio_i2c_sda_port_control(GPIO_CON_INPUT);
	udelay(PORT_CHANGE_DELAY_TIME);

	for(cnt = 0, mask = 0x80, *rdata = 0; cnt < 8; cnt++, mask >>= 1)	{
		gpio_i2c_set_clk(HIGH);		udelay(DELAY_TIME);
		
		if(gpio_i2c_get_sda())		*rdata |= mask;
		
		gpio_i2c_set_clk(LOW);		udelay(DELAY_TIME);
		
	}
}

//[*]----------------------------------------------------------------------------------------------[*]
int 			bma150_sensor_write			(unsigned char reg, unsigned char data)
{
	unsigned char ack;

	// start
	gpio_i2c_start();
	
	gpio_i2c_byte_write(BMA150_SENSOR_WR_ADDR);	// i2c address

	if((ack = gpio_i2c_chk_ack()))	{
		#if defined(DEBUG_GPIO_I2C)
			DEBUG_MSG(("%s [write address] : no ack\n",__FUNCTION__));
		#endif

		goto	write_stop;
	}
	
	gpio_i2c_byte_write(reg);	// register
	
	if((ack = gpio_i2c_chk_ack()))	{
		#if defined(DEBUG_GPIO_I2C)
			DEBUG_MSG(("%s [write register] : no ack\n",__FUNCTION__));
		#endif
	}

	gpio_i2c_byte_write(data);	// data
	
	if((ack = gpio_i2c_chk_ack()))	{
		#if defined(DEBUG_GPIO_I2C)
			DEBUG_MSG(("%s [write data] : no ack\n",__FUNCTION__));
		#endif
	}

write_stop:
	gpio_i2c_stop();

	#if defined(DEBUG_GPIO_I2C)
		DEBUG_MSG(("%s : %d\n", __FUNCTION__, ack));
	#endif

	return	ack;
}

//[*]--------------------------------------------------------------------------------------------------[*]
int 			bma150_sensor_read			(unsigned char st_reg, unsigned char *rdata, unsigned char rsize)
{
	unsigned char ack, cnt;

	// start
	gpio_i2c_start();

	gpio_i2c_byte_write(BMA150_SENSOR_WR_ADDR);	// i2c address

	if((ack = gpio_i2c_chk_ack()))	{
		#if defined(DEBUG_GPIO_I2C)
			DEBUG_MSG(("%s [write address] : no ack\n",__FUNCTION__));
		#endif

		goto	read_stop;
	}
	
	gpio_i2c_byte_write(st_reg);	// start register

	if((ack = gpio_i2c_chk_ack()))	{
		#if defined(DEBUG_GPIO_I2C)
			DEBUG_MSG(("%s [write address] : no ack\n",__FUNCTION__));
		#endif

		goto	read_stop;
	}
	
	gpio_i2c_stop();
	
	// re-start
	gpio_i2c_start();

	gpio_i2c_byte_write(BMA150_SENSOR_RD_ADDR);	// i2c address

	if((ack = gpio_i2c_chk_ack()))	{
		#if defined(DEBUG_GPIO_I2C)
			DEBUG_MSG(("%s [write address] : no ack\n",__FUNCTION__));
		#endif

		goto	read_stop;
	}

	for(cnt=0; cnt < rsize; cnt++)	{
		
		gpio_i2c_byte_read(&rdata[cnt]);
		
		if(cnt == rsize -1)		gpio_i2c_send_noack();
		else					gpio_i2c_send_ack();
	}
	
read_stop:
	gpio_i2c_stop();

	#if defined(DEBUG_GPIO_I2C)
		DEBUG_MSG(("%s : %d\n", __FUNCTION__, ack));
	#endif

	return	ack;
}

//[*]----------------------------------------------------------------------------------------------[*]
void 			bma150_sensor_port_init			(void)
{
    /* SCL, SDA ports are must be pulled up, ktj */
    
    s3c_gpio_cfgpin(GPIO_I2C_SDA, S3C_GPIO_SFN(1));
    s3c_gpio_setpull(GPIO_I2C_SDA, S3C_GPIO_PULL_UP);

    s3c_gpio_cfgpin(GPIO_I2C_SCL, S3C_GPIO_SFN(1));
    s3c_gpio_setpull(GPIO_I2C_SCL, S3C_GPIO_PULL_UP);

    gpio_i2c_clk_port_control(GPIO_CON_OUTPUT);
    gpio_i2c_sda_port_control(GPIO_CON_OUTPUT);
	
	gpio_i2c_set_sda(HIGH);	gpio_i2c_set_clk(HIGH);
}

//[*]----------------------------------------------------------------------------------------------[*]



