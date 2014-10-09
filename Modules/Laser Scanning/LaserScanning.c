
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
#define DAQmxErrChk(functionCall) if( DAQmxFailed(error=(functionCall)) ) goto Error; else 

#define MOD_LaserScanning_UI 					"./Modules/Laser Scanning/UI_LaserScanning.uir"
// Default VChan base names. Default base names must be unique among each other!
#define VChan_Default_FastAxis_Command			"Fast Axis Command"
#define VChan_Default_FastAxis_Position			"Fast Axis Position" 
#define VChan_Default_SlowAxis_Command			"Slow Axis Command"
#define VChan_Default_SlowAxis_Position			"Slow Axis Position"
#define VChan_Default_ImageOut					"Image"
#define VChan_Default_DetectionChan				"Detector"
#define AxisCal_Default_TaskController_Name		"Scan Axis Calibration"
#define AxisCal_Default_NewCalibration_Name		"Calibration"

// scan engine settings

#define Max_NewScanEngine_NameLength	50

// non-resonant galvo calibration parameters
#define OKfree(ptr) if (ptr) { free(ptr); ptr = NULL; }
#define CALPOINTS 									50
#define RELERR 										0.05
#define AIREADS 									200 		// number of AI samples to read and average for one measurement of the galvo position 
#define FUNC_Triangle 								0
#define FUNC_Sawtooth 								1
#define FIND_MAXSLOPES_AMP_ITER_FACTOR 				0.9
#define SWITCH_TIMES_AMP_ITER_FACTOR 				0.95 
#define DYNAMICCAL_SLOPE_ITER_FACTOR 				0.90
#define DYNAMICCAL_INITIAL_SLOPE_REDUCTION_FACTOR 	0.8	 		// for each amplitude, the initial maxslope value is reduced by this factor
#define MAX_SLOPE_REDUCTION_FACTOR 					0.25		// position lag is measured with quarter the maximum ramp to allow the galvo reach a steady state
#define VAL_OVLD_ERR -8000

//==============================================================================
// Types

	// forward declared
typedef struct LaserScanning 	LaserScanning_type; 

//------------------------------
// Generic scan axis calibration
//------------------------------

// Scan axis types
typedef enum {
	NonResonantGalvo,								// Normal non-resonant deflectors (common galvos).
	ResonantGalvo,									// Resonant deflectors.
	AOD,											// Acousto-optic deflector.
	Translation										// Translation type scanning axis such as a moving stage.
} ScanAxis_type;

// Generic scan axis class
typedef struct ScanAxisCal ScanAxisCal_type;
struct ScanAxisCal {
	// DATA
	ScanAxis_type			scanAxisType;
	char*					calName;
	TaskControl_type*		taskController;			// Task Controller for the calibration of this scan axis.
	SourceVChan_type*		VChanCom;   			// VChan used to send command signals to the scan axis (optional, depending on the scan axis type)
	SinkVChan_type*			VChanPos;				// VChan used to receive position feedback signals from the scan axis (optional, depending on the scan axis type)
	double*					comSampRate;			// Command sample rate in [Hz] taken from VChanCom when connected.
	double*					posSampRate;			// Position sample rate in [Hz] taken from VChanPos when connected.
	int						calPanHndl;				// Panel handle for performing scan axis calibration
	LaserScanning_type*		lsModule;				// Reference to the laser scanning module to which this calibration belongs.
	// METHODS
	void	(*Discard) (ScanAxisCal_type** scanAxisCal);
};

//---------------------------------------------------
// Normal non-resonant galvo scanner calibration data
//---------------------------------------------------
typedef struct{
	size_t 					n;						// Number of steps.
	double* 				stepSize;  				// Amplitude in [V] Pk-Pk of the command signal.
	double* 				halfSwitch;				// Time to reach 50% of the stepsize voltage in [ms].  
} SwitchTimes_type;

typedef struct{
	size_t 					n;						// Number of amplitudes for whih the slope is measured.
	double* 				slope; 					// Maximum slope in [V/ms] of the command signal that the galvo can still follow given a certain ramp amplitude.
	double* 				amplitude;				// Amplitude in [V] Pk-Pk of the command signal.	
} MaxSlopes_type;

typedef struct{
	size_t 					n;						// Number of triangle waveform amplitudes.
	double 					deadTime;				// Dead time in [ms] before and at the end of each line scan when the galvo is not moving at constant speed (due to turning around).
													// This time is about the same for all scan frequencies and amplitudes that the galvo can follow.
	double* 				commandAmp;				// Amplitude in [V] Pk-Pk of the command signal.
	double* 				actualAmp;				// Amplitude in [V] Pk-Pk where the galvo motion is constant.
	double* 				maxFreq;				// For given amplitude, maximum triangle wave frequency that the galvo can still follow for at least 1 sec.
													// Note: the line scan frequency is twice this frequency
	double* 				resLag;					// Residual lag in [ms] that must be added to lag to accurately describe overall lag during dynamic scanning.
} TriangleCal_type;

typedef enum {
	
	NonResGalvoCal_Slope_Offset_Lag,
	NonResGalvoCal_SwitchTimes,
	NonResGalvoCal_MaxSlopes,
	NonResGalvoCal_TriangleWave
	
} NonResGalvoCalTypes;
typedef struct {
	ScanAxisCal_type		baseClass;				// Base structure describing scan engine calibration. Must be first member of the structure.
	size_t					calIterations[4];		// number of Task Controller iterations for each calibration mode: 
													// 1) Slope, Offset & Lag, 2) Switch Times, 3) Max Slopes, 4) Triangle Wave.
	NonResGalvoCalTypes		currentCal;				// Index of the current calibration
	size_t					currIterIdx;			// Current iteration index for each calibration method.
	double					commandVMin;			// Minimum galvo command voltage in [V] for maximal deflection in a given direction.
	double					commandVMax;			// Maximum galvo command voltage in [V] for maximal deflection in the oposite direction when applying VminOut.
	double					positionVMin;   		// Minimum galvo position feedback signal expected in [V] for maximal deflection when applying VminOut.
	double					positionVMax;   		// Maximum galvo position feedback signal expected in [V] for maximal deflection when applying VmaxOut.
	double*					slope;					// in [V/V], together with offset, it relates the command and position feedback signal in a linear way when the galvo is still.
	double*					offset;					// in [V], together with slope, it relates the command and position feedback signal in a linear way when the galvo is still. 
	double*					posStdDev;				// in [V], measures noise level on the position feedback signal.
	double*					lag;					// in [ms]
	SwitchTimes_type*		switchTimes;
	MaxSlopes_type*			maxSlopes;
	TriangleCal_type*		triangleCal;
	struct {
		double resolution;							// in [V]
		double minStepSize;							// in [V]
		double parked;         						// in [V]
		double scanTime;							// in [s]
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
	
	//---------------------------------------
	// Reference to the Laser Scanning module
	//---------------------------------------
	LaserScanning_type*		lsModule;
	
	//-----------------------------------
	// Task Controller
	//-----------------------------------
	TaskControl_type*		taskControl;
	
	//-----------------------------------
	// VChans
	//-----------------------------------
		// Command signals
	SourceVChan_type*		VChanFastAxisCom;   
	SourceVChan_type*		VChanSlowAxisCom;   
		// Position feedback signals (optional for some scan axis types)
	SinkVChan_type*			VChanFastAxisPos;
	SinkVChan_type*			VChanSlowAxisPos;
		// Scan Engine output
	SourceVChan_type*		VChanImageOut;
		// Detector input channels of DetChan_type* with incoming fluorescence pixel stream 
	ListType				DetChans; 
	
	//-----------------------------------
	// UI
	//-----------------------------------
	int						scanSetPanHndl;			// Panel handle for adjusting scan settings such as pixel size, etc...
	int						engineSetPanHndl;		// Panel handle for scan engine settings such as VChans and scan axis types.
	
	//-----------------------------------
	// Methods
	//-----------------------------------
	void	(*Discard) (ScanEngine_type** scanEngine);
		
};

//------------------------
// Rectangular raster scan
//------------------------
typedef struct {
	ScanEngine_type			baseClass;					// Base class, must be first structure member.
	
	//----------------
	// Scan parameters
	//----------------
	double*					fastAxisSamplingRate;   // Pointer to fast axis waveform sampling rate in [Hz]. Value taken from VChanFastAxisCom
	double*					slowAxisSamplingRate;	// Pointer to slow axis waveform sampling rate in [Hz]. Value taken from VChanSlowAxisCom
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
	
		// Available calibration data per scan axis. Of ScanAxisCal_type* which can be casted to various child classes such as GalvoCal_type*
	ListType				availableCals;
		// Active calibrations that are in progress. Of ScanAxisCal_type* which can be casted to various child classes such as GalvoCal_type*
	ListType				activeCal;
		// Scan engines of ScanEngine_type* base class which can be casted to specific scan engines. 
	ListType				scanEngines;
	
		//-------------------------
		// UI
		//-------------------------
	
	int						mainPanHndl;	  			// Main panel for the laser scanning module.
	int						enginesPanHndl;   			// List of available scan engine types.
	int						manageAxisCalPanHndl;		// Panel handle where axis calibrations are managed.
	int						newAxisCalTypePanHndl;		// Panel handle to select different calibrations for various axis types.
	int						menuBarHndl;				// Laser scanning module menu bar.
	int						engineSettingsMenuItemHndl;	// Settings menu item handle for adjusting scan engine settings such as VChans, etc.
	
};

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//-------------------
// Detection channels
//-------------------
static DetChan_type*				init_DetChan_type						(char VChanName[]);

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
	// generic scan axis calibration data 

static ScanAxisCal_type*			initalloc_ScanAxisCal_type				(ScanAxisCal_type* cal);

static void							discard_ScanAxisCal_type				(ScanAxisCal_type** cal);

void CVICALLBACK 					ScanAxisCalibrationMenu_CB				(int menuBarHandle, int menuItemID, void *callbackData, int panelHandle);

static void							UpdateAvailableCalibrations				(ScanEngine_type* scanEngine, ScanAxis_type axisType);

static int CVICALLBACK 				ManageScanAxisCalib_CB					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 				NewScanAxisCalib_CB						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

	// galvo response analysis functions

static int 							FindSlope								(double* corrpos, int nsamples, double samplingRate, double signalStdDev, int nrep, double sloperelerr, double* maxslope); 
static int 							MeasureLag								(double* signal1, double* signal2, int nelem); 
static int 							GetStepTimes							(double* signal, int nsamples, double lowlevel, double highlevel, double threshold, int* lowidx, int* highidx, int* stepsamples);

	//-----------------------------------------
	// Non-resonant galvo axis calibration data
	//-----------------------------------------

static NonResGalvoCal_type* 		init_NonResGalvoCal_type 				(LaserScanning_type* lsModule, char calName[], char commandVChanName[], char positionVChanName[]);

static void							discard_NonResGalvoCal_type				(ScanAxisCal_type** cal);

	// switch times data
static SwitchTimes_type* 			init_SwitchTimes_type					(int nElem);

static void 						discard_SwitchTimes_type 				(SwitchTimes_type** a);

	// max slopes data

static MaxSlopes_type* 				init_MaxSlopes_type 					(int nElem);

static void 						discard_MaxSlopes_type 					(MaxSlopes_type** a);

	// triangle wave calibration data

static TriangleCal_type* 			init_TriangleCal_type					(int nElem);

static void 						discard_TriangleCal_type 				(TriangleCal_type** a);

	// command VChan
static void							NonResGalvoCal_ComVChan_Connected		(VChan_type* self, VChan_type* connectedVChan);
static void							NonResGalvoCal_ComVChan_Disconnected	(VChan_type* self, VChan_type* disconnectedVChan);

	// position VChan
static void							NonResGalvoCal_PosVChan_Connected		(VChan_type* self, VChan_type* connectedVChan);
static void							NonResGalvoCal_PosVChan_Disconnected	(VChan_type* self, VChan_type* disconnectedVChan);

	// galvo calibration
static int CVICALLBACK 				NonResGalvoCal_MainPan_CB				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static int CVICALLBACK 				NonResGalvoCal_CalPan_CB				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static int CVICALLBACK 				NonResGalvoCal_TestPan_CB				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

//-------------
// Scan Engines
//-------------
	// parent class
static int 							init_ScanEngine_type 					(ScanEngine_type* 		engine,
																			 LaserScanning_type*	lsModule,
																			 ScanEngineEnum_type 	engineType,
																			 char 					fastAxisComVChanName[], 
								 											 char 					slowAxisComVChanName[], 
								 											 char					fastAxisPosVChanName[],
								 											 char					slowAxisPosVChanName[],
								 											 char					imageOutVChanName[]);

static void							discard_ScanEngine_type 				(ScanEngine_type** engine);

	// rectangle raster scan

static RectangleRaster_type*		init_RectangleRaster_type				(LaserScanning_type*	lsModule, 
																			 char 					engineName[],
														   					 char 					fastAxisComVChanName[], 
								 											 char 					slowAxisComVChanName[], 
								 											 char					fastAxisPosVChanName[],
								 											 char					slowAxisPosVChanName[],
								 											 char					imageOutVChanName[]);

static void							discard_RectangleRaster_type			(ScanEngine_type** engine);

static int CVICALLBACK 				RectangleRasterScan_CB 					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2); 

