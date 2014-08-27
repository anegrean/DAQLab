
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
#include "LaserScanning.h"
#include <userint.h>
#include "UI_LaserScanning.h"



//==============================================================================
// Constants

#define MOD_GalvoScanEngine_UI 		"./Modules/Galvo Scan Engine/UI_GalvoScanEngine.uir"
// default VChan names
#define VChan_FastGalvo_Command		"Fast Galvo Command"
#define VChan_FastGalvo_Position	"Fast Galvo Position" 
#define VChan_SlowGalvo_Command		"Slow Galvo Command"
#define VChan_SlowGalvo_Position	"Slow Galvo Position"
#define VChan_ImageOut				"Image Out"

//==============================================================================
// Types

	// forward declared
typedef struct LaserScanning 	LaserScanning_type;  

//------------------------------
// Generic scan axis calibration
//------------------------------

// Scan axis types
typedef enum {
	NonResonant,							// Normal non-resonant deflectors (common galvos).
	Resonant,								// Resonant deflectors.
	AOD,									// Acousto-optic deflector.
	Translation								// Translation type axis such as a moving stage.
} ScanAxis_type;

// Generic scan axis class
typedef struct {
	ScanAxis_type			scanAxisType;
	char*					calName;
} ScanAxisCal_type;

//---------------------------------------------------
// Normal non-resonant galvo scanner calibration data
//---------------------------------------------------
typedef struct{
	size_t 					n;				// Number of steps.
	double* 				stepSize;  		// Amplitude in [V] Pk-Pk of the command signal.
	double* 				halfSwitch;		// Time to reach 50% of the stepsize voltage in [ms].  
} SwitchTimes_type;

typedef struct{
	size_t 					n;				// Number of amplitudes for whih the slope is measured.
	double* 				slope; 			// Maximum slope in [V/ms] of the command signal that the galvo can still follow given a certain ramp amplitude.
	double* 				amplitude;		// Amplitude in [V] Pk-Pk of the command signal.	
} MaxSlopes_type;

typedef struct{
	size_t 					n;				// Number of triangle waveform amplitudes.
	double 					deadTime;		// Dead time in [ms] before and at the end of each line scan when the galvo is not moving at constant speed (due to turning around).
											// This time is about the same for all scan frequencies and amplitudes that the galvo can follow.
	double* 				commandAmp;		// Amplitude in [V] Pk-Pk of the command signal.
	double* 				actualAmp;		// Amplitude in [V] Pk-Pk where the galvo motion is constant.
	double* 				maxFreq;		// For given amplitude, maximum triangle wave frequency that the galvo can still follow for at least 1 sec.
											// Note: the line scan frequency is twice this frequency
	double* 				resLag;			// Residual lag in [ms] that must be added to lag to accurately describe overall lag during dynamic scanning.
} TriangleCal_type;
		
typedef struct {
	ScanAxisCal_type		calType;		// Base structure describing scan engine calibration. Must be first member of the structure.
	double*					slope;			// in [V/V]
	double*					offset;			// in [V]
	double*					posStdDev;		// in [V]
	double*					lag;			// in [ms]
	SwitchTimes_type*		switchTimes;
	MaxSlopes_type*			maxSlopes;
	TriangleCal_type*		triangleCal;
	struct {
		double resolution;					// in [V]
		double minStepSize;					// in [V]
		double parked;         				// in [V]
		double scantime;					// in [s]
	}	 					galvocalUIset;
} SimpleGalvoCal_type;

//---------------------------------------------------
// Resonant galvo scanner calibration data
//---------------------------------------------------

// add here structures

//---------------------------------------------------





//---------------------------------------------------
// Acousto-optic deflector calibration data
//---------------------------------------------------

// add here structures

//---------------------------------------------------





//---------------------------------------------------
// Translation axis calibration data
//---------------------------------------------------

// add here structures

//---------------------------------------------------


typedef struct {
	SinkVChan_type*			detVChan;
} DetChan_type;

//------------------
// Scan Engine Class
//------------------
// scan engine child classes
typedef enum {
	ScanEngine_RectRaster
} ScanEngineEnum_type;

typedef struct {
	//-----------------------------------
	// Scan engine type
	//-----------------------------------
	ScanEngineEnum_type		engineType;
	
	//-----------------------------------
	// Reference to axis calibration data
	//-----------------------------------
	ScanAxisCal_type*		fastAxisCal;
	ScanAxisCal_type*		slowAxisCal;
	
	//-----------------------------------
	// Task Controller
	//-----------------------------------
	TaskControl_type*		taskControl;
	
	//-----------------------------------
	// VChans
	//-----------------------------------
		// Command signals
	SinkVChan_type*			VChanFastAxisCom;   
	SinkVChan_type*			VChanSlowAxisCom;   
		// Position feedback signals (optional)
	SourceVChan_type*		VChanFastAxisPos;
	SourceVChan_type*		VChanSlowAxisPos;
	BOOL					useFastAxisPos;			// Flag to use position feedback to reconstruct scanner position
	BOOL					useSlowAxisPos;			// Flag to use position feedback to reconstruct scanner position
		// Scan Engine output
	SourceVChan_type*		VChanImgOUT;
		// Detector input channels of DetChan_type* with incoming fluorescence pixel stream 
	ListType				DetChans; 
	
	//-----------------------------------
	// Methods
	//-----------------------------------
	void	(*Discard) (ScanEngine_type** scanEngine);
		
} ScanEngine_type;

