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
#include "DAQLab.h" 		// include this first 
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

static int PIMercuryC863_Load (DAQLabModule_type* mod, int workspacePanHndl);

static int PIMercuryC863_LoadCfg (DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  DAQLabCfg_RootElement);

static int PIMercuryC863_Move (Zstage_type* self, Zstage_move_type moveType, double moveVal);

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
	
		// updating name
	OKfree(zstage->module_base.className);
	zstage->module_base.className	= StrDup(MOD_PIMercuryC863_NAME);
		
	
		//METHODS
	
		// overriding methods
	zstage->module_base.Discard 	= discard_PIMercuryC863;
	zstage->module_base.Load 		= PIMercuryC863_Load;
	zstage->module_base.LoadCfg		= NULL; //PIMercuryC863_LoadCfg;
		
	
	//---------------------------
	// Child Level 1: Zstage_type 
	
		// DATA
	
	
		// METHODS
	
		// overriding methods
	zstage->MoveZ					= PIMercuryC863_Move;  
		
		
	
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

/// HIFN Loads PIMercuryC863 motorized stage specific resources. 
static int PIMercuryC863_Load (DAQLabModule_type* mod, int workspacePanHndl)
{
	// load generic Z stage resources
	Zstage_Load (mod, workspacePanHndl); 
	return 0;
	
}

/// HIFN Moves a PIMercuryC863 motorized stage 
static int PIMercuryC863_Move (Zstage_type* self, Zstage_move_type moveType, double moveVal)
{
	return 0;
}

/*
static int PIMercuryC863_LoadCfg (DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  DAQLabCfg_RootElement)
{
	PIMercuryC863_type* 	PIMzstage	= (PIMercuryC863_type*) mod;
	
	return Zstage_LoadCfg(mod, DAQLabCfg_RootElement); 
}
*/

