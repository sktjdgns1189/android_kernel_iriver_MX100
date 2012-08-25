/*
 * linux/sound/wm8993.h -- Platform data for WM8993
 *
 * Copyright 2009 Wolfson Microelectronics. PLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_SND_WM8993_H
#define __LINUX_SND_WM8993_H

/* Note that EQ1 only contains the enable/disable bit so will be
   ignored but is included for simplicity.
 */
struct wm8993_retune_mobile_setting {
	const char *name;
	unsigned int rate;
	u16 config[24];
};

struct wm8993_platform_data {
	struct wm8993_retune_mobile_setting *retune_configs;
	int num_retune_configs;

	/* LINEOUT can be differential or single ended */
	unsigned int lineout1_diff:1;
	unsigned int lineout2_diff:1;

	/* Common mode feedback */
	unsigned int lineout1fb:1;
	unsigned int lineout2fb:1;

	/* Microphone biases: 0=0.9*AVDD1 1=0.65*AVVD1 */
	unsigned int micbias1_lvl:1;
	unsigned int micbias2_lvl:1;

	/* Jack detect threashold levels, see datasheet for values */
	unsigned int jd_scthr:2;
	unsigned int jd_thr:2;
};

#ifdef CONFIG_I2S_FM_FEED_MCLK
#define ENABLE_PLAYBACK_FM  // added 2011.03.09 to support fm , playback  Simultaneously.

void enable_i2s_fm_mclk(void);
void disable_i2s_fm_mclk(void);

#endif


//#define ENABLE_DELAY_TUNING

#ifdef CONFIG_PM
#define ENABLE_WM8993_PM  // added 2011.03.19 : support wm8993 power suspend 
//#define ENABLE_WM8993_REG_SAVE  /* 2011.04.20  */
#endif

typedef struct 
{ 
	unsigned int path_type;
	unsigned int path_cmd;
	unsigned char buffer[64];  
} __attribute__ ((packed)) audiocodec_ctl; 

// wm8993 sound codec.
#define MX100_AUDIOCODEC_CTL_MAGIC    't'  

#define MX100_AUDIOCODEC_CTL_INFO              	_IO(  MX100_AUDIOCODEC_CTL_MAGIC, 0 )  
#define MX100_AUDIOCODEC_CTL_GET      			_IOWR( MX100_AUDIOCODEC_CTL_MAGIC, 1 , audiocodec_ctl )  
#define MX100_AUDIOCODEC_CTL_SET         		_IOW( MX100_AUDIOCODEC_CTL_MAGIC, 2 , audiocodec_ctl )  


//added jhlim
// moved here. 2011.01.31 
typedef enum _eWM8993_AUDIO_PATH
{
	WAP_NONE = 0,
	WAP_PLAY = 1,
	WAP_RECORD = 2,
	WAP_HEADSET = 4,
	WAP_SPEAKER = 8,
	WAP_MIC = 16,
	WAP_EXTMIC = 32,
	WAP_FM = 64
}eWM8993_AUDIO_PATH;

enum e_audio_path_type	   {PATH_PLAYBACK,PATH_MIC,PATH_FMRADIO,PATH_FMVOLUME,PATH_H2W};
enum e_playback_path      { PLAYBACK_OFF=0, RCV, SPK,SPK_OFF, HP3P, HP4P, HP_OFF, BT, HP_SPK, EXTRA_DOCK_SPEAKER, HDMI_SPK, HDMI_DUAL };
enum e_mic_path		   {  MIC_MAIN=0, MIC_EXT, BT_REC, MIC_OFF };
enum e_fmradio_path     { FMR_OFF=0,FMR_ON, FMR_SPK, FMR_HP,FMR_DUAL_MIX };
enum e_h2w_path		   {  H2W_SPEAKER=0,H2W_HEADSET };

enum e_power_state	       { CODEC_OFF,CODEC_RESUME, CODEC_ON };
enum e_state                 		{ OFF, ON };
enum e_voice_record_path     { CALL_RECORDING_OFF, CALL_RECORDING_MAIN, CALL_RECORDING_SUB};

enum e_audio_stream_type	   {PCM_STREAM_DEACTIVE,PCM_STREAM_PLAYBACK,PCM_STREAM_CAPTURE,PCM_STREAM_FM };


#define DEACTIVE				0x00
#define PLAYBACK_ACTIVE		0x01
#define CAPTURE_ACTIVE		0x02
#define CALL_ACTIVE			0x04
#define FMRADIO_ACTIVE		0x08

typedef enum _eWM8993_POWER_STATUS
{
	POWER_OFF = 0,
	POWER_ON = 1,
	POWER_STATUS= 2
}eWM8993_POWER_STATUS;

