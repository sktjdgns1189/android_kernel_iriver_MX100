
#ifndef	_MX100_KEYCODE_H_
#define	_MX100_KEYCODE_H_
#include "mx100_keypad.h"

//Å×½ºÆ®


#ifdef CONFIG_MX100_EVM

		int MX100_Keycode[] = {
				KEY_BACK,		
				KEY_MENU,
				KEY_HOME,				
				KEY_VOLUMEUP,
				KEY_POWER,
				KEY_VOLUMEDOWN,
				
				#ifdef ENABLE_KEY_CAMERA
				KEY_CAMERA
				#endif
		};
		#ifdef INSTEAD_VOL_MENU
		int MX100_Keycode2[] = {
				KEY_BACK,		
				KEY_MENU,
				KEY_HOME,				
				KEY_MENU,
				KEY_POWER,
				KEY_BACK,
				#ifdef ENABLE_KEY_CAMERA
				KEY_CAMERA
				#endif
		};
		#endif

		int MX100_KeycodeAllow[] = {
				KEY_BACK,		
				KEY_MENU,
				KEY_HOME,				
				KEY_VOLUMEUP,
				KEY_POWER,
				KEY_VOLUMEDOWN,
				
				#ifdef ENABLE_KEY_CAMERA
				KEY_CAMERA,
				#endif
				KEY_LEFT,		
				KEY_DOWN,
				KEY_ENTER,				
				KEY_RIGHT,
				KEY_UP			
		};

		#define	MAX_KEYCODE_CNT (sizeof(MX100_Keycode) / sizeof(int))
	
		#if	defined(DEBUG_MSG)
			const char *MX100_KeyMapStr[MAX_KEYCODE_CNT] = {
					"KEY_BACK\n",
					"KEY_MENU\n",
					"KEY_HOME\n",
					"KEY_VOLUMEUP\n",
					"KEY_POWER\n",	
					"KEY_VOLUMEDOWN\n",
					#ifdef ENABLE_KEY_CAMERA
					"KEY_CAMERA\n"
					#endif					
			};
		#endif	// DEBUG_MSG

		#ifdef ENABLE_CROSSKEY_ON_CAMERA_KEY
		
		int MX100_Keycode_Shift[] = {
				KEY_LEFT,		
				KEY_DOWN,
				KEY_ENTER,				
				KEY_RIGHT,
				KEY_RIGHT,
				KEY_UP,				
				#ifdef ENABLE_KEY_CAMERA
				KEY_CAMERA
				#endif
				
		};
		
	
		#if	defined(DEBUG_MSG)
			const char *MX100_KeyMapStr_Shift[MAX_KEYCODE_CNT] = {
					"KEY_LEFT\n",
					"KEY_DOWN\n",
					"KEY_ENTER\n",
					"KEY_RIGHT\n",
					"KEY_POWER\n",	
					"KEY_UP\n",
					#ifdef ENABLE_KEY_CAMERA
					"KEY_CAMERA\n"
					#endif					
			};
		#endif	// DEBUG_MSG

		#endif

		
#elif defined(CONFIG_MX100_WS)
		int MX100_Keycode[] = {
				KEY_VOLUMEUP,
				KEY_POWER,
				KEY_VOLUMEDOWN
		};

		#ifdef INSTEAD_VOL_MENU
		int MX100_Keycode2[] = {
				KEY_MENU,
				KEY_POWER,
				KEY_BACK
		};
		#endif

		int MX100_KeycodeAllow[] = {
				KEY_VOLUMEUP,
				KEY_POWER,
				KEY_VOLUMEDOWN,
				KEY_HOME,				
				#ifdef INSTEAD_VOL_MENU
				,KEY_MENU,
				KEY_POWER,
				KEY_BACK
				#endif
		};

		#define	MAX_KEYCODE_CNT (sizeof(MX100_Keycode) / sizeof(int))
	
		#if	defined(DEBUG_MSG)
			const char *MX100_KeyMapStr[MAX_KEYCODE_CNT] = {
					#ifdef INSTEAD_VOL_MENU
					"KEY_MENU\n",
					"KEY_POWER\n",	
					"KEY_BACK\n",
					#else
					"KEY_VOLUMEUP\n",
					"KEY_POWER\n",	
					"KEY_VOLUMEDOWN\n",
					#endif
			};
		#endif	// DEBUG_MSG

	
		
#endif		
		
		
#endif		/* _MX100_KEYPAD_H_*/
