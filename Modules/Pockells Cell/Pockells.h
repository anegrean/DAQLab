//==============================================================================
//
// Title:		Pockells.h
// Purpose:		A short description of the interface.
//
// Created on:	5-3-2015 at 12:36:38 by Adrian Negrean.
// Copyright:	VU University Amsterdam. All Rights Reserved.
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
