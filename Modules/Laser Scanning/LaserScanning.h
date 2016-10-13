//==============================================================================
//
// Title:		LaserScanning.h
// Purpose:		Laser scanning and photostimulation.
//
// Created on:	25-8-2014 at 14:17:14 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
// License:     This Source Code Form is subject to the terms of the Mozilla Public 
//              License v. 2.0. If a copy of the MPL was not distributed with this 
//              file, you can obtain one at https://mozilla.org/MPL/2.0/ . 
//
//==============================================================================

#ifndef __LaserScanning_H__
#define __LaserScanning_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "DAQLabModule.h"

//==============================================================================
// Constants
		
#define MOD_LaserScanning_NAME 	"Laser Scanning"

//==============================================================================
// Types
	

//==============================================================================
// External variables

//==============================================================================
// Global functions

DAQLabModule_type*	initalloc_LaserScanning		(DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl);
void 				discard_LaserScanning		(DAQLabModule_type** mod); 

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __GalvoScanEngine_H__ */
