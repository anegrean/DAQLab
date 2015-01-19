//==============================================================================
//
// Title:		DAQLabModule.c
// Purpose:		A short description of the implementation.
//
// Created on:	8-3-2014 at 17:13:45 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "DAQLab.h" 		// include this first
#include <formatio.h> 
#include <ansi_c.h>
#include "DAQLabModule.h"  

//==============================================================================
// Types

//==============================================================================
// Constants

//==============================================================================
// Static global variables

//==============================================================================
// Static functions


//==============================================================================
// Global variables

//==============================================================================
// Global functions


/// HIFN Allocates memory and initializes a generic DAQLabModule
DAQLabModule_type* initalloc_DAQLabModule (DAQLabModule_type* mod, char className[], char instanceName[])
{
	if (!mod) {
		mod = malloc (sizeof(DAQLabModule_type));
		if (!mod) return NULL;
	} 
	
	// DATA
	
	mod->className					= StrDup(className);
	mod->instanceName				= StrDup(instanceName);
	mod->cfgPanHndl					= 0; 
	//init
	mod->taskControllers			= 0;
	mod->VChans						= 0;
	
	if ( !(mod->taskControllers		= ListCreate(sizeof(TaskControl_type*))) ) goto Error;
	if ( !(mod->VChans				= ListCreate(sizeof(VChan_type*))) ) goto Error;
	
	// METHODS
	
	mod->Discard					= discard_DAQLabModule;
	mod->Load						= NULL;
	mod->LoadCfg					= NULL;
	mod->SaveCfg					= NULL;
	mod->DisplayPanels				= NULL;
	
	return mod;
	
Error:
	
	OKfree(mod->className); 
	OKfree(mod->instanceName); 
	if (mod->taskControllers) 	ListDispose(mod->taskControllers);
	if (mod->VChans) 			ListDispose(mod->VChans);
	
	OKfree(mod); 
	
	return NULL;
}

/// HIFN Deallocates a generic DAQLabModule 
void discard_DAQLabModule (DAQLabModule_type** mod) 
{
	if (!*mod) return;
	
	OKfree((*mod)->className);
	OKfree((*mod)->instanceName);
	
	ListDispose((*mod)->taskControllers);
	ListDispose((*mod)->VChans);
	
	if ((*mod)->cfgPanHndl)
		DiscardPanel((*mod)->cfgPanHndl);
	
	OKfree(*mod);
}

void DAQLabModule_empty	(ListType* modules)
{
	DAQLabModule_type** modPtrPtr;
	size_t				nummodules =  ListNumItems(*modules);
	
	if (!(*modules)) return;
	
	for (size_t i = 1; i <= nummodules; i++) {
		modPtrPtr = ListGetPtrToItem(*modules, i);   
		// call discard method specific to each module
		(*(*modPtrPtr)->Discard)(modPtrPtr);
		
	}
	//do NOT remove items in previous loop; List gets renumbered because of the removed items
	for (size_t i = 1; i <= nummodules; i++) {  
		// remove module from list
		ListRemoveItem(*modules, 0, i);	
	}
	
	ListDispose(*modules);
	*modules = 0;
}