//------------------
// Module management
//------------------

static int							Load 									(DAQLabModule_type* mod, int workspacePanHndl);

static int 							DisplayPanels							(DAQLabModule_type* mod, BOOL visibleFlag); 

static int CVICALLBACK 				ScanEngineSettings_CB 					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 				ScanEnginesTab_CB 						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

void CVICALLBACK 					ScanEngineSettingsMenu_CB				(int menuBarHandle, int menuItemID, void *callbackData, int panelHandle);


static void CVICALLBACK 			NewScanEngineMenu_CB					(int menuBar, int menuItem, void *callbackData, int panel);

static int CVICALLBACK 				NewScanEngine_CB 						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static BOOL							ValidScanEngineName						(char name[], void* dataPtr);

//-----------------------------------------
// Scan Engine VChan management
//-----------------------------------------

	// Fast Axis Command VChan
static void							FastAxisComVChan_Connected				(VChan_type* self, VChan_type* connectedVChan);
static void							FastAxisComVChan_Disconnected			(VChan_type* self, VChan_type* disconnectedVChan);

	// Slow Axis Command VChan
static void							SlowAxisComVChan_Connected				(VChan_type* self, VChan_type* connectedVChan);
static void							SlowAxisComVChan_Disconnected			(VChan_type* self, VChan_type* disconnectedVChan);

	// Fast Axis Position VChan
static void							FastAxisPosVChan_Connected				(VChan_type* self, VChan_type* connectedVChan);
static void							FastAxisPosVChan_Disconnected			(VChan_type* self, VChan_type* disconnectedVChan);

	// Slow Axis Position VChan
static void							SlowAxisPosVChan_Connected				(VChan_type* self, VChan_type* connectedVChan);
static void							SlowAxisPosVChan_Disconnected			(VChan_type* self, VChan_type* disconnectedVChan);

	// Image Out VChan
static void							ImageOutVChan_Connected					(VChan_type* self, VChan_type* connectedVChan);
static void							ImageOutVChan_Disconnected				(VChan_type* self, VChan_type* disconnectedVChan);

	// Detection VChan
static void							detVChan_Connected						(VChan_type* self, VChan_type* connectedVChan);
static void							detVChan_Disconnected					(VChan_type* self, VChan_type* disconnectedVChan);


//-----------------------------------------
// Task Controller Callbacks
//-----------------------------------------

	// for Non Resonant Galvo scan axis calibration and testing
static FCallReturn_type*			ConfigureTC_NonResGalvoCal				(TaskControl_type* taskControl, BOOL const* abortFlag);

static void							IterateTC_NonResGalvoCal				(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortIterationFlag);

static void							AbortIterationTC_NonResGalvoCal			(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);

static FCallReturn_type*			StartTC_NonResGalvoCal					(TaskControl_type* taskControl, BOOL const* abortFlag);

static FCallReturn_type*			DoneTC_NonResGalvoCal					(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);

static FCallReturn_type*			StoppedTC_NonResGalvoCal				(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);

static FCallReturn_type* 			ResetTC_NonResGalvoCal 					(TaskControl_type* taskControl, BOOL const* abortFlag);

static void							DimTC_NonResGalvoCal					(TaskControl_type* taskControl, BOOL dimmed);

static FCallReturn_type* 			DataReceivedTC_NonResGalvoCal 			(TaskControl_type* taskControl, TaskStates_type taskState, SinkVChan_type* sinkVChan, BOOL const* abortFlag);

static void 						ErrorTC_NonResGalvoCal 					(TaskControl_type* taskControl, char* errorMsg);

static FCallReturn_type*			ModuleEventHandler_NonResGalvoCal		(TaskControl_type* taskControl, TaskStates_type taskState, size_t currentIteration, void* eventData, BOOL const* abortFlag);
	
	// for RectangleRaster_type scanning
static FCallReturn_type*			ConfigureTC_RectRaster					(TaskControl_type* taskControl, BOOL const* abortFlag);

static void							IterateTC_RectRaster					(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortIterationFlag);

static void							AbortIterationTC_RectRaster				(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);

static FCallReturn_type*			StartTC_RectRaster						(TaskControl_type* taskControl, BOOL const* abortFlag);

static FCallReturn_type*			DoneTC_RectRaster						(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);

static FCallReturn_type*			StoppedTC_RectRaster					(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);

static FCallReturn_type* 			ResetTC_RectRaster 						(TaskControl_type* taskControl, BOOL const* abortFlag);

static void							DimTC_RectRaster						(TaskControl_type* taskControl, BOOL dimmed);

static FCallReturn_type* 			DataReceivedTC_RectRaster 				(TaskControl_type* taskControl, TaskStates_type taskState, SinkVChan_type* sinkVChan, BOOL const* abortFlag);

static void 						ErrorTC_RectRaster 						(TaskControl_type* taskControl, char* errorMsg);

static FCallReturn_type*			ModuleEventHandler_RectRaster			(TaskControl_type* taskControl, TaskStates_type taskState, size_t currentIteration, void* eventData, BOOL const* abortFlag);

	// add below more task controller callbacks for different scan engine types

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
	ls->availableCals				= 0;
	ls->activeCal					= 0;
	ls->scanEngines					= 0;
		
	if (!(ls->availableCals			= ListCreate(sizeof(ScanAxisCal_type*))))	goto Error;
	if (!(ls->activeCal				= ListCreate(sizeof(ScanAxisCal_type*))))	goto Error;
	if (!(ls->scanEngines			= ListCreate(sizeof(ScanEngine_type*))))	goto Error;
			
		//---
		// UI
		//---
	ls->mainPanHndl					= 0;
	ls->enginesPanHndl				= 0;
	ls->newAxisCalTypePanHndl		= 0;
	ls->manageAxisCalPanHndl		= 0;		
	ls->menuBarHndl					= 0;
	ls->engineSettingsMenuItemHndl	= 0;
	
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
	
	if (ls->availableCals) {
		size_t 				nItems = ListNumItems(ls->availableCals);
		ScanAxisCal_type**  calPtr;
		for (size_t i = 1; i <= nItems; i++) {
			calPtr = ListGetPtrToItem(ls->availableCals, i);
			(*(*calPtr)->Discard)	(calPtr); 
		}
		
		ListDispose(ls->availableCals);
	}
	
	if (ls->activeCal) {
		size_t 				nItems = ListNumItems(ls->activeCal);
		ScanAxisCal_type**  calPtr;
		for (size_t i = 1; i <= nItems; i++) {
			calPtr = ListGetPtrToItem(ls->activeCal, i);
			(*(*calPtr)->Discard)	(calPtr); 
		}
		
		ListDispose(ls->activeCal);
	}
	
	if (ls->scanEngines) {
		size_t 				nItems = ListNumItems(ls->scanEngines);
		ScanEngine_type**  	enginePtr;
		for (size_t i = 1; i <= nItems; i++) {
			enginePtr = ListGetPtrToItem(ls->scanEngines, i);
			(*(*enginePtr)->Discard)	(enginePtr);
		}
		
		ListDispose(ls->scanEngines);
	}
	
		//----------------------------------
		// UI
		//----------------------------------
	if (ls->mainPanHndl) {DiscardPanel(ls->mainPanHndl); ls->mainPanHndl = 0;}
	if (ls->enginesPanHndl) {DiscardPanel(ls->enginesPanHndl); ls->enginesPanHndl = 0;}
	if (ls->manageAxisCalPanHndl) {DiscardPanel(ls->manageAxisCalPanHndl); ls->manageAxisCalPanHndl = 0;}
	if (ls->newAxisCalTypePanHndl) {DiscardPanel(ls->newAxisCalTypePanHndl); ls->newAxisCalTypePanHndl = 0;}
	
	//----------------------------------------
	// discard DAQLabModule_type specific data
	//----------------------------------------
	
	discard_DAQLabModule(mod);
}

