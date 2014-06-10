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
const double ZStepSizes[] = { 0.1, 0.2, 0.5, 1, 1.5, 2, 5, 10, 
							  20, 50, 100, 200, 500, 1000 };  
	
//==============================================================================
// Types

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//-----------------------------------------
// ZStage Task Controller Callbacks
//-----------------------------------------

static FCallReturn_type*	Zstage_ConfigureTC				(TaskControl_type* taskControl, BOOL const* abortFlag);
static FCallReturn_type*	Zstage_IterateTC				(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type*	Zstage_StartTC					(TaskControl_type* taskControl, BOOL const* abortFlag);
static FCallReturn_type*	Zstage_DoneTC					(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type*	Zstage_StoppedTC				(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type* 	Zstage_ResetTC 					(TaskControl_type* taskControl, BOOL const* abortFlag); 
static FCallReturn_type* 	Zstage_ErrorTC 					(TaskControl_type* taskControl, char* errorMsg, BOOL const* abortFlag);
static FCallReturn_type*	Zstage_EventHandler				(TaskControl_type* taskControl, TaskStates_type taskState, size_t currentIteration, void* eventData, BOOL const* abortFlag);  


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
	zstage->module_base.XMLname		= StrDup(MOD_Zstage_XMLTAG);
	zstage->module_base.taskControl	= init_TaskControl_type (MOD_Zstage_NAME, Zstage_ConfigureTC, Zstage_IterateTC, Zstage_StartTC, Zstage_ResetTC,
															 Zstage_DoneTC, Zstage_StoppedTC, NULL, Zstage_EventHandler,Zstage_ErrorTC);   
		// connect ZStage module data to Task Controller
	SetTaskControlModuleData(zstage->module_base.taskControl, zstage);
	
		// METHODS
	
		// overriding methods
	zstage->module_base.Discard 	= discard_Zstage;
	zstage->module_base.Load		= Zstage_Load;
	zstage->module_base.LoadCfg		= Zstage_LoadCfg;
	
	//---------------------------
	// Child Level 1: Zstage_type 
	
		// DATA
	
	zstage->zPos					= NULL; 
	zstage->revertDirection			= 0;
	zstage->zHomePos				= NULL;
	zstage->zULimPos				= NULL;
	zstage->zLLimPos				= NULL;
	zstage->zStackStep				= 0;
	


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
	Zstage_type* zstage = (Zstage_type*) mod;
	
	OKfree(zstage->zPos);
	OKfree(zstage->zHomePos);
	OKfree(zstage->zULimPos);
	OKfree(zstage->zLLimPos);

	// discard DAQLabModule_type specific data
	discard_DAQLabModule(mod);
}


/// HIFN Loads ZStage specific resources. 
int Zstage_Load (DAQLabModule_type* mod, int workspacePanHndl) 
{
	char	stepsizeName[50];
	
	// load panel resources
	int panHndl = LoadPanel(workspacePanHndl, MOD_Zstage_UI_ZStage, ZStagePan);
	
	// place panel handles in module structure
	ListInsertItem(mod->ctrlPanelHndls, &panHndl, END_OF_LIST);
	
	// connect module data and user interface callbackFn to all direct controls in the panel
	SetCtrlsInPanCBInfo(mod, ((Zstage_type*)mod)->uiCtrlsCB, panHndl);
	
	// populate Z step sizes
	for (int i = 0; i < NumElem(ZStepSizes); i++) {
		Fmt(stepsizeName, "%s<%f[p1]", ZStepSizes[i]);
		InsertListItem(panHndl, ZStagePan_ZStepSize, -1, stepsizeName, ZStepSizes[i]);   
	}
	
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
					
				case ZStagePan_GoHomePos:
					
					break;
					
				case ZStagePan_ZAbsPos:
					
					break;
					
				case ZStagePan_ZRelPos:
					
					break;
					
				case ZStagePan_ZStepSize:
					
					break;
					
				case ZStagePan_Cycle_RelStart:
				case ZStagePan_Cycle_RelEnd:
					
					break;
					
				case ZStagePan_SetStartPos:
					
					break;
					
				case ZStagePan_SetEndPos:
					
					break;
					
				case ZStagePan_AddHomePos:
					
					break;
					
				case ZStagePan_UpdateHomePos:
					
					break;
					
			}
			

			break;
	}
	return 0;
}

//-----------------------------------------
// ZStage Task Controller Callbacks
//-----------------------------------------

static FCallReturn_type* Zstage_ConfigureTC	(TaskControl_type* taskControl, BOOL const* abortFlag)
{
	Zstage_type* 		zstage = GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "", 0);
}

static FCallReturn_type* Zstage_IterateTC (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	Zstage_type* 		zstage = GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "", 0);
}

static FCallReturn_type* Zstage_StartTC	(TaskControl_type* taskControl, BOOL const* abortFlag)
{
	Zstage_type* 		zstage = GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "", 0);
}

static FCallReturn_type* Zstage_DoneTC (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	Zstage_type* 		zstage = GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "", 0);
}
static FCallReturn_type* Zstage_StoppedTC (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	Zstage_type* 		zstage = GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "", 0);
}

static FCallReturn_type* Zstage_ResetTC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	Zstage_type* 		zstage = GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "", 0);
}

static FCallReturn_type* Zstage_ErrorTC (TaskControl_type* taskControl, char* errorMsg, BOOL const* abortFlag)
{
	Zstage_type* 		zstage = GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "", 0);
}

static FCallReturn_type* Zstage_EventHandler (TaskControl_type* taskControl, TaskStates_type taskState, size_t currentIteration, void* eventData, BOOL const* abortFlag)
{
	Zstage_type* 		zstage = GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "", 0);
}


int Zstage_LoadCfg (DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  DAQLabCfg_RootElement) 
{
	Zstage_type*  	zstage = (Zstage_type*) mod; 
	
	return DAQLabModule_LoadCfg(mod, DAQLabCfg_RootElement);
}
