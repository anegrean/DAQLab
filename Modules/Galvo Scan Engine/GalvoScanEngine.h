//==============================================================================
//
// Title:		GalvoScanEngine.h
// Purpose:		A short description of the interface.
//
// Created on:	25-8-2014 at 14:17:14 by Adrian.
// Copyright:	. All Rights Reserved.
//
//==============================================================================

#ifndef __GalvoScanEngine_H__
#define __GalvoScanEngine_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "DAQLabModule.h"

//==============================================================================
// Constants
		
#define MOD_GalvoScanEngine_NAME 	"Galvo Scan Engine"

//==============================================================================
// Types
	

//==============================================================================
// External variables

//==============================================================================
// Global functions

DAQLabModule_type*	initalloc_GalvoScanEngine	(DAQLabModule_type* mod, char className[], char instanceName[]);
void 				discard_GalvoScanEngine		(DAQLabModule_type** mod); 

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __GalvoScanEngine_H__ */
