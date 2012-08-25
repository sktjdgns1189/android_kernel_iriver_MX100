/*
 * wm8993_mx100.c -- WM8993 set Sound Path driver.
 *
 * Copyright 2011 Iriver Inc.
 *
 * Author: JH LIM <jaehwan.lim@iriver.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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
#include <sound/wm8993.h>
#include <linux/io.h>
#include "wm8993.h"
#include "wm_hubs.h"

#include <mach/regs-clock.h>

#include <mach/gpio-bank.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/mutex.h>

#include <linux/miscdevice.h>


#ifdef CONFIG_MX100_IOCTL
#include <mach/mx100_ioctl.h>
#endif

#include <mach/mx100_jack.h>

extern int get_fm_output(void);
extern void fm_start_for_codec(void);
extern int fm_stop(void);

extern unsigned int wm8993_read(struct snd_soc_codec *codec,
				unsigned int reg);
extern int wm8993_write(struct snd_soc_codec *codec, unsigned int reg,
			unsigned int value);
extern struct snd_soc_codec *wm8993_codec;

//static int wm8993_route_set_fm_path(int fm_path);

//[yoon 20110428]HDMI output status
#ifdef CONFIG_MX100_REV_TP
extern int mx100_hdmi_status;
#endif

 /* JHLIM  */

// added Alsa route path.


static enum e_mic_path g_mic_path;
static enum e_playback_path g_playback_path;
static enum e_fmradio_path g_fm_path;

static enum e_h2w_path g_h2w_path;

int g_fm_volume = -1;

static int wm8993_route_get_mic_path(void)
{

	MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"wm8993_route_get_mic_path : %d\n",g_mic_path);

	return g_mic_path;
}

 

static int wm8993_route_set_mic_path(int mic_path)
{
	extern int g_fm_on;

	MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"wm8993_route_set_mic_path : %d\n",mic_path);

      if(mic_path == MIC_OFF) {
//		wm8993_setpath_mic(0);
	//	msleep(100);
		//MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"MIC_OFF\n");
		wm8993_setpath_mic_off();
		if(g_fm_on==1) {
		//	wm8993_setpath_fm_on(1);
			// 2011.05.13 fix for VOCA APK problem	
			//mx100_wm8993_input_output_mix_route(MIX_ON, WIC_FM,MIX_ON,WOS_MIXIN);
		}
      }
      else if(mic_path == MIC_MAIN) {
		wm8993_setpath_mic(MIC_MAIN);
		//MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"MIC_MAIN\n");		
      } else if(mic_path==MIC_EXT) {
		wm8993_setpath_mic(MIC_EXT);
		//MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"MIC_MAIN\n");		
      } 
	return 0;
}

int wm8993_route_get_fm_path(void)
{
	MX100_DBG(DBG_FILTER_WM8993_MIXER,"wm8993_route_get_fm_path : %d\n",g_fm_path);

	return g_fm_path;
}
/*JHLIM 2011.04.13  : not use below code */
#if 0
static int wm8993_route_set_fm_path(int fm_path)
{
	g_fm_path = fm_path;

	if (FMR_OFF == g_fm_path)
	{
		MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"FMR_OFF start\n");
		#ifdef CONFIG_I2S_FM_FEED_MCLK
		mx100_wm8993_mute(MUTE_ON,WMS_FM);
		mx100_wm8993_power(POWER_OFF,WPS_FM_OUT);
		msleep(500);

		#else
		mx100_wm8993_mute(MUTE_ON,WMS_FM |WMS_MIXOUT);
		wm8993_internal_clk_opt(0);
		mx100_wm8993_power(POWER_OFF,WPS_FM_OUT | WPS_MIXOUT);

		msleep(100);
		#endif
		
	//	MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"FMR_OFF end\n");
	}
	else if(FMR_SPK == g_fm_path)
	{
		//MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"FMR_HP start\n");
		#ifdef ENABLE_DELAY_TUNING
		__kernel_loff_t start =  mx100_get_tick_count();
		#endif
		
		wm8993_setpath_fm_speaker_play();

		#ifdef ENABLE_DELAY_TUNING
		MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"FMR SPK ELAPSED : %d\n" , (unsigned int)(mx100_get_tick_count() - start) );
		#endif
		
		//MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"FMR_HP end\n");	
	}
	else if(FMR_HP == g_fm_path)
	{
		//MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"FMR_HP start\n");
		#ifdef ENABLE_DELAY_TUNING
		__kernel_loff_t start =  mx100_get_tick_count();
		#endif

		wm8993_setpath_fm_hp_play();

		#ifdef ENABLE_DELAY_TUNING
		MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"FMR HP ELAPSED : %d\n" , (unsigned int)(mx100_get_tick_count() - start) );
		#endif

		//MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"FMR_HP end\n");	
	}
	
	return 0;
}
#endif

static int wm8993_route_get_h2w_path(void)
{
	MX100_DBG(DBG_FILTER_WM8993_MIXER,"get h2w path : %d\n",g_h2w_path);

	return g_h2w_path;
}

 int wm8993_route_set_h2w_path(int h2w_path)
{
	g_h2w_path = h2w_path;
	return g_h2w_path;
}

 int wm8993_route_get_playback_path(void)
{
	MX100_DBG(DBG_FILTER_WM8993_MIXER,"get playback path : %d\n",g_playback_path);

	return g_playback_path;
}

 int wm8993_route_set_playback_path(int playback_path)
{
	if(playback_path == g_playback_path) {
		MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"Already route playback_path\n");
		return 0;
	}
	g_playback_path = playback_path;

	if(g_playback_path==PLAYBACK_OFF) {

	}
	else if(g_playback_path==RCV) {

	}
	else if(g_playback_path==HP3P || g_playback_path==HP4P) {
		wm8993_setpath_headset_play();
		
	} 
	else if(g_playback_path==SPK) {
		wm8993_setpath_speaker_play();
	} else if(g_playback_path==SPK_OFF) {
		MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"SPK_OFF\n");


	} else if(g_playback_path==HP_OFF) {
		MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"HP_OFF\n");
	}

	return 0;
}



//=====================================================================
// codec path control for mx100.
// jhlim.
//
//======================================================================



#define SET (1)
#define CLEAR (0)


#define TUNING_MP3_OUTPUTL_VOL                  0x35  //  2011.01.07 output 586mv
#define TUNING_MP3_OUTPUTR_VOL                  0x35  //

#define TUNING_MP3_OPGAL_VOL                    0x39 // 20h
#define TUNING_MP3_OPGAR_VOL                    0x39 // 21h

#define WM8994_VMID_SEL_NORMAL		0x0002
#define WM8994_CP_ENA_DEFAULT		0x1F25

void wm8993_set_bit(struct snd_soc_codec *codec,unsigned int reg,
	int setbit_vaue,int clearbit_value,int setclear)
{
	int val;
	val = wm8993_read(codec, reg);

	if(setclear ==1) {
		val |= setbit_vaue;
	} else {
		val &= ~clearbit_value;
	}
	wm8993_write(codec, reg, val);
}

void wm8993_set_dac_parameter(void)
{
	u16 val;
	struct snd_soc_codec *codec;

	if (wm8993_codec) {
		codec = wm8993_codec;
	} else {
		return;
	}


	wm8993_write(codec, WM8993_LEFT_DAC_DIGITAL_VOLUME, 0xBE | WM8993_DAC_VU);  //JHL  -1.125DB fix. in order to S/N ratio tuning.

	wm8993_write(codec, WM8993_RIGHT_DAC_DIGITAL_VOLUME, 0xBE | WM8993_DAC_VU);  //  -1.125DB fix.

	val = wm8993_read(codec, WM8993_LEFT_OPGA_VOLUME);
	val &= ~(WM8993_MIXOUTL_MUTE_N_MASK|WM8993_MIXOUTL_VOL_MASK);
	val |= (WM8993_MIXOUT_VU|TUNING_MP3_OPGAL_VOL);
	wm8993_write(codec, WM8993_LEFT_OPGA_VOLUME, val);

	val = wm8993_read(codec, WM8993_RIGHT_OPGA_VOLUME);
	val &= ~(WM8993_MIXOUTR_MUTE_N_MASK|WM8993_MIXOUTR_VOL_MASK);
	val |= (WM8993_MIXOUT_VU|TUNING_MP3_OPGAR_VOL);
	wm8993_write(codec, WM8993_RIGHT_OPGA_VOLUME, val);


}

void mx100_wm8993_setpath_headset_route(eWM8993_HEADSET_SOURCE from_source)
{
	u16 val;
	u16 TestReturn1=0;
	u16 TestReturn2=0;
	u16 TestLow1=0;
	u16 TestHigh1=0;
	u8 TestLow=0;
	u8 TestHigh=0;

	struct snd_soc_codec *codec;
	if (wm8993_codec) {
		codec = wm8993_codec;
	} else {
		return;
	}


//	MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"mx100 headset on\n");

	// Headset Control
	
	val = wm8993_read(codec, WM8993_LEFT_OUTPUT_VOLUME);
	val &= ~(WM8993_HPOUT1L_MUTE_N_MASK|WM8993_HPOUT1L_VOL_MASK);
	val |= (WM8993_HPOUT1_VU|TUNING_MP3_OUTPUTL_VOL);
	wm8993_write(codec, WM8993_LEFT_OUTPUT_VOLUME, val);

	val = wm8993_read(codec, WM8993_RIGHT_OUTPUT_VOLUME);
	val &= ~(WM8993_HPOUT1R_MUTE_N_MASK|WM8993_HPOUT1R_VOL_MASK);
	val |= (WM8993_HPOUT1_VU|TUNING_MP3_OUTPUTR_VOL);
	wm8993_write(codec, WM8993_RIGHT_OUTPUT_VOLUME, val);

	val = wm8993_read(codec, WM8993_DC_SERVO_1  ); 	
	val &= ~(0x03E0);
	val = 0x03E0;
	wm8993_write(codec,WM8993_DC_SERVO_1,val);


	if(from_source==HEADSET_FROM_DAC) {
		// Enable DAC1L to HPOUT1L path
		val = wm8993_read(codec, WM8993_OUTPUT_MIXER1);
		val |=   WM8993_DACL_TO_HPOUT1L;
		wm8993_write(codec, WM8993_OUTPUT_MIXER1, val);

		// Enable DAC1R to HPOUT1R path
		val = wm8993_read(codec, WM8993_OUTPUT_MIXER2);
		val |=   WM8993_DACR_TO_HPOUT1R;
	//		val &= ~0b0000000100000000;
		wm8993_write(codec, WM8993_OUTPUT_MIXER2, val);
	} else if(from_source == HEADSET_FROM_MIXOUT) {
		// Enable DAC1L to HPOUT1L path
		val = wm8993_read(codec, WM8993_OUTPUT_MIXER1);
		val &=   ~WM8993_DACL_TO_HPOUT1L;
		wm8993_write(codec, WM8993_OUTPUT_MIXER1, val);

		// Enable DAC1R to HPOUT1R path
		val = wm8993_read(codec, WM8993_OUTPUT_MIXER2);
		val &=   ~WM8993_DACR_TO_HPOUT1R;
	//		val &= ~0b0000000100000000;
		wm8993_write(codec, WM8993_OUTPUT_MIXER2, val);
	}

	//Enable Charge Pump	
	val = wm8993_read(codec, WM8993_CHARGE_PUMP_1);
	val &= ~(WM8993_CP_ENA_MASK);
	val |= (WM8993_CP_ENA|WM8994_CP_ENA_DEFAULT); // this is from wolfson
	wm8993_write(codec, WM8993_CHARGE_PUMP_1, val);

	msleep(5);// 20ms delay

	val = wm8993_read(codec, WM8993_DC_SERVO_0  ); 	
	val &= ~(0x0303);
	val = 0x0303;
	wm8993_write(codec,WM8993_DC_SERVO_0,val);

	msleep(160);	// 160ms delay

	TestReturn1=wm8993_read(codec,WM8993_DC_SERVO_3);

	TestLow=(signed char)(TestReturn1 & 0xff);
	TestHigh=(signed char)((TestReturn1>>8) & 0xff);

	TestLow1=((signed short)(TestLow-5))&0x00ff;
	TestHigh1=(((signed short)(TestHigh-5)<<8)&0xff00);
	TestReturn2=TestLow1|TestHigh1;
	wm8993_write(codec,WM8993_DC_SERVO_3, TestReturn2);

	val = wm8993_read(codec, WM8993_DC_SERVO_0  ); 	
	val &= ~(0x000F);
	val = 0x000F;
	wm8993_write(codec,WM8993_DC_SERVO_0,val);

	msleep(20);
	
	
	// Intermediate HP settings
	val = (WM8993_HPOUT1L_RMV_SHORT|WM8993_HPOUT1L_DLY|WM8993_HPOUT1R_RMV_SHORT|
		WM8993_HPOUT1R_DLY);
	wm8993_write(codec, WM8993_ANALOGUE_HP_0, val);
}

