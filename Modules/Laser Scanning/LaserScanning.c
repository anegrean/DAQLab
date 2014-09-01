
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
	Translation								// Translation type scanning axis such as a moving stage.
} ScanAxis_type;

// Generic scan axis class
typedef struct ScanAxisCal ScanAxisCal_type;
struct ScanAxisCal {
	// DATA
	ScanAxis_type			scanAxisType;
	char*					calName;
	SourceVChan_type*		VChanCom;   	// VChan used to send command signals to the scan axis (optional, depending on the scan axis type)
	SinkVChan_type*			VChanPos;		// VChan used to receive position feedback signals from the scan axis (optional, depending on the scan axis type)
	// METHODS
	void	(*Discard) (ScanAxisCal_type** scanAxisCal);
};

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
	ScanAxisCal_type		base;			// Base structure describing scan engine calibration. Must be first member of the structure.
	double					commandVMin;	// Minimum galvo command voltage in [V] for maximal deflection in a given direction.
	double					commandVMax;	// Maximum galvo command voltage in [V] for maximal deflection in the oposite direction when applying VminOut.
	double					positionVMin;   // Minimum galvo position feedback signal expected in [V] for maximal deflection when applying VminOut.
	double					positionVMax;   // Maximum galvo position feedback signal expected in [V] for maximal deflection when applying VmaxOut.
	double*					slope;			// in [V/V], together with offset, it relates the command and position feedback signal in a linear way when the galvo is still.
	double*					offset;			// in [V], together with slope, it relates the command and position feedback signal in a linear way when the galvo is still. 
	double*					posStdDev;		// in [V], measures noise level on the position feedback signal.
	double*					lag;			// in [ms]
	SwitchTimes_type*		switchTimes;
	MaxSlopes_type*			maxSlopes;
	TriangleCal_type*		triangleCal;
	struct {
		double resolution;					// in [V]
		double minStepSize;					// in [V]
		double parked;         				// in [V]
		double scantime;					// in [s]
	}	 					UISet;
} NonResGalvoCal_type;

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

typedef struct ScanEngine ScanEngine_type;
struct ScanEngine {
	//-----------------------------------
	// Scan engine type
	//-----------------------------------
	ScanEngineEnum_type		engineType;
	
	//-----------------------------------
	// Reference to axis calibration data
	//-----------------------------------
	ScanAxisCal_type*		fastAxisCal;			// Pixel index changes faster in this scan direction.
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
		// Position feedback signals (optional for some scan axis types)
	SourceVChan_type*		VChanFastAxisPos;
	SourceVChan_type*		VChanSlowAxisPos;
		// Scan Engine output
	SourceVChan_type*		VChanImgOUT;
		// Detector input channels of DetChan_type* with incoming fluorescence pixel stream 
	ListType				DetChans; 
	
	//-----------------------------------
	// Methods
	//-----------------------------------
	void	(*Discard) (ScanEngine_type** scanEngine);
		
};

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
	// generic scan axis calibration data 
static ScanAxisCal_type*			initalloc_ScanAxisCal_type				(ScanAxisCal_type* scanAxisCal);
static void							discard_ScanAxisCal_type				(ScanAxisCal_type** scanAxisCal);
	//-----------------------------------------
	// Non-resonant galvo axis calibration data
	//-----------------------------------------
static NonResGalvoCal_type*			init_NonResGalvoCal_type				(char calName[], SourceVChan_type* VChanCom, SinkVChan_type* VChanPos);
static void							discard_NonResGalvoCal_type				(ScanAxisCal_type** scanAxisCal);
	// switch times data
static SwitchTimes_type* 			init_SwitchTimes_type					(int nElem);
static void 						discard_SwitchTimes_type 				(SwitchTimes_type** a);
	// max slopes data
static MaxSlopes_type* 				init_MaxSlopes_type 					(int nElem);
static void 						discard_MaxSlopes_type 					(MaxSlopes_type** a);
	// triangle wave calibration data
static TriangleCal_type* 			init_TriangleCal_type					(int nElem);
static void 						discard_TriangleCal_type 				(TriangleCal_type** a);
	// galvo calibration
static int 							CalibrateNonResGalvo					(NonResGalvoCal_type** calData);

//-------------
// Scan Engines
//-------------
	// parent class
static int 							init_ScanEngine_type 					(ScanEngine_type* engine, ScanEngineEnum_type engineType);
static void							discard_ScanEngine_type 				(ScanEngine_type** engine);
	// rectangle raster scan
static void							discard_RectangleRaster_type			(ScanEngine_type** scanEngine);
static int CVICALLBACK 				RectangleRasterScan_CB 					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2); 

//------------------
// Module management
//------------------

