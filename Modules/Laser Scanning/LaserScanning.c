#include <userint.h>
#include "UI_GalvoScanEngine.h"

//==============================================================================
//
// Title:		GalvoScanEngine.c
// Purpose:		A short description of the implementation.
//
// Created on:	25-8-2014 at 14:17:14 by Adrian.
// Copyright:	. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "DAQLab.h" 		// include this first   
#include "GalvoScanEngine.h"


//==============================================================================
// Constants

#define MOD_GalvoScanEngine_UI 		"./Modules/Galvo Scan Engine/UI_GalvoScanEngine.uir"
// default VChan names
#define VChan_XGalvo_Command		"X Galvo Command"
#define VChan_YGalvo_Command		"Y Galvo Command"
#define VChan_ImageOut				"Image Out"

//==============================================================================
// Types

	// forward declared
typedef struct ScanEngine 	ScanEngine_type;  

//------------------
// Galvo calibration
//------------------
typedef struct{
	
	int 				n;				// Number of steps.
	double* 			stepSize;  		// Amplitude in [V] Pk-Pk of the command signal.
	double* 			halfSwitch;		// Time to reach 50% of the stepsize voltage in [ms].  
	
} SwitchTimes_type;

typedef struct{
	
	int 				n;				// Number of amplitudes for whih the slope is measured.
	double* 			slope; 			// Maximum slope in [V/ms] of the command signal that the galvo can still follow given a certain ramp amplitude.
	double* 			amplitude;		// Amplitude in [V] Pk-Pk of the command signal.	
	
} MaxSlopes_type;

typedef struct{
	
	int 				n;				// Number of triangle waveform amplitudes.
	double 				deadTime;		// Dead time in [ms] before and at the end of each line scan when the galvo is not moving at constant speed (due to turning around).
										// This time is about the same for all scan frequencies and amplitudes that the galvo can follow.
	double* 			commandAmp;		// Amplitude in [V] Pk-Pk of the command signal.
	double* 			actualAmp;		// Amplitude in [V] Pk-Pk where the galvo motion is constant.
	double* 			maxFreq;		// For given amplitude, maximum triangle wave frequency that the galvo can still follow for at least 1 sec.
										// Note: the line scan frequency is twice this frequency
	double* 			resLag;			// Residual lag in [ms] that must be added to lag to accurately describe overall lag during dynamic scanning.
	
} TriangleCal_type;
		
typedef struct {
	double*				slope;			// in [V/V]
	double*				offset;			// in [V]
	double*				posStdDev;		// in [V]
	double*				lag;			// in [ms]
	SwitchTimes_type*	switchTimes;
	MaxSlopes_type*		maxSlopes;
	TriangleCal_type*	triangleCal;
	struct {
		double resolution;				// in [V]
		double minStepSize;				// in [V]
		double parked;         			// in [V]
		double scantime;				// in [s]
	}	 				galvocalUIset;
} GalvoCal_type;

typedef struct {
	
	ScanEngine_type*	scanEngineRef;	// Reference to the scan engine that owns this detection VChan.
	SinkVChan_type*		detVChan;
	
} DetChan_type;

//----------------------
// Module implementation
//----------------------
struct ScanEngine {
	
	// SUPER, must be the first member to inherit from
	
	DAQLabModule_type 	baseClass;
	
	// DATA
	
		//-------------------------
		// VChans
		//-------------------------
	
		// Galvo command
	SinkVChan_type*		VChanXCom;
	SinkVChan_type*		VChanYCom;
		// Scan Engine output
	SourceVChan_type*	VChanImgOUT;
		// Detector input channels of DetChan_type* with incoming fluorescence pixel stream 
	ListType			DetChans; 
	
		//-------------------------
		// UI
		//-------------------------
	
	int					mainPanHndl;
	int					scanSetPanHndl;
	
	
	
