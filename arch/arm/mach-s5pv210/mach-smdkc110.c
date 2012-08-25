/* linux/arch/arm/mach-s5pv210/mach-smdkc110.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/gpio.h>
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/regulator/max8698.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/usb/ch9.h>
#include <linux/pwm_backlight.h>
#include <linux/spi/spi.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/regs-mem.h>
#include <mach/regs-gpio.h>
#include <mach/gpio-bank.h>
#include <mach/ts.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <mach/adc.h>

#include <media/s5k3ba_platform.h>
#include <media/s5k4ba_platform.h>
#include <media/s5k4ea_platform.h>
#include <media/s5k6aa_platform.h>

#include <plat/regs-serial.h>
#include <plat/s5pv210.h>
#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/fb.h>
#include <plat/gpio-cfg.h>
#include <plat/iic.h>
#include <plat/spi.h>
#include <plat/fimc.h>
#include <plat/jpeg.h>
#include <plat/csis.h>
#include <plat/mfc.h>
#include <plat/sdhci.h>
#include <plat/ide.h>
#include <plat/regs-otg.h>
#include <plat/clock.h>
#include <mach/gpio-bank-b.h>
#ifdef CONFIG_ANDROID_PMEM
#include <linux/android_pmem.h>
#include <plat/media.h>
#endif

#if defined(CONFIG_PM)
#include <plat/pm.h>
#endif
#ifdef CONFIG_TOUCHSCREEN_CYPRESS
#include <linux/cyttsp.h>
#endif

#ifdef CONFIG_TOUCHSCREEN_MELFAS
#include <linux/melfas_ts.h>
#endif

#include <linux/i2c-gpio.h>     /* ktj */
#include <mach/system.h>        /* ktj */

#if defined(CONFIG_BATTERY_MAX17040)    /* ktj */
#include <linux/max17040_battery.h>
#endif

#ifdef CONFIG_BT
#include <mach/bcm_bt.h>
#endif

/* JHL
in order to use touch device create (under working)
*/
struct class *iriver_class;
EXPORT_SYMBOL(iriver_class);


#ifdef CONFIG_BCM4329_MODULE
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#define GPIO_WLAN_SDIO_CLK      S5PV210_GPG3(0)
#define GPIO_WLAN_SDIO_CMD      S5PV210_GPG3(1)
#define GPIO_WLAN_SDIO_D0       S5PV210_GPG3(3)
#define GPIO_WLAN_SDIO_D1       S5PV210_GPG3(4)
#define GPIO_WLAN_SDIO_D2       S5PV210_GPG3(5)
#define GPIO_WLAN_SDIO_D3       S5PV210_GPG3(6)

#define GPIO_WLAN_WAKE		S5PV210_GPJ0(0)
#define GPIO_WLAN_HOST_WAKE	S5PV210_GPH3(2)
#define GPIO_WLAN_nRST		S5PV210_GPJ0(5)
#define GPIO_BT_nRST		S5PV210_GPJ0(6)

#define GPIO_WLAN_BT_EN		S5PV210_GPJ2(3)

#ifdef CONFIG_MX100_REV_ES
#define GPIO_WLAN_BT_CLK_EN	S5PV210_GPJ0(1)
#endif

#define PREALLOC_WLAN_SEC_NUM		4
#define PREALLOC_WLAN_BUF_NUM		160
#define PREALLOC_WLAN_SECTION_HEADER	24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)

#define WLAN_SKB_BUF_NUM	16

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

struct wifi_mem_prealloc {
	void *mem_ptr;
	unsigned long size;
};

static unsigned int wlan_gpio_table[][4] = {	
	{GPIO_WLAN_nRST, 	    0x1, 	GPIO_LEVEL_LOW, 	S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_HOST_WAKE, 	0xF, 	GPIO_LEVEL_NONE, 	S3C_GPIO_PULL_DOWN},
	{GPIO_WLAN_WAKE, 	    0x1, 	GPIO_LEVEL_LOW, 	S3C_GPIO_PULL_NONE},
#ifdef CONFIG_MX100_REV_ES
	{GPIO_WLAN_BT_CLK_EN, 	0x1, 	GPIO_LEVEL_LOW, 	S3C_GPIO_PULL_NONE},
#endif
};

static unsigned int wlan_sdio_on_table[][4] = {
	{GPIO_WLAN_SDIO_CLK, 	0x2, 	GPIO_LEVEL_NONE, 	S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_CMD, 	0x2, 	GPIO_LEVEL_NONE, 	S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D0, 	0x2, 	GPIO_LEVEL_NONE, 	S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D1, 	0x2, 	GPIO_LEVEL_NONE, 	S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D2, 	0x2, 	GPIO_LEVEL_NONE, 	S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D3, 	0x2, 	GPIO_LEVEL_NONE, 	S3C_GPIO_PULL_NONE},
};

static unsigned int wlan_sdio_off_table[][4] = {
	{GPIO_WLAN_SDIO_CLK, 	0x1, 	GPIO_LEVEL_LOW, 	S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_CMD, 	0x0, 	GPIO_LEVEL_NONE, 	S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D0, 	0x0, 	GPIO_LEVEL_NONE, 	S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D1, 	0x0, 	GPIO_LEVEL_NONE, 	S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D2, 	0x0, 	GPIO_LEVEL_NONE, 	S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D3, 	0x0, 	GPIO_LEVEL_NONE, 	S3C_GPIO_PULL_NONE},
};

#if 1 /* ktj_wifi */
void s3c_config_gpio_alive_table(int array_size, int (*gpio_table)[4])
{
	u32 i, gpio;

	for (i = 0; i < array_size; i++) {
		gpio = gpio_table[i][0];
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(gpio_table[i][1]));
		s3c_gpio_setpull(gpio, gpio_table[i][3]);
		if (gpio_table[i][2] != GPIO_LEVEL_NONE)
			gpio_set_value(gpio, gpio_table[i][2]);
	}
}

static int wlan_power_en(int onoff)
{
//  printk("____[wifi] wlan_power_en = %d \n", onoff);

	if (onoff) {
		s3c_config_gpio_alive_table(ARRAY_SIZE(wlan_gpio_table), wlan_gpio_table);

    #ifdef CONFIG_MX100_REV_ES
        gpio_set_value(GPIO_WLAN_BT_CLK_EN, GPIO_LEVEL_HIGH);
		s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_CLK_EN, S3C_GPIO_SLP_OUT1);
    #endif

		gpio_set_value(GPIO_WLAN_BT_EN, GPIO_LEVEL_HIGH);
		s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_SLP_OUT1);
		
		msleep(100);
		
		gpio_set_value(GPIO_WLAN_nRST, GPIO_LEVEL_HIGH);
		s3c_gpio_slp_cfgpin(GPIO_WLAN_nRST, S3C_GPIO_SLP_OUT1);
	} 
	else 
	{
		if (gpio_get_value(GPIO_BT_nRST) == 0) {
		    gpio_set_value(GPIO_WLAN_BT_EN, GPIO_LEVEL_LOW);
			s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_SLP_OUT0);

        #ifdef CONFIG_MX100_REV_ES
            gpio_set_value(GPIO_WLAN_BT_CLK_EN, GPIO_LEVEL_LOW);
			s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_CLK_EN, S3C_GPIO_SLP_OUT0);
        #endif
		}

		gpio_set_value(GPIO_WLAN_nRST, GPIO_LEVEL_LOW);
		s3c_gpio_slp_cfgpin(GPIO_WLAN_nRST, S3C_GPIO_SLP_OUT0);
	}
	
	return 0;
}
#else
static int wlan_power_en(int onoff)
{
	if (onoff) {
		s3c_gpio_cfgpin(GPIO_WLAN_HOST_WAKE, S3C_GPIO_SFN(0xF));
		s3c_gpio_setpull(GPIO_WLAN_HOST_WAKE, S3C_GPIO_PULL_DOWN);

#ifdef CONFIG_MX100_REV_ES
                s3c_gpio_cfgpin(GPIO_WLAN_BT_CLK_EN, S3C_GPIO_OUTPUT);
                s3c_gpio_setpull(GPIO_WLAN_BT_CLK_EN, S3C_GPIO_PULL_NONE);
                gpio_set_value(GPIO_WLAN_BT_CLK_EN, GPIO_LEVEL_HIGH);
#endif
		s3c_gpio_cfgpin(GPIO_WLAN_nRST, S3C_GPIO_SFN(0x1));
		s3c_gpio_setpull(GPIO_WLAN_nRST, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_WLAN_nRST, GPIO_LEVEL_LOW);	
	
		s3c_gpio_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_WLAN_BT_EN, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_WLAN_BT_EN, GPIO_LEVEL_HIGH);
		msleep(100);
		gpio_set_value(GPIO_WLAN_nRST, GPIO_LEVEL_HIGH);
	} else {
		gpio_set_value(GPIO_WLAN_nRST, GPIO_LEVEL_LOW);
	}
	return 0;
}
#endif

static int wlan_reset_en(int onoff)
{
	gpio_set_value(GPIO_WLAN_nRST,
			onoff ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);
	return 0;
}

static int wlan_carddetect_en(int onoff)
{
	u32 i;
	u32 sdio;

	if (onoff) {
		for (i = 0; i < ARRAY_SIZE(wlan_sdio_on_table); i++) {
			sdio = wlan_sdio_on_table[i][0];
			s3c_gpio_cfgpin(sdio,
					S3C_GPIO_SFN(wlan_sdio_on_table[i][1]));
			s3c_gpio_setpull(sdio, wlan_sdio_on_table[i][3]);
			if (wlan_sdio_on_table[i][2] != GPIO_LEVEL_NONE)
				gpio_set_value(sdio, wlan_sdio_on_table[i][2]);
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(wlan_sdio_off_table); i++) {
			sdio = wlan_sdio_off_table[i][0];
			s3c_gpio_cfgpin(sdio,
				S3C_GPIO_SFN(wlan_sdio_off_table[i][1]));
			s3c_gpio_setpull(sdio, wlan_sdio_off_table[i][3]);
			if (wlan_sdio_off_table[i][2] != GPIO_LEVEL_NONE)
				gpio_set_value(sdio, wlan_sdio_off_table[i][2]);
		}
	}
	udelay(5);

	sdhci_s3c_force_presence_change(&s3c_device_hsmmc3);
	return 0;
}

static struct resource wifi_resources[] = {
	[0] = {
		.name	= "bcm4329_wlan_irq",
		.start	= IRQ_EINT(26),
		.end	= IRQ_EINT(26),
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
	},
};

static struct wifi_mem_prealloc wifi_mem_array[PREALLOC_WLAN_SEC_NUM] = {
	{NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER)}
};

static void *mx100_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_SEC_NUM)
		return wlan_static_skb;

	if ((section < 0) || (section > PREALLOC_WLAN_SEC_NUM))
		return NULL;

	if (wifi_mem_array[section].size < size)
		return NULL;

	return wifi_mem_array[section].mem_ptr;
}

