
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
#include <formatio.h>  
#include "LaserScanning.h"
#include <userint.h>
#include <analysis.h>   
#include "UI_LaserScanning.h"



//==============================================================================
// Constants
#define OKfree(ptr) if (ptr) { free(ptr); ptr = NULL; } 
#define DAQmxErrChk(functionCall) if( DAQmxFailed(error=(functionCall)) ) goto Error; else 
	// maximum number of characters to represent a double precision number
#define MAX_DOUBLE_NCHARS						30

#define MOD_LaserScanning_UI 						"./Modules/Laser Scanning/UI_LaserScanning.uir"
// Default VChan base names. Default base names must be unique among each other!
#define VChan_Default_FastAxis_Command				"Fast Axis Command"
#define VChan_Default_FastAxis_Position				"Fast Axis Position" 
#define VChan_Default_SlowAxis_Command				"Slow Axis Command"
#define VChan_Default_SlowAxis_Position				"Slow Axis Position"
#define VChan_Default_ImageOut						"Image"
#define VChan_Default_DetectionChan					"Detector"
#define AxisCal_Default_TaskController_Name			"Scan Axis Calibration"
#define AxisCal_Default_NewCalibration_Name			"Calibration"

// scan engine settings

#define Max_NewScanEngine_NameLength				50
#define Allowed_Detector_Data_Types					{DL_Waveform_Char, DL_Waveform_Double, DL_Waveform_Float, DL_Waveform_Int, DL_Waveform_Short, DL_Waveform_UChar, DL_Waveform_UInt, DL_Waveform_UShort}

// non-resonant galvo calibration parameters
#define	CALIBRATION_DATA_TO_STRING					"%s<%*f[e6j1] "
#define STRING_TO_CALIBRATION_DATA					"%s>%*f[j1] "
#define MAX_CAL_NAME_LENGTH							50			// Maximum name for non-resonant galvo scan axis calibration data.
#define CALPOINTS 									50
#define SLOPE_OFFSET_DELAY							0.01		// Time to wait in [s] after a galvo step is made before estimating slope and offset parameters					
#define RELERR 										0.05
#define POSSAMPLES									200 		// number of AI samples to read and average for one measurement of the galvo position
#define POSTRAMPTIME								20.0		// recording time in [ms] after a ramp signal is applied
#define MAXFLYBACKTIME								20.0		// maximum expected galvo flyback time in [ms]
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
typedef struct LaserScanning 		LaserScanning_type; 
typedef struct ScanEngine 			ScanEngine_type; 
typedef struct ScanAxisCal			ScanAxisCal_type; 
typedef struct ActiveScanAxisCal 	ActiveScanAxisCal_type; 

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

// Generic active scan axis calibration class

struct ActiveScanAxisCal {
		// DATA
	ScanAxis_type			scanAxisType;
	char*					calName;
	TaskControl_type*		taskController;			// Task Controller for the calibration of this scan axis.
	SourceVChan_type*		VChanCom;   			// VChan used to send command signals to the scan axis (optional, depending on the scan axis type)
	SinkVChan_type*			VChanPos;				// VChan used to receive position feedback signals from the scan axis (optional, depending on the scan axis type)
	double*					comSampRate;			// Command sample rate in [Hz] taken from VChanCom when connected. Same rate is used for the position sampling rate.
	int						calPanHndl;				// Panel handle for performing scan axis calibration
	LaserScanning_type*		lsModule;				// Reference to the laser scanning module to which this calibration belongs.
		// METHODS
	void	(*Discard) (ActiveScanAxisCal_type** scanAxisCal);
};

// Generic scan axis calibration data class

struct ScanAxisCal {
		// DATA 
	ScanAxis_type			scanAxisType;
	char*					calName; 
	LaserScanning_type*		lsModule;
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

// Non-resonant galvo calibration types
typedef enum {
	
	NonResGalvoCal_Slope_Offset,
	NonResGalvoCal_Lag,
	NonResGalvoCal_SwitchTimes,
	NonResGalvoCal_MaxSlopes,
	NonResGalvoCal_TriangleWave
	
} NonResGalvoCalTypes;

// Non-resonant active galvo calibration data structure
typedef struct {
	ActiveScanAxisCal_type	baseClass;				// Base structure describing scan engine calibration. Must be first member of the structure.
	NonResGalvoCalTypes		currentCal;				// Index of the current calibration
	size_t					currIterIdx;			// Current iteration index for each calibration method.
	double					targetSlope;			// Used for triangle waveform calibration, in [V/ms]. 
	double					commandVMin;			// Minimum galvo command voltage in [V] for maximal deflection in a given direction.
	double					commandVMax;			// Maximum galvo command voltage in [V] for maximal deflection in the oposite direction.
	double					extraRuns;				// Number of extra runs for triangle waveform calibration.
	double*					slope;					// in [V/V], together with offset, it relates the command and position feedback signal in a linear way when the galvo is still.
	double*					offset;					// in [V], together with slope, it relates the command and position feedback signal in a linear way when the galvo is still. 
	double*					posStdDev;				// in [V], measures noise level on the position feedback signal.
	double*					lag;					// in [ms]
	size_t					nRepeat;				// Number of times to repeat a command signal.
	size_t					nRampSamples;			// Number of samples within a ramp command signal.
	BOOL					lastRun;				// Marks the last run for lag measurement.
	SwitchTimes_type*		switchTimes;			// Calibration data for jumping between two voltages.
	MaxSlopes_type*			maxSlopes;				// Calibration data for maximum scan speeds.
	TriangleCal_type*		triangleCal;			// Calibration data for triangle waveforms.
	double 					resolution;				// in [V]
	double 					minStepSize;			// in [V]
	double 					parked;         		// Value of command signal to be applied to the galvo when parked, in [V].
	double 					scanTime;				// Time to apply a triangle waveform to estimate galvo response and maximum scan speed in [s].
	DataPacket_type*		commandPacket;			// Used to send command waveforms.
	Waveform_type*			positionSignal;			// Calibration waveforms buffer.
} ActiveNonResGalvoCal_type;

// Non-resonant galvo scan axis calibration data
typedef struct {
	ScanAxisCal_type		baseClass;
	double					commandVMin;			// Minimum galvo command voltage in [V] for maximal deflection in a given direction.   
	double					commandVMax;			// Maximum galvo command voltage in [V] for maximal deflection in the oposite direction.
	double					slope;					// in [V/V], together with offset, it relates the command and position feedback signal in a linear way when the galvo is still.
	double					offset;					// in [V], together with slope, it relates the command and position feedback signal in a linear way when the galvo is still. 
	double					posStdDev;				// in [V], measures noise level on the position feedback signal.
	double					lag;					// in [ms]
	SwitchTimes_type*		switchTimes;			// Calibration data for jumping between two voltages.
	MaxSlopes_type*			maxSlopes;				// Calibration data for maximum scan speeds.
	TriangleCal_type*		triangleCal;			// Calibration data for triangle waveforms.
	double 					resolution;				// in [V]
	double 					minStepSize;			// in [V]
	double 					parked;         		// Value of command signal to be applied to the galvo when parked, in [V].
}NonResGalvoCal_type;

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
	SinkVChan_type*			detVChan;				// Sink VChan for receiving pixel data
	ScanEngine_type*		scanEngine;				// Reference to scan engine to which this detection channel belongs
} DetChan_type;

//------------------
// Scan Engine Class
//------------------
// scan engine child classes
typedef enum {
	ScanEngine_RectRaster
} ScanEngineEnum_type;

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
	SourceVChan_type*		VChanScanOut;
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
		// discard method to be overridden by child class
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
		// Active calibrations that are in progress. Of ActiveScanAxisCal_type* which can be casted to various child classes such as GalvoCal_type*
	ListType				activeCal;
		// Scan engines of ScanEngine_type* base class which can be casted to specific scan engines. 
	ListType				scanEngines;
	
		//-------------------------
		// UI
		//-------------------------
	
	int						mainPanHndl;	  			// Main panel for the laser scanning module.
	int*					mainPanLeftPos;				// Main panel left position to be applied when module is loaded.
	int*					mainPanTopPos;				// Main panel top position to be applied when module is loaded. 
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
static DetChan_type*				init_DetChan_type						(ScanEngine_type* scanEngine, char VChanName[]);

static void							discard_DetChan_type					(DetChan_type** a);

//---------------------------
// VChan management callbacks
//---------------------------
	// detection VChans
static void							DetVChan_Connected						(VChan_type* self, VChan_type* connectedVChan);

static void							DetVChan_Disconnected					(VChan_type* self, VChan_type* disconnectedVChan); 

//----------------------
// Scan axis calibration
//----------------------
	// generic active scan axis calibration data 

static ActiveScanAxisCal_type*		initalloc_ActiveScanAxisCal_type		(ActiveScanAxisCal_type* cal);

static void							discard_ActiveScanAxisCal_type			(ActiveScanAxisCal_type** cal);

	// generic scan axis calibration data
static ScanAxisCal_type*			initalloc_ScanAxisCal_type				(ScanAxisCal_type* cal);

static void							discard_ScanAxisCal_type				(ScanAxisCal_type** cal);

void CVICALLBACK 					ScanAxisCalibrationMenu_CB				(int menuBarHandle, int menuItemID, void *callbackData, int panelHandle);

static void							UpdateScanEngineCalibrations			(ScanEngine_type* scanEngine);

static void 						UpdateAvailableCalibrations 			(LaserScanning_type* lsModule); 

static int CVICALLBACK 				ManageScanAxisCalib_CB					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 				NewScanAxisCalib_CB						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

	// galvo response analysis functions

static int 							FindSlope								(double* signal, int nsamples, double samplingRate, double signalStdDev, int nRep, double sloperelerr, double* maxslope); 
static int 							MeasureLag								(double* signal1, double* signal2, int nelem); 
static int 							GetStepTimes							(double* signal, int nsamples, double lowlevel, double highlevel, double threshold, int* lowidx, int* highidx, int* stepsamples);

	//-----------------------------------------
	// Non-resonant galvo axis calibration data
	//-----------------------------------------

static ActiveNonResGalvoCal_type* 	init_ActiveNonResGalvoCal_type 			(LaserScanning_type* lsModule, char calName[], char commandVChanName[], char positionVChanName[]);

static void							discard_ActiveNonResGalvoCal_type		(ActiveScanAxisCal_type** cal);

static NonResGalvoCal_type*			init_NonResGalvoCal_type				(char calName[], LaserScanning_type* lsModule, double commandVMin, 
																			 double commandVMax, double slope, double offset, double posStdDev, 
																			 double lag, SwitchTimes_type* switchTimes, MaxSlopes_type* maxSlopes,
																			 TriangleCal_type* triangleCal, double resolution, double minStepSize, double parked);

static void							discard_NonResGalvoCal_type				(NonResGalvoCal_type** cal);

	// validation for new scan axis calibration name
static BOOL 						ValidateNewScanAxisCal					(char inputStr[], void* dataPtr); 
	// switch times data
static SwitchTimes_type* 			init_SwitchTimes_type					(void);

static void 						discard_SwitchTimes_type 				(SwitchTimes_type** a);

	// copies switch times data
static SwitchTimes_type*			copy_SwitchTimes_type					(SwitchTimes_type* switchTimes);

	// max slopes data

static MaxSlopes_type* 				init_MaxSlopes_type 					(void);

static void 						discard_MaxSlopes_type 					(MaxSlopes_type** a);
	// copies max slopes data
static MaxSlopes_type* 				copy_MaxSlopes_type 					(MaxSlopes_type* maxSlopes);

	// triangle wave calibration data

static TriangleCal_type* 			init_TriangleCal_type					(void);

static void 						discard_TriangleCal_type 				(TriangleCal_type** a);
	// copies triangle waveform calibration data
static TriangleCal_type* 			copy_TriangleCal_type 					(TriangleCal_type* triangleCal);
	// saves non resonant galvo scan axis calibration data to XML
static int 							SaveNonResGalvoCalToXML					(NonResGalvoCal_type* nrgCal, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_  axisCalibrationsElement);
static int 							LoadNonResGalvoCalFromXML 				(LaserScanning_type* lsModule, ActiveXMLObj_IXMLDOMElement_ axisCalibrationElement);    

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
								 											 char					imageOutVChanName[],
																			 char					detectorVChanName[]);

static void							discard_ScanEngine_type 				(ScanEngine_type** engine);

	// rectangle raster scan

static RectangleRaster_type*		init_RectangleRaster_type				(LaserScanning_type*	lsModule, 
																			 char 					engineName[],
														   					 char 					fastAxisComVChanName[], 
								 											 char 					slowAxisComVChanName[], 
								 											 char					fastAxisPosVChanName[],
								 											 char					slowAxisPosVChanName[],
								 											 char					imageOutVChanName[],
																			 char					detectorVChanName[]);

static void							discard_RectangleRaster_type			(ScanEngine_type** engine);

static int CVICALLBACK 				RectangleRasterScan_CB 					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2); 

//------------------
// Module management
//------------------

static int							Load 									(DAQLabModule_type* mod, int workspacePanHndl);

static int 							LoadCfg 								(DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  moduleElement);

static int 							SaveCfg 								(DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_  moduleElement);

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
	ls->baseClass.LoadCfg			= LoadCfg;
	ls->baseClass.SaveCfg			= SaveCfg;
			
	//---------------------------
	// Child Level 1: LaserScanning_type
	
		// DATA
	
		// init
	ls->availableCals				= 0;
	ls->activeCal					= 0;
	ls->scanEngines					= 0;
		
	if (!(ls->availableCals			= ListCreate(sizeof(ScanAxisCal_type*))))	goto Error;
	if (!(ls->activeCal				= ListCreate(sizeof(ActiveScanAxisCal_type*))))	goto Error;
	if (!(ls->scanEngines			= ListCreate(sizeof(ScanEngine_type*))))	goto Error;
			
		//---
		// UI
		//---
	ls->mainPanHndl					= 0;
	ls->mainPanLeftPos				= NULL;
	ls->mainPanTopPos				= NULL;
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
		ActiveScanAxisCal_type**  calPtr;
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
	OKfree(ls->mainPanLeftPos);
	OKfree(ls->mainPanTopPos);
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
	// set main panel position
	if (ls->mainPanLeftPos)
		SetPanelAttribute(ls->mainPanHndl, ATTR_LEFT, *ls->mainPanLeftPos);
	else
		SetPanelAttribute(ls->mainPanHndl, ATTR_LEFT, VAL_AUTO_CENTER); 
		
	if (ls->mainPanTopPos)
		SetPanelAttribute(ls->mainPanHndl, ATTR_TOP, *ls->mainPanTopPos);
	else
		SetPanelAttribute(ls->mainPanHndl, ATTR_TOP, VAL_AUTO_CENTER); 
	
	
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

