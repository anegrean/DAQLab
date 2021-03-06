//==============================================================================
//
// Title:		Pockells.c
// Purpose:		Provides Pockells cell electro-optic modulator control.
//
// Created on:	5-3-2015 at 12:36:38 by Adrian Negrean.
// Copyright:	VU University Amsterdam. All Rights Reserved.
// License:     This Source Code Form is subject to the terms of the Mozilla Public 
//              License v. 2.0. If a copy of the MPL was not distributed with this 
//              file, you can obtain one at https://mozilla.org/MPL/2.0/ . 
//
//==============================================================================

//==============================================================================
// Include files

#include "DAQLab.h" 		// include this first
#include "DAQLabErrHandling.h"
#include <analysis.h> 
#include <formatio.h>
#include "UI_Pockells.h"
#include "Pockells.h"

//==============================================================================
// Constants
#define DAQmxErrChk(functionCall) if( DAQmxFailed(error=(functionCall)) ) goto Error; else

#define MOD_PockellsModule_UI 				"./Modules/Pockells Cell/UI_Pockells.uir"

// Task Controller base name
#define TaskController_BaseName				"Pockells cell"

// Pockells calibration table column indices
#define CalibTableColIdx_Wavelength			1
#define CalibTableColIdx_MaxPower			2
#define CalibTableColIdx_CurrentPower		3
#define CalibTableColIdx_aCoefficient		4
#define CalibTableColIdx_bCoefficient		5
#define CalibTableColIdx_cCoefficient		6
#define CalibTableColIdx_dCoefficient		7

//----------------------------------------
// VChans
//----------------------------------------
#define PockellsEOM_VChan_Timing			"timing"				// Sink VChan of Waveform_UChar or RepeatedWaveform_UChar used to time the light pulses
#define PockellsEOM_VChan_Command			"command"				// Source VChan of RepeatedWaveform_Double used to modulate the Pockells cell.
#define PockellsEOM_VChan_Timing_DataTypes  {	DL_Waveform_Char, 				\
												DL_RepeatedWaveform_Char,		\
												DL_Waveform_UChar, 				\
												DL_RepeatedWaveform_UChar,		\
												DL_Waveform_Short, 				\
												DL_RepeatedWaveform_Short,		\
												DL_Waveform_UShort, 			\
												DL_RepeatedWaveform_UShort,		\
												DL_Waveform_Int, 				\
												DL_RepeatedWaveform_Int,		\
												DL_Waveform_UInt, 				\
												DL_RepeatedWaveform_UInt,		\
												DL_Waveform_Int64, 				\
												DL_RepeatedWaveform_Int64,		\
												DL_Waveform_UInt64, 			\
												DL_RepeatedWaveform_UInt64,		\
												DL_Waveform_SSize, 				\
												DL_RepeatedWaveform_SSize,		\
												DL_Waveform_Size, 				\
												DL_RepeatedWaveform_Size	}   \

#define VChanDataTimeout					1e4						// Timeout in [ms] for Sink VChans to receive data.

//==============================================================================
// Types

	// forward declared
typedef struct PockellsEOMCal		PockellsEOMCal_type;
typedef struct PockellsEOM			PockellsEOM_type;  
typedef struct PockellsModule		PockellsModule_type;


struct PockellsEOMCal {
	double 						wavelength;					// Wavelength in [nm] at which the calibration of minTransmV and maxTransmV was done.
	double						maxPower;					// Maximum optical power in [mW] that is available from the pockells cell. This value may be adjusted whenever needed, and can be measured e.g. 
															// at the sample. Default value is 1 mW.
	double						a;							// Calibration coefficients according to the formula I(Vbias) = a * sin(b * Vbias + c)^2 + d that 
	double						b;							// is a fit to the experimentally obtained normalized transmitted power vs. bias voltage. The units are:
	double						c;							// a [ ], b [1/V], c [ ], d [ ] 
	double						d;							
};

struct PockellsEOM {
	
	// DATA
	double						outputPower;				// Pockells cell normalized optical power output when not in pulsed mode.
	double						pulsedOutputPower;			// Pockells cell normalized optical power output for pulsed mode.
	double						maxSafeVoltage;				// Maximum safe voltage that can be applied to the pockells cell in [V].
	BOOL						isPulsed;					// If True, the Pockells cell is switched to a pulsed operation mode with appropriate pulse timing provided by the timingVChan.
	int							calibIdx;					// 1-based calibration index set for the pockells cell from the calib list.
	ListType 					calib;          			// List of pockells EOM calibration wavelengths of PockellsEOMCal_type. 
	SourceVChan_type*			modulationVChan;			// Output waveform VChan used to modulate the pockells cell. VChan of RepeatedWaveform_Double.
	SinkVChan_type*				timingVChan;				// Input waveform VChan used to time the modulation of the pockells cell. VChan of Waveform_UChar or RepeatedWaveform_UChar. 
	TaskControl_type*			taskControl;				// Pockells cell task controller.
	BOOL						DLRegistered;				// If True, pockells cell registered its task controller and VChans with the framework, False otherwise.
	
	// UI
	int							eomPanHndl;					// Panel handle with pockells cell controls.
	int							calibPanHndl;				// Panel handle for pockells cell calibration data.
};

struct PockellsModule {
	
	// SUPER, must be the first member to inherit from
	DAQLabModule_type 			baseClass;
	
	//-------------------------
	// DATA
	//-------------------------
	
	ListType					pockellsCells;				// List of available pockells cells of PockellsEOM_type*
	
		//-------------------------
		// UI
		//-------------------------
		
	int							mainPanHndl;	  			// Main panel.
	int*						mainPanLeftPos;				// Main panel left position to be applied when module is loaded.
	int*						mainPanTopPos;				// Main panel top position to be applied when module is loaded.
	int							menuBarHndl;				// Main panel menu bar handle.
	int							calMenuItemHndl;			// Calibration menu item handle for calibrating pockells cells.
	
	//-------------------------
	// METHODS
	//-------------------------
	
	
};

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

static PockellsEOM_type* 			init_PockellsEOM_type								(PockellsModule_type* eomModule, TaskControl_type** taskControllerPtr);

static void							discard_PockellsEOM_type							(PockellsEOM_type** eomPtr);

//------------------
// Module management
//------------------

static int							Load 												(DAQLabModule_type* mod, int workspacePanHndl, char** errorMsg);

static int 							LoadCfg 											(DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_ moduleElement, ERRORINFO* xmlErrorInfo);

static PockellsEOM_type* 			LoadPockellsCellFromXMLData 						(PockellsModule_type* eomModule, ActiveXMLObj_IXMLDOMElement_ pockellsXMLElement, ERRORINFO* xmlErrorInfo);

static int 							LoadPockellsCalibrationFromXMLData 					(PockellsEOMCal_type* eomCal, ActiveXMLObj_IXMLDOMElement_ calXMLElement, ERRORINFO* xmlErrorInfo);

static int 							SaveCfg 											(DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ moduleElement, ERRORINFO* xmlErrorInfo);

static int 							SavePockellsCellXMLData								(PockellsEOM_type* eom, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ pockellsXMLElement, ERRORINFO* xmlErrorInfo);

static int 							SavePockellsCalibrationXMLData						(PockellsEOMCal_type* eomCal, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ calXMLElement, ERRORINFO* xmlErrorInfo); 

static int 							DisplayPanels										(DAQLabModule_type* mod, BOOL visibleFlag); 

	// menu callback to adds a new Pockells cell
static void CVICALLBACK 			NewPockellsCell_CB 									(int menuBar, int menuItem, void *callbackData, int panel);
	// registers a Pockells cell with the framework
static int							RegisterPockellsCell								(PockellsModule_type* eomModule, PockellsEOM_type* eom, char** errorMsg);
	// unregisters a Pockells cell from the framework
static int							UnregisterPockellsCell								(PockellsModule_type* eomModule, PockellsEOM_type* eom);
	// initializes new pockells cell UI 
static int							InitNewPockellsCellUI								(PockellsModule_type* eomModule, PockellsEOM_type* eom, int eomNewPanHndl, char** errorMsg);
	// opens the calibration window for a given pockells cell
static void CVICALLBACK 			PockellsCellCalibration_CB 							(int menuBar, int menuItem, void *callbackData, int panel);
	// adds a calibration table entry
static void 						AddCalibTableEntry 									(PockellsEOM_type* eom, PockellsEOMCal_type* eomCal); 

	// dims cells in a calibration table row when calibration has been added
//static void							DimCalibTableCells									(PockellsEOM_type* eom, int row);
	// adds a table calibration entry to the calibration list
static void 						AddTableEntryToCalibList							(PockellsEOM_type* eom, int tableRow); 
	// checks if a calibration table entry is valid
static BOOL 						CalibTableRowIsValid 								(PockellsEOM_type* eom, int tableRow);
	// pockells module main panel callback