static int __init mx100_init_wifi_mem(void)
{
	int i;
	int j;

	for (i = 0 ; i < WLAN_SKB_BUF_NUM ; i++) {
		wlan_static_skb[i] = dev_alloc_skb(
				((i < (WLAN_SKB_BUF_NUM / 2)) ? 4096 : 8192));

		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	for (i = 0 ; i < PREALLOC_WLAN_SEC_NUM ; i++) {
		wifi_mem_array[i].mem_ptr =
				kmalloc(wifi_mem_array[i].size, GFP_KERNEL);

		if (!wifi_mem_array[i].mem_ptr)
			goto err_mem_alloc;
	}
	return 0;

 err_mem_alloc:
	pr_err("Failed to mem_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		kfree(wifi_mem_array[j].mem_ptr);

	i = WLAN_SKB_BUF_NUM;

 err_skb_alloc:
	pr_err("Failed to skb_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		dev_kfree_skb(wlan_static_skb[j]);

	return -ENOMEM;
}

static struct wifi_platform_data wifi_pdata = {
	.set_power		= wlan_power_en,
	.set_reset		= wlan_reset_en,
	.set_carddetect		= wlan_carddetect_en,
	.mem_prealloc		= mx100_mem_prealloc, 
};

static struct platform_device sec_device_wifi = {
	.name			= "bcm4329_wlan",
	.id			= 1,
	.num_resources		= ARRAY_SIZE(wifi_resources),
	.resource		= wifi_resources,
	.dev			= {
		.platform_data = &wifi_pdata,
	},
};
int wifi_set_power(int on, unsigned long msec)
{
	wlan_power_en(on);
	wlan_carddetect_en(on);
}
EXPORT_SYMBOL(wifi_set_power);
#endif /* CONFIG_BCM4329_MODULE */

/* Following are default values for UCON, ULCON and UFCON UART registers */
#define S5PV210_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define S5PV210_ULCON_DEFAULT	S3C2410_LCON_CS8

#define S5PV210_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)

extern void s5pv210_reserve_bootmem(void);
extern void s3c_sdhci_set_platdata(void);

extern void cdma_uart_wake_peer(struct uart_port *port);
static struct s3c2410_uartcfg smdkv210_uartcfgs[] __initdata = {
	[0] = {
		.hwport		= 0,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
		.wake_peer	= bcm_bt_uart_wake_peer,
	},
	[1] = {
		.hwport		= 1,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
	},
	[2] = {
		.hwport		= 2,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
	},
	[3] = {
		.hwport		= 3,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
		//.wake_peer	= cdma_uart_wake_peer,
	},
};

/* PMIC */
#ifdef CONFIG_REGULATOR_MAX8698
static struct regulator_consumer_supply buck1_consumers[] = {
	{
		.supply		= "vddarm",
	},
};

static struct regulator_init_data max8698_buck1_data = {
	.constraints	= {
		.name		= "VCC_ARM",
		.min_uV		=  750000,
		.max_uV		= 1500000,
		.always_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE,
		.state_mem	= {
			.uV	= 1250000,
			.mode	= REGULATOR_MODE_STANDBY,
			.enabled = 0,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(buck1_consumers),
	.consumer_supplies	= buck1_consumers,
};

static struct regulator_consumer_supply buck2_consumers[] = {
	{
		.supply		= "vddint",
	},
};

static struct regulator_init_data max8698_buck2_data = {
	.constraints	= {
		.name		= "VCC_INTERNAL",
		.min_uV		= 950000,
		.max_uV		= 1200000,
		.always_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE,
		.state_mem	= {
			.uV	= 1100000,
			.mode	= REGULATOR_MODE_NORMAL,
			.enabled = 0,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(buck2_consumers),
	.consumer_supplies	= buck2_consumers,
};

static struct regulator_init_data max8698_buck3_data = {
	.constraints	= {
		.name		= "VCC_MEM",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.state_mem	= {
			.uV	= 1800000,
			.mode	= REGULATOR_MODE_NORMAL,
			.enabled = 1,
		},
		/* .initial_state	= PM_SUSPEND_MEM, */
	},
};

static struct regulator_init_data max8698_ldo2_data = {
	.constraints	= {
		.name		= "VALIVE_1.1V",
		.min_uV		= 1100000,
		.max_uV		= 1100000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.uV	= 1100000,
			.mode	= REGULATOR_MODE_NORMAL,
			.enabled = 1,
		},
	},
};

static struct regulator_consumer_supply ldo3_consumers[] = {
	{
		.supply		= "vddusb_dig",
	},
};

static struct regulator_init_data max8698_ldo3_data = {
	.constraints	= {
		.name		= "VUOTG_D_1.1V/VUHOST_D_1.1V",
		.min_uV		= 1100000,
		.max_uV		= 1100000,
		.apply_uV	= 1,
		.boot_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.uV		= 1100000,
			.mode		= REGULATOR_MODE_NORMAL,
			.enabled	= 1,	/* LDO3 should be OFF in sleep mode */
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo3_consumers),
	.consumer_supplies	= ldo3_consumers,
};

static struct regulator_consumer_supply ldo4_consumers[] = {
	{
    #if defined(CONFIG_MX100) /* ktj_pm */
		.supply		= "vddlcd_mod",
    #else
		.supply		= "vddmipi",
    #endif
	},
};

static struct regulator_init_data max8698_ldo4_data = {
	.constraints	= {
#if defined(CONFIG_MX100) /* ktj */
		.name		= "V_LCD_3.3V",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.boot_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.uV		= 3300000,
			.mode		= REGULATOR_MODE_NORMAL,
			.enabled	= 0,	/* LDO4 should be OFF in sleep mode */
		},
#else
		.name		= "V_MIPI_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.boot_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.uV		= 1800000,
			.mode		= REGULATOR_MODE_NORMAL,
			.enabled	= 0,	/* LDO4 should be OFF in sleep mode */
		},
#endif
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo4_consumers),
	.consumer_supplies	= ldo4_consumers,
};

#if defined(CONFIG_MX100_WS) /* Shanghai ewada */
static struct regulator_consumer_supply ldo5_consumers[] = {
	{
		.supply		= "vddcam_28",
	},
};
#endif

#if defined(CONFIG_MX100) /* ktj_pm */
static struct regulator_init_data max8698_ldo5_data = {
	.constraints	= {
		.name		= "VCAM_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		//.boot_on	= 1,
		.boot_on	= 0,//Shanghai ewada
		#if defined(CONFIG_MX100_WS) /* Shanghai ewada */
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		#endif
		.state_mem = {
			.uV		= 2800000,
			.mode		= REGULATOR_MODE_NORMAL,
			.enabled	= 0,    /* LDO5 should be OFF in sleep mode */
		},
	},
	#if defined(CONFIG_MX100_WS) /* Shanghai ewada */
	.num_consumer_supplies	= ARRAY_SIZE(ldo5_consumers),
	.consumer_supplies	= ldo5_consumers,
	#endif
};
#else
static struct regulator_init_data max8698_ldo5_data = {
	.constraints	= {
		.name		= "VMMC_2.8V/VEXT_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.state_mem = {
			.uV		= 2800000,
			.mode		= REGULATOR_MODE_NORMAL,
			.enabled	= 1,
		},
	},
};
#endif

static struct regulator_consumer_supply ldo6_consumers[] = {
	{
		.supply		= "vddlcd",
	},
};

static struct regulator_init_data max8698_ldo6_data = {
	.constraints	= {
#if defined(CONFIG_MX100) /* ktj */
		.name		= "VCC_3.3V",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.boot_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.uV		= 3300000,
			.mode		= REGULATOR_MODE_NORMAL,
			.enabled	= 0,    /* LDO6 should be OFF in sleep mode */
		},
#else
		.name		= "VCC_2.6V",
		.min_uV		= 2600000,
		.max_uV		= 2600000,
		.apply_uV	= 1,
		.boot_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.uV		= 2600000,
			.mode		= REGULATOR_MODE_NORMAL,
			.enabled	= 0,
		},
#endif
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo6_consumers),
	.consumer_supplies	= ldo6_consumers,
};

#if defined(CONFIG_MX100) /* ktj_pm */
static struct regulator_consumer_supply ldo7_consumers[] = {
	{
		.supply		= "vddcam",
	},
};

static struct regulator_init_data max8698_ldo7_data = {
	.constraints	= {
		.name		= "VCAM_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		//.boot_on	= 1,  
		.boot_on	= 0,//Shanghai ewada
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.uV		= 1800000,
			.mode		= REGULATOR_MODE_NORMAL,
			.enabled	= 0,    /* LDO7 should be OFF in sleep mode */
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo7_consumers),
	.consumer_supplies	= ldo7_consumers,
};
#else
static struct regulator_consumer_supply ldo7_consumers[] = {
	{
		.supply		= "vddmodem",
	},
};

static struct regulator_init_data max8698_ldo7_data = {
	.constraints	= {
		.name		= "VDAC_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.boot_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.uV		= 2800000,
			.mode		= REGULATOR_MODE_NORMAL,
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo7_consumers),
	.consumer_supplies	= ldo7_consumers,
};
#endif

static struct regulator_consumer_supply ldo8_consumers[] = {
	{
		.supply		= "vddusb_anlg",
	},
};

static struct regulator_init_data max8698_ldo8_data = {
	.constraints	= {
		.name		= "VUOTG_A_3.3V/VUHOST_A_3.3V",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.boot_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.uV		= 0,
			.mode		= REGULATOR_MODE_NORMAL,
			.enabled	= 1,	/* LDO8 should be OFF in sleep mode */
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo8_consumers),
	.consumer_supplies	= ldo8_consumers,
};

static struct regulator_init_data max8698_ldo9_data = {
	.constraints	= {
		.name		= "{VADC/VSYS/VKEY}_2.8V",
		.min_uV		= 3000000,
		.max_uV		= 3000000,
		.apply_uV	= 1,
		.state_mem = {
			.uV		= 3000000,
			.mode		= REGULATOR_MODE_NORMAL,
			.enabled	= 1,
		},
	},
};

static struct max8698_subdev_data smdkc110_regulators[] = {
	{ MAX8698_LDO2, &max8698_ldo2_data },
	{ MAX8698_LDO3, &max8698_ldo3_data },
	{ MAX8698_LDO4, &max8698_ldo4_data },
	{ MAX8698_LDO5, &max8698_ldo5_data },
	{ MAX8698_LDO6, &max8698_ldo6_data },
	{ MAX8698_LDO7, &max8698_ldo7_data },
	{ MAX8698_LDO8, &max8698_ldo8_data },
	{ MAX8698_LDO9, &max8698_ldo9_data },
	{ MAX8698_BUCK1, &max8698_buck1_data },
	{ MAX8698_BUCK2, &max8698_buck2_data },
	{ MAX8698_BUCK3, &max8698_buck3_data },
};

/* 800MHz default voltage */
#if 0
static struct max8698_platform_data max8698_platform_data_0 = {
	.num_regulators	= ARRAY_SIZE(smdkc110_regulators),
	.regulators	= smdkc110_regulators,

	.set1		= S5PV210_GPH1(6),
	.set2		= S5PV210_GPH1(7),
	.set3		= S5PV210_GPH0(4),

	.dvsarm1	= 0x8,
	.dvsarm2	= 0x6,
	.dvsarm3	= 0x5,
	.dvsarm4	= 0x5,

	.dvsint1	= 0x9,
	.dvsint2	= 0x5,
};
#endif
/* 1Ghz default voltage */
static struct max8698_platform_data max8698_platform_data = {
	.num_regulators = ARRAY_SIZE(smdkc110_regulators),
	.regulators     = smdkc110_regulators,

	.set1           = S5PV210_GPH1(6),
	.set2           = S5PV210_GPH1(7),
	.set3           = S5PV210_GPH0(4),
	.dvsarm1        = 0xa,  // 1.25v
	.dvsarm2        = 0x9,  // 1.20V
	.dvsarm3        = 0x6,  // 1.05V
	.dvsarm4        = 0x4,  // 0.95V

	.dvsint1        = 0x7,  // 1.10v
	.dvsint2        = 0x5,  // 1.00V
};
#endif

#ifdef CONFIG_TOUCHSCREEN_S3C
static struct s3c_ts_mach_info s3c_ts_platform __initdata = {
	.delay                  = 10000,
	.presc                  = 49,
	.oversampling_shift     = 2,
	.resol_bit              = 12,
	.s3c_adc_con            = ADC_TYPE_2,
};
#endif

#ifdef CONFIG_S5PV210_ADC
static struct s3c_adc_mach_info s3c_adc_platform __initdata = {
	/* s5pc100 supports 12-bit resolution */
	.delay  = 10000,
	.presc  = 49,
	.resolution = 12,
};
#endif

#ifdef 	CONFIG_SOC_CAMERA_MT9P111

//AF15 SDA
//AE16 SCL
#include <linux/regulator/consumer.h>
int mt9p111_sensor_power(int on, struct clk *clk)
{
    static int mt9p111_power_status = false;
	if(mt9p111_power_status == on)
	{
		return 0;
	}else
	{
		mt9p111_power_status = on;
	}
	/* ldo5 regulator get */
	struct regulator *cam_regulator_28 = regulator_get(NULL, "vddcam_28");
	if (IS_ERR(cam_regulator_28)) {
		printk(KERN_ERR "failed to get resource %s\n", "vddcam_28");
		return PTR_ERR(cam_regulator_28);
	}

	/* ldo7 regulator get */
	struct regulator *cam_regulator_18 = regulator_get(NULL, "vddcam");
	if (IS_ERR(cam_regulator_18)) {
		printk(KERN_ERR "failed to get resource %s\n", "vddcam");
		return PTR_ERR(cam_regulator_18);
	}

	printk("\n.........  void mt9p111_sensor_power(%d)\n", on);
	//int powerdown = ; /*active high, work to set to low*/

	int stanby = S5PV210_GPH3(3);   /*active high, work to set to low*/

	int reset = S5PV210_GPH0(2); /*active  low , work to set to high*/

	int stanby_1p3M =S5PV210_GPH3(5);   /*active high, work to set to low*/

	int reset_1p3M = S5PV210_GPH3(1); /*active  low , work to set to high*/
	
	int err;
	/*cam 2.8v max8698c ldo5*/
	/*cam 1.8v max8698c ldo7*/
#if 0
	/* Camera A */
	err = gpio_request(S5PV210_GPH0(2), "GPH0");
	if (err)
		printk(KERN_ERR "#### failed to request GPH0 for CAM_2V8\n");

	s3c_gpio_setpull(S5PV210_GPH0(2), S3C_GPIO_PULL_NONE);
	gpio_direction_output(S5PV210_GPH0(2), 0);
	gpio_direction_output(S5PV210_GPH0(2), 1);
	gpio_free(S5PV210_GPH0(2));
#endif

	/* Camera A */
	err = gpio_request(stanby, "cam stanby");
	if (err) {
		printk(KERN_ERR "#### failed to request %d for 5M camera STB\n",stanby);
		return err;
	}
	s3c_gpio_setpull(stanby, S3C_GPIO_PULL_NONE);

	/* Camera A */
	err = gpio_request(reset, "cam reset");
	if (err) {
		printk(KERN_ERR "#### failed to request %d for 5M cam RST\n",reset);
		return err;
	}
	s3c_gpio_setpull(reset, S3C_GPIO_PULL_NONE);

//config 1.3M camera
	err = gpio_request(stanby_1p3M, "Fcam stanby");
	if (err) {
		printk(KERN_ERR "#### failed to request %d for 1.3M cam STB\n",stanby_1p3M);
		return err;
	}
	s3c_gpio_setpull(stanby_1p3M, S3C_GPIO_PULL_NONE);

	/* Camera A */
	err = gpio_request(reset_1p3M, "Fcam reset");
	if (err) {
		printk(KERN_ERR "#### failed to request %d for1.3M cam reset\n",reset_1p3M);
		return err;
	}
	s3c_gpio_setpull(reset_1p3M, S3C_GPIO_PULL_NONE);
	
	if (on)
	{
//prepare 
		gpio_direction_output(reset, 0);
		gpio_direction_output(stanby, 1);
		gpio_direction_output(reset_1p3M, 0);  //liujianhua guess
		gpio_direction_output(stanby_1p3M, 1); //liujianhua guess
		
		if(regulator_is_enabled(cam_regulator_18))
			regulator_disable(cam_regulator_18);
		if(regulator_is_enabled(cam_regulator_28))
			regulator_disable(cam_regulator_28);
		mdelay(10);
		mdelay(10); //just test for first time start camera

//start power on	
		/* ldo7 regulator on */
		regulator_enable(cam_regulator_18);
		mdelay(5);
		/* ldo5 regulator on */
		regulator_enable(cam_regulator_28);
		mdelay(5);
		clk_enable(clk);
		msleep(20);
		
		gpio_direction_output(stanby, 0);
		msleep(10);

		gpio_direction_output(reset, 1);
		msleep(10);
	}
	else
	{
#if 1
		gpio_direction_output(reset, 0);
		msleep(10);
		gpio_direction_output(stanby, 1);
		mdelay(5);
		/* ldo5 regulator off */
		regulator_disable(cam_regulator_28);
		mdelay(5);
		/* ldo7 regulator off */
		regulator_disable(cam_regulator_18);
		clk_disable(clk);
		mdelay(10);
#endif

#if 0 //liujianhua test	
		gpio_direction_output(reset, 0);
		msleep(10);
		gpio_direction_output(stanby, 1);
		mdelay(5);
		/* ldo5 regulator off */
		regulator_disable(cam_regulator_28);
		mdelay(5);
		/* ldo7 regulator off */
		regulator_disable(cam_regulator_18);
		mdelay(5);
		clk_disable(clk);

		//liujianhua test
		gpio_direction_output(stanby, 0);
		gpio_direction_output(reset_1p3M, 0);  //liujianhua guess
		gpio_direction_output(stanby_1p3M, 0); //liujianhua guess
		mdelay(5);
		gpio_direction_input(reset);
		gpio_direction_input(stanby);
		gpio_direction_input(reset_1p3M);
		gpio_direction_input(stanby_1p3M);
		mdelay(5);
#endif
	}

	regulator_put(cam_regulator_28);
	regulator_put(cam_regulator_18);

	gpio_free(stanby);	
	gpio_free(reset);
	gpio_free(stanby_1p3M);
	gpio_free(reset_1p3M);
	if(!on)
	{
		msleep(50);
	}
	
	return 0;
}



static struct s5k4ba_platform_data mt9p111_plat = {
	.default_width = 1024,
	.default_height = 768,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.is_mipi = 0,
};

static struct i2c_board_info  mt9p111_i2c_info = {
	I2C_BOARD_INFO("mt9p111", (0x7a>>1)),
	.platform_data = &mt9p111_plat,
};

static struct s3c_platform_camera mt9p111 = {
	.id		= CAMERA_PAR_A,
	.type		= CAM_TYPE_ITU,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.i2c_busnum	= 10,
	.info		= &mt9p111_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name	= "xusbxti",
	.clk_name	= "sclk_cam0",
	.clk_rate	= 24000000,
	.line_length	= 2592,
	.width		= 1024,
	.height		= 768,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 1024,
		.height	= 768,
	},

	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync	= 0,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized	= 0,

	.cam_power	= mt9p111_sensor_power,
};

#endif

#ifdef 	CONFIG_SOC_CAMERA_MT9M113

//AF15 SDA
//AE16 SCL

int mt9m113_sensor_power(int onoff, struct clk *clk)
{
	/* ldo5 regulator get */
	struct regulator *cam_regulator_28 = regulator_get(NULL, "vddcam_28");
	if (IS_ERR(cam_regulator_28)) {
		printk(KERN_ERR "failed to get resource %s\n", "vddcam_28");
		return PTR_ERR(cam_regulator_28);
	}

	/* ldo7 regulator get */
	struct regulator *cam_regulator_18 = regulator_get(NULL, "vddcam");
	if (IS_ERR(cam_regulator_18)) {
		printk(KERN_ERR "failed to get resource %s\n", "vddcam");
		return PTR_ERR(cam_regulator_18);
	}

    printk("\n.........  %s (%d) \n", __FUNCTION__, onoff);

#if 0
	int err;
	return 0;

	/* Camera A */
	err = gpio_request(S5PV210_GPH0(2), "GPH0");
	if (err)
		printk(KERN_ERR "#### failed to request GPH0 for CAM_2V8\n");

	s3c_gpio_setpull(S5PV210_GPH0(2), S3C_GPIO_PULL_NONE);
	gpio_direction_output(S5PV210_GPH0(2), 0);
	gpio_direction_output(S5PV210_GPH0(2), 1);
	gpio_free(S5PV210_GPH0(2));
#endif
	//int powerdown = ; /*active high, work to set to low*/

	int stanby =S5PV210_GPH3(5);   /*active high, work to set to low*/

	int reset = S5PV210_GPH3(1); /*active  low , work to set to high*/

#if 1 //test for lee
	int stanby_5M = S5PV210_GPH3(3);   /*active high, work to set to low*/

	int reset_5M = S5PV210_GPH0(2); /*active  low , work to set to high*/
#endif
	
	int err;
	/*cam 2.8v max8698c ldo5*/
	/*cam 1.8v max8698c ldo7*/
	
#if 0
	/* Camera A */
	err = gpio_request(S5PV210_GPH0(2), "GPH0");
	if (err)
		printk(KERN_ERR "#### failed to request GPH0 for CAM_2V8\n");

	s3c_gpio_setpull(S5PV210_GPH0(2), S3C_GPIO_PULL_NONE);
	gpio_direction_output(S5PV210_GPH0(2), 0);
	gpio_direction_output(S5PV210_GPH0(2), 1);
	gpio_free(S5PV210_GPH0(2));
#endif

	
		/* Camera A */
	err = gpio_request(stanby, "cam stanby");
	if (err)
		printk(KERN_ERR "#### failed to request GPH0 for CAM_2V8\n");

	s3c_gpio_setpull(stanby, S3C_GPIO_PULL_NONE);

		/* Camera A */
	err = gpio_request(reset, "cam reset");

	s3c_gpio_setpull(reset, S3C_GPIO_PULL_NONE);

#if 1 //test for lee
		/* Camera A */
	err = gpio_request(stanby_5M, "cam stanby 5M");
	if (err)
		printk(KERN_ERR "#### failed to request GPH0 for CAM_2V8\n");

	s3c_gpio_setpull(stanby_5M, S3C_GPIO_PULL_NONE);

		/* Camera A */
	err = gpio_request(reset_5M, "cam reset 5M");

	s3c_gpio_setpull(reset_5M, S3C_GPIO_PULL_NONE);
#endif
	
	if (onoff)
	{
#if 0 //work ok
		gpio_direction_output(reset, 0);
		
		/* ldo7 regulator on */
		regulator_enable(cam_regulator_18);
		mdelay(10);
		
		/* ldo5 regulator on */
		regulator_enable(cam_regulator_28);
		
		gpio_direction_output(stanby, 0);
		mdelay(5);
		
		clk_enable(clk);
		msleep(10);
		gpio_direction_output(reset, 1);
		msleep(10);
#endif

#if 1 //test for lee
		gpio_direction_output(reset, 0);
		gpio_direction_output(stanby,1); 
		gpio_direction_output(reset_5M, 0);
		gpio_direction_output(stanby_5M, 0);
		if(regulator_is_enabled(cam_regulator_28))
			regulator_disable(cam_regulator_28);
		if(regulator_is_enabled(cam_regulator_18))
			regulator_disable(cam_regulator_18);
		mdelay(10);
		mdelay(10);
		
		/* ldo7 regulator on */
		regulator_enable(cam_regulator_18);
		///* ldo5 regulator on */
		//regulator_enable(cam_regulator_28);
		msleep(5);
		
		///* ldo7 regulator on */
		//regulator_enable(cam_regulator_18);
		/* ldo5 regulator on */
		regulator_enable(cam_regulator_28);
		
		gpio_direction_output(stanby, 0);
		mdelay(5);
		
		clk_enable(clk);
		msleep(10);
		
		gpio_direction_output(reset, 1);
		msleep(10);

		gpio_direction_output(reset_5M, 1);
		msleep(5);
		
#endif


#if 0
		/* ldo7 regulator on */
		regulator_enable(cam_regulator_18);
		/* ldo5 regulator on */
		regulator_enable(cam_regulator_28);

		mdelay(5);
		clk_enable(clk);
		msleep(20);
		gpio_direction_output(stanby, 0);
		mdelay(5);

		gpio_direction_output(reset, 0);
		msleep(10);
		gpio_direction_output(reset, 1);
		msleep(10);
#endif
	}
	else
	{
#if 0
		gpio_direction_output(reset, 0);
		msleep(10);
		gpio_direction_output(stanby, 1);
		mdelay(5);
		clk_disable(clk);
		/* ldo5 regulator off */
		regulator_disable(cam_regulator_28);
		msleep(10);
		/* ldo7 regulator off */
		regulator_disable(cam_regulator_18);
#endif	

#if 0//work ok
		gpio_direction_output(reset, 0);
		mdelay(5);
		gpio_direction_output(stanby, 1);

		/* ldo5 regulator off */
		regulator_disable(cam_regulator_28);
		/* ldo7 regulator off */
		regulator_disable(cam_regulator_18);
		clk_disable(clk);
#endif

#if 1 //just follow the power sequence setting
		gpio_direction_output(reset_5M, 0);
		mdelay(3);
		gpio_direction_output(reset, 0);
		mdelay(5);
		gpio_direction_output(stanby, 1);
		mdelay(1);
		//clk_disable(clk);
		/* ldo7 regulator off */
		regulator_disable(cam_regulator_28);
		clk_disable(clk);
		mdelay(5);
		regulator_disable(cam_regulator_18);
		mdelay(20);
#endif



#if 0//liujianhua test
		gpio_direction_output(reset_5M, 0);
		mdelay(3);
		gpio_direction_output(reset, 0);
		mdelay(5);
		gpio_direction_output(stanby, 1);
		gpio_direction_output(stanby_5M, 0);  //liujianhua add
		mdelay(1);
		/* ldo5 regulator off */
		regulator_disable(cam_regulator_28);
		clk_disable(clk);
		mdelay(5);
		/* ldo7 regulator off */
		regulator_disable(cam_regulator_18);
		mdelay(5);
		gpio_direction_output(stanby, 0);   //liujianhua
		mdelay(5);
		gpio_direction_input(reset_5M);
		gpio_direction_input(stanby_5M);
		gpio_direction_input(reset);
		gpio_direction_input(stanby);
		
#endif

	}

	regulator_put(cam_regulator_28);
	regulator_put(cam_regulator_18);

	gpio_free(stanby);
	
	gpio_free(reset);

#if 1 //test for lee
	gpio_free(stanby_5M);
	
	gpio_free(reset_5M);
#endif 
	if (err)
		printk(KERN_ERR "#### failed to request GPH0 for CAM_2V8\n");
	return 0;

}



static struct s5k4ba_platform_data mt9m113_plat = {
	.default_width = 1280,
	.default_height = 960,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.is_mipi = 0,
};

static struct i2c_board_info  mt9m113_i2c_info = {
	I2C_BOARD_INFO("mt9m113", (0x78>>1)),
	.platform_data = &mt9m113_plat,
};

static struct s3c_platform_camera mt9m113 = {
	.id		= CAMERA_PAR_B,
	.type		= CAM_TYPE_ITU,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.i2c_busnum	= 10,
	.info		= &mt9m113_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name	= "xusbxti",
	.clk_name	= "sclk_cam0",
	.clk_rate	= 24000000,
	.line_length	= 1920,
	.width		= 1280,
	.height		= 960,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 1280,
		.height	= 960,
	},

	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized	= 0,

	.cam_power	= mt9m113_sensor_power,
};

