//==============================================================================
//
// Title:		HDF5support.h
// Purpose:		Writes data to disk in HDF5 file format.
//
// Created on:	30-3-2015 at 10:08:17 by Adrian Negrean.
// Copyright:	Vrije Universiteit. All Rights Reserved.
// License:     This Source Code Form is subject to the terms of the Mozilla Public 
//              License v. 2.0. If a copy of the MPL was not distributed with this 
//              file, you can obtain one at https://mozilla.org/MPL/2.0/ . 
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

int 				CreateHDF5File					(char fileName[], char datasetName[], char** errorMsg);

	// Writes a waveform given data storage info
int 				WriteHDF5Waveform				(char fileName[], char datasetName[], DSInfo_type* dsInfo, Waveform_type* waveform, CompressionMethods compression, char** errorMsg);

	// Writes a list of waveforms of Waveform_type*
int					WriteHDF5WaveformList			(char fileName[], ListType waveformList, CompressionMethods compression, char** errorMsg);

int 				WriteHDF5Image					(char fileName[], char datasetName[], DSInfo_type* dsInfo, Image_type* image, CompressionMethods compression, char** errorMsg);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __HDF5supporth_H__ */
