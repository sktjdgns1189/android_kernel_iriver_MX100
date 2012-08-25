/*
 * Tosa bluetooth built-in chip control.
 *
 * Later it may be shared with some other platforms.
 *
 * Copyright (c) 2008 Dmitry Baryshkov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef BCM_BT_H
#define BCM_BT_H

struct bcm_bt_data {
	int gpio_pwr;
	int gpio_reset;
	int wake_up;
	int reg_on;
};

struct uart_port; 
void bcm_bt_uart_wake_peer(struct uart_port *port);
#endif