static int Load (DAQLabModule_type* mod, int workspacePanHndl)
{
	LaserScanning_type*		ls					= (LaserScanning_type*) mod;
	int 					error 				= 0;
	int						newMenuItem			= 0;
	int						settingsMenuItemHndl;
	
	// load main panel
	errChk(ls->mainPanHndl 		= LoadPanel(workspacePanHndl, MOD_LaserScanning_UI, ScanPan));
	
	// add menu bar to scan panel and link it to module data
	ls->menuBarHndl 			= NewMenuBar(ls->mainPanHndl);
	// new menu item
	newMenuItem					= NewMenu(ls->menuBarHndl, "New", -1);
	SetMenuBarAttribute(ls->menuBarHndl, 0, ATTR_SHOW_IMMEDIATE_ACTION_SYMBOL, 0);
	SetMenuBarAttribute(ls->menuBarHndl, newMenuItem, ATTR_CALLBACK_DATA, ls);
	SetMenuBarAttribute(ls->menuBarHndl, newMenuItem, ATTR_CALLBACK_FUNCTION_POINTER, NewScanEngineMenu_CB);
	
	// settings menu
	settingsMenuItemHndl 		= NewMenu(ls->menuBarHndl, "Settings", -1);
	// scan engine settings menu item
	ls->engineSettingsMenuItemHndl = NewMenuItem(ls->menuBarHndl, settingsMenuItemHndl, "Scan engine", -1, (VAL_MENUKEY_MODIFIER | 'S'), ScanEngineSettingsMenu_CB, ls);  
	SetMenuBarAttribute(ls->menuBarHndl, ls->engineSettingsMenuItemHndl, ATTR_DIMMED, 1);
	// axis calibration settings menu item
	NewMenuItem(ls->menuBarHndl, settingsMenuItemHndl, "Axis calibration", -1, (VAL_MENUKEY_MODIFIER | 'A'), ScanAxisCalibrationMenu_CB, ls);  
	
	// add callback function and data to scan engines tab
	SetCtrlAttribute(ls->mainPanHndl, ScanPan_ScanEngines, ATTR_CALLBACK_DATA, ls);
	SetCtrlAttribute(ls->mainPanHndl, ScanPan_ScanEngines, ATTR_CALLBACK_FUNCTION_POINTER, ScanEnginesTab_CB);
	
	// change main module panel title to module instance name
	SetPanelAttribute(ls->mainPanHndl, ATTR_TITLE, mod->instanceName);
	
	return 0;
Error:
	
	if (ls->mainPanHndl) {DiscardPanel(ls->mainPanHndl); ls->mainPanHndl = 0;}
	
	return error;
}

static int DisplayPanels (DAQLabModule_type* mod, BOOL visibleFlag)
{
	LaserScanning_type*		ls				= (LaserScanning_type*) mod;
	int						error			= 0;
	size_t					nEngines		= ListNumItems(ls->scanEngines);
	size_t					nActiveCals		= ListNumItems(ls->activeCal);
	size_t					nAvailableCals	= ListNumItems(ls->availableCals);
	ScanEngine_type**		enginePtr;
	ScanAxisCal_type**		calPtr;
	
	
	// laser scanning module panels
	if (ls->mainPanHndl)
		errChk(SetPanelAttribute(ls->mainPanHndl, ATTR_VISIBLE, visibleFlag));
	
	if (ls->enginesPanHndl)
		errChk(SetPanelAttribute(ls->enginesPanHndl, ATTR_VISIBLE, visibleFlag));
	
	if (ls->manageAxisCalPanHndl)
		errChk(SetPanelAttribute(ls->manageAxisCalPanHndl, ATTR_VISIBLE, visibleFlag));
	
	// scan engine settings panels
	for(size_t i = 1; i <= nEngines; i++) {
		enginePtr = ListGetPtrToItem(ls->scanEngines, i);
		
		if ((*enginePtr)->engineSetPanHndl)
			errChk(SetPanelAttribute((*enginePtr)->engineSetPanHndl, ATTR_VISIBLE, visibleFlag));
	}
	
	// active scan axis calibration panels
	for(size_t i = 1; i <= nActiveCals; i++) {
		calPtr = ListGetPtrToItem(ls->activeCal, i);
		
		if ((*calPtr)->calPanHndl)
			errChk(SetPanelAttribute((*calPtr)->calPanHndl, ATTR_VISIBLE, visibleFlag));
	}
	
	// available scan axis calibration panels
	for(size_t i = 1; i <= nAvailableCals; i++) {
		calPtr = ListGetPtrToItem(ls->availableCals, i);
		
		if ((*calPtr)->calPanHndl)
			errChk(SetPanelAttribute((*calPtr)->calPanHndl, ATTR_VISIBLE, visibleFlag));
	}
	
	
Error:
	return error;	
}

void CVICALLBACK ScanEngineSettingsMenu_CB (int menuBarHandle, int menuItemID, void *callbackData, int panelHandle)
{
	LaserScanning_type*		ls					= callbackData;
	ScanEngine_type**		enginePtr;
	ScanEngine_type*		engine;
	int						workspacePanHndl;
	int						tabIdx;
	
	// get pointer to selected scan engine
	GetActiveTabPage(panelHandle, ScanPan_ScanEngines, &tabIdx);
	enginePtr = ListGetPtrToItem(ls->scanEngines, tabIdx+1);
	engine = *enginePtr;
	
	// load settings panel if not loaded already
	GetPanelAttribute(panelHandle, ATTR_PANEL_PARENT, &workspacePanHndl);
	if (engine->engineSetPanHndl) return; // do nothing
	
	engine->engineSetPanHndl 	= LoadPanel(workspacePanHndl, MOD_LaserScanning_UI, ScanSetPan);
	// change panel title
	char* panTitle = GetTaskControlName(engine->taskControl);
	AppendString(&panTitle, " settings", -1);
	SetPanelAttribute(engine->engineSetPanHndl, ATTR_TITLE, panTitle);
	OKfree(panTitle);
	
	char* VChanFastAxisComName 	= GetVChanName((VChan_type*)engine->VChanFastAxisCom);
	char* VChanSlowAxisComName 	= GetVChanName((VChan_type*)engine->VChanSlowAxisCom);
	char* VChanFastAxisPosName 	= GetVChanName((VChan_type*)engine->VChanFastAxisPos);
	char* VChanSlowAxisPosName 	= GetVChanName((VChan_type*)engine->VChanSlowAxisPos);
	char* VChanImageOutName 	= GetVChanName((VChan_type*)engine->VChanImageOut); 
	
	// update VChans from data structure
	if (engine->VChanFastAxisCom)
		SetCtrlVal(engine->engineSetPanHndl, ScanSetPan_FastAxisCommand, VChanFastAxisComName);
	
	if (engine->VChanSlowAxisCom)
		SetCtrlVal(engine->engineSetPanHndl, ScanSetPan_SlowAxisCommand, VChanSlowAxisComName);
	
	if (engine->VChanFastAxisPos)
		SetCtrlVal(engine->engineSetPanHndl, ScanSetPan_FastAxisPosition, VChanFastAxisPosName);
	
	if (engine->VChanSlowAxisPos)
		SetCtrlVal(engine->engineSetPanHndl, ScanSetPan_SlowAxisPosition, VChanSlowAxisPosName);
		
	if (engine->VChanImageOut)
		SetCtrlVal(engine->engineSetPanHndl, ScanSetPan_ImageOut, VChanImageOutName);
	
	size_t 				nImgChans 		= ListNumItems(engine->DetChans);
	DetChan_type** 		detChanPtrPtr;
	char*				VChanDetName;
	for (size_t i = 1; i <= nImgChans; i++) {
		detChanPtrPtr = ListGetPtrToItem(engine->DetChans, i);
		VChanDetName = GetVChanName((VChan_type*)(*detChanPtrPtr)->detVChan);
		InsertListItem(engine->engineSetPanHndl, ScanSetPan_ImgChans, -1, VChanDetName, VChanDetName);
		OKfree(VChanDetName);
	}
	
	// add callback to all controls in the panel
	SetCtrlsInPanCBInfo(engine, ScanEngineSettings_CB, engine->engineSetPanHndl); 
	
	//-------------------------------
	// populate fast axis scan types
	//-------------------------------
	InsertListItem(engine->engineSetPanHndl, ScanSetPan_FastAxisType, -1, "Non-resonant galvo", NonResonantGalvo);
	InsertListItem(engine->engineSetPanHndl, ScanSetPan_FastAxisType, -1, "Resonant galvo", ResonantGalvo);
	InsertListItem(engine->engineSetPanHndl, ScanSetPan_FastAxisType, -1, "Acousto-optic deflector", AOD);
	InsertListItem(engine->engineSetPanHndl, ScanSetPan_FastAxisType, -1, "Translation stage", Translation);
	// select by default NonResonantGalvo
	SetCtrlIndex(engine->engineSetPanHndl, ScanSetPan_FastAxisType, 0);
	//-------------------------------
	// populate slow axis scan types
	//-------------------------------
	InsertListItem(engine->engineSetPanHndl, ScanSetPan_SlowAxisType, -1, "Non-resonant galvo", NonResonantGalvo);
	InsertListItem(engine->engineSetPanHndl, ScanSetPan_SlowAxisType, -1, "Resonant galvo", ResonantGalvo);
	InsertListItem(engine->engineSetPanHndl, ScanSetPan_SlowAxisType, -1, "Acousto-optic deflector", AOD);
	InsertListItem(engine->engineSetPanHndl, ScanSetPan_SlowAxisType, -1, "Translation stage", Translation);
	// select by default NonResonantGalvo
	SetCtrlIndex(engine->engineSetPanHndl, ScanSetPan_SlowAxisType, 0);
	
	//----------------------------------------------------------
	// populate by default NonResonantGalvo type of calibrations
	//----------------------------------------------------------
	UpdateAvailableCalibrations	(engine, NonResonantGalvo);
	
	
	
	
	DisplayPanel(engine->engineSetPanHndl);
}