void wm8993_setpath_headset_play(void)
{
//	u16 val; 
	struct snd_soc_codec *codec;
	if (wm8993_codec) {
		codec = wm8993_codec;
	} else {
		return;
	}

	MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"wm8993_setpath_headset_play\n");

#if 1
	wm8993_write(codec, WM8993_CLASS_W_0, 0);
#endif

	#ifdef ENABLE_PLAYBACK_FM
	mx100_wm8993_mute(MUTE_ON,WMS_HPOUT| WMS_SPKOUT | WMS_ANALOGUE_HP);

	mx100_wm8993_input_output_mix_route(MIX_NOP, WIC_NONE,MIX_ON,WOS_DAC);
	mx100_wm8993_setpath_headset_route(HEADSET_FROM_MIXOUT);

	wm8993_set_dac_parameter();

	mx100_wm8993_power(POWER_ON,WPS_MAIN | WPS_PLAYBACK | WPS_HP | WPS_MIXOUT);
	mx100_wm8993_power(POWER_OFF,WPS_SPK);

	mx100_wm8993_mute(MUTE_OFF,WMS_HPOUT| WMS_ANALOGUE_HP | WMS_MIXOUT);

	#else
	mx100_wm8993_mute(MUTE_ON, WMS_HPOUT| WMS_SPKOUT | WMS_ANALOGUE_HP);
	mx100_wm8993_power(POWER_ON,WPS_MAIN | WPS_PLAYBACK | WPS_HP);

	msleep(20);
	
	mx100_wm8993_setpath_headset_route(HEADSET_FROM_DAC);
	wm8993_set_dac_parameter();

	mx100_wm8993_power(POWER_OFF,WPS_SPK);

	mx100_wm8993_mute(MUTE_OFF,WMS_HPOUT| WMS_ANALOGUE_HP);

	#endif
	


}

void wm8993_setpath_speaker_route(eWM8993_SPEAKER_SOURCE from_source)
{
	u16 val;
	struct snd_soc_codec *codec;
	if (wm8993_codec) {
		codec = wm8993_codec;
	} else {
		return;
	}


//	MX100_DBG(DBG_FILTER_MX100_JACK,"mx100 headset on\n");

// 34 SPKMIXL Attenuation
	val = 0x0;  // JHL 2011.02.07 adjust the SPK gain.
	//	val &=   ~0b11; // SPK mixer mute;
	wm8993_write(codec, WM8993_SPKMIXL_ATTENUATION, val);

// 35 SPKMIXR Attenuation
	val = wm8993_read(codec, WM8993_SPKMIXR_ATTENUATION);
	val = 0x0;  // JHL 2011.02.07 adjust the SPK gain.
	//	val &=   ~0b11; // SPK mixer mute;
	wm8993_write(codec, WM8993_SPKMIXR_ATTENUATION, val);



// 38 Speaker Volume Left
	val = wm8993_read(codec, WM8993_SPEAKER_VOLUME_LEFT);
		val = 0b0000000001111111 | WM8993_SPKOUT_VU; 
//		val = 0b0000000001111100 | WM8993_SPKOUT_VU;  //2011.02.07 SPK POST GAIN 0x3c 
//		val &= ~0b1000000;

	wm8993_write(codec, WM8993_SPEAKER_VOLUME_LEFT, val);

// 39 Speaker Volume Right
	val = wm8993_read(codec, WM8993_SPEAKER_VOLUME_RIGHT);
	val = 0b0000000001111111 | WM8993_SPKOUT_VU; 
//		val = 0b0000000001111100 | WM8993_SPKOUT_VU;  //2011.02.07 SPK POST GAIN 0x3c 
//		val &= ~0b1000000;
	wm8993_write(codec, WM8993_SPEAKER_VOLUME_RIGHT, val);

//	wm8993_write(codec, WM8993_SPKOUT_BOOST, 0b0000000000011011);
//   2011.03.04 Speaker Tuning.

	wm8993_write(codec, WM8993_SPKOUT_BOOST, 0b0000000000010010);
//   2011.03.08 Speaker Tuning.
//	msleep(5);// 20ms delay

	// 36 SPKOUT Mixers
	wm8993_write(codec, WM8993_SPKOUT_MIXERS, WM8993_SPKMIXL_TO_SPKOUTL | WM8993_SPKMIXR_TO_SPKOUTR );


	if(from_source==SPEAKER_FROM_DAC) {

	// 54 Speaker Mixer
		wm8993_write(codec, WM8993_SPEAKER_MIXER, WM8993_DACL_TO_SPKMIXL | WM8993_DACR_TO_SPKMIXR);
	
	} else if(from_source==SPEAKER_FROM_MIXOUT) {

	// 54 Speaker Mixer
		wm8993_write(codec, WM8993_SPEAKER_MIXER, WM8993_MIXOUTL_TO_SPKMIXL | WM8993_MIXOUTR_TO_SPKMIXR);
	}

	val = wm8993_read(codec, WM8993_DC_SERVO_0);
	val &= ~(0x0303);
	val = (0x0303);
	wm8993_write(codec, WM8993_DC_SERVO_0, val);

	msleep(160); // 160ms delay

	val = wm8993_read(codec, WM8993_DC_SERVO_0);
	val &= ~(0x000F);
	val = (0x000F);
	wm8993_write(codec, WM8993_DC_SERVO_0, val);
	
	msleep(20);

}

void wm8993_setpath_speaker_play(void)
{

	u16 val;

	struct snd_soc_codec *codec;
	if (wm8993_codec) {
		codec = wm8993_codec;
	} else {
		return;
	}
	MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"\nwm8993_setpath_speaker_play\n");

	mx100_wm8993_mute(MUTE_ON,WMS_SPKOUT);
	msleep(20);
	
	mx100_wm8993_power(POWER_ON,WPS_MAIN | WPS_PLAYBACK | WPS_SPK | WPS_MIXOUT);
	mx100_wm8993_power(POWER_OFF,WPS_HP);

	#ifdef ENABLE_PLAYBACK_FM
	mx100_wm8993_input_output_mix_route(MIX_NOP, WIC_NONE,MIX_ON,WOS_DAC);
	wm8993_setpath_speaker_route(SPEAKER_FROM_MIXOUT);
	#else
	wm8993_setpath_speaker_route(SPEAKER_FROM_DAC);
	#endif

#if 1
	wm8993_write(codec, WM8993_CLASS_W_0, 0);
#endif

	wm8993_set_dac_parameter();

	val = wm8993_read(codec, WM8993_DC_SERVO_0);
	val &= ~(0x0303);
	val = (0x0303);
	wm8993_write(codec, WM8993_DC_SERVO_0, val);

//	msleep(160); // 160ms delay

	val = wm8993_read(codec, WM8993_DC_SERVO_0);
	val &= ~(0x000F);
	val = (0x000F);
	wm8993_write(codec, WM8993_DC_SERVO_0, val);
	
//	msleep(20);

	mx100_wm8993_mute(MUTE_OFF,WMS_DAC | WMS_DACLR |WMS_MIXOUT | WMS_SPKOUT);

}



//#define USING_SIDETONE
// added 2011.03.04 , for chicking the MIC noise.
#define USING_ADC_HPF_CUT_VOICE   // request samsy. 2011.03.08

void wm8993_setpath_mic(enum e_mic_path mic_path)
{
	u16 val;

	struct snd_soc_codec *codec;
	if (wm8993_codec) {
		codec = wm8993_codec;
	} else {
		return;
	}
	MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"\nwm8993_setpath_mic %s\n",mic_path == MIC_MAIN ? "MIC" : "EXTMIC");

	mx100_wm8993_mute(MUTE_ON,WMS_MIC | WMS_EXT_MIC | WMS_FM);

//	msleep(50);
	

	#ifdef  USING_SIDETONE
	wm8993_setpath_headset_play();
	mx100_wm8993_mute(MUTE_OFF, WMS_HPOUT);

	#else

//	mx100_wm8993_power(POWER_OFF,WPS_PLAYBACK);

//	msleep(100);
	#endif
//	msleep(10);

//13 Digital side Tone
	#ifdef USING_SIDETONE  // Disable side tone.
	val = wm8993_read(codec, WM8993_DIGITAL_SIDE_TONE);

	if(mic_path ==MIC_MAIN)  {   
		val =  0b0000111011100101;
	} else if(mic_path==MIC_EXT)  {   
		val =  0b0000001000101010;
	} 
	wm8993_write(codec, WM8993_DIGITAL_SIDE_TONE, val);
	#else
	wm8993_write(codec, WM8993_DIGITAL_SIDE_TONE, 0b0000111011100000);
	#endif

	#ifdef USING_ADC_HPF_CUT_VOICE
	if(mic_path ==MIC_MAIN)  {   
		wm8993_write(codec, WM8993_ADC_CTRL, 0b0000001100100000);
	} else if(mic_path==MIC_EXT)  {   
		wm8993_write(codec, WM8993_ADC_CTRL, 0b0000001100000000);
	} 	
	#endif
	
