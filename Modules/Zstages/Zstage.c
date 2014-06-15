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

#define MOD_Zstage_UI_ZStage 		"./Modules/Zstages/UI_ZStage.uir"

	// maximum entry length in characters for reference positions
#define Zstage_MAX_REF_POS_LENGTH	50

	// number of digits after the decimal point used to display stage position in [um]
#define Zstage_POSITION_DISPLAY_PRECISION 1

	// step sizes in [um] displayed in the ZStagePan_ZStepSize control
const double ZStepSizes[] 			= { 0.1, 0.2, 0.5, 1, 1.5, 2, 5, 10, 
							  			20, 50, 100, 200, 500, 1000 };  
	
//==============================================================================
// Types

typedef struct {
	char*	name;			// name of reference position
	double	val;			// position in [mm]
} Zstage_RefPosition_type;

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

static int							Zstage_DisplayPanels			(DAQLabModule_type* self, BOOL visibleFlag); 

static int							Zstage_ChangeLEDStatus			(Zstage_type* zstage, Zstage_LED_type status);

static int							Zstage_UpdatePositionDisplay	(Zstage_type* zstage);

static Zstage_RefPosition_type*		init_Zstage_RefPosition_type	(char refName[], double refVal);

static void							discard_Zstage_RefPosition_type	(Zstage_RefPosition_type** a);

static BOOL 						Zstage_ValidateRefPosName		(char inputStr[], void* dataPtr);  

static int CVICALLBACK 				Zstage_UICtrls_CB 				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 				Zstage_UIPan_CB 				(int panel, int event, void *callbackData, int eventData1, int eventData2);



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

/// HIFN  Allocates memory and initializes a generic Zstage. No functionality implemented.
/// HIPAR mod/ if NULL both memory allocation and initialization must take place.
/// HIPAR mod/ if !NULL the function performs only an initialization of a DAQLabModule_type.
/// HIRET address of a DAQLabModule_type if memory allocation and initialization took place.
/// HIRET NULL if only initialization was performed. 
DAQLabModule_type*	initalloc_Zstage (DAQLabModule_type* mod, char className[], char instanceName[])
{
	Zstage_type* zstage;
	
	if (!mod) {
		zstage = malloc (sizeof(Zstage_type));
		if (!zstage) return NULL;
	} else
		zstage = (Zstage_type*) mod;
		
	// initialize base class
	initalloc_DAQLabModule(&zstage->module_base, className, instanceName);
	//------------------------------------------------------------
	
	//---------------------------
	// Parent Level 0: DAQLabModule_type 
	
		// DATA
	zstage->module_base.taskControl		= NULL;  //init_TaskControl_type (MOD_Zstage_NAME, Zstage_ConfigureTC, Zstage_IterateTC, Zstage_StartTC, Zstage_ResetTC,
												 //				 Zstage_DoneTC, Zstage_StoppedTC, NULL, Zstage_EventHandler,Zstage_ErrorTC);   
		// connect ZStage module data to Task Controller
												 //SetTaskControlModuleData(zstage->module_base.taskControl, zstage);
		// METHODS
	
		// overriding methods
	zstage->module_base.Discard 		= discard_Zstage;
	zstage->module_base.Load			= Zstage_Load;
	zstage->module_base.LoadCfg			= NULL; //Zstage_LoadCfg;
	zstage->module_base.DisplayPanels	= Zstage_DisplayPanels;
	
	//---------------------------
	// Child Level 1: Zstage_type 
	
		// DATA
	zstage->controlPanHndl			= 0;
	zstage->zPos					= NULL; 
	zstage->revertDirection			= FALSE;
	zstage->zRefPos					= ListCreate(sizeof(Zstage_RefPosition_type*));
	zstage->zULimPos				= NULL;
	zstage->zLLimPos				= NULL;
	zstage->zStackStep				= 0;
	


		// METHODS
		
		// assign default controls callback to UI_ZStage.uir panel
	zstage->uiCtrlsCB				= Zstage_UICtrls_CB;
	
		// assign default panel callback to UI_Zstage.uir
	zstage->uiPanelCB				= Zstage_UIPan_CB;
		// no functionality
	zstage->MoveZ					= NULL;
	zstage->StopZ					= NULL;
	zstage->StatusLED				= NULL;		// functionality assigned if Zstage_Load is called
	zstage->UpdatePositionDisplay	= NULL;		// functionality assigned if Zstage_Load is called
	
	
	//----------------------------------------------------------
	if (!mod)
		return (DAQLabModule_type*) zstage;
	else
		return NULL;
}


