//==============================================================================
//
// Title:		Zstage.c
// Purpose:		Generic Z stage.
//
// Created on:	8-3-2014 at 18:33:04 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
// License:     This Source Code Form is subject to the terms of the Mozilla Public 
//              License v. 2.0. If a copy of the MPL was not distributed with this 
//              file, you can obtain one at https://mozilla.org/MPL/2.0/ . 
//
//==============================================================================

//==============================================================================
// Include files
#include "DAQLab.h" 		// include this first
#include "DAQLabModule.h"
#include <formatio.h>
#include "toolbox.h"
#include <userint.h>
#include "DAQLabUtility.h"
#include "UI_ZStage.h"
#include "Zstage.h"


//==============================================================================
// Constants

	// Zstage UI resource

#define MOD_Zstage_UI 				"./Modules/Zstages/UI_ZStage.uir"

	// maximum entry length in characters for reference positions
#define MAX_REF_POS_LENGTH			50

	// number of digits after the decimal point used to display stage position in [um]
#define POS_DISPLAY_PRECISION		1

	// step sizes in [um] displayed in the ZStagePan_ZStepSize control
const double 	ZStepSizes[] 		= { 0.1, 0.2, 0.5, 1, 1.5, 2, 5, 10, 
							  			20, 50, 100, 200, 500, 1000 };

	// controls from UI_Zstage.uir that will be dimmed or undimmed when the stage is stepping (RUNNING state)
const int 		UIZStageCtrls[] = {ZStagePan_MoveZUp, ZStagePan_MoveZDown, ZStagePan_ZAbsPos, ZStagePan_ZRelPos, 
								   ZStagePan_ZStepSize, ZStagePan_RefPosList, ZStagePan_AddRefPos, ZStagePan_StartAbsPos, 
								   ZStagePan_EndRelPos };

	
//==============================================================================
// Types

typedef struct {
	char*	name;			// name of reference position
	double	val;			// position in [mm]
} RefPosition_type;

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

static int							DisplayPanels					(DAQLabModule_type* mod, BOOL visibleFlag); 

static int							ChangeLEDStatus					(ZStage_type* zstage, Zstage_LED_type status);

static int							UpdatePositionDisplay			(ZStage_type* zstage);

static int							UpdateZSteps					(ZStage_type* zstage);

static void 						SetStepCounter					(ZStage_type* zstage, size_t val);

static void							DimWhenRunning					(ZStage_type* zstage, BOOL dimmed);

static RefPosition_type*			init_RefPosition_type			(char refName[], double refVal);

static void							discard_RefPosition_type		(RefPosition_type** refPosPtr);

static BOOL 						ValidateRefPosName				(char inputStr[], void* dataPtr);  

static int CVICALLBACK 				UICtrls_CB 						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 				SettingsCtrls_CB 				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 				UIPan_CB 						(int panel, int event, void *callbackData, int eventData1, int eventData2);

static void CVICALLBACK 			SettingsMenu_CB 				(int menuBar, int menuItem, void *callbackData, int panel);




//-----------------------------------------
// ZStage Task Controller Callbacks
//-----------------------------------------

/*
static int							ConfigureTC						(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);
static int							UnconfigureTC					(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);
static void							IterateTC						(TaskControl_type* taskControl, BOOL const* abortIterationFlag);
static void							AbortIterationTC				(TaskControl_type* taskControl, BOOL const* abortFlag);  
static int						 	StartTC			 				(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);
static int						 	ResetTC			 				(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg); 
static int							DoneTC							(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);
static int							StoppedTC						(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);
static void				 			ErrorTC 						(TaskControl_type* taskControl, int errorID, char errorMsg[]);
static int							EventHandler					(TaskControl_type* taskControl, TCStates taskState, BOOL taskActive,  void* eventData, BOOL const* abortFlag, char** errorMsg);
*/


//==============================================================================
// Global variables

//==============================================================================
// Global functions for module data management

/// HIFN  Allocates memory and initializes a generic Zstage. No functionality implemented.
/// HIPAR mod/ if NULL both memory allocation and initialization must take place.
/// HIPAR mod/ if !NULL the function performs only an initialization of a DAQLabModule_type.
/// HIRET address of a DAQLabModule_type if memory allocation and initialization took place.
/// HIRET NULL if only initialization was performed. 
DAQLabModule_type*	initalloc_Zstage (DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl)
{
	ZStage_type* zstage;
	
	if (!mod) {
		zstage = malloc (sizeof(ZStage_type));
		if (!zstage) return NULL;
	} else
		zstage = (ZStage_type*) mod;
		
	// initialize base class
	initalloc_DAQLabModule(&zstage->baseClass, className, instanceName, workspacePanHndl);
	
	// initialize Task Controller
	zstage->taskController 			= NULL;
	
	//---------------------------
	// Parent Level 0: DAQLabModule_type 
	
		// DATA
			
	
		// METHODS
	
			// overriding methods
	zstage->baseClass.Discard 		= discard_Zstage;
	zstage->baseClass.Load			= ZStage_Load;
	zstage->baseClass.LoadCfg		= ZStage_LoadCfg;
	zstage->baseClass.SaveCfg		= ZStage_SaveCfg;
	zstage->baseClass.DisplayPanels	= DisplayPanels;
	
	//---------------------------
	// Child Level 1: ZStage_type 
	
		// DATA
		
	// zstage->taskController		= tc;
	zstage->controlPanHndl			= 0;
	zstage->controlPanTopPos		= NULL;
	zstage->controlPanLeftPos		= NULL;
	zstage->setPanHndl				= 0;
	zstage->menuBarHndl				= 0;
	zstage->menuIDSettings			= 0;
	zstage->zPos					= NULL;
	zstage->stageVelocity			= NULL;
	zstage->lowVelocity				= NULL;
	zstage->midVelocity				= NULL;
	zstage->highVelocity			= NULL;
	zstage->startAbsPos				= NULL;
	zstage->endRelPos				= 0;
	zstage->stepSize				= 0;
	zstage->nZSteps					= 0;
	zstage->revertDirection			= FALSE;
	zstage->zRefPos					= ListCreate(sizeof(RefPosition_type*));
	zstage->zMaximumLimit			= NULL;		// if parameter is saved, it will be loaded and applied to the stage
	zstage->zMinimumLimit			= NULL;		// if parameter is saved, it will be loaded and applied to the stage
	
	
		// METHODS
		
			// assign default controls callback to UI_ZStage.uir panel
	zstage->uiCtrlsCB				= UICtrls_CB;
	
			// assign default panel callback to UI_Zstage.uir
	zstage->uiPanelCB				= UIPan_CB;
			// no functionality
	zstage->MoveZ					= NULL;		// child class must implement this functionality.
	zstage->UseJoystick				= NULL;		// child class may implement this functionality if applicable.
	zstage->StopZ					= NULL;		// child class must implement this functionality.
	zstage->GetHWStageLimits		= NULL;		// child class may implement this functionality.
	zstage->SetHWStageLimits		= NULL;		// child class may implement this functionality.
	zstage->GetStageVelocity		= NULL;		// child class may implement this functionality.
	zstage->SetStageVelocity		= NULL;		// child class may implement this functionality.
	zstage->StatusLED				= NULL;		// functionality assigned if ZStage_Load is called.
	zstage->UpdatePositionDisplay	= NULL;		// functionality assigned if ZStage_Load is called.
	zstage->UpdateZSteps 			= NULL;		// functionality assigned if ZStage_Load is called.
	zstage->SetStepCounter			= NULL;		// functionality assigned if ZStage_Load is called.
	zstage->DimWhenRunning			= NULL;		// functionality assigned if ZStage_Load is called.
	
	
	//----------------------------------------------------------
	if (!mod)
		return (DAQLabModule_type*) zstage;
	else
		return NULL;
}