// * 15   15 Left ADC Digital Volume
	val = wm8993_read(codec, WM8993_LEFT_ADC_DIGITAL_VOLUME);
//	val = 0b0000000011111111;
	val = 0b0000000111111111 | WM8993_ADC_VU;  // =>2011.02.04  increase MIC volume. 
	wm8993_write(codec, WM8993_LEFT_ADC_DIGITAL_VOLUME, val);

// * 16   16 Right ADC Digital Volume
	val = wm8993_read(codec, WM8993_RIGHT_ADC_DIGITAL_VOLUME);
//	val = 0b0000000011111111;
	val = 0b0000000111111111 | WM8993_ADC_VU;  // =>2011.02.04  increase MIC volume. 
	wm8993_write(codec, WM8993_RIGHT_ADC_DIGITAL_VOLUME, val);
 
// 24 WM8993_LEFT_LINE_INPUT_1_2_VOLUME
	val = wm8993_read(codec, WM8993_LEFT_LINE_INPUT_1_2_VOLUME);
	val =  0b0000000100011111 | WM8993_IN1_VU;   // =>2011.02.04  increase MIC volume. 
	wm8993_write(codec, WM8993_LEFT_LINE_INPUT_1_2_VOLUME, val);

// 26 WM8993_RIGHT_LINE_INPUT_1_2_VOLUME
	val = wm8993_read(codec, WM8993_RIGHT_LINE_INPUT_1_2_VOLUME);
//	val =  0b0000000100001110 | WM8993_IN1_VU;   // =>2011.02.04  increase MIC volume. 	
	val =  0b0000000100011100 | WM8993_IN1_VU;   // =>2011.05.29  
	wm8993_write(codec, WM8993_RIGHT_LINE_INPUT_1_2_VOLUME, val);

	mx100_wm8993_input_output_mix_route(MIX_OFF, WIC_FM,MIX_OFF,WOS_MIXIN); // 2011.05.13 fix for VOCA APK problem

	if(mic_path ==MIC_MAIN)  {   
		val = wm8993_read(codec, WM8993_AUDIO_INTERFACE_1);
		val |= WM8993_AIFADCR_SRC; 
		val &= ~WM8993_AIFADCL_SRC; 		
		wm8993_write(codec, WM8993_AUDIO_INTERFACE_1, val);
		mx100_wm8993_input_output_mix_route(MIX_ON, WIC_MIC,MIX_NOP,WOS_NONE);
		mx100_wm8993_input_output_mix_route(MIX_OFF, WIC_EXT_MIC,MIX_NOP,WOS_NONE);
		#ifdef  USING_SIDETONE
		mx100_wm8993_power(POWER_ON,WPS_MAIN | WPS_MIXIN | WPS_MIC |WPS_CAPTURE|WPS_HP);
		#else
		mx100_wm8993_power(POWER_ON,WPS_MAIN |WPS_MIXIN |WPS_MIC | WPS_CAPTURE);
		#endif
	//	msleep(100);

	}
	else if(mic_path==MIC_EXT) {
		val = wm8993_read(codec, WM8993_AUDIO_INTERFACE_1);
		val |= WM8993_AIFADCL_SRC; 
		val &= ~WM8993_AIFADCR_SRC; 		
		wm8993_write(codec, WM8993_AUDIO_INTERFACE_1, val);


		mx100_wm8993_input_output_mix_route(MIX_ON, WIC_EXT_MIC,MIX_NOP,WOS_NONE);
		mx100_wm8993_input_output_mix_route(MIX_OFF, WIC_MIC,MIX_NOP,WOS_NONE);
		
		#ifdef  USING_SIDETONE
		mx100_wm8993_power(POWER_ON,WPS_MAIN |WPS_MIXIN |  WPS_EXT_MIC |WPS_CAPTURE|WPS_HP);
		#else
		mx100_wm8993_power(POWER_ON,WPS_MAIN |WPS_MIXIN | WPS_EXT_MIC | WPS_CAPTURE);
		#endif
	//	msleep(100);

        }
	

	if(mic_path==MIC_MAIN) {
		mx100_wm8993_mute(MUTE_OFF,WMS_MIC);
	} else if(mic_path==MIC_EXT) {
		mx100_wm8993_mute(MUTE_OFF,WMS_EXT_MIC);
	}
}

void wm8993_setpath_mic_off(void)
{

	struct snd_soc_codec *codec;
	if (wm8993_codec) {
		codec = wm8993_codec;
	} else {
		return;
	}
	MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"\nwm8993_setpath_mic_off\n");

	mx100_wm8993_mute(MUTE_ON,WMS_MIC | WMS_EXT_MIC);

	mx100_wm8993_input_output_mix_route(MIX_OFF, WIC_MIC | WIC_EXT_MIC,MIX_NOP,WOS_NONE);  /* 2011.05.13 JHLIM */

	wm8993_write(codec, WM8993_DIGITAL_SIDE_TONE, 0b0000111011100000);

	mx100_wm8993_power(POWER_OFF,WPS_MIC | WPS_EXT_MIC | WPS_CAPTURE);

//	mx100_wm8993_mute(MUTE_OFF,WMS_MIC | WMS_EXT_MIC);

}

// for detect ext mic.
void wm8993_enable_extmic_bias(int onoff)
{
	u16 val;

	struct snd_soc_codec *codec;
	if (wm8993_codec) {
		codec = wm8993_codec;
	} else {
		return;
	}


// Power Management 1
	val = wm8993_read(codec, WM8993_POWER_MANAGEMENT_1);

	if(onoff ==1) {
		val |=   WM8993_MICB2_ENA;
	} else {
		val &= ~WM8993_MICB2_ENA;
	}

	wm8993_write(codec, WM8993_POWER_MANAGEMENT_1, val);
}


extern int is_headset_connected(void);


/*Shanghai ewada  add for fm path */
int wm8993_internal_clk_opt(int onoff)
{
	u16 val;

	struct snd_soc_codec *codec;
	if (wm8993_codec) {
		codec = wm8993_codec;
	} else {
		return 0;
	}
	
	if(onoff ==1) {
		MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"wm8993_internal_clk_opt on \n");
	} else {
		MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"wm8993_internal_clk_opt off \n");
	}

	// for use inter nal clk
	val= wm8993_read(codec, WM8993_FLL_CONTROL_1);

	if (onoff){
		val |=3;
	}
	else {
		val &=~3;
	}
	
	wm8993_write(codec, WM8993_FLL_CONTROL_1, val);

	val= wm8993_read(codec, WM8993_CLOCKING_2);
	if (onoff) {
		val |= WM8993_SYSCLK_SRC;
	}
	else {
	//	val &=~WM8993_SYSCLK_SRC;
	}
	
	wm8993_write(codec, WM8993_CLOCKING_2, val);

	return 0;

}

#if 0
void wm8993_setpath_fm_hp_play(void)
{
//	u16 val;
	
	struct snd_soc_codec *codec;
	if (wm8993_codec) {
		codec = wm8993_codec;
	} else {
		return;
	}


	MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"wm8993_setpath_fm_hp_play\n");

	mx100_wm8993_mute(MUTE_ON,WMS_FM | WMS_HPOUT |WMS_SPKOUT | WMS_ANALOGUE_HP |WMS_MIXOUT);

	mx100_wm8993_power(POWER_ON,WPS_MAIN | WPS_FM_OUT |WPS_MIXIN | WPS_MIXOUT| WPS_HP);
	mx100_wm8993_power(POWER_OFF,WPS_SPK);
	#ifdef CONFIG_I2S_FM_FEED_MCLK
	msleep(300);
	#else
	msleep(300);

	wm8993_internal_clk_opt(1);

	msleep(200);
	#endif
	
	wm8993_write(codec, WM8993_CLASS_W_0, 0x0);

	mx100_wm8993_input_output_mix_route(MIX_ON, WIC_FM,MIX_ON,WOS_MIXIN);

	mx100_wm8993_setpath_headset_route(HEADSET_FROM_MIXOUT);


	if(g_fm_volume !=-1) 
	{
		wm8993_set_fm_volume(g_fm_volume);
	}	
	msleep(100);

	mx100_wm8993_mute(MUTE_OFF,WMS_FM | WMS_HPOUT | WMS_ANALOGUE_HP |WMS_MIXOUT);
}
#endif

static int g_fmpath_onoff = 0;

void wm8993_setpath_fm_on(int onoff)
{
//	u16 val;
	
	struct snd_soc_codec *codec;
	if (wm8993_codec) {
		codec = wm8993_codec;
	} else {
		return;
	}


	if(onoff ==1) {
		if(get_fm_output() == 0) {  // speaker.
			MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"wm8993_setpath_fm_on SPK\n");

			
			mx100_wm8993_mute(MUTE_ON,WMS_FM | WMS_MIXOUT | WMS_SPKOUT);
			msleep(50);
			mx100_wm8993_power(POWER_ON,WPS_MAIN|WPS_FM_OUT |WPS_MIXIN|WPS_MIXOUT | WPS_SPK);
			msleep(50);
			#ifdef CONFIG_I2S_FM_FEED_MCLK	
			enable_i2s_fm_mclk();
			#endif
			msleep(50);
			fm_start_for_codec();
			msleep(50);

			wm8993_route_set_playback_path(SPK);

			wm8993_write(codec, WM8993_CLASS_W_0, 0x0);


		//	wm8993_setpath_speaker_route(SPEAKER_FROM_MIXOUT);

			mx100_wm8993_input_output_mix_route(MIX_OFF, WIC_MIC |WIC_EXT_MIC  ,MIX_NOP,WOS_NONE);
			mx100_wm8993_input_output_mix_route(MIX_ON, WIC_FM,MIX_ON,WOS_MIXIN);
			wm8993_set_fm_volume(g_fm_volume);

			mx100_wm8993_mute(MUTE_OFF,WMS_MIXOUT | WMS_SPKOUT);
			if(g_fm_volume > 6) {
				mx100_wm8993_mute(MUTE_OFF,WMS_FM);
			}

			

		} else {   // headset
			MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"wm8993_setpath_fm_on HEADSET: %d\n");

			wm8993_route_set_playback_path(HP3P);

			mx100_wm8993_mute(MUTE_ON,WMS_FM|WMS_HPOUT | WMS_ANALOGUE_HP |WMS_MIXOUT);			
			msleep(50);
			mx100_wm8993_power(POWER_ON,WPS_MAIN|WPS_FM_OUT |WPS_MIXIN|WPS_MIXOUT | WPS_HP);
			msleep(50);

			#ifdef CONFIG_I2S_FM_FEED_MCLK	
			enable_i2s_fm_mclk();
			#endif
			msleep(50);

			wm8993_write(codec, WM8993_CLASS_W_0, 0x0);
	//		mx100_wm8993_setpath_headset_route(HEADSET_FROM_MIXOUT);

			mx100_wm8993_input_output_mix_route(MIX_OFF, WIC_MIC |WIC_EXT_MIC  ,MIX_NOP,WOS_NONE);
			mx100_wm8993_input_output_mix_route(MIX_ON, WIC_FM,MIX_ON,WOS_MIXIN);
			wm8993_set_fm_volume(g_fm_volume);
			msleep(100);
			fm_start_for_codec();
			msleep(50);

			mx100_wm8993_mute(MUTE_OFF,WMS_HPOUT | WMS_ANALOGUE_HP |WMS_MIXOUT);
			if(g_fm_volume > 6) {
				mx100_wm8993_mute(MUTE_OFF,WMS_FM);
			}			
		}

		g_fmpath_onoff = 1;
	
	} else {		
	
		if(wm8993_route_get_playback_path()==HP3P || wm8993_route_get_playback_path()==HP4P	) {
			mx100_hp_hw_mute(1);
			mx100_wm8993_mute(MUTE_ON,WMS_MIXOUT |  WMS_HPOUT | WMS_ANALOGUE_HP);
		}
		msleep(100);

		mx100_wm8993_mute(MUTE_ON,WMS_FM);
		mx100_wm8993_input_output_mix_route(MIX_OFF, WIC_FM,MIX_OFF,WOS_MIXIN);
		mx100_wm8993_power(POWER_OFF,WPS_MIXIN | WPS_FM_OUT);

		msleep(100);

		#ifdef CONFIG_I2S_FM_FEED_MCLK	
		disable_i2s_fm_mclk();
		#endif	
		msleep(200);

	        fm_stop();

		msleep(100);

		if(wm8993_route_get_playback_path()==HP3P || wm8993_route_get_playback_path()==HP4P	) {
			mx100_wm8993_mute(MUTE_OFF,WMS_MIXOUT |  WMS_HPOUT | WMS_ANALOGUE_HP);
			mx100_hp_hw_mute(0);
		} 	
		
		g_fmpath_onoff = 0;	
	}
}