/// HIFN Discards a generic Zstage_type data but does not free the structure memory.
void discard_Zstage (DAQLabModule_type** mod)
{
	Zstage_type* 				zstage = (Zstage_type*) (*mod);
	
	Zstage_RefPosition_type**	refPosPtrPtr;
	
	if (!zstage) return;
	
	// discard Zstage_type specific data
	
	if (zstage->controlPanHndl)
		DiscardPanel(zstage->controlPanHndl);
	OKfree(zstage->zPos);
	
	for (int i = 1; i <= ListNumItems(zstage->zRefPos); i++) {
		refPosPtrPtr = ListGetPtrToItem(zstage->zRefPos, i);
		discard_Zstage_RefPosition_type(refPosPtrPtr);
	}
	ListDispose(zstage->zRefPos);
	
	OKfree(zstage->zULimPos);
	OKfree(zstage->zLLimPos);

	// discard DAQLabModule_type specific data
	discard_DAQLabModule(mod);
}

static Zstage_RefPosition_type*	init_Zstage_RefPosition_type (char refName[], double refVal)
{
	Zstage_RefPosition_type* ref = malloc (sizeof(Zstage_RefPosition_type));
	if (!ref) return NULL;
	
	ref->name 	= StrDup(refName);
	ref->val	= refVal;
	
	return ref;
}

static void	discard_Zstage_RefPosition_type	(Zstage_RefPosition_type** a)
{
	if (!*a) return;
	
	OKfree((*a)->name);
	OKfree(*a);
}

/// HIFN Loads ZStage specific resources. 
int Zstage_Load (DAQLabModule_type* mod, int workspacePanHndl) 
{
	Zstage_type* 	zstage 				= (Zstage_type*) mod;  
	char			stepsizeName[50];
	
	// load panel resources
	zstage->controlPanHndl = LoadPanel(workspacePanHndl, MOD_Zstage_UI_ZStage, ZStagePan);
	
	// connect module data and user interface callbackFn to all direct controls in the panel
	SetCtrlsInPanCBInfo(mod, ((Zstage_type*)mod)->uiCtrlsCB, zstage->controlPanHndl);
	
	// add panel callback function pointer and callback data
	SetPanelAttribute(zstage->controlPanHndl, ATTR_CALLBACK_FUNCTION_POINTER, Zstage_UIPan_CB);
	SetPanelAttribute(zstage->controlPanHndl, ATTR_CALLBACK_DATA, mod);
	
	// change panel title to module instance name
	SetPanelAttribute(zstage->controlPanHndl, ATTR_TITLE, mod->instanceName);
	
	// adjust position display precision
	SetCtrlAttribute(zstage->controlPanHndl, ZStagePan_ZAbsPos, ATTR_PRECISION, Zstage_POSITION_DISPLAY_PRECISION);
	SetCtrlAttribute(zstage->controlPanHndl, ZStagePan_ZRelPos, ATTR_PRECISION, Zstage_POSITION_DISPLAY_PRECISION);
	SetCtrlAttribute(zstage->controlPanHndl, ZStagePan_Cycle_RelStart, ATTR_PRECISION, Zstage_POSITION_DISPLAY_PRECISION);
	SetCtrlAttribute(zstage->controlPanHndl, ZStagePan_Cycle_RelEnd, ATTR_PRECISION, Zstage_POSITION_DISPLAY_PRECISION);
	
	// populate Z step sizes
	for (int i = 0; i < NumElem(ZStepSizes); i++) {
		Fmt(stepsizeName, "%s<%f[p*]", Zstage_POSITION_DISPLAY_PRECISION, ZStepSizes[i]);
		InsertListItem(zstage->controlPanHndl, ZStagePan_ZStepSize, -1, stepsizeName, ZStepSizes[i]);   
	}
	
	// add functionality to change LED status
	zstage->StatusLED = Zstage_ChangeLEDStatus;
	
	// add functionality to update stage position
	zstage->UpdatePositionDisplay = Zstage_UpdatePositionDisplay;
	
	// update stage absolute position if position has been determined
	if (zstage->zPos)
		SetCtrlVal(zstage->controlPanHndl, ZStagePan_ZAbsPos, *zstage->zPos * 1000);	  // convert from [mm] to [um]
	else {
		DLMsg("Stage position must be determined first. Aborting load operation.", 1);
		return -1;
	}
	
	return 0;

}