#endif

/*Shanghai ewada camera power*/
#ifdef CONFIG_MX100
static void cam_hw_config_gpio()
{
	/*cam mt9p111*/
	int stanby = S5PV210_GPH3(3);   /*active high, work to set to low*/
	int reset = S5PV210_GPH0(2); /*active  low , work to set to high*/
	int err;

	err = gpio_request(stanby, "cam stanby");
	if (err) {
		printk(KERN_ERR "#### failed to request GPH0 for CAM_2V8\n");
		return err;
	}
	s3c_gpio_setpull(stanby, S3C_GPIO_PULL_NONE);
	err = gpio_request(reset, "cam reset");
	if (err) {
		printk(KERN_ERR "#### failed to request GPH0 for cam reset\n");
		return err;
	}
	s3c_gpio_setpull(reset, S3C_GPIO_PULL_NONE);

	gpio_direction_output(reset, 0);
	gpio_direction_output(stanby, 1);

	gpio_free(stanby);	
	gpio_free(reset);

    /*mt9113*/
	stanby =S5PV210_GPH3(5);   /*active high, work to set to low*/
	reset = S5PV210_GPH3(1); /*active  low , work to set to high*/
	err = gpio_request(stanby, "cam stanby");
	if (err) {
		printk(KERN_ERR "#### failed to request GPH0 for CAM_2V8\n");
		return err;
	}
	s3c_gpio_setpull(stanby, S3C_GPIO_PULL_NONE);
	err = gpio_request(reset, "cam reset");
	if (err) {
		printk(KERN_ERR "#### failed to request GPH0 for cam reset\n");
		return err;
	}
	s3c_gpio_setpull(reset, S3C_GPIO_PULL_NONE);
	gpio_direction_output(reset, 0);
	gpio_direction_output(stanby, 1);

	gpio_free(stanby);	
	gpio_free(reset);

}
#endif

#ifdef CONFIG_VIDEO_FIMC
/*
 * External camera reset
 * Because the most of cameras take i2c bus signal, so that
 * you have to reset at the boot time for other i2c slave devices.
 * This function also called at fimc_init_camera()
 * Do optimization for cameras on your platform.
*/
static int smdkv210_cam0_power(int onoff)
{


	printk("\n.........  %s (%d) \n", __FUNCTION__, onoff);
#if 0
	int err;
	/* Camera A */
	err = gpio_request(S5PV210_GPH0(2), "GPH0");
	if (err)
		printk(KERN_ERR "#### failed to request GPH0 for CAM_2V8\n");

	s3c_gpio_setpull(S5PV210_GPH0(2), S3C_GPIO_PULL_NONE);
	gpio_direction_output(S5PV210_GPH0(2), 0);
	gpio_direction_output(S5PV210_GPH0(2), 1);
	gpio_free(S5PV210_GPH0(2));
#endif
	//int powerdown = ; /*active high, work to set to low*/

	int stanby =S5PV210_GPH3(5);   /*active high, work to set to low*/

	int reset = S5PV210_GPH3(1); /*active  low , work to set to high*/
	int err;
	/*cam 2.8v max8698c ldo5*/
	/*cam 1.8v max8698c ldo7*/
#if 0
	/* Camera A */
	err = gpio_request(S5PV210_GPH0(2), "GPH0");
	if (err)
		printk(KERN_ERR "#### failed to request GPH0 for CAM_2V8\n");

	s3c_gpio_setpull(S5PV210_GPH0(2), S3C_GPIO_PULL_NONE);
	gpio_direction_output(S5PV210_GPH0(2), 0);
	gpio_direction_output(S5PV210_GPH0(2), 1);
	gpio_free(S5PV210_GPH0(2));
#endif

	
		/* Camera A */
	err = gpio_request(stanby, "cam stanby");
	if (err)
		printk(KERN_ERR "#### failed to request GPH0 for CAM_2V8\n");

	s3c_gpio_setpull(stanby, S3C_GPIO_PULL_NONE);

		/* Camera A */
	err = gpio_request(reset, "cam reset");

	s3c_gpio_setpull(reset, S3C_GPIO_PULL_NONE);

	
	if (onoff)
	{
		gpio_direction_output(stanby, 0);
		msleep(1);

		gpio_direction_output(reset, 0);
		msleep(10);
		gpio_direction_output(reset, 1);
		msleep(10);
	}
	else
	{
		gpio_direction_output(reset, 0);
		msleep(1);
		gpio_direction_output(stanby, 1);
	}


	gpio_free(stanby);
	
	gpio_free(reset);
	if (err)
		printk(KERN_ERR "#### failed to request GPH0 for CAM_2V8\n");
	return 0;
}

static int smdkv210_cam1_power(int onoff)
{
	int err;

	printk("\n.........  %s (%d) \n", __FUNCTION__, onoff);

	return 0;


	/* S/W workaround for the SMDK_CAM4_type board
	 * When SMDK_CAM4 type module is initialized at power reset,
	 * it needs the cam_mclk.
	 *
	 * Now cam_mclk is set as below, need only set the gpio mux.
	 * CAM_SRC1 = 0x0006000, CLK_DIV1 = 0x00070400.
	 * cam_mclk source is SCLKMPLL, and divider value is 8.
	*/

	/* gpio mux select the cam_mclk */
	err = gpio_request(S5PV210_GPJ1(4), "GPJ1");
	if (err)
		printk(KERN_ERR "#### failed to request GPJ1 for CAM_2V8\n");

	s3c_gpio_setpull(S5PV210_GPJ1(4), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S5PV210_GPJ1(4), (0x3<<16));


	/* Camera B */
	err = gpio_request(S5PV210_GPH0(3), "GPH0");
	if (err)
		printk(KERN_ERR "#### failed to request GPH0 for CAM_2V8\n");

	s3c_gpio_setpull(S5PV210_GPH0(3), S3C_GPIO_PULL_NONE);
	gpio_direction_output(S5PV210_GPH0(3), 0);
	gpio_direction_output(S5PV210_GPH0(3), 1);

	udelay(1000);

	gpio_free(S5PV210_GPJ1(4));
	gpio_free(S5PV210_GPH0(3));

	return 0;
}

static int smdkv210_mipi_cam_reset(void)
{
	int err;

	err = gpio_request(S5PV210_GPH0(3), "GPH0");
	if (err)
		printk(KERN_ERR "#### failed to reset(GPH0) for MIPI CAM\n");

	s3c_gpio_setpull(S5PV210_GPH0(3), S3C_GPIO_PULL_NONE);
	gpio_direction_output(S5PV210_GPH0(3), 0);
	gpio_direction_output(S5PV210_GPH0(3), 1);
	gpio_free(S5PV210_GPH0(3));

	return 0;
}

static int smdkv210_mipi_cam_power(int onoff)
{
	int err;

	/* added for V210 CAM power */
	err = gpio_request(S5PV210_GPH1(2), "GPH1");
	if (err)
		printk(KERN_ERR "#### failed to request(GPH1)for CAM_2V8\n");

	s3c_gpio_setpull(S5PV210_GPH1(2), S3C_GPIO_PULL_NONE);
	gpio_direction_output(S5PV210_GPH1(2), onoff);
	gpio_free(S5PV210_GPH1(2));

	return 0;
}

/*
 * Guide for Camera Configuration for smdkv210
 * ITU channel must be set as A or B
 * ITU CAM CH A: S5K3BA only
 * ITU CAM CH B: one of S5K3BA and S5K4BA
 * MIPI: one of S5K4EA and S5K6AA
 *
 * NOTE1: if the S5K4EA is enabled, all other cameras must be disabled
 * NOTE2: currently, only 1 MIPI camera must be enabled
 * NOTE3: it is possible to use both one ITU cam and
 * 	  one MIPI cam except for S5K4EA case
 *
*/
#undef CAM_ITU_CH_A
#undef WRITEBACK_ENABLED

#ifdef CONFIG_VIDEO_S5K4EA
#define S5K4EA_ENABLED
/* undef : 3BA, 4BA, 6AA */
#else
#ifdef CONFIG_VIDEO_S5K6AA
#define S5K6AA_ENABLED
/* undef : 4EA */
#else
#ifdef CONFIG_VIDEO_S5K3BA
#define S5K3BA_ENABLED
/* undef : 4BA */
#elif CONFIG_VIDEO_S5K4BA
#define S5K4BA_ENABLED
/* undef : 3BA */
#endif
#endif
#endif

/* External camera module setting */
/* 2 ITU Cameras */
#ifdef S5K3BA_ENABLED
static struct s5k3ba_platform_data s5k3ba_plat = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_VYUY,
	.freq = 24000000,
	.is_mipi = 0,
};

static struct i2c_board_info  s5k3ba_i2c_info = {
	I2C_BOARD_INFO("S5K3BA", 0x2d),
	.platform_data = &s5k3ba_plat,
};

static struct s3c_platform_camera s5k3ba = {
#ifdef CAM_ITU_CH_A
	.id		= CAMERA_PAR_A,
#else
	.id		= CAMERA_PAR_B,
#endif
	.type		= CAM_TYPE_ITU,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CRYCBY,
	.i2c_busnum	= 0,
	.info		= &s5k3ba_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_VYUY,
	.srclk_name	= "mout_epll",
	.clk_name	= "sclk_cam1",
	.clk_rate	= 24000000,
	.line_length	= 1920,
	.width		= 640,
	.height		= 480,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 640,
		.height	= 480,
	},

	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized	= 0,
#ifdef CAM_ITU_CH_A
	.cam_power	= smdkv210_cam0_power,
#else
	.cam_power	= smdkv210_cam1_power,
#endif
};
#endif

#ifdef S5K4BA_ENABLED
static struct s5k4ba_platform_data s5k4ba_plat = {
	.default_width = 800,
	.default_height = 600,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 44000000,
	.is_mipi = 0,
};

static struct i2c_board_info  s5k4ba_i2c_info = {
	I2C_BOARD_INFO("S5K4BA", 0x2d),
	.platform_data = &s5k4ba_plat,
};

static struct s3c_platform_camera s5k4ba = {
	.id		= CAMERA_PAR_B,
	.type		= CAM_TYPE_ITU,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.i2c_busnum	= 10,
	.info		= &s5k4ba_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name	= "mout_mpll",
	.clk_name	= "sclk_cam1",
	.clk_rate	= 44000000,
	.line_length	= 1920,
	.width		= 800,
	.height		= 600,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 800,
		.height	= 600,
	},

	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized	= 0,

	.cam_power	= smdkv210_cam0_power,
#if 0
#ifdef CAM_ITU_CH_A
	.cam_power	= smdkv210_cam0_power,
#else
	.cam_power	= smdkv210_cam1_power,
#endif
#endif
};
#endif

/* 2 MIPI Cameras */
#ifdef S5K4EA_ENABLED
static struct s5k4ea_platform_data s5k4ea_plat = {
	.default_width = 1920,
	.default_height = 1080,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.is_mipi = 1,
};

static struct i2c_board_info  s5k4ea_i2c_info = {
	I2C_BOARD_INFO("S5K4EA", 0x2d),
	.platform_data = &s5k4ea_plat,
};

static struct s3c_platform_camera s5k4ea = {
	.id		= CAMERA_CSI_C,
	.type		= CAM_TYPE_MIPI,
	.fmt		= MIPI_CSI_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.i2c_busnum	= 0,
	.info		= &s5k4ea_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name	= "mout_mpll",
	.clk_name	= "sclk_cam0",
	.clk_rate	= 48000000,
	.line_length	= 1920,
	.width		= 1920,
	.height		= 1080,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 1920,
		.height	= 1080,
	},

	.mipi_lanes	= 2,
	.mipi_settle	= 12,
	.mipi_align	= 32,

	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized	= 0,
	.cam_power	= smdkv210_mipi_cam_power,
};
#endif

#ifdef S5K6AA_ENABLED
static struct s5k6aa_platform_data s5k6aa_plat = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.is_mipi = 1,
};

static struct i2c_board_info  s5k6aa_i2c_info = {
	I2C_BOARD_INFO("S5K6AA", 0x3c),
	.platform_data = &s5k6aa_plat,
};

static struct s3c_platform_camera s5k6aa = {
	.id		= CAMERA_CSI_C,
	.type		= CAM_TYPE_MIPI,
	.fmt		= MIPI_CSI_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.i2c_busnum	= 0,
	.info		= &s5k6aa_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name	= "xusbxti",
	.clk_name	= "sclk_cam",
	.clk_rate	= 24000000,
	.line_length	= 1920,
	/* default resol for preview kind of thing */
	.width		= 640,
	.height		= 480,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 640,
		.height	= 480,
	},

	.mipi_lanes	= 1,
	.mipi_settle	= 6,
	.mipi_align	= 32,

	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized	= 0,
	.cam_power	= smdkv210_mipi_cam_power,
};
#endif

#ifdef WRITEBACK_ENABLED
static struct i2c_board_info  writeback_i2c_info = {
	I2C_BOARD_INFO("WriteBack", 0x0),
};

static struct s3c_platform_camera writeback = {
	.id		= CAMERA_WB,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.i2c_busnum	= 0,
	.info		= &writeback_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_YUV444,
	.line_length	= 800,
	.width		= 480,
	.height		= 800,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 480,
		.height	= 800,
	},

	.initialized	= 0,
};
#endif

/* Interface setting */
static struct s3c_platform_fimc fimc_plat = {
	.srclk_name	= "mout_mpll",
	.clk_name	= "fimc",
	.clk_rate	= 166000000,
#if defined(S5K4EA_ENABLED) || defined(S5K6AA_ENABLED)
	.default_cam	= CAMERA_PAR_A,   /*CAMERA_CSI_C*/
#else

#ifdef WRITEBACK_ENABLED
	.default_cam	= CAMERA_WB,
#elif CAM_ITU_CH_A
	.default_cam	= CAMERA_PAR_A,
#else
	.default_cam	= CAMERA_PAR_A, /*CAMERA_PAR_B*/
#endif

#endif
	.camera		= {
#ifdef 	CONFIG_SOC_CAMERA_MT9P111
		&mt9p111,
#endif

#ifdef 	CONFIG_SOC_CAMERA_MT9M113
		&mt9m113,
#endif


#if 0
#ifdef S5K3BA_ENABLED
		&s5k3ba,
#endif
#ifdef S5K4BA_ENABLED
		&s5k4ba,
#endif
#ifdef S5K4EA_ENABLED
		&s5k4ea,
#endif
#ifdef S5K6AA_ENABLED
		&s5k6aa,
#endif
#ifdef WRITEBACK_ENABLED
		&writeback,
#endif
#endif

	},
	.hw_ver		= 0x43,
};
#endif

#ifdef CONFIG_VIDEO_JPEG_V2
static struct s3c_platform_jpeg jpeg_plat __initdata = {
	.max_main_width		= 2592,
	.max_main_height	= 1944,
	.max_thumb_width	= 320,
	.max_thumb_height	= 240,
};
#endif

#if defined(CONFIG_SPI_CNTRLR_0) || defined(CONFIG_SPI_CNTRLR_1) || defined(CONFIG_SPI_CNTRLR_2)
static void s3c_cs_suspend(int pin, pm_message_t pm)
{
        /* Whatever need to be done */
}

static void s3c_cs_resume(int pin)
{
        /* Whatever need to be done */
}

static void s3c_cs_set(int pin, int lvl)
{
        if(lvl == CS_HIGH)
           s3c_gpio_setpin(pin, 1);
        else
           s3c_gpio_setpin(pin, 0);
}
static void s3c_cs_config(int pin, int mode, int pull)
{
        s3c_gpio_cfgpin(pin, mode);

        if(pull == CS_HIGH){
           s3c_gpio_setpull(pin, S3C_GPIO_PULL_UP);
		   s3c_gpio_setpin(pin, 0);
		}
        else{
           s3c_gpio_setpull(pin, S3C_GPIO_PULL_DOWN);
		   s3c_gpio_setpin(pin, 1);
		}
}
#endif

#if defined(CONFIG_SPI_CNTRLR_0)
static struct s3c_spi_pdata s3c_slv_pdata_0[] __initdata = {
        [0] = { /* Slave-0 */
                .cs_level     = CS_FLOAT,
                .cs_pin       = S5PV210_GPB(1),
                .cs_mode      = S5PV210_GPB_OUTPUT(1),
                .cs_set       = s3c_cs_set,
                .cs_config    = s3c_cs_config,
                .cs_suspend   = s3c_cs_suspend,
                .cs_resume    = s3c_cs_resume,
        },
        #if 0
        [1] = { /* Slave-1 */
                .cs_level     = CS_FLOAT,
                .cs_pin       = S5PV210_GPA1(1),
                .cs_mode      = S5PV210_GPA1_OUTPUT(1),
                .cs_set       = s3c_cs_set,
                .cs_config    = s3c_cs_config,
                .cs_suspend   = s3c_cs_suspend,
                .cs_resume    = s3c_cs_resume,
        },
        #endif
};
#endif

#if defined(CONFIG_SPI_CNTRLR_1)
static struct s3c_spi_pdata s3c_slv_pdata_1[] __initdata = {
        [0] = { /* Slave-0 */
                .cs_level     = CS_FLOAT,
                .cs_pin       = S5PV210_GPB(5),
                .cs_mode      = S5PV210_GPB_OUTPUT(5),
                .cs_set       = s3c_cs_set,
                .cs_config    = s3c_cs_config,
                .cs_suspend   = s3c_cs_suspend,
                .cs_resume    = s3c_cs_resume,
        },
		#if 0
        [1] = { /* Slave-1 */
                .cs_level     = CS_FLOAT,
                .cs_pin       = S5PV210_GPA1(3),
                .cs_mode      = S5PV210_GPA1_OUTPUT(3),
                .cs_set       = s3c_cs_set,
                .cs_config    = s3c_cs_config,
                .cs_suspend   = s3c_cs_suspend,
                .cs_resume    = s3c_cs_resume,
        },
		#endif
};
#endif

static struct spi_board_info s3c_spi_devs[] __initdata = {
#if defined(CONFIG_SPI_CNTRLR_0)
        [0] = {
                .modalias        = "spidev", /* Test Interface */
                .mode            = SPI_MODE_0,  /* CPOL=0, CPHA=0 */
                .max_speed_hz    = 100000,
                /* Connected to SPI-0 as 1st Slave */
                .bus_num         = 0,
                .irq             = IRQ_SPI0,
                .chip_select     = 0,
        },
		#if 0
        [1] = {
                .modalias        = "spidev", /* Test Interface */
                .mode            = SPI_MODE_0,  /* CPOL=0, CPHA=0 */
                .max_speed_hz    = 100000,
                /* Connected to SPI-0 as 2nd Slave */
                .bus_num         = 0,
                .irq             = IRQ_SPI0,
                .chip_select     = 1,
        },
		#endif
#endif

#if defined(CONFIG_SPI_CNTRLR_1)
        [1] = {
                .modalias        = "spidev", /* Test Interface */
                .mode            = SPI_MODE_0,  /* CPOL=0, CPHA=0 */
                .max_speed_hz    = 100000,
                /* Connected to SPI-1 as 1st Slave */
                .bus_num         = 1,
                .irq             = IRQ_SPI1,
                .chip_select     = 0,
        },
		#if 0
        [3] = {
                .modalias        = "spidev", /* Test Interface */
                .mode            = SPI_MODE_0 | SPI_CS_HIGH,    /* CPOL=0, CPHA=0 & CS is Active High */
                .max_speed_hz    = 100000,
                /* Connected to SPI-1 as 3rd Slave */
                .bus_num         = 1,
                .irq             = IRQ_SPI1,
                .chip_select     = 1,
        },
		#endif
#endif
};