static int CVICALLBACK 				MainPan_CB 											(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
	// pockells module calibration panel callback
static int CVICALLBACK 				CalibPan_CB											(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
//--------------------------
// Pockells cell operation
//--------------------------

static int CVICALLBACK 				PockellsControl_CB									(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
	// applies a command voltage to the Pockells cell
static int 							ApplyPockellsCellVoltage 							(PockellsEOM_type* eom, double voltage, char** errorMsg);
	// calculates the command voltage to apply to the pockells cell given a certain calibration to generate the desired output power.
static double						GetPockellsCellVoltage								(PockellsEOMCal_type* eomCal, double normalizedPower);
	


//-----------------------------------------
// VChan callbacks
//-----------------------------------------

	// Timing VChan
static void							TimingVChan_StateChange 							(VChan_type* self, void* VChanOwner, VChanStates state);

	// Modulation VChan
static void							ModulationVChan_StateChange							(VChan_type* self, void* VChanOwner, VChanStates state);

//-----------------------------------------
// Task Controller callbacks
//-----------------------------------------

static int							ConfigureTC											(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);

static int							UnconfigureTC										(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);

static void							IterateTC											(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortIterationFlag);

static int							StartTC												(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);

static int				 			ResetTC 											(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);

static int							DoneTC												(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg);

static int							StoppedTC											(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg);

static int							TaskTreeStateChange 								(TaskControl_type* taskControl, TaskTreeStates state, char** errorMsg);

static int				 			DataReceivedTC										(TaskControl_type* taskControl, TCStates taskState, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorMsg);

static void 						ErrorTC 											(TaskControl_type* taskControl, int errorID, char* errorMsg);

static int							ModuleEventHandler									(TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, void* eventData, BOOL const* abortFlag, char** errorMsg);

//==============================================================================
// Global variables

//==============================================================================
// Global functions

DAQLabModule_type* initalloc_PockellsModule (DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl)
{
INIT_ERR

	PockellsModule_type*	eomModule	= NULL;
	
	if (!mod)
		nullChk( eomModule = malloc (sizeof(PockellsModule_type)) );
	else
		eomModule = (PockellsModule_type*) mod;
		
	// initialize base class
	initalloc_DAQLabModule(&eomModule->baseClass, className, instanceName, workspacePanHndl);
	//------------------------------------------------------------
	
	//---------------------------
	// Parent Level 0: DAQLabModule_type 
	
		// DATA
		
		// METHODS
		
			// overriding methods
	eomModule->baseClass.Discard 			= discard_PockellsModule;
	eomModule->baseClass.Load				= Load; 
	eomModule->baseClass.DisplayPanels		= DisplayPanels;
	eomModule->baseClass.LoadCfg			= LoadCfg;
	eomModule->baseClass.SaveCfg			= SaveCfg;
			
	//---------------------------
	// Child Level 1: PockellsModule_type
	
		// DATA
		
	// init
	eomModule->pockellsCells			= 0;
	eomModule->mainPanHndl				= 0;
	eomModule->mainPanLeftPos			= NULL;
	eomModule->mainPanTopPos			= NULL;
	eomModule->menuBarHndl				= 0;
	eomModule->calMenuItemHndl			= 0;
		
	// alloc
		
	nullChk( eomModule->pockellsCells	= ListCreate(sizeof(PockellsEOM_type*)) ); 
	
	
		// METHODS
		
	//----------------------------------------------------------
	if (!mod)
		return (DAQLabModule_type*) eomModule;
	else
		return NULL;
	
Error:
	
	discard_PockellsModule((DAQLabModule_type**)&eomModule); 
	return NULL;
}

void discard_PockellsModule (DAQLabModule_type** mod)
{
	PockellsModule_type*	eomModule = (PockellsModule_type*) *mod; 
	
	if (!eomModule) return;
	
	//-------------------------------------------
	// discard PockellsModule_type specific data
	//-------------------------------------------
	
	OKfreePanHndl(eomModule->mainPanHndl);
	
	if (eomModule->menuBarHndl)  { 
		DiscardMenuBar (eomModule->menuBarHndl); 
		eomModule->menuBarHndl = 0; 
	} 
	
	OKfree(eomModule->mainPanTopPos);
	OKfree(eomModule->mainPanLeftPos);
	
	// available pockells cells
	size_t				nCells 	= ListNumItems(eomModule->pockellsCells);
	PockellsEOM_type**	cellPtr	= NULL;
	for (size_t i = 1; i <= nCells; i++) {
		cellPtr = ListGetPtrToItem(eomModule->pockellsCells, i);
		UnregisterPockellsCell(eomModule, *cellPtr);
		discard_PockellsEOM_type(cellPtr);
	}
	ListDispose(eomModule->pockellsCells);
	eomModule->pockellsCells = 0;
	
	
	//----------------------------------------
	// discard DAQLabModule_type specific data
	//----------------------------------------
	
	discard_DAQLabModule(mod);
}

static PockellsEOM_type* init_PockellsEOM_type (PockellsModule_type* eomModule, TaskControl_type** taskControllerPtr)
{
INIT_ERR

	PockellsEOM_type*		eom 					= malloc (sizeof(PockellsEOM_type));
	char*					timingVChanName			= NULL;
	char*					modulationVChanName		= NULL;
	
	if (!eom) return NULL;
	
	// init
		// DATA
	eom->outputPower				= 0;
	eom->pulsedOutputPower			= 0;
	eom->isPulsed					= FALSE;
	eom->maxSafeVoltage				= 0;
	eom->calibIdx					= 0;
	eom->calib						= 0;
	eom->taskControl				= *taskControllerPtr;
	*taskControllerPtr				= NULL;
	SetTaskControlModuleData(eom->taskControl, eom);
	eom->timingVChan				= NULL;
	eom->modulationVChan			= NULL;
	eom->DLRegistered				= FALSE;
		// UI
	eom->eomPanHndl					= 0;
	eom->calibPanHndl				= 0;
	
	// alloc
	
		// VChans
	nullChk( timingVChanName  		= DLVChanName((DAQLabModule_type*)eomModule, eom->taskControl, PockellsEOM_VChan_Timing, 0) );
	nullChk( modulationVChanName	= DLVChanName((DAQLabModule_type*)eomModule, eom->taskControl, PockellsEOM_VChan_Command, 0) );
		
		// DATA
	nullChk( eom->calib				= ListCreate(sizeof(PockellsEOMCal_type)) );
	
	DLDataTypes		timingVChanDataTypes[] = PockellsEOM_VChan_Timing_DataTypes;
	
	nullChk( eom->timingVChan  		= init_SinkVChan_type(timingVChanName, timingVChanDataTypes, NumElem(timingVChanDataTypes), eom, VChanDataTimeout, TimingVChan_StateChange) );
	nullChk( eom->modulationVChan	= init_SourceVChan_type(modulationVChanName, DL_RepeatedWaveform_Double, eom, ModulationVChan_StateChange) );
	
	// cleanup
	OKfree(timingVChanName);
	OKfree(modulationVChanName);
	
	return eom;
	
Error:
	
	// cleanup
	OKfree(timingVChanName);
	OKfree(modulationVChanName);
	
	discard_PockellsEOM_type(&eom);
	
	return NULL;
}

static void discard_PockellsEOM_type (PockellsEOM_type** eomPtr)
{
	PockellsEOM_type* 		eom 		= *eomPtr;
	if (!eom) return;
	
	if (eom->calib) {
		ListDispose(eom->calib);
		eom->calib = 0;
	}
	
	if (eom->calibPanHndl) {
		DiscardPanel(eom->calibPanHndl);
		eom->calibPanHndl = 0;
		
	}
	
	discard_TaskControl_type(&eom->taskControl);
	discard_VChan_type((VChan_type**)&eom->timingVChan);
	discard_VChan_type((VChan_type**)&eom->modulationVChan);
	
		
	OKfree(*eomPtr);
}

//------------------
// Module management
//------------------

static int Load (DAQLabModule_type* mod, int workspacePanHndl, char** errorMsg)
{
INIT_ERR
	
	PockellsModule_type*	eomModule 			= (PockellsModule_type*) mod;
	int						newMenuItem			= 0; 
	int						eomPanHndl  		= 0;
	

	// load main panel
	errChk(eomModule->mainPanHndl 		= LoadPanel(workspacePanHndl, MOD_PockellsModule_UI, MainPan));
	// load pockells cell panel
	errChk(eomPanHndl			 		= LoadPanel(workspacePanHndl, MOD_PockellsModule_UI, Pockells));
	// set main panel position
	if (eomModule->mainPanLeftPos)
		SetPanelAttribute(eomModule->mainPanHndl, ATTR_LEFT, *eomModule->mainPanLeftPos);
	else
		SetPanelAttribute(eomModule->mainPanHndl, ATTR_LEFT, VAL_AUTO_CENTER); 
		
	if (eomModule->mainPanTopPos)
		SetPanelAttribute(eomModule->mainPanHndl, ATTR_TOP, *eomModule->mainPanTopPos);
	else
		SetPanelAttribute(eomModule->mainPanHndl, ATTR_TOP, VAL_AUTO_CENTER);
	
	// add menu bar and link it to module data
	eomModule->menuBarHndl 		= NewMenuBar(eomModule->mainPanHndl);
	SetMenuBarAttribute(eomModule->menuBarHndl, 0, ATTR_SHOW_IMMEDIATE_ACTION_SYMBOL, 0);
	
	// new menu item
	newMenuItem					= NewMenu(eomModule->menuBarHndl, "New", -1);
	SetMenuBarAttribute(eomModule->menuBarHndl, newMenuItem, ATTR_CALLBACK_DATA, eomModule);
	SetMenuBarAttribute(eomModule->menuBarHndl, newMenuItem, ATTR_CALLBACK_FUNCTION_POINTER, NewPockellsCell_CB);
	
	// calibration menu
	eomModule->calMenuItemHndl 	= NewMenu(eomModule->menuBarHndl, "Calibration", -1);
	SetMenuBarAttribute(eomModule->menuBarHndl, eomModule->calMenuItemHndl, ATTR_CALLBACK_DATA, eomModule);
	SetMenuBarAttribute(eomModule->menuBarHndl, eomModule->calMenuItemHndl, ATTR_CALLBACK_FUNCTION_POINTER, PockellsCellCalibration_CB);
	 
	
	// add callback function and data to pockell eom module main panel controls
	SetCtrlsInPanCBInfo(eomModule, MainPan_CB, eomModule->mainPanHndl);  				 
	
	// change main module panel title to module instance name
	SetPanelAttribute(eomModule->mainPanHndl, ATTR_TITLE, mod->instanceName);
	
	//----------------------------------------------------------------------
	// load pockells cells
	//----------------------------------------------------------------------
	size_t					nCells 		= ListNumItems(eomModule->pockellsCells);
	PockellsEOM_type*   	eom			= NULL;
	
	for (size_t i = 1; i <= nCells; i++) {
		eom = *(PockellsEOM_type**) ListGetPtrToItem(eomModule->pockellsCells, i);
		errChk( InitNewPockellsCellUI(eomModule, eom, eomPanHndl, &errorInfo.errMsg) );
		errChk( RegisterPockellsCell(eomModule, eom, &errorInfo.errMsg) );
	}
	
	// delete "None" tab if present and undim "Calibration" menu item
	if (nCells) {
		DeleteTabPage(eomModule->mainPanHndl, MainPan_Tab, 0, 1);
		SetMenuBarAttribute(eomModule->menuBarHndl, eomModule->calMenuItemHndl, ATTR_DIMMED, 0);
	} else
		SetMenuBarAttribute(eomModule->menuBarHndl, eomModule->calMenuItemHndl, ATTR_DIMMED, 1);
		
	
	// display main panel
	DisplayPanel(eomModule->mainPanHndl);
	
	// cleanup
	DiscardPanel(eomPanHndl);
	
	return 0;
	
Error:
	
	// cleanup
	OKfreePanHndl(eomPanHndl);
	
RETURN_ERR
}

static int LoadCfg (DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_ moduleElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	PockellsModule_type*			eomModule						= (PockellsModule_type*)mod;    
	eomModule->mainPanTopPos										= malloc(sizeof(int));
	eomModule->mainPanLeftPos										= malloc(sizeof(int));
	DAQLabXMLNode 					eomModuleAttr[]					= { {"PanTopPos", BasicData_Int, eomModule->mainPanTopPos},
											  		   					{"PanLeftPos", BasicData_Int, eomModule->mainPanLeftPos}};
	
	//-------------------------------------------------------------------------- 
	// Load main panel position 
	//-------------------------------------------------------------------------- 
	
	errChk( DLGetXMLElementAttributes("", moduleElement, eomModuleAttr, NumElem(eomModuleAttr)) ); 
	
	//-------------------------------------------------------------------------- 
	// Load pockells cells
	//--------------------------------------------------------------------------
	
	ActiveXMLObj_IXMLDOMNodeList_	pockellsNodeList				= 0;
	ActiveXMLObj_IXMLDOMNode_		pockellsNode					= 0;
	long							nCells							= 0;
	PockellsEOM_type*				eom								= NULL;
	
	errChk ( ActiveXML_IXMLDOMElement_getElementsByTagName(moduleElement, xmlErrorInfo, "PockellsCell", &pockellsNodeList) );
	errChk ( ActiveXML_IXMLDOMNodeList_Getlength(pockellsNodeList, xmlErrorInfo, &nCells) );
	
	for (long i = 0; i < nCells; i++) {
		errChk ( ActiveXML_IXMLDOMNodeList_Getitem(pockellsNodeList, xmlErrorInfo, i, &pockellsNode) );
		nullChk( eom = LoadPockellsCellFromXMLData(eomModule, (ActiveXMLObj_IXMLDOMElement_) pockellsNode, xmlErrorInfo) ); 
		ListInsertItem(eomModule->pockellsCells, &eom, END_OF_LIST);
		OKfreeCAHndl(pockellsNode); 
	}
	
	OKfreeCAHndl(pockellsNodeList);
	
	return 0;
	
Error:
	
	return errorInfo.error;
}

static PockellsEOM_type* LoadPockellsCellFromXMLData (PockellsModule_type* eomModule, ActiveXMLObj_IXMLDOMElement_ pockellsXMLElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	PockellsEOM_type*				eom								= NULL;
	double							maxSafeVoltage					= 0;
	int								calibIdx						= 0;
	char*							taskControllerName				= NULL;
	TaskControl_type*				taskController					= NULL;
	
	DAQLabXMLNode 					eomAttr[] 						= {	{"MaxSafeVoltage", BasicData_Double, &maxSafeVoltage},
											  		   		   			{"CalibrationIdx", BasicData_Int, &calibIdx},
															   			{"TaskControllerName", BasicData_CString, &taskControllerName} };
	
	//-------------------------------------------------------------------------- 
	// Load pockells cell data
	//--------------------------------------------------------------------------
	
	errChk( DLGetXMLElementAttributes("", pockellsXMLElement, eomAttr, NumElem(eomAttr)) );
	
	nullChk( taskController = init_TaskControl_type(taskControllerName, NULL, DLGetCommonThreadPoolHndl(), ConfigureTC, UnconfigureTC, IterateTC, 
									  StartTC, ResetTC, DoneTC, StoppedTC, NULL, TaskTreeStateChange, NULL, ModuleEventHandler, ErrorTC) );
	
	OKfree(taskControllerName);
	
	nullChk( eom = init_PockellsEOM_type(eomModule, &taskController) );
	
	// configure task controller
	SetTaskControlIterations(eom->taskControl, 1);
	errChk( TaskControlEvent(eom->taskControl, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
	
	eom->calibIdx 			= calibIdx;
	eom->maxSafeVoltage 	= maxSafeVoltage;
	
	//-------------------------------------------------------------------------- 
	// Load pockells cell calibrations
	//--------------------------------------------------------------------------
	ActiveXMLObj_IXMLDOMNodeList_	calNodeList				= 0;
	ActiveXMLObj_IXMLDOMNode_		calNode					= 0;
	long							nCals					= 0;
	PockellsEOMCal_type				eomCal;
	
	errChk ( ActiveXML_IXMLDOMElement_getElementsByTagName(pockellsXMLElement, xmlErrorInfo, "Calibration", &calNodeList) );
	errChk ( ActiveXML_IXMLDOMNodeList_Getlength(calNodeList, xmlErrorInfo, &nCals) );
	
	for (long i = 0; i < nCals; i++) {
		errChk ( ActiveXML_IXMLDOMNodeList_Getitem(calNodeList, xmlErrorInfo, i, &calNode) );
		errChk( LoadPockellsCalibrationFromXMLData(&eomCal, (ActiveXMLObj_IXMLDOMElement_) calNode, xmlErrorInfo) ); 
		ListInsertItem(eom->calib, &eomCal, END_OF_LIST);
		OKfreeCAHndl(calNode); 
	}
	
	OKfreeCAHndl(calNodeList);

	return eom;
	
Error:

	return NULL;
}

static int LoadPockellsCalibrationFromXMLData (PockellsEOMCal_type* eomCal, ActiveXMLObj_IXMLDOMElement_ calXMLElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	DAQLabXMLNode 					calAttr[] 						= {	{"Wavelength", BasicData_Double, &eomCal->wavelength},
																		{"MaxPower", BasicData_Double, &eomCal->maxPower},
											  		   		   			{"a", BasicData_Double, &eomCal->a},
															   			{"b", BasicData_Double, &eomCal->b},
															   			{"c", BasicData_Double, &eomCal->c},
															   			{"d", BasicData_Double, &eomCal->d}};
																		
	errChk( DLGetXMLElementAttributes("", calXMLElement, calAttr, NumElem(calAttr)) ); 
																		
Error:
	
	return errorInfo.error;
}

static int SaveCfg (DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ moduleElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	PockellsModule_type*			eomModule				= (PockellsModule_type*)mod;
	int								lsPanTopPos				= 0;
	int								lsPanLeftPos			= 0;
	DAQLabXMLNode 					moduleAttr[] 			= {{"PanTopPos", BasicData_Int, &lsPanTopPos},
											  		   		   {"PanLeftPos", BasicData_Int, &lsPanLeftPos}};
	
	//--------------------------------------------------------------------------
	// Save pockells module main panel position
	//--------------------------------------------------------------------------
	
	errChk( GetPanelAttribute(eomModule->mainPanHndl, ATTR_LEFT, &lsPanLeftPos) );
	errChk( GetPanelAttribute(eomModule->mainPanHndl, ATTR_TOP, &lsPanTopPos) );
	errChk( DLAddToXMLElem(xmlDOM, moduleElement, moduleAttr, DL_ATTRIBUTE, NumElem(moduleAttr), xmlErrorInfo) ); 
	
	//--------------------------------------------------------------------------
	// Save pockells cells
	//--------------------------------------------------------------------------
	size_t							nCells					= ListNumItems(eomModule->pockellsCells);
	PockellsEOM_type*				eom						= NULL;
	ActiveXMLObj_IXMLDOMElement_	pockellsXMLElement		= 0;
	
	for (size_t i = 1; i <= nCells; i++) {
		eom = *(PockellsEOM_type**) ListGetPtrToItem(eomModule->pockellsCells, i);
		// create new pockells cell element
		errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "PockellsCell", &pockellsXMLElement) );
		// add pockells cell data to the element
		errChk( SavePockellsCellXMLData(eom, xmlDOM, pockellsXMLElement, xmlErrorInfo) );
		// add pockells cell element to the module
		errChk ( ActiveXML_IXMLDOMElement_appendChild (moduleElement, xmlErrorInfo, pockellsXMLElement, NULL) );
		OKfreeCAHndl(pockellsXMLElement); 
	}
	
Error:
	
	return errorInfo.error;
}

static int SavePockellsCellXMLData (PockellsEOM_type* eom, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ pockellsXMLElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	char*							taskControllerName		= GetTaskControlName(eom->taskControl);
	ActiveXMLObj_IXMLDOMElement_	calXMLElement			= 0;   // pockells cell XML calibration element
	DAQLabXMLNode 					eomAttr[] 				= {{"MaxSafeVoltage", BasicData_Double, &eom->maxSafeVoltage},
											  		   		   {"CalibrationIdx", BasicData_Int, &eom->calibIdx},
															   {"TaskControllerName", BasicData_CString, taskControllerName}};
	
															   
	//--------------------------------------------------------------------------
	// Save pockells cell attributes
	//--------------------------------------------------------------------------
	
	errChk( DLAddToXMLElem(xmlDOM, pockellsXMLElement, eomAttr, DL_ATTRIBUTE, NumElem(eomAttr), xmlErrorInfo) );
	OKfree(taskControllerName);
	
	//--------------------------------------------------------------------------
	// Save pockells cell calibrations
	//--------------------------------------------------------------------------
	size_t							nCals				= ListNumItems(eom->calib);
	PockellsEOMCal_type*			eomCal				= NULL;
	for (size_t i = 1; i <= nCals; i++) {
		eomCal = ListGetPtrToItem(eom->calib, i);
		// create new calibration element
		errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "Calibration", &calXMLElement) );
		// add calibration data to the element
		errChk( SavePockellsCalibrationXMLData(eomCal, xmlDOM, calXMLElement, xmlErrorInfo) );
		// add calibration element to the pockells cell element
		errChk ( ActiveXML_IXMLDOMElement_appendChild (pockellsXMLElement, xmlErrorInfo, calXMLElement, NULL) );
		OKfreeCAHndl(calXMLElement); 
	}
	
Error:
	
	return errorInfo.error;
}

static int SavePockellsCalibrationXMLData (PockellsEOMCal_type* eomCal, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ calXMLElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	DAQLabXMLNode 					calAttr[] 				= {{"Wavelength", BasicData_Double, &eomCal->wavelength},
															   {"MaxPower", BasicData_Double, &eomCal->maxPower},
											  		   		   {"a", BasicData_Double, &eomCal->a},
															   {"b", BasicData_Double, &eomCal->b},
															   {"c", BasicData_Double, &eomCal->c},
															   {"d", BasicData_Double, &eomCal->d}};
			
	//--------------------------------------------------------------------------
	// Save calibration attributes
	//--------------------------------------------------------------------------
	
	errChk( DLAddToXMLElem(xmlDOM, calXMLElement, calAttr, DL_ATTRIBUTE, NumElem(calAttr), xmlErrorInfo) );
	
	
Error:
	
	return errorInfo.error;
}

static int DisplayPanels (DAQLabModule_type* mod, BOOL visibleFlag)
{
	return 0;
}

static void CVICALLBACK NewPockellsCell_CB (int menuBar, int menuItem, void *callbackData, int panel)
{
INIT_ERR

	PockellsModule_type*	eomModule				= callbackData;
	char*					taskControllerName  	= NULL;
	char*					timingVChanName  		= NULL;
	char*					modulationVChanName		= NULL;
	PockellsEOM_type*		eom						= NULL;
	int						eomPanHndl				= 0;
	int						workspacePanHndl		= 0;
	void*					panelData				= NULL;
	int						panelHndl				= 0;
	TaskControl_type*		taskController			= NULL;
	
	
	// get new task controller name
	nullChk( taskControllerName = DLGetUniqueTaskControllerName(TaskController_BaseName) );
	
	
	nullChk(taskController = init_TaskControl_type(taskControllerName, eom, DLGetCommonThreadPoolHndl(), ConfigureTC, UnconfigureTC, IterateTC, 
									  StartTC, ResetTC, DoneTC, StoppedTC, NULL, TaskTreeStateChange, NULL, ModuleEventHandler, ErrorTC) );
	// configure task controller
	errChk( TaskControlEvent(taskController, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
	
	nullChk( eom = init_PockellsEOM_type(eomModule, &taskController) );
	
	GetPanelAttribute(panel, ATTR_PANEL_PARENT, &workspacePanHndl);
	// load Pockells cell panel
	errChk(eomPanHndl = LoadPanel(workspacePanHndl, MOD_PockellsModule_UI, Pockells));
	
	ListInsertItem(eomModule->pockellsCells, &eom, END_OF_LIST);
	
	errChk( InitNewPockellsCellUI(eomModule, eom, eomPanHndl, &errorInfo.errMsg) );
	errChk( RegisterPockellsCell(eomModule, eom, &errorInfo.errMsg) );
	
	// delete "None" tab if present
	GetPanelHandleFromTabPage(eomModule->mainPanHndl, MainPan_Tab, 0, &panelHndl);
	GetPanelAttribute(panelHndl, ATTR_CALLBACK_DATA, &panelData);
	if (!panelData) DeleteTabPage(eomModule->mainPanHndl, MainPan_Tab, 0, 1);
	
	// undim "Calibration" menu item
	SetMenuBarAttribute(eomModule->menuBarHndl, eomModule->calMenuItemHndl, ATTR_DIMMED, 0); 
	
	// cleanup
	DiscardPanel(eomPanHndl);
	OKfree(taskControllerName);
	OKfree(timingVChanName);
	OKfree(modulationVChanName);
	
	return;
	
Error:
	
	// cleanup
	DiscardPanel(eomPanHndl);
	UnregisterPockellsCell(eomModule, eom);
	discard_PockellsEOM_type(&eom);
	OKfree(taskControllerName);
	OKfree(timingVChanName);
	OKfree(modulationVChanName);
	
PRINT_ERR
}

static int RegisterPockellsCell (PockellsModule_type* eomModule, PockellsEOM_type* eom,  char** errorMsg)
{
INIT_ERR

	if (eom->DLRegistered) return 0; // already registered
	
	// add task controller to DAQLab framework
	nullChk( DLAddTaskController((DAQLabModule_type*)eomModule, eom->taskControl) );
		
	// add VChans to DAQLab framework and register with task controller
	errChk( AddSinkVChan(eom->taskControl, eom->timingVChan, NULL, &errorInfo.errMsg) );
	nullChk( DLRegisterVChan((DAQLabModule_type*)eomModule, (VChan_type*)eom->modulationVChan) );
	nullChk( DLRegisterVChan((DAQLabModule_type*)eomModule, (VChan_type*)eom->timingVChan) );
	
	eom->DLRegistered = TRUE;
	
Error:
	
RETURN_ERR
}

static int UnregisterPockellsCell (PockellsModule_type* eomModule, PockellsEOM_type* eom)
{
INIT_ERR
	
	if (!eom) return 0;
	if (!eom->DLRegistered) return 0; // not registered yet
	
	// remove VChan from DAQLab framework and unregister from task controller
	nullChk( DLUnregisterVChan((DAQLabModule_type*)eomModule, (VChan_type*)eom->timingVChan) );
	errChk( RemoveSinkVChan(eom->taskControl, eom->timingVChan, NULL) );
	nullChk( DLUnregisterVChan((DAQLabModule_type*)eomModule, (VChan_type*)eom->modulationVChan) );
	// unregister task controller
	nullChk( DLRemoveTaskController((DAQLabModule_type*)eomModule, eom->taskControl) );
	
	eom->DLRegistered = FALSE;
	
Error:
	
	return errorInfo.error;
}

static int InitNewPockellsCellUI (PockellsModule_type* eomModule, PockellsEOM_type* eom, int eomNewPanHndl, char** errorMsg)
{
INIT_ERR

	int						nTabs			= 0;
	char*					eomName			= NULL;
	size_t					nCals			= ListNumItems(eom->calib); 
	PockellsEOMCal_type*	eomCal			= NULL; 
	char					name[50]		= ""; 
	
	// insert new pockells cell tab
	InsertPanelAsTabPage(eomModule->mainPanHndl, MainPan_Tab, -1, eomNewPanHndl);  
	GetNumTabPages(eomModule->mainPanHndl, MainPan_Tab, &nTabs);
	// get panel handle to the new pockells cell
	GetPanelHandleFromTabPage(eomModule->mainPanHndl, MainPan_Tab, nTabs - 1, &eom->eomPanHndl);
	// change pockells cell tab title
	eomName = GetTaskControlName(eom->taskControl); 
	SetTabPageAttribute(eomModule->mainPanHndl, MainPan_Tab, nTabs - 1, ATTR_LABEL_TEXT, eomName);
	OKfree(eomName);
	// add pockells cell panel and controls callback data
	SetPanelAttribute(eom->eomPanHndl, ATTR_CALLBACK_DATA, eom);
	SetCtrlsInPanCBInfo(eom, PockellsControl_CB, eom->eomPanHndl);
	// populate calibration wavelengths
	for (size_t i = 1; i <= nCals; i++) {
		eomCal = ListGetPtrToItem(eom->calib, i);
		Fmt(name, "%s<%f[p1]", eomCal->wavelength); 
		InsertListItem(eom->eomPanHndl, Pockells_Wavelength, -1, name, eomCal->wavelength);
	}
	
	if (nCals) {
		SetCtrlAttribute(eom->eomPanHndl, Pockells_Wavelength, ATTR_DIMMED, 0);
		SetCtrlAttribute(eom->eomPanHndl, Pockells_Pulsed, ATTR_DIMMED, 0);   
		SetCtrlIndex(eom->eomPanHndl, Pockells_Wavelength, eom->calibIdx - 1);
		eomCal = ListGetPtrToItem(eom->calib, eom->calibIdx);
		SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_DIMMED, 0);
		// set limits
		SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_MIN_VALUE, eomCal->d * eomCal->maxPower);
		SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_MAX_VALUE, eomCal->maxPower);
		eom->outputPower 			= eomCal->d;
		eom->pulsedOutputPower		= eomCal->d;
		// apply voltage
		SetCtrlVal(eom->eomPanHndl, Pockells_Output, eom->outputPower * eomCal->maxPower);
		// update pockells cell voltage
		errChk( ApplyPockellsCellVoltage(eom, GetPockellsCellVoltage(eomCal, eomCal->d), &errorInfo.errMsg) );
	}
	
Error:
	
RETURN_ERR
}

