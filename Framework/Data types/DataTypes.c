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
#include "DAQLabUtility.h"
#include <ansi_c.h>
#include "toolbox.h"
#include <nidaqmx.h>





//==============================================================================
// Constants

#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;}   

//==============================================================================
// Types

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Waveforms
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------

struct Waveform {
	WaveformTypes			waveformType;				// Waveform data type.
	char*					waveformName;				// Name of signal represented by the waveform. 
	char*					unitName;					// Physical SI unit such as V, A, Ohm, etc.
	double					dateTimestamp;				// Number of seconds since midnight, January 1, 1900 in the local time zone.
	double					samplingRate;				// Sampling rate in [Hz]. If 0, sampling rate is not given.
	size_t					nSamples;					// Number of samples in the waveform.
	void*					data;						// Array of waveformType elements.
};

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Repeated Waveforms
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------

struct RepeatedWaveform {
	RepeatedWaveformTypes	waveformType;				// Waveform data type. 
	char*					waveformName;				// Name of signal represented by the waveform.
	char*					unitName;					// Physical SI unit such as V, A, Ohm, etc.
	double					dateTimestamp;				// Number of seconds since midnight, January 1, 1900 in the local time zone.  
	double					samplingRate;				// Sampling rate in [Hz]. If 0, sampling rate is not given.
	double					repeat;						// number of times to repeat the waveform.
	size_t					nSamples;					// Number of samples in the waveform.
	void*					data;						// Array of waveformType elements. 
};

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Pulse trains
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------

// parent class
struct PulseTrain {
	PulseTrainTimingTypes		pulseType;					// timing type.  
	PulseTrainIdleStates		idleState;					// counter output idle state
	PulseTrainModes				mode;						// measurement mode
	uInt64						nPulses;					// number of pulses
};

// pulse train defined by frequency
struct PulseTrainFreqTiming {
	PulseTrain_type				baseClass;
	double 						frequency;					// pulse frequencu in [Hz]   				 
	double 						dutyCycle;					// width of the pulse divided by the period in [%]  
	double						initialDelay;    		   	// initial delay [s] 
};

// pulse train defined by pulse duration
struct PulseTrainTimeTiming {
	PulseTrain_type				baseClass; 
	double 						highTime;   				// the time the pulse stays high [s]
	double 						lowTime;    				// the time the pulse stays low [s]
	double						initialDelay;    		    // initial delay [s]
};

// pulse train defined by number of ticks
struct PulseTrainTickTiming {
	PulseTrain_type				baseClass; 
	uInt32 						highTicks;
	uInt32 						lowTicks;
	uInt32 						delayTicks;
};


//==============================================================================
// Static Functions

	// Pulse Train
static void 				init_PulseTrain_type 					(PulseTrain_type* pulseTrain, PulseTrainTimingTypes pulseType, 
																	 PulseTrainModes mode, PulseTrainIdleStates idleState, uInt64 nPulses);

	// ROI base class
static void 				init_ROI_type							(ROI_type* ROI, ROITypes ROIType, char ROIName[], DiscardFptr_type discardFptr);

static void					discard_ROIBaseClass					(ROI_type** ROIPtr);

	// Point ROI
static void 				discard_Point_type 						(Point_type** PointPtr);

	// Rectangle ROI
static void 				discard_Rect_type 						(Rect_type** RectPtr);


//==============================================================================
// Global Functions (other than defined in DataTypes.h)



//==============================================================================

static void init_PulseTrain_type (PulseTrain_type* pulseTrain, PulseTrainTimingTypes pulseType, PulseTrainModes mode, PulseTrainIdleStates idleState, uInt64 nPulses) 
{
	pulseTrain->pulseType		= pulseType;
	pulseTrain->mode			= mode;
	pulseTrain->nPulses			= nPulses;
	pulseTrain->idleState		= idleState;
}