static int SaveCfg (DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_  moduleElement)
{
	LaserScanning_type*				ls 				= (LaserScanning_type*) mod;
	int								error			= 0;
	int								lsPanTopPos;
	int								lsPanLeftPos;
	HRESULT							xmlerror;
	ERRORINFO						xmlERRINFO;
	DAQLabXMLNode 					lsAttr[] 		= {{"PanTopPos", BasicData_Int, &lsPanTopPos},
											  		   {"PanLeftPos", BasicData_Int, &lsPanLeftPos}};
	
	//--------------------------------------------------------------------------
	// Save laser scanning module main panel position
	//--------------------------------------------------------------------------
	
	errChk( GetPanelAttribute(ls->mainPanHndl, ATTR_LEFT, &lsPanLeftPos) );
	errChk( GetPanelAttribute(ls->mainPanHndl, ATTR_TOP, &lsPanTopPos) );
	DLAddToXMLElem(xmlDOM, moduleElement, lsAttr, DL_ATTRIBUTE, NumElem(lsAttr)); 
	
	//--------------------------------------------------------------------------
	// Save scan axis calibrations
	//--------------------------------------------------------------------------
	ActiveXMLObj_IXMLDOMElement_	scanAxisCalibrationsXMLElement;   	// element containing multiple scan axis calibrations
	ActiveXMLObj_IXMLDOMElement_	scanAxisCalXMLElement;  		  	// element containing calibration data
	
	// create scan axis calibration element
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, "ScanAxisCalibrations", &scanAxisCalibrationsXMLElement) );
	
	
	size_t					nAxisCals 			= ListNumItems(ls->availableCals);
	ScanAxisCal_type**		axisCalPtr;
	DAQLabXMLNode 			scanAxisCalAttr[2];
	unsigned int			scanAxisType;
	for (size_t i = 1; i <= nAxisCals; i++) {
		axisCalPtr = ListGetPtrToItem(ls->availableCals, i);
		// create new axis calibration element
		XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, "AxisCalibration", &scanAxisCalXMLElement) );
		// initialize generic scan axis calibration attributes
		scanAxisCalAttr[0].tag 		= "Name";
		scanAxisCalAttr[0].type 	= BasicData_CString;
		scanAxisCalAttr[0].pData	= (*axisCalPtr)->calName;
		scanAxisCalAttr[1].tag 		= "AxisType";
		scanAxisType 				= (unsigned int)(*axisCalPtr)->scanAxisType;
		scanAxisCalAttr[1].type 	= BasicData_UInt;
		scanAxisCalAttr[1].pData	= &scanAxisType;
		// save attributes
		DLAddToXMLElem(xmlDOM, scanAxisCalXMLElement, scanAxisCalAttr, DL_ATTRIBUTE, NumElem(scanAxisCalAttr));  
		// add scan axis specific calibration data
		switch((*axisCalPtr)->scanAxisType) {
				
			case NonResonantGalvo:
				SaveNonResGalvoCalToXML((NonResGalvoCal_type*) *axisCalPtr, xmlDOM, scanAxisCalXMLElement);
				break;
				
			case ResonantGalvo:
				break;
				
			case AOD:
				break;
				
			case Translation:
				break;
	
		}
		
		// add new axis calibration element
		XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (scanAxisCalibrationsXMLElement, &xmlERRINFO, scanAxisCalXMLElement, NULL) );  
		CA_DiscardObjHandle(scanAxisCalXMLElement);  
	}
	
	// add scan axis calibrations element to module element
	XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (moduleElement, &xmlERRINFO, scanAxisCalibrationsXMLElement, NULL) );
	CA_DiscardObjHandle(scanAxisCalibrationsXMLElement); 
	
	//--------------------------------------------------------------------------
	// Save scan engines and VChans
	//--------------------------------------------------------------------------
	ActiveXMLObj_IXMLDOMElement_	scanEnginesXMLElement; 			  	// element containing multiple scan engines
	ActiveXMLObj_IXMLDOMElement_	scanEngineXMLElement; 			  	// scan engine element
	ActiveXMLObj_IXMLDOMElement_	VChanNamesXMLElement;	 			// VChan names element      
	
	// create scan engines element
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, "ScanEngines", &scanEnginesXMLElement) );
	
	size_t					nScanEngines 				= ListNumItems(ls->scanEngines);
	ScanEngine_type**		scanEnginePtr;
	DAQLabXMLNode 			scanEngineAttr[4];
	DAQLabXMLNode			rectangleRasterAttr[5];
	unsigned int			scanEngineType;
	unsigned long long		imgHeight, imgWidth, imgHeightOffset, imgWidthOffset;
	char* 					fastAxisCommandVChanName	= NULL;
	char* 					slowAxisCommandVChanName	= NULL;
	char* 					fastAxisPositionVChanName	= NULL;
	char* 					slowAxisPositionVChanName	= NULL;
	char* 					scanEngineOutVChanName		= NULL;
	DAQLabXMLNode			scanEngineVChansAttr[5];
	DAQLabXMLNode			detectorVChanAttr;
	size_t					nDetectionChans;
	DetChan_type**			detChanPtr;
	
	
	for (size_t i = 1; i <= nScanEngines; i++) {
		scanEnginePtr = ListGetPtrToItem(ls->scanEngines, i);
		// create new scan engine element
		XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, "ScanEngine", &scanEngineXMLElement) );
		// create new VChan element with all the VChan names for the Scan Engine
		XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, "VChannels", &VChanNamesXMLElement) );
		// initialize generic scan engine attributes
		scanEngineAttr[0].tag 		= "Name";
		scanEngineAttr[0].type 		= BasicData_CString;
		scanEngineAttr[0].pData		= GetTaskControlName((*scanEnginePtr)->taskControl);
		
		scanEngineAttr[1].tag 		= "ScanEngineType";
		scanEngineType 				= (unsigned int)(*scanEnginePtr)->engineType;
		scanEngineAttr[1].type 		= BasicData_UInt;
		scanEngineAttr[1].pData		= &scanEngineType;
		
		scanEngineAttr[2].tag		= "FastAxisCalibrationName";
		scanEngineAttr[2].type 		= BasicData_CString;
		if ((*scanEnginePtr)->fastAxisCal)
			scanEngineAttr[2].pData	= (*scanEnginePtr)->fastAxisCal->calName;
		else
			scanEngineAttr[2].pData = "";
		
		scanEngineAttr[3].tag		= "SlowAxisCalibrationName";
		scanEngineAttr[3].type 		= BasicData_CString;
		if ((*scanEnginePtr)->slowAxisCal)
			scanEngineAttr[3].pData		= (*scanEnginePtr)->slowAxisCal->calName;
		else
			scanEngineAttr[3].pData		= ""; 
			
		// save attributes
		DLAddToXMLElem(xmlDOM, scanEngineXMLElement, scanEngineAttr, DL_ATTRIBUTE, NumElem(scanEngineAttr));
		OKfree(scanEngineAttr[0].pData);
		
		// save generic scan engine VChans:scan axis & scan engine out 
		fastAxisCommandVChanName 	= GetVChanName((VChan_type*)(*scanEnginePtr)->VChanFastAxisCom);
		slowAxisCommandVChanName 	= GetVChanName((VChan_type*)(*scanEnginePtr)->VChanSlowAxisCom);
		fastAxisPositionVChanName 	= GetVChanName((VChan_type*)(*scanEnginePtr)->VChanFastAxisPos);
		slowAxisPositionVChanName 	= GetVChanName((VChan_type*)(*scanEnginePtr)->VChanSlowAxisPos);
		scanEngineOutVChanName		= GetVChanName((VChan_type*)(*scanEnginePtr)->VChanScanOut);
				
		scanEngineVChansAttr[0].tag 	= "FastAxisCommand";
		scanEngineVChansAttr[0].type   = BasicData_CString;
		scanEngineVChansAttr[0].pData	= fastAxisCommandVChanName;
		scanEngineVChansAttr[1].tag 	= "SlowAxisCommand";
		scanEngineVChansAttr[1].type   = BasicData_CString;
		scanEngineVChansAttr[1].pData	= slowAxisCommandVChanName;
		scanEngineVChansAttr[2].tag 	= "FastAxisPosition";
		scanEngineVChansAttr[2].type   = BasicData_CString;
		scanEngineVChansAttr[2].pData	= fastAxisPositionVChanName;
		scanEngineVChansAttr[3].tag 	= "SlowAxisPosition";
		scanEngineVChansAttr[3].type   = BasicData_CString;
		scanEngineVChansAttr[3].pData	= slowAxisPositionVChanName;
		scanEngineVChansAttr[4].tag 	= "ScanEngineOut";
		scanEngineVChansAttr[4].type   = BasicData_CString;
		scanEngineVChansAttr[4].pData	= scanEngineOutVChanName;
														 	
		DLAddToXMLElem(xmlDOM, VChanNamesXMLElement, scanEngineVChansAttr, DL_ATTRIBUTE, NumElem(scanEngineVChansAttr));
				
		OKfree(fastAxisCommandVChanName);
		OKfree(slowAxisCommandVChanName);
		OKfree(fastAxisPositionVChanName);
		OKfree(slowAxisPositionVChanName);
		OKfree(scanEngineOutVChanName);
		
		// save detector VChans
		nDetectionChans = ListNumItems((*scanEnginePtr)->DetChans);
		for (size_t j = 0; j < nDetectionChans; j++) {
			detChanPtr = ListGetPtrToItem((*scanEnginePtr)->DetChans, j+1);
			detectorVChanAttr.tag 		= "DetectorChannel";
			detectorVChanAttr.type   	= BasicData_CString;
			detectorVChanAttr.pData		= GetVChanName((VChan_type*)(*detChanPtr)->detVChan);
			DLAddToXMLElem(xmlDOM, VChanNamesXMLElement, &detectorVChanAttr, DL_ELEMENT, 1);
			OKfree(detectorVChanAttr.pData);
		}
		
		// add scan engine specific data
		switch((*scanEnginePtr)->engineType) {
				
			case ScanEngine_RectRaster:
				
				// initialize raster scan attributes
				imgHeight 						= ((RectangleRaster_type*) *scanEnginePtr)->height;
				imgWidth 						= ((RectangleRaster_type*) *scanEnginePtr)->width;
				imgHeightOffset 				= ((RectangleRaster_type*) *scanEnginePtr)->heightOffset;
				imgWidthOffset					= ((RectangleRaster_type*) *scanEnginePtr)->widthOffset;
				
				rectangleRasterAttr[0].tag		= "ImageHeight";
				rectangleRasterAttr[0].type		= BasicData_ULongLong;
				rectangleRasterAttr[0].pData	= &imgHeight;
				rectangleRasterAttr[1].tag		= "ImageWidth";
				rectangleRasterAttr[1].type		= BasicData_ULongLong;
				rectangleRasterAttr[1].pData	= &imgWidth;
				rectangleRasterAttr[2].tag		= "ImageHeightOffset";
				rectangleRasterAttr[2].type		= BasicData_ULongLong;
				rectangleRasterAttr[2].pData	= &imgHeightOffset;
				rectangleRasterAttr[3].tag		= "ImageWidthOffset";
				rectangleRasterAttr[3].type		= BasicData_ULongLong;
				rectangleRasterAttr[3].pData	= &imgWidthOffset;
				rectangleRasterAttr[4].tag		= "PixelSize";
				rectangleRasterAttr[4].type		= BasicData_Double;
				rectangleRasterAttr[4].pData	= &((RectangleRaster_type*) *scanEnginePtr)->pixSize;
				
				// save scan engine attributes
				DLAddToXMLElem(xmlDOM, scanEngineXMLElement, rectangleRasterAttr, DL_ATTRIBUTE, NumElem(rectangleRasterAttr));
				
				
				
				break;
		}
		// add VChan Names element to scan engine element
		// add new scan engine element
		XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (scanEngineXMLElement, &xmlERRINFO, VChanNamesXMLElement, NULL) ); 
		CA_DiscardObjHandle(VChanNamesXMLElement);   
		
		// add new scan engine element
		XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (scanEnginesXMLElement, &xmlERRINFO, scanEngineXMLElement, NULL) );  
		CA_DiscardObjHandle(scanEngineXMLElement); 
	}
	
	// add scan engines element to module element
	XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (moduleElement, &xmlERRINFO, scanEnginesXMLElement, NULL) );
	CA_DiscardObjHandle(scanEnginesXMLElement); 
	
	return 0;
	
Error:
	
	return error;
	
XMLError:   
	
	return xmlerror;
}

