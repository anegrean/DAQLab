//==============================================================================
//
// Title:		DAQLabModule.h
// Purpose:		Generic DAQLab module.
//
// Created on:	8-3-2014 at 17:13:45 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
// License:     This Source Code Form is subject to the terms of the Mozilla Public 
//              License v. 2.0. If a copy of the MPL was not distributed with this 
//              file, you can obtain one at https://mozilla.org/MPL/2.0/ . 
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
#include "ActiveXML.h"	   //?

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
		// List of Task Controllers provided by the module to the framework. Of TaskControl_type* 
	ListType		 			taskControllers;
		// List of Virtual Channels provided by the module to the framework. Of VChan_type* 
	ListType					VChans;
		// module configuration panel; one panel per module
	int							cfgPanHndl;	
		// workspace panel where module panels can be loaded
	int							workspacePanHndl;

	// METHODS
		
		// discards module-specific data  
	void	(* Discard) 		(DAQLabModule_type** self);	
	
		// loads module into DAQLab workspace
	int		(* Load)			(DAQLabModule_type* self, int workspacePanHndl, char** errorMsg);
	
		// loads config from XML node
	int		(* LoadCfg) 		(DAQLabModule_type* self, ActiveXMLObj_IXMLDOMElement_  moduleElement, ERRORINFO* xmlErrorInfo);
	
		// adds config to XML node
	int		(* SaveCfg)			(DAQLabModule_type* self, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ moduleElement, ERRORINFO* xmlErrorInfo);
	
		// if visibleFlag is TRUE, displays or otherwise hides all module panels
	int		(* DisplayPanels)	(DAQLabModule_type* self, BOOL visibleFlag);

};

//==============================================================================
// Constants


//==============================================================================
// External variables

//==============================================================================
// Global functions

	// module creation/discarding
DAQLabModule_type*				initalloc_DAQLabModule				(DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl);
void 							discard_DAQLabModule 				(DAQLabModule_type** mod);

	// removes modules
void							DAQLabModule_empty					(ListType* modules);


#ifdef __cplusplus
    }
#endif

#endif  /* ndef __DAQLabModule_H__ */
