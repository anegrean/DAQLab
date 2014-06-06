//==============================================================================
//
// Title:		PIMercuryC863.h
// Purpose:		A short description of the interface.
//
// Created on:	10-3-2014 at 12:06:57 by Adrian Negrean.
// Copyright:	. All Rights Reserved.
//
//==============================================================================

#ifndef __PIMercuryC863_H__
#define __PIMercuryC863_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "Zstage.h"

//==============================================================================
// Constants

#define MOD_PIMercuryC863_NAME 	"PI Mercury C863 servo motion controller" 

//==============================================================================
// Types
		
typedef struct PIMercuryC863 PIMercuryC863_type;
		
//==============================================================================
// Module implementation

struct PIMercuryC863 {
	
	// SUPER, must be the first member to inherit from
	
	Zstage_type 	zstage_base;

	// DATA
	
	// METHODS
	
};

//==============================================================================
// External variables

//==============================================================================
// Global functions

DAQLabModule_type*	initalloc_PIMercuryC863		(DAQLabModule_type* mod);
void 				discard_PIMercuryC863 		(DAQLabModule_type* mod);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __PIMercuryC863_H__ */