PulseTrainFreqTiming_type* init_PulseTrainFreqTiming_type (PulseTrainModes mode, PulseTrainIdleStates idleState, uInt64 nPulses, double dutyCycle, double frequency, double initialDelay)
{
	PulseTrainFreqTiming_type*	pulseTrain = malloc (sizeof(PulseTrainFreqTiming_type));
	if (!pulseTrain) return NULL;
	
	// init base class
	init_PulseTrain_type(&pulseTrain->baseClass, PulseTrain_Freq, mode, idleState, nPulses);
	
	// init PulseTrainFreqTiming child class
	pulseTrain->dutyCycle		= dutyCycle;
	pulseTrain->frequency		= frequency;  
	pulseTrain->initialDelay	= initialDelay;
	
	return pulseTrain;
}

PulseTrainTimeTiming_type* init_PulseTrainTimeTiming_type (PulseTrainModes mode, PulseTrainIdleStates idleState, uInt64 nPulses, double highTime, double lowTime, double initialDelay)
{
	PulseTrainTimeTiming_type* pulseTrain = malloc (sizeof(PulseTrainTimeTiming_type));
	if (!pulseTrain) return NULL;
	
	// init base class
	init_PulseTrain_type(&pulseTrain->baseClass, PulseTrain_Time, mode, idleState, nPulses);
	
	// init PulseTrainTimeTiming
	pulseTrain->highTime		= highTime;
	pulseTrain->lowTime			= lowTime;
	pulseTrain->initialDelay	= initialDelay;   
	
	return pulseTrain;
}

PulseTrainTickTiming_type* init_PulseTrainTickTiming_type (PulseTrainModes mode, PulseTrainIdleStates idleState, uInt64 nPulses, uInt32 highTicks, uInt32 lowTicks, uInt32 delayTicks)
{
	PulseTrainTickTiming_type* pulseTrain = malloc (sizeof(PulseTrainTickTiming_type));
	if (!pulseTrain) return NULL;
	
	// init base class
	init_PulseTrain_type(&pulseTrain->baseClass, PulseTrain_Ticks, mode, idleState, nPulses);
	
	// init PulseTrainTickTiming
	pulseTrain->highTicks		= highTicks;  
	pulseTrain->lowTicks		= lowTicks;  
	pulseTrain->delayTicks		= delayTicks;  
	
	return pulseTrain;
}

void discard_PulseTrain_type (PulseTrain_type** pulseTrain)
{
	if (!*pulseTrain) return;
	
	OKfree(*pulseTrain);
}

// sets the pulsetrain idle state
void SetPulseTrainIdleState (PulseTrain_type* pulseTrain, PulseTrainIdleStates idleState)
{
	pulseTrain->idleState = idleState;
}

// gets the pulsetrain idle state
PulseTrainIdleStates GetPulseTrainIdleState (PulseTrain_type* pulseTrain)
{
	return pulseTrain->idleState;
}

// sets the pulsetrain amount of pulses
void SetPulseTrainNPulses (PulseTrain_type* pulseTrain, uInt64 nPulses)
{
	pulseTrain->nPulses = nPulses;	
}

// gets the pulsetrain amount of pulses
uInt64 GetPulseTrainNPulses (PulseTrain_type* pulseTrain)
{
	return pulseTrain->nPulses;	
}

// sets the pulsetrain mode
void SetPulseTrainMode (PulseTrain_type* pulseTrain, PulseTrainModes mode)
{
	pulseTrain->mode = mode;	
}

// gets the pulsetrain mode
PulseTrainModes GetPulseTrainMode (PulseTrain_type* pulseTrain)
{
	return pulseTrain->mode;	
}

// gets the pulsetrain type
PulseTrainTimingTypes GetPulseTrainType (PulseTrain_type* pulseTrain)
{
	return pulseTrain->pulseType;	
}

//sets the pulsetrain frequency
void SetPulseTrainFreqTimingFreq (PulseTrainFreqTiming_type* pulseTrain, double frequency)
{
	pulseTrain->frequency = frequency;
}

// gets the pulsetrain frequency
double GetPulseTrainFreqTimingFreq (PulseTrainFreqTiming_type* pulseTrain)
{
	  return pulseTrain->frequency;
}

// sets the pulsetrain duty cycle
void SetPulseTrainFreqTimingDutyCycle (PulseTrainFreqTiming_type* pulseTrain, double dutyCycle)
{
	pulseTrain->dutyCycle = dutyCycle;
}

