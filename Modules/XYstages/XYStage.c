//==============================================================================
//
// Title:		XYStage.c
// Purpose:		A short description of the implementation.
//
// Created on:	18-3-2015 at 16:38:28 by Adrian Negrean.
// Copyright:	VU University Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "DAQLab.h" 		// include this first
#include "DAQLabModule.h"
#include <formatio.h>
#include "toolbox.h"
#include <userint.h>

#include "XYStage.h"
#include "UI_XYStage.h" 

//==============================================================================
// Constants


	// XYstage UI resource

#define MOD_XYStage_UI 				"./Modules/XYstages/UI_XYStage.uir"

	// Maximum entry length in characters for reference positions
#define MAX_REF_POS_LENGTH			50

	// Number of digits after the decimal point used to display stage position in [um]
#define POS_DISPLAY_PRECISION		1

#define REF_POS_LABEL_FORMAT		"%s @ x = %f[p*], y = %f[p*]"

	// Predefined step sizes in [um] displayed in the StagePan_XStepSize and StagePan_YStepSize controls
const double 	XYStepSizes[] 		= { 1, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000 };

	// Controls from UI_XYstage.uir that will be dimmed or undimmed when the stage is stepping (RUNNING state)
const int 		UIXYStageCtrls[] = {StagePan_MoveXForward, StagePan_MoveXBackward, StagePan_MoveYLeft, StagePan_MoveYRight, StagePan_XAbsPos, StagePan_XRelPos,
							 	    StagePan_YAbsPos, StagePan_YRelPos, StagePan_XStepSize, StagePan_YStepSize, StagePan_RefPosList, StagePan_AddRefPos, 
							   	    StagePan_StartXAbsPos, StagePan_EndXRelPos, StagePan_StartYAbsPos, StagePan_EndYRelPos};


//==============================================================================
// Types

typedef struct {
	char*	name;		// name of reference position
	double	x;			// position in [mm]
	double	y;			// position in [mm]
} RefPosition_type;

typedef struct {
	StageMoveTypes		moveType;
	StageAxes			axis;
	double				moveVal;
} MoveCommand_type;

typedef enum {
	
	XYSTAGE_LED_IDLE,
	XYSTAGE_LED_MOVING,
	XYSTAGE_LED_ERROR
	
} StageLEDStates;


//==============================================================================
// Static functions

static RefPosition_type* 				init_RefPosition_type 				(char refName[], double xRefPos, double yRefPos);

static void								discard_RefPosition_type 			(RefPosition_type** refPosPtr);

static int CVICALLBACK 					UICtrls_CB 							(int panel, int control, int event, void *callbackData, int eventData1, int eventData2); 

static int CVICALLBACK 					UIPan_CB 							(int panel, int event, void *callbackData, int eventData1, int eventData2); 

static void CVICALLBACK 				SettingsMenu_CB 					(int menuBar, int menuItem, void *callbackData, int panel);

static int CVICALLBACK 					SettingsCtrls_CB 					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

	// displays the current interation index and the total number of iterations in a string control
//static void								DisplayNSteps						(int panHndl, int ctrlId, uInt32 nTotalSteps, uInt32 currentStep);

static BOOL 							ValidateRefPosName					(char inputStr[], void* dataPtr);  

static int								DisplayPanels 						(DAQLabModule_type* mod, BOOL visibleFlag); 

static void 							ChangeLEDStatus 					(XYStage_type* stage, StageLEDStates state); 

static void 							UpdatePositionDisplay 				(XYStage_type* stage);


//-----------------------------------------
// XY Stage Task Controller Callbacks
//-----------------------------------------

static int								ConfigureTC							(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);

static void								IterateTC							(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortIterationFlag);

static int								StartTC								(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);

static int								DoneTC								(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg);

static int								StoppedTC							(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg);

static int								TaskTreeStateChange	 				(TaskControl_type* taskControl, TaskTreeStates state, char** errorMsg);

static int				 				ResetTC 							(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);

static void				 				ErrorTC 							(TaskControl_type* taskControl, int errorID, char errorMsg[]);

static int								StageEventHandler					(TaskControl_type* taskControl, TCStates taskState, BOOL taskActive,  void* eventData, BOOL const* abortFlag, char** errorMsg);

//------------------------------------
// task controller module commands
//------------------------------------

static MoveCommand_type*				init_MoveCommand_type				(StageMoveTypes moveType, StageAxes axis, double moveVal);
static void								discard_MoveCommand_type			(MoveCommand_type** moveCommandPtr);


//==============================================================================
// Global variables

//==============================================================================
// Global functions


DAQLabModule_type* initalloc_XYStage (DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl)
{
	XYStage_type* stage	= NULL;
	
	if (!mod) {
		stage = malloc (sizeof(XYStage_type));
		if (!stage) return NULL;
	} else
		stage = (XYStage_type*) mod;
		
	// initialize base class
	initalloc_DAQLabModule(&stage->baseClass, className, instanceName, workspacePanHndl);
	
	// initialize Task Controller from child class
	
	stage->taskController 		= init_TaskControl_type (instanceName, stage, DLGetCommonThreadPoolHndl(), ConfigureTC, NULL, IterateTC, StartTC, ResetTC, DoneTC, StoppedTC, 
								  NULL, TaskTreeStateChange, NULL, StageEventHandler, ErrorTC);
	
	//---------------------------
	// Parent Level 0: DAQLabModule_type 
	
		// DATA
			
			
	
		// METHODS
	
			// overriding methods
	stage->baseClass.Discard 			= discard_XYStage;
	stage->baseClass.Load				= XYStage_Load;
	stage->baseClass.LoadCfg			= XYStage_LoadCfg;
	stage->baseClass.SaveCfg			= XYStage_SaveCfg;
	stage->baseClass.DisplayPanels		= DisplayPanels;
	
	//---------------------------
	// Child Level 1: XYStage_type 
	
		// DATA
		//------------------------------------------------------------
	
		// UI
	stage->controlPanHndl				= 0;
	stage->setPanHndl					= 0;
	stage->menuBarHndl					= 0;
	stage->menuIDSettings				= 0;
	
		// XY stage operation
	stage->xPos							= 0;
	stage->yPos							= 0;
	stage->reverseXAxis					= FALSE;
	stage->reverseYAxis					= FALSE;
	stage->xyRefPos						= ListCreate(sizeof(RefPosition_type*));
	
		// Stage velocity
	stage->stageVelocity				= NULL;
	stage->lowVelocity					= NULL;
	stage->midVelocity					= NULL;
	stage->highVelocity					= NULL;
	
		// Stage stepping mode
	stage->xStartAbsPos					= 0;
	stage->yStartAbsPos					= 0;
	stage->xEndRelPos					= 0;
	stage->yEndRelPos					= 0;
	stage->xStepSize					= 0;
	stage->yStepSize					= 0;
	stage->nXSteps						= 0;
	stage->nYSteps						= 0;
	
		// Safety limits
	stage->xMaximumLimit				= NULL;			// if parameter is saved, it will be loaded and applied to the stage
	stage->xMinimumLimit				= NULL;			// if parameter is saved, it will be loaded and applied to the stage
	stage->yMaximumLimit				= NULL;			// if parameter is saved, it will be loaded and applied to the stage
	stage->yMinimumLimit				= NULL;			// if parameter is saved, it will be loaded and applied to the stage
	
	
		// METHODS
		//------------------------------------------------------------
		
		// UI	
	stage->uiCtrlsCB					= UICtrls_CB;	// assign default controls callback to UI_XYStage.uir panel
	stage->uiPanelCB					= UIPan_CB;  	// assign default panel callback to UI_XYstage.uir
	
		// No functionality, override from child class
	stage->Move							= NULL;
	stage->UseJoystick					= NULL;
	stage->Stop							= NULL;
	stage->GetLimits					= NULL;
	stage->SetLimits					= NULL;
	stage->GetVelocity					= NULL;
	stage->SetVelocity					= NULL;
	stage->GetAbsPosition				= NULL;
	
	
	//----------------------------------------------------------
	if (!mod)
		return (DAQLabModule_type*) stage;
	else
		return NULL;
}

