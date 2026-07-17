// v90s — Powkiddy V90S (Allwinner A133P, 640x480 clamshell)

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////

#ifdef SDL
#	include "sdl.h"
#endif

///////////////////////////////

// Confirmed by on-device recon (2026-07-17): the built-in pad is
// "adc_gamepad" (rocknix-singleadc-joypad driver, event4) reporting
// BTN_* codes 304/305/307/308 (ABXY), 310-315 (L1/R1/L2/R2/Select/Start)
// and 316 (BTN_MODE = the Menu button stock leaves unmapped), dpad on
// hat0. Volume keys are a separate "sunxi-keyboard" device (event0,
// 114/115); the power key is "axp2202-pek" (event1, 116). Physical
// A/B/X/Y label orientation still needs eyes on the device.

#define BUTTON_UP		BUTTON_NA
#define BUTTON_DOWN	BUTTON_NA
#define BUTTON_LEFT	BUTTON_NA
#define BUTTON_RIGHT	BUTTON_NA

#define BUTTON_SELECT	BUTTON_NA
#define BUTTON_START	BUTTON_NA

#define BUTTON_A		BUTTON_NA
#define BUTTON_B		BUTTON_NA
#define BUTTON_X		BUTTON_NA
#define BUTTON_Y		BUTTON_NA

#define BUTTON_L1		BUTTON_NA
#define BUTTON_R1		BUTTON_NA
#define BUTTON_L2		BUTTON_NA
#define BUTTON_R2		BUTTON_NA
#define BUTTON_L3		BUTTON_NA
#define BUTTON_R3		BUTTON_NA
#define BUTTON_L4		BUTTON_NA
#define BUTTON_R4		BUTTON_NA

#define BUTTON_MENU		BUTTON_NA
#define BUTTON_MENU_ALT	BUTTON_NA
#define	BUTTON_POWER	116
#define	BUTTON_PLUS		BUTTON_NA
#define	BUTTON_MINUS	BUTTON_NA

///////////////////////////////

#define CODE_UP			103
#define CODE_DOWN		108
#define CODE_LEFT		105
#define CODE_RIGHT		106

#define CODE_SELECT		314
#define CODE_START		315

#define CODE_A			304
#define CODE_B			305
#define CODE_X			307
#define CODE_Y			308

#define CODE_L1			310
#define CODE_R1			311
#define CODE_L2			312
#define CODE_R2			313
#define CODE_L3			CODE_NA
#define CODE_R3			CODE_NA
#define CODE_L4			CODE_NA
#define CODE_R4			CODE_NA

#define CODE_MENU		316	// BTN_MODE, confirmed in the adc_gamepad key bitmap
#define CODE_MENU_ALT	CODE_NA
#define CODE_POWER		116

#define CODE_PLUS		115	// sunxi-keyboard volume keys, confirmed
#define CODE_MINUS		114

///////////////////////////////
						// HATS (dpad is hat0 on sunxi-joypad)
#define JOY_UP			JOY_NA
#define JOY_DOWN		JOY_NA
#define JOY_LEFT		JOY_NA
#define JOY_RIGHT		JOY_NA

#define JOY_SELECT		8
#define JOY_START		9

#define JOY_A			0	// TODO(recon): physical label orientation
#define JOY_B			1
#define JOY_X			2
#define JOY_Y			3

#define JOY_L1			4
#define JOY_R1			5
#define JOY_L2			6
#define JOY_R2			7
#define JOY_L3			JOY_NA
#define JOY_R3			JOY_NA
#define JOY_L4			JOY_NA
#define JOY_R4			JOY_NA

// JOY_* ids only apply to external USB pads (the built-in adc_gamepad is
// handled via raw evdev); these follow the common SDL ordering.
#define JOY_MENU		12
#define JOY_POWER		JOY_NA
#define JOY_PLUS		JOY_NA
#define JOY_MINUS		JOY_NA

///////////////////////////////

// no analog sticks
#define AXIS_L2			AXIS_NA
#define AXIS_R2			AXIS_NA

#define AXIS_LX			AXIS_NA
#define AXIS_LY			AXIS_NA
#define AXIS_RX			AXIS_NA
#define AXIS_RY			AXIS_NA

///////////////////////////////

#define BTN_RESUME			BTN_X
#define BTN_SLEEP 			BTN_POWER
#define BTN_WAKE 			BTN_POWER
#define BTN_MOD_VOLUME 		BTN_NONE
#define BTN_MOD_BRIGHTNESS 	BTN_MENU
#define BTN_MOD_COLORTEMP 	BTN_SELECT
#define BTN_MOD_PLUS 		BTN_PLUS
#define BTN_MOD_MINUS 		BTN_MINUS

///////////////////////////////

#define FIXED_SCALE 	2
#define FIXED_WIDTH		640
#define FIXED_HEIGHT	480
#define FIXED_BPP		2
#define FIXED_DEPTH		(FIXED_BPP * 8)
#define FIXED_PITCH		(FIXED_WIDTH * FIXED_BPP)
#define FIXED_SIZE		(FIXED_PITCH * FIXED_HEIGHT)

///////////////////////////////

#define MAIN_ROW_COUNT 6
#define QUICK_SWITCHER_COUNT 3
#define PADDING 5

///////////////////////////////

#define SDCARD_PATH "/mnt/SDCARD"
#define MUTE_VOLUME_RAW 0

#define SCREEN_FPS 60.0	// TODO(recon): measure real panel refresh
#define MAX_LIGHTS 0

///////////////////////////////

#endif
