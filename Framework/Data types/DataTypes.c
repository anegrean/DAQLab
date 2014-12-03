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
#define DAQmxDefault_CO_Frequency_Task_freq				1000000   		// frequency
#define DAQmxDefault_CO_Frequency_Task_dutycycle    	50.0			// duty cycle
#define DAQmxDefault_CO_Frequency_Task_hightime			0.0005   		// the time the pulse stays high [s]
#define DAQmxDefault_CO_Frequency_Task_lowtime     		0.0005			// the time the pulse stays low [s]
#define DAQmxDefault_CO_Frequency_Task_highticks		10   			// the ticks the pulse stays high [s]
#define DAQmxDefault_CO_Frequency_Task_lowticks     	2				// the ticks the pulse stays low [s]	
#define DAQmxDefault_CO_Frequency_Task_initdelay 		0.0			    // initial delay [s]
#define DAQmxDefault_CO_Frequency_Task_initdelayticks 	0			    // initial delay [ticks]  
#define DAQmxDefault_CO_Frequency_Task_idlestate    	DAQmx_Val_Low   // terminal state at rest, pulses switch this to the oposite state
#define DAQmxDefault_CO_Frequency_Task_timeout      	10.0            // timeout  
#define DAQmxDefault_CO_Frequency_Task_npulses      	1000            // number of finite pulses     	



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




struct PulseTrainFreqTiming {
	double 					frequency;   				// 
	double 					dutycycle;   
	double					initialdelay;    		   // initial delay [s] 
} ;



struct PulseTrainTimeTiming {
	double 					hightime;   				// the time the pulse stays high [s]
	double 					lowtime;    				// the time the pulse stays low [s]
	double					initialdelay;    		    // initial delay [s]
};



struct PulseTrainTickTiming {
	int 					highticks;
	int 					lowticks;
	int 					delayticks;
};




struct PulseTrain {
	int							idlestate;
	PulseTrainTimingTypes		type;				// timing type. 
	void*						timing;   			//pointer to PulseTrainFreqTiming_type,PulseTrainTimeTiming_type or PulseTrainTickTiming_type
	int							mode;				// measurement mode
	size_t						npulses;			// number of pulses
};


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
			freqtiming -> initialdelay		= DAQmxDefault_CO_Frequency_Task_initdelay;      
			pulsetrain->timing=freqtiming;
			break;
		case PulseTrain_Time:
			timetiming=malloc(sizeof(PulseTrainTimeTiming_type));  
			timetiming->hightime		= DAQmxDefault_CO_Frequency_Task_hightime;
			timetiming->lowtime			= DAQmxDefault_CO_Frequency_Task_lowtime;
			timetiming -> initialdelay		= DAQmxDefault_CO_Frequency_Task_initdelay;      
			pulsetrain->timing=timetiming; 
			break;
		case PulseTrain_Ticks:
			ticktiming=malloc(sizeof(PulseTrainTickTiming_type));   
			ticktiming->highticks		= DAQmxDefault_CO_Frequency_Task_highticks;
			ticktiming->lowticks		= DAQmxDefault_CO_Frequency_Task_lowticks; 
			ticktiming -> delayticks	= DAQmxDefault_CO_Frequency_Task_initdelayticks;      
			pulsetrain->timing=ticktiming; 
			break;
	}
	
	pulsetrain -> idlestate			= DAQmxDefault_CO_Frequency_Task_idlestate;
	pulsetrain -> mode				= mode;    
	pulsetrain -> npulses			= DAQmxDefault_CO_Frequency_Task_npulses; 

	return pulsetrain;
}



void discard_PulseTrain (PulseTrain_type* pulsetrain)
{
	if (!pulsetrain) return;
	
	// discard pulsetrain
	OKfree(pulsetrain);
}

void* GetPulseTrainTiming(PulseTrain_type* pulsetrain) 
{
	 return pulsetrain->timing;
}

//sets the pulsetrain idle state
void   SetPulseTrainIdleState(PulseTrain_type* pulsetrain,int idlestate)
{
	pulsetrain->idlestate=idlestate;
}

//gets the pulsetrain idle state
int   GetPulseTrainIdleState(PulseTrain_type* pulsetrain)
{
	return pulsetrain->idlestate;
}

//sets the pulsetrain amount of pulses
void   SetPulseTrainNPulses(PulseTrain_type* pulsetrain,int npulses)
{
	pulsetrain->npulses=npulses;	
}

//gets the pulsetrain amount of pulses
int   GetPulseTrainNPulses(PulseTrain_type* pulsetrain)
{
	return pulsetrain->npulses;	
}

//sets the pulsetrain mode
void   SetPulseTrainMode(PulseTrain_type* pulsetrain,int mode)
{
	pulsetrain->mode=mode;	
}

////gets the pulsetrain type
int   GetPulseTrainType(PulseTrain_type* pulsetrain)
{
	return pulsetrain->type;	
}

//sets the pulsetrain type
void   SetPulseTrainType(PulseTrain_type* pulsetrain,int type)
{
	pulsetrain->type=type;	
}

//gets the pulsetrain mode
int   GetPulseTrainMode(PulseTrain_type* pulsetrain)
{
	return pulsetrain->mode;	
}


