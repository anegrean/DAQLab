//==============================================================================
//
// Title:		Pockells.c
// Purpose:		A short description of the implementation.
//
// Created on:	5-3-2015 at 12:36:38 by Adrian Negrean.
// Copyright:	VU University Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "DAQLab.h" 		// include this first 
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
#define CalibTableColIdx_aCoefficient		3
#define CalibTableColIdx_bCoefficient		4
#define CalibTableColIdx_cCoefficient		5
#define CalibTableColIdx_dCoefficient		6

// VChan base names
#define VChanDataTimeout					1e4						// Timeout in [ms] for Sink VChans to receive data.
#define VChan_PockellsEOM_In				"Pockells In"			// Sink VChan of Waveform_UChar or RepeatedWaveform_UChar used to time the light pulses
#define VChan_PockellsEOM_Out				"Pockells Out"			// Source VChan of RepeatedWaveform_Double used to modulate the Pockells cell.


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
	double						eomOutput;					// Pockells cell normalized power output.
	double						maxSafeVoltage;				// Maximum safe voltage that can be applied to the pockells cell in [V].
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

static PockellsEOM_type* 			init_PockellsEOM_type								(char 	taskControllerName[],
											    										 char 	timingVChanName[],
																	 					 char	modulationVChanName[]);

static void							discard_PockellsEOM_type							(PockellsEOM_type** eomPtr);

//------------------
// Module management
//------------------

static int							Load 												(DAQLabModule_type* mod, int workspacePanHndl);

static int 							LoadCfg 											(DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_ moduleElement);

static PockellsEOM_type* 			LoadPockellsCellFromXMLData 						(ActiveXMLObj_IXMLDOMElement_ pockellsXMLElement);

static int 							LoadPockellsCalibrationFromXMLData 					(PockellsEOMCal_type* eomCal, ActiveXMLObj_IXMLDOMElement_ calXMLElement);

static int 							SaveCfg 											(DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ moduleElement);

static int 							SavePockellsCellXMLData								(PockellsEOM_type* eom, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ pockellsXMLElement);

static int 							SavePockellsCalibrationXMLData						(PockellsEOMCal_type* eomCal, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ calXMLElement); 

static int 							DisplayPanels										(DAQLabModule_type* mod, BOOL visibleFlag); 

	// menu callback to adds a new Pockells cell
static void CVICALLBACK 			NewPockellsCell_CB 									(int menuBar, int menuItem, void *callbackData, int panel);
	// registers a Pockells cell with the framework
static int							RegisterPockellsCell								(PockellsModule_type* eomModule, PockellsEOM_type* eom);
	// unregisters a Pockells cell from the framework
static int							UnregisterPockellsCell								(PockellsModule_type* eomModule, PockellsEOM_type* eom);
	// initializes new pockells cell UI 
static int							InitNewPockellsCellUI								(PockellsModule_type* eomModule, PockellsEOM_type* eom, int eomNewPanHndl, char** errorInfo);
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
static int 							ApplyPockellsCellVoltage 							(PockellsEOM_type* eom, double voltage, char** errorInfo);
	// calculates the command voltage to apply to the pockells cell given a certain calibration to generate the desired output power.
static double						GetPockellsCellVoltage								(PockellsEOMCal_type* eomCal, double normalizedPower);
	


//-----------------------------------------
// VChan callbacks
//-----------------------------------------

	// Modulation VChan
static void							ModulationVChan_Connected							(VChan_type* self, void* VChanOwner, VChan_type* connectedVChan);

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
	eom->maxSafeVoltage				= 0;
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
	// configure task controller
	TaskControlEvent(eom->taskControl, TASK_EVENT_CONFIGURE, NULL, NULL);
	
	DLDataTypes		timingVChanDataTypes[] = {DL_Waveform_UChar, DL_RepeatedWaveform_UChar};
	
	nullChk( eom->timingVChan  		= init_SinkVChan_type(timingVChanName, timingVChanDataTypes, NumElem(timingVChanDataTypes), eom, VChanDataTimeout, NULL, NULL) );
	nullChk( eom->modulationVChan	= init_SourceVChan_type(modulationVChanName, DL_RepeatedWaveform_Double, eom, ModulationVChan_Connected, NULL) );
	
	
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
	char*					errMsg				= NULL;
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
		errChk( InitNewPockellsCellUI(eomModule, eom, eomPanHndl, &errMsg) );
		errChk( RegisterPockellsCell(eomModule, eom) );
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
	
	if (errMsg)
		DLMsg(errMsg, 1);
	
	// cleanup
	if (eomPanHndl) DiscardPanel(eomPanHndl);
	
	return error;
}