void discard_XYStage (DAQLabModule_type** mod)
{
	XYStage_type* 		stage 			= *(XYStage_type**) mod;
	RefPosition_type**	refPosPtr		= NULL;
	size_t				nRefPos			= ListNumItems(stage->xyRefPos);
	
	if (!stage) return;
	
	// task controller
	discard_TaskControl_type(&stage->taskController);
	
	// UI resources
	if (stage->controlPanHndl) {
		DiscardPanel(stage->controlPanHndl);
		stage->controlPanHndl = 0;
	}
	
	if (stage->setPanHndl) {
		DiscardPanel(stage->setPanHndl);
		stage->setPanHndl = 0;
	}
	
	for (size_t i = 1; i <= nRefPos; i++) {
		refPosPtr = ListGetPtrToItem(stage->xyRefPos, i);
		discard_RefPosition_type(refPosPtr);
	}
	
	ListDispose(stage->xyRefPos);
	stage->xyRefPos = 0;
	
	// velocity 
	OKfree(stage->stageVelocity);
	OKfree(stage->lowVelocity);
	OKfree(stage->midVelocity);
	OKfree(stage->highVelocity);
	
	// safety limits
	OKfree(stage->xMaximumLimit);
	OKfree(stage->xMinimumLimit);
	OKfree(stage->yMaximumLimit);
	OKfree(stage->yMinimumLimit);
	
	// discard DAQLabModule_type specific data
	discard_DAQLabModule(mod);
}

static RefPosition_type* init_RefPosition_type (char refName[], double xRefPos, double yRefPos)
{
	RefPosition_type* ref = malloc (sizeof(RefPosition_type));
	if (!ref) return NULL;
	
	ref->name 	= StrDup(refName);
	ref->x		= xRefPos;
	ref->y		= yRefPos;
	
	return ref;
}

static void	discard_RefPosition_type (RefPosition_type** refPosPtr)
{
	if (!*refPosPtr) return;
	
	OKfree((*refPosPtr)->name);
	OKfree(*refPosPtr);
}

int XYStage_Load (DAQLabModule_type* mod, int workspacePanHndl, char** errorMsg)
{
#define XYStage_Load_Err_CurrentPositionUnknown		-1
	
INIT_ERROR_INFO	
	
	XYStage_type* 	stage 				= (XYStage_type*) mod;  
	char			stepsizeName[100];
	
	// load panel resources
	stage->controlPanHndl = LoadPanel(workspacePanHndl, MOD_XYStage_UI, StagePan);
	
	// connect module data and user interface callbackFn to all direct controls in the panel
	SetCtrlsInPanCBInfo(mod, ((XYStage_type*)mod)->uiCtrlsCB, stage->controlPanHndl);
	
	// add panel callback function pointer and callback data
	SetPanelAttribute(stage->controlPanHndl, ATTR_CALLBACK_FUNCTION_POINTER, UIPan_CB);
	SetPanelAttribute(stage->controlPanHndl, ATTR_CALLBACK_DATA, mod);
	
	// add "Settings" menu bar item, callback data and callback function
	stage->menuBarHndl = NewMenuBar(stage->controlPanHndl);
	stage->menuIDSettings = NewMenu(stage->menuBarHndl, "Settings", -1);
	SetMenuBarAttribute(stage->menuBarHndl, 0, ATTR_SHOW_IMMEDIATE_ACTION_SYMBOL, 0);
	SetMenuBarAttribute(stage->menuBarHndl, stage->menuIDSettings, ATTR_CALLBACK_DATA, stage);
	SetMenuBarAttribute(stage->menuBarHndl, stage->menuIDSettings, ATTR_CALLBACK_FUNCTION_POINTER, SettingsMenu_CB);
	
	// change panel title to module instance name
	SetPanelAttribute(stage->controlPanHndl, ATTR_TITLE, mod->instanceName);
	
	// adjust position display precision
	SetCtrlAttribute(stage->controlPanHndl, StagePan_XAbsPos, ATTR_PRECISION, POS_DISPLAY_PRECISION);
	SetCtrlAttribute(stage->controlPanHndl, StagePan_XRelPos, ATTR_PRECISION, POS_DISPLAY_PRECISION);
	SetCtrlAttribute(stage->controlPanHndl, StagePan_StartXAbsPos, ATTR_PRECISION, POS_DISPLAY_PRECISION);
	SetCtrlAttribute(stage->controlPanHndl, StagePan_EndXRelPos, ATTR_PRECISION, POS_DISPLAY_PRECISION);
	//DisplayNSteps(stage->controlPanHndl, StagePan_XSteps, 0, 0);
	
	SetCtrlAttribute(stage->controlPanHndl, StagePan_YAbsPos, ATTR_PRECISION, POS_DISPLAY_PRECISION);
	SetCtrlAttribute(stage->controlPanHndl, StagePan_YRelPos, ATTR_PRECISION, POS_DISPLAY_PRECISION);
	SetCtrlAttribute(stage->controlPanHndl, StagePan_StartYAbsPos, ATTR_PRECISION, POS_DISPLAY_PRECISION);
	SetCtrlAttribute(stage->controlPanHndl, StagePan_EndYRelPos, ATTR_PRECISION, POS_DISPLAY_PRECISION);
	//DisplayNSteps(stage->controlPanHndl, StagePan_YSteps, 0, 0);
	
	// populate XY step sizes
	for (unsigned int i = 0; i < NumElem(XYStepSizes); i++) {
		Fmt(stepsizeName, "%s<%f[p*]", POS_DISPLAY_PRECISION, XYStepSizes[i]);
		InsertListItem(stage->controlPanHndl, StagePan_XStepSize, -1, stepsizeName, XYStepSizes[i]);
		InsertListItem(stage->controlPanHndl, StagePan_YStepSize, -1, stepsizeName, XYStepSizes[i]);   
	}
	
	// update XY step size in structure data to be the first element in the list
	stage->xStepSize = XYStepSizes[0] * 1e-3; // convert from [um] to [mm]
	stage->yStepSize = XYStepSizes[0] * 1e-3; // convert from [um] to [mm]
	
	//------------------------------------------
	// Update stage position
	//------------------------------------------
	
	if (stage->GetAbsPosition)
		errChk( (*stage->GetAbsPosition) (stage, &stage->xPos, &stage->yPos, &errorInfo.errMsg) );
		
	// update stage X absolute position if position has been determined
	// make visible stepper controls and update them if position has been determined
	SetCtrlVal(stage->controlPanHndl, StagePan_XAbsPos, stage->xPos * 1000);	  	// convert from [mm] to [um]
	SetCtrlVal(stage->controlPanHndl, StagePan_StartXAbsPos, stage->xPos * 1000);  	// convert from [mm] to [um]
		
	// add start pos data to structure;
	stage->xStartAbsPos = stage->xPos;
		
	// set negative and positive stage limits to be the same as the current stage position if not specified by hardware
	if (!stage->xMaximumLimit) {
		stage->xMaximumLimit = malloc(sizeof(double));
		*stage->xMaximumLimit = stage->xPos;
	}
		
	if (!stage->xMinimumLimit) {
		stage->xMinimumLimit = malloc(sizeof(double));
		*stage->xMinimumLimit = stage->xPos;
	}
		
	// update stage Y absolute position if position has been determined
	// make visible stepper controls and update them if position has been determined
	SetCtrlVal(stage->controlPanHndl, StagePan_YAbsPos, stage->yPos * 1000);	  	// convert from [mm] to [um]
	SetCtrlVal(stage->controlPanHndl, StagePan_StartYAbsPos, stage->yPos * 1000);  // convert from [mm] to [um]
		
	// add start pos data to structure;
	stage->yStartAbsPos = stage->yPos;
		
	// set negative and positive stage limits to be the same as the current stage position if not specified by hardware
	if (!stage->yMaximumLimit) {
		stage->yMaximumLimit = malloc(sizeof(double));
		*stage->yMaximumLimit = stage->yPos;
	}
		
	if (!stage->yMinimumLimit) {
		stage->yMinimumLimit = malloc(sizeof(double));
		*stage->yMinimumLimit = stage->yPos;
	}
		
	// dim/undim Joystick control box if such functionality is made or not available by the child class
	if (stage->UseJoystick)
		SetCtrlAttribute(stage->controlPanHndl, StagePan_Joystick, ATTR_DIMMED, 0);
	else
		SetCtrlAttribute(stage->controlPanHndl, StagePan_Joystick, ATTR_DIMMED, 1);
	
	// dim/undim velocity setting if such functionality is made available by the child class
	if (stage->SetVelocity)
		SetCtrlAttribute(stage->controlPanHndl, StagePan_StageVel, ATTR_DIMMED, 0);
	else
		SetCtrlAttribute(stage->controlPanHndl, StagePan_StageVel, ATTR_DIMMED, 1);
		
	// if a stage velocity has been determined (by the child class) and there are no settings loaded for
	// the different velocities, then make them equal to the current velocity of the stage
	if (stage->stageVelocity) {
		
		// set low velocity
		if (!stage->lowVelocity) {
			nullChk( stage->lowVelocity = malloc(sizeof(double)) );
			*stage->lowVelocity = *stage->stageVelocity;
		}
		
		// set mid velocity
		if (!stage->midVelocity) {
			nullChk( stage->midVelocity = malloc(sizeof(double)) );
			*stage->midVelocity = *stage->stageVelocity;
		}
		
		// set high velocity
		if (!stage->highVelocity) {
			nullChk( stage->highVelocity = malloc(sizeof(double)) );
			*stage->highVelocity = *stage->stageVelocity;
		}
	}
	
	// configure Z Stage Task Controller
	TaskControlEvent(stage->taskController, TC_Event_Configure, NULL, NULL); 
	
	// add reference positions if any
	size_t				nRefPos										= ListNumItems(stage->xyRefPos);
	char				refPosDisplayItem[MAX_REF_POS_LENGTH + 100]	= ""; 
	RefPosition_type*	refPos										= NULL;
	if (nRefPos) {
		// undim ref pos listbox
		SetCtrlAttribute(stage->controlPanHndl, StagePan_RefPosList, ATTR_DIMMED, 0);
		for (size_t i = 1; i <= nRefPos; i++) {
			refPos = *(RefPosition_type**)ListGetPtrToItem(stage->xyRefPos, i);
			// add reference position to listbox
			Fmt(refPosDisplayItem, REF_POS_LABEL_FORMAT, refPos->name, POS_DISPLAY_PRECISION, refPos->x * 1000, POS_DISPLAY_PRECISION, refPos->y * 1000);  	// display in [um]
			InsertListItem(stage->controlPanHndl, StagePan_RefPosList, -1, refPosDisplayItem, 0);
		}
	} else
		SetCtrlAttribute(stage->controlPanHndl, StagePan_RefPosList, ATTR_DIMMED, 1);
	
Error:
	
RETURN_ERROR_INFO
}