int wm8993_getpath_fm_on(void)
{
	return g_fmpath_onoff;
}
// added 2011.03.28 JHLIM
#ifdef CONFIG_I2S_FM_FEED_MCLK
extern struct snd_pcm_substream *g_fm_substream;

void enable_i2s_fm_mclk(void)
{
	extern int s5p_i2s_fm_wr_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai);
	struct snd_soc_pcm_runtime *rtd = g_fm_substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;

	s5p_i2s_fm_wr_startup(g_fm_substream,cpu_dai);

}

void disable_i2s_fm_mclk(void)
{
	extern void s5p_i2s_fm_wr_shutdown(struct snd_pcm_substream *substream, struct snd_soc_dai *dai);

	struct snd_soc_pcm_runtime *rtd = g_fm_substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;

	s5p_i2s_fm_wr_shutdown(g_fm_substream,cpu_dai);
}
#endif

#if 0
void wm8993_setpath_fm_speaker_play(void)
{
//	u16 val;
	struct snd_soc_codec *codec;
	if (wm8993_codec) {
		codec = wm8993_codec;
	} else {
		return;
	}


	MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"\nwm8993_setpath_fm_speaker_play\n");

//	mx100_wm8993_mute(MUTE_ON,WMS_FM |WMS_MIC | WMS_EXT_MIC | WMS_HPOUT| WMS_SPKOUT);
	mx100_wm8993_mute(MUTE_ON,WMS_SPKOUT);

	#ifdef CONFIG_I2S_FM_FEED_MCLK
	msleep(100);
	#else
	msleep(100);

	wm8993_internal_clk_opt(1);

	msleep(200);
	#endif	
	wm8993_write(codec, WM8993_CLASS_W_0, 0x0);

	wm8993_setpath_speaker_route(SPEAKER_FROM_MIXOUT);

	mx100_wm8993_input_output_mix_route(MIX_ON, WIC_FM,MIX_ON,WOS_MIXIN);

	mx100_wm8993_power(POWER_ON,WPS_MAIN | WPS_FM_OUT |WPS_MIXIN | WPS_MIXOUT | WPS_SPK);
	mx100_wm8993_power(POWER_OFF,WPS_HP);

	if(g_fm_volume !=-1) {
		wm8993_set_fm_volume(g_fm_volume);
	}
	msleep(100);

	mx100_wm8993_mute(MUTE_OFF,WMS_SPKOUT| WMS_MIXOUT | WMS_FM );
}
#endif

/*Shanghai ewada  add for fm path */
void wm8993_setpath_fm_record(int onoff)
{
	u16 val;

	struct snd_soc_codec *codec;
	if (wm8993_codec) {
		codec = wm8993_codec;
	} else {
		return;
	}


	if(onoff ==1) {
		MX100_DBG(DBG_FILTER_WM8993_MIXER,"wm8993 fm record on \n");
	} else {
		MX100_DBG(DBG_FILTER_WM8993_MIXER,"wm8993 fm record off \n");
	}
   
#if 0
//13 Digital side Tone
	val = wm8993_read(codec, WM8993_DIGITAL_SIDE_TONE);

	if(onoff ==1) {
		val |=  0b0000111011100101;
	} else {
		val &= ~0b0000000000000110;
	}
	wm8993_write(codec, WM8993_DIGITAL_SIDE_TONE, val);
#endif

// * 15   15 Left ADC Digital Volume
	val = wm8993_read(codec, WM8993_LEFT_ADC_DIGITAL_VOLUME);
	val = 0b0000000011111111;
	wm8993_write(codec, WM8993_LEFT_ADC_DIGITAL_VOLUME, val);

// * 16   16 Right ADC Digital Volume
	val = wm8993_read(codec, WM8993_RIGHT_ADC_DIGITAL_VOLUME);
	val = 0b0000000011111111;
	wm8993_write(codec, WM8993_RIGHT_ADC_DIGITAL_VOLUME, val);


#if 0
	
// 24 WM8993_LEFT_LINE_INPUT_1_2_VOLUME
	val = wm8993_read(codec, WM8993_LEFT_LINE_INPUT_3_4_VOLUME);

	if(onoff ==1) {
		val =  0b0000000000011111;
	} else {
		val =  0b0000000000000000;
	}
	wm8993_write(codec, WM8993_LEFT_LINE_INPUT_3_4_VOLUME, val);

// 26 WM8993_RIGHT_LINE_INPUT_1_2_VOLUME
	val = wm8993_read(codec, WM8993_RIGHT_LINE_INPUT_3_4_VOLUME);

	if(onoff ==1) {
		val =  0b0000000000011111;
	} else {
		val =  0b0000000000000000;
	}
	wm8993_write(codec, WM8993_RIGHT_LINE_INPUT_3_4_VOLUME, val);
#endif
 
#ifndef USE_FM_FRONT

//41 WM8993_INPUT_MIXER3
	val = wm8993_read(codec, WM8993_INPUT_MIXER3);

	if(onoff ) {
		val |= 0b0000000000000111;  //val |= 0b0000000000000101;
	} else {
		val &=   ~0b00000000000000111;
	}
	wm8993_write(codec, WM8993_INPUT_MIXER3, val);


//42 WM8993_INPUT_MIXER4
	val = wm8993_read(codec, WM8993_INPUT_MIXER4);

	if(onoff) {
		val |= 0b0000000000000111;  //val |= 0b0000000000000101;
	} else {
		val &=   ~0b00000000000000111;
	}
	wm8993_write(codec, WM8993_INPUT_MIXER4, val);

#if 1
//43 WM8993_INPUT_MIXER5
	val = wm8993_read(codec, WM8993_INPUT_MIXER5);

	if(onoff ==1) {
		val |= 0b0000000000000111;
	} else {
		val &=    ~0b0000000000000111;
	}
	wm8993_write(codec, WM8993_INPUT_MIXER5, val);
#endif

//44 WM8993_INPUT_MIXER6
	val = wm8993_read(codec, WM8993_INPUT_MIXER6);

	if(onoff ==1) {
		val |= 0b0000000000000111;
	} else {
		val &=  ~0b0000000000000111;
	}

	wm8993_write(codec, WM8993_INPUT_MIXER6, val);

#if 1
// Power Management 1
	val = wm8993_read(codec, WM8993_POWER_MANAGEMENT_1);

	if(onoff ==1) {
		val |= 0b0000000000000101;
	} else {
		val &= ~0b0000000000000100;
	}

	wm8993_write(codec, WM8993_POWER_MANAGEMENT_1, val);
#endif
#endif

// Power Management 2
	val = wm8993_read(codec, WM8993_POWER_MANAGEMENT_2);

	if(onoff ==1) {
		val |= 0b0100000000000011;
	} else {
	//	val &= ~0b0100000000000011;
	}

	wm8993_write(codec, WM8993_POWER_MANAGEMENT_2, val);

#if 0

// Power Management 3
	val = wm8993_read(codec, WM8993_POWER_MANAGEMENT_3);

	if(onoff ==1) {
		val |=     0b0000000011110000;
		//val &= ~ 0b0000000000000011;
	} else {
		val &= ~0b11000000;
	}
	wm8993_write(codec, WM8993_POWER_MANAGEMENT_3, val);
#endif


#if 0
	// for use inter nal clk when off the rec, the mclk is offed
	if (!onoff) //!onoff   shuld open internal clk if off or fm can not be played
	{
		val= wm8993_read(codec, WM8993_CLOCKING_2);

		
		val |=3;
		
		wm8993_write(codec, WM8993_FLL_CONTROL_1, val);

		val= wm8993_read(codec, WM8993_CLOCKING_2);
		
		val |= WM8993_SYSCLK_SRC;
		
		wm8993_write(codec, WM8993_CLOCKING_2, val);
	}
#endif
}

#define USING_FM_VOL_TABLE

#define START_FM_VOL (0)
#define END_FM_VOL (15)
#define MAX_VOLUME_VALUE (31)

