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
		
		// Waveform types
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

typedef enum {
	
	Image_NIVision
	
} ImageTypes;

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


typedef enum {
	//------------------
	// atomic types
	//------------------
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
	
	//------------------
	// composite types
	//------------------
	// string
	DL_CString,							// null-terminated string
	
	// waveforms
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
	
	// images
	DL_Image_NIVision   
	
} DLDataTypes;


//==============================================================================
// External variables

//==============================================================================
// Global functions

//---------------------------------------------------------------------------------------------------------
// Waveforms
//---------------------------------------------------------------------------------------------------------  
	// Creates a waveform container for void* data allocated with malloc.
Waveform_type*			init_Waveform_type						(WaveformTypes dataType, char YAxisName[], char physicalUnit[], double dateTimestamp, double samplingRate, size_t nSamples, void* data);
	// Discards the waveform container and its data allocated with malloc.
void 					discard_Waveform_type 					(Waveform_type** waveform);
	// Returns number of bytes per waveform element.
size_t					GetWaveformSizeofData					(Waveform_type* waveform);
	// Returns number of samples in the waveform.
size_t					GetWaveformNumSamples					(Waveform_type* waveform);
	// Returns pointer to waveform data.
void* 					GetWaveformDataPtr 						(Waveform_type* waveform);
	// Copies samples from one waveform to another. Sampling rate, data type and physical unit must be the same. YAxisName and dateTimestamp of waveformToCopyTo are unchanged.
int						CopyWaveformData						(Waveform_type* waveformToCopyTo, Waveform_type* waveformToCopyFrom);
	// Appends data from one waveform to another and discards the appended waveform. Sampling rate, data type and physical unit must be the same.
int						AppendWaveformData						(Waveform_type* waveformToAppendTo, Waveform_type** waveformToAppend); 



#ifdef __cplusplus
    }
#endif

#endif  /* ndef __DataType_H__ */