/// HIFN Discards a generic ZStage_type data but does not free the structure memory.
void discard_Zstage (DAQLabModule_type** mod)
{
	ZStage_type* 		zstage 		= (ZStage_type*) (*mod);
	RefPosition_type**	refPosPtr	= NULL;
	size_t				nZRefpos	= ListNumItems(zstage->zRefPos);
	
	if (!zstage) return;
	
	// discard ZStage_type specific data
	discard_TaskControl_type(&zstage->taskController);
	
	// discard UI resources
	OKfreePanHndl(zstage->controlPanHndl);
	OKfree(zstage->controlPanTopPos);
	OKfree(zstage->controlPanLeftPos);
	if (zstage->menuBarHndl) {DiscardMenuBar(zstage->menuBarHndl); zstage->menuBarHndl = 0;}
	
	OKfreePanHndl(zstage->setPanHndl);
	
	OKfree(zstage->zPos);
	OKfree(zstage->startAbsPos);
	
	for (size_t i = 1; i <= nZRefpos; i++) {
		refPosPtr = ListGetPtrToItem(zstage->zRefPos, i);
		discard_RefPosition_type(refPosPtr);
	}
	ListDispose(zstage->zRefPos);
	
	// position limits
	OKfree(zstage->zMaximumLimit);
	OKfree(zstage->zMinimumLimit);
	
	// velocity and velocity settings
	OKfree(zstage->stageVelocity);
	OKfree(zstage->lowVelocity);
	OKfree(zstage->midVelocity);
	OKfree(zstage->highVelocity);

	// discard DAQLabModule_type specific data
	discard_DAQLabModule(mod);
}

static RefPosition_type* init_RefPosition_type (char refName[], double refVal)
{
	RefPosition_type* ref = malloc (sizeof(RefPosition_type));
	if (!ref) return NULL;
	
	ref->name 	= StrDup(refName);
	ref->val	= refVal;
	
	return ref;
}

static void	discard_RefPosition_type (RefPosition_type** refPosPtr)
{
	if (!*refPosPtr) return;
	
	OKfree((*refPosPtr)->name);
	OKfree(*refPosPtr);
}