int	XYStage_LoadCfg	(DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  moduleElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERROR_INFO

	XYStage_type* 		stage	= (XYStage_type*) mod;
	
	// allocate memory to store values
	// Note: This must be done before initializing the attributes array!
	stage->xMaximumLimit 	= malloc(sizeof(double));
	stage->xMinimumLimit 	= malloc(sizeof(double));
	stage->yMaximumLimit 	= malloc(sizeof(double));
	stage->yMinimumLimit 	= malloc(sizeof(double));
	stage->lowVelocity		= malloc(sizeof(double));
	stage->midVelocity		= malloc(sizeof(double));
	stage->highVelocity		= malloc(sizeof(double));
	
	// initialize attributes array
	DAQLabXMLNode 		StageAttr[] 	= {	{"MinXPositionLimit", 	BasicData_Double, 	stage->xMinimumLimit},
								 			{"MaxXPositionLimit", 	BasicData_Double, 	stage->xMaximumLimit},
											{"MinYPositionLimit", 	BasicData_Double, 	stage->yMinimumLimit},
								 			{"MaxYPositionLimit", 	BasicData_Double, 	stage->yMaximumLimit},
											{"LowVelocity", 		BasicData_Double, 	stage->lowVelocity},
											{"MidVelocity", 		BasicData_Double, 	stage->midVelocity},
											{"HighVelocity", 		BasicData_Double, 	stage->highVelocity},
											{"ReverseXAxis",		BasicData_Bool,		&stage->reverseXAxis},
											{"ReverseYAxis",		BasicData_Bool,		&stage->reverseYAxis} };
											
	//--------------------------------------------------------------							
	// get stage attributes
	//--------------------------------------------------------------
	DLGetXMLElementAttributes(moduleElement, StageAttr, NumElem(StageAttr));
	
	//--------------------------------------------------------------
	// get saved reference positions
	//--------------------------------------------------------------
	ActiveXMLObj_IXMLDOMNodeList_	xmlRefPositionsNodeList		= 0;
	ActiveXMLObj_IXMLDOMNode_		xmlRefPositionsNode			= 0;
	ActiveXMLObj_IXMLDOMNodeList_	xmlRefPosNodeList			= 0;
	ActiveXMLObj_IXMLDOMNode_		xmlRefPosNode				= 0;
	long							nXMLElements;
	
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
	double				refXPos			= 0;
	double				refYPos			= 0;
	DAQLabXMLNode		refPosAttr[] 	= { {"Name", 		BasicData_CString, 	&refName}, 
										 	{"XPosition", 	BasicData_Double, 	&refXPos},
										 	{"YPosition", 	BasicData_Double, 	&refYPos} };
										  
	for (long i = 0; i < nXMLElements; i++) {
		errChk ( ActiveXML_IXMLDOMNodeList_Getitem(xmlRefPosNodeList, xmlErrorInfo, i, &xmlRefPosNode) );
		DLGetXMLNodeAttributes(xmlRefPosNode, refPosAttr, NumElem(refPosAttr));
		OKfreeCAHndl(xmlRefPosNode);
		refPos = init_RefPosition_type(refName, refXPos, refYPos);
		ListInsertItem(stage->xyRefPos, &refPos, END_OF_LIST);
	}
	
NoReferencePositions:
	
	// cleanup
	if (xmlRefPositionsNodeList) OKfreeCAHndl(xmlRefPositionsNodeList); 
	if (xmlRefPositionsNode) OKfreeCAHndl(xmlRefPositionsNode);
	if (xmlRefPosNodeList) OKfreeCAHndl(xmlRefPosNodeList); 
	
	return 0;
	
Error:   
	
	return errorInfo.error;
	
}

