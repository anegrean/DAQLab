
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
#include "DAQLabErrHandling.h"
#include "DAQLabUtility.h"
#include <formatio.h>  
#include "LaserScanning.h"
#include <userint.h>
#include "combobox.h" 
#include <analysis.h>
#include "WaveformDisplay.h"
#include "UI_LaserScanning.h"

//----------------------------------------------------------------------------
// Display module
// Choose only one of the include files below
//----------------------------------------------------------------------------
#include "ImageDisplayNIVision.h"
//#include "ImageDisplayCVI.h"
//----------------------------------------------------------------------------

									 

//==============================================================================
// Constants

	// maximum number of characters to represent a double precision number
#define MAX_DOUBLE_NCHARS									30

#define MOD_LaserScanning_UI 								"./Modules/Laser Scanning/UI_LaserScanning.uir"
#define VChanDataTimeout									1e4					// Timeout in [ms] for Sink VChans to receive data
#define TaskControllerIterationTimeout						20					// Timeout in [s] to complete the task controller iteration function

//--------------------------------------------------------------------------------
// Default VChan names
//--------------------------------------------------------------------------------

// Default Scan Engine VChan base names. Default base names must be unique among each other!
#define ScanEngine_SourceVChan_FastAxis_Command				"fast axis command"
#define ScanEngine_SourceVChan_FastAxis_Command_NSamples	"fast axis command n samples"
#define ScanEngine_SourceVChan_Galvo_Command_SamplingRate	"galvo command sampling rate"
#define ScanEngine_SinkVChan_FastAxis_Position				"fast axis position" 
#define ScanEngine_SourceVChan_SlowAxis_Command				"slow axis command"
#define ScanEngine_SourceVChan_SlowAxis_Command_NSamples	"slow axis command n samples"
#define ScanEngine_SinkVChan_SlowAxis_Position				"slow axis position"
#define ScanEngine_SourceVChan_CompositeImage				"composite image"			// Combined image channels.
#define ScanEngine_SourceVChan_ImageChannel					"image channel"				// Assembled image from a single detection channel. VChan of DL_Image type for frame scan and Allowed_Detector_Data_Types for point scan.
#define ScanEngine_SinkVChan_DetectionChan					"detection channel"			// Incoming fluorescence signal to assemble an image from.
#define ScanEngine_SourceVChan_PixelPulseTrain				"pixel pulse train"			// Source VChan of DL_PulseTrain_Ticks type
#define ScanEngine_SourceVChan_PixelSamplingRate			"pixel sampling rate"		// 1/pixel_dwell_time = pixel sampling rate in [Hz]
#define ScanEngine_SourceVChan_NPixels						"detection channel n pixels"
#define ScanEngine_SourceVChan_ROIHold						"ROI hold"
#define ScanEngine_SourceVChan_ROIShutter					"ROI shutter"

// Default Scan Axis Calibration VChan base names. Default base names must be unique among each other!
#define ScanAxisCal_SourceVChan_Command						"scan axis cal command"
#define ScanAxisCal_SinkVChan_Position						"scan axis cal position"
#define ScanAxisCal_SourceVChan_NSamples					"scan axis command n samples"

//--------------------------------------------------------------------------------

#define AxisCal_Default_TaskController_Name					"Scan Axis Calibration"
#define AxisCal_Default_NewCalibration_Name					"Calibration"

// Scan engine settings
#define Max_NewScanEngine_NameLength						50
#define Allowed_Detector_Data_Types							{DL_Waveform_UChar, DL_Waveform_UShort, DL_Waveform_Short, DL_Waveform_Float}  // DL_Waveform_UInt is not allowed because of NI Vision

// Non-resonant galvo calibration parameters
#define	CALIBRATION_DATA_TO_STRING							"%s<%*f[j1] "
#define STRING_TO_CALIBRATION_DATA							"%s>%*f[j1] "
#define MAX_CAL_NAME_LENGTH									50			// Maximum name for non-resonant galvo scan axis calibration data.
#define CALPOINTS 											50
#define SLOPE_OFFSET_DELAY									0.01		// Time to wait in [s] after a galvo step is made before estimating slope and offset parameters					
#define RELERR 												0.05
#define MINSNR												10
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
// Non-resonant galvo raster scan default & min max settings
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

	// UI
#define NonResGalvoRasterScan_FrameScanTabIDX				0
#define NonResGalvoRasterScan_PointScanTabIDX				1

	// Point ROI stimulation and recording
#define NonResGalvoRasterScan_Default_HoldTime				5.0			// Time in [ms] during which the galvos are stationary at this point ROI.
#define NonResGalvoRasterScan_Default_StimPulseONDuration   1.0			// Time in [ms] during which a stimulation pulse is delivered at the given ROI.
#define NonResGalvoRasterScan_Default_StimPulseOFFDuration	1.0			// If multiple stimulation pulses are delivered to the given ROI (burst), this is the idle time between stimulation pulses the burst.


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
	double* 				switchTime;				// Time in [ms] to make a jump of stepSize and settle within chosen step resolution.
	double*					delay;					// Delay in [ms] from command onset until the galvo starts switching its position.
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

typedef enum {
	// "grey"
	ScanChanColor_Grey,		// Default
	// "red"
	ScanChanColor_Red,
	// "green"
	ScanChanColor_Green,
	// "blue"
	ScanChanColor_Blue
} ScanChanColorScales;

#define ScanChanColor_ColIdx	1


typedef struct {
	SinkVChan_type*				detVChan;					// For receiving pixel data. See Allowed_Detector_Data_Types for VChan data types
	SourceVChan_type*			outputVChan;				// Assembled image or waveform for this channel. VChan of DL_Image type for frame scan and Allowed_Detector_Data_Types for point scan.
	ScanChanColorScales			color;						// Color channel assigned to this channel.
	ScanEngine_type*			scanEngine;					// Reference to scan engine to which this scan channel belongs.
	CmtTSVHandle				imgDisplayTSV;				// Thread safe variable of ImageDisplay_type*
	int							imgDisplayTSVLineNumDebug;	// Used for debug purposes to find out where the display lock is acquired and not released
	WaveformDisplay_type*		waveDisplay;				// Handle to display waveforms for this channel.
} ScanChan_type;

//---------------------------------------------------------------
// Scan Engine Class
//--------------------------------------------------------------- 
// scan engine child classes
typedef enum {
	ScanEngine_RectRaster_NonResonantGalvoFastAxis_NonResonantGalvoSlowAxis
} ScanEngineEnum_type;

//------------------------------------------------------------------------------------------
// Scan engine operation mode
#define ScanEngineMode_FrameScan_Name				"Frame scan"
#define ScanEngineMode_PointScan_Name				"Point scan"
typedef enum {
	ScanEngineMode_FrameScan	= NonResGalvoRasterScan_FrameScanTabIDX,	// Frame scans 
	ScanEngineMode_PointScan	= NonResGalvoRasterScan_PointScanTabIDX		// Jumps between point ROIs repeatedly
} ScanEngineModes;

typedef struct {
	ScanEngineModes				mode;
	char*						label;
	int							tabIdx;
} ScanEngineModes_type;

 ScanEngineModes_type			scanEngineModes[] = {	{.mode = ScanEngineMode_FrameScan, .label = ScanEngineMode_FrameScan_Name, .tabIdx = NonResGalvoRasterScan_FrameScanTabIDX}, 
	 													{.mode = ScanEngineMode_PointScan, .label = ScanEngineMode_PointScan_Name, .tabIdx = NonResGalvoRasterScan_PointScanTabIDX}	};
//------------------------------------------------------------------------------------------  
typedef struct {
	char*						objectiveName;				// Objective name.
	double						objectiveFL;				// Objective focal length in [mm].
} Objective_type;

typedef enum {
	
	ScanAxis_Fast,
	ScanAxis_Slow
	
} RasterScanAxes;

struct ScanEngine {
	//-----------------------------------
	// Scan engine type
	//-----------------------------------
	ScanEngineEnum_type			engineType;
	
	ScanEngineModes				scanMode;
	
	//-----------------------------------
	// Reference to axis calibration data
	//-----------------------------------
	ScanAxisCal_type*			fastAxisCal;				// Pixel index changes faster in this scan direction.
	ScanAxisCal_type*			slowAxisCal;
	
	//---------------------------------------
	// Reference to the Laser Scanning module
	//---------------------------------------
	LaserScanning_type*			lsModule;
	
	//-----------------------------------
	// Task Controller
	//-----------------------------------
	size_t						nFrames;					// Number of frames to acquire in finite frame scan mode, i.e. continuousAcq is False.
	BOOL						continuousAcq;				// If True, frame acquisition is continuous, and False, finite acquisition of frames.
	TaskControl_type*			taskControl;
	
	//-----------------------------------
	// VChans
	//-----------------------------------
	
	SourceVChan_type*			VChanFastAxisCom;   		// Command signals for scanning.
	SourceVChan_type*			VChanSlowAxisCom;
	
	SourceVChan_type*			VChanFastAxisComNSamples;   // Command signals waveform n samples.
	SourceVChan_type*			VChanSlowAxisComNSamples;
	
	SourceVChan_type*			VChanGalvoComSamplingRate;	// Sampling rate in [Hz] for the galvo command signal for both scan axes.
	
	SinkVChan_type*				VChanFastAxisPos;			// Position feedback signals (optional for some scan axis types)
	SinkVChan_type*				VChanSlowAxisPos;
	
	SourceVChan_type*			VChanPixelPulseTrain;		// Pixel pulse train.
	SourceVChan_type*			VChanPixelSamplingRate;		// Pixel sampling rate (same as pixel pulse train except this is just the sampling rate in [Hz])
	SourceVChan_type*			VChanNPixels;				// N pixels.
	
	ScanChan_type**				scanChans;					// Array of detector input channels of ScanChan_type* with incoming fluorescence pixel stream. 
	uInt32						nScanChans;
	
	SourceVChan_type*			VChanROIHold;				// ROI timing signal of DL_RepeatedWaveform_UChar type generated when the galvos are holding their position at a point ROI.
															// The signal starts with the galvos in their parked resting position having a value of 0 (FALSE) and once the galvos reach
															// a point ROI the signal has a value of 1 (TRUE) which remains TRUE until the galvos leave the point ROI to another point
															// ROI or back to their parked position, during which the signal is again FALSE.
															
	SourceVChan_type*			VChanROIShutter;			// ROI timing signal of DL_RepeatedWaveform_UChar type generated when a point ROI must be photostimulated or an area must be scanned. 
															// The signal starts with the galvos in their parked position with a value of 0 (FALSE) and takes a value of 1 (TRUE) when the laser must be on.
															// This signal can be sent for example to a Pockells module which will apply a certain stimulation intensity to it.
	
	SourceVChan_type*			VChanCompositeImage;		// Scan Engine composite RGB image output.
	
	//-----------------------------------
	// Image display
	//-----------------------------------
	
	ImageDisplay_type*			compositeImgDisplay;		// Handle to display composite RGB image.
	
	//-----------------------------------
	// Pixel assembly thread coordination
	//-----------------------------------
	
	CmtTSVHandle				nActivePixelBuildersTSV;	// Variable of int type. Thread safe variable indicating whether all channels have completed assembling their pixels in separate threads. 
															// Initially, this variable is equal to the number of used channels and its value decreses to 0 with each channel completing pixel assembly.
	
	//-----------------------------------
	// Scan Settings										// For performing frame scans.
	//-----------------------------------
	
	double						referenceClockFreq;			// Reference clock frequency in [Hz] that determines the smallest time unit in which the galvo and pixel sampling times can be divided.
	double						pixDelay;					// Pixel signal delay in [us] due to processing electronics measured from the moment the signal (fluorescence) is generated at the sample.
	double						shutterSwitchTime;			// Switch time in [us] for laser beam modulation shutter (pockells cell or similar) to blank the beam during scan area fly-in time and line scan turnaround.
	
	//-----------------------------------
	// Optics
	//-----------------------------------
	
	double						scanLensFL;					// Scan lens focal length in [mm]
	double						tubeLensFL;					// Tube lens focal length in [mm]
	Objective_type*				objectiveLens;				// Chosen objective lens
	ListType					objectives;					// List of microscope objectives of Objective_type* elements;
	
	//-----------------------------------
	// UI
	//-----------------------------------
	
	int							newObjectivePanHndl;
	int							scanPanHndl;				// Panel handle for the scan engine.
	int							frameScanPanHndl;			// Panel handle for adjusting scan settings such as pixel size, etc...
	int							pointScanPanHndl;			// Panel handle for the management of ROIs
	int							engineSetPanHndl;			// Panel handle for scan engine settings such as VChans and scan axis types.
	
	ImageDisplay_type*			activeDisplay;				// Reference to the active image display that interacts with the scan engine
	
	//-----------------------------------
	// Methods
	//-----------------------------------
	
	DiscardFptr_type			discardFptr;				// Discard method to be overridden by child class.

};

//------------------------
// Rectangular raster scan
//------------------------

typedef struct {
	void*						imagePixels;				// Pixel array for the image assembled so far. Array contains nImagePixels
	uInt64						nImagePixels;				// Total number of pixels in imagePixels. Maximum Array size is imgWidth*imgHeight
	uInt32						nAssembledRows;				// Number of assembled rows (in the direction of image height).
	void*						tmpPixels;					// Temporary pixels used to assemble an image. Array containing nTmpPixels elements
	size_t						nTmpPixels;					// Number of pixels in tmpPixels.
	size_t						nSkipPixels;				// Number of pixels left to skip from the pixel stream.
	BOOL				        flipRow;		   			// Flag used to flip every second row in the image in the width direction (fast-axis).
	BOOL						skipRows;					// If TRUE, then skipFlybackRows rows will be skipped.
	uInt32        				skipFlybackRows;			// Number of fast-axis rows to skip at the end of each frame while the slow axis returns to the beginning of the image.
	uInt32						rowsSkipped;				// Number of rows skipped so far.
	DLDataTypes					pixelDataType;
	ScanChan_type*				scanChan;					// Detection channel to which this image assembly buffer belongs.
	Image_type*					image;						// A completely assembled image, otherwise this is NULL.
} RectRasterImgBuff_type;


typedef struct {
	size_t						nSkipPixels;				// Number of pixels left to skip from the pixel stream.   
	Waveform_type*				rawPixels;					// Incoming pixel stream sampled at the galvo sampling rate.
	Waveform_type*				integratedPixels;			// Incoming pixel stream sampled at the galvo sampling rate.
	ScanChan_type*				scanChan;					// Detection channel to which this point scan buffer belongs.
} RectRasterPointBuff_type; 


typedef struct {
	double						pixSize;					// Image pixel size in [um].
	uInt32						height;						// Image height in [pix] when viewed on the screen.
	int							heightOffset;				// Image height offset from center in [pix].
	uInt32						width;						// Image width in [pix] when viewed on the screen.
	int							widthOffset;				// Image width offset in [pix].
	double						pixelDwellTime;				// Pixel dwell time in [us].
} RectRasterScanSet_type;

typedef enum {
	
	PointJump_SinglePoints,									// Point ROIs are visited one at a time, with the beam jumping from and returning to the parked position.
															// If Repeat is > 1 then each point is visited several times in the same manner before moving on to the next point and if the "start delay increment"
															// is != 0 then with each repetition, the start delay is incremented. Note that the total number of iterations of the task controller is the number of 
															// points times the number of repetitions for each point.
															
	PointJump_PointGroup,									// The beam jumps between the selected point ROIs starting from parked position, then to the first point, next point, until last point and finally returns 
															// to parked position. If Repeat > 1 then the point group is visited several times, and if "start delay increment" is != 0 then with each repetition, 
															// the start delay is increased. Note that in this case the number of task controller iterations is the same as the number of repetitions.
															
	PointJump_IncrementalPointGroup							// The beam jumps between an increasing number of selected point ROIs starting from the parked position. The total number of iterations is equal to the number
															// of selected point ROIs times the number of repetitions. If "startdelay increment" is !=0 then with each repetition the start delay is increased.
} PointJumpMethods;

// Settings for stimulation and recording from point ROIs
typedef struct {
	char*						protocolName;				// Name assigned to these settings.
	PointJumpMethods			jumpMethod;					// Determines point jump method.
	BOOL						record;						// If True, fluorescence signals from point ROIs are recorded while holding position.
	BOOL						stimulate;					// If True, then optical stimulation is applied to the point ROI.
	uInt32						nIntegration;				// Number of samples to use for integration of recorded fluorescence signal.
	uInt32						nSequenceRepeat;			// Number of times to repeat a sequence of ROI point jumps.
	double						startDelayInitVal;			// Scan start delay value for the first iteration.
	double						startDelayIncrement;		// Scan start delay increment in [ms] with each iteration.
	double						repeatWait;					// Additional time in [s] to wait between sequence repetitions.
	
	double						holdTime;					// Time in [ms] during which the galvos are stationary at this point ROI.
	uInt32						nHoldBurst;					// Number of hold events within a single sweep that share a common start timepoint, i.e. the control waveform is continuous. Default is 1.
	double						holdBurstPeriod;			// Elapsed time between hold events when holdBurstPeriodIncr is set to 0, otherwise it is the elapsed time between the first and second hold event.
															// The holdBurstPeriod cannot be shorter than the time needed to return the galvos at the end of each hold period to the parked position and then positioning them back again at the hold location.
	double						holdBurstPeriodIncr;		// Ammount of time to increment the holdBurstPeriod with each hold event. If this value is not zero, subsequent hold event occur at longer times from each other.
	double						stimDelay;					// Delay in [ms] measured from the moment the galvos are stationary at the point ROI to the moment a stimulation is initiated. 
															// This delay together with the stimulation duration is less than or equal to the holding time.
	double						stimPulseONDuration;		// Time in [ms] during which a stimulation pulse is delivered at the given ROI.
	double						stimPulseOFFDuration;		// If multiple stimulation pulses are delivered to the given ROI (burst), this is the idle time between stimulation pulses the burst.
	uInt32						nStimPulses;				// Number of stimulation pulses delivered to the ROI with stimPulseONDuration and stimPulseOFFDuration.
} PointScanProtocol_type;

// Child class of Point_type*
typedef struct {
	// Parent Class, must be first element
	Point_type					point;						// Point ROI.
	
	// DATA
	
} PointJump_type;

typedef struct {
	double						startDelay;					// Initial delay in [ms] from the global start trigger to the start of the first point jump sequence defined to be the beginning 
															// of the galvo hold time at the first point ROI in the first sequence. This initial delay is composed of a delay during which 
															// the galvos are kept at their parked position and a jump time needed to position the galvos to the first point ROI.
	double						minStartDelay;				// Minimum value for startDelay determined by the galvo jump speed in [ms].
	double						sequenceDuration;			// Time in [ms] during which the beam jumps between all the point ROIs in a sequence. This time is measured from the start of the first point
															// ROI holding time to the end holding time of the last point ROI in the sequence.
	double						minSequencePeriod;			// Minimum value of sequence period due to galvo jump speed in [ms].
	ListType					pointJumps;					// List of points to jump to of PointJump_type*. The order of point jumps is determined by the order of the elements in the list.
	size_t						currentActivePoint;			// 1-based index of current active point to visit when using the PointJump_SinglePoints mode.
	size_t						nPointsInGroup;				// Number of points in a group when using PointJump_IncrementalPointGroup mode.
	double*						jumpTimes;					// Array of galvo jump times between ROIs in [ms]. The first entry is the galvo jump time from the parked position to the first point ROI which does not include any additional start delay.
															// Note: a jump time is the sum of a galvo response delay and a switching time between two ROIs.
	double*						responseDelays;				// Array of galvo response delays in [ms] to a jump command. This value together with the switching time makes up a jump time.
	size_t						nJumpTimes;					// Number of jumpTimes array elements.
	PointScanProtocol_type*		pointScanProtocol;			// Protocol used for stimulation and recording from point ROIs.
	ListType					pointScanProtocols;			// List of PointScanProtocol_type* with predefined point scan settings.
} PointScan_type;

typedef struct {
	ScanEngine_type				baseClass;					// Base class, must be first structure member.
	
	//----------------------------------------------------
	// Frame scan mode
	//----------------------------------------------------
	
	RectRasterScanSet_type*		scanSettings;				// Scan settings used to perform a frame scan.
	RectRasterScanSet_type*		parentFrameScanSettings;	// Parent ROI frame scan settings restored from previous scans used to reference subregion scans defined in scanROIs or point scans.
	double						galvoSamplingRate;			// Default galvo sampling rate set by the scan engine in [Hz].
	double						flyInDelay;					// Galvo fly-in time from parked position to start of the imaging area in [us]. This value is also an integer multiple of the pixelDwellTime.
	ListType					scanROIs;					// List of Rect_type* scan ROIs marking subregions to scan within the parent scan area described by the scanSettings.
	
	//----------------------------------------------------
	// Point jump mode										// For jumping as fast as possible between a series of points and staying at or stimulating each point a given amount of time. 
	//----------------------------------------------------
	
	PointScan_type				pointScan;
	
	//----------------------------------------------------
	// Image and point scan buffers
	//----------------------------------------------------
	
	RectRasterImgBuff_type**	imgBuffers;					// Array of image buffers. Number of buffers and the buffer index is taken from the list of available (open) detection channels (baseClass.scanChans)
	size_t						nImgBuffers;				// Number of image buffers available.
	
	RectRasterPointBuff_type**  pointBuffers;				// Array of point scan buffers. Number of buffers and the buffer index is taken from the list of available (open) detection channels (baseClass.scanChans)
	size_t						nPointBuffers;				// Number of point scan buffers available. 
	

} RectRaster_type;

// scan engine image display data binding
typedef struct {
	RectRaster_type*			scanEngine;
	RectRasterScanSet_type*		scanSettings;
	size_t						scanChanIdx;				// Scan Engine channel display index.
} RectRasterDisplayCBData_type; 

// data structure binding used to pixel assembly from different channels in separate threads
typedef struct{
	RectRaster_type*			scanEngine;
	size_t 						channelIdx;
} PixelAssemblyBinding_type;

//----------------------
// Module implementation
//----------------------
struct LaserScanning {
	
	// SUPER, must be the first member to inherit from
	
	DAQLabModule_type 			baseClass;
	
	//-------------------------
	// DATA
	//-------------------------
	
		// Available calibration data per scan axis. Of ScanAxisCal_type* which can be casted to various child classes such as GalvoCal_type*
	ListType					availableCals;
		// Active calibrations that are in progress. Of ActiveScanAxisCal_type* which can be casted to various child classes such as GalvoCal_type*
	ListType					activeCal;
		// Scan engines of ScanEngine_type* base class which can be casted to specific scan engines. 
	ListType					scanEngines;
	
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
static ScanChan_type*					init_ScanChan_type									(ScanEngine_type* scanEngine, uInt32 chanIdx);

static void								discard_ScanChan_type								(ScanChan_type** scanChanPtr);

static int								RegisterDLScanChan									(ScanChan_type* scanChan);

static int								UnregisterDLScanChan								(ScanChan_type* scanChan);

//----------------------
// Scan axis calibration
//----------------------
	// generic active scan axis calibration data 

static ActiveScanAxisCal_type*			initalloc_ActiveScanAxisCal_type					(ActiveScanAxisCal_type* cal);

static void								discard_ActiveScanAxisCal_type						(ActiveScanAxisCal_type** cal);

	// generic scan axis calibration data
static void								discard_ScanAxisCal_type							(ScanAxisCal_type** cal);

void CVICALLBACK 						ScanAxisCalibrationMenu_CB							(int menuBarHandle, int menuItemID, void *callbackData, int panelHandle);

static void								UpdateScanEngineCalibrations						(ScanEngine_type* scanEngine);

static void 							UpdateAvailableCalibrations 						(LaserScanning_type* lsModule); 

static int CVICALLBACK 					ManageScanAxisCalib_CB								(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 					NewScanAxisCalib_CB									(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

	// galvo response analysis functions

static int 								FindSlope											(double* signal, size_t nsamples, double samplingRate, double signalStdDev, size_t nRep, double sloperelerr, double* maxslope); 
static int 								MeasureLag											(double* signal1, double* signal2, int nelem); 
static int 								GetStepTime 										(double* signal, size_t nSamples, double samplingRate, double lowLevel, double highLevel, double threshold, double* stepTimePtr, double* responseDelayPtr, char** errorInfo);

	//-----------------------------------------
	// Non-resonant galvo axis calibration data
	//-----------------------------------------

static ActiveNonResGalvoCal_type* 		init_ActiveNonResGalvoCal_type 						(LaserScanning_type* lsModule, char calName[], char commandVChanName[], char commandNSamplesVChanName[], char positionVChanName[]);

static void								discard_ActiveNonResGalvoCal_type					(ActiveScanAxisCal_type** cal);

static NonResGalvoCal_type*				init_NonResGalvoCal_type							(char calName[], LaserScanning_type* lsModule, double commandVMax, double slope, double offset, double posStdDev, 
																			 		 		double lag, SwitchTimes_type* switchTimes, MaxSlopes_type* maxSlopes, TriangleCal_type* triangleCal, double resolution, 
																							double minStepSize, double parked, double mechanicalResponse);

static void								discard_NonResGalvoCal_type							(NonResGalvoCal_type** cal);

	// updates optical settings
void									NonResGalvoCal_UpdateOptics							(NonResGalvoCal_type* cal);

	// validation for new scan axis calibration name
static BOOL 							ValidateNewScanAxisCal								(char inputStr[], void* dataPtr); 
	// switch times data
static SwitchTimes_type* 				init_SwitchTimes_type								(void);

static void 							discard_SwitchTimes_type 							(SwitchTimes_type** switchTimesPtr);

	// copies switch times data
static SwitchTimes_type*				copy_SwitchTimes_type								(SwitchTimes_type* switchTimes);

	// max slopes data

static MaxSlopes_type* 					init_MaxSlopes_type 								(void);

static void 							discard_MaxSlopes_type 								(MaxSlopes_type** maxSlopesPtr);
	// copies max slopes data
static MaxSlopes_type* 					copy_MaxSlopes_type 								(MaxSlopes_type* maxSlopes);

	// triangle wave calibration data

static TriangleCal_type* 				init_TriangleCal_type								(void);

static void 							discard_TriangleCal_type 							(TriangleCal_type** a);
	// copies triangle waveform calibration data
static TriangleCal_type* 				copy_TriangleCal_type 								(TriangleCal_type* triangleCal);
	// saves non resonant galvo scan axis calibration data to XML
static int 								SaveNonResGalvoCalToXML								(NonResGalvoCal_type* nrgCal, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ axisCalibrationsElement, ERRORINFO* xmlErrorInfo);
static int 								LoadNonResGalvoCalFromXML 							(LaserScanning_type* lsModule, ActiveXMLObj_IXMLDOMElement_ axisCalibrationElement, ERRORINFO* xmlErrorInfo);    

	// galvo calibration
static int CVICALLBACK 					NonResGalvoCal_MainPan_CB							(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static int CVICALLBACK 					NonResGalvoCal_CalPan_CB							(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static int CVICALLBACK 					NonResGalvoCal_TestPan_CB							(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

	// determines maximum frequency for a triangle waveform with given amplitude, taking into account the calibration data of the galvo
static void								MaxTriangleWaveformScan								(NonResGalvoCal_type* cal, double targetAmplitude, double* maxTriangleWaveFrequency);

	// determines galvo response lag (static+residual dynamic lag) for a triangle waveform  command signal of a given frequency in [ms]
//static double						TriangleWaveformLag									(NonResGalvoCal_type* cal, double triangleWaveFrequency);

	// Moves the galvo between two positions given by startVoltage and endVoltage in [V] given a sampleRate in [Hz] and calibration data.
	// The command signal is a ramp such that the galvo lags behind the command signal with a constant lag
static Waveform_type* 					NonResGalvoMoveBetweenPoints 						(NonResGalvoCal_type* cal, double sampleRate, double startVoltage, double endVoltage, double startDelay, double endDelay);
static int 								NonResGalvoPointJumpTime 							(NonResGalvoCal_type* cal, double jumpAmplitude, double* switchTimePtr, double* responseDelayPtr);
//static Waveform_type* 					NonResGalvoJumpBetweenPoints 						(NonResGalvoCal_type* cal, double sampleRate, double startVoltage, double endVoltage, double startDelay, double endDelay);

//-------------
// Scan Engines
//-------------
	// parent class
static int 								init_ScanEngine_type 								(ScanEngine_type** 			engine, 
								 														 	LaserScanning_type*			lsModule,
																						 	TaskControl_type**			taskController,
																							BOOL						continuousFrameScan,
								 															size_t						nFrames,
																						 	ScanEngineEnum_type 		engineType,
																						 	double						referenceClockFreq,
																						 	double						pixelDelay,
																							double						shutterSwitchTime,
																						 	double						scanLensFL,
																						 	double						tubeLensFL);	

static void								discard_ScanEngine_type 							(ScanEngine_type** enginePtr);

	// registers scan engine VChans and task controller with DAQLab framework
static int								DLRegisterScanEngine								(ScanEngine_type* engine);
	// unregisters scan engine VChans task controller from DAQLab framework
static void								DLUnregisterScanEngine								(ScanEngine_type* engine);

	//------------------------
	// Common functionality
	//------------------------

	// sends a voltage command to one of the scan axes
static int 								SendScanAxisCommand									(ScanEngine_type* engine, RasterScanAxes scanAxis, double voltage, char** errorMsg);

	//--------------------------------------
	// Non-Resonant Rectangle Raster Scan
	//--------------------------------------

static RectRasterScanSet_type*			init_RectRasterScanSet_type							(double pixSize, uInt32 height, int heightOffset, uInt32 width, int widthOffset, double pixelDwellTime); 

static void 							discard_RectRasterScanSet_type 						(RectRasterScanSet_type** scanSetPtr);

	// scan engine image display callback data binding
static RectRasterDisplayCBData_type* 	init_RectRasterDisplayCBData_type 					(RectRaster_type* scanEngine, size_t scanChanIdx);
static void 							discard_RectRasterDisplayCBData_type 				(RectRasterDisplayCBData_type** dataPtr);

static RectRaster_type*					init_RectRaster_type								(LaserScanning_type*		lsModule, 
																			 	 	 		char 						engineName[],
																							BOOL						continuousFrameScan,
																							size_t						nFrames,
																				 	 		double						galvoSamplingRate,
																				 	 		double						referenceClockFreq,
																					 		double						pixelDelay,
																							double						shutterSwitchTime,
																				 	 		RectRasterScanSet_type**	frameScanSettingsPtr,
																							PointScanProtocol_type**	pointScanProtocolPtr,
																							ListType*					pointScanProtocolsListPtr,
																				 	 		double						scanLensFL,
																				 	 		double						tubeLensFL);

static void								discard_RectRaster_type								(RectRaster_type** rectRasterPtr);

static int								SetRectRasterPointScanProtocol						(RectRaster_type* rectRaster, PointScanProtocol_type* pointScanProtocol);

static void 							SetRectRasterScanEnginePointScanStimulateUI			(int panel, BOOL stimulate);

static void 							SetRectRasterScanEnginePointScanRecordUI 			(int panel, BOOL record);

static void 							SetRectRasterScanEnginePointScanJumpModeUI 			(int panel, PointJumpMethods method);

static int 								InsertRectRasterScanEngineToUI 						(LaserScanning_type* ls, RectRaster_type* rectRaster, char** errorMsg);

	// Image scan pixel buffer
static RectRasterImgBuff_type* 			init_RectRasterImgBuff_type 						(ScanChan_type* scanChan, uInt32 imgHeight, uInt32 imgWidth, DLDataTypes pixelDataType, BOOL flipRows);
static void								discard_RectRasterImgBuff_type						(RectRasterImgBuff_type** imgBufferPtr); 
	// Resets the image scan buffe. Note: this function does not resize the buffer! For this, discard the buffer and re-initialize it.
static void 							ResetRectRasterImgBuffer 							(RectRasterImgBuff_type* imgBuffer, BOOL flipRows);

	// Point scan pixel buffer
static RectRasterPointBuff_type*		init_RectRasterPointBuff_type						(ScanChan_type* scanChan);
static void								discard_RectRasterPointBuff_type					(RectRasterPointBuff_type** pointBufferPtr);
	// Resets the point scan buffer.
static void 							ResetRectRasterPointBuffer 							(RectRasterPointBuff_type* pointBuffer);

	// return both the fast and slow axis galvos to their parked position
static int 								ReturnRectRasterToParkedPosition 					(RectRaster_type* engine, char** errorMsg); 

static PixelAssemblyBinding_type*		init_PixelAssemblyBinding_type						(RectRaster_type* rectRaster, size_t channelIdx);
static void								discard_PixelAssemblyBinding_type					(PixelAssemblyBinding_type** pixelBindingPtr);

	// starts pixel assembly for a scan channel
static int CVICALLBACK 					NonResRectRasterScan_LaunchPixelBuilder 			(void* functionData);

static int CVICALLBACK 					NonResRectRasterScan_MainPan_CB 					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 					NonResRectRasterScan_FrameScanPan_CB 				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

	// given a subregion frame scan 1-based list index from scanEngine->scanROIs list, this function sets the UI and scan engine data structures to scan the desired subregion 
static int								NonResRectRasterScan_SetSubregionFrameScan			(RectRaster_type* rectRaster, size_t subregionIdx, char** errorMsg);

static int CVICALLBACK 					NonResRectRasterScan_PointScanPan_CB		 		(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int 								NonResRectRasterScan_ScanWidths						(RectRaster_type* scanEngine, char** errorMsg);

static int								NonResRectRasterScan_PixelDwellTimes				(RectRaster_type* scanEngine, char** errorMsg);

	// determines whether a given rectangular ROI falls within a circular perimeter with radius
static BOOL 							RectROIInsideCircle									(double ROIHeight, double ROIWidth, double ROIHeightOffset, double ROIWidthOffset, double circleRadius);
	// determines whether a given rectangular ROI falls within another rectangle of size maxHeight and maxWidth
static BOOL								RectROIInsideRect									(double ROIHeight, double ROIWidth, double ROIHeightOffset, double ROIWidthOffset, double maxHeight, double maxWidth);

	// evaluates whether the scan engine configuration is valid, i.e. it has both scan axes assigned, objective, pixel sampling rate, etc.
static BOOL								NonResRectRasterScan_ValidConfig					(RectRaster_type* scanEngine);
	// evaluates whether the scan engine is ready to scan
static BOOL 							NonResRectRasterScan_ReadyToScan					(RectRaster_type* scanEngine);
	// generates rectangular raster scan waveforms and sends them to the galvo command VChans
static int								NonResRectRasterScan_GenerateScanSignals			(RectRaster_type* scanEngine, char** errorMsg);
	// builds images from a continuous pixel stream
static int 								NonResRectRasterScan_BuildImage 					(RectRaster_type* rectRaster, size_t bufferIdx, char** errorMsg);
	// assembles a composite frame scan image
static int								NonResRectRasterScan_AssembleCompositeImage			(RectRaster_type* rectRaster, char** errorMsg);
	// rounds a given time in [ms] to an integer of galvo sampling intervals
static double 							NonResRectRasterScan_RoundToGalvoSampling 			(RectRaster_type* scanEngine, double time);

	//--------------------------------------
	// Point jump mode
	//--------------------------------------

static PointScanProtocol_type* 			init_PointScanProtocol_type 						(char protocolName[], double holdTime, uInt32 nHoldBurst, double holdBurstPeriod, double holdBurstPeriodIncr, double stimDelay,
														    								 double stimPulseONDuration, double stimPulseOFFDuration, uInt32 nStimPulses, PointJumpMethods jumpMethod, 
																							 BOOL record, BOOL stimulate, uInt32 nIntegration, uInt32 nSequenceRepeat, double startDelayInitVal,
																							 double startDelayIncrement, double repeatWait);
static void 							discard_PointScanProtocol_type						(PointScanProtocol_type** pointScanProtocolPtr);
static PointScanProtocol_type*			copy_PointScanProtocol_type							(PointScanProtocol_type* pointScanProtocol);

	// applies scan engine point scan configuration to the UI
static int								SetPointScanProtocolUI								(RectRaster_type* rectRaster, BOOL updateProtocolsUIRing);

static BOOL								VerifyPointScanProtocolName							(char inputStr[], void* callbackData);

static PointJump_type* 					init_PointJump_type 								(Point_type* point);
static void								discard_PointJump_type								(PointJump_type** pointJumpPtr);
static PointJump_type* 					copy_PointJump_type 								(PointJump_type* pointJump);

	// generates galvo and ROI timing signals to jump between a series of points
static int								NonResRectRasterScan_GeneratePointJumpSignals 		(RectRaster_type* scanEngine, char** errorMsg);
	// sets the minimum jump delay from parked position to the first point ROI
static void								NonResRectRasterScan_SetMinimumPointJumpStartDelay 	(RectRaster_type* rectRaster);
	// calculates the minimum period in [ms] to complete a point jump cycle starting and ending at the parked location for both galvos
//static void 							NonResRectRasterScan_SetMinimumPointJumpPeriod 		(RectRaster_type* rectRaster);
	// convert a point ROI coordinate from a given scan setting to a command voltage for both scan axes
static void 							NonResRectRasterScan_PointROIVoltage 				(RectRaster_type* rectRaster, Point_type* point, double* fastAxisCommandV, double* slowAxisCommandV);
	// calculates the combined jump time for both scan axes to jump a given amplitude voltage in [V]
static int 								NonResRectRasterScan_JumpTime 						(RectRaster_type* rectRaster, double fastAxisAmplitude, double slowAxisAmplitude, double* jumpTimePtr, double* responseDelayPtr);
	// returns the number of active points that will be used for point jumping
static size_t							NonResRectRasterScan_GetNumActivePoints				(RectRaster_type* rectRaster);
	// assembles a point scan from a pixel stream
static int 								NonResRectRasterScan_BuildPointScan					(RectRaster_type* rectRaster, size_t bufferIdx, char** errorMsg);



	// closes the display of an image panel
//void CVICALLBACK 						DisplayClose_CB 									(int menuBar, int menuItem, void *callbackData, int panel);


//---------------------------------------------------------
// determines scan axis types based on the scan engine type
//---------------------------------------------------------
static void								GetScanAxisTypes									(ScanEngine_type* scanEngine, ScanAxis_type* fastAxisType, ScanAxis_type* slowAxisType); 

//-------------------------
// Objectives for scanning
//-------------------------
static Objective_type*					init_Objective_type									(char objectiveName[], double objectiveFL);

static void								discard_Objective_type								(Objective_type** objective);

static int CVICALLBACK 					NewObjective_CB										(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

//---------------------------------------------------------
// Waveforms
//---------------------------------------------------------

static Waveform_type* 					StaircaseWaveform 									(double sampleRate, size_t nSamplesPerStep, size_t nSteps, size_t delaySamples, double startVoltage, double stepVoltage);

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Module management
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

static int								Load 												(DAQLabModule_type* mod, int workspacePanHndl, char** errorMsg);

//----------------------------------------
// Load settings
//----------------------------------------

static int 								LoadCfg 											(DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_ moduleElement, ERRORINFO* xmlErrorInfo);

static int								LoadCfg_ScanAxisCalibrations						(LaserScanning_type* ls, ActiveXMLObj_IXMLDOMElement_ scanAxisCalibrationsXMLElement, ERRORINFO* xmlErrorInfo);

static int 								LoadCfg_ScanChannels 								(ScanEngine_type* scanEngine, ActiveXMLObj_IXMLDOMElement_ scanEngineXMLElement, ERRORINFO* xmlErrorInfo);

static int								LoadCfg_RectRaster_FrameScan						(RectRasterScanSet_type** scanSetPtr, ActiveXMLObj_IXMLDOMElement_ frameScanSettingsXMLElement, ERRORINFO* xmlErrorInfo);

static int								LoadCfg_PointScanProtocol							(PointScanProtocol_type** pointScanProtocolPtr, ActiveXMLObj_IXMLDOMElement_ pointScanProtocolXMLElement, ERRORINFO* xmlErrorInfo);

	// Returns a new list of PointScanProtocol_type* elements with loaded protocols if pointScanProtocols is set to 0 or modifies an existsing list. Also returns a selectedProtocol from the list if there is one, otherwise this parameter returns NULL.
static int 								LoadCfg_RectRaster_PointScanProtocols				(ListType* pointScanProtocolsPtr, PointScanProtocol_type** selectedProtocolPtr, ActiveXMLObj_IXMLDOMElement_ pointScanProtocolsXMLElement, ERRORINFO* xmlErrorInfo);

//----------------------------------------
// Save settings
//----------------------------------------

static int 								SaveCfg 											(DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_  moduleElement, ERRORINFO* xmlErrorInfo);

static int								SaveCfg_ScanAxisCalibrations						(LaserScanning_type* ls, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, ERRORINFO* xmlErrorInfo);

static int								SaveCfg_Objectives									(ScanEngine_type* scanEngine, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, ERRORINFO* xmlErrorInfo);

static int 								SaveCfg_ScanChannels 								(ScanEngine_type* scanEngine, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ scanEngineXMLElement, ERRORINFO* xmlErrorInfo);

static int								SaveCfg_RectRaster_FrameScan						(RectRasterScanSet_type* scanSet, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, ERRORINFO* xmlErrorInfo);

static int								SaveCfg_PointScanProtocol							(PointScanProtocol_type* pointScanProtocol, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, ERRORINFO* xmlErrorInfo);

static int								SaveCfg_RectRaster_PointScanProtocols				(RectRaster_type* rectRaster, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_* parentXMLElementPtr, ERRORINFO* xmlErrorInfo);

static int 								DisplayPanels										(DAQLabModule_type* mod, BOOL visibleFlag); 

static int CVICALLBACK 					ScanEngineSettings_CB 								(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 					ScanEnginesTab_CB 									(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

void CVICALLBACK 						ScanEngineSettingsMenu_CB							(int menuBarHandle, int menuItemID, void *callbackData, int panelHandle);

static void CVICALLBACK 				NewScanEngineMenu_CB								(int menuBar, int menuItem, void *callbackData, int panel);

static int CVICALLBACK 					NewScanEngine_CB 									(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static BOOL								ValidScanEngineName									(char name[], void* dataPtr);

//-----------------------------------------
// Scan Engine VChan management
//-----------------------------------------

	// Fast Axis Command VChan
static void								FastAxisComVChan_StateChange						(VChan_type* self, void* VChanOwner, VChanStates state);

	// Slow Axis Command VChan
static void								SlowAxisComVChan_StateChange						(VChan_type* self, void* VChanOwner, VChanStates state);

	// Galvo sampling rate for both scan axes
static void								GalvoComSampleRateVChan_StateChange 				(VChan_type* self, void* VChanOwner, VChanStates state);

	// Detection Channels
static void								DetectionVChan_StateChange							(VChan_type* self, void* VChanOwner, VChanStates state); 

	// Scan engine mode switching VChan activation/deactivation
static void 							SetRectRasterScanEngineModeVChans 					(RectRaster_type* scanEngine);

static uInt32							GetNumOpenDetectionVChans							(RectRaster_type* scanEngine);

//-----------------------------------------
// Display interface
//-----------------------------------------

static void 							ImageDisplay_CB 									(ImageDisplay_type* imgDisplay, int event, void* eventData, void* callbackFunctionData);

static void 							WaveformDisplay_CB 									(WaveformDisplay_type* waveformDisplay, int event, void* callbackData);

static void								RestoreScanSettingsFromImageDisplay					(ImageDisplay_type* imgDisplay, RectRaster_type* scanEngine, RectRasterScanSet_type* previousScanSettings); 

//-----------------------------------------
// Task Controller Callbacks
//-----------------------------------------

	// for Non Resonant Galvo scan axis calibration and testing
static int								ConfigureTC_NonResGalvoCal							(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);

static int								UncofigureTC_NonResGalvoCal							(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);

static void								IterateTC_NonResGalvoCal							(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortIterationFlag);

static int								StartTC_NonResGalvoCal								(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);

static int								DoneTC_NonResGalvoCal								(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg);

static int								StoppedTC_NonResGalvoCal							(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg);

static int				 				ResetTC_NonResGalvoCal 								(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);

static int								TaskTreeStateChange_NonResGalvoCal 					(TaskControl_type* taskControl, TaskTreeStates state, char** errorMsg);

//static int				 			DataReceivedTC_NonResGalvoCal 					(TaskControl_type* taskControl, TCStates taskState, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorMsg);

	// for RectRaster_type scanning (frame scan and point scan)

static int								ConfigureTC_RectRaster								(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);

static int								UnconfigureTC_RectRaster							(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);

static void								IterateTC_RectRaster								(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortIterationFlag);

static int								StartTC_RectRaster									(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);

static int								DoneTC_RectRaster									(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg);

static int								StoppedTC_RectRaster								(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg);

static int				 				ResetTC_RectRaster 									(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);

static int								TaskTreeStateChange_RectRaster 						(TaskControl_type* taskControl, TaskTreeStates state, char** errorMsg);

//static int				 			DataReceivedTC_RectRaster						(TaskControl_type* taskControl, TCStates taskState, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorMsg);

static void 							ErrorTC_RectRaster 									(TaskControl_type* taskControl, int errorID, char* errorMsg);

static int								ModuleEventHandler_RectRaster						(TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, void* eventData, BOOL const* abortFlag, char** errorMsg);

static void								SetRectRasterTaskControllerSettings					(RectRaster_type* scanEngine);

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
		size_t 				nItems 	= ListNumItems(ls->availableCals);
		ScanAxisCal_type**  calPtr	= NULL;
		for (size_t i = 1; i <= nItems; i++) {
			calPtr = ListGetPtrToItem(ls->availableCals, i);
			(*(*calPtr)->Discard)	(calPtr); 
		}
		
		ListDispose(ls->availableCals);
	}
	
	// active calibrations
	if (ls->activeCal) {
		size_t 						nItems 	= ListNumItems(ls->activeCal);
		ActiveScanAxisCal_type**  	calPtr	= NULL;
		for (size_t i = 1; i <= nItems; i++) {
			calPtr = ListGetPtrToItem(ls->activeCal, i);
			(*(*calPtr)->Discard)	(calPtr); 
		}
		
		ListDispose(ls->activeCal);
	}
	
	// scan engines
	if (ls->scanEngines) {
		size_t 				nItems 		= ListNumItems(ls->scanEngines);
		ScanEngine_type**  	enginePtr	= NULL;
		for (size_t i = 1; i <= nItems; i++) {
			enginePtr = ListGetPtrToItem(ls->scanEngines, i);
			DLUnregisterScanEngine(*enginePtr);
			(*(*enginePtr)->discardFptr) ((void**)enginePtr);
		}
		
		ListDispose(ls->scanEngines);
	}
	
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

static void SetRectRasterScanEnginePointScanStimulateUI (int panel, BOOL stimulate)
{
	if (stimulate) {
		SetCtrlAttribute(panel, PointTab_StimDelay, ATTR_DIMMED, FALSE);
		SetCtrlAttribute(panel, PointTab_NPulses, ATTR_DIMMED, FALSE);
		SetCtrlAttribute(panel, PointTab_PulseON, ATTR_DIMMED, FALSE);
		SetCtrlAttribute(panel, PointTab_PulseOFF, ATTR_DIMMED, FALSE);
	} else {
		SetCtrlAttribute(panel, PointTab_StimDelay, ATTR_DIMMED, TRUE);
		SetCtrlAttribute(panel, PointTab_NPulses, ATTR_DIMMED, TRUE);
		SetCtrlAttribute(panel, PointTab_PulseON, ATTR_DIMMED, TRUE);
		SetCtrlAttribute(panel, PointTab_PulseOFF, ATTR_DIMMED, TRUE);
	}
}

static void SetRectRasterScanEnginePointScanRecordUI (int panel, BOOL record)
{
	if (record)
		SetCtrlAttribute(panel, PointTab_NIntegration, ATTR_DIMMED, FALSE);
	else
		SetCtrlAttribute(panel, PointTab_NIntegration, ATTR_DIMMED, TRUE);
}

static void SetRectRasterScanEnginePointScanJumpModeUI (int panel, PointJumpMethods method)
{
	switch (method) {
			
		case PointJump_SinglePoints:
			
			SetCtrlAttribute(panel, PointTab_Record, ATTR_DIMMED, FALSE);
			
			break;
			
		case PointJump_PointGroup:
		case PointJump_IncrementalPointGroup:
			
			SetCtrlAttribute(panel, PointTab_Record, ATTR_DIMMED, TRUE);
			
			break;
	}
	
	
}

static int InsertRectRasterScanEngineToUI (LaserScanning_type* ls, RectRaster_type* rectRaster, char** errorMsg)
{
INIT_ERR

	int							scanEnginePanHndl	= 0;
	int							newTabIdx			= 0;
	char*						scanEngineName		= NULL; 
	
	
	//------------------------------------------------------------------------------------------------------------------------------------------------------ 
	// Prepare scan engine UI
	//------------------------------------------------------------------------------------------------------------------------------------------------------
	
	errChk( scanEnginePanHndl = LoadPanel(ls->mainPanHndl, MOD_LaserScanning_UI, RectRaster) );
	
	errChk( newTabIdx = InsertPanelAsTabPage(ls->mainPanHndl, ScanPan_ScanEngines, -1, scanEnginePanHndl) );
	OKfreePanHndl(scanEnginePanHndl);
	
	// set scan engine tab title
	nullChk( scanEngineName = GetTaskControlName(rectRaster->baseClass.taskControl) ); 
	SetTabPageAttribute(ls->mainPanHndl, ScanPan_ScanEngines, newTabIdx, ATTR_LABEL_TEXT, scanEngineName);
	OKfree(scanEngineName);
	
	// get scan engine panel handle
	GetPanelHandleFromTabPage(ls->mainPanHndl, ScanPan_ScanEngines, newTabIdx, &rectRaster->baseClass.scanPanHndl);
	// get frame scan panel handle
	GetPanelHandleFromTabPage(rectRaster->baseClass.scanPanHndl, RectRaster_Tab, NonResGalvoRasterScan_FrameScanTabIDX, &rectRaster->baseClass.frameScanPanHndl);
	// get point scan panel handle
	GetPanelHandleFromTabPage(rectRaster->baseClass.scanPanHndl, RectRaster_Tab, NonResGalvoRasterScan_PointScanTabIDX, &rectRaster->baseClass.pointScanPanHndl);
	
	// set scan mode
	SetActiveTabPage(rectRaster->baseClass.scanPanHndl, RectRaster_Tab, rectRaster->baseClass.scanMode);
	
	// add scan engine panel callback data
	SetPanelAttribute(rectRaster->baseClass.scanPanHndl, ATTR_CALLBACK_DATA, rectRaster);
	
	// add callbacks to UI
	SetCtrlsInPanCBInfo(rectRaster, NonResRectRasterScan_MainPan_CB, rectRaster->baseClass.scanPanHndl);
	SetCtrlsInPanCBInfo(rectRaster, NonResRectRasterScan_FrameScanPan_CB, rectRaster->baseClass.frameScanPanHndl);
	SetCtrlsInPanCBInfo(rectRaster, NonResRectRasterScan_PointScanPan_CB, rectRaster->baseClass.pointScanPanHndl);
				
	//------------------------------------------------------------------------------------------------------------------------------------------------------ 
	// Set up Frame Scan UI
	//------------------------------------------------------------------------------------------------------------------------------------------------------ 
	
	// update operation mode and dim N Frames if necessary
	SetCtrlVal(rectRaster->baseClass.frameScanPanHndl, ScanTab_ExecutionMode, rectRaster->baseClass.continuousAcq);
	if (rectRaster->baseClass.continuousAcq)
		SetCtrlAttribute(rectRaster->baseClass.frameScanPanHndl, ScanTab_NFrames, ATTR_DIMMED, TRUE);
	else
		SetCtrlAttribute(rectRaster->baseClass.frameScanPanHndl, ScanTab_NFrames, ATTR_DIMMED, FALSE);
					
	// update number of frames
	SetCtrlVal(rectRaster->baseClass.frameScanPanHndl, ScanTab_NFrames, (unsigned int) rectRaster->baseClass.nFrames);
							
	// update objectives
	size_t				nObjectives			= ListNumItems(rectRaster->baseClass.objectives);
	Objective_type*		objective			= NULL;
	for (size_t i = 1; i <= nObjectives; i++) {
		objective = *(Objective_type**)ListGetPtrToItem(rectRaster->baseClass.objectives, i);
		InsertListItem(rectRaster->baseClass.frameScanPanHndl, ScanTab_Objective, -1, objective->objectiveName, objective->objectiveFL);
		// select assigned objective
		if (!strcmp(objective->objectiveName, rectRaster->baseClass.objectiveLens->objectiveName))
		SetCtrlIndex(rectRaster->baseClass.frameScanPanHndl, ScanTab_Objective, i-1);
	}
	
	// add default parent frame scan ROI
	InsertListItem(rectRaster->baseClass.frameScanPanHndl, ScanTab_ROIs, 0, "parent", 0);
	// check default parent frame scan ROI
	CheckListItem(rectRaster->baseClass.frameScanPanHndl, ScanTab_ROIs, 0, 1);
				
	// update height
	SetCtrlVal(rectRaster->baseClass.frameScanPanHndl, ScanTab_Height, rectRaster->scanSettings->height * rectRaster->scanSettings->pixSize);
				
	// update height and width offsets
	SetCtrlVal(rectRaster->baseClass.frameScanPanHndl, ScanTab_HeightOffset, rectRaster->scanSettings->heightOffset * rectRaster->scanSettings->pixSize); 
	SetCtrlVal(rectRaster->baseClass.frameScanPanHndl, ScanTab_WidthOffset, rectRaster->scanSettings->widthOffset * rectRaster->scanSettings->pixSize);
				
	// update pixel size & set boundaries
	SetCtrlVal(rectRaster->baseClass.frameScanPanHndl, ScanTab_PixelSize, rectRaster->scanSettings->pixSize);
	SetCtrlAttribute(rectRaster->baseClass.frameScanPanHndl, ScanTab_PixelSize, ATTR_MIN_VALUE, NonResGalvoRasterScan_Min_PixelSize);
	SetCtrlAttribute(rectRaster->baseClass.frameScanPanHndl, ScanTab_PixelSize, ATTR_MAX_VALUE, NonResGalvoRasterScan_Max_PixelSize);
	SetCtrlAttribute(rectRaster->baseClass.frameScanPanHndl, ScanTab_PixelSize, ATTR_CHECK_RANGE, VAL_COERCE);
	
	// make width string control into combo box
	// do this after calling SetCtrlsInPanCBInfo, otherwise the combobox is disrupted
	errChk( Combo_NewComboBox(rectRaster->baseClass.frameScanPanHndl, ScanTab_Width) );
	errChk( Combo_SetComboAttribute(rectRaster->baseClass.frameScanPanHndl, ScanTab_Width, ATTR_COMBO_MAX_LENGTH, NonResGalvoRasterScan_Max_ComboboxEntryLength) );
	
	char widthString[NonResGalvoRasterScan_Max_ComboboxEntryLength+1];
	Fmt(widthString, "%s<%f[p1]", rectRaster->scanSettings->width * rectRaster->scanSettings->pixSize);
	if (rectRaster->scanSettings->width)
		SetCtrlVal(rectRaster->baseClass.frameScanPanHndl, ScanTab_Width, widthString); 
	else
		SetCtrlVal(rectRaster->baseClass.frameScanPanHndl, ScanTab_Width, ""); 
				
	// make Pixel Dwell time string control into combo box
	// do this after calling SetCtrlsInPanCBInfo, otherwise the combobox is disrupted  
	errChk( Combo_NewComboBox(rectRaster->baseClass.frameScanPanHndl, ScanTab_PixelDwell) );
	errChk( Combo_SetComboAttribute(rectRaster->baseClass.frameScanPanHndl, ScanTab_PixelDwell, ATTR_COMBO_MAX_LENGTH, NonResGalvoRasterScan_Max_ComboboxEntryLength) );
				
	// update pixel dwell time
	char pixelDwellString[NonResGalvoRasterScan_Max_ComboboxEntryLength+1];
	Fmt(pixelDwellString, "%s<%f[p1]", rectRaster->scanSettings->pixelDwellTime);
	if (rectRaster->scanSettings->pixelDwellTime != 0.0)
		SetCtrlVal(rectRaster->baseClass.frameScanPanHndl, ScanTab_PixelDwell, pixelDwellString);
	else
		SetCtrlVal(rectRaster->baseClass.frameScanPanHndl, ScanTab_PixelDwell, "");
	
	//------------------------------------------------------------------------------------------------------------------------------------------------------ 
	// Set up Point Scan UI
	//------------------------------------------------------------------------------------------------------------------------------------------------------ 
	
	SetPointScanProtocolUI(rectRaster, TRUE);
	
	// configure/unconfigure scan engine
	// do this after adding combo boxes!
	if (NonResRectRasterScan_ValidConfig(rectRaster)) {
				
		errChk( NonResRectRasterScan_ScanWidths(rectRaster, &errorInfo.errMsg) );
		errChk( NonResRectRasterScan_PixelDwellTimes(rectRaster, &errorInfo.errMsg) );
					
		if (NonResRectRasterScan_ReadyToScan(rectRaster)) 
			errChk( TaskControlEvent(rectRaster->baseClass.taskControl, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
		else
			errChk( TaskControlEvent(rectRaster->baseClass.taskControl, TC_Event_Unconfigure, NULL, NULL, &errorInfo.errMsg) ); 	
	} else
		errChk( TaskControlEvent(rectRaster->baseClass.taskControl, TC_Event_Unconfigure, NULL, NULL, &errorInfo.errMsg) );
	
	
Error:
	
RETURN_ERR
}

static int Load (DAQLabModule_type* mod, int workspacePanHndl, char** errorMsg)
{
INIT_ERR

	LaserScanning_type*		ls						= (LaserScanning_type*) mod;
	int						newMenuItem				= 0;
	int						settingsMenuItemHndl	= 0;
	
	
	//----------------------------------------------------------------------
	// load module
	//----------------------------------------------------------------------
	
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
	ScanEngine_type*	scanEngine			= NULL;
	size_t				nScanEngines 		= ListNumItems(ls->scanEngines);
	
	for (size_t i = 1; i <= nScanEngines; i++) {
		scanEngine = *(ScanEngine_type**)ListGetPtrToItem(ls->scanEngines, i);
		// load scan pannel depending on scan engine type
		switch (scanEngine->engineType) {
			//------------------------------------------------------------------------------------------------------------------------------------------------------
			// ScanEngine_RectRaster_NonResonantGalvoFastAxis_NonResonantGalvoSlowAxis
			//------------------------------------------------------------------------------------------------------------------------------------------------------ 
			case ScanEngine_RectRaster_NonResonantGalvoFastAxis_NonResonantGalvoSlowAxis:
				
				// add UI
				errChk( InsertRectRasterScanEngineToUI(ls, (RectRaster_type*)scanEngine, &errorInfo.errMsg) );
				
				// configure VChans depending on scan engine mode
				SetRectRasterScanEngineModeVChans((RectRaster_type*)scanEngine);
				
				break;
				
			// insert here more scan engine types
		}
		
		DLRegisterScanEngine(scanEngine);
		
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
	
	return 0;
	
Error:
	
	// cleanup
	OKfreePanHndl(ls->mainPanHndl);
	
RETURN_ERR
}

static int SaveCfg (DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ moduleElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	LaserScanning_type*				ls 							= (LaserScanning_type*) mod;
	int								lsPanTopPos					= 0;
	int								lsPanLeftPos				= 0;
	DAQLabXMLNode 					lsAttr[] 					= {	{"PanTopPos", BasicData_Int, &lsPanTopPos},
											  		   				{"PanLeftPos", BasicData_Int, &lsPanLeftPos} };
	size_t							nScanEngines 				= ListNumItems(ls->scanEngines);
	ScanEngine_type*				scanEngine					= NULL;
	ActiveXMLObj_IXMLDOMElement_	scanEnginesXMLElement		= 0; 			  	// Element containing multiple scan engines.
	ActiveXMLObj_IXMLDOMElement_	scanEngineXMLElement		= 0; 			  	// Scan engine element.
	
	
	//--------------------------------------------------------------------------
	// Save laser scanning module main panel position
	//--------------------------------------------------------------------------
	
	errChk( GetPanelAttribute(ls->mainPanHndl, ATTR_LEFT, &lsPanLeftPos) );
	errChk( GetPanelAttribute(ls->mainPanHndl, ATTR_TOP, &lsPanTopPos) );
	errChk( DLAddToXMLElem(xmlDOM, moduleElement, lsAttr, DL_ATTRIBUTE, NumElem(lsAttr), xmlErrorInfo) ); 
	
	//--------------------------------------------------------------------------
	// Save scan axis calibrations
	//--------------------------------------------------------------------------
	errChk( SaveCfg_ScanAxisCalibrations(ls, xmlDOM, moduleElement, xmlErrorInfo) );
	
	//--------------------------------------------------------------------------
	// Save scan engines
	//--------------------------------------------------------------------------
	
	// create scan engines element
	errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "ScanEngines", &scanEnginesXMLElement) );
	
	for (size_t i = 1; i <= nScanEngines; i++) {
		scanEngine = *(ScanEngine_type**) ListGetPtrToItem(ls->scanEngines, i);
		// create new scan engine element
		errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "ScanEngine", &scanEngineXMLElement) );
		// initialize generic scan engine attributes
		char*			tcName				= GetTaskControlName(scanEngine->taskControl);
		char*			fastAxisCalName		= NULL;
		char*			slowAxisCalName		= NULL;
		char*			objectiveName		= NULL;
		unsigned int 	nFrames				= scanEngine->nFrames;
		unsigned int	scanEngineType		= scanEngine->engineType;
		
		if (scanEngine->fastAxisCal)
			fastAxisCalName	= scanEngine->fastAxisCal->calName;
		else
			fastAxisCalName	= "";
		
		if (scanEngine->slowAxisCal)
			slowAxisCalName	= scanEngine->slowAxisCal->calName;
		else
			slowAxisCalName	= "";
		
		if (scanEngine->objectiveLens)
			objectiveName	= scanEngine->objectiveLens->objectiveName;
		else
			objectiveName 	= "";
		
		
		DAQLabXMLNode 	scanEngineAttr[] 	= {	{"Name", 						BasicData_CString, 		tcName},
												{"ContinuousFrameScan",			BasicData_Bool, 		&scanEngine->continuousAcq},
												{"NFrames", 					BasicData_UInt, 		&nFrames},
												{"ScanEngineType", 				BasicData_UInt, 		&scanEngineType},
												{"FastAxisCalibrationName", 	BasicData_CString, 		fastAxisCalName},
												{"SlowAxisCalibrationName", 	BasicData_CString, 		slowAxisCalName},
												{"ScanLensFL", 					BasicData_Double, 		&scanEngine->scanLensFL},
												{"TubeLensFL", 					BasicData_Double, 		&scanEngine->tubeLensFL},
												{"Objective", 					BasicData_CString, 		objectiveName},
												{"PixelClockRate", 				BasicData_Double, 		&scanEngine->referenceClockFreq},
												{"PixelSignalDelay", 			BasicData_Double, 		&scanEngine->pixDelay},
												{"ShutterSwitchTime",			BasicData_Double,		&scanEngine->shutterSwitchTime} };
												
		// save attributes
		errChk( DLAddToXMLElem(xmlDOM, scanEngineXMLElement, scanEngineAttr, DL_ATTRIBUTE, NumElem(scanEngineAttr), xmlErrorInfo) );
		OKfree(tcName);
		
		// save channels
		errChk( SaveCfg_ScanChannels(scanEngine, xmlDOM, scanEngineXMLElement, xmlErrorInfo) ); 
		
		// save objectives
		errChk( SaveCfg_Objectives(scanEngine, xmlDOM, scanEngineXMLElement, xmlErrorInfo) );
		
		// add scan engine specific data
		switch(scanEngine->engineType) {
				
			case ScanEngine_RectRaster_NonResonantGalvoFastAxis_NonResonantGalvoSlowAxis:
				
				RectRaster_type*		rectRaster			 	= (RectRaster_type*) scanEngine;
				DAQLabXMLNode			rectangleRasterAttr[] 	= { {"GalvoSamplingRate", 	BasicData_Double, 	&rectRaster->galvoSamplingRate} }; 				
				
				// add rectangular raster scan attributes to the generic scan engine element
				errChk( DLAddToXMLElem(xmlDOM, scanEngineXMLElement, rectangleRasterAttr, DL_ATTRIBUTE, NumElem(rectangleRasterAttr), xmlErrorInfo) );
				
				// save frame scan settings
				errChk( SaveCfg_RectRaster_FrameScan(rectRaster->scanSettings, xmlDOM, scanEngineXMLElement, xmlErrorInfo) );
				
				// save point scan protocols
				errChk( SaveCfg_RectRaster_PointScanProtocols(rectRaster, xmlDOM, &scanEngineXMLElement, xmlErrorInfo) );
				
				break;
		}
	
		// add new scan engine element
		errChk ( ActiveXML_IXMLDOMElement_appendChild (scanEnginesXMLElement, xmlErrorInfo, scanEngineXMLElement, NULL) );  
		OKfreeCAHndl(scanEngineXMLElement); 
	}
	
	// add scan engines element to module element
	errChk ( ActiveXML_IXMLDOMElement_appendChild (moduleElement, xmlErrorInfo, scanEnginesXMLElement, NULL) );
	OKfreeCAHndl(scanEnginesXMLElement); 
	
	return 0;
	
Error:
	
	// cleanup
	OKfreeCAHndl(scanEnginesXMLElement); 
	OKfreeCAHndl(scanEngineXMLElement); 
	
	return errorInfo.error;
	
}

static int SaveCfg_ScanAxisCalibrations (LaserScanning_type* ls, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, ERRORINFO* xmlErrorInfo)
{  
INIT_ERR

	ActiveXMLObj_IXMLDOMElement_	scanAxisCalibrationsXMLElement	= 0;   			// element containing multiple scan axis calibrations
	ActiveXMLObj_IXMLDOMElement_	scanAxisCalXMLElement			= 0;  		  	// element containing calibration data
	size_t							nAxisCals 						= ListNumItems(ls->availableCals);
	ScanAxisCal_type*				axisCal							= NULL;
	
	// create scan axis calibration element
	errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "ScanAxisCalibrations", &scanAxisCalibrationsXMLElement) );
	
	for (size_t i = 1; i <= nAxisCals; i++) {
		axisCal = *(ScanAxisCal_type**) ListGetPtrToItem(ls->availableCals, i);
		// create new axis calibration element
		errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "AxisCalibration", &scanAxisCalXMLElement) );
		// initialize generic scan axis calibration attributes
		unsigned int		scanAxisType		= (unsigned int)axisCal->scanAxisType;   
		DAQLabXMLNode 		scanAxisCalAttr[] 	= {	{"Name",		BasicData_CString,		axisCal->calName},
													{"AxisType",	BasicData_UInt,			&scanAxisType} };
		
		// save attributes
		errChk( DLAddToXMLElem(xmlDOM, scanAxisCalXMLElement, scanAxisCalAttr, DL_ATTRIBUTE, NumElem(scanAxisCalAttr), xmlErrorInfo) );
		
		// add scan axis specific calibration data
		switch(axisCal->scanAxisType) {
				
			case NonResonantGalvo:
				errChk( SaveNonResGalvoCalToXML((NonResGalvoCal_type*) axisCal, xmlDOM, scanAxisCalXMLElement, xmlErrorInfo) );
				break;
				
			case ResonantGalvo:
				break;
				
			case AOD:
				break;
				
			case Translation:
				break;
	
		}
		
		// add new axis calibration element
		errChk ( ActiveXML_IXMLDOMElement_appendChild (scanAxisCalibrationsXMLElement, xmlErrorInfo, scanAxisCalXMLElement, NULL) );  
		OKfreeCAHndl(scanAxisCalXMLElement);  
	}
	
	// add scan axis calibrations element to parent element
	errChk ( ActiveXML_IXMLDOMElement_appendChild (parentXMLElement, xmlErrorInfo, scanAxisCalibrationsXMLElement, NULL) );
	OKfreeCAHndl(scanAxisCalibrationsXMLElement);
	
	return 0;
	
Error:
	
	// cleanup
	OKfreeCAHndl(scanAxisCalibrationsXMLElement);
	OKfreeCAHndl(scanAxisCalXMLElement);
	
	return errorInfo.error;
}

static int SaveCfg_Objectives (ScanEngine_type* scanEngine, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	ActiveXMLObj_IXMLDOMElement_	objectivesXMLElement	= 0;				// Objectives element containing multiple scan engine objectives.
	ActiveXMLObj_IXMLDOMElement_	objectiveXMLElement		= 0;				// Objectives element.
	size_t							nObjectives				= ListNumItems(scanEngine->objectives);
	Objective_type*					objective				= NULL;
	
	// create new objectives element
	errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "Objectives", &objectivesXMLElement) );
		
	for (size_t i = 1; i <= nObjectives; i++) {
		objective = *(Objective_type**) ListGetPtrToItem(scanEngine->objectives, i);
		
		DAQLabXMLNode	objectivesAttr[] = { {"Name", BasicData_CString, objective->objectiveName},
											 {"FL", BasicData_Double, &objective->objectiveFL} };
		
		errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "Objective", &objectiveXMLElement) );
		errChk( DLAddToXMLElem(xmlDOM, objectiveXMLElement, objectivesAttr, DL_ATTRIBUTE, NumElem(objectivesAttr), xmlErrorInfo) ); 
		errChk ( ActiveXML_IXMLDOMElement_appendChild (objectivesXMLElement, xmlErrorInfo, objectiveXMLElement, NULL) ); 
		OKfreeCAHndl(objectiveXMLElement);   
	}
	
	// add objectives to parent element
	errChk ( ActiveXML_IXMLDOMElement_appendChild (parentXMLElement, xmlErrorInfo, objectivesXMLElement, NULL) );    
	OKfreeCAHndl(objectivesXMLElement);

	return 0;

Error:
	
	// cleanup
	OKfreeCAHndl(objectiveXMLElement);
	OKfreeCAHndl(objectivesXMLElement);
	
	return errorInfo.error;
}

static int SaveCfg_ScanChannels (ScanEngine_type* scanEngine, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ scanEngineXMLElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	ActiveXMLObj_IXMLDOMElement_	scanChannelsXMLElement		= 0;
	ActiveXMLObj_IXMLDOMElement_	scanChannelXMLElement		= 0;
	
	// create scan channels XML element
	errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "ScanChannels", &scanChannelsXMLElement) );    
	
	for (size_t i = 0; i < scanEngine->nScanChans; i++) {
		// create channel element
		errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "Channel", &scanChannelXMLElement) );
		uInt32	chanColor = (uInt32)scanEngine->scanChans[i]->color;
		DAQLabXMLNode	scanChannelAttr[] 	= { {"Color", BasicData_UInt, &chanColor} };
		// save scan channel attributes
		errChk( DLAddToXMLElem(xmlDOM, scanChannelXMLElement, scanChannelAttr, DL_ATTRIBUTE, NumElem(scanChannelAttr), xmlErrorInfo) );
		// add scan channel XML element to scan channels XML element
		errChk ( ActiveXML_IXMLDOMElement_appendChild (scanChannelsXMLElement, xmlErrorInfo, scanChannelXMLElement, NULL) );  
		OKfreeCAHndl(scanChannelXMLElement);
	}
	
	// add scan channels element to scan engine element
	errChk ( ActiveXML_IXMLDOMElement_appendChild (scanEngineXMLElement, xmlErrorInfo, scanChannelsXMLElement, NULL) ); 
	OKfreeCAHndl(scanChannelsXMLElement);
	
	return 0;
	
Error:
	
	OKfreeCAHndl(scanChannelsXMLElement);
	OKfreeCAHndl(scanChannelXMLElement);
	return errorInfo.error;
}

static int SaveCfg_RectRaster_FrameScan (RectRasterScanSet_type* scanSet, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	ActiveXMLObj_IXMLDOMElement_	frameScanXMLElement		= 0;
	DAQLabXMLNode					frameScanAttr[] 		= { {"ImageHeight", 		BasicData_UInt, 	&scanSet->height},
																{"ImageWidth",  		BasicData_UInt, 	&scanSet->width},
																{"ImageHeightOffset", 	BasicData_Int, 		&scanSet->heightOffset},
																{"ImageWidthOffset", 	BasicData_Int, 		&scanSet->widthOffset},
																{"PixelSize", 			BasicData_Double, 	&scanSet->pixSize},
																{"PixelDwellTime",		BasicData_Double, 	&scanSet->pixelDwellTime} };
	
	errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "FrameScanSettings", &frameScanXMLElement) );   
	errChk( DLAddToXMLElem(xmlDOM, frameScanXMLElement, frameScanAttr, DL_ATTRIBUTE, NumElem(frameScanAttr), xmlErrorInfo) );

	// add scan info element to scan engine element
	errChk ( ActiveXML_IXMLDOMElement_appendChild (parentXMLElement, xmlErrorInfo, frameScanXMLElement, NULL) ); 
	OKfreeCAHndl(frameScanXMLElement);

	return 0;
	
Error:
	
	// cleanup
	OKfreeCAHndl(frameScanXMLElement);
	
	return errorInfo.error;
}
													 
static int SaveCfg_PointScanProtocol (PointScanProtocol_type* pointScanProtocol, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR
	
	ActiveXMLObj_IXMLDOMElement_	pointScanProtocolXMLElement		= 0;
	ActiveXMLObj_IXMLDOMElement_	jumpSettingsXMLElement			= 0;
	ActiveXMLObj_IXMLDOMElement_	pointSettingsXMLElement			= 0;
	
	uInt32							jumpMethod						= pointScanProtocol->jumpMethod;
	
	DAQLabXMLNode					pointScanAttr[]					= { {"Name",						BasicData_CString,		pointScanProtocol->protocolName} };        
	
	DAQLabXMLNode					jumpSettingsAttr[] 				= { {"Repeat",						BasicData_UInt,			&pointScanProtocol->nSequenceRepeat},
																		{"StartDelay",					BasicData_Double,		&pointScanProtocol->startDelayInitVal},
																		{"StartDelayIncrement",			BasicData_Double,		&pointScanProtocol->startDelayIncrement},
																		{"RepeatWait",					BasicData_Double,		&pointScanProtocol->repeatWait},
																		{"PointJumpMode", 				BasicData_UInt, 		&jumpMethod},
																		{"Record",						BasicData_Bool,			&pointScanProtocol->record},
																		{"NIntegrate",					BasicData_UInt,			&pointScanProtocol->nIntegration},
																		{"Stimulate",					BasicData_Bool,			&pointScanProtocol->stimulate} };
	
	DAQLabXMLNode					pointSettingsAttr[] 			= { {"NHoldBurst",					BasicData_UInt,			&pointScanProtocol->nHoldBurst},
																		{"Hold", 						BasicData_Double, 		&pointScanProtocol->holdTime},
																		{"HoldBurstPeriod",				BasicData_Double,		&pointScanProtocol->holdBurstPeriod},
																		{"HoldBurstPeriodIncrement",	BasicData_Double,		&pointScanProtocol->holdBurstPeriodIncr},
																		{"StimulationDelay",			BasicData_Double,		&pointScanProtocol->stimDelay},
																		{"NPulses",						BasicData_UInt,			&pointScanProtocol->nStimPulses},
																		{"PulseONDuration",				BasicData_Double,		&pointScanProtocol->stimPulseONDuration},
																		{"PulseOFFDuration",			BasicData_Double,		&pointScanProtocol->stimPulseOFFDuration} };
	
	
	// create point scan protocol XML element
	errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "PointScanProtocol", &pointScanProtocolXMLElement) );
	// save point scan settings XML element attributes
	errChk( DLAddToXMLElem(xmlDOM, pointScanProtocolXMLElement, pointScanAttr, DL_ATTRIBUTE, NumElem(pointScanAttr), xmlErrorInfo) );
	
	// create jump settings XML element
	errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "JumpSettings", &jumpSettingsXMLElement) );
	// save jump settings attributes
	errChk( DLAddToXMLElem(xmlDOM, jumpSettingsXMLElement, jumpSettingsAttr, DL_ATTRIBUTE, NumElem(jumpSettingsAttr), xmlErrorInfo) );
	
	// create point settings XML element
	errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "PointSettings", &pointSettingsXMLElement) );
	// save jump settings attributes
	errChk( DLAddToXMLElem(xmlDOM, pointSettingsXMLElement, pointSettingsAttr, DL_ATTRIBUTE, NumElem(pointSettingsAttr), xmlErrorInfo) );
	
	// add jump and point settings XML elements to the point scan settings XML element
	errChk ( ActiveXML_IXMLDOMElement_appendChild (pointScanProtocolXMLElement, xmlErrorInfo, jumpSettingsXMLElement, NULL) ); 
	errChk ( ActiveXML_IXMLDOMElement_appendChild (pointScanProtocolXMLElement, xmlErrorInfo, pointSettingsXMLElement, NULL) ); 
	
	// add point scan settings to parent XML element
	errChk ( ActiveXML_IXMLDOMElement_appendChild (parentXMLElement, xmlErrorInfo, pointScanProtocolXMLElement, NULL) ); 
	
Error:
	
	// cleanup
	OKfreeCAHndl(pointScanProtocolXMLElement);
	OKfreeCAHndl(jumpSettingsXMLElement);
	OKfreeCAHndl(pointSettingsXMLElement);
	
	return errorInfo.error;
}

static int SaveCfg_RectRaster_PointScanProtocols (RectRaster_type* rectRaster, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_* parentXMLElementPtr, ERRORINFO* xmlErrorInfo)
{
INIT_ERR
	
	ActiveXMLObj_IXMLDOMElement_	pointScanProtocolsXMLElement	= 0;
	DAQLabXMLNode					pointScanProtocolsAttr[]		= {{"SelectedProtocol",			BasicData_CString,		rectRaster->pointScan.pointScanProtocol->protocolName}};
	
	// create point scan protocols XML element
	errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "PointScanProtocols", &pointScanProtocolsXMLElement) );
	
	// save selected point scan protocol name
	errChk( DLAddToXMLElem(xmlDOM, pointScanProtocolsXMLElement, pointScanProtocolsAttr, DL_ATTRIBUTE, NumElem(pointScanProtocolsAttr), xmlErrorInfo) );
	
	// add available point scan protocols
	size_t						nProtocols			= ListNumItems(rectRaster->pointScan.pointScanProtocols);
	PointScanProtocol_type*		pointScanProtocol   = NULL;
	
	for (size_t i = 1; i <= nProtocols; i++) {
		pointScanProtocol = *(PointScanProtocol_type**) ListGetPtrToItem(rectRaster->pointScan.pointScanProtocols, i);
		errChk( SaveCfg_PointScanProtocol(pointScanProtocol, xmlDOM, pointScanProtocolsXMLElement, xmlErrorInfo) );
	}
	
	// add point scan protocols XML element to parent
	if (*parentXMLElementPtr) {
		errChk ( ActiveXML_IXMLDOMElement_appendChild (*parentXMLElementPtr, xmlErrorInfo, pointScanProtocolsXMLElement, NULL) );
		OKfreeCAHndl(pointScanProtocolsXMLElement);
	} else
		*parentXMLElementPtr = pointScanProtocolsXMLElement;
	
	return 0;
	
Error:
	
	// cleanup
	OKfreeCAHndl(pointScanProtocolsXMLElement);
	
	return 0;
}

static int LoadCfg (DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  moduleElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	LaserScanning_type*				ls 								= (LaserScanning_type*) mod;
	ls->mainPanTopPos												= malloc(sizeof(int));
	ls->mainPanLeftPos												= malloc(sizeof(int));
	DAQLabXMLNode 					lsAttr[] 						= { {"PanTopPos", BasicData_Int, ls->mainPanTopPos},
											  		   					{"PanLeftPos", BasicData_Int, ls->mainPanLeftPos}};
	ActiveXMLObj_IXMLDOMElement_	scanAxisCalibrationsXMLElement 	= 0;   			// element containing multiple scan axis calibrations
	ActiveXMLObj_IXMLDOMElement_	scanEnginesXMLElement 			= 0;   			// element containing multiple scan engines
	ActiveXMLObj_IXMLDOMElement_	frameScanXMLElement				= 0;
	ActiveXMLObj_IXMLDOMElement_	pointScanProtocolsXMLElement	= 0;
	ActiveXMLObj_IXMLDOMElement_	objectivesXMLElement			= 0;
	ActiveXMLObj_IXMLDOMNodeList_	scanEngineNodeList				= 0;
	ActiveXMLObj_IXMLDOMNodeList_	objectivesNodeList				= 0;
	ActiveXMLObj_IXMLDOMNode_		scanEngineNode					= 0;
	ActiveXMLObj_IXMLDOMNode_		objectiveNode					= 0;
	long							nScanEngines					= 0;
	long							nObjectives						= 0;
	unsigned int					scanEngineType					= 0;
	char*							scanEngineName					= NULL;
	char*							fastAxisCalibrationName			= NULL;
	char*							slowAxisCalibrationName			= NULL;
	double							scanLensFL						= 0;
	double							tubeLensFL						= 0;
	double							objectiveFL						= 0;
	double							referenceClockFreq				= 0;
	double							pixelDelay						= 0;
	double							shutterSwitchTime				= 0;
	unsigned int					nFrames							= 0;
	BOOL							continuousFrameScan				= FALSE;
	char*							assignedObjectiveName			= NULL;
	char*							objectiveName					= NULL;
	ScanEngine_type*				scanEngine						= NULL;
	Objective_type*					objective						= NULL;
	ListType						pointScanProtocolsList			= 0;	  // elements of PointScanProtocol_type* type
	PointScanProtocol_type*			selectedPointScanProtocol		= NULL;
	PointScanProtocol_type*			pointScanProtocolCopy			= NULL;
	DAQLabXMLNode					scanEngineGenericAttr[] 		= {	{"Name", 						BasicData_CString, 		&scanEngineName},
																		{"ContinuousFrameScan", 		BasicData_Bool, 		&continuousFrameScan},
																		{"NFrames", 					BasicData_UInt, 		&nFrames},
																		{"ScanEngineType", 				BasicData_UInt, 		&scanEngineType},
																		{"FastAxisCalibrationName", 	BasicData_CString, 		&fastAxisCalibrationName},
																		{"SlowAxisCalibrationName", 	BasicData_CString, 		&slowAxisCalibrationName},
																		{"ScanLensFL", 					BasicData_Double, 		&scanLensFL},
																		{"TubeLensFL", 					BasicData_Double, 		&tubeLensFL},
																		{"Objective", 					BasicData_CString, 		&assignedObjectiveName},
																		{"PixelClockRate", 				BasicData_Double, 		&referenceClockFreq},
																		{"PixelSignalDelay", 			BasicData_Double, 		&pixelDelay},
																		{"ShutterSwitchTime",			BasicData_Double,		&shutterSwitchTime}};
																	
	DAQLabXMLNode					objectiveAttr[]					= { {"Name", 						BasicData_CString, 		&objectiveName},
																		{"FL", 							BasicData_Double, 		&objectiveFL} };
	
	// load main panel position 
	errChk( DLGetXMLElementAttributes("", moduleElement, lsAttr, NumElem(lsAttr)) ); 
	
	//--------------------------------------------------------------------------
	// Load scan axis calibrations
	//--------------------------------------------------------------------------

	errChk( DLGetSingleXMLElementFromElement(moduleElement, "ScanAxisCalibrations", &scanAxisCalibrationsXMLElement) );
	errChk( LoadCfg_ScanAxisCalibrations(ls, scanAxisCalibrationsXMLElement, xmlErrorInfo) );
	OKfreeCAHndl(scanAxisCalibrationsXMLElement);
	
	//--------------------------------------------------------------------------
	// Load scan engines
	//--------------------------------------------------------------------------
	
		
	errChk( DLGetSingleXMLElementFromElement(moduleElement, "ScanEngines", &scanEnginesXMLElement) );
	
	errChk ( ActiveXML_IXMLDOMElement_getElementsByTagName(scanEnginesXMLElement, xmlErrorInfo, "ScanEngine", &scanEngineNodeList) );
	errChk ( ActiveXML_IXMLDOMNodeList_Getlength(scanEngineNodeList, xmlErrorInfo, &nScanEngines) );
	
	for (long i = 0; i < nScanEngines; i++) {
		errChk ( ActiveXML_IXMLDOMNodeList_Getitem(scanEngineNodeList, xmlErrorInfo, i, &scanEngineNode) );
		// get generic scan engine attributes
		errChk( DLGetXMLElementAttributes("", (ActiveXMLObj_IXMLDOMElement_)scanEngineNode, scanEngineGenericAttr, NumElem(scanEngineGenericAttr)) );
		// get objectives XML element
		errChk( DLGetSingleXMLElementFromElement((ActiveXMLObj_IXMLDOMElement_)scanEngineNode, "Objectives", &objectivesXMLElement ) );  
		
		switch (scanEngineType) {
				
			case ScanEngine_RectRaster_NonResonantGalvoFastAxis_NonResonantGalvoSlowAxis:
				
				
				double						galvoSamplingRate		= 0;
				RectRasterScanSet_type*		frameScanSettings		= NULL;
				DAQLabXMLNode				rectangleRasterAttr[] 	= { {"GalvoSamplingRate", 	BasicData_Double, 	&galvoSamplingRate} }; 				
				
				// load rectangular raster scan attributes from the generic scan engine element
				errChk( DLGetXMLElementAttributes("", (ActiveXMLObj_IXMLDOMElement_)scanEngineNode, rectangleRasterAttr, NumElem(rectangleRasterAttr)) );
				
				// load frame scan settings
				errChk( DLGetSingleXMLElementFromElement((ActiveXMLObj_IXMLDOMElement_)scanEngineNode, "FrameScanSettings", &frameScanXMLElement) );
				errChk( LoadCfg_RectRaster_FrameScan(&frameScanSettings, frameScanXMLElement, xmlErrorInfo) );
				OKfreeCAHndl(frameScanXMLElement);
				
				// load point scan protocols
				errChk( DLGetSingleXMLElementFromElement((ActiveXMLObj_IXMLDOMElement_)scanEngineNode, "PointScanProtocols", &pointScanProtocolsXMLElement) );
				errChk( LoadCfg_RectRaster_PointScanProtocols(&pointScanProtocolsList, &selectedPointScanProtocol, pointScanProtocolsXMLElement, xmlErrorInfo) );
				OKfreeCAHndl(pointScanProtocolsXMLElement);
				nullChk( pointScanProtocolCopy = copy_PointScanProtocol_type(selectedPointScanProtocol) );
				
				nullChk( scanEngine = (ScanEngine_type*)init_RectRaster_type((LaserScanning_type*)mod, scanEngineName, continuousFrameScan, nFrames, galvoSamplingRate, referenceClockFreq, 
							 												 pixelDelay, shutterSwitchTime, &frameScanSettings, &pointScanProtocolCopy, &pointScanProtocolsList, scanLensFL, tubeLensFL) ); 
				
				break;
			
		}
		
		// assign fast and slow scan axis to scan engine
		// do this before loading the objective and updating optics
		size_t				nAxisCal	= ListNumItems(ls->availableCals);
		ScanAxisCal_type*	scanAxisCal = NULL;
		
		for (long j = 1; j <= nAxisCal; j++) {
			scanAxisCal = *(ScanAxisCal_type**) ListGetPtrToItem(ls->availableCals, j);
			if (!strcmp(fastAxisCalibrationName, scanAxisCal->calName)) {
				scanEngine->fastAxisCal = scanAxisCal;
				scanAxisCal->scanEngine = scanEngine;
				break;
			}
		}
		
		for (long j = 1; j <= nAxisCal; j++) {
			scanAxisCal = *(ScanAxisCal_type**) ListGetPtrToItem(ls->availableCals, j);
			if (!strcmp(slowAxisCalibrationName, scanAxisCal->calName)) {
				scanEngine->slowAxisCal = scanAxisCal;
				scanAxisCal->scanEngine = scanEngine;
				break;
			}
		}
		
		// load scan channels
		errChk( LoadCfg_ScanChannels(scanEngine, (ActiveXMLObj_IXMLDOMElement_)scanEngineNode, xmlErrorInfo) );
		
		// load objectives
		errChk ( ActiveXML_IXMLDOMElement_getElementsByTagName(objectivesXMLElement, xmlErrorInfo, "Objective", &objectivesNodeList) );
		errChk ( ActiveXML_IXMLDOMNodeList_Getlength(objectivesNodeList, xmlErrorInfo, &nObjectives) );
		
		for (long j = 0; j < nObjectives; j++) {
			errChk ( ActiveXML_IXMLDOMNodeList_Getitem(objectivesNodeList, xmlErrorInfo, j, &objectiveNode) );
			errChk( DLGetXMLElementAttributes("", (ActiveXMLObj_IXMLDOMElement_)objectiveNode, objectiveAttr, NumElem(objectiveAttr)) );    
			objective = init_Objective_type(objectiveName, objectiveFL);
			// assign objective to scan engine
			if (!strcmp(objectiveName, assignedObjectiveName)) {
				scanEngine->objectiveLens = objective;
				// MAKE SURE THIS IS DONE AFTER READING TUBE LENS AND SCAN LENS FL AND SCAN AXIS SETTINGS
				// update both scan axes
				if (scanEngine->fastAxisCal)
					(*scanEngine->fastAxisCal->UpdateOptics) (scanEngine->fastAxisCal);
				
				if (scanEngine->slowAxisCal)
					(*scanEngine->slowAxisCal->UpdateOptics) (scanEngine->slowAxisCal);
				
			}
			OKfree(objectiveName);
			// add objective to scan engine
			ListInsertItem(scanEngine->objectives, &objective, END_OF_LIST);
			OKfreeCAHndl(objectiveNode);
		}
		
		// add scan engine to laser scanning module
		ListInsertItem(ls->scanEngines, &scanEngine, END_OF_LIST);
		
		// cleanup
	
		OKfreeCAHndl(scanEngineNode);
		OKfreeCAHndl(objectivesXMLElement);
		OKfreeCAHndl(objectivesNodeList);
		OKfree(scanEngineName);
		OKfree(assignedObjectiveName);
		OKfree(fastAxisCalibrationName);
		OKfree(slowAxisCalibrationName);
	}
	

Error:
	
	discard_PointScanProtocol_type(&pointScanProtocolCopy);
	OKfreeCAHndl(frameScanXMLElement);
	OKfreeCAHndl(pointScanProtocolsXMLElement);
	OKfreeCAHndl(scanEngineNodeList); 
	OKfreeCAHndl(scanEnginesXMLElement);
	OKfreeCAHndl(scanEngineNode); 
	OKfreeCAHndl(scanAxisCalibrationsXMLElement); 
	
	return errorInfo.error;	
}

static int LoadCfg_ScanAxisCalibrations (LaserScanning_type* ls, ActiveXMLObj_IXMLDOMElement_ scanAxisCalibrationsXMLElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	ActiveXMLObj_IXMLDOMNode_		axisCalibrationNode				= 0;      
	long							nAxisCalibrations				= 0;
	unsigned int					axisCalibrationType				= 0;
	DAQLabXMLNode					axisCalibrationGenericAttr[] 	= {{"AxisType", BasicData_UInt, &axisCalibrationType}};
	ActiveXMLObj_IXMLDOMNodeList_	axisCalibrationNodeList			= 0;
	
	errChk ( ActiveXML_IXMLDOMElement_getElementsByTagName(scanAxisCalibrationsXMLElement, xmlErrorInfo, "AxisCalibration", &axisCalibrationNodeList) );
	errChk ( ActiveXML_IXMLDOMNodeList_Getlength(axisCalibrationNodeList, xmlErrorInfo, &nAxisCalibrations) );
	
	for (long i = 0; i < nAxisCalibrations; i++) {
		errChk ( ActiveXML_IXMLDOMNodeList_Getitem(axisCalibrationNodeList, xmlErrorInfo, i, &axisCalibrationNode) );
		errChk ( DLGetXMLElementAttributes("", (ActiveXMLObj_IXMLDOMElement_)axisCalibrationNode, axisCalibrationGenericAttr, NumElem(axisCalibrationGenericAttr)) ); 
		   
		switch (axisCalibrationType) {
				
			case NonResonantGalvo:
				errChk( LoadNonResGalvoCalFromXML(ls, (ActiveXMLObj_IXMLDOMElement_)axisCalibrationNode, xmlErrorInfo) );   
				break;
				
			case ResonantGalvo:
				break;
				
			case AOD:
				break;
				
			case Translation:
				break;
		}
		OKfreeCAHndl(axisCalibrationNode); 
		
	}
	
	OKfreeCAHndl(axisCalibrationNodeList);
	
	return 0;
	
Error:
	
	// cleanup
	OKfreeCAHndl(axisCalibrationNode); 
	OKfreeCAHndl(axisCalibrationNodeList);
	
	return errorInfo.error;
}

static int LoadCfg_ScanChannels (ScanEngine_type* scanEngine, ActiveXMLObj_IXMLDOMElement_ scanEngineXMLElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	ActiveXMLObj_IXMLDOMElement_	scanChannelsXMLElement		= 0;
	ActiveXMLObj_IXMLDOMNodeList_	scanChannelsNodeList		= 0;
	ActiveXMLObj_IXMLDOMNode_		scanChannelNode				= 0;
	long							nScanChannels				= 0;
	uInt32							chanColor 					= 0;
	DAQLabXMLNode					scanChannelAttr[] 			= { {"Color", BasicData_UInt, &chanColor} };
	
	// get channels XML element
	errChk( DLGetSingleXMLElementFromElement(scanEngineXMLElement, "ScanChannels", &scanChannelsXMLElement) );
	errChk ( ActiveXML_IXMLDOMElement_getElementsByTagName(scanChannelsXMLElement, xmlErrorInfo, "Channel", &scanChannelsNodeList) );
	errChk ( ActiveXML_IXMLDOMNodeList_Getlength(scanChannelsNodeList, xmlErrorInfo, &nScanChannels) );
	OKfreeCAHndl(scanChannelsXMLElement);
	
	scanEngine->nScanChans = (uInt32) nScanChannels; 
	if (nScanChannels)
		nullChk( scanEngine->scanChans = calloc(nScanChannels, sizeof(ScanChan_type*)) );
	for (long i = 0; i < nScanChannels; i++)
		nullChk( scanEngine->scanChans[i] = init_ScanChan_type(scanEngine, i+1) );
	
	// get channel attributes
	for (long i = 0; i < nScanChannels; i++) {
		errChk ( ActiveXML_IXMLDOMNodeList_Getitem(scanChannelsNodeList, xmlErrorInfo, i, &scanChannelNode) );
		errChk( DLGetXMLElementAttributes("", (ActiveXMLObj_IXMLDOMElement_)scanChannelNode, scanChannelAttr, NumElem(scanChannelAttr)) ); 
		scanEngine->scanChans[i]->color = chanColor;
		OKfreeCAHndl(scanChannelNode);
	}
	
	OKfreeCAHndl(scanChannelsNodeList);
	
	return 0;
	
Error:
	
	OKfreeCAHndl(scanChannelsXMLElement);
	OKfreeCAHndl(scanChannelsNodeList);
	OKfreeCAHndl(scanChannelNode);
	scanEngine->nScanChans = 0;
	OKfree(scanEngine->scanChans);
	
	return errorInfo.error;
}

static int LoadCfg_RectRaster_FrameScan (RectRasterScanSet_type** scanSetPtr, ActiveXMLObj_IXMLDOMElement_ frameScanSettingsXMLElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	uInt32							height					= 0;
	int								heightOffset			= 0;
	uInt32							width					= 0;
	int								widthOffset				= 0;
	double							pixelSize				= 0;
	double							pixelDwellTime			= 0;
	RectRasterScanSet_type*			frameScanSettings		= NULL;
	DAQLabXMLNode					frameScanAttr[] 		= { {"ImageHeight", 		BasicData_UInt, 	&height},
																{"ImageWidth",  		BasicData_UInt, 	&width},
																{"ImageHeightOffset", 	BasicData_Int, 		&heightOffset},
																{"ImageWidthOffset", 	BasicData_Int, 		&widthOffset},
																{"PixelSize", 			BasicData_Double, 	&pixelSize},
																{"PixelDwellTime",		BasicData_Double, 	&pixelDwellTime} };
	// load frame scan settings
	errChk( DLGetXMLElementAttributes("", frameScanSettingsXMLElement, frameScanAttr, NumElem(frameScanAttr)) ); 															
																
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
	
	
	nullChk( frameScanSettings = init_RectRasterScanSet_type(pixelSize, height, heightOffset, width, widthOffset, pixelDwellTime) );
	
	*scanSetPtr = frameScanSettings;
	frameScanSettings = NULL;
	return 0;
	
Error:
	
	// cleanup
	
	
	return errorInfo.error;
}

static int LoadCfg_PointScanProtocol (PointScanProtocol_type** pointScanProtocolPtr, ActiveXMLObj_IXMLDOMElement_ pointScanProtocolXMLElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR
	
	ActiveXMLObj_IXMLDOMElement_	jumpSettingsXMLElement		= 0;
	ActiveXMLObj_IXMLDOMElement_	pointSettingsXMLElement		= 0;
	
	double							holdTime					= 0;				
	uInt32							nHoldBurst					= 0;				
	double							holdBurstPeriod				= 0;		
	double							holdBurstPeriodIncr			= 0;
	double							stimDelay					= 0;
	double							stimPulseONDuration			= 0;
	double							stimPulseOFFDuration		= 0;
	uInt32							nStimPulses					= 0;
	uInt32							nSequenceRepeat				= 0;
	double							startDelayInitVal			= 0;
	double							startDelayIncrement			= 0;
	double							repeatWait					= 0;
	BOOL							record						= 0;
	uInt32							nIntegration				= 0;
	BOOL							stimulate					= 0;
	char*							protocolName				= NULL;
	
	// get jump settings XML element
	errChk( DLGetSingleXMLElementFromElement(pointScanProtocolXMLElement, "JumpSettings", &jumpSettingsXMLElement) );
	
	// get point settings XML element
	errChk( DLGetSingleXMLElementFromElement(pointScanProtocolXMLElement, "PointSettings", &pointSettingsXMLElement) );
	
	// skip loading point jump settings if either element is missing
	if (!jumpSettingsXMLElement || !pointSettingsXMLElement) goto SkipLoad;
	
	uInt32							jumpMethod					= 0;
	
	DAQLabXMLNode					pointScanAttr[]				= { {"Name",						BasicData_CString,		&protocolName} };
	
	DAQLabXMLNode					jumpSettingsAttr[] 			= { {"Repeat",						BasicData_UInt,			&nSequenceRepeat},
																	{"StartDelay",					BasicData_Double,		&startDelayInitVal},
																	{"StartDelayIncrement",			BasicData_Double,		&startDelayIncrement},
																	{"RepeatWait",					BasicData_Double,		&repeatWait},
																	{"PointJumpMode", 				BasicData_UInt, 		&jumpMethod},
																	{"Record",						BasicData_Bool,			&record},
																	{"NIntegrate",					BasicData_UInt,			&nIntegration},
																	{"Stimulate",					BasicData_Bool,			&stimulate} };
	
	DAQLabXMLNode					pointSettingsAttr[] 		= { {"NHoldBurst",					BasicData_UInt,			&nHoldBurst},
																	{"Hold", 						BasicData_Double, 		&holdTime},
																	{"HoldBurstPeriod",				BasicData_Double,		&holdBurstPeriod},
																	{"HoldBurstPeriodIncrement",	BasicData_Double,		&holdBurstPeriodIncr},
																	{"StimulationDelay",			BasicData_Double,		&stimDelay},
																	{"NPulses",						BasicData_UInt,			&nStimPulses},
																	{"PulseONDuration",				BasicData_Double,		&stimPulseONDuration},
																	{"PulseOFFDuration",			BasicData_Double,		&stimPulseOFFDuration} };
	
	// get point scan protocol name
	errChk( DLGetXMLElementAttributes("", pointScanProtocolXMLElement, pointScanAttr, NumElem(pointScanAttr)) );
	
	// get point scan settings
	errChk( DLGetXMLElementAttributes("", jumpSettingsXMLElement, jumpSettingsAttr, NumElem(jumpSettingsAttr)) ); 
	errChk( DLGetXMLElementAttributes("", pointSettingsXMLElement, pointSettingsAttr, NumElem(pointSettingsAttr)) );
	
	// store settings in data structures
	nullChk( *pointScanProtocolPtr = init_PointScanProtocol_type(protocolName, holdTime, nHoldBurst, holdBurstPeriod, holdBurstPeriodIncr, stimDelay, stimPulseONDuration, 
								 								stimPulseOFFDuration, nStimPulses, (PointJumpMethods)jumpMethod, record, stimulate, nIntegration, nSequenceRepeat, 
								 								startDelayInitVal, startDelayIncrement, repeatWait) );
	OKfree(protocolName);
	

SkipLoad:
Error:
		
	// cleanup
	OKfreeCAHndl(jumpSettingsXMLElement);
	OKfreeCAHndl(pointSettingsXMLElement);
	
	return errorInfo.error;																
}

static int LoadCfg_RectRaster_PointScanProtocols (ListType* pointScanProtocolsPtr, PointScanProtocol_type** selectedProtocolPtr, ActiveXMLObj_IXMLDOMElement_ pointScanProtocolsXMLElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	char*							selectedProtocolName			= NULL;
	long							nXMLProtocols					= 0;
	DAQLabXMLNode					pointScanProtocolsAttr[]		= { {"SelectedProtocol",		BasicData_CString,		&selectedProtocolName} };
	ActiveXMLObj_IXMLDOMNodeList_	protocolsNodeList				= 0;
	ActiveXMLObj_IXMLDOMNode_		protocolNode					= 0;
	PointScanProtocol_type*			pointScanProtocol   			= NULL;
	ListType						protocolsList					= 0;
	
	
	errChk( DLGetXMLElementAttributes("", pointScanProtocolsXMLElement, pointScanProtocolsAttr, NumElem(pointScanProtocolsAttr)) );
	
	errChk( ActiveXML_IXMLDOMElement_getElementsByTagName(pointScanProtocolsXMLElement, xmlErrorInfo, "PointScanProtocol", &protocolsNodeList) );
	errChk( ActiveXML_IXMLDOMNodeList_Getlength(protocolsNodeList, xmlErrorInfo, &nXMLProtocols) );
	
	// discard previous protocols if list already exists, otherwise create a new list
	if (*pointScanProtocolsPtr) {
		size_t						nProtocols 				= ListNumItems(*pointScanProtocolsPtr);
		PointScanProtocol_type**	pointScanProtocolPtr   	= NULL;
	
		for (size_t i = 1; i <= nProtocols; i++) {
			pointScanProtocolPtr = (PointScanProtocol_type**)ListGetPtrToItem(*pointScanProtocolsPtr, i);
			discard_PointScanProtocol_type(pointScanProtocolPtr);
		}
		ListClear(*pointScanProtocolsPtr);
	
	} else {
		nullChk( protocolsList = ListCreate(sizeof(PointScanProtocol_type*)) );
	}
		
	// load protocols and assign chosen protocol
	for (long i = 0; i < nXMLProtocols; i++) {
		errChk( ActiveXML_IXMLDOMNodeList_Getitem(protocolsNodeList, xmlErrorInfo, i, &protocolNode) );
		errChk( LoadCfg_PointScanProtocol(&pointScanProtocol, (ActiveXMLObj_IXMLDOMElement_) protocolNode, xmlErrorInfo) );
		OKfreeCAHndl(protocolNode);
		nullChk( ListInsertItem(protocolsList, &pointScanProtocol, END_OF_LIST) );
		
		// assign protocol
		if (!strcmp(selectedProtocolName, pointScanProtocol->protocolName))
			*selectedProtocolPtr = pointScanProtocol;
		
		pointScanProtocol = NULL;
	}
	
	*pointScanProtocolsPtr = protocolsList;
	
	return 0;

Error:
	
	OKfreeList(&protocolsList, (DiscardFptr_type)discard_PointScanProtocol_type);
	OKfree(selectedProtocolName);
	discard_PointScanProtocol_type(&pointScanProtocol);
	
	return errorInfo.error;
	
}

static int SaveNonResGalvoCalToXML	(NonResGalvoCal_type* nrgCal, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ axisCalibrationsElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	ActiveXMLObj_IXMLDOMElement_	switchTimesXMLElement; 			  
	ActiveXMLObj_IXMLDOMElement_	maxSlopesXMLElement;   		
	ActiveXMLObj_IXMLDOMElement_	triangleCalXMLElement;
	char*							switchTimesStepSizeStr 				= NULL;
	char*							switchTimesSwitchTimeStr			= NULL;
	char*							switchTimesDelayStr					= NULL;
	char*							maxSlopesSlopeStr					= NULL;
	char*							maxSlopesAmplitudeStr				= NULL;
	char*							triangleCalCommandAmplitudeStr		= NULL;
	char*							triangleCalActualAmplitudeStr		= NULL;
	char*							triangleCalMaxFreqStr				= NULL;
	char*							triangleCalResiduaLagStr			= NULL;
	DAQLabXMLNode 					nrgCalAttr[] 						= {	{"CommandVMax", BasicData_Double, &nrgCal->commandVMax},
																			{"CommandAsFunctionOfPositionLinFitSlope", BasicData_Double, &nrgCal->slope},
																			{"CommandAsFunctionOfPositionLinFitOffset", BasicData_Double, &nrgCal->offset},
																			{"PositionStdDev", BasicData_Double, &nrgCal->posStdDev},
																			{"ResponseLag", BasicData_Double, &nrgCal->lag},
																			{"Resolution", BasicData_Double, &nrgCal->resolution},
																			{"MinStepSize", BasicData_Double, &nrgCal->minStepSize},
																			{"ParkedCommandV", BasicData_Double, &nrgCal->parked},
																			{"MechanicalResponse", BasicData_Double, &nrgCal->mechanicalResponse} };
	
	// Save calibration attributes
	errChk( DLAddToXMLElem(xmlDOM, axisCalibrationsElement, nrgCalAttr, DL_ATTRIBUTE, NumElem(nrgCalAttr), xmlErrorInfo) ); 
	
	// create calibration elements
	errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "SwitchTimes", &switchTimesXMLElement) );
	errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "MaxSlopes", &maxSlopesXMLElement) );
	errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "TriangleWaveform", &triangleCalXMLElement) );
	
	// convert switch times calibration data to string
	nullChk( switchTimesStepSizeStr = malloc (nrgCal->switchTimes->n * MAX_DOUBLE_NCHARS * sizeof(char)+1) );
	nullChk( switchTimesSwitchTimeStr = malloc (nrgCal->switchTimes->n * MAX_DOUBLE_NCHARS * sizeof(char)+1) );
	nullChk( switchTimesDelayStr = malloc (nrgCal->switchTimes->n * MAX_DOUBLE_NCHARS * sizeof(char)+1) );
	
	Fmt(switchTimesStepSizeStr, CALIBRATION_DATA_TO_STRING, nrgCal->switchTimes->n, nrgCal->switchTimes->stepSize); 
	Fmt(switchTimesSwitchTimeStr, CALIBRATION_DATA_TO_STRING, nrgCal->switchTimes->n, nrgCal->switchTimes->switchTime);
	Fmt(switchTimesDelayStr, CALIBRATION_DATA_TO_STRING, nrgCal->switchTimes->n, nrgCal->switchTimes->delay);
	
	// convert max slopes calibration data to string
	nullChk( maxSlopesSlopeStr = malloc (nrgCal->maxSlopes->n * MAX_DOUBLE_NCHARS * sizeof(char)+1) );
	nullChk( maxSlopesAmplitudeStr = malloc (nrgCal->maxSlopes->n * MAX_DOUBLE_NCHARS * sizeof(char)+1) );
	
	Fmt(maxSlopesSlopeStr, CALIBRATION_DATA_TO_STRING, nrgCal->maxSlopes->n, nrgCal->maxSlopes->slope); 
	Fmt(maxSlopesAmplitudeStr, CALIBRATION_DATA_TO_STRING, nrgCal->maxSlopes->n, nrgCal->maxSlopes->amplitude);
	
	// convert triangle waveform calibration data to string
	nullChk( triangleCalCommandAmplitudeStr = malloc (nrgCal->triangleCal->n * MAX_DOUBLE_NCHARS * sizeof(char)+1) );
	nullChk( triangleCalActualAmplitudeStr = malloc (nrgCal->triangleCal->n * MAX_DOUBLE_NCHARS * sizeof(char)+1) );
	nullChk( triangleCalMaxFreqStr = malloc (nrgCal->triangleCal->n * MAX_DOUBLE_NCHARS * sizeof(char)+1) );
	nullChk( triangleCalResiduaLagStr = malloc (nrgCal->triangleCal->n * MAX_DOUBLE_NCHARS * sizeof(char)+1) );
	
	Fmt(triangleCalCommandAmplitudeStr, CALIBRATION_DATA_TO_STRING, nrgCal->triangleCal->n, nrgCal->triangleCal->commandAmp); 
	Fmt(triangleCalActualAmplitudeStr, CALIBRATION_DATA_TO_STRING, nrgCal->triangleCal->n, nrgCal->triangleCal->actualAmp);
	Fmt(triangleCalMaxFreqStr, CALIBRATION_DATA_TO_STRING, nrgCal->triangleCal->n, nrgCal->triangleCal->maxFreq); 
	Fmt(triangleCalResiduaLagStr, CALIBRATION_DATA_TO_STRING, nrgCal->triangleCal->n, nrgCal->triangleCal->resLag);
	
	// add calibration data to XML elements
	DAQLabXMLNode 			switchTimesAttr[] 		= {	{"NElements", 	BasicData_UInt, 	&nrgCal->switchTimes->n},
														{"StepSize", 	BasicData_CString, 	switchTimesStepSizeStr},
														{"SwitchTime", 	BasicData_CString, 	switchTimesSwitchTimeStr},
														{"Delay",		BasicData_CString,	switchTimesDelayStr}};
														
	DAQLabXMLNode 			maxSlopesAttr[] 		= {	{"NElements", BasicData_UInt, &nrgCal->maxSlopes->n},
														{"Slope", BasicData_CString, maxSlopesSlopeStr},
														{"Amplitude", BasicData_CString, maxSlopesAmplitudeStr} };
														
	DAQLabXMLNode 			triangleCalAttr[] 		= {	{"NElements", BasicData_UInt, &nrgCal->triangleCal->n},
														{"DeadTime", BasicData_Double, &nrgCal->triangleCal->deadTime},
														{"CommandAmplitude", BasicData_CString, triangleCalCommandAmplitudeStr},
														{"ActualAmplitude", BasicData_CString, triangleCalActualAmplitudeStr},
														{"MaxFrequency", BasicData_CString, triangleCalMaxFreqStr},
														{"ResidualLag", BasicData_CString, triangleCalResiduaLagStr} };
	
	errChk( DLAddToXMLElem(xmlDOM, switchTimesXMLElement, switchTimesAttr, DL_ATTRIBUTE, NumElem(switchTimesAttr), xmlErrorInfo) );
	errChk( DLAddToXMLElem(xmlDOM, maxSlopesXMLElement, maxSlopesAttr, DL_ATTRIBUTE, NumElem(maxSlopesAttr), xmlErrorInfo) );
	errChk( DLAddToXMLElem(xmlDOM, triangleCalXMLElement, triangleCalAttr, DL_ATTRIBUTE, NumElem(triangleCalAttr), xmlErrorInfo) );
								
	// add calibration elements to scan axis calibration element
	errChk ( ActiveXML_IXMLDOMElement_appendChild (axisCalibrationsElement, xmlErrorInfo, switchTimesXMLElement, NULL) );
	OKfreeCAHndl(switchTimesXMLElement); 
	errChk ( ActiveXML_IXMLDOMElement_appendChild (axisCalibrationsElement, xmlErrorInfo, maxSlopesXMLElement, NULL) );
	OKfreeCAHndl(maxSlopesXMLElement);
	errChk ( ActiveXML_IXMLDOMElement_appendChild (axisCalibrationsElement, xmlErrorInfo, triangleCalXMLElement, NULL) );
	OKfreeCAHndl(triangleCalXMLElement); 
	

Error:
	
	// cleanup
	OKfree(switchTimesStepSizeStr);
	OKfree(switchTimesSwitchTimeStr);
	OKfree(switchTimesDelayStr);
	OKfree(maxSlopesSlopeStr);
	OKfree(maxSlopesAmplitudeStr);
	OKfree(triangleCalCommandAmplitudeStr);
	OKfree(triangleCalActualAmplitudeStr);
	OKfree(triangleCalMaxFreqStr);
	OKfree(triangleCalResiduaLagStr);
	
	return errorInfo.error;
}

static int LoadNonResGalvoCalFromXML (LaserScanning_type* lsModule, ActiveXMLObj_IXMLDOMElement_ axisCalibrationElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

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
																		
	char*							switchTimesStepSizeStr 			= NULL;
	char*							switchTimesSwitchTimeStr		= NULL;
	char*							switchTimesDelayStr				= NULL;
	unsigned int					nSwitchTimes					= 0;
	char*							maxSlopesSlopeStr				= NULL;
	char*							maxSlopesAmplitudeStr			= NULL;
	unsigned int					nMaxSlopes						= 0;
	char*							triangleCalCommandAmplitudeStr	= NULL;
	char*							triangleCalActualAmplitudeStr	= NULL;
	char*							triangleCalMaxFreqStr			= NULL;
	char*							triangleCalResiduaLagStr		= NULL;
	unsigned int					nTriangleCal					= 0;
	double							deadTime						= 0;
	
	errChk( DLGetXMLElementAttributes("", axisCalibrationElement, axisCalibrationAttr, NumElem(axisCalibrationAttr)) ); 
	
	errChk( DLGetSingleXMLElementFromElement(axisCalibrationElement, "SwitchTimes", &switchTimesXMLElement) ); 
	errChk( DLGetSingleXMLElementFromElement(axisCalibrationElement, "MaxSlopes", &maxSlopesXMLElement) );
	errChk( DLGetSingleXMLElementFromElement(axisCalibrationElement, "TriangleWaveform", &triangleCalXMLElement) );
	
	// get calibration data from XML elements
	DAQLabXMLNode 					switchTimesAttr[] 				= {	{"NElements", 			BasicData_UInt, 	&nSwitchTimes},
																		{"StepSize", 			BasicData_CString, 	&switchTimesStepSizeStr},
																		{"SwitchTime", 			BasicData_CString, 	&switchTimesSwitchTimeStr},
																		{"Delay",				BasicData_CString, 	&switchTimesDelayStr}};
														
	DAQLabXMLNode 					maxSlopesAttr[] 				= {	{"NElements", 			BasicData_UInt, 	&nMaxSlopes},
																		{"Slope", 				BasicData_CString, 	&maxSlopesSlopeStr},
																		{"Amplitude", 			BasicData_CString, 	&maxSlopesAmplitudeStr} };
														
	DAQLabXMLNode 					triangleCalAttr[] 				= {	{"NElements", 			BasicData_UInt, 	&nTriangleCal}, 
																		{"DeadTime", 			BasicData_Double,	&deadTime},
																		{"CommandAmplitude", 	BasicData_CString, 	&triangleCalCommandAmplitudeStr},
																		{"ActualAmplitude", 	BasicData_CString, 	&triangleCalActualAmplitudeStr},
																		{"MaxFrequency", 		BasicData_CString, 	&triangleCalMaxFreqStr},
																		{"ResidualLag", 		BasicData_CString, 	&triangleCalResiduaLagStr} };
	
	// switch times
	errChk( DLGetXMLElementAttributes("", switchTimesXMLElement, switchTimesAttr, NumElem(switchTimesAttr)) );
	OKfreeCAHndl(switchTimesXMLElement);
	
	SwitchTimes_type*	switchTimes = init_SwitchTimes_type();
	
	switchTimes->n 							= nSwitchTimes;
	nullChk( switchTimes->switchTime		= malloc(nSwitchTimes * sizeof(double)) );
	nullChk( switchTimes->delay				= malloc(nSwitchTimes * sizeof(double)) );
	nullChk( switchTimes->stepSize			= malloc(nSwitchTimes * sizeof(double)) );
	
	Scan(switchTimesStepSizeStr, STRING_TO_CALIBRATION_DATA, switchTimes->n, switchTimes->stepSize);
	Scan(switchTimesSwitchTimeStr, STRING_TO_CALIBRATION_DATA, switchTimes->n, switchTimes->switchTime);
	Scan(switchTimesDelayStr, STRING_TO_CALIBRATION_DATA, switchTimes->n, switchTimes->delay);  
	
	// max slopes
	errChk( DLGetXMLElementAttributes("", maxSlopesXMLElement, maxSlopesAttr, NumElem(maxSlopesAttr)) );
	OKfreeCAHndl(maxSlopesXMLElement);
	
	MaxSlopes_type*		maxSlopes = init_MaxSlopes_type();
	
	maxSlopes->n 						= nMaxSlopes;
	nullChk( maxSlopes->amplitude		= malloc(nMaxSlopes * sizeof(double)) );
	nullChk( maxSlopes->slope			= malloc(nMaxSlopes * sizeof(double)) );
	
	Scan(maxSlopesSlopeStr, STRING_TO_CALIBRATION_DATA, maxSlopes->n , maxSlopes->slope);
	Scan(maxSlopesAmplitudeStr, STRING_TO_CALIBRATION_DATA, maxSlopes->n , maxSlopes->amplitude);  
	
	// triangle waveform calibration
	errChk( DLGetXMLElementAttributes("", triangleCalXMLElement, triangleCalAttr, NumElem(triangleCalAttr)) );
	OKfreeCAHndl(triangleCalXMLElement);
	
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
	OKfree(switchTimesSwitchTimeStr);
	OKfree(switchTimesDelayStr);
	OKfree(maxSlopesSlopeStr);
	OKfree(maxSlopesAmplitudeStr);
	OKfree(triangleCalCommandAmplitudeStr);
	OKfree(triangleCalActualAmplitudeStr);
	OKfree(triangleCalMaxFreqStr);
	OKfree(triangleCalResiduaLagStr);
	
	OKfreeCAHndl(switchTimesXMLElement);
	OKfreeCAHndl(maxSlopesXMLElement); 
	OKfreeCAHndl(triangleCalXMLElement);   
	
	return errorInfo.error;	
}

static int DisplayPanels (DAQLabModule_type* mod, BOOL visibleFlag)
{
INIT_ERR

	LaserScanning_type*			ls				= (LaserScanning_type*) mod;
	size_t						nEngines		= ListNumItems(ls->scanEngines);
	size_t						nActiveCals		= ListNumItems(ls->activeCal);
	ScanEngine_type**			enginePtr		= NULL;
	ActiveScanAxisCal_type**	activeCalPtr	= NULL;
	
	
	// laser scanning module panels
	if (ls->mainPanHndl) {
		errChk(SetPanelAttribute(ls->mainPanHndl, ATTR_VISIBLE, visibleFlag));
	}
	
	if (ls->enginesPanHndl) {
		errChk(SetPanelAttribute(ls->enginesPanHndl, ATTR_VISIBLE, visibleFlag));
	}
	
	if (ls->manageAxisCalPanHndl) {
		errChk(SetPanelAttribute(ls->manageAxisCalPanHndl, ATTR_VISIBLE, visibleFlag));
	}
	
	if (ls->newAxisCalTypePanHndl) {
		errChk(SetPanelAttribute(ls->newAxisCalTypePanHndl, ATTR_VISIBLE, visibleFlag));
	}
		
	
	// scan engine settings panels
	for (size_t i = 1; i <= nEngines; i++) {
		enginePtr = ListGetPtrToItem(ls->scanEngines, i);
		
		if ((*enginePtr)->engineSetPanHndl) {
			errChk(SetPanelAttribute((*enginePtr)->engineSetPanHndl, ATTR_VISIBLE, visibleFlag));
		}
	}
	
	// active scan axis calibration panels
	for(size_t i = 1; i <= nActiveCals; i++) {
		activeCalPtr = ListGetPtrToItem(ls->activeCal, i);
		
		if ((*activeCalPtr)->calPanHndl) {
			errChk(SetPanelAttribute((*activeCalPtr)->calPanHndl, ATTR_VISIBLE, visibleFlag));
		}
	}
	
Error:
	return errorInfo.error;	
}

void CVICALLBACK ScanEngineSettingsMenu_CB (int menuBarHandle, int menuItemID, void *callbackData, int panelHandle)
{
	LaserScanning_type*		ls					= callbackData;
	ScanEngine_type**		enginePtr			= NULL;
	ScanEngine_type*		engine				= NULL;
	int						workspacePanHndl	= 0;
	int						tabIdx				= 0;
	
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
	
	// populate channels
	InsertTableRows(engine->engineSetPanHndl, ScanSetPan_Channels, -1, engine->nScanChans, VAL_USE_MASTER_CELL_TYPE);
	for (size_t i = 0; i < engine->nScanChans; i++) {
		Point			chanCell 	= {ScanChanColor_ColIdx, i+1};
		// insert available channel colors
		InsertTableCellRingItem(engine->engineSetPanHndl, ScanSetPan_Channels, chanCell, ScanChanColor_Grey, "grey");
		InsertTableCellRingItem(engine->engineSetPanHndl, ScanSetPan_Channels, chanCell, ScanChanColor_Red, "red");
		InsertTableCellRingItem(engine->engineSetPanHndl, ScanSetPan_Channels, chanCell, ScanChanColor_Green, "green");
		InsertTableCellRingItem(engine->engineSetPanHndl, ScanSetPan_Channels, chanCell, ScanChanColor_Blue, "blue");
		// select channel color
		SetTableCellValFromIndex(engine->engineSetPanHndl, ScanSetPan_Channels, chanCell, (int)engine->scanChans[i]->color);
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
	SetCtrlVal(engine->engineSetPanHndl, ScanSetPan_RefClkFreq, engine->referenceClockFreq/1e6);								// convert from [Hz] to [MHz] 
	
	// update pixel delay
	SetCtrlVal(engine->engineSetPanHndl, ScanSetPan_PixelDelay, engine->pixDelay);												// convert from [Hz] to [MHz] 
	
	// update shutter switch time
	SetCtrlVal(engine->engineSetPanHndl, ScanSetPan_ShutterSwitchTime, engine->shutterSwitchTime);										
	
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
INIT_ERR

	LaserScanning_type*		ls 		= callbackData;
	
	switch (event) {
			
		case EVENT_LEFT_DOUBLE_CLICK:
			
			switch (control) {
					
				case EnginesPan_ScanTypes:
				
					unsigned int			 	engineType			= 0;				//	  of  ScanEngineEnum_type
					ScanEngine_type*			newScanEngine		= NULL;
					char* 						engineName			= NULL;
					int							tabPagePanHndl		= 0;
					void*						panDataPtr			= NULL;
					RectRasterScanSet_type*		frameScanSettings	= NULL;
					PointScanProtocol_type*		pointScanProtocol	= NULL;
					
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
					switch (engineType) {
					
						case ScanEngine_RectRaster_NonResonantGalvoFastAxis_NonResonantGalvoSlowAxis:
					
							frameScanSettings = init_RectRasterScanSet_type(NonResGalvoRasterScan_Default_PixelSize, 1, 0, 1, 0, NonResGalvoRasterScan_Default_PixelDwellTime); 
							
							pointScanProtocol = init_PointScanProtocol_type("", NonResGalvoRasterScan_Default_HoldTime, 1, 0, 0, 0, NonResGalvoRasterScan_Default_StimPulseONDuration, 
									 NonResGalvoRasterScan_Default_StimPulseOFFDuration, 1, PointJump_SinglePoints, FALSE, FALSE, 1, 1, 0, 0, 0);
							
							newScanEngine = (ScanEngine_type*) init_RectRaster_type(ls, engineName, FALSE, 1, NonResGalvoRasterScan_Default_GalvoSamplingRate, NonResGalvoRasterScan_Default_PixelClockRate, 
											0, 0, &frameScanSettings, &pointScanProtocol, NULL, 1, 1);
							
							errChk( InsertRectRasterScanEngineToUI(ls, (RectRaster_type*)newScanEngine, &errorInfo.errMsg) );
							
							break;
							// add below more cases	
					}
					
					DLRegisterScanEngine(newScanEngine);
					
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
	
Error:
	
PRINT_ERR

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
INIT_ERR

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
					errChk( TaskControlEvent(scanEngine->taskControl, TC_Event_Unconfigure, NULL, NULL, &errorInfo.errMsg) );
				}
				// remove slow scan axis calibration if assigned
				if (scanEngine->slowAxisCal == *axisCalPtr) {
					scanEngine->slowAxisCal = NULL;
					// unconfigure scan engine
					errChk( TaskControlEvent(scanEngine->taskControl, TC_Event_Unconfigure, NULL, NULL, &errorInfo.errMsg) );
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
	
Error:
	
PRINT_ERR
	
	return 0;
}

static int CVICALLBACK NewScanAxisCalib_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	LaserScanning_type*			ls 							= callbackData;
	char* 						calTCName					= NULL;
	char* 						commandVChanName			= NULL;
	char* 						commandNSamplesVChanName	= NULL;
	char* 						positionVChanName			= NULL;
	ActiveNonResGalvoCal_type* 	nrgCal						= NULL;
	int 						calibPanHndl				= 0;
	int 						testPanHndl					= 0; 
	unsigned int 				axisType					= 0;
	int							parentPanHndl				= 0;
	
	switch (event) {
			
		case EVENT_COMMIT:
			
			switch (control) {
					
				case AxisSelect_OKBTTN:
					
					
					GetPanelAttribute(panel, ATTR_PANEL_PARENT, &parentPanHndl);
					GetCtrlVal(panel, AxisSelect_AxisType, &axisType);
					
					switch (axisType) {
							
						case NonResonantGalvo:
							
							// get unique name for calibration task controller
							nullChk( calTCName = DLGetUniqueTaskControllerName(AxisCal_Default_TaskController_Name) );
							
							// get unique name for command signal VChan
							nullChk( commandVChanName = StrDup(calTCName) );
							nullChk( AppendString(&commandVChanName,": ", -1) ); 
							nullChk( AppendString(&commandVChanName, ScanAxisCal_SourceVChan_Command, -1) );
							
							// get unique name for command signal nSamples VChan
							nullChk( commandNSamplesVChanName = StrDup(calTCName) );
							nullChk( AppendString(&commandNSamplesVChanName,": ", -1) );
							nullChk( AppendString(&commandNSamplesVChanName, ScanAxisCal_SourceVChan_NSamples, -1) );
							
							// get unique name for position feedback signal VChan 
							nullChk( positionVChanName = StrDup(calTCName) );
							nullChk( AppendString(&positionVChanName, ": ", -1) );
							nullChk( AppendString(&positionVChanName, ScanAxisCal_SinkVChan_Position, -1) );
							
							// init structure for galvo calibration
							nullChk( nrgCal = init_ActiveNonResGalvoCal_type(ls, calTCName, commandVChanName, commandNSamplesVChanName, positionVChanName) );
							
							//----------------------------------------
							// Task Controller and VChans registration
							//----------------------------------------
							// register calibration Task Controller with the framework
							DLAddTaskController((DAQLabModule_type*)ls, nrgCal->baseClass.taskController);
							// configure Task Controller
							errChk( TaskControlEvent(nrgCal->baseClass.taskController, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
							// register VChans with framework
							DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)nrgCal->baseClass.VChanCom);
							DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)nrgCal->baseClass.VChanComNSamples);
							DLRegisterVChan((DAQLabModule_type*)ls, (VChan_type*)nrgCal->baseClass.VChanPos);
							// register sink VChan with task controller
							errChk( AddSinkVChan(nrgCal->baseClass.taskController, nrgCal->baseClass.VChanPos, NULL, &errorInfo.errMsg) );
							// add new galvo calibration to laser scanning module list
							nullChk( ListInsertItem(ls->activeCal, &nrgCal, END_OF_LIST) );
							//----------------------------------------------------------
							
							errChk( nrgCal->baseClass.calPanHndl = LoadPanel(parentPanHndl, MOD_LaserScanning_UI, NonResGCal) );
							// change panel title
							SetPanelAttribute(nrgCal->baseClass.calPanHndl, ATTR_TITLE, calTCName);
							// add callback function and data first to all the direct controls in the calibration panel
							SetCtrlsInPanCBInfo(nrgCal, NonResGalvoCal_MainPan_CB, nrgCal->baseClass.calPanHndl); 
							
							// get calibration panel handle
							GetPanelHandleFromTabPage(nrgCal->baseClass.calPanHndl, NonResGCal_Tab, 0, &calibPanHndl);
							// add callback function and data to calibration controls
							SetCtrlsInPanCBInfo(nrgCal, NonResGalvoCal_CalPan_CB, calibPanHndl); 
							
							// get test panel handle
							GetPanelHandleFromTabPage(nrgCal->baseClass.calPanHndl, NonResGCal_Tab, 1, &testPanHndl);
							// add callback function and data to test controls
							SetCtrlsInPanCBInfo(nrgCal, NonResGalvoCal_TestPan_CB, testPanHndl);
							
							// Display calibration panel
							DisplayPanel(nrgCal->baseClass.calPanHndl);
							
							break;
							
						default:
							
							DLMsg("Calibration not implemented for the chosen axis type.", 1);
					}
					
					// discard axis calibration selection panel
					OKfreePanHndl(ls->newAxisCalTypePanHndl);
					break;
					
				case AxisSelect_CancelBTTN:
					
					// discard axis calibration selection panel
					OKfreePanHndl(ls->newAxisCalTypePanHndl);
					break;
			}
			break;
	}
	
Error:
	
	// cleanup
	OKfree(calTCName);
	OKfree(commandVChanName);
	OKfree(commandNSamplesVChanName);
	OKfree(positionVChanName);
	
PRINT_ERR

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

/// HIFN Analyzes a step response signal to calculate the step time in [ms] needed to cross two thresholds between a low-level and a high level.
static int GetStepTime (double* signal, size_t nSamples, double samplingRate, double lowLevel, double highLevel, double threshold, double* stepTimePtr, double* responseDelayPtr, char** errorMsg)
{
#define GetStepTime_Err_Levels						-1
#define GetStepTime_Err_LowLevelCrossingNotFound	-2
#define GetStepTime_Err_HighLevelCrossingNotFound	-3
#define GetStepTime_Err_LevelCrossingOrder			-4
	
INIT_ERR

	BOOL 	crossed		= FALSE; 		// flag showing if threshold was crossed 
	size_t	lowIdx		= 0;
	size_t	highIdx		= 0;
	
	
	// check if levels are correctly given
	if (lowLevel > highLevel)
		SET_ERR(GetStepTime_Err_Levels, "Low level must be smaller than High level");
	
	threshold = fabs(threshold); 		// make sure it's positive	
	
	// calculate a simple high level crossing from the left side
	for (size_t i = 0; i < nSamples; i++) 
		if (signal[i] > highLevel - threshold) { 
			highIdx = i; 
			crossed = TRUE; 
			break;
		}
	
	if (!crossed)
		SET_ERR(GetStepTime_Err_HighLevelCrossingNotFound, "Could not find low level crossing of signal");
	
	// calculate low level crossing
	crossed = FALSE;
	for (size_t i = 0; i < nSamples; i++) 
		if (signal[i] > lowLevel + threshold) { 
			lowIdx = i; 
			crossed = TRUE; 
			break;
		}
	
	if (!crossed)
		SET_ERR(GetStepTime_Err_LowLevelCrossingNotFound, "Could not find low level crossing of signal");
	
	if (highIdx < lowIdx)
		SET_ERR(GetStepTime_Err_LevelCrossingOrder, "High level crossing occurs before low level crossing");
		
	*stepTimePtr = (highIdx-lowIdx) * 1e3/samplingRate;  
	
	*responseDelayPtr = lowIdx * 1e3/samplingRate; 
	
Error:

	RETURN_ERR
}

//-------------------------------------------
// ScanChan_type
//-------------------------------------------
static ScanChan_type* init_ScanChan_type (ScanEngine_type* engine, uInt32 chanIdx)
{
#define init_ScanChan_type_ERR_NoDisplay	-1	// There is no display selected.
INIT_ERR
	
	ScanChan_type* 			scanChan		= malloc(sizeof(ScanChan_type));
	char*					detVChanName	= NULL;
	char*					outputVChanName	= NULL;
	ImageDisplay_type**		imgDisplayPtr	= NULL;
	
	if (!scanChan) return NULL;
	
	//--------------------
	// Init
	//--------------------
	
	DLDataTypes allowedPacketTypes[] 		= Allowed_Detector_Data_Types;
	scanChan->imgDisplayTSV					= 0;
	scanChan->imgDisplayTSVLineNumDebug		= 0;
	scanChan->waveDisplay					= NULL;
	scanChan->detVChan						= NULL;
	scanChan->outputVChan					= NULL;
	scanChan->color							= ScanChanColor_Grey;
	scanChan->scanEngine 					= engine;
	
	//--------------------
	// Alloc resources
	//--------------------
	
	// image display TSV
	errChk( CmtNewTSV(sizeof(ImageDisplay_type*), &scanChan->imgDisplayTSV) );
	
	errChk( CmtGetTSVPtr(scanChan->imgDisplayTSV, &imgDisplayPtr) ); scanChan->imgDisplayTSVLineNumDebug = __LINE__; 
	
	// Switching between different displays
	#ifdef __ImageDisplayNIVision_H__ 
	
		*imgDisplayPtr = NULL;  // Display handle initialization is not needed here.
		
	#else
		
		#ifdef __ImageDisplayCVI_H__
		
			nullChk( *imgDisplayPtr = (ImageDisplay_type*)init_ImageDisplayCVI_type(engine->lsModule->baseClass.workspacePanHndl, "", 10, 10, NULL) );
	
		#else
	
			SET_ERR(init_ScanChan_type_ERR_NoDisplay, "There is no display selected.");
	
		#endif
			
	#endif
	
	errChk( CmtReleaseTSVPtr(scanChan->imgDisplayTSV) );
	imgDisplayPtr = NULL;
	scanChan->imgDisplayTSVLineNumDebug = 0;

	// incoming pixel data from detection channel
	nullChk( detVChanName = DLVChanName((DAQLabModule_type*)engine->lsModule, engine->taskControl, ScanEngine_SinkVChan_DetectionChan, chanIdx) );
	nullChk( scanChan->detVChan = init_SinkVChan_type(detVChanName, allowedPacketTypes, NumElem(allowedPacketTypes), scanChan, VChanDataTimeout, DetectionVChan_StateChange) );
	
	// outgoing image channel
	nullChk( outputVChanName = DLVChanName((DAQLabModule_type*)engine->lsModule, engine->taskControl, ScanEngine_SourceVChan_ImageChannel, chanIdx) );
	nullChk( scanChan->outputVChan = init_SourceVChan_type(outputVChanName, DL_Image, scanChan, NULL) );
	
	// register sink VChan with task controller
	errChk( AddSinkVChan(engine->taskControl, scanChan->detVChan, NULL, NULL) );
	
	// waveform display for point recordings
	nullChk( scanChan->waveDisplay = init_WaveformDisplay_type(engine->lsModule->baseClass.workspacePanHndl, detVChanName, WaveformDisplay_CB, scanChan) );
	
	// cleanup
	OKfree(detVChanName);
	OKfree(outputVChanName);
	
	return scanChan;
	
Error:
	
	// cleanup
	OKfree(detVChanName);
	OKfree(outputVChanName);
	if (imgDisplayPtr) {
		CmtReleaseTSVPtr(scanChan->imgDisplayTSV);
		imgDisplayPtr = NULL;
	}
	
	discard_ScanChan_type(&scanChan);
	
	return NULL;
}

static void	discard_ScanChan_type (ScanChan_type** scanChanPtr)
{
	ScanChan_type*			scanChan 		= *scanChanPtr;
	ImageDisplay_type**		imgDisplayPtr   = NULL;
	
	if (!scanChan) return;
	
	// unregister sink VChan from task controller
	RemoveSinkVChan(scanChan->scanEngine->taskControl, scanChan->detVChan, NULL); 
	discard_VChan_type((VChan_type**)&scanChan->detVChan);
	discard_VChan_type((VChan_type**)&scanChan->outputVChan);
	
	// discard image display
	if (!CmtGetTSVPtr(scanChan->imgDisplayTSV, &imgDisplayPtr)) {
		scanChan->imgDisplayTSVLineNumDebug = __LINE__;
		// discard image display child class
		if (*imgDisplayPtr)
			(*(*imgDisplayPtr)->imageDisplayDiscardFptr)  ((void**)imgDisplayPtr);
		
		CmtReleaseTSVPtr(scanChan->imgDisplayTSV);
		scanChan->imgDisplayTSVLineNumDebug = 0;
	}
	CmtDiscardTSV(scanChan->imgDisplayTSV);
	scanChan->imgDisplayTSV = 0;
	
	// discard waveform display
	discard_WaveformDisplay_type(&scanChan->waveDisplay);
	
	OKfree(*scanChanPtr);
}

static int RegisterDLScanChan (ScanChan_type* scanChan)
{
	// add detection and image channels to the framework
	DLRegisterVChan((DAQLabModule_type*)scanChan->scanEngine->lsModule, (VChan_type*)scanChan->detVChan);
	DLRegisterVChan((DAQLabModule_type*)scanChan->scanEngine->lsModule, (VChan_type*)scanChan->outputVChan);
	
	return 0;
}

static int UnregisterDLScanChan (ScanChan_type* scanChan)
{
	// remove detection and image VChans from the framework
	DLUnregisterVChan((DAQLabModule_type*)scanChan->scanEngine->lsModule, (VChan_type*)scanChan->detVChan);
	DLUnregisterVChan((DAQLabModule_type*)scanChan->scanEngine->lsModule, (VChan_type*)scanChan->outputVChan);
	
	return 0;
}

static int CVICALLBACK ScanEngineSettings_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR
	
	ScanEngine_type*		engine					= callbackData;
	RectRaster_type*		rectRasterEngine		= (RectRaster_type*) engine;
	ScanAxisCal_type**		scanAxisPtr				= NULL;
	int						listItemIdx				= 0;
	int						nRows					= 0;
	int						workspacePanHndl		= 0;
	
	switch (event) {
			
		case EVENT_COMMIT:
			
			switch (control) {
				
				case ScanSetPan_AddChan:
					
					engine->nScanChans++;
					engine->scanChans = realloc(engine->scanChans, engine->nScanChans * sizeof(ScanChan_type*));
					engine->scanChans[engine->nScanChans - 1] = init_ScanChan_type(engine, engine->nScanChans);
					RegisterDLScanChan(engine->scanChans[engine->nScanChans - 1]);
					
					InsertTableRows(panel, ScanSetPan_Channels, -1, 1, VAL_USE_MASTER_CELL_TYPE);
					
					// add default gray scale
					GetNumTableRows(panel, ScanSetPan_Channels, &nRows);
					Point	chanCell = {ScanChanColor_ColIdx, nRows};
					// insert available channel colors
					InsertTableCellRingItem(panel, ScanSetPan_Channels, chanCell, ScanChanColor_Grey, "grey");
					InsertTableCellRingItem(panel, ScanSetPan_Channels, chanCell, ScanChanColor_Red, "red");
					InsertTableCellRingItem(panel, ScanSetPan_Channels, chanCell, ScanChanColor_Green, "green");
					InsertTableCellRingItem(panel, ScanSetPan_Channels, chanCell, ScanChanColor_Blue, "blue");
					// select channel color
					SetTableCellValFromIndex(panel, ScanSetPan_Channels, chanCell, ScanChanColor_Grey);
					break;
					
				case ScanSetPan_Channels:
					
					// eventData1 = Row of cell where event was generated. If the event affected multiple cells, such as when you sort or paste a range of cells, eventData1 is set to 0.
					// eventData2 = Column of cell where event was generated. If the event affected multiple cells, such as when you sort or paste a range of cells, eventData2 is set to 0.
					
					switch (eventData2) {
							
						case ScanChanColor_ColIdx: // channel color
							
							Point	cell 		= {eventData2, eventData1};
							int		nChars  	= 0;
							char*	colorLabel 	= NULL;
							GetTableCellValLength(panel, control, cell, &nChars);
							colorLabel = malloc((nChars+1)*sizeof(char));
							GetTableCellVal(panel, control, cell, colorLabel);
							if (!strcmp("grey", colorLabel)) engine->scanChans[eventData1-1]->color = ScanChanColor_Grey;
							if (!strcmp("red", colorLabel)) engine->scanChans[eventData1-1]->color = ScanChanColor_Red;
							if (!strcmp("green", colorLabel)) engine->scanChans[eventData1-1]->color = ScanChanColor_Green;
							if (!strcmp("blue", colorLabel)) engine->scanChans[eventData1-1]->color = ScanChanColor_Blue;
							OKfree(colorLabel);
							break;
					}
					
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
					if (NonResRectRasterScan_ValidConfig(rectRasterEngine)) {
				
						errChk( NonResRectRasterScan_ScanWidths(rectRasterEngine, &errorInfo.errMsg) );
						errChk( NonResRectRasterScan_PixelDwellTimes(rectRasterEngine, &errorInfo.errMsg) );
					
						if (NonResRectRasterScan_ReadyToScan(rectRasterEngine))
							errChk( TaskControlEvent(engine->taskControl, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
						else 
							errChk( TaskControlEvent(engine->taskControl, TC_Event_Unconfigure, NULL, NULL, &errorInfo.errMsg) ); 	
					} else 
						errChk( TaskControlEvent(engine->taskControl, TC_Event_Unconfigure, NULL, NULL, &errorInfo.errMsg) );
						
					break;
					
				case ScanSetPan_TubeLensFL:
					
					GetCtrlVal(panel, control, &engine->tubeLensFL);
					// update scan axes if any
					if (engine->fastAxisCal)
						(*engine->fastAxisCal->UpdateOptics) (engine->fastAxisCal);
					
					if (engine->slowAxisCal)
						(*engine->slowAxisCal->UpdateOptics) (engine->slowAxisCal);
					
					// configure/unconfigure scan engine
					if (NonResRectRasterScan_ValidConfig(rectRasterEngine)) {
				
						errChk( NonResRectRasterScan_ScanWidths(rectRasterEngine, &errorInfo.errMsg) );
						errChk( NonResRectRasterScan_PixelDwellTimes(rectRasterEngine, &errorInfo.errMsg) );
					
						if (NonResRectRasterScan_ReadyToScan(rectRasterEngine))
							errChk( TaskControlEvent(engine->taskControl, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
						else 
							errChk( TaskControlEvent(engine->taskControl, TC_Event_Unconfigure, NULL, NULL, &errorInfo.errMsg) ); 	
					} else 
						errChk( TaskControlEvent(engine->taskControl, TC_Event_Unconfigure, NULL, NULL, &errorInfo.errMsg) );
					
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
					if (NonResRectRasterScan_ValidConfig(rectRasterEngine)) {
				
						errChk( NonResRectRasterScan_ScanWidths(rectRasterEngine, &errorInfo.errMsg) );
						errChk( NonResRectRasterScan_PixelDwellTimes(rectRasterEngine, &errorInfo.errMsg) );
					
						if (NonResRectRasterScan_ReadyToScan(rectRasterEngine))
							errChk( TaskControlEvent(engine->taskControl, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
						else 
							errChk( TaskControlEvent(engine->taskControl, TC_Event_Unconfigure, NULL, NULL, &errorInfo.errMsg) ); 	
						
					} else 
						errChk( TaskControlEvent(engine->taskControl, TC_Event_Unconfigure, NULL, NULL, &errorInfo.errMsg) );
						
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
					if (NonResRectRasterScan_ValidConfig(rectRasterEngine)) {
				
						errChk( NonResRectRasterScan_ScanWidths(rectRasterEngine, &errorInfo.errMsg) );
						errChk( NonResRectRasterScan_PixelDwellTimes(rectRasterEngine, &errorInfo.errMsg) );
					
						if (NonResRectRasterScan_ReadyToScan(rectRasterEngine)) 
							errChk( TaskControlEvent(engine->taskControl, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
						else 
							errChk( TaskControlEvent(engine->taskControl, TC_Event_Unconfigure, NULL, NULL, &errorInfo.errMsg) ); 	
							
					} else
						errChk( TaskControlEvent(engine->taskControl, TC_Event_Unconfigure, NULL, NULL, &errorInfo.errMsg) );
						
					break;
					
				case ScanSetPan_RefClkFreq:
					
					GetCtrlVal(panel, control, &engine->referenceClockFreq);								// read in [MHz]
					engine->referenceClockFreq *= 1e6;														// convert to [Hz]
					// configure/unconfigure scan engine
					if (NonResRectRasterScan_ValidConfig(rectRasterEngine)) {
				
						errChk( NonResRectRasterScan_ScanWidths(rectRasterEngine, &errorInfo.errMsg) );
						errChk( NonResRectRasterScan_PixelDwellTimes(rectRasterEngine, &errorInfo.errMsg) );
					
						if (NonResRectRasterScan_ReadyToScan(rectRasterEngine))
							errChk( TaskControlEvent(engine->taskControl, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
						else
							errChk( TaskControlEvent(engine->taskControl, TC_Event_Unconfigure, NULL, NULL, &errorInfo.errMsg) ); 	
					} else
						errChk( TaskControlEvent(engine->taskControl, TC_Event_Unconfigure, NULL, NULL, &errorInfo.errMsg) );
					break;
					
				case ScanSetPan_PixelDelay:
					
					GetCtrlVal(panel, control, &engine->pixDelay);
					break;
					
				case ScanSetPan_ShutterSwitchTime:
					GetCtrlVal(panel, control, &engine->shutterSwitchTime);
					break;
					
				case ScanSetPan_GalvoSamplingRate:
					
					GetCtrlVal(panel, control, &rectRasterEngine->galvoSamplingRate);			// read in [kHz]
					rectRasterEngine->galvoSamplingRate *= 1e3;									// convert to [Hz]
					// enforce the chosen galvo sampling rate to divide exactly the reference clock frequency
					uInt32	refClkTicks = (uInt32)(engine->referenceClockFreq/rectRasterEngine->galvoSamplingRate);
					rectRasterEngine->galvoSamplingRate = engine->referenceClockFreq/refClkTicks;
					SetCtrlVal(panel, control, rectRasterEngine->galvoSamplingRate * 1e-3);  	// display in [kHz]
					
					// configure/unconfigure scan engine
					if (NonResRectRasterScan_ValidConfig(rectRasterEngine)) {
				
						errChk( NonResRectRasterScan_ScanWidths(rectRasterEngine, &errorInfo.errMsg) );
						errChk( NonResRectRasterScan_PixelDwellTimes(rectRasterEngine, &errorInfo.errMsg) );
					
						if (NonResRectRasterScan_ReadyToScan(rectRasterEngine))
							errChk( TaskControlEvent(engine->taskControl, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
						else
							errChk( TaskControlEvent(engine->taskControl, TC_Event_Unconfigure, NULL, NULL, &errorInfo.errMsg) ); 	
					} else
						errChk( TaskControlEvent(engine->taskControl, TC_Event_Unconfigure, NULL, NULL, &errorInfo.errMsg) );
					break;
					
				case ScanSetPan_ParkGalvos:
					
					// send fast axis to parked position
					if (engine->fastAxisCal) {
						errChk( SendScanAxisCommand(engine, ScanAxis_Fast, ((NonResGalvoCal_type*)engine->fastAxisCal)->parked, &errorInfo.errMsg) );
					}
					// send slow axis to parked position
					if (engine->slowAxisCal) {
						errChk( SendScanAxisCommand(engine, ScanAxis_Slow, ((NonResGalvoCal_type*)engine->slowAxisCal)->parked, &errorInfo.errMsg) );
					}
					break;
					
				case ScanSetPan_CenterGalvos:
					
					// send fast axis to parked position
					errChk( SendScanAxisCommand(engine, ScanAxis_Fast, 0.0, &errorInfo.errMsg) );
					// send slow axis to parked position
					errChk( SendScanAxisCommand(engine, ScanAxis_Slow, 0.0, &errorInfo.errMsg) );
					break;
					
			}
			break;
			
		case EVENT_KEYPRESS:
					
			// continue only if Del key is pressed
			if (eventData1 != VAL_FWD_DELETE_VKEY) break;
			
			switch (control) {
				
				// discard last scan channel
				case ScanSetPan_Channels:
					
					GetNumTableRows(panel, control, &nRows);
					if (!nRows) break; // stop here if list is empty
					
					UnregisterDLScanChan(engine->scanChans[nRows-1]);
					discard_ScanChan_type(&engine->scanChans[nRows-1]);
					
					engine->nScanChans--;
					engine->scanChans = realloc(engine->scanChans, engine->nScanChans * sizeof(ScanChan_type*));
					
					DeleteTableRows(panel, control, nRows, 1);
					
					// configure / unconfigure
					if (NonResRectRasterScan_ReadyToScan(rectRasterEngine))
						errChk( TaskControlEvent(engine->taskControl, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
					else
						errChk( TaskControlEvent(engine->taskControl, TC_Event_Unconfigure, NULL, NULL, &errorInfo.errMsg) ); 
						
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
					DeleteListItem(engine->frameScanPanHndl, ScanTab_Objective, listItemIdx, 1);
					
					// update chosen objective in scan engine structure
					GetCtrlIndex(engine->frameScanPanHndl, ScanTab_Objective, &listItemIdx);
					if (listItemIdx >= 0) {
						objectivePtr = ListGetPtrToItem(engine->objectives, listItemIdx+1); 
						engine->objectiveLens = *objectivePtr;
					} else
						engine->objectiveLens = NULL;
					
					// configure / unconfigure
					if (NonResRectRasterScan_ReadyToScan(rectRasterEngine))
						errChk( TaskControlEvent(engine->taskControl, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
					else
						errChk( TaskControlEvent(engine->taskControl, TC_Event_Unconfigure, NULL, NULL, &errorInfo.errMsg) );
					
					break;
			}
			
			break;
			
	}
	
Error:
	
PRINT_ERR

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
	ScanEngine_type**	enginePtr = ListGetPtrToItem(ls->scanEngines, activeTabIdx + 1);
	DLUnregisterScanEngine(*enginePtr);
	(*(*enginePtr)->discardFptr) ((void**)enginePtr);
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
INIT_ERR
	
	ActiveNonResGalvoCal_type*		activeNRGCal 	= callbackData;
	
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
					
					errChk( DisassembleTaskTreeBranch(calTC, &errorInfo.errMsg) );
					
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

Error:
	
PRINT_ERR

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
static Waveform_type* NonResGalvoMoveBetweenPoints (NonResGalvoCal_type* cal, double sampleRate, double startVoltage, double endVoltage, double startDelay, double endDelay)
{
	double 			maxSlope;			// in [V/ms]
	double 			amplitude			= fabs(startVoltage - endVoltage);
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

// Estimates from the galvo calibration data, the time it takes the galvo to jump and settle in [ms] given a jump amplitude in [V].
// If estimation is successful, the function return 0 and negative values otherwise.
static int NonResGalvoPointJumpTime (NonResGalvoCal_type* cal, double jumpAmplitude, double* switchTimePtr, double* responseDelayPtr)
{
INIT_ERR
	
	double 			min					= 0;
	double 			max					= 0;
	int    			minIdx				= 0;
	int    			maxIdx				= 0;
	double* 		secondDerivatives	= NULL;
	
	// make sure that jump amplitude is bounded to the calibration limits
	// Warning: if jump amplitude is outside of these limits, incorrect results are possible
	errChk( MaxMin1D(cal->switchTimes->stepSize, cal->switchTimes->n, &max, &maxIdx, &min, &minIdx) );
	if (fabs(jumpAmplitude) < min)
		jumpAmplitude = min;
	else
		if (fabs(jumpAmplitude) > max)
			jumpAmplitude = max;
			
	nullChk( secondDerivatives = malloc(cal->switchTimes->n * sizeof(double)) );
	
	// interpolate switch times vs. amplitude measurements
	errChk( Spline(cal->switchTimes->stepSize, cal->switchTimes->switchTime, cal->switchTimes->n, 0, 0, secondDerivatives) );
	errChk( SpInterp(cal->switchTimes->stepSize, cal->switchTimes->switchTime, secondDerivatives,  cal->switchTimes->n, fabs(jumpAmplitude), switchTimePtr) );
	
	// interpolate delay vs. amplitude measurements
	errChk( Spline(cal->switchTimes->stepSize, cal->switchTimes->delay, cal->switchTimes->n, 0, 0, secondDerivatives) );
	errChk( SpInterp(cal->switchTimes->stepSize, cal->switchTimes->delay, secondDerivatives,  cal->switchTimes->n, fabs(jumpAmplitude), responseDelayPtr) );

Error:
	
	OKfree(secondDerivatives);
	
	return errorInfo.error;
}

/* 
Generates a step waveform with a given initial delay, and end delay that is added to a settling delay that is determined by the galvo calibration data.

		  initial delay	   settling  (determined by the switch time from calibration data)
	   :<-------------->:<--------->:
	   :				:			:		  :
	   v				v			:  end delay:
startV _________________			:<----------->:
		  				|			:		  	  :
		  				| 			:		      :
		  				| 			:		  	  :
		  				|			v		  	  v
		  				|__________________________ endV 
				     
*/
// sampleRate in [Hz], startVoltage and endVoltage in [V], startDelay and endDelay in [s]
/*
static Waveform_type* NonResGalvoJumpBetweenPoints (NonResGalvoCal_type* cal, double sampleRate, double startVoltage, double endVoltage, double startDelay, double endDelay)
{
	int				error				= 0;
	double 			amplitude			= fabs(startVoltage - endVoltage);
	double			switchTime			= 0;   	// Time required for the galvo to reach 50% between startVoltage to endVoltage in [ms].
	size_t 			nElemJump			= 0;	// Number of galvo samples required for the galvo to jump and settle when switching from startVoltage to endVoltage.
	size_t			nElemStartDelay		= 0;
	size_t			nElemEndDelay		= 0;
	double*			waveformData		= NULL;
	Waveform_type* 	waveform			= NULL; 
	
	errChk( NonResGalvoPointJumpTime(cal, amplitude, &switchTime) );
	
	nElemJump 			= (size_t) ceil(sampleRate * switchTime * 1e-3);
	nElemStartDelay		= (size_t) floor(sampleRate * startDelay);
	nElemEndDelay		= (size_t) floor(sampleRate * endDelay);
	
	nullChk( waveformData = malloc ((nElemStartDelay + nElemJump + nElemEndDelay) * sizeof(double)) );
	
	if (nElemStartDelay)
		Set1D(waveformData, nElemStartDelay, startVoltage);
	
	Set1D(waveformData + nElemStartDelay, nElemJump + nElemEndDelay, endVoltage);
	
	nullChk( waveform = init_Waveform_type(Waveform_Double, sampleRate, nElemStartDelay + nElemJump + nElemEndDelay, (void**)&waveformData) );  
    
	return waveform;
	
Error:
	
	OKfree(waveformData);
	discard_Waveform_type(&waveform);
	
	return NULL;
}
*/

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
	cal->baseClass.VChanCom			= init_SourceVChan_type(commandVChanName, DL_Waveform_Double, cal, NULL);   
	cal->baseClass.VChanComNSamples	= init_SourceVChan_type(commandNSamplesVChanName, DL_UInt64, cal, NULL);   
	cal->baseClass.VChanPos			= init_SinkVChan_type(positionVChanName, allowedPacketTypes, NumElem(allowedPacketTypes), cal, VChanDataTimeout + Default_ActiveNonResGalvoCal_ScanTime * 1e3, NULL);  
	cal->baseClass.scanAxisType  	= NonResonantGalvo;
	cal->baseClass.Discard			= discard_ActiveNonResGalvoCal_type; // override
	cal->baseClass.taskController	= init_TaskControl_type(calName, cal, DLGetCommonThreadPoolHndl(), ConfigureTC_NonResGalvoCal, UncofigureTC_NonResGalvoCal, IterateTC_NonResGalvoCal, StartTC_NonResGalvoCal, ResetTC_NonResGalvoCal, 
								  DoneTC_NonResGalvoCal, StoppedTC_NonResGalvoCal, NULL, TaskTreeStateChange_NonResGalvoCal, NULL, NULL, NULL);
	cal->baseClass.lsModule			= lsModule;
	
								  
	// init ActiveNonResGalvoCal_type
	cal->commandVMax				= 0;
	cal->currentCal					= NonResGalvoCal_Slope_Offset;
	cal->currIterIdx				= 0;
	cal->slope						= NULL;	// i.e. calibration not performed yet
	cal->offset						= NULL;
	cal->posStdDev					= NULL;
	cal->targetSlope				= 0;
	cal->extraRuns					= 0;
	cal->lag						= NULL;
	cal->nRepeat					= 0;
	cal->lastRun					= FALSE;
	cal->nRampSamples				= 0;
	cal->switchTimes				= NULL;
	cal->maxSlopes					= NULL;
	cal->triangleCal				= NULL;
	cal->positionWaveform			= NULL;
	cal->commandWaveform			= NULL;
	cal->resolution  				= 0;
	cal->minStepSize 				= 0;
	cal->scanTime    				= Default_ActiveNonResGalvoCal_ScanTime;	// in [s]
	cal->parked						= 0;
	cal->mechanicalResponse			= 1;
	
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
	switchTimes->switchTime 	= NULL;	
	switchTimes->delay		 	= NULL;	
	
	return switchTimes;
}

static void discard_SwitchTimes_type (SwitchTimes_type** switchTimesPtr)
{
	SwitchTimes_type*	switchTimes = *switchTimesPtr;
	
	if (!switchTimes) return;
	
	OKfree(switchTimes->stepSize);
	OKfree(switchTimes->switchTime);
	OKfree(switchTimes->delay);
	
	OKfree(*switchTimesPtr);
}

static SwitchTimes_type* copy_SwitchTimes_type (SwitchTimes_type* switchTimes)
{
INIT_ERR

	SwitchTimes_type*	switchTimesCopy = malloc(sizeof(SwitchTimes_type));
	if (!switchTimesCopy) return NULL;
	
	// init
	switchTimesCopy->n 				= switchTimes->n;
	switchTimesCopy->switchTime		= NULL;
	switchTimesCopy->delay			= NULL;
	switchTimesCopy->stepSize		= NULL;
	
	
	//----------- 
	// copy
	//----------- 
	if (!switchTimesCopy->n) return switchTimesCopy; 
	
	// switchTime
	nullChk( switchTimesCopy->switchTime 	= malloc(switchTimes->n * sizeof(double)) );
	memcpy(switchTimesCopy->switchTime, switchTimes->switchTime, switchTimes->n * sizeof(double));
	
	// delay
	nullChk( switchTimesCopy->delay 	= malloc(switchTimes->n * sizeof(double)) );
	memcpy(switchTimesCopy->delay, switchTimes->delay, switchTimes->n * sizeof(double));
	
	// stepSize
	nullChk( switchTimesCopy->stepSize 		= malloc(switchTimes->n * sizeof(double)) );
	memcpy(switchTimesCopy->stepSize, switchTimes->stepSize, switchTimes->n * sizeof(double));
	
	return switchTimesCopy;
	
Error:
	
	OKfree(switchTimesCopy->switchTime);
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

static void discard_MaxSlopes_type (MaxSlopes_type** maxSlopesPtr)
{
	MaxSlopes_type* maxSlopes = *maxSlopesPtr;
	
	if (!maxSlopes) return; 
	
	OKfree(maxSlopes->slope);
	OKfree(maxSlopes->amplitude);
	
	OKfree(*maxSlopesPtr);
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

static int init_ScanEngine_type (ScanEngine_type** 			enginePtr, 
								 LaserScanning_type*		lsModule,
								 TaskControl_type**			taskControllerPtr,
								 BOOL						continuousFrameScan,
								 size_t						nFrames,
								 ScanEngineEnum_type 		engineType,
								 double						referenceClockFreq,
								 double						pixelDelay,
								 double						shutterSwitchTime,
								 double						scanLensFL,
								 double						tubeLensFL)					
{
INIT_ERR	
	
	ScanEngine_type*	engine									= *enginePtr;
	DLDataTypes 		allowedScanAxisPacketTypes[] 			= {DL_Waveform_Double};
	char*				fastAxisComVChanName					= NULL;
	char*				fastAxisComNSampVChanName				= NULL;
	char*				galvoComSamplingRateVChanName			= NULL;
	char*				slowAxisComVChanName					= NULL;
	char*				slowAxisComNSampVChanName				= NULL;
	char*				fastAxisPosVChanName					= NULL;
	char*				slowAxisPosVChanName					= NULL;
	char*				compositeImageVChanName					= NULL;
	char*				pixelPulseTrainVChanName				= NULL;
	char*				pixelSamplingRateVChanName				= NULL;
	char*				nPixelsVChanName						= NULL;
	char*				ROIHoldVChanName						= NULL;
	char*				ROIShutterVChanName						= NULL;
	
	// assemble default VChan names
	nullChk( fastAxisComVChanName 				= DLVChanName((DAQLabModule_type*)lsModule, *taskControllerPtr, ScanEngine_SourceVChan_FastAxis_Command, 0) );
	nullChk( fastAxisComNSampVChanName 			= DLVChanName((DAQLabModule_type*)lsModule, *taskControllerPtr, ScanEngine_SourceVChan_FastAxis_Command_NSamples, 0) );
	nullChk( slowAxisComVChanName	 			= DLVChanName((DAQLabModule_type*)lsModule, *taskControllerPtr, ScanEngine_SourceVChan_SlowAxis_Command, 0) );
	nullChk( slowAxisComNSampVChanName 			= DLVChanName((DAQLabModule_type*)lsModule, *taskControllerPtr, ScanEngine_SourceVChan_SlowAxis_Command_NSamples, 0) );
	nullChk( galvoComSamplingRateVChanName		= DLVChanName((DAQLabModule_type*)lsModule, *taskControllerPtr, ScanEngine_SourceVChan_Galvo_Command_SamplingRate, 0) );
	nullChk( fastAxisPosVChanName	 			= DLVChanName((DAQLabModule_type*)lsModule, *taskControllerPtr,	ScanEngine_SinkVChan_FastAxis_Position, 0) );
	nullChk( slowAxisPosVChanName	 			= DLVChanName((DAQLabModule_type*)lsModule, *taskControllerPtr,	ScanEngine_SinkVChan_SlowAxis_Position, 0) );
	nullChk( compositeImageVChanName 			= DLVChanName((DAQLabModule_type*)lsModule, *taskControllerPtr,	ScanEngine_SourceVChan_CompositeImage, 0) );
	nullChk( pixelPulseTrainVChanName 			= DLVChanName((DAQLabModule_type*)lsModule, *taskControllerPtr,	ScanEngine_SourceVChan_PixelPulseTrain, 0) );
	nullChk( pixelSamplingRateVChanName			= DLVChanName((DAQLabModule_type*)lsModule, *taskControllerPtr,	ScanEngine_SourceVChan_PixelSamplingRate, 0) );
	nullChk( nPixelsVChanName 					= DLVChanName((DAQLabModule_type*)lsModule, *taskControllerPtr,	ScanEngine_SourceVChan_NPixels, 0) );
	nullChk( ROIHoldVChanName					= DLVChanName((DAQLabModule_type*)lsModule, *taskControllerPtr,	ScanEngine_SourceVChan_ROIHold, 0) );
	nullChk( ROIShutterVChanName				= DLVChanName((DAQLabModule_type*)lsModule, *taskControllerPtr,	ScanEngine_SourceVChan_ROIShutter, 0) );
	
	//------------------------
	// init
	//------------------------
	
	engine->engineType					= engineType;
	engine->scanMode					= ScanEngineMode_FrameScan;
	// reference to axis calibration
	engine->fastAxisCal					= NULL;
	engine->slowAxisCal					= NULL;
	// reference to the laser scanning module owning the scan engine
	engine->lsModule					= lsModule;
	// task controller
	engine->nFrames						= nFrames;
	engine->continuousAcq				= continuousFrameScan;
	engine->taskControl					= *taskControllerPtr;
	*taskControllerPtr					= NULL;
	SetTaskControlModuleData(engine->taskControl, engine);
	// VChans
	engine->VChanFastAxisCom			= NULL;
	engine->VChanFastAxisComNSamples	= NULL;
	engine->VChanSlowAxisCom			= NULL;
	engine->VChanSlowAxisComNSamples	= NULL;
	engine->VChanGalvoComSamplingRate	= NULL;
	engine->VChanFastAxisPos			= NULL;
	engine->VChanCompositeImage			= NULL;
	engine->VChanPixelPulseTrain		= NULL;
	engine->VChanPixelSamplingRate		= NULL;
	engine->VChanNPixels				= NULL;
	engine->VChanROIHold				= NULL;
	engine->VChanROIShutter				= NULL;
	engine->scanChans 					= NULL;
	engine->nScanChans					= 0;
	// composite image display
	engine->compositeImgDisplay			= NULL;
	engine->nActivePixelBuildersTSV		= 0;
	// optics
	engine->objectives					= 0;
	// new objective panel handle
	engine->newObjectivePanHndl			= 0;
	// scan settings panel handle
	engine->scanPanHndl					= 0;
	engine->frameScanPanHndl			= 0;
	engine->pointScanPanHndl			= 0;
	engine->activeDisplay				= NULL;
	// scan engine settings panel handle
	engine->engineSetPanHndl			= 0;
	engine->referenceClockFreq			= referenceClockFreq;
	engine->pixDelay					= pixelDelay;
	engine->shutterSwitchTime			= shutterSwitchTime;
	// optics
	engine->scanLensFL					= scanLensFL;
	engine->tubeLensFL					= tubeLensFL;
	engine->objectiveLens				= NULL;
	
	engine->discardFptr					= (DiscardFptr_type)discard_ScanEngine_type;	   // overriden by derived scan engine classes
	
	//------------------------
	// allocate
	//------------------------
	
	// VChans
	nullChk( engine->VChanFastAxisCom			= init_SourceVChan_type(fastAxisComVChanName, DL_RepeatedWaveform_Double, engine, FastAxisComVChan_StateChange) ); 
	nullChk( engine->VChanFastAxisComNSamples	= init_SourceVChan_type(fastAxisComNSampVChanName, DL_UInt64, engine, NULL) ); 
	nullChk( engine->VChanSlowAxisCom			= init_SourceVChan_type(slowAxisComVChanName, DL_RepeatedWaveform_Double, engine, SlowAxisComVChan_StateChange) ); 
	nullChk( engine->VChanSlowAxisComNSamples	= init_SourceVChan_type(slowAxisComNSampVChanName, DL_UInt64, engine, NULL) ); 
	nullChk( engine->VChanGalvoComSamplingRate	= init_SourceVChan_type(galvoComSamplingRateVChanName, DL_Double, engine, GalvoComSampleRateVChan_StateChange) ); 
	nullChk( engine->VChanFastAxisPos			= init_SinkVChan_type(fastAxisPosVChanName, allowedScanAxisPacketTypes, NumElem(allowedScanAxisPacketTypes), engine, VChanDataTimeout, NULL) ); 
	nullChk( engine->VChanSlowAxisPos			= init_SinkVChan_type(slowAxisPosVChanName, allowedScanAxisPacketTypes, NumElem(allowedScanAxisPacketTypes), engine, VChanDataTimeout, NULL) ); 
	nullChk( engine->VChanCompositeImage		= init_SourceVChan_type(compositeImageVChanName, DL_Image, engine, NULL) );
	nullChk( engine->VChanPixelPulseTrain		= init_SourceVChan_type(pixelPulseTrainVChanName, DL_PulseTrain_Ticks, engine, NULL) ); 	
	nullChk( engine->VChanPixelSamplingRate		= init_SourceVChan_type(pixelSamplingRateVChanName, DL_Double, engine, NULL) ); 	
	nullChk( engine->VChanNPixels				= init_SourceVChan_type(nPixelsVChanName, DL_UInt64, engine, NULL) ); 	
	nullChk( engine->objectives					= ListCreate(sizeof(Objective_type*)) ); 
	nullChk( engine->VChanROIHold				= init_SourceVChan_type(ROIHoldVChanName, DL_Waveform_UShort, engine, NULL) );
	nullChk( engine->VChanROIShutter			= init_SourceVChan_type(ROIShutterVChanName, DL_Waveform_UShort, engine, NULL) );
	
	// register Sink VChans with the task controller
	errChk( AddSinkVChan(engine->taskControl, engine->VChanFastAxisPos, NULL, NULL) );
	errChk( AddSinkVChan(engine->taskControl, engine->VChanSlowAxisPos, NULL, NULL) );
	
	// composite image thread safe variable
	errChk( CmtNewTSV(sizeof(int), &engine->nActivePixelBuildersTSV) ); 
	
	// cleanup
	OKfree(fastAxisComVChanName);
	OKfree(fastAxisComNSampVChanName);
	OKfree(slowAxisComVChanName);
	OKfree(slowAxisComNSampVChanName);
	OKfree(galvoComSamplingRateVChanName);
	OKfree(fastAxisPosVChanName);
	OKfree(slowAxisPosVChanName);
	OKfree(compositeImageVChanName);
	OKfree(pixelPulseTrainVChanName);
	OKfree(pixelSamplingRateVChanName);
	OKfree(nPixelsVChanName);
	OKfree(ROIHoldVChanName);
	OKfree(ROIShutterVChanName);
	
	return 0;
	
Error:
	
	// cleanup
	OKfree(fastAxisComVChanName);
	OKfree(fastAxisComNSampVChanName);
	OKfree(slowAxisComVChanName);
	OKfree(slowAxisComNSampVChanName);
	OKfree(galvoComSamplingRateVChanName);
	OKfree(fastAxisPosVChanName);
	OKfree(slowAxisPosVChanName);
	OKfree(compositeImageVChanName);
	OKfree(pixelPulseTrainVChanName);
	OKfree(pixelSamplingRateVChanName);
	OKfree(nPixelsVChanName);
	OKfree(ROIHoldVChanName);
	OKfree(ROIShutterVChanName);
	
	discard_ScanEngine_type(enginePtr);
	return errorInfo.error;
}

static void	discard_ScanEngine_type (ScanEngine_type** enginePtr)
{
	ScanEngine_type* engine = *enginePtr;
	
	// VChans
	RemoveSinkVChan(engine->taskControl, engine->VChanFastAxisPos, NULL);
	RemoveSinkVChan(engine->taskControl, engine->VChanSlowAxisPos, NULL);
	
	discard_VChan_type((VChan_type**)&engine->VChanFastAxisCom);
	discard_VChan_type((VChan_type**)&engine->VChanFastAxisComNSamples);
	discard_VChan_type((VChan_type**)&engine->VChanSlowAxisCom);
	discard_VChan_type((VChan_type**)&engine->VChanSlowAxisComNSamples);
	discard_VChan_type((VChan_type**)&engine->VChanGalvoComSamplingRate);
	discard_VChan_type((VChan_type**)&engine->VChanFastAxisPos);
	discard_VChan_type((VChan_type**)&engine->VChanSlowAxisPos);
	discard_VChan_type((VChan_type**)&engine->VChanCompositeImage);
	discard_VChan_type((VChan_type**)&engine->VChanPixelPulseTrain);
	discard_VChan_type((VChan_type**)&engine->VChanPixelSamplingRate);
	discard_VChan_type((VChan_type**)&engine->VChanNPixels);
	discard_VChan_type((VChan_type**)&engine->VChanROIHold);
	discard_VChan_type((VChan_type**)&engine->VChanROIShutter); 
	
	// discard composite image display
	discard_ImageDisplay_type(&engine->compositeImgDisplay);
	
	// discard active pixel builders thread safe variable
	if (engine->nActivePixelBuildersTSV) {
		CmtDiscardTSV(engine->nActivePixelBuildersTSV);
		engine->nActivePixelBuildersTSV = 0;
	}
	
	// detection channels
	for (size_t i = 0; i < engine->nScanChans; i++)
		discard_ScanChan_type(&engine->scanChans[i]);
	
	engine->nScanChans = 0;
	OKfree(engine->scanChans);
	
	// objectives
	size_t nObjectives = ListNumItems(engine->objectives);
	Objective_type** objectivesPtr;
	for (size_t i = 1; i <= nObjectives; i++) {
		objectivesPtr = ListGetPtrToItem(engine->objectives, i);
		discard_Objective_type(objectivesPtr);
	}
	ListDispose(engine->objectives);
	
	// Task controller
	discard_TaskControl_type(&engine->taskControl);
	
	// UI
	
	if (engine->engineSetPanHndl) {DiscardPanel(engine->engineSetPanHndl); engine->engineSetPanHndl = 0;}
	if (engine->frameScanPanHndl) Combo_DiscardComboBox(engine->frameScanPanHndl, ScanTab_PixelDwell);
	if (engine->frameScanPanHndl) Combo_DiscardComboBox(engine->frameScanPanHndl, ScanTab_Width);
	
	OKfree(*enginePtr);
}

static int DLRegisterScanEngine (ScanEngine_type* engine)
{
	// add scan engine task controller to DAQLab framework
	DLAddTaskController((DAQLabModule_type*)engine->lsModule, engine->taskControl);
		
	// add scan engine VChans to DAQLab framework
	DLRegisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanFastAxisCom);
	DLRegisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanFastAxisComNSamples);
	DLRegisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanSlowAxisCom);
	DLRegisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanSlowAxisComNSamples);
	DLRegisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanGalvoComSamplingRate);
	DLRegisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanFastAxisPos);
	DLRegisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanSlowAxisPos);
	DLRegisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanCompositeImage);
	DLRegisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanPixelPulseTrain);
	DLRegisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanPixelSamplingRate);
	DLRegisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanNPixels);
	DLRegisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanROIHold);
	DLRegisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanROIShutter); 
		
	for (size_t i = 0; i < engine->nScanChans; i++)
		RegisterDLScanChan(engine->scanChans[i]);
	
	
	return 0;
}
	
static void DLUnregisterScanEngine (ScanEngine_type* engine)
{
	//----------------------------------
	// VChans
	//----------------------------------
	// fast axis command
	if (engine->VChanFastAxisCom)
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanFastAxisCom);
	
	// fast axis command num samples
	if (engine->VChanFastAxisComNSamples)
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanFastAxisComNSamples);
	
	// slow axis command
	if (engine->VChanSlowAxisCom)
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanSlowAxisCom);
	
	// slow axis command n samples
	if (engine->VChanSlowAxisComNSamples)
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanSlowAxisComNSamples);
	
	// galvo command sampling rate
	if (engine->VChanGalvoComSamplingRate)
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanGalvoComSamplingRate);
	
	// fast axis position feedback
	if (engine->VChanFastAxisPos) {
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanFastAxisPos);
		RemoveSinkVChan(engine->taskControl, engine->VChanFastAxisPos, NULL); 
	}
	
	// slow axis position feedback
	if (engine->VChanSlowAxisPos) {
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanSlowAxisPos);
		RemoveSinkVChan(engine->taskControl, engine->VChanSlowAxisPos, NULL);
	}
	
	// scan engine image output
	if (engine->VChanCompositeImage)
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanCompositeImage);
	
	// pixel pulse train
	if (engine->VChanPixelPulseTrain)
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanPixelPulseTrain);
	
	// pixel sampling rate
	if (engine->VChanPixelSamplingRate)
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanPixelSamplingRate);
	
	// n pixels
	if (engine->VChanNPixels)
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanNPixels);
	
	// ROI hold
	if (engine->VChanROIHold)
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanROIHold);
	
	// ROI shutter
	if (engine->VChanROIShutter)
		DLUnregisterVChan((DAQLabModule_type*)engine->lsModule, (VChan_type*)engine->VChanROIShutter);
	
	// scan channels
	for (size_t i = 0; i < engine->nScanChans; i++)
		UnregisterDLScanChan(engine->scanChans[i]);
		
	//----------------------------------
	// Task controller
	//----------------------------------
	if (engine->taskControl)
		DLRemoveTaskController((DAQLabModule_type*)engine->lsModule, engine->taskControl); 
		
}

static int SendScanAxisCommand (ScanEngine_type* engine, RasterScanAxes scanAxis, double voltage, char** errorMsg)
{
INIT_ERR
	
	double*					signal			= NULL; 
	DataPacket_type*		dataPacket		= NULL;
	
	switch(scanAxis) {
			
		case ScanAxis_Fast:
			
			if (IsVChanOpen((VChan_type*)engine->VChanFastAxisCom)) {
				nullChk( signal = malloc(sizeof(double)) );
				*signal = voltage;
				nullChk( dataPacket = init_DataPacket_type(DL_Double, (void**)&signal, NULL, NULL) );
				errChk( SendDataPacket(engine->VChanFastAxisCom, &dataPacket, FALSE, &errorInfo.errMsg) );
			}
			break;
			
		case ScanAxis_Slow:
			
			if (IsVChanOpen((VChan_type*)engine->VChanSlowAxisCom)) {
				nullChk( signal = malloc(sizeof(double)) );
				*signal = voltage;
				nullChk( dataPacket = init_DataPacket_type(DL_Double, (void**)&signal, NULL, NULL) );
				errChk( SendDataPacket(engine->VChanSlowAxisCom, &dataPacket, FALSE, &errorInfo.errMsg) );
			}
			break;
	}

	return 0;
	
Error:
	
	OKfree(signal);
	discard_DataPacket_type(&dataPacket);
	
RETURN_ERR
}

static int ReturnRectRasterToParkedPosition (RectRaster_type* engine, char** errorMsg) 
{
INIT_ERR
	
	double*						parkedFastAxis				= NULL;
	double*						parkedSlowAxis				= NULL;
	size_t						nParkedSamples				= (size_t) (engine->galvoSamplingRate * 0.2); // generate 0.2s long parked signal
	RepeatedWaveform_type*		parkedFastAxisWaveform		= NULL;
	RepeatedWaveform_type*		parkedSlowAxisWaveform		= NULL;
	DataPacket_type*			fastAxisDataPacket			= NULL;
	DataPacket_type*			slowAxisDataPacket			= NULL;
	DSInfo_type*				dsInfo						= NULL;
	
	
	nullChk( parkedFastAxis = malloc(nParkedSamples * sizeof(double)) );
	nullChk( parkedSlowAxis = malloc(nParkedSamples * sizeof(double)) );
	Set1D(parkedFastAxis, nParkedSamples, ((NonResGalvoCal_type*)engine->baseClass.fastAxisCal)->parked);
	Set1D(parkedSlowAxis, nParkedSamples, ((NonResGalvoCal_type*)engine->baseClass.slowAxisCal)->parked);
	nullChk( parkedFastAxisWaveform = init_RepeatedWaveform_type(RepeatedWaveform_Double, engine->galvoSamplingRate, nParkedSamples, (void**)&parkedFastAxis, 0) );
	nullChk( parkedSlowAxisWaveform = init_RepeatedWaveform_type(RepeatedWaveform_Double, engine->galvoSamplingRate, nParkedSamples, (void**)&parkedSlowAxis, 0) );
	nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(engine->baseClass.taskControl), WAVERANK) );
	nullChk( fastAxisDataPacket		= init_DataPacket_type(DL_RepeatedWaveform_Double, (void**)&parkedFastAxisWaveform, &dsInfo, (DiscardFptr_type)discard_RepeatedWaveform_type) );
	nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(engine->baseClass.taskControl), WAVERANK) );
	nullChk( slowAxisDataPacket		= init_DataPacket_type(DL_RepeatedWaveform_Double, (void**)&parkedSlowAxisWaveform, &dsInfo, (DiscardFptr_type)discard_RepeatedWaveform_type) );
	// send parked signal data packets
	errChk( SendDataPacket(engine->baseClass.VChanSlowAxisCom, &slowAxisDataPacket, FALSE, &errorInfo.errMsg) );
	errChk( SendDataPacket(engine->baseClass.VChanFastAxisCom, &fastAxisDataPacket, FALSE, &errorInfo.errMsg) );    
	// send NULL packets to terminate AO
	errChk( SendNullPacket( engine->baseClass.VChanFastAxisCom, &errorInfo.errMsg) );
	errChk( SendNullPacket( engine->baseClass.VChanSlowAxisCom, &errorInfo.errMsg) );
	
	return 0;
	
Error:

	// cleanup
	OKfree(parkedFastAxis);
	OKfree(parkedSlowAxis);
	discard_RepeatedWaveform_type(&parkedFastAxisWaveform);
	discard_RepeatedWaveform_type(&parkedSlowAxisWaveform);
	discard_DataPacket_type(&fastAxisDataPacket);
	discard_DataPacket_type(&slowAxisDataPacket);
	discard_DSInfo_type(&dsInfo);
	
RETURN_ERR	
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

static RectRasterDisplayCBData_type* init_RectRasterDisplayCBData_type (RectRaster_type* scanEngine, size_t scanChanIdx)
{
	RectRasterDisplayCBData_type*	binding = malloc(sizeof(RectRasterDisplayCBData_type));
	if (!binding) return NULL;
	
	binding->scanEngine		= scanEngine;
	binding->scanSettings   = scanEngine->scanSettings;
	binding->scanChanIdx	= scanChanIdx;
	
	return binding;
}

static void discard_RectRasterDisplayCBData_type (RectRasterDisplayCBData_type** dataPtr)
{
	OKfree(*dataPtr);	
}

static RectRaster_type* init_RectRaster_type (	LaserScanning_type*			lsModule,
												char 						engineName[], 
												BOOL						continuousFrameScan,
												size_t						nFrames,
												double						galvoSamplingRate,
												double						referenceClockFreq,
												double						pixelDelay,
												double						shutterSwitchTime,
												RectRasterScanSet_type**	frameScanSettingsPtr,
												PointScanProtocol_type**	pointScanProtocolPtr,
												ListType*					pointScanProtocolsListPtr,
												double						scanLensFL,
												double						tubeLensFL)
{
INIT_ERR

	RectRaster_type*	rectRaster 						= malloc (sizeof(RectRaster_type));
	TaskControl_type*	taskController  				= NULL;
	Rect_type*			parentROIRect					= NULL;
	BOOL				baseScanEngineClassInitialized 	= FALSE;
	
	if (!rectRaster) return NULL;
	
	//------------------------------------------------------------------------------------------------------------
	// Init base class
	//------------------------------------------------------------------------------------------------------------
	
	// task controller
	nullChk( taskController	= init_TaskControl_type(engineName, NULL, DLGetCommonThreadPoolHndl(), ConfigureTC_RectRaster, UnconfigureTC_RectRaster, IterateTC_RectRaster, StartTC_RectRaster, ResetTC_RectRaster, 
										  DoneTC_RectRaster, StoppedTC_RectRaster, NULL, TaskTreeStateChange_RectRaster, NULL, ModuleEventHandler_RectRaster, ErrorTC_RectRaster) );
	
	SetTaskControlIterationTimeout(taskController, TaskControllerIterationTimeout);
	
	errChk( init_ScanEngine_type((ScanEngine_type**)&rectRaster, lsModule, &taskController, continuousFrameScan, nFrames, ScanEngine_RectRaster_NonResonantGalvoFastAxis_NonResonantGalvoSlowAxis, referenceClockFreq, pixelDelay, shutterSwitchTime, scanLensFL, tubeLensFL) );
	baseScanEngineClassInitialized = TRUE;
	// Note: scan engine base class has been initialized and if there is an error, then the discard method of the base class will be used
	
	//------------------------------------------------------------------------------------------------------------
	// Init RectRaster_type child class
	//------------------------------------------------------------------------------------------------------------
	
	//-----------------------------
	// scan parameters
	//-----------------------------
	
	rectRaster->scanSettings 					= *frameScanSettingsPtr;  // (required)
	*frameScanSettingsPtr 						= NULL;
	
	// parent frame scan settings 
	rectRaster->parentFrameScanSettings			= NULL;
	
	rectRaster->galvoSamplingRate				= galvoSamplingRate;
	rectRaster->flyInDelay						= 0;
	rectRaster->scanROIs						= 0;
	
	//-----------------------------
	// point scan settings
	//-----------------------------
	
	rectRaster->pointScan.startDelay			= 0;
	rectRaster->pointScan.minStartDelay			= 0;
	rectRaster->pointScan.sequenceDuration		= 0;
	rectRaster->pointScan.minSequencePeriod		= 0;
	rectRaster->pointScan.pointJumps			= 0;
	rectRaster->pointScan.currentActivePoint	= 0;
	rectRaster->pointScan.nPointsInGroup		= 0;
	rectRaster->pointScan.jumpTimes				= NULL;
	rectRaster->pointScan.responseDelays		= NULL;
	rectRaster->pointScan.pointScanProtocols	= 0;
	
	if (pointScanProtocolPtr) {
		rectRaster->pointScan.pointScanProtocol		= *pointScanProtocolPtr;
		*pointScanProtocolPtr						= NULL;
		
		// round point scan protocol settings to galvo sampling time
		rectRaster->pointScan.pointScanProtocol->holdTime 				= ceil(rectRaster->pointScan.pointScanProtocol->holdTime * 1e-3 * rectRaster->galvoSamplingRate) * 1e3/rectRaster->galvoSamplingRate;  
		rectRaster->pointScan.pointScanProtocol->holdBurstPeriod 		= ceil(rectRaster->pointScan.pointScanProtocol->holdBurstPeriod * 1e-3 * rectRaster->galvoSamplingRate) * 1e3/rectRaster->galvoSamplingRate;  
		rectRaster->pointScan.pointScanProtocol->holdBurstPeriodIncr 	= ceil(rectRaster->pointScan.pointScanProtocol->holdBurstPeriodIncr * 1e-3 * rectRaster->galvoSamplingRate) * 1e3/rectRaster->galvoSamplingRate;
		rectRaster->pointScan.pointScanProtocol->stimDelay 				= ceil(rectRaster->pointScan.pointScanProtocol->stimDelay * 1e-3 * rectRaster->galvoSamplingRate) * 1e3/rectRaster->galvoSamplingRate;
		rectRaster->pointScan.pointScanProtocol->stimPulseONDuration 	= ceil(rectRaster->pointScan.pointScanProtocol->stimPulseONDuration * 1e-3 * rectRaster->galvoSamplingRate) * 1e3/rectRaster->galvoSamplingRate;
		rectRaster->pointScan.pointScanProtocol->stimPulseOFFDuration 	= ceil(rectRaster->pointScan.pointScanProtocol->stimPulseOFFDuration * 1e-3 * rectRaster->galvoSamplingRate) * 1e3/rectRaster->galvoSamplingRate;
		rectRaster->pointScan.pointScanProtocol->startDelayInitVal 		= ceil(rectRaster->pointScan.pointScanProtocol->startDelayInitVal * 1e-3 * rectRaster->galvoSamplingRate) * 1e3/rectRaster->galvoSamplingRate;
		rectRaster->pointScan.pointScanProtocol->startDelayIncrement	= ceil(rectRaster->pointScan.pointScanProtocol->startDelayIncrement * 1e-3 * rectRaster->galvoSamplingRate) * 1e3/rectRaster->galvoSamplingRate;
	
	} else
		rectRaster->pointScan.pointScanProtocol		= NULL;
		
	//-----------------------------
	// image and point scan buffers
	//-----------------------------
	
	rectRaster->imgBuffers						= NULL;
	rectRaster->nImgBuffers						= 0;
	rectRaster->pointBuffers					= NULL;
	rectRaster->nPointBuffers					= 0;
	
	//-----------------------------
	// Methods
	//-----------------------------
	
	// override discard method
	rectRaster->baseClass.discardFptr = (DiscardFptr_type)discard_RectRaster_type;
	// Note: scan engine child class has been initialized and if there is an error, then the discard method of the child class will be used
	
	//------------------------------------------------------------------------------------------------------------
	// Alloc RectRaster_type child class resources
	//------------------------------------------------------------------------------------------------------------
	
	//-----------------------------
	// frame scan resources
	//-----------------------------
	
	// parent frame scan settings 
	nullChk( rectRaster->parentFrameScanSettings = init_RectRasterScanSet_type(rectRaster->scanSettings->pixSize, rectRaster->scanSettings->height, rectRaster->scanSettings->heightOffset, rectRaster->scanSettings->width, rectRaster->scanSettings->widthOffset, rectRaster->scanSettings->pixelDwellTime) );
	
	nullChk( rectRaster->scanROIs = ListCreate(sizeof(Rect_type*)) );
	
	// Add parent frame scan ROI to frame scan ROI list
	RGBA_type	parentRectROIColor	= {.R = 0, .G = 0, .B = 0, .alpha = 255}; // transparent
	nullChk( parentROIRect = initalloc_Rect_type(NULL, "parent", parentRectROIColor, TRUE, 0, 0, rectRaster->scanSettings->height, rectRaster->scanSettings->width) );
	nullChk( ListInsertItem(rectRaster->scanROIs, &parentROIRect, END_OF_LIST) );
	parentROIRect = NULL;
	
	//-----------------------------
	// point scan resources
	//-----------------------------
	
	nullChk( rectRaster->pointScan.pointJumps = ListCreate(sizeof(PointJump_type*)) );
	if (pointScanProtocolsListPtr) {
		rectRaster->pointScan.pointScanProtocols = *pointScanProtocolsListPtr;
		*pointScanProtocolsListPtr = 0;
	} else {
		nullChk( rectRaster->pointScan.pointScanProtocols = ListCreate(sizeof(PointScanProtocol_type*)) );
	}
		
	return rectRaster;
	
Error:
	
	// cleanup
	discard_Rect_type(&parentROIRect);
	discard_TaskControl_type(&taskController);
	if (baseScanEngineClassInitialized)
		(*rectRaster->baseClass.discardFptr) ((void**)&rectRaster);
	
	return NULL;
}

static void	discard_RectRaster_type (RectRaster_type** rectRasterPtr)
{
	RectRaster_type*	rectRaster = *rectRasterPtr;
	
	if (!rectRaster) return;
	
	//--------------------------------------------------------------------
	// discard RectRaster_type data
	//--------------------------------------------------------------------
	
	//----------------------------------
	// Image and point scan buffers
	//----------------------------------

	for (size_t i = 0; i < rectRaster->nImgBuffers; i++)
		discard_RectRasterImgBuff_type(&rectRaster->imgBuffers[i]);
	
	OKfree(rectRaster->imgBuffers);
	rectRaster->nImgBuffers = 0;
	
	for (size_t i = 0; i < rectRaster->nPointBuffers; i++)
		discard_RectRasterPointBuff_type(&rectRaster->pointBuffers[i]);
	
	OKfree(rectRaster->pointBuffers);
	rectRaster->nPointBuffers = 0;
	
	//----------------------------------
	// Frame scan settings
	//----------------------------------
	
	discard_RectRasterScanSet_type(&rectRaster->scanSettings);
	discard_RectRasterScanSet_type(&rectRaster->parentFrameScanSettings);

	//----------------------------------
	// Point scan settings
	//----------------------------------
	
	// discard point jumps
	OKfreeList(&rectRaster->pointScan.pointJumps, (DiscardFptr_type)discard_PointJump_type);
	
	// discard point jump protocols
	OKfreeList(&rectRaster->pointScan.pointScanProtocols, (DiscardFptr_type)discard_PointScanProtocol_type);
	
	OKfree(rectRaster->pointScan.jumpTimes);
	OKfree(rectRaster->pointScan.responseDelays);
	
	discard_PointScanProtocol_type(&rectRaster->pointScan.pointScanProtocol);
	
	//----------------------------------
	// Scan ROIs
	//----------------------------------
	
	OKfreeList(&rectRaster->scanROIs, (DiscardFptr_type)discard_Rect_type);
	
	//--------------------------------------------------------------------
	// discard Scan Engine data
	//--------------------------------------------------------------------
	discard_ScanEngine_type((ScanEngine_type**)rectRasterPtr);
}

static int SetRectRasterPointScanProtocol (RectRaster_type* rectRaster, PointScanProtocol_type* pointScanProtocol)
{
INIT_ERR

	discard_PointScanProtocol_type(&rectRaster->pointScan.pointScanProtocol);
	nullChk( rectRaster->pointScan.pointScanProtocol = copy_PointScanProtocol_type(pointScanProtocol) );
	
Error:
	
	return errorInfo.error;
}

static RectRasterImgBuff_type* init_RectRasterImgBuff_type (ScanChan_type* scanChan, uInt32 imgHeight, uInt32 imgWidth, DLDataTypes pixelDataType, BOOL flipRows)
{
#define init_RectRasterImgBuff_type_Err_PixelDataTypeInvalid	-1
	RectRasterImgBuff_type*	buffer 			= malloc (sizeof(RectRasterImgBuff_type));
	if (!buffer) return NULL;
	
	// init
	buffer->nImagePixels 			= 0;
	buffer->nAssembledRows   		= 0;
	buffer->tmpPixels				= NULL;
	buffer->nTmpPixels				= 0;
	buffer->nSkipPixels				= 0;			// calculated once scan signals are calculated
	buffer->flipRow					= flipRows;
	buffer->skipRows				= FALSE;
	buffer->skipFlybackRows			= 0;			// calculated once scan signals are calculated
	buffer->rowsSkipped				= 0;
	buffer->pixelDataType			= pixelDataType;
	buffer->scanChan				= scanChan;
	buffer->imagePixels				= NULL;
	buffer->image					= NULL;
	
	return buffer;
}

static void ResetRectRasterImgBuffer (RectRasterImgBuff_type* imgBuffer, BOOL flipRows)
{
	imgBuffer->nImagePixels 		= 0;
	imgBuffer->nAssembledRows   	= 0; 
	OKfree(imgBuffer->tmpPixels);
	imgBuffer->nTmpPixels			= 0;
	imgBuffer->nSkipPixels			= 0;
	imgBuffer->flipRow				= flipRows;
	imgBuffer->skipRows				= FALSE;
	imgBuffer->skipFlybackRows		= 0;			// calculated once scan signals are calculated
	imgBuffer->rowsSkipped			= 0;
	discard_Image_type(&imgBuffer->image);
}

static void	discard_RectRasterImgBuff_type (RectRasterImgBuff_type** imgBufferPtr)
{
	RectRasterImgBuff_type*	imgBuffer = *imgBufferPtr;
	if (!imgBuffer) return;
	
	OKfree(imgBuffer->imagePixels);
	OKfree(imgBuffer->tmpPixels);
	discard_Image_type(&imgBuffer->image);
	
	OKfree(*imgBufferPtr);
}

static RectRasterPointBuff_type* init_RectRasterPointBuff_type (ScanChan_type* scanChan)
{
	RectRasterPointBuff_type*		pointBuffer = malloc(sizeof(RectRasterPointBuff_type));
	if (!pointBuffer) return NULL;
	
	// init
	pointBuffer->nSkipPixels		= 0;
	pointBuffer->rawPixels			= NULL;
	pointBuffer->integratedPixels	= NULL;
	pointBuffer->scanChan			= scanChan;
	
	return pointBuffer;
	
}

static void discard_RectRasterPointBuff_type (RectRasterPointBuff_type** pointBufferPtr)
{
	RectRasterPointBuff_type*	pointBuffer = *pointBufferPtr;
	
	// raw pixels
	discard_Waveform_type(&pointBuffer->rawPixels);
	// integrated pixels
	discard_Waveform_type(&pointBuffer->integratedPixels);
	
	OKfree(*pointBufferPtr);
}

static void ResetRectRasterPointBuffer (RectRasterPointBuff_type* pointBuffer) 
{
	// discard raw pixels
	discard_Waveform_type(&pointBuffer->rawPixels);
	// discard integrated pixels
	discard_Waveform_type(&pointBuffer->integratedPixels);
}

static int CVICALLBACK NonResRectRasterScan_MainPan_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	//RectRaster_type*		scanEngine		= callbackData;
	
	switch (event) {
			
		case EVENT_ACTIVE_TAB_CHANGE:
			
			switch (control) {
					
				case RectRaster_Tab:
					
					
					
					break;
			}
			
			break;
	}
	
	return 0;
}


static int CVICALLBACK NonResRectRasterScan_FrameScanPan_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	RectRaster_type*		scanEngine		= callbackData;
	NonResGalvoCal_type*	fastAxisCal		= (NonResGalvoCal_type*) scanEngine->baseClass.fastAxisCal;     
	NonResGalvoCal_type*	slowAxisCal		= (NonResGalvoCal_type*) scanEngine->baseClass.slowAxisCal;
	
	switch (event) {
			
		case EVENT_COMMIT:
			
			switch (control) {
			
				case ScanTab_Objective:
					
					Objective_type**	objectivePtr	= NULL;
					int					listItemIdx		= 0;
					
					GetCtrlIndex(panel, control, &listItemIdx);
					objectivePtr = ListGetPtrToItem(scanEngine->baseClass.objectives, listItemIdx+1);
					scanEngine->baseClass.objectiveLens = *objectivePtr;
					
					// configure/unconfigure scan engine
					if (NonResRectRasterScan_ValidConfig(scanEngine)) {
				
						errChk( NonResRectRasterScan_ScanWidths(scanEngine, &errorInfo.errMsg) );
						errChk( NonResRectRasterScan_PixelDwellTimes(scanEngine, &errorInfo.errMsg) );
					
						if (NonResRectRasterScan_ReadyToScan(scanEngine))
							errChk( TaskControlEvent(scanEngine->baseClass.taskControl, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
						else
							errChk( TaskControlEvent(scanEngine->baseClass.taskControl, TC_Event_Unconfigure, NULL, NULL, &errorInfo.errMsg) ); 	
					} else
						errChk( TaskControlEvent(scanEngine->baseClass.taskControl, TC_Event_Unconfigure, NULL, NULL, &errorInfo.errMsg) );
					
					break;
					
				case ScanTab_ExecutionMode:
					
					GetCtrlVal(panel, control, &scanEngine->baseClass.continuousAcq);
					// dim/undim N Frames
					if (scanEngine->baseClass.continuousAcq)
						SetCtrlAttribute(panel, ScanTab_NFrames, ATTR_DIMMED, TRUE);
					else
						SetCtrlAttribute(panel, ScanTab_NFrames, ATTR_DIMMED, FALSE);
					
					break;
					
				case ScanTab_ROIs:
					
					break;
					
				case ScanTab_NFrames:
					
					unsigned int nFrames = 0;
					
					GetCtrlVal(panel, control, &nFrames);
					scanEngine->baseClass.nFrames = nFrames;
					
					break;
				
				
				case ScanTab_Width:
					
					char   		widthString[NonResGalvoRasterScan_Max_ComboboxEntryLength+1] 	= "";
					double		width															= 0;
					uInt32		widthPix														= 0;
					
					// get image width
					GetCtrlVal(panel, control, widthString);
					if (Scan(widthString, "%s>%f", &width) <= 0) {
						// invalid entry, display previous value
						Fmt(widthString, "%s<%f[p1]", scanEngine->scanSettings->width * scanEngine->scanSettings->pixSize);
						SetCtrlVal(panel, control, widthString);
						return 0;
					}
					
					// no negative or zero value for width
					if (width <= 0){
						Fmt(widthString, "%s<%f[p1]", scanEngine->scanSettings->width * scanEngine->scanSettings->pixSize);
						SetCtrlVal(panel, control, widthString);
						return 0;
					}
					
					// make sure width is a multiple of the pixel size
					widthPix = (uInt32) ceil(width/scanEngine->scanSettings->pixSize);
					width = widthPix * scanEngine->scanSettings->pixSize;
					Fmt(widthString, "%s<%f[p1]", width);
					SetCtrlVal(panel, control, widthString);
					
					// check if requested image falls within ROI
					if(!RectROIInsideRect (scanEngine->scanSettings->height * scanEngine->scanSettings->pixSize, width, scanEngine->scanSettings->heightOffset * scanEngine->scanSettings->pixSize,
										   scanEngine->scanSettings->widthOffset * scanEngine->scanSettings->pixSize, 2 * slowAxisCal->commandVMax * slowAxisCal->sampleDisplacement,
										  2 * fastAxisCal->commandVMax * fastAxisCal->sampleDisplacement)) {
						DLMsg("Requested ROI exceeds maximum limit for laser scanning module ", 1);
						char*	scanEngineName = GetTaskControlName(scanEngine->baseClass.taskControl);
						DLMsg(scanEngineName, 0);
						OKfree(scanEngineName);
						DLMsg(".\n\n", 0);
						Fmt(widthString, "%s<%f[p1]", scanEngine->scanSettings->width * scanEngine->scanSettings->pixSize);
						SetCtrlVal(panel, control, widthString);
						return 0;
					}
					
					// update width
					scanEngine->scanSettings->width = widthPix;
					Fmt(widthString, "%s<%f[p1]", width);  
					SetCtrlVal(panel, control, widthString); 
					// update pixel dwell times
					errChk( NonResRectRasterScan_PixelDwellTimes(scanEngine, &errorInfo.errMsg) ); 
					
					break;
					
				case ScanTab_Height:
					
					double		height;					// in [um]
					uInt32		heightPix;				// in [pix]
					GetCtrlVal(panel, control, &height);
					// adjust height to be a multiple of pixel size
					heightPix 	= (uInt32) ceil(height/scanEngine->scanSettings->pixSize);
					if (!heightPix) heightPix = 1;
					height 		= heightPix * scanEngine->scanSettings->pixSize;
					
					if (!RectROIInsideRect(height, scanEngine->scanSettings->width * scanEngine->scanSettings->pixSize, scanEngine->scanSettings->heightOffset * scanEngine->scanSettings->pixSize,
										   scanEngine->scanSettings->widthOffset * scanEngine->scanSettings->pixSize, 2 * slowAxisCal->commandVMax * slowAxisCal->sampleDisplacement, 
										   2 * fastAxisCal->commandVMax * fastAxisCal->sampleDisplacement)) {
							// return to previous value
							SetCtrlVal(panel, control, scanEngine->scanSettings->height * scanEngine->scanSettings->pixSize);
							DLMsg("Requested ROI exceeds maximum limit for laser scanning module ", 1);
							char*	scanEngineName = GetTaskControlName(scanEngine->baseClass.taskControl);
							DLMsg(scanEngineName, 0);
							OKfree(scanEngineName);
							DLMsg(".\n\n", 0);
							return 0; // stop here
						}
						
					// update height
					scanEngine->scanSettings->height = heightPix;  // in [pix]
					SetCtrlVal(panel, control, height); 
					break;
					
				case ScanTab_WidthOffset:
					
					double		widthOffset;					// in [um]
					int			widthOffsetPix;					// in [pix]
					GetCtrlVal(panel, control, &widthOffset);  	// in [um]
					// adjust widthOffset to be a multiple of pixel size
					widthOffsetPix		= (int) floor(widthOffset/scanEngine->scanSettings->pixSize);
					widthOffset 		= widthOffsetPix * scanEngine->scanSettings->pixSize;
					
					if (!RectROIInsideRect(scanEngine->scanSettings->height * scanEngine->scanSettings->pixSize, scanEngine->scanSettings->width * scanEngine->scanSettings->pixSize, 
										   scanEngine->scanSettings->heightOffset * scanEngine->scanSettings->pixSize, widthOffset, 2 * slowAxisCal->commandVMax * slowAxisCal->sampleDisplacement,
										  2 * fastAxisCal->commandVMax * fastAxisCal->sampleDisplacement)) {
							// return to previous value
							SetCtrlVal(panel, control, scanEngine->scanSettings->widthOffset * scanEngine->scanSettings->pixSize);
							DLMsg("Requested ROI exceeds maximum limit for laser scanning module ", 1);
							char*	scanEngineName = GetTaskControlName(scanEngine->baseClass.taskControl);
							DLMsg(scanEngineName, 0);
							OKfree(scanEngineName);
							DLMsg(".\n\n", 0);
							return 0; // stop here
					}
							
					// update width offset
					scanEngine->scanSettings->widthOffset = widthOffsetPix;  // in [pix]
					SetCtrlVal(panel, control, widthOffset);
					break;
					
				case ScanTab_HeightOffset:
					
					double		heightOffset;					// in [um]
					int			heightOffsetPix;				// in [pix]
					GetCtrlVal(panel, control, &heightOffset);
					// adjust heightOffset to be a multiple of pixel size
					heightOffsetPix 	= (int) floor(heightOffset/scanEngine->scanSettings->pixSize);
					heightOffset 		= heightOffsetPix * scanEngine->scanSettings->pixSize;
					
					if (!RectROIInsideRect(scanEngine->scanSettings->height * scanEngine->scanSettings->pixSize, scanEngine->scanSettings->width * scanEngine->scanSettings->pixSize, 
						heightOffset, scanEngine->scanSettings->widthOffset * scanEngine->scanSettings->pixSize, 2 * slowAxisCal->commandVMax * slowAxisCal->sampleDisplacement,
						2 * fastAxisCal->commandVMax * fastAxisCal->sampleDisplacement)) {
							// return to previous value
							SetCtrlVal(panel, control, scanEngine->scanSettings->heightOffset * scanEngine->scanSettings->pixSize);
							DLMsg("Requested ROI exceeds maximum limit for laser scanning module ", 1);
							char*	scanEngineName = GetTaskControlName(scanEngine->baseClass.taskControl);
							DLMsg(scanEngineName, 0);
							OKfree(scanEngineName);
							DLMsg(".\n\n", 0);
							return 0; // stop here
						}
						
					// update height offset
					scanEngine->scanSettings->heightOffset = heightOffsetPix;  // in [pix]
					SetCtrlVal(panel, control, heightOffset); 
					break;
					
				case ScanTab_PixelDwell:
					
					char   		dwellTimeString[NonResGalvoRasterScan_Max_ComboboxEntryLength+1];
					double		dwellTime;
					
					// get dwell time string
					GetCtrlVal(panel, control, dwellTimeString);
					if (Scan(dwellTimeString, "%s>%f", &dwellTime) <= 0) {
						// invalid entry, display previous value
						Fmt(dwellTimeString, "%s<%f[p1]", scanEngine->scanSettings->pixelDwellTime);
						SetCtrlVal(panel, control, dwellTimeString);
						return 0;
					}
					
					// dwell time must be within bounds
					if ((dwellTime < NonResGalvoScan_MinPixelDwellTime) || (dwellTime > NonResGalvoScan_MaxPixelDwellTime)){
						Fmt(dwellTimeString, "%s<%f[p1]", scanEngine->scanSettings->pixelDwellTime);
						SetCtrlVal(panel, control, dwellTimeString);
						return 0;
					}
					
					// make sure dwell time is a multiple of the pixel clock rate
					dwellTime =  ceil(dwellTime * scanEngine->baseClass.referenceClockFreq) /scanEngine->baseClass.referenceClockFreq;
					Fmt(dwellTimeString, "%s<%f[p1]", dwellTime);
					SetCtrlVal(panel, control, dwellTimeString);	  // <--- can be taken out
					
					scanEngine->scanSettings->pixelDwellTime = dwellTime;
					// update preferred widths and pixel dwell times
					errChk( NonResRectRasterScan_ScanWidths(scanEngine, &errorInfo.errMsg) );
				
					break;
					
				case ScanTab_FPS:
					
					break;
					
				case ScanTab_PixelSize:
					
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
					scanEngine->scanSettings->height 		= (uInt32) (scanEngine->scanSettings->height * scanEngine->scanSettings->pixSize/newPixelSize);
					scanEngine->scanSettings->heightOffset	= (int) (scanEngine->scanSettings->heightOffset * scanEngine->scanSettings->pixSize/newPixelSize);
					scanEngine->scanSettings->width 		= (uInt32) (scanEngine->scanSettings->width * scanEngine->scanSettings->pixSize/newPixelSize);
					scanEngine->scanSettings->widthOffset 	= (int) (scanEngine->scanSettings->widthOffset * scanEngine->scanSettings->pixSize/newPixelSize);
					// update pixel size
					scanEngine->scanSettings->pixSize = newPixelSize;
					// adjust height control to be multiple of new pixel size
					SetCtrlVal(panel, ScanTab_Height, scanEngine->scanSettings->height * scanEngine->scanSettings->pixSize);
					// adjust height offset control to be multiple of new pixel size
					SetCtrlVal(panel, ScanTab_HeightOffset, scanEngine->scanSettings->heightOffset * scanEngine->scanSettings->pixSize);
					// adjust width offset control to be multiple of new pixel size
					SetCtrlVal(panel, ScanTab_WidthOffset, scanEngine->scanSettings->widthOffset * scanEngine->scanSettings->pixSize);
					// update preferred widths
					errChk( NonResRectRasterScan_ScanWidths(scanEngine, &errorInfo.errMsg) );
					// update preferred pixel dwell times
					errChk( NonResRectRasterScan_PixelDwellTimes(scanEngine, &errorInfo.errMsg) );
					
					break;
			}
			break;
			
		case EVENT_KEYPRESS:
			
			switch (control) {
					
				case ScanTab_ROIs:
					
					// continue only if Del key is pressed
					if (eventData1 != VAL_FWD_DELETE_VKEY) break;
					
					int itemIdx = 0;
					
					GetCtrlIndex(panel, control, &itemIdx);
					if (itemIdx < 1) break; // stop here if list has only the parent frame scan ROI
					
					// get ROI name from UI list
					ROI_type*	ROIItem = *(ROI_type**) ListGetPtrToItem(scanEngine->scanROIs, itemIdx+1);
					
					// if there is an active display, remove ROI item from display and from scan engine list
					if (scanEngine->baseClass.activeDisplay)
						(*scanEngine->baseClass.activeDisplay->ROIActionsFptr) (scanEngine->baseClass.activeDisplay, ROIItem->ROIName, ROI_Delete);
					
					DeleteListItem(scanEngine->baseClass.frameScanPanHndl, ScanTab_ROIs, itemIdx, 1);
					ListRemoveItem(scanEngine->scanROIs, 0, itemIdx + 1);
					
					// update subregion scan settings to be in sync with the UI after deleting a list entry
					GetCtrlIndex(panel, control, &itemIdx);
					errChk( NonResRectRasterScan_SetSubregionFrameScan(scanEngine, itemIdx + 1, &errorInfo.errMsg) );
					
					break;
					
			}
			break;
			
		case EVENT_VAL_CHANGED:
			
			switch (control) {
					
				case ScanTab_ROIs:
					
					int	itemIdx	= 0;
					
					GetCtrlIndex(panel, control, &itemIdx);
					errChk( NonResRectRasterScan_SetSubregionFrameScan(scanEngine, itemIdx + 1, &errorInfo.errMsg) );
					break;
			}
			
		case EVENT_MARK_STATE_CHANGE:
			
			switch (control) {
					
				case ScanTab_ROIs:
					
					if (eventData1) {
						// ROI marked as active
						ROI_type*	ROI = *(ROI_type**) ListGetPtrToItem(scanEngine->scanROIs, eventData2 + 1);
						ROI->active = TRUE;
						// if there is an active display, mark point ROI as active and make it visible
						if (scanEngine->baseClass.activeDisplay)
							(*scanEngine->baseClass.activeDisplay->ROIActionsFptr) (scanEngine->baseClass.activeDisplay, ROI->ROIName, ROI_Show);
					} else {
						// ROI marked as inactive
						ROI_type*	ROI = *(ROI_type**) ListGetPtrToItem(scanEngine->scanROIs, eventData2 + 1);
						ROI->active = FALSE;
						// if there is an active display mark point ROI as inactive and hide it
						if (scanEngine->baseClass.activeDisplay)
							(*scanEngine->baseClass.activeDisplay->ROIActionsFptr) (scanEngine->baseClass.activeDisplay, ROI->ROIName, ROI_Hide);
					}
					
					break;
			}
			
			break;
	}
	
Error:
	
PRINT_ERR

	return 0;
}

static int NonResRectRasterScan_SetSubregionFrameScan (RectRaster_type* rectRaster, size_t subregionIdx, char** errorMsg)
{
INIT_ERR

	//----------------------------------------------------
	// Convert chosen subregion ROI to scan settings
	//----------------------------------------------------
	
	Rect_type*	rectROI		= *(Rect_type**)ListGetPtrToItem(rectRaster->scanROIs, subregionIdx);
	
	// adjust scan engine scan settings
	rectRaster->scanSettings->height			= rectROI->height;
	rectRaster->scanSettings->heightOffset 		= (rectROI->height + 2 * rectROI->top - (int)rectRaster->parentFrameScanSettings->height)/2;
	rectRaster->scanSettings->width				= rectROI->width;
	rectRaster->scanSettings->widthOffset		= (rectROI->width + 2 * rectROI->left - (int)rectRaster->parentFrameScanSettings->width)/2;
	rectRaster->scanSettings->pixSize			= rectRaster->parentFrameScanSettings->pixSize;
	rectRaster->scanSettings->pixelDwellTime	= rectRaster->parentFrameScanSettings->pixelDwellTime;
	
	//----------------------------------------------------
	// Update UI scan settings
	//----------------------------------------------------
	
	// height
	SetCtrlVal(rectRaster->baseClass.frameScanPanHndl, ScanTab_Height, rectRaster->scanSettings->height * rectRaster->scanSettings->pixSize);
	// width
	char widthString[NonResGalvoRasterScan_Max_ComboboxEntryLength+1];
	Fmt(widthString, "%s<%f[p1]", rectRaster->scanSettings->width * rectRaster->scanSettings->pixSize);
	SetCtrlVal(rectRaster->baseClass.frameScanPanHndl, ScanTab_Width, widthString); 
	// update height and width offsets
	SetCtrlVal(rectRaster->baseClass.frameScanPanHndl, ScanTab_HeightOffset, rectRaster->scanSettings->heightOffset * rectRaster->scanSettings->pixSize); 
	SetCtrlVal(rectRaster->baseClass.frameScanPanHndl, ScanTab_WidthOffset, rectRaster->scanSettings->widthOffset * rectRaster->scanSettings->pixSize);
	// update pixel size
	SetCtrlVal(rectRaster->baseClass.frameScanPanHndl, ScanTab_PixelSize, rectRaster->scanSettings->pixSize);
	// update pixel dwell times
	errChk( NonResRectRasterScan_PixelDwellTimes(rectRaster, &errorInfo.errMsg) );
	
Error:
	
RETURN_ERR
}

static int CVICALLBACK NonResRectRasterScan_PointScanPan_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	RectRaster_type*			scanEngine					= callbackData;
	PointScanProtocol_type*		currentPointScanProtocol	= scanEngine->pointScan.pointScanProtocol;
	int							itemIdx						= 0;
	int							nROIs						= 0;
	double						stimTime					= 0;  
	
	switch (event) {
		
		case EVENT_COMMIT:
			
			switch (control) {
					
				case PointTab_Mode:
					
					PointJumpMethods	oldJumpMethod = currentPointScanProtocol->jumpMethod;
					
					GetCtrlIndex(panel, control, &itemIdx);
					currentPointScanProtocol->jumpMethod = (PointJumpMethods) itemIdx;
					
					// do not allow recording for point group
					switch (currentPointScanProtocol->jumpMethod) {
							
						case PointJump_SinglePoints:
							
							break;
							
						case PointJump_PointGroup:
						case PointJump_IncrementalPointGroup:
							
							currentPointScanProtocol->record = FALSE;
							SetCtrlVal(panel, PointTab_Record, FALSE);
							SetRectRasterScanEnginePointScanRecordUI(panel, currentPointScanProtocol->record);
							break;
					}
					
					SetRectRasterScanEnginePointScanJumpModeUI(panel, currentPointScanProtocol->jumpMethod);
					NonResRectRasterScan_SetMinimumPointJumpStartDelay(scanEngine);
					
					// if value changed, then switch to unnamed protocol
					if (currentPointScanProtocol->jumpMethod != oldJumpMethod) {
						SetCtrlIndex(panel, PointTab_Protocol, 0);
						SetCtrlAttribute(panel, PointTab_DelProtocol, ATTR_DIMMED, 1);
						OKfree(scanEngine->pointScan.pointScanProtocol->protocolName);
						scanEngine->pointScan.pointScanProtocol->protocolName = StrDup("");
					}
					
					break;
				
				case PointTab_ROIs:
					
					break;
					
				case PointTab_Record:
					
					BOOL	oldRecord = currentPointScanProtocol->record;
					
					GetCtrlVal(panel, control, &currentPointScanProtocol->record);
					SetRectRasterScanEnginePointScanRecordUI(panel, currentPointScanProtocol->record);
					
					// if value changed, then switch to unnamed protocol
					if (currentPointScanProtocol->record != oldRecord) {
						SetCtrlIndex(panel, PointTab_Protocol, 0);
						SetCtrlAttribute(panel, PointTab_DelProtocol, ATTR_DIMMED, 1);
						OKfree(scanEngine->pointScan.pointScanProtocol->protocolName);
						scanEngine->pointScan.pointScanProtocol->protocolName = StrDup("");
					}
					
					break;
					
				case PointTab_NIntegration:
					
					uInt32	oldNIntegration = currentPointScanProtocol->nIntegration;	
					
					GetCtrlVal(panel, control, &currentPointScanProtocol->nIntegration);
					
					// if value changed, then switch to unnamed protocol
					if (currentPointScanProtocol->nIntegration != oldNIntegration) {
						SetCtrlIndex(panel, PointTab_Protocol, 0);
						SetCtrlAttribute(panel, PointTab_DelProtocol, ATTR_DIMMED, 1);
						OKfree(scanEngine->pointScan.pointScanProtocol->protocolName);
						scanEngine->pointScan.pointScanProtocol->protocolName = StrDup("");
					}
					
					break;
					
				case PointTab_Stimulate:
					
					BOOL oldStimulate = currentPointScanProtocol->stimulate;
					
					// UI
					GetCtrlVal(panel, control, &currentPointScanProtocol->stimulate);
					SetRectRasterScanEnginePointScanStimulateUI(panel, currentPointScanProtocol->stimulate);
					
					// VChan
					if (currentPointScanProtocol->stimulate)
						SetVChanActive((VChan_type*)scanEngine->baseClass.VChanROIShutter, TRUE);
					else
						SetVChanActive((VChan_type*)scanEngine->baseClass.VChanROIShutter, FALSE);
					
					// if value changed, then switch to unnamed protocol
					if (currentPointScanProtocol->stimulate != oldStimulate) {
						SetCtrlIndex(panel, PointTab_Protocol, 0);
						SetCtrlAttribute(panel, PointTab_DelProtocol, ATTR_DIMMED, 1);
						OKfree(scanEngine->pointScan.pointScanProtocol->protocolName);
						scanEngine->pointScan.pointScanProtocol->protocolName = StrDup("");
					}
					
					break;
			
				case PointTab_NHoldBurst:
					
					uInt32	oldNHoldBurst = currentPointScanProtocol->nHoldBurst;
					
					GetCtrlVal(panel, control, &currentPointScanProtocol->nHoldBurst); 
					
					// if value changed, then switch to unnamed protocol
					if (currentPointScanProtocol->nHoldBurst != oldNHoldBurst) {
						SetCtrlIndex(panel, PointTab_Protocol, 0);
						SetCtrlAttribute(panel, PointTab_DelProtocol, ATTR_DIMMED, 1);
						OKfree(scanEngine->pointScan.pointScanProtocol->protocolName);
						scanEngine->pointScan.pointScanProtocol->protocolName = StrDup("");
					}
					
					break;
					
				case PointTab_Hold:
					
					double	holdTime	= 0;
					
					GetCtrlVal(panel, control, &holdTime);
					
					// round up to a multiple of galvo sampling
					holdTime = NonResRectRasterScan_RoundToGalvoSampling(scanEngine, holdTime);
					
					// if hold time is shorter than stimulation delay plus stimulation, then make delay 0 and use only one pulse
					stimTime = currentPointScanProtocol->stimDelay + currentPointScanProtocol->nStimPulses * 
							   currentPointScanProtocol->stimPulseONDuration + (currentPointScanProtocol->nStimPulses - 1) *
							   currentPointScanProtocol->stimPulseOFFDuration;
					
					if (stimTime > holdTime) {
						// adjust stimulation to fit within holding time
						currentPointScanProtocol->stimDelay = 0;
						SetCtrlVal(panel, PointTab_StimDelay, currentPointScanProtocol->stimDelay);
						currentPointScanProtocol->nStimPulses = 1;
						SetCtrlVal(panel, PointTab_NPulses, currentPointScanProtocol->nStimPulses);
						currentPointScanProtocol->stimPulseONDuration = holdTime;
						SetCtrlVal(panel, PointTab_PulseON, currentPointScanProtocol->stimPulseONDuration);
						SetCtrlAttribute(panel, PointTab_PulseOFF, ATTR_DIMMED, TRUE);
					}
							   
					// update in scan engine and UI
					
					double oldHoldTime = currentPointScanProtocol->holdTime;
					
					currentPointScanProtocol->holdTime = holdTime;
					SetCtrlVal(panel, control, currentPointScanProtocol->holdTime);
					// update minimum point jump period
					//NonResRectRasterScan_SetMinimumPointJumpPeriod(scanEngine);
					
					// if value changed, then switch to unnamed protocol
					if (currentPointScanProtocol->holdTime != oldHoldTime) {
						SetCtrlIndex(panel, PointTab_Protocol, 0);
						SetCtrlAttribute(panel, PointTab_DelProtocol, ATTR_DIMMED, 1);
						OKfree(scanEngine->pointScan.pointScanProtocol->protocolName);
						scanEngine->pointScan.pointScanProtocol->protocolName = StrDup("");
					}
					
					break;
					
				case PointTab_HoldBurstPeriod:
					
					double	holdBurstPeriod = 0;
					
					GetCtrlVal(panel, control, &holdBurstPeriod);
					
					// round up to a multiple of galvo sampling time
					holdBurstPeriod = NonResRectRasterScan_RoundToGalvoSampling(scanEngine, holdBurstPeriod);
					
					// make sure here that value is not smaller than the time needed to jump from ROI to parked position and then back again
					
					
					//---------------
					double oldHoldBurstPeriod = currentPointScanProtocol->holdBurstPeriod;
					
					currentPointScanProtocol->holdBurstPeriod = holdBurstPeriod;
					SetCtrlVal(panel, control, currentPointScanProtocol->holdBurstPeriod);
					
					// if value changed, then switch to unnamed protocol
					if (currentPointScanProtocol->holdBurstPeriod != oldHoldBurstPeriod) {
						SetCtrlIndex(panel, PointTab_Protocol, 0);
						SetCtrlAttribute(panel, PointTab_DelProtocol, ATTR_DIMMED, 1);
						OKfree(scanEngine->pointScan.pointScanProtocol->protocolName);
						scanEngine->pointScan.pointScanProtocol->protocolName = StrDup("");
					}
					
					break;
					
				case PointTab_HoldBurstPeriodIncr:
					
					double oldHoldBurstPeriodIncr = currentPointScanProtocol->holdBurstPeriodIncr;
					
					GetCtrlVal(panel, control, &currentPointScanProtocol->holdBurstPeriodIncr);
					
					// if value changed, then switch to unnamed protocol
					if (currentPointScanProtocol->holdBurstPeriodIncr != oldHoldBurstPeriodIncr) {
						SetCtrlIndex(panel, PointTab_Protocol, 0);
						SetCtrlAttribute(panel, PointTab_DelProtocol, ATTR_DIMMED, 1);
						OKfree(scanEngine->pointScan.pointScanProtocol->protocolName);
						scanEngine->pointScan.pointScanProtocol->protocolName = StrDup("");
					}
					
					break;
					
				case PointTab_StimDelay:
					
					double stimDelay 	= 0;
					
					double oldStimDelay = currentPointScanProtocol->stimDelay;
					
					GetCtrlVal(panel, control, &stimDelay);
					
					// round up to a multiple of galvo sampling
					stimDelay = NonResRectRasterScan_RoundToGalvoSampling(scanEngine, stimDelay);
						
					// calculate resulting stimulation time and check if it fits within the holding time
					stimTime = stimDelay + currentPointScanProtocol->nStimPulses * currentPointScanProtocol->stimPulseONDuration + 
							   (currentPointScanProtocol->nStimPulses - 1) * currentPointScanProtocol->stimPulseOFFDuration;
					
					// update stimulation delay if stimulation time does not exceed holding time
					if (stimTime <= currentPointScanProtocol->holdTime)
						currentPointScanProtocol->stimDelay = stimDelay;
						
					SetCtrlVal(panel, control, currentPointScanProtocol->stimDelay);
					
					// if value changed, then switch to unnamed protocol
					if (currentPointScanProtocol->stimDelay != oldStimDelay) {
						SetCtrlIndex(panel, PointTab_Protocol, 0);
						SetCtrlAttribute(panel, PointTab_DelProtocol, ATTR_DIMMED, 1);
						OKfree(scanEngine->pointScan.pointScanProtocol->protocolName);
						scanEngine->pointScan.pointScanProtocol->protocolName = StrDup("");
					}
					
					break;
					
				case PointTab_NPulses:
					
					uInt32	nPulses 	= 0;
					uInt32 	oldNPulses 	= currentPointScanProtocol->nStimPulses;
					
					GetCtrlVal(panel, control, &nPulses);
					
					// calculate resulting stimulation time and check if it fits within the holding time
					stimTime = currentPointScanProtocol->stimDelay + nPulses * currentPointScanProtocol->stimPulseONDuration + 
							   (nPulses - 1) * currentPointScanProtocol->stimPulseOFFDuration;
					
					// update nPulses if stimulation time does not exceed holding time
					if (stimTime <= currentPointScanProtocol->holdTime)
						currentPointScanProtocol->nStimPulses = nPulses;
						
					SetCtrlVal(panel, control, currentPointScanProtocol->nStimPulses);
					
					// if value changed, then switch to unnamed protocol
					if (currentPointScanProtocol->nStimPulses != oldNPulses) {
						SetCtrlIndex(panel, PointTab_Protocol, 0);
						SetCtrlAttribute(panel, PointTab_DelProtocol, ATTR_DIMMED, 1);
						OKfree(scanEngine->pointScan.pointScanProtocol->protocolName);
						scanEngine->pointScan.pointScanProtocol->protocolName = StrDup("");
					}
					
					break;
					
				case PointTab_PulseON:
					
					double	pulseONDuration 	= 0;
					double 	oldPulseOnDuration 	= currentPointScanProtocol->stimPulseONDuration;
					
					GetCtrlVal(panel, control, &pulseONDuration);
					
					pulseONDuration = NonResRectRasterScan_RoundToGalvoSampling(scanEngine, pulseONDuration);
					
					// calculate resulting stimulation time and check if it fits within the holding time
					stimTime = currentPointScanProtocol->stimDelay + currentPointScanProtocol->nStimPulses * 
							    pulseONDuration + (currentPointScanProtocol->nStimPulses - 1) * currentPointScanProtocol->stimPulseOFFDuration;
					
					// update pulseON duration if stimulation time does not exceed holding time
					if (stimTime <= currentPointScanProtocol->holdTime)
						currentPointScanProtocol->stimPulseONDuration = pulseONDuration;
						
					SetCtrlVal(panel, control, currentPointScanProtocol->stimPulseONDuration); 
					
					// if value changed, then switch to unnamed protocol
					if (currentPointScanProtocol->stimPulseONDuration != oldPulseOnDuration) {
						SetCtrlIndex(panel, PointTab_Protocol, 0);
						SetCtrlAttribute(panel, PointTab_DelProtocol, ATTR_DIMMED, 1);
						OKfree(scanEngine->pointScan.pointScanProtocol->protocolName);
						scanEngine->pointScan.pointScanProtocol->protocolName = StrDup("");
					}
					
					break;
					
				case PointTab_PulseOFF:
					
					double	pulseOFFDuration 		= 0;
					double 	oldPulseOffDuration 	= currentPointScanProtocol->stimPulseOFFDuration;
					
					GetCtrlVal(panel, control, &pulseOFFDuration);
					
					pulseOFFDuration = NonResRectRasterScan_RoundToGalvoSampling(scanEngine, pulseOFFDuration);
					
					// calculate resulting stimulation time and check if it fits within the holding time
					stimTime = currentPointScanProtocol->stimDelay + currentPointScanProtocol->nStimPulses * 
							    currentPointScanProtocol->stimPulseONDuration + (currentPointScanProtocol->nStimPulses - 1) * pulseOFFDuration;
					
					// update pulseON duration if stimulation time does not exceed holding time
					if (stimTime <= currentPointScanProtocol->holdTime)
						currentPointScanProtocol->stimPulseOFFDuration = pulseOFFDuration;
						
					SetCtrlVal(panel, control, currentPointScanProtocol->stimPulseOFFDuration); 
					
					// if value changed, then switch to unnamed protocol
					if (currentPointScanProtocol->stimPulseOFFDuration != oldPulseOffDuration) {
						SetCtrlIndex(panel, PointTab_Protocol, 0);
						SetCtrlAttribute(panel, PointTab_DelProtocol, ATTR_DIMMED, 1);
						OKfree(scanEngine->pointScan.pointScanProtocol->protocolName);
						scanEngine->pointScan.pointScanProtocol->protocolName = StrDup("");
					}
					
					break;
					
				case PointTab_Repeat:
					
					uInt32 oldNSequenceRepeat = currentPointScanProtocol->nSequenceRepeat;
					
					GetCtrlVal(panel, control, &currentPointScanProtocol->nSequenceRepeat);
					
					// if value changed, then switch to unnamed protocol
					if (currentPointScanProtocol->nSequenceRepeat != oldNSequenceRepeat) {
						SetCtrlIndex(panel, PointTab_Protocol, 0);
						SetCtrlAttribute(panel, PointTab_DelProtocol, ATTR_DIMMED, 1);
						OKfree(scanEngine->pointScan.pointScanProtocol->protocolName);
						scanEngine->pointScan.pointScanProtocol->protocolName = StrDup("");
					}
					
					break;
					
				case PointTab_RepeatWait:
					
					double oldRepeatWait = currentPointScanProtocol->repeatWait;
					
					GetCtrlVal(panel, control, &currentPointScanProtocol->repeatWait);
					
					// if value changed, then switch to unnamed protocol
					if (currentPointScanProtocol->repeatWait != oldRepeatWait) {
						SetCtrlIndex(panel, PointTab_Protocol, 0);
						SetCtrlAttribute(panel, PointTab_DelProtocol, ATTR_DIMMED, 1);
						OKfree(scanEngine->pointScan.pointScanProtocol->protocolName);
						scanEngine->pointScan.pointScanProtocol->protocolName = StrDup("");
					}
					
					break;
					
				case PointTab_StartDelay:
					
					double oldStartDelayInitVal = currentPointScanProtocol->startDelayInitVal;
					
					GetCtrlVal(panel, control, &currentPointScanProtocol->startDelayInitVal);
					
					// round up to a multiple of galvo sampling
					currentPointScanProtocol->startDelayInitVal = NonResRectRasterScan_RoundToGalvoSampling(scanEngine, currentPointScanProtocol->startDelayInitVal);
					
					// make sure here that value is not smaller than the time needed to jump from parked position to the point ROI
					NonResRectRasterScan_SetMinimumPointJumpStartDelay(scanEngine);
					
					SetCtrlVal(panel, control, currentPointScanProtocol->startDelayInitVal);
					
					// if value changed, then switch to unnamed protocol
					if (currentPointScanProtocol->startDelayInitVal != oldStartDelayInitVal) {
						SetCtrlIndex(panel, PointTab_Protocol, 0);
						SetCtrlAttribute(panel, PointTab_DelProtocol, ATTR_DIMMED, 1);
						OKfree(scanEngine->pointScan.pointScanProtocol->protocolName);
						scanEngine->pointScan.pointScanProtocol->protocolName = StrDup("");
					}
					
					break;
				
				case PointTab_StartDelayIncrement:
					
					double oldStartDelayIncrement = currentPointScanProtocol->startDelayIncrement;
					
					GetCtrlVal(panel, control, &currentPointScanProtocol->startDelayIncrement);
					
					// round up to a multiple of galvo sampling
					currentPointScanProtocol->startDelayIncrement = NonResRectRasterScan_RoundToGalvoSampling(scanEngine, currentPointScanProtocol->startDelayIncrement);
					SetCtrlVal(panel, control, currentPointScanProtocol->startDelayIncrement);
					
					// if value changed, then switch to unnamed protocol
					if (currentPointScanProtocol->startDelayIncrement != oldStartDelayIncrement) {
						SetCtrlIndex(panel, PointTab_Protocol, 0);
						SetCtrlAttribute(panel, PointTab_DelProtocol, ATTR_DIMMED, 1);
						OKfree(scanEngine->pointScan.pointScanProtocol->protocolName);
						scanEngine->pointScan.pointScanProtocol->protocolName = StrDup("");
					}
					
					break;
					
				case PointTab_AddProtocol:
					
					PointScanProtocol_type*		pointScanProtocolCopy 		= NULL;
					char* 						newPointScanProtocolName 	= DLGetUINameInput("New point scan protocol name", 50, VerifyPointScanProtocolName, &scanEngine->pointScan);
					
					if (!newPointScanProtocolName) return 0;
					
					// copy protocol
					pointScanProtocolCopy = copy_PointScanProtocol_type(currentPointScanProtocol);
					// rename protocol
					OKfree(pointScanProtocolCopy->protocolName);
					pointScanProtocolCopy->protocolName = newPointScanProtocolName;
					newPointScanProtocolName = NULL;
					
					// add protocol to UI list
					InsertListItem(panel, PointTab_Protocol, -1, pointScanProtocolCopy->protocolName, ListNumItems(scanEngine->pointScan.pointScanProtocols));
					SetCtrlAttribute(panel, PointTab_Protocol, ATTR_DIMMED, 0);
					SetCtrlAttribute(panel, PointTab_DelProtocol, ATTR_DIMMED, 0);
					SetCtrlAttribute(panel, PointTab_SaveProtocol, ATTR_DIMMED, 0);
					int nListPointScanProtocols = 0;
					GetNumListItems(panel, PointTab_Protocol, &nListPointScanProtocols);
					SetCtrlIndex(panel, PointTab_Protocol, nListPointScanProtocols - 1);
					
					// add protocol to data structure list
					ListInsertItem(scanEngine->pointScan.pointScanProtocols, &pointScanProtocolCopy, END_OF_LIST);
					pointScanProtocolCopy = NULL;
					
					break;
				
				case PointTab_DelProtocol:
					
					{
						int							protocolIdx 		= 0;
						PointScanProtocol_type*		pointScanProtocol 	= NULL;
					
						GetCtrlIndex(panel, PointTab_Protocol, &protocolIdx);
						// do not delete unnamed protocol
						if (!protocolIdx)
							break;
						
						DeleteListItem(panel, PointTab_Protocol, protocolIdx, 1);
						// switch to unnamed protocol (settings kept from the deleted protocol)
						SetCtrlIndex(panel, PointTab_Protocol, 0);
						SetCtrlAttribute(panel, PointTab_DelProtocol, ATTR_DIMMED, 1);
						
						ListRemoveItem(scanEngine->pointScan.pointScanProtocols, &pointScanProtocol, protocolIdx);
						discard_PointScanProtocol_type(&pointScanProtocol);
					
						// dim protocols UI control if there are no more point scan protocols available
						if (!ListNumItems(scanEngine->pointScan.pointScanProtocols)) {
							SetCtrlAttribute(panel, PointTab_Protocol, ATTR_DIMMED, 1);
							SetCtrlAttribute(panel, PointTab_SaveProtocol, ATTR_DIMMED, 1);
							OKfree(scanEngine->pointScan.pointScanProtocol->protocolName);
							scanEngine->pointScan.pointScanProtocol->protocolName = StrDup("");
						}
					}
					
					break;
					
				case PointTab_Protocol:
					
					{
						int							protocolIdx 		= 0;
						PointScanProtocol_type*		pointScanProtocol 	= NULL;
						
						GetCtrlIndex(panel, PointTab_Protocol, &protocolIdx);
						// if the unnamed protocol is selected, do nothing
						if (protocolIdx < 1) {
							SetCtrlAttribute(panel, PointTab_DelProtocol, ATTR_DIMMED, 1);
							OKfree(scanEngine->pointScan.pointScanProtocol->protocolName);
							scanEngine->pointScan.pointScanProtocol->protocolName = StrDup("");
							return 0;
						}
						
						SetCtrlAttribute(panel, PointTab_DelProtocol, ATTR_DIMMED, 0);
						
						pointScanProtocol = *(PointScanProtocol_type**)ListGetPtrToItem(scanEngine->pointScan.pointScanProtocols, protocolIdx);
						SetRectRasterPointScanProtocol(scanEngine, pointScanProtocol);
						currentPointScanProtocol = scanEngine->pointScan.pointScanProtocol;
						
						// apply protocol settings to the scan engine
						SetPointScanProtocolUI(scanEngine, FALSE);
					}
					
					break;
					
				case PointTab_SaveProtocol:
					
					{
						// create new xml DOM
						CAObjHandle						pointScanProtocolsXMLDOM			= 0;
						ActiveXMLObj_IXMLDOMElement_	pointScanProtocolsXMLRootElement	= 0;
						ERRORINFO 						xmlErrorInfo;
						
						
						ActiveXML_NewDOMDocument60IXMLDOMDocument3_ (NULL, 1, LOCALE_NEUTRAL, 0, &pointScanProtocolsXMLDOM);
						SaveCfg_RectRaster_PointScanProtocols(scanEngine, pointScanProtocolsXMLDOM, &pointScanProtocolsXMLRootElement, &xmlErrorInfo);
						DLSaveToXMLPopup(pointScanProtocolsXMLDOM, pointScanProtocolsXMLRootElement, &xmlErrorInfo);
						OKfreeCAHndl(pointScanProtocolsXMLRootElement);
						OKfreeCAHndl(pointScanProtocolsXMLDOM);
						
					}
					
					break;
					
				case PointTab_LoadProtocol:
					
					{
						CAObjHandle						pointScanProtocolsXMLDOM			= 0;
						ActiveXMLObj_IXMLDOMElement_	pointScanProtocolsXMLRootElement	= 0;
						ListType						loadedPointScanProtocols			= 0;	// list of PointScanProtocol_type* elements.
						PointScanProtocol_type*			selectedPointScanProtocol			= NULL;
						ERRORINFO 						xmlErrorInfo;
						
						DLLoadFromXMLPopup(&pointScanProtocolsXMLDOM, &pointScanProtocolsXMLRootElement, "PointScanProtocols", &xmlErrorInfo);
						
						if (pointScanProtocolsXMLDOM && pointScanProtocolsXMLRootElement) {
							
							// convert XML to point scan protocols
							LoadCfg_RectRaster_PointScanProtocols(&loadedPointScanProtocols, &selectedPointScanProtocol, pointScanProtocolsXMLRootElement, &xmlErrorInfo);
							
							// assign loaded protocols to the scan engine
							if (loadedPointScanProtocols) {
								OKfreeList(&scanEngine->pointScan.pointScanProtocols, discard_PointScanProtocol_type);
								scanEngine->pointScan.pointScanProtocols = loadedPointScanProtocols;
								loadedPointScanProtocols = 0;
							}
							
							// select protocol if there is any
							if (selectedPointScanProtocol)
								SetRectRasterPointScanProtocol(scanEngine, selectedPointScanProtocol); 
							else {
								OKfree(scanEngine->pointScan.pointScanProtocol->protocolName);
								scanEngine->pointScan.pointScanProtocol->protocolName = StrDup("");
							}
								
							// update scan engine UI
							SetPointScanProtocolUI(scanEngine, TRUE);
						}
						
						OKfreeCAHndl(pointScanProtocolsXMLRootElement);
						OKfreeCAHndl(pointScanProtocolsXMLDOM);
					}
					
					break;
					
			}
			
			break;
			
		case EVENT_KEYPRESS:
			
			switch (control) {
					
				case PointTab_ROIs:
					
					// continue only if Del key is pressed
					if (eventData1 != VAL_FWD_DELETE_VKEY) break;
					
					GetCtrlIndex(panel, control, &itemIdx);
					if (itemIdx < 0) break; // stop here if list is empty
					
					// get ROI name from UI list
					ROI_type*	ROIItem = *(ROI_type**) ListGetPtrToItem(scanEngine->pointScan.pointJumps, itemIdx+1);
					
					// if there is an active display, remove ROI item from display and from scan engine list
					if (scanEngine->baseClass.activeDisplay)
						(*scanEngine->baseClass.activeDisplay->ROIActionsFptr) (scanEngine->baseClass.activeDisplay, ROIItem->ROIName, ROI_Delete);
					
					DeleteListItem(scanEngine->baseClass.pointScanPanHndl, PointTab_ROIs, itemIdx, 1);
					ListRemoveItem(scanEngine->pointScan.pointJumps, 0, itemIdx + 1);
					
					// calculate minimum point jump start delay
					NonResRectRasterScan_SetMinimumPointJumpStartDelay(scanEngine);
					// calculate point jump period
					//NonResRectRasterScan_SetMinimumPointJumpPeriod(scanEngine);
					
					// dim panel if there are no more active ROIs
					GetNumCheckedItems(panel, control, &nROIs);
					if (!nROIs)
						SetPanelAttribute(panel, ATTR_DIMMED, TRUE); 
					
					break;
			}
			 
			break;
			
		case EVENT_MARK_STATE_CHANGE:
			
			
			switch (control) {
					
				case PointTab_ROIs:
					
					if (eventData1) {
						// ROI marked as active
						ROI_type*	ROI = *(ROI_type**) ListGetPtrToItem(scanEngine->pointScan.pointJumps, eventData2 + 1);
						ROI->active = TRUE;
						// if there is an active display, mark point ROI as active and make it visible
						if (scanEngine->baseClass.activeDisplay)
							(*scanEngine->baseClass.activeDisplay->ROIActionsFptr) (scanEngine->baseClass.activeDisplay, ROI->ROIName, ROI_Show);
					} else {
						// ROI marked as inactive
						ROI_type*	ROI = *(ROI_type**) ListGetPtrToItem(scanEngine->pointScan.pointJumps, eventData2 + 1);
						ROI->active = FALSE;
						// if there is an active display mark point ROI as inactive and hide it
						if (scanEngine->baseClass.activeDisplay)
							(*scanEngine->baseClass.activeDisplay->ROIActionsFptr) (scanEngine->baseClass.activeDisplay, ROI->ROIName, ROI_Hide);
					}
					
					// calculate minimum point jump start delay
					NonResRectRasterScan_SetMinimumPointJumpStartDelay(scanEngine);
					// calculate point jump period
					//NonResRectRasterScan_SetMinimumPointJumpPeriod(scanEngine);
					
					break;
			}
			
			break;
	}
	
	return 0;
}

static int NonResRectRasterScan_ScanWidths (RectRaster_type* scanEngine, char** errorMsg)
{  
INIT_ERR

	size_t   				blank					= 0;
	size_t   				deadTimePixels			= 0;
	BOOL     				inFOVFlag             	= 1; 
	size_t   				i                     	= 1;	// counts how many galvo samples of 1/galvoSampleRate duration fit in a line scan
	double   				pixelsPerLine			= 0;
	double   				rem						= 0;;
	char     				dwellTimeString[NonResGalvoRasterScan_Max_ComboboxEntryLength+1]	= "";
	char					widthString[NonResGalvoRasterScan_Max_ComboboxEntryLength+1]		= "";
	ListType 				Widths               	= 0;
	double					ROIWidth				= 0;
	NonResGalvoCal_type*	fastAxisCal				= (NonResGalvoCal_type*) scanEngine->baseClass.fastAxisCal;     
	NonResGalvoCal_type*	slowAxisCal				= (NonResGalvoCal_type*) scanEngine->baseClass.slowAxisCal;

	nullChk( Widths = ListCreate(sizeof(double)) );
	
	// enforce pixeldwelltime to be an integer multiple of 1/referenceClockFreq
	// make sure pixeldwelltime is in [us] and referenceClockFreq in [Hz] 
	scanEngine->scanSettings->pixelDwellTime = floor(scanEngine->scanSettings->pixelDwellTime * scanEngine->baseClass.referenceClockFreq) / scanEngine->baseClass.referenceClockFreq; // result in [us]
	Fmt(dwellTimeString,"%s<%f[p3]", scanEngine->scanSettings->pixelDwellTime);
	SetCtrlVal(scanEngine->baseClass.frameScanPanHndl, ScanTab_PixelDwell, dwellTimeString);	// in [us]
	
	// empty list of preferred widths
	errChk( Combo_DeleteComboItem(scanEngine->baseClass.frameScanPanHndl, ScanTab_Width, 0, -1) );
	
	// empty value for Width, if the calculated list contains at least one item, then it will be filled out
	SetCtrlVal(scanEngine->baseClass.frameScanPanHndl, ScanTab_Width, ""); 
	
	//---Calculation of prefered widths---
	//deadtime in [ms], scanEngine->pixelDwellTime in [us]
	blank = (size_t)ceil(fastAxisCal->triangleCal->deadTime * 1e+3 /scanEngine->scanSettings->pixelDwellTime);  
	// the sum of unused pixels at the beginning and end of each line
	deadTimePixels = 2 * blank; 
	
	while (inFOVFlag == TRUE){
		// this condition ensures that at the end of each line, the pixels remain in sync with the galvo samples
		// this allows for freedom in choosing the pixel dwell time (and implicitly the scan speed)
		pixelsPerLine = floor(i/(scanEngine->galvoSamplingRate * scanEngine->scanSettings->pixelDwellTime * 1e-6));
		if (pixelsPerLine < deadTimePixels) {
			i++;
			continue;
		}
		rem = i/(scanEngine->galvoSamplingRate * scanEngine->scanSettings->pixelDwellTime * 1e-6) - pixelsPerLine;
		
		ROIWidth = (pixelsPerLine - deadTimePixels) * scanEngine->scanSettings->pixSize;
		if (rem == 0.0){ 
			inFOVFlag = RectROIInsideRect(scanEngine->scanSettings->height * scanEngine->scanSettings->pixSize, ROIWidth, scanEngine->scanSettings->heightOffset * scanEngine->scanSettings->pixSize, 
										  scanEngine->scanSettings->widthOffset * scanEngine->scanSettings->pixSize, 2 * slowAxisCal->commandVMax * slowAxisCal->sampleDisplacement, 
										  2 * fastAxisCal->commandVMax * fastAxisCal->sampleDisplacement);
			
			if (CheckNonResGalvoScanFreq(fastAxisCal, scanEngine->scanSettings->pixSize, scanEngine->scanSettings->pixelDwellTime, ROIWidth + deadTimePixels * scanEngine->scanSettings->pixSize) && inFOVFlag) {
				// add to list if scan frequency is valid
				Fmt(widthString,"%s<%f[p1]", ROIWidth);
				errChk( Combo_InsertComboItem(scanEngine->baseClass.frameScanPanHndl, ScanTab_Width, -1, widthString) );
				ListInsertItem(Widths, &ROIWidth, END_OF_LIST);
			}
			 
		}
		if (ROIWidth > 2 * fastAxisCal->commandVMax * fastAxisCal->sampleDisplacement) inFOVFlag = FALSE;
		i++;
	}
	
	// select value from the list that is closest to the old width value
	size_t 		nElem 				= ListNumItems(Widths);
	double   	diffOld				= 0;
	double   	diffNew				= 0;
	double   	widthItem			= 0;	// in [um]
	double		targetROIWidth 		= scanEngine->scanSettings->width * scanEngine->scanSettings->pixSize;
	size_t     	itemPos				= 0;
	if (nElem) {
		ListGetItem(Widths, &widthItem, 1);
		diffOld = fabs(widthItem - targetROIWidth);
		itemPos = 1;
		for (size_t j = 2; j <= nElem; j++) {
			ListGetItem(Widths, &widthItem, j);
			diffNew = fabs(widthItem - targetROIWidth);
			if (diffNew < diffOld) {
				diffOld = diffNew; 
				itemPos = j;
			}
		}
		ListGetItem(Widths, &widthItem, itemPos);
		// update scan control UI
		Fmt(widthString,"%s<%f[p1]", widthItem);
		SetCtrlVal(scanEngine->baseClass.frameScanPanHndl, ScanTab_Width, widthString);
		// update scan engine structure
		scanEngine->scanSettings->width = (uInt32) (widthItem/scanEngine->scanSettings->pixSize);	// in [pix]
	} else
		// update scan engine structure
		scanEngine->scanSettings->width = 0;														// in [pix]
	
	ListDispose(Widths);
	
Error:

RETURN_ERR
}

static int NonResRectRasterScan_PixelDwellTimes (RectRaster_type* scanEngine, char** errorMsg)
{
INIT_ERR

	char					dwellTimeString[NonResGalvoRasterScan_Max_ComboboxEntryLength+1] = "";
	double    				pixelDwell                  = 0;
	double    				pixelDwellOld				= 0;
	uInt32    				k                           = 1;	// counts how many galvo samples of 1/galvo_sample_rate duration fit in a line scan
	uInt32    				n							= 0;	// counts how many pixel dwell times fit in the deadtime, note that the total dead time per line is twice this value
	double    				rem							= 0;
	ListType  				dwellTimes				  	= 0;
	NonResGalvoCal_type*	fastAxisCal					= (NonResGalvoCal_type*) scanEngine->baseClass.fastAxisCal;    
	
	
	nullChk( dwellTimes = ListCreate(sizeof(double)) );
	// empty list of preferred dwell times
	errChk( Combo_DeleteComboItem(scanEngine->baseClass.frameScanPanHndl, ScanTab_PixelDwell, 0, -1) );
	
	// empty value for Dwell Time, if the calculated list contains at least one item, then it will be filled out
	SetCtrlVal(scanEngine->baseClass.frameScanPanHndl, ScanTab_PixelDwell, "");
	
	// stop here if scan width is 0
	if (!scanEngine->scanSettings->width) {
		scanEngine->scanSettings->pixelDwellTime = 0;
		ListDispose(dwellTimes);
		return 0;
	}
	
	// make sure that pixelDwell is a multiple of 1/pix_clock_rate      
	pixelDwell 	= ceil (NonResGalvoScan_MinPixelDwellTime * (scanEngine->baseClass.referenceClockFreq/1e6)) * (1e6/scanEngine->baseClass.referenceClockFreq);
	n 		 	= (uInt32) ceil (fastAxisCal->triangleCal->deadTime * (1e3/pixelDwell));           				// dead time pixels at the beginning and end of each line
	k		 	= (uInt32) ceil (pixelDwell * (scanEngine->galvoSamplingRate/1e6) * (scanEngine->scanSettings->width + 2*n));
	while (pixelDwell <= NonResGalvoScan_MaxPixelDwellTime) {
		
		pixelDwellOld = 0;
		while (pixelDwellOld != pixelDwell) {
			pixelDwellOld = pixelDwell;
			n 		 = (uInt32) ceil (fastAxisCal->triangleCal->deadTime * (1e3/pixelDwell));   				// dead time pixels at the beginning and end of each line
			pixelDwell = k/(scanEngine->galvoSamplingRate * (scanEngine->scanSettings->width + 2*n)) * 1e6;      // in [us]
		}
	
		// check if the pixel dwell time is a multiple of 1/referenceClockFreq 
		rem = pixelDwell * (scanEngine->baseClass.referenceClockFreq/1e6) - floor(pixelDwell * (scanEngine->baseClass.referenceClockFreq/1e6));
		if (rem == 0.0)
			if (CheckNonResGalvoScanFreq(fastAxisCal, scanEngine->scanSettings->pixSize, pixelDwell, scanEngine->scanSettings->width * scanEngine->scanSettings->pixSize + 2 * n * scanEngine->scanSettings->pixSize)) {
				// add to list
				Fmt(dwellTimeString,"%s<%f[p3]", pixelDwell);
				errChk( Combo_InsertComboItem(scanEngine->baseClass.frameScanPanHndl, ScanTab_PixelDwell, -1, dwellTimeString) );
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
		diffOld = fabs(dwellTimeItem - scanEngine->scanSettings->pixelDwellTime);
		itemPos = 1;
		for (size_t i = 2; i <= nElem; i++) {
			ListGetItem(dwellTimes, &dwellTimeItem, i);
			diffNew = fabs(dwellTimeItem - scanEngine->scanSettings->pixelDwellTime);
			if (diffNew < diffOld) {diffOld = diffNew; itemPos = i;}
		}
		
		ListGetItem(dwellTimes, &dwellTimeItem, itemPos);
		// update scan control UI
		Fmt(dwellTimeString,"%s<%f[p3]", dwellTimeItem);
		SetCtrlVal(scanEngine->baseClass.frameScanPanHndl, ScanTab_PixelDwell, dwellTimeString);
		// update scan engine structure
		scanEngine->scanSettings->pixelDwellTime = dwellTimeItem;
	} else
		scanEngine->scanSettings->pixelDwellTime = 0;
	
	ListDispose(dwellTimes);
	
Error:
	
RETURN_ERR
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
		   				(scanEngine->baseClass.referenceClockFreq != 0.0) && (scanEngine->scanSettings->pixSize != 0.0) && scanEngine->baseClass.nScanChans;
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
	BOOL scanReady = NonResRectRasterScan_ValidConfig(scanEngine) && (scanEngine->scanSettings->width != 0.0) && (scanEngine->scanSettings->height != 0.0);
	
	SetCtrlVal(scanEngine->baseClass.frameScanPanHndl, ScanTab_Ready, scanReady);
	
	return scanReady;
}

/// HIFN Generates galvo scan signals for bidirectional raster scanning
static int NonResRectRasterScan_GenerateScanSignals (RectRaster_type* scanEngine, char** errorMsg)
{
#define NonResRectRasterScan_GenerateScanSignals_Err_ScanSignals		-1
	
INIT_ERR
	
	// init dynamically allocated signals
	double*						fastAxisCommandSignal						= NULL;
	double*						fastAxisCompensationSignal					= NULL;
	double*						slowAxisCompensationSignal					= NULL;
	unsigned char*				shutterScanSignal							= NULL;	// Laser beam modulation signal to blank fast axis line scan galvo turnaround and laser beam fly-in to the scan area.
	unsigned char*				shutterFlyInSignal							= NULL;	// Shutter closed waveform while the beam moves from the parked position to the scan area.
	double*						parkedVoltageSignal							= NULL;
	double*						galvoSamplingRatePtr						= NULL;
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
	RepeatedWaveform_type*		shutterScan_RepWaveform						= NULL;
	RepeatedWaveform_type*		shutterFlyIn_RepWaveform					= NULL;
	DataPacket_type*			dataPacket									= NULL; 
	PulseTrain_type*			pixelPulseTrain								= NULL;
	double*						pixelSamplingRatePtr						= NULL;
	size_t						nFrames										= GetTaskControlIterations(scanEngine->baseClass.taskControl);
	uInt32 						nDeadTimePixels								= 0;	// Number of pixels at the beginning and end of each line where the motion of the galvo is not linear.
	uInt32						nPixelsPerLine								= 0;	// Total number of pixels per line including dead time pixels for galvo turn-around.
	uInt32						nGalvoSamplesPerLine						= 0;	// Total number of analog samples for the galvo command signal per line.
	double						fastAxisCommandAmplitude					= 0;	// Fast axis signal amplitude in [V].
	double						lineDuration								= 0;	// Fast axis line duration in [us] (incl. dead-time pixels at the beginning and end of each line).
	
	double						slowAxisAmplitude							= 0;	// Slow axis signal amplitude in [V].
	double						slowAxisStartVoltage						= 0;	// Slow axis staircase start voltage in [V].
	double						slowAxisStepVoltage							= 0;	// Slow axis staircase step voltage in [V]. 
	
	double						flybackTime									= 0;	// Slow-axis fly back time in [ms] after completing a framescan.
	double						flybackSwitchTime							= 0;
	double						flybackSwitchDelay							= 0;	
	uInt32						nFastAxisFlybackLines						= 0;
	DSInfo_type*				dsInfo										= NULL;
	NonResGalvoCal_type*		fastAxisCal									= (NonResGalvoCal_type*)scanEngine->baseClass.fastAxisCal;
	NonResGalvoCal_type*		slowAxisCal									= (NonResGalvoCal_type*)scanEngine->baseClass.slowAxisCal;

	
//============================================================================================================================================================================================
//                          					   	Preparation of Scan Waveforms for X-axis Galvo (fast axis, triangular waveform scan)
//============================================================================================================================================================================================
	
	// number of line scan dead time pixels
	// note: deadTime in [ms] and pixelDwellTime in [us]
	nDeadTimePixels = (uInt32) ceil(fastAxisCal->triangleCal->deadTime * 1e3/scanEngine->scanSettings->pixelDwellTime);
	
	// number of pixels per line
	nPixelsPerLine = scanEngine->scanSettings->width + 2 * nDeadTimePixels;
	
	// line duration in [us]
	lineDuration = nPixelsPerLine * scanEngine->scanSettings->pixelDwellTime;
	
	// calculate number of fast axis lines to skip at the end of each image until the slow axis flies back to the start of the image
	slowAxisStepVoltage 	= scanEngine->scanSettings->pixSize / slowAxisCal->sampleDisplacement; 
	slowAxisAmplitude 		= (scanEngine->scanSettings->height - 1) * slowAxisStepVoltage;
	slowAxisStartVoltage 	= scanEngine->scanSettings->heightOffset * scanEngine->scanSettings->pixSize / slowAxisCal->sampleDisplacement - slowAxisAmplitude/2;
	
	NonResGalvoPointJumpTime((NonResGalvoCal_type*)scanEngine->baseClass.slowAxisCal, slowAxisAmplitude, &flybackSwitchTime, &flybackSwitchDelay);  
	flybackTime = flybackSwitchTime + flybackSwitchDelay;
	
	nFastAxisFlybackLines 	= (uInt32) ceil(flybackTime*1e3/lineDuration);
	// store this value in the image buffers
	for (size_t i = 0; i < scanEngine->nImgBuffers; i++)
		scanEngine->imgBuffers[i]->skipFlybackRows = nFastAxisFlybackLines;
	
	// number of galvo samples per line
	nGalvoSamplesPerLine = RoundRealToNearestInteger(lineDuration * 1e-6 * scanEngine->galvoSamplingRate);
	
	// generate bidirectional raster scan signal (2 line scans, 1 triangle waveform period)
	fastAxisCommandAmplitude = nPixelsPerLine * scanEngine->scanSettings->pixSize / fastAxisCal->sampleDisplacement;
	
	nullChk( fastAxisCommandSignal = malloc(2 * nGalvoSamplesPerLine * sizeof(double)) );
	double 		phase = -90;   
	errChk( TriangleWave(2 * nGalvoSamplesPerLine , fastAxisCommandAmplitude/2, 0.5/nGalvoSamplesPerLine , &phase, fastAxisCommandSignal) );
	double		fastAxisCommandOffset = scanEngine->scanSettings->widthOffset * scanEngine->scanSettings->pixSize / fastAxisCal->sampleDisplacement; 
	for (size_t i = 0; i < 2 * nGalvoSamplesPerLine; i++)
		fastAxisCommandSignal[i] += fastAxisCommandOffset;
	
	// generate bidirectional raster scan waveform 
	nullChk( fastAxisScan_Waveform = init_Waveform_type(Waveform_Double, scanEngine->galvoSamplingRate, 2 * nGalvoSamplesPerLine, (void**)&fastAxisCommandSignal) );
	
	// generate fast axis fly-in waveform from parked position
	nullChk( fastAxisMoveFromParkedWaveform = NonResGalvoMoveBetweenPoints((NonResGalvoCal_type*)scanEngine->baseClass.fastAxisCal, scanEngine->galvoSamplingRate, 
	fastAxisCal->parked, - fastAxisCommandAmplitude/2 + fastAxisCommandOffset, 0, 0) );
	
	size_t		nGalvoSamplesFastAxisMoveFromParkedWaveform = GetWaveformNumSamples(fastAxisMoveFromParkedWaveform);
	
//============================================================================================================================================================================================
//                             						Preparation of Scan Waveforms for Y-axis Galvo (slow axis, staircase waveform scan)
//============================================================================================================================================================================================
	
	// generate staircase signal
	nullChk( slowAxisScan_Waveform = StaircaseWaveform(scanEngine->galvoSamplingRate, nGalvoSamplesPerLine, scanEngine->scanSettings->height, nGalvoSamplesPerLine * nFastAxisFlybackLines, slowAxisStartVoltage, slowAxisStepVoltage) );  
	
	// generate slow axis fly-in waveform from parked position
	nullChk( slowAxisMoveFromParkedWaveform = NonResGalvoMoveBetweenPoints(slowAxisCal, scanEngine->galvoSamplingRate, slowAxisCal->parked, slowAxisStartVoltage, 0, 0) ); 
	
	size_t		nGalvoSamplesSlowAxisScanWaveform 			= GetWaveformNumSamples(slowAxisScan_Waveform);
	
//============================================================================================================================================================================================
//                             						Compensate delay between X and Y galvo response and fly-in from parked position
//============================================================================================================================================================================================
	
	// fast scan axis response lag in terms of number of galvo samples 
	size_t				nGalvoSamplesFastAxisLag			= (size_t) floor(fastAxisCal->lag * 1e-3 * scanEngine->galvoSamplingRate);
	
	// slow scan axis response lag in terms of number of galvo samples  
	size_t				nGalvoSamplesSlowAxisLag			= (size_t) floor(slowAxisCal->lag * 1e-3 * scanEngine->galvoSamplingRate);
	
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
	
	// calculate number of pixels to skip when assembling the images and set this for all image buffers
	for (size_t i = 0; i < scanEngine->nImgBuffers; i++) 
		scanEngine->imgBuffers[i]->nSkipPixels = (size_t)RoundRealToNearestInteger((scanEngine->flyInDelay - scanEngine->baseClass.pixDelay)/scanEngine->scanSettings->pixelDwellTime);
	
	// determine number of additional galvo samples needed to compensate fast and slow axis fly-in and lag delays
	size_t				nGalvoSamplesFastAxisCompensation	=  nGalvoSamplesFlyIn - nGalvoSamplesFastAxisLag - nGalvoSamplesFastAxisMoveFromParked;
	size_t				nGalvoSamplesSlowAxisCompensation	=  nGalvoSamplesFlyIn - nGalvoSamplesSlowAxisLag - nGalvoSamplesSlowAxisMoveFromParked; 
	
	
	// generate compensated fast axis fly-in waveform from parked position
	if (nGalvoSamplesFastAxisCompensation) {
		nullChk( fastAxisCompensationSignal = malloc(nGalvoSamplesFastAxisCompensation * sizeof(double)) );
		errChk( Set1D(fastAxisCompensationSignal, nGalvoSamplesFastAxisCompensation, fastAxisCal->parked) ); 
		nullChk( fastAxisMoveFromParkedCompensatedWaveform = init_Waveform_type(Waveform_Double, scanEngine->galvoSamplingRate, nGalvoSamplesFastAxisCompensation, (void**)&fastAxisCompensationSignal) );
		errChk( AppendWaveform(fastAxisMoveFromParkedCompensatedWaveform, fastAxisMoveFromParkedWaveform, &errorInfo.errMsg) );
		discard_Waveform_type(&fastAxisMoveFromParkedWaveform);
	}
	
	// generate compensated slow axis fly-in waveform from parked position
	if (nGalvoSamplesSlowAxisCompensation) {
		nullChk( slowAxisCompensationSignal = malloc(nGalvoSamplesSlowAxisCompensation * sizeof(double)) );
		errChk( Set1D(slowAxisCompensationSignal, nGalvoSamplesSlowAxisCompensation, slowAxisCal->parked) ); 
		nullChk( slowAxisMoveFromParkedCompensatedWaveform = init_Waveform_type(Waveform_Double, scanEngine->galvoSamplingRate, nGalvoSamplesSlowAxisCompensation, (void**)&slowAxisCompensationSignal) );
		errChk( AppendWaveform(slowAxisMoveFromParkedCompensatedWaveform, slowAxisMoveFromParkedWaveform, &errorInfo.errMsg) );
		discard_Waveform_type(&slowAxisMoveFromParkedWaveform);
	}
	
//============================================================================================================================================================================================
//		                             						Preparation of Shutter Waveforms for laser beam modulation
//============================================================================================================================================================================================
	
	// generate one cycle of open shutter signal, i.e two line scans since the scanning is bidirectional
	size_t	nShutterCycleSamples = 2*nGalvoSamplesPerLine;
	nullChk( shutterScanSignal = malloc(nShutterCycleSamples * sizeof(unsigned char)) );
	for (size_t i = 0; i < nShutterCycleSamples; i++)
		shutterScanSignal[i] = TRUE;
	
	// calculate number of pixels needed to switch the shutter
	uInt32	nShutterPix = (uInt32) ceil(scanEngine->baseClass.shutterSwitchTime/scanEngine->scanSettings->pixelDwellTime);
	
	// mark closed shutter times if the shutter switch time pixels are fewer than the line dead time pixels
	if (nShutterPix < nDeadTimePixels) {
		for (size_t i = 0; i < nDeadTimePixels - nShutterPix; i++)
			shutterScanSignal[i] = FALSE;
		for (size_t i = nGalvoSamplesPerLine - nDeadTimePixels; i < nGalvoSamplesPerLine + nDeadTimePixels - nShutterPix - 1; i++)
			shutterScanSignal[i] = FALSE;
		for (size_t i = nShutterCycleSamples - nDeadTimePixels; i < nShutterCycleSamples; i++)
			shutterScanSignal[i] = FALSE;
	}
	
	// generate shutter raster scan waveform 
	if (GetTaskControlMode(scanEngine->baseClass.taskControl) == TASK_FINITE)
		// finite mode
		nullChk( shutterScan_RepWaveform = init_RepeatedWaveform_type(Waveform_UChar, scanEngine->galvoSamplingRate, nShutterCycleSamples, (void**)&shutterScanSignal, (scanEngine->scanSettings->height + nFastAxisFlybackLines)/2.0 * nFrames) );
	else
		// continuous mode
		nullChk( shutterScan_RepWaveform = init_RepeatedWaveform_type(Waveform_UChar, scanEngine->galvoSamplingRate, nShutterCycleSamples, (void**)&shutterScanSignal, 0) );
	
	// generate shutter closed fly-in signal
	nullChk( shutterFlyInSignal = calloc(nGalvoSamplesFlyIn, sizeof(unsigned char)) );
	nullChk( shutterFlyIn_RepWaveform = init_RepeatedWaveform_type(Waveform_UChar, scanEngine->galvoSamplingRate, nGalvoSamplesFlyIn, (void**)&shutterFlyInSignal, 1) ); 
	
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
	nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(scanEngine->baseClass.taskControl), WAVERANK) );
	nullChk( dataPacket = init_DataPacket_type(DL_RepeatedWaveform_Double, (void**)&fastAxisMoveFromParked_RepWaveform, &dsInfo, (DiscardFptr_type)discard_RepeatedWaveform_type) );
	errChk( SendDataPacket(scanEngine->baseClass.VChanFastAxisCom, &dataPacket, FALSE, &errorInfo.errMsg) );
	
	// fastAxisScan_Waveform has two line scans (one triangle wave period)
	if (GetTaskControlMode(scanEngine->baseClass.taskControl) == TASK_FINITE)
		// finite mode
		nullChk( fastAxisScan_RepWaveform  = ConvertWaveformToRepeatedWaveformType(&fastAxisScan_Waveform, (scanEngine->scanSettings->height + nFastAxisFlybackLines)/2.0 * nFrames) ); 
	else 
		// for continuous mode
		nullChk( fastAxisScan_RepWaveform  = ConvertWaveformToRepeatedWaveformType(&fastAxisScan_Waveform, 0) ); 
	
	// send data
	nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(scanEngine->baseClass.taskControl), WAVERANK) );
	nullChk( dataPacket = init_DataPacket_type(DL_RepeatedWaveform_Double, (void**)&fastAxisScan_RepWaveform, &dsInfo, (DiscardFptr_type)discard_RepeatedWaveform_type) );   
	errChk( SendDataPacket(scanEngine->baseClass.VChanFastAxisCom, &dataPacket, FALSE, &errorInfo.errMsg) );    
	
	// go back to parked position if finite frame scan mode
	if (GetTaskControlMode(scanEngine->baseClass.taskControl) == TASK_FINITE) {
		// generate one sample
		nullChk( parkedVoltageSignal = malloc(sizeof(double)) );
		*parkedVoltageSignal = fastAxisCal->parked;
		RepeatedWaveform_type*	parkedRepeatedWaveform = init_RepeatedWaveform_type(RepeatedWaveform_Double, scanEngine->galvoSamplingRate, 1, (void**)&parkedVoltageSignal, 0);
		nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(scanEngine->baseClass.taskControl), WAVERANK) );
		nullChk( dataPacket = init_DataPacket_type(DL_RepeatedWaveform_Double, (void**)&parkedRepeatedWaveform, &dsInfo, (DiscardFptr_type)discard_RepeatedWaveform_type) ); 
		errChk( SendDataPacket(scanEngine->baseClass.VChanFastAxisCom, &dataPacket, FALSE, &errorInfo.errMsg) );    
		// send NULL packet to signal termination of data stream
		errChk( SendNullPacket(scanEngine->baseClass.VChanFastAxisCom, &errorInfo.errMsg) );
	}
	
	// send number of samples in fast axis command waveform if scan is finite
	nullChk( nGalvoSamplesPtr = malloc(sizeof(uInt64)) );
	if (GetTaskControlMode(scanEngine->baseClass.taskControl) == TASK_FINITE)
		// move from parked waveform + scan waveform + one sample to return to parked position
		*nGalvoSamplesPtr = (uInt64)((nGalvoSamplesFastAxisCompensation + nGalvoSamplesFastAxisMoveFromParkedWaveform) + (scanEngine->scanSettings->height+nFastAxisFlybackLines) * nGalvoSamplesPerLine * nFrames + 1);
	else
		*nGalvoSamplesPtr = 0;
		
	nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(scanEngine->baseClass.taskControl), WAVERANK) );
	nullChk( dataPacket = init_DataPacket_type(DL_UInt64, (void**)&nGalvoSamplesPtr, &dsInfo, NULL) );
	errChk( SendDataPacket(scanEngine->baseClass.VChanFastAxisComNSamples, &dataPacket, FALSE, &errorInfo.errMsg) );    
	
	//---------------------------------------------------------------------------------------------------------------------------
	// 													Y-galvo slow scan axis
	//---------------------------------------------------------------------------------------------------------------------------
	
	// move from parked position
	// convert waveform to repeated waveform
	nullChk( slowAxisMoveFromParked_RepWaveform = ConvertWaveformToRepeatedWaveformType(&slowAxisMoveFromParkedCompensatedWaveform, 1) );
	
	// send data 
	nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(scanEngine->baseClass.taskControl), WAVERANK) );
	nullChk( dataPacket = init_DataPacket_type(DL_RepeatedWaveform_Double, (void**)&slowAxisMoveFromParked_RepWaveform, &dsInfo, (DiscardFptr_type)discard_RepeatedWaveform_type) );
	errChk( SendDataPacket(scanEngine->baseClass.VChanSlowAxisCom, &dataPacket, FALSE, &errorInfo.errMsg) );
	
	// repeat staircase waveform
	if (GetTaskControlMode(scanEngine->baseClass.taskControl) == TASK_FINITE)
		// finite mode
		nullChk( slowAxisScan_RepWaveform  = ConvertWaveformToRepeatedWaveformType(&slowAxisScan_Waveform, nFrames) ); 
	else 
		// for continuous mode
		nullChk( slowAxisScan_RepWaveform  = ConvertWaveformToRepeatedWaveformType(&slowAxisScan_Waveform, 0) ); 
	
	// send data
	nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(scanEngine->baseClass.taskControl), WAVERANK) );
	nullChk( dataPacket = init_DataPacket_type(DL_RepeatedWaveform_Double, (void**)&slowAxisScan_RepWaveform, &dsInfo, (DiscardFptr_type)discard_RepeatedWaveform_type) );   
	errChk( SendDataPacket(scanEngine->baseClass.VChanSlowAxisCom, &dataPacket, FALSE, &errorInfo.errMsg) );  
	
	// go back to parked position if finite frame scan mode
	if (GetTaskControlMode(scanEngine->baseClass.taskControl) == TASK_FINITE) {
		// generate one sample
		nullChk( parkedVoltageSignal = malloc(sizeof(double)) );
		*parkedVoltageSignal = slowAxisCal->parked;
		RepeatedWaveform_type*	parkedRepeatedWaveform = init_RepeatedWaveform_type(RepeatedWaveform_Double, scanEngine->galvoSamplingRate, 1, (void**)&parkedVoltageSignal, 0);
		nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(scanEngine->baseClass.taskControl), WAVERANK) );
		nullChk( dataPacket = init_DataPacket_type(DL_RepeatedWaveform_Double, (void**)&parkedRepeatedWaveform, &dsInfo, (DiscardFptr_type)discard_RepeatedWaveform_type) ); 
		errChk( SendDataPacket(scanEngine->baseClass.VChanSlowAxisCom, &dataPacket, FALSE, &errorInfo.errMsg) );
		// send NULL packet to signal termination of data stream
		errChk( SendNullPacket(scanEngine->baseClass.VChanSlowAxisCom, &errorInfo.errMsg) );
	}
	
	// send number of samples in slow axis command waveform if scan is finite
	nullChk( nGalvoSamplesPtr = malloc(sizeof(uInt64)) );
	if (GetTaskControlMode(scanEngine->baseClass.taskControl) == TASK_FINITE)
		// move from parked waveform + scan waveform + one sample to return to parked position
		*nGalvoSamplesPtr = (uInt64) ((nGalvoSamplesSlowAxisCompensation + nGalvoSamplesSlowAxisMoveFromParked) + nFrames * nGalvoSamplesSlowAxisScanWaveform + 1);
	else
		*nGalvoSamplesPtr = 0;
	
	nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(scanEngine->baseClass.taskControl), WAVERANK) );
	nullChk( dataPacket = init_DataPacket_type(DL_UInt64, (void**)&nGalvoSamplesPtr, &dsInfo, NULL) );
	errChk( SendDataPacket(scanEngine->baseClass.VChanSlowAxisComNSamples, &dataPacket, FALSE, &errorInfo.errMsg) );    
	
	// Galvo sampling rate for both scan axes
	nullChk( galvoSamplingRatePtr = malloc(sizeof(double)) );
	*galvoSamplingRatePtr = scanEngine->galvoSamplingRate;
	nullChk( dataPacket = init_DataPacket_type(DL_UInt64, (void**)&galvoSamplingRatePtr, NULL, NULL) );
	errChk( SendDataPacket(scanEngine->baseClass.VChanGalvoComSamplingRate, &dataPacket, FALSE, &errorInfo.errMsg) ); 
	
	//---------------------------------------------------------------------------------------------------------------------------
	// 													Shutter signal
	//---------------------------------------------------------------------------------------------------------------------------
	
	// close shutter while moving from parked position to the scan region
	nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(scanEngine->baseClass.taskControl), WAVERANK) );
	nullChk( dataPacket = init_DataPacket_type(DL_RepeatedWaveform_UChar, (void**)&shutterFlyIn_RepWaveform, &dsInfo, (DiscardFptr_type)discard_RepeatedWaveform_type) );
	errChk( SendDataPacket(scanEngine->baseClass.VChanROIShutter, &dataPacket, FALSE, &errorInfo.errMsg) );
	
	// modulate shutter to blank fast axis line scan turn-around
	nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(scanEngine->baseClass.taskControl), WAVERANK) );
	nullChk( dataPacket = init_DataPacket_type(DL_RepeatedWaveform_UChar, (void**)&shutterScan_RepWaveform, &dsInfo, (DiscardFptr_type)discard_RepeatedWaveform_type) );
	errChk( SendDataPacket(scanEngine->baseClass.VChanROIShutter, &dataPacket, FALSE, &errorInfo.errMsg) );
	
	//---------------------------------------------------------------------------------------------------------------------------
	// 													Pixel timing info
	//---------------------------------------------------------------------------------------------------------------------------
	
	
	nullChk( nPixelsPtr = malloc(sizeof(uInt64)) );
		
	if (GetTaskControlMode(scanEngine->baseClass.taskControl) == TASK_FINITE) {
		// total number of pixels
		*nPixelsPtr = (uInt64)RoundRealToNearestInteger((scanEngine->flyInDelay - scanEngine->baseClass.pixDelay)/scanEngine->scanSettings->pixelDwellTime) + (uInt64)nPixelsPerLine * (uInt64)(scanEngine->scanSettings->height + nFastAxisFlybackLines) * (uInt64)nFrames; 
		// pixel pulse train
		nullChk( pixelPulseTrain = (PulseTrain_type*) init_PulseTrainTickTiming_type(PulseTrain_Finite, PulseTrainIdle_Low, *nPixelsPtr, (uInt32) RoundRealToNearestInteger(scanEngine->scanSettings->pixelDwellTime * 1e-6 * scanEngine->baseClass.referenceClockFreq) - 2, 2, 0, scanEngine->baseClass.referenceClockFreq) );
	} else {
		// total number of pixels
		*nPixelsPtr = 0;
		// pixel pulse train
		nullChk( pixelPulseTrain = (PulseTrain_type*) init_PulseTrainTickTiming_type(PulseTrain_Continuous, PulseTrainIdle_Low, 0, (uInt32) RoundRealToNearestInteger(scanEngine->scanSettings->pixelDwellTime * 1e-6 * scanEngine->baseClass.referenceClockFreq) - 2, 2, 0, scanEngine->baseClass.referenceClockFreq) );
	}
	
	// send n pixels
	nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(scanEngine->baseClass.taskControl), WAVERANK) );
	nullChk( dataPacket = init_DataPacket_type(DL_UInt64, (void**)&nPixelsPtr, &dsInfo, NULL) );
	errChk( SendDataPacket(scanEngine->baseClass.VChanNPixels, &dataPacket, FALSE, &errorInfo.errMsg) );    
	
	// send pixel pulse train info
	nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(scanEngine->baseClass.taskControl), WAVERANK) );
	nullChk( dataPacket = init_DataPacket_type(DL_PulseTrain_Ticks, (void**)&pixelPulseTrain, &dsInfo, (DiscardFptr_type)discard_PulseTrain_type) );
	errChk( SendDataPacket(scanEngine->baseClass.VChanPixelPulseTrain, &dataPacket, FALSE, &errorInfo.errMsg) ); 
	
	// send pixel sampling rate
	// pixel sampling rate in [Hz]
	nullChk( pixelSamplingRatePtr = malloc(sizeof(double)) );
	*pixelSamplingRatePtr = 1e+6/scanEngine->scanSettings->pixelDwellTime;
	nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(scanEngine->baseClass.taskControl), WAVERANK) );
	nullChk( dataPacket = init_DataPacket_type(DL_Double, (void**)&pixelSamplingRatePtr, &dsInfo, NULL) );
	errChk( SendDataPacket(scanEngine->baseClass.VChanPixelSamplingRate, &dataPacket, FALSE, &errorInfo.errMsg) ); 
	
	return 0; // no error
				  
Error:
	
	OKfree(fastAxisCommandSignal);
	OKfree(fastAxisCompensationSignal);
	OKfree(slowAxisCompensationSignal);
	OKfree(shutterScanSignal);
	OKfree(shutterFlyInSignal);
	OKfree(parkedVoltageSignal);
	OKfree(galvoSamplingRatePtr);
	OKfree(nGalvoSamplesPtr);
	OKfree(pixelSamplingRatePtr);
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
	discard_RepeatedWaveform_type(&shutterScan_RepWaveform);
	discard_RepeatedWaveform_type(&shutterFlyIn_RepWaveform);
	discard_PulseTrain_type(&pixelPulseTrain);
	ReleaseDataPacket(&dataPacket);
	discard_DSInfo_type(&dsInfo);
	
RETURN_ERR
}

/// HIFN Generates galvo command and ROI timing signals for jumping the laser beam from its parked position, through a series of points and returns it back to the parked position.
static int NonResRectRasterScan_GeneratePointJumpSignals (RectRaster_type* scanEngine, char** errorMsg)
{
#define NonResRectRasterScan_GeneratePointJumpSignals_Err_NoPoints				-1
#define NonResRectRasterScan_GeneratePointJumpSignals_Err_HoldBurstTooShort		-2

INIT_ERR

	PointScan_type*			pointScan						= &scanEngine->pointScan;
	size_t					nTotalPoints					= ListNumItems(scanEngine->pointScan.pointJumps);   	// number of point ROIs available
	Point_type*				pointJump						= NULL;
	size_t					nPointJumps						= 0;															// number of active point jump ROIs
	size_t					nVoltages						= 0;															// includes the number of active point jump ROIs plus twice the parked voltage
	size_t					nSamples						= 0;
	size_t					nJumpSamples					= 0;
	size_t					nGalvoJumpSamples				= 0;		// number of galvo samples during a galvo jump cycle.
	size_t					nNewGalvoJumpSamples			= 0;
	size_t					nPreviousGalvoJumpSamples		= 0;
	size_t					nExtraStartDelaySamples			= 0;		// number of galvo samples to keep the galvos at their parked position after the golbal start time point.
	size_t					nHoldSamples					= 0;		// number of galvo samples during a point ROI hold period.
	size_t					nExtraHoldSamples				= 0;
	size_t					nStimPulseONSamples				= 0;		// number of galvo samples during a stimulation pulse ON period.
	size_t					nStimPulseOFFSamples			= 0;		// number of galvo samples during a stimulation pulse OFF period.
	size_t					nStimPulseDelaySamples			= 0; 		// number of galvo samples from the start of a point ROI hold period to the onset of stimulation.
	double*					fastAxisVoltages				= NULL;
	double*					slowAxisVoltages				= NULL;
	double*					fastAxisJumpSignal				= NULL;
	double*					slowAxisJumpSignal				= NULL;
	double*					pixelSamplingRatePtr			= NULL;
	unsigned short*			ROIHoldSignal					= NULL;
	unsigned short*			ROIShutterSignal				= NULL;
	uInt64*					nGalvoSamplesPtr				= NULL;
	double*					galvoSamplingRatePtr			= NULL;
	uInt64					nPixels							= 0;
	uInt64*					nPixelsPtr						= NULL;
	PulseTrain_type*		pixelPulseTrain					= NULL;
	DataPacket_type*		dataPacket						= NULL; 
	NonResGalvoCal_type*	fastAxisCal						= (NonResGalvoCal_type*) scanEngine->baseClass.fastAxisCal;
	NonResGalvoCal_type*	slowAxisCal						= (NonResGalvoCal_type*) scanEngine->baseClass.slowAxisCal;
	double					totalJumpTime					= 0;	  	// time in [ms] from the global start trigger or the end of a previous hold burst cycle until the point jump cycle completes.
	double					totalPointJumpTime				= 0;		// sum of jump times in [ms] from the parked position to the first point, continuing with the next and finally back to the parked position.
	double					totalExtraHoldTime				= 0;		// together with the total galvo delays it makes up the total hold time in [ms].
	double					extraStartDelay					= 0;		// time in [ms] from the global start trigger or the end of a previous hold burst cycle  before the galvos initiate their jumps.
	double					extraHoldTime					= 0;		// time in [ms] that combined with the galvo response delay makes up the desired ROI hold time during which the galvo is not moving.
	DSInfo_type*			dsInfo							= NULL;
	
	// 1 hold cycle timing and galvo command signals
	RepeatedWaveform_type*	fastAxisJumpWaveform			= NULL;
	RepeatedWaveform_type*	slowAxisJumpWaveform			= NULL;
	Waveform_type*			ROIHoldWaveform					= NULL;
	Waveform_type*			ROIShutterWaveform			= NULL;
	
	
	//-----------------------------------------------
	// Calculate galvo jump voltages
	//-----------------------------------------------
	
	switch (scanEngine->pointScan.pointScanProtocol->jumpMethod) {
			
		case PointJump_SinglePoints:
			
			// set ROI jumping voltages
			for (size_t i = 1; i <= nTotalPoints; i++) {
				pointJump = *(Point_type**)ListGetPtrToItem(pointScan->pointJumps, i);
				if (!pointJump->baseClass.active) continue; // select only active point jumps
				
				nPointJumps++;
				if (nPointJumps < pointScan->currentActivePoint) continue; // select point to visit
				
				nVoltages = 3;
				nullChk(fastAxisVoltages = realloc(fastAxisVoltages, nVoltages * sizeof(double)) );
				nullChk(slowAxisVoltages = realloc(slowAxisVoltages, nVoltages * sizeof(double)) ); 
				NonResRectRasterScan_PointROIVoltage(scanEngine, pointJump, &fastAxisVoltages[1], &slowAxisVoltages[1]);
				break;
			}
			
			break;
				
		case PointJump_PointGroup:
			
			// set ROI jumping voltages
			for (size_t i = 1; i <= nTotalPoints; i++) {
				pointJump = *(Point_type**)ListGetPtrToItem(pointScan->pointJumps, i);
				if (!pointJump->baseClass.active) continue; // select only active point jumps
				
				nPointJumps++;
				nullChk(fastAxisVoltages = realloc(fastAxisVoltages, (nPointJumps+2) * sizeof(double)) );
				nullChk(slowAxisVoltages = realloc(slowAxisVoltages, (nPointJumps+2) * sizeof(double)) ); 
				NonResRectRasterScan_PointROIVoltage(scanEngine, pointJump, &fastAxisVoltages[nPointJumps], &slowAxisVoltages[nPointJumps]);
			}
			nVoltages = nPointJumps + 2;
			
			break;
			
		case PointJump_IncrementalPointGroup:
			
			// set ROI jumping voltages
			for (size_t i = 1; i <= nTotalPoints; i++) {
				pointJump = *(Point_type**)ListGetPtrToItem(pointScan->pointJumps, i);
				if (!pointJump->baseClass.active) continue; // select only active point jumps
				
				if (nPointJumps >= pointScan->nPointsInGroup) break; // add only up to nPointsinGroup per group.
				nPointJumps++;
					
				nullChk(fastAxisVoltages = realloc(fastAxisVoltages, (nPointJumps+2) * sizeof(double)) );
				nullChk(slowAxisVoltages = realloc(slowAxisVoltages, (nPointJumps+2) * sizeof(double)) ); 
				NonResRectRasterScan_PointROIVoltage(scanEngine, pointJump, &fastAxisVoltages[nPointJumps], &slowAxisVoltages[nPointJumps]);
			}
			nVoltages = nPointJumps + 2;
			
			break;
	}
	
	// if there are no active points, generate error
	if (nVoltages < 3)
		SET_ERR(NonResRectRasterScan_GeneratePointJumpSignals_Err_NoPoints, "No points to jump to.");
	
	// set the first and last voltages to be the parked voltage
	fastAxisVoltages[0] = fastAxisCal->parked;
	fastAxisVoltages[nVoltages-1] = fastAxisCal->parked;
	slowAxisVoltages[0] = slowAxisCal->parked;
	slowAxisVoltages[nVoltages-1] = slowAxisCal->parked;
	
	//-----------------------------------------------
	// Calculate galvo jump times
	//-----------------------------------------------
	
	// calculate jump times and galvo response delays
	OKfree(pointScan->jumpTimes);
	OKfree(pointScan->responseDelays);
	
	pointScan->nJumpTimes 		= nVoltages-1;
	pointScan->jumpTimes 		= calloc(pointScan->nJumpTimes, sizeof(double));
	pointScan->responseDelays 	= calloc(pointScan->nJumpTimes, sizeof(double));
	
	for (size_t i = 0; i < pointScan->nJumpTimes; i++) {
		errChk( NonResRectRasterScan_JumpTime(scanEngine, fastAxisVoltages[i] - fastAxisVoltages[i+1], slowAxisVoltages[i] - slowAxisVoltages[i+1], &pointScan->jumpTimes[i], &pointScan->responseDelays[i]) );
		// add up jump times between ROIs
		totalPointJumpTime += pointScan->jumpTimes[i];
	}
	
	// add extra hold time such that this time plus the response delays make up the target hold time
	for (size_t i = 1; i < pointScan->nJumpTimes; i++) {
		extraHoldTime = pointScan->pointScanProtocol->holdTime - pointScan->responseDelays[i];
		if (extraHoldTime < 0.0)
			extraHoldTime = 0.0;  // in this case the entire galvo hold period will fall within the galvo response time (the galvos received a command signal but did not leave yet the ROI within the calibrated resolution)
	
		totalExtraHoldTime += extraHoldTime;	
	}
		
	for (size_t holdBurstIdx = 0; holdBurstIdx < pointScan->pointScanProtocol->nHoldBurst; holdBurstIdx++) {
	
		// initialize total jump time
		totalJumpTime = totalPointJumpTime + totalExtraHoldTime;
		
		extraStartDelay = pointScan->startDelay - pointScan->minStartDelay;   // cannot be < 0 since the smallest value pointScan->startDelay can take is pointScan->minStartDelay
	
		// add extra start delay to the total jump time
		totalJumpTime +=  extraStartDelay;
	
		if (!holdBurstIdx) {
			// calculate number of pixels to skip
			for (size_t i = 0; i < scanEngine->nPointBuffers; i++) 
				scanEngine->pointBuffers[i]->nSkipPixels = (size_t)RoundRealToNearestInteger(((pointScan->startDelay * 1e3 + scanEngine->baseClass.pixDelay) * 1e-6 * scanEngine->galvoSamplingRate));
	
			// calculate number of pixels to record if required
			nPixels = (uInt64)RoundRealToNearestInteger(((totalJumpTime * 1e3 + scanEngine->baseClass.pixDelay) * 1e-6 * scanEngine->galvoSamplingRate));
		}
	
		//-----------------------------------------------
		// Generate galvo jump waveforms
		//-----------------------------------------------
	
		//------------------------------
		// allocate memory for waveforms
		//------------------------------
		
		nNewGalvoJumpSamples = (size_t)RoundRealToNearestInteger(totalJumpTime * 1e-3 * scanEngine->galvoSamplingRate);
		nPreviousGalvoJumpSamples = nGalvoJumpSamples; 
		nGalvoJumpSamples += nNewGalvoJumpSamples; 
		
		// fast axis signal
		nullChk( fastAxisJumpSignal = realloc(fastAxisJumpSignal, nGalvoJumpSamples * sizeof(double)) );
		Set1D(fastAxisJumpSignal + nPreviousGalvoJumpSamples, nNewGalvoJumpSamples, 0);
		
		// slow axis signal
		nullChk( slowAxisJumpSignal = realloc(slowAxisJumpSignal, nGalvoJumpSamples * sizeof(double)) );
		Set1D(slowAxisJumpSignal + nPreviousGalvoJumpSamples, nNewGalvoJumpSamples, 0);
	
		// hold timing signal
		if (IsVChanOpen((VChan_type*)scanEngine->baseClass.VChanROIHold)) {
			nullChk( ROIHoldSignal = realloc(ROIHoldSignal, nGalvoJumpSamples * sizeof(unsigned short)) );
			for (size_t i = nPreviousGalvoJumpSamples; i < nPreviousGalvoJumpSamples + nNewGalvoJumpSamples; i++)
				ROIHoldSignal[i] = 0;
		}
	
		// stimulation timing signal
		if (IsVChanOpen((VChan_type*)scanEngine->baseClass.VChanROIShutter)) {
			nullChk( ROIShutterSignal = realloc(ROIShutterSignal, nGalvoJumpSamples * sizeof(unsigned short)) );
			for (size_t i = nPreviousGalvoJumpSamples; i < nPreviousGalvoJumpSamples + nNewGalvoJumpSamples; i++)
				ROIShutterSignal[i] = 0;
		}
	
		//-----------------
		// create waveforms
		//-----------------
							   
		// calculate number of samples during galvo position holding time at a point ROI
		nExtraStartDelaySamples	= (size_t)RoundRealToNearestInteger(extraStartDelay * 1e-3 * scanEngine->galvoSamplingRate);
		nHoldSamples 			= (size_t)RoundRealToNearestInteger(pointScan->pointScanProtocol->holdTime * 1e-3 * scanEngine->galvoSamplingRate);
		nStimPulseONSamples 	= (size_t)RoundRealToNearestInteger(pointScan->pointScanProtocol->stimPulseONDuration * 1e-3 * scanEngine->galvoSamplingRate);
		nStimPulseOFFSamples 	= (size_t)RoundRealToNearestInteger(pointScan->pointScanProtocol->stimPulseOFFDuration * 1e-3 * scanEngine->galvoSamplingRate);
		nStimPulseDelaySamples	= (size_t)RoundRealToNearestInteger(pointScan->pointScanProtocol->stimDelay * 1e-3 * scanEngine->galvoSamplingRate);
	
		// keep galvos parked during the extra start delay (the extra start delay + jump time from parked position to 1st point ROI is the start delay)
		if (nExtraStartDelaySamples) {
			Set1D(fastAxisJumpSignal + nPreviousGalvoJumpSamples, nExtraStartDelaySamples, fastAxisCal->parked);
			Set1D(slowAxisJumpSignal + nPreviousGalvoJumpSamples, nExtraStartDelaySamples, slowAxisCal->parked);
		}
	
		nSamples = nPreviousGalvoJumpSamples + nExtraStartDelaySamples;
		for (size_t i = 0; i < pointScan->nJumpTimes - 1; i++) {
		
			extraHoldTime = pointScan->pointScanProtocol->holdTime - pointScan->responseDelays[i+1];
			if (extraHoldTime < 0.0)
				extraHoldTime = 0.0;  // in this case the entire galvo hold period will fall within the galvo response time (the galvos received a command signal but did not leave yet the ROI within the calibrated resolution)
		
			nJumpSamples = (size_t)RoundRealToNearestInteger(pointScan->jumpTimes[i] * 1e-3 * scanEngine->galvoSamplingRate);   
			nExtraHoldSamples = (size_t)RoundRealToNearestInteger(extraHoldTime * 1e-3 * scanEngine->galvoSamplingRate);
		
			// set galvo signals
			Set1D(fastAxisJumpSignal + nSamples, nJumpSamples + nExtraHoldSamples, fastAxisVoltages[i+1]);
			Set1D(slowAxisJumpSignal + nSamples, nJumpSamples + nExtraHoldSamples, slowAxisVoltages[i+1]);
		
			// set ROI hold signal
			if (IsVChanOpen((VChan_type*)scanEngine->baseClass.VChanROIHold))
				for (size_t j = nSamples + nJumpSamples; j < nSamples + nJumpSamples + nHoldSamples; j++)
					ROIHoldSignal[j] = TRUE;
		
			// set ROI stimulate signal
			if (IsVChanOpen((VChan_type*)scanEngine->baseClass.VChanROIShutter))
				for (uInt32 pulse = 0; pulse < pointScan->pointScanProtocol->nStimPulses; pulse++)
					for (size_t j = nSamples + nJumpSamples + nStimPulseDelaySamples + pulse * (nStimPulseONSamples + nStimPulseOFFSamples); 
						j < nSamples + nJumpSamples + nStimPulseDelaySamples + pulse * (nStimPulseONSamples+nStimPulseOFFSamples) + nStimPulseONSamples; j++)
						ROIShutterSignal[j] = TRUE;
		
			nSamples += nJumpSamples + nExtraHoldSamples;
		}
	
		// jump back to parked position
		Set1D(fastAxisJumpSignal + nSamples, nGalvoJumpSamples - nSamples, fastAxisVoltages[nVoltages - 1]);
		Set1D(slowAxisJumpSignal + nSamples, nGalvoJumpSamples - nSamples, slowAxisVoltages[nVoltages - 1]);
	
		// update start delay
		if (pointScan->pointScanProtocol->nHoldBurst > 1) {
			pointScan->startDelay = pointScan->pointScanProtocol->holdBurstPeriod + pointScan->pointScanProtocol->holdBurstPeriodIncr - (totalJumpTime - extraStartDelay - pointScan->jumpTimes[0]);
			if (pointScan->startDelay < 0.0)
				SET_ERR(NonResRectRasterScan_GeneratePointJumpSignals_Err_HoldBurstTooShort, "Hold burst period is too short.");
		}
	
	} // cycling over Hold Burst Index
	
	//-------------------------------
	// Transform signals to waveforms
	//-------------------------------
	
	// generate galvo command jump waveforms
	nullChk( fastAxisJumpWaveform = init_RepeatedWaveform_type(RepeatedWaveform_Double, scanEngine->galvoSamplingRate, nGalvoJumpSamples, (void**)&fastAxisJumpSignal, 1.0) );
	nullChk( slowAxisJumpWaveform = init_RepeatedWaveform_type(RepeatedWaveform_Double, scanEngine->galvoSamplingRate, nGalvoJumpSamples, (void**)&slowAxisJumpSignal, 1.0) );
	
	// generate ROI hold waveform
	if (ROIHoldSignal) {
		nullChk( ROIHoldWaveform = init_Waveform_type(Waveform_UShort, scanEngine->galvoSamplingRate, nGalvoJumpSamples, (void**)&ROIHoldSignal) );
	}
	
	// generate ROI stimulate waveform
	if (ROIShutterSignal) {
		nullChk( ROIShutterWaveform = init_Waveform_type(Waveform_UShort, scanEngine->galvoSamplingRate, nGalvoJumpSamples, (void**)&ROIShutterSignal) );
	}
	
	
	//------------------------------------------------------------
	// Send galvo, ROI timing and stimulation waveforms
	//------------------------------------------------------------
	
	// fast galvo axis
	nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(scanEngine->baseClass.taskControl), WAVERANK) );
	nullChk( dataPacket = init_DataPacket_type(DL_RepeatedWaveform_Double, (void**)&fastAxisJumpWaveform, &dsInfo, (DiscardFptr_type)discard_RepeatedWaveform_type) );
	errChk( SendDataPacket(scanEngine->baseClass.VChanFastAxisCom, &dataPacket, FALSE, &errorInfo.errMsg) );
	errChk( SendNullPacket(scanEngine->baseClass.VChanFastAxisCom, &errorInfo.errMsg) );
	
	
	// slow galvo axis
	nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(scanEngine->baseClass.taskControl), WAVERANK) );
	nullChk( dataPacket = init_DataPacket_type(DL_RepeatedWaveform_Double, (void**)&slowAxisJumpWaveform, &dsInfo, (DiscardFptr_type)discard_RepeatedWaveform_type) );
	errChk( SendDataPacket(scanEngine->baseClass.VChanSlowAxisCom, &dataPacket, FALSE, &errorInfo.errMsg) );
	errChk( SendNullPacket(scanEngine->baseClass.VChanSlowAxisCom, &errorInfo.errMsg) );
	
	// ROI hold
	if (IsVChanOpen((VChan_type*)scanEngine->baseClass.VChanROIHold)) {
		nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(scanEngine->baseClass.taskControl), WAVERANK) );
		nullChk( dataPacket = init_DataPacket_type(DL_Waveform_UShort, (void**)&ROIHoldWaveform, &dsInfo, (DiscardFptr_type)discard_Waveform_type) );
		errChk( SendDataPacket(scanEngine->baseClass.VChanROIHold, &dataPacket, FALSE, &errorInfo.errMsg) );
		errChk( SendNullPacket(scanEngine->baseClass.VChanROIHold, &errorInfo.errMsg) );
	}
	
	// ROI stimulate
	if (IsVChanOpen((VChan_type*)scanEngine->baseClass.VChanROIShutter)) {
		nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(scanEngine->baseClass.taskControl), WAVERANK) );
		nullChk( dataPacket = init_DataPacket_type(DL_Waveform_UShort, (void**)&ROIShutterWaveform, &dsInfo, (DiscardFptr_type)discard_Waveform_type) );
		errChk( SendDataPacket(scanEngine->baseClass.VChanROIShutter, &dataPacket, FALSE, &errorInfo.errMsg) );
		errChk( SendNullPacket(scanEngine->baseClass.VChanROIShutter, &errorInfo.errMsg) );
	}
	
	//------------------------------------------------------------
	// Send number of galvo samples needed for the point jump
	//------------------------------------------------------------
	
	nullChk( nGalvoSamplesPtr = malloc(sizeof(uInt64)) );
	// initial start delay waveform + jump waveform
	*nGalvoSamplesPtr = (uInt64) nGalvoJumpSamples;
	nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(scanEngine->baseClass.taskControl), WAVERANK) );
	nullChk( dataPacket = init_DataPacket_type(DL_UInt64, (void**)&nGalvoSamplesPtr, &dsInfo, NULL) );
	errChk( SendDataPacket(scanEngine->baseClass.VChanFastAxisComNSamples, &dataPacket, FALSE, &errorInfo.errMsg) );
	nullChk( nGalvoSamplesPtr = malloc(sizeof(uInt64)) );
	*nGalvoSamplesPtr = (uInt64) nGalvoJumpSamples;
	nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(scanEngine->baseClass.taskControl), WAVERANK) );
	nullChk( dataPacket = init_DataPacket_type(DL_UInt64, (void**)&nGalvoSamplesPtr, &dsInfo, NULL) );
	errChk( SendDataPacket(scanEngine->baseClass.VChanSlowAxisComNSamples, &dataPacket, FALSE, &errorInfo.errMsg) );
	
	//------------------------------------------------------------
	// Send galvo sampling rate
	//------------------------------------------------------------
	
	nullChk( galvoSamplingRatePtr = malloc(sizeof(double)) );
	*galvoSamplingRatePtr = scanEngine->galvoSamplingRate;
	nullChk( dataPacket = init_DataPacket_type(DL_UInt64, (void**)&galvoSamplingRatePtr, NULL, NULL) );
	errChk( SendDataPacket(scanEngine->baseClass.VChanGalvoComSamplingRate, &dataPacket, FALSE, &errorInfo.errMsg) ); 
	
	//------------------------------------------------------------
	// Send number of pixels to acquire if recording is needed
	//------------------------------------------------------------
	
	if (IsVChanOpen((VChan_type*)scanEngine->baseClass.VChanNPixels)) {
		nullChk( nPixelsPtr = malloc(sizeof(uInt64)) );
		*nPixelsPtr = nPixels;
		nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(scanEngine->baseClass.taskControl), WAVERANK) );
		nullChk( dataPacket = init_DataPacket_type(DL_UInt64, (void**)&nPixelsPtr, &dsInfo, NULL) );
		errChk( SendDataPacket(scanEngine->baseClass.VChanNPixels, &dataPacket, FALSE, &errorInfo.errMsg) );  
	}
	
	//------------------------------------------------------------
	// Send pixel sampling info if recording is needed
	//------------------------------------------------------------
	
	// send pixel pulse train info
	if (IsVChanOpen((VChan_type*)scanEngine->baseClass.VChanPixelPulseTrain)) {
		nullChk( pixelPulseTrain = (PulseTrain_type*) init_PulseTrainTickTiming_type(PulseTrain_Finite, PulseTrainIdle_Low, nPixels, (uInt32) RoundRealToNearestInteger(1/scanEngine->galvoSamplingRate * scanEngine->baseClass.referenceClockFreq) - 2, 2, 0, scanEngine->baseClass.referenceClockFreq) );
		nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(scanEngine->baseClass.taskControl), WAVERANK) );
		nullChk( dataPacket = init_DataPacket_type(DL_PulseTrain_Ticks, (void**)&pixelPulseTrain, &dsInfo, (DiscardFptr_type)discard_PulseTrain_type) );
		errChk( SendDataPacket(scanEngine->baseClass.VChanPixelPulseTrain, &dataPacket, FALSE, &errorInfo.errMsg) ); 
	}
	
	// send pixel sampling rate in [Hz]
	if (IsVChanOpen((VChan_type*)scanEngine->baseClass.VChanPixelSamplingRate)) {
		nullChk( pixelSamplingRatePtr = malloc(sizeof(double)) );
		*pixelSamplingRatePtr = scanEngine->galvoSamplingRate;
		nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(scanEngine->baseClass.taskControl), WAVERANK) );
		nullChk( dataPacket = init_DataPacket_type(DL_Double, (void**)&pixelSamplingRatePtr, &dsInfo, NULL) );
		errChk( SendDataPacket(scanEngine->baseClass.VChanPixelSamplingRate, &dataPacket, FALSE, &errorInfo.errMsg) ); 
	}
	
	
	// cleanup
	OKfree(fastAxisVoltages);
	OKfree(slowAxisVoltages);
	
	return 0;
	
Error:
	
	// cleanup
	OKfree(fastAxisVoltages);
	OKfree(slowAxisVoltages);
	OKfree(fastAxisJumpSignal);
	OKfree(slowAxisJumpSignal);
	OKfree(ROIHoldSignal);
	OKfree(ROIShutterSignal);
	OKfree(nGalvoSamplesPtr);
	OKfree(galvoSamplingRatePtr);
	OKfree(nPixelsPtr);
	OKfree(pixelSamplingRatePtr);
	discard_RepeatedWaveform_type(&fastAxisJumpWaveform);
	discard_RepeatedWaveform_type(&slowAxisJumpWaveform);
	discard_Waveform_type(&ROIHoldWaveform);
	discard_Waveform_type(&ROIShutterWaveform);
	discard_PulseTrain_type(&pixelPulseTrain);
	discard_DataPacket_type(&dataPacket);
	discard_DSInfo_type(&dsInfo);
	
RETURN_ERR
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


static int NonResRectRasterScan_BuildImage (RectRaster_type* rectRaster, size_t bufferIdx, char** errorMsg)
{
#define NonResRectRasterScan_BuildImage_Err_WrongPixelDataType			-1
#define NonResRectRasterScan_BuildImage_Err_NoDisplay					-2

INIT_ERR

	DataPacket_type*			pixelPacket					= NULL;
	ImageTypes					imageType					= 0;		// Data type of assembled image
	Waveform_type*				pixelWaveform				= NULL;   	// Incoming pixel waveform.
	void*						pixelData					= NULL;		// Array of received pixels from a single pixel waveform data packet.
	size_t						nPixels						= 0;		// Number of received pixels from a pixel data packet stored in pixelData.
	size_t             			pixelDataIdx   				= 0;      	// The index of the processed pixel from the received pixel waveform.
	RectRasterImgBuff_type*   	imgBuffer					= rectRaster->imgBuffers[bufferIdx];
	size_t						nDeadTimePixels				= (size_t) ceil(((NonResGalvoCal_type*)rectRaster->baseClass.fastAxisCal)->triangleCal->deadTime * 1e3 / rectRaster->scanSettings->pixelDwellTime);
	ImageDisplay_type**			imgDisplayPtr				= NULL;
	DataPacket_type* 			imagePacket         		= NULL;
	Image_type*					sendImage					= NULL;
	int* 						nActivePixelBuildersTSVPtr 	= NULL;
	
	ListType					pointJumpROIList 			= 0;
	ListType					frameScanROIList			= 0;
	ListType					ROIList						= 0;
	
	
	do {
		
		// if there are enough pixels to construct a row take them out of the buffer
		while (imgBuffer->nTmpPixels >= rectRaster->scanSettings->width + 2 * nDeadTimePixels) {
			
			if (!imgBuffer->skipRows) {
				
				switch (imgBuffer->pixelDataType) {   
					
					case DL_Waveform_UChar:
				
						// allocate buffer memory if needed
						if (!imgBuffer->imagePixels)
							imgBuffer->imagePixels = malloc(rectRaster->scanSettings->width * rectRaster->scanSettings->height * sizeof(unsigned char));
						
						// add pixels
						if (imgBuffer->flipRow)
							for (uInt32 i = 0; i < rectRaster->scanSettings->width; i++)
								*((unsigned char*)imgBuffer->imagePixels + imgBuffer->nImagePixels + i) = *((unsigned char*)imgBuffer->tmpPixels + nDeadTimePixels + rectRaster->scanSettings->width - 1 - i);
						else 
							memcpy((unsigned char*)imgBuffer->imagePixels + imgBuffer->nImagePixels, (unsigned char*)imgBuffer->tmpPixels + nDeadTimePixels, rectRaster->scanSettings->width * sizeof(unsigned char));
					
						break;
					
					case DL_Waveform_UShort:
						
						// allocate buffer memory if needed
						if (!imgBuffer->imagePixels)
							imgBuffer->imagePixels = malloc(rectRaster->scanSettings->width * rectRaster->scanSettings->height * sizeof(unsigned short));
						
						// add pixels
						if (imgBuffer->flipRow)
							for (uInt32 i = 0; i < rectRaster->scanSettings->width; i++)
								*((unsigned short*)imgBuffer->imagePixels + imgBuffer->nImagePixels + i) = *((unsigned short*)imgBuffer->tmpPixels + nDeadTimePixels + rectRaster->scanSettings->width - 1 - i);
						else
							memcpy((unsigned short*)imgBuffer->imagePixels + imgBuffer->nImagePixels, (unsigned short*)imgBuffer->tmpPixels + nDeadTimePixels, rectRaster->scanSettings->width * sizeof(unsigned short));
					
						break;
						
					case DL_Waveform_Short:
						
						// allocate buffer memory if needed
						if (!imgBuffer->imagePixels)
							imgBuffer->imagePixels = malloc(rectRaster->scanSettings->width * rectRaster->scanSettings->height * sizeof(short));
						
						// add pixels
						if (imgBuffer->flipRow)
							for (uInt32 i = 0; i < rectRaster->scanSettings->width; i++)
								*((short*)imgBuffer->imagePixels + imgBuffer->nImagePixels + i) = *((short*)imgBuffer->tmpPixels + nDeadTimePixels + rectRaster->scanSettings->width - 1 - i);
						else
							memcpy((short*)imgBuffer->imagePixels + imgBuffer->nImagePixels, (short*)imgBuffer->tmpPixels + nDeadTimePixels, rectRaster->scanSettings->width * sizeof(short));
					
						break;
						
					case DL_Waveform_UInt:
						
						// allocate buffer memory if needed
						if (!imgBuffer->imagePixels)
							imgBuffer->imagePixels = malloc(rectRaster->scanSettings->width * rectRaster->scanSettings->height * sizeof(unsigned int));
						
						// add pixels
						if (imgBuffer->flipRow)
							for (uInt32 i = 0; i < rectRaster->scanSettings->width; i++)
								*((unsigned int*)imgBuffer->imagePixels + imgBuffer->nImagePixels + i) = *((unsigned int*)imgBuffer->tmpPixels + nDeadTimePixels + rectRaster->scanSettings->width - 1 - i);
						else
							memcpy((unsigned int*)imgBuffer->imagePixels + imgBuffer->nImagePixels, (unsigned int*)imgBuffer->tmpPixels + nDeadTimePixels, rectRaster->scanSettings->width * sizeof(unsigned int));
					
						break;
						
					case DL_Waveform_Float:
					
						// allocate buffer memory if needed
						if (!imgBuffer->imagePixels)
							imgBuffer->imagePixels = malloc(rectRaster->scanSettings->width * rectRaster->scanSettings->height * sizeof(float));
						
						// add pixels
						if (imgBuffer->flipRow)
							for (uInt32 i = 0; i < rectRaster->scanSettings->width; i++)
								*((float*)imgBuffer->imagePixels + imgBuffer->nImagePixels + i) = *((float*)imgBuffer->tmpPixels + nDeadTimePixels + rectRaster->scanSettings->width - 1 - i);
						else
							memcpy((float*)imgBuffer->imagePixels + imgBuffer->nImagePixels, (float*)imgBuffer->tmpPixels + nDeadTimePixels, rectRaster->scanSettings->width * sizeof(float));
					
						break;
					
					default:
				
					SET_ERR(NonResRectRasterScan_BuildImage_Err_WrongPixelDataType, "Wrong pixel data type.");
				
				}
			
				// update number of pixels in the image
				imgBuffer->nImagePixels	+= rectRaster->scanSettings->width; 
				// increment number of rows
				imgBuffer->nAssembledRows++;
				
			} else {
				
				imgBuffer->rowsSkipped++;
				if (imgBuffer->rowsSkipped == imgBuffer->skipFlybackRows)
					imgBuffer->skipRows = FALSE;
			}
			
			// move contents of the buffer to its beginning  
			switch (imgBuffer->pixelDataType) {
					
				case DL_Waveform_UChar:
					
					memmove((unsigned char*)imgBuffer->tmpPixels, (unsigned char*)imgBuffer->tmpPixels + 2 * nDeadTimePixels + rectRaster->scanSettings->width, (imgBuffer->nTmpPixels - 2 * nDeadTimePixels - rectRaster->scanSettings->width) * sizeof(unsigned char));
					break;
					
				case DL_Waveform_UShort:
					
					memmove((unsigned short*)imgBuffer->tmpPixels, (unsigned short*)imgBuffer->tmpPixels + 2 * nDeadTimePixels + rectRaster->scanSettings->width, (imgBuffer->nTmpPixels - 2 * nDeadTimePixels - rectRaster->scanSettings->width) * sizeof(unsigned short));
					break;
					
				case DL_Waveform_Short:
					
					memmove((short*)imgBuffer->tmpPixels, (short*)imgBuffer->tmpPixels + 2 * nDeadTimePixels + rectRaster->scanSettings->width, (imgBuffer->nTmpPixels - 2 * nDeadTimePixels - rectRaster->scanSettings->width) * sizeof(short));
					break;
				
				case DL_Waveform_UInt:
					
					memmove((unsigned int*)imgBuffer->tmpPixels, (unsigned int*)imgBuffer->tmpPixels + 2 * nDeadTimePixels + rectRaster->scanSettings->width, (imgBuffer->nTmpPixels - 2 * nDeadTimePixels - rectRaster->scanSettings->width) * sizeof(unsigned int));
					break;
					
				case DL_Waveform_Float:
					
					memmove((float*)imgBuffer->tmpPixels, (float*)imgBuffer->tmpPixels + 2 * nDeadTimePixels + rectRaster->scanSettings->width, (imgBuffer->nTmpPixels - 2 * nDeadTimePixels - rectRaster->scanSettings->width) * sizeof(float));
					break;
					
				default:
				
					SET_ERR(NonResRectRasterScan_BuildImage_Err_WrongPixelDataType, "Wrong pixel data type.");
			}
				
			// update number of pixels in the buffers
			imgBuffer->nTmpPixels  	-= rectRaster->scanSettings->width + 2 * nDeadTimePixels;
			// reverse pixel direction of every second row
			imgBuffer->flipRow = !imgBuffer->flipRow;
			
			
			if (imgBuffer->nAssembledRows == rectRaster->scanSettings->height) {
				
				switch (imgBuffer->pixelDataType) {
				
					case DL_Waveform_UChar:
						imageType = Image_UChar;
						break;
		
					case DL_Waveform_UShort:
						imageType = Image_UShort;    
						break;
			
					case DL_Waveform_Short:
						imageType = Image_Short;    
						break;
						
					case DL_Waveform_UInt:
						imageType = Image_UInt;    
						break;
						
					case DL_Waveform_Float:
						imageType = Image_Float;     
						break;
			
					default:
						
						SET_ERR(NonResRectRasterScan_BuildImage_Err_WrongPixelDataType, "Wrong pixel data type.");
				}
				
				//--------------------------------------------
				// Wait if there is already an assembled image
				//--------------------------------------------
				
				while (imgBuffer->image && !GetTaskControlAbortFlag(rectRaster->baseClass.taskControl)) {
					
					Sleep(20); // this is an idle wait and processing is transfered to other threads.
				}
				
				// check if wait was aborted
				if (GetTaskControlAbortFlag(rectRaster->baseClass.taskControl)) {
					// send iteration done event only once for all the image building threads
					CmtErrChk( CmtGetTSVPtr(rectRaster->baseClass.nActivePixelBuildersTSV, &nActivePixelBuildersTSVPtr) );
					if (*nActivePixelBuildersTSVPtr) {
						errChk( TaskControlIterationDone(rectRaster->baseClass.taskControl, 0, "", FALSE, &errorInfo.errMsg) );
						*nActivePixelBuildersTSVPtr = 0; // so that other threads aborting
						CmtErrChk( CmtReleaseTSVPtr(rectRaster->baseClass.nActivePixelBuildersTSV) );
						return 0;
					} else {
						CmtErrChk( CmtReleaseTSVPtr(rectRaster->baseClass.nActivePixelBuildersTSV) );
						return 0;
					}
				}
				
				//-----------------------
				// Create image container
				//-----------------------
				
				nullChk( imgBuffer->image = init_Image_type(imageType, rectRaster->scanSettings->height, rectRaster->scanSettings->width, &imgBuffer->imagePixels) );  
				SetImagePixSize(imgBuffer->image, rectRaster->scanSettings->pixSize);

				// TEMPORARY: X, Y & Z coordinates set to 0 for now
				SetImageCoord(imgBuffer->image, 0, 0, 0);
				
				// add stored point and frame scan ROIs if any
				nullChk( pointJumpROIList = CopyROIList(rectRaster->pointScan.pointJumps) );
				nullChk( frameScanROIList = CopyROIList(rectRaster->scanROIs) );
				nullChk( ROIList = ListCreate(sizeof(ROI_type*)) );
				nullChk( ListAppend(ROIList, pointJumpROIList) );
				OKfreeList(&pointJumpROIList, NULL);
				nullChk( ListAppend(ROIList, frameScanROIList) );
				OKfreeList(&frameScanROIList, NULL);
				SetImageROIs(imgBuffer->image, ROIList);
				ROIList = 0;
				
				//-------------------------------------
				// Update image display callback group
				//-------------------------------------
				
				imgDisplayPtr = NULL;
				errChk( CmtGetTSVPtr(imgBuffer->scanChan->imgDisplayTSV, &imgDisplayPtr) ); imgBuffer->scanChan->imgDisplayTSVLineNumDebug = __LINE__;
				
				CallbackGroup_type*				imgDisplayCBGroup				= NULL;
				CallbackFptr_type				CBFns[] 						= {(CallbackFptr_type)ImageDisplay_CB};
				void* 							callbackData[]					= {init_RectRasterDisplayCBData_type(rectRaster, bufferIdx)};
				DiscardFptr_type 				discardCallbackDataFunctions[] 	= {(DiscardFptr_type)discard_RectRasterDisplayCBData_type};
				
				#ifdef __ImageDisplayNIVision_H__
				
					if (!*imgDisplayPtr) {
						// if display was discarded, create a new display
						nullChk( *imgDisplayPtr = (ImageDisplay_type*)init_ImageDisplayNIVision_type (imgBuffer->scanChan, 0, imageType, rectRaster->scanSettings->width, rectRaster->scanSettings->height, NULL) );
						// create new callback group
						nullChk( imgDisplayCBGroup = init_CallbackGroup_type(*imgDisplayPtr, NumElem(CBFns), CBFns, callbackData, discardCallbackDataFunctions) );
					} else {
						// discard old callback group
						discard_CallbackGroup_type(&(*imgDisplayPtr)->callbackGroup);
						// create new callback group
						nullChk( imgDisplayCBGroup = init_CallbackGroup_type(*imgDisplayPtr, NumElem(CBFns), CBFns, callbackData, discardCallbackDataFunctions) );
						// assign new callback group to the display
						(*imgDisplayPtr)->callbackGroup = imgDisplayCBGroup;
					}
					
				#else
					
					#ifdef __ImageDisplayCVI_H__
				
						// discard old callback group
						discard_CallbackGroup_type(&(*imgDisplayPtr)->callbackGroup);
						// create new callback group
						nullChk( imgDisplayCBGroup = init_CallbackGroup_type(*imgDisplayPtr, NumElem(CBFns), CBFns, callbackData, discardCallbackDataFunctions) );
						// assign new callback group to the display
						(*imgDisplayPtr)->callbackGroup = imgDisplayCBGroup;

					#else
					
						SET_ERR(NonResRectRasterScan_BuildImage_Err_NoDisplay, "There is no display selected.");
					
					#endif
						
				#endif
				
				
				
				//--------------------------------------
				// Display image for this channel
				//--------------------------------------
				
				errChk( (*(*imgDisplayPtr)->displayImageFptr) (*imgDisplayPtr, &imgBuffer->image, &errorInfo.errMsg) );
				
				errChk( CmtReleaseTSVPtr(imgBuffer->scanChan->imgDisplayTSV) );
				imgBuffer->scanChan->imgDisplayTSVLineNumDebug = 0;
				imgDisplayPtr = NULL;
				
				
				//--------------------------------------
				// Send image for this channel if needed
				//--------------------------------------
				
				if (IsVChanOpen((VChan_type*)imgBuffer->scanChan->outputVChan)) {
					// make a copy of the image
					nullChk( sendImage = copy_Image_type(imgBuffer->image) );
					
					DSInfo_type* dsInfo = GetIteratorDSData(GetTaskControlIterator(rectRaster->baseClass.taskControl), WAVERANK);	// Shouldn't this be IMAGERANK ???
					nullChk( imagePacket = init_DataPacket_type(DL_Image, (void**)&sendImage, &dsInfo,(DiscardFptr_type)discard_Image_type));
					errChk( SendDataPacket(imgBuffer->scanChan->outputVChan, &imagePacket, 0, &errorInfo.errMsg) );
				}
				
				//---------------------------------------------------
				// Assemble composite image if all channels are ready
				//---------------------------------------------------
				
				errChk( CmtGetTSVPtr(rectRaster->baseClass.nActivePixelBuildersTSV, &nActivePixelBuildersTSVPtr) );
				(*nActivePixelBuildersTSVPtr)--;
	
				if (!*nActivePixelBuildersTSVPtr) {
		
					//errChk( NonResRectRasterScan_AssembleCompositeImage(rectRaster, &errMsg) );
					
					// TEMPORARY
					// discard images from all the channels
					for (size_t i = 0; i < rectRaster->nImgBuffers; i++) {
						discard_Image_type(&rectRaster->imgBuffers[i]->image);
						
					}
		
					// complete iteration
					errChk( TaskControlIterationDone(rectRaster->baseClass.taskControl, 0, "", FALSE, &errorInfo.errMsg) );
				}
	
				CmtReleaseTSVPtr(rectRaster->baseClass.nActivePixelBuildersTSV);
				
				
				//------------------------
				// reset number of elements in pixdata buffer (contents is not cleared!) new frame data will overwrite the old frame data in the buffer
				// NOTE & WARNING: data in the imgBuffer->tmpPixels is kept since multiple images can follow and imgBuffer->tmpPixels may contain data from the next image
				imgBuffer->nImagePixels = 0;
				// reset row counter
				imgBuffer->nAssembledRows = 0;
				// start skipping flyback rows
				imgBuffer->skipRows = TRUE;
				imgBuffer->rowsSkipped = 0;
				
				return 0;
			}
			
			
		} // end of while loop, all available lines in tmpdata have been processed into pixdata
		
		//---------------------------------------------------------------------------
		// Receive pixel data 
		//---------------------------------------------------------------------------
		
		errChk( GetDataPacket(imgBuffer->scanChan->detVChan, &pixelPacket, &errorInfo.errMsg) );
		
		//----------------------------------------------------------------------
		// Process NULL packet
		//----------------------------------------------------------------------
		
		// end task controller iteration
		if (!pixelPacket) {
			errChk( TaskControlIterationDone(rectRaster->baseClass.taskControl, 0, "", FALSE, &errorInfo.errMsg) );
			break;
		}
			
		
		//----------------------------------------------------------------------
		// Add data to the buffer
		//----------------------------------------------------------------------
		
		// get pointer to pixel array
		pixelWaveform = *(Waveform_type**) GetDataPacketPtrToData(pixelPacket, &imgBuffer->pixelDataType);
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
		switch (imgBuffer->pixelDataType) {
			
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
				
			case DL_Waveform_UInt:
				
				nullChk( imgBuffer->tmpPixels = realloc(imgBuffer->tmpPixels, (imgBuffer->nTmpPixels + nPixels - pixelDataIdx) * sizeof(unsigned int)) );
				memcpy((unsigned int*)imgBuffer->tmpPixels + imgBuffer->nTmpPixels, (unsigned int*)pixelData + pixelDataIdx, (nPixels - pixelDataIdx) * sizeof(unsigned int));
				break;
				
			case DL_Waveform_Float:
					
				nullChk( imgBuffer->tmpPixels = realloc(imgBuffer->tmpPixels, (imgBuffer->nTmpPixels + nPixels - pixelDataIdx) * sizeof(float)) );
				memcpy((float*)imgBuffer->tmpPixels + imgBuffer->nTmpPixels, (float*)pixelData + pixelDataIdx, (nPixels - pixelDataIdx) * sizeof(float));
				break;
				
			default:
				
				SET_ERR(NonResRectRasterScan_BuildImage_Err_WrongPixelDataType, "Wrong pixel data type.");
		}
		
		ReleaseDataPacket(&pixelPacket);
			
		// update number of pixels in temporary buffer
		imgBuffer->nTmpPixels += nPixels - pixelDataIdx;

	} while (TRUE);
			
	return 0;

CmtError:
	
Cmt_ERR
	
Error:
	
	// cleanup
	ReleaseDataPacket(&pixelPacket);
	OKfreeList(&pointJumpROIList, (DiscardFptr_type)discard_PointJump_type);
	OKfreeList(&frameScanROIList, (DiscardFptr_type)discard_Rect_type);
	OKfreeList(&ROIList, (DiscardFptr_type)discard_ROI_type);
	
	if (imgDisplayPtr) {
		CmtReleaseTSVPtr(imgBuffer->scanChan->imgDisplayTSV);
		imgBuffer->scanChan->imgDisplayTSVLineNumDebug = 0;
		imgDisplayPtr = NULL;
	}
	
RETURN_ERR
}

static PixelAssemblyBinding_type* init_PixelAssemblyBinding_type (RectRaster_type* rectRaster, size_t channelIdx)
{
	PixelAssemblyBinding_type*	binding = malloc(sizeof(PixelAssemblyBinding_type));
	if (!binding) return NULL;
	
	binding->scanEngine		= rectRaster;
	binding->channelIdx   	= channelIdx;
	
	return binding;
}

static void discard_PixelAssemblyBinding_type (PixelAssemblyBinding_type** pixelBindingPtr)
{
	OKfree(*pixelBindingPtr);	
}

static int CVICALLBACK NonResRectRasterScan_LaunchPixelBuilder (void* functionData)
{
INIT_ERR

	PixelAssemblyBinding_type*		binding 				= functionData;
	
	SetSleepPolicy(VAL_SLEEP_SOME);
	
	switch (binding->scanEngine->baseClass.scanMode) {
			
		case ScanEngineMode_FrameScan:
			
			// receive pixel stream and assemble image
			errChk( NonResRectRasterScan_BuildImage(binding->scanEngine, binding->channelIdx, &errorInfo.errMsg) );
			break;
			
		case ScanEngineMode_PointScan:
			
			errChk( NonResRectRasterScan_BuildPointScan(binding->scanEngine, binding->channelIdx, &errorInfo.errMsg) );
			break;
	}

	
	OKfree(binding);
	return 0;
	
Error:
	
	TaskControlIterationDone(binding->scanEngine->baseClass.taskControl, errorInfo.error, errorInfo.errMsg, FALSE, NULL);
	OKfree(errorInfo.errMsg);
	return errorInfo.error;
}

static int NonResRectRasterScan_AssembleCompositeImage (RectRaster_type* rectRaster, char** errorMsg)
{
INIT_ERR

	RGBA_type*		compositePixArray	= NULL;
	void*			chanPixArray		= NULL;
	
	// allocate memory for composite image
	nullChk( compositePixArray = malloc(rectRaster->scanSettings->height * rectRaster->scanSettings->width * sizeof(RGBA_type)) );
	
	// assign channels
	for (size_t chanIdx = 0; chanIdx < rectRaster->nImgBuffers; chanIdx++) {
		chanPixArray = GetImagePixelArray(rectRaster->imgBuffers[chanIdx]->image);
		switch(rectRaster->imgBuffers[chanIdx]->scanChan->color) {
				
			case ScanChanColor_Grey:
											 
				break;
				
			case ScanChanColor_Red:
				
				break;
				
			case ScanChanColor_Green:
				
				break;
				
			case ScanChanColor_Blue:
				
				break;
			
		}
	}
	
	
Error:
	
RETURN_ERR
	
}

static void NonResRectRasterScan_PointROIVoltage (RectRaster_type* rectRaster, Point_type* point, double* fastAxisCommandV, double* slowAxisCommandV)
{
	NonResGalvoCal_type*	fastAxisCal		= (NonResGalvoCal_type*) rectRaster->baseClass.fastAxisCal;
	NonResGalvoCal_type*	slowAxisCal		= (NonResGalvoCal_type*) rectRaster->baseClass.slowAxisCal;
	uInt32					nDeadTimePixels = (uInt32) ceil(fastAxisCal->triangleCal->deadTime * 1e3/rectRaster->scanSettings->pixelDwellTime);	// Number of pixel at the beginning and end of each line scan
																																				// in the fast axis scan direction that are dropped when assembling the image
	// Check if point is within the FOV:
	// point X coordinate is along the fast scan axis, while the Y coordinate is along the slow scan axis
	// X = 0, Y = 0 corresponds to the upper left corner of the image to which this point ROI belongs to
	
	*fastAxisCommandV = (nDeadTimePixels + point->x - (double)(rectRaster->scanSettings->width + 2 * nDeadTimePixels)/2.0 ) * rectRaster->scanSettings->pixSize / fastAxisCal->sampleDisplacement;
	*slowAxisCommandV = (point->y - (double)rectRaster->scanSettings->height/2.0) * rectRaster->scanSettings->pixSize / slowAxisCal->sampleDisplacement;	
	
}

static int NonResRectRasterScan_JumpTime (RectRaster_type* rectRaster, double fastAxisAmplitude, double slowAxisAmplitude, double* jumpTimePtr, double* responseDelayPtr)
{
INIT_ERR
	
	NonResGalvoCal_type*	fastAxisCal				= (NonResGalvoCal_type*) rectRaster->baseClass.fastAxisCal;
	NonResGalvoCal_type*	slowAxisCal				= (NonResGalvoCal_type*) rectRaster->baseClass.slowAxisCal;
	double					fastAxisSwitchTime		= 0;
	double					fastAxisSwitchDelay		= 0;
	double					slowAxisSwitchTime		= 0;
	double					slowAxisSwitchDelay		= 0;
	double					fastAxisJumpTime		= 0;
	double					slowAxisJumpTime		= 0;
	
	// fast axis jump time
	errChk( NonResGalvoPointJumpTime(fastAxisCal, fastAxisAmplitude, &fastAxisSwitchTime, &fastAxisSwitchDelay) );
	fastAxisJumpTime = fastAxisSwitchTime + fastAxisSwitchDelay; 
	// slow axis jump time
	errChk( NonResGalvoPointJumpTime(slowAxisCal, slowAxisAmplitude, &slowAxisSwitchTime, &slowAxisSwitchDelay) );
	slowAxisJumpTime = slowAxisSwitchTime + slowAxisSwitchDelay;
	
	// set jump time to be the largest jump time of the two scan axes (i.e. the jump time includes the switch delay and switch time)
	if (jumpTimePtr) {
		if (fastAxisJumpTime > slowAxisJumpTime)
			*jumpTimePtr = NonResRectRasterScan_RoundToGalvoSampling(rectRaster, fastAxisJumpTime);  
		else
			*jumpTimePtr = NonResRectRasterScan_RoundToGalvoSampling(rectRaster, slowAxisJumpTime);
	}
	
	// set switch delay to be the smallest of the two scan axes (i.e. during this time both galvos are within their initial position given a calibration resolution)
	if (responseDelayPtr) {
		if (fastAxisSwitchDelay < slowAxisSwitchDelay)
			*responseDelayPtr = NonResRectRasterScan_RoundToGalvoSampling(rectRaster, fastAxisSwitchDelay);
		else
			*responseDelayPtr = NonResRectRasterScan_RoundToGalvoSampling(rectRaster, slowAxisSwitchDelay);
	}
	
Error:	
	
	return errorInfo.error;
}

static size_t NonResRectRasterScan_GetNumActivePoints (RectRaster_type* rectRaster)
{
	size_t				nActivePoints	= 0;
	size_t 				nPoints 		= ListNumItems(rectRaster->pointScan.pointJumps);
	PointJump_type*		pointJump   	= NULL;
	
	for (size_t i = 1; i <= nPoints; i++) {
		pointJump = *(PointJump_type**) ListGetPtrToItem(rectRaster->pointScan.pointJumps, i);
		if (pointJump->point.baseClass.active) nActivePoints++;
	}
	
	return nActivePoints;
}

static int NonResRectRasterScan_BuildPointScan (RectRaster_type* rectRaster, size_t bufferIdx, char** errorMsg)
{
INIT_ERR
	
	RectRasterPointBuff_type*	pointBuffer					= rectRaster->pointBuffers[bufferIdx];
	int* 						nActivePixelBuildersTSVPtr 	= NULL;
	DSInfo_type*				dsInfo						= NULL;
	DataPacket_type*			dataPacket					= NULL;
	Waveform_type*				pixelWaveformCopy			= NULL;
	
	
	//-----------------------------------------------------------------------------------------------------------------------------------------------------
	// Receive pixel data
	//-----------------------------------------------------------------------------------------------------------------------------------------------------
	
	errChk( ReceiveWaveform(pointBuffer->scanChan->detVChan, &pointBuffer->rawPixels, NULL, &errorInfo.errMsg) );
	
	// if no waveform is received, send null packet to output channel and complete iteration if there are no other pixel builder threads for other channels
	if (!pointBuffer->rawPixels) {
		
		if (IsVChanOpen((VChan_type*)pointBuffer->scanChan->outputVChan)) {
			errChk( SendNullPacket(pointBuffer->scanChan->outputVChan, &errorInfo.errMsg) );
		}
		
		CmtErrChk( CmtGetTSVPtr(rectRaster->baseClass.nActivePixelBuildersTSV, &nActivePixelBuildersTSVPtr) );
		(*nActivePixelBuildersTSVPtr)--;
	
		// complete iteration
		if (!*nActivePixelBuildersTSVPtr) {
			errChk( TaskControlIterationDone(rectRaster->baseClass.taskControl, 0, "", FALSE, &errorInfo.errMsg) );
		}
		
		CmtErrChk( CmtReleaseTSVPtr(rectRaster->baseClass.nActivePixelBuildersTSV) );
		
		return 0;
	}
	
	//-----------------------------------------------------------------------------------------------------------------------------------------------------
	// Process waveform
	//-----------------------------------------------------------------------------------------------------------------------------------------------------
	
	size_t nHoldSamples = (size_t)RoundRealToNearestInteger(rectRaster->pointScan.pointScanProtocol->holdTime * 1e-3 * rectRaster->galvoSamplingRate);
	errChk( IntegrateWaveform(&pointBuffer->integratedPixels, pointBuffer->rawPixels, pointBuffer->nSkipPixels, pointBuffer->nSkipPixels + nHoldSamples, rectRaster->pointScan.pointScanProtocol->nIntegration, &errorInfo.errMsg) );
	discard_Waveform_type(&pointBuffer->rawPixels);
	
	//-----------------------------------------------------------------------------------------------------------------------------------------------------
	// Send processed waveform
	//-----------------------------------------------------------------------------------------------------------------------------------------------------
	
	if (IsVChanOpen((VChan_type*)pointBuffer->scanChan->outputVChan)) {
		nullChk( dsInfo = GetIteratorDSData(GetTaskControlIterator(rectRaster->baseClass.taskControl), WAVERANK) );
		errChk( CopyWaveform(&pixelWaveformCopy, pointBuffer->integratedPixels, &errorInfo.errMsg) );
		nullChk( dataPacket	= init_DataPacket_type(DL_Waveform_Double, (void**)&pixelWaveformCopy, &dsInfo, (DiscardFptr_type)discard_Waveform_type) );
		errChk( SendDataPacket(pointBuffer->scanChan->outputVChan, &dataPacket, FALSE, &errorInfo.errMsg) );
		errChk( SendNullPacket(pointBuffer->scanChan->outputVChan, &errorInfo.errMsg) );
	}
	
	//-----------------------------------------------------------------------------------------------------------------------------------------------------
	// Display waveform
	//-----------------------------------------------------------------------------------------------------------------------------------------------------
	
	// set waveform color to match channel color
	switch (pointBuffer->scanChan->color) {
			
		case ScanChanColor_Grey:
			SetWaveformColor(pointBuffer->integratedPixels, WaveformColor_DK_GRAY);
			break;
		case ScanChanColor_Red:
			SetWaveformColor(pointBuffer->integratedPixels, WaveformColor_DK_RED);
			break;
		case ScanChanColor_Green:
			SetWaveformColor(pointBuffer->integratedPixels, WaveformColor_DK_GREEN);
			break;
		case ScanChanColor_Blue:
			SetWaveformColor(pointBuffer->integratedPixels, WaveformColor_BLUE);
			break;
	}
	
	// display
	DisplayWaveform(pointBuffer->scanChan->waveDisplay, &pointBuffer->integratedPixels);
	
	
	//-----------------------------------------------------------------------------------------------------------------------------------------------------
	// Finalize point scan for this channel
	//-----------------------------------------------------------------------------------------------------------------------------------------------------
	
	CmtErrChk( CmtGetTSVPtr(rectRaster->baseClass.nActivePixelBuildersTSV, &nActivePixelBuildersTSVPtr) );
	
	(*nActivePixelBuildersTSVPtr)--;
	// complete iteration
	if (!*nActivePixelBuildersTSVPtr) {
		errChk( TaskControlIterationDone(rectRaster->baseClass.taskControl, 0, "", FALSE, &errorInfo.errMsg) );
	}
	
	CmtErrChk( CmtReleaseTSVPtr(rectRaster->baseClass.nActivePixelBuildersTSV) );
	
	return 0;

CmtError:
	
Cmt_ERR

Error:
	
	// cleanup
	discard_Waveform_type(&pixelWaveformCopy);
	discard_DSInfo_type(&dsInfo);
	discard_DataPacket_type(&dataPacket);
	
RETURN_ERR
}

static void NonResRectRasterScan_SetMinimumPointJumpStartDelay (RectRaster_type* rectRaster)
{
INIT_ERR

	PointScan_type*			pointScan			= &rectRaster->pointScan;
	double					fastAxisCommandV	= 0;
	double					slowAxisCommandV	= 0;
	NonResGalvoCal_type*	fastAxisCal			= (NonResGalvoCal_type*) rectRaster->baseClass.fastAxisCal;
	NonResGalvoCal_type*	slowAxisCal			= (NonResGalvoCal_type*) rectRaster->baseClass.slowAxisCal;
	size_t					nTotalPoints		= ListNumItems(pointScan->pointJumps);
	
	Point_type**			pointJumps			= NULL;	// Array of point jumps from the parked position to each point ROI in case of Single Point mode or to the first point ROI in case of Point Group mode.
	size_t					nPointJumps			= 0;	// Number of active points to jump to from pointJumps array.
	Point_type*				pointJump			= NULL;
	double					jumpTime			= 0;
	
	// get points to jump to from the parked position
	for (size_t i = 1; i <= nTotalPoints; i++) {
		pointJump = *(Point_type**)ListGetPtrToItem(pointScan->pointJumps, i);
		if (!pointJump->baseClass.active) continue; // select only active points
		nPointJumps++;
		pointJumps = realloc(pointJumps, nPointJumps * sizeof(Point_type*));
		pointJumps[nPointJumps - 1] = pointJump;
	}
	
	switch (rectRaster->pointScan.pointScanProtocol->jumpMethod) {
		
		case PointJump_SinglePoints:
			
			pointScan->minStartDelay = 0;
			for (size_t i = 0; i < nPointJumps; i++) {
				NonResRectRasterScan_PointROIVoltage(rectRaster, pointJumps[i], &fastAxisCommandV, &slowAxisCommandV);
				errChk( NonResRectRasterScan_JumpTime(rectRaster, fastAxisCal->parked - fastAxisCommandV, slowAxisCal->parked - slowAxisCommandV, &jumpTime, NULL) );
				if (jumpTime > pointScan->minStartDelay)
					pointScan->minStartDelay = jumpTime;
			}
			break;
			
		case PointJump_PointGroup:
		case PointJump_IncrementalPointGroup:
			
			if (nPointJumps) {
				NonResRectRasterScan_PointROIVoltage(rectRaster, pointJumps[0], &fastAxisCommandV, &slowAxisCommandV);
				errChk( NonResRectRasterScan_JumpTime(rectRaster, fastAxisCal->parked - fastAxisCommandV, slowAxisCal->parked - slowAxisCommandV, &jumpTime, NULL) );
				pointScan->minStartDelay = jumpTime;
			}
			break;
	}
	
	// update lower bound and coerce value
	if (pointScan->pointScanProtocol->startDelayInitVal <= pointScan->minStartDelay) {
		pointScan->pointScanProtocol->startDelayInitVal = pointScan->minStartDelay;
		SetCtrlVal(rectRaster->baseClass.pointScanPanHndl, PointTab_StartDelay, pointScan->pointScanProtocol->startDelayInitVal);
	}
	SetCtrlAttribute(rectRaster->baseClass.pointScanPanHndl, PointTab_StartDelay, ATTR_MIN_VALUE, pointScan->minStartDelay);  

	
Error:
	
	// cleanup
	OKfree(pointJumps);
	
}

/*
static void NonResRectRasterScan_SetMinimumHoldBurstPeriod (RectRaster_type* rectRaster)
{
INIT_ERR

	PointScan_type*			pointScan			= rectRaster->pointScan;
	Point_type**			pointJumps			= NULL;	// Array of point jumps from the parked position to each point ROI in case of Single Point mode or to the first point ROI in case of Point Group mode.
	size_t					nPointJumps			= 0;	// Number of active points to jump to from pointJumps array.
	size_t					nTotalPoints		= ListNumItems(pointScan->pointJumps);
	Point_type*				pointJump			= NULL;
	double					minBurstPeriod		= 0;
	
	
	// get points to jump to from the parked position
	for (size_t i = 1; i <= nTotalPoints; i++) {
		pointJump = *(Point_type**)ListGetPtrToItem(pointScan->pointJumps, i);
		if (!pointJump->baseClass.active) continue; // select only active points
		nPointJumps++;
		pointJumps = realloc(pointJumps, nPointJumps * sizeof(Point_type*));
		pointJumps[nPointJumps - 1] = pointJump;
	}

	switch (rectRaster->pointScan.jumpMethod) {
		
		case PointJump_SinglePoints:
			
			for (size_t i = 0; i < nPointJumps; i++) {
				
				
				
			}
			
			break;
			
		case PointJump_PointGroup:
		case PointJump_IncrementalPointGroup:
			
		
			break;
	}
	

Error:
	
	// cleanup
	OKfree(pointJumps);	
}
*/

/*
// Calculates the minimum time it takes to visit all the point ROIs starting from the parked position and ending up again in the parked position
static void NonResRectRasterScan_SetMinimumPointJumpPeriod (RectRaster_type* rectRaster)
{
	double					fastAxisCommandV	= 0;
	double					slowAxisCommandV	= 0;
	double					fastAxisCommandV1	= 0;
	double					slowAxisCommandV1	= 0;
	double					fastAxisCommandV2	= 0;
	double					slowAxisCommandV2	= 0;
	Point_type*				firstPointJump		= NULL;
	Point_type*				lastPointJump		= NULL;
	Point_type*				pointJump			= NULL;
	Point_type*				pointJump1			= NULL;
	Point_type*				pointJump2			= NULL;
	PointScan_type*			pointScan			= rectRaster->pointScan;
	size_t					nTotalPoints		= ListNumItems(pointScan->pointJumps);
	NonResGalvoCal_type*	fastAxisCal			= (NonResGalvoCal_type*) rectRaster->baseClass.fastAxisCal;
	NonResGalvoCal_type*	slowAxisCal			= (NonResGalvoCal_type*) rectRaster->baseClass.slowAxisCal;
	double					ROIsJumpTime		= 0; // Jump time from the first point ROI to the last point ROI including the parked times, in [ms].
	double					jumpTime			= 0; // Jump time between two point ROIs including the parked time spent at the forst point ROI, in [ms]
	
	pointScan->minSequencePeriod = 0;
	
	// get first point to jump to
	for (size_t i = 1; i <= nTotalPoints; i++) {
		pointJump = *(Point_type**)ListGetPtrToItem(pointScan->pointJumps, i);
		if (!pointJump->baseClass.active) continue; // select only the first active point jump
		firstPointJump = pointJump;
		break;
	}
	
	
	// if there are no points, skip calculations
	if (!firstPointJump) goto SkipPoints;
	
	// get last point to jump to
	pointJump = NULL;
	for (size_t i = nTotalPoints; i >= 1; i--) {
		pointJump = *(Point_type**)ListGetPtrToItem(pointScan->pointJumps, i);
		if (!pointJump->baseClass.active) continue; // select only the first from last active point jump
		lastPointJump = pointJump;
		break;
	}
	

	// calculate time to jump from parked position to the first Point ROI
	NonResRectRasterScan_PointROIVoltage(rectRaster, firstPointJump, &fastAxisCommandV, &slowAxisCommandV); 
	pointScan->minSequencePeriod += NonResRectRasterScan_JumpTime(rectRaster, fastAxisCal->parked - fastAxisCommandV, slowAxisCal->parked - slowAxisCommandV);
	// calculate time to jump from last Point ROI back to the parked position and add also the parked time
	NonResRectRasterScan_PointROIVoltage(rectRaster, lastPointJump, &fastAxisCommandV, &slowAxisCommandV); 
	pointScan->minSequencePeriod += NonResRectRasterScan_JumpTime(rectRaster, fastAxisCal->parked - fastAxisCommandV, slowAxisCal->parked - slowAxisCommandV) + rectRaster->pointParkedTime;
	
	// calculate time to jump between active Point ROIs
	for (size_t i = 1; i < nTotalPoints; i++) {
		pointJump1 = *(Point_type**)ListGetPtrToItem(rectRaster->pointJumps, i);
		if (!pointJump1->baseClass.active) continue; // select only active point jumps
		for (size_t j = i+1; j <= nTotalPoints; j++) {
			pointJump2 = *(Point_type**)ListGetPtrToItem(rectRaster->pointJumps, j);
			if (!pointJump2->baseClass.active) continue; // select only active point jump
			
			// calculate jump time and add also the parked time
			NonResRectRasterScan_PointROIVoltage(rectRaster, pointJump1, &fastAxisCommandV1, &slowAxisCommandV1);
			NonResRectRasterScan_PointROIVoltage(rectRaster, pointJump2, &fastAxisCommandV2, &slowAxisCommandV2); 
			jumpTime = NonResRectRasterScan_JumpTime(rectRaster, fastAxisCommandV1 - fastAxisCommandV2, slowAxisCommandV1 - slowAxisCommandV2) + rectRaster->pointParkedTime;
			rectRaster->minimumPointJumpPeriod += jumpTime;
			ROIsJumpTime += jumpTime;
			break; // get next pair
		}
	}
	
	ROIsJumpTime += rectRaster->pointParkedTime; // add the parked time of the last point, before jumping back to the parked position
	
SkipPoints:
	
	// update minimum point jump period
	rectRaster->pointJumpTime = ROIsJumpTime;
	SetCtrlVal(rectRaster->baseClass.pointScanPanHndl, PointTab_JumpTime, ROIsJumpTime); 
	// if period is larger or equal to previous value, then update lower bound 
	if (rectRaster->pointJumpPeriod <= rectRaster->minimumPointJumpPeriod) {
		rectRaster->pointJumpPeriod = rectRaster->minimumPointJumpPeriod;
		SetCtrlVal(rectRaster->baseClass.pointScanPanHndl, PointTab_Period, rectRaster->pointJumpPeriod);
	}
	SetCtrlAttribute(rectRaster->baseClass.pointScanPanHndl, PointTab_Period, ATTR_MIN_VALUE, rectRaster->minimumPointJumpPeriod);
	

}
*/ 

static double NonResRectRasterScan_RoundToGalvoSampling (RectRaster_type* scanEngine, double time)
{
	return ceil(time * 1e-3 * scanEngine->galvoSamplingRate) * 1e3/scanEngine->galvoSamplingRate;
}

static PointScanProtocol_type* init_PointScanProtocol_type (char protocolName[], double holdTime, uInt32 nHoldBurst, double holdBurstPeriod, double holdBurstPeriodIncr, double stimDelay,
														    double stimPulseONDuration, double stimPulseOFFDuration, uInt32 nStimPulses, PointJumpMethods jumpMethod, 
															BOOL record, BOOL stimulate, uInt32 nIntegration, uInt32 nSequenceRepeat, double startDelayInitVal,
															double startDelayIncrement, double repeatWait)
{
	PointScanProtocol_type* pointScanSet = malloc(sizeof(PointScanProtocol_type));
	if (!pointScanSet) return NULL;
	
	// jump settings
	pointScanSet->protocolName			= StrDup(protocolName);
	pointScanSet->jumpMethod			= jumpMethod;
	pointScanSet->record				= record;
	pointScanSet->stimulate				= stimulate;
	pointScanSet->nIntegration			= nIntegration;
	pointScanSet->nSequenceRepeat		= nSequenceRepeat;
	pointScanSet->startDelayInitVal		= startDelayInitVal;
	pointScanSet->startDelayIncrement	= startDelayIncrement;
	pointScanSet->repeatWait			= repeatWait;
	
	// hold settings
	pointScanSet->holdTime				= holdTime;
	pointScanSet->nHoldBurst			= nHoldBurst;
	pointScanSet->holdBurstPeriod		= holdBurstPeriod;
	pointScanSet->holdBurstPeriodIncr	= holdBurstPeriodIncr;
	
	// stimulation settings
	pointScanSet->stimDelay				= stimDelay;
	pointScanSet->stimPulseONDuration	= stimPulseONDuration;
	pointScanSet->stimPulseOFFDuration	= stimPulseOFFDuration;
	pointScanSet->nStimPulses			= nStimPulses;
	
	return pointScanSet;
}

static void discard_PointScanProtocol_type (PointScanProtocol_type** pointScanProtocolPtr)
{
	OKfree(*pointScanProtocolPtr);	
}

static PointScanProtocol_type* copy_PointScanProtocol_type (PointScanProtocol_type* pointScanProtocol)
{
	PointScanProtocol_type*		pointScanProtocolCopy = NULL;
	
	if (!pointScanProtocol) return NULL;
	
	pointScanProtocolCopy = malloc (sizeof(PointScanProtocol_type));
	if (!pointScanProtocolCopy) return NULL;
	
	// copy members that do not need memory allocation
	*pointScanProtocolCopy = *pointScanProtocol;
	// copy members that need memory allocation
	pointScanProtocolCopy->protocolName = StrDup(pointScanProtocol->protocolName);
	
	return pointScanProtocolCopy;
}

static int SetPointScanProtocolUI (RectRaster_type* rectRaster, BOOL updateProtocolsUIRing)
{
	// populate point jump modes
	InsertListItem(rectRaster->baseClass.pointScanPanHndl, PointTab_Mode, PointJump_SinglePoints, "Single points", PointJump_SinglePoints);
	InsertListItem(rectRaster->baseClass.pointScanPanHndl, PointTab_Mode, PointJump_PointGroup, "Point group", PointJump_PointGroup);
	InsertListItem(rectRaster->baseClass.pointScanPanHndl, PointTab_Mode, PointJump_IncrementalPointGroup, "Incremental point group", PointJump_IncrementalPointGroup);
	SetCtrlIndex(rectRaster->baseClass.pointScanPanHndl, PointTab_Mode, (int)rectRaster->pointScan.pointScanProtocol->jumpMethod);
	SetRectRasterScanEnginePointScanJumpModeUI(rectRaster->baseClass.pointScanPanHndl, rectRaster->pointScan.pointScanProtocol->jumpMethod);
				
	// recording UI settings
	SetCtrlVal(rectRaster->baseClass.pointScanPanHndl, PointTab_Record, rectRaster->pointScan.pointScanProtocol->record);
	SetRectRasterScanEnginePointScanRecordUI(rectRaster->baseClass.pointScanPanHndl, rectRaster->pointScan.pointScanProtocol->record);    
	
	//------------------------
	// stimulate UI settings
	
		// UI
	SetCtrlVal(rectRaster->baseClass.pointScanPanHndl, PointTab_Stimulate, rectRaster->pointScan.pointScanProtocol->stimulate);
	SetRectRasterScanEnginePointScanStimulateUI(rectRaster->baseClass.pointScanPanHndl, rectRaster->pointScan.pointScanProtocol->stimulate);
		// VChan
		// activate ROI shutter VChan
	if (rectRaster->pointScan.pointScanProtocol->stimulate)
		SetVChanActive((VChan_type*)rectRaster->baseClass.VChanROIShutter, TRUE);
	else
		SetVChanActive((VChan_type*)rectRaster->baseClass.VChanROIShutter, FALSE);
	//------------------------
	
	// set integration
	SetCtrlVal(rectRaster->baseClass.pointScanPanHndl, PointTab_NIntegration, rectRaster->pointScan.pointScanProtocol->nIntegration);

	// populate jump settings
	SetCtrlVal(rectRaster->baseClass.pointScanPanHndl, PointTab_Repeat, rectRaster->pointScan.pointScanProtocol->nSequenceRepeat);
	SetCtrlVal(rectRaster->baseClass.pointScanPanHndl, PointTab_RepeatWait, rectRaster->pointScan.pointScanProtocol->repeatWait);
	SetCtrlVal(rectRaster->baseClass.pointScanPanHndl, PointTab_StartDelay, rectRaster->pointScan.pointScanProtocol->startDelayInitVal);
	SetCtrlVal(rectRaster->baseClass.pointScanPanHndl, PointTab_StartDelayIncrement, rectRaster->pointScan.pointScanProtocol->startDelayIncrement);
	
	// populate hold settings
	SetCtrlVal(rectRaster->baseClass.pointScanPanHndl, PointTab_NHoldBurst, rectRaster->pointScan.pointScanProtocol->nHoldBurst);
	SetCtrlVal(rectRaster->baseClass.pointScanPanHndl, PointTab_Hold, rectRaster->pointScan.pointScanProtocol->holdTime);
	SetCtrlVal(rectRaster->baseClass.pointScanPanHndl, PointTab_HoldBurstPeriod, rectRaster->pointScan.pointScanProtocol->holdBurstPeriod);
	SetCtrlVal(rectRaster->baseClass.pointScanPanHndl, PointTab_HoldBurstPeriodIncr, rectRaster->pointScan.pointScanProtocol->holdBurstPeriodIncr);
	
	// populate stimulation settings
	SetCtrlVal(rectRaster->baseClass.pointScanPanHndl, PointTab_StimDelay, rectRaster->pointScan.pointScanProtocol->stimDelay);
	SetCtrlVal(rectRaster->baseClass.pointScanPanHndl, PointTab_NPulses, rectRaster->pointScan.pointScanProtocol->nStimPulses);
	SetCtrlVal(rectRaster->baseClass.pointScanPanHndl, PointTab_PulseON, rectRaster->pointScan.pointScanProtocol->stimPulseONDuration);
	SetCtrlVal(rectRaster->baseClass.pointScanPanHndl, PointTab_PulseOFF, rectRaster->pointScan.pointScanProtocol->stimPulseOFFDuration);
	
	if (updateProtocolsUIRing) {
		// select point scan protocol
		size_t						nProtocols			= ListNumItems(rectRaster->pointScan.pointScanProtocols);
		PointScanProtocol_type*		pointScanProtocol	= NULL;
	
		ClearListCtrl(rectRaster->baseClass.pointScanPanHndl, PointTab_Protocol);
		InsertListItem(rectRaster->baseClass.pointScanPanHndl, PointTab_Protocol, 0, "", 0);
		SetCtrlIndex(rectRaster->baseClass.pointScanPanHndl, PointTab_Protocol, 0);
	
		if (nProtocols)
			SetCtrlAttribute(rectRaster->baseClass.pointScanPanHndl, PointTab_Protocol, ATTR_DIMMED, 0);
		else
			SetCtrlAttribute(rectRaster->baseClass.pointScanPanHndl, PointTab_Protocol, ATTR_DIMMED, 1);
	
		for (size_t i = 1; i <= nProtocols; i++) {
			pointScanProtocol = *(PointScanProtocol_type**)ListGetPtrToItem(rectRaster->pointScan.pointScanProtocols, i);
			InsertListItem(rectRaster->baseClass.pointScanPanHndl, PointTab_Protocol, i, pointScanProtocol->protocolName, i);
			// select protocol
			if (!strcmp(pointScanProtocol->protocolName, rectRaster->pointScan.pointScanProtocol->protocolName))
			SetCtrlIndex(rectRaster->baseClass.pointScanPanHndl, PointTab_Protocol, i);
		}
	}
	
	// dim/undim delete and save buttons
	if (ListNumItems(rectRaster->pointScan.pointScanProtocols)) {
		SetCtrlAttribute(rectRaster->baseClass.pointScanPanHndl, PointTab_DelProtocol, ATTR_DIMMED, 0);
		SetCtrlAttribute(rectRaster->baseClass.pointScanPanHndl, PointTab_SaveProtocol, ATTR_DIMMED, 0);
	} else {
		SetCtrlAttribute(rectRaster->baseClass.pointScanPanHndl, PointTab_DelProtocol, ATTR_DIMMED, 1);
		SetCtrlAttribute(rectRaster->baseClass.pointScanPanHndl, PointTab_SaveProtocol, ATTR_DIMMED, 1);
	}
	
	if (ListNumItems(rectRaster->pointScan.pointJumps))
		SetPanelAttribute(rectRaster->baseClass.pointScanPanHndl, ATTR_DIMMED, FALSE);
	else
		// dim point scan panel since there are no points initially
		SetPanelAttribute(rectRaster->baseClass.pointScanPanHndl, ATTR_DIMMED, TRUE);
	
	return 0;
}

static BOOL	VerifyPointScanProtocolName (char inputStr[], void* callbackData)
{
	PointScan_type*				pointScan 			= (PointScan_type*) callbackData;
	PointScanProtocol_type*		pointScanProtocol	= NULL;
	size_t						nPointScanSettings	= ListNumItems(pointScan->pointScanProtocols);
	BOOL						nameExists			= FALSE;
	
	for (size_t i = 1; i <= nPointScanSettings; i++) {
		pointScanProtocol = *(PointScanProtocol_type**) ListGetPtrToItem(pointScan->pointScanProtocols, i);
		if (!strcmp(inputStr, pointScanProtocol->protocolName)) {
			nameExists = TRUE;
			break;
		}
	}
	
	return !nameExists;
}

static PointJump_type* init_PointJump_type (Point_type* point)
{
	PointJump_type*	pointJump = malloc(sizeof(PointJump_type));
	if (!pointJump) return NULL;
	
	// init parent
	
	initalloc_Point_type(&pointJump->point, point->baseClass.ROIName, point->baseClass.rgba, point->baseClass.active, point->x, point->y);
	
	// init child DATA
	
	// override methods
	pointJump->point.baseClass.discardFptr 	= (DiscardFptr_type)discard_PointJump_type;
	pointJump->point.baseClass.copyFptr		= (CopyFptr_type)copy_PointJump_type;
	
	return pointJump;
}

static void	discard_PointJump_type (PointJump_type** pointJumpPtr)
{
	PointJump_type*	pointJump = *pointJumpPtr;
	if (!pointJump) return;
	
	// discard child
	
	// discard parent
	discard_Point_type((Point_type**)pointJumpPtr);
}

static PointJump_type* copy_PointJump_type (PointJump_type* pointJump)
{
	PointJump_type*	pointJumpCopy = init_PointJump_type(&pointJump->point);
	if (!pointJumpCopy) return NULL;
	
	
	return pointJumpCopy;
}

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
INIT_ERR

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
			if (engine->scanPanHndl) {
				// if there were no objectives previously, then assign this FL by default
				if (!nObjectives)
					engine->objectiveLens = objective;
				
				// update both scan axes
				if (engine->fastAxisCal)
						(*engine->fastAxisCal->UpdateOptics) (engine->fastAxisCal);
					
				if (engine->slowAxisCal)
						(*engine->slowAxisCal->UpdateOptics) (engine->slowAxisCal);
				
				// inserts new objective and selects it if there were no other objectives previously
				InsertListItem(engine->frameScanPanHndl, ScanTab_Objective, -1, newObjectiveName, objectiveFL);
			}
			OKfree(newObjectiveName);
			
			// configure/unconfigure scan engine
			if (NonResRectRasterScan_ValidConfig((RectRaster_type*)engine)) {
				
				errChk( NonResRectRasterScan_ScanWidths((RectRaster_type*)engine, &errorInfo.errMsg) );
				errChk( NonResRectRasterScan_PixelDwellTimes((RectRaster_type*)engine, &errorInfo.errMsg) );
					
				if (NonResRectRasterScan_ReadyToScan((RectRaster_type*)engine))
					errChk( TaskControlEvent(engine->taskControl, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
				else
					errChk( TaskControlEvent(engine->taskControl, TC_Event_Unconfigure, NULL, NULL, &errorInfo.errMsg) ); 	
			} else
				errChk( TaskControlEvent(engine->taskControl, TC_Event_Unconfigure, NULL, NULL, &errorInfo.errMsg) );
			
			// cleanup, discard panel
			DiscardPanel(engine->newObjectivePanHndl);
			engine->newObjectivePanHndl = 0;
			break;
			
		case NewObjPan_Cancel:
			
			DiscardPanel(engine->newObjectivePanHndl);
			engine->newObjectivePanHndl = 0;
			break;
	}
	
Error:
	
PRINT_ERR

	return 0;
}

/*
Generates a staircase waveform like this:

 	      start				              end
 		   :   endV -->     _5_		       :	
 		   :            _4_|   |   delay   : 
  		   :        _3_|	   |<--------->:
    	   v    _2_|	       |		   :
 startV --> _1_|		       |___________v

sampleRate 			= Sampling rate in [Hz].
nSamplesPerStep		= Number of samples per staircase step.
nSteps				= Number of steps in the staircase, including the first step at startV. If nStepts is 0, then only the delay samples at startV are included
startVoltage		= Start voltage at the bottom of the staircase [V].
stepVoltage    		= Increase in voltage with each step in [V], except for the symmetric staircase where at the peak are two consecutive lines at the same voltage.
delaySamples		= Number of samples to be added after the last sample from the staircase that return the voltage to startV.
*/
static Waveform_type* StaircaseWaveform (double sampleRate, size_t nSamplesPerStep, size_t nSteps, size_t delaySamples, double startVoltage, double stepVoltage)
{
	double*		waveformData	= NULL;
	size_t		nSamples		= nSteps * nSamplesPerStep + delaySamples;    
		
    waveformData = malloc(nSamples * sizeof(double));
    if (!waveformData) goto Error;

    // build staircase
    for (size_t i = 0; i < nSteps; i++)
        for (size_t j = 0; j < nSamplesPerStep; j++)
                waveformData[i*nSamplesPerStep+j] = startVoltage + stepVoltage*i;
	
	Set1D(waveformData + nSteps * nSamplesPerStep, delaySamples, startVoltage); 
    
    return init_Waveform_type(Waveform_Double, sampleRate, nSamples, (void**)&waveformData);
	
Error:
	
	OKfree(waveformData);
	
	return NULL;
	
}

//-----------------------------------------
// Scan Engine VChan management
//-----------------------------------------

// Fast Axis Command VChan
static void	FastAxisComVChan_StateChange (VChan_type* self, void* VChanOwner, VChanStates state)
{ 
INIT_ERR
	
	ScanEngine_type*	engine		= VChanOwner;
	
	switch (state) {
		
		case VChan_Open:
			
			if (!engine->fastAxisCal) return; // no parked voltage available
			
			errChk( SendScanAxisCommand(engine, ScanAxis_Fast, ((NonResGalvoCal_type*)engine->fastAxisCal)->parked, &errorInfo.errMsg) );
			break;
			
		case VChan_Closed:
			
			break;
	}
	
	return;
	
Error:
	
PRINT_ERR
}

// Slow Axis Command VChan
static void	SlowAxisComVChan_StateChange (VChan_type* self, void* VChanOwner, VChanStates state)
{
INIT_ERR
	
	ScanEngine_type*	engine			= VChanOwner;
	double*				parkedVPtr 		= NULL;
	DataPacket_type*	dataPacket		= NULL;
	
	switch (state) {
		
		case VChan_Open:
			
			if (!engine->slowAxisCal) return; // no parked voltage available
	
			nullChk( parkedVPtr = malloc(sizeof(double)) );
			*parkedVPtr = ((NonResGalvoCal_type*)engine->slowAxisCal)->parked;
			nullChk( dataPacket = init_DataPacket_type(DL_Double, (void**)&parkedVPtr, NULL, NULL) );
			errChk( SendDataPacket(engine->VChanSlowAxisCom, &dataPacket, FALSE, &errorInfo.errMsg) );
			
			break;
			
		case VChan_Closed:
			
			break;
	}
	
	return;
	
Error:
	
	// cleanup
	OKfree(parkedVPtr);
	discard_DataPacket_type(&dataPacket);
	
PRINT_ERR
}

static void	GalvoComSampleRateVChan_StateChange (VChan_type* self, void* VChanOwner, VChanStates state)
{
INIT_ERR

	RectRaster_type*	engine				= VChanOwner;
	double*				samplingRatePtr 	= NULL;
	DataPacket_type*	dataPacket			= NULL;
	
	switch (state) {
		
		case VChan_Open:
			
			nullChk( samplingRatePtr = malloc(sizeof(double)) );
			*samplingRatePtr = engine->galvoSamplingRate;
			nullChk( dataPacket = init_DataPacket_type(DL_Double, (void**)&samplingRatePtr, NULL, NULL) );
			errChk( SendDataPacket(engine->baseClass.VChanGalvoComSamplingRate, &dataPacket, FALSE, &errorInfo.errMsg) );
			
			break;
			
		case VChan_Closed:
			
			break;
	}
	
	return;
	
Error:
	
	// cleanup
	OKfree(samplingRatePtr);
	discard_DataPacket_type(&dataPacket);
	
PRINT_ERR
}


// Detection channel
static void DetectionVChan_StateChange (VChan_type* self, void* VChanOwner, VChanStates state)
{
	ScanChan_type* 	scanChan = VChanOwner;
	switch (state) {
			
		case VChan_Closed:
			
			SetVChanActive((VChan_type*)scanChan->outputVChan, FALSE);
			
			break;
			
		case VChan_Open:
			
			SetVChanActive((VChan_type*)scanChan->outputVChan, TRUE);
			
			break;
	}
}

static void SetRectRasterScanEngineModeVChans (RectRaster_type* scanEngine)
{

	switch (scanEngine->baseClass.scanMode) {
							
		case ScanEngineMode_FrameScan:
							
			// modify scan engine detection and output channels
			for (uInt32 i = 0; i < scanEngine->baseClass.nScanChans; i++) {
				
				// activate detection channels
				SetVChanActive((VChan_type*)scanEngine->baseClass.scanChans[i]->detVChan, TRUE);
				
				/*
				// activate scan engine channel output VChan if the corresponding detection channel is open
				if (IsVChanOpen((VChan_type*)scanEngine->baseClass.scanChans[i]->detVChan))
					SetVChanActive((VChan_type*)scanEngine->baseClass.scanChans[i]->outputVChan, TRUE);
				else
					SetVChanActive((VChan_type*)scanEngine->baseClass.scanChans[i]->outputVChan, FALSE);
				*/
				
				// change scan engine channel output VChan data type to DL_Image
				SetSourceVChanDataType(scanEngine->baseClass.scanChans[i]->outputVChan, DL_Image);
			}
				
			// activate composite image VChan
			SetVChanActive((VChan_type*)scanEngine->baseClass.VChanCompositeImage, TRUE);
			
			// active N Pixels VChan
			SetVChanActive((VChan_type*)scanEngine->baseClass.VChanNPixels, TRUE);
			
			// active pixel sampling rate VChan
			SetVChanActive((VChan_type*)scanEngine->baseClass.VChanPixelSamplingRate, TRUE);
			
			// active pixel pulse train VChan
			SetVChanActive((VChan_type*)scanEngine->baseClass.VChanPixelPulseTrain, TRUE);
			
			// inactivate ROI hold VChan
			SetVChanActive((VChan_type*)scanEngine->baseClass.VChanROIHold, FALSE);
			
			// activate ROI shutter VChan
			SetVChanActive((VChan_type*)scanEngine->baseClass.VChanROIShutter, TRUE);
			
			break;
							
		case ScanEngineMode_PointScan:
			
			// modify scan engine detection and output channels
			for (uInt32 i = 0; i < scanEngine->baseClass.nScanChans; i++) {
				
				// activate/inactivate detection channels if recording is desired
				if (scanEngine->pointScan.pointScanProtocol->record)
					SetVChanActive((VChan_type*)scanEngine->baseClass.scanChans[i]->detVChan, TRUE);
				else
					SetVChanActive((VChan_type*)scanEngine->baseClass.scanChans[i]->detVChan, FALSE);
					
				// change scan engine channel output VChan data type to match the detection channel data type and activate if the corresponding detection channel is open
				if (IsVChanOpen((VChan_type*)scanEngine->baseClass.scanChans[i]->detVChan)) {
					SetSourceVChanDataType(scanEngine->baseClass.scanChans[i]->outputVChan, GetSourceVChanDataType(GetSourceVChan(scanEngine->baseClass.scanChans[i]->detVChan)));
					SetVChanActive((VChan_type*)scanEngine->baseClass.scanChans[i]->outputVChan, TRUE);
				} else
					SetVChanActive((VChan_type*)scanEngine->baseClass.scanChans[i]->outputVChan, FALSE);
					
			}
			
			// inactivate composite image VChan
			SetVChanActive((VChan_type*)scanEngine->baseClass.VChanCompositeImage, FALSE);
			
			if (scanEngine->pointScan.pointScanProtocol->record) { 
				// N Pixels VChan
				SetVChanActive((VChan_type*)scanEngine->baseClass.VChanNPixels, TRUE);
				// pixel sampling rate VChan
				SetVChanActive((VChan_type*)scanEngine->baseClass.VChanPixelSamplingRate, TRUE);
				// pixel pulse train VChan
				SetVChanActive((VChan_type*)scanEngine->baseClass.VChanPixelPulseTrain, TRUE);
			} else {
				// N Pixels VChan
				SetVChanActive((VChan_type*)scanEngine->baseClass.VChanNPixels, FALSE);
				// pixel sampling rate VChan
				SetVChanActive((VChan_type*)scanEngine->baseClass.VChanPixelSamplingRate, FALSE);
				// pixel pulse train VChan
				SetVChanActive((VChan_type*)scanEngine->baseClass.VChanPixelPulseTrain, FALSE);
			}
			
			// activate ROI hold VChan
			SetVChanActive((VChan_type*)scanEngine->baseClass.VChanROIHold, TRUE);
			
			// activate ROI shutter VChan
			if (scanEngine->pointScan.pointScanProtocol->stimulate)
				SetVChanActive((VChan_type*)scanEngine->baseClass.VChanROIShutter, TRUE);
			else
				SetVChanActive((VChan_type*)scanEngine->baseClass.VChanROIShutter, FALSE);
			
			break;
	}
}

static uInt32 GetNumOpenDetectionVChans	(RectRaster_type* scanEngine)
{
	uInt32	nOpenChans = 0;
	
	for (uInt32 i = 0; i < scanEngine->baseClass.nScanChans; i++)
		if (IsVChanOpen((VChan_type*)scanEngine->baseClass.scanChans[i]->detVChan))
			nOpenChans++;
	
	return nOpenChans;
}

//-----------------------------------------
// Display interface
//-----------------------------------------

static void ImageDisplay_CB (ImageDisplay_type* imgDisplay, int event, void* eventData, void* callbackFunctionData)
{
INIT_ERR

	RectRasterDisplayCBData_type*	displayCBData			= callbackFunctionData;
	RectRaster_type*				scanEngine 				= displayCBData->scanEngine;
	RectRasterScanSet_type*			previousScanSettings	= displayCBData->scanSettings;
	ScanChan_type*					scanChan				= scanEngine->baseClass.scanChans[displayCBData->scanChanIdx];
	ImageDisplay_type**				imgDisplayPtr			= NULL;
				
	switch (event) {
			
		case ImageDisplay_Close:
				
			// detach image display from scan engine if it was previously assigned to it
			if (scanEngine->baseClass.activeDisplay == imgDisplay)
				scanEngine->baseClass.activeDisplay = NULL;
			
			CmtErrChk( CmtGetTSVPtr(scanChan->imgDisplayTSV, &imgDisplayPtr) ); scanChan->imgDisplayTSVLineNumDebug = __LINE__;
			
			*imgDisplayPtr = NULL; // display is discarded by Display module
			CmtErrChk( CmtReleaseTSVPtr(scanChan->imgDisplayTSV) );
			scanChan->imgDisplayTSVLineNumDebug = 0;
			imgDisplayPtr = NULL;
			
			break;
			
		case ImageDisplay_RestoreSettings:
			
			RestoreScanSettingsFromImageDisplay(imgDisplay, scanEngine, previousScanSettings);
			break;
			
		case ImageDisplay_ROI_Placed:
			{
				ROI_type*			ROI = eventData;
				
				switch (ROI->ROIType) {
				
					case ROI_Point:
					
						break;
				
					case ROI_Rectangle:
						
						break;
				}
				
			}
			break;
			
		case ImageDisplay_ROI_Added:
			{
				ROI_type*		ROI				= eventData;
				
				switch ( ROI->ROIType) {
						
					case ROI_Point:
						
						// update scan engine if this display is assigned to it
						if (scanEngine->baseClass.activeDisplay == imgDisplay) {
							// insert point ROI item in the UI
							int	nListItems = 0;
							GetNumListItems(scanEngine->baseClass.pointScanPanHndl, PointTab_ROIs, &nListItems);
							InsertListItem(scanEngine->baseClass.pointScanPanHndl, PointTab_ROIs, -1, ROI->ROIName, nListItems+1);
						
							// mark point as checked (in use)
							CheckListItem(scanEngine->baseClass.pointScanPanHndl, PointTab_ROIs, nListItems, 1); 
							// insert ROI item in the scan engine as well
							ROI_type*	ROICopy = (*ROI->copyFptr) (ROI);
							ListInsertItem(scanEngine->pointScan.pointJumps, &ROICopy, END_OF_LIST);
						
							// calculate the minimum initial delay, display it and set this as a lower bound in the UI
							NonResRectRasterScan_SetMinimumPointJumpStartDelay(scanEngine);
						
							// update minimum point jump period
							//NonResRectRasterScan_SetMinimumPointJumpPeriod(scanEngine);
						
							// undim panel
							SetPanelAttribute(scanEngine->baseClass.pointScanPanHndl, ATTR_DIMMED, 0);
						}
					
						break;
						
					case ROI_Rectangle:
						
						// update scan engine if this display is assigned to it
						if (scanEngine->baseClass.activeDisplay == imgDisplay) {
							
							// insert rectangle ROI item in the UI
							int	nListItems = 0;
							GetNumListItems(scanEngine->baseClass.frameScanPanHndl, ScanTab_ROIs, &nListItems);
							InsertListItem(scanEngine->baseClass.frameScanPanHndl, ScanTab_ROIs, -1, ROI->ROIName, nListItems+1);
							
							// mark rectangle as checked (visible)
							CheckListItem(scanEngine->baseClass.frameScanPanHndl, ScanTab_ROIs, nListItems, 1); 
							
							// insert ROI item in the scan engine as well
							ROI_type*	ROICopy = (*ROI->copyFptr) (ROI);
							ListInsertItem(scanEngine->scanROIs, &ROICopy, END_OF_LIST);
						
						}
						
						break;
					
					default:
						
					break;
			}
				
				
			}
			break;
			
		case ImageDisplay_ROI_Removed:
			
			break;
	}
	
	
	
CmtError:
	
Cmt_ERR

Error:

PRINT_ERR
}

static void WaveformDisplay_CB (WaveformDisplay_type* waveformDisplay, int event, void* callbackData)
{
	ScanChan_type*	scanChan = callbackData;
	
	switch (event) {
			
		case WaveformDisplay_Close:
			
			DiscardWaveforms(scanChan->waveDisplay);
			
			break;
	}
}

static void RestoreScanSettingsFromImageDisplay (ImageDisplay_type* imgDisplay, RectRaster_type* scanEngine, RectRasterScanSet_type* previousScanSettings)
{
INIT_ERR

	// scan engine point ROI list 
	ListType			pointROIList	= scanEngine->pointScan.pointJumps;
	size_t				nPointROIs 		= ListNumItems(pointROIList);
	PointJump_type**	pointJumpPtr	= NULL;
	
	// scan engine rectangle ROI list
	ListType			rectROIList		= scanEngine->scanROIs;
	size_t				nRectROIs 		= ListNumItems(rectROIList);
	Rect_type**			rectPtr			= NULL;
	
	// image ROI list
	ListType			imageROIList	= GetImageROIs(imgDisplay->image);
	size_t 				nImageROIs		= ListNumItems(imageROIList);
	ROI_type*   		ROI				= NULL;
	ROI_type*   		ROICopy			= NULL;
	
	
	// assign this image display to the scan engine
	scanEngine->baseClass.activeDisplay	= imgDisplay; 
	
	//----------------------------------------------------------------------------------------------------------------------------------------------------
	// Set UI: Clear point jump ROIs
	//----------------------------------------------------------------------------------------------------------------------------------------------------
	
	// clear ROIs in the scan engine UI
	ClearListCtrl(scanEngine->baseClass.pointScanPanHndl, PointTab_ROIs);
	
	// clear ROIs from the scan engine
	for (size_t i = 1; i <= nPointROIs; i++) {
		pointJumpPtr = ListGetPtrToItem(pointROIList, i);
		discard_PointJump_type(pointJumpPtr);
	}
	ListClear(pointROIList);
	
	//----------------------------------------------------------------------------------------------------------------------------------------------------
	// Set UI: Clear frame scan ROIs and add parent image ROI
	//----------------------------------------------------------------------------------------------------------------------------------------------------
	
	// clear ROIs in the scan engine UI
	ClearListCtrl(scanEngine->baseClass.frameScanPanHndl, ScanTab_ROIs);
	
	// clear ROIs from the scan engine list
	for (size_t i = 1; i <= nRectROIs; i++) {
		rectPtr = ListGetPtrToItem(rectROIList, i);
		discard_Rect_type(rectPtr);
	}
	ListClear(rectROIList);
	
	//----------------------------------------------------------------------------------------------------------------------------------------------------
	// Set UI: Restore point jump and frame scan ROIs
	//----------------------------------------------------------------------------------------------------------------------------------------------------
	
	nPointROIs			= 0;
	nRectROIs			= 0;
	
	for (size_t i = 1; i <= nImageROIs; i++) {
		ROI = *(ROI_type**)ListGetPtrToItem(imageROIList, i);
		ROICopy = (*ROI->copyFptr) ((void*)ROI);
		
		switch (ROI->ROIType) {
						
			case ROI_Point:
				
				nPointROIs++;
				// insert ROI in point jump UI list
				InsertListItem(scanEngine->baseClass.pointScanPanHndl, PointTab_ROIs, -1, ROI->ROIName, nPointROIs); // UI list item index matches always scanEngine->pointScan.pointJumps list index.
				// mark point as checked/unchecked
				CheckListItem(scanEngine->baseClass.pointScanPanHndl, PointTab_ROIs, nPointROIs - 1, ROI->active); 
				// insert ROI item in the scan engine as well
				ListInsertItem(scanEngine->pointScan.pointJumps, &ROICopy, END_OF_LIST);
				break;
				
			case ROI_Rectangle:
				
				nRectROIs++;
				// insert ROI in frame scan UI list
				InsertListItem(scanEngine->baseClass.frameScanPanHndl, ScanTab_ROIs, -1, ROI->ROIName, nRectROIs); // UI list item index matches always scanEngine->pointScan.pointJumps list index.
				// mark point as checked/unchecked (ROI visible or not)
				CheckListItem(scanEngine->baseClass.frameScanPanHndl, ScanTab_ROIs, nRectROIs - 1, ROI->active); 
				// insert ROI item in the scan engine as well
				ListInsertItem(scanEngine->scanROIs, &ROICopy, END_OF_LIST);
				break;
					
			default:
					
				break;
		}
	}
	
	// select parent ROI in frame scan list
	SetCtrlIndex(scanEngine->baseClass.frameScanPanHndl, ScanTab_ROIs, 0);
	
	//----------------------------------------------------------------------------------------------------------------------------------------------------
	// Restore scan settings
	//----------------------------------------------------------------------------------------------------------------------------------------------------

	// assign previous scan settings
	discard_RectRasterScanSet_type(&scanEngine->scanSettings);
	nullChk( scanEngine->scanSettings = init_RectRasterScanSet_type(previousScanSettings->pixSize, previousScanSettings->height, previousScanSettings->heightOffset, previousScanSettings->width, previousScanSettings->widthOffset, previousScanSettings->pixelDwellTime) );
	discard_RectRasterScanSet_type(&scanEngine->parentFrameScanSettings);
	nullChk( scanEngine->parentFrameScanSettings = init_RectRasterScanSet_type(previousScanSettings->pixSize, previousScanSettings->height, previousScanSettings->heightOffset, previousScanSettings->width, previousScanSettings->widthOffset, previousScanSettings->pixelDwellTime) );
	
	// update preferred widths and pixel dwell times
	errChk( NonResRectRasterScan_ScanWidths(scanEngine, &errorInfo.errMsg) );
	errChk( NonResRectRasterScan_PixelDwellTimes(scanEngine, &errorInfo.errMsg) );
	
	// update remaining controls on the scan panel
	SetCtrlVal(scanEngine->baseClass.frameScanPanHndl, ScanTab_Height, scanEngine->scanSettings->height * scanEngine->scanSettings->pixSize);
	SetCtrlVal(scanEngine->baseClass.frameScanPanHndl, ScanTab_HeightOffset, scanEngine->scanSettings->heightOffset * scanEngine->scanSettings->pixSize);
	SetCtrlVal(scanEngine->baseClass.frameScanPanHndl, ScanTab_WidthOffset, scanEngine->scanSettings->widthOffset * scanEngine->scanSettings->pixSize);
	SetCtrlVal(scanEngine->baseClass.frameScanPanHndl, ScanTab_PixelSize, scanEngine->scanSettings->pixSize);
	
	// update jump start delay and minimum jump period
	NonResRectRasterScan_SetMinimumPointJumpStartDelay(scanEngine);
	//NonResRectRasterScan_SetMinimumPointJumpPeriod(scanEngine); 
	
	if (nPointROIs)
		SetPanelAttribute(scanEngine->baseClass.pointScanPanHndl, ATTR_DIMMED, FALSE);
	else
		SetPanelAttribute(scanEngine->baseClass.pointScanPanHndl, ATTR_DIMMED, TRUE);
	
	
Error:

PRINT_ERR
}

//---------------------------------------------------------------------
// Non Resonant Galvo Calibration and Testing Task Controller Callbacks 
//---------------------------------------------------------------------

static int ConfigureTC_NonResGalvoCal (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
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

static int UncofigureTC_NonResGalvoCal (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	//ActiveNonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);  
	
	return 0;
}

static void IterateTC_NonResGalvoCal (TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortIterationFlag)
{
#define	IterateTC_NonResGalvoCal_Err_DifferentCommandAndResponseNumSamples 		-1
#define IterateTC_NonResGalvoCal_Err_SwitchTimeShorterThanGalvoLag		 		-2			

INIT_ERR

	ActiveNonResGalvoCal_type* 		cal 						= GetTaskControlModuleData(taskControl);
	double*							commandSignal				= NULL;
	double*							positionSignal				= NULL;
	DataPacket_type*				commandPacket				= NULL;
	uInt64*							nCommandWaveformSamplesPtr  = NULL;
	size_t							nCommandSignalSamples		= 0;
	size_t							nPositionSignalSamples		= 0;
	Waveform_type*					waveformCopy				= NULL;
	WaveformTypes					waveformType				= 0;
	DSInfo_type*					dsInfo						= NULL;
	
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
			errChk( CopyWaveform(&waveformCopy, cal->commandWaveform, &errorInfo.errMsg) );
			nullChk( dsInfo = GetIteratorDSData(iterator, WAVERANK) );
			nullChk( commandPacket = init_DataPacket_type(DL_Waveform_Double, (void**)&waveformCopy, &dsInfo, (DiscardFptr_type)discard_Waveform_type) );
			errChk( SendDataPacket(cal->baseClass.VChanCom, &commandPacket, 0, &errorInfo.errMsg) );
			errChk( SendNullPacket(cal->baseClass.VChanCom, &errorInfo.errMsg) );
			
			// send number of samples in waveform
			nullChk( nCommandWaveformSamplesPtr = malloc (sizeof(uInt64)) );
			*nCommandWaveformSamplesPtr = (uInt64) nCommandSignalSamples;
			
			nullChk( dsInfo = GetIteratorDSData(iterator, WAVERANK) );
			nullChk( commandPacket = init_DataPacket_type(DL_UInt64, (void**)&nCommandWaveformSamplesPtr, &dsInfo, NULL) );
			errChk( SendDataPacket(cal->baseClass.VChanComNSamples, &commandPacket, 0, &errorInfo.errMsg) );
			
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			// Receive and analyze galvo response signal
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			
			errChk( ReceiveWaveform(cal->baseClass.VChanPos, &cal->positionWaveform, &waveformType, &errorInfo.errMsg) );
			
			//get pointer to galvo position signal
			positionSignal	 	= *(double**) GetWaveformPtrToData (cal->positionWaveform, &nPositionSignalSamples); 
			commandSignal		= *(double**) GetWaveformPtrToData (cal->commandWaveform, &nCommandSignalSamples); 
			
			// check if number of received samples is different than the number of sent samples
			if (nPositionSignalSamples != nCommandSignalSamples)
				SET_ERR(IterateTC_NonResGalvoCal_Err_DifferentCommandAndResponseNumSamples, "Number of command and response samples in the sent and received waveforms must be the same.");
			
			// analyze galvo response
			double  	Pos[CALPOINTS]				= {0};
			double  	Comm[CALPOINTS]				= {0};
			double		PosStdDev[CALPOINTS]		= {0};
			double  	LinFitResult[CALPOINTS]		= {0};
			double  	meanSquaredError			= 0;
			
			// analyze galvo response  
			for (size_t i = 0; i < CALPOINTS; i++) {
				// get average position and signal StdDev
				StdDev(positionSignal + i*nSamplesPerCalPoint + nDelaySamples, POSSAMPLES, &Pos[i], &PosStdDev[i]);
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
			errChk( TaskControlIterationDone(taskControl, 0, "", TRUE, &errorInfo.errMsg) );
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
			errChk( CopyWaveform(&waveformCopy, cal->commandWaveform, &errorInfo.errMsg) );
			nullChk( dsInfo = GetIteratorDSData(iterator, WAVERANK) );
			nullChk( commandPacket = init_DataPacket_type(DL_Waveform_Double, (void**)&waveformCopy, &dsInfo, (DiscardFptr_type)discard_Waveform_type) );
			errChk( SendDataPacket(cal->baseClass.VChanCom, &commandPacket, 0, &errorInfo.errMsg) );
			errChk( SendNullPacket(cal->baseClass.VChanCom, &errorInfo.errMsg) );
			
			// send number of samples in waveform
			nCommandWaveformSamplesPtr = malloc (sizeof(uInt64));
			*nCommandWaveformSamplesPtr = (uInt64)nCommandSignalSamples;
			nullChk( dsInfo = GetIteratorDSData(iterator, WAVERANK) );
			nullChk( commandPacket = init_DataPacket_type(DL_UInt64, (void**)&nCommandWaveformSamplesPtr, &dsInfo, NULL) );
			errChk( SendDataPacket(cal->baseClass.VChanComNSamples, &commandPacket, 0, &errorInfo.errMsg) );
			
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			// Receive and analyze galvo response signal
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			
			errChk( ReceiveWaveform(cal->baseClass.VChanPos, &cal->positionWaveform, &waveformType, &errorInfo.errMsg) );
			
			//get pointer to galvo position signal
			positionSignal 	= *(double**)GetWaveformPtrToData (cal->positionWaveform, &nPositionSignalSamples); 
			commandSignal	= *(double**)GetWaveformPtrToData (cal->commandWaveform, &nCommandSignalSamples); 
			
			// check if number of received samples is different than the number of sent samples
			if (nPositionSignalSamples != nCommandSignalSamples)
				SET_ERR(IterateTC_NonResGalvoCal_Err_DifferentCommandAndResponseNumSamples, "Number of command and response samples in the sent and received waveforms must be the same.");
			
			// average galvo ramp responses, use only ramp and post ramp time to analyze the data
			double*		averageResponse = calloc (cal->nRampSamples + postRampSamples, sizeof(double));
				
			// sum up ramp responses
			for (size_t i = 0; i < cal->nRepeat; i++)
				for (size_t j = 0; j < cal->nRampSamples + postRampSamples; j++)
					averageResponse[j] += positionSignal[i * (flybackSamples + cal->nRampSamples + postRampSamples) + flybackSamples + j];
				
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
			PlotWaveform(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, commandSignal + flybackSamples, cal->nRampSamples + postRampSamples, VAL_DOUBLE, 1.0, 0, 0, 1000/ *cal->baseClass.comSampRate, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, VAL_BLUE);
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
				*cal->lag = MeasureLag(commandSignal + flybackSamples, averageResponse, cal->nRampSamples + postRampSamples) * 1000/ *cal->baseClass.comSampRate; // response lag in [ms]
				// update results
				int calibPanHndl;
				GetPanelHandleFromTabPage(cal->baseClass.calPanHndl, NonResGCal_Tab, 0, &calibPanHndl);
				
				// lag
				SetCtrlVal(calibPanHndl, Cal_ResponseLag, *cal->lag * 1e3);   // display in [us]
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
			errChk( TaskControlIterationDone(taskControl, 0, "", TRUE, &errorInfo.errMsg) );
			
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
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_LABEL_TEXT, "Switch time vs. ramp amplitude");   
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_XNAME, "Ramp amplitude (V)");
				SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, ATTR_YNAME, "Time (µs)");
			}
			
			size_t 	postStepSamples = (size_t) floor(POSTRAMPTIME * 0.001 * *cal->baseClass.comSampRate) + 1;
			size_t 	flybackSamples 	= (size_t) floor(MAXFLYBACKTIME * 0.001 * *cal->baseClass.comSampRate) + 1; 
			double	amplitudeFactor = pow(SWITCH_TIMES_AMP_ITER_FACTOR, (double)cal->currIterIdx);
			
			cal->nRepeat = (size_t)pow(ceil(*cal->posStdDev*MINSNR/cal->resolution), 2);
			
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
			errChk( CopyWaveform(&waveformCopy, cal->commandWaveform, &errorInfo.errMsg) );
			nullChk( dsInfo = GetIteratorDSData(iterator, WAVERANK) );
			nullChk( commandPacket = init_DataPacket_type(DL_Waveform_Double, (void**)&waveformCopy, &dsInfo, (DiscardFptr_type)discard_Waveform_type) );
			errChk( SendDataPacket(cal->baseClass.VChanCom, &commandPacket, 0, &errorInfo.errMsg) );
			errChk( SendNullPacket(cal->baseClass.VChanCom, &errorInfo.errMsg) );
			
			// send number of samples in waveform
			nullChk( nCommandWaveformSamplesPtr = malloc (sizeof(uInt64)) );
			*nCommandWaveformSamplesPtr = (uInt64)nCommandSignalSamples;
			nullChk( dsInfo = GetIteratorDSData(iterator, WAVERANK) );
			nullChk( commandPacket = init_DataPacket_type(DL_UInt64, (void**)&nCommandWaveformSamplesPtr, &dsInfo, NULL) );
			errChk( SendDataPacket(cal->baseClass.VChanComNSamples, &commandPacket, 0, &errorInfo.errMsg) );
			
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			// Receive and analyze galvo response signal
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			
			errChk( ReceiveWaveform(cal->baseClass.VChanPos, &cal->positionWaveform, &waveformType, &errorInfo.errMsg) );
			
			//get pointer to galvo position signal
			positionSignal 	= *(double**)GetWaveformPtrToData (cal->positionWaveform, &nPositionSignalSamples); 
			commandSignal	= *(double**)GetWaveformPtrToData (cal->commandWaveform, &nCommandSignalSamples); 
			
			// check if number of received samples is different than the number of sent samples
			if (nPositionSignalSamples != nCommandSignalSamples)
				SET_ERR(IterateTC_NonResGalvoCal_Err_DifferentCommandAndResponseNumSamples, "Number of command and response samples in the sent and received waveforms must be the same.");
			
			double*		averageResponse = calloc (postStepSamples, sizeof(double));
			// sum up step responses
			for (size_t i = 0; i < cal->nRepeat; i++)
				for (size_t j = 0; j < postStepSamples; j++)
					averageResponse[j] += positionSignal[i * (flybackSamples + postStepSamples) + flybackSamples + j];
			// average ramp responses
			for (size_t i = 0; i < postStepSamples; i++) averageResponse[i] /= cal->nRepeat;
				
			// calculate corrected position signal based on scaling and offset
			for (size_t i = 0; i < postStepSamples; i++) averageResponse[i] = *cal->slope * averageResponse[i] + *cal->offset;
			
			cal->switchTimes->n++;
			cal->switchTimes->stepSize = realloc (cal->switchTimes->stepSize, cal->switchTimes->n * sizeof(double));
			cal->switchTimes->stepSize[cal->switchTimes->n - 1] = 2*cal->commandVMax * amplitudeFactor;
			cal->switchTimes->switchTime = realloc (cal->switchTimes->switchTime, cal->switchTimes->n * sizeof(double));
			cal->switchTimes->delay = realloc (cal->switchTimes->delay, cal->switchTimes->n * sizeof(double));
			
			errChk( GetStepTime(averageResponse, postStepSamples, *cal->baseClass.comSampRate, -cal->commandVMax * amplitudeFactor, cal->commandVMax * amplitudeFactor, cal->resolution, 
								&cal->switchTimes->switchTime[cal->switchTimes->n - 1], &cal->switchTimes->delay[cal->switchTimes->n - 1], &errorInfo.errMsg) );
			
			// plot switching time
			double x, y;
			PlotPoint(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, 2 * cal->commandVMax * amplitudeFactor, cal->switchTimes->switchTime[cal->switchTimes->n - 1]  * 1000 , VAL_ASTERISK, VAL_BLUE);
			GetGraphCursor(cal->baseClass.calPanHndl, NonResGCal_GalvoPlot, 1, &x, &y);
			SetCtrlVal(cal->baseClass.calPanHndl, NonResGCal_CursorX, x);
			SetCtrlVal(cal->baseClass.calPanHndl, NonResGCal_CursorY, y);
			
			
			if (2 * cal->commandVMax * amplitudeFactor >= cal->minStepSize * SWITCH_TIMES_AMP_ITER_FACTOR)
			  cal->currIterIdx++;
			else {
					
				cal->currIterIdx = 0;
				cal->currentCal++;
			}
				
			OKfree(averageResponse);
			errChk( TaskControlIterationDone(taskControl, 0, "", TRUE, &errorInfo.errMsg) );  
			
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
			errChk( CopyWaveform(&waveformCopy, cal->commandWaveform, &errorInfo.errMsg) );
			nullChk( dsInfo = GetIteratorDSData(iterator, WAVERANK) );
			nullChk( commandPacket = init_DataPacket_type(DL_Waveform_Double, (void**)&waveformCopy, &dsInfo, (DiscardFptr_type)discard_Waveform_type) );
			errChk( SendDataPacket(cal->baseClass.VChanCom, &commandPacket, 0, &errorInfo.errMsg) );
			errChk( SendNullPacket(cal->baseClass.VChanCom, &errorInfo.errMsg) );
			
			// send number of samples in waveform
			nullChk( nCommandWaveformSamplesPtr = malloc (sizeof(uInt64)) );
			*nCommandWaveformSamplesPtr = (uInt64)nCommandSignalSamples;
			nullChk( dsInfo = GetIteratorDSData(iterator, WAVERANK) );
			nullChk( commandPacket = init_DataPacket_type(DL_UInt64, (void**)&nCommandWaveformSamplesPtr, &dsInfo, NULL) );
			errChk( SendDataPacket(cal->baseClass.VChanComNSamples, &commandPacket, 0, &errorInfo.errMsg) );
			
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			// Receive and analyze galvo response signal
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			
			errChk( ReceiveWaveform(cal->baseClass.VChanPos, &cal->positionWaveform, &waveformType, &errorInfo.errMsg) );
			
			//get pointer to galvo position signal
			positionSignal	= *(double**)GetWaveformPtrToData (cal->positionWaveform, &nPositionSignalSamples); 
			commandSignal	= *(double**)GetWaveformPtrToData (cal->commandWaveform, &nCommandSignalSamples); 
			
			// check if number of received samples is different than the number of sent samples
			if (nPositionSignalSamples != nCommandSignalSamples)
				SET_ERR(IterateTC_NonResGalvoCal_Err_DifferentCommandAndResponseNumSamples, "Number of command and response samples in the sent and received waveforms must be the same.");
			
			double*		averageResponse = calloc (cal->nRampSamples + postRampSamples, sizeof(double));
				
			// sum up ramp responses
			for (size_t i = 0; i < cal->nRepeat; i++)
				for (size_t j = 0; j < cal->nRampSamples + postRampSamples; j++)
					averageResponse[j] += positionSignal[i * (flybackSamples + cal->nRampSamples + postRampSamples) + flybackSamples + j];
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
			errChk( TaskControlIterationDone(taskControl, 0, "", TRUE, &errorInfo.errMsg) ); 
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
			errChk( CopyWaveform(&waveformCopy, cal->commandWaveform, &errorInfo.errMsg) );
			nullChk( dsInfo = GetIteratorDSData(iterator, WAVERANK) );
			nullChk( commandPacket = init_DataPacket_type(DL_Waveform_Double, (void**)&waveformCopy, &dsInfo, (DiscardFptr_type)discard_Waveform_type) );
			errChk( SendDataPacket(cal->baseClass.VChanCom, &commandPacket, 0, &errorInfo.errMsg) );
			errChk( SendNullPacket(cal->baseClass.VChanCom, &errorInfo.errMsg) );
			
			// send number of samples in waveform
			nullChk( nCommandWaveformSamplesPtr = malloc (sizeof(uInt64)) );
			*nCommandWaveformSamplesPtr = (uInt64)nCommandSignalSamples;
			nullChk( dsInfo = GetIteratorDSData(iterator, WAVERANK) );
			nullChk( commandPacket = init_DataPacket_type(DL_UInt64, (void**)&nCommandWaveformSamplesPtr, &dsInfo, NULL) );
			errChk( SendDataPacket(cal->baseClass.VChanComNSamples, &commandPacket, 0, &errorInfo.errMsg) );
			
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			// Receive and analyze galvo response signal
			//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			
			errChk( ReceiveWaveform(cal->baseClass.VChanPos, &cal->positionWaveform, &waveformType, &errorInfo.errMsg) );
			
			//get pointer to galvo position signal
			positionSignal 	= *(double**)GetWaveformPtrToData (cal->positionWaveform, &nPositionSignalSamples); 
			commandSignal	= *(double**)GetWaveformPtrToData (cal->commandWaveform, &nCommandSignalSamples); 
			
			// check if number of received samples is different than the number of sent samples
			if (nPositionSignalSamples != nCommandSignalSamples)
				SET_ERR(IterateTC_NonResGalvoCal_Err_DifferentCommandAndResponseNumSamples, "Number of command and response samples in the sent and received waveforms must be the same.");
			
			BOOL		overloadFlag	= FALSE;
			double* 	averageResponse = NULL;
				
			// check if there is overload
			for (size_t i = 0; i < nSamples; i++) 
				if ((positionSignal[i] < - funcAmp/2 * 1.5 - 10 * *cal->posStdDev) || (positionSignal[i] > funcAmp/2 * 1.5 + 10 * *cal->posStdDev)) {
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
					cal->triangleCal->commandAmp[cal->triangleCal->n - 1] = cal->maxSlopes->amplitude[cal->triangleCal->n - 1];
					cal->triangleCal->maxFreq = realloc(cal->triangleCal->maxFreq, cal->triangleCal->n * sizeof(double));   
					cal->triangleCal->maxFreq[cal->triangleCal->n - 1] = maxSlope * 1000/(2 * funcAmp);  // maximum triangle function frequency! 
					cal->triangleCal->resLag = realloc(cal->triangleCal->resLag, cal->triangleCal->n * sizeof(double));  
					cal->triangleCal->resLag[cal->triangleCal->n - 1] = MeasureLag(commandSignal, averageResponse, cycleSamples) * 1000/ *cal->baseClass.comSampRate;
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
				errChk( TaskControlIterationDone(taskControl, 0, "", TRUE, &errorInfo.errMsg) ); 
			}
			else {
				// calculate average dead time
				cal->triangleCal->deadTime /= cal->maxSlopes->n;
				// calculate actual triangle waveform amplitude
				cal->triangleCal->actualAmp = malloc(cal->maxSlopes->n * sizeof(double));
				for (size_t i = 0; i < cal->maxSlopes->n; i++)
					cal->triangleCal->actualAmp[i] = cal->triangleCal->commandAmp[i] * (1 - 4 * cal->triangleCal->maxFreq[i] * cal->triangleCal->deadTime * 0.001);  
					
				// end task controller iterations
				errChk( TaskControlIterationDone(taskControl, 0, "", FALSE, &errorInfo.errMsg) );
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
	
	TaskControlIterationDone(cal->baseClass.taskController, errorInfo.error, errorInfo.errMsg, FALSE, NULL);
	OKfree(errorInfo.errMsg);
}

static int StartTC_NonResGalvoCal (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
#define StartTC_NonResGalvoCal_Err_InvalidParameter		-1
	
	ActiveNonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	// dim Save calibration
	SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_SaveCalib, ATTR_DIMMED, 1);  
	
	// check if settings are correct
	if (cal->commandVMax == 0.0) {
		*errorMsg = StrDup("Maximum command voltage cannot be 0");
		return StartTC_NonResGalvoCal_Err_InvalidParameter;
	}
	
	if (cal->minStepSize == 0.0) {
		*errorMsg = StrDup("Minimum step size voltage cannot be 0");
		return StartTC_NonResGalvoCal_Err_InvalidParameter;
	}
		
	if (cal->resolution == 0.0) {
		*errorMsg = StrDup("Minimum resolution voltage cannot be 0");
		return StartTC_NonResGalvoCal_Err_InvalidParameter;
	}
		
	if (cal->mechanicalResponse == 0.0) {
		*errorMsg = StrDup("Mechanical response cannot be 0");
		return StartTC_NonResGalvoCal_Err_InvalidParameter; 
	}
	
	if (cal->scanTime == 0.0) {
		*errorMsg = StrDup("Scan time cannot be 0");
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

static int ResetTC_NonResGalvoCal (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
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

static int DoneTC_NonResGalvoCal (TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg)
{
	ActiveNonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	// calibration complete, undim Save calibration
	SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_SaveCalib, ATTR_DIMMED, 0);
	
	// cleanup
	discard_Waveform_type(&cal->positionWaveform);
	
	return 0; 
}

static int StoppedTC_NonResGalvoCal (TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg)
{
	ActiveNonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	// dim Save calibration since calibration is incomplete
	SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_SaveCalib, ATTR_DIMMED, 1);
	
	return 0; 
}

static int TaskTreeStateChange_NonResGalvoCal (TaskControl_type* taskControl, TaskTreeStates state, char** errorMsg)
{
	ActiveNonResGalvoCal_type* 	cal 	= GetTaskControlModuleData(taskControl);
	
	// dim/undim controls
	SetCtrlAttribute(cal->baseClass.calPanHndl, NonResGCal_Done, ATTR_DIMMED, (int) state);
    int calSetPanHndl;
	GetPanelHandleFromTabPage(cal->baseClass.calPanHndl, NonResGCal_Tab, 0, &calSetPanHndl);
	SetCtrlAttribute(calSetPanHndl, Cal_CommMaxV, ATTR_DIMMED, (int) state); 
	SetCtrlAttribute(calSetPanHndl, Cal_ParkedV, ATTR_DIMMED, (int) state); 
	SetCtrlAttribute(calSetPanHndl, Cal_ScanTime, ATTR_DIMMED, (int) state);
	SetCtrlAttribute(calSetPanHndl, Cal_MinStep, ATTR_DIMMED, (int) state); 
	SetCtrlAttribute(calSetPanHndl, Cal_Resolution, ATTR_DIMMED, (int) state);
	SetCtrlAttribute(calSetPanHndl, Cal_MechanicalResponse, ATTR_DIMMED, (int) state);
	
	return 0;
}

//-----------------------------------------
// LaserScanning Task Controller Callbacks
//-----------------------------------------

static int ConfigureTC_RectRaster (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	RectRaster_type* engine = GetTaskControlModuleData(taskControl);
	
	SetRectRasterTaskControllerSettings(engine);
	
	return 0; 
}

static int UnconfigureTC_RectRaster (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	//RectRaster_type* engine = GetTaskControlModuleData(taskControl);
	
	return 0; 
}

static void	IterateTC_RectRaster (TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortIterationFlag)
{
INIT_ERR

	RectRaster_type* 				engine 						= GetTaskControlModuleData(taskControl);
	int*							nActivePixelBuildersTSVPtr	= NULL;
	PixelAssemblyBinding_type*		pixelBinding				= NULL;
	size_t							currentSequenceRepeat		= 0;
	
	switch(engine->baseClass.scanMode) {
		
		case ScanEngineMode_FrameScan:
			
			// if there are no detection channels assigned, then do nothing  
			if (!engine->imgBuffers) {
				errChk( TaskControlIterationDone(taskControl, 0, 0, FALSE, &errorInfo.errMsg) );
				break;
			}
			
			// reset composite image counter to be equal to the number of image channel buffers in use
			CmtErrChk( CmtGetTSVPtr(engine->baseClass.nActivePixelBuildersTSV, &nActivePixelBuildersTSVPtr) );
			*nActivePixelBuildersTSVPtr = engine->nImgBuffers;
			CmtErrChk( CmtReleaseTSVPtr(engine->baseClass.nActivePixelBuildersTSV) );
			
			// launch image assembly threads for each channel
			for (size_t i = 0; i < engine->nImgBuffers; i++) {
				nullChk( pixelBinding= init_PixelAssemblyBinding_type(engine, i) );
				CmtErrChk( CmtScheduleThreadPoolFunctionAdv(DLGetCommonThreadPoolHndl(), NonResRectRasterScan_LaunchPixelBuilder, pixelBinding, DEFAULT_THREAD_PRIORITY, NULL, 
									 	 (EVENT_TP_THREAD_FUNCTION_BEGIN | EVENT_TP_THREAD_FUNCTION_END), pixelBinding, RUN_IN_SCHEDULED_THREAD, NULL) );	
			}
			
			break;
			
		case ScanEngineMode_PointScan:
			
			// reset point scan buffers
			for (size_t i = 0; i < engine->nPointBuffers; i++)
				ResetRectRasterPointBuffer(engine->pointBuffers[i]);
			
			// set current active point and sequence
			switch (engine->pointScan.pointScanProtocol->jumpMethod) {
							
				case PointJump_SinglePoints:
					
					// update current active point
					engine->pointScan.currentActivePoint = GetCurrentIterIndex(iterator)/engine->pointScan.pointScanProtocol->nSequenceRepeat + 1;
					
					currentSequenceRepeat = GetCurrentIterIndex(iterator)%engine->pointScan.pointScanProtocol->nSequenceRepeat;
					
					break;
							
				case PointJump_PointGroup:
							
					currentSequenceRepeat = GetCurrentIterIndex(iterator);
					
					break;
				
				case PointJump_IncrementalPointGroup:
					
					// update number of points in group
					engine->pointScan.nPointsInGroup = GetCurrentIterIndex(iterator)/engine->pointScan.pointScanProtocol->nSequenceRepeat + 1;
					
					currentSequenceRepeat = GetCurrentIterIndex(iterator)%engine->pointScan.pointScanProtocol->nSequenceRepeat; 
					
					break;
			}
			
			// display current repeat
			SetCtrlVal(engine->baseClass.pointScanPanHndl, PointTab_NRepeat, (uInt32)currentSequenceRepeat);
			
			// update start delay
			engine->pointScan.startDelay = currentSequenceRepeat * engine->pointScan.pointScanProtocol->startDelayIncrement + engine->pointScan.pointScanProtocol->startDelayInitVal;
			
			// send galvo and ROI timing waveforms
			errChk ( NonResRectRasterScan_GeneratePointJumpSignals (engine, &errorInfo.errMsg) );  
			
			// if there are no open detection VChans or no points to jump to
			if (!GetNumOpenDetectionVChans(engine)) {
				errChk( TaskControlIterationDone(taskControl, 0, 0, FALSE, &errorInfo.errMsg) );
				break;
			}
			
			// reset pixel builder counter
			CmtErrChk( CmtGetTSVPtr(engine->baseClass.nActivePixelBuildersTSV, &nActivePixelBuildersTSVPtr) );
			*nActivePixelBuildersTSVPtr = engine->nPointBuffers;
			CmtErrChk( CmtReleaseTSVPtr(engine->baseClass.nActivePixelBuildersTSV) );
			
			// launch point scan threads for each channel
			for (size_t i = 0; i < engine->nPointBuffers; i++) {
				nullChk( pixelBinding = init_PixelAssemblyBinding_type(engine, i) );
				CmtErrChk( CmtScheduleThreadPoolFunctionAdv(DLGetCommonThreadPoolHndl(), NonResRectRasterScan_LaunchPixelBuilder, pixelBinding, DEFAULT_THREAD_PRIORITY, NULL, 
									 	 (EVENT_TP_THREAD_FUNCTION_BEGIN | EVENT_TP_THREAD_FUNCTION_END), pixelBinding, RUN_IN_SCHEDULED_THREAD, NULL) );	
			}
			
			break;
	}
	
	// update iterations
	SetCtrlVal(engine->baseClass.frameScanPanHndl, ScanTab_FramesAcquired, (unsigned int) GetCurrentIterIndex(iterator) );
	
	return;

CmtError:
	
Cmt_ERR

Error:
	
	SetCtrlVal(engine->baseClass.frameScanPanHndl, ScanTab_FramesAcquired, (unsigned int) GetCurrentIterIndex(iterator) );
	
	TaskControlIterationDone(taskControl, errorInfo.error, errorInfo.errMsg, FALSE, NULL);
	OKfree(errorInfo.errMsg);
}

static int StartTC_RectRaster (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
INIT_ERR

	RectRaster_type* 		engine 				= GetTaskControlModuleData(taskControl);
	
	// reset image scan buffers
	for (size_t i = 0; i < engine->nImgBuffers; i++)
		ResetRectRasterImgBuffer(engine->imgBuffers[i], FALSE);
	
	switch(engine->baseClass.scanMode) {
		
		case ScanEngineMode_FrameScan:
			
			// send galvo waveforms
			errChk ( NonResRectRasterScan_GenerateScanSignals (engine, &errorInfo.errMsg) );
			break;
			
		case ScanEngineMode_PointScan:
			
			// reset current active point in case single point jump mode is used
			engine->pointScan.currentActivePoint = 0;
		
			// reset current repeat display
			SetCtrlVal(engine->baseClass.pointScanPanHndl, PointTab_NRepeat, 0);
			break;
	}
	
Error:
	
RETURN_ERR
}

static int DoneTC_RectRaster (TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg)
{
INIT_ERR

	RectRaster_type* 	engine		= GetTaskControlModuleData(taskControl);

	
	switch(engine->baseClass.scanMode) {
		
		case ScanEngineMode_FrameScan:
			
			// return to parked position if continuous
			if (GetTaskControlMode(engine->baseClass.taskControl) == TASK_CONTINUOUS) {
				errChk( ReturnRectRasterToParkedPosition(engine, &errorInfo.errMsg) );
			}
			
			// update iterations
			SetCtrlVal(engine->baseClass.frameScanPanHndl, ScanTab_FramesAcquired, (unsigned int) GetCurrentIterIndex(iterator) );
			break;
			
		case ScanEngineMode_PointScan:
			
			// update sequence repeats
			// reset current repeat display
			SetCtrlVal(engine->baseClass.pointScanPanHndl, PointTab_NRepeat, engine->pointScan.pointScanProtocol->nSequenceRepeat);
			break;
	}
	
	// flush leftover elements from the incoming detection pixel stream
	for (size_t i = 0; i < engine->baseClass.nScanChans; i++)
		errChk( ReleaseAllDataPackets(engine->baseClass.scanChans[i]->detVChan, &errorInfo.errMsg) );
	
Error:
	
RETURN_ERR
}

static int StoppedTC_RectRaster (TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg)
{
INIT_ERR

	RectRaster_type* 			engine 						= GetTaskControlModuleData(taskControl);
	
	// return to parked position
	errChk( ReturnRectRasterToParkedPosition(engine, &errorInfo.errMsg) );
	
	
Error:
	
RETURN_ERR
}

static int ResetTC_RectRaster (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
INIT_ERR

	RectRaster_type* 	engine 		= GetTaskControlModuleData(taskControl);
	
	// reset acquired frames
	SetCtrlVal(engine->baseClass.frameScanPanHndl, ScanTab_FramesAcquired, 0);
	
Error:
	
RETURN_ERR
}

static int TaskTreeStateChange_RectRaster (TaskControl_type* taskControl, TaskTreeStates state, char** errorMsg)
{
#define TaskTreeStateChange_RectRaster_Err_WrongPixelDataType		-1
INIT_ERR

	RectRaster_type* 		engine 				= GetTaskControlModuleData(taskControl);
	size_t					nScanChans 			= engine->baseClass.nScanChans;
	SourceVChan_type*   	detSourceVChan		= NULL;
	ImageTypes				imageType			= 0;
	DLDataTypes				pixelDataType		= 0;
	int 					activeTabIdx 		= 0;
	ImageDisplay_type**		imgDisplayPtr		= NULL;
	CmtTSVHandle			displayTSVHndl		= 0;
	
	GetActiveTabPage(engine->baseClass.scanPanHndl, RectRaster_Tab, &activeTabIdx);
	engine->baseClass.scanMode = (ScanEngineModes) activeTabIdx;
					
	// activate/inactivate VChans
	SetRectRasterScanEngineModeVChans(engine);
	// change task controller settings 
	SetRectRasterTaskControllerSettings(engine);
	
	switch (state) {
			
		case TaskTree_Started:
			
			switch (engine->baseClass.scanMode) {
					
				case ScanEngineMode_FrameScan:
					
					//--------------------------------------------------------------------------------------------------------
					// Discard image assembly buffers
					//--------------------------------------------------------------------------------------------------------
			
					for (size_t i = 0; i < engine->nImgBuffers; i++)
						discard_RectRasterImgBuff_type(&engine->imgBuffers[i]);
			
					OKfree(engine->imgBuffers);
					engine->nImgBuffers = 0;
			
					//--------------------------------------------------------------------------------------------------------
					// Initialize image assembly buffers and obtain display handles
					//--------------------------------------------------------------------------------------------------------
	
					// create new image assembly buffers for connected detector VChans
					for (size_t i = 0; i < nScanChans; i++) {
						if (!IsVChanOpen((VChan_type*)engine->baseClass.scanChans[i]->detVChan)) continue;	// select only open detection channels
		
						detSourceVChan = GetSourceVChan(engine->baseClass.scanChans[i]->detVChan);
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
						
							case DL_Waveform_UInt:
								imageType = Image_UInt;    
								break;
						
							case DL_Waveform_Float:
								imageType = Image_Float;     
								break;
			
							default:
						
								SET_ERR(TaskTreeStateChange_RectRaster_Err_WrongPixelDataType, "Wrong pixel data type.");
						}
						
						#ifdef __ImageDisplayNIVision_H__ 
						
							// display image in new window
							imgDisplayPtr = NULL;
							displayTSVHndl = engine->baseClass.scanChans[i]->imgDisplayTSV;
							errChk( CmtGetTSVPtr(engine->baseClass.scanChans[i]->imgDisplayTSV, &imgDisplayPtr) ); engine->baseClass.scanChans[i]->imgDisplayTSVLineNumDebug = __LINE__;
							nullChk( *imgDisplayPtr = (ImageDisplay_type*)init_ImageDisplayNIVision_type (engine->baseClass.scanChans[i], 0, imageType, engine->scanSettings->width, engine->scanSettings->height, NULL) );
							errChk( CmtReleaseTSVPtr(engine->baseClass.scanChans[i]->imgDisplayTSV) );
							engine->baseClass.scanChans[i]->imgDisplayTSVLineNumDebug = 0;
							imgDisplayPtr = NULL;
						#endif
						
						engine->nImgBuffers++;
						// allocate memory for image assembly
						nullChk( engine->imgBuffers = realloc(engine->imgBuffers, engine->nImgBuffers * sizeof(RectRasterImgBuff_type*)) );
						nullChk( engine->imgBuffers[engine->nImgBuffers - 1] = init_RectRasterImgBuff_type(engine->baseClass.scanChans[i], engine->scanSettings->height, engine->scanSettings->width, pixelDataType, FALSE) ); 
						
					}
			
					// Taken out for now, but must be put back if composite image display is implemented
					// get handle to display composite image if there is at least one open channel
					//if (engine->nImgBuffers)
					//	nullChk( engine->baseClass.compositeImgDisplay = (*displayEngine->getImageDisplayFptr) (displayEngine, engine, engine->scanSettings->height, engine->scanSettings->width, Image_RGBA) ); 
			
					break;
					
				case ScanEngineMode_PointScan:
					
					//--------------------------------------------------------------------------------------------------------
					// Discard point scan buffers
					//--------------------------------------------------------------------------------------------------------
			
					for (size_t i = 0; i < engine->nPointBuffers; i++)
						discard_RectRasterPointBuff_type(&engine->pointBuffers[i]);
			
					OKfree(engine->pointBuffers);
					engine->nPointBuffers = 0;
					
					//--------------------------------------------------------------------------------------------------------
					// Initialize point scan buffers
					//--------------------------------------------------------------------------------------------------------
					
					int	workspacePanHndl = 0;
					GetPanelAttribute(engine->baseClass.scanPanHndl, ATTR_PANEL_PARENT, &workspacePanHndl);
	
					for (size_t i = 0; i < nScanChans; i++) {
						if (!IsVChanOpen((VChan_type*)engine->baseClass.scanChans[i]->detVChan)) continue;	// select only open detection channels
						
						engine->nPointBuffers++;
						nullChk( engine->pointBuffers = realloc(engine->pointBuffers, engine->nPointBuffers * sizeof(RectRasterPointBuff_type*)) );
						nullChk( engine->pointBuffers[engine->nPointBuffers - 1] = init_RectRasterPointBuff_type(engine->baseClass.scanChans[i]) );
						
					}
					
					break;
			}
			
			break;
			
		case TaskTree_Finished:
			
			break;
	}
	
Error:
	
	// release display TSV
	if (imgDisplayPtr)
		CmtReleaseTSVPtr(displayTSVHndl);
	
RETURN_ERR
}

/*
static int DataReceivedTC_RectRaster (TaskControl_type* taskControl, TCStates taskState, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorMsg)
{
	RectRaster_type* engine = GetTaskControlModuleData(taskControl);
	
	return 0;   
}
*/

static void ErrorTC_RectRaster (TaskControl_type* taskControl, int errorID, char* errorMsg)
{
	//RectRaster_type* engine = GetTaskControlModuleData(taskControl);
	
}

static int ModuleEventHandler_RectRaster (TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, void* eventData, BOOL const* abortFlag, char** errorMsg)
{
	//RectRaster_type* engine = GetTaskControlModuleData(taskControl);
	
	return 0;
}

static void SetRectRasterTaskControllerSettings (RectRaster_type* scanEngine)
{
	switch (scanEngine->baseClass.scanMode) {
			
		case ScanEngineMode_FrameScan:
			
			// set iteration mode
			SetTaskControlMode(scanEngine->baseClass.taskControl, !scanEngine->baseClass.continuousAcq);

			// set waiting time between iterations
			SetTaskControlIterationsWait(scanEngine->baseClass.taskControl, 0);
			
			// set number of iterations to be the number of frames to be acquired
			SetTaskControlIterations(scanEngine->baseClass.taskControl, scanEngine->baseClass.nFrames);
			
			break;
			
		case ScanEngineMode_PointScan:
			
			// set iteration mode to finite
			SetTaskControlMode(scanEngine->baseClass.taskControl, TASK_FINITE);

			// set waiting time between iterations
			SetTaskControlIterationsWait(scanEngine->baseClass.taskControl, scanEngine->pointScan.pointScanProtocol->repeatWait);
			
			// set number of iterations
			switch (scanEngine->pointScan.pointScanProtocol->jumpMethod) {
							
				case PointJump_SinglePoints:
			
					SetTaskControlIterations(scanEngine->baseClass.taskControl, scanEngine->pointScan.pointScanProtocol->nSequenceRepeat * NonResRectRasterScan_GetNumActivePoints(scanEngine));
					break;
							
				case PointJump_PointGroup:
							
					SetTaskControlIterations(scanEngine->baseClass.taskControl, scanEngine->pointScan.pointScanProtocol->nSequenceRepeat);
					break;
					
				case PointJump_IncrementalPointGroup:
					
					SetTaskControlIterations(scanEngine->baseClass.taskControl, scanEngine->pointScan.pointScanProtocol->nSequenceRepeat * NonResRectRasterScan_GetNumActivePoints(scanEngine));
					break;
			}
			break;
	}
}
