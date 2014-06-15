//==============================================================================
//
// Title:		PIStage.h
// Purpose:		A short description of the interface.
//
// Created on:	10-3-2014 at 12:06:57 by Adrian Negrean.
// Copyright:	. All Rights Reserved.
//
//==============================================================================

#ifndef __PIStage_H__
#define __PIStage_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "Zstage.h"

//==============================================================================
// Constants

#define MOD_PIStage_NAME 		"PIStage motion controller" 

//==============================================================================
// Types
		
typedef struct PIStage PIStage_type;
		
//==============================================================================
// Module implementation

struct PIStage {
	
	// SUPER, must be the first member to inherit from
	
	Zstage_type 	zstage_base;

	// DATA
		
		// ID of PI Mercury controller
	long			PIStageID;
		// axis name assigned to the controller 
	char			assignedAxis[2];
	
	
	// METHODS
	
};

//==============================================================================
// External variables

//==============================================================================
// Global functions

DAQLabModule_type*	initalloc_PIStage		(DAQLabModule_type* mod, char className[], char instanceName[]);
void 				discard_PIStage 		(DAQLabModule_type** mod);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __PIStage_H__ */