// adds a new scan engine
static void CVICALLBACK NewScanEngineMenu_CB (int menuBar, int menuItem, void *callbackData, int panel)
{
	LaserScanning_type*		ls 				= callbackData;
	int						workspacePanHndl;
	
	if (ls->enginesPanHndl) return; // do nothing if panel is already loaded and visible
	
	// load panel resources
	GetPanelAttribute(panel, ATTR_PANEL_PARENT, &workspacePanHndl);
	ls->enginesPanHndl = LoadPanel(workspacePanHndl, MOD_LaserScanning_UI, EnginesPan); 
	// add callback and data to controls
	SetCtrlsInPanCBInfo(ls, NewScanEngine_CB, ls->enginesPanHndl);
	
	//------------------------------------------------------------------------------------------------------------------
	// Insert scan engine types here
	//------------------------------------------------------------------------------------------------------------------
	InsertListItem(ls->enginesPanHndl, EnginesPan_ScanTypes, -1, "Rectangular raster scan", ScanEngine_RectRaster);
	
	
	//------------------------------------------------------------------------------------------------------------------
	
	DisplayPanel(ls->enginesPanHndl);
}

static int CVICALLBACK NewScanEngine_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	LaserScanning_type*		ls 	= callbackData;
	
	switch (event) {
			
		case EVENT_LEFT_DOUBLE_CLICK:
			
			switch (control) {
					
				case EnginesPan_ScanTypes:
				
					unsigned int		 	engineType;								//	  of  ScanEngineEnum_type
					ScanEngine_type*		newScanEngine		= NULL;
					char* 					engineName;
					int						tabPagePanHndl;
					void*					panDataPtr;
					int						newTabIdx			= 0;
					int						scanPanHndl			= 0;
			
					GetCtrlVal(panel, EnginesPan_ScanTypes, &engineType);
			
					engineName = DLGetUINameInput("New Scan Engine Name", Max_NewScanEngine_NameLength, ValidScanEngineName, NULL); 
			
					if (!engineName) return 0; // do nothing if operation was cancelled
			
					// remove empty "None" tab page
					GetPanelHandleFromTabPage(ls->mainPanHndl, ScanPan_ScanEngines, 0, &tabPagePanHndl);
					GetPanelAttribute(tabPagePanHndl, ATTR_CALLBACK_DATA, &panDataPtr);
					if (!panDataPtr) DeleteTabPage(ls->mainPanHndl, ScanPan_ScanEngines, 0, 1);
			
					//------------------------------------------------------------------------------------------------------------------
					// Load here panels and resources for all scan engine types
					//------------------------------------------------------------------------------------------------------------------
					switch (engineType) {
					
						case ScanEngine_RectRaster:
					
						// initialize raster scan engine
							char*	fastAxisComVChanName 	= DLGetUniqueVChanName(VChan_Default_FastAxis_Command);
							char*	slowAxisComVChanName 	= DLGetUniqueVChanName(VChan_Default_SlowAxis_Command);
							char*	fastAxisPosVChanName 	= DLGetUniqueVChanName(VChan_Default_FastAxis_Position);
							char*	slowAxisPosVChanName	= DLGetUniqueVChanName(VChan_Default_SlowAxis_Position);
							char*	imageOutVChanName		= DLGetUniqueVChanName(VChan_Default_ImageOut);
					
							newScanEngine = (ScanEngine_type*) init_RectangleRaster_type(ls, engineName, fastAxisComVChanName, slowAxisComVChanName,
														fastAxisPosVChanName, slowAxisPosVChanName, imageOutVChanName);
					
							scanPanHndl = LoadPanel(ls->mainPanHndl, MOD_LaserScanning_UI, RectRaster); 
							newTabIdx = InsertPanelAsTabPage(ls->mainPanHndl, ScanPan_ScanEngines, -1, scanPanHndl); 
					
							break;
						// add below more cases	
					}
					//------------------------------------------------------------------------------------------------------------------
			
					// discard loaded panel and add scan control panel handle to scan engine
					DiscardPanel(scanPanHndl);
					GetPanelHandleFromTabPage(ls->mainPanHndl, ScanPan_ScanEngines, newTabIdx, &newScanEngine->scanSetPanHndl);
			
					// change new scan engine tab title
					SetTabPageAttribute(ls->mainPanHndl, ScanPan_ScanEngines, newTabIdx, ATTR_LABEL_TEXT, engineName);  
			
					// add callback function and data to the new tab page panel and controls from the panel
					SetCtrlsInPanCBInfo(newScanEngine, RectangleRasterScan_CB, newScanEngine->scanSetPanHndl);  
					SetPanelAttribute(newScanEngine->scanSetPanHndl, ATTR_CALLBACK_DATA, newScanEngine);
				  
					// add scan engine task controller to the framework
					DLAddTaskController((DAQLabModule_type*)ls, newScanEngine->taskControl);
					
					// add scan engine VChans to DAQLab framework
					DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)newScanEngine->VChanFastAxisCom);
					DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)newScanEngine->VChanSlowAxisCom);
					DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)newScanEngine->VChanFastAxisPos);
					DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)newScanEngine->VChanSlowAxisPos);
					DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)newScanEngine->VChanImageOut);
					
					// add new scan engine to laser scanning module list of scan engines
					ListInsertItem(ls->scanEngines, &newScanEngine, END_OF_LIST);
					
					// undim scan engine "Settings" menu item
					SetMenuBarAttribute(ls->menuBarHndl, ls->engineSettingsMenuItemHndl, ATTR_DIMMED, 0); 
			
					free(engineName);
					break;
			}
			break;
			
		case EVENT_COMMIT:
			
			switch (control) {
				
				case EnginesPan_DoneBTTN:
					
					DiscardPanel(panel);
					ls->enginesPanHndl = 0;
					break;
			}
			break;
	}
	
	
	return 0;
}

static BOOL	ValidScanEngineName (char name[], void* dataPtr)
{
	return DLValidTaskControllerName(name);
}

static void	UpdateAvailableCalibrations	(ScanEngine_type* scanEngine, ScanAxis_type axisType)
{
	size_t					nCal 		= ListNumItems(scanEngine->lsModule->availableCals);
	ScanAxisCal_type**		calPtr;
	
	// empty lists, insert empty selection and select by default
	DeleteListItem(scanEngine->engineSetPanHndl, ScanSetPan_FastAxisCal, 0, -1);
	DeleteListItem(scanEngine->engineSetPanHndl, ScanSetPan_SlowAxisCal, 0, -1);
	InsertListItem(scanEngine->engineSetPanHndl, ScanSetPan_FastAxisCal, -1,"", 0);  
	InsertListItem(scanEngine->engineSetPanHndl, ScanSetPan_SlowAxisCal, -1,"", 0); 
	SetCtrlIndex(scanEngine->engineSetPanHndl, ScanSetPan_FastAxisCal, 0);
	SetCtrlIndex(scanEngine->engineSetPanHndl, ScanSetPan_SlowAxisCal, 0);
	
	for (size_t i = 1; i <= nCal; i++) {
		calPtr = ListGetPtrToItem(scanEngine->lsModule->availableCals, i);
		// fast axis calibration list
		if (((*calPtr)->scanAxisType == axisType) && (*calPtr != scanEngine->slowAxisCal)) 
			InsertListItem(scanEngine->engineSetPanHndl, ScanSetPan_FastAxisCal, -1, (*calPtr)->calName, i);
		// select fast axis list item if calibration is assigned to the scan engine
		if (scanEngine->fastAxisCal == *calPtr)
			SetCtrlIndex(scanEngine->engineSetPanHndl, ScanSetPan_FastAxisCal, i);
		// slow axis calibration list
		if (((*calPtr)->scanAxisType == axisType) && (*calPtr != scanEngine->fastAxisCal)) 
			InsertListItem(scanEngine->engineSetPanHndl, ScanSetPan_SlowAxisCal, -1, (*calPtr)->calName, i);
		// select fast axis list item if calibration is assigned to the scan engine
		if (scanEngine->slowAxisCal == *calPtr)
			SetCtrlIndex(scanEngine->engineSetPanHndl, ScanSetPan_SlowAxisCal, i);
	} 
}

