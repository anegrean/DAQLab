//==============================================================================
//
// Title:		DataTypes.c
// Purpose:		A short description of the implementation.
//
// Created on:	10/8/2014 at 7:56:29 PM by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "DataTypes.h" 
#include <ansi_c.h>
#include "toolbox.h"


//==============================================================================
// Constants

#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;}   

//==============================================================================
// Types

struct Waveform {
	WaveformTypes		dataType;
	char*				YAxisName;
	char*				physicalUnit;			// Physical SI unit such as V, A, Ohm, etc.
	double				dateTimestamp;			// Number of seconds since midnight, January 1, 1900 in the local time zone.
	double				samplingRate;			// Sampling rate in [Hz]. If 0, sampling rate is not given.
	size_t				nSamples;
	void*				data;					// Array of dataType elements.
};

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//==============================================================================
// Global variables

//==============================================================================
// Global functions

Waveform_type* init_Waveform_type (WaveformTypes dataType, char YAxisName[], char physicalUnit[], double dateTimestamp, double samplingRate, size_t nSamples, void* data)
{
	Waveform_type*	waveform = malloc(sizeof(Waveform_type));
	if (!waveform) return NULL;
	
	waveform->dataType 			= dataType;
	waveform->samplingRate 		= samplingRate;
	waveform->YAxisName			= StrDup(YAxisName);
	waveform->physicalUnit		= StrDup(physicalUnit);
	waveform->dateTimestamp		= dateTimestamp;
	waveform->nSamples			= nSamples;
	waveform->data				= data;
	
	return waveform;
}

void discard_Waveform_type (Waveform_type** waveform)
{
	if (!*waveform) return;
	
	OKfree((*waveform)->YAxisName);
	OKfree((*waveform)->physicalUnit);
	OKfree((*waveform)->data);
	
	OKfree(*waveform);
}
