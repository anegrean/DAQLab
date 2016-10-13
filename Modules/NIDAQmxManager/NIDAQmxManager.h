//==============================================================================
//
// Title:		NIDAQmxManager.h
// Purpose:		NIDAQmx control for data acquisition and generation.
//
// Created on:	22-7-2014 at 15:54:27 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
// License:     This Source Code Form is subject to the terms of the Mozilla Public 
//              License v. 2.0. If a copy of the MPL was not distributed with this 
//              file, you can obtain one at https://mozilla.org/MPL/2.0/ . 
//
//==============================================================================

#ifndef __NIDAQmxManager_H__
#define __NIDAQmxManager_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "DAQLabModule.h"
#include <nidaqmx.h>  

//==============================================================================
// Constants

#define MOD_NIDAQmxManager_NAME 	"NI DAQmx Manager" 
		

//==============================================================================
// Types

//==============================================================================
// External variables

//==============================================================================
// Global functions

DAQLabModule_type*	initalloc_NIDAQmxManager	(DAQLabModule_type* mod, char className[], char instanceName[], workspacePanHndl);
void 				discard_NIDAQmxManager		(DAQLabModule_type** mod); 

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __NIDAQmxManager_H__ */