int XYStage_SaveCfg (DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ moduleElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERROR_INFO
	
	XYStage_type* 		stage			= (XYStage_type*) mod;
	DAQLabXMLNode 		StageAttr[] 	= {	{"MinXPositionLimit", 	BasicData_Double, 	stage->xMinimumLimit},
								 			{"MaxXPositionLimit", 	BasicData_Double, 	stage->xMaximumLimit},
											{"MinYPositionLimit", 	BasicData_Double, 	stage->yMinimumLimit},
								 			{"MaxYPositionLimit", 	BasicData_Double, 	stage->yMaximumLimit},
											{"LowVelocity", 		BasicData_Double, 	stage->lowVelocity},
											{"MidVelocity", 		BasicData_Double, 	stage->midVelocity},
											{"HighVelocity", 		BasicData_Double, 	stage->highVelocity},
											{"ReverseXAxis",		BasicData_Bool,		&stage->reverseXAxis},
											{"ReverseYAxis",		BasicData_Bool,		&stage->reverseYAxis} }; 
											
	// add safety limits as stage module attributes
	errChk( DLAddToXMLElem(xmlDOM, moduleElement, StageAttr, DL_ATTRIBUTE, NumElem(StageAttr), xmlErrorInfo) );
	
	// add reference positions element to module element
	ActiveXMLObj_IXMLDOMElement_	refPositionsXMLElement;
	ActiveXMLObj_IXMLDOMElement_	refPosXMLElement;
	RefPosition_type**				refPosPtr;
	
	errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "ReferencePositions", &refPositionsXMLElement) );
	errChk ( ActiveXML_IXMLDOMElement_appendChild (moduleElement, xmlErrorInfo, refPositionsXMLElement, NULL) );
	// add reference positions
	size_t	nPositions = ListNumItems(stage->xyRefPos);
	for (size_t i = 1; i <= nPositions; i++) {
		refPosPtr = ListGetPtrToItem(stage->xyRefPos, i);
		// add new reference position element to reference positions element
		errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "RefPos", &refPosXMLElement) );
		errChk ( ActiveXML_IXMLDOMElement_appendChild (refPositionsXMLElement, xmlErrorInfo, refPosXMLElement, NULL) );
		// add attributes to this reference position
		DAQLabXMLNode		refPosAttr[] 	= { {"Name", 		BasicData_CString, 	(*refPosPtr)->name}, 
										 		{"XPosition", 	BasicData_Double, 	&(*refPosPtr)->x},
										 		{"YPosition", 	BasicData_Double, 	&(*refPosPtr)->y} };
		
		errChk( DLAddToXMLElem(xmlDOM, refPosXMLElement, refPosAttr, DL_ATTRIBUTE, NumElem(refPosAttr), xmlErrorInfo) );   
	}
	
Error:   
	
	return errorInfo.error; 
}

static int DisplayPanels (DAQLabModule_type* mod, BOOL visibleFlag)
{
	XYStage_type* 	stage		= (XYStage_type*) mod; 
	
	if (visibleFlag)
		DisplayPanel(stage->controlPanHndl);
	else
		HidePanel(stage->controlPanHndl);
	
	return 0;
}

//----------------------------------------------------------------------------------------------
// Generic XY stage operation
//----------------------------------------------------------------------------------------------

static void ChangeLEDStatus (XYStage_type* stage, StageLEDStates state)
{
	switch (state) {
			
		case XYSTAGE_LED_IDLE:
			
			SetCtrlVal(stage->controlPanHndl, StagePan_Status, FALSE);
			break;
			
		case XYSTAGE_LED_MOVING:
			
			SetCtrlAttribute(stage->controlPanHndl, StagePan_Status, ATTR_ON_COLOR, VAL_GREEN);
			SetCtrlVal(stage->controlPanHndl, StagePan_Status, TRUE);
			break;
			
		case XYSTAGE_LED_ERROR:
			
			SetCtrlAttribute(stage->controlPanHndl, StagePan_Status, ATTR_ON_COLOR, VAL_RED);
			SetCtrlVal(stage->controlPanHndl, StagePan_Status, TRUE);
			break;
	}
	
}

static void UpdatePositionDisplay (XYStage_type* stage)
{
	// display current position
	SetCtrlVal(stage->controlPanHndl, StagePan_XAbsPos, stage->xPos * 1000);  	// convert from [mm] to [um] 
	SetCtrlVal(stage->controlPanHndl, StagePan_YAbsPos, stage->yPos * 1000);  	// convert from [mm] to [um] 
	
	// reset relative position
	SetCtrlVal(stage->controlPanHndl, StagePan_XRelPos, 0.0);
	SetCtrlVal(stage->controlPanHndl, StagePan_YRelPos, 0.0);
}

void XYStage_UpdateSteps (XYStage_type* stage)
{
	double		nSteps				= 0.0;
	BOOL		lockYStepSize   	= FALSE;
	int			xAxisStepSizeIdx	= 0;
	
	// get X step size
	GetCtrlVal(stage->controlPanHndl, StagePan_XStepSize, &stage->xStepSize);  							// read in [um]
	stage->xStepSize /= 1000;																			// convert to [mm]
	
	// get Y step size
	GetCtrlVal(stage->controlPanHndl, StagePan_LockYStepSize, &lockYStepSize);
	if (lockYStepSize) {
		GetCtrlIndex(stage->controlPanHndl, StagePan_XStepSize, &xAxisStepSizeIdx);
		SetCtrlIndex(stage->controlPanHndl, StagePan_YStepSize, xAxisStepSizeIdx);
		stage->yStepSize = stage->xStepSize;
	} else {
		GetCtrlVal(stage->controlPanHndl, StagePan_YStepSize, &stage->yStepSize);  							// read in [um]
		stage->yStepSize /= 1000;																			// convert to [mm]
	}
	
	// get X absolute start position
	GetCtrlVal(stage->controlPanHndl, StagePan_StartXAbsPos, &stage->xStartAbsPos);   					// read in [um]
	stage->xStartAbsPos /= 1000;																		// convert to [mm]
	
	// get Y absolute start position
	GetCtrlVal(stage->controlPanHndl, StagePan_StartYAbsPos, &stage->yStartAbsPos);   					// read in [um]
	stage->yStartAbsPos /= 1000;																		// convert to [mm]
	
	// get X relative end position
	GetCtrlVal(stage->controlPanHndl, StagePan_EndXRelPos, &stage->xEndRelPos);   						// read in [um]
	stage->xEndRelPos /= 1000;																			// convert to [mm]
	
	// get Y relative end position
	GetCtrlVal(stage->controlPanHndl, StagePan_EndYRelPos, &stage->yEndRelPos);   						// read in [um]
	stage->yEndRelPos /= 1000;																			// convert to [mm]
	
	// calculate X steps and adjust end X position to be multiple of xStepSize
	nSteps					= ceil(stage->xEndRelPos / stage->xStepSize);
	stage->nXSteps			= (size_t) fabs(nSteps);
	stage->xEndRelPos 		= nSteps * stage->xStepSize;
	
	// calculate Y steps and adjust end Y position to be multiple of yStepSize
	nSteps					= ceil(stage->yEndRelPos / stage->yStepSize);
	stage->nYSteps			= (size_t) fabs(nSteps);
	stage->yEndRelPos 		= nSteps * stage->yStepSize;
	
	// display X values
	SetCtrlVal(stage->controlPanHndl, StagePan_StartXAbsPos, stage->xStartAbsPos * 1000);				// convert from [mm] to [um]
	SetCtrlVal(stage->controlPanHndl, StagePan_EndXRelPos, stage->xEndRelPos * 1000);					// convert from [mm] to [um]
	//SetCtrlVal(stage->controlPanHndl, StagePan_XSteps, stage->nXSteps);
	
	// display Y values
	SetCtrlVal(stage->controlPanHndl, StagePan_StartYAbsPos, stage->yStartAbsPos * 1000);				// convert from [mm] to [um]
	SetCtrlVal(stage->controlPanHndl, StagePan_EndYRelPos, stage->yEndRelPos * 1000);					// convert from [mm] to [um]
	//SetCtrlVal(stage->controlPanHndl, StagePan_YSteps, stage->nYSteps);
	
	// configure Task Controller
	TaskControlEvent(stage->taskController, TC_Event_Configure, NULL, NULL);
	
}

void XYStage_SetStepCounter (XYStage_type* stage, uInt32 xSteps, uInt32 ySteps)
{
	
}

void XYStage_DimWhenRunning (XYStage_type* stage, BOOL dimmed)
{
	int menubarHndl = GetPanelMenuBar (stage->controlPanHndl);
	
	SetMenuBarAttribute(menubarHndl, 0, ATTR_DIMMED, dimmed);
	
	for (unsigned int i = 0; i < NumElem(UIXYStageCtrls); i++) {
		SetCtrlAttribute(stage->controlPanHndl, UIXYStageCtrls[i], ATTR_DIMMED, dimmed);
	}
}