static int CVICALLBACK ManageScanAxisCalib_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	LaserScanning_type*		ls = callbackData;
	
	switch (event) {
			
		case EVENT_COMMIT:
			
			switch (control) {
				
				case ManageAxis_New:	   	// new calibration
					
					if (ls->newAxisCalTypePanHndl) {
						SetActivePanel(ls->newAxisCalTypePanHndl);
						return 0; // do nothing if panel is already loaded (only one calibration may be active at a time)
					}
					
					int parentPanHndl;
					GetPanelAttribute(panel, ATTR_PANEL_PARENT, &parentPanHndl);
					ls->newAxisCalTypePanHndl = LoadPanel(parentPanHndl, MOD_LaserScanning_UI, AxisSelect); 
					
					// populate axis type list
					InsertListItem(ls->newAxisCalTypePanHndl, AxisSelect_AxisType, -1, "Non-resonant galvo", NonResonantGalvo);
					InsertListItem(ls->newAxisCalTypePanHndl, AxisSelect_AxisType, -1, "Resonant galvo", ResonantGalvo);
					InsertListItem(ls->newAxisCalTypePanHndl, AxisSelect_AxisType, -1, "Acousto-optic deflector", AOD);
					InsertListItem(ls->newAxisCalTypePanHndl, AxisSelect_AxisType, -1, "Translation stage", Translation);
					// select by default NonResonantGalvo
					SetCtrlIndex(ls->newAxisCalTypePanHndl, AxisSelect_AxisType, 0);
					// attach callback function and data to the controls in the panel
					SetCtrlsInPanCBInfo(ls, NewScanAxisCalib_CB, ls->newAxisCalTypePanHndl);
					
					DisplayPanel(ls->newAxisCalTypePanHndl);
					break;
					
				case ManageAxis_Close:	  	// close axis calibration manager
					
					DiscardPanel(panel);
					ls->manageAxisCalPanHndl = 0;
					break;
					
			}
			break;
			
		case EVENT_KEYPRESS:   				// delete calibration
			
			if ((control != ManageAxis_AxisCalibList) || (eventData1 != VAL_FWD_DELETE_VKEY)) return 0; // continue only if Del key was pressed
			
			break;
			
		case LEFT_DOUBLE_CLICK:			   	// open calibration data
			
			break;
	}
	
	return 0;
}

static int CVICALLBACK NewScanAxisCalib_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	LaserScanning_type*		ls = callbackData;
	
	switch (event) {
			
		case EVENT_COMMIT:
			
			switch (control) {
					
				case AxisSelect_OKBTTN:
					
					unsigned int 	axisType;
					int				parentPanHndl;
					
					GetPanelAttribute(panel, ATTR_PANEL_PARENT, &parentPanHndl);
					GetCtrlVal(panel, AxisSelect_AxisType, &axisType);
					
					switch (axisType) {
							
						case NonResonantGalvo:
							
							// get unique name for calibration task controller
							char* calTCName = DLGetUniqueTaskControllerName(AxisCal_Default_TaskController_Name);
							
							// get unique name for command signal VChan
							char* commandVChanName = StrDup(calTCName);
							AppendString(&commandVChanName,": Command", -1);
							
							// get unique name for position feedback signal VChan 
							char* positionVChanName = StrDup(calTCName);
							AppendString(&positionVChanName,": Position", -1);
							
							// init structure for galvo calibration
							NonResGalvoCal_type* 	nrgCal = init_NonResGalvoCal_type(ls, calTCName, commandVChanName, positionVChanName);
							
							//----------------------------------------
							// Task Controller and VChans registration
							//----------------------------------------
							// register calibration Task Controller with the framework
							DLAddTaskController((DAQLabModule_type*)ls, nrgCal->baseClass.taskController);
							// register VChans with framework
							DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)nrgCal->baseClass.VChanCom);
							DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)nrgCal->baseClass.VChanPos);
							// add new galvo calibration to laser scanning module list
							ListInsertItem(ls->activeCal, &nrgCal, END_OF_LIST);
							//----------------------------------------------------------
							
							nrgCal->baseClass.calPanHndl = LoadPanel(parentPanHndl, MOD_LaserScanning_UI, NonResGCal); 
							// change panel title
							SetPanelAttribute(nrgCal->baseClass.calPanHndl, ATTR_TITLE, calTCName);
							// add callback function and data first to all the direct controls in the calibration panel
							SetCtrlsInPanCBInfo(nrgCal, NonResGalvoCal_MainPan_CB, nrgCal->baseClass.calPanHndl); 
							
							int calibPanHndl;
							// get calibration panel handle
							GetPanelHandleFromTabPage(nrgCal->baseClass.calPanHndl, NonResGCal_Tab, 0, &calibPanHndl);
							// add callback function and data to calibration controls
							SetCtrlsInPanCBInfo(nrgCal, NonResGalvoCal_CalPan_CB, calibPanHndl); 
							
							int testPanHndl;
							// get test panel handle
							GetPanelHandleFromTabPage(nrgCal->baseClass.calPanHndl, NonResGCal_Tab, 1, &testPanHndl);
							// add callback function and data to test controls
							SetCtrlsInPanCBInfo(nrgCal, NonResGalvoCal_TestPan_CB, testPanHndl);
							
							// Display calibration panel
							DisplayPanel(nrgCal->baseClass.calPanHndl);
							
							// cleanup
							OKfree(commandVChanName); 
							OKfree(positionVChanName);
							OKfree(calTCName);
							
							break;
							
						default:
							
							DLMsg("Calibration not implemented for the chosen axis type.", 1);
					}
					
					
					// discard axis calibration selection panel
					DiscardPanel(ls->newAxisCalTypePanHndl);
					ls->newAxisCalTypePanHndl = 0;
					break;
					
				case AxisSelect_CancelBTTN:
					
					// discard axis calibration selection panel
					DiscardPanel(ls->newAxisCalTypePanHndl);
					ls->newAxisCalTypePanHndl = 0;
					break;
			}
			break;
	}
	
	return 0;
}

/// HIFN Finds the maximum slope from a given galvo response.
/// HIRET Returns 0 if slope cannot be determined given required accuracy (RELERR) and 1 otherwise.
static int FindSlope(double* corrpos, int nsamples, double samplingRate, double signalStdDev, int nrep, double sloperelerr, double* maxslope)
{
	double 	tmpslope;
	double 	relslope_err;
	BOOL 	foundmaxslope = 0;
			
	for (int nslope_elem = 1; nslope_elem < nsamples; nslope_elem++){  
			
		*maxslope = 0;   // in [V/ms]
		for (int i = 0; i < (nsamples - nslope_elem); i++) {
			tmpslope = (corrpos[nslope_elem+i] - corrpos[i])/(nslope_elem*1000/samplingRate); // slope in [V/ms]
			if (tmpslope > *maxslope) *maxslope = tmpslope;
		}
		// calculate relative maximum slope error
		relslope_err = fabs(signalStdDev/sqrt(nrep) * 1.414 / (*maxslope * nslope_elem * 1000/samplingRate));
		
		if (relslope_err <= sloperelerr) {
			foundmaxslope = 1;
			break;
		}
	}
	
	return foundmaxslope;
			
}

/// HIFN Calculates the lag in samples between signal1 and signal2, signal2 being delayed and similar to signal 1
/// HIFN In this case signal1 would be the command signal and signal2 the response signal
static int MeasureLag(double* signal1, double* signal2, int nelem)
{
	double newresidual = 0;
	double oldresidual = 0; 
	int    delta       = 0; // response lag in terms of clock samples
	int    dincr       = 1; // number of samples to shift with each iteration
	BOOL negative      = 0; // if lag of signal2 is negative		
			
	for (int i = 0; i < nelem; i++) oldresidual += fabs (signal1[i] - signal2[i]);				   // initial residual
	for (int i = 0; i < (nelem - dincr); i++) newresidual += fabs(signal1[i+dincr] - signal2[i]); // residual with negative lag of signal2
	
	if (newresidual < oldresidual) negative = 1; // change sign of increment if signal2 has a negative lag
	
	delta += dincr;
	while (1) {
		
		newresidual = 0;
		if (negative)
			for (int i = 0; i < (nelem - delta); i++) newresidual += fabs(signal1[i+delta] - signal2[i]);
		else
			for (int i = 0; i < (nelem - delta); i++) newresidual += fabs(signal1[i] - signal2[i+delta]);
		
		if (newresidual < oldresidual) delta += dincr;
		else break;
		
		if (delta >= nelem) break;
		
		oldresidual = newresidual;
	};
	delta -= dincr;
	if (negative) delta = - delta;
	
	return delta;
}

/// HIFN Analyzes a step response signal to calculate the number of samples it takes to cross a threshold from a low-level and number of samples to reach the high level within given threshold. 
/// HIFN The 1-based lowidx and highidx are the number of samples from the beginning of the signal where the crossing of the threshold occurs stepsamples is the number of samples needed to cross from lowlevel to highlevel.
static int GetStepTimes (double* signal, int nsamples, double lowlevel, double highlevel, double threshold, int* lowidx, int* highidx, int* stepsamples)
{
	BOOL crossed; // flag showing if threshold was crossed 
	
	threshold = fabs(threshold); // make sure it's positive	
	
	if (lowlevel > highlevel) { MessagePopup("Error", "Low level must be smaller than High level."); return -1; } 
	crossed = 0;
	for (int i = 0; i < nsamples; i++) 
		if (signal[i] > lowlevel + threshold) { *lowidx = i+1; crossed = 1; break;}
	if (!crossed) { MessagePopup("Warning", "Could not find low level crossing of signal."); return -1; } 
	
	crossed = 0;
	for (int i = nsamples - 1; i >= 0; i--) 
		if (signal[i] < highlevel - threshold) { *highidx = i+1; crossed = 1; break;}
	if (!crossed) { MessagePopup("Warning", "Could not find low level crossing of signal."); return -1; } 
	
	*stepsamples= abs(*highidx - *lowidx);
	
	return 0;
}

//-------------------------------------------
// DetChan_type
//-------------------------------------------
static DetChan_type* init_DetChan_type (char VChanName[])
{
	DetChan_type* det = malloc(sizeof(DetChan_type));
	if (!det) return NULL;
	
	PacketTypes allowedPacketTypes[] = {WaveformPacket_UShort, WaveformPacket_UInt, WaveformPacket_Double};
	
	det->detVChan = init_SinkVChan_type(VChanName, allowedPacketTypes, NumElem(allowedPacketTypes), det, DetVChanConnected, DetVChanDisconnected);
	
	return det;
}