/// HIFN Loads ZStage specific resources. 
int ZStage_Load (DAQLabModule_type* mod, int workspacePanHndl, char** errorMsg) 
{
#define	ZStage_Load_Err_StageNotReferenced		-1

INIT_ERR
	
	ZStage_type* 	zstage 				= (ZStage_type*) mod;  
	char			stepsizeName[50]	= "";
	
	// load panel resources
	zstage->controlPanHndl = LoadPanel(workspacePanHndl, MOD_Zstage_UI, ZStagePan);
	// set panel position
	if (zstage->controlPanLeftPos)
		SetPanelAttribute(zstage->controlPanHndl, ATTR_LEFT, *zstage->controlPanLeftPos);
	if (zstage->controlPanTopPos)
		SetPanelAttribute(zstage->controlPanHndl, ATTR_TOP, *zstage->controlPanTopPos);
	
	// connect module data and user interface callbackFn to all direct controls in the panel
	SetCtrlsInPanCBInfo(mod, ((ZStage_type*)mod)->uiCtrlsCB, zstage->controlPanHndl);
	
	// add panel callback function pointer and callback data
	SetPanelAttribute(zstage->controlPanHndl, ATTR_CALLBACK_FUNCTION_POINTER, UIPan_CB);
	SetPanelAttribute(zstage->controlPanHndl, ATTR_CALLBACK_DATA, mod);
	
	// add "Settings" menu bar item, callback data and callback function
	zstage->menuBarHndl = NewMenuBar(zstage->controlPanHndl);
	zstage->menuIDSettings = NewMenu(zstage->menuBarHndl, "Settings", -1);
	SetMenuBarAttribute(zstage->menuBarHndl, 0, ATTR_SHOW_IMMEDIATE_ACTION_SYMBOL, 0);
	SetMenuBarAttribute(zstage->menuBarHndl, zstage->menuIDSettings, ATTR_CALLBACK_DATA, zstage);
	SetMenuBarAttribute(zstage->menuBarHndl, zstage->menuIDSettings, ATTR_CALLBACK_FUNCTION_POINTER, SettingsMenu_CB);
	
	// change panel title to module instance name
	SetPanelAttribute(zstage->controlPanHndl, ATTR_TITLE, mod->instanceName);
	
	// adjust position display precision
	SetCtrlAttribute(zstage->controlPanHndl, ZStagePan_ZAbsPos, ATTR_PRECISION, POS_DISPLAY_PRECISION);
	SetCtrlAttribute(zstage->controlPanHndl, ZStagePan_ZRelPos, ATTR_PRECISION, POS_DISPLAY_PRECISION);
	SetCtrlAttribute(zstage->controlPanHndl, ZStagePan_StartAbsPos, ATTR_PRECISION, POS_DISPLAY_PRECISION);
	SetCtrlAttribute(zstage->controlPanHndl, ZStagePan_EndRelPos, ATTR_PRECISION, POS_DISPLAY_PRECISION);
	
	// populate Z step sizes
	for (size_t i = 0; i < NumElem(ZStepSizes); i++) {
		Fmt(stepsizeName, "%s<%f[p*]", POS_DISPLAY_PRECISION, ZStepSizes[i]);
		InsertListItem(zstage->controlPanHndl, ZStagePan_ZStepSize, -1, stepsizeName, ZStepSizes[i]);   
	}
	// update Z step size in structure data to be the first element in the list
	zstage->stepSize = ZStepSizes[0]; 
	
	// add functionality to change LED status
	zstage->StatusLED = ChangeLEDStatus;
	
	// add functionality to update stage position
	zstage->UpdatePositionDisplay = UpdatePositionDisplay;
	
	// add functionality to update z stepping values
	zstage->UpdateZSteps = UpdateZSteps;
	
	// add functionality to reset step counter
	zstage->SetStepCounter = SetStepCounter;
	
	// add functionality to dim and undim controls when stage is stepping
	zstage->DimWhenRunning	 = DimWhenRunning;	
	
	if (zstage->zPos) {
		// update stage absolute position if position has been determined
		// make visible stepper controls and update them if position has been determined
		SetCtrlVal(zstage->controlPanHndl, ZStagePan_ZAbsPos, *zstage->zPos * 1000);	  // convert from [mm] to [um]
		SetCtrlVal(zstage->controlPanHndl, ZStagePan_StartAbsPos, *zstage->zPos * 1000);  // convert from [mm] to [um]
		
		// add start pos data to structure;
		zstage->startAbsPos = malloc(sizeof(double));
		*zstage->startAbsPos = *zstage->zPos;
		
		// set negative and positive stage limits to be the same as the current stage position if not specified by hardware
		if (!zstage->zMaximumLimit) {
			zstage->zMaximumLimit = malloc(sizeof(double));
			*zstage->zMaximumLimit = *zstage->zPos;
		}
		
		if (!zstage->zMinimumLimit) {
			zstage->zMinimumLimit = malloc(sizeof(double));
			*zstage->zMinimumLimit = *zstage->zPos;
		}
		
	} else {
		errorInfo.error 	= ZStage_Load_Err_StageNotReferenced;
		errorInfo.errMsg 	= StrDup("Stage position must be determined first");
		goto Error;
	}
	
	// dim/undim Joystick control box if such functionality is made or not available by the child class
	if (zstage->UseJoystick)
		SetCtrlAttribute(zstage->controlPanHndl, ZStagePan_Joystick, ATTR_DIMMED, 0);
	else
		SetCtrlAttribute(zstage->controlPanHndl, ZStagePan_Joystick, ATTR_DIMMED, 1);
	
	// dim/undim velocity setting if such functionality is made available by the child class
	if (zstage->SetStageVelocity)
		SetCtrlAttribute(zstage->controlPanHndl, ZStagePan_StageVel, ATTR_DIMMED, 0);
	else
		SetCtrlAttribute(zstage->controlPanHndl, ZStagePan_StageVel, ATTR_DIMMED, 1);
		
	// if a stage velocity has been determined (by the child class) and there are no settings loaded for
	// the different velocities, then make them equal to the current velocity of the stage
	if (zstage->stageVelocity) {
		
		// set low velocity
		if (!zstage->lowVelocity) {
			zstage->lowVelocity = malloc(sizeof(double));
			*zstage->lowVelocity = *zstage->stageVelocity;
		}
		
		// set mid velocity
		if (!zstage->midVelocity) {
			zstage->midVelocity = malloc(sizeof(double));
			*zstage->midVelocity = *zstage->stageVelocity;
		}
		
		// set high velocity
		if (!zstage->highVelocity) {
			zstage->highVelocity = malloc(sizeof(double));
			*zstage->highVelocity = *zstage->stageVelocity;
		}
	}
	
	// configure Z Stage Task Controller
	errChk( TaskControlEvent(zstage->taskController, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) ); 
	
	// add reference positions if any
	size_t				nRefPos										= ListNumItems(zstage->zRefPos);
	char				refPosDisplayItem[MAX_REF_POS_LENGTH + 50]; 
	RefPosition_type**	refPosPtr;
	if (nRefPos) {
		// undim ref pos listbox
		SetCtrlAttribute(zstage->controlPanHndl, ZStagePan_RefPosList, ATTR_DIMMED, 0);
		for (size_t i = 1; i <= nRefPos; i++) {
			refPosPtr = ListGetPtrToItem(zstage->zRefPos, i);
			// add reference position to listbox
			Fmt(refPosDisplayItem, "%s @ %f[p*]", (*refPosPtr)->name, POS_DISPLAY_PRECISION, (*refPosPtr)->val*1000);  	// display in [um]
			InsertListItem(zstage->controlPanHndl, ZStagePan_RefPosList, -1, refPosDisplayItem, (*refPosPtr)->val); 	// store listbox position value in [mm] 
		}
	} else
		SetCtrlAttribute(zstage->controlPanHndl, ZStagePan_RefPosList, ATTR_DIMMED, 1);
	
	// register task controller with framework
	DLAddTaskController((DAQLabModule_type*)zstage, zstage->taskController);
	
	return 0;
	
Error:
	
RETURN_ERR	
}