static void AddCalibTableEntry (PockellsEOM_type* eom, PockellsEOMCal_type* eomCal)
{
	Point	cell;
	int		nRows;
		
	InsertTableRows(eom->calibPanHndl, Calib_Table, -1, 1, VAL_USE_MASTER_CELL_TYPE);
	GetNumTableRows(eom->calibPanHndl, Calib_Table, &nRows);
	
	// populate entries
	cell.y = nRows; // row
	cell.x = CalibTableColIdx_Wavelength;
	SetTableCellVal(eom->calibPanHndl, Calib_Table, cell, eomCal->wavelength);
	cell.x = CalibTableColIdx_MaxPower;
	SetTableCellVal(eom->calibPanHndl, Calib_Table, cell, eomCal->maxPower);
	cell.x = CalibTableColIdx_CurrentPower;
	if (eom->isPulsed)
		SetTableCellVal(eom->calibPanHndl, Calib_Table, cell, eom->pulsedOutputPower * eomCal->maxPower);
	else
		SetTableCellVal(eom->calibPanHndl, Calib_Table, cell, eom->outputPower * eomCal->maxPower);
	cell.x = CalibTableColIdx_aCoefficient;
	SetTableCellVal(eom->calibPanHndl, Calib_Table, cell, eomCal->a);
	cell.x = CalibTableColIdx_bCoefficient;
	SetTableCellVal(eom->calibPanHndl, Calib_Table, cell, eomCal->b);
	cell.x = CalibTableColIdx_cCoefficient;
	SetTableCellVal(eom->calibPanHndl, Calib_Table, cell, eomCal->c);
	cell.x = CalibTableColIdx_dCoefficient;
	SetTableCellVal(eom->calibPanHndl, Calib_Table, cell, eomCal->d);
}

