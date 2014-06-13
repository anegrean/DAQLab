//==============================================================================
//
// Title:		DAQLabModule.h
// Purpose:		A short description of the interface.
//
// Created on:	8-3-2014 at 17:13:45 by Adrian Negrean.
// Copyright:	. All Rights Reserved.
//
//==============================================================================

#ifndef __DAQLabModule_H__
#define __DAQLabModule_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "cvidef.h"
#include <toolbox.h>
#include "TaskController.h"

//==============================================================================
// Types
		
typedef struct DAQLabModule 	DAQLabModule_type;

//==============================================================================
// Module implementation

struct DAQLabModule {
	
	// DATA
	
		// module class name
	char*						className;
		// module instance name
	char*						instanceName;
		// module XML node
	ActiveXMLObj_IXMLDOMNode_	XMLNode;
		// Task Controller for the module, if any
	TaskControl_type* 			taskControl;
		// module configuration panel; one panel per module
	int							cfgPanHndl;									
		// module control panel handles of int type to be loaded in the DAQLab workspace
	ListType					ctrlPanelHndls;	
	
	// METHODS
		
		// discards module-specific data  
	void	(* Discard ) 		(DAQLabModule_type* self);	
	
		// loads module into DAQLab workspace
	int		(* Load )			(DAQLabModule_type* self, int workspacePanHndl);
	
		// loads config from XML node
	int		(* LoadCfg ) 		(DAQLabModule_type* self, ActiveXMLObj_IXMLDOMElement_  DAQLabCfg_RootElement);
	
		// adds config to XML node
	int		(* SaveCfg )		(DAQLabModule_type* self, ActiveXMLObj_IXMLDOMElement_  DAQLabCfg_RootElement);

};

//==============================================================================
// Constants


//==============================================================================
// External variables

//==============================================================================
// Global functions

	// module creation/discarding
void							init_DAQLabModule					(DAQLabModule_type* mod);
void 							discard_DAQLabModule 				(DAQLabModule_type* mod);

	// removes modules
void							DAQLabModule_empty					(ListType* modules);

	// displays or hides all the workspace panels of a module

void							DAQLabModule_DisplayWorkspacePanels (DAQLabModule_type* mod, BOOL visible);

	// loads generic DAQLab module configuration from an XML file

//int								DAQLabModule_LoadCfg				(DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  DAQLabCfg_RootElement);			


#ifdef __cplusplus
    }
#endif

#endif  /* ndef __DAQLabModule_H__ */