void 	set_fm_headset_volume(u16 index)   // added 2011.03.10 by junki hw.
{

	u16 val ,input_vol,output_vol;
	u16 reg0,reg1;
	extern int g_fm_on;
	
	struct snd_soc_codec *codec;
	if (wm8993_codec) {
		codec = wm8993_codec;
	} else {
		return ;
	}

	if(g_fm_on==0) {
		return;   /*JHLIM remove warning */
	}

	if(index >= 0 && index <6) 		{   input_vol =0x0;    	output_vol = 0x0;		}
	else if(index >= 6 && index < 13)   {	 input_vol =0x02; 	output_vol = 0x06;	}
	else if(index >= 13 && index < 20) 	{	input_vol =0x01;	output_vol = 0x05; }
	else if(index >= 20 && index < 26) 	{	 input_vol =0x01;	output_vol = 0x04;}
	else if(index >= 26 && index < 33) 	{	 input_vol =0x01;	output_vol = 0x03;}
	else if(index >= 33 && index < 40) 	{	 input_vol =0x01;	output_vol = 0x02;}
	else if(index >= 40 && index < 46) 	{	input_vol =0x01;	output_vol = 0x01; }
	else if(index >= 46 && index < 53) 	{	input_vol =0x01; 	output_vol = 0x0;}
	else if(index >= 53 && index < 60) 	{	input_vol =0x02; 	output_vol = 0x0;}
	else if(index >= 60 && index < 66) 	{	 input_vol =0x06; 	output_vol = 0x0;}
	else if(index >= 66 && index < 73) 	{	input_vol =0x08; 	output_vol = 0x0;}
	else if(index >= 73 && index < 80) 	{	input_vol =0x0A; 	output_vol = 0x0;}
	else if(index >= 80 && index < 86) 	{	 input_vol =0x0C; 	output_vol = 0x0;}
	else if(index >= 86 && index < 93) 	{	input_vol =0xE; 	output_vol = 0x0;}
	else if(index >=93 && index <100) {	input_vol =0x10; 	output_vol = 0x0;}
	else if(index ==100) {	input_vol =0x12; 	output_vol = 0x0;}
	else {   input_vol =0x0;  	output_vol = 0x0; }


	if (input_vol)
	{
		val = input_vol | WM8993_IN2_VU;
	}
	else
	{//mute
		val = input_vol | 0x80;
	}

	val  |= WM8993_IN2_VU;
	

	reg0 =	wm8993_read(codec, WM8993_OUTPUT_MIXER5);
	reg0 &= ~WM8993_MIXINR_MIXOUTR_VOL_MASK;
	reg0 |= (output_vol << WM8993_MIXINR_MIXOUTR_VOL_SHIFT);
	wm8993_write(codec, WM8993_OUTPUT_MIXER5,reg0);

	reg1 =	wm8993_read(codec, WM8993_OUTPUT_MIXER6);
	reg1 &= ~WM8993_MIXINL_MIXOUTL_VOL_MASK;
	reg1 |= (output_vol << WM8993_MIXINL_MIXOUTL_VOL_SHIFT);
	wm8993_write(codec, WM8993_OUTPUT_MIXER6, reg1);


	wm8993_write(codec, WM8993_LEFT_LINE_INPUT_3_4_VOLUME, val);
	wm8993_write(codec, WM8993_RIGHT_LINE_INPUT_3_4_VOLUME, val);

	MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"set_fm_headset_volume %d %x\n",index,val);
	
}

void 	set_fm_speaker_volume(u16 index)   // added 2011.03.10 by junki hw.
{

	u16 val ,input_vol,output_vol;
	u16 reg0,reg1;
	extern int g_fm_on;

	struct snd_soc_codec *codec;
	if (wm8993_codec) {
		codec = wm8993_codec;
	} else {
		return ;
	}
	if(g_fm_on==0) return;

	/*JHLIM remove warning */
	MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"set_fm_speaker_volume %d\n",index);

	if(index >= 0 && index <6) 		{  	input_vol =0x0;    	output_vol = 0x0;		}
	else if(index >= 6 && index < 13)   {	input_vol =0x01; 	output_vol = 0x02;	}
	else if(index >= 13 && index < 20) 	{	input_vol =0x01;	output_vol = 0x01;		}
	else if(index >= 20 && index < 26) 	{	input_vol =0x01;	output_vol = 0x00;	}
	else if(index >= 26 && index < 33) 	{	input_vol =0x02;	output_vol = 0x00;	}
	else if(index >= 33 && index < 40) 	{	input_vol =0x03;	output_vol = 0x00;	}
	else if(index >= 40 && index < 46) 	{	input_vol =0x04;	output_vol = 0x00;		}
	else if(index >= 46 && index < 53) 	{	input_vol =0x05; 	output_vol = 0x00;	}
	else if(index >= 53 && index < 60) 	{	input_vol =0x06; 	output_vol = 0x0;	}
	else if(index >= 60 && index < 66) 	{	input_vol =0x07; 	output_vol = 0x0;	}
	else if(index >= 66 && index < 73) 	{	input_vol =0x08; 	output_vol = 0x0;	}
	else if(index >= 73 && index < 80) 	{	input_vol =0x0A; 	output_vol = 0x0;	}
	else if(index >= 80 && index < 86) 	{	input_vol =0x0C; 	output_vol = 0x0;	}
	else if(index >= 86 && index < 93) 	{	input_vol =0xE;		output_vol = 0x0;	}
	else if(index >=93 && index <100) {	input_vol =0x10; 	output_vol = 0x0;	}
	else if(index ==100) 			{	input_vol =0x12; 	output_vol = 0x0;}
	else {   							 input_vol =0x0;  	output_vol = 0x0;	}


	if (input_vol)
	{
		val = input_vol | WM8993_IN2_VU;
	}
	else
	{//mute
		val = input_vol | 0x80;
	}

	val  |= WM8993_IN2_VU;

	reg0 =	wm8993_read(codec, WM8993_SPEAKER_MIXER);
	reg1 =	wm8993_read(codec, WM8993_OUTPUT_MIXER6);

	reg0 &= ~WM8993_MIXINR_MIXOUTR_VOL_MASK;
	reg0 |= (output_vol << WM8993_MIXINR_MIXOUTR_VOL_SHIFT);

	reg1 &= ~WM8993_MIXINL_MIXOUTL_VOL_MASK;
	reg1 |= (output_vol << WM8993_MIXINL_MIXOUTL_VOL_SHIFT);
	
	wm8993_write(codec, WM8993_LEFT_LINE_INPUT_3_4_VOLUME, val);
	wm8993_write(codec, WM8993_RIGHT_LINE_INPUT_3_4_VOLUME, val);


	wm8993_write(codec, WM8993_OUTPUT_MIXER5,reg0);
	wm8993_write(codec, WM8993_OUTPUT_MIXER6, reg1);

}

void wm8993_set_fm_volume(int vol)
{
	
	if(wm8993_route_get_playback_path()== SPK) {
		set_fm_speaker_volume(vol);
	} else {
		set_fm_headset_volume(vol);
	}
	
	
	return ;
}



#ifdef CONFIG_MX100_IOCTL

int WM8993_WRITE(unsigned int reg, unsigned int val)
{
	if(!wm8993_codec) return -1;
	
	return wm8993_write(wm8993_codec,reg,val);
}


int WM8993_READ(unsigned char reg)
{
	if(!wm8993_codec) return -1;

	return wm8993_read(wm8993_codec,reg);
}

void WM8993_SET_PATH_FM(void)
{
	/*Shanghai ewada*/
	//wm8993_set_bias_level(wm8993_codec,SND_SOC_BIAS_ON);
	
	//wm8993_set_audio_path(WAP_HEADSET,1);
	//wm8993_set_audio_path(WAP_FM,1);
}

void WM8993_debug(int onoff)
{
	mx100_debug_filter_set(DBG_FILTER_WM8993,onoff);
}

void WM8993_debug_error(int onoff)
{
	mx100_debug_filter_set(DBG_FILTER_WM8993_REG,onoff);
}

void WM8993_debug_slv(int onoff)
{
	mx100_debug_filter_set(DBG_FILTER_WM8993_SLV,onoff);
}
/* 2011.01.18  for test sw volume */
int g_mx100softvol = 100;

void mx100_set_soft_vol(unsigned int vol)
{
	g_mx100softvol = vol;
}

int mx100_get_soft_vol(void)
{
	return g_mx100softvol;
}

#endif

int get_autio_codec_path(enum e_audio_path_type path_type)
{
int path_cmd = 0;

audio_control_lock();

		switch(path_type) {
			
			case PATH_PLAYBACK:
				path_cmd=wm8993_route_get_playback_path();

				break;
			case PATH_MIC:
				path_cmd=wm8993_route_get_mic_path();
				
				break;
			case PATH_FMRADIO:
				path_cmd = wm8993_getpath_fm_on();
				break;
			case PATH_FMVOLUME:
				break;
			case PATH_H2W:
				path_cmd = wm8993_route_get_h2w_path();
				break;
		}
audio_control_unlock();
   return path_cmd;

}


static int  g_latest_path_type;
static int g_latest_path_cmd;
void set_autio_codec_path(enum e_audio_path_type path_type, int path_cmd)
{
audio_control_lock();

	switch(path_type) {
			
		case PATH_PLAYBACK:
			wm8993_route_set_playback_path(path_cmd);

			break;
		case PATH_MIC:
			wm8993_route_set_mic_path(path_cmd);
			
			break;
		case PATH_FMRADIO:
			if(path_cmd==FMR_ON) {
				wm8993_setpath_fm_on(1);
			} else if(path_cmd==FMR_OFF) {
				wm8993_setpath_fm_on(0);
			}			
			break;
		case PATH_FMVOLUME:
			g_fm_volume = path_cmd;
			wm8993_set_fm_volume(path_cmd);
			break;
		case PATH_H2W:
			wm8993_route_set_h2w_path(path_cmd);
			break;
	}

	if(path_type!=PATH_FMVOLUME) {
		g_latest_path_type = path_type;
		g_latest_path_cmd = path_cmd;
	}
audio_control_unlock();
	
}
void set_autio_codec_resume(void)
{
	set_autio_codec_path(g_latest_path_type,g_latest_path_cmd);
}


#define DEBUG_POWER_STATE