#if defined(CONFIG_MX100) /* ktj rdmb */
/*
static struct spi_board_info Tdmb_board_info[] __initdata = {
		{
				.modalias		= "tdmb",
				.mode			= SPI_MODE_0,
				.platform_data	= NULL,
				.max_speed_hz	= 120000,
				.bus_num		= 0,
				.chip_select	= 0,
				.irq            = IRQ_SPI0,
		},
};
*/
static struct spi_board_info Tdmb_board_info[] __initdata = {
		{
				.modalias		= "tdmb",
				.mode			= SPI_MODE_0,
				.platform_data	= NULL,
				.max_speed_hz	= 120000,
				.bus_num		= 1,
				.chip_select	= 0,
				.irq            = IRQ_SPI1,
		},
};
#endif

#if defined(CONFIG_HAVE_PWM)
#if defined(CONFIG_MX100) /* ktj */
static struct platform_pwm_backlight_data smdk_backlight_data = {
	.pwm_id  = 0,
	.max_brightness = 255,
	.dft_brightness = 255,
	.pwm_period_ns  = 40000,        // 1 cycle 25KHz
//	.pwm_period_ns  = 62500,        // 1 cycle 16KHz
//	.pwm_period_ns  = 71428,        // 1 cycle 14KHz
};

static struct platform_device smdk_backlight_device = {
	.name      = "pwm-backlight",
	.id        = -1,
	.dev        = {
		.parent = &s3c_device_timer[0].dev,
		.platform_data = &smdk_backlight_data,
	},
};

#else  // CONFIG_MX100 

static struct platform_pwm_backlight_data smdk_backlight_data = {
	.pwm_id  = 3,
	.max_brightness = 255,
	.dft_brightness = 255,
	.pwm_period_ns  = 78770,
};

static struct platform_device smdk_backlight_device = {
	.name      = "pwm-backlight",
	.id        = -1,
	.dev        = {
		.parent = &s3c_device_timer[3].dev,
		.platform_data = &smdk_backlight_data,
	},
};
#endif // CONFIG_MX100

static void __init smdk_backlight_register(void)
{
	int ret = platform_device_register(&smdk_backlight_device);
	if (ret)
		printk(KERN_ERR "smdk: failed to register backlight device: %d\n", ret);
}
#endif // CONFIG_HAVE_PWM


#if defined(CONFIG_BLK_DEV_IDE_S3C)
static struct s3c_ide_platdata smdkv210_ide_pdata __initdata = {
	.setup_gpio     = s3c_ide_setup_gpio,
};
#endif


#if 0 /* ktj disable */
#if defined(CONFIG_BATTERY_MAX17040)
#include <linux/max17040_battery.h>

static int max8677a_charger_enable(void)
{
    return 0;
}

static int max8677a_charger_online(void)
{
    if (gpio_get_value(GPIO_CHARGER_ONLINE))
        return 0;
    else    
        return 1;    
}

static int max8677a_battery_online(void)
{
	/*think that  battery always is inserted */
	return 1;
}

static int max8677a_charger_done(void)
{
    if (gpio_get_value(GPIO_CHARGER_STATUS))
        return 0;
    else    
        return 1;    
}

static void max8677a_charger_disable(void)
{

}

static struct max17040_platform_data max17040_data = {
	.battery_online = max8677a_battery_online,
	.charger_online = max8677a_charger_online,
	.charger_enable = max8677a_charger_enable,
	.charger_done   = max8677a_charger_done,
	.charger_disable = max8677a_charger_disable,
};
#endif /* CONFIG_BATTERY_MAX17040 */
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS

#define CYTTSP_I2C_ADDR		0x24 //0x24 ,0x67

static struct cyttsp_platform_data cyttsp_data = {
	.maxx = 480,
	.maxy = 800,
	.flags = 0,
//	.gen = CYTTSP_SPI10,	/* either */
//	.gen = CYTTSP_GEN2,	/* or */
	.gen = CYTTSP_GEN3,	/* or */
	.use_st = CYTTSP_USE_ST,
	.use_mt = CYTTSP_USE_MT,
	.use_trk_id = CYTTSP_USE_TRACKING_ID,
	.use_sleep = CYTTSP_USE_SLEEP,
	.use_gestures = CYTTSP_USE_GESTURES,
	/* activate up to 4 groups
	 * and set active distance
	 */
	.gest_set = CYTTSP_GEST_GRP1 | CYTTSP_GEST_GRP2 | 
				CYTTSP_GEST_GRP3 | CYTTSP_GEST_GRP4 | 
				CYTTSP_ACT_DIST,
	/* change act_intrvl to customize the Active power state 
	 * scanning/processing refresh interval for Operating mode
	 */
	.act_intrvl = CYTTSP_ACT_INTRVL_DFLT,
	/* change tch_tmout to customize the touch timeout for the
	 * Active power state for Operating mode
	 */
	.tch_tmout = CYTTSP_TCH_TMOUT_DFLT,
	/* change lp_intrvl to customize the Low Power power state 
	 * scanning/processing refresh interval for Operating mode
	 */
	.lp_intrvl = CYTTSP_LP_INTRVL_DFLT,
};

#endif

#ifdef CONFIG_TOUCHSCREEN_MELFAS
//#define MELFAS_I2C_7000_ADDR		0x47
#define MELFAS_I2C_7000_ADDR		0x20
#define MELFAS_I2C_8000_ADDR		0x48
#endif

/* I2C0 */
static struct i2c_board_info i2c_devs0[] __initdata = {

#if defined(CONFIG_RADIO_SI4709) && defined(CONFIG_MX100_EVM) /* ktj */
	{
		I2C_BOARD_INFO("Si4709", 0x20 >> 1),
	},
#endif
#ifdef CONFIG_SND_SOC_WM8580
	{
		I2C_BOARD_INFO("wm8580", 0x1b),
	},
#endif
#ifdef CONFIG_TOUCHSCREEN_CYPRESS
	{
		I2C_BOARD_INFO(CYTTSP_I2C_NAME, CYTTSP_I2C_ADDR),
		.platform_data = &cyttsp_data,
		#ifndef CYTTSP_USE_TIMER
		.irq = IRQ_EINT8,  // touch intterupt.
		//TROUT_GPIO_TO_INT(TROUT_GPIO_TP_ATT_N)
		#endif /* CYTTSP_USE_TIMER */
	},
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS
	{
		I2C_BOARD_INFO(MELFAS_7000_I2C_NAME, MELFAS_I2C_7000_ADDR),
		.irq = IRQ_EINT8,  // touch intterupt.
	},
	{
		I2C_BOARD_INFO(MELFAS_8000_I2C_NAME, MELFAS_I2C_8000_ADDR),
		.irq = IRQ_EINT8,  // touch intterupt.
	},
#endif
};

/* I2C1 */
static struct i2c_board_info i2c_devs1[] __initdata = {
#ifdef CONFIG_VIDEO_TV20
	{
		I2C_BOARD_INFO("s5p_ddc", (0x74>>1)),
	},
#endif
};

/* I2C2 */
static struct i2c_board_info i2c_devs2[] __initdata = {
#ifdef CONFIG_REGULATOR_MAX8698
	{
		/* The address is 0xCC used since SRAD = 0 */
		I2C_BOARD_INFO("max8698", (0xCC >> 1)),
		.platform_data = &max8698_platform_data,
	},
#endif
#ifdef CONFIG_SND_SOC_WM8993
	{
		I2C_BOARD_INFO("wm8993", 0x34 >> 1),
	},
#endif
#if defined(CONFIG_SENSORS_ISL29023) && defined(CONFIG_MX100_EVM) /* ktj */
	{
		I2C_BOARD_INFO("isl29023_als_ir", 0x88 >> 1),
		.irq = IRQ_EINT3,
	},
#endif
#if defined(CONFIG_BATTERY_MAX17040) && defined(CONFIG_MX100_EVM) /* ktj */
	{ 	
		I2C_BOARD_INFO("max17040", 0x36), 
//		.platform_data = &max17040_data,
	},
#endif
#if defined(CONFIG_HAPTIC_ISA1200) && defined(CONFIG_MX100_EVM) /* ktj */
	{
		I2C_BOARD_INFO("isa1200", 0x90 >> 1),
	},
#endif
};


//------------------------------------------------------------------------//
#ifdef CONFIG_INPUT_BMA150 /* ktj */
#define		GPIO_ACC_SDA	S5PV210_GPJ2(6)  
#define		GPIO_ACC_SCL	S5PV210_GPJ2(7)  

static struct 	i2c_gpio_platform_data 	i2c_acc_platdata = {
	.sda_pin = GPIO_ACC_SDA,
	.scl_pin = GPIO_ACC_SCL,
	.udelay	 = 2,	/* 250KHz */		
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0
};

static struct 	platform_device 	s5p_device_i2c_acc = {
	.name 	= "i2c-gpio",   // don't modify name, ktj
	.id  	= 4,            // adepter number
	.dev.platform_data = &i2c_acc_platdata,
};

/* I2C4 */
static struct i2c_board_info i2c_devs4[] __initdata = {
	{
		I2C_BOARD_INFO("bma150", (0x70 >> 1)),
	},
};
#endif // CONFIG_INPUT_BMA150

#ifdef CONFIG_SENSORS_AK8975 /* ktj */
#define		GPIO_COMPASS_SDA    S5PV210_GPJ3(0)    
#define		GPIO_COMPASS_SCL	S5PV210_GPJ3(1)

static struct 	i2c_gpio_platform_data 	i2c_compass_platdata = {
	.sda_pin = GPIO_COMPASS_SDA,
	.scl_pin = GPIO_COMPASS_SCL,
	.udelay	 = 2,	/* 250KHz */		
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0
};

static struct 	platform_device 	s5p_device_i2c_compass = {
	.name 	= "i2c-gpio",
	.id  	= 5,
	.dev.platform_data = &i2c_compass_platdata,
};

/* I2C5 */
static struct i2c_board_info i2c_devs5[] __initdata = {
	{
		I2C_BOARD_INFO("akm8975", 0x0C),   /* CAD1=0, CAD0=0 */
		.irq = IRQ_EINT(20),
	},
};
#endif // CONFIG_SENSORS_AK8975

#if defined(CONFIG_RADIO_SI4709) && defined(CONFIG_MX100_WS) /* ktj */
#define		GPIO_FM_SDA     S5PV210_GPJ1(3)    
#define		GPIO_FM_SCL	    S5PV210_GPJ1(4)

static struct 	i2c_gpio_platform_data 	i2c_fm_platdata = {
	.sda_pin = GPIO_FM_SDA,
	.scl_pin = GPIO_FM_SCL,
	.udelay	 = 2,	/* 250KHz */		
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0
};

static struct 	platform_device 	s5p_device_i2c_fm = {
	.name 	= "i2c-gpio",
	.id  	= 6,
	.dev.platform_data = &i2c_fm_platdata,
};

/* I2C6 */
static struct i2c_board_info i2c_devs6[] __initdata = {
	{
		I2C_BOARD_INFO("Si4709", 0x20 >> 1),
	},
};
#endif // CONFIG_RADIO_SI4709

#if defined(CONFIG_SENSORS_ISL29023) && defined(CONFIG_MX100_WS) /* ktj */
#define		GPIO_ALS_SDA        S5PV210_GPJ3(6)    
#define		GPIO_ALS_SCL	    S5PV210_GPJ3(7)

static struct 	i2c_gpio_platform_data 	i2c_als_platdata = {
	.sda_pin = GPIO_ALS_SDA,
	.scl_pin = GPIO_ALS_SCL,
	.udelay	 = 2,	/* 250KHz */		
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0
};

static struct 	platform_device 	s5p_device_i2c_als = {
	.name 	= "i2c-gpio",
	.id  	= 7,
	.dev.platform_data = &i2c_als_platdata,
};

/* I2C7 */
static struct i2c_board_info i2c_devs7[] __initdata = {
	{
		I2C_BOARD_INFO("isl29023_als_ir", 0x88 >> 1),
		.irq = IRQ_EINT3,
	},
};
#endif // CONFIG_SENSORS_ISL29023

#if defined(CONFIG_BATTERY_MAX17040) && defined(CONFIG_MX100_WS) /* ktj */
#define		GPIO_FUEL_SDA           S5PV210_GPJ4(0)    
#define		GPIO_FUEL_SCL	        S5PV210_GPJ4(1)

static struct 	i2c_gpio_platform_data 	i2c_fuel_platdata = {
	.sda_pin = GPIO_FUEL_SDA,
	.scl_pin = GPIO_FUEL_SCL,
	.udelay	 = 2,	/* 250KHz */		
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0
};

static struct 	platform_device 	s5p_device_i2c_fuel = {
	.name 	= "i2c-gpio",
	.id  	= 8,
	.dev.platform_data = &i2c_fuel_platdata,
};

/* I2C8 */
static struct i2c_board_info i2c_devs8[] __initdata = {
	{
		I2C_BOARD_INFO("max17040", 0x36), 
//		.platform_data = &max17040_data,
	},
};
#endif // CONFIG_BATTERY_MAX17040

#if defined(CONFIG_HAPTIC_ISA1200) && defined(CONFIG_MX100_WS) /* ktj */
#define		GPIO_HAPTIC_SDA         S5PV210_GPJ4(2)    
#define		GPIO_HAPTIC_SCL	        S5PV210_GPJ4(3)

static struct 	i2c_gpio_platform_data 	i2c_haptic_platdata = {
	.sda_pin = GPIO_HAPTIC_SDA,
	.scl_pin = GPIO_HAPTIC_SCL,
	.udelay	 = 2,	/* 250KHz */		
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0
};

static struct 	platform_device 	s5p_device_i2c_haptic = {
	.name 	= "i2c-gpio",
	.id  	= 9,
	.dev.platform_data = &i2c_haptic_platdata,
};

/* I2C9 */
static struct i2c_board_info i2c_devs9[] __initdata = {
	{
		I2C_BOARD_INFO("isa1200", 0x90 >> 1),
	},
};
#endif // CONFIG_HAPTIC_ISA1200

#if defined(CONFIG_SOC_CAMERA_MT9P111) && defined(CONFIG_MX100_WS)
#define		GPIO_CAM_SDA          S5PV210_GPJ3(4)   
#define		GPIO_CAM_SCL	      S5PV210_GPJ3(5)

static struct 	i2c_gpio_platform_data 	i2c_cam_platdata = {
	.sda_pin = GPIO_CAM_SDA,
	.scl_pin = GPIO_CAM_SCL,
	.udelay	 = 1,//400k for camera 2,	/* 250KHz */		
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0
};

static struct 	platform_device 	s5p_device_i2c_cam = {
	.name 	= "i2c-gpio",
	.id  	= 10,
	.dev.platform_data = &i2c_cam_platdata,
};


#endif // CONFIG_SOC_CAMERA_MT9P111

//------------------------------------------------------------------------//


#ifdef CONFIG_FB_S3C_TL2796

static struct s3c_platform_fb tl2796_data __initdata = {
	.hw_ver	= 0x62,
	.clk_name = "sclk_lcd",
	.nr_wins = 5,
	.default_win = CONFIG_FB_S3C_DEFAULT_WINDOW,
	.swap = FB_SWAP_HWORD | FB_SWAP_WORD,
};

#define LCD_BUS_NUM 	3

#define DISPLAY_CS	S5PV210_GPB(5)
#define DISPLAY_CLK	S5PV210_GPB(4)
#define DISPLAY_SI	S5PV210_GPB(7)

static struct spi_board_info spi_board_info[] __initdata = {
	{
		.modalias	= "tl2796",
		.platform_data	= NULL,
		.max_speed_hz	= 1200000,
		.bus_num	= LCD_BUS_NUM,
		.chip_select	= 0,
		.mode		= SPI_MODE_3,
		.controller_data = (void *)DISPLAY_CS,
	},
};

static struct spi_gpio_platform_data tl2796_spi_gpio_data = {
	.sck	= DISPLAY_CLK,
	.mosi	= DISPLAY_SI,
	.miso	= -1,
	.num_chipselect	= 1,
};

static struct platform_device s3c_device_spi_gpio = {
	.name	= "spi_gpio",
	.id	= LCD_BUS_NUM,
	.dev	= {
		.parent		= &s3c_device_fb.dev,
		.platform_data	= &tl2796_spi_gpio_data,
	},
};
#endif


#ifdef CONFIG_FB_S3C_LD070 /* ktj */
static struct s3c_platform_fb ld070_fb_data __initdata = {
	.hw_ver	= 0x62,
	.nr_wins = 5,
	.default_win = CONFIG_FB_S3C_DEFAULT_WINDOW,
	.swap = FB_SWAP_WORD | FB_SWAP_HWORD,
};

#define LCD_BUS_NUM 	3

#if 0 // change lcd spi port for T/P
#define DISPLAY_CS	S5PV210_GPH2(6)  // SPI_0_nSS
#define DISPLAY_CLK	S5PV210_GPC0(3)  // SPI_0_CLK
#define DISPLAY_SI	S5PV210_GPC0(4)  // SPI_0_MOSI
#else
#define DISPLAY_CS	S5PV210_GPB(1)  // SPI_0_nSS
#define DISPLAY_CLK	S5PV210_GPB(0)  // SPI_0_CLK
#define DISPLAY_SI	S5PV210_GPB(3)  // SPI_0_MOSI
#endif

static struct spi_board_info spi_board_info[] __initdata = {
	{
		.modalias	= "ld070",
		.platform_data	= NULL,
		.max_speed_hz	= 1200000,
		.bus_num	= LCD_BUS_NUM,
		.chip_select	= 0,
		.mode		= SPI_MODE_3,
		.controller_data = (void *)DISPLAY_CS,
	},
};

static struct spi_gpio_platform_data ld070_spi_gpio_data = {
	.sck	= DISPLAY_CLK,
	.mosi	= DISPLAY_SI,
	.miso	= -1,
	.num_chipselect	= 1,
};

static struct platform_device s3c_device_spi_gpio = {
	.name	= "spi_gpio",
	.id	= LCD_BUS_NUM,
	.dev	= {
		.parent		= &s3c_device_fb.dev,
		.platform_data	= &ld070_spi_gpio_data,
	},
};
#endif // CONFIG_FB_S3C_LD070


#ifdef CONFIG_DM9000
static void __init smdkv210_dm9000_set(void)
{
	unsigned int tmp;

	tmp = ((0<<28)|(0<<24)|(5<<16)|(0<<12)|(0<<8)|(0<<4)|(0<<0));
	__raw_writel(tmp, (S5P_SROM_BW+0x18));

	tmp = __raw_readl(S5P_SROM_BW);
	tmp &= ~(0xf << 20);

#ifdef CONFIG_DM9000_16BIT
	tmp |= (0x1 << 20);
#else
	tmp |= (0x2 << 20);
#endif
	__raw_writel(tmp, S5P_SROM_BW);

	tmp = __raw_readl(S5PV210_MP01CON);
	tmp &= ~(0xf << 20);
	tmp |= (2 << 20);

	__raw_writel(tmp, S5PV210_MP01CON);
}
#endif

