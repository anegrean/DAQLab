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

//==============================================================================
// Constants

//==============================================================================
// Types
		
	//----------------------------------------------------------------------------------------------
	// Additional data types defined in nidaqmx.h in case it's missing
	//---------------------------------------------------------------------------------------------- 
		
// NI-DAQmx Typedefs
#ifndef _NI_int8_DEFINED_
#define _NI_int8_DEFINED_
	typedef signed char        int8;
#endif
#ifndef _NI_uInt8_DEFINED_
#define _NI_uInt8_DEFINED_
	typedef unsigned char      uInt8;
#endif
#ifndef _NI_int16_DEFINED_
#define _NI_int16_DEFINED_
	typedef signed short       int16;
#endif
#ifndef _NI_uInt16_DEFINED_
#define _NI_uInt16_DEFINED_
	typedef unsigned short     uInt16;
#endif
#ifndef _NI_int32_DEFINED_
#define _NI_int32_DEFINED_
	typedef signed long        int32;
#endif
#ifndef _NI_uInt32_DEFINED_
#define _NI_uInt32_DEFINED_
	typedef unsigned long      uInt32;
#endif
#ifndef _NI_float32_DEFINED_
#define _NI_float32_DEFINED_
	typedef float              float32;
#endif
#ifndef _NI_float64_DEFINED_
#define _NI_float64_DEFINED_
	typedef double             float64;
#endif
#ifndef _NI_int64_DEFINED_
#define _NI_int64_DEFINED_
#if defined(__linux__) || defined(__APPLE__)
	typedef long long int      int64;
#else
	typedef __int64            int64;
#endif
#endif
#ifndef _NI_uInt64_DEFINED_
#define _NI_uInt64_DEFINED_
#if defined(__linux__) || defined(__APPLE__)
	typedef unsigned long long uInt64;
#else
	typedef unsigned __int64   uInt64;
#endif
#endif

	//----------------------------------------------------------------------------------------------
	// Function types
	//---------------------------------------------------------------------------------------------- 

	// Generic function type to dispose of dinamically allocated data
typedef void (*DiscardFptr_type) (void** objectPtr);
	
	// Generic function to copy an object
typedef void* (*CopyFptr_type) (void* object);

	// Generic callback
typedef void (*CallbackFptr_type) (void* objectRef, int event, void* callbackData);
		
	//----------------------------------------------------------------------------------------------
	// Callback group
	//----------------------------------------------------------------------------------------------
typedef struct CallbackGroup	CallbackGroup_type;

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
	BasicData_Int64,
	BasicData_UInt64,
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
	Waveform_Int64,						
	Waveform_UInt64,						
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
	RepeatedWaveform_Int64,						
	RepeatedWaveform_UInt64,
	RepeatedWaveform_SSize,						
	RepeatedWaveform_Size,								   			
	RepeatedWaveform_Float,						
	RepeatedWaveform_Double					
	
} RepeatedWaveformTypes;							 
													
	//----------------------------------------------------------------------------------------------
	// Image types
	//----------------------------------------------------------------------------------------------   
typedef struct Image		Image_type;  

typedef enum {
	
	Image_UChar,					// 8 bit unsigned pixel array
	Image_UShort,					// 16 bit unsigned pixel array
	Image_Short,					// 16 bit signed pixel array
	Image_UInt,						// 32 bit unsigned pixel array
	Image_Int,						// 32 bit signed pixel array
	Image_Float,					// 32 bit float pixel array
	Image_RGBA,						// RGBA_type pixel array
	Image_RGBAU64,					// RGBAU64_type pixel array
	
} ImageTypes;

	//----------------------------------------------------------------------------------------------
	// Color
	//----------------------------------------------------------------------------------------------

// identical to NI Vision's RGBValue_struct
typedef struct RGBA		RGBA_type;
struct RGBA {
	unsigned char B;     								// Blue value of the color.
	unsigned char G;     								// Green value of the color.
	unsigned char R;     								// Red value of the color. 
    unsigned char alpha; 								// Alpha value of the color (0 = transparent, 255 = opaque).
};