static void	discard_DetChan_type (DetChan_type** a)
{
	if (!*a) return;
	
	discard_VChan_type((VChan_type**)&(*a)->detVChan);
	
	OKfree(*a);
}

static int CVICALLBACK ScanEngineSettings_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ScanEngine_type*	engine		= callbackData;
	char*				newName		= NULL;
	int					listItemIdx; 
	DetChan_type** 		detChanPtr; 
	
	switch (event)
	{
		case EVENT_COMMIT:
			
			switch (control) {
				
				case ScanSetPan_FastAxisCommand:
					
					newName = GetStringFromControl (panel, control);
					SetVChanName((VChan_type*)engine->VChanFastAxisCom, newName);
					break;
					
				case ScanSetPan_SlowAxisCommand:
					
					newName = GetStringFromControl (panel, control);
					SetVChanName((VChan_type*)engine->VChanSlowAxisCom, newName);
					break;
					
				case ScanSetPan_FastAxisPosition:
					
					newName = GetStringFromControl (panel, control);
					SetVChanName((VChan_type*)engine->VChanFastAxisPos, newName);
					break;
					
				case ScanSetPan_SlowAxisPosition:
					
					newName = GetStringFromControl (panel, control);
					SetVChanName((VChan_type*)engine->VChanSlowAxisPos, newName);
					break;
					
				case ScanSetPan_ImageOut:
					
					newName = GetStringFromControl (panel, control);
					SetVChanName((VChan_type*)engine->VChanImageOut, newName);
					break;
					
				case ScanSetPan_AddImgChan:
					
					// create new VChan with a predefined base name
					newName = DLGetUniqueVChanName(VChan_Default_DetectionChan);
					DetChan_type* detChan = init_DetChan_type(newName);
					// insert new VChan to list control, engine list and register with framework
					InsertListItem(panel, ScanSetPan_ImgChans, -1, newName, newName);
					ListInsertItem(engine->DetChans, &detChan, END_OF_LIST);
					DLRegisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)detChan->detVChan);
					break;
					
				case ScanSetPan_Close:
					
					DiscardPanel(engine->engineSetPanHndl);
					engine->engineSetPanHndl = 0;
					break;
					
				case ScanSetPan_FastAxisType:
					
					unsigned int 	fastAxisType;
					GetCtrlVal(panel, control, &fastAxisType);
					
					UpdateAvailableCalibrations(engine, fastAxisType); 
					break;
					
				case ScanSetPan_SlowAxisType:
					
					unsigned int 	slowAxisType;
					GetCtrlVal(panel, control, &slowAxisType);
					
					UpdateAvailableCalibrations(engine, slowAxisType); 
					break;
					
				case ScanSetPan_FastAxisCal:
					
					unsigned int	fastAxisCalIdx;
					GetCtrlVal(panel, control, &fastAxisCalIdx);
					if (fastAxisCalIdx)
						engine->fastAxisCal = ListGetPtrToItem(engine->lsModule->availableCals, fastAxisCalIdx); 
					else
						engine->fastAxisCal = NULL;
					
					break;
					
				case ScanSetPan_SlowAxisCal:
					
					unsigned int	slowAxisCalIdx;
					GetCtrlVal(panel, control, &slowAxisCalIdx);
					if (slowAxisCalIdx)
						engine->slowAxisCal = ListGetPtrToItem(engine->lsModule->availableCals, slowAxisCalIdx); 
					else
						engine->slowAxisCal = NULL;
					
					break;
					
			}
			break;
			
		case EVENT_KEYPRESS:
			// discard detection channel		
			// continue only if Del key is pressed and the image channels control is active
			if (eventData1 != VAL_FWD_DELETE_VKEY || control != ScanSetPan_ImgChans) break;
			GetCtrlIndex(panel, ScanSetPan_ImgChans, &listItemIdx);
			if (listItemIdx < 0) break; // stop here if list is empty
			
			detChanPtr = ListGetPtrToItem(engine->DetChans, listItemIdx+1);
			DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)(*detChanPtr)->detVChan);
			discard_VChan_type((VChan_type**)&(*detChanPtr)->detVChan);
			DeleteListItem(panel, ScanSetPan_ImgChans, listItemIdx, 1);  
			break;
			
		case EVENT_LEFT_DOUBLE_CLICK:
			
			newName = DLGetUINameInput("Rename VChan", DAQLAB_MAX_VCHAN_NAME, DLValidateVChanName, NULL);
			if (!newName) return 0; // user cancelled, do nothing
			
			GetCtrlIndex(panel, ScanSetPan_ImgChans, &listItemIdx); 
			detChanPtr = ListGetPtrToItem(engine->DetChans, listItemIdx+1);
			SetVChanName((VChan_type*)(*detChanPtr)->detVChan, newName);
			ReplaceListItem(panel, ScanSetPan_ImgChans, listItemIdx, newName, newName);  
			break;
	}
	
	OKfree(newName);
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
	
	// if there are no more scan engines, add default "None" tab page and dim "Settings" menu item
	GetNumTabPages(panel, control, &nTabs);
	if (!nTabs) {
		SetMenuBarAttribute(ls->menuBarHndl, ls->engineSettingsMenuItemHndl, ATTR_DIMMED, 1); 
		InsertTabPage(panel, control, -1, "None");
	}
	
	return 0;
}

static int CVICALLBACK NonResGalvoCal_MainPan_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	NonResGalvoCal_type*		nrgCal = callbackData;
	
	switch (event)
	{
		case EVENT_COMMIT:
			
			switch (control) {
					
				case NonResGCal_SaveCalib:
					
					break;
					
				case NonResGCal_Done:
					
					TaskControl_type*	calTC = nrgCal->baseClass.taskController; 
					
					DisassembleTaskTreeBranch(calTC);
					
					// unregister calibration VChans
					DLUnregisterVChan((DAQLabModule_type*)nrgCal->baseClass.lsModule, (VChan_type*)nrgCal->baseClass.VChanCom);
					DLUnregisterVChan((DAQLabModule_type*)nrgCal->baseClass.lsModule, (VChan_type*)nrgCal->baseClass.VChanPos);
					
					// unregister calibration Task Controller
					DLRemoveTaskController((DAQLabModule_type*)nrgCal->baseClass.lsModule, calTC);
					
					// discard active calibration data structure
					(*nrgCal->baseClass.Discard) ((ScanAxisCal_type**)&nrgCal);
					
					break;
					
				case NonResGCal_GalvoPlot:
					
					double plotX, plotY;
					GetGraphCursor(panel, control, 1, &plotX, &plotY);
					SetCtrlVal(panel, NonResGCal_CursorX, plotX);
					SetCtrlVal(panel, NonResGCal_CursorX, plotY);
					break;
					
			}

			break;
	}
	return 0;
}

static int CVICALLBACK NonResGalvoCal_CalPan_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	NonResGalvoCal_type* nrgCal = callbackData;
	
	switch (event)
	{
		case EVENT_COMMIT:
			
			switch (control) {
					
				case Cal_CommMinV:
					
					GetCtrlVal(panel, control, &nrgCal->commandVMin);
					break;
					
				case Cal_CommMaxV:
					
					GetCtrlVal(panel, control, &nrgCal->commandVMax);
					break;
					
				case Cal_PosMinV:
					
					GetCtrlVal(panel, control, &nrgCal->positionVMin);
					break;
					
				case Cal_PosMaxV:
					
					GetCtrlVal(panel, control, &nrgCal->positionVMax);
					break;
					
				case Cal_ParkedV:
					
					GetCtrlVal(panel, control, &nrgCal->UISet.parked);
					break;
					
				case Cal_ScanTime:
					
					GetCtrlVal(panel, control, &nrgCal->UISet.scanTime);
					break;
					
				case Cal_MinStep:
					
					GetCtrlVal(panel, control, &nrgCal->UISet.minStepSize);
					break;
					
				case Cal_Resolution:
					
					GetCtrlVal(panel, control, &nrgCal->UISet.resolution);
					break;
			}

			break;
	}
	return 0;
}

static int CVICALLBACK NonResGalvoCal_TestPan_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	NonResGalvoCal_type*	nrgCal = callbackData;
	
	switch (event)
	{
		case EVENT_COMMIT:
			/*
			switch (control) {
					
			}*/

			break;
	}
	return 0;
}

static ScanAxisCal_type* initalloc_ScanAxisCal_type	(ScanAxisCal_type* cal)
{
	// if NULL, allocate memory, otherwise just use the provided address
	if (!cal) {
		cal = malloc (sizeof(ScanAxisCal_type));
		if (!cal) return NULL;
	}
	
	cal->scanAxisType	= NonResonantGalvo; 
	cal->calName 		= NULL;
	cal->taskController	= NULL;
	cal->VChanCom		= NULL;
	cal->VChanPos		= NULL;
	cal->comSampRate	= NULL;
	cal->posSampRate	= NULL;
	cal->calPanHndl		= 0;
	cal->Discard		= NULL;
	
	return cal;	
}

static void discard_ScanAxisCal_type (ScanAxisCal_type** cal)
{
	OKfree((*cal)->calName); 
	discard_TaskControl_type(&(*cal)->taskController);
	discard_VChan_type((VChan_type**)&(*cal)->VChanCom);
	discard_VChan_type((VChan_type**)&(*cal)->VChanPos);
	OKfree((*cal)->comSampRate);
	OKfree((*cal)->posSampRate);
	if ((*cal)->calPanHndl) {DiscardPanel((*cal)->calPanHndl); (*cal)->calPanHndl =0;}
	
	OKfree(*cal);
}