typedef enum _eWM8993_POWER_SET
{
	WPS_NONE = 0,
	WPS_MAIN=1,
	WPS_CAPTURE=2,
	WPS_MIC = 4,
	WPS_EXT_MIC = 8,
	WPS_MIXIN = 16,
	WPS_MIXOUT = 32,
	WPS_FM_OUT = 64,
	WPS_PLAYBACK = 128,
	WPS_HP = 256,
	WPS_SPK = 512,
	WPS_POWER_ALL = WPS_MAIN |	WPS_CAPTURE | WPS_MIC | WPS_EXT_MIC |WPS_MIXIN | WPS_MIXOUT | WPS_FM_OUT |WPS_PLAYBACK |WPS_HP |WPS_SPK
}eWM8993_POWER_SET;

int mx100_wm8993_power(eWM8993_POWER_STATUS power_state,int power_type);


typedef enum _eWM8993_MUTE_STATUS
{
	MUTE_OFF = 0,
	MUTE_ON = 1,
	MUTE_STATUS= 2
}eWM8993_MUTE_STATUS;

typedef enum _eWM8993_MUTE_SET
{
	WMS_NONE = 0,
	WMS_DAC = 1,
	WMS_DACLR = 2,
	WMS_MIC = 4,
	WMS_EXT_MIC = 8,
	WMS_FM = 16,
	WMS_HPOUT = 32,
	WMS_MIXOUT = 64,
	WMS_SPKOUT = 128,
	WMS_ANALOGUE_HP = 256,
	MWS_MUTE_ALL= 	WMS_DAC |WMS_DACLR |WMS_MIC  | WMS_EXT_MIC | WMS_FM | WMS_HPOUT |WMS_MIXOUT | WMS_SPKOUT |WMS_ANALOGUE_HP,
}eWM8993_MUTE_STATE;

typedef enum _eWM8993_HEADSET_SOURCE
{
	HEADSET_FROM_DAC = 0,
	HEADSET_FROM_MIXOUT = 1,
}eWM8993_HEADSET_SOURCE;

typedef enum _eWM8993_SPEAKER_SOURCE
{
	SPEAKER_FROM_DAC = 0,
	SPEAKER_FROM_MIXOUT = 1,
}eWM8993_SPEAKER_SOURCE;


typedef enum _eWM8993_MIX_STATUS
{
	MIX_NOP = 0,
	MIX_OFF,
	MIX_ON ,
	MIX_STATUS
}eWM8993_MIX_STATUS;


typedef enum _eWM8993_INPUT_SRC
{
	WIC_NONE = 0,
	WIC_MIC = 1,
	WIC_EXT_MIC = 2,
	WIC_FM = 4,
	WIC_ALL = 	WIC_MIC |WIC_EXT_MIC |WIC_FM 
}eWM8993_INPUT_SRC;


typedef enum _eWM8993_OUTPUT_SRC
{
	WOS_NONE = 0,
	WOS_DAC = 1,
	WOS_MIXIN = 2,
	WOS_ALL = 	WOS_DAC |WOS_MIXIN 
}eWM8993_OUTPUT_SRC;

int mx100_wm8993_input_output_mix_route(
	eWM8993_MIX_STATUS input_status,
	int input_src,
	eWM8993_MIX_STATUS output_status,
	int output_src
);

int get_autio_codec_path(enum e_audio_path_type path_type);
void set_autio_codec_path(enum e_audio_path_type path_type, int path_cmd);

void mx100_hp_hw_mute(int mute);

int wm8993_get_audio_path(void);

void wm8993_setpath_mic(enum e_mic_path mic_path);
void wm8993_setpath_mic_off(void);

void wm8993_enable_extmic_bias(int onoff);

void mx100_wm8993_setpath_headset_route(eWM8993_HEADSET_SOURCE from_source);
void wm8993_setpath_speaker_route(eWM8993_SPEAKER_SOURCE from_source);
void wm8993_setpath_headset_play(void);
void wm8993_setpath_speaker_play(void);


int wm8993_internal_clk_opt(int onoff);
void wm8993_setpath_fm_hp_play(void);
void wm8993_setpath_fm_speaker_play(void);
void wm8993_setpath_fm_record(int onoff);/*Shanghai ewada*/
void wm8993_set_fm_volume(int vol);

int wm8993_route_get_fm_path(void);


int mx100_wm8993_mute(eWM8993_MUTE_STATUS mute_state,int mute_type);
int mx100_wm8993_get_mute_status(void);

void set_autio_codec_resume(void);

void wm8993_setpath_fm_on(int onoff);
int wm8993_getpath_fm_on(void);

void audio_control_lock(void);
void audio_control_unlock(void);

 int wm8993_route_set_h2w_path(int h2w_path);
 int wm8993_route_get_playback_path(void);
 int wm8993_route_set_playback_path(int playback_path);

	
#endif
