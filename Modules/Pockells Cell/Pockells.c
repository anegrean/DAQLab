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
#include "UI_Pockells.h"
#include "Pockells.h"

//==============================================================================
// Constants
#define DAQmxErrChk(functionCall) if( DAQmxFailed(error=(functionCall)) ) goto Error; else

#define MOD_PockellsModule_UI 								"./Modules/Pockells Cell/UI_Pockells.uir"

// VChan base names
#define VChanDataTimeout			10e4					// Timeout in [ms] for Sink VChans to receive data.
#define VChan_PockellsEOM_In		"Pockells In"			// Sink VChan of Waveform_UChar or RepeatedWaveform_UChar used to time the light pulses
#define VChan_PockellsEOM_Out		"Pockells Out"			// Source VChan of Waveform_Double or RepeatedWaveform_Double used to modulate the Pockells cell.


//==============================================================================
// Types

	// forward declared
typedef struct PockellsEOMCal		PockellsEOMCal_type;
typedef struct PockellsEOM			PockellsEOM_type;  
typedef struct PockellsModule		PockellsModule_type;


struct PockellsEOMCal {
	double 						wavelength;					// Wavelength in [nm] at which the calibration of minTransmV and maxTransmV was done.
	double						minTransmV;					// Voltage in [V] at which the pockells cell EOM combined with a polarizing cube attenuates the beam the most.
	double						maxTransmV;					// Voltage in [V] at which the pockells cell EOM combined with a polarizing cube transmits the beam the most.
};

struct PockellsEOM {
	
	// DATA
	double 						minSafeV;					// Minimum safe voltage that can be applied to the pockells cell.
	double 						maxSafeV;					// Maximum safe voltage that can be applied to the pockells cell.
	ListType 					calib;          			// List of pockells EOM calibration wavelengths of PockellsEOMCal_type. 
	SourceVChan_type*			modulationVChan;			// Output waveform VChan used to modulate the pockells cell. VChan of Waveform_Double or RepeatedWaveform_Double.
	SinkVChan_type*				timingVChan;				// Input waveform VChan used to time the modulation of the pockells cell. VChan of Waveform_UChar or RepeatedWaveform_UChar. 
	TaskControl_type*			taskControl;				// Pockells cell task controller.
	
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

static PockellsEOM_type* 		init_PockellsEOM_type				(char 	taskControllerName[],
											    					 char 	timingVChanName[],
																	 char	modulationVChanName[]);

static void						discard_PockellsEOM_type			(PockellsEOM_type** eomPtr);

//------------------
// Module management
//------------------

static int							Load 												(DAQLabModule_type* mod, int workspacePanHndl);

static int 							LoadCfg 											(DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  moduleElement);

static int 							SaveCfg 											(DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_  moduleElement);

static int 							DisplayPanels										(DAQLabModule_type* mod, BOOL visibleFlag); 