int	ZStage_LoadCfg (DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_ moduleElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	ZStage_type* 		zstage			= (ZStage_type*) mod;
	
	// allocate memory to store values
	// Note: This must be done before initializing the attributes array!
	
	// UI
	nullChk( zstage->controlPanLeftPos 	= malloc(sizeof(int)) );
	nullChk( zstage->controlPanTopPos 	= malloc(sizeof(int)) );
	// stage settings
	nullChk( zstage->zMinimumLimit 		= malloc(sizeof(double)) );
	nullChk( zstage->zMaximumLimit 		= malloc(sizeof(double)) );
	nullChk( zstage->lowVelocity		= malloc(sizeof(double)) );
	nullChk( zstage->midVelocity		= malloc(sizeof(double)) );
	nullChk( zstage->highVelocity		= malloc(sizeof(double)) );
	
	// initialize attributes array
	DAQLabXMLNode 		zStageAttr[] 	= {	{"PanTopPos", BasicData_Int, zstage->controlPanTopPos},
											{"PanLeftPos", BasicData_Int, zstage->controlPanLeftPos},
											{"MinPositionLimit", BasicData_Double, zstage->zMinimumLimit},
								 			{"MaxPositionLimit", BasicData_Double, zstage->zMaximumLimit},
											{"LowVelocity", BasicData_Double, zstage->lowVelocity},
											{"MidVelocity", BasicData_Double, zstage->midVelocity},
											{"HighVelocity", BasicData_Double, zstage->highVelocity} };
											
	//--------------------------------------------------------------							
	// get safety limits and velocities	
	//--------------------------------------------------------------
	DLGetXMLElementAttributes("", moduleElement, zStageAttr, NumElem(zStageAttr));
	
	//--------------------------------------------------------------
	// get saved reference positions
	//--------------------------------------------------------------
	ActiveXMLObj_IXMLDOMNodeList_	xmlRefPositionsNodeList		= 0;
	ActiveXMLObj_IXMLDOMNode_		xmlRefPositionsNode			= 0;
	ActiveXMLObj_IXMLDOMNodeList_	xmlRefPosNodeList			= 0;
	ActiveXMLObj_IXMLDOMNode_		xmlRefPosNode				= 0;
	long							nXMLElements;
	long							nRefPos;
	
	errChk ( ActiveXML_IXMLDOMElement_getElementsByTagName(moduleElement, xmlErrorInfo, "ReferencePositions", &xmlRefPositionsNodeList) );
	// skip this if there are no positions stored
	errChk ( ActiveXML_IXMLDOMNodeList_Getlength(xmlRefPositionsNodeList, xmlErrorInfo, &nXMLElements) );
	if (!nXMLElements) goto NoReferencePositions;
	
	errChk ( ActiveXML_IXMLDOMNodeList_Getitem(xmlRefPositionsNodeList, xmlErrorInfo, 0, &xmlRefPositionsNode) );
	
	// get reference position elements
	errChk ( ActiveXML_IXMLDOMElement_getElementsByTagName(xmlRefPositionsNode, xmlErrorInfo, "RefPos", &xmlRefPosNodeList) );
	errChk ( ActiveXML_IXMLDOMNodeList_Getlength(xmlRefPosNodeList, xmlErrorInfo, &nXMLElements) );
	
	if (!nXMLElements) goto NoReferencePositions;
	// load reference positions
	RefPosition_type*	refPos			= NULL;
	char*				refName			= NULL;
	double				refVal			= 0;
	DAQLabXMLNode		refPosAttr[2] 	= { {"Name", 		BasicData_CString, 		&refName }, 
											{"Position", 	BasicData_Double, 		&refVal} };
											
	for (long i = 0; i < nXMLElements; i++) {
		errChk ( ActiveXML_IXMLDOMNodeList_Getitem(xmlRefPosNodeList, xmlErrorInfo, i, &xmlRefPosNode) );
		DLGetXMLElementAttributes("", (ActiveXMLObj_IXMLDOMElement_)xmlRefPosNode, refPosAttr, NumElem(refPosAttr));
		OKfreeCAHndl(xmlRefPosNode);
		refPos = init_RefPosition_type(refName, refVal);
		ListInsertItem(zstage->zRefPos, &refPos, END_OF_LIST);
	}
	
NoReferencePositions:
	
	// cleanup
	OKfreeCAHndl(xmlRefPositionsNodeList); 
	OKfreeCAHndl(xmlRefPositionsNode);
	OKfreeCAHndl(xmlRefPosNodeList); 
	
	return 0;
	
Error:   
	
	return errorInfo.error;
}

int ZStage_SaveCfg (DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ moduleElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	ZStage_type* 					zstage			= (ZStage_type*) mod;
	int*							panTopPos		= NULL;
	int*							panLeftPos		= NULL;
	
	nullChk( panTopPos 		= malloc(sizeof(int)) );
	nullChk( panLeftPos 	= malloc(sizeof(int)) );	
	
	DAQLabXMLNode 					zStageAttr[] 	= {	{"PanTopPos", BasicData_Int, panTopPos},
											  		   	{"PanLeftPos", BasicData_Int, panLeftPos},
														{"MinPositionLimit", BasicData_Double, zstage->zMinimumLimit},
								 						{"MaxPositionLimit", BasicData_Double, zstage->zMaximumLimit},
														{"LowVelocity", BasicData_Double, zstage->lowVelocity},
														{"MidVelocity", BasicData_Double, zstage->midVelocity},
														{"HighVelocity", BasicData_Double, zstage->highVelocity} }; 
	// get panel position
	errChk( GetPanelAttribute(zstage->controlPanHndl, ATTR_LEFT, panLeftPos) );
	errChk( GetPanelAttribute(zstage->controlPanHndl, ATTR_TOP, panTopPos) );
	
	// add zstage module attributes
	errChk( DLAddToXMLElem(xmlDOM, moduleElement, zStageAttr, DL_ATTRIBUTE, NumElem(zStageAttr), xmlErrorInfo) );
	
	// add reference positions element to module element
	ActiveXMLObj_IXMLDOMElement_	refPositionsXMLElement;
	ActiveXMLObj_IXMLDOMElement_	refPosXMLElement;
	RefPosition_type**				refPosPtr;
	DAQLabXMLNode 					refPosAttr[2]; 
	
	errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "ReferencePositions", &refPositionsXMLElement) );
	errChk ( ActiveXML_IXMLDOMElement_appendChild (moduleElement, xmlErrorInfo, refPositionsXMLElement, NULL) );
	// add reference positions
	size_t	nPositions = ListNumItems(zstage->zRefPos);
	for (size_t i = 1; i <= nPositions; i++) {
		refPosPtr = ListGetPtrToItem(zstage->zRefPos, i);
		// add new reference position element to reference positions element
		errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "RefPos", &refPosXMLElement) );
		errChk ( ActiveXML_IXMLDOMElement_appendChild (refPositionsXMLElement, xmlErrorInfo, refPosXMLElement, NULL) );
		// add attributes to this reference position
		refPosAttr[0].tag 		= "Name";
		refPosAttr[0].type		= BasicData_CString;
		refPosAttr[0].pData	= (*refPosPtr)->name;
		refPosAttr[1].tag 		= "Position";
		refPosAttr[1].type		= BasicData_Double;
		refPosAttr[1].pData	= &(*refPosPtr)->val;
		errChk( DLAddToXMLElem(xmlDOM, refPosXMLElement, refPosAttr, DL_ATTRIBUTE, NumElem(refPosAttr), xmlErrorInfo) );   
	}