/*
static void DimCalibTableCells (PockellsEOM_type* eom, int row)
{
	Point 	cell 		= {.x = 0, .y = row};
	int		columns[]	= {	CalibTableColIdx_Wavelength, CalibTableColIdx_aCoefficient, CalibTableColIdx_bCoefficient, CalibTableColIdx_dCoefficient};
	
	for (unsigned int i = 0; i < NumElem(columns); i++) {
		cell.x = columns[i];
		SetTableCellAttribute(eom->calibPanHndl, Calib_Table, cell, ATTR_CELL_MODE, VAL_INDICATOR);
	}
}
*/

static void AddTableEntryToCalibList (PockellsEOM_type* eom, int tableRow)
{
	Point					cell			= { .x = 0, .y = tableRow};
	PockellsEOMCal_type		eomCal			= {.wavelength = 0, .maxPower = 0, .a = 0, .b = 0, .c = 0, .d = 0};
	char					name[50]		= "";
	
	cell.x = CalibTableColIdx_Wavelength;
	GetTableCellVal(eom->calibPanHndl, Calib_Table, cell, &eomCal.wavelength);
	cell.x = CalibTableColIdx_MaxPower;
	GetTableCellVal(eom->calibPanHndl, Calib_Table, cell, &eomCal.maxPower);
	cell.x = CalibTableColIdx_aCoefficient;
	GetTableCellVal(eom->calibPanHndl, Calib_Table, cell, &eomCal.a);
	cell.x = CalibTableColIdx_bCoefficient;
	GetTableCellVal(eom->calibPanHndl, Calib_Table, cell, &eomCal.b);
	cell.x = CalibTableColIdx_cCoefficient;
	GetTableCellVal(eom->calibPanHndl, Calib_Table, cell, &eomCal.c);
	cell.x = CalibTableColIdx_dCoefficient;
	GetTableCellVal(eom->calibPanHndl, Calib_Table, cell, &eomCal.d);
	
	ListInsertItem(eom->calib, &eomCal, END_OF_LIST);
	
	// update calibration wavelength drop down control
	Fmt(name, "%s<%f[p1]", eomCal.wavelength);
	InsertListItem(eom->eomPanHndl, Pockells_Wavelength, -1, name, eomCal.wavelength);
	
	// dim cells
	//DimCalibTableCells(eom, tableRow);
	
}

