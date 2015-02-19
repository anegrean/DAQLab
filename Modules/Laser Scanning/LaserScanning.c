
//==============================================================================
//
// Title:		GalvoScanEngine.c
// Purpose:		A short description of the implementation.
//
// Created on:	25-8-2014 at 14:17:14 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "DAQLab.h" 		// include this first   
#include <formatio.h>  
#include "LaserScanning.h"
#include <userint.h>
#include "combobox.h" 
#include <analysis.h>
#include <NIVisionDisplay.h>
#include "UI_LaserScanning.h"

									 

//==============================================================================
// Constants
#define DAQmxErrChk(functionCall) if( DAQmxFailed(error=(functionCall)) ) goto Error; else 
	// maximum number of characters to represent a double precision number
#define MAX_DOUBLE_NCHARS									30

#define MOD_LaserScanning_UI 								"./Modules/Laser Scanning/UI_LaserScanning.uir"
#define VChanDataTimeout									2e4					// Timeout in [ms] for Sink VChans to receive data
// Default Scan Engine VChan base names. Default base names must be unique among each other!
#define VChan_ScanEngine_FastAxis_Command					"fast axis command"
#define VChan_ScanEngine_FastAxis_Command_NSamples			"fast axis command n samples"
#define VChan_ScanEngine_FastAxis_Position					"fast axis position" 
#define VChan_ScanEngine_SlowAxis_Command					"slow axis command"
#define VChan_ScanEngine_SlowAxis_Command_NSamples			"slow axis command n samples"
#define VChan_ScanEngine_SlowAxis_Position					"slow axis position"
#define VChan_ScanEngine_ImageOut							"image"
#define VChan_ScanEngine_DetectionChan						"detector"
#define VChan_ScanEngine_Shutter_Command					"shutter"
#define VChan_ScanEngine_Pixel_PulseTrain					"pixel pulse train"
#define VChan_ScanEngine_NPixels							"n pixels"
// Default Scan Axis Calibration VChan base names.
#define VChan_AxisCal_Command								"command"
#define VChan_AxisCal_Position								"position"
#define VChan_AxisCal_NSamples								"command n samples"

#define AxisCal_Default_TaskController_Name					"Scan Axis Calibration"
#define AxisCal_Default_NewCalibration_Name					"Calibration"

// Scan engine settings
#define Max_NewScanEngine_NameLength						50
#define Allowed_Detector_Data_Types							{DL_Waveform_UChar, DL_Waveform_UShort, DL_Waveform_Short, DL_Waveform_Float}

// Non-resonant galvo calibration parameters
#define	CALIBRATION_DATA_TO_STRING							"%s<%*f[j1] "
#define STRING_TO_CALIBRATION_DATA							"%s>%*f[j1] "
#define MAX_CAL_NAME_LENGTH									50			// Maximum name for non-resonant galvo scan axis calibration data.
#define CALPOINTS 											50
#define SLOPE_OFFSET_DELAY									0.01		// Time to wait in [s] after a galvo step is made before estimating slope and offset parameters					
#define RELERR 												0.05
#define POSSAMPLES											200 		// Number of AI samples to read and average for one measurement of the galvo position
#define POSTRAMPTIME										20.0		// Recording time in [ms] after a ramp signal is applied
#define MAXFLYBACKTIME										20.0		// Maximum expected galvo flyback time in [ms].
#define FIND_MAXSLOPES_AMP_ITER_FACTOR 						0.9
#define SWITCH_TIMES_AMP_ITER_FACTOR 						0.95 
#define DYNAMICCAL_SLOPE_ITER_FACTOR 						0.90
#define DYNAMICCAL_INITIAL_SLOPE_REDUCTION_FACTOR 			0.8	 		// For each amplitude, the initial maxslope value is reduced by this factor.
#define MAX_SLOPE_REDUCTION_FACTOR 							0.25		// Position lag is measured with quarter the maximum ramp to allow the galvo reach a steady state.
#define VAL_OVLD_ERR -8000
#define Default_ActiveNonResGalvoCal_ScanTime				2.0			// Scan time in [s] to test if using triangle waveform scan	there is an overload		

#define Default_ScanAxisCal_SampleRate						1e5			// Default sampling rate for galvo command and position in [Hz]

//-----------------------------------------------------------------------
// Non-resonant galvo raster scan defaul & min max settings
//-----------------------------------------------------------------------
#define NonResGalvoRasterScan_Max_ComboboxEntryLength		20			// Maximum number of characters that can be entered in the combobox controls.
#define NonResGalvoRasterScan_Default_PixelClockRate		40e6		// Default pixel clock frequency in [Hz].
#define NonResGalvoRasterScan_Default_GalvoSamplingRate		1e5			// Default galvo sampling for command and position signals in [Hz].

	// pixel dwell time
#define NonResGalvoRasterScan_Default_PixelDwellTime		1.0			// Default pixel dwell time in [us].
#define NonResGalvoScan_MaxPixelDwellTime					100.0		// Maximum & minimum pixel dwell time in [us] , should be defined better from info either from the task or the hardware!
#define NonResGalvoScan_MinPixelDwellTime 					0.125		// in [us], depends on hardware and fluorescence integration mode, using photon counting, there is a 25 ns dead time
																		// within each pixel. If the min pix dwell time is 0.125 us then it means that 20% of fluorescence is lost which 
																		// may be still an acceptable situation if speed is important
	// pixel size
#define	NonResGalvoRasterScan_Default_PixelSize				1.0			// Default pixel size in [um].
#define	NonResGalvoRasterScan_Min_PixelSize					0.01		// Minimum allowed pixel size in [um].
#define	NonResGalvoRasterScan_Max_PixelSize					10.0		// Maximum allowed pixel size in [um].


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
	SourceVChan_type*		VChanComNSamples;		// Number of samples per command waveform sent each iteration.
	SinkVChan_type*			VChanPos;				// VChan used to receive position feedback signals from the scan axis (optional, depending on the scan axis type)
	double					samplingRate;			// Galvo calibration sampling rate that can be taken as reference by comSampRate.
	double*					comSampRate;			// Command sample rate in [Hz] taken from VChanCom when connected or from the calibration module. Same rate is used for the position sampling rate.
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
	LaserScanning_type*		lsModule;				// Reference to the laser scanning module that owns this scan axis
	ScanEngine_type*		scanEngine;				// Reference to the scan engine that owns this scan axis
		// METHODS
	void (*Discard) (ScanAxisCal_type** scanAxisCal);
	void (*UpdateOptics) (ScanAxisCal_type* scanAxisCal); // Called by the assigned scan engine when optical settings have been updated.
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
	double* 				maxFreq;				// For given amplitude, maximum triangle wave frequency that the galvo can still follow for the given testing time.
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
	double					commandVMax;			// Maximum galvo command voltage in [V] for maximal deflection in the oposite direction.
	size_t					extraRuns;				// Number of extra runs for triangle waveform calibration.
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
	double 					resolution;				// in [V] for performing Switch Times calibration.
	double 					minStepSize;			// in [V] for performing Max Slopes calibration.
	double 					parked;         		// Value of command signal to be applied to the galvo when parked, in [V].
	double					mechanicalResponse;		// Galvo mechanical response in [deg/V].
	double 					scanTime;				// Time to apply a triangle waveform to estimate galvo response and maximum scan speed in [s].
	Waveform_type*			commandWaveform;		// Command waveform applied to the galvo with each task controller iteration.
	Waveform_type*			positionWaveform;		// Calibration waveforms buffer.
} ActiveNonResGalvoCal_type;

// Non-resonant galvo scan axis calibration data
typedef struct {
	ScanAxisCal_type		baseClass;
	double					commandVMax;			// Maximum galvo command voltage in [V] for maximal deflection.
	double					slope;					// in [V/V], together with offset, it relates the command and position feedback signal in a linear way when the galvo is still.
	double					offset;					// in [V], together with slope, it relates the command and position feedback signal in a linear way when the galvo is still. 
	double					posStdDev;				// in [V], measures noise level on the position feedback signal.
	double					lag;					// Galvo position signal lag with respect to command signal in [ms].
	SwitchTimes_type*		switchTimes;			// Calibration data for jumping between two voltages.
	MaxSlopes_type*			maxSlopes;				// Calibration data for maximum scan speeds.
	TriangleCal_type*		triangleCal;			// Calibration data for triangle waveforms.
	double 					resolution;				// in [V] for performing Switch Times calibration.
	double 					minStepSize;			// in [V] for performing Max Slopes calibration.
	double 					parked;         		// Value of command signal to be applied to the galvo when parked, in [V].
	double					mechanicalResponse;		// Galvo mechanical response in [deg/V].
	double					sampleDisplacement;		// Displacement factor in sample space [um] depending on applied voltage [V] and chosen scan engine optics. The unit of this factor is [um/V].
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
	SinkVChan_type*				detVChan;				// Sink VChan for receiving pixel data.
	ScanEngine_type*			scanEngine;				// Reference to scan engine to which this detection channel belongs.
	DisplayHandle_type			displayHndl;			// Handle to display images for this channel
} DetChan_type;

//---------------------------------------------------------------
// Scan Engine Class
//--------------------------------------------------------------- 
// scan engine child classes
typedef enum {
	ScanEngine_RectRaster_NonResonantGalvoFastAxis_NonResonantGalvoSlowAxis
} ScanEngineEnum_type;

//------------------------------------------------------------------------------------------
// Scan engine operation mode
#define ScanEngineMode_Frames_Name				"Frame scan"
#define ScanEngineMode_PointROIs_Name			"Point scan"
typedef enum {
	ScanEngineMode_Frames,						// Frame scans 
	ScanEngineMode_PointROIs					// Jumps between point ROIs repeatedly
} ScanEngineModesEnum_type;

typedef struct {
	ScanEngineModesEnum_type	mode;
	char*						label;
} ScanEngineModes_type;

 ScanEngineModes_type			scanEngineModes[] = {	{ScanEngineMode_Frames, ScanEngineMode_Frames_Name}, 
	 													{ScanEngineMode_PointROIs, ScanEngineMode_PointROIs_Name}	};
//------------------------------------------------------------------------------------------  
typedef struct {
	char*						objectiveName;			// Objective name.
	double						objectiveFL;			// Objective focal length in [mm].
} Objective_type;

struct ScanEngine {
	//-----------------------------------
	// Scan engine type
	//-----------------------------------
	ScanEngineEnum_type			engineType;
	
	ScanEngineModesEnum_type	scanMode;
	
	//-----------------------------------
	// Reference to axis calibration data
	//-----------------------------------
	ScanAxisCal_type*			fastAxisCal;			// Pixel index changes faster in this scan direction.
	ScanAxisCal_type*			slowAxisCal;
	
	//---------------------------------------
	// Reference to the Laser Scanning module
	//---------------------------------------
	LaserScanning_type*			lsModule;
	
	//-----------------------------------
	// Task Controller
	//-----------------------------------
	TaskControl_type*			taskControl;
	
	//-----------------------------------
	// VChans
	//-----------------------------------
		// Command signals
	SourceVChan_type*			VChanFastAxisCom;   
	SourceVChan_type*			VChanSlowAxisCom;
		// Command signals waveform n samples
	SourceVChan_type*			VChanFastAxisComNSamples;   
	SourceVChan_type*			VChanSlowAxisComNSamples;	
		// Position feedback signals (optional for some scan axis types)
	SinkVChan_type*				VChanFastAxisPos;
	SinkVChan_type*				VChanSlowAxisPos;
		// Scan Engine output
	SourceVChan_type*			VChanScanOut;
		// Mechanical shutter command
	SourceVChan_type*			VChanShutter;
		// Pixel pulse train
	SourceVChan_type*			VChanPixelPulseTrain;
		// N pixels
	SourceVChan_type*			VChanNPixels;
		// Detector input channels of DetChan_type* with incoming fluorescence pixel stream 
	ListType					DetChans;
	
	//-----------------------------------
	// Scan Settings
	//-----------------------------------
	double						pixelClockRate;			// Pixel clock rate in [Hz] for incoming fluorescence data from all detector channels.
														// Note: This is not to be confused with the pixel dwell time, which is a multiple of 1/pixelClockRate.
	double						pixDelay;				// Pixel signal delay in [us] due to processing electronics measured from the moment the signal (fluorescence) is 
														// generated at the sample.
	//-----------------------------------
	// Optics
	//-----------------------------------
	double						scanLensFL;				// Scan lens focal length in [mm]
	double						tubeLensFL;				// Tube lens focal length in [mm]
	Objective_type*				objectiveLens;			// Chosen objective lens
	ListType					objectives;				// List of microscope objectives of Objective_type* elements;
	
	//-----------------------------------
	// UI
	//-----------------------------------
	int							newObjectivePanHndl;
	int							scanSetPanHndl;			// Panel handle for adjusting scan settings such as pixel size, etc...
	int							engineSetPanHndl;		// Panel handle for scan engine settings such as VChans and scan axis types.
	
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
	void*						imagePixels;			// Pixel array for the image assembled so far. Array contains nImagePixels
	uInt64						nImagePixels;			// Total number of pixels in imagePixels. Maximum Array size is imgWidth*imgHeight
	uInt32						nAssembledColumns;		// Number of assembled colums (in the direction of image width).
	void*						tmpPixels;				// Temporary pixels used to assemble an image. Array containing nTmpPixels elements
	size_t						nTmpPixels;				// Number of pixels in tmpPixels.
	size_t						nSkipPixels;			// Number of pixels left to skip from the pixel stream.
	BOOL				        revHeightFlag;		   	// Flag used to reverse every second line in the image in the direction of height.
	BOOL        				revWidthFlag;  			// Flag used to reverse every second completed image in the direction of width.
	DetChan_type*				detChan;				// Detection channel to which this image assembly buffer belongs.
} RectRasterImgBuffer_type;

typedef struct {
	double						pixSize;				// Image pixel size in [um].
	uInt32						height;					// Image height in [pix].
	int							heightOffset;			// Image height offset from center in [pix].
	uInt32						width;					// Image width in [pix].
	int							widthOffset;			// Image width offset in [pix].
	double						pixelDwellTime;			// Pixel dwell time in [us].
} RectRasterScanSet_type;

typedef struct {
	ScanEngine_type				baseClass;				// Base class, must be first structure member.
	
	//----------------
	// Scan parameters
	//----------------
	
	RectRasterScanSet_type		scanSettings;
	double						galvoSamplingRate;		// Default galvo sampling rate set by the scan engine in [Hz].
	double						flyInDelay;				// Galvo fly-in time from parked position to start of the imaging area in [us]. This value is also an integer multiple of the pixelDwellTime. 
	
	//----------------
	// Image buffers
	//----------------
	
	RectRasterImgBuffer_type**	imgBuffers;				// Array of image buffers of RectRasterImgBuffer_type*. Number of buffers and the buffer index is taken from the list of available detection channels (baseClass.DetChans)
	size_t						nImgBuffers;			// Number of image buffers available.
	
	//----------------
	// ROIs
	//----------------
	
	ListType					pointROIs;				// List of Point_type* ROIs
	
} RectRaster_type;


//----------------------
// Module implementation
//----------------------
struct LaserScanning {
	
	// SUPER, must be the first member to inherit from
	
	DAQLabModule_type 			baseClass;
	
	// DATA
	
		// Available calibration data per scan axis. Of ScanAxisCal_type* which can be casted to various child classes such as GalvoCal_type*
	ListType					availableCals;
		// Active calibrations that are in progress. Of ActiveScanAxisCal_type* which can be casted to various child classes such as GalvoCal_type*
	ListType					activeCal;
		// Scan engines of ScanEngine_type* base class which can be casted to specific scan engines. 
	ListType					scanEngines;
		// Display engine
	DisplayEngine_type*			displayEngine;
	
		//-------------------------
		// UI
		//-------------------------
	
	int							mainPanHndl;	  			// Main panel for the laser scanning module.
	int*						mainPanLeftPos;				// Main panel left position to be applied when module is loaded.
	int*						mainPanTopPos;				// Main panel top position to be applied when module is loaded. 
	int							enginesPanHndl;   			// List of available scan engine types.
	int							manageAxisCalPanHndl;		// Panel handle where axis calibrations are managed.
	int							newAxisCalTypePanHndl;		// Panel handle to select different calibrations for various axis types.
	int							menuBarHndl;				// Laser scanning module menu bar.
	int							engineSettingsMenuItemHndl;	// Settings menu item handle for adjusting scan engine settings such as VChans, etc.
	
};

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//-------------------
// Detection channels
//-------------------
static DetChan_type*				init_DetChan_type								(ScanEngine_type* scanEngine, char VChanName[]);

static void							discard_DetChan_type							(DetChan_type** detChanPtr);

//----------------------
// Scan axis calibration
//----------------------
	// generic active scan axis calibration data 

static ActiveScanAxisCal_type*		initalloc_ActiveScanAxisCal_type				(ActiveScanAxisCal_type* cal);

static void							discard_ActiveScanAxisCal_type					(ActiveScanAxisCal_type** cal);

	// generic scan axis calibration data
static void							discard_ScanAxisCal_type						(ScanAxisCal_type** cal);

void CVICALLBACK 					ScanAxisCalibrationMenu_CB						(int menuBarHandle, int menuItemID, void *callbackData, int panelHandle);

static void							UpdateScanEngineCalibrations					(ScanEngine_type* scanEngine);

static void 						UpdateAvailableCalibrations 					(LaserScanning_type* lsModule); 