static int LoadCfg (DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_ moduleElement)
{
	PockellsModule_type*			eomModule						= (PockellsModule_type*)mod;    
	int 							error 							= 0;
	HRESULT							xmlerror						= 0;
	ERRORINFO						xmlERRINFO;
	eomModule->mainPanTopPos										= malloc(sizeof(int));
	eomModule->mainPanLeftPos										= malloc(sizeof(int));
	DAQLabXMLNode 					eomModuleAttr[]					= { {"PanTopPos", BasicData_Int, eomModule->mainPanTopPos},
											  		   					{"PanLeftPos", BasicData_Int, eomModule->mainPanLeftPos}};
	
	//-------------------------------------------------------------------------- 
	// Load main panel position 
	//-------------------------------------------------------------------------- 
	
	errChk( DLGetXMLElementAttributes(moduleElement, eomModuleAttr, NumElem(eomModuleAttr)) ); 
	
	//-------------------------------------------------------------------------- 
	// Load pockells cells
	//--------------------------------------------------------------------------
	
	ActiveXMLObj_IXMLDOMNodeList_	pockellsNodeList				= 0;
	ActiveXMLObj_IXMLDOMNode_		pockellsNode					= 0;
	long							nCells							= 0;
	PockellsEOM_type*				eom								= NULL;
	
	XMLErrChk ( ActiveXML_IXMLDOMElement_getElementsByTagName(moduleElement, &xmlERRINFO, "PockellsCell", &pockellsNodeList) );
	XMLErrChk ( ActiveXML_IXMLDOMNodeList_Getlength(pockellsNodeList, &xmlERRINFO, &nCells) );
	
	for (long i = 0; i < nCells; i++) {
		XMLErrChk ( ActiveXML_IXMLDOMNodeList_Getitem(pockellsNodeList, &xmlERRINFO, i, &pockellsNode) );
		nullChk( eom = LoadPockellsCellFromXMLData((ActiveXMLObj_IXMLDOMElement_) pockellsNode) ); 
		ListInsertItem(eomModule->pockellsCells, &eom, END_OF_LIST);
		OKFreeCAHandle(pockellsNode); 
	}
	
	OKFreeCAHandle(pockellsNodeList);
	
	return 0;
	
Error:
	
	return error;
	
XMLError:   
	
	return xmlerror;
}