static int							Load 									(DAQLabModule_type* mod, int workspacePanHndl);
static int 							DisplayPanels							(DAQLabModule_type* mod, BOOL visibleFlag); 
static int CVICALLBACK 				ScanEngineSettings_CB 					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static int CVICALLBACK 				ScanEnginesTab_CB 							(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static void CVICALLBACK 			SettingsMenu_CB 						(int menuBar, int menuItem, void *callbackData, int panel);
static void CVICALLBACK 			NewScanEngine_CB						(int menuBar, int menuItem, void *callbackData, int panel);



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
		size_t 				nItems = ListNumItems(ls->calScanAxis);
		ScanAxisCal_type**  calPtrPtr;
		for (size_t i = 1; i <= nItems; i++) {
			calPtrPtr = ListGetPtrToItem(ls->calScanAxis, i);
			(*(*calPtrPtr)->Discard)	(calPtrPtr); 
		}
		
		ListDispose(ls->calScanAxis);
	}
	
	if (ls->scanEngines) {
		size_t 				nItems = ListNumItems(ls->scanEngines);
		ScanEngine_type**  	enginePtrPtr;
		for (size_t i = 1; i <= nItems; i++) {
			enginePtrPtr = ListGetPtrToItem(ls->scanEngines, i);
			(*(*enginePtrPtr)->Discard)	(enginePtrPtr);
		}
		
		ListDispose(ls->scanEngines);
	}
	
		//----------------------------------
		// UI
		//----------------------------------
	if (ls->mainPanHndl) {DiscardPanel(ls->mainPanHndl); ls->mainPanHndl = 0;}
	
	//----------------------------------------
	// discard DAQLabModule_type specific data
	//----------------------------------------
	
	discard_DAQLabModule(mod);
}