static int LoadCfg (DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  moduleElement)
{
	LaserScanning_type*				ls 								= (LaserScanning_type*) mod;
	int 							error 							= 0;
	HRESULT							xmlerror						= 0;
	ERRORINFO						xmlERRINFO;
	ls->mainPanTopPos												= malloc(sizeof(int));
	ls->mainPanLeftPos												= malloc(sizeof(int));
	DAQLabXMLNode 					lsAttr[] 						= { {"PanTopPos", BasicData_Int, ls->mainPanTopPos},
											  		   					{"PanLeftPos", BasicData_Int, ls->mainPanLeftPos}};
													   
	ActiveXMLObj_IXMLDOMElement_	scanAxisCalibrationsXMLElement 	= 0;   			// element containing multiple scan axis calibrations
	ActiveXMLObj_IXMLDOMElement_	scanAxisCalXMLElement			= 0;  		  	// element containing calibration data 
	ActiveXMLObj_IXMLDOMNodeList_	axisCalibrationNodeList			= 0;
	
	// load main panel position 
	errChk( DLGetXMLElementAttributes(moduleElement, lsAttr, NumElem(lsAttr)) ); 
	
	//--------------------------------------------------------------------------
	// Load scan axis calibrations
	//--------------------------------------------------------------------------

	ActiveXMLObj_IXMLDOMNode_		axisCalibrationNode				= 0;      
	long							nAxisCalibrations;
	unsigned int					axisCalibrationType;
	DAQLabXMLNode					axisCalibrationGenericAttr[] = {{"AxisType", BasicData_UInt, &axisCalibrationType}};
																	 
	errChk( DLGetSingleXMLElementFromElement(moduleElement, "ScanAxisCalibrations", &scanAxisCalibrationsXMLElement) );
	
	XMLErrChk ( ActiveXML_IXMLDOMElement_getElementsByTagName(scanAxisCalibrationsXMLElement, &xmlERRINFO, "AxisCalibration", &axisCalibrationNodeList) );
	XMLErrChk ( ActiveXML_IXMLDOMNodeList_Getlength(axisCalibrationNodeList, &xmlERRINFO, &nAxisCalibrations) );
	
	for (long i = 0; i < nAxisCalibrations; i++) {
		XMLErrChk ( ActiveXML_IXMLDOMNodeList_Getitem(axisCalibrationNodeList, &xmlERRINFO, i, &axisCalibrationNode) );
		
		errChk( DLGetXMLNodeAttributes(axisCalibrationNode, axisCalibrationGenericAttr, NumElem(axisCalibrationGenericAttr)) ); 
		
		switch (axisCalibrationType) {
				
			case NonResonantGalvo:
				LoadNonResGalvoCalFromXML(ls, (ActiveXMLObj_IXMLDOMElement_)axisCalibrationNode);   
				break;
				
			case ResonantGalvo:
				break;
				
			case AOD:
				break;
				
			case Translation:
				break;
		}
		
	}
	
	//--------------------------------------------------------------------------
	// Load scan engines
	//--------------------------------------------------------------------------
	ActiveXMLObj_IXMLDOMElement_	scanEnginesXMLElement 	= 0;   			// element containing multiple scan engines
	ActiveXMLObj_IXMLDOMElement_	scanEngineXMLElement	= 0;  		  	// element containing scan engine data 
	ActiveXMLObj_IXMLDOMNodeList_	scanEngineNodeList		= 0;
	ActiveXMLObj_IXMLDOMNode_		scanEngineNode			= 0;    
	long							nScanEngines;
	unsigned int					scanEngineType;
	char*							scanEngineName			= NULL;
	char*							fastAxisCalibrationName	= NULL;
	char*							slowAxisCalibrationName	= NULL;
	ScanEngine_type*				scanEngine				= NULL;
	DAQLabXMLNode					scanEngineGenericAttr[] = {	{"Name", BasicData_CString, &scanEngineName},
																{"ScanEngineType", BasicData_UInt, &scanEngineType},
																{"FastAxisCalibrationName", BasicData_CString, &fastAxisCalibrationName},
																{"SlowAxisCalibrationName", BasicData_CString, &slowAxisCalibrationName} }; 			
		
	errChk( DLGetSingleXMLElementFromElement(moduleElement, "ScanEngines", &scanEnginesXMLElement) );
	
	XMLErrChk ( ActiveXML_IXMLDOMElement_getElementsByTagName(scanEnginesXMLElement, &xmlERRINFO, "ScanEngine", &scanEngineNodeList) );
	XMLErrChk ( ActiveXML_IXMLDOMNodeList_Getlength(scanEngineNodeList, &xmlERRINFO, &nScanEngines) );
	
	for (long i = 0; i < nScanEngines; i++) {
		XMLErrChk ( ActiveXML_IXMLDOMNodeList_Getitem(scanEngineNodeList, &xmlERRINFO, i, &scanEngineNode) );
		
		errChk( DLGetXMLNodeAttributes(scanEngineNode, scanEngineGenericAttr, NumElem(scanEngineGenericAttr)) ); 
		
		switch (scanEngineType) {
				
			case ScanEngine_RectRaster:
				
				scanEngine = init_RectangleRaster_type((LaserScanning_type*)mod, scanEngineName, 
				break;
			
		}
	}
	
	
XMLError:
	
	error = xmlerror;
	
Error:
	
	if (scanAxisCalibrationsXMLElement) CA_DiscardObjHandle(scanAxisCalibrationsXMLElement);
	if (scanAxisCalXMLElement) CA_DiscardObjHandle(scanAxisCalXMLElement); 
	if (axisCalibrationNodeList) CA_DiscardObjHandle(axisCalibrationNodeList); 
	
	return error;	
}

static int SaveNonResGalvoCalToXML	(NonResGalvoCal_type* nrgCal, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_  axisCalibrationsElement)
{
#define SaveNonResGalvoCalToXML_Err_OutOfMemory		-1
	int								error 				= 0;
	HRESULT							xmlerror;
	ERRORINFO						xmlERRINFO;
	ActiveXMLObj_IXMLDOMElement_	switchTimesXMLElement; 			  
	ActiveXMLObj_IXMLDOMElement_	maxSlopesXMLElement;   		
	ActiveXMLObj_IXMLDOMElement_	triangleCalXMLElement;
	DAQLabXMLNode 					nrgCalAttr[] 		= {	{"CommandVMin", BasicData_Double, &nrgCal->commandVMin},
											  		   		{"CommandVMax", BasicData_Double, &nrgCal->commandVMax},
															{"CommandAsFunctionOfPositionLinFitSlope", BasicData_Double, &nrgCal->slope},
															{"CommandAsFunctionOfPositionLinFitOffset", BasicData_Double, &nrgCal->offset},
															{"PositionStdDev", BasicData_Double, &nrgCal->posStdDev},
															{"ResponseLag", BasicData_Double, &nrgCal->lag},
															{"Resolution", BasicData_Double, &nrgCal->resolution},
															{"MinStepSize", BasicData_Double, &nrgCal->minStepSize},
															{"ParkedCommandV", BasicData_Double, &nrgCal->parked} };
	
	// Save calibration attributes
	DLAddToXMLElem(xmlDOM, axisCalibrationsElement, nrgCalAttr, DL_ATTRIBUTE, NumElem(nrgCalAttr)); 
	
	// create calibration elements
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, "SwitchTimes", &switchTimesXMLElement) );
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, "MaxSlopes", &maxSlopesXMLElement) );
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, "TriangleWaveform", &triangleCalXMLElement) );
	
	char*	switchTimesStepSizeStr 				= NULL;
	char*	switchTimesHalfSwitchStr			= NULL;
	char*	maxSlopesSlopeStr					= NULL;
	char*	maxSlopesAmplitudeStr				= NULL;
	char*	triangleCalCommandAmplitudeStr		= NULL;
	char*	triangleCalActualAmplitudeStr		= NULL;
	char*	triangleCalMaxFreqStr				= NULL;
	char*	triangleCalResiduaLagStr			= NULL;
	
	// convert switch times calibration data to string
	switchTimesStepSizeStr = malloc (nrgCal->switchTimes->n * MAX_DOUBLE_NCHARS * sizeof(char)+1);
	if (!switchTimesStepSizeStr) {error = SaveNonResGalvoCalToXML_Err_OutOfMemory; goto Error;}
	switchTimesHalfSwitchStr = malloc (nrgCal->switchTimes->n * MAX_DOUBLE_NCHARS * sizeof(char)+1);
	if (!switchTimesHalfSwitchStr) {error = SaveNonResGalvoCalToXML_Err_OutOfMemory; goto Error;} 
	
	Fmt(switchTimesStepSizeStr, CALIBRATION_DATA_TO_STRING, nrgCal->switchTimes->n, nrgCal->switchTimes->stepSize); 
	Fmt(switchTimesHalfSwitchStr, CALIBRATION_DATA_TO_STRING, nrgCal->switchTimes->n, nrgCal->switchTimes->halfSwitch);
	
	// convert max slopes calibration data to string
	maxSlopesSlopeStr = malloc (nrgCal->maxSlopes->n * MAX_DOUBLE_NCHARS * sizeof(char)+1);
	if (!maxSlopesSlopeStr) {error = SaveNonResGalvoCalToXML_Err_OutOfMemory; goto Error;}   
	maxSlopesAmplitudeStr = malloc (nrgCal->maxSlopes->n * MAX_DOUBLE_NCHARS * sizeof(char)+1);
	if (!maxSlopesAmplitudeStr) {error = SaveNonResGalvoCalToXML_Err_OutOfMemory; goto Error;} 
	
	Fmt(maxSlopesSlopeStr, CALIBRATION_DATA_TO_STRING, nrgCal->maxSlopes->n, nrgCal->maxSlopes->slope); 
	Fmt(maxSlopesAmplitudeStr, CALIBRATION_DATA_TO_STRING, nrgCal->maxSlopes->n, nrgCal->maxSlopes->amplitude);
	
	// convert triangle waveform calibration data to string
	triangleCalCommandAmplitudeStr = malloc (nrgCal->triangleCal->n * MAX_DOUBLE_NCHARS * sizeof(char)+1);
	if (!triangleCalCommandAmplitudeStr) {error = SaveNonResGalvoCalToXML_Err_OutOfMemory; goto Error;}  
	triangleCalActualAmplitudeStr = malloc (nrgCal->triangleCal->n * MAX_DOUBLE_NCHARS * sizeof(char)+1);
	if (!triangleCalActualAmplitudeStr) {error = SaveNonResGalvoCalToXML_Err_OutOfMemory; goto Error;} 
	triangleCalMaxFreqStr = malloc (nrgCal->triangleCal->n * MAX_DOUBLE_NCHARS * sizeof(char)+1);
	if (!triangleCalMaxFreqStr) {error = SaveNonResGalvoCalToXML_Err_OutOfMemory; goto Error;} 
	triangleCalResiduaLagStr = malloc (nrgCal->triangleCal->n * MAX_DOUBLE_NCHARS * sizeof(char)+1);
	if (!triangleCalResiduaLagStr) {error = SaveNonResGalvoCalToXML_Err_OutOfMemory; goto Error;} 
	
	Fmt(triangleCalCommandAmplitudeStr, CALIBRATION_DATA_TO_STRING, nrgCal->triangleCal->n, nrgCal->triangleCal->commandAmp); 
	Fmt(triangleCalActualAmplitudeStr, CALIBRATION_DATA_TO_STRING, nrgCal->triangleCal->n, nrgCal->triangleCal->actualAmp);
	Fmt(triangleCalMaxFreqStr, CALIBRATION_DATA_TO_STRING, nrgCal->triangleCal->n, nrgCal->triangleCal->maxFreq); 
	Fmt(triangleCalResiduaLagStr, CALIBRATION_DATA_TO_STRING, nrgCal->triangleCal->n, nrgCal->triangleCal->resLag);
	
	// add calibration data to XML elements
	DAQLabXMLNode 			switchTimesAttr[] 		= {	{"StepSize", BasicData_CString, switchTimesStepSizeStr},
														{"HalfSwitchTime", BasicData_CString, switchTimesHalfSwitchStr} };
														
	DAQLabXMLNode 			maxSlopesAttr[] 		= {	{"Slope", BasicData_CString, maxSlopesSlopeStr},
														{"Amplitude", BasicData_CString, maxSlopesAmplitudeStr} };
														
	DAQLabXMLNode 			triangleCalAttr[] 		= {	{"DeadTime", BasicData_Double, &nrgCal->triangleCal->deadTime},
														{"CommandAmplitude", BasicData_CString, triangleCalCommandAmplitudeStr},
														{"ActualAmplitude", BasicData_CString, triangleCalActualAmplitudeStr},
														{"MaxFrequency", BasicData_CString, triangleCalMaxFreqStr},
														{"ResidualLag", BasicData_CString, triangleCalResiduaLagStr} };
	
	DLAddToXMLElem(xmlDOM, switchTimesXMLElement, switchTimesAttr, DL_ATTRIBUTE, NumElem(switchTimesAttr));
	DLAddToXMLElem(xmlDOM, maxSlopesXMLElement, maxSlopesAttr, DL_ATTRIBUTE, NumElem(maxSlopesAttr));
	DLAddToXMLElem(xmlDOM, triangleCalXMLElement, triangleCalAttr, DL_ATTRIBUTE, NumElem(triangleCalAttr));
								
	// add calibration elements to scan axis calibration element
	XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (axisCalibrationsElement, &xmlERRINFO, switchTimesXMLElement, NULL) );
	CA_DiscardObjHandle(switchTimesXMLElement); 
	XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (axisCalibrationsElement, &xmlERRINFO, maxSlopesXMLElement, NULL) );
	CA_DiscardObjHandle(maxSlopesXMLElement);
	XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (axisCalibrationsElement, &xmlERRINFO, triangleCalXMLElement, NULL) );
	CA_DiscardObjHandle(triangleCalXMLElement); 
	
	OKfree(switchTimesStepSizeStr);
	OKfree(switchTimesHalfSwitchStr);
	OKfree(maxSlopesSlopeStr);
	OKfree(maxSlopesAmplitudeStr);
	OKfree(triangleCalCommandAmplitudeStr);
	OKfree(triangleCalActualAmplitudeStr);
	
	return 0;
	
Error:
	
	OKfree(switchTimesStepSizeStr);
	OKfree(switchTimesHalfSwitchStr);
	OKfree(maxSlopesSlopeStr);
	OKfree(maxSlopesAmplitudeStr);
	OKfree(triangleCalCommandAmplitudeStr);
	OKfree(triangleCalActualAmplitudeStr);
	
	return error;
	
XMLError:   
	
	return xmlerror;
	
}