static BOOL CalibTableRowIsValid (PockellsEOM_type* eom, int tableRow)
{
	Point					cell			= { .x = 0, .y = tableRow};
	double					wavelength		= 0;
	double					maxPower		= 0;
	double					a				= 0;
	double					b				= 0;
	double					c				= 0;
	double					d				= 0;
	
	// do not allow null wavelengths
	cell.x = CalibTableColIdx_Wavelength;
	GetTableCellVal(eom->calibPanHndl, Calib_Table, cell, &wavelength);
	if (wavelength == 0.0) return FALSE;
	
	// do not allow null maximum power
	cell.x = CalibTableColIdx_MaxPower;
	GetTableCellVal(eom->calibPanHndl, Calib_Table, cell, &maxPower);
	if (maxPower == 0.0) return FALSE;
	
	// a > 0
	cell.x = CalibTableColIdx_aCoefficient;
	GetTableCellVal(eom->calibPanHndl, Calib_Table, cell, &a);
	if (a <= 0.0) return FALSE;
	
	// b > 0
	cell.x = CalibTableColIdx_bCoefficient;
	GetTableCellVal(eom->calibPanHndl, Calib_Table, cell, &b);
	if (b <= 0.0) return FALSE;
	
	// c <= 0
	cell.x = CalibTableColIdx_cCoefficient;
	GetTableCellVal(eom->calibPanHndl, Calib_Table, cell, &c);
	if (c > 0.0) return FALSE;
	
	// d >= 0
	cell.x = CalibTableColIdx_dCoefficient;
	GetTableCellVal(eom->calibPanHndl, Calib_Table, cell, &d);
	if (d < 0.0) return FALSE;
	
		
	return TRUE;
}

static void CVICALLBACK PockellsCellCalibration_CB (int menuBar, int menuItem, void *callbackData, int panel)
{
	PockellsModule_type*	eomModule			= callbackData;
	PockellsEOM_type*		eom					= NULL;
	int 					activeTabIdx		= 0; 
	int						workspacePanHndl	= 0;

	
	// get workspace panel handle
	GetPanelAttribute(panel, ATTR_PANEL_PARENT, &workspacePanHndl);
	
	// get pockells cell from active tab page
	GetActiveTabPage(panel, MainPan_Tab, &activeTabIdx);
	eom = *(PockellsEOM_type**) ListGetPtrToItem(eomModule->pockellsCells, activeTabIdx + 1);
	
	// load calibration panel if not loaded already
	if (eom->calibPanHndl) {
		DisplayPanel(eom->calibPanHndl);
		return;
	}
	
	// load and add callback info
	eom->calibPanHndl = LoadPanel(workspacePanHndl, MOD_PockellsModule_UI, Calib);
	SetCtrlsInPanCBInfo(eom, CalibPan_CB, eom->calibPanHndl); 
	
	// populate table with calibration data
	size_t					nCals 	= ListNumItems(eom->calib);
	PockellsEOMCal_type*	eomCal	= NULL;
	for (size_t i = 1; i <= nCals; i++) {
		eomCal = ListGetPtrToItem(eom->calib, i);
		AddCalibTableEntry(eom, eomCal);
		//DimCalibTableCells(eom, i);
	}
	
	// add default new calibration item to the end
	PockellsEOMCal_type		defaultEOMCal = {.wavelength = 0, .maxPower = 1, .a = 0, .b = 0, .c = 0, .d = 0};
	AddCalibTableEntry(eom, &defaultEOMCal);
	
	// display panel
	DisplayPanel(eom->calibPanHndl);
}

static int CVICALLBACK MainPan_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	PockellsModule_type*	eomModule		= callbackData; 
	int 					activeTabIdx	= 0; 
	int						tabPagePanHndl	= 0; 
	int						nTabs			= 0;   
	PockellsEOM_type*		eom				= NULL;  
	
	switch (event) {
			
		case EVENT_KEYPRESS:
			
			switch (control) {
					
				case MainPan_Tab:
					
					switch (eventData1) {
							
						case VAL_FWD_DELETE_VKEY:   // delete selected pockells cell
							
							GetActiveTabPage(panel, control, &activeTabIdx); 
							GetPanelHandleFromTabPage(panel, control, activeTabIdx, &tabPagePanHndl);
							GetPanelAttribute(tabPagePanHndl, ATTR_CALLBACK_DATA, &eom);
							// don't delete this tab page if it's the default "None" page that has no callback data
							if (!eom) return 0;
							
							// delete Pockells cell and tab page
							UnregisterPockellsCell(eomModule, eom);
							discard_PockellsEOM_type(&eom);
							ListRemoveItem(eomModule->pockellsCells, 0, activeTabIdx + 1);
							DeleteTabPage(panel, control, activeTabIdx, 1);
							
							// if there are no more pockells cells, add default "None" tab page and dim "Calibration" menu item
							GetNumTabPages(panel, control, &nTabs);
							if (!nTabs) {
								SetMenuBarAttribute(eomModule->menuBarHndl, eomModule->calMenuItemHndl, ATTR_DIMMED, 1); 
								InsertTabPage(panel, control, -1, "None");
							}
							
							break;
					}
					
					break;
			}
			
			break;
	}
	
	return 0;
}

