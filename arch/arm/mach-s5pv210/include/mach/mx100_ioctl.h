#ifndef _MX100_IOCTL_
#define _MX100_IOCTL_

#include <linux/fs.h>
#include <linux/syscalls.h>


#include <asm/unistd.h> //#include <linux/unistd.h>
#include <asm/uaccess.h> //#include <linux/unistd.h>

// 
// mx100 define 
//
#ifdef CONFIG_MX100_IOCTL


#ifdef CONFIG_MX100_EVM

#else
#define CONFIG_CDMA_IOCTL
#endif


#define ENABLE_DEBUG_FILTER

//#define ENABLE_DISPLAY_CALLER




//
//
//
//  debugging filter list.
//
//
#define MAKE_ID(a,b,c,d)     (((a)<<24) | ((b)<<16) | ((c)<<8) | (d))


#define DBG_FILTER_NO_OUTPUT  		(0)
#define DBG_FILTER_TEMP				(0x1)
#define DBG_FILTER_02				(0x2)
#define DBG_FILTER_TOUCH_XY 			(0x4)
#define DBG_FILTER_TOUCH_KEY		(0x8)
#define DBG_FILTER_TOUCH_TUNING		(0x10)
#define DBG_FILTER_RTC				(0x20)
#define DBG_FILTER_07				(0x40)    
#define DBG_FILTER_UART				(0x80)
#define DBG_FILTER_WM8993			(0x100)
#define DBG_FILTER_WM8993_REG		(0x200)
#define DBG_FILTER_WM8993_SLV		(0x400)
#define DBG_FILTER_WM8993_MIXER		(0x800)
#define DBG_FILTER_WM8993_POP		(0x1000)
#define DBG_FILTER_MX100_JACK		(0x2000)
#define DBG_FILTER_WM8993_ALSA_ROUTE (0x4000)
#define DBG_FILTER_PM			(0x8000)
#define DBG_FILTER_I2S_FM_MCLK	(0x10000)
#define DBG_FILTER_18	(0x20000)
#define DBG_FILTER_19	(0x40000)
#define DBG_FILTER_20	(0x80000)
#define DBG_FILTER_21	(0x100000)
#define DBG_FILTER_22	(0x200000)
#define DBG_FILTER_23	(0x400000)
#define DBG_FILTER_24	(0x800000)
#define DBG_FILTER_25	(0x1000000)
#define DBG_FILTER_26	(0x2000000)
#define DBG_FILTER_27	(0x4000000)
#define DBG_FILTER_28	(0x8000000)
#define DBG_FILTER_29	(0x10000000)
#define DBG_FILTER_30	(0x20000000) 
#define DBG_FILTER_31	(0x40000000) 




#define DEB_ALL_MESS   0xFFFF

extern unsigned int g_MX100_DEBUG_FILTER;
extern int g_MX100_DEBUG_FILTER_ENABLE;
extern int g_MX100_PRINTK_ENABLE ;
extern int g_MX100_DEBUG_RIGH ;

void mx100_debug_filter(int enable);
void mx100_printk_enable(int enable);
void mx100_debug_rich(int enable);
void mx100_debug_filter_set(int filter,int setreset);

int mx100_gpio_debug_out(void);

#endif
asmlinkage int mx100_printk(const char *fmt, ...);

#ifdef ENABLE_DISPLAY_CALLER
#define PRINT_CALLER  do {   __backtrace_mx100();  } while (0)	
#else
#define PRINT_CALLER  
#endif

#ifdef ENABLE_DEBUG_FILTER
#define MX100_DBG(n, args...) do {  if ((g_MX100_DEBUG_FILTER) & (n)){   if(g_MX100_DEBUG_RIGH)  mx100_printk("[%x %s:%s:%d]:\n",n,__FILE__, __FUNCTION__,__LINE__);     mx100_printk(args);} } while (0)
#define MX100_GPIO_DBG(args...) do {   mx100_printk("[%d %s:%s:%d]:\n",mx100_gpio_debug_out(),__FILE__, __FUNCTION__,__LINE__);     mx100_printk(args); } while (0)

#else
#define MX100_DBG(n, args...) 
#define MX100_GPIO_DBG( args...)
#endif



#define  MX100_IOCTL_DEV_NAME            "mx100ioctldev"  
#define   MX100_IOCTL_DEV_MAJOR            240  

typedef struct 
{ 
	unsigned int reg;
	unsigned int value;
	unsigned int value1;
	unsigned int value2;	
	unsigned int size;  
	unsigned char buff[128];  
} __attribute__ ((packed)) mx100ioctldev_info; 

  
#define MX100_IOCTL_MAGIC    't'  