int mx100_wm8993_power(eWM8993_POWER_STATUS power_state,int power_type)
{
	u16 val0=0,val1=0;
	u16 n_val0=0,n_val1=0;
	int all_power_state = 0;
	char mute_log[256];
	int count = 0;
		
	struct snd_soc_codec *codec;
	if (wm8993_codec) {
		codec = wm8993_codec;
	} else {
		return 0;
	}

	#ifdef ENABLE_DELAY_TUNING
	if(power_state == POWER_OFF) return 0;
	#endif	

	if(power_type & WPS_MAIN) {
		if(power_state==POWER_STATUS) {
			val0 = wm8993_read(codec, WM8993_POWER_MANAGEMENT_1);

			if(val0 & WM8993_STARTUP_BIAS_ENA)  {
				all_power_state |= WPS_MAIN;
			}

		} else if(power_state == POWER_ON) {
			val1 = wm8993_read(codec, WM8993_POWER_MANAGEMENT_1);

			if(val1 & WM8993_BIAS_ENA)  {
				count += sprintf(&mute_log[count],"MAIN POWER ON Already|");
			} else  {
				/* Bring up VMID with fast soft start */
				snd_soc_update_bits(codec, WM8993_ANTIPOP2,
						    WM8993_STARTUP_BIAS_ENA |
						    WM8993_VMID_BUF_ENA |
						    WM8993_VMID_RAMP_MASK |
						    WM8993_BIAS_SRC,
						    WM8993_STARTUP_BIAS_ENA |
						    WM8993_VMID_BUF_ENA |
						    WM8993_VMID_RAMP_MASK |
						    WM8993_BIAS_SRC);

				/* VMID=2*40k */
				snd_soc_update_bits(codec, WM8993_POWER_MANAGEMENT_1,
						    WM8993_VMID_SEL_MASK |
						    WM8993_BIAS_ENA,
						    WM8993_BIAS_ENA | 0x2);
				msleep(52);

				/* Switch to normal bias */
				snd_soc_update_bits(codec, WM8993_ANTIPOP2,
						    WM8993_BIAS_SRC |
						    WM8993_STARTUP_BIAS_ENA, 0);
			
				snd_soc_update_bits(codec, WM8993_POWER_MANAGEMENT_2,
						    WM8993_TSHUT_ENA, 0);
				msleep(100);
				count += sprintf(&mute_log[count],"MAIN POWER ON|");
				all_power_state |= WPS_MAIN;
			}

			/* VMID=2*240k */
			snd_soc_update_bits(codec, WM8993_POWER_MANAGEMENT_1,
					    WM8993_VMID_SEL_MASK, 0x4);
			

		} 

		else if(power_state == POWER_OFF) {
			val1 = wm8993_read(codec, WM8993_POWER_MANAGEMENT_1);
			if(val1 & WM8993_BIAS_ENA) {
				val1 &= ~WM8993_BIAS_ENA;
				wm8993_write(codec, WM8993_POWER_MANAGEMENT_1, val1);
				count += sprintf(&mute_log[count],"MAIN POWER OFF|");
			} else {
				all_power_state &= ~WPS_MAIN;
				count += sprintf(&mute_log[count],"MAIN POWER OFF Already.|");
			}
			
		}

		if(power_state != POWER_STATUS) {
		}
	}

	if(power_type & WPS_CAPTURE) {
		val1 = wm8993_read(codec, WM8993_POWER_MANAGEMENT_2);
		if(power_state==POWER_STATUS) {
			if(val1 & WM8993_ADCL_ENA)  {
				all_power_state |= WPS_CAPTURE;
			}
		} else if(power_state == POWER_ON) {
			if(val1 & WM8993_ADCL_ENA)  {
				count += sprintf(&mute_log[count],"CAPTURE POWER ON Already|");
			} else {
				n_val1 = val1 | (WM8993_ADCL_ENA |WM8993_ADCR_ENA );
				all_power_state |= WPS_CAPTURE;
				wm8993_write(codec, WM8993_POWER_MANAGEMENT_2, n_val1 );
				count += sprintf(&mute_log[count],"CAPTURE POWER ON|");
				//msleep(50);
			}
		} else if(power_state == POWER_OFF) {
			if(val1 & WM8993_ADCL_ENA)  {
				n_val1 = val1 & ~(WM8993_ADCL_ENA |WM8993_ADCR_ENA );
				all_power_state &= ~WPS_CAPTURE;			
				wm8993_write(codec, WM8993_POWER_MANAGEMENT_2, n_val1 );
				count += sprintf(&mute_log[count],"CAPTURE POWER OFF|");
			} else {
				count += sprintf(&mute_log[count],"CAPTURE POWER OFF Already|");
			}
		}


	}

	if(power_type & WPS_MIC) {
		val0 = wm8993_read(codec, WM8993_POWER_MANAGEMENT_1);
		val1 = wm8993_read(codec, WM8993_POWER_MANAGEMENT_2);
		if(power_state==POWER_STATUS) {
			if(val0 & WM8993_MICB1_ENA)  {
				all_power_state |= WPS_MIC;
			}
		} else if(power_state == POWER_ON) {
			n_val0 = val0 | WM8993_MICB1_ENA;
			n_val1 = val1 |(WM8993_ADCL_ENA |WM8993_IN1L_ENA);
		
			all_power_state |= WPS_MIC;
		} else if(power_state == POWER_OFF) {
			n_val0 = val0 & ~WM8993_MICB1_ENA;
			n_val1 = val1 & ~(WM8993_ADCL_ENA |WM8993_IN1L_ENA);
			all_power_state &= ~WPS_MIC;			
		}

		if(power_state != POWER_STATUS) {
			if( n_val1 != val0 )  {
				wm8993_write(codec, WM8993_POWER_MANAGEMENT_1, n_val0 );
			}
			if( n_val1!= val1 ) {
				wm8993_write(codec, WM8993_POWER_MANAGEMENT_2, n_val1 );
			}
			count += sprintf(&mute_log[count],"MIC POWER %s|",power_state==POWER_ON ? "ON" : "OFF");
		}

	}

	if(power_type & WPS_EXT_MIC) {
		val0 = wm8993_read(codec, WM8993_POWER_MANAGEMENT_1);
		val1 = wm8993_read(codec, WM8993_POWER_MANAGEMENT_2);
		if(power_state==POWER_STATUS) {
			if(val0 & WM8993_MICB2_ENA)  {
				all_power_state |= WPS_EXT_MIC;
			}
		} else if(power_state == POWER_ON) {
			n_val0 = val0 |WM8993_MICB2_ENA;
			n_val1 = val1 |(WM8993_ADCR_ENA | WM8993_IN1R_ENA);
		
			all_power_state |= WPS_MIC;
		} else if(power_state == POWER_OFF) {
//			n_val0 = val0 & ~WM8993_MICB2_ENA;
			n_val1 = val1 & ~(WM8993_ADCR_ENA | WM8993_IN1R_ENA);
			all_power_state &= ~WPS_EXT_MIC;			
		}

		if(power_state != POWER_STATUS) {
			if(n_val0 != val0) {
				wm8993_write(codec, WM8993_POWER_MANAGEMENT_1, n_val0 );
			}
			if(n_val1 != val1) {
				wm8993_write(codec, WM8993_POWER_MANAGEMENT_2, n_val1 );
			}
			count += sprintf(&mute_log[count],"EXT MIC POWER %s|",power_state==POWER_ON ? "ON" : "OFF");
		}

	}

	if(power_type & WPS_FM_OUT) {

		val0 = wm8993_read(codec, WM8993_POWER_MANAGEMENT_2);
		if(power_state==POWER_STATUS) {
			if(val0 & WM8993_MICB2_ENA)  {
				all_power_state |= WPS_FM_OUT;
			}
		} else if(power_state == POWER_ON) {
			if(val0 & WM8993_IN2L_ENA) {
				count += sprintf(&mute_log[count],"FM OUT POWER ON Already|");		
			} else {
				n_val0 = val0 |(WM8993_IN2L_ENA | WM8993_IN2R_ENA);
				all_power_state |= WPS_FM_OUT;
				wm8993_write(codec, WM8993_POWER_MANAGEMENT_2, n_val0 );
				count += sprintf(&mute_log[count],"FM OUT POWER ON|");		
				msleep(10);
			}
		} else if(power_state == POWER_OFF) {
			if(val0 & WM8993_IN2L_ENA) {
				n_val0 = val0 & ~(WM8993_IN2L_ENA | WM8993_IN2R_ENA);
				all_power_state &= ~WPS_FM_OUT;			
				wm8993_write(codec, WM8993_POWER_MANAGEMENT_2, n_val0 );
				count += sprintf(&mute_log[count],"FM OUT POWER OFF|");		
		
			} else {
				count += sprintf(&mute_log[count],"FM OUT POWER OFF Already|");		
			}

		}

	}
	if(power_type & WPS_MIXIN) {

		val0 = wm8993_read(codec, WM8993_POWER_MANAGEMENT_2);
		if(power_state==POWER_STATUS) {
			if(val0 & WM8993_MIXINL_ENA)  {
				all_power_state |= WPS_MIXIN;
			}
		} else if(power_state == POWER_ON) {
			n_val0 = val0 |(WM8993_MIXINL_ENA| WM8993_MIXINR_ENA);	
			all_power_state |= WPS_MIXIN;
		} else if(power_state == POWER_OFF) {
			n_val0 = val0 & ~( WM8993_MIXINL_ENA| WM8993_MIXINR_ENA);
			all_power_state &= ~WPS_MIXIN;			
		}

		if(power_state != POWER_STATUS) {
			if(n_val0 != val0) {
				wm8993_write(codec, WM8993_POWER_MANAGEMENT_2, n_val0 );
				msleep(10);
			}
			count += sprintf(&mute_log[count],"MIXINT POWER %s|",power_state==POWER_ON ? "ON" : "OFF");
		}

	}

	if(power_type & WPS_MIXOUT) {

		val1 = wm8993_read(codec, WM8993_POWER_MANAGEMENT_3);
		if(power_state==POWER_STATUS) {
			if(val0 & WM8993_MIXOUTL_ENA)  {
				all_power_state |= WPS_MIXOUT;
			}
		} else if(power_state == POWER_ON) {
			n_val1 = val1 |(WM8993_MIXOUTL_ENA | WM8993_MIXOUTR_ENA);
		
			all_power_state |= WPS_MIXOUT;
		} else if(power_state == POWER_OFF) {
			n_val1 = val1 & ~( WM8993_MIXOUTL_ENA | WM8993_MIXOUTR_ENA);
			all_power_state &= ~WPS_MIXOUT;			
		}

		if(power_state != POWER_STATUS) {
			if(n_val1 != val1) {
				wm8993_write(codec, WM8993_POWER_MANAGEMENT_3, n_val1 );
				msleep(10);
			}
			count += sprintf(&mute_log[count],"MIXOUT POWER %s|",power_state==POWER_ON ? "ON" : "OFF");
		}

	}


	if(power_type & WPS_PLAYBACK) {
		val1 = wm8993_read(codec, WM8993_POWER_MANAGEMENT_3);
		if(power_state==POWER_STATUS) {
			if(val1 & WM8993_DACL_ENA)  {
				all_power_state |= WPS_PLAYBACK;
			}
		} else if(power_state == POWER_ON) {
			if(val1 & WM8993_DACL_ENA) {
				count += sprintf(&mute_log[count],"PLAYBACK POWER ON Already|");
			} else {
				n_val1 = val1 |(WM8993_DACL_ENA | WM8993_DACR_ENA);	
				wm8993_write(codec, WM8993_POWER_MANAGEMENT_3, n_val1 );
				all_power_state |= WPS_PLAYBACK;
				count += sprintf(&mute_log[count],"PLAYBACK POWER ON|");
			}
		} else if(power_state == POWER_OFF) {
			if(val1 & WM8993_DACL_ENA) {
				n_val1 = val1 & ~(WM8993_DACL_ENA | WM8993_DACR_ENA);
				wm8993_write(codec, WM8993_POWER_MANAGEMENT_3, n_val1 );
				all_power_state &= ~WPS_PLAYBACK;			
				count += sprintf(&mute_log[count],"PLAYBACK POWER OFF|");
			
			} else {
				count += sprintf(&mute_log[count],"PLAYBACK POWER OFF Already|");
			}
		}

	}
	if(power_type & WPS_HP) {
		val0 = wm8993_read(codec, WM8993_POWER_MANAGEMENT_1);
		if(power_state==POWER_STATUS) {
			if(val0 & WM8993_HPOUT1L_ENA)  {
				all_power_state |= WPS_HP;
			}
		} else if(power_state == POWER_ON) {
			if(val0 & WM8993_HPOUT1L_ENA) {
				count += sprintf(&mute_log[count],"HP POWER ON Already|");
			}  else {
				n_val0 = val0 |(WM8993_HPOUT1L_ENA | WM8993_HPOUT1R_ENA);		
				all_power_state |= WPS_HP;
				wm8993_write(codec, WM8993_POWER_MANAGEMENT_1, n_val0 );
				count += sprintf(&mute_log[count],"HP POWER ON|");
//				msleep(100);
			}
		} else if(power_state == POWER_OFF) {
			if(val0 & WM8993_HPOUT1L_ENA) {
				n_val0 = val0 & ~(WM8993_HPOUT1L_ENA | WM8993_HPOUT1R_ENA);
				all_power_state &= ~WPS_HP;			
				wm8993_write(codec, WM8993_POWER_MANAGEMENT_1, n_val0 );
				count += sprintf(&mute_log[count],"HP POWER OFF|");
	//			msleep(50);
			}  else {
				count += sprintf(&mute_log[count],"HP POWER OFF Already|");
			}
		}

	}

	if(power_type & WPS_SPK) {
		
		val0 = wm8993_read(codec, WM8993_POWER_MANAGEMENT_1);
		val1 = wm8993_read(codec, WM8993_POWER_MANAGEMENT_3);
		if(power_state==POWER_STATUS) {
			if(val0 & WM8993_SPKOUTL_ENA)  {
				all_power_state |= WPS_SPK;
			}
		} else if(power_state == POWER_ON) {
//[yoon 20110428]HDMI Ãâ·Â½Ã, speak - OFF
#ifdef CONFIG_MX100_REV_TP
			if (mx100_hdmi_status)
				return 0;
#endif
			n_val0 = val0 |(WM8993_SPKOUTL_ENA | WM8993_SPKOUTR_ENA);		
			n_val1 = val1 |(WM8993_SPKLVOL_ENA | WM8993_SPKRVOL_ENA);		
			all_power_state |= WPS_SPK;
		} else if(power_state == POWER_OFF) {
			n_val0 = val0 & ~(WM8993_SPKOUTL_ENA | WM8993_SPKOUTR_ENA);		
			n_val1 = val1 & ~(WM8993_SPKLVOL_ENA | WM8993_SPKRVOL_ENA);		

			all_power_state &= ~WPS_SPK;			
		}

		if(power_state != POWER_STATUS) {
			if(n_val0 != val0) {
				wm8993_write(codec, WM8993_POWER_MANAGEMENT_1, n_val0 );
			//	msleep(50);
			}
			if(n_val1 != val1) {
				wm8993_write(codec, WM8993_POWER_MANAGEMENT_3, n_val1 );
			//	msleep(50);
			}
			count += sprintf(&mute_log[count],"SPK POWER %s|",power_state==POWER_ON ? "ON" : "OFF");
		}
	}

	MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"%s\n",mute_log);

	return all_power_state;
}