// identical to NI Vision's RGBU64Value_struct
typedef struct RGBAU64	RGBAU64_type;
struct RGBAU64 {
    unsigned short B;     								// Blue value of the color.
    unsigned short G;     								// Green value of the color.
    unsigned short R;     								// Red value of the color.
    unsigned short alpha; 								// Alpha value of the color (0 = transparent, 255 = opaque).
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
	BOOL				active;			// Flag to mark active ROIs, by default, TRUE.
	// METHODS
	CopyFptr_type		copyFptr;		// Overriden by child. Copies the object.
	DiscardFptr_type	discardFptr;  	// Overriden by child. Discards the object.
	
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

// pulse train idle states
#ifdef ___nidaqmx_h___
typedef enum {
	PulseTrainIdle_Low 		= DAQmx_Val_Low,
	PulseTrainIdle_High		= DAQmx_Val_High
} PulseTrainIdleStates;
#else
typedef enum {
	PulseTrainIdle_Low 		= 0,
	PulseTrainIdle_High		= 1
} PulseTrainIdleStates;
#endif

// pulse train modes
#ifdef ___nidaqmx_h___ 
typedef enum {
	PulseTrain_Finite		= DAQmx_Val_FiniteSamps,
	PulseTrain_Continuous	= DAQmx_Val_ContSamps
} PulseTrainModes;
#else 
typedef enum {
	PulseTrain_Finite		= 0,
	PulseTrain_Continuous	= 1
} PulseTrainModes;
#endif

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
	DL_Int64,
	DL_UInt64,
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
	DL_Waveform_Int64,						
	DL_Waveform_UInt64,
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
	DL_RepeatedWaveform_Int64,						
	DL_RepeatedWaveform_UInt64,						
	DL_RepeatedWaveform_SSize,						
	DL_RepeatedWaveform_Size,								   			
	DL_RepeatedWaveform_Float,						
	DL_RepeatedWaveform_Double,	
	
	//------------------------------------------
	// Images
	//------------------------------------------
	DL_Image,
	
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
	// Gets the physical unit name for the waveform      
char* 						GetWaveformPhysicalUnit 				(Waveform_type* waveform);
	// Gets the waveform name     
char* 						GetWaveformName 						(Waveform_type* waveform);
	// Adds current date timestamp to mark the beginning of the waveform
int							AddWaveformDateTimestamp				(Waveform_type* waveform);

	// Returns waveform timestamp
double						GetWaveformDateTimestamp				(Waveform_type* waveform); 

	// Returns waveform sampling rate
double						GetWaveformSamplingRate					(Waveform_type* waveform);
	
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

	// Applies an nSamples integration to an entire waveform or to a part of it and returns a new waveform.
	// The portion of the waveform used starts at element with 0-based index startIdx and ends at (including) element with 0-based index endIdx.
	// If endIdx = 0 the entire input waveform is used. nInt is the number of samples used for integration. If the number of elements in the input is less
	// than the number of points used for integration, then an empty waveform is returned.
	// The output waveform will be of Waveform_Double type
int							IntegrateWaveform						(Waveform_type** waveformOut, Waveform_type* waveformIn, size_t startIdx, size_t endIdx, size_t nInt);


//---------------------------------------------------------------------------------------------------------  
// Repeated Waveform 
//--------------------------------------------------------------------------------------------------------- 
	// Creates a waveform that must be repeated repeat times. data* must be allocated with malloc and be of a basic data type
RepeatedWaveform_type*		init_RepeatedWaveform_type				(RepeatedWaveformTypes waveformType, double samplingRate, size_t nSamples, void** ptrToData, double repeat);

	// Converts a Waveform_type to a RepeatedWaveform_type
RepeatedWaveform_type*		ConvertWaveformToRepeatedWaveformType	(Waveform_type** waveform, double repeat);	

	// Discards a repeated waveform contained and its waveform data that was allocated with malloc.
void						discard_RepeatedWaveform_type			(RepeatedWaveform_type** waveformPtr);

	// Returns pointer to repeated waveform data (for one repeat)
void**						GetRepeatedWaveformPtrToData			(RepeatedWaveform_type* waveform, size_t* nSamples);