static int LoadNonResGalvoCalFromXML (LaserScanning_type* lsModule, ActiveXMLObj_IXMLDOMElement_ axisCalibrationElement)
{
#define SaveNonResGalvoCalToXML_Err_OutOfMemory		-1 
	int 							error 							= 0;
	HRESULT							xmlerror						= 0;
	ERRORINFO						xmlERRINFO;
	char*							axisCalibrationName				= NULL;
	double							commandVMin;
	double							commandVMax;
	double							slope;
	double							offset;
	double							posStdDev;
	double							lag;
	double							resolution;
	double							minStepSize;
	double							parked;
	ActiveXMLObj_IXMLDOMElement_	switchTimesXMLElement			= 0; 			  
	ActiveXMLObj_IXMLDOMElement_	maxSlopesXMLElement				= 0;   		
	ActiveXMLObj_IXMLDOMElement_	triangleCalXMLElement			= 0;
	DAQLabXMLNode					axisCalibrationAttr[] 			= { {"Name", BasicData_CString, &axisCalibrationName},
																		{"CommandVMin", BasicData_Double, &commandVMin},
											  		   					{"CommandVMax", BasicData_Double, &commandVMax},
																		{"CommandAsFunctionOfPositionLinFitSlope", BasicData_Double, &slope},
																		{"CommandAsFunctionOfPositionLinFitOffset", BasicData_Double, &offset},
																		{"PositionStdDev", BasicData_Double, &posStdDev},
																		{"ResponseLag", BasicData_Double, &lag},
																		{"Resolution", BasicData_Double, &resolution},
																		{"MinStepSize", BasicData_Double, &minStepSize},
																		{"ParkedCommandV", BasicData_Double, &parked} };
	
	errChk( DLGetXMLElementAttributes(axisCalibrationElement, axisCalibrationAttr, NumElem(axisCalibrationAttr)) ); 
	
	errChk( DLGetSingleXMLElementFromElement(axisCalibrationElement, "SwitchTimes", &switchTimesXMLElement) ); 
	errChk( DLGetSingleXMLElementFromElement(axisCalibrationElement, "MaxSlopes", &maxSlopesXMLElement) );
	errChk( DLGetSingleXMLElementFromElement(axisCalibrationElement, "TriangleWaveform", &triangleCalXMLElement) );
	
	char*							switchTimesStepSizeStr 			= NULL;
	char*							switchTimesHalfSwitchStr		= NULL;
	unsigned int					nSwitchTimes;
	char*							maxSlopesSlopeStr				= NULL;
	char*							maxSlopesAmplitudeStr			= NULL;
	unsigned int					nMaxSlopes;
	char*							triangleCalCommandAmplitudeStr	= NULL;
	char*							triangleCalActualAmplitudeStr	= NULL;
	char*							triangleCalMaxFreqStr			= NULL;
	char*							triangleCalResiduaLagStr		= NULL;
	unsigned int					nTriangleCal;
	double							deadTime;
	
	// get calibration data from XML elements
	DAQLabXMLNode 					switchTimesAttr[] 				= {	{"NElements", BasicData_UInt, &nSwitchTimes},
																		{"StepSize", BasicData_CString, &switchTimesStepSizeStr},
																		{"HalfSwitchTime", BasicData_CString, &switchTimesHalfSwitchStr} };
														
	DAQLabXMLNode 					maxSlopesAttr[] 				= {	{"NElements", BasicData_UInt, &nMaxSlopes},
																		{"Slope", BasicData_CString, &maxSlopesSlopeStr},
																		{"Amplitude", BasicData_CString, &maxSlopesAmplitudeStr} };
														
	DAQLabXMLNode 					triangleCalAttr[] 				= {	{"NElements", BasicData_UInt, &nTriangleCal}, 
																		{"DeadTime", BasicData_Double, &deadTime},
																		{"CommandAmplitude", BasicData_CString, &triangleCalCommandAmplitudeStr},
																		{"ActualAmplitude", BasicData_CString, &triangleCalActualAmplitudeStr},
																		{"MaxFrequency", BasicData_CString, &triangleCalMaxFreqStr},
																		{"ResidualLag", BasicData_CString, &triangleCalResiduaLagStr} };
	
	// switch times
	errChk( DLGetXMLElementAttributes(switchTimesXMLElement, switchTimesAttr, NumElem(switchTimesAttr)) );
	CA_DiscardObjHandle(switchTimesXMLElement);
	
	SwitchTimes_type*	switchTimes = init_SwitchTimes_type();
	
	switchTimes->n 				= nSwitchTimes;
	switchTimes->halfSwitch		= malloc(nSwitchTimes * sizeof(double));
	switchTimes->stepSize		= malloc(nSwitchTimes * sizeof(double));
	
	Scan(switchTimesStepSizeStr, STRING_TO_CALIBRATION_DATA, switchTimes->n, switchTimes->stepSize);
	Scan(switchTimesHalfSwitchStr, STRING_TO_CALIBRATION_DATA, switchTimes->n, switchTimes->halfSwitch);  
	
	// max slopes
	errChk( DLGetXMLElementAttributes(maxSlopesXMLElement, maxSlopesAttr, NumElem(maxSlopesAttr)) );
	CA_DiscardObjHandle(maxSlopesXMLElement);
	
	MaxSlopes_type*		maxSlopes = init_MaxSlopes_type();
	
	maxSlopes->n 				= nMaxSlopes;
	maxSlopes->amplitude		= malloc(nMaxSlopes * sizeof(double));
	maxSlopes->slope			= malloc(nMaxSlopes * sizeof(double));
	
	Scan(maxSlopesSlopeStr, STRING_TO_CALIBRATION_DATA, maxSlopes->n , maxSlopes->slope);
	Scan(maxSlopesAmplitudeStr, STRING_TO_CALIBRATION_DATA, maxSlopes->n , maxSlopes->amplitude);  
	
	// triangle waveform calibration
	errChk( DLGetXMLElementAttributes(triangleCalXMLElement, triangleCalAttr, NumElem(triangleCalAttr)) );
	CA_DiscardObjHandle(triangleCalXMLElement);
	
	TriangleCal_type*	triangleCal = init_TriangleCal_type();
	
	triangleCal->n				= nTriangleCal;
	triangleCal->actualAmp		= malloc(triangleCal->n * sizeof(double));
	triangleCal->commandAmp		= malloc(triangleCal->n * sizeof(double));
	triangleCal->maxFreq		= malloc(triangleCal->n * sizeof(double));
	triangleCal->resLag			= malloc(triangleCal->n * sizeof(double));
	triangleCal->deadTime		= deadTime;
	
	Scan(triangleCalCommandAmplitudeStr, STRING_TO_CALIBRATION_DATA, triangleCal->n, triangleCal->commandAmp);
	Scan(triangleCalActualAmplitudeStr, STRING_TO_CALIBRATION_DATA, triangleCal->n, triangleCal->actualAmp);
	Scan(triangleCalMaxFreqStr, STRING_TO_CALIBRATION_DATA, triangleCal->n, triangleCal->maxFreq);
	Scan(triangleCalResiduaLagStr, STRING_TO_CALIBRATION_DATA, triangleCal->n, triangleCal->resLag);  
	
	
	// collect calibration data																	
	NonResGalvoCal_type*		nrgCal = init_NonResGalvoCal_type(axisCalibrationName, lsModule, commandVMin, commandVMax, slope, offset,
									     posStdDev, lag, switchTimes, maxSlopes, triangleCal, resolution, minStepSize, parked);  
	
	// add calibration data to module
	ListInsertItem(lsModule->availableCals, &nrgCal, END_OF_LIST);
	
	
	
XMLError:
	
	error = xmlerror;
	
Error:
	
	// clean up
	OKfree(axisCalibrationName);
	OKfree(switchTimesStepSizeStr);
	OKfree(switchTimesHalfSwitchStr);
	OKfree(maxSlopesSlopeStr);
	OKfree(maxSlopesAmplitudeStr);
	OKfree(triangleCalCommandAmplitudeStr);
	OKfree(triangleCalActualAmplitudeStr);
	OKfree(triangleCalMaxFreqStr);
	OKfree(triangleCalResiduaLagStr);
	
	if (switchTimesXMLElement) CA_DiscardObjHandle(switchTimesXMLElement);
	if (maxSlopesXMLElement) CA_DiscardObjHandle(maxSlopesXMLElement); 
	if (triangleCalXMLElement) CA_DiscardObjHandle(triangleCalXMLElement);   
	
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
	ActiveScanAxisCal_type**		calPtr;
	
	
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
	char* VChanScanOutName 		= GetVChanName((VChan_type*)engine->VChanScanOut); 
	
	// update VChans from data structure
	if (engine->VChanFastAxisCom)
		SetCtrlVal(engine->engineSetPanHndl, ScanSetPan_FastAxisCommand, VChanFastAxisComName);
	
	if (engine->VChanSlowAxisCom)
		SetCtrlVal(engine->engineSetPanHndl, ScanSetPan_SlowAxisCommand, VChanSlowAxisComName);
	
	if (engine->VChanFastAxisPos)
		SetCtrlVal(engine->engineSetPanHndl, ScanSetPan_FastAxisPosition, VChanFastAxisPosName);
	
	if (engine->VChanSlowAxisPos)
		SetCtrlVal(engine->engineSetPanHndl, ScanSetPan_SlowAxisPosition, VChanSlowAxisPosName);
		
	if (engine->VChanScanOut)
		SetCtrlVal(engine->engineSetPanHndl, ScanSetPan_ImageOut, VChanScanOutName);
	
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
	UpdateScanEngineCalibrations (engine);
	
	
	
	
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
							char*	detectionVChanName		= DLGetUniqueVChanName(VChan_Default_DetectionChan);
					
							newScanEngine = (ScanEngine_type*) init_RectangleRaster_type(ls, engineName, fastAxisComVChanName, slowAxisComVChanName,
														fastAxisPosVChanName, slowAxisPosVChanName, imageOutVChanName, detectionVChanName);
					
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
					DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)newScanEngine->VChanScanOut);
					DetChan_type**	detPtr = ListGetPtrToItem(newScanEngine->DetChans, 1);
					DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)(*detPtr)->detVChan);   
					
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

static void	UpdateScanEngineCalibrations (ScanEngine_type* scanEngine)
{
	size_t					nCal 		= ListNumItems(scanEngine->lsModule->availableCals);
	ScanAxisCal_type**		calPtr;
	unsigned int 			fastAxisType;
	unsigned int			slowAxisType;
					
	
	if (!scanEngine->engineSetPanHndl) return; // do nothing if there is no pannel
	
	// empty lists, insert empty selection and select by default
	GetCtrlVal(scanEngine->engineSetPanHndl, ScanSetPan_FastAxisType, &fastAxisType);
	GetCtrlVal(scanEngine->engineSetPanHndl, ScanSetPan_SlowAxisType, &slowAxisType);  
	DeleteListItem(scanEngine->engineSetPanHndl, ScanSetPan_FastAxisCal, 0, -1);
	DeleteListItem(scanEngine->engineSetPanHndl, ScanSetPan_SlowAxisCal, 0, -1);
	InsertListItem(scanEngine->engineSetPanHndl, ScanSetPan_FastAxisCal, -1,"", 0);  
	InsertListItem(scanEngine->engineSetPanHndl, ScanSetPan_SlowAxisCal, -1,"", 0); 
	SetCtrlIndex(scanEngine->engineSetPanHndl, ScanSetPan_FastAxisCal, 0);
	SetCtrlIndex(scanEngine->engineSetPanHndl, ScanSetPan_SlowAxisCal, 0);
	
	for (size_t i = 1; i <= nCal; i++) {
		calPtr = ListGetPtrToItem(scanEngine->lsModule->availableCals, i);
		// fast axis calibration list
		if (((*calPtr)->scanAxisType == fastAxisType) && (*calPtr != scanEngine->slowAxisCal)) 
			InsertListItem(scanEngine->engineSetPanHndl, ScanSetPan_FastAxisCal, -1, (*calPtr)->calName, i);
		// select fast axis list item if calibration is assigned to the scan engine
		if (scanEngine->fastAxisCal == *calPtr)
			SetCtrlIndex(scanEngine->engineSetPanHndl, ScanSetPan_FastAxisCal, i);
		// slow axis calibration list
		if (((*calPtr)->scanAxisType == slowAxisType) && (*calPtr != scanEngine->fastAxisCal)) 
			InsertListItem(scanEngine->engineSetPanHndl, ScanSetPan_SlowAxisCal, -1, (*calPtr)->calName, i);
		// select fast axis list item if calibration is assigned to the scan engine
		if (scanEngine->slowAxisCal == *calPtr)
			SetCtrlIndex(scanEngine->engineSetPanHndl, ScanSetPan_SlowAxisCal, i);
	} 
}

