/* linux/arch/arm/plat-s3c/pm.c
 *
 * Copyright 2008 Openmoko, Inc.
 * Copyright 2004-2008 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *	http://armlinux.simtec.co.uk/
 *
 * S3C common power management (suspend to ram) support.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/serial_core.h>
#include <linux/io.h>
#include <linux/regulator/machine.h>

#include <asm/cacheflush.h>
#include <mach/hardware.h>
#include <mach/map.h>

#include <plat/regs-serial.h>
#include <mach/regs-clock.h>
#include <mach/regs-irq.h>
#include <asm/irq.h>

#include <plat/pm.h>
#include <mach/pm-core.h>

/* ktj add */
#include <mach/regs-gpio.h>
#include <mach/gpio-bank.h>
#include <mach/gpio-bank-eint.h>

#undef IRIVER_WAKEUP_TEST  /* sleep auto wakeup test */
//#define IRIVER_WAKEUP_TEST  /* sleep auto wakeup test */ //janged

#if 0   /* temporary option */
#define DISABLE_WAKEUP_RTC   
#define DISABLE_WAKEUP_WIFI_BT
#endif

#define PROCESS_WKUP_SRC    /* ktj process wakup source */

#ifdef PROCESS_WKUP_SRC
static int request_poweroff = 0;
static int request_screenon = 0;

void set_request_poweroff(void)
{
    request_poweroff = 1;        
}

int get_request_poweroff(void)
{
    return request_poweroff;    
}

void set_request_screenon(int value)
{
    request_screenon = value;        
}

int get_request_screenon(void)
{
    return request_screenon;    
}
#else
int get_request_poweroff(void)
{
    return 0;    
}

int get_request_screenon(void)
{
    return 1;    
}
#endif    
/* ktj done */


#define USE_DMA_ALLOC

#ifdef USE_DMA_ALLOC
#include <linux/dma-mapping.h>

static unsigned long *regs_save;
static dma_addr_t phy_regs_save;
#endif /* USE_DMA_ALLOC */

/* for external use */

unsigned long s3c_pm_flags;
extern void s3c_config_sleep_gpio(void); /* ktj */

/* Debug code:
 *
 * This code supports debug output to the low level UARTs for use on
 * resume before the console layer is available.
*/

#ifdef CONFIG_SAMSUNG_PM_DEBUG
extern void printascii(const char *);

void s3c_pm_dbg(const char *fmt, ...)
{
#if 0 // janged test
	va_list va;
	char buff[256];

	va_start(va, fmt);
	vsprintf(buff, fmt, va);
	va_end(va);

	printascii(buff);
#endif
}

static inline void s3c_pm_debug_init(void)
{
	/* restart uart clocks so we can use them to output */
	s3c_pm_debug_init_uart();
}

#else
#define s3c_pm_debug_init() do { } while(0)

#endif /* CONFIG_SAMSUNG_PM_DEBUG */



/* Save the UART configurations if we are configured for debug. */

unsigned char pm_uart_udivslot;

#ifdef CONFIG_SAMSUNG_PM_DEBUG

struct pm_uart_save uart_save[CONFIG_SERIAL_SAMSUNG_UARTS];

#define S5P_SZ_UART_FULL        SZ_1K
#define S3C_VA_UARTx(uart) (S3C_VA_UART + ((uart * S5P_SZ_UART_FULL)))
#define S3C2410_UDIVSLOT  (0x2C)
#define S3C2410_UINTMSK   (0x38)


static void s3c_pm_save_uart(unsigned int uart, struct pm_uart_save *save)
{
	void __iomem *regs = S3C_VA_UARTx(uart);

	save->ulcon = __raw_readl(regs + S3C2410_ULCON);
	save->ucon = __raw_readl(regs + S3C2410_UCON);
	save->ufcon = __raw_readl(regs + S3C2410_UFCON);
	save->umcon = __raw_readl(regs + S3C2410_UMCON);
	save->ubrdiv = __raw_readl(regs + S3C2410_UBRDIV);

	if (pm_uart_udivslot)
		save->udivslot = __raw_readl(regs + S3C2443_DIVSLOT);

	S3C_PMDBG("UART[%d]: ULCON=%04x, UCON=%04x, UFCON=%04x, UBRDIV=%04x\n",
		  uart, save->ulcon, save->ucon, save->ufcon, save->ubrdiv);
}