static PockellsEOM_type* LoadPockellsCellFromXMLData (ActiveXMLObj_IXMLDOMElement_ pockellsXMLElement)
{
	int 							error 							= 0;
	HRESULT							xmlerror						= 0;
	ERRORINFO						xmlERRINFO;
	PockellsEOM_type*				eom								= NULL;
	double							maxSafeVoltage					= 0;
	int								calibIdx						= 0;
	char*							modulationVChanName				= NULL;
	char*							timingVChanName					= NULL;
	char*							taskControllerName				= NULL;
	
	DAQLabXMLNode 					eomAttr[] 						= {	{"MaxSafeVoltage", BasicData_Double, &maxSafeVoltage},
											  		   		   			{"CalibrationIdx", BasicData_Int, &calibIdx},
															   			{"ModulationVChan", BasicData_CString, &modulationVChanName},
															   			{"TimingVChan", BasicData_CString, &timingVChanName},
															   			{"TaskControllerName", BasicData_CString, &taskControllerName} };
	
	//-------------------------------------------------------------------------- 
	// Load pockells cell data
	//--------------------------------------------------------------------------
	
	errChk( DLGetXMLElementAttributes(pockellsXMLElement, eomAttr, NumElem(eomAttr)) ); 
	
	nullChk( eom = init_PockellsEOM_type(taskControllerName, timingVChanName, modulationVChanName) );
	OKfree(taskControllerName);
	OKfree(timingVChanName);
	OKfree(modulationVChanName);
	
	eom->calibIdx 			= calibIdx;
	eom->maxSafeVoltage 	= maxSafeVoltage;
	
	//-------------------------------------------------------------------------- 
	// Load pockells cell calibrations
	//--------------------------------------------------------------------------
	ActiveXMLObj_IXMLDOMNodeList_	calNodeList				= 0;
	ActiveXMLObj_IXMLDOMNode_		calNode					= 0;
	long							nCals					= 0;
	PockellsEOMCal_type				eomCal;
	
	XMLErrChk ( ActiveXML_IXMLDOMElement_getElementsByTagName(pockellsXMLElement, &xmlERRINFO, "Calibration", &calNodeList) );
	XMLErrChk ( ActiveXML_IXMLDOMNodeList_Getlength(calNodeList, &xmlERRINFO, &nCals) );
	
	for (long i = 0; i < nCals; i++) {
		XMLErrChk ( ActiveXML_IXMLDOMNodeList_Getitem(calNodeList, &xmlERRINFO, i, &calNode) );
		errChk( LoadPockellsCalibrationFromXMLData(&eomCal, (ActiveXMLObj_IXMLDOMElement_) calNode) ); 
		ListInsertItem(eom->calib, &eomCal, END_OF_LIST);
		OKFreeCAHandle(calNode); 
	}
	
	OKFreeCAHandle(calNodeList);

	return eom;
	
Error:
XMLError:   
	
	return NULL;
}

static int LoadPockellsCalibrationFromXMLData (PockellsEOMCal_type* eomCal, ActiveXMLObj_IXMLDOMElement_ calXMLElement)
{
	int 							error 							= 0;
	HRESULT							xmlerror						= 0;
	ERRORINFO						xmlERRINFO;
	DAQLabXMLNode 					calAttr[] 						= {	{"Wavelength", BasicData_Double, &eomCal->wavelength},
																		{"MaxPower", BasicData_Double, &eomCal->maxPower},
											  		   		   			{"a", BasicData_Double, &eomCal->a},
															   			{"b", BasicData_Double, &eomCal->b},
															   			{"c", BasicData_Double, &eomCal->c},
															   			{"d", BasicData_Double, &eomCal->d}};
																		
	errChk( DLGetXMLElementAttributes(calXMLElement, calAttr, NumElem(calAttr)) ); 
																		
	return 0;
	
Error:
	
	return error;
	
XMLError:   
	
	return xmlerror;	
	
	
}

static int SaveCfg (DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_  moduleElement)
{
	PockellsModule_type*			eomModule				= (PockellsModule_type*)mod;
	int								error					= 0;
	int								lsPanTopPos				= 0;
	int								lsPanLeftPos			= 0;
	HRESULT							xmlerror				= 0;
	ERRORINFO						xmlERRINFO;
	DAQLabXMLNode 					moduleAttr[] 			= {{"PanTopPos", BasicData_Int, &lsPanTopPos},
											  		   		   {"PanLeftPos", BasicData_Int, &lsPanLeftPos}};
	
	//--------------------------------------------------------------------------
	// Save pockells module main panel position
	//--------------------------------------------------------------------------
	
	errChk( GetPanelAttribute(eomModule->mainPanHndl, ATTR_LEFT, &lsPanLeftPos) );
	errChk( GetPanelAttribute(eomModule->mainPanHndl, ATTR_TOP, &lsPanTopPos) );
	errChk( DLAddToXMLElem(xmlDOM, moduleElement, moduleAttr, DL_ATTRIBUTE, NumElem(moduleAttr)) ); 
	
	//--------------------------------------------------------------------------
	// Save pockells cells
	//--------------------------------------------------------------------------
	size_t							nCells					= ListNumItems(eomModule->pockellsCells);
	PockellsEOM_type*				eom						= NULL;
	ActiveXMLObj_IXMLDOMElement_	pockellsXMLElement		= 0;
	
	for (size_t i = 1; i <= nCells; i++) {
		eom = *(PockellsEOM_type**) ListGetPtrToItem(eomModule->pockellsCells, i);
		// create new pockells cell element
		XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, "PockellsCell", &pockellsXMLElement) );
		// add pockells cell data to the element
		errChk( SavePockellsCellXMLData(eom, xmlDOM, pockellsXMLElement) );
		// add pockells cell element to the module
		XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (moduleElement, &xmlERRINFO, pockellsXMLElement, NULL) );
		OKFreeCAHandle(pockellsXMLElement); 
	}
	
	
	return 0;
	
