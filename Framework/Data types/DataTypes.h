//==============================================================================
//
// Title:		DataTypes.h
// Purpose:		A short description of the interface.
//
// Created on:	10/8/2014 at 7:56:29 PM by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __DataType_H__
#define __DataType_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "cvidef.h"

//==============================================================================
// Constants

//==============================================================================
// Types
		
	//----------------------------------------------------------------------------------------------
	// Basic types
	//----------------------------------------------------------------------------------------------  

typedef enum {
	
	BasicData_Null,
	BasicData_Bool,							
	BasicData_Char,						// 8 bits	   			 			char 
	BasicData_UChar,					// 8 bits			 				unsigned char
	BasicData_Short,					// 16 bits							short
	BasicData_UShort,					// 16 bits							unsigned short 
	BasicData_Int,						// 32 bits							int    
	BasicData_UInt,						// 32 bits							unsigned int   							
	BasicData_Long,
	BasicData_ULong,
	BasicData_LongLong,
	BasicData_ULongLong,
	BasicData_Float,					// 16 bits							float  
	BasicData_Double,					// 32 bits							double
	BasicData_CString					// null-terminated string
	
} BasicDataTypes;
		
	//----------------------------------------------------------------------------------------------
	// Waveform types
	//----------------------------------------------------------------------------------------------  
		
typedef struct Waveform		Waveform_type;
		
typedef enum {
	
	Waveform_Char,						
	Waveform_UChar,						
	Waveform_Short,						
	Waveform_UShort,					
	Waveform_Int,						
	Waveform_UInt,						
	Waveform_SSize,						
	Waveform_Size,								   			
	Waveform_Float,						
	Waveform_Double					
	
} WaveformTypes;

	//----------------------------------------------------------------------------------------------
	// Repeated Waveform types
	//---------------------------------------------------------------------------------------------- 
typedef struct RepeatedWaveform		RepeatedWaveform_type;

typedef enum {
	
	RepeatedWaveform_Char,						
	RepeatedWaveform_UChar,						
	RepeatedWaveform_Short,						
	RepeatedWaveform_UShort,					
	RepeatedWaveform_Int,						
	RepeatedWaveform_UInt,						
	RepeatedWaveform_SSize,						
	RepeatedWaveform_Size,								   			
	RepeatedWaveform_Float,						
	RepeatedWaveform_Double					
	
} RepeatedWaveformTypes;

	//----------------------------------------------------------------------------------------------
	// Image types
	//----------------------------------------------------------------------------------------------   

typedef enum {
	
	Image_NIVision
	
} ImageTypes;

	//----------------------------------------------------------------------------------------------
	// Pulse train types
	//----------------------------------------------------------------------------------------------

typedef struct PulseTrain PulseTrain_type; 

typedef struct PulseTrainFreqTiming	PulseTrainFreqTiming_type;

struct PulseTrainFreqTiming {
	double 					frequency;   				// 
	double 					dutycycle;   
} ;

typedef struct PulseTrainTimeTiming	PulseTrainTimeTiming_type;  

struct PulseTrainTimeTiming {
	double 					hightime;   				// the time the pulse stays high [s]
	double 					lowtime;    				// the time the pulse stays low [s]
};

typedef struct PulseTrainTickTiming	PulseTrainTickTiming_type;  

struct PulseTrainTickTiming {
	int 					highticks;
	int 					lowticks;
};

typedef enum {
	PulseTrain_Freq,
	PulseTrain_Time,
	PulseTrain_Ticks
	
} PulseTrainTimingTypes;


struct PulseTrain {
	char*						chanName;
	double						timeout;
	int							idlestate;
	double						initialdelay;
	PulseTrainTimingTypes		type;				// timing type. 
	void*						timing;   			//pointer to PulseTrainFreqTiming_type,PulseTrainTimeTiming_type or PulseTrainTickTiming_type
	int							mode;				// measurement mode
	size_t						npulses;			// number of pulses
	char*						refclk_source;
	int							refclk_slope;
	char*						trigger_source;
	int							trigger_slope;
};

	





	
	//----------------------------------------------------------------------------------------------
	// All types
	//---------------------------------------------------------------------------------------------- 
	