static void s3c_pm_save_uarts(void)
{
	struct pm_uart_save *save = uart_save;
	unsigned int uart;

	for (uart = 0; uart < CONFIG_SERIAL_SAMSUNG_UARTS; uart++, save++)
		s3c_pm_save_uart(uart, save);
}

static void s3c_pm_restore_uart(unsigned int uart, struct pm_uart_save *save)
{
	void __iomem *regs = S3C_VA_UARTx(uart);

	s3c_pm_arch_update_uart(regs, save);

	__raw_writel(save->ulcon, regs + S3C2410_ULCON);
	__raw_writel(save->ucon,  regs + S3C2410_UCON);
	__raw_writel(save->ufcon, regs + S3C2410_UFCON);
	__raw_writel(save->umcon, regs + S3C2410_UMCON);
	__raw_writel(save->ubrdiv, regs + S3C2410_UBRDIV);

	if (pm_uart_udivslot)
		__raw_writel(save->udivslot, regs + S3C2443_DIVSLOT);
}

static void s3c_pm_restore_uarts(void)
{
	struct pm_uart_save *save = uart_save;
	unsigned int uart;

	for (uart = 0; uart < CONFIG_SERIAL_SAMSUNG_UARTS; uart++, save++)
		s3c_pm_restore_uart(uart, save);
}
#else
static void s3c_pm_save_uarts(void) { }
static void s3c_pm_restore_uarts(void) { }
#endif

/* The IRQ ext-int code goes here, it is too small to currently bother
 * with its own file. */

#if defined(CONFIG_MX100) /* ktj */
  unsigned long s3c_irqwake_intmask	= 0xfffffffdL;      // rtc-alarm(1)
//unsigned long s3c_irqwake_intmask	= 0xffffffffL;      // none

//unsigned long s3c_irqwake_eintmask	= 0xfffefffdL;  // low-battery(16), power-key(1)  
//unsigned long s3c_irqwake_eintmask	= 0xfffcfffdL;  // usb-adaptor(17), low-battery(16), power-key(1)  
  unsigned long s3c_irqwake_eintmask	= 0xfbdcfbfdL;  // 26, 21, 17, 16, 10, 1  
#else
  unsigned long s3c_irqwake_intmask	    = 0xffffffddL;  // key i/f(5), rtc-alarm(1)
  unsigned long s3c_irqwake_eintmask	= 0xffffffffL;  // none
#endif

int s3c_irqext_wake(unsigned int irqno, unsigned int state)
{
	unsigned long bit = 1L << IRQ_EINT_BIT(irqno);

	if (!(s3c_irqwake_eintallow & bit))
		return -ENOENT;

	printk(KERN_INFO "wake %s for irq %d\n",
	       state ? "enabled" : "disabled", irqno);

	if (!state)
		s3c_irqwake_eintmask |= bit;
	else
		s3c_irqwake_eintmask &= ~bit;

	return 0;
}

/* ktj add */
/////////////////////////////////

#define eint_offset_x(irq)                (irq)		//janged add _x
#define eint_irq_to_bit_x(irq)            (1 << (eint_offset_x(irq) & 0x7))
#define eint_conf_reg_x(irq)              ((eint_offset_x(irq)) >> 3)
#define eint_filt_reg_x(irq)              ((eint_offset_x(irq)) >> 2)
#define eint_mask_reg_x(irq)              ((eint_offset_x(irq)) >> 3)
#define eint_pend_reg_x(irq)              ((eint_offset_x(irq)) >> 3)