//------------------------
// Rectangular raster scan
//------------------------
typedef struct {
	ScanEngine_type			scanEngine;				// Base class, must be first structure member.
	
	//----------------
	// Scan parameters
	//----------------
	double					masterClockRate;		// Clock frequency defined as the smallest time unit in [Hz].
	double					fastAxisSamplingRate;   // Fast axis waveform sampling rate in [Hz].
	double					slowAxisSamplingRate;	// Slow axis waveform sampling rate in [Hz].
	size_t					height;					// Image height in [pix].
	size_t					heightOffset;			// Image height offset from center in [pix].
	size_t					width;					// Image width in [pix].
	size_t					widthOffset;			// Image width offset in [pix].
	double					pixSize;				// Image pixel size in [um]. 
	double					fps;					// Actual frames per second. Upper limit determined by image size, pixel dwell time, etc.
} RectangleRaster_type;


//----------------------
// Module implementation
//----------------------
struct LaserScanning {
	
	// SUPER, must be the first member to inherit from
	
	DAQLabModule_type 		baseClass;
	
	// DATA
	
		// Calibration data per scan axis. Of ScanAxisCal_type* which can be casted to various child classes such as GalvoCal_type*
	ListType				calScanAxis;
		// Scan engines of ScanEngine_type* base class which can be casted to specific scan engines. 
	ListType				scanEngines;
	
	
		//-------------------------
		// UI
		//-------------------------
	
	int						mainPanHndl;

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

//----------------------
// Scan axis calibration
//----------------------
static int CVICALLBACK 				GalvoCal_CB 							(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

//-------------
// Scan Engines
//-------------
	// parent class
static ScanEngine_type*				init_ScanEngine_type					(ScanEngineEnum_type engineType);
static void							discard_ScanEngine_type					(ScanEngine_type** scanEngine);
	// rectangle raster scan
static void							discard_RectangleRaster_type			(ScanEngine_type** scanEngine);

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
	LaserScanning_type*		ls;
	
	if (!mod) {
		ls = malloc (sizeof(LaserScanning_type));
		if (!ls) return NULL;
	} else
		ls = (LaserScanning_type*) mod;
		
	// initialize base class
	initalloc_DAQLabModule(&ls->baseClass, className, instanceName);
	//------------------------------------------------------------
	
	//---------------------------
	// Parent Level 0: DAQLabModule_type 
	
		// DATA
		
		// METHODS
		
			// overriding methods
	ls->baseClass.Discard 			= discard_LaserScanning;
			
	ls->baseClass.Load				= Load; 
	
	ls->baseClass.DisplayPanels		= DisplayPanels;
			
	//---------------------------
	// Child Level 1: LaserScanning_type
	
		// DATA
	
		// init
	ls->calScanAxis					= 0;
	ls->scanEngines					= 0;
		
	if (!(ls->calScanAxis			= ListCreate(sizeof(ScanAxisCal_type*))))	goto Error;
	if (!(ls->scanEngines			= ListCreate(sizeof(ScanEngine_type*))))	goto Error;
			
			
	/*	
		//-------
		// VChans
		//-------
	engine->VChanXCom				= NULL;
	engine->VChanYCom				= NULL;
	engine->VChanImgOUT				= NULL;
	engine->DetChans				= ListCreate(sizeof(DetChan_type*));
	if (!engine->DetChans) {discard_LaserScanning((DAQLabModule_type**)&engine); return NULL;}
	*/
		//---
		// UI
		//---
	ls->mainPanHndl				= 0;
	
	
		// METHODS
		
	//----------------------------------------------------------
	if (!mod)
		return (DAQLabModule_type*) ls;
	else
		return NULL;
	
Error:
	discard_LaserScanning((DAQLabModule_type**)&ls); 
	return NULL;

}

void discard_LaserScanning (DAQLabModule_type** mod)
{
	LaserScanning_type*		ls		= (LaserScanning_type*) (*mod);
	
	if (!ls) return;
	
	//-----------------------------------------
	// discard LaserScanning_type specific data
	//-----------------------------------------
	
	if (ls->calScanAxis) {
		
		ListDispose(ls->calScanAxis);
	}
	if (ls->scanEngines) ListDispose(ls->scanEngines);
	
	  /*
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
	}  */
	  
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

static ScanEngine_type*	init_ScanEngine_type (ScanEngineEnum_type engineType)
{
	ScanEngine_type*	engine = NULL;
	
	switch (engineType) {
			
		case ScanEngine_RectRaster:
			
			break;
	}
	
	return engine;
}

static void	discard_ScanEngine_type (ScanEngine_type** scanEngine)
{
	
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