typedef enum {
	//------------------------------------------------------
	// Atomic types
	//------------------------------------------------------  
	DL_Null,
	DL_Bool,							
	DL_Char,							// 8 bits	   			 			char 
	DL_UChar,							// 8 bits			 				unsigned char
	DL_Short,							// 16 bits							short
	DL_UShort,							// 16 bits							unsigned short 
	DL_Int,								// 32 bits							int    
	DL_UInt,							// 32 bits							unsigned int   							
	DL_Long,
	DL_ULong,
	DL_LongLong,
	DL_ULongLong,
	DL_Float,							// 16 bits							float  
	DL_Double,							// 32 bits							double 
	
	//------------------------------------------------------  
	// Composite types
	//------------------------------------------------------
	
	//---------------------
	// String
	//---------------------
	DL_CString,							// null-terminated string
	
	//---------------------
	// Waveforms
	//---------------------
	DL_Waveform_Char,						
	DL_Waveform_UChar,						
	DL_Waveform_Short,						
	DL_Waveform_UShort,					
	DL_Waveform_Int,						
	DL_Waveform_UInt,						
	DL_Waveform_SSize,						
	DL_Waveform_Size,								   			
	DL_Waveform_Float,						
	DL_Waveform_Double,
	
	//---------------------
	// Repeated Waveforms
	//---------------------
	DL_RepeatedWaveform_Char,						
	DL_RepeatedWaveform_UChar,						
	DL_RepeatedWaveform_Short,						
	DL_RepeatedWaveform_UShort,					
	DL_RepeatedWaveform_Int,						
	DL_RepeatedWaveform_UInt,						
	DL_RepeatedWaveform_SSize,						
	DL_RepeatedWaveform_Size,								   			
	DL_RepeatedWaveform_Float,						
	DL_RepeatedWaveform_Double,	
	
	//---------------------
	// Images
	//---------------------
	DL_Image_NIVision,
	
	//---------------------
	// Pulse Train
	//---------------------
	DL_PulseTrain			 
	
} DLDataTypes;


//==============================================================================
// External variables

//==============================================================================
// Global functions

//---------------------------------------------------------------------------------------------------------
// Waveform
//---------------------------------------------------------------------------------------------------------  
	// Creates a waveform container for void* basic data allocated with malloc.
Waveform_type*			init_Waveform_type						(WaveformTypes waveformType, double samplingRate, size_t nSamples, void* data);
	// Discards the waveform container and its data allocated with malloc.
void 					discard_Waveform_type 					(Waveform_type** waveform);
	// Sets waveform name
void					SetWaveformName							(Waveform_type* waveform, char waveformName[]);
	// Sets a physical unit name for the waveform
void					SetWaveformPhysicalUnit 				(Waveform_type* waveform, char unitName[]);
	// Adds current date timestamp to mark the beginning of the waveform
int						AddWaveformDateTimestamp				(Waveform_type* waveform);
	// Returns waveform timestamp
double					GetWaveformDateTimestamp				(Waveform_type* waveform); 
	
	// Returns number of bytes per waveform element.
size_t					GetWaveformSizeofData					(Waveform_type* waveform);
	// Returns number of samples in the waveform.
size_t					GetWaveformNumSamples					(Waveform_type* waveform);
	// Returns pointer to waveform data.
void* 					GetWaveformDataPtr 						(Waveform_type* waveform, size_t* nSamples);
	// Makes a waveform copy
Waveform_type*			CopyWaveform							(Waveform_type* waveform);
	// Appends data from one waveform to another. Sampling rate, data type and physical unit must be the same.
int 					AppendWaveform 							(Waveform_type* waveformToAppendTo, Waveform_type* waveformToAppend); 

//---------------------------------------------------------------------------------------------------------  
// Repeated Waveform 
//--------------------------------------------------------------------------------------------------------- 
	// Creates a waveform that must be repeated repeat times. data* must be allocated with malloc and be of a basic data type
RepeatedWaveform_type*	init_RepeatedWaveform_type				(RepeatedWaveformTypes waveformType, double samplingRate, size_t nSamples, void* data, double repeat);
	// Discards a repeated waveform contained and its waveform data that was allocated with malloc.
void					discard_RepeatedWaveform_type			(RepeatedWaveform_type** waveform);
	// Returns pointer to repeated waveform data (for one repeat)
void*					GetRepeatedWaveformDataPtr				(RepeatedWaveform_type* waveform, size_t* nSamples);
	// Returns the number of times the waveform must be repeated
double					GetRepeatedWaveformRepeats				(RepeatedWaveform_type* waveform);
	// Returns the number of samples in the waveform that must be repeated. Note: the total number of samples is this value times the number of repeats
size_t					GetRepeatedWaveformNumSamples			(RepeatedWaveform_type* waveform);
	// Returns number of bytes per repeated waveform element.
size_t					GetRepeatedWaveformSizeofData			(RepeatedWaveform_type* waveform);

//---------------------------------------------------------------------------------------------------------  
// Pulse Trains     
//--------------------------------------------------------------------------------------------------------- 
 //initializes a pulsetrain 
PulseTrain_type* init_PulseTrain (PulseTrainTimingTypes type,int mode);
// Discards pulsetrain        
void discard_PulseTrain (PulseTrain_type* pulsetrain);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __DataType_H__ */