// intr_mode 0x2=>falling edge, 0x3=>rising dege, 0x4=>Both edge
static void s3c_pm_set_eint(unsigned int irq, unsigned int intr_mode)
{
	int offs = (irq);
	int shift;
	u32 ctrl, mask, tmp;

	shift = (offs & 0x7) * 4;
	if((0 <= offs) && (offs < 8)){
		tmp = readl(S5PV210_GPH0CON);
		tmp |= (0xf << shift);
		writel(tmp , S5PV210_GPH0CON);
		/*pull up disable*/
	}
	else if((8 <= offs) && (offs < 16)){
		tmp = readl(S5PV210_GPH1CON);
		tmp |= (0xf << shift);
		writel(tmp , S5PV210_GPH1CON);
	}
	else if((16 <= offs) && (offs < 24)){
		tmp = readl(S5PV210_GPH2CON);
		tmp |= (0xf << shift);
		writel(tmp , S5PV210_GPH2CON);
	}
	else if((24 <= offs) && (offs < 32)){
		tmp = readl(S5PV210_GPH3CON);
		tmp |= (0xf << shift);
		writel(tmp , S5PV210_GPH3CON);
	}
	else{
		printk(KERN_ERR "No such irq number %d", offs);
		return;
	}

#if 0 /* ktj disable */
	/*special handling for keypad eint*/
	if( (24 <= irq) && (irq <= 27))
	{// disable the pull up
		tmp = readl(S5PV210_GPH3PUD);
		tmp &= ~(0x3 << ((offs & 0x7) * 2));	
		writel(tmp, S5PV210_GPH3PUD);
		S3C_PMDBG("S5PV210_GPH3PUD = %x\n",readl(S5PV210_GPH3PUD));
	}
#endif
	
	/*Set irq type*/
	mask = 0x7 << shift;
	ctrl = readl(S5PV210_EINTCON(eint_conf_reg_x(irq)));
	ctrl &= ~mask;
	ctrl |= intr_mode << shift;

	writel(ctrl, S5PV210_EINTCON(eint_conf_reg_x(irq)));
	/*clear mask*/
	mask = readl(S5PV210_EINTMASK(eint_mask_reg_x(irq)));
	mask &= ~(eint_irq_to_bit_x(irq));
	writel(mask, S5PV210_EINTMASK(eint_mask_reg_x(irq)));

	/*clear pending*/
	mask = readl(S5PV210_EINTPEND(eint_pend_reg_x(irq)));
	mask &= (eint_irq_to_bit_x(irq));
	writel(mask, S5PV210_EINTPEND(eint_pend_reg_x(irq)));
	
	/*Enable wake up mask*/
	tmp = readl(S5P_EINT_WAKEUP_MASK);
	tmp &= ~(1 << (irq));
	writel(tmp , S5P_EINT_WAKEUP_MASK);

	S3C_PMDBG("S5PV210_EINTCON = %x\n",readl(S5PV210_EINTCON(eint_conf_reg_x(irq))));
	S3C_PMDBG("S5PV210_EINTMASK = %x\n",readl(S5PV210_EINTMASK(eint_mask_reg_x(irq))));
	S3C_PMDBG("S5PV210_EINTPEND = %x\n",readl(S5PV210_EINTPEND(eint_pend_reg_x(irq))));
	
	return;
}


static void s3c_pm_clear_eint(unsigned int irq)
{
	u32 mask;
	/*clear pending*/
	mask = readl(S5PV210_EINTPEND(eint_pend_reg(irq)));
	mask &= (eint_irq_to_bit(irq));
	writel(mask, S5PV210_EINTPEND(eint_pend_reg(irq)));
}

//////////////////////////////////
/* ktj end */


/* helper functions to save and restore register state */

/**
 * s3c_pm_do_save() - save a set of registers for restoration on resume.
 * @ptr: Pointer to an array of registers.
 * @count: Size of the ptr array.
 *
 * Run through the list of registers given, saving their contents in the
 * array for later restoration when we wakeup.
 */
void s3c_pm_do_save(struct sleep_save *ptr, int count)
{
	for (; count > 0; count--, ptr++) {
		ptr->val = __raw_readl(ptr->reg);
		S3C_PMDBG("saved %p value %08lx\n", ptr->reg, ptr->val);
	}
}

/**
 * s3c_pm_do_restore() - restore register values from the save list.
 * @ptr: Pointer to an array of registers.
 * @count: Size of the ptr array.
 *
 * Restore the register values saved from s3c_pm_do_save().
 *
 * Note, we do not use S3C_PMDBG() in here, as the system may not have
 * restore the UARTs state yet
*/

void s3c_pm_do_restore(struct sleep_save *ptr, int count)
{
	for (; count > 0; count--, ptr++) {
	//	printk(KERN_DEBUG "restore %p (restore %08lx, was %08x)\n",
	//	       ptr->reg, ptr->val, __raw_readl(ptr->reg));

		__raw_writel(ptr->val, ptr->reg);
	}
}

