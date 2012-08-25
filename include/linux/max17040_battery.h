/*
 *  Copyright (C) 2009 Samsung Electronics
 *  Minkyu Kang <mk7.kang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MAX17040_BATTERY_H_
#define __MAX17040_BATTERY_H_

/* ktj */
#define GPIO_CHARGER_ONLINE	S5PV210_GPH2(1)     /* ADT_DETECT# */
#define GPIO_CHARGER_STATUS	S5PV210_GPH1(1)     /* CHG# */
//#define GPIO_CHARGER_DONE	
//#define GPIO_CHARGER_ENABLE	

struct max17040_platform_data {
	int (*battery_online)(void);
	int (*charger_online)(void);
	int (*charger_enable)(void);
	int (*charger_done)(void);
	void (*charger_disable)(void);
};

#endif