static int CVICALLBACK 				ManageScanAxisCalib_CB							(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 				NewScanAxisCalib_CB								(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

	// galvo response analysis functions

static int 							FindSlope										(double* signal, size_t nsamples, double samplingRate, double signalStdDev, size_t nRep, double sloperelerr, double* maxslope); 
static int 							MeasureLag										(double* signal1, double* signal2, int nelem); 
static int 							GetStepTimes									(double* signal, int nsamples, double lowlevel, double highlevel, double threshold, int* lowidx, int* highidx, int* stepsamples);

	//-----------------------------------------
	// Non-resonant galvo axis calibration data
	//-----------------------------------------

static ActiveNonResGalvoCal_type* 	init_ActiveNonResGalvoCal_type 					(LaserScanning_type* lsModule, char calName[], char commandVChanName[], char commandNSamplesVChanName[], char positionVChanName[]);

static void							discard_ActiveNonResGalvoCal_type				(ActiveScanAxisCal_type** cal);

static NonResGalvoCal_type*			init_NonResGalvoCal_type						(char calName[], LaserScanning_type* lsModule, double commandVMax, double slope, double offset, double posStdDev, 
																			 		 double lag, SwitchTimes_type* switchTimes, MaxSlopes_type* maxSlopes,
																			 		 TriangleCal_type* triangleCal, double resolution, double minStepSize, double parked, double mechanicalResponse);

static void							discard_NonResGalvoCal_type						(NonResGalvoCal_type** cal);

	// updates optical settings
void								NonResGalvoCal_UpdateOptics						(NonResGalvoCal_type* cal);

	// validation for new scan axis calibration name
static BOOL 						ValidateNewScanAxisCal							(char inputStr[], void* dataPtr); 
	// switch times data
static SwitchTimes_type* 			init_SwitchTimes_type							(void);

static void 						discard_SwitchTimes_type 						(SwitchTimes_type** a);

	// copies switch times data
static SwitchTimes_type*			copy_SwitchTimes_type							(SwitchTimes_type* switchTimes);

	// max slopes data

static MaxSlopes_type* 				init_MaxSlopes_type 							(void);

static void 						discard_MaxSlopes_type 							(MaxSlopes_type** a);
	// copies max slopes data
static MaxSlopes_type* 				copy_MaxSlopes_type 							(MaxSlopes_type* maxSlopes);

	// triangle wave calibration data

static TriangleCal_type* 			init_TriangleCal_type							(void);

static void 						discard_TriangleCal_type 						(TriangleCal_type** a);
	// copies triangle waveform calibration data
static TriangleCal_type* 			copy_TriangleCal_type 							(TriangleCal_type* triangleCal);
	// saves non resonant galvo scan axis calibration data to XML
static int 							SaveNonResGalvoCalToXML							(NonResGalvoCal_type* nrgCal, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_  axisCalibrationsElement);
static int 							LoadNonResGalvoCalFromXML 						(LaserScanning_type* lsModule, ActiveXMLObj_IXMLDOMElement_ axisCalibrationElement);    

	// command VChan
static void							NonResGalvoCal_ComVChan_Connected				(VChan_type* self, void* VChanOwner, VChan_type* connectedVChan);
static void							NonResGalvoCal_ComVChan_Disconnected			(VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan);

	// number of samples in calibration command waveform
static void							NonResGalvoCal_ComNSamplesVChan_Connected		(VChan_type* self, void* VChanOwner, VChan_type* connectedVChan);
static void							NonResGalvoCal_ComNSamplesVChan_Disconnected	(VChan_type* self, void* VChanOwner, VChan_type* connectedVChan);

	// position VChan
static void							NonResGalvoCal_PosVChan_Connected				(VChan_type* self, void* VChanOwner, VChan_type* connectedVChan);
static void							NonResGalvoCal_PosVChan_Disconnected			(VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan);

	// galvo calibration
static int CVICALLBACK 				NonResGalvoCal_MainPan_CB						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static int CVICALLBACK 				NonResGalvoCal_CalPan_CB						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static int CVICALLBACK 				NonResGalvoCal_TestPan_CB						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

	// determines maximum frequency for a triangle waveform with given amplitude, taking into account the calibration data of the galvo
static void							MaxTriangleWaveformScan							(NonResGalvoCal_type* cal, double targetAmplitude, double* maxTriangleWaveFrequency);

	// determines galvo response lag (static+residual dynamic lag) for a triangle waveform  command signal of a given frequency in [ms]
//static double						TriangleWaveformLag								(NonResGalvoCal_type* cal, double triangleWaveFrequency);

	// Moves the galvo between two positions given by startVoltage and endVoltage in [V] given a sampleRate in [Hz] and calibration data.
	// The command signal is a ramp such that the galvo lags behind the command signal with a constant lag
static Waveform_type* 				NonResGalvoMoveBetweenPoints 					(NonResGalvoCal_type* cal, double sampleRate, double startVoltage, double startDelay, 
																					 double endVoltage, double endDelay);

//-------------
// Scan Engines
//-------------
	// parent class
static int 							init_ScanEngine_type 							(ScanEngine_type* 		engine,
																			 	 	 LaserScanning_type*	lsModule,
																			 	 	 ScanEngineEnum_type 	engineType,
																			 	 	 char 					fastAxisComVChanName[],
																					 char					fastAxisComNSampVChanName[],
								 											 	 	 char 					slowAxisComVChanName[], 
																					 char					slowAxisComNSampVChanName[],
								 											 	 	 char					fastAxisPosVChanName[],
								 											 	 	 char					slowAxisPosVChanName[],
								 											 	 	 char					imageOutVChanName[],
																				 	 char					detectorVChanName[],
																				 	 char					shutterVChanName[],
																				 	 char					pixelPulseTrainVChanName[],
																					 char					nPixelsVChanName[],
																				 	 double					pixelClockRate,
																					 double					pixelDelay,
																				 	 double					scanLensFL,
																				 	 double					tubeLensFL);

static void							discard_ScanEngine_type 						(ScanEngine_type** engine);

	//------------------------
	// Common functionality
	//------------------------

	// opens/closes scan engine laser shutter
static int							OpenScanEngineShutter							(ScanEngine_type* engine, BOOL openStatus, char** errorInfo);

	// return both the fast and slow axis galvos to their parked position
static int 							ReturnRectRasterToParkedPosition 				(RectRaster_type* engine, char** errorInfo);  
	
	//--------------------------------------
	// Non-Resonant Rectangle Raster Scan
	//--------------------------------------

static RectRasterScanSet_type*		init_RectRasterScanSet_type						(double pixSize, uInt32 height, int heightOffset, uInt32 width, int widthOffset, double pixelDwellTime); 

static void 						discard_RectRasterScanSet_type 					(RectRasterScanSet_type** scanSetPtr);

static RectRaster_type*				init_RectRaster_type							(LaserScanning_type*	lsModule, 
																			 	 	 char 					engineName[],
														   					 	 	 char 					fastAxisComVChanName[],
																					 char					fastAxisComNSampVChanName[],
								 											 	 	 char 					slowAxisComVChanName[], 
																					 char					slowAxisComNSampVChanName[],
								 											 	 	 char					fastAxisPosVChanName[],
								 											 	 	 char					slowAxisPosVChanName[],
								 											 	 	 char					imageOutVChanName[],
																			 	 	 char					detectorVChanName[],
																				 	 char					shutterVChanName[],
																				 	 char					pixelPulseTrainVChanName[],
																					 char					nPixelsVChanName[],
																				 	 double					galvoSamplingRate,
																				 	 double					pixelClockRate,
																					 double					pixelDelay,
																				 	 uInt32					scanHeight,
																				 	 int					scanHeightOffset,
																				 	 uInt32					scanWidth,
																				 	 int					scanWidthOffset,
																			  	 	 double					pixelSize,
																				 	 double					pixelDwellTime,
																				 	 double					scanLensFL,
																				 	 double					tubeLensFL);

static void							discard_RectRaster_type							(ScanEngine_type** engine);

	// image assembly buffer
static RectRasterImgBuffer_type* 	init_RectRasterImgBuffer_type 					(DetChan_type* detChan, uInt32 imgWidth, uInt32 imgHeight, size_t nSkipPixels, DLDataTypes pixelDataType, BOOL reverseWidthDirection, BOOL reverseHeightDirection);

static void							discard_RectRasterImgBuffer_type				(RectRasterImgBuffer_type** rectRasterPtr); 

static int CVICALLBACK 				NonResRectRasterScan_CB 						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

void 								NonResRectRasterScan_ScanHeights				(RectRaster_type* scanEngine);

void								NonResRectRasterScan_PixelDwellTimes			(RectRaster_type* scanEngine);

	// determines whether a given rectangular ROI falls within a circular perimeter with radius
static BOOL 						RectROIInsideCircle								(double ROIHeight, double ROIWidth, double ROIHeightOffset, double ROIWidthOffset, double circleRadius);
	// determines whether a given rectangular ROI falls within another rectangle of size maxHeight and maxWidth
static BOOL							RectROIInsideRect								(double ROIHeight, double ROIWidth, double ROIHeightOffset, double ROIWidthOffset, double maxHeight, double maxWidth);

	// evaluates whether the scan engine configuration is valid, i.e. it has both scan axes assigned, objective, pixel sampling rate, etc.
static BOOL							NonResRectRasterScan_ValidConfig				(RectRaster_type* scanEngine);
	// evaluates whether the scan engine is ready to scan
static BOOL 						NonResRectRasterScan_ReadyToScan				(RectRaster_type* scanEngine);
	// generates rectangular raster scan waveforms and sends them to the galvo command VChans
static int							NonResRectRasterScan_GenerateScanSignals		(RectRaster_type* scanEngine, char** errorInfo);
	// builds images from a continuous pixel stream
static int 							NonResRectRasterScan_BuildImage 				(RectRaster_type* rectRaster, size_t imgBufferIdx, char** errorInfo);
	// closes the display of an image panel
//void CVICALLBACK 					DisplayClose_CB 								(int menuBar, int menuItem, void *callbackData, int panel);


//---------------------------------------------------------
// determines scan axis types based on the scan engine type
//---------------------------------------------------------
static void							GetScanAxisTypes								(ScanEngine_type* scanEngine, ScanAxis_type* fastAxisType, ScanAxis_type* slowAxisType); 

//-------------------------
// Objectives for scanning
//-------------------------
static Objective_type*				init_Objective_type								(char objectiveName[], double objectiveFL);

static void							discard_Objective_type							(Objective_type** objective);

static int CVICALLBACK 				NewObjective_CB									(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

//---------------------------------------------------------
// Waveforms
//---------------------------------------------------------

static Waveform_type* 				StaircaseWaveform								(BOOL symmetricStaircase, double sampleRate, size_t nSamplesPerStep, size_t nSteps, double startVoltage, double stepVoltage); 

//------------------
// Module management
//------------------

static int							Load 											(DAQLabModule_type* mod, int workspacePanHndl);

static int 							LoadCfg 										(DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  moduleElement);

static int 							SaveCfg 										(DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_  moduleElement);

static int 							DisplayPanels									(DAQLabModule_type* mod, BOOL visibleFlag); 

static int CVICALLBACK 				ScanEngineSettings_CB 							(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 				ScanEnginesTab_CB 								(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

void CVICALLBACK 					ScanEngineSettingsMenu_CB						(int menuBarHandle, int menuItemID, void *callbackData, int panelHandle);


static void CVICALLBACK 			NewScanEngineMenu_CB							(int menuBar, int menuItem, void *callbackData, int panel);

static int CVICALLBACK 				NewScanEngine_CB 								(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static BOOL							ValidScanEngineName								(char name[], void* dataPtr);

//-----------------------------------------
// Scan Engine VChan management
//-----------------------------------------

	// Fast Axis Command VChan
static void							FastAxisComVChan_Connected						(VChan_type* self, void* VChanOwner, VChan_type* connectedVChan);
static void							FastAxisComVChan_Disconnected					(VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan);

	// Slow Axis Command VChan
static void							SlowAxisComVChan_Connected						(VChan_type* self, void* VChanOwner, VChan_type* connectedVChan);
static void							SlowAxisComVChan_Disconnected					(VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan);

	// Fast Axis Position VChan
static void							FastAxisPosVChan_Connected						(VChan_type* self, void* VChanOwner, VChan_type* connectedVChan);
static void							FastAxisPosVChan_Disconnected					(VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan);

	// Slow Axis Position VChan
static void							SlowAxisPosVChan_Connected						(VChan_type* self, void* VChanOwner, VChan_type* connectedVChan);
static void							SlowAxisPosVChan_Disconnected					(VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan);

	// Image Out VChan
static void							ImageOutVChan_Connected							(VChan_type* self, void* VChanOwner, VChan_type* connectedVChan);
static void							ImageOutVChan_Disconnected						(VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan);

	// Detection VChan
static void							DetVChan_Connected								(VChan_type* self, void* VChanOwner, VChan_type* connectedVChan);
static void							DetVChan_Disconnected							(VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan); 

	// Shutter VChan
static void							ShutterVChan_Connected							(VChan_type* self, void* VChanOwner, VChan_type* connectedVChan);
static void							ShutterVChan_Disconnected						(VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan);

//-----------------------------------------
// Display interface
//-----------------------------------------

	// Generates a unique ROI name given an existing ROI list, starting with a single letter "a", 
	// trying each letter alphabetically, after which it increments the number of characters and starts again e.g."aa", "ab"
static char*						GenerateUniqueROIName							(ListType	ROIList);

static void							ROIPlacedOnDisplay_CB							(DisplayHandle_type displayHandle, void* callbackData, ROI_type** ROI);

static void							ROIRemovedFromDisplay_CB						(DisplayHandle_type displayHandle, void* callbackData, ROI_type* ROI);

static int							RestoreImgSettings_CB							(DisplayEngine_type* displayEngine, DisplayHandle_type displayHandle, void* callbackData, char** errorInfo);

static void							DisplayErrorHandler_CB							(DisplayHandle_type displayHandle, int errorID, char* errorInfo);

//-----------------------------------------
// Task Controller Callbacks
//-----------------------------------------

	// for Non Resonant Galvo scan axis calibration and testing
static int							ConfigureTC_NonResGalvoCal						(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static int							UncofigureTC_NonResGalvoCal						(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static void							IterateTC_NonResGalvoCal						(TaskControl_type* taskControl, BOOL const* abortIterationFlag);

static void							AbortIterationTC_NonResGalvoCal					(TaskControl_type* taskControl, BOOL const* abortFlag);

static int							StartTC_NonResGalvoCal							(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static int							DoneTC_NonResGalvoCal							(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static int							StoppedTC_NonResGalvoCal						(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static int				 			ResetTC_NonResGalvoCal 							(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static int							TaskTreeStatus_NonResGalvoCal 					(TaskControl_type* taskControl, TaskTreeExecution_type status, char** errorInfo);

//static int				 			DataReceivedTC_NonResGalvoCal 					(TaskControl_type* taskControl, TaskStates_type taskState, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorInfo);

	// for RectRaster_type scanning
static int							ConfigureTC_RectRaster							(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static int							UnconfigureTC_RectRaster						(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static void							IterateTC_RectRaster							(TaskControl_type* taskControl, BOOL const* abortIterationFlag);

static void							AbortIterationTC_RectRaster						(TaskControl_type* taskControl, BOOL const* abortFlag);

static int							StartTC_RectRaster								(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static int							DoneTC_RectRaster								(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static int							StoppedTC_RectRaster							(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static int				 			ResetTC_RectRaster 								(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static int							TaskTreeStatus_RectRaster 						(TaskControl_type* taskControl, TaskTreeExecution_type status, char** errorInfo);

//static int				 			DataReceivedTC_RectRaster						(TaskControl_type* taskControl, TaskStates_type taskState, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorInfo);

static void 						ErrorTC_RectRaster 								(TaskControl_type* taskControl, int errorID, char* errorMsg);

static int							ModuleEventHandler_RectRaster					(TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, void* eventData, BOOL const* abortFlag, char** errorInfo);

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
DAQLabModule_type*	initalloc_LaserScanning (DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl)
{
	LaserScanning_type*		ls;
	
	if (!mod) {
		ls = malloc (sizeof(LaserScanning_type));
		if (!ls) return NULL;
	} else
		ls = (LaserScanning_type*) mod;
		
	// initialize base class
	initalloc_DAQLabModule(&ls->baseClass, className, instanceName, workspacePanHndl);
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
	ls->displayEngine				= NULL; // added when the module is loading
		
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
	
	// available calibrations
	if (ls->availableCals) {
		size_t 				nItems = ListNumItems(ls->availableCals);
		ScanAxisCal_type**  calPtr;
		for (size_t i = 1; i <= nItems; i++) {
			calPtr = ListGetPtrToItem(ls->availableCals, i);
			(*(*calPtr)->Discard)	(calPtr); 
		}
		
		ListDispose(ls->availableCals);
	}
	
	// active calibrations
	if (ls->activeCal) {
		size_t 				nItems = ListNumItems(ls->activeCal);
		ActiveScanAxisCal_type**  calPtr;
		for (size_t i = 1; i <= nItems; i++) {
			calPtr = ListGetPtrToItem(ls->activeCal, i);
			(*(*calPtr)->Discard)	(calPtr); 
		}
		
		ListDispose(ls->activeCal);
	}
	
	// scan engines
	if (ls->scanEngines) {
		size_t 				nItems = ListNumItems(ls->scanEngines);
		ScanEngine_type**  	enginePtr;
		for (size_t i = 1; i <= nItems; i++) {
			enginePtr = ListGetPtrToItem(ls->scanEngines, i);
			(*(*enginePtr)->Discard)	(enginePtr);
		       
			
		}
		
		ListDispose(ls->scanEngines);
	}
	
	// display engine
	discard_DisplayEngine_type(&ls->displayEngine);
	
	// UI
	OKfree(ls->mainPanLeftPos);
	OKfree(ls->mainPanTopPos);
	if (ls->mainPanHndl) {DiscardPanel(ls->mainPanHndl); ls->mainPanHndl = 0;}
	if (ls->enginesPanHndl) {DiscardPanel(ls->enginesPanHndl); ls->enginesPanHndl = 0;}
	if (ls->manageAxisCalPanHndl) {DiscardPanel(ls->manageAxisCalPanHndl); ls->manageAxisCalPanHndl = 0;}
	if (ls->newAxisCalTypePanHndl) {DiscardPanel(ls->newAxisCalTypePanHndl); ls->newAxisCalTypePanHndl = 0;}

	//discard combo boxes here?
 
	if (ls->menuBarHndl) DiscardMenuBar (ls->menuBarHndl);
	
	
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
	
	//----------------------------------------------------------------------
	// load scan engines
	//----------------------------------------------------------------------
	ScanEngine_type**	scanEnginePtr;
	size_t				nScanEngines 		= ListNumItems(ls->scanEngines);
	size_t				nDetectorVChans;
	int					scanPanHndl			= 0;
	int					newTabIdx			= 0; 
	char*				scanEngineName		= NULL;
	DetChan_type**		detPtr;
	size_t				nObjectives;
	Objective_type**	objectivePtr;
	
	
	for (size_t i = 1; i <= nScanEngines; i++) {
		scanEnginePtr = ListGetPtrToItem(ls->scanEngines, i);
		nObjectives = ListNumItems((*scanEnginePtr)->objectives);
		// load scan pannel depending on scan engine type
		switch ((*scanEnginePtr)->engineType) {
			//------------------------------------------------------------------------------------------------------------------------------------------------------
			// ScanEngine_RectRaster_NonResonantGalvoFastAxis_NonResonantGalvoSlowAxis
			//------------------------------------------------------------------------------------------------------------------------------------------------------ 
			case ScanEngine_RectRaster_NonResonantGalvoFastAxis_NonResonantGalvoSlowAxis:
				
				RectRaster_type*	rectRasterScanEngine = (RectRaster_type*) *scanEnginePtr;
				
				scanPanHndl = LoadPanel(ls->mainPanHndl, MOD_LaserScanning_UI, RectRaster);
				
				// populate scan engine modes
				for (size_t j = 0; j < NumElem(scanEngineModes); j++)
					InsertListItem(scanPanHndl, RectRaster_Mode, -1, scanEngineModes[j].label, scanEngineModes[j].mode);
				SetCtrlIndex(scanPanHndl, RectRaster_Mode, 0);
							
				// populate objectives
				for (size_t j = 1; j <= nObjectives; j++) {
					objectivePtr = ListGetPtrToItem((*scanEnginePtr)->objectives, j);
					InsertListItem(scanPanHndl, RectRaster_Objective, -1, (*objectivePtr)->objectiveName, (*objectivePtr)->objectiveFL);
					// select assigned objective
					if (!strcmp((*objectivePtr)->objectiveName, (*scanEnginePtr)->objectiveLens->objectiveName))
						SetCtrlIndex(scanPanHndl, RectRaster_Objective, j-1);
				}
				
				// update width
				SetCtrlVal(scanPanHndl, RectRaster_Width, rectRasterScanEngine->scanSettings.width * rectRasterScanEngine->scanSettings.pixSize);
				
				// update height and width offsets
				SetCtrlVal(scanPanHndl, RectRaster_WidthOffset, rectRasterScanEngine->scanSettings.widthOffset * rectRasterScanEngine->scanSettings.pixSize); 
				SetCtrlVal(scanPanHndl, RectRaster_HeightOffset, rectRasterScanEngine->scanSettings.heightOffset * rectRasterScanEngine->scanSettings.pixSize);
				
				// update pixel size & set boundaries
				SetCtrlVal(scanPanHndl, RectRaster_PixelSize, ((RectRaster_type*)(*scanEnginePtr))->scanSettings.pixSize);
				SetCtrlAttribute(scanPanHndl, RectRaster_PixelSize, ATTR_MIN_VALUE, NonResGalvoRasterScan_Min_PixelSize);
				SetCtrlAttribute(scanPanHndl, RectRaster_PixelSize, ATTR_MAX_VALUE, NonResGalvoRasterScan_Max_PixelSize);
				SetCtrlAttribute(scanPanHndl, RectRaster_PixelSize, ATTR_CHECK_RANGE, VAL_COERCE);
				
				break;
				
			// insert here more scan engine types
		}
		
		newTabIdx = InsertPanelAsTabPage(ls->mainPanHndl, ScanPan_ScanEngines, -1, scanPanHndl);
		DiscardPanel(scanPanHndl);
		scanPanHndl = 0;
		GetPanelHandleFromTabPage(ls->mainPanHndl, ScanPan_ScanEngines, newTabIdx, &(*scanEnginePtr)->scanSetPanHndl);
		
		// change new scan engine tab title
		scanEngineName = GetTaskControlName((*scanEnginePtr)->taskControl); 
		SetTabPageAttribute(ls->mainPanHndl, ScanPan_ScanEngines, newTabIdx, ATTR_LABEL_TEXT, scanEngineName);
		OKfree(scanEngineName);
		
		// add pannel callback data
		SetPanelAttribute((*scanEnginePtr)->scanSetPanHndl, ATTR_CALLBACK_DATA, *scanEnginePtr);
		
		// add callback function and data to scan engine controls
		switch ((*scanEnginePtr)->engineType) {
				
			case ScanEngine_RectRaster_NonResonantGalvoFastAxis_NonResonantGalvoSlowAxis:
				
				RectRaster_type*	rectRasterScanEngine = (RectRaster_type*) *scanEnginePtr;  
				
				SetCtrlsInPanCBInfo(*scanEnginePtr, NonResRectRasterScan_CB, (*scanEnginePtr)->scanSetPanHndl);
				
				// make Height string control into combo box
				// do this after calling SetCtrlsInPanCBInfo, otherwise the combobox is disrupted
				errChk( Combo_NewComboBox((*scanEnginePtr)->scanSetPanHndl, RectRaster_Height) );
				errChk( Combo_SetComboAttribute((*scanEnginePtr)->scanSetPanHndl, RectRaster_Height, ATTR_COMBO_MAX_LENGTH, NonResGalvoRasterScan_Max_ComboboxEntryLength) );
				
				// update height
				char heightString[NonResGalvoRasterScan_Max_ComboboxEntryLength+1];
				Fmt(heightString, "%s<%f[p1]", rectRasterScanEngine->scanSettings.height * rectRasterScanEngine->scanSettings.pixSize);
				if (rectRasterScanEngine->scanSettings.height)
					SetCtrlVal((*scanEnginePtr)->scanSetPanHndl, RectRaster_Height, heightString); 
				else
					SetCtrlVal((*scanEnginePtr)->scanSetPanHndl, RectRaster_Height, ""); 
				
				// make Pixel Dwell time string control into combo box
				// do this after calling SetCtrlsInPanCBInfo, otherwise the combobox is disrupted  
				errChk( Combo_NewComboBox((*scanEnginePtr)->scanSetPanHndl, RectRaster_PixelDwell) );
				errChk( Combo_SetComboAttribute((*scanEnginePtr)->scanSetPanHndl, RectRaster_PixelDwell, ATTR_COMBO_MAX_LENGTH, NonResGalvoRasterScan_Max_ComboboxEntryLength) );
				
				// update pixel dwell time
				char pixelDwellString[NonResGalvoRasterScan_Max_ComboboxEntryLength+1];
				Fmt(pixelDwellString, "%s<%f[p1]", rectRasterScanEngine->scanSettings.pixelDwellTime);
				if (rectRasterScanEngine->scanSettings.pixelDwellTime != 0.0)
					SetCtrlVal((*scanEnginePtr)->scanSetPanHndl, RectRaster_PixelDwell, pixelDwellString);
				else
					SetCtrlVal((*scanEnginePtr)->scanSetPanHndl, RectRaster_PixelDwell, "");
				
				// configure/unconfigure scan engine
				// do this after adding combo boxes!
				if (NonResRectRasterScan_ValidConfig(rectRasterScanEngine)) {
				
					NonResRectRasterScan_ScanHeights(rectRasterScanEngine);
					NonResRectRasterScan_PixelDwellTimes(rectRasterScanEngine);
					
					if (NonResRectRasterScan_ReadyToScan((RectRaster_type*)*scanEnginePtr)) 
						TaskControlEvent((*scanEnginePtr)->taskControl, TASK_EVENT_CONFIGURE, NULL, NULL);
					else
						TaskControlEvent((*scanEnginePtr)->taskControl, TASK_EVENT_UNCONFIGURE, NULL, NULL); 	
				} else
					TaskControlEvent((*scanEnginePtr)->taskControl, TASK_EVENT_UNCONFIGURE, NULL, NULL);
				   
				break;
				
			// insert here more scan engine types
		}
		
		// add scan engine task controller to DAQLab framework
		DLAddTaskController((DAQLabModule_type*)ls, (*scanEnginePtr)->taskControl);
		
		// add scan engine VChans to DAQLab framework and register with task controller
		DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)(*scanEnginePtr)->VChanFastAxisCom);
		DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)(*scanEnginePtr)->VChanFastAxisComNSamples);
		DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)(*scanEnginePtr)->VChanSlowAxisCom);
		DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)(*scanEnginePtr)->VChanSlowAxisComNSamples);
		DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)(*scanEnginePtr)->VChanFastAxisPos);
		AddSinkVChan((*scanEnginePtr)->taskControl, (*scanEnginePtr)->VChanFastAxisPos, NULL); 
		DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)(*scanEnginePtr)->VChanSlowAxisPos);
		AddSinkVChan((*scanEnginePtr)->taskControl, (*scanEnginePtr)->VChanSlowAxisPos, NULL);
		DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)(*scanEnginePtr)->VChanScanOut);
		DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)(*scanEnginePtr)->VChanShutter);
		DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)(*scanEnginePtr)->VChanPixelPulseTrain); 
		DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)(*scanEnginePtr)->VChanNPixels); 
		
		nDetectorVChans = ListNumItems((*scanEnginePtr)->DetChans);
		for (size_t j = 1; j <= nDetectorVChans; j++) {
			detPtr = ListGetPtrToItem((*scanEnginePtr)->DetChans, j);
			DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)(*detPtr)->detVChan);
			AddSinkVChan((*scanEnginePtr)->taskControl, (*detPtr)->detVChan, NULL);
		}
		
	}
	
	if (nScanEngines) {
		// undim scan engine "Settings" menu item if there is at least one scan engine   
		SetMenuBarAttribute(ls->menuBarHndl, ls->engineSettingsMenuItemHndl, ATTR_DIMMED, 0);
		// remove empty "None" tab page  
		DeleteTabPage(ls->mainPanHndl, ScanPan_ScanEngines, 0, 1); 
	}
	
	// initialize display engine
	intptr_t workspaceWndHndl = 0;	
	GetPanelAttribute(ls->baseClass.workspacePanHndl, ATTR_SYSTEM_WINDOW_HANDLE, &workspaceWndHndl);
	nullChk( ls->displayEngine = (DisplayEngine_type*)init_NIVisionDisplay_type(	workspaceWndHndl,
																			   		ROIPlacedOnDisplay_CB,
																					ROIRemovedFromDisplay_CB,
																					DisplayErrorHandler_CB		) );
	
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
		OKFreeCAHandle(scanAxisCalXMLElement);  
	}
	
	// add scan axis calibrations element to module element
	XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (moduleElement, &xmlERRINFO, scanAxisCalibrationsXMLElement, NULL) );
	OKFreeCAHandle(scanAxisCalibrationsXMLElement); 
	
	//--------------------------------------------------------------------------
	// Save scan engines and VChans
	//--------------------------------------------------------------------------
	ActiveXMLObj_IXMLDOMElement_	scanEnginesXMLElement; 			  	// element containing multiple scan engines
	ActiveXMLObj_IXMLDOMElement_	scanEngineXMLElement; 			  	// scan engine element
	ActiveXMLObj_IXMLDOMElement_	VChanNamesXMLElement;	 			// VChan names element 
	ActiveXMLObj_IXMLDOMElement_	scanInfoXMLElement;					// Contains scan engine info;
	ActiveXMLObj_IXMLDOMElement_	objectivesXMLElement;				// Objectives element containing multiple scan engine objectives
	ActiveXMLObj_IXMLDOMElement_	objectiveXMLElement;				// Objectives element
	
	// create scan engines element
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, "ScanEngines", &scanEnginesXMLElement) );
	
	size_t					nScanEngines 					= ListNumItems(ls->scanEngines);
	ScanEngine_type**		scanEnginePtr;
	DAQLabXMLNode 			scanEngineAttr[9];
	DAQLabXMLNode			objectivesAttr[2];	
	unsigned int			scanEngineType;
	char* 					fastAxisCommandVChanName		= NULL;
	char*					fastAxisCommandNSampVChanName	= NULL;
	char* 					slowAxisCommandVChanName		= NULL;
	char*					slowAxisCommandNSampVChanName	= NULL;
	char* 					fastAxisPositionVChanName		= NULL;
	char* 					slowAxisPositionVChanName		= NULL;
	char* 					scanEngineOutVChanName			= NULL;
	char*					shutterVChanName				= NULL;
	char*					pixelPulseTrainVChanName		= NULL;
	char*					nPixelsVChanName				= NULL;
	DAQLabXMLNode			scanEngineVChansAttr[10];
	DAQLabXMLNode			detectorVChanAttr;
	size_t					nDetectionChans;
	size_t					nObjectives;
	DetChan_type**			detChanPtr;
	Objective_type**		objectivePtr;
	
	
	for (size_t i = 1; i <= nScanEngines; i++) {
		scanEnginePtr = ListGetPtrToItem(ls->scanEngines, i);
		// create new scan engine element
		XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, "ScanEngine", &scanEngineXMLElement) );
		// create new scan info element
		XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, "ScanInfo", &scanInfoXMLElement) );   
		// create new VChan element with all the VChan names for the Scan Engine
		XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, "VChannels", &VChanNamesXMLElement) );
		// create new objectives element
		XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, "Objectives", &objectivesXMLElement) );
		// initialize generic scan engine attributes
		scanEngineAttr[0].tag 			= "Name";
		scanEngineAttr[0].type 			= BasicData_CString;
		scanEngineAttr[0].pData			= GetTaskControlName((*scanEnginePtr)->taskControl);
		
		scanEngineAttr[1].tag 			= "ScanEngineType";
		scanEngineType 					= (unsigned int)(*scanEnginePtr)->engineType;
		scanEngineAttr[1].type 			= BasicData_UInt;
		scanEngineAttr[1].pData			= &scanEngineType;
		
		scanEngineAttr[2].tag			= "FastAxisCalibrationName";
		scanEngineAttr[2].type 			= BasicData_CString;
		if ((*scanEnginePtr)->fastAxisCal)
			scanEngineAttr[2].pData		= (*scanEnginePtr)->fastAxisCal->calName;
		else
			scanEngineAttr[2].pData 	= "";
		
		scanEngineAttr[3].tag			= "SlowAxisCalibrationName";
		scanEngineAttr[3].type 			= BasicData_CString;
		if ((*scanEnginePtr)->slowAxisCal)
			scanEngineAttr[3].pData		= (*scanEnginePtr)->slowAxisCal->calName;
		else
			scanEngineAttr[3].pData		= ""; 
		
		// save optics
		scanEngineAttr[4].tag 			= "ScanLensFL";
		scanEngineAttr[4].type 			= BasicData_Double;
		scanEngineAttr[4].pData			= &(*scanEnginePtr)->scanLensFL;
		scanEngineAttr[5].tag 			= "TubeLensFL";
		scanEngineAttr[5].type 			= BasicData_Double;
		scanEngineAttr[5].pData			= &(*scanEnginePtr)->tubeLensFL;
		scanEngineAttr[6].tag 			= "Objective";
		scanEngineAttr[6].type 			= BasicData_CString;
		if ((*scanEnginePtr)->objectiveLens)
			scanEngineAttr[6].pData		= (*scanEnginePtr)->objectiveLens->objectiveName;
		else
			scanEngineAttr[6].pData 	= "";
		// save scan settings
		scanEngineAttr[7].tag 			= "PixelClockRate";
		scanEngineAttr[7].type 			= BasicData_Double;
		scanEngineAttr[7].pData			= &(*scanEnginePtr)->pixelClockRate;
		
		scanEngineAttr[8].tag 			= "PixelSignalDelay";
		scanEngineAttr[8].type 			= BasicData_Double;
		scanEngineAttr[8].pData			= &(*scanEnginePtr)->pixDelay;
			
		// save attributes
		DLAddToXMLElem(xmlDOM, scanEngineXMLElement, scanEngineAttr, DL_ATTRIBUTE, NumElem(scanEngineAttr));
		OKfree(scanEngineAttr[0].pData);
		
		// save generic scan engine VChans:scan axis & scan engine out 
		fastAxisCommandVChanName 		= GetVChanName((VChan_type*)(*scanEnginePtr)->VChanFastAxisCom);
		fastAxisCommandNSampVChanName	= GetVChanName((VChan_type*)(*scanEnginePtr)->VChanFastAxisComNSamples); 
		slowAxisCommandVChanName 		= GetVChanName((VChan_type*)(*scanEnginePtr)->VChanSlowAxisCom);
		slowAxisCommandNSampVChanName	= GetVChanName((VChan_type*)(*scanEnginePtr)->VChanSlowAxisComNSamples);   
		fastAxisPositionVChanName 		= GetVChanName((VChan_type*)(*scanEnginePtr)->VChanFastAxisPos);
		slowAxisPositionVChanName 		= GetVChanName((VChan_type*)(*scanEnginePtr)->VChanSlowAxisPos);
		scanEngineOutVChanName			= GetVChanName((VChan_type*)(*scanEnginePtr)->VChanScanOut);
		shutterVChanName				= GetVChanName((VChan_type*)(*scanEnginePtr)->VChanShutter);
		pixelPulseTrainVChanName		= GetVChanName((VChan_type*)(*scanEnginePtr)->VChanPixelPulseTrain);
		nPixelsVChanName				= GetVChanName((VChan_type*)(*scanEnginePtr)->VChanNPixels);
		
				
		scanEngineVChansAttr[0].tag 	= "FastAxisCommand";
		scanEngineVChansAttr[0].type   	= BasicData_CString;
		scanEngineVChansAttr[0].pData	= fastAxisCommandVChanName;
		
		scanEngineVChansAttr[1].tag 	= "FastAxisCommandNSamples";
		scanEngineVChansAttr[1].type   	= BasicData_CString;
		scanEngineVChansAttr[1].pData	= fastAxisCommandNSampVChanName;
		
		scanEngineVChansAttr[2].tag 	= "SlowAxisCommand";
		scanEngineVChansAttr[2].type   	= BasicData_CString;
		scanEngineVChansAttr[2].pData	= slowAxisCommandVChanName;
		
		scanEngineVChansAttr[3].tag 	= "SlowAxisCommandNSamples";
		scanEngineVChansAttr[3].type   	= BasicData_CString;
		scanEngineVChansAttr[3].pData	= slowAxisCommandNSampVChanName;
		
		scanEngineVChansAttr[4].tag 	= "FastAxisPosition";
		scanEngineVChansAttr[4].type   	= BasicData_CString;
		scanEngineVChansAttr[4].pData	= fastAxisPositionVChanName;
		
		scanEngineVChansAttr[5].tag 	= "SlowAxisPosition";
		scanEngineVChansAttr[5].type   	= BasicData_CString;
		scanEngineVChansAttr[5].pData	= slowAxisPositionVChanName;
		
		scanEngineVChansAttr[6].tag 	= "ScanEngineOut";
		scanEngineVChansAttr[6].type   	= BasicData_CString;
		scanEngineVChansAttr[6].pData	= scanEngineOutVChanName;
		
		scanEngineVChansAttr[7].tag 	= "Shutter";
		scanEngineVChansAttr[7].type   	= BasicData_CString;
		scanEngineVChansAttr[7].pData	= shutterVChanName;
		
		scanEngineVChansAttr[8].tag 	= "PixelPulseTrain";
		scanEngineVChansAttr[8].type   	= BasicData_CString;
		scanEngineVChansAttr[8].pData	= pixelPulseTrainVChanName;
		
		scanEngineVChansAttr[9].tag 	= "NPixels";
		scanEngineVChansAttr[9].type   	= BasicData_CString;
		scanEngineVChansAttr[9].pData	= nPixelsVChanName;
														 	
		DLAddToXMLElem(xmlDOM, VChanNamesXMLElement, scanEngineVChansAttr, DL_ATTRIBUTE, NumElem(scanEngineVChansAttr));
				
		OKfree(fastAxisCommandVChanName);
		OKfree(fastAxisCommandNSampVChanName);
		OKfree(slowAxisCommandVChanName);
		OKfree(slowAxisCommandNSampVChanName);
		OKfree(fastAxisPositionVChanName);
		OKfree(slowAxisPositionVChanName);
		OKfree(scanEngineOutVChanName);
		OKfree(shutterVChanName);
		OKfree(pixelPulseTrainVChanName);
		
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
		
		// save objectives to objectives XML element
		nObjectives = ListNumItems((*scanEnginePtr)->objectives);
		for (size_t j = 0; j < nObjectives; j++) {
			objectivePtr = ListGetPtrToItem((*scanEnginePtr)->objectives, j);
			objectivesAttr[0].tag		= "Name";
			objectivesAttr[0].type		= BasicData_CString;
			objectivesAttr[0].pData		= (*objectivePtr)->objectiveName;
			objectivesAttr[1].tag		= "FL";
			objectivesAttr[1].type		= BasicData_Double;
			objectivesAttr[1].pData		= &(*objectivePtr)->objectiveFL;
			
			XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, "Objective", &objectiveXMLElement) );
			DLAddToXMLElem(xmlDOM, objectiveXMLElement, objectivesAttr, DL_ATTRIBUTE, NumElem(objectivesAttr)); 
			XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (objectivesXMLElement, &xmlERRINFO, objectiveXMLElement, NULL) ); 
			OKFreeCAHandle(objectiveXMLElement);   
		}
		
		// add scan engine specific data
		switch((*scanEnginePtr)->engineType) {
				
			case ScanEngine_RectRaster_NonResonantGalvoFastAxis_NonResonantGalvoSlowAxis:
				
				// initialize raster scan attributes
				unsigned int			height					= ((RectRaster_type*) *scanEnginePtr)->scanSettings.height;  
				unsigned int			width					= ((RectRaster_type*) *scanEnginePtr)->scanSettings.width;  
				int						heightOffset			= ((RectRaster_type*) *scanEnginePtr)->scanSettings.heightOffset;  
				int						widthOffset				= ((RectRaster_type*) *scanEnginePtr)->scanSettings.widthOffset; 
				double					pixelSize				= ((RectRaster_type*) *scanEnginePtr)->scanSettings.pixSize;
				double					pixelDwellTime			= ((RectRaster_type*) *scanEnginePtr)->scanSettings.pixelDwellTime;
				double					galvoSamplingRate		= ((RectRaster_type*) *scanEnginePtr)->galvoSamplingRate;
						
				DAQLabXMLNode			rectangleRasterAttr[] 	= { {"ImageHeight", BasicData_UInt, &height},
																	{"ImageWidth",  BasicData_UInt, &width},
																	{"ImageHeightOffset", BasicData_Int, &heightOffset},
																	{"ImageWidthOffset", BasicData_Int, &widthOffset},
																	{"PixelSize", BasicData_Double, &pixelSize},
																	{"PixelDwellTime", BasicData_Double, &pixelDwellTime},
																	{"GalvoSamplingRate", BasicData_Double, &galvoSamplingRate}}; 				
				
				// save scan engine attributes
				DLAddToXMLElem(xmlDOM, scanInfoXMLElement, rectangleRasterAttr, DL_ATTRIBUTE, NumElem(rectangleRasterAttr));
				
				break;
		}
		// add VChan Names element to scan engine element
		// add new scan engine element
		XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (scanEngineXMLElement, &xmlERRINFO, VChanNamesXMLElement, NULL) ); 
		OKFreeCAHandle(VChanNamesXMLElement); 
		
		// add scan info element to scan engine element
		XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (scanEngineXMLElement, &xmlERRINFO, scanInfoXMLElement, NULL) ); 
		OKFreeCAHandle(scanInfoXMLElement); 
		
		// add objectives to scan engine element
		XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (scanEngineXMLElement, &xmlERRINFO, objectivesXMLElement, NULL) );    
		OKFreeCAHandle(objectivesXMLElement);
		
		// add new scan engine element
		XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (scanEnginesXMLElement, &xmlERRINFO, scanEngineXMLElement, NULL) );  
		OKFreeCAHandle(scanEngineXMLElement); 
	}
	
	// add scan engines element to module element
	XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (moduleElement, &xmlERRINFO, scanEnginesXMLElement, NULL) );
	OKFreeCAHandle(scanEnginesXMLElement); 
	
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
	ActiveXMLObj_IXMLDOMElement_	scanEnginesXMLElement 			= 0;   			// element containing multiple scan engines
	ActiveXMLObj_IXMLDOMElement_	VChansXMLElement				= 0;
	ActiveXMLObj_IXMLDOMElement_	scanInfoXMLElement				= 0;
	ActiveXMLObj_IXMLDOMElement_	objectivesXMLElement			= 0;
	ActiveXMLObj_IXMLDOMNodeList_	scanEngineNodeList				= 0;
	ActiveXMLObj_IXMLDOMNodeList_	objectivesNodeList				= 0;
	ActiveXMLObj_IXMLDOMNodeList_	detectorVChanNodeList			= 0;
	ActiveXMLObj_IXMLDOMNode_		scanEngineNode					= 0;
	ActiveXMLObj_IXMLDOMNode_		objectiveNode					= 0;
	ActiveXMLObj_IXMLDOMNode_		detectorVChanNode				= 0;
	long							nScanEngines					= 0;
	long							nObjectives						= 0;
	long							nDetectorVChans					= 0;
	unsigned int					scanEngineType					= 0;
	char*							scanEngineName					= NULL;
	char*							fastAxisCalibrationName			= NULL;
	char*							slowAxisCalibrationName			= NULL;
	char*							fastAxisCommandVChanName		= NULL;
	char*							fastAxisCommandNSampVChanName	= NULL;
	char*							slowAxisCommandVChanName		= NULL;
	char*							slowAxisCommandNSampVChanName	= NULL;
	char*							fastAxisPositionVChanName		= NULL;
	char*							slowAxisPositionVChanName		= NULL;
	char*							scanEngineOutVChanName			= NULL;
	char*							detectorVChanName				= NULL;
	char*							shutterVChanName				= NULL;
	char*							pixelPulseTrainVChanName		= NULL;
	char*							nPixelsVChanName				= NULL;
	double							scanLensFL						= 0;
	double							tubeLensFL						= 0;
	double							objectiveFL						= 0;
	double							pixelClockRate					= 0;
	double							pixelDelay						= 0;
	char*							assignedObjectiveName			= NULL;
	char*							objectiveName					= NULL;
	DetChan_type*					detChan							= NULL;
	ScanEngine_type*				scanEngine						= NULL;
	Objective_type*					objective						= NULL;
	DAQLabXMLNode					scanEngineGenericAttr[] 		= {	{"Name", BasicData_CString, &scanEngineName},
																		{"ScanEngineType", BasicData_UInt, &scanEngineType},
																		{"FastAxisCalibrationName", BasicData_CString, &fastAxisCalibrationName},
																		{"SlowAxisCalibrationName", BasicData_CString, &slowAxisCalibrationName},
																		{"ScanLensFL", BasicData_Double, &scanLensFL},
																		{"TubeLensFL", BasicData_Double, &tubeLensFL},
																		{"Objective", BasicData_CString, &assignedObjectiveName},
																		{"PixelClockRate", BasicData_Double, &pixelClockRate},
																		{"PixelSignalDelay", BasicData_Double, &pixelDelay} };
																	
	DAQLabXMLNode					scanEngineVChansAttr[]			= {	{"FastAxisCommand", BasicData_CString, &fastAxisCommandVChanName},
																		{"FastAxisCommandNSamples", BasicData_CString, &fastAxisCommandNSampVChanName},
																		{"SlowAxisCommand", BasicData_CString, &slowAxisCommandVChanName},
																		{"SlowAxisCommandNSamples", BasicData_CString, &slowAxisCommandNSampVChanName},  
																		{"FastAxisPosition", BasicData_CString, &fastAxisPositionVChanName},
																		{"SlowAxisPosition", BasicData_CString, &slowAxisPositionVChanName},
																		{"ScanEngineOut", BasicData_CString, &scanEngineOutVChanName},
																		{"Shutter", BasicData_CString, &shutterVChanName},
																		{"PixelPulseTrain", BasicData_CString, &pixelPulseTrainVChanName},
																		{"NPixels", BasicData_CString, &nPixelsVChanName} };
																	
	DAQLabXMLNode					objectiveAttr[]					= { {"Name", BasicData_CString, &objectiveName},
																		{"FL", BasicData_Double, &objectiveFL} };
		
	errChk( DLGetSingleXMLElementFromElement(moduleElement, "ScanEngines", &scanEnginesXMLElement) );
	
	XMLErrChk ( ActiveXML_IXMLDOMElement_getElementsByTagName(scanEnginesXMLElement, &xmlERRINFO, "ScanEngine", &scanEngineNodeList) );
	XMLErrChk ( ActiveXML_IXMLDOMNodeList_Getlength(scanEngineNodeList, &xmlERRINFO, &nScanEngines) );
	
	for (long i = 0; i < nScanEngines; i++) {
		XMLErrChk ( ActiveXML_IXMLDOMNodeList_Getitem(scanEngineNodeList, &xmlERRINFO, i, &scanEngineNode) );
		// get generic scan engine attributes
		errChk( DLGetXMLNodeAttributes(scanEngineNode, scanEngineGenericAttr, NumElem(scanEngineGenericAttr)) );
		// get scan engine VChans element
		errChk( DLGetSingleXMLElementFromElement((ActiveXMLObj_IXMLDOMElement_)scanEngineNode, "VChannels", &VChansXMLElement) );
		// get scan info element
		errChk( DLGetSingleXMLElementFromElement((ActiveXMLObj_IXMLDOMElement_)scanEngineNode, "ScanInfo", &scanInfoXMLElement) );
		// get scan engine VChan attributes
		errChk( DLGetXMLElementAttributes(VChansXMLElement, scanEngineVChansAttr, NumElem(scanEngineVChansAttr)) ); 
		// get objectives XML element
		errChk( DLGetSingleXMLElementFromElement((ActiveXMLObj_IXMLDOMElement_)scanEngineNode, "Objectives", &objectivesXMLElement ) );  
		
		switch (scanEngineType) {
				
			case ScanEngine_RectRaster_NonResonantGalvoFastAxis_NonResonantGalvoSlowAxis:
				
				uInt32				height;
				int					heightOffset;
				uInt32				width;
				int					widthOffset;
				double				pixelSize;
				double				pixelDwellTime;
				double				galvoSamplingRate;
				
				DAQLabXMLNode		scanInfoAttr[]	= { {"ImageHeight", BasicData_UInt, &height},
														{"ImageHeightOffset", BasicData_Int, &heightOffset},
														{"ImageWidth", BasicData_UInt, &width},
														{"ImageWidthOffset", BasicData_Int, &widthOffset},
														{"PixelSize", BasicData_Double, &pixelSize},
														{"PixelDwellTime", BasicData_Double, &pixelDwellTime},
														{"GalvoSamplingRate", BasicData_Double, &galvoSamplingRate}};
														
				// get scan info attributes
				errChk( DLGetXMLElementAttributes(scanInfoXMLElement, scanInfoAttr, NumElem(scanInfoAttr)) );
				
				//-----------------------------------
				// coerce min and max limits
				//-----------------------------------
					// pixel dwell time
				if (pixelDwellTime < NonResGalvoScan_MinPixelDwellTime)
					pixelDwellTime = NonResGalvoScan_MinPixelDwellTime;
				if (pixelDwellTime > NonResGalvoScan_MaxPixelDwellTime)
					pixelDwellTime = NonResGalvoScan_MaxPixelDwellTime;
				
					// pixel size
				if (pixelSize < NonResGalvoRasterScan_Min_PixelSize)
					pixelSize = NonResGalvoRasterScan_Min_PixelSize;
				if (pixelSize > NonResGalvoRasterScan_Max_PixelSize)
					pixelSize = NonResGalvoRasterScan_Max_PixelSize;
				
					// image height (pix)
				if (!height) height = 1;
				
					// image width (pix)
				if (!width) width = 1;
					
				//-----------------------------------  
					
				
				scanEngine = (ScanEngine_type*)init_RectRaster_type((LaserScanning_type*)mod, scanEngineName, fastAxisCommandVChanName, fastAxisCommandNSampVChanName, slowAxisCommandVChanName, slowAxisCommandNSampVChanName, fastAxisPositionVChanName,
													  slowAxisPositionVChanName, scanEngineOutVChanName, NULL, shutterVChanName, pixelPulseTrainVChanName, nPixelsVChanName, galvoSamplingRate, pixelClockRate, pixelDelay, height, heightOffset, width, widthOffset, 
													  pixelSize, pixelDwellTime, scanLensFL, tubeLensFL); 
				break;
			
		}
		
		// load detector VChans
		XMLErrChk ( ActiveXML_IXMLDOMElement_getElementsByTagName(VChansXMLElement, &xmlERRINFO, "DetectorChannel", &detectorVChanNodeList) );
		XMLErrChk ( ActiveXML_IXMLDOMNodeList_Getlength(detectorVChanNodeList, &xmlERRINFO, &nDetectorVChans) );
		
		for (long j = 0; j < nDetectorVChans; j++) {
			XMLErrChk ( ActiveXML_IXMLDOMNodeList_Getitem(detectorVChanNodeList, &xmlERRINFO, j, &detectorVChanNode) );
			XMLErrChk ( ActiveXML_IXMLDOMNode_Gettext(detectorVChanNode, &xmlERRINFO, &detectorVChanName) );
			detChan = init_DetChan_type(scanEngine, detectorVChanName);
			CA_FreeMemory(detectorVChanName);
			// add detector channel to scan engine
			ListInsertItem(scanEngine->DetChans, &detChan, END_OF_LIST);
			OKFreeCAHandle(detectorVChanNode);
		}
		
		// assign fast scan axis to scan engine
		// do this before loading the objective and updating optics
		ScanAxisCal_type**	scanAxisPtr;
		for (long j = 1; j <= nAxisCalibrations; j++) {
			scanAxisPtr = ListGetPtrToItem(ls->availableCals, j);
			if (!strcmp(fastAxisCalibrationName, (*scanAxisPtr)->calName)) {
				scanEngine->fastAxisCal = *scanAxisPtr;
				(*scanAxisPtr)->scanEngine = scanEngine;
				break;
			}
		}
		
		// assign slow scan axis to scan engine
		// do this before loading the objective and updating optics 
		for (long j = 1; j <= nAxisCalibrations; j++) {
			scanAxisPtr = ListGetPtrToItem(ls->availableCals, j);
			if (!strcmp(slowAxisCalibrationName, (*scanAxisPtr)->calName)) {
				scanEngine->slowAxisCal = *scanAxisPtr;
				(*scanAxisPtr)->scanEngine = scanEngine;
				break;
			}
		}
		
		// load objectives
		XMLErrChk ( ActiveXML_IXMLDOMElement_getElementsByTagName(objectivesXMLElement, &xmlERRINFO, "Objective", &objectivesNodeList) );
		XMLErrChk ( ActiveXML_IXMLDOMNodeList_Getlength(objectivesNodeList, &xmlERRINFO, &nObjectives) );
		
		for (long j = 0; j < nObjectives; j++) {
			XMLErrChk ( ActiveXML_IXMLDOMNodeList_Getitem(objectivesNodeList, &xmlERRINFO, j, &objectiveNode) );
			errChk( DLGetXMLNodeAttributes(objectiveNode, objectiveAttr, NumElem(objectiveAttr)) );    
			objective = init_Objective_type(objectiveName, objectiveFL);
			// assign objective to scan engine
			if (!strcmp(objectiveName, assignedObjectiveName)) {
				scanEngine->objectiveLens = objective;
				// MAKE SURE THIS IS DONE AFTER READING IS TUBE LENS AND SCAN LENS FL AND SCAN AXIS SETTINGS
				// update both scan axes
				if (scanEngine->fastAxisCal)
					(*scanEngine->fastAxisCal->UpdateOptics) (scanEngine->fastAxisCal);
				
				if (scanEngine->slowAxisCal)
					(*scanEngine->slowAxisCal->UpdateOptics) (scanEngine->slowAxisCal);
				
			}
			OKfree(objectiveName);
			// add objective to scan engine
			ListInsertItem(scanEngine->objectives, &objective, END_OF_LIST);
			OKFreeCAHandle(objectiveNode);
		}
		
		// add scan engine to laser scanning module
		ListInsertItem(ls->scanEngines, &scanEngine, END_OF_LIST);
		
		// cleanup
		OKFreeCAHandle(scanEngineNode);
		OKFreeCAHandle(VChansXMLElement);
		OKFreeCAHandle(scanInfoXMLElement);
		OKFreeCAHandle(detectorVChanNodeList);
		OKFreeCAHandle(objectivesXMLElement);
		OKFreeCAHandle(objectivesNodeList);
		OKfree(scanEngineName);
		OKfree(assignedObjectiveName);
		OKfree(fastAxisCalibrationName);
		OKfree(slowAxisCalibrationName);
		OKfree(fastAxisCommandVChanName);
		OKfree(fastAxisCommandNSampVChanName);
		OKfree(slowAxisCommandVChanName);
		OKfree(slowAxisCommandNSampVChanName); 
		OKfree(fastAxisPositionVChanName);
		OKfree(slowAxisPositionVChanName);
		OKfree(scanEngineOutVChanName);
		OKfree(shutterVChanName);
		OKfree(pixelPulseTrainVChanName);
	}
	
	
