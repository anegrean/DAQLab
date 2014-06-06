//==============================================================================
//
// Title:		Zstage.c
// Purpose:		A short description of the implementation.
//
// Created on:	8-3-2014 at 18:33:04 by Adrian Negrean.
// Copyright:	. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files
#include "DAQLab.h" 		// include this first
#include <formatio.h>
#include <userint.h>
#include "DAQLabModule.h"
#include "UI_ZStage.h"
#include "Zstage.h"


//==============================================================================
// Constants

	// Zstage UI resource

#define MOD_Zstage_UI_ZStage "./Modules/Zstages/UI_ZStage.uir"

	// step sizes in [um] displayed in the ZStagePan_ZStepSize control
const double ZStepSizes[] = { 0.1, 0.2, 0.5, 1, 2, 5, 10, 
							  20, 50, 100, 200, 500, 1000 };  
	
//==============================================================================
// Types

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

static int Zstage_Load (DAQLabModule_type* mod, int workspacePanHndl);


//==============================================================================
// Global variables

//==============================================================================
// Global functions for module data management

int CVICALLBACK Zstage_UI_CB (int panel, int control, int event,
							  void *callbackData, int eventData1, int eventData2);



/// HIFN  Allocates memory and initializes a generic Zstage. No functionality implemented.
/// HIPAR mod/ if NULL both memory allocation and initialization must take place.
/// HIPAR mod/ if !NULL the function performs only an initialization of a DAQLabModule_type.
/// HIRET address of a DAQLabModule_type if memory allocation and initialization took place.
/// HIRET NULL if only initialization was performed. 
DAQLabModule_type*	initalloc_Zstage (DAQLabModule_type* mod)
{
	Zstage_type* zstage;
	
	if (!mod) {
		zstage = malloc (sizeof(Zstage_type));
		if (!zstage) return NULL;
	} else
		zstage = (Zstage_type*) mod;
		
	// initialize base class
	init_DAQLabModule(&zstage->module_base);
	//------------------------------------------------------------
	
	//---------------------------
	// Parent Level 0: DAQLabModule_type 
	
		// DATA
		
	zstage->module_base.name		= StrDup(MOD_Zstage_NAME);
	
		// METHODS
	
		// overriding methods
	zstage->module_base.Discard 	= discard_Zstage;
	zstage->module_base.Load		= Zstage_Load;
	
	//---------------------------
	// Child Level 1: Zstage_type 
	
		// DATA
	
	zstage->revertDirection			= 0;
	zstage->zPos					= NULL;

		// METHODS
		
		// assign default callback to UI_ZStage.uir panel
	zstage->uiCtrlsCB				= Zstage_UI_CB;
		// no functionality
	zstage->MoveZ					= NULL;
	
	
	//----------------------------------------------------------
	if (!mod)
		return (DAQLabModule_type*) zstage;
	else
		return NULL;
}


/// HIFN Discards a generic Zstage_type data but does not free the structure memory.
void discard_Zstage (DAQLabModule_type* mod)
{
	if (!mod) return;
	
	// discard Zstage_type specific data
	//Zstage_type* zstage = (Zstage_type*) mod;
	
	// discard DAQLabModule_type specific data
	discard_DAQLabModule(mod);
}


/// HIFN Loads ZStage specific resources. 
static int Zstage_Load (DAQLabModule_type* mod, int workspacePanHndl) 
{
	char	stepsizeName[50];
	
	int panHndl = LoadPanel(workspacePanHndl, MOD_Zstage_UI_ZStage, ZStagePan);
	
	// connect module data and user interface callbackFn to all direct controls in the panel
	SetCtrlsInPanCBInfo(mod, ((Zstage_type*)mod)->uiCtrlsCB, panHndl);
	
	// populate Z step sizes
	for (int i = 0; i < NumElem(ZStepSizes); i++) {
		Fmt(stepsizeName, "%s<%f[p1]", ZStepSizes[i]);
		InsertListItem(panHndl, ZStagePan_ZStepSize, -1, stepsizeName, ZStepSizes[i]);   
	}
	

	// display workspace pannels
	DisplayPanel(panHndl);
	
	
	return 0;

}

//==============================================================================
// Global functions for module specific UI management


int CVICALLBACK Zstage_UI_CB (int panel, int control, int event,
							  void *callbackData, int eventData1, int eventData2)
{
	Zstage_type* zstage = callbackData;
	
	switch (event)
	{
		case EVENT_COMMIT:
			
			double stepsize;
			double direction = 1;
			
			GetCtrlVal(panel, ZStagePan_ZStepSize, &stepsize); 
			if (zstage->revertDirection) direction = -1;
			
			switch (control) {
			
				case ZStagePan_MoveZUp:
					
					// move up relative to current position with selected step size
					if (zstage->MoveZ)
						(*zstage->MoveZ)	(zstage, ZSTAGE_MOVE_REL, direction*stepsize);
					
					
					break;
					
				case ZStagePan_MoveZDown:
					
					// move down relative to current position with selected step size
					if (zstage->MoveZ)
						(*zstage->MoveZ)	(zstage, ZSTAGE_MOVE_REL, direction*stepsize);
					
					
					break;
					
				case ZStagePan_ZAbsPos:
					
					break;
					
				case ZStagePan_ZRelPos:
					
					break;
					
				case ZStagePan_ZStepSize:
					
					break;
					
			}
			

			break;
	}
	return 0;
}