// gets the pulsetrain duty cycle
double GetPulseTrainFreqTimingDutyCycle (PulseTrainFreqTiming_type* pulseTrain)
{
	return pulseTrain->dutyCycle;
}

// sets the pulsetrain duty cycle
void SetPulseTrainFreqTimingInitialDelay (PulseTrainFreqTiming_type* pulseTrain, double initialDelay)
{
	pulseTrain->initialDelay = initialDelay;
}

// gets the pulsetrain initial delay 
double GetPulseTrainFreqTimingInitialDelay (PulseTrainFreqTiming_type* pulseTrain)
{
	 return pulseTrain->initialDelay;
}

// sets the pulsetrain highticks
void SetPulseTrainTickTimingHighTicks (PulseTrainTickTiming_type* pulseTrain, uInt32 highTicks)
{
	pulseTrain->highTicks = highTicks;
}

// gets the pulsetrain highticks
uInt32 GetPulseTrainTickTimingHighTicks (PulseTrainTickTiming_type* pulseTrain)
{
	 return pulseTrain->highTicks;
}

// sets the pulsetrain lowticks
void SetPulseTrainTickTimingLowTicks (PulseTrainTickTiming_type* pulseTrain, uInt32 lowTicks)
{
	pulseTrain->lowTicks=lowTicks;
}

// gets the pulsetrain lowticks
uInt32 GetPulseTrainTickTimingLowTicks (PulseTrainTickTiming_type* pulseTrain)
{
	return pulseTrain->lowTicks;
}

// sets the pulsetrain delayticks
void SetPulseTrainTickTimingDelayTicks (PulseTrainTickTiming_type* pulseTrain, uInt32 delayTicks)
{
	pulseTrain->delayTicks = delayTicks;
}

// gets the pulsetrain delay ticks
uInt32 GetPulseTrainTickTimingDelayTicks (PulseTrainTickTiming_type* pulseTrain)
{
	return pulseTrain->delayTicks;
}

// sets the pulsetrain high time
void SetPulseTrainTimeTimingHighTime (PulseTrainTimeTiming_type* pulseTrain, double highTime)
{
	pulseTrain->highTime = highTime;
}

// gets the pulsetrain high time
double GetPulseTrainTimeTimingHighTime (PulseTrainTimeTiming_type* pulseTrain)
{
	 return pulseTrain->highTime;
}

// sets the pulsetrain low time
void SetPulseTrainTimeTimingLowTime (PulseTrainTimeTiming_type* pulseTrain, double lowTime)
{
	pulseTrain->lowTime = lowTime;
}

// gets the pulsetrain low time
double GetPulseTrainTimeTimingLowTime (PulseTrainTimeTiming_type* pulseTrain)
{
	 return pulseTrain->lowTime;
}

//sets the pulsetrain initial delay
void SetPulseTrainTimeTimingInitialDelay (PulseTrainTimeTiming_type* pulseTrain, double delayTime)
{
	pulseTrain->initialDelay = delayTime;
}

//gets the pulsetrain delay time
double GetPulseTrainTimeTimingInitialDelay (PulseTrainTimeTiming_type* pulseTrain)
{
	return pulseTrain->initialDelay;
}