XMLError:
	
	error = xmlerror;
	
Error:
	
	OKFreeCAHandle(scanEngineNodeList); 
	OKFreeCAHandle(scanEnginesXMLElement);
	OKFreeCAHandle(scanEngineNode); 
	OKFreeCAHandle(scanAxisCalibrationsXMLElement); 
	OKFreeCAHandle(scanAxisCalXMLElement); 
	OKFreeCAHandle(axisCalibrationNodeList);
	OKFreeCAHandle(VChansXMLElement);

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
	DAQLabXMLNode 					nrgCalAttr[] 		= {	{"CommandVMax", BasicData_Double, &nrgCal->commandVMax},
															{"CommandAsFunctionOfPositionLinFitSlope", BasicData_Double, &nrgCal->slope},
															{"CommandAsFunctionOfPositionLinFitOffset", BasicData_Double, &nrgCal->offset},
															{"PositionStdDev", BasicData_Double, &nrgCal->posStdDev},
															{"ResponseLag", BasicData_Double, &nrgCal->lag},
															{"Resolution", BasicData_Double, &nrgCal->resolution},
															{"MinStepSize", BasicData_Double, &nrgCal->minStepSize},
															{"ParkedCommandV", BasicData_Double, &nrgCal->parked},
															{"MechanicalResponse", BasicData_Double, &nrgCal->mechanicalResponse}};
	
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
	DAQLabXMLNode 			switchTimesAttr[] 		= {	{"NElements", BasicData_UInt, &nrgCal->switchTimes->n},
														{"StepSize", BasicData_CString, switchTimesStepSizeStr},
														{"HalfSwitchTime", BasicData_CString, switchTimesHalfSwitchStr} };
														
	DAQLabXMLNode 			maxSlopesAttr[] 		= {	{"NElements", BasicData_UInt, &nrgCal->maxSlopes->n},
														{"Slope", BasicData_CString, maxSlopesSlopeStr},
														{"Amplitude", BasicData_CString, maxSlopesAmplitudeStr} };
														
	DAQLabXMLNode 			triangleCalAttr[] 		= {	{"NElements", BasicData_UInt, &nrgCal->triangleCal->n},
														{"DeadTime", BasicData_Double, &nrgCal->triangleCal->deadTime},
														{"CommandAmplitude", BasicData_CString, triangleCalCommandAmplitudeStr},
														{"ActualAmplitude", BasicData_CString, triangleCalActualAmplitudeStr},
														{"MaxFrequency", BasicData_CString, triangleCalMaxFreqStr},
														{"ResidualLag", BasicData_CString, triangleCalResiduaLagStr} };
	
	DLAddToXMLElem(xmlDOM, switchTimesXMLElement, switchTimesAttr, DL_ATTRIBUTE, NumElem(switchTimesAttr));
	DLAddToXMLElem(xmlDOM, maxSlopesXMLElement, maxSlopesAttr, DL_ATTRIBUTE, NumElem(maxSlopesAttr));
	DLAddToXMLElem(xmlDOM, triangleCalXMLElement, triangleCalAttr, DL_ATTRIBUTE, NumElem(triangleCalAttr));
								
	// add calibration elements to scan axis calibration element
	XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (axisCalibrationsElement, &xmlERRINFO, switchTimesXMLElement, NULL) );
	OKFreeCAHandle(switchTimesXMLElement); 
	XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (axisCalibrationsElement, &xmlERRINFO, maxSlopesXMLElement, NULL) );
	OKFreeCAHandle(maxSlopesXMLElement);
	XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (axisCalibrationsElement, &xmlERRINFO, triangleCalXMLElement, NULL) );
	OKFreeCAHandle(triangleCalXMLElement); 
	
	OKfree(switchTimesStepSizeStr);
	OKfree(switchTimesHalfSwitchStr);
	OKfree(maxSlopesSlopeStr);
	OKfree(maxSlopesAmplitudeStr);
	OKfree(triangleCalCommandAmplitudeStr);
	OKfree(triangleCalActualAmplitudeStr);
	OKfree(triangleCalMaxFreqStr);
	OKfree(triangleCalResiduaLagStr);
	
	return 0;
	
Error:
	
	OKfree(switchTimesStepSizeStr);
	OKfree(switchTimesHalfSwitchStr);
	OKfree(maxSlopesSlopeStr);
	OKfree(maxSlopesAmplitudeStr);
	OKfree(triangleCalCommandAmplitudeStr);
	OKfree(triangleCalActualAmplitudeStr);
	OKfree(triangleCalMaxFreqStr);
	OKfree(triangleCalResiduaLagStr);
	
	return error;
	
XMLError:   
	
	return xmlerror;
	
}

static int LoadNonResGalvoCalFromXML (LaserScanning_type* lsModule, ActiveXMLObj_IXMLDOMElement_ axisCalibrationElement)
{
	int 							error 							= 0;
	char*							axisCalibrationName				= NULL;
	double							commandVMax;
	double							slope;
	double							offset;
	double							posStdDev;
	double							lag;
	double							resolution;
	double							minStepSize;
	double							parked;
	double							mechanicalResponse;
	ActiveXMLObj_IXMLDOMElement_	switchTimesXMLElement			= 0; 			  
	ActiveXMLObj_IXMLDOMElement_	maxSlopesXMLElement				= 0;   		
	ActiveXMLObj_IXMLDOMElement_	triangleCalXMLElement			= 0;
	DAQLabXMLNode					axisCalibrationAttr[] 			= { {"Name", BasicData_CString, &axisCalibrationName},
											  		   					{"CommandVMax", BasicData_Double, &commandVMax},
																		{"CommandAsFunctionOfPositionLinFitSlope", BasicData_Double, &slope},
																		{"CommandAsFunctionOfPositionLinFitOffset", BasicData_Double, &offset},
																		{"PositionStdDev", BasicData_Double, &posStdDev},
																		{"ResponseLag", BasicData_Double, &lag},
																		{"Resolution", BasicData_Double, &resolution},
																		{"MinStepSize", BasicData_Double, &minStepSize},
																		{"ParkedCommandV", BasicData_Double, &parked},
																		{"MechanicalResponse", BasicData_Double, &mechanicalResponse}};
	
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
	OKFreeCAHandle(switchTimesXMLElement);
	
	SwitchTimes_type*	switchTimes = init_SwitchTimes_type();
	
	switchTimes->n 				= nSwitchTimes;
	switchTimes->halfSwitch		= malloc(nSwitchTimes * sizeof(double));
	switchTimes->stepSize		= malloc(nSwitchTimes * sizeof(double));
	
	Scan(switchTimesStepSizeStr, STRING_TO_CALIBRATION_DATA, switchTimes->n, switchTimes->stepSize);
	Scan(switchTimesHalfSwitchStr, STRING_TO_CALIBRATION_DATA, switchTimes->n, switchTimes->halfSwitch);  
	
	// max slopes
	errChk( DLGetXMLElementAttributes(maxSlopesXMLElement, maxSlopesAttr, NumElem(maxSlopesAttr)) );
	OKFreeCAHandle(maxSlopesXMLElement);
	
	MaxSlopes_type*		maxSlopes = init_MaxSlopes_type();
	
	maxSlopes->n 				= nMaxSlopes;
	maxSlopes->amplitude		= malloc(nMaxSlopes * sizeof(double));
	maxSlopes->slope			= malloc(nMaxSlopes * sizeof(double));
	
	Scan(maxSlopesSlopeStr, STRING_TO_CALIBRATION_DATA, maxSlopes->n , maxSlopes->slope);
	Scan(maxSlopesAmplitudeStr, STRING_TO_CALIBRATION_DATA, maxSlopes->n , maxSlopes->amplitude);  
	
	// triangle waveform calibration
	errChk( DLGetXMLElementAttributes(triangleCalXMLElement, triangleCalAttr, NumElem(triangleCalAttr)) );
	OKFreeCAHandle(triangleCalXMLElement);
	
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
	NonResGalvoCal_type*		nrgCal = init_NonResGalvoCal_type(axisCalibrationName, lsModule, commandVMax, slope, offset,
									     posStdDev, lag, switchTimes, maxSlopes, triangleCal, resolution, minStepSize, parked, mechanicalResponse);  
	
	// add calibration data to module
	ListInsertItem(lsModule->availableCals, &nrgCal, END_OF_LIST);
	
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
	
	if (switchTimesXMLElement) OKFreeCAHandle(switchTimesXMLElement);
	if (maxSlopesXMLElement) OKFreeCAHandle(maxSlopesXMLElement); 
	if (triangleCalXMLElement) OKFreeCAHandle(triangleCalXMLElement);   
	
	return error;	
}

static int DisplayPanels (DAQLabModule_type* mod, BOOL visibleFlag)
{
	LaserScanning_type*			ls				= (LaserScanning_type*) mod;
	int							error			= 0;
	size_t						nEngines		= ListNumItems(ls->scanEngines);
	size_t						nActiveCals		= ListNumItems(ls->activeCal);
	ScanEngine_type**			enginePtr;
	ActiveScanAxisCal_type**	activeCalPtr;
	
	
	// laser scanning module panels
	if (ls->mainPanHndl)
		errChk(SetPanelAttribute(ls->mainPanHndl, ATTR_VISIBLE, visibleFlag));
	
	if (ls->enginesPanHndl)
		errChk(SetPanelAttribute(ls->enginesPanHndl, ATTR_VISIBLE, visibleFlag));
	
	if (ls->manageAxisCalPanHndl)
		errChk(SetPanelAttribute(ls->manageAxisCalPanHndl, ATTR_VISIBLE, visibleFlag));
	
	if (ls->newAxisCalTypePanHndl)
		errChk(SetPanelAttribute(ls->newAxisCalTypePanHndl, ATTR_VISIBLE, visibleFlag));  
		
	
	// scan engine settings panels
	for(size_t i = 1; i <= nEngines; i++) {
		enginePtr = ListGetPtrToItem(ls->scanEngines, i);
		
		if ((*enginePtr)->engineSetPanHndl)
			errChk(SetPanelAttribute((*enginePtr)->engineSetPanHndl, ATTR_VISIBLE, visibleFlag));
	}
	
	// active scan axis calibration panels
	for(size_t i = 1; i <= nActiveCals; i++) {
		activeCalPtr = ListGetPtrToItem(ls->activeCal, i);
		
		if ((*activeCalPtr)->calPanHndl)
			errChk(SetPanelAttribute((*activeCalPtr)->calPanHndl, ATTR_VISIBLE, visibleFlag));
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
	
	// load settings panel if not loaded already, otherwise bring it to focus
	GetPanelAttribute(panelHandle, ATTR_PANEL_PARENT, &workspacePanHndl);
	if (engine->engineSetPanHndl) {
		DisplayPanel(engine->engineSetPanHndl);
		return;
	}
	
	engine->engineSetPanHndl 	= LoadPanel(workspacePanHndl, MOD_LaserScanning_UI, ScanSetPan);
	// change panel title
	char* panTitle = GetTaskControlName(engine->taskControl);
	AppendString(&panTitle, " settings", -1);
	SetPanelAttribute(engine->engineSetPanHndl, ATTR_TITLE, panTitle);
	OKfree(panTitle);
	
	char* VChanFastAxisComName 		= GetVChanName((VChan_type*)engine->VChanFastAxisCom);
	char* VChanSlowAxisComName 		= GetVChanName((VChan_type*)engine->VChanSlowAxisCom);
	char* VChanFastAxisPosName 		= GetVChanName((VChan_type*)engine->VChanFastAxisPos);
	char* VChanSlowAxisPosName 		= GetVChanName((VChan_type*)engine->VChanSlowAxisPos);
	char* VChanScanOutName 			= GetVChanName((VChan_type*)engine->VChanScanOut); 
	char* VChanShutterName			= GetVChanName((VChan_type*)engine->VChanShutter);   
	char* VChanPixelPulseTrainName	= GetVChanName((VChan_type*)engine->VChanPixelPulseTrain);   
	
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
	
	if (engine->VChanShutter)
		SetCtrlVal(engine->engineSetPanHndl, ScanSetPan_ShutterCommand, VChanShutterName);
	
	if (engine->VChanPixelPulseTrain)
		SetCtrlVal(engine->engineSetPanHndl, ScanSetPan_PixelPulseTrain, VChanPixelPulseTrainName); 
		
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
	
	// populate by default NonResonantGalvo type of calibrations
	UpdateScanEngineCalibrations(engine);
	
	// update optics
	// objective
	size_t 				nObjectives 		= ListNumItems(engine->objectives);
	Objective_type**	objectivePtr		= NULL;
	char				objFLString[30];
	char*				extendedObjName		= NULL; 
	for (size_t i = 1; i <= nObjectives; i++) {
		objectivePtr = ListGetPtrToItem(engine->objectives, i);
		extendedObjName = StrDup((*objectivePtr)->objectiveName);
		Fmt(objFLString, "%s< (%f[p3] mm FL)", (*objectivePtr)->objectiveFL);
		AppendString(&extendedObjName, objFLString, -1);
		InsertListItem(engine->engineSetPanHndl, ScanSetPan_Objectives, -1, extendedObjName, 0); 
		OKfree(extendedObjName);
	}
	
	// scan and tube lens FL
	SetCtrlVal(engine->engineSetPanHndl, ScanSetPan_ScanLensFL, engine->scanLensFL);
	SetCtrlVal(engine->engineSetPanHndl, ScanSetPan_TubeLensFL, engine->tubeLensFL);
	
	// update galvo and pixel sampling rates
	SetCtrlVal(engine->engineSetPanHndl, ScanSetPan_GalvoSamplingRate, ((RectRaster_type*)engine)->galvoSamplingRate/1e3); 		// convert from [Hz] to [kHz]
	SetCtrlVal(engine->engineSetPanHndl, ScanSetPan_PixelClockFrequency, engine->pixelClockRate/1e6);								// convert from [Hz] to [MHz] 
	// update pixel delay
	SetCtrlVal(engine->engineSetPanHndl, ScanSetPan_PixelDelay, engine->pixDelay);													// convert from [Hz] to [MHz] 
	
	DisplayPanel(engine->engineSetPanHndl);
}

// adds a new scan engine
static void CVICALLBACK NewScanEngineMenu_CB (int menuBar, int menuItem, void *callbackData, int panel)
{
	LaserScanning_type*		ls 				= callbackData;
	int						workspacePanHndl;
	
	if (ls->enginesPanHndl) {
		DisplayPanel(ls->enginesPanHndl);
		return;
	}
	
	// load panel resources
	GetPanelAttribute(panel, ATTR_PANEL_PARENT, &workspacePanHndl);
	ls->enginesPanHndl = LoadPanel(workspacePanHndl, MOD_LaserScanning_UI, EnginesPan); 
	// add callback and data to controls
	SetCtrlsInPanCBInfo(ls, NewScanEngine_CB, ls->enginesPanHndl);
	
	//------------------------------------------------------------------------------------------------------------------
	// Insert scan engine types here
	//------------------------------------------------------------------------------------------------------------------
	InsertListItem(ls->enginesPanHndl, EnginesPan_ScanTypes, -1, "Rectangular raster scan: Non-resonant galvos", ScanEngine_RectRaster_NonResonantGalvoFastAxis_NonResonantGalvoSlowAxis);
	
	
	//------------------------------------------------------------------------------------------------------------------
	
	DisplayPanel(ls->enginesPanHndl);
}

static int CVICALLBACK NewScanEngine_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	LaserScanning_type*		ls 		= callbackData;
	int						error 	= 0;
	
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
					
					// initialize raster scan engine
					char*	fastAxisComVChanName 		= DLGetUniqueVChanName(VChan_ScanEngine_FastAxis_Command);
					char*	fastAxisComNSampVChanName   = DLGetUniqueVChanName(VChan_ScanEngine_FastAxis_Command_NSamples);
					char*	slowAxisComVChanName 		= DLGetUniqueVChanName(VChan_ScanEngine_SlowAxis_Command);
					char*	slowAxisComNSampVChanName   = DLGetUniqueVChanName(VChan_ScanEngine_SlowAxis_Command_NSamples);
					char*	fastAxisPosVChanName 		= DLGetUniqueVChanName(VChan_ScanEngine_FastAxis_Position);
					char*	slowAxisPosVChanName		= DLGetUniqueVChanName(VChan_ScanEngine_SlowAxis_Position);
					char*	imageOutVChanName			= DLGetUniqueVChanName(VChan_ScanEngine_ImageOut);
					char*	detectionVChanName			= DLGetUniqueVChanName(VChan_ScanEngine_DetectionChan);
					char*	shutterVChanName			= DLGetUniqueVChanName(VChan_ScanEngine_Shutter_Command);
					char*	pixelPulseTrainVChanName	= DLGetUniqueVChanName(VChan_ScanEngine_Pixel_PulseTrain); 
					char*	nPixelsVChanName			= DLGetUniqueVChanName(VChan_ScanEngine_NPixels);
							
					switch (engineType) {
					
						case ScanEngine_RectRaster_NonResonantGalvoFastAxis_NonResonantGalvoSlowAxis:
					
							newScanEngine = (ScanEngine_type*) init_RectRaster_type(ls, engineName, fastAxisComVChanName, fastAxisComNSampVChanName, slowAxisComVChanName, slowAxisComNSampVChanName,
														fastAxisPosVChanName, slowAxisPosVChanName, imageOutVChanName, detectionVChanName, shutterVChanName, pixelPulseTrainVChanName, nPixelsVChanName,
														NonResGalvoRasterScan_Default_GalvoSamplingRate, NonResGalvoRasterScan_Default_PixelClockRate, 0, 1, 0, 1, 0, NonResGalvoRasterScan_Default_PixelSize, NonResGalvoRasterScan_Default_PixelDwellTime, 1, 1);
							
							RectRaster_type*	rectRasterScanEngine = (RectRaster_type*) newScanEngine;   
							
							scanPanHndl = LoadPanel(ls->mainPanHndl, MOD_LaserScanning_UI, RectRaster);
							// populate scan engine modes
							for (size_t i = 0; i < NumElem(scanEngineModes); i++)
								InsertListItem(scanPanHndl, RectRaster_Mode, -1, scanEngineModes[i].label, scanEngineModes[i].mode);
							SetCtrlIndex(scanPanHndl, RectRaster_Mode, 0);
							
							// dim panel since the scan engine is not configured
							SetPanelAttribute(scanPanHndl, ATTR_DIMMED, 1);
							
							// update width
							SetCtrlVal(scanPanHndl, RectRaster_Width, rectRasterScanEngine->scanSettings.width * rectRasterScanEngine->scanSettings.pixSize);
				
							// update height and width offsets
							SetCtrlVal(scanPanHndl, RectRaster_WidthOffset, rectRasterScanEngine->scanSettings.widthOffset * rectRasterScanEngine->scanSettings.pixSize); 
							SetCtrlVal(scanPanHndl, RectRaster_HeightOffset, rectRasterScanEngine->scanSettings.heightOffset * rectRasterScanEngine->scanSettings.pixSize);
				
							// update pixel size
							SetCtrlVal(scanPanHndl, RectRaster_PixelSize, ((RectRaster_type*)newScanEngine)->scanSettings.pixSize);
							SetCtrlAttribute(scanPanHndl, RectRaster_PixelSize, ATTR_MIN_VALUE, NonResGalvoRasterScan_Min_PixelSize);
							SetCtrlAttribute(scanPanHndl, RectRaster_PixelSize, ATTR_MAX_VALUE, NonResGalvoRasterScan_Max_PixelSize);
							SetCtrlAttribute(scanPanHndl, RectRaster_PixelSize, ATTR_CHECK_RANGE, VAL_COERCE);
							
							break;
							// add below more cases	
					}
					
					// cleanup generated VChan names
					OKfree(fastAxisComVChanName);
					OKfree(fastAxisComNSampVChanName);
					OKfree(slowAxisComVChanName);
					OKfree(slowAxisComNSampVChanName);
					OKfree(fastAxisPosVChanName);
					OKfree(slowAxisPosVChanName);
					OKfree(imageOutVChanName);
					OKfree(detectionVChanName);
					OKfree(shutterVChanName);
					OKfree(pixelPulseTrainVChanName);
					//------------------------------------------------------------------------------------------------------------------
					newTabIdx = InsertPanelAsTabPage(ls->mainPanHndl, ScanPan_ScanEngines, -1, scanPanHndl);  
					// discard loaded panel and add scan control panel handle to scan engine
					DiscardPanel(scanPanHndl); 
					scanPanHndl = 0;
					GetPanelHandleFromTabPage(ls->mainPanHndl, ScanPan_ScanEngines, newTabIdx, &newScanEngine->scanSetPanHndl);
			
					// change new scan engine tab title
					SetTabPageAttribute(ls->mainPanHndl, ScanPan_ScanEngines, newTabIdx, ATTR_LABEL_TEXT, engineName);  
			
					// add panel callback data
					SetPanelAttribute(newScanEngine->scanSetPanHndl, ATTR_CALLBACK_DATA, newScanEngine); 
					
					// add callback function and data to scan engine controls
					switch (newScanEngine->engineType) {
				
						case ScanEngine_RectRaster_NonResonantGalvoFastAxis_NonResonantGalvoSlowAxis:
				
							RectRaster_type*	rectRasterScanEngine = (RectRaster_type*) newScanEngine;  
				
							SetCtrlsInPanCBInfo(newScanEngine, NonResRectRasterScan_CB, newScanEngine->scanSetPanHndl);
				
							// make Height string control into combo box
							// do this after calling SetCtrlsInPanCBInfo, otherwise the combobox is disrupted
							errChk( Combo_NewComboBox(newScanEngine->scanSetPanHndl, RectRaster_Height) );
							errChk( Combo_SetComboAttribute(newScanEngine->scanSetPanHndl, RectRaster_Height, ATTR_COMBO_MAX_LENGTH, NonResGalvoRasterScan_Max_ComboboxEntryLength) );
				
							// update height
							char heightString[NonResGalvoRasterScan_Max_ComboboxEntryLength+1];
							Fmt(heightString, "%s<%f[p1]", rectRasterScanEngine->scanSettings.height * rectRasterScanEngine->scanSettings.pixSize);
							if (rectRasterScanEngine->scanSettings.height)
								SetCtrlVal(newScanEngine->scanSetPanHndl, RectRaster_Height, heightString); 
							else
								SetCtrlVal(newScanEngine->scanSetPanHndl, RectRaster_Height, ""); 
				
							// make Pixel Dwell time string control into combo box
							// do this after calling SetCtrlsInPanCBInfo, otherwise the combobox is disrupted  
							errChk( Combo_NewComboBox(newScanEngine->scanSetPanHndl, RectRaster_PixelDwell) );
							errChk( Combo_SetComboAttribute(newScanEngine->scanSetPanHndl, RectRaster_PixelDwell, ATTR_COMBO_MAX_LENGTH, NonResGalvoRasterScan_Max_ComboboxEntryLength) );
				
							// update pixel dwell time
							char pixelDwellString[NonResGalvoRasterScan_Max_ComboboxEntryLength+1];
							Fmt(pixelDwellString, "%s<%f[p1]", rectRasterScanEngine->scanSettings.pixelDwellTime);
							if (rectRasterScanEngine->scanSettings.pixelDwellTime != 0.0)
								SetCtrlVal(newScanEngine->scanSetPanHndl, RectRaster_PixelDwell, pixelDwellString);
							else
								SetCtrlVal(newScanEngine->scanSetPanHndl, RectRaster_PixelDwell, "");
				
							// configure/unconfigure scan engine
							// do this after adding combo boxes!
							if (NonResRectRasterScan_ValidConfig(rectRasterScanEngine)) {
				
								NonResRectRasterScan_ScanHeights(rectRasterScanEngine);
								NonResRectRasterScan_PixelDwellTimes(rectRasterScanEngine);
					
								if (NonResRectRasterScan_ReadyToScan((RectRaster_type*)newScanEngine))
									TaskControlEvent(newScanEngine->taskControl, TASK_EVENT_CONFIGURE, NULL, NULL);
								else 
									TaskControlEvent(newScanEngine->taskControl, TASK_EVENT_UNCONFIGURE, NULL, NULL); 	
							} else 
								TaskControlEvent(newScanEngine->taskControl, TASK_EVENT_UNCONFIGURE, NULL, NULL);
								
							break;
				
							// insert here more scan engine types
					}
					
					// add scan engine task controller to the framework
					DLAddTaskController((DAQLabModule_type*)ls, newScanEngine->taskControl);
					
					// add scan engine VChans to DAQLab framework
					DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)newScanEngine->VChanFastAxisCom);
					DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)newScanEngine->VChanFastAxisComNSamples);
					DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)newScanEngine->VChanSlowAxisCom);
					DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)newScanEngine->VChanSlowAxisComNSamples);
					DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)newScanEngine->VChanFastAxisPos);
					DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)newScanEngine->VChanSlowAxisPos);
					DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)newScanEngine->VChanScanOut);
					DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)newScanEngine->VChanShutter);     
					DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)newScanEngine->VChanPixelPulseTrain); 
					DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)newScanEngine->VChanNPixels); 
					DetChan_type**	detPtr = ListGetPtrToItem(newScanEngine->DetChans, 1);
					DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)(*detPtr)->detVChan);
					
					// register Sink VChans with the task controller
					AddSinkVChan(newScanEngine->taskControl, (*detPtr)->detVChan, NULL);
					AddSinkVChan(newScanEngine->taskControl, newScanEngine->VChanFastAxisPos, NULL);
					AddSinkVChan(newScanEngine->taskControl, newScanEngine->VChanSlowAxisPos, NULL);
					
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
	
