

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/input.h>

#include <mach/regs-clock.h>

#include <mach/gpio-bank.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <asm/gpio.h>

#include "mx100_keypad.h"

int		mx100_keypad_sysfs_create		(struct platform_device *pdev);
void	mx100_keypad_sysfs_remove		(struct platform_device *pdev);	
		
static	ssize_t show_system_off			(struct device *dev, struct device_attribute *attr, char *buf);
static	ssize_t set_system_off			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(system_off, S_IRWXUGO, show_system_off, set_system_off);

static	ssize_t show_hold_state			(struct device *dev, struct device_attribute *attr, char *buf);
static	ssize_t set_hold_state			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(hold_state, S_IRWXUGO, show_hold_state, set_hold_state);

static	ssize_t show_poweroff_control	(struct device *dev, struct device_attribute *attr, char *buf);
static 	ssize_t set_poweroff_control	(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(poweroff_control, S_IRWXUGO, show_poweroff_control, set_poweroff_control);

static struct attribute *mx100_keypad_sysfs_entries[] = {
	&dev_attr_system_off.attr,
	&dev_attr_hold_state.attr,
	&dev_attr_poweroff_control.attr,
	NULL
};

static struct attribute_group mx100_keypad_attr_group = {
	.name   = NULL,
	.attrs  = mx100_keypad_sysfs_entries,
};
static 	ssize_t show_system_off			(struct device *dev, struct device_attribute *attr, char *buf)
{
	return	sprintf(buf, "%s : unsupport this function!\n", __FUNCTION__);
}

static 	ssize_t set_system_off			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned long flags;

    local_irq_save(flags);

	if(!strcmp(buf, "on\n"))	{
		POWER_OFF_ENABLE();
		#if 1 // jhlim
		//gpio_set_value(POWER_LED_PORT, 0);	 
		gpio_set_value(POWER_CONTROL_PORT, 0);		
		#endif
	}

    local_irq_restore(flags);

    return count;
}
static 	ssize_t show_hold_state			(struct device *dev, struct device_attribute *attr, char *buf)
{
	if(mx100_keypad.hold_state)	return	sprintf(buf, "on\n");
	else							return	sprintf(buf, "off\n");
}

static 	ssize_t set_hold_state			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned long flags;

    local_irq_save(flags);

	if(!strcmp(buf, "on\n"))	mx100_keypad.hold_state = 1;
	else						mx100_keypad.hold_state = 0;

    local_irq_restore(flags);

    return count;
}

static 	ssize_t show_poweroff_control	(struct device *dev, struct device_attribute *attr, char *buf)
{
	switch(mx100_keypad.poweroff_time)	{
		default	:
			mx100_keypad.poweroff_time = 0;
		case	0:
			return	sprintf(buf, "1 sec\n");
		case	1:
			return	sprintf(buf, "3 sec\n");
		case	2:
			return	sprintf(buf, "5 sec\n");
	}
}

static 	ssize_t set_poweroff_control	(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned long 	flags;
    unsigned int	val;

    if(!(sscanf(buf, "%u\n", &val)))	return	-EINVAL;

    local_irq_save(flags);
    
    if		(val > 3)		mx100_keypad.sampling_rate = 2;
    else if	(val > 1)		mx100_keypad.sampling_rate = 1;
    else					mx100_keypad.sampling_rate = 0;

    local_irq_restore(flags);

    return count;
}

int		mx100_keypad_sysfs_create		(struct platform_device *pdev)	
{
	// variable init
	mx100_keypad.hold_state		= 0;	// key hold off
	mx100_keypad.sampling_rate		= 0;	// 10 msec sampling
	mx100_keypad.poweroff_time		= 1;	// 3 sec

	return	sysfs_create_group(&pdev->dev.kobj, &mx100_keypad_attr_group);
}

void	mx100_keypad_sysfs_remove		(struct platform_device *pdev)	
{
    sysfs_remove_group(&pdev->dev.kobj, &mx100_keypad_attr_group);
}