static int CVICALLBACK CalibPan_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	PockellsEOM_type*		eom			= callbackData;
	PockellsEOMCal_type*	eomCal		= NULL;
	Point					cell		= {.x = 0, .y = 0};
	int						nRows		= 0;
	double					paramVal	= 0;
	
	switch (event) {
			
		case EVENT_COMMIT:
			
			switch (control) {
					
				case Calib_Add:
					
					// dim add button, until this new entry is valid, after which another entry is possible
					GetNumTableRows(panel, Calib_Table, &nRows);
					// add new entry
					AddTableEntryToCalibList(eom, nRows); 
					eomCal = ListGetPtrToItem(eom->calib, nRows);
					// undim pockells cell controls
					SetCtrlAttribute(eom->eomPanHndl, Pockells_Wavelength, ATTR_DIMMED, 0);
					SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_DIMMED, 0);
					SetCtrlAttribute(eom->eomPanHndl, Pockells_Pulsed, ATTR_DIMMED, 0);   
					// update pockells cell output limits if this is the first entry
					if (nRows == 1) {
						// update output
						eom->outputPower 		= eomCal->d;
						eom->pulsedOutputPower  = eomCal->d;
						// set limits
						SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_MIN_VALUE, eomCal->d * eomCal->maxPower);
						SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_MAX_VALUE, eomCal->maxPower);
						// apply voltage
						SetCtrlVal(eom->eomPanHndl, Pockells_Output, eomCal->d * eomCal->maxPower);
						// send command voltage
						errChk( ApplyPockellsCellVoltage(eom, GetPockellsCellVoltage(eomCal, eomCal->d), &errorInfo.errMsg) );
						// update calibration index assigned to the pockells cell
						eom->calibIdx = 1;
					}
					// add default new calibration item to the end
					PockellsEOMCal_type		defaultEOMCal = {.wavelength = 0, .maxPower = 1, .a = 0, .b = 0, .c = 0, .d = 0};
					AddCalibTableEntry(eom, &defaultEOMCal);
					SetCtrlAttribute(panel, control, ATTR_DIMMED, 1);
					break;
					
				case Calib_Close:
					
					OKfreePanHndl(eom->calibPanHndl);
					break;
					
				case Calib_Table:
					
					cell.x = eventData2;
					cell.y = eventData1;
					
					// if this is the last (new entry) row, undim the add button, otherwise update calibration values
					GetNumTableRows(panel, control, &nRows);
					if (eventData1 == nRows) {
						if (CalibTableRowIsValid(eom, nRows))
							SetCtrlAttribute(panel, Calib_Add, ATTR_DIMMED, 0);
						
						return 0;
					}
					
					eomCal = ListGetPtrToItem(eom->calib, eventData1);
					
					switch(eventData2) {
							
						case CalibTableColIdx_Wavelength:	
						
							break;
							
						case CalibTableColIdx_MaxPower:
							
							GetTableCellVal(panel, control, cell, &eomCal->maxPower);
							
							// change scaling for pockells cell control
							if (cell.y == eom->calibIdx) {
								// set limits
								SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_MIN_VALUE, eomCal->d * eomCal->maxPower);
								SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_MAX_VALUE, eomCal->maxPower);
								// update UI power display
								if (eom->isPulsed)
									SetCtrlVal(eom->eomPanHndl, Pockells_Output, eom->pulsedOutputPower * eomCal->maxPower);
								else
									SetCtrlVal(eom->eomPanHndl, Pockells_Output, eom->outputPower * eomCal->maxPower);
							}
							break;
							
						case CalibTableColIdx_CurrentPower:
							
							double	currentPower 	= 0;
							Point	maxPowerCell	= {.x = CalibTableColIdx_MaxPower, .y = cell.y};
							
							GetTableCellVal(panel, control, cell, &currentPower);
							
							if (eom->isPulsed)
								eomCal->maxPower = currentPower/eom->pulsedOutputPower;
							else
								eomCal->maxPower = currentPower/eom->outputPower;
							
							SetTableCellVal(panel, control, maxPowerCell, eomCal->maxPower); 
							// change scaling for pockells cell control
							if (cell.y == eom->calibIdx) {
								// set limits
								SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_MIN_VALUE, eomCal->d * eomCal->maxPower);
								SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_MAX_VALUE, eomCal->maxPower);
								// update UI power display
								if (eom->isPulsed)
									SetCtrlVal(eom->eomPanHndl, Pockells_Output, eom->pulsedOutputPower * eomCal->maxPower);
								else
									SetCtrlVal(eom->eomPanHndl, Pockells_Output, eom->outputPower * eomCal->maxPower);
							}
							
							break;
						
						case CalibTableColIdx_aCoefficient:
							
							GetTableCellVal(panel, control, cell, &paramVal);
							
							if (CalibTableRowIsValid(eom, eventData1))
								eomCal->a = paramVal;
							else
								SetTableCellVal(panel, control, cell, eomCal->a);
								
							break;
							
						case CalibTableColIdx_bCoefficient:
							
							GetTableCellVal(panel, control, cell, &paramVal);
							
							if (CalibTableRowIsValid(eom, eventData1))
								eomCal->b = paramVal;
							else
								SetTableCellVal(panel, control, cell, eomCal->b);
							
							break;
							
						case CalibTableColIdx_cCoefficient:
							
							GetTableCellVal(panel, control, cell, &paramVal);
							
							if (CalibTableRowIsValid(eom, eventData1)) {
								eomCal->c = paramVal;
								if (cell.y == eom->calibIdx) {
								// apply voltage
								if (eom->isPulsed)
									errChk( ApplyPockellsCellVoltage(eom, GetPockellsCellVoltage(eomCal, eom->pulsedOutputPower), &errorInfo.errMsg) );
								else
									errChk( ApplyPockellsCellVoltage(eom, GetPockellsCellVoltage(eomCal, eom->outputPower), &errorInfo.errMsg) );
								}
							} else
								SetTableCellVal(panel, control, cell, eomCal->c);
							
							break;
							
						case CalibTableColIdx_dCoefficient:
							
							GetTableCellVal(panel, control, cell, &paramVal);
							
							if (CalibTableRowIsValid(eom, eventData1)) {
								eomCal->d = paramVal;
								SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_MIN_VALUE, eomCal->d * eomCal->maxPower);
								if (eom->outputPower < eomCal->d) {
									eom->outputPower 		= eomCal->d;
									eom->pulsedOutputPower  = eomCal->d;
									// apply voltage
									SetCtrlVal(eom->eomPanHndl, Pockells_Output, eom->outputPower * eomCal->maxPower);
									// update pockells cell voltage
									errChk( ApplyPockellsCellVoltage(eom, GetPockellsCellVoltage(eomCal, eom->outputPower), &errorInfo.errMsg) );
								}
							} else 
								SetTableCellVal(panel, control, cell, eomCal->d);
								
							break;
					}
					
					break;
					
				case Calib_Del:
					
					// confirm delete
					if (!ConfirmPopup("Pockells cell calibration", "Delete calibration?"))
						return 0;
							
					GetActiveTableCell(panel, control, &cell);
					GetNumTableRows(panel, control, &nRows);
							
					// delete if the selected row is not the last row in the table
					if (cell.y >= nRows) return 0;
							
					DeleteTableRows(panel, control, cell.y, 1);
					ListRemoveItem(eom->calib, 0, cell.y);
							
					// if this wavelength is currently selected by the pockells cell, remove it and select first available wavelength
					int 		wavelengthIdx 	= 0;
					BOOL		isSelected		= FALSE;
							
					GetCtrlIndex(eom->eomPanHndl, Pockells_Wavelength, &wavelengthIdx);
					if (wavelengthIdx + 1 == cell.y)
						isSelected = TRUE;
					else
						isSelected = FALSE;
							
					DeleteListItem(eom->eomPanHndl, Pockells_Wavelength, cell.y - 1, 1);
					GetCtrlIndex(eom->eomPanHndl, Pockells_Wavelength, &wavelengthIdx);
					eom->calibIdx = wavelengthIdx + 1;
							
					if (isSelected && eom->calibIdx) {
						// apply voltage for min transmission at the new wavelength
						eomCal = ListGetPtrToItem(eom->calib, eom->calibIdx);
						// adjust operation range
						SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_MIN_VALUE, eomCal->d * eomCal->maxPower);
						SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_MAX_VALUE, eomCal->maxPower);
						// apply voltage
						SetCtrlVal(eom->eomPanHndl, Pockells_Output, eomCal->d * eomCal->maxPower);  
								
						errChk( ApplyPockellsCellVoltage(eom, GetPockellsCellVoltage(eomCal, eomCal->d), &errorInfo.errMsg) );       
					}
							
					// if there are no more calibrations available, dim the pockells cell controls
					if (nRows == 2) {
						SetCtrlAttribute(eom->eomPanHndl, Pockells_Wavelength, ATTR_DIMMED, 1);
						SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_DIMMED, 1);
						SetCtrlAttribute(eom->eomPanHndl, Pockells_Pulsed, ATTR_DIMMED, 1);   
						}
					
					break;
						
						
			}
			
			break;
			
	}
	
Error:
	
PRINT_ERR

	return 0;
}

//--------------------------
// Pockells cell operation
//--------------------------