Error:
	
	// cleanup
	OKfree(panTopPos);
	OKfree(panLeftPos);
	
	return errorInfo.error; 
}

static int DisplayPanels (DAQLabModule_type* mod, BOOL visibleFlag)
{
INIT_ERR

	ZStage_type* 	zstage		= (ZStage_type*) mod; 
	
	
	if (visibleFlag)
		errChk(	DisplayPanel(zstage->controlPanHndl) );
	else
		errChk( HidePanel(zstage->controlPanHndl) );
	
Error:
	
	return errorInfo.error;
}

static int ChangeLEDStatus (ZStage_type* zstage, Zstage_LED_type status)
{
INIT_ERR	
	
	switch (status) {																	   
			
		case ZSTAGE_LED_IDLE:
			
			errChk( SetCtrlVal(zstage->controlPanHndl, ZStagePan_Status, FALSE) );
			break;
			
		case ZSTAGE_LED_MOVING:
			
			errChk( SetCtrlAttribute(zstage->controlPanHndl, ZStagePan_Status, ATTR_ON_COLOR, VAL_GREEN) );
			errChk( SetCtrlVal(zstage->controlPanHndl, ZStagePan_Status, TRUE) );
			break;
			
		case ZSTAGE_LED_ERROR:
			
			errChk( SetCtrlAttribute(zstage->controlPanHndl, ZStagePan_Status, ATTR_ON_COLOR, VAL_RED) );
			errChk( SetCtrlVal(zstage->controlPanHndl, ZStagePan_Status, TRUE) );
			break;
	}
	
	return 0;
	
Error:
	
	return errorInfo.error;
}

static int UpdatePositionDisplay (ZStage_type* zstage)
{
INIT_ERR
	
	// display current position
	errChk( SetCtrlVal(zstage->controlPanHndl, ZStagePan_ZAbsPos, *zstage->zPos * 1000) );  // convert from [mm] to [um] 
	// reset relative position
	errChk( SetCtrlVal(zstage->controlPanHndl, ZStagePan_ZRelPos, 0.0) );
	
Error:
	
	return errorInfo.error;
}