Error:
	
	return error;
	
XMLError:   
	
	return xmlerror;
}

static int SavePockellsCellXMLData (PockellsEOM_type* eom, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ pockellsXMLElement)
{
	int								error					= 0;
	HRESULT							xmlerror				= 0;
	char*							modulationVChanName		= GetVChanName((VChan_type*)eom->modulationVChan);
	char*							timingVChanName			= GetVChanName((VChan_type*)eom->timingVChan);
	char*							taskControllerName		= GetTaskControlName(eom->taskControl);
	ERRORINFO						xmlERRINFO;
	ActiveXMLObj_IXMLDOMElement_	calXMLElement			= 0;   // pockells cell XML calibration element
	DAQLabXMLNode 					eomAttr[] 				= {{"MaxSafeVoltage", BasicData_Double, &eom->maxSafeVoltage},
											  		   		   {"CalibrationIdx", BasicData_Int, &eom->calibIdx},
															   {"ModulationVChan", BasicData_CString, modulationVChanName},
															   {"TimingVChan", BasicData_CString, timingVChanName},
															   {"TaskControllerName", BasicData_CString, taskControllerName}};
	
															   
	//--------------------------------------------------------------------------
	// Save pockells cell attributes
	//--------------------------------------------------------------------------
	
	errChk( DLAddToXMLElem(xmlDOM, pockellsXMLElement, eomAttr, DL_ATTRIBUTE, NumElem(eomAttr)) );
	OKfree(modulationVChanName);
	OKfree(timingVChanName);
	OKfree(taskControllerName);
	
	//--------------------------------------------------------------------------
	// Save pockells cell calibrations
	//--------------------------------------------------------------------------
	size_t							nCals				= ListNumItems(eom->calib);
	PockellsEOMCal_type*			eomCal				= NULL;
	for (size_t i = 1; i <= nCals; i++) {
		eomCal = ListGetPtrToItem(eom->calib, i);
		// create new calibration element
		XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, "Calibration", &calXMLElement) );
		// add calibration data to the element
		errChk( SavePockellsCalibrationXMLData(eomCal, xmlDOM, calXMLElement) );
		// add calibration element to the pockells cell element
		XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (pockellsXMLElement, &xmlERRINFO, calXMLElement, NULL) );
		OKFreeCAHandle(calXMLElement); 
	}
	
	return 0;
	
Error:
	
	return error;
	
XMLError:   
	
	return xmlerror;	
	
}

