//==============================================================================
//
// Title:		WaveformDisplay.h
// Purpose:		Displays waveforms.
//
// Created on:	16-7-2015 at 13:16:19 by Adrian Negrean.
// Copyright:	VU University Amsterdam. All Rights Reserved.
// License:     This Source Code Form is subject to the terms of the Mozilla Public 
//              License v. 2.0. If a copy of the MPL was not distributed with this 
//              file, you can obtain one at https://mozilla.org/MPL/2.0/ .
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
ListType					GetDisplayWaveformList				(WaveformDisplay_type* waveDisp);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __WaveformDisplay_H__ */
