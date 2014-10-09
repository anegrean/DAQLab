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
	
	Waveform_Char,						// 8 bits	   			 			char
	Waveform_UChar,						// 8 bits			 				unsigned char
	Waveform_Short,						// 16 bits							short
	Waveform_UShort,					// 16 bits							unsigned short
	Waveform_Int,						// 32 bits							int
	Waveform_UInt,						// 32 bits							unsigned int
	Waveform_SSize,						// 32 bits or 64 bits 				ssize_t
	Waveform_Size,						// 32 bits or 64 bits				size_t			   			
	Waveform_Float,						// 16 bits							float
	Waveform_Double,					// 32 bits							double
	
} WaveformTypes;

//==============================================================================
// External variables

//==============================================================================
// Global functions

	// Creates a waveform container for void* data allocated with malloc.

Waveform_type*			init_Waveform_type						(WaveformTypes dataType, char YAxisName[], char physicalUnit[], double dateTimestamp, double samplingRate, size_t nSamples, void* data);

	// Discards the waveform container and its data allocated with malloc.
void 					discard_Waveform_type 					(Waveform_type** waveform); 



#ifdef __cplusplus
    }
#endif

#endif  /* ndef __DataType_H__ */
