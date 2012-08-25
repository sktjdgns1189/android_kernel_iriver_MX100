/* linux/arch/arm/mach-s5pv210/init.c
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

#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/s5pv210.h>
#include <plat/regs-serial.h>

#ifndef CONFIG_SERIAL_USE_HIGH_CLOCK
#ifdef CONFIG_MX100_REV_TP
/* JHLIM 2011.02.18
on mx100 TP hw, 

uart0 -> BT
uart1 -> cdma cm port
uart2 -> DEBUG,GPS port.
uart3-> cdma dm port.

*/

static struct s3c24xx_uart_clksrc s5pv210_serial_clocks[] = {
	[0] = {
// ktj disable for bt sleep crash
//		.name		= "sclk_uart",
		.name		= "pclk",
		.divisor	= 1,
		.min_baud	= 0,
		.max_baud	= 0,
	},
	[1] = {
		.name		= "sclk_uart",
		.divisor	= 1,
		.min_baud	= 0,
		.max_baud	= 0,
	},
	[2] = {
		.name		= "pclk",
		.divisor	= 1,
		.min_baud	= 0,
		.max_baud	= 0,
	},
	[3] = {
		.name		= "pclk",
		.divisor	= 1,
		.min_baud	= 0,
		.max_baud	= 0,
	},
};
#else
static struct s3c24xx_uart_clksrc s5pv210_serial_clocks[] = {
	[0] = {
		.name		= "pclk",
		.divisor	= 1,
		.min_baud	= 0,
		.max_baud	= 0,
	},
};

#endif

#endif

#ifdef CONFIG_SERIAL_USE_HIGH_CLOCK
static struct s3c24xx_uart_clksrc s5pv210_serial_clocks[] = {
        [0] = {
                .name           = "uclk1",
                .divisor        = 1,
                .min_baud       = 0,
                .max_baud       = 0,
        },
};
#endif
/* uart registration process */
void __init s5pv210_common_init_uarts(struct s3c2410_uartcfg *cfg, int no)
{
	struct s3c2410_uartcfg *tcfg = cfg;
	u32 ucnt;

	for (ucnt = 0; ucnt < no; ucnt++, tcfg++) {
		if (!tcfg->clocks) {
			tcfg->clocks = s5pv210_serial_clocks;
			tcfg->clocks_size = ARRAY_SIZE(s5pv210_serial_clocks);
		}
	}

	s3c24xx_init_uartdevs("s5pv210-uart", s5p_uart_resources, cfg, no);
}
