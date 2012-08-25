/*
 * mx100_ioctl -- IOCTL for mx100 driver.
 *
 * Copyright 2010 iriver
*  jhlim
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>

#include <linux/io.h>
#include <mach/regs-clock.h>
#include <mach/gpio-bank.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>

#include <linux/miscdevice.h>

#include <mach/mx100_ioctl.h>
#include <linux/file.h>
#include <asm/cacheflush.h>

#ifdef CONFIG_SND_SMDK_WM8993               
#include <sound/wm8993.h>
#endif


//#define DEBUG_MX100IOCTL


#ifdef DEBUG_MX100IOCTL
#define mx100_ioctl_print(fmt, ...) \
	mx100_printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)

#define mx100_ioctl_printerr(fmt, ...) \
        mx100_printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#else
#define mx100_ioctl_print(fmt, ...) 
#define mx100_ioctl_printerr(fmt, ...) 
#endif

unsigned int g_MX100_DEBUG_FILTER =DBG_FILTER_NO_OUTPUT \
	|  DBG_FILTER_TEMP	
//	|  DBG_FILTER_02
//	|  DBG_FILTER_TOUCH_XY 	
//	|  DBG_FILTER_TOUCH_KEY	
//	|  DBG_FILTER_TOUCH_TUNING	
// 	|  DBG_FILTER_RTC
// 	|  DBG_FILTER_07
// 	|  DBG_FILTER_UART
// 	|  DBG_FILTER_WM8993
 //	|  DBG_FILTER_WM8993_REG  //display,read,write reg
//	|  DBG_FILTER_WM8993_SLV 	
//	|  DBG_FILTER_WM8993_MIXER	
// 	|  DBG_FILTER_WM8993_POP
	|  DBG_FILTER_MX100_JACK
//	| DBG_FILTER_WM8993_ALSA_ROUTE
// 	|  DBG_FILTER_PM
// 	|  DBG_FILTER_I2S_FM_MCLK
// 	|  DBG_FILTER_POWER_SUPPELY
// 	|  DBG_FILTER_19
// 	|  DBG_FILTER_20
// 	|  DBG_FILTER_21
// 	|  DBG_FILTER_22
// 	|  DBG_FILTER_23
// 	|  DBG_FILTER_24
// 	|  DBG_FILTER_25
// 	|  DBG_FILTER_26
// 	|  DBG_FILTER_27
// 	|  DBG_FILTER_28
// 	|  DBG_FILTER_29
// 	|  DBG_FILTER_30
// 	|  DBG_FILTER_31
;

//
// kernel debugging filter method.
// added 2010.12.15
//


//

int g_MX100_DEBUG_RIGH = 0;
int g_MX100_PRINTK_ENABLE = 1;   // if g_MX100_PRINTK_ENABLE==0 ,all printk message not working. 
int g_MX100_DEBUG_FILTER_ENABLE = 0;

void mx100_printk_enable(int enable)
{
	g_MX100_PRINTK_ENABLE = enable;
}

void mx100_debug_rich(int enable)
{
	g_MX100_DEBUG_RIGH = enable;
}
void mx100_debug_filter(int enable)
{
	g_MX100_DEBUG_FILTER_ENABLE = enable;
}

void mx100_debug_filter_set(int filter,int setreset)
{
	if(setreset==1) {
		g_MX100_DEBUG_FILTER|= filter;
	} else {
		g_MX100_DEBUG_FILTER &= ~filter;
	}
}
#include <linux/kallsyms.h>	


void dump_backtrace_entry_mx100(unsigned long where, unsigned long from, unsigned long frame)
{
#ifdef CONFIG_KALLSYMS
	char sym1[KSYM_SYMBOL_LEN], sym2[KSYM_SYMBOL_LEN];
//	sprint_symbol(sym1, where);
	sprint_symbol(sym2, from);
//	printk("[<%08lx>] (%s) from [<%08lx>] (%s)", where, sym1, from, sym2);
	printk("from (%s)", sym2);
#else
	printk("Function entered at [<%08lx>] from [<%08lx>]\n", where, from);
#endif
 }


static void write_file(char *filename,  unsigned char *data, unsigned int nSize)
{
  struct file *file;
  loff_t pos = 0;
  int fd;
  mm_segment_t old_fs;
  
  dmac_inv_range(data, data + nSize);
  
  old_fs = get_fs();
  set_fs(KERNEL_DS);
  fd = sys_open(filename, O_WRONLY|O_CREAT, 0644);
  if (fd >= 0) {
	sys_write(fd, data, nSize);
    file = fget(fd);
    if (file) {
		vfs_write(file, data, nSize, &pos);
      fput(file);
    }
    sys_close(fd);
  }
  else
  {
  }
  set_fs(old_fs);
  
  dmac_clean_range(data, data + nSize);
  
}


// user kernel log management.

#define DEFAULT_LOG_BUF_SIZE (1024 * 8)
int k_open(struct ts_kernel_file_io *ps_kfi,char *filename,int flags, int mode)
{
#if 0
  ps_kfi->old_fs = get_fs();
  set_fs(KERNEL_DS);
  ps_kfi->fd = sys_open(filename, flags,mode);

  if ( ps_kfi->fd>= 0) {
	printk(KERN_ERR "kopen_file success: %s\n",filename);
  } else {
	printk(KERN_ERR "kopen_file fail: %s\n",filename);
  }

  return ps_kfi->fd;
  #else
   ps_kfi->file_pos = 0;
  if(ps_kfi->log_size==0) {
  	ps_kfi->log_size = DEFAULT_LOG_BUF_SIZE;
  } 

  ps_kfi->logbuffer = kzalloc(ps_kfi->log_size, GFP_KERNEL);
 
  strcpy(ps_kfi->filename,filename);
   return 0; 
  #endif

 
}
int klog_open(struct ts_kernel_file_io *ps_kfi,char *filename,int logsize)
{
	ps_kfi->log_size = logsize;
	
	return k_open(ps_kfi,filename, O_CREAT | O_WRONLY, 0644);
}
int klog_printf(struct ts_kernel_file_io *ps_kfi,const char *fmt, ...)
{
	va_list va;
	char buff[512];
	int buff_len;
	int write_size;
	
	va_start(va, fmt);
	vsprintf(buff, fmt, va);
	va_end(va);
	buff_len = strlen(buff);
	write_size=k_write(ps_kfi,buff,buff_len);

	return write_size;
}
int k_write(struct ts_kernel_file_io *ps_kfi,char  *buf,size_t count)
{
  int writesize;

#if 0
  loff_t pos;

  if (ps_kfi->fd >= 0) {
	sys_write(ps_kfi->fd, buf, count);
    	ps_kfi->file = fget(ps_kfi->fd);
    	if (ps_kfi->file) {
		writesize=vfs_write(ps_kfi->file, (const char __user *)buf, count, &pos);
      		fput(ps_kfi->file);
    	}
  }
  #else
	if(ps_kfi->file_pos > (ps_kfi->log_size-count-1)) {
		writesize = 0;
	} else {
		memcpy(&ps_kfi->logbuffer[ps_kfi->file_pos],buf,count);
		ps_kfi->file_pos  += count;
		writesize = count;
	}
  #endif

  return writesize;
}

void k_close(struct ts_kernel_file_io *ps_kfi)
{
#if 0
  if (ps_kfi->fd >= 0) {
	 sys_close(ps_kfi->fd);
	 set_fs(ps_kfi->old_fs);
	printk(KERN_ERR "close kfile\n");
  }
  #else
	if(ps_kfi->logbuffer) {
  		write_file(ps_kfi->filename,ps_kfi->logbuffer,ps_kfi->file_pos);
	 	kfree(ps_kfi->logbuffer);
		memset(ps_kfi,0x0,sizeof(struct ts_kernel_file_io));
	}
  #endif
  
}

__kernel_loff_t mx100_get_tick_count(void)
{
	__kernel_loff_t tick;

	struct timeval tv;
//	int write_size;
	
	do_gettimeofday(&tv);

	tick = tv.tv_sec * 1000000 + tv.tv_usec;

	return tick;
}

#ifdef CONFIG_CDMA_IOCTL

//CDMA modem power on signal, 1S high 로 유지 후 내리면 전원 on. 다시 1S high 로 유지 후
 //내리면 전원 off.
 
/*Shanghai ewada*/
/*# define DEBUG_CDMA_INFO*/ /*Shanghai ewada delete cdma debug 2011.03.19*/
int mx100_cdma_power(int status)
{
 int i;
//jhlim2011.07.07	  gpio_set_value(GPIO_USBHOST_CHARGE_IC_OUT, 0);
//	  msleep(100);
#ifdef DEBUG_CDMA_INFO
    int readysingal=gpio_get_value(GPIO_CDMA_READY_SIGNAL_IN);

    printk("\n entry mx100_cdma_power status=%d\n",status);
    printk("\n entry power readysingal status=%d\n",readysingal);
#endif

	if(status==1) {
		gpio_set_value(GPIO_CDMA_SLEEP_CTL_OUT, 1);
        msleep(500);
#ifdef DEBUG_CDMA_INFO
        int readysingal=gpio_get_value(GPIO_CDMA_READY_SIGNAL_IN);

        printk("\n entry sleep readysingal status=%d\n",readysingal);
#endif
		return gpio_get_value(GPIO_CDMA_READY_SIGNAL_IN);
	} else {
	 
	  gpio_set_value(GPIO_CDMA_SLEEP_CTL_OUT, 1);

	 
	  if(gpio_get_value(GPIO_CDMA_READY_SIGNAL_IN)==1) {  // if alive
#ifdef DEBUG_CDMA_INFO
            readysingal=gpio_get_value(GPIO_CDMA_READY_SIGNAL_IN);

            printk("\n entry power on readysingal status=%d\n",readysingal);
#endif
		 gpio_set_value(GPIO_CDMA_RESET_OUT, 1);
		 msleep(500);
	  } else {
		 gpio_set_value(GPIO_CDMA_RESET_OUT, 0);
         msleep(500);
		 gpio_set_value(GPIO_CDMA_RESET_OUT, 1);
		 // msleep(500);

	  }

//jhlim2011.07.07	  gpio_set_value(GPIO_USBHOST_CHARGE_IC_OUT, 0);

	  

	   msleep(10);

	   gpio_set_value(GPIO_CDMA_PWR_ON_OUT, 1);
	   msleep(1000);
	   gpio_set_value(GPIO_CDMA_PWR_ON_OUT, 0);
#ifdef DEBUG_CDMA_INFO
        for(i = 0; i < 50; i++){
            readysingal=gpio_get_value(GPIO_CDMA_READY_SIGNAL_IN);
            printk("\n power on readysingal status=%d,i=%d\n",readysingal,i);
            if(readysingal)
                break;
            msleep(100);
        }
        if(i >= 50 && !readysingal)
        {
            printk("\n power on can not get readysingal.\n");
            return 0;
        }
#endif
	//#define 	S5PV210_GPJ2(1)
	//#define GPIO_CDMA_RESET_OUT				S5PV210_GPJ2(2)

		for(i=0;i<40;i++) {
			  if(gpio_get_value(GPIO_CDMA_READY_SIGNAL_IN)==1) {
				mx100_ioctl_print("\nCDMA alived !!!\n");

			        msleep(100);
//jhlim2011.07.07	                        gpio_set_value(GPIO_USBHOST_CHARGE_IC_OUT, 1);
				
				return 1;
			  }
			  msleep(100);
		}

	  msleep(100);
//jhlim2011.07.07	  gpio_set_value(GPIO_USBHOST_CHARGE_IC_OUT, 1);

	mx100_ioctl_print("\nCDMA dead !!!\n");
		
		return 0;
    }
}