PulseTrain_type* CopyPulseTrain(PulseTrain_type* pulsetrain)
{
	PulseTrain_type* 		newpulsetrain			= NULL;
	PulseTrainTimingTypes 	pulseTrainType;
	
	pulseTrainType=GetPulseTrainType(pulsetrain);
	switch (pulseTrainType) {
		case PulseTrain_Freq:
			newpulsetrain   = (PulseTrain_type*) init_PulseTrainFreqTiming_type(GetPulseTrainMode(pulsetrain), GetPulseTrainIdleState(pulsetrain), GetPulseTrainNPulses(pulsetrain), 
									  GetPulseTrainFreqTimingDutyCycle((PulseTrainFreqTiming_type*)pulsetrain), GetPulseTrainFreqTimingFreq((PulseTrainFreqTiming_type*)pulsetrain), 
									  GetPulseTrainFreqTimingDutyCycle((PulseTrainFreqTiming_type*)pulsetrain)); 
			break;
			
		case PulseTrain_Time:
			newpulsetrain   = (PulseTrain_type*) init_PulseTrainTimeTiming_type(GetPulseTrainMode(pulsetrain), GetPulseTrainIdleState(pulsetrain), GetPulseTrainNPulses(pulsetrain), 
									  GetPulseTrainTimeTimingHighTime((PulseTrainTimeTiming_type*)pulsetrain), GetPulseTrainTimeTimingLowTime((PulseTrainTimeTiming_type*)pulsetrain), 
									  GetPulseTrainFreqTimingDutyCycle((PulseTrainFreqTiming_type*)pulsetrain)); 
			break;
			
		case PulseTrain_Ticks:
			newpulsetrain   = (PulseTrain_type*) init_PulseTrainTickTiming_type(GetPulseTrainMode(pulsetrain), GetPulseTrainIdleState(pulsetrain), GetPulseTrainNPulses(pulsetrain),
									  GetPulseTrainTickTimingHighTicks((PulseTrainTickTiming_type*)pulsetrain), GetPulseTrainTickTimingLowTicks((PulseTrainTickTiming_type*)pulsetrain), 
									  GetPulseTrainTickTimingDelayTicks((PulseTrainTickTiming_type*)pulsetrain));  
			break;
	}
	
	return newpulsetrain;
}


//---------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Waveforms
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------

Waveform_type* init_Waveform_type (WaveformTypes waveformType, double samplingRate, size_t nSamples, void** ptrToData)
{
	Waveform_type*	waveform = malloc(sizeof(Waveform_type));
	if (!waveform) return NULL;
	
	waveform->waveformType 		= waveformType;
	waveform->samplingRate 		= samplingRate;
	waveform->waveformName		= NULL;
	waveform->unitName			= NULL;
	waveform->dateTimestamp		= 0;
	waveform->nSamples			= nSamples;
	waveform->data				= *ptrToData;  // assign data
	*ptrToData					= NULL;		   // consume data
	
	return waveform;
}

