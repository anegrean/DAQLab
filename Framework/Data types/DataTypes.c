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
#include <nidaqmx.h>





//==============================================================================
// Constants

#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;}   

 	// CO Frequency 
#define DAQmxDefault_CO_Frequency_Task_freq			10000000   		// frequency
#define DAQmxDefault_CO_Frequency_Task_dutycycle    50.0			// duty cycle
#define DAQmxDefault_CO_Frequency_Task_hightime		0.0005   		// the time the pulse stays high [s]
#define DAQmxDefault_CO_Frequency_Task_lowtime     	0.0005			// the time the pulse stays low [s]
#define DAQmxDefault_CO_Frequency_Task_highticks	10   			// the ticks the pulse stays high [s]
#define DAQmxDefault_CO_Frequency_Task_lowticks     2				// the ticks the pulse stays low [s]	
#define DAQmxDefault_CO_Frequency_Task_initialdel 	0.0			    // initial delay [s]
#define DAQmxDefault_CO_Frequency_Task_idlestate    DAQmx_Val_Low   // terminal state at rest, pulses switch this to the oposite state
#define DAQmxDefault_CO_Frequency_Task_timeout      10.0            // timeout  
#define DAQmxDefault_CO_Frequency_Task_npulses      1000            // number of finite pulses     	



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
// Pulsetrains
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------

PulseTrain_type* init_PulseTrain (PulseTrainTimingTypes type,int mode) 
{
		
	
	PulseTrainFreqTiming_type* freqtiming;
	PulseTrainTimeTiming_type* timetiming;
	PulseTrainTickTiming_type* ticktiming;
	
	PulseTrain_type* pulsetrain = malloc (sizeof(PulseTrain_type));
	
	pulsetrain->type=type;
	
	switch (type){
		case PulseTrain_Freq:
			freqtiming=malloc(sizeof(PulseTrainFreqTiming_type));
			freqtiming->frequency		= DAQmxDefault_CO_Frequency_Task_freq;
			freqtiming->dutycycle		= DAQmxDefault_CO_Frequency_Task_dutycycle;
			pulsetrain->timing=freqtiming;
			break;
		case PulseTrain_Time:
			timetiming=malloc(sizeof(PulseTrainTimeTiming_type));  
			timetiming->hightime		= DAQmxDefault_CO_Frequency_Task_hightime;
			timetiming->lowtime			= DAQmxDefault_CO_Frequency_Task_lowtime;
			pulsetrain->timing=timetiming; 
			break;
		case PulseTrain_Ticks:
			ticktiming=malloc(sizeof(PulseTrainTickTiming_type));   
			ticktiming->highticks		= DAQmxDefault_CO_Frequency_Task_highticks;
			ticktiming->lowticks		= DAQmxDefault_CO_Frequency_Task_lowticks; 
			pulsetrain->timing=ticktiming; 
			break;
	}
	pulsetrain -> chanName			= NULL;
	pulsetrain -> refclk_source		= NULL;
	pulsetrain -> trigger_source	= NULL;
	pulsetrain -> idlestate			= DAQmxDefault_CO_Frequency_Task_idlestate;
	pulsetrain -> initialdelay		= DAQmxDefault_CO_Frequency_Task_initialdel;
	pulsetrain -> timeout			= DAQmxDefault_CO_Frequency_Task_timeout;
	pulsetrain -> mode				= mode;    
	pulsetrain -> npulses			= DAQmxDefault_CO_Frequency_Task_npulses; 
	pulsetrain -> refclk_slope		= 0;
	pulsetrain -> trigger_slope		= 0;
	
	return pulsetrain;
}


void discard_PulseTrain (PulseTrain_type* pulsetrain)
{
	if (!pulsetrain) return;
	
	OKfree((pulsetrain)-> chanName);     
	OKfree((pulsetrain)-> refclk_source);     
	OKfree((pulsetrain)-> trigger_source);  
//	OKfree((pulsetrain)-> timing);   
	
	// discard pulsetrain
	OKfree(pulsetrain);
}


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
