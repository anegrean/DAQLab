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
#include "toolbox.h"
#include "nidaqmx.h"

//==============================================================================
// Constants

//==============================================================================
// Types

	//----------------------------------------------------------------------------------------------
	// Function types
	//---------------------------------------------------------------------------------------------- 
	
typedef void (*DiscardFptr_type) (void** dataPtr);  // generic function type to dispose of dinamically allocated data
		
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
	
	Image_UChar,					// 8 bit unsigned
	Image_UShort,					// 16 bit unsigned
	Image_Short,					// 16 bit signed
	Image_UInt,						// 32 bit unsigned
	Image_Int,						// 32 bit signed
	Image_Float,					// 32 bit float
	Image_NIVision,					// NI VISION
	
} ImageTypes;

	//----------------------------------------------------------------------------------------------
	// Color
	//----------------------------------------------------------------------------------------------

typedef struct RGBA		RGBA_type;

struct RGBA {
	
	unsigned char R;     								// Red value of the color. 
	unsigned char G;     								// Green value of the color.
    unsigned char B;     								// Blue value of the color.
    unsigned char A; 									// Alpha value of the color (0 = transparent, 255 = opaque).
	
};

	//----------------------------------------------------------------------------------------------
	// Region Of Interest (ROI) types for images
	//----------------------------------------------------------------------------------------------

typedef struct ROI 			ROI_type;		 // Base class
typedef struct Point		Point_type;		 // Child class of ROI_type
typedef struct Rect			Rect_type;		 // Child class of ROI_type

typedef enum {
	
	ROI_Point,
	ROI_Rectangle
	
} ROITypes;

	// Generic ROI base class	
struct ROI {
	// DATA
	ROITypes			ROIType;
	char*				ROIName;
	RGBA_type			rgba;			// ROI color
	// METHODS
	DiscardFptr_type	discardFptr;  	// overriden by child classes
};

	// Point
struct Point {
	ROI_type			baseClass;
	int 				x;
	int 				y;
};

	// Rectangle
struct Rect {
	ROI_type			baseClass;
	int 				top;
	int 				left;
	int 				height;
	int 				width;
};


	//----------------------------------------------------------------------------------------------
	// Pulse train types
	//----------------------------------------------------------------------------------------------

typedef struct PulseTrain PulseTrain_type; 

typedef struct PulseTrainFreqTiming	PulseTrainFreqTiming_type; 
typedef struct PulseTrainTimeTiming	PulseTrainTimeTiming_type;  
typedef struct PulseTrainTickTiming	PulseTrainTickTiming_type;  

typedef enum {
	PulseTrain_Freq,
	PulseTrain_Time,
	PulseTrain_Ticks
} PulseTrainTimingTypes;

typedef enum {
	PulseTrainIdle_Low 		= DAQmx_Val_Low,
	PulseTrainIdle_High		= DAQmx_Val_High
} PulseTrainIdleStates;

typedef enum {
	PulseTrain_Finite		= DAQmx_Val_FiniteSamps,
	PulseTrain_Continuous	= DAQmx_Val_ContSamps
} PulseTrainModes;

	//---------------------------------------------------------------------------------------------------------------------------------------
	// All types
	//---------------------------------------------------------------------------------------------------------------------------------------
	
typedef enum {
	//----------------------------------------------------------------------------------------
	// Atomic types
	//----------------------------------------------------------------------------------------
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
	
	//----------------------------------------------------------------------------------------  
	// Composite types
	//----------------------------------------------------------------------------------------
	
	//------------------------------------------
	// String
	//------------------------------------------
	DL_CString,		// null-terminated string
	
	//------------------------------------------
	// Waveforms
	//------------------------------------------
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
	
	//------------------------------------------
	// Repeated Waveforms
	//------------------------------------------
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
	
	//------------------------------------------
	// Images
	//------------------------------------------
	DL_Image_NIVision,
	DL_Image_UChar,	
	DL_Image_UShort,				
	DL_Image_Short,			
	DL_Image_UInt,				
	DL_Image_Int,				
	DL_Image_Float,	
	
	//------------------------------------------
	// Region Of Interest (ROI) types for images
	//------------------------------------------
	DL_ROI_Point,
	DL_ROI_Rectangle,
	
	//------------------------------------------
	// Pulse Train
	//------------------------------------------
	DL_PulseTrain_Freq,
	DL_PulseTrain_Time,
	DL_PulseTrain_Ticks

} DLDataTypes;


//==============================================================================
// External variables

//==============================================================================
// Global functions


//---------------------------------------------------------------------------------------------------------
// Waveform
//---------------------------------------------------------------------------------------------------------  
	// Creates a waveform container for void* basic data allocated with malloc.
Waveform_type*				init_Waveform_type						(WaveformTypes waveformType, double samplingRate, size_t nSamples, void** ptrToData);

	// Discards the waveform container and its data allocated with malloc.
