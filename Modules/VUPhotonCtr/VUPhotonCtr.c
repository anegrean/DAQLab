//==============================================================================
//
// Title:		VUPhotonCtr.c
// Purpose:		A short description of the implementation.
//
// Created on:	17-6-2014 at 20:51:00 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files
#include "DAQLab.h" 		// include this first 
#include "VUPhotonCtr.h"

//==============================================================================
// Constants

// VUPhotonCtr UI resource

#define MOD_VUPhotonCtr_UI 		"./Modules/VUPhotonCtr/UI_VUPhotonCtr.uir"


//==============================================================================
// Types

typedef struct {
	int		chanPanHndl;
} VUPhotonCtr_UIChanels_type;

//==============================================================================
// Module implementation

struct VUPhotonCtr {
	
	// SUPER, must be the first member to inherit from
	
	DAQLabModule_type 	baseClass;
	
	// DATA
	
	int					mainPanHndl; 
	
	int					statusPanHndl;
		// 
	ListType			UIChanels;
	
	// METHODS
	
		// Callback to install on controls from UI_VUPhotonCtr.uir
		// Default set to VUPhotonCtr_UI_CB in VUPhotonCtr.c
		// Override: Optional, to change UI panel behavior. 
	CtrlCallbackPtr		uiCtrlsCB;
	
		// Callback to install on the panels from UI_VUPhotonCtr.uir
	PanelCallbackPtr	uiPanelCB;
	
}

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

static int					VUPhotonCtr_InitHardware 		(VUPhotonCtr_type* vupc); 

static int					VUPhotonCtr_Load 				(DAQLabModule_type* mod, int workspacePanHndl);

static int					VUPhotonCtr_DisplayPanels		(DAQLabModule_type* mod, BOOL visibleFlag);