static int Zstage_DisplayPanels	(DAQLabModule_type* self, BOOL visibleFlag)
{
	int error = 0;
	Zstage_type* zstage	= (Zstage_type*) self;
		
	if (visibleFlag)
		errChk(	DisplayPanel(zstage->controlPanHndl) );
	else
		errChk( HidePanel(zstage->controlPanHndl) );
	
	return 0;
	
	Error:
	
	return error;
}

static int Zstage_ChangeLEDStatus (Zstage_type* zstage, Zstage_LED_type status)
{
	int error = 0;
	
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
	
	return error;
}

static int Zstage_UpdatePositionDisplay	(Zstage_type* zstage)
{
	int error = 0;
	
	// display current position
	errChk( SetCtrlVal(zstage->controlPanHndl, ZStagePan_ZAbsPos, *zstage->zPos * 1000) );  // convert from [mm] to [um] 
	// reset relative position
	errChk( SetCtrlVal(zstage->controlPanHndl, ZStagePan_ZRelPos, 0.0) );
	return 0;
	
	Error:
	return error;
}

//==============================================================================
// Global functions for module specific UI management


static int CVICALLBACK Zstage_UICtrls_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Zstage_type* 				zstage 			= callbackData;
	int							refPosIdx;						// 0-based index of ref pos selected in the list
	Zstage_RefPosition_type**	refPosPtrPtr;
	double						stepsize;	  					// in [mm]
	double 						direction;
	double 						moveAbsPos;   					// in [mm]
	double						moveRelPos;   					// in [mm]
	char						refPosDisplayItem[Zstage_MAX_REF_POS_LENGTH + 50];
	
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
					if (zstage->MoveZ)
						(*zstage->MoveZ)	(zstage, ZSTAGE_MOVE_REL, direction*stepsize);
					break;
					
				case ZStagePan_MoveZDown:
					
					// move down relative to current position with selected step size
					if (zstage->MoveZ)
						(*zstage->MoveZ)	(zstage, ZSTAGE_MOVE_REL, -direction*stepsize);
					break;
					
				case ZStagePan_Stop:
					
					// stop stage motion
					if (zstage->StopZ)
						(*zstage->StopZ)	(zstage);
					break;
				
				case ZStagePan_ZAbsPos:
					
					// move to absolute position with selected step size
					if (zstage->MoveZ)
						(*zstage->MoveZ)	(zstage, ZSTAGE_MOVE_ABS, moveAbsPos);
					
					break;
					
				case ZStagePan_ZRelPos:
					
					// move relative to current position
					if (zstage->MoveZ)
						(*zstage->MoveZ)	(zstage, ZSTAGE_MOVE_REL, moveRelPos);
					
					break;
					
				case ZStagePan_Cycle_RelStart:
				case ZStagePan_Cycle_RelEnd:
					
					break;
					
				case ZStagePan_SetStartPos:
					
					break;
					
				case ZStagePan_SetEndPos:
					
					break;
					
				case ZStagePan_AddRefPos:
					
					char* 						newRefPosName		= NULL;
					Zstage_RefPosition_type*	newRefPos			= NULL;
					
					newRefPosName = DLGetUINameInput("New Reference Position", Zstage_MAX_REF_POS_LENGTH, Zstage_ValidateRefPosName, &zstage->zRefPos); 
					if (!newRefPosName) return 0;	// operation cancelled or an error occured.
					
					// add new reference position to structure data
					newRefPos = init_Zstage_RefPosition_type(newRefPosName, moveAbsPos);
					if (!newRefPos) goto AddRefPosError;
					
					if (!ListInsertItem(zstage->zRefPos, &newRefPos, END_OF_LIST)) goto AddRefPosError;
					
					// undim ref pos listbox
					SetCtrlAttribute(panel, ZStagePan_RefPosList, ATTR_DIMMED, 0);
					
					// add reference position to listbox
					Fmt(refPosDisplayItem, "%s @ %f[p*]", newRefPosName, Zstage_POSITION_DISPLAY_PRECISION, moveAbsPos*1000);  		// display in [um]
					OKfree(newRefPosName);
					InsertListItem(panel, ZStagePan_RefPosList, -1, refPosDisplayItem, moveAbsPos); // store listbox position value in [mm] 
					
					break;
					AddRefPosError:
					OKfree(newRefPosName);
					discard_Zstage_RefPosition_type(&newRefPos);
					DLMsg("Error. Could not add new reference position. Out of memory?\n\n", 1);
					return 0;
					
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
						discard_Zstage_RefPosition_type(refPosPtrPtr);
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
					if (zstage->MoveZ)
						(*zstage->MoveZ)	(zstage, ZSTAGE_MOVE_ABS, moveAbsPos);
					
					break;
			}
			break;
			
		case EVENT_RIGHT_DOUBLE_CLICK: // update reference position value
			
			switch (control) {
					
				case ZStagePan_RefPosList:
					
					// get ref pos idx
					GetCtrlIndex(panel, control, &refPosIdx);
					// update reference position in structure data
					refPosPtrPtr = ListGetPtrToItem(zstage->zRefPos, refPosIdx + 1);
					(*refPosPtrPtr)->val = *zstage->zPos;
					// update reference position label
					// display in [um]  
					Fmt(refPosDisplayItem, "%s @ %f[p*]", (*refPosPtrPtr)->name, Zstage_POSITION_DISPLAY_PRECISION, *zstage->zPos * 1000); 
					// store listbox position value in [mm] 
					ReplaceListItem(panel, ZStagePan_RefPosList, refPosIdx, refPosDisplayItem, (*refPosPtrPtr)->val); 			
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
							break;
					
						case MOUSE_WHEEL_SCROLL_DOWN:
					
							SetCtrlVal(panel, control, (*zstage->zPos - direction * stepsize) * 1000);	// convert back to [um] 
							break;
					
					}
					break;
					
				case ZStagePan_ZRelPos:				// adjust relative position
					
					switch (eventData1) {
					
						case MOUSE_WHEEL_SCROLL_UP:
							
							SetCtrlVal(panel, control, direction * stepsize * 1000);					// convert back to [um]
							break;
					
						case MOUSE_WHEEL_SCROLL_DOWN:
					
							SetCtrlVal(panel, control, -direction * stepsize * 1000);					// convert back to [um]
							break;
					
					}
					break;
					
				break;
			}
	}
	
	return 0;
}

static int CVICALLBACK Zstage_UIPan_CB (int panel, int event, void *callbackData, int eventData1, int eventData2)
{
	Zstage_type* 				zstage 			= callbackData;
	
	return 0;
}

static BOOL Zstage_ValidateRefPosName (char inputStr[], void* dataPtr)
{
	ListType 					RefPosList	= *(ListType*)dataPtr;
	Zstage_RefPosition_type**	refPosPtrPtr;
	
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

   /*
int Zstage_LoadCfg (DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  DAQLabCfg_RootElement) 
{
	Zstage_type*  	zstage = (Zstage_type*) mod; 
	
	return DAQLabModule_LoadCfg(mod, DAQLabCfg_RootElement);
}
		  */