#ifdef CONFIG_ANDROID_PMEM
static struct android_pmem_platform_data pmem_pdata = {
	.name = "pmem",
	.no_allocator = 1,
	.cached = 1,
	.start = 0, // will be set during proving pmem driver.
	.size = 0 // will be set during proving pmem driver.
};

static struct android_pmem_platform_data pmem_gpu1_pdata = {
   .name = "pmem_gpu1",
   .no_allocator = 1,
   .cached = 1,
   .buffered = 1,
   .start = 0,
   .size = 0,
};

static struct android_pmem_platform_data pmem_adsp_pdata = {
   .name = "pmem_adsp",
   .no_allocator = 1,
   .cached = 1,
   .buffered = 1,
   .start = 0,
   .size = 0,
};

static struct platform_device pmem_device = {
   .name = "android_pmem",
   .id = 0,
   .dev = { .platform_data = &pmem_pdata },
};

static struct platform_device pmem_gpu1_device = {
	.name = "android_pmem",
	.id = 1,
	.dev = { .platform_data = &pmem_gpu1_pdata },
};

static struct platform_device pmem_adsp_device = {
	.name = "android_pmem",
	.id = 2,
	.dev = { .platform_data = &pmem_adsp_pdata },
};

static void __init android_pmem_set_platdata(void)
{
	pmem_pdata.start = (u32)s3c_get_media_memory_bank(S3C_MDEV_PMEM, 0);
	pmem_pdata.size = (u32)s3c_get_media_memsize_bank(S3C_MDEV_PMEM, 0);

	pmem_gpu1_pdata.start = (u32)s3c_get_media_memory_bank(S3C_MDEV_PMEM_GPU1, 0);
	pmem_gpu1_pdata.size = (u32)s3c_get_media_memsize_bank(S3C_MDEV_PMEM_GPU1, 0);

	pmem_adsp_pdata.start = (u32)s3c_get_media_memory_bank(S3C_MDEV_PMEM_ADSP, 0);
	pmem_adsp_pdata.size = (u32)s3c_get_media_memsize_bank(S3C_MDEV_PMEM_ADSP, 0);
}
#endif
struct platform_device sec_device_battery = {
	.name	= "sec-fake-battery",
	.id		= -1,
};

#ifdef CONFIG_BT
#if 1 /* ktj_bt */
static struct platform_device	iriver_device_rfkill = {
	.name = "bt_rfkill",
	.id	  = -1,
};

#else
static struct bcm_bt_data s_bcm_bt_data = {
	.gpio_pwr	= S5PV210_GPJ0(2),
	.gpio_reset	= S5PV210_GPJ0(6),
	.reg_on = S5PV210_GPJ2(3),
};
static struct platform_device bcm_bt_device = {
	.name	= "bcm-bt",
	.id	= -1,
	.dev.platform_data = &s_bcm_bt_data,
};
#endif
#endif

#if defined(CONFIG_MX100_JACK)  /* JHLIM 2011.01.31 */

static struct platform_device mx100_device_jack = {
        .name           = "mx100_jack",
        .id             = -1,
        .dev            = {
                .platform_data  =NULL,
        },
};
#endif 


static struct platform_device *smdkv210_devices[] __initdata = {
	/* last enable to support regulator on/off in device driver */
	&s3c_device_i2c0,
	&s3c_device_i2c1,
	&s3c_device_i2c2,
#ifdef CONFIG_MTD_ONENAND
	&s3c_device_onenand,
#endif
#ifdef CONFIG_FB_S3C
	&s3c_device_fb,
#endif
	&s3c_device_keypad,
#ifdef CONFIG_TOUCHSCREEN_S3C
	&s3c_device_ts,
#endif
#ifdef CONFIG_S5PV210_ADC
	&s3c_device_adc,
#endif
#ifdef CONFIG_DM9000
	&s5p_device_dm9000,
#endif
#ifdef CONFIG_S3C2410_WATCHDOG
	&s3c_device_wdt,
#endif
#if defined(CONFIG_BLK_DEV_IDE_S3C)
	&s3c_device_cfcon,
#endif
#ifdef CONFIG_RTC_DRV_S3C
	&s5p_device_rtc,
#endif
#ifdef CONFIG_HAVE_PWM
	&s3c_device_timer[0],
	&s3c_device_timer[1],
	&s3c_device_timer[2],
	&s3c_device_timer[3],
#endif
#ifdef CONFIG_SND_S3C24XX_SOC
	&s3c64xx_device_iis0,
#endif
#ifdef CONFIG_SND_S3C_SOC_AC97
	&s3c64xx_device_ac97,
#endif
#ifdef CONFIG_SND_S3C_SOC_PCM
	&s3c64xx_device_pcm1,
#endif
#ifdef CONFIG_SND_S5P_SOC_SPDIF
	&s5p_device_spdif,
#endif
#ifdef CONFIG_VIDEO_FIMC
	&s3c_device_fimc0,
	&s3c_device_fimc1,
	&s3c_device_fimc2,
	&s3c_device_csis,
	&s3c_device_ipc,
#endif
#ifdef CONFIG_VIDEO_MFC50
	&s3c_device_mfc,
#endif

#ifdef CONFIG_VIDEO_JPEG_V2
	&s3c_device_jpeg,
#endif

#ifdef CONFIG_VIDEO_ROTATOR
	&s5p_device_rotator,
#endif

#ifdef CONFIG_USB
	&s3c_device_usb_ehci,
	&s3c_device_usb_ohci,
#endif
#ifdef CONFIG_USB_GADGET
	&s3c_device_usbgadget,
#endif
#ifdef CONFIG_USB_ANDROID
#ifdef CONFIG_USB_ANDROID_RNDIS
    &s3c_device_rndis,
#endif
	&s3c_device_android_usb,
	&s3c_device_usb_mass_storage,
#endif

#ifdef CONFIG_S3C_DEV_HSMMC
	&s3c_device_hsmmc0,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC1
	&s3c_device_hsmmc1,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	&s3c_device_hsmmc2,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC3
	&s3c_device_hsmmc3,
#endif

#ifdef CONFIG_SPI_CNTRLR_0
        &s3c_device_spi0,
#endif
#ifdef CONFIG_SPI_CNTRLR_1
        &s3c_device_spi1,
#endif

#ifdef CONFIG_VIDEO_TV20
	&s5p_device_tvout,
	&s5p_device_cec,
	&s5p_device_hpd,
#endif

#ifdef CONFIG_ANDROID_PMEM
	&pmem_device,
	&pmem_gpu1_device,
	&pmem_adsp_device,
#endif
	&sec_device_battery,
#ifdef CONFIG_VIDEO_G2D
	&s5p_device_g2d,
#endif
#ifdef CONFIG_FB_S3C_TL2796
	&s3c_device_spi_gpio,
#endif
#ifdef CONFIG_FB_S3C_LD070 /* ktj */
	&s3c_device_spi_gpio,
#endif

#ifdef CONFIG_INPUT_BMA150 /* ktj */
    &s5p_device_i2c_acc,
#endif
#ifdef CONFIG_SENSORS_AK8975 /* ktj */
    &s5p_device_i2c_compass,
#endif
#if defined(CONFIG_RADIO_SI4709) && defined(CONFIG_MX100_WS) /* ktj */
    &s5p_device_i2c_fm,
#endif
#if defined(CONFIG_SENSORS_ISL29023) && defined(CONFIG_MX100_WS) /* ktj */
    &s5p_device_i2c_als,
#endif
#if defined(CONFIG_BATTERY_MAX17040) && defined(CONFIG_MX100_WS) /* ktj */
    &s5p_device_i2c_fuel,
#endif
#if defined(CONFIG_HAPTIC_ISA1200) && defined(CONFIG_MX100_WS) /* ktj */
    &s5p_device_i2c_haptic,
#endif

#if defined(CONFIG_SOC_CAMERA_MT9P111) && defined(CONFIG_MX100_WS)
	&s5p_device_i2c_cam,
#endif

#if defined(CONFIG_BT)
    #if 1 /* ktj_bt */
	&iriver_device_rfkill,
	#else
	&bcm_bt_device,
    #endif
#endif

#if defined(CONFIG_BCM4329_MODULE)
    &sec_device_wifi,
#endif

#if defined(CONFIG_MX100_JACK)  /* JHLIM 2011.01.31 */
 &mx100_device_jack,
#endif

};

static void __init smdkv210_map_io(void)
{
	s5p_init_io(NULL, 0, S5P_VA_CHIPID);
	s3c24xx_init_clocks(24000000);
	s3c24xx_init_uarts(smdkv210_uartcfgs, ARRAY_SIZE(smdkv210_uartcfgs));
	s5pv210_reserve_bootmem();

#ifdef CONFIG_MTD_ONENAND
	s3c_device_onenand.name = "s5pc110-onenand";
#endif
}

#ifdef CONFIG_S3C_SAMSUNG_PMEM
static void __init s3c_pmem_set_platdata(void)
{
	pmem_pdata.start = s3c_get_media_memory_bank(S3C_MDEV_PMEM, 1);
	pmem_pdata.size = s3c_get_media_memsize_bank(S3C_MDEV_PMEM, 1);
}
#endif

#ifdef CONFIG_FB_S3C_LTE480WV
static struct s3c_platform_fb lte480wv_fb_data __initdata = {
	.hw_ver	= 0x62,
	.nr_wins = 5,
	.default_win = CONFIG_FB_S3C_DEFAULT_WINDOW,
	.swap = FB_SWAP_WORD | FB_SWAP_HWORD,
	.lcd_on = s3cfb_lcd_on,
};
#endif

#ifdef CONFIG_FB_S3C_LMS700 /* ktj */
static struct s3c_platform_fb lms700_fb_data __initdata = {
	.hw_ver	= 0x62,
	.nr_wins = 5,
	.default_win = CONFIG_FB_S3C_DEFAULT_WINDOW,
	.swap = FB_SWAP_WORD | FB_SWAP_HWORD,
	.lcd_on = s3cfb_lcd_on,
};
#endif

/* this function are used to detect s5pc110 chip version temporally */

int s5pc110_version ;

void _hw_version_check(void)
{
	void __iomem * phy_address ;
	int temp; 

	phy_address = ioremap (0x40,1);

	temp = __raw_readl(phy_address);


	if (temp == 0xE59F010C)
	{
		s5pc110_version = 0;
	}
	else
	{
		s5pc110_version=1 ;
	}
	printk("S5PC110 Hardware version : EVT%d \n",s5pc110_version);
	
	iounmap(phy_address);
}

/* Temporally used
 * return value 0 -> EVT 0
 * value 1 -> evt 1
 */

int hw_version_check(void)
{
	return s5pc110_version ;
}
EXPORT_SYMBOL(hw_version_check);

/*Shanghai ewada add for power off*/
#if defined(CONFIG_MX100_WS)
extern max_8698_poweroff();
#endif

static void smdkv210_power_off(void)
{
extern void power_off_wm8993(void);
/* JHLIM */
	power_off_wm8993();
	
#if defined(CONFIG_MX100_WS)
		max_8698_poweroff();
#endif

    /* ktj, added for charging */
#if defined(CONFIG_MX100_WS)
    if (!gpio_get_value(GPIO_CHARGER_ONLINE))
        arch_reset('s', NULL); 
#endif

	/* PS_HOLD --> Output Low */
	printk(KERN_EMERG "%s : setting GPIO_PDA_PS_HOLD low.\n", __func__);
	/* PS_HOLD output High --> Low  PS_HOLD_CONTROL, R/W, 0xE010_E81C */
	writel(readl(S5P_PSHOLD_CONTROL) & 0xFFFFFEFF, S5P_PSHOLD_CONTROL);

	arch_reset('s', NULL);
	while(1);

	printk(KERN_EMERG "%s : should not reach here!\n", __func__);
}

#ifdef CONFIG_VIDEO_TV20
void s3c_set_qos()
{
	/* VP QoS */
	__raw_writel(0x00400001, S5P_VA_DMC0 + 0xC8);
	__raw_writel(0x387F0022, S5P_VA_DMC0 + 0xCC);
	/* MIXER QoS */
	__raw_writel(0x00400001, S5P_VA_DMC0 + 0xD0);
	__raw_writel(0x3FFF0062, S5P_VA_DMC0 + 0xD4);
	/* LCD1 QoS */
	__raw_writel(0x00800001, S5P_VA_DMC1 + 0x90);
	__raw_writel(0x3FFF005B, S5P_VA_DMC1 + 0x94);
	/* LCD2 QoS */
	__raw_writel(0x00800001, S5P_VA_DMC1 + 0x98);
	__raw_writel(0x3FFF015B, S5P_VA_DMC1 + 0x9C);
	/* VP QoS */
	__raw_writel(0x00400001, S5P_VA_DMC1 + 0xC8);
	__raw_writel(0x387F002B, S5P_VA_DMC1 + 0xCC);
	/* DRAM Controller QoS */
	__raw_writel((__raw_readl(S5P_VA_DMC0)&~(0xFFF<<16)|(0x100<<16)),
			S5P_VA_DMC0 + 0x0);
	__raw_writel((__raw_readl(S5P_VA_DMC1)&~(0xFFF<<16)|(0x100<<16)),
			S5P_VA_DMC1 + 0x0);
	/* BUS QoS AXI_DSYS Control */
	__raw_writel(0x00000007, S5P_VA_BUS_AXI_DSYS + 0x400);
	__raw_writel(0x00000007, S5P_VA_BUS_AXI_DSYS + 0x420);
	__raw_writel(0x00000030, S5P_VA_BUS_AXI_DSYS + 0x404);
	__raw_writel(0x00000030, S5P_VA_BUS_AXI_DSYS + 0x424);
}
#endif

#ifdef CONFIG_MX100_REV_TP

/*  JHL 2011.02.25  */
int g_uart2_port=1;  // 1:GPS 0 :debug

static int __init uart2_device_setting(char *uarg2_setting)
{
	if(uarg2_setting) {
		if(strcmp(uarg2_setting,"on")==0) {
			g_uart2_port = 0;
		}  else if(strcmp(uarg2_setting,"off")==0) {
			g_uart2_port = 1;
		}
	} 
	return 1;  /*JHLIM remove warning */
}
__setup("uart2debug=", uart2_device_setting);
#endif


/* ktj */
extern int mx100_cdma_power(int status);

#ifdef CONFIG_MX100_REV_TP

/*  JHL 2011.02.28  */
int g_cdma_pclink_mode=0;

static int __init cdma_mode_setting(char *cdma_setting)
{
	if(cdma_setting) {
		if(strcmp(cdma_setting,"on")==0) {
			g_cdma_pclink_mode = 1;
		}  else {
			g_cdma_pclink_mode = 0;
		}
	} 
	return 1; /*JHLIM remove warning */
}
__setup("cdmalink=", cdma_mode_setting);
#endif

void s3c_pre_config_sleep_gpio(void);