void 						discard_Waveform_type 					(Waveform_type** waveform);

	// Sets waveform name
void						SetWaveformName							(Waveform_type* waveform, char waveformName[]);

	// Sets a physical unit name for the waveform
void						SetWaveformPhysicalUnit 				(Waveform_type* waveform, char unitName[]);

	// Adds current date timestamp to mark the beginning of the waveform
int							AddWaveformDateTimestamp				(Waveform_type* waveform);

	// Returns waveform timestamp
double						GetWaveformDateTimestamp				(Waveform_type* waveform); 
	
	// Returns number of bytes per waveform element.
size_t						GetWaveformSizeofData					(Waveform_type* waveform);

	// Returns number of samples in the waveform.
size_t						GetWaveformNumSamples					(Waveform_type* waveform);

	// Returns pointer to waveform data.
void** 						GetWaveformPtrToData 					(Waveform_type* waveform, size_t* nSamples);

	// Returns waveform data type
WaveformTypes				GetWaveformDataType						(Waveform_type* waveform);

	// Makes a waveform copy
int							CopyWaveform							(Waveform_type** waveformCopy, Waveform_type* waveform, char** errorInfo);

	// Appends data from one waveform to another. Sampling rate, data type and physical unit must be the same.
int 						AppendWaveform 							(Waveform_type* waveformToAppendTo, Waveform_type* waveformToAppend, char** errorInfo); 

//---------------------------------------------------------------------------------------------------------  
// Repeated Waveform 
//--------------------------------------------------------------------------------------------------------- 
	// Creates a waveform that must be repeated repeat times. data* must be allocated with malloc and be of a basic data type
RepeatedWaveform_type*		init_RepeatedWaveform_type				(RepeatedWaveformTypes waveformType, double samplingRate, size_t nSamples, void** ptrToData, double repeat);

	// Converts a Waveform_type to a RepeatedWaveform_type
RepeatedWaveform_type*		ConvertWaveformToRepeatedWaveformType	(Waveform_type** waveform, double repeat);	

	// Discards a repeated waveform contained and its waveform data that was allocated with malloc.
void						discard_RepeatedWaveform_type			(RepeatedWaveform_type** waveform);

	// Returns pointer to repeated waveform data (for one repeat)
void**						GetRepeatedWaveformPtrToData			(RepeatedWaveform_type* waveform, size_t* nSamples);

	// Returns the number of times the waveform must be repeated
double						GetRepeatedWaveformRepeats				(RepeatedWaveform_type* waveform);

	// Returns the number of samples in the waveform that must be repeated. Note: the total number of samples is this value times the number of repeats
size_t						GetRepeatedWaveformNumSamples			(RepeatedWaveform_type* waveform);

	// Returns number of bytes per repeated waveform element.
size_t						GetRepeatedWaveformSizeofData			(RepeatedWaveform_type* waveform);


//---------------------------------------------------------------------------------------------------------  
// Pulse Trains     
//--------------------------------------------------------------------------------------------------------- 

	//-------------------------------------
	// Init/Discard
	//-------------------------------------

	// Frequency defined pulse train
PulseTrainFreqTiming_type* 	init_PulseTrainFreqTiming_type 			(PulseTrainModes mode, PulseTrainIdleStates idleState, uInt64 nPulses, double dutyCycle, 
																	 double frequency, double initialDelay);
	// Time defined pulse train
PulseTrainTimeTiming_type* 	init_PulseTrainTimeTiming_type 			(PulseTrainModes mode, PulseTrainIdleStates idleState, uInt64 nPulses, double highTime, 
																	 double lowTime, double initialDelay);
	// Ticks defined pulse train
PulseTrainTickTiming_type* 	init_PulseTrainTickTiming_type 			(PulseTrainModes mode, PulseTrainIdleStates idleState, uInt64 nPulses, uInt32 highTicks, 
																	 uInt32 lowTicks, uInt32 delayTicks);
	// Discards all pulse train types       
void 						discard_PulseTrain_type 				(PulseTrain_type** pulseTrain);

	//-------------------------------------
	// General pulse train functions
	//-------------------------------------
	
		//returns a copy of the pulsetrain 
PulseTrain_type* 			CopyPulseTrain							(PulseTrain_type* pulsetrain);

	//-------------------------------------
	// Set/Get All pulse train types
	//-------------------------------------

	// sets the pulse train idle state
void   						SetPulseTrainIdleState					(PulseTrain_type* pulseTrain, PulseTrainIdleStates idleState);

	// gets thepulsetrain idle state   
PulseTrainIdleStates	 	GetPulseTrainIdleState					(PulseTrain_type* pulseTrain);

	// sets the pulsetrain number of pulses
void   						SetPulseTrainNPulses					(PulseTrain_type* pulseTrain, uInt64 nPulses);

	// gets the number of pulses in a pulsetrain
