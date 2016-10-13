//==============================================================================
//
// Title:		PIStage.h
// Purpose:		PI Z-stage.
//
// Created on:	10-3-2014 at 12:06:57 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
// License:     This Source Code Form is subject to the terms of the Mozilla Public 
//              License v. 2.0. If a copy of the MPL was not distributed with this 
//              file, you can obtain one at https://mozilla.org/MPL/2.0/ . 
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
	
	ZStage_type 	baseClass;

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

DAQLabModule_type*	initalloc_PIStage		(DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl);
void 				discard_PIStage 		(DAQLabModule_type** mod);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __PIStage_H__ */
