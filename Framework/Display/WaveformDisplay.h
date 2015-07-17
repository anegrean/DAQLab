//==============================================================================
//
// Title:		WaveformDisplay.h
// Purpose:		A short description of the interface.
//
// Created on:	16-7-2015 at 13:16:19 by Adrian.
// Copyright:	. All Rights Reserved.
//
//==============================================================================

#ifndef __WaveformDisplay_H__
#define __WaveformDisplay_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "cvidef.h"
#include "DataTypes.h"

//==============================================================================
// Constants

//==============================================================================
// Types
		
typedef struct WaveformDisplay   WaveformDisplay_type;

// Waveform display callback function
typedef void (*WaveformDisplayCB_type) (WaveformDisplay_type* waveformDisplay, int event, void* callbackData);

typedef enum {
	
	WaveformDisplay_Close,
	WaveformDisplay_Save
	
} WaveformDisplayEvents;

typedef enum {
	
	WaveformDisplay_RED			=   VAL_RED,
	WaveformDisplay_GREEN		=   VAL_GREEN,
	WaveformDisplay_BLUE		=   VAL_BLUE,
	WaveformDisplay_CYAN		=   VAL_CYAN,
	WaveformDisplay_MAGENTA		=   VAL_MAGENTA,
	WaveformDisplay_YELLOW		=   VAL_YELLOW,
	WaveformDisplay_DK_RED		=   VAL_DK_RED,
	WaveformDisplay_DK_BLUE		=   VAL_DK_BLUE,
	WaveformDisplay_DK_GREEN	=   VAL_DK_GREEN,
	WaveformDisplay_DK_CYAN		=   VAL_DK_CYAN,
	WaveformDisplay_DK_MAGENTA	=   VAL_DK_MAGENTA,
	WaveformDisplay_DK_YELLOW	=   VAL_DK_YELLOW,
	WaveformDisplay_LT_GRAY		=   VAL_LT_GRAY,
	WaveformDisplay_DK_GRAY		=   VAL_DK_GRAY,
	WaveformDisplay_BLACK		=   VAL_BLACK,
	WaveformDisplay_WHITE		=   VAL_WHITE,
	
} WaveformDisplayColor;


//==============================================================================
// External variables

//==============================================================================
// Global functions

WaveformDisplay_type*		init_WaveformDisplay_type		(int parentPanHndl, WaveformDisplayCB_type waveformDisplayCBFptr, void* callbackData);
void						discard_WaveformDisplay_type	(WaveformDisplay_type** waveformDisplayPtr);

int							DisplayWaveform					(WaveformDisplay_type* waveformDisplay, Waveform_type** waveformPtr, 
															 char plotTitle[], WaveformDisplayColor waveformColor);

	// Discards waveform from display and hides panel. Note: this does not discard the waveform display
int							DiscardWaveform					(WaveformDisplay_type* waveformDisplay);

//----------------------------------------------------
// Set/Get
//----------------------------------------------------

void						SetWaveformDisplayCB			(WaveformDisplay_type* waveformDisplay, WaveformDisplayCB_type waveformDisplayCBFptr, void* callbackData);

	// Returns reference to the waveform used for display. Do not modify this waveform directly.
Waveform_type*				GetDisplayWaveform				(WaveformDisplay_type* waveformDisplay);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __WaveformDisplay_H__ */