static int CVICALLBACK UICtrls_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERROR_INFO	
	
	XYStage_type* 				stage 				= callbackData;
	int							refPosIdx			= 0;									// 0-based index of ref pos selected in the list
	RefPosition_type**			refPosPtr			= NULL;
	double						xStepSize			= 0;
	double						yStepSize			= 0;	  					
	double 						xDirection			= 0;
	double 						yDirection			= 0;
	double 						moveXAbsPos			= 0;   					
	double						moveXRelPos			= 0;
	double 						moveYAbsPos			= 0;   					
	double						moveYRelPos			= 0; 
	char						refPosDisplayItem[MAX_REF_POS_LENGTH + 100];
	
	switch (event) {
			
		case EVENT_COMMIT:
			
			GetCtrlVal(panel, StagePan_XStepSize, &xStepSize);  // get value in [um]
			xStepSize /= 1000;									// convert from [um] to [mm]
			GetCtrlVal(panel, StagePan_XAbsPos, &moveXAbsPos);  // get value in [um]
			moveXAbsPos /= 1000;								// convert from [um] to [mm]
			GetCtrlVal(panel, StagePan_XRelPos, &moveXRelPos);  // get value in [um]
			moveXRelPos /= 1000;								// convert from [um] to [mm]
			
			GetCtrlVal(panel, StagePan_YStepSize, &yStepSize);  // get value in [um]
			yStepSize /= 1000;									// convert from [um] to [mm]
			GetCtrlVal(panel, StagePan_YAbsPos, &moveYAbsPos);  // get value in [um]
			moveYAbsPos /= 1000;								// convert from [um] to [mm]
			GetCtrlVal(panel, StagePan_YRelPos, &moveYRelPos);  // get value in [um]
			moveYRelPos /= 1000;								// convert from [um] to [mm]
			
			// revert direction if needed
			if (stage->reverseXAxis) 
				xDirection = -1;
			else
				xDirection = 1;
			
			if (stage->reverseYAxis) 
				yDirection = -1;
			else
				yDirection = 1;
			
			switch (control) {
					
				case StagePan_MoveXForward:
					
					// move X axis in the positive direction if direction is positive, relative to current position with selected step size
					TaskControlEvent(stage->taskController, TC_Event_Custom, init_MoveCommand_type(XYSTAGE_MOVE_REL, XYSTAGE_X_AXIS, xDirection * xStepSize), (DiscardFptr_type)discard_MoveCommand_type); 
					break;
					
				case StagePan_MoveXBackward:
					
					// move X axis in the negative direction if direction is positive, relative to current position with selected step size
					TaskControlEvent(stage->taskController, TC_Event_Custom, init_MoveCommand_type(XYSTAGE_MOVE_REL, XYSTAGE_X_AXIS, -xDirection * xStepSize), (DiscardFptr_type)discard_MoveCommand_type); 
					break;
					
				case StagePan_MoveYRight:
					
					// move Y axis in the positive direction if direction is positive, relative to current position with selected step size
					TaskControlEvent(stage->taskController, TC_Event_Custom, init_MoveCommand_type(XYSTAGE_MOVE_REL, XYSTAGE_Y_AXIS, yDirection * yStepSize), (DiscardFptr_type)discard_MoveCommand_type); 
					break;
					
				case StagePan_MoveYLeft:
					
					// move Y axis in the negative direction if direction is positive, relative to current position with selected step size
					TaskControlEvent(stage->taskController, TC_Event_Custom, init_MoveCommand_type(XYSTAGE_MOVE_REL, XYSTAGE_Y_AXIS, -yDirection * yStepSize), (DiscardFptr_type)discard_MoveCommand_type); 
					break;
					
				case StagePan_Stop:
					
					// stop stage motion
					if (stage->Stop)
						errChk( (*stage->Stop)	(stage, &errorInfo.errMsg) );
					break;
				
				case StagePan_XAbsPos:
					
					// move to X absolute position with selected step size
					TaskControlEvent(stage->taskController, TC_Event_Custom, init_MoveCommand_type(XYSTAGE_MOVE_ABS, XYSTAGE_X_AXIS, moveXAbsPos), (DiscardFptr_type)discard_MoveCommand_type); 
					break;
					
				case StagePan_YAbsPos:
					
					// move to Y absolute position with selected step size
					TaskControlEvent(stage->taskController, TC_Event_Custom, init_MoveCommand_type(XYSTAGE_MOVE_ABS, XYSTAGE_Y_AXIS, moveYAbsPos), (DiscardFptr_type)discard_MoveCommand_type); 
					break;
					
				case StagePan_XRelPos:
					
					// move X axis relative to current position
					TaskControlEvent(stage->taskController, TC_Event_Custom, init_MoveCommand_type(XYSTAGE_MOVE_REL, XYSTAGE_X_AXIS, moveXRelPos), (DiscardFptr_type)discard_MoveCommand_type); 
					break;
					
				case StagePan_YRelPos:
					
					// move Y axis relative to current position
					TaskControlEvent(stage->taskController, TC_Event_Custom, init_MoveCommand_type(XYSTAGE_MOVE_REL, XYSTAGE_Y_AXIS, moveYRelPos), (DiscardFptr_type)discard_MoveCommand_type); 
					break;
					
				case StagePan_StartXAbsPos:
				case StagePan_EndXRelPos:
				case StagePan_XStepSize:
				case StagePan_StartYAbsPos:
				case StagePan_EndYRelPos:
				case StagePan_YStepSize:
				
					XYStage_UpdateSteps	(stage);
					break;
					
				case StagePan_AddRefPos:
					
					char* 				newRefPosName		= NULL;
					RefPosition_type*	newRefPos			= NULL;
					
					newRefPosName = DLGetUINameInput("New Reference Position", MAX_REF_POS_LENGTH, ValidateRefPosName, &stage->xyRefPos); 
					if (!newRefPosName) return 0;	// operation cancelled or an error occured.
					
					// add new reference position to structure data
					newRefPos = init_RefPosition_type(newRefPosName, moveXAbsPos, moveYAbsPos);
					if (!newRefPos) goto AddRefPosError;
					
					if (!ListInsertItem(stage->xyRefPos, &newRefPos, END_OF_LIST)) goto AddRefPosError;
					
					// undim ref pos listbox
					SetCtrlAttribute(panel, StagePan_RefPosList, ATTR_DIMMED, 0);
					
					// add reference position to listbox
					Fmt(refPosDisplayItem, REF_POS_LABEL_FORMAT, newRefPosName, POS_DISPLAY_PRECISION, moveXAbsPos * 1000, POS_DISPLAY_PRECISION, moveYAbsPos * 1000);  		// display in [um]
					OKfree(newRefPosName);
					InsertListItem(panel, StagePan_RefPosList, -1, refPosDisplayItem, 0); 
					
					break;
					AddRefPosError:
					OKfree(newRefPosName);
					discard_RefPosition_type(&newRefPos);
					DLMsg("Error. Could not add new reference position. Out of memory?\n\n", 1);
					return 0;
					
				case StagePan_Joystick:
					
					BOOL	useJoystick	= FALSE;
					
					GetCtrlVal(panel, control, &useJoystick);
					
					if (stage->UseJoystick)
						errChk( (*stage->UseJoystick)	(stage, useJoystick, &errorInfo.errMsg) );
					break;
					
				case StagePan_LockYStepSize:
					
					BOOL	lockYAxis			= FALSE;
					int		xAxisStepSizeIdx	= 0;
					
					GetCtrlVal(panel, control, &lockYAxis);
					GetCtrlIndex(panel, StagePan_XStepSize, &xAxisStepSizeIdx);
					
					if (lockYAxis) {
						SetCtrlAttribute(panel, StagePan_YStepSize, ATTR_DIMMED, TRUE);
						SetCtrlIndex(panel, StagePan_YStepSize, xAxisStepSizeIdx);
						stage->yStepSize = stage->xStepSize;
					} else
						SetCtrlAttribute(panel, StagePan_YStepSize, ATTR_DIMMED, FALSE);
					
					break;
					
				case StagePan_StageVel:
					
					int		velIdx;
					
					GetCtrlIndex(panel, control, &velIdx);
					
					if (!stage->SetVelocity) break; // do nothing if there is no such functionality implemented by the child class
					
					if (velIdx == 0 && stage->lowVelocity)
						errChk( (*stage->SetVelocity) (stage, *stage->lowVelocity, &errorInfo.errMsg) );
					else
						if (velIdx == 1 && stage->midVelocity)
						errChk( (*stage->SetVelocity) (stage, *stage->midVelocity, &errorInfo.errMsg) );
						else
							if (velIdx == 2 && stage->highVelocity)
								errChk( (*stage->SetVelocity)	(stage, *stage->highVelocity, &errorInfo.errMsg) );
					
					break;
					
			}
			
			break;
			
		case EVENT_KEYPRESS:		// delete reference position from list
			
			switch (control) {
					
				case StagePan_RefPosList:
					
					// if Del key is pressed
					if (eventData1 == VAL_FWD_DELETE_VKEY) {
						// get ref pos idx
						GetCtrlIndex(panel, control, &refPosIdx);
						// remove from list box
						DeleteListItem(panel, control, refPosIdx, 1);
						// remove from structure data
						refPosPtr = ListGetPtrToItem(stage->xyRefPos, refPosIdx + 1);
						discard_RefPosition_type(refPosPtr);
						ListRemoveItem(stage->xyRefPos, 0, refPosIdx + 1);
						// dim listbox if there are no more items
						if (!ListNumItems(stage->xyRefPos))
							SetCtrlAttribute(panel, StagePan_RefPosList, ATTR_DIMMED, 1);
						
					}
					break;
			}
			break;
			
		case EVENT_LEFT_DOUBLE_CLICK: // moves stage to selected reference position
			
			switch (control) {
					
				case StagePan_RefPosList:
					
					// get ref pos idx
					GetCtrlIndex(panel, control, &refPosIdx);
					refPosPtr = ListGetPtrToItem(stage->xyRefPos, refPosIdx + 1);
					
					// move X axis to reference position
					TaskControlEvent(stage->taskController, TC_Event_Custom, init_MoveCommand_type(XYSTAGE_MOVE_ABS, XYSTAGE_X_AXIS, (*refPosPtr)->x), (DiscardFptr_type)discard_MoveCommand_type); 
					
					// move Y axis to reference position
					TaskControlEvent(stage->taskController, TC_Event_Custom, init_MoveCommand_type(XYSTAGE_MOVE_ABS, XYSTAGE_Y_AXIS, (*refPosPtr)->y), (DiscardFptr_type)discard_MoveCommand_type); 
					
					break;
			}
			break;
			
		case EVENT_RIGHT_DOUBLE_CLICK: 
			
			switch (control) {
					
				case StagePan_RefPosList:  	// update reference position value
					
					// get ref pos idx
					GetCtrlIndex(panel, control, &refPosIdx);
					// update reference position in structure data
					refPosPtr = ListGetPtrToItem(stage->xyRefPos, refPosIdx + 1);
					(*refPosPtr)->x = stage->xPos;
					(*refPosPtr)->y = stage->yPos;
					// update reference position label
					Fmt(refPosDisplayItem, REF_POS_LABEL_FORMAT, (*refPosPtr)->name, POS_DISPLAY_PRECISION, stage->xPos * 1000, POS_DISPLAY_PRECISION, stage->yPos * 1000);    // display in [um]
					// store listbox position value in [mm] 
					ReplaceListItem(panel, StagePan_RefPosList, refPosIdx, refPosDisplayItem, 0); 			
					break;
					
				case StagePan_StartXAbsPos:	// set start position to match current position
					
					SetCtrlVal(panel, control, stage->xPos * 1000); 							// convert to [um]
					XYStage_UpdateSteps(stage);  
					break;
					
				case StagePan_StartYAbsPos:	// set start position to match current position
					
					SetCtrlVal(panel, control, stage->yPos * 1000); 							// convert to [um]
					XYStage_UpdateSteps(stage);  
					break;
					
				case StagePan_EndXRelPos: 	// set relative end position (to absolute start position) to match current stage position
					
					SetCtrlVal(panel, control, (stage->xPos - stage->xStartAbsPos) * 1000);   	// convert to [um] 
					XYStage_UpdateSteps(stage);
					break;
					
				case StagePan_EndYRelPos: 	// set relative end position (to absolute start position) to match current stage position
					
					SetCtrlVal(panel, control, (stage->yPos - stage->yStartAbsPos) * 1000);   	// convert to [um] 
					XYStage_UpdateSteps(stage);
					break;
					
			}
			break;
			
		case EVENT_MOUSE_WHEEL_SCROLL:
			
			// revert direction if needed
			if (stage->reverseXAxis) 
				xDirection = -1;
			else
				xDirection = 1;
			
			if (stage->reverseYAxis) 
				yDirection = -1;
			else
				yDirection = 1;
			
			GetCtrlVal(panel, StagePan_XStepSize, &xStepSize);  // get value in [um]
			xStepSize /= 1000;									// convert from [um] to [mm]
			
			GetCtrlVal(panel, StagePan_YStepSize, &yStepSize);  // get value in [um]
			yStepSize /= 1000;									// convert from [um] to [mm]
			
			switch (control) {
					
				case StagePan_XAbsPos:				// adjust absolute position
					
					switch (eventData1) {
					
						case MOUSE_WHEEL_SCROLL_UP:
							
							SetCtrlVal(panel, control, (stage->xPos + xDirection * xStepSize) * 1000);	// convert back to [um]
							// move relative to current position
							TaskControlEvent(stage->taskController, TC_Event_Custom, init_MoveCommand_type(XYSTAGE_MOVE_REL, XYSTAGE_X_AXIS, xDirection * xStepSize), (DiscardFptr_type)discard_MoveCommand_type); 
							return 1;
					
						case MOUSE_WHEEL_SCROLL_DOWN:
					
							SetCtrlVal(panel, control, (stage->xPos - xDirection * xStepSize) * 1000);	// convert back to [um] 
							// move relative to current position
							TaskControlEvent(stage->taskController, TC_Event_Custom, init_MoveCommand_type(XYSTAGE_MOVE_REL, XYSTAGE_X_AXIS, -xDirection * xStepSize), (DiscardFptr_type)discard_MoveCommand_type); 
							return 1;
					
					}
					break;
					
				case StagePan_YAbsPos:				// adjust absolute position
					
					switch (eventData1) {
					
						case MOUSE_WHEEL_SCROLL_UP:
							
							SetCtrlVal(panel, control, (stage->yPos + yDirection * yStepSize) * 1000);	// convert back to [um]
							// move relative to current position
							TaskControlEvent(stage->taskController, TC_Event_Custom, init_MoveCommand_type(XYSTAGE_MOVE_REL, XYSTAGE_Y_AXIS, yDirection * yStepSize), (DiscardFptr_type)discard_MoveCommand_type); 
							return 1;
					
						case MOUSE_WHEEL_SCROLL_DOWN:
					
							SetCtrlVal(panel, control, (stage->yPos - yDirection * yStepSize) * 1000);	// convert back to [um] 
							// move relative to current position
							TaskControlEvent(stage->taskController, TC_Event_Custom, init_MoveCommand_type(XYSTAGE_MOVE_REL, XYSTAGE_Y_AXIS, -yDirection * yStepSize), (DiscardFptr_type)discard_MoveCommand_type); 
							return 1;
					
					}
					break;
					
				case StagePan_XRelPos:				// adjust relative position
					
					switch (eventData1) {
					
						case MOUSE_WHEEL_SCROLL_UP:
							
							SetCtrlVal(panel, control, xDirection * xStepSize * 1000);					// convert back to [um]
							// move relative to current position
							TaskControlEvent(stage->taskController, TC_Event_Custom, init_MoveCommand_type(XYSTAGE_MOVE_REL, XYSTAGE_X_AXIS, xDirection * xStepSize), (DiscardFptr_type)discard_MoveCommand_type); 
							return 1;
					
						case MOUSE_WHEEL_SCROLL_DOWN:
					
							SetCtrlVal(panel, control, -xDirection * xStepSize * 1000);					// convert back to [um]
							// move relative to current position
							TaskControlEvent(stage->taskController, TC_Event_Custom, init_MoveCommand_type(XYSTAGE_MOVE_REL, XYSTAGE_X_AXIS, -xDirection * xStepSize), (DiscardFptr_type)discard_MoveCommand_type); 
							return 1;
					}
					break;
					
				case StagePan_YRelPos:				// adjust relative position
					
					switch (eventData1) {
					
						case MOUSE_WHEEL_SCROLL_UP:
							
							SetCtrlVal(panel, control, yDirection * yStepSize * 1000);					// convert back to [um]
							// move relative to current position
							TaskControlEvent(stage->taskController, TC_Event_Custom, init_MoveCommand_type(XYSTAGE_MOVE_REL, XYSTAGE_Y_AXIS, yDirection * yStepSize), (DiscardFptr_type)discard_MoveCommand_type); 
							return 1;
					
						case MOUSE_WHEEL_SCROLL_DOWN:
					
							SetCtrlVal(panel, control, -yDirection * yStepSize * 1000);					// convert back to [um]
							// move relative to current position
							TaskControlEvent(stage->taskController, TC_Event_Custom, init_MoveCommand_type(XYSTAGE_MOVE_REL, XYSTAGE_Y_AXIS, -yDirection * yStepSize), (DiscardFptr_type)discard_MoveCommand_type); 
							return 1;
					}
					break;
					
				case StagePan_StartXAbsPos:
					
					switch (eventData1) {
					
						case MOUSE_WHEEL_SCROLL_UP:
							
							SetCtrlVal(panel, control, (stage->xStartAbsPos + xStepSize) * 1000);		// convert back to [um]
							break;
					
						case MOUSE_WHEEL_SCROLL_DOWN:
					
							SetCtrlVal(panel, control, (stage->xStartAbsPos - xStepSize) * 1000);		// convert back to [um]
							break;
					
					}
					break;
					
				case StagePan_StartYAbsPos:
					
					switch (eventData1) {
					
						case MOUSE_WHEEL_SCROLL_UP:
							
							SetCtrlVal(panel, control, (stage->yStartAbsPos + yStepSize) * 1000);		// convert back to [um]
							break;
					
						case MOUSE_WHEEL_SCROLL_DOWN:
					
							SetCtrlVal(panel, control, (stage->yStartAbsPos - yStepSize) * 1000);		// convert back to [um]
							break;
					
					}
					break;
					
				case StagePan_EndXRelPos:
					
					switch (eventData1) {
					
						case MOUSE_WHEEL_SCROLL_UP:
							
							SetCtrlVal(panel, control, (stage->xEndRelPos + xStepSize) * 1000);		// convert back to [um]
							break;
					
						case MOUSE_WHEEL_SCROLL_DOWN:
					
							SetCtrlVal(panel, control, (stage->xEndRelPos - xStepSize) * 1000);		// convert back to [um]
							break;
					
					}
					break;
					
				case StagePan_EndYRelPos:
					
					switch (eventData1) {
					
						case MOUSE_WHEEL_SCROLL_UP:
							
							SetCtrlVal(panel, control, (stage->yEndRelPos + yStepSize) * 1000);		// convert back to [um]
							break;
					
						case MOUSE_WHEEL_SCROLL_DOWN:
					
							SetCtrlVal(panel, control, (stage->yEndRelPos - yStepSize) * 1000);		// convert back to [um]
							break;
					
					}
					break;
			}
	}
	