static int CVICALLBACK PockellsControl_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR	
	
	PockellsEOM_type*  		eom							= callbackData;
	PockellsEOMCal_type*	eomCal						= NULL;
	int						wavelengthIdx				= 0;
	Point					cell						= {.x = 0, .y = 0};
	BOOL					taskTreeInUse				= FALSE;
	BOOL					taskTreeStateLockObtained	= FALSE;
	
	
	switch (event) {
			
		case EVENT_COMMIT:
			
			// update index of assigned calibration
			GetCtrlIndex(panel, Pockells_Wavelength, &wavelengthIdx);
			if (wavelengthIdx < 0) return 0; // no calibrations
			eom->calibIdx = wavelengthIdx + 1;
			eomCal = ListGetPtrToItem(eom->calib, eom->calibIdx);
			
			switch (control) {
					
				case Pockells_Wavelength:
					
					// adjust operation range
					SetCtrlAttribute(panel, Pockells_Output, ATTR_MIN_VALUE, eomCal->d * eomCal->maxPower);
					SetCtrlAttribute(panel, Pockells_Output, ATTR_MAX_VALUE, eomCal->maxPower);
					// apply voltage for min transmission at this wavelength
					SetCtrlVal(panel, Pockells_Output, eomCal->d * eomCal->maxPower);
					
					// update output
					eom->outputPower 		= eomCal->d;
					eom->pulsedOutputPower  = eomCal->d;
					errChk( ApplyPockellsCellVoltage(eom, GetPockellsCellVoltage(eomCal, eomCal->d), &errorInfo.errMsg) );
					break;
					
				case Pockells_Output:
					
					errChk( IsTaskTreeInUse_GetLock(eom->taskControl, &taskTreeInUse, NULL, &taskTreeStateLockObtained, __LINE__, __FILE__, &errorInfo.errMsg) ); 
					
					// update voltages in the data structure
					if (eom->isPulsed) {
						// set voltage to get desired beam intensity
						GetCtrlVal(panel, control, &eom->pulsedOutputPower);
						eom->pulsedOutputPower /= eomCal->maxPower;
						if (eom->calibPanHndl) {
							cell.x = CalibTableColIdx_CurrentPower;
							cell.y = wavelengthIdx + 1;
							SetTableCellVal(eom->calibPanHndl, Calib_Table, cell, eom->pulsedOutputPower * eomCal->maxPower);
						}
					} else {
						// apply voltage to get desired beam intensity
						GetCtrlVal(panel, control, &eom->outputPower);
						eom->outputPower /= eomCal->maxPower;
					
						if (eom->calibPanHndl) {
							cell.x = CalibTableColIdx_CurrentPower;
							cell.y = wavelengthIdx + 1;
							SetTableCellVal(eom->calibPanHndl, Calib_Table, cell, eom->outputPower * eomCal->maxPower);
						}
						
					}
					
					// generate output signal only if task tree is not in use and if not in pulsed mode
					if (!taskTreeInUse && !eom->isPulsed) {
						errChk( ApplyPockellsCellVoltage(eom, GetPockellsCellVoltage(eomCal, eom->outputPower), &errorInfo.errMsg) );	
					}
					
					errChk( IsTaskTreeInUse_ReleaseLock (eom->taskControl, &taskTreeStateLockObtained, &errorInfo.errMsg) );
					
					break;
					
				case Pockells_Pulsed:
					
					GetCtrlVal(panel, control, &eom->isPulsed);
					if (eom->isPulsed) {
						// apply pockels voltage for min transmission
						errChk( ApplyPockellsCellVoltage(eom, GetPockellsCellVoltage(eomCal, eomCal->d), &errorInfo.errMsg) );
						// update UI with current pulsed power settings
						SetCtrlVal(panel, Pockells_Output, eom->pulsedOutputPower * eomCal->maxPower);
					} else {
						// apply voltage to get desired beam intensity
						errChk( ApplyPockellsCellVoltage(eom, GetPockellsCellVoltage(eomCal, eom->outputPower), &errorInfo.errMsg) );
						// update UI
						SetCtrlVal(panel, Pockells_Output, eom->outputPower * eomCal->maxPower);
					}
					
					break;
			}
			
			break;
			
	}
	
Error:
	
	// cleanup
	if (taskTreeStateLockObtained)
		IsTaskTreeInUse_ReleaseLock (eom->taskControl, &taskTreeStateLockObtained, &errorInfo.errMsg);
		

PRINT_ERR

	return 0;
}

static int ApplyPockellsCellVoltage (PockellsEOM_type* eom, double voltage, char** errorMsg)
{
#define ApplyPockellsCellVoltage_Waveform_NSamples		100
	
INIT_ERR	
	
	double*						commandSignal			= NULL;
	RepeatedWaveform_type*		commandWaveform			= NULL;
	DataPacket_type*			dataPacket				= NULL;
	DSInfo_type*				dsInfo					= NULL;
	
	nullChk( commandSignal = malloc(ApplyPockellsCellVoltage_Waveform_NSamples * sizeof(double)) );
	Set1D(commandSignal, ApplyPockellsCellVoltage_Waveform_NSamples, voltage);
	
	nullChk( commandWaveform = init_RepeatedWaveform_type(RepeatedWaveform_Double, 0, ApplyPockellsCellVoltage_Waveform_NSamples, (void**)&commandSignal, 0) );
	nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(eom->taskControl), WAVERANK) );
	nullChk( dataPacket = init_DataPacket_type(DL_RepeatedWaveform_Double, (void**)&commandWaveform, &dsInfo, (DiscardFptr_type) discard_RepeatedWaveform_type) );
	errChk( SendDataPacket(eom->modulationVChan, &dataPacket, FALSE, &errorInfo.errMsg) );
							  
	return 0;
	
Error:
	
	// cleanup
	OKfree(commandSignal);
	discard_RepeatedWaveform_type(&commandWaveform);
	discard_DataPacket_type(&dataPacket);
	discard_DSInfo_type(&dsInfo);
	
RETURN_ERR
}

static double GetPockellsCellVoltage (PockellsEOMCal_type* eomCal, double normalizedPower)
{
	double	val = (normalizedPower - eomCal->d)/eomCal->a;
	if (val < 0.0) val = 0.0; 	// Must do it like this cause even a simple exact difference of 2.755e-3 - 2.755e-3 can 
								// generate a negative number (although very small) which will make the sqrt fail on its domain check.
	
	return (asin(sqrt(val)) - eomCal->c)/eomCal->b;
}


//-----------------------------------------
// VChan callbacks
//-----------------------------------------

// Timing VChan
static void	TimingVChan_StateChange (VChan_type* self, void* VChanOwner, VChanStates state)
{
INIT_ERR
	
	PockellsEOM_type* 		eom 	= VChanOwner;
	
	/*
	switch (state) {
			
		case VChan_Open:
			
			// if the Timing VChan is open, then activate the Modulation VChan
			SetVChanActive(eom->modulationVChan, TRUE);
			break;
			
		case VChan_Closed:
			
			// if the Timing VChan is closed, then inactivate the Modulation VChan
			SetVChanActive(eom->modulationVChan, FALSE);
			break;
	}
	*/
}


// Modulation VChan
static void	ModulationVChan_StateChange (VChan_type* self, void* VChanOwner, VChanStates state)
{
INIT_ERR

	PockellsEOM_type* 		eom 	= VChanOwner;
	PockellsEOMCal_type* 	eomCal 	= ListGetPtrToItem(eom->calib, eom->calibIdx);
	
	switch (state) {
			
		case VChan_Open:
			
			errChk( ApplyPockellsCellVoltage(eom, GetPockellsCellVoltage(eomCal, eom->outputPower), &errorInfo.errMsg) );
			break;
			
		case VChan_Closed:
			
			break;
	}
	
	return;
	
Error:
	
PRINT_ERR
}

//-----------------------------------------
// Task Controller callbacks
//-----------------------------------------

static int ConfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	//PockellsEOM_type* eom = GetTaskControlModuleData(taskControl);
	
	return 0;
}

static int UnconfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	//PockellsEOM_type* eom = GetTaskControlModuleData(taskControl);
	
	return 0;
}

