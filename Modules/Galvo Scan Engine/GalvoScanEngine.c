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

//==============================================================================
// Types

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
typedef struct ScanEngine 	ScanEngine_type;
struct ScanEngine {
	
	// SUPER, must be the first member to inherit from
	
	DAQLabModule_type 	baseClass;
	
	// DATA
	
		// Galvo command and position feedback
	SinkVChan_type*		VChanXCom;
	SourceVChan_type*	VChanXPos;
	SinkVChan_type*		VChanYCom;
	SourceVChan_type*	VChanYPos;
		// Scan Engine output
	SourceVChan_type*	VChanScanEngineOUT;
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

//----------------
// VChan Callbacks
//----------------
	// detection VChans
static void							DetVChanConnected						(VChan_type* self, VChan_type* connectedVChan);
static void							DetVChanDisconnected					(VChan_type* self, VChan_type* disconnectedVChan); 

//------------------
// Module management
//------------------

static int							Load 									(DAQLabModule_type* mod, int workspacePanHndl);

//==============================================================================
// Global variables

//==============================================================================
// Global functions


/// HIFN  Allocates memory and initializes a Galvo Scan Engine for laser scanning microscopy.
/// HIPAR mod/ Pass NULL if both memory allocation and initialization must take place.
/// HIPAR mod/ Pass !NULL if the function performs only an initialization of a DAQLabModule_type.
/// HIRET address of a DAQLabModule_type if memory allocation and initialization took place.
/// HIRET NULL if only initialization was performed.
DAQLabModule_type*	initalloc_GalvoScanEngine (DAQLabModule_type* mod, char className[], char instanceName[])
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
	engine->baseClass.Discard 		= discard_GalvoScanEngine;
			
	engine->baseClass.Load			= Load; 
	
	engine->baseClass.DisplayPanels	= DisplayPanels;
			
	//---------------------------
	// Child Level 1: ScanEngine_type
	
		// DATA
	engine->DetChans				= ListCreate(sizeof(DetChan_type*));
	if (!engine->DetChans) {discard_GalvoScanEngine((DAQLabModule_type**)&engine); return NULL;}
	
	engine->mainPanHndl				= 0;
	engine->scanSetPanHndl			= 0;
	
		// METHODS
		
	//----------------------------------------------------------
	if (!mod)
		return (DAQLabModule_type*) engine;
	else
		return NULL;
	
}

void discard_GalvoScanEngine (DAQLabModule_type** mod)
{
	ScanEngine_type*		engine		= (ScanEngine_type*) (*mod);
	
	if (!engine) return;
	
	//--------------------------------------
	// discard ScanEngine_type specific data
	//--------------------------------------
	// data
	size_t	nDet = ListNumItems(engine->DetChans);
	DetChan_type** detChanPtrPtr;
	for (size_t i = 1; i <= nDet; i++) {
		detChanPtrPtr = ListGetPtrToItem(engine->DetChans, i);
		// remove VChan from framework
		DLUnregisterVChan((*detChanPtrPtr)->detVChan);
		
		discard_DetChan_type(detChanPtrPtr);
	}
	
	// UI
	if (engine->mainPanHndl) {DiscardPanel(engine->mainPanHndl); engine->mainPanHndl = 0;}
	if (engine->scanSetPanHndl) {DiscardPanel(engine->scanSetPanHndl); engine->scanSetPanHndl = 0;}
	
	//----------------------------------------
	// discard DAQLabModule_type specific data
	//----------------------------------------
	
	discard_DAQLabModule(mod);
}

static int Load (DAQLabModule_type* mod, int workspacePanHndl)
{
	int 				error 		= 0;
	ScanEngine_type*	engine		= (ScanEngine_type*) mod;
	
	// main panel
	errChk(engine->mainPanHndl = LoadPanel(workspacePanHndl, MOD_GalvoScanEngine_UI, ScanPan));
	
	return 0;
Error:
	
	if (engine->mainPanHndl) {DiscardPanel(engine->mainPanHndl); engine->mainPanHndl = 0;}
	
	return error;
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
	DLRegisterVChan(det->detVChan);
	
	det->scanEngineRef	= scanEnginePtr;
	
	return det;
}

static void	discard_DetChan_type (DetChan_type** a)
{
	if (!*a) return;
	
	discard_VChan_type(&(*a)->detVChan);
	
	OKfree(*a);
}

// detection VChans
static void	DetVChanConnected (VChan_type* self, VChan_type* connectedVChan)
{
	
}

static void	DetVChanDisconnected (VChan_type* self, VChan_type* disconnectedVChan)
{
	
}


