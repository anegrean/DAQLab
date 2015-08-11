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





//==============================================================================
// Constants

#define OKfree(ptr) 		if (ptr) {free(ptr); ptr = NULL;}
#define OKfreeList(list)	if (list) {ListDispose(list); list = 0;}  

//==============================================================================
// Types

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Callback group
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------

struct CallbackGroup {
	void*						objectRef;				// Reference to object owing the callback group.
	size_t 						nCBs;					// Number of callbacks to be called.
	CallbackFptr_type* 			CBs;					// Callback array called sequencially.
	void** 						CBsData;				// Array of callback data assigned to each callback function.
	DiscardFptr_type* 			discardCBsData;   		// Array of callback data discard functions to automatically dispose of the callback data when the callback group is disposed. 
																// If a function is NULL, then data is not disposed of automatically when disposing of the callback group.
};

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Images
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------
struct Image { 
	ImageTypes					imageType;				// pixel data type.
	int							imgHeight;				// image height in [pix].
	int							imgWidth;				// image width in [pix].
	void*						pixData;				// pixel array of imageType data type.
	double						pixSize;				// image pixel size in [um].
	double						imgTopLeftXCoord;		// image top-left corner X-Axis coordinates in [um].
	double						imgTopLeftYCoord;		// image top-left corner Y-Axis coordinates in [um].
	double						imgZCoord;				// image z-axis (height) location in [um].
	ListType					ROIs;					// list of ROIs added to the image of ROI_type*.
	ImageDisplayTransforms		dispTransformFunc;		// function to use for mapping pixel values to display.
};


//---------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Waveforms
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------

struct Waveform {
	WaveformTypes				waveformType;			// Waveform data type.
	WaveformColors				color;					// Color used to display waveform. WaveformColor_BLUE by default.
	char*						waveformName;			// Name of signal represented by the waveform. 
	char*						unitName;				// Physical SI unit such as V, A, Ohm, etc.
	double						dateTimestamp;			// Number of seconds since midnight, January 1, 1900 in the local time zone.
	double						samplingRate;			// Sampling rate in [Hz]. If 0, sampling rate is not given.
	size_t						nSamples;				// Number of samples in the waveform.
	void*						data;					// Array of waveformType elements.
};

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Repeated Waveforms
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------

struct RepeatedWaveform {
	RepeatedWaveformTypes		waveformType;			// Waveform data type. 
	WaveformColors				color;					// Color used to display waveform. WaveformColor_BLUE by default.
	char*						waveformName;			// Name of signal represented by the waveform.
	char*						unitName;				// Physical SI unit such as V, A, Ohm, etc.
	double						dateTimestamp;			// Number of seconds since midnight, January 1, 1900 in the local time zone.  
	double						samplingRate;			// Sampling rate in [Hz]. If 0, sampling rate is not given.
	double						repeat;					// number of times to repeat the waveform.
	size_t						nSamples;				// Number of samples in the waveform.
	void*						data;					// Array of waveformType elements. 
};

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Pulse trains
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------

// Parent class
struct PulseTrain {
	PulseTrainTimingTypes		pulseType;				// Timing type.  
	PulseTrainIdleStates		idleState;				// Counter output idle state.
	PulseTrainModes				mode;					// Generation mode.
	uInt64						nPulses;				// Number of pulses.
};

// Pulse train defined by frequency
struct PulseTrainFreqTiming {
	PulseTrain_type				baseClass;
	double 						frequency;				// Pulse frequencu in [Hz].
	double 						dutyCycle;				// Width of the pulse divided by the period in [%].  
	double						initialDelay;    		// Initial delay in [s].
};

// Pulse train defined by pulse duration
struct PulseTrainTimeTiming {
	PulseTrain_type				baseClass; 
	double 						highTime;   			// The time the pulse stays high in [s].
	double 						lowTime;    			// The time the pulse stays low in [s].
	double						initialDelay;    		// Initial delay in [s].
};

// Pulse train defined by number of ticks
struct PulseTrainTickTiming {
	PulseTrain_type				baseClass; 
	uInt32 						highTicks;				// Number of ticks for high state.
	uInt32 						lowTicks;				// Number of ticks for low state.
	uInt32 						delayTicks;				// Number of delay ticks after receiving a start trigger.
};


