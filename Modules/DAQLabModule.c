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
void init_DAQLabModule (DAQLabModule_type* mod)
{
	// DATA
	
	mod->name			= NULL;
	mod->XMLname		= NULL;
	mod->XMLNode		= 0;
	mod->cfgPanHndl		= 0;
	mod->ctrlPanelHndls	= ListCreate(sizeof(int));
	
	// METHODS
	
	mod->Discard		= discard_DAQLabModule;
	mod->Load			= NULL;
	mod->LoadCfg		= DAQLabModule_LoadCfg;
	mod->SaveCfg		= NULL;
}

/// HIFN Deallocates a generic DAQLabModule 
void discard_DAQLabModule (DAQLabModule_type* mod) 
{
	int* panHndlPtr;
	
	if (!mod) return;
	
	if (mod->cfgPanHndl)
		DiscardPanel(mod->cfgPanHndl);
	
	for (size_t i = 1; i <= ListNumItems(mod->ctrlPanelHndls); i++) {
		panHndlPtr = ListGetPtrToItem(mod->ctrlPanelHndls, i);
		DiscardPanel(*panHndlPtr);
	}
	ListDispose(mod->ctrlPanelHndls);
	
	OKfree(mod->name);
	OKfree(mod->XMLname);
}

void DAQLabModule_empty	(ListType* modules)
{
	DAQLabModule_type** modptr;
	
	if (!(*modules)) return;
	
	for (size_t i = 1; i <= ListNumItems(*modules); i++) {
		modptr = ListGetPtrToItem(*modules, i);
		// call discard method specific to each module
		(*(*modptr)->Discard)(*modptr);
		// free dynamically allocated memory for each module
		OKfree(*modptr);
	}
	
	ListDispose(*modules);
	*modules = 0;
}

void DAQLabModule_DisplayWorkspacePanels (DAQLabModule_type* mod, BOOL visible)
{
	int* panHndlPtr;
	
	if (!mod) return;
	
	for (size_t i = 1; i <= ListNumItems(mod->ctrlPanelHndls); i++) {
		panHndlPtr = ListGetPtrToItem(mod->ctrlPanelHndls, i);
		if (visible)
			DisplayPanel(*panHndlPtr);
		else
			HidePanel(*panHndlPtr);
	}
}

/// HIFN  Loads configuration data for a generic DAQLab module from an XML file.
/// HIRET If configuration is found, returns DAQLab module XML Element node handle of intptr_t type.
/// HIRET 0 if configuration was not found or error occured.
int DAQLabModule_LoadCfg (DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  DAQLabCfg_RootElement)
{
	HRESULT								xmlerror;
	ERRORINFO							xmlERRINFO;
	ActiveXMLObj_IXMLDOMNodeList_		nodelist	= 0;
	long								nelem		= 0;
	
	// get list of nodes with the same XML name as the module XML name
	if ( (xmlerror = ActiveXML_IXMLDOMElement_getElementsByTagName(DAQLabCfg_RootElement, &xmlERRINFO, mod->XMLname, &nodelist)) < 0 ) {
		DAQLabModule_Msg(DAQLABMODULES_MSG_ERR_ACTIVEXML, &DAQLabCfg_RootElement, &xmlerror, &xmlERRINFO, 0); 
		return -1;
	} 
	
	// get number of nodes in the list
	if ( (xmlerror = ActiveXML_IXMLDOMNodeList_Getlength(nodelist, &xmlERRINFO, &nelem)) < 0 ) {
		DAQLabModule_Msg(DAQLABMODULES_MSG_ERR_ACTIVEXML, &nodelist, &xmlerror, &xmlERRINFO, 0);
		return -1;
	} 
	
	// make sure there is exactly one such element in the list
	if (!nelem) {
		DAQLabModule_Msg(DAQLABMODULES_MSG_WARN_MODULE_CFG_NOT_FOUND, mod->name, 0, 0, 0);  
		return -1;
	} else
		if (nelem > 1) {
			DAQLabModule_Msg(DAQLABMODULES_MSG_ERR_CFG_SAME_XML_MODULE_NAMES, mod->XMLname, 0, 0, 0);
			return -1;
		} else
			DAQLabModule_Msg(DAQLABMODULES_MSG_OK_MODULE_CFG_LOADED, mod->XMLname, 0, 0, 0);
	
	// get single DAQLab module node from list (0-based index)
	if ( (xmlerror = ActiveXML_IXMLDOMNodeList_Getitem(nodelist, &xmlERRINFO, 0, &mod->XMLNode)) < 0 ) {
		DAQLabModule_Msg(DAQLABMODULES_MSG_ERR_ACTIVEXML, &nodelist, &xmlerror, &xmlERRINFO, 0);
		return -1;
	} 
	 
	return 0;
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
