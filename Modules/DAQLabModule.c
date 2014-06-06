//==============================================================================
//
// Title:		DAQLabModule.c
// Purpose:		A short description of the implementation.
//
// Created on:	8-3-2014 at 17:13:45 by Adrian Negrean.
// Copyright:	. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "DAQLab.h" 		// include this first
#include <ansi_c.h>
#include "DAQLabModule.h"  
//==============================================================================
// Include modules

#include "Zstage.h"
#include "PIMercuryC863.h"


//==============================================================================
// Types

// Constant function pointer that is used to launch the initialization 
// of different DAQLabModules 
typedef DAQLabModule_type* (* const ModuleInitAllocFunction ) (DAQLabModule_type* mod);


//==============================================================================
// Constants

		
ModuleInitAllocFunction DAQLabModules_InitFunctions[] = {
	
	initalloc_Zstage,
	initalloc_PIMercuryC863
	
};

//==============================================================================
// Static global variables

//==============================================================================
// Static functions


//==============================================================================
// Global variables

//==============================================================================
// Global functions


/// HIFN Allocates memory and initializes a generic DAQLabModule
void init_DAQLabModule (DAQLabModule_type* mod)
{
	// DATA
	
	mod->name			= NULL;
	mod->cfgPanHndl		= 0;
	mod->ctrlPanelHndls	= ListCreate(sizeof(int));
	
	// METHODS
	
	mod->Discard		= discard_DAQLabModule;
	mod->Load			= NULL;
	mod->LoadCfg		= NULL;
	mod->SaveCfg		= NULL;
}

/// HIFN Deallocates a generic DAQLabModule 
void discard_DAQLabModule (DAQLabModule_type* mod) 
{
	if (!mod) return;
	
	OKfree(mod->name);
}

/// HIFN  Returns a list of available modules that are loaded from DAQLabModules_InitFunctions[] declared in DAQLabModule.c 
/// HIRET Returns non-zero list handle if successful
/// HIRET 0 if error occured
ListType DAQLabModule_populate (void)
{
	ListType modules = ListCreate(sizeof(DAQLabModule_type*));
	if (!modules) return 0;
	
	DAQLabModule_type* modptr;
	for (int i = 0; i < NumElem(DAQLabModules_InitFunctions); i++) {
		modptr = (*DAQLabModules_InitFunctions[i])(NULL);
		ListInsertItem(modules, &modptr, END_OF_LIST);
	}
	
	return modules;
}

void DAQLabModule_empty	(ListType* modules)
{
	DAQLabModule_type** modptr;
	
	if (!(*modules)) return;
	
	for (int i = 1; i <= ListNumItems(*modules); i++) {
		modptr = ListGetPtrToItem(*modules, i);
		// call discard method specific to each module
		(*(*modptr)->Discard)(*modptr);
		// free dynamically allocated memory for each module
		OKfree(*modptr);
	}
	
	ListDispose(*modules);
	*modules = 0;
}