void CVICALLBACK ScanAxisCalibrationMenu_CB	(int menuBarHandle, int menuItemID, void *callbackData, int panelHandle)
{
	LaserScanning_type*		ls 				= callbackData;
	int						parentPanHndl;
	
	// if panel is already loaded, do nothing
	if (ls->manageAxisCalPanHndl) return;
	// get workspace panel handle
	GetPanelAttribute(panelHandle, ATTR_PANEL_PARENT, &parentPanHndl);
	
	ls->manageAxisCalPanHndl = LoadPanel(parentPanHndl, MOD_LaserScanning_UI, ManageAxis);
	//change panel title
	SetPanelAttribute(ls->manageAxisCalPanHndl, ATTR_TITLE, ls->baseClass.instanceName);
	
	// list available calibrations
	size_t					nCal 			= ListNumItems(ls->availableCals);
	ScanAxisCal_type**		calPtr;
	for (size_t i = 1; i <= nCal; i++) {
		calPtr = ListGetPtrToItem(ls->availableCals, i);
		InsertListItem(ls->manageAxisCalPanHndl, ManageAxis_AxisCalibList, -1, (*calPtr)->calName, 0); 
	} 
	
	// attach callback function and data to all controls in the panel
	SetCtrlsInPanCBInfo(ls, ManageScanAxisCalib_CB, ls->manageAxisCalPanHndl); 
	
	DisplayPanel(ls->manageAxisCalPanHndl);
}

static NonResGalvoCal_type* init_NonResGalvoCal_type (LaserScanning_type* lsModule, char calName[], char commandVChanName[], char positionVChanName[])
{
	NonResGalvoCal_type*	cal = malloc(sizeof(NonResGalvoCal_type));
	if (!cal) return NULL;
	
	PacketTypes allowedPacketTypes[] = {WaveformPacket_Double};
	
	// init parent class
	initalloc_ScanAxisCal_type(&cal->baseClass);
	if(!(cal->baseClass.calName		= StrDup(calName))) {free(cal); return NULL;}
	cal->baseClass.VChanCom			= init_SourceVChan_type(commandVChanName, WaveformPacket_Double, cal, NonResGalvoCal_ComVChan_Connected, NonResGalvoCal_ComVChan_Disconnected);   
	cal->baseClass.VChanPos			= init_SinkVChan_type(positionVChanName, allowedPacketTypes, NumElem(allowedPacketTypes), cal, NonResGalvoCal_PosVChan_Connected, NonResGalvoCal_PosVChan_Disconnected);  
	cal->baseClass.scanAxisType  	= NonResonantGalvo;
	cal->baseClass.Discard			= discard_NonResGalvoCal_type; // override
	cal->baseClass.taskController	= init_TaskControl_type(calName, cal, ConfigureTC_NonResGalvoCal, IterateTC_NonResGalvoCal, AbortIterationTC_NonResGalvoCal, StartTC_NonResGalvoCal, ResetTC_NonResGalvoCal, 
								  DoneTC_NonResGalvoCal, StoppedTC_NonResGalvoCal, DimTC_NonResGalvoCal, NULL, ModuleEventHandler_NonResGalvoCal, ErrorTC_NonResGalvoCal);
	// connect sink VChans (VChanPos) to the Task Controller so that it can process incoming galvo position data
	AddSinkVChan(cal->baseClass.taskController, cal->baseClass.VChanPos, DataReceivedTC_NonResGalvoCal, TASK_VCHAN_FUNC_ITERATE);  
	cal->baseClass.lsModule			= lsModule;
	
								  
	// init NonResGalvoCal_type
	cal->commandVMin		= 0;
	cal->commandVMax		= 0;
	cal->positionVMin		= 0;
	cal->positionVMax		= 0;
	cal->calIterations[NonResGalvoCal_Slope_Offset_Lag] 	= 0;
	cal->calIterations[NonResGalvoCal_SwitchTimes]			= 0;
	cal->calIterations[NonResGalvoCal_MaxSlopes]			= 0;
	cal->calIterations[NonResGalvoCal_TriangleWave]			= 0;
	cal->currentCal											= NonResGalvoCal_Slope_Offset_Lag;
	cal->currIterIdx										= 0;
	cal->slope				= NULL;	// i.e. calibration not performed yet
	cal->offset				= NULL;
	cal->posStdDev			= NULL;
	cal->lag				= NULL;
	cal->switchTimes		= NULL;
	cal->maxSlopes			= NULL;
	cal->triangleCal		= NULL;
	cal->UISet.resolution  	= 0;
	cal->UISet.minStepSize 	= 0;
	cal->UISet.scanTime    	= 2;	// in [s]
	cal->UISet.parked		= 0;
	
	return cal;
}

static void	discard_NonResGalvoCal_type	(ScanAxisCal_type** cal)
{
	if (!*cal) return;
	
	NonResGalvoCal_type*	NRGCal = (NonResGalvoCal_type*) *cal;
	
	// discard NonResGalvoCal_type specific data
	OKfree(NRGCal->slope);
	OKfree(NRGCal->offset);
	OKfree(NRGCal->posStdDev);
	OKfree(NRGCal->lag);
	discard_SwitchTimes_type(&NRGCal->switchTimes);
	discard_MaxSlopes_type(&NRGCal->maxSlopes);
	discard_TriangleCal_type(&NRGCal->triangleCal);
	
	// discard ScanAxisCal_type base class data
	discard_ScanAxisCal_type(cal);
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
	if (!(*a)) return;
	
	OKfree((*a)->commandAmp);
	OKfree((*a)->actualAmp);
	OKfree((*a)->maxFreq);
	OKfree((*a)->resLag);
	OKfree(*a);
}

// calibration command VChan connected callback
static void	NonResGalvoCal_ComVChan_Connected (VChan_type* self, VChan_type* connectedVChan)
{
	
}

// calibration command VChan disconnected callback
static void	NonResGalvoCal_ComVChan_Disconnected (VChan_type* self, VChan_type* disconnectedVChan)
{
	
}

// calibration position VChan connected callback
static void	NonResGalvoCal_PosVChan_Connected (VChan_type* self, VChan_type* connectedVChan)
{
	
}

// calibration position VChan disconnected callback
static void	NonResGalvoCal_PosVChan_Disconnected (VChan_type* self, VChan_type* disconnectedVChan)
{
	
}

static int init_ScanEngine_type (ScanEngine_type* 		engine, 
								 LaserScanning_type*	lsModule,
								 ScanEngineEnum_type 	engineType,
								 char 					fastAxisComVChanName[], 
								 char 					slowAxisComVChanName[], 
								 char					fastAxisPosVChanName[],
								 char					slowAxisPosVChanName[],
								 char					imageOutVChanName[])					
{
	PacketTypes allowedPacketTypes[] = {WaveformPacket_Double};
	
	// VChans
	engine->engineType				= engineType;
	engine->VChanFastAxisCom		= init_SourceVChan_type(fastAxisComVChanName, WaveformPacket_Double, engine, FastAxisComVChan_Connected, FastAxisComVChan_Disconnected); 
	engine->VChanSlowAxisCom		= init_SourceVChan_type(slowAxisComVChanName, WaveformPacket_Double, engine, SlowAxisComVChan_Connected, SlowAxisComVChan_Disconnected); 
	engine->VChanFastAxisPos		= init_SinkVChan_type(fastAxisPosVChanName, allowedPacketTypes, NumElem(allowedPacketTypes), engine, FastAxisPosVChan_Connected, FastAxisPosVChan_Disconnected); 
	engine->VChanSlowAxisPos		= init_SinkVChan_type(slowAxisPosVChanName, allowedPacketTypes, NumElem(allowedPacketTypes), engine, SlowAxisPosVChan_Connected, SlowAxisPosVChan_Disconnected); 
	engine->VChanImageOut			= init_SourceVChan_type(imageOutVChanName, ImagePacket_NIVision, engine, ImageOutVChan_Connected, ImageOutVChan_Disconnected); 
	if(!(engine->DetChans			= ListCreate(sizeof(DetChan_type*)))) goto Error;
	
	// reference to axis calibration
	engine->fastAxisCal				= NULL;
	engine->slowAxisCal				= NULL;
	
	// reference to the laser scanning module owning the scan engine
	engine->lsModule				= lsModule;
	// task controller
	engine->taskControl				= NULL; 	// initialized by derived scan engine classes
	
	// scan settings panel handle
	engine->scanSetPanHndl			= 0;
	// scan engine settings panel handle
	engine->engineSetPanHndl		= 0;
	
	// position feedback flags
	engine->Discard					= NULL;	   // overriden by derived scan engine classes
	
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
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanFastAxisCom);
		discard_VChan_type((VChan_type**)&engine->VChanFastAxisCom);
	}
	
	// slow axis command
	if (engine->VChanSlowAxisCom) {
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanSlowAxisCom);
		discard_VChan_type((VChan_type**)&engine->VChanSlowAxisCom);
	}
	
	// fast axis position feedback
	if (engine->VChanFastAxisPos) {
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanFastAxisPos);
		discard_VChan_type((VChan_type**)&engine->VChanFastAxisPos);
	}
	
	// slow axis position feedback
	if (engine->VChanSlowAxisPos) {
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanSlowAxisPos);
		discard_VChan_type((VChan_type**)&engine->VChanSlowAxisPos);
	}
	
	// scan engine image output
	if (engine->VChanImageOut) {
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanImageOut);
		discard_VChan_type((VChan_type**)&engine->VChanImageOut);
	}
	
	// detection channels
	size_t	nDet = ListNumItems(engine->DetChans);
	DetChan_type** detChanPtrPtr;
	for (size_t i = 1; i <= nDet; i++) {
		detChanPtrPtr = ListGetPtrToItem(engine->DetChans, i);
		// remove VChan from framework
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)(*detChanPtrPtr)->detVChan);
		discard_DetChan_type(detChanPtrPtr);
	}
	
	//----------------------------------
	// Task controller
	//----------------------------------
	if (engine->taskControl) {
		DLRemoveTaskController((DAQLabModule_type*)engine->lsModule, engine->taskControl); 
		discard_TaskControl_type(&engine->taskControl);
	}
	
	//----------------------------------
	// UI
	//----------------------------------
	if (engine->engineSetPanHndl)		{DiscardPanel(engine->engineSetPanHndl); engine->engineSetPanHndl = 0;}
	
	OKfree(engine);
}