void iriver_init_gpio(void)
{
#ifdef CONFIG_MX100_WS
    s3c_gpio_cfgpin(GPIO_WLAN_nRST, S3C_GPIO_OUTPUT);
    s3c_gpio_setpull(GPIO_WLAN_nRST, S3C_GPIO_PULL_NONE);
    gpio_set_value(GPIO_WLAN_nRST, GPIO_LEVEL_LOW);

    s3c_gpio_cfgpin(GPIO_BT_nRST, S3C_GPIO_OUTPUT);
    s3c_gpio_setpull(GPIO_BT_nRST, S3C_GPIO_PULL_NONE);
    gpio_set_value(GPIO_BT_nRST, GPIO_LEVEL_LOW);
#endif
    
#if defined(CONFIG_MX100_REV_TP)
   /* added 2011.02.28 JHLIM  select cdma pclink mode from boot menu.*/

    if (g_cdma_pclink_mode==1) {
	    printk(KERN_ERR "CDMA USB is PCLINK MODE !\n");
	    s3c_gpio_cfgpin(GPIO_CDMA_USB_ON_OUT, S3C_GPIO_OUTPUT);
    		s3c_gpio_setpull(GPIO_CDMA_USB_ON_OUT, S3C_GPIO_PULL_NONE);
	    gpio_set_value(GPIO_CDMA_USB_ON_OUT, GPIO_LEVEL_HIGH);

	    s3c_gpio_cfgpin(GPIO_USB_SW, S3C_GPIO_OUTPUT);
    		s3c_gpio_setpull(GPIO_USB_SW, S3C_GPIO_PULL_NONE);
	    gpio_set_value(GPIO_USB_SW, GPIO_LEVEL_HIGH);

		s3c_gpio_cfgpin(GPIO_USB_SW_OE, S3C_GPIO_OUTPUT);
	    s3c_gpio_setpull(GPIO_USB_SW_OE, S3C_GPIO_PULL_NONE);
	    gpio_set_value(GPIO_USB_SW_OE, GPIO_LEVEL_HIGH);
    } else {
	    printk(KERN_ERR "CDMA USB is Normal MODE !\n");
//	    gpio_set_value(GPIO_CDMA_USB_ON_OUT, GPIO_LEVEL_LOW);
//	    gpio_set_value(GPIO_USB_SW, GPIO_LEVEL_LOW);
	    s3c_gpio_cfgpin(GPIO_USB_SW_OE, S3C_GPIO_OUTPUT);
	    s3c_gpio_setpull(GPIO_USB_SW_OE, S3C_GPIO_PULL_NONE);
	    gpio_set_value(GPIO_USB_SW_OE, GPIO_LEVEL_HIGH);
     }
#endif    

//added 12.07
//jhlim 2011.02.25 : moved from mx100_ioctl.c to here.

#ifdef CONFIG_MX100_WS
//for cdma modem control==================================================

	if (gpio_is_valid(GPIO_USBHOST_CHARGE_IC_OUT)) {
		if (gpio_request(GPIO_USBHOST_CHARGE_IC_OUT, "GPIO_USBHOST_CHARGE_IC_OUT")) 
			printk(KERN_ERR "Failed to request GPIO_USBHOST_CHARGE_IC_OUT!\n");

		s3c_gpio_setpull(GPIO_USBHOST_CHARGE_IC_OUT, S3C_GPIO_PULL_UP);
		gpio_direction_output(GPIO_USBHOST_CHARGE_IC_OUT, GPIO_LEVEL_LOW);
	}

	if (gpio_is_valid(GPIO_CDMA_PWR_ON_OUT)) {
		if (gpio_request(GPIO_CDMA_PWR_ON_OUT, "GPJ2")) 
			printk(KERN_ERR "Failed to request GPIO_CDMA_PWR_ON_OUT!\n");
		gpio_direction_output(GPIO_CDMA_PWR_ON_OUT, GPIO_LEVEL_LOW);
	}
	s3c_gpio_setpull(GPIO_CDMA_PWR_ON_OUT, S3C_GPIO_PULL_DOWN);

	if (gpio_is_valid(GPIO_CDMA_RESET_OUT)) {
		if (gpio_request(GPIO_CDMA_RESET_OUT, "GPJ2")) 
			printk(KERN_ERR "Failed to request GPIO_CDMA_RESET_OUT!\n");
		gpio_direction_output(GPIO_CDMA_RESET_OUT, GPIO_LEVEL_LOW);
	}
	s3c_gpio_setpull(GPIO_CDMA_RESET_OUT, S3C_GPIO_PULL_NONE);

	if (gpio_is_valid(GPIO_CDMA_READY_SIGNAL_IN)) {
		if (gpio_request(GPIO_CDMA_READY_SIGNAL_IN, "GPJ2")) 
			printk(KERN_ERR "Failed to request GPIO_CDMA_READY_SIGNAL_IN!\n");
		
		gpio_direction_input(GPIO_CDMA_READY_SIGNAL_IN);
	}
	s3c_gpio_setpull(GPIO_CDMA_READY_SIGNAL_IN, S3C_GPIO_PULL_DOWN);

	if (gpio_is_valid(GPIO_CDMA_SLEEP_CTL_OUT)) {
		if (gpio_request(GPIO_CDMA_SLEEP_CTL_OUT, "GPJ2")) 
			printk(KERN_ERR "Failed to request GPIO_CDMA_SLEEP_CTL_OUT!\n");
		
		gpio_direction_output(GPIO_CDMA_SLEEP_CTL_OUT,1);
	}
	s3c_gpio_setpull(GPIO_CDMA_SLEEP_CTL_OUT, S3C_GPIO_PULL_UP);
#endif

#ifdef CONFIG_MX100_WS
// charger ic test.
    /* ktj, fix build error on evm */
	if (gpio_is_valid(GPIO_FULL_BATT_REQ_IN)) {
		if (gpio_request(GPIO_FULL_BATT_REQ_IN, "U_GPIO_FULL_BATT_REQ_IN")) 
			printk(KERN_ERR "Failed to request U_GPIO_FULL_BATT_REQ_IN!\n");
		gpio_direction_input(GPIO_FULL_BATT_REQ_IN);
		s3c_gpio_setpull(GPIO_FULL_BATT_REQ_IN, S3C_GPIO_PULL_NONE);		
	}

	if (gpio_is_valid(GPIO_DC_APT_DETECT_IN)) {
		if (gpio_request(GPIO_DC_APT_DETECT_IN, "GPIO_DC_APT_DETECT_IN")) 
			printk(KERN_ERR "Failed to request GPIO_DC_APT_DETECT_IN!\n");
		gpio_direction_input(GPIO_DC_APT_DETECT_IN);
		s3c_gpio_setpull(GPIO_DC_APT_DETECT_IN, S3C_GPIO_PULL_NONE);		
	}

	if (gpio_is_valid(GPIO_USB_INSERT_IN)) {
		if (gpio_request(GPIO_USB_INSERT_IN, "GPIO_FULL_BATT_REQ_IN")) 
			printk(KERN_ERR "Failed to request GPIO_FULL_BATT_REQ_IN!\n");
		gpio_direction_input(GPIO_USB_INSERT_IN);
		s3c_gpio_setpull(GPIO_USB_INSERT_IN, S3C_GPIO_PULL_NONE);		
	}
#endif

#ifdef CONFIG_MX100_REV_TP
//JHLIM 2011.02.25  
//UART2 GPS[HIGH], DEBUG[LOW]
//
 	if (gpio_is_valid(GPIO_GPS_DEBUG_SELECT_OUT)) {
		if (gpio_request(GPIO_GPS_DEBUG_SELECT_OUT, "GPIO_GPS_DEBUG_SELECT_OUT")) 
			printk(KERN_ERR "Failed to request GPIO_CDMA_SLEEP_CTL_OUT!\n");
		
//#if 0 // ktj_gps
//		printk(KERN_ERR "UART2 is SERIAL DEBUG!\n");
//		gpio_direction_output(GPIO_GPS_DEBUG_SELECT_OUT, 0); // default debug.
//#else

		if(g_uart2_port==1) {	// gps.	
			printk(KERN_ERR "UART2 is GPS!\n");
			gpio_direction_output(GPIO_GPS_DEBUG_SELECT_OUT,1); // gps
		} else {  // debug
			printk(KERN_ERR "UART2 is SERIAL DEBUG!\n");
			gpio_direction_output(GPIO_GPS_DEBUG_SELECT_OUT,0); // default debug.
		}
//#endif
	}


       // CDMA cm port DTR
 	if (gpio_is_valid(GPIO_UART1_DTR_OUT)) {
		if (gpio_request(GPIO_UART1_DTR_OUT, "GPIO_UART1_DTR_OUT")) 
			printk(KERN_ERR "Failed to request GPIO_UART1_DTR_OUT!\n");

		gpio_direction_output(GPIO_UART1_DTR_OUT,0); // active low.
	}


 	if (gpio_is_valid(GPIO_DEBUG_OUT)) {
		if (gpio_request(GPIO_DEBUG_OUT, "GPIO_DEBUG_OUT")) 
			printk(KERN_ERR "Failed to request GPIO_DEBUG_OUT!\n");

		gpio_direction_output(GPIO_DEBUG_OUT,0); // active low.
	}
	
#endif

    /* ktj set pull */	
    s3c_gpio_setpull(GPIO_BT_HOST_WALKUP_IN,    S3C_GPIO_PULL_UP);
    s3c_gpio_setpull(GPIO_WIFI_WAKEUP_IN,       S3C_GPIO_PULL_DOWN);

//[yoon 20110504]changed cdma power squency : iriver_init_gpio() -> cdma_manager_probe()
#if 0
	mx100_cdma_power(0); 
#endif
}

//JHLIM added 2011.04.03 : get mx100 hw event ;
char  g_mx100_hw_name[8] = {'U','N','K','N','O','W','N'};  // ES,TP,MP,UNKNOWN

EXPORT_SYMBOL(g_mx100_hw_name);

static int __init get_mx100_hw_name(char * mx100_hw_name)
{
	if(mx100_hw_name) {
		strcpy(g_mx100_hw_name,mx100_hw_name);
	} 
	return 1;
	/*JHLIM remove warning */
}
__setup("mx100_hw=", get_mx100_hw_name);

char  g_mx100_mp_mode[8] = {'n','o','r','m','a','l' };  // normal,process  

static int __init get_mx100_mp_mode(char * mx100_mp_mode)
{
	if(mx100_mp_mode) {
		strcpy(g_mx100_mp_mode,mx100_mp_mode);
	}  else {
		strcpy(g_mx100_mp_mode,"normal");
	}
	return 1; /*JHLIM remove warning */
}
__setup("mpmode=", get_mx100_mp_mode);


static void __init smdkv210_machine_init(void)
{
	/* Find out S5PC110 chip version */
	_hw_version_check();

	printk("\nMX100 Hardware version : %s \n",g_mx100_hw_name);  /* JHLIM  */
	
    /* ktj init gpio */
	iriver_init_gpio();

	/* OneNAND */
#ifdef CONFIG_MTD_ONENAND
	//s3c_device_onenand.dev.platform_data = &s5p_onenand_data;
#endif

#ifdef CONFIG_DM9000
	smdkv210_dm9000_set();
#endif

#ifdef CONFIG_ANDROID_PMEM
	android_pmem_set_platdata();
#endif
	/* i2c */
	s3c_i2c0_set_platdata(NULL);
	s3c_i2c1_set_platdata(NULL);
	s3c_i2c2_set_platdata(NULL);
	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));
	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));
	i2c_register_board_info(2, i2c_devs2, ARRAY_SIZE(i2c_devs2));
#ifdef CONFIG_INPUT_BMA150 /* ktj */
	i2c_register_board_info(4, i2c_devs4, ARRAY_SIZE(i2c_devs4));
#endif
#ifdef CONFIG_SENSORS_AK8975 /* ktj */
	i2c_register_board_info(5, i2c_devs5, ARRAY_SIZE(i2c_devs5));
#endif
#if defined(CONFIG_MX100_WS) /* ktj */
	i2c_register_board_info(6, i2c_devs6, ARRAY_SIZE(i2c_devs6));
	i2c_register_board_info(7, i2c_devs7, ARRAY_SIZE(i2c_devs7));
#if defined(CONFIG_BATTERY_MAX17040)
	i2c_register_board_info(8, i2c_devs8, ARRAY_SIZE(i2c_devs8));
#endif
	i2c_register_board_info(9, i2c_devs9, ARRAY_SIZE(i2c_devs9));
	
#endif

	/*Shanghai ewada add for cdma poweron fiirst*/
#ifdef MX100_CDMA_POWERON_FIRST
	mx100_cdma_power_onoff(0);
#endif

	/* to support system shut down */
	pm_power_off = smdkv210_power_off;
#if defined(CONFIG_SPI_CNTRLR_0)
        s3cspi_set_slaves(BUSNUM(0), ARRAY_SIZE(s3c_slv_pdata_0), s3c_slv_pdata_0);
#endif
#if defined(CONFIG_SPI_CNTRLR_1)
        s3cspi_set_slaves(BUSNUM(1), ARRAY_SIZE(s3c_slv_pdata_1), s3c_slv_pdata_1);
#endif
#if defined(CONFIG_SPI_CNTRLR_2)
        s3cspi_set_slaves(BUSNUM(2), ARRAY_SIZE(s3c_slv_pdata_2), s3c_slv_pdata_2);
#endif
#if defined(CONFIG_MX100) /* ktj tdmb */
	spi_register_board_info(Tdmb_board_info, ARRAY_SIZE(Tdmb_board_info));
#endif
  spi_register_board_info(s3c_spi_devs, ARRAY_SIZE(s3c_spi_devs));

#ifdef CONFIG_FB_S3C_LTE480WV
	s3cfb_set_platdata(&lte480wv_fb_data);
#endif

#ifdef CONFIG_FB_S3C_LMS700 /* ktj */
	s3cfb_set_platdata(&lms700_fb_data);
#endif

#ifdef CONFIG_FB_S3C_LD070  /* ktj */
	spi_register_board_info(spi_board_info, ARRAY_SIZE(spi_board_info));
	s3cfb_set_platdata(&ld070_fb_data);
#endif

#ifdef CONFIG_FB_S3C_TL2796
	spi_register_board_info(spi_board_info, ARRAY_SIZE(spi_board_info));
	s3cfb_set_platdata(&tl2796_data);
#endif


#if defined(CONFIG_BLK_DEV_IDE_S3C)
	s3c_ide_set_platdata(&smdkv210_ide_pdata);
#endif

#if defined(CONFIG_TOUCHSCREEN_S3C)
	s3c_ts_set_platdata(&s3c_ts_platform);
#endif

#if defined(CONFIG_S5PV210_ADC)
	s3c_adc_set_platdata(&s3c_adc_platform);
#endif

#if defined(CONFIG_PM)
	s3c_pm_init();
#endif
#ifdef CONFIG_VIDEO_FIMC
	/* fimc */
	s3c_fimc0_set_platdata(&fimc_plat);
	s3c_fimc1_set_platdata(&fimc_plat);
	s3c_fimc2_set_platdata(&fimc_plat);
	s3c_csis_set_platdata(NULL);
	
#ifndef CONFIG_MX100
	smdkv210_cam0_power(0);
	smdkv210_cam1_power(0);
#endif

	/*Shanghai ewada camera power*/
#if CONFIG_MX100
	cam_hw_config_gpio();
#endif

	
	smdkv210_mipi_cam_reset();
#endif
#ifdef CONFIG_VIDEO_JPEG_V2
	s3c_jpeg_set_platdata(&jpeg_plat);
#endif

#ifdef CONFIG_VIDEO_MFC50
	/* mfc */
	s3c_mfc_set_platdata(NULL);
#endif

#ifdef CONFIG_VIDEO_TV20
	s3c_set_qos();
#endif

#ifdef CONFIG_S3C_DEV_HSMMC
	s5pv210_default_sdhci0();
#endif
#ifdef CONFIG_S3C_DEV_HSMMC1
	s5pv210_default_sdhci1();
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	s5pv210_default_sdhci2();
#endif
#ifdef CONFIG_S3C_DEV_HSMMC3
	s5pv210_default_sdhci3();
#endif
#ifdef CONFIG_S5PV210_SETUP_SDHCI
	s3c_sdhci_set_platdata();
#endif
	platform_add_devices(smdkv210_devices, ARRAY_SIZE(smdkv210_devices));
#if defined(CONFIG_HAVE_PWM)
	smdk_backlight_register();
#endif
#if defined(CONFIG_BCM4329_MODULE)
	mx100_init_wifi_mem();
#endif

	iriver_class = class_create(THIS_MODULE, "iriver");
	if (IS_ERR(iriver_class))
		pr_err("Failed to create class!\n");
	
}

#ifdef CONFIG_USB_SUPPORT
/* Initializes OTG Phy. */
void otg_phy_init(void)
{
	__raw_writel(__raw_readl(S5P_USB_PHY_CONTROL)
		|(0x1<<0), S5P_USB_PHY_CONTROL); /*USB PHY0 Enable */
	__raw_writel((__raw_readl(S3C_USBOTG_PHYPWR)
		&~(0x3<<3)&~(0x1<<0))|(0x1<<5), S3C_USBOTG_PHYPWR);
	__raw_writel((__raw_readl(S3C_USBOTG_PHYCLK)
		&~(0x5<<2))|(0x3<<0), S3C_USBOTG_PHYCLK);
	__raw_writel((__raw_readl(S3C_USBOTG_RSTCON)
		&~(0x3<<1))|(0x1<<0), S3C_USBOTG_RSTCON);
	udelay(10);
	__raw_writel(__raw_readl(S3C_USBOTG_RSTCON)
		&~(0x7<<0), S3C_USBOTG_RSTCON);
	udelay(10);
}
EXPORT_SYMBOL(otg_phy_init);

/* USB Control request data struct must be located here for DMA transfer */
//struct usb_ctrlrequest usb_ctrl __attribute__((aligned(8)));

/* OTG PHY Power Off */
void otg_phy_off(void)
{
	__raw_writel(__raw_readl(S3C_USBOTG_PHYPWR)
		|(0x3<<3), S3C_USBOTG_PHYPWR);
	__raw_writel(__raw_readl(S5P_USB_PHY_CONTROL)
		&~(1<<0), S5P_USB_PHY_CONTROL);
}
EXPORT_SYMBOL(otg_phy_off);

void usb_host_phy_init(void)
{
	struct clk *otg_clk;

	otg_clk = clk_get(NULL, "usbotg");
	clk_enable(otg_clk);

	if (readl(S5P_USB_PHY_CONTROL) & (0x1<<1))
		return;

	__raw_writel(__raw_readl(S5P_USB_PHY_CONTROL)
		|(0x1<<1), S5P_USB_PHY_CONTROL);
	__raw_writel((__raw_readl(S3C_USBOTG_PHYPWR)
		&~(0x1<<7)&~(0x1<<6))|(0x1<<8)|(0x1<<5), S3C_USBOTG_PHYPWR);
	__raw_writel((__raw_readl(S3C_USBOTG_PHYCLK)
		&~(0x1<<7))|(0x3<<0), S3C_USBOTG_PHYCLK);
	__raw_writel((__raw_readl(S3C_USBOTG_RSTCON))
		|(0x1<<4)|(0x1<<3), S3C_USBOTG_RSTCON);
	__raw_writel(__raw_readl(S3C_USBOTG_RSTCON)
		&~(0x1<<4)&~(0x1<<3), S3C_USBOTG_RSTCON);
}
EXPORT_SYMBOL(usb_host_phy_init);

void usb_host_phy_off(void)
{
	__raw_writel(__raw_readl(S3C_USBOTG_PHYPWR)
		|(0x1<<7)|(0x1<<6), S3C_USBOTG_PHYPWR);
	__raw_writel(__raw_readl(S5P_USB_PHY_CONTROL)
		&~(1<<1), S5P_USB_PHY_CONTROL);
}
EXPORT_SYMBOL(usb_host_phy_off);
#endif

#if defined(CONFIG_KEYPAD_S3C) || defined(CONFIG_KEYPAD_S3C_MODULE)
#if defined(CONFIG_KEYPAD_S3C_MSM)
void s3c_setup_keypad_cfg_gpio(void)
{
	unsigned int gpio;
	unsigned int end;

	/* gpio setting for KP_COL0 */
	s3c_gpio_cfgpin(S5PV210_GPJ1(5), S3C_GPIO_SFN(3));
	s3c_gpio_setpull(S5PV210_GPJ1(5), S3C_GPIO_PULL_NONE);

	/* gpio setting for KP_COL1 ~ KP_COL7 and KP_ROW0 */
	end = S5PV210_GPJ2(8);
	for (gpio = S5PV210_GPJ2(0); gpio < end; gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	}

	/* gpio setting for KP_ROW1 ~ KP_ROW8 */
	end = S5PV210_GPJ3(8);
	for (gpio = S5PV210_GPJ3(0); gpio < end; gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	}

	/* gpio setting for KP_ROW9 ~ KP_ROW13 */
	end = S5PV210_GPJ4(5);
	for (gpio = S5PV210_GPJ4(0); gpio < end; gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	}
}
#else
void s3c_setup_keypad_cfg_gpio(int rows, int columns)
{
	unsigned int gpio;
	unsigned int end;

	end = S5PV210_GPH3(rows);

	/* Set all the necessary GPH2 pins to special-function 0 */
	for (gpio = S5PV210_GPH3(0); gpio < end; gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
	}

	end = S5PV210_GPH2(columns);

	/* Set all the necessary GPK pins to special-function 0 */
	for (gpio = S5PV210_GPH2(0); gpio < end; gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	}
}
#endif /* if defined(CONFIG_KEYPAD_S3C_MSM)*/
EXPORT_SYMBOL(s3c_setup_keypad_cfg_gpio);
#endif


#if defined(CONFIG_MX100_WS) /* ktj */

#define SLEEP_GPIO_LEVEL    GPIO_LEVEL_HIGH    
#define SLEEP_GPIO_CFG      S3C_GPIO_OUTPUT  
#define SLEEP_GPIO_PULL     S3C_GPIO_PULL_NONE     

void set_gpio_config(void *reg, int port, int data)
{
	unsigned int gpa0_bt_con;
//	unsigned int gpa0_bt_pud;

    if (data == 0)
    {    
    	gpa0_bt_con = __raw_readl(reg);
	    gpa0_bt_con &= ~(1 << ((port * 2) + 1));
	    gpa0_bt_con &= ~(1 << (port * 2));
	    __raw_writel(gpa0_bt_con, reg);
    }
    else if (data == 1)
    {    
    	gpa0_bt_con = __raw_readl(reg);
	    gpa0_bt_con &= ~(1 << ((port * 2) + 1));
	    gpa0_bt_con |=  (1 << (port * 2));
	    __raw_writel(gpa0_bt_con, reg);
    }
    else if (data == 2)
    {    
    	gpa0_bt_con = __raw_readl(reg);
	    gpa0_bt_con |=  (1 << ((port * 2) + 1));
	    gpa0_bt_con &= ~(1 << (port * 2));
	    __raw_writel(gpa0_bt_con, reg);
    }
    else if (data == 3)
    {    
    	gpa0_bt_con = __raw_readl(reg);
	    gpa0_bt_con |=  (1 << ((port * 2) + 1));
	    gpa0_bt_con |=  (1 << (port * 2));
	    __raw_writel(gpa0_bt_con, reg);
    }
}

