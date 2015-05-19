//==============================================================================
//
// Title:		CoherentCham.c
// Purpose:		A short description of the implementation.
//
// Created on:	5/17/2015 at 12:15:00 AM by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "DAQLab.h" 		// include this first
#include "DAQLabUtility.h"
#include "CoherentCham.h"

//==============================================================================
// Constants

//==============================================================================
// Types

typedef struct {
	// SUPER, must be the first member to inherit from
	DAQLabModule_type 	baseClass;
	
	// CHILD class
	uInt16				wavelength;
	
} CoherentCham_type;

//==============================================================================
// Static global variables

//==============================================================================
// Static functions


static int 							LoadCfg 							(DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  moduleElement, ERRORINFO* xmlErrorInfo);

static int							SaveCfg								(DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ moduleElement, ERRORINFO* xmlErrorInfo);

static int							Load 								(DAQLabModule_type* mod, int workspacePanHndl);





//==============================================================================
// Global variables

//==============================================================================
// Global functions

DAQLabModule_type* initalloc_CoherentCham (DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl)
{
	CoherentCham_type* laser	= NULL;
	
	if (!mod) {
		laser = malloc (sizeof(CoherentCham_type));
		if (!laser) return NULL;
	} else
		laser = (CoherentCham_type*) mod;
		
	// initialize base class
	initalloc_DAQLabModule(&laser->baseClass, className, instanceName, workspacePanHndl);
	
	//---------------------------
	// Parent class: DAQLabModule_type 
	
		// DATA
			
		// METHODS
	
			// overriding methods
	laser->baseClass.Discard 		= discard_CoherentCham;
	laser->baseClass.Load			= Load;
	laser->baseClass.LoadCfg		= LoadCfg;
	laser->baseClass.SaveCfg		= SaveCfg;
	laser->baseClass.DisplayPanels	= NULL;
	
	//---------------------------
	// Child class: DAQLabModule_type
	
		// DATA
	laser->wavelength				= 0;	
	
	
}

void discard_CoherentCham (DAQLabModule_type** mod)
{
	CoherentCham_type* 		laser = *(CoherentCham_type**) mod;
	
	// discard CoherentCham_type child data
	
	// discard parent class
	discard_DAQLabModule(mod);
}

static int LoadCfg (DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  moduleElement, ERRORINFO* xmlErrorInfo)
{
	return 0;	
}


static int SaveCfg (DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ moduleElement, ERRORINFO* xmlErrorInfo)
{
	return 0;
}

static int Load (DAQLabModule_type* mod, int workspacePanHndl)
{
	CoherentCham_type* 		laser = (CoherentCham_type*) mod;
	
	// load panel resources
	zstage->controlPanHndl = LoadPanel(workspacePanHndl, MOD_Zstage_UI, ZStagePan);
	
	// connect module data and user interface callbackFn to all direct controls in the panel
	SetCtrlsInPanCBInfo(mod, ((Zstage_type*)mod)->uiCtrlsCB, zstage->controlPanHndl);
	
	// add panel callback function pointer and callback data
	SetPanelAttribute(zstage->controlPanHndl, ATTR_CALLBACK_FUNCTION_POINTER, UIPan_CB);
	SetPanelAttribute(zstage->controlPanHndl, ATTR_CALLBACK_DATA, mod);
	
	// add "Settings" menu bar item, callback data and callback function
	zstage->menuBarHndl = NewMenuBar(zstage->controlPanHndl);
	zstage->menuIDSettings = NewMenu(zstage->menuBarHndl, "Settings", -1);
	SetMenuBarAttribute(zstage->menuBarHndl, 0, ATTR_SHOW_IMMEDIATE_ACTION_SYMBOL, 0);
	SetMenuBarAttribute(zstage->menuBarHndl, zstage->menuIDSettings, ATTR_CALLBACK_DATA, zstage);
	SetMenuBarAttribute(zstage->menuBarHndl, zstage->menuIDSettings, ATTR_CALLBACK_FUNCTION_POINTER, SettingsMenu_CB);
	
	return 0;
}



