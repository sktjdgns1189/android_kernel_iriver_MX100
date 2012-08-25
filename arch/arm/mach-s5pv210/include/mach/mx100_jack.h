/*
 * Copyright (C) 2010 Iriver, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ASM_ARCH_MX100_JACK_H
#define __ASM_ARCH_MX100_JACK_H

enum
{
	MX100_JACK_NO_DEVICE					= 0x0,
	MX100_HEADSET_4_POLE_DEVICE			= 0x01 << 0,	
	MX100_HEADSET_3_POLE_DEVICE			= 0x01 << 1,
	MX100_UNKNOWN_DEVICE					= 0x01 << 2,	
};

#if defined(CONFIG_MX100_WS)
#define SUPPORT_EXT_4POLE_HEADSET // ktj disable
#endif


#ifdef  SUPPORT_EXT_4POLE_HEADSET
#define SUPPORT_HEADSET_HOOK_KEY

#endif

#define ENABLE_ANDROID_SWITCH_H2W  // JHLIM 2011.02.09 

void	mx100_headset_detect(struct work_struct *dwork);
void reset_4pole_sendkey_ready(void);
int is_4pole_headset_connected(void);

#endif