// wm8993 sound codec.
#define MX100_WM8993_CODECOFF           _IO(  MX100_IOCTL_MAGIC, 0 )  
#define MX100_WM8993_CODECON            _IO(  MX100_IOCTL_MAGIC, 1 )  
#define MX100_WM8993_DEBUG         		_IOWR( MX100_IOCTL_MAGIC, 2 , mx100ioctldev_info )  
  
#define MX100_WM8993_REG_READ       _IOWR( MX100_IOCTL_MAGIC, 5 , mx100ioctldev_info )  
#define MX100_WM8993_REG_WRITE            _IOW( MX100_IOCTL_MAGIC, 6 , mx100ioctldev_info )  

#define MX100_WM8993_HWMUTEOFF           _IO(  MX100_IOCTL_MAGIC, 7 )  
#define MX100_WM8993_HWMUTEON            _IO(  MX100_IOCTL_MAGIC, 8 )  
#define MX100_WM8993_SET_PATH_FM         _IO(  MX100_IOCTL_MAGIC, 9 )  

// aat1271a flash led driver.
#define MX100_AAT1271A_FLASH_WRITE     _IOW( MX100_IOCTL_MAGIC, 10 , mx100ioctldev_info )  
#define MX100_AAT1271A_FLASH_EN            _IOW( MX100_IOCTL_MAGIC, 11 , mx100ioctldev_info )  
#define MX100_AAT1271A_FLASH_SET           _IOW( MX100_IOCTL_MAGIC, 12 , mx100ioctldev_info )  
#define MX100_AAT1271A_CAMERA_CTL           _IOW( MX100_IOCTL_MAGIC, 13 , mx100ioctldev_info )  
#define MX100_AAT1271A_MOVIE_CTL           _IOW( MX100_IOCTL_MAGIC, 14 , mx100ioctldev_info )  


// max17040 battery fuel gage.
#define MX100_MAX17040_REG_READ       _IOWR( MX100_IOCTL_MAGIC, 20 , mx100ioctldev_info )  
#define MX100_MAX17040_REG_WRITE            _IOW( MX100_IOCTL_MAGIC, 21 , mx100ioctldev_info )  

// touch.
#define MX100_MELFASTS_REG_READ       _IOWR( MX100_IOCTL_MAGIC, 30 , mx100ioctldev_info )  
#define MX100_MELFASTS_REG_WRITE     _IOW( MX100_IOCTL_MAGIC, 31 , mx100ioctldev_info )  
#define MX100_MELFASTS_KEY_LED_CTL   _IOW( MX100_IOCTL_MAGIC, 32 , mx100ioctldev_info )  
#define MX100_MELFASTS_LOG_START  	 _IOW( MX100_IOCTL_MAGIC, 33 , mx100ioctldev_info )  
#define MX100_MELFASTS_LOG_STOP  	 _IOW( MX100_IOCTL_MAGIC, 34 , mx100ioctldev_info )  
#define MX100_MELFASTS_PRINT_DEBUG 	 _IOW( MX100_IOCTL_MAGIC, 35 , mx100ioctldev_info )  
#define MX100_MELFASTS_DEBUG         	_IOWR( MX100_IOCTL_MAGIC, 36 , mx100ioctldev_info )  


#define MX100_CPU_REG_READ       	_IOWR( MX100_IOCTL_MAGIC, 40 , mx100ioctldev_info )  
#define MX100_CPU_REG_WRITE            _IOW( MX100_IOCTL_MAGIC, 41 , mx100ioctldev_info )  
#define MX100_GET_MX100_HW            _IOW( MX100_IOCTL_MAGIC, 42 , mx100ioctldev_info )  
#define MX100_GET_MX100_MP_MODE  _IOW( MX100_IOCTL_MAGIC, 43 , mx100ioctldev_info )  
#define MX100_GET_MX100_SYSTEM_LOG  _IOW( MX100_IOCTL_MAGIC, 44 , mx100ioctldev_info )  

#define MX100_WIFI_POWER       	_IOWR( MX100_IOCTL_MAGIC, 50 , mx100ioctldev_info )  
#define MX100_WIFI_RESET                 _IOW( MX100_IOCTL_MAGIC, 51 , mx100ioctldev_info )  


#define MX100_CDMA_POWER       	_IOWR( MX100_IOCTL_MAGIC, 60 , mx100ioctldev_info )  
#define MX100_CDMA_RESET                 _IOW( MX100_IOCTL_MAGIC, 61 , mx100ioctldev_info )  
#define MX100_CDMA_SLEEP       		_IOWR( MX100_IOCTL_MAGIC, 62 , mx100ioctldev_info )  