/**
 * s3c_pm_do_restore_core() - early restore register values from save list.
 *
 * This is similar to s3c_pm_do_restore() except we try and minimise the
 * side effects of the function in case registers that hardware might need
 * to work has been restored.
 *
 * WARNING: Do not put any debug in here that may effect memory or use
 * peripherals, as things may be changing!
*/

void s3c_pm_do_restore_core(struct sleep_save *ptr, int count)
{
	for (; count > 0; count--, ptr++)
		__raw_writel(ptr->val, ptr->reg);
}

/* s3c2410_pm_show_resume_irqs
 *
 * print any IRQs asserted at resume time (ie, we woke from)
*/
static void s3c_pm_show_resume_irqs(int start, unsigned long which,
				    unsigned long mask)
{
	int i;

	which &= ~mask;

	for (i = 0; i <= 31; i++) {
		if (which & (1L<<i)) {
			S3C_PMDBG("IRQ %d asserted at resume\n", start+i);
		}
	}
}

#ifdef PROCESS_WKUP_SRC
static void s3c_process_wakeup_source(void)
{
	unsigned int tmp;

    /* cdma host (21) */
	tmp = __raw_readl(S5PV210_EINT2PEND);
    if (tmp & 0x20)
    {
       	printk("=== wakeup by cdma host ===\n");
    }    

    /* wifi host (26) */
	tmp = __raw_readl(S5PV210_EINT3PEND);
    if (tmp & 0x04)
    {
       	printk("=== wakeup by wifi host ===\n");
    } 

    /* bt host (10) */
	tmp = __raw_readl(S5PV210_EINT1PEND);
    if (tmp & 0x04)
    {
       	printk("=== wakeup by bt host ===\n");
    } 



	/* rtc alarm (1) */
	tmp = __raw_readl(S5P_WAKEUP_STAT);
    if (tmp & 0x02)
    {
       	printk("=== wakeup by rtc alarm ===\n");
#ifdef IRIVER_WAKEUP_TEST
        set_request_screenon(1);
#endif
    }    

    /* power key (1) */
	tmp = __raw_readl(S5PV210_EINT0PEND);
    if (tmp & 0x02)
    {
       	printk("=== wakeup by power key ===\n");
        set_request_screenon(1);
   	//	s3c_pm_clear_eint(1);   // ---> crash
    }    
    
    /* pmic lowbat (16) */
	tmp = __raw_readl(S5PV210_EINT2PEND);
    if (tmp & 0x01)
    {
       	printk("=== wakeup by pmic lowbat ===\n");
        set_request_screenon(1);
        set_request_poweroff();
    }    

    /* usb insert (17) */
	tmp = __raw_readl(S5PV210_EINT2PEND);
    if (tmp & 0x02)
    {
       	printk("=== wakeup by usb ===\n");
        set_request_screenon(1);
    }    

}
#endif

void (*pm_cpu_prep)(void);
void (*pm_cpu_sleep)(void);

#define any_allowed(mask, allow) (((mask) & (allow)) != (allow))

#ifdef CONFIG_REGULATOR
static int s3c_pm_begin(suspend_state_t state)
{
	return regulator_suspend_prepare(state);
}
#endif

#define CP3_UNUSED_CLK_MASK 0x001e0000//Shanghai ewada
/* s3c_pm_enter
 *
 * central control for sleep/resume process
*/

//janged modify sleep crash
static int start_sleep_state = 0;
void set_start_sleep_state(int value)
{
	start_sleep_state = value;
	printk("[KERNEL] %s start_sleep_state = %d =================\n", __FUNCTION__, start_sleep_state);
}

int get_start_sleep_state(void)
{
	return start_sleep_state;
}
EXPORT_SYMBOL(get_start_sleep_state);
EXPORT_SYMBOL(set_start_sleep_state);

