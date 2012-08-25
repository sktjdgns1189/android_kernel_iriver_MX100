
#ifndef	_MX100_KEYPAD_H_
#define	_MX100_KEYPAD_H_

	#define TRUE 				1
	#define FALSE 				0
	
	#define	ON					1
	#define	OFF					0
	
	#define	KEY_PRESS			1
	#define	KEY_RELEASE			0
	
	#define	KEYPAD_STATE_BOOT	0
	#define	KEYPAD_STATE_RESUME	1
	
	// keypad sampling rate
	#define	PERIOD_10MS			(HZ/100)	// 10ms
	#define	PERIOD_20MS			(HZ/50)		// 20ms
	#define	PERIOD_50MS			(HZ/20)		// 50ms

	// power off timer	
	#define	PERIOD_1SEC			(1*HZ)		// 1sec
	#define	PERIOD_3SEC			(3*HZ)		// 3sec
	#define	PERIOD_5SEC			(5*HZ)		// 5sec

	// Keypad wake up delay
	#define	KEYPAD_WAKEUP_DELAY		100		// 1 sec delay
	
	//
	// Power Off Enable
	//
	#define	PSHOLD_CONTROL			(*(unsigned long *)S5P_PSHOLD_CONTROL)
	#define	POWER_OFF_ENABLE()		{	PSHOLD_CONTROL = 0;	}
	
	//
	// Popwer off port
	//
	//External Interrupt Control Registers (EXT_INT_1_CON, R/W, Address = 0xE020_0E04)

	//#define ENABLE_POWER_KEY
	
	#if 1 //MX100
	#define	POWER_CONTROL_PORT	(S5PV210_GPH0(1))
	#else
	#define	POWER_CONTROL_PORT	(S5PV210_GPH0(0))
	#endif
	
	#define	POWER_CONTROL_STR	"POWER CONTROL"
	
	//
	// Power LED Port
	//
	#define	POWER_LED_PORT		(S5PV210_GPC1(2))
	#define	POWER_LED_STR		"POWER LED"

	//
	// HDMI Connect event(Switch event used)
	//
	#define	SW_HDMI				0x06

	//
	// Key Hold event(Switch event used)
	//
	#define	SW_KEY_HOLD			0x07

	//
	// Screen rotate event
	// 
	#define	SW_SCREEN_ROTATE	0x08
	

	typedef	struct	mx100_keypad__t	{
		
		// keypad control
		struct input_dev	*driver;			// input driver
		struct timer_list 	rd_timer;			// keyscan timer

		// power off control
		struct timer_list	poweroff_timer;		// long power key process

		// sysfs used		
		unsigned char		hold_state;			// key hold off
		unsigned char		sampling_rate;		// 10 msec sampling
		unsigned char		poweroff_time;		// reset off
		
		unsigned int		wakeup_delay;		// key wakeup delay

		#if 0       // slide switch status
			unsigned char	slide_sw_state;
		#endif

	}	mx100_keypad_t;
	
	extern	mx100_keypad_t	mx100_keypad;

#ifdef CONFIG_MX100_EVM
	#define ENABLE_KEY_CAMERA
	#define ENABLE_CROSSKEY_ON_CAMERA_KEY
//	#define INSTEAD_VOL_MENU

#elif defined(CONFIG_MX100_WS)
//	#define INSTEAD_VOL_MENU
#endif

// #define ENABLE_POWER_LONG_KEY   // 2011.02.08
	
#endif	
