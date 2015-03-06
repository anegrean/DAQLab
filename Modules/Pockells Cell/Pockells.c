//==============================================================================
//
// Title:		Pockells.c
// Purpose:		A short description of the implementation.
//
// Created on:	5-3-2015 at 12:36:38 by Adrian.
// Copyright:	. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "DAQLab.h" 		// include this first 
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
#define CalibTableColIdx_aCoefficient		2
#define CalibTableColIdx_bCoefficient		3
#define CalibTableColIdx_cCoefficient		4
#define CalibTableColIdx_dCoefficient		5

// VChan base names
#define VChanDataTimeout					10e4					// Timeout in [ms] for Sink VChans to receive data.
#define VChan_PockellsEOM_In				"Pockells In"			// Sink VChan of Waveform_UChar or RepeatedWaveform_UChar used to time the light pulses
#define VChan_PockellsEOM_Out				"Pockells Out"			// Source VChan of Waveform_Double or RepeatedWaveform_Double used to modulate the Pockells cell.


//==============================================================================
// Types

	// forward declared
typedef struct PockellsEOMCal		PockellsEOMCal_type;
typedef struct PockellsEOM			PockellsEOM_type;  
typedef struct PockellsModule		PockellsModule_type;


struct PockellsEOMCal {
	double 						wavelength;					// Wavelength in [nm] at which the calibration of minTransmV and maxTransmV was done.
	double						a;							// Calibration coefficients according to the formula I(Vbias) = a * sin(b * Vbias + c) + d that 
	double						b;							// is a fit to the experimentally obtained transmitted power vs. bias voltage. The units are:
	double						c;							// a [mW], b [1/V], c [], d [mW] 
	double						d;							
};

struct PockellsEOM {
	
	// DATA
	double						eomOutput;					// Pockells cell output in [mW].
	int							calibIdx;					// 1-based calibration index set for the pockells cell from the calib list.
	ListType 					calib;          			// List of pockells EOM calibration wavelengths of PockellsEOMCal_type. 
	SourceVChan_type*			modulationVChan;			// Output waveform VChan used to modulate the pockells cell. VChan of Waveform_Double or RepeatedWaveform_Double.
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

static PockellsEOM_type* 			init_PockellsEOM_type								(char 	taskControllerName[],
											    										 char 	timingVChanName[],
																	 					 char	modulationVChanName[]);

static void							discard_PockellsEOM_type							(PockellsEOM_type** eomPtr);

//------------------
// Module management
//------------------

static int							Load 												(DAQLabModule_type* mod, int workspacePanHndl);

static int 							LoadCfg 											(DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  moduleElement);

static int 							SaveCfg 											(DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_  moduleElement);

static int 							DisplayPanels										(DAQLabModule_type* mod, BOOL visibleFlag); 

	// menu callback to adds a new Pockells cell
static void CVICALLBACK 			NewPockellsCell_CB 									(int menuBar, int menuItem, void *callbackData, int panel);
	// registers a Pockells cell with the framework
static int							RegisterPockellsCell								(PockellsModule_type* eomModule, PockellsEOM_type* eom);
	// unregisters a Pockells cell from the framework
static int							UnregisterPockellsCell								(PockellsModule_type* eomModule, PockellsEOM_type* eom);
	// initializes new pockells cell UI 
static void							InitNewPockellsCellUI								(PockellsModule_type* eomModule, PockellsEOM_type* eom, int eomNewPanHndl);
	// opens the calibration window for a given pockells cell
static void CVICALLBACK 			PockellsCellCalibration_CB 							(int menuBar, int menuItem, void *callbackData, int panel);
	// adds a calibration table entry
static void 						AddCalibTableEntry 									(PockellsEOM_type* eom, PockellsEOMCal_type* eomCal); 
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
static int 							ApplyPockellsCellVoltage 							(PockellsEOM_type* eom, double voltage, char** errorInfo);
	// calculates the command voltage to apply to the pockells cell given a certain calibration to generate the desired output power.
static double						GetPockellsCellVoltage								(PockellsEOMCal_type* eomCal, double power); // power in [mW]
	


//-----------------------------------------
// VChan callbacks
//-----------------------------------------