/*Shanghai ewada*/
int mx100_cdma_sleep(int status)
{
#ifdef DEBUG_CDMA_INFO
	int readysingal=gpio_get_value(GPIO_CDMA_READY_SIGNAL_IN);
	int wakeup=gpio_get_value(GPIO_CDMA_HOST_WAKEUP_IN);

    printk("\n entry mx100_cdma_sleep status=%d\n",status);
	printk("\n entry sleep readysingal status=%d\n",readysingal);
	printk("\n entry sleep wakeup status=%d\n",wakeup);
#endif
	if(status==1) {
		gpio_set_value(GPIO_CDMA_SLEEP_CTL_OUT, 1);
		msleep(100);
#ifdef DEBUG_CDMA_INFO		
		readysingal=gpio_get_value(GPIO_CDMA_READY_SIGNAL_IN);
		wakeup=gpio_get_value(GPIO_CDMA_HOST_WAKEUP_IN);
		printk("\n wakeup readysingal status=%d\n",readysingal);
		printk("\n sleep wakeup status=%d\n",wakeup);
#endif		
		return gpio_get_value(GPIO_CDMA_READY_SIGNAL_IN);
	}
	else{
		gpio_set_value(GPIO_CDMA_SLEEP_CTL_OUT, 0);
		msleep(100);
		
#ifdef DEBUG_CDMA_INFO		
		readysingal=gpio_get_value(GPIO_CDMA_READY_SIGNAL_IN);
		wakeup=gpio_get_value(GPIO_CDMA_HOST_WAKEUP_IN);
		printk("\n sleep readysingal status=%d\n",readysingal);
		printk("\n wakeup wakup status=%d\n",wakeup);
#endif		
		return gpio_get_value(GPIO_CDMA_READY_SIGNAL_IN);
	}
}