//gets the pulsetrain frequency
double GetPulseTrainFreq(PulseTrainFreqTiming_type* freqtiming)
{
	  return freqtiming->frequency;
}

//sets the pulsetrain frequency
void   SetPulseTrainFreq(PulseTrainFreqTiming_type* freqtiming,double freq)
{
	freqtiming->frequency=freq;
}

//sets the pulsetrain duty cycle
void   SetPulseTrainDutyCycle(PulseTrainFreqTiming_type* freqtiming,double dutycycle)
{
	freqtiming->dutycycle=dutycycle;
}

//sets the pulsetrain duty cycle
double   GetPulseTrainDutyCycle(PulseTrainFreqTiming_type* freqtiming)
{
	return freqtiming->dutycycle;
}



//sets the pulsetrain duty cycle
void   SetPulseTrainDelay(PulseTrainFreqTiming_type* freqtiming,double delay)
{
	freqtiming->initialdelay=delay;
}

//gets the pulsetrain initial delay 
double GetPulseTrainDelay(PulseTrainFreqTiming_type* freqtiming)
{
	 return freqtiming->initialdelay;
}

//sets the pulsetrain highticks
void   SetPulseTrainHighTicks(PulseTrainTickTiming_type* ticktiming,int ticks)
{
	ticktiming->highticks=ticks;
}

//gets the pulsetrain highticks
int   GetPulseTrainHighTicks(PulseTrainTickTiming_type* ticktiming)
{
	 return ticktiming->highticks;
}

//sets the pulsetrain lowticks
void   SetPulseTrainLowTicks(PulseTrainTickTiming_type* ticktiming,int ticks)
{
	ticktiming->lowticks=ticks;
}

//gets the pulsetrain lowticks
int   GetPulseTrainLowTicks(PulseTrainTickTiming_type* ticktiming)
{
	return ticktiming->lowticks;
}

//sets the pulsetrain delayticks
void   SetPulseTrainDelayTicks(PulseTrainTickTiming_type* ticktiming,int ticks)
{
	ticktiming->delayticks=ticks;
}

//gets the pulsetrain delayticks
int   GetPulseTrainDelayTicks(PulseTrainTickTiming_type* ticktiming)
{
		return ticktiming->delayticks;
}

//sets the pulsetrain hightime
void   SetPulseTrainHighTime(PulseTrainTimeTiming_type* timetiming,double time)
{
	timetiming->hightime=time;
}

//gets the pulsetrain hightime
double   GetPulseTrainHighTime(PulseTrainTimeTiming_type* timetiming)
{
	 return timetiming->hightime;
}

//sets the pulsetrain lowtime
void   SetPulseTrainLowTime(PulseTrainTimeTiming_type* timetiming,double time)
{
	timetiming->lowtime=time;
}

//gets the pulsetrain hightime
double   GetPulseTrainLowTime(PulseTrainTimeTiming_type* timetiming)
{
	 return timetiming->lowtime;
}

//sets the pulsetrain initialdelay
void   SetPulseTrainDelayTime(PulseTrainTimeTiming_type* timetiming,double time)
{
	timetiming->initialdelay=time;
}

//gets the pulsetrain delaytime
double   GetPulseTrainDelayTime(PulseTrainTimeTiming_type* timetiming)
{
	return timetiming->initialdelay;	
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

Waveform_type* CopyWaveform (Waveform_type* waveform)
{
	Waveform_type*	waveformCopy = init_Waveform_type(waveform->waveformType, waveform->samplingRate, 0, NULL);
	if (!waveformCopy) return NULL;
	
	if (AppendWaveform(waveformCopy, waveform) < 0) {
		discard_Waveform_type(&waveformCopy);
		return NULL;
	}
	
	return waveformCopy;
}

int AppendWaveform (Waveform_type* waveformToAppendTo, Waveform_type* waveformToAppend)
{
#define AppendWaveformData_Err_SamplingRatesAreDifferent		-1
#define AppendWaveformData_Err_DataTypesAreDifferent			-2
#define AppendWaveformData_Err_UnitsAreDifferent				-3  
#define AppendWaveformData_Err_OutOfMemory						-4
	
	// do nothing if there is nothing to be copied
	if (!waveformToAppend) return 0;
	if (!waveformToAppend->nSamples) return 0;
	// check if sampling rates are the same
	if (waveformToAppendTo->samplingRate != waveformToAppend->samplingRate) return AppendWaveformData_Err_SamplingRatesAreDifferent;
	
	// check if data types are the same
	if (waveformToAppendTo->waveformType != waveformToAppend->waveformType) return AppendWaveformData_Err_DataTypesAreDifferent;
	
	// check if units are the same
	if (waveformToAppendTo->unitName && waveformToAppend->unitName)
	if (strcmp(waveformToAppendTo->unitName, waveformToAppend->unitName)) return AppendWaveformData_Err_UnitsAreDifferent;
	
	
	void* dataBuffer = realloc(waveformToAppendTo->data, (waveformToAppendTo->nSamples + waveformToAppend->nSamples) * GetWaveformSizeofData(waveformToAppendTo));
	if (!dataBuffer) return AppendWaveformData_Err_OutOfMemory;
	
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
