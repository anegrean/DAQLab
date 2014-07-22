//==============================================================================
//
// Title:		NIDAQmx.h
// Purpose:		A short description of the interface.
//
// Created on:	22-7-2014 at 15:54:27 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __NIDAQmx_H__
#define __NIDAQmx_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "DAQLabModule.h"

//==============================================================================
// Constants

#define MOD_NIDAQmx_NAME 	"NI DAQmx device" 

//==============================================================================
// Types

typedef struct NIDAQmx 	NIDAQmx_type;

//==============================================================================
// External variables

//==============================================================================
// Global functions

DAQLabModule_type*	initalloc_NIDAQmx 	(DAQLabModule_type* mod, char className[], char instanceName[]);
void 				discard_NIDAQmx		(DAQLabModule_type** mod); 

	// loads generic Z stage resources
int					NIDAQmx_Load 		(DAQLabModule_type* mod, int workspacePanHndl);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __NIDAQmx_H__ */