Error:
	
	DLMsg("Error: Could not load new scan engine.\n\n", 1);
	
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
	ScanAxis_type 			fastAxisType;
	ScanAxis_type			slowAxisType;
					
	
	if (!scanEngine->engineSetPanHndl) return; // do nothing if there is no pannel
	
	// determine scan axis types based on scan engine type
	GetScanAxisTypes(scanEngine, &fastAxisType, &slowAxisType);
	
	// empty lists, insert empty selection and select by default
	DeleteListItem(scanEngine->engineSetPanHndl, ScanSetPan_FastAxisCal, 0, -1);
	DeleteListItem(scanEngine->engineSetPanHndl, ScanSetPan_SlowAxisCal, 0, -1);
	InsertListItem(scanEngine->engineSetPanHndl, ScanSetPan_FastAxisCal, -1,"", 0);  
	InsertListItem(scanEngine->engineSetPanHndl, ScanSetPan_SlowAxisCal, -1,"", 0); 
	SetCtrlIndex(scanEngine->engineSetPanHndl, ScanSetPan_FastAxisCal, 0);
	SetCtrlIndex(scanEngine->engineSetPanHndl, ScanSetPan_SlowAxisCal, 0);
	
	int nListItems;
	for (size_t i = 1; i <= nCal; i++) {
		calPtr = ListGetPtrToItem(scanEngine->lsModule->availableCals, i);
		// fast axis calibration list
		if (((*calPtr)->scanAxisType == fastAxisType) && (*calPtr != scanEngine->slowAxisCal)) {
			InsertListItem(scanEngine->engineSetPanHndl, ScanSetPan_FastAxisCal, -1, (*calPtr)->calName, i);
			// select fast axis list item if calibration is assigned to the scan engine
			if (scanEngine->fastAxisCal == *calPtr) {
				GetNumListItems(scanEngine->engineSetPanHndl, ScanSetPan_FastAxisCal, &nListItems);
				SetCtrlIndex(scanEngine->engineSetPanHndl, ScanSetPan_FastAxisCal, nListItems - 1);
			}
		}
		
		// slow axis calibration list
		if (((*calPtr)->scanAxisType == slowAxisType) && (*calPtr != scanEngine->fastAxisCal)) {
			InsertListItem(scanEngine->engineSetPanHndl, ScanSetPan_SlowAxisCal, -1, (*calPtr)->calName, i);
			// select fast axis list item if calibration is assigned to the scan engine
			if (scanEngine->slowAxisCal == *calPtr) {
				GetNumListItems(scanEngine->engineSetPanHndl, ScanSetPan_SlowAxisCal, &nListItems);
				SetCtrlIndex(scanEngine->engineSetPanHndl, ScanSetPan_SlowAxisCal, nListItems - 1);
			}
		}
		
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
	LaserScanning_type*		ls 			= callbackData;
	
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
			
			ScanAxisCal_type**		axisCalPtr	= NULL;
			int						itemIdx;
			
			GetCtrlIndex(ls->manageAxisCalPanHndl, ManageAxis_AxisCalibList, &itemIdx);
			if (itemIdx < 0) return 0; // no items to delete
			axisCalPtr = ListGetPtrToItem(ls->availableCals, itemIdx+1);
			
			// remove from scan engines
			size_t 				nEngines = ListNumItems(ls->scanEngines);
			ScanEngine_type*   	scanEngine;
			for (size_t i = 1; i <= nEngines; i++) {
				scanEngine = *(ScanEngine_type**)ListGetPtrToItem(ls->scanEngines, i);
				
				// remove fast scan axis calibration if assigned
				if (scanEngine->fastAxisCal == *axisCalPtr) {
					scanEngine->fastAxisCal = NULL;
					// unconfigure scan engine
					TaskControlEvent(scanEngine->taskControl, TASK_EVENT_UNCONFIGURE, NULL, NULL);
				}
				// remove slow scan axis calibration if assigned
				if (scanEngine->slowAxisCal == *axisCalPtr) {
					scanEngine->slowAxisCal = NULL;
					// unconfigure scan engine
					TaskControlEvent(scanEngine->taskControl, TASK_EVENT_UNCONFIGURE, NULL, NULL);
				}
				
				UpdateScanEngineCalibrations(scanEngine); 
			}
			
			// discard calibration data
			discard_ScanAxisCal_type(axisCalPtr);
			
			// delete from calibration list and UI
			DeleteListItem(ls->manageAxisCalPanHndl, ManageAxis_AxisCalibList, itemIdx, 1);
			ListRemoveItem(ls->availableCals, 0, itemIdx+1);
			
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
							AppendString(&commandVChanName,": ", -1); 
							AppendString(&commandVChanName, VChan_AxisCal_Command, -1);
							
							// get unique name for command signal nSamples VChan
							char* commandNSamplesVChanName = StrDup(calTCName);
							AppendString(&commandNSamplesVChanName,": ", -1);
							AppendString(&commandNSamplesVChanName, VChan_AxisCal_NSamples, -1);
							
							// get unique name for position feedback signal VChan 
							char* positionVChanName = StrDup(calTCName);
							AppendString(&positionVChanName, ": ", -1);
							AppendString(&positionVChanName, VChan_AxisCal_Position, -1);
							
							// init structure for galvo calibration
							ActiveNonResGalvoCal_type* 	nrgCal = init_ActiveNonResGalvoCal_type(ls, calTCName, commandVChanName, commandNSamplesVChanName, positionVChanName);
							
							//----------------------------------------
							// Task Controller and VChans registration
							//----------------------------------------
							// register calibration Task Controller with the framework
							DLAddTaskController((DAQLabModule_type*)ls, nrgCal->baseClass.taskController);
							// configure Task Controller
							TaskControlEvent(nrgCal->baseClass.taskController, TASK_EVENT_CONFIGURE, NULL, NULL);
							// register VChans with framework
							DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)nrgCal->baseClass.VChanCom);
							DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)nrgCal->baseClass.VChanComNSamples);
							DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)nrgCal->baseClass.VChanPos);
							// register sink VChan with task controller
							AddSinkVChan(nrgCal->baseClass.taskController, nrgCal->baseClass.VChanPos, NULL); 
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
							OKfree(calTCName);
							OKfree(commandVChanName);
							OKfree(commandNSamplesVChanName);
							OKfree(positionVChanName);
							
							
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
static int FindSlope(double* signal, size_t nsamples, double samplingRate, double signalStdDev, size_t nRep, double sloperelerr, double* maxslope)
{
	double 		tmpslope		= 0;
	double 		relslope_err	= 0;
	BOOL 		foundmaxslope 	= 0;
	size_t		nDiffSamples	= 0;
	double		k				= 0;
	double		l				= signalStdDev/sqrt(nRep) * 1.414;
			
	for (size_t nslope_elem = 1; nslope_elem < nsamples; nslope_elem++){  
			
		*maxslope 		= 0;   // in [V/ms]
		nDiffSamples 	= nsamples - nslope_elem;
		k				= 1/(nslope_elem*1000/samplingRate);
		for (size_t i = 0; i < nDiffSamples; i++) {
			tmpslope = (signal[nslope_elem+i] - signal[i])*k; // slope in [V/ms]
			if (tmpslope > *maxslope) *maxslope = tmpslope;
		}
		// calculate relative maximum slope error
		relslope_err = fabs(l*k/(*maxslope));
		
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
	
	// init
	DLDataTypes allowedPacketTypes[] 	= Allowed_Detector_Data_Types;
	det->displayHndl						= NULL;
	det->detVChan						= NULL;
	
	if ( !(det->detVChan 		= init_SinkVChan_type(VChanName, allowedPacketTypes, NumElem(allowedPacketTypes), det, VChanDataTimeout, DetVChan_Connected, DetVChan_Disconnected)) ) goto Error;
	det->scanEngine 			= scanEngine;
	
	return det;
	
Error:
	
	// cleanup
	discard_VChan_type((VChan_type**)&det->detVChan);
	
	free(det);
	return NULL;
}

static void	discard_DetChan_type (DetChan_type** detChanPtr)
{
	if (!*detChanPtr) return;
	
	discard_VChan_type((VChan_type**)&(*detChanPtr)->detVChan);
	
	OKfree(*detChanPtr);
}

static int CVICALLBACK ScanEngineSettings_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ScanEngine_type*		engine		= callbackData;
	ScanAxisCal_type**		scanAxisPtr;
	char*					newName		= NULL;
	int						listItemIdx; 
	int						workspacePanHndl;
	
	switch (event) {
			
		case EVENT_COMMIT:
			
			switch (control) {
				
				case ScanSetPan_FastAxisCommand:
					
					newName = GetStringFromControl (panel, control);
					DLRenameVChan((VChan_type*)engine->VChanFastAxisCom, newName);
					break;
					
				case ScanSetPan_SlowAxisCommand:
					
					newName = GetStringFromControl (panel, control);
					DLRenameVChan((VChan_type*)engine->VChanSlowAxisCom, newName);
					break;
					
				case ScanSetPan_FastAxisPosition:
					
					newName = GetStringFromControl (panel, control);
					DLRenameVChan((VChan_type*)engine->VChanFastAxisPos, newName); 
					break;
					
				case ScanSetPan_SlowAxisPosition:
					
					newName = GetStringFromControl (panel, control);
					DLRenameVChan((VChan_type*)engine->VChanSlowAxisPos, newName); 
					break;
					
				case ScanSetPan_ImageOut:
					
					newName = GetStringFromControl (panel, control);
					DLRenameVChan((VChan_type*)engine->VChanScanOut, newName);
					break;
					
				case ScanSetPan_ShutterCommand:
					
					newName = GetStringFromControl (panel, control);
					DLRenameVChan((VChan_type*)engine->VChanShutter, newName); 
					break;
					
				case ScanSetPan_PixelPulseTrain:
					
					newName = GetStringFromControl (panel, control);
					DLRenameVChan((VChan_type*)engine->VChanPixelPulseTrain, newName);  
					break;
					
				case ScanSetPan_AddImgChan:
					
					// create new VChan with a predefined base name
					newName = DLGetUniqueVChanName(VChan_ScanEngine_DetectionChan);
					DetChan_type* detChan = init_DetChan_type(engine, newName);
					// insert new VChan to list control, engine list and register with framework and task controller
					InsertListItem(panel, ScanSetPan_ImgChans, -1, newName, newName);
					ListInsertItem(engine->DetChans, &detChan, END_OF_LIST);
					DLRegisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)detChan->detVChan);
					AddSinkVChan(engine->taskControl, detChan->detVChan, NULL);
					break;
					
				case ScanSetPan_AddObjective:
					
					if (engine->newObjectivePanHndl) {
						DisplayPanel(engine->newObjectivePanHndl);
						return 0;   // just highlight panel if already loaded
					}
					
					GetPanelAttribute(engine->engineSetPanHndl, ATTR_PANEL_PARENT, &workspacePanHndl);
					engine->newObjectivePanHndl = LoadPanel(workspacePanHndl, MOD_LaserScanning_UI, NewObjPan);
					SetCtrlsInPanCBInfo(engine, NewObjective_CB, engine->newObjectivePanHndl); 
					DisplayPanel(engine->newObjectivePanHndl);  
					break;
					
				case ScanSetPan_ScanLensFL:
					
					GetCtrlVal(panel, control, &engine->scanLensFL);
					// update scan axes if any
					if (engine->fastAxisCal)
						(*engine->fastAxisCal->UpdateOptics) (engine->fastAxisCal);
					
					if (engine->slowAxisCal)
						(*engine->slowAxisCal->UpdateOptics) (engine->slowAxisCal);
					
					// configure/unconfigure scan engine
					if (NonResRectRasterScan_ValidConfig((RectRaster_type*)engine)) {
				
						NonResRectRasterScan_ScanHeights((RectRaster_type*)engine);
						NonResRectRasterScan_PixelDwellTimes((RectRaster_type*)engine);
					
						if (NonResRectRasterScan_ReadyToScan((RectRaster_type*)engine))
							TaskControlEvent(engine->taskControl, TASK_EVENT_CONFIGURE, NULL, NULL);
						else 
							TaskControlEvent(engine->taskControl, TASK_EVENT_UNCONFIGURE, NULL, NULL); 	
					} else 
						TaskControlEvent(engine->taskControl, TASK_EVENT_UNCONFIGURE, NULL, NULL);
						
					break;
					
				case ScanSetPan_TubeLensFL:
					
					GetCtrlVal(panel, control, &engine->tubeLensFL);
					// update scan axes if any
					if (engine->fastAxisCal)
						(*engine->fastAxisCal->UpdateOptics) (engine->fastAxisCal);
					
					if (engine->slowAxisCal)
						(*engine->slowAxisCal->UpdateOptics) (engine->slowAxisCal);
					
					// configure/unconfigure scan engine
					if (NonResRectRasterScan_ValidConfig((RectRaster_type*)engine)) {
				
						NonResRectRasterScan_ScanHeights((RectRaster_type*)engine);
						NonResRectRasterScan_PixelDwellTimes((RectRaster_type*)engine);
					
						if (NonResRectRasterScan_ReadyToScan((RectRaster_type*)engine))
							TaskControlEvent(engine->taskControl, TASK_EVENT_CONFIGURE, NULL, NULL);
						else 
							TaskControlEvent(engine->taskControl, TASK_EVENT_UNCONFIGURE, NULL, NULL); 	
					} else 
						TaskControlEvent(engine->taskControl, TASK_EVENT_UNCONFIGURE, NULL, NULL);
					
					break;
					
				case ScanSetPan_Close:
					
					DiscardPanel(engine->engineSetPanHndl);
					engine->engineSetPanHndl = 0;
					break;
					
				case ScanSetPan_FastAxisCal:
					
					unsigned int	fastAxisCalIdx;
					GetCtrlVal(panel, control, &fastAxisCalIdx);
					if (fastAxisCalIdx) {
						scanAxisPtr = ListGetPtrToItem(engine->lsModule->availableCals, fastAxisCalIdx);
						engine->fastAxisCal = *scanAxisPtr;
						(*scanAxisPtr)->scanEngine = engine;
						// update both scan axes
						if (engine->fastAxisCal)
							(*engine->fastAxisCal->UpdateOptics) (engine->fastAxisCal);
				
						if (engine->slowAxisCal)
							(*engine->slowAxisCal->UpdateOptics) (engine->slowAxisCal);
					} else {
						if (engine->fastAxisCal)
							engine->fastAxisCal->scanEngine = NULL;
						engine->fastAxisCal = NULL;
					}
					
					// update scan engine calibrations UI
					UpdateScanEngineCalibrations(engine);
					
					// configure/unconfigure scan engine
					if (NonResRectRasterScan_ValidConfig((RectRaster_type*)engine)) {
				
						NonResRectRasterScan_ScanHeights((RectRaster_type*)engine);
						NonResRectRasterScan_PixelDwellTimes((RectRaster_type*)engine);
					
						if (NonResRectRasterScan_ReadyToScan((RectRaster_type*)engine))
							TaskControlEvent(engine->taskControl, TASK_EVENT_CONFIGURE, NULL, NULL);
						else 
							TaskControlEvent(engine->taskControl, TASK_EVENT_UNCONFIGURE, NULL, NULL); 	
						
					} else 
						TaskControlEvent(engine->taskControl, TASK_EVENT_UNCONFIGURE, NULL, NULL);
						
					break;
					
				case ScanSetPan_SlowAxisCal:
					
					unsigned int	slowAxisCalIdx;
					GetCtrlVal(panel, control, &slowAxisCalIdx);
					if (slowAxisCalIdx) {
						scanAxisPtr = ListGetPtrToItem(engine->lsModule->availableCals, slowAxisCalIdx);
						engine->slowAxisCal = *scanAxisPtr;
						(*scanAxisPtr)->scanEngine = engine;
						// update both scan axes
						if (engine->fastAxisCal)
							(*engine->fastAxisCal->UpdateOptics) (engine->fastAxisCal);
				
						if (engine->slowAxisCal)
							(*engine->slowAxisCal->UpdateOptics) (engine->slowAxisCal);
					}
					else {
						if (engine->slowAxisCal)
							engine->slowAxisCal->scanEngine = NULL;
						engine->slowAxisCal = NULL;
					}
					
					// update scan engine calibrations UI
					UpdateScanEngineCalibrations(engine);
					
					// configure/unconfigure scan engine
					if (NonResRectRasterScan_ValidConfig((RectRaster_type*)engine)) {
				
						NonResRectRasterScan_ScanHeights((RectRaster_type*)engine);
						NonResRectRasterScan_PixelDwellTimes((RectRaster_type*)engine);
					
						if (NonResRectRasterScan_ReadyToScan((RectRaster_type*)engine)) 
							TaskControlEvent(engine->taskControl, TASK_EVENT_CONFIGURE, NULL, NULL);
						else 
							TaskControlEvent(engine->taskControl, TASK_EVENT_UNCONFIGURE, NULL, NULL); 	
							
					} else
						TaskControlEvent(engine->taskControl, TASK_EVENT_UNCONFIGURE, NULL, NULL);
						
					break;
					
				case ScanSetPan_PixelClockFrequency:
					
					GetCtrlVal(panel, control, &engine->pixelClockRate);								// read in [MHz]
					engine->pixelClockRate *= 1e6;														// convert to [Hz]
					// configure/unconfigure scan engine
					if (NonResRectRasterScan_ValidConfig((RectRaster_type*)engine)) {
				
						NonResRectRasterScan_ScanHeights((RectRaster_type*)engine);
						NonResRectRasterScan_PixelDwellTimes((RectRaster_type*)engine);
					
						if (NonResRectRasterScan_ReadyToScan((RectRaster_type*)engine))
							TaskControlEvent(engine->taskControl, TASK_EVENT_CONFIGURE, NULL, NULL);
						else
							TaskControlEvent(engine->taskControl, TASK_EVENT_UNCONFIGURE, NULL, NULL); 	
					} else
						TaskControlEvent(engine->taskControl, TASK_EVENT_UNCONFIGURE, NULL, NULL);
					break;
					
				case ScanSetPan_PixelDelay:
					
					GetCtrlVal(panel, control, &engine->pixDelay);
					
					break;
					
				case ScanSetPan_GalvoSamplingRate:
					
					GetCtrlVal(panel, control, &((RectRaster_type*) engine)->galvoSamplingRate);	// read in [kHz]
					((RectRaster_type*) engine)->galvoSamplingRate *= 1e3;							// convert to [Hz]
					// configure/unconfigure scan engine
					if (NonResRectRasterScan_ValidConfig((RectRaster_type*)engine)) {
				
						NonResRectRasterScan_ScanHeights((RectRaster_type*)engine);
						NonResRectRasterScan_PixelDwellTimes((RectRaster_type*)engine);
					
						if (NonResRectRasterScan_ReadyToScan((RectRaster_type*)engine))
							TaskControlEvent(engine->taskControl, TASK_EVENT_CONFIGURE, NULL, NULL);
						else
							TaskControlEvent(engine->taskControl, TASK_EVENT_UNCONFIGURE, NULL, NULL); 	
					} else
						TaskControlEvent(engine->taskControl, TASK_EVENT_UNCONFIGURE, NULL, NULL);
					break;
					
			}
			break;
			
		case EVENT_KEYPRESS:
					
			// continue only if Del key is pressed
			if (eventData1 != VAL_FWD_DELETE_VKEY) break;
			
			switch (control) {
				
				// discard detection channel
				case ScanSetPan_ImgChans:
					
					GetCtrlIndex(panel, control, &listItemIdx);
					if (listItemIdx < 0) break; // stop here if list is empty
					
					DetChan_type** 	detChanPtr; 
					detChanPtr = ListGetPtrToItem(engine->DetChans, listItemIdx+1);
					DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)(*detChanPtr)->detVChan);
					RemoveSinkVChan(engine->taskControl, (*detChanPtr)->detVChan);
					discard_VChan_type((VChan_type**)&(*detChanPtr)->detVChan);
					DeleteListItem(panel, ScanSetPan_ImgChans, listItemIdx, 1);
					
					// configure / unconfigure
					if (NonResRectRasterScan_ReadyToScan((RectRaster_type*)engine))
						TaskControlEvent(engine->taskControl, TASK_EVENT_CONFIGURE, NULL, NULL);
					else
						TaskControlEvent(engine->taskControl, TASK_EVENT_UNCONFIGURE, NULL, NULL); 
						
					break;
				
				// discard objective
				case ScanSetPan_Objectives:
					
					GetCtrlIndex(panel, control, &listItemIdx);
					if (listItemIdx < 0) break; // stop here if list is empty
					
					Objective_type** objectivePtr;
					objectivePtr = ListGetPtrToItem(engine->objectives, listItemIdx+1);
					// remove objective from scan engine structure
					discard_Objective_type(objectivePtr);
					ListRemoveItem(engine->objectives, 0, listItemIdx+1);
					// remove objective from scan engine settings UI 
					DeleteListItem(panel, control, listItemIdx, 1);  
					// remove objective from scan control UI
					DeleteListItem(engine->scanSetPanHndl, RectRaster_Objective, listItemIdx, 1);
					
					// update chosen objective in scan engine structure
					GetCtrlIndex(engine->scanSetPanHndl, RectRaster_Objective, &listItemIdx);
					if (listItemIdx >= 0) {
						objectivePtr = ListGetPtrToItem(engine->objectives, listItemIdx+1); 
						engine->objectiveLens = *objectivePtr;
					} else
						engine->objectiveLens = NULL;
					
					// configure / unconfigure
					if (NonResRectRasterScan_ReadyToScan((RectRaster_type*)engine))
						TaskControlEvent(engine->taskControl, TASK_EVENT_CONFIGURE, NULL, NULL);
					else
						TaskControlEvent(engine->taskControl, TASK_EVENT_UNCONFIGURE, NULL, NULL);
					
					break;
			}
			
			break;
			
		case EVENT_LEFT_DOUBLE_CLICK:
			
			switch (control) {
					
				case ScanSetPan_ImgChans:
					
					newName = DLGetUINameInput("Rename VChan", DAQLAB_MAX_VCHAN_NAME, DLValidateVChanName, NULL);
					if (!newName) return 0; // user cancelled, do nothing
			
					DetChan_type** 	detChanPtr; 
					GetCtrlIndex(panel, control, &listItemIdx); 
					detChanPtr = ListGetPtrToItem(engine->DetChans, listItemIdx+1);
					DLRenameVChan((VChan_type*)(*detChanPtr)->detVChan, newName);
					ReplaceListItem(panel, control, listItemIdx, newName, newName);  
					break;
			}
			
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
					
					NonResGalvoCal_type*	nrgCal = init_NonResGalvoCal_type(calName, activeNRGCal->baseClass.lsModule, activeNRGCal->commandVMax, *activeNRGCal->slope, *activeNRGCal->offset, *activeNRGCal->posStdDev,
													 *activeNRGCal->lag, copy_SwitchTimes_type(activeNRGCal->switchTimes), copy_MaxSlopes_type(activeNRGCal->maxSlopes),
													 copy_TriangleCal_type(activeNRGCal->triangleCal), activeNRGCal->resolution, activeNRGCal->minStepSize, activeNRGCal->parked, activeNRGCal->mechanicalResponse);
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
					DLUnregisterVChan((DAQLabModule_type*)activeNRGCal->baseClass.lsModule, (VChan_type*)activeNRGCal->baseClass.VChanComNSamples);
					
					// unregister calibration Task Controller
					DLRemoveTaskController((DAQLabModule_type*)activeNRGCal->baseClass.lsModule, calTC);
					
					// remove calibration data structure from module list of active calibrations and also discard calibration data
					size_t							nActiveCal = ListNumItems(activeNRGCal->baseClass.lsModule->activeCal);
					ActiveScanAxisCal_type**		activeCalPtr;
					
					for (size_t i = 1; i <= nActiveCal; i++) {
						activeCalPtr = ListGetPtrToItem(activeNRGCal->baseClass.lsModule->activeCal, i);
						if (*activeCalPtr == (ActiveScanAxisCal_type*)activeNRGCal) {
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
					
				case Cal_CommMaxV:
					
					GetCtrlVal(panel, control, &nrgCal->commandVMax);   // read in [V]
					break;
					
				case Cal_MechanicalResponse:
					
					GetCtrlVal(panel, control, &nrgCal->mechanicalResponse);
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
	//ActiveNonResGalvoCal_type*	nrgCal = callbackData;
	
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

static void MaxTriangleWaveformScan (NonResGalvoCal_type* cal, double targetAmplitude, double* maxTriangleWaveFrequency)
{
	if (!cal->triangleCal->n) {
		*maxTriangleWaveFrequency = 0;
		return;
	}
	
	double* secondDerivative = malloc(cal->triangleCal->n * sizeof(double));
	
	// interpolate frequency vs. amplitude measurements
	Spline(cal->triangleCal->actualAmp, cal->triangleCal->maxFreq, cal->triangleCal->n, 0, 0, secondDerivative);
	SpInterp(cal->triangleCal->actualAmp, cal->triangleCal->maxFreq, secondDerivative, cal->triangleCal->n, targetAmplitude, maxTriangleWaveFrequency);
	
	OKfree(secondDerivative);
}

/*
it is not clear how useful this measure is
static double TriangleWaveformLag (NonResGalvoCal_type* cal, double triangleWaveFrequency)
{
	if (!cal->triangleCal->n) {
		return 0;
	}
	
	double* 	secondDerivative = malloc(cal->triangleCal->n * sizeof(double));
	double		residualLag;
	
	// interpolate frequency vs. amplitude measurements
	Spline(cal->triangleCal->maxFreq, cal->triangleCal->resLag, cal->triangleCal->n, 0, 0, secondDerivative);
	SpInterp(cal->triangleCal->maxFreq, cal->triangleCal->resLag, secondDerivative, cal->triangleCal->n, triangleWaveFrequency, &residualLag);
	
	OKfree(secondDerivative);
	return residualLag + cal->lag;
}
*/

/* 
Generates a ramp waveform with maximum slope obtained from galvo calibration for given voltage jump starting from startVoltage and ending with endVoltage.
Before and at the end of the ramp, also a certain number of samples can be added.

startV ___
		   \
		     \
		       \
			     \
			       \ _____ endV 
				     
*/
// sampleRate in [Hz], startVoltage and endVoltage in [V], startDelay and endDelay in [s]
static Waveform_type* NonResGalvoMoveBetweenPoints (NonResGalvoCal_type* cal, double sampleRate, double startVoltage, double startDelay, double endVoltage, double endDelay)
{
	double 			maxSlope;			// in [V/ms]
	double 			amplitude			= fabs(startVoltage - endVoltage);;
	double 			min;
	double 			max;
	int    			minIdx;
	int    			maxIdx;
	size_t 			nElemRamp;
	size_t			nElemStartDelay;
	size_t			nElemEndDelay;
	double*			waveformData		= NULL;
	double* 		secondDerivatives	= NULL;
	Waveform_type* 	waveform			= NULL; 
	
	// check if jump amplitude is within calibration range   
	MaxMin1D(cal->maxSlopes->amplitude, cal->maxSlopes->n, &max, &maxIdx, &min, &minIdx);
	if ((amplitude < min) || (amplitude > max)) 
		return NULL;
	
	secondDerivatives = malloc(cal->maxSlopes->n * sizeof(double));
	if (!secondDerivatives) goto Error;
	
	// interpolate frequency vs. amplitude measurements
	Spline(cal->maxSlopes->amplitude, cal->maxSlopes->slope, cal->maxSlopes->n, 0, 0, secondDerivatives);
	SpInterp(cal->maxSlopes->amplitude, cal->maxSlopes->slope, secondDerivatives,  cal->maxSlopes->n, amplitude, &maxSlope);
	OKfree(secondDerivatives);
	
	nElemRamp 			= (size_t) floor(sampleRate * amplitude * 1e-3/maxSlope);
	if (nElemRamp < 2) nElemRamp = 2;
	nElemStartDelay		= (size_t) floor(sampleRate * startDelay);
	nElemEndDelay		= (size_t) floor(sampleRate * endDelay);
	
	waveformData = malloc ((nElemStartDelay + nElemRamp + nElemEndDelay) * sizeof(double));
	if (!waveformData) goto Error; 
	
	if (nElemStartDelay)
		Set1D(waveformData, nElemStartDelay, startVoltage);
	Ramp(nElemRamp, startVoltage, endVoltage, waveformData + nElemStartDelay);
	if (nElemEndDelay)
		Set1D(waveformData+nElemStartDelay+nElemRamp, nElemEndDelay, endVoltage);
	
	waveform = init_Waveform_type(Waveform_Double, sampleRate, nElemStartDelay + nElemRamp + nElemEndDelay, (void**)&waveformData); 
    if (!waveform) goto Error;
	
	// cleanup
	OKfree(secondDerivatives);
	
	return waveform;
	
Error:
	
	OKfree(waveformData);
	OKfree(secondDerivatives);
	discard_Waveform_type(&waveform);
	
	return NULL;
}

//returns -1 on error,0 if scan speed out of range, 1 if desiredspeed is within range
// scanSize in [um], pixelDwellTime in [us], pixelSize in [um]
int CheckNonResGalvoScanFreq (NonResGalvoCal_type* cal, double pixelSize, double pixelDwellTime, double scanSize)
{
	int    		frequencyOK          = 0;
	double 		targetAmplitude;
	double 		maxTriangleWaveformFrequency;
	double 		desiredFrequency;
	double 		desiredSpeed;
	
	// check input values
	if (pixelDwellTime == 0.0 || !cal->triangleCal->n)
		return -1;
	
	// calculate the requested speed (slope)
	desiredSpeed = pixelSize/pixelDwellTime;   //in um/us, or m/s

	//convert um to Volts
	targetAmplitude = scanSize/cal->sampleDisplacement;  //convert um to voltages   
	//get the max speed for the desired scansize 
	
	MaxTriangleWaveformScan(cal, targetAmplitude, &maxTriangleWaveformFrequency);
	// Note: Lines per second is twice maxtrianglefreq
	desiredFrequency = desiredSpeed/(2*scanSize*1e-6);   //in um/us  is m/s
	
	if (desiredFrequency > maxTriangleWaveformFrequency) 
		frequencyOK=0;  //out of range
	else 
		frequencyOK=1;
	
	
	return frequencyOK;
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
	cal->comSampRate	= &cal->samplingRate;
	cal->samplingRate	= Default_ScanAxisCal_SampleRate;
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
	discard_VChan_type((VChan_type**)&(*cal)->VChanComNSamples);
	discard_VChan_type((VChan_type**)&(*cal)->VChanPos);
	if ((*cal)->calPanHndl) {DiscardPanel((*cal)->calPanHndl); (*cal)->calPanHndl =0;}
	
	OKfree(*cal);
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
	
	// if panel is already loaded, bring it to focus
	if (ls->manageAxisCalPanHndl) {
		DisplayPanel(ls->manageAxisCalPanHndl);
		return;
	}
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

static ActiveNonResGalvoCal_type* init_ActiveNonResGalvoCal_type (LaserScanning_type* lsModule, char calName[], char commandVChanName[], char commandNSamplesVChanName[], char positionVChanName[])
{
	ActiveNonResGalvoCal_type*	cal = malloc(sizeof(ActiveNonResGalvoCal_type));
	if (!cal) return NULL;
	
	DLDataTypes allowedPacketTypes[] = {DL_Waveform_Double};
	
	// init parent class
	initalloc_ActiveScanAxisCal_type(&cal->baseClass);
	if(!(cal->baseClass.calName		= StrDup(calName))) {free(cal); return NULL;}
	cal->baseClass.VChanCom			= init_SourceVChan_type(commandVChanName, DL_Waveform_Double, cal, NonResGalvoCal_ComVChan_Connected, NonResGalvoCal_ComVChan_Disconnected);   
	cal->baseClass.VChanComNSamples	= init_SourceVChan_type(commandNSamplesVChanName, DL_ULongLong, cal, NonResGalvoCal_ComNSamplesVChan_Connected, NonResGalvoCal_ComNSamplesVChan_Disconnected);   
	cal->baseClass.VChanPos			= init_SinkVChan_type(positionVChanName, allowedPacketTypes, NumElem(allowedPacketTypes), cal, VChanDataTimeout + Default_ActiveNonResGalvoCal_ScanTime * 1e3, NonResGalvoCal_PosVChan_Connected, NonResGalvoCal_PosVChan_Disconnected);  
	cal->baseClass.scanAxisType  	= NonResonantGalvo;
	cal->baseClass.Discard			= discard_ActiveNonResGalvoCal_type; // override
	cal->baseClass.taskController	= init_TaskControl_type(calName, cal, DLGetCommonThreadPoolHndl(), ConfigureTC_NonResGalvoCal, UncofigureTC_NonResGalvoCal, IterateTC_NonResGalvoCal, AbortIterationTC_NonResGalvoCal, StartTC_NonResGalvoCal, ResetTC_NonResGalvoCal, 
								  DoneTC_NonResGalvoCal, StoppedTC_NonResGalvoCal, TaskTreeStatus_NonResGalvoCal, NULL, NULL, NULL);
	cal->baseClass.lsModule			= lsModule;
	
								  
	// init ActiveNonResGalvoCal_type
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
	cal->positionWaveform	= NULL;
	cal->commandWaveform	= NULL;
	cal->resolution  		= 0;
	cal->minStepSize 		= 0;
	cal->scanTime    		= Default_ActiveNonResGalvoCal_ScanTime;	// in [s]
	cal->parked				= 0;
	cal->mechanicalResponse	= 1;
	
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
	
	// discard waveforms
	discard_Waveform_type(&NRGCal->commandWaveform);
	discard_Waveform_type(&NRGCal->positionWaveform);
	
	discard_SwitchTimes_type(&NRGCal->switchTimes);
	discard_MaxSlopes_type(&NRGCal->maxSlopes);
	discard_TriangleCal_type(&NRGCal->triangleCal);
	
	// discard ActiveScanAxisCal_type base class data
	discard_ActiveScanAxisCal_type(cal);
}

static NonResGalvoCal_type* init_NonResGalvoCal_type (char calName[], LaserScanning_type* lsModule, double commandVMax, double slope, double offset, double posStdDev, 
													  double lag, SwitchTimes_type* switchTimes, MaxSlopes_type* maxSlopes,
													  TriangleCal_type* triangleCal, double resolution, double minStepSize, double parked, double mechanicalResponse)
{
	NonResGalvoCal_type*	cal = malloc (sizeof(NonResGalvoCal_type));
	if (!cal) return NULL;
	
	// base class
	cal->baseClass.calName 			= StrDup(calName);
	cal->baseClass.lsModule			= lsModule;
	cal->baseClass.scanEngine		= NULL;
	cal->baseClass.scanAxisType		= NonResonantGalvo;
	cal->baseClass.Discard			= (void (*)(ScanAxisCal_type**)) discard_NonResGalvoCal_type;
	cal->baseClass.UpdateOptics		= (void (*)(ScanAxisCal_type*)) NonResGalvoCal_UpdateOptics;
	
	// child class
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
	cal->mechanicalResponse			= mechanicalResponse;
	cal->sampleDisplacement			= 0;
	
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

void NonResGalvoCal_UpdateOptics (NonResGalvoCal_type* cal)
{
	if (!cal->baseClass.scanEngine) {
		cal->sampleDisplacement = 0;
		return;	   // no scan engine
	}
	
	if (!cal->baseClass.scanEngine->objectiveLens) {
		cal->sampleDisplacement = 0;
		return;	   // no objective
	}
	
	cal->sampleDisplacement = 1000 * cal->mechanicalResponse * Pi()/180.0 * 2 * cal->baseClass.scanEngine->scanLensFL * 
							  cal->baseClass.scanEngine->objectiveLens->objectiveFL/cal->baseClass.scanEngine->tubeLensFL;  
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
static void	NonResGalvoCal_ComVChan_Connected (VChan_type* self, void* VChanOwner, VChan_type* connectedVChan)
{
	
}

// calibration command VChan disconnected callback
static void	NonResGalvoCal_ComVChan_Disconnected (VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan)
{
	
}

// used to exchange number of calibration samples in the waveform
static void NonResGalvoCal_ComNSamplesVChan_Connected (VChan_type* self, void* VChanOwner, VChan_type* connectedVChan)
{
	
}

static void NonResGalvoCal_ComNSamplesVChan_Disconnected (VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan)
{
	
}

// calibration position VChan connected callback
static void	NonResGalvoCal_PosVChan_Connected (VChan_type* self, void* VChanOwner, VChan_type* connectedVChan)
{
	
}

// calibration position VChan disconnected callback
static void	NonResGalvoCal_PosVChan_Disconnected (VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan)
{
	
}

static int init_ScanEngine_type (ScanEngine_type* 		engine, 
								 LaserScanning_type*	lsModule,
								 ScanEngineEnum_type 	engineType,
								 char 					fastAxisComVChanName[],
								 char					fastAxisComNSampVChanName[],
								 char 					slowAxisComVChanName[], 
								 char					slowAxisComNSampVChanName[],
								 char					fastAxisPosVChanName[],
								 char					slowAxisPosVChanName[],
								 char					imageOutVChanName[],
								 char					detectorVChanName[],
								 char					shutterVChanName[],
								 char					pixelPulseTrainVChanName[],
								 char					nPixelsVChanName[],
								 double					pixelClockRate,
								 double					pixelDelay,
								 double					scanLensFL,
								 double					tubeLensFL)					
{
	int				error 							= 0;
	DLDataTypes 	allowedScanAxisPacketTypes[] 	= {DL_Waveform_Double};
	DetChan_type* 	detChan							= NULL;
	
	//------------------------
	// init
	//------------------------
	
	engine->engineType					= engineType;
	engine->scanMode					= ScanEngineMode_Frames;
	// reference to axis calibration
	engine->fastAxisCal					= NULL;
	engine->slowAxisCal					= NULL;
	// reference to the laser scanning module owning the scan engine
	engine->lsModule					= lsModule;
	// task controller
	engine->taskControl					= NULL; 	// added by derived scan engine classes
	// VChans
	engine->VChanFastAxisCom			= NULL;
	engine->VChanFastAxisComNSamples	= NULL;
	engine->VChanSlowAxisCom			= NULL;
	engine->VChanSlowAxisComNSamples	= NULL;
	engine->VChanFastAxisPos			= NULL;
	engine->VChanScanOut				= NULL;
	engine->VChanShutter				= NULL;
	engine->VChanPixelPulseTrain		= NULL;
	engine->VChanNPixels				= NULL;
	engine->DetChans 					= 0;
	// optics
	engine->objectives					= 0;
	// new objective panel handle
	engine->newObjectivePanHndl			= 0;
	// scan settings panel handle
	engine->scanSetPanHndl				= 0;
	// scan engine settings panel handle
	engine->engineSetPanHndl			= 0;
	engine->pixelClockRate				= pixelClockRate;
	engine->pixDelay					= pixelDelay;
	// optics
	engine->scanLensFL					= scanLensFL;
	engine->tubeLensFL					= tubeLensFL;
	engine->objectiveLens				= NULL;
	
	engine->Discard						= NULL;	   // overriden by derived scan engine classes
	
	//------------------------
	// allocate
	//------------------------
	
	// VChans
	nullChk( engine->VChanFastAxisCom			= init_SourceVChan_type(fastAxisComVChanName, DL_RepeatedWaveform_Double, engine, FastAxisComVChan_Connected, FastAxisComVChan_Disconnected) ); 
	nullChk( engine->VChanFastAxisComNSamples	= init_SourceVChan_type(fastAxisComNSampVChanName, DL_ULongLong, engine, NULL, NULL) ); 
	nullChk( engine->VChanSlowAxisCom			= init_SourceVChan_type(slowAxisComVChanName, DL_RepeatedWaveform_Double, engine, SlowAxisComVChan_Connected, SlowAxisComVChan_Disconnected) ); 
	nullChk( engine->VChanSlowAxisComNSamples	= init_SourceVChan_type(slowAxisComNSampVChanName, DL_ULongLong, engine, NULL, NULL) ); 
	nullChk( engine->VChanFastAxisPos			= init_SinkVChan_type(fastAxisPosVChanName, allowedScanAxisPacketTypes, NumElem(allowedScanAxisPacketTypes), engine, VChanDataTimeout, FastAxisPosVChan_Connected, FastAxisPosVChan_Disconnected) ); 
	nullChk( engine->VChanSlowAxisPos			= init_SinkVChan_type(slowAxisPosVChanName, allowedScanAxisPacketTypes, NumElem(allowedScanAxisPacketTypes), engine, VChanDataTimeout, SlowAxisPosVChan_Connected, SlowAxisPosVChan_Disconnected) ); 
	nullChk( engine->VChanScanOut				= init_SourceVChan_type(imageOutVChanName, DL_Image_NIVision, engine, ImageOutVChan_Connected, ImageOutVChan_Disconnected) );
	nullChk( engine->VChanShutter				= init_SourceVChan_type(shutterVChanName, DL_Waveform_UChar, engine, ShutterVChan_Connected, ShutterVChan_Disconnected) ); 
	nullChk( engine->VChanPixelPulseTrain		= init_SourceVChan_type(pixelPulseTrainVChanName, DL_PulseTrain_Ticks, engine, NULL, NULL) ); 	
	nullChk( engine->VChanNPixels				= init_SourceVChan_type(nPixelsVChanName, DL_ULongLong, engine, NULL, NULL) ); 	
	nullChk( engine->DetChans					= ListCreate(sizeof(DetChan_type*)) ); 
	// add one detection channel to the scan engine if required
	if (detectorVChanName) {
		nullChk ( detChan = init_DetChan_type(engine, detectorVChanName) ); 
		ListInsertItem(engine->DetChans, &detChan, END_OF_LIST);
	}
	
	nullChk( engine->objectives					= ListCreate(sizeof(Objective_type*)) );     
	
	return 0;
	
Error:
	
	discard_VChan_type((VChan_type**)&engine->VChanFastAxisCom);
	discard_VChan_type((VChan_type**)&engine->VChanFastAxisComNSamples);
	discard_VChan_type((VChan_type**)&engine->VChanSlowAxisCom);
	discard_VChan_type((VChan_type**)&engine->VChanSlowAxisComNSamples);
	discard_VChan_type((VChan_type**)&engine->VChanFastAxisPos);
	discard_VChan_type((VChan_type**)&engine->VChanSlowAxisPos);
	discard_VChan_type((VChan_type**)&engine->VChanScanOut);
	discard_VChan_type((VChan_type**)&engine->VChanShutter);
	discard_VChan_type((VChan_type**)&engine->VChanPixelPulseTrain);
	discard_VChan_type((VChan_type**)&engine->VChanNPixels); 
	if (engine->DetChans) {
		if (ListNumItems(engine->DetChans)) {
			DetChan_type** 	detChanPtr = ListGetPtrToItem(engine->DetChans, 1);
			discard_DetChan_type(detChanPtr);
		}
		ListDispose(engine->DetChans);
	}
	if (engine->objectives) ListDispose(engine->objectives);
	
	return error;
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
	
	// fast axis command num samples
	if (engine->VChanFastAxisComNSamples) {
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanFastAxisComNSamples);
		discard_VChan_type((VChan_type**)&engine->VChanFastAxisComNSamples);
	}
	
	// slow axis command
	if (engine->VChanSlowAxisCom) {
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanSlowAxisCom);
		discard_VChan_type((VChan_type**)&engine->VChanSlowAxisCom);
	}
	
	// slow axis command num samples
	if (engine->VChanSlowAxisComNSamples) {
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanSlowAxisComNSamples);
		discard_VChan_type((VChan_type**)&engine->VChanSlowAxisComNSamples);
	}
	
	// fast axis position feedback
	if (engine->VChanFastAxisPos) {
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanFastAxisPos);
		RemoveSinkVChan(engine->taskControl, engine->VChanFastAxisPos); 
		discard_VChan_type((VChan_type**)&engine->VChanFastAxisPos);
	}
	
	// slow axis position feedback
	if (engine->VChanSlowAxisPos) {
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanSlowAxisPos);
		RemoveSinkVChan(engine->taskControl, engine->VChanSlowAxisPos);
		discard_VChan_type((VChan_type**)&engine->VChanSlowAxisPos);
	}
	
	// scan engine image output
	if (engine->VChanScanOut) {
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanScanOut);
		discard_VChan_type((VChan_type**)&engine->VChanScanOut);
	}
	
	// shutter command
	if (engine->VChanShutter) {
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanShutter);
		discard_VChan_type((VChan_type**)&engine->VChanShutter);
	}
	
	// pixel pulse train
	if (engine->VChanPixelPulseTrain) {
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanPixelPulseTrain);
		discard_VChan_type((VChan_type**)&engine->VChanPixelPulseTrain);
	}
	
	// n pixels
	if (engine->VChanNPixels) {
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanNPixels);
		discard_VChan_type((VChan_type**)&engine->VChanNPixels);
	}
	
	// detection channels
	size_t	nDet = ListNumItems(engine->DetChans);
	DetChan_type** detChanPtr;
	for (size_t i = 1; i <= nDet; i++) {
		detChanPtr = ListGetPtrToItem(engine->DetChans, i);
		// remove VChan from framework
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)(*detChanPtr)->detVChan);
		RemoveSinkVChan(engine->taskControl, (*detChanPtr)->detVChan);
		discard_DetChan_type(detChanPtr);
	}
	ListDispose(engine->DetChans);
	
	// objectives
	size_t nObjectives = ListNumItems(engine->objectives);
	Objective_type** objectivesPtr;
	for (size_t i = 1; i <= nObjectives; i++) {
		objectivesPtr = ListGetPtrToItem(engine->objectives, i);
		discard_Objective_type(objectivesPtr);
	}
	ListDispose(engine->objectives);
	
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
	if (engine->scanSetPanHndl) Combo_DiscardComboBox(engine->scanSetPanHndl, RectRaster_PixelDwell);
	if (engine->scanSetPanHndl) Combo_DiscardComboBox(engine->scanSetPanHndl, RectRaster_Height);
	
	OKfree(engine);
}

static int OpenScanEngineShutter (ScanEngine_type* engine, BOOL openStatus, char** errorInfo)
{
	unsigned char*			shutterCommand				= NULL;
	DataPacket_type*		shutterDataPacket			= NULL;
	int						error 						= 0;
	
	nullChk( shutterCommand			= malloc (sizeof(unsigned char)) );
	*shutterCommand					= (unsigned char) openStatus;
	nullChk( shutterDataPacket		= init_DataPacket_type(DL_UChar, (void**)&shutterCommand, NULL, NULL) );
	
	errChk( SendDataPacket(engine->VChanShutter, &shutterDataPacket, FALSE, errorInfo) );
	
	
	return 0; // no error
	
Error:
	
	// cleanup
	OKfree(shutterCommand);
	discard_DataPacket_type(&shutterDataPacket);
	
	// create out of memory message
	if (error == UIEOutOfMemory && !*errorInfo)
		*errorInfo = StrDup("Out of memory");
	
	return error;
	
}