void mx100_cdma_reset(void)
{
	//gpio_set_value(GPIO_CDMA_RESET_OUT, 0);
#ifdef DEBUG_CDMA_INFO
    printk("\n entry mx100_cdma_reset \n");
#endif
	gpio_set_value(GPIO_CDMA_RESET_OUT, 0);
	msleep(500);
	gpio_set_value(GPIO_CDMA_RESET_OUT, 1);
	msleep(10);
#ifdef DEBUG_CDMA_INFO
	int readysingal=gpio_get_value(GPIO_CDMA_READY_SIGNAL_IN);
	printk("\n after reset readysingal status=%d\n",readysingal);
#endif	
//	return gpio_get_value(GPIO_CDMA_READY_SIGNAL_IN); /*JHLIM remove warning */
}
/*Shanghai ewada*/
/*void mx100_cdma_sleep(int onoff)
{
	  gpio_set_value(GPIO_CDMA_SLEEP_CTL_OUT, onoff);
}*/

#endif

int mx100ioctldev_open (struct inode *inode, struct file *filp)  
{  
	mx100_ioctl_printerr(KERN_ERR "mx100 ioctl opened\n");

	return nonseekable_open(inode, filp);
}  
  
int mx100ioctldev_release (struct inode *inode, struct file *filp)  
{  
    return 0;  
}  
 static int g_temp_media_volume = 100;
 
