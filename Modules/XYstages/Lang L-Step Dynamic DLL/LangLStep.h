//==============================================================================
//
// Title:		LangLStep.h
// Purpose:		A short description of the interface.
//
// Created on:	22-3-2015 at 18:31:38 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __LangLStep_H__
#define __LangLStep_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "XYStage.h"

//==============================================================================
// Constants
		
#define MOD_LangLStep_NAME		"Lang XY stage"

//==============================================================================
// Types
		
typedef struct LangLStep LangLStep_type;  

//==============================================================================
// Module implementation

struct LangLStep {
	
	// SUPER, must be the first member to inherit from
	
	XYStage_type 	baseClass;

	// DATA
	HINSTANCE 		lstepLibHndl;	// DLL handle	
	
	// METHODS
	
	
};

//==============================================================================
// External variables

//==============================================================================
// Global functions

DAQLabModule_type*	initalloc_LangLStep		(DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl);
void 				discard_LangLStep 		(DAQLabModule_type** mod);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __LangLStep_H__ */
