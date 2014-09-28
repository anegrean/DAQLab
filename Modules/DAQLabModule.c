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

//---------------------------------------------------------------------------------
	// Predefined DAQLabModule errors and warnings to be used with DAQLabModule_Msg function.
	//
	// DAQLabModule_Msg is a function with variable arguments and types which depend on
	// the particular warning or error message. When calling this function with a 
	// certain error code from below, pass in the additional parameters with 
	// required data type.
	//
typedef enum _DAQLabModuleMessageID {
												// parameter types (data1, data2, data3, data4)
	// errors									// to pass in DAQLab_Msg
	DAQLABMODULES_MSG_ERR_CFG_SAME_XML_MODULE_NAMES,		// char* module name
	DAQLABMODULES_MSG_ERR_ACTIVEXML,						// CAObjHandle* object handle, HRESULT* error code, ERRORINFO* additional error info
	
	// warnings									
	DAQLABMODULES_MSG_WARN_MODULE_CFG_NOT_FOUND,			// CAObjHandle* object handle, HRESULT* error code, ERRORINFO* additional error info 
	
	// success
	DAQLABMODULES_MSG_OK_MODULE_CFG_LOADED					// char* module name

} DAQLabModuleMessageID;
//----------------------------------------------------------------------------------


//==============================================================================
// Constants

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

static void 	DAQLabModule_Msg 				(DAQLabModuleMessageID msgID, void* data1, void* data2, void* data3, void* data4);


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
	
	mod->className			= StrDup(className);
	mod->instanceName		= StrDup(instanceName);
	mod->cfgPanHndl			= 0; 
	//init
	mod->taskControllers	= 0;
	mod->VChans				= 0;
	mod->taskControllers	= ListCreate(sizeof(TaskControl_type*));
	if (!mod->taskControllers) 	goto Error;
	mod->VChans				= ListCreate(sizeof(VChan_type*));
	if (!mod->VChans) 			goto Error;
	
	
	
	// METHODS
	
	mod->Discard		= discard_DAQLabModule;
	mod->Load			= NULL;
	mod->LoadCfg		= NULL;
	mod->SaveCfg		= NULL;
	mod->DisplayPanels	= NULL;
	
	return mod;
	
Error:
	
	OKfree(mod->className); 
	OKfree(mod->instanceName); 
	if (mod->taskControllers) ListDispose(mod->taskControllers);
	if (mod->VChans) ListDispose(mod->VChans); 
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
	
	if (!(*modules)) return;
	
	for (size_t i = 1; i <= ListNumItems(*modules); i++) {
		modPtrPtr = ListGetPtrToItem(*modules, i);
		// call discard method specific to each module
		(*(*modPtrPtr)->Discard)(modPtrPtr);
	}
	
	ListDispose(*modules);
	*modules = 0;
}

/// HIFN Displays predefined error and warning messages in the main workspace log panel.
/// HIPAR msgID/ If required, pass in additional data to format the message.
static void DAQLabModule_Msg (DAQLabModuleMessageID msgID, void* data1, void* data2, void* data3, void* data4)
{
	char buff[1000]="";
	char buffCA[1000]="";
	
	switch (msgID) {
		
		case DAQLABMODULES_MSG_OK_MODULE_CFG_LOADED:
			
			DLMsg("Module ", 0);
			DLMsg((char*)data1, 0);
			DLMsg(" configuration loaded.\n\n", 0);

			break;
		
		case DAQLABMODULES_MSG_WARN_MODULE_CFG_NOT_FOUND:
			
			DLMsg("Warning: Could not find ", 1);
			DLMsg((char*)data1, 0);
			DLMsg(" configuration. \n\n", 0);
			
			break;
			
		case DAQLABMODULES_MSG_ERR_CFG_SAME_XML_MODULE_NAMES:
			Fmt(buff, "Error: Multiple identical module XML names \"%s\" are not allowed. \n\n", (char*)data1);
			DLMsg(buff, 1);
			
			break;
			
		case DAQLABMODULES_MSG_ERR_ACTIVEXML:
			
			CA_GetAutomationErrorString(*(HRESULT*)data2, buffCA, sizeof(buffCA));
			DLMsg(buffCA, 1);
			DLMsg("\n", 0);
			// additional error info
			if (*(HRESULT*)data2 == DISP_E_EXCEPTION) {
				CA_DisplayErrorInfo(*(CAObjHandle*)data1, NULL, *(HRESULT*)data2, (ERRORINFO*)data3); 
				DLMsg("\n\n", 0);
			} else
				DLMsg("\n", 0);
			
			break;
			
	}
	
	
}