	// Timing VChan
static void							TimingVChan_Connected								(VChan_type* self, void* VChanOwner, VChan_type* connectedVChan);
static void							TimingVChan_Disconnected							(VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan);

	// Modulation VChan
static void							ModulationVChan_Connected							(VChan_type* self, void* VChanOwner, VChan_type* connectedVChan);
static void							ModulationVChan_Disconnected						(VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan);

//-----------------------------------------
// Task Controller callbacks
//-----------------------------------------

static int							ConfigureTC											(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static int							UnconfigureTC										(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static void							IterateTC											(TaskControl_type* taskControl, BOOL const* abortIterationFlag);

static void							AbortIterationTC									(TaskControl_type* taskControl, BOOL const* abortFlag);

static int							StartTC												(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static int				 			ResetTC 											(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static int							DoneTC												(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static int							StoppedTC											(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static int							TaskTreeStatus 										(TaskControl_type* taskControl, TaskTreeExecution_type status, char** errorInfo);

static int				 			DataReceivedTC										(TaskControl_type* taskControl, TaskStates_type taskState, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorInfo);

static void 						ErrorTC 											(TaskControl_type* taskControl, int errorID, char* errorMsg);

static int							ModuleEventHandler									(TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, void* eventData, BOOL const* abortFlag, char** errorInfo);

//==============================================================================
// Global variables

//==============================================================================
// Global functions

DAQLabModule_type* initalloc_PockellsModule (DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl)
{
	int						error		= 0;
	PockellsModule_type*	eomModule;
	
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

static PockellsEOM_type* init_PockellsEOM_type (char 	taskControllerName[],
											    char 	timingVChanName[],
												char	modulationVChanName[])
{
	int					error	= 0;
	PockellsEOM_type*	eom 	= malloc (sizeof(PockellsEOM_type));
	if (!eom) return NULL;
	
	// init
		// DATA
	eom->eomOutput					= 0.0;
	eom->calibIdx					= 0;
	eom->calib						= 0;
	eom->taskControl				= NULL;
	eom->timingVChan				= NULL;
	eom->modulationVChan			= NULL;
	eom->DLRegistered				= FALSE;
		// UI
	eom->eomPanHndl					= 0;
	eom->calibPanHndl				= 0;
		
	// alloc
		// DATA
	nullChk( eom->calib				= ListCreate(sizeof(PockellsEOMCal_type)) );
	nullChk( eom->taskControl   	= init_TaskControl_type(taskControllerName, eom, DLGetCommonThreadPoolHndl(), ConfigureTC, UnconfigureTC, IterateTC, 
									  AbortIterationTC, StartTC, ResetTC, DoneTC, StoppedTC, TaskTreeStatus, NULL, ModuleEventHandler, ErrorTC) );
	
	DLDataTypes		timingVChanDataTypes[] = {DL_Waveform_UChar, DL_RepeatedWaveform_UChar};
	
	nullChk( eom->timingVChan  		= init_SinkVChan_type(timingVChanName, timingVChanDataTypes, NumElem(timingVChanDataTypes), eom, VChanDataTimeout, TimingVChan_Connected, TimingVChan_Disconnected) );
	nullChk( eom->modulationVChan	= init_SourceVChan_type(modulationVChanName, DL_Waveform_Double, eom, ModulationVChan_Connected, ModulationVChan_Disconnected) );
	
	
	return eom;
	
Error:
	
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

static int Load (DAQLabModule_type* mod, int workspacePanHndl)
{
	PockellsModule_type*	eomModule 			= (PockellsModule_type*) mod;
	int						error				= 0;
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
	SetMenuBarAttribute(eomModule->menuBarHndl, eomModule->calMenuItemHndl, ATTR_DIMMED, 1);  
	
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
		InitNewPockellsCellUI(eomModule, eom, eomPanHndl);
		errChk( RegisterPockellsCell(eomModule, eom) );
	}
	
	// delete "None" tab if present
	if (nCells) DeleteTabPage(eomModule->mainPanHndl, MainPan_Tab, 0, 1);
	
	// display main panel
	DisplayPanel(eomModule->mainPanHndl);
	
	// cleanup
	DiscardPanel(eomPanHndl);
	
	return 0;
	
Error:
	
	// cleanup
	if (eomPanHndl) DiscardPanel(eomPanHndl);
	
	return error;
}

static int LoadCfg (DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  moduleElement)
{
	return 0;
}

static int SaveCfg (DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_  moduleElement)
{
	return 0;
}

static int DisplayPanels (DAQLabModule_type* mod, BOOL visibleFlag)
{
	return 0;
}

static void CVICALLBACK NewPockellsCell_CB (int menuBar, int menuItem, void *callbackData, int panel)
{
	PockellsModule_type*	eomModule				= callbackData;
	char*					taskControllerName  	= NULL;
	char*					timingVChanName  		= NULL;
	char*					modulationVChanName		= NULL;
	PockellsEOM_type*		eom						= NULL;
	int						eomPanHndl				= 0;
	int						workspacePanHndl		= 0;
	int						error					= 0;
	void*					panelData				= NULL;
	int						panelHndl				= 0;
	
	// get new task controller name
	nullChk( taskControllerName 	= DLGetUniqueTaskControllerName(TaskController_BaseName) );
	nullChk( timingVChanName  		= DLGetUniqueVChanName(VChan_PockellsEOM_In) );
	nullChk( modulationVChanName	= DLGetUniqueVChanName(VChan_PockellsEOM_Out) );
	
	GetPanelAttribute(panel, ATTR_PANEL_PARENT, &workspacePanHndl);
	// load Pockells cell panel
	errChk(eomPanHndl = LoadPanel(workspacePanHndl, MOD_PockellsModule_UI, Pockells));
	
	nullChk( eom = init_PockellsEOM_type(taskControllerName, timingVChanName, modulationVChanName) );
	
	ListInsertItem(eomModule->pockellsCells, &eom, END_OF_LIST);
	
	InitNewPockellsCellUI(eomModule, eom, eomPanHndl);
	errChk( RegisterPockellsCell(eomModule, eom) );
	
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
	
	DiscardPanel(eomPanHndl);
	UnregisterPockellsCell(eomModule, eom);
	discard_PockellsEOM_type(&eom);
	OKfree(taskControllerName);
	OKfree(timingVChanName);
	OKfree(modulationVChanName);
}

static int RegisterPockellsCell (PockellsModule_type* eomModule, PockellsEOM_type* eom)
{
	if (eom->DLRegistered) return 0; // already registered
	
	// add task controller to DAQLab framework
	if (!DLAddTaskController((DAQLabModule_type*)eomModule, eom->taskControl)) goto Error;
		
	// add VChans to DAQLab framework and register with task controller
	if (!DLRegisterVChan((DAQLabModule_type*)eomModule, (VChan_type*)eom->modulationVChan)) goto Error;
	if (!DLRegisterVChan((DAQLabModule_type*)eomModule, (VChan_type*)eom->timingVChan)) goto Error;
	if (AddSinkVChan(eom->taskControl, eom->timingVChan, NULL) < 0) goto Error; 
	
	eom->DLRegistered = TRUE;
	return 0;
	
Error:
	
	return -1;
}

static int UnregisterPockellsCell (PockellsModule_type* eomModule, PockellsEOM_type* eom)
{
	if (!eom) return 0;
	if (!eom->DLRegistered) return 0; // not registered yet
	
	// remove VChan from DAQLab framework and unregister from task controller
	if (!DLUnregisterVChan((DAQLabModule_type*)eomModule, (VChan_type*)eom->timingVChan)) goto Error;
	if (RemoveSinkVChan(eom->taskControl, eom->timingVChan) < 0) goto Error;
	if (!DLUnregisterVChan((DAQLabModule_type*)eomModule, (VChan_type*)eom->modulationVChan)) goto Error;
	// unregister task controller
	if (!DLRemoveTaskController((DAQLabModule_type*)eomModule, eom->taskControl)) goto Error;
	
	eom->DLRegistered = FALSE;
	return 0;
	
Error:
	
	return -1;
}

static void InitNewPockellsCellUI (PockellsModule_type* eomModule, PockellsEOM_type* eom, int eomNewPanHndl)
{
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
	if (nCals) {
		SetCtrlAttribute(eom->eomPanHndl, Pockells_Wavelength, ATTR_DIMMED, 0);
		SetCtrlIndex(eom->eomPanHndl, Pockells_Wavelength, eom->calibIdx);
		SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_DIMMED, 0);
		SetCtrlVal(eom->eomPanHndl, Pockells_Output, eom->eomOutput);
	}
	
	for (size_t i = 1; i <= nCals; i++) {
		eomCal = ListGetPtrToItem(eom->calib, i);
		Fmt(name, "%s<%f[p1]", eomCal->wavelength); 
		InsertListItem(eom->eomPanHndl, Pockells_Wavelength, -1, name, eomCal->wavelength);
	}
	
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
	cell.x = CalibTableColIdx_aCoefficient;
	SetTableCellVal(eom->calibPanHndl, Calib_Table, cell, eomCal->a);
	cell.x = CalibTableColIdx_bCoefficient;
	SetTableCellVal(eom->calibPanHndl, Calib_Table, cell, eomCal->b);
	cell.x = CalibTableColIdx_cCoefficient;
	SetTableCellVal(eom->calibPanHndl, Calib_Table, cell, eomCal->c);
	cell.x = CalibTableColIdx_dCoefficient;
	SetTableCellVal(eom->calibPanHndl, Calib_Table, cell, eomCal->d);
}

static void AddTableEntryToCalibList (PockellsEOM_type* eom, int tableRow)
{
	Point					cell			= { .x = 0, .y = tableRow};
	PockellsEOMCal_type		eomCal			= {.wavelength = 0, .a = 0, .b = 0, .c = 0, .d = 0};
	char					name[50]		= "";
	
	cell.x = CalibTableColIdx_Wavelength;
	GetTableCellVal(eom->calibPanHndl, Calib_Table, cell, &eomCal.wavelength);
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
	
}

static BOOL CalibTableRowIsValid (PockellsEOM_type* eom, int tableRow)
{
	Point					cell			= { .x = 0, .y = tableRow};
	double					wavelength		= 0;
	double					a				= 0;
	double					b				= 0;
	size_t					nCals			= ListNumItems(eom->calib);
	PockellsEOMCal_type*	eomCal			= NULL;  
	
	// do not allow null wavelengths
	cell.x = CalibTableColIdx_Wavelength;
	GetTableCellVal(eom->calibPanHndl, Calib_Table, cell, &wavelength);
	if (wavelength == 0.0) return FALSE;
	
	// do not allow entering the same wavelength more than once
	for (size_t i = 1; i <= nCals; i++) {
		eomCal = ListGetPtrToItem(eom->calib, i);
		if (eomCal->wavelength == wavelength)
			return FALSE;
	}
	
	// do not allow coefficient a to be 0
	cell.x = CalibTableColIdx_aCoefficient;
	GetTableCellVal(eom->calibPanHndl, Calib_Table, cell, &a);
	if (a == 0.0) return FALSE;
	
	// do not allow coefficient b to be 0
	cell.x = CalibTableColIdx_bCoefficient;
	GetTableCellVal(eom->calibPanHndl, Calib_Table, cell, &b);
	if (b == 0.0) return FALSE;
		
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
	}
	
	// add default new calibration item to the end
	InsertTableRows(eom->calibPanHndl, Calib_Table, -1, 1, VAL_USE_MASTER_CELL_TYPE);
	
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
	PockellsEOM_type*		eom		= callbackData;
	PockellsEOMCal_type*	eomCal	= NULL;
	Point					cell	= {.x = 0, .y = 0};
	int						nRows	= 0;
	int						error	= 0;
	char*					errMsg	= NULL;
	
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
					// update pockells cell output limits if this is the first entry
					if (nRows == 1) {
						SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_MIN_VALUE, eomCal->d);
						SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_MAX_VALUE, eomCal->a + eomCal->d);
						// send command voltage
						errChk( ApplyPockellsCellVoltage(eom, GetPockellsCellVoltage(eomCal, eomCal->d), &errMsg) );
					}
					// add default new calibration item to the end
					InsertTableRows(eom->calibPanHndl, Calib_Table, -1, 1, VAL_USE_MASTER_CELL_TYPE);
					SetCtrlAttribute(panel, control, ATTR_DIMMED, 1);
					break;
					
				case Calib_Close:
					
					DiscardPanel(eom->calibPanHndl);
					eom->calibPanHndl = 0;
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
						
						case CalibTableColIdx_aCoefficient:
							
							GetTableCellVal(panel, control, cell, &eomCal->a);
							break;
							
						case CalibTableColIdx_bCoefficient:
							
							GetTableCellVal(panel, control, cell, &eomCal->b);
							break;
							
						case CalibTableColIdx_cCoefficient:
							
							GetTableCellVal(panel, control, cell, &eomCal->c);
							break;
							
						case CalibTableColIdx_dCoefficient:
							
							GetTableCellVal(panel, control, cell, &eomCal->d);
							break;
					}
					
					break;
						
						
			}
			
			break;
			
		case EVENT_KEYPRESS:
			
			switch (control) {
					
				case Calib_Table:
					
					switch (eventData1) {
							
						case VAL_FWD_DELETE_VKEY:  
							
							GetActiveTableCell(panel, control, &cell);
							GetNumTableRows(panel, control, &nRows);
							
							// delete if the selected row is not the last row in the table
							if (cell.y >= nRows) return 0;
							
							DeleteTableRows(panel, control, cell.y, 1);
							ListRemoveItem(eom->calib, 0, cell.y);
							
							// if this wavelength is currently selected by the pockells cell, remove it and select first available wavelength
							int wavelengthIdx 	= 0;
							
							DeleteListItem(eom->eomPanHndl, Pockells_Wavelength, cell.y - 1, 1);
							GetCtrlIndex(eom->eomPanHndl, Pockells_Wavelength, &wavelengthIdx);
							eom->calibIdx = wavelengthIdx + 1;
							
							break;
					}
					
					break;
			}
			
			break;
	}
	
	return 0;
	
Error:
	
	if (errMsg)
		DLMsg(errMsg, 1);
	
	return 0;
}

//--------------------------
// Pockells cell operation
//--------------------------

static int CVICALLBACK PockellsControl_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	PockellsEOM_type*   eom		= callbackData;
	