	// Returns the number of times the waveform must be repeated
double						GetRepeatedWaveformRepeats				(RepeatedWaveform_type* waveform);

	// Returns the sampling rate
double						GetRepeatedWaveformSamplingRate			(RepeatedWaveform_type* waveform);

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
void 						discard_PulseTrain_type 				(PulseTrain_type** pulseTrainPtr);

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
// Images
//---------------------------------------------------------------------------------------------------------
	// Creates an image container for void* basic data allocated with malloc.
Image_type*					init_Image_type							(ImageTypes imageType, int imgHeight, int imgWidth, void** imgDataPtr);

	// Discards the image container and its data allocated with malloc.
void 						discard_Image_type 						(Image_type** imagePtr);

	// Set/Get

void						GetImageSize							(Image_type* image, int* widthPtr, int* heightPtr);

void 						SetImagePixSize 						(Image_type* image, double pixSize);

double	 					GetImagePixSize 						(Image_type* image);

void 						SetImageTopLeftXCoord 					(Image_type* image, double imgTopLeftXCoord);

void 						SetImageTopLeftYCoord 					(Image_type* image, double imgTopLeftYCoord);

void 						SetImageZCoord 							(Image_type* image, double imgZCoord);

void						SetImageCoord							(Image_type* image, double imgTopLeftXCoord, double imgTopLeftYCoord, double imgZCoord);

void						GetImageCoordinates						(Image_type* image, double* imgTopLeftXCoordPtr, double* imgTopLeftYCoordPtr, double* imgZCoordPtr);

void* 						GetImagePixelArray 						(Image_type* image);
	
ImageTypes	 				GetImageType 							(Image_type* image);

size_t 						GetImageSizeofData 						(Image_type* image);

void 						SetImageROIs 							(Image_type* image, ListType ROIs);

ListType 					GetImageROIs 							(Image_type* image);

	// Image operations
Image_type* 				copy_Image_type							(Image_type* imgSource);


//---------------------------------------------------------------------------------------------------------  
// Region Of Interest (ROI) types for images
//---------------------------------------------------------------------------------------------------------

//---------------------------
// Init/Discard
//---------------------------

	// Creates a Point ROI with default Black Opaque color.
Point_type*					initalloc_Point_type					(Point_type* point, char ROIName[], RGBA_type color, BOOL active, int x, int y);

void 						discard_Point_type 						(Point_type** PointPtr);

	// Creates a Rectangle with default Black Opaque color.
Rect_type*					initalloc_Rect_type						(Rect_type* rect, char ROIName[], RGBA_type color, BOOL active, int top, int left, int height, int width);
void 						discard_Rect_type 						(Rect_type** RectPtr);


//---------------------------
// ROI operations
//---------------------------

	// Discards a list of ROI_type* elements
	
void						DiscardROIList							(ListType* ROIListPtr);

	// Copies a ROI list. ListType of ROI_type* elements.
ListType					CopyROIList								(ListType ROIList);

	// Generates a unique ROI name given an existing ROI_type* list, starting with a single letter "a", 
	// trying each letter alphabetically, after which it increments the number of characters and starts again e.g."aa", "ab"
char*						GetDefaultUniqueROIName					(ListType ROIList);

int							SetROIName								(ROI_type* ROI, char newName[]);

//----------------------------------------------------------------------------------------------
// Callback group
//----------------------------------------------------------------------------------------------

	// Creates a callback group. Each callback function has its own dinamically allocated callback data. By providing discardCallbackDataFunctions, the callback data
	// will be disposed of automatically when discarding the callback group. If this is not needed, then pass NULL instead of a discard function pointer.
CallbackGroup_type*			init_CallbackGroup_type					(void* objectRef, size_t nCallbackFunctions, CallbackFptr_type* callbackFunctions, void** callbackData, DiscardFptr_type* discardCallbackDataFunctions);

void						discard_CallbackGroup_type				(CallbackGroup_type** callbackGroupPtr);

void						FireCallbackGroup						(CallbackGroup_type* callbackGroup, int event);




#ifdef __cplusplus
    }
#endif

#endif  /* ndef __DataType_H__ */