static RectangleRaster_type* init_RectangleRaster_type (LaserScanning_type*		lsModule,
														char 					engineName[], 
														char 					fastAxisComVChanName[], 
														char 					slowAxisComVChanName[], 
														char					fastAxisPosVChanName[],
														char					slowAxisPosVChanName[],
														char					imageOutVChanName[])
{
	RectangleRaster_type*	engine = malloc (sizeof(RectangleRaster_type));
	if (!engine) return NULL;
	
	//--------------------------------------------------------
	// init base scan engine class
	//--------------------------------------------------------
	init_ScanEngine_type(&engine->baseClass, lsModule, ScanEngine_RectRaster, fastAxisComVChanName, slowAxisComVChanName, fastAxisPosVChanName, slowAxisPosVChanName, imageOutVChanName);
	// override discard method
	engine->baseClass.Discard			= discard_RectangleRaster_type;
	// add task controller
	engine->baseClass.taskControl		= init_TaskControl_type(engineName, engine, ConfigureTC_RectRaster, IterateTC_RectRaster, AbortIterationTC_RectRaster, StartTC_RectRaster, ResetTC_RectRaster, 
										  DoneTC_RectRaster, StoppedTC_RectRaster, DimTC_RectRaster, NULL, ModuleEventHandler_RectRaster, ErrorTC_RectRaster);
	
	if (!engine->baseClass.taskControl) {discard_RectangleRaster_type((ScanEngine_type**)&engine); return NULL;}
	
	//--------------------------------------------------------
	// init RectangleRaster_type
	//--------------------------------------------------------
	engine->fastAxisSamplingRate 	= NULL;
	engine->slowAxisSamplingRate	= NULL;
	engine->height					= 0;
	engine->heightOffset			= 0;
	engine->width					= 0;
	engine->widthOffset				= 0;
	engine->pixSize					= 0;
	engine->fps						= 0;
	
	return engine;
}

static void	discard_RectangleRaster_type (ScanEngine_type** engine)
{
	RectangleRaster_type*	rectRasterPtr = (RectangleRaster_type*) *engine;
	
	// discard RectangleRaster_type data
	
	
	// discard Scan Engine data
	discard_ScanEngine_type(engine);
}

static int CVICALLBACK RectangleRasterScan_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:
			
			switch (control) {
			
				case RectRaster_Mode:
					
					break;
					
				case RectRaster_Duration:
					
					break;
				
				case RectRaster_Height:
					
					break;
					
				case RectRaster_Width:
					
					break;
					
				case RectRaster_HeightOffset:
					
					break;
					
				case RectRaster_WidthOffset:
					
					break;
					
				case RectRaster_PixelDwell:
					
					break;
					
				case RectRaster_FPS:
					
					break;
					
				case RectRaster_PSF_FWHM:
					
					break;
					
				case RectRaster_PixelSize:
					
					break;
				
			}

			break;
	}
	return 0;
}

//-----------------------------------------
// Scan Engine VChan management
//-----------------------------------------

// Fast Axis Command VChan
static void	FastAxisComVChan_Connected (VChan_type* self, VChan_type* connectedVChan)
{
	
}

static void	FastAxisComVChan_Disconnected (VChan_type* self, VChan_type* disconnectedVChan)
{
	
}

// Slow Axis Command VChan
static void	SlowAxisComVChan_Connected (VChan_type* self, VChan_type* connectedVChan)
{
	
}

static void	SlowAxisComVChan_Disconnected (VChan_type* self, VChan_type* disconnectedVChan)
{
	
}

// Fast Axis Position VChan
static void FastAxisPosVChan_Connected (VChan_type* self, VChan_type* connectedVChan)
{
	
}

static void	FastAxisPosVChan_Disconnected (VChan_type* self, VChan_type* disconnectedVChan)
{
	
}

// Slow Axis Position VChan
static void SlowAxisPosVChan_Connected (VChan_type* self, VChan_type* connectedVChan)
{
	
}

static void	SlowAxisPosVChan_Disconnected (VChan_type* self, VChan_type* disconnectedVChan)
{
	
}

// Image Out VChan
static void	ImageOutVChan_Connected (VChan_type* self, VChan_type* connectedVChan)
{
	
}

static void ImageOutVChan_Disconnected (VChan_type* self, VChan_type* disconnectedVChan)
{
	
}

// Detection VChans
static void	DetVChanConnected (VChan_type* self, VChan_type* connectedVChan)
{
	
}

static void	DetVChanDisconnected (VChan_type* self, VChan_type* disconnectedVChan)
{
	
}

//---------------------------------------------------------------------
// Non Resonant Galvo Calibration and Testing Task Controller Callbacks 
//---------------------------------------------------------------------

static FCallReturn_type* ConfigureTC_NonResGalvoCal	(TaskControl_type* taskControl, BOOL const* abortFlag)
{
	NonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	// number of iterations for each calibration method
	cal->calIterations[NonResGalvoCal_Slope_Offset_Lag] 	= CALPOINTS;
	cal->calIterations[NonResGalvoCal_SwitchTimes]			= 0;
	cal->calIterations[NonResGalvoCal_MaxSlopes]			= 0;
	cal->calIterations[NonResGalvoCal_TriangleWave]			= 0;
	
	// total number of iterations
	SetTaskControlIterations(taskControl, cal->calIterations[NonResGalvoCal_Slope_Offset_Lag] + cal->calIterations[NonResGalvoCal_SwitchTimes] + 
							 cal->calIterations[NonResGalvoCal_MaxSlopes] + cal->calIterations[NonResGalvoCal_TriangleWave]);
	// set starting calibration type
	cal->currentCal = NonResGalvoCal_Slope_Offset_Lag;
	// reset iteration index
	cal->currIterIdx = 0;
	
	return init_FCallReturn_type(0, "", "");
}

static void IterateTC_NonResGalvoCal (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortIterationFlag)
{
	NonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	switch (cal->currentCal) {
			
		case NonResGalvoCal_Slope_Offset_Lag:
			
			// just a quick test to send waveforms
			double*  			dVal = malloc(100 * sizeof(double));	
			DataPacket_type*	dataPacket;
					
			for (int i = 0; i < 100; i++) dVal[i] = i * 0.01;   // create ramp
			
			dataPacket = init_WaveformPacket_type(Waveform_Double, 100, dVal, 0, 1);
			SendDataPacket(cal->baseClass.VChanCom, dataPacket);
			
			// end iteration here
			TaskControlIterationDone(taskControl, 0, "");
					
			break;
			
		case NonResGalvoCal_SwitchTimes:
			break;
			
		case NonResGalvoCal_MaxSlopes:
			break;
			
		case NonResGalvoCal_TriangleWave:
			break;
	}
}

static void AbortIterationTC_NonResGalvoCal (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	NonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);

}

static FCallReturn_type* StartTC_NonResGalvoCal (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	NonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", ""); 
}

static FCallReturn_type* ResetTC_NonResGalvoCal (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	NonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", ""); 
}

static FCallReturn_type* DoneTC_NonResGalvoCal (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	NonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", ""); 
}

static FCallReturn_type* StoppedTC_NonResGalvoCal (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	NonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", ""); 
}

static void	DimTC_NonResGalvoCal (TaskControl_type* taskControl, BOOL dimmed)
{
	NonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
}

static FCallReturn_type* DataReceivedTC_NonResGalvoCal (TaskControl_type* taskControl, TaskStates_type taskState, SinkVChan_type* sinkVChan, BOOL const* abortFlag)
{
	NonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	switch (cal->currentCal) {
			
		case NonResGalvoCal_Slope_Offset_Lag:
			break;
			
		case NonResGalvoCal_SwitchTimes:
			break;
			
		case NonResGalvoCal_MaxSlopes:
			break;
			
		case NonResGalvoCal_TriangleWave:
			break;
	}
	
	return init_FCallReturn_type(0, "", "");
}

static FCallReturn_type* ModuleEventHandler_NonResGalvoCal (TaskControl_type* taskControl, TaskStates_type taskState, size_t currentIteration, void* eventData, BOOL const* abortFlag)
{
	NonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "");
}

static void ErrorTC_NonResGalvoCal (TaskControl_type* taskControl, char* errorMsg)
{
	NonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
}


//-----------------------------------------
// LaserScanning Task Controller Callbacks
//-----------------------------------------

static FCallReturn_type* ConfigureTC_RectRaster (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	RectangleRaster_type* engine = GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", ""); 
}

static void	IterateTC_RectRaster (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortIterationFlag)
{
	RectangleRaster_type* engine = GetTaskControlModuleData(taskControl);
	
}

static void AbortIterationTC_RectRaster (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	RectangleRaster_type* engine = GetTaskControlModuleData(taskControl);
	
}

static FCallReturn_type* StartTC_RectRaster (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	RectangleRaster_type* engine = GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", ""); 
}

static FCallReturn_type* DoneTC_RectRaster (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	RectangleRaster_type* engine = GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "");   
}

static FCallReturn_type* StoppedTC_RectRaster (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	RectangleRaster_type* engine = GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "");   
}

static FCallReturn_type* ResetTC_RectRaster (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	RectangleRaster_type* engine = GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "");   
}

static void	DimTC_RectRaster (TaskControl_type* taskControl, BOOL dimmed)
{
	RectangleRaster_type* engine = GetTaskControlModuleData(taskControl);
	
}

static FCallReturn_type* DataReceivedTC_RectRaster (TaskControl_type* taskControl, TaskStates_type taskState, SinkVChan_type* sinkVChan, BOOL const* abortFlag)
{
	RectangleRaster_type* engine = GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "");   
}

static void ErrorTC_RectRaster (TaskControl_type* taskControl, char* errorMsg)
{
	RectangleRaster_type* engine = GetTaskControlModuleData(taskControl);
	
}

static FCallReturn_type* ModuleEventHandler_RectRaster (TaskControl_type* taskControl, TaskStates_type taskState, size_t currentIteration, void* eventData, BOOL const* abortFlag)
{
	RectangleRaster_type* engine = GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "");
}