int mx100ioctldev_ioctl (struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)  
{  
  
    mx100ioctldev_info   ctrl_info;  
    int               size;  
    unsigned int virtual_addr,virtual_addr2;
    unsigned int physical_addr;
	
    int ret;
	
//    int               loop;  
      
    if( _IOC_TYPE( cmd ) != MX100_IOCTL_MAGIC ) return -EINVAL;  
    //if( _IOC_NR( cmd )   >= MX100_IOCTL_MAXNR ) return -EINVAL;  
      
    size = _IOC_SIZE( cmd );   
      
    #if 0	
    if( size )  
    {  
        err = 0;  
        if( _IOC_DIR( cmd ) & _IOC_READ  ) err = verify_area( VERIFY_WRITE, (void *) arg, size );  
        else if( _IOC_DIR( cmd ) & _IOC_WRITE ) err = verify_area( VERIFY_READ , (void *) arg, size );  
              
        if( err ) return err;          
    }  
    #endif
		
            
    switch( cmd )  
    {  
	case MX100_CPU_REG_READ:
		ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   

		#if 0
		virtual_addr=__phys_to_virt(ctrl_info.reg);
		#else
		physical_addr =  ctrl_info.reg >> 12;
		physical_addr =  physical_addr <<12;
	
		virtual_addr = (unsigned long)ioremap(physical_addr,0x1000);  // 0x1000  로 떨어지게

		if(virtual_addr) {
			virtual_addr2 = virtual_addr + (ctrl_info.reg % 0x1000);
			if(ctrl_info.size == 1) {
				ctrl_info.value =  readb(virtual_addr2);	
			} else {
				ctrl_info.value =  readl(virtual_addr2);	
			}	
//			writel(ctrl_info.value,virtual_addr2);		
	
			iounmap((void __iomem *)virtual_addr);
		}
		#endif

		
		mx100_ioctl_printerr(KERN_ERR "reg read phy1:%x phy2:%x vir:%x %x\n",ctrl_info.reg,
		physical_addr,
		virtual_addr2,
		ctrl_info.value);
                ret = copy_to_user ( (void *) arg, (const void *) &ctrl_info, (unsigned long ) size );   
		break;
	case MX100_CPU_REG_WRITE:
		ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
		#if 0
		virtual_addr=__phys_to_virt(ctrl_info.reg);
		writel(virtual_addr,ctrl_info.value);		
		
		#else
		physical_addr =  ctrl_info.reg >> 12;
		physical_addr =  physical_addr <<12;
	
		virtual_addr = (unsigned long)ioremap(physical_addr,0x1000);  // 0x1000  로 떨어지게
		
		if(virtual_addr) {
			virtual_addr2 = virtual_addr + (ctrl_info.reg % 0x1000);
			mx100_ioctl_printerr(KERN_ERR "reg write phy1:%x phy2:%x vir:%x %x %d\n",
			ctrl_info.reg,
			physical_addr,
			virtual_addr2,
			ctrl_info.value,
			ctrl_info.size
			);			

			local_irq_disable();

			if(ctrl_info.size == 1) {
				writeb(ctrl_info.value,virtual_addr2);		
			} else {
				writel(ctrl_info.value,virtual_addr2);		
			}
			local_irq_enable();
	
			iounmap((void __iomem *)virtual_addr);
		}
		#endif
		break;
	case MX100_GET_MX100_HW   : 
		{
		extern char  g_mx100_hw_name[8];  // ES,TP,MP,KNOWN
		ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
		strcpy(ctrl_info.buff,g_mx100_hw_name);
                ret = copy_to_user ( (void *) arg, (const void *) &ctrl_info, (unsigned long ) size );   
		mx100_printk("mx100_hw: %s\n",ctrl_info.buff);
		}
		break;

	case MX100_GET_MX100_MP_MODE: 
		{
		extern char  g_mx100_mp_mode[8];  // on ,off
		ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
		strcpy(ctrl_info.buff,g_mx100_mp_mode);
                ret = copy_to_user ( (void *) arg, (const void *) &ctrl_info, (unsigned long ) size );   
		mx100_printk("g_mx100_mp_mode: %s\n",ctrl_info.buff);
		}
		break;

	case MX100_GET_MX100_SYSTEM_LOG: 
		{
		extern char  g_mx100_mp_mode[8];  // on ,off
		ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
		ctrl_info.value =  g_mx100_system_log;
               ret = copy_to_user ( (void *) arg, (const void *) &ctrl_info, (unsigned long ) size );   
		}
		break;

  #ifdef CONFIG_SND_SMDK_WM8993               
   
    case MX100_WM8993_CODECOFF     :
	{

    				}
											
                                break;   
                                  
    case MX100_WM8993_CODECON      :
				{

    				}
                                break;  

	case MX100_WM8993_HWMUTEOFF:
		mx100_hp_hw_mute(0);
		break;

	case MX100_WM8993_HWMUTEON:
		mx100_hp_hw_mute(1);
		break;

	case MX100_WM8993_SET_PATH_FM:
		WM8993_SET_PATH_FM();
		break;
	
	case MX100_WM8993_DEBUG   : 
		ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
		if(ctrl_info.reg==0) {
			WM8993_debug(ctrl_info.value);
		} else if(ctrl_info.reg==1) {
			WM8993_debug_error(ctrl_info.value);
		}
		else if(ctrl_info.reg==2) {
			WM8993_debug_slv(ctrl_info.value);
		}
		mx100_printk("wm8993 debug mode: %d %d\n",ctrl_info.reg,ctrl_info.value);
		
		break;
    case MX100_WM8993_REG_READ : 
				ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
				ctrl_info.value =  WM8993_READ(ctrl_info.reg);						   
                                ret = copy_to_user ( (void *) arg, (const void *) &ctrl_info, (unsigned long ) size );    
                                break;         
    case MX100_WM8993_REG_WRITE      :
				ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
				mx100_ioctl_printerr(KERN_ERR "wm8993 reg write %d %d\n",ctrl_info.reg,ctrl_info.value);
				WM8993_WRITE(ctrl_info.reg,ctrl_info.value);						   
	                        break;        
    #endif	

    case MX100_MT9P111_REG_READ : 
		ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
		ctrl_info.value =  MT9P111_READ(ctrl_info.reg);						   
		ret = copy_to_user ( (void *) arg, (const void *) &ctrl_info, (unsigned long ) size );    
		mx100_ioctl_printerr(KERN_ERR "mt9p111 reg read %d %d\n",ctrl_info.reg,ctrl_info.value);
		
		break;         
    case MX100_MT9P111_REG_WRITE      :
		ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
		mx100_ioctl_printerr(KERN_ERR "mt9p111 reg write %d %d\n",ctrl_info.reg,ctrl_info.value);
		MT9P111_WRITE(ctrl_info.reg,ctrl_info.value);						   
		break;        
    case MX100_MT9M113_REG_READ : 
		ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
		ctrl_info.value =  MT9M113_READ(ctrl_info.reg);						   
		ret = copy_to_user ( (void *) arg, (const void *) &ctrl_info, (unsigned long ) size );    
		break;         
    case MX100_MT9M113_REG_WRITE      :
		ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
		mx100_ioctl_printerr(KERN_ERR "wm8993 reg write %d %d\n",ctrl_info.reg,ctrl_info.value);
		MT9M113_WRITE(ctrl_info.reg,ctrl_info.value);						   
		break;        
		
	#ifdef CONFIG_AAT1271A_FLASH_DRIVER
	case MX100_AAT1271A_FLASH_WRITE:
		ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
		mx100_ioctl_printerr(KERN_ERR "aat1271a reg write %d %d\n",ctrl_info.reg,ctrl_info.value);
		mx100_aat1271a_flash_write(ctrl_info.reg,ctrl_info.value);	
		break;		
	case MX100_AAT1271A_FLASH_EN:
		ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
		mx100_ioctl_printerr(KERN_ERR "MX100_AAT1271A_FLASH_EN %d %d\n",ctrl_info.reg,ctrl_info.value);
		mx100_aat1271a_flash_en(ctrl_info.value);
		break;
	case MX100_AAT1271A_FLASH_SET:
		ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
		mx100_ioctl_printerr(KERN_ERR "MX100_AAT1271A_FLASH_SET reg write %d %d\n",ctrl_info.reg,ctrl_info.value);
		mx100_aat1271a_flash_set(ctrl_info.value);
		break;
	case MX100_AAT1271A_CAMERA_CTL:
		ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
		mx100_ioctl_printerr(KERN_ERR "MX100_AAT1271A_FLASH_SET reg write %d %d\n",ctrl_info.reg,ctrl_info.value);
		mx100_aat1271a_camera_control(ctrl_info.value);
		break;
	case MX100_AAT1271A_MOVIE_CTL:
		ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
		mx100_ioctl_printerr(KERN_ERR "MX100_AAT1271A_FLASH_SET reg write %d %d\n",ctrl_info.reg,ctrl_info.value);
		mx100_aat1271a_movie_control(ctrl_info.value,ctrl_info.value1);
		break;
	#endif
	
	#ifdef CONFIG_BATTERY_MAX17040

    case MX100_MAX17040_REG_READ : 
				ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
				ctrl_info.value =  mx100_max17040_reg_read(ctrl_info.reg);						   
                                ret = copy_to_user ( (void *) arg, (const void *) &ctrl_info, (unsigned long ) size );    
                                break;         
    case MX100_MAX17040_REG_WRITE      :
				ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
				mx100_ioctl_printerr(KERN_ERR "max17040 reg write %d %d\n",ctrl_info.reg,ctrl_info.value);
				mx100_max17040_reg_write(ctrl_info.reg,ctrl_info.value);						   
	                        break;     

	#endif

	#ifdef CONFIG_TOUCHSCREEN_MELFAS

    case MX100_MELFASTS_REG_READ : 
				ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
				ctrl_info.value =  mx100_melfasts_reg_read(ctrl_info.reg);						   
                                ret = copy_to_user ( (void *) arg, (const void *) &ctrl_info, (unsigned long ) size );    
                                break;         
    case MX100_MELFASTS_REG_WRITE      :
				ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
				mx100_ioctl_printerr(KERN_ERR "melfasts reg write %d %d\n",ctrl_info.reg,ctrl_info.value);
				mx100_melfasts_reg_write(ctrl_info.reg,ctrl_info.value);						   
	                        break;     
    case MX100_MELFASTS_KEY_LED_CTL:
				ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
				mx100_ioctl_printerr(KERN_ERR "melfasts key led ctl %d\n",ctrl_info.value);
				mx100_melfasts_key_led(ctrl_info.value);
	                        break;     
    case MX100_MELFASTS_LOG_START:
				ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
				mx100_ioctl_printerr(KERN_ERR "melfasts log start: %s\n",ctrl_info.buff);
				mx100_melfasts_log_start(ctrl_info.buff);
	                        break;     
    case MX100_MELFASTS_LOG_STOP:
				ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
				mx100_ioctl_printerr(KERN_ERR "melfasts log stop\n");
				mx100_melfasts_log_stop();
	                        break;     
	case MX100_MELFASTS_DEBUG   : 
		ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
		if(ctrl_info.reg==0) {
			mx100_debug_filter_set(DBG_FILTER_TOUCH_XY,ctrl_info.value);
		} else if(ctrl_info.reg==1) {
			mx100_debug_filter_set(DBG_FILTER_TOUCH_KEY,ctrl_info.value);
		}
		
		mx100_printk("melfasts debug mode: %d %d\n",ctrl_info.reg,ctrl_info.value);						
	#endif

	#ifdef CONFIG_BCM4329

	case MX100_WIFI_POWER:
				ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
				mx100_ioctl_printerr(KERN_ERR "wifi power %d\n",ctrl_info.value);
				mx100_bcm_power(ctrl_info.value);		
				break;
	case MX100_WIFI_RESET:
				ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
				mx100_ioctl_printerr(KERN_ERR "wifi reset %d\n",ctrl_info.value);
				mx100_bcm_reset(ctrl_info.value);				
				break;
	#endif

	#ifdef CONFIG_CDMA_IOCTL
	
	case MX100_CDMA_POWER:
				ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
				ctrl_info.value = mx100_cdma_power(ctrl_info.reg);					   
                                ret = copy_to_user ( (void *) arg, (const void *) &ctrl_info, (unsigned long ) size );    					
				break;
	/*Shanghai ewada*/
	case MX100_CDMA_SLEEP:
				ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size ); 
				ctrl_info.value = mx100_cdma_sleep(ctrl_info.reg);
				mx100_ioctl_print("\nMX100_CDMA_SLEEP ret=%d.\n",ctrl_info.value);
                            ret = copy_to_user ( (void *) arg, (const void *) &ctrl_info, (unsigned long ) size );
                break;
	case MX100_CDMA_RESET:
				mx100_ioctl_printerr(KERN_ERR "cdma reset %d\n",ctrl_info.value);
				mx100_cdma_reset();		
				break;
	/*Shanghai ewada*/
/*	case MX100_CDMA_SLEEP:
				ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
				mx100_ioctl_printerr(KERN_ERR "cdma sleep %d\n",ctrl_info.value);
				mx100_cdma_sleep(ctrl_info.value);		
				break;*/

				
				
	#endif

	#ifdef  ENABLE_DEBUG_FILTER	
	case MX100_DEBUG_FILTER_ENABLE:
				ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
				mx100_printk("debug filter %d\n",ctrl_info.value);
				mx100_debug_filter(ctrl_info.value);
				break;
	case MX100_PRINTK_ENABLE:
				ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
				mx100_printk("printk enable %d\n",ctrl_info.value);
				mx100_printk_enable(ctrl_info.value);
				break;
				
	case MX100_DEBUG_RIGH:
				ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
				mx100_printk("debug rich enable %d\n",ctrl_info.value);
				mx100_debug_rich(ctrl_info.value);
				break;
	#endif
	case MX100_DEBUG_PANIC:
			{
				ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
				char *test=0;
				*test = 0;
			}	
				break;

	case MX100_SET_SOFTVOL:	 
				ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
				mx100_ioctl_printerr(KERN_ERR "soft vol %d\n",ctrl_info.value);
				mx100_set_soft_vol(ctrl_info.value);
				break;
    	case MX100_GET_SOFTVOL : 
				ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
				ctrl_info.value =  mx100_get_soft_vol();				
				mx100_ioctl_printerr(KERN_ERR "get soft vol %d\n",ctrl_info.value);				
                                ret = copy_to_user ( (void *) arg, (const void *) &ctrl_info, (unsigned long ) size );    
				break;
	case MX100_SET_MEDIA_VOL:	 
				ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
				g_temp_media_volume = ctrl_info.value;
				break;
    	case MX100_GET_MEDIA_VOL : 
				ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
				ctrl_info.value =  g_temp_media_volume;			
                                ret = copy_to_user ( (void *) arg, (const void *) &ctrl_info, (unsigned long ) size );    
				break;


	case MX100_SET_FMVOL:
			{
				extern void fm_volume_tuning(int vol);
				ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, size );   
				mx100_ioctl_printerr(KERN_ERR "fm vol %d\n",ctrl_info.value);
				fm_volume_tuning(ctrl_info.value);
			}
		break;
        case MX100_CDMA_USB_ON:
                        {
#if defined(CONFIG_MX100_REV_TP)
                /* added 2011.02.28 JHLIM? select cdma pclink mode from boot menu.*/
		printk(KERN_ERR "CDMA USB is PCLINK MODE !\n");//-> CDMA upgrade
		s3c_gpio_cfgpin(GPIO_CDMA_USB_ON_OUT, S3C_GPIO_OUTPUT);
                s3c_gpio_setpull(GPIO_CDMA_USB_ON_OUT, S3C_GPIO_PULL_NONE);
                gpio_set_value(GPIO_CDMA_USB_ON_OUT, GPIO_LEVEL_HIGH);
                s3c_gpio_cfgpin(GPIO_USB_SW, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_USB_SW, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_USB_SW, GPIO_LEVEL_HIGH);
		s3c_gpio_cfgpin(GPIO_USB_SW_OE, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_USB_SW_OE, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_USB_SW_OE, GPIO_LEVEL_HIGH);
#endif
                        }
                break;
        case MX100_CDMA_USB_OFF:
                        {
#if defined(CONFIG_MX100_REV_TP)
        printk(KERN_ERR "CDMA USB is Normal MODE !\n"); //-> Mass Storage
//???????? ??? gpio_set_value(GPIO_CDMA_USB_ON_OUT, GPIO_LEVEL_LOW);
//???????? ??? gpio_set_value(GPIO_USB_SW, GPIO_LEVEL_LOW);
	s3c_gpio_cfgpin(GPIO_USB_SW_OE,S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_USB_SW_OE,S3C_GPIO_PULL_NONE);
	gpio_set_value(GPIO_USB_SW_OE,GPIO_LEVEL_HIGH);
#endif
                        }
                break;

	default: break;
    }  
  
    return 0;  
}  