#define GPIO_CON_PDN    1       // Output High
#define GPIO_CON_PUD    2       // Pull Up Enabled

#define GPIO_BT_WAKE          	S5PV210_GPJ0(2)
#define GPIO_BT_nRST            S5PV210_GPJ0(6)
#define GPIO_WLAN_BT_EN         S5PV210_GPJ2(3)
#define GPIO_WLAN_nRST          S5PV210_GPJ0(5)
#define GPIO_WLAN_BT_CLK_EN     S5PV210_GPJ0(1)
#define GPIO_HAPTIC_LEN         S5PV210_GPJ3(2)
#define GPIO_HAPTIC_HEN         S5PV210_GPJ3(3)

void s3c_pre_config_sleep_gpio(void)
{

//  === GPA0 === 

	// bt uart0
	        s3c_gpio_slp_cfgpin(S5PV210_GPA0(0), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPA0(0), S3C_GPIO_PULL_UP);

	        s3c_gpio_slp_cfgpin(S5PV210_GPA0(1), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPA0(1), S3C_GPIO_PULL_UP);

	        s3c_gpio_slp_cfgpin(S5PV210_GPA0(2), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPA0(2), S3C_GPIO_PULL_UP);

	        s3c_gpio_slp_cfgpin(S5PV210_GPA0(3), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPA0(3), S3C_GPIO_PULL_UP);

#if 1
    // cdma uart1
	        s3c_gpio_slp_cfgpin(S5PV210_GPA0(4), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPA0(4), S3C_GPIO_PULL_UP);

	        s3c_gpio_slp_cfgpin(S5PV210_GPA0(5), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPA0(5), S3C_GPIO_PULL_UP);

	        s3c_gpio_slp_cfgpin(S5PV210_GPA0(6), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPA0(6), S3C_GPIO_PULL_UP);

	        s3c_gpio_slp_cfgpin(S5PV210_GPA0(7), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPA0(7), S3C_GPIO_PULL_UP);
#endif

//  === GPA1 === 

    // gps(tx,rx) debug(rx,tx)
	        s3c_gpio_slp_cfgpin(S5PV210_GPA1(0), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPA1(0), S3C_GPIO_PULL_UP);

	        s3c_gpio_slp_cfgpin(S5PV210_GPA1(1), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPA1(1), S3C_GPIO_PULL_UP);

#if 1
    // cdma uart3
	        s3c_gpio_slp_cfgpin(S5PV210_GPA1(2), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPA1(2), S3C_GPIO_PULL_UP);

	        s3c_gpio_slp_cfgpin(S5PV210_GPA1(3), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPA1(3), S3C_GPIO_PULL_UP);
#endif

//  === GPB  === 

    // LCD SPI Port (b0, b1, b3)      
	        s3c_gpio_slp_cfgpin(GPIO_LCD_CLK, S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(GPIO_LCD_CLK, S3C_GPIO_PULL_NONE);

	        s3c_gpio_slp_cfgpin(GPIO_LCD_CS, S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(GPIO_LCD_CS, S3C_GPIO_PULL_NONE);

#if 1
    // NC
	        s3c_gpio_slp_cfgpin(S5PV210_GPB(2), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPB(2), S3C_GPIO_PULL_NONE);
#endif

	        s3c_gpio_slp_cfgpin(GPIO_LCD_SI, S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(GPIO_LCD_SI, S3C_GPIO_PULL_NONE);
        
#if 1
    // dmb spi (4,5,6,7)
	s3c_gpio_slp_cfgpin(S5PV210_GPB(4), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPB(4), S3C_GPIO_PULL_NONE);

	s3c_gpio_slp_cfgpin(S5PV210_GPB(5), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPB(5), S3C_GPIO_PULL_NONE);

	s3c_gpio_slp_cfgpin(S5PV210_GPB(6), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPB(6), S3C_GPIO_PULL_DOWN);

	s3c_gpio_slp_cfgpin(S5PV210_GPB(7), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPB(7), S3C_GPIO_PULL_NONE);
#endif

//  === GPI  === 

#if 1
    // wm8993 i2s (GPI 0,1,2,3,4)
	s3c_gpio_slp_cfgpin(S5PV210_GPI(0), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPI(0), S3C_GPIO_PULL_NONE);

	s3c_gpio_slp_cfgpin(S5PV210_GPI(1), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPI(1), S3C_GPIO_PULL_NONE);

	s3c_gpio_slp_cfgpin(S5PV210_GPI(2), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPI(2), S3C_GPIO_PULL_NONE);

	s3c_gpio_slp_cfgpin(S5PV210_GPI(3), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPI(3), S3C_GPIO_PULL_DOWN);

	s3c_gpio_slp_cfgpin(S5PV210_GPI(4), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPI(4), S3C_GPIO_PULL_NONE);
#endif    


#if 1 
    // NC
	s3c_gpio_slp_cfgpin(S5PV210_GPI(5), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPI(5), S3C_GPIO_PULL_NONE);

	s3c_gpio_slp_cfgpin(S5PV210_GPI(6), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPI(6), S3C_GPIO_PULL_NONE);
#endif

// GPC0 0~4
#if 1
    // KEY_SEARCH
	        s3c_gpio_slp_cfgpin(S5PV210_GPC0(0), S3C_GPIO_SLP_OUT0   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPC0(0), S3C_GPIO_PULL_NONE  );

    // KEY_MENU
	        s3c_gpio_slp_cfgpin(S5PV210_GPC0(1), S3C_GPIO_SLP_OUT0   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPC0(1), S3C_GPIO_PULL_NONE  );

    // KEY_HOME
	        s3c_gpio_slp_cfgpin(S5PV210_GPC0(2), S3C_GPIO_SLP_OUT0   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPC0(2), S3C_GPIO_PULL_NONE  );

	// UART_AN_SW, gps_debug_select
//	        s3c_gpio_slp_cfgpin(S5PV210_GPC0(3), S3C_GPIO_SLP_OUT0   );
//  s3c_gpio_slp_setpull_updown(S5PV210_GPC0(3), S3C_GPIO_PULL_NONE  );

    // CDMA_USB_ON
	        s3c_gpio_slp_cfgpin(S5PV210_GPC0(4), S3C_GPIO_SLP_OUT0   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPC0(4), S3C_GPIO_PULL_NONE  );
#endif

// GPC1 0~4
#if 1
    // bt pcm (0,2,3,4)
	        s3c_gpio_slp_cfgpin(S5PV210_GPC1(0), S3C_GPIO_SLP_OUT0   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPC1(0), S3C_GPIO_PULL_NONE  );

    // NC
	        s3c_gpio_slp_cfgpin(S5PV210_GPC1(1), S3C_GPIO_SLP_OUT0   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPC1(1), S3C_GPIO_PULL_NONE  );

	        s3c_gpio_slp_cfgpin(S5PV210_GPC1(2), S3C_GPIO_SLP_OUT0   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPC1(2), S3C_GPIO_PULL_NONE  );

	        s3c_gpio_slp_cfgpin(S5PV210_GPC1(3), S3C_GPIO_SLP_INPUT  );
    s3c_gpio_slp_setpull_updown(S5PV210_GPC1(3), S3C_GPIO_PULL_DOWN  );

	        s3c_gpio_slp_cfgpin(S5PV210_GPC1(4), S3C_GPIO_SLP_OUT0   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPC1(4), S3C_GPIO_PULL_NONE  );
#endif

// GPG0,1,2,3 sd/mmc
    // movi
	        s3c_gpio_slp_cfgpin(S5PV210_GPG0(0), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG0(0), S3C_GPIO_PULL_UP     );
	        s3c_gpio_slp_cfgpin(S5PV210_GPG0(1), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG0(1), S3C_GPIO_PULL_UP     );
	        s3c_gpio_slp_cfgpin(S5PV210_GPG0(2), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG0(2), S3C_GPIO_PULL_UP     );
	        s3c_gpio_slp_cfgpin(S5PV210_GPG0(3), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG0(3), S3C_GPIO_PULL_UP     );
	        s3c_gpio_slp_cfgpin(S5PV210_GPG0(4), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG0(4), S3C_GPIO_PULL_UP     );
	        s3c_gpio_slp_cfgpin(S5PV210_GPG0(5), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG0(5), S3C_GPIO_PULL_UP     );
	        s3c_gpio_slp_cfgpin(S5PV210_GPG0(6), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG0(6), S3C_GPIO_PULL_UP     );

    // NC
	        s3c_gpio_slp_cfgpin(S5PV210_GPG1(0), S3C_GPIO_SLP_OUT0    );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG1(0), S3C_GPIO_PULL_NONE   );
	        s3c_gpio_slp_cfgpin(S5PV210_GPG1(1), S3C_GPIO_SLP_OUT0    );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG1(1), S3C_GPIO_PULL_NONE   );
	        s3c_gpio_slp_cfgpin(S5PV210_GPG1(2), S3C_GPIO_SLP_OUT0    );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG1(2), S3C_GPIO_PULL_NONE   );

    // movi 8bit
	        s3c_gpio_slp_cfgpin(S5PV210_GPG1(3), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG1(3), S3C_GPIO_PULL_UP     );
	        s3c_gpio_slp_cfgpin(S5PV210_GPG1(4), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG1(4), S3C_GPIO_PULL_UP     );
	        s3c_gpio_slp_cfgpin(S5PV210_GPG1(5), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG1(5), S3C_GPIO_PULL_UP     );
	        s3c_gpio_slp_cfgpin(S5PV210_GPG1(6), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG1(6), S3C_GPIO_PULL_UP     );

    // sdcard
	        s3c_gpio_slp_cfgpin(S5PV210_GPG2(0), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG2(0), S3C_GPIO_PULL_UP     );
	        s3c_gpio_slp_cfgpin(S5PV210_GPG2(1), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG2(1), S3C_GPIO_PULL_UP     );
	        s3c_gpio_slp_cfgpin(S5PV210_GPG2(2), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG2(2), S3C_GPIO_PULL_UP     );
	        s3c_gpio_slp_cfgpin(S5PV210_GPG2(3), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG2(3), S3C_GPIO_PULL_UP     );
	        s3c_gpio_slp_cfgpin(S5PV210_GPG2(4), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG2(4), S3C_GPIO_PULL_UP     );
	        s3c_gpio_slp_cfgpin(S5PV210_GPG2(5), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG2(5), S3C_GPIO_PULL_UP     );
	        s3c_gpio_slp_cfgpin(S5PV210_GPG2(6), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG2(6), S3C_GPIO_PULL_UP     );

    // wifi sdio
	        s3c_gpio_slp_cfgpin(S5PV210_GPG3(0), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG3(0), S3C_GPIO_PULL_UP     );
	        s3c_gpio_slp_cfgpin(S5PV210_GPG3(1), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG3(1), S3C_GPIO_PULL_UP     );
	        s3c_gpio_slp_cfgpin(S5PV210_GPG3(2), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG3(2), S3C_GPIO_PULL_UP     );
	        s3c_gpio_slp_cfgpin(S5PV210_GPG3(3), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG3(3), S3C_GPIO_PULL_UP     );
	        s3c_gpio_slp_cfgpin(S5PV210_GPG3(4), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG3(4), S3C_GPIO_PULL_UP     );
	        s3c_gpio_slp_cfgpin(S5PV210_GPG3(5), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG3(5), S3C_GPIO_PULL_UP     );
	        s3c_gpio_slp_cfgpin(S5PV210_GPG3(6), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPG3(6), S3C_GPIO_PULL_UP     );


// GPD0 0~3
#if 1
    // backlight_pwm_out
	        s3c_gpio_slp_cfgpin(S5PV210_GPD0(0), S3C_GPIO_SLP_OUT0    );
    s3c_gpio_slp_setpull_updown(S5PV210_GPD0(0), S3C_GPIO_PULL_NONE   );
    
    // flash led pwm out
	        s3c_gpio_slp_cfgpin(S5PV210_GPD0(1), S3C_GPIO_SLP_OUT0    );
    s3c_gpio_slp_setpull_updown(S5PV210_GPD0(1), S3C_GPIO_PULL_NONE   );

    // motor pwm out
	        s3c_gpio_slp_cfgpin(S5PV210_GPD0(2), S3C_GPIO_SLP_OUT0    );
    s3c_gpio_slp_setpull_updown(S5PV210_GPD0(2), S3C_GPIO_PULL_NONE   );

    // flash led pwm out ??? check_gpio 
	        s3c_gpio_slp_cfgpin(S5PV210_GPD0(3), S3C_GPIO_SLP_OUT0    );
    s3c_gpio_slp_setpull_updown(S5PV210_GPD0(3), S3C_GPIO_PULL_NONE   );
#endif

// GPD1 0~5, i2c1,2,3
	        s3c_gpio_slp_cfgpin(S5PV210_GPD1(0), S3C_GPIO_SLP_OUT1    );
    s3c_gpio_slp_setpull_updown(S5PV210_GPD1(0), S3C_GPIO_PULL_NONE   );

	        s3c_gpio_slp_cfgpin(S5PV210_GPD1(1), S3C_GPIO_SLP_OUT1    );
    s3c_gpio_slp_setpull_updown(S5PV210_GPD1(1), S3C_GPIO_PULL_NONE   );

	        s3c_gpio_slp_cfgpin(S5PV210_GPD1(2), S3C_GPIO_SLP_OUT1    );
    s3c_gpio_slp_setpull_updown(S5PV210_GPD1(2), S3C_GPIO_PULL_NONE   );

	        s3c_gpio_slp_cfgpin(S5PV210_GPD1(3), S3C_GPIO_SLP_OUT1    );
    s3c_gpio_slp_setpull_updown(S5PV210_GPD1(3), S3C_GPIO_PULL_NONE   );

	        s3c_gpio_slp_cfgpin(S5PV210_GPD1(4), S3C_GPIO_SLP_OUT1    );
    s3c_gpio_slp_setpull_updown(S5PV210_GPD1(4), S3C_GPIO_PULL_NONE   );

	        s3c_gpio_slp_cfgpin(S5PV210_GPD1(5), S3C_GPIO_SLP_OUT1    );
    s3c_gpio_slp_setpull_updown(S5PV210_GPD1(5), S3C_GPIO_PULL_NONE   );
    
    
// GPH0 0~7 
#if 1
    // power_on_output, pmic
	        s3c_gpio_slp_cfgpin(S5PV210_GPH0(0), S3C_GPIO_SLP_OUT1    );
    s3c_gpio_slp_setpull_updown(S5PV210_GPH0(0), S3C_GPIO_PULL_NONE   );

    // Power key PMIC signal input
	        s3c_gpio_slp_cfgpin(S5PV210_GPH0(1), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPH0(1), S3C_GPIO_PULL_UP     );

    // 5M A/F camera reset
	        s3c_gpio_slp_cfgpin(S5PV210_GPH0(2), S3C_GPIO_SLP_OUT1    );
    s3c_gpio_slp_setpull_updown(S5PV210_GPH0(2), S3C_GPIO_PULL_NONE   );

    // als interrupt input
	        s3c_gpio_slp_cfgpin(S5PV210_GPH0(3), S3C_GPIO_SLP_INPUT   );
    s3c_gpio_slp_setpull_updown(S5PV210_GPH0(3), S3C_GPIO_PULL_UP     );

	// pmic dvfs select
	        s3c_gpio_slp_cfgpin(S5PV210_GPH0(4), S3C_GPIO_SLP_OUT0    );
    s3c_gpio_slp_setpull_updown(S5PV210_GPH0(4), S3C_GPIO_PULL_NONE   );

    // T-DMB IC power down
	        s3c_gpio_slp_cfgpin(S5PV210_GPH0(5), S3C_GPIO_SLP_OUT0    );
    s3c_gpio_slp_setpull_updown(S5PV210_GPH0(5), S3C_GPIO_PULL_NONE   );

    // T-DMB IC reset
	        s3c_gpio_slp_cfgpin(S5PV210_GPH0(6), S3C_GPIO_SLP_OUT0    );
    s3c_gpio_slp_setpull_updown(S5PV210_GPH0(6), S3C_GPIO_PULL_NONE   );


    // CDMA_UART_DTR
//	        s3c_gpio_slp_cfgpin(S5PV210_GPH0(7), S3C_GPIO_SLP_OUT1    );
//  s3c_gpio_slp_setpull_updown(S5PV210_GPH0(7), S3C_GPIO_PULL_NONE   );
#endif


// 여기서 부터 작업 필요


//  === GPH1 === 

	// GPIO_TOUCH_INT_IN
	        s3c_gpio_slp_cfgpin(S5PV210_GPH1(0), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPH1(0), S3C_GPIO_PULL_UP);

    // charger_full_detect
	        s3c_gpio_slp_cfgpin(S5PV210_GPH1(1), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPH1(1), S3C_GPIO_PULL_UP);

    // bt_host_wakup
//	        s3c_gpio_slp_cfgpin(S5PV210_GPH1(2), S3C_GPIO_SLP_INPUT);
//  s3c_gpio_slp_setpull_updown(S5PV210_GPH1(2), S3C_GPIO_PULL_DOWN);

    // dmb_interrupt
	        s3c_gpio_slp_cfgpin(S5PV210_GPH1(3), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPH1(3), S3C_GPIO_PULL_DOWN);

    // hdmi_cec
	        s3c_gpio_slp_cfgpin(S5PV210_GPH1(4), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPH1(4), S3C_GPIO_PULL_NONE);

    // hdmi_hpd
	        s3c_gpio_slp_cfgpin(S5PV210_GPH1(5), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPH1(5), S3C_GPIO_PULL_UP);
    
    // pmic_dvfs_select
	        s3c_gpio_slp_cfgpin(S5PV210_GPH1(6), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPH1(6), S3C_GPIO_PULL_NONE);

    // pmic_dvfs_select
	        s3c_gpio_slp_cfgpin(S5PV210_GPH1(7), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPH1(7), S3C_GPIO_PULL_NONE);

//  === GPH2 === 

    // pmic_lowbat_detect
	        s3c_gpio_slp_cfgpin(S5PV210_GPH2(0), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPH2(0), S3C_GPIO_PULL_UP);

    // adaptor_detect
	        s3c_gpio_slp_cfgpin(S5PV210_GPH2(1), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPH2(1), S3C_GPIO_PULL_UP);

    // gyro_int1
	        s3c_gpio_slp_cfgpin(S5PV210_GPH2(2), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPH2(2), S3C_GPIO_PULL_UP);

    // earjack_intr
//	        s3c_gpio_slp_cfgpin(S5PV210_GPH2(3), S3C_GPIO_SLP_OUT0);
//  s3c_gpio_slp_setpull_updown(S5PV210_GPH2(3), S3C_GPIO_PULL_NONE);

    // compass_intr
	        s3c_gpio_slp_cfgpin(S5PV210_GPH2(4), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPH2(4), S3C_GPIO_PULL_UP);

    // cdma_host_wakeup
//	        s3c_gpio_slp_cfgpin(S5PV210_GPH2(5), S3C_GPIO_SLP_OUT0);
//  s3c_gpio_slp_setpull_updown(S5PV210_GPH2(5), S3C_GPIO_PULL_NONE);

   // gyro_int2
	        s3c_gpio_slp_cfgpin(S5PV210_GPH2(6), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPH2(6), S3C_GPIO_PULL_DOWN);

    // cdma_ready
//	        s3c_gpio_slp_cfgpin(S5PV210_GPH2(7), S3C_GPIO_SLP_INPUT);
//  s3c_gpio_slp_setpull_updown(S5PV210_GPH2(7), S3C_GPIO_PULL_DOWN);


//  === GPH3 === 

    // volume +
        	s3c_gpio_slp_cfgpin(S5PV210_GPH3(0), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPH3(0), S3C_GPIO_PULL_UP);

    // cam_1.3m_reset
        	s3c_gpio_slp_cfgpin(S5PV210_GPH3(1), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPH3(1), S3C_GPIO_PULL_NONE);

    // wifi_host_wakeup
//      	s3c_gpio_slp_cfgpin(S5PV210_GPH3(2), S3C_GPIO_SLP_INPUT);
//  s3c_gpio_slp_setpull_updown(S5PV210_GPH3(2), S3C_GPIO_PULL_DOWN);

    // cam_5m_standby_out
        	s3c_gpio_slp_cfgpin(S5PV210_GPH3(3), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPH3(3), S3C_GPIO_PULL_NONE);

    //  volume-
        	s3c_gpio_slp_cfgpin(S5PV210_GPH3(4), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPH3(4), S3C_GPIO_PULL_UP);

    // cam_1.3m_standby_out 
        	s3c_gpio_slp_cfgpin(S5PV210_GPH3(5), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPH3(5), S3C_GPIO_PULL_NONE);

// 여기 동작 안됨
    #if defined(CONFIG_MX100_REV_TP)
	// GPIO_USB_SW_OE
//        	s3c_gpio_slp_cfgpin(S5PV210_GPH3(6), S3C_GPIO_SLP_INPUT);
//  s3c_gpio_slp_setpull_updown(S5PV210_GPH3(6), S3C_GPIO_PULL_DOWN);
    #endif

    // accel interrupt
        	s3c_gpio_slp_cfgpin(S5PV210_GPH3(7), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPH3(7), S3C_GPIO_PULL_DOWN);


//  === GPJ0 === 

    // wifi wakeup output
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ0(0), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ0(0), S3C_GPIO_PULL_NONE);

    // wlan_clk_en