int mx100_wm8993_input_output_mix_route(
	eWM8993_MIX_STATUS input_status,
	int input_src,
	eWM8993_MIX_STATUS output_status,
	int output_src
)
{
	u16 val0,val1,val2;

//	int input_mix_state = 0;
	
	struct snd_soc_codec *codec;
	if (wm8993_codec) {
		codec = wm8993_codec;
	} else {
		return 0;
	}
	if(input_src !=MIX_NOP) {
		val0 = wm8993_read(codec, WM8993_INPUT_MIXER2);
		val1 = wm8993_read(codec, WM8993_INPUT_MIXER3);
		val2 = wm8993_read(codec, WM8993_INPUT_MIXER4);

		if(input_status == MIX_ON) {
			if(input_src & WIC_MIC) {			
				val0 |=(WM8993_IN1LP_TO_IN1L|WM8993_IN1LN_TO_IN1L);
				val1 |= WM8993_IN1L_TO_MIXINL;
			}

			if(input_src & WIC_EXT_MIC) {			
				val0 |=(WM8993_IN1RP_TO_IN1R|WM8993_IN1RN_TO_IN1R);
				val2 |= WM8993_IN1R_TO_MIXINR; // =>2011.05.29   | WM8993_IN1R_MIXINR_VOL;   //2011.03.15 adjust EXT MIC VOL.
			}

			if(input_src & WIC_FM) {			
				val0 |=( WM8993_IN2LN_TO_IN2L | WM8993_IN2RN_TO_IN2R);
				val1 |= (WM8993_IN2L_TO_MIXINL);
				val2 |= (WM8993_IN2R_TO_MIXINR);
			}	
		} else if(input_status == MIX_OFF) {
			if(input_src & WIC_MIC) {			
				val0 &= ~(WM8993_IN1LP_TO_IN1L|WM8993_IN1LN_TO_IN1L);
				val1 &= ~WM8993_IN1L_TO_MIXINL;
			}

			if(input_src & WIC_EXT_MIC) {			
				val0 &=~(WM8993_IN1RP_TO_IN1R|WM8993_IN1RN_TO_IN1R);
				val2 &= ~WM8993_IN1R_TO_MIXINR;			
			}

			if(input_src & WIC_FM) {			
				val0 &=~(WM8993_IN2LN_TO_IN2L | WM8993_IN2RN_TO_IN2R);
				val1 &= ~(WM8993_IN2L_TO_MIXINL);
				val2 &= ~(WM8993_IN2R_TO_MIXINR);
			}	
		}

		if(input_status != MIX_STATUS) {

			val0 &=~(WM8993_IN2LP_TO_IN2L | WM8993_IN2RP_TO_IN2R);		// mix off unused port.
			wm8993_write(codec, WM8993_INPUT_MIXER2, val0 );
			wm8993_write(codec, WM8993_INPUT_MIXER3, val1 );
			wm8993_write(codec, WM8993_INPUT_MIXER4, val2 );
			
			wm8993_write(codec, WM8993_INPUT_MIXER5, 0x0);
			wm8993_write(codec, WM8993_INPUT_MIXER6, 0x0);
		}
	}
	

	if(output_src !=MIX_NOP) {

		val0 = wm8993_read(codec, WM8993_OUTPUT_MIXER1);
		val1 = wm8993_read(codec, WM8993_OUTPUT_MIXER2);

		if(output_status == MIX_ON) {
			if(output_src & WOS_DAC) {			
				val0 |=(WM8993_DACL_TO_MIXOUTL);
				val1 |=(WM8993_DACR_TO_MIXOUTR);
			}

			if(output_src & WOS_MIXIN) {			
				val0 |=(WM8993_MIXINL_TO_MIXOUTL);
				val1 |= (WM8993_MIXINR_TO_MIXOUTR);
			}	
		} else if(output_status == MIX_OFF) {

			if(output_src & WOS_DAC) {			
				val0 &=~(WM8993_DACL_TO_MIXOUTL);
				val1 &=~(WM8993_DACR_TO_MIXOUTR);
			}

			if(output_src & WOS_MIXIN) {			
				val0 &= ~(WM8993_MIXINL_TO_MIXOUTL);
				val1 &= ~(WM8993_MIXINR_TO_MIXOUTR);
			}	
		}

		if(output_status != MIX_STATUS) {

			val0 &=~(WM8993_IN2RN_TO_MIXOUTL | 
					WM8993_IN2LN_TO_MIXOUTL | 
					WM8993_IN1R_TO_MIXOUTL | 
					WM8993_IN1L_TO_MIXOUTL |
					WM8993_IN2LP_TO_MIXOUTL); // mix off unused port.
			
			wm8993_write(codec, WM8993_OUTPUT_MIXER1, val0 );


			val1 &=~(WM8993_IN2LN_TO_MIXOUTR | 
					WM8993_IN2RN_TO_MIXOUTR | 
					WM8993_IN1L_TO_MIXOUTR | 
					WM8993_IN1R_TO_MIXOUTR |
					WM8993_IN2RP_TO_MIXOUTR);  // mix off unused port.
			
			wm8993_write(codec, WM8993_OUTPUT_MIXER2, val1 );

		//47 WM8993_OUTPUT_MIXER3
			wm8993_write(codec, WM8993_OUTPUT_MIXER3,0x0);

		//48 WM8993_OUTPUT_MIXER4
			wm8993_write(codec, WM8993_OUTPUT_MIXER4,0x0);

		//49 WM8993_OUTPUT_MIXER5
			wm8993_write(codec, WM8993_OUTPUT_MIXER5,0x0);

		//50 WM8993_OUTPUT_MIXER6
			wm8993_write(codec, WM8993_OUTPUT_MIXER6,0x0);
		}
	}

	return 0;
}
	
void mx100_hp_hw_mute(int mute)
{
	if(mute==1) {
		gpio_set_value(GPIO_EARJACK_MUTE_OUT,0);
	} else {
		gpio_set_value(GPIO_EARJACK_MUTE_OUT,1);
	}

	MX100_DBG(DBG_FILTER_WM8993_REG,"wm8993 mute %d \n",mute);
}

#define DEBUG_MUTE_STATUS
int g_mute_state = 0;

