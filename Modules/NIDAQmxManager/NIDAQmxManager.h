//==============================================================================
//
// Title:		NIDAQmxManager.h
// Purpose:		A short description of the interface.
//
// Created on:	22-7-2014 at 15:54:27 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
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

//==============================================================================
// Constants

#define MOD_NIDAQmxManager_NAME 	"NI DAQmx Manager" 

//==============================================================================
// Types



//==============================================================================
// External variables

//==============================================================================
// Global functions

DAQLabModule_type*	initalloc_NIDAQmxManager	(DAQLabModule_type* mod, char className[], char instanceName[]);
void 				discard_NIDAQmxManager		(DAQLabModule_type** mod); 

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __NIDAQmxManager_H__ */
