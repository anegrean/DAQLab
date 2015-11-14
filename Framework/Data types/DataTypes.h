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
typedef void (*CallbackFptr_type) (void* objectRef, int event, void* eventData, void* callbackFunctionData);
		
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
	
	WaveformColor_RED			=   VAL_RED,
	WaveformColor_GREEN			=   VAL_GREEN,
	WaveformColor_BLUE			=   VAL_BLUE,
	WaveformColor_CYAN			=   VAL_CYAN,
	WaveformColor_MAGENTA		=   VAL_MAGENTA,
	WaveformColor_YELLOW		=   VAL_YELLOW,
	WaveformColor_DK_RED		=   VAL_DK_RED,
	WaveformColor_DK_BLUE		=   VAL_DK_BLUE,
	WaveformColor_DK_GREEN		=   VAL_DK_GREEN,
	WaveformColor_DK_CYAN		=   VAL_DK_CYAN,
	WaveformColor_DK_MAGENTA	=   VAL_DK_MAGENTA,
	WaveformColor_DK_YELLOW		=   VAL_DK_YELLOW,
	WaveformColor_LT_GRAY		=   VAL_LT_GRAY,
	WaveformColor_DK_GRAY		=   VAL_DK_GRAY,
	WaveformColor_BLACK			=   VAL_BLACK,
	WaveformColor_WHITE			=   VAL_WHITE,
	
} WaveformColors;
		
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

	// transform method to use for displaying image
typedef enum {
	
	ImageDisplayTransform_Linear,		// Display uses linear remapping. 
	ImageDisplayTransform_Logarithmic   // Display uses logarithmic remapping. Enhances contrast for small pixel values and reduces contrast for large pixel values. 
	
} ImageDisplayTransforms;

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

typedef struct ROI 			ROI_type;			// Base class
typedef struct Point		Point_type;		 	// Child class of ROI_type
typedef struct Rect			Rect_type;		 	// Child class of ROI_type

typedef enum {
	ROI_Point,
	ROI_Rectangle
} ROITypes;

	// Generic ROI base class	