#define MX100_DEBUG_FILTER_ENABLE   	_IOWR( MX100_IOCTL_MAGIC, 70 , mx100ioctldev_info )  
#define MX100_PRINTK_ENABLE   		_IOWR( MX100_IOCTL_MAGIC, 71 , mx100ioctldev_info )  
#define MX100_DEBUG_RIGH   		_IOWR( MX100_IOCTL_MAGIC, 72 , mx100ioctldev_info )  
#define MX100_DEBUG_PANIC   		_IOWR( MX100_IOCTL_MAGIC, 73 , mx100ioctldev_info )  

#define MX100_SET_SOFTVOL	   	_IOWR( MX100_IOCTL_MAGIC, 80 , mx100ioctldev_info )  
#define MX100_GET_SOFTVOL	   	_IOWR( MX100_IOCTL_MAGIC, 81 , mx100ioctldev_info )  
#define MX100_SET_MEDIA_VOL	   	_IOWR( MX100_IOCTL_MAGIC, 82 , mx100ioctldev_info )  
#define MX100_GET_MEDIA_VOL	   	_IOWR( MX100_IOCTL_MAGIC, 83 , mx100ioctldev_info )  

#define MX100_SET_FMVOL	   		_IOWR( MX100_IOCTL_MAGIC, 90 , mx100ioctldev_info )  

#define MX100_CDMA_USB_ON	   		_IOWR( MX100_IOCTL_MAGIC, 91 , mx100ioctldev_info )  
#define MX100_CDMA_USB_OFF	   		_IOWR( MX100_IOCTL_MAGIC, 92 , mx100ioctldev_info )  


#define MX100_MT9P111_REG_READ       _IOWR( MX100_IOCTL_MAGIC,100 , mx100ioctldev_info )  
#define MX100_MT9P111_REG_WRITE     _IOW( MX100_IOCTL_MAGIC, 101 , mx100ioctldev_info )  

#define MX100_MT9M113_REG_READ       _IOWR( MX100_IOCTL_MAGIC,104 , mx100ioctldev_info )  
#define MX100_MT9M113_REG_WRITE     _IOW( MX100_IOCTL_MAGIC, 105 , mx100ioctldev_info )  


#define MX100_IOCTL_MAXNR                                   9


struct ts_kernel_file_io
{
  struct file *file;
  mm_segment_t old_fs ;
  int fd;
  char filename[128];
  int log_size;
  char *logbuffer;
  int file_pos;
};

int k_open(struct ts_kernel_file_io *ps_kfi,char *filename,int flags, int mode);
void k_close(struct ts_kernel_file_io *ps_kfi);
int k_write(struct ts_kernel_file_io *ps_kfi,char  *buf,size_t count);

int klog_open(struct ts_kernel_file_io *ps_kfi,char *filename,int logsize);
int klog_printf(struct ts_kernel_file_io *ps_kfi,const char *fmt, ...);

__kernel_loff_t mx100_get_tick_count(void);

  #ifdef CONFIG_SND_SMDK_WM8993               

// wm8993 codec ctl
int WM8993_WRITE(unsigned int reg, unsigned int val);
int WM8993_READ(unsigned char reg);
void mx100_hp_hw_mute(int mute);
void WM8993_SET_PATH_FM(void);
void WM8993_debug(int onoff);
void WM8993_debug_error(int onoff);
void WM8993_debug_slv(int onoff);

void mx100_set_soft_vol(unsigned int vol);
int mx100_get_soft_vol(void);

int mx100_is_playback(void);
void mx100_set_playback(int status);
int mx100_is_capture(void);
void mx100_set_capture(int status);

#endif


#ifdef CONFIG_AAT1271A_FLASH_DRIVER
// aat1271a flash led ctl
void mx100_aat1271a_flash_write(int addr,int data);
void mx100_aat1271a_flash_en(int value);
void mx100_aat1271a_flash_set(int value);
void mx100_aat1271a_camera_control(int value);
void mx100_aat1271a_movie_control(int onoff,int value);

#endif

#ifdef CONFIG_BATTERY_MAX17040
int mx100_max17040_reg_write(int addr,int data);
int mx100_max17040_reg_read(int addr);
#endif

#ifdef CONFIG_TOUCHSCREEN_MELFAS
int mx100_melfasts_reg_write(int addr,int data);
int mx100_melfasts_reg_read(int addr);
int mx100_melfasts_key_led(int onoff);
int mx100_melfasts_log_start(char *logfile);
int mx100_melfasts_print_log(char *logMsg);
int mx100_melfasts_log_stop(void);
#endif

#ifdef CONFIG_BCM4329
void mx100_bcm_power(int onoff);
void mx100_bcm_reset(int onoff);
#endif

// 5mp cam.
int MT9P111_WRITE(unsigned int reg, unsigned int val);
int MT9P111_READ(unsigned int reg);

// front cam.
int MT9M113_WRITE(unsigned int reg, unsigned int val);
int MT9M113_READ(unsigned int reg);

int mx100_enable_system_log(void);
extern int g_mx100_system_log;


#endif