void discard_Waveform_type (Waveform_type** waveform)
{
	if (!*waveform) return;
	
	OKfree((*waveform)->waveformName);
	OKfree((*waveform)->unitName);
	OKfree((*waveform)->data);
	(*waveform)->nSamples = 0;
	
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

double GetWaveformSamplingRate (Waveform_type* waveform)
{
	return waveform->samplingRate;
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

void** GetWaveformPtrToData (Waveform_type* waveform, size_t* nSamples)
{
	*nSamples = waveform->nSamples;
	return &waveform->data;
}

WaveformTypes GetWaveformDataType (Waveform_type* waveform)
{
	return waveform->waveformType;
}

int CopyWaveform (Waveform_type** waveformCopy, Waveform_type* waveform, char** errorInfo)
{
#define CopyWaveform_Err_OutOfMemory	-1

	void*	nullData	= NULL;
	int		error		= 0;
	char*	errMsg		= NULL;
	
	*waveformCopy = init_Waveform_type(waveform->waveformType, waveform->samplingRate, 0, &nullData);
	if (!*waveformCopy) {
		if (errorInfo)
			*errorInfo = FormatMsg(CopyWaveform_Err_OutOfMemory, "CopyWaveform", "Out of memory");
		return CopyWaveform_Err_OutOfMemory;
	}
	
	// copy waveform attributes
	(*waveformCopy)->waveformName 	= StrDup(waveform->waveformName);
	(*waveformCopy)->unitName		= StrDup(waveform->unitName);
	(*waveformCopy)->dateTimestamp	= waveform->dateTimestamp;
	
	errChk( AppendWaveform(*waveformCopy, waveform, &errMsg) );
	
	return 0;
	
Error:
	
	discard_Waveform_type(waveformCopy);
	
	if (errorInfo)
		*errorInfo = FormatMsg(error, "CopyWaveform", errMsg);	// chain error
	OKfree(errMsg);
	return error;
}

int AppendWaveform (Waveform_type* waveformToAppendTo, Waveform_type* waveformToAppend, char** errorInfo) 
{
#define AppendWaveformData_Err_SamplingRatesAreDifferent		-1
#define AppendWaveformData_Err_DataTypesAreDifferent			-2
#define AppendWaveformData_Err_UnitsAreDifferent				-3  
#define AppendWaveformData_Err_OutOfMemory						-4
	
	// do nothing if there is nothing to be copied
	if (!waveformToAppend) return 0;
	if (!waveformToAppend->nSamples) return 0;
	// check if sampling rates are the same
	if (waveformToAppendTo->samplingRate != waveformToAppend->samplingRate) {
		if (errorInfo)
			*errorInfo = FormatMsg(AppendWaveformData_Err_SamplingRatesAreDifferent, "AppendWaveform", "Waveform sampling rates are different");
		return AppendWaveformData_Err_SamplingRatesAreDifferent;	
	}
	
	// check if data types are the same
	if (waveformToAppendTo->waveformType != waveformToAppend->waveformType) {
		if (errorInfo)
			*errorInfo = FormatMsg(AppendWaveformData_Err_DataTypesAreDifferent, "AppendWaveform", "Waveform data types are different");
		return AppendWaveformData_Err_DataTypesAreDifferent;
	}
	
	// check if units are the same
	if (waveformToAppendTo->unitName && waveformToAppend->unitName)
		if (strcmp(waveformToAppendTo->unitName, waveformToAppend->unitName)) {
			if (errorInfo)
				*errorInfo = FormatMsg(AppendWaveformData_Err_UnitsAreDifferent, "AppendWaveform", "Waveform units must be the same");
			return AppendWaveformData_Err_UnitsAreDifferent;
		}
	
	if (waveformToAppendTo->unitName || waveformToAppend->unitName) {
		if (errorInfo)
			*errorInfo = FormatMsg(AppendWaveformData_Err_UnitsAreDifferent, "AppendWaveform", "Waveform units must be the same");
		return AppendWaveformData_Err_UnitsAreDifferent;
	}
	
	
	void* dataBuffer = realloc(waveformToAppendTo->data, (waveformToAppendTo->nSamples + waveformToAppend->nSamples) * GetWaveformSizeofData(waveformToAppendTo));
	if (!dataBuffer) {
		if (errorInfo)
			*errorInfo = FormatMsg(AppendWaveformData_Err_OutOfMemory, "AppendWaveform", "Out of memory");
		return AppendWaveformData_Err_OutOfMemory;
	}
	
	switch (waveformToAppendTo->waveformType) {
			
		case Waveform_Char:
		case Waveform_UChar:
			memcpy(((char*)dataBuffer) + waveformToAppendTo->nSamples, waveformToAppend->data, waveformToAppend->nSamples * sizeof(char)); 
			break;
			
		case Waveform_Short:
		case Waveform_UShort:
			memcpy(((short*)dataBuffer) + waveformToAppendTo->nSamples, waveformToAppend->data, waveformToAppend->nSamples * sizeof(short)); 
			break;
			
		case Waveform_Int:
		case Waveform_UInt:
			memcpy(((int*)dataBuffer) + waveformToAppendTo->nSamples, waveformToAppend->data, waveformToAppend->nSamples * sizeof(int)); 
			break;
			
		case Waveform_SSize:
		case Waveform_Size:
			memcpy(((size_t*)dataBuffer) + waveformToAppendTo->nSamples, waveformToAppend->data, waveformToAppend->nSamples * sizeof(size_t)); 
			break;
			
		case Waveform_Float:
			memcpy(((float*)dataBuffer) + waveformToAppendTo->nSamples, waveformToAppend->data, waveformToAppend->nSamples * sizeof(float)); 
			break;
			
		case Waveform_Double:
			memcpy(((double*)dataBuffer) + waveformToAppendTo->nSamples, waveformToAppend->data, waveformToAppend->nSamples * sizeof(double)); 
			break;	
	}
	
	waveformToAppendTo->data = dataBuffer; 
	waveformToAppendTo->nSamples += waveformToAppend->nSamples;
	
	return 0;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Repeated Waveforms
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------

RepeatedWaveform_type* init_RepeatedWaveform_type (RepeatedWaveformTypes waveformType, double samplingRate, size_t nSamples, void** ptrToData, double repeat)
{
	RepeatedWaveform_type* waveform = malloc (sizeof(RepeatedWaveform_type));
	if (!waveform) return NULL;
	
	waveform->waveformType 		= waveformType;
	waveform->waveformName		= NULL;
	waveform->unitName			= NULL;
	waveform->dateTimestamp		= 0;
	waveform->samplingRate 		= samplingRate;
	waveform->repeat			= repeat;
	waveform->nSamples			= nSamples;
	waveform->data				= *ptrToData;  // assign data
	*ptrToData					= NULL;		   // consume data
	
	return waveform;
}

RepeatedWaveform_type* ConvertWaveformToRepeatedWaveformType (Waveform_type** waveform, double repeat)
{
	RepeatedWaveformTypes	repWaveformType		= Waveform_Char;
	
	switch ((*waveform)->waveformType) {
			
		case Waveform_Char:
			repWaveformType = RepeatedWaveform_Char;
			break;
			
		case Waveform_UChar:
			repWaveformType = RepeatedWaveform_UChar; 
			break;
			
		case Waveform_Short:
			repWaveformType = RepeatedWaveform_Short; 
			break;
			
		case Waveform_UShort:
			repWaveformType = RepeatedWaveform_UShort;
			break;
			
		case Waveform_Int:
			repWaveformType = RepeatedWaveform_Int;
			break;
			
		case Waveform_UInt:
			repWaveformType = RepeatedWaveform_UInt;
			break;
			
		case Waveform_SSize:
			repWaveformType = RepeatedWaveform_SSize;
			break;
			
		case Waveform_Size:
			repWaveformType = RepeatedWaveform_Size;
			break;
			
		case Waveform_Float:
			repWaveformType = RepeatedWaveform_Float;
			break;
			
		case Waveform_Double:
			repWaveformType = RepeatedWaveform_Double; 
			break;
	}
	
	RepeatedWaveform_type* repWaveform = init_RepeatedWaveform_type(repWaveformType, (*waveform)->samplingRate, (*waveform)->nSamples, &(*waveform)->data, repeat);
	if (!repWaveform) return NULL;
	// transfer info from waveform to repeated waveform
	repWaveform->waveformName 	= (*waveform)->waveformName;
	(*waveform)->waveformName 	= NULL;
	repWaveform->unitName 		= (*waveform)->unitName;
	(*waveform)->waveformName 	= NULL;
	repWaveform->dateTimestamp 	= (*waveform)->dateTimestamp;
	
	// discard waveform data structure
	discard_Waveform_type(waveform);
	
	return repWaveform;
}

void discard_RepeatedWaveform_type (RepeatedWaveform_type** waveform)
{
	if (!*waveform) return;
	
	// discard data
	OKfree((*waveform)->data);
	OKfree((*waveform)->waveformName);
	OKfree((*waveform)->unitName);
	
	OKfree(*waveform);
}

void** GetRepeatedWaveformPtrToData (RepeatedWaveform_type* waveform, size_t* nSamples)
{
	*nSamples = waveform->nSamples;
	return &waveform->data;
}

double GetRepeatedWaveformRepeats (RepeatedWaveform_type* waveform)
{
	return waveform->repeat;
}

double GetRepeatedWaveformSamplingRate (RepeatedWaveform_type* waveform)
{
	return waveform->samplingRate;
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

//---------------------------------------------------------------------------------------------------------  
// Region Of Interest (ROI) types for images
//---------------------------------------------------------------------------------------------------------

//-------------------------
// ROI
//-------------------------

static void init_ROI_type (ROI_type* ROI, ROITypes ROIType, char ROIName[], DiscardFptr_type discardFptr)
{
	ROI->ROIType 		= ROIType;
	ROI->ROIName		= StrDup(ROIName);
	ROI->active			= TRUE;
	
	// init color to opaque black
	ROI->rgba.R			= 0;
	ROI->rgba.G			= 0;
	ROI->rgba.B			= 0;
	ROI->rgba.A			= 0;
	
	
	ROI->discardFptr	= discardFptr;
}

static void	discard_ROIBaseClass (ROI_type** ROIPtr)
{
	if (!*ROIPtr) return;
	
	// ROI name
	OKfree((*ROIPtr)->ROIName);
	
	OKfree(*ROIPtr);
}				  

//-------------------------
// Point
//-------------------------
Point_type* init_Point_type (char ROIName[], int x, int y)
{
	Point_type* 	point = malloc(sizeof(Point_type));
	if (!point) return NULL;
	
	
	// init base class
	init_ROI_type(&point->baseClass, ROI_Point, ROIName, (DiscardFptr_type) discard_Point_type); 
	
	// init point child class
	point->x	= x;
	point->y	= y;
	
	return point;
}
			  
static void discard_Point_type (Point_type** PointPtr)
{
	if (!*PointPtr) return;
	
	// discard Point specific data
	
	// discard base class data
	discard_ROIBaseClass((ROI_type**)PointPtr);
}

//-------------------------
// Rectangle
//-------------------------
	
Rect_type* init_Rect_type (char ROIName[], int top, int left, int height, int width)
{
	Rect_type*	rect = malloc(sizeof(Rect_type));
	if (!rect) return NULL;
	
	// init base class
	init_ROI_type(&rect->baseClass, ROI_Rectangle, ROIName, (DiscardFptr_type) discard_Rect_type); 
	
	// init rectangle child class
	rect->top 		= top;
	rect->left 		= left;
	rect->height	= height;
	rect->width		= width;
	
	return rect;
}

static void discard_Rect_type (Rect_type** RectPtr)
{
	if (!*RectPtr) return;
	
	// discard Rectangle specific data
	
	// discard base class data
	discard_ROIBaseClass((ROI_type**)RectPtr);
}

//-------------------------     
// All ROIs
//-------------------------     

void discard_ROI_type (ROI_type** ROIPtr)
{
	if (!*ROIPtr) return;
	
	// call ROI specific discard method
	(*(*ROIPtr)->discardFptr) ((void**)ROIPtr);
}

ROI_type* copy_ROI_type (ROI_type* ROI)
{
	ROI_type*	ROICopy	= NULL;
	
	if (!ROI) return NULL;
	
	switch (ROI->ROIType) {
			
		case ROI_Point:
			
			ROICopy = (ROI_type*)init_Point_type(ROI->ROIName, ((Point_type*)ROI)->x, ((Point_type*)ROI)->y);
			
			break;
			
		case ROI_Rectangle:
			
			ROICopy = (ROI_type*)init_Rect_type(ROI->ROIName, ((Rect_type*)ROI)->top, ((Rect_type*)ROI)->left,
												((Rect_type*)ROI)->height, ((Rect_type*)ROI)->width);
			
			break;
			
	}
	
	// copy ROI base class properties
	ROICopy->active = ROI->active;
	ROICopy->rgba.A = ROI->rgba.A;
	ROICopy->rgba.B = ROI->rgba.B;
	ROICopy->rgba.G = ROI->rgba.G;
	ROICopy->rgba.R = ROI->rgba.R;
	
	return ROICopy;
}

char* GetDefaultUniqueROIName (ListType ROIList)
{
	char*		newName			= NULL;
	BOOL		nameExists  	= FALSE;
	size_t 		nROIs 			= ListNumItems(ROIList);
	ROI_type*   ROI				= NULL;
	size_t		i				= 0;
	size_t		j				= 0;
	char		letters[]		= "abcdefghijklmnopqrstuvwxyz";
	
	
	do {
		
		// generate new name
		j = i;
		OKfree(newName);
		newName = StrDup("");
		do {
            if (newName[0])
                j--;
			AddStringPrefix(&newName, letters + j % 26, 1);
            j /= 26;
        } while (j > 0);
		
		// check if name exists
		nameExists = FALSE;
		for (size_t i = 1; i <= nROIs; i++) {
			ROI = *(ROI_type**) ListGetPtrToItem(ROIList, i);
			if (!strcmp(newName, ROI->ROIName)) {
				nameExists = TRUE;
				break;
			}
		}
		
		i++; // get another name
	} while (nameExists);
	
	
	return newName;
}


int SetROIName (ROI_type* ROI, char newName[])
{
	int		error = 0;
	
	OKfree(ROI->ROIName);
	
	if (newName)
		nullChk( ROI->ROIName = StrDup(newName) );
	
	return 0;
Error:
	
	return error;
}
