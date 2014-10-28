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
	WaveformTypes			waveformType;				// Waveform data type.
	char*					waveformName;				// Name of signal represented by the waveform. 
	char*					unitName;					// Physical SI unit such as V, A, Ohm, etc.
	double					dateTimestamp;				// Number of seconds since midnight, January 1, 1900 in the local time zone.
	double					samplingRate;				// Sampling rate in [Hz]. If 0, sampling rate is not given.
	size_t					nSamples;					// Number of samples in the waveform.
	void*					data;						// Array of waveformType elements.
};

struct RepeatedWaveform {
	RepeatedWaveformTypes	waveformType;				// Waveform data type.  
	double					samplingRate;				// Sampling rate in [Hz]. If 0, sampling rate is not given.
	double					repeat;						// number of times to repeat the waveform.
	size_t					nSamples;					// Number of samples in the waveform.
	void*					data;						// Array of waveformType elements. 
};

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//==============================================================================
// Global variables

//==============================================================================
// Global functions


//---------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Waveforms
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------

Waveform_type* init_Waveform_type (WaveformTypes waveformType, double samplingRate, size_t nSamples, void* waveformData)
{
	Waveform_type*	waveform = malloc(sizeof(Waveform_type));
	if (!waveform) return NULL;
	
	waveform->waveformType 		= waveformType;
	waveform->samplingRate 		= samplingRate;
	waveform->waveformName		= NULL;
	waveform->unitName			= NULL;
	waveform->dateTimestamp		= 0;
	waveform->nSamples			= nSamples;
	waveform->data				= waveformData;
	
	return waveform;
}

void discard_Waveform_type (Waveform_type** waveform)
{
	if (!*waveform) return;
	
	OKfree((*waveform)->waveformName);
	OKfree((*waveform)->unitName);
	OKfree((*waveform)->data);
	
	OKfree(*waveform);
}

void SetWaveformName (Waveform_type* waveform, char waveformName[])
{
	waveform->waveformName = StrDup(waveformName);
}

void SetWaveformPhysicalUnit (Waveform_type* waveform, char unitName[])
{
	waveform->unitName = StrDup(unitName); 
}

int AddWaveformDateTimestamp (Waveform_type* waveform)
{
	return GetCurrentDateTime(&waveform->dateTimestamp);
}

double GetWaveformDateTimestamp (Waveform_type* waveform)
{
	return waveform->dateTimestamp;
}

size_t GetWaveformSizeofData (Waveform_type* waveform)
{
	size_t dataTypeSize = 0;
	
	switch (waveform->waveformType) {
			
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

void* GetWaveformDataPtr (Waveform_type* waveform, size_t* nSamples)
{
	*nSamples = waveform->nSamples;
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
	if (waveformToCopyTo->waveformType != waveformToCopyFrom->waveformType) return AppendWaveformData_Err_DataTypesAreDifferent;
	
	// check if units are the same
	if (waveformToCopyTo->unitName && waveformToCopyFrom->unitName)
	if (strcmp(waveformToCopyTo->unitName, waveformToCopyFrom->unitName)) return AppendWaveformData_Err_UnitsAreDifferent;
	
	void* newBuffer = realloc(waveformToCopyTo->data, (waveformToCopyTo->nSamples + waveformToCopyFrom->nSamples) * GetWaveformSizeofData(waveformToCopyTo));
	if (!newBuffer) return AppendWaveformData_Err_OutOfMemory;
	waveformToCopyTo->data = newBuffer;
	
	switch (waveformToCopyTo->waveformType) {
			
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

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Repeated Waveforms
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------

RepeatedWaveform_type* init_RepeatedWaveform_type (RepeatedWaveformTypes waveformType, double samplingRate, size_t nSamples, void* data, double repeat)
{
	RepeatedWaveform_type* waveform = malloc (sizeof(RepeatedWaveform_type));
	if (!waveform) return NULL;
	
	waveform->waveformType 		= waveformType;
	waveform->samplingRate 		= samplingRate;
	waveform->repeat			= repeat;
	waveform->nSamples			= nSamples;
	waveform->data				= data;
	
	return waveform;
}

void discard_RepeatedWaveform_type (RepeatedWaveform_type** waveform)
{
	if (!*waveform) return;
	
	// discard data
	OKfree((*waveform)->data);
	
	OKfree(*waveform);
}

void* GetRepeatedWaveformDataPtr (RepeatedWaveform_type* waveform, size_t* nSamples)
{
	*nSamples = waveform->nSamples;
	return waveform->data;
}

double GetRepeatedWaveformRepeats (RepeatedWaveform_type* waveform)
{
	return waveform->repeat;
}

size_t GetRepeatedWaveformNumSamples (RepeatedWaveform_type* waveform)
{
	return waveform->nSamples;
}

size_t GetRepeatedWaveformSizeofData (RepeatedWaveform_type* waveform)
{
	size_t dataTypeSize = 0;
	
	switch (waveform->waveformType) {
			
		case RepeatedWaveform_Char:
		case RepeatedWaveform_UChar:
			dataTypeSize = sizeof(char);
			break;
			
		case RepeatedWaveform_Short:
		case RepeatedWaveform_UShort:
			dataTypeSize = sizeof(short);
			break;
			
		case RepeatedWaveform_Int:
		case RepeatedWaveform_UInt:
			dataTypeSize = sizeof(int);
			break;
			
		case RepeatedWaveform_SSize:
		case RepeatedWaveform_Size:
			dataTypeSize = sizeof(size_t);
			break;
			
		case RepeatedWaveform_Float:
			dataTypeSize = sizeof(float);
			break;
			
		case RepeatedWaveform_Double:
			dataTypeSize = sizeof(double);
			break;	
	}
	
	return dataTypeSize;
	
}