static void UpdateAvailableCalibrations (LaserScanning_type* lsModule)
{
	size_t					nCal 		= ListNumItems(lsModule->availableCals);
	ScanAxisCal_type**		calPtr;
	
	if (!lsModule->manageAxisCalPanHndl) return; // do nothing if there is no panel
	
	// empty listbox
	DeleteListItem(lsModule->manageAxisCalPanHndl, ManageAxis_AxisCalibList, 0, -1);
	
	// list available calibrations
	for (size_t i = 1; i <= nCal; i++) {
		calPtr = ListGetPtrToItem(lsModule->availableCals, i);
		InsertListItem(lsModule->manageAxisCalPanHndl, ManageAxis_AxisCalibList, -1, (*calPtr)->calName, i);
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
							ActiveNonResGalvoCal_type* 	nrgCal = init_ActiveNonResGalvoCal_type(ls, calTCName, commandVChanName, positionVChanName);
							
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

/// HIFN Finds the maximum slope in [V/ms] from a given galvo response.
/// HIRET Returns 0 if slope cannot be determined given required accuracy (RELERR) and 1 otherwise.
static int FindSlope(double* signal, int nsamples, double samplingRate, double signalStdDev, int nRep, double sloperelerr, double* maxslope)
{
	double 	tmpslope;
	double 	relslope_err;
	BOOL 	foundmaxslope = 0;
			
	for (size_t nslope_elem = 1; nslope_elem < nsamples; nslope_elem++){  
			
		*maxslope = 0;   // in [V/ms]
		for (size_t i = 0; i < (nsamples - nslope_elem); i++) {
			tmpslope = (signal[nslope_elem+i] - signal[i])/(nslope_elem*1000/samplingRate); // slope in [V/ms]
			if (tmpslope > *maxslope) *maxslope = tmpslope;
		}
		// calculate relative maximum slope error
		relslope_err = fabs(signalStdDev/sqrt(nRep) * 1.414 / (*maxslope * nslope_elem * 1000/samplingRate));
		
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
static DetChan_type* init_DetChan_type (ScanEngine_type* scanEngine, char VChanName[])
{
	DetChan_type* det = malloc(sizeof(DetChan_type));
	if (!det) return NULL;
	
	DLDataTypes allowedPacketTypes[] = Allowed_Detector_Data_Types;
	
	det->detVChan 	= init_SinkVChan_type(VChanName, allowedPacketTypes, NumElem(allowedPacketTypes), det, DetVChan_Connected, DetVChan_Disconnected);
	det->scanEngine = scanEngine;
	
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
					SetVChanName((VChan_type*)engine->VChanScanOut, newName);
					break;
					
				case ScanSetPan_AddImgChan:
					
					// create new VChan with a predefined base name
					newName = DLGetUniqueVChanName(VChan_Default_DetectionChan);
					DetChan_type* detChan = init_DetChan_type(engine, newName);
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
				case ScanSetPan_SlowAxisType:
					
					UpdateScanEngineCalibrations(engine); 
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
	ActiveNonResGalvoCal_type*		activeNRGCal = callbackData;
	
	switch (event)
	{
		case EVENT_COMMIT:
			
			switch (control) {
					
				case NonResGCal_SaveCalib:
					
					char*	calName = DLGetUINameInput("New Calibration Name", MAX_CAL_NAME_LENGTH, ValidateNewScanAxisCal, activeNRGCal);  
					
					if (!calName) return 0; // do nothing if operation is cancelled or invalid
					
					NonResGalvoCal_type*	nrgCal = init_NonResGalvoCal_type(calName, activeNRGCal->baseClass.lsModule, activeNRGCal->commandVMin, 
													 activeNRGCal->commandVMax, *activeNRGCal->slope, *activeNRGCal->offset, *activeNRGCal->posStdDev,
													 *activeNRGCal->lag, copy_SwitchTimes_type(activeNRGCal->switchTimes), copy_MaxSlopes_type(activeNRGCal->maxSlopes),
													 copy_TriangleCal_type(activeNRGCal->triangleCal), activeNRGCal->resolution, activeNRGCal->minStepSize, activeNRGCal->parked);
					// save calibration
					ListInsertItem(activeNRGCal->baseClass.lsModule->availableCals, &nrgCal, END_OF_LIST);
					// update scan axis calibrations UI list
					UpdateAvailableCalibrations(activeNRGCal->baseClass.lsModule);
					// update scan engines
					unsigned int 		fastAxisType;
					ScanEngine_type**	scanEnginePtr;
					size_t				nScanEngines = ListNumItems(activeNRGCal->baseClass.lsModule->scanEngines);
					for (size_t i = 1; i <= nScanEngines; i++) {
						scanEnginePtr = ListGetPtrToItem(activeNRGCal->baseClass.lsModule->scanEngines, i);
						GetCtrlVal(panel, control, &fastAxisType);   
						UpdateScanEngineCalibrations(*scanEnginePtr); 
					}
					
					
					break;
					
				case NonResGCal_Done:
					
					TaskControl_type*	calTC = activeNRGCal->baseClass.taskController; 
					
					DisassembleTaskTreeBranch(calTC);
					
					// unregister calibration VChans
					DLUnregisterVChan((DAQLabModule_type*)activeNRGCal->baseClass.lsModule, (VChan_type*)activeNRGCal->baseClass.VChanCom);
					DLUnregisterVChan((DAQLabModule_type*)activeNRGCal->baseClass.lsModule, (VChan_type*)activeNRGCal->baseClass.VChanPos);
					
					// unregister calibration Task Controller
					DLRemoveTaskController((DAQLabModule_type*)activeNRGCal->baseClass.lsModule, calTC);
					
					// remove calibration data structure from module list of active calibrations and also discard calibration data
					size_t							nActiveCal = ListNumItems(activeNRGCal->baseClass.lsModule->activeCal);
					ActiveScanAxisCal_type**		activeCalPtr;
					
					for (size_t i = 1; i <= nActiveCal; i++) {
						activeCalPtr = ListGetPtrToItem(activeNRGCal->baseClass.lsModule->activeCal, i);
						if (*activeCalPtr == activeNRGCal) {
							// remove list item
							ListRemoveItem(activeNRGCal->baseClass.lsModule->activeCal, 0, i);
							// discard active calibration data structure
							(*activeNRGCal->baseClass.Discard) ((ActiveScanAxisCal_type**)&activeNRGCal);
							break;
						}
					}
					
					
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
	ActiveNonResGalvoCal_type* nrgCal = callbackData;
	
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
					
				case Cal_ParkedV:
					
					GetCtrlVal(panel, control, &nrgCal->parked);
					break;
					
				case Cal_ScanTime:
					
					GetCtrlVal(panel, control, &nrgCal->scanTime);		// read in [s]
					break;
					
				case Cal_MinStep:
					
					GetCtrlVal(panel, control, &nrgCal->minStepSize); 	// read in [mV]
					nrgCal->minStepSize *= 0.001;						// convert to [V]
					break;
					
				case Cal_Resolution:
					
					GetCtrlVal(panel, control, &nrgCal->resolution);  	// read in [mV]
					nrgCal->resolution *= 0.001;						// convert to [V]
					break;
			}

			break;
	}
	return 0;
}

static int CVICALLBACK NonResGalvoCal_TestPan_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ActiveNonResGalvoCal_type*	nrgCal = callbackData;
	
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

static ActiveScanAxisCal_type* initalloc_ActiveScanAxisCal_type	(ActiveScanAxisCal_type* cal)
{
	// if NULL, allocate memory, otherwise just use the provided address
	if (!cal) {
		cal = malloc (sizeof(ActiveScanAxisCal_type));
		if (!cal) return NULL;
	}
	
	cal->scanAxisType	= NonResonantGalvo; 
	cal->calName 		= NULL;
	cal->taskController	= NULL;
	cal->VChanCom		= NULL;
	cal->VChanPos		= NULL;
	cal->comSampRate	= NULL;
	cal->calPanHndl		= 0;
	cal->lsModule		= NULL;
	cal->Discard		= NULL;
	
	return cal;	
}

static void discard_ActiveScanAxisCal_type (ActiveScanAxisCal_type** cal)
{
	OKfree((*cal)->calName); 
	discard_TaskControl_type(&(*cal)->taskController);
	discard_VChan_type((VChan_type**)&(*cal)->VChanCom);
	discard_VChan_type((VChan_type**)&(*cal)->VChanPos);
	OKfree((*cal)->comSampRate);
	if ((*cal)->calPanHndl) {DiscardPanel((*cal)->calPanHndl); (*cal)->calPanHndl =0;}
	
	OKfree(*cal);
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
	cal->lsModule		= NULL;     
	cal->Discard		= NULL;
	
	return cal;	
}

static void discard_ScanAxisCal_type (ScanAxisCal_type** cal)
{
	OKfree((*cal)->calName); 
	
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

static ActiveNonResGalvoCal_type* init_ActiveNonResGalvoCal_type (LaserScanning_type* lsModule, char calName[], char commandVChanName[], char positionVChanName[])
{
	ActiveNonResGalvoCal_type*	cal = malloc(sizeof(ActiveNonResGalvoCal_type));
	if (!cal) return NULL;
	
	DLDataTypes allowedPacketTypes[] = {DL_Waveform_Double};
	
	// init parent class
	initalloc_ActiveScanAxisCal_type(&cal->baseClass);
	if(!(cal->baseClass.calName		= StrDup(calName))) {free(cal); return NULL;}
	cal->baseClass.VChanCom			= init_SourceVChan_type(commandVChanName, DL_Waveform_Double, cal, NonResGalvoCal_ComVChan_Connected, NonResGalvoCal_ComVChan_Disconnected);   
	cal->baseClass.VChanPos			= init_SinkVChan_type(positionVChanName, allowedPacketTypes, NumElem(allowedPacketTypes), cal, NonResGalvoCal_PosVChan_Connected, NonResGalvoCal_PosVChan_Disconnected);  
	cal->baseClass.scanAxisType  	= NonResonantGalvo;
	cal->baseClass.Discard			= discard_ActiveNonResGalvoCal_type; // override
	cal->baseClass.taskController	= init_TaskControl_type(calName, cal, ConfigureTC_NonResGalvoCal, IterateTC_NonResGalvoCal, AbortIterationTC_NonResGalvoCal, StartTC_NonResGalvoCal, ResetTC_NonResGalvoCal, 
								  DoneTC_NonResGalvoCal, StoppedTC_NonResGalvoCal, DimTC_NonResGalvoCal, NULL, ModuleEventHandler_NonResGalvoCal, ErrorTC_NonResGalvoCal);
	// connect sink VChans (VChanPos) to the Task Controller so that it can process incoming galvo position data
	AddSinkVChan(cal->baseClass.taskController, cal->baseClass.VChanPos, DataReceivedTC_NonResGalvoCal, TASK_VCHAN_FUNC_ITERATE);  
	cal->baseClass.lsModule			= lsModule;
	
								  
	// init ActiveNonResGalvoCal_type
	cal->commandVMin		= 0;
	cal->commandVMax		= 0;
	cal->currentCal			= NonResGalvoCal_Slope_Offset;
	cal->currIterIdx		= 0;
	cal->slope				= NULL;	// i.e. calibration not performed yet
	cal->offset				= NULL;
	cal->posStdDev			= NULL;
	cal->targetSlope		= 0;
	cal->extraRuns			= 0;
	cal->lag				= NULL;
	cal->nRepeat			= 0;
	cal->lastRun			= FALSE;
	cal->nRampSamples		= 0;
	cal->switchTimes		= NULL;
	cal->maxSlopes			= NULL;
	cal->triangleCal		= NULL;
	cal->positionSignal		= NULL;
	cal->commandPacket		= NULL;
	cal->resolution  		= 0;
	cal->minStepSize 		= 0;
	cal->scanTime    		= 2;	// in [s]
	cal->parked				= 0;
	
	return cal;
}

static void	discard_ActiveNonResGalvoCal_type	(ActiveScanAxisCal_type** cal)
{
	if (!*cal) return;
	
	ActiveNonResGalvoCal_type*	NRGCal = (ActiveNonResGalvoCal_type*) *cal;
	
	// discard ActiveNonResGalvoCal_type specific data
	OKfree(NRGCal->slope);
	OKfree(NRGCal->offset);
	OKfree(NRGCal->posStdDev);
	OKfree(NRGCal->lag);
	
	// discard calibration waveforms
	ReleaseDataPacket(&NRGCal->commandPacket);
	discard_Waveform_type(&NRGCal->positionSignal);
	
	discard_SwitchTimes_type(&NRGCal->switchTimes);
	discard_MaxSlopes_type(&NRGCal->maxSlopes);
	discard_TriangleCal_type(&NRGCal->triangleCal);
	
	// discard ActiveScanAxisCal_type base class data
	discard_ActiveScanAxisCal_type(cal);
}

static NonResGalvoCal_type* init_NonResGalvoCal_type (char calName[], LaserScanning_type* lsModule, double commandVMin, 
													  double commandVMax, double slope, double offset, double posStdDev, 
													  double lag, SwitchTimes_type* switchTimes, MaxSlopes_type* maxSlopes,
													  TriangleCal_type* triangleCal, double resolution, double minStepSize, double parked)
{
	NonResGalvoCal_type*	cal = malloc (sizeof(NonResGalvoCal_type));
	if (!cal) return NULL;
	
	// base class
	cal->baseClass.calName 			= StrDup(calName);
	cal->baseClass.lsModule			= lsModule;
	cal->baseClass.scanAxisType		= NonResonantGalvo;
	cal->baseClass.Discard			= discard_NonResGalvoCal_type;
	
	// child class
	cal->commandVMin				= commandVMin;
	cal->commandVMax				= commandVMax;
	cal->slope						= slope;
	cal->offset						= offset;
	cal->posStdDev					= posStdDev;
	cal->lag						= lag;
	cal->switchTimes				= switchTimes;
	cal->maxSlopes					= maxSlopes;
	cal->triangleCal				= triangleCal;
	cal->resolution					= resolution;
	cal->minStepSize				= minStepSize;
	cal->parked						= parked;
	
	return cal;
}

static void discard_NonResGalvoCal_type (NonResGalvoCal_type** cal)
{
	if (!*cal) return;
	
	OKfree((*cal)->baseClass.calName);
	
	discard_SwitchTimes_type(&(*cal)->switchTimes);
	discard_MaxSlopes_type(&(*cal)->maxSlopes);
	discard_TriangleCal_type(&(*cal)->triangleCal);
	
	OKfree(*cal);
}

static BOOL ValidateNewScanAxisCal (char inputStr[], void* dataPtr)
{
	ActiveNonResGalvoCal_type*		activeNRGCal = dataPtr; 
	// check if there is another galvo calibration with the same name
	size_t 					nCalibrations = ListNumItems(activeNRGCal->baseClass.lsModule->availableCals);
	NonResGalvoCal_type**	calPtr;
	for (size_t i = 1; i <= nCalibrations; i++) {
		calPtr = ListGetPtrToItem(activeNRGCal->baseClass.lsModule->availableCals, i);
		if (!strcmp((*calPtr)->baseClass.calName, inputStr))
			return FALSE;
	}
	
	return TRUE; // valid entry 
}   

static SwitchTimes_type* init_SwitchTimes_type(void)
{
	SwitchTimes_type* switchTimes = malloc(sizeof(SwitchTimes_type));
	if (!switchTimes) return NULL;
	
	switchTimes->n          	= 0;
	switchTimes->stepSize   	= NULL;
	switchTimes->halfSwitch 	= NULL;	
	
	return switchTimes;
}

static void discard_SwitchTimes_type (SwitchTimes_type** a)
{
	if (!(*a)) return;
	
	OKfree((*a)->stepSize);
	OKfree((*a)->halfSwitch);
	OKfree(*a);
}

static SwitchTimes_type* copy_SwitchTimes_type (SwitchTimes_type* switchTimes)
{
	SwitchTimes_type*	switchTimesCopy = malloc(sizeof(SwitchTimes_type));
	if (!switchTimesCopy) return NULL;
	
	// init
	switchTimesCopy->n 				= switchTimes->n;
	switchTimesCopy->halfSwitch		= NULL;
	switchTimesCopy->stepSize		= NULL;
	
	
	//----------- 
	// copy
	//----------- 
	if (!switchTimesCopy->n) return switchTimesCopy; 
		// halfSwitch
	switchTimesCopy->halfSwitch 	= malloc(switchTimes->n * sizeof(double));
	if (!switchTimesCopy->halfSwitch) goto Error;
	memcpy(switchTimesCopy->halfSwitch, switchTimes->halfSwitch, switchTimes->n * sizeof(double));
		// stepSize
	switchTimesCopy->stepSize 		= malloc(switchTimes->n * sizeof(double));
	if (!switchTimesCopy->stepSize) goto Error;
	memcpy(switchTimesCopy->stepSize, switchTimes->stepSize, switchTimes->n * sizeof(double));
	
	return switchTimesCopy;
	
Error:
	
	OKfree(switchTimesCopy->halfSwitch);
	OKfree(switchTimesCopy->stepSize);
	OKfree(switchTimesCopy);
	return NULL;
}

static MaxSlopes_type* copy_MaxSlopes_type (MaxSlopes_type* maxSlopes)
{
	MaxSlopes_type*	maxSlopesCopy = malloc(sizeof(MaxSlopes_type));
	if (!maxSlopesCopy) return NULL;
	
	// init
	maxSlopesCopy->n 				= maxSlopes->n;
	maxSlopesCopy->slope			= NULL;
	maxSlopesCopy->amplitude		= NULL;
	
	//----------- 
	// copy
	//----------- 
	if (!maxSlopesCopy->n) return maxSlopesCopy;
		// slope
	maxSlopesCopy->slope 			= malloc(maxSlopes->n * sizeof(double));
	if (!maxSlopesCopy->slope) goto Error;
	memcpy(maxSlopesCopy->slope, maxSlopes->slope, maxSlopes->n * sizeof(double));
		// amplitude
	maxSlopesCopy->amplitude 		= malloc(maxSlopes->n * sizeof(double));
	if (!maxSlopesCopy->amplitude) goto Error;
	memcpy(maxSlopesCopy->amplitude, maxSlopes->amplitude, maxSlopes->n * sizeof(double));
	
	return maxSlopesCopy;
	
Error:
	
	OKfree(maxSlopesCopy->slope);
	OKfree(maxSlopesCopy->amplitude);
	OKfree(maxSlopesCopy);
	return NULL;
}

static TriangleCal_type* copy_TriangleCal_type (TriangleCal_type* triangleCal)
{
	TriangleCal_type* triangleCalCopy = malloc(sizeof(TriangleCal_type));
	if (!triangleCalCopy) return NULL;
	
	// init
	triangleCalCopy->n 			= triangleCal->n;
	triangleCalCopy->deadTime 	= triangleCal->deadTime;
	triangleCalCopy->commandAmp	= NULL;
	triangleCalCopy->actualAmp	= NULL;
	triangleCalCopy->maxFreq	= NULL;
	triangleCalCopy->resLag		= NULL;
	
	//-----------
	// copy
	//-----------
	if (!triangleCalCopy->n) return triangleCalCopy;
		// commandAmp
	triangleCalCopy->commandAmp	= malloc(triangleCal->n * sizeof(double));
	if (!triangleCalCopy->commandAmp) goto Error;
	memcpy(triangleCalCopy->commandAmp, triangleCal->commandAmp, triangleCal->n * sizeof(double)); 
		// actualAmp
	triangleCalCopy->actualAmp	= malloc(triangleCal->n * sizeof(double));
	if (!triangleCalCopy->actualAmp) goto Error;
	memcpy(triangleCalCopy->actualAmp, triangleCal->actualAmp, triangleCal->n * sizeof(double)); 
		// maxFreq
	triangleCalCopy->maxFreq	= malloc(triangleCal->n * sizeof(double));
	if (!triangleCalCopy->maxFreq) goto Error;
	memcpy(triangleCalCopy->maxFreq, triangleCal->maxFreq, triangleCal->n * sizeof(double));
		// resLag
	triangleCalCopy->resLag		= malloc(triangleCal->n * sizeof(double));
	if (!triangleCalCopy->resLag) goto Error;
	memcpy(triangleCalCopy->resLag, triangleCal->resLag, triangleCal->n * sizeof(double));
		
	return triangleCalCopy;
	
Error:
	
	OKfree(triangleCalCopy->commandAmp);
	OKfree(triangleCalCopy->actualAmp);
	OKfree(triangleCalCopy->maxFreq);
	OKfree(triangleCalCopy->resLag);
	OKfree(triangleCalCopy);
	return NULL;
}

static MaxSlopes_type* init_MaxSlopes_type (void)
{
	MaxSlopes_type* maxSlopes = malloc (sizeof(MaxSlopes_type));
	if (!maxSlopes) return NULL;
	
	maxSlopes->n         	= 0;
	maxSlopes->slope     	= NULL;
	maxSlopes->amplitude 	= NULL;
	
	return maxSlopes;
}

static void discard_MaxSlopes_type (MaxSlopes_type** a)
{
	if (!(*a)) return; 
	
	OKfree((*a)->slope);
	OKfree((*a)->amplitude);
	OKfree(*a);
}

static TriangleCal_type* init_TriangleCal_type (void)
{
	TriangleCal_type*  cal = malloc(sizeof(TriangleCal_type));
	if (!cal) return NULL;
	
	cal->n           	= 0;
	cal->deadTime    	= 0;
	cal->commandAmp 	= NULL;
	cal->actualAmp  	= NULL;
    cal->maxFreq     	= NULL;
	cal->resLag      	= NULL;
	
	return cal;	
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
								 char					imageOutVChanName[],
								 char					detectorVChanName[])					
{
	DLDataTypes 	allowedScanAxisPacketTypes[] = {DL_Waveform_Double};
	DLDataTypes		allowedDetectorPacketTypes[] = Allowed_Detector_Data_Types;    
	
	
	// VChans
	engine->engineType				= engineType;
	engine->VChanFastAxisCom		= init_SourceVChan_type(fastAxisComVChanName, DL_Waveform_Double, engine, FastAxisComVChan_Connected, FastAxisComVChan_Disconnected); 
	engine->VChanSlowAxisCom		= init_SourceVChan_type(slowAxisComVChanName, DL_Waveform_Double, engine, SlowAxisComVChan_Connected, SlowAxisComVChan_Disconnected); 
	engine->VChanFastAxisPos		= init_SinkVChan_type(fastAxisPosVChanName, allowedScanAxisPacketTypes, NumElem(allowedScanAxisPacketTypes), engine, FastAxisPosVChan_Connected, FastAxisPosVChan_Disconnected); 
	engine->VChanSlowAxisPos		= init_SinkVChan_type(slowAxisPosVChanName, allowedScanAxisPacketTypes, NumElem(allowedScanAxisPacketTypes), engine, SlowAxisPosVChan_Connected, SlowAxisPosVChan_Disconnected); 
	engine->VChanScanOut			= init_SourceVChan_type(imageOutVChanName, DL_Image_NIVision, engine, ImageOutVChan_Connected, ImageOutVChan_Disconnected); 
	if(!(engine->DetChans			= ListCreate(sizeof(DetChan_type*)))) goto Error;
	// add one detection channel to the scan engine if required
	if (detectorVChanName) {
		DetChan_type* detChan 			= init_DetChan_type(engine, detectorVChanName); 
		ListInsertItem(engine->DetChans, &detChan, END_OF_LIST);
	}
	
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
	if (engine->VChanScanOut) {
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanScanOut);
		discard_VChan_type((VChan_type**)&engine->VChanScanOut);
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
														char					imageOutVChanName[],
														char					detectorVChanName[])
{
	RectangleRaster_type*	engine = malloc (sizeof(RectangleRaster_type));
	if (!engine) return NULL;
	
	//--------------------------------------------------------
	// init base scan engine class
	//--------------------------------------------------------
	init_ScanEngine_type(&engine->baseClass, lsModule, ScanEngine_RectRaster, fastAxisComVChanName, slowAxisComVChanName, fastAxisPosVChanName, slowAxisPosVChanName, imageOutVChanName, detectorVChanName);
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
static void	DetVChan_Connected (VChan_type* self, VChan_type* connectedVChan)
{
	
}

static void	DetVChan_Disconnected (VChan_type* self, VChan_type* disconnectedVChan)
{
	
}

//---------------------------------------------------------------------
// Non Resonant Galvo Calibration and Testing Task Controller Callbacks 
//---------------------------------------------------------------------

static FCallReturn_type* ConfigureTC_NonResGalvoCal	(TaskControl_type* taskControl, BOOL const* abortFlag)
{
	ActiveNonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	// total number of iterations to start with
	SetTaskControlIterations(taskControl, 1);
	// set starting calibration type
	cal->currentCal = NonResGalvoCal_Slope_Offset;
	// reset iteration index
	cal->currIterIdx = 0;
	
	return init_FCallReturn_type(0, "", "");
}

static void IterateTC_NonResGalvoCal (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortIterationFlag)
{
	ActiveNonResGalvoCal_type* 	cal 				= GetTaskControlModuleData(taskControl);
	Waveform_type*			commandWaveform;      
	
	// add empty galvo response waveform 
	discard_Waveform_type(&cal->positionSignal);
	cal->positionSignal = init_Waveform_type(Waveform_Double, *cal->baseClass.comSampRate, 0, NULL);
	
	switch (cal->currentCal) {
			
		case NonResGalvoCal_Slope_Offset:
		{
			
			// discard previous measurements
			OKfree(cal->slope);
			OKfree(cal->offset);
			OKfree(cal->posStdDev);
			
			// create waveform with galvo steps to measure slope and offset between command and position signals
			size_t 		nDelaySamples 			= (size_t) (SLOPE_OFFSET_DELAY * *cal->baseClass.comSampRate);
			size_t		nSamplesPerCalPoint		= nDelaySamples + POSSAMPLES;
			double*		commandSignal 			= malloc (CALPOINTS * nSamplesPerCalPoint * sizeof(double));
			double		VCommand				= cal->commandVMin;
			
			for (size_t i = 0; i < CALPOINTS; i++) {
				for (size_t j = 0; j < nSamplesPerCalPoint; j++)
					commandSignal[i*nSamplesPerCalPoint+j] = VCommand;
				VCommand += (cal->commandVMax - cal->commandVMin) / (CALPOINTS - 1); 
			}
			
			// send waveform
			commandWaveform = init_Waveform_type(Waveform_Double, *cal->baseClass.comSampRate, CALPOINTS * nSamplesPerCalPoint, commandSignal);
			cal->commandPacket = init_DataPacket_type(DL_Waveform_Double, commandSignal, (DiscardPacketDataFptr_type)discard_Waveform_type);
			SendDataPacket(cal->baseClass.VChanCom, cal->commandPacket, 1);
		}
			
			break;
			
		case NonResGalvoCal_Lag:
		{
			
			// discard previous lag measurement if it exists and allocate new memory    
			if (!cal->currIterIdx) {
				OKfree(cal->lag);
				// determine how many times to apply the ramp such that the relative error in the position is less than RELERR
				cal->nRepeat = ceil(pow(*cal->posStdDev * 1.414 /(RELERR * (cal->commandVMax - cal->commandVMin)), 2)); 
				cal->nRampSamples = 2;
				cal->lastRun = FALSE; 
			}
			
			size_t 	postRampSamples = (size_t) floor(POSTRAMPTIME * 0.001 * *cal->baseClass.comSampRate) + 1;
			size_t 	flybackSamples 	= (size_t) floor(MAXFLYBACKTIME * 0.001 * *cal->baseClass.comSampRate) + 1; 
			
			// create ramp
			double*	rampSignal = malloc ((flybackSamples + cal->nRampSamples + postRampSamples) * cal->nRepeat * sizeof(double));
			// pad with flyback samples
			for (size_t i = 0; i < flybackSamples; i++) rampSignal[i] = cal->commandVMin;
			// generate ramp
			Ramp(cal->nRampSamples, cal->commandVMin, cal->commandVMax, rampSignal + flybackSamples);
			// pad with postrampsamples
			for (size_t i = flybackSamples + cal->nRampSamples; i < (flybackSamples + cal->nRampSamples + postRampSamples); i++) 
				rampSignal[i] = cal->commandVMax;
			// extend signal to nRepeat times
			for (size_t i = 1; i < cal->nRepeat; i++)
				memcpy(rampSignal + i * (flybackSamples + cal->nRampSamples + postRampSamples), rampSignal, (flybackSamples + cal->nRampSamples + postRampSamples) * sizeof(double));
			
			// send waveform
			commandWaveform = init_Waveform_type(Waveform_Double, *cal->baseClass.comSampRate, (flybackSamples + cal->nRampSamples + postRampSamples) * cal->nRepeat, rampSignal);
			cal->commandPacket = init_DataPacket_type(DL_Waveform_Double, commandWaveform, (DiscardPacketDataFptr_type)discard_Waveform_type);
			SendDataPacket(cal->baseClass.VChanCom, cal->commandPacket, 1);
			
		}
			
			break;
			
		case NonResGalvoCal_SwitchTimes:
		{
			
			// delete previous measurements and adjust plotting area
			if (!cal->currIterIdx) {
				discard_SwitchTimes_type(&cal->switchTimes);
				cal->switchTimes = init_SwitchTimes_type();
				
				DeleteGraphPlot(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, -1 , VAL_IMMEDIATE_DRAW);
				SetAxisScalingMode(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, VAL_BOTTOM_XAXIS, VAL_MANUAL, 0, cal->commandVMax - cal->commandVMin);
				SetAxisScalingMode(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, VAL_LEFT_YAXIS, VAL_AUTOSCALE, 0, 0);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_XLABEL_VISIBLE, 1);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_YLABEL_VISIBLE, 1);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_XGRID_VISIBLE, 1);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_YGRID_VISIBLE, 1);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_LABEL_TEXT, "Halfswitch time vs. ramp amplitude");   
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_XNAME, "Ramp amplitude (V)");
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_YNAME, "Time (µs)");
			}
			
			size_t 	postStepSamples = (size_t) floor(POSTRAMPTIME * 0.001 * *cal->baseClass.comSampRate) + 1;
			size_t 	flybackSamples 	= (size_t) floor(MAXFLYBACKTIME * 0.001 * *cal->baseClass.comSampRate) + 1; 
			double	amplitudeFactor = pow(SWITCH_TIMES_AMP_ITER_FACTOR, (double)cal->currIterIdx);
			
			// determine how many times to apply the step such that the relative error in the position is less than RELERR   
			cal->nRepeat = ceil(pow(*cal->posStdDev * 1.414 /(RELERR * (cal->commandVMax - cal->commandVMin) * amplitudeFactor), 2));
			
			// create step signal
			double*	stepSignal = malloc ((flybackSamples + postStepSamples) * cal->nRepeat * sizeof(double));
			
			// pad flyback samples
			for (size_t i = 0; i < flybackSamples; i++) stepSignal[i] = cal->commandVMin * amplitudeFactor;
			// pad post step samples
			for (size_t i = flybackSamples; i < (flybackSamples + postStepSamples); i++) stepSignal[i] = cal->commandVMax * amplitudeFactor;
			// apply signal nRepeat times
			for (size_t i = 1; i < cal->nRepeat; i++)
				memcpy(stepSignal + i * (flybackSamples + postStepSamples), stepSignal, (flybackSamples + postStepSamples) * sizeof(double));
			
			// send waveform
			commandWaveform = init_Waveform_type(Waveform_Double, *cal->baseClass.comSampRate, (flybackSamples + postStepSamples) * cal->nRepeat, stepSignal);
			cal->commandPacket = init_DataPacket_type(DL_Waveform_Double, commandWaveform, (DiscardPacketDataFptr_type)discard_Waveform_type);
			SendDataPacket(cal->baseClass.VChanCom, cal->commandPacket, 1);
			
		}
			break;
			
		case NonResGalvoCal_MaxSlopes:
		{	
			// delete previous measurements
			if (!cal->currIterIdx) {
				discard_MaxSlopes_type(&cal->maxSlopes);
				cal->maxSlopes = init_MaxSlopes_type();
				// determine how many times to apply the ramp such that the relative error in the position is less than RELERR
				cal->nRampSamples = 2;
				
				// adjust plotting
				DeleteGraphPlot(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, -1 , VAL_IMMEDIATE_DRAW);
				SetAxisScalingMode(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, VAL_BOTTOM_XAXIS, VAL_MANUAL, 0, cal->commandVMax - cal->commandVMin);
				SetAxisScalingMode(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, VAL_LEFT_YAXIS, VAL_AUTOSCALE, 0, 0);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_XLABEL_VISIBLE, 1);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_YLABEL_VISIBLE, 1);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_XGRID_VISIBLE, 1);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_YGRID_VISIBLE, 1);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_LABEL_TEXT, "Maximum slope vs. ramp amplitude");   
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_XNAME, "Ramp amplitude (V)");
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_YNAME, "Max slope (V/ms)");
			}
			
			size_t 	postRampSamples = (size_t) floor(POSTRAMPTIME * 0.001 * *cal->baseClass.comSampRate) + 1;
			size_t 	flybackSamples 	= (size_t) floor(MAXFLYBACKTIME * 0.001 * *cal->baseClass.comSampRate) + 1; 
			double	amplitudeFactor = pow(FIND_MAXSLOPES_AMP_ITER_FACTOR, (double)cal->currIterIdx);
			
			// determine how many times to apply the step such that the relative error in the position is less than RELERR   
			cal->nRepeat = ceil(pow(*cal->posStdDev * 1.414 /(RELERR * (cal->commandVMax - cal->commandVMin) * amplitudeFactor), 2));
			
			// create ramp
			double*	rampSignal = malloc ((flybackSamples + cal->nRampSamples + postRampSamples) * cal->nRepeat * sizeof(double));
			// pad with flyback samples
			for (size_t i = 0; i < flybackSamples; i++) rampSignal[i] = cal->commandVMin * amplitudeFactor;
			// generate ramp
			Ramp(cal->nRampSamples, cal->commandVMin * amplitudeFactor, cal->commandVMax * amplitudeFactor, rampSignal + flybackSamples);
			// pad with postrampsamples
			for (size_t i = flybackSamples + cal->nRampSamples; i < (flybackSamples + cal->nRampSamples + postRampSamples); i++) rampSignal[i] = cal->commandVMax * amplitudeFactor;
			// apply signal nRepeat times
			for (size_t i = 1; i < cal->nRepeat; i++)
				memcpy(rampSignal + i * (flybackSamples + cal->nRampSamples + postRampSamples), rampSignal, (flybackSamples + cal->nRampSamples + postRampSamples) * sizeof(double));
			
			// send waveform
			commandWaveform = init_Waveform_type(Waveform_Double, *cal->baseClass.comSampRate, (flybackSamples + cal->nRampSamples + postRampSamples) * cal->nRepeat, rampSignal);
			cal->commandPacket = init_DataPacket_type(DL_Waveform_Double, commandWaveform, (DiscardPacketDataFptr_type)discard_Waveform_type);
			SendDataPacket(cal->baseClass.VChanCom, cal->commandPacket, 1);
			
		}
			break;
			
		case NonResGalvoCal_TriangleWave:
		{
			if (!cal->currIterIdx) {
				discard_TriangleCal_type(&cal->triangleCal);
				cal->triangleCal 		= init_TriangleCal_type();
				// initialize target slope
				cal->targetSlope		= cal->maxSlopes->slope[0] * DYNAMICCAL_INITIAL_SLOPE_REDUCTION_FACTOR;
				cal->extraRuns 			= 0;
				// delete previous plot
				DeleteGraphPlot(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, -1, VAL_IMMEDIATE_DRAW); 
				SetAxisScalingMode(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, VAL_BOTTOM_XAXIS, VAL_MANUAL, 0, cal->commandVMax - cal->commandVMin);
				SetAxisScalingMode(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, VAL_LEFT_YAXIS, VAL_AUTOSCALE, 0, 0);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_XLABEL_VISIBLE, 1);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_YLABEL_VISIBLE, 1);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_XGRID_VISIBLE, 1);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_YGRID_VISIBLE, 1);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_LABEL_TEXT, "Maximum scan frequency vs. amplitude");   
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_XNAME, "Amplitude (V)");
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_YNAME, "Max triangle frequency (Hz)");
			}
			
			double 		funcAmp 		= cal->maxSlopes->amplitude[cal->currIterIdx]; 											// in [V] Pk-Pk
			// apply scan function with slope equal to max slope measured previously at a certain amplitude
			double		funcFreq 		= cal->targetSlope * 1000/(2 * funcAmp);			   											// in [Hz]
			size_t 		nCycles 		= ceil(cal->scanTime * funcFreq); 
			size_t 		cycleSamples 	= (size_t) floor (1/funcFreq * *cal->baseClass.comSampRate);
			// calculate number of cycles, precycles and samples
			size_t		preCycles 		= (size_t) ceil (*cal->lag /1000 * funcFreq);
			size_t 		nSamples 		= (nCycles + preCycles) * cycleSamples;
			
		  
			// create ramp
			double*		commandSignal 	= malloc (nSamples * sizeof(double));
			
			// generate command signal
			double		phase	 		= -90;
			TriangleWave(nSamples, funcAmp/2, 1.0/cycleSamples, &phase, commandSignal);
						
			// send waveform
			commandWaveform = init_Waveform_type(Waveform_Double, *cal->baseClass.comSampRate, nSamples, commandSignal);
			cal->commandPacket = init_DataPacket_type(DL_Waveform_Double, commandWaveform, (DiscardPacketDataFptr_type)discard_Waveform_type);
			SendDataPacket(cal->baseClass.VChanCom, cal->commandPacket, 1);
		}
			break;
	}
	
}

