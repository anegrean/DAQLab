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
typedef void (*WaveformDisplayCB_type) (WaveformDisplay_type* waveDisp, int event, void* callbackData);

typedef enum {
	
	WaveformDisplay_Close

} WaveformDisplayEvents;


//==============================================================================
// External variables

//==============================================================================
// Global functions

WaveformDisplay_type*		init_WaveformDisplay_type		(int parentPanHndl, char displayTitle[], WaveformDisplayCB_type waveDispCBFptr, void* callbackData);
void						discard_WaveformDisplay_type	(WaveformDisplay_type** waveDispPtr);

// Adds a waveform to display. Multiple waveforms can be added to the same waveform display and the user can make use of a scrollbar to view each waveform.
int							DisplayWaveform					(WaveformDisplay_type* waveDisp, Waveform_type** waveformPtr);

// Discards waveforms from display and hides panel. Note: this does not discard the waveform display
int							DiscardWaveforms				(WaveformDisplay_type* waveDisp);

//----------------------------------------------------
// Set/Get
//----------------------------------------------------

void						SetWaveformDisplayCB			(WaveformDisplay_type* waveDisp, WaveformDisplayCB_type waveDispCBFptr, void* callbackData);

	// Returns a list of Waveform_type* elements used for display. Do not modify this list directly.
ListType					GetDisplayWaveform				(WaveformDisplay_type* waveDisp);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __WaveformDisplay_H__ */