//==============================================================================
// Static Functions

	// Pulse Train
static void 				init_PulseTrain_type 					(PulseTrain_type* pulseTrain, PulseTrainTimingTypes pulseType, 
																	 PulseTrainModes mode, PulseTrainIdleStates idleState, uInt64 nPulses);

	// ROI base class
static void 				init_ROI_type 							(ROI_type* ROI, ROITypes ROIType, char ROIName[], RGBA_type color, BOOL active, CopyFptr_type copyFptr, DiscardFptr_type discardFptr);

static void					discard_ROIBaseClass					(ROI_type** ROIPtr);

	// Point ROI
static Point_type* 			copy_Point_type 						(Point_type* point);

	// Rectangle ROI
static Rect_type*			copy_Rect_type							(Rect_type* rect);


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

void discard_PulseTrain_type (PulseTrain_type** pulseTrainPtr)
{
	if (!*pulseTrainPtr) return;
	
	OKfree(*pulseTrainPtr);
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
	waveform->color				= WaveformColor_BLUE;
	waveform->samplingRate 		= samplingRate;
	waveform->waveformName		= NULL;
	waveform->unitName			= NULL;
	waveform->dateTimestamp		= 0;
	waveform->nSamples			= nSamples;
	
	// by default add timestamp at point of waveform creation
	if (GetCurrentDateTime(&waveform->dateTimestamp) <0) {
		free(waveform);
		return NULL;
	}
	
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

void ClearWaveformList (ListType waveformList)
{
	size_t				nWaveforms 	= ListNumItems(waveformList);
	Waveform_type**		waveformPtr = NULL;
	
	for (size_t i = 1; i <= nWaveforms; i++) {
		waveformPtr = ListGetPtrToItem(waveformList, i);
		discard_Waveform_type(waveformPtr);
	}
	ListClear(waveformList);
}

void DiscardWaveformList (ListType* waveformListPtr)
{
	ClearWaveformList(*waveformListPtr);
	OKfreeList(*waveformListPtr);
}

void SetWaveformName (Waveform_type* waveform, char waveformName[])
{
	waveform->waveformName = StrDup(waveformName);
}

void SetWaveformColor (Waveform_type* waveform, WaveformColors color)
{
	waveform->color = color;
}

WaveformColors GetWaveformColor (Waveform_type* waveform)
{
	return waveform->color;
}

void SetWaveformPhysicalUnit (Waveform_type* waveform, char unitName[])
{
	waveform->unitName = StrDup(unitName); 
}

char* GetWaveformPhysicalUnit (Waveform_type* waveform)
{
	 return StrDup(waveform->unitName); 
}

char* GetWaveformName (Waveform_type* waveform)
{
	 return StrDup(waveform->waveformName); 
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
			
		case Waveform_Int64:
		case Waveform_UInt64:
			dataTypeSize = sizeof(int64);
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

int CopyWaveform (Waveform_type** waveformCopy, Waveform_type* waveform, char** errorMsg)
{
INIT_ERR

	void*	nullData	= NULL;
	
	nullChk( *waveformCopy = init_Waveform_type(waveform->waveformType, waveform->samplingRate, 0, &nullData) );
	
	// copy waveform attributes
	(*waveformCopy)->color			= waveform->color;
	(*waveformCopy)->waveformName 	= StrDup(waveform->waveformName);
	(*waveformCopy)->unitName		= StrDup(waveform->unitName);
	(*waveformCopy)->dateTimestamp	= waveform->dateTimestamp;
	
	errChk( AppendWaveform(*waveformCopy, waveform, &errorInfo.errMsg) );
	
	return 0;
	
Error:
	
	// cleanup
	discard_Waveform_type(waveformCopy);
	
RETURN_ERR
}

int AppendWaveform (Waveform_type* waveformToAppendTo, Waveform_type* waveformToAppend, char** errorMsg) 
{
#define AppendWaveformData_Err_SamplingRatesAreDifferent		-1
#define AppendWaveformData_Err_DataTypesAreDifferent			-2
#define AppendWaveformData_Err_UnitsAreDifferent				-3  
	
INIT_ERR
	
	void* 	dataBuffer		= NULL;

	// do nothing if there is nothing to be copied
	if (!waveformToAppend) return 0;
	if (!waveformToAppend->nSamples) return 0;
	// check if sampling rates are the same
	if (waveformToAppendTo->samplingRate != waveformToAppend->samplingRate)
		SET_ERR(AppendWaveformData_Err_SamplingRatesAreDifferent, "Waveform sampling rates are different.");
		
	// check if data types are the same
	if (waveformToAppendTo->waveformType != waveformToAppend->waveformType)
		SET_ERR(AppendWaveformData_Err_DataTypesAreDifferent, "Waveform data types are different.");
	
	// check if units are the same
	if (waveformToAppendTo->unitName && waveformToAppend->unitName)
		if (strcmp(waveformToAppendTo->unitName, waveformToAppend->unitName))
			SET_ERR(AppendWaveformData_Err_UnitsAreDifferent, "Waveform units must be the same.");
		
	if (waveformToAppendTo->unitName || waveformToAppend->unitName)
		SET_ERR(AppendWaveformData_Err_UnitsAreDifferent, "Waveform units must be the same.");
	
	nullChk( dataBuffer = realloc(waveformToAppendTo->data, (waveformToAppendTo->nSamples + waveformToAppend->nSamples) * GetWaveformSizeofData(waveformToAppendTo)) );
	
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
			
		case Waveform_Int64:
		case Waveform_UInt64:
			memcpy(((int64*)dataBuffer) + waveformToAppendTo->nSamples, waveformToAppend->data, waveformToAppend->nSamples * sizeof(int64)); 
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
	
Error:
	
RETURN_ERR
}

int IntegrateWaveform (Waveform_type** waveformOut, Waveform_type* waveformIn, size_t startIdx, size_t endIdx, size_t nInt, char** errorMsg)
{
#define IntegrateWaveform_Err_NInt			-1		// nInt is 0
#define IntegrateWaveform_Err_Selection		-2		// (startIdx >= endIdx) and (endIdx != 0)
#define IntegrateWaveform_Err_OutOfBounds	-3		// selection is out of waveform bounds

	// macro to integrate waveforms of different data type
#define IntegrateWaveformType(DataType)												\
	{	DataType* dataIn = (DataType*)waveformIn->data;								\
		nullChk( dataOutDouble = calloc(nDataOutSamples, sizeof(double)) );			\
		for (size_t i = 0; i < nDataOutSamples; i++)								\
			for (size_t j = startIdx + i*nInt; j < startIdx + (i+1)*nInt; j++)		\
				dataOutDouble[i] += dataIn[j];										\
		nullChk( *waveformOut = init_Waveform_type(Waveform_Double, waveformIn->samplingRate/nInt, nDataOutSamples, (void**)&dataOutDouble) );}
	
INIT_ERR

	size_t 		nDataOutSamples		= 0;
	double*		dataOutDouble		= NULL;
	
	// check if nInt != 0
	if (!nInt) 
		SET_ERR(IntegrateWaveform_Err_NInt, "Number of point to integrate must be greater than 0.");
	
	// check if endIdx > startIdx unless endIdx = 0
	if ((startIdx >= endIdx) && endIdx) 
		SET_ERR(IntegrateWaveform_Err_Selection, "Start index must be greater or equal than End index if End index is greater than 0.");
	
	// selection waveform bounds
	if ( (startIdx > waveformIn->nSamples - 1) || (endIdx > waveformIn->nSamples - 1)) 
		SET_ERR(IntegrateWaveform_Err_OutOfBounds, "Selection is outside of input waveform bounds.");
	
	if (endIdx)
		nDataOutSamples = (endIdx - startIdx + 1) / nInt;
	else
		nDataOutSamples = (waveformIn->nSamples - startIdx) / nInt;
	
	// generate empty waveform
	if (!nDataOutSamples) {
		void*	nullData = NULL;
		nullChk( *waveformOut = init_Waveform_type(waveformIn->waveformType, waveformIn->samplingRate/nInt, 0, &nullData) );
		return 0;
	}
	
	switch (waveformIn->waveformType) {
			
		case Waveform_Char:
			IntegrateWaveformType(char);
			break;
			
		case Waveform_UChar:
			IntegrateWaveformType(unsigned char);
			break;
			
		case Waveform_Short:
			IntegrateWaveformType(short);
			break;
			
		case Waveform_UShort:
			IntegrateWaveformType(unsigned short);
			break;
			
		case Waveform_Int:
			IntegrateWaveformType(int);
			break;
			
		case Waveform_UInt:
			IntegrateWaveformType(unsigned int);
			break;
			
		case Waveform_Int64:
			IntegrateWaveformType(int64);
			break;
			
		case Waveform_UInt64:
			IntegrateWaveformType(uInt64);
			break;
		
		case Waveform_SSize:
			IntegrateWaveformType(ssize_t);
			break;
			
		case Waveform_Size:
			IntegrateWaveformType(size_t);
			break;
			
		case Waveform_Float:
			IntegrateWaveformType(float);
			break;
			
		case Waveform_Double:
			IntegrateWaveformType(double);
			break;
	}
	
	return 0;
	
Error:
	
	// cleanup
	OKfree(dataOutDouble);
	
RETURN_ERR
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Repeated Waveforms
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------

RepeatedWaveform_type* init_RepeatedWaveform_type (RepeatedWaveformTypes waveformType, double samplingRate, size_t nSamples, void** ptrToData, double repeat)
{
	RepeatedWaveform_type* waveform = malloc (sizeof(RepeatedWaveform_type));
	if (!waveform) return NULL;
	
	waveform->waveformType 		= waveformType;
	waveform->color				= WaveformColor_BLUE;
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
			
		case Waveform_Int64:
			repWaveformType = RepeatedWaveform_Int64;
			break;
			
		case Waveform_UInt64:
			repWaveformType = RepeatedWaveform_UInt64;
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

void discard_RepeatedWaveform_type (RepeatedWaveform_type** waveformPtr)
{
	if (!*waveformPtr) return;
	
	// discard data
	OKfree((*waveformPtr)->data);
	OKfree((*waveformPtr)->waveformName);
	OKfree((*waveformPtr)->unitName);
	
	OKfree(*waveformPtr);
}

void SetRepeatedWaveformColor (RepeatedWaveform_type* waveform, WaveformColors color)
{
	waveform->color = color;
}

WaveformColors GetrepeatedWaveformColor (RepeatedWaveform_type* waveform)
{
	return waveform->color;
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
			
		case RepeatedWaveform_Int64:
		case RepeatedWaveform_UInt64:
			dataTypeSize = sizeof(int64);
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




//---------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Images
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------

Image_type* init_Image_type (ImageTypes imageType, int imgHeight, int imgWidth, void** imgDataPtr)
{
INIT_ERR

	Image_type*		image 		= malloc(sizeof(Image_type));
	
	if (!image) return NULL;
	
	//-----------------------------------------------------------
	// init
	image->imageType					= imageType;						// imagetype
	image->imgHeight					= imgHeight;						// image height in [pix]. 
	image->imgWidth						= imgWidth;							// image width in [pix].
	image->pixData						= *imgDataPtr;						// assign data
	*imgDataPtr							= NULL;								// consume image data
	image->imgTopLeftXCoord				= 0.0;								// image top-left corner X-Axis coordinates in [um].    
	image->imgTopLeftYCoord				= 0.0;								// image top-left corner Y-Axis coordinates in [um]. 
	image->imgZCoord					= 0.0;								// image z-axis (height) location in [um].    
	image->ROIs							= 0;								// ROI list
	image->dispTransformFunc			= ImageDisplayTransform_Linear;		// used to map pixels to the display.
	
	//-----------------------------------------------------------
	// alloc
	nullChk( image->ROIs		= ListCreate(sizeof(ROI_type*)));
	
	return image;
	
Error:
	
	OKfreeList(image->ROIs);
	free(image);
	return NULL;
}

void discard_Image_type (Image_type** imagePtr)
{
	Image_type*		image 	= *imagePtr;
	
	if (!image) return;
	
	// free image data
	OKfree(image->pixData);
	
	// free ROIs
	DiscardROIList(&image->ROIs);

	OKfree(*imagePtr);
}

Image_type* copy_Image_type(Image_type* imgSource)
{
INIT_ERR

	size_t			imgNBytes   = imgSource->imgHeight * imgSource->imgWidth * GetImageSizeofData(imgSource);
	Image_type*		imgCopy		= NULL;
	void*			imgDataCopy = NULL;
	
	// copy image data
	nullChk( imgDataCopy = malloc(imgNBytes) );
	memcpy(imgDataCopy, imgSource->pixData, imgNBytes);
	
	nullChk( imgCopy = init_Image_type(imgSource->imageType, imgSource->imgHeight, imgSource->imgWidth, &imgDataCopy) ); 
	
	imgCopy->pixSize			= imgSource->pixSize;
	imgCopy->imgTopLeftXCoord	= imgSource->imgTopLeftXCoord;		       
	imgCopy->imgTopLeftYCoord	= imgSource->imgTopLeftYCoord;		    
	imgCopy->imgZCoord			= imgSource->imgZCoord;
	
	// copy ROIs
	size_t 		nROIs 	= ListNumItems(imgSource->ROIs);
	ROI_type*	ROI		= NULL;
	ROI_type*   ROICopy	= NULL;
	for (size_t i = 1; i <= nROIs; i++) {
		ROI = *(ROI_type**)ListGetPtrToItem(imgSource->ROIs, i);
		nullChk( ROICopy =  (ROI_type*) (*ROI->copyFptr) ((void*)ROI) );
		ListInsertItem(imgCopy->ROIs, &ROICopy, END_OF_LIST);
	}
	
	return imgCopy;
	
Error:
	
	OKfree(imgDataCopy);
	discard_Image_type(&imgCopy);
	return NULL;
}

void GetImageSize (Image_type* image, int* widthPtr, int* heightPtr)
{
	if (widthPtr)
		*widthPtr = image->imgWidth;
	
	if (heightPtr)
		*heightPtr = image->imgHeight;
}

void SetImagePixSize (Image_type* image, double pixSize)
{
	image->pixSize = pixSize; 
}

double GetImagePixSize (Image_type* image)
{
	 return image->pixSize; 
}

void SetImageTopLeftXCoord (Image_type* image, double imgTopLeftXCoord)
{
	image->imgTopLeftXCoord = imgTopLeftXCoord; 
}

void SetImageTopLeftYCoord (Image_type* image, double imgTopLeftYCoord)
{
	image->imgTopLeftYCoord = imgTopLeftYCoord; 
}

void SetImageZCoord (Image_type* image, double imgZCoord)
{
	image->imgZCoord = imgZCoord; 
}

void SetImageCoord (Image_type* image, double imgTopLeftXCoord, double imgTopLeftYCoord, double imgZCoord)
{
	image->imgTopLeftXCoord = imgTopLeftXCoord;
	image->imgTopLeftYCoord = imgTopLeftYCoord;
	image->imgZCoord 		= imgZCoord;
}

void GetImageCoordinates (Image_type* image, double* imgTopLeftXCoordPtr, double* imgTopLeftYCoordPtr, double* imgZCoordPtr)
{
	if (imgTopLeftXCoordPtr)
		*imgTopLeftXCoordPtr = image->imgTopLeftXCoord;
	
	if (imgTopLeftYCoordPtr)
		*imgTopLeftYCoordPtr = image->imgTopLeftYCoord;
	
	if (imgZCoordPtr)
		*imgZCoordPtr = image->imgZCoord;
}

void* GetImagePixelArray (Image_type* image)
{
	 return image->pixData; 
}	

ImageTypes GetImageType (Image_type* image)
{
	 return image->imageType; 
}

void SetImageROIs (Image_type* image, ListType	ROIs)
{
	// free previous ROIs
	DiscardROIList(&image->ROIs);
	image->ROIs = ROIs; 
}

ListType GetImageROIs (Image_type* image)
{
	 return image->ROIs; 
}

void SetImageDisplayTransform (Image_type* image, ImageDisplayTransforms dispTransformFunc)
{
	image->dispTransformFunc = dispTransformFunc;
}

ImageDisplayTransforms GetImageDisplayTransform (Image_type* image)
{
	return image->dispTransformFunc;
}

size_t GetImageSizeofData (Image_type* image)
{
	size_t dataTypeSize = 0;
	
	switch (image->imageType) {
			
		case Image_UChar:					// 8 bit unsigned
		  	dataTypeSize = sizeof(char);
			break;
	
		case Image_UShort:					// 16 bit unsigned   
		case Image_Short:					// 16 bit signed   
		   dataTypeSize = sizeof(short int);
			break;
			
		case Image_UInt:					// 32 bit unsigned
		case Image_Int:						// 32 bit signed
			dataTypeSize = sizeof(int);
			break;
			
		case Image_Float:					// 32 bit float   :
			dataTypeSize = sizeof(float);
			break;
			
		case Image_RGBA:
			dataTypeSize = sizeof (RGBA_type);
			break;
			
		case Image_RGBAU64:
			dataTypeSize = sizeof (RGBAU64_type);
			break;
			
	}
	
	return dataTypeSize;
}


//-------------------------
// ROI
//-------------------------

static void init_ROI_type (ROI_type* ROI, ROITypes ROIType, char ROIName[], RGBA_type color, BOOL active, CopyFptr_type copyFptr, DiscardFptr_type discardFptr)
{
	ROI->ROIType 					= ROIType;
	ROI->ROIName					= StrDup(ROIName);
	ROI->active						= active;
	
	// init color to opaque black
	ROI->rgba.R						= color.R;
	ROI->rgba.G						= color.G;
	ROI->rgba.B						= color.B;
	ROI->rgba.alpha					= color.alpha;
	
	ROI->textBackground.R			= 0;
	ROI->textBackground.G			= 0;	
	ROI->textBackground.B			= 0;	
	ROI->textBackground.alpha		= 255;	// transparent
	ROI->textFontSize				= 12;
	
	// METHODS
	ROI->copyFptr		= copyFptr;
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
Point_type* initalloc_Point_type (Point_type* point, char ROIName[], RGBA_type color, BOOL active, int x, int y)
{
	if (!point) {
		point = malloc(sizeof(Point_type));
		if (!point) return NULL;
	}
	
	// init base class
	init_ROI_type(&point->baseClass, ROI_Point, ROIName, color, active, (CopyFptr_type)copy_Point_type, (DiscardFptr_type) discard_Point_type); 
	
	// init point child class
	point->x	= x;
	point->y	= y;
	
	return point;
}
			  
void discard_Point_type (Point_type** PointPtr)
{
	if (!*PointPtr) return;
	
	// discard Point specific data
	
	// discard base class data
	discard_ROIBaseClass((ROI_type**)PointPtr);
}

static Point_type* copy_Point_type (Point_type* point)
{
INIT_ERR

	Point_type*		pointCopy 	= malloc (sizeof(Point_type));
	if (!pointCopy) return NULL;
	
	// shallow copy
	memcpy(pointCopy, point, sizeof(Point_type));
	
	// deep copy
	nullChk( pointCopy->baseClass.ROIName = StrDup(point->baseClass.ROIName) );
	
	return pointCopy;
	
Error:
	
	free(pointCopy);
	return NULL;
}

//-------------------------
// Rectangle
//-------------------------
	
Rect_type* initalloc_Rect_type (Rect_type* rect, char ROIName[], RGBA_type color, BOOL active, int top, int left, int height, int width)
{
	if (!rect) {
		rect = malloc(sizeof(Rect_type));
		if (!rect) return NULL;
	}
	
	// init base class
	init_ROI_type(&rect->baseClass, ROI_Rectangle, ROIName, color, active, (CopyFptr_type)copy_Rect_type, (DiscardFptr_type)discard_Rect_type); 
	
	// init rectangle child class
	rect->top 		= top;
	rect->left 		= left;
	rect->height	= height;
	rect->width		= width;
	
	return rect;
}

void discard_Rect_type (Rect_type** RectPtr)
{
	if (!*RectPtr) return;
	
	// discard Rectangle specific data		 
	
	// discard base class data
	discard_ROIBaseClass((ROI_type**)RectPtr);
}

static Rect_type* copy_Rect_type (Rect_type* rect)
{
INIT_ERR

	Rect_type*		rectCopy 	= malloc (sizeof(Rect_type));
	if (!rectCopy) return NULL;
	
	// shallow copy
	memcpy(rectCopy, rect, sizeof(Rect_type));
	
	// deep copy
	nullChk( rectCopy->baseClass.ROIName = StrDup(rect->baseClass.ROIName) );
	
	return rectCopy;
	
Error:
	
	free(rectCopy);
	return NULL;
}

//-------------------------     
// All ROIs
//-------------------------     

void DiscardROIList (ListType* ROIListPtr)
{
	ListType	ROIList = *ROIListPtr;
	
	if (!ROIList) return;
	
	// free ROIs
	size_t 			nROIs 	= ListNumItems(ROIList);
	ROI_type** 		ROIPtr	= NULL;
	for (size_t i = 1; i <= nROIs; i++) {
		ROIPtr = ListGetPtrToItem(ROIList, i);
		(*(*ROIPtr)->discardFptr) ((void**)ROIPtr);
	}
	
	OKfreeList(*ROIListPtr);
}

ListType CopyROIList (ListType ROIList)
{
INIT_ERR

	ListType		listCopy 	= 0;
	size_t 			nROIs 		= ListNumItems(ROIList);
	ROI_type* 		ROI			= NULL;
	ROI_type*		ROICopy		= NULL;
	
	nullChk( listCopy = ListCreate(sizeof(ROI_type*)) );
	
	for (size_t i = 1; i <= nROIs; i++) {
		ROI = *(ROI_type**) ListGetPtrToItem(ROIList, i);
		nullChk( ROICopy = (ROI_type*) (*ROI->copyFptr) ((void*)ROI) );
		ListInsertItem(listCopy, &ROICopy, END_OF_LIST);
	}
	
	return listCopy;
	
Error:
	
	DiscardROIList(&listCopy);
	return 0;
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
		for (size_t k = 1; k <= nROIs; k++) {
			ROI = *(ROI_type**) ListGetPtrToItem(ROIList, k);
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
INIT_ERR
	
	OKfree(ROI->ROIName);
	
	if (newName)
		nullChk( ROI->ROIName = StrDup(newName) );
	

Error:
	
	return errorInfo.error;
}

CallbackGroup_type* init_CallbackGroup_type	(void* objectRef, size_t nCallbackFunctions, CallbackFptr_type* callbackFunctions, void** callbackData, DiscardFptr_type* discardCallbackDataFunctions)
{
INIT_ERR

	CallbackGroup_type*		cbg 	= malloc(sizeof(CallbackGroup_type));
	if (!cbg) return NULL;
	
	// init
	cbg->objectRef		= objectRef;
	cbg->nCBs 			= nCallbackFunctions;
	cbg->CBs			= NULL;
	cbg->CBsData		= NULL;
	cbg->discardCBsData = NULL;
	
	// alloc
	nullChk( cbg->CBs 				= malloc (nCallbackFunctions * sizeof(CallbackFptr_type)) );
	nullChk( cbg->CBsData   		= malloc (nCallbackFunctions * sizeof(void*)) );
	nullChk( cbg->discardCBsData	= malloc (nCallbackFunctions * sizeof(DiscardFptr_type)) );
	
	for (size_t i = 0; i < nCallbackFunctions; i++) {
		cbg->CBs[i] 				= callbackFunctions[i];
		cbg->CBsData[i]				= callbackData[i];
		cbg->discardCBsData[i] 		= discardCallbackDataFunctions[i];
	}
	
	return cbg;
	
Error:
	
	OKfree(cbg->CBs);
	OKfree(cbg->CBsData);
	OKfree(cbg->discardCBsData);
	
	free(cbg);
	return NULL;
}

void discard_CallbackGroup_type (CallbackGroup_type** callbackGroupPtr)
{
	CallbackGroup_type*		cbg = *callbackGroupPtr;
	if (!cbg) return;
	
	// discard restore settings callback data
	for (size_t i = 0; i < cbg->nCBs; i++)
		if (cbg->discardCBsData[i])
			(*cbg->discardCBsData[i]) (&cbg->CBsData[i]);	
	
	OKfree(cbg->CBs);
	OKfree(cbg->CBsData); 
	OKfree(cbg->discardCBsData);

	OKfree(*callbackGroupPtr);
}

void FireCallbackGroup (CallbackGroup_type* callbackGroup, int event)
{
	if (!callbackGroup) return;
	
	for (size_t i = 0; i < callbackGroup->nCBs; i++)
		if (callbackGroup->CBs[i]) 
			(*callbackGroup->CBs[i]) (callbackGroup->objectRef, event, callbackGroup->CBsData[i]);
	
}