uInt64   					GetPulseTrainNPulses					(PulseTrain_type* pulseTrain);

	// sets the pulsetrain mode
void   						SetPulseTrainMode						(PulseTrain_type* pulseTrain, PulseTrainModes mode);

	// gets the pulsetrain mode
PulseTrainModes   			GetPulseTrainMode						(PulseTrain_type* pulseTrain);

	// gets the pulsetrain type
PulseTrainTimingTypes   	GetPulseTrainType						(PulseTrain_type* pulseTrain);

	//-------------------------------------
	// Set/Get Frequency pulse train type
	//-------------------------------------

	// sets the pulsetrain frequency
void   						SetPulseTrainFreqTimingFreq				(PulseTrainFreqTiming_type* pulseTrain, double frequency);

	// gets the pulsetrain frequency
double 						GetPulseTrainFreqTimingFreq				(PulseTrainFreqTiming_type* pulseTrain);

	// sets the pulsetrain duty cycle
void   						SetPulseTrainFreqTimingDutyCycle		(PulseTrainFreqTiming_type* pulseTrain, double dutyCycle);

	// gets the pulsetrain duty cycle  
double 						GetPulseTrainFreqTimingDutyCycle		(PulseTrainFreqTiming_type* pulseTrain);

	// sets the pulsetrain initial delay 
void   						SetPulseTrainFreqTimingInitialDelay		(PulseTrainFreqTiming_type* pulseTrain, double initialDelay);

	// gets the pulsetrain initial delay 
double 						GetPulseTrainFreqTimingInitialDelay		(PulseTrainFreqTiming_type* pulseTrain);

	//-------------------------------------
	// Set/Get Ticks pulse train type
	//-------------------------------------

	// sets the pulsetrain highticks
void   						SetPulseTrainTickTimingHighTicks		(PulseTrainTickTiming_type* pulseTrain, uInt32 highTicks);

	// gets the pulsetrain highticks
uInt32   					GetPulseTrainTickTimingHighTicks		(PulseTrainTickTiming_type* pulseTrain);

	// sets the pulsetrain lowticks
void   						SetPulseTrainTickTimingLowTicks			(PulseTrainTickTiming_type* pulseTrain, uInt32 lowTicks);

	// gets the pulsetrain lowticks
uInt32   					GetPulseTrainTickTimingLowTicks			(PulseTrainTickTiming_type* pulseTrain);

	// sets the pulsetrain delayticks
void   						SetPulseTrainTickTimingDelayTicks		(PulseTrainTickTiming_type* pulseTrain, uInt32 delayTicks);

	// gets the pulsetrain delayticks
uInt32   					GetPulseTrainTickTimingDelayTicks		(PulseTrainTickTiming_type* pulseTrain);

	//-------------------------------------
	// Set/Get Time pulse train type
	//-------------------------------------

	// sets the pulsetrain hightime
void   						SetPulseTrainTimeTimingHighTime			(PulseTrainTimeTiming_type* pulseTrain, double highTime);

	// gets the pulsetrain hightime
double   					GetPulseTrainTimeTimingHighTime			(PulseTrainTimeTiming_type* pulseTrain);

	// sets the pulsetrain lowtime
void   						SetPulseTrainTimeTimingLowTime			(PulseTrainTimeTiming_type* pulseTrain, double lowTime);

	// gets the pulsetrain lowtime
double   					GetPulseTrainTimeTimingLowTime			(PulseTrainTimeTiming_type* pulseTrain);

	// sets the pulsetrain initial delay time
void   						SetPulseTrainTimeTimingInitialDelay		(PulseTrainTimeTiming_type* pulseTrain, double delayTime);

	// gets the pulsetrain iniial delay time
double   					GetPulseTrainTimeTimingInitialDelay		(PulseTrainTimeTiming_type* pulseTrain);

//---------------------------------------------------------------------------------------------------------  
// Region Of Interest (ROI) types for images
//---------------------------------------------------------------------------------------------------------

//---------------------------
// Init/Discard
//---------------------------

	// Creates a Point ROI with default Black Opaque color.
Point_type*					init_Point_type							(char ROIName[], int x, int y);

	// Creates a Rectangle with default Black Opaque color.
Rect_type*					init_Rect_type							(char ROIName[], int top, int left, int height, int width);

	// Discards all ROIs
void						discard_ROI_type						(ROI_type** ROIPtr);

//---------------------------
// ROI operations
//---------------------------

	// Copies a ROI
ROI_type*					copy_ROI_type							(ROI_type* ROI);

	// Generates a unique ROI name given an existing ROI_type* list, starting with a single letter "a", 
	// trying each letter alphabetically, after which it increments the number of characters and starts again e.g."aa", "ab"
char*						GetDefaultUniqueROIName					(ListType ROIList);

int							SetROIName								(ROI_type* ROI, char newName[]);






#ifdef __cplusplus
    }
#endif

#endif  /* ndef __DataType_H__ */