static int ReturnRectRasterToParkedPosition (RectRaster_type* engine, char** errorInfo) 
{
	double*						parkedFastAxis				= NULL;
	double*						parkedSlowAxis				= NULL;
	size_t						nParkedSamples				= (size_t) (engine->galvoSamplingRate * 0.1); // generate 1s long parked signal
	RepeatedWaveform_type*		parkedFastAxisWaveform		= NULL;
	RepeatedWaveform_type*		parkedSlowAxisWaveform		= NULL;
	DataPacket_type*			fastAxisDataPacket			= NULL;
	DataPacket_type*			slowAxisDataPacket			= NULL;
	DataPacket_type*			nullPacket					= NULL;
	char*						errMsg						= NULL; 
	int							error						= 0;         
	
	nullChk( parkedFastAxis = malloc(nParkedSamples * sizeof(double)) );
	nullChk( parkedSlowAxis = malloc(nParkedSamples * sizeof(double)) );
	Set1D(parkedFastAxis, nParkedSamples, ((NonResGalvoCal_type*)engine->baseClass.fastAxisCal)->parked);
	Set1D(parkedSlowAxis, nParkedSamples, ((NonResGalvoCal_type*)engine->baseClass.slowAxisCal)->parked);
	nullChk( parkedFastAxisWaveform = init_RepeatedWaveform_type(RepeatedWaveform_Double, engine->galvoSamplingRate, nParkedSamples, (void**)&parkedFastAxis, 0) );
	nullChk( parkedSlowAxisWaveform = init_RepeatedWaveform_type(RepeatedWaveform_Double, engine->galvoSamplingRate, nParkedSamples, (void**)&parkedSlowAxis, 0) );
	nullChk( fastAxisDataPacket		= init_DataPacket_type(DL_RepeatedWaveform_Double, (void**)&parkedFastAxisWaveform, NULL, (DiscardPacketDataFptr_type)discard_RepeatedWaveform_type) );
	nullChk( slowAxisDataPacket		= init_DataPacket_type(DL_RepeatedWaveform_Double, (void**)&parkedSlowAxisWaveform, NULL, (DiscardPacketDataFptr_type)discard_RepeatedWaveform_type) );
	// send parked signal data packets
	errChk( SendDataPacket(engine->baseClass.VChanFastAxisCom, &fastAxisDataPacket, FALSE, &errMsg) );
	errChk( SendDataPacket(engine->baseClass.VChanSlowAxisCom, &slowAxisDataPacket, FALSE, &errMsg) );
	// send NULL packets to terminate AO
	errChk( SendDataPacket(engine->baseClass.VChanFastAxisCom, &nullPacket, FALSE, &errMsg) );
	errChk( SendDataPacket(engine->baseClass.VChanSlowAxisCom, &nullPacket, FALSE, &errMsg) );
	
	return 0;
	
Error:

	// cleanup
	OKfree(parkedFastAxis);
	OKfree(parkedSlowAxis);
	discard_RepeatedWaveform_type(&parkedFastAxisWaveform);
	discard_RepeatedWaveform_type(&parkedSlowAxisWaveform);
	discard_DataPacket_type(&fastAxisDataPacket);
	discard_DataPacket_type(&slowAxisDataPacket);
	
	if (!errMsg)
		errMsg = StrDup("Out of memory");
	
	if (errorInfo)
		*errorInfo = FormatMsg(error, "ReturnToParkedPosition", errMsg);
	
	OKfree(errMsg);	
	return error;
	
}

static RectRasterScanSet_type* init_RectRasterScanSet_type (double pixSize, uInt32 height, int heightOffset, uInt32 width, int widthOffset, double pixelDwellTime)
{
	RectRasterScanSet_type*	scanSet = malloc(sizeof(RectRasterScanSet_type));
	if (!scanSet) return NULL;
	
	// init
	scanSet->pixSize 		= pixSize;
	scanSet->height 		= height;
	scanSet->heightOffset 	= heightOffset;
	scanSet->width			= width;
	scanSet->widthOffset	= widthOffset;
	scanSet->pixelDwellTime	= pixelDwellTime;
	
	return scanSet;
}

static void discard_RectRasterScanSet_type (RectRasterScanSet_type** scanSetPtr)
{
	OKfree(*scanSetPtr);
}

static RectRaster_type* init_RectRaster_type (LaserScanning_type*				lsModule,
														char 					engineName[], 
														char 					fastAxisComVChanName[],
														char					fastAxisComNSampVChanName[],
														char 					slowAxisComVChanName[],
														char					slowAxisComNSampVChanName[],
														char					fastAxisPosVChanName[],
														char					slowAxisPosVChanName[],
														char					imageOutVChanName[],
														char					detectorVChanName[],
														char					shutterVChanName[],
														char					pixelPulseTrainVChanName[],
														char					nPixelsVChanName[],
														double					galvoSamplingRate,
														double					pixelClockRate,
														double					pixelDelay,
														uInt32					scanHeight,
														int						scanHeightOffset,
														uInt32					scanWidth,
														int						scanWidthOffset,
														double					pixelSize,
														double					pixelDwellTime,
														double					scanLensFL,
														double					tubeLensFL)
{
	int					error 	= 0;
	RectRaster_type*	engine 	= malloc (sizeof(RectRaster_type));
	if (!engine) return NULL;
	
	//--------------------------------------------------------
	// init base scan engine class
	//--------------------------------------------------------
	errChk( init_ScanEngine_type(&engine->baseClass, lsModule, ScanEngine_RectRaster_NonResonantGalvoFastAxis_NonResonantGalvoSlowAxis, fastAxisComVChanName, fastAxisComNSampVChanName, slowAxisComVChanName, slowAxisComNSampVChanName, fastAxisPosVChanName, 
						 slowAxisPosVChanName, imageOutVChanName, detectorVChanName, shutterVChanName, pixelPulseTrainVChanName, nPixelsVChanName, pixelClockRate, pixelDelay, scanLensFL, tubeLensFL) );
	// override discard method
	engine->baseClass.Discard			= discard_RectRaster_type;
	// add task controller
	engine->baseClass.taskControl		= init_TaskControl_type(engineName, engine, DLGetCommonThreadPoolHndl(), ConfigureTC_RectRaster, UnconfigureTC_RectRaster, IterateTC_RectRaster, AbortIterationTC_RectRaster, StartTC_RectRaster, ResetTC_RectRaster, 
										  DoneTC_RectRaster, StoppedTC_RectRaster, TaskTreeStatus_RectRaster, NULL, ModuleEventHandler_RectRaster, ErrorTC_RectRaster);
	
	if (!engine->baseClass.taskControl) {discard_RectRaster_type((ScanEngine_type**)&engine); return NULL;}
	
	//--------------------------------------------------------
	// init RectRaster_type
	//--------------------------------------------------------
	
	engine->galvoSamplingRate			= galvoSamplingRate;
	
	// scan settings
	engine->scanSettings.height			= scanHeight;
	engine->scanSettings.heightOffset	= scanHeightOffset;
	engine->scanSettings.width			= scanWidth;
	engine->scanSettings.widthOffset	= scanWidthOffset;
	engine->scanSettings.pixelDwellTime	= pixelDwellTime;
	engine->scanSettings.pixSize		= pixelSize;
	
	engine->imgBuffers				= NULL;
	engine->nImgBuffers				= 0;
	engine->pointROIs				= ListCreate(sizeof(Point_type*));

	return engine;
	
Error:
	
	free(engine);
	return NULL;
}

static void	discard_RectRaster_type (ScanEngine_type** engine)
{
	RectRaster_type*	rectRaster = (RectRaster_type*) *engine;
	
	//------------------------------
	// discard RectRaster_type data
	//------------------------------
	
	// discard image assembly buffers
	for (size_t i = 0; i < rectRaster->nImgBuffers; i++)
		discard_RectRasterImgBuffer_type(&rectRaster->imgBuffers[i]);
	
	OKfree(rectRaster->imgBuffers);
	rectRaster->nImgBuffers = 0;
	
	// discard ROIs
	if (rectRaster->pointROIs) {
		size_t			nROIs = ListNumItems(rectRaster->pointROIs);
		Point_type**	pointPtr;
		for (size_t i = 1; i <= nROIs; i++) {
			pointPtr = ListGetPtrToItem(rectRaster->pointROIs, i);
			discard_ROI_type((ROI_type**)pointPtr);
		}
		ListDispose(rectRaster->pointROIs);
	}
		
	//------------------------------
	// discard Scan Engine data
	//------------------------------
	discard_ScanEngine_type(engine);
}

static RectRasterImgBuffer_type* init_RectRasterImgBuffer_type (DetChan_type* detChan, uInt32 imgWidth, uInt32 imgHeight, size_t nSkipPixels, DLDataTypes pixelDataType, BOOL reverseWidthDirection, BOOL reverseHeightDirection)
{
#define init_RectRasterImgBuffer_type_Err_PixelDataTypeInvalid	-1
	int 						error;
	RectRasterImgBuffer_type*	buffer 			= malloc (sizeof(RectRasterImgBuffer_type));
	size_t						pixelSizeBytes 	= 0;
	if (!buffer) return NULL;
	
	switch (pixelDataType) {
		case DL_Waveform_UChar:
			pixelSizeBytes 		= sizeof(unsigned char);   
			break;
		
		case DL_Waveform_UShort:
			pixelSizeBytes 		= sizeof(unsigned short);
			break;
			
		case DL_Waveform_Short:
			pixelSizeBytes 		= sizeof(short); 
			break;
			
		case DL_Waveform_Float:
			pixelSizeBytes 		= sizeof(float); 
			break;
			
		default:
			
			goto Error;
	}
	
	// init
	buffer->nImagePixels 			= 0;
	buffer->nAssembledColumns   	= 0;
	buffer->tmpPixels				= NULL;
	buffer->nTmpPixels				= 0;
	buffer->nSkipPixels				= nSkipPixels;
	buffer->revHeightFlag			= reverseHeightDirection;
	buffer->revWidthFlag			= reverseWidthDirection;
	buffer->detChan					= detChan;
	
	nullChk( buffer->imagePixels 	= malloc(imgWidth * imgHeight * pixelSizeBytes) );
	
	return buffer;
	
Error:
	
	// cleanup
	OKfree(buffer->imagePixels);
	
	free(buffer);
	return NULL;	
}

static void	discard_RectRasterImgBuffer_type (RectRasterImgBuffer_type** rectRasterPtr)
{
	if (!*rectRasterPtr) return;
	
	OKfree((*rectRasterPtr)->imagePixels);
	OKfree((*rectRasterPtr)->tmpPixels);
	
	OKfree(*rectRasterPtr)
}

static int CVICALLBACK NonResRectRasterScan_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	RectRaster_type*		scanEngine		= callbackData;
	NonResGalvoCal_type*	fastAxisCal		= (NonResGalvoCal_type*) scanEngine->baseClass.fastAxisCal;     
	NonResGalvoCal_type*	slowAxisCal		= (NonResGalvoCal_type*) scanEngine->baseClass.slowAxisCal;
	int						listItemIdx;

	switch (event)
	{
		case EVENT_COMMIT:
			
			switch (control) {
			
				case RectRaster_Objective:
					
					Objective_type**	objectivePtr;
					GetCtrlIndex(panel, control, &listItemIdx);
					objectivePtr = ListGetPtrToItem(scanEngine->baseClass.objectives, listItemIdx+1);
					scanEngine->baseClass.objectiveLens = *objectivePtr;
					
					// configure/unconfigure scan engine
					if (NonResRectRasterScan_ValidConfig(scanEngine)) {
				
						NonResRectRasterScan_ScanHeights(scanEngine);
						NonResRectRasterScan_PixelDwellTimes(scanEngine);
					
						if (NonResRectRasterScan_ReadyToScan(scanEngine))
							TaskControlEvent(scanEngine->baseClass.taskControl, TASK_EVENT_CONFIGURE, NULL, NULL);
						else
							TaskControlEvent(scanEngine->baseClass.taskControl, TASK_EVENT_UNCONFIGURE, NULL, NULL); 	
					} else
						TaskControlEvent(scanEngine->baseClass.taskControl, TASK_EVENT_UNCONFIGURE, NULL, NULL);
					break;
					
				case RectRaster_Mode:
					
					int		scanMode;
					GetCtrlVal(panel, control, &scanMode);
					scanEngine->baseClass.scanMode = (ScanEngineModesEnum_type) scanMode;
					break;
					
				case RectRaster_ExecutionMode:
					
					BOOL	executionMode;
					GetCtrlVal(panel, control, &executionMode);
					SetTaskControlMode(scanEngine->baseClass.taskControl, (TaskMode_type)!executionMode);
					// dim/undim N Frames
					if (executionMode)
						SetCtrlAttribute(panel, RectRaster_NFrames, ATTR_DIMMED, 1);
					else
						SetCtrlAttribute(panel, RectRaster_NFrames, ATTR_DIMMED, 0);
					
					break;
					
				case RectRaster_NFrames:
					
					unsigned int nFrames = 0;
					
					GetCtrlVal(panel, control, &nFrames);
					SetTaskControlIterations(scanEngine->baseClass.taskControl, nFrames);
					
					break;
				
				case RectRaster_Height:
					
					char   		heightString[NonResGalvoRasterScan_Max_ComboboxEntryLength+1];
					double		height;
					uInt32		heightPix;
					
					// get image height
					GetCtrlVal(panel, control, heightString);
					if (Scan(heightString, "%s>%f", &height) <= 0) {
						// invalid entry, display previous value
						Fmt(heightString, "%s<%f[p1]", scanEngine->scanSettings.height * scanEngine->scanSettings.pixSize);
						SetCtrlVal(panel, control, heightString);
						return 0;
					}
					
					// no negative or zero value for height
					if (height <= 0){
						Fmt(heightString, "%s<%f[p1]", scanEngine->scanSettings.height * scanEngine->scanSettings.pixSize);
						SetCtrlVal(panel, control, heightString);
						return 0;
					}
					
					// make sure height is a multiple of the pixel size
					heightPix = (uInt32) ceil(height/scanEngine->scanSettings.pixSize);
					height = heightPix * scanEngine->scanSettings.pixSize;
					Fmt(heightString, "%s<%f[p1]", height);
					SetCtrlVal(panel, control, heightString);
					
					// check if requested image falls within ROI
					if(!RectROIInsideRect (height, scanEngine->scanSettings.width * scanEngine->scanSettings.pixSize, scanEngine->scanSettings.heightOffset * scanEngine->scanSettings.pixSize, 
												 scanEngine->scanSettings.widthOffset * scanEngine->scanSettings.pixSize, 2 * fastAxisCal->commandVMax * fastAxisCal->sampleDisplacement,
												2 * slowAxisCal->commandVMax * slowAxisCal->sampleDisplacement)) {
						DLMsg("Requested ROI exceeds maximum limit for laser scanning module ", 1);
						char*	scanEngineName = GetTaskControlName(scanEngine->baseClass.taskControl);
						DLMsg(scanEngineName, 0);
						OKfree(scanEngineName);
						DLMsg(".\n\n", 0);
						Fmt(heightString, "%s<%f[p1]", scanEngine->scanSettings.height * scanEngine->scanSettings.pixSize);
						SetCtrlVal(panel, control, heightString);
						return 0;
					}
					
					// update height
					scanEngine->scanSettings.height = heightPix;
					Fmt(heightString, "%s<%f[p1]", height);  
					SetCtrlVal(panel, control, heightString); 
					// update pixel dwell times
					NonResRectRasterScan_PixelDwellTimes(scanEngine); 
					
					break;
					
				case RectRaster_Width:
					
					double		width;					// in [um]
					uInt32		widthPix;				// in [pix]
					GetCtrlVal(panel, control, &width);
					// adjust width to be a multiple of pixel size
					widthPix 	= (uInt32) ceil(width/scanEngine->scanSettings.pixSize);
					if (!widthPix) widthPix = 1;
					width 		= widthPix * scanEngine->scanSettings.pixSize;
					
					if (!RectROIInsideRect(scanEngine->scanSettings.height * scanEngine->scanSettings.pixSize, width, scanEngine->scanSettings.heightOffset * scanEngine->scanSettings.pixSize, 
						scanEngine->scanSettings.widthOffset * scanEngine->scanSettings.pixSize, 2 * fastAxisCal->commandVMax * fastAxisCal->sampleDisplacement, 
						2 * slowAxisCal->commandVMax * slowAxisCal->sampleDisplacement)) {
							// return to previous value
							SetCtrlVal(panel, control, scanEngine->scanSettings.width * scanEngine->scanSettings.pixSize);
							DLMsg("Requested ROI exceeds maximum limit for laser scanning module ", 1);
							char*	scanEngineName = GetTaskControlName(scanEngine->baseClass.taskControl);
							DLMsg(scanEngineName, 0);
							OKfree(scanEngineName);
							DLMsg(".\n\n", 0);
							return 0; // stop here
						}
						
					// update width
					scanEngine->scanSettings.width = widthPix;  // in [pix]
					SetCtrlVal(panel, control, width); 
					break;
					
				case RectRaster_HeightOffset:
					
					double		heightOffset;					// in [um]
					int			heightOffsetPix;				// in [pix]
					GetCtrlVal(panel, control, &heightOffset);  // in [um]
					// adjust heightOffset to be a multiple of pixel size
					heightOffsetPix		= (int) floor(heightOffset/scanEngine->scanSettings.pixSize);
					heightOffset 		= heightOffsetPix * scanEngine->scanSettings.pixSize;
					
					if (!RectROIInsideRect(scanEngine->scanSettings.height * scanEngine->scanSettings.pixSize, scanEngine->scanSettings.width * scanEngine->scanSettings.pixSize, heightOffset, 
						scanEngine->scanSettings.widthOffset * scanEngine->scanSettings.pixSize, 2 * fastAxisCal->commandVMax * fastAxisCal->sampleDisplacement,
						2 * slowAxisCal->commandVMax * slowAxisCal->sampleDisplacement)) {
							// return to previous value
							SetCtrlVal(panel, control, scanEngine->scanSettings.heightOffset * scanEngine->scanSettings.pixSize);
							DLMsg("Requested ROI exceeds maximum limit for laser scanning module ", 1);
							char*	scanEngineName = GetTaskControlName(scanEngine->baseClass.taskControl);
							DLMsg(scanEngineName, 0);
							OKfree(scanEngineName);
							DLMsg(".\n\n", 0);
							return 0; // stop here
					}
							
					// update height offset
					scanEngine->scanSettings.heightOffset = heightOffsetPix;  // in [pix]
					SetCtrlVal(panel, control, heightOffset);
					break;
					
				case RectRaster_WidthOffset:
					
					double		widthOffset;					// in [um]
					int			widthOffsetPix;					// in [pix]
					GetCtrlVal(panel, control, &widthOffset);
					// adjust heightOffset to be a multiple of pixel size
					widthOffsetPix 		= (int) floor(widthOffset/scanEngine->scanSettings.pixSize);
					widthOffset 		= widthOffsetPix * scanEngine->scanSettings.pixSize;
					
					if (!RectROIInsideRect(scanEngine->scanSettings.height * scanEngine->scanSettings.pixSize, scanEngine->scanSettings.width * scanEngine->scanSettings.pixSize, 
						scanEngine->scanSettings.heightOffset * scanEngine->scanSettings.pixSize, widthOffset, 2 * fastAxisCal->commandVMax * fastAxisCal->sampleDisplacement, 
						2 * slowAxisCal->commandVMax * slowAxisCal->sampleDisplacement)) {
							// return to previous value
							SetCtrlVal(panel, control, scanEngine->scanSettings.widthOffset * scanEngine->scanSettings.pixSize);
							DLMsg("Requested ROI exceeds maximum limit for laser scanning module ", 1);
							char*	scanEngineName = GetTaskControlName(scanEngine->baseClass.taskControl);
							DLMsg(scanEngineName, 0);
							OKfree(scanEngineName);
							DLMsg(".\n\n", 0);
							return 0; // stop here
						}
						
					// update width offset
					scanEngine->scanSettings.widthOffset = widthOffsetPix;  // in [pix]
					SetCtrlVal(panel, control, widthOffset); 
					break;
					
				case RectRaster_PixelDwell:
					
					char   		dwellTimeString[NonResGalvoRasterScan_Max_ComboboxEntryLength+1];
					double		dwellTime;
					
					// get dwell time string
					GetCtrlVal(panel, control, dwellTimeString);
					if (Scan(dwellTimeString, "%s>%f", &dwellTime) <= 0) {
						// invalid entry, display previous value
						Fmt(dwellTimeString, "%s<%f[p1]", scanEngine->scanSettings.pixelDwellTime);
						SetCtrlVal(panel, control, dwellTimeString);
						return 0;
					}
					
					// dwell time must be within bounds
					if ((dwellTime < NonResGalvoScan_MinPixelDwellTime) || (dwellTime > NonResGalvoScan_MaxPixelDwellTime)){
						Fmt(dwellTimeString, "%s<%f[p1]", scanEngine->scanSettings.pixelDwellTime);
						SetCtrlVal(panel, control, dwellTimeString);
						return 0;
					}
					
					// make sure dwell time is a multiple of the pixel clock rate
					dwellTime =  ceil(dwellTime * scanEngine->baseClass.pixelClockRate) /scanEngine->baseClass.pixelClockRate;
					Fmt(dwellTimeString, "%s<%f[p1]", dwellTime);
					SetCtrlVal(panel, control, dwellTimeString);	  // <--- can be taken out
					
					scanEngine->scanSettings.pixelDwellTime = dwellTime;
					// update preferred heights and pixel dwell times
					NonResRectRasterScan_ScanHeights(scanEngine);
				
					break;
					
				case RectRaster_FPS:
					
					break;
					
				case RectRaster_PixelSize:
					
					double newPixelSize = 0;
					
					GetCtrlVal(panel, control, &newPixelSize);
					
					if (newPixelSize < NonResGalvoRasterScan_Min_PixelSize) {
						SetCtrlVal(panel, control, NonResGalvoRasterScan_Min_PixelSize);
						newPixelSize = NonResGalvoRasterScan_Min_PixelSize;
					} else
						if (newPixelSize > NonResGalvoRasterScan_Max_PixelSize) {
							SetCtrlVal(panel, control, NonResGalvoRasterScan_Max_PixelSize);
							newPixelSize = NonResGalvoRasterScan_Max_PixelSize;
						}
					
					// calculate new image width, height and offsets in pixels to keep current image size in sample space constant
					scanEngine->scanSettings.width 			= (uInt32) (scanEngine->scanSettings.width * scanEngine->scanSettings.pixSize/newPixelSize);
					scanEngine->scanSettings.widthOffset	= (int) (scanEngine->scanSettings.widthOffset * scanEngine->scanSettings.pixSize/newPixelSize);
					scanEngine->scanSettings.height 		= (uInt32) (scanEngine->scanSettings.height * scanEngine->scanSettings.pixSize/newPixelSize);
					scanEngine->scanSettings.heightOffset 	= (int) (scanEngine->scanSettings.heightOffset * scanEngine->scanSettings.pixSize/newPixelSize);
					// update pixel size
					scanEngine->scanSettings.pixSize = newPixelSize;
					// adjust width control to be multiple of new pixel size
					SetCtrlVal(panel, RectRaster_Width, scanEngine->scanSettings.width * scanEngine->scanSettings.pixSize);
					// adjust width offset control to be multiple of new pixel size
					SetCtrlVal(panel, RectRaster_WidthOffset, scanEngine->scanSettings.widthOffset * scanEngine->scanSettings.pixSize);
					// adjust height offset control to be multiple of new pixel size
					SetCtrlVal(panel, RectRaster_HeightOffset, scanEngine->scanSettings.heightOffset * scanEngine->scanSettings.pixSize);
					// update preferred heights
					NonResRectRasterScan_ScanHeights(scanEngine);
					// update preferred pixel dwell times
					NonResRectRasterScan_PixelDwellTimes(scanEngine);
					
					break;
				
			}

			break;
	}
	return 0;
}

void NonResRectRasterScan_ScanHeights (RectRaster_type* scanEngine)
{  
	int						error 					= 0;
	size_t   				blank;
	size_t   				deadTimePixels;
	BOOL     				inFOVFlag             	= 1; 
	size_t   				i                     	= 1;	// counts how many galvo samples of 1/galvoSampleRate duration fit in a line scan
	double   				pixelsPerLine;
	double   				rem;
	char     				dwellTimeString[NonResGalvoRasterScan_Max_ComboboxEntryLength+1];
	char					heightString[NonResGalvoRasterScan_Max_ComboboxEntryLength+1];
	ListType 				Heights               	= ListCreate(sizeof(double));
	double					ROIHeight;
	NonResGalvoCal_type*	fastAxisCal				= (NonResGalvoCal_type*) scanEngine->baseClass.fastAxisCal;     
	NonResGalvoCal_type*	slowAxisCal				= (NonResGalvoCal_type*) scanEngine->baseClass.slowAxisCal;

	// enforce pixeldwelltime to be an integer multiple of 1/pixelClockRate
	// make sure pixeldwelltime is in [us] and pixelClockRate in [Hz] 
	scanEngine->scanSettings.pixelDwellTime = floor(scanEngine->scanSettings.pixelDwellTime * scanEngine->baseClass.pixelClockRate) / scanEngine->baseClass.pixelClockRate; // result in [us]
	Fmt(dwellTimeString,"%s<%f[p3]", scanEngine->scanSettings.pixelDwellTime);
	SetCtrlVal(scanEngine->baseClass.scanSetPanHndl, RectRaster_PixelDwell, dwellTimeString);	// in [us]
	
	// empty list of preferred heights
	errChk( Combo_DeleteComboItem(scanEngine->baseClass.scanSetPanHndl, RectRaster_Height, 0, -1) );
	
	// empty value for Height, if the calculated list contains at least one item, then it will be filled out
	SetCtrlVal(scanEngine->baseClass.scanSetPanHndl, RectRaster_Height, ""); 
	
	//---Calculation of prefered heights---
	//deadtime in [ms], scanEngine->pixelDwellTime in [us]
	blank = (size_t)ceil(fastAxisCal->triangleCal->deadTime * 1e+3 /scanEngine->scanSettings.pixelDwellTime);  
	// the sum of unused pixels at the beginning and end of each line
	deadTimePixels = 2 * blank; 
	
	while (inFOVFlag == TRUE){
		// this condition ensures that at the end of each line, the pixels remain in sync with the galvo samples
		// this allows for freedom in choosing the pixel dwell time (and implicitly the scan speed)
		pixelsPerLine = floor(i/(scanEngine->galvoSamplingRate * scanEngine->scanSettings.pixelDwellTime * 1e-6));
		if (pixelsPerLine < deadTimePixels) {
			i++;
			continue;
		}
		rem = i/(scanEngine->galvoSamplingRate * scanEngine->scanSettings.pixelDwellTime * 1e-6) - pixelsPerLine;
		
		ROIHeight = (pixelsPerLine - deadTimePixels) * scanEngine->scanSettings.pixSize;
		if (rem == 0.0){ 
			inFOVFlag = RectROIInsideRect(ROIHeight, scanEngine->scanSettings.width * scanEngine->scanSettings.pixSize, scanEngine->scanSettings.heightOffset * scanEngine->scanSettings.pixSize, 
						scanEngine->scanSettings.widthOffset * scanEngine->scanSettings.pixSize, 2 * fastAxisCal->commandVMax * fastAxisCal->sampleDisplacement, 2 * slowAxisCal->commandVMax * slowAxisCal->sampleDisplacement);
			
			if (CheckNonResGalvoScanFreq(fastAxisCal, scanEngine->scanSettings.pixSize, scanEngine->scanSettings.pixelDwellTime, ROIHeight + deadTimePixels * scanEngine->scanSettings.pixSize) && inFOVFlag) {
				// add to list if scan frequency is valid
				Fmt(heightString,"%s<%f[p1]", ROIHeight);
				errChk( Combo_InsertComboItem(scanEngine->baseClass.scanSetPanHndl, RectRaster_Height, -1, heightString) );
				ListInsertItem(Heights, &ROIHeight, END_OF_LIST);
			}
			 
		}
		if (ROIHeight > 2 * fastAxisCal->commandVMax * fastAxisCal->sampleDisplacement) inFOVFlag = FALSE;
		i++;
	}
	
	// select value from the list that is closest to the old height value
	size_t 		nElem 				= ListNumItems(Heights);
	double   	diffOld;
	double   	diffNew;
	double   	heightItem; 				// in [um]
	double		targetROIHeight 	= scanEngine->scanSettings.height * scanEngine->scanSettings.pixSize;
	size_t     	itemPos;
	if (nElem) {
		ListGetItem(Heights, &heightItem, 1);
		diffOld = fabs(heightItem - targetROIHeight);
		itemPos = 1;
		for (size_t j = 2; j <= nElem; j++) {
			ListGetItem(Heights, &heightItem, j);
			diffNew = fabs(heightItem - targetROIHeight);
			if (diffNew < diffOld) {
				diffOld = diffNew; 
				itemPos = j;
			}
		}
		ListGetItem(Heights, &heightItem, itemPos);
		// update scan control UI
		Fmt(heightString,"%s<%f[p1]", heightItem);
		SetCtrlVal(scanEngine->baseClass.scanSetPanHndl, RectRaster_Height, heightString);
		// update scan engine structure
		scanEngine->scanSettings.height = (uInt32) (heightItem/scanEngine->scanSettings.pixSize);	// in [pix]
	} else
		// update scan engine structure
		scanEngine->scanSettings.height = 0;														// in [pix]
	
	ListDispose(Heights);
	
Error:
	return;
}

void NonResRectRasterScan_PixelDwellTimes (RectRaster_type* scanEngine)
{
	int						error						= 0;
	char					dwellTimeString[NonResGalvoRasterScan_Max_ComboboxEntryLength+1];
	double    				pixelDwell                  = 0;
	double    				pixelDwellOld;
	uInt32    				k                           = 1;  // Counts how many galvo samples of 1/galvo_sample_rate duration fit in a line scan
	uInt32    				n; 							      // Counts how many pixel dwell times fit in the deadtime, note that the total dead time per line is twice this value
	double    				rem;
	ListType  				dwellTimes				  	= ListCreate(sizeof(double));
	NonResGalvoCal_type*	fastAxisCal					= (NonResGalvoCal_type*) scanEngine->baseClass.fastAxisCal;    
	
	// empty list of preferred dwell times
	errChk( Combo_DeleteComboItem(scanEngine->baseClass.scanSetPanHndl, RectRaster_PixelDwell, 0, -1) );
	
	// empty value for Dwell Time, if the calculated list contains at least one item, then it will be filled out
	SetCtrlVal(scanEngine->baseClass.scanSetPanHndl, RectRaster_PixelDwell, "");
	
	// stop here if scan height is 0
	if (!scanEngine->scanSettings.height) {
		scanEngine->scanSettings.pixelDwellTime = 0;
		ListDispose(dwellTimes);
		return;
	}
	
	// make sure that pixelDwell is a multiple of 1/pix_clock_rate      
	pixelDwell 	= ceil (NonResGalvoScan_MinPixelDwellTime * (scanEngine->baseClass.pixelClockRate/1e6)) * (1e6/scanEngine->baseClass.pixelClockRate);
	n 		 	= (uInt32) ceil (fastAxisCal->triangleCal->deadTime * (1e3/pixelDwell));           				// dead time pixels at the beginning and end of each line
	k		 	= (uInt32) ceil (pixelDwell * (scanEngine->galvoSamplingRate/1e6) * (scanEngine->scanSettings.height + 2*n));
	while (pixelDwell <= NonResGalvoScan_MaxPixelDwellTime) {
		
		pixelDwellOld = 0;
		while (pixelDwellOld != pixelDwell) {
			pixelDwellOld = pixelDwell;
			n 		 = (uInt32) ceil (fastAxisCal->triangleCal->deadTime * (1e3/pixelDwell));   				// dead time pixels at the beginning and end of each line
			pixelDwell = k/(scanEngine->galvoSamplingRate * (scanEngine->scanSettings.height + 2*n)) * 1e6;      // in [us]
		}
	
		// check if the pixel dwell time is a multiple of 1/pixelClockRate 
		rem = pixelDwell * (scanEngine->baseClass.pixelClockRate/1e6) - floor(pixelDwell * (scanEngine->baseClass.pixelClockRate/1e6));
		if (rem == 0.0)
			if (CheckNonResGalvoScanFreq(fastAxisCal, scanEngine->scanSettings.pixSize, pixelDwell, scanEngine->scanSettings.height * scanEngine->scanSettings.pixSize + 2 * n * scanEngine->scanSettings.pixSize)) {
				// add to list
				Fmt(dwellTimeString,"%s<%f[p3]", pixelDwell);
				errChk( Combo_InsertComboItem(scanEngine->baseClass.scanSetPanHndl, RectRaster_PixelDwell, -1, dwellTimeString) );
				ListInsertItem(dwellTimes, &pixelDwell, END_OF_LIST);
			}
		
		k++;
	}
	
	// select value from the list that is closest to the old_dwelltime value
	size_t      nElem			= ListNumItems(dwellTimes);
	int       	itemPos;
	double    	dwellTimeItem;
	double    	diffOld;
	double    	diffNew;
	
	if (nElem) {
		ListGetItem(dwellTimes, &dwellTimeItem, 1);
		diffOld = fabs(dwellTimeItem - scanEngine->scanSettings.pixelDwellTime);
		itemPos = 1;
		for (size_t i = 2; i <= nElem; i++) {
			ListGetItem(dwellTimes, &dwellTimeItem, i);
			diffNew = fabs(dwellTimeItem - scanEngine->scanSettings.pixelDwellTime);
			if (diffNew < diffOld) {diffOld = diffNew; itemPos = i;}
		}
		
		ListGetItem(dwellTimes, &dwellTimeItem, itemPos);
		// update scan control UI
		Fmt(dwellTimeString,"%s<%f[p3]", dwellTimeItem);
		SetCtrlVal(scanEngine->baseClass.scanSetPanHndl, RectRaster_PixelDwell, dwellTimeString);
		// update scan engine structure
		scanEngine->scanSettings.pixelDwellTime = dwellTimeItem;
	} else
		scanEngine->scanSettings.pixelDwellTime = 0;
	
	ListDispose(dwellTimes);
	
Error:
	
	return;
}
																	
// Determines whether a given rectangular ROI with given width and height as well as offsets is falling within a circular region with given radius.
// 0 - Rect ROI is outside the circular perimeter, 1 - otherwise.
static BOOL RectROIInsideCircle (double ROIHeight, double ROIWidth, double ROIHeightOffset, double ROIWidthOffset, double circleRadius)
{
	BOOL ok = TRUE;
	
	if (sqrt(pow((ROIHeightOffset-ROIHeight/2),2)+pow((ROIWidthOffset+ROIWidth/2),2)) > fabs(circleRadius)) ok = FALSE;
	if (sqrt(pow((ROIHeightOffset+ROIHeight/2),2)+pow((ROIWidthOffset+ROIWidth/2),2)) > fabs(circleRadius)) ok = FALSE;
	if (sqrt(pow((ROIHeightOffset-ROIHeight/2),2)+pow((ROIWidthOffset-ROIWidth/2),2)) > fabs(circleRadius)) ok = FALSE;
	if (sqrt(pow((ROIHeightOffset+ROIHeight/2),2)+pow((ROIWidthOffset-ROIWidth/2),2)) > fabs(circleRadius)) ok = FALSE;
	
	return ok;
}

static BOOL RectROIInsideRect (double ROIHeight, double ROIWidth, double ROIHeightOffset, double ROIWidthOffset, double maxHeight, double maxWidth)
{
	return (2*fabs(ROIHeightOffset)+fabs(ROIHeight) <= fabs(maxHeight)) && (2*fabs(ROIWidthOffset)+fabs(ROIWidth) <= fabs(maxWidth));
}