static int Load (DAQLabModule_type* mod, int workspacePanHndl)
{
	LaserScanning_type*		ls					= (LaserScanning_type*) mod;
	int 					error 				= 0;
	int						menubarHndl			= 0;
	int						settingsMenuItem	= 0;
	int						newMenuItem			= 0;
	
	// load main panel
	errChk(ls->mainPanHndl 		= LoadPanel(workspacePanHndl, MOD_GalvoScanEngine_UI, ScanPan));
	
	// add menu bar to scan panel and link it to module data
	menubarHndl 		= NewMenuBar(ls->mainPanHndl);
	// new menu item
	newMenuItem			= NewMenu(menubarHndl, "New", -1);
	SetMenuBarAttribute(menubarHndl, 0, ATTR_SHOW_IMMEDIATE_ACTION_SYMBOL, 0);
	SetMenuBarAttribute(menubarHndl, newMenuItem, ATTR_CALLBACK_DATA, ls);
	SetMenuBarAttribute(menubarHndl, newMenuItem, ATTR_CALLBACK_FUNCTION_POINTER, NewScanEngine_CB);
	
	// settings menu item
	settingsMenuItem	= NewMenu(menubarHndl, "Settings", -1);
	SetMenuBarAttribute(menubarHndl, 0, ATTR_SHOW_IMMEDIATE_ACTION_SYMBOL, 0);
	SetMenuBarAttribute(menubarHndl, settingsMenuItem, ATTR_CALLBACK_DATA, ls);
	SetMenuBarAttribute(menubarHndl, settingsMenuItem, ATTR_CALLBACK_FUNCTION_POINTER, SettingsMenu_CB);
	
	// add callback function and data to scan engines tab
	SetCtrlAttribute(ls->mainPanHndl, ScanPan_ScanEngines, ATTR_CALLBACK_DATA, ls);
	SetCtrlAttribute(ls->mainPanHndl, ScanPan_ScanEngines, ATTR_CALLBACK_FUNCTION_POINTER, ScanEnginesTab_CB);
	
	return 0;
Error:
	
	if (ls->mainPanHndl) {DiscardPanel(ls->mainPanHndl); ls->mainPanHndl = 0;}
	
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

// adds a new scan engine
static void CVICALLBACK NewScanEngine_CB (int menuBar, int menuItem, void *callbackData, int panel)
{
	LaserScanning_type*		ls = callbackData;
	
	
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

static int CVICALLBACK ScanEnginesTab_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	LaserScanning_type*		ls = callbackData;
	
	// continue only if Del button was pressed on the ScanEngines tab control
	if (control != ScanPan_ScanEngines || event != EVENT_KEYPRESS || eventData1 != VAL_FWD_DELETE_VKEY) return 0;
	
	int 	activeTabIdx;
	int		tabPagePanHndl;
	int		nTabs;
	void*   tabPageDataPtr;
	GetActiveTabPage(panel, control, &activeTabIdx);
	GetPanelHandleFromTabPage(panel, control, activeTabIdx, &tabPagePanHndl);
	GetPanelAttribute(tabPagePanHndl, ATTR_CALLBACK_DATA, &tabPageDataPtr);
	// don't delete this tab page if it's the default "None" page that has no callback data
	if (!tabPageDataPtr) return 0;
	
	// delete scan engine and tab page
	ScanEngine_type**	enginePtrPtr = ListGetPtrToItem(ls->scanEngines, activeTabIdx + 1);
	(*(*enginePtrPtr)->Discard)	(enginePtrPtr);
	ListRemoveItem(ls->scanEngines, 0, activeTabIdx + 1);
	DeleteTabPage(panel, control, activeTabIdx, 1);
	
	// if there are no more scan engines, add default "None" tab page
	GetNumTabPages(panel, control, &nTabs);
	if (!nTabs)
		InsertTabPage(panel, control, -1, "None");
	
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

static ScanAxisCal_type* initalloc_ScanAxisCal_type	(ScanAxisCal_type* scanAxisCal)
{
	// if NULL, allocate memory, otherwise just use the provided address
	if (!scanAxisCal) {
		scanAxisCal = malloc (sizeof(ScanAxisCal_type));
		if (!scanAxisCal) return NULL;
	}
	
	scanAxisCal->calName 		= NULL;
	scanAxisCal->VChanCom		= NULL;
	scanAxisCal->VChanPos		= NULL;
	scanAxisCal->scanAxisType	= NonResonant;
	scanAxisCal->Discard		= NULL;
	
	return scanAxisCal;	
}

static void discard_ScanAxisCal_type (ScanAxisCal_type** scanAxisCal)
{
	OKfree((*scanAxisCal)->calName);
	OKfree(*scanAxisCal);
}

static NonResGalvoCal_type* init_NonResGalvoCal_type (char calName[], SourceVChan_type* VChanCom, SinkVChan_type* VChanPos)
{
	NonResGalvoCal_type*	cal = malloc(sizeof(NonResGalvoCal_type));
	if (!cal) return NULL;
	
	// init parent class
	initalloc_ScanAxisCal_type(&cal->base);
	if(!(cal->base.calName	= StrDup(calName))) {free(cal); return NULL;}
	cal->base.VChanCom		= VChanCom;
	cal->base.VChanPos		= VChanPos;
	cal->base.scanAxisType  = NonResonant;
	cal->base.Discard		= discard_NonResGalvoCal_type; // override
	
	// init NonResGalvoCal_type
	cal->calSet				= NULL;
	cal->commandVMin		= 0;
	cal->commandVMax		= 0;
	cal->positionVMin		= 0;
	cal->positionVMax		= 0;
	cal->slope				= NULL;	// i.e. calibration not performed yet
	cal->offset				= NULL;
	cal->posStdDev			= NULL;
	cal->lag				= NULL;
	cal->switchTimes		= NULL;
	cal->maxSlopes			= NULL;
	cal->triangleCal		= NULL;
	cal->UISet.resolution  	= 0;
	cal->UISet.minstepsize 	= 0;
	cal->UISet.scantime    	= 0;
	cal->UISet.parked		= 0;
	
	return cal;
}

static void	discard_NonResGalvoCal_type	(ScanAxisCal_type** scanAxisCal)
{
	
}

static SwitchTimes_type* init_SwitchTimes_type(int nElem)
{
	SwitchTimes_type* switchT = malloc(sizeof(SwitchTimes_type));
	if (!switchT) return NULL;
	
	switchT->n          	= 0;
	switchT->stepSize   	= NULL;
	switchT->halfSwitch 	= NULL;	
		
	switchT->stepSize	 	= malloc(nElem * sizeof(double));
	if(!switchT->stepSize && nElem) goto Error;
	
	switchT->halfSwitch 	= malloc(nElem * sizeof(double));
	if(!switchT->halfSwitch && nElem) goto Error;
	
	return switchT;
	
Error:
	OKfree(switchT->stepSize);
	OKfree(switchT->halfSwitch);
	OKfree(switchT);
	return NULL;
}

static void discard_SwitchTimes_type (SwitchTimes_type** a)
{
	if (!(*a)) return;
	
	OKfree((*a)->stepSize);
	OKfree((*a)->halfSwitch);
	OKfree(*a);
}

static MaxSlopes_type* init_MaxSlopes_type (int nElem)
{
	MaxSlopes_type* maxSlopes = malloc (sizeof(MaxSlopes_type));
	if (!maxSlopes) return NULL;
	
	maxSlopes->n         	= 0;
	maxSlopes->slope     	= NULL;
	maxSlopes->amplitude 	= NULL;
	
	maxSlopes->slope		= malloc (nElem * sizeof(double));
	if(!maxSlopes->slope && nElem) goto Error;
	
	maxSlopes->amplitude 	= malloc(nElem * sizeof(double));
	if(!maxSlopes->amplitude && nElem) goto Error;
	
	return maxSlopes;
	
Error:
	OKfree(maxSlopes->slope);
	OKfree(maxSlopes->amplitude);
	OKfree(maxSlopes);
	return NULL;
}

static void discard_MaxSlopes_type (MaxSlopes_type** a)
{
	if (!(*a)) return; 
	
	OKfree((*a)->slope);
	OKfree((*a)->amplitude);
	OKfree(*a);
}

static TriangleCal_type* init_TriangleCal_type(int nElem)
{
	TriangleCal_type*  cal = malloc(sizeof(TriangleCal_type));
	if (!cal) return NULL;
	
	// init
	cal->n           	= 0;
	cal->deadTime    	= 0;
	cal->commandAmp 	= NULL;
	cal->actualAmp  	= NULL;
    cal->maxFreq     	= NULL;
	cal->resLag      	= NULL;
	
	cal->commandAmp 	= malloc(nElem * sizeof(double));
	if(!cal->commandAmp && nElem) goto Error;
	
	cal->actualAmp 		= malloc(nElem * sizeof(double));
	if(!cal->actualAmp && nElem) goto Error;
	
	cal->maxFreq 		= malloc(nElem * sizeof(double));
	if(!cal->maxFreq && nElem) goto Error;
	
	cal->resLag 		= malloc(nElem * sizeof(double));
	if(!cal->resLag && nElem) goto Error;
	
	return cal;	
	
Error:
	OKfree(cal->maxFreq);
	OKfree(cal->actualAmp);
	OKfree(cal->commandAmp);
	OKfree(cal->resLag);
	OKfree(cal);
	return NULL;
}

static void discard_TriangleCal_type (TriangleCal_type** a)
{
	if (!(*a)) goto DoNothing; 
	OKfree((*a) -> command_amp);
	OKfree((*a) -> actual_amp);
	OKfree((*a) -> maxfreq);
	OKfree((*a) -> reslag);
	OKfree(*a);
	
	DoNothing:
	return;
}

static int init_ScanEngine_type (ScanEngine_type* engine, ScanEngineEnum_type engineType)
{
	// VChans
	engine->engineType				= engineType;
	engine->VChanFastAxisCom		= NULL;
	engine->VChanSlowAxisCom		= NULL;
	engine->VChanFastAxisPos		= NULL;
	engine->VChanSlowAxisPos		= NULL;
	engine->VChanImgOUT				= NULL;
	if(!(engine->DetChans			= ListCreate(sizeof(DetChan_type*)))) goto Error;
	
	// reference to axis calibration
	engine->fastAxisCal				= NULL;
	engine->slowAxisCal				= NULL;
	
	// task controller
	engine->taskControl				= NULL;
	
	// position feedback flags
	engine->Discard					= NULL;
	
	return 0;
Error:
	return -1;
}

static void	discard_ScanEngine_type (ScanEngine_type** scanEngine)
{
	ScanEngine_type* engine = *scanEngine;
	
	//----------------------------------
	// VChans
	//----------------------------------
	// fast axis command
	if (engine->VChanFastAxisCom) {
		DLUnregisterVChan((VChan_type*)engine->VChanFastAxisCom);
		discard_VChan_type((VChan_type**)&engine->VChanFastAxisCom);
	}
	
	// slow axis command
	if (engine->VChanSlowAxisCom) {
		DLUnregisterVChan((VChan_type*)engine->VChanSlowAxisCom);
		discard_VChan_type((VChan_type**)&engine->VChanSlowAxisCom);
	}
	
	// fast axis position feedback
	if (engine->VChanFastAxisPos) {
		DLUnregisterVChan((VChan_type*)engine->VChanFastAxisPos);
		discard_VChan_type((VChan_type**)&engine->VChanFastAxisPos);
	}
	
	// slow axis position feedback
	if (engine->VChanSlowAxisPos) {
		DLUnregisterVChan((VChan_type*)engine->VChanSlowAxisPos);
		discard_VChan_type((VChan_type**)&engine->VChanSlowAxisPos);
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
	// Task controller
	//----------------------------------
	if (engine->taskControl) {
		ListType tcList = ListCreate(sizeof(TaskControl_type*));
		ListInsertItem(tcList, &engine->taskControl, END_OF_LIST);
		DLRemoveTaskControllers(tcList); 
		ListDispose(tcList);
		discard_TaskControl_type(engine->taskControl);
	}
	
	OKfree(scanEngine);
}

static int CVICALLBACK RectangleRasterScan_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
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