	// METHODS 
	
};

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//-------------------
// Detection channels
//-------------------
static DetChan_type*				init_DetChan_type						(ScanEngine_type* scanEnginePtr, char VChanName[]);
static void							discard_DetChan_type					(DetChan_type** a);

//---------------------------
// VChan management callbacks
//---------------------------
	// detection VChans
static void							DetVChanConnected						(VChan_type* self, VChan_type* connectedVChan);
static void							DetVChanDisconnected					(VChan_type* self, VChan_type* disconnectedVChan); 

//------------------
// Galvo calibration
//------------------
static int CVICALLBACK 				GalvoCal_CB 							(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

//------------------
// Module management
//------------------

static int							Load 									(DAQLabModule_type* mod, int workspacePanHndl);
static int 							DisplayPanels							(DAQLabModule_type* mod, BOOL visibleFlag); 
static int CVICALLBACK 				ScanEngineSettings_CB 					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static int CVICALLBACK 				ScanControl_CB 							(int panel, int control, int event, void *callbackData, int eventData1, int eventData2); 
static void CVICALLBACK 			SettingsMenu_CB 						(int menuBar, int menuItem, void *callbackData, int panel);



//==============================================================================
// Global variables

//==============================================================================
// Global functions


/// HIFN  Allocates memory and initializes a Galvo Scan Engine for laser scanning microscopy.
/// HIPAR mod/ Pass NULL if both memory allocation and initialization must take place.
/// HIPAR mod/ Pass !NULL if the function performs only an initialization of a DAQLabModule_type.
/// HIRET address of a DAQLabModule_type if memory allocation and initialization took place.
/// HIRET NULL if only initialization was performed.
DAQLabModule_type*	initalloc_LaserScanning (DAQLabModule_type* mod, char className[], char instanceName[])
{
	ScanEngine_type*		engine;
	
	if (!mod) {
		engine = malloc (sizeof(ScanEngine_type));
		if (!engine) return NULL;
	} else
		engine = (ScanEngine_type*) mod;
		
	// initialize base class
	initalloc_DAQLabModule(&engine->baseClass, className, instanceName);
	//------------------------------------------------------------
	
	//---------------------------
	// Parent Level 0: DAQLabModule_type 
	
		// DATA
		
		// METHODS
		
			// overriding methods
	engine->baseClass.Discard 		= discard_LaserScanning;
			
	engine->baseClass.Load			= Load; 
	
	engine->baseClass.DisplayPanels	= DisplayPanels;
			
	//---------------------------
	// Child Level 1: ScanEngine_type
	
		// DATA
		
		//-------
		// VChans
		//-------
	engine->VChanXCom				= NULL;
	engine->VChanYCom				= NULL;
	engine->VChanImgOUT				= NULL;
	engine->DetChans				= ListCreate(sizeof(DetChan_type*));
	if (!engine->DetChans) {discard_LaserScanning((DAQLabModule_type**)&engine); return NULL;}
	
		//---
		// UI
		//---
	engine->mainPanHndl				= 0;
	engine->scanSetPanHndl			= 0;
	
		// METHODS
		
	//----------------------------------------------------------
	if (!mod)
		return (DAQLabModule_type*) engine;
	else
		return NULL;
	
}

void discard_LaserScanning (DAQLabModule_type** mod)
{
	ScanEngine_type*		engine		= (ScanEngine_type*) (*mod);
	
	if (!engine) return;
	
	//--------------------------------------
	// discard ScanEngine_type specific data
	//--------------------------------------
	
		//----------------------------------
		// VChans
		//----------------------------------
	// X galvo command
	if (engine->VChanXCom) {
		DLUnregisterVChan((VChan_type*)engine->VChanXCom);
		discard_VChan_type((VChan_type**)&engine->VChanXCom);
	}
	
	// Y galvo command
	if (engine->VChanYCom) {
		DLUnregisterVChan((VChan_type*)engine->VChanYCom);
		discard_VChan_type((VChan_type**)&engine->VChanYCom);
	}
	
	// scan engine image output
	if (engine->VChanImgOUT) {
		DLUnregisterVChan((VChan_type*)engine->VChanImgOUT);
		discard_VChan_type((VChan_type**)&engine->VChanImgOUT);
	}
	
	// detection channels
	size_t	nDet = ListNumItems(engine->DetChans);
	DetChan_type** detChanPtrPtr;
	for (size_t i = 1; i <= nDet; i++) {
		detChanPtrPtr = ListGetPtrToItem(engine->DetChans, i);
		// remove VChan from framework
		DLUnregisterVChan((VChan_type*)(*detChanPtrPtr)->detVChan);
		discard_DetChan_type(detChanPtrPtr);
	}
		//----------------------------------
		// UI
		//----------------------------------
	if (engine->mainPanHndl) {DiscardPanel(engine->mainPanHndl); engine->mainPanHndl = 0;}
	if (engine->scanSetPanHndl) {DiscardPanel(engine->scanSetPanHndl); engine->scanSetPanHndl = 0;}
	
	//----------------------------------------
	// discard DAQLabModule_type specific data
	//----------------------------------------
	
	discard_DAQLabModule(mod);
}

static int Load (DAQLabModule_type* mod, int workspacePanHndl)
{
	ScanEngine_type*	engine				= (ScanEngine_type*) mod;
	int 				error 				= 0;
	int					menubarHndl			= 0;
	int					settingsMenuItem	= 0;
	
	// load main panel
	errChk(engine->mainPanHndl 		= LoadPanel(workspacePanHndl, MOD_GalvoScanEngine_UI, ScanPan));
	
	// add menu bar to scan panel and link it to module data
	menubarHndl 		= NewMenuBar(engine->mainPanHndl);
	settingsMenuItem	= NewMenu(menubarHndl, "Settings", -1);
	SetMenuBarAttribute(menubarHndl, 0, ATTR_SHOW_IMMEDIATE_ACTION_SYMBOL, 0);
	SetMenuBarAttribute(menubarHndl, settingsMenuItem, ATTR_CALLBACK_DATA, engine);
	SetMenuBarAttribute(menubarHndl, settingsMenuItem, ATTR_CALLBACK_FUNCTION_POINTER, SettingsMenu_CB);
	
	// add callback to all controls in the panel
	SetCtrlsInPanCBInfo(engine, ScanControl_CB, engine->mainPanHndl);   
	
	return 0;
Error:
	
	if (engine->mainPanHndl) {DiscardPanel(engine->mainPanHndl); engine->mainPanHndl = 0;}
	
	return error;
}

static int DisplayPanels (DAQLabModule_type* mod, BOOL visibleFlag)
{
	ScanEngine_type*	engine				= (ScanEngine_type*) mod;
	int					error				= 0;
	
	if (engine->mainPanHndl)
		errChk(SetPanelAttribute(engine->mainPanHndl, ATTR_VISIBLE, visibleFlag));
	
	if (engine->scanSetPanHndl)
		errChk(SetPanelAttribute(engine->scanSetPanHndl, ATTR_VISIBLE, visibleFlag));
	
Error:
	return error;	
}

static void CVICALLBACK SettingsMenu_CB (int menuBar, int menuItem, void *callbackData, int panel)
{
	ScanEngine_type*	engine				= callbackData;
	int					workspacePanHndl;
	
	// load settings panel if not loaded already
	GetPanelAttribute(engine->mainPanHndl, ATTR_PANEL_PARENT, &workspacePanHndl);
	if (engine->scanSetPanHndl) return;
		engine->scanSetPanHndl 	= LoadPanel(workspacePanHndl, MOD_GalvoScanEngine_UI, ScanSetPan);
	
	// update vChans
	
	if (engine->VChanXCom)
		SetCtrlVal(engine->scanSetPanHndl, ScanSetPan_XCommand, engine->VChanXCom);
	
	if (engine->VChanYCom)
		SetCtrlVal(engine->scanSetPanHndl, ScanSetPan_YCommand, engine->VChanYCom);
	
	if (engine->VChanImgOUT)
		SetCtrlVal(engine->scanSetPanHndl, ScanSetPan_ImageOut, engine->VChanImgOUT);
	
	size_t 			nImgChans = ListNumItems(engine->DetChans);
	DetChan_type** 	detChanPtrPtr;
	char*			VChanName;
	for (size_t i = 1; i <= nImgChans; i++) {
		detChanPtrPtr = ListGetPtrToItem(engine->DetChans, i);
		VChanName = GetVChanName((VChan_type*)(*detChanPtrPtr)->detVChan);
		InsertListItem(engine->scanSetPanHndl, ScanSetPan_ImgChans, -1, VChanName, VChanName);
	}
	
	// add callback to all controls in the panel
	SetCtrlsInPanCBInfo(engine, ScanEngineSettings_CB, engine->scanSetPanHndl);  
	
	DisplayPanel(engine->scanSetPanHndl);
}

//-------------------------------------------
// DetChan_type
//-------------------------------------------
static DetChan_type* init_DetChan_type (ScanEngine_type* scanEnginePtr, char VChanName[])
{
	DetChan_type* det = malloc(sizeof(DetChan_type));
	if (!det) return NULL;
	
	det->detVChan 		= init_SinkVChan_type(VChanName, VChan_Waveform, det, DetVChanConnected, DetVChanDisconnected);
	// register VChan with framework
	DLRegisterVChan((VChan_type*)det->detVChan);
	
	det->scanEngineRef	= scanEnginePtr;
	
	return det;
}

static void	discard_DetChan_type (DetChan_type** a)
{
	if (!*a) return;
	
	discard_VChan_type((VChan_type**)&(*a)->detVChan);
	
	OKfree(*a);
}

// detection VChans
static void	DetVChanConnected (VChan_type* self, VChan_type* connectedVChan)
{
	
}

static void	DetVChanDisconnected (VChan_type* self, VChan_type* disconnectedVChan)
{
	
}

static int CVICALLBACK ScanEngineSettings_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ScanEngine_type*	engine	= callbackData;
	
	switch (event)
	{
		case EVENT_COMMIT:
			
			switch (control) {
					
				case ScanSetPan_XCommand:
					
					break;
					
				case ScanSetPan_YCommand:
					
					break;
					
				case ScanSetPan_ImageOut:
					
					break;
					
				case ScanSetPan_AddImgChan:
					
					break;
					
				case ScanSetPan_Close:
					
					DiscardPanel(engine->scanSetPanHndl);
					engine->scanSetPanHndl = 0;
					
					break;
					
				case ScanSetPan_Calibrate:
					
					break;
					
			}
			break;
			
		case EVENT_KEYPRESS:
					
			// continue only if Del key is pressed and the image channels control is active
			if (eventData1 != VAL_FWD_DELETE_VKEY || control != ScanSetPan_ImgChans) break; 
			
			break;
	}
	
	return 0;
}

static int CVICALLBACK GalvoCal_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:

			break;
	}
	return 0;
}

static int CVICALLBACK ScanControl_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:
			
			switch (control) {
			
				case ScanPan_Mode:
					
					break;
					
				case ScanPan_Duration:
					
					break;
				
				case ScanPan_Height:
					
					break;
					
				case ScanPan_Width:
					
					break;
					
				case ScanPan_HeightOffset:
					
					break;
					
				case ScanPan_WidthOffset:
					
					break;
					
				case ScanPan_PixelDwell:
					
					break;
					
				case ScanPan_FPS:
					
					break;
					
				case ScanPan_PSF_FWHM:
					
					break;
					
				case ScanPan_PixelSize:
					
					break;
				
			}

			break;
	}
	return 0;
}
