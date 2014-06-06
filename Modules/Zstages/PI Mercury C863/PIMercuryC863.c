//==============================================================================
//
// Title:		PIMercuryC863.c
// Purpose:		A short description of the implementation.
//
// Created on:	10-3-2014 at 12:06:57 by Adrian Negrean.
// Copyright:	. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include <ansi_c.h> 
#include "PIMercuryC863.h"

//==============================================================================
// Constants

//==============================================================================
// Types

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//==============================================================================
// Global variables

//==============================================================================
// Global functions

DAQLabModule_type*	initalloc_PIMercuryC863	(DAQLabModule_type* mod)
{
	PIMercuryC863_type* 	PIMzstage;
	Zstage_type*  			zstage;
	
	if (!mod) {
		PIMzstage = malloc (sizeof(PIMercuryC863_type));
		if (!PIMzstage) return NULL;
	} else
		PIMzstage = (PIMercuryC863_type*) mod;
	
	zstage = (Zstage_type*) PIMzstage;
	
	// initialize base class
	initalloc_Zstage ((DAQLabModule_type*)zstage);
	//------------------------------------------------------------
	
	//---------------------------
	// Parent Level 0: DAQLabModule_type 
	
		// DATA
		
	zstage->module_base.name		= StrDup(MOD_PIMercuryC863_NAME);
		

		//METHODS
	
		// overriding methods
	zstage->module_base.Discard 	= discard_PIMercuryC863;
	
	//---------------------------
	// Child Level 1: Zstage_type 
	
		// DATA
	
	zstage->zPos					= NULL;
	zstage->revertDirection			= 0;

		// METHODS
		
		
	
	//--------------------------
	// Child Level 2: PIMercuryC863_type
	
		// DATA
	
	
		// METHODS
	
		
	//----------------------------------------------------------
	if (!mod)
		return (DAQLabModule_type*) zstage;
	else
		return NULL;
}

void discard_PIMercuryC863 (DAQLabModule_type* mod)
{
	// discard PIMercuryC863_type specific data
	
	// discard Zstage_type specific data
	discard_Zstage (mod);
}