/// HIFN Evaluates whether the scan engine has a valid configuration
/// HIRET True, if scan engine configuration is valid and False otherwise.
static BOOL	NonResRectRasterScan_ValidConfig (RectRaster_type* scanEngine)
{
	// check if scan engine configuration is valid
	BOOL	validFlag = scanEngine->baseClass.fastAxisCal && scanEngine->baseClass.slowAxisCal && scanEngine->baseClass.objectiveLens &&
		   				(scanEngine->baseClass.pixelClockRate != 0.0) && (scanEngine->scanSettings.pixSize != 0.0) && 
						ListNumItems(scanEngine->baseClass.DetChans);
	if (validFlag)
		validFlag = (scanEngine->galvoSamplingRate != 0.0) && (scanEngine->baseClass.scanLensFL != 0.0) && (scanEngine->baseClass.tubeLensFL != 0.0) &&
					(scanEngine->baseClass.objectiveLens->objectiveFL != 0.0);
	
	// check if fast scan axis configuration is valid
	if (validFlag)
		switch (scanEngine->baseClass.fastAxisCal->scanAxisType) {
				
			case NonResonantGalvo:
				
				NonResGalvoCal_type* nrgCal = (NonResGalvoCal_type*) scanEngine->baseClass.fastAxisCal;
				
				validFlag = (nrgCal->commandVMax != 0.0) && (nrgCal->mechanicalResponse != 0.0) && (nrgCal->sampleDisplacement != 0.0);
				
				break;
				
			case ResonantGalvo:
				
				break;
				
			case AOD:
				
				break;
				
			case Translation:
				
				break;
		}
	
	// check if slow scan axis configuration is valid
	if (validFlag)
		switch (scanEngine->baseClass.slowAxisCal->scanAxisType) {
				
			case NonResonantGalvo:
				
				NonResGalvoCal_type* nrgCal = (NonResGalvoCal_type*) scanEngine->baseClass.slowAxisCal;
				
				validFlag = (nrgCal->commandVMax != 0.0) && (nrgCal->mechanicalResponse != 0.0) && (nrgCal->sampleDisplacement != 0.0);
				
				break;
				
			case ResonantGalvo:
				
				break;
				
			case AOD:
				
				break;
				
			case Translation:
				
				break;
		}
	
	return validFlag;
}

static BOOL NonResRectRasterScan_ReadyToScan (RectRaster_type* scanEngine)
{
	BOOL scanReady = NonResRectRasterScan_ValidConfig(scanEngine) && (scanEngine->scanSettings.height != 0.0) && (scanEngine->scanSettings.width != 0.0);
	
	SetCtrlVal(scanEngine->baseClass.scanSetPanHndl, RectRaster_Ready, scanReady);
	
	return scanReady;
}

/// HIFN Generates galvo scan signals for bidirectional raster scanning
static int NonResRectRasterScan_GenerateScanSignals (RectRaster_type* scanEngine, char** errorInfo)
{
#define NonResRectRasterScan_GenerateScanSignals_Err_ScanSignals		-1
	
	// init dynamically allocated signals
	double*						fastAxisCommandSignal						= NULL;
	double*						fastAxisCompensationSignal					= NULL;
	double*						slowAxisCompensationSignal					= NULL; 
	double*						parkedVoltageSignal							= NULL;
	uInt64*						nGalvoSamplesPtr							= NULL;
	uInt64* 					nPixelsPtr									= NULL;
	Waveform_type* 				fastAxisScan_Waveform						= NULL;
	Waveform_type*				fastAxisMoveFromParkedWaveform				= NULL;
	Waveform_type*				fastAxisMoveFromParkedCompensatedWaveform   = NULL;
	Waveform_type*				slowAxisScan_Waveform						= NULL;
	Waveform_type*				slowAxisMoveFromParkedWaveform  			= NULL;
	Waveform_type*				slowAxisMoveFromParkedCompensatedWaveform   = NULL; 
	RepeatedWaveform_type*		fastAxisMoveFromParked_RepWaveform			= NULL;
	RepeatedWaveform_type*		slowAxisMoveFromParked_RepWaveform			= NULL;
	RepeatedWaveform_type*		fastAxisScan_RepWaveform					= NULL;
	RepeatedWaveform_type*		slowAxisScan_RepWaveform					= NULL; 
	DataPacket_type*			galvoCommandPacket							= NULL; 
	DataPacket_type*			pixelPulseTrainPacket						= NULL;
	DataPacket_type*			nPixelsPacket								= NULL;
	DataPacket_type*			nullPacket									= NULL;
	PulseTrain_type*			pixelPulseTrain								= NULL;
	char*						errMsg										= NULL;
	int							error 										= 0;
	
//============================================================================================================================================================================================
//                          					   	Preparation of Scan Waveforms for X-axis Galvo (fast axis, triangular waveform scan)
//============================================================================================================================================================================================
	
	
	uInt32 			nDeadTimePixels;						// Number of pixels at the beginning and end of each line where the motion of the galvo is not linear.
	uInt32			nPixelsPerLine;							// Total number of pixels per line including dead time pixels for galvo turn-around.
	uInt32			nGalvoSamplesPerLine;					// Total number of analog samples for the galvo command signal per line.
	double			fastAxisCommandAmplitude;				// Fast axis signal amplitude in [V].

	
	// number of line scan dead time pixels
	// note: deadTime in [ms] and pixelDwellTime in [us]
	nDeadTimePixels = (uInt32) ceil(((NonResGalvoCal_type*)scanEngine->baseClass.fastAxisCal)->triangleCal->deadTime * 1e3/scanEngine->scanSettings.pixelDwellTime);
	
	// number of pixels per line
	nPixelsPerLine = scanEngine->scanSettings.height + 2 * nDeadTimePixels;
	
	// number of galvo samples per line
	nGalvoSamplesPerLine = RoundRealToNearestInteger(nPixelsPerLine * scanEngine->scanSettings.pixelDwellTime * 1e-6 * scanEngine->galvoSamplingRate);
	
	// generate bidirectional raster scan signal (2 line scans, 1 triangle waveform period)
	fastAxisCommandAmplitude = nPixelsPerLine * scanEngine->scanSettings.pixSize / ((NonResGalvoCal_type*)scanEngine->baseClass.fastAxisCal)->sampleDisplacement;
	
	nullChk( fastAxisCommandSignal = malloc(2 * nGalvoSamplesPerLine * sizeof(double)) );
	double 		phase = -90;   
	errChk( TriangleWave(2 * nGalvoSamplesPerLine , fastAxisCommandAmplitude/2, 0.5/nGalvoSamplesPerLine , &phase, fastAxisCommandSignal) );
	double		fastAxisCommandOffset = scanEngine->scanSettings.heightOffset * scanEngine->scanSettings.pixSize / ((NonResGalvoCal_type*)scanEngine->baseClass.fastAxisCal)->sampleDisplacement; 
	for (size_t i = 0; i < 2 * nGalvoSamplesPerLine; i++)
		fastAxisCommandSignal[i] += fastAxisCommandOffset;
	
	// generate bidirectional raster scan waveform 
	nullChk( fastAxisScan_Waveform = init_Waveform_type(Waveform_Double, scanEngine->galvoSamplingRate, 2 * nGalvoSamplesPerLine, (void**)&fastAxisCommandSignal) );
	
	// generate fast axis fly-in waveform from parked position
	nullChk( fastAxisMoveFromParkedWaveform = NonResGalvoMoveBetweenPoints((NonResGalvoCal_type*)scanEngine->baseClass.fastAxisCal, scanEngine->galvoSamplingRate, 
	((NonResGalvoCal_type*)scanEngine->baseClass.fastAxisCal)->parked, 0, - fastAxisCommandAmplitude/2 + fastAxisCommandOffset, 0) );
	
	size_t		nGalvoSamplesFastAxisMoveFromParkedWaveform = GetWaveformNumSamples(fastAxisMoveFromParkedWaveform);
	
//============================================================================================================================================================================================
//                             						Preparation of Scan Waveforms for Y-axis Galvo (slow axis, staircase waveform scan)
//============================================================================================================================================================================================
	
	double				slowAxisAmplitude;						// Slow axis signal amplitude in [V].
	double				slowAxisStartVoltage;					// Slow axis staircase start voltage in [V].
	double				slowAxisStepVoltage;					// Slow axis staircase step voltage in [V]. 

	// generate staircase signal
	slowAxisStepVoltage  	= scanEngine->scanSettings.pixSize / ((NonResGalvoCal_type*)scanEngine->baseClass.slowAxisCal)->sampleDisplacement; 
	slowAxisAmplitude 		= (scanEngine->scanSettings.width - 1) * slowAxisStepVoltage;
	slowAxisStartVoltage 	= scanEngine->scanSettings.widthOffset * scanEngine->scanSettings.pixSize / ((NonResGalvoCal_type*)scanEngine->baseClass.slowAxisCal)->sampleDisplacement - slowAxisAmplitude/2;
	
	if (scanEngine->scanSettings.width > 1)
		// generate staircase signal
		nullChk( slowAxisScan_Waveform = StaircaseWaveform(TRUE, scanEngine->galvoSamplingRate, nGalvoSamplesPerLine, scanEngine->scanSettings.width, slowAxisStartVoltage, slowAxisStepVoltage) );  
	else {
		// generate constant signal (no movement)
		double* slowAxisSignal = NULL;
		nullChk( slowAxisSignal = malloc (2*nGalvoSamplesPerLine * sizeof(double)) );
		Set1D(slowAxisSignal, 2*nGalvoSamplesPerLine, scanEngine->scanSettings.widthOffset * scanEngine->scanSettings.pixSize / ((NonResGalvoCal_type*)scanEngine->baseClass.slowAxisCal)->sampleDisplacement);
		slowAxisScan_Waveform = init_Waveform_type(Waveform_Double, scanEngine->galvoSamplingRate, 2*nGalvoSamplesPerLine, (void**)&slowAxisSignal);
	}
	
	// generate slow axis fly-in waveform from parked position
	nullChk( slowAxisMoveFromParkedWaveform = NonResGalvoMoveBetweenPoints((NonResGalvoCal_type*)scanEngine->baseClass.slowAxisCal, scanEngine->galvoSamplingRate, 
	((NonResGalvoCal_type*)scanEngine->baseClass.slowAxisCal)->parked, 0, slowAxisStartVoltage, 0) ); 
	
	size_t		nGalvoSamplesSlowAxisScanWaveform 			= GetWaveformNumSamples(slowAxisScan_Waveform);    
	size_t		nGalvoSamplesSlowAxisMoveFromParkedWaveform = GetWaveformNumSamples(slowAxisMoveFromParkedWaveform);    
	
//============================================================================================================================================================================================
//                             						Compensate delay between X and Y galvo response and fly-in from parked position
//============================================================================================================================================================================================
	
	// fast scan axis response lag in terms of number of galvo samples 
	size_t				nGalvoSamplesFastAxisLag			= (size_t) floor(((NonResGalvoCal_type*)scanEngine->baseClass.fastAxisCal)->lag * 1e-3 * scanEngine->galvoSamplingRate);
	
	// slow scan axis response lag in terms of number of galvo samples  
	size_t				nGalvoSamplesSlowAxisLag			= (size_t) floor(((NonResGalvoCal_type*)scanEngine->baseClass.slowAxisCal)->lag * 1e-3 * scanEngine->galvoSamplingRate);
	
	// fast axis number of samples needed to move the galvo from the parked position to the start of the scan region
	size_t				nGalvoSamplesFastAxisMoveFromParked = GetWaveformNumSamples(fastAxisMoveFromParkedWaveform);
	
	// slow axis number of samples needed to move the galvo from the parked position to the start of the scan region
	size_t				nGalvoSamplesSlowAxisMoveFromParked = GetWaveformNumSamples(slowAxisMoveFromParkedWaveform);
	
	// total number of fly-in samples to move the galvos from their parked position to the start of the scan region
	size_t				nGalvoSamplesFlyIn					= 0;		
	
	// determine the minimum number of galvo samples required from start to reach the scan region for both the fast and slow scan axis
	if (nGalvoSamplesSlowAxisMoveFromParked + nGalvoSamplesSlowAxisLag > nGalvoSamplesFastAxisMoveFromParked + nGalvoSamplesFastAxisLag) 
		nGalvoSamplesFlyIn = nGalvoSamplesSlowAxisMoveFromParked + nGalvoSamplesSlowAxisLag;
	else
		nGalvoSamplesFlyIn = nGalvoSamplesFastAxisMoveFromParked + nGalvoSamplesFastAxisLag;  
	
	// make sure that the number of fly-in samples from start to scan region for both galvos is a non-zero integer multiple of the number of galvo samples per fast axis line scan
	nGalvoSamplesFlyIn = (nGalvoSamplesFlyIn/nGalvoSamplesPerLine + 1) * nGalvoSamplesPerLine;
	
	// update galvo fly-in duration in raster scan data structure
	scanEngine->flyInDelay = nGalvoSamplesFlyIn / scanEngine->galvoSamplingRate * 1e6;
	
	// determine number of additional galvo samples needed to compensate fast and slow axis fly-in and lag delays
	size_t				nGalvoSamplesFastAxisCompensation	=  nGalvoSamplesFlyIn - nGalvoSamplesFastAxisLag - nGalvoSamplesFastAxisMoveFromParked;
	size_t				nGalvoSamplesSlowAxisCompensation	=  nGalvoSamplesFlyIn - nGalvoSamplesSlowAxisLag - nGalvoSamplesSlowAxisMoveFromParked; 
	
	
	// generate compensated fast axis fly-in waveform from parked position
	if (nGalvoSamplesFastAxisCompensation) {
		nullChk( fastAxisCompensationSignal = malloc(nGalvoSamplesFastAxisCompensation * sizeof(double)) );
		errChk( Set1D(fastAxisCompensationSignal, nGalvoSamplesFastAxisCompensation, ((NonResGalvoCal_type*)scanEngine->baseClass.fastAxisCal)->parked) ); 
		nullChk( fastAxisMoveFromParkedCompensatedWaveform = init_Waveform_type(Waveform_Double, scanEngine->galvoSamplingRate, nGalvoSamplesFastAxisCompensation, (void**)&fastAxisCompensationSignal) );
		errChk( AppendWaveform(fastAxisMoveFromParkedCompensatedWaveform, fastAxisMoveFromParkedWaveform, &errMsg) );
		discard_Waveform_type(&fastAxisMoveFromParkedWaveform);
	}
	
	// generate compensated slow axis fly-in waveform from parked position
	if (nGalvoSamplesSlowAxisCompensation) {
		nullChk( slowAxisCompensationSignal = malloc(nGalvoSamplesSlowAxisCompensation * sizeof(double)) );
		errChk( Set1D(slowAxisCompensationSignal, nGalvoSamplesSlowAxisCompensation, ((NonResGalvoCal_type*)scanEngine->baseClass.slowAxisCal)->parked) ); 
		nullChk( slowAxisMoveFromParkedCompensatedWaveform = init_Waveform_type(Waveform_Double, scanEngine->galvoSamplingRate, nGalvoSamplesSlowAxisCompensation, (void**)&slowAxisCompensationSignal) );
		errChk( AppendWaveform(slowAxisMoveFromParkedCompensatedWaveform, slowAxisMoveFromParkedWaveform, &errMsg) );
		discard_Waveform_type(&slowAxisMoveFromParkedWaveform);
	}
	
	
//============================================================================================================================================================================================
//                             											Send command waveforms to the scan axes VChans
//============================================================================================================================================================================================

	//---------------------------------------------------------------------------------------------------------------------------
	// 													X-galvo fast scan axis
	//---------------------------------------------------------------------------------------------------------------------------
	
	// move from parked position
	// convert waveform to repeated waveform
	nullChk( fastAxisMoveFromParked_RepWaveform = ConvertWaveformToRepeatedWaveformType(&fastAxisMoveFromParkedCompensatedWaveform, 1) );
	
	// send data 
	nullChk( galvoCommandPacket = init_DataPacket_type(DL_RepeatedWaveform_Double, (void**)&fastAxisMoveFromParked_RepWaveform, NULL, (DiscardPacketDataFptr_type)discard_RepeatedWaveform_type) );
	errChk( SendDataPacket(scanEngine->baseClass.VChanFastAxisCom, &galvoCommandPacket, FALSE, &errMsg) );
	
	// fastAxisScan_Waveform has two line scans (one triangle wave period)
	if (GetTaskControlMode(scanEngine->baseClass.taskControl) == TASK_FINITE)
		// finite mode
		nullChk( fastAxisScan_RepWaveform  = ConvertWaveformToRepeatedWaveformType(&fastAxisScan_Waveform, scanEngine->scanSettings.width/2.0 * GetTaskControlIterations(scanEngine->baseClass.taskControl)) ); 
	else 
		// for continuous mode
		nullChk( fastAxisScan_RepWaveform  = ConvertWaveformToRepeatedWaveformType(&fastAxisScan_Waveform, 0) ); 
	
	// send data
	nullChk( galvoCommandPacket = init_DataPacket_type(DL_RepeatedWaveform_Double, (void**)&fastAxisScan_RepWaveform, NULL, (DiscardPacketDataFptr_type)discard_RepeatedWaveform_type) );   
	errChk( SendDataPacket(scanEngine->baseClass.VChanFastAxisCom, &galvoCommandPacket, FALSE, &errMsg) );    
	
	// go back to parked position if finite frame scan mode
	if (GetTaskControlMode(scanEngine->baseClass.taskControl) == TASK_FINITE) {
		// generate one sample
		nullChk( parkedVoltageSignal = malloc(sizeof(double)) );
		*parkedVoltageSignal = ((NonResGalvoCal_type*)scanEngine->baseClass.fastAxisCal)->parked;
		RepeatedWaveform_type*	parkedRepeatedWaveform = init_RepeatedWaveform_type(RepeatedWaveform_Double, scanEngine->galvoSamplingRate, 1, (void**)&parkedVoltageSignal, 0);
		nullChk( galvoCommandPacket = init_DataPacket_type(DL_RepeatedWaveform_Double, (void**)&parkedRepeatedWaveform, NULL, (DiscardPacketDataFptr_type)discard_RepeatedWaveform_type) ); 
		errChk( SendDataPacket(scanEngine->baseClass.VChanFastAxisCom, &galvoCommandPacket, FALSE, &errMsg) );    
		// send NULL packet to signal termination of data stream
		errChk( SendDataPacket(scanEngine->baseClass.VChanFastAxisCom, &nullPacket, FALSE, &errMsg) );    
	}
	
	// send number of samples in fast axis command waveform if scan is finite
	if (GetTaskControlMode(scanEngine->baseClass.taskControl) == TASK_FINITE) {
		nullChk( nGalvoSamplesPtr = malloc(sizeof(uInt64)) );
		// move from parked waveform + scan waveform + one sample to return to parked position
		*nGalvoSamplesPtr = (uInt64)((nGalvoSamplesFastAxisCompensation + nGalvoSamplesFastAxisMoveFromParkedWaveform) + 
					   (scanEngine->scanSettings.width/2.0 * GetTaskControlIterations(scanEngine->baseClass.taskControl)) * 2 * nGalvoSamplesPerLine + 1);
		nullChk( galvoCommandPacket = init_DataPacket_type(DL_ULongLong, (void**)&nGalvoSamplesPtr, NULL, NULL) );
		errChk( SendDataPacket(scanEngine->baseClass.VChanFastAxisComNSamples, &galvoCommandPacket, FALSE, &errMsg) );    
	}
	
	//---------------------------------------------------------------------------------------------------------------------------
	// 													Y-galvo slow scan axis
	//---------------------------------------------------------------------------------------------------------------------------
	
	// move from parked position
	// convert waveform to repeated waveform
	nullChk( slowAxisMoveFromParked_RepWaveform = ConvertWaveformToRepeatedWaveformType(&slowAxisMoveFromParkedCompensatedWaveform, 1) );
	
	// send data 
	nullChk( galvoCommandPacket = init_DataPacket_type(DL_RepeatedWaveform_Double, (void**)&slowAxisMoveFromParked_RepWaveform, NULL, (DiscardPacketDataFptr_type)discard_RepeatedWaveform_type) );
	errChk( SendDataPacket(scanEngine->baseClass.VChanSlowAxisCom, &galvoCommandPacket, FALSE, &errMsg) );
	
	// slowAxisScan_Waveform has a symmetric staircase waveform (equivalent to two scan frames)
	if (GetTaskControlMode(scanEngine->baseClass.taskControl) == TASK_FINITE)
		// finite mode
		nullChk( slowAxisScan_RepWaveform  = ConvertWaveformToRepeatedWaveformType(&slowAxisScan_Waveform, 0.5 * GetTaskControlIterations(scanEngine->baseClass.taskControl)) ); 
	else 
		// for continuous mode
		nullChk( slowAxisScan_RepWaveform  = ConvertWaveformToRepeatedWaveformType(&slowAxisScan_Waveform, 0) ); 
	
	// send data
	nullChk( galvoCommandPacket = init_DataPacket_type(DL_RepeatedWaveform_Double, (void**)&slowAxisScan_RepWaveform, NULL, (DiscardPacketDataFptr_type)discard_RepeatedWaveform_type) );   
	errChk( SendDataPacket(scanEngine->baseClass.VChanSlowAxisCom, &galvoCommandPacket, FALSE, &errMsg) );  
	
	// go back to parked position if finite frame scan mode
	if (GetTaskControlMode(scanEngine->baseClass.taskControl) == TASK_FINITE) {
		// generate one sample
		nullChk( parkedVoltageSignal = malloc(sizeof(double)) );
		*parkedVoltageSignal = ((NonResGalvoCal_type*)scanEngine->baseClass.slowAxisCal)->parked;
		RepeatedWaveform_type*	parkedRepeatedWaveform = init_RepeatedWaveform_type(RepeatedWaveform_Double, scanEngine->galvoSamplingRate, 1, (void**)&parkedVoltageSignal, 0);
		nullChk( galvoCommandPacket = init_DataPacket_type(DL_RepeatedWaveform_Double, (void**)&parkedRepeatedWaveform, NULL, (DiscardPacketDataFptr_type)discard_RepeatedWaveform_type) ); 
		errChk( SendDataPacket(scanEngine->baseClass.VChanSlowAxisCom, &galvoCommandPacket, FALSE, &errMsg) );
		// send NULL packet to signal termination of data stream
		errChk( SendDataPacket(scanEngine->baseClass.VChanSlowAxisCom, &nullPacket, FALSE, &errMsg) );    
	}
	
	// send number of samples in slow axis command waveform if scan is finite
	if (GetTaskControlMode(scanEngine->baseClass.taskControl) == TASK_FINITE) {
		nGalvoSamplesPtr = malloc(sizeof(uInt64));
		// move from parked waveform + scan waveform + one sample to return to parked position
		*nGalvoSamplesPtr = (uInt64) ((nGalvoSamplesSlowAxisCompensation + nGalvoSamplesSlowAxisMoveFromParkedWaveform) + 
					    GetTaskControlIterations(scanEngine->baseClass.taskControl) * 0.5 * nGalvoSamplesSlowAxisScanWaveform + 1);
		nullChk( galvoCommandPacket = init_DataPacket_type(DL_ULongLong, (void**)&nGalvoSamplesPtr, NULL, NULL) );
		errChk( SendDataPacket(scanEngine->baseClass.VChanSlowAxisComNSamples, &galvoCommandPacket, FALSE, &errMsg) );    
	}
	
	//---------------------------------------------------------------------------------------------------------------------------
	// 													Pixel timing info
	//---------------------------------------------------------------------------------------------------------------------------
	
	
	if (GetTaskControlMode(scanEngine->baseClass.taskControl) == TASK_FINITE) {
		//--------------------
		// finite mode
		//--------------------
		// total number of pixels
		uInt64	nPixels = (uInt64)((scanEngine->flyInDelay + scanEngine->baseClass.pixDelay)/scanEngine->scanSettings.pixelDwellTime) + (uInt64)nPixelsPerLine * (uInt64)scanEngine->scanSettings.width * (uInt64)GetTaskControlIterations(scanEngine->baseClass.taskControl); 
		nullChk( pixelPulseTrain = (PulseTrain_type*) init_PulseTrainTickTiming_type(PulseTrain_Finite, PulseTrainIdle_Low, nPixels, (uInt32)(scanEngine->scanSettings.pixelDwellTime * 1e-6 * scanEngine->baseClass.pixelClockRate) - 2, 2, 0) );
		
		// send n pixels
		nullChk( nPixelsPtr = malloc(sizeof(uInt64)) );
		*nPixelsPtr = nPixels;
		nullChk( nPixelsPacket = init_DataPacket_type(DL_ULongLong, (void**)&nPixelsPtr, NULL, NULL) );
		errChk( SendDataPacket(scanEngine->baseClass.VChanNPixels, &nPixelsPacket, FALSE, &errMsg) );    
	}
	else 
		//--------------------
		// continuous mode
		//--------------------
		nullChk( pixelPulseTrain = (PulseTrain_type*) init_PulseTrainTickTiming_type(PulseTrain_Continuous, PulseTrainIdle_Low, 0, (uInt32)(scanEngine->scanSettings.pixelDwellTime * 1e-6 * scanEngine->baseClass.pixelClockRate) - 2, 2, 0) );
	
	// send pulse train info
	nullChk( pixelPulseTrainPacket = init_DataPacket_type(DL_PulseTrain_Ticks, (void**)&pixelPulseTrain, NULL, (DiscardPacketDataFptr_type)discard_PulseTrain_type) );
	errChk( SendDataPacket(scanEngine->baseClass.VChanPixelPulseTrain, &pixelPulseTrainPacket, FALSE, &errMsg) ); 
	
	return 0; // no error
				  
Error:
	
	OKfree(fastAxisCommandSignal);
	OKfree(fastAxisCompensationSignal);
	OKfree(slowAxisCompensationSignal);
	OKfree(parkedVoltageSignal);
	OKfree(nGalvoSamplesPtr);
	discard_Waveform_type(&fastAxisScan_Waveform);
	discard_Waveform_type(&fastAxisMoveFromParkedWaveform);
	discard_Waveform_type(&fastAxisMoveFromParkedCompensatedWaveform); 
	discard_Waveform_type(&slowAxisScan_Waveform);
	discard_Waveform_type(&slowAxisMoveFromParkedWaveform);
	discard_Waveform_type(&slowAxisMoveFromParkedCompensatedWaveform);
	discard_RepeatedWaveform_type(&fastAxisMoveFromParked_RepWaveform);
	discard_RepeatedWaveform_type(&slowAxisMoveFromParked_RepWaveform); 
	discard_RepeatedWaveform_type(&fastAxisScan_RepWaveform);
	discard_RepeatedWaveform_type(&slowAxisScan_RepWaveform);
	discard_PulseTrain_type(&pixelPulseTrain);
	ReleaseDataPacket(&galvoCommandPacket);
	ReleaseDataPacket(&pixelPulseTrainPacket);
	ReleaseDataPacket(&nPixelsPacket);
	
	*errorInfo = FormatMsg(NonResRectRasterScan_GenerateScanSignals_Err_ScanSignals, "NonResRectRasterScan_GenerateScanSignals", errMsg);
	OKfree(errMsg);
	
	return NonResRectRasterScan_GenerateScanSignals_Err_ScanSignals;
}

/* 

PURPOSE
----------------

Constructs an image from multiple data blocks of smaller size. When the image is ready, it is sent as a data packet to the source VChanName[]. 

Suppose there are N different data channels, with data coming from e.g. photomultipliers. The data from each channel must be then assembled in N different images. 
As more data comes in, in certain packet sizes, when enough data has been gathered each time an image is assembled and sent to a certain named source VChan.
Finally, the source VChan is linked to one or more sink VChans, which will receive the assembled images.

USAGE
----------------

When using the function the first time, by specifying a VChanName, the function will create an internal buffer which will be used to gather the pixel data.
If there are more channels to be used, then by calling this function with different VChanName[] the function will create separate buffers for each channel.

For each image call the function with a specified image width and height. The image width and heights can be different for each channel as they are processed independently.

The data from one channel is given as {void* pixarray, size_t npix} where pixarray points to a valid ImageType imaqimgtype accepted by NI-Vision that specified data for one channel.

The size of the pixarray may be different between channels and also between different calls to this function.

The incoming image data stream for each channel is assumed to be of the form:

sssssss|dduuuuuuuuuuuuuudd|dduuuuuuuuuuuuuudd|dduuuuuuuuuuuuuudd|dduuuuuuuuuuuuuudd|dduuuuuuuuuuuuuudd|dduuuuuuuuuuuuuudd|dduuuuuuuuuuuuuudd|dduuuuuuuuuuuuuudd| ...
																																						
^		^   ^								 ^				    ^      											         ^
|		|   |								 |					|												         |
|       |   																															
|       |   u = useful pixels = height		 beginning of line	end of line 									   	     new frame starts here 
|		  																											   
|		d = deadtimepix

s = skipinitialpix

The width of the image means how many separate useful pixel regions are in the stream.

By giving a VChanName[] string that is already in the buffer and setting pixarray to NULL, the buffer for the specified VChanName is emptied.
By giving an empty string VChanName[] like "" the function emties all buffers.

The function returns 0 on successfully adding data to the buffers and sending it to the specified VChanName[] when an image is available.
The function returns -1 if error occurs and more detailed error info is sent to the UI.

WARNING 
----------------

The function is not multi-threaded.

*/