	// adds a new Pockells cell
static void CVICALLBACK 			NewPockellsCell_CB 									(int menuBar, int menuItem, void *callbackData, int panel);
	// opens the calibration window for a given pockells cell
static void CVICALLBACK 			PockellsCellCalibration_CB 							(int menuBar, int menuItem, void *callbackData, int panel);
	// pockells module main panel callback
static int CVICALLBACK 				MainPan_CB 											(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
	// pockells module calibration panel callback
static int CVICALLBACK 				CalibPan_CB											(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
//--------------------------
// Pockells cell operation
//--------------------------

static int CVICALLBACK 				PockellsControl_CB									(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


//-----------------------------------------
// VChan callbacks
//-----------------------------------------

	// Timing VChan
static void						TimingVChan_Connected				(VChan_type* self, void* VChanOwner, VChan_type* connectedVChan);
static void						TimingVChan_Disconnected			(VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan);

	// Modulation VChan
static void						ModulationVChan_Connected			(VChan_type* self, void* VChanOwner, VChan_type* connectedVChan);
static void						ModulationVChan_Disconnected		(VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan);

//-----------------------------------------
// Task Controller callbacks
//-----------------------------------------

static int						ConfigureTC							(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static int						UnconfigureTC						(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static void						IterateTC							(TaskControl_type* taskControl, BOOL const* abortIterationFlag);

static void						AbortIterationTC					(TaskControl_type* taskControl, BOOL const* abortFlag);

static int						StartTC								(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static int				 		ResetTC 							(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static int						DoneTC								(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static int						StoppedTC							(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static int						TaskTreeStatus 						(TaskControl_type* taskControl, TaskTreeExecution_type status, char** errorInfo);

static int				 		DataReceivedTC						(TaskControl_type* taskControl, TaskStates_type taskState, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorInfo);

static void 					ErrorTC 							(TaskControl_type* taskControl, int errorID, char* errorMsg);

static int						ModuleEventHandler					(TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, void* eventData, BOOL const* abortFlag, char** errorInfo);

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
	eom->minSafeV 					= 0;
	eom->maxSafeV 					= 0;
	eom->calib						= 0;
	eom->taskControl				= NULL;
	eom->timingVChan				= NULL;
	eom->modulationVChan			= NULL;
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
	PockellsModule_type*	eomModule 					= (PockellsModule_type*) mod;
	int						error						= 0;
	int						newMenuItem					= 0; 
	int						eomPanHndl  				= 0;
	char*					eomName						= NULL;

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
	size_t				nCells 		= ListNumItems(eomModule->pockellsCells);
	PockellsEOM_type*   eom			= NULL;
	
	for (size_t i = 1; i <= nCells; i++) {
		eom = *(PockellsEOM_type**) ListGetPtrToItem(eomModule->pockellsCells, i);
		// insert new pockells cell tab
		InsertPanelAsTabPage(eomModule->mainPanHndl, MainPan_Tab, -1, eomPanHndl);  
		// get panel handle to the new pockells cell
		GetPanelHandleFromTabPage(eomModule->mainPanHndl, MainPan_Tab, i, &eom->eomPanHndl);
		// change pockells cell tab title
		eomName = GetTaskControlName(eom->taskControl); 
		SetTabPageAttribute(eomModule->mainPanHndl, MainPan_Tab, i, ATTR_LABEL_TEXT, eomName);
		OKfree(eomName);
		// add pockells cell panel and controls callback data
		SetPanelAttribute(eom->eomPanHndl, ATTR_CALLBACK_DATA, eom);
		SetCtrlsInPanCBInfo(eom, PockellsControl_CB, eom->eomPanHndl);
		
	}
	
	// delete "None" tab if pockells cells were inserted
	if (nCells) DeleteTabPage(eomModule->mainPanHndl, MainPan_Tab, 0, 1);
	  
	
	// cleanup
	DiscardPanel(eomPanHndl);
	
	return 0;
	
Error:
	
	// cleanup
	if (eomPanHndl) DiscardPanel(eomPanHndl);
	OKfree(eomName);
	
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
	PockellsModule_type*	eomModule	= callbackData;
	
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
	
	// set min & max operation voltages
	SetCtrlVal(eom->calibPanHndl, Calib_VMin, eom->minSafeV);
	SetCtrlVal(eom->calibPanHndl, Calib_VMax, eom->maxSafeV);
	
	// populate table with calibration data
	size_t					nCals 	= ListNumItems(eom->calib);
	PockellsEOMCal_type*	eomCal	= NULL;
	for (size_t i = 1; i <= nCals; i++) {
		eomCal = ListGetPtrToItem(eom->calib, i);
		
	}
	
	
}

static int CVICALLBACK MainPan_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	PockellsModule_type*	eomModule		= callbackData; 
	int 					activeTabIdx	= 0; 
	int						tabPagePanHndl	= 0; 
	int						nTabs			= 0;   
	void*   				tabPageDataPtr	= NULL;  
	
	switch (event) {
			
		case EVENT_KEYPRESS:
			
			switch (control) {
					
				case MainPan_Tab:
					
					switch (eventData1) {
							
						case VAL_FWD_DELETE_VKEY:   // delete selected pockells cell
							
							GetActiveTabPage(panel, control, &activeTabIdx); 
							GetPanelHandleFromTabPage(panel, control, activeTabIdx, &tabPagePanHndl);
							GetPanelAttribute(tabPagePanHndl, ATTR_CALLBACK_DATA, &tabPageDataPtr);
							// don't delete this tab page if it's the default "None" page that has no callback data
							if (!tabPageDataPtr) return 0;
							
							// delete Pockells cell and tab page
							
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
	
	switch (event) {
			
		case EVENT_COMMIT:
			
			switch (control) {
					
				case Calib_Add:
					
					break;
					
				case Calib_VMin:
					
					break;
					
				case Calib_VMax:
					
					break;
					
				case Calib_Close:
					
					DiscardPanel(eom->calibPanHndl);
					eom->calibPanHndl = 0;
					break;
						
						
			}
			
			break;
	}
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
					
				case Pockells_Calibration:
					
					break;
					
				case Pockells_Output:
					
					break;
			}
			
			break;
			
	}
	return 0;
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