static int SavePockellsCalibrationXMLData (PockellsEOMCal_type* eomCal, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ calXMLElement)
{
	int								error					= 0;
	HRESULT							xmlerror				= 0;
	ERRORINFO						xmlERRINFO;
	DAQLabXMLNode 					calAttr[] 				= {{"Wavelength", BasicData_Double, &eomCal->wavelength},
															   {"MaxPower", BasicData_Double, &eomCal->maxPower},
											  		   		   {"a", BasicData_Double, &eomCal->a},
															   {"b", BasicData_Double, &eomCal->b},
															   {"c", BasicData_Double, &eomCal->c},
															   {"d", BasicData_Double, &eomCal->d}};
			
	//--------------------------------------------------------------------------
	// Save calibration attributes
	//--------------------------------------------------------------------------
	
	errChk( DLAddToXMLElem(xmlDOM, calXMLElement, calAttr, DL_ATTRIBUTE, NumElem(calAttr)) );
	
	return 0;
	
Error:
	
	return error;
	
XMLError:   
	
	return xmlerror;
	
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
	char*					errMsg					= NULL;
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
	
	errChk( InitNewPockellsCellUI(eomModule, eom, eomPanHndl, &errMsg) );
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
	
	if (errMsg)
		DLMsg(errMsg, 1);
	OKfree(errMsg);
	
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

static int InitNewPockellsCellUI (PockellsModule_type* eomModule, PockellsEOM_type* eom, int eomNewPanHndl, char** errorInfo)
{
	int						nTabs			= 0;
	char*					eomName			= NULL;
	size_t					nCals			= ListNumItems(eom->calib); 
	PockellsEOMCal_type*	eomCal			= NULL; 
	char					name[50]		= ""; 
	int						error			= 0;
	char*					errMsg			= NULL;
	
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
		SetCtrlIndex(eom->eomPanHndl, Pockells_Wavelength, eom->calibIdx - 1);
		eomCal = ListGetPtrToItem(eom->calib, eom->calibIdx);
		SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_DIMMED, 0);
		// set limits
		SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_MIN_VALUE, eomCal->d * eomCal->maxPower);
		SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_MAX_VALUE, eomCal->maxPower);
		eom->eomOutput = eomCal->d;
		// apply voltage
		SetCtrlVal(eom->eomPanHndl, Pockells_Output, eom->eomOutput * eomCal->maxPower);
		// update pockells cell voltage
		errChk( ApplyPockellsCellVoltage(eom, GetPockellsCellVoltage(eomCal, eom->eomOutput), &errMsg) );
	}
	
	return 0;
	