struct ROI {
	// DATA
	ROITypes			ROIType;
	char*				ROIName;
	RGBA_type			rgba;					// ROI color
	BOOL				active;					// Flag to mark active ROIs, by default, TRUE.
	RGBA_type			textBackground;			// Color of ROIs label background.
	int					textFontSize;			// Font size for displaying ROI labels. Default: 12
	// METHODS
	CopyFptr_type		copyFptr;				// Overriden by child. Copies the object.
	DiscardFptr_type	discardFptr;  			// Overriden by child. Discards the object.
	
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
typedef enum {
	PulseTrainIdle_Low 		= FALSE,
	PulseTrainIdle_High		= TRUE
} PulseTrainIdleStates;

// pulse train modes
typedef enum {
	PulseTrain_Finite		= FALSE,
	PulseTrain_Continuous	= TRUE
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

	// Clears a list of Waveform_type* elements and keeps the list.
void						ClearWaveformList						(ListType waveformList);

	// Clears and discards a list of Waveform_type* elements..
void						DiscardWaveformList						(ListType* waveformListPtr);



//-----------------------------------------
// Set/Get
//-----------------------------------------

	// Returns pointer to waveform data.
void** 						GetWaveformPtrToData 					(Waveform_type* waveform, size_t* nSamples);

	// Waveform type
WaveformTypes				GetWaveformDataType						(Waveform_type* waveform);

	// Samples in the waveform.
size_t						GetWaveformNumSamples					(Waveform_type* waveform);

	// Sampling rate
double						GetWaveformSamplingRate					(Waveform_type* waveform);

	// Name
void						SetWaveformName							(Waveform_type* waveform, char waveformName[]);
char* 						GetWaveformName 						(Waveform_type* waveform);

	// Color
void						SetWaveformColor						(Waveform_type* waveform, WaveformColors color);
WaveformColors				GetWaveformColor						(Waveform_type* waveform);

	// Physical unit
void						SetWaveformPhysicalUnit 				(Waveform_type* waveform, char unitName[]);
char* 						GetWaveformPhysicalUnit 				(Waveform_type* waveform);

	// Adds timestamp marking the beginning of the waveform. Function returns 0 on success and <0 if it fails.
int							AddWaveformDateTimestamp				(Waveform_type* waveform);
double						GetWaveformDateTimestamp				(Waveform_type* waveform); 

	// Returns number of bytes per waveform element.
size_t						GetWaveformSizeofData					(Waveform_type* waveform);

//-----------------------------------------
// Operations
//-----------------------------------------

	// Makes a waveform copy
int							CopyWaveform							(Waveform_type** waveformCopy, Waveform_type* waveform, char** errorMsg);

	// Appends data from one waveform to another. Sampling rate, data type and physical unit must be the same. The color used for the extended waveform
	// is the same as for the waveform to which the second waveform was appended.
int 						AppendWaveform 							(Waveform_type* waveformToAppendTo, Waveform_type* waveformToAppend, char** errorMsg);

	// Applies an nSamples integration to an entire waveform or to a part of it and returns a new waveform.
	// The portion of the waveform used starts at element with 0-based index startIdx and ends at (including) element with 0-based index endIdx.
	// If endIdx = 0 the entire input waveform is used. nInt is the number of samples used for integration. If the number of elements in the input is less
	// than the number of points used for integration, then an empty waveform is returned.
	// The output waveform will be of Waveform_Double type
int							IntegrateWaveform						(Waveform_type** waveformOut, Waveform_type* waveformIn, size_t startIdx, size_t endIdx, size_t nInt, char** errorMsg);


//---------------------------------------------------------------------------------------------------------  
// Repeated Waveform 
//--------------------------------------------------------------------------------------------------------- 
	// Creates a waveform that must be repeated repeat times. data* must be allocated with malloc and be of a basic data type
RepeatedWaveform_type*		init_RepeatedWaveform_type				(RepeatedWaveformTypes waveformType, double samplingRate, size_t nSamples, void** ptrToData, double repeat);

	// Converts a Waveform_type to a RepeatedWaveform_type
RepeatedWaveform_type*		ConvertWaveformToRepeatedWaveformType	(Waveform_type** waveform, double repeat);	

	// Discards a repeated waveform contained and its waveform data that was allocated with malloc.
void						discard_RepeatedWaveform_type			(RepeatedWaveform_type** waveformPtr);

//-----------------------------------------
// Set/Get
//-----------------------------------------

	// Color
void						SetRepeatedWaveformColor				(RepeatedWaveform_type* waveform, WaveformColors color);
WaveformColors				GetrepeatedWaveformColor				(RepeatedWaveform_type* waveform);

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
	// Ticks defined pulse train. Set clockFrequency to 0 Hz if unknown.
PulseTrainTickTiming_type* 	init_PulseTrainTickTiming_type 			(PulseTrainModes mode, PulseTrainIdleStates idleState, uInt64 nPulses, uInt32 highTicks, 
																	 uInt32 lowTicks, uInt32 delayTicks, double clockFrequency);
	// Discards all pulse train types       
void 						discard_PulseTrain_type 				(PulseTrain_type** pulseTrainPtr);

	//-------------------------------------
	// General pulse train functions
	//-------------------------------------
	
		//returns a copy of the pulsetrain 
PulseTrain_type* 			CopyPulseTrain							(PulseTrain_type* pulseTrain);

	//-------------------------------------
	// Set/Get All pulse train types
	//-------------------------------------

	// idle state
void   						SetPulseTrainIdleState					(PulseTrain_type* pulseTrain, PulseTrainIdleStates idleState);
PulseTrainIdleStates	 	GetPulseTrainIdleState					(PulseTrain_type* pulseTrain);

	// number of pulses
void   						SetPulseTrainNPulses					(PulseTrain_type* pulseTrain, uInt64 nPulses);
uInt64   					GetPulseTrainNPulses					(PulseTrain_type* pulseTrain);

	// pulsetrain mode
void   						SetPulseTrainMode						(PulseTrain_type* pulseTrain, PulseTrainModes mode);
PulseTrainModes   			GetPulseTrainMode						(PulseTrain_type* pulseTrain);

	// gets the pulsetrain type
PulseTrainTimingTypes   	GetPulseTrainType						(PulseTrain_type* pulseTrain);

	//-------------------------------------
	// Set/Get Frequency pulse train type
	//-------------------------------------

	// frequency
void   						SetPulseTrainFreqTimingFreq				(PulseTrainFreqTiming_type* pulseTrain, double frequency);
double 						GetPulseTrainFreqTimingFreq				(PulseTrainFreqTiming_type* pulseTrain);

	// duty cycle
void   						SetPulseTrainFreqTimingDutyCycle		(PulseTrainFreqTiming_type* pulseTrain, double dutyCycle);
double 						GetPulseTrainFreqTimingDutyCycle		(PulseTrainFreqTiming_type* pulseTrain);

	// initial delay 
void   						SetPulseTrainFreqTimingInitialDelay		(PulseTrainFreqTiming_type* pulseTrain, double initialDelay);
double 						GetPulseTrainFreqTimingInitialDelay		(PulseTrainFreqTiming_type* pulseTrain);

	//-------------------------------------
	// Set/Get Ticks pulse train type
	//-------------------------------------

	// highticks
void   						SetPulseTrainTickTimingHighTicks		(PulseTrainTickTiming_type* pulseTrain, uInt32 highTicks);
uInt32   					GetPulseTrainTickTimingHighTicks		(PulseTrainTickTiming_type* pulseTrain);

	// lowticks
void   						SetPulseTrainTickTimingLowTicks			(PulseTrainTickTiming_type* pulseTrain, uInt32 lowTicks);
uInt32   					GetPulseTrainTickTimingLowTicks			(PulseTrainTickTiming_type* pulseTrain);

	// delayticks
void   						SetPulseTrainTickTimingDelayTicks		(PulseTrainTickTiming_type* pulseTrain, uInt32 delayTicks);
uInt32   					GetPulseTrainTickTimingDelayTicks		(PulseTrainTickTiming_type* pulseTrain);

	// clock frequency in [Hz]
void						SetPulseTrainTickTimingClockFrequency	(PulseTrainTickTiming_type* pulseTrain, double clockFrequency);
double						GetPulseTrainTickTimingClockFrequency	(PulseTrainTickTiming_type* pulseTrain);

	//-------------------------------------
	// Set/Get Time pulse train type
	//-------------------------------------

	// hightime
void   						SetPulseTrainTimeTimingHighTime			(PulseTrainTimeTiming_type* pulseTrain, double highTime);
double   					GetPulseTrainTimeTimingHighTime			(PulseTrainTimeTiming_type* pulseTrain);

	// lowtime
void   						SetPulseTrainTimeTimingLowTime			(PulseTrainTimeTiming_type* pulseTrain, double lowTime);
double   					GetPulseTrainTimeTimingLowTime			(PulseTrainTimeTiming_type* pulseTrain);

	// delay time
void   						SetPulseTrainTimeTimingInitialDelay		(PulseTrainTimeTiming_type* pulseTrain, double delayTime);
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

int							AddImageROI								(Image_type* image, ROI_type** ROIPtr, size_t* ROIIdxPtr);

ListType 					GetImageROIs 							(Image_type* image);

void						SetImageDisplayTransform				(Image_type* image, ImageDisplayTransforms dispTransformFunc);

ImageDisplayTransforms		GetImageDisplayTransform				(Image_type* image);

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

	// Creates a Rectangle.
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
CallbackGroup_type*			init_CallbackGroup_type					(void* callbackGroupOwner, size_t nCallbackFunctions, CallbackFptr_type* callbackFunctions, void** callbackFunctionsData, DiscardFptr_type* discardCallbackDataFunctions);

void						discard_CallbackGroup_type				(CallbackGroup_type** callbackGroupPtr);

	// Dispatches event to all callback function in the callback group 
void						FireCallbackGroup						(CallbackGroup_type* callbackGroup, int event, void* eventData);

	// Set/Get
	
void						SetCallbackGroupOwner					(CallbackGroup_type* callbackGroup, void* callbackGroupOwner);

void*						GetCallbackGroupOwner					(CallbackGroup_type* callbackGroup);






#ifdef __cplusplus
    }
#endif

#endif  /* ndef __DataType_H__ */