	switch (event) {
			
		case EVENT_COMMIT:
			
			switch (control) {
					
				case Pockells_Wavelength:
					
					// apply voltage for min transmission at this wavelength
					
					break;
					
				case Pockells_Output:
					
					// apply voltage
					
					break;
			}
			
			break;
			
	}
	return 0;
}

static int ApplyPockellsCellVoltage (PockellsEOM_type* eom, double voltage, char** errorInfo)
{
	double*				commandVoltage			= NULL;
	Waveform_type*		commandWaveform			= NULL;
	DataPacket_type*	dataPacket				= NULL;
	int					error					= 0;
	char*				errMsg					= NULL;
	DLDataTypes			modulationVChanDataType = GetSourceVChanDataType(eom->modulationVChan);
	
	// switch modulationVChan data type to DL_Waveform_Double if otherwise
	if (modulationVChanDataType != DL_Waveform_Double)
		SetSourceVChanDataType(eom->modulationVChan, DL_Waveform_Double);
	
	nullChk( commandVoltage = malloc(sizeof(double)) );
	*commandVoltage = voltage;
	
	nullChk( commandWaveform = init_Waveform_type(Waveform_Double, 0, 1, (void**)&commandVoltage) );
	nullChk( dataPacket = init_DataPacket_type(DL_Waveform_Double, (void**)&commandWaveform, NULL, (DiscardPacketDataFptr_type) discard_Waveform_type) );
	errChk( SendDataPacket(eom->modulationVChan, &dataPacket, FALSE, &errMsg) ); 
	
	return 0;
	
Error:
	
	OKfree(commandVoltage);
	discard_Waveform_type(&commandWaveform);
	discard_DataPacket_type(&dataPacket);
	
	if (!errMsg)
		errMsg = StrDup("Out of memory");
	
	if (errorInfo)
		*errorInfo = FormatMsg(error, "ApplyPockellsCellVoltage", errMsg);
	OKfree(errMsg);
	
	return error;
}