static int UpdateZSteps (ZStage_type* zstage)
{
INIT_ERR	
	
	long		nSteps		= 0;
	double 		stepSize	= 0;
	
	// get step size
	GetCtrlVal(zstage->controlPanHndl, ZStagePan_ZStepSize, &stepSize);  				// in [um]
	stepSize 				/= 1000;													// convert to [mm]
	Assert(stepSize > 0);  	// allow only positive non-zero step sizes
	zstage->stepSize 		= stepSize;
	
	// get absolute start position
	GetCtrlVal(zstage->controlPanHndl, ZStagePan_StartAbsPos, zstage->startAbsPos);   	// in [um]
	*zstage->startAbsPos	/= 1000;													// convert to [mm]
	
	// get relative end position (to absolute start position)
	GetCtrlVal(zstage->controlPanHndl, ZStagePan_EndRelPos, &zstage->endRelPos);   		// in [um]
	zstage->endRelPos		/= 1000;													// convert to [mm]
	
	// calculate steps and adjust end position to be multiple of stepSize
	nSteps					= RoundRealToNearestInteger(zstage->endRelPos/stepSize);
	if (nSteps)
		zstage->nZSteps		= fabs(nSteps)+1;
	else
		zstage->nZSteps		= 0;
		
	zstage->endRelPos 		= nSteps * stepSize;
	
	// display values
	errChk( SetCtrlVal(zstage->controlPanHndl, ZStagePan_StartAbsPos, *zstage->startAbsPos * 1000) );		// convert from [mm] to [um]
	errChk( SetCtrlVal(zstage->controlPanHndl, ZStagePan_EndRelPos, zstage->endRelPos * 1000) );			// convert from [mm] to [um]
	errChk( SetCtrlVal(zstage->controlPanHndl, ZStagePan_NSteps, zstage->nZSteps) );
	
	// configure Task Controller
	errChk( TaskControlEvent(zstage->taskController, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
	
Error:
	
	return errorInfo.error;
}

static void SetStepCounter	(ZStage_type* zstage, size_t val)
{
	SetCtrlVal(zstage->controlPanHndl, ZStagePan_Step, val);
}

static void	DimWhenRunning (ZStage_type* zstage, BOOL dimmed)
{
	int menubarHndl = GetPanelMenuBar (zstage->controlPanHndl);
	
	SetMenuBarAttribute(menubarHndl, 0, ATTR_DIMMED, dimmed);
	
	for (int i = 0; i < NumElem(UIZStageCtrls); i++) {
		SetCtrlAttribute(zstage->controlPanHndl, UIZStageCtrls[i], ATTR_DIMMED, dimmed);
	}
}

//==============================================================================
// Global functions for module specific UI management


static int CVICALLBACK UICtrls_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	ZStage_type* 				zstage 											= callbackData;
	int							refPosIdx										= 0;			// 0-based index of ref pos selected in the list
	RefPosition_type**			refPosPtrPtr									= NULL;
	double						stepsize										= 0;	  					
	double 						direction										= 0;
	double 						moveAbsPos										= 0;   					
	double						moveRelPos										= 0; 
	char						refPosDisplayItem[MAX_REF_POS_LENGTH + 50]		= "";
	
	switch (event) {
			
		case EVENT_COMMIT:
			
			GetCtrlVal(panel, ZStagePan_ZStepSize, &stepsize);  // get value in [um]
			stepsize /= 1000;									// convert from [um] to [mm]
			GetCtrlVal(panel, ZStagePan_ZAbsPos, &moveAbsPos);  // get value in [um]
			moveAbsPos /= 1000;									// convert from [um] to [mm]
			GetCtrlVal(panel, ZStagePan_ZRelPos, &moveRelPos);  // get value in [um]
			moveRelPos /= 1000;									// convert from [um] to [mm]
			
			// revert direction if needed
			if (zstage->revertDirection) 
				direction = -1;
			else
				direction = 1;
			
			switch (control) {
					
				case ZStagePan_MoveZUp:
					
					// move up relative to current position with selected step size
					if (zstage->MoveZ) {
						errChk( (*zstage->MoveZ)(zstage, ZSTAGE_MOVE_REL, direction * stepsize, &errorInfo.errMsg) );
					}
					break;
					
				case ZStagePan_MoveZDown:
					
					// move down relative to current position with selected step size
					if (zstage->MoveZ) {
						errChk( (*zstage->MoveZ)(zstage, ZSTAGE_MOVE_REL, -direction*stepsize, &errorInfo.errMsg) );
					}
					break;
					
				case ZStagePan_Stop:
					
					// stop stage motion
					if (zstage->StopZ) {
						errChk( (*zstage->StopZ)(zstage, &errorInfo.errMsg) );
					}
					break;
				
				case ZStagePan_ZAbsPos:
					
					// move to absolute position with selected step size
					if (zstage->MoveZ) {
						errChk( (*zstage->MoveZ)(zstage, ZSTAGE_MOVE_ABS, moveAbsPos, &errorInfo.errMsg) );
					}
					break;
					
				case ZStagePan_ZRelPos:
					
					// move relative to current position
					if (zstage->MoveZ) {
						errChk( (*zstage->MoveZ)(zstage, ZSTAGE_MOVE_REL, moveRelPos, &errorInfo.errMsg) );
					}
					break;
					
				case ZStagePan_StartAbsPos:
				case ZStagePan_EndRelPos:
				case ZStagePan_ZStepSize:
					
					UpdateZSteps	(zstage);
					
					break;
					
				case ZStagePan_AddRefPos:
					
					char* 				newRefPosName		= NULL;
					RefPosition_type*	newRefPos			= NULL;
					
					newRefPosName = DLGetUINameInput("New Reference Position", MAX_REF_POS_LENGTH, ValidateRefPosName, &zstage->zRefPos); 
					if (!newRefPosName) return 0;	// operation cancelled or an error occured.
					
					// add new reference position to structure data
					nullChk( newRefPos = init_RefPosition_type(newRefPosName, moveAbsPos) );
					
					nullChk( ListInsertItem(zstage->zRefPos, &newRefPos, END_OF_LIST) );
					
					// undim ref pos listbox
					SetCtrlAttribute(panel, ZStagePan_RefPosList, ATTR_DIMMED, 0);
					
					// add reference position to listbox
					Fmt(refPosDisplayItem, "%s @ %f[p*]", newRefPosName, POS_DISPLAY_PRECISION, moveAbsPos * 1000);  		// display in [um]
					OKfree(newRefPosName);
					errChk( InsertListItem(panel, ZStagePan_RefPosList, -1, refPosDisplayItem, moveAbsPos) ); // store listbox position value in [mm] 
					break;
				
				case ZStagePan_Joystick:
					
					BOOL	useJoystick;
					
					if (!zstage->UseJoystick) break; // do nothing if no joystick functionality is implemented
					
					GetCtrlVal(panel, control, &useJoystick);
					
					errChk( (*zstage->UseJoystick)(zstage, useJoystick, &errorInfo.errMsg) );
					
					break;
					
				case ZStagePan_StageVel:
					
					int		velIdx;
					
					GetCtrlIndex(panel, control, &velIdx);
					
					if (!zstage->SetStageVelocity) break; // do nothing if there is no such functionality implemented by the child class
					
					if (velIdx == 0 && zstage->lowVelocity) {
						errChk( (*zstage->SetStageVelocity)(zstage, *zstage->lowVelocity, &errorInfo.errMsg) );
					} else
						if (velIdx == 1 && zstage->midVelocity) {
							errChk( (*zstage->SetStageVelocity)(zstage, *zstage->midVelocity, &errorInfo.errMsg) );
						} else
							if (velIdx == 2 && zstage->highVelocity) {
								errChk( (*zstage->SetStageVelocity)(zstage, *zstage->highVelocity, &errorInfo.errMsg) );
							}
					
					break;
					
			}
			
			break;
			
		case EVENT_KEYPRESS:		// delete reference position from list
			
			switch (control) {
					
				case ZStagePan_RefPosList:
					
					// if Del key is pressed
					if (eventData1 == VAL_FWD_DELETE_VKEY) {
						// get ref pos idx
						GetCtrlIndex(panel, control, &refPosIdx);
						// remove from list box
						DeleteListItem(panel, control, refPosIdx, 1);
						// remove from structure data
						refPosPtrPtr = ListGetPtrToItem(zstage->zRefPos, refPosIdx + 1);
						discard_RefPosition_type(refPosPtrPtr);
						ListRemoveItem(zstage->zRefPos, 0, refPosIdx + 1);
						// dim listbox if there are no more items
						if (!ListNumItems(zstage->zRefPos))
							SetCtrlAttribute(panel, ZStagePan_RefPosList, ATTR_DIMMED, 1);
						
						
					}
					break;
			}
			break;
			
		case EVENT_LEFT_DOUBLE_CLICK: // moves stage to selected reference position
			
			switch (control) {
					
				case ZStagePan_RefPosList:
					
					// get ref pos idx
					GetCtrlIndex(panel, control, &refPosIdx);
					GetValueFromIndex(panel, control, refPosIdx, &moveAbsPos);
					// move to reference position
					if (zstage->MoveZ) {
						errChk( (*zstage->MoveZ) (zstage, ZSTAGE_MOVE_ABS, moveAbsPos, &errorInfo.errMsg) );
					}
					break;
			}
			break;
			
		case EVENT_RIGHT_DOUBLE_CLICK: 
			
			switch (control) {
					
				case ZStagePan_RefPosList:  	// update reference position value
					
					// get ref pos idx
					GetCtrlIndex(panel, control, &refPosIdx);
					// update reference position in structure data
					refPosPtrPtr = ListGetPtrToItem(zstage->zRefPos, refPosIdx + 1);
					(*refPosPtrPtr)->val = *zstage->zPos;
					// update reference position label
					// display in [um]  
					Fmt(refPosDisplayItem, "%s @ %f[p*]", (*refPosPtrPtr)->name, POS_DISPLAY_PRECISION, *zstage->zPos * 1000); 
					// store listbox position value in [mm] 
					ReplaceListItem(panel, ZStagePan_RefPosList, refPosIdx, refPosDisplayItem, (*refPosPtrPtr)->val); 			
					break;
					
				case ZStagePan_StartAbsPos:	// set start position to match current position
					
					SetCtrlVal(panel, control, *zstage->zPos * 1000); 							// convert to [um]
					UpdateZSteps(zstage);  
					break;
					
				case ZStagePan_EndRelPos: 	// set relative end position (to absolute start position) to match current stage position
					
					SetCtrlVal(panel, control, (*zstage->zPos - *zstage->startAbsPos) * 1000);   	// convert to [um] 
					UpdateZSteps(zstage); 
					break;
					
			}
			break;
			
		case EVENT_MOUSE_WHEEL_SCROLL:
			
			// revert direction if needed
			if (zstage->revertDirection) 
				direction = -1;
			else
				direction = 1;
			
			GetCtrlVal(panel, ZStagePan_ZStepSize, &stepsize);  // get value in [um]
			stepsize /= 1000;									// convert from [um] to [mm]
			
			switch (control) {
					
				case ZStagePan_ZAbsPos:				// adjust absolute position
					
					switch (eventData1) {
					
						case MOUSE_WHEEL_SCROLL_UP:
							
							SetCtrlVal(panel, control, (*zstage->zPos + direction * stepsize) * 1000);	// convert back to [um]
							// move relative to current position
							if (zstage->MoveZ) {
								errChk( (*zstage->MoveZ)(zstage, ZSTAGE_MOVE_REL, direction * stepsize, &errorInfo.errMsg) );
							}
							return 1;
					
						case MOUSE_WHEEL_SCROLL_DOWN:
					
							SetCtrlVal(panel, control, (*zstage->zPos - direction * stepsize) * 1000);	// convert back to [um] 
							// move relative to current position
							if (zstage->MoveZ) {
								errChk( (*zstage->MoveZ)(zstage, ZSTAGE_MOVE_REL, -direction * stepsize, &errorInfo.errMsg) );
							}
							return 1;
					
					}
					break;
					
				case ZStagePan_ZRelPos:				// adjust relative position
					
					switch (eventData1) {
					
						case MOUSE_WHEEL_SCROLL_UP:
							
							SetCtrlVal(panel, control, direction * stepsize * 1000);					// convert back to [um]
							// move relative to current position
							if (zstage->MoveZ) {
								errChk( (*zstage->MoveZ)(zstage, ZSTAGE_MOVE_REL, direction * stepsize, &errorInfo.errMsg) );
							}
							return 1;
					
						case MOUSE_WHEEL_SCROLL_DOWN:
					
							SetCtrlVal(panel, control, -direction * stepsize * 1000);					// convert back to [um]
							// move relative to current position
							if (zstage->MoveZ) {
								errChk( (*zstage->MoveZ)(zstage, ZSTAGE_MOVE_REL, -direction * stepsize, &errorInfo.errMsg) );
							}
							return 1;
					
					}
					break;
					
				case ZStagePan_StartAbsPos:
					
					switch (eventData1) {
					
						case MOUSE_WHEEL_SCROLL_UP:
							
							SetCtrlVal(panel, control, (*zstage->startAbsPos + stepsize) * 1000);		// convert back to [um]
							break;
					
						case MOUSE_WHEEL_SCROLL_DOWN:
					
							SetCtrlVal(panel, control, (*zstage->startAbsPos - stepsize) * 1000);		// convert back to [um]
							break;
					
					}
					break;
					
				case ZStagePan_EndRelPos:
					
					switch (eventData1) {
					
						case MOUSE_WHEEL_SCROLL_UP:
							
							SetCtrlVal(panel, control, (zstage->endRelPos + stepsize) * 1000);		// convert back to [um]
							break;
					
						case MOUSE_WHEEL_SCROLL_DOWN:
					
							SetCtrlVal(panel, control, (zstage->endRelPos - stepsize) * 1000);		// convert back to [um]
							break;
					
					}
					break;
			}
	}
	
Error:
	
PRINT_ERR

	return 0;
}

static int CVICALLBACK SettingsCtrls_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	ZStage_type* 		zstage 	= callbackData;
	
	switch (event) {
		
		case EVENT_COMMIT:
			
			switch (control) {
					
				case ZSetPan_OKBTTN:
					
					DiscardPanel(zstage->setPanHndl);
					zstage->setPanHndl = 0;
					break;
					
				case ZSetPan_MinimumLimit:
				case ZSetPan_MaximumLimit:
					
					// update limits in structure data
					double	newMinLimit;
					double	newMaxLimit;
					GetCtrlVal(panel, ZSetPan_MinimumLimit, &newMinLimit);
					newMinLimit *= 0.001; // convert from [um] to [mm]
					GetCtrlVal(panel, ZSetPan_MaximumLimit, &newMaxLimit);
					newMaxLimit *= 0.001; // convert from [um] to [mm]
					
					// if given, call hardware specific function to set these limits
					if (zstage->SetHWStageLimits) {
						if ( (*zstage->SetHWStageLimits)(zstage, newMinLimit, newMaxLimit, NULL) < 0) {
							// error setting new limits, return to old values
							SetCtrlVal(panel, ZSetPan_MinimumLimit, *zstage->zMinimumLimit * 1000);		// convert from [mm] to [um]
							SetCtrlVal(panel, ZSetPan_MaximumLimit, *zstage->zMaximumLimit * 1000);		// convert from [mm] to [um]
						} else {
							// new limits set succesfully, update structure data with correct limits
							*zstage->zMinimumLimit = newMinLimit;
							*zstage->zMaximumLimit = newMaxLimit;
						}
					}
					break;
					
				case ZSetPan_LowVelocity:
					
					// set velocity value in structure data
					GetCtrlVal(panel, control, zstage->lowVelocity);
					
					// if this velocity setting is selected, then set stage velocity
					{	int		velIdx;	// velocity settings index to be applied to the stage:
									// 0 - low, 1 - mid, 2 - high.
						GetCtrlIndex(zstage->controlPanHndl, ZStagePan_StageVel, &velIdx);
						if (velIdx == 0 && zstage->SetStageVelocity) {
							errChk( (*zstage->SetStageVelocity)(zstage, *zstage->lowVelocity, &errorInfo.errMsg) );
						}
					}
					break;
					
				case ZSetPan_MidVelocity:
					
					// set velocity value in structure data
					GetCtrlVal(panel, control, zstage->midVelocity);
					
					// if this velocity setting is selected, then set stage velocity
					{	int		velIdx;	// velocity settings index to be applied to the stage:
									// 0 - low, 1 - mid, 2 - high.
						GetCtrlIndex(zstage->controlPanHndl, ZStagePan_StageVel, &velIdx);
						if (velIdx == 1 && zstage->SetStageVelocity) {
							errChk( (*zstage->SetStageVelocity)(zstage, *zstage->midVelocity, &errorInfo.errMsg) );
						}
					}
					break;
					
				case ZSetPan_HighVelocity:
					
					// set velocity value in structure data
					GetCtrlVal(panel, control, zstage->highVelocity);
					
					// if this velocity setting is selected, then set stage velocity
					{	int		velIdx;	// velocity settings index to be applied to the stage:
									// 0 - low, 1 - mid, 2 - high.
						GetCtrlIndex(zstage->controlPanHndl, ZStagePan_StageVel, &velIdx);
						if (velIdx == 0 && zstage->SetStageVelocity) {
							errChk( (*zstage->SetStageVelocity)(zstage, *zstage->highVelocity, &errorInfo.errMsg) );
						}
					}
					break;
					
			}
			
			break;
	}
	
Error:
	
PRINT_ERR

	return 0;
}

