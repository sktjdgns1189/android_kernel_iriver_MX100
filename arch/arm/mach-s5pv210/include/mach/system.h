/* linux/arch/arm/mach-s5pv210/include/mach/system.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * S5PV210 - system support header
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_SYSTEM_H
#define __ASM_ARCH_SYSTEM_H __FILE__

#include <plat/watchdog-reset.h>

static void arch_idle(void)
{
	/* nothing here yet */
}

#define SW_RST_OFFSET			0x2000  /* JHLIM  */

static void arch_reset(char mode, const char *cmd)
{
	if (mode == 's') {   /* JHLIM 2011.05.02 */
		writel( 0x1, S3C_VA_SYS + SW_RST_OFFSET);  /* reset by swreset  */
	} else {
		arch_wdt_reset();
	}
	/* if all else fails, or mode was for soft, jump to 0 */
	cpu_reset(0);
}

#endif /* __ASM_ARCH_SYSTEM_H */