Error:
	
PRINT_ERROR_INFO
	return 0;
}

static int CVICALLBACK UIPan_CB (int panel, int event, void *callbackData, int eventData1, int eventData2)
{
//	XYStage_type* 	stage 	= callbackData;
	
	return 0;
}
   
static void CVICALLBACK SettingsMenu_CB (int menuBar, int menuItem, void *callbackData, int panel)
{
	XYStage_type* 	stage 			= callbackData;
	int				parentPanHndl	= 0;
	
	GetPanelAttribute(panel, ATTR_PANEL_PARENT, &parentPanHndl);
	
	// load settings panel resources if not already loaded
	if (!stage->setPanHndl)
		stage->setPanHndl = LoadPanel(parentPanHndl, MOD_XYStage_UI, SetPan);
	// add callback data and function
	SetCtrlsInPanCBInfo(stage, SettingsCtrls_CB, stage->setPanHndl);
	
	// update limits
	SetCtrlVal(stage->setPanHndl, SetPan_XMinLimit, *stage->xMinimumLimit * 1000); 	// convert from [mm] to [um]
	SetCtrlVal(stage->setPanHndl, SetPan_XMaxLimit, *stage->xMaximumLimit * 1000); 	// convert from [mm] to [um]
	SetCtrlVal(stage->setPanHndl, SetPan_YMinLimit, *stage->yMinimumLimit * 1000); 	// convert from [mm] to [um]
	SetCtrlVal(stage->setPanHndl, SetPan_YMaxLimit, *stage->yMaximumLimit * 1000); 	// convert from [mm] to [um]
	
	// update reverse axis
	SetCtrlVal(stage->setPanHndl, SetPan_ReverseX, stage->reverseXAxis);
	SetCtrlVal(stage->setPanHndl, SetPan_ReverseY, stage->reverseYAxis);
	
	// update stage low velocity setting
	if (stage->lowVelocity) {
		SetCtrlVal(stage->setPanHndl, SetPan_LowVelocity, *stage->lowVelocity);			
		SetCtrlAttribute(stage->setPanHndl, SetPan_LowVelocity, ATTR_DIMMED, 0);
	} else
		SetCtrlAttribute(stage->setPanHndl, SetPan_LowVelocity, ATTR_DIMMED, 1);
	
	// update stage mid velocity setting
	if (stage->midVelocity) {
		SetCtrlVal(stage->setPanHndl, SetPan_MidVelocity, *stage->midVelocity);			
		SetCtrlAttribute(stage->setPanHndl, SetPan_MidVelocity, ATTR_DIMMED, 0);
	} else
		SetCtrlAttribute(stage->setPanHndl, SetPan_MidVelocity, ATTR_DIMMED, 1);
	
	// update stage high velocity setting
	if (stage->highVelocity) {
		SetCtrlVal(stage->setPanHndl, SetPan_HighVelocity, *stage->highVelocity);			
		SetCtrlAttribute(stage->setPanHndl, SetPan_HighVelocity, ATTR_DIMMED, 0);
	} else
		SetCtrlAttribute(stage->setPanHndl, SetPan_HighVelocity, ATTR_DIMMED, 1);
	
	DisplayPanel(stage->setPanHndl);
}