static int CVICALLBACK 		VUPhotonCtr_UICtrls_CB 			(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 		VUPhotonCtr_UIPan_CB 			(int panel, int event, void *callbackData, int eventData1, int eventData2);



//-----------------------------------------
// VUPhotonCtr Task Controller Callbacks
//-----------------------------------------

static FCallReturn_type*	VUPhotonCtr_ConfigureTC			(TaskControl_type* taskControl, BOOL const* abortFlag);
static FCallReturn_type*	VUPhotonCtr_IterateTC			(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type*	VUPhotonCtr_StartTC				(TaskControl_type* taskControl, BOOL const* abortFlag);
static FCallReturn_type*	VUPhotonCtr_DoneTC				(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type*	VUPhotonCtr_StoppedTC			(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type* 	VUPhotonCtr_ResetTC 			(TaskControl_type* taskControl, BOOL const* abortFlag); 
static FCallReturn_type* 	VUPhotonCtr_ErrorTC 			(TaskControl_type* taskControl, char* errorMsg, BOOL const* abortFlag);
static FCallReturn_type*	VUPhotonCtr_EventHandler		(TaskControl_type* taskControl, TaskStates_type taskState, size_t currentIteration, void* eventData, BOOL const* abortFlag);  


//==============================================================================
// Global variables

//==============================================================================
// Global functions for module data management

/// HIFN  Allocates memory and initializes the VU Photon Counter module.
/// HIPAR mod/ if NULL both memory allocation and initialization must take place.
/// HIPAR mod/ if !NULL the function performs only an initialization of a DAQLabModule_type.
/// HIRET address of a DAQLabModule_type if memory allocation and initialization took place.
/// HIRET NULL if only initialization was performed. 
DAQLabModule_type*	initalloc_VUPhotonCtr (DAQLabModule_type* mod, char className[], char instanceName[]) 
{
	VUPhotonCtr_type* vupc;
	
	if (!mod) {
		vupc = malloc (sizeof(VUPhotonCtr_type));
		if (!vupc) return NULL;
	} else
		vupc = (VUPhotonCtr_type*) mod;
		
	// initialize base class
	initalloc_DAQLabModule(&vupc->baseClass, className, instanceName);
	//------------------------------------------------------------
	
	//---------------------------
	// Parent Level 0: DAQLabModule_type 
	
		// DATA
	vupc->baseClass.taskControl		= init_TaskControl_type (MOD_VUPhotonCtr_NAME, VUPhotonCtr_ConfigureTC, VUPhotonCtr_IterateTC, VUPhotonCtr_StartTC, VUPhotonCtr_ResetTC,
												 				 VUPhotonCtr_DoneTC, VUPhotonCtr_StoppedTC, NULL, VUPhotonCtr_EventHandler, VUPhotonCtr_ErrorTC);   
		// connect VUPhotonCtr module data to Task Controller
	SetTaskControlModuleData(vupc->baseClass.taskControl, vupc);
		// METHODS
	
		// overriding methods
	vupc->baseClass.Discard 		= discard_VUPhotonCtr;
	vupc->baseClass.Load			= VUPhotonCtr_Load;
	vupc->baseClass.LoadCfg			= NULL;//VUPhotonCtr_LoadCfg;
	vupc->baseClass.DisplayPanels	= VUPhotonCtr_DisplayPanels;
	
	//---------------------------
	// Child Level 1: VUPhotonCtr_type 
	
		// DATA
	vupc->mainPanHndl			= 0;
	vupc->statusPanHndl				= 0;
	
		// METHODS
		
		// assign default controls callback to UI_ZStage.uir panel
	vupc->uiCtrlsCB				= VUPhotonCtr_UICtrls_CB;
		// assign default panel callback to UI_VUPhotonCtr.uir
	vupc->uiPanelCB				= VUPhotonCtr_UIPan_CB;
	
		
	
	//----------------------------------------------------------
	if (!mod)
		return (DAQLabModule_type*) vupc;
	else
		return NULL;
	
}

/// HIFN Discards VUPhotonCtr_type data but does not free the structure memory.
void discard_VUPhotonCtr (DAQLabModule_type** mod)
{
	VUPhotonCtr_type* 				vupc = (VUPhotonCtr_type*) (*mod);
	
	if (!vupc) return;
	
	// discard VUPhotonCtr_type specific data
	
	if (vupc->mainPanHndl)
		DiscardPanel(vupc->mainPanHndl);
	
	if (vupc->statusPanHndl)
		DiscardPanel(vupc->statusPanHndl);
	
	// discard DAQLabModule_type specific data
	discard_DAQLabModule(mod);
}

static int	VUPhotonCtr_InitHardware (VUPhotonCtr_type* vupc)
{
	
}

static int VUPhotonCtr_Load (DAQLabModule_type* mod, int workspacePanHndl)
{
	VUPhotonCtr_type* 	vupc 				= (VUPhotonCtr_type*) mod; 
	
	// init VU Photon Counting hardware
	if (VUPhotonCtr_InitHardware((VUPhotonCtr_type*) mod) < 0) return -1; 
	
	// load panel resources
	vupc->mainPanHndl = LoadPanel(workspacePanHndl, MOD_VUPhotonCtr_UI, VUPCMain);
	
	// connect module data and user interface callbackFn to all direct controls in the panel
	SetCtrlsInPanCBInfo(mod, ((VUPhotonCtr_type*)mod)->uiCtrlsCB, vupc->mainPanHndl);
	
	// add panel callback function pointer and callback data
	SetPanelAttribute(vupc->mainPanHndl, ATTR_CALLBACK_FUNCTION_POINTER, VUPhotonCtr_UIPan_CB);
	SetPanelAttribute(vupc->mainPanHndl, ATTR_CALLBACK_DATA, mod);
	
	// change panel title to module instance name
	SetPanelAttribute(vupc->mainPanHndl, ATTR_TITLE, mod->instanceName);
	
	
	return 0;
}

static int CVICALLBACK VUPhotonCtr_UICtrls_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	VUPhotonCtr_type* 	vupc = callbackData;
	
}

static int CVICALLBACK VUPhotonCtr_UIPan_CB (int panel, int event, void *callbackData, int eventData1, int eventData2)
{
	VUPhotonCtr_type* 	vupc = callbackData;
	
}


//-----------------------------------------
// VUPhotonCtr Task Controller Callbacks
//-----------------------------------------

static FCallReturn_type* VUPhotonCtr_ConfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "", 0);
}

static FCallReturn_type* VUPhotonCtr_IterateTC (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);
	
	
	return init_FCallReturn_type(0, "", "", 0);
}

static FCallReturn_type* VUPhotonCtr_StartTC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "", 0);
}

static FCallReturn_type* VUPhotonCtr_DoneTC (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "", 0);
}
static FCallReturn_type* VUPhotonCtr_StoppedTC (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "", 0);
}

static FCallReturn_type* VUPhotonCtr_ResetTC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "", 0);
}

static FCallReturn_type* VUPhotonCtr_ErrorTC (TaskControl_type* taskControl, char* errorMsg, BOOL const* abortFlag)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "", 0);
}

static FCallReturn_type* VUPhotonCtr_EventHandler (TaskControl_type* taskControl, TaskStates_type taskState, size_t currentIteration, void* eventData, BOOL const* abortFlag)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "", 0);
}