static int NonResRectRasterScan_BuildImage (RectRaster_type* rectRaster, size_t imgBufferIdx, char** errorInfo)
{
#define NonResRectRasterScan_BuildImage_Err_WrongPixelDataType	-1 
	int							error				= 0;
	char*						errMsg				= NULL;
	DataPacket_type*			pixelPacket			= NULL;
	DLDataTypes					pixelDataType		= 0; 		// Data type of incoming pixel waveform
	ImageTypes					imageType			= 0;		// Data type of assembled image
	Waveform_type*				pixelWaveform		= NULL;   	// Incoming pixel waveform.
	void*						pixelData			= NULL;		// Array of received pixels from a single pixel waveform data packet.
	size_t						nPixels				= 0;		// Number of received pixels from a pixel data packet stored in pixelData.
	size_t             			pixelDataIdx   		= 0;      	// The index of the processed pixel from the received pixel waveform.
	RectRasterImgBuffer_type*   imgBuffer			= rectRaster->imgBuffers[imgBufferIdx];
	size_t						nDeadTimePixels		= (size_t) ceil(((NonResGalvoCal_type*)rectRaster->baseClass.fastAxisCal)->triangleCal->deadTime * 1e3 / rectRaster->scanSettings.pixelDwellTime);
	
	size_t 						iterindex			= 0;
	Iterator_type* 				currentiter		    = NULL;
	DisplayEngine_type* 		displayEngine 		= imgBuffer->detChan->scanEngine->lsModule->displayEngine;
	RectRasterScanSet_type*		imgSettings			= NULL;
	
	do {
		
		//----------------------------------------------------------------------
		// Receive pixel data
		//----------------------------------------------------------------------
		
		errChk( GetDataPacket(imgBuffer->detChan->detVChan, &pixelPacket, &errMsg) );
		
		//----------------------------------------------------------------------
		// Process NULL packet
		//----------------------------------------------------------------------
		
		// TEMPORARY for one channel only
		// end task controller iteration
		if (!pixelPacket) {
			//TaskControlEvent(rectRaster->baseClass.taskControl, TASK_EVENT_STOP, NULL, NULL);
			//TaskControlIterationDone(rectRaster->baseClass.taskControl, 0, "", FALSE);
			break;
		}
			
		
		//----------------------------------------------------------------------
		// Add data to the buffer
		//----------------------------------------------------------------------
		
		// get pointer to pixel array
		pixelWaveform = *(Waveform_type**) GetDataPacketPtrToData(pixelPacket, &pixelDataType);
		pixelData = *(void**) GetWaveformPtrToData(pixelWaveform, &nPixels);
			
		// skip initial pixels if needed. Here pixelDataIdx is the index of the pixel within the received
		// pixelData from which data must be processed up to nPixels
		if (imgBuffer->nSkipPixels)
			if (nPixels > imgBuffer->nSkipPixels) {
				pixelDataIdx = imgBuffer->nSkipPixels; 
				imgBuffer->nSkipPixels = 0;
			} else {
				imgBuffer->nSkipPixels -= nPixels;
				ReleaseDataPacket(&pixelPacket);
				continue; // get another pixel data packet
			}
		else 
			pixelDataIdx = 0;
			
		// add pixels to the temporary buffer depending on the image data type
		switch (pixelDataType) {
			
			case DL_Waveform_UChar:
				
				nullChk( imgBuffer->tmpPixels = realloc(imgBuffer->tmpPixels, (imgBuffer->nTmpPixels + nPixels - pixelDataIdx) * sizeof(unsigned char)) );
				memcpy((unsigned char*)imgBuffer->tmpPixels + imgBuffer->nTmpPixels, (unsigned char*)pixelData + pixelDataIdx, (nPixels - pixelDataIdx) * sizeof(unsigned char));
				break;
				
			case DL_Waveform_UShort:
				
				nullChk( imgBuffer->tmpPixels = realloc(imgBuffer->tmpPixels, (imgBuffer->nTmpPixels + nPixels - pixelDataIdx) * sizeof(unsigned short)) );
				memcpy((unsigned short*)imgBuffer->tmpPixels + imgBuffer->nTmpPixels, (unsigned short*)pixelData + pixelDataIdx, (nPixels - pixelDataIdx) * sizeof(unsigned short));
				break;
				
			case DL_Waveform_Short:
				
				nullChk( imgBuffer->tmpPixels = realloc(imgBuffer->tmpPixels, (imgBuffer->nTmpPixels + nPixels - pixelDataIdx) * sizeof(short)) );
				memcpy((short*)imgBuffer->tmpPixels + imgBuffer->nTmpPixels, (short*)pixelData + pixelDataIdx, (nPixels - pixelDataIdx) * sizeof(short));
				break;
				
			case DL_Waveform_Float:
					
				nullChk( imgBuffer->tmpPixels = realloc(imgBuffer->tmpPixels, (imgBuffer->nTmpPixels + nPixels - pixelDataIdx) * sizeof(float)) );
				memcpy((float*)imgBuffer->tmpPixels + imgBuffer->nTmpPixels, (float*)pixelData + pixelDataIdx, (nPixels - pixelDataIdx) * sizeof(float));
				break;
				
			default:
				
				error = NonResRectRasterScan_BuildImage_Err_WrongPixelDataType;
				errMsg = StrDup("Wrong pixel data type");
				goto Error;
		}
		
		ReleaseDataPacket(&pixelPacket);
			
		// update number of pixels in temporary buffer
		imgBuffer->nTmpPixels += nPixels - pixelDataIdx;
		
		// if there are enough pixels to construct a row take them out of the buffer
		while (imgBuffer->nTmpPixels >= rectRaster->scanSettings.height + 2 * nDeadTimePixels) {
				
			switch (pixelDataType) {   
					
				case DL_Waveform_UChar:
				
					// copy pixels depending on their direction
					if (imgBuffer->revWidthFlag){
						if (!imgBuffer->revHeightFlag)
							memcpy((unsigned char*)imgBuffer->imagePixels + imgBuffer->nImagePixels, (unsigned char*)imgBuffer->tmpPixels + nDeadTimePixels, rectRaster->scanSettings.height * sizeof(unsigned char));
						else 
							for (uInt32 i = 0; i < rectRaster->scanSettings.height; i++)
								*((unsigned char*)imgBuffer->imagePixels + imgBuffer->nImagePixels + i) = *((unsigned char*)imgBuffer->tmpPixels + nDeadTimePixels + rectRaster->scanSettings.height - 1 - i);
					} else {
						if (!imgBuffer->revHeightFlag)
							memcpy((unsigned char*)imgBuffer->imagePixels + (rectRaster->scanSettings.height * rectRaster->scanSettings.width - imgBuffer->nImagePixels - rectRaster->scanSettings.height), (unsigned char*)imgBuffer->tmpPixels + nDeadTimePixels, rectRaster->scanSettings.height * sizeof(unsigned char));
						else 
							for (uInt32 i = 0; i < rectRaster->scanSettings.height; i++)
								*((unsigned char*)imgBuffer->imagePixels + (rectRaster->scanSettings.height * rectRaster->scanSettings.width - imgBuffer->nImagePixels - rectRaster->scanSettings.height) + i) = *((unsigned char*)imgBuffer->tmpPixels + nDeadTimePixels + rectRaster->scanSettings.height - 1 - i);
					} 
						
					// move contents of the buffer to its beginning
					memmove((unsigned char*)imgBuffer->tmpPixels, (unsigned char*)imgBuffer->tmpPixels + 2 * nDeadTimePixels + rectRaster->scanSettings.height, (imgBuffer->nTmpPixels - 2 * nDeadTimePixels - rectRaster->scanSettings.height) * sizeof(unsigned char));
					break;
					
				case DL_Waveform_UShort:
						
					// copy pixels depending on their direction
					if (imgBuffer->revWidthFlag){
						if (!imgBuffer->revHeightFlag)
							memcpy((unsigned short*)imgBuffer->imagePixels + imgBuffer->nImagePixels, (unsigned short*)imgBuffer->tmpPixels + nDeadTimePixels, rectRaster->scanSettings.height * sizeof(unsigned short));
						else 
							for (uInt32 i = 0; i < rectRaster->scanSettings.height; i++)
								*((unsigned short*)imgBuffer->imagePixels + imgBuffer->nImagePixels + i) = *((unsigned short*)imgBuffer->tmpPixels + nDeadTimePixels + rectRaster->scanSettings.height - 1 - i);
					} else {
						if (!imgBuffer->revHeightFlag)
							memcpy((unsigned short*)imgBuffer->imagePixels + (rectRaster->scanSettings.height * rectRaster->scanSettings.width - imgBuffer->nImagePixels - rectRaster->scanSettings.height), (unsigned short*)imgBuffer->tmpPixels + nDeadTimePixels, rectRaster->scanSettings.height * sizeof(unsigned short));
						else 
							for (uInt32 i = 0; i < rectRaster->scanSettings.height; i++)
								*((unsigned short*)imgBuffer->imagePixels + (rectRaster->scanSettings.height * rectRaster->scanSettings.width - imgBuffer->nImagePixels - rectRaster->scanSettings.height) + i) = *((unsigned short*)imgBuffer->tmpPixels + nDeadTimePixels + rectRaster->scanSettings.height - 1 - i);
					} 
						
					// move contents of the buffer to its beginning
					memmove((unsigned short*)imgBuffer->tmpPixels, (unsigned short*)imgBuffer->tmpPixels + 2 * nDeadTimePixels + rectRaster->scanSettings.height, (imgBuffer->nTmpPixels - 2 * nDeadTimePixels - rectRaster->scanSettings.height) * sizeof(unsigned short));
					break;
						
				case DL_Waveform_Short:
						
					// copy pixels depending on their direction
					if (imgBuffer->revWidthFlag){
						if (!imgBuffer->revHeightFlag)
							memcpy((short*)imgBuffer->imagePixels + imgBuffer->nImagePixels, (short*)imgBuffer->tmpPixels + nDeadTimePixels, rectRaster->scanSettings.height * sizeof(short));
						else 
							for (uInt32 i = 0; i < rectRaster->scanSettings.height; i++)
								*((short*)imgBuffer->imagePixels + imgBuffer->nImagePixels + i) = *((short*)imgBuffer->tmpPixels + nDeadTimePixels + rectRaster->scanSettings.height - 1 - i);
					} else {
						if (!imgBuffer->revHeightFlag)
							memcpy((short*)imgBuffer->imagePixels + (rectRaster->scanSettings.height * rectRaster->scanSettings.width - imgBuffer->nImagePixels - rectRaster->scanSettings.height), (short*)imgBuffer->tmpPixels + nDeadTimePixels, rectRaster->scanSettings.height * sizeof(short));
						else 
							for (uInt32 i = 0; i < rectRaster->scanSettings.height; i++)
								*((short*)imgBuffer->imagePixels + (rectRaster->scanSettings.height * rectRaster->scanSettings.width - imgBuffer->nImagePixels - rectRaster->scanSettings.height) + i) = *((short*)imgBuffer->tmpPixels + nDeadTimePixels + rectRaster->scanSettings.height - 1 - i);
					} 
						
					// move contents of the buffer to its beginning
					memmove((short*)imgBuffer->tmpPixels, (short*)imgBuffer->tmpPixels + 2 * nDeadTimePixels + rectRaster->scanSettings.height, (imgBuffer->nTmpPixels - 2 * nDeadTimePixels - rectRaster->scanSettings.height) * sizeof(short));
					break;
						
				case DL_Waveform_Float:
					
					// copy pixels depending on their direction
					if (imgBuffer->revWidthFlag){
						if (!imgBuffer->revHeightFlag)
							memcpy((float*)imgBuffer->imagePixels + imgBuffer->nImagePixels, (float*)imgBuffer->tmpPixels + nDeadTimePixels, rectRaster->scanSettings.height * sizeof(float));
						else 
							for (uInt32 i = 0; i < rectRaster->scanSettings.height; i++)
								*((float*)imgBuffer->imagePixels + imgBuffer->nImagePixels + i) = *((float*)imgBuffer->tmpPixels + nDeadTimePixels + rectRaster->scanSettings.height - 1 - i);
					} else {
						if (!imgBuffer->revHeightFlag)
							memcpy((float*)imgBuffer->imagePixels + (rectRaster->scanSettings.height * rectRaster->scanSettings.width - imgBuffer->nImagePixels - rectRaster->scanSettings.height), (float*)imgBuffer->tmpPixels + nDeadTimePixels, rectRaster->scanSettings.height * sizeof(float));
						else 
							for (uInt32 i = 0; i < rectRaster->scanSettings.height; i++)
								*((float*)imgBuffer->imagePixels + (rectRaster->scanSettings.height * rectRaster->scanSettings.width - imgBuffer->nImagePixels - rectRaster->scanSettings.height) + i) = *((float*)imgBuffer->tmpPixels + nDeadTimePixels + rectRaster->scanSettings.height - 1 - i);
					} 
						
					// move contents of the buffer to its beginning
					memmove((float*)imgBuffer->tmpPixels, (float*)imgBuffer->tmpPixels + 2 * nDeadTimePixels + rectRaster->scanSettings.height, (imgBuffer->nTmpPixels - 2 * nDeadTimePixels - rectRaster->scanSettings.height) * sizeof(float));
					break;
					
				default:
				
				error = NonResRectRasterScan_BuildImage_Err_WrongPixelDataType;
				errMsg = StrDup("Wrong pixel data type");
				goto Error;
						
			}
				
			// update number of pixels in the buffers
			imgBuffer->nTmpPixels  	-= rectRaster->scanSettings.height + 2 * nDeadTimePixels;
			imgBuffer->nImagePixels	+= rectRaster->scanSettings.height;
			// increment number of columns
			imgBuffer->nAssembledColumns++;
			// reverse pixel direction of every second column
			imgBuffer->revHeightFlag = !imgBuffer->revHeightFlag;
				
			if (imgBuffer->nAssembledColumns == rectRaster->scanSettings.width) {
				
			
				switch (pixelDataType) {
				
					case DL_Waveform_UChar:
						imageType = Image_UChar;  
						break;
		
					case DL_Waveform_UShort:
						imageType = Image_UShort;    
						break;
			
					case DL_Waveform_Short:
						imageType = Image_Short;    
						break;
			
					case DL_Waveform_Float:
						imageType = Image_Float;     
						break;
			
					default:
						error = NonResRectRasterScan_BuildImage_Err_WrongPixelDataType;
						errMsg = StrDup("Wrong pixel data type");
						goto Error;
				}
				
				// add restore image settings callback info
				nullChk( imgSettings = init_RectRasterScanSet_type(rectRaster->scanSettings.pixSize, rectRaster->scanSettings.height, rectRaster->scanSettings.heightOffset, rectRaster->scanSettings.width,
													 rectRaster->scanSettings.widthOffset, rectRaster->scanSettings.pixelDwellTime) );
				RestoreImgSettings_CBFptr_type	restoreCBFns[] 					= {RestoreImgSettings_CB};
				void* 							callbackData[]					= {imgSettings};
				DiscardFptr_type 				discardCallbackDataFunctions[] 	= {(DiscardFptr_type)discard_RectRasterScanSet_type};
				
				errChk( (*displayEngine->setRestoreImgSettingsFptr) (imgBuffer->detChan->displayHndl, NumElem(restoreCBFns), restoreCBFns, callbackData, discardCallbackDataFunctions) );
				
				// send pixel array to display 
				errChk( (*displayEngine->displayImageFptr) (imgBuffer->detChan->displayHndl, imgBuffer->imagePixels, rectRaster->scanSettings.width, rectRaster->scanSettings.height, imageType) );
				 
				
				  /*
					//test lex 
				//create image packet
				switch (pixelDataType) {
					case DL_Waveform_UChar:
						imaqImgType 		= IMAQ_IMAGE_U8;
					break;
		
					case DL_Waveform_UShort:
						imaqImgType 		= IMAQ_IMAGE_U16;
					break;
					case DL_Waveform_Short:
						imaqImgType 		= IMAQ_IMAGE_I16; 
					break;
				   	case DL_Waveform_Float:
						imaqImgType 		= IMAQ_IMAGE_SGL; 
					break;
				}
					sendImage=imaqCreateImage(imaqImgType, 0); 
					imaqDuplicate(sendImage,imgBuffer->detChan->imaqImg); 
					
					currentiter=GetTaskControlCurrentIterDup(rectRaster->baseClass.taskControl);
					//test
					iterindex=GetCurrentIterationIndex(currentiter);
					iterindex++;
					SetCurrentIterationIndex(currentiter,iterindex);
					nullChk( imagePacket	= init_DataPacket_type(DL_Image_NIVision, &sendImage,currentiter , (DiscardPacketDataFptr_type) discard_Image_type));       
					// send data packet with image
					errChk( SendDataPacket(rectRaster->baseClass.VChanScanOut, &imagePacket, 0, &errMsg) );
				  */
			
			
				// TEMPORARY: just complete iteration, and use only one channel
				TaskControlIterationDone(rectRaster->baseClass.taskControl, 0, "", FALSE);
				
			
				
				// reset number of elements in pixdata buffer (contents is not cleared!) new frame data will overwrite the old frame data in the buffer
				// NOTE & WARNING: data in the imgBuffer->tmpPixels is kept since multiple images can follow and imgBuffer->tmpPixels may contain data from the next image
				imgBuffer->nImagePixels = 0;
				// reset column counter
				imgBuffer->nAssembledColumns = 0;
				// reverse direction of every second image
				imgBuffer->revWidthFlag = !imgBuffer->revWidthFlag;
				return 0;
			}
			
			
		} // end of while loop, all available lines in tmpdata have been processed into pixdata
		
	} while (TRUE);
			
	return 0;

Error:
	
	// cleanup
	ReleaseDataPacket(&pixelPacket);
	discard_RectRasterScanSet_type(&imgSettings);
	
	if (!errMsg)
		errMsg = StrDup("Out of memory");
	
	if (errorInfo)
		*errorInfo = FormatMsg(error, "NonResRectRasterScan_BuildImage", errMsg);
	OKfree(errMsg);
	
	return error;
}

/*
void CVICALLBACK DisplayClose_CB (int menuBar, int menuItem, void *callbackData, int panel)
{
	DetChan_type*	detChan = callbackData;
	
	ImageControl_SetAttribute(panel, DisplayPan_Image, ATTR_IMAGECTRL_IMAGE, NULL);
	HidePanel(panel);
}
*/

static void	GetScanAxisTypes (ScanEngine_type* scanEngine, ScanAxis_type* fastAxisType, ScanAxis_type* slowAxisType)
{
	switch (scanEngine->engineType) {
			
		case ScanEngine_RectRaster_NonResonantGalvoFastAxis_NonResonantGalvoSlowAxis:
			
			*fastAxisType = NonResonantGalvo;
			*slowAxisType = NonResonantGalvo;
			break;
	}
}

static Objective_type* init_Objective_type (char objectiveName[], double objectiveFL)
{
	Objective_type*		objective = malloc(sizeof(Objective_type));
	if (!objective) return NULL;
	
	objective->objectiveName 	= StrDup(objectiveName);
	objective->objectiveFL		= objectiveFL;
	
	return objective;
}

static void discard_Objective_type (Objective_type** objective)
{
	if (!*objective) return;
	
	OKfree((*objective)->objectiveName);
	OKfree(*objective);
}

static int CVICALLBACK NewObjective_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ScanEngine_type*	engine				= callbackData;
	char*				newObjectiveName	= NULL;
	
	if (event != EVENT_COMMIT) return 0; // do nothing if event is not commit
	
	switch (control) {
			
		case NewObjPan_OK:
			
			newObjectiveName = GetStringFromControl(panel, NewObjPan_Name);
			
			// do nothing if there is no name
			if (!newObjectiveName[0]) {
				OKfree(newObjectiveName);
				return 0;
			}
			
			// make sure name is unique
			size_t				nObjectives			= ListNumItems(engine->objectives);
			BOOL				uniqueFlag			= TRUE;
			Objective_type**	objectivePtr;
			for (size_t i = 1; i <= nObjectives; i++) {
				objectivePtr = ListGetPtrToItem(engine->objectives, i);
				if (!strcmp(newObjectiveName, (*objectivePtr)->objectiveName)) {
					uniqueFlag = FALSE;
					break;
				}
			}
			
			// stop here if objective name already exists
			if (!uniqueFlag) {
				OKfree(newObjectiveName);
				SetCtrlVal(panel, NewObjPan_Name, ""); 
				return 0;
			}
			
			// add objective to scan engine structure
			double				objectiveFL;
			GetCtrlVal(panel, NewObjPan_ObjectiveLensFL, &objectiveFL);
			Objective_type*		objective = init_Objective_type(newObjectiveName, objectiveFL);
			
			ListInsertItem(engine->objectives, &objective, END_OF_LIST);
			
			// add FL info to objective name
			char	objFLString[30];
			char*	extendedObjName = StrDup(newObjectiveName);
			Fmt(objFLString, "%s< (%f[p3] mm FL)", objectiveFL);
			AppendString(&extendedObjName, objFLString, -1);
				
			// update engine settings UI
			if (engine->engineSetPanHndl)
				InsertListItem(engine->engineSetPanHndl, ScanSetPan_Objectives, -1, extendedObjName, 0); 
			OKfree(extendedObjName);
			
			// update rectangular raster scan UI
			if (engine->scanSetPanHndl) {
				// if there were no objectives previously, then assign this FL by default
				if (!nObjectives)
					engine->objectiveLens = objective;
				
				// update both scan axes
				if (engine->fastAxisCal)
						(*engine->fastAxisCal->UpdateOptics) (engine->fastAxisCal);
					
				if (engine->slowAxisCal)
						(*engine->slowAxisCal->UpdateOptics) (engine->slowAxisCal);
				
				// inserts new objective and selects it if there were no other objectives previously
				InsertListItem(engine->scanSetPanHndl, RectRaster_Objective, -1, newObjectiveName, objectiveFL);
			}
			OKfree(newObjectiveName);
			
			// configure/unconfigure scan engine
			if (NonResRectRasterScan_ValidConfig((RectRaster_type*)engine)) {
				
				NonResRectRasterScan_ScanHeights((RectRaster_type*)engine);
				NonResRectRasterScan_PixelDwellTimes((RectRaster_type*)engine);
					
				if (NonResRectRasterScan_ReadyToScan((RectRaster_type*)engine))
					TaskControlEvent(engine->taskControl, TASK_EVENT_CONFIGURE, NULL, NULL);
				else
					TaskControlEvent(engine->taskControl, TASK_EVENT_UNCONFIGURE, NULL, NULL); 	
			} else
				TaskControlEvent(engine->taskControl, TASK_EVENT_UNCONFIGURE, NULL, NULL);
			
			// cleanup, discard panel
			DiscardPanel(engine->newObjectivePanHndl);
			engine->newObjectivePanHndl = 0;
			break;
			
		case NewObjPan_Cancel:
			
			DiscardPanel(engine->newObjectivePanHndl);
			engine->newObjectivePanHndl = 0;
			break;
	}
	
	return 0;
}

/*
Generates a staircase waveform that either goes up or up and down like this:
        	 __			 ____
		  __|  		  __|    |__
	   __|		   __|          |__
	__|			__|                |__

sampleRate 			= Sampling rate in [Hz].
nSamplesPerStep		= Number of samples per staircase step.
nSteps				= Number of steps in the staircase.
startVoltage		= Start voltage at the bottom of the staircase [V].
stepVoltage    		= Increase in voltage with each step in [V], except for the symmetric staircase where at the peak are two consecutive lines at the same voltage.
*/
static Waveform_type* StaircaseWaveform (BOOL symmetricStaircase, double sampleRate, size_t nSamplesPerStep, size_t nSteps, double startVoltage, double stepVoltage)
{
	double*				waveformData	= NULL;
	size_t				nSamples		= 0;
    
	if (symmetricStaircase)
		nSamples = 2 * nSteps * nSamplesPerStep; 
	else 
		nSamples = nSteps * nSamplesPerStep;    
		
    waveformData = malloc(nSamples * sizeof(double));
    if (!waveformData) goto Error;

    // build first half of the staircase
    for (size_t i = 0; i < nSteps; i++)
        for (size_t j = 0; j < nSamplesPerStep; j++)
                waveformData[i*nSamplesPerStep+j] = startVoltage + stepVoltage*i;
	
	
    // if neccessary, build second half which is the mirror image of the first half
	if (symmetricStaircase)
    	for (size_t i = nSteps * nSamplesPerStep; i < 2 * nSteps * nSamplesPerStep; i++)
        	waveformData[i] = waveformData[2 * nSteps * nSamplesPerStep - i - 1];
	
    return init_Waveform_type(Waveform_Double, sampleRate, nSamples, (void**)&waveformData);
	
Error:
	
	OKfree(waveformData);
	
	return NULL;
	
}

//-----------------------------------------
// Scan Engine VChan management
//-----------------------------------------

// Fast Axis Command VChan
static void	FastAxisComVChan_Connected (VChan_type* self, void* VChanOwner, VChan_type* connectedVChan)
{ 
	ScanEngine_type*	engine				= VChanOwner;
	double*				parkedVPtr 			= NULL;
	int					error				= 0;
	char*				errMsg				= NULL;
	DataPacket_type*	parkedDataPacket	= NULL;
	
	if (!engine->fastAxisCal) return; // no parked voltage available
	
	nullChk( parkedVPtr = malloc(sizeof(double)) );
	*parkedVPtr = ((NonResGalvoCal_type*)engine->fastAxisCal)->parked;
	nullChk( parkedDataPacket = init_DataPacket_type(DL_Double, (void**)&parkedVPtr, NULL, NULL) );
	errChk( SendDataPacket(engine->VChanFastAxisCom, &parkedDataPacket, FALSE, &errMsg) );
	
	return;
	
Error:
	
	// cleanup
	OKfree(parkedVPtr);
	discard_DataPacket_type(&parkedDataPacket);
	
	if (!errMsg)
		errMsg = StrDup("Out of memory");
	
	DLMsg(errMsg, TRUE);
	OKfree(errMsg);
}

static void	FastAxisComVChan_Disconnected (VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan)
{
	
}

// Slow Axis Command VChan
static void	SlowAxisComVChan_Connected (VChan_type* self, void* VChanOwner, VChan_type* connectedVChan)
{
	ScanEngine_type*	engine				= VChanOwner;
	double*				parkedVPtr 			= NULL;
	int					error				= 0;
	char*				errMsg				= NULL;
	DataPacket_type*	parkedDataPacket	= NULL;
	
	if (!engine->slowAxisCal) return; // no parked voltage available
	
	nullChk( parkedVPtr = malloc(sizeof(double)) );
	*parkedVPtr = ((NonResGalvoCal_type*)engine->slowAxisCal)->parked;
	nullChk( parkedDataPacket = init_DataPacket_type(DL_Double, (void**)&parkedVPtr, NULL, NULL) );
	errChk( SendDataPacket(engine->VChanSlowAxisCom, &parkedDataPacket, FALSE, &errMsg) );
	
	return;
	
Error:
	
	// cleanup
	OKfree(parkedVPtr);
	discard_DataPacket_type(&parkedDataPacket);
	
	if (!errMsg)
		errMsg = StrDup("Out of memory");
	
	DLMsg(errMsg, TRUE);
	OKfree(errMsg);
}

static void	SlowAxisComVChan_Disconnected (VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan)
{
	
}

// Fast Axis Position VChan
static void FastAxisPosVChan_Connected (VChan_type* self, void* VChanOwner, VChan_type* connectedVChan)
{
	
}

static void	FastAxisPosVChan_Disconnected (VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan)
{
	
}

// Slow Axis Position VChan
static void SlowAxisPosVChan_Connected (VChan_type* self, void* VChanOwner, VChan_type* connectedVChan)
{
	
}

static void	SlowAxisPosVChan_Disconnected (VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan)
{
	
}

// Image Out VChan
static void	ImageOutVChan_Connected (VChan_type* self, void* VChanOwner, VChan_type* connectedVChan)
{
	
}

static void ImageOutVChan_Disconnected (VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan)
{
	
}

// Detection VChans
static void	DetVChan_Connected (VChan_type* self, void* VChanOwner, VChan_type* connectedVChan)
{
	
}

static void	DetVChan_Disconnected (VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan)
{
	
}

// Shutter VChan
static void	ShutterVChan_Connected (VChan_type* self, void* VChanOwner, VChan_type* connectedVChan)
{
	
}

static void	ShutterVChan_Disconnected (VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan)
{
	
}

//-----------------------------------------
// Display interface
//-----------------------------------------

static char* GenerateUniqueROIName (ListType ROIList)
{
	return NULL;
}

static void ROIPlacedOnDisplay_CB (DisplayHandle_type displayHandle, void* callbackData, ROI_type** ROI)
{
	DetChan_type*	detChan = callbackData; 
	
	
}

static void	ROIRemovedFromDisplay_CB (DisplayHandle_type displayHandle, void* callbackData, ROI_type* ROI)
{
	DetChan_type*	detChan = callbackData;
	
}

static int RestoreImgSettings_CB (DisplayEngine_type* displayEngine, DisplayHandle_type displayHandle, void* callbackData, char** errorInfo)
{
	RectRasterScanSet_type* 	previousScanSettings 		= callbackData;
	int							error						= 0;
	DetChan_type*				detChan						= (DetChan_type*)(*displayEngine->getDisplayHandleCBDataFptr) (displayHandle);
	RectRasterScanSet_type*		currentScanSettings			= &((RectRaster_type*)detChan->scanEngine)->scanSettings;			
	RectRaster_type*			scanEngine					= (RectRaster_type*)detChan->scanEngine;
	
	// assign previous scan settings
	currentScanSettings->pixSize			= previousScanSettings->pixSize;
	currentScanSettings->height				= previousScanSettings->height;
	currentScanSettings->heightOffset		= previousScanSettings->heightOffset;
	currentScanSettings->width				= previousScanSettings->width;
	currentScanSettings->widthOffset		= previousScanSettings->widthOffset;
	currentScanSettings->pixelDwellTime		= previousScanSettings->pixelDwellTime;
	
	// update preferred heights and pixel dwell times
	NonResRectRasterScan_ScanHeights(scanEngine);
	NonResRectRasterScan_PixelDwellTimes(scanEngine);
	
	// update remaining controls on the scan panel
	SetCtrlVal(scanEngine->baseClass.scanSetPanHndl, RectRaster_HeightOffset, currentScanSettings->heightOffset * currentScanSettings->pixSize);
	SetCtrlVal(scanEngine->baseClass.scanSetPanHndl, RectRaster_Width, currentScanSettings->width * currentScanSettings->pixSize);
	SetCtrlVal(scanEngine->baseClass.scanSetPanHndl, RectRaster_WidthOffset, currentScanSettings->widthOffset * currentScanSettings->pixSize);
	SetCtrlVal(scanEngine->baseClass.scanSetPanHndl, RectRaster_PixelSize, currentScanSettings->pixSize);
	
	return 0;
	
}

static void	DisplayErrorHandler_CB (DisplayHandle_type displayHandle, int errorID, char* errorInfo)
{
	DLMsg(errorInfo, 1);
}

//---------------------------------------------------------------------
// Non Resonant Galvo Calibration and Testing Task Controller Callbacks 
//---------------------------------------------------------------------

static int ConfigureTC_NonResGalvoCal (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	ActiveNonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	// total number of iterations to start with
	SetTaskControlIterations(taskControl, 1);
	// set starting calibration type
	cal->currentCal = NonResGalvoCal_Slope_Offset;
	// reset iteration index
	cal->currIterIdx = 0;
	
	return 0;
}

static int UncofigureTC_NonResGalvoCal (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	//ActiveNonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);  
	
	return 0;
}