static int CVICALLBACK SettingsCtrls_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERROR_INFO

	XYStage_type* 	stage 	= callbackData;
	
	switch (event) {
		
		case EVENT_COMMIT:
			
			switch (control) {
					
				case SetPan_Close:
					
					DiscardPanel(stage->setPanHndl);
					stage->setPanHndl = 0;
					break;
					
				case SetPan_XMinLimit:
				case SetPan_XMaxLimit:
				case SetPan_YMinLimit:
				case SetPan_YMaxLimit:
					
					// update limits in structure data
					double	newXMinLimit		= 0;
					double	newXMaxLimit		= 0;
					double	newYMinLimit		= 0;
					double	newYMaxLimit		= 0;
					
					GetCtrlVal(panel, SetPan_XMinLimit, &newXMinLimit);
					GetCtrlVal(panel, SetPan_XMaxLimit, &newXMaxLimit);
					GetCtrlVal(panel, SetPan_YMinLimit, &newYMinLimit);
					GetCtrlVal(panel, SetPan_YMaxLimit, &newYMaxLimit); 
					
					newXMinLimit *= 0.001; 	// convert from [um] to [mm]
					newXMaxLimit *= 0.001; 	// convert from [um] to [mm]
					newYMinLimit *= 0.001; 	// convert from [um] to [mm]
					newYMaxLimit *= 0.001; 	// convert from [um] to [mm]
					
					// if given, call hardware specific function to set these limits
					if (stage->SetLimits)
						if ( (errorInfo.error = (*stage->SetLimits) (stage, newXMinLimit, newXMaxLimit, newYMinLimit, newYMaxLimit, &errorInfo.errMsg)) < 0) {
							// error setting new limits, return to old values
							SetCtrlVal(panel, SetPan_XMinLimit, *stage->xMinimumLimit * 1000);		// convert from [mm] to [um]
							SetCtrlVal(panel, SetPan_XMaxLimit, *stage->xMaximumLimit * 1000);		// convert from [mm] to [um]
							SetCtrlVal(panel, SetPan_YMinLimit, *stage->yMinimumLimit * 1000);		// convert from [mm] to [um]
							SetCtrlVal(panel, SetPan_YMaxLimit, *stage->yMaximumLimit * 1000);		// convert from [mm] to [um]
							goto Error;
						} else {
							// new limits set succesfully, update structure data with correct limits
							*stage->xMinimumLimit = newXMinLimit;
							*stage->xMaximumLimit = newXMaxLimit;
							*stage->yMinimumLimit = newYMinLimit;
							*stage->yMaximumLimit = newYMaxLimit;
						}
					
					break;
					
				case SetPan_LowVelocity:
					
					// set velocity value in structure data
					GetCtrlVal(panel, control, stage->lowVelocity);
					
					// if this velocity setting is selected, then set stage velocity
					{	
						int		velIdx;	// velocity settings index to be applied to the stage:
										// 0 - low, 1 - mid, 2 - high.
						GetCtrlIndex(stage->controlPanHndl, StagePan_StageVel, &velIdx);
						if (velIdx == 0 && stage->SetVelocity)
							errChk( (*stage->SetVelocity) (stage, *stage->lowVelocity, &errorInfo.errMsg) );
					}
					break;
					
				case SetPan_MidVelocity:
					
					// set velocity value in structure data
					GetCtrlVal(panel, control, stage->midVelocity);
					
					// if this velocity setting is selected, then set stage velocity
					{	
						int		velIdx;	// velocity settings index to be applied to the stage:
										// 0 - low, 1 - mid, 2 - high.
						GetCtrlIndex(stage->controlPanHndl, StagePan_StageVel, &velIdx);
						if (velIdx == 1 && stage->SetVelocity)
							errChk( (*stage->SetVelocity) (stage, *stage->midVelocity, &errorInfo.errMsg) );
					}
					break;
					
				case SetPan_HighVelocity:
					
					// set velocity value in structure data
					GetCtrlVal(panel, control, stage->highVelocity);
					
					// if this velocity setting is selected, then set stage velocity
					{	int		velIdx;	// velocity settings index to be applied to the stage:
										// 0 - low, 1 - mid, 2 - high.
						GetCtrlIndex(stage->controlPanHndl, StagePan_StageVel, &velIdx);
						if (velIdx == 0 && stage->SetVelocity)
							errChk( (*stage->SetVelocity) (stage, *stage->highVelocity, &errorInfo.errMsg) );
					}
					break;
					
				case SetPan_ReverseX:
					
					GetCtrlVal(panel, control, &stage->reverseXAxis);
					break;
					
				case SetPan_ReverseY:
					
					GetCtrlVal(panel, control, &stage->reverseYAxis);
					break;
					
			}
			
			break;
	}
	
