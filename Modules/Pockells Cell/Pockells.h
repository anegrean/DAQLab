//==============================================================================
//
// Title:		Pockells.h
// Purpose:		Provides Pockells cell electro-optic modulator control.
//
// Created on:	5-3-2015 at 12:36:38 by Adrian Negrean.
// Copyright:	VU University Amsterdam. All Rights Reserved.
// License:     This Source Code Form is subject to the terms of the Mozilla Public 
//              License v. 2.0. If a copy of the MPL was not distributed with this 
//              file, you can obtain one at https://mozilla.org/MPL/2.0/ . 
//
//==============================================================================

#ifndef __Pockells_H__
#define __Pockells_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "DAQLabModule.h"

//==============================================================================
// Constants
		
#define MOD_Pockells_NAME 	"Pockells cells"

//==============================================================================
// Types
	

//==============================================================================
// External variables

//==============================================================================
// Global functions

DAQLabModule_type*	initalloc_PockellsModule			(DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl);
void 				discard_PockellsModule				(DAQLabModule_type** mod); 


#ifdef __cplusplus
    }
#endif

#endif  /* ndef __Pockells_H__ */