int mx100_gpio_debug_out(void)
{
static int g_debug_toggle = 0;

g_debug_toggle ^=1;

#ifdef CONFIG_MX100_REV_TP
	  gpio_set_value(GPIO_DEBUG_OUT, g_debug_toggle);
#else
#endif
return g_debug_toggle;
}

 /*  JHL 2011.06.02  */
 
 int g_mx100_system_log = 0;
static int __init system_log_setting(char *system_log)
{
	if(system_log) {
		if(strcmp(system_log,"on")==0) {
			g_mx100_system_log = 1;
		}  else if(strcmp(system_log,"off")==0) {
			g_mx100_system_log = 0;
		}
	} 
	return 1; 
}
__setup("systemlog=", system_log_setting);

int mx100_enable_system_log(void)
{
	return g_mx100_system_log;
}
 
struct file_operations mx100ioctldev_fops =  
{  
    .owner    = THIS_MODULE,  
    .ioctl    = mx100ioctldev_ioctl,  
    .open     = mx100ioctldev_open,       
    .release  = mx100ioctldev_release,    
};  
  

static struct miscdevice mx100_ioctl_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "mx100ioctldev",
    .fops = &mx100ioctldev_fops,
};

static int __init mx100_ioctl_modinit(void)
{
	int ret;

    if( (ret = misc_register(&mx100_ioctl_misc_device)) < 0 )
	    {
		mx100_ioctl_printerr(KERN_ERR "regist mx100_ioctl drv failed \n");
		misc_deregister(&mx100_ioctl_misc_device);
	    }

	return ret;
}
module_init(mx100_ioctl_modinit);

static void __exit mx100_ioctl_exit(void)
{
	 misc_deregister(&mx100_ioctl_misc_device);
}

module_exit(mx100_ioctl_exit);


MODULE_DESCRIPTION("mx100 peri ioctl driver");
MODULE_AUTHOR("jaehwanlim <jaehwan.lim@iriver.com>");
MODULE_LICENSE("GPL");