static void AbortIterationTC_NonResGalvoCal (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	ActiveNonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);

}

static FCallReturn_type* StartTC_NonResGalvoCal (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	ActiveNonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	// dim Save calibration
	SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_SaveCalib, ATTR_DIMMED, 1);    
	
	return init_FCallReturn_type(0, "", ""); 
}

static FCallReturn_type* ResetTC_NonResGalvoCal (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	ActiveNonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", ""); 
}

static FCallReturn_type* DoneTC_NonResGalvoCal (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	ActiveNonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	// calibration complete, undim Save calibration
	SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_SaveCalib, ATTR_DIMMED, 0);
	
	return init_FCallReturn_type(0, "", ""); 
}

static FCallReturn_type* StoppedTC_NonResGalvoCal (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	ActiveNonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	// dim Save calibration since calibration is incomplete
	SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_SaveCalib, ATTR_DIMMED, 1);  
	
	return init_FCallReturn_type(0, "", ""); 
}

static void	DimTC_NonResGalvoCal (TaskControl_type* taskControl, BOOL dimmed)
{
	ActiveNonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	// dim/undim controls
	SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_Done, ATTR_DIMMED, dimmed);
    int calSetPanHndl;
	GetPanelHandleFromTabPage(cal->baseClass.calPanHndl, NonResGCal_Tab, 0, &calSetPanHndl);
	SetCtrlAttribute(calSetPanHndl, Cal_CommMinV, ATTR_DIMMED, dimmed);  
	SetCtrlAttribute(calSetPanHndl, Cal_CommMaxV, ATTR_DIMMED, dimmed); 
	SetCtrlAttribute(calSetPanHndl, Cal_ParkedV, ATTR_DIMMED, dimmed); 
	SetCtrlAttribute(calSetPanHndl, Cal_ScanTime, ATTR_DIMMED, dimmed);
	SetCtrlAttribute(calSetPanHndl, Cal_MinStep, ATTR_DIMMED, dimmed); 
	SetCtrlAttribute(calSetPanHndl, Cal_Resolution, ATTR_DIMMED, dimmed); 
}

