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

#include "cvidef.h"

//==============================================================================
// Constants

//==============================================================================
// Types

//==============================================================================
// External variables

//==============================================================================
// Global functions

int CreateHDF5file(char *filename,char* dataset_name);
int WriteHDF5Data(char *filename,char* dataset_name,TC_DS_Data_type* dsdata,Waveform_type* waveform,DLDataTypes datatype) ;
int WriteHDF5Image(char *filename,char* dataset_name,TC_DS_Data_type* dsdata,Image_type*	receivedimage);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __HDF5supporth_H__ */