Error:
	
	if (!errMsg)
		errMsg = StrDup("Out of memory");
	
	if (errorInfo)
		*errorInfo = FormatMsg(error, "InitNewPockellsCellUI", errMsg);
	OKfree(errMsg);
	
	return error;
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
	size_t					nCals			= ListNumItems(eom->calib);
	PockellsEOMCal_type*	eomCal			= NULL;  
	
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
	PockellsEOM_type*		eom			= callbackData;
	PockellsEOMCal_type*	eomCal		= NULL;
	Point					cell		= {.x = 0, .y = 0};
	int						nRows		= 0;
	int						error		= 0;
	char*					errMsg		= NULL;
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
					// update pockells cell output limits if this is the first entry
					if (nRows == 1) {
						// update output
						eom->eomOutput = eomCal->d;
						// set limits
						SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_MIN_VALUE, eomCal->d * eomCal->maxPower);
						SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_MAX_VALUE, eomCal->maxPower);
						// apply voltage
						SetCtrlVal(eom->eomPanHndl, Pockells_Output, eomCal->d * eomCal->maxPower);
						// send command voltage
						errChk( ApplyPockellsCellVoltage(eom, GetPockellsCellVoltage(eomCal, eomCal->d), &errMsg) );
						// update calibration index assigned to the pockells cell
						eom->calibIdx = 1;
					}
					// add default new calibration item to the end
					PockellsEOMCal_type		defaultEOMCal = {.wavelength = 0, .maxPower = 1, .a = 0, .b = 0, .c = 0, .d = 0};
					AddCalibTableEntry(eom, &defaultEOMCal);
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
							
						case CalibTableColIdx_MaxPower:
							
							GetTableCellVal(panel, control, cell, &eomCal->maxPower);
							// change scaling for pockells cell control
							if (cell.y == eom->calibIdx) {
								// set limits
								SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_MIN_VALUE, eomCal->d * eomCal->maxPower);
								SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_MAX_VALUE, eomCal->maxPower);
								// update UI power display
								SetCtrlVal(eom->eomPanHndl, Pockells_Output, eom->eomOutput * eomCal->maxPower);
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
								if (cell.y == eom->calibIdx)
								// apply voltage
								errChk( ApplyPockellsCellVoltage(eom, GetPockellsCellVoltage(eomCal, eom->eomOutput), &errMsg) );
							}
							else
								SetTableCellVal(panel, control, cell, eomCal->c);
							
							 
							break;
							
						case CalibTableColIdx_dCoefficient:
							
							GetTableCellVal(panel, control, cell, &paramVal);
							
							if (CalibTableRowIsValid(eom, eventData1)) {
								eomCal->d = paramVal;
								SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_MIN_VALUE, eomCal->d * eomCal->maxPower);
								if (eom->eomOutput < eomCal->d) {
									eom->eomOutput = eomCal->d;
									// apply voltage
									SetCtrlVal(eom->eomPanHndl, Pockells_Output, eom->eomOutput * eomCal->maxPower);
									// update pockells cell voltage
									errChk( ApplyPockellsCellVoltage(eom, GetPockellsCellVoltage(eomCal, eom->eomOutput), &errMsg) );
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
					eom->calibIdx == wavelengthIdx + 1;
							
					if (isSelected && eom->calibIdx) {
						// apply voltage for min transmission at the new wavelength
						eomCal = ListGetPtrToItem(eom->calib, eom->calibIdx);
						// adjust operation range
						SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_MIN_VALUE, eomCal->d * eomCal->maxPower);
						SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_MAX_VALUE, eomCal->maxPower);
						// apply voltage
						SetCtrlVal(eom->eomPanHndl, Pockells_Output, eomCal->d * eomCal->maxPower);  
								
						errChk( ApplyPockellsCellVoltage(eom, GetPockellsCellVoltage(eomCal, eomCal->d), &errMsg) );       
					}
							
					// if there are no more calibrations available, dim the pockells cell controls
					if (nRows == 2) {
						SetCtrlAttribute(eom->eomPanHndl, Pockells_Wavelength, ATTR_DIMMED, 1);
						SetCtrlAttribute(eom->eomPanHndl, Pockells_Output, ATTR_DIMMED, 1);
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
	PockellsEOM_type*  		eom				= callbackData;
	PockellsEOMCal_type*	eomCal			= NULL;
	int						wavelengthIdx	= 0;
	int						error			= 0;
	char*					errMsg			= NULL;
	
	// update index of assigned calibration
	GetCtrlIndex(panel, Pockells_Wavelength, &wavelengthIdx);
	if (wavelengthIdx < 0) return 0; // no calibrations
	eom->calibIdx = wavelengthIdx + 1;
	eomCal = ListGetPtrToItem(eom->calib, eom->calibIdx);
	
	switch (event) {
			
		case EVENT_COMMIT:
			
			switch (control) {
					
				case Pockells_Wavelength:
					
					// adjust operation range
					SetCtrlAttribute(panel, Pockells_Output, ATTR_MIN_VALUE, eomCal->d * eomCal->maxPower);
					SetCtrlAttribute(panel, Pockells_Output, ATTR_MAX_VALUE, eomCal->maxPower);
					// apply voltage for min transmission at this wavelength
					SetCtrlVal(panel, Pockells_Output, eomCal->d * eomCal->maxPower);
					
					// update output
					eom->eomOutput = eomCal->d;
					errChk( ApplyPockellsCellVoltage(eom, GetPockellsCellVoltage(eomCal, eomCal->d), &errMsg) );
					break;
					
				case Pockells_Output:
					
					// apply voltage to get desired beam intensity
					GetCtrlVal(panel, control, &eom->eomOutput);
					eom->eomOutput /= eomCal->maxPower;
					errChk( ApplyPockellsCellVoltage(eom, GetPockellsCellVoltage(eomCal, eom->eomOutput), &errMsg) );
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

static int ApplyPockellsCellVoltage (PockellsEOM_type* eom, double voltage, char** errorInfo)
{
#define ApplyPockellsCellVoltage_Waveform_NSamples		100
	
	double*						commandSignal			= NULL;
	RepeatedWaveform_type*		commandWaveform			= NULL;
	DataPacket_type*			dataPacket				= NULL;
	DataPacket_type*			nullPacket				= NULL;
	int							error					= 0;
	char*						errMsg					= NULL;
	
	nullChk( commandSignal = malloc(ApplyPockellsCellVoltage_Waveform_NSamples * sizeof(double)) );
	Set1D(commandSignal, ApplyPockellsCellVoltage_Waveform_NSamples, voltage);
	
	nullChk( commandWaveform = init_RepeatedWaveform_type(RepeatedWaveform_Double, 0, ApplyPockellsCellVoltage_Waveform_NSamples, (void**)&commandSignal, 0) );
	nullChk( dataPacket = init_DataPacket_type(DL_RepeatedWaveform_Double, (void**)&commandWaveform, NULL, (DiscardPacketDataFptr_type) discard_RepeatedWaveform_type) );
	errChk( SendDataPacket(eom->modulationVChan, &dataPacket, FALSE, &errMsg) );
							  
	return 0;
	
Error:
	
	OKfree(commandSignal);
	discard_RepeatedWaveform_type(&commandWaveform);
	discard_DataPacket_type(&dataPacket);
	
	if (!errMsg)
		errMsg = StrDup("Out of memory");
	
	if (errorInfo)
		*errorInfo = FormatMsg(error, "ApplyPockellsCellVoltage", errMsg);
	OKfree(errMsg);
	
	return error;
}

static double GetPockellsCellVoltage (PockellsEOMCal_type* eomCal, double normalizedPower)
{
	return (asin(sqrt((normalizedPower - eomCal->d)/eomCal->a)) - eomCal->c)/eomCal->b;
}


//-----------------------------------------
// VChan callbacks
//-----------------------------------------

// Modulation VChan
static void	ModulationVChan_Connected (VChan_type* self, void* VChanOwner, VChan_type* connectedVChan)
{
	PockellsEOM_type* 		eom 	= VChanOwner;
	PockellsEOMCal_type* 	eomCal 	= ListGetPtrToItem(eom->calib, eom->calibIdx);
	int						error	= 0;
	char*					errMsg	= NULL;
	
	errChk( ApplyPockellsCellVoltage(eom, GetPockellsCellVoltage(eomCal, eom->eomOutput), &errMsg) );
	
	return;
	
Error:
	
	if (!errMsg)
		errMsg = StrDup("Out of memory");
	
	DLMsg(errMsg, 1);
	OKfree(errMsg);
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
#define	IterateTC_Err_DataTypeNotSupported	-1
	
	PockellsEOM_type* 			eom 						= GetTaskControlModuleData(taskControl);
	PockellsEOMCal_type*		eomCal						= ListGetPtrToItem(eom->calib, eom->calibIdx);
	int							error						= 0;
	char*						errMsg						= NULL;
	DataPacket_type*			dataPacketIN				= NULL;
	DataPacket_type*			dataPacketOUT				= NULL;
	DataPacket_type*			nullDataPacket				= NULL;
	DLDataTypes					dataPacketINType			= 0;
	void**						dataPacketINDataPtr			= NULL;
	double*						commandSignal				= NULL;
	unsigned char*				timingSignal				= NULL;
	Waveform_type*				waveformIN					= NULL;
	RepeatedWaveform_type* 		commandRepeatedWaveform		= NULL;
	RepeatedWaveform_type* 		repeatedWaveformIN			= NULL;
	size_t						nSamples					= 0;
	double						commandVoltage				= GetPockellsCellVoltage(eomCal, eom->eomOutput);
	double						samplingRate				= 0;
	double						nRepeats					= 0;
	
	
	// if there is no Source VChan attached to the timing Sink VChan of the pockells cell, then just send constant voltage waveform
	if (!IsVChanConnected((VChan_type*)eom->timingVChan)) {
		errChk( ApplyPockellsCellVoltage(eom, GetPockellsCellVoltage(eomCal, eom->eomOutput), &errMsg) );
		// send NULL packet as well to signal termination of transmission
		errChk( SendDataPacket(eom->modulationVChan, &nullDataPacket, FALSE, &errMsg) ); 
		TaskControlIterationDone(taskControl, 0, "", FALSE);
		return;
	}
	
	errChk( GetDataPacket(eom->timingVChan, &dataPacketIN, &errMsg) );
	
	while (dataPacketIN) {
	
		// get pockells cell timing signal
		dataPacketINDataPtr = GetDataPacketPtrToData(dataPacketIN, &dataPacketINType);
		switch (dataPacketINType) {
				
			case DL_Waveform_UChar:   			// Pockells output VChan is of DL_Waveform_Double type
				
				waveformIN 			= *(Waveform_type**)dataPacketINDataPtr;
				timingSignal 		= *(unsigned char**)GetWaveformPtrToData(waveformIN, &nSamples);
				samplingRate 		= GetWaveformSamplingRate(waveformIN);
				break;
				
			case DL_RepeatedWaveform_UChar:		// Pockells output VChan is of DL_RepeatedWaveform_Double type
				
				repeatedWaveformIN 	= *(RepeatedWaveform_type**)dataPacketINDataPtr;
				timingSignal 		= *(unsigned char**)GetRepeatedWaveformPtrToData(repeatedWaveformIN, &nSamples);
				samplingRate 		= GetRepeatedWaveformSamplingRate(repeatedWaveformIN);
				nRepeats 			= GetRepeatedWaveformRepeats(repeatedWaveformIN);
				break;
				
			default:
				TaskControlIterationDone(taskControl, IterateTC_Err_DataTypeNotSupported, "Incoming data type is not supported", FALSE);   
				return;
		}
		
		// apply pockells cell command voltage to the timing signal
		nullChk( commandSignal = malloc(nSamples * sizeof(double)) );
		for (size_t i = 0; i < nSamples; i++)
			commandSignal[i] = (timingSignal[i] != 0) * commandVoltage;
		
		// send pockells cell command waveform
		switch (dataPacketINType) { 
				
			case DL_Waveform_UChar:
				
				nullChk( commandRepeatedWaveform = init_RepeatedWaveform_type(RepeatedWaveform_Double, samplingRate, nSamples, (void**)&commandSignal, 1) );
				break;
				
			case DL_RepeatedWaveform_UChar:
				nullChk( commandRepeatedWaveform = init_RepeatedWaveform_type(RepeatedWaveform_Double, samplingRate, nSamples, (void**)&commandSignal, nRepeats) );
				break;
				
			default:
				TaskControlIterationDone(taskControl, IterateTC_Err_DataTypeNotSupported, "Incoming data type is not supported", FALSE);   
				return;
		}
		nullChk( dataPacketOUT = init_DataPacket_type(DL_RepeatedWaveform_Double, (void**)&commandRepeatedWaveform, NULL, (DiscardPacketDataFptr_type) discard_RepeatedWaveform_type) );
		errChk( SendDataPacket(eom->modulationVChan, &dataPacketOUT, FALSE, &errMsg) );
	
		ReleaseDataPacket(&dataPacketIN);
		errChk( GetDataPacket(eom->timingVChan, &dataPacketIN, &errMsg) );   // get new packet
	}
	
	// send NULL packet to terminate transmission
	errChk( SendDataPacket(eom->modulationVChan, &nullDataPacket, FALSE, &errMsg) );
	
	TaskControlIterationDone(taskControl, 0, "", FALSE);
	
	return;
	
Error:
	
	// cleanup
	ReleaseDataPacket(&dataPacketIN);
	discard_DataPacket_type(&dataPacketOUT);
	discard_RepeatedWaveform_type(&commandRepeatedWaveform);
	OKfree(commandSignal);
	
	if (!errMsg)
		errMsg = StrDup("Out of memory");
	
	TaskControlIterationDone(taskControl, error, errMsg, FALSE);
	OKfree(errMsg);
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
	PockellsEOM_type* 		eom 					= GetTaskControlModuleData(taskControl);
	
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