static FCallReturn_type* DataReceivedTC_NonResGalvoCal (TaskControl_type* taskControl, TaskStates_type taskState, SinkVChan_type* sinkVChan, BOOL const* abortFlag)
{
	ActiveNonResGalvoCal_type* 	cal 					= GetTaskControlModuleData(taskControl);
	DataPacket_type**		dataPackets				= NULL;
	Waveform_type*			commandWaveform			= NULL;
	Waveform_type*			positionWaveform		= NULL;
	double*					positionSignal;
	size_t					nPositionSignalSamples; 
	double*					commandSignal;
	size_t					nCommandSignalSamples;
	size_t					nPackets;
	DLDataTypes 			dataPacketType;
	BOOL					waveformCompleteFlag	= FALSE;
	
	// collect data packets and assemble galvo response waveform
	GetAllDataPackets (sinkVChan, &dataPackets, &nPackets);
	for (size_t i = 0; i < nPackets; i++)
		if (dataPackets[i]) {
			// get waveform out of data packet
			positionWaveform = GetDataPacketDataPtr(dataPackets[i], &dataPacketType);
			// collect waveforms that belong to a single galvo response
			AppendWaveformData(cal->positionSignal, &positionWaveform);
			ReleaseDataPacket(&dataPackets[i]);
		} else {
			waveformCompleteFlag = TRUE;
			ReleaseDataPacket(&dataPackets[i]);
			break;
		}
			
	if (waveformCompleteFlag){
		// get pointer to galvo command signal
		commandWaveform	= GetDataPacketDataPtr (cal->commandPacket, &dataPacketType);
		commandSignal	= GetWaveformDataPtr(commandWaveform, &nCommandSignalSamples);
		
		//get pointer to galvo position signal
		positionSignal = GetWaveformDataPtr (cal->positionSignal, &nPositionSignalSamples);  
		
		// process galvo position signal
		switch (cal->currentCal) {
			
			case NonResGalvoCal_Slope_Offset:
			{
				
				size_t 		nDelaySamples 			= (size_t) (SLOPE_OFFSET_DELAY * *cal->baseClass.comSampRate);
				size_t		nSamplesPerCalPoint		= nDelaySamples + POSSAMPLES;
				double  	Pos[CALPOINTS];
				double  	Comm[CALPOINTS];
				double		PosStdDev[CALPOINTS];
				double  	LinFitResult[CALPOINTS];
				double  	meanSquaredError;
			
				// analyze galvo response  
				for (size_t i = 0; i < CALPOINTS; i++) {
					// get average position and signal StdDev
					StdDev(positionSignal + i*nSamplesPerCalPoint + nDelaySamples, POSSAMPLES, &Pos[i], &PosStdDev[i]);
					Comm[i] = (cal->commandVMax - cal->commandVMin) * i /(CALPOINTS - 1) + cal->commandVMin; 
				}
				
				// calculate average of standard deviations to better estimate noise on position signal
				if (!cal->posStdDev) cal->posStdDev = malloc (sizeof(double));
				Mean(PosStdDev, CALPOINTS, cal->posStdDev);
			
				// determine slope and offset for linear cal curves for X and Y
				if (!cal->slope) cal->slope = malloc (sizeof(double));
				if (!cal->offset) cal->offset = malloc (sizeof(double));
				LinFit (Pos, Comm, CALPOINTS, LinFitResult, cal->slope , cal->offset, &meanSquaredError);	
			
				// update results
				int calibPanHndl;
				GetPanelHandleFromTabPage(cal->baseClass.calPanHndl, NonResGCal_Tab, 0, &calibPanHndl);
				
				// slope
				SetCtrlVal(calibPanHndl, Cal_Slope_a, *cal->slope);
				SetCtrlAttribute(calibPanHndl, Cal_Slope_a, ATTR_DIMMED, 0);
				// offset
				SetCtrlVal(calibPanHndl, Cal_Offset_b, *cal->offset * 1000); 		// display in [mV]
				SetCtrlAttribute(calibPanHndl, Cal_Offset_b, ATTR_DIMMED, 0);
				// StdDev
				SetCtrlVal(calibPanHndl, Cal_PosStdDev, *cal->posStdDev * 1000); 	// display in [mV]
				SetCtrlAttribute(calibPanHndl, Cal_Offset_b, ATTR_DIMMED, 0);
				
				// proceed to next calibration method
				cal->currentCal++;
				cal->currIterIdx = 0;
				ReleaseDataPacket(&cal->commandPacket);
				TaskControlIterationDone(taskControl, 0, "", TRUE);
			}
				break;
		
			case NonResGalvoCal_Lag:
			{
				
				// average galvo ramp responses, use only ramp and post ramp time to analyze the data
				
				size_t 		postRampSamples = (size_t) floor(POSTRAMPTIME * 0.001 * *cal->baseClass.comSampRate) + 1;
				size_t 		flybackSamples 	= (size_t) floor(MAXFLYBACKTIME * 0.001 * *cal->baseClass.comSampRate) + 1;
				
				double*		averageResponse = calloc (cal->nRampSamples + postRampSamples, sizeof(double));
				
				// sum up ramp responses
				for (size_t i = 0; i < cal->nRepeat; i++)
					for (size_t j = 0; j < cal->nRampSamples + postRampSamples; j++)
						averageResponse[j] += positionSignal[i*(flybackSamples + cal->nRampSamples + postRampSamples)+flybackSamples+j];
				
				// average ramp responses
				for (size_t i = 0; i < cal->nRampSamples + postRampSamples; i++) averageResponse[i] /= cal->nRepeat;
				
				// calculate corrected position signal based on scaling and offset
				for (size_t i = 0; i < cal->nRampSamples + postRampSamples; i++) averageResponse[i] = *cal->slope * averageResponse[i] + *cal->offset;
				
				// calculate response slope
				double rampSlope;
				FindSlope(averageResponse, cal->nRampSamples + postRampSamples, *cal->baseClass.comSampRate, *cal->posStdDev, cal->nRepeat, RELERR, &rampSlope);   
				// calculate command ramp slope
				double targetSlope = fabs(cal->commandVMax - cal->commandVMin) / ((cal->nRampSamples - 1) * 1000/ *cal->baseClass.comSampRate);  // ramp slope in [V/ms]
				
				// if this was the last run, calculate lag
				if (cal->lastRun) {
					
					cal->lag = malloc(sizeof(double));
					cal->currIterIdx = 0;
					cal->currentCal++; 
					cal->lastRun = FALSE;
					*cal->lag = MeasureLag(commandSignal+flybackSamples, averageResponse, cal->nRampSamples + postRampSamples) * 1000/ *cal->baseClass.comSampRate; // response lag in [ms]
				
				} else {
					if (rampSlope < targetSlope * 0.98) {
						// calculate ramp that has maxslope
						cal->nRampSamples = (size_t) floor(fabs(cal->commandVMax - cal->commandVMin) / rampSlope * *cal->baseClass.comSampRate * 0.001);
						cal->lastRun = FALSE;
					}
					else { 
						cal->nRampSamples = (size_t) floor(fabs(cal->commandVMax - cal->commandVMin) / (rampSlope * MAX_SLOPE_REDUCTION_FACTOR) * *cal->baseClass.comSampRate * 0.001); 
						cal->lastRun = TRUE;
					}
					cal->currIterIdx++;
				}
				
				// plot the command and response signals
				DeleteGraphPlot(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, -1, VAL_IMMEDIATE_DRAW); 
				// adjust plot time axis ranges
				SetAxisScalingMode(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, VAL_BOTTOM_XAXIS, VAL_AUTOSCALE, 0, 0);
				SetAxisScalingMode(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, VAL_LEFT_YAXIS, VAL_AUTOSCALE, 0, 0);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_XLABEL_VISIBLE, 1);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_YLABEL_VISIBLE, 1);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_XGRID_VISIBLE, 1);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_YGRID_VISIBLE, 1);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_LABEL_TEXT, "Galvo command and response");   
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_XNAME, "Time (ms)");
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_YNAME, "Galvo signals (V)");
				// plot waveforms
				PlotWaveform(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, commandSignal, cal->nRampSamples + postRampSamples, VAL_DOUBLE, 1.0, 0, 0, 1000/ *cal->baseClass.comSampRate, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, VAL_BLUE);
				PlotWaveform(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, averageResponse, cal->nRampSamples + postRampSamples, VAL_DOUBLE, 1.0, 0, 0, 1000/ *cal->baseClass.comSampRate, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, VAL_RED);
			
				double x, y;
			
				GetGraphCursor(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, 1, &x, &y);
				SetCtrlVal(cal->baseClass.calPanHndl, NonResGCal_CursorX, x);
				SetCtrlVal(cal->baseClass.calPanHndl, NonResGCal_CursorY, y);
				
				OKfree(averageResponse);
				ReleaseDataPacket(&cal->commandPacket);
				TaskControlIterationDone(taskControl, 0, "", TRUE);
			
			}
				break;
			
			case NonResGalvoCal_SwitchTimes:
			{
				// average galvo ramp responses, use only ramp and post ramp time to analyze the data
				
				size_t 		postStepSamples = (size_t) floor(POSTRAMPTIME * 0.001 * *cal->baseClass.comSampRate) + 1;
				size_t 		flybackSamples 	= (size_t) floor(MAXFLYBACKTIME * 0.001 * *cal->baseClass.comSampRate) + 1; 
				double		amplitudeFactor = pow(SWITCH_TIMES_AMP_ITER_FACTOR, (double)cal->currIterIdx);
				
				double*		averageResponse = calloc (postStepSamples, sizeof(double));
				// sum up step responses
				for (size_t i = 0; i < cal->nRepeat; i++)
					for (size_t j = 0; j < postStepSamples; j++)
						averageResponse[j] += positionSignal[i * (flybackSamples + postStepSamples) + flybackSamples + j];
				// average ramp responses
				for (size_t i = 0; i < postStepSamples; i++) averageResponse[i] /= cal->nRepeat;
				
				// calculate corrected position signal based on scaling and offset
				for (size_t i = 0; i < postStepSamples; i++) averageResponse[i] = *cal->slope * averageResponse[i] + *cal->offset;
				
				// find 50% crossing point where response is halfway between the applied step
				for (int i = 0; i < postStepSamples; i++) 
					if (averageResponse[i] > (cal->commandVMax + cal->commandVMin) * amplitudeFactor/2) { 
						cal->switchTimes->n++;
						cal->switchTimes->stepSize = realloc (cal->switchTimes->stepSize, cal->switchTimes->n * sizeof(double));
						cal->switchTimes->stepSize[cal->switchTimes->n - 1] = (cal->commandVMax - cal->commandVMin) * amplitudeFactor;
						cal->switchTimes->halfSwitch = realloc (cal->switchTimes->halfSwitch, cal->switchTimes->n * sizeof(double));
						cal->switchTimes->halfSwitch[cal->switchTimes->n - 1] = i / *cal->baseClass.comSampRate * 1000;
						
						// plot switching time
						double x, y;
						PlotPoint(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, (cal->commandVMax - cal->commandVMin)*amplitudeFactor, cal->switchTimes->halfSwitch[cal->switchTimes->n - 1]  * 1000 , VAL_ASTERISK, VAL_BLUE);
						GetGraphCursor(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, 1, &x, &y);
						SetCtrlVal(cal->baseClass.calPanHndl, NonResGCal_CursorX, x);
						SetCtrlVal(cal->baseClass.calPanHndl, NonResGCal_CursorY, y);
						
						break;
					}
				
				if ((cal->commandVMax - cal->commandVMin) * amplitudeFactor >= cal->resolution * SWITCH_TIMES_AMP_ITER_FACTOR)
				  cal->currIterIdx++;
				else {
					
					cal->currIterIdx = 0;
					cal->currentCal++;
				}
				
				OKfree(averageResponse);
				ReleaseDataPacket(&cal->commandPacket); 
				TaskControlIterationDone(taskControl, 0, "", TRUE);   
			}
				break;
			
			case NonResGalvoCal_MaxSlopes:
			{
				// average galvo ramp responses, use only ramp and post ramp time to analyze the data
				
				size_t 		postRampSamples = (size_t) floor(POSTRAMPTIME * 0.001 * *cal->baseClass.comSampRate) + 1;
				size_t 		flybackSamples 	= (size_t) floor(MAXFLYBACKTIME * 0.001 * *cal->baseClass.comSampRate) + 1; 
				double		amplitudeFactor = pow(FIND_MAXSLOPES_AMP_ITER_FACTOR, (double)cal->currIterIdx);
				
				double*		averageResponse = calloc (cal->nRampSamples + postRampSamples, sizeof(double));
				// sum up ramp responses
				for (size_t i = 0; i < cal->nRepeat; i++)
					for (size_t j = 0; j < cal->nRampSamples + postRampSamples; j++)
						averageResponse[j] += positionSignal[i * (flybackSamples + cal->nRampSamples + postRampSamples) + flybackSamples + j];
				// average ramp responses
				for (size_t i = 0; i <  cal->nRampSamples + postRampSamples; i++) averageResponse[i] /= cal->nRepeat;
				
				// calculate corrected position signal based on scaling and offset
				for (size_t i = 0; i < cal->nRampSamples + postRampSamples; i++) averageResponse[i] = *cal->slope * averageResponse[i] + *cal->offset;
				
				// calculate ramp slope
				double responseSlope;
				double commandSlope = fabs(cal->commandVMax - cal->commandVMin)*amplitudeFactor / ((cal->nRampSamples - 1) * 1000/ *cal->baseClass.comSampRate); 
				
				FindSlope(averageResponse, cal->nRampSamples + postRampSamples, *cal->baseClass.comSampRate, *cal->posStdDev, cal->nRepeat, RELERR, &responseSlope);  
				
				if ((cal->commandVMax - cal->commandVMin) * amplitudeFactor >= cal->minStepSize * FIND_MAXSLOPES_AMP_ITER_FACTOR) 
					if (responseSlope < commandSlope * 0.98)
						// calculate ramp that has maxslope
						cal->nRampSamples = (size_t) floor(fabs(cal->commandVMax - cal->commandVMin) * amplitudeFactor / responseSlope * *cal->baseClass.comSampRate * 0.001);
					else {
						// store slope value
						cal->currIterIdx++;  
						cal->maxSlopes->n++;
						cal->maxSlopes->amplitude = realloc (cal->maxSlopes->amplitude, cal->maxSlopes->n * sizeof(double));
						cal->maxSlopes->amplitude[cal->maxSlopes->n - 1] = (cal->commandVMax - cal->commandVMin) * amplitudeFactor;
						cal->maxSlopes->slope = realloc (cal->maxSlopes->slope, cal->maxSlopes->n * sizeof(double)); 
						cal->maxSlopes->slope[cal->maxSlopes->n] = responseSlope;
						// plot slope
						double x, y;
						PlotPoint(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, (cal->commandVMax - cal->commandVMin) * amplitudeFactor, responseSlope, VAL_ASTERISK, VAL_BLUE);
						GetGraphCursor(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, 1, &x, &y);
						SetCtrlVal(cal->baseClass.calPanHndl, NonResGCal_CursorX, x);
						SetCtrlVal(cal->baseClass.calPanHndl, NonResGCal_CursorY, y);
					}
				else {
					cal->currIterIdx = 0;
					cal->currentCal++; 
				}
				
				OKfree(averageResponse);
				ReleaseDataPacket(&cal->commandPacket); 
				TaskControlIterationDone(taskControl, 0, "", TRUE); 	
			}	
				
				break;
			
			case NonResGalvoCal_TriangleWave:
			{	
				double 		funcAmp 		= cal->maxSlopes->amplitude[cal->currIterIdx];												// in [V] Pk-Pk
				// apply scan function with slope equal to max slope measured previously at a certain amplitude
				double		funcFreq 		= cal->targetSlope * 1000/(2 * funcAmp);													// in [Hz]	
				size_t 		nCycles 		= ceil(cal->scanTime * funcFreq); 
				size_t 		cycleSamples 	= (size_t) floor (1/funcFreq * *cal->baseClass.comSampRate);
				// calculate number of cycles, precycles and samples
				size_t		preCycles 		= (size_t) ceil (*cal->lag /1000 * funcFreq);
				size_t 		nSamples 		= (nCycles + preCycles) * cycleSamples;
				BOOL		overloadFlag	= FALSE;
				double* 	averageResponse = NULL;
				
				// check if there is overload
				for (int i = 0; i < nSamples; i++) 
					if ((positionSignal[i] < - funcAmp/2 * 1.1 - 5 * *cal->posStdDev) || (positionSignal[i] > funcAmp/2 * 1.1 + 5 * *cal->posStdDev)) {
						overloadFlag = TRUE;
						break;
					}
				
				cal->targetSlope 	*= DYNAMICCAL_SLOPE_ITER_FACTOR;
				// in case of overload, reduce slope and repeat until there is no overload
				if (overloadFlag)
					cal->extraRuns	= 0;
				else {
					
					// calculate lag between position signal and command signal in number of samples
					size_t delta = (size_t) (*cal->lag * *cal->baseClass.comSampRate/1000);
					// average position signal triangle waveforms
					averageResponse = calloc (cycleSamples, sizeof(double));   
					// sum up triangle wave response taking into account the lag between command and response
					for (size_t i = 0; i < nCycles; i++)
						for (size_t j = 0; j < cycleSamples; j++)
							averageResponse[j] += positionSignal[i * cycleSamples + delta + j];
					// average ramp responses
					for (size_t i = 0; i < cycleSamples; i++) averageResponse[i] /= nCycles;
				
					// calculate corrected position signal based on scaling and offset
					for (size_t i = 0; i < cycleSamples; i++) averageResponse[i] = *cal->slope * averageResponse[i] + *cal->offset;
					
					double 	maxSlope, maxVal, minVal;
					ssize_t maxIdx, minIdx;
					FindSlope(averageResponse, cycleSamples, *cal->baseClass.comSampRate, *cal->posStdDev, nCycles, 0.05, &maxSlope);
					MaxMin1D(averageResponse, cycleSamples, &maxVal, &maxIdx, &minVal, &minIdx);  
					
					if (maxSlope > cal->targetSlope * 0.95) cal->extraRuns++;
					
					if (cal->extraRuns == 2) {
						// measure deadtime for galvo turn-around before and after a line scan, i.e. the duration during which the galvo response is not linear with the galvo command
						// iterate a few times to converge
						size_t 	nTotalSamplesDelay = 0;
						double  fVal = funcAmp/2;
						for (int i = 0; i < 3; i++){
							// check if there is sufficient SNR to continue the estimation
							if (1.141 * *cal->posStdDev / sqrt(nCycles) / fabs(fVal - maxVal) > 0.05) break;
							if (fVal - maxVal < 0) break; 	// assumes the response is smaller in amplitude than the command
							nTotalSamplesDelay += (size_t) floor( (fVal - maxVal) / (funcAmp * 2 * funcFreq) * *cal->baseClass.comSampRate ) ;
							fVal = maxVal;
							maxVal = averageResponse[maxIdx - nTotalSamplesDelay];
						}
						
						// measure residual lag that may depend somewhat on the scan frequency and amplitude
						cal->triangleCal->n++;
						cal->triangleCal->commandAmp = realloc(cal->triangleCal->commandAmp, cal->triangleCal->n * sizeof(double)); 
						cal->triangleCal->commandAmp[cal->currIterIdx] = cal->maxSlopes->amplitude[cal->currIterIdx];
						cal->triangleCal->maxFreq[cal->currIterIdx] = maxSlope * 1000/(2 * funcAmp);  // maximum triangle function frequency! 
						cal->triangleCal->resLag[cal->currIterIdx] = MeasureLag(commandSignal, averageResponse, cycleSamples) * 1000/ *cal->baseClass.comSampRate;
						cal->triangleCal->deadTime += nTotalSamplesDelay / *cal->baseClass.comSampRate * 1000 ; // delay in ms 
						
						// plot
						PlotPoint(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, funcAmp, cal->triangleCal->maxFreq[cal->triangleCal->n-1], VAL_ASTERISK, VAL_BLUE);
						SetGraphCursor(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, 1, funcAmp, cal->triangleCal->maxFreq[cal->triangleCal->n-1]);
						SetCtrlVal(cal->baseClass.calPanHndl, NonResGCal_CursorX, funcAmp);
						SetCtrlVal(cal->baseClass.calPanHndl, NonResGCal_CursorY, cal->triangleCal->maxFreq[cal->triangleCal->n-1]);
						
						// continue with next maximum slope
						cal->extraRuns = 0;
						cal->currIterIdx++;
						
						// init next target slope
						if (cal->currIterIdx < cal->maxSlopes->n) 
							cal->targetSlope = cal->maxSlopes->slope[cal->currIterIdx] * DYNAMICCAL_INITIAL_SLOPE_REDUCTION_FACTOR;
					}
				}
				
				OKfree(averageResponse);
				ReleaseDataPacket(&cal->commandPacket); 
				
				if (cal->currIterIdx < cal->maxSlopes->n) {
					TaskControlIterationDone(taskControl, 0, "", TRUE); 
				}
				else {
					// calculate average dead time
					cal->triangleCal->deadTime /= cal->maxSlopes->n;
					// calculate actual triangle waveform amplitude
					for (size_t i = 0; i < cal->maxSlopes->n; i++)
						cal->triangleCal->actualAmp[i] = cal->triangleCal->commandAmp[i] * (1 - 4 * cal->triangleCal->maxFreq[i] * cal->triangleCal->deadTime * 0.001);  
					
					// end task controller iterations
					TaskControlIterationDone(taskControl, 0, "", FALSE);
				}
					
				
			}
		
		
		}
	}
		
	discard_Waveform_type(&cal->positionSignal); 
	
	OKfree(dataPackets);
	
	return init_FCallReturn_type(0, "", "");
}

static FCallReturn_type* ModuleEventHandler_NonResGalvoCal (TaskControl_type* taskControl, TaskStates_type taskState, size_t currentIteration, void* eventData, BOOL const* abortFlag)
{
	ActiveNonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "");
}

static void ErrorTC_NonResGalvoCal (TaskControl_type* taskControl, char* errorMsg)
{
	ActiveNonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
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