static int CVICALLBACK UIPan_CB (int panel, int event, void *callbackData, int eventData1, int eventData2)
{
	ZStage_type* 		zstage 	= callbackData;
	
	return 0;
}

static void CVICALLBACK SettingsMenu_CB (int menuBar, int menuItem, void *callbackData, int panel)
{
	ZStage_type* 	zstage 			= callbackData;
	int				parentPanHndl;
	
	GetPanelAttribute(panel, ATTR_PANEL_PARENT, &parentPanHndl);
	
	// load settings panel resources if not already loaded
	if (!zstage->setPanHndl)
		zstage->setPanHndl = LoadPanel(parentPanHndl, MOD_Zstage_UI, ZSetPan);
	// add callback data and function
	SetCtrlsInPanCBInfo(zstage, SettingsCtrls_CB, zstage->setPanHndl);
	
	// update limits
	SetCtrlVal(zstage->setPanHndl, ZSetPan_MinimumLimit, *zstage->zMinimumLimit * 1000); 	// convert from [mm] to [um]
	SetCtrlVal(zstage->setPanHndl, ZSetPan_MaximumLimit, *zstage->zMaximumLimit * 1000); 	// convert from [mm] to [um]
	
	// update stage low velocity setting
	if (zstage->lowVelocity) {
		SetCtrlVal(zstage->setPanHndl, ZSetPan_LowVelocity, *zstage->lowVelocity);			
		SetCtrlAttribute(zstage->setPanHndl, ZSetPan_LowVelocity, ATTR_DIMMED, 0);
	} else
		SetCtrlAttribute(zstage->setPanHndl, ZSetPan_LowVelocity, ATTR_DIMMED, 1);
	
	// update stage mid velocity setting
	if (zstage->midVelocity) {
		SetCtrlVal(zstage->setPanHndl, ZSetPan_MidVelocity, *zstage->midVelocity);			
		SetCtrlAttribute(zstage->setPanHndl, ZSetPan_MidVelocity, ATTR_DIMMED, 0);
	} else
		SetCtrlAttribute(zstage->setPanHndl, ZSetPan_MidVelocity, ATTR_DIMMED, 1);
	
	// update stage high velocity setting
	if (zstage->highVelocity) {
		SetCtrlVal(zstage->setPanHndl, ZSetPan_HighVelocity, *zstage->highVelocity);			
		SetCtrlAttribute(zstage->setPanHndl, ZSetPan_HighVelocity, ATTR_DIMMED, 0);
	} else
		SetCtrlAttribute(zstage->setPanHndl, ZSetPan_HighVelocity, ATTR_DIMMED, 1);
	
	
	DisplayPanel(zstage->setPanHndl);
}