static double GetPockellsCellVoltage (PockellsEOMCal_type* eomCal, double power) // power in [mW]
{
	return asin(sqrt((power - eomCal->d)/eomCal->a) - eomCal->c)/eomCal->b;
}


//-----------------------------------------
// VChan callbacks
//-----------------------------------------

// Timing VChan
static void TimingVChan_Connected (VChan_type* self, void* VChanOwner, VChan_type* connectedVChan)
{
	PockellsEOM_type* eom = VChanOwner;
	
}

static void	TimingVChan_Disconnected (VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan)
{
	PockellsEOM_type* eom = VChanOwner;
	
}

// Modulation VChan
static void	ModulationVChan_Connected (VChan_type* self, void* VChanOwner, VChan_type* connectedVChan)
{
	PockellsEOM_type* eom = VChanOwner;
	
}

static void ModulationVChan_Disconnected (VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan)
{
	PockellsEOM_type* eom = VChanOwner;
	
}

//-----------------------------------------
// Task Controller callbacks
//-----------------------------------------

static int ConfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	PockellsEOM_type* eom = GetTaskControlModuleData(taskControl);
	
	return 0;
}

static int UnconfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	PockellsEOM_type* eom = GetTaskControlModuleData(taskControl);
	
	return 0;
}

static void IterateTC (TaskControl_type* taskControl, BOOL const* abortIterationFlag)
{
	PockellsEOM_type* eom = GetTaskControlModuleData(taskControl);
	
	TaskControlIterationDone(taskControl, 0, "", TRUE);
	
}

