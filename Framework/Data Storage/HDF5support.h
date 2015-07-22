//==============================================================================
//
// Title:		HDF5support.h
// Purpose:		A short description of the interface.
//
// Created on:	30-3-2015 at 10:08:17 by Adrian Negrean.
// Copyright:	Vrije Universiteit. All Rights Reserved.
//
//==============================================================================

#ifndef __HDF5supporth_H__
#define __HDF5supporth_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

//#include "cvidef.h"

//==============================================================================
// Constants

//==============================================================================
// Types
		
typedef enum {
	
	Compression_None,
	Compression_GZIP, 	// GNU ZIP
	Compression_SZIP 	// scientific zip, license free only for non-commercial use
	
} CompressionMethods;

//==============================================================================
// External variables

//==============================================================================
// Global functions

int 				CreateHDF5File					(char fileName[], char datasetName[]);

int 				WriteHDF5Waveform				(char fileName[], char datasetName[], DSInfo_type* dsInfo, Waveform_type* waveform, CompressionMethods compression);

int 				WriteHDF5Image					(char fileName[], char datasetName[], DSInfo_type* dsInfo, Image_type* image, CompressionMethods compression);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __HDF5supporth_H__ */
