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

size_t GetWaveformSizeofData (Waveform_type* waveform)
{
	size_t dataTypeSize = 0;
	
	switch (waveform->dataType) {
			
		case Waveform_Char:
		case Waveform_UChar:
			dataTypeSize = sizeof(char);
			break;
			
		case Waveform_Short:
		case Waveform_UShort:
			dataTypeSize = sizeof(short);
			break;
			
		case Waveform_Int:
		case Waveform_UInt:
			dataTypeSize = sizeof(int);
			break;
			
		case Waveform_SSize:
		case Waveform_Size:
			dataTypeSize = sizeof(size_t);
			break;
			
		case Waveform_Float:
			dataTypeSize = sizeof(float);
			break;
			
		case Waveform_Double:
			dataTypeSize = sizeof(double);
			break;	
	}
	
	return dataTypeSize;
}

size_t GetWaveformNumSamples (Waveform_type* waveform)
{
	return waveform->nSamples;
}

void* GetWaveformDataPtr (Waveform_type* waveform)
{
	return waveform->data;
}

int CopyWaveformData (Waveform_type* waveformToCopyTo, Waveform_type* waveformToCopyFrom) 
{
#define AppendWaveformData_Err_SamplingRatesAreDifferent		-1
#define AppendWaveformData_Err_DataTypesAreDifferent			-2
#define AppendWaveformData_Err_UnitsAreDifferent				-3  
#define AppendWaveformData_Err_OutOfMemory						-4
	
	
	// do nothing if there is nothing to be copied
	if (!waveformToCopyFrom) return 0;
	if (!waveformToCopyFrom->nSamples) return 0;
	// check if sampling rates are the same
	if (waveformToCopyTo->samplingRate != waveformToCopyFrom->samplingRate) return AppendWaveformData_Err_SamplingRatesAreDifferent;
	
	// check if data types are the same
	if (waveformToCopyTo->dataType != waveformToCopyFrom->dataType) return AppendWaveformData_Err_DataTypesAreDifferent;
	
	// check if units are the same
	if (waveformToCopyTo->physicalUnit && waveformToCopyFrom->physicalUnit)
	if (strcmp(waveformToCopyTo->physicalUnit, waveformToCopyFrom->physicalUnit)) return AppendWaveformData_Err_UnitsAreDifferent;
	
	void* newBuffer = realloc(waveformToCopyTo->data, (waveformToCopyTo->nSamples + waveformToCopyFrom->nSamples) * GetWaveformSizeofData(waveformToCopyTo));
	if (!newBuffer) return AppendWaveformData_Err_OutOfMemory;
	waveformToCopyTo->data = newBuffer;
	
	switch (waveformToCopyTo->dataType) {
			
		case Waveform_Char:
		case Waveform_UChar:
			memcpy(((char*)waveformToCopyTo->data) + waveformToCopyTo->nSamples, waveformToCopyFrom->data, waveformToCopyFrom->nSamples * sizeof(char)); 
			break;
			
		case Waveform_Short:
		case Waveform_UShort:
			memcpy(((short*)waveformToCopyTo->data) + waveformToCopyTo->nSamples, waveformToCopyFrom->data, waveformToCopyFrom->nSamples * sizeof(short)); 
			break;
			
		case Waveform_Int:
		case Waveform_UInt:
			memcpy(((int*)waveformToCopyTo->data) + waveformToCopyTo->nSamples, waveformToCopyFrom->data, waveformToCopyFrom->nSamples * sizeof(int)); 
			break;
			
		case Waveform_SSize:
		case Waveform_Size:
			memcpy(((size_t*)waveformToCopyTo->data) + waveformToCopyTo->nSamples, waveformToCopyFrom->data, waveformToCopyFrom->nSamples * sizeof(size_t)); 
			break;
			
		case Waveform_Float:
			memcpy(((float*)waveformToCopyTo->data) + waveformToCopyTo->nSamples, waveformToCopyFrom->data, waveformToCopyFrom->nSamples * sizeof(float)); 
			break;
			
		case Waveform_Double:
			memcpy(((double*)waveformToCopyTo->data) + waveformToCopyTo->nSamples, waveformToCopyFrom->data, waveformToCopyFrom->nSamples * sizeof(double)); 
			break;	
	}
	
	return 0;
}

int AppendWaveformData (Waveform_type* waveformToAppendTo, Waveform_type** waveformToAppend)
{
	int error;
	
	if ((error = CopyWaveformData(waveformToAppendTo, *waveformToAppend)) < 0) return error;
	
	discard_Waveform_type(waveformToAppend);
	return 0;
}
