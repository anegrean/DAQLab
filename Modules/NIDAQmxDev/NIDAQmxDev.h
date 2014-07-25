//==============================================================================
//
// Title:		NIDAQmxDev.h
// Purpose:		A short description of the interface.
//
// Created on:	22-7-2014 at 15:54:27 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __NIDAQmxDev_H__
#define __NIDAQmxDev_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "DAQLabModule.h"

//==============================================================================
// Constants

#define MOD_NIDAQmxDev_NAME 	"NI DAQmx Devices" 

//==============================================================================
// Types

typedef struct NIDAQmxDev 	NIDAQmxDev_type;

//==============================================================================
// External variables

//==============================================================================
// Global functions

DAQLabModule_type*	initalloc_NIDAQmxDev	(DAQLabModule_type* mod, char className[], char instanceName[]);
void 				discard_NIDAQmxDev		(DAQLabModule_type** mod); 

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __NIDAQmxDev_H__ */