static void AbortIterationTC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	PockellsEOM_type* eom = GetTaskControlModuleData(taskControl);
	
	TaskControlIterationDone(taskControl, 0, "", FALSE);
}

static int StartTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	PockellsEOM_type* eom = GetTaskControlModuleData(taskControl);
	
	return 0;
}

static int ResetTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	PockellsEOM_type* eom = GetTaskControlModuleData(taskControl);
	
	return 0;
}

static int DoneTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	PockellsEOM_type* eom = GetTaskControlModuleData(taskControl);
	
	return 0;
}

static int StoppedTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	PockellsEOM_type* eom = GetTaskControlModuleData(taskControl);
	
	return 0;
}

static int TaskTreeStatus (TaskControl_type* taskControl, TaskTreeExecution_type status, char** errorInfo)
{
	PockellsEOM_type* eom = GetTaskControlModuleData(taskControl);
	
	return 0;
}

static int DataReceivedTC (TaskControl_type* taskControl, TaskStates_type taskState, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorInfo)
{
	PockellsEOM_type* eom = GetTaskControlModuleData(taskControl);
	
	return 0;
}

static void ErrorTC (TaskControl_type* taskControl, int errorID, char* errorMsg)
{
	
}

static int ModuleEventHandler (TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, void* eventData, BOOL const* abortFlag, char** errorInfo)
{
	PockellsEOM_type* eom = GetTaskControlModuleData(taskControl);
	
	return 0;
}