static BOOL ValidateRefPosName (char inputStr[], void* dataPtr)
{
	ListType 					RefPosList	= *(ListType*)dataPtr;
	RefPosition_type**	refPosPtrPtr;
	
	for (int i = 1; i <= ListNumItems(RefPosList); i++) {
		refPosPtrPtr = ListGetPtrToItem(RefPosList, i);
		// if ref pos with same name is found, input is not valid
		if (!strcmp((*refPosPtrPtr)->name, inputStr)) return FALSE;
	}
	
	return TRUE;
}

//-----------------------------------------
// ZStage Task Controller Callbacks
//-----------------------------------------

/*
static int ConfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	ZStage_type* 		zstage 			= GetTaskControlModuleData(taskControl);
	
	return 0;
}

static int UnconfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	ZStage_type* 		zstage 			= GetTaskControlModuleData(taskControl);
	
	return 0;
}

static void IterateTC (TaskControl_type* taskControl, BOOL const* abortIterationFlag)
{
	ZStage_type* 		zstage 			= GetTaskControlModuleData(taskControl);
	
	
	TaskControlEvent(taskControl, TC_Event_IterationDone, NULL, NULL);
}

static void	AbortIterationTC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	ZStage_type* 		zstage 			= GetTaskControlModuleData(taskControl);
	
}

static int StartTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	ZStage_type* 		zstage 			= GetTaskControlModuleData(taskControl);
	
	return 0;
}

static int ResetTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	ZStage_type* 		zstage 			= GetTaskControlModuleData(taskControl);
	
	return 0;
}

static int DoneTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	ZStage_type* 		zstage 			= GetTaskControlModuleData(taskControl);
	
	return 0;
}

static int StoppedTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	ZStage_type* 		zstage 			= GetTaskControlModuleData(taskControl);
	
	return 0;
}

static void	ErrorTC (TaskControl_type* taskControl, int errorID, char errorMsg[])
{
	ZStage_type* 		zstage 			= GetTaskControlModuleData(taskControl);
	
}

static int EventHandler (TaskControl_type* taskControl, TCStates taskState, BOOL taskActive,  void* eventData, BOOL const* abortFlag, char** errorMsg)
{
	ZStage_type* 		zstage 			= GetTaskControlModuleData(taskControl);
	
	return 0;
}

*/