static void IterateTC (TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortIterationFlag)
{
#define	IterateTC_Err_DataTypeNotSupported	-1
	
#define WaveformToPockellsCommand(timingSignal, dataType)								\
	waveformIN 			= *(Waveform_type**)dataPacketINDataPtr;						\
	timingSignal		= *(dataType**)GetWaveformPtrToData(waveformIN, &nSamples);		\
	samplingRate 		= GetWaveformSamplingRate(waveformIN);							\
	nullChk( commandSignal = malloc(nSamples * sizeof(double)) );						\
	for (size_t i = 0; i < nSamples; i++)												\
		if (timingSignal[i])															\
			commandSignal[i] = voltageHigh;												\
		else																			\
			commandSignal[i] = voltageLow;												\
	nullChk( commandRepeatedWaveform = init_RepeatedWaveform_type(RepeatedWaveform_Double, samplingRate, nSamples, (void**)&commandSignal, 1) );

#define RepeatedWaveformToPockellsCommand(timingSignal, dataType)											\
	repeatedWaveformIN 	= *(RepeatedWaveform_type**)dataPacketINDataPtr;									\
	timingSignal 		= *(dataType**)GetRepeatedWaveformPtrToData(repeatedWaveformIN, &nSamples);			\
	samplingRate 		= GetRepeatedWaveformSamplingRate(repeatedWaveformIN);								\
	nRepeats 			= GetRepeatedWaveformRepeats(repeatedWaveformIN);									\
	nullChk( commandSignal = malloc(nSamples * sizeof(double)) );											\
	for (size_t i = 0; i < nSamples; i++)																	\
		if (timingSignal[i])																				\
			commandSignal[i] = voltageHigh;																	\
		else																								\
			commandSignal[i] = voltageLow;																	\
	nullChk( commandRepeatedWaveform = init_RepeatedWaveform_type(RepeatedWaveform_Double, samplingRate, nSamples, (void**)&commandSignal, nRepeats) );
	
INIT_ERR
	
	PockellsEOM_type* 			eom 						= GetTaskControlModuleData(taskControl);
	PockellsEOMCal_type*		eomCal						= ListGetPtrToItem(eom->calib, eom->calibIdx);
	DataPacket_type*			dataPacketIN				= NULL;
	DataPacket_type*			dataPacketOUT				= NULL;
	DataPacket_type*			nullDataPacket				= NULL;
	DLDataTypes					dataPacketINType			= 0;
	void**						dataPacketINDataPtr			= NULL;
	double*						commandSignal				= NULL;
	Waveform_type*				waveformIN					= NULL;
	RepeatedWaveform_type* 		commandRepeatedWaveform		= NULL;
	RepeatedWaveform_type* 		repeatedWaveformIN			= NULL;
	size_t						nSamples					= 0;
	double						voltageHigh					= 0; // determined by the operation mode
	double						voltageLow					= GetPockellsCellVoltage(eomCal, eomCal->d);
	double						samplingRate				= 0;
	double						nRepeats					= 0;
	DSInfo_type*				dsInfo						= NULL;
	
	
	// timing signals of different types
	char*		 				timingSignal_Char 			= NULL;  
	unsigned char* 				timingSignal_UChar 			= NULL;
	short*		 				timingSignal_Short 			= NULL;  
	unsigned short* 			timingSignal_UShort			= NULL;
	int*		 				timingSignal_Int 			= NULL;  
	unsigned int*	 			timingSignal_UInt			= NULL;
	int64*		 				timingSignal_Int64 			= NULL;  
	uInt64*			 			timingSignal_UInt64			= NULL;
	ssize_t*					timingSignal_SSize			= NULL;
	size_t*						timingSignal_Size			= NULL;
	
	// if timing Sink VChan is closed then just send constant voltage waveform
	if (!IsVChanOpen((VChan_type*)eom->timingVChan)) {
		errChk( ApplyPockellsCellVoltage(eom, GetPockellsCellVoltage(eomCal, eom->outputPower), &errorInfo.errMsg) );
		// send NULL packet as well to signal termination of transmission
		errChk( SendDataPacket(eom->modulationVChan, &nullDataPacket, FALSE, &errorInfo.errMsg) ); 
		errChk( TaskControlIterationDone(taskControl, 0, "", FALSE, &errorInfo.errMsg) );
		return;
	}
	
	// choose between high voltage levels for normal (imaging) mode and pulsed mode
	if (eom->isPulsed)
		voltageHigh	= GetPockellsCellVoltage(eomCal, eom->pulsedOutputPower);
	else
		voltageHigh	= GetPockellsCellVoltage(eomCal, eom->outputPower);
		
	
	errChk( GetDataPacket(eom->timingVChan, &dataPacketIN, &errorInfo.errMsg) );
	
	while (dataPacketIN) {
	
		// get pockells cell timing signal
		dataPacketINDataPtr = GetDataPacketPtrToData(dataPacketIN, &dataPacketINType);
		switch (dataPacketINType) {
			
			//------------------------------------------------------------------------------
			// WAVEFORMS	
			//------------------------------------------------------------------------------
			
			case DL_Waveform_Char:   			
				WaveformToPockellsCommand(timingSignal_Char, char);
				break;
			
			case DL_Waveform_UChar:   			
				WaveformToPockellsCommand(timingSignal_UChar, unsigned char);
				break;
				
			case DL_Waveform_Short:   			
				WaveformToPockellsCommand(timingSignal_Short, short);
				break;
			
			case DL_Waveform_UShort:   			
				WaveformToPockellsCommand(timingSignal_UShort, unsigned short);
				break;
			
			case DL_Waveform_Int:   			
				WaveformToPockellsCommand(timingSignal_Int, int);
				break;
			
			case DL_Waveform_UInt:   			
				WaveformToPockellsCommand(timingSignal_UInt, unsigned int);
				break;
				
			case DL_Waveform_Int64:   			
				WaveformToPockellsCommand(timingSignal_Int64, int64);
				break;
			
			case DL_Waveform_UInt64:   			
				WaveformToPockellsCommand(timingSignal_UInt64, uInt64);
				break;
				
			case DL_Waveform_SSize:   			
				WaveformToPockellsCommand(timingSignal_SSize, ssize_t);
				break;
			
			case DL_Waveform_Size:   			
				WaveformToPockellsCommand(timingSignal_Size, size_t);
				break;	
			
			//------------------------------------------------------------------------------
			// REPEATED WAVEFORMS	
			//------------------------------------------------------------------------------
			
			case DL_RepeatedWaveform_Char:   			
				RepeatedWaveformToPockellsCommand(timingSignal_Char, char);
				break;
			
			case DL_RepeatedWaveform_UChar:   			
				RepeatedWaveformToPockellsCommand(timingSignal_UChar, unsigned char);
				break;
				
			case DL_RepeatedWaveform_Short:   			
				RepeatedWaveformToPockellsCommand(timingSignal_Short, short);
				break;
			
			case DL_RepeatedWaveform_UShort:   			
				RepeatedWaveformToPockellsCommand(timingSignal_UShort, unsigned short);
				break;
			
			case DL_RepeatedWaveform_Int:   			
				RepeatedWaveformToPockellsCommand(timingSignal_Int, int);
				break;
			
			case DL_RepeatedWaveform_UInt:   			
				RepeatedWaveformToPockellsCommand(timingSignal_UInt, unsigned int);
				break;
				
			case DL_RepeatedWaveform_Int64:   			
				RepeatedWaveformToPockellsCommand(timingSignal_Int64, int64);
				break;
			
			case DL_RepeatedWaveform_UInt64:   			
				RepeatedWaveformToPockellsCommand(timingSignal_UInt64, uInt64);
				break;
				
			case DL_RepeatedWaveform_SSize:   			
				RepeatedWaveformToPockellsCommand(timingSignal_SSize, ssize_t);
				break;
			
			case DL_RepeatedWaveform_Size:   			
				RepeatedWaveformToPockellsCommand(timingSignal_Size, size_t);
				break;	
			
			default:
				errChk( TaskControlIterationDone(taskControl, IterateTC_Err_DataTypeNotSupported, "Incoming data packet type is not supported", FALSE, &errorInfo.errMsg));   
				return;
		}
		
		nullChk( dsInfo = GetIteratorDSData(iterator, WAVERANK) );
		nullChk( dataPacketOUT = init_DataPacket_type(DL_RepeatedWaveform_Double, (void**)&commandRepeatedWaveform, &dsInfo, (DiscardFptr_type) discard_RepeatedWaveform_type) );
		errChk( SendDataPacket(eom->modulationVChan, &dataPacketOUT, FALSE, &errorInfo.errMsg) );
	
		ReleaseDataPacket(&dataPacketIN);
		errChk( GetDataPacket(eom->timingVChan, &dataPacketIN, &errorInfo.errMsg) );   // get new packet
	}
	
	// send NULL packet to terminate transmission
	errChk( SendDataPacket(eom->modulationVChan, &nullDataPacket, FALSE, &errorInfo.errMsg) );
	
	errChk( TaskControlIterationDone(taskControl, 0, "", FALSE, &errorInfo.errMsg) );
	
	return;
	
Error:
	
	// cleanup
	ReleaseDataPacket(&dataPacketIN);
	discard_DataPacket_type(&dataPacketOUT);
	discard_RepeatedWaveform_type(&commandRepeatedWaveform);
	OKfree(commandSignal);
	discard_DSInfo_type(&dsInfo);
	
	char* errorMsg = FormatMsg(errorInfo.error, __FILE__, __func__, errorInfo.line, errorInfo.errMsg);
	
	TaskControlIterationDone(taskControl, errorInfo.error, errorMsg, FALSE, NULL);
	OKfree(errorMsg);
}

static int StartTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	//PockellsEOM_type* eom = GetTaskControlModuleData(taskControl);
	
	return 0;
}

static int ResetTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	//PockellsEOM_type* eom = GetTaskControlModuleData(taskControl);
	
	return 0;
}

static int DoneTC (TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg)
{
	//PockellsEOM_type* eom = GetTaskControlModuleData(taskControl);
	
	return 0;
}

static int StoppedTC (TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg)
{
	//PockellsEOM_type* 		eom = GetTaskControlModuleData(taskControl);
	
	return 0;
}

static int TaskTreeStateChange (TaskControl_type* taskControl, TaskTreeStates state, char** errorMsg)
{
	PockellsEOM_type* eom = GetTaskControlModuleData(taskControl);
	
	
	switch (state) {
			
		case TaskTree_Finished:
			
			// undim pulsed mode control
			SetCtrlAttribute(eom->eomPanHndl, Pockells_Pulsed, ATTR_DIMMED, FALSE);
			
			break;
			
		case TaskTree_Started:
			
			// dim pulsed mode control
			SetCtrlAttribute(eom->eomPanHndl, Pockells_Pulsed, ATTR_DIMMED, TRUE);
			
			break;
	}
	
	
	return 0;
}

static int DataReceivedTC (TaskControl_type* taskControl, TCStates taskState, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorMsg)
{
	//PockellsEOM_type* eom = GetTaskControlModuleData(taskControl);
	
	return 0;
}

static void ErrorTC (TaskControl_type* taskControl, int errorID, char* errorMsg)
{
	
}

static int ModuleEventHandler (TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, void* eventData, BOOL const* abortFlag, char** errorMsg)
{
	//PockellsEOM_type* eom = GetTaskControlModuleData(taskControl);
	
	return 0;
}