static int s3c_pm_enter(suspend_state_t state)
{
#ifndef USE_DMA_ALLOC
	static unsigned long regs_save[16];
#endif /* !USE_DMA_ALLOC */
	unsigned int tmp;


	//janged
	start_sleep_state = 1;
	/* ensure the debug is initialised (if enabled) */


	s3c_pm_debug_init();

#ifdef PROCESS_WKUP_SRC
   set_request_screenon(0);
#endif

	S3C_PMDBG("%s(%d)\n", __func__, state);

	if (pm_cpu_prep == NULL || pm_cpu_sleep == NULL) {
		printk(KERN_ERR "%s: error: no cpu sleep function\n", __func__);
		start_sleep_state = 0;
		return -EINVAL;
	}

	/* check if we have anything to wake-up with... bad things seem
	 * to happen if you suspend with no wakeup (system will often
	 * require a full power-cycle)
	*/
#ifdef DISABLE_WAKEUP_RTC
	s3c_irqwake_intmask = 0xFFFF; // none
#else
	s3c_irqwake_intmask = 0xFFFD; // rtc_alarm(1)
#endif

	if (!any_allowed(s3c_irqwake_intmask, s3c_irqwake_intallow) &&
	    !any_allowed(s3c_irqwake_eintmask, s3c_irqwake_eintallow)) {
		printk(KERN_ERR "%s: No wake-up sources!\n", __func__);
		printk(KERN_ERR "%s: Aborting sleep\n", __func__);
		start_sleep_state = 0;
		return -EINVAL;
	}

	/* ktj, If pmic INT occured while entering sleep mode, then return. */
	if( __raw_readl(S5PV210_EINT2PEND) & 0x1 ) 
	{
		set_request_poweroff();
		printk(KERN_ERR "%s: Aborting sleep because pmic INT\n", __func__);
		start_sleep_state = 0;
		return -EINVAL;
	}

	/* store the physical address of the register recovery block */

#ifndef USE_DMA_ALLOC
	s3c_sleep_save_phys = virt_to_phys(regs_save);
#else
	__raw_writel(phy_regs_save, S5P_INFORM2);
#endif /* !USE_DMA_ALLOC */

	/* set flag for sleep mode idle2 flag is also reserved */
	__raw_writel(SLEEP_MODE, S5P_INFORM1);

	S3C_PMDBG("s3c_sleep_save_phys=0x%08lx\n", s3c_sleep_save_phys);

	/* save all necessary core registers not covered by the drivers */

	s3c_pm_save_gpios();
	s3c_pm_save_uarts();
	s3c_pm_save_core();

///	s3c_config_sleep_gpio(); /* ktj */

	/* set the irq configuration for wake */
#ifdef DISABLE_WAKEUP_WIFI_BT
//  s3c_irqwake_eintmask	= 0xfffcfffd;   // disable wifi, bt, 3g
    s3c_irqwake_eintmask	= 0xffdcfffd;   // disable wifi, bt
#else
    s3c_irqwake_eintmask	= 0xfbdcfbfd;
#endif
	s3c_pm_configure_extint();

	S3C_PMDBG("sleep: irq wakeup masks: %08lx,%08lx\n",
		s3c_irqwake_intmask, s3c_irqwake_eintmask);

	/* Set wake up sources */
#if defined(CONFIG_MX100)   /* ktj */
    #ifdef DISABLE_WAKEUP_WIFI_BT
	s3c_pm_set_eint( 1, 0x02);  // power-key    : falling     
	s3c_pm_set_eint(16, 0x02);  // pmic-lowbat  : falling  
	s3c_pm_set_eint(17, 0x02);  // usb-adaptor  : falling 
	s3c_pm_set_eint(21, 0x01);  // cdma-host    : high
    #else
	s3c_pm_set_eint( 1, 0x02);  // power-key    : falling     
	s3c_pm_set_eint(10, 0x02);  // bt-host      : falling            
	s3c_pm_set_eint(16, 0x02);  // pmic-lowbat  : falling  
	s3c_pm_set_eint(17, 0x02);  // usb-adaptor  : falling
	s3c_pm_set_eint(21, 0x03);  // cdma-host    : rising  
	s3c_pm_set_eint(26, 0x03);  // wifi-host    : rising
    #endif
#endif

///	s3c_pm_arch_prepare_irqs();

	/* call cpu specific preparation */
	pm_cpu_prep();

	/* flush cache back to ram */
	flush_cache_all();

	s3c_pm_check_store();

	__raw_writel(s3c_irqwake_intmask, S5P_WAKEUP_MASK);

	/*clear for next wakeup*/
	tmp = __raw_readl(S5P_WAKEUP_STAT);
	__raw_writel(tmp, S5P_WAKEUP_STAT);

s3c_config_sleep_gpio(); // move here

	/* Enable PS_HOLD pin to avoid reset failure */
    __raw_writel((0x5 << 12 | 0x1<<9 | 0x1<<8 | 0x1<<0),S5P_PSHOLD_CONTROL);

	/* send the cpu to sleep... */
	s3c_pm_arch_stop_clocks();

	/* s3c_cpu_save will also act as our return point from when
	 * we resume as it saves its own register state and restores it
	 * during the resume.  */
	s3c_cpu_save(regs_save);

	/* restore the cpu state using the kernel's cpu init code. */
	cpu_init();

	/* restore the system state */

	s3c_pm_restore_core();
	
#if 0
	/*Reset the uart registers*/
	__raw_writel(0x0, S3C24XX_VA_UART3+S3C2410_UCON);
	__raw_writel(0xf, S3C24XX_VA_UART3+S5P_UINTM);
	__raw_writel(0xf, S3C24XX_VA_UART3+S5P_UINTSP);
	__raw_writel(0xf, S3C24XX_VA_UART3+S5P_UINTP);
	__raw_writel(0x0, S3C24XX_VA_UART2+S3C2410_UCON);
	__raw_writel(0xf, S3C24XX_VA_UART2+S5P_UINTM);
	__raw_writel(0xf, S3C24XX_VA_UART2+S5P_UINTSP);
	__raw_writel(0xf, S3C24XX_VA_UART2+S5P_UINTP);
	__raw_writel(0x0, S3C24XX_VA_UART1+S3C2410_UCON);
	__raw_writel(0xf, S3C24XX_VA_UART1+S5P_UINTM);
	__raw_writel(0xf, S3C24XX_VA_UART1+S5P_UINTSP);
	__raw_writel(0xf, S3C24XX_VA_UART1+S5P_UINTP);
	__raw_writel(0x0, S3C24XX_VA_UART0+S3C2410_UCON);
	__raw_writel(0xf, S3C24XX_VA_UART0+S5P_UINTM);
	__raw_writel(0xf, S3C24XX_VA_UART0+S5P_UINTSP);
	__raw_writel(0xf, S3C24XX_VA_UART0+S5P_UINTP);
#endif
	
	s3c_pm_restore_uarts();
	s3c_pm_restore_gpios();


#ifdef PROCESS_WKUP_SRC
	printk("\n\n\n\n\n");
    s3c_process_wakeup_source();    /* ktj proc_wkup_drc */	//janged move here
#endif

	/*clear for next wakeup*/
	tmp = __raw_readl(S5P_WAKEUP_STAT);
	__raw_writel(tmp, S5P_WAKEUP_STAT);


#if 0 // ktj disable
    //Shanghai ewada
	tmp = __raw_readl(S5P_CLKGATE_IP3);
	__raw_writel(tmp | CP3_UNUSED_CLK_MASK, S5P_CLKGATE_IP3);
#endif
	
	s3c_pm_debug_init();

	/* check what irq (if any) restored the system */

	s3c_pm_arch_show_resume_irqs();

	S3C_PMDBG("%s: post sleep, preparing to return\n", __func__);
	
	s3c_pm_check_restore();

	/* ok, let's return from sleep */

	S3C_PMDBG("S3C PM Resume (post-restore)\n");
	
	return 0;
}