static void IterateTC_NonResGalvoCal (TaskControl_type* taskControl, BOOL const* abortIterationFlag)
{
#define		IterateTC_NonResGalvoCal_Err_DifferentCommandAndResponseNumSamples 		-1
	ActiveNonResGalvoCal_type* 		cal 						= GetTaskControlModuleData(taskControl);
	double*							commandSignal				= NULL;
	double**						commandSignalPtr			= NULL;
	double**						positionSignalPtr			= NULL;
	DataPacket_type*				commandPacket				= NULL;
	char*							errMsg						= NULL;
	uInt64*							nCommandWaveformSamplesPtr  = NULL;
	size_t							nCommandSignalSamples		= 0;
	size_t							nPositionSignalSamples		= 0;
	int								error						= 0;
	Waveform_type*					waveformCopy				= NULL;
	WaveformTypes					waveformType;
	DataPacket_type*				nullPacket					= NULL;
	
	switch (cal->currentCal) {
			
		case NonResGalvoCal_Slope_Offset:
		{
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			// Generate and send galvo command signal
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			
			// discard previous measurements
			OKfree(cal->slope);
			OKfree(cal->offset);
			OKfree(cal->posStdDev);
			
			// create waveform with galvo steps to measure slope and offset between command and position signals
			size_t 		nDelaySamples 			= (size_t) (SLOPE_OFFSET_DELAY * *cal->baseClass.comSampRate);
			size_t		nSamplesPerCalPoint		= nDelaySamples + POSSAMPLES;
			double		VCommand				= -cal->commandVMax;
			
			nullChk( commandSignal = malloc (CALPOINTS * nSamplesPerCalPoint * sizeof(double)) );
			
			for (size_t i = 0; i < CALPOINTS; i++) {
				for (size_t j = 0; j < nSamplesPerCalPoint; j++)
					commandSignal[i*nSamplesPerCalPoint+j] = VCommand;
				VCommand += 2*cal->commandVMax / (CALPOINTS - 1); 
			}
			
			// send command waveform
			nCommandSignalSamples = CALPOINTS * nSamplesPerCalPoint;
			nullChk( cal->commandWaveform = init_Waveform_type(Waveform_Double, *cal->baseClass.comSampRate, nCommandSignalSamples, (void**)&commandSignal) );
			errChk( CopyWaveform(&waveformCopy, cal->commandWaveform, &errMsg) );
			nullChk( commandPacket = init_DataPacket_type(DL_Waveform_Double, (void**)&waveformCopy, NULL, (DiscardPacketDataFptr_type)discard_Waveform_type) );
			errChk( SendDataPacket(cal->baseClass.VChanCom, &commandPacket, 0, &errMsg) );
			errChk( SendDataPacket(cal->baseClass.VChanCom, &nullPacket, 0, &errMsg) );
			
			// send number of samples in waveform
			nullChk( nCommandWaveformSamplesPtr = malloc (sizeof(uInt64)) );
			*nCommandWaveformSamplesPtr = (uInt64) nCommandSignalSamples;
			
			nullChk( commandPacket = init_DataPacket_type(DL_ULongLong, (void**)&nCommandWaveformSamplesPtr, NULL, NULL) );
			errChk( SendDataPacket(cal->baseClass.VChanComNSamples, &commandPacket, 0, &errMsg) );
			
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			// Receive and analyze galvo response signal
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			
			errChk( ReceiveWaveform(cal->baseClass.VChanPos, &cal->positionWaveform, &waveformType, &errMsg) );
			
			//get pointer to galvo position signal
			positionSignalPtr 	= (double**) GetWaveformPtrToData (cal->positionWaveform, &nPositionSignalSamples); 
			commandSignalPtr	= (double**) GetWaveformPtrToData (cal->commandWaveform, &nCommandSignalSamples); 
			
			// check if number of received samples is different than the number of sent samples
			if (nPositionSignalSamples != nCommandSignalSamples) {
				errMsg = FormatMsg(IterateTC_NonResGalvoCal_Err_DifferentCommandAndResponseNumSamples, "IterateTC_NonResGalvoCal", "Number of command and response samples in the sent and received \
									waveforms must be the same");
				error = IterateTC_NonResGalvoCal_Err_DifferentCommandAndResponseNumSamples;
				goto Error;
			}
			
			// analyze galvo response
			double  	Pos[CALPOINTS]				= {0};
			double  	Comm[CALPOINTS]				= {0};
			double		PosStdDev[CALPOINTS]		= {0};
			double  	LinFitResult[CALPOINTS]		= {0};
			double  	meanSquaredError			= 0;
			
			// analyze galvo response  
			for (size_t i = 0; i < CALPOINTS; i++) {
				// get average position and signal StdDev
				StdDev(*positionSignalPtr + i*nSamplesPerCalPoint + nDelaySamples, POSSAMPLES, &Pos[i], &PosStdDev[i]);
				Comm[i] = 2*cal->commandVMax * i /(CALPOINTS - 1) - cal->commandVMax; 
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
			TaskControlIterationDone(taskControl, 0, "", TRUE);
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			
		}
			
			break;
			
		case NonResGalvoCal_Lag:
		{
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			// Generate and send galvo command signal
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			
			// discard previous lag measurement if it exists and allocate new memory    
			if (!cal->currIterIdx) {
				OKfree(cal->lag);
				// determine how many times to apply the ramp such that the relative error in the position is less than RELERR
				cal->nRepeat 		= (size_t) ceil(pow(*cal->posStdDev * 1.414 /(RELERR * 2 * cal->commandVMax), 2)); 
				cal->nRampSamples 	= 2;	 // must be at least 2 otherwise the Ramp function doesn't work
				cal->lastRun 		= FALSE; 
			}
			
			size_t 	postRampSamples = (size_t) floor(POSTRAMPTIME * 0.001 * *cal->baseClass.comSampRate) + 1;
			size_t 	flybackSamples 	= (size_t) floor(MAXFLYBACKTIME * 0.001 * *cal->baseClass.comSampRate) + 1; 
			
			// create ramp
			nullChk( commandSignal = malloc ((flybackSamples + cal->nRampSamples + postRampSamples) * cal->nRepeat * sizeof(double)) );
			// pad with flyback samples
			for (size_t i = 0; i < flybackSamples; i++) commandSignal[i] = -cal->commandVMax;
			// generate ramp
			Ramp(cal->nRampSamples, -cal->commandVMax, cal->commandVMax, commandSignal + flybackSamples);
			// pad with postrampsamples
			for (size_t i = flybackSamples + cal->nRampSamples; i < (flybackSamples + cal->nRampSamples + postRampSamples); i++) 
				commandSignal[i] = cal->commandVMax;
			// extend signal to nRepeat times
			for (size_t i = 1; i < cal->nRepeat; i++)
				memcpy(commandSignal + i * (flybackSamples + cal->nRampSamples + postRampSamples), commandSignal, (flybackSamples + cal->nRampSamples + postRampSamples) * sizeof(double));
			
			// send command waveform
			nCommandSignalSamples = (flybackSamples + cal->nRampSamples + postRampSamples) * cal->nRepeat;
			nullChk( cal->commandWaveform = init_Waveform_type(Waveform_Double, *cal->baseClass.comSampRate, nCommandSignalSamples, (void**)&commandSignal) );
			errChk( CopyWaveform(&waveformCopy, cal->commandWaveform, &errMsg) );
			nullChk( commandPacket = init_DataPacket_type(DL_Waveform_Double, (void**)&waveformCopy, NULL, (DiscardPacketDataFptr_type)discard_Waveform_type) );
			errChk( SendDataPacket(cal->baseClass.VChanCom, &commandPacket, 0, &errMsg) );
			errChk( SendDataPacket(cal->baseClass.VChanCom, &nullPacket, 0, &errMsg) ); 
			
			// send number of samples in waveform
			nCommandWaveformSamplesPtr = malloc (sizeof(uInt64));
			*nCommandWaveformSamplesPtr = (uInt64)nCommandSignalSamples;
			nullChk( commandPacket = init_DataPacket_type(DL_ULongLong, (void**)&nCommandWaveformSamplesPtr, NULL, NULL) );
			errChk( SendDataPacket(cal->baseClass.VChanComNSamples, &commandPacket, 0, &errMsg) );
			
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			// Receive and analyze galvo response signal
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			
			errChk( ReceiveWaveform(cal->baseClass.VChanPos, &cal->positionWaveform, &waveformType, &errMsg) );
			
			//get pointer to galvo position signal
			positionSignalPtr 	= (double**)GetWaveformPtrToData (cal->positionWaveform, &nPositionSignalSamples); 
			commandSignalPtr	= (double**)GetWaveformPtrToData (cal->commandWaveform, &nCommandSignalSamples); 
			
			// check if number of received samples is different than the number of sent samples
			if (nPositionSignalSamples != nCommandSignalSamples) {
				errMsg = FormatMsg(IterateTC_NonResGalvoCal_Err_DifferentCommandAndResponseNumSamples, "IterateTC_NonResGalvoCal", "Number of command and response samples in the sent and received \
									waveforms must be the same");
				error = IterateTC_NonResGalvoCal_Err_DifferentCommandAndResponseNumSamples;
				goto Error;
			}
			
			// average galvo ramp responses, use only ramp and post ramp time to analyze the data
			double*		averageResponse = calloc (cal->nRampSamples + postRampSamples, sizeof(double));
				
			// sum up ramp responses
			for (size_t i = 0; i < cal->nRepeat; i++)
				for (size_t j = 0; j < cal->nRampSamples + postRampSamples; j++)
					averageResponse[j] += (*positionSignalPtr)[i*(flybackSamples + cal->nRampSamples + postRampSamples)+flybackSamples+j];
				
			// average ramp responses
			for (size_t i = 0; i < cal->nRampSamples + postRampSamples; i++) averageResponse[i] /= cal->nRepeat;
				
			// calculate corrected position signal based on scaling and offset
			for (size_t i = 0; i < cal->nRampSamples + postRampSamples; i++) averageResponse[i] = *cal->slope * averageResponse[i] + *cal->offset;
				
			// calculate response slope
			double rampSlope;
			FindSlope(averageResponse, cal->nRampSamples + postRampSamples, *cal->baseClass.comSampRate, *cal->posStdDev, cal->nRepeat, RELERR, &rampSlope);   
			// calculate command ramp slope
			double targetSlope = 2*cal->commandVMax / ((cal->nRampSamples - 1) * 1000/ *cal->baseClass.comSampRate);  // ramp slope in [V/ms]
				
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
			PlotWaveform(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, *commandSignalPtr+flybackSamples, cal->nRampSamples + postRampSamples, VAL_DOUBLE, 1.0, 0, 0, 1000/ *cal->baseClass.comSampRate, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, VAL_BLUE);
			PlotWaveform(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, averageResponse, cal->nRampSamples + postRampSamples, VAL_DOUBLE, 1.0, 0, 0, 1000/ *cal->baseClass.comSampRate, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, VAL_RED);
			
			double x, y;
			
			GetGraphCursor(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, 1, &x, &y);
			SetCtrlVal(cal->baseClass.calPanHndl, NonResGCal_CursorX, x);
			SetCtrlVal(cal->baseClass.calPanHndl, NonResGCal_CursorY, y);
				
			// if this was the last run, calculate lag
			if (cal->lastRun) {
					
				cal->lag = malloc(sizeof(double));
				cal->currIterIdx = 0;
				cal->currentCal++; 
				cal->lastRun = FALSE;
				*cal->lag = MeasureLag(*commandSignalPtr+flybackSamples, averageResponse, cal->nRampSamples + postRampSamples) * 1000/ *cal->baseClass.comSampRate; // response lag in [ms]
				// update results
				int calibPanHndl;
				GetPanelHandleFromTabPage(cal->baseClass.calPanHndl, NonResGCal_Tab, 0, &calibPanHndl);
				
				// slope
				SetCtrlVal(calibPanHndl, Cal_ResponseLag, *cal->lag);
				SetCtrlAttribute(calibPanHndl, Cal_ResponseLag, ATTR_DIMMED, 0);
				
			} else {
				if (rampSlope < targetSlope * 0.98) {
					// calculate ramp that has maxslope
					cal->nRampSamples = (size_t) floor(2*cal->commandVMax / rampSlope * *cal->baseClass.comSampRate * 0.001);
					cal->lastRun = FALSE;
				}
				else {
					// reduce slope
					cal->nRampSamples = (size_t) floor(2*cal->commandVMax / (rampSlope * MAX_SLOPE_REDUCTION_FACTOR) * *cal->baseClass.comSampRate * 0.001); 
					cal->lastRun = TRUE;
				}
				if (cal->nRampSamples < 2) cal->nRampSamples = 2;  // make sure that the ramp generating function has at least 2 samples
				cal->currIterIdx++;
			}
				
			OKfree(averageResponse);
			TaskControlIterationDone(taskControl, 0, "", TRUE);
			
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
		}
			
			break;
			
		case NonResGalvoCal_SwitchTimes:
		{
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			// Generate and send galvo command signal
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			
			// delete previous measurements and adjust plotting area
			if (!cal->currIterIdx) {
				discard_SwitchTimes_type(&cal->switchTimes);
				cal->switchTimes = init_SwitchTimes_type();
				
				DeleteGraphPlot(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, -1 , VAL_IMMEDIATE_DRAW);
				SetAxisScalingMode(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, VAL_BOTTOM_XAXIS, VAL_MANUAL, 0, 2*cal->commandVMax);
				SetAxisScalingMode(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, VAL_LEFT_YAXIS, VAL_AUTOSCALE, 0, 0);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_XLABEL_VISIBLE, 1);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_YLABEL_VISIBLE, 1);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_XGRID_VISIBLE, 1);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_YGRID_VISIBLE, 1);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_LABEL_TEXT, "Halfswitch time vs. ramp amplitude");   
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_XNAME, "Ramp amplitude (V)");
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_YNAME, "Time (�s)");
			}
			
			size_t 	postStepSamples = (size_t) floor(POSTRAMPTIME * 0.001 * *cal->baseClass.comSampRate) + 1;
			size_t 	flybackSamples 	= (size_t) floor(MAXFLYBACKTIME * 0.001 * *cal->baseClass.comSampRate) + 1; 
			double	amplitudeFactor = pow(SWITCH_TIMES_AMP_ITER_FACTOR, (double)cal->currIterIdx);
			
			// determine how many times to apply the step such that the relative error in the position is less than RELERR   
			cal->nRepeat = (size_t) ceil(pow(*cal->posStdDev * 1.414 /(RELERR * 2*cal->commandVMax * amplitudeFactor), 2));
			
			// create step signal
			nullChk( commandSignal = malloc ((flybackSamples + postStepSamples) * cal->nRepeat * sizeof(double)) );
			
			// pad flyback samples
			for (size_t i = 0; i < flybackSamples; i++) commandSignal[i] = -cal->commandVMax * amplitudeFactor;
			// pad post step samples
			for (size_t i = flybackSamples; i < (flybackSamples + postStepSamples); i++) commandSignal[i] = cal->commandVMax * amplitudeFactor;
			// apply signal nRepeat times
			for (size_t i = 1; i < cal->nRepeat; i++)
				memcpy(commandSignal + i * (flybackSamples + postStepSamples), commandSignal, (flybackSamples + postStepSamples) * sizeof(double));
			
			// send command waveform
			nCommandSignalSamples = (flybackSamples + postStepSamples) * cal->nRepeat;
			nullChk( cal->commandWaveform = init_Waveform_type(Waveform_Double, *cal->baseClass.comSampRate, nCommandSignalSamples, (void**)&commandSignal) );
			errChk( CopyWaveform(&waveformCopy, cal->commandWaveform, &errMsg) );
			nullChk( commandPacket = init_DataPacket_type(DL_Waveform_Double, (void**)&waveformCopy, NULL, (DiscardPacketDataFptr_type)discard_Waveform_type) );
			errChk( SendDataPacket(cal->baseClass.VChanCom, &commandPacket, 0, &errMsg) );
			errChk( SendDataPacket(cal->baseClass.VChanCom, &nullPacket, 0, &errMsg) );
			
			// send number of samples in waveform
			nullChk( nCommandWaveformSamplesPtr = malloc (sizeof(uInt64)) );
			*nCommandWaveformSamplesPtr = (uInt64)nCommandSignalSamples;
			nullChk( commandPacket = init_DataPacket_type(DL_ULongLong, (void**)&nCommandWaveformSamplesPtr, NULL,NULL) );
			errChk( SendDataPacket(cal->baseClass.VChanComNSamples, &commandPacket, 0, &errMsg) );
			
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			// Receive and analyze galvo response signal
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			
			errChk( ReceiveWaveform(cal->baseClass.VChanPos, &cal->positionWaveform, &waveformType, &errMsg) );
			
			//get pointer to galvo position signal
			positionSignalPtr 	= (double**)GetWaveformPtrToData (cal->positionWaveform, &nPositionSignalSamples); 
			commandSignalPtr	= (double**)GetWaveformPtrToData (cal->commandWaveform, &nCommandSignalSamples); 
			
			// check if number of received samples is different than the number of sent samples
			if (nPositionSignalSamples != nCommandSignalSamples) {
				errMsg = FormatMsg(IterateTC_NonResGalvoCal_Err_DifferentCommandAndResponseNumSamples, "IterateTC_NonResGalvoCal", "Number of command and response samples in the sent and received \
									waveforms must be the same");
				error = IterateTC_NonResGalvoCal_Err_DifferentCommandAndResponseNumSamples;
				goto Error;
			}
			
			double*		averageResponse = calloc (postStepSamples, sizeof(double));
			// sum up step responses
			for (size_t i = 0; i < cal->nRepeat; i++)
				for (size_t j = 0; j < postStepSamples; j++)
					averageResponse[j] += (*positionSignalPtr)[i * (flybackSamples + postStepSamples) + flybackSamples + j];
			// average ramp responses
			for (size_t i = 0; i < postStepSamples; i++) averageResponse[i] /= cal->nRepeat;
				
			// calculate corrected position signal based on scaling and offset
			for (size_t i = 0; i < postStepSamples; i++) averageResponse[i] = *cal->slope * averageResponse[i] + *cal->offset;
				
			// find 50% crossing point where response is halfway between the applied step
			for (size_t i = 0; i < postStepSamples; i++) 
				if (averageResponse[i] > 0) { 
					cal->switchTimes->n++;
					cal->switchTimes->stepSize = realloc (cal->switchTimes->stepSize, cal->switchTimes->n * sizeof(double));
					cal->switchTimes->stepSize[cal->switchTimes->n - 1] = 2*cal->commandVMax * amplitudeFactor;
					cal->switchTimes->halfSwitch = realloc (cal->switchTimes->halfSwitch, cal->switchTimes->n * sizeof(double));
					cal->switchTimes->halfSwitch[cal->switchTimes->n - 1] = i / *cal->baseClass.comSampRate * 1000;
						
					// plot switching time
					double x, y;
					PlotPoint(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, 2 * cal->commandVMax * amplitudeFactor, cal->switchTimes->halfSwitch[cal->switchTimes->n - 1]  * 1000 , VAL_ASTERISK, VAL_BLUE);
					GetGraphCursor(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, 1, &x, &y);
					SetCtrlVal(cal->baseClass.calPanHndl, NonResGCal_CursorX, x);
					SetCtrlVal(cal->baseClass.calPanHndl, NonResGCal_CursorY, y);
						
					break;
				}
				
			if (2 * cal->commandVMax * amplitudeFactor >= cal->resolution * SWITCH_TIMES_AMP_ITER_FACTOR)
			  cal->currIterIdx++;
			else {
					
				cal->currIterIdx = 0;
				cal->currentCal++;
			}
				
			OKfree(averageResponse);
			TaskControlIterationDone(taskControl, 0, "", TRUE);  
			
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
		}
			break;
			
		case NonResGalvoCal_MaxSlopes:
		{	
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			// Generate and send galvo command signal
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			
			// delete previous measurements
			if (!cal->currIterIdx) {
				discard_MaxSlopes_type(&cal->maxSlopes);
				cal->maxSlopes = init_MaxSlopes_type();
				// determine how many times to apply the ramp such that the relative error in the position is less than RELERR
				cal->nRampSamples = 2;
				
				// adjust plotting
				DeleteGraphPlot(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, -1 , VAL_IMMEDIATE_DRAW);
				SetAxisScalingMode(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, VAL_BOTTOM_XAXIS, VAL_MANUAL, 0, 2*cal->commandVMax);
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
			double	amplitudeFactor = pow(FIND_MAXSLOPES_AMP_ITER_FACTOR, (double)cal->maxSlopes->n);
			
			// determine how many times to apply the step such that the relative error in the position is less than RELERR   
			cal->nRepeat = (size_t) ceil(pow(*cal->posStdDev * 1.414 /(RELERR * 2*cal->commandVMax * amplitudeFactor), 2));
			
			// create ramp
			nullChk( commandSignal = malloc ((flybackSamples + cal->nRampSamples + postRampSamples) * cal->nRepeat * sizeof(double)) );
			// pad with flyback samples
			for (size_t i = 0; i < flybackSamples; i++) commandSignal[i] = -cal->commandVMax * amplitudeFactor;
			// generate ramp
			Ramp(cal->nRampSamples, -cal->commandVMax * amplitudeFactor, cal->commandVMax * amplitudeFactor, commandSignal + flybackSamples);
			// pad with postrampsamples
			for (size_t i = flybackSamples + cal->nRampSamples; i < (flybackSamples + cal->nRampSamples + postRampSamples); i++) commandSignal[i] = cal->commandVMax * amplitudeFactor;
			// apply signal nRepeat times
			for (size_t i = 1; i < cal->nRepeat; i++)
				memcpy(commandSignal + i * (flybackSamples + cal->nRampSamples + postRampSamples), commandSignal, (flybackSamples + cal->nRampSamples + postRampSamples) * sizeof(double));
			
			// send command waveform
			nCommandSignalSamples = (flybackSamples + cal->nRampSamples + postRampSamples) * cal->nRepeat;
			nullChk( cal->commandWaveform = init_Waveform_type(Waveform_Double, *cal->baseClass.comSampRate, nCommandSignalSamples, (void**)&commandSignal) );
			errChk( CopyWaveform(&waveformCopy, cal->commandWaveform, &errMsg) );
			nullChk( commandPacket = init_DataPacket_type(DL_Waveform_Double, (void**)&waveformCopy, NULL, (DiscardPacketDataFptr_type)discard_Waveform_type) );
			errChk( SendDataPacket(cal->baseClass.VChanCom, &commandPacket, 0, &errMsg) );
			errChk( SendDataPacket(cal->baseClass.VChanCom, &nullPacket, 0, &errMsg) ); 
			
			// send number of samples in waveform
			nullChk( nCommandWaveformSamplesPtr = malloc (sizeof(uInt64)) );
			*nCommandWaveformSamplesPtr = (uInt64)nCommandSignalSamples;
			nullChk( commandPacket = init_DataPacket_type(DL_ULongLong, (void**)&nCommandWaveformSamplesPtr, NULL, NULL) );
			errChk( SendDataPacket(cal->baseClass.VChanComNSamples, &commandPacket, 0, &errMsg) );
			
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			// Receive and analyze galvo response signal
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			
			errChk( ReceiveWaveform(cal->baseClass.VChanPos, &cal->positionWaveform, &waveformType, &errMsg) );
			
			//get pointer to galvo position signal
			positionSignalPtr	= (double**)GetWaveformPtrToData (cal->positionWaveform, &nPositionSignalSamples); 
			commandSignalPtr	= (double**)GetWaveformPtrToData (cal->commandWaveform, &nCommandSignalSamples); 
			
			// check if number of received samples is different than the number of sent samples
			if (nPositionSignalSamples != nCommandSignalSamples) {
				errMsg = FormatMsg(IterateTC_NonResGalvoCal_Err_DifferentCommandAndResponseNumSamples, "IterateTC_NonResGalvoCal", "Number of command and response samples in the sent and received \
									waveforms must be the same");
				error = IterateTC_NonResGalvoCal_Err_DifferentCommandAndResponseNumSamples;
				goto Error;
			}
			
			double*		averageResponse = calloc (cal->nRampSamples + postRampSamples, sizeof(double));
				
			// sum up ramp responses
			for (size_t i = 0; i < cal->nRepeat; i++)
				for (size_t j = 0; j < cal->nRampSamples + postRampSamples; j++)
					averageResponse[j] += (*positionSignalPtr)[i * (flybackSamples + cal->nRampSamples + postRampSamples) + flybackSamples + j];
			// average ramp responses
			for (size_t i = 0; i < cal->nRampSamples + postRampSamples; i++) averageResponse[i] /= cal->nRepeat;
				
			// calculate corrected position signal based on scaling and offset
			for (size_t i = 0; i < cal->nRampSamples + postRampSamples; i++) averageResponse[i] = *cal->slope * averageResponse[i] + *cal->offset;
				
			// calculate ramp slope
			double responseSlope;
			double commandSlope = 2 * cal->commandVMax * amplitudeFactor / ((cal->nRampSamples - 1) * 1000/ *cal->baseClass.comSampRate); 
				
			FindSlope(averageResponse, cal->nRampSamples + postRampSamples, *cal->baseClass.comSampRate, *cal->posStdDev, cal->nRepeat, RELERR, &responseSlope); 
			
			if (2 * cal->commandVMax * amplitudeFactor >= cal->minStepSize * FIND_MAXSLOPES_AMP_ITER_FACTOR) {
				if (responseSlope < commandSlope * 0.98) {
					// calculate ramp that has maxslope
					size_t newRampSamples = (size_t) ceil(2 * cal->commandVMax * amplitudeFactor / responseSlope * *cal->baseClass.comSampRate * 0.001);
					// ensure convergence
					if (newRampSamples == cal->nRampSamples) cal->nRampSamples++;
					else
						cal->nRampSamples = newRampSamples;
					
					if (cal->nRampSamples < 2) cal->nRampSamples = 2;
				}
				else {
					// reset ramp samples
					cal->nRampSamples = 2;
					// store slope value
					cal->maxSlopes->n++;
					cal->maxSlopes->amplitude = realloc (cal->maxSlopes->amplitude, cal->maxSlopes->n * sizeof(double));
					cal->maxSlopes->amplitude[cal->maxSlopes->n - 1] = 2 * cal->commandVMax * amplitudeFactor;
					cal->maxSlopes->slope = realloc (cal->maxSlopes->slope, cal->maxSlopes->n * sizeof(double)); 
					cal->maxSlopes->slope[cal->maxSlopes->n - 1] = responseSlope;
					// plot slope
					double x, y;
					PlotPoint(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, 2 * cal->commandVMax * amplitudeFactor, responseSlope, VAL_ASTERISK, VAL_BLUE);
					GetGraphCursor(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, 1, &x, &y);
					SetCtrlVal(cal->baseClass.calPanHndl, NonResGCal_CursorX, x);
					SetCtrlVal(cal->baseClass.calPanHndl, NonResGCal_CursorY, y);
				}
				
				cal->currIterIdx++;  
				
			} else {
				cal->currIterIdx	= 0;
				cal->extraRuns 		= 0;  
				cal->currentCal++; 
			}
				
			OKfree(averageResponse);
			TaskControlIterationDone(taskControl, 0, "", TRUE); 
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			
		}
			break;
			
		case NonResGalvoCal_TriangleWave:
		{
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			// Generate and send galvo command signal
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			
			if (!cal->currIterIdx) {
				discard_TriangleCal_type(&cal->triangleCal);
				cal->triangleCal 		= init_TriangleCal_type();
				// initialize target slope 
				cal->targetSlope		= cal->maxSlopes->slope[0] * DYNAMICCAL_INITIAL_SLOPE_REDUCTION_FACTOR;    
				// delete previous plot
				DeleteGraphPlot(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, -1, VAL_IMMEDIATE_DRAW); 
				SetAxisScalingMode(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, VAL_BOTTOM_XAXIS, VAL_MANUAL, 0, 2*cal->commandVMax);
				SetAxisScalingMode(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, VAL_LEFT_YAXIS, VAL_AUTOSCALE, 0, 0);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_XLABEL_VISIBLE, 1);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_YLABEL_VISIBLE, 1);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_XGRID_VISIBLE, 1);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_YGRID_VISIBLE, 1);
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_LABEL_TEXT, "Maximum scan frequency vs. amplitude");   
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_XNAME, "Amplitude (V)");
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_YNAME, "Max triangle frequency (Hz)");
			}
			
			double 		funcAmp 		= cal->maxSlopes->amplitude[cal->triangleCal->n]; 											// in [V] Pk-Pk
			// apply scan function with slope equal to max slope measured previously at a certain amplitude
			double		funcFreq 		= cal->targetSlope * 1000/(2 * funcAmp);			   										// in [Hz]
			size_t 		nCycles 		= (size_t) ceil(cal->scanTime * funcFreq); 
			size_t 		cycleSamples 	= (size_t) ceil (1/funcFreq * *cal->baseClass.comSampRate);
			// re-calculate target slope due to rounding of number of cycle samples
			cal->targetSlope			= funcAmp * *cal->baseClass.comSampRate * 2e-3 /cycleSamples;
			// calculate number of cycles, precycles and samples
			size_t		preCycles 		= (size_t) ceil (*cal->lag /1000 * funcFreq);
			size_t 		nSamples 		= (nCycles + preCycles) * cycleSamples;
			
		  
			// create ramp
			nullChk( commandSignal 	= malloc (nSamples * sizeof(double)) );
			
			// generate command signal
			double		phase	 		= -90;
			TriangleWave(nSamples, funcAmp/2, 1.0/cycleSamples, &phase, commandSignal);
						
			// send command waveform
			nCommandSignalSamples = nSamples;
			nullChk( cal->commandWaveform = init_Waveform_type(Waveform_Double, *cal->baseClass.comSampRate, nCommandSignalSamples, (void**)&commandSignal) );
			errChk( CopyWaveform(&waveformCopy, cal->commandWaveform, &errMsg) );
			nullChk( commandPacket = init_DataPacket_type(DL_Waveform_Double, (void**)&waveformCopy, NULL, (DiscardPacketDataFptr_type)discard_Waveform_type) );
			errChk( SendDataPacket(cal->baseClass.VChanCom, &commandPacket, 0, &errMsg) );
			errChk( SendDataPacket(cal->baseClass.VChanCom, &nullPacket, 0, &errMsg) );
			
			// send number of samples in waveform
			nullChk( nCommandWaveformSamplesPtr = malloc (sizeof(uInt64)) );
			*nCommandWaveformSamplesPtr = (uInt64)nCommandSignalSamples;
			nullChk( commandPacket = init_DataPacket_type(DL_ULongLong, (void**)&nCommandWaveformSamplesPtr, NULL, NULL) );
			errChk( SendDataPacket(cal->baseClass.VChanComNSamples, &commandPacket, 0, &errMsg) );
			
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			// Receive and analyze galvo response signal
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			
			errChk( ReceiveWaveform(cal->baseClass.VChanPos, &cal->positionWaveform, &waveformType, &errMsg) );
			
			//get pointer to galvo position signal
			positionSignalPtr 	= (double**)GetWaveformPtrToData (cal->positionWaveform, &nPositionSignalSamples); 
			commandSignalPtr	= (double**)GetWaveformPtrToData (cal->commandWaveform, &nCommandSignalSamples); 
			
			// check if number of received samples is different than the number of sent samples
			if (nPositionSignalSamples != nCommandSignalSamples) {
				errMsg = FormatMsg(IterateTC_NonResGalvoCal_Err_DifferentCommandAndResponseNumSamples, "IterateTC_NonResGalvoCal", "Number of command and response samples in the sent and received \
									waveforms must be the same");
				error = IterateTC_NonResGalvoCal_Err_DifferentCommandAndResponseNumSamples;
				goto Error;
			}
			
			BOOL		overloadFlag	= FALSE;
			double* 	averageResponse = NULL;
				
			// check if there is overload
			for (size_t i = 0; i < nSamples; i++) 
				if (((*positionSignalPtr)[i] < - funcAmp/2 * 1.5 - 10 * *cal->posStdDev) || ((*positionSignalPtr)[i] > funcAmp/2 * 1.5 + 10 * *cal->posStdDev)) {
					overloadFlag = TRUE;
					break;
				}
				
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
						averageResponse[j] += (*positionSignalPtr)[i * cycleSamples + delta + j];
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
					cal->triangleCal->commandAmp[cal->triangleCal->n - 1] = cal->maxSlopes->amplitude[cal->triangleCal->n - 1];
					cal->triangleCal->maxFreq = realloc(cal->triangleCal->maxFreq, cal->triangleCal->n * sizeof(double));   
					cal->triangleCal->maxFreq[cal->triangleCal->n - 1] = maxSlope * 1000/(2 * funcAmp);  // maximum triangle function frequency! 
					cal->triangleCal->resLag = realloc(cal->triangleCal->resLag, cal->triangleCal->n * sizeof(double));  
					cal->triangleCal->resLag[cal->triangleCal->n - 1] = MeasureLag(*commandSignalPtr, averageResponse, cycleSamples) * 1000/ *cal->baseClass.comSampRate;
					cal->triangleCal->deadTime += nTotalSamplesDelay / *cal->baseClass.comSampRate * 1000 ; // delay in ms 
						
					// plot
					PlotPoint(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, funcAmp, cal->triangleCal->maxFreq[cal->triangleCal->n-1], VAL_ASTERISK, VAL_BLUE);
					SetGraphCursor(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, 1, funcAmp, cal->triangleCal->maxFreq[cal->triangleCal->n-1]);
					SetCtrlVal(cal->baseClass.calPanHndl, NonResGCal_CursorX, funcAmp);
					SetCtrlVal(cal->baseClass.calPanHndl, NonResGCal_CursorY, cal->triangleCal->maxFreq[cal->triangleCal->n-1]);
						
					// continue with next maximum slope
					cal->extraRuns = 0;
					
					// init next target slope
					cal->targetSlope = cal->maxSlopes->slope[cal->triangleCal->n] * DYNAMICCAL_INITIAL_SLOPE_REDUCTION_FACTOR;
				}
			}
			
			cal->currIterIdx++;
			
			// reduce slope
			cal->targetSlope 	*= DYNAMICCAL_SLOPE_ITER_FACTOR;    
					
			OKfree(averageResponse);
			discard_Waveform_type(&cal->commandWaveform); 
				
			if (cal->triangleCal->n < cal->maxSlopes->n) {
				TaskControlIterationDone(taskControl, 0, "", TRUE); 
			}
			else {
				// calculate average dead time
				cal->triangleCal->deadTime /= cal->maxSlopes->n;
				// calculate actual triangle waveform amplitude
				cal->triangleCal->actualAmp = malloc(cal->maxSlopes->n * sizeof(double));
				for (size_t i = 0; i < cal->maxSlopes->n; i++)
					cal->triangleCal->actualAmp[i] = cal->triangleCal->commandAmp[i] * (1 - 4 * cal->triangleCal->maxFreq[i] * cal->triangleCal->deadTime * 0.001);  
					
				// end task controller iterations
				TaskControlIterationDone(taskControl, 0, "", FALSE);
			}
			
		
			
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
		}
			break;
	}
	
	// cleanup
	discard_Waveform_type(&cal->positionWaveform);
	discard_Waveform_type(&cal->commandWaveform);
	
	return;
	
Error:
	
	discard_Waveform_type(&cal->positionWaveform);
	discard_Waveform_type(&cal->commandWaveform);
	discard_Waveform_type(&waveformCopy);
	discard_DataPacket_type(&commandPacket);
	OKfree(commandSignal);
	OKfree(nCommandWaveformSamplesPtr);
	
	// create out of memory message
	if (error == UIEOutOfMemory && !errMsg)
		errMsg = StrDup("Out of memory");
	
	TaskControlIterationDone(cal->baseClass.taskController, error, errMsg, 0);
	OKfree(errMsg);
}

static void AbortIterationTC_NonResGalvoCal (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	ActiveNonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	// cleanup
	discard_Waveform_type(&cal->positionWaveform);
	discard_Waveform_type(&cal->commandWaveform); 
	
	TaskControlIterationDone(taskControl, 0, "", FALSE);
}

static int StartTC_NonResGalvoCal (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
#define StartTC_NonResGalvoCal_Err_InvalidParameter		-1
	
	ActiveNonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	// dim Save calibration
	SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_SaveCalib, ATTR_DIMMED, 1);  
	
	// check if settings are correct
	if (cal->commandVMax == 0.0) {
		*errorInfo = StrDup("Maximum command voltage cannot be 0");
		return StartTC_NonResGalvoCal_Err_InvalidParameter;
	}
	
	if (cal->minStepSize == 0.0) {
		*errorInfo = StrDup("Minimum step size voltage cannot be 0");
		return StartTC_NonResGalvoCal_Err_InvalidParameter;
	}
		
	if (cal->resolution == 0.0) {
		*errorInfo = StrDup("Minimum resolution voltage cannot be 0");
		return StartTC_NonResGalvoCal_Err_InvalidParameter;
	}
		
	if (cal->mechanicalResponse == 0.0) {
		*errorInfo = StrDup("Mechanical response cannot be 0");
		return StartTC_NonResGalvoCal_Err_InvalidParameter; 
	}
	
	if (cal->scanTime == 0.0) {
		*errorInfo = StrDup("Scan time cannot be 0");
		return StartTC_NonResGalvoCal_Err_InvalidParameter; 
	}
		
	// total number of iterations to start with
	SetTaskControlIterations(taskControl, 1);
	// set starting calibration type
	cal->currentCal = NonResGalvoCal_Slope_Offset;
	// reset iteration index
	cal->currIterIdx = 0;
	
	return 0; 
}

static int ResetTC_NonResGalvoCal (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	ActiveNonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	// dim Save calibration
	SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_SaveCalib, ATTR_DIMMED, 1);
	// set starting calibration type
	cal->currentCal = NonResGalvoCal_Slope_Offset;
	// reset iteration index
	cal->currIterIdx = 0;
	
	return 0; 
}

static int DoneTC_NonResGalvoCal (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	ActiveNonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	// calibration complete, undim Save calibration
	SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_SaveCalib, ATTR_DIMMED, 0);
	
	// cleanup
	discard_Waveform_type(&cal->positionWaveform);
	
	return 0; 
}

static int StoppedTC_NonResGalvoCal (TaskControl_type* taskControl,  BOOL const* abortFlag, char** errorInfo)
{
	ActiveNonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	// dim Save calibration since calibration is incomplete
	SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_SaveCalib, ATTR_DIMMED, 1);
	
	return 0; 
}

static int TaskTreeStatus_NonResGalvoCal (TaskControl_type* taskControl, TaskTreeExecution_type status, char** errorInfo)
{
	ActiveNonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	// dim/undim controls
	SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_Done, ATTR_DIMMED, (int) status);
    int calSetPanHndl;
	GetPanelHandleFromTabPage(cal->baseClass.calPanHndl, NonResGCal_Tab, 0, &calSetPanHndl);
	SetCtrlAttribute(calSetPanHndl, Cal_CommMaxV, ATTR_DIMMED, (int) status); 
	SetCtrlAttribute(calSetPanHndl, Cal_ParkedV, ATTR_DIMMED, (int) status); 
	SetCtrlAttribute(calSetPanHndl, Cal_ScanTime, ATTR_DIMMED, (int) status);
	SetCtrlAttribute(calSetPanHndl, Cal_MinStep, ATTR_DIMMED, (int) status); 
	SetCtrlAttribute(calSetPanHndl, Cal_Resolution, ATTR_DIMMED, (int) status);
	SetCtrlAttribute(calSetPanHndl, Cal_MechanicalResponse, ATTR_DIMMED, (int) status);
	
	return 0;
}

//-----------------------------------------
// LaserScanning Task Controller Callbacks
//-----------------------------------------

static int ConfigureTC_RectRaster (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	//RectRaster_type* engine = GetTaskControlModuleData(taskControl);
	
	//SetCtrlVal(engine->baseClass.scanSetPanHndl, RectRaster_Ready, 1);
	
	return 0; 
}

static int UnconfigureTC_RectRaster (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	//RectRaster_type* engine = GetTaskControlModuleData(taskControl);
	
	//SetCtrlVal(engine->baseClass.scanSetPanHndl, RectRaster_Ready, 0);
	
	return 0; 
}

static void	IterateTC_RectRaster (TaskControl_type* taskControl, BOOL const* abortIterationFlag)
{
	RectRaster_type* 	engine 			= GetTaskControlModuleData(taskControl);
	int					error 			= 0;
	char*				errMsg			= NULL;

	switch(engine->baseClass.scanMode) {
		
		case ScanEngineMode_Frames:
			
			if (engine->imgBuffers)
				// receive pixel stream and assemble image
				errChk( NonResRectRasterScan_BuildImage (engine, 0, &errMsg) );
			else
				// if there are no detection channels assigned, then do nothing
				TaskControlIterationDone(taskControl, 0, 0, FALSE);
			
			break;
			
		case ScanEngineMode_PointROIs:
			
			break;
	}
	
	// update iterations
	SetCtrlVal(engine->baseClass.scanSetPanHndl, RectRaster_FramesAcquired, (unsigned int) GetCurrentIterationIndex(GetTaskControlCurrentIter(engine->baseClass.taskControl)) );
	
	return;
	
Error:
	
	TaskControlIterationDone(taskControl, error, errMsg, FALSE);
	SetCtrlVal(engine->baseClass.scanSetPanHndl, RectRaster_FramesAcquired, (unsigned int) GetCurrentIterationIndex(GetTaskControlCurrentIter(engine->baseClass.taskControl)) );
	OKfree(errMsg);
}

static void AbortIterationTC_RectRaster (TaskControl_type* taskControl,BOOL const* abortFlag)
{
	TaskControlIterationDone(taskControl, 0, "", FALSE);   
}

static int StartTC_RectRaster (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	RectRaster_type* 		engine 				= GetTaskControlModuleData(taskControl);
	int						error 				= 0;
	
	// reset iterations display
	SetCtrlVal(engine->baseClass.scanSetPanHndl, RectRaster_FramesAcquired, 0);
	
	//--------------------------------------------------------------------------------------------------------
	// Open shutter
	//--------------------------------------------------------------------------------------------------------
	
	errChk( OpenScanEngineShutter(&engine->baseClass, 1, errorInfo) );
	
	//--------------------------------------------------------------------------------------------------------
	// Send galvo waveforms
	//--------------------------------------------------------------------------------------------------------
	
	errChk ( NonResRectRasterScan_GenerateScanSignals (engine, errorInfo) );  
	
	return 0; // no error
	
Error:
	
	// create out of memory message
	if (error == UIEOutOfMemory && !*errorInfo)
		*errorInfo = StrDup("Out of memory");
	
	return error;
}

static int DoneTC_RectRaster (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	RectRaster_type* 	engine		= GetTaskControlModuleData(taskControl);
	int					error		= 0;
	
	// close shutter
	errChk( OpenScanEngineShutter(&engine->baseClass, FALSE, errorInfo) );
	// return to parked position if continuous
	if (GetTaskControlMode(engine->baseClass.taskControl) == TASK_CONTINUOUS)
		errChk( ReturnRectRasterToParkedPosition(engine, errorInfo) );
	// update iterations
	SetCtrlVal(engine->baseClass.scanSetPanHndl, RectRaster_FramesAcquired, (unsigned int) GetCurrentIterationIndex(GetTaskControlCurrentIter(engine->baseClass.taskControl)) );
	
	return 0; 
	
Error:
	
	// create out of memory message
	if (error == UIEOutOfMemory && !*errorInfo)
		*errorInfo = StrDup("Out of memory");
	
	return error;
}

static int StoppedTC_RectRaster (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	RectRaster_type* 			engine 						= GetTaskControlModuleData(taskControl);
	int							error						= 0;
	
	// close shutter
	errChk( OpenScanEngineShutter(&engine->baseClass, FALSE, errorInfo) );
	// return to parked position
	errChk( ReturnRectRasterToParkedPosition(engine, errorInfo) );
	
	return 0;
	
Error:
	
	return error;
}

static int ResetTC_RectRaster (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	RectRaster_type* 	engine 		= GetTaskControlModuleData(taskControl);
	int					error		= 0;
	
	// reset acquired frames
	SetCtrlVal(engine->baseClass.scanSetPanHndl, RectRaster_FramesAcquired, 0);
	
	// close shutter
	errChk( OpenScanEngineShutter(&engine->baseClass, FALSE, errorInfo) ); 
	
	return 0;
	
Error:
	
	// create out of memory message
	if (error == UIEOutOfMemory && !*errorInfo)
		*errorInfo = StrDup("Out of memory");
	
	return error;
}

static int TaskTreeStatus_RectRaster (TaskControl_type* taskControl, TaskTreeExecution_type status, char** errorInfo)
{
	RectRaster_type* 		engine 				= GetTaskControlModuleData(taskControl);
	int						error				= 0;
	DetChan_type*			detChan				= NULL;
	size_t					nDetChans 			= ListNumItems(engine->baseClass.DetChans);
	SourceVChan_type*   	detSourceVChan		= NULL;
	DisplayEngine_type*		displayEngine		= engine->baseClass.lsModule->displayEngine;
	DLDataTypes				pixelDataType		= 0;
	ImageTypes				imageType			= 0;
	
	switch (status) {
			
		case TASK_TREE_STARTED:
			
			//--------------------------------------------------------------------------------------------------------
			// Discard image assembly buffers
			//--------------------------------------------------------------------------------------------------------
			
			for (size_t i = 0; i < engine->nImgBuffers; i++)
				discard_RectRasterImgBuffer_type(&engine->imgBuffers[i]);
			
			OKfree(engine->imgBuffers);
			engine->nImgBuffers = 0;
			
			//--------------------------------------------------------------------------------------------------------
			// Initialize image assembly buffers obtain display handles
			//--------------------------------------------------------------------------------------------------------
	
			// create new image assembly buffers for connected detector VChans
			for (size_t i = 1; i <= nDetChans; i++) {
				detChan = *(DetChan_type**) ListGetPtrToItem(engine->baseClass.DetChans, i);
				if (!(detSourceVChan = GetSourceVChan(detChan->detVChan))) continue;	// select only connected detection channels
		
				pixelDataType = GetSourceVChanDataType(detSourceVChan);
				switch (pixelDataType) {
				
					case DL_Waveform_UChar:
						imageType = Image_UChar;  
						break;
		
					case DL_Waveform_UShort:
						imageType = Image_UShort;    
						break;
			
					case DL_Waveform_Short:
						imageType = Image_Short;    
						break;
			
					case DL_Waveform_Float:
						imageType = Image_Float;     
						break;
			
					default:
					
					goto Error;
				}
		
				// get display handles for connected channels
				nullChk( detChan->displayHndl 	= (*displayEngine->getDisplayHandleFptr) (displayEngine, detChan, engine->scanSettings.width, engine->scanSettings.height, imageType) ); 
		
				engine->nImgBuffers++;
				// allocate memory for image assembly
				engine->imgBuffers = realloc(engine->imgBuffers, engine->nImgBuffers * sizeof(RectRasterImgBuffer_type*));
				engine->imgBuffers[engine->nImgBuffers - 1] = init_RectRasterImgBuffer_type(detChan, engine->scanSettings.width, engine->scanSettings.height, 
						(size_t)((engine->flyInDelay - engine->baseClass.pixDelay)/engine->scanSettings.pixelDwellTime), pixelDataType,FALSE, FALSE); 
			}
			
			break;
			
		case TASK_TREE_FINISHED:
			
			break;
	}
	
	return 0;

Error:
	
	// create out of memory message
	if (error == UIEOutOfMemory && !*errorInfo)
		*errorInfo = StrDup("Out of memory");
	
	return error;
}

/*
static int DataReceivedTC_RectRaster (TaskControl_type* taskControl, TaskStates_type taskState, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorInfo)
{
	RectRaster_type* engine = GetTaskControlModuleData(taskControl);
	
	return 0;   
}
*/

static void ErrorTC_RectRaster (TaskControl_type* taskControl, int errorID, char* errorMsg)
{
	//RectRaster_type* engine = GetTaskControlModuleData(taskControl);
	
}

static int ModuleEventHandler_RectRaster (TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, void* eventData, BOOL const* abortFlag, char** errorInfo)
{
	//RectRaster_type* engine = GetTaskControlModuleData(taskControl);
	
	return 0;
}