//	        s3c_gpio_slp_cfgpin(S5PV210_GPJ0(1), S3C_GPIO_SLP_OUT0);
//  s3c_gpio_slp_setpull_updown(S5PV210_GPJ0(1), S3C_GPIO_PULL_NONE);

    // bt wakeup output
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ0(2), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ0(2), S3C_GPIO_PULL_NONE);

    // hdmi_enable
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ0(3), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ0(3), S3C_GPIO_PULL_NONE);

	// blu_enable, backlight
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ0(4), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ0(4), S3C_GPIO_PULL_NONE);

    // wifi reset
//	        s3c_gpio_slp_cfgpin(S5PV210_GPJ0(5), S3C_GPIO_SLP_OUT0);
//  s3c_gpio_slp_setpull_updown(S5PV210_GPJ0(5), S3C_GPIO_PULL_NONE);

	// bt reset
//	        s3c_gpio_slp_cfgpin(S5PV210_GPJ0(6), S3C_GPIO_SLP_OUT0);
//  s3c_gpio_slp_setpull_updown(S5PV210_GPJ0(6), S3C_GPIO_PULL_NONE);

    // gps_enable
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ0(7), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ0(7), S3C_GPIO_PULL_NONE);
        

//  === GPJ1 === 

	// earjack mute
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ1(0), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ1(0), S3C_GPIO_PULL_NONE);

#if 1
    // LVDS_PWDN
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ1(1), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ1(1), S3C_GPIO_PULL_NONE);
#endif

    // fm_reset
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ1(0), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ1(0), S3C_GPIO_PULL_NONE);

    // fm i2c
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ1(3), S3C_GPIO_SLP_OUT1);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ1(3), S3C_GPIO_PULL_NONE);
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ1(4), S3C_GPIO_SLP_OUT1);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ1(4), S3C_GPIO_PULL_NONE);

    // ear mic hoo_key input
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ1(0), S3C_GPIO_SLP_INPUT);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ1(0), S3C_GPIO_PULL_UP);

//  === GPJ2 === 

	// cdma sleep control
//	        s3c_gpio_slp_cfgpin(S5PV210_GPJ2(0), S3C_GPIO_SLP_OUT0);
//  s3c_gpio_slp_setpull_updown(S5PV210_GPJ2(0), S3C_GPIO_PULL_NONE);

    // cdma power on
//	        s3c_gpio_slp_cfgpin(S5PV210_GPJ2(0), S3C_GPIO_SLP_OUT0);
//  s3c_gpio_slp_setpull_updown(S5PV210_GPJ2(0), S3C_GPIO_PULL_NONE);

    // cdma reset
//	        s3c_gpio_slp_cfgpin(S5PV210_GPJ2(0), S3C_GPIO_SLP_OUT0);
//  s3c_gpio_slp_setpull_updown(S5PV210_GPJ2(0), S3C_GPIO_PULL_NONE);

    // wifi-bt enable out
//	        s3c_gpio_slp_cfgpin(S5PV210_GPJ2(0), S3C_GPIO_SLP_OUT0);
//  s3c_gpio_slp_setpull_updown(S5PV210_GPJ2(0), S3C_GPIO_PULL_NONE);

#if 1
    // KEY_LED3
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ2(4), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ2(4), S3C_GPIO_PULL_NONE);
#endif

	// GPIO_TOUCH_ENABLE_OUT
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ2(5), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ2(5), S3C_GPIO_PULL_NONE);

    // accel-i2c
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ2(6), S3C_GPIO_SLP_OUT1);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ2(6), S3C_GPIO_PULL_NONE);
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ2(7), S3C_GPIO_SLP_OUT1);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ2(7), S3C_GPIO_PULL_NONE);

//  === GPJ3 === 

    // comp-i2c
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ3(0), S3C_GPIO_SLP_OUT1);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ3(0), S3C_GPIO_PULL_NONE);
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ3(1), S3C_GPIO_SLP_OUT1);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ3(1), S3C_GPIO_PULL_NONE);

#if 0 // fixed set high to vibe issue, ktj
    // GPIO_HAPTIC_LEN
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ3(2), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ3(2), S3C_GPIO_PULL_NONE);
#else   
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ3(2), S3C_GPIO_SLP_OUT1);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ3(2), S3C_GPIO_PULL_NONE);
#endif

    // GPIO_HAPTIC_HEN
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ3(3), S3C_GPIO_SLP_OUT0);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ3(3), S3C_GPIO_PULL_NONE);

    // cam-i2c
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ3(4), S3C_GPIO_SLP_OUT1);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ3(4), S3C_GPIO_PULL_NONE);
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ3(5), S3C_GPIO_SLP_OUT1);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ3(5), S3C_GPIO_PULL_NONE);
    
    // als-i2c
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ3(6), S3C_GPIO_SLP_OUT1);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ3(6), S3C_GPIO_PULL_NONE);
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ3(7), S3C_GPIO_SLP_OUT1);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ3(7), S3C_GPIO_PULL_NONE);

//  === GPJ4 === 

    // fuel-i2c
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ4(0), S3C_GPIO_SLP_OUT1);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ4(0), S3C_GPIO_PULL_NONE);
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ4(1), S3C_GPIO_SLP_OUT1);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ4(1), S3C_GPIO_PULL_NONE);

    // motor-i2c
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ4(2), S3C_GPIO_SLP_OUT1);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ4(2), S3C_GPIO_PULL_NONE);
	        s3c_gpio_slp_cfgpin(S5PV210_GPJ4(3), S3C_GPIO_SLP_OUT1);
    s3c_gpio_slp_setpull_updown(S5PV210_GPJ4(3), S3C_GPIO_PULL_NONE);

    // usb_sw
//	        s3c_gpio_slp_cfgpin(S5PV210_GPJ4(4), S3C_GPIO_SLP_OUT1);
//  s3c_gpio_slp_setpull_updown(S5PV210_GPJ4(4), S3C_GPIO_PULL_NONE);



//-----------------------------------------------------------------------------//

/*  
    // UART_AN_SW    
    set_gpio_config(S5PV210_GPC0CONPDN, 3, GPIO_CON_PDN);
    set_gpio_config(S5PV210_GPC0PUDPDN, 3, GPIO_CON_PUD);

    // CDMA_USB_ON
    set_gpio_config(S5PV210_GPC0CONPDN, 4, GPIO_CON_PDN);
    set_gpio_config(S5PV210_GPC0PUDPDN, 4, GPIO_CON_PUD);
*/

/* i2c ch0,1,2
    set_gpio_config(S5PV210_GPD1CONPDN, 0, GPIO_CON_PDN);
    set_gpio_config(S5PV210_GPD1PUDPDN, 0, GPIO_CON_PUD);

    set_gpio_config(S5PV210_GPD1CONPDN, 1, GPIO_CON_PDN);
    set_gpio_config(S5PV210_GPD1PUDPDN, 1, GPIO_CON_PUD);

    set_gpio_config(S5PV210_GPD1CONPDN, 2, GPIO_CON_PDN);
    set_gpio_config(S5PV210_GPD1PUDPDN, 2, GPIO_CON_PUD);

    set_gpio_config(S5PV210_GPD1CONPDN, 3, GPIO_CON_PDN);
    set_gpio_config(S5PV210_GPD1PUDPDN, 3, GPIO_CON_PUD);

    set_gpio_config(S5PV210_GPD1CONPDN, 4, GPIO_CON_PDN);
    set_gpio_config(S5PV210_GPD1PUDPDN, 4, GPIO_CON_PUD);

    set_gpio_config(S5PV210_GPD1CONPDN, 5, GPIO_CON_PDN);
    set_gpio_config(S5PV210_GPD1PUDPDN, 5, GPIO_CON_PUD);
*/
/*
    // fm-i2c
    set_gpio_config(S5PV210_GPJ1CONPDN, 3, GPIO_CON_PDN);
    set_gpio_config(S5PV210_GPJ1PUDPDN, 3, GPIO_CON_PUD);
    set_gpio_config(S5PV210_GPJ1CONPDN, 4, GPIO_CON_PDN);
    set_gpio_config(S5PV210_GPJ1PUDPDN, 4, GPIO_CON_PUD);
*/
/*
    // accel-i2c
    set_gpio_config(S5PV210_GPJ2CONPDN, 6, GPIO_CON_PDN);
    set_gpio_config(S5PV210_GPJ2PUDPDN, 6, GPIO_CON_PUD);
    set_gpio_config(S5PV210_GPJ2CONPDN, 7, GPIO_CON_PDN);
    set_gpio_config(S5PV210_GPJ2PUDPDN, 7, GPIO_CON_PUD);
*/
/*
    set_gpio_config(S5PV210_GPJ3CONPDN, 0, GPIO_CON_PDN);
    set_gpio_config(S5PV210_GPJ3PUDPDN, 0, GPIO_CON_PUD);
    set_gpio_config(S5PV210_GPJ3CONPDN, 1, GPIO_CON_PDN);
    set_gpio_config(S5PV210_GPJ3PUDPDN, 1, GPIO_CON_PUD);
    set_gpio_config(S5PV210_GPJ3CONPDN, 4, GPIO_CON_PDN);
    set_gpio_config(S5PV210_GPJ3PUDPDN, 4, GPIO_CON_PUD);
    set_gpio_config(S5PV210_GPJ3CONPDN, 5, GPIO_CON_PDN);
    set_gpio_config(S5PV210_GPJ3PUDPDN, 5, GPIO_CON_PUD);
    set_gpio_config(S5PV210_GPJ3CONPDN, 6, GPIO_CON_PDN);
    set_gpio_config(S5PV210_GPJ3PUDPDN, 6, GPIO_CON_PUD);
    set_gpio_config(S5PV210_GPJ3CONPDN, 7, GPIO_CON_PDN);
    set_gpio_config(S5PV210_GPJ3PUDPDN, 7, GPIO_CON_PUD);

    set_gpio_config(S5PV210_GPJ4CONPDN, 0, GPIO_CON_PDN);
    set_gpio_config(S5PV210_GPJ4PUDPDN, 0, GPIO_CON_PUD);
    set_gpio_config(S5PV210_GPJ4CONPDN, 1, GPIO_CON_PDN);
    set_gpio_config(S5PV210_GPJ4PUDPDN, 1, GPIO_CON_PUD);
    set_gpio_config(S5PV210_GPJ4CONPDN, 2, GPIO_CON_PDN);
    set_gpio_config(S5PV210_GPJ4PUDPDN, 2, GPIO_CON_PUD);
    set_gpio_config(S5PV210_GPJ4CONPDN, 3, GPIO_CON_PDN);
    set_gpio_config(S5PV210_GPJ4PUDPDN, 3, GPIO_CON_PUD);
*/
}

void s3c_config_sleep_gpio(void)
{
    s3c_pre_config_sleep_gpio(); // ktj	

#ifdef CONFIG_MX100_REV_TP
    // select_gps_debug
    if(g_uart2_port==1) // gps 
    {	
	    s3c_gpio_slp_cfgpin(GPIO_GPS_DEBUG_SELECT_OUT, S3C_GPIO_SLP_OUT1); 
        s3c_gpio_slp_setpull_updown(GPIO_GPS_DEBUG_SELECT_OUT, S3C_GPIO_PULL_NONE);
    }
    else // debug
    {
	    s3c_gpio_slp_cfgpin(GPIO_GPS_DEBUG_SELECT_OUT, S3C_GPIO_SLP_OUT0); 
        s3c_gpio_slp_setpull_updown(GPIO_GPS_DEBUG_SELECT_OUT, S3C_GPIO_PULL_NONE);
    }
#endif


	if(gpio_get_value(GPIO_WLAN_nRST) == 1 && gpio_get_value(GPIO_BT_nRST) == 1)
    {
		s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_CLK_EN, S3C_GPIO_SLP_OUT1);
	    s3c_gpio_slp_setpull_updown(GPIO_WLAN_BT_CLK_EN, S3C_GPIO_PULL_NONE);

		s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_SLP_OUT1);
	    s3c_gpio_slp_setpull_updown(GPIO_WLAN_BT_EN, S3C_GPIO_PULL_NONE);

	    s3c_gpio_slp_cfgpin(GPIO_WLAN_nRST, S3C_GPIO_SLP_OUT1);
        s3c_gpio_slp_setpull_updown(GPIO_WLAN_nRST, S3C_GPIO_PULL_NONE);

   	    s3c_gpio_slp_cfgpin(GPIO_BT_nRST, S3C_GPIO_SLP_OUT1);
        s3c_gpio_slp_setpull_updown(GPIO_BT_nRST, S3C_GPIO_PULL_NONE);

       s3c_gpio_slp_cfgpin(GPIO_BT_WAKE, S3C_GPIO_SLP_OUT1);
            s3c_gpio_slp_setpull_updown(GPIO_BT_WAKE, S3C_GPIO_PULL_NONE);
    }
	else if(gpio_get_value(GPIO_WLAN_nRST) == 1 && gpio_get_value(GPIO_BT_nRST) == 0)
    {
		s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_CLK_EN, S3C_GPIO_SLP_OUT1);
	    s3c_gpio_slp_setpull_updown(GPIO_WLAN_BT_CLK_EN, S3C_GPIO_PULL_NONE);

		s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_SLP_OUT1);
	    s3c_gpio_slp_setpull_updown(GPIO_WLAN_BT_EN, S3C_GPIO_PULL_NONE);

	    s3c_gpio_slp_cfgpin(GPIO_WLAN_nRST, S3C_GPIO_SLP_OUT1);
        s3c_gpio_slp_setpull_updown(GPIO_WLAN_nRST, S3C_GPIO_PULL_NONE);

   	    s3c_gpio_slp_cfgpin(GPIO_BT_nRST, S3C_GPIO_SLP_OUT0);
        s3c_gpio_slp_setpull_updown(GPIO_BT_nRST, S3C_GPIO_PULL_NONE);

   	    s3c_gpio_slp_cfgpin(GPIO_BT_WAKE, S3C_GPIO_SLP_OUT0);
            s3c_gpio_slp_setpull_updown(GPIO_BT_WAKE, S3C_GPIO_PULL_NONE);
    }
	else if(gpio_get_value(GPIO_WLAN_nRST) == 0 && gpio_get_value(GPIO_BT_nRST) == 1)
    {
		s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_CLK_EN, S3C_GPIO_SLP_OUT1);
	    s3c_gpio_slp_setpull_updown(GPIO_WLAN_BT_CLK_EN, S3C_GPIO_PULL_NONE);

		s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_SLP_OUT1);
	    s3c_gpio_slp_setpull_updown(GPIO_WLAN_BT_EN, S3C_GPIO_PULL_NONE);

	    s3c_gpio_slp_cfgpin(GPIO_WLAN_nRST, S3C_GPIO_SLP_OUT0);
        s3c_gpio_slp_setpull_updown(GPIO_WLAN_nRST, S3C_GPIO_PULL_NONE);

   	    s3c_gpio_slp_cfgpin(GPIO_BT_nRST, S3C_GPIO_SLP_OUT1);
        s3c_gpio_slp_setpull_updown(GPIO_BT_nRST, S3C_GPIO_PULL_NONE);

   	    s3c_gpio_slp_cfgpin(GPIO_BT_WAKE, S3C_GPIO_SLP_OUT1);
            s3c_gpio_slp_setpull_updown(GPIO_BT_WAKE, S3C_GPIO_PULL_NONE);
    }
    else
    {
		s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_CLK_EN, S3C_GPIO_SLP_OUT0);
	    s3c_gpio_slp_setpull_updown(GPIO_WLAN_BT_CLK_EN, S3C_GPIO_PULL_NONE);

		s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_SLP_OUT0);
	    s3c_gpio_slp_setpull_updown(GPIO_WLAN_BT_EN, S3C_GPIO_PULL_NONE);

	    s3c_gpio_slp_cfgpin(GPIO_WLAN_nRST, S3C_GPIO_SLP_OUT0);
        s3c_gpio_slp_setpull_updown(GPIO_WLAN_nRST, S3C_GPIO_PULL_NONE);

   	    s3c_gpio_slp_cfgpin(GPIO_BT_nRST, S3C_GPIO_SLP_OUT0);
        s3c_gpio_slp_setpull_updown(GPIO_BT_nRST, S3C_GPIO_PULL_NONE);

   	    s3c_gpio_slp_cfgpin(GPIO_BT_WAKE, S3C_GPIO_SLP_OUT0);
            s3c_gpio_slp_setpull_updown(GPIO_BT_WAKE, S3C_GPIO_PULL_NONE);
    }
}
EXPORT_SYMBOL(s3c_config_sleep_gpio);
#endif // CONFIG_MX100_WS


MACHINE_START(SMDKC110, "SMDKC110")
	/* Maintainer: Kukjin Kim <kgene.kim@samsung.com> */
	.phys_io	= S3C_PA_UART & 0xfff00000,
	.io_pg_offst	= (((u32)S3C_VA_UART) >> 18) & 0xfffc,
	.boot_params	= S5P_PA_SDRAM + 0x100,
	.init_irq	= s5pv210_init_irq,
	.map_io		= smdkv210_map_io,
	.init_machine	= smdkv210_machine_init,
	.timer		= &s5p_systimer,
MACHINE_END