/* callback from assembly code */
void s3c_pm_cb_flushcache(void)
{
	flush_cache_all();
}

static int s3c_pm_prepare(void)
{
	/* prepare check area if configured */

	s3c_pm_check_prepare();
	return 0;
}

static void s3c_pm_finish(void)
{
	s3c_pm_check_cleanup();
}

static struct platform_suspend_ops s3c_pm_ops = {
#ifdef CONFIG_REGULATOR
	.begin		= s3c_pm_begin,
#endif
	.enter		= s3c_pm_enter,
	.prepare	= s3c_pm_prepare,
	.finish		= s3c_pm_finish,
	.valid		= suspend_valid_only_mem,
};

/* s3c_pm_init
 *
 * Attach the power management functions. This should be called
 * from the board specific initialisation if the board supports
 * it.
*/

int __init s3c_pm_init(void)
{
	printk("S3C Power Management, Copyright 2004 Simtec Electronics\n");

#ifdef USE_DMA_ALLOC
	regs_save = dma_alloc_coherent(NULL, 4096, &phy_regs_save, GFP_KERNEL);
	if (regs_save == NULL) {
		printk(KERN_ERR "DMA alloc error\n");
		return -1;
	}
#endif /* USE_DMA_ALLOC */

	suspend_set_ops(&s3c_pm_ops);
	return 0;
}