int mx100_wm8993_mute(eWM8993_MUTE_STATUS mute_state,int mute_type)
{
	u16 lval,rval;
	char mute_log[128];
	int count = 0;
	
	int all_mute_state = 0;
	
	struct snd_soc_codec *codec;


	#ifdef ENABLE_DELAY_TUNING
	if(mute_state == MUTE_ON) return 0;
	#endif
	
	if (wm8993_codec) {
		codec = wm8993_codec;
	} else {
		return 0;
	}

	if(mute_type & WMS_DAC) {
		lval = wm8993_read(codec, WM8993_DAC_CTRL);
		if(mute_state==MUTE_STATUS) {
			if(lval & WM8993_DAC_MUTE)  {
				all_mute_state |= WMS_DAC;
			}
		} else if(mute_state == MUTE_ON) {
			lval |=(WM8993_DAC_MUTE);
		} else if(mute_state == MUTE_OFF) {
			lval &= ~WM8993_DAC_MUTE;
			lval |=(WM8993_DAC_OSR128);

		}

		if(mute_state != MUTE_STATUS) {
			wm8993_write(codec, WM8993_DAC_CTRL, lval );
		}
	}

	if(mute_type & WMS_DACLR) {


	}

	if(mute_type & WMS_MIC) {
		// Input Mixer3
		lval = wm8993_read(codec, WM8993_LEFT_LINE_INPUT_1_2_VOLUME);
		if(mute_state==MUTE_STATUS) {
			if(lval & WM8993_IN1L_MUTE)  {
				all_mute_state |= WMS_MIC;
			}
		} else if(mute_state == MUTE_ON) {
			lval |=WM8993_IN1L_MUTE;
			all_mute_state |= WMS_MIC;
		} else if(mute_state == MUTE_OFF) {
			lval &= ~WM8993_IN1L_MUTE;
			all_mute_state &= ~WMS_MIC;			
		}

		if(mute_state != MUTE_STATUS) {
			wm8993_write(codec, WM8993_LEFT_LINE_INPUT_1_2_VOLUME, lval );
			count += sprintf(&mute_log[count],"MIC MUTE %s|",mute_state==MUTE_ON ? "ON" : "OFF");
		}
	}

	if(mute_type & WMS_EXT_MIC) {
		rval = wm8993_read(codec, WM8993_RIGHT_LINE_INPUT_1_2_VOLUME);
		if(mute_state==MUTE_STATUS) {
			if(rval & WM8993_IN1R_MUTE)  {
				all_mute_state |= WMS_EXT_MIC;
			}
		} else if(mute_state == MUTE_ON) {
			rval |=WM8993_IN1R_MUTE;
			all_mute_state |= WMS_EXT_MIC;
			
		} else if(mute_state == MUTE_OFF) {
			rval &= ~WM8993_IN1R_MUTE;
			all_mute_state &= ~WMS_EXT_MIC;
		}

		if(mute_state != MUTE_STATUS) {
			wm8993_write(codec, WM8993_RIGHT_LINE_INPUT_1_2_VOLUME, rval );
			count += sprintf(&mute_log[count],"EXT MIC MUTE %s|",mute_state==MUTE_ON ? "ON" : "OFF");
		}


	}
	if(mute_type & WMS_FM) {

		lval = wm8993_read(codec, WM8993_LEFT_LINE_INPUT_3_4_VOLUME);
		rval = wm8993_read(codec, WM8993_RIGHT_LINE_INPUT_3_4_VOLUME);

		if(mute_state==MUTE_STATUS) {
			if(lval & WM8993_IN2L_MUTE)  {
				all_mute_state |= WMS_FM;
			}
		} else if(mute_state == MUTE_ON) {
			lval |=WM8993_IN2L_MUTE;
			rval |=WM8993_IN2R_MUTE;		

			all_mute_state |= WMS_FM;
			
		} else if(mute_state == MUTE_OFF) {
			lval &= ~WM8993_IN2L_MUTE;
			rval &= ~WM8993_IN2R_MUTE;		
			all_mute_state &= ~WMS_FM;
		}

		if(mute_state != MUTE_STATUS) {
			wm8993_write(codec, WM8993_LEFT_LINE_INPUT_3_4_VOLUME, lval );
			wm8993_write(codec, WM8993_RIGHT_LINE_INPUT_3_4_VOLUME, rval);
			count += sprintf(&mute_log[count],"FM MUTE %s|",mute_state==MUTE_ON ? "ON" : "OFF");
		}
	}

	if(mute_type & WMS_HPOUT) {
		lval = wm8993_read(codec, WM8993_LEFT_OUTPUT_VOLUME);
		rval = wm8993_read(codec, WM8993_RIGHT_OUTPUT_VOLUME);
		if(mute_state==MUTE_STATUS) {
			if(!(lval & WM8993_HPOUT1L_MUTE_N))  {
				all_mute_state |= WMS_HPOUT;
			}
		} else if(mute_state == MUTE_ON) {
			lval &= ~WM8993_HPOUT1L_MUTE_N;
			rval &= ~WM8993_HPOUT1R_MUTE_N;		

			all_mute_state |= WMS_HPOUT;
//			PRINT_CALLER; mx100_printk("hpout mute on\n");
		} else if(mute_state == MUTE_OFF) {
			lval |=WM8993_HPOUT1L_MUTE_N;
			rval |=WM8993_HPOUT1R_MUTE_N;		
//			PRINT_CALLER; mx100_printk("hpout mute off\n");

			all_mute_state &= ~WMS_HPOUT;
		}

		if(mute_state != MUTE_STATUS) {
			wm8993_write(codec, WM8993_LEFT_OUTPUT_VOLUME, lval );
			wm8993_write(codec, WM8993_RIGHT_OUTPUT_VOLUME, rval);
			count += sprintf(&mute_log[count],"HP MUTE %s|",mute_state==MUTE_ON ? "ON" : "OFF");			
		}
	}

	if(mute_type & WMS_ANALOGUE_HP) {
		lval = wm8993_read(codec, WM8993_ANALOGUE_HP_0);
		if(mute_state==MUTE_STATUS) {
			if(!(lval & WM8993_HPOUT1L_OUTP))  {
				all_mute_state |= WMS_ANALOGUE_HP;
			}
		} else if(mute_state == MUTE_ON) {
			lval &= ~(WM8993_HPOUT1L_OUTP | WM8993_HPOUT1R_OUTP);		
			all_mute_state |= WMS_ANALOGUE_HP;
		} else if(mute_state == MUTE_OFF) {
			lval |=(WM8993_HPOUT1L_OUTP | WM8993_HPOUT1R_OUTP);		
			all_mute_state &= ~WMS_ANALOGUE_HP;
		}

		if(mute_state == MUTE_OFF) {
			lval |=(WM8993_HPOUT1L_OUTP | WM8993_HPOUT1R_OUTP);		
			wm8993_write(codec, WM8993_ANALOGUE_HP_0, lval );
			count += sprintf(&mute_log[count],"ANALOGUE HP MUTE %s|",mute_state==MUTE_ON ? "ON" : "OFF");			
		}

		if(mute_state == MUTE_ON) {
			mx100_hp_hw_mute(1);
			msleep(50);
		} else if(mute_state == MUTE_OFF) {
			mx100_hp_hw_mute(0);
		}
		
	}

	
	if(mute_type & WMS_MIXOUT) {

		lval = wm8993_read(codec, WM8993_LEFT_OPGA_VOLUME);
		rval = wm8993_read(codec, WM8993_RIGHT_OPGA_VOLUME);
		if(mute_state==MUTE_STATUS) {
			if(!(lval & WM8993_MIXOUTL_MUTE_N))  {
				all_mute_state |= WMS_MIXOUT;
			}
		} else if(mute_state == MUTE_ON) {
			lval &= ~WM8993_MIXOUTL_MUTE_N;
			rval &= ~WM8993_MIXOUTR_MUTE_N;		

			all_mute_state |= WMS_MIXOUT;
	//		PRINT_CALLER; mx100_printk("mixout mute on\n");
		} else if(mute_state == MUTE_OFF) {
			lval |=WM8993_MIXOUTL_MUTE_N;
			rval |=WM8993_MIXOUTR_MUTE_N;		

			all_mute_state &= ~WMS_MIXOUT;
		//	PRINT_CALLER; mx100_printk("mixout mute off\n");
	
		}

		if(mute_state != MUTE_STATUS) {
			wm8993_write(codec, WM8993_LEFT_OPGA_VOLUME, lval );
			wm8993_write(codec, WM8993_RIGHT_OPGA_VOLUME, rval);
			count += sprintf(&mute_log[count],"MIXOUT MUTE %s|",mute_state==MUTE_ON ? "ON" : "OFF");

		}


	}

	if(mute_type & WMS_SPKOUT) {
		lval = wm8993_read(codec, WM8993_SPEAKER_VOLUME_LEFT);
		rval = wm8993_read(codec, WM8993_SPEAKER_VOLUME_LEFT);
		if(mute_state==MUTE_STATUS) {
			if(!(lval & WM8993_SPKOUTL_MUTE_N))  {
				all_mute_state |= WMS_SPKOUT;
			}
		} else if(mute_state == MUTE_ON) {
			lval &= ~WM8993_SPKOUTL_MUTE_N;
			rval &= ~WM8993_SPKOUTL_MUTE_N;		

			all_mute_state |= WMS_SPKOUT;

		} else if(mute_state == MUTE_OFF) {
			lval |=WM8993_SPKOUTL_MUTE_N;
			rval |=WM8993_SPKOUTL_MUTE_N;		

			all_mute_state &= ~WMS_SPKOUT;
		}

		if(mute_state != MUTE_STATUS) {
			wm8993_write(codec, WM8993_SPEAKER_VOLUME_LEFT, lval );
			wm8993_write(codec, WM8993_SPEAKER_VOLUME_LEFT, rval);
			count += sprintf(&mute_log[count],"SPK MUTE %s|",mute_state==MUTE_ON ? "ON" : "OFF");

		}
	}
	g_mute_state	 = all_mute_state;

//	MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"%s\n",mute_log);



	return all_mute_state;
}

int mx100_wm8993_get_mute_status(void)
{
	return g_mute_state;
}

int audio_codec_ctl_open (struct inode *inode, struct file *filp)  
{  
//	printk(KERN_ERR "mx100 audio codec  ctl opened\n");

	return nonseekable_open(inode, filp);
}  
  
int audio_codec_ctl_release (struct inode *inode, struct file *filp)  
{  
    return 0;  
}  

int audio_codec_ioctl (struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)  
{  
  
    audiocodec_ctl   ctrl_info;  
    int               size;  
    int ret;
	
    if( _IOC_TYPE( cmd ) != MX100_AUDIOCODEC_CTL_MAGIC ) return -EINVAL;  
      
    size = _IOC_SIZE( cmd );   
      
//   mutex_lock(&g_codec_ctl_mutex);
   
    switch( cmd )  
    {  
	case MX100_AUDIOCODEC_CTL_GET:
		ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, sizeof(ctrl_info) );   

	//	MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"MX100_AUDIOCODEC_CTL_GET %d \n", ctrl_info.path_type);

		ctrl_info.path_cmd = get_autio_codec_path(ctrl_info.path_type);
		
                ret = copy_to_user ( (void *) arg, (const void *) &ctrl_info,  sizeof(ctrl_info));
				
		break;
	case MX100_AUDIOCODEC_CTL_SET:
		ret = copy_from_user ( (void *)&ctrl_info, (const void *) arg, sizeof(ctrl_info) );   

		MX100_DBG(DBG_FILTER_WM8993_ALSA_ROUTE,"MX100_AUDIOCODEC_CTL_SET %d %d \n", ctrl_info.path_type,ctrl_info.path_cmd);

		set_autio_codec_path(ctrl_info.path_type,ctrl_info.path_cmd);

		break;
		
	default: break;
    }  

//  mutex_unlock(&g_codec_ctl_mutex);

    return 0;  
}  


struct file_operations mx100_audiocodec_fops =  
{  
    .owner    = THIS_MODULE,  
    .ioctl    = audio_codec_ioctl,  
    .open     = audio_codec_ctl_open,       
    .release  = audio_codec_ctl_release,    
};  
  

static struct miscdevice mx100_audiocodecl_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "audiocodec",
    .fops = &mx100_audiocodec_fops,
};

static int __init audio_codec_ctl_modinit(void)
{
//int reg;
int ret;

    if( (ret = misc_register(&mx100_audiocodecl_misc_device)) < 0 )    {
		misc_deregister(&mx100_audiocodecl_misc_device);
    }

   return ret;
}


module_init(audio_codec_ctl_modinit);

static void __exit autio_codec_ctl_exit(void)
{

	printk("\nmx100 alsa control driver initialized\n");
	 misc_deregister(&mx100_audiocodecl_misc_device);
}

module_exit(autio_codec_ctl_exit);



MODULE_DESCRIPTION("WM8993 sound path driver");
MODULE_AUTHOR("iRiver <jaehwan.lim@iriver.com>");
MODULE_LICENSE("GPL");