Error:
	
PRINT_ERROR_INFO
	return 0;
}
   
static BOOL ValidateRefPosName (char inputStr[], void* dataPtr)
{
	ListType 				refPosList	= *(ListType*)dataPtr;
	size_t					nRefPos		= ListNumItems(refPosList);
	RefPosition_type**		refPosPtr	= NULL;
	
	for (size_t i = 1; i <= nRefPos; i++) {
		refPosPtr = ListGetPtrToItem(refPosList, i);
		// if ref pos with same name is found, input is not valid
		if (!strcmp((*refPosPtr)->name, inputStr)) return FALSE;
	}
	
	return TRUE;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller Callbacks
//------------------------------------------------------------------------------------------------------------------------------------------

static int ConfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	return 0;
}
static void IterateTC (TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortIterationFlag)
{
	TaskControlIterationDone(taskControl, 0, "", FALSE);
}

static int StartTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	return 0;
}

static int DoneTC (TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg)
{
	return 0;
}

static int StoppedTC (TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg)
{
	return 0;
}

static int TaskTreeStateChange (TaskControl_type* taskControl, TaskTreeStates state, char** errorMsg)
{
	return 0;	
}

static int ResetTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	return 0;	
}

static void ErrorTC (TaskControl_type* taskControl, int errorID, char errorMsg[])
{
	DLMsg(errorMsg, 1);
}

static int StageEventHandler (TaskControl_type* taskControl, TCStates taskState, BOOL taskActive,  void* eventData, BOOL const* abortFlag, char** errorMsg)
{
INIT_ERROR_INFO

	MoveCommand_type*	moveCommand = eventData;
	XYStage_type*		stage		= GetTaskControlModuleData(taskControl);

	ChangeLEDStatus(stage, XYSTAGE_LED_MOVING);
	
	// move stage
	if (stage->Move)
		errChk( (*stage->Move)	(stage, moveCommand->moveType, moveCommand->axis, moveCommand->moveVal, &errorInfo.errMsg) );
	
	// movement assumed to be complete, check position if method given
	if (stage->GetAbsPosition)
		errChk( (*stage->GetAbsPosition)	(stage, &stage->xPos, &stage->yPos, &errorInfo.errMsg) );
	else
		switch (moveCommand->moveType) {
			
			case XYSTAGE_MOVE_REL:
				
				switch (moveCommand->axis) {
						
					case XYSTAGE_X_AXIS:
						
						stage->xPos += moveCommand->moveVal;  
						break;
						
					case XYSTAGE_Y_AXIS:
						
						stage->yPos += moveCommand->moveVal;  
						break;
				}
				break;
				
			case XYSTAGE_MOVE_ABS:
				
				switch (moveCommand->axis) {
						
					case XYSTAGE_X_AXIS:
						
						stage->xPos = moveCommand->moveVal;
						break;
						
					case XYSTAGE_Y_AXIS:
						
						stage->yPos = moveCommand->moveVal;
						break;
				}
				break;
		}
		
	
	ChangeLEDStatus(stage, XYSTAGE_LED_IDLE);
	UpdatePositionDisplay(stage);
	
	return 0;
	
Error:
	
	ChangeLEDStatus(stage, XYSTAGE_LED_ERROR);
	
	return errorInfo.error;
}

static MoveCommand_type* init_MoveCommand_type (StageMoveTypes moveType, StageAxes axis, double moveVal)
{
	MoveCommand_type* moveCommand = malloc(sizeof(MoveCommand_type));
	if (!moveCommand) return NULL;
	
	moveCommand->moveType 	= moveType;
	moveCommand->axis		= axis;
	moveCommand->moveVal	= moveVal;
	
	return moveCommand;
}

static void discard_MoveCommand_type (MoveCommand_type** moveCommandPtr)
{
	if (!*moveCommandPtr) return;
	
	OKfree(*moveCommandPtr);
}
